/*	$NetBSD: print-ripng.c,v 1.3 1999/09/04 03:36:42 itojun Exp $	*/

/*
 * Copyright (c) 1989, 1990, 1991, 1993, 1994
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
#if 0
static const char rcsid[] =
    "@(#) /master/usr.sbin/tcpdump/tcpdump/print-rip.c,v 2.1 1995/02/03 18:15:05 polk Exp (LBL)";
#else
#include <sys/cdefs.h>
__RCSID("$NetBSD: print-ripng.c,v 1.3 1999/09/04 03:36:42 itojun Exp $");
#endif
#endif

#ifdef INET6

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <errno.h>
#include <stdio.h>

#include <netinet/ip6.h>

#include "route6d.h"
#include "interface.h"
#include "addrtoname.h"

static void
rip6_entry_print(register const struct netinfo6 *ni, int metric)
{
	printf(" %s/%d", ip6addr_string(&ni->rip6_dest), ni->rip6_plen);
	if (ni->rip6_tag)
		printf(" [%d]", ntohs(ni->rip6_tag));
	if (metric)
		printf(" (%d)", ni->rip6_metric);
}

void
ripng_print(const u_char *dat, int length)
{
	register const struct rip6 *rp = (struct rip6 *)dat;
	register const struct netinfo6 *ni;
	register int amt = snapend - dat;
	register int i = min(length, amt) -
			 (sizeof(struct rip6) - sizeof(struct netinfo6));
	int j;
	int trunc;

	if (i < 0)
		return;

	switch (rp->rip6_cmd) {

	case RIP6_REQUEST:
		j = length / sizeof(*ni);
		if (j == 1
		    &&  rp->rip6_nets->rip6_metric == HOPCNT_INFINITY6
		    &&  IN6_IS_ADDR_ANY(&rp->rip6_nets->rip6_dest)) {
			printf(" ripng-req dump");
			break;
		}
		if (j * sizeof(*ni) != length - 4)
			printf(" ripng-req %d[%d]:", j, length);
		else
			printf(" ripng-req %d:", j);
		trunc = ((i / sizeof(*ni)) * sizeof(*ni) != i);
		for (ni = rp->rip6_nets; (i -= sizeof(*ni)) >= 0; ++ni)
			rip6_entry_print(ni, 0);
		break;
	case RIP6_RESPONSE:
		j = length / sizeof(*ni);
		if (j * sizeof(*ni) != length - 4)
			printf(" ripng-resp %d[%d]:", j, length);
		else
			printf(" ripng-resp %d:", j);
		trunc = ((i / sizeof(*ni)) * sizeof(*ni) != i);
		for (ni = rp->rip6_nets; (i -= sizeof(*ni)) >= 0; ++ni)
			rip6_entry_print(ni, ni->rip6_metric);
		if (trunc)
			printf("[|rip]");
		break;
	default:
		printf(" ripng-%d ?? %d", rp->rip6_cmd, length);
		break;
	}
	if (rp->rip6_vers != RIP6_VERSION)
		printf(" [vers %d]", rp->rip6_vers);
}
#endif /* INET6 */
