/*	$NetBSD: cd9660_vnops.c,v 1.5.2.4 1994/08/03 06:16:26 cgd Exp $	*/

/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	@(#)cd9660_vnops.c	8.3 (Berkeley) 1/23/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>
#include <sys/malloc.h>
#include <sys/dir.h>

#include <isofs/cd9660/iso.h>
#include <isofs/cd9660/cd9660_node.h>
#include <isofs/cd9660/iso_rrip.h>

#if 0
/*
 * Mknod vnode call
 *  Actually remap the device number
 */
cd9660_mknod(ndp, vap, cred, p)
	struct nameidata *ndp;
	struct ucred *cred;
	struct vattr *vap;
	struct proc *p;
{
#ifndef	ISODEVMAP
	free(ndp->ni_pnbuf, M_NAMEI);
	vput(ndp->ni_dvp);
	vput(ndp->ni_vp);
	return (EINVAL);
#else
	register struct vnode *vp;
	struct iso_node *ip;
	struct iso_dnode *dp;
	int error;

	vp = ndp->ni_vp;
	ip = VTOI(vp);

	if (ip->i_mnt->iso_ftype != ISO_FTYPE_RRIP
	    || vap->va_type != vp->v_type
	    || (vap->va_type != VCHR && vap->va_type != VBLK)) {
		free(ndp->ni_pnbuf, M_NAMEI);
		vput(ndp->ni_dvp);
		vput(ndp->ni_vp);
		return (EINVAL);
	}

	dp = iso_dmap(ip->i_dev,ip->i_number,1);
	if (ip->inode.iso_rdev == vap->va_rdev || vap->va_rdev == VNOVAL) {
		/* same as the unmapped one, delete the mapping */
		remque(dp);
		FREE(dp,M_CACHE);
	} else
		/* enter new mapping */
		dp->d_dev = vap->va_rdev;

	/*
	 * Remove inode so that it will be reloaded by iget and
	 * checked to see if it is an alias of an existing entry
	 * in the inode cache.
	 */
	vput(vp);
	vp->v_type = VNON;
	vgone(vp);
	return (0);
#endif
}
#endif

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
int
cd9660_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	return (0);
}

/*
 * Close called
 *
 * Update the times on the inode on writeable file systems.
 */
/* ARGSUSED */
int
cd9660_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	return (0);
}

/*
 * Check mode permission on inode pointer. Mode is READ, WRITE or EXEC.
 * The mode is shifted to select the owner/group/other fields. The
 * super user is granted all permissions.
 */
/* ARGSUSED */
cd9660_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct iso_node *ip = VTOI(ap->a_vp);
	struct ucred *cred = ap->a_cred;
	mode_t mask, mode = ap->a_mode;
	gid_t *gp;
	int i;

	/* User id 0 always gets access. */
	if (cred->cr_uid == 0)
		return (0);

	mask = 0;

	/* Otherwise, check the owner. */
	if (cred->cr_uid == ip->inode.iso_uid) {
		if (mode & VEXEC)
			mask |= S_IXUSR;
		if (mode & VREAD)
			mask |= S_IRUSR;
		if (mode & VWRITE)
			mask |= S_IWUSR;
		return ((ip->inode.iso_mode & mask) == mask ? 0 : EACCES);
	}

	/* Otherwise, check the groups. */
	for (i = 0, gp = cred->cr_groups; i < cred->cr_ngroups; i++, gp++)
		if (ip->inode.iso_gid == *gp) {
			if (mode & VEXEC)
				mask |= S_IXGRP;
			if (mode & VREAD)
				mask |= S_IRGRP;
			if (mode & VWRITE)
				mask |= S_IWGRP;
			return ((ip->inode.iso_mode & mask) == mask ?
			    0 : EACCES);
		}

	/* Otherwise, check everyone else. */
	if (mode & VEXEC)
		mask |= S_IXOTH;
	if (mode & VREAD)
		mask |= S_IROTH;
	if (mode & VWRITE)
		mask |= S_IWOTH;
	return ((ip->inode.iso_mode & mask) == mask ? 0 : EACCES);
}

cd9660_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;

{
	struct vnode *vp = ap->a_vp;
	register struct vattr *vap = ap->a_vap;
	register struct iso_node *ip = VTOI(vp);
	int i;

	vap->va_fsid	= ip->i_dev;
	vap->va_fileid	= ip->i_number;

	vap->va_mode	= ip->inode.iso_mode;
	vap->va_nlink	= ip->inode.iso_links;
	vap->va_uid	= ip->inode.iso_uid;
	vap->va_gid	= ip->inode.iso_gid;
	vap->va_atime	= ip->inode.iso_atime;
	vap->va_mtime	= ip->inode.iso_mtime;
	vap->va_ctime	= ip->inode.iso_ctime;
	vap->va_rdev	= ip->inode.iso_rdev;

	vap->va_size	= (u_quad_t) ip->i_size;
	vap->va_flags	= 0;
	vap->va_gen = 1;
	vap->va_blocksize = ip->i_mnt->logical_block_size;
	vap->va_bytes	= (u_quad_t) ip->i_size;
	vap->va_type	= vp->v_type;
	return (0);
}

#if ISO_DEFAULT_BLOCK_SIZE >= NBPG
#ifdef DEBUG
extern int doclusterread;
#else
#define doclusterread 1
#endif
#else
/* XXX until cluster routines can handle block sizes less than one page */
#define doclusterread 0
#endif

/*
 * Vnode op for reading.
 */
cd9660_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	register struct uio *uio = ap->a_uio;
	register struct iso_node *ip = VTOI(vp);
	register struct iso_mnt *imp;
	struct buf *bp;
	daddr_t lbn, bn, rablock;
	off_t diff;
	int rasize, error = 0;
	long size, n, on;

	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)
		return (EINVAL);
	ip->i_flag |= IN_ACCESS;
	imp = ip->i_mnt;
	do {
		lbn = lblkno(imp, uio->uio_offset);
		on = blkoff(imp, uio->uio_offset);
		n = min((u_int)(imp->logical_block_size - on),
			uio->uio_resid);
		diff = (off_t)ip->i_size - uio->uio_offset;
		if (diff <= 0)
			return (0);
		if (diff < n)
			n = diff;
		size = blksize(imp, ip, lbn);
		rablock = lbn + 1;
		if (doclusterread) {
			if (lblktosize(imp, rablock) <= ip->i_size)
				error = cluster_read(vp, (off_t)ip->i_size,
						     lbn, size, NOCRED, &bp);
			else
				error = bread(vp, lbn, size, NOCRED, &bp);
		} else {
			if (vp->v_lastr + 1 == lbn &&
			    lblktosize(imp, rablock) < ip->i_size) {
				rasize = blksize(imp, ip, rablock);
				error = breadn(vp, lbn, size, &rablock,
					       &rasize, 1, NOCRED, &bp);
			} else
				error = bread(vp, lbn, size, NOCRED, &bp);
		}
		vp->v_lastr = lbn;
		n = min(n, size - bp->b_resid);
		if (error) {
			brelse(bp);
			return (error);
		}

		error = uiomove(bp->b_data + on, (int)n, uio);
		if (n + on == imp->logical_block_size ||
		    uio->uio_offset == (off_t)ip->i_size)
			bp->b_flags |= B_AGE;
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n != 0);
	return (error);
}

/* ARGSUSED */
int
cd9660_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	printf("You did ioctl for isofs !!\n");
	return (ENOTTY);
}

/* ARGSUSED */
int
cd9660_select(ap)
	struct vop_select_args /* {
		struct vnode *a_vp;
		int  a_which;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	/*
	 * We should really check to see if I/O is possible.
	 */
	return (1);
}

/*
 * Mmap a file
 *
 * NB Currently unsupported.
 */
/* ARGSUSED */
int
cd9660_mmap(ap)
	struct vop_mmap_args /* {
		struct vnode *a_vp;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	return (EINVAL);
}

/*
 * Seek on a file
 *
 * Nothing to do, so just return.
 */
/* ARGSUSED */
int
cd9660_seek(ap)
	struct vop_seek_args /* {
		struct vnode *a_vp;
		off_t  a_oldoff;
		off_t  a_newoff;
		struct ucred *a_cred;
	} */ *ap;
{

	return (0);
}

/*
 * Structure for reading directories
 */
struct isoreaddir {
	struct dirent saveent;
	struct dirent assocent;
	struct dirent current;
	off_t saveoff;
	off_t assocoff;
	off_t curroff;
	struct uio *uio;
	off_t uio_off;
	int eofflag;
	u_long *cookies;
	int ncookies;
};

int
iso_uiodir(idp,dp,off)
	struct isoreaddir *idp;
	struct dirent *dp;
	off_t off;
{
	int error;

	dp->d_name[dp->d_namlen] = 0;
	dp->d_reclen = DIRSIZ(dp);

	if (idp->uio->uio_resid < dp->d_reclen) {
		idp->eofflag = 0;
		return (-1);
	}

	if (idp->cookies) {
		if (idp->ncookies <= 0) {
			idp->eofflag = 0;
			return (-1);
		}

		*idp->cookies++ = off;
		--idp->ncookies;
	}

	if (error = uiomove(dp,dp->d_reclen,idp->uio))
		return (error);
	idp->uio_off = off;
	return (0);
}

int
iso_shipdir(idp)
	struct isoreaddir *idp;
{
	struct dirent *dp;
	int cl, sl, assoc;
	int error;
	char *cname, *sname;

	cl = idp->current.d_namlen;
	cname = idp->current.d_name;
	if (assoc = cl > 1 && *cname == ASSOCCHAR) {
		cl--;
		cname++;
	}

	dp = &idp->saveent;
	sname = dp->d_name;
	if (!(sl = dp->d_namlen)) {
		dp = &idp->assocent;
		sname = dp->d_name + 1;
		sl = dp->d_namlen - 1;
	}
	if (sl > 0) {
		if (sl != cl
		    || bcmp(sname,cname,sl)) {
			if (idp->assocent.d_namlen) {
				if (error = iso_uiodir(idp,&idp->assocent,idp->assocoff))
					return (error);
				idp->assocent.d_namlen = 0;
			}
			if (idp->saveent.d_namlen) {
				if (error = iso_uiodir(idp,&idp->saveent,idp->saveoff))
					return (error);
				idp->saveent.d_namlen = 0;
			}
		}
	}
	idp->current.d_reclen = DIRSIZ(&idp->current);
	if (assoc) {
		idp->assocoff = idp->curroff;
		bcopy(&idp->current,&idp->assocent,idp->current.d_reclen);
	} else {
		idp->saveoff = idp->curroff;
		bcopy(&idp->current,&idp->saveent,idp->current.d_reclen);
	}
	return (0);
}

/*
 * Vnode op for readdir
 */
int
cd9660_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
	} */ *ap;
{
	register struct uio *uio = ap->a_uio;
	struct isoreaddir *idp;
	struct vnode *vdp = ap->a_vp;
	struct iso_node *dp;
	struct iso_mnt *imp;
	struct buf *bp = NULL;
	struct iso_directory_record *ep;
	int entryoffsetinblock;
	doff_t endsearch;
	u_long bmask;
	int error = 0;
	int reclen;
	u_short namelen;

	dp = VTOI(vdp);
	imp = dp->i_mnt;
	bmask = imp->im_bmask;

	MALLOC(idp, struct isoreaddir *, sizeof(*idp), M_TEMP, M_WAITOK);
	idp->saveent.d_namlen = idp->assocent.d_namlen = 0;
	/*
	 * XXX
	 * Is it worth trying to figure out the type?
	 */
	idp->saveent.d_type = idp->assocent.d_type = idp->current.d_type =
	    DT_UNKNOWN;
	idp->uio = uio;
	idp->eofflag = 1;
	idp->cookies = ap->a_cookies;
	idp->ncookies = ap->a_ncookies;
	idp->curroff = uio->uio_offset;

	if ((entryoffsetinblock = idp->curroff & bmask) &&
	    (error = VOP_BLKATOFF(vdp, (off_t)idp->curroff, NULL, &bp))) {
		FREE(idp, M_TEMP);
		return (error);
	}
	endsearch = dp->i_size;

	while (idp->curroff < endsearch) {
		/*
		 * If offset is on a block boundary,
		 * read the next directory block.
		 * Release previous if it exists.
		 */
		if ((idp->curroff & bmask) == 0) {
			if (bp != NULL)
				brelse(bp);
			if (error =
			    VOP_BLKATOFF(vdp, (off_t)idp->curroff, NULL, &bp))
				break;
			entryoffsetinblock = 0;
		}
		/*
		 * Get pointer to next entry.
		 */
		ep = (struct iso_directory_record *)
			((char *)bp->b_data + entryoffsetinblock);

		reclen = isonum_711(ep->length);
		if (reclen == 0) {
			/* skip to next block, if any */
			idp->curroff =
			    (idp->curroff & ~bmask) + imp->logical_block_size;
			continue;
		}

		if (reclen < ISO_DIRECTORY_RECORD_SIZE) {
			error = EINVAL;
			/* illegal entry, stop */
			break;
		}

		if (entryoffsetinblock + reclen > imp->logical_block_size) {
			error = EINVAL;
			/* illegal directory, so stop looking */
			break;
		}

		idp->current.d_namlen = isonum_711(ep->name_len);

		if (reclen < ISO_DIRECTORY_RECORD_SIZE + idp->current.d_namlen) {
			error = EINVAL;
			/* illegal entry, stop */
			break;
		}

		if (isonum_711(ep->flags)&2)
			idp->current.d_fileno = isodirino(ep, imp);
		else
			idp->current.d_fileno = dbtob(bp->b_blkno) +
				entryoffsetinblock;

		idp->curroff += reclen;

		switch (imp->iso_ftype) {
		case ISO_FTYPE_RRIP:
			cd9660_rrip_getname(ep,idp->current.d_name, &namelen,
					   &idp->current.d_fileno,imp);
			idp->current.d_namlen = (u_char)namelen;
			if (idp->current.d_namlen)
				error = iso_uiodir(idp,&idp->current,idp->curroff);
			break;
		default:	/* ISO_FTYPE_DEFAULT || ISO_FTYPE_9660 */
			strcpy(idp->current.d_name,"..");
			switch (ep->name[0]) {
			case 0:
				idp->current.d_namlen = 1;
				error = iso_uiodir(idp,&idp->current,idp->curroff);
				break;
			case 1:
				idp->current.d_namlen = 2;
				error = iso_uiodir(idp,&idp->current,idp->curroff);
				break;
			default:
				isofntrans(ep->name,idp->current.d_namlen,
					   idp->current.d_name, &namelen,
					   imp->iso_ftype == ISO_FTYPE_9660,
					   isonum_711(ep->flags)&4);
				idp->current.d_namlen = (u_char)namelen;
				if (imp->iso_ftype == ISO_FTYPE_DEFAULT)
					error = iso_shipdir(idp);
				else
					error = iso_uiodir(idp,&idp->current,idp->curroff);
				break;
			}
		}
		if (error)
			break;

		entryoffsetinblock += reclen;
	}

	if (!error && imp->iso_ftype == ISO_FTYPE_DEFAULT) {
		idp->current.d_namlen = 0;
		error = iso_shipdir(idp);
	}
	if (error < 0)
		error = 0;

	if (bp)
		brelse (bp);

	uio->uio_offset = idp->uio_off;
	*ap->a_eofflag = idp->eofflag;

	FREE(idp, M_TEMP);

	return (error);
}

/*
 * Return target name of a symbolic link
 * Shouldn't we get the parent vnode and read the data from there?
 * This could eventually result in deadlocks in cd9660_lookup.
 * But otherwise the block read here is in the block buffer two times.
 */
typedef struct iso_directory_record ISODIR;
typedef struct iso_node             ISONODE;
typedef struct iso_mnt              ISOMNT;
int
cd9660_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	ISONODE	*ip;
	ISODIR	*dirp;
	ISOMNT	*imp;
	struct	buf *bp;
	u_short	symlen;
	int	error;
	char	*symname;
	ino_t	ino;

	ip  = VTOI(ap->a_vp);
	imp = ip->i_mnt;

	if (imp->iso_ftype != ISO_FTYPE_RRIP)
		return (EINVAL);

	/*
	 * Get parents directory record block that this inode included.
	 */
	error = bread(imp->im_devvp,
		      (ip->i_number >> imp->im_bshift) <<
		      (imp->im_bshift - DEV_BSHIFT),
		      imp->logical_block_size, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (EINVAL);
	}

	/*
	 * Setup the directory pointer for this inode
	 */
	dirp = (ISODIR *)(bp->b_data + (ip->i_number & imp->im_bmask));
#ifdef DEBUG
	printf("lbn=%d,off=%d,bsize=%d,DEV_BSIZE=%d, dirp= %08x, b_addr=%08x, offset=%08x(%08x)\n",
	       (daddr_t)(ip->i_number >> imp->im_bshift),
	       ip->i_number & imp->im_bmask,
	       imp->logical_block_size,
	       DEV_BSIZE,
	       dirp,
	       bp->b_data,
	       ip->i_number,
	       ip->i_number & imp->im_bmask );
#endif

	/*
	 * Just make sure, we have a right one....
	 *   1: Check not cross boundary on block
	 */
	if ((ip->i_number & imp->im_bmask) + isonum_711(dirp->length)
	    > imp->logical_block_size) {
		brelse(bp);
		return (EINVAL);
	}

	/*
	 * Now get a buffer
	 * Abuse a namei buffer for now.
	 */
	MALLOC(symname,char *,MAXPATHLEN,M_NAMEI,M_WAITOK);

	/*
	 * Ok, we just gathering a symbolic name in SL record.
	 */
	if (cd9660_rrip_getsymname(dirp,symname,&symlen,imp) == 0) {
		FREE(symname,M_NAMEI);
		brelse(bp);
		return (EINVAL);
	}
	/*
	 * Don't forget before you leave from home ;-)
	 */
	brelse(bp);

	/*
	 * return with the symbolic name to caller's.
	 */
	error = uiomove(symname,symlen,ap->a_uio);

	FREE(symname,M_NAMEI);

	return (error);
}

/*
 * Ufs abort op, called after namei() when a CREATE/DELETE isn't actually
 * done. If a buffer has been saved in anticipation of a CREATE, delete it.
 */
int
cd9660_abortop(ap)
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap;
{
	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		FREE(ap->a_cnp->cn_pnbuf, M_NAMEI);
	return (0);
}

/*
 * Lock an inode.
 */
int
cd9660_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct iso_node *ip;
	struct proc *p = curproc;	/* XXX */

start:
	while (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		sleep((caddr_t)vp, PINOD);
	}
	if (vp->v_tag == VT_NON)
		return (ENOENT);
	ip = VTOI(vp);
	if (ip->i_flag & IN_LOCKED) {
		ip->i_flag |= IN_WANTED;
#ifdef DIAGNOSTIC
		if (p) {
			if (p->p_pid == ip->i_lockholder)
				panic("locking against myself");
			ip->i_lockwaiter = p->p_pid;
		} else
			ip->i_lockwaiter = -1;
#endif
		(void) sleep((caddr_t)ip, PINOD);
	}
#ifdef DIAGNOSTIC
	ip->i_lockwaiter = 0;
	if (ip->i_lockholder != 0)
		panic("lockholder (%d) != 0", ip->i_lockholder);
	if (p && p->p_pid == 0)
		printf("locking by process 0\n");
	if (p)
		ip->i_lockholder = p->p_pid;
	else
		ip->i_lockholder = -1;
#endif
	ip->i_flag |= IN_LOCKED;
	return (0);
}

/*
 * Unlock an inode.
 */
int
cd9660_unlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct iso_node *ip = VTOI(ap->a_vp);
	struct proc *p = curproc;	/* XXX */

#ifdef DIAGNOSTIC
	if ((ip->i_flag & IN_LOCKED) == 0) {
		vprint("ufs_unlock: unlocked inode", ap->a_vp);
		panic("ufs_unlock NOT LOCKED");
	}
	if (p && p->p_pid != ip->i_lockholder && p->p_pid > -1 &&
	    ip->i_lockholder > -1/* && lockcount++ < 100*/)
		panic("unlocker (%d) != lock holder (%d)",
		    p->p_pid, ip->i_lockholder);
	ip->i_lockholder = 0;
#endif
	ip->i_flag &= ~IN_LOCKED;
	if (ip->i_flag & IN_WANTED) {
		ip->i_flag &= ~IN_WANTED;
		wakeup((caddr_t)ip);
	}
	return (0);
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
cd9660_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	register struct buf *bp = ap->a_bp;
	register struct vnode *vp = bp->b_vp;
	register struct iso_node *ip;
	int error;

	ip = VTOI(vp);
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("cd9660_strategy: spec");
	if (bp->b_blkno == bp->b_lblkno) {
		if (error =
		    VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL)) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			biodone(bp);
			return (error);
		}
		if ((long)bp->b_blkno == -1)
			clrbuf(bp);
	}
	if ((long)bp->b_blkno == -1) {
		biodone(bp);
		return (0);
	}
	vp = ip->i_devvp;
	bp->b_dev = vp->v_rdev;
	VOCALL (vp->v_op, VOFFSET(vop_strategy), ap);
	return (0);
}

/*
 * Print out the contents of an inode.
 */
int
cd9660_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	printf("tag VT_ISOFS, isofs vnode\n");
	return (0);
}

/*
 * Check for a locked inode.
 */
int
cd9660_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	if (VTOI(ap->a_vp)->i_flag & IN_LOCKED)
		return (1);
	return (0);
}

/*
 * Return POSIX pathconf information applicable to cd9660 filesystems.
 */
int
cd9660_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 1;
		return (0);
	case _PC_NAME_MAX:
		if (VTOI(ap->a_vp)->i_mnt->iso_ftype == ISO_FTYPE_RRIP)
			*ap->a_retval = NAME_MAX;
		else
			*ap->a_retval = 37;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Unsupported operation
 */
int
cd9660_enotsupp()
{

	return (EOPNOTSUPP);
}

/*
 * Global vfs data structures for isofs
 */
#define cd9660_create \
	((int (*) __P((struct  vop_create_args *)))cd9660_enotsupp)
#define cd9660_mknod ((int (*) __P((struct  vop_mknod_args *)))cd9660_enotsupp)
#define cd9660_setattr \
	((int (*) __P((struct  vop_setattr_args *)))cd9660_enotsupp)
#define cd9660_write ((int (*) __P((struct  vop_write_args *)))cd9660_enotsupp)
#define cd9660_fsync ((int (*) __P((struct  vop_fsync_args *)))nullop)
#define cd9660_remove \
	((int (*) __P((struct  vop_remove_args *)))cd9660_enotsupp)
#define cd9660_link ((int (*) __P((struct  vop_link_args *)))cd9660_enotsupp)
#define cd9660_rename \
	((int (*) __P((struct  vop_rename_args *)))cd9660_enotsupp)
#define cd9660_mkdir ((int (*) __P((struct  vop_mkdir_args *)))cd9660_enotsupp)
#define cd9660_rmdir ((int (*) __P((struct  vop_rmdir_args *)))cd9660_enotsupp)
#define cd9660_symlink \
	((int (*) __P((struct vop_symlink_args *)))cd9660_enotsupp)
#define cd9660_advlock \
	((int (*) __P((struct vop_advlock_args *)))cd9660_enotsupp)
#define cd9660_valloc ((int(*) __P(( \
		struct vnode *pvp, \
		int mode, \
		struct ucred *cred, \
		struct vnode **vpp))) cd9660_enotsupp)
#define cd9660_vfree ((int (*) __P((struct  vop_vfree_args *)))cd9660_enotsupp)
#define cd9660_truncate \
	((int (*) __P((struct  vop_truncate_args *)))cd9660_enotsupp)
#define cd9660_update \
	((int (*) __P((struct  vop_update_args *)))cd9660_enotsupp)
#define cd9660_bwrite \
	((int (*) __P((struct  vop_bwrite_args *)))cd9660_enotsupp)

/*
 * Global vfs data structures for nfs
 */
int (**cd9660_vnodeop_p)();
struct vnodeopv_entry_desc cd9660_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, cd9660_lookup },	/* lookup */
	{ &vop_create_desc, cd9660_create },	/* create */
	{ &vop_mknod_desc, cd9660_mknod },	/* mknod */
	{ &vop_open_desc, cd9660_open },	/* open */
	{ &vop_close_desc, cd9660_close },	/* close */
	{ &vop_access_desc, cd9660_access },	/* access */
	{ &vop_getattr_desc, cd9660_getattr },	/* getattr */
	{ &vop_setattr_desc, cd9660_setattr },	/* setattr */
	{ &vop_read_desc, cd9660_read },	/* read */
	{ &vop_write_desc, cd9660_write },	/* write */
	{ &vop_ioctl_desc, cd9660_ioctl },	/* ioctl */
	{ &vop_select_desc, cd9660_select },	/* select */
	{ &vop_mmap_desc, cd9660_mmap },	/* mmap */
	{ &vop_fsync_desc, cd9660_fsync },	/* fsync */
	{ &vop_seek_desc, cd9660_seek },	/* seek */
	{ &vop_remove_desc, cd9660_remove },	/* remove */
	{ &vop_link_desc, cd9660_link },	/* link */
	{ &vop_rename_desc, cd9660_rename },	/* rename */
	{ &vop_mkdir_desc, cd9660_mkdir },	/* mkdir */
	{ &vop_rmdir_desc, cd9660_rmdir },	/* rmdir */
	{ &vop_symlink_desc, cd9660_symlink },	/* symlink */
	{ &vop_readdir_desc, cd9660_readdir },	/* readdir */
	{ &vop_readlink_desc, cd9660_readlink },/* readlink */
	{ &vop_abortop_desc, cd9660_abortop },	/* abortop */
	{ &vop_inactive_desc, cd9660_inactive },/* inactive */
	{ &vop_reclaim_desc, cd9660_reclaim },	/* reclaim */
	{ &vop_lock_desc, cd9660_lock },	/* lock */
	{ &vop_unlock_desc, cd9660_unlock },	/* unlock */
	{ &vop_bmap_desc, cd9660_bmap },	/* bmap */
	{ &vop_strategy_desc, cd9660_strategy },/* strategy */
	{ &vop_print_desc, cd9660_print },	/* print */
	{ &vop_islocked_desc, cd9660_islocked },/* islocked */
	{ &vop_pathconf_desc, cd9660_pathconf },/* pathconf */
	{ &vop_advlock_desc, cd9660_advlock },	/* advlock */
	{ &vop_blkatoff_desc, cd9660_blkatoff },/* blkatoff */
	{ &vop_valloc_desc, cd9660_valloc },	/* valloc */
	{ &vop_vfree_desc, cd9660_vfree },	/* vfree */
	{ &vop_truncate_desc, cd9660_truncate },/* truncate */
	{ &vop_update_desc, cd9660_update },	/* update */
	{ &vop_bwrite_desc, vn_bwrite },
	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
struct vnodeopv_desc cd9660_vnodeop_opv_desc =
	{ &cd9660_vnodeop_p, cd9660_vnodeop_entries };

/*
 * Special device vnode ops
 */
int (**cd9660_specop_p)();
struct vnodeopv_entry_desc cd9660_specop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },	/* lookup */
	{ &vop_create_desc, cd9660_create },	/* create */
	{ &vop_mknod_desc, cd9660_mknod },	/* mknod */
	{ &vop_open_desc, spec_open },		/* open */
	{ &vop_close_desc, spec_close },	/* close */
	{ &vop_access_desc, cd9660_access },	/* access */
	{ &vop_getattr_desc, cd9660_getattr },	/* getattr */
	{ &vop_setattr_desc, cd9660_setattr },	/* setattr */
	{ &vop_read_desc, spec_read },		/* read */
	{ &vop_write_desc, spec_write },	/* write */
	{ &vop_ioctl_desc, spec_ioctl },	/* ioctl */
	{ &vop_select_desc, spec_select },	/* select */
	{ &vop_mmap_desc, spec_mmap },		/* mmap */
	{ &vop_fsync_desc, spec_fsync },	/* fsync */
	{ &vop_seek_desc, spec_seek },		/* seek */
	{ &vop_remove_desc, cd9660_remove },	/* remove */
	{ &vop_link_desc, cd9660_link },	/* link */
	{ &vop_rename_desc, cd9660_rename },	/* rename */
	{ &vop_mkdir_desc, cd9660_mkdir },	/* mkdir */
	{ &vop_rmdir_desc, cd9660_rmdir },	/* rmdir */
	{ &vop_symlink_desc, cd9660_symlink },	/* symlink */
	{ &vop_readdir_desc, spec_readdir },	/* readdir */
	{ &vop_readlink_desc, spec_readlink },	/* readlink */
	{ &vop_abortop_desc, spec_abortop },	/* abortop */
	{ &vop_inactive_desc, cd9660_inactive },/* inactive */
	{ &vop_reclaim_desc, cd9660_reclaim },	/* reclaim */
	{ &vop_lock_desc, cd9660_lock },	/* lock */
	{ &vop_unlock_desc, cd9660_unlock },	/* unlock */
	{ &vop_bmap_desc, spec_bmap },		/* bmap */
		/* XXX strategy: panics, should be notsupp instead? */
	{ &vop_strategy_desc, cd9660_strategy },/* strategy */
	{ &vop_print_desc, cd9660_print },	/* print */
	{ &vop_islocked_desc, cd9660_islocked },/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },	/* pathconf */
	{ &vop_advlock_desc, spec_advlock },	/* advlock */
	{ &vop_blkatoff_desc, spec_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, spec_valloc },	/* valloc */
	{ &vop_vfree_desc, spec_vfree },	/* vfree */
	{ &vop_truncate_desc, spec_truncate },	/* truncate */
	{ &vop_update_desc, cd9660_update },	/* update */
	{ &vop_bwrite_desc, vn_bwrite },
	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
struct vnodeopv_desc cd9660_specop_opv_desc =
	{ &cd9660_specop_p, cd9660_specop_entries };

#ifdef FIFO
int (**cd9660_fifoop_p)();
struct vnodeopv_entry_desc cd9660_fifoop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, fifo_lookup },	/* lookup */
	{ &vop_create_desc, cd9660_create },	/* create */
	{ &vop_mknod_desc, cd9660_mknod },	/* mknod */
	{ &vop_open_desc, fifo_open },		/* open */
	{ &vop_close_desc, fifo_close },	/* close */
	{ &vop_access_desc, cd9660_access },	/* access */
	{ &vop_getattr_desc, cd9660_getattr },	/* getattr */
	{ &vop_setattr_desc, cd9660_setattr },	/* setattr */
	{ &vop_read_desc, fifo_read },		/* read */
	{ &vop_write_desc, fifo_write },	/* write */
	{ &vop_ioctl_desc, fifo_ioctl },	/* ioctl */
	{ &vop_select_desc, fifo_select },	/* select */
	{ &vop_mmap_desc, fifo_mmap },		/* mmap */
	{ &vop_fsync_desc, fifo_fsync },	/* fsync */
	{ &vop_seek_desc, fifo_seek },		/* seek */
	{ &vop_remove_desc, cd9660_remove },	/* remove */
	{ &vop_link_desc, cd9660_link },	/* link */
	{ &vop_rename_desc, cd9660_rename },	/* rename */
	{ &vop_mkdir_desc, cd9660_mkdir },	/* mkdir */
	{ &vop_rmdir_desc, cd9660_rmdir },	/* rmdir */
	{ &vop_symlink_desc, cd9660_symlink },	/* symlink */
	{ &vop_readdir_desc, fifo_readdir },	/* readdir */
	{ &vop_readlink_desc, fifo_readlink },	/* readlink */
	{ &vop_abortop_desc, fifo_abortop },	/* abortop */
	{ &vop_inactive_desc, cd9660_inactive },/* inactive */
	{ &vop_reclaim_desc, cd9660_reclaim },	/* reclaim */
	{ &vop_lock_desc, cd9660_lock },	/* lock */
	{ &vop_unlock_desc, cd9660_unlock },	/* unlock */
	{ &vop_bmap_desc, fifo_bmap },		/* bmap */
	{ &vop_strategy_desc, fifo_badop },	/* strategy */
	{ &vop_print_desc, cd9660_print },	/* print */
	{ &vop_islocked_desc, cd9660_islocked },/* islocked */
	{ &vop_pathconf_desc, fifo_pathconf },	/* pathconf */
	{ &vop_advlock_desc, fifo_advlock },	/* advlock */
	{ &vop_blkatoff_desc, fifo_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, fifo_valloc },	/* valloc */
	{ &vop_vfree_desc, fifo_vfree },	/* vfree */
	{ &vop_truncate_desc, fifo_truncate },	/* truncate */
	{ &vop_update_desc, cd9660_update },	/* update */
	{ &vop_bwrite_desc, vn_bwrite },
	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
struct vnodeopv_desc cd9660_fifoop_opv_desc =
	{ &cd9660_fifoop_p, cd9660_fifoop_entries };
#endif /* FIFO */
