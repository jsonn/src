/*	$NetBSD: ping6.c,v 1.6 1999/07/04 02:46:28 itojun Exp $	*/

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

/*	BSDI	ping.c,v 2.3 1996/01/21 17:56:50 jch Exp	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
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

#if 0
#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ping.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */
#else
#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD");
#endif
#endif

/*
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
 */

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/ah.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#endif

#define MAXPACKETLEN	131072
#define	IP6LEN		40
#define ICMP6ECHOLEN	8	/* icmp echo header len excluding time */
#define ICMP6ECHOTMLEN sizeof(struct timeval)
#define ICMP6_NIQLEN	(ICMP6ECHOLEN + 8)
#define ICMP6_NIRLEN	(ICMP6ECHOLEN + 12) /* 64 bits of nonce + 32 bits ttl */
#define	EXTRA		256	/* for AH and various other headers. weird. */
#define	DEFDATALEN	ICMP6ECHOTMLEN
#define MAXDATALEN	MAXPACKETLEN - IP6LEN - ICMP6ECHOLEN
#define	NROUTES		9		/* number of record route slots */

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

#define	F_FLOOD		0x0001
#define	F_INTERVAL	0x0002
#define	F_NUMERIC	0x0004
#define	F_PINGFILLED	0x0008
#define	F_QUIET		0x0010
#define	F_RROUTE	0x0020
#define	F_SO_DEBUG	0x0040
#define	F_VERBOSE	0x0100
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
#define	F_POLICY	0x4000
#else
#define F_AUTHHDR	0x0200
#define F_ENCRYPT	0x0400
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
#define F_NODEADDR	0x0800
#define F_FQDN		0x1000
#define F_INTERFACE	0x2000
u_int options;

#define IN6LEN		sizeof(struct in6_addr)
#define SA6LEN		sizeof(struct sockaddr_in6)
#define DUMMY_PORT      10101

#define SIN6(s) ((struct sockaddr_in6 *)(s))

/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 */
#define	MAX_DUP_CHK	(8 * 8192)
int mx_dup_ck = MAX_DUP_CHK;
char rcvd_tbl[MAX_DUP_CHK / 8];

struct addrinfo *res;	        
struct sockaddr_in6 dst;        /* who to ping6 */
struct sockaddr_in6 src;        /* src addr of this packet */
int datalen = DEFDATALEN;
int s;				/* socket file descriptor */
u_char outpack[MAXPACKETLEN];
char BSPACE = '\b';		/* characters written for flood */
char DOT = '.';
char *hostname;
int ident;			/* process id to identify our packets */

/* counters */
long npackets;			/* max packets to transmit */
long nreceived;			/* # of packets we got back */
long nrepeats;			/* number of duplicates */
long ntransmitted;		/* sequence # for outbound packets = #sent */
int interval = 1;		/* interval between packets */
int hoplimit = -1;		/* hoplimit */

/* timing */
int timing;			/* flag to do timing */
double tmin = 999999999.0;	/* minimum round trip time */
double tmax = 0.0;		/* maximum round trip time */
double tsum = 0.0;		/* sum of all times, for doing average */

/* for inet_ntop() */
char ntop_buf[INET6_ADDRSTRLEN];

/* for node addresses */
u_short naflags;

/* for ancillary data(advanced API) */
struct msghdr smsghdr;
struct iovec smsgiov;
char *scmsg = 0;

int	 main __P((int, char *[]));
void	 fill __P((char *, char *));
int	 get_hoplim __P((struct msghdr *));
void	 onalrm __P((int));
void	 oninfo __P((int));
void	 onint __P((int));
void	 pinger __P((void));
char	*pr_addr __P((struct in6_addr *));
void	 pr_icmph __P((struct icmp6_hdr *, u_char *));
void	 pr_iph __P((struct ip6_hdr *));
void	 pr_nodeaddr __P((struct icmp6_nodeinfo *, int));
void	 pr_pack __P((u_char *, int, struct msghdr *));
void	 pr_retip __P((struct ip6_hdr *, u_char *));
void	 summary __P((void));
void	 tvsub __P((struct timeval *, struct timeval *));
void	 usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int errno, optind;
	extern char *optarg;
	struct itimerval itimer;
	struct sockaddr_in6 from;
	struct timeval timeout;
	struct addrinfo hints;
	fd_set fdset;
	register int cc, i;
	int ch, fromlen, hold, packlen, preload, optval, ret_ga;
	u_char *datap, *packet;
	char *e, *target, *ifname = NULL;
	int ip6optlen = 0;
	struct cmsghdr *scmsgp = NULL;
	int sockbufsize = 0;
#ifdef IPSEC_POLICY_IPSEC
	char *policy = NULL;
#endif

	/* just to be sure */
	memset(&smsghdr, 0, sizeof(&smsghdr));
	memset(&smsgiov, 0, sizeof(&smsgiov));

	preload = 0;
	datap = &outpack[ICMP6ECHOLEN + ICMP6ECHOTMLEN];
#ifndef IPSEC
	while ((ch = getopt(argc, argv, "a:b:c:dfh:I:i:l:np:qRrs:vwW")) != EOF)
#else
#ifdef IPSEC_POLICY_IPSEC
	while ((ch = getopt(argc, argv, "a:b:c:dfh:I:i:l:np:qRrs:vwWP:")) != EOF)
#else
	while ((ch = getopt(argc, argv, "a:b:c:dfh:I:i:l:np:qRrs:vwWAE")) != EOF)
#endif /*IPSEC_POLICY_IPSEC*/
#endif
		switch(ch) {
		 case 'a':
		 {
			 u_char *cp;

			 options |= F_NODEADDR;
			 datalen = 2048; /* XXX: enough? */
			 for (cp = optarg; *cp != '\0'; cp++) {
				 switch(*cp) {
				  case 'a':
				  case 'A':
					  naflags |= NI_NODEADDR_FLAG_ALL;
					  break;
				  case 'l':
				  case 'L':
					  naflags |= NI_NODEADDR_FLAG_LINKLOCAL;
					  break;
				  case 's':
				  case 'S':
					  naflags |= NI_NODEADDR_FLAG_SITELOCAL;
					  break;
				  case 'g':
				  case 'G':
					  naflags |= NI_NODEADDR_FLAG_GLOBAL;
					  break;
				  default:
					  usage();
				 }
			 }
			 break;
		 }
		 case 'b':
#if defined(SO_SNDBUF) && defined(SO_RCVBUF)
			sockbufsize = atoi(optarg);
#else
			err(1,
"-b option ignored: SO_SNDBUF/SO_RCVBUF socket options not supported");
#endif
			break;
		case 'c':
			npackets = strtol(optarg, &e, 10);
			if (npackets <= 0 || *optarg == '\0' || *e != '\0')
				errx(1,
				    "illegal number of packets -- %s", optarg);
			break;
		case 'd':
			options |= F_SO_DEBUG;
			break;
		case 'f':
			if (getuid()) {
				errno = EPERM;
				errx(1, "Must be superuser to flood ping");
			}
			options |= F_FLOOD;
			setbuf(stdout, (char *)NULL);
			break;
		case 'h':		/* hoplimit */
			hoplimit = strtol(optarg, &e, 10);
			if (255 < hoplimit || hoplimit < -1)
				errx(1,
				    "illegal hoplimit -- %s", optarg);
			break;
		case 'I':
			ifname = optarg;
			options |= F_INTERFACE;
			break;
		case 'i':		/* wait between sending packets */
			interval = strtol(optarg, &e, 10);
			if (interval <= 0 || *optarg == '\0' || *e != '\0')
				errx(1,
				    "illegal timing interval -- %s", optarg);
			options |= F_INTERVAL;
			break;
		case 'l':
			preload = strtol(optarg, &e, 10);
			if (preload < 0 || *optarg == '\0' || *e != '\0')
				errx(1, "illegal preload value -- %s", optarg);
			break;
		case 'n':
			options |= F_NUMERIC;
			break;
		case 'p':		/* fill buffer with user pattern */
			options |= F_PINGFILLED;
			fill((char *)datap, optarg);
				break;
		case 'q':
			options |= F_QUIET;
			break;
		case 'R':
			options |= F_RROUTE;
			break;
		case 's':		/* size of packet to send */
			datalen = strtol(optarg, &e, 10);
			if (datalen <= 0 || *optarg == '\0' || *e != '\0')
				errx(1, "illegal datalen value -- %s", optarg);
			if (datalen > MAXDATALEN)
				errx(1,
				    "datalen value too large, maximum is %d",
				    MAXDATALEN);
			break;
		case 'v':
			options |= F_VERBOSE;
			break;
		case 'w':
		case 'W':
			options |= F_FQDN;
			break;
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
		case 'P':
			options |= F_POLICY;
			policy = strdup(optarg);
			break;
#else
		case 'A':
			options |= F_AUTHHDR;
			break;
		case 'E':
			options |= F_ENCRYPT;
			break;
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if (argc > 1)
		ip6optlen += inet6_rthdr_space(IPV6_RTHDR_TYPE_0, argc - 1);

	target = argv[argc - 1];

	/* getaddrinfo */
	bzero(&hints, sizeof(struct addrinfo));
	if ((options & F_NUMERIC) == 0)
		hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMPV6;

	ret_ga = getaddrinfo(target, NULL, &hints, &res);
	if (ret_ga) {
		fprintf(stderr, "ping6: %s\n", gai_strerror(ret_ga));
		exit(1);
	}
	if (res->ai_canonname)
		hostname = res->ai_canonname;
	else
		hostname = target;
		
	if (!res->ai_addr)
		errx(1, "getaddrinfo failed");

	(void)memcpy(&dst, res->ai_addr, res->ai_addrlen);

	if (options & F_FLOOD && options & F_INTERVAL)
		errx(1, "-f and -i incompatible options");

	if (datalen >= sizeof(struct timeval))	/* can we time transfer */
		timing = 1;
	packlen = datalen + IP6LEN + ICMP6ECHOLEN + EXTRA;
	if (!(packet = (u_char *)malloc((u_int)packlen)))
		err(1, "Unable to allocate packet");
	if (!(options & F_PINGFILLED))
		for (i = 8; i < datalen; ++i)
			*datap++ = i;

	ident = getpid() & 0xFFFF;

	if ((s = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
	  err(1, "socket");

	hold = 1;

	if (options & F_SO_DEBUG)
		(void)setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&hold,
		    sizeof(hold));
	optval = IPV6_DEFHLIM;
	if (IN6_IS_ADDR_MULTICAST(&dst.sin6_addr))
		if (setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
			       &optval, sizeof(optval)) == -1)
			err(1, "IPV6_MULTICAST_HOPS");

#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
	if (options & F_POLICY) {
		int len;
		char *buf;
		if ((len = ipsec_get_policylen(policy)) < 0)
			errx(1, ipsec_strerror());
		if ((buf = malloc(len)) == NULL)
			err(1, "malloc");
		if ((len = ipsec_set_policy(buf, len, policy)) < 0)
			errx(1, ipsec_strerror());
		if (setsockopt(s, IPPROTO_IPV6, IPV6_IPSEC_POLICY, buf, len) < 0)
			warnx("Unable to set IPSec policy");
		free(buf);
	}
#else
	if (options & F_AUTHHDR) {
		optval = IPSEC_LEVEL_REQUIRE;
#ifdef IPV6_AUTH_TRANS_LEVEL
		if (setsockopt(s, IPPROTO_IPV6, IPV6_AUTH_TRANS_LEVEL,
				&optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_AUTH_TRANS_LEVEL)");
#else /* old def */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_AUTH_LEVEL,
				&optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_AUTH_LEVEL)");
#endif
	}
	if (options & F_ENCRYPT) {
		optval = IPSEC_LEVEL_REQUIRE;
		if (setsockopt(s, IPPROTO_IPV6, IPV6_ESP_TRANS_LEVEL,
				&optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_ESP_TRANS_LEVEL)");
	}
#endif /*IPSEC_POLICY_IPSEC*/
#endif

#ifdef ICMP6_FILTER
    {
	struct icmp6_filter filt;
	if (!(options & F_VERBOSE)) {
		ICMP6_FILTER_SETBLOCKALL(&filt);
		if ((options & F_FQDN) || (options & F_NODEADDR))
			ICMP6_FILTER_SETPASS(ICMP6_NI_REPLY, &filt);
		else
			ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filt);
	} else {
		ICMP6_FILTER_SETPASSALL(&filt);
	}
	if (setsockopt(s, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
			sizeof(filt)) < 0)
		err(1, "setsockopt(ICMP6_FILTER)");
    }
#endif /*ICMP6_FILTER*/

/*
	optval = 1; 
	if (IN6_IS_ADDR_MULTICAST(&dst.sin6_addr))
		if (setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
			       &optval, sizeof(optval)) == -1)
			err(1, "IPV6_MULTICAST_LOOP");
*/
	/* record route option */
	if (options & F_RROUTE)
		errx(1, "record route not available in this implementation");

	/* Outgoing interface */
#ifndef SIN6_IFINDEX
	if (options & F_INTERFACE)
		ip6optlen += CMSG_SPACE(sizeof(struct in6_pktinfo));
#endif

	if (hoplimit != -1)
		ip6optlen += CMSG_SPACE(sizeof(int));

	/* set IP6 packet options */
	if (ip6optlen) {
		if ((scmsg = (char *)malloc(ip6optlen)) == 0)
			errx(1, "can't allocate enough memory");
		smsghdr.msg_control = (caddr_t)scmsg;
		smsghdr.msg_controllen = ip6optlen;
		scmsgp = (struct cmsghdr *)scmsg;
	}
	if (options & F_INTERFACE) {
#ifndef SIN6_IFINDEX
		struct in6_pktinfo *pktinfo =
			(struct in6_pktinfo *)(CMSG_DATA(scmsgp));

		if ((pktinfo->ipi6_ifindex = if_nametoindex(ifname)) == 0)
			errx(1, "%s: invalid interface name", ifname);
		bzero(&pktinfo->ipi6_addr, sizeof(struct in6_addr));
		scmsgp->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
		scmsgp->cmsg_level = IPPROTO_IPV6;
		scmsgp->cmsg_type = IPV6_PKTINFO;

		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
#else
		if ((dst.sin6_ifindex = if_nametoindex(ifname)) == 0)
			errx(1, "%s: invalid interface name", ifname);
#endif
	}
	if (hoplimit != -1) {
		scmsgp->cmsg_len = CMSG_LEN(sizeof(int));
		scmsgp->cmsg_level = IPPROTO_IPV6;
		scmsgp->cmsg_type = IPV6_HOPLIMIT;
		*(int *)(CMSG_DATA(scmsgp)) = hoplimit;

		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
	}
	if (argc > 1) {	/* some intermediate addrs are specified */
		int hops, error;
		
		if ((scmsgp = (struct cmsghdr *)inet6_rthdr_init(scmsgp,
								 IPV6_RTHDR_TYPE_0)) == 0)
			errx(1, "can't initialize rthdr");

		for (hops = 0; hops < argc - 1; hops++) {
			struct addrinfo *iaip;

			if ((error = getaddrinfo(argv[hops], NULL, &hints, &iaip)))
				errx(1, gai_strerror(error));
			if (SIN6(res->ai_addr)->sin6_family != AF_INET6)
				errx(1,
				     "bad addr family of an intermediate addr");

			if (inet6_rthdr_add(scmsgp,
					    &(SIN6(iaip->ai_addr))->sin6_addr,
					    IPV6_RTHDR_LOOSE))
				errx(1, "can't add an intermediate node");
		}

		if (inet6_rthdr_lasthop(scmsgp, IPV6_RTHDR_LOOSE))
			errx(1, "can't set the last flag");

		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
	}

	{
		/* 
		 * source selection
		 */
		int dummy, len = sizeof(src);
		
		if ((dummy = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
			err(1, "UDP socket");

		src.sin6_family = AF_INET6;
		src.sin6_addr = dst.sin6_addr;
		src.sin6_port = ntohs(DUMMY_PORT);

#ifndef SIN6_IFINDEX
		if (setsockopt(dummy, IPPROTO_IPV6, IPV6_PKTOPTIONS,
			       (void *)smsghdr.msg_control,
			       smsghdr.msg_controllen)) {
			err(1, "UDP setsockopt");
		}
#else
		src.sin6_ifindex = dst.sin6_ifindex;
#endif
				
		if (connect(dummy, (struct sockaddr *)&src, len) < 0)
			err(1, "UDP connect");
	
		if (getsockname(dummy, (struct sockaddr *)&src, &len) < 0)
			err(1, "getsockname");

		close(dummy);
	}

#if defined(SO_SNDBUF) && defined(SO_RCVBUF)
	if (sockbufsize) {
		if (datalen > sockbufsize)
			warnx("you need -b to increae socket buffer size");
		if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sockbufsize,
			       sizeof(sockbufsize)) < 0)
			err(1, "setsockopt(SO_SNDBUF)");
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &sockbufsize,
			       sizeof(sockbufsize)) < 0)
			err(1, "setsockopt(SO_RCVBUF)");
	}
	else {
		if (datalen > 8 * 1024)	/*XXX*/
			warnx("you need -b to increase socket buffer size");
		/*
		 * When pinging the broadcast address, you can get a lot of
		 * answers. Doing something so evil is useful if you are trying
		 * to stress the ethernet, or just want to fill the arp cache
		 * to get some stuff for /etc/ethers.
		 */
		hold = 48 * 1024;
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&hold, sizeof(hold));
	}
#endif

	optval = 1;
#ifndef SIN6_IFINDEX
	setsockopt(s, IPPROTO_IPV6, IPV6_PKTINFO, &optval, sizeof(optval));
#endif
	setsockopt(s, IPPROTO_IPV6, IPV6_HOPLIMIT, &optval, sizeof(optval));

	printf("PING6(%d=40+8+%d bytes) ", datalen + 48, datalen);
	printf("%s --> ", inet_ntop(AF_INET6, &src.sin6_addr, ntop_buf, sizeof(ntop_buf)));
	printf("%s\n", inet_ntop(AF_INET6, &dst.sin6_addr, ntop_buf, sizeof(ntop_buf)));

	while (preload--)		/* Fire off them quickies. */
		pinger();

	(void)signal(SIGINT, onint);
	(void)signal(SIGINFO, oninfo);

	if ((options & F_FLOOD) == 0) {
		(void)signal(SIGALRM, onalrm);
		itimer.it_interval.tv_sec = interval;
		itimer.it_interval.tv_usec = 0;
		itimer.it_value.tv_sec = 0;
		itimer.it_value.tv_usec = 1;
		(void)setitimer(ITIMER_REAL, &itimer, NULL);
	}

	FD_ZERO(&fdset);
	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;
	for (;;) {
		struct msghdr m;
		struct cmsghdr *cm;
		u_char buf[256];
		struct iovec iov[2];

		if (options & F_FLOOD) {
			pinger();
			FD_SET(s, &fdset);
			if (select(s + 1, &fdset, NULL, NULL, &timeout) < 1)
				continue;
		}
		fromlen = sizeof(from);

		m.msg_name = (caddr_t)&from;
		m.msg_namelen = sizeof(from);
		memset(&iov, 0, sizeof(iov));
		iov[0].iov_base = (caddr_t)packet;
		iov[0].iov_len = packlen;
		m.msg_iov = iov;
		m.msg_iovlen = 1;
		cm = (struct cmsghdr *)buf;
		m.msg_control = (caddr_t)buf;
		m.msg_controllen = sizeof(buf);

		if ((cc = recvmsg(s, &m, 0)) < 0) {
			if (errno == EINTR)
				continue;
			warn("recvfrom");
			continue;
		}

		pr_pack(packet, cc, &m);
		if (npackets && nreceived >= npackets)
			break;
	}
	summary();
	exit(nreceived == 0);
}

/*
 * onalrm --
 *	This routine transmits another ping6.
 */
void
onalrm(signo)
	int signo;
{
	struct itimerval itimer;

	if (!npackets || ntransmitted < npackets) {
		pinger();
		return;
	}

	/*
	 * If we're not transmitting any more packets, change the timer
	 * to wait two round-trip times if we've received any packets or
	 * ten seconds if we haven't.
	 */
#define	MAXWAIT		10
	if (nreceived) {
		itimer.it_value.tv_sec =  2 * tmax / 1000;
		if (itimer.it_value.tv_sec == 0)
			itimer.it_value.tv_sec = 1;
	} else
		itimer.it_value.tv_sec = MAXWAIT;
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = 0;
	itimer.it_value.tv_usec = 0;

	(void)signal(SIGALRM, onint);
	(void)setitimer(ITIMER_REAL, &itimer, NULL);
}

/*
 * pinger --
 *	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
void
pinger()
{
	struct icmp6_hdr *icp;
	struct iovec iov[2];
	int i, cc;

	icp = (struct icmp6_hdr *)outpack;
	icp->icmp6_code = 0;
	icp->icmp6_cksum = 0;
	icp->icmp6_seq = ntransmitted++;
	icp->icmp6_id = ident;			/* ID */

	CLR(icp->icmp6_seq % mx_dup_ck);

	if (options & F_FQDN) {
		icp->icmp6_type = ICMP6_NI_QUERY;
		/* code field is always 0 */
		/* XXX: overwrite icmp6_id */
		icp->icmp6_data16[0] = htons(NI_QTYPE_FQDN);
		if (timing)
			(void)gettimeofday((struct timeval *)
					   &outpack[ICMP6ECHOLEN], NULL);
		cc = ICMP6_NIQLEN;
		datalen = 0;
	} else if (options & F_NODEADDR) {
		icp->icmp6_type = ICMP6_NI_QUERY;
		/* code field is always 0 */
		/* XXX: overwrite icmp6_id */
		icp->icmp6_data16[0] = htons(NI_QTYPE_NODEADDR);
		if (timing)
			(void)gettimeofday((struct timeval *)
					   &outpack[ICMP6ECHOLEN], NULL);
		cc = ICMP6_NIQLEN;
		datalen = 0;
		((struct icmp6_nodeinfo *)icp)->ni_flags = naflags;
	}
	else {
		icp->icmp6_type = ICMP6_ECHO_REQUEST;
		if (timing)
			(void)gettimeofday((struct timeval *)
					   &outpack[ICMP6ECHOLEN], NULL);
		cc = ICMP6ECHOLEN + datalen;
	}

	smsghdr.msg_name = (caddr_t)&dst;
	smsghdr.msg_namelen = sizeof(dst);
	memset(&iov, 0, sizeof(iov));
	iov[0].iov_base = (caddr_t)outpack;
	iov[0].iov_len = cc;
	smsghdr.msg_iov = iov;
	smsghdr.msg_iovlen = 1;

	i = sendmsg(s, &smsghdr, 0);

	if (i < 0 || i != cc)  {
		if (i < 0)
			warn("sendmsg");
		(void)printf("ping6: wrote %s %d chars, ret=%d\n",
		    hostname, cc, i);
	}
	if (!(options & F_QUIET) && options & F_FLOOD)
		(void)write(STDOUT_FILENO, &DOT, 1);
}

/*
 * pr_pack --
 *	Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
void
pr_pack(buf, cc, mhdr)
	u_char *buf;
	int cc;
	struct msghdr *mhdr;
{
	struct icmp6_hdr *icp;
	int i;
	int hoplim;
	struct sockaddr_in6 *from = (struct sockaddr_in6 *)mhdr->msg_name;
	u_char *cp = NULL, *dp, *end = buf + cc;
#ifdef OLD_RAWSOCKET
	struct ip6_hdr *ip;
#endif
	struct timeval tv, *tp;
	double triptime = 0;
	int dupflag;
	size_t off;

	(void)gettimeofday(&tv, NULL);

#ifdef OLD_RAWSOCKET
	/* Check the IP header */
	ip = (struct ip6_hdr *)buf;
	if (cc < sizeof(struct icmp6_hdr) + sizeof(struct ip6_hdr)) {
		if (options & F_VERBOSE)
			warnx("packet too short (%d bytes) from %s\n", cc,
			  inet_ntop(AF_INET6, (void *)&from->sin6_addr,
				    ntop_buf, sizeof(ntop_buf)));
		return;
	}

	/* chase nexthdr link */
    {
	u_int8_t nh;
	struct ah *ah;
	struct ip6_ext *ip6e;

	off = IP6LEN;
	nh = ip->ip6_nxt;
	while (nh != IPPROTO_ICMPV6) {
		if (options & F_VERBOSE)
			fprintf(stderr, "header chain: type=0x%x\n", nh);

		switch (nh) {
		case IPPROTO_AH:
			ah = (struct ah *)(buf + off);
			off += sizeof(struct ah);
			off += (ah->ah_len << 2);
			nh = ah->ah_nxt;
			break;

		 case IPPROTO_HOPOPTS:
			ip6e = (struct ip6_ext *)(buf + off);
			off += (ip6e->ip6e_len + 1) << 3;
			nh = ip6e->ip6e_nxt;
			break;
		default:
			if (options & F_VERBOSE) {
				fprintf(stderr,
					"unknown header type=0x%x: drop it\n",
					nh);
			}
			return;
		}
	}
    }
	/* Now the ICMP part */
	icp = (struct icmp6_hdr *)(buf + off);
#else
	if (cc < sizeof(struct icmp6_hdr)) {
		if (options & F_VERBOSE)
			warnx("packet too short (%d bytes) from %s\n", cc,
			  inet_ntop(AF_INET6, (void *)&from->sin6_addr,
				    ntop_buf, sizeof(ntop_buf)));
		return;
	}
	icp = (struct icmp6_hdr *)buf;
	off = 0;
#endif

	if ((hoplim = get_hoplim(mhdr)) == -1) {
		warnx("failed to get receiving hop limit");
		return;
	}

	if (icp->icmp6_type == ICMP6_ECHO_REPLY) {
		if (icp->icmp6_id != ident)
			return;			/* It was not our ECHO */
		++nreceived;
		if (timing) {
			tp = (struct timeval *)(icp + 1);
			tvsub(&tv, tp);
			triptime = ((double)tv.tv_sec) * 1000.0 +
			    ((double)tv.tv_usec) / 1000.0;
			tsum += triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
		}

		if (TST(icp->icmp6_seq % mx_dup_ck)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(icp->icmp6_seq % mx_dup_ck);
			dupflag = 0;
		}

		if (options & F_QUIET)
			return;

		if (options & F_FLOOD)
			(void)write(STDOUT_FILENO, &BSPACE, 1);
		else {
			(void)printf("%d bytes from %s, icmp_seq=%u", cc,
				     pr_addr(&from->sin6_addr),
				     icp->icmp6_seq);
			(void)printf(" hlim=%d", hoplim);
			if (timing)
				(void)printf(" time=%g ms", triptime);
			if (dupflag)
				(void)printf("(DUP!)");
			/* check the data */
			cp = buf + off + ICMP6ECHOLEN + ICMP6ECHOTMLEN;
			dp = outpack + ICMP6ECHOLEN + ICMP6ECHOTMLEN;
			for (i = 8; cp < end; ++i, ++cp, ++dp) {
				if (*cp != *dp) {
					(void)printf("\nwrong data byte #%d should be 0x%x but was 0x%x", i, *dp, *cp);
					break;
				}
			}
		}
	} else if (icp->icmp6_type == ICMP6_NI_REPLY) { /* ICMP6_NI_REPLY */
		struct icmp6_nodeinfo *ni = (struct icmp6_nodeinfo *)(buf + off);

		(void)printf("%d bytes from %s: ", cc,
			     pr_addr(&from->sin6_addr));

		switch(ntohs(ni->ni_qtype)) {
		 case NI_QTYPE_NOOP:
			 printf("NodeInfo NOOP");
			 break;
		 case NI_QTYPE_SUPTYPES:
			 printf("NodeInfo Supported Qtypes");
			 break;
		 case NI_QTYPE_NODEADDR:
			 pr_nodeaddr(ni, end - (u_char *)ni);
			 break;
		 case NI_QTYPE_FQDN:
		 default:	/* XXX: for backward compatibility */
			 cp = (u_char *)ni + ICMP6_NIRLEN + 1;
			 while (cp < end) {
				 if (isprint(*cp))
					 putchar(*cp);
				 else
					 printf("\\%03o", *cp & 0xff);
				 cp++;
			 }
			 if (options & F_VERBOSE) {
				 long ttl;

				 (void)printf(" (");

				 switch(ni->ni_code) {
				  case ICMP6_NI_REFUSED:
					  (void)printf("refused,");
					  break;
				  case ICMP6_NI_UNKNOWN:
					  (void)printf("unknwon qtype,");
					  break;
				 }

				 if ((end - (u_char *)ni) < ICMP6_NIRLEN) {
					 /* case of refusion, unkown */
					 goto fqdnend;
				 }
				 ttl = ntohl(*(u_long *)&buf[off+ICMP6ECHOLEN+8]);
				 if (!(ni->ni_flags & NI_FQDN_FLAG_VALIDTTL))
					 (void)printf("TTL=%d:meaningless",
						      (int)ttl);
				 else {
					 if (ttl < 0)
						 (void)printf("TTL=%d:invalid",
							      (int)ttl);
					 else
						 (void)printf("TTL=%d",
							      (int)ttl);
				 }

				 if (buf[off + ICMP6_NIRLEN] !=
				     cc - off - ICMP6_NIRLEN - 1) {
					 (void)printf(",invalid namelen:%d/%lu",
						      buf[off + ICMP6_NIRLEN],
						      (u_long)cc - off - ICMP6_NIRLEN - 1);
				 }
				 putchar(')');
			 }
		  fqdnend:
			 ;
		}
	} else {
		/* We've got something other than an ECHOREPLY */
		if (!(options & F_VERBOSE))
			return;
		(void)printf("%d bytes from %s: ", cc,
			     pr_addr(&from->sin6_addr));
		pr_icmph(icp, end);
	}

	if (!(options & F_FLOOD)) {
		(void)putchar('\n');
		(void)fflush(stdout);
	}
}

void
pr_nodeaddr(ni, nilen)
	struct icmp6_nodeinfo *ni; /* ni->qtype must be NODEADDR */
	int nilen;
{
	struct in6_addr *ia6 = (struct in6_addr *)(ni + 1);

	nilen -= sizeof(struct icmp6_nodeinfo);

	if (options & F_VERBOSE) {
		switch(ni->ni_code) {
		 case ICMP6_NI_REFUSED:
			 (void)printf("refused");
			 break;
		 case ICMP6_NI_UNKNOWN:
			 (void)printf("unknown qtype");
			 break;
		}
		if (ni->ni_flags & NI_NODEADDR_FLAG_ALL)
			(void)printf(" truncated");
	}
	putchar('\n');
	if (nilen <= 0)
		printf("  no address\n");
	for (; nilen > 0; nilen -= sizeof(*ia6), ia6 += 1) {
		printf("  %s\n",
		       inet_ntop(AF_INET6, ia6, ntop_buf, sizeof(ntop_buf)));
	}
}

int
get_hoplim(mhdr)
	struct msghdr *mhdr;
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			return(*(int *)CMSG_DATA(cm));
	}

	return(-1);
}

/*
 * tvsub --
 *	Subtract 2 timeval structs:  out = out - in.  Out is assumed to
 * be >= in.
 */
void
tvsub(out, in)
	register struct timeval *out, *in;
{
	if ((out->tv_usec -= in->tv_usec) < 0) {
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/*
 * oninfo --
 *	SIGINFO handler.
 */
void
oninfo(notused)
	int notused;
{
	summary();
}

/*
 * onint --
 *	SIGINT handler.
 */
void
onint(notused)
	int notused;
{
	summary();

	(void)signal(SIGINT, SIG_DFL);
	(void)kill(getpid(), SIGINT);

	/* NOTREACHED */
	exit(1);
}

/*
 * summary --
 *	Print out statistics.
 */
void
summary()
{
	register int i;

	(void)printf("\n--- %s ping6 statistics ---\n", hostname);
	(void)printf("%ld packets transmitted, ", ntransmitted);
	(void)printf("%ld packets received, ", nreceived);
	if (nrepeats)
		(void)printf("+%ld duplicates, ", nrepeats);
	if (ntransmitted) {
		if (nreceived > ntransmitted)
			(void)printf("-- somebody's printing up packets!");
		else
			(void)printf("%d%% packet loss",
			    (int) (((ntransmitted - nreceived) * 100) /
			    ntransmitted));
	}
	(void)putchar('\n');
	if (nreceived && timing) {
		/* Only display average to microseconds */
		i = 1000.0 * tsum / (nreceived + nrepeats);
		(void)printf("round-trip min/avg/max = %g/%g/%g ms\n",
		    tmin, ((double)i) / 1000.0, tmax);
		(void)fflush(stdout);
	}
}

#ifdef notdef
static char *ttab[] = {
	"Echo Reply",		/* ip + seq + udata */
	"Dest Unreachable",	/* net, host, proto, port, frag, sr + IP */
	"Source Quench",	/* IP */
	"Redirect",		/* redirect type, gateway, + IP  */
	"Echo",
	"Time Exceeded",	/* transit, frag reassem + IP */
	"Parameter Problem",	/* pointer + IP */
	"Timestamp",		/* id + seq + three timestamps */
	"Timestamp Reply",	/* " */
	"Info Request",		/* id + sq */
	"Info Reply"		/* " */
};
#endif

/*
 * pr_icmph --
 *	Print a descriptive string about an ICMP header.
 */
void
pr_icmph(icp, end)
	struct icmp6_hdr *icp;
	u_char *end;
{
	switch(icp->icmp6_type) {
	case ICMP6_DST_UNREACH:
		switch(icp->icmp6_code) {
		case ICMP6_DST_UNREACH_NOROUTE:
			(void)printf("No Route to Destination\n");
			break;
		case ICMP6_DST_UNREACH_ADMIN:
			(void)printf("Destination Administratively "
				     "Unreachable\n");
			break;
		case ICMP6_DST_UNREACH_NOTNEIGHBOR:
			(void)printf("Destination Unreachable Notneighbor\n");
			break;
		case ICMP6_DST_UNREACH_ADDR:
			(void)printf("Destination Host Unreachable\n");
			break;
		case ICMP6_DST_UNREACH_NOPORT:
			(void)printf("Destination Port Unreachable\n");
			break;
		default:
			(void)printf("Destination Unreachable, Bad Code: %d\n",
			    icp->icmp6_code);
			break;
		}
		/* Print returned IP header information */
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_PACKET_TOO_BIG:
		(void)printf("Packet too big mtu = %d\n",
			     (int)ntohl(icp->icmp6_mtu));
		break;
	case ICMP6_TIME_EXCEEDED:
		switch(icp->icmp6_code) {
		case ICMP6_TIME_EXCEED_TRANSIT:
			(void)printf("Time to live exceeded\n");
			break;
		case ICMP6_TIME_EXCEED_REASSEMBLY:
			(void)printf("Frag reassembly time exceeded\n");
			break;
		default:
			(void)printf("Time exceeded, Bad Code: %d\n",
			    icp->icmp6_code);
			break;
		}
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_PARAM_PROB:
		(void)printf("Parameter problem: ");
		switch(icp->icmp6_code) {
		 case ICMP6_PARAMPROB_HEADER:
			 (void)printf("Erroneous Header ");
			 break;
		 case ICMP6_PARAMPROB_NEXTHEADER:
			 (void)printf("Unknown Nextheader ");
			 break;
		 case ICMP6_PARAMPROB_OPTION:
			 (void)printf("Unrecognized Option ");
			 break;
		 default:
			 (void)printf("Bad code(%d) ", icp->icmp6_code);
			 break;
		}
		(void)printf("pointer = 0x%02x\n",
			     (int)ntohl(icp->icmp6_pptr));
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_ECHO_REQUEST:
		(void)printf("Echo Request\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP6_ECHO_REPLY:
		(void)printf("Echo Reply\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP6_MEMBERSHIP_QUERY:
		(void)printf("Membership Query\n");
		break;
	case ICMP6_MEMBERSHIP_REPORT:
		(void)printf("Membership Report\n");
		break;
	case ICMP6_MEMBERSHIP_REDUCTION:
		(void)printf("Membership Reduction\n");
		break;
	case ND_ROUTER_SOLICIT:
		(void)printf("Router Solicitation\n");
		break;
	case ND_ROUTER_ADVERT:
		(void)printf("Router Advertisement\n");
		break;
	case ND_NEIGHBOR_SOLICIT:
		(void)printf("Neighbor Solicitation\n");
		break;
	case ND_NEIGHBOR_ADVERT:
		(void)printf("Neighbor Advertisement\n");
		break;
	case ND_REDIRECT:
	{
		struct nd_redirect *red = (struct nd_redirect *)icp;

		(void)printf("Redirect\n");
		(void)printf("Destination: %s\n",
			     inet_ntop(AF_INET6, &red->nd_rd_dst,
				       ntop_buf, sizeof(ntop_buf)));
		(void)printf("New Target: %s\n",
			     inet_ntop(AF_INET6, &red->nd_rd_target,
				       ntop_buf, sizeof(ntop_buf)));
		break;
	}
	case ICMP6_NI_QUERY:
		(void)printf("Node Information Query\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP6_NI_REPLY:
		(void)printf("Node Information Reply\n");
		/* XXX ID + Seq + Data */
		break;
	default:
		(void)printf("Bad ICMP type: %d\n", icp->icmp6_type);
	}
}

/*
 * pr_iph --
 *	Print an IP6 header.
 */
void
pr_iph(ip6)
	struct ip6_hdr *ip6;
{
	u_int32_t flow = ip6->ip6_flow & IPV6_FLOWLABEL_MASK;
	u_int8_t tc;

	tc = *(&ip6->ip6_vfc + 1); /* XXX */
	tc = (tc >> 4) & 0x0f;
	tc |= (ip6->ip6_vfc << 4);

	printf("Vr TC  Flow Plen Nxt Hlim\n");
	printf(" %1x %02x %05x %04x  %02x   %02x\n",
	       (ip6->ip6_vfc & IPV6_VERSION_MASK) >> 4, tc, (int)ntohl(flow),
	       ntohs(ip6->ip6_plen),
	       ip6->ip6_nxt, ip6->ip6_hlim);
	printf("%s->", inet_ntop(AF_INET6, &ip6->ip6_src,
				  ntop_buf, INET6_ADDRSTRLEN));
	printf("%s\n", inet_ntop(AF_INET6, &ip6->ip6_dst,
				 ntop_buf, INET6_ADDRSTRLEN));
}

/*
 * pr_addr --
 *	Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
char *
pr_addr(addr)
	struct in6_addr *addr;
{
	static char buf[MAXHOSTNAMELEN];
	struct sockaddr_in6 sin6;	
	int flag = 0;

	if (options & F_NUMERIC)
		flag |= NI_NUMERICHOST;
	
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_addr = *addr;

	getnameinfo((struct sockaddr *)&sin6, sizeof(sin6),
		    buf, sizeof(buf), NULL, 0, flag);

	return (buf);
}

/*
 * pr_retip --
 *	Dump some info on a returned (via ICMPv6) IPv6 packet.
 */
void
pr_retip(ip6, end)
	struct ip6_hdr *ip6;
	u_char *end;
{
	u_char *cp = (u_char *)ip6, nh;
	int hlen;

	if (end - (u_char *)ip6 < sizeof(*ip6)) {
		printf("IP6");
		goto trunc;
	}
	pr_iph(ip6);
	hlen = sizeof(*ip6);

	nh = ip6->ip6_nxt;
	cp += hlen;
	while(end - cp >= 8) {
		switch(nh) {
		 case IPPROTO_HOPOPTS:
			 printf("HBH ");
			 hlen = (((struct ip6_hbh *)cp)->ip6h_len+1) << 3;
			 nh = ((struct ip6_hbh *)cp)->ip6h_nxt;
			 break;
		 case IPPROTO_DSTOPTS:
			 printf("DSTOPT ");
			 hlen = (((struct ip6_dest *)cp)->ip6d_len+1) << 3;
			 nh = ((struct ip6_dest *)cp)->ip6d_nxt;
			 break;
		 case IPPROTO_FRAGMENT:
			 printf("FRAG ");
			 hlen = sizeof(struct ip6_frag);
			 nh = ((struct ip6_frag *)cp)->ip6f_nxt;
			 break;
		 case IPPROTO_ROUTING:
			 printf("RTHDR ");
			 hlen = (((struct ip6_rthdr *)cp)->ip6r_len+1) << 3;
			 nh = ((struct ip6_rthdr *)cp)->ip6r_nxt;
			 break;
		 case IPPROTO_AH:
			 printf("AH ");
			 hlen = (((struct ah *)cp)->ah_len+2) << 2;
			 nh = ((struct ah *)cp)->ah_nxt;
			 break;
		 case IPPROTO_ICMPV6:
			 printf("ICMP6: type = %d, code = %d\n",
				*cp, *(cp + 1));
			 return;
		 case IPPROTO_ESP:
			 printf("ESP\n");
			 return;
		 case IPPROTO_TCP:
			 printf("TCP: from port %u, to port %u (decimal)\n",
				(*cp * 256 + *(cp + 1)),
				(*(cp + 2) * 256 + *(cp + 3)));
			 return;
		 case IPPROTO_UDP:
			 printf("UDP: from port %u, to port %u (decimal)\n",
				(*cp * 256 + *(cp + 1)),
				(*(cp + 2) * 256 + *(cp + 3)));
			 return;
		 default:
			 printf("Unknown Header(%d)\n", nh);
			 return;
		}

		if ((cp += hlen) >= end)
			goto trunc;
	}
	if (end - cp < 8)
		goto trunc;

	putchar('\n');
	return;

  trunc:
	printf("...\n");
	return;
}

void
fill(bp, patp)
	char *bp, *patp;
{
	register int ii, jj, kk;
	int pat[16];
	char *cp;

	for (cp = patp; *cp; cp++)
		if (!isxdigit(*cp))
			errx(1, "patterns must be specified as hex digits");
	ii = sscanf(patp,
	    "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	    &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	    &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	    &pat[13], &pat[14], &pat[15]);

/* xxx */	
	if (ii > 0)
		for (kk = 0;
		    kk <= MAXDATALEN - (8 + sizeof(struct timeval) + ii);
		    kk += ii)
			for (jj = 0; jj < ii; ++jj)
				bp[jj + kk] = pat[jj];
	if (!(options & F_QUIET)) {
		(void)printf("PATTERN: 0x");
		for (jj = 0; jj < ii; ++jj)
			(void)printf("%02x", bp[jj] & 0xFF);
		(void)printf("\n");
	}
}

void
usage()
{
	(void)fprintf(stderr,
"usage: ping6 [-dfnqRrvwW"
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
		      "] [-P policy"
#else
		      "AE"
#endif
#endif		      
		      "] [-a [alsg]] [-b sockbufsiz] [-c count] [-I interface]\n\
             [-i wait] [-l preload] [-p pattern] [-s packetsize]\n\
             [-h hoplimit] host [hosts...]\n");
	exit(1);
}
