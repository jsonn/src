/*	$NetBSD: res_init.c,v 1.12.2.2 1998/11/02 03:29:37 lukem Exp $	*/

/*-
 * Copyright (c) 1985, 1989, 1993
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)res_init.c	8.1 (Berkeley) 6/7/93";
static char rcsid[] = "Id: res_init.c,v 8.8 1997/06/01 20:34:37 vixie Exp ";
#else
__RCSID("$NetBSD: res_init.c,v 1.12.2.2 1998/11/02 03:29:37 lukem Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#if defined(_LIBC)
#include "namespace.h"
#endif
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_LIBC) && defined(__weak_alias)
__weak_alias(res_init,_res_init);
#endif

static void res_setoptions __P((char *, char *));
static u_int32_t net_mask __P((struct in_addr));
static const char sort_mask[] = "/&";
#define ISSORTMASK(ch) (strchr(sort_mask, ch) != NULL)

/*
 * Resolver state default settings
 */

struct __res_state _res = {
	RES_TIMEOUT,               	/* retransmition time interval */
	4,                         	/* number of times to retransmit */
	RES_DEFAULT,			/* options flags */
	1,                         	/* number of name servers */
};

/*
 * Set up default settings.  If the configuration file exist, the values
 * there will have precedence.  Otherwise, the server address is set to
 * INADDR_ANY and the default domain name comes from the gethostname().
 *
 * An interrim version of this code (BIND 4.9, pre-4.4BSD) used 127.0.0.1
 * rather than INADDR_ANY ("0.0.0.0") as the default name server address
 * since it was noted that INADDR_ANY actually meant ``the first interface
 * you "ifconfig"'d at boot time'' and if this was a SLIP or PPP interface,
 * it had to be "up" in order for you to reach your own name server.  It
 * was later decided that since the recommended practice is to always 
 * install local static routes through 127.0.0.1 for all your network
 * interfaces, that we could solve this problem without a code change.
 *
 * The configuration file should always be used, since it is the only way
 * to specify a default domain.  If you are running a server on your local
 * machine, you should say "nameserver 0.0.0.0" or "nameserver 127.0.0.1"
 * in the configuration file.
 *
 * Return 0 if completes successfully, -1 on error
 */
int
res_init()
{
	register FILE *fp;
	register char *cp, **pp, *net;
	register int n;
	char buf[MAXDNAME];
	int nserv = 0;    /* number of nameserver records read from file */
	int haveenv = 0;
	int havesearch = 0;
	int nsort = 0;
#ifdef SEARCH_LOCAL_DOMAINS
	int dots;
#endif

	_res.id = res_randomid();
	_res.nsaddr.sin_len = sizeof(struct sockaddr_in);
	_res.nsaddr.sin_family = AF_INET;
	_res.nsaddr.sin_port = htons(NAMESERVER_PORT);
#ifdef USELOOPBACK
	_res.nsaddr.sin_addr = inet_makeaddr(IN_LOOPBACKNET, 1);
#else
	_res.nsaddr.sin_addr.s_addr = INADDR_ANY;
#endif
	_res.nscount = 1;
	_res.ndots = 1;
	_res.pfcode = 0;

	/* Allow user to override the local domain definition */
	if ((cp = getenv("LOCALDOMAIN")) != NULL) {
		(void)strncpy(_res.defdname, cp, sizeof(_res.defdname) - 1);
		if ((cp = strpbrk(_res.defdname, " \t\n")) != NULL)
			*cp = '\0';
		haveenv++;

		/*
		 * Set search list to be blank-separated strings
		 * from rest of env value.  Permits users of LOCALDOMAIN
		 * to still have a search list, and anyone to set the
		 * one that they want to use as an individual (even more
		 * important now that the rfc1535 stuff restricts searches)
		 */
		cp = _res.defdname;
		pp = _res.dnsrch;
		*pp++ = cp;
		for (n = 0; *cp && pp < _res.dnsrch + MAXDNSRCH; cp++) {
			if (*cp == '\n')        /* silly backwards compat */
				break;
			else if (*cp == ' ' || *cp == '\t') {
				*cp = 0;
				n = 1;
			} else if (n) {
				*pp++ = cp;
				n = 0;
				havesearch = 1;
			}
		}
		/* null terminate last domain if there are excess */
		while (*cp != '\0' && *cp != ' ' && *cp != '\t' && *cp != '\n')
			cp++;
		*cp = '\0';
		*pp++ = 0;
	}

#define	MATCH(line, name) \
	(!strncmp(line, name, sizeof(name) - 1) && \
	(line[sizeof(name) - 1] == ' ' || \
	 line[sizeof(name) - 1] == '\t'))

	if ((fp = fopen(_PATH_RESCONF, "r")) != NULL) {
	    /* read the config file */
	    while (fgets(buf, sizeof(buf), fp) != NULL) {
		/* skip comments */
		if ((*buf == ';') || (*buf == '#'))
			continue;
		/* read default domain name */
		if (MATCH(buf, "domain")) {
		    if (haveenv)	/* skip if have from environ */
			    continue;
		    cp = buf + sizeof("domain") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    (void)strncpy(_res.defdname, cp,
				  sizeof(_res.defdname) - 1);
		    if ((cp = strpbrk(_res.defdname, " \t\n")) != NULL)
			    *cp = '\0';
		    havesearch = 0;
		    continue;
		}
		/* lookup types; deprecated in favour of nsswitch.conf(5) */
		if (!strncmp(buf, "lookup", sizeof("lookup") -1))
		    continue;
		/* set search list */
		if (MATCH(buf, "search")) {
		    if (haveenv)	/* skip if have from environ */
			    continue;
		    cp = buf + sizeof("search") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    (void)strncpy(_res.defdname, cp,
				  sizeof(_res.defdname) - 1);
		    if ((cp = strchr(_res.defdname, '\n')) != NULL)
			    *cp = '\0';
		    /*
		     * Set search list to be blank-separated strings
		     * on rest of line.
		     */
		    cp = _res.defdname;
		    pp = _res.dnsrch;
		    *pp++ = cp;
		    for (n = 0; *cp && pp < _res.dnsrch + MAXDNSRCH; cp++) {
			    if (*cp == ' ' || *cp == '\t') {
				    *cp = 0;
				    n = 1;
			    } else if (n) {
				    *pp++ = cp;
				    n = 0;
			    }
		    }
		    /* null terminate last domain if there are excess */
		    while (*cp != '\0' && *cp != ' ' && *cp != '\t')
			    cp++;
		    *cp = '\0';
		    *pp++ = 0;
		    havesearch = 1;
		    continue;
		}
		/* read nameservers to query */
		if (MATCH(buf, "nameserver") && nserv < MAXNS) {
		   struct in_addr a;

		    cp = buf + sizeof("nameserver") - 1;
		    while (*cp == ' ' || *cp == '\t')
			cp++;
		    if ((*cp != '\0') && (*cp != '\n') && inet_aton(cp, &a)) {
			_res.nsaddr_list[nserv].sin_len = sizeof(struct sockaddr_in);
			_res.nsaddr_list[nserv].sin_family = AF_INET;
			_res.nsaddr_list[nserv].sin_port =
				htons(NAMESERVER_PORT);
			_res.nsaddr_list[nserv].sin_addr = a;
			nserv++;
		    }
		    continue;
		}
		if (MATCH(buf, "sortlist")) {
		    struct in_addr a;

		    cp = buf + sizeof("sortlist") - 1;
		    while (nsort < MAXRESOLVSORT) {
			while (*cp == ' ' || *cp == '\t')
			    cp++;
			if (*cp == '\0' || *cp == '\n' || *cp == ';')
			    break;
			net = cp;
			while (*cp && !ISSORTMASK(*cp) && *cp != ';' &&
			       isascii(*cp) && !isspace(*cp))
				cp++;
			n = *cp;
			*cp = 0;
			if (inet_aton(net, &a)) {
			    _res.sort_list[nsort].addr = a;
			    if (ISSORTMASK(n)) {
				*cp++ = n;
				net = cp;
				while (*cp && *cp != ';' &&
					isascii(*cp) && !isspace(*cp))
				    cp++;
				n = *cp;
				*cp = 0;
				if (inet_aton(net, &a)) {
				    _res.sort_list[nsort].mask = a.s_addr;
				} else {
				    _res.sort_list[nsort].mask = 
					net_mask(_res.sort_list[nsort].addr);
				}
			    } else {
				_res.sort_list[nsort].mask = 
				    net_mask(_res.sort_list[nsort].addr);
			    }
			    nsort++;
			}
			*cp = n;
		    }
		    continue;
		}
		if (MATCH(buf, "options")) {
		    res_setoptions(buf + sizeof("options") - 1, "conf");
		    continue;
		}
	    }
	    if (nserv > 1) 
		_res.nscount = nserv;
	    _res.nsort = nsort;
	    (void) fclose(fp);
	}
	if (_res.defdname[0] == 0) {
		int rv;

		rv = gethostname(buf, sizeof(_res.defdname));
		_res.defdname[sizeof(_res.defdname) - 1] = '\0';
		if (rv == 0 && (cp = strchr(buf, '.')))
			(void)strncpy(_res.defdname, cp + 1, sizeof(_res.defdname) - 1);
	}

	/* find components of local domain that might be searched */
	if (havesearch == 0) {
		pp = _res.dnsrch;
		*pp++ = _res.defdname;
#ifndef SEARCH_LOCAL_DOMAINS
		*pp = NULL;
#else
		dots = 0;
		for (cp = _res.defdname, n = 0; *cp; cp++)
			dots += (*cp == '.');

		cp = _res.defdname;
		while (pp < _res.dnsrch + MAXDFLSRCH) {
			if (dots < LOCALDOMAINPARTS)
				break;
			cp = strchr(cp, '.') + 1;
			*pp++ = cp;
			dots--;
		}
		*pp++ = 0;
#ifdef DEBUG
		if (_res.options & RES_DEBUG) {
			printf(";; res_init()... default dnsrch list:\n");
			for (pp = _res.dnsrch; *pp; pp++)
				printf(";;\t%s\n", *pp);
			printf(";;\t..END..\n");
		}
#endif /* DEBUG */
#endif
	}

	if ((cp = getenv("RES_OPTIONS")) != NULL)
		res_setoptions(cp, "env");
	_res.options |= RES_INIT;
	return (0);
}

static void
res_setoptions(options, source)
	char *options, *source;
{
	char *cp = options;
	int i;

#ifdef DEBUG
	if (_res.options & RES_DEBUG)
		printf(";; res_setoptions(\"%s\", \"%s\")...\n",
		       options, source);
#endif
	while (*cp) {
		/* skip leading and inner runs of spaces */
		while (*cp == ' ' || *cp == '\t')
			cp++;
		/* search for and process individual options */
		if (!strncmp(cp, "ndots:", sizeof("ndots:")-1)) {
			i = atoi(cp + sizeof("ndots:") - 1);
			if (i <= RES_MAXNDOTS)
				_res.ndots = i;
			else
				_res.ndots = RES_MAXNDOTS;
#ifdef DEBUG
			if (_res.options & RES_DEBUG)
				printf(";;\tndots=%d\n", _res.ndots);
#endif
		} else if (!strncmp(cp, "debug", sizeof("debug")-1)) {
#ifdef DEBUG
			if (!(_res.options & RES_DEBUG)) {
				printf(";; res_setoptions(\"%s\", \"%s\")..\n",
				       options, source);
				_res.options |= RES_DEBUG;
			}
			printf(";;\tdebug\n");
#endif
		} else if (!strncmp(cp, "inet6", sizeof("inet6") - 1)) {
			_res.options |= RES_USE_INET6;
		} else {
			/* XXX - print a warning here? */
		}
		/* skip to next run of spaces */
		while (*cp && *cp != ' ' && *cp != '\t')
			cp++;
	}
}

/* XXX - should really support CIDR which means explicit masks always. */
static u_int32_t
net_mask(in)		/* XXX - should really use system's version of this */
	struct in_addr in;
{
	register u_int32_t i = ntohl(in.s_addr);

	if (IN_CLASSA(i))
		return (htonl(IN_CLASSA_NET));
	if (IN_CLASSB(i))
		return (htonl(IN_CLASSB_NET));
	return (htonl(IN_CLASSC_NET));
}

u_int
res_randomid()
{
	struct timeval now;

	gettimeofday(&now, NULL);
	return (0xffff & (now.tv_sec ^ now.tv_usec ^ getpid()));
}
