/*	$NetBSD: raw_ip6.c,v 1.22.2.1 2000/06/22 17:10:05 minoura Exp $	*/
/*	$KAME: raw_ip6.c,v 1.28 2000/05/28 23:25:07 itojun Exp $	*/

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
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)raw_ip.c	8.2 (Berkeley) 1/4/94
 */

#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6_mroute.h>
#include <netinet/icmp6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6protosw.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#endif /*IPSEC*/

#include <machine/stdarg.h>

#include "faith.h"

struct	in6pcb rawin6pcb;
#define ifatoia6(ifa)	((struct in6_ifaddr *)(ifa))

/*
 * Raw interface to IP6 protocol.
 */

/*
 * Initialize raw connection block queue.
 */
void
rip6_init()
{
	rawin6pcb.in6p_next = rawin6pcb.in6p_prev = &rawin6pcb;
}

/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
int
rip6_input(mp, offp, proto)
	struct	mbuf **mp;
	int	*offp, proto;
{
	struct mbuf *m = *mp;
	register struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	register struct in6pcb *in6p;
	struct in6pcb *last = NULL;
	struct sockaddr_in6 rip6src;
	struct mbuf *opts = NULL;

#if defined(NFAITH) && 0 < NFAITH
	if (m->m_pkthdr.rcvif) {
		if (m->m_pkthdr.rcvif->if_type == IFT_FAITH) {
			/* send icmp6 host unreach? */
			m_freem(m);
			return IPPROTO_DONE;
		}
	}
#endif

	/* Be proactive about malicious use of IPv4 mapped address */
	if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		/* XXX stat */
		m_freem(m);
		return IPPROTO_DONE;
	}

	bzero(&rip6src, sizeof(rip6src));
	rip6src.sin6_len = sizeof(struct sockaddr_in6);
	rip6src.sin6_family = AF_INET6;
	rip6src.sin6_addr = ip6->ip6_src;
	if (IN6_IS_SCOPE_LINKLOCAL(&rip6src.sin6_addr))
		rip6src.sin6_addr.s6_addr16[1] = 0;
	if (m->m_pkthdr.rcvif) {
		if (IN6_IS_SCOPE_LINKLOCAL(&rip6src.sin6_addr))
			rip6src.sin6_scope_id = m->m_pkthdr.rcvif->if_index;
		else
			rip6src.sin6_scope_id = 0;
	} else
		rip6src.sin6_scope_id = 0;

	for (in6p = rawin6pcb.in6p_next;
	     in6p != &rawin6pcb; in6p = in6p->in6p_next) {
		if (in6p->in6p_ip6.ip6_nxt &&
		    in6p->in6p_ip6.ip6_nxt != proto)
			continue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr) &&
		   !IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, &ip6->ip6_dst))
			continue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr) &&
		   !IN6_ARE_ADDR_EQUAL(&in6p->in6p_faddr, &ip6->ip6_src))
			continue;
		if (in6p->in6p_cksum != -1
		 && in6_cksum(m, ip6->ip6_nxt, *offp, m->m_pkthdr.len - *offp))
		{
			/* XXX bark something */
			continue;
		}
		if (last) {
			struct	mbuf *n;
			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				if (last->in6p_flags & IN6P_CONTROLOPTS)
					ip6_savecontrol(last, &opts, ip6, n);
				/* strip intermediate headers */
				m_adj(n, *offp);
				if (sbappendaddr(&last->in6p_socket->so_rcv,
						(struct sockaddr *)&rip6src,
						 n, opts) == 0) {
					/* should notify about lost packet */
					m_freem(n);
					if (opts)
						m_freem(opts);
				} else
					sorwakeup(last->in6p_socket);
				opts = NULL;
			}
		}
		last = in6p;
	}
	if (last) {
		if (last->in6p_flags & IN6P_CONTROLOPTS)
			ip6_savecontrol(last, &opts, ip6, m);
		/* strip intermediate headers */
		m_adj(m, *offp);
		if (sbappendaddr(&last->in6p_socket->so_rcv,
				(struct sockaddr *)&rip6src, m, opts) == 0) {
			m_freem(m);
			if (opts)
				m_freem(opts);
		} else
			sorwakeup(last->in6p_socket);
	} else {
		if (proto == IPPROTO_NONE)
			m_freem(m);
		else {
			char *prvnxtp = ip6_get_prevhdr(m, *offp); /* XXX */
			icmp6_error(m, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_NEXTHEADER,
				    prvnxtp - mtod(m, char *));
		}
		ip6stat.ip6s_delivered--;
	}
	return IPPROTO_DONE;
}

void
rip6_ctlinput(cmd, sa, d)
	int cmd;
	struct sockaddr *sa;
	void *d;
{
	struct sockaddr_in6 sa6;
	register struct ip6_hdr *ip6;
	struct mbuf *m;
	int off;
	void (*notify) __P((struct in6pcb *, int)) = in6_rtchange;

	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6))
		return;

	if ((unsigned)cmd >= PRC_NCMDS)
		return;
	if (PRC_IS_REDIRECT(cmd))
		notify = in6_rtchange, d = NULL;
	else if (cmd == PRC_HOSTDEAD)
		d = NULL;
	else if (inet6ctlerrmap[cmd] == 0)
		return;

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		struct ip6ctlparam *ip6cp = (struct ip6ctlparam *)d;
		m = ip6cp->ip6c_m;
		ip6 = ip6cp->ip6c_ip6;
		off = ip6cp->ip6c_off;
	} else {
		m = NULL;
		ip6 = NULL;
	}

	/* translate addresses into internal form */
	sa6 = *(struct sockaddr_in6 *)sa;
	if (IN6_IS_ADDR_LINKLOCAL(&sa6.sin6_addr) && m && m->m_pkthdr.rcvif)
		sa6.sin6_addr.s6_addr16[1] = htons(m->m_pkthdr.rcvif->if_index);

	if (ip6) {
		/*
		 * XXX: We assume that when IPV6 is non NULL,
		 * M and OFF are valid.
		 */
		struct in6_addr s;

		/* translate addresses into internal form */
		memcpy(&s, &ip6->ip6_src, sizeof(s));
		if (IN6_IS_ADDR_LINKLOCAL(&s))
			s.s6_addr16[1] = htons(m->m_pkthdr.rcvif->if_index);

		(void) in6_pcbnotify(&rawin6pcb, (struct sockaddr *)&sa6,
					0, &s, 0, cmd, notify);
	} else {
		(void) in6_pcbnotify(&rawin6pcb, (struct sockaddr *)&sa6, 0,
					&zeroin6_addr, 0, cmd, notify);
	}
}

/*
 * Generate IPv6 header and pass packet to ip6_output.
 * Tack on options user may have setup with control call.
 */
int
#if __STDC__
rip6_output(struct mbuf *m, ...)
#else
rip6_output(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
	struct socket *so;
	struct sockaddr_in6 *dstsock;
	struct mbuf *control;
	struct in6_addr *dst;
	struct ip6_hdr *ip6;
	struct in6pcb *in6p;
	u_int	plen = m->m_pkthdr.len;
	int error = 0;
	struct ip6_pktopts opt, *optp = NULL;
	struct ifnet *oifp = NULL;
	int type, code;		/* for ICMPv6 output statistics only */
	int priv = 0;
	va_list ap;

	va_start(ap, m);
	so = va_arg(ap, struct socket *);
	dstsock = va_arg(ap, struct sockaddr_in6 *);
	control = va_arg(ap, struct mbuf *);
	va_end(ap);

	in6p = sotoin6pcb(so);

	priv = 0;
    {
	struct proc *p = curproc;	/* XXX */

	if (p && !suser(p->p_ucred, &p->p_acflag))
		priv = 1;
    }
	dst = &dstsock->sin6_addr;
	if (control) {
		if ((error = ip6_setpktoptions(control, &opt, priv)) != 0)
			goto bad;
		optp = &opt;
	} else
		optp = in6p->in6p_outputopts;

	/*
	 * For an ICMPv6 packet, we should know its type and code
	 * to update statistics.
	 */
	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6) {
		struct icmp6_hdr *icmp6;
		if (m->m_len < sizeof(struct icmp6_hdr) &&
		    (m = m_pullup(m, sizeof(struct icmp6_hdr))) == NULL) {
			error = ENOBUFS;
			goto bad;
		}
		icmp6 = mtod(m, struct icmp6_hdr *);
		type = icmp6->icmp6_type;
		code = icmp6->icmp6_code;
	}

	M_PREPEND(m, sizeof(*ip6), M_WAIT);
	ip6 = mtod(m, struct ip6_hdr *);

	/*
	 * Next header might not be ICMP6 but use its pseudo header anyway.
	 */
	ip6->ip6_dst = *dst;

	/*
	 * If the scope of the destination is link-local, embed the interface
	 * index in the address.
	 *
	 * XXX advanced-api value overrides sin6_scope_id 
	 */
	if (IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&ip6->ip6_dst)) {
		struct in6_pktinfo *pi;

		/*
		 * XXX Boundary check is assumed to be already done in
		 * ip6_setpktoptions().
		 */
		if (optp && (pi = optp->ip6po_pktinfo) && pi->ipi6_ifindex) {
			ip6->ip6_dst.s6_addr16[1] = htons(pi->ipi6_ifindex);
			oifp = ifindex2ifnet[pi->ipi6_ifindex];
		}
		else if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) &&
			 in6p->in6p_moptions &&
			 in6p->in6p_moptions->im6o_multicast_ifp) {
			oifp = in6p->in6p_moptions->im6o_multicast_ifp;
			ip6->ip6_dst.s6_addr16[1] = htons(oifp->if_index);
		} else if (dstsock->sin6_scope_id) {
			/* boundary check */
			if (dstsock->sin6_scope_id < 0 
			 || if_index < dstsock->sin6_scope_id) {
				error = ENXIO;  /* XXX EINVAL? */
				goto bad;
			}
			ip6->ip6_dst.s6_addr16[1]
				= htons(dstsock->sin6_scope_id & 0xffff);/*XXX*/
		}
	}

	/*
	 * Source address selection.
	 */
	{
		struct in6_addr *in6a;

		if ((in6a = in6_selectsrc(dstsock, optp,
					  in6p->in6p_moptions,
					  &in6p->in6p_route,
					  &in6p->in6p_laddr,
					  &error)) == 0) {
			if (error == 0)
				error = EADDRNOTAVAIL;
			goto bad;
		}
		ip6->ip6_src = *in6a;
		if (in6p->in6p_route.ro_rt)
			oifp = ifindex2ifnet[in6p->in6p_route.ro_rt->rt_ifp->if_index];
	}

	ip6->ip6_flow = in6p->in6p_flowinfo & IPV6_FLOWINFO_MASK;
	ip6->ip6_vfc  &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc  |= IPV6_VERSION;
#if 0				/* ip6_plen will be filled in ip6_output. */
	ip6->ip6_plen  = htons((u_short)plen);
#endif
	ip6->ip6_nxt   = in6p->in6p_ip6.ip6_nxt;
	ip6->ip6_hlim = in6_selecthlim(in6p, oifp);

	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6 ||
	    in6p->in6p_cksum != -1) {
		struct mbuf *n;
		int off;
		u_int16_t *p;

#define	offsetof(type, member)	((size_t)(&((type *)0)->member)) /* XXX */

		/* compute checksum */
		if (so->so_proto->pr_protocol == IPPROTO_ICMPV6)
			off = offsetof(struct icmp6_hdr, icmp6_cksum);
		else
			off = in6p->in6p_cksum;
		if (plen < off + 1) {
			error = EINVAL;
			goto bad;
		}
		off += sizeof(struct ip6_hdr);

		n = m;
		while (n && n->m_len <= off) {
			off -= n->m_len;
			n = n->m_next;
		}
		if (!n)
			goto bad;
		p = (u_int16_t *)(mtod(n, caddr_t) + off);
		*p = 0;
		*p = in6_cksum(m, ip6->ip6_nxt, sizeof(*ip6), plen);
	}

	error = ip6_output(m, optp, &in6p->in6p_route, 0, in6p->in6p_moptions,
			   &oifp);
	if (so->so_proto->pr_protocol == IPPROTO_ICMPV6) {
		if (oifp)
			icmp6_ifoutstat_inc(oifp, type, code);
		icmp6stat.icp6s_outhist[type]++;
	}

	goto freectl;

 bad:
	if (m)
		m_freem(m);

 freectl:
	if (optp == &opt && optp->ip6po_rthdr && optp->ip6po_route.ro_rt)
		RTFREE(optp->ip6po_route.ro_rt);
	if (control)
		m_freem(control);
	return(error);
}

/*
 * Raw IPv6 socket option processing.
 */
int
rip6_ctloutput(op, so, level, optname, m)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **m;
{
	int error = 0;

	switch(level) {
	 case IPPROTO_IPV6:
		 switch(optname) {
		  case MRT6_INIT:
		  case MRT6_DONE:
		  case MRT6_ADD_MIF:
		  case MRT6_DEL_MIF:
		  case MRT6_ADD_MFC:
		  case MRT6_DEL_MFC:
		  case MRT6_PIM:
			  if (op == PRCO_SETOPT) {
				  error = ip6_mrouter_set(optname, so, *m);
				  if (*m)
					  (void)m_free(*m);
			  } else if (op == PRCO_GETOPT) {
				  error = ip6_mrouter_get(optname, so, m);
			  } else
				  error = EINVAL;
			  return (error);
		 }
		 return (ip6_ctloutput(op, so, level, optname, m));
		 /* NOTREACHED */

	 case IPPROTO_ICMPV6:
		 /*
		  * XXX: is it better to call icmp6_ctloutput() directly
		  * from protosw?
		  */
		 return(icmp6_ctloutput(op, so, level, optname, m));

	 default:
		 if (op == PRCO_SETOPT && *m)
			 (void)m_free(*m);
		 return(EINVAL);
	}
}

extern	u_long rip6_sendspace;
extern	u_long rip6_recvspace;

int
rip6_usrreq(so, req, m, nam, control, p)
	register struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
	struct proc *p;
{
	register struct in6pcb *in6p = sotoin6pcb(so);
	int s;
	int error = 0;
/*	extern	struct socket *ip6_mrouter; */ /* xxx */
	int priv;

	priv = 0;
	if (p && !suser(p->p_ucred, &p->p_acflag))
		priv++;

	if (req == PRU_CONTROL)
		return (in6_control(so, (u_long)m, (caddr_t)nam,
				    (struct ifnet *)control, p));

	if (req == PRU_PURGEIF) {
		in6_purgeif((struct ifnet *)control);
		in6_pcbpurgeif(&rawin6pcb, (struct ifnet *)control);
		return (0);
	}

	switch (req) {
	case PRU_ATTACH:
		if (in6p)
			panic("rip6_attach");
		if (!priv) {
			error = EACCES;
			break;
		}
		s = splsoftnet();
		if ((error = soreserve(so, rip6_sendspace, rip6_recvspace)) ||
		    (error = in6_pcballoc(so, &rawin6pcb))) {
			splx(s);
			break;
		}
		splx(s);
		in6p = sotoin6pcb(so);
		in6p->in6p_ip6.ip6_nxt = (long)nam;
		in6p->in6p_cksum = -1;
#ifdef IPSEC
		error = ipsec_init_policy(so, &in6p->in6p_sp);
		if (error != 0) {
			in6_pcbdetach(in6p);
			break;
		}
#endif /*IPSEC*/
		
		MALLOC(in6p->in6p_icmp6filt, struct icmp6_filter *,
			sizeof(struct icmp6_filter), M_PCB, M_NOWAIT);
		if (in6p->in6p_icmp6filt == NULL) {
			in6_pcbdetach(in6p);
			error = ENOMEM;
			break;
		}
		ICMP6_FILTER_SETPASSALL(in6p->in6p_icmp6filt);
		break;

	case PRU_DISCONNECT:
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = ENOTCONN;
			break;
		}
		in6p->in6p_faddr = in6addr_any;
		so->so_state &= ~SS_ISCONNECTED;	/* XXX */
		break;

	case PRU_ABORT:
		soisdisconnected(so);
		/* Fallthrough */
	case PRU_DETACH:
		if (in6p == 0)
			panic("rip6_detach");
		if (so == ip6_mrouter)
			ip6_mrouter_done();
		/* xxx: RSVP */
		if (in6p->in6p_icmp6filt) {
			FREE(in6p->in6p_icmp6filt, M_PCB);
			in6p->in6p_icmp6filt = NULL;
		}
		in6_pcbdetach(in6p);
		break;

	case PRU_BIND:
	    {
		struct sockaddr_in6 *addr = mtod(nam, struct sockaddr_in6 *);
		struct ifaddr *ia = NULL;

		if (nam->m_len != sizeof(*addr)) {
			error = EINVAL;
			break;
		}

		/*
		 * we don't support mapped address here, it would confuse
		 * users so reject it
		 */
		if (IN6_IS_ADDR_V4MAPPED(&addr->sin6_addr)) {
			error = EADDRNOTAVAIL;
			break;
		}

		if ((ifnet.tqh_first == 0) ||
		   (addr->sin6_family != AF_INET6) ||
		   (!IN6_IS_ADDR_UNSPECIFIED(&addr->sin6_addr) &&
		    (ia = ifa_ifwithaddr((struct sockaddr *)addr)) == 0)) {
			error = EADDRNOTAVAIL;
			break;
		}
		if (ia &&
		    ((struct in6_ifaddr *)ia)->ia6_flags &
		    (IN6_IFF_ANYCAST|IN6_IFF_NOTREADY|
		     IN6_IFF_DETACHED|IN6_IFF_DEPRECATED)) {
			error = EADDRNOTAVAIL;
			break;
		}
		in6p->in6p_laddr = addr->sin6_addr;
		break;
	    }
		
	case PRU_CONNECT:
	    {
		struct sockaddr_in6 *addr = mtod(nam, struct sockaddr_in6 *);
		struct in6_addr *in6a = NULL;

		if (nam->m_len != sizeof(*addr)) {
			error = EINVAL;
			break;
		}
		if (ifnet.tqh_first == 0) {
			error = EADDRNOTAVAIL;
			break;
		}
		if (addr->sin6_family != AF_INET6) {
			error = EAFNOSUPPORT;
			break;
		}

		/* Source address selection. XXX: need pcblookup? */
		in6a = in6_selectsrc(addr, in6p->in6p_outputopts,
				     in6p->in6p_moptions,
				     &in6p->in6p_route,
				     &in6p->in6p_laddr,
				     &error);
		if (in6a == NULL) {
			if (error == 0)
				error = EADDRNOTAVAIL;
			break;
		}
		in6p->in6p_laddr = *in6a;
		in6p->in6p_faddr = addr->sin6_addr;
		soisconnected(so);
		break;
	    }

	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	/*
	 * Mark the connection as being incapable of futther input.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;
	/*
	 * Ship a packet out. The appropriate raw output
	 * routine handles any messaging necessary.
	 */
	case PRU_SEND:
	    {
		struct sockaddr_in6 tmp;
		struct sockaddr_in6 *dst;

		if (so->so_state & SS_ISCONNECTED) {
			if (nam) {
				error = EISCONN;
				break;
			}
			/* XXX */
			bzero(&tmp, sizeof(tmp));
			tmp.sin6_family = AF_INET6;
			tmp.sin6_len = sizeof(struct sockaddr_in6);
			bcopy(&in6p->in6p_faddr, &tmp.sin6_addr,
				sizeof(struct in6_addr));
			dst = &tmp;
		} else {
			if (nam == NULL) {
				error = ENOTCONN;
				break;
			}
			dst = mtod(nam, struct sockaddr_in6 *);
		}
		error = rip6_output(m, so, dst, control);
		m = NULL;
		break;
	    }

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize
		 */
		return(0);
	/*
	 * Not supported.
	 */
	case PRU_RCVOOB:
	case PRU_RCVD:
	case PRU_LISTEN:
	case PRU_ACCEPT:
	case PRU_SENDOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		in6_setsockaddr(in6p, nam);
		break;

	case PRU_PEERADDR:
		in6_setpeeraddr(in6p, nam);
		break;

	default:
		panic("rip6_usrreq");
	}
	if (m != NULL)
		m_freem(m);
	return(error);
}
