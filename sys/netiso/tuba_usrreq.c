/*	$NetBSD: tuba_usrreq.c,v 1.11.10.1 1997/10/14 10:29:54 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
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
 *	@(#)tuba_usrreq.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/stat.h>

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

#include <netiso/tp_param.h>
#include <netiso/argo_debug.h>
#include <netiso/iso.h>
#include <netiso/clnp.h>
#include <netiso/iso_pcb.h>
#include <netiso/iso_var.h>
#include <netiso/tuba_table.h>
#include <netiso/tp_var.h>

/*
 * TCP protocol interface to socket abstraction.
 */
extern char    *tcpstates[];
extern struct inpcbtable tuba_inpcb;
extern struct isopcb tuba_isopcb;

/*
 * Process a TCP user request for TCP tb.  If this is a send request
 * then m is the mbuf chain of send data.  If this is a timer expiration
 * (called from the software clock routine), then timertype tells which timer.
 */
/* ARGSUSED */
int
tuba_usrreq(so, req, m, nam, control, p)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
	struct proc *p;
{
	register struct inpcb *inp;
	register struct isopcb *isop = NULL;
	register struct tcpcb *tp = NULL;
	int             s;
	int             error = 0;
	int             ostate;
	struct sockaddr_iso *siso;

	if (req == PRU_CONTROL)
		return (iso_control(so, (long)m, (caddr_t)nam,
		    (struct ifnet *)control, p));

	s = splsoftnet();
	inp = sotoinpcb(so);
	/*
	 * When a TCP is attached to a socket, then there will be
	 * a (struct inpcb) pointed at by the socket, and this
	 * structure will point at a subsidary (struct tcpcb).
	 */
	if (inp == 0 && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}
	if (inp) {
		tp = intotcpcb(inp);
		if (tp == 0)
			panic("tuba_usrreq");
		ostate = tp->t_state;
		isop = (struct isopcb *)tp->t_tuba_pcb;
		if (isop == 0)
			panic("tuba_usrreq 2");
	} else
		ostate = 0;
	switch (req) {

		/*
		 * TCP attaches to socket via PRU_ATTACH, reserving space,
		 * and an internet control block.  We also need to
		 * allocate an isopcb and separate the control block from
		 * tcp/ip ones.
		 */
	case PRU_ATTACH:
		if ((error = iso_pcballoc(so, &tuba_isopcb)) != 0)
			break;
		isop = (struct isopcb *) so->so_pcb;
		so->so_pcb = 0;
		if ((error = tcp_usrreq(so, req, m, nam, control, p)) != 0) {
			isop->isop_socket = 0;
			iso_pcbdetach(isop);
		} else {
			inp = sotoinpcb(so);
			CIRCLEQ_REMOVE(&inp->inp_table->inpt_queue, inp,
				       inp_queue);
			CIRCLEQ_INSERT_HEAD(&tuba_inpcb.inpt_queue, inp,
					    inp_queue);
			inp->inp_table = &tuba_inpcb;
			tp = intotcpcb(inp);
			if (tp == 0)
				panic("tuba_usrreq 3");
			tp->t_tuba_pcb = (caddr_t) isop;
		}
		goto notrace;

		/*
		 * PRU_DETACH detaches the TCP protocol from the socket.
		 * If the protocol state is non-embryonic, then can't
		 * do this directly: have to initiate a PRU_DISCONNECT,
		 * which may finish later; embryonic TCB's can just
		 * be discarded here.
		 */
	case PRU_DETACH:
		if (tp->t_state > TCPS_LISTEN)
			tp = tcp_disconnect(tp);
		else
			tp = tcp_close(tp);
		if (tp == 0)
			tuba_pcbdetach(isop);
		break;

		/*
		 * Give the socket an address.
		 */
	case PRU_BIND:
		siso = mtod(nam, struct sockaddr_iso *);
		if (siso->siso_tlen && siso->siso_tlen != 2) {
			error = EINVAL;
			break;
		}
		if ((error = iso_pcbbind(isop, nam, p)) ||
		    (siso = isop->isop_laddr) == 0)
			break;
		bcopy(TSEL(siso), &inp->inp_lport, 2);
		if (siso->siso_nlen &&
		    !(inp->inp_laddr.s_addr = tuba_lookup(siso, M_WAITOK)))
			error = ENOBUFS;
		break;

	case PRU_LISTEN:
		if (inp->inp_lport == 0) {
			error = iso_pcbbind(isop, (struct mbuf *)0,
			    (struct proc *)0);
			if (error)
				break;
		}
		bcopy(TSEL(isop->isop_laddr), &inp->inp_lport, 2);
		tp->t_state = TCPS_LISTEN;
		break;

		/*
		 * Prepare to accept connections.
		 */
	case PRU_CONNECT:
		if (inp->inp_lport == 0) {
			error = iso_pcbbind(isop, (struct mbuf *)0,
			    (struct proc *)0);
			if (error)
				break;
		}
		bcopy(TSEL(isop->isop_laddr), &inp->inp_lport, 2);
		/*
		 * Initiate connection to peer.
		 * Create a template for use in transmissions on this connection.
		 * Enter SYN_SENT state, and mark socket as connecting.
		 * Start keep-alive timer, and seed output sequence space.
		 * Send initial segment on connection.
		 */
		if ((error = iso_pcbconnect(isop, nam)) != 0)
			break;
		if ((siso = isop->isop_laddr) && siso->siso_nlen > 1)
			siso->siso_data[siso->siso_nlen - 1] = ISOPROTO_TCP;
		else
			panic("tuba_usrreq: connect");
		siso = mtod(nam, struct sockaddr_iso *);
		if (!(inp->inp_faddr.s_addr = tuba_lookup(siso, M_WAITOK))) {
	unconnect:
			iso_pcbdisconnect(isop);
			error = ENOBUFS;
			break;
		}
		bcopy(TSEL(isop->isop_faddr), &inp->inp_fport, 2);
		if (inp->inp_laddr.s_addr == 0 &&
		    (inp->inp_laddr.s_addr =
		     tuba_lookup(isop->isop_laddr, M_WAITOK)) == 0)
			goto unconnect;
		if ((tp->t_template = tcp_template(tp)) == 0)
			goto unconnect;
		soisconnecting(so);
		tcpstat.tcps_connattempt++;
		tp->t_state = TCPS_SYN_SENT;
		tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
		tp->iss = tcp_new_iss(tp, sizeof(tp), 0);
		tcp_sendseqinit(tp);
		error = tcp_output(tp);
		tuba_refcnt(isop, 1);
		break;

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
		if ((tp = tcp_disconnect(tp)) == 0)
			tuba_pcbdetach(isop);
		break;

		/*
		 * Accept a connection.  Essentially all the work is
		 * done at higher levels; just return the address
		 * of the peer, storing through addr.
		 */
	case PRU_ACCEPT:
		iso_getnetaddr(isop, nam, TP_FOREIGN);
		break;

		/*
		 * Mark the connection as being incapable of further output.
		 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		tp = tcp_usrclosed(tp);
		if (tp)
			error = tcp_output(tp);
		else
			tuba_pcbdetach(isop);
		break;

		/*
		 * Abort the TCP.
		 */
	case PRU_ABORT:
		if ((tp = tcp_drop(tp, ECONNABORTED)) == 0)
			tuba_pcbdetach(isop);
		break;


	case PRU_SOCKADDR:
		iso_getnetaddr(isop, nam, TP_LOCAL);
		break;

	case PRU_PEERADDR:
		iso_getnetaddr(isop, nam, TP_FOREIGN);
		break;

	default:
		error = tcp_usrreq(so, req, m, nam, control, p);
		goto notrace;
	}
	if (tp && (so->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, (struct tcpiphdr *)0, req);
notrace:
release:
	splx(s);
	return (error);
}

int
tuba_ctloutput(op, so, level, optname, mp)
	int             op;
	struct socket  *so;
	int             level, optname;
	struct mbuf   **mp;
{
	if (level != IPPROTO_TCP)
		return rclnp_ctloutput(op, so, level, optname, mp);
	else
		return tcp_ctloutput(op, so, level, optname, mp);
}
