/*	$NetBSD: ext2fs_balloc.c,v 1.4.2.1 2000/06/22 17:10:32 minoura Exp $	*/

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
 *	@(#)ffs_balloc.c	8.4 (Berkeley) 9/23/93
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/vnode.h>

#include <vm/vm.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>

/*
 * Balloc defines the structure of file system storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 */
int
ext2fs_balloc(ip, bn, size, cred, bpp, flags)
	struct inode *ip;
	ufs_daddr_t bn;
	int size;
	struct ucred *cred;
	struct buf **bpp;
	int flags;
{
	struct m_ext2fs *fs;
	ufs_daddr_t nb;
	struct buf *bp, *nbp;
	struct vnode *vp = ITOV(ip);
	struct indir indirs[NIADDR + 2];
	ufs_daddr_t newb, lbn, *bap, pref;
	int num, i, error;
	u_int deallocated;
	ufs_daddr_t *allocib, *blkp, *allocblk, allociblk[NIADDR + 1];
	int unwindidx = -1;

	*bpp = NULL;
	if (bn < 0)
		return (EFBIG);
	fs = ip->i_e2fs;
	lbn = bn;

	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (bn < NDADDR) {
		nb = fs2h32(ip->i_e2fs_blocks[bn]);
		if (nb != 0) {
			error = bread(vp, bn, fs->e2fs_bsize, NOCRED, &bp);
			if (error) {
				brelse(bp);
				return (error);
			}
			*bpp = bp;
			return (0);
		} else {
			error = ext2fs_alloc(ip, bn,
				ext2fs_blkpref(ip, bn, (int)bn, &ip->i_e2fs_blocks[0]),
				cred, &newb);
			if (error)
				return (error);
			ip->i_e2fs_last_lblk = lbn;
			ip->i_e2fs_last_blk = newb;
			bp = getblk(vp, bn, fs->e2fs_bsize, 0, 0);
			bp->b_blkno = fsbtodb(fs, newb);
			if (flags & B_CLRBUF)
				clrbuf(bp);
		}
		ip->i_e2fs_blocks[bn] = h2fs32(dbtofsb(fs, bp->b_blkno));
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		*bpp = bp;
		return (0);
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	pref = 0;
	if ((error = ufs_getlbns(vp, bn, indirs, &num)) != 0)
		return(error);
#ifdef DIAGNOSTIC
	if (num < 1)
		panic ("ext2fs_balloc: ufs_getlbns returned indirect block\n");
#endif
	/*
	 * Fetch the first indirect block allocating if necessary.
	 */
	--num;
	nb = fs2h32(ip->i_e2fs_blocks[NDADDR + indirs[0].in_off]);
	allocib = NULL;
	allocblk = allociblk;
	if (nb == 0) {
		pref = ext2fs_blkpref(ip, lbn, 0, (ufs_daddr_t *)0);
		error = ext2fs_alloc(ip, lbn, pref, cred, &newb);
		if (error)
			return (error);
		nb = newb;
		*allocblk++ = nb;
		ip->i_e2fs_last_blk = newb;
		bp = getblk(vp, indirs[1].in_lbn, fs->e2fs_bsize, 0, 0);
		bp->b_blkno = fsbtodb(fs, newb);
		clrbuf(bp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(bp)) != 0)
			goto fail;
		unwindidx = 0;
		allocib = &ip->i_e2fs_blocks[NDADDR + indirs[0].in_off];
		*allocib = h2fs32(newb);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	for (i = 1;;) {
		error = bread(vp,
		    indirs[i].in_lbn, (int)fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			goto fail;
		}
		bap = (ufs_daddr_t *)bp->b_data;
		nb = fs2h32(bap[indirs[i].in_off]);
		if (i == num)
			break;
		i++;
		if (nb != 0) {
			brelse(bp);
			continue;
		}
		pref = ext2fs_blkpref(ip, lbn, 0, (ufs_daddr_t *)0);
		error = ext2fs_alloc(ip, lbn, pref, cred, &newb);
		if (error) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		ip->i_e2fs_last_blk = newb;
		nbp = getblk(vp, indirs[i].in_lbn, fs->e2fs_bsize, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
		clrbuf(nbp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(nbp)) != 0) {
			brelse(bp);
			goto fail;
		}
		if (unwindidx < 0)
			unwindidx = i - 1;
		bap[indirs[i - 1].in_off] = h2fs32(nb);
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			bdwrite(bp);
		}
	}
	/*
	 * Get the data block, allocating if necessary.
	 */
	if (nb == 0) {
		pref = ext2fs_blkpref(ip, lbn, indirs[num].in_off, &bap[0]);
		error = ext2fs_alloc(ip, lbn, pref, cred, &newb);
		if (error) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		ip->i_e2fs_last_lblk = lbn;
		ip->i_e2fs_last_blk = newb;
		nbp = getblk(vp, lbn, fs->e2fs_bsize, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
		if (flags & B_CLRBUF)
			clrbuf(nbp);
		bap[indirs[num].in_off] = h2fs32(nb);
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			bdwrite(bp);
		}
		*bpp = nbp;
		return (0);
	}
	brelse(bp);
	if (flags & B_CLRBUF) {
		error = bread(vp, lbn, (int)fs->e2fs_bsize, NOCRED, &nbp);
		if (error) {
			brelse(nbp);
			goto fail;
		}
	} else {
		nbp = getblk(vp, lbn, fs->e2fs_bsize, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
	}
	*bpp = nbp;
	return (0);
fail:
	/*
	 * If we have failed part way through block allocation, we
	 * have to deallocate any indirect blocks that we have allocated.
	 */
	for (deallocated = 0, blkp = allociblk; blkp < allocblk; blkp++) {
		ext2fs_blkfree(ip, *blkp);
		deallocated += fs->e2fs_bsize;
	}
	if (unwindidx >= 0) {
		if (unwindidx == 0) {
			*allocib = 0;
		} else {
			int r;
	
			r = bread(vp, indirs[unwindidx].in_lbn, 
			    (int)fs->e2fs_bsize, NOCRED, &bp);
			if (r) {
				panic("Could not unwind indirect block, error %d", r);
				brelse(bp);
			} else {
				bap = (ufs_daddr_t *)bp->b_data;
				bap[indirs[unwindidx].in_off] = 0;
				if (flags & B_SYNC)
					bwrite(bp);
				else
					bdwrite(bp);
			}
		}
		for (i = unwindidx + 1; i <= num; i++) {
			bp = getblk(vp, indirs[i].in_lbn, (int)fs->e2fs_bsize,
			    0, 0);
			bp->b_flags |= B_INVAL;
			brelse(bp);
		}
	}
	if (deallocated) {
		ip->i_e2fs_nblock -= btodb(deallocated);
		ip->i_e2fs_flags |= IN_CHANGE | IN_UPDATE;
	}
	return (error);
}
