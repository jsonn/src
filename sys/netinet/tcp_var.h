/*	$NetBSD: tcp_var.h,v 1.25.2.1 1997/11/08 06:31:36 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993, 1994
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
 *	@(#)tcp_var.h	8.3 (Berkeley) 4/10/94
 */

/*
 * Kernel variables for tcp.
 */

/*
 * Tcp control block, one per tcp; fields:
 */
struct tcpcb {
	struct ipqehead segq;		/* sequencing queue */
	short	t_state;		/* state of this connection */
	short	t_timer[TCPT_NTIMERS];	/* tcp timers */
	short	t_rxtshift;		/* log(2) of rexmt exp. backoff */
	short	t_rxtcur;		/* current retransmit value */
	short	t_dupacks;		/* consecutive dup acks recd */
	u_short	t_peermss;		/* peer's maximum segment size */
	u_short	t_ourmss;		/* our's maximum segment size */
	u_short t_segsz;		/* current segment size in use */
	char	t_force;		/* 1 if forcing out a byte */
	u_short	t_flags;
#define	TF_ACKNOW	0x0001		/* ack peer immediately */
#define	TF_DELACK	0x0002		/* ack, but try to delay it */
#define	TF_NODELAY	0x0004		/* don't delay packets to coalesce */
#define	TF_NOOPT	0x0008		/* don't use tcp options */
#define	TF_SENTFIN	0x0010		/* have sent FIN */
#define	TF_REQ_SCALE	0x0020		/* have/will request window scaling */
#define	TF_RCVD_SCALE	0x0040		/* other side has requested scaling */
#define	TF_REQ_TSTMP	0x0080		/* have/will request timestamps */
#define	TF_RCVD_TSTMP	0x0100		/* a timestamp was received in SYN */
#define	TF_SACK_PERMIT	0x0200		/* other side said I could SACK */

	struct	tcpiphdr *t_template;	/* skeletal packet for transmit */
	struct	inpcb *t_inpcb;		/* back pointer to internet pcb */
/*
 * The following fields are used as in the protocol specification.
 * See RFC783, Dec. 1981, page 21.
 */
/* send sequence variables */
	tcp_seq	snd_una;		/* send unacknowledged */
	tcp_seq	snd_nxt;		/* send next */
	tcp_seq	snd_up;			/* send urgent pointer */
	tcp_seq	snd_wl1;		/* window update seg seq number */
	tcp_seq	snd_wl2;		/* window update seg ack number */
	tcp_seq	iss;			/* initial send sequence number */
	u_long	snd_wnd;		/* send window */
/* receive sequence variables */
	u_long	rcv_wnd;		/* receive window */
	tcp_seq	rcv_nxt;		/* receive next */
	tcp_seq	rcv_up;			/* receive urgent pointer */
	tcp_seq	irs;			/* initial receive sequence number */
/*
 * Additional variables for this implementation.
 */
/* receive variables */
	tcp_seq	rcv_adv;		/* advertised window */
/* retransmit variables */
	tcp_seq	snd_max;		/* highest sequence number sent;
					 * used to recognize retransmits
					 */
/* congestion control (for slow start, source quench, retransmit after loss) */
	u_long	snd_cwnd;		/* congestion-controlled window */
	u_long	snd_ssthresh;		/* snd_cwnd size threshhold for
					 * for slow start exponential to
					 * linear switch
					 */
/*
 * transmit timing stuff.  See below for scale of srtt and rttvar.
 * "Variance" is actually smoothed difference.
 */
	short	t_idle;			/* inactivity time */
	short	t_rtt;			/* round trip time */
	tcp_seq	t_rtseq;		/* sequence number being timed */
	short	t_srtt;			/* smoothed round-trip time */
	short	t_rttvar;		/* variance in round-trip time */
	short	t_rttmin;		/* minimum rtt allowed */
	u_long	max_sndwnd;		/* largest window peer has offered */

/* out-of-band data */
	char	t_oobflags;		/* have some */
	char	t_iobc;			/* input character */
#define	TCPOOB_HAVEDATA	0x01
#define	TCPOOB_HADDATA	0x02
	short	t_softerror;		/* possible error not yet reported */

/* RFC 1323 variables */
	u_char	snd_scale;		/* window scaling for send window */
	u_char	rcv_scale;		/* window scaling for recv window */
	u_char	request_r_scale;	/* pending window scaling */
	u_char	requested_s_scale;
	u_int32_t ts_recent;		/* timestamp echo data */
	u_int32_t ts_recent_age;		/* when last updated */
	tcp_seq	last_ack_sent;

/* TUBA stuff */
	caddr_t	t_tuba_pcb;		/* next level down pcb for TCP over z */
};

/*
 * Handy way of passing around TCP option info.
 */
struct tcp_opt_info {
	int		ts_present;
	u_int32_t	ts_val;
	u_int32_t	ts_ecr;
	u_int16_t	maxseg;
};

/*
 * This structure should not exceed 32 bytes.
 * XXX On the Alpha, it's already 36-bytes, which rounds to 40.
 * XXX Need to eliminate the pointer.
 *
 * XXX We've blown 32 bytes on non-Alpha systems, too, since we're
 * XXX storing the maxseg we advertised to the peer.  Should we
 * XXX create another malloc bucket?  Should we care?
 */
struct syn_cache {
	struct syn_cache *sc_next;
	u_int32_t sc_tstmp		: 1,
		  sc_hash		: 31;
	struct in_addr sc_src;
	struct in_addr sc_dst;
	tcp_seq sc_irs;
	tcp_seq sc_iss;
	u_int16_t sc_sport;
	u_int16_t sc_dport;
	u_int16_t sc_peermaxseg;
	u_int16_t sc_ourmaxseg;
	u_int8_t sc_timer;
	u_int8_t sc_request_r_scale	: 4,
		 sc_requested_s_scale	: 4;
};

struct syn_cache_head {
	struct syn_cache *sch_first;		/* First entry in the bucket */
	struct syn_cache *sch_last;		/* Last entry in the bucket */
	struct syn_cache_head *sch_headq;	/* The next non-empty bucket */
	int16_t sch_timer_sum;			/* Total time in this bucket */
	u_int16_t sch_length;			/* @ # elements in bucket */
};

#define	intotcpcb(ip)	((struct tcpcb *)(ip)->inp_ppcb)
#define	sototcpcb(so)	(intotcpcb(sotoinpcb(so)))

/*
 * The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 3 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 2 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define	TCP_RTT_SHIFT		3	/* shift for srtt; 3 bits frac. */
#define	TCP_RTTVAR_SHIFT	2	/* multiplier for rttvar; 2 bits */

/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This macro assumes that the value of 1<<TCP_RTTVAR_SHIFT
 * is the same as the multiplier for rttvar.
 */
#define	TCP_REXMTVAL(tp) \
	((((tp)->t_srtt >> TCP_RTT_SHIFT) + (tp)->t_rttvar) >> 2)

/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct	tcpstat {
	u_long	tcps_connattempt;	/* connections initiated */
	u_long	tcps_accepts;		/* connections accepted */
	u_long	tcps_connects;		/* connections established */
	u_long	tcps_drops;		/* connections dropped */
	u_long	tcps_conndrops;		/* embryonic connections dropped */
	u_long	tcps_closed;		/* conn. closed (includes drops) */
	u_long	tcps_segstimed;		/* segs where we tried to get rtt */
	u_long	tcps_rttupdated;	/* times we succeeded */
	u_long	tcps_delack;		/* delayed acks sent */
	u_long	tcps_timeoutdrop;	/* conn. dropped in rxmt timeout */
	u_long	tcps_rexmttimeo;	/* retransmit timeouts */
	u_long	tcps_persisttimeo;	/* persist timeouts */
	u_long	tcps_keeptimeo;		/* keepalive timeouts */
	u_long	tcps_keepprobe;		/* keepalive probes sent */
	u_long	tcps_keepdrops;		/* connections dropped in keepalive */

	u_long	tcps_sndtotal;		/* total packets sent */
	u_long	tcps_sndpack;		/* data packets sent */
	u_long	tcps_sndbyte;		/* data bytes sent */
	u_long	tcps_sndrexmitpack;	/* data packets retransmitted */
	u_long	tcps_sndrexmitbyte;	/* data bytes retransmitted */
	u_long	tcps_sndacks;		/* ack-only packets sent */
	u_long	tcps_sndprobe;		/* window probes sent */
	u_long	tcps_sndurg;		/* packets sent with URG only */
	u_long	tcps_sndwinup;		/* window update-only packets sent */
	u_long	tcps_sndctrl;		/* control (SYN|FIN|RST) packets sent */

	u_long	tcps_rcvtotal;		/* total packets received */
	u_long	tcps_rcvpack;		/* packets received in sequence */
	u_long	tcps_rcvbyte;		/* bytes received in sequence */
	u_long	tcps_rcvbadsum;		/* packets received with ccksum errs */
	u_long	tcps_rcvbadoff;		/* packets received with bad offset */
	u_long	tcps_rcvmemdrop;	/* packets dropped for lack of memory */
	u_long	tcps_rcvshort;		/* packets received too short */
	u_long	tcps_rcvduppack;	/* duplicate-only packets received */
	u_long	tcps_rcvdupbyte;	/* duplicate-only bytes received */
	u_long	tcps_rcvpartduppack;	/* packets with some duplicate data */
	u_long	tcps_rcvpartdupbyte;	/* dup. bytes in part-dup. packets */
	u_long	tcps_rcvoopack;		/* out-of-order packets received */
	u_long	tcps_rcvoobyte;		/* out-of-order bytes received */
	u_long	tcps_rcvpackafterwin;	/* packets with data after window */
	u_long	tcps_rcvbyteafterwin;	/* bytes rcvd after window */
	u_long	tcps_rcvafterclose;	/* packets rcvd after "close" */
	u_long	tcps_rcvwinprobe;	/* rcvd window probe packets */
	u_long	tcps_rcvdupack;		/* rcvd duplicate acks */
	u_long	tcps_rcvacktoomuch;	/* rcvd acks for unsent data */
	u_long	tcps_rcvackpack;	/* rcvd ack packets */
	u_long	tcps_rcvackbyte;	/* bytes acked by rcvd acks */
	u_long	tcps_rcvwinupd;		/* rcvd window update packets */
	u_long	tcps_pawsdrop;		/* segments dropped due to PAWS */
	u_long	tcps_predack;		/* times hdr predict ok for acks */
	u_long	tcps_preddat;		/* times hdr predict ok for data pkts */

	u_long	tcps_pcbhashmiss;	/* input packets missing pcb hash */
	u_long	tcps_noport;		/* no socket on port */
	u_long	tcps_badsyn;		/* received ack for which we have
					   no SYN in compressed state */

	/* These statistics deal with the SYN cache. */
	u_long	tcps_sc_added;		/* # of entries added */
	u_long	tcps_sc_completed;	/* # of connections completed */
	u_long	tcps_sc_timed_out;	/* # of entries timed out */
	u_long	tcps_sc_overflowed;	/* # dropped due to overflow */
	u_long	tcps_sc_reset;		/* # dropped due to RST */
	u_long	tcps_sc_unreach;	/* # dropped due to ICMP unreach */
	u_long	tcps_sc_bucketoverflow;	/* # dropped due to bucket overflow */
	u_long	tcps_sc_aborted;	/* # of entries aborted (no mem) */
	u_long	tcps_sc_dupesyn;	/* # of duplicate SYNs received */
	u_long	tcps_sc_dropped;	/* # of SYNs dropped (no route/mem) */
	u_long	tcps_sc_collisions;	/* # of hash collisions */
};

/*
 * Names for TCP sysctl objects.
 */
#define	TCPCTL_RFC1323		1	/* RFC1323 timestamps/scaling */
#define	TCPCTL_SENDSPACE	2	/* default send buffer */
#define	TCPCTL_RECVSPACE	3	/* default recv buffer */
#define	TCPCTL_MSSDFLT		4	/* default seg size */
#define	TCPCTL_SYN_CACHE_LIMIT	5	/* max size of comp. state engine */
#define	TCPCTL_SYN_BUCKET_LIMIT	6	/* max size of hash bucket */
#define	TCPCTL_SYN_CACHE_INTER	7	/* interval of comp. state timer */
#define	TCPCTL_MAXID		8

#define	TCPCTL_NAMES { \
	{ 0, 0 }, \
	{ "rfc1323",	CTLTYPE_INT }, \
	{ "sendspace",	CTLTYPE_INT }, \
	{ "recvspace",	CTLTYPE_INT }, \
	{ "mssdflt",	CTLTYPE_INT }, \
	{ "syn_cache_limit", CTLTYPE_INT }, \
	{ "syn_bucket_limit", CTLTYPE_INT }, \
	{ "syn_cache_interval", CTLTYPE_INT },\
}

#ifdef _KERNEL
struct	inpcbtable tcbtable;	/* head of queue of active tcpcb's */
struct	tcpstat tcpstat;	/* tcp statistics */
u_int32_t tcp_now;		/* for RFC 1323 timestamps */
extern	int tcp_do_rfc1323;	/* enabled/disabled? */
extern	int tcp_mssdflt;	/* default seg size */
extern	int tcp_syn_cache_limit; /* max entries for compressed state engine */
extern	int tcp_syn_bucket_limit;/* max entries per hash bucket */
extern	int tcp_syn_cache_interval; /* compressed state timer */

extern	int tcp_syn_cache_size;
extern	int tcp_syn_cache_timeo;
extern	struct syn_cache_head tcp_syn_cache[], *tcp_syn_cache_first;
extern	u_long syn_cache_count;

int	 tcp_attach __P((struct socket *));
void	 tcp_canceltimers __P((struct tcpcb *));
struct tcpcb *
	 tcp_close __P((struct tcpcb *));
void	 *tcp_ctlinput __P((int, struct sockaddr *, void *));
int	 tcp_ctloutput __P((int, struct socket *, int, int, struct mbuf **));
struct tcpcb *
	 tcp_disconnect __P((struct tcpcb *));
struct tcpcb *
	 tcp_drop __P((struct tcpcb *, int));
void	 tcp_dooptions __P((struct tcpcb *,
	    u_char *, int, struct tcpiphdr *, struct tcp_opt_info *));
void	 tcp_drain __P((void));
void	 tcp_established __P((struct tcpcb *));
void	 tcp_fasttimo __P((void));
void	 tcp_init __P((void));
void	 tcp_input __P((struct mbuf *, ...));
int	 tcp_mss_to_advertise __P((const struct tcpcb *));
void	 tcp_mss_from_peer __P((struct tcpcb *, int));
void	 tcp_mtudisc __P((struct inpcb *, int));
struct tcpcb *
	 tcp_newtcpcb __P((struct inpcb *));
void	 tcp_notify __P((struct inpcb *, int));
int	 tcp_output __P((struct tcpcb *));
void	 tcp_pulloutofband __P((struct socket *,
	    struct tcpiphdr *, struct mbuf *));
void	 tcp_quench __P((struct inpcb *, int));
int	 tcp_reass __P((struct tcpcb *, struct tcpiphdr *, struct mbuf *));
int	 tcp_respond __P((struct tcpcb *,
	    struct tcpiphdr *, struct mbuf *, tcp_seq, tcp_seq, int));
void	 tcp_rmx_rtt __P((struct tcpcb *));
void	 tcp_setpersist __P((struct tcpcb *));
void	 tcp_slowtimo __P((void));
struct tcpiphdr *
	 tcp_template __P((struct tcpcb *));
struct tcpcb *
	 tcp_timers __P((struct tcpcb *, int));
void	 tcp_trace __P((int, int, struct tcpcb *, struct tcpiphdr *, int));
struct tcpcb *
	 tcp_usrclosed __P((struct tcpcb *));
int	 tcp_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));
int	 tcp_usrreq __P((struct socket *,
	    int, struct mbuf *, struct mbuf *, struct mbuf *, struct proc *));
void	 tcp_xmit_timer __P((struct tcpcb *, int));
tcp_seq  tcp_new_iss __P((void *, u_long, tcp_seq));

int	 syn_cache_add __P((struct socket *, struct mbuf *, u_char *,
	    int, struct tcp_opt_info *));
void	 syn_cache_unreach __P((struct ip *, struct tcphdr *));
struct socket *
	 syn_cache_get __P((struct socket *so, struct mbuf *));
void	 syn_cache_insert __P((struct syn_cache *, struct syn_cache ***,
	    struct syn_cache_head **));
struct syn_cache *
	 syn_cache_lookup __P((struct tcpiphdr *, struct syn_cache ***,
	    struct syn_cache_head **));
void	 syn_cache_reset __P((struct tcpiphdr *));
int	 syn_cache_respond __P((struct syn_cache *, struct mbuf *,
	    register struct tcpiphdr *, long, u_long));
void	 syn_cache_timer __P((int));

#endif
