/*	$NetBSD: tp_subr.c,v 1.12.8.1 2002/01/10 20:03:56 thorpej Exp $	*/

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
 *	@(#)tp_subr.c	8.1 (Berkeley) 6/10/93
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
 * The main work of data transfer is done here. These routines are called
 * from tp.trans. They include the routines that check the validity of acks
 * and Xacks, (tp_goodack() and tp_goodXack() ) take packets from socket
 * buffers and send them (tp_send()), drop the data from the socket buffers
 * (tp_sbdrop()),  and put incoming packet data into socket buffers
 * (tp_stash()).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tp_subr.c,v 1.12.8.1 2002/01/10 20:03:56 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <netiso/tp_ip.h>
#include <netiso/iso.h>
#include <netiso/argo_debug.h>
#include <netiso/tp_timer.h>
#include <netiso/tp_param.h>
#include <netiso/tp_stat.h>
#include <netiso/tp_pcb.h>
#include <netiso/tp_tpdu.h>
#include <netiso/tp_trace.h>
#include <netiso/tp_meas.h>
#include <netiso/tp_seq.h>
#include <netiso/tp_var.h>

int             tprexmtthresh = 3;

/*
 * CALLED FROM:
 *	tp.trans, when an XAK arrives
 * FUNCTION and ARGUMENTS:
 * 	Determines if the sequence number (seq) from the XAK
 * 	acks anything new.  If so, drop the appropriate tpdu
 * 	from the XPD send queue.
 * RETURN VALUE:
 * 	Returns 1 if it did this, 0 if the ack caused no action.
 */
int
tp_goodXack(tpcb, seq)
	struct tp_pcb  *tpcb;
	SeqNum          seq;
{

#ifdef TPPT
	if (tp_traceflags[D_XPD]) {
		tptraceTPCB(TPPTgotXack,
		      seq, tpcb->tp_Xuna, tpcb->tp_Xsndnxt, tpcb->tp_sndnew,
			    tpcb->tp_snduna);
	}
#endif

	if (seq == tpcb->tp_Xuna) {
		tpcb->tp_Xuna = tpcb->tp_Xsndnxt;

		/*
		 * DROP 1 packet from the Xsnd socket buf - just so happens
		 * that only one packet can be there at any time so drop the
		 * whole thing.  If you allow > 1 packet the socket buffer,
		 * then you'll have to keep track of how many characters went
		 * w/ each XPD tpdu, so this will get messier
		 */
#ifdef ARGO_DEBUG
		if (argo_debug[D_XPD]) {
			dump_mbuf(tpcb->tp_Xsnd.sb_mb,
				  "tp_goodXack Xsnd before sbdrop");
		}
#endif

#ifdef TPPT
		if (tp_traceflags[D_XPD]) {
			tptraceTPCB(TPPTmisc,
				    "goodXack: dropping cc ",
				    (int) (tpcb->tp_Xsnd.sb_cc),
				    0, 0, 0);
		}
#endif
		sbdroprecord(&tpcb->tp_Xsnd);
		return 1;
	}
	return 0;
}

/*
 * CALLED FROM:
 *  tp_good_ack()
 * FUNCTION and ARGUMENTS:
 *  updates
 *  smoothed average round trip time (*rtt)
 *  roundtrip time variance (*rtv) - actually deviation, not variance
 *  given the new value (diff)
 * RETURN VALUE:
 * void
 */

void
tp_rtt_rtv(tpcb)
	struct tp_pcb *tpcb;
{
	int             old = tpcb->tp_rtt;
	int             s, elapsed, delta = 0;

	s = splclock();
	elapsed = (int)(hardclock_ticks - tpcb->tp_rttemit);
	splx(s);

	if (tpcb->tp_rtt != 0) {
		/*
		 * rtt is the smoothed round trip time in machine clock
		 * ticks (hz). It is stored as a fixed point number,
		 * unscaled (unlike the tcp srtt).  The rationale here
		 * is that it is only significant to the nearest unit of
		 * slowtimo, which is at least 8 machine clock ticks
		 * so there is no need to scale.  The smoothing is done
		 * according to the same formula as TCP (rtt = rtt*7/8
		 * + measured_rtt/8).
		 */
		delta = elapsed - tpcb->tp_rtt;
		if ((tpcb->tp_rtt += (delta >> TP_RTT_ALPHA)) <= 0)
			tpcb->tp_rtt = 1;
		/*
		 * rtv is a smoothed accumulated mean difference, unscaled
		 * for reasons expressed above.
		 * It is smoothed with an alpha of .75, and the round trip timer
		 * will be set to rtt + 4*rtv, also as TCP does.
		 */
		if (delta < 0)
			delta = -delta;
		if ((tpcb->tp_rtv += ((delta - tpcb->tp_rtv) >> TP_RTV_ALPHA)) <= 0)
			tpcb->tp_rtv = 1;
	} else {
		/*
		 * No rtt measurement yet - use the unsmoothed rtt. Set the
		 * variance to half the rtt (so our first retransmit happens
		 * at 3*rtt)
		 */
		tpcb->tp_rtt = elapsed;
		tpcb->tp_rtv = elapsed >> 1;
	}
	tpcb->tp_rttemit = 0;
	tpcb->tp_rxtshift = 0;
	/*
	 * Quoting TCP: "the retransmit should happen at rtt + 4 * rttvar.
	 * Because of the way we do the smoothing, srtt and rttvar
	 * will each average +1/2 tick of bias.  When we compute
	 * the retransmit timer, we want 1/2 tick of rounding and
	 * 1 extra tick because of +-1/2 tick uncertainty in the
	 * firing of the timer.  The bias will give us exactly the
	 * 1.5 tick we need.  But, because the bias is
	 * statistical, we have to test that we don't drop below
	 * the minimum feasible timer (which is 2 ticks)."
	 */
	TP_RANGESET(tpcb->tp_dt_ticks, TP_REXMTVAL(tpcb),
		    tpcb->tp_peer_acktime, 128 /* XXX */ );
#ifdef ARGO_DEBUG
	if (argo_debug[D_RTT]) {
		printf("%s tpcb %p, elapsed %d, delta %d, rtt %d, rtv %d, old %d\n",
		       "tp_rtt_rtv:", tpcb, elapsed, delta, tpcb->tp_rtt, tpcb->tp_rtv, old);
	}
#endif
	tpcb->tp_rxtcur = tpcb->tp_dt_ticks;
}

/*
 * CALLED FROM:
 *  tp.trans when an AK arrives
 * FUNCTION and ARGUMENTS:
 * 	Given (cdt), the credit from the AK tpdu, and
 *	(seq), the sequence number from the AK tpdu,
 *  tp_goodack() determines if the AK acknowledges something in the send
 * 	window, and if so, drops the appropriate packets from the retransmission
 *  list, computes the round trip time, and updates the retransmission timer
 *  based on the new smoothed round trip time.
 * RETURN VALUE:
 * 	Returns 1 if
 * 	EITHER it actually acked something heretofore unacknowledged
 * 	OR no news but the credit should be processed.
 * 	If something heretofore unacked was acked with this sequence number,
 * 	the appropriate tpdus are dropped from the retransmission control list,
 * 	by calling tp_sbdrop().
 * 	No need to see the tpdu itself.
 */
int
tp_goodack(tpcb, cdt, seq, subseq)
	struct tp_pcb *tpcb;
	u_int           cdt;
	SeqNum seq;
	u_int           subseq;
{
	int             old_fcredit = 0;
	int             bang = 0;	/* bang --> ack for something
					 * heretofore unacked */
	u_int           bytes_acked;

#ifdef ARGO_DEBUG
	if (argo_debug[D_ACKRECV]) {
		printf("goodack tpcb %p seq 0x%x cdt %d una 0x%x new 0x%x nxt 0x%x\n",
		       tpcb, seq, cdt, tpcb->tp_snduna, tpcb->tp_sndnew, tpcb->tp_sndnxt);
	}
#endif

#ifdef TPPT
	if (tp_traceflags[D_ACKRECV]) {
		tptraceTPCB(TPPTgotack,
			seq, cdt, tpcb->tp_snduna, tpcb->tp_sndnew, subseq);
	}
#endif

#ifdef TP_PERF_MEAS
		if (DOPERF(tpcb)) {
		tpmeas(tpcb->tp_lref, TPtime_ack_rcvd, (struct timeval *) 0, seq, 0, 0);
	}
#endif

	if (seq == tpcb->tp_snduna) {
		if (subseq < tpcb->tp_r_subseq ||
		 (subseq == tpcb->tp_r_subseq && cdt <= tpcb->tp_fcredit)) {
	discard_the_ack:
#ifdef ARGO_DEBUG
			if (argo_debug[D_ACKRECV]) {
				printf("goodack discard : tpcb %p subseq %d r_subseq %d\n",
				       tpcb, subseq, tpcb->tp_r_subseq);
			}
#endif
			goto done;
		}
		if (cdt == tpcb->tp_fcredit	/* && thus subseq >
		        tpcb->tp_r_subseq */ ) {
			tpcb->tp_r_subseq = subseq;
			if (tpcb->tp_timer[TM_data_retrans] == 0)
				tpcb->tp_dupacks = 0;
			else if (++tpcb->tp_dupacks == tprexmtthresh) {
				/*
				 * partner went out of his way to signal with
				 * different subsequences that he has the
				 * same lack of an expected packet.  This may
				 * be an early indiciation of a loss
				 */

				SeqNum          onxt = tpcb->tp_sndnxt;
				struct mbuf    *onxt_m = tpcb->tp_sndnxt_m;
				u_int           win = min(tpcb->tp_fcredit,
				tpcb->tp_cong_win / tpcb->tp_l_tpdusize) / 2;
#ifdef ARGO_DEBUG
				if (argo_debug[D_ACKRECV]) {
					printf("%s tpcb %p seq 0x%x rttseq 0x%x onxt 0x%x\n",
					       "goodack dupacks:", tpcb, seq, tpcb->tp_rttseq, onxt);
				}
#endif
				if (win < 2)
					win = 2;
				tpcb->tp_ssthresh = win * tpcb->tp_l_tpdusize;
				tpcb->tp_timer[TM_data_retrans] = 0;
				tpcb->tp_rttemit = 0;
				tpcb->tp_sndnxt = tpcb->tp_snduna;
				tpcb->tp_sndnxt_m = 0;
				tpcb->tp_cong_win = tpcb->tp_l_tpdusize;
				tp_send(tpcb);
				tpcb->tp_cong_win = tpcb->tp_ssthresh +
					tpcb->tp_dupacks * tpcb->tp_l_tpdusize;
				if (SEQ_GT(tpcb, onxt, tpcb->tp_sndnxt)) {
					tpcb->tp_sndnxt = onxt;
					tpcb->tp_sndnxt_m = onxt_m;
				}
			} else if (tpcb->tp_dupacks > tprexmtthresh) {
				tpcb->tp_cong_win += tpcb->tp_l_tpdusize;
			}
			goto done;
		}
	} else if (SEQ_LT(tpcb, seq, tpcb->tp_snduna))
		goto discard_the_ack;
	/*
	 * If the congestion window was inflated to account
	 * for the other side's cached packets, retract it.
	 */
	if (tpcb->tp_dupacks > tprexmtthresh &&
	    tpcb->tp_cong_win > tpcb->tp_ssthresh)
		tpcb->tp_cong_win = tpcb->tp_ssthresh;
	tpcb->tp_r_subseq = subseq;
	old_fcredit = tpcb->tp_fcredit;
	tpcb->tp_fcredit = cdt;
	if (cdt > tpcb->tp_maxfcredit)
		tpcb->tp_maxfcredit = cdt;
	tpcb->tp_dupacks = 0;

	if (IN_SWINDOW(tpcb, seq, tpcb->tp_snduna, tpcb->tp_sndnew)) {

		tpsbcheck(tpcb, 0);
		bytes_acked = tp_sbdrop(tpcb, seq);
		tpsbcheck(tpcb, 1);
		/*
		 * If transmit timer is running and timed sequence
		 * number was acked, update smoothed round trip time.
		 * Since we now have an rtt measurement, cancel the
		 * timer backoff (cf., Phil Karn's retransmit alg.).
		 * Recompute the initial retransmit timer.
		 */
		if (tpcb->tp_rttemit && SEQ_GT(tpcb, seq, tpcb->tp_rttseq))
			tp_rtt_rtv(tpcb);
		/*
		 * If all outstanding data is acked, stop retransmit timer.
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 * OSI combines the keepalive and persistance functions.
		 * So, there is no persistance timer per se, to restart.
		 */
		if (tpcb->tp_class != TP_CLASS_0)
			tpcb->tp_timer[TM_data_retrans] =
				(seq == tpcb->tp_sndnew) ? 0 : tpcb->tp_rxtcur;
		/*
		 * When new data is acked, open the congestion window.
		 * If the window gives us less than ssthresh packets
		 * in flight, open exponentially (maxseg per packet).
		 * Otherwise open linearly: maxseg per window
		 * (maxseg^2 / cwnd per packet), plus a constant
		 * fraction of a packet (maxseg/8) to help larger windows
		 * open quickly enough.
		 */
		{
			u_int           cw = tpcb->tp_cong_win, incr = tpcb->tp_l_tpdusize;

			incr = min(incr, bytes_acked);
			if (cw > tpcb->tp_ssthresh)
				incr = incr * incr / cw + incr / 8;
			tpcb->tp_cong_win =
				min(cw + incr, tpcb->tp_sock->so_snd.sb_hiwat);
		}
		tpcb->tp_snduna = seq;
		if (SEQ_LT(tpcb, tpcb->tp_sndnxt, seq)) {
			tpcb->tp_sndnxt = seq;
			tpcb->tp_sndnxt_m = 0;
		}
		bang++;
	}
	if (cdt != 0 && old_fcredit == 0) {
		tpcb->tp_sendfcc = 1;
	}
	if (cdt == 0) {
		if (old_fcredit != 0)
			IncStat(ts_zfcdt);
		/* The following might mean that the window shrunk */
		if (tpcb->tp_timer[TM_data_retrans]) {
			tpcb->tp_timer[TM_data_retrans] = 0;
			tpcb->tp_timer[TM_sendack] = tpcb->tp_dt_ticks;
			if (tpcb->tp_sndnxt != tpcb->tp_snduna) {
				tpcb->tp_sndnxt = tpcb->tp_snduna;
				tpcb->tp_sndnxt_m = 0;
			}
		}
	}
	tpcb->tp_fcredit = cdt;
	bang |= (old_fcredit < cdt);

done:
#ifdef ARGO_DEBUG
	if (argo_debug[D_ACKRECV]) {
		printf("goodack returns 0x%x, cdt 0x%x ocdt 0x%x cwin 0x%lx\n",
		       bang, cdt, old_fcredit, tpcb->tp_cong_win);
	}
#endif
	/*
	 * if (bang) XXXXX Very bad to remove this test, but somethings
	 * broken
	 */
	tp_send(tpcb);
	return (bang);
}

/*
 * CALLED FROM:
 *  tp_goodack()
 * FUNCTION and ARGUMENTS:
 *  drops everything up TO but not INCLUDING seq # (seq)
 *  from the retransmission queue.
 */
int
tp_sbdrop(tpcb, seq)
	struct tp_pcb *tpcb;
	SeqNum          seq;
{
	struct sockbuf *sb = &tpcb->tp_sock->so_snd;
	int    i = SEQ_SUB(tpcb, seq, tpcb->tp_snduna);
	int             oldcc = sb->sb_cc, oldi = i;

	if (i >= tpcb->tp_seqhalf)
		printf("tp_spdropping too much -- should panic");
	while (i-- > 0)
		sbdroprecord(sb);
#ifdef ARGO_DEBUG
	if (argo_debug[D_ACKRECV]) {
		printf("tp_sbdroping %d pkts %ld bytes on %p at 0x%x\n",
		       oldi, oldcc - sb->sb_cc, tpcb, seq);
	}
#endif
	if (sb_notify(sb))
		sowwakeup(tpcb->tp_sock);
	return (oldcc - sb->sb_cc);
}

/*
 * CALLED FROM:
 * 	tp.trans on user send request, arrival of AK and arrival of XAK
 * FUNCTION and ARGUMENTS:
 * 	Emits tpdus starting at sequence number (tpcb->tp_sndnxt).
 * 	Emits until a) runs out of data, or  b) runs into an XPD mark, or
 * 			c) it hits seq number (highseq) limited by cong or credit.
 *
 * 	If you want XPD to buffer > 1 du per socket buffer, you can
 * 	modifiy this to issue XPD tpdus also, but then it'll have
 * 	to take some argument(s) to distinguish between the type of DU to
 * 	hand tp_emit.
 *
 * 	When something is sent for the first time, its time-of-send
 * 	is stashed (in system clock ticks rather than pf_slowtimo ticks).
 *  When the ack arrives, the smoothed round-trip time is figured
 *  using this value.
 */
void
tp_send(tpcb)
	struct tp_pcb *tpcb;
{
	int    len;
	struct mbuf *m;
	struct mbuf    *mb = 0;
	struct sockbuf *sb = &tpcb->tp_sock->so_snd;
	unsigned int    eotsdu = 0;
	SeqNum          highseq, checkseq;
	int             s, idle, idleticks, off, cong_win;
#ifdef TP_PERF_MEAS
	u_int64_t       send_start_time;
	SeqNum          oldnxt = tpcb->tp_sndnxt;

	s = splclock();
	send_start_time = hardclock_ticks;
	splx(s);
#endif /* TP_PERF_MEAS */

	idle = (tpcb->tp_snduna == tpcb->tp_sndnew);
	if (idle) {
		idleticks = tpcb->tp_inact_ticks - tpcb->tp_timer[TM_inact];
		if (idleticks > tpcb->tp_dt_ticks)
			/*
			 * We have been idle for "a while" and no acks are
			 * expected to clock out any data we send --
			 * slow start to get ack "clock" running again.
			 */
			tpcb->tp_cong_win = tpcb->tp_l_tpdusize;
	}
	cong_win = tpcb->tp_cong_win;
	highseq = SEQ(tpcb, tpcb->tp_fcredit + tpcb->tp_snduna);
	if (tpcb->tp_Xsnd.sb_mb)
		highseq = SEQ_MIN(tpcb, highseq, tpcb->tp_sndnew);

#ifdef ARGO_DEBUG
	if (argo_debug[D_DATA]) {
		printf("tp_send enter tpcb %p nxt 0x%x win %d high 0x%x\n",
		       tpcb, tpcb->tp_sndnxt, cong_win, highseq);
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_DATA]) {
		tptraceTPCB(TPPTmisc, "tp_send sndnew snduna",
			    tpcb->tp_sndnew, tpcb->tp_snduna, 0, 0);
	tptraceTPCB(TPPTmisc, "tp_send tpcb->tp_sndnxt win fcredit congwin",
	    tpcb->tp_sndnxt, cong_win, tpcb->tp_fcredit, tpcb->tp_cong_win);
	}
#endif
#ifdef TPPT
		if (tp_traceflags[D_DATA]) {
		tptraceTPCB(TPPTmisc, "tp_send 2 nxt high fcredit congwin",
		      tpcb->tp_sndnxt, highseq, tpcb->tp_fcredit, cong_win);
	}
#endif

		if (tpcb->tp_sndnxt_m)
		m = tpcb->tp_sndnxt_m;
	else {
		off = SEQ_SUB(tpcb, tpcb->tp_sndnxt, tpcb->tp_snduna);
		for (m = sb->sb_mb; m && off > 0; m = m->m_next)
			off--;
	}
	/*
	 * Avoid silly window syndrome here . . . figure out how!
	 */
	checkseq = tpcb->tp_sndnum;
	if (idle && SEQ_LT(tpcb, tpcb->tp_sndnum, highseq))
		checkseq = highseq;	/* i.e. DON'T retain highest assigned
					 * packet */

	while ((SEQ_LT(tpcb, tpcb->tp_sndnxt, highseq)) && m && cong_win > 0) {

		eotsdu = (m->m_flags & M_EOR) != 0;
		len = m->m_pkthdr.len;
		if (tpcb->tp_sndnxt == checkseq && eotsdu == 0 &&
		    len < (tpcb->tp_l_tpdusize / 2))
			break;	/* Nagle . . . . . */
		cong_win -= len;
		/*
		 * make a copy - mb goes into the retransmission list while m
		 * gets emitted.  m_copy won't copy a zero-length mbuf.
		 */
		mb = m;
		m = m_copy(mb, 0, M_COPYALL);
		if (m == MNULL)
			break;
#ifdef TPPT
		if (tp_traceflags[D_STASH]) {
			tptraceTPCB(TPPTmisc,
				    "tp_send mcopy nxt high eotsdu len",
				    tpcb->tp_sndnxt, highseq, eotsdu, len);
		}
#endif

#ifdef ARGO_DEBUG
			if (argo_debug[D_DATA]) {
			printf("tp_sending tpcb %p nxt 0x%x\n",
			       tpcb, tpcb->tp_sndnxt);
		}
#endif
		/*
		 * when headers are precomputed, may need to fill in checksum
		 * here
		 */
		tpcb->tp_sock->so_error =
		      tp_emit(DT_TPDU_type, tpcb, tpcb->tp_sndnxt, eotsdu, m);
		if (tpcb->tp_sock->so_error != 0)
			/* error */
			break;
		m = mb->m_nextpkt;
		tpcb->tp_sndnxt_m = m;
		if (tpcb->tp_sndnxt == tpcb->tp_sndnew) {
			SEQ_INC(tpcb, tpcb->tp_sndnew);
			/*
			 * Time this transmission if not a retransmission and
			 * not currently timing anything.
			 */
			if (tpcb->tp_rttemit == 0) {
				s = splclock();
				tpcb->tp_rttemit = hardclock_ticks;
				splx(s);
				tpcb->tp_rttseq = tpcb->tp_sndnxt;
			}
			tpcb->tp_sndnxt = tpcb->tp_sndnew;
		} else
			SEQ_INC(tpcb, tpcb->tp_sndnxt);
		/*
		 * Set retransmit timer if not currently set.
		 * Initial value for retransmit timer is smoothed
		 * round-trip time + 2 * round-trip time variance.
		 * Initialize shift counter which is used for backoff
		 * of retransmit time.
		 */
		if (tpcb->tp_timer[TM_data_retrans] == 0 &&
		    tpcb->tp_class != TP_CLASS_0) {
			tpcb->tp_timer[TM_data_retrans] = tpcb->tp_dt_ticks;
			tpcb->tp_timer[TM_sendack] = tpcb->tp_keepalive_ticks;
			tpcb->tp_rxtshift = 0;
		}
	}
	if (SEQ_GT(tpcb, tpcb->tp_sndnew, tpcb->tp_sndnum))
		tpcb->tp_oktonagle = 0;
#ifdef TP_PERF_MEAS
	if (DOPERF(tpcb)) {
		int    npkts;
		int             s, elapsed, *t;
		struct timeval  now;

		s = splclock();
		elapsed = (int)(hardclock_ticks - send_start_time);
		splx(s);

		npkts = SEQ_SUB(tpcb, tpcb->tp_sndnxt, oldnxt);

		if (npkts > 0)
			tpcb->tp_Nwindow++;

		if (npkts > TP_PM_MAX)
			npkts = TP_PM_MAX;

		t = &(tpcb->tp_p_meas->tps_sendtime[npkts]);
		*t += (t - elapsed) >> TP_RTT_ALPHA;

		if (mb == 0) {
			IncPStat(tpcb, tps_win_lim_by_data[npkts]);
		} else {
			IncPStat(tpcb, tps_win_lim_by_cdt[npkts]);
			/* not true with congestion-window being used */
		}
		now.tv_sec = elapsed / hz;
		now.tv_usec = (elapsed - (hz * now.tv_sec)) * 1000000 / hz;
		tpmeas(tpcb->tp_lref,
		       TPsbsend, &elapsed, newseq, tpcb->tp_Nwindow, npkts);
	}
#endif				/* TP_PERF_MEAS */


#ifdef TPPT
	if (tp_traceflags[D_DATA]) {
		tptraceTPCB(TPPTmisc,
			    "tp_send at end: new nxt eotsdu error",
			    tpcb->tp_sndnew, tpcb->tp_sndnxt, eotsdu,
			    tpcb->tp_sock->so_error);

	}
#endif
}

int             TPNagleok;
int             TPNagled;

int
tp_packetize(tpcb, m, eotsdu)
	struct tp_pcb *tpcb;
	struct mbuf *m;
	int             eotsdu;
{
	struct mbuf *n = NULL;
	struct sockbuf *sb = &tpcb->tp_sock->so_snd;
	int             maxsize = tpcb->tp_l_tpdusize
			    - tp_headersize(DT_TPDU_type, tpcb)
			    - (tpcb->tp_use_checksum ? 4 : 0);
	int             totlen = m->m_pkthdr.len;

	/*
	 * Pre-packetize the data in the sockbuf
	 * according to negotiated mtu.  Do it here
	 * where we can safely wait for mbufs.
	 *
	 * This presumes knowledge of sockbuf conventions.
	 * TODO: allocate space for header and fill it in (once!).
	 */
#ifdef ARGO_DEBUG
	if (argo_debug[D_DATA]) {
		printf("SEND BF: maxsize %d totlen %d eotsdu %d sndnum 0x%x\n",
		       maxsize, totlen, eotsdu, tpcb->tp_sndnum);
	}
#endif
	if (tpcb->tp_oktonagle) {
		if ((n = sb->sb_mb) == 0)
			panic("tp_packetize");
		while (n->m_nextpkt)
			n = n->m_nextpkt;
		if (n->m_flags & M_EOR)
			panic("tp_packetize 2");
		SEQ_INC(tpcb, tpcb->tp_sndnum);
		if (totlen + n->m_pkthdr.len < maxsize) {
			/*
			 * There is an unsent packet with space,
			 * combine data
			 */
			struct mbuf    *old_n = n;
			tpsbcheck(tpcb, 3);
			n->m_pkthdr.len += totlen;
			while (n->m_next)
				n = n->m_next;
			sbcompress(sb, m, n);
			tpsbcheck(tpcb, 4);
			n = old_n;
			TPNagled++;
			goto out;
		}
	}

	while (m) {
		n = m;
		if (totlen > maxsize) {
			if ((m = m_split(n, maxsize, M_WAIT)) == 0)
				panic("tp_packetize");
		} else
			m = 0;
		totlen -= maxsize;
		tpsbcheck(tpcb, 5);
		sbappendrecord(sb, n);
		tpsbcheck(tpcb, 6);
		SEQ_INC(tpcb, tpcb->tp_sndnum);
	}
out:
	if (eotsdu) {
		n->m_flags |= M_EOR;	/* XXX belongs at end */
		tpcb->tp_oktonagle = 0;
	} else {
		SEQ_DEC(tpcb, tpcb->tp_sndnum);
		tpcb->tp_oktonagle = 1;
		TPNagleok++;
	}

#ifdef ARGO_DEBUG
	if (argo_debug[D_DATA]) {
		printf("SEND out: oktonagle %d sndnum 0x%x\n",
		       tpcb->tp_oktonagle, tpcb->tp_sndnum);
	}
#endif
	return 0;
}


/*
 * NAME: tp_stash()
 * CALLED FROM:
 *	tp.trans on arrival of a DT tpdu
 * FUNCTION, ARGUMENTS, and RETURN VALUE:
 * 	Returns 1 if
 *	a) something new arrived and it's got eotsdu_reached bit on,
 * 	b) this arrival was caused other out-of-sequence things to be
 *    	accepted, or
 * 	c) this arrival is the highest seq # for which we last gave credit
 *   	(sender just sent a whole window)
 *  In other words, returns 1 if tp should send an ack immediately, 0 if
 *  the ack can wait a while.
 *
 * Note: this implementation no longer renegs on credit, (except
 * when debugging option D_RENEG is on, for the purpose of testing
 * ack subsequencing), so we don't  need to check for incoming tpdus
 * being in a reneged portion of the window.
 */

int
tp_stash(tpcb, e)
	struct tp_pcb *tpcb;
	struct tp_event *e;
{
	int    ack_reason = tpcb->tp_ack_strat & ACK_STRAT_EACH;
	/* 0--> delay acks until full window */
	/* 1--> ack each tpdu */
#define E e->TPDU_ATTR(DT)

	if (E.e_eot) {
		struct mbuf *n = E.e_data;
		n->m_flags |= M_EOR;
		n->m_nextpkt = 0;
	}
#ifdef ARGO_DEBUG
	if (argo_debug[D_STASH]) {
		dump_mbuf(tpcb->tp_sock->so_rcv.sb_mb,
			  "stash: so_rcv before appending");
		dump_mbuf(E.e_data,
			  "stash: e_data before appending");
	}
#endif

#ifdef TP_PERF_MEAS
	if (DOPERF(tpcb)) {
		PStat(tpcb, Nb_from_ll) += E.e_datalen;
		tpmeas(tpcb->tp_lref, TPtime_from_ll,
		       &e->e_time, E.e_seq,
		       (u_int) PStat(tpcb, Nb_from_ll),
		       (u_int) E.e_datalen);
	}
#endif

	if (E.e_seq == tpcb->tp_rcvnxt) {

#ifdef ARGO_DEBUG
		if (argo_debug[D_STASH]) {
			printf("stash EQ: seq 0x%x datalen 0x%x eot 0x%x\n",
			     E.e_seq, E.e_datalen, E.e_eot);
		}
#endif

#ifdef TPPT
		if (tp_traceflags[D_STASH]) {
			tptraceTPCB(TPPTmisc, "stash EQ: seq len eot",
			  E.e_seq, E.e_datalen, E.e_eot, 0);
		}
#endif

		SET_DELACK(tpcb);

		sbappend(&tpcb->tp_sock->so_rcv, E.e_data);

		SEQ_INC(tpcb, tpcb->tp_rcvnxt);
		/*
		 * move chains from the reassembly queue to the socket buffer
		 */
		if (tpcb->tp_rsycnt) {
			struct mbuf **mp;
			struct mbuf   **mplim;

			mp = tpcb->tp_rsyq + (tpcb->tp_rcvnxt %
					      tpcb->tp_maxlcredit);
			mplim = tpcb->tp_rsyq + tpcb->tp_maxlcredit;

			while (tpcb->tp_rsycnt && *mp) {
				sbappend(&tpcb->tp_sock->so_rcv, *mp);
				tpcb->tp_rsycnt--;
				*mp = 0;
				SEQ_INC(tpcb, tpcb->tp_rcvnxt);
				ack_reason |= ACK_REORDER;
				if (++mp == mplim)
					mp = tpcb->tp_rsyq;
			}
		}
#ifdef ARGO_DEBUG
		if (argo_debug[D_STASH]) {
			dump_mbuf(tpcb->tp_sock->so_rcv.sb_mb,
			   "stash: so_rcv after appending");
		}
#endif

	} else {
		struct mbuf **mp;
		SeqNum          uwe;

#ifdef TPPT
		if (tp_traceflags[D_STASH]) {
			tptraceTPCB(TPPTmisc, "stash Reseq: seq rcvnxt lcdt",
				    E.e_seq, tpcb->tp_rcvnxt,
				    tpcb->tp_lcredit, 0);
		}
#endif

		if (tpcb->tp_rsyq == 0)
			tp_rsyset(tpcb);
		uwe = SEQ(tpcb, tpcb->tp_rcvnxt + tpcb->tp_maxlcredit);
		if (tpcb->tp_rsyq == 0 ||
		    !IN_RWINDOW(tpcb, E.e_seq, tpcb->tp_rcvnxt, uwe)) {
			ack_reason = ACK_DONT;
			m_freem(E.e_data);
		} else if (*(mp = tpcb->tp_rsyq +
			     (E.e_seq % tpcb->tp_maxlcredit)) != NULL ) {
#ifdef ARGO_DEBUG
			if (argo_debug[D_STASH]) {
				printf("tp_stash - drop & ack\n");
			}
#endif

			/*
			 * retransmission - drop it and force
			 * an ack
			 */
			IncStat(ts_dt_dup);
#ifdef TP_PERF_MEAS
			if (DOPERF(tpcb)) {
				IncPStat(tpcb, tps_n_ack_cuz_dup);
			}
#endif

				m_freem(E.e_data);
			ack_reason |= ACK_DUP;
		} else {
			*mp = E.e_data;
			tpcb->tp_rsycnt++;
			ack_reason = ACK_DONT;
		}
	}
	/*
	 * there were some comments of historical interest
	 * here.
	 */
	{
		LOCAL_CREDIT(tpcb);

		if (E.e_seq == tpcb->tp_sent_uwe)
			ack_reason |= ACK_STRAT_FULLWIN;

#ifdef TPPT
		if (tp_traceflags[D_STASH]) {
			tptraceTPCB(TPPTmisc,
		 "end of stash, eot, ack_reason, sent_uwe ",
		 E.e_eot, ack_reason, tpcb->tp_sent_uwe, 0);
		}
#endif

		if (ack_reason == ACK_DONT) {
			IncStat(ts_ackreason[ACK_DONT]);
			return 0;
		} else {
#ifdef TP_PERF_MEAS
			if (DOPERF(tpcb)) {
				if (ack_reason & ACK_STRAT_EACH) {
				IncPStat(tpcb, tps_n_ack_cuz_strat);
			} else if (ack_reason & ACK_STRAT_FULLWIN) {
				IncPStat(tpcb, tps_n_ack_cuz_fullwin);
			} else if (ack_reason & ACK_REORDER) {
				IncPStat(tpcb, tps_n_ack_cuz_reorder);
			}
			tpmeas(tpcb->tp_lref, TPtime_ack_sent, 0,
			   SEQ_ADD(tpcb, E.e_seq, 1), 0, 0);
			}
#endif
			{
				int    i;

				/*
				 * keep track of all reasons
				 * that apply
				 */
				for (i = 1; i < _ACK_NUM_REASONS_; i++) {
					if (ack_reason & (1 << i))
						IncStat(ts_ackreason[i]);
				}
			}
			return 1;
		}
	}
}

/*
 * tp_rsyflush - drop all the packets on the reassembly queue.
 * Do this when closing the socket, or when somebody has changed
 * the space avaible in the receive socket (XXX).
 */
void
tp_rsyflush(tpcb)
	struct tp_pcb *tpcb;
{
	struct mbuf **mp;
	if (tpcb->tp_rsycnt) {
		for (mp = tpcb->tp_rsyq + tpcb->tp_maxlcredit;
		     --mp >= tpcb->tp_rsyq;)
			if (*mp) {
				tpcb->tp_rsycnt--;
				m_freem(*mp);
			}
		if (tpcb->tp_rsycnt) {
			printf("tp_rsyflush %p\n", tpcb);
			tpcb->tp_rsycnt = 0;
		}
	}
	free((caddr_t) tpcb->tp_rsyq, M_PCB);
	tpcb->tp_rsyq = 0;
}

void
tp_rsyset(tpcb)
	struct tp_pcb *tpcb;
{
	struct socket *so = tpcb->tp_sock;
	int             maxcredit = tpcb->tp_xtd_format ? 0xffff : 0xf;
	int             old_credit = tpcb->tp_maxlcredit;
	caddr_t         rsyq;

	tpcb->tp_maxlcredit = maxcredit = min(maxcredit,
					      (so->so_rcv.sb_hiwat + tpcb->tp_l_tpdusize) / tpcb->tp_l_tpdusize);

	if (old_credit == tpcb->tp_maxlcredit && tpcb->tp_rsyq != 0)
		return;
	maxcredit *= sizeof(struct mbuf *);
	if (tpcb->tp_rsyq)
		tp_rsyflush(tpcb);
	if ((rsyq = (caddr_t) malloc(maxcredit, M_PCB, M_NOWAIT)) != NULL)
		bzero(rsyq, maxcredit);
	tpcb->tp_rsyq = (struct mbuf **) rsyq;
}


void
tpsbcheck(tpcb, i)
	struct tp_pcb  *tpcb;
	int i;
{
	struct mbuf *n, *m;
	int    len = 0, mbcnt = 0, pktlen;
	struct sockbuf *sb = &tpcb->tp_sock->so_snd;

	for (n = sb->sb_mb; n; n = n->m_nextpkt) {
		if ((n->m_flags & M_PKTHDR) == 0)
			panic("tpsbcheck nohdr");
		pktlen = len + n->m_pkthdr.len;
		for (m = n; m; m = m->m_next) {
			len += m->m_len;
			mbcnt += MSIZE;
			if (m->m_flags & M_EXT)
				mbcnt += m->m_ext.ext_size;
		}
		if (len != pktlen) {
			printf("test %d; len %d != pktlen %d on mbuf %p\n",
			       i, len, pktlen, n);
			panic("tpsbcheck short");
		}
	}
	if (len != sb->sb_cc || mbcnt != sb->sb_mbcnt) {
		printf("test %d: cc %d != %ld || mbcnt %d != %ld\n", i, len, sb->sb_cc,
		       mbcnt, sb->sb_mbcnt);
		panic("tpsbcheck");
	}
}
