/*	$NetBSD: ipft_hx.c,v 1.1.1.11 1998/11/22 14:21:48 mrg Exp $	*/

/*
 * Copyright (C) 1995-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
#include <ctype.h>
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
#include <netinet/in_systm.h>
#ifndef	linux
#include <netinet/ip_var.h>
#endif
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netinet/ip_compat.h>
#include <netinet/tcpip.h>
#include "ipf.h"
#include "ipt.h"

#if !defined(lint)
static const char sccsid[] = "@(#)ipft_hx.c	1.1 3/9/96 (C) 1996 Darren Reed";
static const char rcsid[] = "@(#)Id: ipft_hx.c,v 2.0.2.8.2.2 1998/11/22 01:50:33 darrenr Exp ";
#endif

extern	int	opts;

static	int	hex_open __P((char *));
static	int	hex_close __P((void));
static	int	hex_readip __P((char *, int, char **, int *));
static	char	*readhex __P((char *, char *));

struct	ipread	iphex = { hex_open, hex_close, hex_readip };
static	FILE	*tfp = NULL;
static	int	tfd = -1;

static	int	hex_open(fname)
char	*fname;
{
	if (tfp && tfd != -1) {
		rewind(tfp);
		return tfd;
	}

	if (!strcmp(fname, "-")) {
		tfd = 0;
		tfp = stdin;
	} else {
		tfd = open(fname, O_RDONLY);
		if (tfd != -1)
			tfp = fdopen(tfd, "r");
	}
	return tfd;
}


static	int	hex_close()
{
	int	cfd = tfd;

	tfd = -1;
	return close(cfd);
}


static	int	hex_readip(buf, cnt, ifn, dir)
char	*buf, **ifn;
int	cnt, *dir;
{
	register char *s, *t, *u;
	char	line[513];
	ip_t	*ip;

 	ip = (ip_t *)buf;
	while (fgets(line, sizeof(line)-1, tfp)) {
		if ((s = index(line, '\n'))) {
			if (s == line)
				return (char *)ip - buf;
			*s = '\0';
		}
		if ((s = index(line, '#')))
			*s = '\0';
		if (!*line)
			continue;
		if (!(opts & OPT_BRIEF)) {
			printf("input: %s\n", line);
			fflush(stdout);
		}

		/*
		 * interpret start of line as possibly "[ifname]" or
		 * "[in/out,ifname]".
		 */
		if (ifn)
			*ifn = NULL;
		if (dir)
			*dir = 0;
		if ((*buf == '[') && (s = index(line, ']'))) {
			t = buf + 1;
			if (t - s > 0) {
				if ((u = index(t, ',')) && (u < s)) {
					u++;
					if (ifn)
						*ifn = u;
					if (dir) {
						if (*t == 'i')
							*dir = 0;
						else if (*t == 'o')
							*dir = 1;
					}
				} else if (ifn)
					*ifn = t;
				*s++ = '\0';
			}
		} else
			s = line;
		ip = (ip_t *)readhex(s, (char *)ip);
	}
	return -1;
}


static	char	*readhex(src, dst)
register char	*src, *dst;
{
	int	state = 0;
	char	c;

	while ((c = *src++)) {
		if (isspace(c)) {
			if (state) {
				dst++;
				state = 0;
			}
			continue;
		} else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
			   (c >= 'A' && c <= 'F')) {
			c = isdigit(c) ? (c - '0') : (toupper(c) - 55);
			if (state == 0) {
				*dst = (c << 4);
				state++;
			} else {
				*dst++ |= c;
				state = 0;
			}
		} else
			break;
	}
	return dst;
}
