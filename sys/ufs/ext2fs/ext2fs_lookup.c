/*	$NetBSD: ext2fs_lookup.c,v 1.1.4.1 1997/08/23 07:14:34 thorpej Exp $	*/

/* 
 * Modified for NetBSD 1.2E
 * May 1997, Manuel Bouyer
 * Laboratoire d'informatique de Paris VI
 */
/*
 *  modified for Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 * Copyright (c) 1989, 1993
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
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *	may be used to endorse or promote products derived from this software
 *	without specific prior written permission.
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
 *	@(#)ufs_lookup.c	8.6 (Berkeley) 4/1/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/dirent.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext2fs/ext2fs_extern.h>
#include <ufs/ext2fs/ext2fs_dir.h>
#include <ufs/ext2fs/ext2fs.h>

extern	int dirchk;

static void	ext2fs_dirconv2ffs __P((struct ext2fs_direct *e2dir,
					  struct dirent *ffsdir));
static int	ext2fs_dirbadentry __P((struct vnode *dp,
					  struct ext2fs_direct *de,
					  int entryoffsetinblock));

/*
 * the problem that is tackled below is the fact that FFS
 * includes the terminating zero on disk while EXT2FS doesn't
 * this implies that we need to introduce some padding.
 * For instance, a filename "sbin" has normally a reclen 12 
 * in EXT2, but 16 in FFS. 
 * This reminds me of that Pepsi commercial: 'Kid saved a lousy nine cents...'
 * If it wasn't for that, the complete ufs code for directories would
 * have worked w/o changes (except for the difference in DIRBLKSIZ)
 */
static void
ext2fs_dirconv2ffs( e2dir, ffsdir)
	struct ext2fs_direct	*e2dir;
	struct dirent 		*ffsdir;
{
	bzero(ffsdir, sizeof(struct dirent));
	ffsdir->d_fileno = e2dir->e2d_ino;
	ffsdir->d_namlen = e2dir->e2d_namlen;

	ffsdir->d_type = DT_UNKNOWN;		/* don't know more here */
#ifdef DIAGNOSTIC
	/*
	 * XXX Rigth now this can't happen, but if one day
	 * MAXNAMLEN != E2FS_MAXNAMLEN we should handle this more gracefully !
	 */
	if (e2dir->e2d_namlen > MAXNAMLEN) panic("ext2fs: e2dir->e2d_namlen\n");
#endif
	strncpy(ffsdir->d_name, e2dir->e2d_name, ffsdir->d_namlen);

	/* Godmar thinks: since e2dir->e2d_reclen can be big and means 
	   nothing anyway, we compute our own reclen according to what
	   we think is right
	 */
	ffsdir->d_reclen = DIRENT_SIZE(ffsdir);
}

/*
 * Vnode op for reading directories.
 *
 * Convert the on-disk entries to <sys/dirent.h> entries.
 * the problem is that the conversion will blow up some entries by four bytes,
 * so it can't be done in place. This is too bad. Right now the conversion is
 * done entry by entry, the converted entry is sent via uiomove. 
 *
 * XXX allocate a buffer, convert as many entries as possible, then send
 * the whole buffer to uiomove
 */
int
ext2fs_readdir(v)
	void *v;
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int ncookies;
	} */ *ap = v;
	register struct uio *uio = ap->a_uio;
	int error;
	size_t e2fs_count, readcnt;
	struct m_ext2fs *fs = VTOI(ap->a_vp)->i_e2fs;

	struct ext2fs_direct *dp;
	struct dirent dstd;
	struct uio auio;
	struct iovec aiov;
	caddr_t dirbuf;
	off_t off = uio->uio_offset;
	u_long *cookies = ap->a_cookies;
	int ncookies = ap->a_ncookies;

	e2fs_count = uio->uio_resid;
	/* Make sure we don't return partial entries. */
	e2fs_count -= (uio->uio_offset + e2fs_count) & (fs->e2fs_bsize -1);
	if (e2fs_count <= 0)
		return (EINVAL);

	auio = *uio;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = UIO_SYSSPACE;
	aiov.iov_len = e2fs_count;
	auio.uio_resid = e2fs_count;
	MALLOC(dirbuf, caddr_t, e2fs_count, M_TEMP, M_WAITOK);
	bzero(dirbuf, e2fs_count);
	aiov.iov_base = dirbuf;

	error = VOP_READ(ap->a_vp, &auio, 0, ap->a_cred);
	if (error == 0) {
		readcnt = e2fs_count - auio.uio_resid;
		for (dp = (struct ext2fs_direct *)dirbuf; 
			(char *)dp < (char *)dirbuf + readcnt; ) {
			if (dp->e2d_reclen <= 0) {
				error = EIO;
				break;
			}
			ext2fs_dirconv2ffs(dp, &dstd);
			if(dstd.d_reclen > uio->uio_resid) {
				break;
			}
			if ((error = uiomove((caddr_t)&dstd, dstd.d_reclen, uio)) != 0) {
				break;
			}
			off = off + dp->e2d_reclen;
			if (cookies != NULL) {
				*cookies++ = off;
				if (--ncookies <= 0){
					break;  /* out of cookies */
				}
			}
			/* advance dp */
			dp = (struct ext2fs_direct *) ((char *)dp + dp->e2d_reclen);
		}
		/* we need to correct uio_offset */
		uio->uio_offset = off;
	}
	FREE(dirbuf, M_TEMP);
	*ap->a_eofflag = VTOI(ap->a_vp)->i_e2fs_size <= uio->uio_offset;
	return (error);
}

/*
 * Convert a component of a pathname into a pointer to a locked inode.
 * This is a very central and rather complicated routine.
 * If the file system is not maintained in a strict tree hierarchy,
 * this can result in a deadlock situation (see comments in code below).
 *
 * The cnp->cn_nameiop argument is LOOKUP, CREATE, RENAME, or DELETE depending
 * on whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If flag has LOCKPARENT or'ed into it and the target of the pathname
 * exists, lookup returns both the target and its parent directory locked.
 * When creating or renaming and LOCKPARENT is specified, the target may
 * not be ".".  When deleting and LOCKPARENT is specified, the target may
 * be "."., but the caller must check to ensure it does an vrele and vput
 * instead of two vputs.
 *
 * Overall outline of ext2fs_lookup:
 *
 *	check accessibility of directory
 *	look for name in cache, if found, then if at end of path
 *	  and deleting or creating, drop it, else return name
 *	search for name in directory, to found or notfound
 * notfound:
 *	if creating, return locked directory, leaving info on available slots
 *	else return error
 * found:
 *	if at end of path and deleting, return information to allow delete
 *	if at end of path and rewriting (RENAME and LOCKPARENT), lock target
 *	  inode and return info to allow rewrite
 *	if not at end, add name to cache; if at end and neither creating
 *	  nor deleting, add name to cache
 */
int
ext2fs_lookup(v)
	void *v;
{
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	register struct vnode *vdp;	/* vnode for directory being searched */
	register struct inode *dp;	/* inode for directory being searched */
	struct buf *bp;			/* a buffer of directory entries */
	register struct ext2fs_direct *ep; /* the current directory entry */
	int entryoffsetinblock;		/* offset of ep in bp's buffer */
	enum {NONE, COMPACT, FOUND} slotstatus;
	doff_t slotoffset;		/* offset of area with free space */
	int slotsize;			/* size of area at slotoffset */
	int slotfreespace;		/* amount of space free in slot */
	int slotneeded;			/* size of the entry we're seeking */
	int numdirpasses;		/* strategy for directory search */
	doff_t endsearch;		/* offset to end directory search */
	doff_t prevoff;			/* prev entry dp->i_offset */
	struct vnode *pdp;		/* saved dp during symlink work */
	struct vnode *tdp;		/* returned by VFS_VGET */
	doff_t enduseful;		/* pointer past last used dir slot */
	u_long bmask;			/* block offset mask */
	int lockparent;			/* 1 => lockparent flag is set */
	int wantparent;			/* 1 => wantparent or lockparent flag */
	int namlen, error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;

	int	dirblksize = VTOI(ap->a_dvp)->i_e2fs->e2fs_bsize;

	bp = NULL;
	slotoffset = -1;
	*vpp = NULL;
	vdp = ap->a_dvp;
	dp = VTOI(vdp);
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT|WANTPARENT);
	/*
	 * Check accessiblity of directory.
	 */
	if ((error = VOP_ACCESS(vdp, VEXEC, cred, cnp->cn_proc)) != 0)
		return (error);

	/*
	 * We now have a segment name to search for, and a directory to search.
	 *
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 */
	if ((error = cache_lookup(vdp, vpp, cnp)) != 0) {
		int vpid;	/* capability number of vnode */

		if (error == ENOENT)
			return (error);
		/*
		 * Get the next vnode in the path.
		 * See comment below starting `Step through' for
		 * an explaination of the locking protocol.
		 */
		pdp = vdp;
		dp = VTOI(*vpp);
		vdp = *vpp;
		vpid = vdp->v_id;
		if (pdp == vdp) {   /* lookup on "." */
			VREF(vdp);
			error = 0;
		} else if (flags & ISDOTDOT) {
			VOP_UNLOCK(pdp);
			error = vget(vdp, 1);
			if (!error && lockparent && (flags & ISLASTCN))
				error = VOP_LOCK(pdp);
		} else {
			error = vget(vdp, 1);
			if (!lockparent || error || !(flags & ISLASTCN))
				VOP_UNLOCK(pdp);
		}
		/*
		 * Check that the capability number did not change
		 * while we were waiting for the lock.
		 */
		if (!error) {
			if (vpid == vdp->v_id)
				return (0);
			vput(vdp);
			if (lockparent && pdp != vdp && (flags & ISLASTCN))
				VOP_UNLOCK(pdp);
		}
		if ((error = VOP_LOCK(pdp)) != 0)
			return (error);
		vdp = pdp;
		dp = VTOI(pdp);
		*vpp = NULL;
	}

	/*
	 * Suppress search for slots unless creating
	 * file and at end of pathname, in which case
	 * we watch for a place to put the new file in
	 * case it doesn't already exist.
	 */
	slotstatus = FOUND;
	slotfreespace = slotsize = slotneeded = 0;
	if ((nameiop == CREATE || nameiop == RENAME) &&
		(flags & ISLASTCN)) {
		slotstatus = NONE;
		slotneeded = EXT2FS_DIRSIZ(cnp->cn_namelen);
	}

	/*
	 * If there is cached information on a previous search of
	 * this directory, pick up where we last left off.
	 * We cache only lookups as these are the most common
	 * and have the greatest payoff. Caching CREATE has little
	 * benefit as it usually must search the entire directory
	 * to determine that the entry does not exist. Caching the
	 * location of the last DELETE or RENAME has not reduced
	 * profiling time and hence has been removed in the interest
	 * of simplicity.
	 */
	bmask = VFSTOUFS(vdp->v_mount)->um_mountp->mnt_stat.f_iosize - 1;
	if (nameiop != LOOKUP || dp->i_diroff == 0 ||
		dp->i_diroff > dp->i_e2fs_size) {
		entryoffsetinblock = 0;
		dp->i_offset = 0;
		numdirpasses = 1;
	} else {
		dp->i_offset = dp->i_diroff;
		if ((entryoffsetinblock = dp->i_offset & bmask) &&
			(error = VOP_BLKATOFF(vdp, (off_t)dp->i_offset, NULL, &bp)))
			return (error);
		numdirpasses = 2;
	}
	prevoff = dp->i_offset;
	endsearch = roundup(dp->i_e2fs_size, dirblksize);
	enduseful = 0;

searchloop:
	while (dp->i_offset < endsearch) {
		/*
		 * If necessary, get the next directory block.
		 */
		if ((dp->i_offset & bmask) == 0) {
			if (bp != NULL)
				brelse(bp);
			error = VOP_BLKATOFF(vdp, (off_t)dp->i_offset, NULL, &bp);
			if (error != 0)
				return (error);
			entryoffsetinblock = 0;
		}
		/*
		 * If still looking for a slot, and at a dirblksize
		 * boundary, have to start looking for free space again.
		 */
		if (slotstatus == NONE &&
			(entryoffsetinblock & (dirblksize - 1)) == 0) {
			slotoffset = -1;
			slotfreespace = 0;
		}
		/*
		 * Get pointer to next entry.
		 * Full validation checks are slow, so we only check
		 * enough to insure forward progress through the
		 * directory. Complete checks can be run by patching
		 * "dirchk" to be true.
		 */
		ep = (struct ext2fs_direct *)
			((char *)bp->b_data + entryoffsetinblock);
		if (ep->e2d_reclen == 0 ||
			(dirchk && ext2fs_dirbadentry(vdp, ep, entryoffsetinblock))) {
			int i;
			ufs_dirbad(dp, dp->i_offset, "mangled entry");
			i = dirblksize - (entryoffsetinblock & (dirblksize - 1));
			dp->i_offset += i;
			entryoffsetinblock += i;
			continue;
		}

		/*
		 * If an appropriate sized slot has not yet been found,
		 * check to see if one is available. Also accumulate space
		 * in the current block so that we can determine if
		 * compaction is viable.
		 */
		if (slotstatus != FOUND) {
			int size = ep->e2d_reclen;

			if (ep->e2d_ino != 0)
				size -= EXT2FS_DIRSIZ(ep->e2d_namlen);
			if (size > 0) {
				if (size >= slotneeded) {
					slotstatus = FOUND;
					slotoffset = dp->i_offset;
					slotsize = ep->e2d_reclen;
				} else if (slotstatus == NONE) {
					slotfreespace += size;
					if (slotoffset == -1)
						slotoffset = dp->i_offset;
					if (slotfreespace >= slotneeded) {
						slotstatus = COMPACT;
						slotsize = dp->i_offset +
							  ep->e2d_reclen - slotoffset;
					}
				}
			}
		}

		/*
		 * Check for a name match.
		 */
		if (ep->e2d_ino) {
			namlen = ep->e2d_namlen;
			if (namlen == cnp->cn_namelen &&
				!bcmp(cnp->cn_nameptr, ep->e2d_name,
				(unsigned)namlen)) {
				/*
				 * Save directory entry's inode number and
				 * reclen in ndp->ni_ufs area, and release
				 * directory buffer.
				 */
				dp->i_ino = ep->e2d_ino;
				dp->i_reclen = ep->e2d_reclen;
				brelse(bp);
				goto found;
			}
		}
		prevoff = dp->i_offset;
		dp->i_offset += ep->e2d_reclen;
		entryoffsetinblock += ep->e2d_reclen;
		if (ep->e2d_ino)
			enduseful = dp->i_offset;
	}
/* notfound: */
	/*
	 * If we started in the middle of the directory and failed
	 * to find our target, we must check the beginning as well.
	 */
	if (numdirpasses == 2) {
		numdirpasses--;
		dp->i_offset = 0;
		endsearch = dp->i_diroff;
		goto searchloop;
	}
	if (bp != NULL)
		brelse(bp);
	/*
	 * If creating, and at end of pathname and current
	 * directory has not been removed, then can consider
	 * allowing file to be created.
	 */
	if ((nameiop == CREATE || nameiop == RENAME) &&
		(flags & ISLASTCN) && dp->i_e2fs_nlink != 0) {
		/*
		 * Creation of files on a read-only mounted file system
		 * is pointless, so don't proceed any further.
		 */
		if (vdp->v_mount->mnt_flag & MNT_RDONLY)
					return (EROFS);
		/*
		 * Access for write is interpreted as allowing
		 * creation of files in the directory.
		 */
		if ((error = VOP_ACCESS(vdp, VWRITE, cred, cnp->cn_proc)) != 0)
			return (error);
		/*
		 * Return an indication of where the new directory
		 * entry should be put.  If we didn't find a slot,
		 * then set dp->i_count to 0 indicating
		 * that the new slot belongs at the end of the
		 * directory. If we found a slot, then the new entry
		 * can be put in the range from dp->i_offset to
		 * dp->i_offset + dp->i_count.
		 */
		if (slotstatus == NONE) {
			dp->i_offset = roundup(dp->i_e2fs_size, dirblksize);
			dp->i_count = 0;
			enduseful = dp->i_offset;
		} else {
			dp->i_offset = slotoffset;
			dp->i_count = slotsize;
			if (enduseful < slotoffset + slotsize)
				enduseful = slotoffset + slotsize;
		}
		dp->i_endoff = roundup(enduseful, dirblksize);
		dp->i_flag |= IN_CHANGE | IN_UPDATE;
		/*
		 * We return with the directory locked, so that
		 * the parameters we set up above will still be
		 * valid if we actually decide to do a direnter().
		 * We return ni_vp == NULL to indicate that the entry
		 * does not currently exist; we leave a pointer to
		 * the (locked) directory inode in ndp->ni_dvp.
		 * The pathname buffer is saved so that the name
		 * can be obtained later.
		 *
		 * NB - if the directory is unlocked, then this
		 * information cannot be used.
		 */
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)
			VOP_UNLOCK(vdp);
		return (EJUSTRETURN);
	}
	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	if ((cnp->cn_flags & MAKEENTRY) && nameiop != CREATE)
		cache_enter(vdp, *vpp, cnp);
	return (ENOENT);

found:
	/*
	 * Check that directory length properly reflects presence
	 * of this entry.
	 */
	if (entryoffsetinblock + EXT2FS_DIRSIZ(ep->e2d_namlen)
		> dp->i_e2fs_size) {
		ufs_dirbad(dp, dp->i_offset, "i_size too small");
		dp->i_e2fs_size = entryoffsetinblock+EXT2FS_DIRSIZ(ep->e2d_namlen);
		dp->i_flag |= IN_CHANGE | IN_UPDATE;
	}

	/*
	 * Found component in pathname.
	 * If the final component of path name, save information
	 * in the cache as to where the entry was found.
	 */
	if ((flags & ISLASTCN) && nameiop == LOOKUP)
		dp->i_diroff = dp->i_offset &~ (dirblksize - 1);

	/*
	 * If deleting, and at end of pathname, return
	 * parameters which can be used to remove file.
	 * If the wantparent flag isn't set, we return only
	 * the directory (in ndp->ni_dvp), otherwise we go
	 * on and lock the inode, being careful with ".".
	 */
	if (nameiop == DELETE && (flags & ISLASTCN)) {
		/*
		 * Write access to directory required to delete files.
		 */
		if ((error = VOP_ACCESS(vdp, VWRITE, cred, cnp->cn_proc)) != 0)
			return (error);
		/*
		 * Return pointer to current entry in dp->i_offset,
		 * and distance past previous entry (if there
		 * is a previous entry in this block) in dp->i_count.
		 * Save directory inode pointer in ndp->ni_dvp for dirremove().
		 */
		if ((dp->i_offset & (dirblksize - 1)) == 0)
			dp->i_count = 0;
		else
			dp->i_count = dp->i_offset - prevoff;
		if (dp->i_number == dp->i_ino) {
			VREF(vdp);
			*vpp = vdp;
			return (0);
		}
		if ((error = VFS_VGET(vdp->v_mount, dp->i_ino, &tdp)) != 0)
			return (error);
		/*
		 * If directory is "sticky", then user must own
		 * the directory, or the file in it, else she
		 * may not delete it (unless she's root). This
		 * implements append-only directories.
		 */
		if ((dp->i_e2fs_mode & ISVTX) &&
			cred->cr_uid != 0 &&
			cred->cr_uid != dp->i_e2fs_uid &&
			VTOI(tdp)->i_e2fs_uid != cred->cr_uid) {
			vput(tdp);
			return (EPERM);
		}
		*vpp = tdp;
		if (!lockparent)
			VOP_UNLOCK(vdp);
		return (0);
	}

	/*
	 * If rewriting (RENAME), return the inode and the
	 * information required to rewrite the present directory
	 * Must get inode of directory entry to verify it's a
	 * regular file, or empty directory.
	 */
	if (nameiop == RENAME && wantparent &&
		(flags & ISLASTCN)) {
		if ((error = VOP_ACCESS(vdp, VWRITE, cred, cnp->cn_proc)) != 0)
			return (error);
		/*
		 * Careful about locking second inode.
		 * This can only occur if the target is ".".
		 */
		if (dp->i_number == dp->i_ino)
			return (EISDIR);
		if ((error = VFS_VGET(vdp->v_mount, dp->i_ino, &tdp)) != 0)
			return (error);
		*vpp = tdp;
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)
			VOP_UNLOCK(vdp);
		return (0);
	}

	/*
	 * Step through the translation in the name.  We do not `vput' the
	 * directory because we may need it again if a symbolic link
	 * is relative to the current directory.  Instead we save it
	 * unlocked as "pdp".  We must get the target inode before unlocking
	 * the directory to insure that the inode will not be removed
	 * before we get it.  We prevent deadlock by always fetching
	 * inodes from the root, moving down the directory tree. Thus
	 * when following backward pointers ".." we must unlock the
	 * parent directory before getting the requested directory.
	 * There is a potential race condition here if both the current
	 * and parent directories are removed before the VFS_VGET for the
	 * inode associated with ".." returns.  We hope that this occurs
	 * infrequently since we cannot avoid this race condition without
	 * implementing a sophisticated deadlock detection algorithm.
	 * Note also that this simple deadlock detection scheme will not
	 * work if the file system has any hard links other than ".."
	 * that point backwards in the directory structure.
	 */
	pdp = vdp;
	if (flags & ISDOTDOT) {
		VOP_UNLOCK(pdp);	/* race to get the inode */
		if ((error = VFS_VGET(vdp->v_mount, dp->i_ino, &tdp)) != 0) {
			VOP_LOCK(pdp);
			return (error);
		}
		if (lockparent && (flags & ISLASTCN) &&
			(error = VOP_LOCK(pdp)) != 0) {
			vput(tdp);
			return (error);
		}
		*vpp = tdp;
	} else if (dp->i_number == dp->i_ino) {
		VREF(vdp);	/* we want ourself, ie "." */
		*vpp = vdp;
	} else {
		if ((error = VFS_VGET(vdp->v_mount, dp->i_ino, &tdp)) != 0)
			return (error);
		if (!lockparent || !(flags & ISLASTCN))
			VOP_UNLOCK(pdp);
		*vpp = tdp;
	}

	/*
	 * Insert name into cache if appropriate.
	 */
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vdp, *vpp, cnp);
	return (0);
}

/*
 * Do consistency checking on a directory entry:
 *	record length must be multiple of 4
 *	entry must fit in rest of its dirblksize block
 *	record must be large enough to contain entry
 *	name is not longer than MAXNAMLEN
 *	name must be as long as advertised, and null terminated
 */
/*
 *	changed so that it confirms to ext2fs_check_dir_entry
 */
static int
ext2fs_dirbadentry(dp, de, entryoffsetinblock)
	struct vnode *dp;
	register struct ext2fs_direct *de;
	int entryoffsetinblock;
{
	int	dirblksize = VTOI(dp)->i_e2fs->e2fs_bsize;

		char * error_msg = NULL;

		if (de->e2d_reclen < EXT2FS_DIRSIZ(1)) /* e2d_namlen = 1 */
				error_msg = "rec_len is smaller than minimal";
		else if (de->e2d_reclen % 4 != 0)
				error_msg = "rec_len % 4 != 0";
		else if (de->e2d_reclen < EXT2FS_DIRSIZ(de->e2d_namlen))
				error_msg = "reclen is too small for name_len";
		else if (entryoffsetinblock + de->e2d_reclen > dirblksize)
				error_msg = "directory entry across blocks";
		else if (de->e2d_ino > VTOI(dp)->i_e2fs->e2fs.e2fs_icount)
				error_msg = "inode out of bounds";

		if (error_msg != NULL) {
			printf( "bad directory entry: %s\n"
						"offset=%d, inode=%lu, rec_len=%d, name_len=%d \n",
						error_msg, entryoffsetinblock, 
			(unsigned long) de->e2d_ino, de->e2d_reclen, de->e2d_namlen);
			panic("ext2fs_dirbadentry");
		}
		return error_msg == NULL ? 0 : 1;
}

/*
 * Write a directory entry after a call to namei, using the parameters
 * that it left in nameidata.  The argument ip is the inode which the new
 * directory entry will refer to.  Dvp is a pointer to the directory to
 * be written, which was left locked by namei. Remaining parameters
 * (dp->i_offset, dp->i_count) indicate how the space for the new
 * entry is to be obtained.
 */
int
ext2fs_direnter(ip, dvp, cnp)
	struct inode *ip;
	struct vnode *dvp;
	register struct componentname *cnp;
{
	register struct ext2fs_direct *ep, *nep;
	register struct inode *dp;
	struct buf *bp;
	struct ext2fs_direct newdir;
	struct iovec aiov;
	struct uio auio;
	u_int dsize;
	int error, loc, newentrysize, spacefree;
	char *dirbuf;
	int	 dirblksize = ip->i_e2fs->e2fs_bsize;


#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & SAVENAME) == 0)
		panic("direnter: missing name");
#endif
	dp = VTOI(dvp);
	newdir.e2d_ino = ip->i_number;
	newdir.e2d_namlen = cnp->cn_namelen;
	bcopy(cnp->cn_nameptr, newdir.e2d_name, (unsigned)cnp->cn_namelen + 1);
	newentrysize = EXT2FS_DIRSIZ(newdir.e2d_namlen);
	if (dp->i_count == 0) {
		/*
		 * If dp->i_count is 0, then namei could find no
		 * space in the directory. Here, dp->i_offset will
		 * be on a directory block boundary and we will write the
		 * new entry into a fresh block.
		 */
		if (dp->i_offset & (dirblksize - 1))
			panic("ext2fs_direnter: newblk");
		auio.uio_offset = dp->i_offset;
		newdir.e2d_reclen = dirblksize;
		auio.uio_resid = newentrysize;
		aiov.iov_len = newentrysize;
		aiov.iov_base = (caddr_t)&newdir;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_rw = UIO_WRITE;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_procp = (struct proc *)0;
		error = VOP_WRITE(dvp, &auio, IO_SYNC, cnp->cn_cred);
		if (dirblksize >
			VFSTOUFS(dvp->v_mount)->um_mountp->mnt_stat.f_bsize)
			/* XXX should grow with balloc() */
			panic("ext2fs_direnter: frag size");
		else if (!error) {
			dp->i_e2fs_size = roundup(dp->i_e2fs_size, dirblksize);
			dp->i_flag |= IN_CHANGE;
		}
		return (error);
	}

	/*
	 * If dp->i_count is non-zero, then namei found space
	 * for the new entry in the range dp->i_offset to
	 * dp->i_offset + dp->i_count in the directory.
	 * To use this space, we may have to compact the entries located
	 * there, by copying them together towards the beginning of the
	 * block, leaving the free space in one usable chunk at the end.
	 */

	/*
	 * Get the block containing the space for the new directory entry.
	 */
	if ((error = VOP_BLKATOFF(dvp, (off_t)dp->i_offset, &dirbuf, &bp)) != 0)
		return (error);
	/*
	 * Find space for the new entry. In the simple case, the entry at
	 * offset base will have the space. If it does not, then namei
	 * arranged that compacting the region dp->i_offset to
	 * dp->i_offset + dp->i_count would yield the
	 * space.
	 */
	ep = (struct ext2fs_direct *)dirbuf;
	dsize = EXT2FS_DIRSIZ(ep->e2d_namlen);
	spacefree = ep->e2d_reclen - dsize;
	for (loc = ep->e2d_reclen; loc < dp->i_count; ) {
		nep = (struct ext2fs_direct *)(dirbuf + loc);
		if (ep->e2d_ino) {
			/* trim the existing slot */
			ep->e2d_reclen = dsize;
			ep = (struct ext2fs_direct *)((char *)ep + dsize);
		} else {
			/* overwrite; nothing there; header is ours */
			spacefree += dsize;
		}
		dsize = EXT2FS_DIRSIZ(nep->e2d_namlen);
		spacefree += nep->e2d_reclen - dsize;
		loc += nep->e2d_reclen;
		bcopy((caddr_t)nep, (caddr_t)ep, dsize);
	}
	/*
	 * Update the pointer fields in the previous entry (if any),
	 * copy in the new entry, and write out the block.
	 */
	if (ep->e2d_ino == 0) {
#ifdef DIAGNOSTIC
		if (spacefree + dsize < newentrysize)
			panic("ext2fs_direnter: compact1");
#endif
		newdir.e2d_reclen = spacefree + dsize;
	} else {
#ifdef DIAGNOSTIC
		if (spacefree < newentrysize) {
			printf("ext2fs_direnter: compact2 %u %u",
						(u_int)spacefree, (u_int)newentrysize);
			panic("ext2fs_direnter: compact2");
		}
#endif
		newdir.e2d_reclen = spacefree;
		ep->e2d_reclen = dsize;
		ep = (struct ext2fs_direct *)((char *)ep + dsize);
	}
	bcopy((caddr_t)&newdir, (caddr_t)ep, (u_int)newentrysize);
	error = VOP_BWRITE(bp);
	dp->i_flag |= IN_CHANGE | IN_UPDATE;
	if (!error && dp->i_endoff && dp->i_endoff < dp->i_e2fs_size)
		error = VOP_TRUNCATE(dvp, (off_t)dp->i_endoff, IO_SYNC,
			cnp->cn_cred, cnp->cn_proc);
	return (error);
}

/*
 * Remove a directory entry after a call to namei, using
 * the parameters which it left in nameidata. The entry
 * dp->i_offset contains the offset into the directory of the
 * entry to be eliminated.  The dp->i_count field contains the
 * size of the previous record in the directory.  If this
 * is 0, the first entry is being deleted, so we need only
 * zero the inode number to mark the entry as free.  If the
 * entry is not the first in the directory, we must reclaim
 * the space of the now empty record by adding the record size
 * to the size of the previous entry.
 */
int
ext2fs_dirremove(dvp, cnp)
	struct vnode *dvp;
	struct componentname *cnp;
{
	register struct inode *dp;
	struct ext2fs_direct *ep;
	struct buf *bp;
	int error;
	 
	dp = VTOI(dvp);
	if (dp->i_count == 0) {
		/*
		 * First entry in block: set d_ino to zero.
		 */
		error = VOP_BLKATOFF(dvp, (off_t)dp->i_offset, (char **)&ep, &bp);
		if (error != 0)
			return (error);
		ep->e2d_ino = 0;
		error = VOP_BWRITE(bp);
		dp->i_flag |= IN_CHANGE | IN_UPDATE;
		return (error);
	}
	/*
	 * Collapse new free space into previous entry.
	 */
	error = VOP_BLKATOFF(dvp, (off_t)(dp->i_offset - dp->i_count),
			(char **)&ep, &bp);
	if (error != 0)
		return (error);
	ep->e2d_reclen += dp->i_reclen;
	error = VOP_BWRITE(bp);
	dp->i_flag |= IN_CHANGE | IN_UPDATE;
	return (error);
}

/*
 * Rewrite an existing directory entry to point at the inode
 * supplied.  The parameters describing the directory entry are
 * set up by a call to namei.
 */
int
ext2fs_dirrewrite(dp, ip, cnp)
	struct inode *dp, *ip;
	struct componentname *cnp;
{
	struct buf *bp;
	struct ext2fs_direct *ep;
	struct vnode *vdp = ITOV(dp);
	int error;

	error = VOP_BLKATOFF(vdp, (off_t)dp->i_offset, (char **)&ep, &bp);
	if (error != 0)
		return (error);
	ep->e2d_ino = ip->i_number;
	error = VOP_BWRITE(bp);
	dp->i_flag |= IN_CHANGE | IN_UPDATE;
	return (error);
}

/*
 * Check if a directory is empty or not.
 * Inode supplied must be locked.
 *
 * Using a struct dirtemplate here is not precisely
 * what we want, but better than using a struct ext2fs_direct.
 *
 * NB: does not handle corrupted directories.
 */
int
ext2fs_dirempty(ip, parentino, cred)
	register struct inode *ip;
	ino_t parentino;
	struct ucred *cred;
{
	register off_t off;
	struct ext2fs_dirtemplate dbuf;
	register struct ext2fs_direct *dp = (struct ext2fs_direct *)&dbuf;
	int error, count, namlen;
		 
#define	MINDIRSIZ (sizeof (struct ext2fs_dirtemplate) / 2)

	for (off = 0; off < ip->i_e2fs_size; off += dp->e2d_reclen) {
		error = vn_rdwr(UIO_READ, ITOV(ip), (caddr_t)dp, MINDIRSIZ, off,
		   UIO_SYSSPACE, IO_NODELOCKED, cred, &count, (struct proc *)0);
		/*
		 * Since we read MINDIRSIZ, residual must
		 * be 0 unless we're at end of file.
		 */
		if (error || count != 0)
			return (0);
		/* avoid infinite loops */
		if (dp->e2d_reclen == 0)
			return (0);
		/* skip empty entries */
		if (dp->e2d_ino == 0)
			continue;
		/* accept only "." and ".." */
		namlen = dp->e2d_namlen;
		if (namlen > 2)
			return (0);
		if (dp->e2d_name[0] != '.')
			return (0);
		/*
		 * At this point namlen must be 1 or 2.
		 * 1 implies ".", 2 implies ".." if second
		 * char is also "."
		 */
		if (namlen == 1)
			continue;
		if (dp->e2d_name[1] == '.' && dp->e2d_ino == parentino)
			continue;
		return (0);
	}
	return (1);
}

/*
 * Check if source directory is in the path of the target directory.
 * Target is supplied locked, source is unlocked.
 * The target is always vput before returning.
 */
int
ext2fs_checkpath(source, target, cred)
	struct inode *source, *target;
	struct ucred *cred;
{
	struct vnode *vp;
	int error, rootino, namlen;
	struct ext2fs_dirtemplate dirbuf;

	vp = ITOV(target);
	if (target->i_number == source->i_number) {
		error = EEXIST;
		goto out;
	}
	rootino = ROOTINO;
	error = 0;
	if (target->i_number == rootino)
		goto out;

	for (;;) {
		if (vp->v_type != VDIR) {
			error = ENOTDIR;
			break;
		}
		error = vn_rdwr(UIO_READ, vp, (caddr_t)&dirbuf,
			sizeof (struct ext2fs_dirtemplate), (off_t)0, UIO_SYSSPACE,
			IO_NODELOCKED, cred, (int *)0, (struct proc *)0);
		if (error != 0)
			break;
		namlen = dirbuf.dotdot_namlen;
		if (namlen != 2 ||
			dirbuf.dotdot_name[0] != '.' ||
			dirbuf.dotdot_name[1] != '.') {
			error = ENOTDIR;
			break;
		}
		if (dirbuf.dotdot_ino == source->i_number) {
			error = EINVAL;
			break;
		}
		if (dirbuf.dotdot_ino == rootino)
			break;
		vput(vp);
		error = VFS_VGET(vp->v_mount, dirbuf.dotdot_ino, &vp);
		if (error != 0) {
			vp = NULL;
			break;
		}
	}

out:
	if (error == ENOTDIR) {
		printf("checkpath: .. not a directory\n");
		panic("checkpath");
	}
	if (vp != NULL)
		vput(vp);
	return (error);
}

