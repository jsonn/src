/*-
 * Copyright (c) 1993, 1994
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
 */

#ifndef lint
static char sccsid[] = "@(#)sex_refresh.c	8.16 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "sex_screen.h"

#ifndef SYSV_CURSES
#define	A_NORMAL	1
#define	A_STANDOUT	2
#define	vidattr(attr)	Xvidattr(sp, attr)

static int	Xvidattr __P((SCR *, int));
#endif

/*
 * sex_refresh --
 *	In ex, just display any messages.
 */
int
sex_refresh(sp, ep)
	SCR *sp;
	EXF *ep;
{
	MSG *mp;

	/* Check for screen resize. */
	if (F_ISSET(sp, S_RESIZE)) {
		sp->rows = O_VAL(sp, O_LINES);
		sp->cols = O_VAL(sp, O_COLUMNS);
		F_CLR(sp, S_RESIZE);
	}

	/* Ring the bell. */
	if (F_ISSET(sp, S_BELLSCHED)) {
		sex_bell(sp);
		F_CLR(sp, S_BELLSCHED);
	}

	/* Display messages. */
	for (mp = sp->msgq.lh_first;
	    mp != NULL && !(F_ISSET(mp, M_EMPTY)); mp = mp->q.le_next) {
		if (F_ISSET(mp, M_INV_VIDEO) &&
		    vidattr(A_STANDOUT) == ERR && O_ISSET(sp, O_ERRORBELLS))
			(void)printf("\07");
		(void)printf("%.*s.\n", (int)mp->len, mp->mbuf);
		F_SET(mp, M_EMPTY);

		if (F_ISSET(mp, M_INV_VIDEO))
			vidattr(A_NORMAL);
		(void)fflush(stdout);
	}
	return (0);
}

#ifndef SYSV_CURSES
/*
 * Xvidattr --
 *	Set the video attributes to a value.
 *
 * XXX
 * Just enough to make the above code work when using non-System V
 * curses.
 */
static int
Xvidattr(sp, attr)
	SCR *sp;
	int attr;
{
	SEX_PRIVATE *sxp;

	sxp = SXP(sp);

	/* Check to see if standout isn't available. */
	if (sxp->SO == NULL)
		return (ERR);

	switch (attr) {
	case A_NORMAL:
		(void)tputs(SXP(sp)->SE, 1, vi_putchar);
		break;
	case A_STANDOUT:
		(void)tputs(SXP(sp)->SO, 1, vi_putchar);
		break;
	default:
		abort();
	}
	return (0);
}
#endif
