/*	$NetBSD: route6d.c,v 1.13.4.7 2004/04/12 05:04:28 jmc Exp $	*/
/*	$KAME: route6d.c,v 1.36 2000/10/05 22:20:39 itojun Exp $	*/

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

#include <sys/cdefs.h>
#ifndef	lint
__RCSID("$NetBSD: route6d.c,v 1.13.4.7 2004/04/12 05:04:28 jmc Exp $");
#endif

#include <stdio.h>

#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <syslog.h>
#include <stddef.h>
#include <errno.h>
#include <err.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <net/if.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif /* __FreeBSD__ >= 3 */
#define	KERNEL	1
#define	_KERNEL	1
#include <net/route.h>
#undef KERNEL
#undef _KERNEL
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netdb.h>
#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif

#include <arpa/inet.h>

#include "route6d.h"

#define	MAXFILTER	40

#ifdef	DEBUG
#define	INIT_INTERVAL6	6
#else
#define	INIT_INTERVAL6	10	/* Wait to submit a initial riprequest */
#endif

/* alignment constraint for routing socket */
#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

/*
 * Following two macros are highly depending on KAME Release
 */
#define	IN6_LINKLOCAL_IFINDEX(addr) \
	((addr).s6_addr[2] << 8 | (addr).s6_addr[3])

#define	SET_IN6_LINKLOCAL_IFINDEX(addr, index) \
	do { \
		(addr).s6_addr[2] = ((index) >> 8) & 0xff; \
		(addr).s6_addr[3] = (index) & 0xff; \
	} while (0)

struct	ifc {			/* Configuration of an interface */
	char	*ifc_name;			/* if name */
	struct	ifc *ifc_next;
	int	ifc_index;			/* if index */
	int	ifc_mtu;			/* if mtu */
	int	ifc_metric;			/* if metric */
	short	ifc_flags;			/* flags */
	struct	in6_addr ifc_mylladdr;		/* my link-local address */
	struct	sockaddr_in6 ifc_ripsin;	/* rip multicast address */
	struct	iff *ifc_filter;		/* filter structure */
	struct	ifac *ifc_addr;			/* list of AF_INET6 addresses */
	int	ifc_joined;			/* joined to ff02::9 */
};

struct	ifac {			/* Adddress associated to an interface */ 
	struct	ifc *ifa_conf;		/* back pointer */
	struct	ifac *ifa_next;
	struct	in6_addr ifa_addr;	/* address */
	struct	in6_addr ifa_raddr;	/* remote address, valid in p2p */
	int	ifa_plen;		/* prefix length */
};

struct	iff {
	int	iff_type;
	struct	in6_addr iff_addr;
	int	iff_plen;
	struct	iff *iff_next;
};

struct	ifc *ifc;
int	nifc;		/* number of valid ifc's */
struct	ifc **index2ifc;
int	nindex2ifc;
struct	ifc *loopifcp = NULL;	/* pointing to loopback */
int	loopifindex = 0;	/* ditto */
fd_set	sockvec;	/* vector to select() for receiving */
int	rtsock;		/* the routing socket */
int	ripsock;	/* socket to send/receive RIP datagram */

struct	rip6 *ripbuf;	/* packet buffer for sending */

/*
 * Maintain the routes in a linked list. When the number of the routes
 * grows, somebody would like to introduce a hash based or a radix tree
 * based strucutre. I believe the number of routes handled by RIP is
 * limited and I don't have to manage a complex data structure, however.
 *
 * One of the major drawbacks of the linear linked list is the difficulty
 * of representing the relationship between a couple of routes. This may
 * be a significant problem when we have to support route aggregation with
 * supressing the specifices covered by the aggregate.
 */

struct	riprt {
	struct	riprt *rrt_next;	/* next destination */
	struct	riprt *rrt_same;	/* same destination - future use */
	struct	netinfo6 rrt_info;	/* network info */
	struct	in6_addr rrt_gw;	/* gateway */
	u_long	rrt_flags;		/* kernel routing table flags */
	u_long	rrt_rflags;		/* route6d routing table flags */
	time_t	rrt_t;			/* when the route validated */
	int	rrt_index;		/* ifindex from which this route got */
};

struct	riprt *riprt = 0;

int	dflag = 0;	/* debug flag */
int	qflag = 0;	/* quiet flag */
int	nflag = 0;	/* don't update kernel routing table */
int	aflag = 0;	/* age out even the statically defined routes */
int	hflag = 0;	/* don't split horizon */
int	lflag = 0;	/* exchange site local routes */
int	sflag = 0;	/* announce static routes w/ split horizon */
int	Sflag = 0;	/* announce static routes to every interface */
unsigned long routetag = 0;	/* route tag attached on originating case */

char	*filter[MAXFILTER];
int	filtertype[MAXFILTER];
int	nfilter = 0;

pid_t	pid;

struct	sockaddr_storage ripsin;

struct	rtentry rtentry;

int	interval = 1;
time_t	nextalarm = 0;
time_t	sup_trig_update = 0;

FILE	*rtlog = NULL;

int logopened = 0;

static	u_long	seq = 0;

#define	RRTF_AGGREGATE		0x08000000
#define	RRTF_NOADVERTISE	0x10000000
#define	RRTF_NH_NOT_LLADDR	0x20000000
#define RRTF_SENDANYWAY		0x40000000
#define	RRTF_CHANGED		0x80000000

int main __P((int, char **));
void ripalarm __P((int));
void riprecv __P((void));
void ripsend __P((struct ifc *, struct sockaddr_in6 *, int));
void init __P((void));
void sockopt __P((struct ifc *));
void ifconfig __P((void));
void ifconfig1 __P((const char *, const struct sockaddr *, struct ifc *, int));
void rtrecv __P((void));
int rt_del __P((const struct sockaddr_in6 *, const struct sockaddr_in6 *,
	const struct sockaddr_in6 *));
int rt_deladdr __P((struct ifc *, const struct sockaddr_in6 *,
	const struct sockaddr_in6 *));
void filterconfig __P((void));
int getifmtu __P((int));
const char *rttypes __P((struct rt_msghdr *rtm));
const char *rtflags __P((struct rt_msghdr *rtm));
const char *ifflags __P((int flags));
void ifrt __P((struct ifc *, int));
void ifrt_p2p __P((struct ifc *, int));
void applymask __P((struct in6_addr *, struct in6_addr *));
void applyplen __P((struct in6_addr *, int));
void ifrtdump __P((int));
void ifdump __P((int));
void ifdump0 __P((FILE *, const struct ifc *));
void rtdump __P((int));
void rt_entry __P((struct rt_msghdr *, int));
void rtdexit __P((int));
void riprequest __P((struct ifc *, struct netinfo6 *, int, struct sockaddr_in6 *));
void ripflush __P((struct ifc *, struct sockaddr_in6 *));
void sendrequest __P((struct ifc *));
int mask2len __P((const struct in6_addr *, int));
int sendpacket __P((struct sockaddr_in6 *, int));
int addroute __P((struct riprt *, const struct in6_addr *, struct ifc *));
int delroute __P((struct netinfo6 *, struct in6_addr *));
struct in6_addr *getroute __P((struct netinfo6 *, struct in6_addr *));
void krtread __P((int));
int tobeadv __P((struct riprt *, struct ifc *));
char *allocopy __P((char *));
char *hms __P((void));
const char *inet6_n2p __P((const struct in6_addr *));
struct ifac *ifa_match __P((const struct ifc *, const struct in6_addr *, int));
struct in6_addr *plen2mask __P((int));
struct riprt *rtsearch __P((struct netinfo6 *));
int ripinterval __P((int));
time_t ripsuptrig __P((void));
void fatal __P((const char *, ...))
	__attribute__((__format__(__printf__, 1, 2)));
void trace __P((int, const char *, ...))
	__attribute__((__format__(__printf__, 2, 3)));
void tracet __P((int, const char *, ...))
	__attribute__((__format__(__printf__, 2, 3)));
unsigned int if_maxindex __P((void));
struct ifc *ifc_find __P((char *));
struct iff *iff_find __P((struct ifc *, int));
void setindex2ifc __P((int, struct ifc *));

#define	MALLOC(type)	((type *)malloc(sizeof(type)))

int
main(argc, argv)
	int	argc;
	char	**argv;
{
	int	ch;
	int	error = 0;
	struct	ifc *ifcp;
	sigset_t mask, omask;
	FILE	*pidfile;
	char *progname;
	char *ep;

	progname = strrchr(*argv, '/');
	if (progname)
		progname++;
	else
		progname = *argv;

	pid = getpid();
	while ((ch = getopt(argc, argv, "A:N:O:R:T:L:t:adDhlnqsS")) != EOF) {
		switch (ch) {
		case 'A':
		case 'N':
		case 'O':
		case 'T':
		case 'L':
			if (nfilter >= MAXFILTER) {
				fatal("Exceeds MAXFILTER");
				/*NOTREACHED*/
			}
			filtertype[nfilter] = ch;
			filter[nfilter++] = allocopy(optarg);
			break;
		case 't':
			ep = NULL;
			routetag = strtoul(optarg, &ep, 0);
			if (!ep || *ep != '\0' || (routetag & ~0xffff) != 0) {
				fatal("invalid route tag");
				/*NOTREACHED*/
			}
			break;
		case 'R':
			if ((rtlog = fopen(optarg, "w")) == NULL) {
				fatal("Can not write to routelog");
				/*NOTREACHED*/
			}
			break;
#define	FLAG(c, flag, n)	case c: do { flag = n; break; } while(0)
		FLAG('a', aflag, 1); break;
		FLAG('d', dflag, 1); break;
		FLAG('D', dflag, 2); break;
		FLAG('h', hflag, 1); break;
		FLAG('l', lflag, 1); break;
		FLAG('n', nflag, 1); break;
		FLAG('q', qflag, 1); break;
		FLAG('s', sflag, 1); break;
		FLAG('S', Sflag, 1); break;
#undef	FLAG
		default:
			fatal("Invalid option specified, terminating");
			/*NOTREACHED*/
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0)
		fatal("bogus extra arguments");

	if (geteuid()) {
		nflag = 1;
		fprintf(stderr, "No kernel update is allowed\n");
	}
	openlog(progname, LOG_NDELAY|LOG_PID, LOG_DAEMON);
	logopened++;
	init();
	ifconfig();
	for (ifcp = ifc; ifcp; ifcp = ifcp->ifc_next) {
		if (ifcp->ifc_index < 0) {
			fprintf(stderr,
"No ifindex found at %s (no link-local address?)\n",
				ifcp->ifc_name);
			error++;
		}
	}
	if (error)
		exit(1);
	if (loopifcp == NULL)
		fatal("No loopback found");
	loopifindex = loopifcp->ifc_index;
	for (ifcp = ifc; ifcp; ifcp = ifcp->ifc_next)
		ifrt(ifcp, 0);
	filterconfig();
	krtread(0);
	if (dflag)
		ifrtdump(0);

	if (dflag == 0) {
#if 1
		if (daemon(0, 0) < 0)
			fatal("daemon");
#else
		if (fork())
			exit(0);
		if (setsid() < 0)
			fatal("setid");
#endif
	}
	pid = getpid();
	if ((pidfile = fopen(ROUTE6D_PID, "w")) != NULL) {
		fprintf(pidfile, "%d\n", pid);
		fclose(pidfile);
	}

	if ((ripbuf = (struct rip6 *)malloc(RIP6_MAXMTU)) == NULL)
		fatal("malloc");
	memset(ripbuf, 0, RIP6_MAXMTU);
	ripbuf->rip6_cmd = RIP6_RESPONSE;
	ripbuf->rip6_vers = RIP6_VERSION;
	ripbuf->rip6_res1[0] = 0;
	ripbuf->rip6_res1[1] = 0;

	if (signal(SIGALRM, ripalarm) == SIG_ERR)
		fatal("signal: SIGALRM");
	if (signal(SIGQUIT, rtdexit) == SIG_ERR)
		fatal("signal: SIGQUIT");
	if (signal(SIGTERM, rtdexit) == SIG_ERR)
		fatal("signal: SIGTERM");
	if (signal(SIGUSR1, ifrtdump) == SIG_ERR)
		fatal("signal: SIGUSR1");
	if (signal(SIGHUP, ifrtdump) == SIG_ERR)
		fatal("signal: SIGHUP");
	if (signal(SIGINT, ifrtdump) == SIG_ERR)
		fatal("signal: SIGINT");
	/*
	 * To avoid rip packet congestion (not on a cable but in this
	 * process), wait for a moment to send the first RIP6_RESPONSE
	 * packets.
	 */
	alarm(ripinterval(INIT_INTERVAL6));

	for (ifcp = ifc; ifcp; ifcp = ifcp->ifc_next) {
		if (ifcp->ifc_index > 0 && (ifcp->ifc_flags & IFF_UP))
			sendrequest(ifcp);
	}

	syslog(LOG_INFO, "**** Started ****");
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	while (1) {
		fd_set	recvec;

		FD_COPY(&sockvec, &recvec);
		switch (select(FD_SETSIZE, &recvec, 0, 0, 0)) {
		case -1:
			if (errno == EINTR)
				continue;
			fatal("select");
		case 0:
			continue;
		default:
			if (FD_ISSET(ripsock, &recvec)) {
				sigprocmask(SIG_BLOCK, &mask, &omask);
				riprecv();
				sigprocmask(SIG_SETMASK, &omask, NULL);
			}
			if (FD_ISSET(rtsock, &recvec)) {
				sigprocmask(SIG_BLOCK, &mask, &omask);
				rtrecv();
				sigprocmask(SIG_SETMASK, &omask, NULL);
			}
		}
	}
}

/*
 * gracefully exits after resetting sockopts.
 */
/* ARGSUSED */
void
rtdexit(sig)
	int sig;
{
	struct	riprt *rrt;

	alarm(0);
	for (rrt = riprt; rrt; rrt = rrt->rrt_next) {
		if (rrt->rrt_rflags & RRTF_AGGREGATE) {
			delroute(&rrt->rrt_info, &rrt->rrt_gw);
		}
	}
	close(ripsock);
	close(rtsock);
	syslog(LOG_INFO, "**** Terminated ****");
	closelog();
	exit(1);
}

/*
 * Called periodically:
 *	1. age out the learned route. remove it if necessary.
 *	2. submit RIP6_RESPONSE packets.
 * Invoked in every SUPPLY_INTERVAL6 (30) seconds. I believe we don't have
 * to invoke this function in every 1 or 5 or 10 seconds only to age the
 * routes more precisely.
 */
/* ARGSUSED */
void
ripalarm(sig)
	int sig;
{
	struct	ifc *ifcp;
	struct	riprt *rrt, *rrt_prev, *rrt_next;
	time_t	t_lifetime, t_holddown;

	/* age the RIP routes */
	rrt_prev = 0;
	t_lifetime = time(NULL) - RIP_LIFETIME;
	t_holddown = t_lifetime - RIP_HOLDDOWN;
	for (rrt = riprt; rrt; rrt = rrt_next) {
		rrt_next = rrt->rrt_next;

		if (rrt->rrt_t == 0) {
			rrt_prev = rrt;
			continue;
		}
		if (rrt->rrt_t < t_holddown) {
			if (rrt_prev) {
				rrt_prev->rrt_next = rrt->rrt_next;
			} else {
				riprt = rrt->rrt_next;
			}
			delroute(&rrt->rrt_info, &rrt->rrt_gw);
			free(rrt);
			continue;
		}
		if (rrt->rrt_t < t_lifetime)
			rrt->rrt_info.rip6_metric = HOPCNT_INFINITY6;
		rrt_prev = rrt;
	}
	/* Supply updates */
	for (ifcp = ifc; ifcp; ifcp = ifcp->ifc_next) {
		if (ifcp->ifc_index > 0 && (ifcp->ifc_flags & IFF_UP))
			ripsend(ifcp, &ifcp->ifc_ripsin, 0);
	}
	alarm(ripinterval(SUPPLY_INTERVAL6));
}

void
init()
{
	int	i, int0, int255, error;
	struct	addrinfo hints, *res;
	char	port[10];

	ifc = (struct ifc *)NULL;
	nifc = 0;
	nindex2ifc = 0;	/*initial guess*/
	index2ifc = NULL;
	snprintf(port, sizeof(port), "%d", RIP6_PORT);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(NULL, port, &hints, &res);
	if (error)
		fatal("%s", gai_strerror(error));
	if (res->ai_next)
		fatal(":: resolved to multiple address");

	int0 = 0; int255 = 255;
	ripsock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (ripsock < 0)
		fatal("rip socket");
	if (bind(ripsock, res->ai_addr, res->ai_addrlen) < 0)
		fatal("rip bind");
	if (setsockopt(ripsock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
		&int255, sizeof(int255)) < 0)
		fatal("rip IPV6_MULTICAST_HOPS");
	if (setsockopt(ripsock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
		&int0, sizeof(int0)) < 0)
		fatal("rip IPV6_MULTICAST_LOOP");

	i = 1;
#ifdef IPV6_RECVPKTINFO
	if (setsockopt(ripsock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &i,
		       sizeof(i)) < 0)
		fatal("rip IPV6_RECVPKTINFO");
#else  /* old adv. API */
	if (setsockopt(ripsock, IPPROTO_IPV6, IPV6_PKTINFO, &i,
		       sizeof(i)) < 0)
		fatal("rip IPV6_PKTINFO");
#endif 

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	error = getaddrinfo(RIP6_DEST, port, &hints, &res);
	if (error)
		fatal("%s", gai_strerror(error));
	if (res->ai_next)
		fatal("%s resolved to multiple address", RIP6_DEST);
	memcpy(&ripsin, res->ai_addr, res->ai_addrlen);

#ifdef FD_ZERO
	FD_ZERO(&sockvec);
#else
	memset(&sockvec, 0, sizeof(sockvec));
#endif
	FD_SET(ripsock, &sockvec);

	if (nflag == 0) {
		if ((rtsock = socket(PF_ROUTE, SOCK_RAW, 0)) < 0)
			fatal("route socket");
		FD_SET(rtsock, &sockvec);
	} else
		rtsock = -1;	/*just for safety */
}

#define	RIPSIZE(n) \
	(sizeof(struct rip6) + ((n)-1) * sizeof(struct netinfo6))

/*
 * ripflush flushes the rip datagram stored in the rip buffer
 */
static int nrt;
static struct netinfo6 *np;

void
ripflush(ifcp, sin)
	struct ifc *ifcp;
	struct sockaddr_in6 *sin;
{
	int i;
	int error;

	if (ifcp)
		tracet(1, "Send(%s): info(%d) to %s.%d\n",
			ifcp->ifc_name, nrt,
			inet6_n2p(&sin->sin6_addr), ntohs(sin->sin6_port));
	else
		tracet(1, "Send: info(%d) to %s.%d\n",
			nrt, inet6_n2p(&sin->sin6_addr), ntohs(sin->sin6_port));
	if (dflag >= 2) {
		np = ripbuf->rip6_nets;
		for (i = 0; i < nrt; i++, np++) {
			if (np->rip6_metric == NEXTHOP_METRIC) {
				if (IN6_IS_ADDR_UNSPECIFIED(&np->rip6_dest))
					trace(2, "    NextHop reset");
				else {
					trace(2, "    NextHop %s",
						inet6_n2p(&np->rip6_dest));
				}
			} else {
				trace(2, "    %s/%d[%d]",
					inet6_n2p(&np->rip6_dest),
					np->rip6_plen, np->rip6_metric);
			}
			if (np->rip6_tag) {
				trace(2, "  tag=0x%04x",
					ntohs(np->rip6_tag) & 0xffff);
			}
			trace(2, "\n");
		}
	}
	error = sendpacket(sin, RIPSIZE(nrt));
	if (error == EAFNOSUPPORT) {
		/* Protocol not supported */
		tracet(1, "Could not send info to %s (%s): "
			"set IFF_UP to 0\n",
			ifcp->ifc_name, inet6_n2p(&ifcp->ifc_ripsin.sin6_addr));
		ifcp->ifc_flags &= ~IFF_UP;	/* As if down for AF_INET6 */
	}
	nrt = 0; np = ripbuf->rip6_nets;
}

/*
 * Generate RIP6_RESPONSE packets and send them.
 */
void
ripsend(ifcp, sin, flag)
	struct	ifc *ifcp;
	struct	sockaddr_in6 *sin;
	int flag;
{
	struct	riprt *rrt;
	struct	in6_addr *nh;	/* next hop */
	struct	in6_addr ia;
	struct	iff *iffp;
	int	maxrte, ok;

	if (ifcp == NULL) {
		/*
		 * Request from non-link local address is not
		 * a regular route6d update.
		 */
		maxrte = (IFMINMTU - sizeof(struct ip6_hdr) - 
				sizeof(struct udphdr) - 
				sizeof(struct rip6) + sizeof(struct netinfo6)) /
				sizeof(struct netinfo6);
		nrt = 0; np = ripbuf->rip6_nets; nh = NULL;
		for (rrt = riprt; rrt; rrt = rrt->rrt_next) {
			if (rrt->rrt_rflags & RRTF_NOADVERTISE)
				continue;
			/* Put the route to the buffer */
			*np = rrt->rrt_info;
			np++; nrt++;
			if (nrt == maxrte) {
				ripflush(NULL, sin);
				nh = NULL;
			}
		}
		if (nrt)	/* Send last packet */
			ripflush(NULL, sin);
		return;
	}

	if ((flag & RRTF_SENDANYWAY) == 0 &&
	    (qflag || (ifcp->ifc_flags & IFF_LOOPBACK)))
		return;
	if (iff_find(ifcp, 'N') != NULL)
		return;
	if (iff_find(ifcp, 'T') != NULL) {
		struct netinfo6 rrt_info;
		memset(&rrt_info, 0, sizeof(struct netinfo6));
		rrt_info.rip6_dest = in6addr_any;
		rrt_info.rip6_plen = 0;
		rrt_info.rip6_metric = 1;
		rrt_info.rip6_tag = htons(routetag & 0xffff);
		np = ripbuf->rip6_nets;
		*np = rrt_info;
		nrt = 1;
		ripflush(ifcp, sin);
		return;
	}
	maxrte = (ifcp->ifc_mtu - sizeof(struct ip6_hdr) - 
			sizeof(struct udphdr) - 
			sizeof(struct rip6) + sizeof(struct netinfo6)) /
			sizeof(struct netinfo6);
	nrt = 0; np = ripbuf->rip6_nets; nh = NULL;
	for (rrt = riprt; rrt; rrt = rrt->rrt_next) {
		if (rrt->rrt_rflags & RRTF_NOADVERTISE)
			continue;
		/* Need to check filer here */
		ok = 1;
		for (iffp = ifcp->ifc_filter; iffp; iffp = iffp->iff_next) {
			if (iffp->iff_type != 'A')
				continue;
			if (rrt->rrt_info.rip6_plen <= iffp->iff_plen)
				continue;
			ia = rrt->rrt_info.rip6_dest; 
			applyplen(&ia, iffp->iff_plen);
			if (IN6_ARE_ADDR_EQUAL(&ia, &iffp->iff_addr)) {
				ok = 0;
				break;
			}
		}
		if (!ok)
			continue;
		for (iffp = ifcp->ifc_filter; iffp; iffp = iffp->iff_next) {
			if (iffp->iff_type != 'O')
				continue;
			ok = 0;
			if (rrt->rrt_info.rip6_plen < iffp->iff_plen)
				continue;
			ia = rrt->rrt_info.rip6_dest; 
			applyplen(&ia, iffp->iff_plen);
			if (IN6_ARE_ADDR_EQUAL(&ia, &iffp->iff_addr)) {
				ok = 1;
				break;
			}
		}
		if (!ok)
			continue;
		/* Check split horizon and other conditions */
		if (tobeadv(rrt, ifcp) == 0)
			continue;
		/* Only considers the routes with flag if specified */
		if ((flag & RRTF_CHANGED) &&
		    (rrt->rrt_rflags & RRTF_CHANGED) == 0)
			continue;
		/* Check nexthop */
		if (rrt->rrt_index == ifcp->ifc_index &&
		    !IN6_IS_ADDR_UNSPECIFIED(&rrt->rrt_gw) &&
		    (rrt->rrt_rflags & RRTF_NH_NOT_LLADDR) == 0) {
			if (nh == NULL || !IN6_ARE_ADDR_EQUAL(nh, &rrt->rrt_gw)) {
				if (nrt == maxrte - 2)
					ripflush(ifcp, sin);
				np->rip6_dest = rrt->rrt_gw;
				if (IN6_IS_ADDR_LINKLOCAL(&np->rip6_dest))
					SET_IN6_LINKLOCAL_IFINDEX(np->rip6_dest, 0);
				np->rip6_plen = 0;
				np->rip6_tag = 0;
				np->rip6_metric = NEXTHOP_METRIC;
				nh = &rrt->rrt_gw;
				np++; nrt++;
			}
		} else if (nh && (rrt->rrt_index != ifcp->ifc_index ||
			          !IN6_ARE_ADDR_EQUAL(nh, &rrt->rrt_gw) ||
				  rrt->rrt_rflags & RRTF_NH_NOT_LLADDR)) {
			/* Reset nexthop */
			if (nrt == maxrte - 2)
				ripflush(ifcp, sin);
			memset(np, 0, sizeof(struct netinfo6));
			np->rip6_metric = NEXTHOP_METRIC;
			nh = NULL;
			np++; nrt++;
		}
		/* Put the route to the buffer */
		*np = rrt->rrt_info;
		np++; nrt++;
		if (nrt == maxrte) {
			ripflush(ifcp, sin);
			nh = NULL;
		}
	}
	if (nrt)	/* Send last packet */
		ripflush(ifcp, sin);
}

/*
 * Determine if the route is to be advertised on the specified interface.
 * It checks options specified in the arguments and the split horizon rule.
 */
int
tobeadv(rrt, ifcp)
	struct riprt *rrt;
	struct ifc *ifcp;
{

	/* Special care for static routes */
	if (rrt->rrt_flags & RTF_STATIC) {
		/* XXX don't advertise reject/blackhole routes */
		if (rrt->rrt_flags & (RTF_REJECT | RTF_BLACKHOLE))
			return 0;

		if (Sflag)	/* Yes, advertise it anyway */
			return 1;
		if (sflag && rrt->rrt_index != ifcp->ifc_index)
			return 1;
		return 0;
	}
	/* Regular split horizon */
	if (hflag == 0 && rrt->rrt_index == ifcp->ifc_index)
		return 0;
	return 1;
}

/*
 * Send a rip packet actually.
 */
int
sendpacket(sin, len)
	struct	sockaddr_in6 *sin;
	int	len;
{
	/*
	 * MSG_DONTROUTE should not be specified when it responds with a
	 * RIP6_REQUEST message. SO_DONTROUTE has been specified to
	 * other sockets.
	 */
	struct msghdr m;
	struct cmsghdr *cm;
	struct iovec iov[2];
	u_char cmsgbuf[256];
	struct in6_pktinfo *pi;
	int index;
	struct sockaddr_in6 sincopy;

	/* do not overwrite the given sin */
	sincopy = *sin;
	sin = &sincopy;

	if (IN6_IS_ADDR_LINKLOCAL(&sin->sin6_addr)
	 || IN6_IS_ADDR_MULTICAST(&sin->sin6_addr)) {
		index = IN6_LINKLOCAL_IFINDEX(sin->sin6_addr);
		SET_IN6_LINKLOCAL_IFINDEX(sin->sin6_addr, 0);
	} else
		index = 0;

	m.msg_name = (caddr_t)sin;
	m.msg_namelen = sizeof(*sin);
	iov[0].iov_base = (caddr_t)ripbuf;
	iov[0].iov_len = len;
	m.msg_iov = iov;
	m.msg_iovlen = 1;
	if (!index) {
		m.msg_control = NULL;
		m.msg_controllen = 0;
	} else {
		memset(cmsgbuf, 0, sizeof(cmsgbuf));
		cm = (struct cmsghdr *)cmsgbuf;
		m.msg_control = (caddr_t)cm;
		m.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));

		cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
		cm->cmsg_level = IPPROTO_IPV6;
		cm->cmsg_type = IPV6_PKTINFO;
		pi = (struct in6_pktinfo *)CMSG_DATA(cm);
		memset(&pi->ipi6_addr, 0, sizeof(pi->ipi6_addr)); /*::*/
		pi->ipi6_ifindex = index;
	}

	if (sendmsg(ripsock, &m, 0 /*MSG_DONTROUTE*/) < 0) {
		trace(1, "sendmsg: %s\n", strerror(errno));
		return errno;
	}

	return 0;
}

/*
 * Receive and process RIP packets. Update the routes/kernel forwarding
 * table if necessary.
 */
void
riprecv()
{
	struct	ifc *ifcp, *ic;
	struct	sockaddr_in6 fsock;
	struct	in6_addr nh;	/* next hop */
	struct	rip6 *rp;
	struct	netinfo6 *np, *nq;
	struct	riprt *rrt;
	int	len, nn, need_trigger, index;
	char	buf[4 * RIP6_MAXMTU];
	time_t	t;
	struct msghdr m;
	struct cmsghdr *cm;
	struct iovec iov[2];
	u_char cmsgbuf[256];
	struct in6_pktinfo *pi;
	struct iff *iffp;
	struct in6_addr ia;
	int ok;

	need_trigger = 0;

	m.msg_name = (caddr_t)&fsock;
	m.msg_namelen = sizeof(fsock);
	iov[0].iov_base = (caddr_t)buf;
	iov[0].iov_len = sizeof(buf);
	m.msg_iov = iov;
	m.msg_iovlen = 1;
	cm = (struct cmsghdr *)cmsgbuf;
	m.msg_control = (caddr_t)cm;
	m.msg_controllen = sizeof(cmsgbuf);
	if ((len = recvmsg(ripsock, &m, 0)) < 0)
		fatal("recvmsg");
	index = 0;
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&m);
	     cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(&m, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6
		 && cm->cmsg_type == IPV6_PKTINFO) {
			pi = (struct in6_pktinfo *)(CMSG_DATA(cm));
			index = pi->ipi6_ifindex;
			break;
		}
	}
	if (index && IN6_IS_ADDR_LINKLOCAL(&fsock.sin6_addr))
		SET_IN6_LINKLOCAL_IFINDEX(fsock.sin6_addr, index);

	nh = fsock.sin6_addr;
	nn = (len - sizeof(struct rip6) + sizeof(struct netinfo6)) /
		sizeof(struct netinfo6);
	rp = (struct rip6 *)buf;
	np = rp->rip6_nets;

	if (rp->rip6_vers !=  RIP6_VERSION) {
		trace(1, "Incorrect RIP version %d\n", rp->rip6_vers);
		return;
	}
	if (rp->rip6_cmd == RIP6_REQUEST) {
		if (index && index < nindex2ifc) {
			ifcp = index2ifc[index];
			riprequest(ifcp, np, nn, &fsock);
		} else {
			riprequest(NULL, np, nn, &fsock);
		}
		return; 
	} 

	if (!IN6_IS_ADDR_LINKLOCAL(&fsock.sin6_addr)) {
		trace(1, "Packets from non-ll addr: %s\n",
			inet6_n2p(&fsock.sin6_addr));
		return;		/* Ignore packets from non-link-local addr */
	}
	index = IN6_LINKLOCAL_IFINDEX(fsock.sin6_addr);
	ifcp = (index < nindex2ifc) ? index2ifc[index] : NULL;
	if (!ifcp) {
		trace(1, "Packets to unknown interface index %d\n", index);
		return;		/* Ignore it */
	}
	if (IN6_ARE_ADDR_EQUAL(&ifcp->ifc_mylladdr, &fsock.sin6_addr))
		return;		/* The packet is from me; ignore */
	if (rp->rip6_cmd != RIP6_RESPONSE) {
		trace(1, "Invalid command %d\n", rp->rip6_cmd);
		return; 
	}
	if (iff_find(ifcp, 'N') != NULL)
		return;
	tracet(1, "Recv(%s): from %s.%d info(%d)\n",
		ifcp->ifc_name, inet6_n2p(&nh), ntohs(fsock.sin6_port), nn);

	t = time(NULL);
	for (; nn; nn--, np++) {
		if (np->rip6_metric == NEXTHOP_METRIC) {
			/* modify neighbor address */
			if (IN6_IS_ADDR_LINKLOCAL(&np->rip6_dest)) {
				nh = np->rip6_dest;
				SET_IN6_LINKLOCAL_IFINDEX(nh, index);
				trace(1, "\tNexthop: %s\n", inet6_n2p(&nh));
			} else if (IN6_IS_ADDR_UNSPECIFIED(&np->rip6_dest)) {
				nh = fsock.sin6_addr;
				trace(1, "\tNexthop: %s\n", inet6_n2p(&nh));
			} else {
				nh = fsock.sin6_addr;
				trace(1, "\tInvalid Nexthop: %s\n",
					inet6_n2p(&np->rip6_dest));
			}
			continue;
		}
		if (IN6_IS_ADDR_MULTICAST(&np->rip6_dest)) {
			trace(1, "\tMulticast netinfo6: %s/%d [%d]\n",
				inet6_n2p(&np->rip6_dest),
				np->rip6_plen, np->rip6_metric);
			continue;
		}
		if (IN6_IS_ADDR_LOOPBACK(&np->rip6_dest)) {
			trace(1, "\tLoopback netinfo6: %s/%d [%d]\n",
				inet6_n2p(&np->rip6_dest),
				np->rip6_plen, np->rip6_metric);
			continue;
		}
		if (IN6_IS_ADDR_LINKLOCAL(&np->rip6_dest)) {
			trace(1, "\tLink Local netinfo6: %s/%d [%d]\n",
				inet6_n2p(&np->rip6_dest),
				np->rip6_plen, np->rip6_metric);
			continue;
		}
		/* may need to pass sitelocal prefix in some case, however*/
		if (IN6_IS_ADDR_SITELOCAL(&np->rip6_dest) && !lflag) {
			trace(1, "\tSite Local netinfo6: %s/%d [%d]\n",
				inet6_n2p(&np->rip6_dest),
				np->rip6_plen, np->rip6_metric);
			continue;
		}
		trace(2, "\tnetinfo6: %s/%d [%d]",
			inet6_n2p(&np->rip6_dest),
			np->rip6_plen, np->rip6_metric);
		if (np->rip6_tag)
			trace(2, "  tag=0x%04x", ntohs(np->rip6_tag) & 0xffff);
		if (dflag >= 2) {
			ia = np->rip6_dest;
			applyplen(&ia, np->rip6_plen);
			if (!IN6_ARE_ADDR_EQUAL(&ia, &np->rip6_dest))
				trace(2, " [junk outside prefix]");
		}

		/* Listen-only filter */
		ok = 1;		/* if there's no L filter, it is ok */
		for (iffp = ifcp->ifc_filter; iffp; iffp = iffp->iff_next) {
			if (iffp->iff_type != 'L')
				continue;
			ok = 0;
			if (np->rip6_plen < iffp->iff_plen)
				continue;
			/* special rule: ::/0 means default, not "in /0" */
			if (iffp->iff_plen == 0 && np->rip6_plen > 0)
				continue;
			ia = np->rip6_dest; 
			applyplen(&ia, iffp->iff_plen);
			if (IN6_ARE_ADDR_EQUAL(&ia, &iffp->iff_addr)) {
				ok = 1;
				break;
			}
		}
		if (!ok) {
			trace(2, "  (filtered)\n");
			continue;
		}

		trace(2, "\n");
		np->rip6_metric++;
		np->rip6_metric += ifcp->ifc_metric;
		if (np->rip6_metric > HOPCNT_INFINITY6)
			np->rip6_metric = HOPCNT_INFINITY6;

		applyplen(&np->rip6_dest, np->rip6_plen);
		if ((rrt = rtsearch(np)) != NULL) {
			if (rrt->rrt_t == 0)
				continue;	/* Intf route has priority */
			nq = &rrt->rrt_info;
			if (nq->rip6_metric > np->rip6_metric) {
				if (rrt->rrt_index == ifcp->ifc_index &&
				    IN6_ARE_ADDR_EQUAL(&nh, &rrt->rrt_gw)) {
					/* Small metric from the same gateway */
					nq->rip6_metric = np->rip6_metric;
				} else {
					/* Better route found */
					rrt->rrt_index = ifcp->ifc_index;
					/* Update routing table */
					delroute(nq, &rrt->rrt_gw);
					rrt->rrt_gw = nh;
					*nq = *np;
					addroute(rrt, &nh, ifcp);
				}
				rrt->rrt_rflags |= RRTF_CHANGED;
				rrt->rrt_t = t;
				need_trigger = 1;
			} else if (nq->rip6_metric < np->rip6_metric &&
				   rrt->rrt_index == ifcp->ifc_index &&
				   IN6_ARE_ADDR_EQUAL(&nh, &rrt->rrt_gw)) {
				/* Got worse route from same gw */
				nq->rip6_metric = np->rip6_metric;
				rrt->rrt_t = t;
				rrt->rrt_rflags |= RRTF_CHANGED;
				need_trigger = 1;
			} else if (nq->rip6_metric == np->rip6_metric &&
				   rrt->rrt_index == ifcp->ifc_index &&
				   IN6_ARE_ADDR_EQUAL(&nh, &rrt->rrt_gw) &&
				   np->rip6_metric < HOPCNT_INFINITY6) {
				/* same metric, same route from same gw */
				rrt->rrt_t = t;
			}
			/* 
			 * if nq->rip6_metric == HOPCNT_INFINITY6 then
			 * do not update age value. Do nothing.
			 */
		} else if (np->rip6_metric < HOPCNT_INFINITY6) {
			/* Got a new valid route */
			if ((rrt = MALLOC(struct riprt)) == NULL)
				fatal("malloc: struct riprt");
			memset(rrt, 0, sizeof(*rrt));
			nq = &rrt->rrt_info;

			rrt->rrt_same = NULL;
			rrt->rrt_index = ifcp->ifc_index;
			rrt->rrt_flags = RTF_UP|RTF_GATEWAY;
			rrt->rrt_gw = nh;
			*nq = *np;
			applyplen(&nq->rip6_dest, nq->rip6_plen);
			if (nq->rip6_plen == sizeof(struct in6_addr) * 8)
				rrt->rrt_flags |= RTF_HOST;

			/* Put the route to the list */
			rrt->rrt_next = riprt;
			riprt = rrt;
			/* Update routing table */
			addroute(rrt, &nh, ifcp);
			rrt->rrt_rflags |= RRTF_CHANGED;
			need_trigger = 1;
			rrt->rrt_t = t;
		}
	}
	/* XXX need to care the interval between triggered updates */
	if (need_trigger) {
		if (nextalarm > time(NULL) + RIP_TRIG_INT6_MAX) {
			for (ic = ifc; ic; ic = ic->ifc_next) {
				if (ifcp->ifc_index == ic->ifc_index)
					continue;
				if (ic->ifc_flags & IFF_UP)
					ripsend(ic, &ic->ifc_ripsin,
						RRTF_CHANGED);
			}
		}
		/* Reset the flag */
		for (rrt = riprt; rrt; rrt = rrt->rrt_next)
			rrt->rrt_rflags &= ~RRTF_CHANGED;
	}
}

/*
 * Send all routes request packet to the specified interface.
 */
void
sendrequest(ifcp)
	struct ifc *ifcp;
{
	struct netinfo6 *np;
	int error;

	if (ifcp->ifc_flags & IFF_LOOPBACK)
		return;
	ripbuf->rip6_cmd = RIP6_REQUEST;
	np = ripbuf->rip6_nets;
	memset(np, 0, sizeof(struct netinfo6));
	np->rip6_metric = HOPCNT_INFINITY6;
	tracet(1, "Send rtdump Request to %s (%s)\n",
		ifcp->ifc_name, inet6_n2p(&ifcp->ifc_ripsin.sin6_addr));
	error = sendpacket(&ifcp->ifc_ripsin, RIPSIZE(1));
	if (error == EAFNOSUPPORT) {
		/* Protocol not supported */
		tracet(1, "Could not send rtdump Request to %s (%s): "
			"set IFF_UP to 0\n",
			ifcp->ifc_name, inet6_n2p(&ifcp->ifc_ripsin.sin6_addr));
		ifcp->ifc_flags &= ~IFF_UP;	/* As if down for AF_INET6 */
	}
	ripbuf->rip6_cmd = RIP6_RESPONSE;
}

/*
 * Process a RIP6_REQUEST packet.
 */
void
riprequest(ifcp, np, nn, sin)
	struct ifc *ifcp;
	struct netinfo6 *np;
	int nn;
	struct sockaddr_in6 *sin;
{
	int i;
	struct riprt *rrt;

	if (!(nn == 1 && IN6_IS_ADDR_UNSPECIFIED(&np->rip6_dest) &&
	      np->rip6_plen == 0 && np->rip6_metric == HOPCNT_INFINITY6)) {
		/* Specific response, don't split-horizon */
		trace(1, "\tRIP Request\n");
		for (i = 0; i < nn; i++, np++) {
			rrt = rtsearch(np);
			if (rrt)
				np->rip6_metric = rrt->rrt_info.rip6_metric;
			else
				np->rip6_metric = HOPCNT_INFINITY6;
		}
		(void)sendpacket(sin, RIPSIZE(nn));
		return;
	}
	/* Whole routing table dump */
	trace(1, "\tRIP Request -- whole routing table\n");
	ripsend(ifcp, sin, RRTF_SENDANYWAY);
}

/*
 * Get information of each interface.
 */
void
ifconfig()
{
#ifdef HAVE_GETIFADDRS
	struct ifaddrs *ifap, *ifa;
	struct ifc *ifcp;
	struct ipv6_mreq mreq;
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		fatal("socket");

	if (getifaddrs(&ifap) != 0)
		fatal("getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		ifcp = ifc_find(ifa->ifa_name);
		/* we are interested in multicast-capable interfaces */
		if ((ifa->ifa_flags & IFF_MULTICAST) == 0)
			continue;
		if (!ifcp) {
			/* new interface */
			if ((ifcp = MALLOC(struct ifc)) == NULL)
				fatal("malloc: struct ifc");
			memset(ifcp, 0, sizeof(*ifcp));
			ifcp->ifc_index = -1;
			ifcp->ifc_next = ifc;
			ifc = ifcp;
			nifc++;
			ifcp->ifc_name = allocopy(ifa->ifa_name);
			ifcp->ifc_addr = 0;
			ifcp->ifc_filter = 0;
			ifcp->ifc_flags = ifa->ifa_flags;
			trace(1, "newif %s <%s>\n", ifcp->ifc_name,
				ifflags(ifcp->ifc_flags));
			if (!strcmp(ifcp->ifc_name, LOOPBACK_IF))
				loopifcp = ifcp;
		} else {
			/* update flag, this may be up again */
			if (ifcp->ifc_flags != ifa->ifa_flags) {
				trace(1, "%s: <%s> -> ", ifcp->ifc_name,
					ifflags(ifcp->ifc_flags));
				trace(1, "<%s>\n", ifflags(ifa->ifa_flags));
			}
			ifcp->ifc_flags = ifa->ifa_flags;
		}
		ifconfig1(ifa->ifa_name, ifa->ifa_addr, ifcp, s);
		if ((ifcp->ifc_flags & (IFF_LOOPBACK | IFF_UP)) == IFF_UP
		 && 0 < ifcp->ifc_index && !ifcp->ifc_joined) {
			mreq.ipv6mr_multiaddr = ifcp->ifc_ripsin.sin6_addr;
			mreq.ipv6mr_interface = ifcp->ifc_index;
			if (setsockopt(ripsock, IPPROTO_IPV6,
				IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0)
				fatal("IPV6_JOIN_GROUP");
			trace(1, "join %s %s\n", ifcp->ifc_name, RIP6_DEST);
			ifcp->ifc_joined++;
		}
	}
	close(s);
	freeifaddrs(ifap);
#else
	int	s, i;
	char	*buf;
	struct	ifconf ifconf;
	struct	ifreq *ifrp, ifr;
	struct	ifc *ifcp;
	struct	ipv6_mreq mreq;
	int	bufsiz;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		fatal("socket");

	/* wild guess - v4, media, link, v6 * 3 */
	bufsiz = if_maxindex() * sizeof(struct ifreq) * 6;
	if ((buf = (char *)malloc(bufsiz)) == NULL)
		fatal("malloc");

	/*
	 * ioctl(SIOCGIFCONF) does not return error on buffer size.
	 * we'll try to guess the buffer size by trying it twice, with
	 * different buffer size.
	 */
	ifconf.ifc_buf = buf;
	ifconf.ifc_len = bufsiz / 2;
	if (ioctl(s, SIOCGIFCONF, (char *)&ifconf) < 0)
		fatal("ioctl: SIOCGIFCONF");
	i = ifconf.ifc_len;
	while (1) {
		char *newbuf;

		ifconf.ifc_buf = buf;
		ifconf.ifc_len = bufsiz;
		if (ioctl(s, SIOCGIFCONF, (char *)&ifconf) < 0)
			fatal("ioctl: SIOCGIFCONF");
		if (i == ifconf.ifc_len)
			break;
		i = ifconf.ifc_len;
		bufsiz *= 2;
		if ((newbuf = (char *)realloc(buf, bufsiz)) == NULL) {
			free(buf);
			fatal("realloc");
		}
		buf = newbuf;
	}
	for (i = 0; i < ifconf.ifc_len; ) {
		ifrp = (struct ifreq *)(buf + i);
		if (ifrp->ifr_addr.sa_family != AF_INET6)
			goto skip;
		ifcp = ifc_find(ifrp->ifr_name);
		strcpy(ifr.ifr_name, ifrp->ifr_name);
		if (ioctl(s, SIOCGIFFLAGS, (char *)&ifr) < 0)
			fatal("ioctl: SIOCGIFFLAGS");
		/* we are interested in multicast-capable interfaces */
		if ((ifr.ifr_flags & IFF_MULTICAST) == 0)
			goto skip;
		if (!ifcp) {
			/* new interface */
			if ((ifcp = MALLOC(struct ifc)) == NULL)
				fatal("malloc: struct ifc");
			memset(ifcp, 0, sizeof(*ifcp));
			ifcp->ifc_index = -1;
			ifcp->ifc_next = ifc;
			ifc = ifcp;
			nifc++;
			ifcp->ifc_name = allocopy(ifrp->ifr_name);
			ifcp->ifc_addr = 0;
			ifcp->ifc_filter = 0;
			ifcp->ifc_flags = ifr.ifr_flags;
			trace(1, "newif %s <%s>\n", ifcp->ifc_name,
				ifflags(ifcp->ifc_flags));
			if (!strcmp(ifcp->ifc_name, LOOPBACK_IF))
				loopifcp = ifcp;
		} else {
			/* update flag, this may be up again */
			if (ifcp->ifc_flags != ifr.ifr_flags) {
				trace(1, "%s: <%s> -> ", ifcp->ifc_name,
					ifflags(ifcp->ifc_flags));
				trace(1, "<%s>\n", ifflags(ifr.ifr_flags));
			}
			ifcp->ifc_flags = ifr.ifr_flags;
		}
		ifconfig1(ifrp->ifr_name, &ifrp->ifr_addr, ifcp, s);
		if ((ifcp->ifc_flags & (IFF_LOOPBACK | IFF_UP)) == IFF_UP
		 && 0 < ifcp->ifc_index && !ifcp->ifc_joined) {
			mreq.ipv6mr_multiaddr = ifcp->ifc_ripsin.sin6_addr;
			mreq.ipv6mr_interface = ifcp->ifc_index;
			if (setsockopt(ripsock, IPPROTO_IPV6,
				IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0)
				fatal("IPV6_JOIN_GROUP");
			trace(1, "join %s %s\n", ifcp->ifc_name, RIP6_DEST);
			ifcp->ifc_joined++;
		}
skip:
		i += IFNAMSIZ;
		if (ifrp->ifr_addr.sa_len > sizeof(struct sockaddr))
			i += ifrp->ifr_addr.sa_len;
		else
			i += sizeof(struct sockaddr);
	}
	close(s);
	free(buf);
#endif
}

void
ifconfig1(name, sa, ifcp, s)
	const char *name;
	const struct sockaddr *sa;
	struct	ifc *ifcp;
	int	s;
{
	struct	in6_ifreq ifr;
	struct	sockaddr_in6 *sin;
	struct	ifac *ifa;
	int	plen;
	char	buf[BUFSIZ];

	sin = (struct sockaddr_in6 *)sa;
	ifr.ifr_addr = *sin;
	strcpy(ifr.ifr_name, name);
	if (ioctl(s, SIOCGIFNETMASK_IN6, (char *)&ifr) < 0)
		fatal("ioctl: SIOCGIFNETMASK_IN6");
	plen = mask2len(&ifr.ifr_addr.sin6_addr, 16);
	if ((ifa = ifa_match(ifcp, &sin->sin6_addr, plen)) != NULL) {
		/* same interface found */
		/* need check if something changed */
		/* XXX not yet implemented */
		return;
	}
	/*
	 * New address is found
	 */
	if ((ifa = MALLOC(struct ifac)) == NULL)
		fatal("malloc: struct ifac");
	memset(ifa, 0, sizeof(*ifa));
	ifa->ifa_conf = ifcp;
	ifa->ifa_next = ifcp->ifc_addr;
	ifcp->ifc_addr = ifa;
	ifa->ifa_addr = sin->sin6_addr;
	ifa->ifa_plen = plen;
	if (ifcp->ifc_flags & IFF_POINTOPOINT) {
		ifr.ifr_addr = *sin;
		if (ioctl(s, SIOCGIFDSTADDR_IN6, (char *)&ifr) < 0)
			fatal("ioctl: SIOCGIFDSTADDR_IN6");
		ifa->ifa_raddr = ifr.ifr_dstaddr.sin6_addr;
		inet_ntop(AF_INET6, (void *)&ifa->ifa_raddr, buf, sizeof(buf));
		trace(1, "found address %s/%d -- %s\n",
			inet6_n2p(&ifa->ifa_addr), ifa->ifa_plen, buf);
	} else {
		trace(1, "found address %s/%d\n",
			inet6_n2p(&ifa->ifa_addr), ifa->ifa_plen);
	}
	if (ifcp->ifc_index < 0 && IN6_IS_ADDR_LINKLOCAL(&ifa->ifa_addr)) {
		ifcp->ifc_mylladdr = ifa->ifa_addr;
		ifcp->ifc_index = IN6_LINKLOCAL_IFINDEX(ifa->ifa_addr);
		memcpy(&ifcp->ifc_ripsin, &ripsin, ripsin.ss_len);
		SET_IN6_LINKLOCAL_IFINDEX(ifcp->ifc_ripsin.sin6_addr,
			ifcp->ifc_index);
		setindex2ifc(ifcp->ifc_index, ifcp);
		ifcp->ifc_mtu = getifmtu(ifcp->ifc_index);
		if (ifcp->ifc_mtu > RIP6_MAXMTU)
			ifcp->ifc_mtu = RIP6_MAXMTU;
		if (ioctl(s, SIOCGIFMETRIC, (char *)&ifr) < 0)
			fatal("ioctl: SIOCGIFMETRIC");
		ifcp->ifc_metric = ifr.ifr_metric;
		trace(1, "\tindex: %d, mtu: %d, metric: %d\n",
			ifcp->ifc_index, ifcp->ifc_mtu, ifcp->ifc_metric);
	}
}

/*
 * Receive and process routing messages.
 * Update interface information as necesssary.
 */
void
rtrecv()
{
	char buf[BUFSIZ];
	char *p, *q;
	struct rt_msghdr *rtm;
	struct ifa_msghdr *ifam;
	struct if_msghdr *ifm;
	int len;
	struct ifc *ifcp;
	int iface = 0, rtable = 0;
	struct sockaddr_in6 *rta[RTAX_MAX];
	int i, addrs;

	if ((len = read(rtsock, buf, sizeof(buf))) < 0) {
		perror("read from rtsock");
		exit(-1);
	}
	if (len < sizeof(*rtm)) {
		trace(1, "short read from rtsock: %d (should be > %lu)\n",
			len, (u_long)sizeof(*rtm));
		return;
	}

	for (p = buf; p - buf < len; p += ((struct rt_msghdr *)p)->rtm_msglen) {
		/* safety against bogus message */
		if (((struct rt_msghdr *)p)->rtm_msglen <= 0) {
			trace(1, "bogus rtmsg: length=%d\n",
				((struct rt_msghdr *)p)->rtm_msglen);
			break;
		}
		rtm = NULL;
		ifam = NULL;
		ifm = NULL;
		switch (((struct rt_msghdr *)p)->rtm_type) {
		case RTM_NEWADDR:
		case RTM_DELADDR:
			ifam = (struct ifa_msghdr *)p;
			addrs = ifam->ifam_addrs;
			q = (char *)(ifam + 1);
			break;
		case RTM_IFINFO:
			ifm = (struct if_msghdr *)p;
			addrs = ifm->ifm_addrs;
			q = (char *)(ifm + 1);
			break;
		default:
			rtm = (struct rt_msghdr *)p;
			addrs = rtm->rtm_addrs;
			q = (char *)(rtm + 1);
			if (rtm->rtm_version != RTM_VERSION) {
				trace(1, "unexpected rtmsg version %d "
					"(should be %d)\n",
					rtm->rtm_version, RTM_VERSION);
				continue;
			}
			if (rtm->rtm_pid == pid) {
#if 0
				trace(1, "rtmsg looped back to me, ignored\n");
#endif
				continue;
			}
			break;
		}
		memset(&rta, 0, sizeof(rta));
		for (i = 0; i < RTAX_MAX; i++) {
			if (addrs & (1 << i)) {
				rta[i] = (struct sockaddr_in6 *)q;
				q += ROUNDUP(rta[i]->sin6_len);
			}
		}

		trace(1, "rtsock: %s (addrs=%x)\n",
			rttypes((struct rt_msghdr *)p), addrs);
		if (dflag >= 2) {
			int i;
			for (i = 0;
			     i < ((struct rt_msghdr *)p)->rtm_msglen;
			     i++) {
				fprintf(stderr, "%02x ", p[i] & 0xff);
				if (i % 16 == 15) fprintf(stderr, "\n");
			}
			fprintf(stderr, "\n");
		}

		/*
		 * Easy ones first.
		 *
		 * We may be able to optimize by using ifm->ifm_index or
		 * ifam->ifam_index.  For simplicity we don't do that here.
		 */
		switch (((struct rt_msghdr *)p)->rtm_type) {
		case RTM_NEWADDR:
		case RTM_IFINFO:
			iface++;
			continue;
		case RTM_ADD:
			rtable++;
			continue;
		case RTM_LOSING:
		case RTM_MISS:
		case RTM_RESOLVE:
		case RTM_GET:
		case RTM_LOCK:
			/* nothing to be done here */
			trace(1, "\tnothing to be done, ignored\n");
			continue;
		}

#if 0
		if (rta[RTAX_DST] == NULL) {
			trace(1, "\tno destination, ignored\n");
			continue;	
		}
		if (rta[RTAX_DST]->sin6_family != AF_INET6) {
			trace(1, "\taf mismatch, ignored\n");
			continue;
		}
		if (IN6_IS_ADDR_LINKLOCAL(&rta[RTAX_DST]->sin6_addr)) {
			trace(1, "\tlinklocal destination, ignored\n");
			continue;
		}
		if (IN6_ARE_ADDR_EQUAL(&rta[RTAX_DST]->sin6_addr, &in6addr_loopback)) {
			trace(1, "\tloopback destination, ignored\n");
			continue;		/* Loopback */
		}
		if (IN6_IS_ADDR_MULTICAST(&rta[RTAX_DST]->sin6_addr)) {
			trace(1, "\tmulticast destination, ignored\n");
			continue;
		}
#endif

		/* hard ones */
		switch (((struct rt_msghdr *)p)->rtm_type) {
		case RTM_NEWADDR:
		case RTM_IFINFO:
		case RTM_ADD:
		case RTM_LOSING:
		case RTM_MISS:
		case RTM_RESOLVE:
		case RTM_GET:
		case RTM_LOCK:
			/* should already be handled */
			fatal("rtrecv: never reach here");
		case RTM_DELETE:
			if (!rta[RTAX_DST] || !rta[RTAX_GATEWAY]
			 || !rta[RTAX_NETMASK]) {
				trace(1, "\tsome of dst/gw/netamsk are unavailable, ignored\n");
				break;
			}
			if (rt_del(rta[RTAX_DST], rta[RTAX_GATEWAY], rta[RTAX_NETMASK]) == 0) {
				rtable++;	/*just to be sure*/
			}
			break;
		case RTM_CHANGE:
		case RTM_REDIRECT:
			trace(1, "\tnot supported yet, ignored\n");
			break;
		case RTM_DELADDR:
			if (!rta[RTAX_NETMASK] || !rta[RTAX_IFA]) {
				trace(1, "\tno netmask or ifa given, ignored\n");
				break;
			}
			if (ifam->ifam_index < nindex2ifc)
				ifcp = index2ifc[ifam->ifam_index];
			else
				ifcp = NULL;
			if (!ifcp) {
				trace(1, "\tinvalid ifam_index %d, ignored\n",
					ifam->ifam_index);
				break;
			}
			rt_deladdr(ifcp, rta[RTAX_IFA], rta[RTAX_NETMASK]);
			iface++;
			break;
		case RTM_OLDADD:
		case RTM_OLDDEL:
			trace(1, "\tnot supported yet, ignored\n");
			break;
		}

	}

	if (iface) {
		trace(1, "rtsock: reconfigure interfaces, refresh interface routes\n");
		ifconfig();
		for (ifcp = ifc; ifcp; ifcp = ifcp->ifc_next)
			ifrt(ifcp, 1);
	}
	if (rtable) {
		trace(1, "rtsock: read routing table again\n");
		krtread(1);
	}
}

/*
 * remove specified route from the internal routing table.
 */
int
rt_del(sdst, sgw, smask)
	const struct sockaddr_in6 *sdst;
	const struct sockaddr_in6 *sgw;
	const struct sockaddr_in6 *smask;
{
	const struct in6_addr *dst = NULL;
	const struct in6_addr *gw = NULL;
	int prefix;
	struct netinfo6 ni6;
	struct riprt *rrt = NULL;
	time_t t_lifetime;

	if (sdst->sin6_family != AF_INET6) {
		trace(1, "\tother AF, ignored\n");
		return -1;
	}
	if (IN6_IS_ADDR_LINKLOCAL(&sdst->sin6_addr)
	 || IN6_ARE_ADDR_EQUAL(&sdst->sin6_addr, &in6addr_loopback)
	 || IN6_IS_ADDR_MULTICAST(&sdst->sin6_addr)) {
		trace(1, "\taddress %s not interesting, ignored\n",
			inet6_n2p(&sdst->sin6_addr));
		return -1;
	}
	dst = &sdst->sin6_addr;
	if (sgw->sin6_family == AF_INET6
	 && smask->sin6_family == AF_INET6) {
		/* easy case */
		gw = &sgw->sin6_addr;
		prefix = mask2len(&smask->sin6_addr, 16);
	} else if (sgw->sin6_family == AF_LINK) {
		/*
		 * Interface route... a hard case.  We need to get the prefix
		 * length from the kernel, but we now are parsing rtmsg.
		 * We'll purge matching routes from my list, then get the
		 * fresh list.
		 */
		struct riprt *longest;
		trace(1, "\t%s is a interface route, guessing prefixlen\n",
			inet6_n2p(dst));
		longest = NULL;
		for (rrt = riprt; rrt; rrt = rrt->rrt_next) {
			if (IN6_ARE_ADDR_EQUAL(&rrt->rrt_info.rip6_dest,
					&sdst->sin6_addr)
			 && IN6_IS_ADDR_LOOPBACK(&rrt->rrt_gw)) {
				if (!longest
				 || longest->rrt_info.rip6_plen <
						 rrt->rrt_info.rip6_plen) {
					longest = rrt;
				}
			}
		}
		rrt = longest;
		if (!rrt) {
			trace(1, "\tno matching interface route found\n");
			return -1;
		}
		gw = &in6addr_loopback;
		prefix = rrt->rrt_info.rip6_plen;
	} else {
		trace(1, "\tunsupported af: (gw=%d, mask=%d)\n", 
			sgw->sin6_family, smask->sin6_family);
		return -1;
	}

	trace(1, "\tdeleting %s/%d ", inet6_n2p(dst), prefix);
	trace(1, "gw %s\n", inet6_n2p(gw));
	t_lifetime = time(NULL) - RIP_LIFETIME;
	/* age route for interface address */
	memset(&ni6, 0, sizeof(ni6));
	ni6.rip6_dest = *dst;
	ni6.rip6_plen = prefix;
	applyplen(&ni6.rip6_dest, ni6.rip6_plen);	/*to be sure*/
	trace(1, "\tfind route %s/%d\n", inet6_n2p(&ni6.rip6_dest),
		ni6.rip6_plen);
	if (!rrt && (rrt = rtsearch(&ni6)) == NULL) {
		trace(1, "\tno route found\n");
		return -1;
	}
	if ((rrt->rrt_flags & RTF_STATIC) == 0) {
		trace(1, "\tyou can delete static routes only\n");
	} else if (memcmp(&rrt->rrt_gw, gw, sizeof(rrt->rrt_gw)) != 0) {
		trace(1, "\tgw mismatch: %s <-> ",
			inet6_n2p(&rrt->rrt_gw));
		trace(1, "%s\n", inet6_n2p(gw));
	} else {
		trace(1, "\troute found, age it\n");
		if (rrt->rrt_t == 0 || rrt->rrt_t > t_lifetime) {
			rrt->rrt_t = t_lifetime;
			rrt->rrt_info.rip6_metric = HOPCNT_INFINITY6;
		}
	}
	return 0;
}

/*
 * remove specified address from internal interface/routing table.
 */
int
rt_deladdr(ifcp, sifa, smask)
	struct ifc *ifcp;
	const struct sockaddr_in6 *sifa;
	const struct sockaddr_in6 *smask;
{
	const struct in6_addr *addr = NULL;
	int prefix;
	struct ifac *ifa = NULL;
	struct netinfo6 ni6;
	struct riprt *rrt = NULL;
	time_t t_lifetime;
	int updated = 0;

	if (sifa->sin6_family != AF_INET6 || smask->sin6_family != AF_INET6) {
		trace(1, "\tother AF, ignored\n");
		return -1;
	}
	addr = &sifa->sin6_addr;
	prefix = mask2len(&smask->sin6_addr, 16);

	trace(1, "\tdeleting %s/%d from %s\n",
		inet6_n2p(addr), prefix, ifcp->ifc_name);
	ifa = ifa_match(ifcp, addr, prefix);
	if (!ifa) {
		trace(1, "\tno matching ifa found for %s/%d on %s\n",
			inet6_n2p(addr), prefix, ifcp->ifc_name);
		return -1;
	}
	if (ifa->ifa_conf != ifcp) {
		trace(1, "\taddress table corrupt: back pointer does not match "
			"(%s != %s)\n",
			ifcp->ifc_name, ifa->ifa_conf->ifc_name);
		return -1;
	}
	/* remove ifa from interface */
	if (ifcp->ifc_addr == ifa)
		ifcp->ifc_addr = ifa->ifa_next;
	else {
		struct ifac *p;
		for (p = ifcp->ifc_addr; p; p = p->ifa_next) {
			if (p->ifa_next == ifa) {
				p->ifa_next = ifa->ifa_next;
				break;
			}
		}
	}
	ifa->ifa_next = NULL;
	ifa->ifa_conf = NULL;
	t_lifetime = time(NULL) - RIP_LIFETIME;
	/* age route for interface address */
	memset(&ni6, 0, sizeof(ni6));
	ni6.rip6_dest = ifa->ifa_addr;
	ni6.rip6_plen = ifa->ifa_plen;
	applyplen(&ni6.rip6_dest, ni6.rip6_plen);
	trace(1, "\tfind interface route %s/%d on %d\n",
		inet6_n2p(&ni6.rip6_dest), ni6.rip6_plen, ifcp->ifc_index);
	if ((rrt = rtsearch(&ni6)) != NULL) {
		struct in6_addr none;
		memset(&none, 0, sizeof(none));
		if (rrt->rrt_index == ifcp->ifc_index
		 && memcmp(&rrt->rrt_gw, &none, sizeof(rrt->rrt_gw)) == 0) {
			trace(1, "\troute found, age it\n");
			if (rrt->rrt_t == 0 || rrt->rrt_t > t_lifetime) {
				rrt->rrt_t = t_lifetime;
				rrt->rrt_info.rip6_metric = HOPCNT_INFINITY6;
			}
			updated++;
		} else {
			trace(1, "\tnon-interface route found: %s/%d on %d\n",
				inet6_n2p(&rrt->rrt_info.rip6_dest),
				rrt->rrt_info.rip6_plen,
				rrt->rrt_index);
		}
	} else
		trace(1, "\tno interface route found\n");
	/* age route for p2p destination */
	if (ifcp->ifc_flags & IFF_POINTOPOINT) {
		memset(&ni6, 0, sizeof(ni6));
		ni6.rip6_dest = ifa->ifa_raddr;
		ni6.rip6_plen = 128;
		applyplen(&ni6.rip6_dest, ni6.rip6_plen);	/*to be sure*/
		trace(1, "\tfind p2p route %s/%d on %d\n",
			inet6_n2p(&ni6.rip6_dest), ni6.rip6_plen,
			ifcp->ifc_index);
		if ((rrt = rtsearch(&ni6)) != NULL) {
			if (rrt->rrt_index == ifcp->ifc_index
			 && memcmp(&rrt->rrt_gw, &ifa->ifa_addr,
					 sizeof(rrt->rrt_gw)) == 0) {
				trace(1, "\troute found, age it\n");
				if (rrt->rrt_t == 0 || rrt->rrt_t > t_lifetime) {
					rrt->rrt_t = t_lifetime;
					rrt->rrt_info.rip6_metric =
						HOPCNT_INFINITY6;
					updated++;
				}
			} else {
				trace(1, "\tnon-p2p route found: %s/%d on %d\n",
					inet6_n2p(&rrt->rrt_info.rip6_dest),
					rrt->rrt_info.rip6_plen,
					rrt->rrt_index);
			}
		} else
			trace(1, "\tno p2p route found\n");
	}
	return updated ? 0 : -1;
}

/*
 * Get each interface address and put those interface routes to the route
 * list.
 */
void
ifrt(ifcp, again)
	struct ifc *ifcp;
	int again;
{
	struct ifac *ifa;
	struct riprt *rrt;
	struct netinfo6 *np;

	if (ifcp->ifc_flags & IFF_LOOPBACK)
		return;			/* ignore loopback */
	if (ifcp->ifc_flags & IFF_POINTOPOINT) {
		ifrt_p2p(ifcp, again);
		return;
	}

	for (ifa = ifcp->ifc_addr; ifa; ifa = ifa->ifa_next) {
		if (IN6_IS_ADDR_LINKLOCAL(&ifa->ifa_addr)) {
#if 0
			trace(1, "route: %s on %s: "
			    "skip linklocal interface address\n",
			    inet6_n2p(&ifa->ifa_addr), ifcp->ifc_name);
#endif
			continue;
		}
		if (IN6_IS_ADDR_UNSPECIFIED(&ifa->ifa_addr)) {
#if 0
			trace(1, "route: %s: skip unspec interface address\n",
			    ifcp->ifc_name);
#endif
			continue;
		}
		if ((rrt = MALLOC(struct riprt)) == NULL)
			fatal("malloc: struct riprt");
		memset(rrt, 0, sizeof(*rrt));
		rrt->rrt_same = NULL;
		rrt->rrt_index = ifcp->ifc_index;
		rrt->rrt_t = 0;	/* don't age */
		rrt->rrt_info.rip6_dest = ifa->ifa_addr;
		rrt->rrt_info.rip6_tag = htons(routetag & 0xffff);
		rrt->rrt_info.rip6_metric = 1 + ifcp->ifc_metric;
		rrt->rrt_info.rip6_plen = ifa->ifa_plen;
		applyplen(&rrt->rrt_info.rip6_dest, ifa->ifa_plen);
		memset(&rrt->rrt_gw, 0, sizeof(struct in6_addr));
		np = &rrt->rrt_info;
		if (rtsearch(np) == NULL) {
			/* Attach the route to the list */
			trace(1, "route: %s/%d: register route (%s)\n",
			    inet6_n2p(&np->rip6_dest), np->rip6_plen,
			    ifcp->ifc_name);
			rrt->rrt_next = riprt;
			riprt = rrt;
		} else {
			/* Already found */
			if (!again) {
				trace(1, "route: %s/%d: "
				    "already registered (%s)\n",
				    inet6_n2p(&np->rip6_dest), np->rip6_plen,
				    ifcp->ifc_name);
			}
			free(rrt);
		}
	}
}

/*
 * there are couple of p2p interface routing models.  "behavior" lets
 * you pick one.  it looks that gated behavior fits best with BSDs,
 * since BSD kernels does not look at prefix length on p2p interfaces.
 */
void
ifrt_p2p(ifcp, again)
	struct ifc *ifcp;
	int again;
{
	struct ifac *ifa;
	struct riprt *rrt;
	struct netinfo6 *np;
	struct in6_addr addr, dest;
	int advert, ignore, i;
#define P2PADVERT_NETWORK	1
#define P2PADVERT_ADDR		2
#define P2PADVERT_DEST		4
#define P2PADVERT_MAX		4
	const enum { CISCO, GATED, ROUTE6D } behavior = GATED;
	const char *category;
	const char *noadv;

	for (ifa = ifcp->ifc_addr; ifa; ifa = ifa->ifa_next) {
		addr = ifa->ifa_addr;
		dest = ifa->ifa_raddr;
		applyplen(&addr, ifa->ifa_plen);
		applyplen(&dest, ifa->ifa_plen);
		advert = ignore = 0;
		switch (behavior) {
		case CISCO:
			/*
			 * honor addr/plen, just like normal shared medium
			 * interface.  this may cause trouble if you reuse
			 * addr/plen on other interfaces.
			 *
			 * advertise addr/plen.
			 */
			advert |= P2PADVERT_NETWORK;
			break;
		case GATED:
			/*
			 * prefixlen on p2p interface is meaningless.
			 * advertise addr/128 and dest/128.
			 *
			 * do not install network route to route6d routing
			 * table (if we do, it would prevent route installation
			 * for other p2p interface that shares addr/plen).
			 */
			advert |= P2PADVERT_ADDR;
			advert |= P2PADVERT_DEST;
			ignore |= P2PADVERT_NETWORK;
			break;
		case ROUTE6D:
			/*
			 * just for testing...
			 */
			if (IN6_ARE_ADDR_EQUAL(&addr, &dest))
				advert |= P2PADVERT_NETWORK;
			else {
				advert |= P2PADVERT_ADDR;
				advert |= P2PADVERT_DEST;
				ignore |= P2PADVERT_NETWORK;
			}
			break;
		}

		for (i = 1; i <= P2PADVERT_MAX; i *= 2) {
			if ((ignore & i) != 0)
				continue;
			if ((rrt = MALLOC(struct riprt)) == NULL)
				fatal("malloc: struct riprt");
			memset(rrt, 0, sizeof(*rrt));
			rrt->rrt_same = NULL;
			rrt->rrt_index = ifcp->ifc_index;
			rrt->rrt_t = 0;	/* don't age */
			switch (i) {
			case P2PADVERT_NETWORK:
				rrt->rrt_info.rip6_dest = ifa->ifa_addr;
				rrt->rrt_info.rip6_plen = ifa->ifa_plen;
				applyplen(&rrt->rrt_info.rip6_dest,
				    ifa->ifa_plen);
				category = "network";
				break;
			case P2PADVERT_ADDR:
				rrt->rrt_info.rip6_dest = ifa->ifa_addr;
				rrt->rrt_info.rip6_plen = 128;
				category = "addr";
				break;
			case P2PADVERT_DEST:
				rrt->rrt_info.rip6_dest = ifa->ifa_raddr;
				rrt->rrt_info.rip6_plen = 128;
				category = "dest";
				break;
			}
			if (IN6_IS_ADDR_UNSPECIFIED(&rrt->rrt_info.rip6_dest) ||
			    IN6_IS_ADDR_LINKLOCAL(&rrt->rrt_info.rip6_dest)) {
#if 0
				trace(1, "route: %s: skip unspec/linklocal "
				    "(%s on %s)\n", category, ifcp->ifc_name);
#endif
				free(rrt);
				continue;
			}
			if ((advert & i) == 0) {
				rrt->rrt_rflags |= RRTF_NOADVERTISE;
				noadv = ", NO-ADV";
			} else
				noadv = "";
			rrt->rrt_info.rip6_tag = htons(routetag & 0xffff);
			rrt->rrt_info.rip6_metric = 1 + ifcp->ifc_metric;
			memset(&rrt->rrt_gw, 0, sizeof(struct in6_addr));
			np = &rrt->rrt_info;
			if (rtsearch(np) == NULL) {
				/* Attach the route to the list */
				trace(1, "route: %s/%d: register route "
				    "(%s on %s%s)\n",
				    inet6_n2p(&np->rip6_dest), np->rip6_plen,
				    category, ifcp->ifc_name, noadv);
				rrt->rrt_next = riprt;
				riprt = rrt;
			} else {
				/* Already found */
				if (!again) {
					trace(1, "route: %s/%d: "
					    "already registered (%s on %s%s)\n",
					    inet6_n2p(&np->rip6_dest),
					    np->rip6_plen, category,
					    ifcp->ifc_name, noadv);
				}
				free(rrt);
			}
		}
	}
#undef P2PADVERT_NETWORK
#undef P2PADVERT_ADDR
#undef P2PADVERT_DEST
#undef P2PADVERT_MAX
}

int
getifmtu(ifindex)
	int	ifindex;
{
	int	mib[6];
	char	*buf;
	size_t	msize;
	struct	if_msghdr *ifm;
	int	mtu;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;
	mib[4] = NET_RT_IFLIST;
	mib[5] = ifindex;
	if (sysctl(mib, 6, NULL, &msize, NULL, 0) < 0)
		fatal("sysctl estimate NET_RT_IFLIST");
	if ((buf = malloc(msize)) == NULL)
		fatal("malloc");
	if (sysctl(mib, 6, buf, &msize, NULL, 0) < 0)
		fatal("sysctl NET_RT_IFLIST");
	ifm = (struct if_msghdr *)buf;
	mtu = ifm->ifm_data.ifi_mtu;
#ifdef	__FREEBSD__
	if (ifindex != ifm->ifm_index)
		fatal("ifindex does not match with ifm_index");
#endif	/* __FREEBSD__ */
	free(buf);
	return mtu;
}

const char *
rttypes(rtm)
	struct rt_msghdr *rtm;
{
#define	RTTYPE(s, f) \
do { \
	if (rtm->rtm_type == (f)) \
		return (s); \
} while (0)
	RTTYPE("ADD", RTM_ADD);
	RTTYPE("DELETE", RTM_DELETE);
	RTTYPE("CHANGE", RTM_CHANGE);
	RTTYPE("GET", RTM_GET);
	RTTYPE("LOSING", RTM_LOSING);
	RTTYPE("REDIRECT", RTM_REDIRECT);
	RTTYPE("MISS", RTM_MISS);
	RTTYPE("LOCK", RTM_LOCK);
	RTTYPE("OLDADD", RTM_OLDADD);
	RTTYPE("OLDDEL", RTM_OLDDEL);
	RTTYPE("RESOLVE", RTM_RESOLVE);
	RTTYPE("NEWADDR", RTM_NEWADDR);
	RTTYPE("DELADDR", RTM_DELADDR);
	RTTYPE("IFINFO", RTM_IFINFO);
#undef RTTYPE
	return NULL;
}

const char *
rtflags(rtm)
	struct rt_msghdr *rtm;
{
	static char buf[BUFSIZ];

	strcpy(buf, "");
#define	RTFLAG(s, f) \
do { \
	if (rtm->rtm_flags & (f)) \
		strcat(buf, (s)); \
} while (0)
	RTFLAG("U", RTF_UP);
	RTFLAG("G", RTF_GATEWAY);
	RTFLAG("H", RTF_HOST);
	RTFLAG("R", RTF_REJECT);
	RTFLAG("D", RTF_DYNAMIC);
	RTFLAG("M", RTF_MODIFIED);
	RTFLAG("d", RTF_DONE);
#ifdef	RTF_MASK
	RTFLAG("m", RTF_MASK);
#endif
	RTFLAG("C", RTF_CLONING);
	RTFLAG("X", RTF_XRESOLVE);
	RTFLAG("L", RTF_LLINFO);
	RTFLAG("S", RTF_STATIC);
	RTFLAG("B", RTF_BLACKHOLE);
	RTFLAG("2", RTF_PROTO2);
	RTFLAG("1", RTF_PROTO1);
#undef RTFLAG
	return buf;
}

const char *
ifflags(flags)
	int flags;
{
	static char buf[BUFSIZ];

	strcpy(buf, "");
#define	IFFLAG(s, f) \
do { \
	if (flags & f) { \
		if (buf[0]) \
			strcat(buf, ","); \
		strcat(buf, s); \
	} \
} while (0)
	IFFLAG("UP", IFF_UP);
	IFFLAG("BROADCAST", IFF_BROADCAST);
	IFFLAG("DEBUG", IFF_DEBUG);
	IFFLAG("LOOPBACK", IFF_LOOPBACK);
	IFFLAG("POINTOPOINT", IFF_POINTOPOINT);
#ifdef IFF_NOTRAILERS
	IFFLAG("NOTRAILERS", IFF_NOTRAILERS);
#endif
	IFFLAG("RUNNING", IFF_RUNNING);
	IFFLAG("NOARP", IFF_NOARP);
	IFFLAG("PROMISC", IFF_PROMISC);
	IFFLAG("ALLMULTI", IFF_ALLMULTI);
	IFFLAG("OACTIVE", IFF_OACTIVE);
	IFFLAG("SIMPLEX", IFF_SIMPLEX);
	IFFLAG("LINK0", IFF_LINK0);
	IFFLAG("LINK1", IFF_LINK1);
	IFFLAG("LINK2", IFF_LINK2);
	IFFLAG("MULTICAST", IFF_MULTICAST);
#undef IFFLAG
	return buf;
}

void
krtread(again)
	int again;
{
	int mib[6];
	size_t msize;
	char *buf, *p, *lim;
	struct rt_msghdr *rtm;
	int retry;
	const char *errmsg;

	retry = 0;
	buf = NULL;
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;	/* Address family */
	mib[4] = NET_RT_DUMP;	/* Dump the kernel routing table */
	mib[5] = 0;		/* No flags */
	do {
		retry++;
		errmsg = NULL;
		if (buf)
			free(buf);
		if (sysctl(mib, 6, NULL, &msize, NULL, 0) < 0) {
			errmsg = "sysctl estimate";
			continue;
		}
		if ((buf = malloc(msize)) == NULL) {
			errmsg = "malloc";
			continue;
		}
		if (sysctl(mib, 6, buf, &msize, NULL, 0) < 0) {
			errmsg = "sysctl NET_RT_DUMP";
			continue;
		}
	} while (retry < 5 && errmsg != NULL);
	if (errmsg)
		fatal("%s (with %d retries, msize=%lu)", errmsg, retry,
		    (u_long)msize);
	else if (1 < retry)
		syslog(LOG_INFO, "NET_RT_DUMP %d retires", retry);

	lim = buf + msize;
	for (p = buf; p < lim; p += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)p;
		rt_entry(rtm, again);
	}
	free(buf);
}

void
rt_entry(rtm, again)
	struct rt_msghdr *rtm;
	int again;
{
	struct	sockaddr_in6 *sin6_dst, *sin6_gw, *sin6_mask;
	struct	sockaddr_in6 *sin6_genmask, *sin6_ifp;
	char	*rtmp, *ifname = NULL;
	struct	riprt *rrt;
	struct	netinfo6 *np;
	int	s;

	sin6_dst = sin6_gw = sin6_mask = sin6_genmask = sin6_ifp = 0;
	if ((rtm->rtm_flags & RTF_UP) == 0 || rtm->rtm_flags &
		(RTF_CLONING|RTF_XRESOLVE|RTF_LLINFO|RTF_BLACKHOLE)) {
		return;		/* not interested in the link route */
	}
	rtmp = (char *)(rtm + 1);
	/* Destination */
	if ((rtm->rtm_addrs & RTA_DST) == 0)
		return;		/* ignore routes without destination address */
	sin6_dst = (struct sockaddr_in6 *)rtmp;
	rtmp += ROUNDUP(sin6_dst->sin6_len);
	if (rtm->rtm_addrs & RTA_GATEWAY) {
		sin6_gw = (struct sockaddr_in6 *)rtmp;
		rtmp += ROUNDUP(sin6_gw->sin6_len);
	}
	if (rtm->rtm_addrs & RTA_NETMASK) {
		sin6_mask = (struct sockaddr_in6 *)rtmp;
		rtmp += ROUNDUP(sin6_mask->sin6_len);
	}
	if (rtm->rtm_addrs & RTA_GENMASK) {
		sin6_genmask = (struct sockaddr_in6 *)rtmp;
		rtmp += ROUNDUP(sin6_genmask->sin6_len);
	}
	if (rtm->rtm_addrs & RTA_IFP) {
		sin6_ifp = (struct sockaddr_in6 *)rtmp;
		rtmp += ROUNDUP(sin6_ifp->sin6_len);
	}

	/* Destination */
	if (sin6_dst->sin6_family != AF_INET6)
		return;
	if (IN6_IS_ADDR_LINKLOCAL(&sin6_dst->sin6_addr))
		return;		/* Link-local */
	if (IN6_ARE_ADDR_EQUAL(&sin6_dst->sin6_addr, &in6addr_loopback))
		return;		/* Loopback */
	if (IN6_IS_ADDR_MULTICAST(&sin6_dst->sin6_addr))
		return;

	if ((rrt = MALLOC(struct riprt)) == NULL)
		fatal("malloc: struct riprt");
	memset(rrt, 0, sizeof(*rrt));
	np = &rrt->rrt_info;
	rrt->rrt_same = NULL;
	rrt->rrt_t = time(NULL);
	if (aflag == 0 && (rtm->rtm_flags & RTF_STATIC))
		rrt->rrt_t = 0;	/* Don't age static routes */
	if ((rtm->rtm_flags & (RTF_HOST|RTF_GATEWAY)) == RTF_HOST)
		rrt->rrt_t = 0; /* Don't age non-gateway host routes */
#if 0
	np->rip6_tag = htons(routetag & 0xffff);
#else
	np->rip6_tag = 0;
#endif
	np->rip6_metric = rtm->rtm_rmx.rmx_hopcount;
	if (np->rip6_metric < 1)
		np->rip6_metric = 1;
	rrt->rrt_flags = rtm->rtm_flags;
	np->rip6_dest = sin6_dst->sin6_addr;

	/* Mask or plen */
	if (rtm->rtm_flags & RTF_HOST)
		np->rip6_plen = 128;	/* Host route */
	else if (sin6_mask) {
		np->rip6_plen = mask2len(&sin6_mask->sin6_addr,
			sin6_mask->sin6_len - offsetof(struct sockaddr_in6, sin6_addr));
	} else
		np->rip6_plen = 0;

	if (rtsearch(np)) {
		/* Already found */
		if (!again) {
			trace(1, "route: %s/%d flags %s: already registered\n",
				inet6_n2p(&np->rip6_dest), np->rip6_plen,
				rtflags(rtm));
		}
		free(rrt);
		return;
	}
	/* Gateway */
	if (!sin6_gw)
		memset(&rrt->rrt_gw, 0, sizeof(struct in6_addr));
	else {
		if (sin6_gw->sin6_family == AF_INET6)
			rrt->rrt_gw = sin6_gw->sin6_addr;
		else if (sin6_gw->sin6_family == AF_LINK) {
			/* XXX in case ppp link? */
			rrt->rrt_gw = in6addr_loopback;
		} else
			memset(&rrt->rrt_gw, 0, sizeof(struct in6_addr));
	}
	trace(1, "route: %s/%d flags %s",
		inet6_n2p(&np->rip6_dest), np->rip6_plen, rtflags(rtm));
	trace(1, " gw %s", inet6_n2p(&rrt->rrt_gw));

	/* Interface */
	s = rtm->rtm_index;
	if (s < nindex2ifc && index2ifc[s])
		ifname = index2ifc[s]->ifc_name;
	else {
		trace(1, " not configured\n");
		free(rrt);
		return;
	}
	trace(1, " if %s sock %d", ifname, s);
	rrt->rrt_index = s;

	trace(1, "\n");

	/* Check gateway */
	if (!IN6_IS_ADDR_LINKLOCAL(&rrt->rrt_gw) &&
	    !IN6_IS_ADDR_LOOPBACK(&rrt->rrt_gw)
#ifdef __FreeBSD__
	 && (rrt->rrt_flags & RTF_LOCAL) == 0
#endif
	    ) {
		trace(0, "***** Gateway %s is not a link-local address.\n",
			inet6_n2p(&rrt->rrt_gw));
		trace(0, "*****     dest(%s) if(%s) -- Not optimized.\n",
			inet6_n2p(&rrt->rrt_info.rip6_dest), ifname);
		rrt->rrt_rflags |= RRTF_NH_NOT_LLADDR;
	}

	/* Put it to the route list */
	rrt->rrt_next = riprt;
	riprt = rrt;
}

int
addroute(rrt, gw, ifcp)
	struct riprt *rrt;
	const struct in6_addr *gw;
	struct ifc *ifcp;
{
	struct	netinfo6 *np;
	u_char	buf[BUFSIZ], buf1[BUFSIZ], buf2[BUFSIZ];
	struct	rt_msghdr	*rtm;
	struct	sockaddr_in6	*sin;
	int	len;

	np = &rrt->rrt_info;
	inet_ntop(AF_INET6, (void *)gw, (char *)buf1, sizeof(buf1));
	inet_ntop(AF_INET6, (void *)&ifcp->ifc_mylladdr, (char *)buf2, sizeof(buf2));
	tracet(1, "ADD: %s/%d gw %s [%d] ifa %s\n",
		inet6_n2p(&np->rip6_dest), np->rip6_plen, buf1,
		np->rip6_metric - 1, buf2);
	if (rtlog)
		fprintf(rtlog, "%s: ADD: %s/%d gw %s [%d] ifa %s\n", hms(),
			inet6_n2p(&np->rip6_dest), np->rip6_plen, buf1,
			np->rip6_metric - 1, buf2);
	if (nflag)
		return 0;

	memset(buf, 0, sizeof(buf));
	rtm = (struct rt_msghdr *)buf;
	rtm->rtm_type = RTM_ADD;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_seq = ++seq;
	rtm->rtm_pid = pid;
	rtm->rtm_flags = rrt->rrt_flags;
	rtm->rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
	rtm->rtm_rmx.rmx_hopcount = np->rip6_metric - 1;
	rtm->rtm_inits = RTV_HOPCOUNT;
	sin = (struct sockaddr_in6 *)&buf[sizeof(struct rt_msghdr)];
	/* Destination */
	sin->sin6_len = sizeof(struct sockaddr_in6);
	sin->sin6_family = AF_INET6;
	sin->sin6_addr = np->rip6_dest;
	sin = (struct sockaddr_in6 *)((char *)sin + ROUNDUP(sin->sin6_len));
	/* Gateway */
	sin->sin6_len = sizeof(struct sockaddr_in6);
	sin->sin6_family = AF_INET6;
	sin->sin6_addr = *gw;
	sin = (struct sockaddr_in6 *)((char *)sin + ROUNDUP(sin->sin6_len));
	/* Netmask */
	sin->sin6_len = sizeof(struct sockaddr_in6);
	sin->sin6_family = AF_INET6;
	sin->sin6_addr = *(plen2mask(np->rip6_plen));
	sin = (struct sockaddr_in6 *)((char *)sin + ROUNDUP(sin->sin6_len));

	len = (char *)sin - (char *)buf;
	rtm->rtm_msglen = len;
	if (write(rtsock, buf, len) > 0)
		return 0;

	if (errno == EEXIST) {
		trace(0, "ADD: Route already exists %s/%d gw %s\n",
			inet6_n2p(&np->rip6_dest), np->rip6_plen, buf1);
		if (rtlog)
			fprintf(rtlog, "ADD: Route already exists %s/%d gw %s\n",
				inet6_n2p(&np->rip6_dest), np->rip6_plen, buf1);
	} else {
		trace(0, "Can not write to rtsock (addroute): %s\n",
			strerror(errno));
		if (rtlog)
			fprintf(rtlog, "\tCan not write to rtsock: %s\n",
				strerror(errno));
	}
	return -1;
}

int
delroute(np, gw)
	struct netinfo6 *np;
	struct in6_addr *gw;
{
	u_char	buf[BUFSIZ], buf2[BUFSIZ];
	struct	rt_msghdr	*rtm;
	struct	sockaddr_in6	*sin;
	int	len;

	inet_ntop(AF_INET6, (void *)gw, (char *)buf2, sizeof(buf2));
	tracet(1, "DEL: %s/%d gw %s\n", inet6_n2p(&np->rip6_dest),
		np->rip6_plen, buf2);
	if (rtlog)
		fprintf(rtlog, "%s: DEL: %s/%d gw %s\n",
			hms(), inet6_n2p(&np->rip6_dest), np->rip6_plen, buf2);
	if (nflag)
		return 0;

	memset(buf, 0, sizeof(buf));
	rtm = (struct rt_msghdr *)buf;
	rtm->rtm_type = RTM_DELETE;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_seq = ++seq;
	rtm->rtm_pid = pid;
	rtm->rtm_flags = RTF_UP | RTF_GATEWAY;
	rtm->rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
	sin = (struct sockaddr_in6 *)&buf[sizeof(struct rt_msghdr)];
	/* Destination */
	sin->sin6_len = sizeof(struct sockaddr_in6);
	sin->sin6_family = AF_INET6;
	sin->sin6_addr = np->rip6_dest;
	sin = (struct sockaddr_in6 *)((char *)sin + ROUNDUP(sin->sin6_len));
	/* Gateway */
	sin->sin6_len = sizeof(struct sockaddr_in6);
	sin->sin6_family = AF_INET6;
	sin->sin6_addr = *gw;
	sin = (struct sockaddr_in6 *)((char *)sin + ROUNDUP(sin->sin6_len));
	/* Netmask */
	sin->sin6_len = sizeof(struct sockaddr_in6);
	sin->sin6_family = AF_INET6;
	sin->sin6_addr = *(plen2mask(np->rip6_plen));
	sin = (struct sockaddr_in6 *)((char *)sin + ROUNDUP(sin->sin6_len));

	len = (char *)sin - (char *)buf;
	rtm->rtm_msglen = len;
	if (write(rtsock, buf, len) >= 0)
		return 0;

	if (errno == ESRCH) {
		trace(0, "RTDEL: Route does not exist: %s/%d gw %s\n",
			inet6_n2p(&np->rip6_dest), np->rip6_plen, buf2);
		if (rtlog)
			fprintf(rtlog, "RTDEL: Route does not exist: %s/%d gw %s\n",
				inet6_n2p(&np->rip6_dest), np->rip6_plen, buf2);
	} else {
		trace(0, "Can not write to rtsock (delroute): %s\n",
			strerror(errno));
		if (rtlog)
			fprintf(rtlog, "\tCan not write to rtsock: %s\n",
				strerror(errno));
	}
	return -1;
}

struct in6_addr *
getroute(np, gw)
	struct netinfo6 *np;
	struct in6_addr *gw;
{
	u_char buf[BUFSIZ];
	u_long myseq;
	int len;
	struct rt_msghdr *rtm;
	struct sockaddr_in6 *sin;

	rtm = (struct rt_msghdr *)buf;
	len = sizeof(struct rt_msghdr) + sizeof(struct sockaddr_in6);
	memset(rtm, 0, len);
	rtm->rtm_type = RTM_GET;
	rtm->rtm_version = RTM_VERSION;
	myseq = ++seq;
	rtm->rtm_seq = myseq;
	rtm->rtm_addrs = RTA_DST;
	rtm->rtm_msglen = len;
	sin = (struct sockaddr_in6 *)&buf[sizeof(struct rt_msghdr)];
	sin->sin6_len = sizeof(struct sockaddr_in6);
	sin->sin6_family = AF_INET6;
	sin->sin6_addr = np->rip6_dest;
	if (write(rtsock, buf, len) < 0) {
		if (errno == ESRCH)	/* No such route found */
			return NULL;
		perror("write to rtsock");
		exit(-1);
	}
	do {
		if ((len = read(rtsock, buf, sizeof(buf))) < 0) {
			perror("read from rtsock");
			exit(-1);
		}
		rtm = (struct rt_msghdr *)buf;
	} while (rtm->rtm_seq != myseq || rtm->rtm_pid != pid);
	sin = (struct sockaddr_in6 *)&buf[sizeof(struct rt_msghdr)];
	if (rtm->rtm_addrs & RTA_DST) {
		sin = (struct sockaddr_in6 *)
			((char *)sin + ROUNDUP(sin->sin6_len));
	}
	if (rtm->rtm_addrs & RTA_GATEWAY) {
		*gw = sin->sin6_addr;
		return gw;
	}
	return NULL;
}

const char *
inet6_n2p(p)
	const struct in6_addr *p;
{
	static char buf[BUFSIZ];

	return inet_ntop(AF_INET6, (void *)p, buf, sizeof(buf));
}

void
ifrtdump(sig)
	int sig;
{

	ifdump(sig);
	rtdump(sig);
}

void
ifdump(sig)
	int sig;
{
	struct ifc *ifcp;
	FILE *dump;
	int i;

	if (sig == 0)
		dump = stderr;
	else
		if ((dump = fopen(ROUTE6D_DUMP, "a")) == NULL)
			dump = stderr;

	fprintf(dump, "%s: Interface Table Dump\n", hms());
	fprintf(dump, "  Number of interfaces: %d\n", nifc);
	for (i = 0; i < 2; i++) {
		fprintf(dump, "  %sadvertising interfaces:\n", i ? "non-" : "");
		for (ifcp = ifc; ifcp; ifcp = ifcp->ifc_next) {
			if (i == 0) {
				if ((ifcp->ifc_flags & IFF_UP) == 0)
					continue;
				if (iff_find(ifcp, 'N') != NULL)
					continue;
			} else {
				if (ifcp->ifc_flags & IFF_UP)
					continue;
			}
			ifdump0(dump, ifcp);
		}
	}
	fprintf(dump, "\n");
	if (dump != stderr)
		fclose(dump);
}

void
ifdump0(dump, ifcp)
	FILE *dump;
	const struct ifc *ifcp;
{
	struct ifac *ifa;
	struct iff *iffp;
	char buf[BUFSIZ];
	const char *ft;
	int addr;

	fprintf(dump, "    %s: index(%d) flags(%s) addr(%s) mtu(%d) metric(%d)\n",
		ifcp->ifc_name, ifcp->ifc_index, ifflags(ifcp->ifc_flags),
		inet6_n2p(&ifcp->ifc_mylladdr),
		ifcp->ifc_mtu, ifcp->ifc_metric);
	for (ifa = ifcp->ifc_addr; ifa; ifa = ifa->ifa_next) {
		if (ifcp->ifc_flags & IFF_POINTOPOINT) {
			inet_ntop(AF_INET6, (void *)&ifa->ifa_raddr,
				buf, sizeof(buf));
			fprintf(dump, "\t%s/%d -- %s\n",
				inet6_n2p(&ifa->ifa_addr),
				ifa->ifa_plen, buf);
		} else {
			fprintf(dump, "\t%s/%d\n",
				inet6_n2p(&ifa->ifa_addr),
				ifa->ifa_plen);
		}
	}
	if (ifcp->ifc_filter) {
		fprintf(dump, "\tFilter:");
		for (iffp = ifcp->ifc_filter; iffp; iffp = iffp->iff_next) {
			addr = 0;
			switch (iffp->iff_type) {
			case 'A':
				ft = "Aggregate"; addr++; break;
			case 'N':
				ft = "No-advertise"; break;
			case 'O':
				ft = "Advertise-only"; addr++; break;
			case 'T':
				ft = "Default-only"; break;
			case 'L':
				ft = "Listen-only"; addr++; break;
			default:
				snprintf(buf, sizeof(buf), "Unknown-%c", iffp->iff_type);
				ft = buf;
				addr++;
				break;
			}
			fprintf(dump, " %s", ft);
			if (addr) {
				fprintf(dump, "(%s/%d)", inet6_n2p(&iffp->iff_addr),
					iffp->iff_plen);
			}
		}
		fprintf(dump, "\n");
	}
}

void
rtdump(sig)
	int sig;
{
	struct	riprt *rrt;
	char	buf[BUFSIZ];
	FILE	*dump;
	time_t	t, age;

	if (sig == 0)
		dump = stderr;
	else
		if ((dump = fopen(ROUTE6D_DUMP, "a")) == NULL)
			dump = stderr;

	t = time(NULL);
	fprintf(dump, "\n%s: Routing Table Dump\n", hms());
	for (rrt = riprt; rrt; rrt = rrt->rrt_next) {
		if (rrt->rrt_t == 0)
			age = 0;
		else
			age = t - rrt->rrt_t;
		inet_ntop(AF_INET6, (void *)&rrt->rrt_info.rip6_dest,
			buf, sizeof(buf));
		fprintf(dump, "    %s/%d if(%d:%s) gw(%s) [%d] age(%ld)",
			buf, rrt->rrt_info.rip6_plen, rrt->rrt_index,
			index2ifc[rrt->rrt_index]->ifc_name,
			inet6_n2p(&rrt->rrt_gw),
			rrt->rrt_info.rip6_metric, (long)age);
		if (rrt->rrt_info.rip6_tag) {
			fprintf(dump, " tag(0x%04x)",
				ntohs(rrt->rrt_info.rip6_tag) & 0xffff);
		}
		if (rrt->rrt_rflags & RRTF_NH_NOT_LLADDR)
			fprintf(dump, " NOT-LL");
		if (rrt->rrt_rflags & RRTF_NOADVERTISE)
			fprintf(dump, " NO-ADV");
		fprintf(dump, "\n");
	}
	fprintf(dump, "\n");
	if (dump != stderr)
		fclose(dump);
}

/*
 * Parse the -A (and -O) options and put corresponding filter object to the
 * specified interface structures. Each of the -A/O option has the following
 * syntax:	-A 5f09:c400::/32,ef0,ef1  (aggregate)
 * 		-O 5f09:c400::/32,ef0,ef1  (only when match)
 */
void
filterconfig()
{
	int i;
	char *p, *ap, *iflp, *ifname;
	struct	iff ftmp, *iff_obj;
	struct	ifc *ifcp;
	struct	riprt *rrt;
#if 0
	struct	in6_addr gw;
#endif

	for (i = 0; i < nfilter; i++) {
		ap = filter[i];
		iflp = NULL;
		ifcp = NULL;
		if (filtertype[i] == 'N' || filtertype[i] == 'T') {
			iflp = ap;
			goto ifonly;
		}
		if ((p = index(ap, ',')) != NULL) {
			*p++ = '\0';
			iflp = p;
		}
		if ((p = index(ap, '/')) == NULL)
			fatal("no prefixlen specified for '%s'", ap);
		*p++ = '\0';
		if (inet_pton(AF_INET6, ap, &ftmp.iff_addr) != 1)
			fatal("invalid prefix specified for '%s'", ap);
		ftmp.iff_plen = atoi(p);
		ftmp.iff_next = NULL;
		applyplen(&ftmp.iff_addr, ftmp.iff_plen);
ifonly:
		ftmp.iff_type = filtertype[i];
		if (iflp == NULL || *iflp == '\0')
			fatal("no interface specified for '%s'", ap);
		/* parse the interface listing portion */
		while (iflp) {
			ifname = iflp;
			if ((iflp = index(iflp, ',')) != NULL)
				*iflp++ = '\0';
			ifcp = ifc_find(ifname);
			if (ifcp == NULL)
				fatal("no interface %s exists", ifname);
			iff_obj = (struct iff *)malloc(sizeof(struct iff));
			if (iff_obj == NULL)
				fatal("malloc of iff_obj");
			memcpy((void *)iff_obj, (void *)&ftmp,
				sizeof(struct iff));
			/* link it to the interface filter */
			iff_obj->iff_next = ifcp->ifc_filter;
			ifcp->ifc_filter = iff_obj;
		}
		if (filtertype[i] != 'A')
			continue;
		/* put the aggregate to the kernel routing table */
		rrt = (struct riprt *)malloc(sizeof(struct riprt));
		if (rrt == NULL)
			fatal("malloc: rrt");
		memset(rrt, 0, sizeof(struct riprt));
		rrt->rrt_info.rip6_dest = ftmp.iff_addr;
		rrt->rrt_info.rip6_plen = ftmp.iff_plen;
		rrt->rrt_info.rip6_metric = 1;
		rrt->rrt_info.rip6_tag = htons(routetag & 0xffff);
		rrt->rrt_gw = in6addr_loopback;
		rrt->rrt_flags = RTF_UP | RTF_REJECT;
		rrt->rrt_rflags = RRTF_AGGREGATE;
		rrt->rrt_t = 0;
		rrt->rrt_index = loopifindex;
#if 0
		if (getroute(&rrt->rrt_info, &gw)) {
#if 0
			/*
			 * When the address has already been registered in the
			 * kernel routing table, it should be removed 
			 */
			delroute(&rrt->rrt_info, &gw);
#else
			/* it is more safe behavior */
			errno = EINVAL;
			fatal("%s/%u already in routing table, "
			    "cannot aggregate",
			    inet6_n2p(&rrt->rrt_info.rip6_dest),
			    rrt->rrt_info.rip6_plen);
#endif
		}
#endif
		/* Put the route to the list */
		rrt->rrt_next = riprt;
		riprt = rrt;
		trace(1, "Aggregate: %s/%d for %s\n",
			inet6_n2p(&ftmp.iff_addr), ftmp.iff_plen,
			ifcp->ifc_name);
		/* Add this route to the kernel */
		if (nflag) 	/* do not modify kernel routing table */
			continue;
		addroute(rrt, &in6addr_loopback, loopifcp);
	}
}

/***************** utility functions *****************/

/*
 * Returns a pointer to ifac whose address and prefix length matches
 * with the address and prefix length specified in the arguments.
 */
struct ifac *
ifa_match(ifcp, ia, plen)
	const struct ifc *ifcp;
	const struct in6_addr *ia;
	int plen;
{
	struct ifac *ifa;

	for (ifa = ifcp->ifc_addr; ifa; ifa = ifa->ifa_next) {
		if (IN6_ARE_ADDR_EQUAL(&ifa->ifa_addr, ia) &&
		    ifa->ifa_plen == plen)
			break;
	}
	return ifa;
}

/*
 * Return a pointer to riprt structure whose address and prefix length
 * matches with the address and prefix length found in the argument.
 * Note: This is not a rtalloc(). Therefore exact match is necessary.
 */

struct riprt *
rtsearch(np)
	struct	netinfo6 *np;
{
	struct	riprt	*rrt;

	for (rrt = riprt; rrt; rrt = rrt->rrt_next) {
		if (rrt->rrt_info.rip6_plen == np->rip6_plen &&
		    IN6_ARE_ADDR_EQUAL(&rrt->rrt_info.rip6_dest,
				       &np->rip6_dest))
			return rrt;
	}
	return 0;
}

int
mask2len(addr, lenlim)
	const struct in6_addr *addr;
	int lenlim;
{
	int i = 0, j;
	u_char *p = (u_char *)addr;
	
	for (j = 0; j < lenlim; j++, p++) {
		if (*p != 0xff)
			break;
		i += 8;
	}
	if (j < lenlim) {
		switch (*p) {
#define	MASKLEN(m, l)	case m: do { i += l; break; } while (0)
		MASKLEN(0xfe, 7); break;
		MASKLEN(0xfc, 6); break;
		MASKLEN(0xf8, 5); break;
		MASKLEN(0xf0, 4); break;
		MASKLEN(0xe0, 3); break;
		MASKLEN(0xc0, 2); break;
		MASKLEN(0x80, 1); break;
#undef	MASKLEN
		}
	}
	return i;
}

void
applymask(addr, mask)
	struct in6_addr *addr, *mask;
{
	int	i;
	u_long	*p, *q;

	p = (u_long *)addr; q = (u_long *)mask;
	for (i = 0; i < 4; i++)
		*p++ &= *q++;
}

static const u_char plent[8] = {
	0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe
};

void
applyplen(ia, plen)
	struct	in6_addr *ia;
	int	plen;
{
	u_char	*p;
	int	i;

	p = ia->s6_addr;
	for (i = 0; i < 16; i++) {
		if (plen <= 0)
			*p = 0;
		else if (plen < 8)
			*p &= plent[plen];
		p++, plen -= 8;
	}
}

static const int pl2m[9] = {
	0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff
};

struct in6_addr *
plen2mask(n)
	int	n;
{
	static struct in6_addr ia;
	u_char	*p;
	int	i;

	memset(&ia, 0, sizeof(struct in6_addr));
	p = (u_char *)&ia;
	for (i = 0; i < 16; i++, p++, n -= 8) {
		if (n >= 8) {
			*p = 0xff;
			continue;
		}
		*p = pl2m[n];
		break;
	}
	return &ia;
}

char *
allocopy(p)
	char *p;
{
	char *q = (char *)malloc(strlen(p) + 1);

	strcpy(q, p);
	return q;
}

char *
hms()
{
	static char buf[BUFSIZ];
	time_t t;
	struct	tm *tm;

	t = time(NULL);
	if ((tm = localtime(&t)) == 0)
		fatal("localtime");
	snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
	return buf;
}

#define	RIPRANDDEV	1.0	/* 30 +- 15, max - min = 30 */

int
ripinterval(timer)
	int timer;
{
	double r = rand();

	interval = (int)(timer + timer * RIPRANDDEV * (r / RAND_MAX - 0.5));
	nextalarm = time(NULL) + interval;
	return interval;
}

time_t
ripsuptrig()
{
	time_t t;

	double r = rand();
	t  = (int)(RIP_TRIG_INT6_MIN + 
		(RIP_TRIG_INT6_MAX - RIP_TRIG_INT6_MIN) * (r / RAND_MAX ));
	sup_trig_update = time(NULL) + t;
	return t;
}

void
#ifdef __STDC__
fatal(const char *fmt, ...)
#else
fatal(fmt, va_alist)
	char	*fmt;
	va_dcl
#endif
{
	va_list ap;
	char buf[1024];

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vsnprintf(buf, sizeof(buf), fmt, ap);
	perror(buf);
	syslog(LOG_ERR, "%s: %s", buf, strerror(errno));
	rtdexit(0);
	va_end(ap);
}

void
#ifdef __STDC__
tracet(int level, const char *fmt, ...)
#else
tracet(level, fmt, va_alist)
	int level;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (level <= dflag) {
		fprintf(stderr, "%s: ", hms());
		vfprintf(stderr, fmt, ap);
	}
	if (dflag) {
		if (level > 0)
			vsyslog(LOG_DEBUG, fmt, ap);
		else
			vsyslog(LOG_WARNING, fmt, ap);
	}
	va_end(ap);
}

void
#ifdef __STDC__
trace(int level, const char *fmt, ...)
#else
trace(level, fmt, va_alist)
	int level;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	if (level <= dflag)
		vfprintf(stderr, fmt, ap);
	if (dflag) {
		if (level > 0)
			vsyslog(LOG_DEBUG, fmt, ap);
		else
			vsyslog(LOG_WARNING, fmt, ap);
	}
	va_end(ap);
}

unsigned int
if_maxindex()
{
	struct if_nameindex *p, *p0;
	unsigned int max = 0;

	p0 = if_nameindex();
	for (p = p0; p && p->if_index && p->if_name; p++) {
		if (max < p->if_index)
			max = p->if_index;
	}
	if_freenameindex(p0);
	return max;
}

struct ifc *
ifc_find(name)
	char *name;
{
	struct ifc *ifcp;

	for (ifcp = ifc; ifcp; ifcp = ifcp->ifc_next) {
		if (strcmp(name, ifcp->ifc_name) == 0)
			return ifcp;
	}
	return (struct ifc *)NULL;
}

struct iff *
iff_find(ifcp, type)
	struct ifc *ifcp;
	int type;
{
	struct iff *iffp;

	for (iffp = ifcp->ifc_filter; iffp; iffp = iffp->iff_next) {
		if (iffp->iff_type == type)
			return iffp;
	}
	return NULL;
}

void
setindex2ifc(index, ifcp)
	int index;
	struct ifc *ifcp;
{
	int n;
	struct ifc **p;

	if (!index2ifc) {
		nindex2ifc = 5;	/*initial guess*/
		index2ifc = (struct ifc **)
			malloc(sizeof(*index2ifc) * nindex2ifc);
		if (index2ifc == NULL)
			fatal("malloc");
		memset(index2ifc, 0, sizeof(*index2ifc) * nindex2ifc);
	}
	n = nindex2ifc;
	while (nindex2ifc <= index)
		nindex2ifc *= 2;
	if (n != nindex2ifc) {
		p = (struct ifc **)realloc(index2ifc,
		    sizeof(*index2ifc) * nindex2ifc);
		if (p == NULL)
			fatal("realloc");
		index2ifc = p;
	}
	index2ifc[index] = ifcp;
}
