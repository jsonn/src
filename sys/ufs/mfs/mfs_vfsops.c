/*	$NetBSD: mfs_vfsops.c,v 1.51.2.3 2004/08/03 10:56:59 skrll Exp $	*/

/*
 * Copyright (c) 1989, 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mfs_vfsops.c	8.11 (Berkeley) 6/19/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mfs_vfsops.c,v 1.51.2.3 2004/08/03 10:56:59 skrll Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <miscfs/syncfs/syncfs.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <ufs/mfs/mfsnode.h>
#include <ufs/mfs/mfs_extern.h>

caddr_t	mfs_rootbase;	/* address of mini-root in kernel virtual memory */
u_long	mfs_rootsize;	/* size of mini-root in bytes */

static	int mfs_minor;	/* used for building internal dev_t */

extern int (**mfs_vnodeop_p) __P((void *));

MALLOC_DEFINE(M_MFSNODE, "MFS node", "MFS vnode private part");

/*
 * mfs vfs operations.
 */

extern const struct vnodeopv_desc mfs_vnodeop_opv_desc;  

const struct vnodeopv_desc * const mfs_vnodeopv_descs[] = {
	&mfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops mfs_vfsops = {
	MOUNT_MFS,
	mfs_mount,
	mfs_start,
	ffs_unmount,
	ufs_root,
	ufs_quotactl,
	mfs_statvfs,
	ffs_sync,
	ffs_vget,
	ffs_fhtovp,
	ffs_vptofh,
	mfs_init,
	mfs_reinit,
	mfs_done,
	NULL,
	NULL,
	ufs_check_export,
	(int (*)(struct mount *, struct vnode *, struct timespec *)) eopnotsupp,
	mfs_vnodeopv_descs,
};

SYSCTL_SETUP(sysctl_vfs_mfs_setup, "sysctl vfs.mfs subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "vfs", NULL,
		       NULL, 0, NULL, 0,
		       CTL_VFS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_ALIAS,
		       CTLTYPE_NODE, "mfs",
		       SYSCTL_DESCR("Memory based file system"),
		       NULL, 1, NULL, 0,
		       CTL_VFS, 3, CTL_EOL);
	/*
	 * XXX the "1" and the "3" above could be dynamic, thereby
	 * eliminating one more instance of the "number to vfs"
	 * mapping problem, but they are in order as taken from
	 * sys/mount.h
	 */
}

/* 
 * Memory based filesystem initialization.
 */ 
void
mfs_init()
{
#ifdef _LKM
	malloc_type_attach(M_MFSNODE);
#endif
	/*
	 * ffs_init() ensures to initialize necessary resources
	 * only once.
	 */
	ffs_init();
}

void
mfs_reinit()
{
	ffs_reinit();
}

void
mfs_done()
{
	/*
	 * ffs_done() ensures to free necessary resources
	 * only once, when it's no more needed.
	 */
	ffs_done();
#ifdef _LKM
	malloc_type_detach(M_MFSNODE);
#endif
}

/*
 * Called by main() when mfs is going to be mounted as root.
 */

int
mfs_mountroot()
{
	struct fs *fs;
	struct mount *mp;
	struct lwp *l = curlwp;		/* XXX */
	struct ufsmount *ump;
	struct mfsnode *mfsp;
	int error = 0;

	/*
	 * Get vnodes for rootdev.
	 */
	if (bdevvp(rootdev, &rootvp)) {
		printf("mfs_mountroot: can't setup bdevvp's");
		return (error);
	}

	if ((error = vfs_rootmountalloc(MOUNT_MFS, "mfs_root", &mp))) {
		vrele(rootvp);
		return (error);
	}

	mfsp = malloc(sizeof *mfsp, M_MFSNODE, M_WAITOK);
	rootvp->v_data = mfsp;
	rootvp->v_op = mfs_vnodeop_p;
	rootvp->v_tag = VT_MFS;
	mfsp->mfs_baseoff = mfs_rootbase;
	mfsp->mfs_size = mfs_rootsize;
	mfsp->mfs_vnode = rootvp;
	mfsp->mfs_proc = NULL;		/* indicate kernel space */
	mfsp->mfs_shutdown = 0;
	bufq_alloc(&mfsp->mfs_buflist, BUFQ_FCFS);
	if ((error = ffs_mountfs(rootvp, mp, l)) != 0) {
		mp->mnt_op->vfs_refcount--;
		vfs_unbusy(mp);
		bufq_free(&mfsp->mfs_buflist);
		free(mp, M_MOUNT);
		free(mfsp, M_MFSNODE);
		vrele(rootvp);
		return (error);
	}	
	simple_lock(&mountlist_slock);
	CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	simple_unlock(&mountlist_slock);
	mp->mnt_vnodecovered = NULLVP;
	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	(void) copystr(mp->mnt_stat.f_mntonname, fs->fs_fsmnt, MNAMELEN - 1, 0);
	(void)ffs_statvfs(mp, &mp->mnt_stat, l);
	vfs_unbusy(mp);
	return (0);
}

/*
 * This is called early in boot to set the base address and size
 * of the mini-root.
 */
int
mfs_initminiroot(base)
	caddr_t base;
{
	struct fs *fs = (struct fs *)(base + SBLOCK_UFS1);

	/* check for valid super block */
	if (fs->fs_magic != FS_UFS1_MAGIC || fs->fs_bsize > MAXBSIZE ||
	    fs->fs_bsize < sizeof(struct fs))
		return (0);
	mountroot = mfs_mountroot;
	mfs_rootbase = base;
	mfs_rootsize = fs->fs_fsize * fs->fs_size;
	rootdev = makedev(255, mfs_minor);
	mfs_minor++;
	return (mfs_rootsize);
}

/*
 * VFS Operations.
 *
 * mount system call
 */
/* ARGSUSED */
int
mfs_mount(mp, path, data, ndp, l)
	struct mount *mp;
	const char *path;
	void *data;
	struct nameidata *ndp;
	struct lwp *l;
{
	struct vnode *devvp;
	struct mfs_args args;
	struct ufsmount *ump;
	struct fs *fs;
	struct mfsnode *mfsp;
	struct proc *p;
	int flags, error;

	p = l->l_proc;
	if (mp->mnt_flag & MNT_GETARGS) {
		struct vnode *vp;
		struct mfsnode *mfsp;

		ump = VFSTOUFS(mp);
		if (ump == NULL)
			return EIO;

		vp = ump->um_devvp;
		if (vp == NULL)
			return EIO;

		mfsp = VTOMFS(vp);
		if (mfsp == NULL)
			return EIO;

		args.fspec = NULL;
		vfs_showexport(mp, &args.export, &ump->um_export);
		args.base = mfsp->mfs_baseoff;
		args.size = mfsp->mfs_size;
		return copyout(&args, data, sizeof(args));
	}
	/*
	 * XXX turn off async to avoid hangs when writing lots of data.
	 * the problem is that MFS needs to allocate pages to clean pages,
	 * so if we wait until the last minute to clean pages then there
	 * may not be any pages available to do the cleaning.
	 * ... and since the default partially-synchronous mode turns out
	 * to not be sufficient under heavy load, make it full synchronous.
	 */
	mp->mnt_flag &= ~MNT_ASYNC;
	mp->mnt_flag |= MNT_SYNCHRONOUS;

	error = copyin(data, (caddr_t)&args, sizeof (struct mfs_args));
	if (error)
		return (error);

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		ump = VFSTOUFS(mp);
		fs = ump->um_fs;
		if (fs->fs_ronly == 0 && (mp->mnt_flag & MNT_RDONLY)) {
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			error = ffs_flushfiles(mp, flags, l);
			if (error)
				return (error);
		}
		if (fs->fs_ronly && (mp->mnt_iflag & IMNT_WANTRDWR))
			fs->fs_ronly = 0;
		if (args.fspec == 0)
			return (vfs_export(mp, &ump->um_export, &args.export));
		return (0);
	}
	error = getnewvnode(VT_MFS, (struct mount *)0, mfs_vnodeop_p, &devvp);
	if (error)
		return (error);
	devvp->v_type = VBLK;
	if (checkalias(devvp, makedev(255, mfs_minor), (struct mount *)0))
		panic("mfs_mount: dup dev");
	mfs_minor++;
	mfsp = (struct mfsnode *)malloc(sizeof *mfsp, M_MFSNODE, M_WAITOK);
	devvp->v_data = mfsp;
	mfsp->mfs_baseoff = args.base;
	mfsp->mfs_size = args.size;
	mfsp->mfs_vnode = devvp;
	mfsp->mfs_proc = p;
	mfsp->mfs_shutdown = 0;
	bufq_alloc(&mfsp->mfs_buflist, BUFQ_FCFS);
	if ((error = ffs_mountfs(devvp, mp, l)) != 0) {
		mfsp->mfs_shutdown = 1;
		vrele(devvp);
		return (error);
	}
	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	error = set_statvfs_info(path, UIO_USERSPACE, args.fspec,
	    UIO_USERSPACE, mp, l);
	if (error)
		return error;
	(void)strncpy(fs->fs_fsmnt, mp->mnt_stat.f_mntonname,
		sizeof(fs->fs_fsmnt));
	fs->fs_fsmnt[sizeof(fs->fs_fsmnt) - 1] = '\0';
	/* XXX: cleanup on error */
	return 0;
}

int	mfs_pri = PWAIT | PCATCH;		/* XXX prob. temp */

/*
 * Used to grab the process and keep it in the kernel to service
 * memory filesystem I/O requests.
 *
 * Loop servicing I/O requests.
 * Copy the requested data into or out of the memory filesystem
 * address space.
 */
/* ARGSUSED */
int
mfs_start(mp, flags, l)
	struct mount *mp;
	int flags;
	struct lwp *l;
{
	struct vnode *vp = VFSTOUFS(mp)->um_devvp;
	struct mfsnode *mfsp = VTOMFS(vp);
	struct buf *bp;
	caddr_t base;
	int sleepreturn = 0;

	base = mfsp->mfs_baseoff;
	while (mfsp->mfs_shutdown != 1) {
		while ((bp = BUFQ_GET(&mfsp->mfs_buflist)) != NULL) {
			mfs_doio(bp, base);
			wakeup((caddr_t)bp);
		}
		/*
		 * If a non-ignored signal is received, try to unmount.
		 * If that fails, or the filesystem is already in the
		 * process of being unmounted, clear the signal (it has been
		 * "processed"), otherwise we will loop here, as tsleep
		 * will always return EINTR/ERESTART.
		 */
		if (sleepreturn != 0) {
			/*
			 * XXX Freeze syncer.  Must do this before locking
			 * the mount point.  See dounmount() for details.
			 */
			lockmgr(&syncer_lock, LK_EXCLUSIVE, NULL);
			if (vfs_busy(mp, LK_NOWAIT, 0) != 0)
				lockmgr(&syncer_lock, LK_RELEASE, NULL);
			else if (dounmount(mp, 0, l) != 0)
				CLRSIG(l->l_proc, CURSIG(l));
			sleepreturn = 0;
			continue;
		}

		sleepreturn = tsleep(vp, mfs_pri, "mfsidl", 0);
	}
	KASSERT(BUFQ_PEEK(&mfsp->mfs_buflist) == NULL);
	bufq_free(&mfsp->mfs_buflist);
	return (sleepreturn);
}

/*
 * Get file system statistics.
 */
int
mfs_statvfs(mp, sbp, l)
	struct mount *mp;
	struct statvfs *sbp;
	struct lwp *l;
{
	int error;

	error = ffs_statvfs(mp, sbp, l);
	if (error)
		return error;
	(void)strncpy(sbp->f_fstypename, mp->mnt_op->vfs_name,
	    sizeof(sbp->f_fstypename));
	sbp->f_fstypename[sizeof(sbp->f_fstypename) - 1] = '\0';
	return 0;
}
