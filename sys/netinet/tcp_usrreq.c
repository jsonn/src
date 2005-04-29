/*	$NetBSD: tcp_usrreq.c,v 1.93.2.1 2005/04/29 11:29:34 kent Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1997, 1998, 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe and Kevin M. Lahey of the Numerical Aerospace Simulation
 * Facility, NASA Ames Research Center.
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1993, 1995
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
 *	@(#)tcp_usrreq.c	8.5 (Berkeley) 6/21/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tcp_usrreq.c,v 1.93.2.1 2005/04/29 11:29:34 kent Exp $");

#include "opt_inet.h"
#include "opt_ipsec.h"
#include "opt_tcp_debug.h"
#include "opt_mbuftrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#endif

#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

#include "opt_tcp_space.h"

#ifdef IPSEC
#include <netinet6/ipsec.h>
#endif /*IPSEC*/

/*
 * TCP protocol interface to socket abstraction.
 */

/*
 * Process a TCP user request for TCP tb.  If this is a send request
 * then m is the mbuf chain of send data.  If this is a timer expiration
 * (called from the software clock routine), then timertype tells which timer.
 */
/*ARGSUSED*/
int
tcp_usrreq(struct socket *so, int req,
    struct mbuf *m, struct mbuf *nam, struct mbuf *control, struct proc *p)
{
	struct inpcb *inp;
#ifdef INET6
	struct in6pcb *in6p;
#endif
	struct tcpcb *tp = NULL;
	int s;
	int error = 0;
#ifdef TCP_DEBUG
	int ostate = 0;
#endif
	int family;	/* family of the socket */

	family = so->so_proto->pr_domain->dom_family;

	if (req == PRU_CONTROL) {
		switch (family) {
#ifdef INET
		case PF_INET:
			return (in_control(so, (long)m, (caddr_t)nam,
			    (struct ifnet *)control, p));
#endif
#ifdef INET6
		case PF_INET6:
			return (in6_control(so, (long)m, (caddr_t)nam,
			    (struct ifnet *)control, p));
#endif
		default:
			return EAFNOSUPPORT;
		}
	}

	if (req == PRU_PURGEIF) {
		switch (family) {
#ifdef INET
		case PF_INET:
			in_pcbpurgeif0(&tcbtable, (struct ifnet *)control);
			in_purgeif((struct ifnet *)control);
			in_pcbpurgeif(&tcbtable, (struct ifnet *)control);
			break;
#endif
#ifdef INET6
		case PF_INET6:
			in6_pcbpurgeif0(&tcbtable, (struct ifnet *)control);
			in6_purgeif((struct ifnet *)control);
			in6_pcbpurgeif(&tcbtable, (struct ifnet *)control);
			break;
#endif
		default:
			return (EAFNOSUPPORT);
		}
		return (0);
	}

	s = splsoftnet();
	switch (family) {
#ifdef INET
	case PF_INET:
		inp = sotoinpcb(so);
#ifdef INET6
		in6p = NULL;
#endif
		break;
#endif
#ifdef INET6
	case PF_INET6:
		inp = NULL;
		in6p = sotoin6pcb(so);
		break;
#endif
	default:
		splx(s);
		return EAFNOSUPPORT;
	}

#ifdef DIAGNOSTIC
#ifdef INET6
	if (inp && in6p)
		panic("tcp_usrreq: both inp and in6p set to non-NULL");
#endif
	if (req != PRU_SEND && req != PRU_SENDOOB && control)
		panic("tcp_usrreq: unexpected control mbuf");
#endif
	/*
	 * When a TCP is attached to a socket, then there will be
	 * a (struct inpcb) pointed at by the socket, and this
	 * structure will point at a subsidary (struct tcpcb).
	 */
#ifndef INET6
	if (inp == 0 && req != PRU_ATTACH)
#else
	if ((inp == 0 && in6p == 0) && req != PRU_ATTACH)
#endif
	{
		error = EINVAL;
		goto release;
	}
#ifdef INET
	if (inp) {
		tp = intotcpcb(inp);
		/* WHAT IF TP IS 0? */
#ifdef KPROF
		tcp_acounts[tp->t_state][req]++;
#endif
#ifdef TCP_DEBUG
		ostate = tp->t_state;
#endif
	}
#endif
#ifdef INET6
	if (in6p) {
		tp = in6totcpcb(in6p);
		/* WHAT IF TP IS 0? */
#ifdef KPROF
		tcp_acounts[tp->t_state][req]++;
#endif
#ifdef TCP_DEBUG
		ostate = tp->t_state;
#endif
	}
#endif

	switch (req) {

	/*
	 * TCP attaches to socket via PRU_ATTACH, reserving space,
	 * and an internet control block.
	 */
	case PRU_ATTACH:
#ifndef INET6
		if (inp != 0)
#else
		if (inp != 0 || in6p != 0)
#endif
		{
			error = EISCONN;
			break;
		}
		error = tcp_attach(so);
		if (error)
			break;
		if ((so->so_options & SO_LINGER) && so->so_linger == 0)
			so->so_linger = TCP_LINGERTIME;
		tp = sototcpcb(so);
		break;

	/*
	 * PRU_DETACH detaches the TCP protocol from the socket.
	 */
	case PRU_DETACH:
		tp = tcp_disconnect(tp);
		break;

	/*
	 * Give the socket an address.
	 */
	case PRU_BIND:
		switch (family) {
#ifdef INET
		case PF_INET:
			error = in_pcbbind(inp, nam, p);
			break;
#endif
#ifdef INET6
		case PF_INET6:
			error = in6_pcbbind(in6p, nam, p);
			if (!error) {
				/* mapped addr case */
				if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr))
					tp->t_family = AF_INET;
				else
					tp->t_family = AF_INET6;
			}
			break;
#endif
		}
		break;

	/*
	 * Prepare to accept connections.
	 */
	case PRU_LISTEN:
#ifdef INET
		if (inp && inp->inp_lport == 0) {
			error = in_pcbbind(inp, (struct mbuf *)0,
			    (struct proc *)0);
			if (error)
				break;
		}
#endif
#ifdef INET6
		if (in6p && in6p->in6p_lport == 0) {
			error = in6_pcbbind(in6p, (struct mbuf *)0,
			    (struct proc *)0);
			if (error)
				break;
		}
#endif
		tp->t_state = TCPS_LISTEN;
		break;

	/*
	 * Initiate connection to peer.
	 * Create a template for use in transmissions on this connection.
	 * Enter SYN_SENT state, and mark socket as connecting.
	 * Start keep-alive timer, and seed output sequence space.
	 * Send initial segment on connection.
	 */
	case PRU_CONNECT:
#ifdef INET
		if (inp) {
			if (inp->inp_lport == 0) {
				error = in_pcbbind(inp, (struct mbuf *)0,
				    (struct proc *)0);
				if (error)
					break;
			}
			error = in_pcbconnect(inp, nam);
		}
#endif
#ifdef INET6
		if (in6p) {
			if (in6p->in6p_lport == 0) {
				error = in6_pcbbind(in6p, (struct mbuf *)0,
				    (struct proc *)0);
				if (error)
					break;
			}
			error = in6_pcbconnect(in6p, nam);
			if (!error) {
				/* mapped addr case */
				if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_faddr))
					tp->t_family = AF_INET;
				else
					tp->t_family = AF_INET6;
			}
		}
#endif
		if (error)
			break;
		tp->t_template = tcp_template(tp);
		if (tp->t_template == 0) {
#ifdef INET
			if (inp)
				in_pcbdisconnect(inp);
#endif
#ifdef INET6
			if (in6p)
				in6_pcbdisconnect(in6p);
#endif
			error = ENOBUFS;
			break;
		}
		/* Compute window scaling to request.  */
		while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
		    (TCP_MAXWIN << tp->request_r_scale) < so->so_rcv.sb_hiwat)
			tp->request_r_scale++;
		soisconnecting(so);
		tcpstat.tcps_connattempt++;
		tp->t_state = TCPS_SYN_SENT;
		TCP_TIMER_ARM(tp, TCPT_KEEP, TCPTV_KEEP_INIT);
		tp->iss = tcp_new_iss(tp, 0);
		tcp_sendseqinit(tp);
		error = tcp_output(tp);
		break;

	/*
	 * Create a TCP connection between two sockets.
	 */
	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	/*
	 * Initiate disconnect from peer.
	 * If connection never passed embryonic stage, just drop;
	 * else if don't need to let data drain, then can just drop anyways,
	 * else have to begin TCP shutdown process: mark socket disconnecting,
	 * drain unread data, state switch to reflect user close, and
	 * send segment (e.g. FIN) to peer.  Socket will be really disconnected
	 * when peer sends FIN and acks ours.
	 *
	 * SHOULD IMPLEMENT LATER PRU_CONNECT VIA REALLOC TCPCB.
	 */
	case PRU_DISCONNECT:
		tp = tcp_disconnect(tp);
		break;

	/*
	 * Accept a connection.  Essentially all the work is
	 * done at higher levels; just return the address
	 * of the peer, storing through addr.
	 */
	case PRU_ACCEPT:
#ifdef INET
		if (inp)
			in_setpeeraddr(inp, nam);
#endif
#ifdef INET6
		if (in6p)
			in6_setpeeraddr(in6p, nam);
#endif
		break;

	/*
	 * Mark the connection as being incapable of further output.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		tp = tcp_usrclosed(tp);
		if (tp)
			error = tcp_output(tp);
		break;

	/*
	 * After a receive, possibly send window update to peer.
	 */
	case PRU_RCVD:
		/*
		 * soreceive() calls this function when a user receives
		 * ancillary data on a listening socket. We don't call
		 * tcp_output in such a case, since there is no header
		 * template for a listening socket and hence the kernel
		 * will panic.
		 */
		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) != 0)
			(void) tcp_output(tp);
		break;

	/*
	 * Do a send by putting data in output queue and updating urgent
	 * marker if URG set.  Possibly send more data.
	 */
	case PRU_SEND:
		if (control && control->m_len) {
			m_freem(control);
			m_freem(m);
			error = EINVAL;
			break;
		}
		sbappendstream(&so->so_snd, m);
		error = tcp_output(tp);
		break;

	/*
	 * Abort the TCP.
	 */
	case PRU_ABORT:
		tp = tcp_drop(tp, ECONNABORTED);
		break;

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		splx(s);
		return (0);

	case PRU_RCVOOB:
		if (control && control->m_len) {
			m_freem(control);
			m_freem(m);
			error = EINVAL;
			break;
		}
		if ((so->so_oobmark == 0 &&
		    (so->so_state & SS_RCVATMARK) == 0) ||
		    so->so_options & SO_OOBINLINE ||
		    tp->t_oobflags & TCPOOB_HADDATA) {
			error = EINVAL;
			break;
		}
		if ((tp->t_oobflags & TCPOOB_HAVEDATA) == 0) {
			error = EWOULDBLOCK;
			break;
		}
		m->m_len = 1;
		*mtod(m, caddr_t) = tp->t_iobc;
		if (((long)nam & MSG_PEEK) == 0)
			tp->t_oobflags ^= (TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		break;

	case PRU_SENDOOB:
		if (sbspace(&so->so_snd) < -512) {
			m_freem(m);
			error = ENOBUFS;
			break;
		}
		/*
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section.
		 * Otherwise, snd_up should be one lower.
		 */
		sbappendstream(&so->so_snd, m);
		tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
		tp->t_force = 1;
		error = tcp_output(tp);
		tp->t_force = 0;
		break;

	case PRU_SOCKADDR:
#ifdef INET
		if (inp)
			in_setsockaddr(inp, nam);
#endif
#ifdef INET6
		if (in6p)
			in6_setsockaddr(in6p, nam);
#endif
		break;

	case PRU_PEERADDR:
#ifdef INET
		if (inp)
			in_setpeeraddr(inp, nam);
#endif
#ifdef INET6
		if (in6p)
			in6_setpeeraddr(in6p, nam);
#endif
		break;

	default:
		panic("tcp_usrreq");
	}
#ifdef TCP_DEBUG
	if (tp && (so->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, NULL, req);
#endif

release:
	splx(s);
	return (error);
}

int
tcp_ctloutput(int op, struct socket *so, int level, int optname,
    struct mbuf **mp)
{
	int error = 0, s;
	struct inpcb *inp;
#ifdef INET6
	struct in6pcb *in6p;
#endif
	struct tcpcb *tp;
	struct mbuf *m;
	int i;
	int family;	/* family of the socket */

	family = so->so_proto->pr_domain->dom_family;

	s = splsoftnet();
	switch (family) {
#ifdef INET
	case PF_INET:
		inp = sotoinpcb(so);
#ifdef INET6
		in6p = NULL;
#endif
		break;
#endif
#ifdef INET6
	case PF_INET6:
		inp = NULL;
		in6p = sotoin6pcb(so);
		break;
#endif
	default:
		splx(s);
		return EAFNOSUPPORT;
	}
#ifndef INET6
	if (inp == NULL)
#else
	if (inp == NULL && in6p == NULL)
#endif
	{
		splx(s);
		if (op == PRCO_SETOPT && *mp)
			(void) m_free(*mp);
		return (ECONNRESET);
	}
	if (level != IPPROTO_TCP) {
		switch (family) {
#ifdef INET
		case PF_INET:
			error = ip_ctloutput(op, so, level, optname, mp);
			break;
#endif
#ifdef INET6
		case PF_INET6:
			error = ip6_ctloutput(op, so, level, optname, mp);
			break;
#endif
		}
		splx(s);
		return (error);
	}
	if (inp)
		tp = intotcpcb(inp);
#ifdef INET6
	else if (in6p)
		tp = in6totcpcb(in6p);
#endif
	else
		tp = NULL;

	switch (op) {

	case PRCO_SETOPT:
		m = *mp;
		switch (optname) {

#ifdef TCP_SIGNATURE
		case TCP_MD5SIG:
			if (m == NULL || m->m_len < sizeof (int))
				error = EINVAL;
			if (error)
				break;
			if (*mtod(m, int *) > 0)
				tp->t_flags |= TF_SIGNATURE;
			else
				tp->t_flags &= ~TF_SIGNATURE;
			break;
#endif /* TCP_SIGNATURE */

		case TCP_NODELAY:
			if (m == NULL || m->m_len < sizeof (int))
				error = EINVAL;
			else if (*mtod(m, int *))
				tp->t_flags |= TF_NODELAY;
			else
				tp->t_flags &= ~TF_NODELAY;
			break;

		case TCP_MAXSEG:
			if (m && (i = *mtod(m, int *)) > 0 &&
			    i <= tp->t_peermss)
				tp->t_peermss = i;  /* limit on send size */
			else
				error = EINVAL;
			break;

		default:
			error = ENOPROTOOPT;
			break;
		}
		if (m)
			(void) m_free(m);
		break;

	case PRCO_GETOPT:
		*mp = m = m_get(M_WAIT, MT_SOOPTS);
		m->m_len = sizeof(int);
		MCLAIM(m, so->so_mowner);

		switch (optname) {
#ifdef TCP_SIGNATURE
		case TCP_MD5SIG:
			*mtod(m, int *) = (tp->t_flags & TF_SIGNATURE) ? 1 : 0;
			break;
#endif
		case TCP_NODELAY:
			*mtod(m, int *) = tp->t_flags & TF_NODELAY;
			break;
		case TCP_MAXSEG:
			*mtod(m, int *) = tp->t_peermss;
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	splx(s);
	return (error);
}

#ifndef TCP_SENDSPACE
#define	TCP_SENDSPACE	1024*32
#endif
int	tcp_sendspace = TCP_SENDSPACE;
#ifndef TCP_RECVSPACE
#define	TCP_RECVSPACE	1024*32
#endif
int	tcp_recvspace = TCP_RECVSPACE;

/*
 * Attach TCP protocol to socket, allocating
 * internet protocol control block, tcp control block,
 * bufer space, and entering LISTEN state if to accept connections.
 */
int
tcp_attach(struct socket *so)
{
	struct tcpcb *tp;
	struct inpcb *inp;
#ifdef INET6
	struct in6pcb *in6p;
#endif
	int error;
	int family;	/* family of the socket */

	family = so->so_proto->pr_domain->dom_family;

#ifdef MBUFTRACE
	so->so_mowner = &tcp_mowner;
	so->so_rcv.sb_mowner = &tcp_rx_mowner;
	so->so_snd.sb_mowner = &tcp_tx_mowner;
#endif
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve(so, tcp_sendspace, tcp_recvspace);
		if (error)
			return (error);
	}
	switch (family) {
#ifdef INET
	case PF_INET:
		error = in_pcballoc(so, &tcbtable);
		if (error)
			return (error);
		inp = sotoinpcb(so);
#ifdef INET6
		in6p = NULL;
#endif
		break;
#endif
#ifdef INET6
	case PF_INET6:
		error = in6_pcballoc(so, &tcbtable);
		if (error)
			return (error);
		inp = NULL;
		in6p = sotoin6pcb(so);
		break;
#endif
	default:
		return EAFNOSUPPORT;
	}
	if (inp)
		tp = tcp_newtcpcb(family, (void *)inp);
#ifdef INET6
	else if (in6p)
		tp = tcp_newtcpcb(family, (void *)in6p);
#endif
	else
		tp = NULL;

	if (tp == 0) {
		int nofd = so->so_state & SS_NOFDREF;	/* XXX */

		so->so_state &= ~SS_NOFDREF;	/* don't free the socket yet */
#ifdef INET
		if (inp)
			in_pcbdetach(inp);
#endif
#ifdef INET6
		if (in6p)
			in6_pcbdetach(in6p);
#endif
		so->so_state |= nofd;
		return (ENOBUFS);
	}
	tp->t_state = TCPS_CLOSED;
	return (0);
}

/*
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 */
struct tcpcb *
tcp_disconnect(struct tcpcb *tp)
{
	struct socket *so;

	if (tp->t_inpcb)
		so = tp->t_inpcb->inp_socket;
#ifdef INET6
	else if (tp->t_in6pcb)
		so = tp->t_in6pcb->in6p_socket;
#endif
	else
		so = NULL;

	if (TCPS_HAVEESTABLISHED(tp->t_state) == 0)
		tp = tcp_close(tp);
	else if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		tp = tcp_drop(tp, 0);
	else {
		soisdisconnecting(so);
		sbflush(&so->so_rcv);
		tp = tcp_usrclosed(tp);
		if (tp)
			(void) tcp_output(tp);
	}
	return (tp);
}

/*
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 */
struct tcpcb *
tcp_usrclosed(struct tcpcb *tp)
{

	switch (tp->t_state) {

	case TCPS_CLOSED:
	case TCPS_LISTEN:
	case TCPS_SYN_SENT:
		tp->t_state = TCPS_CLOSED;
		tp = tcp_close(tp);
		break;

	case TCPS_SYN_RECEIVED:
	case TCPS_ESTABLISHED:
		tp->t_state = TCPS_FIN_WAIT_1;
		break;

	case TCPS_CLOSE_WAIT:
		tp->t_state = TCPS_LAST_ACK;
		break;
	}
	if (tp && tp->t_state >= TCPS_FIN_WAIT_2) {
		struct socket *so;
		if (tp->t_inpcb)
			so = tp->t_inpcb->inp_socket;
#ifdef INET6
		else if (tp->t_in6pcb)
			so = tp->t_in6pcb->in6p_socket;
#endif
		else
			so = NULL;
		soisdisconnected(so);
		/*
		 * If we are in FIN_WAIT_2, we arrived here because the
		 * application did a shutdown of the send side.  Like the
		 * case of a transition from FIN_WAIT_1 to FIN_WAIT_2 after
		 * a full close, we start a timer to make sure sockets are
		 * not left in FIN_WAIT_2 forever.
		 */
		if ((tp->t_state == TCPS_FIN_WAIT_2) && (tcp_maxidle > 0))
			TCP_TIMER_ARM(tp, TCPT_2MSL, tcp_maxidle);
	}
	return (tp);
}

/*
 * sysctl helper routine for net.inet.ip.mssdflt.  it can't be less
 * than 32.
 */
static int
sysctl_net_inet_tcp_mssdflt(SYSCTLFN_ARGS)
{
	int error, mssdflt;
	struct sysctlnode node;

	mssdflt = tcp_mssdflt;
	node = *rnode;
	node.sysctl_data = &mssdflt;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (mssdflt < 32)
		return (EINVAL);
	tcp_mssdflt = mssdflt;

	return (0);
}

/*
 * sysctl helper routine for setting port related values under
 * net.inet.ip and net.inet6.ip6.  does basic range checking and does
 * additional checks for each type.  this code has placed in
 * tcp_input.c since INET and INET6 both use the same tcp code.
 *
 * this helper is not static so that both inet and inet6 can use it.
 */
int
sysctl_net_inet_ip_ports(SYSCTLFN_ARGS)
{
	int error, tmp;
	int apmin, apmax;
#ifndef IPNOPRIVPORTS
	int lpmin, lpmax;
#endif /* IPNOPRIVPORTS */
	struct sysctlnode node;

	if (namelen != 0)
		return (EINVAL);

	switch (name[-3]) {
#ifdef INET
	    case PF_INET:
		apmin = anonportmin;
		apmax = anonportmax;
#ifndef IPNOPRIVPORTS
		lpmin = lowportmin;
		lpmax = lowportmax;
#endif /* IPNOPRIVPORTS */
		break;
#endif /* INET */
#ifdef INET6
	    case PF_INET6:
		apmin = ip6_anonportmin;
		apmax = ip6_anonportmax;
#ifndef IPNOPRIVPORTS
		lpmin = ip6_lowportmin;
		lpmax = ip6_lowportmax;
#endif /* IPNOPRIVPORTS */
		break;
#endif /* INET6 */
	    default:
		return (EINVAL);
	}

	/*
	 * insert temporary copy into node, perform lookup on
	 * temporary, then restore pointer
	 */
	node = *rnode;
	tmp = *(int*)rnode->sysctl_data;
	node.sysctl_data = &tmp;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	/*
	 * simple port range check
	 */
	if (tmp < 0 || tmp > 65535)
		return (EINVAL);

	/*
	 * per-node range checks
	 */
	switch (rnode->sysctl_num) {
	case IPCTL_ANONPORTMIN:
		if (tmp >= apmax)
			return (EINVAL);
#ifndef IPNOPRIVPORTS
		if (tmp < IPPORT_RESERVED)
                        return (EINVAL);
#endif /* IPNOPRIVPORTS */
		break;

	case IPCTL_ANONPORTMAX:
                if (apmin >= tmp)
			return (EINVAL);
#ifndef IPNOPRIVPORTS
		if (tmp < IPPORT_RESERVED)
                        return (EINVAL);
#endif /* IPNOPRIVPORTS */
		break;

#ifndef IPNOPRIVPORTS
	case IPCTL_LOWPORTMIN:
		if (tmp >= lpmax ||
		    tmp > IPPORT_RESERVEDMAX ||
		    tmp < IPPORT_RESERVEDMIN)
			return (EINVAL);
		break;

	case IPCTL_LOWPORTMAX:
		if (lpmin >= tmp ||
		    tmp > IPPORT_RESERVEDMAX ||
		    tmp < IPPORT_RESERVEDMIN)
			return (EINVAL);
		break;
#endif /* IPNOPRIVPORTS */

	default:
		return (EINVAL);
	}

	*(int*)rnode->sysctl_data = tmp;

	return (0);
}

/*
 * sysctl helper routine for the net.inet.tcp.ident and
 * net.inet6.tcp6.ident nodes.  contains backwards compat code for the
 * old way of looking up the ident information for ipv4 which involves
 * stuffing the port/addr pairs into the mib lookup.
 */
static int
sysctl_net_inet_tcp_ident(SYSCTLFN_ARGS)
{
#ifdef INET
	struct inpcb *inb;
	struct sockaddr_in *si4[2];
#endif /* INET */
#ifdef INET6
	struct in6pcb *in6b;
	struct sockaddr_in6 *si6[2];
#endif /* INET6 */
	struct sockaddr_storage sa[2];
	struct socket *sockp;
	size_t sz;
	uid_t uid;
	int error, pf;

	if (namelen != 4 && namelen != 0)
		return (EINVAL);
	if (name[-2] != IPPROTO_TCP)
		return (EINVAL);
	pf = name[-3];

	/* old style lookup, ipv4 only */
	if (namelen == 4) {
#ifdef INET
		struct in_addr laddr, raddr;
		u_int lport, rport;

		if (pf != PF_INET)
			return (EPROTONOSUPPORT);
		raddr.s_addr = (uint32_t)name[0];
		rport = (u_int)name[1];
		laddr.s_addr = (uint32_t)name[2];
		lport = (u_int)name[3];
		inb = in_pcblookup_connect(&tcbtable, raddr, rport,
					   laddr, lport);
		if (inb == NULL || (sockp = inb->inp_socket) == NULL)
			return (ESRCH);
		uid = sockp->so_uid;
		if (oldp) {
			sz = MIN(sizeof(uid), *oldlenp);
			error = copyout(&uid, oldp, sz);
			if (error)
				return (error);
		}
		*oldlenp = sizeof(uid);
		return (0);
#else /* INET */
		return (EINVAL);
#endif /* INET */
	}

	if (newp == NULL || newlen != sizeof(sa))
		return (EINVAL);
	error = copyin(newp, &sa, newlen);
	if (error)
		return (error);

	/*
	 * requested families must match
	 */
	if (pf != sa[0].ss_family || sa[0].ss_family != sa[1].ss_family)
		return (EINVAL);

	switch (pf) {
#ifdef INET
	    case PF_INET:
		si4[0] = (struct sockaddr_in*)&sa[0];
		si4[1] = (struct sockaddr_in*)&sa[1];
		if (si4[0]->sin_len != sizeof(*si4[0]) ||
		    si4[0]->sin_len != si4[1]->sin_len)
			return (EINVAL);
		inb = in_pcblookup_connect(&tcbtable,
		    si4[0]->sin_addr, si4[0]->sin_port,
		    si4[1]->sin_addr, si4[1]->sin_port);
		if (inb == NULL || (sockp = inb->inp_socket) == NULL)
			return (ESRCH);
		break;
#endif /* INET */
#ifdef INET6
	    case PF_INET6:
		si6[0] = (struct sockaddr_in6*)&sa[0];
		si6[1] = (struct sockaddr_in6*)&sa[1];
		if (si6[0]->sin6_len != sizeof(*si6[0]) ||
		    si6[0]->sin6_len != si6[1]->sin6_len)
			return (EINVAL);
		in6b = in6_pcblookup_connect(&tcbtable,
		    &si6[0]->sin6_addr, si6[0]->sin6_port,
		    &si6[1]->sin6_addr, si6[1]->sin6_port, 0);
		if (in6b == NULL || (sockp = in6b->in6p_socket) == NULL)
			return (ESRCH);
		break;
#endif /* INET6 */
	    default:
		return (EPROTONOSUPPORT);
	}

	uid = sockp->so_uid;
	if (oldp) {
		sz = MIN(sizeof(uid), *oldlenp);
		error = copyout(&uid, oldp, sz);
		if (error)
			return (error);
	}
	*oldlenp = sizeof(uid);

	return (0);
}

/*
 * sysctl helper for the inet and inet6 pcblists.  handles tcp/udp and
 * inet/inet6, as well as raw pcbs for each.  specifically not
 * declared static so that raw sockets and udp/udp6 can use it as
 * well.
 */
int
sysctl_inpcblist(SYSCTLFN_ARGS)
{
#ifdef INET
	struct sockaddr_in *in;
	struct inpcb *inp;
#endif
#ifdef INET6
	struct sockaddr_in6 *in6;
	struct in6pcb *in6p;
#endif
	const struct inpcbtable *pcbtbl = rnode->sysctl_data;
	struct inpcb_hdr *inph;
	struct tcpcb *tp;
	struct kinfo_pcb pcb;
	char *dp;
	u_int op, arg;
	size_t len, needed, elem_size, out_size;
	int error, elem_count, pf, proto, pf2;

	if (namelen != 4)
		return (EINVAL);

	error = 0;
	dp = oldp;
	len = (oldp != NULL) ? *oldlenp : 0;
	op = name[0];
	arg = name[1];
	elem_size = name[2];
	elem_count = name[3];
	out_size = MIN(sizeof(pcb), elem_size);
	needed = 0;

	elem_count = INT_MAX;
	elem_size = out_size = sizeof(pcb);

	if (namelen == 1 && name[0] == CTL_QUERY)
		return (sysctl_query(SYSCTLFN_CALL(rnode)));

	if (name - oname != 4)
		return (EINVAL);

	pf = oname[1];
	proto = oname[2];
	pf2 = (oldp == NULL) ? 0 : pf;

	CIRCLEQ_FOREACH(inph, &pcbtbl->inpt_queue, inph_queue) {
#ifdef INET
		inp = (struct inpcb *)inph;
#endif
#ifdef INET6
		in6p = (struct in6pcb *)inph;
#endif

		if (inph->inph_af != pf)
			continue;

		memset(&pcb, 0, sizeof(pcb));

		pcb.ki_family = pf;
		pcb.ki_type = proto;

		switch (pf2) {
		case 0:
			/* just probing for size */
			break;
#ifdef INET
		case PF_INET:
			pcb.ki_family = inp->inp_socket->so_proto->
			    pr_domain->dom_family;
			pcb.ki_type = inp->inp_socket->so_proto->
			    pr_type;
			pcb.ki_protocol = inp->inp_socket->so_proto->
			    pr_protocol;
			pcb.ki_pflags = inp->inp_flags;

			pcb.ki_sostate = inp->inp_socket->so_state;
			pcb.ki_prstate = inp->inp_state;
			if (proto == IPPROTO_TCP) {
				tp = intotcpcb(inp);
				pcb.ki_tstate = tp->t_state;
				pcb.ki_tflags = tp->t_flags;
			}

			pcb.ki_pcbaddr = PTRTOUINT64(inp);
			pcb.ki_ppcbaddr = PTRTOUINT64(inp->inp_ppcb);
			pcb.ki_sockaddr = PTRTOUINT64(inp->inp_socket);

			pcb.ki_rcvq = inp->inp_socket->so_rcv.sb_cc;
			pcb.ki_sndq = inp->inp_socket->so_snd.sb_cc;

			in = satosin(&pcb.ki_src);
			in->sin_len = sizeof(*in);
			in->sin_family = pf;
			in->sin_port = inp->inp_lport;
			in->sin_addr = inp->inp_laddr;
			if (pcb.ki_prstate >= INP_CONNECTED) {
				in = satosin(&pcb.ki_dst);
				in->sin_len = sizeof(*in);
				in->sin_family = pf;
				in->sin_port = inp->inp_fport;
				in->sin_addr = inp->inp_faddr;
			}
			break;
#endif
#ifdef INET6
		case PF_INET6:
			pcb.ki_family = in6p->in6p_socket->so_proto->
			    pr_domain->dom_family;
			pcb.ki_type = in6p->in6p_socket->so_proto->pr_type;
			pcb.ki_protocol = in6p->in6p_socket->so_proto->
			    pr_protocol;
			pcb.ki_pflags = in6p->in6p_flags;

			pcb.ki_sostate = in6p->in6p_socket->so_state;
			pcb.ki_prstate = in6p->in6p_state;
			if (proto == IPPROTO_TCP) {
				tp = in6totcpcb(in6p);
				pcb.ki_tstate = tp->t_state;
				pcb.ki_tflags = tp->t_flags;
			}

			pcb.ki_pcbaddr = PTRTOUINT64(in6p);
			pcb.ki_ppcbaddr = PTRTOUINT64(in6p->in6p_ppcb);
			pcb.ki_sockaddr = PTRTOUINT64(in6p->in6p_socket);

			pcb.ki_rcvq = in6p->in6p_socket->so_rcv.sb_cc;
			pcb.ki_sndq = in6p->in6p_socket->so_snd.sb_cc;

			in6 = satosin6(&pcb.ki_src);
			in6->sin6_len = sizeof(*in6);
			in6->sin6_family = pf;
			in6->sin6_port = in6p->in6p_lport;
			in6->sin6_flowinfo = in6p->in6p_flowinfo;
			in6->sin6_addr = in6p->in6p_laddr;
			in6->sin6_scope_id = 0; /* XXX? */

			if (pcb.ki_prstate >= IN6P_CONNECTED) {
				in6 = satosin6(&pcb.ki_dst);
				in6->sin6_len = sizeof(*in6);
				in6->sin6_family = pf;
				in6->sin6_port = in6p->in6p_fport;
				in6->sin6_flowinfo = in6p->in6p_flowinfo;
				in6->sin6_addr = in6p->in6p_faddr;
				in6->sin6_scope_id = 0; /* XXX? */
			}
			break;
#endif
		}

		if (len >= elem_size && elem_count > 0) {
			error = copyout(&pcb, dp, out_size);
			if (error)
				return (error);
			dp += elem_size;
			len -= elem_size;
		}
		if (elem_count > 0) {
			needed += elem_size;
			if (elem_count != INT_MAX)
				elem_count--;
		}
	}

	*oldlenp = needed;
	if (oldp == NULL)
		*oldlenp += PCB_SLOP * sizeof(struct kinfo_pcb);

	return (error);
}

/*
 * this (second stage) setup routine is a replacement for tcp_sysctl()
 * (which is currently used for ipv4 and ipv6)
 */
static void
sysctl_net_inet_tcp_setup2(struct sysctllog **clog, int pf, const char *pfname,
			   const char *tcpname)
{
	struct sysctlnode *sack_node;

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "net", NULL,
		       NULL, 0, NULL, 0,
		       CTL_NET, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, pfname, NULL,
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, tcpname,
		       SYSCTL_DESCR("TCP related settings"),
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "rfc1323",
		       SYSCTL_DESCR("Enable RFC1323 TCP extensions"),
		       NULL, 0, &tcp_do_rfc1323, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_RFC1323, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "sendspace",
		       SYSCTL_DESCR("Default TCP send buffer size"),
		       NULL, 0, &tcp_sendspace, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SENDSPACE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "recvspace",
		       SYSCTL_DESCR("Default TCP receive buffer size"),
		       NULL, 0, &tcp_recvspace, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_RECVSPACE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "mssdflt",
		       SYSCTL_DESCR("Default maximum segment size"),
		       sysctl_net_inet_tcp_mssdflt, 0, &tcp_mssdflt, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_MSSDFLT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "syn_cache_limit",
		       SYSCTL_DESCR("Maximum number of entries in the TCP "
				    "compressed state engine"),
		       NULL, 0, &tcp_syn_cache_limit, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SYN_CACHE_LIMIT,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "syn_bucket_limit",
		       SYSCTL_DESCR("Maximum number of entries per hash "
				    "bucket in the TCP compressed state "
				    "engine"),
		       NULL, 0, &tcp_syn_bucket_limit, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SYN_BUCKET_LIMIT,
		       CTL_EOL);
#if 0 /* obsoleted */
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "syn_cache_interval",
		       SYSCTL_DESCR("TCP compressed state engine's timer interval"),
		       NULL, 0, &tcp_syn_cache_interval, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SYN_CACHE_INTER,
		       CTL_EOL);
#endif
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "init_win",
		       SYSCTL_DESCR("Initial TCP congestion window"),
		       NULL, 0, &tcp_init_win, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_INIT_WIN, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "mss_ifmtu",
		       SYSCTL_DESCR("Use interface MTU for calculating MSS"),
		       NULL, 0, &tcp_mss_ifmtu, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_MSS_IFMTU, CTL_EOL);
	sysctl_createv(clog, 0, NULL, &sack_node,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "sack",
		       SYSCTL_DESCR("RFC2018 Selective ACKnowledgement tunables"),
		       NULL, 0, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SACK, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "win_scale",
		       SYSCTL_DESCR("Use RFC1323 window scale options"),
		       NULL, 0, &tcp_do_win_scale, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_WSCALE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "timestamps",
		       SYSCTL_DESCR("Use RFC1323 time stamp options"),
		       NULL, 0, &tcp_do_timestamps, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_TSTAMP, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "compat_42",
		       SYSCTL_DESCR("Enable workarounds for 4.2BSD TCP bugs"),
		       NULL, 0, &tcp_compat_42, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_COMPAT_42, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "cwm",
		       SYSCTL_DESCR("Hughes/Touch/Heidemann Congestion Window "
				    "Monitoring"),
		       NULL, 0, &tcp_cwm, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_CWM, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "cwm_burstsize",
		       SYSCTL_DESCR("Congestion Window Monitoring allowed "
				    "burst count in packets"),
		       NULL, 0, &tcp_cwm_burstsize, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_CWM_BURSTSIZE,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "ack_on_push",
		       SYSCTL_DESCR("Immediately return ACK when PSH is "
				    "received"),
		       NULL, 0, &tcp_ack_on_push, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_ACK_ON_PUSH, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "keepidle",
		       SYSCTL_DESCR("Allowed connection idle ticks before a "
				    "keepalive probe is sent"),
		       NULL, 0, &tcp_keepidle, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_KEEPIDLE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "keepintvl",
		       SYSCTL_DESCR("Ticks before next keepalive probe is sent"),
		       NULL, 0, &tcp_keepintvl, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_KEEPINTVL, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "keepcnt",
		       SYSCTL_DESCR("Number of keepalive probes to send"),
		       NULL, 0, &tcp_keepcnt, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_KEEPCNT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "slowhz",
		       SYSCTL_DESCR("Keepalive ticks per second"),
		       NULL, PR_SLOWHZ, NULL, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SLOWHZ, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "newreno",
		       SYSCTL_DESCR("NewReno congestion control algorithm"),
		       NULL, 0, &tcp_do_newreno, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_NEWRENO, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "log_refused",
		       SYSCTL_DESCR("Log refused TCP connections"),
		       NULL, 0, &tcp_log_refused, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_LOG_REFUSED, CTL_EOL);
#if 0 /* obsoleted */
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "rstratelimit", NULL,
		       NULL, 0, &tcp_rst_ratelim, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_RSTRATELIMIT, CTL_EOL);
#endif
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "rstppslimit",
		       SYSCTL_DESCR("Maximum number of RST packets to send "
				    "per second"),
		       NULL, 0, &tcp_rst_ppslim, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_RSTPPSLIMIT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "delack_ticks",
		       SYSCTL_DESCR("Number of ticks to delay sending an ACK"),
		       NULL, 0, &tcp_delack_ticks, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_DELACK_TICKS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "init_win_local",
		       SYSCTL_DESCR("Initial TCP window size (in segments)"),
		       NULL, 0, &tcp_init_win_local, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_INIT_WIN_LOCAL,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRUCT, "ident",
		       SYSCTL_DESCR("RFC1413 Identification Protocol lookups"),
		       sysctl_net_inet_tcp_ident, 0, NULL, sizeof(uid_t),
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_IDENT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "do_loopback_cksum",
		       SYSCTL_DESCR("Perform TCP checksum on loopback"),
		       NULL, 0, &tcp_do_loopback_cksum, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_LOOPBACKCKSUM,
		       CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "pcblist",
		       SYSCTL_DESCR("TCP protocol control block list"),
		       sysctl_inpcblist, 0, &tcbtable, 0,
		       CTL_NET, pf, IPPROTO_TCP, CTL_CREATE,
		       CTL_EOL);

	/* SACK gets it's own little subtree. */
	sysctl_createv(clog, 0, NULL, &sack_node,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "enable",
		       SYSCTL_DESCR("Enable RFC2018 Selective ACKnowledgement"),
		       NULL, 0, &tcp_do_sack, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SACK, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, &sack_node,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxholes",
		       SYSCTL_DESCR("Maximum number of TCP SACK holes allowed per connection"),
		       NULL, 0, &tcp_sack_tp_maxholes, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SACK, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, &sack_node,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "globalmaxholes",
		       SYSCTL_DESCR("Global maximum number of TCP SACK holes"),
		       NULL, 0, &tcp_sack_globalmaxholes, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SACK, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, &sack_node,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "globalholes",
		       SYSCTL_DESCR("Global number of TCP SACK holes"),
		       NULL, 0, &tcp_sack_globalholes, 0,
		       CTL_NET, pf, IPPROTO_TCP, TCPCTL_SACK, CTL_CREATE, CTL_EOL);
}

/*
 * Sysctl for tcp variables.
 */
#ifdef INET
SYSCTL_SETUP(sysctl_net_inet_tcp_setup, "sysctl net.inet.tcp subtree setup")
{

	sysctl_net_inet_tcp_setup2(clog, PF_INET, "inet", "tcp");
}
#endif /* INET */

#ifdef INET6
SYSCTL_SETUP(sysctl_net_inet6_tcp6_setup, "sysctl net.inet6.tcp6 subtree setup")
{

	sysctl_net_inet_tcp_setup2(clog, PF_INET6, "inet6", "tcp6");
}
#endif /* INET6 */
