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

#if (defined(__FreeBSD__) && __FreeBSD__ >= 3) || defined(__NetBSD__)
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#if !defined(__FreeBSD__) || __FreeBSD__ < 3
#include <sys/ioctl.h>
#endif 
#include <sys/syslog.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/icmp6.h>

#define SDL(s) ((struct sockaddr_dl *)s)

#if 0
extern	struct timeval time;
#endif

struct dadq;
static struct dadq *nd6_dad_find __P((struct ifaddr *));
static void nd6_dad_timer __P((struct ifaddr *));
static void nd6_dad_ns_input __P((struct ifaddr *));
static void nd6_dad_na_input __P((struct ifaddr *));

/* ignore NS in DAD - specwise incorrect, */
int dad_ignore_ns = 0;

/*
 * Input an Neighbor Solicitation Message.
 *
 * Based on RFC 2461
 * Based on RFC 2462 (duplicated address detection)
 *
 * XXX proxy advertisement
 */
void
nd6_ns_input(m, off, icmp6len)
	struct mbuf *m;
	int off, icmp6len;
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_neighbor_solicit *nd_ns
		= (struct nd_neighbor_solicit *)((caddr_t)ip6 + off);
	struct in6_addr saddr6 = ip6->ip6_src;
	struct in6_addr daddr6 = ip6->ip6_dst;
	struct in6_addr taddr6 = nd_ns->nd_ns_target;
	struct in6_addr myaddr6;
	char *lladdr = NULL;
	struct ifaddr *ifa;
	int lladdrlen = 0;
	int anycast = 0, proxy = 0, tentative = 0;
	int tlladdr;
	union nd_opts ndopts;

	if (ip6->ip6_hlim != 255) {
		log(LOG_ERR,
		    "nd6_ns_input: invalid hlim %d\n", ip6->ip6_hlim);
		return;
	}

	if (IN6_IS_ADDR_UNSPECIFIED(&saddr6)) {
		/* dst has to be solicited node multicast address. */
		if (daddr6.s6_addr16[0] == IPV6_ADDR_INT16_MLL
		    /*don't check ifindex portion*/
		    && daddr6.s6_addr32[1] == 0
		    && daddr6.s6_addr32[2] == IPV6_ADDR_INT32_ONE
		    && daddr6.s6_addr8[12] == 0xff) {
			; /*good*/
		} else {
			log(LOG_INFO, "nd6_ns_input: bad DAD packet "
				"(wrong ip6 dst)\n");
			goto bad;
		}
	}

	if (IN6_IS_ADDR_MULTICAST(&taddr6)) {
		log(LOG_INFO, "nd6_ns_input: bad NS target (multicast)\n");
		goto bad;
	}

	if (IN6_IS_SCOPE_LINKLOCAL(&taddr6))
		taddr6.s6_addr16[1] = htons(ifp->if_index);

	icmp6len -= sizeof(*nd_ns);
	nd6_option_init(nd_ns + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		log(LOG_INFO, "nd6_ns_input: invalid ND option, ignored\n");
		goto bad;
	}

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_src_lladdr +1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
	}
	
	if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src) && lladdr) {
		log(LOG_INFO, "nd6_ns_input: bad DAD packet "
			"(link-layer address option)\n");
		goto bad;
	}

	/*
	 * Attaching target link-layer address to the NA?
	 * (RFC 2461 7.2.4)
	 *
	 * NS IP dst is unicast/anycast			MUST NOT add
	 * NS IP dst is solicited-node multicast	MUST add
	 *
	 * In implementation, we add target link-layer address by default.
	 * We do not add one in MUST NOT cases.
	 */
#if 0 /* too much! */
	ifa = (struct ifaddr *)in6ifa_ifpwithaddr(ifp, &daddr6);
	if (ifa && (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_ANYCAST))
		tlladdr = 0;
	else
#endif
	if (!IN6_IS_ADDR_MULTICAST(&daddr6))
		tlladdr = 0;
	else
		tlladdr = 1;

	/*
	 * Target address (taddr6) must be either:
	 * (1) Valid unicast/anycast address for my receiving interface,
	 * (2) Unicast address for which I'm offering proxy service, or
	 * (3) "tentative" address on which DAD is being performed.
	 */
	/* (1) and (3) check. */
	ifa = (struct ifaddr *)in6ifa_ifpwithaddr(ifp, &taddr6);

	/* (2) check. */
	if (!ifa && nd6_proxyall) {
		struct rtentry *rt;
		struct sockaddr_in6 tsin6;

		bzero(&tsin6, sizeof tsin6);		
		tsin6.sin6_len = sizeof(struct sockaddr_in6);
		tsin6.sin6_family = AF_INET6;
		tsin6.sin6_addr = taddr6;

		rt = rtalloc1((struct sockaddr *)&tsin6, 0
#ifdef __FreeBSD__
			      , 0
#endif /* __FreeBSD__ */
			      );
		if (rt && rt->rt_ifp != ifp) {
			/*
			 * search link local addr for ifp, and use it for
			 * proxy NA.
			 */
			ifa = (struct ifaddr *)in6ifa_ifpforlinklocal(ifp);
			if (ifa)
				proxy = 1;
		}
		rtfree(rt);
	}
	if (!ifa) {
		/*
		 * We've got a NS packet, and we don't have that adddress
		 * assigned for us.  We MUST silently ignore it.
		 * See RFC2461 7.2.3.
		 */
		return;
	}
	myaddr6 = IFA_IN6(ifa);
	anycast = ((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_ANYCAST;
	tentative = ((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_TENTATIVE;
	if (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_DUPLICATED)
		return;

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		log(LOG_INFO,
		    "nd6_ns_input: lladdrlen mismatch for %s "
		    "(if %d, NS packet %d)\n",
			ip6_sprintf(&taddr6), ifp->if_addrlen, lladdrlen - 2);
	}

	if (IN6_ARE_ADDR_EQUAL(&myaddr6, &saddr6)) {
		log(LOG_INFO,
		    "nd6_ns_input: duplicate IP6 address %s\n",
		    ip6_sprintf(&saddr6));
		return;
	}

	/*
	 * We have neighbor solicitation packet, with target address equals to
	 * one of my tentative address.
	 *
	 * src addr	how to process?
	 * ---		---
	 * multicast	of course, invalid (rejected in ip6_input)
	 * unicast	somebody is doing address resolution -> ignore
	 * unspec	dup address detection
	 *
	 * The processing is defined in RFC 2462.
	 */
	if (tentative) {
		/*
		 * If source address is unspecified address, it is for
		 * duplicated address detection.
		 *
		 * If not, the packet is for addess resolution;
		 * silently ignore it.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&saddr6))
			nd6_dad_ns_input(ifa);

		return;
	}

	/*
	 * If the source address is unspecified address, entries must not
	 * be created or updated.
	 * It looks that sender is performing DAD.  Output NA toward
	 * all-node multicast address, to tell the sender that I'm using
	 * the address.
	 * S bit ("solicited") must be zero.
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&saddr6)) {
		saddr6 = in6addr_linklocal_allnodes;
		saddr6.s6_addr16[1] = htons(ifp->if_index);
		nd6_na_output(ifp, &saddr6, &taddr6,
			      ((anycast || proxy || !tlladdr)
				      ? 0 : ND_NA_FLAG_OVERRIDE)
			      	| (ip6_forwarding ? ND_NA_FLAG_ROUTER : 0),
			      tlladdr);
		return;
	}

	nd6_cache_lladdr(ifp, &saddr6, lladdr, lladdrlen, ND_NEIGHBOR_SOLICIT);

	nd6_na_output(ifp, &saddr6, &taddr6,
		      ((anycast || proxy || !tlladdr) ? 0 : ND_NA_FLAG_OVERRIDE)
			| (ip6_forwarding ? ND_NA_FLAG_ROUTER : 0)
			| ND_NA_FLAG_SOLICITED,
		      tlladdr);
	return;

 bad:
	log(LOG_ERR, "nd6_ns_input: src=%s\n", ip6_sprintf(&saddr6));
	log(LOG_ERR, "nd6_ns_input: dst=%s\n", ip6_sprintf(&daddr6));
	log(LOG_ERR, "nd6_ns_input: tgt=%s\n", ip6_sprintf(&taddr6));
	return;
}

/*
 * Output an Neighbor Solicitation Message. Caller specifies:
 *	- ICMP6 header source IP6 address
 *	- ND6 header target IP6 address
 *	- ND6 header source datalink address
 *
 * Based on RFC 2461
 * Based on RFC 2462 (duplicated address detection)
 */
void
nd6_ns_output(ifp, daddr6, taddr6, ln, dad)
	struct ifnet *ifp;
	struct in6_addr *daddr6, *taddr6;
	struct llinfo_nd6 *ln;	/* for source address determination */
	int dad;	/* duplicated address detection */
{
	struct mbuf *m;
	struct ip6_hdr *ip6;
	struct nd_neighbor_solicit *nd_ns;
	struct in6_ifaddr *ia = NULL;
	struct ip6_moptions im6o;
	int icmp6len;
	caddr_t mac;
	
	if (IN6_IS_ADDR_MULTICAST(taddr6))
		return;

	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;

	if (daddr6 == NULL || IN6_IS_ADDR_MULTICAST(daddr6)) {
		m->m_flags |= M_MCAST;
		im6o.im6o_multicast_ifp = ifp;
		im6o.im6o_multicast_hlim = 255;
		im6o.im6o_multicast_loop = 0;
	}

	icmp6len = sizeof(*nd_ns);
	m->m_pkthdr.len = m->m_len = sizeof(*ip6) + icmp6len;
	MH_ALIGN(m, m->m_len + 16); /* 1+1+6 is enought. but just in case */

	/* fill neighbor solicitation packet */
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow = 0;
	ip6->ip6_vfc = IPV6_VERSION;
	/* ip6->ip6_plen will be set later */
	ip6->ip6_nxt = IPPROTO_ICMPV6;
	ip6->ip6_hlim = 255;
	if (daddr6)
		ip6->ip6_dst = *daddr6;
	else {
		ip6->ip6_dst.s6_addr16[0] = IPV6_ADDR_INT16_MLL;
		ip6->ip6_dst.s6_addr16[1] = htons(ifp->if_index);
		ip6->ip6_dst.s6_addr32[1] = 0;
		ip6->ip6_dst.s6_addr32[2] = IPV6_ADDR_INT32_ONE;
		ip6->ip6_dst.s6_addr32[3] = taddr6->s6_addr32[3];
		ip6->ip6_dst.s6_addr8[12] = 0xff;
	}
	if (!dad) {
#if 0	/* KAME way, exact address scope match */
		/*
		 * Select a source whose scope is the same as that of the dest.
		 * Typically, the dest is link-local solicitation multicast
		 * (i.e. neighbor discovery) or link-local/global unicast
		 * (i.e. neighbor un-reachability detection).
		 */
		ia = in6_ifawithifp(ifp, &ip6->ip6_dst);
		if (ia == NULL) {
			m_freem(m);
			return;
		}
		ip6->ip6_src = ia->ia_addr.sin6_addr;
#else	/* spec-wise correct */
		/*
		 * RFC2461 7.2.2:
		 * "If the source address of the packet prompting the
		 * solicitation is the same as one of the addresses assigned
		 * to the outgoing interface, that address SHOULD be placed
		 * in the IP Source Address of the outgoing solicitation.
		 * Otherwise, any one of the addresses assigned to the
		 * interface should be used."
		 *
		 * We use the source address for the prompting packet
		 * (saddr6), if:
		 * - saddr6 is given from the caller (by giving "ln"), and
		 * - saddr6 belongs to the outgoing interface.
		 * Otherwise, we perform a scope-wise match.
		 */
		struct ip6_hdr *hip6;		/*hold ip6*/
		struct in6_addr *saddr6;

		if (ln && ln->ln_hold) {
			hip6 = mtod(ln->ln_hold, struct ip6_hdr *);
			/* XXX pullup? */
			if (sizeof(*hip6) < ln->ln_hold->m_len)
				saddr6 = &hip6->ip6_src;
			else
				saddr6 = NULL;
		} else
			saddr6 = NULL;
		if (saddr6 && in6ifa_ifpwithaddr(ifp, saddr6))
			bcopy(saddr6, &ip6->ip6_src, sizeof(*saddr6));
		else {
			ia = in6_ifawithifp(ifp, &ip6->ip6_dst);
			if (ia == NULL) {
				m_freem(m);	/*XXX*/
				return;
			}
			ip6->ip6_src = ia->ia_addr.sin6_addr;
		}
#endif
	} else {
		/*
		 * Source address for DAD packet must always be IPv6
		 * unspecified address. (0::0)
		 */
		bzero(&ip6->ip6_src, sizeof(ip6->ip6_src));
	}
	nd_ns = (struct nd_neighbor_solicit *)(ip6 + 1);
	nd_ns->nd_ns_type = ND_NEIGHBOR_SOLICIT;
	nd_ns->nd_ns_code = 0;
	nd_ns->nd_ns_reserved = 0;
	nd_ns->nd_ns_target = *taddr6;

	if (IN6_IS_SCOPE_LINKLOCAL(&nd_ns->nd_ns_target))
		nd_ns->nd_ns_target.s6_addr16[1] = 0;

	/*
	 * Add source link-layer address option.
	 *
	 *				spec		implementation
	 *				---		---
	 * DAD packet			MUST NOT	do not add the option
	 * there's no link layer address:
	 *				impossible	do not add the option
	 * there's link layer address:
	 *	Multicast NS		MUST add one	add the option
	 *	Unicast NS		SHOULD add one	add the option
	 */
	if (!dad && (mac = nd6_ifptomac(ifp))) {
		int optlen = sizeof(struct nd_opt_hdr) + ifp->if_addrlen;
		struct nd_opt_hdr *nd_opt = (struct nd_opt_hdr *)(nd_ns + 1);
		
		m->m_pkthdr.len += optlen;
		m->m_len += optlen;
		icmp6len += optlen;
		nd_opt->nd_opt_type = ND_OPT_SOURCE_LINKADDR;
		/* xxx 8 byte alignments? */
		nd_opt->nd_opt_len = optlen >> 3;
		bcopy(mac, (caddr_t)(nd_opt + 1), ifp->if_addrlen);
	}

	ip6->ip6_plen = htons((u_short)icmp6len);
	nd_ns->nd_ns_cksum = 0;
	nd_ns->nd_ns_cksum
		= in6_cksum(m, IPPROTO_ICMPV6, sizeof(*ip6), icmp6len);

#ifdef IPSEC
	m->m_pkthdr.rcvif = NULL;
#endif /*IPSEC*/
	ip6_output(m, NULL, NULL, dad ? IPV6_DADOUTPUT : 0, &im6o);
	icmp6stat.icp6s_outhist[ND_NEIGHBOR_SOLICIT]++;
}

/*
 * Neighbor advertisement input handling.
 *
 * Based on RFC 2461
 * Based on RFC 2462 (duplicated address detection)
 */
void
nd6_na_input(m, off, icmp6len)
	struct mbuf *m;
	int off, icmp6len;
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_neighbor_advert *nd_na
		= (struct nd_neighbor_advert *)((caddr_t)ip6 + off);
#if 0
	struct in6_addr saddr6 = ip6->ip6_src;
#endif
	struct in6_addr daddr6 = ip6->ip6_dst;
	struct in6_addr taddr6 = nd_na->nd_na_target;
	int flags = nd_na->nd_na_flags_reserved;
	int is_router = ((flags & ND_NA_FLAG_ROUTER) != 0);
	int is_solicited = ((flags & ND_NA_FLAG_SOLICITED) != 0);
	int is_override = ((flags & ND_NA_FLAG_OVERRIDE) != 0);
	char *lladdr = NULL;
	int lladdrlen = 0;
	struct ifaddr *ifa;
	struct llinfo_nd6 *ln;
	struct rtentry *rt;
	struct sockaddr_dl *sdl;
	union nd_opts ndopts;

	if (ip6->ip6_hlim != 255) {
		log(LOG_ERR,
		    "nd6_na_input: invalid hlim %d\n", ip6->ip6_hlim);
		return;
	}

	if (IN6_IS_SCOPE_LINKLOCAL(&taddr6))
		taddr6.s6_addr16[1] = htons(ifp->if_index);

	if (IN6_IS_ADDR_MULTICAST(&taddr6)) {
		log(LOG_ERR,
		    "nd6_na_input: invalid target address %s\n",
		    ip6_sprintf(&taddr6));
		return;
	}
	if (IN6_IS_ADDR_MULTICAST(&daddr6))
		if (is_solicited) {
			log(LOG_ERR,
			    "nd6_na_input: a solicited adv is multicasted\n");
			return;
		}

	icmp6len -= sizeof(*nd_na);
	nd6_option_init(nd_na + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		log(LOG_INFO, "nd6_na_input: invalid ND option, ignored\n");
		return;
	}

	if (ndopts.nd_opts_tgt_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_tgt_lladdr + 1);
		lladdrlen = ndopts.nd_opts_tgt_lladdr->nd_opt_len << 3;
	}

	ifa = (struct ifaddr *)in6ifa_ifpwithaddr(ifp, &taddr6);

	/*
	 * Target address matches one of my interface address.
	 *
	 * If my address is tentative, this means that there's somebody
	 * already using the same address as mine.  This indicates DAD failure.
	 * This is defined in RFC 2462.
	 *
	 * Otherwise, process as defined in RFC 2461.
	 */
	if (ifa
	 && (((struct in6_ifaddr *)ifa)->ia6_flags & IN6_IFF_TENTATIVE)) {
		nd6_dad_na_input(ifa);
		return;
	}

	/* Just for safety, maybe unnecessery. */
	if (ifa) {
		log(LOG_ERR,
		    "nd6_na_input: duplicate IP6 address %s\n",
		    ip6_sprintf(&taddr6));
		return;
	}

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		log(LOG_INFO,
		    "nd6_na_input: lladdrlen mismatch for %s "
		    "(if %d, NA packet %d)\n",
			ip6_sprintf(&taddr6), ifp->if_addrlen, lladdrlen - 2);
	}

	/*
	 * If no neighbor cache entry is found, NA SHOULD silently be discarded.
	 */
	rt = nd6_lookup(&taddr6, 0, ifp);
	if ((rt == NULL) ||
	   ((ln = (struct llinfo_nd6 *)rt->rt_llinfo) == NULL) ||
	   ((sdl = SDL(rt->rt_gateway)) == NULL))
		return;

	if (ln->ln_state == ND6_LLINFO_INCOMPLETE) {
		/*
		 * If the link-layer has address, and no lladdr option came,
		 * discard the packet.
		 */
		if (ifp->if_addrlen && !lladdr)
			return;

		/*
		 * Record link-layer address, and update the state.
		 */
		sdl->sdl_alen = ifp->if_addrlen;
		bcopy(lladdr, LLADDR(sdl), ifp->if_addrlen);
		if (is_solicited) {
			ln->ln_state = ND6_LLINFO_REACHABLE;
			if (ln->ln_expire)
#if !defined(__FreeBSD__) || __FreeBSD__ < 3
				ln->ln_expire = time.tv_sec +
#else
				ln->ln_expire = time_second +
#endif
					nd_ifinfo[rt->rt_ifp->if_index].reachable;
		} else
			ln->ln_state = ND6_LLINFO_STALE;
		ln->ln_router = is_router;
	} else {
		int llchange;

		/*
		 * Check if the link-layer address has changed or not.
		 */
		if (!lladdr)
			llchange = 0;
		else {
			if (sdl->sdl_alen) {
				if (bcmp(lladdr, LLADDR(sdl), ifp->if_addrlen))
					llchange = 1;
				else
					llchange = 0;
			} else
				llchange = 1;
		}

		/*
		 * This is VERY complex.  Look at it with care.
		 *
		 * override solicit lladdr llchange	action
		 *					(L: record lladdr)
		 *
		 *	0	0	n	--	(2c)
		 *	0	0	y	n	(2b) L
		 *	0	0	y	y	(1)    REACHABLE->STALE
		 *	0	1	n	--	(2c)   *->REACHABLE
		 *	0	1	y	n	(2b) L *->REACHABLE
		 *	0	1	y	y	(1)    REACHABLE->STALE
		 *	1	0	n	--	(2a)
		 *	1	0	y	n	(2a) L
		 *	1	0	y	y	(2a) L *->STALE
		 *	1	1	n	--	(2a)   *->REACHABLE
		 *	1	1	y	n	(2a) L *->REACHABLE
		 *	1	1	y	y	(2a) L *->REACHABLE
		 */
		if (!is_override && (lladdr && llchange)) {	   /* (1) */
			/*
			 * If state is REACHABLE, make it STALE.
			 * no other updates should be done.
			 */
			if (ln->ln_state == ND6_LLINFO_REACHABLE)
				ln->ln_state = ND6_LLINFO_STALE;
			return;
		} else if (is_override				   /* (2a) */
			|| (!is_override && (lladdr && !llchange)) /* (2b) */
			|| !lladdr) {				   /* (2c) */
			/*
			 * Update link-local address, if any.
			 */
			if (lladdr) {
				sdl->sdl_alen = ifp->if_addrlen;
				bcopy(lladdr, LLADDR(sdl), ifp->if_addrlen);
			}

			/*
			 * If solicited, make the state REACHABLE.
			 * If not solicited and the link-layer address was
			 * changed, make it STALE.
			 */
			if (is_solicited) {
				ln->ln_state = ND6_LLINFO_REACHABLE;
				if (ln->ln_expire) {
#if !defined(__FreeBSD__) || __FreeBSD__ < 3
					ln->ln_expire = time.tv_sec +
#else
					ln->ln_expire = time_second +
#endif
						nd_ifinfo[ifp->if_index].reachable;
				}
			} else {
				if (lladdr && llchange)
					ln->ln_state = ND6_LLINFO_STALE;
			}
		}

		if (ln->ln_router && !is_router) {
			/*
			 * The peer dropped the router flag.
			 * Remove the sender from the Default Router List and
			 * update the Destination Cache entries.
			 */
			struct nd_defrouter *dr;
			struct in6_addr *in6;
			int s;

			in6 = &((struct sockaddr_in6 *)rt_key(rt))->sin6_addr;
			s = splnet();
			dr = defrouter_lookup(in6, rt->rt_ifp);
			if (dr)
				defrtrlist_del(dr);
			else if (!ip6_forwarding && ip6_accept_rtadv) {
				/*
				 * Even if the neighbor is not in the default
				 * router list, the neighbor may be used
				 * as a next hop for some destinations
				 * (e.g. redirect case). So we must
				 * call rt6_flush explicitly.
				 */
				rt6_flush(&ip6->ip6_src, rt->rt_ifp);
			}
			splx(s);
		}
		ln->ln_router = is_router;
	}
	rt->rt_flags &= ~RTF_REJECT;
	ln->ln_asked = 0;
	if (ln->ln_hold) {
		(*ifp->if_output)(ifp, ln->ln_hold, rt_key(rt), rt);
		ln->ln_hold = 0;
	}
}

/*
 * Neighbor advertisement output handling.
 *
 * Based on RFC 2461
 *
 * XXX NA delay for anycast address is not implemented yet
 *      (RFC 2461 7.2.7)
 * XXX proxy advertisement?
 */
void
nd6_na_output(ifp, daddr6, taddr6, flags, tlladdr)
	struct ifnet *ifp;
	struct in6_addr *daddr6, *taddr6;
	u_long flags;
	int tlladdr;	/* 1 if include target link-layer address */
{
	struct mbuf *m;
	struct ip6_hdr *ip6;
	struct nd_neighbor_advert *nd_na;
	struct in6_ifaddr *ia = NULL;
	struct ip6_moptions im6o;
	int icmp6len;
	caddr_t mac;
	
	if ((m = m_gethdr(M_DONTWAIT, MT_DATA)) == NULL)
		return;

	if (IN6_IS_ADDR_MULTICAST(daddr6)) {
		m->m_flags |= M_MCAST;
		im6o.im6o_multicast_ifp = ifp;
		im6o.im6o_multicast_hlim = 255;
		im6o.im6o_multicast_loop = 0;
	}

	icmp6len = sizeof(*nd_na);
	m->m_pkthdr.len = m->m_len = sizeof(struct ip6_hdr) + icmp6len;
	MH_ALIGN(m, m->m_len + 16); /* 1+1+6 is enough. but just in case */

	/* fill neighbor advertisement packet */
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow = 0;
	ip6->ip6_vfc = IPV6_VERSION;
	ip6->ip6_nxt = IPPROTO_ICMPV6;
	ip6->ip6_hlim = 255;
	if (IN6_IS_ADDR_UNSPECIFIED(daddr6)) {
		/* reply to DAD */
		ip6->ip6_dst.s6_addr16[0] = IPV6_ADDR_INT16_MLL;
		ip6->ip6_dst.s6_addr16[1] = htons(ifp->if_index);
		ip6->ip6_dst.s6_addr32[1] = 0;
		ip6->ip6_dst.s6_addr32[2] = 0;
		ip6->ip6_dst.s6_addr32[3] = IPV6_ADDR_INT32_ONE;
		flags &= ~ND_NA_FLAG_SOLICITED;
	} else
		ip6->ip6_dst = *daddr6;

	/*
	 * Select a source whose scope is the same as that of the dest.
	 */
	ia = in6_ifawithifp(ifp, &ip6->ip6_dst);
	if (ia == NULL) {
		m_freem(m);
		return;
	}
	ip6->ip6_src = ia->ia_addr.sin6_addr;
	nd_na = (struct nd_neighbor_advert *)(ip6 + 1);
	nd_na->nd_na_type = ND_NEIGHBOR_ADVERT;
	nd_na->nd_na_code = 0;
	nd_na->nd_na_target = *taddr6;
	if (IN6_IS_SCOPE_LINKLOCAL(&nd_na->nd_na_target))
		nd_na->nd_na_target.s6_addr16[1] = 0;

	/*
	 * "tlladdr" indicates NS's condition for adding tlladdr or not.
	 * see nd6_ns_input() for details.
	 * Basically, if NS packet is sent to unicast/anycast addr,
	 * target lladdr option SHOULD NOT be included.
	 */
	if (tlladdr && (mac = nd6_ifptomac(ifp))) {
		int optlen = sizeof(struct nd_opt_hdr) + ifp->if_addrlen;
		struct nd_opt_hdr *nd_opt = (struct nd_opt_hdr *)(nd_na + 1);
		
		m->m_pkthdr.len += optlen;
		m->m_len += optlen;
		icmp6len += optlen;
		nd_opt->nd_opt_type = ND_OPT_TARGET_LINKADDR;
		/* xxx 8 bytes alignment? */
		nd_opt->nd_opt_len = optlen >> 3;
		bcopy(mac, (caddr_t)(nd_opt + 1), ifp->if_addrlen);
	} else
		flags &= ~ND_NA_FLAG_OVERRIDE;

	ip6->ip6_plen = htons((u_short)icmp6len);
	nd_na->nd_na_flags_reserved = flags;
	nd_na->nd_na_cksum = 0;
	nd_na->nd_na_cksum =
		in6_cksum(m, IPPROTO_ICMPV6, sizeof(struct ip6_hdr), icmp6len);

#ifdef IPSEC
	m->m_pkthdr.rcvif = NULL;
#endif /*IPSEC*/
	ip6_output(m, NULL, NULL, 0, &im6o);
	icmp6stat.icp6s_outhist[ND_NEIGHBOR_ADVERT]++;
}

caddr_t
nd6_ifptomac(ifp)
	struct ifnet *ifp;
{
	switch (ifp->if_type) {
	case IFT_ETHER:
	case IFT_FDDI:
#ifdef __NetBSD__
		return LLADDR(ifp->if_sadl);
#else
		return ((caddr_t)(ifp + 1));
#endif
		break;
	default:
		return NULL;
	}
}

TAILQ_HEAD(dadq_head, dadq);
struct dadq {
	TAILQ_ENTRY(dadq) dad_list;
	struct ifaddr *dad_ifa;
	int dad_count;		/* max NS to send */
	int dad_ns_ocount;	/* NS sent so far */
	int dad_ns_icount;
	int dad_na_icount;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	struct callout_handle dad_timer;
#endif
};

static struct dadq_head dadq;

static struct dadq *
nd6_dad_find(ifa)
	struct ifaddr *ifa;
{
	struct dadq *dp;

	for (dp = dadq.tqh_first; dp; dp = dp->dad_list.tqe_next) {
		if (dp->dad_ifa == ifa)
			return dp;
	}
	return NULL;
}

/*
 * Start Duplicated Address Detection (DAD) for specified interface address.
 */
void
nd6_dad_start(ifa, tick)
	struct ifaddr *ifa;
	int *tick;	/* minimum delay ticks for IFF_UP event */
{
	struct in6_ifaddr *ia = (struct in6_ifaddr *)ifa;
	struct dadq *dp;
	static int dad_init = 0;

	if (!dad_init) {
		TAILQ_INIT(&dadq);
		dad_init++;
	}

	/*
	 * If we don't need DAD, don't do it.
	 * There are several cases:
	 * - DAD is disabled (ip6_dad_count == 0)
	 * - the interface address is anycast
	 */
	if (!(ia->ia6_flags & IN6_IFF_TENTATIVE)) {
		printf("nd6_dad_start: called with non-tentative address "
			"%s(%s)\n",
			ip6_sprintf(&ia->ia_addr.sin6_addr),
			ifa->ifa_ifp ? ifa->ifa_ifp->if_xname : "???");
		return;
	}
	if (ia->ia6_flags & IN6_IFF_ANYCAST) {
		ia->ia6_flags &= ~IN6_IFF_TENTATIVE;
		return;
	}
	if (!ip6_dad_count) {
		ia->ia6_flags &= ~IN6_IFF_TENTATIVE;
		return;
	}
	if (!ifa->ifa_ifp)
		panic("nd6_dad_start: ifa->ifa_ifp == NULL");
	if (!(ifa->ifa_ifp->if_flags & IFF_UP))
		return;
	if (nd6_dad_find(ifa) != NULL) {
		/* DAD already in progress */
		return;
	}

	dp = malloc(sizeof(*dp), M_IP6NDP, M_NOWAIT);
	if (dp == NULL) {
		printf("nd6_dad_start: memory allocation failed for "
			"%s(%s)\n",
			ip6_sprintf(&ia->ia_addr.sin6_addr),
			ifa->ifa_ifp ? ifa->ifa_ifp->if_xname : "???");
		return;
	}
	bzero(dp, sizeof(*dp));
	TAILQ_INSERT_TAIL(&dadq, (struct dadq *)dp, dad_list);

	printf("performing DAD for %s(%s)\n",
		ip6_sprintf(&ia->ia_addr.sin6_addr), ifa->ifa_ifp->if_xname);

	/*
	 * Send NS packet for DAD, ip6_dad_count times.
	 * Note that we must delay the first transmission, if this is the
	 * first packet to be sent from the interface after interface
	 * (re)initialization.
	 */
	dp->dad_ifa = ifa;
	ifa->ifa_refcnt++;	/*just for safety*/
	dp->dad_count = ip6_dad_count;
	dp->dad_ns_icount = dp->dad_na_icount = 0;
	dp->dad_ns_ocount = 0;
	if (!tick) {
		dp->dad_ns_ocount++;
		nd6_ns_output(ifa->ifa_ifp, NULL, &ia->ia_addr.sin6_addr,
			NULL, 1);
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		dp->dad_timer =
#endif
		timeout((void (*) __P((void *)))nd6_dad_timer, (void *)ifa,
			nd_ifinfo[ifa->ifa_ifp->if_index].retrans * hz / 1000);
	} else {
		int ntick;

		if (*tick == 0)
			ntick = random() % (MAX_RTR_SOLICITATION_DELAY * hz);
		else
			ntick = *tick + random() % (hz / 2);
		*tick = ntick;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		dp->dad_timer =
#endif
		timeout((void (*) __P((void *)))nd6_dad_timer, (void *)ifa,
			ntick);
	}
}

static void
nd6_dad_timer(ifa)
	struct ifaddr *ifa;
{
	int s;
	struct in6_ifaddr *ia = (struct in6_ifaddr *)ifa;
	struct dadq *dp;

	s = splnet();	/*XXX*/

	/* Sanity check */
	if (ia == NULL) {
		printf("nd6_dad_timer: called with null parameter\n");
		goto done;
	}
	dp = nd6_dad_find(ifa);
	if (dp == NULL) {
		printf("nd6_dad_timer: DAD structure not found\n");
		goto done;
	}
	if (ia->ia6_flags & IN6_IFF_DUPLICATED) {
		printf("nd6_dad_timer: called with duplicated address "
			"%s(%s)\n",
			ip6_sprintf(&ia->ia_addr.sin6_addr),
			ifa->ifa_ifp ? ifa->ifa_ifp->if_xname : "???");
		goto done;
	}
	if ((ia->ia6_flags & IN6_IFF_TENTATIVE) == 0) {
		printf("nd6_dad_timer: called with non-tentative address "
			"%s(%s)\n",
			ip6_sprintf(&ia->ia_addr.sin6_addr),
			ifa->ifa_ifp ? ifa->ifa_ifp->if_xname : "???");
		goto done;
	}

	/* Need more checks? */
	if (dp->dad_ns_ocount < dp->dad_count) {
		/*
		 * We have more NS to go.  Send NS packet for DAD.
		 */
		dp->dad_ns_ocount++;
		nd6_ns_output(ifa->ifa_ifp, NULL, &ia->ia_addr.sin6_addr,
			NULL, 1);
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		dp->dad_timer =
#endif
		timeout((void (*) __P((void *)))nd6_dad_timer, (void *)ifa,
			nd_ifinfo[ifa->ifa_ifp->if_index].retrans * hz / 1000);
	} else {
		/*
		 * We have transmitted sufficient number of DAD packets.
		 * See what we've got.
		 */
		int duplicate;

		duplicate = 0;

		if (dp->dad_na_icount) {
			/*
			 * the check is in nd6_dad_na_input(),
			 * but just in case
			 */
			duplicate++;
		}

		if (dp->dad_ns_icount) {
#if 0 /*heuristics*/
			/*
			 * if
			 * - we have sent many(?) DAD NS, and
			 * - the number of NS we sent equals to the
			 *   number of NS we've got, and
			 * - we've got no NA
			 * we may have a faulty network card/driver which
			 * loops back multicasts to myself.
			 */
			if (3 < dp->dad_count
			 && dp->dad_ns_icount == dp->dad_count
			 && dp->dad_na_icount == 0) {
				log(LOG_INFO, "DAD questionable for %s(%s): "
					"network card loops back multicast?\n",
					ip6_sprintf(&ia->ia_addr.sin6_addr),
					ifa->ifa_ifp->if_xname);
				/* XXX consider it a duplicate or not? */
				/* duplicate++; */
			} else {
				/* We've seen NS, means DAD has failed. */
				duplicate++;
			}
#else
			/* We've seen NS, means DAD has failed. */
			duplicate++;
#endif
		}

		if (duplicate) {
			/* (*dp) will be freed in nd6_dad_duplicated() */
			dp = NULL;
			nd6_dad_duplicated(ifa);
		} else {
			/*
			 * We are done with DAD.  No NA came, no NS came.
			 * duplicated address found.
			 */
			ia->ia6_flags &= ~IN6_IFF_TENTATIVE;

			printf("DAD success for %s(%s)\n",
				ip6_sprintf(&ia->ia_addr.sin6_addr),
				ifa->ifa_ifp->if_xname);
			TAILQ_REMOVE(&dadq, (struct dadq *)dp, dad_list);
			free(dp, M_IP6NDP);
			dp = NULL;
			ifa->ifa_refcnt--;
		}
	}

done:
	splx(s);
}

void
nd6_dad_duplicated(ifa)
	struct ifaddr *ifa;
{
	struct in6_ifaddr *ia = (struct in6_ifaddr *)ifa;
	struct dadq *dp;

	dp = nd6_dad_find(ifa);
	if (dp == NULL) {
		printf("nd6_dad_duplicated: DAD structure not found\n");
		return;
	}

	log(LOG_ERR, "DAD detected duplicate IP6 address %s(%s): "
		"got %d NS and %d NA\n", ip6_sprintf(&ia->ia_addr.sin6_addr),
		ifa->ifa_ifp->if_xname,
		dp->dad_ns_icount, dp->dad_na_icount);

	ia->ia6_flags &= ~IN6_IFF_TENTATIVE;
	ia->ia6_flags |= IN6_IFF_DUPLICATED;

	/* We are done with DAD, with duplicated address found. (failure) */
	untimeout((void (*) __P((void *)))nd6_dad_timer, (void *)ifa
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		, dp->dad_timer
#endif
		);

	printf("DAD failed for %s(%s): manual operation required\n",
		ip6_sprintf(&ia->ia_addr.sin6_addr),
		ifa->ifa_ifp->if_xname);
	TAILQ_REMOVE(&dadq, (struct dadq *)dp, dad_list);
	free(dp, M_IP6NDP);
	dp = NULL;
	ifa->ifa_refcnt--;
}

void
nd6_dad_ns_input(ifa)
	struct ifaddr *ifa;
{
	struct in6_ifaddr *ia;
	struct ifnet *ifp;
	struct in6_addr *taddr6;
	struct dadq *dp;
	int duplicate;

	if (!ifa)
		panic("ifa == NULL in nd6_dad_ns_input");

	ia = (struct in6_ifaddr *)ifa;
	ifp = ifa->ifa_ifp;
	taddr6 = &ia->ia_addr.sin6_addr;
	duplicate = 0;
	dp = nd6_dad_find(ifa);

	/*
	 * If it is from myself, ignore this.
	 */
	if (ifp && (ifp->if_flags & IFF_LOOPBACK))
		return;

	/* Quickhack - completely ignore DAD NS packets */
	if (dad_ignore_ns) {
		log(LOG_INFO, "nd6_dad_ns_input: ignoring DAD NS packet for "
		    "address %s(%s)\n", ip6_sprintf(taddr6),
		    ifa->ifa_ifp->if_xname);
		return;
	}

	/*
	 * if I'm yet to start DAD, someone else started using this address
	 * first.  I have a duplicate and you win.
	 */
	if (!dp || dp->dad_ns_ocount == 0)
		duplicate++;

	/* XXX more checks for loopback situation - see nd6_dad_timer too */

	if (duplicate) {
		dp = NULL;	/* will be freed in nd6_dad_duplicated() */
		nd6_dad_duplicated(ifa);
	} else {
		/*
		 * not sure if I got a duplicate.
		 * increment ns count and see what happens.
		 */
		if (dp)
			dp->dad_ns_icount++;
	}
}

void
nd6_dad_na_input(ifa)
	struct ifaddr *ifa;
{
	struct dadq *dp;

	if (!ifa)
		panic("ifa == NULL in nd6_dad_na_input");

	dp = nd6_dad_find(ifa);
	if (dp)
		dp->dad_na_icount++;

	/* remove the address. */
	nd6_dad_duplicated(ifa);
}
