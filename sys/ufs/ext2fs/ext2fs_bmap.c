/*	$NetBSD: ext2fs_bmap.c,v 1.4.14.1 2000/11/20 18:11:41 bouyer Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.
 * Copyright (c) 1989, 1991, 1993
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
 *	@(#)ufs_bmap.c	8.6 (Berkeley) 1/21/94
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/resourcevar.h>
#include <sys/trace.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>

static int ext2fs_bmaparray __P((struct vnode *, ufs_daddr_t, ufs_daddr_t *,
								struct indir *, int *, int *));

/*
 * Bmap converts a the logical block number of a file to its physical block
 * number on the disk. The conversion is done by using the logical block
 * number to index into the array of block pointers described by the dinode.
 */
int
ext2fs_bmap(v)
	void *v;
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;
	/*
	 * Check for underlying vnode requests and ensure that logical
	 * to physical mapping is requested.
	 */
	if (ap->a_vpp != NULL)
		*ap->a_vpp = VTOI(ap->a_vp)->i_devvp;
	if (ap->a_bnp == NULL)
		return (0);

	return (ext2fs_bmaparray(ap->a_vp, ap->a_bn, ap->a_bnp, NULL, NULL,
		ap->a_runp));
}

/*
 * Indirect blocks are now on the vnode for the file.  They are given negative
 * logical block numbers.  Indirect blocks are addressed by the negative
 * address of the first data block to which they point.  Double indirect blocks
 * are addressed by one less than the address of the first indirect block to
 * which they point.  Triple indirect blocks are addressed by one less than
 * the address of the first double indirect block to which they point.
 *
 * ext2fs_bmaparray does the bmap conversion, and if requested returns the
 * array of logical blocks which must be traversed to get to a block.
 * Each entry contains the offset into that block that gets you to the
 * next block and the disk address of the block (if it is assigned).
 */

int
ext2fs_bmaparray(vp, bn, bnp, ap, nump, runp)
	struct vnode *vp;
	ufs_daddr_t bn;
	ufs_daddr_t *bnp;
	struct indir *ap;
	int *nump;
	int *runp;
{
	struct inode *ip;
	struct buf *bp;
	struct ufsmount *ump;
	struct mount *mp;
	struct vnode *devvp;
	struct indir a[NIADDR+1], *xap;
	ufs_daddr_t daddr;
	long metalbn;
	int error, maxrun = 0, num;

	ip = VTOI(vp);
	mp = vp->v_mount;
	ump = VFSTOUFS(mp);
#ifdef DIAGNOSTIC
	if ((ap != NULL && nump == NULL) || (ap == NULL && nump != NULL))
		panic("ext2fs_bmaparray: invalid arguments");
#endif

	if (runp) {
		/*
		 * XXX
		 * If MAXBSIZE is the largest transfer the disks can handle,
		 * we probably want maxrun to be 1 block less so that we
		 * don't create a block larger than the device can handle.
		 */
		*runp = 0;
		maxrun = MAXBSIZE / mp->mnt_stat.f_iosize - 1;
	}

	xap = ap == NULL ? a : ap;
	if (!nump)
		nump = &num;
	if ((error = ufs_getlbns(vp, bn, xap, nump)) != 0)
		return (error);

	num = *nump;
	if (num == 0) {
		*bnp = blkptrtodb(ump, fs2h32(ip->i_e2fs_blocks[bn]));
		if (*bnp == 0)
			*bnp = -1;
		else if (runp)
			for (++bn; bn < NDADDR && *runp < maxrun &&
				is_sequential(ump, fs2h32(ip->i_e2fs_blocks[bn - 1]),
							  fs2h32(ip->i_e2fs_blocks[bn]));
				++bn, ++*runp);
		return (0);
	}


	/* Get disk address out of indirect block array */
	daddr = fs2h32(ip->i_e2fs_blocks[NDADDR + xap->in_off]);

	devvp = VFSTOUFS(vp->v_mount)->um_devvp;

#ifdef DIAGNOSTIC
    if (num > NIADDR + 1 || num < 1) {
		printf("ext2fs_bmaparray: num=%d\n", num);
		panic("ext2fs_bmaparray: num");
	}
#endif
	for (bp = NULL, ++xap; --num; ++xap) {
		/* 
		 * Exit the loop if there is no disk address assigned yet and
		 * the indirect block isn't in the cache, or if we were
		 * looking for an indirect block and we've found it.
		 */

		metalbn = xap->in_lbn;
		if ((daddr == 0 && !incore(vp, metalbn)) || metalbn == bn)
			break;
		/*
		 * If we get here, we've either got the block in the cache
		 * or we have a disk address for it, go fetch it.
		 */
		if (bp)
			brelse(bp);

		xap->in_exists = 1;
		bp = getblk(vp, metalbn, mp->mnt_stat.f_iosize, 0, 0);
		if (bp->b_flags & (B_DONE | B_DELWRI)) {
			trace(TR_BREADHIT, pack(vp, size), metalbn);
		}
#ifdef DIAGNOSTIC
		else if (!daddr)
			panic("ext2fs_bmaparry: indirect block not in cache");
#endif
		else {
			trace(TR_BREADMISS, pack(vp, size), metalbn);
			bp->b_blkno = blkptrtodb(ump, daddr);
			bp->b_flags |= B_READ;
			VOP_STRATEGY(bp);
			curproc->p_stats->p_ru.ru_inblock++;	/* XXX */
			if ((error = biowait(bp)) != 0) {
				brelse(bp);
				return (error);
			}
		}

		daddr = fs2h32(((ufs_daddr_t *)bp->b_data)[xap->in_off]);
		if (num == 1 && daddr && runp)
			for (bn = xap->in_off + 1;
				bn < MNINDIR(ump) && *runp < maxrun &&
				is_sequential(ump, ((ufs_daddr_t *)bp->b_data)[bn - 1],
				((ufs_daddr_t *)bp->b_data)[bn]);
				++bn, ++*runp);
	}
	if (bp)
		brelse(bp);

	daddr = blkptrtodb(ump, daddr);
	*bnp = daddr == 0 ? -1 : daddr;
	return (0);
}
