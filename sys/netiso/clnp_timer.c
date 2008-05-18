/*	$NetBSD: clnp_timer.c,v 1.13.72.1 2008/05/18 12:35:40 yamt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)clnp_timer.c	8.1 (Berkeley) 6/10/93
 */

/***********************************************************
		Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: clnp_timer.c,v 1.13.72.1 2008/05/18 12:35:40 yamt Exp $");

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/route.h>

#include <netiso/iso.h>
#include <netiso/clnp.h>
#include <netiso/clnp_stat.h>
#include <netiso/argo_debug.h>

extern struct clnp_fragl *clnp_frags;

/*
 * FUNCTION:		clnp_freefrags
 *
 * PURPOSE:			Free the resources associated with a fragment
 *
 * RETURNS:			pointer to next fragment in list of fragments
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 *			TODO: send ER back to source
 */
struct clnp_fragl *
clnp_freefrags(
	struct clnp_fragl *cfh)	/* fragment header to delete */
{
	struct clnp_fragl *next = cfh->cfl_next;
	struct clnp_frag *cf;

	/* free any frags hanging around */
	cf = cfh->cfl_frags;
	while (cf != NULL) {
		struct clnp_frag *cf_next = cf->cfr_next;
		INCSTAT(cns_fragdropped);
		m_freem(cf->cfr_data);
		cf = cf_next;
	}

	/* free the copy of the header */
	INCSTAT(cns_fragdropped);
	m_freem(cfh->cfl_orighdr);

	if (clnp_frags == cfh) {
		clnp_frags = cfh->cfl_next;
	} else {
		struct clnp_fragl *scan;

		for (scan = clnp_frags; scan != NULL; scan = scan->cfl_next) {
			if (scan->cfl_next == cfh) {
				scan->cfl_next = cfh->cfl_next;
				break;
			}
		}
	}

	/* free the fragment header */
	FREE(cfh, M_FTABLE);

	return (next);
}

/*
 * FUNCTION:		clnp_slowtimo
 *
 * PURPOSE:			clnp timer processing; if the ttl expires on a
 *					packet on the reassembly queue, discard it.
 *
 * RETURNS:			none
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
clnp_slowtimo(void)
{
	struct clnp_fragl *cfh;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);
	cfh = clnp_frags;
	while (cfh != NULL) {
		if (--cfh->cfl_ttl == 0) {
			cfh = clnp_freefrags(cfh);
			INCSTAT(cns_fragtimeout);
		} else {
			cfh = cfh->cfl_next;
		}
	}
	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

/*
 * FUNCTION:		clnp_drain
 *
 * PURPOSE:			drain off all datagram fragments
 *
 * RETURNS:			none
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 *	TODO: should send back ER
 */
void
clnp_drain(void)
{
	struct clnp_fragl *cfh;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);
	cfh = clnp_frags;
	while (cfh != NULL)
		cfh = clnp_freefrags(cfh);
	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}
