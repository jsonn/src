/*	$NetBSD: ex_undo.c,v 1.8.6.1 2005/05/06 14:42:18 riz Exp $	*/

/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/cdefs.h>
#ifndef lint
#if 0
static const char sccsid[] = "@(#)ex_undo.c	10.6 (Berkeley) 3/6/96";
#else
__RCSID("$NetBSD: ex_undo.c,v 1.8.6.1 2005/05/06 14:42:18 riz Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/common.h"

/*
 * ex_undo -- u
 *	Undo the last change.
 *
 * PUBLIC: int ex_undo __P((SCR *, EXCMD *));
 */
int
ex_undo(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EXF *ep;
	MARK m;

	/*
	 * !!!
	 * Historic undo always set the previous context mark.
	 */
	m.lno = sp->lno;
	m.cno = sp->cno;
	if (mark_set(sp, ABSMARK1, &m, 1))
		return (1);

	/*
	 * !!!
	 * Multiple undo isn't available in ex, as there's no '.' command.
	 * Whether 'u' is undo or redo is toggled each time, unless there
	 * was a change since the last undo, in which case it's an undo.
	 */
	ep = sp->ep;
	if (!F_ISSET(ep, F_UNDO)) {
		F_SET(ep, F_UNDO);
		ep->lundo = FORWARD;
	}
	switch (ep->lundo) {
	case BACKWARD:
		if (log_forward(sp, &m))
			return (1);
		ep->lundo = FORWARD;
		break;
	case FORWARD:
		if (log_backward(sp, &m))
			return (1);
		ep->lundo = BACKWARD;
		break;
	case NOTSET:
		abort();
	}
	sp->lno = m.lno;
	sp->cno = m.cno;
	return (0);
}
