/*	$NetBSD: coda_vnops.h,v 1.4 1998/09/15 02:03:00 rvb Exp $	*/

/*
 * 
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 * 
 *           Copyright (c) 1987-1998 Carnegie Mellon University
 *                          All Rights Reserved
 * 
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 * 
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 * 
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 * 
 * 	@(#) coda/coda_vnops.h,v 1.1.1.1 1998/08/29 21:26:46 rvb Exp $ 
 */

/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  
 */

/*
 * HISTORY
 * $Log: coda_vnops.h,v $
 * Revision 1.4  1998/09/15 02:03:00  rvb
 * Final piece of rename cfs->coda
 *
 * Revision 1.3  1998/09/12 15:05:49  rvb
 * Change cfs/CFS in symbols, strings and constants to coda/CODA
 * to avoid fs conflicts.
 *
 * Revision 1.2  1998/09/08 17:12:48  rvb
 * Pass2 complete
 *
 * Revision 1.1.1.1  1998/08/29 21:26:46  rvb
 * Very Preliminary Coda
 *
 * Revision 1.7  1998/08/28 18:12:24  rvb
 * Now it also works on FreeBSD -current.  This code will be
 * committed to the FreeBSD -current and NetBSD -current
 * trees.  It will then be tailored to the particular platform
 * by flushing conditional code.
 *
 * Revision 1.6  1998/08/18 17:05:22  rvb
 * Don't use __RCSID now
 *
 * Revision 1.5  1998/08/18 16:31:47  rvb
 * Sync the code for NetBSD -current; test on 1.3 later
 *
 * Revision 1.4  98/01/23  11:53:49  rvb
 * Bring RVB_CODA1_1 to HEAD
 * 
 * Revision 1.3.2.3  98/01/23  11:21:13  rvb
 * Sync with 2.2.5
 * 
 * Revision 1.3.2.2  97/12/16  12:40:20  rvb
 * Sync with 1.3
 * 
 * Revision 1.3.2.1  97/12/10  14:08:34  rvb
 * Fix O_ flags; check result in coda_call
 * 
 * Revision 1.3  97/12/05  10:39:25  rvb
 * Read CHANGES
 * 
 * Revision 1.2.34.2  97/11/20  11:46:54  rvb
 * Capture current coda_venus
 * 
 * Revision 1.2.34.1  97/11/13  22:03:04  rvb
 * pass2 cfs_NetBSD.h mt
 * 
 * Revision 1.2  96/01/02  16:57:14  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 * 
 * Revision 1.1.2.1  1995/12/20 01:57:40  bnoble
 * Added Coda-specific files
 *
 */

/* NetBSD interfaces to the vnodeops */
int coda_open      __P((void *));
int coda_close     __P((void *));
int coda_read      __P((void *));
int coda_write     __P((void *));
int coda_ioctl     __P((void *));
/* 1.3 int coda_select    __P((void *));*/
int coda_getattr   __P((void *));
int coda_setattr   __P((void *));
int coda_access    __P((void *));
int coda_abortop   __P((void *));
int coda_readlink  __P((void *));
int coda_fsync     __P((void *));
int coda_inactive  __P((void *));
int coda_lookup    __P((void *));
int coda_create    __P((void *));
int coda_remove    __P((void *));
int coda_link      __P((void *));
int coda_rename    __P((void *));
int coda_mkdir     __P((void *));
int coda_rmdir     __P((void *));
int coda_symlink   __P((void *));
int coda_readdir   __P((void *));
int coda_bmap      __P((void *));
int coda_strategy  __P((void *));
int coda_reclaim   __P((void *));
int coda_lock      __P((void *));
int coda_unlock    __P((void *));
int coda_islocked  __P((void *));
int coda_vop_error   __P((void *));
int coda_vop_nop     __P((void *));

int (**coda_vnodeop_p)(void *);
int coda_rdwr(struct vnode *vp, struct uio *uiop, enum uio_rw rw,
    int ioflag, struct ucred *cred, struct proc *p);

int coda_grab_vnode(dev_t dev, ino_t ino, struct vnode **vpp);
void print_vattr(struct vattr *attr);
void print_cred(struct ucred *cred);
