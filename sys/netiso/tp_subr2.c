/*	$NetBSD: tp_subr2.c,v 1.35.2.1 2007/12/26 19:57:51 ad Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tp_subr2.c	8.1 (Berkeley) 6/10/93
 */

/***********************************************************
		Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */
/*
 * Some auxiliary routines: tp_protocol_error: required by xebec- called when
 * a combo of state, event, predicate isn't covered for by the transition
 * file. tp_indicate: gives indications(signals) to the user process
 * tp_getoptions: initializes variables that are affected by the options
 * chosen.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tp_subr2.c,v 1.35.2.1 2007/12/26 19:57:51 ad Exp $");

/*
 * this def'n is to cause the expansion of this macro in the routine
 * tp_local_credit :
 */
#define LOCAL_CREDIT_EXPAND

#include "opt_inet.h"
#include "opt_iso.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_types.h>

#include <netiso/argo_debug.h>
#include <netiso/tp_param.h>
#include <netiso/tp_ip.h>
#include <netiso/iso.h>
#include <netiso/iso_errno.h>
#include <netiso/iso_pcb.h>
#include <netiso/iso_var.h>
#include <netiso/tp_timer.h>
#include <netiso/tp_stat.h>
#include <netiso/tp_tpdu.h>
#include <netiso/tp_pcb.h>
#include <netiso/tp_seq.h>
#include <netiso/tp_trace.h>
#include <netiso/tp_user.h>
#include <netiso/tp_var.h>
#include <netiso/cons.h>
#include <netiso/clnp.h>

#if 0
static void copyQOSparms (struct tp_conn_param *, struct tp_conn_param *);
#endif

/*
 * NAME: 	tp_local_credit()
 *
 * CALLED FROM:
 *  tp_emit(), tp_usrreq()
 *
 * FUNCTION and ARGUMENTS:
 *	Computes the local credit and stashes it in tpcb->tp_lcredit.
 *  It's a macro in the production system rather than a procdure.
 *
 * RETURNS:
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 *  This doesn't actually get called in a production system -
 *  the macro gets expanded instead in place of calls to this proc.
 *  But for debugging, we call this and that allows us to add
 *  debugging messages easily here.
 */
void
tp_local_credit(struct tp_pcb *tpcb)
{
	LOCAL_CREDIT(tpcb);
#ifdef ARGO_DEBUG
	if (argo_debug[D_CREDIT]) {
		printf("ref 0x%x lcdt 0x%x l_tpdusize 0x%x decbit 0x%x cong_win 0x%lx\n",
		       tpcb->tp_lref,
		       tpcb->tp_lcredit,
		       tpcb->tp_l_tpdusize,
		       tpcb->tp_decbit,
		       tpcb->tp_cong_win
			);
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_CREDIT]) {
		tptraceTPCB(TPPTmisc,
			    "lcdt tpdusz \n",
			    tpcb->tp_lcredit, tpcb->tp_l_tpdusize, 0, 0);
	}
#endif
}

/*
 * NAME:  tp_protocol_error()
 *
 * CALLED FROM:
 *  tp_driver(), when it doesn't know what to do with
 * 	a combo of event, state, predicate
 *
 * FUNCTION and ARGUMENTS:
 *  print error mesg
 *
 * RETURN VALUE:
 *  EIO - always
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
int
tp_protocol_error(struct tp_event *e, struct tp_pcb  *tpcb)
{
	printf("TP PROTOCOL ERROR! tpcb %p event 0x%x, state 0x%x\n",
	       tpcb, e->ev_number, tpcb->tp_state);
#ifdef TPPT
	if (tp_traceflags[D_DRIVER]) {
		tptraceTPCB(TPPTmisc, "PROTOCOL ERROR tpcb event state",
			    tpcb, e->ev_number, tpcb->tp_state, 0);
	}
#endif
	return EIO;	/* for lack of anything better */
}


/* Not used at the moment */
void
tp_drain(void)
{
}


/*
 * NAME: tp_indicate()
 *
 * CALLED FROM:
 * 	tp.trans when XPD arrive, when a connection is being disconnected by
 *  the arrival of a DR or ER, and when a connection times out.
 *
 * FUNCTION and ARGUMENTS:
 *  (ind) is the type of indication : T_DISCONNECT, T_XPD
 *  (error) is an E* value that will be put in the socket structure
 *  to be passed along to the user later.
 * 	Gives a SIGURG to the user process or group indicated by the socket
 * 	attached to the tpcb.
 *
 * RETURNS:  Rien
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
tp_indicate(int ind, struct tp_pcb *tpcb, u_int error)
{
	struct socket *so = tpcb->tp_sock;
#ifdef TPPT
	if (tp_traceflags[D_INDICATION]) {
		tptraceTPCB(TPPTindicate, ind, *(u_short *) (tpcb->tp_lsuffix),
		       *(u_short *) (tpcb->tp_fsuffix), error, so->so_pgid);
	}
#endif
#ifdef ARGO_DEBUG
	if (argo_debug[D_INDICATION]) {
		char           *ls, *fs;
		ls = tpcb->tp_lsuffix,
			fs = tpcb->tp_fsuffix,

			printf(
			       "indicate 0x%x lsuf 0x%02x%02x fsuf 0x%02x%02x err 0x%x  noind 0x%x ref 0x%x\n",
			       ind,
			       *ls, *(ls + 1), *fs, *(fs + 1),
			       error,	/* so->so_pgrp, */
			       tpcb->tp_no_disc_indications,
			       tpcb->tp_lref);
	}
#endif

	if (ind == ER_TPDU) {
		struct mbuf *m;
		struct tp_disc_reason x;

		if ((so->so_state & SS_CANTRCVMORE) == 0 &&
		    (m = m_get(M_DONTWAIT, MT_OOBDATA)) != 0) {

			x.dr_hdr.cmsg_len = m->m_len = sizeof(x);
			x.dr_hdr.cmsg_level = SOL_TRANSPORT;
			x.dr_hdr.cmsg_type = TPOPT_DISC_REASON;
			x.dr_reason = error;
			*mtod(m, struct tp_disc_reason *) = x;
			sbappendrecord(&tpcb->tp_Xrcv, m);
			error = 0;
		} else
			error = ECONNRESET;
	}
	so->so_error = error;

	if (ind == T_DISCONNECT) {
		if (error == 0)
			so->so_error = ENOTCONN;
		if (tpcb->tp_no_disc_indications)
			return;
	}
#ifdef TPPT
	if (tp_traceflags[D_INDICATION]) {
		tptraceTPCB(TPPTmisc, "doing sohasoutofband(so)", so, 0, 0, 0);
	}
#endif
	sohasoutofband(so);
}

/*
 * NAME : tp_getoptions()
 *
 * CALLED FROM:
 * 	tp.trans whenever we go into OPEN state
 *
 * FUNCTION and ARGUMENTS:
 *  sets the proper flags and values in the tpcb, to control
 *  the appropriate actions for the given class, options,
 *  sequence space, etc, etc.
 *
 * RETURNS: Nada
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
tp_getoptions(struct tp_pcb *tpcb)
{
	tpcb->tp_seqmask =
		tpcb->tp_xtd_format ? TP_XTD_FMT_MASK : TP_NML_FMT_MASK;
	tpcb->tp_seqbit =
		tpcb->tp_xtd_format ? TP_XTD_FMT_BIT : TP_NML_FMT_BIT;
	tpcb->tp_seqhalf = tpcb->tp_seqbit >> 1;
	tpcb->tp_dt_ticks =
		max(tpcb->tp_dt_ticks, (tpcb->tp_peer_acktime + 2));
	tp_rsyset(tpcb);

}

/*
 * NAME:  tp_recycle_tsuffix()
 *
 * CALLED FROM:
 *  Called when a ref is frozen.
 *
 * FUNCTION and ARGUMENTS:
 *  allows the suffix to be reused.
 *
 * RETURNS: zilch
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
tp_recycle_tsuffix(void *v)
{
	struct tp_pcb  *tpcb = v;
	bzero((void *) tpcb->tp_lsuffix, sizeof(tpcb->tp_lsuffix));
	bzero((void *) tpcb->tp_fsuffix, sizeof(tpcb->tp_fsuffix));
	tpcb->tp_fsuffixlen = tpcb->tp_lsuffixlen = 0;

	(tpcb->tp_nlproto->nlp_recycle_suffix) (tpcb->tp_npcb);
}

/*
 * NAME: tp_quench()
 *
 * CALLED FROM:
 *  tp{af}_quench() when ICMP source quench or similar thing arrives.
 *
 * FUNCTION and ARGUMENTS:
 *  Drop the congestion window back to 1.
 *  Congestion window scheme:
 *  Initial value is 1.  ("slow start" as Nagle, et. al. call it)
 *  For each good ack that arrives, the congestion window is increased
 *  by 1 (up to max size of logical infinity, which is to say,
 *	it doesn't wrap around).
 *  Source quench causes it to drop back to 1.
 *  tp_send() uses the smaller of (regular window, congestion window).
 *  One retransmission strategy option is to have any retransmission
 *	cause reset the congestion window back  to 1.
 *
 *	(cmd) is either PRC_QUENCH: source quench, or
 *		PRC_QUENCH2: dest. quench (dec bit)
 *
 * RETURNS:
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
tp_quench(struct inpcb  *ipcb, int cmd)
{
	struct tp_pcb  *tpcb = (struct tp_pcb *) ipcb;
#ifdef ARGO_DEBUG
	if (argo_debug[D_QUENCH]) {
		printf("tp_quench tpcb %p ref 0x%x sufx 0x%x\n",
		       tpcb, tpcb->tp_lref, *(u_short *) (tpcb->tp_lsuffix));
		printf("cong_win 0x%lx decbit 0x%x \n",
		       tpcb->tp_cong_win, tpcb->tp_decbit);
	}
#endif
	switch (cmd) {
	case PRC_QUENCH:
		tpcb->tp_cong_win = tpcb->tp_l_tpdusize;
		IncStat(ts_quench);
		break;
	case PRC_QUENCH2:
		/* might as well quench source also */
		tpcb->tp_cong_win = tpcb->tp_l_tpdusize;
		tpcb->tp_decbit = TP_DECBIT_CLEAR_COUNT;
		IncStat(ts_rcvdecbit);
		break;
	}
}


/*
 * NAME:	tp_netcmd()
 *
 * CALLED FROM:
 *
 * FUNCTION and ARGUMENTS:
 *
 * RETURNS:
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
tp_netcmd(struct tp_pcb *tpcb, int cmd)
{
#ifdef TPCONS
	struct isopcb  *isop;
	struct pklcd   *lcp;

	if (tpcb->tp_netservice != ISO_CONS)
		return;
	isop = (struct isopcb *) tpcb->tp_npcb;
	lcp = (struct pklcd *) isop->isop_chan;
	switch (cmd) {

	case CONN_CLOSE:
	case CONN_REFUSE:
		if (isop->isop_refcnt == 1) {
			/*
			 * This is really superfluous, since it would happen
			 * anyway in iso_pcbdetach, although it is a courtesy
			 * to free up the x.25 channel before the refwait
			 * timer expires.
			 */
			lcp->lcd_upper = 0;
			lcp->lcd_upnext = 0;
			pk_disconnect(lcp);
			isop->isop_chan = 0;
			isop->isop_refcnt = 0;
		}
		break;

	default:
		printf("tp_netcmd(%p, %#x) NOT IMPLEMENTED\n", tpcb, cmd);
		break;
	}
#else				/* TPCONS */
	printf("tp_netcmd(): X25 NOT CONFIGURED!!\n");
#endif
}

/*
 * CALLED FROM:
 *  tp_ctloutput() and tp_emit()
 * FUNCTION and ARGUMENTS:
 * 	Convert a class mask to the highest numeric value it represents.
 */
int
tp_mask_to_num(u_char x)
{
	int    j;

	for (j = 4; j >= 0; j--) {
		if (x & (1 << j))
			break;
	}
	ASSERT((j == 4) || (j == 0));	/* for now */
	if ((j != 4) && (j != 0)) {
		printf("ASSERTION ERROR: tp_mask_to_num: x 0x%x j %d\n",
		       x, j);
	}
#ifdef TPPT
	if (tp_traceflags[D_TPINPUT]) {
		tptrace(TPPTmisc, "tp_mask_to_num(x) returns j", x, j, 0, 0);
	}
#endif
#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		printf("tp_mask_to_num(0x%x) returns 0x%x\n", x, j);
	}
#endif
	return j;
}

#if 0
static void
copyQOSparms(const struct tp_conn_param *src, struct tp_conn_params *dst)
{
	/* copy all but the bits stuff at the end */
#define COPYSIZE (12 * sizeof(short))

	bcopy((void *) src, (void *) dst, COPYSIZE);
	dst->p_tpdusize = src->p_tpdusize;
	dst->p_ack_strat = src->p_ack_strat;
	dst->p_rx_strat = src->p_rx_strat;
#undef COPYSIZE
}
#endif

/*
 * Determine a reasonable value for maxseg size.
 * If the route is known, check route for mtu.
 * We also initialize the congestion/slow start
 * window to be a single segment if the destination isn't local.
 * While looking at the routing entry, we also initialize other path-dependent
 * parameters from pre-set or cached values in the routing entry.
 */
void
tp_mss(struct tp_pcb *tpcb, int nhdr_size)
{
	struct rtentry *rt;
	struct ifnet   *ifp;
	int    rtt, mss;
	u_long          bufsize;
	int             i, ssthresh = 0, rt_mss;
	struct socket  *so;

	if (tpcb->tp_ptpdusize)
		mss = tpcb->tp_ptpdusize << 7;
	else
		mss = 1 << tpcb->tp_tpdusize;
	so = tpcb->tp_sock;
	if ((rt = rtcache_getrt(tpcb->tp_routep)) == NULL) {
		bufsize = so->so_rcv.sb_hiwat;
		goto punt_route;
	}
	ifp = rt->rt_ifp;

#ifdef RTV_MTU			/* if route characteristics exist ... */
	/*
	 * While we're here, check if there's an initial rtt
	 * or rttvar.  Convert from the route-table units
	 * to hz ticks for the smoothed timers and slow-timeout units
	 * for other inital variables.
	 */
	if (tpcb->tp_rtt == 0 && (rtt = rt->rt_rmx.rmx_rtt)) {
		tpcb->tp_rtt = rtt * hz / RTM_RTTUNIT;
		if (rt->rt_rmx.rmx_rttvar)
			tpcb->tp_rtv = rt->rt_rmx.rmx_rttvar
				* hz / RTM_RTTUNIT;
		else
			tpcb->tp_rtv = tpcb->tp_rtt;
	}
	/*
	 * if there's an mtu associated with the route, use it
	 */
	if (rt->rt_rmx.rmx_mtu)
		rt_mss = rt->rt_rmx.rmx_mtu - nhdr_size;
	else
#endif				/* RTV_MTU */
		rt_mss = (ifp->if_mtu - nhdr_size);
	if (tpcb->tp_ptpdusize == 0 ||	/* assume application doesn't care */
	    mss > rt_mss /* network won't support what was asked for */ )
		mss = rt_mss;
	/* can propose mtu which are multiples of 128 */
	mss &= ~0x7f;
	/*
	 * If there's a pipesize, change the socket buffer
	 * to that size.
	 */
#ifdef RTV_SPIPE
	if ((bufsize = rt->rt_rmx.rmx_sendpipe) > 0) {
#endif
		bufsize = min(bufsize, so->so_snd.sb_hiwat);
		(void) sbreserve(&so->so_snd, bufsize, so);
	}
#ifdef RTV_SPIPE
	if ((bufsize = rt->rt_rmx.rmx_recvpipe) > 0) {
#endif
		bufsize = min(bufsize, so->so_rcv.sb_hiwat);
		(void) sbreserve(&so->so_rcv, bufsize, so);
	} else
		bufsize = so->so_rcv.sb_hiwat;
#ifdef RTV_SSTHRESH
	/*
	 * There's some sort of gateway or interface
	 * buffer limit on the path.  Use this to set
	 * the slow start threshhold, but set the
	 * threshold to no less than 2*mss.
	 */
	ssthresh = rt->rt_rmx.rmx_ssthresh;
punt_route:
	/*
	 * The current mss is initialized to the default value.
	 * If we compute a smaller value, reduce the current mss.
	 * If we compute a larger value, return it for use in sending
	 * a max seg size option.
	 * If we received an offer, don't exceed it.
	 * However, do not accept offers under 128 bytes.
	 */
	if (tpcb->tp_l_tpdusize)
		mss = min(mss, tpcb->tp_l_tpdusize);
	/*
	 * We want a minimum recv window of 4 packets to
	 * signal packet loss by duplicate acks.
	 */
	mss = min(mss, bufsize >> 2) & ~0x7f;
	mss = max(mss, 128);	/* sanity */
	tpcb->tp_cong_win =
		(rt == 0 || (rt->rt_flags & RTF_GATEWAY)) ? mss : bufsize;
	tpcb->tp_l_tpdusize = mss;
	tp_rsyset(tpcb);
	tpcb->tp_ssthresh = max(2 * mss, ssthresh);
	/* Calculate log2 of mss */
	for (i = TP_MIN_TPDUSIZE + 1; i <= TP_MAX_TPDUSIZE; i++)
		if ((1 << i) > mss)
			break;
	i--;
	tpcb->tp_tpdusize = i;
#endif	/* RTV_MTU */
}

/*
 * CALLED FROM:
 *  tp_usrreq on PRU_CONNECT and tp_input on receipt of CR
 *
 * FUNCTION and ARGUMENTS:
 * 	-- An mbuf containing the peer's network address.
 *  -- Our control block, which will be modified
 *  -- In the case of cons, a control block for that layer.
 *
 *
 * RETURNS:
 *	errno value	 :
 *	EAFNOSUPPORT if can't find an nl_protosw for x.25 (really could panic)
 *	ECONNREFUSED if trying to run TP0 with non-type 37 address
 *  possibly other E* returned from cons_netcmd()
 *
 * SIDE EFFECTS:
 *   Determines recommended tpdusize, buffering and intial delays
 *	 based on information cached on the route.
 */
int
tp_route_to(struct mbuf *m, struct tp_pcb *tpcb, void *channel)
{
	struct sockaddr_iso *siso;	/* NOTE: this may be a
						 * sockaddr_in */
	int             error = 0, save_netservice = tpcb->tp_netservice;
	struct rtentry *rt = 0;
	int             nhdr_size;

	siso = mtod(m, struct sockaddr_iso *);
#ifdef TPPT
	if (tp_traceflags[D_CONN]) {
		tptraceTPCB(TPPTmisc,
			    "route_to: so  afi netservice class",
	tpcb->tp_sock, siso->siso_addr.isoa_genaddr[0], tpcb->tp_netservice,
			    tpcb->tp_class);
	}
#endif
#ifdef ARGO_DEBUG
		if (argo_debug[D_CONN]) {
		printf("tp_route_to( m %p, channel %p, tpcb %p netserv 0x%x)\n",
		       m, channel, tpcb, tpcb->tp_netservice);
		printf("m->mlen x%x, m->m_data:\n", m->m_len);
		dump_buf(mtod(m, void *), m->m_len);
	}
#endif
	if (channel) {
#ifdef TPCONS
		struct pklcd   *lcp = (struct pklcd *) channel;
		struct isopcb  *isop = (struct isopcb *) lcp->lcd_upnext,
		               *isop_new = (struct isopcb *) tpcb->tp_npcb;
		/*
		 * The next 2 lines believe that you haven't set any network
		 * level options or done a pcbconnect and XXXXXXX'edly apply
		 * to both inpcb's and isopcb's
		 */
		iso_remque(isop_new);
		free(isop_new, M_PCB);
		tpcb->tp_npcb = (void *) isop;
		tpcb->tp_netservice = ISO_CONS;
		tpcb->tp_nlproto = nl_protosw + ISO_CONS;
		if (isop->isop_refcnt++ == 0) {
			iso_putsufx(isop, tpcb->tp_lsuffix,
				    tpcb->tp_lsuffixlen, TP_LOCAL);
			isop->isop_socket = tpcb->tp_sock;
		}
#endif
	} else {
		switch (siso->siso_family) {
		default:
			error = EAFNOSUPPORT;
			goto done;
#ifdef ISO
		case AF_ISO:
			{
				struct isopcb  *isop = (struct isopcb *) tpcb->tp_npcb;
				int             flags = tpcb->tp_sock->so_options & SO_DONTROUTE;
				tpcb->tp_netservice = ISO_CLNS;
				if (clnp_route(&siso->siso_addr, &isop->isop_route,
				    flags, NULL, NULL) == 0) {
					rt = rtcache_getrt(&isop->isop_route);
					if (rt && rt->rt_flags & RTF_PROTO1)
						tpcb->tp_netservice = ISO_CONS;
				}
			} break;
#endif
#ifdef INET
		case AF_INET:
			tpcb->tp_netservice = IN_CLNS;
#endif
		}
		if (tpcb->tp_nlproto->nlp_afamily != siso->siso_family) {
#ifdef ARGO_DEBUG
			if (argo_debug[D_CONN]) {
				printf("tp_route_to( CHANGING nlproto old 0x%x new 0x%x)\n",
				       save_netservice, tpcb->tp_netservice);
			}
#endif
			if ((error = tp_set_npcb(tpcb)) != 0)
				goto done;
		}
#ifdef ARGO_DEBUG
		if (argo_debug[D_CONN]) {
			printf("tp_route_to  calling nlp_pcbconn, netserv %d\n",
			       tpcb->tp_netservice);
		}
#endif
		tpcb->tp_nlproto = nl_protosw + tpcb->tp_netservice;
		error = (*tpcb->tp_nlproto->nlp_pcbconn) (tpcb->tp_npcb, m, NULL);
	}
	if (error)
		goto done;
	/* only gets common info */
	nhdr_size = (*tpcb->tp_nlproto->nlp_mtu)(tpcb);
	tp_mss(tpcb, nhdr_size);
done:
#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("tp_route_to  returns 0x%x\n", error);
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_CONN]) {
		tptraceTPCB(TPPTmisc, "route_to: returns: error netserv class", error,
			    tpcb->tp_netservice, tpcb->tp_class, 0);
	}
#endif
	return error;
}

/* class zero version */
void
tp0_stash(struct tp_pcb *tpcb, struct tp_event *e)
{
#define E e->TPDU_ATTR(DT)

	struct sockbuf *sb = &tpcb->tp_sock->so_rcv;

#ifdef TP_PERF_MEAS
	if (DOPERF(tpcb)) {
		PStat(tpcb, Nb_from_ll) += E.e_datalen;
	tpmeas(tpcb->tp_lref, TPtime_from_ll, &e->e_time,
	       E.e_seq, PStat(tpcb, Nb_from_ll), E.e_datalen);
	}
#endif

#ifdef ARGO_DEBUG
		if (argo_debug[D_STASH]) {
		printf("stash EQ: seq 0x%x datalen 0x%x eot 0x%x",
		       E.e_seq, E.e_datalen, E.e_eot);
	}
#endif

#ifdef TPPT
	if (tp_traceflags[D_STASH]) {
		tptraceTPCB(TPPTmisc, "stash EQ: seq len eot",
			    E.e_seq, E.e_datalen, E.e_eot, 0);
	}
#endif

	if (E.e_eot) {
		struct mbuf *n = E.e_data;
		n->m_flags |= M_EOR;
		n->m_nextpkt = NULL;	/* set on tp_input */
	}
	sbappend(sb, E.e_data);
#ifdef ARGO_DEBUG
	if (argo_debug[D_STASH]) {
		dump_mbuf(sb->sb_mb, "stash 0: so_rcv after appending");
	}
#endif
	if (tpcb->tp_netservice != ISO_CONS)
		printf("tp0_stash: tp running over something weird\n");
}

void
tp0_openflow(struct tp_pcb *tpcb)
{
	if (tpcb->tp_netservice != ISO_CONS)
		printf("tp0_openflow: tp running over something weird\n");
}

#ifdef TP_PERF_MEAS
/*
 * CALLED FROM:
 *  tp_ctloutput() when the user sets TPOPT_PERF_MEAS on
 *  and tp_newsocket() when a new connection is made from
 *  a listening socket with tp_perf_on == true.
 * FUNCTION and ARGUMENTS:
 *  (tpcb) is the usual; this procedure gets a clear cluster mbuf for
 *  a tp_pmeas structure, and makes tpcb->tp_p_meas point to it.
 * RETURN VALUE:
 *  ENOBUFS if it cannot get a cluster mbuf.
 */

int
tp_setup_perf(struct tp_pcb *tpcb)
{
	if (tpcb->tp_p_meas == 0) {
		tpcb->tp_p_meas = malloc(sizeof(struct tp_pmeas), M_PCB, M_WAITOK|M_ZERO);
#ifdef ARGO_DEBUG
		if (argo_debug[D_PERF_MEAS]) {
			printf(
			       "tpcb 0x%x so 0x%x ref 0x%x tp_p_meas 0x%x tp_perf_on 0x%x\n",
			       tpcb, tpcb->tp_sock, tpcb->tp_lref,
			       tpcb->tp_p_meas, tpcb->tp_perf_on);
		}
#endif
		tpcb->tp_perf_on = 1;
	}
	return 0;
}
#endif				/* TP_PERF_MEAS */

#ifdef ARGO_DEBUG
void
dump_addr(struct sockaddr *addr)
{
	switch (addr->sa_family) {
#ifdef INET
	case AF_INET:
		dump_inaddr(satosin(addr));
		break;
#endif
#ifdef ISO
	case AF_ISO:
		dump_isoaddr(satosiso(addr));
		break;
#endif				/* ISO */
	default:
		printf("BAD AF: 0x%x\n", addr->sa_family);
		break;
	}
}

#define	MAX_COLUMNS	8
/*
 *	Dump the buffer to the screen in a readable format. Format is:
 *
 *		hex/dec  where hex is the hex format, dec is the decimal format.
 *		columns of hex/dec numbers will be printed, followed by the
 *		character representations (if printable).
 */
void
Dump_buf(const void *buf, size_t len)
{
	int             i, j;
#define Buf ((const u_char *)buf)
	printf("Dump buf %p len 0x%lx\n", buf, (unsigned long)len);
	for (i = 0; i < len; i += MAX_COLUMNS) {
		printf("+%d:\t", i);
		for (j = 0; j < MAX_COLUMNS; j++) {
			if (i + j < len) {
				printf("%x/%d\t", Buf[i + j], Buf[i + j]);
			} else {
				printf("	");
			}
		}

		for (j = 0; j < MAX_COLUMNS; j++) {
			if (i + j < len) {
				if (((Buf[i + j]) > 31) && ((Buf[i + j]) < 128))
					printf("%c", Buf[i + j]);
				else
					printf(".");
			}
		}
		printf("\n");
	}
}
#endif /* ARGO_DEBUG */
