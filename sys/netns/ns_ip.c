/*	$NetBSD: ns_ip.c,v 1.27.2.4 2002/10/18 02:45:31 nathanw Exp $	*/

/*
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
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
 *	@(#)ns_ip.c	8.2 (Berkeley) 1/9/95
 */

/*
 * Software interface driver for encapsulating ns in ip.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ns_ip.c,v 1.27.2.4 2002/10/18 02:45:31 nathanw Exp $");

#include "opt_ns.h"		/* options NSIP, needed by ns_if.h */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/protosw.h>

#include <machine/stdarg.h>
#include <machine/cpu.h>	/* XXX for setsoftnet().  This must die. */

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <netns/ns.h>
#include <netns/ns_if.h>
#include <netns/ns_var.h>
#include <netns/idp.h>

struct ifnet_en {
	struct ifnet ifen_ifnet;
	struct route ifen_route;
	struct in_addr ifen_src;
	struct in_addr ifen_dst;
	struct ifnet_en *ifen_next;
};

int	nsipoutput __P((struct ifnet *, struct mbuf *m, struct sockaddr *,
    struct rtentry *));
int	nsipioctl __P((struct ifnet *, u_long, caddr_t));
void	nsipstart __P((struct ifnet *));
int	nsip_route __P((struct mbuf *));
void	nsip_rtchange __P((struct in_addr *));
#define LOMTU	(1024+512);

int	nsipif_unit;			/* XXX */
struct ifnet nsipif;
struct ifnet_en *nsip_list;		/* list of all hosts and gateways or
					broadcast addrs */

struct ifnet_en *
nsipattach()
{
	struct ifnet_en *m;
	struct ifnet *ifp;

	if (nsipif.if_mtu == 0) {
		ifp = &nsipif;
		sprintf(ifp->if_xname, "nsip%d", nsipif_unit);
		ifp->if_mtu = LOMTU;
		ifp->if_ioctl = nsipioctl;
		ifp->if_output = nsipoutput;
		ifp->if_start = nsipstart;
		ifp->if_flags = IFF_POINTOPOINT;
	}

	MALLOC((m), struct ifnet_en *, sizeof(*m), M_PCB, M_NOWAIT);
	if (m == NULL) return (NULL);
	m->ifen_next = nsip_list;
	nsip_list = m;
	ifp = &m->ifen_ifnet;

	sprintf(ifp->if_xname, "nsip%d", nsipif_unit++);
	ifp->if_mtu = LOMTU;
	ifp->if_ioctl = nsipioctl;
	ifp->if_output = nsipoutput;
	ifp->if_start = nsipstart;
	ifp->if_flags = IFF_POINTOPOINT;
	if_attach(ifp);
	if_alloc_sadl(ifp);

	/*
	 * XXX Emulate the side effect of incrementing nsipif.if_unit
	 * XXX in the days before if_xname.
	 */
	bzero(nsipif.if_xname, sizeof(nsipif.if_xname));
	sprintf(nsipif.if_xname, "nsip%d", nsipif_unit);

	return (m);
}


/*
 * Process an ioctl request.
 */
/* ARGSUSED */
int
nsipioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	int error = 0;
	struct ifreq *ifr;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* fall into: */

	case SIOCSIFDSTADDR:
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	case SIOCSIFFLAGS:
		ifr = (struct ifreq *)data;
		if ((ifr->ifr_flags & IFF_UP) == 0)
			error = nsip_free(ifp);


	default:
		error = EINVAL;
	}
	return (error);
}

struct mbuf *nsip_badlen;
struct mbuf *nsip_lastin;
int nsip_hold_input;

void
#if __STDC__
idpip_input(struct mbuf *m, ...)
#else
idpip_input(va_alist)
	va_dcl
#endif
{
	struct ifnet *ifp;
	struct ip *ip;
	struct idp *idp;
	struct ifqueue *ifq = &nsintrq;
	int len, s;
	va_list ap;
#if __STDC__
	va_start(ap, m);
#else
	struct mbuf *m;

	va_start(ap);
	m = va_arg(ap, struct mbuf *);
#endif
	ifp = va_arg(ap, struct ifnet *);
	va_end(ap);

	if (nsip_hold_input) {
		if (nsip_lastin) {
			m_freem(nsip_lastin);
		}
		nsip_lastin = m_copym(m, 0, (int)M_COPYALL, M_DONTWAIT);
	}
	/*
	 * Get IP and IDP header together in first mbuf.
	 */
	nsipif.if_ipackets++;
	s = sizeof (struct ip) + sizeof (struct idp);
	if (((m->m_flags & M_EXT) || m->m_len < s) &&
	    (m = m_pullup(m, s)) == 0) {
		nsipif.if_ierrors++;
		return;
	}
	ip = mtod(m, struct ip *);
	if (ip->ip_hl > (sizeof (struct ip) >> 2)) {
		ip_stripoptions(m, (struct mbuf *)0);
		if (m->m_len < s) {
			if ((m = m_pullup(m, s)) == 0) {
				nsipif.if_ierrors++;
				return;
			}
			ip = mtod(m, struct ip *);
		}
	}

	/*
	 * Make mbuf data length reflect IDP length.
	 * If not enough data to reflect IDP length, drop.
	 */
	m->m_data += sizeof (struct ip);
	m->m_len -= sizeof (struct ip);
	m->m_pkthdr.len -= sizeof (struct ip);
	idp = mtod(m, struct idp *);
	len = ntohs(idp->idp_len);
	if (len & 1) len++;		/* Preserve Garbage Byte */
	if (ntohs(ip->ip_len) != len) {
		if (len > ntohs(ip->ip_len)) {
			nsipif.if_ierrors++;
			if (nsip_badlen) m_freem(nsip_badlen);
			nsip_badlen = m;
			return;
		}
		/* Any extra will be trimmed off by the NS routines */
	}

	/*
	 * Place interface pointer before the data
	 * for the receiving protocol.
	 */
	m->m_pkthdr.rcvif = ifp;
	/*
	 * Deliver to NS
	 */
	s = splnet();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
		splx(s);
		return;
	}
	IF_ENQUEUE(ifq, m);
	schednetisr(NETISR_NS);
	splx(s);
	return;
}

/* ARGSUSED */
int
nsipoutput(ifp, m, dst, rt)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	struct ifnet_en *ifn = (struct ifnet_en *) ifp;

	struct ip *ip;
	struct route *ro = &(ifn->ifen_route);
	int len = 0;
	struct idp *idp = mtod(m, struct idp *);
	int error;

	ifn->ifen_ifnet.if_opackets++;
	nsipif.if_opackets++;


	/*
	 * Calculate data length and make space
	 * for IP header.
	 */
	len = ntohs(idp->idp_len);
	if (len & 1) len++;		/* Preserve Garbage Byte */
	/* following clause not necessary on vax */
	if (3 & (int)m->m_data) {
		/* force longword alignment of ip hdr */
		struct mbuf *m0 = m_gethdr(M_DONTWAIT, MT_HEADER);
		if (m0 == 0) {
			m_freem(m);
			return (ENOBUFS);
		}
		MH_ALIGN(m0, sizeof (struct ip));
		m0->m_flags = m->m_flags & M_COPYFLAGS;
		m0->m_next = m;
		m0->m_len = sizeof (struct ip);
		m0->m_pkthdr.len = m0->m_len + m->m_len;
		m->m_flags &= ~M_PKTHDR;
	} else {
		M_PREPEND(m, sizeof (struct ip), M_DONTWAIT);
		if (m == 0)
			return (ENOBUFS);
	}
	/*
	 * Fill in IP header.
	 */
	ip = mtod(m, struct ip *);
	*(int32_t *)ip = 0;
	ip->ip_p = IPPROTO_IDP;
	ip->ip_src = ifn->ifen_src;
	ip->ip_dst = ifn->ifen_dst;
	if (len + sizeof (struct ip) > IP_MAXPACKET) {
		m_freem(m);
		return (EMSGSIZE);
	}
	ip->ip_len = htons(len + sizeof (struct ip));
	ip->ip_ttl = MAXTTL;

	/*
	 * Output final datagram.
	 */
	error = ip_output(m, (struct mbuf *)0, ro, SO_BROADCAST, NULL);
	if (error) {
		ifn->ifen_ifnet.if_oerrors++;
		ifn->ifen_ifnet.if_ierrors = error;
	}
	return (error);
}

void
nsipstart(ifp)
	struct ifnet *ifp;
{
	panic("nsip_start called");
}

struct ifreq ifr = {"nsip0"};		/* XXX */

int
nsip_route(m)
	struct mbuf *m;
{
	struct nsip_req *rq = mtod(m, struct nsip_req *);
	struct sockaddr_ns *ns_dst = satosns(&rq->rq_ns);
	struct sockaddr_in *ip_dst = satosin(&rq->rq_ip);
	struct route ro;
	struct ifnet_en *ifn;
	struct sockaddr_in *src;

	/*
	 * First, make sure we already have an ns address:
	 */
	if (ns_hosteqnh(ns_thishost, ns_zerohost))
		return (EADDRNOTAVAIL);
	/*
	 * Now, determine if we can get to the destination
	 */
	bzero((caddr_t)&ro, sizeof (ro));
	ro.ro_dst = *sintosa(ip_dst);
	rtalloc(&ro);
	if (ro.ro_rt == 0 || ro.ro_rt->rt_ifp == 0) {
		return (ENETUNREACH);
	}

	/*
	 * And see how he's going to get back to us:
	 * i.e., what return ip address do we use?
	 */
	{
		struct in_ifaddr *ia;
		struct ifnet *ifp = ro.ro_rt->rt_ifp;

		for (ia = in_ifaddr.tqh_first; ia != 0;
		    ia = ia->ia_list.tqe_next)
			if (ia->ia_ifp == ifp)
				break;
		if (ia == 0)
			ia = in_ifaddr.tqh_first;
		if (ia == 0) {
			RTFREE(ro.ro_rt);
			return (EADDRNOTAVAIL);
		}
		src = satosin(&ia->ia_addr);
	}

	/*
	 * Is there a free (pseudo-)interface or space?
	 */
	for (ifn = nsip_list; ifn; ifn = ifn->ifen_next) {
		if ((ifn->ifen_ifnet.if_flags & IFF_UP) == 0)
			break;
	}
	if (ifn == NULL)
		ifn = nsipattach();
	if (ifn == NULL) {
		RTFREE(ro.ro_rt);
		return (ENOBUFS);
	}
	ifn->ifen_route = ro;
	ifn->ifen_dst =  ip_dst->sin_addr;
	ifn->ifen_src = src->sin_addr;

	/*
	 * now configure this as a point to point link
	 */
	bzero(ifr.ifr_name, sizeof(ifr.ifr_name));
	sprintf(ifr.ifr_name, "nsip%d", nsipif_unit - 1);
	ifr.ifr_dstaddr = *snstosa(ns_dst);
	(void)ns_control((struct socket *)0, SIOCSIFDSTADDR, (caddr_t)&ifr,
	    (struct ifnet *)ifn, NULL);
	satons_addr(ifr.ifr_addr).x_host = ns_thishost;
	return (ns_control((struct socket *)0, SIOCSIFADDR, (caddr_t)&ifr,
	    (struct ifnet *)ifn, NULL));
}

int
nsip_free(ifp)
	struct ifnet *ifp;
{
	struct ifnet_en *ifn = (struct ifnet_en *)ifp;
	struct route *ro = & ifn->ifen_route;

	if (ro->ro_rt) {
		RTFREE(ro->ro_rt);
		ro->ro_rt = 0;
	}
	ifp->if_flags &= ~IFF_UP;
	return (0);
}

void *
nsip_ctlinput(cmd, sa, v)
	int cmd;
	struct sockaddr *sa;
	void *v;
{
	struct sockaddr_in *sin;

	if ((unsigned)cmd >= PRC_NCMDS)
		return NULL;
	if (sa->sa_family != AF_INET && sa->sa_family != AF_IMPLINK)
		return NULL;
	sin = satosin(sa);
	if (sin->sin_addr.s_addr == INADDR_ANY)
		return NULL;

	switch (cmd) {

	case PRC_ROUTEDEAD:
	case PRC_REDIRECT_NET:
	case PRC_REDIRECT_HOST:
	case PRC_REDIRECT_TOSNET:
	case PRC_REDIRECT_TOSHOST:
		nsip_rtchange(&sin->sin_addr);
		break;
	}
	return NULL;
}

void
nsip_rtchange(dst)
	struct in_addr *dst;
{
	struct ifnet_en *ifn;

	for (ifn = nsip_list; ifn; ifn = ifn->ifen_next) {
		if (ifn->ifen_dst.s_addr == dst->s_addr &&
			ifn->ifen_route.ro_rt) {
				RTFREE(ifn->ifen_route.ro_rt);
				ifn->ifen_route.ro_rt = 0;
		}
	}
}
