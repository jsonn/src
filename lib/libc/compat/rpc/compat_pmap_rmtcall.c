/*	$NetBSD: compat_pmap_rmtcall.c,v 1.1.2.1 2008/11/08 21:49:35 christos Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_pmap_rmtcall.c,v 1.1.2.1 2008/11/08 21:49:35 christos Exp $");
#endif /* LIBC_SCCS and not lint */


#define __LIBC12_SOURCE__

#include "namespace.h"
#include <sys/types.h>
#include <sys/time.h>
#include <compat/sys/time.h>
#include <rpc/rpc.h>
#include <compat/include/rpc/pmap_clnt.h>

__warn_references(pmap_rmtcall,
    "warning: reference to compatibility pmap_rmtcall(); include <rpc/pmap_clnt.h> to generate correct reference")

enum clnt_stat
pmap_rmtcall(struct sockaddr_in *addr, u_long prognum, u_long versnum,
    u_long procnum, xdrproc_t inproc, char *in, xdrproc_t outproc,
    char *out, struct timeval50 tout50, u_long *portp)
{
	struct timeval tout;
	timeval50_to_timeval(&tout50, &tout);
	return __pmap_rmtcall50(addr, prognum, versnum, procnum, inproc, in,
	    outproc, out, tout, portp);
}
