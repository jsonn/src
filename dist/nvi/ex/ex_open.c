/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "$Id: ex_open.c,v 1.1.1.1.2.2 2008/05/18 12:29:27 yamt Exp $ (Berkeley) $Date: 2008/05/18 12:29:27 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"

/*
 * ex_open -- :[line] o[pen] [/pattern/] [flags]
 *
 *	Switch to single line "open" mode.
 *
 * PUBLIC: int ex_open __P((SCR *, EXCMD *));
 */
int
ex_open(SCR *sp, EXCMD *cmdp)
{
	/* If open option off, disallow open command. */
	if (!O_ISSET(sp, O_OPEN)) {
		msgq(sp, M_ERR,
	    "140|The open command requires that the open option be set");
		return (1);
	}

	msgq(sp, M_ERR, "141|The open command is not yet implemented");
	return (1);
}
