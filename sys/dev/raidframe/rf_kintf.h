/*	$NetBSD: rf_kintf.h,v 1.4 1999/03/09 03:52:41 oster Exp $	*/
/*
 * rf_kintf.h
 *
 * RAIDframe exported kernel interface
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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

#ifndef _RF__RF_KINTF_H_
#define _RF__RF_KINTF_H_

#include "rf_types.h"

void    rf_ReconKernelThread(void);
int     rf_GetSpareTableFromDaemon(RF_SparetWait_t * req);

int rf_DoAccessKernel(RF_Raid_t * raidPtr, struct buf * bp,
    RF_RaidAccessFlags_t flags, void (*cbFunc) (struct buf *), void *cbArg);
int     rf_DispatchKernelIO(RF_DiskQueue_t * queue, RF_DiskQueueData_t * req);

#endif				/* _RF__RF_KINTF_H_ */
