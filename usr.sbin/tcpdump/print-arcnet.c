/*	$NetBSD: print-arcnet.c,v 1.1.4.1 1999/12/27 18:38:12 wrstuden Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <sys/cdefs.h>
#ifndef lint
#if 0
static const char rcsid[] =
    "@(#) Header: print-ether.c,v 1.44 97/05/26 17:18:13 leres Exp  (LBL)";
#else
__RCSID("$NetBSD: print-arcnet.c,v 1.1.4.1 1999/12/27 18:38:12 wrstuden Exp $");
#endif
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#ifdef __NetBSD__
#include <net/if_arc.h>
#else
Error: ARCnet support only available on NetBSD
#endif
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <stdio.h>
#include <pcap.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"

const u_char *packetp;
const u_char *snapend;

int arcnet_encap_print(u_char arctype, const u_char *p,
    u_int length, u_int caplen);

struct arctype_map {
	const int arctype;
	char * const name;
} arctypemap[] = {
	{ ARCTYPE_IP_OLD,	"oldip" },
	{ ARCTYPE_ARP_OLD,	"oldarp" },
	{ ARCTYPE_IP,		"ip" },
	{ ARCTYPE_ARP,		"arp" },
	{ ARCTYPE_REVARP,	"rarp" },
	{ ARCTYPE_ATALK,	"atalk" },
	{ ARCTYPE_BANIAN,	"banyan" },
	{ ARCTYPE_IPX,		"ipx" },
	{ ARCTYPE_INET6,	"ipv6" },
	{ ARCTYPE_DIAGNOSE,	"diag" },
	{ 0, 0 }
};

static inline void
arcnet_print(const u_char *bp, u_int length, int phds, int flag, u_int seqid)
{
	const struct arc_header *ap;
	struct arctype_map *atmp;
	char *arctypename;
	char typebuf[3];


	ap = (const struct arc_header *)bp;


	if (qflag) {
		(void)printf("%02x %02x %d: ",
			     ap->arc_shost,
			     ap->arc_dhost,
			     length);
		return;
	}

	for (arctypename = NULL, atmp = arctypemap; atmp->arctype; atmp++) {
		if (atmp->arctype == ap->arc_type) {
			arctypename = atmp->name;
			break;
		}
	}
	if (!arctypename) {
		arctypename = typebuf;
		(void)snprintf(typebuf, sizeof(typebuf), "%02x", ap->arc_type);
	}

	if (!phds) {
		(void)printf("%02x %02x %s %d: ",
			     ap->arc_shost, ap->arc_dhost, arctypename,
			     length);
			     return;
	}

	if (flag == 0) {
		(void)printf("%02x %02x %s seqid %04x %d: ",
			ap->arc_shost, ap->arc_dhost, arctypename, seqid,
			length);
			return;
	}

	if (flag & 1)
		(void)printf("%02x %02x %s seqid %04x "
			"(first of %d fragments) %d: ",
			ap->arc_shost, ap->arc_dhost, arctypename, seqid,
			(flag + 3) / 2, length);
	else
		(void)printf("%02x %02x %s seqid %04x "
			"(fragment %d) %d: ",
			ap->arc_shost, ap->arc_dhost, arctypename, seqid,
			flag/2 + 1, length);
}

/*
 * This is the top level routine of the printer.  'p' is the points
 * to the ether header of the packet, 'tvp' is the timestamp,
 * 'length' is the length of the packet off the wire, and 'caplen'
 * is the number of bytes actually captured.
 */
void
arcnet_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	struct arc_header *ap;

	int phds, flag, archdrlen;
	u_int seqid;
	u_char arc_type;

	ts_print(&h->ts);

	if (caplen < ARC_HDRLEN) {
		printf("[|arcnet]");
		goto out;
	}

	ap = (struct arc_header *)p;
	arc_type = ap->arc_type;

	switch(arc_type) {
	default:
		phds = 1;
		break;
	case ARCTYPE_IP_OLD:
	case ARCTYPE_ARP_OLD:
	case ARCTYPE_DIAGNOSE:
		phds = 0;
		archdrlen = ARC_HDRLEN;
		break;
	}

	if (phds) {
		if (caplen < ARC_HDRNEWLEN) {
			arcnet_print(p, length, 0, 0, 0);
			printf("[|phds]");
			goto out;
		}

		if (ap->arc_flag == 0xff) {
			if (caplen < ARC_HDRNEWLEN_EXC) {
				arcnet_print(p, length, 0, 0, 0);
				printf("[|phds extended]");
				goto out;
			}
			flag = ap->arc_flag2;
			seqid = ap->arc_seqid2;
			archdrlen = ARC_HDRNEWLEN_EXC;
		} else {
			flag = ap->arc_flag;
			seqid = ap->arc_seqid;
			archdrlen = ARC_HDRNEWLEN;
		}
	}


	if (eflag)
		arcnet_print(p, length, phds, flag, seqid);

	/*
	 * Some printers want to get back at the ethernet addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	length -= archdrlen;
	caplen -= archdrlen;
	p += archdrlen;

	if (phds && flag && (flag & 1) == 0)
		goto out2;

	if (!arcnet_encap_print(arc_type, p, length, caplen)) {
		default_print(p, caplen);
		goto out;
	}

 out2:
	if (xflag)
		default_print(p, caplen);

 out:
	putchar('\n');
}

/*
 * Prints the packet encapsulated in an ARCnet data field,
 * given the ARCnet system code.
 *
 * Returns non-zero if it can do so, zero if the system code is unknown.
 */


int
arcnet_encap_print(u_char arctype, const u_char *p,
    u_int length, u_int caplen)
{
	switch (arctype) {

	case ARCTYPE_IP_OLD:
	case ARCTYPE_IP:
		ip_print(p, length);
		return (1);

#ifdef INET6
	case ARCTYPE_INET6:
		ip6_print(p, length);
		return (1);
#endif /*INET6*/

	case ARCTYPE_ARP_OLD:
	case ARCTYPE_ARP:
	case ARCTYPE_REVARP:
		arp_print(p, length, caplen);
		return (1);

	case ARCTYPE_ATALK:	/* XXX was this ever used? */
		if (vflag)
			fputs("et1 ", stdout);
		atalk_print(p, length);
		return (1);

	default:
		return (0);
	}
}
