/*	$NetBSD: rf_netbsd.h,v 1.5.4.1 1999/06/21 01:18:58 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Greg Oster; Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RF__RF_NETBSDSTUFF_H_
#define _RF__RF_NETBSDSTUFF_H_

#ifdef _KERNEL
#include <sys/fcntl.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#endif /* _KERNEL */

/* The per-component label information that the user can set */
typedef struct RF_ComponentInfo_s {
	int row;              /* the row number of this component */
	int column;           /* the column number of this component */
	int serial_number;    /* a user-specified serial number for this
				 RAID set */
} RF_ComponentInfo_t;

/* The per-component label information */
typedef struct RF_ComponentLabel_s {
	int version;          /* The version of this label. */
	int serial_number;    /* a user-specified serial number for this
				 RAID set */
	int mod_counter;      /* modification counter.  Changed (usually
				 by incrementing) every time the label 
				 is changed */
	int row;              /* the row number of this component */
	int column;           /* the column number of this component */
	int num_rows;         /* number of rows in this RAID set */
	int num_columns;      /* number of columns in this RAID set */
	int clean;            /* 1 when clean, 0 when dirty */
	int status;           /* rf_ds_optimal, rf_ds_dist_spared, whatever. */
} RF_ComponentLabel_t;

typedef struct RF_SingleComponent_s {
	int row;
	int column;
	char component_name[50]; /* name of the component */
} RF_SingleComponent_t; 

#ifdef _KERNEL

/* XXX this is *not* the place for these... */
int rf_add_hot_spare(RF_Raid_t *raidPtr, RF_SingleComponent_t *sparePtr);
int rf_remove_hot_spare(RF_Raid_t *raidPtr, RF_SingleComponent_t *sparePtr);


struct raidcinfo {
	struct vnode *ci_vp;	/* component device's vnode */
	dev_t   ci_dev;		/* component device's dev_t */
	RF_ComponentLabel_t ci_label; /* components RAIDframe label */
#if 0
	size_t  ci_size;	/* size */
	char   *ci_path;	/* path to component */
	size_t  ci_pathlen;	/* length of component path */
#endif
};


#endif /* _KERNEL */
#endif /* _RF__RF_NETBSDSTUFF_H_ */
