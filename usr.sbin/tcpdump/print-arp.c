/*	$NetBSD: print-arp.c,v 1.2.4.1 1997/03/11 16:29:21 is Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994
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

#ifndef lint
static char rcsid[] =
    "@(#) Header: print-arp.c,v 1.28 94/06/14 20:17:36 leres Exp (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#ifdef __NetBSD__
#include <net/if_arp.h>
#include <net/if_ether.h>
#include <netinet/if_inarp.h>
#else
#include <netinet/if_ether.h>
#endif

#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"

static u_char ezero[6];

void
arp_print(register const u_char *bp, int length, int caplen)
{
	register const APTYPE *ap;
	register const struct ether_header *eh;
	const u_char *p;
	int pro, hrd, op;

	ap = (APTYPE *)bp;
	if ((u_char *)(ap + 1) > snapend) {
		printf("[|arp]");
		return;
	}
	if (length < sizeof(struct arphdr)) {
		(void)printf("truncated-arp");
		default_print((u_char *)ap, length);
		return;
	}
	/*
	 * Don't assume alignment.
	 */
	p = (u_char*)&PRO(ap);
	pro = (p[0] << 8) | p[1];
	p = (u_char*)&HRD(ap);
	hrd = (p[0] << 8) | p[1];
	p = (u_char*)&OP(ap);
	op = (p[0] << 8) | p[1];

	if ((pro != ETHERTYPE_IP && pro != ETHERTYPE_TRAIL)
#ifndef __NetBSD__
	    || ap->arp_hln != sizeof(SHA(ap))
	    || ap->arp_pln != sizeof(SPA(ap))
#endif
	    ) {
		(void)printf("arp-#%d for proto #%d (%d) hardware #%d (%d)",
				op, pro, PLN(ap), hrd, HLN(ap));
		return;
	}
	if (pro == ETHERTYPE_TRAIL)
		(void)printf("trailer-");
	eh = (struct ether_header *)packetp;
	switch (op) {

	case ARPOP_REQUEST:
		(void)printf("arp who-has %s", ipaddr_string(TPA(ap)));
		if (bcmp((char *)ezero, (char *)THA(ap), HLN(ap)) != 0)
			(void)printf(" (%s)",
			    linkaddr_string(THA(ap), HLN(ap)));
		(void)printf(" tell %s", ipaddr_string(SPA(ap)));

#ifndef __NetBSD__
		if (bcmp((char *)ESRC(eh), (char *)SHA(ap), 6) != 0)
			(void)printf(" (%s)", etheraddr_string(SHA(ap)));
#endif
		break;

	case ARPOP_REPLY:
		(void)printf("arp reply %s", ipaddr_string(SPA(ap)));

#ifndef __NetBSD__
		if (bcmp((char *)ESRC(eh), (char *)SHA(ap), 6) != 0)
			(void)printf(" (%s)", etheraddr_string(SHA(ap)));
#endif

		(void)printf(" is-at %s", linkaddr_string(SHA(ap), HLN(ap)));

#ifndef __NetBSD__
		if (bcmp((char *)EDST(eh), (char *)THA(ap), 6) != 0)
			(void)printf(" (%s)", etheraddr_string(THA(ap)));
#endif
		break;

	case REVARP_REQUEST:
		(void)printf("rarp who-is %s tell %s",
			linkaddr_string(THA(ap), HLN(ap)),
			linkaddr_string(SHA(ap), HLN(ap)));
		break;

	case REVARP_REPLY:
		(void)printf("rarp reply %s at %s",
			linkaddr_string(THA(ap), HLN(ap)),
			ipaddr_string(TPA(ap)));
		break;

	default:
		(void)printf("arp-#%d", op);
		default_print((u_char *)ap, caplen);
		return;
	}
	if (hrd != ARPHRD_ETHER)
		printf(" hardware #%d", HRD(ap));
}
