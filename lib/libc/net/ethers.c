/*	$NetBSD: ethers.c,v 1.10.2.2 1997/11/11 21:46:01 mjacob Exp $	*/

/* 
 * ethers(3N) a la Sun.
 *
 * Written by Roland McGrath <roland@frob.com> 10/14/93.
 * Public domain.
 */

#include "namespace.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_ether.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <paths.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef YP
#include <rpcsvc/ypclnt.h>
#endif

#ifdef __weak_alias
__weak_alias(ether_aton,_ether_aton);
__weak_alias(ether_hostton,_ether_hostton);
__weak_alias(ether_line,_ether_line);
__weak_alias(ether_ntoa,_ether_ntoa);
__weak_alias(ether_ntohost,_ether_ntohost);
#endif

#ifndef _PATH_ETHERS
#define _PATH_ETHERS "/etc/ethers"
#endif

char *
ether_ntoa(e)
	struct ether_addr *e;
{
	static char a[] = "xx:xx:xx:xx:xx:xx";

	snprintf(a, sizeof a, "%02x:%02x:%02x:%02x:%02x:%02x",
	    e->ether_addr_octet[0], e->ether_addr_octet[1],
	    e->ether_addr_octet[2], e->ether_addr_octet[3],
	    e->ether_addr_octet[4], e->ether_addr_octet[5]);
	return a;
}

struct ether_addr *
ether_aton(s)
	const char *s;
{
	static struct ether_addr n;
	u_int i[6];

	if (sscanf(s, " %x:%x:%x:%x:%x:%x ", &i[0], &i[1],
	    &i[2], &i[3], &i[4], &i[5]) == 6) {
		n.ether_addr_octet[0] = (u_char)i[0];
		n.ether_addr_octet[1] = (u_char)i[1];
		n.ether_addr_octet[2] = (u_char)i[2];
		n.ether_addr_octet[3] = (u_char)i[3];
		n.ether_addr_octet[4] = (u_char)i[4];
		n.ether_addr_octet[5] = (u_char)i[5];
		return &n;
	}
	return NULL;
}

int
ether_ntohost(hostname, e)
	char *hostname;
	struct ether_addr *e;
{
	FILE *f; 
	char *p;
	size_t len;
	struct ether_addr try;

#ifdef YP
	char trybuf[sizeof "xx:xx:xx:xx:xx:xx"];
	int trylen;

	trylen = snprintf(trybuf, sizeof trybuf, "%x:%x:%x:%x:%x:%x", 
	    e->ether_addr_octet[0], e->ether_addr_octet[1],
	    e->ether_addr_octet[2], e->ether_addr_octet[3],
	    e->ether_addr_octet[4], e->ether_addr_octet[5]);
#endif

	f = fopen(_PATH_ETHERS, "r");
	if (f == NULL)
		return -1;
	while ((p = fgetln(f, &len)) != NULL) {
		if (p[len - 1] != '\n')
			continue;		/* skip lines w/o \n */
		p[--len] = '\0';
#ifdef YP
		/* A + in the file means try YP now.  */
		if (len == 1 && *p == '+') {
			char *ypbuf, *ypdom;
			int ypbuflen;

			if (yp_get_default_domain(&ypdom))
				continue;
			if (yp_match(ypdom, "ethers.byaddr", trybuf,
			    trylen, &ypbuf, &ypbuflen))
				continue;
			if (ether_line(ypbuf, &try, hostname) == 0) {
				free(ypbuf);
				(void)fclose(f);
				return 0;
			}
			free(ypbuf);
			continue;
		}
#endif
		if (ether_line(p, &try, hostname) == 0 &&
		    memcmp((char *)&try, (char *)e, sizeof try) == 0) {
			(void)fclose(f);
			return 0;
		}     
	}
	(void)fclose(f);
	errno = ENOENT;
	return -1;
}

int
ether_hostton(hostname, e)
	const char *hostname;
	struct ether_addr *e;
{
	FILE *f;
	char *p;
	size_t len;
	char try[MAXHOSTNAMELEN + 1];
#ifdef YP
	int hostlen = strlen(hostname);
#endif

	f = fopen(_PATH_ETHERS, "r");
	if (f==NULL)
		return -1;

	while ((p = fgetln(f, &len)) != NULL) {
		if (p[len - 1] != '\n')
			continue;		/* skip lines w/o \n */
		p[--len] = '\0';
#ifdef YP
		/* A + in the file means try YP now.  */
		if (len == 1 && *p == '+') {
			char *ypbuf, *ypdom;
			int ypbuflen;

			if (yp_get_default_domain(&ypdom))
				continue;
			if (yp_match(ypdom, "ethers.byname", hostname, hostlen,
			    &ypbuf, &ypbuflen))
				continue;
			if (ether_line(ypbuf, e, try) == 0) {
				free(ypbuf);
				(void)fclose(f);
				return 0;
			}
			free(ypbuf);
			continue;
		}
#endif
		if (ether_line(p, e, try) == 0 && strcmp(hostname, try) == 0) {
			(void)fclose(f);
			return 0;
		}
	}
	(void)fclose(f);
	errno = ENOENT;
	return -1;
}

int
ether_line(l, e, hostname)
	const char *l;
	struct ether_addr *e;
	char *hostname;
{
	u_int i[6];
	static char buf[sizeof " %x:%x:%x:%x:%x:%x %s\\n" + 21];
		/* XXX: 21 == strlen (ASCII representation of 2^64) */

	if (! buf[0])
		snprintf(buf, sizeof buf, " %%x:%%x:%%x:%%x:%%x:%%x %%%ds\\n",
		    MAXHOSTNAMELEN);
	if (sscanf(l, buf,
	    &i[0], &i[1], &i[2], &i[3], &i[4], &i[5], hostname) == 7) {
		e->ether_addr_octet[0] = (u_char)i[0];
		e->ether_addr_octet[1] = (u_char)i[1];
		e->ether_addr_octet[2] = (u_char)i[2];
		e->ether_addr_octet[3] = (u_char)i[3];
		e->ether_addr_octet[4] = (u_char)i[4];
		e->ether_addr_octet[5] = (u_char)i[5];
		return 0;
	}
	errno = EINVAL;
	return -1;
}
