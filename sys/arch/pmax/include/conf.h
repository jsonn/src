/*	$NetBSD: conf.h,v 1.2.32.1 2000/11/20 20:20:26 bouyer Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * This file contributed by Jonathan Stone.
 */

#ifndef _PMAX_CONF_H_
#define _PMAX_CONF_H_

#define mmread mmrw
#define mmwrite mmrw
cdev_decl(mm);

cdev_decl(scc);		/* pmax (also alpha m-d z8530 SCC */
cdev_decl(dc);		/* dc7085 dz11-on-a-chip */

bdev_decl(rz);		/* antique 4.4bsd/pmax SCSI disk */
cdev_decl(rz);

bdev_decl(tz);		/* antique 4.4bsd/pmax SCSI tape driver */
cdev_decl(tz);

cdev_decl(dtop);	/* Personal Decstation (MAXINE) desktop bus */
cdev_decl(fb);		/* generic framebuffer pseudo-device */
cdev_decl(rcons);	/* framebuffer-based raster console pseudo-device */
cdev_decl(px);		/* PixelStamp framebuffers */

#endif	/* !_PMAX_CONF_H_ */
