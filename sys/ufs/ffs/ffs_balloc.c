/*	$NetBSD: ffs_balloc.c,v 1.24.4.2 2002/01/10 20:05:00 thorpej Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ffs_balloc.c,v 1.24.4.2 2002/01/10 20:05:00 thorpej Exp $");

#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufs_bswap.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <uvm/uvm.h>

/*
 * Balloc defines the structure of file system storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 */
int
ffs_balloc(v)
	void *v;
{
	struct vop_balloc_args /* {
		struct vnode *a_vp;
		off_t a_startoffset;
		int a_size;
		struct ucred *a_cred;
		int a_flags;
		struct buf **a_bpp;
	} */ *ap = v;
	ufs_daddr_t lbn;
	int size;
	struct ucred *cred;
	int flags;
	ufs_daddr_t nb;
	struct buf *bp, *nbp;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct fs *fs = ip->i_fs;
	struct indir indirs[NIADDR + 2];
	ufs_daddr_t newb, *bap, pref;
	int deallocated, osize, nsize, num, i, error;
	ufs_daddr_t *allocib, *blkp, *allocblk, allociblk[NIADDR + 1];
	int unwindidx = -1;
	struct buf **bpp = ap->a_bpp;
#ifdef FFS_EI
	const int needswap = UFS_FSNEEDSWAP(fs);
#endif
	UVMHIST_FUNC("ffs_balloc"); UVMHIST_CALLED(ubchist);

	lbn = lblkno(fs, ap->a_startoffset);
	size = blkoff(fs, ap->a_startoffset) + ap->a_size;
	if (size > fs->fs_bsize)
		panic("ffs_balloc: blk too big");
	if (bpp != NULL) {
		*bpp = NULL;
	}
	UVMHIST_LOG(ubchist, "vp %p lbn 0x%x size 0x%x", vp, lbn, size,0);

	KASSERT(size <= fs->fs_bsize);
	if (lbn < 0)
		return (EFBIG);
	cred = ap->a_cred;
	flags = ap->a_flags;

	/*
	 * If the next write will extend the file into a new block,
	 * and the file is currently composed of a fragment
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
			if (DOINGSOFTDEP(vp))
				softdep_setup_allocdirect(ip, nb, newb,
				    ufs_rw32(ip->i_ffs_db[nb], needswap),
				    fs->fs_bsize, osize, bpp ? *bpp : NULL);
			ip->i_ffs_size = lblktosize(fs, nb + 1);
			uvm_vnp_setsize(vp, ip->i_ffs_size);
			ip->i_ffs_db[nb] = ufs_rw32(newb, needswap);
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			if (bpp) {
				if (flags & B_SYNC)
					bwrite(*bpp);
				else
					bawrite(*bpp);
			}
		}
	}

	/*
	 * The first NDADDR blocks are direct blocks
	 */

	if (lbn < NDADDR) {
		nb = ufs_rw32(ip->i_ffs_db[lbn], needswap);
		if (nb != 0 && ip->i_ffs_size >= lblktosize(fs, lbn + 1)) {

			/*
			 * The block is an already-allocated direct block
			 * and the file already extends past this block,
			 * thus this must be a whole block.
			 * Just read the block (if requested).
			 */

			if (bpp != NULL) {
				error = bread(vp, lbn, fs->fs_bsize, NOCRED,
					      bpp);
				if (error) {
					brelse(*bpp);
					return (error);
				}
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
				 * The existing block is already
				 * at least as big as we want.
				 * Just read the block (if requested).
				 */

				if (bpp != NULL) {
					error = bread(vp, lbn, osize, NOCRED,
						      bpp);
					if (error) {
						brelse(*bpp);
						return (error);
					}
				}
				return 0;
			} else {

				/*
				 * The existing block is smaller than we want,
				 * grow it.
				 */

				error = ffs_realloccg(ip, lbn,
				    ffs_blkpref(ip, lbn, (int)lbn,
					&ip->i_ffs_db[0]), osize, nsize, cred,
					bpp, &newb);
				if (error)
					return (error);
				if (DOINGSOFTDEP(vp))
					softdep_setup_allocdirect(ip, lbn,
					    newb, nb, nsize, osize,
					    bpp ? *bpp : NULL);
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
			if (bpp != NULL) {
				bp = getblk(vp, lbn, nsize, 0, 0);
				bp->b_blkno = fsbtodb(fs, newb);
				if (flags & B_CLRBUF)
					clrbuf(bp);
				*bpp = bp;
			}
			if (DOINGSOFTDEP(vp)) {
				softdep_setup_allocdirect(ip, lbn, newb, 0,
				    nsize, 0, bpp ? *bpp : NULL);
			}
		}
		ip->i_ffs_db[lbn] = ufs_rw32(newb, needswap);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (0);
	}

	/*
	 * Determine the number of levels of indirection.
	 */

	pref = 0;
	if ((error = ufs_getlbns(vp, lbn, indirs, &num)) != 0)
		return (error);

	/*
	 * Fetch the first indirect block allocating if necessary.
	 */

	--num;
	nb = ufs_rw32(ip->i_ffs_ib[indirs[0].in_off], needswap);
	allocib = NULL;
	allocblk = allociblk;
	if (nb == 0) {
		pref = ffs_blkpref(ip, lbn, 0, (ufs_daddr_t *)0);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, cred,
		    &newb);
		if (error)
			goto fail;
		nb = newb;
		*allocblk++ = nb;
		bp = getblk(vp, indirs[1].in_lbn, fs->fs_bsize, 0, 0);
		bp->b_blkno = fsbtodb(fs, nb);
		clrbuf(bp);
		if (DOINGSOFTDEP(vp)) {
			softdep_setup_allocdirect(ip, NDADDR + indirs[0].in_off,
			    newb, 0, fs->fs_bsize, 0, bp);
			bdwrite(bp);
		} else {

			/*
			 * Write synchronously so that indirect blocks
			 * never point at garbage.
			 */

			if ((error = bwrite(bp)) != 0)
				goto fail;
		}
		unwindidx = 0;
		allocib = &ip->i_ffs_ib[indirs[0].in_off];
		*allocib = ufs_rw32(nb, needswap);
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
		nb = ufs_rw32(bap[indirs[i].in_off], needswap);
		if (i == num)
			break;
		i++;
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
		if (DOINGSOFTDEP(vp)) {
			softdep_setup_allocindir_meta(nbp, ip, bp,
			    indirs[i - 1].in_off, nb);
			bdwrite(nbp);
		} else {

			/*
			 * Write synchronously so that indirect blocks
			 * never point at garbage.
			 */

			if ((error = bwrite(nbp)) != 0) {
				brelse(bp);
				goto fail;
			}
		}
		if (unwindidx < 0)
			unwindidx = i - 1;
		bap[indirs[i - 1].in_off] = ufs_rw32(nb, needswap);

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
		pref = ffs_blkpref(ip, lbn, indirs[num].in_off, &bap[0]);
		error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, cred,
		    &newb);
		if (error) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		if (bpp != NULL) {
			nbp = getblk(vp, lbn, fs->fs_bsize, 0, 0);
			nbp->b_blkno = fsbtodb(fs, nb);
			if (flags & B_CLRBUF)
				clrbuf(nbp);
			*bpp = nbp;
		}
		if (DOINGSOFTDEP(vp))
			softdep_setup_allocindir_page(ip, lbn, bp,
			    indirs[num].in_off, nb, 0, bpp ? *bpp : NULL);
		bap[indirs[num].in_off] = ufs_rw32(nb, needswap);
		if (allocib == NULL && unwindidx < 0) {
			unwindidx = i - 1;
		}

		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */

		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			bdwrite(bp);
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

	if (unwindidx >= 0) {

		/*
		 * First write out any buffers we've created to resolve their
		 * softdeps.  This must be done in reverse order of creation
		 * so that we resolve the dependencies in one pass.
		 * Write the cylinder group buffers for these buffers too.
		 */

		for (i = num; i >= unwindidx; i--) {
			if (i == 0) {
				break;
			}
			bp = getblk(vp, indirs[i].in_lbn, (int)fs->fs_bsize, 0,
			    0);
			if (bp->b_flags & B_DELWRI) {
				nb = fsbtodb(fs, cgtod(fs, dtog(fs,
				    bp->b_blkno)));
				bwrite(bp);
				bp = getblk(ip->i_devvp, nb, (int)fs->fs_cgsize,
				    0, 0);
				if (bp->b_flags & B_DELWRI) {
					bwrite(bp);
				} else {
					bp->b_flags |= B_INVAL;
					brelse(bp);
				}
			} else {
				bp->b_flags |= B_INVAL;
				brelse(bp);
			}
		}
		if (unwindidx == 0) {
			ip->i_flag |= IN_MODIFIED | IN_CHANGE | IN_UPDATE;
			VOP_UPDATE(vp, NULL, NULL, UPDATE_WAIT);
		}

		/*
		 * Now that any dependencies that we created have been
		 * resolved, we can undo the partial allocation.
		 */

		if (unwindidx == 0) {
			*allocib = 0;
			ip->i_flag |= IN_MODIFIED | IN_CHANGE | IN_UPDATE;
			VOP_UPDATE(vp, NULL, NULL, UPDATE_WAIT);
		} else {
			int r;

			r = bread(vp, indirs[unwindidx].in_lbn,
			    (int)fs->fs_bsize, NOCRED, &bp);
			if (r) {
				panic("Could not unwind indirect block, error %d", r);
				brelse(bp);
			} else {
				bap = (ufs_daddr_t *)bp->b_data;
				bap[indirs[unwindidx].in_off] = 0;
				bwrite(bp);
			}
		}
		for (i = unwindidx + 1; i <= num; i++) {
			bp = getblk(vp, indirs[i].in_lbn, (int)fs->fs_bsize, 0,
			    0);
			bp->b_flags |= B_INVAL;
			brelse(bp);
		}
	}
	for (deallocated = 0, blkp = allociblk; blkp < allocblk; blkp++) {
		ffs_blkfree(ip, *blkp, fs->fs_bsize);
		deallocated += fs->fs_bsize;
	}
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


int
ffs_gop_alloc(struct vnode *vp, off_t off, off_t len, int flags,
    struct ucred *cred)
{
	struct inode *ip = VTOI(vp);
	struct fs *fs = ip->i_fs;
	int error, delta, bshift, bsize;
	UVMHIST_FUNC("ffs_gop_alloc"); UVMHIST_CALLED(ubchist);

	error = 0;
	bshift = fs->fs_bshift;
	bsize = 1 << bshift;

	delta = off & (bsize - 1);
	off -= delta;
	len += delta;

	while (len > 0) {
		bsize = MIN(bsize, len);

		error = VOP_BALLOC(vp, off, bsize, cred, flags, NULL);
		if (error) {
			goto out;
		}

		/*
		 * increase file size now, VOP_BALLOC() requires that
		 * EOF be up-to-date before each call.
		 */

		if (ip->i_ffs_size < off + bsize) {
			UVMHIST_LOG(ubchist, "vp %p old 0x%x new 0x%x",
			    vp, ip->i_ffs_size, off + bsize, 0);
			ip->i_ffs_size = off + bsize;
		}

		off += bsize;
		len -= bsize;
	}

out:
	return error;
}
