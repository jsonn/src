/*	$NetBSD: os.h,v 1.3.2.1 1997/03/11 16:29:20 is Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1993, 1994
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
 *
 * @(#) Header: os-bsd.h,v 1.18 94/06/14 20:15:17 leres Exp (LBL)
 */

#include <sys/param.h>

#ifndef BSD
#define BSD
#endif

#ifdef __NetBSD__	/* actually NetBSD 1.2D and later */
#define APTYPE  struct arphdr
#define HRD(ap)	((ap)->ar_hrd)
#define HLN(ap)	((ap)->ar_hln)
#define PLN(ap)	((ap)->ar_pln)
#define OP(ap)	((ap)->ar_op)
#define PRO(ap)	((ap)->ar_pro)
#define SHA(ap) (ar_sha(ap))
#define SPA(ap) (ar_spa(ap))
#define THA(ap) (ar_tha(ap))
#define TPA(ap) (ar_tpa(ap))
#else
#define APTYPE  struct ether_arp
#define HRD(ap)	((ap)->arp_hrd)
#define HLN(ap)	((ap)->arp_hln)
#define PLN(ap)	((ap)->arp_pln)
#define OP(ap)	((ap)->arp_op)
#define PRO(ap)	((ap)->arp_pro)
#define SHA(ap) ((ap)->arp_sha)
#define SPA(ap) ((ap)->arp_spa)
#define THA(ap) ((ap)->arp_tha)
#define TPA(ap) ((ap)->arp_tpa)
#endif

#define EDST(ep) ((ep)->ether_dhost)
#define ESRC(ep) ((ep)->ether_shost)

#ifndef ETHERTYPE_REVARP
#define ETHERTYPE_REVARP 0x8035
#endif

#ifndef	IPPROTO_ND
/* From <netinet/in.h> on a Sun somewhere. */
#define	IPPROTO_ND	77
#endif

#ifndef REVARP_REQUEST
#define REVARP_REQUEST 3
#endif
#ifndef REVARP_REPLY
#define REVARP_REPLY 4
#endif

/* newish RIP commands */
#ifndef	RIPCMD_POLL
#define	RIPCMD_POLL 5
#endif
#ifndef	RIPCMD_POLLENTRY
#define	RIPCMD_POLLENTRY 6
#endif

typedef int64_t         int64;
typedef u_int64_t       u_int64;

#define   INT64_FORMAT   "%qd"
#define U_INT64_FORMAT   "%qu"
#define HEX_INT64_FORMAT "%qx"
