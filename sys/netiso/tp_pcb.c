/*	$NetBSD: tp_pcb.c,v 1.13 1996/03/16 23:13:58 christos Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)tp_pcb.c	8.1 (Berkeley) 6/10/93
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
 * This is the initialization and cleanup stuff - for the tp machine in
 * general as well as  for the individual pcbs. tp_init() is called at system
 * startup.  tp_attach() and tp_getref() are called when a socket is created.
 * tp_detach() and tp_freeref() are called during the closing stage and/or
 * when the reference timer goes off. tp_soisdisconnecting() and
 * tp_soisdisconnected() are tp-specific versions of soisconnect* and are
 * called (obviously) during the closing phase.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>

#include <netiso/argo_debug.h>
#include <netiso/tp_param.h>
#include <netiso/tp_timer.h>
#include <netiso/tp_ip.h>
#include <netiso/tp_stat.h>
#include <netiso/tp_pcb.h>
#include <netiso/tp_tpdu.h>
#include <netiso/tp_trace.h>
#include <netiso/tp_meas.h>
#include <netiso/tp_seq.h>
#include <netiso/tp_clnp.h>
#include <netiso/tp_var.h>

/*
 * ticks are in units of: 500 nano-fortnights ;-) or 500 ms or 1/2 second
 */

struct tp_conn_param tp_conn_param[] = {
	/* ISO_CLNS: TP4 CONNECTION LESS */
	{
		TP_NRETRANS,	/* short p_Nretrans;  */
		20,		/* 10 sec *//* short p_dr_ticks;  */

		20,		/* 10 sec *//* short p_cc_ticks; */
		20,		/* 10 sec *//* short p_dt_ticks; */

		40,		/* 20 sec *//* short p_x_ticks;	 */
		80,		/* 40 sec *//* short p_cr_ticks; */

		240,		/* 2 min *//* short p_keepalive_ticks; */
		10,		/* 5 sec *//* short p_sendack_ticks;  */

		600,		/* 5 min *//* short p_ref_ticks;	 */
		360,		/* 3 min *//* short p_inact_ticks;	 */

		(short) 100,	/* short p_lcdtfract */
		(short) TP_SOCKBUFSIZE,	/* short p_winsize */
		TP_TPDUSIZE,	/* u_char p_tpdusize */

		TPACK_WINDOW,	/* 4 bits p_ack_strat */
		TPRX_USE_CW | TPRX_FASTSTART,
		/* 4 bits p_rx_strat */
		TP_CLASS_4 | TP_CLASS_0,	/* 5 bits p_class */
		1,		/* 1 bit xtd format */
		1,		/* 1 bit xpd service */
		1,		/* 1 bit use_checksum */
		0,		/* 1 bit use net xpd */
		0,		/* 1 bit use rcc */
		0,		/* 1 bit use efc */
		1,		/* no disc indications */
		0,		/* don't change params */
		ISO_CLNS,	/* p_netservice */
	},
	/* IN_CLNS: TP4 CONNECTION LESS */
	{
		TP_NRETRANS,	/* short p_Nretrans;  */
		20,		/* 10 sec *//* short p_dr_ticks;  */

		20,		/* 10 sec *//* short p_cc_ticks; */
		20,		/* 10 sec *//* short p_dt_ticks; */

		40,		/* 20 sec *//* short p_x_ticks;	 */
		80,		/* 40 sec *//* short p_cr_ticks; */

		240,		/* 2 min *//* short p_keepalive_ticks; */
		10,		/* 5 sec *//* short p_sendack_ticks;  */

		600,		/* 5 min *//* short p_ref_ticks;	 */
		360,		/* 3 min *//* short p_inact_ticks;	 */

		(short) 100,	/* short p_lcdtfract */
		(short) TP_SOCKBUFSIZE,	/* short p_winsize */
		TP_TPDUSIZE,	/* u_char p_tpdusize */

		TPACK_WINDOW,	/* 4 bits p_ack_strat */
		TPRX_USE_CW | TPRX_FASTSTART,
		/* 4 bits p_rx_strat */
		TP_CLASS_4,	/* 5 bits p_class */
		1,		/* 1 bit xtd format */
		1,		/* 1 bit xpd service */
		1,		/* 1 bit use_checksum */
		0,		/* 1 bit use net xpd */
		0,		/* 1 bit use rcc */
		0,		/* 1 bit use efc */
		1,		/* no disc indications */
		0,		/* don't change params */
		IN_CLNS,	/* p_netservice */
	},
	/* ISO_CONS: TP0 CONNECTION MODE */
	{
		TP_NRETRANS,	/* short p_Nretrans;  */
		0,		/* n/a *//* short p_dr_ticks; */

		40,		/* 20 sec *//* short p_cc_ticks; */
		0,		/* n/a *//* short p_dt_ticks; */

		0,		/* n/a *//* short p_x_ticks;	 */
		360,		/* 3  min *//* short p_cr_ticks; */

		0,		/* n/a *//* short p_keepalive_ticks; */
		0,		/* n/a *//* short p_sendack_ticks; */

		600,		/* for cr/cc to clear *//* short p_ref_ticks;	 */
		0,		/* n/a *//* short p_inact_ticks;	 */

		/*
		 * Use tp4 defaults just in case the user changes ONLY the
		 * class
		 */
		(short) 100,	/* short p_lcdtfract */
		(short) TP0_SOCKBUFSIZE,	/* short p_winsize */
		TP0_TPDUSIZE,	/* 8 bits p_tpdusize */

		0,		/* 4 bits p_ack_strat */
		0,		/* 4 bits p_rx_strat */
		TP_CLASS_0,	/* 5 bits p_class */
		0,		/* 1 bit xtd format */
		0,		/* 1 bit xpd service */
		0,		/* 1 bit use_checksum */
		0,		/* 1 bit use net xpd */
		0,		/* 1 bit use rcc */
		0,		/* 1 bit use efc */
		0,		/* no disc indications */
		0,		/* don't change params */
		ISO_CONS,	/* p_netservice */
	},
	/* ISO_COSNS: TP4 CONNECTION LESS SERVICE over CONSNS */
	{
		TP_NRETRANS,	/* short p_Nretrans;  */
		40,		/* 20 sec *//* short p_dr_ticks;  */

		40,		/* 20 sec *//* short p_cc_ticks; */
		80,		/* 40 sec *//* short p_dt_ticks; */

		120,		/* 1 min *//* short p_x_ticks;	 */
		360,		/* 3 min *//* short p_cr_ticks; */

		360,		/* 3 min *//* short p_keepalive_ticks; */
		20,		/* 10 sec *//* short p_sendack_ticks;  */

		600,		/* 5 min *//* short p_ref_ticks;	 */
		480,		/* 4 min *//* short p_inact_ticks;	 */

		(short) 100,	/* short p_lcdtfract */
		(short) TP0_SOCKBUFSIZE,	/* short p_winsize */
		TP0_TPDUSIZE,	/* u_char p_tpdusize */

		TPACK_WINDOW,	/* 4 bits p_ack_strat */
		TPRX_USE_CW,	/* No fast start */
		/* 4 bits p_rx_strat */
		TP_CLASS_4 | TP_CLASS_0,	/* 5 bits p_class */
		0,		/* 1 bit xtd format */
		1,		/* 1 bit xpd service */
		1,		/* 1 bit use_checksum */
		0,		/* 1 bit use net xpd */
		0,		/* 1 bit use rcc */
		0,		/* 1 bit use efc */
		0,		/* no disc indications */
		0,		/* don't change params */
		ISO_COSNS,	/* p_netservice */
	},
};

#ifdef INET
struct inpcbtable tp_inpcb;
#endif				/* INET */
#ifdef ISO
struct isopcb   tp_isopcb;
#endif				/* ISO */
#ifdef TPCONS
struct isopcb   tp_isopcb;
#endif				/* TPCONS */


struct nl_protosw nl_protosw[] = {
	/* ISO_CLNS */
#ifdef ISO
	{AF_ISO, iso_putnetaddr, iso_getnetaddr, iso_cmpnetaddr,
		iso_putsufx, iso_getsufx,
		iso_recycle_tsuffix,
		tpclnp_mtu, iso_pcbbind, iso_pcbconnect,
		iso_pcbdisconnect, iso_pcbdetach,
		iso_pcballoc,
		tpclnp_output, tpclnp_output_dg, iso_nlctloutput,
		(caddr_t) & tp_isopcb,
	},
#else
	{0},
#endif				/* ISO */
	/* IN_CLNS */
#ifdef INET
	{AF_INET, in_putnetaddr, in_getnetaddr, in_cmpnetaddr,
		in_putsufx, in_getsufx,
		in_recycle_tsuffix,
		tpip_mtu, in_pcbbind, in_pcbconnect,
		in_pcbdisconnect, in_pcbdetach,
		in_pcballoc,
		tpip_output, tpip_output_dg, /* nl_ctloutput */ NULL,
		(caddr_t) & tp_inpcb,
	},
#else
	{0},
#endif				/* INET */
	/* ISO_CONS */
#if defined(ISO) && defined(TPCONS)
	{AF_ISO, iso_putnetaddr, iso_getnetaddr, iso_cmpnetaddr,
		iso_putsufx, iso_getsufx,
		iso_recycle_tsuffix,
		tpclnp_mtu, iso_pcbbind, tpcons_pcbconnect,
		iso_pcbdisconnect, iso_pcbdetach,
		iso_pcballoc,
		tpcons_output, tpcons_output, iso_nlctloutput,
		(caddr_t) & tp_isopcb,
	},
#else
	{0},
#endif				/* ISO_CONS */
	/* End of protosw marker */
	{0}
};

u_long          tp_sendspace = 1024 * 4;
u_long          tp_recvspace = 1024 * 4;

/*
 * NAME:  tp_init()
 *
 * CALLED FROM:
 *  autoconf through the protosw structure
 *
 * FUNCTION:
 *  initialize tp machine
 *
 * RETURNS:  Nada
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
tp_init()
{
	static int      init_done = 0;

	if (init_done++)
		return;

	/* FOR INET */
	in_pcbinit(&tp_inpcb, 1);
	/* FOR ISO */
	tp_isopcb.isop_next = tp_isopcb.isop_prev = &tp_isopcb;

	tp_start_win = 2;

	tp_timerinit();
	bzero((caddr_t) & tp_stat, sizeof(struct tp_stat));
}

/*
 * NAME: 	tp_soisdisconnecting()
 *
 * CALLED FROM:
 *  tp.trans
 *
 * FUNCTION and ARGUMENTS:
 *  Set state of the socket (so) to reflect that fact that we're disconnectING
 *
 * RETURNS: 	Nada
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 *  This differs from the regular soisdisconnecting() in that the latter
 *  also sets the SS_CANTRECVMORE and SS_CANTSENDMORE flags.
 *  We don't want to set those flags because those flags will cause
 *  a SIGPIPE to be delivered in sosend() and we don't like that.
 *  If anyone else is sleeping on this socket, wake 'em up.
 */
void
tp_soisdisconnecting(so)
	register struct socket *so;
{
	soisdisconnecting(so);
	so->so_state &= ~SS_CANTSENDMORE;
#ifdef TP_PERF_MEAS
	if (DOPERF(sototpcb(so))) {
		register struct tp_pcb *tpcb = sototpcb(so);
		u_int           fsufx, lsufx;

		bcopy((caddr_t) tpcb->tp_fsuffix, (caddr_t) &fsufx,
		      sizeof(u_int));
		bcopy((caddr_t) tpcb->tp_lsuffix, (caddr_t) &lsufx,
		      sizeof(u_int));

		tpmeas(tpcb->tp_lref, TPtime_close, &time, fsufx, lsufx,
		       tpcb->tp_fref);
		tpcb->tp_perf_on = 0;	/* turn perf off */
	}
#endif
}


/*
 * NAME: tp_soisdisconnected()
 *
 * CALLED FROM:
 *	tp.trans
 *
 * FUNCTION and ARGUMENTS:
 *  Set state of the socket (so) to reflect that fact that we're disconnectED
 *  Set the state of the reference structure to closed, and
 *  recycle the suffix.
 *  Start a reference timer.
 *
 * RETURNS:	Nada
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 *  This differs from the regular soisdisconnected() in that the latter
 *  also sets the SS_CANTRECVMORE and SS_CANTSENDMORE flags.
 *  We don't want to set those flags because those flags will cause
 *  a SIGPIPE to be delivered in sosend() and we don't like that.
 *  If anyone else is sleeping on this socket, wake 'em up.
 */
void
tp_soisdisconnected(tpcb)
	register struct tp_pcb *tpcb;
{
	register struct socket *so = tpcb->tp_sock;

	soisdisconnecting(so);
	so->so_state &= ~SS_CANTSENDMORE;
#ifdef TP_PERF_MEAS
	if (DOPERF(tpcb)) {
		register struct tp_pcb *ttpcb = sototpcb(so);
		u_int           fsufx, lsufx;

		/* CHOKE */
		bcopy((caddr_t) ttpcb->tp_fsuffix, (caddr_t) &fsufx,
		      sizeof(u_int));
		bcopy((caddr_t) ttpcb->tp_lsuffix, (caddr_t) &lsufx,
		      sizeof(u_int));

		tpmeas(ttpcb->tp_lref, TPtime_close,
		       &time, &lsufx, &fsufx, ttpcb->tp_fref);
		tpcb->tp_perf_on = 0;	/* turn perf off */
	}
#endif

	tpcb->tp_refstate = REF_FROZEN;
	tp_recycle_tsuffix(tpcb);
	tp_etimeout(tpcb, TM_reference, (int) tpcb->tp_refer_ticks);
}

/*
 * NAME:	tp_freeref()
 *
 * CALLED FROM:
 *  tp.trans when the reference timer goes off, and
 *  from tp_attach() and tp_detach() when a tpcb is partially set up but not
 *  set up enough to have a ref timer set for it, and it's discarded
 *  due to some sort of error or an early close()
 *
 * FUNCTION and ARGUMENTS:
 *  Frees the reference represented by (r) for re-use.
 *
 * RETURNS: Nothing
 *
 * SIDE EFFECTS:
 *
 * NOTES:	better be called at clock priority !!!!!
 */
void
tp_freeref(n)
	RefNum          n;
{
	register struct tp_ref *r = tp_ref + n;
	register struct tp_pcb *tpcb;

	tpcb = r->tpr_pcb;
#ifdef ARGO_DEBUG
	if (argo_debug[D_TIMER]) {
		printf("tp_freeref called for ref %d pcb %p maxrefopen %d\n",
		       n, tpcb, tp_refinfo.tpr_maxopen);
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_TIMER]) {
		tptrace(TPPTmisc, "tp_freeref ref maxrefopen pcb",
			n, tp_refinfo.tpr_maxopen, tpcb, 0);
	}
#endif
	if (tpcb == 0)
		return;
#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("tp_freeref: CLEARING tpr_pcb %p\n", tpcb);
	}
#endif
	r->tpr_pcb = (struct tp_pcb *) 0;
	tpcb->tp_refstate = REF_FREE;

	for (r = tp_ref + tp_refinfo.tpr_maxopen; r > tp_ref; r--)
		if (r->tpr_pcb)
			break;
	tp_refinfo.tpr_maxopen = r - tp_ref;
	tp_refinfo.tpr_numopen--;

#ifdef ARGO_DEBUG
	if (argo_debug[D_TIMER]) {
		printf("tp_freeref ends w/ maxrefopen %d\n", tp_refinfo.tpr_maxopen);
	}
#endif
}

/*
 * NAME:  tp_getref()
 *
 * CALLED FROM:
 *  tp_attach()
 *
 * FUNCTION and ARGUMENTS:
 *  obtains the next free reference and allocates the appropriate
 *  ref structure, links that structure to (tpcb)
 *
 * RETURN VALUE:
 *	a reference number
 *  or TP_ENOREF
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
u_long
tp_getref(tpcb)
	register struct tp_pcb *tpcb;
{
	register struct tp_ref *r, *rlim;
	register int    i;
	caddr_t         obase;
	unsigned        size;

	if (++tp_refinfo.tpr_numopen < tp_refinfo.tpr_size)
		for (r = tp_refinfo.tpr_base, rlim = r + tp_refinfo.tpr_size;
		     ++r < rlim;)	/* tp_ref[0] is never used */
			if (r->tpr_pcb == 0)
				goto got_one;
	/* else have to allocate more space */

	obase = (caddr_t) tp_refinfo.tpr_base;
	size = tp_refinfo.tpr_size * sizeof(struct tp_ref);
	r = (struct tp_ref *) malloc(size + size, M_PCB, M_NOWAIT);
	if (r == 0)
		return (--tp_refinfo.tpr_numopen, TP_ENOREF);
	tp_refinfo.tpr_base = tp_ref = r;
	tp_refinfo.tpr_size *= 2;
	bcopy(obase, (caddr_t) r, size);
	free(obase, M_PCB);
	r = (struct tp_ref *) (size + (caddr_t) r);
	bzero((caddr_t) r, size);

got_one:
	r->tpr_pcb = tpcb;
	tpcb->tp_refstate = REF_OPENING;
	i = r - tp_refinfo.tpr_base;
	if (tp_refinfo.tpr_maxopen < i)
		tp_refinfo.tpr_maxopen = i;
	return (u_long) i;
}

/*
 * NAME: tp_set_npcb()
 *
 * CALLED FROM:
 *	tp_attach(), tp_route_to()
 *
 * FUNCTION and ARGUMENTS:
 *  given a tpcb, allocate an appropriate lower-lever npcb, freeing
 *  any old ones that might need re-assigning.
 */
int
tp_set_npcb(tpcb)
	register struct tp_pcb *tpcb;
{
	register struct socket *so = tpcb->tp_sock;
	int             error;

	if (tpcb->tp_nlproto && tpcb->tp_npcb) {
		short           so_state = so->so_state;
		so->so_state &= ~SS_NOFDREF;
		(*tpcb->tp_nlproto->nlp_pcbdetach)(tpcb->tp_npcb);
		so->so_state = so_state;
	}
	tpcb->tp_nlproto = &nl_protosw[tpcb->tp_netservice];
	/* xx_pcballoc sets so_pcb */
	error = (*tpcb->tp_nlproto->nlp_pcballoc)(so,
				    tpcb->tp_nlproto->nlp_pcblist);
	tpcb->tp_npcb = so->so_pcb;
	so->so_pcb = tpcb;
	return (error);
}
/*
 * NAME: tp_attach()
 *
 * CALLED FROM:
 *	tp_usrreq, PRU_ATTACH
 *
 * FUNCTION and ARGUMENTS:
 *  given a socket (so) and a protocol family (dom), allocate a tpcb
 *  and ref structure, initialize everything in the structures that
 *  needs to be initialized.
 *
 * RETURN VALUE:
 *  0 ok
 *  EINVAL if DEBUG(X) in is on and a disaster has occurred
 *  ENOPROTOOPT if TP hasn't been configured or if the
 *   socket wasn't created with tp as its protocol
 *  EISCONN if this socket is already part of a connection
 *  ETOOMANYREFS if ran out of tp reference numbers.
 *  E* whatever error is returned from soreserve()
 *    for from the network-layer pcb allocation routine
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
int
tp_attach(so, protocol)
	struct socket  *so;
	long            protocol;
{
	register struct tp_pcb *tpcb;
	int             error = 0;
	int             dom = so->so_proto->pr_domain->dom_family;
	u_long          lref;
	extern struct tp_conn_param tp_conn_param[];

#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("tp_attach:dom 0x%x so %p ", dom, so);
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_CONN]) {
		tptrace(TPPTmisc, "tp_attach:dom so", dom, so, 0, 0);
	}
#endif

	if (so->so_pcb != NULL) {
		return EISCONN;	/* socket already part of a connection */
	}
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0)
		error = soreserve(so, tp_sendspace, tp_recvspace);
	/* later an ioctl will allow reallocation IF still in closed state */

	if (error)
		goto bad2;

	MALLOC(tpcb, struct tp_pcb *, sizeof(*tpcb), M_PCB, M_NOWAIT);
	if (tpcb == NULL) {
		error = ENOBUFS;
		goto bad2;
	}
	bzero((caddr_t) tpcb, sizeof(struct tp_pcb));

	if (((lref = tp_getref(tpcb)) & TP_ENOREF) != 0) {
		error = ETOOMANYREFS;
		goto bad3;
	}
	tpcb->tp_lref = lref;
	tpcb->tp_sock = so;
	tpcb->tp_domain = dom;
	tpcb->tp_rhiwat = so->so_rcv.sb_hiwat;
	/* tpcb->tp_proto = protocol; someday maybe? */
	if (protocol && protocol < ISOPROTO_TP4) {
		tpcb->tp_netservice = ISO_CONS;
		tpcb->tp_snduna = (SeqNum) - 1;	/* kludge so the pseudo-ack
						 * from the CR/CC will
						 * generate correct fake-ack
						 * values */
	} else {
		tpcb->tp_netservice = (dom == AF_INET) ? IN_CLNS : ISO_CLNS;
		/* the default */
	}
	tpcb->_tp_param = tp_conn_param[tpcb->tp_netservice];

	tpcb->tp_state = TP_CLOSED;
	tpcb->tp_vers = TP_VERSION;
	tpcb->tp_notdetached = 1;

	/*
	 * Spec says default is 128 octets, that is, if the tpdusize argument
	 * never appears, use 128. As the initiator, we will always "propose"
	 * the 2048 size, that is, we will put this argument in the CR
	 * always, but accept what the other side sends on the CC. If the
	 * initiator sends us something larger on a CR, we'll respond w/
	 * this. Our maximum is 4096.  See tp_chksum.c comments.
	 */
	tpcb->tp_cong_win =
		tpcb->tp_l_tpdusize = 1 << tpcb->tp_tpdusize;

	tpcb->tp_seqmask = TP_NML_FMT_MASK;
	tpcb->tp_seqbit = TP_NML_FMT_BIT;
	tpcb->tp_seqhalf = tpcb->tp_seqbit >> 1;

	/* attach to a network-layer protoswitch */
	if ((error = tp_set_npcb(tpcb)) != 0)
		goto bad4;
	ASSERT(tpcb->tp_nlproto->nlp_afamily == tpcb->tp_domain);

	/* nothing to do for iso case */
	if (dom == AF_INET)
		sotoinpcb(so)->inp_ppcb = (caddr_t) tpcb;

	return 0;

bad4:
#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("BAD4 in tp_attach, so %p\n", so);
	}
#endif
	tp_freeref(tpcb->tp_lref);

bad3:
#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("BAD3 in tp_attach, so %p\n", so);
	}
#endif

	free((caddr_t) tpcb, M_PCB);	/* never a cluster  */

bad2:
#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("BAD2 in tp_attach, so %p\n", so);
	}
#endif
	so->so_pcb = 0;

	/* bad: */
#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("BAD in tp_attach, so %p\n", so);
	}
#endif
	return error;
}

/*
 * NAME:  tp_detach()
 *
 * CALLED FROM:
 *	tp.trans, on behalf of a user close request
 *  and when the reference timer goes off
 * (if the disconnect  was initiated by the protocol entity
 * rather than by the user)
 *
 * FUNCTION and ARGUMENTS:
 *  remove the tpcb structure from the list of active or
 *  partially active connections, recycle all the mbufs
 *  associated with the pcb, ref structure, sockbufs, etc.
 *  Only free the ref structure if you know that a ref timer
 *  wasn't set for this tpcb.
 *
 * RETURNS:  Nada
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 *  tp_soisdisconnected() was already when this is called
 */
void
tp_detach(tpcb)
	register struct tp_pcb *tpcb;
{
	register struct socket *so = tpcb->tp_sock;

#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("tp_detach(tpcb %p, so %p)\n",
		       tpcb, so);
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_CONN]) {
		tptraceTPCB(TPPTmisc, "tp_detach tpcb so lsufx",
			    tpcb, so, *(u_short *) (tpcb->tp_lsuffix), 0);
	}
#endif

#ifdef ARGO_DEBUG
		if (argo_debug[D_CONN]) {
		printf("so_snd at %p so_rcv at %p\n", &so->so_snd, &so->so_rcv);
		dump_mbuf(so->so_snd.sb_mb, "so_snd at detach ");
		printf("about to call LL detach, nlproto %p, nl_detach %p\n",
		       tpcb->tp_nlproto, tpcb->tp_nlproto->nlp_pcbdetach);
	}
#endif

	if (tpcb->tp_Xsnd.sb_mb) {
		printf("Unsent Xdata on detach; would panic");
		sbflush(&tpcb->tp_Xsnd);
	}
	if (tpcb->tp_ucddata)
		m_freem(tpcb->tp_ucddata);

#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("reassembly info cnt %d rsyq %p\n",
		       tpcb->tp_rsycnt, tpcb->tp_rsyq);
	}
#endif
	if (tpcb->tp_rsyq)
		tp_rsyflush(tpcb);

	if (tpcb->tp_next) {
		remque(tpcb);
		tpcb->tp_next = tpcb->tp_prev = 0;
	}
	tpcb->tp_notdetached = 0;

#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("calling (...nlproto->...)(%p, so %p)\n",
		       tpcb->tp_npcb, so);
		printf("so %p so_head %p,  qlen %d q0len %d qlimit %d\n",
		       so, so->so_head,
		       so->so_q0len, so->so_qlen, so->so_qlimit);
	}
#endif

	(*tpcb->tp_nlproto->nlp_pcbdetach)(tpcb->tp_npcb);
	/* does an so->so_pcb = 0; sofree(so) */

#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("after xxx_pcbdetach\n");
	}
#endif

	if (tpcb->tp_state == TP_LISTENING) {
		register struct tp_pcb **tt;
		for (tt = &tp_listeners; *tt; tt = &((*tt)->tp_nextlisten))
			if (*tt == tpcb)
				break;
		if (*tt)
			*tt = tpcb->tp_nextlisten;
		else
			printf("tp_detach from listen: should panic\n");
	}
	if (tpcb->tp_refstate == REF_OPENING) {
		/*
		 * no connection existed here so no reference timer will be
		 * called
		 */
#ifdef ARGO_DEBUG
		if (argo_debug[D_CONN]) {
			printf("SETTING ref %d to REF_FREE\n", tpcb->tp_lref);
		}
#endif

		tp_freeref(tpcb->tp_lref);
	}
#ifdef TP_PERF_MEAS
	/*
	 * Get rid of the cluster mbuf allocated for performance
	 * measurements, if there is one.  Note that tpcb->tp_perf_on says
	 * nothing about whether or not a cluster mbuf was allocated, so you
	 * have to check for a pointer to one (that is, we need the
	 * TP_PERF_MEASs around the following section of code, not the
	 * IFPERFs)
	 */
	if (tpcb->tp_p_meas) {
		register struct mbuf *m = tpcb->tp_p_mbuf;
		struct mbuf    *n;
#ifdef ARGO_DEBUG
		if (argo_debug[D_PERF_MEAS]) {
			printf("freeing tp_p_meas 0x%x  ", tpcb->tp_p_meas);
		}
#endif
		free(tpcb->tp_p_meas, M_PCB);
		tpcb->tp_p_meas = 0;
	}
#endif				/* TP_PERF_MEAS */

#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("end of detach, NOT single, tpcb %p\n", tpcb);
	}
#endif
	/* free((caddr_t)tpcb, M_PCB); WHere to put this ? */
}

struct que {
	struct tp_pcb  *next;
	struct tp_pcb  *prev;
}               tp_bound_pcbs =
{
	(struct tp_pcb *) & tp_bound_pcbs, (struct tp_pcb *) & tp_bound_pcbs
};

u_short         tp_unique;

int
tp_tselinuse(tlen, tsel, siso, reuseaddr)
	int tlen;
	caddr_t         tsel;
	register struct sockaddr_iso *siso;
	int reuseaddr;
{
	struct tp_pcb  *b = tp_bound_pcbs.next, *l = tp_listeners;
	register struct tp_pcb *t;

	for (;;) {
		if (b != (struct tp_pcb *) & tp_bound_pcbs) {
			t = b;
			b = t->tp_next;
		} else if (l) {
			t = l;
			l = t->tp_nextlisten;
		} else
			break;
		if (tlen == t->tp_lsuffixlen && bcmp(tsel, t->tp_lsuffix, tlen) == 0) {
			if (t->tp_flags & TPF_GENERAL_ADDR) {
				if (siso == 0 || reuseaddr == 0)
					return 1;
			} else if (siso) {
				if (siso->siso_family == t->tp_domain &&
				    (*t->tp_nlproto->nlp_cmpnetaddr)(t->tp_npcb,
					(struct sockaddr *) siso, TP_LOCAL))
					return 1;
			} else if (reuseaddr == 0)
				return 1;
		}
	}
	return 0;

}


int
tp_pcbbind(v, nam)
	register void *v;
	register struct mbuf *nam;
{
	register struct tp_pcb *tpcb = v;
	register struct sockaddr_iso *siso = 0;
	int             tlen = 0, wrapped = 0;
	caddr_t         tsel = NULL;
	u_short         tutil;

	if (tpcb->tp_state != TP_CLOSED)
		return (EINVAL);
	if (nam) {
		siso = mtod(nam, struct sockaddr_iso *);
		switch (siso->siso_family) {
		default:
			return (EAFNOSUPPORT);
#ifdef ISO
		case AF_ISO:
			tlen = siso->siso_tlen;
			tsel = TSEL(siso);
			if (siso->siso_nlen == 0)
				siso = 0;
			break;
#endif
#ifdef INET
		case AF_INET:
			tsel = (caddr_t) & tutil;
			if ((tutil = satosin(siso)->sin_port) != 0)
				tlen = 2;
			if (satosin(siso)->sin_addr.s_addr == 0)
				siso = 0;
		}
#endif
	}
	if (tpcb->tp_lsuffixlen == 0) {
		if (tlen) {
			if (tp_tselinuse(tlen, tsel, siso,
				  tpcb->tp_sock->so_options & SO_REUSEADDR))
				return (EINVAL);
		} else {
			for (tsel = (caddr_t) & tutil, tlen = 2;;) {
				if (tp_unique++ < ISO_PORT_RESERVED ||
				    tp_unique > ISO_PORT_USERRESERVED) {
					if (wrapped++)
						return ESRCH;
					tp_unique = ISO_PORT_RESERVED;
				}
				tutil = htons(tp_unique);
				if (tp_tselinuse(tlen, tsel, siso, 0) == 0)
					break;
			}
			if (siso)
				switch (siso->siso_family) {
#ifdef ISO
				case AF_ISO:
					bcopy(tsel, TSEL(siso), tlen);
					siso->siso_tlen = tlen;
					break;
#endif
#ifdef INET
				case AF_INET:
					satosin(siso)->sin_port = tutil;
#endif
				}
		}
		bcopy(tsel, tpcb->tp_lsuffix, (tpcb->tp_lsuffixlen = tlen));
		insque(tpcb, &tp_bound_pcbs);
	} else {
		if (tlen || siso == 0)
			return (EINVAL);
	}
	if (siso == 0) {
		tpcb->tp_flags |= TPF_GENERAL_ADDR;
		return (0);
	}
	return (*tpcb->tp_nlproto->nlp_pcbbind)(tpcb->tp_npcb, nam);
}
