/*	$NetBSD: ndp.c,v 1.3 1999/09/03 03:54:47 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
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
 * Copyright (c) 1984, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sun Microsystems, Inc.
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
 * Based on:
 * "@(#) Copyright (c) 1984, 1993\n\
 *	The Regents of the University of California.  All rights reserved.\n";
 *
 * "@(#)arp.c	8.2 (Berkeley) 1/2/94";
 */

/*
 * ndp - display, set, delete and flush neighbor cache
 */


#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif /* __FreeBSD__ >= 3 */
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#ifndef __NetBSD__
#include <netinet/if_ether.h>
#endif

#include <netinet/icmp6.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <netdb.h>
#include <errno.h>
#include <nlist.h>
#include <stdio.h>
#include <string.h>
#include <paths.h>
#include <err.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "gmt2local.h"

/* packing rule for routing socket */
#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

extern int errno;
static int pid;
static int fflag;
static int nflag;
static int tflag;
static int32_t thiszone;	/* time difference with gmt */
static int s = -1;
static int repeat = 0;

char ntop_buf[INET6_ADDRSTRLEN];	/* inet_ntop() */
char ifix_buf[IFNAMSIZ];		/* if_indextoname() */

int main __P((int, char **));
int file __P((char *));
void getsocket __P((void));
int set __P((int, char **));
void get __P((char *));
int delete __P((char *));
void dump __P((struct in6_addr *));
static struct in6_nbrinfo *getnbrinfo __P((struct in6_addr *addr, int ifindex));
static char *ether_str __P((struct sockaddr_dl *));
int ndp_ether_aton __P((char *, u_char *));
void usage __P((void));
int rtmsg __P((int));
void ifinfo __P((char *));
void rtrlist __P((void));
void plist __P((void));
void pfx_flush __P((void));
void rtr_flush __P((void));
void harmonize_rtr __P((void));
static char *sec2str __P((time_t t));
static char *ether_str __P((struct sockaddr_dl *sdl));
static void ts_print __P((const struct timeval *));

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	int aflag = 0, cflag = 0, dflag = 0, sflag = 0, Hflag = 0,
		pflag = 0, rflag = 0, Pflag = 0, Rflag = 0;
	extern char *optarg;
	extern int optind;

	pid = getpid();
	thiszone = gmt2local(0);
	while ((ch = getopt(argc, argv, "acndfiprstA:HPR")) != EOF)
		switch ((char)ch) {
		case 'a':
			aflag = 1;
			break;
		case 'c':
			fflag = 1;
			cflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'i' :
			if (argc != 3)
				usage();
			ifinfo(argv[2]);
			exit(0);
		case 'n':
			nflag = 1;
			continue;
		case 'p':
			pflag = 1;
			break;
		case 'f' :
			if (argc != 3)
				usage();
			file(argv[2]);
			exit(0);
		case 'r' :
			rflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'A':
			aflag = 1;
			repeat = atoi(optarg);
			if (repeat < 0)
				usage();
			break;
		case 'H' :
			Hflag = 1;
			break;
		case 'P':
			Pflag = 1;
			break;
		case 'R':
			Rflag = 1;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (aflag || cflag) {
		dump(0);
		exit(0);
	}
	if (dflag) {
		if (argc != 1)
			usage();
		delete(argv[0]);
	}
	if (pflag) {
		plist();
		exit(0);
	}
	if (rflag) {
		rtrlist();
		exit(0);
	}
	if (sflag) {
		if (argc < 2 || argc > 4)
			usage();
		exit(set(argc, argv) ? 1 : 0);
	}
	if (Hflag) {
		harmonize_rtr();
		exit(0);
	}
	if (Pflag) {
		pfx_flush();
		exit(0);
	}
	if (Rflag) {
		rtr_flush();
		exit(0);
	}

	if (argc != 1)
		usage();
	get(argv[0]);
	exit(0);
}

/*
 * Process a file to set standard ndp entries
 */
int
file(name)
	char *name;
{
	FILE *fp;
	int i, retval;
	char line[100], arg[5][50], *args[5];

	if ((fp = fopen(name, "r")) == NULL) {
		fprintf(stderr, "ndp: cannot open %s\n", name);
		exit(1);
	}
	args[0] = &arg[0][0];
	args[1] = &arg[1][0];
	args[2] = &arg[2][0];
	args[3] = &arg[3][0];
	args[4] = &arg[4][0];
	retval = 0;
	while(fgets(line, 100, fp) != NULL) {
		i = sscanf(line, "%s %s %s %s %s", arg[0], arg[1], arg[2],
		    arg[3], arg[4]);
		if (i < 2) {
			fprintf(stderr, "ndp: bad line: %s\n", line);
			retval = 1;
			continue;
		}
		if (set(i, args))
			retval = 1;
	}
	fclose(fp);
	return (retval);
}

void
getsocket()
{
	if (s < 0) {
		s = socket(PF_ROUTE, SOCK_RAW, 0);
		if (s < 0) {
			perror("ndp: socket");
			exit(1);
		}
	}
}

struct	sockaddr_in so_mask = {8, 0, 0, { 0xffffffff}};
struct	sockaddr_in6 blank_sin = {sizeof(blank_sin), AF_INET6 }, sin_m;
struct	sockaddr_dl blank_sdl = {sizeof(blank_sdl), AF_LINK }, sdl_m;
int	expire_time, flags, found_entry;
struct	{
	struct	rt_msghdr m_rtm;
	char	m_space[512];
}	m_rtmsg;

/*
 * Set an individual neighbor cache entry
 */
int
set(argc, argv)
	int argc;
	char **argv;
{
	register struct sockaddr_in6 *sin = &sin_m;
	register struct sockaddr_dl *sdl;
	register struct rt_msghdr *rtm = &(m_rtmsg.m_rtm);
	struct addrinfo hints, *res;
	int gai_error;
	u_char *ea;
	char *host = argv[0], *eaddr = argv[1];

	getsocket();
	argc -= 2;
	argv += 2;
	sdl_m = blank_sdl;
	sin_m = blank_sin;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	gai_error = getaddrinfo(host, NULL, &hints, &res);
	if (gai_error) {
		fprintf(stderr, "ndp: %s: %s\n", host,
			gai_strerror(gai_error));
		return 1;
	}
	sin->sin6_addr = ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
	ea = (u_char *)LLADDR(&sdl_m);
	if (ndp_ether_aton(eaddr, ea) == 0)
		sdl_m.sdl_alen = 6;
	flags = expire_time = 0;
	while (argc-- > 0) {
		if (strncmp(argv[0], "temp", 4) == 0) {
			struct timeval time;
			gettimeofday(&time, 0);
			expire_time = time.tv_sec + 20 * 60;
		}
		argv++;
	}
tryagain:
	if (rtmsg(RTM_GET) < 0) {
		perror(host);
		return (1);
	}
	sin = (struct sockaddr_in6 *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(ROUNDUP(sin->sin6_len) + (char *)sin);
	if (IN6_ARE_ADDR_EQUAL(&sin->sin6_addr, &sin_m.sin6_addr)) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) switch (sdl->sdl_type) {
		case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
		case IFT_ISO88024: case IFT_ISO88025:
			goto overwrite;
		}
		goto tryagain;
	}
overwrite:
	if (sdl->sdl_family != AF_LINK) {
		printf("cannot intuit interface index and type for %s\n", host);
		return (1);
	}
	sdl_m.sdl_type = sdl->sdl_type;
	sdl_m.sdl_index = sdl->sdl_index;
	return (rtmsg(RTM_ADD));
}

/*
 * Display an individual neighbor cache entry
 */
void
get(host)
	char *host;
{
	struct sockaddr_in6 *sin = &sin_m;
	struct addrinfo hints, *res;
	int gai_error;

	sin_m = blank_sin;
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	gai_error = getaddrinfo(host, NULL, &hints, &res);
	if (gai_error) {
		fprintf(stderr, "ndp: %s: %s\n", host,
			gai_strerror(gai_error));
		return;
	}
	sin->sin6_addr = ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
	dump(&sin->sin6_addr);
	if (found_entry == 0) {
		printf("%s (%s) -- no entry\n",
		    host, inet_ntop(AF_INET6, &sin->sin6_addr, ntop_buf,
				sizeof(ntop_buf)));
		exit(1);
	}
}

/*
 * Delete a neighbor cache entry
 */
int
delete(host)
	char *host;
{
	register struct sockaddr_in6 *sin = &sin_m;
	register struct rt_msghdr *rtm = &m_rtmsg.m_rtm;
	struct sockaddr_dl *sdl;
	struct addrinfo hints, *res;
	int gai_error;

	getsocket();
	sin_m = blank_sin;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	gai_error = getaddrinfo(host, NULL, &hints, &res);
	if (gai_error) {
		fprintf(stderr, "ndp: %s: %s\n", host,
			gai_strerror(gai_error));
		return 1;
	}
	sin->sin6_addr = ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
/*tryagain:*/
	if (rtmsg(RTM_GET) < 0) {
		perror(host);
		return (1);
	}
	sin = (struct sockaddr_in6 *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(ROUNDUP(sin->sin6_len) + (char *)sin);
	if (IN6_ARE_ADDR_EQUAL(&sin->sin6_addr, &sin_m.sin6_addr)) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) switch (sdl->sdl_type) {
		case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
		case IFT_ISO88024: case IFT_ISO88025:
			goto delete;
		}
	}
delete:
	if (sdl->sdl_family != AF_LINK) {
		printf("cannot locate %s\n", host);
		return (1);
	}
	if (rtmsg(RTM_DELETE) == 0)
		printf("%s (%s) deleted\n", host,
			inet_ntop(AF_INET6, &sin->sin6_addr, ntop_buf,
					sizeof(ntop_buf)));
	return 0;
}

/*
 * Dump the entire neighbor cache
 */
void
dump(addr)
	struct in6_addr *addr;
{
	int mib[6];
	size_t needed;
	char *host, *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_in6 *sin;
	struct sockaddr_dl *sdl;
	extern int h_errno;
	struct hostent *hp;
	struct in6_nbrinfo *nbi;
	struct timeval time;

	/* Print header */
	if (!tflag)
		printf("%-29.29s %-18.18s %6.6s %-9.9s %2s %4s %4s\n",
		       "Neighbor", "Linklayer Address", "Netif", "Expire",
		       "St", "Flgs", "Prbs");

again:;
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(1, "sysctl(PF_ROUTE estimate)");
	if (needed > 0) {
		if ((buf = malloc(needed)) == NULL)
			errx(1, "malloc");
		if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
			err(1, "sysctl(PF_ROUTE, NET_RT_FLAGS)");
		lim = buf + needed;
	} else
		buf = lim = NULL;

	for (next = buf; next && next < lim; next += rtm->rtm_msglen) {
		int isrouter = 0, prbs = 0;

		rtm = (struct rt_msghdr *)next;
		sin = (struct sockaddr_in6 *)(rtm + 1);
		sdl = (struct sockaddr_dl *)((char *)sin + ROUNDUP(sin->sin6_len));
		if (addr) {
			if (!IN6_ARE_ADDR_EQUAL(addr, &sin->sin6_addr))
				continue;
			found_entry = 1;
		} else if (IN6_IS_ADDR_MULTICAST(&sin->sin6_addr))
			continue;
		if (fflag == 1) {
			delete((char *)inet_ntop(AF_INET6, &sin->sin6_addr,
						ntop_buf, sizeof(ntop_buf)));
			continue;
		}
		host = NULL;
		if (nflag == 0) {
			hp = gethostbyaddr((char *)&sin->sin6_addr,
					   sizeof(struct in6_addr), AF_INET6);
			if (hp)
				host = hp->h_name;
		}
		if (host == NULL) {
			inet_ntop(AF_INET6, &sin->sin6_addr,
				  ntop_buf, sizeof(ntop_buf));
			host = ntop_buf;
		}

		gettimeofday(&time, 0);
		if (tflag)
			ts_print(&time);

		printf("%-29.29s %-18.18s %6.6s", host,
		       ether_str(sdl),
		       if_indextoname(sdl->sdl_index, ifix_buf));

		/* Print neighbor discovery specific informations */
		putchar(' ');
		nbi = getnbrinfo(&sin->sin6_addr, sdl->sdl_index);
		if (nbi) {
			if (nbi->expire > time.tv_sec) {
				printf(" %-9.9s",
				       sec2str(nbi->expire - time.tv_sec));
			}
			else if (nbi->expire == 0)
				printf(" %-9.9s", "permanent");
			else
				printf(" %-9.9s", "expired");

			switch(nbi->state) {
			 case ND6_LLINFO_NOSTATE:
				 printf(" N");
				 break;
			 case ND6_LLINFO_WAITDELETE:
				 printf(" W");
				 break;
			 case ND6_LLINFO_INCOMPLETE:
				 printf(" I");
				 break;
			 case ND6_LLINFO_REACHABLE:
				 printf(" R");
				 break;
			 case ND6_LLINFO_STALE:
				 printf(" S");
				 break;
			 case ND6_LLINFO_DELAY:
				 printf(" D");
				 break;
			 case ND6_LLINFO_PROBE:
				 printf(" P");
				 break;
			 default:
				 printf(" ?");
				 break;
			}

			isrouter = nbi->isrouter;
			prbs = nbi->asked;
		}
		else {
			warnx("failed to get neighbor information");
			printf("  ");
		}

		/* other flags */
		putchar(' ');
		{
			u_char flgbuf[8], *p = flgbuf;

			flgbuf[0] = '\0';
			if (isrouter)
				p += sprintf((char *)p, "R");
#ifndef RADISH
			if (rtm->rtm_addrs & RTA_NETMASK) {
				sin = (struct sockaddr_in6 *)
					(sdl->sdl_len + (char *)sdl);
				if (!IN6_IS_ADDR_UNSPECIFIED(&sin->sin6_addr))
					p += sprintf((char *)p, "P");
				if (sin->sin6_len != sizeof(struct sockaddr_in6))
					p += sprintf((char *)p, "W");
			}
#endif /*RADISH*/
			printf("%4s", flgbuf);
		}

		putchar(' ');
		if (prbs)
			printf("% 4d", prbs);

		printf("\n");
	}

	if (repeat) {
		printf("\n");
		sleep(repeat);
		goto again;
	}
}

static struct in6_nbrinfo *
getnbrinfo(addr, ifindex)
	struct in6_addr *addr;
	int ifindex;
{
	static struct in6_nbrinfo nbi;
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	bzero(&nbi, sizeof(nbi));
	if_indextoname(ifindex, nbi.ifname);
	nbi.addr = *addr;
	if (ioctl(s, SIOCGNBRINFO_IN6, (caddr_t)&nbi) < 0) {
		warn("ioctl");
		close(s);
		return(NULL);
	}

	close(s);
	return(&nbi);
}

static char *
ether_str(sdl)
	struct sockaddr_dl *sdl;
{
	static char ebuf[32];
	u_char *cp;

	if (sdl->sdl_alen) {
		cp = (u_char *)LLADDR(sdl);
		sprintf(ebuf, "%x:%x:%x:%x:%x:%x",
			cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
	}
	else {
		sprintf(ebuf, "(incomplete)");
	}

	return(ebuf);
}

int
ndp_ether_aton(a, n)
	char *a;
	u_char *n;
{
	int i, o[6];

	i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o[0], &o[1], &o[2],
					   &o[3], &o[4], &o[5]);
	if (i != 6) {
		fprintf(stderr, "ndp: invalid Ethernet address '%s'\n", a);
		return (1);
	}
	for (i=0; i<6; i++)
		n[i] = o[i];
	return (0);
}

void
usage()
{
	printf("usage: ndp hostname\n");
	printf("       ndp -a[nt]\n");
	printf("       ndp [-nt] -A wait\n");
	printf("       ndp -c[nt]\n");
	printf("       ndp -d[nt] hostname\n");
	printf("       ndp -f[nt] filename\n");
	printf("       ndp -i interface\n");
	printf("       ndp -p\n");
	printf("       ndp -r\n");
	printf("       ndp -s hostname ether_addr [temp]\n");
	printf("       ndp -H\n");
	printf("       ndp -P\n");
	printf("       ndp -R\n");
	exit(1);
}

int
rtmsg(cmd)
	int cmd;
{
	static int seq;
	int rlen;
	register struct rt_msghdr *rtm = &m_rtmsg.m_rtm;
	register char *cp = m_rtmsg.m_space;
	register int l;

	errno = 0;
	if (cmd == RTM_DELETE)
		goto doit;
	bzero((char *)&m_rtmsg, sizeof(m_rtmsg));
	rtm->rtm_flags = flags;
	rtm->rtm_version = RTM_VERSION;

	switch (cmd) {
	default:
		fprintf(stderr, "ndp: internal wrong cmd\n");
		exit(1);
	case RTM_ADD:
		rtm->rtm_addrs |= RTA_GATEWAY;
		rtm->rtm_rmx.rmx_expire = expire_time;
		rtm->rtm_inits = RTV_EXPIRE;
		rtm->rtm_flags |= (RTF_HOST | RTF_STATIC);
		/* FALLTHROUGH */
	case RTM_GET:
		rtm->rtm_addrs |= RTA_DST;
	}
#define NEXTADDR(w, s) \
	if (rtm->rtm_addrs & (w)) { \
		bcopy((char *)&s, cp, sizeof(s)); cp += sizeof(s);}

	NEXTADDR(RTA_DST, sin_m);
	NEXTADDR(RTA_GATEWAY, sdl_m);
	NEXTADDR(RTA_NETMASK, so_mask);

	rtm->rtm_msglen = cp - (char *)&m_rtmsg;
doit:
	l = rtm->rtm_msglen;
	rtm->rtm_seq = ++seq;
	rtm->rtm_type = cmd;
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		if (errno != ESRCH || cmd != RTM_DELETE) {
			perror("writing to routing socket");
			return (-1);
		}
	}
	do {
		l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
	} while (l > 0 && (rtm->rtm_seq != seq || rtm->rtm_pid != pid));
	if (l < 0)
		(void) fprintf(stderr, "ndp: read from routing socket: %s\n",
		    strerror(errno));
	return (0);
}

void
ifinfo(ifname)
	char *ifname;
{
	struct in6_ndireq nd;
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("ndp: socket");
		exit(1);
	}
	bzero(&nd, sizeof(nd));
	strcpy(nd.ifname, ifname);
	if (ioctl(s, SIOCGIFINFO_IN6, (caddr_t)&nd) < 0) {
 		perror("ioctl (SIOCGIFINFO_IN6)");
 		exit(1);
 	}
#define ND nd.ndi
	printf("linkmtu=%d", ND.linkmtu);
	printf(", curhlim=%d", ND.chlim);
	printf(", basereachable=%ds%dms",
	       ND.basereachable / 1000, ND.basereachable % 1000);
	printf(", reachable=%ds", ND.reachable);
	printf(", retrans=%ds%dms\n", ND.retrans / 1000, ND.retrans % 1000);
#undef ND
	close(s);
}

void
rtrlist()
{
	struct in6_drlist dr;
	int s, i;
	struct timeval time;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("ndp: socket");
		exit(1);
	}
	bzero(&dr, sizeof(dr));
	strcpy(dr.ifname, "lo0"); /* dummy */
	if (ioctl(s, SIOCGDRLST_IN6, (caddr_t)&dr) < 0) {
 		perror("ioctl (SIOCGDRLST_IN6)");
 		exit(1);
 	}
#define DR dr.defrouter[i]
	for (i = 0 ; DR.if_index && i < PRLSTSIZ ; i++) {
		printf("%s if=%s", inet_ntop(AF_INET6, &DR.rtaddr,
					     ntop_buf, sizeof(ntop_buf)),
		       if_indextoname(DR.if_index, ifix_buf));
		printf(", flags=%s%s",
		       DR.flags & ND_RA_FLAG_MANAGED ? "M" : "",
		       DR.flags & ND_RA_FLAG_OTHER   ? "O" : "");
		gettimeofday(&time, 0);
		if (DR.expire == 0)
			printf(", expire=Never\n");
		else
			printf(", expire=%s\n",
				sec2str(DR.expire - time.tv_sec));
	}
#undef DR
	close(s);
}

void
plist()
{
	struct in6_prlist pr;
	int s, i;
	struct timeval time;

	gettimeofday(&time, 0);

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("ndp: socket");
		exit(1);
	}
	bzero(&pr, sizeof(pr));
	strcpy(pr.ifname, "lo0"); /* dummy */
	if (ioctl(s, SIOCGPRLST_IN6, (caddr_t)&pr) < 0) {
 		perror("ioctl (SIOCGPRLST_IN6)");
 		exit(1);
 	}
#define PR pr.prefix[i]
	for (i = 0; PR.if_index && i < PRLSTSIZ ; i++) {
		printf("%s/%d if=%s\n",
		       inet_ntop(AF_INET6, &PR.prefix, ntop_buf,
				 sizeof(ntop_buf)), PR.prefixlen,
		       if_indextoname(PR.if_index, ifix_buf));
		gettimeofday(&time, 0);
		printf("  flags=%s%s",
		       PR.raflags.onlink ? "L" : "",
		       PR.raflags.autonomous ? "A" : "");
		if (PR.vltime == ND6_INFINITE_LIFETIME)
			printf(" vltime=infinity");
		else
			printf(" vltime=%ld", (long)PR.vltime);
		if (PR.pltime == ND6_INFINITE_LIFETIME)
			printf(", pltime=infinity");
		else
			printf(", pltime=%ld", (long)PR.pltime);
		if (PR.expire == 0)
			printf(", expire=Never\n");
		else if (PR.expire >= time.tv_sec)
			printf(", expire=%s\n",
				sec2str(PR.expire - time.tv_sec));
		else
			printf(", expired\n");
		if (PR.advrtrs) {
			int j;
			printf("  advertised by\n");
			for (j = 0; j < PR.advrtrs; j++) {
				printf("    %s\n",
				       inet_ntop(AF_INET6, &PR.advrtr[j],
						 ntop_buf,
						 sizeof(ntop_buf)));
			}
			if (PR.advrtrs > DRLSTSIZ)
				printf("    and %d routers\n",
				       PR.advrtrs - DRLSTSIZ);
		}
		else
			printf("  No advertising router\n");
	}
#undef PR
	close(s);
}

void
pfx_flush()
{
	char dummyif[IFNAMSIZ+8];
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");
	strcpy(dummyif, "lo0"); /* dummy */
	if (ioctl(s, SIOCSPFXFLUSH_IN6, (caddr_t)&dummyif) < 0)
 		err(1, "ioctl(SIOCSPFXFLUSH_IN6)");
}

void
rtr_flush()
{
	char dummyif[IFNAMSIZ+8];
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");
	strcpy(dummyif, "lo0"); /* dummy */
	if (ioctl(s, SIOCSRTRFLUSH_IN6, (caddr_t)&dummyif) < 0)
 		err(1, "ioctl(SIOCSRTRFLUSH_IN6)");
}

void
harmonize_rtr()
{
	char dummyif[IFNAMSIZ+8];
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("ndp: socket");
		exit(1);
	}
	strcpy(dummyif, "lo0"); /* dummy */
	if (ioctl(s, SIOCSNDFLUSH_IN6, (caddr_t)&dummyif) < 0) {
 		perror("ioctl (SIOCSNDFLUSH_IN6)");
 		exit(1);
 	}
}

static char *
sec2str(total)
	time_t total;
{
	static char result[256];
	int days, hours, mins, secs;
	int first = 1;
	char *p = result;

	days = total / 3600 / 24;
	hours = (total / 3600) % 24;
	mins = (total / 60) % 60;
	secs = total % 60;

	if (days) {
		first = 0;
		p += sprintf(p, "%dd", days);
	}
	if (!first || hours) {
		first = 0;
		p += sprintf(p, "%dh", hours);
	}
	if (!first || mins) {
		first = 0;
		p += sprintf(p, "%dm", mins);
	}
	sprintf(p, "%ds", secs);

	return(result);
}

/*
 * Print the timestamp
 * from tcpdump/util.c
 */
static void
ts_print(tvp)
	const struct timeval *tvp;
{
	int s;

	/* Default */
	s = (tvp->tv_sec + thiszone) % 86400;
	(void)printf("%02d:%02d:%02d.%06u ",
	    s / 3600, (s % 3600) / 60, s % 60, (u_int32_t)tvp->tv_usec);
}
