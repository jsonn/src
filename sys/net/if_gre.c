/*	$NetBSD: if_gre.c,v 1.54.6.1 2005/03/19 08:36:31 yamt Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 * Encapsulate L3 protocols into IP
 * See RFC 1701 and 1702 for more details.
 * If_gre is compatible with Cisco GRE tunnels, so you can
 * have a NetBSD box as the other end of a tunnel interface of a Cisco
 * router. See gre(4) for more details.
 * Also supported:  IP in IP encaps (proto 55) as of RFC 2004
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_gre.c,v 1.54.6.1 2005/03/19 08:36:31 yamt Exp $");

#include "opt_inet.h"
#include "opt_ns.h"
#include "bpfilter.h"

#ifdef INET
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#if __NetBSD__
#include <sys/systm.h>
#endif

#include <machine/cpu.h>

#include <net/ethertypes.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#else
#error "Huh? if_gre without inet?"
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>
#endif

#if NBPFILTER > 0
#include <sys/time.h>
#include <net/bpf.h>
#endif

#include <net/if_gre.h>

/*
 * It is not easy to calculate the right value for a GRE MTU.
 * We leave this task to the admin and use the same default that
 * other vendors use.
 */
#define GREMTU 1476

struct gre_softc_head gre_softc_list;
int ip_gre_ttl = GRE_TTL;

int	gre_clone_create __P((struct if_clone *, int));
int	gre_clone_destroy __P((struct ifnet *));

struct if_clone gre_cloner =
    IF_CLONE_INITIALIZER("gre", gre_clone_create, gre_clone_destroy);

int gre_compute_route(struct gre_softc *sc);

int
gre_clone_create(ifc, unit)
	struct if_clone *ifc;
	int unit;
{
	struct gre_softc *sc;

	sc = malloc(sizeof(struct gre_softc), M_DEVBUF, M_WAITOK);
	memset(sc, 0, sizeof(struct gre_softc));

	snprintf(sc->sc_if.if_xname, sizeof(sc->sc_if.if_xname), "%s%d",
	    ifc->ifc_name, unit);
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_type = IFT_TUNNEL;
	sc->sc_if.if_addrlen = 0;
	sc->sc_if.if_hdrlen = 24; /* IP + GRE */
	sc->sc_if.if_dlt = DLT_NULL;
	sc->sc_if.if_mtu = GREMTU;
	sc->sc_if.if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	sc->sc_if.if_output = gre_output;
	sc->sc_if.if_ioctl = gre_ioctl;
	sc->g_dst.s_addr = sc->g_src.s_addr = INADDR_ANY;
	sc->g_proto = IPPROTO_GRE;
	sc->sc_if.if_flags |= IFF_LINK0;
	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);
#if NBPFILTER > 0
	bpfattach(&sc->sc_if, DLT_NULL, sizeof(u_int32_t));
#endif
	LIST_INSERT_HEAD(&gre_softc_list, sc, sc_list);
	return (0);
}

int
gre_clone_destroy(ifp)
	struct ifnet *ifp;
{
	struct gre_softc *sc = ifp->if_softc;

	LIST_REMOVE(sc, sc_list);
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);
	free(sc, M_DEVBUF);

	return (0);
}

/*
 * The output routine. Takes a packet and encapsulates it in the protocol
 * given by sc->g_proto. See also RFC 1701 and RFC 2004
 */
int
gre_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	   struct rtentry *rt)
{
	int error = 0;
	struct gre_softc *sc = ifp->if_softc;
	struct greip *gh;
	struct ip *ip;
	u_int16_t etype = 0;
	struct mobile_h mob_h;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == 0 ||
	    sc->g_src.s_addr == INADDR_ANY || sc->g_dst.s_addr == INADDR_ANY) {
		m_freem(m);
		error = ENETDOWN;
		goto end;
	}

	gh = NULL;
	ip = NULL;

#if NBPFILTER >0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, dst->sa_family, m);
#endif

	m->m_flags &= ~(M_BCAST|M_MCAST);

	if (sc->g_proto == IPPROTO_MOBILE) {
		if (dst->sa_family == AF_INET) {
			struct mbuf *m0;
			int msiz;

			ip = mtod(m, struct ip *);

			memset(&mob_h, 0, MOB_H_SIZ_L);
			mob_h.proto = (ip->ip_p) << 8;
			mob_h.odst = ip->ip_dst.s_addr;
			ip->ip_dst.s_addr = sc->g_dst.s_addr;

			/*
			 * If the packet comes from our host, we only change
			 * the destination address in the IP header.
			 * Else we also need to save and change the source
			 */
			if (in_hosteq(ip->ip_src, sc->g_src)) {
				msiz = MOB_H_SIZ_S;
			} else {
				mob_h.proto |= MOB_H_SBIT;
				mob_h.osrc = ip->ip_src.s_addr;
				ip->ip_src.s_addr = sc->g_src.s_addr;
				msiz = MOB_H_SIZ_L;
			}
			HTONS(mob_h.proto);
			mob_h.hcrc = gre_in_cksum((u_int16_t *)&mob_h, msiz);

			if ((m->m_data - msiz) < m->m_pktdat) {
				/* need new mbuf */
				MGETHDR(m0, M_DONTWAIT, MT_HEADER);
				if (m0 == NULL) {
					IF_DROP(&ifp->if_snd);
					m_freem(m);
					error = ENOBUFS;
					goto end;
				}
				m0->m_next = m;
				m->m_data += sizeof(struct ip);
				m->m_len -= sizeof(struct ip);
				m0->m_pkthdr.len = m->m_pkthdr.len + msiz;
				m0->m_len = msiz + sizeof(struct ip);
				m0->m_data += max_linkhdr;
				memcpy(mtod(m0, caddr_t), (caddr_t)ip,
				       sizeof(struct ip));
				m = m0;
			} else {  /* we have some space left in the old one */
				m->m_data -= msiz;
				m->m_len += msiz;
				m->m_pkthdr.len += msiz;
				memmove(mtod(m, caddr_t), ip,
					sizeof(struct ip));
			}
			ip = mtod(m, struct ip *);
			memcpy((caddr_t)(ip + 1), &mob_h, (unsigned)msiz);
			ip->ip_len = htons(ntohs(ip->ip_len) + msiz);
		} else {  /* AF_INET */
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EINVAL;
			goto end;
		}
	} else if (sc->g_proto == IPPROTO_GRE) {
		switch (dst->sa_family) {
		case AF_INET:
			ip = mtod(m, struct ip *);
			etype = ETHERTYPE_IP;
			break;
#ifdef NETATALK
		case AF_APPLETALK:
			etype = ETHERTYPE_ATALK;
			break;
#endif
#ifdef NS
		case AF_NS:
			etype = ETHERTYPE_NS;
			break;
#endif
		default:
			IF_DROP(&ifp->if_snd);
			m_freem(m);
			error = EAFNOSUPPORT;
			goto end;
		}
		M_PREPEND(m, sizeof(struct greip), M_DONTWAIT);
	} else {
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		error = EINVAL;
		goto end;
	}

	if (m == NULL) {	/* impossible */
		IF_DROP(&ifp->if_snd);
		error = ENOBUFS;
		goto end;
	}

	gh = mtod(m, struct greip *);
	if (sc->g_proto == IPPROTO_GRE) {
		/* we don't have any GRE flags for now */

		memset((void *)&gh->gi_g, 0, sizeof(struct gre_h));
		gh->gi_ptype = htons(etype);
	}

	gh->gi_pr = sc->g_proto;
	if (sc->g_proto != IPPROTO_MOBILE) {
		gh->gi_src = sc->g_src;
		gh->gi_dst = sc->g_dst;
		((struct ip*)gh)->ip_hl = (sizeof(struct ip)) >> 2;
		((struct ip*)gh)->ip_ttl = ip_gre_ttl;
		((struct ip*)gh)->ip_tos = ip->ip_tos;
		gh->gi_len = htons(m->m_pkthdr.len);
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
	/* send it off */
	error = ip_output(m, NULL, &sc->route, 0,
	    (struct ip_moptions *)NULL, (struct socket *)NULL);
  end:
	if (error)
		ifp->if_oerrors++;
	return (error);
}

int
gre_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct proc *p = curproc;	/* XXX */
	struct ifreq *ifr = (struct ifreq *)data;
	struct if_laddrreq *lifr = (struct if_laddrreq *)data;
	struct gre_softc *sc = ifp->if_softc;
	int s;
	struct sockaddr_in si;
	struct sockaddr *sa = NULL;
	int error;

	error = 0;

	s = splnet();
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		break;
	case SIOCSIFDSTADDR:
		break;
	case SIOCSIFFLAGS:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			break;
		if ((ifr->ifr_flags & IFF_LINK0) != 0)
			sc->g_proto = IPPROTO_GRE;
		else
			sc->g_proto = IPPROTO_MOBILE;
		break;
	case SIOCSIFMTU:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			break;
		if (ifr->ifr_mtu < 576) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGIFMTU:
		ifr->ifr_mtu = sc->sc_if.if_mtu;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			break;
		if (ifr == 0) {
			error = EAFNOSUPPORT;
			break;
		}
		switch (ifr->ifr_addr.sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;
	case GRESPROTO:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			break;
		sc->g_proto = ifr->ifr_flags;
		switch (sc->g_proto) {
		case IPPROTO_GRE:
			ifp->if_flags |= IFF_LINK0;
			break;
		case IPPROTO_MOBILE:
			ifp->if_flags &= ~IFF_LINK0;
			break;
		default:
			error = EPROTONOSUPPORT;
			break;
		}
		break;
	case GREGPROTO:
		ifr->ifr_flags = sc->g_proto;
		break;
	case GRESADDRS:
	case GRESADDRD:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			break;
		/*
		 * set tunnel endpoints, compute a less specific route
		 * to the remote end and mark if as up
		 */
		sa = &ifr->ifr_addr;
		if (cmd == GRESADDRS)
			sc->g_src = (satosin(sa))->sin_addr;
		if (cmd == GRESADDRD)
			sc->g_dst = (satosin(sa))->sin_addr;
	recompute:
		if ((sc->g_src.s_addr != INADDR_ANY) &&
		    (sc->g_dst.s_addr != INADDR_ANY)) {
			if (sc->route.ro_rt != 0) /* free old route */
				RTFREE(sc->route.ro_rt);
			if (gre_compute_route(sc) == 0)
				ifp->if_flags |= IFF_RUNNING;
			else
				ifp->if_flags &= ~IFF_RUNNING;
		}
		break;
	case GREGADDRS:
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_src.s_addr;
		sa = sintosa(&si);
		ifr->ifr_addr = *sa;
		break;
	case GREGADDRD:
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_dst.s_addr;
		sa = sintosa(&si);
		ifr->ifr_addr = *sa;
		break;
	case SIOCSLIFPHYADDR:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			break;
		if (lifr->addr.ss_family != AF_INET ||
		    lifr->dstaddr.ss_family != AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
		if (lifr->addr.ss_len != sizeof(si) ||
		    lifr->dstaddr.ss_len != sizeof(si)) {
			error = EINVAL;
			break;
		}
		sc->g_src = (satosin((struct sockadrr *)&lifr->addr))->sin_addr;
		sc->g_dst =
		    (satosin((struct sockadrr *)&lifr->dstaddr))->sin_addr;
		goto recompute;
	case SIOCDIFPHYADDR:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			break;
		sc->g_src.s_addr = INADDR_ANY;
		sc->g_dst.s_addr = INADDR_ANY;
		break;
	case SIOCGLIFPHYADDR:
		if (sc->g_src.s_addr == INADDR_ANY ||
		    sc->g_dst.s_addr == INADDR_ANY) {
			error = EADDRNOTAVAIL;
			break;
		}
		memset(&si, 0, sizeof(si));
		si.sin_family = AF_INET;
		si.sin_len = sizeof(struct sockaddr_in);
		si.sin_addr.s_addr = sc->g_src.s_addr;
		memcpy(&lifr->addr, &si, sizeof(si));
		si.sin_addr.s_addr = sc->g_dst.s_addr;
		memcpy(&lifr->dstaddr, &si, sizeof(si));
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}

/*
 * computes a route to our destination that is not the one
 * which would be taken by ip_output(), as this one will loop back to
 * us. If the interface is p2p as  a--->b, then a routing entry exists
 * If we now send a packet to b (e.g. ping b), this will come down here
 * gets src=a, dst=b tacked on and would from ip_output() sent back to
 * if_gre.
 * Goal here is to compute a route to b that is less specific than
 * a-->b. We know that this one exists as in normal operation we have
 * at least a default route which matches.
 */
int
gre_compute_route(struct gre_softc *sc)
{
	struct route *ro;
	u_int32_t a, b, c;

	ro = &sc->route;

	memset(ro, 0, sizeof(struct route));
	((struct sockaddr_in *)&ro->ro_dst)->sin_addr = sc->g_dst;
	ro->ro_dst.sa_family = AF_INET;
	ro->ro_dst.sa_len = sizeof(ro->ro_dst);

	/*
	 * toggle last bit, so our interface is not found, but a less
	 * specific route. I'd rather like to specify a shorter mask,
	 * but this is not possible. Should work though. XXX
	 * there is a simpler way ...
	 */
	if ((sc->sc_if.if_flags & IFF_LINK1) == 0) {
		a = ntohl(sc->g_dst.s_addr);
		b = a & 0x01;
		c = a & 0xfffffffe;
		b = b ^ 0x01;
		a = b | c;
		((struct sockaddr_in *)&ro->ro_dst)->sin_addr.s_addr
		    = htonl(a);
	}

#ifdef DIAGNOSTIC
	printf("%s: searching for a route to %s", sc->sc_if.if_xname,
	    inet_ntoa(((struct sockaddr_in *)&ro->ro_dst)->sin_addr));
#endif

	rtalloc(ro);

	/*
	 * check if this returned a route at all and this route is no
	 * recursion to ourself
	 */
	if (ro->ro_rt == NULL || ro->ro_rt->rt_ifp->if_softc == sc) {
#ifdef DIAGNOSTIC
		if (ro->ro_rt == NULL)
			printf(" - no route found!\n");
		else
			printf(" - route loops back to ourself!\n");
#endif
		return EADDRNOTAVAIL;
	}

	/*
	 * now change it back - else ip_output will just drop
	 * the route and search one to this interface ...
	 */
	if ((sc->sc_if.if_flags & IFF_LINK1) == 0)
		((struct sockaddr_in *)&ro->ro_dst)->sin_addr = sc->g_dst;

#ifdef DIAGNOSTIC
	printf(", choosing %s with gateway %s", ro->ro_rt->rt_ifp->if_xname,
	    inet_ntoa(((struct sockaddr_in *)(ro->ro_rt->rt_gateway))->sin_addr));
	printf("\n");
#endif

	return 0;
}

/*
 * do a checksum of a buffer - much like in_cksum, which operates on
 * mbufs.
 */
u_int16_t
gre_in_cksum(u_int16_t *p, u_int len)
{
	u_int32_t sum = 0;
	int nwords = len >> 1;

	while (nwords-- != 0)
		sum += *p++;

	if (len & 1) {
		union {
			u_short w;
			u_char c[2];
		} u;
		u.c[0] = *(u_char *)p;
		u.c[1] = 0;
		sum += u.w;
	}

	/* end-around-carry */
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return (~sum);
}
#endif

void	greattach __P((int));

/* ARGSUSED */
void
greattach(count)
	int count;
{
#ifdef INET
	LIST_INIT(&gre_softc_list);
	if_clone_attach(&gre_cloner);
#endif
}
