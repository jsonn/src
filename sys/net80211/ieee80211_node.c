/*	$NetBSD: ieee80211_node.c,v 1.10.2.1 2004/08/03 16:54:45 jmc Exp $	*/
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
 */

#include <sys/cdefs.h>
#ifdef __FreeBSD__
__FBSDID("$FreeBSD: src/sys/net80211/ieee80211_node.c,v 1.13 2003/11/09 23:36:46 sam Exp $");
#else
__KERNEL_RCSID(0, "$NetBSD: ieee80211_node.c,v 1.10.2.1 2004/08/03 16:54:45 jmc Exp $");
#endif

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/errno.h>
#ifdef __FreeBSD__
#include <sys/bus.h>
#endif
#include <sys/proc.h>
#include <sys/sysctl.h>

#ifdef __FreeBSD__
#include <machine/atomic.h>
#endif
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#ifdef __FreeBSD__
#include <net/ethernet.h>
#else
#include <net/if_ether.h>
#endif
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_compat.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h> 
#ifdef __FreeBSD__
#include <netinet/if_ether.h>
#else
#include <net/if_ether.h>
#endif
#endif

static struct ieee80211_node *ieee80211_node_alloc(struct ieee80211com *);
static void ieee80211_node_free(struct ieee80211com *, struct ieee80211_node *);
static void ieee80211_node_copy(struct ieee80211com *,
		struct ieee80211_node *, const struct ieee80211_node *);
static u_int8_t ieee80211_node_getrssi(struct ieee80211com *,
		struct ieee80211_node *);

static void ieee80211_setup_node(struct ieee80211com *ic,
		struct ieee80211_node *ni, u_int8_t *macaddr);
static void _ieee80211_free_node(struct ieee80211com *,
		struct ieee80211_node *);

MALLOC_DEFINE(M_80211_NODE, "node", "802.11 node state");

void
ieee80211_node_attach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

#ifdef __FreeBSD__
	/* XXX need unit */
	IEEE80211_NODE_LOCK_INIT(ic, ifp->if_xname);
#endif
	TAILQ_INIT(&ic->ic_node);
	ic->ic_node_alloc = ieee80211_node_alloc;
	ic->ic_node_free = ieee80211_node_free;
	ic->ic_node_copy = ieee80211_node_copy;
	ic->ic_node_getrssi = ieee80211_node_getrssi;
	ic->ic_scangen = 1;
}

void
ieee80211_node_lateattach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	ic->ic_bss = (*ic->ic_node_alloc)(ic);
	IASSERT(ic->ic_bss != NULL, ("unable to setup inital BSS node"));
	ic->ic_bss->ni_chan = IEEE80211_CHAN_ANYC;
}

void
ieee80211_node_detach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	if (ic->ic_bss != NULL)
		(*ic->ic_node_free)(ic, ic->ic_bss);
	ieee80211_free_allnodes(ic);
#ifdef __FreeBSD__
	IEEE80211_NODE_LOCK_DESTROY(ic);
#endif
}

/*
 * AP scanning support.
 */

/*
 * Initialize the active channel set based on the set
 * of available channels and the current PHY mode.
 */
static void
ieee80211_reset_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	memcpy(ic->ic_chan_scan, ic->ic_chan_active,
		sizeof(ic->ic_chan_active));
	/* NB: hack, setup so next_scan starts with the first channel */
	if (ic->ic_bss->ni_chan == IEEE80211_CHAN_ANYC)
		ic->ic_bss->ni_chan = &ic->ic_channels[IEEE80211_CHAN_MAX];
}

/*
 * Begin an active scan.
 */
void
ieee80211_begin_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	/*
	 * In all but hostap mode scanning starts off in
	 * an active mode before switching to passive.
	 */
	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		ic->ic_flags |= IEEE80211_F_ASCAN;
		ic->ic_stats.is_scan_active++;
	} else
		ic->ic_stats.is_scan_passive++;
	if (ifp->if_flags & IFF_DEBUG)
		if_printf(ifp, "begin %s scan\n",
			(ic->ic_flags & IEEE80211_F_ASCAN) ?
				"active" : "passive");
	/*
	 * Clear scan state and flush any previously seen
	 * AP's.  Note that the latter assumes we don't act
	 * as both an AP and a station, otherwise we'll
	 * potentially flush state of stations associated
	 * with us.
	 */
	ieee80211_reset_scan(ifp);
	ieee80211_free_allnodes(ic);

	/* Scan the next channel. */
	ieee80211_next_scan(ifp);
}

/*
 * Switch to the next channel marked for scanning.
 */
void
ieee80211_next_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_channel *chan;

	chan = ic->ic_bss->ni_chan;
	for (;;) {
		if (++chan > &ic->ic_channels[IEEE80211_CHAN_MAX])
			chan = &ic->ic_channels[0];
		if (isset(ic->ic_chan_scan, ieee80211_chan2ieee(ic, chan))) {
			/*
			 * Honor channels marked passive-only
			 * during an active scan.
			 */
			if ((ic->ic_flags & IEEE80211_F_ASCAN) == 0 ||
			    (chan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0)
				break;
		}
		if (chan == ic->ic_bss->ni_chan) {
			ieee80211_end_scan(ifp);
			return;
		}
	}
	clrbit(ic->ic_chan_scan, ieee80211_chan2ieee(ic, chan));
	IEEE80211_DPRINTF(("ieee80211_next_scan: chan %d->%d\n",
	    ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan),
	    ieee80211_chan2ieee(ic, chan)));
	ic->ic_bss->ni_chan = chan;
	ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
}

void
ieee80211_create_ibss(struct ieee80211com* ic, struct ieee80211_channel *chan)
{
	struct ieee80211_node *ni;
	struct ifnet *ifp = &ic->ic_if;

	ni = ic->ic_bss;
	if (ifp->if_flags & IFF_DEBUG)
		if_printf(ifp, "creating ibss\n");
	ic->ic_flags |= IEEE80211_F_SIBSS;
	ni->ni_chan = chan;
	ni->ni_rates = ic->ic_sup_rates[ieee80211_chan2mode(ic, ni->ni_chan)];
	IEEE80211_ADDR_COPY(ni->ni_macaddr, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_myaddr);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		ni->ni_bssid[0] |= 0x02;	/* local bit for IBSS */
	ni->ni_esslen = ic->ic_des_esslen;
	memcpy(ni->ni_essid, ic->ic_des_essid, ni->ni_esslen);
	ni->ni_rssi = 0;
	ni->ni_rstamp = 0;
	memset(ni->ni_tstamp, 0, sizeof(ni->ni_tstamp));
	ni->ni_intval = ic->ic_lintval;
	ni->ni_capinfo = IEEE80211_CAPINFO_IBSS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if (ic->ic_phytype == IEEE80211_T_FH) {
		ni->ni_fhdwell = 200;	/* XXX */
		ni->ni_fhindex = 1;
	}
	ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
}

int
ieee80211_match_bss(struct ieee80211com *ic, struct ieee80211_node *ni)
{
        struct ifnet *ifp = &ic->ic_if;
        u_int8_t rate;
        int fail;

	fail = 0;
	if (isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ni->ni_chan)))
		fail |= 0x01;
	if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
	    ni->ni_chan != ic->ic_des_chan)
		fail |= 0x01;
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			fail |= 0x02;
	} else {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			fail |= 0x02;
	}
	if (ic->ic_flags & IEEE80211_F_WEPON) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
			fail |= 0x04;
	} else {
		if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
			fail |= 0x04;
	}
	rate = ieee80211_fix_rate(ic, ni, IEEE80211_F_DONEGO);
	if (rate & IEEE80211_RATE_BASIC)
		fail |= 0x08;
	if (ic->ic_des_esslen != 0 &&
	    (ni->ni_esslen != ic->ic_des_esslen ||
	     memcmp(ni->ni_essid, ic->ic_des_essid,
	     ic->ic_des_esslen != 0)))
		fail |= 0x10;
	if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(ic->ic_des_bssid, ni->ni_bssid))
		fail |= 0x20;
	if (ifp->if_flags & IFF_DEBUG) {
		printf(" %c %s", fail ? '-' : '+',
		    ether_sprintf(ni->ni_macaddr));
		printf(" %s%c", ether_sprintf(ni->ni_bssid),
		    fail & 0x20 ? '!' : ' ');
		printf(" %3d%c", ieee80211_chan2ieee(ic, ni->ni_chan),
			fail & 0x01 ? '!' : ' ');
		printf(" %+4d", ni->ni_rssi);
		printf(" %2dM%c", (rate & IEEE80211_RATE_VAL) / 2,
		    fail & 0x08 ? '!' : ' ');
		printf(" %4s%c",
		    (ni->ni_capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
		    (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" :
		    "????",
		    fail & 0x02 ? '!' : ' ');
		printf(" %3s%c ",
		    (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) ?
		    "wep" : "no",
		    fail & 0x04 ? '!' : ' ');
		ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
		printf("%s\n", fail & 0x10 ? "!" : "");
	}
	return fail;
}

/*
 * Complete a scan of potential channels.
 */
void
ieee80211_end_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_node *ni, *nextbs, *selbs;
	int i, fail;

	ic->ic_flags &= ~IEEE80211_F_ASCAN;
	ni = TAILQ_FIRST(&ic->ic_node);

	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		/* XXX off stack? */
		u_char occupied[roundup(IEEE80211_CHAN_MAX, NBBY)];
		/*
		 * The passive scan to look for existing AP's completed,
		 * select a channel to camp on.  Identify the channels
		 * that already have one or more AP's and try to locate
		 * an unnoccupied one.  If that fails, pick a random
		 * channel from the active set.
		 */
		for (; ni != NULL; ni = nextbs) {
			ieee80211_ref_node(ni);
			nextbs = TAILQ_NEXT(ni, ni_list);
			setbit(occupied, ieee80211_chan2ieee(ic, ni->ni_chan));
			ieee80211_free_node(ic, ni);
		}
		for (i = 0; i < IEEE80211_CHAN_MAX; i++)
			if (isset(ic->ic_chan_active, i) && isclr(occupied, i))
				break;
		if (i == IEEE80211_CHAN_MAX) {
			fail = arc4random() & 3;	/* random 0-3 */
			for (i = 0; i < IEEE80211_CHAN_MAX; i++)
				if (isset(ic->ic_chan_active, i) && fail-- == 0)
					break;
		}
		ieee80211_create_ibss(ic, &ic->ic_channels[i]);
		return;
	}
	if (ni == NULL) {
		IEEE80211_DPRINTF(("%s: no scan candidate\n", __func__));
  notfound:
		if (ic->ic_opmode == IEEE80211_M_IBSS &&
		    (ic->ic_flags & IEEE80211_F_IBSSON) &&
		    ic->ic_des_esslen != 0) {
			ieee80211_create_ibss(ic, ic->ic_ibss_chan);
			return;
		}
		/*
		 * Reset the list of channels to scan and start again.
		 */
		ieee80211_reset_scan(ifp);
		ieee80211_next_scan(ifp);
		return;
	}
	selbs = NULL;
	if (ifp->if_flags & IFF_DEBUG)
		if_printf(ifp, "\tmacaddr          bssid         chan  rssi rate flag  wep  essid\n");
	for (; ni != NULL; ni = nextbs) {
		ieee80211_ref_node(ni);
		nextbs = TAILQ_NEXT(ni, ni_list);
		if (ni->ni_fails) {
			/*
			 * The configuration of the access points may change
			 * during my scan.  So delete the entry for the AP
			 * and retry to associate if there is another beacon.
			 */
			if (ni->ni_fails++ > 2)
				ieee80211_free_node(ic, ni);
			continue;
		}
		if (ieee80211_match_bss(ic, ni) == 0) {
			if (selbs == NULL)
				selbs = ni;
			else if (ni->ni_rssi > selbs->ni_rssi) {
				ieee80211_unref_node(&selbs);
				selbs = ni;
			} else
				ieee80211_unref_node(&ni);
		} else {
			ieee80211_unref_node(&ni);
		}
	}
	if (selbs == NULL)
		goto notfound;
	(*ic->ic_node_copy)(ic, ic->ic_bss, selbs);
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		ieee80211_fix_rate(ic, ic->ic_bss, IEEE80211_F_DOFRATE |
		    IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (ic->ic_bss->ni_rates.rs_nrates == 0) {
			selbs->ni_fails++;
			ieee80211_unref_node(&selbs);
			goto notfound;
		}
		ieee80211_unref_node(&selbs);
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	} else {
		ieee80211_unref_node(&selbs);
		ieee80211_new_state(ic, IEEE80211_S_AUTH, -1);
	}
}

int
ieee80211_get_rate(struct ieee80211com *ic)
{
	u_int8_t (*rates)[IEEE80211_RATE_MAXSIZE];
	int rate;

	rates = &ic->ic_bss->ni_rates.rs_rates;

	if (ic->ic_fixed_rate != -1)
		rate = (*rates)[ic->ic_fixed_rate];
	else if (ic->ic_state == IEEE80211_S_RUN)
		rate = (*rates)[ic->ic_bss->ni_txrate];
	else
		rate = 0;

	return rate & IEEE80211_RATE_VAL;
}

static struct ieee80211_node *
ieee80211_node_alloc(struct ieee80211com *ic)
{
	return malloc(sizeof(struct ieee80211_node), M_80211_NODE,
		M_NOWAIT | M_ZERO);
}

static void
ieee80211_node_free(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (ni->ni_challenge != NULL) {
		free(ni->ni_challenge, M_DEVBUF);
		ni->ni_challenge = NULL;
	}
	free(ni, M_80211_NODE);
}

static void
ieee80211_node_copy(struct ieee80211com *ic,
	struct ieee80211_node *dst, const struct ieee80211_node *src)
{
	*dst = *src;
	dst->ni_challenge = NULL;
}

static u_int8_t
ieee80211_node_getrssi(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	return ni->ni_rssi;
}

static void
ieee80211_setup_node(struct ieee80211com *ic,
	struct ieee80211_node *ni, u_int8_t *macaddr)
{
	int hash;
	ieee80211_node_critsec_decl(s);

	IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
	hash = IEEE80211_NODE_HASH(macaddr);
	ni->ni_refcnt = 1;		/* mark referenced */
	ieee80211_node_critsec_begin(ic, s);
	TAILQ_INSERT_TAIL(&ic->ic_node, ni, ni_list);
	LIST_INSERT_HEAD(&ic->ic_hash[hash], ni, ni_hash);
	/* 
	 * Note we don't enable the inactive timer when acting
	 * as a station.  Nodes created in this mode represent
	 * AP's identified while scanning.  If we time them out
	 * then several things happen: we can't return the data
	 * to users to show the list of AP's we encountered, and
	 * more importantly, we'll incorrectly deauthenticate
	 * ourself because the inactivity timer will kick us off. 
	 */
	if (ic->ic_opmode != IEEE80211_M_STA)
		ic->ic_inact_timer = IEEE80211_INACT_WAIT;
	ieee80211_node_critsec_end(ic, s);
}

struct ieee80211_node *
ieee80211_alloc_node(struct ieee80211com *ic, u_int8_t *macaddr)
{
	struct ieee80211_node *ni = (*ic->ic_node_alloc)(ic);
	if (ni != NULL)
		ieee80211_setup_node(ic, ni, macaddr);
	return ni;
}

struct ieee80211_node *
ieee80211_dup_bss(struct ieee80211com *ic, u_int8_t *macaddr)
{
	struct ieee80211_node *ni = (*ic->ic_node_alloc)(ic);
	if (ni != NULL) {
		memcpy(ni, ic->ic_bss, sizeof(struct ieee80211_node));
		ieee80211_setup_node(ic, ni, macaddr);
	}
	return ni;
}

struct ieee80211_node *
ieee80211_find_node(struct ieee80211com *ic, u_int8_t *macaddr)
{
	struct ieee80211_node *ni;
	int hash;
	ieee80211_node_critsec_decl(s);

	hash = IEEE80211_NODE_HASH(macaddr);
	ieee80211_node_critsec_begin(ic, s);
	LIST_FOREACH(ni, &ic->ic_hash[hash], ni_hash) {
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr)) {
			ieee80211_node_incref(ni); /* mark referenced */
			break;
		}
	}
	ieee80211_node_critsec_end(ic, s);
	return ni;
}

struct ieee80211_node *
ieee80211_find_txnode(struct ieee80211com *ic, u_int8_t *macaddr)
{
	struct ieee80211_node *ni;
	ieee80211_node_critsec_decl(s);

	/*
	 * The destination address should be in the node table
	 * unless this is a multicast/broadcast frames or we are
	 * in station mode.
	 */
	if (IEEE80211_IS_MULTICAST(macaddr) || ic->ic_opmode == IEEE80211_M_STA)
		return ic->ic_bss;

	ieee80211_node_critsec_begin(ic, s);
	ni = ieee80211_find_node(ic, macaddr);
	if (ni == NULL) {
		if (ic->ic_opmode != IEEE80211_M_MONITOR)
			ni = ieee80211_dup_bss(ic, macaddr);
		IEEE80211_DPRINTF(("%s: faked-up node %p for %s\n",
		    __func__, ni, ether_sprintf(macaddr)));
		if (ni == NULL) {
			ieee80211_node_critsec_end(ic, s);
			/* ic->ic_stats.st_tx_nonode++; XXX statistic */
			return NULL;
		}
		(void)ieee80211_ref_node(ni);
	}
	ieee80211_node_critsec_end(ic, s);
	return ni;
}

/*
 * For some types of packet and for some operating modes, it is
 * desirable to process a Rx packet using its sender's node-record
 * instead of the BSS record, when that is possible.
 *
 * - AP mode: it is desirable to keep a node-record for every
 *   authenticated/associated station *in the BSS*. For future use,
 *   we also track neighboring APs, since they might belong to the
 *   same ESSID.
 *
 * - IBSS mode: it is desirable to keep a node-record for every
 *   station *in the BSS*.
 *
 * - monitor mode: it is desirable to keep a node-record for every
 *   sender, regardless of BSS.
 *
 * - STA mode: the only available node-record is the BSS record,
 *   ic->ic_bss.
 *
 * Of all the 802.11 Control packets, only the node-records for
 * RTS packets node-record can be looked up.
 *
 * Return non-zero if the packet's node-record is kept, zero
 * otherwise.
 */
static __inline int
ieee80211_needs_rxnode(struct ieee80211com *ic, struct ieee80211_frame *wh,
    u_int8_t **bssid)
{
	struct ieee80211_node *bss = ic->ic_bss;
	int needsnode, rc = 0;

	if (ic->ic_opmode == IEEE80211_M_STA)
		return 0;

	needsnode = (ic->ic_opmode == IEEE80211_M_MONITOR);

	*bssid = NULL;

	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_CTL:
		return (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_RTS;

	case IEEE80211_FC0_TYPE_MGT:
		*bssid = wh->i_addr3;
		rc = IEEE80211_ADDR_EQ(*bssid, bss->ni_bssid);
		break;
	case IEEE80211_FC0_TYPE_DATA:
		switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
		case IEEE80211_FC1_DIR_NODS:
			*bssid = wh->i_addr3;
			if (ic->ic_opmode == IEEE80211_M_IBSS ||
			    ic->ic_opmode == IEEE80211_M_AHDEMO)
				rc = IEEE80211_ADDR_EQ(*bssid, bss->ni_bssid);
			break;
		case IEEE80211_FC1_DIR_TODS:
			*bssid = wh->i_addr1;
			if (ic->ic_opmode == IEEE80211_M_HOSTAP)
				rc = IEEE80211_ADDR_EQ(*bssid, bss->ni_bssid);
			break;
		case IEEE80211_FC1_DIR_FROMDS:
		case IEEE80211_FC1_DIR_DSTODS:
			*bssid = wh->i_addr2;
			rc = (ic->ic_opmode == IEEE80211_M_HOSTAP);
			break;
		}
		break;
	}
	return needsnode || rc;
}

struct ieee80211_node *
ieee80211_find_rxnode(struct ieee80211com *ic, struct ieee80211_frame *wh)
{
	struct ieee80211_node *ni;
	const static u_int8_t zero[IEEE80211_ADDR_LEN];
	u_int8_t *bssid;
	ieee80211_node_critsec_decl(s);

	ieee80211_node_critsec_begin(ic, s);

	if (!ieee80211_needs_rxnode(ic, wh, &bssid))
	        return ieee80211_ref_node(ic->ic_bss);

	ni = ieee80211_find_node(ic, wh->i_addr2);

	if (ni == NULL) {
		if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
			if ((ni = ieee80211_dup_bss(ic, wh->i_addr2)) != NULL)
				IEEE80211_ADDR_COPY(ni->ni_bssid,
				    (bssid != NULL) ? bssid : zero);

			IEEE80211_DPRINTF(("%s: faked-up node %p for %s\n",
			    __func__, ni, ether_sprintf(wh->i_addr2)));
		}
		ni = ieee80211_ref_node((ni == NULL) ? ic->ic_bss : ni);
	}
	ieee80211_node_critsec_end(ic, s);
	IASSERT(ni != NULL, ("%s: null node", __func__));
	return ni;
}

/*
 * Like find but search based on the channel too.
 */
struct ieee80211_node *
ieee80211_lookup_node(struct ieee80211com *ic,
	u_int8_t *macaddr, struct ieee80211_channel *chan)
{
	struct ieee80211_node *ni;
	int hash;
	ieee80211_node_critsec_decl(s);

	hash = IEEE80211_NODE_HASH(macaddr);
	ieee80211_node_critsec_begin(ic, s);
	LIST_FOREACH(ni, &ic->ic_hash[hash], ni_hash) {
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr) && 
		    ni->ni_chan == chan) {
			ieee80211_node_incref(ni);/* mark referenced */
			break;
		}
	}
	ieee80211_node_critsec_end(ic, s);
	return ni;
}

/*
 * Like find but search based on the channel and ssid too.
 */
struct ieee80211_node *
ieee80211_lookup_node_for_beacon(struct ieee80211com *ic,
	u_int8_t *macaddr, struct ieee80211_channel *chan, char *ssid)
{
	struct ieee80211_node *ni;
	int hash;
	ieee80211_node_critsec_decl(s);

	hash = IEEE80211_NODE_HASH(macaddr);
	ieee80211_node_critsec_begin(ic, s);
	LIST_FOREACH(ni, &ic->ic_hash[hash], ni_hash) {
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr) && 
		    ni->ni_chan == chan &&
		    (ssid[1] == 0 || (ssid[1] == ni->ni_esslen &&
		     !memcmp(ssid + 2, ni->ni_essid, ssid[1])))) {
			ieee80211_node_incref(ni);/* mark referenced */
			break;
		}
	}
	ieee80211_node_critsec_end(ic, s);
	return ni;
}

static void
_ieee80211_free_node(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	IASSERT(ni != ic->ic_bss, ("freeing bss node"));

	IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
	TAILQ_REMOVE(&ic->ic_node, ni, ni_list);
	LIST_REMOVE(ni, ni_hash);
	if (!IF_IS_EMPTY(&ni->ni_savedq)) {
		IF_PURGE(&ni->ni_savedq); 
		if (ic->ic_set_tim)
			ic->ic_set_tim(ic, ni->ni_associd, 0);
	}
	if (TAILQ_EMPTY(&ic->ic_node))
		ic->ic_inact_timer = 0;
	(*ic->ic_node_free)(ic, ni);
}

void
ieee80211_free_node(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	ieee80211_node_critsec_decl(s);

	IASSERT(ni != ic->ic_bss, ("freeing ic_bss"));

	if (ieee80211_node_decref(ni) == 0) {
		ieee80211_node_critsec_begin(ic, s);
		_ieee80211_free_node(ic, ni);
		ieee80211_node_critsec_end(ic, s);
	}
}

void
ieee80211_free_allnodes(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
	ieee80211_node_critsec_decl(s);

	ieee80211_node_critsec_begin(ic, s);
	while ((ni = TAILQ_FIRST(&ic->ic_node)) != NULL)
		_ieee80211_free_node(ic, ni);  
	ieee80211_node_critsec_end(ic, s);
}

/*
 * Timeout inactive nodes.  Note that we cannot hold the node
 * lock while sending a frame as this would lead to a LOR.
 * Instead we use a generation number to mark nodes that we've
 * scanned and drop the lock and restart a scan if we have to
 * time out a node.  Since we are single-threaded by virtue of
 * controlling the inactivity timer we can be sure this will
 * process each node only once.
 */
void
ieee80211_timeout_nodes(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
	ieee80211_node_critsec_decl(s);
	u_int gen = ic->ic_scangen++;		/* NB: ok 'cuz single-threaded*/

restart:
	ieee80211_node_critsec_begin(ic, s);
	TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
		if (ni->ni_scangen == gen)	/* previously handled */
			continue;
		ni->ni_scangen = gen;
		if (++ni->ni_inact > IEEE80211_INACT_MAX) {
			IEEE80211_DPRINTF(("station %s timed out "
			    "due to inactivity (%u secs)\n",
			    ether_sprintf(ni->ni_macaddr),
			    ni->ni_inact));
			/*
			 * Send a deauthenticate frame.
			 *
			 * Drop the node lock before sending the
			 * deauthentication frame in case the driver takes     
			 * a lock, as this will result in a LOR between the     
			 * node lock and the driver lock.
			 */
			ieee80211_node_critsec_end(ic, s);
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_DEAUTH,
			    IEEE80211_REASON_AUTH_EXPIRE);
			ieee80211_free_node(ic, ni);
			ic->ic_stats.is_node_timeout++;
			goto restart;
		}
	}
	if (!TAILQ_EMPTY(&ic->ic_node))
		ic->ic_inact_timer = IEEE80211_INACT_WAIT;
	ieee80211_node_critsec_end(ic, s);
}

void
ieee80211_iterate_nodes(struct ieee80211com *ic, ieee80211_iter_func *f, void *arg)
{
	struct ieee80211_node *ni;
	ieee80211_node_critsec_decl(s);

	ieee80211_node_critsec_begin(ic, s);
	TAILQ_FOREACH(ni, &ic->ic_node, ni_list)
		(*f)(arg, ni);
	ieee80211_node_critsec_end(ic, s);
}
