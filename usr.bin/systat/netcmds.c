/*	$NetBSD: netcmds.c,v 1.15.4.1 2000/09/01 16:37:09 ad Exp $	*/

/*-
 * Copyright (c) 1980, 1992, 1993
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
static char sccsid[] = "@(#)netcmds.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: netcmds.c,v 1.15.4.1 2000/09/01 16:37:09 ad Exp $");
#endif /* not lint */

/*
 * Common network command support routines.
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#endif

#include <arpa/inet.h>

#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "systat.h"
#include "extern.h"

#define	streq(a,b)	(strcmp(a,b)==0)

static	struct hitem {
	struct	sockaddr_storage addr;
	int	onoff;
} *hosts = NULL;

int nports, nhosts, protos;

static void changeitems(char *, int);
static void selectproto(char *);
static void showprotos(void);
static int selectport(long, int);
static void showports(void);
static int addrcmp(struct sockaddr *, struct sockaddr *);
static int selecthost(struct sockaddr *, int);
static void showhosts(void);

/* please note: there are also some netstat commands in netstat.c */

void
netstat_display(char *args)
{
	changeitems(args, 1);
}

void
netstat_ignore(char *args)
{
	changeitems(args, 0);
}

void
netstat_reset(char *args)
{
	selectproto(0);
	selecthost(0, 0);
	selectport(-1, 0);
}

void
netstat_show(char *args)
{
	move(CMDLINE, 0); clrtoeol();
	if (!args || *args == '\0') {
		showprotos();
		showhosts();
		showports();
		return;
	}
	if (strstr(args, "protos") == args)
		showprotos();
	else if (strstr(args, "hosts") == args)
		showhosts();
	else if (strstr(args, "ports") == args)
		showports();
	else
		addstr("show what?");
}

void
netstat_tcp(char *args)
{
	selectproto("tcp");
}

void
netstat_udp(char *args)
{
	selectproto("udp");
}

static void
changeitems(char *args, int onoff)
{
	char *cp;
	struct servent *sp;
	struct addrinfo hints, *res, *res0;

	cp = strchr(args, '\n');
	if (cp)
		*cp = '\0';
	for (;;args = cp) {
		for (cp = args; *cp && isspace(*cp); cp++)
			;
		args = cp;
		for (; *cp && !isspace(*cp); cp++)
			;
		if (*cp)
			*cp++ = '\0';
		if (cp - args == 0)
			break;
		sp = getservbyname(args,
		    protos == TCP ? "tcp" : protos == UDP ? "udp" : 0);
		if (sp) {
			selectport(sp->s_port, onoff);
			continue;
		}

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		if (getaddrinfo(args, "0", &hints, &res0) != 0) {
			error("%s: unknown host or port", args);
			continue;
		}
		for (res = res0; res; res = res->ai_next)
			selecthost(res->ai_addr, onoff);
		freeaddrinfo(res0);
	}
}

static void
selectproto(char *proto)
{

	if (proto == 0 || streq(proto, "all"))
		protos = TCP|UDP;
	else if (streq(proto, "tcp"))
		protos = TCP;
	else if (streq(proto, "udp"))
		protos = UDP;
}

static void
showprotos(void)
{

	if ((protos & TCP) == 0)
		addch('!');
	addstr("tcp ");
	if ((protos & UDP) == 0)
		addch('!');
	addstr("udp ");
}

static	struct pitem {
	long	port;
	int	onoff;
} *ports = NULL;

static int
selectport(long port, int onoff)
{
	struct pitem *p;

	if (port == -1) {
		if (ports == NULL)
			return (0);
		free(ports);
		ports = NULL;
		nports = 0;
		return (1);
	}
	for (p = ports; p < ports+nports; p++)
		if (p->port == port) {
			p->onoff = onoff;
			return (0);
		}
	p = (struct pitem *)realloc(ports, (nports+1)*sizeof (*p));
	if (p == NULL) {
		error("malloc failed");
		die(0);
	}
	ports = p;
	p = &ports[nports++];
	p->port = port;
	p->onoff = onoff;
	return (1);
}

int
checkport(struct inpcb *inp)
{
	struct pitem *p;

	if (ports)
	for (p = ports; p < ports+nports; p++)
		if (p->port == inp->inp_lport || p->port == inp->inp_fport)
			return (p->onoff);
	return (1);
}

#ifdef INET6
int
checkport6(struct in6pcb *in6p)
{
	struct pitem *p;

	if (ports)
	for (p = ports; p < ports+nports; p++)
		if (p->port == in6p->in6p_lport || p->port == in6p->in6p_fport)
			return (p->onoff);
	return (1);
}
#endif

static void
showports(void)
{
	struct pitem *p;
	struct servent *sp;

	for (p = ports; p < ports+nports; p++) {
		sp = getservbyport(p->port,
		    protos == (TCP|UDP) ? 0 : protos == TCP ? "tcp" : "udp");
		if (!p->onoff)
			addch('!');
		if (sp)
			printw("%s ", sp->s_name);
		else
			printw("%ld ", p->port);
	}
}

static int
addrcmp(struct sockaddr *sa1, struct sockaddr *sa2)
{
	if (sa1->sa_family != sa2->sa_family)
		return 0;
	if (sa1->sa_len != sa2->sa_len)
		return 0;
	switch (sa1->sa_family) {
	case AF_INET:
		if (((struct sockaddr_in *)sa1)->sin_addr.s_addr ==
				((struct sockaddr_in *)sa2)->sin_addr.s_addr)
			return 1;
		break;
#ifdef INET6
	case AF_INET6:
		if (IN6_ARE_ADDR_EQUAL(&((struct sockaddr_in6 *)sa1)->sin6_addr,
				&((struct sockaddr_in6 *)sa2)->sin6_addr))
			return 1;
		break;
#endif
	default:
		if (memcmp(sa1, sa2, sa1->sa_len) == 0)
			return 1;
		break;
	}
	return 0;
}

static int
selecthost(struct sockaddr *sa, int onoff)
{
	struct hitem *p;

	if (sa == 0) {
		if (hosts == 0)
			return (0);
		free((char *)hosts), hosts = 0;
		nhosts = 0;
		return (1);
	}
	for (p = hosts; p < hosts+nhosts; p++)
		if (addrcmp((struct sockaddr *)&p->addr, sa)) {
			p->onoff = onoff;
			return (0);
		}
	if (sa->sa_len > sizeof(struct sockaddr_storage))
		return (-1);	/*XXX*/
	p = (struct hitem *)realloc(hosts, (nhosts+1)*sizeof (*p));
	if (p == NULL) {
		error("malloc failed");
		die(0);
	}
	hosts = p;
	p = &hosts[nhosts++];
	memcpy(&p->addr, sa, sa->sa_len);
	p->onoff = onoff;
	return (1);
}

int
checkhost(struct inpcb *inp)
{
	struct hitem *p;
	struct sockaddr_in *sin;

	if (hosts)
		for (p = hosts; p < hosts+nhosts; p++) {
			if (((struct sockaddr *)&p->addr)->sa_family != AF_INET)
				continue;
			sin = (struct sockaddr_in *)&p->addr;
			if (sin->sin_addr.s_addr == inp->inp_laddr.s_addr ||
			    sin->sin_addr.s_addr == inp->inp_faddr.s_addr)
				return (p->onoff);
		}
	return (1);
}

#ifdef INET6
int
checkhost6(struct in6pcb *in6p)
{
	struct hitem *p;
	struct sockaddr_in6 *sin6;

	if (hosts)
		for (p = hosts; p < hosts+nhosts; p++) {
			if (((struct sockaddr *)&p->addr)->sa_family != AF_INET6)
				continue;
			sin6 = (struct sockaddr_in6 *)&p->addr;
			if (IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr, &in6p->in6p_laddr) ||
			    IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr, &in6p->in6p_faddr))
				return (p->onoff);
		}
	return (1);
}
#endif

static void
showhosts(void)
{
	struct hitem *p;
	char hbuf[NI_MAXHOST];
	struct sockaddr *sa;
	int flags;

#if 0
	flags = nflag ? NI_NUMERICHOST : 0;
#else
	flags = 0;
#endif
	for (p = hosts; p < hosts+nhosts; p++) {
		sa = (struct sockaddr *)&p->addr;
		if (getnameinfo(sa, sa->sa_len, hbuf, sizeof(hbuf), NULL, 0,
				flags) != 0)
			strcpy(hbuf, "(invalid)");
		if (!p->onoff)
			addch('!');
		printw("%s ", hbuf);
	}
}
