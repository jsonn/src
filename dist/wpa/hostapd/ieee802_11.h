/*
 * hostapd / IEEE 802.11 Management
 * Copyright (c) 2002-2006, Jouni Malinen <j@w1.fi>
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

#ifndef IEEE802_11_H
#define IEEE802_11_H

#include "ieee802_11_defs.h"

/* Parsed Information Elements */
struct ieee802_11_elems {
	u8 *ssid;
	u8 ssid_len;
	u8 *supp_rates;
	u8 supp_rates_len;
	u8 *fh_params;
	u8 fh_params_len;
	u8 *ds_params;
	u8 ds_params_len;
	u8 *cf_params;
	u8 cf_params_len;
	u8 *tim;
	u8 tim_len;
	u8 *ibss_params;
	u8 ibss_params_len;
	u8 *challenge;
	u8 challenge_len;
	u8 *erp_info;
	u8 erp_info_len;
	u8 *ext_supp_rates;
	u8 ext_supp_rates_len;
	u8 *wpa_ie;
	u8 wpa_ie_len;
	u8 *rsn_ie;
	u8 rsn_ie_len;
	u8 *wme;
	u8 wme_len;
	u8 *wme_tspec;
	u8 wme_tspec_len;
	u8 *power_cap;
	u8 power_cap_len;
	u8 *supp_channels;
	u8 supp_channels_len;
	u8 *mdie;
	u8 mdie_len;
	u8 *ftie;
	u8 ftie_len;
};

typedef enum { ParseOK = 0, ParseUnknown = 1, ParseFailed = -1 } ParseRes;


struct hostapd_frame_info {
	u32 phytype;
	u32 channel;
	u32 datarate;
	u32 ssi_signal;

	unsigned int passive_scan:1;
};

struct hostapd_data;
struct sta_info;

void ieee802_11_send_deauth(struct hostapd_data *hapd, u8 *addr, u16 reason);
void ieee802_11_mgmt(struct hostapd_data *hapd, u8 *buf, size_t len,
		     u16 stype, struct hostapd_frame_info *fi);
void ieee802_11_mgmt_cb(struct hostapd_data *hapd, u8 *buf, size_t len,
			u16 stype, int ok);
ParseRes ieee802_11_parse_elems(struct hostapd_data *hapd, u8 *start,
				size_t len,
				struct ieee802_11_elems *elems,
				int show_errors);
void ieee802_11_print_ssid(char *buf, const u8 *ssid, u8 len);
void ieee80211_michael_mic_failure(struct hostapd_data *hapd, const u8 *addr,
				   int local);
int ieee802_11_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen);
int ieee802_11_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   char *buf, size_t buflen);
u16 hostapd_own_capab_info(struct hostapd_data *hapd, struct sta_info *sta,
			   int probe);
u8 * hostapd_eid_supp_rates(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ext_supp_rates(struct hostapd_data *hapd, u8 *eid);

#endif /* IEEE802_11_H */
