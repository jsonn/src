/*	$NetBSD: vfs_vnops.c,v 1.22.4.1 1997/10/14 10:26:30 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)vfs_vnops.c	8.5 (Berkeley) 12/8/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/poll.h>

#include <vm/vm.h>

struct 	fileops vnops =
	{ vn_read, vn_write, vn_ioctl, vn_poll, vn_closefile };

/*
 * Common code for vnode open operations.
 * Check permissions, and call the VOP_OPEN or VOP_CREATE routine.
 */
int
vn_open(ndp, fmode, cmode)
	register struct nameidata *ndp;
	int fmode, cmode;
{
	register struct vnode *vp;
	register struct proc *p = ndp->ni_cnd.cn_proc;
	register struct ucred *cred = p->p_ucred;
	struct vattr va;
	int error;

	if (fmode & O_CREAT) {
		ndp->ni_cnd.cn_nameiop = CREATE;
		ndp->ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
		if ((fmode & O_EXCL) == 0)
			ndp->ni_cnd.cn_flags |= FOLLOW;
		if ((error = namei(ndp)) != 0)
			return (error);
		if (ndp->ni_vp == NULL) {
			VATTR_NULL(&va);
			va.va_type = VREG;
			va.va_mode = cmode;
			VOP_LEASE(ndp->ni_dvp, p, cred, LEASE_WRITE);
			error = VOP_CREATE(ndp->ni_dvp, &ndp->ni_vp,
					   &ndp->ni_cnd, &va);
			if (error)
				return (error);
			fmode &= ~O_TRUNC;
			vp = ndp->ni_vp;
		} else {
			VOP_ABORTOP(ndp->ni_dvp, &ndp->ni_cnd);
			if (ndp->ni_dvp == ndp->ni_vp)
				vrele(ndp->ni_dvp);
			else
				vput(ndp->ni_dvp);
			ndp->ni_dvp = NULL;
			vp = ndp->ni_vp;
			if (fmode & O_EXCL) {
				error = EEXIST;
				goto bad;
			}
			fmode &= ~O_CREAT;
		}
	} else {
		ndp->ni_cnd.cn_nameiop = LOOKUP;
		ndp->ni_cnd.cn_flags = FOLLOW | LOCKLEAF;
		if ((error = namei(ndp)) != 0)
			return (error);
		vp = ndp->ni_vp;
	}
	if (vp->v_type == VSOCK) {
		error = EOPNOTSUPP;
		goto bad;
	}
	if ((fmode & O_CREAT) == 0) {
		if (fmode & FREAD) {
			if ((error = VOP_ACCESS(vp, VREAD, cred, p)) != 0)
				goto bad;
		}
		if (fmode & (FWRITE | O_TRUNC)) {
			if (vp->v_type == VDIR) {
				error = EISDIR;
				goto bad;
			}
			if ((error = vn_writechk(vp)) != 0 ||
			    (error = VOP_ACCESS(vp, VWRITE, cred, p)) != 0)
				goto bad;
		}
	}
	if (fmode & O_TRUNC) {
		VOP_UNLOCK(vp);				/* XXX */
		VOP_LEASE(vp, p, cred, LEASE_WRITE);
		VOP_LOCK(vp);				/* XXX */
		VATTR_NULL(&va);
		va.va_size = 0;
		if ((error = VOP_SETATTR(vp, &va, cred, p)) != 0)
			goto bad;
	}
	if ((error = VOP_OPEN(vp, fmode, cred, p)) != 0)
		goto bad;
	if (fmode & FWRITE)
		vp->v_writecount++;
	return (0);
bad:
	vput(vp);
	return (error);
}

/*
 * Check for write permissions on the specified vnode.
 * The read-only status of the file system is checked.
 * Also, prototype text segments cannot be written.
 */
int
vn_writechk(vp)
	register struct vnode *vp;
{

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket or a block or character
	 * device resident on the file system.
	 */
	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		switch (vp->v_type) {
		case VREG: case VDIR: case VLNK:
			return (EROFS);
		case VNON: case VCHR: case VSOCK:
		case VFIFO: case VBAD: case VBLK:
			break;
		}
	}
	/*
	 * If there's shared text associated with
	 * the vnode, try to free it up once.  If
	 * we fail, we can't allow writing.
	 */
	if ((vp->v_flag & VTEXT) && !vnode_pager_uncache(vp))
		return (ETXTBSY);
	return (0);
}

/*
 * Vnode close call
 */
int
vn_close(vp, flags, cred, p)
	register struct vnode *vp;
	int flags;
	struct ucred *cred;
	struct proc *p;
{
	int error;

	if (flags & FWRITE)
		vp->v_writecount--;
	error = VOP_CLOSE(vp, flags, cred, p);
	vrele(vp);
	return (error);
}

/*
 * Package up an I/O request on a vnode into a uio and do it.
 */
int
vn_rdwr(rw, vp, base, len, offset, segflg, ioflg, cred, aresid, p)
	enum uio_rw rw;
	struct vnode *vp;
	caddr_t base;
	int len;
	off_t offset;
	enum uio_seg segflg;
	int ioflg;
	struct ucred *cred;
	int *aresid;
	struct proc *p;
{
	struct uio auio;
	struct iovec aiov;
	int error;

	if ((ioflg & IO_NODELOCKED) == 0)
		VOP_LOCK(vp);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = base;
	aiov.iov_len = len;
	auio.uio_resid = len;
	auio.uio_offset = offset;
	auio.uio_segflg = segflg;
	auio.uio_rw = rw;
	auio.uio_procp = p;
	if (rw == UIO_READ) {
		error = VOP_READ(vp, &auio, ioflg, cred);
	} else {
		error = VOP_WRITE(vp, &auio, ioflg, cred);
	}
	if (aresid)
		*aresid = auio.uio_resid;
	else
		if (auio.uio_resid && error == 0)
			error = EIO;
	if ((ioflg & IO_NODELOCKED) == 0)
		VOP_UNLOCK(vp);
	return (error);
}

int
vn_readdir(fp, buf, segflg, count, done, p, cookies, ncookies)
	struct file *fp;
	char *buf;
	int segflg, *done, ncookies;
	u_int count;
	struct proc *p;
	off_t *cookies;
{
	struct vnode *vp = (struct vnode *)fp->f_data;
	struct iovec aiov;
	struct uio auio;
	int error, eofflag;

unionread:
	if (vp->v_type != VDIR)
		return (EINVAL);
	aiov.iov_base = buf;
	aiov.iov_len = count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = segflg;
	auio.uio_procp = p;
	auio.uio_resid = count;
	VOP_LOCK(vp);
	auio.uio_offset = fp->f_offset;
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, (off_t *)cookies,
		    ncookies);
	fp->f_offset = auio.uio_offset;
	VOP_UNLOCK(vp);
	if (error)
		return (error);

#ifdef UNION
{
	extern int (**union_vnodeop_p) __P((void *));
	extern struct vnode *union_dircache __P((struct vnode *));

	if (count == auio.uio_resid && (vp->v_op == union_vnodeop_p)) {
		struct vnode *lvp;

		lvp = union_dircache(vp);
		if (lvp != NULLVP) {
			struct vattr va;

			/*
			 * If the directory is opaque,
			 * then don't show lower entries
			 */
			error = VOP_GETATTR(vp, &va, fp->f_cred, p);
			if (va.va_flags & OPAQUE) {
				vput(lvp);
				lvp = NULL;
			}
		}
		
		if (lvp != NULLVP) {
			error = VOP_OPEN(lvp, FREAD, fp->f_cred, p);
			VOP_UNLOCK(lvp);

			if (error) {
				vrele(lvp);
				return (error);
			}
			fp->f_data = (caddr_t) lvp;
			fp->f_offset = 0;
			error = vn_close(vp, FREAD, fp->f_cred, p);
			if (error)
				return (error);
			vp = lvp;
			goto unionread;
		}
	}
}
#endif /* UNION */

	if (count == auio.uio_resid && (vp->v_flag & VROOT) &&
	    (vp->v_mount->mnt_flag & MNT_UNION)) {
		struct vnode *tvp = vp;
		vp = vp->v_mount->mnt_vnodecovered;
		VREF(vp);
		fp->f_data = (caddr_t) vp;
		fp->f_offset = 0;
		vrele(tvp);
		goto unionread;
	}
	*done = count - auio.uio_resid;
	return error;
}

/*
 * File table vnode read routine.
 */
int
vn_read(fp, uio, cred)
	struct file *fp;
	struct uio *uio;
	struct ucred *cred;
{
	register struct vnode *vp = (struct vnode *)fp->f_data;
	int count, error;

	VOP_LEASE(vp, uio->uio_procp, cred, LEASE_READ);
	VOP_LOCK(vp);
	uio->uio_offset = fp->f_offset;
	count = uio->uio_resid;
	error = VOP_READ(vp, uio, (fp->f_flag & FNONBLOCK) ? IO_NDELAY : 0,
		cred);
	fp->f_offset += count - uio->uio_resid;
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * File table vnode write routine.
 */
int
vn_write(fp, uio, cred)
	struct file *fp;
	struct uio *uio;
	struct ucred *cred;
{
	register struct vnode *vp = (struct vnode *)fp->f_data;
	int count, error, ioflag = IO_UNIT;

	if (vp->v_type == VREG && (fp->f_flag & O_APPEND))
		ioflag |= IO_APPEND;
	if (fp->f_flag & FNONBLOCK)
		ioflag |= IO_NDELAY;
	VOP_LEASE(vp, uio->uio_procp, cred, LEASE_WRITE);
	VOP_LOCK(vp);
	uio->uio_offset = fp->f_offset;
	count = uio->uio_resid;
	error = VOP_WRITE(vp, uio, ioflag, cred);
	if (ioflag & IO_APPEND)
		fp->f_offset = uio->uio_offset;
	else
		fp->f_offset += count - uio->uio_resid;
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * File table vnode stat routine.
 */
int
vn_stat(vp, sb, p)
	struct vnode *vp;
	register struct stat *sb;
	struct proc *p;
{
	struct vattr va;
	int error;
	u_short mode;

	error = VOP_GETATTR(vp, &va, p->p_ucred, p);
	if (error)
		return (error);
	/*
	 * Copy from vattr table
	 */
	sb->st_dev = va.va_fsid;
	sb->st_ino = va.va_fileid;
	mode = va.va_mode;
	switch (vp->v_type) {
	case VREG:
		mode |= S_IFREG;
		break;
	case VDIR:
		mode |= S_IFDIR;
		break;
	case VBLK:
		mode |= S_IFBLK;
		break;
	case VCHR:
		mode |= S_IFCHR;
		break;
	case VLNK:
		mode |= S_IFLNK;
		break;
	case VSOCK:
		mode |= S_IFSOCK;
		break;
	case VFIFO:
		mode |= S_IFIFO;
		break;
	default:
		return (EBADF);
	};
	sb->st_mode = mode;
	sb->st_nlink = va.va_nlink;
	sb->st_uid = va.va_uid;
	sb->st_gid = va.va_gid;
	sb->st_rdev = va.va_rdev;
	sb->st_size = va.va_size;
	sb->st_atimespec = va.va_atime;
	sb->st_mtimespec = va.va_mtime;
	sb->st_ctimespec = va.va_ctime;
	sb->st_blksize = va.va_blocksize;
	sb->st_flags = va.va_flags;
	sb->st_gen = 0;
	sb->st_blocks = va.va_bytes / S_BLKSIZE;
	return (0);
}

/*
 * File table vnode ioctl routine.
 */
int
vn_ioctl(fp, com, data, p)
	struct file *fp;
	u_long com;
	caddr_t data;
	struct proc *p;
{
	register struct vnode *vp = ((struct vnode *)fp->f_data);
	struct vattr vattr;
	int error;

	switch (vp->v_type) {

	case VREG:
	case VDIR:
		if (com == FIONREAD) {
			error = VOP_GETATTR(vp, &vattr, p->p_ucred, p);
			if (error)
				return (error);
			*(int *)data = vattr.va_size - fp->f_offset;
			return (0);
		}
		if (com == FIONBIO || com == FIOASYNC)	/* XXX */
			return (0);			/* XXX */
		/* fall into ... */

	default:
		return (ENOTTY);

	case VFIFO:
	case VCHR:
	case VBLK:
		error = VOP_IOCTL(vp, com, data, fp->f_flag, p->p_ucred, p);
		if (error == 0 && com == TIOCSCTTY) {
			if (p->p_session->s_ttyvp)
				vrele(p->p_session->s_ttyvp);
			p->p_session->s_ttyvp = vp;
			VREF(vp);
		}
		return (error);
	}
}

/*
 * File table vnode poll routine.
 */
int
vn_poll(fp, events, p)
	struct file *fp;
	int events;
	struct proc *p;
{

	return (VOP_POLL(((struct vnode *)fp->f_data), events, p));
}

/*
 * File table vnode close routine.
 */
int
vn_closefile(fp, p)
	struct file *fp;
	struct proc *p;
{

	return (vn_close(((struct vnode *)fp->f_data), fp->f_flag,
		fp->f_cred, p));
}
