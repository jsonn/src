/*	$NetBSD: in6_gif.c,v 1.7.2.1 2000/11/20 18:10:47 bouyer Exp $	*/
/*	$KAME: in6_gif.c,v 1.34 2000/04/19 04:51:58 itojun Exp $	*/

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
 * in6_gif.c
 */

#if (defined(__FreeBSD__) && __FreeBSD__ >= 3) || defined(__NetBSD__)
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#if !(defined(__FreeBSD__) && __FreeBSD__ >= 3)
#include <sys/ioctl.h>
#endif

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/malloc.h>
#endif

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifdef INET
#include <netinet/ip.h>
#endif
#include <netinet/ip_encap.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_gif.h>
#include <netinet6/in6_var.h>
#endif
#include <netinet/ip_ecn.h>

#include <net/if_gif.h>

#include <net/net_osdep.h>

int
in6_gif_output(ifp, family, m, rt)
	struct ifnet *ifp;
	int family; /* family of the packet to be encapsulate. */
	struct mbuf *m;
	struct rtentry *rt;
{
	struct gif_softc *sc = (struct gif_softc*)ifp;
	struct sockaddr_in6 *dst = (struct sockaddr_in6 *)&sc->gif_ro6.ro_dst;
	struct sockaddr_in6 *sin6_src = (struct sockaddr_in6 *)sc->gif_psrc;
	struct sockaddr_in6 *sin6_dst = (struct sockaddr_in6 *)sc->gif_pdst;
	struct ip6_hdr *ip6;
	int proto;
	u_int8_t itos, otos;

	if (sin6_src == NULL || sin6_dst == NULL ||
	    sin6_src->sin6_family != AF_INET6 ||
	    sin6_dst->sin6_family != AF_INET6) {
		m_freem(m);
		return EAFNOSUPPORT;
	}

	switch (family) {
#ifdef INET
	case AF_INET:
	    {
		struct ip *ip;

		proto = IPPROTO_IPV4;
		if (m->m_len < sizeof(*ip)) {
			m = m_pullup(m, sizeof(*ip));
			if (!m)
				return ENOBUFS;
		}
		ip = mtod(m, struct ip *);
		itos = ip->ip_tos;
		break;
	    }
#endif
#ifdef INET6
	case AF_INET6:
	    {
		struct ip6_hdr *ip6;
		proto = IPPROTO_IPV6;
		if (m->m_len < sizeof(*ip6)) {
			m = m_pullup(m, sizeof(*ip6));
			if (!m)
				return ENOBUFS;
		}
		ip6 = mtod(m, struct ip6_hdr *);
		itos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		break;
	    }
#endif
	default:
#ifdef DEBUG
		printf("in6_gif_output: warning: unknown family %d passed\n",
			family);
#endif
		m_freem(m);
		return EAFNOSUPPORT;
	}
	
	/* prepend new IP header */
	M_PREPEND(m, sizeof(struct ip6_hdr), M_DONTWAIT);
	if (m && m->m_len < sizeof(struct ip6_hdr))
		m = m_pullup(m, sizeof(struct ip6_hdr));
	if (m == NULL) {
		printf("ENOBUFS in in6_gif_output %d\n", __LINE__);
		return ENOBUFS;
	}

	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow	= 0;
	ip6->ip6_vfc	&= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc	|= IPV6_VERSION;
	ip6->ip6_plen	= htons((u_short)m->m_pkthdr.len);
	ip6->ip6_nxt	= proto;
	ip6->ip6_hlim	= ip6_gif_hlim;
	ip6->ip6_src	= sin6_src->sin6_addr;
	if (ifp->if_flags & IFF_LINK0) {
		/* multi-destination mode */
		if (!IN6_IS_ADDR_UNSPECIFIED(&sin6_dst->sin6_addr))
			ip6->ip6_dst = sin6_dst->sin6_addr;
		else if (rt) {
			if (family != AF_INET6) {
				m_freem(m);
				return EINVAL;	/*XXX*/
			}
			ip6->ip6_dst = ((struct sockaddr_in6 *)(rt->rt_gateway))->sin6_addr;
		} else {
			m_freem(m);
			return ENETUNREACH;
		}
	} else {
		/* bidirectional configured tunnel mode */
		if (!IN6_IS_ADDR_UNSPECIFIED(&sin6_dst->sin6_addr))
			ip6->ip6_dst = sin6_dst->sin6_addr;
		else  {
			m_freem(m);
			return ENETUNREACH;
		}
	}
	if (ifp->if_flags & IFF_LINK1) {
		otos = 0;
		ip_ecn_ingress(ECN_ALLOWED, &otos, &itos);
		ip6->ip6_flow |= htonl((u_int32_t)otos << 20);
	}

	if (dst->sin6_family != sin6_dst->sin6_family ||
	     !IN6_ARE_ADDR_EQUAL(&dst->sin6_addr, &sin6_dst->sin6_addr)) {
		/* cache route doesn't match */
		bzero(dst, sizeof(*dst));
		dst->sin6_family = sin6_dst->sin6_family;
		dst->sin6_len = sizeof(struct sockaddr_in6);
		dst->sin6_addr = sin6_dst->sin6_addr;
		if (sc->gif_ro6.ro_rt) {
			RTFREE(sc->gif_ro6.ro_rt);
			sc->gif_ro6.ro_rt = NULL;
		}
#if 0
		sc->gif_if.if_mtu = GIF_MTU;
#endif
	}

	if (sc->gif_ro6.ro_rt == NULL) {
		rtalloc((struct route *)&sc->gif_ro6);
		if (sc->gif_ro6.ro_rt == NULL) {
			m_freem(m);
			return ENETUNREACH;
		}

		/* if it constitutes infinite encapsulation, punt. */
		if (sc->gif_ro.ro_rt->rt_ifp == ifp) {
			m_freem(m);
			return ENETUNREACH;	/*XXX*/
		}
#if 0
		ifp->if_mtu = sc->gif_ro6.ro_rt->rt_ifp->if_mtu
			- sizeof(struct ip6_hdr);
#endif
	}
	
#ifdef IPV6_MINMTU
	/*
	 * force fragmentation to minimum MTU, to avoid path MTU discovery.
	 * it is too painful to ask for resend of inner packet, to achieve
	 * path MTU discovery for encapsulated packets.
	 */
	return(ip6_output(m, 0, &sc->gif_ro6, IPV6_MINMTU, 0, NULL));
#else
	return(ip6_output(m, 0, &sc->gif_ro6, 0, 0, NULL));
#endif
}

int in6_gif_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct mbuf *m = *mp;
	struct ifnet *gifp = NULL;
	struct ip6_hdr *ip6;
	int af = 0;
	u_int32_t otos;

	ip6 = mtod(m, struct ip6_hdr *);

	gifp = (struct ifnet *)encap_getarg(m);

	if (gifp == NULL || (gifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		ip6stat.ip6s_nogif++;
		return IPPROTO_DONE;
	}

	otos = ip6->ip6_flow;
	m_adj(m, *offp);

	switch (proto) {
#ifdef INET
	case IPPROTO_IPV4:
	    {
		struct ip *ip;
		u_int8_t otos8;
		af = AF_INET;
		otos8 = (ntohl(otos) >> 20) & 0xff;
		if (m->m_len < sizeof(*ip)) {
			m = m_pullup(m, sizeof(*ip));
			if (!m)
				return IPPROTO_DONE;
		}
		ip = mtod(m, struct ip *);
		if (gifp->if_flags & IFF_LINK1)
			ip_ecn_egress(ECN_ALLOWED, &otos8, &ip->ip_tos);
		break;
	    }
#endif /* INET */
#ifdef INET6
	case IPPROTO_IPV6:
	    {
		struct ip6_hdr *ip6;
		af = AF_INET6;
		if (m->m_len < sizeof(*ip6)) {
			m = m_pullup(m, sizeof(*ip6));
			if (!m)
				return IPPROTO_DONE;
		}
		ip6 = mtod(m, struct ip6_hdr *);
		if (gifp->if_flags & IFF_LINK1)
			ip6_ecn_egress(ECN_ALLOWED, &otos, &ip6->ip6_flow);
		break;
	    }
#endif
	default:
		ip6stat.ip6s_nogif++;
		m_freem(m);
		return IPPROTO_DONE;
	}
		
	gif_input(m, af, gifp);
	return IPPROTO_DONE;
}

/*
 * we know that we are in IFF_UP, outer address available, and outer family
 * matched the physical addr family.  see gif_encapcheck().
 */
int
gif_encapcheck6(m, off, proto, arg)
	const struct mbuf *m;
	int off;
	int proto;
	void *arg;
{
	struct ip6_hdr ip6;
	struct gif_softc *sc;
	struct sockaddr_in6 *src, *dst;
	int addrmatch;

	/* sanity check done in caller */
	sc = (struct gif_softc *)arg;
	src = (struct sockaddr_in6 *)sc->gif_psrc;
	dst = (struct sockaddr_in6 *)sc->gif_pdst;

	/* LINTED const cast */
	m_copydata((struct mbuf *)m, 0, sizeof(ip6), (caddr_t)&ip6);

	/* check for address match */
	addrmatch = 0;
	if (IN6_ARE_ADDR_EQUAL(&src->sin6_addr, &ip6.ip6_dst))
		addrmatch |= 1;
	if (IN6_ARE_ADDR_EQUAL(&dst->sin6_addr, &ip6.ip6_src))
		addrmatch |= 2;
	else if ((sc->gif_if.if_flags & IFF_LINK0) != 0 &&
		 IN6_IS_ADDR_UNSPECIFIED(&dst->sin6_addr)) {
		addrmatch |= 2; /* we accept any source */
	}
	if (addrmatch != 3)
		return 0;

	/* martian filters on outer source - done in ip6_input */

	/* ingress filters on outer source */
	if ((m->m_flags & M_PKTHDR) != 0 && m->m_pkthdr.rcvif) {
		struct sockaddr_in6 sin6;
		struct rtentry *rt;

		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_addr = ip6.ip6_src;
		/* XXX scopeid */
#ifdef __FreeBSD__
		rt = rtalloc1((struct sockaddr *)&sin6, 0, 0UL);
#else
		rt = rtalloc1((struct sockaddr *)&sin6, 0);
#endif
		if (!rt)
			return 0;
		if (rt->rt_ifp != m->m_pkthdr.rcvif) {
			rtfree(rt);
			return 0;
		}
		rtfree(rt);
	}

	/* prioritize: IFF_LINK0 mode is less preferred */
	return (sc->gif_if.if_flags & IFF_LINK0) ? 128 : 128 * 2;
}
