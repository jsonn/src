/*	$NetBSD: spec_vnops.c,v 1.56.2.6 2001/10/01 12:47:23 fvdl Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)spec_vnops.c	8.15 (Berkeley) 7/14/95
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/lockf.h>
#include <sys/conf.h>
#include <sys/malloc.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

/* symbolic sleep message strings for devices */
const char	devopn[] = "devopn";
const char	devio[] = "devio";
const char	devwait[] = "devwait";
const char	devin[] = "devin";
const char	devout[] = "devout";
const char	devioc[] = "devioc";
const char	devcls[] = "devcls";

/*
 * This vnode operations vector is used for two things only:
 * - special device nodes created from whole cloth by the kernel.
 * - as a temporary vnodeops replacement for vnodes which were found to
 *	be aliased by callers of checkalias().
 * For the ops vector for vnodes built from special devices found in a
 * filesystem, see (e.g) ffs_specop_entries[] in ffs_vnops.c or the
 * equivalent for other filesystems.
 */

int (**spec_vnodeop_p) __P((void *));
const struct vnodeopv_entry_desc spec_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup */
	{ &vop_create_desc, spec_create },		/* create */
	{ &vop_mknod_desc, spec_mknod },		/* mknod */
	{ &vop_open_desc, spec_open },			/* open */
	{ &vop_close_desc, spec_close },		/* close */
	{ &vop_access_desc, spec_access },		/* access */
	{ &vop_getattr_desc, spec_getattr },		/* getattr */
	{ &vop_setattr_desc, spec_setattr },		/* setattr */
	{ &vop_read_desc, spec_read },			/* read */
	{ &vop_write_desc, spec_write },		/* write */
	{ &vop_lease_desc, spec_lease_check },		/* lease */
	{ &vop_fcntl_desc, spec_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, spec_ioctl },		/* ioctl */
	{ &vop_poll_desc, spec_poll },			/* poll */
	{ &vop_revoke_desc, spec_revoke },		/* revoke */
	{ &vop_mmap_desc, spec_mmap },			/* mmap */
	{ &vop_fsync_desc, spec_fsync },		/* fsync */
	{ &vop_seek_desc, spec_seek },			/* seek */
	{ &vop_remove_desc, spec_remove },		/* remove */
	{ &vop_link_desc, spec_link },			/* link */
	{ &vop_rename_desc, spec_rename },		/* rename */
	{ &vop_mkdir_desc, spec_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, spec_rmdir },		/* rmdir */
	{ &vop_symlink_desc, spec_symlink },		/* symlink */
	{ &vop_readdir_desc, spec_readdir },		/* readdir */
	{ &vop_readlink_desc, spec_readlink },		/* readlink */
	{ &vop_abortop_desc, spec_abortop },		/* abortop */
	{ &vop_inactive_desc, spec_inactive },		/* inactive */
	{ &vop_reclaim_desc, spec_reclaim },		/* reclaim */
	{ &vop_lock_desc, spec_lock },			/* lock */
	{ &vop_unlock_desc, spec_unlock },		/* unlock */
	{ &vop_bmap_desc, spec_bmap },			/* bmap */
	{ &vop_strategy_desc, spec_strategy },		/* strategy */
	{ &vop_print_desc, spec_print },		/* print */
	{ &vop_islocked_desc, spec_islocked },		/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* advlock */
	{ &vop_blkatoff_desc, spec_blkatoff },		/* blkatoff */
	{ &vop_valloc_desc, spec_valloc },		/* valloc */
	{ &vop_vfree_desc, spec_vfree },		/* vfree */
	{ &vop_truncate_desc, spec_truncate },		/* truncate */
	{ &vop_update_desc, spec_update },		/* update */
	{ &vop_bwrite_desc, spec_bwrite },		/* bwrite */
	{ &vop_getpages_desc, spec_getpages },		/* getpages */
	{ &vop_putpages_desc, spec_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc spec_vnodeop_opv_desc =
	{ &spec_vnodeop_p, spec_vnodeop_entries };

static int spec_clonevnode(struct vnode *, struct vnode **, dev_t, int,
			   struct proc *);

/*
 * Trivial lookup routine that always fails.
 */
int
spec_lookup(void *v)
{
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;

	*ap->a_vpp = NULL;
	return (ENOTDIR);
}

/*
 * Open a special file.
 */
/* ARGSUSED */
int
spec_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
		struct vnode *a_vpp;
	} */ *ap = v;
	struct proc *p = ap->a_p;
	struct vnode *bvp, *vp = ap->a_vp, **vpp = ap->a_vpp;
	dev_t bdev, dev = (dev_t)vp->v_rdev;
	int maj = major(dev);
	int error;
	struct partinfo pi;

	/*
	 * Don't allow open if fs is mounted -nodev.
	 */
	if (vp->v_mount && (vp->v_mount->mnt_flag & MNT_NODEV))
		return (ENXIO);

	if (vp->v_specinfo == NULL)
		panic("spec_open: NULL specinfo");

	switch (vp->v_type) {

	case VCHR:
		if ((u_int)maj >= nchrdev)
			return (ENXIO);
		if (ap->a_cred != FSCRED && (ap->a_mode & FWRITE)) {
			/*
			 * When running in very secure mode, do not allow
			 * opens for writing of any disk character devices.
			 */
			if (securelevel >= 2 && cdevsw[maj].d_type == D_DISK)
				return (EPERM);
			/*
			 * When running in secure mode, do not allow opens
			 * for writing of /dev/mem, /dev/kmem, or character
			 * devices whose corresponding block devices are
			 * currently mounted.
			 */
			if (securelevel >= 1) {
				if ((bdev = chrtoblk(dev)) != (dev_t)NODEV &&
				    vfinddev(bdev, VBLK, &bvp) &&
				    (error = vfs_mountedon(bvp)))
					return (error);
				if (iskmemdev(dev))
					return (EPERM);
			}
		}
		if (cdevsw[maj].d_type == D_TTY)
			vp->v_flag |= VISTTY;

		if (iscloningcdev(dev) && vpp != NULL) {
			error = spec_clonevnode(vp, vpp, dev, VCHR, p);
			if (error != 0)
				return error;
			vp = *vpp;
		} else
			VOP_UNLOCK(vp, 0);
		error = (*cdevsw[maj].d_open)(vp, ap->a_mode, S_IFCHR, p);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		return (error);

	case VBLK:
		if ((u_int)maj >= nblkdev)
			return (ENXIO);
		/*
		 * When running in very secure mode, do not allow
		 * opens for writing of any disk block devices.
		 */
		if (securelevel >= 2 && ap->a_cred != FSCRED &&
		    (ap->a_mode & FWRITE) && bdevsw[maj].d_type == D_DISK)
			return (EPERM);
		/*
		 * Do not allow opens of block devices that are
		 * currently mounted.
		 */
		if ((error = vfs_mountedon(vp)) != 0)
			return (error);
		/*
		 * XXXXXfvdl why is the vnode returned unlocked for
		 * a character device open but not for a block device??
		 * This can't be right.
		 */
		if (iscloningbdev(dev) && vpp != NULL) {
			error = spec_clonevnode(vp, vpp, dev, VBLK, p);
			if (error != 0)
				return error;
			vp = *vpp;
		}
		error = (*bdevsw[maj].d_open)(vp, ap->a_mode, S_IFBLK, p);
		if (error)
			return error;
		error = (*bdevsw[major(vp->v_rdev)].d_ioctl)(vp,
		    DIOCGPART, (caddr_t)&pi, FREAD, curproc);
		if (error == 0) {
			vp->v_size = (voff_t)pi.disklab->d_secsize *
			    pi.part->p_size;
		}
		return 0;

	case VNON:
	case VLNK:
	case VDIR:
	case VREG:
	case VBAD:
	case VFIFO:
	case VSOCK:
		break;
	}
	return (0);
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
int
spec_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
 	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr_t bn;
	int bsize, bscale;
	struct partinfo dpart;
	int n, on, majordev;
	int (*ioctl) __P((struct vnode *, u_long, caddr_t, int, struct proc *));
	int error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("spec_read mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("spec_read proc");
#endif
	if (uio->uio_resid == 0)
		return (0);

	switch (vp->v_type) {

	case VCHR:
		VOP_UNLOCK(vp, 0);
		error = (*cdevsw[major(vp->v_rdev)].d_read)
			(vp, uio, ap->a_ioflag);
		vn_lock(vp, LK_SHARED | LK_RETRY);
		return (error);

	case VBLK:
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;
		if ((majordev = major(vp->v_rdev)) < nblkdev &&
		    (ioctl = bdevsw[majordev].d_ioctl) != NULL &&
		    (*ioctl)(vp, DIOCGPART, (caddr_t)&dpart, FREAD, p) == 0) {
			if (dpart.part->p_fstype == FS_BSDFFS &&
			    dpart.part->p_frag != 0 && dpart.part->p_fsize != 0)
				bsize = dpart.part->p_frag *
				    dpart.part->p_fsize;
		}
		bscale = bsize >> DEV_BSHIFT;
		do {
			bn = (uio->uio_offset >> DEV_BSHIFT) &~ (bscale - 1);
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			error = bread(vp, bn, bsize, NOCRED, &bp);
			n = min(n, bsize - bp->b_resid);
			if (error) {
				brelse(bp);
				return (error);
			}
			error = uiomove((char *)bp->b_data + on, n, uio);
			brelse(bp);
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("spec_read type");
	}
	/* NOTREACHED */
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
int
spec_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr_t bn;
	int bsize, bscale;
	struct partinfo dpart;
	int n, on, majordev;
	int (*ioctl) __P((struct vnode *, u_long, caddr_t, int, struct proc *));
	int error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("spec_write mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("spec_write proc");
#endif

	switch (vp->v_type) {

	case VCHR:
		VOP_UNLOCK(vp, 0);
		error = (*cdevsw[major(vp->v_rdev)].d_write)
			(vp, uio, ap->a_ioflag);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		return (error);

	case VBLK:
		if (uio->uio_resid == 0)
			return (0);
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;
		if ((majordev = major(vp->v_rdev)) < nblkdev &&
		    (ioctl = bdevsw[majordev].d_ioctl) != NULL &&
		    (*ioctl)(vp, DIOCGPART, (caddr_t)&dpart, FREAD, p) == 0) {
			if (dpart.part->p_fstype == FS_BSDFFS &&
			    dpart.part->p_frag != 0 && dpart.part->p_fsize != 0)
				bsize = dpart.part->p_frag *
				    dpart.part->p_fsize;
		}
		bscale = bsize >> DEV_BSHIFT;
		do {
			bn = (uio->uio_offset >> DEV_BSHIFT) &~ (bscale - 1);
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			if (n == bsize)
				bp = getblk(vp, bn, bsize, 0, 0);
			else
				error = bread(vp, bn, bsize, NOCRED, &bp);
			if (error) {
				brelse(bp);
				return (error);
			}
			n = min(n, bsize - bp->b_resid);
			error = uiomove((char *)bp->b_data + on, n, uio);
			if (error)
				brelse(bp);
			else {
				if (n + on == bsize)
					bawrite(bp);
				else
					bdwrite(bp);
				if (bp->b_flags & B_ERROR)
					error = bp->b_error;
			}
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("spec_write type");
	}
	/* NOTREACHED */
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
int
spec_ioctl(void *v)
{
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	dev_t dev = ap->a_vp->v_rdev;
	int maj = major(dev);

	switch (ap->a_vp->v_type) {

	case VCHR:
		return ((*cdevsw[maj].d_ioctl)(ap->a_vp, ap->a_command,
		    ap->a_data, ap->a_fflag, ap->a_p));

	case VBLK:
		if (ap->a_command == 0 && (long)ap->a_data == B_TAPE) {
			if (bdevsw[maj].d_type == D_TAPE)
				return (0);
			else
				return (1);
		}
		return ((*bdevsw[maj].d_ioctl)(ap->a_vp, ap->a_command,
		    ap->a_data, ap->a_fflag, ap->a_p));

	default:
		panic("spec_ioctl");
		/* NOTREACHED */
	}
}

/* ARGSUSED */
int
spec_poll(void *v)
{
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int a_events;
		struct proc *a_p;
	} */ *ap = v;
	dev_t dev;

	switch (ap->a_vp->v_type) {

	case VCHR:
		dev = ap->a_vp->v_rdev;
		return (*cdevsw[major(dev)].d_poll)(ap->a_vp, ap->a_events,
		    ap->a_p);

	default:
		return (genfs_poll(v));
	}
}
/*
 * Synch buffers associated with a block device
 */
/* ARGSUSED */
int
spec_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int  a_flags;
		off_t offlo;
		off_t offhi;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_type == VBLK)
		vflushbuf(vp, (ap->a_flags & FSYNC_WAIT) != 0);
	return (0);
}

/*
 * Just call the device strategy routine
 */
int
spec_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap = v;
	struct buf *bp;

	bp = ap->a_bp;
	if (!(bp->b_flags & B_READ) &&
	    (LIST_FIRST(&bp->b_dep)) != NULL && bioops.io_start)
		(*bioops.io_start)(bp);
	(*bdevsw[major(bp->b_devvp->v_rdev)].d_strategy)(bp);
	return (0);
}

int
spec_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;

	VOP_UNLOCK(ap->a_vp, 0);
	return (0);
}

/*
 * This is a noop, simply returning what one has been given.
 */
int
spec_bmap(void *v)
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
		*ap->a_runp = (MAXBSIZE >> DEV_BSHIFT) - 1;
	return (0);
}

/*
 * Device close routine
 */
/* ARGSUSED */
int
spec_close(void *v)
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	dev_t dev = vp->v_rdev;
	int error, count, flags, fflag;

	count = vcount(vp);
	simple_lock(&vp->v_interlock);
	flags = vp->v_flag;
	simple_unlock(&vp->v_interlock);

	fflag = ap->a_fflag;
	if (flags & VXLOCK)
		fflag |= FNONBLOCK;

	switch (vp->v_type) {

	case VCHR:
		/*
		 * Hack: a tty device that is a controlling terminal
		 * has a reference from the session structure.
		 * We cannot easily tell that a character device is
		 * a controlling terminal, unless it is the closing
		 * process' controlling terminal.  In that case,
		 * if the reference count is 2 (this last descriptor
		 * plus the session), release the reference from the session.
		 */

		/* XXXDEVVP */
		if (count == 2 && ap->a_p &&
		    vp == ap->a_p->p_session->s_ttyvp) {
			vrele(vp);
			count--;
			ap->a_p->p_session->s_ttyvp = NULL;
		}
		/*
		 * If the vnode is locked, then we are in the midst
		 * of forcably closing the device, otherwise we only
		 * close on last reference.
		 */
		if (count > 1 && (flags & VXLOCK) == 0)
			return (0);
		if ((flags & VXLOCK) == 0)
			VOP_UNLOCK(vp, 0);
		error = cdevsw[major(dev)].d_close(vp, fflag, S_IFCHR, ap->a_p);
		if ((flags & VXLOCK) == 0)
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		break;

	case VBLK:
		/*
		 * On last close of a block device (that isn't mounted)
		 * we must invalidate any in core blocks, so that
		 * we can, for instance, change floppy disks.
		 */
		error = vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 0, 0);
		if (error)
			return (error);
		/*
		 * We do not want to really close the device if it
		 * is still in use unless we are trying to close it
		 * forcibly. Since every use (buffer, vnode, swap, cmap)
		 * holds a reference to the vnode, and because we mark
		 * any other vnodes that alias this device, when the
		 * sum of the reference counts on all the aliased
		 * vnodes descends to one, we are on last close.
		 */
		if (count > 1 && (flags & VXLOCK) == 0)
			return (0);
		error = bdevsw[major(dev)].d_close(vp, fflag, S_IFBLK, ap->a_p);
		break;

	default:
		panic("spec_close: not special");
	}

	return (error);
}

/*
 * Print out the contents of a special device vnode.
 */
int
spec_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;

	printf("tag VT_NON, dev %d, %d\n", major(ap->a_vp->v_rdev),
	    minor(ap->a_vp->v_rdev));
	return 0;
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
int
spec_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/* 
 * Advisory record locking support.
 */
int
spec_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t a_id;
		int a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	return lf_advlock(ap, &vp->v_speclockf, (off_t)0);
}

int
spec_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp;

	vp = ap->a_vp;

	if ((vp->v_flag & VCLONED) == 0)
		return EBADF;

	return vaccess(vp->v_type, vp->v_cloneattr->va_mode & ALLPERMS,
	    vp->v_cloneattr->va_uid, vp->v_cloneattr->va_gid, ap->a_mode,
	    ap->a_cred);
}

int
spec_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct prof *a_p;
	} */ *ap = v;
	struct vnode *vp;

	vp = ap->a_vp;

	if ((vp->v_flag & VCLONED) == 0)
		return EBADF;

	*ap->a_vap = *vp->v_cloneattr;
	ap->a_vap->va_rdev = vp->v_rdev;
	return 0;
}

/*
 * XXX can probably always do locking, but not for now, until it can
 * be verified whether vnodes created with {b,c}devvp during bootstrap
 * are OK to lock.
 */
int
spec_lock(void *v)
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_flag & VCLONED)
		return genfs_lock(ap);
	else
		return genfs_nolock(ap);
}

/*
 * Unlock the node.
 */
int
spec_unlock(void *v)
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_flag & VCLONED)
		return genfs_unlock(ap);
	else
		return genfs_nolock(ap);
}

/*
 * Return whether or not the node is locked.
 */
int
spec_islocked(void *v)
{
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_flag & VCLONED)
		return genfs_islocked(ap);
	else
		return genfs_noislocked(ap);
}

/*
 * Create a clone of a vnode.
 * XXX convoluted locking code, because if the inconsistency between
 * locking for VCHR and VBLK vnodes.
 */
static int
spec_clonevnode(struct vnode *vp, struct vnode **vpp, dev_t dev, int type,
		struct proc *p)
{
	int error;
	struct vattr *vap;

	VOP_UNLOCK(vp, 0);

	error = type == VCHR ? cdevvp(dev, vpp) : bdevvp(dev, vpp);
	if (error != 0)
		goto out;
	vap = malloc(sizeof (struct vattr), M_VNODE, M_WAITOK);
	error = VOP_GETATTR(vp, vap, p->p_ucred, p);
	if (error != 0) {
		vrele(*vpp);
		free(vap, M_VNODE);
		goto out;
	}
out:
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (type == VBLK)
		vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
	/*
	 * XXX check if vnode was revoked while sleeping for the lock.
	 * Can only happen in the VCHR case.
	 */
	if ((*vpp)->v_type == VBAD) {
		vput(*vpp);
		free(vap, M_VNODE);
		return EIO;
	}

	if (error == 0) {
		(*vpp)->v_cloneattr = vap;
		(*vpp)->v_flag |= VCLONED;
	}
	return error;
}
