/*	$NetBSD: route.c,v 1.48.4.1 2000/10/18 01:32:48 tv Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "from: @(#)route.c	8.3 (Berkeley) 3/9/94";
#else
__RCSID("$NetBSD: route.c,v 1.48.4.1 2000/10/18 01:32:48 tv Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/un.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#define _KERNEL
#include <net/route.h>
#undef _KERNEL
#include <netinet/in.h>
#include <netatalk/at.h>
#include <netiso/iso.h>

#include <netns/ns.h>

#include <sys/sysctl.h>

#include <arpa/inet.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "netstat.h"

#define kget(p, d) (kread((u_long)(p), (char *)&(d), sizeof (d)))

/* alignment constraint for routing socket */
#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

/*
 * Definitions for showing gateway flags.
 */
struct bits {
	short	b_mask;
	char	b_val;
} bits[] = {
	{ RTF_UP,	'U' },
	{ RTF_GATEWAY,	'G' },
	{ RTF_HOST,	'H' },
	{ RTF_REJECT,	'R' },
	{ RTF_BLACKHOLE,'B' },
	{ RTF_DYNAMIC,	'D' },
	{ RTF_MODIFIED,	'M' },
	{ RTF_DONE,	'd' }, /* Completed -- for routing messages only */
	{ RTF_MASK,	'm' }, /* Mask Present -- for routing messages only */
	{ RTF_CLONING,	'C' },
	{ RTF_XRESOLVE,	'X' },
	{ RTF_LLINFO,	'L' },
	{ RTF_STATIC,	'S' },
	{ RTF_PROTO1,	'1' },
	{ RTF_PROTO2,	'2' },
	{ 0 }
};

/*
 * XXX we put all of the sockaddr types in here to force the alignment
 * to be correct.
 */
static union sockaddr_union {
	struct	sockaddr u_sa;
	struct	sockaddr_in u_in;
	struct	sockaddr_un u_un;
	struct	sockaddr_iso u_iso;
	struct	sockaddr_at u_at;
	struct	sockaddr_dl u_dl;
	struct	sockaddr_ns u_ns;
	u_short	u_data[128];
	int u_dummy;		/* force word-alignment */
} pt_u;

int	do_rtent = 0;
struct	rtentry rtentry;
struct	radix_node rnode;
struct	radix_mask rmask;

int	NewTree = 0;

static struct sockaddr *kgetsa __P((struct sockaddr *));
static void p_tree __P((struct radix_node *));
static void p_rtnode __P((void));
static void ntreestuff __P((void));
static void np_rtentry __P((struct rt_msghdr *));
static void p_sockaddr __P((const struct sockaddr *,
			    const struct sockaddr *, int, int));
static void p_flags __P((int));
static void p_rtentry __P((struct rtentry *));
static void ntreestuff __P((void));
static u_long forgemask __P((u_long));
static void domask __P((char *, size_t, u_long, u_long));
#ifdef INET6
char *netname6 __P((struct sockaddr_in6 *, struct in6_addr *));
#endif 

/*
 * Print routing tables.
 */
void
routepr(rtree)
	u_long rtree;
{
	struct radix_node_head *rnh, head;
	int i;

	printf("Routing tables\n");

	if (Aflag == 0 && NewTree)
		ntreestuff();
	else {
		if (rtree == 0) {
			printf("rt_tables: symbol not in namelist\n");
			return;
		}

		kget(rtree, rt_tables);
		for (i = 0; i <= AF_MAX; i++) {
			if ((rnh = rt_tables[i]) == 0)
				continue;
			kget(rnh, head);
			if (i == AF_UNSPEC) {
				if (Aflag && af == 0) {
					printf("Netmasks:\n");
					p_tree(head.rnh_treetop);
				}
			} else if (af == AF_UNSPEC || af == i) {
				pr_family(i);
				do_rtent = 1;
				pr_rthdr(i);
				p_tree(head.rnh_treetop);
			}
		}
	}
}

/*
 * Print address family header before a section of the routing table.
 */
void
pr_family(af)
	int af;
{
	char *afname;

	switch (af) {
	case AF_INET:
		afname = "Internet";
		break;
#ifdef INET6
	case AF_INET6:
		afname = "Internet6";
		break;
#endif 
	case AF_NS:
		afname = "XNS";
		break;
	case AF_ISO:
		afname = "ISO";
		break;
	case AF_APPLETALK:
		afname = "AppleTalk";
		break;
	case AF_CCITT:
		afname = "X.25";
		break;
	default:
		afname = NULL;
		break;
	}
	if (afname)
		printf("\n%s:\n", afname);
	else
		printf("\nProtocol Family %d:\n", af);
}

/* column widths; each followed by one space */
#ifndef INET6
#define	WID_DST(af)	18	/* width of destination column */
#define	WID_GW(af)	18	/* width of gateway column */
#else
/* width of destination/gateway column */
/* strlen("fe80::aaaa:bbbb:cccc:dddd") == 25, strlen("/128") == 4 */
#define	WID_DST(af)	((af) == AF_INET6 ? (nflag ? 29 : 18) : 18)
#define	WID_GW(af)	((af) == AF_INET6 ? (nflag ? 25 : 18) : 18)
#endif /* INET6 */

/*
 * Print header for routing table columns.
 */
void
pr_rthdr(af)
	int af;
{

	if (Aflag)
		printf("%-8.8s ","Address");
	printf("%-*.*s %-*.*s %-6.6s  %6.6s%8.8s %6.6s  %s\n",
		WID_DST(af), WID_DST(af), "Destination",
		WID_GW(af), WID_GW(af), "Gateway",
		"Flags", "Refs", "Use", "Mtu", "Interface");
}

static struct sockaddr *
kgetsa(dst)
	struct sockaddr *dst;
{

	kget(dst, pt_u.u_sa);
	if (pt_u.u_sa.sa_len > sizeof (pt_u.u_sa))
		kread((u_long)dst, (char *)pt_u.u_data, pt_u.u_sa.sa_len);
	return (&pt_u.u_sa);
}

static void
p_tree(rn)
	struct radix_node *rn;
{

again:
	kget(rn, rnode);
	if (rnode.rn_b < 0) {
		if (Aflag)
			printf("%-8.8lx ", (u_long) rn);
		if (rnode.rn_flags & RNF_ROOT) {
			if (Aflag)
				printf("(root node)%s",
				    rnode.rn_dupedkey ? " =>\n" : "\n");
		} else if (do_rtent) {
			kget(rn, rtentry);
			p_rtentry(&rtentry);
			if (Aflag)
				p_rtnode();
		} else {
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_key),
			    NULL, 0, 44);
			putchar('\n');
		}
		if ((rn = rnode.rn_dupedkey) != NULL)
			goto again;
	} else {
		if (Aflag && do_rtent) {
			printf("%-8.8lx ", (u_long) rn);
			p_rtnode();
		}
		rn = rnode.rn_r;
		p_tree(rnode.rn_l);
		p_tree(rn);
	}
}

static void
p_rtnode()
{
	struct radix_mask *rm = rnode.rn_mklist;
	char	nbuf[20];

	if (rnode.rn_b < 0) {
		if (rnode.rn_mask) {
			printf("\t  mask ");
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_mask),
				    NULL, 0, -1);
		} else if (rm == 0)
			return;
	} else {
		(void)snprintf(nbuf, sizeof nbuf, "(%d)", rnode.rn_b);
		printf("%6.6s %8.8lx : %8.8lx", nbuf, (u_long) rnode.rn_l,
		    (u_long) rnode.rn_r);
	}
	while (rm) {
		kget(rm, rmask);
		(void)snprintf(nbuf, sizeof nbuf, " %d refs, ", rmask.rm_refs);
		printf(" mk = %8.8lx {(%d),%s", (u_long) rm,
		    -1 - rmask.rm_b, rmask.rm_refs ? nbuf : " ");
		if (rmask.rm_flags & RNF_NORMAL) {
			struct radix_node rnode_aux;
			printf(" <normal>, ");
			kget(rmask.rm_leaf, rnode_aux);
			p_sockaddr(kgetsa((struct sockaddr *)rnode_aux.rn_mask),
				    NULL, 0, -1);
		} else
			p_sockaddr(kgetsa((struct sockaddr *)rmask.rm_mask),
			    NULL, 0, -1);
		putchar('}');
		if ((rm = rmask.rm_mklist) != NULL)
			printf(" ->");
	}
	putchar('\n');
}

static void
ntreestuff()
{
	size_t needed;
	int mib[6];
	char *buf, *next, *lim;
	struct rt_msghdr *rtm;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(1, "route sysctl estimate");
	if ((buf = malloc(needed)) == 0)
		errx(1, "out of space");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		err(1, "sysctl of routing table");
	lim  = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		np_rtentry(rtm);
	}
}

static void
np_rtentry(rtm)
	struct rt_msghdr *rtm;
{
	struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
#ifdef notdef
	static int masks_done, banner_printed;
#endif
	static int old_af;
	int af = 0, interesting = RTF_UP | RTF_GATEWAY | RTF_HOST;

	if (Lflag && (rtm->rtm_flags & RTF_LLINFO))
		return;
#ifdef notdef
	/* for the moment, netmasks are skipped over */
	if (!banner_printed) {
		printf("Netmasks:\n");
		banner_printed = 1;
	}
	if (masks_done == 0) {
		if (rtm->rtm_addrs != RTA_DST ) {
			masks_done = 1;
			af = sa->sa_family;
		}
	} else
#endif
		af = sa->sa_family;
	if (af != old_af) {
		pr_family(af);
		old_af = af;
	}
	if (rtm->rtm_addrs == RTA_DST)
		p_sockaddr(sa, NULL, 0, 36);
	else {
		p_sockaddr(sa, NULL, rtm->rtm_flags, 16);
#if 0
		if (sa->sa_len == 0)
			sa->sa_len = sizeof(long);
#endif
		sa = (struct sockaddr *)(ROUNDUP(sa->sa_len) + (char *)sa);
		p_sockaddr(sa, NULL, 0, 18);
	}
	p_flags(rtm->rtm_flags & interesting);
	putchar('\n');
}

static void
p_sockaddr(sa, mask, flags, width)
	const struct sockaddr *sa, *mask;
	int flags, width;
{
	char workbuf[128], *cplim;
	char *cp = workbuf;

	switch(sa->sa_family) {
	case AF_INET:
	    {
		struct sockaddr_in *sin = (struct sockaddr_in *)sa;

		if ((sin->sin_addr.s_addr == INADDR_ANY) &&
		    (mask != NULL) &&
		    (((struct sockaddr_in *)mask)->sin_addr.s_addr == 0))
			cp = "default";
		else if (flags & RTF_HOST)
			cp = routename(sin->sin_addr.s_addr);
		else if (mask)
			cp = netname(sin->sin_addr.s_addr,
			    ((struct sockaddr_in *)mask)->sin_addr.s_addr);
		else
			cp = netname(sin->sin_addr.s_addr, INADDR_ANY);
		break;
	    }

#ifdef INET6
	case AF_INET6:
	    {
		struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
#ifdef KAME_SCOPEID
		struct in6_addr *in6 = &sa6->sin6_addr;

		/*
		 * XXX: This is a special workaround for KAME kernels.
		 * sin6_scope_id field of SA should be set in the future.
		 */
		if (IN6_IS_ADDR_LINKLOCAL(in6) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(in6)) {
		    /* XXX: override is ok? */
		    sa6->sin6_scope_id = (u_int32_t)ntohs(*(u_short *)&in6->s6_addr[2]);
		    *(u_short *)&in6->s6_addr[2] = 0;
		}
#endif

		if (flags & RTF_HOST)
			cp = routename6(sa6);
		else if (mask) {
			cp = netname6(sa6,
				      &((struct sockaddr_in6 *)mask)->sin6_addr);
		} else
			cp = netname6(sa6, NULL);
		break;
	    }
#endif 

#ifndef SMALL
	case AF_APPLETALK:
	case 0:
	    {
		if (!(flags & RTF_HOST) && mask)
			cp = atalk_print2(sa,mask,11);
		else
			cp = atalk_print(sa,11);
		break;
	    }
	case AF_NS:
		cp = ns_print((struct sockaddr *)sa);
		break;
#endif

	case AF_LINK:
	    {
		struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;

		if (sdl->sdl_nlen == 0 && sdl->sdl_alen == 0 &&
		    sdl->sdl_slen == 0)
			(void)snprintf(workbuf, sizeof workbuf, "link#%d",
			    sdl->sdl_index);
		else switch (sdl->sdl_type) {
		case IFT_FDDI:
		case IFT_ETHER:
		    {
			int i;
			u_char *lla = (u_char *)sdl->sdl_data +
			    sdl->sdl_nlen;

			cplim = "";
			for (i = 0; i < sdl->sdl_alen; i++, lla++) {
				/* XXX */
				cp += sprintf(cp, "%s%02x", cplim, *lla);
				cplim = ":";
			}
			cp = workbuf;
			break;
		    }
		default:
			cp = link_ntoa(sdl);
			break;
		}
		break;
	    }

	default:
	    {
		u_char *s = (u_char *)sa->sa_data, *slim;

		slim =  sa->sa_len + (u_char *) sa;
		cplim = cp + sizeof(workbuf) - 6;
		cp += sprintf(cp, "(%d)", sa->sa_family);
		while (s < slim && cp < cplim) {
			cp += sprintf(cp, " %02x", *s++);
			if (s < slim)
			    cp += sprintf(cp, "%02x", *s++);
		}
		cp = workbuf;
	    }
	}
	if (width < 0 )
		printf("%s ", cp);
	else {
		if (nflag)
			printf("%-*s ", width, cp);
		else
			printf("%-*.*s ", width, width, cp);
	}
}

static void
p_flags(f)
	int f;
{
	char name[33], *flags;
	struct bits *p = bits;

	for (flags = name; p->b_mask && flags - name < sizeof(name); p++)
		if (p->b_mask & f)
			*flags++ = p->b_val;
	*flags = '\0';
	printf("%-6.6s ", name);
}

static struct sockaddr *sockcopy __P((struct sockaddr *,
    union sockaddr_union *));

/*
 * copy a sockaddr into an allocated region, allocate at least sockaddr
 * bytes and zero unused
 */
static struct sockaddr *
sockcopy(sp, dp)
	struct sockaddr *sp;
	union sockaddr_union *dp;
{
	int len;

	if (sp == 0 || sp->sa_len == 0)
		(void)memset(dp, 0, sizeof (*sp));
	else {
		len = (sp->sa_len >= sizeof (*sp)) ? sp->sa_len : sizeof (*sp);
		(void)memcpy(dp, sp, len);
	}
	return ((struct sockaddr *)dp);
}

static void
p_rtentry(rt)
	struct rtentry *rt;
{
	static struct ifnet ifnet, *lastif;
	union sockaddr_union addr_un, mask_un;
	struct sockaddr *addr, *mask;
	int af;

	if (Lflag && (rt->rt_flags & RTF_LLINFO))
		return;

	memset(&addr_un, 0, sizeof(addr_un));
	memset(&mask_un, 0, sizeof(mask_un));
	addr = sockcopy(kgetsa(rt_key(rt)), &addr_un);
	af = addr->sa_family;
	if (rt_mask(rt))
		mask = sockcopy(kgetsa(rt_mask(rt)), &mask_un);
	else
		mask = sockcopy(NULL, &mask_un);
	p_sockaddr(addr, mask, rt->rt_flags, WID_DST(af));
	p_sockaddr(kgetsa(rt->rt_gateway), NULL, RTF_HOST, WID_GW(af));
	p_flags(rt->rt_flags);
	printf("%6d %8lu ", rt->rt_refcnt, rt->rt_use);
	if (rt->rt_rmx.rmx_mtu)
		printf("%6lu", rt->rt_rmx.rmx_mtu); 
	else
		printf("%6s", "-");
	putchar((rt->rt_rmx.rmx_locks & RTV_MTU) ? 'L' : ' ');
	if (rt->rt_ifp) {
		if (rt->rt_ifp != lastif) {
			kget(rt->rt_ifp, ifnet);
			lastif = rt->rt_ifp;
		}
		printf(" %.16s%s", ifnet.if_xname,
			rt->rt_nodes[0].rn_dupedkey ? " =>" : "");
	}
	putchar('\n');
 	if (vflag) {
 		printf("\texpire   %10lu%c  recvpipe %10ld%c  "
		       "sendpipe %10ld%c\n",
 			rt->rt_rmx.rmx_expire, 
 			(rt->rt_rmx.rmx_locks & RTV_EXPIRE) ? 'L' : ' ',
 			rt->rt_rmx.rmx_recvpipe,
 			(rt->rt_rmx.rmx_locks & RTV_RPIPE) ? 'L' : ' ',
 			rt->rt_rmx.rmx_sendpipe,
 			(rt->rt_rmx.rmx_locks & RTV_SPIPE) ? 'L' : ' ');
 		printf("\tssthresh %10lu%c  rtt      %10ld%c  "
		       "rttvar   %10ld%c\n",
 			rt->rt_rmx.rmx_ssthresh, 
 			(rt->rt_rmx.rmx_locks & RTV_SSTHRESH) ? 'L' : ' ',
 			rt->rt_rmx.rmx_rtt, 
 			(rt->rt_rmx.rmx_locks & RTV_RTT) ? 'L' : ' ',
 			rt->rt_rmx.rmx_rttvar, 
			(rt->rt_rmx.rmx_locks & RTV_RTTVAR) ? 'L' : ' ');
 	}	

}

char *
routename(in)
	u_int32_t in;
{
	char *cp;
	static char line[MAXHOSTNAMELEN + 1];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0) {
			domain[sizeof(domain) - 1] = '\0';
			if ((cp = strchr(domain, '.')))
				(void)strcpy(domain, cp + 1);
			else
				domain[0] = 0;
		} else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag) {
		hp = gethostbyaddr((char *)&in, sizeof (struct in_addr),
			AF_INET);
		if (hp) {
			if ((cp = strchr(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = 0;
			cp = hp->h_name;
		}
	}
	if (cp) {
		strncpy(line, cp, sizeof(line) - 1);
		line[sizeof(line) - 1] = '\0';
	} else {
#define C(x)	((x) & 0xff)
		in = ntohl(in);
		snprintf(line, sizeof line, "%u.%u.%u.%u",
		    C(in >> 24), C(in >> 16), C(in >> 8), C(in));
	}
	return (line);
}

static u_long
forgemask(a)
	u_long a;
{
	u_long m;

	if (IN_CLASSA(a))
		m = IN_CLASSA_NET;
	else if (IN_CLASSB(a))
		m = IN_CLASSB_NET;
	else
		m = IN_CLASSC_NET;
	return (m);
}

static void
domask(dst, dlen, addr, mask)
	char *dst;
	size_t dlen;
	u_long addr, mask;
{
	int b, i;

	if (!mask || (forgemask(addr) == mask)) {
		*dst = '\0';
		return;
	}
	i = 0;
	for (b = 0; b < 32; b++)
		if (mask & (1 << b)) {
			int bb;

			i = b;
			for (bb = b+1; bb < 32; bb++)
				if (!(mask & (1 << bb))) {
					i = -1;	/* noncontig */
					break;
				}
			break;
		}
	if (i == -1)
		(void)snprintf(dst, dlen, "&0x%lx", mask);
	else
		(void)snprintf(dst, dlen, "/%d", 32-i);
}

/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *
netname(in, mask)
	u_int32_t in, mask;
{
	char *cp = 0;
	static char line[MAXHOSTNAMELEN + 4];
	struct netent *np = 0;
	u_int32_t net, omask;
	u_int32_t i;
	int subnetshift;

	i = ntohl(in);
	omask = mask = ntohl(mask);
	if (!nflag && i != INADDR_ANY) {
		if (mask == INADDR_ANY) {
			switch (mask = forgemask(i)) {
			case IN_CLASSA_NET:
				subnetshift = 8;
				break;
			case IN_CLASSB_NET:
				subnetshift = 8;
				break;
			case IN_CLASSC_NET:
				subnetshift = 4;
				break;
			default:
				abort();
			}
			/*
			 * If there are more bits than the standard mask
			 * would suggest, subnets must be in use.
			 * Guess at the subnet mask, assuming reasonable
			 * width subnet fields.
			 */
			while (i &~ mask)
				mask = (long)mask >> subnetshift;
		}
		net = i & mask;
		/*
		 * Note: shift the hosts bits out in octet units, since
		 * not all versions of getnetbyaddr() do this for us (e.g.
		 * the current `etc/networks' parser).
		 */
		while ((mask & 0xff) == 0)
			mask >>= 8, net >>= 8;
		np = getnetbyaddr(net, AF_INET);
		if (np)
			cp = np->n_name;
	}
	if (cp)
		strncpy(line, cp, sizeof(line) - 1);
	else if ((i & 0xffffff) == 0)
		(void)snprintf(line, sizeof line, "%u", C(i >> 24));
	else if ((i & 0xffff) == 0)
		(void)snprintf(line, sizeof line, "%u.%u", C(i >> 24)
		    , C(i >> 16));
	else if ((i & 0xff) == 0)
		(void)snprintf(line, sizeof line, "%u.%u.%u", C(i >> 24),
		    C(i >> 16), C(i >> 8));
	else
		(void)snprintf(line, sizeof line, "%u.%u.%u.%u", C(i >> 24),
			C(i >> 16), C(i >> 8), C(i));
	domask(line + strlen(line), sizeof(line) - strlen(line), i, omask);
	return (line);
}

#ifdef INET6
char *
netname6(sa6, mask)
	struct sockaddr_in6 *sa6;
	struct in6_addr *mask;
{
	static char line[NI_MAXHOST];
	u_char *p, *q;
	u_char *lim;
	int masklen, final = 0, illegal = 0;
#ifdef KAME_SCOPEID
	int flag = NI_WITHSCOPEID;
#else
	int flag = 0;
#endif
	int error;
	struct sockaddr_in6 sin6;

	sin6 = *sa6;
	if (mask) {
		masklen = 0;
		lim = (u_char *)mask + 16;
		for (p = (u_char *)mask, q = (u_char *)&sin6.sin6_addr;
		     p < lim;
		     p++, q++) {
			if (final && *p) {
				illegal++;
				*q = 0;
				continue;
			}

			switch (*p & 0xff) {
			 case 0xff:
				 masklen += 8;
				 break;
			 case 0xfe:
				 masklen += 7;
				 final++;
				 break;
			 case 0xfc:
				 masklen += 6;
				 final++;
				 break;
			 case 0xf8:
				 masklen += 5;
				 final++;
				 break;
			 case 0xf0:
				 masklen += 4;
				 final++;
				 break;
			 case 0xe0:
				 masklen += 3;
				 final++;
				 break;
			 case 0xc0:
				 masklen += 2;
				 final++;
				 break;
			 case 0x80:
				 masklen += 1;
				 final++;
				 break;
			 case 0x00:
				 final++;
				 break;
			 default:
				 final++;
				 illegal++;
				 break;
			}

			if (!illegal)
				*q &= *p;
			else
				*q = 0;
		}
	}
	else
		masklen = 128;

	if (masklen == 0 && IN6_IS_ADDR_UNSPECIFIED(&sa6->sin6_addr))
		return("default");

	if (illegal)
		fprintf(stderr, "illegal prefixlen\n");
	if (nflag)
		flag |= NI_NUMERICHOST;
	error = getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
			line, sizeof(line), NULL, 0, flag);
	if (error)
		strcpy(line, "invalid");

	if (nflag)
		sprintf(&line[strlen(line)], "/%d", masklen);

	return line;
}

char *
routename6(sa6)
	struct sockaddr_in6 *sa6;
{
	static char line[NI_MAXHOST];
#ifdef KAME_SCOPEID
	int flag = NI_WITHSCOPEID;
#else
	int flag = 0;
#endif
	/* use local variable for safety */
	struct sockaddr_in6 sa6_local;
	int error;

	memset(&sa6_local, 0, sizeof(sa6_local));
	sa6_local.sin6_family = AF_INET6;
	sa6_local.sin6_len = sizeof(struct sockaddr_in6);
	sa6_local.sin6_addr = sa6->sin6_addr;
	sa6_local.sin6_scope_id = sa6->sin6_scope_id;

	if (nflag)
		flag |= NI_NUMERICHOST;

	error = getnameinfo((struct sockaddr *)&sa6_local, sa6_local.sin6_len,
			line, sizeof(line), NULL, 0, flag);
	if (error)
		strcpy(line, "invalid");

	return line;
}
#endif /*INET6*/

/*
 * Print routing statistics
 */
void
rt_stats(off)
	u_long off;
{
	struct rtstat rtstat;

	if (off == 0) {
		printf("rtstat: symbol not in namelist\n");
		return;
	}
	kread(off, (char *)&rtstat, sizeof (rtstat));
	printf("routing:\n");
	printf("\t%u bad routing redirect%s\n",
		rtstat.rts_badredirect, plural(rtstat.rts_badredirect));
	printf("\t%u dynamically created route%s\n",
		rtstat.rts_dynamic, plural(rtstat.rts_dynamic));
	printf("\t%u new gateway%s due to redirects\n",
		rtstat.rts_newgateway, plural(rtstat.rts_newgateway));
	printf("\t%u destination%s found unreachable\n",
		rtstat.rts_unreach, plural(rtstat.rts_unreach));
	printf("\t%u use%s of a wildcard route\n",
		rtstat.rts_wildcard, plural(rtstat.rts_wildcard));
}
short ns_nullh[] = {0,0,0};
short ns_bh[] = {-1,-1,-1};

char *
ns_print(sa)
	struct sockaddr *sa;
{
	struct sockaddr_ns *sns = (struct sockaddr_ns*)sa;
	struct ns_addr work;
	union {
		union	ns_net net_e;
		u_long	long_e;
	} net;
	u_short port;
	static char mybuf[50], cport[10], chost[25];
	char *host = "";
	char *p;
	u_char *q;

	work = sns->sns_addr;
	port = ntohs(work.x_port);
	work.x_port = 0;
	net.net_e  = work.x_net;
	if (ns_nullhost(work) && net.long_e == 0) {
		if (port ) {
			(void)snprintf(mybuf, sizeof mybuf, "*.%xH", port);
			upHex(mybuf);
		} else
			(void)snprintf(mybuf, sizeof mybuf, "*.*");
		return (mybuf);
	}

	if (memcmp(ns_bh, work.x_host.c_host, 6) == 0) {
		host = "any";
	} else if (memcmp(ns_nullh, work.x_host.c_host, 6) == 0) {
		host = "*";
	} else {
		q = work.x_host.c_host;
		(void)snprintf(chost, sizeof chost, "%02x%02x%02x%02x%02x%02xH",
			q[0], q[1], q[2], q[3], q[4], q[5]);
		for (p = chost; *p == '0' && p < chost + 12; p++)
			continue;
		host = p;
	}
	if (port)
		(void)snprintf(cport, sizeof cport, ".%xH", htons(port));
	else
		*cport = 0;

	(void)snprintf(mybuf, sizeof mybuf, "%xH.%s%s", (int)ntohl(net.long_e),
	    host, cport);
	upHex(mybuf);
	return (mybuf);
}

char *
ns_phost(sa)
	struct sockaddr *sa;
{
	struct sockaddr_ns *sns = (struct sockaddr_ns *)sa;
	struct sockaddr_ns work;
	static union ns_net ns_zeronet;
	char *p;

	work = *sns;
	work.sns_addr.x_port = 0;
	work.sns_addr.x_net = ns_zeronet;

	p = ns_print((struct sockaddr *)&work);
	if (strncmp("0H.", p, 3) == 0)
		p += 3;
	return (p);
}

void
upHex(p0)
	char *p0;
{
	char *p = p0;

	for (; *p; p++)
		switch (*p) {
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
			*p += ('A' - 'a');
		}
}
