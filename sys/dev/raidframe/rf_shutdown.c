/*	$NetBSD: rf_shutdown.c,v 1.6.8.1 2002/01/10 19:58:00 thorpej Exp $	*/
/*
 * rf_shutdown.c
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
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
 * Maintain lists of cleanup functions. Also, mechanisms for coordinating
 * thread startup and shutdown.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_shutdown.c,v 1.6.8.1 2002/01/10 19:58:00 thorpej Exp $");

#include <dev/raidframe/raidframevar.h>

#include "rf_archs.h"
#include "rf_threadstuff.h"
#include "rf_shutdown.h"
#include "rf_debugMem.h"
#include "rf_freelist.h"

static void 
rf_FreeShutdownEnt(RF_ShutdownList_t * ent)
{
	FREE(ent, M_RAIDFRAME);
}

int 
_rf_ShutdownCreate(
    RF_ShutdownList_t ** listp,
    void (*cleanup) (void *arg),
    void *arg,
    char *file,
    int line)
{
	RF_ShutdownList_t *ent;

	/*
         * Have to directly allocate memory here, since we start up before
         * and shutdown after RAIDframe internal allocation system.
         */
	/* 	ent = (RF_ShutdownList_t *) malloc(sizeof(RF_ShutdownList_t), 
		M_RAIDFRAME, M_WAITOK); */
	ent = (RF_ShutdownList_t *) malloc(sizeof(RF_ShutdownList_t), 
					   M_RAIDFRAME, M_NOWAIT);
	if (ent == NULL)
		return (ENOMEM);
	ent->cleanup = cleanup;
	ent->arg = arg;
	ent->file = file;
	ent->line = line;
	ent->next = *listp;
	*listp = ent;
	return (0);
}

int 
rf_ShutdownList(RF_ShutdownList_t ** list)
{
	RF_ShutdownList_t *r, *next;
	char   *file;
	int     line;

	for (r = *list; r; r = next) {
		next = r->next;
		file = r->file;
		line = r->line;

		if (rf_shutdownDebug) {
			printf("call shutdown, created %s:%d\n", file, line);
		}
		r->cleanup(r->arg);

		if (rf_shutdownDebug) {
			printf("completed shutdown, created %s:%d\n", file, line);
		}
		rf_FreeShutdownEnt(r);
	}
	*list = NULL;
	return (0);
}
