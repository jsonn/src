/*	$NetBSD: trap_linux.h,v 1.1.1.1.4.2 2008/10/19 22:39:39 haad Exp $	*/

/* $srcdir/conf/trap/trap_linux.h */
extern int mount_linux(MTYPE_TYPE type, mntent_t *mnt, int flags, caddr_t data);
#define	MOUNT_TRAP(type, mnt, flags, mnt_data) 	mount_linux(type, mnt, flags, mnt_data)
