/*	$NetBSD: ping.c,v 1.32.2.1 1997/12/01 20:02:05 mellon Exp $	*/

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
/*
 *			P I N G . C
 *
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 * Modified at Uc Berkeley
 * Record Route and verbose headers - Phil Dykstra, BRL, March 1988.
 * Multicast options (ttl, if, loop) - Steve Deering, Stanford, August 1988.
 * ttl, duplicate detection - Cliff Frost, UCB, April 1989
 * Pad pattern - Cliff Frost (from Tom Ferrin, UCSF), April 1989
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 *
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: ping.c,v 1.32.2.1 1997/12/01 20:02:05 mellon Exp $");
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <err.h>
#ifdef sgi
#include <bstring.h>
#include <getopt.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#endif

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>

#define FLOOD_INTVL	0.01		/* default flood output interval */
#define	MAXPACKET	(65536-60-8)	/* max packet size */

#define F_VERBOSE	0x0001
#define F_QUIET		0x0002		/* minimize all output */
#define F_SEMI_QUIET	0x0004		/* ignore our ICMP errors */
#define F_FLOOD		0x0008		/* flood-ping */
#define	F_RECORD_ROUTE	0x0010		/* record route */
#define F_SOURCE_ROUTE	0x0020		/* loose source route */
#define F_PING_FILLED	0x0040		/* is buffer filled with user data? */
#define F_PING_RANDOM	0x0080		/* use random data */
#define	F_NUMERIC	0x0100		/* do not do gethostbyaddr() calls */
#define F_TIMING	0x0200		/* room for a timestamp */
#define F_DF		0x0400		/* set IP DF bit */
#define F_SOURCE_ADDR	0x0800		/* set source IP address/interface */
#define F_ONCE		0x1000		/* exit(0) after receiving 1 reply */
#define F_MCAST		0x2000		/* multicast target */
#define F_MCAST_NOLOOP	0x4000		/* no multicast loopback */

/* MAX_DUP_CHK is the number of bits in received table, the
 *	maximum number of received sequence numbers we can track to check
 *	for duplicates.
 */
#define MAX_DUP_CHK     (8 * 2048)
u_char	rcvd_tbl[MAX_DUP_CHK/8];
int     nrepeats = 0;
#define A(seq)	rcvd_tbl[(seq/8)%sizeof(rcvd_tbl)]  /* byte in array */
#define B(seq)	(1 << (seq & 0x07))	/* bit in byte */
#define SET(seq) (A(seq) |= B(seq))
#define CLR(seq) (A(seq) &= (~B(seq)))
#define TST(seq) (A(seq) & B(seq))



u_char	*packet;
int	packlen;
int	pingflags = 0, options;
char	*fill_pat;

int s;					/* Socket file descriptor */

#define PHDR_LEN sizeof(struct timeval)	/* size of timestamp header */
struct sockaddr_in whereto, send_addr;	/* Who to ping */
struct sockaddr_in src_addr;		/* from where */
struct sockaddr_in loc_addr;		/* 127.1 */
int datalen = 64-PHDR_LEN;		/* How much data */

#ifdef sgi
static char *__progname;
#else
extern char *__progname;
#endif


char hostname[MAXHOSTNAMELEN];

static struct {
	struct ip	o_ip;
	char		o_opt[MAX_IPOPTLEN];
	union {
		u_char	    u_buf[MAXPACKET];
		struct icmp u_icmp;
	} o_u;
} out_pack;
#define	opack_icmp	out_pack.o_u.u_icmp
struct ip *opack_ip;

char optspace[MAX_IPOPTLEN];		/* record route space */
int optlen;


int npackets;				/* total packets to send */
int preload;				/* number of packets to "preload" */
int ntransmitted;			/* output sequence # = #sent */
int ident;

int nreceived;				/* # of packets we got back */

double interval;			/* interval between packets */
struct timeval interval_tv;
double tmin = 999999999;
double tmax = 0;
double tsum = 0;			/* sum of all times */
double maxwait = 0;

#ifdef SIGINFO
int reset_kerninfo;
#endif

int bufspace = 60*1024;

struct timeval now, clear_cache, last_tx, next_tx, first_tx;
struct timeval last_rx, first_rx;
int lastrcvd = 1;			/* last ping sent has been received */

static struct timeval jiggle_time;
static int jiggle_cnt, total_jiggled, jiggle_direction = -1;

static void doit(void);
static void prefinish(int);
static void prtsig(int);
static void finish(int);
static void summary(int);
static void pinger(void);
static void fill(void);
static void rnd_fill(void);
static double diffsec(struct timeval *, struct timeval *);
static void timevaladd(struct timeval *, struct timeval *);
static void sec_to_timeval(const double, struct timeval *);
static double timeval_to_sec(const struct timeval *);
static void pr_pack(u_char *, int, struct sockaddr_in *);
static u_short in_cksum(u_short *, u_int);
static void pr_saddr(char *, u_char *);
static char *pr_addr(struct in_addr *);
static void pr_iph(struct icmp *, int);
static void pr_retip(struct icmp *, int);
static int pr_icmph(struct icmp *, struct sockaddr_in *, int);
static void jiggle(int), jiggle_flush(int);
static void gethost(const char *, const char *,
		    struct sockaddr_in *, char *, int);
static void usage(void);


int
main(int argc, char *argv[])
{
	int c, i, on = 1, hostind = 0;
	long l;
	u_char ttl = 0;
	u_long tos = 0;
	char *p;
#ifdef SIGINFO
	struct termios ts;
#endif


#if defined(SIGINFO) && defined(NOKERNINFO)
	if (tcgetattr (0, &ts) != -1) {
		reset_kerninfo = !(ts.c_lflag & NOKERNINFO);
		ts.c_lflag |= NOKERNINFO;
		(void)tcsetattr(0, TCSANOW, &ts);
	}
#endif
	while ((c = getopt(argc, argv,
			   "c:dDfg:h:i:I:l:Lnop:PqQrRs:t:T:vw:")) != -1) {
		switch (c) {
		case 'c':
			npackets = strtol(optarg, &p, 0);
			if (*p != '\0' || npackets <= 0)
				errx(1, "Bad/invalid number of packets");
			break;
		case 'D':
			pingflags |= F_DF;
			break;
		case 'd':
			options |= SO_DEBUG;
			break;
		case 'f':
			pingflags |= F_FLOOD;
			break;
		case 'h':
			hostind = optind-1;
			break;
		case 'i':		/* wait between sending packets */
			interval = strtod(optarg, &p);
			if (*p != '\0' || interval <= 0)
				errx(1, "Bad/invalid interval %s", optarg);
			break;
		case 'l':
			preload = strtol(optarg, &p, 0);
			if (*p != '\0' || preload < 0)
				errx(1, "Bad/invalid preload value %s",
				     optarg);
			break;
		case 'n':
			pingflags |= F_NUMERIC;
			break;
		case 'o':
			pingflags |= F_ONCE;
			break;
		case 'p':		/* fill buffer with user pattern */
			if (pingflags & F_PING_RANDOM)
				errx(1, "Only one of -P and -p allowed");
			pingflags |= F_PING_FILLED;
			fill_pat = optarg;
			break;
		case 'P':
			if (pingflags & F_PING_FILLED)
				errx(1, "Only one of -P and -p allowed");
			pingflags |= F_PING_RANDOM;
			break;
		case 'q':
			pingflags |= F_QUIET;
			break;
		case 'Q':
			pingflags |= F_SEMI_QUIET;
			break;
		case 'r':
			options |= SO_DONTROUTE;
			break;
		case 's':		/* size of packet to send */
			datalen = strtol(optarg, &p, 0);
			if (*p != '\0' || datalen <= 0)
				errx(1, "Bad/invalid packet size %s", optarg);
			if (datalen > MAXPACKET)
				errx(1, "packet size is too large");
			break;
		case 'v':
			pingflags |= F_VERBOSE;
			break;
		case 'R':
			pingflags |= F_RECORD_ROUTE;
			break;
		case 'L':
			pingflags |= F_MCAST_NOLOOP;
			break;
		case 't':
			tos = strtoul(optarg, &p, 0);
			if (*p != '\0' ||  tos > 0xFF)
				errx(1, "bad tos value: %s", optarg);
			break;
		case 'T':
			l = strtol(optarg, &p, 0);
			if (*p != '\0' || l > 255 || l <= 0)
				errx(1, "ttl out of range");
			ttl = (u_char)l;    /* cannot check >255 otherwise */
			break;
		case 'I':
			pingflags |= F_SOURCE_ADDR;
			gethost("-I", optarg, &src_addr, 0, 0);
			break;
		case 'g':
			pingflags |= F_SOURCE_ROUTE;
			gethost("-g", optarg, &send_addr, 0, 0);
			break;
		case 'w':
			maxwait = strtod(optarg, &p);
			if (*p != '\0' || maxwait <= 0)
				errx(1, "Bad/invalid maxwait time %s", optarg);
			break;
		default:
			usage();
			break;
		}
	}

	if (interval == 0)
		interval = (pingflags & F_FLOOD) ? FLOOD_INTVL : 1.0;
#ifndef sgi
	if (interval < 1.0 && getuid())
		errx(1, "Must be superuser to use < 1 sec ping interval");
#endif
	sec_to_timeval(interval, &interval_tv);

	if (npackets != 0) {
		npackets += preload;
	} else {
		npackets = INT_MAX;
	}

	if (hostind == 0) {
		if (optind != argc-1)
			usage();
		else
			hostind = optind;
	}
	else if (hostind >= argc - 1)
		usage();

	gethost("", argv[hostind], &whereto, hostname, sizeof(hostname));
	if (IN_MULTICAST(ntohl(whereto.sin_addr.s_addr)))
		pingflags |= F_MCAST;
	if (!(pingflags & F_SOURCE_ROUTE))
		(void) memcpy(&send_addr, &whereto, sizeof(send_addr));

	loc_addr.sin_family = AF_INET;
	loc_addr.sin_addr.s_addr = htonl((127<<24)+1);

	if (datalen >= PHDR_LEN)	/* can we time them? */
		pingflags |= F_TIMING;
	packlen = datalen + 60 + 76;	/* MAXIP + MAXICMP */
	if ((packet = (u_char *)malloc(packlen)) == NULL)
		err(1, "Out of memory");

	if (pingflags & F_PING_FILLED) {
		fill();
	} else if (pingflags & F_PING_RANDOM) {
		rnd_fill();
	} else {
		for (i = PHDR_LEN; i < datalen; i++)
			opack_icmp.icmp_data[i] = i;
	}

	ident = getpid() & 0xFFFF;

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
		err(1, "Cannot create socket");
	if (options & SO_DEBUG) {
		if (setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *) &on,
		    sizeof(on)) == -1)
			warn("Can't turn on socket debugging");
	}
	if (options & SO_DONTROUTE) {
		if (setsockopt(s, SOL_SOCKET, SO_DONTROUTE, (char *) &on,
		    sizeof(on)) == -1)
			warn("SO_DONTROUTE");
	}

	if (pingflags & F_SOURCE_ROUTE) {
		optspace[IPOPT_OPTVAL] = IPOPT_LSRR;
		optspace[IPOPT_OLEN] = optlen = 7;
		optspace[IPOPT_OFFSET] = IPOPT_MINOFF;
		(void) memcpy(&whereto.sin_addr, &optspace[IPOPT_MINOFF-1],
		    sizeof(whereto.sin_addr));
		optspace[optlen++] = IPOPT_NOP;
	}
	if (pingflags & F_RECORD_ROUTE) {
		optspace[optlen+IPOPT_OPTVAL] = IPOPT_RR;
		optspace[optlen+IPOPT_OLEN] = (MAX_IPOPTLEN -1-optlen);
		optspace[optlen+IPOPT_OFFSET] = IPOPT_MINOFF;
		optlen = MAX_IPOPTLEN;
	}
	/* this leaves opack_ip 0(mod 4) aligned */
	opack_ip = (struct ip *)((char *)&out_pack.o_ip
				 + sizeof(out_pack.o_opt)
				 - optlen);
	(void) memcpy(opack_ip + 1, optspace, optlen);

	if (setsockopt(s,IPPROTO_IP,IP_HDRINCL, (char *) &on, sizeof(on)) < 0)
		err(1, "Can't set special IP header");

	opack_ip->ip_v = IPVERSION;
	opack_ip->ip_hl = (sizeof(struct ip)+optlen) >> 2;
	opack_ip->ip_tos = tos;
	opack_ip->ip_off = (pingflags & F_DF) ? IP_DF : 0;
	opack_ip->ip_ttl = ttl ? ttl : MAXTTL;
	opack_ip->ip_p = IPPROTO_ICMP;
	opack_ip->ip_src = src_addr.sin_addr;
	opack_ip->ip_dst = send_addr.sin_addr;

	if (pingflags & F_MCAST) {
		if (pingflags & F_MCAST_NOLOOP) {
			u_char loop = 0;
			if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP,
			    (char *) &loop, 1) < 0)
				err(1, "Can't disable multicast loopback");
		}

		if (ttl != 0
		    && setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL,
		    (char *) &ttl, 1) < 0)
			err(1, "Can't set multicast time-to-live");

		if ((pingflags & F_SOURCE_ADDR)
		    && setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF,
				  (char *) &src_addr.sin_addr,
				  sizeof(src_addr.sin_addr)) < 0)
			err(1, "Can't set multicast source interface");

	} else if (pingflags & F_SOURCE_ADDR) {
		if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF,
			       (char *) &src_addr.sin_addr,
			       sizeof(src_addr.sin_addr)) < 0)
			err(1, "Can't set source interface/address");
	}

	(void)printf("PING %s (%s): %d data bytes\n", hostname,
		     inet_ntoa(whereto.sin_addr), datalen);

	/* When pinging the broadcast address, you can get a lot
	 * of answers.  Doing something so evil is useful if you
	 * are trying to stress the ethernet, or just want to
	 * fill the arp cache to get some stuff for /etc/ethers.
	 */
	while (0 > setsockopt(s, SOL_SOCKET, SO_RCVBUF,
			      (char*)&bufspace, sizeof(bufspace))) {
		if ((bufspace -= 4096) == 0)
			err(1, "Cannot set the receive buffer size");
	}

	/* make it possible to send giant probes, but do not worry now
	 * if it fails, since we probably won't send giant probes.
	 */
	(void)setsockopt(s, SOL_SOCKET, SO_SNDBUF,
			 (char*)&bufspace, sizeof(bufspace));

	(void)signal(SIGINT, prefinish);
#ifdef SIGINFO
	(void)signal(SIGINFO, prtsig);
#else
	(void)signal(SIGQUIT, prtsig);
#endif
	(void)signal(SIGCONT, prtsig);

#ifdef sgi
	/* run with a non-degrading priority to improve the delay values. */
	(void) cap_schedctl(NDPRI, 0, NDPHIMAX);
#endif

	/* fire off them quickies */
	for (i = 0; i < preload; i++) {
		(void)gettimeofday(&now, 0);
		pinger();
	}

	doit();
	return 0;
}


static void
doit(void)
{
	int cc;
	struct sockaddr_in from;
	int fromlen;
	double sec, last, d_last;
	struct timeval timeout;
	fd_set fdmask;


	(void)gettimeofday(&clear_cache,0);
	if (maxwait != 0) {
		last = timeval_to_sec(&clear_cache) + maxwait;
		d_last = 0;
	} else {
		last = 0;
		d_last = 365*24*60*60;
	}

	FD_ZERO(&fdmask);
	do {
		(void)gettimeofday(&now,0);

		if (last != 0)
			d_last = last - timeval_to_sec(&now);

		if (ntransmitted < npackets && d_last > 0) {
			/* send if within 100 usec or late for next packet */
			sec = diffsec(&next_tx,&now);
			if (sec <= 0.0001
			    || (lastrcvd && (pingflags & F_FLOOD))) {
				pinger();
				sec = diffsec(&next_tx,&now);
			}
			if (sec < 0.0)
				sec = 0.0;
			if (d_last < sec)
				sec = d_last;

		} else {
			/* For the last response, wait twice as long as the
			 * worst case seen, or 10 times as long as the
			 * maximum interpacket interval, whichever is longer.
			 */
			sec = MAX(2*tmax,10*interval) - diffsec(&now,&last_tx);
			if (d_last < sec)
				sec = d_last;
			if (sec <= 0)
				break;
		}


		sec_to_timeval(sec, &timeout);

		FD_SET(s, &fdmask);
		cc = select(s+1, &fdmask, 0, 0, &timeout);
		if (cc <= 0) {
			if (cc < 0) {
				if (errno == EINTR)
					continue;
				jiggle_flush(1);
				err(1, "select");
			}
			continue;
		}

		fromlen  = sizeof(from);
		cc = recvfrom(s, (char *) packet, packlen,
			      0, (struct sockaddr *)&from,
			      &fromlen);
		if (cc < 0) {
			if (errno != EINTR) {
				jiggle_flush(1);
				warn("recvfrom");
				(void)fflush(stderr);
			}
			continue;
		}
		(void)gettimeofday(&now, 0);
		pr_pack(packet, cc, &from);

	} while (nreceived < npackets
		 && (nreceived == 0 || !(pingflags & F_ONCE)));

	finish(0);
}


static void
jiggle_flush(int nl)			/* new line if there are dots */
{
	int serrno = errno;

	if (jiggle_cnt > 0) {
		total_jiggled += jiggle_cnt;
		jiggle_direction = 1;
		do {
			(void)putchar('.');
		} while (--jiggle_cnt > 0);

	} else if (jiggle_cnt < 0) {
		total_jiggled -= jiggle_cnt;
		jiggle_direction = -1;
		do {
			(void)putchar('\b');
		} while (++jiggle_cnt < 0);
	}

	if (nl) {
		if (total_jiggled != 0)
			(void)putchar('\n');
		total_jiggled = 0;
		jiggle_direction = -1;
	}

	(void)fflush(stdout);
	(void)fflush(stderr);
	jiggle_time = now;
	errno = serrno;
}


/* jiggle the cursor for flood-ping
 */
static void
jiggle(int delta)
{
	double dt;

	if (pingflags & F_QUIET)
		return;

	/* do not back up into messages */
	if (total_jiggled+jiggle_cnt+delta < 0)
		return;

	jiggle_cnt += delta;

	/* flush the FLOOD dots when things are quiet
	 * or occassionally to make the cursor jiggle.
	 */
	dt = diffsec(&last_tx, &jiggle_time);
	if (dt > 0.2 || (dt >= 0.15 && delta*jiggle_direction < 0))
		jiggle_flush(0);
}


/*
 * Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first PHDR_LEN bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
static void
pinger(void)
{
	int i, cc, sw;

	opack_icmp.icmp_code = 0;
	opack_icmp.icmp_seq = htons((u_short)(ntransmitted));

	/* clear the cached route in the kernel after an ICMP
	 * response such as a Redirect is seen to stop causing
	 * more such packets.  Also clear the cached route
	 * periodically in case of routing changes that make
	 * black holes come and go.
	 */
	if (clear_cache.tv_sec != now.tv_sec) {
		opack_icmp.icmp_type = ICMP_ECHOREPLY;
		opack_icmp.icmp_id = ~ident;
		opack_icmp.icmp_cksum = 0;
		opack_icmp.icmp_cksum = in_cksum((u_short*)&opack_icmp,
						 PHDR_LEN);
		sw = 0;
		if (setsockopt(s,IPPROTO_IP,IP_HDRINCL,
			       (char *)&sw,sizeof(sw)) < 0)
			err(1, "Can't turn off special IP header");
		if (sendto(s, (char *) &opack_icmp, PHDR_LEN, MSG_DONTROUTE,
			   (struct sockaddr *)&loc_addr,
			   sizeof(struct sockaddr_in)) < 0) {
			/*
			 * XXX: we only report this as a warning in verbose
			 * mode because people get confused when they see
			 * this error when they are running in single user
			 * mode and they have not configured lo0
			 */
			if (pingflags & F_VERBOSE)
				warn("failed to clear cached route");
		}
		sw = 1;
		if (setsockopt(s,IPPROTO_IP,IP_HDRINCL,
			       (char *)&sw, sizeof(sw)) < 0)
			err(1, "Can't set special IP header");
		
		(void)gettimeofday(&clear_cache,0);
	}

	opack_icmp.icmp_type = ICMP_ECHO;
	opack_icmp.icmp_id = ident;
	if (pingflags & F_TIMING)
		(void) memcpy(&opack_icmp.icmp_data[0], &now, sizeof(now));
	cc = datalen+PHDR_LEN;
	opack_icmp.icmp_cksum = 0;
	opack_icmp.icmp_cksum = in_cksum((u_short*)&opack_icmp, cc);

	cc += opack_ip->ip_hl<<2;
	opack_ip->ip_len = cc;
	i = sendto(s, (char *) opack_ip, cc, 0,
		   (struct sockaddr *)&send_addr, sizeof(struct sockaddr_in));
	if (i != cc) {
		jiggle_flush(1);
		if (i < 0)
			warn("sendto");
		else
			warnx("wrote %s %d chars, ret=%d", hostname, cc, i);
		(void)fflush(stderr);
	}
	lastrcvd = 0;

	CLR(ntransmitted);
	ntransmitted++;

	last_tx = now;
	if (next_tx.tv_sec == 0) {
		first_tx = now;
		next_tx = now;
	}

	/* Transmit regularly, at always the same microsecond in the
	 * second when going at one packet per second.
	 * If we are at most 100 ms behind, send extras to get caught up.
	 * Otherwise, skip packets we were too slow to send.
	 */
	if (diffsec(&next_tx, &now) <= interval) {
		do {
			timevaladd(&next_tx, &interval_tv);
		} while (diffsec(&next_tx, &now) < -0.1);
	}

	if (pingflags & F_FLOOD)
		jiggle(1);

	/* While the packet is going out, ready buffer for the next
	 * packet. Use a fast but not very good random number generator.
	 */
	if (pingflags & F_PING_RANDOM)
		rnd_fill();
}


static void
pr_pack_sub(int cc,
	    char *addr,
	    int seqno,
	    int dupflag,
	    int ttl,
	    double triptime)
{
	jiggle_flush(1);

	if (pingflags & F_FLOOD)
		return;

	(void)printf("%d bytes from %s: icmp_seq=%u", cc, addr, seqno);
	if (dupflag)
		(void)printf(" DUP!");
	(void)printf(" ttl=%d", ttl);
	if (pingflags & F_TIMING)
		(void)printf(" time=%.3f ms", triptime*1000.0);
}


/*
 * Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
static void
pr_pack(u_char *buf,
	int cc,
	struct sockaddr_in *from)
{
	struct ip *ip;
	struct icmp *icp;
	int i, j;
	u_char *cp;
	static int old_rrlen;
	static char old_rr[MAX_IPOPTLEN];
	int hlen, dupflag = 0, dumped;
	double triptime = 0.0;
#define PR_PACK_SUB() {if (!dumped) {			\
	dumped = 1;					\
	pr_pack_sub(cc, inet_ntoa(from->sin_addr),	\
		    ntohs((u_short)icp->icmp_seq),	\
		    dupflag, ip->ip_ttl, triptime);}}

	/* Check the IP header */
	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (pingflags & F_VERBOSE) {
			jiggle_flush(1);
			(void)printf("packet too short (%d bytes) from %s\n",
				     cc, inet_ntoa(from->sin_addr));
		}
		return;
	}

	/* Now the ICMP part */
	dumped = 0;
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
	if (icp->icmp_type == ICMP_ECHOREPLY
	    && icp->icmp_id == ident) {

		if (icp->icmp_seq == htons((u_short)(ntransmitted-1)))
			lastrcvd = 1;
		last_rx = now;
		if (first_rx.tv_sec == 0)
			first_rx = last_rx;
		nreceived++;
		if (pingflags & F_TIMING) {
			struct timeval tv;
			(void) memcpy(&tv, icp->icmp_data, sizeof(tv));
			triptime = diffsec(&last_rx, &tv);
			tsum += triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
		}

		if (TST(ntohs((u_short)icp->icmp_seq))) {
			nrepeats++, nreceived--;
			dupflag=1;
		} else {
			SET(ntohs((u_short)icp->icmp_seq));
		}

		if (pingflags & F_QUIET)
			return;

		if (!(pingflags & F_FLOOD))
			PR_PACK_SUB();

		/* check the data */
		if (datalen > PHDR_LEN
		    && !(pingflags & F_PING_RANDOM)
		    && memcmp(&icp->icmp_data[PHDR_LEN],
			    &opack_icmp.icmp_data[PHDR_LEN],
			    datalen-PHDR_LEN)) {
			for (i=PHDR_LEN; i<datalen; i++) {
				if (icp->icmp_data[PHDR_LEN+i]
				    != opack_icmp.icmp_data[PHDR_LEN+i])
					break;
			}
			PR_PACK_SUB();
			(void)printf("\nwrong data byte #%d should have been"
				     " %#x but was %#x",
				     i, (u_char)opack_icmp.icmp_data[i],
				     (u_char)icp->icmp_data[i]);
			for (i=PHDR_LEN; i<datalen; i++) {
				if ((i%16) == PHDR_LEN)
					(void)printf("\n\t");
				(void)printf("%2x ",(u_char)icp->icmp_data[i]);
			}
		}

	} else {
		if (!pr_icmph(icp, from, cc))
			return;
		dumped = 2;
	}

	/* Display any IP options */
	cp = buf + sizeof(struct ip);
	while (hlen > (int)sizeof(struct ip)) {
		switch (*cp) {
		case IPOPT_EOL:
			hlen = 0;
			break;
		case IPOPT_LSRR:
			hlen -= 2;
			j = *++cp;
			++cp;
			j -= IPOPT_MINOFF;
			if (j <= 0)
				continue;
			if (dumped <= 1) {
				j = ((j+3)/4)*4;
				hlen -= j;
				cp += j;
				break;
			}
			PR_PACK_SUB();
			(void)printf("\nLSRR: ");
			for (;;) {
				pr_saddr("\t%s", cp);
				cp += 4;
				hlen -= 4;
				j -= 4;
				if (j <= 0)
					break;
				(void)putchar('\n');
			}
			break;
		case IPOPT_RR:
			j = *++cp;	/* get length */
			i = *++cp;	/* and pointer */
			hlen -= 2;
			if (i > j)
				i = j;
			i -= IPOPT_MINOFF;
			if (i <= 0)
				continue;
			if (dumped <= 1) {
				if (i == old_rrlen
				    && !memcmp(cp, old_rr, i)) {
					if (dumped)
					    (void)printf("\t(same route)");
					j = ((i+3)/4)*4;
					hlen -= j;
					cp += j;
					break;
				}
				old_rrlen = i;
				(void) memcpy(old_rr, cp, i);
			}
			if (!dumped) {
				jiggle_flush(1);
				(void)printf("RR: ");
				dumped = 1;
			} else {
				(void)printf("\nRR: ");
			}
			for (;;) {
				pr_saddr("\t%s", cp);
				cp += 4;
				hlen -= 4;
				i -= 4;
				if (i <= 0)
					break;
				(void)putchar('\n');
			}
			break;
		case IPOPT_NOP:
			if (dumped <= 1)
				break;
			PR_PACK_SUB();
			(void)printf("\nNOP");
			break;
#ifdef sgi
		case IPOPT_SECURITY:	/* RFC 1108 RIPSO BSO */
		case IPOPT_ESO:		/* RFC 1108 RIPSO ESO */
		case IPOPT_CIPSO:	/* Commercial IPSO */
			if ((sysconf(_SC_IP_SECOPTS)) > 0) {
				i = (unsigned)cp[1];
				hlen -= i - 1;
				PR_PACK_SUB();
				(void)printf("\nSEC:");
				while (i--) {
					(void)printf(" %02x", *cp++);
				}
				cp--;
				break;
			}
#endif
		default:
			PR_PACK_SUB();
			(void)printf("\nunknown option 0x%x", *cp);
			break;
		}
		hlen--;
		cp++;
	}

	if (dumped) {
		(void)putchar('\n');
		(void)fflush(stdout);
	} else {
		jiggle(-1);
	}
}


/* Compute the IP checksum
 *	This assumes the packet is less than 32K long.
 */
static u_short
in_cksum(u_short *p,
	 u_int len)
{
	u_int sum = 0;
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


/*
 * compute the difference of two timevals in seconds
 */
static double
diffsec(struct timeval *now,
	struct timeval *then)
{
	return ((now->tv_sec - then->tv_sec)*1.0
		+ (now->tv_usec - then->tv_usec)/1000000.0);
}


static void
timevaladd(struct timeval *t1,
	   struct timeval *t2)
{

	t1->tv_sec += t2->tv_sec;
	if ((t1->tv_usec += t2->tv_usec) > 1000000) {
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
}


static void
sec_to_timeval(const double sec, struct timeval *tp)
{
	tp->tv_sec = sec;
	tp->tv_usec = (sec - tp->tv_sec) * 1000000.0;
}

static double
timeval_to_sec(const struct timeval *tp)
{
	return tp->tv_sec + tp->tv_usec / 1000000.0;
}


/*
 * Print statistics.
 * Heavily buffered STDIO is used here, so that all the statistics
 * will be written with 1 sys-write call.  This is nice when more
 * than one copy of the program is running on a terminal;  it prevents
 * the statistics output from becomming intermingled.
 */
static void
summary(int header)
{
	jiggle_flush(1);

	if (header)
		(void)printf("\n----%s PING Statistics----\n", hostname);
	(void)printf("%d packets transmitted, ", ntransmitted);
	(void)printf("%d packets received, ", nreceived);
	if (nrepeats)
		(void)printf("+%d duplicates, ", nrepeats);
	if (ntransmitted) {
		if (nreceived > ntransmitted)
			(void)printf("-- somebody's printing up packets!");
		else
			(void)printf("%d%% packet loss",
				     (int) (((ntransmitted-nreceived)*100) /
					    ntransmitted));
	}
	(void)printf("\n");
	if (nreceived && (pingflags & F_TIMING)) {
		(void)printf("round-trip min/avg/max = %.3f/%.3f/%.3f ms\n",
			     tmin*1000.0,
			     (tsum/(nreceived+nrepeats))*1000.0,
			     tmax*1000.0);
		if (pingflags & F_FLOOD) {
			double r = diffsec(&last_rx, &first_rx);
			double t = diffsec(&last_tx, &first_tx);
			if (r == 0)
				r = 0.0001;
			if (t == 0)
				t = 0.0001;
			(void)printf("  %.1f packets/sec sent, "
				     " %.1f packets/sec received\n",
				     ntransmitted/t, nreceived/r);
		}
	}
}


/*
 * Print statistics when SIGINFO is received.
 */
/* ARGSUSED */
static void
prtsig(int s)
{
	summary(0);
#ifdef SIGINFO
	(void)signal(SIGINFO, prtsig);
#else
	(void)signal(SIGQUIT, prtsig);
#endif
}


/*
 * On the first SIGINT, allow any outstanding packets to dribble in
 */
static void
prefinish(int s)
{
	if (lastrcvd			/* quit now if caught up */
	    || nreceived == 0)		/* or if remote is dead */
		finish(0);

	(void)signal(s, finish);	/* do this only the 1st time */

	if (npackets > ntransmitted)	/* let the normal limit work */
		npackets = ntransmitted;
}


/*
 * Print statistics and give up.
 */
/* ARGSUSED */
static void
finish(int s)
{
#if defined(SIGINFO) && defined(NOKERNINFO)
	struct termios ts;

	if (reset_kerninfo && tcgetattr (0, &ts) != -1) {
		ts.c_lflag &= ~NOKERNINFO;
		(void)tcsetattr(0, TCSANOW, &ts);
	}
	(void)signal(SIGINFO, SIG_IGN);
#else
	(void)signal(SIGQUIT, SIG_DFL);
#endif

	summary(1);
	exit(nreceived > 0 ? 0 : 2);
}


static int				/* 0=do not print it */
ck_pr_icmph(struct icmp *icp,
	    struct sockaddr_in *from,
	    int cc,
	    int override)		/* 1=override VERBOSE if interesting */
{
	int	hlen;
	struct ip ip;
	struct icmp icp2;
	int res;

	if (pingflags & F_VERBOSE) {
		res = 1;
		jiggle_flush(1);
	} else {
		res = 0;
	}

	(void) memcpy(&ip, icp->icmp_data, sizeof(ip));
	hlen = ip.ip_hl << 2;
	if (ip.ip_p == IPPROTO_ICMP
	    && hlen + 6 <= cc) {
		(void) memcpy(&icp2, &icp->icmp_data[hlen], sizeof(icp2));
		if (icp2.icmp_id == ident) {
			/* remember to clear route cached in kernel
			 * if this ICMP message was for one of our packet.
			 */
			clear_cache.tv_sec = 0;

			if (!res && override
			    && (pingflags & (F_QUIET|F_SEMI_QUIET)) == 0) {
				jiggle_flush(1);
				(void)printf("%d bytes from %s: ",
					     cc, pr_addr(&from->sin_addr));
				res = 1;
			}
		}
	}

	return res;
}


/*
 *  Print a descriptive string about an ICMP header other than an echo reply.
 */
static int				/* 0=printed nothing */
pr_icmph(struct icmp *icp,
	 struct sockaddr_in *from,
	 int cc)
{
	switch (icp->icmp_type ) {
	case ICMP_UNREACH:
		if (!ck_pr_icmph(icp, from, cc, 1))
			return 0;
		switch (icp->icmp_code) {
		case ICMP_UNREACH_NET:
			(void)printf("Destination Net Unreachable");
			break;
		case ICMP_UNREACH_HOST:
			(void)printf("Destination Host Unreachable");
			break;
		case ICMP_UNREACH_PROTOCOL:
			(void)printf("Destination Protocol Unreachable");
			break;
		case ICMP_UNREACH_PORT:
			(void)printf("Destination Port Unreachable");
			break;
		case ICMP_UNREACH_NEEDFRAG:
			(void)printf("frag needed and DF set.  Next MTU=%d",
			       icp->icmp_nextmtu);
			break;
		case ICMP_UNREACH_SRCFAIL:
			(void)printf("Source Route Failed");
			break;
		case ICMP_UNREACH_NET_UNKNOWN:
			(void)printf("Unreachable unknown net");
			break;
		case ICMP_UNREACH_HOST_UNKNOWN:
			(void)printf("Unreachable unknown host");
			break;
		case ICMP_UNREACH_ISOLATED:
			(void)printf("Unreachable host isolated");
			break;
		case ICMP_UNREACH_NET_PROHIB:
			(void)printf("Net prohibited access");
			break;
		case ICMP_UNREACH_HOST_PROHIB:
			(void)printf("Host prohibited access");
			break;
		case ICMP_UNREACH_TOSNET:
			(void)printf("Bad TOS for net");
			break;
		case ICMP_UNREACH_TOSHOST:
			(void)printf("Bad TOS for host");
			break;
		case 13:
			(void)printf("Communication prohibited");
			break;
		case 14:
			(void)printf("Host precedence violation");
			break;
		case 15:
			(void)printf("Precedence cutoff");
			break;
		default:
			(void)printf("Bad Destination Unreachable Code: %d",
				     icp->icmp_code);
			break;
		}
		/* Print returned IP header information */
		pr_retip(icp, cc);
		break;

	case ICMP_SOURCEQUENCH:
		if (!ck_pr_icmph(icp, from, cc, 1))
			return 0;
		(void)printf("Source Quench");
		pr_retip(icp, cc);
		break;

	case ICMP_REDIRECT:
		if (!ck_pr_icmph(icp, from, cc, 1))
			return 0;
		switch (icp->icmp_code) {
		case ICMP_REDIRECT_NET:
			(void)printf("Redirect: Network");
			break;
		case ICMP_REDIRECT_HOST:
			(void)printf("Redirect: Host");
			break;
		case ICMP_REDIRECT_TOSNET:
			(void)printf("Redirect: Type of Service and Network");
			break;
		case ICMP_REDIRECT_TOSHOST:
			(void)printf("Redirect: Type of Service and Host");
			break;
		default:
			(void)printf("Redirect: Bad Code: %d", icp->icmp_code);
			break;
		}
		(void)printf(" New addr: %s",
			     pr_addr(&icp->icmp_hun.ih_gwaddr));
		pr_retip(icp, cc);
		break;

	case ICMP_ECHO:
		if (!ck_pr_icmph(icp, from, cc, 0))
			return 0;
		(void)printf("Echo Request: ID=%d seq=%d",
			     icp->icmp_id, icp->icmp_seq);
		break;

	case ICMP_ECHOREPLY:
		/* displaying other's pings is too noisey */
#if 0
		if (!ck_pr_icmph(icp, from, cc, 0))
			return 0;
		(void)printf("Echo Reply: ID=%d seq=%d",
			     icp->icmp_id, icp->icmp_seq);
		break;
#else
		return 0;
#endif

	case ICMP_ROUTERADVERT:
		if (!ck_pr_icmph(icp, from, cc, 0))
			return 0;
		(void)printf("Router Discovery Advert");
		break;

	case ICMP_ROUTERSOLICIT:
		if (!ck_pr_icmph(icp, from, cc, 0))
			return 0;
		(void)printf("Router Discovery Solicit");
		break;

	case ICMP_TIMXCEED:
		if (!ck_pr_icmph(icp, from, cc, 1))
			return 0;
		switch (icp->icmp_code ) {
		case ICMP_TIMXCEED_INTRANS:
			(void)printf("Time To Live exceeded");
			break;
		case ICMP_TIMXCEED_REASS:
			(void)printf("Frag reassembly time exceeded");
			break;
		default:
			(void)printf("Time exceeded, Bad Code: %d",
				     icp->icmp_code);
			break;
		}
		pr_retip(icp, cc);
		break;

	case ICMP_PARAMPROB:
		if (!ck_pr_icmph(icp, from, cc, 1))
			return 0;
		(void)printf("Parameter problem: pointer = 0x%02x",
			     icp->icmp_hun.ih_pptr);
		pr_retip(icp, cc);
		break;

	case ICMP_TSTAMP:
		if (!ck_pr_icmph(icp, from, cc, 0))
			return 0;
		(void)printf("Timestamp");
		break;

	case ICMP_TSTAMPREPLY:
		if (!ck_pr_icmph(icp, from, cc, 0))
			return 0;
		(void)printf("Timestamp Reply");
		break;

	case ICMP_IREQ:
		if (!ck_pr_icmph(icp, from, cc, 0))
			return 0;
		(void)printf("Information Request");
		break;

	case ICMP_IREQREPLY:
		if (!ck_pr_icmph(icp, from, cc, 0))
			return 0;
		(void)printf("Information Reply");
		break;

	case ICMP_MASKREQ:
		if (!ck_pr_icmph(icp, from, cc, 0))
			return 0;
		(void)printf("Address Mask Request");
		break;

	case ICMP_MASKREPLY:
		if (!ck_pr_icmph(icp, from, cc, 0))
			return 0;
		(void)printf("Address Mask Reply");
		break;

	default:
		if (!ck_pr_icmph(icp, from, cc, 0))
			return 0;
		(void)printf("Bad ICMP type: %d", icp->icmp_type);
		if (pingflags & F_VERBOSE)
			pr_iph(icp, cc);
	}

	return 1;
}


/*
 *  Print an IP header with options.
 */
static void
pr_iph(struct icmp *icp,
       int cc)
{
	int	hlen;
	u_char	*cp;
	struct ip ip;

	(void) memcpy(&ip, icp->icmp_data, sizeof(ip));

	hlen = ip.ip_hl << 2;
	cp = (u_char *) &icp->icmp_data[20];	/* point to options */

	(void)printf("\n Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src	     Dst\n");
	(void)printf("  %1x  %1x  %02x %04x %04x",
		     ip.ip_v, ip.ip_hl, ip.ip_tos, ip.ip_len, ip.ip_id);
	(void)printf("   %1x %04x",
		     ((ip.ip_off)&0xe000)>>13, (ip.ip_off)&0x1fff);
	(void)printf("  %02x  %02x %04x",
		     ip.ip_ttl, ip.ip_p, ip.ip_sum);
	(void)printf(" %15s ",
		     inet_ntoa(*(struct in_addr *)&ip.ip_src.s_addr));
	(void)printf(" %s ", inet_ntoa(*(struct in_addr *)&ip.ip_dst.s_addr));
	/* dump any option bytes */
	while (hlen-- > 20 && cp < (u_char*)icp+cc) {
		(void)printf("%02x", *cp++);
	}
}

/*
 * Print an ASCII host address starting from a string of bytes.
 */
static void
pr_saddr(char *pat,
	 u_char *cp)
{
	n_long l;
	struct in_addr addr;

	l = (u_char)*++cp;
	l = (l<<8) + (u_char)*++cp;
	l = (l<<8) + (u_char)*++cp;
	l = (l<<8) + (u_char)*++cp;
	addr.s_addr = htonl(l);
	(void)printf(pat, (l == 0) ? "0.0.0.0" : pr_addr(&addr));
}


/*
 *  Return an ASCII host address
 *  as a dotted quad and optionally with a hostname
 */
static char *
pr_addr(struct in_addr *addr)		/* in network order */
{
	struct	hostent	*hp;
	static	char buf[MAXHOSTNAMELEN+4+16+1];

	if ((pingflags & F_NUMERIC)
	    || !(hp = gethostbyaddr((char *)addr, sizeof(*addr), AF_INET))) {
		(void)sprintf(buf, "%s", inet_ntoa(*addr));
	} else {
		(void)sprintf(buf, "%s (%s)", hp->h_name, inet_ntoa(*addr));
	}

	return buf;
}

/*
 *  Dump some info on a returned (via ICMP) IP packet.
 */
static void
pr_retip(struct icmp *icp,
	 int cc)
{
	int	hlen;
	u_char	*cp;
	struct ip ip;

	(void) memcpy(&ip, icp->icmp_data, sizeof(ip));

	if (pingflags & F_VERBOSE)
		pr_iph(icp, cc);

	hlen = ip.ip_hl << 2;
	cp = (u_char *) &icp->icmp_data[hlen];

	if (ip.ip_p == IPPROTO_TCP) {
		if (pingflags & F_VERBOSE)
			(void)printf("\n  TCP: from port %u, to port %u",
				     (*cp*256+*(cp+1)), (*(cp+2)*256+*(cp+3)));
	} else if (ip.ip_p == IPPROTO_UDP) {
		if (pingflags & F_VERBOSE)
			(void)printf("\n  UDP: from port %u, to port %u",
				     (*cp*256+*(cp+1)), (*(cp+2)*256+*(cp+3)));
	} else if (ip.ip_p == IPPROTO_ICMP) {
		struct icmp icp2;
		(void) memcpy(&icp2, cp, sizeof(icp2));
		if (icp2.icmp_type == ICMP_ECHO) {
			if (pingflags & F_VERBOSE)
				(void)printf("\n  ID=%u icmp_seq=%u",
					     ntohs((u_short)icp2.icmp_id),
					     ntohs((u_short)icp2.icmp_seq));
			else
				(void)printf(" for icmp_seq=%u",
					     ntohs((u_short)icp2.icmp_seq));
		}
	}
}

static void
fill(void)
{
	int i, j, k;
	char *cp;
	int pat[16];

	for (cp = fill_pat; *cp != '\0'; cp++) {
		if (!isxdigit(*cp))
			break;
	}
	if (cp == fill_pat || *cp != '\0' || (cp-fill_pat) > 16*2) {
		(void)fflush(stdout);
		errx(1, "\"-p %s\": patterns must be specified with"
		     " 1-32 hex digits\n",
		     fill_pat);
	}

	i = sscanf(fill_pat,
		   "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
		    &pat[0], &pat[1], &pat[2], &pat[3],
		    &pat[4], &pat[5], &pat[6], &pat[7],
		    &pat[8], &pat[9], &pat[10], &pat[11],
		    &pat[12], &pat[13], &pat[14], &pat[15]);

	for (k=PHDR_LEN, j = 0; k <= datalen; k++) {
		opack_icmp.icmp_data[k] = pat[j];
		if (++j >= i)
			j = 0;
	}

	if (!(pingflags & F_QUIET)) {
		(void)printf("PATTERN: 0x");
		for (j=0; j<i; j++)
			(void)printf("%02x",
				     (u_char)opack_icmp.icmp_data[PHDR_LEN+j]);
		(void)printf("\n");
	}

}


static void
rnd_fill(void)
{
	static u_int rnd;
	int i;

	for (i = PHDR_LEN; i < datalen; i++) {
		rnd = (314157*rnd + 66329) & 0xffff;
		opack_icmp.icmp_data[i] = rnd>>8;
	}
}

static void
gethost(const char *arg,
	const char *name,
	struct sockaddr_in *sa,
	char *realname,
	int realname_len)
{
	struct hostent *hp;

	(void)memset(sa, 0, sizeof(*sa));
	sa->sin_family = AF_INET;

	/* If it is an IP address, try to convert it to a name to
	 * have something nice to display.
	 */
	if (inet_aton(name, &sa->sin_addr) != 0) {
		if (realname) {
			if (pingflags & F_NUMERIC)
				hp = 0;
			else
				hp = gethostbyaddr((char *)&sa->sin_addr,
						   sizeof(sa->sin_addr),
						   AF_INET);
			(void)strncpy(realname, hp ? hp->h_name : name,
				      realname_len);
			realname[realname_len-1] = '\0';
		}
		return;
	}
	
	hp = gethostbyname(name);
	if (!hp)
		errx(1, "Cannot resolve \"%s\" (%s)",name,hstrerror(h_errno));

	if (hp->h_addrtype != AF_INET)
		errx(1, "%s only supported with IP", arg);

	(void)memmove(&sa->sin_addr, hp->h_addr, sizeof(sa->sin_addr));

	if (realname) {
		(void)strncpy(realname, hp->h_name, realname_len);
		realname[realname_len-1] = '\0';
	}
}


static void
usage(void)
{
	(void)fprintf(stderr, "Usage: \n"
		      "%s [-dDfnoqrvRLP] [-c count] [-s size] [-l preload]"
		      " [-p pattern]\n"
		      "     [-i interval] [-i maxwait] [-t tos] [-T ttl]"
		      " [-I addr] [-g gateway] host\n",
		      __progname);
	exit(1);
}
