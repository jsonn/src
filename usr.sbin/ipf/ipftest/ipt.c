/*	$NetBSD: ipt.c,v 1.6.2.2 1997/11/17 16:26:57 mrg Exp $	*/

/*
 * Copyright (C) 1993-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#ifdef	__FreeBSD__
# include <osreldate.h>
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
#include <netinet/ip_compat.h>
#include <netinet/tcpip.h>
#include <netinet/ip_fil.h>
#include "ipf.h"
#include "ipt.h"

#if !defined(lint)
static const char sccsid[] = "@(#)ipt.c	1.19 6/3/96 (C) 1993-1996 Darren Reed";
static const char rcsid[] = "@(#)Id: ipt.c,v 2.0.2.12.2.1 1997/11/12 10:58:10 darrenr Exp ";
#endif

extern	char	*optarg;
extern	struct frentry	*ipfilter[2][2];
extern	struct ipread	snoop, etherf, tcpd, pcap, iptext, iphex;
extern	struct ifnet	*get_unit __P((char *));
extern	void	init_ifp __P((void));

int	opts = 0;
int	main __P((int, char *[]));

int main(argc,argv)
int argc;
char *argv[];
{
	struct	ipread	*r = &iptext;
	u_long	buf[64];
	struct	ifnet	*ifp;
	char	*rules = NULL, *datain = NULL, *iface = NULL;
	ip_t	*ip;
	int	fd, i, dir = 0, c;

	while ((c = getopt(argc, argv, "bdEHi:I:oPr:STvX")) != -1)
		switch (c)
		{
		case 'b' :
			opts |= OPT_BRIEF;
			break;
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'i' :
			datain = optarg;
			break;
		case 'I' :
			iface = optarg;
			break;
		case 'o' :
			opts |= OPT_SAVEOUT;
			break;
		case 'r' :
			rules = optarg;
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
		case 'P' :
			r = &pcap;
			break;
		case 'S' :
			r = &snoop;
			break;
		case 'T' :
			r = &tcpd;
			break;
		case 'X' :
			r = &iptext;
			break;
		}

	if (!rules) {
		(void)fprintf(stderr,"no rule file present\n");
		exit(-1);
	}

	initparse();

	if (rules) {
		struct	frentry *fr;
		char	line[513], *s;
		FILE	*fp;

		if (!strcmp(rules, "-"))
			fp = stdin;
		else if (!(fp = fopen(rules, "r"))) {
			(void)fprintf(stderr, "couldn't open %s\n", rules);
			exit(-1);
		}
		if (!(opts & OPT_BRIEF))
			(void)printf("opening rule file \"%s\"\n", rules);
		while (fgets(line, sizeof(line)-1, fp)) {
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

			if (!(fr = parse(line)))
				continue;
			/* fake an `ioctl' call :) */
			i = IPL_EXTERN(ioctl)(0, SIOCADDFR, (caddr_t)fr, FWRITE|FREAD);
			if (opts & OPT_DEBUG)
				fprintf(stderr,
					"iplioctl(SIOCADDFR,%p,1) = %d\n",
					fr, i);
		}
		(void)fclose(fp);
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
		ifp = iface ? get_unit(iface) : NULL;
		ip->ip_off = ntohs(ip->ip_off);
		ip->ip_len = ntohs(ip->ip_len);
		switch (fr_check(ip, ip->ip_hl << 2, ifp, dir, (mb_t **)&buf))
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
		if (!(opts & OPT_BRIEF)) {
			putchar(' ');
			printpacket((ip_t *)buf);
			printf("--------------");
		}
#ifndef	linux
		if (dir && ifp && ip->ip_v)
# ifdef __sgi
			(*ifp->if_output)(ifp, (void *)buf, NULL);
# else
			(*ifp->if_output)(ifp, (void *)buf, NULL, 0);
# endif
#endif
		putchar('\n');
		dir = 0;
	}
	(*r->r_close)();
	return 0;
}
