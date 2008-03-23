/*
 * hostapd - IEEE 802.11r - Fast BSS Transition
 * Copyright (c) 2004-2007, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"

#include "common.h"
#include "config.h"
#include "wpa.h"
#include "aes_wrap.h"
#include "ieee802_11.h"
#include "defs.h"
#include "wpa_auth_i.h"
#include "wpa_auth_ie.h"


#ifdef CONFIG_IEEE80211R

static int wpa_ft_rrb_send(struct wpa_authenticator *wpa_auth, const u8 *dst,
			   const u8 *data, size_t data_len)
{
	if (wpa_auth->cb.send_ether == NULL)
		return -1;
	return wpa_auth->cb.send_ether(wpa_auth->cb.ctx, dst, ETH_P_RRB,
				       data, data_len);
}


static int wpa_ft_action_send(struct wpa_authenticator *wpa_auth,
			      const u8 *dst, const u8 *data, size_t data_len)
{
	if (wpa_auth->cb.send_ft_action == NULL)
		return -1;
	return wpa_auth->cb.send_ft_action(wpa_auth->cb.ctx, dst,
					   data, data_len);
}


static struct wpa_state_machine *
wpa_ft_add_sta(struct wpa_authenticator *wpa_auth, const u8 *sta_addr)
{
	if (wpa_auth->cb.add_sta == NULL)
		return NULL;
	return wpa_auth->cb.add_sta(wpa_auth->cb.ctx, sta_addr);
}


int wpa_write_mdie(struct wpa_auth_config *conf, u8 *buf, size_t len)
{
	u8 *pos = buf;
	u8 capab;
	if (len < 2 + sizeof(struct rsn_mdie))
		return -1;

	*pos++ = WLAN_EID_MOBILITY_DOMAIN;
	*pos++ = MOBILITY_DOMAIN_ID_LEN + 1;
	os_memcpy(pos, conf->mobility_domain, MOBILITY_DOMAIN_ID_LEN);
	pos += MOBILITY_DOMAIN_ID_LEN;
	capab = RSN_FT_CAPAB_FT_OVER_DS;
	*pos++ = capab;

	return pos - buf;
}


static int wpa_write_ftie(struct wpa_auth_config *conf, const u8 *r0kh_id,
			  size_t r0kh_id_len,
			  const u8 *anonce, const u8 *snonce,
			  u8 *buf, size_t len, const u8 *subelem,
			  size_t subelem_len)
{
	u8 *pos = buf, *ielen;
	struct rsn_ftie *hdr;

	if (len < 2 + sizeof(*hdr) + 2 + FT_R1KH_ID_LEN + 2 + r0kh_id_len +
	    subelem_len)
		return -1;

	*pos++ = WLAN_EID_FAST_BSS_TRANSITION;
	ielen = pos++;

	hdr = (struct rsn_ftie *) pos;
	os_memset(hdr, 0, sizeof(*hdr));
	pos += sizeof(*hdr);
	WPA_PUT_LE16(hdr->mic_control, 0);
	if (anonce)
		os_memcpy(hdr->anonce, anonce, WPA_NONCE_LEN);
	if (snonce)
		os_memcpy(hdr->snonce, snonce, WPA_NONCE_LEN);

	/* Optional Parameters */
	*pos++ = FTIE_SUBELEM_R1KH_ID;
	*pos++ = FT_R1KH_ID_LEN;
	os_memcpy(pos, conf->r1_key_holder, FT_R1KH_ID_LEN);
	pos += FT_R1KH_ID_LEN;

	if (r0kh_id) {
		*pos++ = FTIE_SUBELEM_R0KH_ID;
		*pos++ = r0kh_id_len;
		os_memcpy(pos, r0kh_id, r0kh_id_len);
		pos += r0kh_id_len;
	}

	if (subelem) {
		os_memcpy(pos, subelem, subelem_len);
		pos += subelem_len;
	}

	*ielen = pos - buf - 2;

	return pos - buf;
}


struct wpa_ft_pmk_r0_sa {
	struct wpa_ft_pmk_r0_sa *next;
	u8 pmk_r0[PMK_LEN];
	u8 pmk_r0_name[WPA_PMK_NAME_LEN];
	u8 spa[ETH_ALEN];
	/* TODO: expiration, identity, radius_class, EAP type, VLAN ID */
	int pmk_r1_pushed;
};

struct wpa_ft_pmk_r1_sa {
	struct wpa_ft_pmk_r1_sa *next;
	u8 pmk_r1[PMK_LEN];
	u8 pmk_r1_name[WPA_PMK_NAME_LEN];
	u8 spa[ETH_ALEN];
	/* TODO: expiration, identity, radius_class, EAP type, VLAN ID */
};

struct wpa_ft_pmk_cache {
	struct wpa_ft_pmk_r0_sa *pmk_r0;
	struct wpa_ft_pmk_r1_sa *pmk_r1;
};

struct wpa_ft_pmk_cache * wpa_ft_pmk_cache_init(void)
{
	struct wpa_ft_pmk_cache *cache;

	cache = os_zalloc(sizeof(*cache));

	return cache;
}


void wpa_ft_pmk_cache_deinit(struct wpa_ft_pmk_cache *cache)
{
	struct wpa_ft_pmk_r0_sa *r0, *r0prev;
	struct wpa_ft_pmk_r1_sa *r1, *r1prev;

	r0 = cache->pmk_r0;
	while (r0) {
		r0prev = r0;
		r0 = r0->next;
		os_memset(r0prev->pmk_r0, 0, PMK_LEN);
		os_free(r0prev);
	}

	r1 = cache->pmk_r1;
	while (r1) {
		r1prev = r1;
		r1 = r1->next;
		os_memset(r1prev->pmk_r1, 0, PMK_LEN);
		os_free(r1prev);
	}

	os_free(cache);
}


static int wpa_ft_store_pmk_r0(struct wpa_authenticator *wpa_auth,
			       const u8 *spa, const u8 *pmk_r0,
			       const u8 *pmk_r0_name)
{
	struct wpa_ft_pmk_cache *cache = wpa_auth->ft_pmk_cache;
	struct wpa_ft_pmk_r0_sa *r0;

	/* TODO: add expiration and limit on number of entries in cache */

	r0 = os_zalloc(sizeof(*r0));
	if (r0 == NULL)
		return -1;

	os_memcpy(r0->pmk_r0, pmk_r0, PMK_LEN);
	os_memcpy(r0->pmk_r0_name, pmk_r0_name, WPA_PMK_NAME_LEN);
	os_memcpy(r0->spa, spa, ETH_ALEN);

	r0->next = cache->pmk_r0;
	cache->pmk_r0 = r0;

	return 0;
}


static int wpa_ft_fetch_pmk_r0(struct wpa_authenticator *wpa_auth,
			       const u8 *spa, const u8 *pmk_r0_name,
			       u8 *pmk_r0)
{
	struct wpa_ft_pmk_cache *cache = wpa_auth->ft_pmk_cache;
	struct wpa_ft_pmk_r0_sa *r0;

	r0 = cache->pmk_r0;
	while (r0) {
		if (os_memcmp(r0->spa, spa, ETH_ALEN) == 0 &&
		    os_memcmp(r0->pmk_r0_name, pmk_r0_name, WPA_PMK_NAME_LEN)
		    == 0) {
			os_memcpy(pmk_r0, r0->pmk_r0, PMK_LEN);
			return 0;
		}

		r0 = r0->next;
	}

	return -1;
}


static int wpa_ft_store_pmk_r1(struct wpa_authenticator *wpa_auth,
			       const u8 *spa, const u8 *pmk_r1,
			       const u8 *pmk_r1_name)
{
	struct wpa_ft_pmk_cache *cache = wpa_auth->ft_pmk_cache;
	struct wpa_ft_pmk_r1_sa *r1;

	/* TODO: add expiration and limit on number of entries in cache */

	r1 = os_zalloc(sizeof(*r1));
	if (r1 == NULL)
		return -1;

	os_memcpy(r1->pmk_r1, pmk_r1, PMK_LEN);
	os_memcpy(r1->pmk_r1_name, pmk_r1_name, WPA_PMK_NAME_LEN);
	os_memcpy(r1->spa, spa, ETH_ALEN);

	r1->next = cache->pmk_r1;
	cache->pmk_r1 = r1;

	return 0;
}


static int wpa_ft_fetch_pmk_r1(struct wpa_authenticator *wpa_auth,
			       const u8 *spa, const u8 *pmk_r1_name,
			       u8 *pmk_r1)
{
	struct wpa_ft_pmk_cache *cache = wpa_auth->ft_pmk_cache;
	struct wpa_ft_pmk_r1_sa *r1;

	r1 = cache->pmk_r1;
	while (r1) {
		if (os_memcmp(r1->spa, spa, ETH_ALEN) == 0 &&
		    os_memcmp(r1->pmk_r1_name, pmk_r1_name, WPA_PMK_NAME_LEN)
		    == 0) {
			os_memcpy(pmk_r1, r1->pmk_r1, PMK_LEN);
			return 0;
		}

		r1 = r1->next;
	}

	return -1;
}


static int wpa_ft_pull_pmk_r1(struct wpa_authenticator *wpa_auth,
			      const u8 *s1kh_id, const u8 *r0kh_id,
			      size_t r0kh_id_len, const u8 *pmk_r0_name)
{
	struct ft_remote_r0kh *r0kh;
	struct ft_r0kh_r1kh_pull_frame frame, f;

	r0kh = wpa_auth->conf.r0kh_list;
	while (r0kh) {
		if (r0kh->id_len == r0kh_id_len &&
		    os_memcmp(r0kh->id, r0kh_id, r0kh_id_len) == 0)
			break;
		r0kh = r0kh->next;
	}
	if (r0kh == NULL)
		return -1;

	wpa_printf(MSG_DEBUG, "FT: Send PMK-R1 pull request to remote R0KH "
		   "address " MACSTR, MAC2STR(r0kh->addr));

	os_memset(&frame, 0, sizeof(frame));
	frame.frame_type = RSN_REMOTE_FRAME_TYPE_FT_RRB;
	frame.packet_type = FT_PACKET_R0KH_R1KH_PULL;
	frame.data_length = host_to_le16(FT_R0KH_R1KH_PULL_DATA_LEN);
	os_memcpy(frame.ap_address, wpa_auth->addr, ETH_ALEN);

	/* aes_wrap() does not support inplace encryption, so use a temporary
	 * buffer for the data. */
	if (os_get_random(f.nonce, sizeof(f.nonce))) {
		wpa_printf(MSG_DEBUG, "FT: Failed to get random data for "
			   "nonce");
		return -1;
	}
	os_memcpy(f.pmk_r0_name, pmk_r0_name, WPA_PMK_NAME_LEN);
	os_memcpy(f.r1kh_id, wpa_auth->conf.r1_key_holder, FT_R1KH_ID_LEN);
	os_memcpy(f.s1kh_id, s1kh_id, ETH_ALEN);

	if (aes_wrap(r0kh->key, (FT_R0KH_R1KH_PULL_DATA_LEN + 7) / 8,
		     f.nonce, frame.nonce) < 0)
		return -1;

	wpa_ft_rrb_send(wpa_auth, r0kh->addr, (u8 *) &frame, sizeof(frame));

	return 0;
}


int wpa_auth_derive_ptk_ft(struct wpa_state_machine *sm, const u8 *pmk,
			   struct wpa_ptk *ptk)
{
	u8 pmk_r0[PMK_LEN], pmk_r0_name[WPA_PMK_NAME_LEN];
	u8 pmk_r1[PMK_LEN], pmk_r1_name[WPA_PMK_NAME_LEN];
	u8 ptk_name[WPA_PMK_NAME_LEN];
	const u8 *mdid = sm->wpa_auth->conf.mobility_domain;
	const u8 *r0kh = sm->wpa_auth->conf.r0_key_holder;
	size_t r0kh_len = sm->wpa_auth->conf.r0_key_holder_len;
	const u8 *r1kh = sm->wpa_auth->conf.r1_key_holder;
	const u8 *ssid = sm->wpa_auth->conf.ssid;
	size_t ssid_len = sm->wpa_auth->conf.ssid_len;


	if (sm->xxkey_len == 0) {
		wpa_printf(MSG_DEBUG, "FT: XXKey not available for key "
			   "derivation");
		return -1;
	}

	wpa_derive_pmk_r0(sm->xxkey, sm->xxkey_len, ssid, ssid_len, mdid,
			  r0kh, r0kh_len, sm->addr, pmk_r0, pmk_r0_name);
	wpa_hexdump_key(MSG_DEBUG, "FT: PMK-R0", pmk_r0, PMK_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: PMKR0Name", pmk_r0_name, WPA_PMK_NAME_LEN);
	wpa_ft_store_pmk_r0(sm->wpa_auth, sm->addr, pmk_r0, pmk_r0_name);

	wpa_derive_pmk_r1(pmk_r0, pmk_r0_name, r1kh, sm->addr,
			  pmk_r1, pmk_r1_name);
	wpa_hexdump_key(MSG_DEBUG, "FT: PMK-R1", pmk_r1, PMK_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: PMKR1Name", pmk_r1_name, WPA_PMK_NAME_LEN);
	wpa_ft_store_pmk_r1(sm->wpa_auth, sm->addr, pmk_r1, pmk_r1_name);

	wpa_pmk_r1_to_ptk(pmk_r1, sm->SNonce, sm->ANonce, sm->addr,
			  sm->wpa_auth->addr, pmk_r1_name,
			  (u8 *) ptk, sizeof(*ptk), ptk_name);
	wpa_hexdump_key(MSG_DEBUG, "FT: PTK", (u8 *) ptk, sizeof(*ptk));
	wpa_hexdump(MSG_DEBUG, "FT: PTKName", ptk_name, WPA_PMK_NAME_LEN);

	return 0;
}


static inline int wpa_auth_get_seqnum(struct wpa_authenticator *wpa_auth,
				      const u8 *addr, int idx, u8 *seq)
{
	if (wpa_auth->cb.get_seqnum == NULL)
		return -1;
	return wpa_auth->cb.get_seqnum(wpa_auth->cb.ctx, addr, idx, seq);
}


static u8 * wpa_ft_gtk_subelem(struct wpa_state_machine *sm, size_t *len)
{
	u8 *subelem;
	struct wpa_group *gsm = sm->group;
	size_t subelem_len, pad_len;
	const u8 *key;
	size_t key_len;
	u8 keybuf[32];

	key_len = gsm->GTK_len;
	if (key_len > sizeof(keybuf))
		return NULL;

	/*
	 * Pad key for AES Key Wrap if it is not multiple of 8 bytes or is less
	 * than 16 bytes.
	 */
	pad_len = key_len % 8;
	if (pad_len)
		pad_len = 8 - pad_len;
	if (key_len + pad_len < 16)
		pad_len += 8;
	if (pad_len) {
		os_memcpy(keybuf, gsm->GTK[gsm->GN - 1], key_len);
		os_memset(keybuf + key_len, 0, pad_len);
		keybuf[key_len] = 0xdd;
		key_len += pad_len;
		key = keybuf;
	} else
		key = gsm->GTK[gsm->GN - 1];

	/*
	 * Sub-elem ID[1] | Length[1] | Key Info[1] | Key Length[1] | RSC[8] |
	 * Key[5..32].
	 */
	subelem_len = 12 + key_len + 8;
	subelem = os_zalloc(subelem_len);
	if (subelem == NULL)
		return NULL;

	subelem[0] = FTIE_SUBELEM_GTK;
	subelem[1] = 10 + key_len + 8;
	subelem[2] = gsm->GN & 0x03; /* Key ID in B0-B1 of Key Info */
	subelem[3] = gsm->GTK_len;
	wpa_auth_get_seqnum(sm->wpa_auth, NULL, gsm->GN, subelem + 4);
	if (aes_wrap(sm->PTK.kek, key_len / 8, key, subelem + 12)) {
		os_free(subelem);
		return NULL;
	}

	*len = subelem_len;
	return subelem;
}


u8 * wpa_sm_write_assoc_resp_ies(struct wpa_state_machine *sm, u8 *pos,
				 size_t max_len, int auth_alg)
{
	u8 *end, *mdie, *ftie, *rsnie, *r0kh_id, *subelem = NULL;
	size_t mdie_len, ftie_len, rsnie_len, r0kh_id_len, subelem_len = 0;
	int res;
	struct wpa_auth_config *conf;
	struct rsn_ftie *_ftie;

	if (sm == NULL)
		return pos;

	conf = &sm->wpa_auth->conf;

	if (sm->wpa_key_mgmt != WPA_KEY_MGMT_FT_IEEE8021X &&
	    sm->wpa_key_mgmt != WPA_KEY_MGMT_FT_PSK)
		return pos;

	end = pos + max_len;

	/* RSN */
	res = wpa_write_rsn_ie(conf, pos, end - pos, sm->pmk_r1_name);
	if (res < 0)
		return pos;
	rsnie = pos;
	rsnie_len = res;
	pos += res;

	/* Mobility Domain Information */
	res = wpa_write_mdie(conf, pos, end - pos);
	if (res < 0)
		return pos;
	mdie = pos;
	mdie_len = res;
	pos += res;

	/* Fast BSS Transition Information */
	if (auth_alg == WLAN_AUTH_FT) {
		subelem = wpa_ft_gtk_subelem(sm, &subelem_len);
		r0kh_id = sm->r0kh_id;
		r0kh_id_len = sm->r0kh_id_len;
	} else {
		r0kh_id = conf->r0_key_holder;
		r0kh_id_len = conf->r0_key_holder_len;
	}
	res = wpa_write_ftie(conf, r0kh_id, r0kh_id_len, NULL, NULL, pos,
			     end - pos, subelem, subelem_len);
	os_free(subelem);
	if (res < 0)
		return pos;
	ftie = pos;
	ftie_len = res;
	pos += res;

	_ftie = (struct rsn_ftie *) (ftie + 2);
	_ftie->mic_control[1] = 3; /* Information element count */
	if (wpa_ft_mic(sm->PTK.kck, sm->pairwise == WPA_CIPHER_CCMP, sm->addr,
		       sm->wpa_auth->addr, 6, mdie, mdie_len, ftie, ftie_len,
		       rsnie, rsnie_len, NULL, 0, _ftie->mic) < 0)
		wpa_printf(MSG_DEBUG, "FT: Failed to calculate MIC");

	return pos;
}


struct wpa_ft_ies {
	const u8 *mdie;
	size_t mdie_len;
	const u8 *ftie;
	size_t ftie_len;
	const u8 *r1kh_id;
	const u8 *gtk;
	size_t gtk_len;
	const u8 *r0kh_id;
	size_t r0kh_id_len;
	const u8 *rsn;
	size_t rsn_len;
	const u8 *rsn_pmkid;
};


static int wpa_ft_parse_ftie(const u8 *ie, size_t ie_len,
			     struct wpa_ft_ies *parse)
{
	const u8 *end, *pos;

	parse->ftie = ie;
	parse->ftie_len = ie_len;

	pos = ie + sizeof(struct rsn_ftie);
	end = ie + ie_len;

	while (pos + 2 <= end && pos + 2 + pos[1] <= end) {
		switch (pos[0]) {
		case FTIE_SUBELEM_R1KH_ID:
			if (pos[1] != FT_R1KH_ID_LEN) {
				wpa_printf(MSG_DEBUG, "FT: Invalid R1KH-ID "
					   "length in FTIE: %d", pos[1]);
				return -1;
			}
			parse->r1kh_id = pos + 2;
			break;
		case FTIE_SUBELEM_GTK:
			parse->gtk = pos + 2;
			parse->gtk_len = pos[1];
			break;
		case FTIE_SUBELEM_R0KH_ID:
			if (pos[1] < 1 || pos[1] > FT_R0KH_ID_MAX_LEN) {
				wpa_printf(MSG_DEBUG, "FT: Invalid R0KH-ID "
					   "length in FTIE: %d", pos[1]);
				return -1;
			}
			parse->r0kh_id = pos + 2;
			parse->r0kh_id_len = pos[1];
			break;
		}

		pos += 2 + pos[1];
	}

	return 0;
}


static int wpa_ft_parse_ies(const u8 *ies, size_t ies_len,
			    struct wpa_ft_ies *parse)
{
	const u8 *end, *pos;
	struct wpa_ie_data data;
	int ret;

	os_memset(parse, 0, sizeof(*parse));
	if (ies == NULL)
		return 0;

	pos = ies;
	end = ies + ies_len;
	while (pos + 2 <= end && pos + 2 + pos[1] <= end) {
		switch (pos[0]) {
		case WLAN_EID_RSN:
			parse->rsn = pos + 2;
			parse->rsn_len = pos[1];
			ret = wpa_parse_wpa_ie_rsn(parse->rsn - 2,
						   parse->rsn_len + 2,
						   &data);
			if (ret < 0) {
				wpa_printf(MSG_DEBUG, "FT: Failed to parse "
					   "RSN IE: %d", ret);
				return -1;
			}
			if (data.num_pmkid == 1 && data.pmkid)
				parse->rsn_pmkid = data.pmkid;
			break;
		case WLAN_EID_MOBILITY_DOMAIN:
			parse->mdie = pos + 2;
			parse->mdie_len = pos[1];
			break;
		case WLAN_EID_FAST_BSS_TRANSITION:
			if (wpa_ft_parse_ftie(pos + 2, pos[1], parse) < 0)
				return -1;
			break;
		}

		pos += 2 + pos[1];
	}

	return 0;
}


static inline int wpa_auth_set_key(struct wpa_authenticator *wpa_auth,
				   int vlan_id,
				   const char *alg, const u8 *addr, int idx,
				   u8 *key, size_t key_len)
{
	if (wpa_auth->cb.set_key == NULL)
		return -1;
	return wpa_auth->cb.set_key(wpa_auth->cb.ctx, vlan_id, alg, addr, idx,
				    key, key_len);
}


static void wpa_ft_install_ptk(struct wpa_state_machine *sm)
{
	char *alg;
	int klen;

	/* MLME-SETKEYS.request(PTK) */
	if (sm->pairwise == WPA_CIPHER_TKIP) {
		alg = "TKIP";
		klen = 32;
	} else if (sm->pairwise == WPA_CIPHER_CCMP) {
		alg = "CCMP";
		klen = 16;
	} else
		return;

	/* FIX: add STA entry to kernel/driver here? The set_key will fail
	 * most likely without this.. At the moment, STA entry is added only
	 * after association has been completed. Alternatively, could
	 * re-configure PTK at that point(?).
	 */
	if (wpa_auth_set_key(sm->wpa_auth, 0, alg, sm->addr, 0,
			     sm->PTK.tk1, klen))
		return;

	/* FIX: MLME-SetProtection.Request(TA, Tx_Rx) */
	sm->pairwise_set = TRUE;
}


static u16 wpa_ft_process_auth_req(struct wpa_state_machine *sm,
				   const u8 *ies, size_t ies_len,
				   u8 **resp_ies, size_t *resp_ies_len)
{
	struct rsn_mdie *mdie;
	struct rsn_ftie *ftie;
	u8 pmk_r1[PMK_LEN], pmk_r1_name[WPA_PMK_NAME_LEN];
	u8 ptk_name[WPA_PMK_NAME_LEN];
	struct wpa_auth_config *conf;
	struct wpa_ft_ies parse;
	size_t buflen;
	int ret;
	u8 *pos, *end;

	*resp_ies = NULL;
	*resp_ies_len = 0;

	sm->pmk_r1_name_valid = 0;
	conf = &sm->wpa_auth->conf;

	wpa_hexdump(MSG_DEBUG, "FT: Received authentication frame IEs",
		    ies, ies_len);

	if (wpa_ft_parse_ies(ies, ies_len, &parse) < 0) {
		wpa_printf(MSG_DEBUG, "FT: Failed to parse FT IEs");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	mdie = (struct rsn_mdie *) parse.mdie;
	if (mdie == NULL || parse.mdie_len < sizeof(*mdie) ||
	    os_memcmp(mdie->mobility_domain,
		      sm->wpa_auth->conf.mobility_domain,
		      MOBILITY_DOMAIN_ID_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "FT: Invalid MDIE");
		return WLAN_STATUS_INVALID_MDIE;
	}

	ftie = (struct rsn_ftie *) parse.ftie;
	if (ftie == NULL || parse.ftie_len < sizeof(*ftie)) {
		wpa_printf(MSG_DEBUG, "FT: Invalid FTIE");
		return WLAN_STATUS_INVALID_FTIE;
	}

	os_memcpy(sm->SNonce, ftie->snonce, WPA_NONCE_LEN);

	if (parse.r0kh_id == NULL) {
		wpa_printf(MSG_DEBUG, "FT: Invalid FTIE - no R0KH-ID");
		return WLAN_STATUS_INVALID_FTIE;
	}

	wpa_hexdump(MSG_DEBUG, "FT: STA R0KH-ID",
		    parse.r0kh_id, parse.r0kh_id_len);
	os_memcpy(sm->r0kh_id, parse.r0kh_id, parse.r0kh_id_len);
	sm->r0kh_id_len = parse.r0kh_id_len;

	if (parse.rsn_pmkid == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No PMKID in RSNIE");
		return WLAN_STATUS_INVALID_PMKID;
	}

	wpa_hexdump(MSG_DEBUG, "FT: Requested PMKR0Name",
		    parse.rsn_pmkid, WPA_PMK_NAME_LEN);
	wpa_derive_pmk_r1_name(parse.rsn_pmkid,
			       sm->wpa_auth->conf.r1_key_holder, sm->addr,
			       pmk_r1_name);
	wpa_hexdump(MSG_DEBUG, "FT: Derived requested PMKR1Name",
		    pmk_r1_name, WPA_PMK_NAME_LEN);

	if (wpa_ft_fetch_pmk_r1(sm->wpa_auth, sm->addr, pmk_r1_name, pmk_r1) <
	    0) {
		if (wpa_ft_pull_pmk_r1(sm->wpa_auth, sm->addr, sm->r0kh_id,
				       sm->r0kh_id_len, parse.rsn_pmkid) < 0) {
			wpa_printf(MSG_DEBUG, "FT: Did not have matching "
				   "PMK-R1 and unknown R0KH-ID");
			return WLAN_STATUS_INVALID_PMKID;
		}

		/*
		 * TODO: Should return "status pending" (and the caller should
		 * not send out response now). The real response will be sent
		 * once the response from R0KH is received.
		 */
		return WLAN_STATUS_INVALID_PMKID;
	}

	wpa_hexdump_key(MSG_DEBUG, "FT: Selected PMK-R1", pmk_r1, PMK_LEN);
	sm->pmk_r1_name_valid = 1;
	os_memcpy(sm->pmk_r1_name, pmk_r1_name, WPA_PMK_NAME_LEN);

	if (os_get_random(sm->ANonce, WPA_NONCE_LEN)) {
		wpa_printf(MSG_DEBUG, "FT: Failed to get random data for "
			   "ANonce");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	wpa_hexdump(MSG_DEBUG, "FT: Received SNonce",
		    sm->SNonce, WPA_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: Generated ANonce",
		    sm->ANonce, WPA_NONCE_LEN);

	wpa_pmk_r1_to_ptk(pmk_r1, sm->SNonce, sm->ANonce, sm->addr,
			  sm->wpa_auth->addr, pmk_r1_name,
			  (u8 *) &sm->PTK, sizeof(sm->PTK), ptk_name);
	wpa_hexdump_key(MSG_DEBUG, "FT: PTK",
			(u8 *) &sm->PTK, sizeof(sm->PTK));
	wpa_hexdump(MSG_DEBUG, "FT: PTKName", ptk_name, WPA_PMK_NAME_LEN);

	wpa_ft_install_ptk(sm);

	buflen = 2 + sizeof(struct rsn_mdie) + 2 + sizeof(struct rsn_ftie) +
		2 + FT_R1KH_ID_LEN + 200;
	*resp_ies = os_zalloc(buflen);
	if (*resp_ies == NULL) {
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	pos = *resp_ies;
	end = *resp_ies + buflen;

	ret = wpa_write_rsn_ie(conf, pos, end - pos, parse.rsn_pmkid);
	if (ret < 0) {
		os_free(*resp_ies);
		*resp_ies = NULL;
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	pos += ret;

	ret = wpa_write_mdie(conf, pos, end - pos);
	if (ret < 0) {
		os_free(*resp_ies);
		*resp_ies = NULL;
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	pos += ret;

	ret = wpa_write_ftie(conf, parse.r0kh_id, parse.r0kh_id_len,
			     sm->ANonce, sm->SNonce, pos, end - pos, NULL, 0);
	if (ret < 0) {
		os_free(*resp_ies);
		*resp_ies = NULL;
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	pos += ret;

	*resp_ies_len = pos - *resp_ies;

	return WLAN_STATUS_SUCCESS;
}


void wpa_ft_process_auth(struct wpa_state_machine *sm, const u8 *bssid,
			 u16 auth_transaction, const u8 *ies, size_t ies_len,
			 void (*cb)(void *ctx, const u8 *dst, const u8 *bssid,
				    u16 auth_transaction, u16 status,
				    const u8 *ies, size_t ies_len),
			 void *ctx)
{
	u16 status;
	u8 *resp_ies;
	size_t resp_ies_len;

	if (sm == NULL) {
		wpa_printf(MSG_DEBUG, "FT: Received authentication frame, but "
			   "WPA SM not available");
		return;
	}

	wpa_printf(MSG_DEBUG, "FT: Received authentication frame: STA=" MACSTR
		   " BSSID=" MACSTR " transaction=%d",
		   MAC2STR(sm->addr), MAC2STR(bssid), auth_transaction);
	status = wpa_ft_process_auth_req(sm, ies, ies_len, &resp_ies,
					 &resp_ies_len);

	wpa_printf(MSG_DEBUG, "FT: FT authentication response: dst=" MACSTR
		   " auth_transaction=%d status=%d",
		   MAC2STR(sm->addr), auth_transaction + 1, status);
	wpa_hexdump(MSG_DEBUG, "FT: Response IEs", resp_ies, resp_ies_len);
	cb(ctx, sm->addr, bssid, auth_transaction + 1, status,
	   resp_ies, resp_ies_len);
	os_free(resp_ies);
}


u16 wpa_ft_validate_reassoc(struct wpa_state_machine *sm, const u8 *ies,
			    size_t ies_len)
{
	struct wpa_ft_ies parse;
	struct rsn_mdie *mdie;
	struct rsn_ftie *ftie;
	u8 mic[16];

	if (sm == NULL)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	wpa_hexdump(MSG_DEBUG, "FT: Reassoc Req IEs", ies, ies_len);

	if (wpa_ft_parse_ies(ies, ies_len, &parse) < 0) {
		wpa_printf(MSG_DEBUG, "FT: Failed to parse FT IEs");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (parse.rsn == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No RSNIE in Reassoc Req");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (parse.rsn_pmkid == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No PMKID in RSNIE");
		return WLAN_STATUS_INVALID_PMKID;
	}

	if (os_memcmp(parse.rsn_pmkid, sm->pmk_r1_name, WPA_PMK_NAME_LEN) != 0)
	{
		wpa_printf(MSG_DEBUG, "FT: PMKID in Reassoc Req did not match "
			   "with the PMKR1Name derived from auth request");
		return WLAN_STATUS_INVALID_PMKID;
	}

	mdie = (struct rsn_mdie *) parse.mdie;
	if (mdie == NULL || parse.mdie_len < sizeof(*mdie) ||
	    os_memcmp(mdie->mobility_domain,
		      sm->wpa_auth->conf.mobility_domain,
		      MOBILITY_DOMAIN_ID_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "FT: Invalid MDIE");
		return WLAN_STATUS_INVALID_MDIE;
	}

	ftie = (struct rsn_ftie *) parse.ftie;
	if (ftie == NULL || parse.ftie_len < sizeof(*ftie)) {
		wpa_printf(MSG_DEBUG, "FT: Invalid FTIE");
		return WLAN_STATUS_INVALID_FTIE;
	}

	/*
	 * Assume that MDIE, FTIE, and RSN IE are protected and that there is
	 * no RIC, so total of 3 protected IEs.
	 */
	if (ftie->mic_control[1] != 3) {
		wpa_printf(MSG_DEBUG, "FT: Unexpected IE count in FTIE (%d)",
			   ftie->mic_control[1]);
		return WLAN_STATUS_INVALID_FTIE;
	}

	if (wpa_ft_mic(sm->PTK.kck, sm->pairwise == WPA_CIPHER_CCMP, sm->addr,
		       sm->wpa_auth->addr, 5,
		       parse.mdie - 2, parse.mdie_len + 2,
		       parse.ftie - 2, parse.ftie_len + 2,
		       parse.rsn - 2, parse.rsn_len + 2, NULL, 0,
		       mic) < 0) {
		wpa_printf(MSG_DEBUG, "FT: Failed to calculate MIC");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (os_memcmp(mic, ftie->mic, 16) != 0) {
		wpa_printf(MSG_DEBUG, "FT: Invalid MIC in FTIE");
		wpa_hexdump(MSG_MSGDUMP, "FT: Received MIC", ftie->mic, 16);
		wpa_hexdump(MSG_MSGDUMP, "FT: Calculated MIC", mic, 16);
		return WLAN_STATUS_INVALID_FTIE;
	}

	return WLAN_STATUS_SUCCESS;
}


int wpa_ft_action_rx(struct wpa_state_machine *sm, const u8 *data, size_t len)
{
	const u8 *sta_addr, *target_ap;
	const u8 *ies;
	size_t ies_len;
	u8 action;
	struct ft_rrb_frame *frame;

	if (sm == NULL)
		return -1;

	/*
	 * data: Category[1] Action[1] STA_Address[6] Target_AP_Address[6]
	 * FT Request action frame body[variable]
	 */

	if (len < 14) {
		wpa_printf(MSG_DEBUG, "FT: Too short FT Action frame "
			   "(len=%lu)", (unsigned long) len);
		return -1;
	}

	action = data[1];
	sta_addr = data + 2;
	target_ap = data + 8;
	ies = data + 14;
	ies_len = len - 14;

	wpa_printf(MSG_DEBUG, "FT: Received FT Action frame (STA=" MACSTR
		   " Target AP=" MACSTR " Action=%d)",
		   MAC2STR(sta_addr), MAC2STR(target_ap), action);

	if (os_memcmp(sta_addr, sm->addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "FT: Mismatch in FT Action STA address: "
			   "STA=" MACSTR " STA-Address=" MACSTR,
			   MAC2STR(sm->addr), MAC2STR(sta_addr));
		return -1;
	}

	/*
	 * Do some sanity checking on the target AP address (not own and not
	 * broadcast. This could be extended to filter based on a list of known
	 * APs in the MD (if such a list were configured).
	 */
	if ((target_ap[0] & 0x01) ||
	    os_memcmp(target_ap, sm->wpa_auth->addr, ETH_ALEN) == 0) {
		wpa_printf(MSG_DEBUG, "FT: Invalid Target AP in FT Action "
			   "frame");
		return -1;
	}

	wpa_hexdump(MSG_MSGDUMP, "FT: Action frame body", ies, ies_len);

	/* RRB - Forward action frame to the target AP */
	frame = os_malloc(sizeof(*frame) + len);
	frame->frame_type = RSN_REMOTE_FRAME_TYPE_FT_RRB;
	frame->packet_type = FT_PACKET_REQUEST;
	frame->action_length = host_to_le16(len);
	os_memcpy(frame->ap_address, sm->wpa_auth->addr, ETH_ALEN);
	os_memcpy(frame + 1, data, len);

	wpa_ft_rrb_send(sm->wpa_auth, target_ap, (u8 *) frame,
			sizeof(*frame) + len);
	os_free(frame);

	return 0;
}


static int wpa_ft_rrb_rx_request(struct wpa_authenticator *wpa_auth,
				 const u8 *current_ap, const u8 *sta_addr,
				 const u8 *body, size_t len)
{
	struct wpa_state_machine *sm;
	u16 status;
	u8 *resp_ies, *pos;
	size_t resp_ies_len, rlen;
	struct ft_rrb_frame *frame;

	sm = wpa_ft_add_sta(wpa_auth, sta_addr);
	if (sm == NULL) {
		wpa_printf(MSG_DEBUG, "FT: Failed to add new STA based on "
			   "RRB Request");
		return -1;
	}

	wpa_hexdump(MSG_MSGDUMP, "FT: RRB Request Frame body", body, len);

	status = wpa_ft_process_auth_req(sm, body, len, &resp_ies,
					 &resp_ies_len);

	wpa_printf(MSG_DEBUG, "FT: RRB authentication response: STA=" MACSTR
		   " CurrentAP=" MACSTR " status=%d",
		   MAC2STR(sm->addr), MAC2STR(current_ap), status);
	wpa_hexdump(MSG_DEBUG, "FT: Response IEs", resp_ies, resp_ies_len);

	/* RRB - Forward action frame response to the Current AP */

	/*
	 * data: Category[1] Action[1] STA_Address[6] Target_AP_Address[6]
	 * Status_Code[2] FT Request action frame body[variable]
	 */
	rlen = 2 + 2 * ETH_ALEN + 2 + resp_ies_len;

	frame = os_malloc(sizeof(*frame) + rlen);
	frame->frame_type = RSN_REMOTE_FRAME_TYPE_FT_RRB;
	frame->packet_type = FT_PACKET_RESPONSE;
	frame->action_length = host_to_le16(rlen);
	os_memcpy(frame->ap_address, wpa_auth->addr, ETH_ALEN);
	pos = (u8 *) (frame + 1);
	*pos++ = WLAN_ACTION_FT;
	*pos++ = 2; /* Action: Response */
	os_memcpy(pos, sta_addr, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, wpa_auth->addr, ETH_ALEN);
	pos += ETH_ALEN;
	WPA_PUT_LE16(pos, status);
	pos += 2;
	if (resp_ies) {
		os_memcpy(pos, resp_ies, resp_ies_len);
		os_free(resp_ies);
	}

	wpa_ft_rrb_send(wpa_auth, current_ap, (u8 *) frame,
			sizeof(*frame) + rlen);
	os_free(frame);

	return 0;
}


static int wpa_ft_rrb_rx_pull(struct wpa_authenticator *wpa_auth,
			      const u8 *src_addr,
			      const u8 *data, size_t data_len)
{
	struct ft_r0kh_r1kh_pull_frame *frame, f;
	struct ft_remote_r1kh *r1kh;
	struct ft_r0kh_r1kh_resp_frame resp, r;
	u8 pmk_r0[PMK_LEN];

	wpa_printf(MSG_DEBUG, "FT: Received PMK-R1 pull");

	if (data_len < sizeof(*frame))
		return -1;

	r1kh = wpa_auth->conf.r1kh_list;
	while (r1kh) {
		if (os_memcmp(r1kh->addr, src_addr, ETH_ALEN) == 0)
			break;
		r1kh = r1kh->next;
	}
	if (r1kh == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No matching R1KH address found for "
			   "PMK-R1 pull source address " MACSTR,
			   MAC2STR(src_addr));
		return -1;
	}

	frame = (struct ft_r0kh_r1kh_pull_frame *) data;
	/* aes_unwrap() does not support inplace decryption, so use a temporary
	 * buffer for the data. */
	if (aes_unwrap(r1kh->key, (FT_R0KH_R1KH_PULL_DATA_LEN + 7) / 8,
		       frame->nonce, f.nonce) < 0) {
		wpa_printf(MSG_DEBUG, "FT: Failed to decrypt PMK-R1 pull "
			   "request from " MACSTR, MAC2STR(src_addr));
		return -1;
	}

	wpa_hexdump(MSG_DEBUG, "FT: PMK-R1 pull - nonce",
		    f.nonce, sizeof(f.nonce));
	wpa_hexdump(MSG_DEBUG, "FT: PMK-R1 pull - PMKR0Name",
		    f.pmk_r0_name, WPA_PMK_NAME_LEN);
	wpa_printf(MSG_DEBUG, "FT: PMK-R1 pull - R1KH-ID=" MACSTR "S1KH-ID="
		   MACSTR, MAC2STR(f.r1kh_id), MAC2STR(f.s1kh_id));

	os_memset(&resp, 0, sizeof(resp));
	resp.frame_type = RSN_REMOTE_FRAME_TYPE_FT_RRB;
	resp.packet_type = FT_PACKET_R0KH_R1KH_RESP;
	resp.data_length = host_to_le16(FT_R0KH_R1KH_RESP_DATA_LEN);
	os_memcpy(resp.ap_address, wpa_auth->addr, ETH_ALEN);

	/* aes_wrap() does not support inplace encryption, so use a temporary
	 * buffer for the data. */
	os_memcpy(r.nonce, f.nonce, sizeof(f.nonce));
	os_memcpy(r.r1kh_id, f.r1kh_id, FT_R1KH_ID_LEN);
	os_memcpy(r.s1kh_id, f.s1kh_id, ETH_ALEN);
	if (wpa_ft_fetch_pmk_r0(wpa_auth, f.s1kh_id, f.pmk_r0_name, pmk_r0) <
	    0) {
		wpa_printf(MSG_DEBUG, "FT: No matching PMKR0Name found for "
			   "PMK-R1 pull");
		return -1;
	}

	wpa_derive_pmk_r1(pmk_r0, f.pmk_r0_name, f.r1kh_id, f.s1kh_id,
			  r.pmk_r1, r.pmk_r1_name);
	wpa_hexdump_key(MSG_DEBUG, "FT: PMK-R1", r.pmk_r1, PMK_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: PMKR1Name", r.pmk_r1_name,
		    WPA_PMK_NAME_LEN);

	if (aes_wrap(r1kh->key, (FT_R0KH_R1KH_RESP_DATA_LEN + 7) / 8,
		     r.nonce, resp.nonce) < 0) {
		os_memset(pmk_r0, 0, PMK_LEN);
		return -1;
	}

	os_memset(pmk_r0, 0, PMK_LEN);

	wpa_ft_rrb_send(wpa_auth, src_addr, (u8 *) &resp, sizeof(resp));

	return 0;
}


static int wpa_ft_rrb_rx_resp(struct wpa_authenticator *wpa_auth,
			      const u8 *src_addr,
			      const u8 *data, size_t data_len)
{
	struct ft_r0kh_r1kh_resp_frame *frame, f;
	struct ft_remote_r0kh *r0kh;

	wpa_printf(MSG_DEBUG, "FT: Received PMK-R1 pull response");

	if (data_len < sizeof(*frame))
		return -1;

	r0kh = wpa_auth->conf.r0kh_list;
	while (r0kh) {
		if (os_memcmp(r0kh->addr, src_addr, ETH_ALEN) == 0)
			break;
		r0kh = r0kh->next;
	}
	if (r0kh == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No matching R0KH address found for "
			   "PMK-R0 pull response source address " MACSTR,
			   MAC2STR(src_addr));
		return -1;
	}

	frame = (struct ft_r0kh_r1kh_resp_frame *) data;
	/* aes_unwrap() does not support inplace decryption, so use a temporary
	 * buffer for the data. */
	if (aes_unwrap(r0kh->key, (FT_R0KH_R1KH_RESP_DATA_LEN + 7) / 8,
		       frame->nonce, f.nonce) < 0) {
		wpa_printf(MSG_DEBUG, "FT: Failed to decrypt PMK-R1 pull "
			   "response from " MACSTR, MAC2STR(src_addr));
		return -1;
	}

	if (os_memcmp(f.r1kh_id, wpa_auth->conf.r1_key_holder, FT_R1KH_ID_LEN)
	    != 0) {
		wpa_printf(MSG_DEBUG, "FT: PMK-R1 pull response did not use a "
			   "matching R1KH-ID");
		return -1;
	}

	/* TODO: verify that <nonce,s1kh_id> matches with a pending request
	 * and call this requests callback function to finish request
	 * processing */

	wpa_hexdump(MSG_DEBUG, "FT: PMK-R1 pull - nonce",
		    f.nonce, sizeof(f.nonce));
	wpa_printf(MSG_DEBUG, "FT: PMK-R1 pull - R1KH-ID=" MACSTR "S1KH-ID="
		   MACSTR, MAC2STR(f.r1kh_id), MAC2STR(f.s1kh_id));
	wpa_hexdump_key(MSG_DEBUG, "FT: PMK-R1 pull - PMK-R1",
			f.pmk_r1, PMK_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: PMK-R1 pull - PMKR1Name",
			f.pmk_r1_name, WPA_PMK_NAME_LEN);

	wpa_ft_store_pmk_r1(wpa_auth, f.s1kh_id, f.pmk_r1, f.pmk_r1_name);
	os_memset(f.pmk_r1, 0, PMK_LEN);

	return 0;
}


static int wpa_ft_rrb_rx_push(struct wpa_authenticator *wpa_auth,
			      const u8 *src_addr,
			      const u8 *data, size_t data_len)
{
	struct ft_r0kh_r1kh_push_frame *frame, f;
	struct ft_remote_r0kh *r0kh;
	struct os_time now;
	os_time_t tsend;

	wpa_printf(MSG_DEBUG, "FT: Received PMK-R1 push");

	if (data_len < sizeof(*frame))
		return -1;

	r0kh = wpa_auth->conf.r0kh_list;
	while (r0kh) {
		if (os_memcmp(r0kh->addr, src_addr, ETH_ALEN) == 0)
			break;
		r0kh = r0kh->next;
	}
	if (r0kh == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No matching R0KH address found for "
			   "PMK-R0 push source address " MACSTR,
			   MAC2STR(src_addr));
		return -1;
	}

	frame = (struct ft_r0kh_r1kh_push_frame *) data;
	/* aes_unwrap() does not support inplace decryption, so use a temporary
	 * buffer for the data. */
	if (aes_unwrap(r0kh->key, (FT_R0KH_R1KH_PUSH_DATA_LEN + 7) / 8,
		       frame->timestamp, f.timestamp) < 0) {
		wpa_printf(MSG_DEBUG, "FT: Failed to decrypt PMK-R1 push from "
			   MACSTR, MAC2STR(src_addr));
		return -1;
	}

	os_get_time(&now);
	tsend = WPA_GET_LE32(f.timestamp);
	if ((now.sec > tsend && now.sec - tsend > 60) ||
	    (now.sec < tsend && tsend - now.sec > 60)) {
		wpa_printf(MSG_DEBUG, "FT: PMK-R1 push did not have a valid "
			   "timestamp: sender time %d own time %d\n",
			   (int) tsend, (int) now.sec);
		return -1;
	}

	if (os_memcmp(f.r1kh_id, wpa_auth->conf.r1_key_holder, FT_R1KH_ID_LEN)
	    != 0) {
		wpa_printf(MSG_DEBUG, "FT: PMK-R1 push did not use a matching "
			   "R1KH-ID (received " MACSTR " own " MACSTR ")",
			   MAC2STR(f.r1kh_id),
			   MAC2STR(wpa_auth->conf.r1_key_holder));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "FT: PMK-R1 push - R1KH-ID=" MACSTR " S1KH-ID="
		   MACSTR, MAC2STR(f.r1kh_id), MAC2STR(f.s1kh_id));
	wpa_hexdump_key(MSG_DEBUG, "FT: PMK-R1 push - PMK-R1",
			f.pmk_r1, PMK_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: PMK-R1 push - PMKR1Name",
			f.pmk_r1_name, WPA_PMK_NAME_LEN);

	wpa_ft_store_pmk_r1(wpa_auth, f.s1kh_id, f.pmk_r1, f.pmk_r1_name);
	os_memset(f.pmk_r1, 0, PMK_LEN);

	return 0;
}


int wpa_ft_rrb_rx(struct wpa_authenticator *wpa_auth, const u8 *src_addr,
		  const u8 *data, size_t data_len)
{
	struct ft_rrb_frame *frame;
	u16 alen;
	const u8 *pos, *end, *start;
	u8 action;
	const u8 *sta_addr, *target_ap_addr;

	wpa_printf(MSG_DEBUG, "FT: RRB received frame from remote AP " MACSTR,
		   MAC2STR(src_addr));

	if (data_len < sizeof(*frame)) {
		wpa_printf(MSG_DEBUG, "FT: Too short RRB frame (data_len=%lu)",
			   (unsigned long) data_len);
		return -1;
	}

	pos = data;
	frame = (struct ft_rrb_frame *) pos;
	pos += sizeof(*frame);

	alen = le_to_host16(frame->action_length);
	wpa_printf(MSG_DEBUG, "FT: RRB frame - frame_type=%d packet_type=%d "
		   "action_length=%d ap_address=" MACSTR,
		   frame->frame_type, frame->packet_type, alen,
		   MAC2STR(frame->ap_address));

	if (frame->frame_type != RSN_REMOTE_FRAME_TYPE_FT_RRB) {
		/* Discard frame per IEEE 802.11r/D8.0, 10A.10.3 */
		wpa_printf(MSG_DEBUG, "FT: RRB discarded frame with "
			   "unrecognized type %d", frame->frame_type);
		return -1;
	}

	if (alen > data_len - sizeof(*frame)) {
		wpa_printf(MSG_DEBUG, "FT: RRB frame too short for action "
			   "frame");
		return -1;
	}

	if (frame->packet_type == FT_PACKET_R0KH_R1KH_PULL)
		return wpa_ft_rrb_rx_pull(wpa_auth, src_addr, data, data_len);
	if (frame->packet_type == FT_PACKET_R0KH_R1KH_RESP)
		return wpa_ft_rrb_rx_resp(wpa_auth, src_addr, data, data_len);
	if (frame->packet_type == FT_PACKET_R0KH_R1KH_PUSH)
		return wpa_ft_rrb_rx_push(wpa_auth, src_addr, data, data_len);

	wpa_hexdump(MSG_MSGDUMP, "FT: RRB - FT Action frame", pos, alen);

	if (alen < 1 + 1 + 2 * ETH_ALEN) {
		wpa_printf(MSG_DEBUG, "FT: Too short RRB frame (not enough "
			   "room for Action Frame body); alen=%lu",
			   (unsigned long) alen);
		return -1;
	}
	start = pos;
	end = pos + alen;

	if (*pos != WLAN_ACTION_FT) {
		wpa_printf(MSG_DEBUG, "FT: Unexpected Action frame category "
			   "%d", *pos);
		return -1;
	}

	pos++;
	action = *pos++;
	sta_addr = pos;
	pos += ETH_ALEN;
	target_ap_addr = pos;
	pos += ETH_ALEN;
	wpa_printf(MSG_DEBUG, "FT: RRB Action Frame: action=%d sta_addr="
		   MACSTR " target_ap_addr=" MACSTR,
		   action, MAC2STR(sta_addr), MAC2STR(target_ap_addr));

	if (frame->packet_type == FT_PACKET_REQUEST) {
		wpa_printf(MSG_DEBUG, "FT: FT Packet Type - Request");

		if (action != 1) {
			wpa_printf(MSG_DEBUG, "FT: Unexpected Action %d in "
				   "RRB Request", action);
			return -1;
		}

		if (os_memcmp(target_ap_addr, wpa_auth->addr, ETH_ALEN) != 0) {
			wpa_printf(MSG_DEBUG, "FT: Target AP address in the "
				   "RRB Request does not match with own "
				   "address");
			return -1;
		}

		if (wpa_ft_rrb_rx_request(wpa_auth, frame->ap_address,
					  sta_addr, pos, end - pos) < 0)
			return -1;
	} else if (frame->packet_type == FT_PACKET_RESPONSE) {
		u16 status_code;

		if (end - pos < 2) {
			wpa_printf(MSG_DEBUG, "FT: Not enough room for status "
				   "code in RRB Response");
			return -1;
		}
		status_code = WPA_GET_LE16(pos);
		pos += 2;

		wpa_printf(MSG_DEBUG, "FT: FT Packet Type - Response "
			   "(status_code=%d)", status_code);

		if (wpa_ft_action_send(wpa_auth, sta_addr, start, alen) < 0)
			return -1;
	} else {
		wpa_printf(MSG_DEBUG, "FT: RRB discarded frame with unknown "
			   "packet_type %d", frame->packet_type);
		return -1;
	}

	return 0;
}


static void wpa_ft_generate_pmk_r1(struct wpa_authenticator *wpa_auth,
				   struct wpa_ft_pmk_r0_sa *pmk_r0,
				   struct ft_remote_r1kh *r1kh,
				   const u8 *s1kh_id)
{
	struct ft_r0kh_r1kh_push_frame frame, f;
	struct os_time now;

	os_memset(&frame, 0, sizeof(frame));
	frame.frame_type = RSN_REMOTE_FRAME_TYPE_FT_RRB;
	frame.packet_type = FT_PACKET_R0KH_R1KH_PUSH;
	frame.data_length = host_to_le16(FT_R0KH_R1KH_PUSH_DATA_LEN);
	os_memcpy(frame.ap_address, wpa_auth->addr, ETH_ALEN);

	/* aes_wrap() does not support inplace encryption, so use a temporary
	 * buffer for the data. */
	os_memcpy(f.r1kh_id, r1kh->id, FT_R1KH_ID_LEN);
	os_memcpy(f.s1kh_id, s1kh_id, ETH_ALEN);
	os_memcpy(f.pmk_r0_name, pmk_r0->pmk_r0_name, WPA_PMK_NAME_LEN);
	wpa_derive_pmk_r1(pmk_r0->pmk_r0, pmk_r0->pmk_r0_name, r1kh->id,
			  s1kh_id, f.pmk_r1, f.pmk_r1_name);
	wpa_printf(MSG_DEBUG, "FT: R1KH-ID " MACSTR, MAC2STR(r1kh->id));
	wpa_hexdump_key(MSG_DEBUG, "FT: PMK-R1", f.pmk_r1, PMK_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: PMKR1Name", f.pmk_r1_name,
		    WPA_PMK_NAME_LEN);
	os_get_time(&now);
	WPA_PUT_LE32(f.timestamp, now.sec);
	if (aes_wrap(r1kh->key, (FT_R0KH_R1KH_PUSH_DATA_LEN + 7) / 8,
		     f.timestamp, frame.timestamp) < 0)
		return;

	wpa_ft_rrb_send(wpa_auth, r1kh->addr, (u8 *) &frame, sizeof(frame));
}


void wpa_ft_push_pmk_r1(struct wpa_authenticator *wpa_auth, const u8 *addr)
{
	struct wpa_ft_pmk_r0_sa *r0;
	struct ft_remote_r1kh *r1kh;

	if (!wpa_auth->conf.pmk_r1_push)
		return;

	r0 = wpa_auth->ft_pmk_cache->pmk_r0;
	while (r0) {
		if (os_memcmp(r0->spa, addr, ETH_ALEN) == 0)
			break;
		r0 = r0->next;
	}

	if (r0 == NULL || r0->pmk_r1_pushed)
		return;
	r0->pmk_r1_pushed = 1;

	wpa_printf(MSG_DEBUG, "FT: Deriving and pushing PMK-R1 keys to R1KHs "
		   "for STA " MACSTR, MAC2STR(addr));

	r1kh = wpa_auth->conf.r1kh_list;
	while (r1kh) {
		wpa_ft_generate_pmk_r1(wpa_auth, r0, r1kh, addr);
		r1kh = r1kh->next;
	}
}

#endif /* CONFIG_IEEE80211R */
