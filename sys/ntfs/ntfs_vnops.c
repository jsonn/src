/*	$NetBSD: ntfs_vnops.c,v 1.36.4.1 2001/11/12 21:19:40 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 *	Id: ntfs_vnops.c,v 1.5 1999/05/12 09:43:06 semenu Exp
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ntfs_vnops.c,v 1.36.4.1 2001/11/12 21:19:40 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/dirent.h>

#if !defined(__NetBSD__)
#include <vm/vm.h>
#endif

#if defined(__FreeBSD__)
#include <vm/vnode_pager.h>
#endif

#include <sys/sysctl.h>


/*#define NTFS_DEBUG 1*/
#include <ntfs/ntfs.h>
#include <ntfs/ntfs_inode.h>
#include <ntfs/ntfs_subr.h>
#include <miscfs/specfs/specdev.h>
#include <miscfs/genfs/genfs.h>

#include <sys/unistd.h> /* for pathconf(2) constants */

static int	ntfs_bypass __P((struct vop_generic_args *ap));
static int	ntfs_read __P((struct vop_read_args *));
static int	ntfs_write __P((struct vop_write_args *ap));
static int	ntfs_getattr __P((struct vop_getattr_args *ap));
static int	ntfs_inactive __P((struct vop_inactive_args *ap));
static int	ntfs_print __P((struct vop_print_args *ap));
static int	ntfs_reclaim __P((struct vop_reclaim_args *ap));
static int	ntfs_strategy __P((struct vop_strategy_args *ap));
static int	ntfs_access __P((struct vop_access_args *ap));
static int	ntfs_open __P((struct vop_open_args *ap));
static int	ntfs_close __P((struct vop_close_args *ap));
static int	ntfs_readdir __P((struct vop_readdir_args *ap));
static int	ntfs_lookup __P((struct vop_lookup_args *ap));
static int	ntfs_bmap __P((struct vop_bmap_args *ap));
#if defined(__FreeBSD__)
static int	ntfs_getpages __P((struct vop_getpages_args *ap));
static int	ntfs_putpages __P((struct vop_putpages_args *));
static int	ntfs_fsync __P((struct vop_fsync_args *ap));
#endif
static int	ntfs_pathconf __P((void *));

int	ntfs_prtactive = 1;	/* 1 => print out reclaim of active vnodes */

#if defined(__FreeBSD__)
int
ntfs_getpages(ap)
	struct vop_getpages_args *ap;
{
	return vnode_pager_generic_getpages(ap->a_vp, ap->a_m, ap->a_count,
		ap->a_reqpage);
}

int
ntfs_putpages(ap)
	struct vop_putpages_args *ap;
{
	return vnode_pager_generic_putpages(ap->a_vp, ap->a_m, ap->a_count,
		ap->a_sync, ap->a_rtvals);
}
#endif

/*
 * This is a noop, simply returning what one has been given.
 */
int
ntfs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{
	dprintf(("ntfs_bmap: vn: %p, blk: %d\n", ap->a_vp,(u_int32_t)ap->a_bn));
	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
#if !defined(__NetBSD__)
	if (ap->a_runb != NULL)
		*ap->a_runb = 0;
#endif
	return (0);
}

static int
ntfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	u_int64_t toread;
	int error;

	dprintf(("ntfs_read: ino: %d, off: %d resid: %d, segflg: %d\n",ip->i_number,(u_int32_t)uio->uio_offset,uio->uio_resid,uio->uio_segflg));

	dprintf(("ntfs_read: filesize: %d",(u_int32_t)fp->f_size));

	/* don't allow reading after end of file */
	if (uio->uio_offset > fp->f_size)
		toread = 0;
	else
		toread = min( uio->uio_resid, fp->f_size - uio->uio_offset );

	dprintf((", toread: %d\n",(u_int32_t)toread));

	if (toread == 0)
		return (0);

	error = ntfs_readattr(ntmp, ip, fp->f_attrtype,
		fp->f_attrname, uio->uio_offset, toread, NULL, uio);
	if (error) {
		printf("ntfs_read: ntfs_readattr failed: %d\n",error);
		return (error);
	}

	return (0);
}

static int
ntfs_bypass(ap)
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap;
{
	int error = ENOTTY;
	dprintf(("ntfs_bypass: %s\n", ap->a_desc->vdesc_name));
	return (error);
}


static int
ntfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct vattr *vap = ap->a_vap;

	dprintf(("ntfs_getattr: %d, flags: %d\n",ip->i_number,ip->i_flag));

#if defined(__FreeBSD__)
	vap->va_fsid = dev2udev(ip->i_dev);
#else /* NetBSD */
	vap->va_fsid = ip->i_dev;
#endif
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mp->ntm_mode;
	vap->va_nlink = ip->i_nlink;
	vap->va_uid = ip->i_mp->ntm_uid;
	vap->va_gid = ip->i_mp->ntm_gid;
	vap->va_rdev = 0;				/* XXX UNODEV ? */
	vap->va_size = fp->f_size;
	vap->va_bytes = fp->f_allocated;
	vap->va_atime = ntfs_nttimetounix(fp->f_times.t_access);
	vap->va_mtime = ntfs_nttimetounix(fp->f_times.t_write);
	vap->va_ctime = ntfs_nttimetounix(fp->f_times.t_create);
	vap->va_flags = ip->i_flag;
	vap->va_gen = 0;
	vap->va_blocksize = ip->i_mp->ntm_spc * ip->i_mp->ntm_bps;
	vap->va_type = vp->v_type;
	vap->va_filerev = 0;
	return (0);
}


/*
 * Last reference to an ntnode.  If necessary, write or delete it.
 */
int
ntfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
#ifdef NTFS_DEBUG
	struct ntnode *ip = VTONT(vp);
#endif

	dprintf(("ntfs_inactive: vnode: %p, ntnode: %d\n", vp, ip->i_number));

	if (ntfs_prtactive && vp->v_usecount != 0)
		vprint("ntfs_inactive: pushing active", vp);

	VOP__UNLOCK(vp, 0, ap->a_p);

	/* XXX since we don't support any filesystem changes
	 * right now, nothing more needs to be done
	 */
	return (0);
}

/*
 * Reclaim an fnode/ntnode so that it can be used for other purposes.
 */
int
ntfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	int error;

	dprintf(("ntfs_reclaim: vnode: %p, ntnode: %d\n", vp, ip->i_number));

	if (ntfs_prtactive && vp->v_usecount != 0)
		vprint("ntfs_reclaim: pushing active", vp);

	if ((error = ntfs_ntget(ip)) != 0)
		return (error);
	
	/* Purge old data structures associated with the inode. */
	cache_purge(vp);
	if (ip->i_devvp) {
		vrele(ip->i_devvp);
		ip->i_devvp = NULL;
	}

	ntfs_frele(fp);
	ntfs_ntput(ip);
	vp->v_data = NULL;

	return (0);
}

static int
ntfs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	return (0);
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
ntfs_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	struct buf *bp = ap->a_bp;
	struct vnode *vp = bp->b_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct ntfsmount *ntmp = ip->i_mp;
	int error;

#ifdef __FreeBSD__
	dprintf(("ntfs_strategy: offset: %d, blkno: %d, lblkno: %d\n",
		(u_int32_t)bp->b_offset,(u_int32_t)bp->b_blkno,
		(u_int32_t)bp->b_lblkno));
#else
	dprintf(("ntfs_strategy: blkno: %d, lblkno: %d\n",
		(u_int32_t)bp->b_blkno,
		(u_int32_t)bp->b_lblkno));
#endif

	dprintf(("strategy: bcount: %d flags: 0x%lx\n", 
		(u_int32_t)bp->b_bcount,bp->b_flags));

	if (bp->b_flags & B_READ) {
		u_int32_t toread;

		if (ntfs_cntob(bp->b_blkno) >= fp->f_size) {
			clrbuf(bp);
			error = 0;
		} else {
			toread = min(bp->b_bcount,
				 fp->f_size-ntfs_cntob(bp->b_blkno));
			dprintf(("ntfs_strategy: toread: %d, fsize: %d\n",
				toread,(u_int32_t)fp->f_size));

			error = ntfs_readattr(ntmp, ip, fp->f_attrtype,
				fp->f_attrname, ntfs_cntob(bp->b_blkno),
				toread, bp->b_data, NULL);

			if (error) {
				printf("ntfs_strategy: ntfs_readattr failed\n");
				bp->b_error = error;
				bp->b_flags |= B_ERROR;
			}

			bzero(bp->b_data + toread, bp->b_bcount - toread);
		}
	} else {
		size_t tmp;
		u_int32_t towrite;

		if (ntfs_cntob(bp->b_blkno) + bp->b_bcount >= fp->f_size) {
			printf("ntfs_strategy: CAN'T EXTEND FILE\n");
			bp->b_error = error = EFBIG;
			bp->b_flags |= B_ERROR;
		} else {
			towrite = min(bp->b_bcount,
				fp->f_size-ntfs_cntob(bp->b_blkno));
			dprintf(("ntfs_strategy: towrite: %d, fsize: %d\n",
				towrite,(u_int32_t)fp->f_size));

			error = ntfs_writeattr_plain(ntmp, ip, fp->f_attrtype,	
				fp->f_attrname, ntfs_cntob(bp->b_blkno),towrite,
				bp->b_data, &tmp, NULL);

			if (error) {
				printf("ntfs_strategy: ntfs_writeattr fail\n");
				bp->b_error = error;
				bp->b_flags |= B_ERROR;
			}
		}
	}
	biodone(bp);
	return (error);
}

static int
ntfs_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	u_int64_t towrite;
	size_t written;
	int error;

	dprintf(("ntfs_write: ino: %d, off: %d resid: %d, segflg: %d\n",ip->i_number,(u_int32_t)uio->uio_offset,uio->uio_resid,uio->uio_segflg));
	dprintf(("ntfs_write: filesize: %d",(u_int32_t)fp->f_size));

	if (uio->uio_resid + uio->uio_offset > fp->f_size) {
		printf("ntfs_write: CAN'T WRITE BEYOND END OF FILE\n");
		return (EFBIG);
	}

	towrite = min(uio->uio_resid, fp->f_size - uio->uio_offset);

	dprintf((", towrite: %d\n",(u_int32_t)towrite));

	error = ntfs_writeattr_plain(ntmp, ip, fp->f_attrtype,
		fp->f_attrname, uio->uio_offset, towrite, NULL, &written, uio);
#ifdef NTFS_DEBUG
	if (error)
		printf("ntfs_write: ntfs_writeattr failed: %d\n", error);
#endif

	return (error);
}

int
ntfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct ntnode *ip = VTONT(vp);
	struct ucred *cred = ap->a_cred;
	mode_t mask, mode = ap->a_mode;
	gid_t *gp;
	int i;
#ifdef QUOTA
	int error;
#endif

	dprintf(("ntfs_access: %d\n",ip->i_number));

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch ((int)vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
#ifdef QUOTA
			if (error = getinoquota(ip))
				return (error);
#endif
			break;
		}
	}

	/* Otherwise, user id 0 always gets access. */
	if (cred->cr_uid == 0)
		return (0);

	mask = 0;

	/* Otherwise, check the owner. */
	if (cred->cr_uid == ip->i_mp->ntm_uid) {
		if (mode & VEXEC)
			mask |= S_IXUSR;
		if (mode & VREAD)
			mask |= S_IRUSR;
		if (mode & VWRITE)
			mask |= S_IWUSR;
		return ((ip->i_mp->ntm_mode & mask) == mask ? 0 : EACCES);
	}

	/* Otherwise, check the groups. */
	for (i = 0, gp = cred->cr_groups; i < cred->cr_ngroups; i++, gp++)
		if (ip->i_mp->ntm_gid == *gp) {
			if (mode & VEXEC)
				mask |= S_IXGRP;
			if (mode & VREAD)
				mask |= S_IRGRP;
			if (mode & VWRITE)
				mask |= S_IWGRP;
			return ((ip->i_mp->ntm_mode&mask) == mask ? 0 : EACCES);
		}

	/* Otherwise, check everyone else. */
	if (mode & VEXEC)
		mask |= S_IXOTH;
	if (mode & VREAD)
		mask |= S_IROTH;
	if (mode & VWRITE)
		mask |= S_IWOTH;
	return ((ip->i_mp->ntm_mode & mask) == mask ? 0 : EACCES);
}

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
static int
ntfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
#if NTFS_DEBUG
	struct vnode *vp = ap->a_vp;
	struct ntnode *ip = VTONT(vp);

	printf("ntfs_open: %d\n",ip->i_number);
#endif

	/*
	 * Files marked append-only must be opened for appending.
	 */

	return (0);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
/* ARGSUSED */
static int
ntfs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
#if NTFS_DEBUG
	struct vnode *vp = ap->a_vp;
	struct ntnode *ip = VTONT(vp);

	printf("ntfs_close: %d\n",ip->i_number);
#endif

	return (0);
}

int
ntfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_ncookies;
		u_int **cookies;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct fnode *fp = VTOF(vp);
	struct ntnode *ip = FTONT(fp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	int i, error = 0;
	u_int32_t faked = 0, num;
	int ncookies = 0;
	struct dirent *cde;
	off_t off;

	dprintf(("ntfs_readdir %d off: %d resid: %d\n",ip->i_number,(u_int32_t)uio->uio_offset,uio->uio_resid));

	off = uio->uio_offset;

	MALLOC(cde, struct dirent *, sizeof(struct dirent), M_TEMP, M_WAITOK);

	/* Simulate . in every dir except ROOT */
	if (ip->i_number != NTFS_ROOTINO
	    && uio->uio_offset < sizeof(struct dirent)) {
		cde->d_fileno = ip->i_number;
		cde->d_reclen = sizeof(struct dirent);
		cde->d_type = DT_DIR;
		cde->d_namlen = 1;
		strncpy(cde->d_name, ".", 2);
		error = uiomove((void *)cde, sizeof(struct dirent), uio);
		if (error)
			goto out;

		ncookies++;
	}

	/* Simulate .. in every dir including ROOT */
	if (uio->uio_offset < 2 * sizeof(struct dirent)) {
		cde->d_fileno = NTFS_ROOTINO;	/* XXX */
		cde->d_reclen = sizeof(struct dirent);
		cde->d_type = DT_DIR;
		cde->d_namlen = 2;
		strncpy(cde->d_name, "..", 3);

		error = uiomove((void *) cde, sizeof(struct dirent), uio);
		if (error)
			goto out;

		ncookies++;
	}

	faked = (ip->i_number == NTFS_ROOTINO) ? 1 : 2;
	num = uio->uio_offset / sizeof(struct dirent) - faked;

	while (uio->uio_resid >= sizeof(struct dirent)) {
		struct attr_indexentry *iep;
		char *fname;
		size_t remains;
		int sz;

		error = ntfs_ntreaddir(ntmp, fp, num, &iep);
		if (error)
			goto out;

		if (NULL == iep)
			break;

		for(; !(iep->ie_flag & NTFS_IEFLAG_LAST) && (uio->uio_resid >= sizeof(struct dirent));
			iep = NTFS_NEXTREC(iep, struct attr_indexentry *))
		{
			if(!ntfs_isnamepermitted(ntmp,iep))
				continue;

			remains = sizeof(cde->d_name) - 1;
			fname = cde->d_name;
			for(i=0; i<iep->ie_fnamelen; i++) {
				sz = (*ntmp->ntm_wput)(fname, remains,
						iep->ie_fname[i]);
				fname += sz;
				remains -= sz;
			}
			*fname = '\0';
			dprintf(("ntfs_readdir: elem: %d, fname:[%s] type: %d, flag: %d, ",
				num, cde->d_name, iep->ie_fnametype,
				iep->ie_flag));
			cde->d_namlen = fname - (char *) cde->d_name;
			cde->d_fileno = iep->ie_number;
			cde->d_type = (iep->ie_fflag & NTFS_FFLAG_DIR) ? DT_DIR : DT_REG;
			cde->d_reclen = sizeof(struct dirent);
			dprintf(("%s\n", (cde->d_type == DT_DIR) ? "dir":"reg"));

			error = uiomove((void *)cde, sizeof(struct dirent), uio);
			if (error)
				goto out;

			ncookies++;
			num++;
		}
	}

	dprintf(("ntfs_readdir: %d entries (%d bytes) read\n",
		ncookies,(u_int)(uio->uio_offset - off)));
	dprintf(("ntfs_readdir: off: %d resid: %d\n",
		(u_int32_t)uio->uio_offset,uio->uio_resid));

	if (!error && ap->a_ncookies != NULL) {
		struct dirent* dpStart;
		struct dirent* dp;
#if defined(__FreeBSD__)
		u_long *cookies;
		u_long *cookiep;
#else /* defined(__NetBSD__) */
		off_t *cookies;
		off_t *cookiep;
#endif

		printf("ntfs_readdir: %d cookies\n",ncookies);
		if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
			panic("ntfs_readdir: unexpected uio from NFS server");
		dpStart = (struct dirent *)
		     ((caddr_t)uio->uio_iov->iov_base -
			 (uio->uio_offset - off));
#if defined(__FreeBSD__)
		MALLOC(cookies, u_long *, ncookies * sizeof(u_long),
		       M_TEMP, M_WAITOK);
#else /* defined(__NetBSD__) */
		cookies = malloc(ncookies * sizeof(off_t), M_TEMP, M_WAITOK);
#endif
		for (dp = dpStart, cookiep = cookies, i=0;
		     i < ncookies;
		     dp = (struct dirent *)((caddr_t) dp + dp->d_reclen), i++) {
			off += dp->d_reclen;
			*cookiep++ = (u_int) off;
		}
		*ap->a_ncookies = ncookies;
		*ap->a_cookies = cookies;
	}
/*
	if (ap->a_eofflag)
	    *ap->a_eofflag = VTONT(ap->a_vp)->i_size <= uio->uio_offset;
*/
    out:
	FREE(cde, M_TEMP);
	return (error);
}

int
ntfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *dvp = ap->a_dvp;
	struct ntnode *dip = VTONT(dvp);
	struct ntfsmount *ntmp = dip->i_mp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int error;
	int lockparent = cnp->cn_flags & LOCKPARENT;
#if NTFS_DEBUG
	int wantparent = cnp->cn_flags & (LOCKPARENT|WANTPARENT);
#endif
	dprintf(("ntfs_lookup: \"%.*s\" (%ld bytes) in %d, lp: %d, wp: %d \n",
		(int)cnp->cn_namelen, cnp->cn_nameptr, cnp->cn_namelen,
		dip->i_number, lockparent, wantparent));

	error = VOP_ACCESS(dvp, VEXEC, cred, cnp->cn_proc);
	if(error)
		return (error);

	if ((cnp->cn_flags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

#ifdef __NetBSD__
	/*
	 * We now have a segment name to search for, and a directory
	 * to search.
	 *
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 */
	if ((error = cache_lookup(ap->a_dvp, ap->a_vpp, cnp)) >= 0)
		return (error);
#endif

	if(cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		dprintf(("ntfs_lookup: faking . directory in %d\n",
			dip->i_number));

		VREF(dvp);
		*ap->a_vpp = dvp;
		error = 0;
	} else if (cnp->cn_flags & ISDOTDOT) {
		struct ntvattr *vap;

		dprintf(("ntfs_lookup: faking .. directory in %d\n",
			 dip->i_number));

		error = ntfs_ntvattrget(ntmp, dip, NTFS_A_NAME, NULL, 0, &vap);
		if(error)
			return (error);

		VOP__UNLOCK(dvp,0,cnp->cn_proc);
		cnp->cn_flags |= PDIRUNLOCK;

		dprintf(("ntfs_lookup: parentdir: %d\n",
			 vap->va_a_name->n_pnumber));
		error = VFS_VGET(ntmp->ntm_mountp,
				 vap->va_a_name->n_pnumber,ap->a_vpp); 
		ntfs_ntvattrrele(vap);
		if (error) {
			if (VN_LOCK(dvp,LK_EXCLUSIVE|LK_RETRY,cnp->cn_proc)==0)
				cnp->cn_flags &= ~PDIRUNLOCK;
			return (error);
		}

		if (lockparent && (cnp->cn_flags & ISLASTCN)) {
			error = VN_LOCK(dvp, LK_EXCLUSIVE, cnp->cn_proc);
			if (error) {
				vput( *(ap->a_vpp) );
				return (error);
			}
			cnp->cn_flags &= ~PDIRUNLOCK;
		}
	} else {
		error = ntfs_ntlookupfile(ntmp, dvp, cnp, ap->a_vpp);
		if (error) {
			dprintf(("ntfs_ntlookupfile: returned %d\n", error));
			return (error);
		}

		dprintf(("ntfs_lookup: found ino: %d\n", 
			VTONT(*ap->a_vpp)->i_number));

		if(!lockparent || !(cnp->cn_flags & ISLASTCN))
			VOP__UNLOCK(dvp, 0, cnp->cn_proc);
	}

	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(dvp, *ap->a_vpp, cnp);

	return (error);
}

#if defined(__FreeBSD__)
/*
 * Flush the blocks of a file to disk.
 *
 * This function is worthless for vnodes that represent directories. Maybe we
 * could just do a sync if they try an fsync on a directory file.
 */
static int
ntfs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		off_t offlo;
		off_t offhi;
		struct proc *a_p;
	} */ *ap;
{
	return (0);
}
#endif

/*
 * Return POSIX pathconf information applicable to NTFS filesystem
 */
int
ntfs_pathconf(v)
	void *v;
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 1;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = NTFS_MAXFILENAME;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 0;
		return (0);
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		return (0);
	case _PC_FILESIZEBITS:
		*ap->a_retval = 64;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Global vfs data structures
 */
vop_t **ntfs_vnodeop_p;
#if defined(__FreeBSD__)
static
struct vnodeopv_entry_desc ntfs_vnodeop_entries[] = {
	{ &vop_default_desc, (vop_t *)ntfs_bypass },

	{ &vop_getattr_desc, (vop_t *)ntfs_getattr },
	{ &vop_inactive_desc, (vop_t *)ntfs_inactive },
	{ &vop_reclaim_desc, (vop_t *)ntfs_reclaim },
	{ &vop_print_desc, (vop_t *)ntfs_print },
	{ &vop_pathconf_desc, ntfs_pathconf },

	{ &vop_islocked_desc, (vop_t *)vop_stdislocked },
	{ &vop_unlock_desc, (vop_t *)vop_stdunlock },
	{ &vop_lock_desc, (vop_t *)vop_stdlock },
	{ &vop_cachedlookup_desc, (vop_t *)ntfs_lookup },
	{ &vop_lookup_desc, (vop_t *)vfs_cache_lookup },

	{ &vop_access_desc, (vop_t *)ntfs_access },
	{ &vop_close_desc, (vop_t *)ntfs_close },
	{ &vop_open_desc, (vop_t *)ntfs_open },
	{ &vop_readdir_desc, (vop_t *)ntfs_readdir },
	{ &vop_fsync_desc, (vop_t *)ntfs_fsync },

	{ &vop_bmap_desc, (vop_t *)ntfs_bmap },
	{ &vop_getpages_desc, (vop_t *) ntfs_getpages },
	{ &vop_putpages_desc, (vop_t *) ntfs_putpages },
	{ &vop_strategy_desc, (vop_t *)ntfs_strategy },
	{ &vop_bwrite_desc, (vop_t *)vop_stdbwrite },
	{ &vop_read_desc, (vop_t *)ntfs_read },
	{ &vop_write_desc, (vop_t *)ntfs_write },

	{ NULL, NULL }
};

static
struct vnodeopv_desc ntfs_vnodeop_opv_desc =
	{ &ntfs_vnodeop_p, ntfs_vnodeop_entries };

VNODEOP_SET(ntfs_vnodeop_opv_desc);

#else /* !FreeBSD */

const struct vnodeopv_entry_desc ntfs_vnodeop_entries[] = {
	{ &vop_default_desc, (vop_t *) ntfs_bypass },
	{ &vop_lookup_desc, (vop_t *) ntfs_lookup },	/* lookup */
	{ &vop_create_desc, genfs_eopnotsupp },		/* create */
	{ &vop_mknod_desc, genfs_eopnotsupp },		/* mknod */
	{ &vop_open_desc, (vop_t *) ntfs_open },	/* open */
	{ &vop_close_desc,(vop_t *)  ntfs_close },	/* close */
	{ &vop_access_desc, (vop_t *) ntfs_access },	/* access */
	{ &vop_getattr_desc, (vop_t *) ntfs_getattr },	/* getattr */
	{ &vop_setattr_desc, genfs_eopnotsupp },	/* setattr */
	{ &vop_read_desc, (vop_t *) ntfs_read },	/* read */
	{ &vop_write_desc, (vop_t *) ntfs_write },	/* write */
	{ &vop_lease_desc, genfs_lease_check },		/* lease */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, genfs_enoioctl },		/* ioctl */
	{ &vop_poll_desc, genfs_poll },			/* poll */
	{ &vop_revoke_desc, genfs_revoke },		/* revoke */
	{ &vop_mmap_desc, genfs_eopnotsupp },		/* mmap */
	{ &vop_fsync_desc, genfs_fsync },		/* fsync */
	{ &vop_seek_desc, genfs_seek },			/* seek */
	{ &vop_remove_desc, genfs_eopnotsupp },		/* remove */
	{ &vop_link_desc, genfs_eopnotsupp },		/* link */
	{ &vop_rename_desc, genfs_eopnotsupp },		/* rename */
	{ &vop_mkdir_desc, genfs_eopnotsupp },		/* mkdir */
	{ &vop_rmdir_desc, genfs_eopnotsupp },		/* rmdir */
	{ &vop_symlink_desc, genfs_eopnotsupp },	/* symlink */
	{ &vop_readdir_desc, (vop_t *) ntfs_readdir },	/* readdir */
	{ &vop_readlink_desc, genfs_eopnotsupp },	/* readlink */
	{ &vop_abortop_desc, genfs_abortop },		/* abortop */
	{ &vop_inactive_desc, (vop_t *) ntfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, (vop_t *) ntfs_reclaim },	/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_bmap_desc, (vop_t *) ntfs_bmap },	/* bmap */
	{ &vop_strategy_desc, (vop_t *) ntfs_strategy },	/* strategy */
	{ &vop_print_desc, (vop_t *) ntfs_print },	/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, ntfs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, genfs_nullop },		/* advlock */
	{ &vop_blkatoff_desc, genfs_eopnotsupp },	/* blkatoff */
	{ &vop_valloc_desc, genfs_eopnotsupp },		/* valloc */
	{ &vop_reallocblks_desc, genfs_eopnotsupp },	/* reallocblks */
	{ &vop_vfree_desc, genfs_eopnotsupp },		/* vfree */
	{ &vop_truncate_desc, genfs_eopnotsupp },	/* truncate */
	{ &vop_update_desc, genfs_eopnotsupp },		/* update */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ (struct vnodeop_desc *)NULL, (int (*) __P((void *)))NULL }
};
const struct vnodeopv_desc ntfs_vnodeop_opv_desc =
	{ &ntfs_vnodeop_p, ntfs_vnodeop_entries };

#endif
