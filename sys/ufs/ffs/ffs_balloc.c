/*	$NetBSD: ffs_balloc.c,v 1.13.2.1 1998/11/09 06:06:35 chs Exp $	*/

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
 *	@(#)ffs_balloc.c	8.8 (Berkeley) 6/16/95
 */

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_quota.h"
#include "opt_uvm.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <vm/vm.h>

#if defined(UVM)
#include <uvm/uvm.h>
#endif

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufs_bswap.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

/*
 * Balloc defines the structure of file system storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 */
int
ffs_balloc(ip, lbn, size, cred, bpp, flags)
	register struct inode *ip;
	register ufs_daddr_t lbn;
	int size;
	struct ucred *cred;
	struct buf **bpp;
	int flags;
{
	register struct fs *fs;
	register ufs_daddr_t nb;
	struct buf *bp, *nbp;
	struct vnode *vp = ITOV(ip);
	struct indir indirs[NIADDR + 2];
	ufs_daddr_t newb, *bap, pref;
	int deallocated, osize, nsize, num, i, error;
	ufs_daddr_t *allocib, *blkp, *allocblk, allociblk[NIADDR + 1];

	if (bpp != NULL) {
		*bpp = NULL;
	}

	if (lbn < 0)
		return (EFBIG);
	fs = ip->i_fs;

	/*
	 * If the file currently ends with a fragment and
	 * the block we're allocating now is after the current EOF,
	 * this fragment has to be extended to be a full block.
	 */
	nb = lblkno(fs, ip->i_ffs_size);
	if (nb < NDADDR && nb < lbn) {
		osize = blksize(fs, ip, nb);
		if (osize < fs->fs_bsize && osize > 0) {
			error = ffs_realloccg(ip, nb,
				ffs_blkpref(ip, nb, (int)nb, &ip->i_ffs_db[0]),
				osize, (int)fs->fs_bsize, cred, bpp, &newb);
			if (error)
				return (error);
			ip->i_ffs_size = lblktosize(fs, nb + 1);
#if defined(UVM)
			uvm_vnp_setsize(vp, ip->i_ffs_size);
#else
			vnode_pager_setsize(vp, ip->i_ffs_size);
#endif
			ip->i_ffs_db[nb] = ufs_rw32(newb,
			    UFS_MPNEEDSWAP(vp->v_mount));
			ip->i_flag |= IN_CHANGE | IN_UPDATE;

			if (bpp) {
				if (flags & B_SYNC)
					bwrite(*bpp);
				else
					bawrite(*bpp);
			}
			else {
				/*
				 * XXX the data in the frag might be
				 * moving to a new disk location.
				 * we need to flush pages to the
				 * new disk locations.
				 * XXX we could do this in realloccg
				 * except for the sync flag.
				 */
				(vp->v_uvm.u_obj.pgops->pgo_flush)
					(&vp->v_uvm.u_obj, lblktosize(fs, nb),
					 lblktosize(fs, nb + 1),
					 flags & B_SYNC ? PGO_SYNCIO : 0);
			}
		}
	}
	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (lbn < NDADDR) {

		nb = ufs_rw32(ip->i_ffs_db[lbn], UFS_MPNEEDSWAP(vp->v_mount));
		if (nb != 0 && ip->i_ffs_size >= lblktosize(fs, lbn + 1)) {

			/*
			 * the block is an already-allocated direct block
			 * and the file already extends past this block,
			 * thus this must be a whole block.
			 * just read the block (if requested).
			 */

justread:
			if (bpp != NULL) {
				error = bread(vp, lbn, fs->fs_bsize, NOCRED,
					      &bp);
				if (error) {
					brelse(bp);
					return (error);
				}
				*bpp = bp;
			}
			return (0);
		}
		if (nb != 0) {
			/*
			 * Consider need to reallocate a fragment.
			 */
			osize = fragroundup(fs, blkoff(fs, ip->i_ffs_size));
			nsize = fragroundup(fs, size);
			if (nsize <= osize) {

				/*
				 * the existing block is already
				 * at least as big as we want.
				 * just read the block (if requested).
				 */

				goto justread;
			} else {

				/*
				 * the existing block is smaller than we want,
				 * grow it.
				 */

				error = ffs_realloccg(ip, lbn,
				    ffs_blkpref(ip, lbn, (int)lbn,
					&ip->i_ffs_db[0]), osize, nsize, cred,
					bpp, &newb);
				if (error)
					return (error);
				ip->i_ffs_db[lbn] = ufs_rw32(newb,
					UFS_MPNEEDSWAP(vp->v_mount));
				ip->i_flag |= IN_CHANGE | IN_UPDATE;
			}
		} else {

			/*
			 * the block was not previously allocated,
			 * allocate a new block or fragment.
			 */

			if (ip->i_ffs_size < lblktosize(fs, lbn + 1))
				nsize = fragroundup(fs, size);
			else
				nsize = fs->fs_bsize;
			error = ffs_alloc(ip, lbn,
			    ffs_blkpref(ip, lbn, (int)lbn, &ip->i_ffs_db[0]),
				nsize, cred, &newb);
			if (error)
				return (error);

			ip->i_ffs_db[lbn] = ufs_rw32(newb,
				UFS_MPNEEDSWAP(vp->v_mount));
			ip->i_flag |= IN_CHANGE | IN_UPDATE;

			if (bpp != NULL) {
				bp = getblk(vp, lbn, nsize, 0, 0);
				bp->b_blkno = fsbtodb(fs, newb);
				if (flags & B_CLRBUF)
					clrbuf(bp);
				*bpp = bp;
			}
		}
		return (0);
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	pref = 0;
	if ((error = ufs_getlbns(vp, lbn, indirs, &num)) != 0)
		return(error);
#ifdef DIAGNOSTIC
	if (num < 1)
		panic ("ffs_balloc: ufs_bmaparray returned indirect block\n");
#endif
	/*
	 * Fetch the first indirect block allocating if necessary.
	 */
	--num;
	nb = ufs_rw32(ip->i_ffs_ib[indirs[0].in_off],
	    UFS_MPNEEDSWAP(vp->v_mount));
	allocib = NULL;
	allocblk = allociblk;
	if (nb == 0) {
		pref = ffs_blkpref(ip, lbn, 0, (ufs_daddr_t *)0);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize,
			cred, &newb);
		if (error)
			return (error);
		nb = newb;
		*allocblk++ = nb;
		bp = getblk(vp, indirs[1].in_lbn, fs->fs_bsize, 0, 0);
		bp->b_blkno = fsbtodb(fs, nb);
		clrbuf(bp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(bp)) != 0)
			goto fail;
		allocib = &ip->i_ffs_ib[indirs[0].in_off];
		*allocib = ufs_rw32(nb, UFS_MPNEEDSWAP(vp->v_mount));
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	for (i = 1;;) {
		error = bread(vp,
		    indirs[i].in_lbn, (int)fs->fs_bsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			goto fail;
		}
		bap = (ufs_daddr_t *)bp->b_data;
		nb = ufs_rw32(bap[indirs[i].in_off],
		    UFS_MPNEEDSWAP(vp->v_mount));
		if (i == num)
			break;
		i += 1;
		if (nb != 0) {
			brelse(bp);
			continue;
		}
		if (pref == 0)
			pref = ffs_blkpref(ip, lbn, 0, (ufs_daddr_t *)0);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, cred,
				  &newb);
		if (error) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		nbp = getblk(vp, indirs[i].in_lbn, fs->fs_bsize, 0, 0);
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
		bap[indirs[i - 1].in_off] = ufs_rw32(nb,
		    UFS_MPNEEDSWAP(vp->v_mount));
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
		pref = ffs_blkpref(ip, lbn, indirs[i].in_off, &bap[0]);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, cred,
				  &newb);
		if (error) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		bap[indirs[i].in_off] = ufs_rw32(nb,
		    UFS_MPNEEDSWAP(vp->v_mount));
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			bdwrite(bp);
		}
		if (bpp != NULL) {
			nbp = getblk(vp, lbn, fs->fs_bsize, 0, 0);
			nbp->b_blkno = fsbtodb(fs, nb);
			if (flags & B_CLRBUF)
				clrbuf(nbp);
			*bpp = nbp;
		}
		return (0);
	}

	brelse(bp);

	if (bpp != NULL) {
		if (flags & B_CLRBUF) {
			error = bread(vp, lbn, (int)fs->fs_bsize, NOCRED, &nbp);
			if (error) {
				brelse(nbp);
				goto fail;
			}
		} else {
			nbp = getblk(vp, lbn, fs->fs_bsize, 0, 0);
			nbp->b_blkno = fsbtodb(fs, nb);
			clrbuf(nbp);
		}
		*bpp = nbp;
	}

	return (0);
fail:
	/*
	 * If we have failed part way through block allocation, we
	 * have to deallocate any indirect blocks that we have allocated.
	 */
	for (deallocated = 0, blkp = allociblk; blkp < allocblk; blkp++) {
		ffs_blkfree(ip, *blkp, fs->fs_bsize);
		deallocated += fs->fs_bsize;
	}
	if (allocib != NULL)
		*allocib = 0;
	if (deallocated) {
#ifdef QUOTA
		/*
		 * Restore user's disk quota because allocation failed.
		 */
		(void)chkdq(ip, (long)-btodb(deallocated), cred, FORCE);
#endif
		ip->i_ffs_blocks -= btodb(deallocated);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	return (error);
}
