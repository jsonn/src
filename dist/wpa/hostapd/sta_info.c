/*
 * hostapd / Station table
 * Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi>
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

#include "hostapd.h"
#include "sta_info.h"
#include "eloop.h"
#include "accounting.h"
#include "ieee802_1x.h"
#include "ieee802_11.h"
#include "radius/radius.h"
#include "wpa.h"
#include "preauth.h"
#include "radius/radius_client.h"
#include "driver.h"
#include "beacon.h"
#include "hw_features.h"
#include "mlme.h"
#include "vlan_init.h"

static int ap_sta_in_other_bss(struct hostapd_data *hapd,
			       struct sta_info *sta, u32 flags);
static void ap_handle_session_timer(void *eloop_ctx, void *timeout_ctx);

int ap_for_each_sta(struct hostapd_data *hapd,
		    int (*cb)(struct hostapd_data *hapd, struct sta_info *sta,
			      void *ctx),
		    void *ctx)
{
	struct sta_info *sta;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (cb(hapd, sta, ctx))
			return 1;
	}

	return 0;
}


struct sta_info * ap_get_sta(struct hostapd_data *hapd, const u8 *sta)
{
	struct sta_info *s;

	s = hapd->sta_hash[STA_HASH(sta)];
	while (s != NULL && os_memcmp(s->addr, sta, 6) != 0)
		s = s->hnext;
	return s;
}


static void ap_sta_list_del(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct sta_info *tmp;

	if (hapd->sta_list == sta) {
		hapd->sta_list = sta->next;
		return;
	}

	tmp = hapd->sta_list;
	while (tmp != NULL && tmp->next != sta)
		tmp = tmp->next;
	if (tmp == NULL) {
		wpa_printf(MSG_DEBUG, "Could not remove STA " MACSTR " from "
			   "list.", MAC2STR(sta->addr));
	} else
		tmp->next = sta->next;
}


void ap_sta_hash_add(struct hostapd_data *hapd, struct sta_info *sta)
{
	sta->hnext = hapd->sta_hash[STA_HASH(sta->addr)];
	hapd->sta_hash[STA_HASH(sta->addr)] = sta;
}


static void ap_sta_hash_del(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct sta_info *s;

	s = hapd->sta_hash[STA_HASH(sta->addr)];
	if (s == NULL) return;
	if (os_memcmp(s->addr, sta->addr, 6) == 0) {
		hapd->sta_hash[STA_HASH(sta->addr)] = s->hnext;
		return;
	}

	while (s->hnext != NULL &&
	       os_memcmp(s->hnext->addr, sta->addr, ETH_ALEN) != 0)
		s = s->hnext;
	if (s->hnext != NULL)
		s->hnext = s->hnext->hnext;
	else
		wpa_printf(MSG_DEBUG, "AP: could not remove STA " MACSTR
			   " from hash table", MAC2STR(sta->addr));
}


void ap_free_sta(struct hostapd_data *hapd, struct sta_info *sta)
{
	int set_beacon = 0;

	accounting_sta_stop(hapd, sta);

	if (!ap_sta_in_other_bss(hapd, sta, WLAN_STA_ASSOC) &&
	    !(sta->flags & WLAN_STA_PREAUTH))
		hostapd_sta_remove(hapd, sta->addr);

	ap_sta_hash_del(hapd, sta);
	ap_sta_list_del(hapd, sta);

	if (sta->aid > 0)
		hapd->sta_aid[sta->aid - 1] = NULL;

	hapd->num_sta--;
	if (sta->nonerp_set) {
		sta->nonerp_set = 0;
		hapd->iface->num_sta_non_erp--;
		if (hapd->iface->num_sta_non_erp == 0)
			set_beacon++;
	}

	if (sta->no_short_slot_time_set) {
		sta->no_short_slot_time_set = 0;
		hapd->iface->num_sta_no_short_slot_time--;
		if (hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G
		    && hapd->iface->num_sta_no_short_slot_time == 0)
			set_beacon++;
	}

	if (sta->no_short_preamble_set) {
		sta->no_short_preamble_set = 0;
		hapd->iface->num_sta_no_short_preamble--;
		if (hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G
		    && hapd->iface->num_sta_no_short_preamble == 0)
			set_beacon++;
	}

	if (set_beacon)
		ieee802_11_set_beacons(hapd->iface);

	eloop_cancel_timeout(ap_handle_timer, hapd, sta);
	eloop_cancel_timeout(ap_handle_session_timer, hapd, sta);

	ieee802_1x_free_station(sta);
	wpa_auth_sta_deinit(sta->wpa_sm);
	rsn_preauth_free_station(hapd, sta);
	radius_client_flush_auth(hapd->radius, sta->addr);

	os_free(sta->last_assoc_req);
	os_free(sta->challenge);
	os_free(sta);
}


void hostapd_free_stas(struct hostapd_data *hapd)
{
	struct sta_info *sta, *prev;

	sta = hapd->sta_list;

	while (sta) {
		prev = sta;
		if (sta->flags & WLAN_STA_AUTH) {
			mlme_deauthenticate_indication(
				hapd, sta, WLAN_REASON_UNSPECIFIED);
		}
		sta = sta->next;
		wpa_printf(MSG_DEBUG, "Removing station " MACSTR,
			   MAC2STR(prev->addr));
		ap_free_sta(hapd, prev);
	}
}


void ap_handle_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;
	unsigned long next_time = 0;

	if (sta->timeout_next == STA_REMOVE) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "deauthenticated due to "
			       "local deauth request");
		ap_free_sta(hapd, sta);
		return;
	}

	if ((sta->flags & WLAN_STA_ASSOC) &&
	    (sta->timeout_next == STA_NULLFUNC ||
	     sta->timeout_next == STA_DISASSOC)) {
		int inactive_sec;
		wpa_printf(MSG_DEBUG, "Checking STA " MACSTR " inactivity:",
			   MAC2STR(sta->addr));
		inactive_sec = hostapd_get_inact_sec(hapd, sta->addr);
		if (inactive_sec == -1) {
			wpa_printf(MSG_DEBUG, "Could not get station info "
				   "from kernel driver for " MACSTR ".",
				   MAC2STR(sta->addr));
		} else if (inactive_sec < hapd->conf->ap_max_inactivity &&
			   sta->flags & WLAN_STA_ASSOC) {
			/* station activity detected; reset timeout state */
			wpa_printf(MSG_DEBUG, "  Station has been active");
			sta->timeout_next = STA_NULLFUNC;
			next_time = hapd->conf->ap_max_inactivity -
				inactive_sec;
		}
	}

	if ((sta->flags & WLAN_STA_ASSOC) &&
	    sta->timeout_next == STA_DISASSOC &&
	    !(sta->flags & WLAN_STA_PENDING_POLL)) {
		wpa_printf(MSG_DEBUG, "  Station has ACKed data poll");
		/* data nullfunc frame poll did not produce TX errors; assume
		 * station ACKed it */
		sta->timeout_next = STA_NULLFUNC;
		next_time = hapd->conf->ap_max_inactivity;
	}

	if (next_time) {
		eloop_register_timeout(next_time, 0, ap_handle_timer, hapd,
				       sta);
		return;
	}

	if (sta->timeout_next == STA_NULLFUNC &&
	    (sta->flags & WLAN_STA_ASSOC)) {
		/* send data frame to poll STA and check whether this frame
		 * is ACKed */
		struct ieee80211_hdr hdr;

		wpa_printf(MSG_DEBUG, "  Polling STA with data frame");
		sta->flags |= WLAN_STA_PENDING_POLL;

#ifndef CONFIG_NATIVE_WINDOWS
		/* FIX: WLAN_FC_STYPE_NULLFUNC would be more appropriate, but
		 * it is apparently not retried so TX Exc events are not
		 * received for it */
		os_memset(&hdr, 0, sizeof(hdr));
		hdr.frame_control =
			IEEE80211_FC(WLAN_FC_TYPE_DATA, WLAN_FC_STYPE_DATA);
		hdr.frame_control |= host_to_le16(BIT(1));
		hdr.frame_control |= host_to_le16(WLAN_FC_FROMDS);
		os_memcpy(hdr.IEEE80211_DA_FROMDS, sta->addr, ETH_ALEN);
		os_memcpy(hdr.IEEE80211_BSSID_FROMDS, hapd->own_addr,
			  ETH_ALEN);
		os_memcpy(hdr.IEEE80211_SA_FROMDS, hapd->own_addr, ETH_ALEN);

		if (hostapd_send_mgmt_frame(hapd, &hdr, sizeof(hdr), 0) < 0)
			perror("ap_handle_timer: send");
#endif /* CONFIG_NATIVE_WINDOWS */
	} else if (sta->timeout_next != STA_REMOVE) {
		int deauth = sta->timeout_next == STA_DEAUTH;

		wpa_printf(MSG_DEBUG, "Sending %s info to STA " MACSTR,
			   deauth ? "deauthentication" : "disassociation",
			   MAC2STR(sta->addr));

		if (deauth) {
			hostapd_sta_deauth(hapd, sta->addr,
					   WLAN_REASON_PREV_AUTH_NOT_VALID);
		} else {
			hostapd_sta_disassoc(
				hapd, sta->addr,
				WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY);
		}
	}

	switch (sta->timeout_next) {
	case STA_NULLFUNC:
		sta->timeout_next = STA_DISASSOC;
		eloop_register_timeout(AP_DISASSOC_DELAY, 0, ap_handle_timer,
				       hapd, sta);
		break;
	case STA_DISASSOC:
		sta->flags &= ~WLAN_STA_ASSOC;
		ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
		if (!sta->acct_terminate_cause)
			sta->acct_terminate_cause =
				RADIUS_ACCT_TERMINATE_CAUSE_IDLE_TIMEOUT;
		accounting_sta_stop(hapd, sta);
		ieee802_1x_free_station(sta);
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "disassociated due to "
			       "inactivity");
		sta->timeout_next = STA_DEAUTH;
		eloop_register_timeout(AP_DEAUTH_DELAY, 0, ap_handle_timer,
				       hapd, sta);
		mlme_disassociate_indication(
			hapd, sta, WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY);
		break;
	case STA_DEAUTH:
	case STA_REMOVE:
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "deauthenticated due to "
			       "inactivity");
		if (!sta->acct_terminate_cause)
			sta->acct_terminate_cause =
				RADIUS_ACCT_TERMINATE_CAUSE_IDLE_TIMEOUT;
		mlme_deauthenticate_indication(
			hapd, sta,
			WLAN_REASON_PREV_AUTH_NOT_VALID);
		ap_free_sta(hapd, sta);
		break;
	}
}


static void ap_handle_session_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;
	u8 addr[ETH_ALEN];

	if (!(sta->flags & WLAN_STA_AUTH))
		return;

	mlme_deauthenticate_indication(hapd, sta,
				       WLAN_REASON_PREV_AUTH_NOT_VALID);
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "deauthenticated due to "
		       "session timeout");
	sta->acct_terminate_cause =
		RADIUS_ACCT_TERMINATE_CAUSE_SESSION_TIMEOUT;
	os_memcpy(addr, sta->addr, ETH_ALEN);
	ap_free_sta(hapd, sta);
	hostapd_sta_deauth(hapd, addr, WLAN_REASON_PREV_AUTH_NOT_VALID);
}


void ap_sta_session_timeout(struct hostapd_data *hapd, struct sta_info *sta,
			    u32 session_timeout)
{
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "setting session timeout to %d "
		       "seconds", session_timeout);
	eloop_cancel_timeout(ap_handle_session_timer, hapd, sta);
	eloop_register_timeout(session_timeout, 0, ap_handle_session_timer,
			       hapd, sta);
}


void ap_sta_no_session_timeout(struct hostapd_data *hapd, struct sta_info *sta)
{
	eloop_cancel_timeout(ap_handle_session_timer, hapd, sta);
}


struct sta_info * ap_sta_add(struct hostapd_data *hapd, const u8 *addr)
{
	struct sta_info *sta;

	sta = ap_get_sta(hapd, addr);
	if (sta)
		return sta;

	wpa_printf(MSG_DEBUG, "  New STA");
	if (hapd->num_sta >= hapd->conf->max_num_sta) {
		/* FIX: might try to remove some old STAs first? */
		wpa_printf(MSG_DEBUG, "no more room for new STAs (%d/%d)",
			   hapd->num_sta, hapd->conf->max_num_sta);
		return NULL;
	}

	sta = os_zalloc(sizeof(struct sta_info));
	if (sta == NULL) {
		wpa_printf(MSG_ERROR, "malloc failed");
		return NULL;
	}
	sta->acct_interim_interval = hapd->conf->radius->acct_interim_interval;

	/* initialize STA info data */
	eloop_register_timeout(hapd->conf->ap_max_inactivity, 0,
			       ap_handle_timer, hapd, sta);
	os_memcpy(sta->addr, addr, ETH_ALEN);
	sta->next = hapd->sta_list;
	hapd->sta_list = sta;
	hapd->num_sta++;
	ap_sta_hash_add(hapd, sta);
	sta->ssid = &hapd->conf->ssid;

	return sta;
}


static int ap_sta_remove(struct hostapd_data *hapd, struct sta_info *sta)
{
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);

	wpa_printf(MSG_DEBUG, "Removing STA " MACSTR " from kernel driver",
		   MAC2STR(sta->addr));
	if (hostapd_sta_remove(hapd, sta->addr) &&
	    sta->flags & WLAN_STA_ASSOC) {
		wpa_printf(MSG_DEBUG, "Could not remove station " MACSTR
			   " from kernel driver.", MAC2STR(sta->addr));
		return -1;
	}
	return 0;
}


static int ap_sta_in_other_bss(struct hostapd_data *hapd,
			       struct sta_info *sta, u32 flags)
{
	struct hostapd_iface *iface = hapd->iface;
	size_t i;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss = iface->bss[i];
		struct sta_info *sta2;
		/* bss should always be set during operation, but it may be
		 * NULL during reconfiguration. Assume the STA is not
		 * associated to another BSS in that case to avoid NULL pointer
		 * dereferences. */
		if (bss == hapd || bss == NULL)
			continue;
		sta2 = ap_get_sta(bss, sta->addr);
		if (sta2 && ((sta2->flags & flags) == flags))
			return 1;
	}

	return 0;
}


void ap_sta_disassociate(struct hostapd_data *hapd, struct sta_info *sta,
			 u16 reason)
{
	wpa_printf(MSG_DEBUG, "%s: disassociate STA " MACSTR,
		   hapd->conf->iface, MAC2STR(sta->addr));
	sta->flags &= ~WLAN_STA_ASSOC;
	if (!ap_sta_in_other_bss(hapd, sta, WLAN_STA_ASSOC))
		ap_sta_remove(hapd, sta);
	sta->timeout_next = STA_DEAUTH;
	eloop_cancel_timeout(ap_handle_timer, hapd, sta);
	eloop_register_timeout(AP_MAX_INACTIVITY_AFTER_DISASSOC, 0,
			       ap_handle_timer, hapd, sta);
	accounting_sta_stop(hapd, sta);
	ieee802_1x_free_station(sta);

	mlme_disassociate_indication(hapd, sta, reason);
}


void ap_sta_deauthenticate(struct hostapd_data *hapd, struct sta_info *sta,
			   u16 reason)
{
	wpa_printf(MSG_DEBUG, "%s: deauthenticate STA " MACSTR,
		   hapd->conf->iface, MAC2STR(sta->addr));
	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
	if (!ap_sta_in_other_bss(hapd, sta, WLAN_STA_ASSOC))
		ap_sta_remove(hapd, sta);
	sta->timeout_next = STA_REMOVE;
	eloop_cancel_timeout(ap_handle_timer, hapd, sta);
	eloop_register_timeout(AP_MAX_INACTIVITY_AFTER_DEAUTH, 0,
			       ap_handle_timer, hapd, sta);
	accounting_sta_stop(hapd, sta);
	ieee802_1x_free_station(sta);

	mlme_deauthenticate_indication(hapd, sta, reason);
}


int ap_sta_bind_vlan(struct hostapd_data *hapd, struct sta_info *sta,
		     int old_vlanid)
{
	const char *iface;
	struct hostapd_vlan *vlan = NULL;

	/*
	 * Do not proceed furthur if the vlan id remains same. We do not want
	 * duplicate dynamic vlan entries.
	 */
	if (sta->vlan_id == old_vlanid)
		return 0;

	/*
	 * During 1x reauth, if the vlan id changes, then remove the old id and
	 * proceed furthur to add the new one.
	 */
	if (old_vlanid > 0)
		vlan_remove_dynamic(hapd, old_vlanid);

	iface = hapd->conf->iface;
	if (sta->ssid->vlan[0])
		iface = sta->ssid->vlan;

	if (sta->ssid->dynamic_vlan == DYNAMIC_VLAN_DISABLED)
		sta->vlan_id = 0;
	else if (sta->vlan_id > 0) {
		vlan = hapd->conf->vlan;
		while (vlan) {
			if (vlan->vlan_id == sta->vlan_id ||
			    vlan->vlan_id == VLAN_ID_WILDCARD) {
				iface = vlan->ifname;
				break;
			}
			vlan = vlan->next;
		}
	}

	if (sta->vlan_id > 0 && vlan == NULL) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG, "could not find VLAN for "
			       "binding station to (vlan_id=%d)",
			       sta->vlan_id);
		return -1;
	} else if (sta->vlan_id > 0 && vlan->vlan_id == VLAN_ID_WILDCARD) {
		vlan = vlan_add_dynamic(hapd, vlan, sta->vlan_id);
		if (vlan == NULL) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG, "could not add "
				       "dynamic VLAN interface for vlan_id=%d",
				       sta->vlan_id);
			return -1;
		}

		iface = vlan->ifname;
		if (vlan_setup_encryption_dyn(hapd, sta->ssid, iface) != 0) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG, "could not "
				       "configure encryption for dynamic VLAN "
				       "interface for vlan_id=%d",
				       sta->vlan_id);
		}

		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG, "added new dynamic VLAN "
			       "interface '%s'", iface);
	} else if (vlan && vlan->vlan_id == sta->vlan_id) {
		if (sta->vlan_id > 0) {
			vlan->dynamic_vlan++;
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG, "updated existing "
				       "dynamic VLAN interface '%s'", iface);
		}

		/*
		 * Update encryption configuration for statically generated
		 * VLAN interface. This is only used for static WEP
		 * configuration for the case where hostapd did not yet know
		 * which keys are to be used when the interface was added.
		 */
		if (vlan_setup_encryption_dyn(hapd, sta->ssid, iface) != 0) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG, "could not "
				       "configure encryption for VLAN "
				       "interface for vlan_id=%d",
				       sta->vlan_id);
		}
	}

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "binding station to interface "
		       "'%s'", iface);

	if (wpa_auth_sta_set_vlan(sta->wpa_sm, sta->vlan_id) < 0)
		wpa_printf(MSG_INFO, "Failed to update VLAN-ID for WPA");

	return hostapd_set_sta_vlan(iface, hapd, sta->addr, sta->vlan_id);
}
