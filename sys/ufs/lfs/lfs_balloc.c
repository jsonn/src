/*	$NetBSD: lfs_balloc.c,v 1.12.4.3 1999/08/31 21:03:45 perseant Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
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

#if defined(_KERNEL) && !defined(_LKM)
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

#include <vm/vm.h>
#include <uvm/uvm_extern.h>

int lfs_fragextend __P((struct vnode *, int, int, ufs_daddr_t, struct buf **));

/*
 * Balloc defines the structure of file system storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 */

int
lfs_balloc(v)
	void *v;
{
	struct vop_balloc_args /* {
		struct vnode *a_vp;
		off_t a_offset;
		off_t a_length;
		struct ucred *a_cred;
		int a_flags;
	} */ *ap = v;

	off_t off, len;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct lfs *fs = ip->i_lfs;
	int error, delta, bshift, bsize;

	bshift = fs->lfs_bshift;
	bsize = 1 << bshift;

	off = ap->a_offset;
	len = ap->a_length;

	delta = off & (bsize - 1);
	off -= delta;
	len += delta;

	while (len > 0) {
		bsize = min(bsize, len);

		if ((error = lfs_balloc1(vp, blkoff(fs,off), bsize,
					 lblkno(fs, off), NULL))) {
			return error;
		}

		/*
		 * increase file size now, VOP_BALLOC() requires that
		 * EOF be up-to-date before each call.
		 */

		if (ip->i_ffs_size < off + bsize) {
			ip->i_ffs_size = off + bsize;
			if (vp->v_uvm.u_size < ip->i_ffs_size) {
				uvm_vnp_setsize(vp, ip->i_ffs_size);
			}
		}

		off += bsize;
		len -= bsize;
	}
	return 0;
}

int
lfs_balloc1(vp, offset, iosize, lbn, bpp)
	struct vnode *vp;
	int offset;
	u_long iosize;
	ufs_daddr_t lbn;
	struct buf **bpp;
{
	struct buf *ibp, *bp;
	struct inode *ip;
	struct lfs *fs;
	struct indir *ap, indirs[NIADDR+2];
	ufs_daddr_t	daddr, lastblock;
	int bb;		/* number of disk blocks in a block disk blocks */
	int error, frags, i, nsize, osize, num;
	
	ip = VTOI(vp);
	fs = ip->i_lfs;

	/* XXX printf("lfs_balloc1(%p,%d,%ld,%x,%p)\n",
			 vp, offset, iosize, lbn, bpp); */
	
#ifdef DEBUG
	if(!VOP_ISLOCKED(vp)) {
		printf("lfs_balloc: warning: ino %d not locked\n",ip->i_number);
	}
#endif
	
	/* 
	 * Three cases: it's a block beyond the end of file, it's a block in
	 * the file that may or may not have been assigned a disk address or
	 * we're writing an entire block.  Note, if the daddr is unassigned,
	 * the block might still have existed in the cache (if it was read
	 * or written earlier).	 If it did, make sure we don't count it as a
	 * new block or zero out its contents.	If it did not, make sure
	 * we allocate any necessary indirect blocks.
	 * If we are writing a block beyond the end of the file, we need to
	 * check if the old last block was a fragment.	If it was, we need
	 * to rewrite it.
	 */
	
	if (bpp != NULL) {
		*bpp = NULL;
	}
	error = ufs_bmaparray(vp, lbn, &daddr, &indirs[0], &num, NULL );
	if (error)
		return (error);
	
	/* Check for block beyond end of file and fragment extension needed. */
	lastblock = lblkno(fs, ip->i_ffs_size);
	if (lastblock < NDADDR && lastblock < lbn) {
		osize = blksize(fs, ip, lastblock);
		if (osize < fs->lfs_bsize && osize > 0) {
			if ((error = lfs_fragextend(vp, osize, fs->lfs_bsize,
						    lastblock, (bpp ? &bp : NULL))))
				return(error);
			ip->i_ffs_size = (lastblock + 1) * fs->lfs_bsize;
			uvm_vnp_setsize(vp, ip->i_ffs_size);
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			VOP_BWRITE(bp);
		}
	}
	
	bb = VFSTOUFS(vp->v_mount)->um_seqinc;
	if (daddr == UNASSIGNED) {
		/* May need to allocate indirect blocks */
		for (i = 1; i < num; ++i) {
			if (!indirs[i].in_exists) {
				ibp = getblk(vp, indirs[i].in_lbn, fs->lfs_bsize,
					     0, 0);
				if ((ibp->b_flags & (B_DONE | B_DELWRI))) 
					panic ("Indirect block should not exist");

				if (!ISSPACE(fs, bb, curproc->p_ucred)){
					ibp->b_flags |= B_INVAL;
					brelse(ibp);
					return(ENOSPC);
				} else {
					ip->i_ffs_blocks += bb;
					ip->i_lfs->lfs_bfree -= bb;
					clrbuf(ibp);
					if ((error = VOP_BWRITE(ibp)))
						return(error);
				}
			}
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
		frags = numfrags(fs, nsize);
		bb = fragstodb(fs, frags);
		if (lblktosize(fs, lbn) >= ip->i_ffs_size) {
			/* Brand new block or fragment */
			if (bpp != NULL) {
				*bpp = bp = getblk(vp, lbn, nsize, 0, 0);
			}
		} else {
			if (nsize <= osize) {
				/* No need to extend */
				/* XXX KS - Are we wasting space? */
				if (bpp != NULL && (error = bread(vp, lbn, osize, NOCRED, &bp)))
					return error;
			} else {
				/* Extend existing block */
				if (bpp != NULL && (error =
				     lfs_fragextend(vp, osize, nsize, lbn, &bp)))
					return(error);
			}
			if (bpp != NULL) {
				*bpp = bp;
			}
		}
	} else {
		/*
		 * Get the existing block from the cache either because the
		 * block 1) is not a direct block or 2) is not the last
		 * block in the file.
		 */
		frags = dbtofrags(fs, bb);
		if (bpp != NULL) {
			*bpp = bp = getblk(vp, lbn, blksize(fs, ip, lbn), 0, 0);
		}
	}
	
	/* 
	 * The block we are writing may be a brand new block
	 * in which case we need to do accounting (i.e. check
	 * for free space and update the inode number of blocks.
	 */
	if (bpp == NULL && daddr == UNASSIGNED) {
		/*
		 * If bpp is NULL we are being called from UBC.  In this
		 * case, all we have to do is block accounting for the
		 * inode and the filesystem.
		 */
		if (!ISSPACE(fs, bb, curproc->p_ucred))
			return(ENOSPC);

		ip->i_ffs_blocks += bb;
		ip->i_lfs->lfs_bfree -= bb;

		/* Assign a daddr != UNASSIGNED, so we don't overcount */
                switch (num) {
                case 0:
                        ip->i_ffs_db[lbn] = UNWRITTEN;
                        break;
                case 1:
                        ip->i_ffs_ib[indirs[0].in_off] = UNWRITTEN;
                        break;
                default:
                        ap = &indirs[num - 1];
                        if (bread(vp, ap->in_lbn, fs->lfs_bsize, NOCRED, &bp))
                                panic("lfs_balloc: bread bno %d", ap->in_lbn);
			/* The indirect block exists, no need to account it */
                        ((ufs_daddr_t *)bp->b_data)[ap->in_off] = UNWRITTEN;
                        VOP_BWRITE(bp);
                }
	} else if (bpp && !(bp->b_flags & (B_DONE | B_DELWRI))) {
		if (daddr == UNASSIGNED) {
			if (!ISSPACE(fs, bb, curproc->p_ucred)) {
				bp->b_flags |= B_INVAL;
				brelse(bp);
				return(ENOSPC);
			} else {
				ip->i_ffs_blocks += bb;
				ip->i_lfs->lfs_bfree -= bb;
				if (iosize != fs->lfs_bsize)
					clrbuf(bp);
			}
		} else if (iosize == fs->lfs_bsize) {
			/* Optimization: I/O is unnecessary. */
			bp->b_blkno = daddr;
		} else  {
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

int
lfs_fragextend(vp, osize, nsize, lbn, bpp)
	struct vnode *vp;
	int osize;
	int nsize;
	ufs_daddr_t lbn;
	struct buf **bpp;
{
	struct inode *ip;
	struct lfs *fs;
	long bb;
	int error;
	extern long locked_queue_bytes;
	struct buf *ibp;
	SEGUSE *sup;
	daddr_t daddr;

	ip = VTOI(vp);
	fs = ip->i_lfs;
	
	bb = (long)fragstodb(fs, numfrags(fs, nsize - osize));
 top:
	if (!ISSPACE(fs, bb, curproc->p_ucred)) {
		return(ENOSPC);
	}
	if (bpp) {
		if ((error = bread(vp, lbn, osize, NOCRED, bpp))) {
			brelse(*bpp);
			return(error);
		}
		daddr = (*bpp)->b_blkno;
	} else {
		/* We still need the daddr, just not contents. */
		if((error = VOP_BMAP(vp, lbn, NULL, &daddr, NULL)))
			return (error);
	}

	/*
 	 * Fix the allocation for this fragment so that it looks like the
         * source segment contained a block of the new size.  This overcounts;
	 * but the overcount only lasts until the block in question
	 * is written, so the on-disk live bytes count is always correct.
	 */
	LFS_SEGENTRY(sup, fs, datosn(fs,daddr), ibp);
	sup->su_nbytes += (nsize-osize);
	VOP_BWRITE(ibp);

#ifdef QUOTA
	if ((error = chkdq(ip, bb, curproc->p_ucred, 0))) {
		if (bpp)
			brelse(*bpp);
		return (error);
	}
#endif
	/*
	 * XXX - KS - Don't change size while we're gathered, as we could
	 * then overlap another buffer in lfs_writeseg.
	 */
	if(bpp && (*bpp)->b_flags & B_GATHERED) {
		(*bpp)->b_flags |= B_NEEDCOMMIT; /* XXX KS - what flag to use? */
		brelse(*bpp);
		tsleep(*bpp, (PRIBIO+1), "lfs_fragextend", 0);
		goto top;
	}
	ip->i_ffs_blocks += bb;
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	fs->lfs_bfree -= fragstodb(fs, numfrags(fs, (nsize - osize)));
	if (bpp) {
		if((*bpp)->b_flags & B_LOCKED)
			locked_queue_bytes += (nsize - osize);
		allocbuf(*bpp, nsize);
		bzero((char *)((*bpp)->b_data) + osize, (u_int)(nsize - osize));
	}
	return(0);
}
