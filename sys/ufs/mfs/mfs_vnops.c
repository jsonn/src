/*	$NetBSD: mfs_vnops.c,v 1.33.2.6 2005/03/04 16:55:00 skrll Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
 *	@(#)mfs_vnops.c	8.11 (Berkeley) 5/22/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mfs_vnops.c,v 1.33.2.6 2005/03/04 16:55:00 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <machine/vmparam.h>

#include <ufs/mfs/mfsnode.h>
#include <ufs/mfs/mfs_extern.h>

/*
 * mfs vnode operations.
 */
int (**mfs_vnodeop_p) __P((void *));
const struct vnodeopv_entry_desc mfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, mfs_lookup },		/* lookup */
	{ &vop_create_desc, mfs_create },		/* create */
	{ &vop_mknod_desc, mfs_mknod },			/* mknod */
	{ &vop_open_desc, mfs_open },			/* open */
	{ &vop_close_desc, mfs_close },			/* close */
	{ &vop_access_desc, mfs_access },		/* access */
	{ &vop_getattr_desc, mfs_getattr },		/* getattr */
	{ &vop_setattr_desc, mfs_setattr },		/* setattr */
	{ &vop_read_desc, mfs_read },			/* read */
	{ &vop_write_desc, mfs_write },			/* write */
	{ &vop_ioctl_desc, mfs_ioctl },			/* ioctl */
	{ &vop_poll_desc, mfs_poll },			/* poll */
	{ &vop_revoke_desc, mfs_revoke },		/* revoke */
	{ &vop_mmap_desc, mfs_mmap },			/* mmap */
	{ &vop_fsync_desc, spec_fsync },		/* fsync */
	{ &vop_seek_desc, mfs_seek },			/* seek */
	{ &vop_remove_desc, mfs_remove },		/* remove */
	{ &vop_link_desc, mfs_link },			/* link */
	{ &vop_rename_desc, mfs_rename },		/* rename */
	{ &vop_mkdir_desc, mfs_mkdir },			/* mkdir */
	{ &vop_rmdir_desc, mfs_rmdir },			/* rmdir */
	{ &vop_symlink_desc, mfs_symlink },		/* symlink */
	{ &vop_readdir_desc, mfs_readdir },		/* readdir */
	{ &vop_readlink_desc, mfs_readlink },		/* readlink */
	{ &vop_abortop_desc, mfs_abortop },		/* abortop */
	{ &vop_inactive_desc, mfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, mfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, mfs_lock },			/* lock */
	{ &vop_unlock_desc, mfs_unlock },		/* unlock */
	{ &vop_bmap_desc, mfs_bmap },			/* bmap */
	{ &vop_strategy_desc, mfs_strategy },		/* strategy */
	{ &vop_print_desc, mfs_print },			/* print */
	{ &vop_islocked_desc, mfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, mfs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, mfs_advlock },		/* advlock */
	{ &vop_blkatoff_desc, mfs_blkatoff },		/* blkatoff */
	{ &vop_valloc_desc, mfs_valloc },		/* valloc */
	{ &vop_vfree_desc, mfs_vfree },			/* vfree */
	{ &vop_truncate_desc, mfs_truncate },		/* truncate */
	{ &vop_update_desc, mfs_update },		/* update */
	{ &vop_bwrite_desc, mfs_bwrite },		/* bwrite */
	{ &vop_putpages_desc, mfs_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc mfs_vnodeop_opv_desc =
	{ &mfs_vnodeop_p, mfs_vnodeop_entries };

/*
 * Vnode Operations.
 *
 * Open called to allow memory filesystem to initialize and
 * validate before actual IO. Record our process identifier
 * so we can tell when we are doing I/O to ourself.
 */
/* ARGSUSED */
int
mfs_open(v)
	void *v;
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct lwp *a_l;
	} */ *ap = v;

	if (ap->a_vp->v_type != VBLK) {
		panic("mfs_ioctl not VBLK");
		/* NOTREACHED */
	}
	return (0);
}

/*
 * Pass I/O requests to the memory filesystem process.
 */
int
mfs_strategy(v)
	void *v;
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct buf *bp = ap->a_bp;
	struct mfsnode *mfsp;
	struct proc *p = curproc;		/* XXX */

	if (vp->v_type != VBLK || vp->v_usecount == 0)
		panic("mfs_strategy: bad dev");
	mfsp = VTOMFS(vp);
	/* check for mini-root access */
	if (mfsp->mfs_proc == NULL) {
		caddr_t base;

		base = mfsp->mfs_baseoff + (bp->b_blkno << DEV_BSHIFT);
		if (bp->b_flags & B_READ)
			memcpy(bp->b_data, base, bp->b_bcount);
		else
			memcpy(base, bp->b_data, bp->b_bcount);
		bp->b_resid = 0;
		biodone(bp);
	} else if (mfsp->mfs_proc == p) {
		mfs_doio(bp, mfsp->mfs_baseoff);
	} else if (doing_shutdown) {
		/*
		 * bitbucket I/O during shutdown.
		 * Note that reads should *not* happen here, but..
		 */
		if (bp->b_flags & B_READ)
			printf("warning: mfs read during shutdown\n");
		bp->b_resid = 0;
		biodone(bp);
	} else {
		BUFQ_PUT(&mfsp->mfs_buflist, bp);
		wakeup((caddr_t)vp);
	}
	return (0);
}

/*
 * Memory file system I/O.
 *
 * Trivial on the HP since buffer has already been mapping into KVA space.
 */
void
mfs_doio(bp, base)
	struct buf *bp;
	caddr_t base;
{
	base += (bp->b_blkno << DEV_BSHIFT);
	if (bp->b_flags & B_READ)
		bp->b_error = copyin(base, bp->b_data, bp->b_bcount);
	else
		bp->b_error = copyout(bp->b_data, base, bp->b_bcount);
	if (bp->b_error)
		bp->b_flags |= B_ERROR;
	else
		bp->b_resid = 0;
	biodone(bp);
}

/*
 * This is a noop, simply returning what one has been given.
 */
int
mfs_bmap(v)
	void *v;
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		 *ap->a_runp = 0;
	return (0);
}

/*
 * Memory filesystem close routine
 */
/* ARGSUSED */
int
mfs_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct mfsnode *mfsp = VTOMFS(vp);
	struct buf *bp;
	int error;

	/*
	 * Finish any pending I/O requests.
	 */
	while ((bp = BUFQ_GET(&mfsp->mfs_buflist)) != NULL) {
		mfs_doio(bp, mfsp->mfs_baseoff);
		wakeup((caddr_t)bp);
	}
	/*
	 * On last close of a memory filesystem
	 * we must invalidate any in core blocks, so that
	 * we can, free up its vnode.
	 */
	if ((error = vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_l, 0, 0)) != 0)
		return (error);
	/*
	 * There should be no way to have any more uses of this
	 * vnode, so if we find any other uses, it is a panic.
	 */
	if (vp->v_usecount > 1)
		printf("mfs_close: ref count %d > 1\n", vp->v_usecount);
	if (vp->v_usecount > 1 || BUFQ_PEEK(&mfsp->mfs_buflist) != NULL)
		panic("mfs_close");
	/*
	 * Send a request to the filesystem server to exit.
	 */
	mfsp->mfs_shutdown = 1;
	wakeup((caddr_t)vp);
	return (0);
}

/*
 * Memory filesystem inactive routine
 */
/* ARGSUSED */
int
mfs_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct mfsnode *mfsp = VTOMFS(vp);

	if (BUFQ_PEEK(&mfsp->mfs_buflist) != NULL)
		panic("mfs_inactive: not inactive (mfs_buflist %p)",
			BUFQ_PEEK(&mfsp->mfs_buflist));
	VOP_UNLOCK(vp, 0);
	return (0);
}

/*
 * Reclaim a memory filesystem devvp so that it can be reused.
 */
int
mfs_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	FREE(vp->v_data, M_MFSNODE);
	vp->v_data = NULL;
	return (0);
}

/*
 * Print out the contents of an mfsnode.
 */
int
mfs_print(v)
	void *v;
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct mfsnode *mfsp = VTOMFS(ap->a_vp);

	printf("tag VT_MFS, pid %d, base %p, size %ld\n",
	    (mfsp->mfs_proc != NULL) ? mfsp->mfs_proc->p_pid : 0,
	    mfsp->mfs_baseoff, mfsp->mfs_size);
	return (0);
}
