/*	$NetBSD: misc.c,v 1.1.1.6.2.1 1997/10/30 07:16:39 mrg Exp $	*/

/*
 * Copyright (C) 1993-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/param.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcpip.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netinet/ip_compat.h>
#include <netinet/ip_fil.h>
#include "ipf.h"
#include "ipt.h"

#if !defined(lint)
static const char sccsid[] = "@(#)misc.c	1.3 2/4/96 (C) 1995 Darren Reed";
static const char rcsid[] = "@(#)Id: misc.c,v 2.0.2.8 1997/10/19 15:39:28 darrenr Exp ";
#endif

extern	int	opts;


void	printpacket(ip)
struct	ip	*ip;
{
	struct	tcphdr	*tcp;

	tcp = (struct tcphdr *)((char *)ip + (ip->ip_hl << 2));
	printf("ip %d(%d) %d ", ip->ip_len, ip->ip_hl << 2, ip->ip_p);
	if (ip->ip_off & 0x1fff)
		printf("@%d", ip->ip_off << 3);
	(void)printf(" %s", inet_ntoa(ip->ip_src));
	if (!(ip->ip_off & 0x1fff))
		if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
			(void)printf(",%d", ntohs(tcp->th_sport));
	(void)printf(" > ");
	(void)printf("%s", inet_ntoa(ip->ip_dst));
	if (!(ip->ip_off & 0x1fff))
		if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
			(void)printf(",%d", ntohs(tcp->th_dport));
	putchar('\n');
}


#ifdef __STDC__
void	verbose(char *fmt, ...)
#else
void	verbose(fmt, va_alist)
char	*fmt;
va_dcl
#endif
{
	va_list pvar;

	va_start(pvar, fmt);
	if (opts & OPT_VERBOSE)
		vprintf(fmt, pvar);
	va_end(pvar);
}


#ifdef	__STDC__
void	debug(char *fmt, ...)
#else
void	debug(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
	va_list pvar;

	va_start(pvar, fmt);
	if (opts & OPT_DEBUG)
		vprintf(fmt, pvar);
	va_end(pvar);
}
