/*	$NetBSD: ex_delete.c,v 1.9.4.1 2005/02/12 12:24:13 aymeric Exp $	*/

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
static const char sccsid[] = "@(#)ex_delete.c	10.9 (Berkeley) 10/23/96";
#else
__RCSID("$NetBSD: ex_delete.c,v 1.9.4.1 2005/02/12 12:24:13 aymeric Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"

/*
 * ex_delete: [line [,line]] d[elete] [buffer] [count] [flags]
 *
 *	Delete lines from the file.
 *
 * PUBLIC: int ex_delete __P((SCR *, EXCMD *));
 */
int
ex_delete(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	recno_t lno;

	NEEDFILE(sp, cmdp);

	/*
	 * !!!
	 * Historically, lines deleted in ex were not placed in the numeric
	 * buffers.  We follow historic practice so that we don't overwrite
	 * vi buffers accidentally.
	 */
	if (cut(sp,
	    FL_ISSET(cmdp->iflags, E_C_BUFFER) ? &cmdp->buffer : NULL,
	    &cmdp->addr1, &cmdp->addr2, CUT_LINEMODE))
		return (1);

	/* Delete the lines. */
	if (del(sp, &cmdp->addr1, &cmdp->addr2, 1))
		return (1);

	/* Set the cursor to the line after the last line deleted. */
	sp->lno = cmdp->addr1.lno;

	/* Or the last line in the file if deleted to the end of the file. */
	if (db_last(sp, &lno))
		return (1);
	if (sp->lno > lno)
		sp->lno = lno;
	return (0);
}
