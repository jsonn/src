/*	$NetBSD: ffs_inode.c,v 1.24.2.3 1999/05/30 15:04:34 chs Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ffs_inode.c	8.13 (Berkeley) 4/21/95
 */

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_quota.h"
#include "opt_uvm.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/trace.h>
#include <sys/resourcevar.h>

#include <vm/vm.h>

#if defined(UVM)
#include <uvm/uvm_extern.h>
#endif

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufs_bswap.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

static int ffs_indirtrunc __P((struct inode *, ufs_daddr_t, ufs_daddr_t,
			       ufs_daddr_t, int, long *));

/*
 * Update the access, modified, and inode change times as specified
 * by the IN_ACCESS, IN_UPDATE, and IN_CHANGE flags respectively.
 * The IN_MODIFIED flag is used to specify that the inode needs to be
 * updated but that the times have already been set. The access
 * and modified times are taken from the second and third parameters;
 * the inode change time is always taken from the current time. If
 * waitfor is set, then wait for the disk write of the inode to
 * complete.
 */

int
ffs_update(v)
	void *v;
{
	struct vop_update_args /* {
		struct vnode *a_vp;
		struct timespec *a_access;
		struct timespec *a_modify;
		int a_waitfor;
	} */ *ap = v;
	register struct fs *fs;
	struct buf *bp;
	struct inode *ip;
	int error;
	struct timespec ts;
	caddr_t cp;

	if (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)
		return (0);
	ip = VTOI(ap->a_vp);
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	FFS_ITIMES(ip, ap->a_access, ap->a_modify, &ts);
	if ((ip->i_flag & IN_MODIFIED) == 0)
		return (0);
	ip->i_flag &= ~IN_MODIFIED;
	fs = ip->i_fs;
	/*
	 * Ensure that uid and gid are correct. This is a temporary
	 * fix until fsck has been changed to do the update.
	 */
	if (fs->fs_inodefmt < FS_44INODEFMT) {			/* XXX */
		ip->i_din.ffs_din.di_ouid = ip->i_ffs_uid;	/* XXX */
		ip->i_din.ffs_din.di_ogid = ip->i_ffs_gid;	/* XXX */
	}							/* XXX */
	error = bread(ip->i_devvp,
		      fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
		      (int)fs->fs_bsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}
	cp = (caddr_t)bp->b_data +
	    (ino_to_fsbo(fs, ip->i_number) * DINODE_SIZE);
#ifdef FFS_EI
	if (UFS_MPNEEDSWAP(ap->a_vp->v_mount))
		ffs_dinode_swap(&ip->i_din.ffs_din, (struct dinode *)cp);
	else
#endif
		memcpy(cp, &ip->i_din.ffs_din, DINODE_SIZE);
	if (ap->a_waitfor && (ap->a_vp->v_mount->mnt_flag & MNT_ASYNC) == 0)
		return (bwrite(bp));
	else {
		bdwrite(bp);
		return (0);
	}
}

#define	SINGLE	0	/* index of single indirect block */
#define	DOUBLE	1	/* index of double indirect block */
#define	TRIPLE	2	/* index of triple indirect block */
/*
 * Truncate the inode oip to at most length size, freeing the
 * disk blocks.
 */
int
ffs_truncate(v)
	void *v;
{
	struct vop_truncate_args /* {
		struct vnode *a_vp;
		off_t a_length;
		int a_flags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	register struct vnode *ovp = ap->a_vp;
	register ufs_daddr_t lastblock;
	register struct inode *oip;
	ufs_daddr_t bn, lbn, lastiblock[NIADDR], indir_lbn[NIADDR];
	ufs_daddr_t oldblks[NDADDR + NIADDR], newblks[NDADDR + NIADDR];
	off_t length = ap->a_length;
	register struct fs *fs;
	struct buf *bp;
	int offset, size, level;
	long count, nblocks, vflags, blocksreleased = 0;
	struct timespec ts;
	register int i;
	int aflags, error, allerror;
	off_t osize;

	if (length < 0)
		return (EINVAL);
	oip = VTOI(ovp);
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	if (ovp->v_type == VLNK &&
	    (oip->i_ffs_size < ovp->v_mount->mnt_maxsymlinklen ||
	     (ovp->v_mount->mnt_maxsymlinklen == 0 &&
	      oip->i_din.ffs_din.di_blocks == 0))) {
#ifdef DIAGNOSTIC
		if (length != 0)
			panic("ffs_truncate: partial truncate of symlink");
#endif
		memset((char *)&oip->i_ffs_shortlink, 0, (u_int)oip->i_ffs_size);
		oip->i_ffs_size = 0;
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(ovp, &ts, &ts, 1));
	}
	if (oip->i_ffs_size == length) {
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(ovp, &ts, &ts, 0));
	}
#ifdef QUOTA
	if ((error = getinoquota(oip)) != 0)
		return (error);
#endif
	fs = oip->i_fs;
	osize = oip->i_ffs_size;
	ovp->v_lasta = ovp->v_clen = ovp->v_cstart = ovp->v_lastw = 0;
	/*
	 * Lengthen the size of the file. We must ensure that the
	 * last byte of the file is allocated. Since the smallest
	 * value of osize is 0, length will be at least 1.
	 */
	/*
	 * XXXCHS why must the last byte of the file be allocated?
	 * this seems bogus.
	 */
	if (osize < length) {
		if (length > fs->fs_maxfilesize)
			return (EFBIG);
#ifdef UBC
		aflags = 0;
		bp = 0;
#else
		offset = blkoff(fs, length - 1);
		lbn = lblkno(fs, length - 1);
		aflags = B_CLRBUF;
		if (ap->a_flags & IO_SYNC)
			aflags |= B_SYNC;
		error = ffs_balloc(oip, lbn, offset + 1, ap->a_cred, &bp, NULL,
				   aflags);
		if (error)
			return (error);
#endif
		oip->i_ffs_size = length;
#if defined(UVM)
		uvm_vnp_setsize(ovp, length);
		(void) uvm_vnp_uncache(ovp);
#else
		vnode_pager_setsize(ovp, length);
		(void) vnode_pager_uncache(ovp);
#endif
#ifdef UBC
#else
		if (aflags & B_SYNC)
			bwrite(bp);
		else
			bawrite(bp);
#endif
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(ovp, &ts, &ts, 1));
	}
	/*
	 * Shorten the size of the file. If the file is not being
	 * truncated to a block boundry, the contents of the
	 * partial block following the end of the file must be
	 * zero'ed in case it ever become accessable again because
	 * of subsequent file growth.
	 */
	offset = blkoff(fs, length);
	if (offset == 0) {
		oip->i_ffs_size = length;
	} else {
		lbn = lblkno(fs, length);
#ifdef UBC
#else
		aflags = B_CLRBUF;
		if (ap->a_flags & IO_SYNC)
			aflags |= B_SYNC;
		error = ffs_balloc(oip, lbn, offset, ap->a_cred, &bp, NULL,
				   aflags);
		if (error)
			return (error);
#endif
		oip->i_ffs_size = length;
		size = blksize(fs, oip, lbn);
#if defined(UVM)
		(void) uvm_vnp_uncache(ovp);
#else
		(void) vnode_pager_uncache(ovp);
#endif
#ifdef UBC
		uvm_vnp_zerorange(ovp, oip->i_ffs_size, size - offset);
#else
		memset((char *)bp->b_data + offset, 0,  (u_int)(size - offset));
		allocbuf(bp, size);
		if (aflags & B_SYNC)
			bwrite(bp);
		else
			bawrite(bp);
#endif
	}
#if defined(UVM)
	uvm_vnp_setsize(ovp, length);
#else
	vnode_pager_setsize(ovp, length);
#endif
	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	lastblock = lblkno(fs, length + fs->fs_bsize - 1) - 1;
	lastiblock[SINGLE] = lastblock - NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - NINDIR(fs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - NINDIR(fs) * NINDIR(fs);
	nblocks = btodb(fs->fs_bsize);
	/*
	 * Update file and block pointers on disk before we start freeing
	 * blocks.  If we crash before free'ing blocks below, the blocks
	 * will be returned to the free list.  lastiblock values are also
	 * normalized to -1 for calls to ffs_indirtrunc below.
	 */
	memcpy((caddr_t)oldblks, (caddr_t)&oip->i_ffs_db[0], sizeof oldblks);
	for (level = TRIPLE; level >= SINGLE; level--)
		if (lastiblock[level] < 0) {
			oip->i_ffs_ib[level] = 0;
			lastiblock[level] = -1;
		}
	for (i = NDADDR - 1; i > lastblock; i--)
		oip->i_ffs_db[i] = 0;
	oip->i_flag |= IN_CHANGE | IN_UPDATE;
	if ((error = VOP_UPDATE(ovp, &ts, &ts, 1)) != 0)
		allerror = error;
	/*
	 * Having written the new inode to disk, save its new configuration
	 * and put back the old block pointers long enough to process them.
	 * Note that we save the new block configuration so we can check it
	 * when we are done.
	 */
	memcpy((caddr_t)newblks, (caddr_t)&oip->i_ffs_db[0], sizeof newblks);
	memcpy((caddr_t)&oip->i_ffs_db[0], (caddr_t)oldblks, sizeof oldblks);
	oip->i_ffs_size = osize;
	vflags = ((length > 0) ? V_SAVE : 0) | V_SAVEMETA;
	allerror = vinvalbuf(ovp, vflags, ap->a_cred, ap->a_p, 0, 0);

	/*
	 * Indirect blocks first.
	 */
	indir_lbn[SINGLE] = -NDADDR;
	indir_lbn[DOUBLE] = indir_lbn[SINGLE] - NINDIR(fs) - 1;
	indir_lbn[TRIPLE] = indir_lbn[DOUBLE] - NINDIR(fs) * NINDIR(fs) - 1;
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = ufs_rw32(oip->i_ffs_ib[level], UFS_MPNEEDSWAP(ovp->v_mount));
		if (bn != 0) {
			error = ffs_indirtrunc(oip, indir_lbn[level],
			    fsbtodb(fs, bn), lastiblock[level], level, &count);
			if (error)
				allerror = error;
			blocksreleased += count;
			if (lastiblock[level] < 0) {
				oip->i_ffs_ib[level] = 0;
				ffs_blkfree(oip, bn, fs->fs_bsize);
				blocksreleased += nblocks;
			}
		}
		if (lastiblock[level] >= 0)
			goto done;
	}

	/*
	 * All whole direct blocks or frags.
	 */
	for (i = NDADDR - 1; i > lastblock; i--) {
		register long bsize;

		bn = ufs_rw32(oip->i_ffs_db[i], UFS_MPNEEDSWAP(ovp->v_mount));
		if (bn == 0)
			continue;
		oip->i_ffs_db[i] = 0;
		bsize = blksize(fs, oip, i);
		ffs_blkfree(oip, bn, bsize);
		blocksreleased += btodb(bsize);
	}
	if (lastblock < 0)
		goto done;

	/*
	 * Finally, look for a change in size of the
	 * last direct block; release any frags.
	 */
	bn = ufs_rw32(oip->i_ffs_db[lastblock], UFS_MPNEEDSWAP(ovp->v_mount));
	if (bn != 0) {
		long oldspace, newspace;

		/*
		 * Calculate amount of space we're giving
		 * back as old block size minus new block size.
		 */
		oldspace = blksize(fs, oip, lastblock);
		oip->i_ffs_size = length;
		newspace = blksize(fs, oip, lastblock);
		if (newspace == 0)
			panic("itrunc: newspace");
		if (oldspace - newspace > 0) {
			/*
			 * Block number of space to be free'd is
			 * the old block # plus the number of frags
			 * required for the storage we're keeping.
			 */
			bn += numfrags(fs, newspace);
			ffs_blkfree(oip, bn, oldspace - newspace);
			blocksreleased += btodb(oldspace - newspace);
		}
	}
done:
#ifdef DIAGNOSTIC
	for (level = SINGLE; level <= TRIPLE; level++)
		if (newblks[NDADDR + level] != oip->i_ffs_ib[level])
			panic("itrunc1");
	for (i = 0; i < NDADDR; i++)
		if (newblks[i] != oip->i_ffs_db[i])
			panic("itrunc2");
	if (length == 0 &&
	    (ovp->v_dirtyblkhd.lh_first || ovp->v_cleanblkhd.lh_first))
		panic("itrunc3");
#endif /* DIAGNOSTIC */
	/*
	 * Put back the real size.
	 */
	oip->i_ffs_size = length;
	oip->i_ffs_blocks -= blocksreleased;
	if (oip->i_ffs_blocks < 0)			/* sanity */
		oip->i_ffs_blocks = 0;
	oip->i_flag |= IN_CHANGE;
#ifdef QUOTA
	(void) chkdq(oip, -blocksreleased, NOCRED, 0);
#endif
	return (allerror);
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block bn.  Blocks are free'd in LIFO order up to (but not including)
 * lastbn.  If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 *
 * NB: triple indirect blocks are untested.
 */
static int
ffs_indirtrunc(ip, lbn, dbn, lastbn, level, countp)
	register struct inode *ip;
	ufs_daddr_t lbn, lastbn;
	ufs_daddr_t dbn;
	int level;
	long *countp;
{
	register int i;
	struct buf *bp;
	register struct fs *fs = ip->i_fs;
	register ufs_daddr_t *bap;
	struct vnode *vp;
	ufs_daddr_t *copy = NULL, nb, nlbn, last;
	long blkcount, factor;
	int nblocks, blocksreleased = 0;
	int error = 0, allerror = 0;

	/*
	 * Calculate index in current block of last
	 * block to be kept.  -1 indicates the entire
	 * block so we need not calculate the index.
	 */
	factor = 1;
	for (i = SINGLE; i < level; i++)
		factor *= NINDIR(fs);
	last = lastbn;
	if (lastbn > 0)
		last /= factor;
	nblocks = btodb(fs->fs_bsize);
	/*
	 * Get buffer of block pointers, zero those entries corresponding
	 * to blocks to be free'd, and update on disk copy first.  Since
	 * double(triple) indirect before single(double) indirect, calls
	 * to bmap on these blocks will fail.  However, we already have
	 * the on disk address, so we have to set the b_blkno field
	 * explicitly instead of letting bread do everything for us.
	 */
	vp = ITOV(ip);
	bp = getblk(vp, lbn, (int)fs->fs_bsize, 0, 0);
	if (bp->b_flags & (B_DONE | B_DELWRI)) {
		/* Braces must be here in case trace evaluates to nothing. */
		trace(TR_BREADHIT, pack(vp, fs->fs_bsize), lbn);
	} else {
		trace(TR_BREADMISS, pack(vp, fs->fs_bsize), lbn);
		curproc->p_stats->p_ru.ru_inblock++;	/* pay for read */
		bp->b_flags |= B_READ;
		if (bp->b_bcount > bp->b_bufsize)
			panic("ffs_indirtrunc: bad buffer size");
		bp->b_blkno = dbn;
		VOP_STRATEGY(bp);
		error = biowait(bp);
	}
	if (error) {
		brelse(bp);
		*countp = 0;
		return (error);
	}

	bap = (ufs_daddr_t *)bp->b_data;
	if (lastbn != -1) {
		MALLOC(copy, ufs_daddr_t *, fs->fs_bsize, M_TEMP, M_WAITOK);
		memcpy((caddr_t)copy, (caddr_t)bap, (u_int)fs->fs_bsize);
		memset((caddr_t)&bap[last + 1], 0,
		  (u_int)(NINDIR(fs) - (last + 1)) * sizeof (ufs_daddr_t));
		error = bwrite(bp);
		if (error)
			allerror = error;
		bap = copy;
	}

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = NINDIR(fs) - 1, nlbn = lbn + 1 - i * factor; i > last;
	    i--, nlbn += factor) {
		nb = ufs_rw32(bap[i], UFS_MPNEEDSWAP(vp->v_mount));
		if (nb == 0)
			continue;
		if (level > SINGLE) {
			error = ffs_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
					       (ufs_daddr_t)-1, level - 1,
					       &blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
		ffs_blkfree(ip, nb, fs->fs_bsize);
		blocksreleased += nblocks;
	}

	/*
	 * Recursively free last partial block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		nb = ufs_rw32(bap[i], UFS_MPNEEDSWAP(vp->v_mount));
		if (nb != 0) {
			error = ffs_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
					       last, level - 1, &blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
	}

	if (copy != NULL) {
		FREE(copy, M_TEMP);
	} else {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}

	*countp = blocksreleased;
	return (allerror);
}
