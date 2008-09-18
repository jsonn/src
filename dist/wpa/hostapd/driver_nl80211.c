/*
 * hostapd / Kernel driver communication via nl80211
 * Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
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

#include <sys/ioctl.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */
#include "wireless_copy.h"
#include <net/if_arp.h>

#include "hostapd.h"
#include "driver.h"
#include "ieee802_1x.h"
#include "eloop.h"
#include "ieee802_11.h"
#include "sta_info.h"
#include "hw_features.h"
#include "mlme.h"
#include "radiotap.h"
#include "radiotap_iter.h"

enum ieee80211_msg_type {
	ieee80211_msg_normal = 0,
	ieee80211_msg_tx_callback_ack = 1,
	ieee80211_msg_tx_callback_fail = 2,
};

struct i802_driver_data {
	struct hostapd_data *hapd;

	char iface[IFNAMSIZ + 1];
	int bridge;
	int ioctl_sock; /* socket for ioctl() use */
	int wext_sock; /* socket for wireless events */
	int eapol_sock; /* socket for EAPOL frames */
	int monitor_sock; /* socket for monitor */
	int monitor_ifidx;

	int default_if_indices[16];
	int *if_indices;
	int num_if_indices;

	int we_version;
	struct nl_handle *nl_handle;
	struct nl_cache *nl_cache;
	struct nl_cb *nl_cb;
	struct genl_family *nl80211;
	int dtim_period, beacon_int;
	unsigned int beacon_set:1;
	unsigned int ieee802_1x_active:1;
};


static void add_ifidx(struct i802_driver_data *drv, int ifidx)
{
	int i;
	int *old;

	for (i = 0; i < drv->num_if_indices; i++) {
		if (drv->if_indices[i] == 0) {
			drv->if_indices[i] = ifidx;
			return;
		}
	}

	if (drv->if_indices != drv->default_if_indices)
		old = drv->if_indices;
	else
		old = NULL;

	drv->if_indices = realloc(old,
				  sizeof(int) * (drv->num_if_indices + 1));
	if (!drv->if_indices) {
		if (!old)
			drv->if_indices = drv->default_if_indices;
		else
			drv->if_indices = old;
		wpa_printf(MSG_ERROR, "Failed to reallocate memory for "
			   "interfaces");
		wpa_printf(MSG_ERROR, "Ignoring EAPOL on interface %d", ifidx);
		return;
	}
	drv->if_indices[drv->num_if_indices] = ifidx;
	drv->num_if_indices++;
}


static void del_ifidx(struct i802_driver_data *drv, int ifidx)
{
	int i;

	for (i = 0; i < drv->num_if_indices; i++) {
		if (drv->if_indices[i] == ifidx) {
			drv->if_indices[i] = 0;
			break;
		}
	}
}


static int have_ifidx(struct i802_driver_data *drv, int ifidx)
{
	int i;

	if (ifidx == drv->bridge)
		return 1;

	for (i = 0; i < drv->num_if_indices; i++)
		if (drv->if_indices[i] == ifidx)
			return 1;

	return 0;
}


/* helper for netlink get routines */
static int ack_wait_handler(struct nl_msg *msg, void *arg)
{
	int *finished = arg;

	*finished = 1;
	return NL_STOP;
}


static int hostapd_set_iface_flags(struct i802_driver_data *drv,
				   const char *ifname, int dev_up)
{
	struct ifreq ifr;

	if (drv->ioctl_sock < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);

	if (ioctl(drv->ioctl_sock, SIOCGIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCGIFFLAGS]");
		wpa_printf(MSG_DEBUG, "Could not read interface flags (%s)",
			   drv->iface);
		return -1;
	}

	if (dev_up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~IFF_UP;

	if (ioctl(drv->ioctl_sock, SIOCSIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCSIFFLAGS]");
		return -1;
	}

	return 0;
}


static int nl_set_encr(int ifindex, struct i802_driver_data *drv,
		       const char *alg, const u8 *addr, int idx, const u8 *key,
		       size_t key_len, int txkey)
{
	struct nl_msg *msg;
	int ret = -1;
	int err = 0;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	if (strcmp(alg, "none") == 0) {
		genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
			    0, NL80211_CMD_DEL_KEY, 0);
	} else {
		genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
			    0, NL80211_CMD_NEW_KEY, 0);
		NLA_PUT(msg, NL80211_ATTR_KEY_DATA, key_len, key);
		if (strcmp(alg, "WEP") == 0) {
			if (key_len == 5)
				NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
					    0x000FAC01);
			else
				NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
					    0x000FAC05);
		} else if (strcmp(alg, "TKIP") == 0)
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER, 0x000FAC02);
		else if (strcmp(alg, "CCMP") == 0)
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER, 0x000FAC04);
		else if (strcmp(alg, "IGTK") == 0)
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER, 0x000FAC06);
		else {
			wpa_printf(MSG_ERROR, "%s: Unsupported encryption "
				   "algorithm '%s'", __func__, alg);
			goto out;
		}
	}

	if (addr)
		NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);
	NLA_PUT_U8(msg, NL80211_ATTR_KEY_IDX, idx);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifindex);

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    (err = nl_wait_for_ack(drv->nl_handle)) < 0) {
		if (err != -ENOENT) {
			ret = 0;
			goto out;
		}
	}

	/*
	 * If we need to set the default TX key we do that below,
	 * otherwise we're done here.
	 */
	if (!txkey || addr) {
		ret = 0;
		goto out;
	}

	nlmsg_free(msg);

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_SET_KEY, 0);
	NLA_PUT_U8(msg, NL80211_ATTR_KEY_IDX, idx);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifindex);
#ifdef NL80211_MFP_PENDING
	if (strcmp(alg, "IGTK") == 0)
		NLA_PUT_FLAG(msg, NL80211_ATTR_KEY_DEFAULT_MGMT);
	else
		NLA_PUT_FLAG(msg, NL80211_ATTR_KEY_DEFAULT);
#else /* NL80211_MFP_PENDING */
	NLA_PUT_FLAG(msg, NL80211_ATTR_KEY_DEFAULT);
#endif /* NL80211_MFP_PENDING */

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    (err = nl_wait_for_ack(drv->nl_handle)) < 0) {
		if (err != -ENOENT) {
			ret = 0;
			goto out;
		}
	}

	ret = 0;

 out:
 nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int i802_set_encryption(const char *iface, void *priv, const char *alg,
			       const u8 *addr, int idx, const u8 *key,
			       size_t key_len, int txkey)
{
	struct i802_driver_data *drv = priv;
	int ret;

	ret = nl_set_encr(if_nametoindex(iface), drv, alg, addr, idx, key,
			  key_len, txkey);
	if (ret < 0)
		return ret;

	if (strcmp(alg, "IGTK") == 0) {
		ret = nl_set_encr(drv->monitor_ifidx, drv, alg, addr, idx, key,
				  key_len, txkey);
	}

	return ret;
}


static inline int min_int(int a, int b)
{
	if (a < b)
		return a;
	return b;
}


static int get_key_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	/*
	 * TODO: validate the key index and mac address!
	 * Otherwise, there's a race condition as soon as
	 * the kernel starts sending key notifications.
	 */

	if (tb[NL80211_ATTR_KEY_SEQ])
		memcpy(arg, nla_data(tb[NL80211_ATTR_KEY_SEQ]),
		       min_int(nla_len(tb[NL80211_ATTR_KEY_SEQ]), 6));
	return NL_SKIP;
}


static int i802_get_seqnum(const char *iface, void *priv, const u8 *addr,
			   int idx, u8 *seq)
{
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg;
	struct nl_cb *cb = NULL;
	int ret = -1;
	int err = 0;
	int finished = 0;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_GET_KEY, 0);

	if (addr)
		NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);
	NLA_PUT_U8(msg, NL80211_ATTR_KEY_IDX, idx);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(iface));

	cb = nl_cb_clone(drv->nl_cb);
	if (!cb)
		goto out;

	memset(seq, 0, 6);

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0)
		goto out;

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, get_key_handler, seq);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_wait_handler, &finished);

	err = nl_recvmsgs(drv->nl_handle, cb);

	if (!finished)
		err = nl_wait_for_ack(drv->nl_handle);

	if (err < 0)
		goto out;

	ret = 0;

 out:
	nl_cb_put(cb);
 nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int i802_set_rate_sets(void *priv, int *supp_rates, int *basic_rates,
			      int mode)
{
	return -1;
}


static int i802_set_ssid(const char *ifname, void *priv, const u8 *buf,
			 int len)
{
	struct i802_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, ifname, IFNAMSIZ);
	iwr.u.essid.flags = 1; /* SSID active */
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len;

	if (ioctl(drv->ioctl_sock, SIOCSIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCSIWESSID]");
		printf("len=%d\n", len);
		return -1;
	}

	return 0;
}


static int i802_send_mgmt_frame(void *priv, const void *data, size_t len,
				int flags)
{
	struct ieee80211_hdr *hdr = (void*) data;
	__u8 rtap_hdr[] = {
		0x00, 0x00, /* radiotap version */
		0x0e, 0x00, /* radiotap length */
		0x02, 0xc0, 0x00, 0x00, /* bmap: flags, tx and rx flags */
		0x0c,       /* F_WEP | F_FRAG (encrypt/fragment if required) */
		0x00,       /* padding */
		0x00, 0x00, /* RX and TX flags to indicate that */
		0x00, 0x00, /* this is the injected frame directly */
	};
	struct i802_driver_data *drv = priv;
	struct iovec iov[2] = {
		{
			.iov_base = &rtap_hdr,
			.iov_len = sizeof(rtap_hdr),
		},
		{
			.iov_base = (void*)data,
			.iov_len = len,
		}
	};
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = iov,
		.msg_iovlen = 2,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0,
	};

	/*
	 * ugh, guess what, the generic code sets one of the version
	 * bits to request tx callback
	 */
	hdr->frame_control &= ~host_to_le16(BIT(1));
	return sendmsg(drv->monitor_sock, &msg, flags);
}


/* Set kernel driver on given frequency (MHz) */
static int i802_set_freq(void *priv, int mode, int freq)
{
	struct i802_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->hapd->conf->iface, IFNAMSIZ);
	iwr.u.freq.m = freq;
	iwr.u.freq.e = 6;

	if (ioctl(drv->ioctl_sock, SIOCSIWFREQ, &iwr) < 0) {
		perror("ioctl[SIOCSIWFREQ]");
		return -1;
	}

	return 0;
}


static int i802_set_rts(void *priv, int rts)
{
	struct i802_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->hapd->conf->iface, IFNAMSIZ);
	iwr.u.rts.value = rts;
	iwr.u.rts.fixed = 1;

	if (ioctl(drv->ioctl_sock, SIOCSIWRTS, &iwr) < 0) {
		perror("ioctl[SIOCSIWRTS]");
		return -1;
	}

	return 0;
}


static int i802_get_rts(void *priv, int *rts)
{
	struct i802_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->hapd->conf->iface, IFNAMSIZ);

	if (ioctl(drv->ioctl_sock, SIOCGIWRTS, &iwr) < 0) {
		perror("ioctl[SIOCGIWRTS]");
		return -1;
	}

	*rts = iwr.u.rts.value;

	return 0;
}


static int i802_set_frag(void *priv, int frag)
{
	struct i802_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->hapd->conf->iface, IFNAMSIZ);
	iwr.u.frag.value = frag;
	iwr.u.frag.fixed = 1;

	if (ioctl(drv->ioctl_sock, SIOCSIWFRAG, &iwr) < 0) {
		perror("ioctl[SIOCSIWFRAG]");
		return -1;
	}

	return 0;
}


static int i802_get_frag(void *priv, int *frag)
{
	struct i802_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->hapd->conf->iface, IFNAMSIZ);

	if (ioctl(drv->ioctl_sock, SIOCGIWFRAG, &iwr) < 0) {
		perror("ioctl[SIOCGIWFRAG]");
		return -1;
	}

	*frag = iwr.u.frag.value;

	return 0;
}


static int i802_set_retry(void *priv, int short_retry, int long_retry)
{
	struct i802_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->hapd->conf->iface, IFNAMSIZ);

	iwr.u.retry.value = short_retry;
	iwr.u.retry.flags = IW_RETRY_LIMIT | IW_RETRY_MIN;
	if (ioctl(drv->ioctl_sock, SIOCSIWFRAG, &iwr) < 0) {
		perror("ioctl[SIOCSIWRETRY(short)]");
		return -1;
	}

	iwr.u.retry.value = long_retry;
	iwr.u.retry.flags = IW_RETRY_LIMIT | IW_RETRY_MAX;
	if (ioctl(drv->ioctl_sock, SIOCSIWFRAG, &iwr) < 0) {
		perror("ioctl[SIOCSIWRETRY(long)]");
		return -1;
	}

	return 0;
}


static int i802_get_retry(void *priv, int *short_retry, int *long_retry)
{
	struct i802_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->hapd->conf->iface, IFNAMSIZ);

	iwr.u.retry.flags = IW_RETRY_LIMIT | IW_RETRY_MIN;
	if (ioctl(drv->ioctl_sock, SIOCGIWRETRY, &iwr) < 0) {
		perror("ioctl[SIOCGIWFRAG(short)]");
		return -1;
	}
	*short_retry = iwr.u.retry.value;

	iwr.u.retry.flags = IW_RETRY_LIMIT | IW_RETRY_MAX;
	if (ioctl(drv->ioctl_sock, SIOCGIWRETRY, &iwr) < 0) {
		perror("ioctl[SIOCGIWFRAG(long)]");
		return -1;
	}
	*long_retry = iwr.u.retry.value;

	return 0;
}


static int i802_flush(void *priv)
{
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_DEL_STATION, 0);

	/*
	 * XXX: FIX! this needs to flush all VLANs too
	 */
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(drv->iface));

	ret = 0;

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    nl_wait_for_ack(drv->nl_handle) < 0) {
		ret = -1;
	}

 nla_put_failure:
	nlmsg_free(msg);

 out:
	return ret;
}


static int get_sta_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct hostap_sta_driver_data *data = arg;
	struct nlattr *stats[NL80211_STA_INFO_MAX + 1];
	static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
		[NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32 },
		[NL80211_STA_INFO_RX_BYTES] = { .type = NLA_U32 },
		[NL80211_STA_INFO_TX_BYTES] = { .type = NLA_U32 },
	};

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	/*
	 * TODO: validate the interface and mac address!
	 * Otherwise, there's a race condition as soon as
	 * the kernel starts sending station notifications.
	 */

	if (!tb[NL80211_ATTR_STA_INFO]) {
		wpa_printf(MSG_DEBUG, "sta stats missing!");
		return NL_SKIP;
	}
	if (nla_parse_nested(stats, NL80211_STA_INFO_MAX,
			     tb[NL80211_ATTR_STA_INFO],
			     stats_policy)) {
		wpa_printf(MSG_DEBUG, "failed to parse nested attributes!");
		return NL_SKIP;
	}

	if (stats[NL80211_STA_INFO_INACTIVE_TIME])
		data->inactive_msec =
			nla_get_u32(stats[NL80211_STA_INFO_INACTIVE_TIME]);
	if (stats[NL80211_STA_INFO_RX_BYTES])
		data->rx_bytes = nla_get_u32(stats[NL80211_STA_INFO_RX_BYTES]);
	if (stats[NL80211_STA_INFO_TX_BYTES])
		data->rx_bytes = nla_get_u32(stats[NL80211_STA_INFO_TX_BYTES]);

	return NL_SKIP;
}

static int i802_read_sta_data(void *priv, struct hostap_sta_driver_data *data,
			      const u8 *addr)
{
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg;
	struct nl_cb *cb = NULL;
	int ret = -1;
	int err = 0;
	int finished = 0;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_GET_STATION, 0);

	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(drv->iface));

	cb = nl_cb_clone(drv->nl_cb);
	if (!cb)
		goto out;

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0)
		goto out;

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, get_sta_handler, data);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_wait_handler, &finished);

	err = nl_recvmsgs(drv->nl_handle, cb);

	if (!finished)
		err = nl_wait_for_ack(drv->nl_handle);

	if (err < 0)
		goto out;

	ret = 0;

 out:
	nl_cb_put(cb);
 nla_put_failure:
	nlmsg_free(msg);
	return ret;

}


static int i802_send_eapol(void *priv, const u8 *addr, const u8 *data,
			   size_t data_len, int encrypt, const u8 *own_addr)
{
	struct i802_driver_data *drv = priv;
	struct ieee80211_hdr *hdr;
	size_t len;
	u8 *pos;
	int res;
#if 0 /* FIX */
	int qos = sta->flags & WLAN_STA_WME;
#else
	int qos = 0;
#endif

	len = sizeof(*hdr) + (qos ? 2 : 0) + sizeof(rfc1042_header) + 2 +
		data_len;
	hdr = os_zalloc(len);
	if (hdr == NULL) {
		printf("malloc() failed for i802_send_data(len=%lu)\n",
		       (unsigned long) len);
		return -1;
	}

	hdr->frame_control =
		IEEE80211_FC(WLAN_FC_TYPE_DATA, WLAN_FC_STYPE_DATA);
	hdr->frame_control |= host_to_le16(WLAN_FC_FROMDS);
	if (encrypt)
		hdr->frame_control |= host_to_le16(WLAN_FC_ISWEP);
#if 0 /* To be enabled if qos determination is added above */
	if (qos) {
		hdr->frame_control |=
			host_to_le16(WLAN_FC_STYPE_QOS_DATA << 4);
	}
#endif

	memcpy(hdr->IEEE80211_DA_FROMDS, addr, ETH_ALEN);
	memcpy(hdr->IEEE80211_BSSID_FROMDS, own_addr, ETH_ALEN);
	memcpy(hdr->IEEE80211_SA_FROMDS, own_addr, ETH_ALEN);
	pos = (u8 *) (hdr + 1);

#if 0 /* To be enabled if qos determination is added above */
	if (qos) {
		/* add an empty QoS header if needed */
		pos[0] = 0;
		pos[1] = 0;
		pos += 2;
	}
#endif

	memcpy(pos, rfc1042_header, sizeof(rfc1042_header));
	pos += sizeof(rfc1042_header);
	WPA_PUT_BE16(pos, ETH_P_PAE);
	pos += 2;
	memcpy(pos, data, data_len);

	res = i802_send_mgmt_frame(drv, (u8 *) hdr, len, 0);
	free(hdr);

	if (res < 0) {
		perror("i802_send_eapol: send");
		printf("i802_send_eapol - packet len: %lu - failed\n",
		       (unsigned long) len);
	}

	return res;
}


static int i802_sta_add(const char *ifname, void *priv, const u8 *addr,
			u16 aid, u16 capability, u8 *supp_rates,
			size_t supp_rates_len, int flags, u16 listen_interval)
{
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_NEW_STATION, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(drv->iface));
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);
	NLA_PUT_U16(msg, NL80211_ATTR_STA_AID, aid);
	NLA_PUT(msg, NL80211_ATTR_STA_SUPPORTED_RATES, supp_rates_len,
		supp_rates);
	NLA_PUT_U16(msg, NL80211_ATTR_STA_LISTEN_INTERVAL, listen_interval);

	ret = nl_send_auto_complete(drv->nl_handle, msg);
	if (ret < 0)
		goto nla_put_failure;

	ret = nl_wait_for_ack(drv->nl_handle);
	/* ignore EEXIST, this happens if a STA associates while associated */
	if (ret == -EEXIST || ret >= 0)
		ret = 0;

 nla_put_failure:
	nlmsg_free(msg);

 out:
	return ret;
}


static int i802_sta_remove(void *priv, const u8 *addr)
{
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_DEL_STATION, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(drv->iface));
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);

	ret = 0;

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    nl_wait_for_ack(drv->nl_handle) < 0) {
		ret = -1;
	}

 nla_put_failure:
	nlmsg_free(msg);

 out:
	return ret;
}


static int i802_sta_set_flags(void *priv, const u8 *addr,
			      int total_flags, int flags_or, int flags_and)
{
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg, *flags = NULL;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	flags = nlmsg_alloc();
	if (!flags)
		goto free_msg;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_SET_STATION, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(drv->iface));
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);

	if (total_flags & WLAN_STA_AUTHORIZED || !drv->ieee802_1x_active)
		NLA_PUT_FLAG(flags, NL80211_STA_FLAG_AUTHORIZED);

	if (total_flags & WLAN_STA_WME)
		NLA_PUT_FLAG(flags, NL80211_STA_FLAG_WME);

	if (total_flags & WLAN_STA_SHORT_PREAMBLE)
		NLA_PUT_FLAG(flags, NL80211_STA_FLAG_SHORT_PREAMBLE);

#ifdef NL80211_MFP_PENDING
	if (total_flags & WLAN_STA_MFP)
		NLA_PUT_FLAG(flags, NL80211_STA_FLAG_MFP);
#endif /* NL80211_MFP_PENDING */

	if (nla_put_nested(msg, NL80211_ATTR_STA_FLAGS, flags))
		goto nla_put_failure;

	ret = 0;

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    nl_wait_for_ack(drv->nl_handle) < 0) {
		ret = -1;
	}

 nla_put_failure:
	nlmsg_free(flags);

 free_msg:
	nlmsg_free(msg);

 out:
	return ret;
}


static int i802_set_channel_flag(void *priv, int mode, int chan, int flag,
				 unsigned char power_level,
				 unsigned char antenna_max)
{
	return -1;
}


static int i802_set_regulatory_domain(void *priv, unsigned int rd)
{
	return -1;
}


static int i802_set_tx_queue_params(void *priv, int queue, int aifs,
				    int cw_min, int cw_max, int burst_time)
{
	return -1;
}


static void nl80211_remove_iface(struct i802_driver_data *drv, int ifidx)
{
	struct nl_msg *msg;

	/* stop listening for EAPOL on this interface */
	del_ifidx(drv, ifidx);

	msg = nlmsg_alloc();
	if (!msg)
		goto nla_put_failure;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_DEL_INTERFACE, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifidx);
	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    nl_wait_for_ack(drv->nl_handle) < 0)
	nla_put_failure:
		printf("Failed to remove interface.\n");
	nlmsg_free(msg);
}


static int nl80211_create_iface(struct i802_driver_data *drv,
				const char *ifname,
				enum nl80211_iftype iftype,
				const u8 *addr)
{
	struct nl_msg *msg, *flags = NULL;
	int ifidx;
	struct ifreq ifreq;
	struct iwreq iwr;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_NEW_INTERFACE, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(drv->hapd->conf->iface));
	NLA_PUT_STRING(msg, NL80211_ATTR_IFNAME, ifname);
	NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, iftype);

	if (iftype == NL80211_IFTYPE_MONITOR) {
		int err;

		flags = nlmsg_alloc();
		if (!flags)
			goto nla_put_failure;

		NLA_PUT_FLAG(flags, NL80211_MNTR_FLAG_COOK_FRAMES);

		err = nla_put_nested(msg, NL80211_ATTR_MNTR_FLAGS, flags);

		nlmsg_free(flags);

		if (err)
			goto nla_put_failure;
	}

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    nl_wait_for_ack(drv->nl_handle) < 0) {
 nla_put_failure:
		printf("Failed to create interface %s.\n", ifname);
		nlmsg_free(msg);
		return -1;
	}

	nlmsg_free(msg);

	ifidx = if_nametoindex(ifname);

	if (ifidx <= 0)
		return -1;

	/* start listening for EAPOL on this interface */
	add_ifidx(drv, ifidx);

	if (addr) {
		switch (iftype) {
		case NL80211_IFTYPE_AP:
			os_strlcpy(ifreq.ifr_name, ifname, IFNAMSIZ);
			memcpy(ifreq.ifr_hwaddr.sa_data, addr, ETH_ALEN);
			ifreq.ifr_hwaddr.sa_family = ARPHRD_ETHER;

			if (ioctl(drv->ioctl_sock, SIOCSIFHWADDR, &ifreq)) {
				nl80211_remove_iface(drv, ifidx);
				return -1;
			}
			break;
		case NL80211_IFTYPE_WDS:
			memset(&iwr, 0, sizeof(iwr));
			os_strlcpy(iwr.ifr_name, ifname, IFNAMSIZ);
			iwr.u.addr.sa_family = ARPHRD_ETHER;
			memcpy(iwr.u.addr.sa_data, addr, ETH_ALEN);
			if (ioctl(drv->ioctl_sock, SIOCSIWAP, &iwr))
				return -1;
			break;
		default:
			/* nothing */
			break;
		}
	}

	return ifidx;
}


static int i802_bss_add(void *priv, const char *ifname, const u8 *bssid)
{
	int ifidx;

	/*
	 * The kernel supports that when the low-level driver does,
	 * but we currently don't because we need per-BSS data that
	 * currently we can't handle easily.
	 */
	return -1;

	ifidx = nl80211_create_iface(priv, ifname, NL80211_IFTYPE_AP, bssid);
	if (ifidx < 0)
		return -1;
	if (hostapd_set_iface_flags(priv, ifname, 1)) {
		nl80211_remove_iface(priv, ifidx);
		return -1;
	}
	return 0;
}


static int i802_bss_remove(void *priv, const char *ifname)
{
	nl80211_remove_iface(priv, if_nametoindex(ifname));
	return 0;
}


static int i802_set_beacon(const char *iface, void *priv,
			   u8 *head, size_t head_len,
			   u8 *tail, size_t tail_len)
{
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg;
	u8 cmd = NL80211_CMD_NEW_BEACON;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	if (drv->beacon_set)
		cmd = NL80211_CMD_SET_BEACON;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, cmd, 0);
	NLA_PUT(msg, NL80211_ATTR_BEACON_HEAD, head_len, head);
	NLA_PUT(msg, NL80211_ATTR_BEACON_TAIL, tail_len, tail);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(iface));
	NLA_PUT_U32(msg, NL80211_ATTR_BEACON_INTERVAL, drv->beacon_int);

	if (!drv->dtim_period)
		drv->dtim_period = 2;
	NLA_PUT_U32(msg, NL80211_ATTR_DTIM_PERIOD, drv->dtim_period);

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    nl_wait_for_ack(drv->nl_handle) < 0)
		goto out;

	ret = 0;

	drv->beacon_set = 1;

 out:
 nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int i802_del_beacon(struct i802_driver_data *drv)
{
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_DEL_BEACON, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(drv->iface));

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    nl_wait_for_ack(drv->nl_handle) < 0)
		goto out;

	ret = 0;

 out:
 nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int i802_set_ieee8021x(const char *ifname, void *priv, int enabled)
{
	struct i802_driver_data *drv = priv;

	/*
	 * FIXME: This needs to be per interface (BSS)
	 */
	drv->ieee802_1x_active = enabled;
	return 0;
}


static int i802_set_privacy(const char *ifname, void *priv, int enabled)
{
	struct i802_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));

	os_strlcpy(iwr.ifr_name, ifname, IFNAMSIZ);
	iwr.u.param.flags = IW_AUTH_PRIVACY_INVOKED;
	iwr.u.param.value = enabled;

	ioctl(drv->ioctl_sock, SIOCSIWAUTH, &iwr);

	/* ignore errors, the kernel/driver might not care */
	return 0;
}


static int i802_set_internal_bridge(void *priv, int value)
{
	return -1;
}


static int i802_set_beacon_int(void *priv, int value)
{
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg;
	int ret = -1;

	drv->beacon_int = value;

	if (!drv->beacon_set)
		return 0;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_SET_BEACON, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(drv->iface));

	NLA_PUT_U32(msg, NL80211_ATTR_BEACON_INTERVAL, value);

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    nl_wait_for_ack(drv->nl_handle) < 0)
		goto out;

	ret = 0;

 out:
 nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int i802_set_dtim_period(const char *iface, void *priv, int value)
{
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_SET_BEACON, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(iface));

	drv->dtim_period = value;
	NLA_PUT_U32(msg, NL80211_ATTR_DTIM_PERIOD, drv->dtim_period);

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    nl_wait_for_ack(drv->nl_handle) < 0)
		goto out;

	ret = 0;

 out:
 nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int i802_set_bss(void *priv, int cts, int preamble, int slot)
{
#ifdef NL80211_CMD_SET_BSS
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0, 0,
		    NL80211_CMD_SET_BSS, 0);

	if (cts >= 0)
		NLA_PUT_U8(msg, NL80211_ATTR_BSS_CTS_PROT, cts);
	if (preamble >= 0)
		NLA_PUT_U8(msg, NL80211_ATTR_BSS_SHORT_PREAMBLE, preamble);
	if (slot >= 0)
		NLA_PUT_U8(msg, NL80211_ATTR_BSS_SHORT_SLOT_TIME, slot);

	ret = 0;

	/* TODO: multi-BSS support */
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(drv->iface));

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    nl_wait_for_ack(drv->nl_handle) < 0) {
		ret = -1;
	}

nla_put_failure:
	nlmsg_free(msg);

out:
	return ret;
#else /* NL80211_CMD_SET_BSS */
	return -1;
#endif /* NL80211_CMD_SET_BSS */
}


static int i802_set_cts_protect(void *priv, int value)
{
	return i802_set_bss(priv, value, -1, -1);
}


static int i802_set_preamble(void *priv, int value)
{
	return i802_set_bss(priv, -1, value, -1);
}


static int i802_set_short_slot_time(void *priv, int value)
{
	return i802_set_bss(priv, -1, -1, value);
}


static enum nl80211_iftype i802_if_type(enum hostapd_driver_if_type type)
{
	switch (type) {
	case HOSTAPD_IF_VLAN:
		return NL80211_IFTYPE_AP_VLAN;
	case HOSTAPD_IF_WDS:
		return NL80211_IFTYPE_WDS;
	}
	return -1;
}


static int i802_if_add(const char *iface, void *priv,
		       enum hostapd_driver_if_type type, char *ifname,
		       const u8 *addr)
{
	if (nl80211_create_iface(priv, ifname, i802_if_type(type), addr) < 0)
		return -1;
	return 0;
}


static int i802_if_update(void *priv, enum hostapd_driver_if_type type,
			  char *ifname, const u8 *addr)
{
	/* unused at the moment */
	return -1;
}


static int i802_if_remove(void *priv, enum hostapd_driver_if_type type,
			  const char *ifname, const u8 *addr)
{
	nl80211_remove_iface(priv, if_nametoindex(ifname));
	return 0;
}


struct phy_info_arg {
	u16 *num_modes;
	struct hostapd_hw_modes *modes;
	int error;
};

static int phy_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct phy_info_arg *phy_info = arg;

	struct nlattr *tb_band[NL80211_BAND_ATTR_MAX + 1];

	struct nlattr *tb_freq[NL80211_FREQUENCY_ATTR_MAX + 1];
	static struct nla_policy freq_policy[NL80211_FREQUENCY_ATTR_MAX + 1] = {
		[NL80211_FREQUENCY_ATTR_FREQ] = { .type = NLA_U32 },
		[NL80211_FREQUENCY_ATTR_DISABLED] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_PASSIVE_SCAN] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_NO_IBSS] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_RADAR] = { .type = NLA_FLAG },
	};

	struct nlattr *tb_rate[NL80211_BITRATE_ATTR_MAX + 1];
	static struct nla_policy rate_policy[NL80211_BITRATE_ATTR_MAX + 1] = {
		[NL80211_BITRATE_ATTR_RATE] = { .type = NLA_U32 },
		[NL80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE] = { .type = NLA_FLAG },
	};

	struct nlattr *nl_band;
	struct nlattr *nl_freq;
	struct nlattr *nl_rate;
	int rem_band, rem_freq, rem_rate;
	struct hostapd_hw_modes *mode;
	int idx, mode_is_set;

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb_msg[NL80211_ATTR_WIPHY_BANDS])
		return NL_SKIP;

	nla_for_each_nested(nl_band, tb_msg[NL80211_ATTR_WIPHY_BANDS], rem_band) {
		mode = realloc(phy_info->modes, (*phy_info->num_modes + 1) * sizeof(*mode));
		if (!mode)
			return NL_SKIP;
		phy_info->modes = mode;

		mode_is_set = 0;

		mode = &phy_info->modes[*(phy_info->num_modes)];
		memset(mode, 0, sizeof(*mode));
		*(phy_info->num_modes) += 1;

		nla_parse(tb_band, NL80211_BAND_ATTR_MAX, nla_data(nl_band),
			  nla_len(nl_band), NULL);

		nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS], rem_freq) {
			nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX, nla_data(nl_freq),
				  nla_len(nl_freq), freq_policy);
			if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
				continue;
			mode->num_channels++;
		}

		mode->channels = calloc(mode->num_channels, sizeof(struct hostapd_channel_data));
		if (!mode->channels)
			return NL_SKIP;

		idx = 0;

		nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS], rem_freq) {
			nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX, nla_data(nl_freq),
				  nla_len(nl_freq), freq_policy);
			if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
				continue;

			mode->channels[idx].freq = nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_FREQ]);
			mode->channels[idx].flag |= HOSTAPD_CHAN_W_SCAN |
						    HOSTAPD_CHAN_W_ACTIVE_SCAN |
						    HOSTAPD_CHAN_W_IBSS;

			if (!mode_is_set) {
				/* crude heuristic */
				if (mode->channels[idx].freq < 4000)
					mode->mode = HOSTAPD_MODE_IEEE80211B;
				else
					mode->mode = HOSTAPD_MODE_IEEE80211A;
				mode_is_set = 1;
			}

			/* crude heuristic */
			if (mode->channels[idx].freq < 4000)
				if (mode->channels[idx].freq == 2848)
					mode->channels[idx].chan = 14;
				else
					mode->channels[idx].chan = (mode->channels[idx].freq - 2407) / 5;
			else
				mode->channels[idx].chan = mode->channels[idx].freq/5 - 1000;

			if (tb_freq[NL80211_FREQUENCY_ATTR_DISABLED])
				mode->channels[idx].flag &= ~HOSTAPD_CHAN_W_SCAN;
			if (tb_freq[NL80211_FREQUENCY_ATTR_PASSIVE_SCAN])
				mode->channels[idx].flag &= ~HOSTAPD_CHAN_W_ACTIVE_SCAN;
			if (tb_freq[NL80211_FREQUENCY_ATTR_NO_IBSS])
				mode->channels[idx].flag &= ~HOSTAPD_CHAN_W_IBSS;
			idx++;
		}

		nla_for_each_nested(nl_rate, tb_band[NL80211_BAND_ATTR_RATES], rem_rate) {
			nla_parse(tb_rate, NL80211_BITRATE_ATTR_MAX, nla_data(nl_rate),
				  nla_len(nl_rate), rate_policy);
			if (!tb_rate[NL80211_BITRATE_ATTR_RATE])
				continue;
			mode->num_rates++;
		}

		mode->rates = calloc(mode->num_rates, sizeof(struct hostapd_rate_data));
		if (!mode->rates)
			return NL_SKIP;

		idx = 0;

		nla_for_each_nested(nl_rate, tb_band[NL80211_BAND_ATTR_RATES], rem_rate) {
			nla_parse(tb_rate, NL80211_BITRATE_ATTR_MAX, nla_data(nl_rate),
				  nla_len(nl_rate), rate_policy);
			if (!tb_rate[NL80211_BITRATE_ATTR_RATE])
				continue;
			mode->rates[idx].rate = nla_get_u32(tb_rate[NL80211_BITRATE_ATTR_RATE]);

			/* crude heuristic */
			if (mode->mode == HOSTAPD_MODE_IEEE80211B &&
			    mode->rates[idx].rate > 200)
				mode->mode = HOSTAPD_MODE_IEEE80211G;

			if (tb_rate[NL80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE])
				mode->rates[idx].flags |= HOSTAPD_RATE_PREAMBLE2;

			idx++;
		}
	}

	phy_info->error = 0;

	return NL_SKIP;
}

static struct hostapd_hw_modes *i802_get_hw_feature_data(void *priv,
							 u16 *num_modes,
							 u16 *flags)
{
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg;
	int err = -1;
	struct nl_cb *cb = NULL;
	int finished = 0;
	struct phy_info_arg result = {
		.num_modes = num_modes,
		.modes = NULL,
		.error = 1,
	};

	*num_modes = 0;
	*flags = 0;

	msg = nlmsg_alloc();
	if (!msg)
		return NULL;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_GET_WIPHY, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(drv->iface));

	cb = nl_cb_clone(drv->nl_cb);
	if (!cb)
		goto out;

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0)
		goto out;

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, phy_info_handler, &result);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_wait_handler, &finished);

	err = nl_recvmsgs(drv->nl_handle, cb);

	if (!finished)
		err = nl_wait_for_ack(drv->nl_handle);

	if (err < 0 || result.error) {
		hostapd_free_hw_features(result.modes, *num_modes);
		result.modes = NULL;
	}

 out:
	nl_cb_put(cb);
 nla_put_failure:
	if (err)
		fprintf(stderr, "failed to get information: %d\n", err);
	nlmsg_free(msg);
	return result.modes;
}


static int i802_set_sta_vlan(void *priv, const u8 *addr,
			     const char *ifname, int vlan_id)
{
	struct i802_driver_data *drv = priv;
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_SET_STATION, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(drv->iface));
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(ifname));

	ret = 0;

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    (errno = nl_wait_for_ack(drv->nl_handle) < 0)) {
		ret = -1;
	}

 nla_put_failure:
	nlmsg_free(msg);

 out:
	return ret;
}


static void handle_unknown_sta(struct hostapd_data *hapd, u8 *ta)
{
	struct sta_info *sta;

	sta = ap_get_sta(hapd, ta);
	if (!sta || !(sta->flags & WLAN_STA_ASSOC)) {
		printf("Data/PS-poll frame from not associated STA "
		       MACSTR "\n", MAC2STR(ta));
		if (sta && (sta->flags & WLAN_STA_AUTH))
			hostapd_sta_disassoc(
				hapd, ta,
				WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
		else
			hostapd_sta_deauth(
				hapd, ta,
				WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
	}
}


static void handle_tx_callback(struct hostapd_data *hapd, u8 *buf, size_t len,
			       int ok)
{
	struct ieee80211_hdr *hdr;
	u16 fc, type, stype;
	struct sta_info *sta;

	hdr = (struct ieee80211_hdr *) buf;
	fc = le_to_host16(hdr->frame_control);

	type = WLAN_FC_GET_TYPE(fc);
	stype = WLAN_FC_GET_STYPE(fc);

	switch (type) {
	case WLAN_FC_TYPE_MGMT:
		wpa_printf(MSG_DEBUG, "MGMT (TX callback) %s",
			   ok ? "ACK" : "fail");
		ieee802_11_mgmt_cb(hapd, buf, len, stype, ok);
		break;
	case WLAN_FC_TYPE_CTRL:
		wpa_printf(MSG_DEBUG, "CTRL (TX callback) %s",
			   ok ? "ACK" : "fail");
		break;
	case WLAN_FC_TYPE_DATA:
		wpa_printf(MSG_DEBUG, "DATA (TX callback) %s",
			   ok ? "ACK" : "fail");
		sta = ap_get_sta(hapd, hdr->addr1);
		if (sta && sta->flags & WLAN_STA_PENDING_POLL) {
			wpa_printf(MSG_DEBUG, "STA " MACSTR " %s pending "
				   "activity poll", MAC2STR(sta->addr),
				   ok ? "ACKed" : "did not ACK");
			if (ok)
				sta->flags &= ~WLAN_STA_PENDING_POLL;
		}
		if (sta)
			ieee802_1x_tx_status(hapd, sta, buf, len, ok);
		break;
	default:
		printf("unknown TX callback frame type %d\n", type);
		break;
	}
}


static void handle_frame(struct hostapd_iface *iface, u8 *buf, size_t len,
			 struct hostapd_frame_info *hfi,
			 enum ieee80211_msg_type msg_type)
{
	struct ieee80211_hdr *hdr;
	u16 fc, type, stype;
	size_t data_len = len;
	struct hostapd_data *hapd = NULL;
	int broadcast_bssid = 0;
	size_t i;
	u8 *bssid;

	/*
	 * PS-Poll frames are 16 bytes. All other frames are
	 * 24 bytes or longer.
	 */
	if (len < 16)
		return;

	hdr = (struct ieee80211_hdr *) buf;
	fc = le_to_host16(hdr->frame_control);

	type = WLAN_FC_GET_TYPE(fc);
	stype = WLAN_FC_GET_STYPE(fc);

	switch (type) {
	case WLAN_FC_TYPE_DATA:
		if (len < 24)
			return;
		switch (fc & (WLAN_FC_FROMDS | WLAN_FC_TODS)) {
		case WLAN_FC_TODS:
			bssid = hdr->addr1;
			break;
		default:
			/* discard */
			return;
		}
		break;
	case WLAN_FC_TYPE_CTRL:
		/* discard non-ps-poll frames */
		if (stype != WLAN_FC_STYPE_PSPOLL)
			return;
		bssid = hdr->addr1;
		break;
	case WLAN_FC_TYPE_MGMT:
		bssid = hdr->addr3;
		break;
	default:
		/* discard */
		return;
	}

	/* find interface frame belongs to */
	for (i = 0; i < iface->num_bss; i++) {
		if (memcmp(bssid, iface->bss[i]->own_addr, ETH_ALEN) == 0) {
			hapd = iface->bss[i];
			break;
		}
	}

	if (hapd == NULL) {
		hapd = iface->bss[0];

		if (bssid[0] != 0xff || bssid[1] != 0xff ||
		    bssid[2] != 0xff || bssid[3] != 0xff ||
		    bssid[4] != 0xff || bssid[5] != 0xff) {
			/*
			 * Unknown BSSID - drop frame if this is not from
			 * passive scanning or a beacon (at least ProbeReq
			 * frames to other APs may be allowed through RX
			 * filtering in the wlan hw/driver)
			 */
			if ((type != WLAN_FC_TYPE_MGMT ||
			     stype != WLAN_FC_STYPE_BEACON))
				return;
		} else
			broadcast_bssid = 1;
	}

	switch (msg_type) {
	case ieee80211_msg_normal:
		/* continue processing */
		break;
	case ieee80211_msg_tx_callback_ack:
		handle_tx_callback(hapd, buf, data_len, 1);
		return;
	case ieee80211_msg_tx_callback_fail:
		handle_tx_callback(hapd, buf, data_len, 0);
		return;
	}

	switch (type) {
	case WLAN_FC_TYPE_MGMT:
		if (stype != WLAN_FC_STYPE_BEACON &&
		    stype != WLAN_FC_STYPE_PROBE_REQ)
			wpa_printf(MSG_MSGDUMP, "MGMT");
		if (broadcast_bssid) {
			for (i = 0; i < iface->num_bss; i++)
				ieee802_11_mgmt(iface->bss[i], buf, data_len,
						stype, hfi);
		} else
			ieee802_11_mgmt(hapd, buf, data_len, stype, hfi);
		break;
	case WLAN_FC_TYPE_CTRL:
		/* can only get here with PS-Poll frames */
		wpa_printf(MSG_DEBUG, "CTRL");
		handle_unknown_sta(hapd, hdr->addr2);
		break;
	case WLAN_FC_TYPE_DATA:
		wpa_printf(MSG_DEBUG, "DATA");
		handle_unknown_sta(hapd, hdr->addr2);
		break;
	}
}


static void handle_eapol(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct i802_driver_data *drv = eloop_ctx;
	struct hostapd_data *hapd = drv->hapd;
	struct sockaddr_ll lladdr;
	unsigned char buf[3000];
	int len;
	socklen_t fromlen = sizeof(lladdr);

	len = recvfrom(sock, buf, sizeof(buf), 0,
		       (struct sockaddr *)&lladdr, &fromlen);
	if (len < 0) {
		perror("recv");
		return;
	}

	if (have_ifidx(drv, lladdr.sll_ifindex))
		ieee802_1x_receive(hapd, lladdr.sll_addr, buf, len);
}


static void handle_monitor_read(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct i802_driver_data *drv = eloop_ctx;
	int len;
	unsigned char buf[3000];
	struct hostapd_data *hapd = drv->hapd;
	struct ieee80211_radiotap_iterator iter;
	int ret;
	struct hostapd_frame_info hfi;
	int injected = 0, failed = 0, msg_type, rxflags = 0;

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0) {
		perror("recv");
		return;
	}

	if (ieee80211_radiotap_iterator_init(&iter, (void*)buf, len)) {
		printf("received invalid radiotap frame\n");
		return;
	}

	memset(&hfi, 0, sizeof(hfi));

	while (1) {
		ret = ieee80211_radiotap_iterator_next(&iter);
		if (ret == -ENOENT)
			break;
		if (ret) {
			printf("received invalid radiotap frame (%d)\n", ret);
			return;
		}
		switch (iter.this_arg_index) {
		case IEEE80211_RADIOTAP_FLAGS:
			if (*iter.this_arg & IEEE80211_RADIOTAP_F_FCS)
				len -= 4;
			break;
		case IEEE80211_RADIOTAP_RX_FLAGS:
			rxflags = 1;
			break;
		case IEEE80211_RADIOTAP_TX_FLAGS:
			injected = 1;
			failed = le_to_host16((*(uint16_t *) iter.this_arg)) &
					IEEE80211_RADIOTAP_F_TX_FAIL;
			break;
		case IEEE80211_RADIOTAP_DATA_RETRIES:
			break;
		case IEEE80211_RADIOTAP_CHANNEL:
			/* TODO convert from freq/flags to channel number
			hfi.channel = XXX;
			hfi.phytype = XXX;
			 */
			break;
		case IEEE80211_RADIOTAP_RATE:
			hfi.datarate = *iter.this_arg * 5;
			break;
		case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
			hfi.ssi_signal = *iter.this_arg;
			break;
		}
	}

	if (rxflags && injected)
		return;

	if (!injected)
		msg_type = ieee80211_msg_normal;
	else if (failed)
		msg_type = ieee80211_msg_tx_callback_fail;
	else
		msg_type = ieee80211_msg_tx_callback_ack;

	handle_frame(hapd->iface, buf + iter.max_length,
		     len - iter.max_length, &hfi, msg_type);
}


static int nl80211_create_monitor_interface(struct i802_driver_data *drv)
{
	char buf[IFNAMSIZ];
	struct sockaddr_ll ll;
	int optval;
	socklen_t optlen;

	snprintf(buf, IFNAMSIZ, "mon.%s", drv->iface);
	buf[IFNAMSIZ - 1] = '\0';

	drv->monitor_ifidx =
		nl80211_create_iface(drv, buf, NL80211_IFTYPE_MONITOR, NULL);

	if (drv->monitor_ifidx < 0)
		return -1;

	if (hostapd_set_iface_flags(drv, buf, 1))
		goto error;

	memset(&ll, 0, sizeof(ll));
	ll.sll_family = AF_PACKET;
	ll.sll_ifindex = drv->monitor_ifidx;
	drv->monitor_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (drv->monitor_sock < 0) {
		perror("socket[PF_PACKET,SOCK_RAW]");
		goto error;
	}

	if (bind(drv->monitor_sock, (struct sockaddr *) &ll,
		 sizeof(ll)) < 0) {
		perror("monitor socket bind");
		goto error;
	}

	optlen = sizeof(optval);
	optval = 20;
	if (setsockopt
	    (drv->monitor_sock, SOL_SOCKET, SO_PRIORITY, &optval, optlen)) {
		perror("Failed to set socket priority");
		goto error;
	}

	if (eloop_register_read_sock(drv->monitor_sock, handle_monitor_read,
				     drv, NULL)) {
		printf("Could not register monitor read socket\n");
		goto error;
	}

	return 0;
 error:
	nl80211_remove_iface(drv, drv->monitor_ifidx);
	return -1;
}


static int nl80211_set_master_mode(struct i802_driver_data *drv,
				   const char *ifname)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_SET_INTERFACE, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(ifname));
	NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_AP);

	if (nl_send_auto_complete(drv->nl_handle, msg) < 0 ||
	    nl_wait_for_ack(drv->nl_handle) < 0) {
 nla_put_failure:
		wpa_printf(MSG_ERROR, "Failed to set interface %s to master "
			   "mode.", ifname);
		nlmsg_free(msg);
		return -1;
	}

	nlmsg_free(msg);

	return 0;
}


static int i802_init_sockets(struct i802_driver_data *drv, const u8 *bssid)
{
	struct ifreq ifr;
	struct sockaddr_ll addr;

	drv->ioctl_sock = -1;

	drv->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->ioctl_sock < 0) {
		perror("socket[PF_INET,SOCK_DGRAM]");
		return -1;
	}

	/* start listening for EAPOL on the default AP interface */
	add_ifidx(drv, if_nametoindex(drv->iface));

	if (hostapd_set_iface_flags(drv, drv->iface, 0))
		return -1;

	if (bssid) {
		os_strlcpy(ifr.ifr_name, drv->iface, IFNAMSIZ);
		memcpy(ifr.ifr_hwaddr.sa_data, bssid, ETH_ALEN);
		ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;

		if (ioctl(drv->ioctl_sock, SIOCSIFHWADDR, &ifr)) {
			perror("ioctl(SIOCSIFHWADDR)");
			return -1;
		}
	}

	/*
	 * initialise generic netlink and nl80211
	 */
	drv->nl_cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!drv->nl_cb) {
		printf("Failed to allocate netlink callbacks.\n");
		return -1;
	}

	drv->nl_handle = nl_handle_alloc_cb(drv->nl_cb);
	if (!drv->nl_handle) {
		printf("Failed to allocate netlink handle.\n");
		return -1;
	}

	if (genl_connect(drv->nl_handle)) {
		printf("Failed to connect to generic netlink.\n");
		return -1;
	}

	drv->nl_cache = genl_ctrl_alloc_cache(drv->nl_handle);
	if (!drv->nl_cache) {
		printf("Failed to allocate generic netlink cache.\n");
		return -1;
	}

	drv->nl80211 = genl_ctrl_search_by_name(drv->nl_cache, "nl80211");
	if (!drv->nl80211) {
		printf("nl80211 not found.\n");
		return -1;
	}

	/* Initialise a monitor interface */
	if (nl80211_create_monitor_interface(drv))
		return -1;

	if (nl80211_set_master_mode(drv, drv->iface))
		return -1;

	if (hostapd_set_iface_flags(drv, drv->iface, 1))
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = ifr.ifr_ifindex;
	wpa_printf(MSG_DEBUG, "Opening raw packet socket for ifindex %d",
		   addr.sll_ifindex);

	drv->eapol_sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_PAE));
	if (drv->eapol_sock < 0) {
		perror("socket(PF_PACKET, SOCK_DGRAM, ETH_P_PAE)");
		return -1;
	}

	if (eloop_register_read_sock(drv->eapol_sock, handle_eapol, drv, NULL))
	{
		printf("Could not register read socket for eapol\n");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->iface, sizeof(ifr.ifr_name));
	if (ioctl(drv->ioctl_sock, SIOCGIFHWADDR, &ifr) != 0) {
		perror("ioctl(SIOCGIFHWADDR)");
		return -1;
	}

	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		printf("Invalid HW-addr family 0x%04x\n",
		       ifr.ifr_hwaddr.sa_family);
		return -1;
	}
	memcpy(drv->hapd->own_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	return 0;
}


static int i802_get_inact_sec(void *priv, const u8 *addr)
{
	struct hostap_sta_driver_data data;
	int ret;

	data.inactive_msec = (unsigned long) -1;
	ret = i802_read_sta_data(priv, &data, addr);
	if (ret || data.inactive_msec == (unsigned long) -1)
		return -1;
	return data.inactive_msec / 1000;
}


static int i802_sta_clear_stats(void *priv, const u8 *addr)
{
#if 0
	/* TODO */
#endif
	return 0;
}


static void
hostapd_wireless_event_wireless_custom(struct i802_driver_data *drv,
				       char *custom)
{
	wpa_printf(MSG_DEBUG, "Custom wireless event: '%s'", custom);

	if (strncmp(custom, "MLME-MICHAELMICFAILURE.indication", 33) == 0) {
		char *pos;
		u8 addr[ETH_ALEN];
		pos = strstr(custom, "addr=");
		if (pos == NULL) {
			wpa_printf(MSG_DEBUG,
				   "MLME-MICHAELMICFAILURE.indication "
				   "without sender address ignored");
			return;
		}
		pos += 5;
		if (hwaddr_aton(pos, addr) == 0) {
			ieee80211_michael_mic_failure(drv->hapd, addr, 1);
		} else {
			wpa_printf(MSG_DEBUG,
				   "MLME-MICHAELMICFAILURE.indication "
				   "with invalid MAC address");
		}
	}
}


static void hostapd_wireless_event_wireless(struct i802_driver_data *drv,
					    char *data, int len)
{
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	char *pos, *end, *custom, *buf;

	pos = data;
	end = data + len;

	while (pos + IW_EV_LCP_LEN <= end) {
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		wpa_printf(MSG_DEBUG, "Wireless event: cmd=0x%x len=%d",
			   iwe->cmd, iwe->len);
		if (iwe->len <= IW_EV_LCP_LEN)
			return;

		custom = pos + IW_EV_POINT_LEN;
		if (drv->we_version > 18 &&
		    (iwe->cmd == IWEVMICHAELMICFAILURE ||
		     iwe->cmd == IWEVCUSTOM)) {
			/* WE-19 removed the pointer from struct iw_point */
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			memcpy(dpos, pos + IW_EV_LCP_LEN,
			       sizeof(struct iw_event) - dlen);
		} else {
			memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		switch (iwe->cmd) {
		case IWEVCUSTOM:
			if (custom + iwe->u.data.length > end)
				return;
			buf = malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;
			memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';
			hostapd_wireless_event_wireless_custom(drv, buf);
			free(buf);
			break;
		}

		pos += iwe->len;
	}
}


static void hostapd_wireless_event_rtm_newlink(struct i802_driver_data *drv,
					       struct nlmsghdr *h, int len)
{
	struct ifinfomsg *ifi;
	int attrlen, nlmsg_len, rta_len;
	struct rtattr *attr;

	if (len < (int) sizeof(*ifi))
		return;

	ifi = NLMSG_DATA(h);

	/* TODO: use ifi->ifi_index to filter out wireless events from other
	 * interfaces */

	nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	attrlen = h->nlmsg_len - nlmsg_len;
	if (attrlen < 0)
		return;

	attr = (struct rtattr *) (((char *) ifi) + nlmsg_len);

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_WIRELESS) {
			hostapd_wireless_event_wireless(
				drv, ((char *) attr) + rta_len,
				attr->rta_len - rta_len);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static void hostapd_wireless_event_receive(int sock, void *eloop_ctx,
					   void *sock_ctx)
{
	char buf[256];
	int left;
	struct sockaddr_nl from;
	socklen_t fromlen;
	struct nlmsghdr *h;
	struct i802_driver_data *drv = eloop_ctx;

	fromlen = sizeof(from);
	left = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
			(struct sockaddr *) &from, &fromlen);
	if (left < 0) {
		if (errno != EINTR && errno != EAGAIN)
			perror("recvfrom(netlink)");
		return;
	}

	h = (struct nlmsghdr *) buf;
	while (left >= (int) sizeof(*h)) {
		int len, plen;

		len = h->nlmsg_len;
		plen = len - sizeof(*h);
		if (len > left || plen < 0) {
			printf("Malformed netlink message: "
			       "len=%d left=%d plen=%d\n",
			       len, left, plen);
			break;
		}

		switch (h->nlmsg_type) {
		case RTM_NEWLINK:
			hostapd_wireless_event_rtm_newlink(drv, h, plen);
			break;
		}

		len = NLMSG_ALIGN(len);
		left -= len;
		h = (struct nlmsghdr *) ((char *) h + len);
	}

	if (left > 0) {
		printf("%d extra bytes in the end of netlink message\n", left);
	}
}


static int hostap_get_we_version(struct i802_driver_data *drv)
{
	struct iw_range *range;
	struct iwreq iwr;
	int minlen;
	size_t buflen;

	drv->we_version = 0;

	/*
	 * Use larger buffer than struct iw_range in order to allow the
	 * structure to grow in the future.
	 */
	buflen = sizeof(struct iw_range) + 500;
	range = os_zalloc(buflen);
	if (range == NULL)
		return -1;

	memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) range;
	iwr.u.data.length = buflen;

	minlen = ((char *) &range->enc_capa) - (char *) range +
		sizeof(range->enc_capa);

	if (ioctl(drv->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
		perror("ioctl[SIOCGIWRANGE]");
		free(range);
		return -1;
	} else if (iwr.u.data.length >= minlen &&
		   range->we_version_compiled >= 18) {
		wpa_printf(MSG_DEBUG, "SIOCGIWRANGE: WE(compiled)=%d "
			   "WE(source)=%d enc_capa=0x%x",
			   range->we_version_compiled,
			   range->we_version_source,
			   range->enc_capa);
		drv->we_version = range->we_version_compiled;
	}

	free(range);
	return 0;
}


static int i802_wireless_event_init(void *priv)
{
	struct i802_driver_data *drv = priv;
	int s;
	struct sockaddr_nl local;

	hostap_get_we_version(drv);

	drv->wext_sock = -1;

	s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (s < 0) {
		perror("socket(PF_NETLINK,SOCK_RAW,NETLINK_ROUTE)");
		return -1;
	}

	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = RTMGRP_LINK;
	if (bind(s, (struct sockaddr *) &local, sizeof(local)) < 0) {
		perror("bind(netlink)");
		close(s);
		return -1;
	}

	eloop_register_read_sock(s, hostapd_wireless_event_receive, drv,
				 NULL);
	drv->wext_sock = s;

	return 0;
}


static void i802_wireless_event_deinit(void *priv)
{
	struct i802_driver_data *drv = priv;
	if (drv->wext_sock < 0)
		return;
	eloop_unregister_read_sock(drv->wext_sock);
	close(drv->wext_sock);
}


static int i802_sta_deauth(void *priv, const u8 *addr, int reason)
{
	struct i802_driver_data *drv = priv;
	struct ieee80211_mgmt mgmt;

	memset(&mgmt, 0, sizeof(mgmt));
	mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					  WLAN_FC_STYPE_DEAUTH);
	memcpy(mgmt.da, addr, ETH_ALEN);
	memcpy(mgmt.sa, drv->hapd->own_addr, ETH_ALEN);
	memcpy(mgmt.bssid, drv->hapd->own_addr, ETH_ALEN);
	mgmt.u.deauth.reason_code = host_to_le16(reason);
	return i802_send_mgmt_frame(drv, &mgmt, IEEE80211_HDRLEN +
				      sizeof(mgmt.u.deauth), 0);
}


static int i802_sta_disassoc(void *priv, const u8 *addr, int reason)
{
	struct i802_driver_data *drv = priv;
	struct ieee80211_mgmt mgmt;

	memset(&mgmt, 0, sizeof(mgmt));
	mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					  WLAN_FC_STYPE_DISASSOC);
	memcpy(mgmt.da, addr, ETH_ALEN);
	memcpy(mgmt.sa, drv->hapd->own_addr, ETH_ALEN);
	memcpy(mgmt.bssid, drv->hapd->own_addr, ETH_ALEN);
	mgmt.u.disassoc.reason_code = host_to_le16(reason);
	return  i802_send_mgmt_frame(drv, &mgmt, IEEE80211_HDRLEN +
				       sizeof(mgmt.u.disassoc), 0);
}


static void *i802_init_bssid(struct hostapd_data *hapd, const u8 *bssid)
{
	struct i802_driver_data *drv;

	drv = os_zalloc(sizeof(struct i802_driver_data));
	if (drv == NULL) {
		printf("Could not allocate memory for i802 driver data\n");
		return NULL;
	}

	drv->hapd = hapd;
	memcpy(drv->iface, hapd->conf->iface, sizeof(drv->iface));

	drv->num_if_indices = sizeof(drv->default_if_indices) / sizeof(int);
	drv->if_indices = drv->default_if_indices;
	drv->bridge = if_nametoindex(hapd->conf->bridge);

	if (i802_init_sockets(drv, bssid))
		goto failed;

	return drv;

failed:
	free(drv);
	return NULL;
}


static void *i802_init(struct hostapd_data *hapd)
{
	return i802_init_bssid(hapd, NULL);
}


static void i802_deinit(void *priv)
{
	struct i802_driver_data *drv = priv;

	i802_del_beacon(drv);

	/* remove monitor interface */
	nl80211_remove_iface(drv, drv->monitor_ifidx);

	(void) hostapd_set_iface_flags(drv, drv->iface, 0);

	if (drv->monitor_sock >= 0) {
		eloop_unregister_read_sock(drv->monitor_sock);
		close(drv->monitor_sock);
	}
	if (drv->ioctl_sock >= 0)
		close(drv->ioctl_sock);
	if (drv->eapol_sock >= 0) {
		eloop_unregister_read_sock(drv->eapol_sock);
		close(drv->eapol_sock);
	}

	genl_family_put(drv->nl80211);
	nl_cache_free(drv->nl_cache);
	nl_handle_destroy(drv->nl_handle);
	nl_cb_put(drv->nl_cb);

	if (drv->if_indices != drv->default_if_indices)
		free(drv->if_indices);

	free(drv);
}


const struct wpa_driver_ops wpa_driver_nl80211_ops = {
	.name = "nl80211",
	.init = i802_init,
	.init_bssid = i802_init_bssid,
	.deinit = i802_deinit,
	.wireless_event_init = i802_wireless_event_init,
	.wireless_event_deinit = i802_wireless_event_deinit,
	.set_ieee8021x = i802_set_ieee8021x,
	.set_privacy = i802_set_privacy,
	.set_encryption = i802_set_encryption,
	.get_seqnum = i802_get_seqnum,
	.flush = i802_flush,
	.read_sta_data = i802_read_sta_data,
	.send_eapol = i802_send_eapol,
	.sta_set_flags = i802_sta_set_flags,
	.sta_deauth = i802_sta_deauth,
	.sta_disassoc = i802_sta_disassoc,
	.sta_remove = i802_sta_remove,
	.set_ssid = i802_set_ssid,
	.send_mgmt_frame = i802_send_mgmt_frame,
	.sta_add = i802_sta_add,
	.get_inact_sec = i802_get_inact_sec,
	.sta_clear_stats = i802_sta_clear_stats,
	.set_freq = i802_set_freq,
	.set_rts = i802_set_rts,
	.get_rts = i802_get_rts,
	.set_frag = i802_set_frag,
	.get_frag = i802_get_frag,
	.set_retry = i802_set_retry,
	.get_retry = i802_get_retry,
	.set_rate_sets = i802_set_rate_sets,
	.set_channel_flag = i802_set_channel_flag,
	.set_regulatory_domain = i802_set_regulatory_domain,
	.set_beacon = i802_set_beacon,
	.set_internal_bridge = i802_set_internal_bridge,
	.set_beacon_int = i802_set_beacon_int,
	.set_dtim_period = i802_set_dtim_period,
	.set_cts_protect = i802_set_cts_protect,
	.set_preamble = i802_set_preamble,
	.set_short_slot_time = i802_set_short_slot_time,
	.set_tx_queue_params = i802_set_tx_queue_params,
	.bss_add = i802_bss_add,
	.bss_remove = i802_bss_remove,
	.if_add = i802_if_add,
	.if_update = i802_if_update,
	.if_remove = i802_if_remove,
	.get_hw_feature_data = i802_get_hw_feature_data,
	.set_sta_vlan = i802_set_sta_vlan,
};
