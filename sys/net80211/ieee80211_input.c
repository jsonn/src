/*	$NetBSD: ieee80211_input.c,v 1.34.2.3 2004/08/12 11:42:20 skrll Exp $	*/
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
__FBSDID("$FreeBSD: src/sys/net80211/ieee80211_input.c,v 1.20 2004/04/02 23:35:24 sam Exp $");
#else
__KERNEL_RCSID(0, "$NetBSD: ieee80211_input.c,v 1.34.2.3 2004/08/12 11:42:20 skrll Exp $");
#endif

#include "opt_inet.h"

#ifdef __NetBSD__
#include "bpfilter.h"
#endif /* __NetBSD__ */

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

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h> 
#ifdef __FreeBSD__
#include <netinet/if_ether.h>
#else
#include <net/if_ether.h>
#endif
#endif

const struct timeval ieee80211_merge_print_intvl = {.tv_sec = 1, .tv_usec = 0};

static void ieee80211_recv_pspoll(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);

#ifdef IEEE80211_DEBUG
/*
 * Decide if a received management frame should be
 * printed when debugging is enabled.  This filters some
 * of the less interesting frames that come frequently
 * (e.g. beacons).
 */
static __inline int
doprint(struct ieee80211com *ic, int subtype)
{
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_BEACON:
		return (ic->ic_state == IEEE80211_S_SCAN);
	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
		return (ic->ic_opmode == IEEE80211_M_IBSS);
	}
	return 1;
}
#endif

/*
 * Process a received frame.  The node associated with the sender
 * should be supplied.  If nothing was found in the node table then
 * the caller is assumed to supply a reference to ic_bss instead.
 * The RSSI and a timestamp are also supplied.  The RSSI data is used
 * during AP scanning to select a AP to associate with; it can have
 * any units so long as values have consistent units and higher values
 * mean ``better signal''.  The receive timestamp is currently not used
 * by the 802.11 layer.
 */
void
ieee80211_input(struct ifnet *ifp, struct mbuf *m, struct ieee80211_node *ni,
	int rssi, u_int32_t rstamp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_frame *wh;
	struct ether_header *eh;
	struct mbuf *m1;
	int len;
	u_int8_t dir, type, subtype;
	u_int16_t rxseq;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	IASSERT(ni != NULL, ("null node"));

	/* trim CRC here so WEP can find its own CRC at the end of packet. */
	if (m->m_flags & M_HASFCS) {
		m_adj(m, -IEEE80211_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}

	/*
	 * In monitor mode, send everything directly to bpf.
	 * Also do not process frames w/o i_addr2 any further.
	 * XXX may want to include the CRC
	 */
	if (ic->ic_opmode == IEEE80211_M_MONITOR || 
	    m->m_pkthdr.len < sizeof(struct ieee80211_frame_min))
		goto out;

	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	    IEEE80211_FC0_VERSION_0) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			("receive packet with wrong version: %x\n",
			wh->i_fc[0]));
		ic->ic_stats.is_rx_badversion++;
		goto err;
	}

	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	/*
	 * NB: We are not yet prepared to handle control frames,
	 *     but permitting drivers to send them to us allows
	 *     them to go through bpf tapping at the 802.11 layer.
	 */
	if (m->m_pkthdr.len < sizeof(struct ieee80211_frame)) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			("%s: frame too short, len %u\n",
			__func__, m->m_pkthdr.len));
		ic->ic_stats.is_rx_tooshort++;
		goto out;
	}
	if (ic->ic_state != IEEE80211_S_SCAN) {
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		rxseq = ni->ni_rxseq;
		ni->ni_rxseq =
		    le16toh(*(u_int16_t *)wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT;
		/* TODO: fragment */
		if ((wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
		    rxseq == ni->ni_rxseq) {
			/* duplicate, silently discarded */
			ic->ic_stats.is_rx_dup++; /* XXX per-station stat */
			goto out;
		}
		ni->ni_inact = 0;
		if (ic->ic_opmode == IEEE80211_M_MONITOR)
			goto out;
	}

	if (ic->ic_set_tim != NULL &&
	    (wh->i_fc[1] & IEEE80211_FC1_PWR_MGT)
	    && ni->ni_pwrsave == 0) {
		/* turn on power save mode */

		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: power save mode on for %s\n",
			    ifp->if_xname, ether_sprintf(wh->i_addr2));

		ni->ni_pwrsave = IEEE80211_PS_SLEEP;
	}
	if (ic->ic_set_tim != NULL &&
	    (wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) == 0 &&
	    ni->ni_pwrsave != 0) {
		/* turn off power save mode, dequeue stored packets */

		ni->ni_pwrsave = 0;
		if (ic->ic_set_tim) 
			ic->ic_set_tim(ic, ni->ni_associd, 0);

		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: power save mode off for %s\n",
			    ifp->if_xname, ether_sprintf(wh->i_addr2));

		while (!IF_IS_EMPTY(&ni->ni_savedq)) {
			struct mbuf *m;
			IF_DEQUEUE(&ni->ni_savedq, m);
			IF_ENQUEUE(&ic->ic_pwrsaveq, m);
			(*ifp->if_start)(ifp);
		}
	}

	switch (type) {
	case IEEE80211_FC0_TYPE_DATA:
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			if (dir != IEEE80211_FC1_DIR_FROMDS) {
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			if (ic->ic_state != IEEE80211_S_SCAN &&
			    !IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_bssid)) {
				/* Source address is not our BSS. */
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_INPUT,
					("%s: discard frame from SA %s\n",
					__func__, ether_sprintf(wh->i_addr2)));
				ic->ic_stats.is_rx_wrongbss++;
				goto out;
			}
			if ((ifp->if_flags & IFF_SIMPLEX) &&
			    IEEE80211_IS_MULTICAST(wh->i_addr1) &&
			    IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_myaddr)) {
				/*
				 * In IEEE802.11 network, multicast packet
				 * sent from me is broadcasted from AP.
				 * It should be silently discarded for
				 * SIMPLEX interface.
				 */
				ic->ic_stats.is_rx_mcastecho++;
				goto out;
			}
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
			if (dir != IEEE80211_FC1_DIR_NODS) {
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			if (ic->ic_state != IEEE80211_S_SCAN &&
			    !IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_bss->ni_bssid) &&
			    !IEEE80211_ADDR_EQ(wh->i_addr3, ifp->if_broadcastaddr)) {
				/* Destination is not our BSS or broadcast. */
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_INPUT,
					("%s: discard data frame to DA %s\n",
					__func__, ether_sprintf(wh->i_addr3)));
				ic->ic_stats.is_rx_wrongbss++;
				goto out;
			}
			break;
		case IEEE80211_M_HOSTAP:
			if (dir != IEEE80211_FC1_DIR_TODS) {
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			if (ic->ic_state != IEEE80211_S_SCAN &&
			    !IEEE80211_ADDR_EQ(wh->i_addr1, ic->ic_bss->ni_bssid) &&
			    !IEEE80211_ADDR_EQ(wh->i_addr1, ifp->if_broadcastaddr)) {
				/* BSS is not us or broadcast. */
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_INPUT,
					("%s: discard data frame to BSS %s\n",
					__func__, ether_sprintf(wh->i_addr1)));
				ic->ic_stats.is_rx_wrongbss++;
				goto out;
			}
			/* check if source STA is associated */
			if (ni == ic->ic_bss) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_INPUT,
					("%s: data from unknown src %s\n",
					__func__, ether_sprintf(wh->i_addr2)));
				/* NB: caller deals with reference */
				ni = ieee80211_dup_bss(ic, wh->i_addr2);
				if (ni != NULL) {
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_DEAUTH,
					    IEEE80211_REASON_NOT_AUTHED);
				}
				ic->ic_stats.is_rx_notassoc++;
				goto err;
			}
			if (ni->ni_associd == 0) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_INPUT,
					("%s: data from unassoc src %s\n",
					__func__, ether_sprintf(wh->i_addr2)));
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DISASSOC,
				    IEEE80211_REASON_NOT_ASSOCED);
				ic->ic_stats.is_rx_notassoc++;
				goto err;
			}
			break;
		case IEEE80211_M_MONITOR:
			break;
		}
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			if (ic->ic_flags & IEEE80211_F_PRIVACY) {
				m = ieee80211_wep_crypt(ifp, m, 0);
				if (m == NULL) {
					ic->ic_stats.is_rx_wepfail++;
					goto err;
				}
				wh = mtod(m, struct ieee80211_frame *);
			} else {
				ic->ic_stats.is_rx_nowep++;
				goto out;
			}
		}
#if NBPFILTER > 0
		/* copy to listener after decrypt */
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
#endif
		m = ieee80211_decap(ifp, m);
		if (m == NULL) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_INPUT,
				("%s: decapsulation error for src %s\n",
				__func__, ether_sprintf(wh->i_addr2)));
			ic->ic_stats.is_rx_decap++;
			goto err;
		}
		ifp->if_ipackets++;

		/* perform as a bridge within the AP */
		m1 = NULL;
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			eh = mtod(m, struct ether_header *);
			if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
				m1 = m_copypacket(m, M_DONTWAIT);
				if (m1 == NULL)
					ifp->if_oerrors++;
				else
					m1->m_flags |= M_MCAST;
			} else {
				ni = ieee80211_find_node(ic, eh->ether_dhost);
				if (ni != NULL) {
					if (ni->ni_associd != 0) {
						m1 = m;
						m = NULL;
					}
				}
			}
			if (m1 != NULL) {
#ifdef ALTQ
				if (ALTQ_IS_ENABLED(&ifp->if_snd))
					altq_etherclassify(&ifp->if_snd, m1,
					    &pktattr);
#endif
				len = m1->m_pkthdr.len;
				IF_ENQUEUE(&ifp->if_snd, m1);
				if (m != NULL)
					ifp->if_omcasts++;
				ifp->if_obytes += len;
			}
		}
		if (m != NULL) {
#if NBPFILTER > 0
			/*
			 * If we forward packet into transmitter of the AP,
			 * we don't need to duplicate for DLT_EN10MB.
			 */
			if (ifp->if_bpf && m1 == NULL)
				bpf_mtap(ifp->if_bpf, m);
#endif
			(*ifp->if_input)(ifp, m);
		}
		return;

	case IEEE80211_FC0_TYPE_MGT:
		if (dir != IEEE80211_FC1_DIR_NODS) {
			ic->ic_stats.is_rx_wrongdir++;
			goto err;
		}
		if (ic->ic_opmode == IEEE80211_M_AHDEMO) {
			ic->ic_stats.is_rx_ahdemo_mgt++;
			goto out;
		}
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

		/* drop frames without interest */
		if (ic->ic_state == IEEE80211_S_SCAN) {
			if (subtype != IEEE80211_FC0_SUBTYPE_BEACON &&
			    subtype != IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
				ic->ic_stats.is_rx_mgtdiscard++;
				goto out;
			}
		}

#ifdef IEEE80211_DEBUG
		if ((ieee80211_msg_debug(ic) && doprint(ic, subtype)) ||
		    ieee80211_msg_dumppkts(ic)) {
			if_printf(ifp, "received %s from %s rssi %d\n",
			    ieee80211_mgt_subtype_name[subtype
			    >> IEEE80211_FC0_SUBTYPE_SHIFT],
			    ether_sprintf(wh->i_addr2), rssi);
		}
#endif
#if NBPFILTER > 0
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
#endif
		(*ic->ic_recv_mgmt)(ic, m, ni, subtype, rssi, rstamp);
		m_freem(m);
		return;

	case IEEE80211_FC0_TYPE_CTL:
		ic->ic_stats.is_rx_ctl++;
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			goto out;
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype == IEEE80211_FC0_SUBTYPE_PS_POLL) {
			/* XXX statistic */
			/* Dump out a single packet from the host */
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: got power save probe from %s\n",
				    ifp->if_xname,
				    ether_sprintf(wh->i_addr2));
			ieee80211_recv_pspoll(ic, m, rssi, rstamp);
		}
		goto out;
	default:
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			("%s: bad frame type %x\n", __func__, type));
		/* should not come here */
		break;
	}
  err:
	ifp->if_ierrors++;
  out:
	if (m != NULL) {
#if NBPFILTER > 0
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
#endif
		m_freem(m);
	}
}

struct mbuf *
ieee80211_decap(struct ifnet *ifp, struct mbuf *m)
{
	struct ether_header *eh;
	struct ieee80211_frame wh;
	struct llc *llc;

	if (m->m_len < sizeof(wh) + sizeof(*llc)) {
		m = m_pullup(m, sizeof(wh) + sizeof(*llc));
		if (m == NULL)
			return NULL;
	}
	memcpy(&wh, mtod(m, caddr_t), sizeof(wh));
	llc = (struct llc *)(mtod(m, caddr_t) + sizeof(wh));
	if (llc->llc_dsap == LLC_SNAP_LSAP && llc->llc_ssap == LLC_SNAP_LSAP &&
	    llc->llc_control == LLC_UI && llc->llc_snap.org_code[0] == 0 &&
	    llc->llc_snap.org_code[1] == 0 && llc->llc_snap.org_code[2] == 0) {
		m_adj(m, sizeof(wh) + sizeof(struct llc) - sizeof(*eh));
		llc = NULL;
	} else {
		m_adj(m, sizeof(wh) - sizeof(*eh));
	}
	eh = mtod(m, struct ether_header *);
	switch (wh.i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
		break;
	case IEEE80211_FC1_DIR_TODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr3);
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		/* not yet supported */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			("%s: discard DS to DS frame\n", __func__));
		m_freem(m);
		return NULL;
	}
#ifdef ALIGNED_POINTER
	if (!ALIGNED_POINTER(mtod(m, caddr_t) + sizeof(*eh), u_int32_t)) {
		struct mbuf *n, *n0, **np;
		caddr_t newdata;
		int off, pktlen;

		n0 = NULL;
		np = &n0;
		off = 0;
		pktlen = m->m_pkthdr.len;
		while (pktlen > off) {
			if (n0 == NULL) {
				MGETHDR(n, M_DONTWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m);
					return NULL;
				}
#ifdef __FreeBSD__
				M_MOVE_PKTHDR(n, m);
#else
				M_COPY_PKTHDR(n, m);
#endif
				n->m_len = MHLEN;
			} else {
				MGET(n, M_DONTWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m);
					m_freem(n0);
					return NULL;
				}
				n->m_len = MLEN;
			}
			if (pktlen - off >= MINCLSIZE) {
				MCLGET(n, M_DONTWAIT);
				if (n->m_flags & M_EXT)
					n->m_len = n->m_ext.ext_size;
			}
			if (n0 == NULL) {
				newdata =
				    (caddr_t)ALIGN(n->m_data + sizeof(*eh)) -
				    sizeof(*eh);
				n->m_len -= newdata - n->m_data;
				n->m_data = newdata;
			}
			if (n->m_len > pktlen - off)
				n->m_len = pktlen - off;
			m_copydata(m, off, n->m_len, mtod(n, caddr_t));
			off += n->m_len;
			*np = n;
			np = &n->m_next;
		}
		m_freem(m);
		m = n0;
	}
#endif /* ALIGNED_POINTER */
	if (llc != NULL) {
		eh = mtod(m, struct ether_header *);
		eh->ether_type = htons(m->m_pkthdr.len - sizeof(*eh));
	}
	return m;
}

/*
 * Install received rate set information in the node's state block.
 */
static int
ieee80211_setup_rates(struct ieee80211com *ic, struct ieee80211_node *ni,
	u_int8_t *rates, u_int8_t *xrates, int flags)
{
	struct ieee80211_rateset *rs = &ni->ni_rates;

	memset(rs, 0, sizeof(*rs));
	rs->rs_nrates = rates[1];
	memcpy(rs->rs_rates, rates + 2, rs->rs_nrates);
	if (xrates != NULL) {
		u_int8_t nxrates;
		/*
		 * Tack on 11g extended supported rate element.
		 */
		nxrates = xrates[1];
		if (rs->rs_nrates + nxrates > IEEE80211_RATE_MAXSIZE) {
			nxrates = IEEE80211_RATE_MAXSIZE - rs->rs_nrates;
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_XRATE,
				("%s: extended rate set too large;"
				" only using %u of %u rates\n",
				__func__, nxrates, xrates[1]));
			ic->ic_stats.is_rx_rstoobig++;
		}
		memcpy(rs->rs_rates + rs->rs_nrates, xrates+2, nxrates);
		rs->rs_nrates += nxrates;
	}
	return ieee80211_fix_rate(ic, ni, flags);
}

/* Verify the existence and length of __elem or get out. */
#define IEEE80211_VERIFY_ELEMENT(__elem, __maxlen) do {			\
	if ((__elem) == NULL) {						\
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ELEMID,		\
			("%s: no " #__elem "in %s frame\n",		\
			__func__, ieee80211_mgt_subtype_name[subtype >>	\
				IEEE80211_FC0_SUBTYPE_SHIFT]));		\
		ic->ic_stats.is_rx_elem_missing++;			\
		return;							\
	}								\
	if ((__elem)[1] > (__maxlen)) {					\
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ELEMID,		\
			("%s: bad " #__elem " len %d in %s frame from %s\n",\
			__func__, (__elem)[1],				\
			ieee80211_mgt_subtype_name[subtype >>		\
				IEEE80211_FC0_SUBTYPE_SHIFT],		\
			ether_sprintf(wh->i_addr2)));			\
		ic->ic_stats.is_rx_elem_toobig++;			\
		return;							\
	}								\
} while (0)

#define	IEEE80211_VERIFY_LENGTH(_len, _minlen) do {			\
	if ((_len) < (_minlen)) {					\
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ELEMID,		\
			("%s: %s frame too short from %s\n",		\
			__func__,					\
			ieee80211_mgt_subtype_name[subtype >>		\
				IEEE80211_FC0_SUBTYPE_SHIFT],		\
			ether_sprintf(wh->i_addr2)));			\
		ic->ic_stats.is_rx_elem_toosmall++;			\
		return;							\
	}								\
} while (0)

#ifdef IEEE80211_DEBUG
static void
ieee80211_ssid_mismatch(struct ieee80211com *ic, const char *tag,
	u_int8_t mac[IEEE80211_ADDR_LEN], u_int8_t *ssid)
{
	printf("[%s] %s req ssid mismatch: ", ether_sprintf(mac), tag);
	ieee80211_print_essid(ssid + 2, ssid[1]);
	printf("\n");
}

#define IEEE80211_VERIFY_SSID(_ni, _ssid, _packet_type) do {		\
	if ((_ssid)[1] != 0 &&						\
	    ((_ssid)[1] != (_ni)->ni_esslen ||				\
	    memcmp((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {   \
		if (ieee80211_msg_input(ic))				\
			ieee80211_ssid_mismatch(ic, _packet_type,       \
				wh->i_addr2, _ssid);			\
		ic->ic_stats.is_rx_ssidmismatch++;			\
		return;							\
	}								\
} while (0)
#else /* !IEEE80211_DEBUG */
#define IEEE80211_VERIFY_SSID(_ni, _ssid, _packet_type) do {		\
	if ((_ssid)[1] != 0 &&						\
	    ((_ssid)[1] != (_ni)->ni_esslen ||				\
	    memcmp((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {   \
		ic->ic_stats.is_rx_ssidmismatch++;			\
		return;							\
	}								\
} while (0)
#endif /* !IEEE80211_DEBUG */

static void
ieee80211_auth_open(struct ieee80211com *ic, struct ieee80211_frame *wh,
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp, u_int16_t seq,
    u_int16_t status)
{
	switch (ic->ic_opmode) {
	case IEEE80211_M_IBSS:
		if (ic->ic_state != IEEE80211_S_RUN ||
		    seq != IEEE80211_AUTH_OPEN_REQUEST) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				("%s: discard auth from %s; state %u, seq %u\n",
				__func__, ether_sprintf(wh->i_addr2),
				ic->ic_state, seq));
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		ieee80211_new_state(ic, IEEE80211_S_AUTH,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;

	case IEEE80211_M_AHDEMO:
		/* should not come here */
		break;

	case IEEE80211_M_HOSTAP:
		if (ic->ic_state != IEEE80211_S_RUN ||
		    seq != IEEE80211_AUTH_OPEN_REQUEST) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				("%s: discard auth from %s; state %u, seq %u\n",
				__func__, ether_sprintf(wh->i_addr2),
				ic->ic_state, seq));
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		if (ni == ic->ic_bss) {
			ni = ieee80211_alloc_node(ic, wh->i_addr2);
			if (ni == NULL) {
				ic->ic_stats.is_rx_nodealloc++;
				return;
			}
			IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_bss->ni_bssid);
			ni->ni_rssi = rssi;
			ni->ni_rstamp = rstamp;
			ni->ni_chan = ic->ic_bss->ni_chan;
		}
		IEEE80211_SEND_MGMT(ic, ni,
			IEEE80211_FC0_SUBTYPE_AUTH, seq + 1);
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
			("station %s %s authenticated (open)\n",
			ether_sprintf(ni->ni_macaddr),
			((ni->ni_state != IEEE80211_STA_CACHE)
			    ? "newly" : "already")));
		ieee80211_node_newstate(ni, IEEE80211_STA_AUTH);
		break;

	case IEEE80211_M_STA:
		if (ic->ic_state != IEEE80211_S_AUTH ||
		    seq != IEEE80211_AUTH_OPEN_RESPONSE) {
			ic->ic_stats.is_rx_bad_auth++;
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				("%s: discard auth from %s; state %u, seq %u\n",
				__func__, ether_sprintf(wh->i_addr2),
				ic->ic_state, seq));
			return;
		}
		if (status != 0) {
			IEEE80211_DPRINTF(ic,
			    IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
			    ("open authentication failed (reason %d) for %s\n",
			    status,
			    ether_sprintf(wh->i_addr3)));
			if (ni != ic->ic_bss)
				ni->ni_fails++;
			ic->ic_stats.is_rx_auth_fail++;
			return;
		}
		ieee80211_new_state(ic, IEEE80211_S_ASSOC,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;
	case IEEE80211_M_MONITOR:
		break;
	}
}

/* TBD send appropriate responses on error? */
static void
ieee80211_auth_shared(struct ieee80211com *ic, struct ieee80211_frame *wh,
    u_int8_t *frm, u_int8_t *efrm, struct ieee80211_node *ni, int rssi,
    u_int32_t rstamp, u_int16_t seq, u_int16_t status)
{
	u_int8_t *challenge = NULL;
	int i;

	if ((ic->ic_flags & IEEE80211_F_PRIVACY) == 0) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
			("%s: WEP is off\n", __func__));
		return;
	}

	if (frm + 1 < efrm) {
		if (frm[1] + 2 > efrm - frm) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				("%s: elt %d %d bytes too long\n", __func__,
				frm[0], (frm[1] + 2) - (int)(efrm - frm)));
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		if (*frm == IEEE80211_ELEMID_CHALLENGE)
			challenge = frm;
		frm += frm[1] + 2;
	}
	switch (seq) {
	case IEEE80211_AUTH_SHARED_CHALLENGE:
	case IEEE80211_AUTH_SHARED_RESPONSE:
		if (challenge == NULL) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				("%s: no challenge sent\n", __func__));
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		if (challenge[1] != IEEE80211_CHALLENGE_LEN) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				("%s: bad challenge len %d\n",
				__func__, challenge[1]));
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
	default:
		break;
	}
	switch (ic->ic_opmode) {
	case IEEE80211_M_MONITOR:
	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_IBSS:
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
			("%s: unexpected operating mode\n", __func__));
		return;
	case IEEE80211_M_HOSTAP:
		if (ic->ic_state != IEEE80211_S_RUN) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				("%s: not running\n", __func__));
			return;
		}
		switch (seq) {
		case IEEE80211_AUTH_SHARED_REQUEST:
			if (ni == ic->ic_bss) {
				ni = ieee80211_alloc_node(ic, wh->i_addr2);
				if (ni == NULL) {
					ic->ic_stats.is_rx_nodealloc++;
					return;
				}
				IEEE80211_ADDR_COPY(ni->ni_bssid,
				    ic->ic_bss->ni_bssid);
				ni->ni_rssi = rssi;
				ni->ni_rstamp = rstamp;
				ni->ni_chan = ic->ic_bss->ni_chan;
			}
			if (ni->ni_challenge == NULL)
				ni->ni_challenge = (u_int32_t*)malloc(
				    IEEE80211_CHALLENGE_LEN, M_DEVBUF,
				    M_NOWAIT);
			if (ni->ni_challenge == NULL) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
					("%s: challenge alloc failed\n",
					__func__));
				/* XXX statistic */
				return;
			}
			for (i = IEEE80211_CHALLENGE_LEN / sizeof(u_int32_t);
			     --i >= 0; )
				ni->ni_challenge[i] = arc4random();
			IEEE80211_DPRINTF(ic,
				IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
				("shared key %sauth request from station %s\n",
				((ni->ni_state != IEEE80211_STA_CACHE)
				    ? "" : "re"),
				ether_sprintf(ni->ni_macaddr)));
			break;
		case IEEE80211_AUTH_SHARED_RESPONSE:
			if (ni == ic->ic_bss) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
					("%s: unknown STA\n", __func__));
				return;
			}
			if (ni->ni_challenge == NULL) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
					("%s: no challenge recorded\n",
					__func__));
				ic->ic_stats.is_rx_bad_auth++;
				return;
			}
			if (memcmp(ni->ni_challenge, &challenge[2],
				   challenge[1]) != 0) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
					("%s: challenge mismatch\n", __func__));
				ic->ic_stats.is_rx_auth_fail++;
				return;
			}
			IEEE80211_DPRINTF(ic,
				IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
				("station %s authenticated (shared key)\n",
				ether_sprintf(ni->ni_macaddr)));
			ieee80211_node_newstate(ni, IEEE80211_STA_AUTH);
			break;
		default:
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				("%s: bad shared key auth seq %d from %s\n",
				__func__, seq, ether_sprintf(wh->i_addr2)));
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		IEEE80211_SEND_MGMT(ic, ni,
			IEEE80211_FC0_SUBTYPE_AUTH, seq + 1);
		break;

	case IEEE80211_M_STA:
		if (ic->ic_state != IEEE80211_S_AUTH)
			return;
		switch (seq) {
		case IEEE80211_AUTH_SHARED_PASS:
			if (ni->ni_challenge != NULL) {
				FREE(ni->ni_challenge, M_DEVBUF);
				ni->ni_challenge = NULL;
			}
			if (status != 0) {
				IEEE80211_DPRINTF(ic,
				    IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
				    ("%s: auth failed (reason %d) for %s\n",
				    __func__, status,
				    ether_sprintf(wh->i_addr3)));
				if (ni != ic->ic_bss)
					ni->ni_fails++;
				ic->ic_stats.is_rx_auth_fail++;
				return;
			}
			ieee80211_new_state(ic, IEEE80211_S_ASSOC,
			    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
		case IEEE80211_AUTH_SHARED_CHALLENGE:
			if (ni->ni_challenge == NULL)
				ni->ni_challenge = (u_int32_t*)malloc(
				    challenge[1], M_DEVBUF, M_NOWAIT);
			if (ni->ni_challenge == NULL) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				    ("%s: challenge alloc failed\n", __func__));
				/* XXX statistic */
				return;
			}
			memcpy(ni->ni_challenge, &challenge[2], challenge[1]);
			IEEE80211_SEND_MGMT(ic, ni,
				IEEE80211_FC0_SUBTYPE_AUTH, seq + 1);
			break;
		default:
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				("%s: bad seq %d from %s\n", __func__, seq,
				ether_sprintf(wh->i_addr2)));
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		break;
	}
}

void
ieee80211_recv_mgmt(struct ieee80211com *ic, struct mbuf *m0,
	struct ieee80211_node *ni,
	int subtype, int rssi, u_int32_t rstamp)
{
#define	ISPROBE(_st)	((_st) == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
#define	ISREASSOC(_st)	((_st) == IEEE80211_FC0_SUBTYPE_REASSOC_RESP)
	struct ieee80211_frame *wh;
	u_int8_t *frm, *efrm;
	u_int8_t *ssid, *rates, *xrates;
	int is_new, reassoc, resp;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_BEACON: {
		u_int8_t *tstamp, *bintval, *capinfo, *country;
		u_int8_t chan, bchan, fhindex, erp;
		u_int16_t fhdwell;

		/*
		 * We process beacon/probe response frames for:
		 *    o station mode: to collect state
		 *      updates such as 802.11g slot time and for passive
		 *      scanning of APs
		 *    o adhoc mode: to discover neighbors
		 *    o hostap mode: for passive scanning of neighbor APs
		 *    o when scanning
		 * In other words, in all modes other than monitor (which
		 * does not process incoming packets) and adhoc-demo (which
		 * does not use management frames at all).
		 */
#ifdef DIAGNOSTIC
		if (ic->ic_opmode != IEEE80211_M_STA &&
		    ic->ic_opmode != IEEE80211_M_IBSS &&
		    ic->ic_opmode != IEEE80211_M_HOSTAP &&
		    ic->ic_state != IEEE80211_S_SCAN) {
			panic("%s: impossible", __func__);
		}
#endif
		/*
		 * beacon/probe response frame format
		 *	[8] time stamp
		 *	[2] beacon interval
		 *	[2] capability information
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] country information
		 *	[tlv] parameter set (FH/DS)
		 *	[tlv] erp information
		 *	[tlv] extended supported rates
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 12);
		tstamp  = frm;	frm += 8;
		bintval = frm;	frm += 2;
		capinfo = frm;	frm += 2;
		ssid = rates = xrates = country = NULL;
		bchan = ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan);
		chan = bchan;
		fhdwell = 0;
		fhindex = 0;
		erp = 0;
		while (frm < efrm) {
			switch (*frm) {
			case IEEE80211_ELEMID_SSID:
				ssid = frm;
				break;
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_COUNTRY:
				country = frm;
				break;
			case IEEE80211_ELEMID_FHPARMS:
				if (ic->ic_phytype == IEEE80211_T_FH) {
					fhdwell = (frm[3] << 8) | frm[2];
					chan = IEEE80211_FH_CHAN(frm[4], frm[5]);
					fhindex = frm[6];
				}
				break;
			case IEEE80211_ELEMID_DSPARMS:
				/*
				 * XXX hack this since depending on phytype
				 * is problematic for multi-mode devices.
				 */
				if (ic->ic_phytype != IEEE80211_T_FH)
					chan = frm[2];
				break;
			case IEEE80211_ELEMID_TIM:
				break;
			case IEEE80211_ELEMID_IBSSPARMS:
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			case IEEE80211_ELEMID_ERP:
				if (frm[1] != 1) {
					IEEE80211_DPRINTF(ic,
						IEEE80211_MSG_ELEMID,
						("%s: invalid ERP element; "
						"length %u, expecting 1\n",
						__func__, frm[1]));
					ic->ic_stats.is_rx_elem_toobig++;
					break;
				}
				erp = frm[2];
				break;
			default:
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ELEMID,
					("%s: element id %u/len %u ignored\n",
					__func__, *frm, frm[1]));
				ic->ic_stats.is_rx_elem_unknown++;
				break;
			}
			frm += frm[1] + 2;
		}
		IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
		IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
		if (
#if IEEE80211_CHAN_MAX < 255
		    chan > IEEE80211_CHAN_MAX ||
#endif
		    isclr(ic->ic_chan_active, chan)) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ELEMID,
				("%s: ignore %s with invalid channel %u\n",
				__func__,
				ISPROBE(subtype) ? "probe response" : "beacon",
				chan));
			ic->ic_stats.is_rx_badchan++;
			return;
		}
		if (chan != bchan && ic->ic_phytype != IEEE80211_T_FH) {
			/*
			 * Frame was received on a channel different from the
			 * one indicated in the DS params element id;
			 * silently discard it.
			 *
			 * NB: this can happen due to signal leakage.
			 *     But we should take it for FH phy because
			 *     the rssi value should be correct even for
			 *     different hop pattern in FH.
			 */
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ELEMID,
				("%s: ignore %s on channel %u marked "
				"for channel %u\n", __func__,
				ISPROBE(subtype) ? "probe response" : "beacon",
				bchan, chan));
			ic->ic_stats.is_rx_chanmismatch++;
			return;
		}

		/*
		 * Use mac and channel for lookup so we collect all
		 * potential AP's when scanning.  Otherwise we may
		 * see the same AP on multiple channels and will only
		 * record the last one.  We could filter APs here based
		 * on rssi, etc. but leave that to the end of the scan
		 * so we can keep the selection criteria in one spot.
		 * This may result in a bloat of the scanned AP list but
		 * it shouldn't be too much.
		 */
		ni = ieee80211_find_node_for_beacon(ic, wh->i_addr2,
				&ic->ic_channels[chan], ssid);
#ifdef IEEE80211_DEBUG
		if (ieee80211_debug &&
		    (ni == NULL || ic->ic_state == IEEE80211_S_SCAN)) {
			printf("%s: %s%s on chan %u (bss chan %u) ",
			    __func__, (ni == NULL ? "new " : ""),
			    ISPROBE(subtype) ? "probe response" : "beacon",
			    chan, bchan);
			ieee80211_print_essid(ssid + 2, ssid[1]);
			printf(" from %s\n", ether_sprintf(wh->i_addr2));
			printf("%s: caps 0x%x bintval %u erp 0x%x\n",
				__func__, le16toh(*(u_int16_t *)capinfo),
				le16toh(*(u_int16_t *)bintval), erp);
			if (country) {
				int i;
				printf("%s: country info", __func__);
				for (i = 0; i < country[1]; i++)
					printf(" %02x", country[i+2]);
				printf("\n");
			}
		}
#endif
		if (ni == NULL) {
			ni = ieee80211_alloc_node(ic, wh->i_addr2);
			if (ni == NULL)
				return;
			is_new = 1;
		} else
			is_new = 0;
		if (ssid[1] != 0 && ni->ni_esslen == 0) {
			/*
			 * Update ESSID at probe response to adopt hidden AP by
			 * Lucent/Cisco, which announces null ESSID in beacon.
			 */
			ni->ni_esslen = ssid[1];
			memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
			memcpy(ni->ni_essid, ssid + 2, ssid[1]);
		}
		IEEE80211_ADDR_COPY(ni->ni_bssid, wh->i_addr3);
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		memcpy(ni->ni_tstamp, tstamp, sizeof(ni->ni_tstamp));
		ni->ni_intval = le16toh(*(u_int16_t *)bintval);
		ni->ni_capinfo = le16toh(*(u_int16_t *)capinfo);
		/* XXX validate channel # */
		ni->ni_chan = &ic->ic_channels[chan];
		ni->ni_fhdwell = fhdwell;
		ni->ni_fhindex = fhindex;
		ni->ni_erp = erp;
		/* NB: must be after ni_chan is setup */
		ieee80211_setup_rates(ic, ni, rates, xrates, IEEE80211_F_DOSORT);
		/*
		 * When scanning we record results (nodes) with a zero
		 * refcnt.  Otherwise we want to hold the reference for
		 * ibss neighbors so the nodes don't get released prematurely.
		 * Anything else can be discarded (XXX and should be handled
		 * above so we don't do so much work). 
		 */
		if (ic->ic_opmode == IEEE80211_M_IBSS || (is_new &&
		    ISPROBE(subtype))) {
			/*
			 * Fake an association so the driver can setup it's
			 * private state.  The rate set has been setup above;
			 * there is no handshake as in ap/station operation.
			 */
			if (ic->ic_newassoc)
				(*ic->ic_newassoc)(ic, ni, 1);
		}
		break;
	}

	case IEEE80211_FC0_SUBTYPE_PROBE_REQ: {
		u_int8_t rate;

		if (ic->ic_opmode == IEEE80211_M_STA)
			return;
		if (ic->ic_state != IEEE80211_S_RUN)
			return;

		/*
		 * prreq frame format
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 */
		ssid = rates = xrates = NULL;
		while (frm < efrm) {
			switch (*frm) {
			case IEEE80211_ELEMID_SSID:
				ssid = frm;
				break;
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			}
			frm += frm[1] + 2;
		}
		IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
		IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
		IEEE80211_VERIFY_SSID(ic->ic_bss, ssid, "probe");

		if (ni == ic->ic_bss) {
			ni = ieee80211_dup_bss(ic, wh->i_addr2);
			if (ni == NULL)
				return;
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
				("%s: new probe req from %s\n",
				__func__, ether_sprintf(wh->i_addr2)));
		}
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		rate = ieee80211_setup_rates(ic, ni, rates, xrates,
				IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE
				| IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (rate & IEEE80211_RATE_BASIC) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_XRATE,
				("%s: rate negotiation failed: %s\n",
				__func__,ether_sprintf(wh->i_addr2)));
		} else {
			IEEE80211_SEND_MGMT(ic, ni,
				IEEE80211_FC0_SUBTYPE_PROBE_RESP, 0);
		}
		break;
	}

	case IEEE80211_FC0_SUBTYPE_AUTH: {
		u_int16_t algo, seq, status;
		/*
		 * auth frame format
		 *	[2] algorithm
		 *	[2] sequence
		 *	[2] status
		 *	[tlv*] challenge
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 6);
		algo   = le16toh(*(u_int16_t *)frm);
		seq    = le16toh(*(u_int16_t *)(frm + 2));
		status = le16toh(*(u_int16_t *)(frm + 4));
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
			("%s: algorithm %d seq %d from %s\n",
			__func__, algo, seq, ether_sprintf(wh->i_addr2)));

		if (algo == IEEE80211_AUTH_ALG_SHARED)
			ieee80211_auth_shared(ic, wh, frm + 6, efrm, ni, rssi,
			    rstamp, seq, status);
		else if (algo == IEEE80211_AUTH_ALG_OPEN)
			ieee80211_auth_open(ic, wh, ni, rssi, rstamp, seq,
			    status);
		else {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
				("%s: unsupported auth algorithm %d from %s\n",
				__func__, algo, ether_sprintf(wh->i_addr2)));
			ic->ic_stats.is_rx_auth_unsupported++;
			return;
		}
		break;
	}

	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ: {
		u_int16_t capinfo, bintval;

		if (ic->ic_opmode != IEEE80211_M_HOSTAP ||
		    (ic->ic_state != IEEE80211_S_RUN))
			return;

		if (subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
			reassoc = 1;
			resp = IEEE80211_FC0_SUBTYPE_REASSOC_RESP;
		} else {
			reassoc = 0;
			resp = IEEE80211_FC0_SUBTYPE_ASSOC_RESP;
		}
		/*
		 * asreq frame format
		 *	[2] capability information
		 *	[2] listen interval
		 *	[6*] current AP address (reassoc only)
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, (reassoc ? 10 : 4));
		if (!IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_bss->ni_bssid)) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
				("%s: ignore assoc request with bss %s not "
				"our own\n",
				__func__, ether_sprintf(wh->i_addr2)));
			ic->ic_stats.is_rx_assoc_bss++;
			return;
		}
		capinfo = le16toh(*(u_int16_t *)frm);	frm += 2;
		bintval = le16toh(*(u_int16_t *)frm);	frm += 2;
		if (reassoc)
			frm += 6;	/* ignore current AP info */
		ssid = rates = xrates = NULL;
		while (frm < efrm) {
			switch (*frm) {
			case IEEE80211_ELEMID_SSID:
				ssid = frm;
				break;
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			}
			frm += frm[1] + 2;
		}
		IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
		IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
		IEEE80211_VERIFY_SSID(ic->ic_bss, ssid,
			reassoc ? "reassoc" : "assoc");

		if (ni->ni_state != IEEE80211_STA_AUTH &&
		    ni->ni_state != IEEE80211_STA_ASSOC) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			    ("%s: deny %sassoc from %s, not authenticated\n",
			    __func__, reassoc ? "re" : "",
			    ether_sprintf(wh->i_addr2)));
			ni = ieee80211_dup_bss(ic, wh->i_addr2);
			if (ni != NULL) {
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_ASSOC_NOT_AUTHED);
			}
			ic->ic_stats.is_rx_assoc_notauth++;
			return;
		}
		/* discard challenge after association */
		if (ni->ni_challenge != NULL) {
			FREE(ni->ni_challenge, M_DEVBUF);
			ni->ni_challenge = NULL;
		}
		/* XXX per-node cipher suite */
		/* XXX some stations use the privacy bit for handling APs
		       that suport both encrypted and unencrypted traffic */
		if ((capinfo & IEEE80211_CAPINFO_ESS) == 0 ||
		    (capinfo & IEEE80211_CAPINFO_PRIVACY) !=
		    ((ic->ic_flags & IEEE80211_F_PRIVACY) ?
		     IEEE80211_CAPINFO_PRIVACY : 0)) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
				("%s: capability mismatch %x for %s\n",
				__func__, capinfo, ether_sprintf(wh->i_addr2)));
			IEEE80211_SEND_MGMT(ic, ni, resp,
				IEEE80211_STATUS_CAPINFO);
			ieee80211_node_leave(ic, ni);
			ic->ic_stats.is_rx_assoc_capmismatch++;
			return;
		}
		ieee80211_setup_rates(ic, ni, rates, xrates,
				IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
				IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (ni->ni_rates.rs_nrates == 0) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
				("%s: rate mismatch for %s\n",
				__func__, ether_sprintf(wh->i_addr2)));
			/* XXX what rate will we send this at? */
			IEEE80211_SEND_MGMT(ic, ni, resp,
				IEEE80211_STATUS_BASIC_RATE);
			ieee80211_node_leave(ic, ni);
			ic->ic_stats.is_rx_assoc_norate++;
			return;
		}
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		ni->ni_intval = bintval;
		ni->ni_capinfo = capinfo;
		ni->ni_chan = ic->ic_bss->ni_chan;
		ni->ni_fhdwell = ic->ic_bss->ni_fhdwell;
		ni->ni_fhindex = ic->ic_bss->ni_fhindex;
		ieee80211_node_join(ic, ni, resp);
		break;
	}

	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP: {
		u_int16_t status;

		if (ic->ic_opmode != IEEE80211_M_STA ||
		    ic->ic_state != IEEE80211_S_ASSOC) {
			ic->ic_stats.is_rx_mgtdiscard++;
			return;
		}

		/*
		 * asresp frame format
		 *	[2] capability information
		 *	[2] status
		 *	[2] association ID
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 6);
		ni = ic->ic_bss;
		ni->ni_capinfo = le16toh(*(u_int16_t *)frm);
		frm += 2;

		status = le16toh(*(u_int16_t *)frm);
		frm += 2;
		if (status != 0) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
				("%sassociation failed (reason %d) for %s\n",
				ISREASSOC(subtype) ?  "re" : "",
				status, ether_sprintf(wh->i_addr3)));
			if (ni != ic->ic_bss)
				ni->ni_fails++;
			ic->ic_stats.is_rx_auth_fail++;
			return;
		}
		ni->ni_associd = le16toh(*(u_int16_t *)frm);
		frm += 2;

		rates = xrates = NULL;
		while (frm < efrm) {
			switch (*frm) {
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			}
			frm += frm[1] + 2;
		}

		IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
		ieee80211_setup_rates(ic, ni, rates, xrates,
				IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
				IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (ni->ni_rates.rs_nrates != 0)
			ieee80211_new_state(ic, IEEE80211_S_RUN,
				wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;
	}

	case IEEE80211_FC0_SUBTYPE_DEAUTH: {
		u_int16_t reason;
		/*
		 * deauth frame format
		 *	[2] reason
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
		reason = le16toh(*(u_int16_t *)frm);
		ic->ic_stats.is_rx_deauth++;
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			ieee80211_new_state(ic, IEEE80211_S_AUTH,
			    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
		case IEEE80211_M_HOSTAP:
			if (ni != ic->ic_bss) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
					("station %s deauthenticated by "
					"peer (reason %d)\n",
					ether_sprintf(ni->ni_macaddr), reason));
				ieee80211_node_leave(ic, ni);
			}
			break;
		default:
			break;
		}
		break;
	}

	case IEEE80211_FC0_SUBTYPE_DISASSOC: {
		u_int16_t reason;
		/*
		 * disassoc frame format
		 *	[2] reason
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
		reason = le16toh(*(u_int16_t *)frm);
		ic->ic_stats.is_rx_disassoc++;
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			ieee80211_new_state(ic, IEEE80211_S_ASSOC,
			    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
		case IEEE80211_M_HOSTAP:
			if (ni != ic->ic_bss) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
					("station %s disassociated by "
					"peer (reason %d)\n",
					ether_sprintf(ni->ni_macaddr), reason));
				ieee80211_node_leave(ic, ni);
			}
			break;
		default:
			break;
		}
		break;
	}
	default:
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			("%s: mgmt frame with subtype 0x%x not handled\n",
			__func__, subtype));
		ic->ic_stats.is_rx_badsubtype++;
		break;
	}
}

static void
ieee80211_recv_pspoll(struct ieee80211com *ic, struct mbuf *m0, int rssi,
		      u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m;
	u_int16_t aid;

	if (ic->ic_set_tim == NULL)  /* No powersaving functionality */
		return;

	wh = mtod(m0, struct ieee80211_frame *);

	if ((ni = ieee80211_find_node(ic, wh->i_addr2)) == NULL) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s sent bogus power save poll\n",
			       ifp->if_xname, ether_sprintf(wh->i_addr2));
		return;
	}

	memcpy(&aid, wh->i_dur, sizeof(wh->i_dur));
	if ((aid & 0xc000) != 0xc000) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s sent bogus aid %x\n",
			       ifp->if_xname, ether_sprintf(wh->i_addr2), aid);
		return;
	}

	if (aid != ni->ni_associd) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s aid %x doesn't match pspoll "
			       "aid %x\n",
			       ifp->if_xname, ether_sprintf(wh->i_addr2),
			       ni->ni_associd, aid);
		return;
	}

	/* Okay, take the first queued packet and put it out... */

	IF_DEQUEUE(&ni->ni_savedq, m);
	if (m == NULL) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s sent pspoll, "
			       "but no packets are saved\n",
			       ifp->if_xname, ether_sprintf(wh->i_addr2));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);

	/* 
	 * If this is the last packet, turn off the TIM fields.
	 * If there are more packets, set the more packets bit.
	 */

	if (IF_IS_EMPTY(&ni->ni_savedq)) {
		if (ic->ic_set_tim) 
			ic->ic_set_tim(ic, ni->ni_associd, 0);
	} else {
		wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
	}

	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: enqueued power saving packet for station %s\n",
		       ifp->if_xname, ether_sprintf(ni->ni_macaddr));

	IF_ENQUEUE(&ic->ic_pwrsaveq, m);
	(*ifp->if_start)(ifp);
}

static int
do_slow_print(struct ieee80211com *ic, int *did_print)
{
	if ((ic->ic_if.if_flags & IFF_LINK0) == 0)
		return 0;
	if (!*did_print && (ic->ic_if.if_flags & IFF_DEBUG) == 0 &&
	    !ratecheck(&ic->ic_last_merge_print, &ieee80211_merge_print_intvl))
		return 0;

	*did_print = 1;
	return 1;
}

/* ieee80211_ibss_merge helps merge 802.11 ad hoc networks.  The
 * convention, set by the Wireless Ethernet Compatibility Alliance
 * (WECA), is that an 802.11 station will change its BSSID to match
 * the "oldest" 802.11 ad hoc network, on the same channel, that
 * has the station's desired SSID.  The "oldest" 802.11 network
 * sends beacons with the greatest TSF timestamp.
 *
 * Return ENETRESET if the BSSID changed, 0 otherwise.
 *
 * XXX Perhaps we should compensate for the time that elapses
 * between the MAC receiving the beacon and the host processing it
 * in ieee80211_ibss_merge.
 */
int
ieee80211_ibss_merge(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint64_t local_tsft)
{
	uint64_t beacon_tsft;
	int did_print = 0, sign;
	union {
		uint64_t	word;
		uint8_t		tstamp[8];
	} u;

	/* ensure alignment */
	(void)memcpy(&u, &ni->ni_tstamp[0], sizeof(u));
	beacon_tsft = le64toh(u.word);

	/* we are faster, let the other guy catch up */
	if (beacon_tsft < local_tsft)
		sign = -1;
	else
		sign = 1;

	if (memcmp(ni->ni_bssid, ic->ic_bss->ni_bssid,
	    IEEE80211_ADDR_LEN) == 0) {
		if (!do_slow_print(ic, &did_print))
			return 0;
		printf("%s: tsft offset %s%" PRIu64 "\n", ic->ic_if.if_xname,
		    (sign < 0) ? "-" : "",
		    (sign < 0)
			? (local_tsft - beacon_tsft)
			: (beacon_tsft - local_tsft));
		return 0;
	}

	if (sign < 0)
		return 0;

	if (ieee80211_match_bss(ic, ni) != 0)
		return 0;

	if (do_slow_print(ic, &did_print)) {
		printf("%s: atw_recv_beacon: bssid mismatch %s\n",
		    ic->ic_if.if_xname, ether_sprintf(ni->ni_bssid));
		printf("%s: my tsft %" PRIu64 " beacon tsft %" PRIu64 "\n",
		    ic->ic_if.if_xname, local_tsft, beacon_tsft);
		printf("%s: sync TSF with %s\n",
		    ic->ic_if.if_xname, ether_sprintf(ni->ni_macaddr));
	}

	ic->ic_flags &= ~IEEE80211_F_SIBSS;

	/* negotiate rates with new IBSS */
	ieee80211_fix_rate(ic, ni, IEEE80211_F_DOFRATE |
	    IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
	if (ni->ni_rates.rs_nrates == 0) {
		if (do_slow_print(ic, &did_print)) {
			printf("%s: rates mismatch, BSSID %s\n",
			    ic->ic_if.if_xname, ether_sprintf(ni->ni_bssid));
		}
		return 0;
	}

	if (do_slow_print(ic, &did_print)) {
		printf("%s: sync BSSID %s -> ",
		    ic->ic_if.if_xname, ether_sprintf(ic->ic_bss->ni_bssid));
		printf("%s ", ether_sprintf(ni->ni_bssid));
		printf("(from %s)\n", ether_sprintf(ni->ni_macaddr));
	}

	ieee80211_node_newstate(ni, IEEE80211_STA_BSS);
	(*ic->ic_node_copy)(ic, ic->ic_bss, ni);

	return ENETRESET;
}
#undef IEEE80211_VERIFY_LENGTH
#undef IEEE80211_VERIFY_ELEMENT
