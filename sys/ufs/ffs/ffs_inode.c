/*	$NetBSD: ffs_inode.c,v 1.5.2.1 1994/10/19 04:29:45 cgd Exp $	*/

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
 *	@(#)ffs_inode.c	8.5 (Berkeley) 12/30/93
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

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

static int ffs_indirtrunc __P((struct inode *, daddr_t, daddr_t, daddr_t, int,
	    long *));

int
ffs_init()
{
	return (ufs_init());
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
ffs_update(ap)
	struct vop_update_args /* {
		struct vnode *a_vp;
		struct timeval *a_access;
		struct timeval *a_modify;
		int a_waitfor;
	} */ *ap;
{
	register struct fs *fs;
	struct buf *bp;
	struct inode *ip;
	int error;

	ip = VTOI(ap->a_vp);
	if (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY) {
		ip->i_flag &=
		    ~(IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE);
		return (0);
	}
	if ((ip->i_flag &
	    (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0)
		return (0);
	if (ip->i_flag & IN_ACCESS)
		ip->i_atime.ts_sec = ap->a_access->tv_sec;
	if (ip->i_flag & IN_UPDATE) {
		ip->i_mtime.ts_sec = ap->a_modify->tv_sec;
		ip->i_modrev++;
	}
	if (ip->i_flag & IN_CHANGE)
		ip->i_ctime.ts_sec = time.tv_sec;
	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE);
	fs = ip->i_fs;
	/*
	 * Ensure that uid and gid are correct. This is a temporary
	 * fix until fsck has been changed to do the update.
	 */
	if (fs->fs_inodefmt < FS_44INODEFMT) {		/* XXX */
		ip->i_din.di_ouid = ip->i_uid;		/* XXX */
		ip->i_din.di_ogid = ip->i_gid;		/* XXX */
	}						/* XXX */
	if (error = bread(ip->i_devvp,
	    fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
		(int)fs->fs_bsize, NOCRED, &bp)) {
		brelse(bp);
		return (error);
	}
	*((struct dinode *)bp->b_data +
	    ino_to_fsbo(fs, ip->i_number)) = ip->i_din;
	if (ap->a_waitfor)
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
ffs_truncate(ap)
	struct vop_truncate_args /* {
		struct vnode *a_vp;
		off_t a_length;
		int a_flags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *ovp = ap->a_vp;
	register daddr_t lastblock;
	register struct inode *oip;
	daddr_t bn, lbn, lastiblock[NIADDR], indir_lbn[NIADDR];
	daddr_t oldblks[NDADDR + NIADDR], newblks[NDADDR + NIADDR];
	off_t length = ap->a_length;
	register struct fs *fs;
	struct buf *bp;
	int offset, size, level;
	long count, nblocks, vflags, blocksreleased = 0;
	struct timeval tv;
	register int i;
	int aflags, error, allerror;
	off_t osize;

	if (length < 0 || length > 0xffffffffLL)
		return (EINVAL);
	oip = VTOI(ovp);
	tv = time;
	if (ovp->v_type == VLNK &&
	    (oip->i_size < ovp->v_mount->mnt_maxsymlinklen ||
	     (ovp->v_mount->mnt_maxsymlinklen == 0 &&
	      oip->i_din.di_blocks == 0))) {
#ifdef DIAGNOSTIC
		if (length != 0)
			panic("ffs_truncate: partial truncate of symlink");
#endif
		bzero((char *)&oip->i_shortlink, (u_int)oip->i_size);
		oip->i_size = 0;
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(ovp, &tv, &tv, 1));
	}
	if (oip->i_size == length) {
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(ovp, &tv, &tv, 0));
	}
#ifdef QUOTA
	if (error = getinoquota(oip))
		return (error);
#endif
	vnode_pager_setsize(ovp, (u_long)length);
	fs = oip->i_fs;
	osize = oip->i_size;
	/*
	 * Lengthen the size of the file. We must ensure that the
	 * last byte of the file is allocated. Since the smallest
	 * value of oszie is 0, length will be at least 1.
	 */
	if (osize < length) {
		offset = blkoff(fs, length - 1);
		lbn = lblkno(fs, length - 1);
		aflags = B_CLRBUF;
		if (ap->a_flags & IO_SYNC)
			aflags |= B_SYNC;
		if (error = ffs_balloc(oip, lbn, offset + 1, ap->a_cred, &bp,
		    aflags))
			return (error);
		oip->i_size = length;
		(void) vnode_pager_uncache(ovp);
		if (aflags & B_SYNC)
			bwrite(bp);
		else
			bawrite(bp);
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (VOP_UPDATE(ovp, &tv, &tv, 1));
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
		oip->i_size = length;
	} else {
		lbn = lblkno(fs, length);
		aflags = B_CLRBUF;
		if (ap->a_flags & IO_SYNC)
			aflags |= B_SYNC;
		if (error = ffs_balloc(oip, lbn, offset, ap->a_cred, &bp,
		    aflags))
			return (error);
		oip->i_size = length;
		size = blksize(fs, oip, lbn);
		(void) vnode_pager_uncache(ovp);
		bzero((char *)bp->b_data + offset, (u_int)(size - offset));
		allocbuf(bp, size);
		if (aflags & B_SYNC)
			bwrite(bp);
		else
			bawrite(bp);
	}
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
	bcopy((caddr_t)&oip->i_db[0], (caddr_t)oldblks, sizeof oldblks);
	for (level = TRIPLE; level >= SINGLE; level--)
		if (lastiblock[level] < 0) {
			oip->i_ib[level] = 0;
			lastiblock[level] = -1;
		}
	for (i = NDADDR - 1; i > lastblock; i--)
		oip->i_db[i] = 0;
	oip->i_flag |= IN_CHANGE | IN_UPDATE;
	if (error = VOP_UPDATE(ovp, &tv, &tv, MNT_WAIT))
		allerror = error;
	/*
	 * Having written the new inode to disk, save its new configuration
	 * and put back the old block pointers long enough to process them.
	 * Note that we save the new block configuration so we can check it
	 * when we are done.
	 */
	bcopy((caddr_t)&oip->i_db[0], (caddr_t)newblks, sizeof newblks);
	bcopy((caddr_t)oldblks, (caddr_t)&oip->i_db[0], sizeof oldblks);
	oip->i_size = osize;
	vflags = ((length > 0) ? V_SAVE : 0) | V_SAVEMETA;
	allerror = vinvalbuf(ovp, vflags, ap->a_cred, ap->a_p, 0, 0);

	/*
	 * Indirect blocks first.
	 */
	indir_lbn[SINGLE] = -NDADDR;
	indir_lbn[DOUBLE] = indir_lbn[SINGLE] - NINDIR(fs) - 1;
	indir_lbn[TRIPLE] = indir_lbn[DOUBLE] - NINDIR(fs) * NINDIR(fs) - 1;
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = oip->i_ib[level];
		if (bn != 0) {
			error = ffs_indirtrunc(oip, indir_lbn[level],
			    fsbtodb(fs, bn), lastiblock[level], level, &count);
			if (error)
				allerror = error;
			blocksreleased += count;
			if (lastiblock[level] < 0) {
				oip->i_ib[level] = 0;
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

		bn = oip->i_db[i];
		if (bn == 0)
			continue;
		oip->i_db[i] = 0;
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
	bn = oip->i_db[lastblock];
	if (bn != 0) {
		long oldspace, newspace;

		/*
		 * Calculate amount of space we're giving
		 * back as old block size minus new block size.
		 */
		oldspace = blksize(fs, oip, lastblock);
		oip->i_size = length;
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
		if (newblks[NDADDR + level] != oip->i_ib[level])
			panic("itrunc1");
	for (i = 0; i < NDADDR; i++)
		if (newblks[i] != oip->i_db[i])
			panic("itrunc2");
	if (length == 0 &&
	    (ovp->v_dirtyblkhd.lh_first || ovp->v_cleanblkhd.lh_first))
		panic("itrunc3");
#endif /* DIAGNOSTIC */
	/*
	 * Put back the real size.
	 */
	oip->i_size = length;
	oip->i_blocks -= blocksreleased;
	if (oip->i_blocks < 0)			/* sanity */
		oip->i_blocks = 0;
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
	daddr_t lbn, lastbn;
	daddr_t dbn;
	int level;
	long *countp;
{
	register int i;
	struct buf *bp;
	register struct fs *fs = ip->i_fs;
	register daddr_t *bap;
	struct vnode *vp;
	daddr_t *copy, nb, nlbn, last;
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

	bap = (daddr_t *)bp->b_data;
	MALLOC(copy, daddr_t *, fs->fs_bsize, M_TEMP, M_WAITOK);
	bcopy((caddr_t)bap, (caddr_t)copy, (u_int)fs->fs_bsize);
	bzero((caddr_t)&bap[last + 1],
	  (u_int)(NINDIR(fs) - (last + 1)) * sizeof (daddr_t));
	if (last == -1)
		bp->b_flags |= B_INVAL;
	error = bwrite(bp);
	if (error)
		allerror = error;
	bap = copy;

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = NINDIR(fs) - 1, nlbn = lbn + 1 - i * factor; i > last;
	    i--, nlbn += factor) {
		nb = bap[i];
		if (nb == 0)
			continue;
		if (level > SINGLE) {
			if (error = ffs_indirtrunc(ip, nlbn,
			    fsbtodb(fs, nb), (daddr_t)-1, level - 1, &blkcount))
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
		nb = bap[i];
		if (nb != 0) {
			if (error = ffs_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
			    last, level - 1, &blkcount))
				allerror = error;
			blocksreleased += blkcount;
		}
	}
	FREE(copy, M_TEMP);
	*countp = blocksreleased;
	return (allerror);
}
