/*	$NetBSD: lfs_subr.c,v 1.23.2.2 2002/06/02 15:31:24 tv Exp $	*/

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
 * Copyright (c) 1991, 1993
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
 *	@(#)lfs_subr.c	8.4 (Berkeley) 5/8/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_subr.c,v 1.23.2.2 2002/06/02 15:31:24 tv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <ufs/ufs/inode.h>
#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

/*
 * Return buffer with the contents of block "offset" from the beginning of
 * directory "ip".  If "res" is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 */
int
lfs_blkatoff(void *v)
{
	struct vop_blkatoff_args /* {
		struct vnode *a_vp;
		off_t a_offset;
		char **a_res;
		struct buf **a_bpp;
		} */ *ap = v;
	struct lfs *fs;
	struct inode *ip;
	struct buf *bp;
	ufs_daddr_t lbn;
	int bsize, error;
	
	ip = VTOI(ap->a_vp);
	fs = ip->i_lfs;
	lbn = lblkno(fs, ap->a_offset);
	bsize = blksize(fs, ip, lbn);
	
	*ap->a_bpp = NULL;
	if ((error = bread(ap->a_vp, lbn, bsize, NOCRED, &bp)) != 0) {
		brelse(bp);
		return (error);
	}
	if (ap->a_res)
		*ap->a_res = (char *)bp->b_data + blkoff(fs, ap->a_offset);
	*ap->a_bpp = bp;
	return (0);
}


/*
 * lfs_seglock --
 *	Single thread the segment writer.
 */
void
lfs_seglock(struct lfs *fs, unsigned long flags)
{
	struct segment *sp;
	int s;
	
	if (fs->lfs_seglock) {
		if (fs->lfs_lockpid == curproc->p_pid) {
			++fs->lfs_seglock;
			fs->lfs_sp->seg_flags |= flags;
			return;			
		} else while (fs->lfs_seglock)
			(void)tsleep(&fs->lfs_seglock, PRIBIO + 1,
				     "lfs seglock", 0);
	}
	
	fs->lfs_seglock = 1;
	fs->lfs_lockpid = curproc->p_pid;
	
	sp = fs->lfs_sp = malloc(sizeof(struct segment), M_SEGMENT, M_WAITOK);
	sp->bpp = malloc(((fs->lfs_sumsize - SEGSUM_SIZE(fs)) /
			  sizeof(ufs_daddr_t) + 1) * sizeof(struct buf *),
			 M_SEGMENT, M_WAITOK);
	sp->seg_flags = flags;
	sp->vp = NULL;
	(void) lfs_initseg(fs);
	
	/*
	 * Keep a cumulative count of the outstanding I/O operations.  If the
	 * disk drive catches up with us it could go to zero before we finish,
	 * so we artificially increment it by one until we've scheduled all of
	 * the writes we intend to do.
	 */
	s = splbio();
	++fs->lfs_iocount;
	splx(s);
}

/*
 * lfs_segunlock --
 *	Single thread the segment writer.
 */
void
lfs_segunlock(struct lfs *fs)
{
	struct segment *sp;
	unsigned long sync, ckp;
	int s;
	struct buf *bp;
	struct vnode *vp, *nvp;
	struct mount *mp;
	extern int lfs_dirvcount;
#ifdef LFS_MALLOC_SUMMARY
	extern int locked_queue_count;
	extern long locked_queue_bytes;
#endif
	
	sp = fs->lfs_sp;

	if (fs->lfs_seglock == 1 && !(sp->seg_flags & SEGM_PROT)) {

		mp = fs->lfs_ivnode->v_mount;
		/*
		 * Go through and unmark all DIROP vnodes, possibly
		 * calling VOP_INACTIVE (through vrele).  This is
		 * delayed until now in order not to accidentally
		 * write a DIROP node through lfs_flush.
		 */
#ifndef LFS_NO_BACKVP_HACK
	/* BEGIN HACK */
#define	VN_OFFSET	(((caddr_t)&LIST_NEXT(vp, v_mntvnodes)) - (caddr_t)vp)
#define	BACK_VP(VP)	((struct vnode *)(((caddr_t)(VP)->v_mntvnodes.le_prev) - VN_OFFSET))
#define	BEG_OF_VLIST	((struct vnode *)(((caddr_t)&LIST_FIRST(&mp->mnt_vnodelist)) - VN_OFFSET))
	
		/* Find last vnode. */
	loop:	for (vp = LIST_FIRST(&mp->mnt_vnodelist);
		     vp && LIST_NEXT(vp, v_mntvnodes) != NULL;
		     vp = LIST_NEXT(vp, v_mntvnodes));
		for (; vp && vp != BEG_OF_VLIST; vp = nvp) {
			nvp = BACK_VP(vp);
#else
	loop:
		 for (vp = LIST_FIRST(&mp->mnt_vnodelist);
		     vp != NULL;
		     vp = nvp) {
			nvp = LIST_NEXT(vp, v_mntvnodes);
#endif
			if (vp->v_mount != mp) {
				printf("lfs_segunlock: starting over\n");
				goto loop;
			}
			if (vp->v_type == VNON)
				continue;
			if (lfs_vref(vp))
				continue;
			if (VOP_ISLOCKED(vp) &&
                            vp->v_lock.lk_lockholder != curproc->p_pid) {
				lfs_vunref(vp);
				continue;
			}
			if ((vp->v_flag & VDIROP) &&
			    !(VTOI(vp)->i_flag & IN_ADIROP)) {
				--lfs_dirvcount;
				vp->v_flag &= ~VDIROP;
				wakeup(&lfs_dirvcount);
				fs->lfs_unlockvp = vp;
				lfs_vunref(vp);
				vrele(vp);
				fs->lfs_unlockvp = NULL;
			} else {
				lfs_vunref(vp);
			}
		}
	}

	if (fs->lfs_seglock == 1) {
		sync = sp->seg_flags & SEGM_SYNC;
		ckp = sp->seg_flags & SEGM_CKP;
		if (sp->bpp != sp->cbpp) {
			/* Free allocated segment summary */
			fs->lfs_offset -= btofsb(fs, fs->lfs_sumsize);
			bp = *sp->bpp;
#ifdef LFS_MALLOC_SUMMARY
			lfs_freebuf(bp);
#else
			s = splbio();
			bremfree(bp);
			bp->b_flags |= B_DONE|B_INVAL;
			bp->b_flags &= ~B_DELWRI;
			reassignbuf(bp,bp->b_vp);
			splx(s);
			brelse(bp);
#endif
		} else
			printf ("unlock to 0 with no summary");

		free(sp->bpp, M_SEGMENT);
		sp->bpp = NULL;
		free(sp, M_SEGMENT);
		fs->lfs_sp = NULL;

		/*
		 * If the I/O count is non-zero, sleep until it reaches zero.
		 * At the moment, the user's process hangs around so we can
		 * sleep.
		 */
		s = splbio();
		if (--fs->lfs_iocount < LFS_THROTTLE)
			wakeup(&fs->lfs_iocount);
		if(fs->lfs_iocount == 0) {
			lfs_countlocked(&locked_queue_count,
					&locked_queue_bytes, "lfs_segunlock");
			wakeup(&locked_queue_count);
			wakeup(&fs->lfs_iocount);
		}
		/*
		 * We let checkpoints happen asynchronously.  That means
		 * that during recovery, we have to roll forward between
		 * the two segments described by the first and second
		 * superblocks to make sure that the checkpoint described
		 * by a superblock completed.
		 */
		while (sync && fs->lfs_iocount)
			(void)tsleep(&fs->lfs_iocount, PRIBIO + 1,
				     "lfs vflush", 0);
		splx(s);
		if (ckp) {
			fs->lfs_nactive = 0;
			/* If we *know* everything's on disk, write both sbs */
			if (sync)
				lfs_writesuper(fs,fs->lfs_sboffs[fs->lfs_activesb]);
			fs->lfs_activesb = 1 - fs->lfs_activesb;
			lfs_writesuper(fs,fs->lfs_sboffs[fs->lfs_activesb]);
		}
		--fs->lfs_seglock;
		fs->lfs_lockpid = 0;
		wakeup(&fs->lfs_seglock);
	} else if (fs->lfs_seglock == 0) {
		panic ("Seglock not held");
	} else {
		--fs->lfs_seglock;
	}
}
