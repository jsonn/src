/*	$NetBSD: ieee80211_node.h,v 1.13.2.5 2004/09/21 13:36:55 skrll Exp $	*/
/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/net80211/ieee80211_node.h,v 1.10 2004/04/05 22:10:26 sam Exp $
 */
#ifndef _NET80211_IEEE80211_NODE_H_
#define _NET80211_IEEE80211_NODE_H_

#ifdef _KERNEL
#define	IEEE80211_PSCAN_WAIT 	5		/* passive scan wait */
#define	IEEE80211_TRANS_WAIT 	5		/* transition wait */
#define	IEEE80211_INACT_WAIT	5		/* inactivity timer interval */
#define	IEEE80211_INACT_MAX	(300/IEEE80211_INACT_WAIT)
#define	IEEE80211_CACHE_SIZE	100

#define	IEEE80211_NODE_HASHSIZE	32
/* simple hash is enough for variation of macaddr */
#define	IEEE80211_NODE_HASH(addr)	\
	(((u_int8_t *)(addr))[IEEE80211_ADDR_LEN - 1] % IEEE80211_NODE_HASHSIZE)
#endif /* _KERNEL */

#define	IEEE80211_RATE_SIZE	8		/* 802.11 standard */
#define	IEEE80211_RATE_MAXSIZE	15		/* max rates we'll handle */

struct ieee80211_rateset {
	u_int8_t		rs_nrates;
	u_int8_t		rs_rates[IEEE80211_RATE_MAXSIZE];
};

enum ieee80211_node_state {
	IEEE80211_STA_CACHE,	/* cached node */
	IEEE80211_STA_BSS,	/* ic->ic_bss, the network we joined */
	IEEE80211_STA_AUTH,	/* successfully authenticated */
	IEEE80211_STA_ASSOC,	/* successfully associated */
	IEEE80211_STA_COLLECT	/* This node remains in the cache while
				 * the driver sends a de-auth message;
				 * afterward it should be freed to make room
				 * for a new node.
				 */
};

#define	ieee80211_node_newstate(__ni, __state)	\
	do {					\
		(__ni)->ni_state = (__state);	\
	} while (0)

#ifdef _KERNEL
/*
 * Node specific information.  Note that drivers are expected
 * to derive from this structure to add device-specific per-node
 * state.  This is done by overriding the ic_node_* methods in
 * the ieee80211com structure.
 */
struct ieee80211_node {
	TAILQ_ENTRY(ieee80211_node)	ni_list;
	LIST_ENTRY(ieee80211_node)	ni_hash;
	u_int			ni_refcnt;
	u_int			ni_scangen;	/* gen# for timeout scan */

	/* hardware */
	u_int32_t		ni_rstamp;	/* recv timestamp */
	u_int8_t		ni_rssi;	/* recv ssi */

	/* header */
	u_int8_t		ni_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t		ni_bssid[IEEE80211_ADDR_LEN];

	/* beacon, probe response */
	u_int8_t		ni_tstamp[8];	/* from last rcv'd beacon */
	u_int16_t		ni_intval;	/* beacon interval */
	u_int16_t		ni_capinfo;	/* capabilities */
	u_int8_t		ni_esslen;
	u_int8_t		ni_essid[IEEE80211_NWID_LEN];
	struct ieee80211_rateset ni_rates;	/* negotiated rate set */
	u_int8_t		*ni_country;	/* country information XXX */
	struct ieee80211_channel *ni_chan;
	u_int16_t		ni_fhdwell;	/* FH only */
	u_int8_t		ni_fhindex;	/* FH only */
	u_int8_t		ni_erp;		/* 11g only */

#ifdef notyet
	/* DTIM and contention free period (CFP) */
	u_int8_t		ni_dtimperiod;
	u_int8_t		ni_cfpperiod;	/* # of DTIMs between CFPs */
	u_int16_t		ni_cfpduremain;	/* remaining cfp duration */
	u_int16_t		ni_cfpmaxduration;/* max CFP duration in TU */
	u_int16_t		ni_nextdtim;	/* time to next DTIM */
	u_int16_t		ni_timoffset;
#endif

	/* power saving mode */

	u_int8_t		ni_pwrsave;
	struct ifqueue		ni_savedq;	/* packets queued for pspoll */

	/* others */
	u_int16_t		ni_associd;	/* assoc response */
	u_int16_t		ni_txseq;	/* seq to be transmitted */
	u_int16_t		ni_rxseq;	/* seq previous received */
	int			ni_fails;	/* failure count to associate */
	int			ni_inact;	/* inactivity mark count */
	int			ni_txrate;	/* index to ni_rates[] */
	int			ni_state;
	u_int32_t		*ni_challenge;	/* shared-key challenge */
};

#ifdef __NetBSD__
#define ieee80211_node_incref(ni)			\
	do {						\
		int _s = splnet();			\
		(ni)->ni_refcnt++;			\
		splx(_s);				\
	} while (0)

static __inline int
ieee80211_node_decref(struct ieee80211_node *ni)
{
	int refcnt, s;
	s = splnet();
	refcnt = --ni->ni_refcnt;
	splx(s);
	return refcnt;
}

#else
#define ieee80211_node_incref(ni) atomic_add_int(&(ni)->ni_refcnt, 1)
static __inline int
ieee80211_node_decref(struct ieee80211_node *ni)
{
	int orefcnt;
	do {
		orefcnt = ni->ni_refcnt;
	} while (atomic_cmpset_int(&ni->ni_refcnt, orefcnt, orefcnt - 1) == 0);
	return orefcnt - 1;
}
#endif

static __inline struct ieee80211_node *
ieee80211_ref_node(struct ieee80211_node *ni)
{
	ieee80211_node_incref(ni);
	return ni;
}

static __inline void
ieee80211_unref_node(struct ieee80211_node **ni)
{
	ieee80211_node_decref(*ni);
	*ni = NULL;			/* guard against use */
}

struct ieee80211com;

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_80211_NODE);
#endif

extern	void ieee80211_node_attach(struct ieee80211com *);
extern	void ieee80211_node_lateattach(struct ieee80211com *);
extern	void ieee80211_node_detach(struct ieee80211com *);

extern	void ieee80211_begin_scan(struct ieee80211com *);
extern	void ieee80211_next_scan(struct ieee80211com *);
extern	void ieee80211_create_ibss(struct ieee80211com *,
		struct ieee80211_channel *);
extern	void ieee80211_end_scan(struct ieee80211com *);
extern	struct ieee80211_node *ieee80211_alloc_node(struct ieee80211com *,
		u_int8_t *);
extern	struct ieee80211_node *ieee80211_dup_bss(struct ieee80211com *,
		u_int8_t *);
extern	struct ieee80211_node *ieee80211_find_node(struct ieee80211com *,
		u_int8_t *);
extern	struct ieee80211_node *ieee80211_find_rxnode(struct ieee80211com *,
		struct ieee80211_frame *);
extern	struct ieee80211_node *ieee80211_find_txnode(struct ieee80211com *,
		u_int8_t *);
extern	struct ieee80211_node *ieee80211_find_node_for_beacon(
		struct ieee80211com *, u_int8_t *macaddr,
		struct ieee80211_channel *, char *ssid);
extern	void ieee80211_release_node(struct ieee80211com *,
		struct ieee80211_node *);
extern	void ieee80211_free_allnodes(struct ieee80211com *);

typedef void ieee80211_iter_func(void *, struct ieee80211_node *);
extern	void ieee80211_iterate_nodes(struct ieee80211com *ic,
		ieee80211_iter_func *, void *);
extern	void ieee80211_clean_nodes(struct ieee80211com *);

extern	void ieee80211_node_join(struct ieee80211com *,
		struct ieee80211_node *, int);
extern	void ieee80211_node_leave(struct ieee80211com *,
		struct ieee80211_node *);

extern	int ieee80211_match_bss(struct ieee80211com *, struct ieee80211_node *);
#endif /* _KERNEL */
#endif /* _NET80211_IEEE80211_NODE_H_ */
