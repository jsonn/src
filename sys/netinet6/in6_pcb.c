/*	$NetBSD: in6_pcb.c,v 1.21.2.1 2000/06/22 17:09:56 minoura Exp $	*/
/*	$KAME: in6_pcb.c,v 1.45 2000/06/05 00:41:58 itojun Exp $	*/

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

/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 *	@(#)in_pcb.c	8.2 (Berkeley) 1/4/94
 */

#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/nd6.h>

#include "loop.h"
extern struct ifnet loif[NLOOP];
#include "faith.h"

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#include <netkey/key_debug.h>
#endif /* IPSEC */

struct in6_addr zeroin6_addr;

int
in6_pcballoc(so, head)
	struct socket *so;
	struct in6pcb *head;
{
	struct in6pcb *in6p;

	MALLOC(in6p, struct in6pcb *, sizeof(*in6p), M_PCB, M_NOWAIT);
	if (in6p == NULL)
		return(ENOBUFS);
	bzero((caddr_t)in6p, sizeof(*in6p));
	in6p->in6p_head = head;
	in6p->in6p_socket = so;
	in6p->in6p_hops = -1;	/* use kernel default */
	in6p->in6p_icmp6filt = NULL;
	in6p->in6p_next = head->in6p_next;
	head->in6p_next = in6p;
	in6p->in6p_prev = head;
	in6p->in6p_next->in6p_prev = in6p;
#ifndef INET6_BINDV6ONLY
	if (ip6_bindv6only)
		in6p->in6p_flags |= IN6P_BINDV6ONLY;
#else
	in6p->in6p_flags |= IN6P_BINDV6ONLY;	/*just for safety*/
#endif
	so->so_pcb = (caddr_t)in6p;
	return(0);
}

int
in6_pcbbind(in6p, nam, p)
	register struct in6pcb *in6p;
	struct mbuf *nam;
	struct proc *p;
{
	struct socket *so = in6p->in6p_socket;
	struct in6pcb *head = in6p->in6p_head;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)NULL;
	u_int16_t lport = 0;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);
	int error;

	if (in6p->in6p_lport || !IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr))
		return(EINVAL);
	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0 &&
	   ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
	    (so->so_options & SO_ACCEPTCONN) == 0))
		wild = IN6PLOOKUP_WILDCARD;
	if (nam) {
		sin6 = mtod(nam, struct sockaddr_in6 *);
		if (nam->m_len != sizeof(*sin6))
			return(EINVAL);
		/*
		 * We should check the family, but old programs
		 * incorrectly fail to intialize it.
		 */
		if (sin6->sin6_family != AF_INET6)
			return(EAFNOSUPPORT);

		/*
		 * since we do not check port number duplicate with IPv4 space,
		 * we reject it at this moment.  If we leave it, we make normal
		 * user to hijack port number from other users.
		 */
		if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
			return(EADDRNOTAVAIL);

		/*
		 * If the scope of the destination is link-local, embed the
		 * interface index in the address.
		 */
		if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr)) {
			/* XXX boundary check is assumed to be already done. */
			/* XXX sin6_scope_id is weaker than advanced-api. */
			struct in6_pktinfo *pi;
			if (in6p->in6p_outputopts &&
			    (pi = in6p->in6p_outputopts->ip6po_pktinfo) &&
			    pi->ipi6_ifindex) {
				sin6->sin6_addr.s6_addr16[1]
					= htons(pi->ipi6_ifindex);
			} else if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)
				&& in6p->in6p_moptions
				&& in6p->in6p_moptions->im6o_multicast_ifp) {
				sin6->sin6_addr.s6_addr16[1] =
					htons(in6p->in6p_moptions->im6o_multicast_ifp->if_index);
			} else if (sin6->sin6_scope_id) {
				/* boundary check */
				if (sin6->sin6_scope_id < 0
				 || if_index < sin6->sin6_scope_id) {
					return ENXIO;  /* XXX EINVAL? */
				}
				sin6->sin6_addr.s6_addr16[1]
					= htons(sin6->sin6_scope_id & 0xffff);/*XXX*/
				/* this must be cleared for ifa_ifwithaddr() */
				sin6->sin6_scope_id = 0;
			}
		}

		lport = sin6->sin6_port;
		if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
			/*
			 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
			 * allow compepte duplication of binding if
			 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
			 * and a multicast address is bound on both
			 * new and duplicated sockets.
			 */
			if (so->so_options & SO_REUSEADDR)
				reuseport = SO_REUSEADDR|SO_REUSEPORT;
		} else if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
			struct sockaddr_in sin;

			bzero(&sin, sizeof(sin));
			sin.sin_len = sizeof(sin);
			sin.sin_family = AF_INET;
			bcopy(&sin6->sin6_addr.s6_addr32[3], &sin.sin_addr,
				sizeof(sin.sin_addr));
			if (ifa_ifwithaddr((struct sockaddr *)&sin) == 0)
				return EADDRNOTAVAIL;
		} else if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			struct ifaddr *ia = NULL;

			sin6->sin6_port = 0;		/* yech... */
#if defined(NFAITH) && NFAITH > 0
			if ((in6p->in6p_flags & IN6P_FAITH) == 0
			 && (ia = ifa_ifwithaddr((struct sockaddr *)sin6)) == 0)
#else
			if ((ia = ifa_ifwithaddr((struct sockaddr *)sin6)) == 0)
#endif
				return(EADDRNOTAVAIL);

			/*
			 * XXX: bind to an anycast address might accidentally
			 * cause sending a packet with anycast source address.
			 */
			if (ia &&
			    ((struct in6_ifaddr *)ia)->ia6_flags &
			    (IN6_IFF_ANYCAST|IN6_IFF_NOTREADY|
			     IN6_IFF_DETACHED|IN6_IFF_DEPRECATED)) {
				return(EADDRNOTAVAIL);
			}
		}
		if (lport) {
#ifndef IPNOPRIVPORTS
			/* GROSS */
			if (ntohs(lport) < IPV6PORT_RESERVED &&
			    (p == 0 ||
			     (error = suser(p->p_ucred, &p->p_acflag))))
				return(EACCES);
#endif

			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
				/* should check this but we can't ... */
#if 0
				struct inpcb *t;

				t = in_pcblookup_bind(&tcbtable,
					(struct in_addr *)&sin6->sin6_addr.s6_addr32[3],
					lport);
				if (t && (reuseport & t->inp_socket->so_options) == 0)
					return EADDRINUSE;
#endif
			} else {
				struct in6pcb *t;

				t = in6_pcblookup(head, &zeroin6_addr, 0,
						  &sin6->sin6_addr, lport, wild);
				if (t && (reuseport & t->in6p_socket->so_options) == 0)
					return(EADDRINUSE);
			}
		}
		in6p->in6p_laddr = sin6->sin6_addr;
	}

	if (lport == 0) {
		int e;
		if ((e = in6_pcbsetport(&in6p->in6p_laddr, in6p)) != 0)
			return(e);
	}
	else
		in6p->in6p_lport = lport;

	in6p->in6p_flowinfo = sin6 ? sin6->sin6_flowinfo : 0;	/*XXX*/
	return(0);
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin6.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in6_pcbconnect(in6p, nam)
	struct in6pcb *in6p;
	struct mbuf *nam;
{
	struct in6_addr *in6a = NULL;
	struct sockaddr_in6 *sin6 = mtod(nam, struct sockaddr_in6 *);
	struct in6_pktinfo *pi;
	struct ifnet *ifp = NULL;	/* outgoing interface */
	int error = 0;
	struct in6_addr mapped;
	struct sockaddr_in6 tmp;

	(void)&in6a;				/* XXX fool gcc */

	if (nam->m_len != sizeof(*sin6))
		return(EINVAL);
	if (sin6->sin6_family != AF_INET6)
		return(EAFNOSUPPORT);
	if (sin6->sin6_port == 0)
		return(EADDRNOTAVAIL);

	/* sanity check for mapped address case */
	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
		if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr))
			in6p->in6p_laddr.s6_addr16[5] = htons(0xffff);
		if (!IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr))
			return EINVAL;
	} else {
		if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr))
			return EINVAL;
	}

	/* protect *sin6 from overwrites */
	tmp = *sin6;
	sin6 = &tmp;

	/*
	 * If the scope of the destination is link-local, embed the interface
	 * index in the address.
	 */
	if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr)) {
		/* XXX boundary check is assumed to be already done. */
		/* XXX sin6_scope_id is weaker than advanced-api. */
		if (in6p->in6p_outputopts &&
		    (pi = in6p->in6p_outputopts->ip6po_pktinfo) &&
		    pi->ipi6_ifindex) {
			sin6->sin6_addr.s6_addr16[1] = htons(pi->ipi6_ifindex);
			ifp = ifindex2ifnet[pi->ipi6_ifindex];
		}
		else if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr) &&
			 in6p->in6p_moptions &&
			 in6p->in6p_moptions->im6o_multicast_ifp) {
			sin6->sin6_addr.s6_addr16[1] =
				htons(in6p->in6p_moptions->im6o_multicast_ifp->if_index);
			ifp = ifindex2ifnet[in6p->in6p_moptions->im6o_multicast_ifp->if_index];
		} else if (sin6->sin6_scope_id) {
			/* boundary check */
			if (sin6->sin6_scope_id < 0
			 || if_index < sin6->sin6_scope_id) {
				return ENXIO;  /* XXX EINVAL? */
			}
			sin6->sin6_addr.s6_addr16[1]
				= htons(sin6->sin6_scope_id & 0xffff);/*XXX*/
			ifp = ifindex2ifnet[sin6->sin6_scope_id];
		}
	}

	/* Source address selection. */
	if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr)
	 && in6p->in6p_laddr.s6_addr32[3] == 0) {
		struct sockaddr_in sin, *sinp;

		bzero(&sin, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		bcopy(&sin6->sin6_addr.s6_addr32[3], &sin.sin_addr,
			sizeof(sin.sin_addr));
		sinp = in_selectsrc(&sin, (struct route *)&in6p->in6p_route,
			in6p->in6p_socket->so_options, NULL, &error);
		if (sinp == 0) {
			if (error == 0)
				error = EADDRNOTAVAIL;
			return(error);
		}
		bzero(&mapped, sizeof(mapped));
		mapped.s6_addr16[5] = htons(0xffff);
		bcopy(&sinp->sin_addr, &mapped.s6_addr32[3], sizeof(sinp->sin_addr));
		in6a = &mapped;
	} else {
		/*
		 * XXX: in6_selectsrc might replace the bound local address
		 * with the address specified by setsockopt(IPV6_PKTINFO).
		 * Is it the intended behavior?
		 */
		in6a = in6_selectsrc(sin6, in6p->in6p_outputopts,
				     in6p->in6p_moptions,
				     &in6p->in6p_route,
				     &in6p->in6p_laddr, &error);
		if (in6a == 0) {
			if (error == 0)
				error = EADDRNOTAVAIL;
			return(error);
		}
	}
	if (in6p->in6p_route.ro_rt)
		ifp = in6p->in6p_route.ro_rt->rt_ifp;

	in6p->in6p_ip6.ip6_hlim = (u_int8_t)in6_selecthlim(in6p, ifp);

	if (in6_pcblookup(in6p->in6p_head,
			 &sin6->sin6_addr,
			 sin6->sin6_port,
			 IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr) ?
			  in6a : &in6p->in6p_laddr,
			 in6p->in6p_lport,
			 0))
		return(EADDRINUSE);
	if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr)
	 || (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr)
	  && in6p->in6p_laddr.s6_addr32[3] == 0)) {
		if (in6p->in6p_lport == 0)
			(void)in6_pcbbind(in6p, (struct mbuf *)0,
			    (struct proc *)0);
		in6p->in6p_laddr = *in6a;
	}
	in6p->in6p_faddr = sin6->sin6_addr;
	in6p->in6p_fport = sin6->sin6_port;
	/*
	 * xxx kazu flowlabel is necessary for connect?
	 * but if this line is missing, the garbage value remains.
	 */
	in6p->in6p_flowinfo = sin6->sin6_flowinfo;
	return(0);
}

void
in6_pcbdisconnect(in6p)
	struct in6pcb *in6p;
{
	bzero((caddr_t)&in6p->in6p_faddr, sizeof(in6p->in6p_faddr));
	in6p->in6p_fport = 0;
	if (in6p->in6p_socket->so_state & SS_NOFDREF)
		in6_pcbdetach(in6p);
}

void
in6_pcbdetach(in6p)
	struct in6pcb *in6p;
{
	struct socket *so = in6p->in6p_socket;

#ifdef IPSEC
	ipsec6_delete_pcbpolicy(in6p);
#endif /* IPSEC */
	sotoin6pcb(so) = 0;
	sofree(so);
	if (in6p->in6p_options)
		m_freem(in6p->in6p_options);
	if (in6p->in6p_outputopts) {
		if (in6p->in6p_outputopts->ip6po_rthdr &&
		    in6p->in6p_outputopts->ip6po_route.ro_rt)
			RTFREE(in6p->in6p_outputopts->ip6po_route.ro_rt);
		if (in6p->in6p_outputopts->ip6po_m)
			(void)m_free(in6p->in6p_outputopts->ip6po_m);
		free(in6p->in6p_outputopts, M_IP6OPT);
	}
	if (in6p->in6p_route.ro_rt)
		rtfree(in6p->in6p_route.ro_rt);
	ip6_freemoptions(in6p->in6p_moptions);
	in6p->in6p_next->in6p_prev = in6p->in6p_prev;
	in6p->in6p_prev->in6p_next = in6p->in6p_next;
	in6p->in6p_prev = NULL;
	FREE(in6p, M_PCB);
}

void
in6_setsockaddr(in6p, nam)
	struct in6pcb *in6p;
	struct mbuf *nam;
{
	struct sockaddr_in6 *sin6;

	nam->m_len = sizeof(*sin6);
	sin6 = mtod(nam, struct sockaddr_in6 *);
	bzero((caddr_t)sin6, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_port = in6p->in6p_lport;
	sin6->sin6_addr = in6p->in6p_laddr;
	if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr))
		sin6->sin6_scope_id = ntohs(sin6->sin6_addr.s6_addr16[1]);
	else
		sin6->sin6_scope_id = 0;	/*XXX*/
	if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr))
		sin6->sin6_addr.s6_addr16[1] = 0;
}

void
in6_setpeeraddr(in6p, nam)
	struct in6pcb *in6p;
	struct mbuf *nam;
{
	struct sockaddr_in6 *sin6;

	nam->m_len = sizeof(*sin6);
	sin6 = mtod(nam, struct sockaddr_in6 *);
	bzero((caddr_t)sin6, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_port = in6p->in6p_fport;
	sin6->sin6_addr = in6p->in6p_faddr;
	if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr))
		sin6->sin6_scope_id = ntohs(sin6->sin6_addr.s6_addr16[1]);
	else
		sin6->sin6_scope_id = 0;	/*XXX*/
	if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr))
		sin6->sin6_addr.s6_addr16[1] = 0;
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The local address and/or port numbers
 * may be specified to limit the search.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 *
 * Must be called at splsoftnet.
 */
int
in6_pcbnotify(head, dst, fport_arg, laddr6, lport_arg, cmd, notify)
	struct in6pcb *head;
	struct sockaddr *dst;
	u_int fport_arg, lport_arg;
	struct in6_addr *laddr6;
	int cmd;
	void (*notify) __P((struct in6pcb *, int));
{
	struct in6pcb *in6p, *nin6p;
	struct in6_addr faddr6;
	u_int16_t fport = fport_arg, lport = lport_arg;
	int errno;
	int nmatch = 0;
	void (*notify2) __P((struct in6pcb *, int));

	notify2 = NULL;

	if ((unsigned)cmd > PRC_NCMDS || dst->sa_family != AF_INET6)
		return 0;
	faddr6 = ((struct sockaddr_in6 *)dst)->sin6_addr;
	if (IN6_IS_ADDR_UNSPECIFIED(&faddr6))
		return 0;

	/*
	 * Redirects go to all references to the destination,
	 * and use in6_rtchange to invalidate the route cache.
	 * Dead host indications: also use in6_rtchange to invalidate
	 * the cache, and deliver the error to all the sockets.
	 * Otherwise, if we have knowledge of the local port and address,
	 * deliver only to that socket.
	 */
	if (PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD) {
		fport = 0;
		lport = 0;
		bzero((caddr_t)laddr6, sizeof(*laddr6));

		/*
		 * Keep the old notify function to store a soft error
		 * in each PCB.
		 */
		if (cmd == PRC_HOSTDEAD && notify != in6_rtchange)
			notify2 = notify;

		notify = in6_rtchange;
	}

	if (notify == NULL)
		return 0;

	errno = inet6ctlerrmap[cmd];
	for (in6p = head->in6p_next; in6p != head; in6p = nin6p) {
		nin6p = in6p->in6p_next;

		if (notify == in6_rtchange) {
			/*
			 * Since a non-connected PCB might have a cached route,
			 * we always call in6_rtchange without matching
			 * the PCB to the src/dst pair.
			 *
			 * XXX: we assume in6_rtchange does not free the PCB.
			 */
			if (IN6_ARE_ADDR_EQUAL(&in6p->in6p_route.ro_dst.sin6_addr,
					       &faddr6))
				in6_rtchange(in6p, errno);

			if (notify2 == NULL)
				continue;

			notify = notify2;
		}

		/* at this point, we can assume that NOTIFY is not NULL. */

		if (!IN6_ARE_ADDR_EQUAL(&in6p->in6p_faddr, &faddr6) ||
		    in6p->in6p_socket == 0 ||
		    (lport && in6p->in6p_lport != lport) ||
		    (!IN6_IS_ADDR_UNSPECIFIED(laddr6) &&
		     !IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, laddr6)) ||
		    (fport && in6p->in6p_fport != fport))
			continue;

		(*notify)(in6p, errno);
		nmatch++;
	}
	return nmatch;
}

void
in6_pcbpurgeif(head, ifp)
	struct in6pcb *head;
	struct ifnet *ifp;
{
	struct in6pcb *in6p, *nin6p;

	for (in6p = head->in6p_next; in6p != head; in6p = nin6p) {
		nin6p = in6p->in6p_next;
		if (in6p->in6p_route.ro_rt != NULL &&
		    in6p->in6p_route.ro_rt->rt_ifp == ifp)
			in6_rtchange(in6p, 0);
	}
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in6_losing(in6p)
	struct in6pcb *in6p;
{
	struct rtentry *rt;
	struct rt_addrinfo info;

	if ((rt = in6p->in6p_route.ro_rt) != NULL) {
		in6p->in6p_route.ro_rt = 0;
		bzero((caddr_t)&info, sizeof(info));
		info.rti_info[RTAX_DST] =
			(struct sockaddr *)&in6p->in6p_route.ro_dst;
		info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		info.rti_info[RTAX_NETMASK] = rt_mask(rt);
		rt_missmsg(RTM_LOSING, &info, rt->rt_flags, 0);
		if (rt->rt_flags & RTF_DYNAMIC)
			(void)rtrequest(RTM_DELETE, rt_key(rt),
					rt->rt_gateway, rt_mask(rt), rt->rt_flags,
					(struct rtentry **)0);
		else
		/*
		 * A new route can be allocated
		 * the next time output is attempted.
		 */
			rtfree(rt);
	}
}

/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
void
in6_rtchange(in6p, errno)
	struct in6pcb *in6p;
	int errno;
{
	if (in6p->in6p_route.ro_rt) {
		rtfree(in6p->in6p_route.ro_rt);
		in6p->in6p_route.ro_rt = 0;
		/*
		 * A new route can be allocated the next time
		 * output is attempted.
		 */
	}
}

struct in6pcb *
in6_pcblookup(head, faddr6, fport_arg, laddr6, lport_arg, flags)
	struct in6pcb *head;
	struct in6_addr *faddr6, *laddr6;
	u_int fport_arg, lport_arg;
	int flags;
{
	struct in6pcb *in6p, *match = 0;
	int matchwild = 3, wildcard;
	u_int16_t fport = fport_arg, lport = lport_arg;

	for (in6p = head->in6p_next; in6p != head; in6p = in6p->in6p_next) {
		if (in6p->in6p_lport != lport)
			continue;
		wildcard = 0;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr)) {
			if (IN6_IS_ADDR_UNSPECIFIED(laddr6))
				wildcard++;
			else if (!IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, laddr6))
				continue;
		}
#ifndef TCP6
		else if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr)
			&& in6p->in6p_laddr.s6_addr32[3] == 0) {
			if (!IN6_IS_ADDR_V4MAPPED(laddr6))
				continue;
			if (laddr6->s6_addr32[3] == 0)
				;
			else
				wildcard++;
		}
#endif
		else {
			if (IN6_IS_ADDR_V4MAPPED(laddr6)) {
#if !defined(TCP6) && !defined(INET6_BINDV6ONLY)
				if (in6p->in6p_flags & IN6P_BINDV6ONLY)
					continue;
				else
					wildcard++;
#else
				continue;
#endif
			} else if (!IN6_IS_ADDR_UNSPECIFIED(laddr6))
				wildcard++;
		}
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr)) {
			if (IN6_IS_ADDR_UNSPECIFIED(faddr6))
				wildcard++;
			else if (!IN6_ARE_ADDR_EQUAL(&in6p->in6p_faddr, faddr6)
			      || in6p->in6p_fport != fport)
				continue;
		}
#ifndef TCP6
		else if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_faddr)
			&& in6p->in6p_faddr.s6_addr32[3] == 0) {
			if (!IN6_IS_ADDR_V4MAPPED(faddr6))
				continue;
			if (faddr6->s6_addr32[3] == 0)
				;
			else
				wildcard++;
		}
#endif
		else {
			if (IN6_IS_ADDR_V4MAPPED(faddr6)) {
#if !defined(TCP6) && !defined(INET6_BINDV6ONLY)
				if (in6p->in6p_flags & IN6P_BINDV6ONLY)
					continue;
				else
					wildcard++;
#else
				continue;
#endif
			} else if (!IN6_IS_ADDR_UNSPECIFIED(faddr6))
				wildcard++;
		}

		if (wildcard && (flags & IN6PLOOKUP_WILDCARD) == 0)
			continue;
		if (wildcard < matchwild) {
			match = in6p;
			matchwild = wildcard;
			if (matchwild == 0)
				break;
		}
	}
	return(match);
}

#ifndef TCP6
struct rtentry *
in6_pcbrtentry(in6p)
	struct in6pcb *in6p;
{
	struct route_in6 *ro;

	ro = &in6p->in6p_route;

	if (ro->ro_rt == NULL) {
		/*
		 * No route yet, so try to acquire one.
		 */
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr)) {
			bzero(&ro->ro_dst, sizeof(ro->ro_dst));
			ro->ro_dst.sin6_family = AF_INET6;
			ro->ro_dst.sin6_len = sizeof(struct sockaddr_in6);
			satosin6(&ro->ro_dst)->sin6_addr = in6p->in6p_faddr;
			rtalloc((struct route *)ro);
		}
	}
	return (ro->ro_rt);
}

struct in6pcb *
in6_pcblookup_connect(head, faddr6, fport_arg, laddr6, lport_arg, faith)
	struct in6pcb *head;
	struct in6_addr *faddr6, *laddr6;
	u_int fport_arg, lport_arg;
	int faith;
{
	struct in6pcb *in6p;
	u_int16_t fport = fport_arg, lport = lport_arg;

	for (in6p = head->in6p_next; in6p != head; in6p = in6p->in6p_next) {
#if defined(NFAITH) && NFAITH > 0
		if (faith && (in6p->in6p_flags & IN6P_FAITH) == 0)
			continue;
#endif
		/* find exact match on both source and dest */
		if (in6p->in6p_fport != fport)
			continue;
		if (in6p->in6p_lport != lport)
			continue;
		if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr))
			continue;
		if (!IN6_ARE_ADDR_EQUAL(&in6p->in6p_faddr, faddr6))
			continue;
		if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr))
			continue;
		if (!IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, laddr6))
			continue;
		return in6p;
	}
	return NULL;
}

struct in6pcb *
in6_pcblookup_bind(head, laddr6, lport_arg, faith)
	struct in6pcb *head;
	struct in6_addr *laddr6;
	u_int lport_arg;
	int faith;
{
	struct in6pcb *in6p, *match;
	u_int16_t lport = lport_arg;

	match = NULL;
	for (in6p = head->in6p_next; in6p != head; in6p = in6p->in6p_next) {
		/*
	 	 * find destination match.  exact match is preferred
		 * against wildcard match.
		 */
#if defined(NFAITH) && NFAITH > 0
		if (faith && (in6p->in6p_flags & IN6P_FAITH) == 0)
			continue;
#endif
		if (in6p->in6p_fport != 0)
			continue;
		if (in6p->in6p_lport != lport)
			continue;
		if (IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr)) {
			if (IN6_IS_ADDR_V4MAPPED(laddr6)) {
#ifndef INET6_BINDV6ONLY
				if (in6p->in6p_flags & IN6P_BINDV6ONLY)
					continue;
				else
					match = in6p;
#else
				continue;
#endif
			} else
				match = in6p;
		}
		else if (IN6_IS_ADDR_V4MAPPED(&in6p->in6p_laddr) &&
			 in6p->in6p_laddr.s6_addr32[3] == 0) {
			if (IN6_IS_ADDR_V4MAPPED(laddr6) &&
			    laddr6->s6_addr32[3] != 0)
				match = in6p;
		}
		else if (IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, laddr6))
			return in6p;
	}
	return match;
}
#endif
