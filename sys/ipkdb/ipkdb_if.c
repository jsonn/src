/*	$NetBSD: ipkdb_if.c,v 1.10.8.1 2002/01/10 19:59:32 thorpej Exp $	*/

/*
 * Copyright (C) 1993-2000 Wolfgang Solfrank.
 * Copyright (C) 1993-2000 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ipkdb_if.c,v 1.10.8.1 2002/01/10 19:59:32 thorpej Exp $");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_ether.h>

#include <netinet/in.h>

#include <ipkdb/ipkdb.h>
#include <machine/ipkdb.h>

/*
 * Initialize IPKDB Interface handling
 */
int
ipkdbifinit(kip)
	struct ipkdb_if *kip;
{
	u_char *cp;

	/* defaults: */
	kip->mtu = ETHERMTU;
	for (cp = kip->hisenetaddr; cp < kip->hisenetaddr + sizeof kip->hisenetaddr;)
		*cp++ = -1;
	for (cp = kip->hisinetaddr; cp < kip->hisinetaddr + sizeof kip->hisinetaddr;)
		*cp++ = -1;
	return ipkdbif_init(kip);
}
