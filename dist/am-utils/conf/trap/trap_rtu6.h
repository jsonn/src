/*	$NetBSD: trap_rtu6.h,v 1.1.1.1.4.2 2000/06/07 00:52:22 dogcow Exp $ */
/* $srcdir/conf/trap/trap_rtu6.h */
#define	MOUNT_TRAP(type, mnt, flags, mnt_data) 	vmount(type, mnt->mnt_dir, flags, mnt_data)
