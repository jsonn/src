/*	$NetBSD: tcp_input.c,v 1.23.4.1 1996/12/10 18:21:07 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994
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
 *	@(#)tcp_input.c	8.5 (Berkeley) 4/10/94
 */

#ifndef TUBA_INCLUDE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

#include <machine/stdarg.h>

int	tcprexmtthresh = 3;
struct	tcpiphdr tcp_saveti;

extern u_long sb_max;

#endif /* TUBA_INCLUDE */
#define TCP_PAWS_IDLE	(24 * 24 * 60 * 60 * PR_SLOWHZ)

/* for modulo comparisons of timestamps */
#define TSTMP_LT(a,b)	((int)((a)-(b)) < 0)
#define TSTMP_GEQ(a,b)	((int)((a)-(b)) >= 0)


/*
 * Insert segment ti into reassembly queue of tcp with
 * control block tp.  Return TH_FIN if reassembly now includes
 * a segment with FIN.  The macro form does the common case inline
 * (segment is the next to be received on an established connection,
 * and the queue is empty), avoiding linkage into and removal
 * from the queue and repetition of various conversions.
 * Set DELACK for segments received in order, but ack immediately
 * when segments are out of order (so fast retransmit can work).
 */
#define	TCP_REASS(tp, ti, m, so, flags) { \
	if ((ti)->ti_seq == (tp)->rcv_nxt && \
	    (tp)->segq.lh_first == NULL && \
	    (tp)->t_state == TCPS_ESTABLISHED) { \
		if ((ti)->ti_flags & TH_PUSH) \
			tp->t_flags |= TF_ACKNOW; \
		else \
			tp->t_flags |= TF_DELACK; \
		(tp)->rcv_nxt += (ti)->ti_len; \
		flags = (ti)->ti_flags & TH_FIN; \
		tcpstat.tcps_rcvpack++;\
		tcpstat.tcps_rcvbyte += (ti)->ti_len;\
		sbappend(&(so)->so_rcv, (m)); \
		sorwakeup(so); \
	} else { \
		(flags) = tcp_reass((tp), (ti), (m)); \
		tp->t_flags |= TF_ACKNOW; \
	} \
}
#ifndef TUBA_INCLUDE

int
tcp_reass(tp, ti, m)
	register struct tcpcb *tp;
	register struct tcpiphdr *ti;
	struct mbuf *m;
{
	register struct ipqent *p, *q, *nq, *tiqe;
	struct socket *so = tp->t_inpcb->inp_socket;
	int flags;

	/*
	 * Call with ti==0 after become established to
	 * force pre-ESTABLISHED data up to user socket.
	 */
	if (ti == 0)
		goto present;

	/*
	 * Allocate a new queue entry, before we throw away any data.
	 * If we can't, just drop the packet.  XXX
	 */
	MALLOC(tiqe, struct ipqent *, sizeof (struct ipqent), M_IPQ, M_NOWAIT);
	if (tiqe == NULL) {
		tcpstat.tcps_rcvmemdrop++;
		m_freem(m);
		return (0);
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (p = NULL, q = tp->segq.lh_first; q != NULL;
	    p = q, q = q->ipqe_q.le_next)
		if (SEQ_GT(q->ipqe_tcp->ti_seq, ti->ti_seq))
			break;

	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (p != NULL) {
		register struct tcpiphdr *phdr = p->ipqe_tcp;
		register int i;

		/* conversion to int (in i) handles seq wraparound */
		i = phdr->ti_seq + phdr->ti_len - ti->ti_seq;
		if (i > 0) {
			if (i >= ti->ti_len) {
				tcpstat.tcps_rcvduppack++;
				tcpstat.tcps_rcvdupbyte += ti->ti_len;
				m_freem(m);
				FREE(tiqe, M_IPQ);
				return (0);
			}
			m_adj(m, i);
			ti->ti_len -= i;
			ti->ti_seq += i;
		}
	}
	tcpstat.tcps_rcvoopack++;
	tcpstat.tcps_rcvoobyte += ti->ti_len;

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	for (; q != NULL; q = nq) {
		register struct tcpiphdr *qhdr = q->ipqe_tcp;
		register int i = (ti->ti_seq + ti->ti_len) - qhdr->ti_seq;

		if (i <= 0)
			break;
		if (i < qhdr->ti_len) {
			qhdr->ti_seq += i;
			qhdr->ti_len -= i;
			m_adj(q->ipqe_m, i);
			break;
		}
		nq = q->ipqe_q.le_next;
		m_freem(q->ipqe_m);
		LIST_REMOVE(q, ipqe_q);
		FREE(q, M_IPQ);
	}

	/* Insert the new fragment queue entry into place. */
	tiqe->ipqe_m = m;
	tiqe->ipqe_tcp = ti;
	if (p == NULL) {
		LIST_INSERT_HEAD(&tp->segq, tiqe, ipqe_q);
	} else {
		LIST_INSERT_AFTER(p, tiqe, ipqe_q);
	}

present:
	/*
	 * Present data to user, advancing rcv_nxt through
	 * completed sequence space.
	 */
	if (TCPS_HAVEESTABLISHED(tp->t_state) == 0)
		return (0);
	q = tp->segq.lh_first;
	if (q == NULL || q->ipqe_tcp->ti_seq != tp->rcv_nxt)
		return (0);
	if (tp->t_state == TCPS_SYN_RECEIVED && q->ipqe_tcp->ti_len)
		return (0);
	do {
		tp->rcv_nxt += q->ipqe_tcp->ti_len;
		flags = q->ipqe_tcp->ti_flags & TH_FIN;

		nq = q->ipqe_q.le_next;
		LIST_REMOVE(q, ipqe_q);
		if (so->so_state & SS_CANTRCVMORE)
			m_freem(q->ipqe_m);
		else
			sbappend(&so->so_rcv, q->ipqe_m);
		FREE(q, M_IPQ);
		q = nq;
	} while (q != NULL && q->ipqe_tcp->ti_seq == tp->rcv_nxt);
	sorwakeup(so);
	return (flags);
}

/*
 * TCP input routine, follows pages 65-76 of the
 * protocol specification dated September, 1981 very closely.
 */
void
#if __STDC__
tcp_input(struct mbuf *m, ...)
#else
tcp_input(m, va_alist)
	register struct mbuf *m;
#endif
{
	register struct tcpiphdr *ti;
	register struct inpcb *inp;
	caddr_t optp = NULL;
	int optlen = 0;
	int len, tlen, off;
	register struct tcpcb *tp = 0;
	register int tiflags;
	struct socket *so = NULL;
	int todrop, acked, ourfinisacked, needoutput = 0;
	short ostate = 0;
	struct in_addr laddr;
	int dropsocket = 0;
	int iss = 0;
	u_long tiwin;
	u_int32_t ts_val, ts_ecr;
	int ts_present = 0;
	int iphlen;
	va_list ap;

	va_start(ap, m);
	iphlen = va_arg(ap, int);
	va_end(ap);

	tcpstat.tcps_rcvtotal++;
	/*
	 * Get IP and TCP header together in first mbuf.
	 * Note: IP leaves IP header in first mbuf.
	 */
	ti = mtod(m, struct tcpiphdr *);
	if (iphlen > sizeof (struct ip))
		ip_stripoptions(m, (struct mbuf *)0);
	if (m->m_len < sizeof (struct tcpiphdr)) {
		if ((m = m_pullup(m, sizeof (struct tcpiphdr))) == 0) {
			tcpstat.tcps_rcvshort++;
			return;
		}
		ti = mtod(m, struct tcpiphdr *);
	}

	/*
	 * Checksum extended TCP header and data.
	 */
	tlen = ((struct ip *)ti)->ip_len;
	len = sizeof (struct ip) + tlen;
	bzero(ti->ti_x1, sizeof ti->ti_x1);
	ti->ti_len = (u_int16_t)tlen;
	HTONS(ti->ti_len);
	if ((ti->ti_sum = in_cksum(m, len)) != 0) {
		tcpstat.tcps_rcvbadsum++;
		goto drop;
	}
#endif /* TUBA_INCLUDE */

	/*
	 * Check that TCP offset makes sense,
	 * pull out TCP options and adjust length.		XXX
	 */
	off = ti->ti_off << 2;
	if (off < sizeof (struct tcphdr) || off > tlen) {
		tcpstat.tcps_rcvbadoff++;
		goto drop;
	}
	tlen -= off;
	ti->ti_len = tlen;
	if (off > sizeof (struct tcphdr)) {
		if (m->m_len < sizeof(struct ip) + off) {
			if ((m = m_pullup(m, sizeof (struct ip) + off)) == 0) {
				tcpstat.tcps_rcvshort++;
				return;
			}
			ti = mtod(m, struct tcpiphdr *);
		}
		optlen = off - sizeof (struct tcphdr);
		optp = mtod(m, caddr_t) + sizeof (struct tcpiphdr);
		/* 
		 * Do quick retrieval of timestamp options ("options
		 * prediction?").  If timestamp is the only option and it's
		 * formatted as recommended in RFC 1323 appendix A, we
		 * quickly get the values now and not bother calling
		 * tcp_dooptions(), etc.
		 */
		if ((optlen == TCPOLEN_TSTAMP_APPA ||
		     (optlen > TCPOLEN_TSTAMP_APPA &&
			optp[TCPOLEN_TSTAMP_APPA] == TCPOPT_EOL)) &&
		     *(u_int32_t *)optp == htonl(TCPOPT_TSTAMP_HDR) &&
		     (ti->ti_flags & TH_SYN) == 0) {
			ts_present = 1;
			ts_val = ntohl(*(u_int32_t *)(optp + 4));
			ts_ecr = ntohl(*(u_int32_t *)(optp + 8));
			optp = NULL;	/* we've parsed the options */
		}
	}
	tiflags = ti->ti_flags;

	/*
	 * Convert TCP protocol specific fields to host format.
	 */
	NTOHL(ti->ti_seq);
	NTOHL(ti->ti_ack);
	NTOHS(ti->ti_win);
	NTOHS(ti->ti_urp);

	/*
	 * Locate pcb for segment.
	 */
findpcb:
	inp = in_pcbhashlookup(&tcbtable, ti->ti_src, ti->ti_sport,
	    ti->ti_dst, ti->ti_dport);
	if (inp == 0) {
		++tcpstat.tcps_pcbhashmiss;
		inp = in_pcblookup(&tcbtable, ti->ti_src, ti->ti_sport,
		    ti->ti_dst, ti->ti_dport, INPLOOKUP_WILDCARD);
		/*
		 * If the state is CLOSED (i.e., TCB does not exist) then
		 * all data in the incoming segment is discarded.
		 * If the TCB exists but is in CLOSED state, it is embryonic,
		 * but should either do a listen or a connect soon.
		 */
		if (inp == 0) {
			++tcpstat.tcps_noport;
			goto dropwithreset;
		}
	}

	tp = intotcpcb(inp);
	if (tp == 0)
		goto dropwithreset;
	if (tp->t_state == TCPS_CLOSED)
		goto drop;
	
	/* Unscale the window into a 32-bit value. */
	if ((tiflags & TH_SYN) == 0)
		tiwin = ti->ti_win << tp->snd_scale;
	else
		tiwin = ti->ti_win;

	so = inp->inp_socket;
	if (so->so_options & (SO_DEBUG|SO_ACCEPTCONN)) {
		if (so->so_options & SO_DEBUG) {
			ostate = tp->t_state;
			tcp_saveti = *ti;
		}
		if (so->so_options & SO_ACCEPTCONN) {
			so = sonewconn(so, 0);
			if (so == 0)
				goto drop;
			/*
			 * This is ugly, but ....
			 *
			 * Mark socket as temporary until we're
			 * committed to keeping it.  The code at
			 * ``drop'' and ``dropwithreset'' check the
			 * flag dropsocket to see if the temporary
			 * socket created here should be discarded.
			 * We mark the socket as discardable until
			 * we're committed to it below in TCPS_LISTEN.
			 */
			dropsocket++;
			inp = (struct inpcb *)so->so_pcb;
			inp->inp_laddr = ti->ti_dst;
			inp->inp_lport = ti->ti_dport;
			in_pcbrehash(inp);
#if BSD>=43
			inp->inp_options = ip_srcroute();
#endif
			tp = intotcpcb(inp);
			tp->t_state = TCPS_LISTEN;

			/* Compute proper scaling value from buffer space
			 */
			while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
			   TCP_MAXWIN << tp->request_r_scale < so->so_rcv.sb_hiwat)
				tp->request_r_scale++;
		}
	}

	/*
	 * Segment received on connection.
	 * Reset idle time and keep-alive timer.
	 */
	tp->t_idle = 0;
	tp->t_timer[TCPT_KEEP] = tcp_keepidle;

	/*
	 * Process options if not in LISTEN state,
	 * else do it below (after getting remote address).
	 */
	if (optp && tp->t_state != TCPS_LISTEN)
		tcp_dooptions(tp, optp, optlen, ti,
			&ts_present, &ts_val, &ts_ecr);

	/* 
	 * Header prediction: check for the two common cases
	 * of a uni-directional data xfer.  If the packet has
	 * no control flags, is in-sequence, the window didn't
	 * change and we're not retransmitting, it's a
	 * candidate.  If the length is zero and the ack moved
	 * forward, we're the sender side of the xfer.  Just
	 * free the data acked & wake any higher level process
	 * that was blocked waiting for space.  If the length
	 * is non-zero and the ack didn't move, we're the
	 * receiver side.  If we're getting packets in-order
	 * (the reassembly queue is empty), add the data to
	 * the socket buffer and note that we need a delayed ack.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	    (tiflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
	    (!ts_present || TSTMP_GEQ(ts_val, tp->ts_recent)) &&
	    ti->ti_seq == tp->rcv_nxt &&
	    tiwin && tiwin == tp->snd_wnd &&
	    tp->snd_nxt == tp->snd_max) {

		/* 
		 * If last ACK falls within this segment's sequence numbers,
		 *  record the timestamp.
		 */
		if (ts_present && SEQ_LEQ(ti->ti_seq, tp->last_ack_sent) &&
		   SEQ_LT(tp->last_ack_sent, ti->ti_seq + ti->ti_len)) {
			tp->ts_recent_age = tcp_now;
			tp->ts_recent = ts_val;
		}

		if (ti->ti_len == 0) {
			if (SEQ_GT(ti->ti_ack, tp->snd_una) &&
			    SEQ_LEQ(ti->ti_ack, tp->snd_max) &&
			    tp->snd_cwnd >= tp->snd_wnd &&
			    tp->t_dupacks < tcprexmtthresh) {
				/*
				 * this is a pure ack for outstanding data.
				 */
				++tcpstat.tcps_predack;
				if (ts_present)
					tcp_xmit_timer(tp, tcp_now-ts_ecr+1);
				else if (tp->t_rtt &&
					    SEQ_GT(ti->ti_ack, tp->t_rtseq))
					tcp_xmit_timer(tp, tp->t_rtt);
				acked = ti->ti_ack - tp->snd_una;
				tcpstat.tcps_rcvackpack++;
				tcpstat.tcps_rcvackbyte += acked;
				sbdrop(&so->so_snd, acked);
				tp->snd_una = ti->ti_ack;
				m_freem(m);

				/*
				 * If all outstanding data are acked, stop
				 * retransmit timer, otherwise restart timer
				 * using current (possibly backed-off) value.
				 * If process is waiting for space,
				 * wakeup/selwakeup/signal.  If data
				 * are ready to send, let tcp_output
				 * decide between more output or persist.
				 */
				if (tp->snd_una == tp->snd_max)
					tp->t_timer[TCPT_REXMT] = 0;
				else if (tp->t_timer[TCPT_PERSIST] == 0)
					tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

				if (sb_notify(&so->so_snd))
					sowwakeup(so);
				if (so->so_snd.sb_cc)
					(void) tcp_output(tp);
				return;
			}
		} else if (ti->ti_ack == tp->snd_una &&
		    tp->segq.lh_first == NULL &&
		    ti->ti_len <= sbspace(&so->so_rcv)) {
			/*
			 * this is a pure, in-sequence data packet
			 * with nothing on the reassembly queue and
			 * we have enough buffer space to take it.
			 */
			++tcpstat.tcps_preddat;
			tp->rcv_nxt += ti->ti_len;
			tcpstat.tcps_rcvpack++;
			tcpstat.tcps_rcvbyte += ti->ti_len;
			/*
			 * Drop TCP, IP headers and TCP options then add data
			 * to socket buffer.
			 */
			m->m_data += sizeof(struct tcpiphdr)+off-sizeof(struct tcphdr);
			m->m_len -= sizeof(struct tcpiphdr)+off-sizeof(struct tcphdr);
			sbappend(&so->so_rcv, m);
			sorwakeup(so);
			if (ti->ti_flags & TH_PUSH)
				tp->t_flags |= TF_ACKNOW;
			else
				tp->t_flags |= TF_DELACK;
			return;
		}
	}

	/*
	 * Drop TCP, IP headers and TCP options.
	 */
	m->m_data += sizeof(struct tcpiphdr)+off-sizeof(struct tcphdr);
	m->m_len  -= sizeof(struct tcpiphdr)+off-sizeof(struct tcphdr);

	/*
	 * Calculate amount of space in receive window,
	 * and then do TCP input processing.
	 * Receive window is amount of space in rcv queue,
	 * but not less than advertised window.
	 */
	{ int win;

	win = sbspace(&so->so_rcv);
	if (win < 0)
		win = 0;
	tp->rcv_wnd = max(win, (int)(tp->rcv_adv - tp->rcv_nxt));
	}

	switch (tp->t_state) {

	/*
	 * If the state is LISTEN then ignore segment if it contains an RST.
	 * If the segment contains an ACK then it is bad and send a RST.
	 * If it does not contain a SYN then it is not interesting; drop it.
	 * Don't bother responding if the destination was a broadcast.
	 * Otherwise initialize tp->rcv_nxt, and tp->irs, select an initial
	 * tp->iss, and send a segment:
	 *     <SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
	 * Also initialize tp->snd_nxt to tp->iss+1 and tp->snd_una to tp->iss.
	 * Fill in remote peer address fields if not previously specified.
	 * Enter SYN_RECEIVED state, and process any other fields of this
	 * segment in this state.
	 */
	case TCPS_LISTEN: {
		struct mbuf *am;
		register struct sockaddr_in *sin;

		if (tiflags & TH_RST)
			goto drop;
		if (tiflags & TH_ACK)
			goto dropwithreset;
		if ((tiflags & TH_SYN) == 0)
			goto drop;
		/*
		 * RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN
		 * in_broadcast() should never return true on a received
		 * packet with M_BCAST not set.
		 */
		if (m->m_flags & (M_BCAST|M_MCAST) ||
		    IN_MULTICAST(ti->ti_dst.s_addr))
			goto drop;
		am = m_get(M_DONTWAIT, MT_SONAME);	/* XXX */
		if (am == NULL)
			goto drop;
		am->m_len = sizeof (struct sockaddr_in);
		sin = mtod(am, struct sockaddr_in *);
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr = ti->ti_src;
		sin->sin_port = ti->ti_sport;
		bzero((caddr_t)sin->sin_zero, sizeof(sin->sin_zero));
		laddr = inp->inp_laddr;
		if (inp->inp_laddr.s_addr == INADDR_ANY)
			inp->inp_laddr = ti->ti_dst;
		if (in_pcbconnect(inp, am)) {
			inp->inp_laddr = laddr;
			(void) m_free(am);
			goto drop;
		}
		(void) m_free(am);
		tp->t_template = tcp_template(tp);
		if (tp->t_template == 0) {
			tp = tcp_drop(tp, ENOBUFS);
			dropsocket = 0;		/* socket is already gone */
			goto drop;
		}
		if (optp)
			tcp_dooptions(tp, optp, optlen, ti,
				&ts_present, &ts_val, &ts_ecr);
		if (iss)
			tp->iss = iss;
		else
			tp->iss = tcp_iss;
		tcp_iss += TCP_ISSINCR/2;
		tp->irs = ti->ti_seq;
		tcp_sendseqinit(tp);
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
		tp->t_state = TCPS_SYN_RECEIVED;
		tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
		dropsocket = 0;		/* committed to socket */
		tcpstat.tcps_accepts++;
		goto trimthenstep6;
		}

	/*
	 * If the state is SYN_SENT:
	 *	if seg contains an ACK, but not for our SYN, drop the input.
	 *	if seg contains a RST, then drop the connection.
	 *	if seg does not contain SYN, then drop it.
	 * Otherwise this is an acceptable SYN segment
	 *	initialize tp->rcv_nxt and tp->irs
	 *	if seg contains ack then advance tp->snd_una
	 *	if SYN has been acked change to ESTABLISHED else SYN_RCVD state
	 *	arrange for segment to be acked (eventually)
	 *	continue processing rest of data/controls, beginning with URG
	 */
	case TCPS_SYN_SENT:
		if ((tiflags & TH_ACK) &&
		    (SEQ_LEQ(ti->ti_ack, tp->iss) ||
		     SEQ_GT(ti->ti_ack, tp->snd_max)))
			goto dropwithreset;
		if (tiflags & TH_RST) {
			if (tiflags & TH_ACK)
				tp = tcp_drop(tp, ECONNREFUSED);
			goto drop;
		}
		if ((tiflags & TH_SYN) == 0)
			goto drop;
		if (tiflags & TH_ACK) {
			tp->snd_una = ti->ti_ack;
			if (SEQ_LT(tp->snd_nxt, tp->snd_una))
				tp->snd_nxt = tp->snd_una;
		}
		tp->t_timer[TCPT_REXMT] = 0;
		tp->irs = ti->ti_seq;
		tcp_rcvseqinit(tp);
		tp->t_flags |= TF_ACKNOW;
		if (tiflags & TH_ACK && SEQ_GT(tp->snd_una, tp->iss)) {
			tcpstat.tcps_connects++;
			soisconnected(so);
			tp->t_state = TCPS_ESTABLISHED;
			/* Do window scaling on this connection? */
			if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
				(TF_RCVD_SCALE|TF_REQ_SCALE)) {
				tp->snd_scale = tp->requested_s_scale;
				tp->rcv_scale = tp->request_r_scale;
			}
			(void) tcp_reass(tp, (struct tcpiphdr *)0,
				(struct mbuf *)0);
			/*
			 * if we didn't have to retransmit the SYN,
			 * use its rtt as our initial srtt & rtt var.
			 */
			if (tp->t_rtt)
				tcp_xmit_timer(tp, tp->t_rtt);
		} else
			tp->t_state = TCPS_SYN_RECEIVED;

trimthenstep6:
		/*
		 * Advance ti->ti_seq to correspond to first data byte.
		 * If data, trim to stay within window,
		 * dropping FIN if necessary.
		 */
		ti->ti_seq++;
		if (ti->ti_len > tp->rcv_wnd) {
			todrop = ti->ti_len - tp->rcv_wnd;
			m_adj(m, -todrop);
			ti->ti_len = tp->rcv_wnd;
			tiflags &= ~TH_FIN;
			tcpstat.tcps_rcvpackafterwin++;
			tcpstat.tcps_rcvbyteafterwin += todrop;
		}
		tp->snd_wl1 = ti->ti_seq - 1;
		tp->rcv_up = ti->ti_seq;
		goto step6;
	}

	/*
	 * States other than LISTEN or SYN_SENT.
	 * First check timestamp, if present.
	 * Then check that at least some bytes of segment are within 
	 * receive window.  If segment begins before rcv_nxt,
	 * drop leading data (and SYN); if nothing left, just ack.
	 * 
	 * RFC 1323 PAWS: If we have a timestamp reply on this segment
	 * and it's less than ts_recent, drop it.
	 */
	if (ts_present && (tiflags & TH_RST) == 0 && tp->ts_recent &&
	    TSTMP_LT(ts_val, tp->ts_recent)) {

		/* Check to see if ts_recent is over 24 days old.  */
		if ((int)(tcp_now - tp->ts_recent_age) > TCP_PAWS_IDLE) {
			/*
			 * Invalidate ts_recent.  If this segment updates
			 * ts_recent, the age will be reset later and ts_recent
			 * will get a valid value.  If it does not, setting
			 * ts_recent to zero will at least satisfy the
			 * requirement that zero be placed in the timestamp
			 * echo reply when ts_recent isn't valid.  The
			 * age isn't reset until we get a valid ts_recent
			 * because we don't want out-of-order segments to be
			 * dropped when ts_recent is old.
			 */
			tp->ts_recent = 0;
		} else {
			tcpstat.tcps_rcvduppack++;
			tcpstat.tcps_rcvdupbyte += ti->ti_len;
			tcpstat.tcps_pawsdrop++;
			goto dropafterack;
		}
	}

	todrop = tp->rcv_nxt - ti->ti_seq;
	if (todrop > 0) {
		if (tiflags & TH_SYN) {
			tiflags &= ~TH_SYN;
			ti->ti_seq++;
			if (ti->ti_urp > 1) 
				ti->ti_urp--;
			else
				tiflags &= ~TH_URG;
			todrop--;
		}
		if (todrop >= ti->ti_len) {
			/*
			 * Any valid FIN must be to the left of the
			 * window.  At this point, FIN must be a
			 * duplicate or out-of-sequence, so drop it.
			 */
			tiflags &= ~TH_FIN;
			/*
			 * Send ACK to resynchronize, and drop any data,
			 * but keep on processing for RST or ACK.
			 */
			tp->t_flags |= TF_ACKNOW;
			tcpstat.tcps_rcvdupbyte += todrop = ti->ti_len;
			tcpstat.tcps_rcvduppack++;
		} else {
			tcpstat.tcps_rcvpartduppack++;
			tcpstat.tcps_rcvpartdupbyte += todrop;
		}
		m_adj(m, todrop);
		ti->ti_seq += todrop;
		ti->ti_len -= todrop;
		if (ti->ti_urp > todrop)
			ti->ti_urp -= todrop;
		else {
			tiflags &= ~TH_URG;
			ti->ti_urp = 0;
		}
	}

	/*
	 * If new data are received on a connection after the
	 * user processes are gone, then RST the other end.
	 */
	if ((so->so_state & SS_NOFDREF) &&
	    tp->t_state > TCPS_CLOSE_WAIT && ti->ti_len) {
		tp = tcp_close(tp);
		tcpstat.tcps_rcvafterclose++;
		goto dropwithreset;
	}

	/*
	 * If segment ends after window, drop trailing data
	 * (and PUSH and FIN); if nothing left, just ACK.
	 */
	todrop = (ti->ti_seq+ti->ti_len) - (tp->rcv_nxt+tp->rcv_wnd);
	if (todrop > 0) {
		tcpstat.tcps_rcvpackafterwin++;
		if (todrop >= ti->ti_len) {
			tcpstat.tcps_rcvbyteafterwin += ti->ti_len;
			/*
			 * If a new connection request is received
			 * while in TIME_WAIT, drop the old connection
			 * and start over if the sequence numbers
			 * are above the previous ones.
			 */
			if (tiflags & TH_SYN &&
			    tp->t_state == TCPS_TIME_WAIT &&
			    SEQ_GT(ti->ti_seq, tp->rcv_nxt)) {
				iss = tp->rcv_nxt + TCP_ISSINCR;
				tp = tcp_close(tp);
				goto findpcb;
			}
			/*
			 * If window is closed can only take segments at
			 * window edge, and have to drop data and PUSH from
			 * incoming segments.  Continue processing, but
			 * remember to ack.  Otherwise, drop segment
			 * and ack.
			 */
			if (tp->rcv_wnd == 0 && ti->ti_seq == tp->rcv_nxt) {
				tp->t_flags |= TF_ACKNOW;
				tcpstat.tcps_rcvwinprobe++;
			} else
				goto dropafterack;
		} else
			tcpstat.tcps_rcvbyteafterwin += todrop;
		m_adj(m, -todrop);
		ti->ti_len -= todrop;
		tiflags &= ~(TH_PUSH|TH_FIN);
	}

	/*
	 * If last ACK falls within this segment's sequence numbers,
	 * record its timestamp.
	 */
	if (ts_present && SEQ_LEQ(ti->ti_seq, tp->last_ack_sent) &&
	    SEQ_LT(tp->last_ack_sent, ti->ti_seq + ti->ti_len +
		   ((tiflags & (TH_SYN|TH_FIN)) != 0))) {
		tp->ts_recent_age = tcp_now;
		tp->ts_recent = ts_val;
	}

	/*
	 * If the RST bit is set examine the state:
	 *    SYN_RECEIVED STATE:
	 *	If passive open, return to LISTEN state.
	 *	If active open, inform user that connection was refused.
	 *    ESTABLISHED, FIN_WAIT_1, FIN_WAIT2, CLOSE_WAIT STATES:
	 *	Inform user that connection was reset, and close tcb.
	 *    CLOSING, LAST_ACK, TIME_WAIT STATES
	 *	Close the tcb.
	 */
	if (tiflags&TH_RST) switch (tp->t_state) {

	case TCPS_SYN_RECEIVED:
		so->so_error = ECONNREFUSED;
		goto close;

	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
		so->so_error = ECONNRESET;
	close:
		tp->t_state = TCPS_CLOSED;
		tcpstat.tcps_drops++;
		tp = tcp_close(tp);
		goto drop;

	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:
		tp = tcp_close(tp);
		goto drop;
	}

	/*
	 * If a SYN is in the window, then this is an
	 * error and we send an RST and drop the connection.
	 */
	if (tiflags & TH_SYN) {
		tp = tcp_drop(tp, ECONNRESET);
		goto dropwithreset;
	}

	/*
	 * If the ACK bit is off we drop the segment and return.
	 */
	if ((tiflags & TH_ACK) == 0)
		goto drop;
	
	/*
	 * Ack processing.
	 */
	switch (tp->t_state) {

	/*
	 * In SYN_RECEIVED state if the ack ACKs our SYN then enter
	 * ESTABLISHED state and continue processing, otherwise
	 * send an RST.
	 */
	case TCPS_SYN_RECEIVED:
		if (SEQ_GT(tp->snd_una, ti->ti_ack) ||
		    SEQ_GT(ti->ti_ack, tp->snd_max))
			goto dropwithreset;
		tcpstat.tcps_connects++;
		soisconnected(so);
		tp->t_state = TCPS_ESTABLISHED;
		/* Do window scaling? */
		if ((tp->t_flags & (TF_RCVD_SCALE|TF_REQ_SCALE)) ==
			(TF_RCVD_SCALE|TF_REQ_SCALE)) {
			tp->snd_scale = tp->requested_s_scale;
			tp->rcv_scale = tp->request_r_scale;
		}
		(void) tcp_reass(tp, (struct tcpiphdr *)0, (struct mbuf *)0);
		tp->snd_wl1 = ti->ti_seq - 1;
		/* fall into ... */

	/*
	 * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	 * ACKs.  If the ack is in the range
	 *	tp->snd_una < ti->ti_ack <= tp->snd_max
	 * then advance tp->snd_una to ti->ti_ack and drop
	 * data from the retransmission queue.  If this ACK reflects
	 * more up to date window information we update our window information.
	 */
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:

		if (SEQ_LEQ(ti->ti_ack, tp->snd_una)) {
			if (ti->ti_len == 0 && tiwin == tp->snd_wnd) {
				tcpstat.tcps_rcvdupack++;
				/*
				 * If we have outstanding data (other than
				 * a window probe), this is a completely
				 * duplicate ack (ie, window info didn't
				 * change), the ack is the biggest we've
				 * seen and we've seen exactly our rexmt
				 * threshhold of them, assume a packet
				 * has been dropped and retransmit it.
				 * Kludge snd_nxt & the congestion
				 * window so we send only this one
				 * packet.
				 *
				 * We know we're losing at the current
				 * window size so do congestion avoidance
				 * (set ssthresh to half the current window
				 * and pull our congestion window back to
				 * the new ssthresh).
				 *
				 * Dup acks mean that packets have left the
				 * network (they're now cached at the receiver) 
				 * so bump cwnd by the amount in the receiver
				 * to keep a constant cwnd packets in the
				 * network.
				 */
				if (tp->t_timer[TCPT_REXMT] == 0 ||
				    ti->ti_ack != tp->snd_una)
					tp->t_dupacks = 0;
				else if (++tp->t_dupacks == tcprexmtthresh) {
					tcp_seq onxt = tp->snd_nxt;
					u_int win =
					    min(tp->snd_wnd, tp->snd_cwnd) / 2 /
						tp->t_maxseg;

					if (win < 2)
						win = 2;
					tp->snd_ssthresh = win * tp->t_maxseg;
					tp->t_timer[TCPT_REXMT] = 0;
					tp->t_rtt = 0;
					tp->snd_nxt = ti->ti_ack;
					tp->snd_cwnd = tp->t_maxseg;
					(void) tcp_output(tp);
					tp->snd_cwnd = tp->snd_ssthresh +
					       tp->t_maxseg * tp->t_dupacks;
					if (SEQ_GT(onxt, tp->snd_nxt))
						tp->snd_nxt = onxt;
					goto drop;
				} else if (tp->t_dupacks > tcprexmtthresh) {
					tp->snd_cwnd += tp->t_maxseg;
					(void) tcp_output(tp);
					goto drop;
				}
			} else
				tp->t_dupacks = 0;
			break;
		}
		/*
		 * If the congestion window was inflated to account
		 * for the other side's cached packets, retract it.
		 */
		if (tp->t_dupacks >= tcprexmtthresh &&
		    tp->snd_cwnd > tp->snd_ssthresh)
			tp->snd_cwnd = tp->snd_ssthresh;
		tp->t_dupacks = 0;
		if (SEQ_GT(ti->ti_ack, tp->snd_max)) {
			tcpstat.tcps_rcvacktoomuch++;
			goto dropafterack;
		}
		acked = ti->ti_ack - tp->snd_una;
		tcpstat.tcps_rcvackpack++;
		tcpstat.tcps_rcvackbyte += acked;

		/*
		 * If we have a timestamp reply, update smoothed
		 * round trip time.  If no timestamp is present but
		 * transmit timer is running and timed sequence
		 * number was acked, update smoothed round trip time.
		 * Since we now have an rtt measurement, cancel the
		 * timer backoff (cf., Phil Karn's retransmit alg.).
		 * Recompute the initial retransmit timer.
		 */
		if (ts_present)
			tcp_xmit_timer(tp, tcp_now-ts_ecr+1);
		else if (tp->t_rtt && SEQ_GT(ti->ti_ack, tp->t_rtseq))
			tcp_xmit_timer(tp,tp->t_rtt);

		/*
		 * If all outstanding data is acked, stop retransmit
		 * timer and remember to restart (more output or persist).
		 * If there is more data to be acked, restart retransmit
		 * timer, using current (possibly backed-off) value.
		 */
		if (ti->ti_ack == tp->snd_max) {
			tp->t_timer[TCPT_REXMT] = 0;
			needoutput = 1;
		} else if (tp->t_timer[TCPT_PERSIST] == 0)
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
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
		register u_int cw = tp->snd_cwnd;
		register u_int incr = tp->t_maxseg;

		if (cw > tp->snd_ssthresh)
			incr = incr * incr / cw;
		tp->snd_cwnd = min(cw + incr, TCP_MAXWIN<<tp->snd_scale);
		}
		if (acked > so->so_snd.sb_cc) {
			tp->snd_wnd -= so->so_snd.sb_cc;
			sbdrop(&so->so_snd, (int)so->so_snd.sb_cc);
			ourfinisacked = 1;
		} else {
			sbdrop(&so->so_snd, acked);
			tp->snd_wnd -= acked;
			ourfinisacked = 0;
		}
		if (sb_notify(&so->so_snd))
			sowwakeup(so);
		tp->snd_una = ti->ti_ack;
		if (SEQ_LT(tp->snd_nxt, tp->snd_una))
			tp->snd_nxt = tp->snd_una;

		switch (tp->t_state) {

		/*
		 * In FIN_WAIT_1 STATE in addition to the processing
		 * for the ESTABLISHED state if our FIN is now acknowledged
		 * then enter FIN_WAIT_2.
		 */
		case TCPS_FIN_WAIT_1:
			if (ourfinisacked) {
				/*
				 * If we can't receive any more
				 * data, then closing user can proceed.
				 * Starting the timer is contrary to the
				 * specification, but if we don't get a FIN
				 * we'll hang forever.
				 */
				if (so->so_state & SS_CANTRCVMORE) {
					soisdisconnected(so);
					tp->t_timer[TCPT_2MSL] = tcp_maxidle;
				}
				tp->t_state = TCPS_FIN_WAIT_2;
			}
			break;

	 	/*
		 * In CLOSING STATE in addition to the processing for
		 * the ESTABLISHED state if the ACK acknowledges our FIN
		 * then enter the TIME-WAIT state, otherwise ignore
		 * the segment.
		 */
		case TCPS_CLOSING:
			if (ourfinisacked) {
				tp->t_state = TCPS_TIME_WAIT;
				tcp_canceltimers(tp);
				tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
				soisdisconnected(so);
			}
			break;

		/*
		 * In LAST_ACK, we may still be waiting for data to drain
		 * and/or to be acked, as well as for the ack of our FIN.
		 * If our FIN is now acknowledged, delete the TCB,
		 * enter the closed state and return.
		 */
		case TCPS_LAST_ACK:
			if (ourfinisacked) {
				tp = tcp_close(tp);
				goto drop;
			}
			break;

		/*
		 * In TIME_WAIT state the only thing that should arrive
		 * is a retransmission of the remote FIN.  Acknowledge
		 * it and restart the finack timer.
		 */
		case TCPS_TIME_WAIT:
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			goto dropafterack;
		}
	}

step6:
	/*
	 * Update window information.
	 * Don't look at window if no ACK: TAC's send garbage on first SYN.
	 */
	if (((tiflags & TH_ACK) && SEQ_LT(tp->snd_wl1, ti->ti_seq)) ||
	    (tp->snd_wl1 == ti->ti_seq && SEQ_LT(tp->snd_wl2, ti->ti_ack)) ||
	    (tp->snd_wl2 == ti->ti_ack && tiwin > tp->snd_wnd)) {
		/* keep track of pure window updates */
		if (ti->ti_len == 0 &&
		    tp->snd_wl2 == ti->ti_ack && tiwin > tp->snd_wnd)
			tcpstat.tcps_rcvwinupd++;
		tp->snd_wnd = tiwin;
		tp->snd_wl1 = ti->ti_seq;
		tp->snd_wl2 = ti->ti_ack;
		if (tp->snd_wnd > tp->max_sndwnd)
			tp->max_sndwnd = tp->snd_wnd;
		needoutput = 1;
	}

	/*
	 * Process segments with URG.
	 */
	if ((tiflags & TH_URG) && ti->ti_urp &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		/*
		 * This is a kludge, but if we receive and accept
		 * random urgent pointers, we'll crash in
		 * soreceive.  It's hard to imagine someone
		 * actually wanting to send this much urgent data.
		 */
		if (ti->ti_urp + so->so_rcv.sb_cc > sb_max) {
			ti->ti_urp = 0;			/* XXX */
			tiflags &= ~TH_URG;		/* XXX */
			goto dodata;			/* XXX */
		}
		/*
		 * If this segment advances the known urgent pointer,
		 * then mark the data stream.  This should not happen
		 * in CLOSE_WAIT, CLOSING, LAST_ACK or TIME_WAIT STATES since
		 * a FIN has been received from the remote side. 
		 * In these states we ignore the URG.
		 *
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section as the original 
		 * spec states (in one of two places).
		 */
		if (SEQ_GT(ti->ti_seq+ti->ti_urp, tp->rcv_up)) {
			tp->rcv_up = ti->ti_seq + ti->ti_urp;
			so->so_oobmark = so->so_rcv.sb_cc +
			    (tp->rcv_up - tp->rcv_nxt) - 1;
			if (so->so_oobmark == 0)
				so->so_state |= SS_RCVATMARK;
			sohasoutofband(so);
			tp->t_oobflags &= ~(TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		}
		/*
		 * Remove out of band data so doesn't get presented to user.
		 * This can happen independent of advancing the URG pointer,
		 * but if two URG's are pending at once, some out-of-band
		 * data may creep in... ick.
		 */
		if (ti->ti_urp <= (u_int16_t) ti->ti_len
#ifdef SO_OOBINLINE
		     && (so->so_options & SO_OOBINLINE) == 0
#endif
		     )
			tcp_pulloutofband(so, ti, m);
	} else
		/*
		 * If no out of band data is expected,
		 * pull receive urgent pointer along
		 * with the receive window.
		 */
		if (SEQ_GT(tp->rcv_nxt, tp->rcv_up))
			tp->rcv_up = tp->rcv_nxt;
dodata:							/* XXX */

	/*
	 * Process the segment text, merging it into the TCP sequencing queue,
	 * and arranging for acknowledgment of receipt if necessary.
	 * This process logically involves adjusting tp->rcv_wnd as data
	 * is presented to the user (this happens in tcp_usrreq.c,
	 * case PRU_RCVD).  If a FIN has already been received on this
	 * connection then we just ignore the text.
	 */
	if ((ti->ti_len || (tiflags & TH_FIN)) &&
	    TCPS_HAVERCVDFIN(tp->t_state) == 0) {
		TCP_REASS(tp, ti, m, so, tiflags);
		/*
		 * Note the amount of data that peer has sent into
		 * our window, in order to estimate the sender's
		 * buffer size.
		 */
		len = so->so_rcv.sb_hiwat - (tp->rcv_adv - tp->rcv_nxt);
	} else {
		m_freem(m);
		tiflags &= ~TH_FIN;
	}

	/*
	 * If FIN is received ACK the FIN and let the user know
	 * that the connection is closing.  Ignore a FIN received before
	 * the connection is fully established.
	 */
	if ((tiflags & TH_FIN) && TCPS_HAVEESTABLISHED(tp->t_state)) {
		if (TCPS_HAVERCVDFIN(tp->t_state) == 0) {
			socantrcvmore(so);
			tp->t_flags |= TF_ACKNOW;
			tp->rcv_nxt++;
		}
		switch (tp->t_state) {

	 	/*
		 * In ESTABLISHED STATE enter the CLOSE_WAIT state.
		 */
		case TCPS_ESTABLISHED:
			tp->t_state = TCPS_CLOSE_WAIT;
			break;

	 	/*
		 * If still in FIN_WAIT_1 STATE FIN has not been acked so
		 * enter the CLOSING state.
		 */
		case TCPS_FIN_WAIT_1:
			tp->t_state = TCPS_CLOSING;
			break;

	 	/*
		 * In FIN_WAIT_2 state enter the TIME_WAIT state,
		 * starting the time-wait timer, turning off the other 
		 * standard timers.
		 */
		case TCPS_FIN_WAIT_2:
			tp->t_state = TCPS_TIME_WAIT;
			tcp_canceltimers(tp);
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			soisdisconnected(so);
			break;

		/*
		 * In TIME_WAIT state restart the 2 MSL time_wait timer.
		 */
		case TCPS_TIME_WAIT:
			tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
			break;
		}
	}
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_INPUT, ostate, tp, &tcp_saveti, 0);

	/*
	 * Return any desired output.
	 */
	if (needoutput || (tp->t_flags & TF_ACKNOW))
		(void) tcp_output(tp);
	return;

dropafterack:
	/*
	 * Generate an ACK dropping incoming segment if it occupies
	 * sequence space, where the ACK reflects our state.
	 */
	if (tiflags & TH_RST)
		goto drop;
	m_freem(m);
	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp);
	return;

dropwithreset:
	/*
	 * Generate a RST, dropping incoming segment.
	 * Make ACK acceptable to originator of segment.
	 * Don't bother to respond if destination was broadcast/multicast.
	 */
	if ((tiflags & TH_RST) || m->m_flags & (M_BCAST|M_MCAST) ||
	    IN_MULTICAST(ti->ti_dst.s_addr))
		goto drop;
	if (tiflags & TH_ACK)
		tcp_respond(tp, ti, m, (tcp_seq)0, ti->ti_ack, TH_RST);
	else {
		if (tiflags & TH_SYN)
			ti->ti_len++;
		tcp_respond(tp, ti, m, ti->ti_seq+ti->ti_len, (tcp_seq)0,
		    TH_RST|TH_ACK);
	}
	/* destroy temporarily created socket */
	if (dropsocket)
		(void) soabort(so);
	return;

drop:
	/*
	 * Drop space held by incoming segment and return.
	 */
	if (tp && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_DROP, ostate, tp, &tcp_saveti, 0);
	m_freem(m);
	/* destroy temporarily created socket */
	if (dropsocket)
		(void) soabort(so);
	return;
#ifndef TUBA_INCLUDE
}

void
tcp_dooptions(tp, cp, cnt, ti, ts_present, ts_val, ts_ecr)
	struct tcpcb *tp;
	u_char *cp;
	int cnt;
	struct tcpiphdr *ti;
	int *ts_present;
	u_int32_t *ts_val, *ts_ecr;
{
	u_int16_t mss;
	int opt, optlen;

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[1];
			if (optlen <= 0)
				break;
		}
		switch (opt) {

		default:
			continue;

		case TCPOPT_MAXSEG:
			if (optlen != TCPOLEN_MAXSEG)
				continue;
			if (!(ti->ti_flags & TH_SYN))
				continue;
			bcopy((char *) cp + 2, (char *) &mss, sizeof(mss));
			NTOHS(mss);
			(void) tcp_mss(tp, mss);	/* sets t_maxseg */
			break;

		case TCPOPT_WINDOW:
			if (optlen != TCPOLEN_WINDOW)
				continue;
			if (!(ti->ti_flags & TH_SYN))
				continue;
			tp->t_flags |= TF_RCVD_SCALE;
			tp->requested_s_scale = min(cp[2], TCP_MAX_WINSHIFT);
			break;

		case TCPOPT_TIMESTAMP:
			if (optlen != TCPOLEN_TIMESTAMP)
				continue;
			*ts_present = 1;
			bcopy((char *)cp + 2, (char *) ts_val, sizeof(*ts_val));
			NTOHL(*ts_val);
			bcopy((char *)cp + 6, (char *) ts_ecr, sizeof(*ts_ecr));
			NTOHL(*ts_ecr);

			/* 
			 * A timestamp received in a SYN makes
			 * it ok to send timestamp requests and replies.
			 */
			if (ti->ti_flags & TH_SYN) {
				tp->t_flags |= TF_RCVD_TSTMP;
				tp->ts_recent = *ts_val;
				tp->ts_recent_age = tcp_now;
			}
			break;
		}
	}
}

/*
 * Pull out of band byte out of a segment so
 * it doesn't appear in the user's data queue.
 * It is still reflected in the segment length for
 * sequencing purposes.
 */
void
tcp_pulloutofband(so, ti, m)
	struct socket *so;
	struct tcpiphdr *ti;
	register struct mbuf *m;
{
	int cnt = ti->ti_urp - 1;
	
	while (cnt >= 0) {
		if (m->m_len > cnt) {
			char *cp = mtod(m, caddr_t) + cnt;
			struct tcpcb *tp = sototcpcb(so);

			tp->t_iobc = *cp;
			tp->t_oobflags |= TCPOOB_HAVEDATA;
			bcopy(cp+1, cp, (unsigned)(m->m_len - cnt - 1));
			m->m_len--;
			return;
		}
		cnt -= m->m_len;
		m = m->m_next;
		if (m == 0)
			break;
	}
	panic("tcp_pulloutofband");
}

/*
 * Collect new round-trip time estimate
 * and update averages and current timeout.
 */
void
tcp_xmit_timer(tp, rtt)
	register struct tcpcb *tp;
	short rtt;
{
	register short delta;

	tcpstat.tcps_rttupdated++;
	--rtt;
	if (tp->t_srtt != 0) {
		/*
		 * srtt is stored as fixed point with 3 bits after the
		 * binary point (i.e., scaled by 8).  The following magic
		 * is equivalent to the smoothing algorithm in rfc793 with
		 * an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
		 * point).  Adjust rtt to origin 0.
		 */
		delta = (rtt << 2) - (tp->t_srtt >> TCP_RTT_SHIFT);
		if ((tp->t_srtt += delta) <= 0)
			tp->t_srtt = 1 << 2;
		/*
		 * We accumulate a smoothed rtt variance (actually, a
		 * smoothed mean difference), then set the retransmit
		 * timer to smoothed rtt + 4 times the smoothed variance.
		 * rttvar is stored as fixed point with 2 bits after the
		 * binary point (scaled by 4).  The following is
		 * equivalent to rfc793 smoothing with an alpha of .75
		 * (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
		 * rfc793's wired-in beta.
		 */
		if (delta < 0)
			delta = -delta;
		delta -= (tp->t_rttvar >> TCP_RTTVAR_SHIFT);
		if ((tp->t_rttvar += delta) <= 0)
			tp->t_rttvar = 1 << 2;
	} else {
		/* 
		 * No rtt measurement yet - use the unsmoothed rtt.
		 * Set the variance to half the rtt (so our first
		 * retransmit happens at 3*rtt).
		 */
		tp->t_srtt = rtt << (TCP_RTT_SHIFT + 2);
		tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT + 2 - 1);
	}
	tp->t_rtt = 0;
	tp->t_rxtshift = 0;

	/*
	 * the retransmit should happen at rtt + 4 * rttvar.
	 * Because of the way we do the smoothing, srtt and rttvar
	 * will each average +1/2 tick of bias.  When we compute
	 * the retransmit timer, we want 1/2 tick of rounding and
	 * 1 extra tick because of +-1/2 tick uncertainty in the
	 * firing of the timer.  The bias will give us exactly the
	 * 1.5 tick we need.  But, because the bias is
	 * statistical, we have to test that we don't drop below
	 * the minimum feasible timer (which is 2 ticks).
	 */
	TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
	    rtt + 2, TCPTV_REXMTMAX);
	
	/*
	 * We received an ack for a packet that wasn't retransmitted;
	 * it is probably safe to discard any error indications we've
	 * received recently.  This isn't quite right, but close enough
	 * for now (a route might have failed after we sent a segment,
	 * and the return path might not be symmetrical).
	 */
	tp->t_softerror = 0;
}

/*
 * Determine a reasonable value for maxseg size.
 * If the route is known, check route for mtu.
 * If none, use an mss that can be handled on the outgoing
 * interface without forcing IP to fragment; if bigger than
 * an mbuf cluster (MCLBYTES), round down to nearest multiple of MCLBYTES
 * to utilize large mbufs.  If no route is found, route has no mtu,
 * or the destination isn't local, use a default, hopefully conservative
 * size (usually 512 or the default IP max size, but no more than the mtu
 * of the interface), as we can't discover anything about intervening
 * gateways or networks.  We also initialize the congestion/slow start
 * window to be a single segment if the destination isn't local.
 * While looking at the routing entry, we also initialize other path-dependent
 * parameters from pre-set or cached values in the routing entry.
 */
int
tcp_mss(tp, offer)
	register struct tcpcb *tp;
	u_int offer;
{
	struct route *ro;
	register struct rtentry *rt;
	struct ifnet *ifp;
	register int rtt, mss;
	u_long bufsize;
	struct inpcb *inp;
	struct socket *so;
	extern int tcp_mssdflt;

	inp = tp->t_inpcb;
	ro = &inp->inp_route;

	if ((rt = ro->ro_rt) == (struct rtentry *)0) {
		/* No route yet, so try to acquire one */
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			ro->ro_dst.sa_family = AF_INET;
			ro->ro_dst.sa_len = sizeof(ro->ro_dst);
			satosin(&ro->ro_dst)->sin_addr = inp->inp_faddr;
			rtalloc(ro);
		}
		if ((rt = ro->ro_rt) == (struct rtentry *)0)
			return (tcp_mssdflt);
	}
	ifp = rt->rt_ifp;
	so = inp->inp_socket;

#ifdef RTV_MTU	/* if route characteristics exist ... */
	/*
	 * While we're here, check if there's an initial rtt
	 * or rttvar.  Convert from the route-table units
	 * to scaled multiples of the slow timeout timer.
	 */
	if (tp->t_srtt == 0 && (rtt = rt->rt_rmx.rmx_rtt)) {
		/*
		 * XXX the lock bit for MTU indicates that the value
		 * is also a minimum value; this is subject to time.
		 */
		if (rt->rt_rmx.rmx_locks & RTV_RTT)
			tp->t_rttmin = rtt / (RTM_RTTUNIT / PR_SLOWHZ);
		tp->t_srtt = rtt /
		    ((RTM_RTTUNIT / PR_SLOWHZ) >> (TCP_RTT_SHIFT + 2));
		if (rt->rt_rmx.rmx_rttvar)
			tp->t_rttvar = rt->rt_rmx.rmx_rttvar /
			    ((RTM_RTTUNIT / PR_SLOWHZ) >> (TCP_RTTVAR_SHIFT + 2));
		else
			/* default variation is +- 1 rtt */
			tp->t_rttvar =
			    tp->t_srtt >> (TCP_RTT_SHIFT - TCP_RTTVAR_SHIFT);
		TCPT_RANGESET(tp->t_rxtcur,
		    ((tp->t_srtt >> 2) + tp->t_rttvar) >> (1 + 2),
		    tp->t_rttmin, TCPTV_REXMTMAX);
	}
	/*
	 * if there's an mtu associated with the route, use it
	 */
	if (rt->rt_rmx.rmx_mtu)
		mss = rt->rt_rmx.rmx_mtu - sizeof(struct tcpiphdr);
	else
#endif /* RTV_MTU */
	{
		mss = ifp->if_mtu - sizeof(struct tcpiphdr);
#if	(MCLBYTES & (MCLBYTES - 1)) == 0
		if (mss > MCLBYTES)
			mss &= ~(MCLBYTES-1);
#else
		if (mss > MCLBYTES)
			mss = mss / MCLBYTES * MCLBYTES;
#endif
		if (!in_localaddr(inp->inp_faddr))
			mss = min(mss, tcp_mssdflt);
	}
	/*
	 * The current mss, t_maxseg, is initialized to the default value.
	 * If we compute a smaller value, reduce the current mss.
	 * If we compute a larger value, return it for use in sending
	 * a max seg size option, but don't store it for use
	 * unless we received an offer at least that large from peer.
	 * However, do not accept offers under 32 bytes.
	 */
	if (offer)
		mss = min(mss, offer);
	mss = max(mss, 32);		/* sanity */
	if (mss < tp->t_maxseg || offer != 0) {
		/*
		 * If there's a pipesize, change the socket buffer
		 * to that size.  Make the socket buffers an integral
		 * number of mss units; if the mss is larger than
		 * the socket buffer, decrease the mss.
		 */
#ifdef RTV_SPIPE
		if ((bufsize = rt->rt_rmx.rmx_sendpipe) == 0)
#endif
			bufsize = so->so_snd.sb_hiwat;
		if (bufsize < mss)
			mss = bufsize;
		else {
			bufsize = roundup(bufsize, mss);
			if (bufsize > sb_max)
				bufsize = sb_max;
			(void)sbreserve(&so->so_snd, bufsize);
		}
		tp->t_maxseg = mss;

#ifdef RTV_RPIPE
		if ((bufsize = rt->rt_rmx.rmx_recvpipe) == 0)
#endif
			bufsize = so->so_rcv.sb_hiwat;
		if (bufsize > mss) {
			bufsize = roundup(bufsize, mss);
			if (bufsize > sb_max)
				bufsize = sb_max;
			(void)sbreserve(&so->so_rcv, bufsize);
		}
	}
	tp->snd_cwnd = mss;

#ifdef RTV_SSTHRESH
	if (rt->rt_rmx.rmx_ssthresh) {
		/*
		 * There's some sort of gateway or interface
		 * buffer limit on the path.  Use this to set
		 * the slow start threshhold, but set the
		 * threshold to no less than 2*mss.
		 */
		tp->snd_ssthresh = max(2 * mss, rt->rt_rmx.rmx_ssthresh);
	}
#endif /* RTV_MTU */
	return (mss);
}
#endif /* TUBA_INCLUDE */
