/*	$NetBSD: lfs_balloc.c,v 1.27.2.4 2001/11/14 19:18:58 nathanw Exp $	*/

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Konrad E. Schroder <perseant@hhhh.org>.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1989, 1991, 1993
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
 *	@(#)lfs_balloc.c	8.4 (Berkeley) 5/8/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_balloc.c,v 1.27.2.4 2001/11/14 19:18:58 nathanw Exp $");

#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#endif

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

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

int lfs_fragextend(struct vnode *, int, int, ufs_daddr_t, struct buf **, struct ucred *);

/*
 * Allocate a block, and to inode and filesystem block accounting for it
 * and for any indirect blocks the may need to be created in order for
 * this block to be created.
 *
 * Blocks which have never been accounted for (i.e., which "do not exist")
 * have disk address 0, which is translated by ufs_bmap to the special value
 * UNASSIGNED == -1, as in the historical UFS.
 * 
 * Blocks which have been accounted for but which have not yet been written
 * to disk are given the new special disk address UNWRITTEN == -2, so that
 * they can be differentiated from completely new blocks.
 */
/* VOP_BWRITE NIADDR+2 times */
int
lfs_balloc(void *v)
{
	struct vop_balloc_args /* {
		struct vnode *a_vp;
		off_t a_startoffset;
		int a_size;
		struct ucred *a_cred;
		int a_flags;
		struct buf *a_bpp;
	} */ *ap = v;
	struct vnode *vp;
	int offset;
	u_long iosize;
	daddr_t daddr, idaddr;
	struct buf *ibp, *bp;
	struct inode *ip;
	struct lfs *fs;
	struct indir indirs[NIADDR+2], *idp;
	ufs_daddr_t	lbn, lastblock;
	int bb, bcount;
	int error, frags, i, nsize, osize, num;

	vp = ap->a_vp;	
	ip = VTOI(vp);
	fs = ip->i_lfs;
	offset = blkoff(fs, ap->a_startoffset);
	iosize = ap->a_size;
	lbn = lblkno(fs, ap->a_startoffset);
	(void)lfs_check(vp, lbn, 0);
	
	/* 
	 * Three cases: it's a block beyond the end of file, it's a block in
	 * the file that may or may not have been assigned a disk address or
	 * we're writing an entire block.
	 *
	 * Note, if the daddr is UNWRITTEN, the block already exists in
	 * the cache (it was read or written earlier).  If so, make sure
	 * we don't count it as a new block or zero out its contents. If
	 * it did not, make sure we allocate any necessary indirect
	 * blocks.
	 *
	 * If we are writing a block beyond the end of the file, we need to
	 * check if the old last block was a fragment.	If it was, we need
	 * to rewrite it.
	 */
	
	*ap->a_bpp = NULL;
	
	/* Check for block beyond end of file and fragment extension needed. */
	lastblock = lblkno(fs, ip->i_ffs_size);
	if (lastblock < NDADDR && lastblock < lbn) {
		osize = blksize(fs, ip, lastblock);
		if (osize < fs->lfs_bsize && osize > 0) {
			if ((error = lfs_fragextend(vp, osize, fs->lfs_bsize,
						    lastblock, &bp,
						    ap->a_cred)))
				return(error);
			ip->i_ffs_size = (lastblock + 1) * fs->lfs_bsize;
			uvm_vnp_setsize(vp, ip->i_ffs_size);
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			(void) VOP_BWRITE(bp);
		}
	}

	/*
	 * If the block we are writing is a direct block, it's the last
	 * block in the file, and offset + iosize is less than a full
	 * block, we can write one or more fragments.  There are two cases:
	 * the block is brand new and we should allocate it the correct
	 * size or it already exists and contains some fragments and
	 * may need to extend it.
	 */
	if (lbn < NDADDR && lblkno(fs, ip->i_ffs_size) <= lbn) {
		osize = blksize(fs, ip, lbn);
		nsize = fragroundup(fs, offset + iosize);
		if (lblktosize(fs, lbn) >= ip->i_ffs_size) {
			/* Brand new block or fragment */
			frags = numfrags(fs, nsize);
			bb = fragstofsb(fs, frags);
			*ap->a_bpp = bp = getblk(vp, lbn, nsize, 0, 0);
			ip->i_lfs_effnblks += bb;
			ip->i_lfs->lfs_bfree -= bb;
			ip->i_ffs_db[lbn] = bp->b_blkno = UNWRITTEN;
		} else {
			if (nsize <= osize) {
				/* No need to extend */
				if ((error = bread(vp, lbn, osize, NOCRED, &bp)))
					return error;
			} else {
				/* Extend existing block */
				if ((error =
				     lfs_fragextend(vp, osize, nsize, lbn, &bp,
						    ap->a_cred)))
					return error;
			}
			*ap->a_bpp = bp;
		}
		return 0;
	}

	error = ufs_bmaparray(vp, lbn, &daddr, &indirs[0], &num, NULL );
	if (error)
		return (error);
	/*
	 * Do byte accounting all at once, so we can gracefully fail *before*
	 * we start assigning blocks.
	 */
	bb = VFSTOUFS(vp->v_mount)->um_seqinc;
	bcount = 0;
	if (daddr == UNASSIGNED) {
		bcount = bb;
	}
	for (i = 1; i < num; ++i) {
		if (!indirs[i].in_exists) {
			bcount += bb;
		}
	}
	if (ISSPACE(fs, bcount, ap->a_cred)) {
		ip->i_lfs->lfs_bfree -= bcount;
		ip->i_lfs_effnblks += bcount;
	} else {
		return ENOSPC;
	}

	if (daddr == UNASSIGNED) {
		if (num > 0 && ip->i_ffs_ib[indirs[0].in_off] == 0) {
			ip->i_ffs_ib[indirs[0].in_off] = UNWRITTEN;
		}

		/*
		 * Create new indirect blocks if necessary
		 */
		if (num > 1)
			idaddr = ip->i_ffs_ib[indirs[0].in_off];
		for (i = 1; i < num; ++i) {
			ibp = getblk(vp, indirs[i].in_lbn, fs->lfs_bsize, 0,0);
			if (!indirs[i].in_exists) {
				clrbuf(ibp);
				ibp->b_blkno = UNWRITTEN;
			} else if (!(ibp->b_flags & (B_DELWRI | B_DONE))) {
				ibp->b_blkno = fsbtodb(fs, idaddr);
				ibp->b_flags |= B_READ;
				VOP_STRATEGY(ibp);
				biowait(ibp);
			}
			/*
			 * This block exists, but the next one may not.
			 * If that is the case mark it UNWRITTEN to keep
			 * the accounting straight.
			 */
			if (((daddr_t *)ibp->b_data)[indirs[i].in_off]==0)
				((daddr_t *)ibp->b_data)[indirs[i].in_off] =
					UNWRITTEN;
			idaddr = ((daddr_t *)ibp->b_data)[indirs[i].in_off];
			if ((error = VOP_BWRITE(ibp))) {
				return error;
			}
		}
	}	


	/*
	 * Get the existing block from the cache.
	 */
	frags = fsbtofrags(fs, bb);
	*ap->a_bpp = bp = getblk(vp, lbn, blksize(fs, ip, lbn), 0, 0);
	
	/* 
	 * The block we are writing may be a brand new block
	 * in which case we need to do accounting.
	 *
	 * We can tell a truly new block because ufs_bmaparray will say
	 * it is UNASSIGNED.  Once we allocate it we will assign it the
	 * disk address UNWRITTEN.
	 */
	if (daddr == UNASSIGNED) {
		if (iosize != fs->lfs_bsize)
			clrbuf(bp);
		
		/* Note the new address */
		bp->b_blkno = UNWRITTEN;
		
		switch (num) {
		    case 0:
			ip->i_ffs_db[lbn] = UNWRITTEN;
			break;
		    case 1:
			ip->i_ffs_ib[indirs[0].in_off] = UNWRITTEN;
			break;
		    default:
			idp = &indirs[num - 1];
			if (bread(vp, idp->in_lbn, fs->lfs_bsize, NOCRED,
				  &ibp))
				panic("lfs_balloc: bread bno %d", idp->in_lbn);
			((ufs_daddr_t *)ibp->b_data)[idp->in_off] = UNWRITTEN;
			VOP_BWRITE(ibp);
		}
	} else if (!(bp->b_flags & (B_DONE|B_DELWRI))) {
		/*
		 * Not a brand new block, also not in the cache;
		 * read it in from disk.
		 */
		if (iosize == fs->lfs_bsize)
			/* Optimization: I/O is unnecessary. */
			bp->b_blkno = daddr;
		else {
			/*
			 * We need to read the block to preserve the
			 * existing bytes.
			 */
			bp->b_blkno = daddr;
			bp->b_flags |= B_READ;
			VOP_STRATEGY(bp);
			return(biowait(bp));
		}
	}
	
	return (0);
}

/* VOP_BWRITE 1 time */
int
lfs_fragextend(struct vnode *vp, int osize, int nsize, ufs_daddr_t lbn, struct buf **bpp, struct ucred *cred)
{
	struct inode *ip;
	struct lfs *fs;
	long bb;
	int error;
	extern long locked_queue_bytes;
	struct buf *ibp;
	size_t obufsize;
	SEGUSE *sup;

	ip = VTOI(vp);
	fs = ip->i_lfs;
	bb = (long)fragstofsb(fs, numfrags(fs, nsize - osize));
	error = 0;

	/*
	 * Get the seglock so we don't enlarge blocks or change the segment
	 * accounting information while a segment is being written.
	 */
    top:
	lfs_seglock(fs, SEGM_PROT);

	if (!ISSPACE(fs, bb, cred)) {
		error = ENOSPC;
		goto out;
	}
	if ((error = bread(vp, lbn, osize, NOCRED, bpp))) {
		brelse(*bpp);
		goto out;
	}
#ifdef QUOTA
	if ((error = chkdq(ip, bb, cred, 0))) {
		brelse(*bpp);
		goto out;
	}
#endif
	/*
	 * Adjust accounting for lfs_avail.  If there's not enough room,
	 * we will have to wait for the cleaner, which we can't do while
	 * holding a block busy or while holding the seglock.  In that case,
	 * release both and start over after waiting.
	 */
	if ((*bpp)->b_flags & B_DELWRI) {
		if (!lfs_fits(fs, bb)) {
			brelse(*bpp);
#ifdef QUOTA
			chkdq(ip, -bb, cred, 0);
#endif
			lfs_segunlock(fs);
			lfs_availwait(fs, bb);
			goto top;
		}
		fs->lfs_avail -= bb;
	}

	/*
 	 * Fix the allocation for this fragment so that it looks like the
         * source segment contained a block of the new size.  This overcounts;
	 * but the overcount only lasts until the block in question
	 * is written, so the on-disk live bytes count is always correct.
	 */
	if ((*bpp)->b_blkno > 0) {
		LFS_SEGENTRY(sup, fs, dtosn(fs, dbtofsb(fs, (*bpp)->b_blkno)), ibp);
		sup->su_nbytes += (nsize - osize);
		VOP_BWRITE(ibp);
		ip->i_ffs_blocks += bb;
	}
	fs->lfs_bfree -= bb;
	ip->i_lfs_effnblks += bb;
	ip->i_flag |= IN_CHANGE | IN_UPDATE;

	obufsize = (*bpp)->b_bufsize;
	allocbuf(*bpp, nsize);

	/* Adjust locked-list accounting */
	if (((*bpp)->b_flags & (B_LOCKED | B_CALL)) == B_LOCKED)
		locked_queue_bytes += (*bpp)->b_bufsize - obufsize;

	bzero((char *)((*bpp)->b_data) + osize, (u_int)(nsize - osize));

    out:
	lfs_segunlock(fs);
	return (error);
}
