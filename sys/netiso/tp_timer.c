/*	$NetBSD: tp_timer.c,v 1.16.4.1 2007/09/03 14:44:08 yamt Exp $	*/

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
 *	@(#)tp_timer.c	8.1 (Berkeley) 6/10/93
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tp_timer.c,v 1.16.4.1 2007/09/03 14:44:08 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/kernel.h>

#include <netiso/argo_debug.h>
#include <netiso/tp_param.h>
#include <netiso/tp_timer.h>
#include <netiso/tp_stat.h>
#include <netiso/tp_pcb.h>
#include <netiso/tp_tpdu.h>
#include <netiso/tp_trace.h>
#include <netiso/tp_seq.h>
#include <netiso/tp_var.h>

struct tp_ref  *tp_ref;
int             tp_rttdiv, tp_rttadd, N_TPREF = 127;
struct tp_refinfo tp_refinfo;
struct tp_pcb  *tp_ftimeolist = (struct tp_pcb *) & tp_ftimeolist;

/*
 * CALLED FROM:
 *  at autoconfig time from tp_init()
 * 	a combo of event, state, predicate
 * FUNCTION and ARGUMENTS:
 *  initialize data structures for the timers
 */
void
tp_timerinit(void)
{
	int    s;
	/*
	 * Initialize storage
	 */
	if (tp_refinfo.tpr_base)
		return;
	tp_refinfo.tpr_size = N_TPREF + 1;	/* Need to start somewhere */
	s = sizeof(*tp_ref) * tp_refinfo.tpr_size;
	if ((tp_ref = (struct tp_ref *) malloc(s, M_PCB, M_NOWAIT|M_ZERO)) == 0)
		panic("tp_timerinit");
	tp_refinfo.tpr_base = tp_ref;
	tp_rttdiv = hz / PR_SLOWHZ;
	tp_rttadd = (2 * tp_rttdiv) - 1;
}
#ifdef TP_DEBUG_TIMERS
/**********************  e timers *************************/

/*
 * CALLED FROM:
 *  tp.trans all over
 * FUNCTION and ARGUMENTS:
 * Set an E type timer.
 */
void
tp_etimeout(
	struct tp_pcb *tpcb,
	int             fun,	/* function to be called */
	int             ticks)
{

	u_int *callp;
#ifdef ARGO_DEBUG
	if (argo_debug[D_TIMER]) {
		printf("etimeout pcb %p state 0x%x\n", tpcb, tpcb->tp_state);
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_TIMER]) {
		tptrace(TPPTmisc, "tp_etimeout ref refstate tks Etick", tpcb->tp_lref,
			tpcb->tp_state, ticks, tp_stat.ts_Eticks);
	}
#endif
	if (tpcb == 0)
		return;
	IncStat(ts_Eset);
	if (ticks == 0)
		ticks = 1;
	callp = tpcb->tp_timer + fun;
	if (*callp == 0 || *callp > ticks)
		*callp = ticks;
}

/*
 * CALLED FROM:
 *  tp.trans all over
 * FUNCTION and ARGUMENTS:
 *  Cancel all occurrences of E-timer function (fun) for reference (refp)
 */
void
tp_euntimeout(struct tp_pcb *tpcb, int fun)
{
#ifdef TPPT
	if (tp_traceflags[D_TIMER]) {
		tptrace(TPPTmisc, "tp_euntimeout ref", tpcb->tp_lref, 0, 0, 0);
	}
#endif

	if (tpcb)
		tpcb->tp_timer[fun] = 0;
}

/****************  c timers **********************
 *
 * These are not chained together; they sit
 * in the tp_ref structure. they are the kind that
 * are typically cancelled so it's faster not to
 * mess with the chains
 */
#endif
/*
 * CALLED FROM:
 *  the clock, every 500 ms
 * FUNCTION and ARGUMENTS:
 *  Look for open references with active timers.
 *  If they exist, call the appropriate timer routines to update
 *  the timers and possibly generate events.
 */
void
tp_slowtimo(void)
{
	u_int *cp;
	struct tp_ref *rp;
	struct tp_pcb  *tpcb;
	struct tp_event E;
	int             s = splsoftnet(), t;

	/* check only open reference structures */
	IncStat(ts_Cticks);
	/* tp_ref[0] is never used */
	for (rp = tp_ref + tp_refinfo.tpr_maxopen; rp > tp_ref; rp--) {
		if ((tpcb = rp->tpr_pcb) == 0 || tpcb->tp_refstate < REF_OPEN)
			continue;
		/* check the timers */
		for (t = 0; t < TM_NTIMERS; t++) {
			cp = tpcb->tp_timer + t;
			if (*cp && --(*cp) <= 0) {
				*cp = 0;
				E.ev_number = t;
#ifdef ARGO_DEBUG
				if (argo_debug[D_TIMER]) {
					printf("tp_slowtimo: pcb %p t %d\n",
					       tpcb, t);
				}
#endif
				IncStat(ts_Cexpired);
				tp_driver(tpcb, &E);
				if (t == TM_reference && tpcb->tp_state == TP_CLOSED) {
					if (tpcb->tp_notdetached) {
#ifdef ARGO_DEBUG
						if (argo_debug[D_CONN]) {
							printf("PRU_DETACH: not detached\n");
						}
#endif
						tp_detach(tpcb);
					}
					/* XXX wart; where else to do it? */
					free((void *) tpcb, M_PCB);
					break;
				}
			}
		}
	}
	splx(s);
}

/*
 * Called From: tp.trans from tp_slowtimo() -- retransmission timer went off.
 */
void
tp_data_retrans(struct tp_pcb *tpcb)
{
	int             rexmt, win;
	tpcb->tp_rttemit = 0;	/* cancel current round trip time */
	tpcb->tp_dupacks = 0;
	tpcb->tp_sndnxt = tpcb->tp_snduna;
	if (tpcb->tp_fcredit == 0) {
		/*
		 * We transmitted new data, started timing it and the window
		 * got shrunk under us.  This can only happen if all data
		 * that they wanted us to send got acked, so don't
		 * bother shrinking the congestion windows, et. al.
		 * The retransmission timer should have been reset in goodack()
		 */
#ifdef ARGO_DEBUG
		if (argo_debug[D_ACKRECV]) {
			printf("tp_data_retrans: 0 window tpcb %p una 0x%x\n",
			       tpcb, tpcb->tp_snduna);
		}
#endif
		tpcb->tp_rxtshift = 0;
		tpcb->tp_timer[TM_data_retrans] = 0;
		tpcb->tp_timer[TM_sendack] = tpcb->tp_dt_ticks;
		return;
	}
	rexmt = tpcb->tp_dt_ticks << min(tpcb->tp_rxtshift, TP_MAXRXTSHIFT);
	win = min(tpcb->tp_fcredit, (tpcb->tp_cong_win / tpcb->tp_l_tpdusize / 2));
	win = max(win, 2);
	tpcb->tp_cong_win = tpcb->tp_l_tpdusize;	/* slow start again. */
	tpcb->tp_ssthresh = win * tpcb->tp_l_tpdusize;
	/*
	 * We're losing; our srtt estimate is probably bogus. Clobber it so
	 * we'll take the next rtt measurement as our srtt; Maintain current
	 * rxt times until then.
	 */
	if (++tpcb->tp_rxtshift > TP_NRETRANS / 4) {
		/* tpcb->tp_nlprotosw->nlp_losing(tpcb->tp_npcb) someday */
		tpcb->tp_rtt = 0;
	}
	TP_RANGESET(tpcb->tp_rxtcur, rexmt, tpcb->tp_peer_acktime, 128);
	tpcb->tp_timer[TM_data_retrans] = tpcb->tp_rxtcur;
	tp_send(tpcb);
}

void
tp_fasttimo(void)
{
	struct tp_pcb *t;
	int             s = splsoftnet();
	struct tp_event E;

	E.ev_number = TM_sendack;
	while ((t = tp_ftimeolist) != (struct tp_pcb *) & tp_ftimeolist) {
		if (t == 0) {
			printf("tp_fasttimeo: should panic");
			tp_ftimeolist = (struct tp_pcb *) & tp_ftimeolist;
		} else {
			if (t->tp_flags & TPF_DELACK) {
				IncStat(ts_Fdelack);
				tp_driver(t, &E);
				t->tp_flags &= ~TPF_DELACK;
			} else
				IncStat(ts_Fpruned);
			tp_ftimeolist = t->tp_fasttimeo;
			t->tp_fasttimeo = 0;
		}
	}
	splx(s);
}

#ifdef TP_DEBUG_TIMERS
/*
 * CALLED FROM:
 *  tp.trans, tp_emit()
 * FUNCTION and ARGUMENTS:
 * 	Set a C type timer of type (which) to go off after (ticks) time.
 */
void
tp_ctimeout(struct tp_pcb *tpcb, int which, int ticks)
{

#ifdef TPPT
	if (tp_traceflags[D_TIMER]) {
		tptrace(TPPTmisc, "tp_ctimeout ref which tpcb active",
			tpcb->tp_lref, which, tpcb, tpcb->tp_timer[which]);
	}
#endif
	if (tpcb->tp_timer[which])
		IncStat(ts_Ccan_act);
	IncStat(ts_Cset);
	if (ticks <= 0)
		ticks = 1;
	tpcb->tp_timer[which] = ticks;
}

/*
 * CALLED FROM:
 *  tp.trans
 * FUNCTION and ARGUMENTS:
 * 	Version of tp_ctimeout that resets the C-type time if the
 * 	parameter (ticks) is > the current value of the timer.
 */
void
tp_ctimeout_MIN(struct tp_pcb *tpcb, int which, int ticks)
{
#ifdef TPPT
	if (tp_traceflags[D_TIMER]) {
		tptrace(TPPTmisc, "tp_ctimeout_MIN ref which tpcb active",
			tpcb->tp_lref, which, tpcb, tpcb->tp_timer[which]);
	}
#endif
		IncStat(ts_Cset);
	if (tpcb->tp_timer[which]) {
		tpcb->tp_timer[which] = min(ticks, tpcb->tp_timer[which]);
		IncStat(ts_Ccan_act);
	} else
		tpcb->tp_timer[which] = ticks;
}

/*
 * CALLED FROM:
 *  tp.trans
 * FUNCTION and ARGUMENTS:
 *  Cancel the (which) timer in the ref structure indicated by (refp).
 */
void
tp_cuntimeout(struct tp_pcb *tpcb, int which)
{
#ifdef ARGO_DEBUG
	if (argo_debug[D_TIMER]) {
		printf("tp_cuntimeout(%p, %d) active %d\n",
		       tpcb, which, tpcb->tp_timer[which]);
	}
#endif

#ifdef TPPT
	if (tp_traceflags[D_TIMER]) {
		tptrace(TPPTmisc, "tp_cuntimeout ref which, active", refp - tp_ref,
			which, tpcb->tp_timer[which], 0);
	}
#endif

	if (tpcb->tp_timer[which])
		IncStat(ts_Ccan_act);
	else
		IncStat(ts_Ccan_inact);
	tpcb->tp_timer[which] = 0;
}
#endif
