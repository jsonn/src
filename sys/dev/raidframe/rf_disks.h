/*	$NetBSD: rf_disks.h,v 1.11.10.1 2005/03/19 08:35:41 yamt Exp $	*/
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
 * rf_disks.h -- header file for code related to physical disks
 */

#ifndef _RF__RF_DISKS_H_
#define _RF__RF_DISKS_H_

#include <sys/types.h>

#include <dev/raidframe/raidframevar.h>
#include "rf_archs.h"
#include "rf_netbsd.h"

/* if a disk is in any of these states, it is inaccessible */
#define RF_DEAD_DISK(_dstat_) (((_dstat_) == rf_ds_spared) || \
	((_dstat_) == rf_ds_reconstructing) || ((_dstat_) == rf_ds_failed) || \
	((_dstat_) == rf_ds_dist_spared))

int rf_ConfigureDisks(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
		      RF_Config_t * cfgPtr);
int rf_ConfigureSpareDisks(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
			   RF_Config_t * cfgPtr);
int rf_ConfigureDisk(RF_Raid_t * raidPtr, char *buf, RF_RaidDisk_t * diskPtr,
		     RF_RowCol_t col);
int rf_AutoConfigureDisks(RF_Raid_t *raidPtr, RF_Config_t *cfgPtr,
			  RF_AutoConfig_t *auto_config);
int rf_CheckLabels( RF_Raid_t *, RF_Config_t *);
int rf_add_hot_spare(RF_Raid_t *raidPtr, RF_SingleComponent_t *sparePtr);
int rf_remove_hot_spare(RF_Raid_t *raidPtr, RF_SingleComponent_t *sparePtr);
int rf_delete_component(RF_Raid_t *raidPtr, RF_SingleComponent_t *component);
int rf_incorporate_hot_spare(RF_Raid_t *raidPtr,
			     RF_SingleComponent_t *component);

#endif				/* !_RF__RF_DISKS_H_ */
