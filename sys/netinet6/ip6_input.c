/*	$NetBSD: ip6_input.c,v 1.8 1999/10/01 10:15:16 itojun Exp $	*/

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
 *	@(#)ip_input.c	8.2 (Berkeley) 1/4/94
 */

#ifdef __FreeBSD__
#include "opt_ip6fw.h"
#endif
#if (defined(__FreeBSD__) && __FreeBSD__ >= 3) || defined(__NetBSD__)
#include "opt_inet.h"
#ifdef __NetBSD__	/*XXX*/
#include "opt_ipsec.h"
#endif
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#ifdef __NetBSD__
#include <sys/proc.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/in_systm.h>
#include <netinet6/ip6.h>
#if !defined(__FreeBSD__) || __FreeBSD__ < 3
#include <netinet6/in6_pcb.h>
#else
#include <netinet/in_pcb.h>
#endif
#include <netinet6/ip6_var.h>
#include <netinet6/icmp6.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/nd6.h>

#ifdef INET
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#endif /*INET*/

#include <netinet6/ip6protosw.h>

/* we need it for NLOOP. */
#include "loop.h"
#include "faith.h"

#include "gif.h"
#include "bpfilter.h"

extern struct domain inet6domain;
extern struct ip6protosw inet6sw[];
#ifdef __bsdi__
extern struct ifnet loif;
#endif

u_char ip6_protox[IPPROTO_MAX];
static int ip6qmaxlen = IFQ_MAXLEN;
struct in6_ifaddr *in6_ifaddr;
struct ifqueue ip6intrq;

#ifdef __NetBSD__
extern struct ifnet loif[NLOOP];
int ip6_forward_srcrt;			/* XXX */
int ip6_sourcecheck;			/* XXX */
int ip6_sourcecheck_interval;		/* XXX */
#endif

struct ip6stat ip6stat;

static void ip6_init2 __P((void *));

static int ip6_hopopts_input __P((u_int32_t *, u_int32_t *, struct mbuf **, int *));

/*
 * IP6 initialization: fill in IP6 protocol switch table.
 * All protocols not implemented in kernel go to raw IP6 protocol handler.
 */
void
ip6_init()
{
	register struct ip6protosw *pr;
	register int i;
	struct timeval tv;

	pr = (struct ip6protosw *)pffindproto(PF_INET6, IPPROTO_RAW, SOCK_RAW);
	if (pr == 0)
		panic("ip6_init");
	for (i = 0; i < IPPROTO_MAX; i++)
		ip6_protox[i] = pr - inet6sw;
	for (pr = (struct ip6protosw *)inet6domain.dom_protosw;
	    pr < (struct ip6protosw *)inet6domain.dom_protoswNPROTOSW; pr++)
		if (pr->pr_domain->dom_family == PF_INET6 &&
		    pr->pr_protocol && pr->pr_protocol != IPPROTO_RAW)
			ip6_protox[pr->pr_protocol] = pr - inet6sw;
	ip6intrq.ifq_maxlen = ip6qmaxlen;
	nd6_init();
	frag6_init();
	/*
	 * in many cases, random() here does NOT return random number
	 * as initialization during bootstrap time occur in fixed order.
	 */
	microtime(&tv);
	ip6_flow_seq = random() ^ tv.tv_usec;

	ip6_init2((void *)0);
}

static void
ip6_init2(dummy)
	void *dummy;
{
	int i;

	/*
	 * to route local address of p2p link to loopback,
	 * assign loopback address first. 
	 */
	for (i = 0; i < NLOOP; i++)
		in6_ifattach(&loif[i], IN6_IFT_LOOP, NULL, 0);

	/* get EUI64 from somewhere, attach pseudo interfaces */
	if (in6_ifattach_getifid(NULL) == 0)
		in6_ifattach_p2p();

	/* nd6_timer_init */
	timeout(nd6_timer, (caddr_t)0, hz);
}

#ifdef __FreeBSD__
/* cheat */
SYSINIT(netinet6init2, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, ip6_init2, NULL);
#endif

/*
 * IP6 input interrupt handling. Just pass the packet to ip6_input.
 */
void
ip6intr()
{
	int s;
	struct mbuf *m;

	for (;;) {
		s = splimp();
		IF_DEQUEUE(&ip6intrq, m);
		splx(s);
		if (m == 0)
			return;
		ip6_input(m);
	}
}

#ifdef __FreeBSD__
NETISR_SET(NETISR_IPV6, ip6intr);
#endif

extern struct	route_in6 ip6_forward_rt;

void
ip6_input(m)
	struct mbuf *m;
{
	register struct ip6_hdr *ip6;
	int off = sizeof(struct ip6_hdr), nest;
	u_int32_t plen;
	u_int32_t rtalert = ~0;
	int nxt, ours = 0;

#ifdef IPSEC
	/*
	 * should the inner packet be considered authentic?
	 * see comment in ah4_input().
	 */
	if (m) {
		m->m_flags &= ~M_AUTHIPHDR;
		m->m_flags &= ~M_AUTHIPDGM;
	}
#endif
	/*
	 * mbuf statistics by kazu
	 */
	if (m->m_flags & M_EXT) {
		if (m->m_next)
			ip6stat.ip6s_mext2m++;
		else
			ip6stat.ip6s_mext1++;
	} else {
		if (m->m_next) {
			if (m->m_flags & M_LOOP)
				ip6stat.ip6s_m2m[loif[0].if_index]++;	/*XXX*/
			else if (m->m_pkthdr.rcvif->if_index <= 31)
				ip6stat.ip6s_m2m[m->m_pkthdr.rcvif->if_index]++;
			else
				ip6stat.ip6s_m2m[0]++;
		} else
			ip6stat.ip6s_m1++;
	}

	IP6_EXTHDR_CHECK(m, 0, sizeof(struct ip6_hdr), /*nothing*/);

	ip6stat.ip6s_total++;

	if (m->m_len < sizeof(struct ip6_hdr) &&
	    (m = m_pullup(m, sizeof(struct ip6_hdr))) == 0) {
		ip6stat.ip6s_toosmall++;
		return;
	}

	ip6 = mtod(m, struct ip6_hdr *);

	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		ip6stat.ip6s_badvers++;
		goto bad;
	}

	ip6stat.ip6s_nxthist[ip6->ip6_nxt]++;

	/*
	 * Scope check
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_src) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_dst)) {
		ip6stat.ip6s_badscope++;
		goto bad;
	}
	if (IN6_IS_ADDR_LOOPBACK(&ip6->ip6_src) ||
	    IN6_IS_ADDR_LOOPBACK(&ip6->ip6_dst)) {
		if (m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK) {
			ours = 1;
			goto hbhcheck;
		} else {
			ip6stat.ip6s_badscope++;
			goto bad;
		}
	}

	if (m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK) {
		if (IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_dst)) {
			ours = 1;
			goto hbhcheck;
		}
	} else {
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src))
			ip6->ip6_src.s6_addr16[1]
				= htons(m->m_pkthdr.rcvif->if_index);
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst))
			ip6->ip6_dst.s6_addr16[1]
				= htons(m->m_pkthdr.rcvif->if_index);
	}

	/*
	 * Multicast check
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
	  	struct	in6_multi *in6m = 0;
		/*
		 * See if we belong to the destination multicast group on the
		 * arrival interface.
		 */
		IN6_LOOKUP_MULTI(ip6->ip6_dst, m->m_pkthdr.rcvif, in6m);
		if (in6m)
			ours = 1;
		else if (!ip6_mrouter) {
			ip6stat.ip6s_notmember++;
			ip6stat.ip6s_cantforward++;
			goto bad;
		}
		goto hbhcheck;
	}

	/*
	 *  Unicast check
	 */
	if (ip6_forward_rt.ro_rt == 0 ||
	    !IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
				&ip6_forward_rt.ro_dst.sin6_addr)) {
		if (ip6_forward_rt.ro_rt) {
			RTFREE(ip6_forward_rt.ro_rt);
			ip6_forward_rt.ro_rt = 0;
		}
		bzero(&ip6_forward_rt.ro_dst, sizeof(struct sockaddr_in6));
		ip6_forward_rt.ro_dst.sin6_len = sizeof(struct sockaddr_in6);
		ip6_forward_rt.ro_dst.sin6_family = AF_INET6;
		ip6_forward_rt.ro_dst.sin6_addr = ip6->ip6_dst;

#if defined(__bsdi__) || defined(__NetBSD__)
		rtalloc((struct route *)&ip6_forward_rt);
#endif
#ifdef __FreeBSD__
		rtalloc_ign((struct route *)&ip6_forward_rt, RTF_PRCLONING);
#endif
	}

#define rt6_key(r) ((struct sockaddr_in6 *)((r)->rt_nodes->rn_key))

	/*
	 * Accept the packet if the forwarding interface to the destination
	 * according to the routing table is the loopback interface,
	 * unless the associated route has a gateway.
	 * Note that this approach causes to accept a packet if there is a
	 * route to the loopback interface for the destination of the packet.
	 * But we think it's even useful in some situations, e.g. when using
	 * a special daemon which wants to intercept the packet.
	 */
	if (ip6_forward_rt.ro_rt &&
	    (ip6_forward_rt.ro_rt->rt_flags &
	     (RTF_HOST|RTF_GATEWAY)) == RTF_HOST &&
#if 0
	    /*
	     * The check below is redundant since the comparison of
	     * the destination and the key of the rtentry has
	     * already done through looking up the routing table.
	     */
	    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst,
			       &rt6_key(ip6_forward_rt.ro_rt)->sin6_addr) &&
#endif
#ifdef __bsdi__
	    ip6_foward.rt.ro_rt->rt_ifp == &loif
#else
	    ip6_forward_rt.ro_rt->rt_ifp == &loif[0]
#endif
		) {
		struct in6_ifaddr *ia6 =
			(struct in6_ifaddr *)ip6_forward_rt.ro_rt->rt_ifa;
		/* packet to tentative address must not be received */
		if (ia6->ia6_flags & IN6_IFF_ANYCAST)
			m->m_flags |= M_ANYCAST6;
		if (!(ia6->ia6_flags & IN6_IFF_NOTREADY)) {
			/* this interface is ready */
			ours = 1;
			goto hbhcheck;
		} else {
			/* this interface is not ready, fall through */
		}
	}

	/*
	 * FAITH(Firewall Aided Internet Translator)
	 */
#if defined(NFAITH) && 0 < NFAITH
	if (ip6_keepfaith) {
		if (ip6_forward_rt.ro_rt && ip6_forward_rt.ro_rt->rt_ifp
		 && ip6_forward_rt.ro_rt->rt_ifp->if_type == IFT_FAITH) {
			/* XXX do we need more sanity checks? */
			ours = 1;
			goto hbhcheck;
		}
	}
#endif

	/*
	 * Now there is no reason to process the packet if it's not our own
	 * and we're not a router.
	 */
	if (!ip6_forwarding) {
		ip6stat.ip6s_cantforward++;
		goto bad;
	}

  hbhcheck:
	/*
	 * Process Hop-by-Hop options header if it's contained.
	 * m may be modified in ip6_hopopts_input().
	 * If a JumboPayload option is included, plen will also be modified.
	 */
	plen = (u_int32_t)ntohs(ip6->ip6_plen);
	if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
		if (ip6_hopopts_input(&plen, &rtalert, &m, &off))
			return;	/* m have already been freed */
		/* adjust pointer */
		ip6 = mtod(m, struct ip6_hdr *);
		nxt = ((struct ip6_hbh *)(ip6 + 1))->ip6h_nxt;

		/*
		 * accept the packet if a router alert option is included
		 * and we act as an IPv6 router.
		 */
		if (rtalert != ~0 && ip6_forwarding)
			ours = 1;
	} else
		nxt = ip6->ip6_nxt;

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IPv6 header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len - sizeof(struct ip6_hdr) < plen) {
		ip6stat.ip6s_tooshort++;
		goto bad;
	}
	if (m->m_pkthdr.len > sizeof(struct ip6_hdr) + plen) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = sizeof(struct ip6_hdr) + plen;
			m->m_pkthdr.len = sizeof(struct ip6_hdr) + plen;
		} else
			m_adj(m, sizeof(struct ip6_hdr) + plen - m->m_pkthdr.len);
	}

	/*
	 * Forward if desirable.
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) {
		/*
		 * If we are acting as a multicast router, all
		 * incoming multicast packets are passed to the
		 * kernel-level multicast forwarding function.
		 * The packet is returned (relatively) intact; if
		 * ip6_mforward() returns a non-zero value, the packet
		 * must be discarded, else it may be accepted below.
		 */
		if (ip6_mrouter && ip6_mforward(ip6, m->m_pkthdr.rcvif, m)) {
			ip6stat.ip6s_cantforward++;
			m_freem(m);
			return;
		}
		if (!ours) {
			m_freem(m);
			return;
		}
	}
	else if (!ours) {
		ip6_forward(m, 0);
		return;
	}	

	/*
	 * Tell launch routine the next header
	 */
	ip6stat.ip6s_delivered++;
	nest = 0;
	while (nxt != IPPROTO_DONE) {
		if (ip6_hdrnestlimit && (++nest > ip6_hdrnestlimit)) {
			ip6stat.ip6s_toomanyhdr++;
			goto bad;
		}

		/*
		 * protection against faulty packet - there should be
		 * more sanity checks in header chain processing.
		 */
		if (m->m_pkthdr.len < off) {
			ip6stat.ip6s_tooshort++;
			goto bad;
		}

		nxt = (*inet6sw[ip6_protox[nxt]].pr_input)(&m, &off, nxt);
	}
	return;
 bad:
	m_freem(m);
}

/*
 * Hop-by-Hop options header processing. If a valid jumbo payload option is
 * included, the real payload length will be stored in plenp.
 */
static int
ip6_hopopts_input(plenp, rtalertp, mp, offp)
	u_int32_t *plenp;
	u_int32_t *rtalertp;	/* XXX: should be stored more smart way */
	struct mbuf **mp;
	int *offp;
{
	register struct mbuf *m = *mp;
	int off = *offp, hbhlen;
	struct ip6_hbh *hbh;
	u_int8_t *opt;

	/* validation of the length of the header */
	IP6_EXTHDR_CHECK(m, off, sizeof(*hbh), -1);
	hbh = (struct ip6_hbh *)(mtod(m, caddr_t) + off);
	hbhlen = (hbh->ip6h_len + 1) << 3;

	IP6_EXTHDR_CHECK(m, off, hbhlen, -1);
	hbh = (struct ip6_hbh *)(mtod(m, caddr_t) + off);
	off += hbhlen;
	hbhlen -= sizeof(struct ip6_hbh);
	opt = (u_int8_t *)hbh + sizeof(struct ip6_hbh);

	if (ip6_process_hopopts(m, (u_int8_t *)hbh + sizeof(struct ip6_hbh),
				hbhlen, rtalertp, plenp) < 0)
		return(-1);

	*offp = off;
	*mp = m;
	return(0);
}

/*
 * Search header for all Hop-by-hop options and process each option.
 * This function is separate from ip6_hopopts_input() in order to
 * handle a case where the sending node itself process its hop-by-hop
 * options header. In such a case, the function is called from ip6_output().
 */
int
ip6_process_hopopts(m, opthead, hbhlen, rtalertp, plenp)
	struct mbuf *m;
	u_int8_t *opthead;
	int hbhlen;
	u_int32_t *rtalertp;
	u_int32_t *plenp;
{
	struct ip6_hdr *ip6;
	int optlen = 0;
	u_int8_t *opt = opthead;
	u_int16_t rtalert_val;

	for (; hbhlen > 0; hbhlen -= optlen, opt += optlen) {
		switch(*opt) {
		 case IP6OPT_PAD1:
			 optlen = 1;
			 break;
		 case IP6OPT_PADN:
			 if (hbhlen < IP6OPT_MINLEN) {
				 ip6stat.ip6s_toosmall++;
				 goto bad;
			 }
			 optlen = *(opt + 1) + 2;
			 break;
		 case IP6OPT_RTALERT:
			 /* XXX may need check for alignment */
			 if (hbhlen < IP6OPT_RTALERT_LEN) {
				 ip6stat.ip6s_toosmall++;
				 goto bad;
			 }
			 if (*(opt + 1) != IP6OPT_RTALERT_LEN - 2)
				  /* XXX: should we discard the packet? */
				 log(LOG_ERR, "length of router alert opt is inconsitent(%d)",
				     *(opt + 1));
			 optlen = IP6OPT_RTALERT_LEN;
			 bcopy((caddr_t)(opt + 2), (caddr_t)&rtalert_val, 2);
			 *rtalertp = ntohs(rtalert_val);
			 break;
		 case IP6OPT_JUMBO:
			 /* XXX may need check for alignment */
			 if (hbhlen < IP6OPT_JUMBO_LEN) {
				 ip6stat.ip6s_toosmall++;
				 goto bad;
			 }
			 if (*(opt + 1) != IP6OPT_JUMBO_LEN - 2)
				  /* XXX: should we discard the packet? */
				 log(LOG_ERR, "length of jumbopayload opt "
				     "is inconsistent(%d)",
				     *(opt + 1));
			 optlen = IP6OPT_JUMBO_LEN;

			 /*
			  * We can simply cast because of the alignment
			  * requirement of the jumbo payload option.
			  */
#if 0
			 *plenp = ntohl(*(u_int32_t *)(opt + 2));
#else
			 bcopy(opt + 2, plenp, sizeof(*plenp));
			 *plenp = htonl(*plenp);
#endif
			 if (*plenp <= IPV6_MAXPACKET) {
				 /*
				  * jumbo payload length must be larger
				  * than 65535
				  */
				 ip6stat.ip6s_badoptions++;
				 icmp6_error(m, ICMP6_PARAM_PROB,
					     ICMP6_PARAMPROB_HEADER,
					     sizeof(struct ip6_hdr) +
					     sizeof(struct ip6_hbh) +
					     opt + 2 - opthead);
				 return(-1);
			 }

			 ip6 = mtod(m, struct ip6_hdr *);
			 if (ip6->ip6_plen) {
				 /*
				  * IPv6 packets that have non 0 payload length
				  * must not contain a jumbo paylod option.
				  */
				 ip6stat.ip6s_badoptions++;
				 icmp6_error(m, ICMP6_PARAM_PROB,
					     ICMP6_PARAMPROB_HEADER,
					     sizeof(struct ip6_hdr) +
					     sizeof(struct ip6_hbh) +
					     opt - opthead);
				 return(-1);
			 }
			 break;
		 default:		/* unknown option */
			 if (hbhlen < IP6OPT_MINLEN) {
				 ip6stat.ip6s_toosmall++;
				 goto bad;
			 }
			 if ((optlen = ip6_unknown_opt(opt, m,
						       sizeof(struct ip6_hdr) +
						       sizeof(struct ip6_hbh) +
						       opt - opthead)) == -1)
				 return(-1);
			 optlen += 2;
			 break;
		}
	}

	return(0);

  bad:
	m_freem(m);
	return(-1);
}

/*
 * Unknown option processing.
 * The third argument `off' is the offset from the IPv6 header to the option,
 * which is necessary if the IPv6 header the and option header and IPv6 header
 * is not continuous in order to return an ICMPv6 error.
 */
int
ip6_unknown_opt(optp, m, off)
	u_int8_t *optp;
	struct mbuf *m;
	int off;
{
	struct ip6_hdr *ip6;

	switch(IP6OPT_TYPE(*optp)) {
	 case IP6OPT_TYPE_SKIP: /* ignore the option */
		 return((int)*(optp + 1));
	 case IP6OPT_TYPE_DISCARD:	/* silently discard */
		 m_freem(m);
		 return(-1);
	 case IP6OPT_TYPE_FORCEICMP: /* send ICMP even if multicasted */
		 ip6stat.ip6s_badoptions++;
		 icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_OPTION, off);
		 return(-1);
	 case IP6OPT_TYPE_ICMP: /* send ICMP if not multicasted */
		 ip6stat.ip6s_badoptions++;
		 ip6 = mtod(m, struct ip6_hdr *);
		 if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
		     (m->m_flags & (M_BCAST|M_MCAST)))
			 m_freem(m);
		 else
			 icmp6_error(m, ICMP6_PARAM_PROB,
				     ICMP6_PARAMPROB_OPTION, off);
		 return(-1);
	}

	m_freem(m);		/* XXX: NOTREACHED */
	return(-1);
}

/*
 * Create the "control" list for this pcb
 */
void
ip6_savecontrol(in6p, mp, ip6, m)
	register struct in6pcb *in6p;
	register struct mbuf **mp;
	register struct ip6_hdr *ip6;
	register struct mbuf *m;
{
#ifdef __NetBSD__
	struct proc *p = curproc;	/* XXX */
#endif
#ifdef __bsdi__
# define sbcreatecontrol	so_cmsg
#endif

	if (in6p->in6p_socket->so_options & SO_TIMESTAMP) {
		struct timeval tv;

		microtime(&tv);
		*mp = sbcreatecontrol((caddr_t) &tv, sizeof(tv),
			SCM_TIMESTAMP, SOL_SOCKET);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (in6p->in6p_flags & IN6P_RECVDSTADDR) {
		*mp = sbcreatecontrol((caddr_t) &ip6->ip6_dst,
			sizeof(struct in6_addr), IPV6_RECVDSTADDR,
			IPPROTO_IPV6);
		if (*mp)
			mp = &(*mp)->m_next;
	}

#ifdef noyet
	/* options were tossed above */
	if (in6p->in6p_flags & IN6P_RECVOPTS)
		/* broken */
	/* ip6_srcroute doesn't do what we want here, need to fix */
	if (in6p->in6p_flags & IPV6P_RECVRETOPTS)
		/* broken */
#endif

	/* RFC 2292 sec. 5 */
	if (in6p->in6p_flags & IN6P_PKTINFO) {
		struct in6_pktinfo pi6;
		bcopy(&ip6->ip6_dst, &pi6.ipi6_addr, sizeof(struct in6_addr));
		if (IN6_IS_SCOPE_LINKLOCAL(&pi6.ipi6_addr))
			pi6.ipi6_addr.s6_addr16[1] = 0;
		pi6.ipi6_ifindex = (m && m->m_pkthdr.rcvif)
					? m->m_pkthdr.rcvif->if_index
					: 0;
		*mp = sbcreatecontrol((caddr_t) &pi6,
			sizeof(struct in6_pktinfo), IPV6_PKTINFO,
			IPPROTO_IPV6);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	if (in6p->in6p_flags & IN6P_HOPLIMIT) {
		int hlim = ip6->ip6_hlim & 0xff;
		*mp = sbcreatecontrol((caddr_t) &hlim,
			sizeof(int), IPV6_HOPLIMIT, IPPROTO_IPV6);
		if (*mp)
			mp = &(*mp)->m_next;
	}
	/* IN6P_NEXTHOP - for outgoing packet only */

	/*
	 * IPV6_HOPOPTS socket option. We require super-user privilege
	 * for the option, but it might be too strict, since there might
	 * be some hop-by-hop options which can be returned to normal user.
	 * See RFC 2292 section 6.
	 */
	if ((in6p->in6p_flags & IN6P_HOPOPTS) &&
	    p && !suser(p->p_ucred, &p->p_acflag)) {
		/*
		 * Check if a hop-by-hop options header is contatined in the
		 * received packet, and if so, store the options as ancillary
		 * data. Note that a hop-by-hop options header must be
		 * just after the IPv6 header, which fact is assured through
		 * the IPv6 input processing.
		 */
		struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
		if (ip6->ip6_nxt == IPPROTO_HOPOPTS) {
			struct ip6_hbh *hbh = (struct ip6_hbh *)(ip6 + 1);

			/*
			 * XXX: We copy whole the header even if a jumbo
			 * payload option is included, which option is to
			 * be removed before returning in the RFC 2292.
			 * But it's too painful operation...
			 */
			*mp = sbcreatecontrol((caddr_t)hbh,
					      (hbh->ip6h_len + 1) << 3,
					      IPV6_HOPOPTS, IPPROTO_IPV6);
			if (*mp)
				mp = &(*mp)->m_next;
		}
	}

	/* IPV6_DSTOPTS and IPV6_RTHDR socket options */
	if (in6p->in6p_flags & (IN6P_DSTOPTS | IN6P_RTHDR)) {
		struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
		int nxt = ip6->ip6_nxt, off = sizeof(struct ip6_hdr);;

		/*
		 * Search for destination options headers or routing
		 * header(s) through the header chain, and stores each
		 * header as ancillary data.
		 * Note that the order of the headers remains in
		 * the chain of ancillary data.
		 */
		while(1) {	/* is explicit loop prevention necessary? */
			struct ip6_ext *ip6e =
				(struct ip6_ext *)(mtod(m, caddr_t) + off);

			switch(nxt) {
		         case IPPROTO_DSTOPTS:
				 if (!in6p->in6p_flags & IN6P_DSTOPTS)
					 break;

				 /*
				  * We also require super-user privilege for
				  * the option.
				  * See the comments on IN6_HOPOPTS.
				  */
				 if (!p || !suser(p->p_ucred, &p->p_acflag))
					 break;

				 *mp = sbcreatecontrol((caddr_t)ip6e,
						       (ip6e->ip6e_len + 1) << 3,
						       IPV6_DSTOPTS,
						       IPPROTO_IPV6);
				 if (*mp)
					 mp = &(*mp)->m_next;
				 break;

			 case IPPROTO_ROUTING:
				 if (!in6p->in6p_flags & IN6P_RTHDR)
					 break;

				 *mp = sbcreatecontrol((caddr_t)ip6e,
						       (ip6e->ip6e_len + 1) << 3,
						       IPV6_RTHDR,
						       IPPROTO_IPV6);
				 if (*mp)
					 mp = &(*mp)->m_next;
				 break;

			 case IPPROTO_UDP:
			 case IPPROTO_TCP:
			 case IPPROTO_ICMPV6:
			 default:
				 /*
				  * stop search if we encounter an upper
				  * layer protocol headers.
				  */
				 goto loopend;

			 case IPPROTO_HOPOPTS:
			 case IPPROTO_AH: /* is it possible? */
				 break;
			}

			/* proceed with the next header. */
			if (nxt == IPPROTO_AH)
				off += (ip6e->ip6e_len + 2) << 2;
			else
				off += (ip6e->ip6e_len + 1) << 3;
			nxt = ip6e->ip6e_nxt;
		}
	  loopend:
	}
	if ((in6p->in6p_flags & IN6P_HOPOPTS)
	 && p && !suser(p->p_ucred, &p->p_acflag)) {
		/* to be done */
	}
	if ((in6p->in6p_flags & IN6P_DSTOPTS)
	 && p && !suser(p->p_ucred, &p->p_acflag)) {
		/* to be done */
	}
	/* IN6P_RTHDR - to be done */

#ifdef __bsdi__
# undef sbcreatecontrol
#endif
}

/*
 * Get pointer to the previous header followed by the header
 * currently processed.
 * XXX: This function supposes that
 *	M includes all headers,
 *	the next header field and the header length field of each header
 *	are valid, and
 *	the sum of each header length equals to OFF.
 * Because of these assumptions, this function must be called very
 * carefully. Moreover, it will not be used in the near future when
 * we develop `neater' mechanism to process extension headers.
 */
char *
ip6_get_prevhdr(m, off)
	struct mbuf *m;
	int off;
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

	if (off == sizeof(struct ip6_hdr))
		return(&ip6->ip6_nxt);
	else {
		int len, nxt;
		struct ip6_ext *ip6e = NULL;

		nxt = ip6->ip6_nxt;
		len = sizeof(struct ip6_hdr);
		while (len < off) {
			ip6e = (struct ip6_ext *)(mtod(m, caddr_t) + len);

			switch(nxt) {
			case IPPROTO_FRAGMENT:
				len += sizeof(struct ip6_frag);
				break;
			case IPPROTO_AH:
				len += (ip6e->ip6e_len + 2) << 2;
				break;
			default:
				len += (ip6e->ip6e_len + 1) << 3;
				break;
			}
			nxt = ip6e->ip6e_nxt;
		}
		if (ip6e)
			return(&ip6e->ip6e_nxt);
		else
			return NULL;
	}
}

/*
 * System control for IP6
 */

u_char	inet6ctlerrmap[PRC_NCMDS] = {
	0,		0,		0,		0,
	0,		EMSGSIZE,	EHOSTDOWN,	EHOSTUNREACH,
	EHOSTUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	EHOSTUNREACH,	0,		0,
	0,		0,		0,		0,
	ENOPROTOOPT
};

#ifdef __NetBSD__
#include <vm/vm.h>
#include <sys/sysctl.h>

int
ip6_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return ENOTDIR;

	switch (name[0]) {

	case IPV6CTL_FORWARDING:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				  &ip6_forwarding);
	case IPV6CTL_SENDREDIRECTS:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&ip6_sendredirects);
	case IPV6CTL_DEFHLIM:
		return sysctl_int(oldp, oldlenp, newp, newlen, &ip6_defhlim);
	case IPV6CTL_MAXFRAGPACKETS:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&ip6_maxfragpackets);
	case IPV6CTL_ACCEPT_RTADV:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&ip6_accept_rtadv);
	case IPV6CTL_KEEPFAITH:
		return sysctl_int(oldp, oldlenp, newp, newlen, &ip6_keepfaith);
	case IPV6CTL_LOG_INTERVAL:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&ip6_log_interval);
	case IPV6CTL_HDRNESTLIMIT:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&ip6_hdrnestlimit);
	case IPV6CTL_DAD_COUNT:
		return sysctl_int(oldp, oldlenp, newp, newlen, &ip6_dad_count);
	case IPV6CTL_AUTO_FLOWLABEL:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&ip6_auto_flowlabel);
	case IPV6CTL_DEFMCASTHLIM:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&ip6_defmcasthlim);
	case IPV6CTL_GIF_HLIM:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&ip6_gif_hlim);
	case IPV6CTL_KAME_VERSION:
		return sysctl_rdstring(oldp, oldlenp, newp, __KAME_VERSION);
	default:
		return EOPNOTSUPP;
	}
	/* NOTREACHED */
}
#endif /* __NetBSD__ */
