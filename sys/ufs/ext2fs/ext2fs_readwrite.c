/*	$NetBSD: ext2fs_readwrite.c,v 1.2.6.1 1997/10/24 20:24:40 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 Manuel Bouyer.
 * Copyright (c) 1993
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
 *	@(#)ufs_readwrite.c	8.8 (Berkeley) 8/4/94
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>

#include <vm/vm.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>


#define doclusterread 0 /* XXX underway */
#define doclusterwrite 0

/*
 * Vnode op for reading.
 */
/* ARGSUSED */
int
ext2fs_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	register struct vnode *vp;
	register struct inode *ip;
	register struct uio *uio;
	register struct m_ext2fs *fs;
	struct buf *bp;
	daddr_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	int error;

	vp = ap->a_vp;
	ip = VTOI(vp);
	uio = ap->a_uio;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("%s: mode", "ext2fs_read");

	if (vp->v_type == VLNK) {
		if ((int)ip->i_e2fs_size < vp->v_mount->mnt_maxsymlinklen ||
			(vp->v_mount->mnt_maxsymlinklen == 0 &&
			 ip->i_e2fs_nblock == 0))
			panic("%s: short symlink", "ext2fs_read");
	} else if (vp->v_type != VREG && vp->v_type != VDIR)
		panic("%s: type %d", "ext2fs_read", vp->v_type);
#endif
	fs = ip->i_e2fs;
	if ((u_int64_t)uio->uio_offset >
		((u_int64_t)0x80000000 * fs->e2fs_bsize - 1))
		return (EFBIG);
	if (uio->uio_resid == 0)
		return (0);

	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		if ((bytesinfile = ip->i_e2fs_size - uio->uio_offset) <= 0)
			break;
		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;
		size = fs->e2fs_bsize;
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->e2fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

		if (lblktosize(fs, nextlbn) >= ip->i_e2fs_size)
			error = bread(vp, lbn, size, NOCRED, &bp);
		else if (doclusterread)
			error = cluster_read(vp,
				ip->i_e2fs_size, lbn, size, NOCRED, &bp);
		else if (lbn - 1 == vp->v_lastr) {
			int nextsize = fs->e2fs_bsize;
			error = breadn(vp, lbn,
				size, &nextlbn, &nextsize, 1, NOCRED, &bp);
		} else
			error = bread(vp, lbn, size, NOCRED, &bp);
		if (error)
			break;
		vp->v_lastr = lbn;

		/*
		 * We should only get non-zero b_resid when an I/O error
		 * has occurred, which should cause us to break above.
		 * However, if the short read did not cause an error,
		 * then we want to ensure that we do not uiomove bad
		 * or uninitialized data.
		 */
		size -= bp->b_resid;
		if (size < xfersize) {
			if (size == 0)
				break;
			xfersize = size;
		}
		error = uiomove((char *)bp->b_data + blkoffset, (int)xfersize,
				uio);
		if (error)
			break;
		brelse(bp);
	}
	if (bp != NULL)
		brelse(bp);
	if (!(vp->v_mount->mnt_flag & MNT_NOATIME))
		ip->i_flag |= IN_ACCESS;
	return (error);
}

/*
 * Vnode op for writing.
 */
int
ext2fs_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	register struct vnode *vp;
	register struct uio *uio;
	register struct inode *ip;
	register struct m_ext2fs *fs;
	struct buf *bp;
	struct proc *p;
	daddr_t lbn;
	off_t osize;
	int blkoffset, error, flags, ioflag, resid, size, xfersize;
	struct timespec ts;

	ioflag = ap->a_ioflag;
	uio = ap->a_uio;
	vp = ap->a_vp;
	ip = VTOI(vp);

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("%s: mode", "ext2fs_write");
#endif

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = ip->i_e2fs_size;
		if ((ip->i_e2fs_flags & EXT2_APPEND) &&
			uio->uio_offset != ip->i_e2fs_size)
			return (EPERM);
		/* FALLTHROUGH */
	case VLNK:
		break;
	case VDIR:
		if ((ioflag & IO_SYNC) == 0)
			panic("%s: nonsync dir write", "ext2fs_write");
		break;
	default:
		panic("%s: type", "ext2fs_write");
	}

	fs = ip->i_e2fs;
	if (uio->uio_offset < 0 ||
		(u_int64_t)uio->uio_offset + uio->uio_resid >
		((u_int64_t)0x80000000 * fs->e2fs_bsize - 1))
		return (EFBIG);
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, I don't think it matters.
	 */
	p = uio->uio_procp;
	if (vp->v_type == VREG && p &&
		uio->uio_offset + uio->uio_resid >
		p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		psignal(p, SIGXFSZ);
		return (EFBIG);
	}

	resid = uio->uio_resid;
	osize = ip->i_e2fs_size;
	flags = ioflag & IO_SYNC ? B_SYNC : 0;

	for (error = 0; uio->uio_resid > 0;) {
		lbn = lblkno(fs, uio->uio_offset);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->e2fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (fs->e2fs_bsize > xfersize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;

		error = ext2fs_balloc(ip,
			lbn, blkoffset + xfersize, ap->a_cred, &bp, flags);
		if (error)
			break;
		if (uio->uio_offset + xfersize > ip->i_e2fs_size) {
			ip->i_e2fs_size = uio->uio_offset + xfersize;
			vnode_pager_setsize(vp, ip->i_e2fs_size);
		}
		(void)vnode_pager_uncache(vp);

		size = fs->e2fs_bsize - bp->b_resid;
		if (size < xfersize)
			xfersize = size;

		error =
			uiomove((char *)bp->b_data + blkoffset, (int)xfersize, uio);
		if (ioflag & IO_SYNC)
			(void)bwrite(bp);
		else if (xfersize + blkoffset == fs->e2fs_bsize)
			if (doclusterwrite)
				cluster_write(bp, ip->i_e2fs_size);
			else
				bawrite(bp);
		else
			bdwrite(bp);
		if (error || xfersize == 0)
			break;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
	if (resid > uio->uio_resid && ap->a_cred && ap->a_cred->cr_uid != 0)
		ip->i_e2fs_mode &= ~(ISUID | ISGID);
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)VOP_TRUNCATE(vp, osize,
				ioflag & IO_SYNC, ap->a_cred, uio->uio_procp);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC)) {
		TIMEVAL_TO_TIMESPEC(&time, &ts);
		error = VOP_UPDATE(vp, &ts, &ts, 1);
	}
	return (error);
}
