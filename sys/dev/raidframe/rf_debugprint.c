/*	$NetBSD: rf_debugprint.c,v 1.5.8.1 2002/08/29 05:22:49 gehenna Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Code to do debug printfs. Calls to rf_debug_printf cause the corresponding
 * information to be printed to a circular buffer rather than the screen.
 * The point is to try and minimize the timing variations induced by the
 * printfs, and to capture only the printf's immediately preceding a failure.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_debugprint.c,v 1.5.8.1 2002/08/29 05:22:49 gehenna Exp $");

#include <dev/raidframe/raidframevar.h>

#include "rf_threadstuff.h"
#include "rf_debugprint.h"
#include "rf_general.h"
#include "rf_options.h"

#include <sys/param.h>

void 
rf_debug_printf(s, a1, a2, a3, a4, a5, a6, a7, a8)
	char   *s;
	void   *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
{
	printf(s, a1, a2, a3, a4, a5, a6, a7, a8);
}
