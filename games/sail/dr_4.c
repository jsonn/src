/*	$NetBSD: dr_4.c,v 1.13.40.1 2009/05/13 19:18:05 jym Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)dr_4.c	8.2 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: dr_4.c,v 1.13.40.1 2009/05/13 19:18:05 jym Exp $");
#endif
#endif /* not lint */

#include <stdlib.h>
#include "extern.h"

void
ungrap(struct ship *from, struct ship *to)
{
	int k;
	char friend;

	if ((k = grappled2(from, to)) == 0)
		return;
	friend = capship(from)->nationality == capship(to)->nationality;
	while (--k >= 0) {
		if (friend || dieroll() < 3) {
			cleangrapple(from, to, 0);
			makesignal(from, "ungrappling $$", to);
		}
	}
}

void
grap(struct ship *from, struct ship *to)
{
	if (capship(from)->nationality != capship(to)->nationality &&
	    dieroll() > 2)
		return;
	send_grap(from, to->file->index);
	send_grap(to, from->file->index);
	makesignal(from, "grappled with $$", to);
}
