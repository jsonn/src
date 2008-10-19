/*	$NetBSD: trap_aix3.h,v 1.1.1.1.4.2 2008/10/19 22:39:38 haad Exp $	*/

/* $srcdir/conf/trap/trap_aix3.h */
extern int mount_aix3(char *fsname, char *dir, int flags, int type, void *data, char *mnt_opts);
#define	MOUNT_TRAP(type, mnt, flags, mnt_data) 	mount_aix3(mnt->mnt_fsname, mnt->mnt_dir, flags, type, mnt_data, mnt->mnt_opts)
/* there is no other better place for this missing external definition */
extern int uvmount(int VirtualFileSystemID, int Flag);
