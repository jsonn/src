/*	$NetBSD: ext2fs_inode.c,v 1.13.4.1 1999/07/11 05:43:59 chs Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.
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
 *	@(#)ffs_inode.c	8.8 (Berkeley) 10/19/94
 * Modified for ext2fs by Manuel Bouyer.
 */

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

#include <uvm/uvm_extern.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>

static int ext2fs_indirtrunc __P((struct inode *, ufs_daddr_t, ufs_daddr_t,
				  ufs_daddr_t, int, long *));

/*
 * Last reference to an inode.  If necessary, write or delete it.
 */
int
ext2fs_inactive(v)
	void *v;
{   
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct proc *p = ap->a_p;
	struct timespec ts;
	int error = 0;
	extern int prtactive;
	
	if (prtactive && vp->v_usecount != 0)
		vprint("ext2fs_inactive: pushing active", vp);
	/* Get rid of inodes related to stale file handles. */
	if (ip->i_e2fs_mode == 0 || ip->i_e2fs_dtime != 0)
		goto out;

	error = 0;
	if (ip->i_e2fs_nlink == 0 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
		error = VOP_TRUNCATE(vp, (off_t)0, 0, NOCRED, NULL); 
		TIMEVAL_TO_TIMESPEC(&time, &ts);
		ip->i_e2fs_dtime = ts.tv_sec;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		VOP_VFREE(vp, ip->i_number, ip->i_e2fs_mode);
	}
	if (ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE))
		VOP_UPDATE(vp, NULL, NULL, 0);
out:
	VOP_UNLOCK(vp, 0);
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */
	if (ip->i_e2fs_dtime != 0)
		vrecycle(vp, (struct simplelock *)0, p);
	return (error);
}   


/*
 * Update the access, modified, and inode change times as specified by the
 * IACCESS, IUPDATE, and ICHANGE flags respectively. The IMODIFIED flag is
 * used to specify that the inode needs to be updated but that the times have
 * already been set. The access and modified times are taken from the second
 * and third parameters; the inode change time is always taken from the current
 * time. If waitfor is set, then wait for the disk write of the inode to
 * complete.
 */
int
ext2fs_update(v)
	void *v;
{
	struct vop_update_args /* {
		struct vnode *a_vp;
		struct timespec *a_access;
		struct timespec *a_modify;
		int a_waitfor;
	} */ *ap = v;
	register struct m_ext2fs *fs;
	struct buf *bp;
	struct inode *ip;
	int error;
	struct timespec ts;
	caddr_t cp;

	if (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)
		return (0);
	ip = VTOI(ap->a_vp);
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	EXT2FS_ITIMES(ip,
	    ap->a_access ? ap->a_access : &ts,
	    ap->a_modify ? ap->a_modify : &ts, &ts);
	if ((ip->i_flag & IN_MODIFIED) == 0)
		return (0);
	ip->i_flag &= ~IN_MODIFIED;
	fs = ip->i_e2fs;
	error = bread(ip->i_devvp,
			  fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
			  (int)fs->e2fs_bsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}
	cp = (caddr_t)bp->b_data +
	    (ino_to_fsbo(fs, ip->i_number) * EXT2_DINODE_SIZE);
	e2fs_isave(&ip->i_din.e2fs_din, (struct ext2fs_dinode *)cp);
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
ext2fs_truncate(v)
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
	register struct m_ext2fs *fs;
	struct buf *bp;
	int offset, size, level;
	long count, nblocks, vflags, blocksreleased = 0;
	register int i;
	int aflags, error, allerror;
	off_t osize;

	if (length < 0)
		return (EINVAL);

	oip = VTOI(ovp);
	if (ovp->v_type == VLNK &&
		(oip->i_e2fs_size < ovp->v_mount->mnt_maxsymlinklen ||
		 (ovp->v_mount->mnt_maxsymlinklen == 0 &&
		  oip->i_e2fs_nblock == 0))) {
#ifdef DIAGNOSTIC
		if (length != 0)
			panic("ext2fs_truncate: partial truncate of symlink");
#endif
		memset((char *)&oip->i_din.e2fs_din.e2di_shortlink, 0,
			(u_int)oip->i_e2fs_size);
		oip->i_e2fs_size = 0;
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(ovp, NULL, NULL, 1));
	}
	if (oip->i_e2fs_size == length) {
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(ovp, NULL, NULL, 0));
	}
	fs = oip->i_e2fs;
	osize = oip->i_e2fs_size;
	/*
	 * Lengthen the size of the file. We must ensure that the
	 * last byte of the file is allocated. Since the smallest
	 * value of osize is 0, length will be at least 1.
	 */
	if (osize < length) {
#if 0 /* XXX */
		if (length > fs->fs_maxfilesize)
			return (EFBIG);
#endif
		offset = blkoff(fs, length - 1);
		lbn = lblkno(fs, length - 1);
		aflags = B_CLRBUF;
		if (ap->a_flags & IO_SYNC)
			aflags |= B_SYNC;
		error = ext2fs_balloc(oip, lbn, offset + 1, ap->a_cred, &bp,
				   aflags);
		if (error)
			return (error);
		oip->i_e2fs_size = length;
		uvm_vnp_setsize(ovp, length);
		if (aflags & B_SYNC)
			bwrite(bp);
		else
			bawrite(bp);
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(ovp, NULL, NULL, 1));
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
		oip->i_e2fs_size = length;
	} else {
		lbn = lblkno(fs, length);
		aflags = B_CLRBUF;
		if (ap->a_flags & IO_SYNC)
			aflags |= B_SYNC;
		error = ext2fs_balloc(oip, lbn, offset, ap->a_cred, &bp, aflags);
		if (error)
			return (error);
		oip->i_e2fs_size = length;
		size = fs->e2fs_bsize;
		memset((char *)bp->b_data + offset, 0,  (u_int)(size - offset));
		allocbuf(bp, size);
		if (aflags & B_SYNC)
			bwrite(bp);
		else
			bawrite(bp);
	}
	uvm_vnp_setsize(ovp, length);

	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	lastblock = lblkno(fs, length + fs->e2fs_bsize - 1) - 1;
	lastiblock[SINGLE] = lastblock - NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - NINDIR(fs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - NINDIR(fs) * NINDIR(fs);
	nblocks = btodb(fs->e2fs_bsize);
	/*
	 * Update file and block pointers on disk before we start freeing
	 * blocks.  If we crash before free'ing blocks below, the blocks
	 * will be returned to the free list.  lastiblock values are also
	 * normalized to -1 for calls to ext2fs_indirtrunc below.
	 */
	memcpy((caddr_t)oldblks, (caddr_t)&oip->i_e2fs_blocks[0], sizeof oldblks);
	for (level = TRIPLE; level >= SINGLE; level--)
		if (lastiblock[level] < 0) {
			oip->i_e2fs_blocks[NDADDR + level] = 0;
			lastiblock[level] = -1;
		}
	for (i = NDADDR - 1; i > lastblock; i--)
		oip->i_e2fs_blocks[i] = 0;
	oip->i_flag |= IN_CHANGE | IN_UPDATE;
	if ((error = VOP_UPDATE(ovp, NULL, NULL, 1)) != 0)
		allerror = error;
	/*
	 * Having written the new inode to disk, save its new configuration
	 * and put back the old block pointers long enough to process them.
	 * Note that we save the new block configuration so we can check it
	 * when we are done.
	 */
	memcpy((caddr_t)newblks, (caddr_t)&oip->i_e2fs_blocks[0], sizeof newblks);
	memcpy((caddr_t)&oip->i_e2fs_blocks[0], (caddr_t)oldblks, sizeof oldblks);
	oip->i_e2fs_size = osize;
	vflags = ((length > 0) ? V_SAVE : 0) | V_SAVEMETA;
	allerror = vinvalbuf(ovp, vflags, ap->a_cred, ap->a_p, 0, 0);

	/*
	 * Indirect blocks first.
	 */
	indir_lbn[SINGLE] = -NDADDR;
	indir_lbn[DOUBLE] = indir_lbn[SINGLE] - NINDIR(fs) -1;
	indir_lbn[TRIPLE] = indir_lbn[DOUBLE] - NINDIR(fs) * NINDIR(fs) - 1;
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = fs2h32(oip->i_e2fs_blocks[NDADDR + level]);
		if (bn != 0) {
			error = ext2fs_indirtrunc(oip, indir_lbn[level],
				fsbtodb(fs, bn), lastiblock[level], level, &count);
			if (error)
				allerror = error;
			blocksreleased += count;
			if (lastiblock[level] < 0) {
				oip->i_e2fs_blocks[NDADDR + level] = 0;
				ext2fs_blkfree(oip, bn);
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
		bn = fs2h32(oip->i_e2fs_blocks[i]);
		if (bn == 0)
			continue;
		oip->i_e2fs_blocks[i] = 0;
		ext2fs_blkfree(oip, bn);
		blocksreleased += btodb(fs->e2fs_bsize);
	}
	if (lastblock < 0)
		goto done;

done:
#ifdef DIAGNOSTIC
	for (level = SINGLE; level <= TRIPLE; level++)
		if (newblks[NDADDR + level] !=
			fs2h32(oip->i_e2fs_blocks[NDADDR + level]))
			panic("itrunc1");
	for (i = 0; i < NDADDR; i++)
		if (newblks[i] != fs2h32(oip->i_e2fs_blocks[i]))
			panic("itrunc2");
	if (length == 0 &&
		(ovp->v_dirtyblkhd.lh_first || ovp->v_cleanblkhd.lh_first))
		panic("itrunc3");
#endif /* DIAGNOSTIC */
	/*
	 * Put back the real size.
	 */
	oip->i_e2fs_size = length;
	oip->i_e2fs_nblock -= blocksreleased;
	if (oip->i_e2fs_nblock < 0)			/* sanity */
		oip->i_e2fs_nblock = 0;
	oip->i_flag |= IN_CHANGE;
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
ext2fs_indirtrunc(ip, lbn, dbn, lastbn, level, countp)
	register struct inode *ip;
	ufs_daddr_t lbn, lastbn;
	ufs_daddr_t dbn;
	int level;
	long *countp;
{
	register int i;
	struct buf *bp;
	register struct m_ext2fs *fs = ip->i_e2fs;
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
	nblocks = btodb(fs->e2fs_bsize);
	/*
	 * Get buffer of block pointers, zero those entries corresponding
	 * to blocks to be free'd, and update on disk copy first.  Since
	 * double(triple) indirect before single(double) indirect, calls
	 * to bmap on these blocks will fail.  However, we already have
	 * the on disk address, so we have to set the b_blkno field
	 * explicitly instead of letting bread do everything for us.
	 */
	vp = ITOV(ip);
	bp = getblk(vp, lbn, (int)fs->e2fs_bsize, 0, 0);
	if (bp->b_flags & (B_DONE | B_DELWRI)) {
		/* Braces must be here in case trace evaluates to nothing. */
		trace(TR_BREADHIT, pack(vp, fs->e2fs_bsize), lbn);
	} else {
		trace(TR_BREADMISS, pack(vp, fs->e2fs_bsize), lbn);
		curproc->p_stats->p_ru.ru_inblock++;	/* pay for read */
		bp->b_flags |= B_READ;
		if (bp->b_bcount > bp->b_bufsize)
			panic("ext2fs_indirtrunc: bad buffer size");
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
		MALLOC(copy, ufs_daddr_t *, fs->e2fs_bsize, M_TEMP, M_WAITOK);
		memcpy((caddr_t)copy, (caddr_t)bap, (u_int)fs->e2fs_bsize);
		memset((caddr_t)&bap[last + 1], 0,
			(u_int)(NINDIR(fs) - (last + 1)) * sizeof (u_int32_t));
		error = bwrite(bp);
		if (error)
			allerror = error;
		bap = copy;
	}

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = NINDIR(fs) - 1,
		nlbn = lbn + 1 - i * factor; i > last;
		i--, nlbn += factor) {
		nb = fs2h32(bap[i]);
		if (nb == 0)
			continue;
		if (level > SINGLE) {
			error = ext2fs_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
						   (ufs_daddr_t)-1, level - 1,
						   &blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
		ext2fs_blkfree(ip, nb);
		blocksreleased += nblocks;
	}

	/*
	 * Recursively free last partial block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		nb = fs2h32(bap[i]);
		if (nb != 0) {
			error = ext2fs_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
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
