/*	$NetBSD: ipt.c,v 1.3.4.1 2002/02/09 16:55:44 he Exp $	*/

/*
 * Copyright (C) 1993-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#ifdef	__FreeBSD__
# ifndef __FreeBSD_cc_version
#  include <osreldate.h>
# else
#  if __FreeBSD_cc_version < 430000
#   include <osreldate.h>
#  endif
# endif
#endif
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__) && !defined(__sgi)
#include <strings.h>
#else
#if !defined(__sgi)
#include <sys/byteorder.h>
#endif
#include <sys/file.h>
#endif
#include <sys/param.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifndef	linux
#include <netinet/ip_var.h>
#endif
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <ctype.h>
#include "ip_compat.h"
#include <netinet/tcpip.h>
#include "ip_fil.h"
#include "ip_nat.h"
#include "ip_state.h"
#include "ip_frag.h"
#include "ipf.h"
#include "ipt.h"

#if !defined(lint)
static const char sccsid[] = "@(#)ipt.c	1.19 6/3/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)Id: ipt.c,v 2.6.2.14 2002/01/15 14:36:53 darrenr Exp";
#endif

extern	char	*optarg;
extern	struct frentry	*ipfilter[2][2];
extern	struct ipread	snoop, etherf, tcpd, pcap, iptext, iphex;
extern	struct ifnet	*get_unit __P((char *, int));
extern	void	init_ifp __P((void));
extern	ipnat_t	*natparse __P((char *, int));
extern	int	fr_running;

int	opts = 0;
int	use_inet6 = 0;
int	main __P((int, char *[]));
int	loadrules __P((char *));
int	kmemcpy __P((char *, long, int));
void	dumpnat __P((void));
void	dumpstate __P((void));
char	*getifname __P((void *));

int main(argc,argv)
int argc;
char *argv[];
{
	char	*datain, *iface, *ifname, *packet;
	int	fd, i, dir, c, loaded, dump, hlen;
	struct	ifnet	*ifp;
	struct	ipread	*r;
	u_long	buf[2048];
	ip_t	*ip;

	dir = 0;
	dump = 0;
	loaded = 0;
	r = &iptext;
	iface = NULL;
	ifname = "anon0";
	datain = NULL;

	nat_init();
	fr_stateinit();
	initparse();
	fr_running = 1;

	while ((c = getopt(argc, argv, "6bdDEHi:I:NoPr:STvxX")) != -1)
		switch (c)
		{
#ifdef	USE_INET6
		case '6' :
			use_inet6 = 1;
			break;
#endif
		case 'b' :
			opts |= OPT_BRIEF;
			break;
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'D' :
			dump = 1;
			break;
		case 'i' :
			datain = optarg;
			break;
		case 'I' :
			ifname = optarg;
			break;
		case 'o' :
			opts |= OPT_SAVEOUT;
			break;
		case 'r' :
			if (loadrules(optarg) == -1)
				return -1;
			loaded = 1;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		case 'E' :
			r = &etherf;
			break;
		case 'H' :
			r = &iphex;
			break;
		case 'N' :
			opts |= OPT_NAT;
			break;
		case 'P' :
			r = &pcap;
			break;
		case 'S' :
			r = &snoop;
			break;
		case 'T' :
			r = &tcpd;
			break;
		case 'x' :
			opts |= OPT_HEX;
			break;
		case 'X' :
			r = &iptext;
			break;
		}

	if (loaded == 0) {
		(void)fprintf(stderr,"no rules loaded\n");
		exit(-1);
	}

	if (opts & OPT_SAVEOUT)
		init_ifp();

	if (datain)
		fd = (*r->r_open)(datain);
	else
		fd = (*r->r_open)("-");

	if (fd < 0)
		exit(-1);

	ip = (ip_t *)buf;
	while ((i = (*r->r_readip)((char *)buf, sizeof(buf),
				    &iface, &dir)) > 0) {
		if (iface == NULL || *iface == '\0')
			iface = ifname;
		ifp = get_unit(iface, ip->ip_v);
		hlen = 0;
		if (!use_inet6) {
			ip->ip_off = ntohs(ip->ip_off);
			ip->ip_len = ntohs(ip->ip_len);
			hlen = ip->ip_hl << 2;
		}
#ifdef	USE_INET6
		else
			hlen = sizeof(ip6_t);
#endif
		packet = (char *)buf;
		ipfr_slowtimer();
		i = fr_check(ip, hlen, ifp, dir, (mb_t **)&packet);
		if ((opts & OPT_NAT) == 0)
			switch (i)
			{
			case -2 :
				(void)printf("auth");
				break;
			case -1 :
				(void)printf("block");
				break;
			case 0 :
				(void)printf("pass");
				break;
			case 1 :
				(void)printf("nomatch");
				break;
			}
		if (!use_inet6) {
			ip->ip_off = htons(ip->ip_off);
			ip->ip_len = htons(ip->ip_len);
		}

		if (!(opts & OPT_BRIEF)) {
			putchar(' ');
			printpacket((ip_t *)buf);
			printf("--------------");
		} else if ((opts & (OPT_BRIEF|OPT_NAT)) == (OPT_NAT|OPT_BRIEF))
			printpacket((ip_t *)buf);
#ifndef	linux
		if (dir && (ifp != NULL) && ip->ip_v && (packet != NULL))
# ifdef __sgi
			(*ifp->if_output)(ifp, (void *)packet, NULL);
# else
			(*ifp->if_output)(ifp, (void *)packet, NULL, 0);
# endif
#endif
		if ((opts & (OPT_BRIEF|OPT_NAT)) != (OPT_NAT|OPT_BRIEF))
			putchar('\n');
		dir = 0;
		if (iface != ifname) {
			free(iface);
			iface = ifname;
		}
	}
	(*r->r_close)();

	if (dump == 1)  {
		dumpnat();
		dumpstate();
	}

	return 0;
}


/*
 * Load in either NAT or ipf rules from a file, which is treated as stdin
 * if the name is "-".  NOTE, stdin can only be used once as the file is
 * closed after use.
 */
int loadrules(file)
char *file;
{
	char	line[513], *s;
	int     linenum, i;
	void	*fr;
	FILE	*fp;

	if (!strcmp(file, "-"))
		fp = stdin;
	else if (!(fp = fopen(file, "r"))) {
		(void)fprintf(stderr, "couldn't open %s\n", file);
		return (-1);
	}

	if (!(opts & OPT_BRIEF))
		(void)printf("opening rule file \"%s\"\n", file);

	linenum = 0;

	while (fgets(line, sizeof(line) - 1, fp)) {
		linenum++;

		/*
		 * treat both CR and LF as EOL
		 */
		if ((s = index(line, '\n')))
			*s = '\0';
		if ((s = index(line, '\r')))
			*s = '\0';

		/*
		 * # is comment marker, everything after is a ignored
		 */
		if ((s = index(line, '#')))
			*s = '\0';

		if (!*line)
			continue;

		/* fake an `ioctl' call :) */

		if ((opts & OPT_NAT) != 0) {
			if (!(fr = natparse(line, linenum)))
				continue;

			i = IPL_EXTERN(ioctl)(IPL_LOGNAT, SIOCADNAT,
					      (caddr_t)&fr, FWRITE|FREAD);
			if (opts & OPT_DEBUG)
				fprintf(stderr, "iplioctl(ADNAT,%p,1) = %d\n",
					fr, i);
		} else {
			if (!(fr = parse(line, linenum)))
				continue;

			i = IPL_EXTERN(ioctl)(0, SIOCADAFR, (caddr_t)&fr,
					      FWRITE|FREAD);
			if (opts & OPT_DEBUG)
				fprintf(stderr, "iplioctl(ADAFR,%p,1) = %d\n",
					fr, i);
		}
	}
	(void)fclose(fp);

	return 0;
}


int kmemcpy(addr, offset, size)
char *addr;
long offset;
int size;
{
	bcopy((char *)offset, addr, size);
	return 0;
}


/*
 * Display the built up NAT table rules and mapping entries.
 */
void dumpnat()
{
	ipnat_t	*ipn;
	nat_t	*nat;

	printf("List of active MAP/Redirect filters:\n");
	for (ipn = nat_list; ipn != NULL; ipn = ipn->in_next)
		printnat(ipn, opts & (OPT_DEBUG|OPT_VERBOSE));
	printf("\nList of active sessions:\n");
	for (nat = nat_instances; nat; nat = nat->nat_next)
		printactivenat(nat, opts);
}


/*
 * Display the built up state table rules and mapping entries.
 */
void dumpstate()
{
	ipstate_t *ips;

	printf("List of active state sessions:\n");
	for (ips = ips_list; ips != NULL; )
		ips = printstate(ips, opts & (OPT_DEBUG|OPT_VERBOSE));
}


/*
 * Given a pointer to an interface in the kernel, return a pointer to a
 * string which is the interface name.
 */
char *getifname(ptr)
void *ptr;
{
#if defined(NetBSD) && (NetBSD >= 199905) && (NetBSD < 1991011) || \
    defined(__OpenBSD__)
#else
	char buf[32];
	int len;
#endif
	struct ifnet netif;

	if (ptr == (void *)-1)
		return "!";
	if (ptr == NULL)
		return "-";

	if (kmemcpy((char *)&netif, (u_long)ptr, sizeof(netif)) == -1)
		return "X";
#if defined(NetBSD) && (NetBSD >= 199905) && (NetBSD < 1991011) || \
    defined(__OpenBSD__)
	return strdup(netif.if_xname);
#else
	if (kmemcpy(buf, (u_long)netif.if_name, sizeof(buf)) == -1)
		return "X";
	if (netif.if_unit < 10)
		len = 2;
	else if (netif.if_unit < 1000)
		len = 3;
	else if (netif.if_unit < 10000)
		len = 4;
	else
		len = 5;
	buf[sizeof(buf) - len] = '\0';
	sprintf(buf + strlen(buf), "%d", netif.if_unit % 10000);
	return strdup(buf);
#endif
}
