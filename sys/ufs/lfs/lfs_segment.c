/*	$NetBSD: lfs_segment.c,v 1.31.4.1 1999/10/19 12:50:43 fvdl Exp $	*/

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
 *	@(#)lfs_segment.c	8.10 (Berkeley) 6/10/95
 */

#define ivndebug(vp,str) printf("ino %d: %s\n",VTOI(vp)->i_number,(str))

#include "opt_ddb.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/resourcevar.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

extern int count_lock_queue __P((void));
extern struct simplelock vnode_free_list_slock;		/* XXX */

/*
 * Determine if it's OK to start a partial in this segment, or if we need
 * to go on to a new segment.
 */
#define	LFS_PARTIAL_FITS(fs) \
	((fs)->lfs_dbpseg - ((fs)->lfs_offset - (fs)->lfs_curseg) > \
	1 << (fs)->lfs_fsbtodb)

void	 lfs_callback __P((struct buf *));
int	 lfs_gather __P((struct lfs *, struct segment *,
	     struct vnode *, int (*) __P((struct lfs *, struct buf *))));
int	 lfs_gatherblock __P((struct segment *, struct buf *, int *));
void	 lfs_iset __P((struct inode *, ufs_daddr_t, time_t));
int	 lfs_match_fake __P((struct lfs *, struct buf *));
int	 lfs_match_data __P((struct lfs *, struct buf *));
int	 lfs_match_dindir __P((struct lfs *, struct buf *));
int	 lfs_match_indir __P((struct lfs *, struct buf *));
int	 lfs_match_tindir __P((struct lfs *, struct buf *));
void	 lfs_newseg __P((struct lfs *));
void	 lfs_shellsort __P((struct buf **, ufs_daddr_t *, register int));
void	 lfs_supercallback __P((struct buf *));
void	 lfs_updatemeta __P((struct segment *));
int	 lfs_vref __P((struct vnode *));
void	 lfs_vunref __P((struct vnode *));
void	 lfs_writefile __P((struct lfs *, struct segment *, struct vnode *));
int	 lfs_writeinode __P((struct lfs *, struct segment *, struct inode *));
int	 lfs_writeseg __P((struct lfs *, struct segment *));
void	 lfs_writesuper __P((struct lfs *, daddr_t));
int	 lfs_writevnodes __P((struct lfs *fs, struct mount *mp,
	    struct segment *sp, int dirops));

int	lfs_allclean_wakeup;		/* Cleaner wakeup address. */
int	lfs_writeindir = 1;             /* whether to flush indir on non-ckp */
int	lfs_clean_vnhead = 0;		/* Allow freeing to head of vn list */

/* Statistics Counters */
int lfs_dostats = 1;
struct lfs_stats lfs_stats;

/* op values to lfs_writevnodes */
#define	VN_REG	        0
#define	VN_DIROP	1
#define	VN_EMPTY	2
#define VN_CLEAN        3

#define LFS_MAX_ACTIVE          10

/*
 * XXX KS - Set modification time on the Ifile, so the cleaner can
 * read the fs mod time off of it.  We don't set IN_UPDATE here,
 * since we don't really need this to be flushed to disk (and in any
 * case that wouldn't happen to the Ifile until we checkpoint).
 */
void
lfs_imtime(fs)
	struct lfs *fs;
{
	struct timespec ts;
	struct inode *ip;
	
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	ip = VTOI(fs->lfs_ivnode);
	ip->i_ffs_mtime = ts.tv_sec;
	ip->i_ffs_mtimensec = ts.tv_nsec;
}

/*
 * Ifile and meta data blocks are not marked busy, so segment writes MUST be
 * single threaded.  Currently, there are two paths into lfs_segwrite, sync()
 * and getnewbuf().  They both mark the file system busy.  Lfs_vflush()
 * explicitly marks the file system busy.  So lfs_segwrite is safe.  I think.
 */

#define SET_FLUSHING(fs,vp) (fs)->lfs_flushvp = (vp)
#define IS_FLUSHING(fs,vp)  ((fs)->lfs_flushvp == (vp))
#define CLR_FLUSHING(fs,vp) (fs)->lfs_flushvp = NULL

int
lfs_vflush(vp)
	struct vnode *vp;
{
	struct inode *ip;
	struct lfs *fs;
	struct segment *sp;
	struct buf *bp, *nbp;
	int error, s;

	ip = VTOI(vp);
	fs = VFSTOUFS(vp->v_mount)->um_lfs;

	if(ip->i_flag & IN_CLEANING) {
#ifdef DEBUG_LFS
		ivndebug(vp,"vflush/in_cleaning");
#endif
		ip->i_flag &= ~IN_CLEANING;
		if(ip->i_flag & IN_MODIFIED) {
			fs->lfs_uinodes--;
		} else
			ip->i_flag |= IN_MODIFIED;
	}

	/* If the node is being written, wait until that is done */
	if(WRITEINPROG(vp)) {
#ifdef DEBUG_LFS
		ivndebug(vp,"vflush/writeinprog");
#endif
		tsleep(vp, PRIBIO+1, "lfs_vw", 0);
	}

	/* Protect against VXLOCK deadlock in vinvalbuf() */
	lfs_seglock(fs, SEGM_SYNC);

	/* If we're supposed to flush a freed inode, just toss it */
	/* XXX - seglock, so these buffers can't be gathered, right? */
	if(ip->i_ffs_mode == 0) {
		printf("lfs_vflush: ino %d is freed, not flushing\n",
			ip->i_number);
		s = splbio();
		for(bp=vp->v_dirtyblkhd.lh_first; bp; bp=nbp) {
			nbp = bp->b_vnbufs.le_next;
			/* Copied from lfs_writeseg */
			if (bp->b_flags & B_CALL) {
				/* if B_CALL, it was created with newbuf */
				lfs_freebuf(bp);
			} else {
				bremfree(bp);
				bp->b_flags &= ~(B_ERROR | B_READ | B_DELWRI |
                                         B_LOCKED | B_GATHERED);
				bp->b_flags |= B_DONE;
				reassignbuf(bp, vp);
				brelse(bp);  
			}
		}
		splx(s);
		if(ip->i_flag & IN_CLEANING)
			fs->lfs_uinodes--;
		if(ip->i_flag & IN_MODIFIED)
			fs->lfs_uinodes--;
		ip->i_flag &= ~(IN_MODIFIED|IN_UPDATE|IN_ACCESS|IN_CHANGE|IN_CLEANING);
		printf("lfs_vflush: done not flushing ino %d\n",
			ip->i_number);
		lfs_segunlock(fs);
		return 0;
	}

	SET_FLUSHING(fs,vp);
	if (fs->lfs_nactive > LFS_MAX_ACTIVE) {
		error = lfs_segwrite(vp->v_mount, SEGM_SYNC|SEGM_CKP);
		CLR_FLUSHING(fs,vp);
		lfs_segunlock(fs);
		return error;
	}
	sp = fs->lfs_sp;

	if (vp->v_dirtyblkhd.lh_first == NULL) {
		lfs_writevnodes(fs, vp->v_mount, sp, VN_EMPTY);
	} else if((ip->i_flag & IN_CLEANING) && (fs->lfs_sp->seg_flags & SEGM_CLEAN)) {
#ifdef DEBUG_LFS
		ivndebug(vp,"vflush/clean");
#endif
		lfs_writevnodes(fs, vp->v_mount, sp, VN_CLEAN);
	}
	else if(lfs_dostats) {
		if(vp->v_dirtyblkhd.lh_first || (VTOI(vp)->i_flag & (IN_MODIFIED|IN_UPDATE|IN_ACCESS|IN_CHANGE|IN_CLEANING)))
			++lfs_stats.vflush_invoked;
#ifdef DEBUG_LFS
		ivndebug(vp,"vflush");
#endif
	}

#ifdef DIAGNOSTIC
	/* XXX KS This actually can happen right now, though it shouldn't(?) */
	if(vp->v_flag & VDIROP) {
		printf("lfs_vflush: flushing VDIROP, this shouldn\'t be\n");
		/* panic("VDIROP being flushed...this can\'t happen"); */
	}
	if(vp->v_usecount<0) {
		printf("usecount=%ld\n",vp->v_usecount);
		panic("lfs_vflush: usecount<0");
	}
#endif

	do {
		do {
			if (vp->v_dirtyblkhd.lh_first != NULL)
				lfs_writefile(fs, sp, vp);
		} while (lfs_writeinode(fs, sp, ip));
	} while (lfs_writeseg(fs, sp) && ip->i_number == LFS_IFILE_INUM);
	
	if(lfs_dostats) {
		++lfs_stats.nwrites;
		if (sp->seg_flags & SEGM_SYNC)
			++lfs_stats.nsync_writes;
		if (sp->seg_flags & SEGM_CKP)
			++lfs_stats.ncheckpoints;
	}
	lfs_segunlock(fs);

	CLR_FLUSHING(fs,vp);
	return (0);
}

#ifdef DEBUG_LFS_VERBOSE
# define vndebug(vp,str) if(VTOI(vp)->i_flag & IN_CLEANING) printf("not writing ino %d because %s (op %d)\n",VTOI(vp)->i_number,(str),op)
#else
# define vndebug(vp,str)
#endif

int
lfs_writevnodes(fs, mp, sp, op)
	struct lfs *fs;
	struct mount *mp;
	struct segment *sp;
	int op;
{
	struct inode *ip;
	struct vnode *vp;
	int inodes_written=0, only_cleaning;

#ifndef LFS_NO_BACKVP_HACK
	/* BEGIN HACK */
#define	VN_OFFSET	(((caddr_t)&vp->v_mntvnodes.le_next) - (caddr_t)vp)
#define	BACK_VP(VP)	((struct vnode *)(((caddr_t)VP->v_mntvnodes.le_prev) - VN_OFFSET))
#define	BEG_OF_VLIST	((struct vnode *)(((caddr_t)&mp->mnt_vnodelist.lh_first) - VN_OFFSET))
	
	/* Find last vnode. */
 loop:	for (vp = mp->mnt_vnodelist.lh_first;
	     vp && vp->v_mntvnodes.le_next != NULL;
	     vp = vp->v_mntvnodes.le_next);
	for (; vp && vp != BEG_OF_VLIST; vp = BACK_VP(vp)) {
#else
	loop:
	for (vp = mp->mnt_vnodelist.lh_first;
	     vp != NULL;
	     vp = vp->v_mntvnodes.le_next) {
#endif
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;

		ip = VTOI(vp);
		if ((op == VN_DIROP && !(vp->v_flag & VDIROP)) ||
		    (op != VN_DIROP && op != VN_CLEAN && (vp->v_flag & VDIROP))) {
			vndebug(vp,"dirop");
			continue;
		}
		
		if (op == VN_EMPTY && vp->v_dirtyblkhd.lh_first) {
			vndebug(vp,"empty");
			continue;
		}
		
		if (vp->v_type == VNON) {
			continue;
		}

		if(op == VN_CLEAN && ip->i_number != LFS_IFILE_INUM
		   && !(ip->i_flag & IN_CLEANING)) {
			vndebug(vp,"cleaning");
			continue;
		}

		if (lfs_vref(vp)) {
			vndebug(vp,"vref");
			continue;
		}

#if 0 /* XXX KS - if we skip the ifile, things could go badly for us. */
		if(WRITEINPROG(vp)) {
			lfs_vunref(vp);
#ifdef DEBUG_LFS
			ivndebug(vp,"writevnodes/writeinprog");
#endif
			continue;
		}
#endif
		only_cleaning = 0;
		/*
		 * Write the inode/file if dirty and it's not the
		 * the IFILE.
		 */
		if ((ip->i_flag &
		     (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE | IN_CLEANING) ||
		     vp->v_dirtyblkhd.lh_first != NULL))
		{
			only_cleaning = ((ip->i_flag & (IN_ACCESS|IN_CHANGE|IN_MODIFIED|IN_UPDATE|IN_CLEANING))==IN_CLEANING);

			if(ip->i_number != LFS_IFILE_INUM
			   && vp->v_dirtyblkhd.lh_first != NULL)
			{
				lfs_writefile(fs, sp, vp);
			}
			if(vp->v_dirtyblkhd.lh_first != NULL) {
				if(WRITEINPROG(vp)) {
#ifdef DEBUG_LFS
					ivndebug(vp,"writevnodes/write2");
#endif
				} else if(!(ip->i_flag & (IN_ACCESS|IN_CHANGE|IN_MODIFIED|IN_UPDATE|IN_CLEANING))) {
#ifdef DEBUG_LFS
					printf("<%d>",ip->i_number);
#endif
					ip->i_flag |= IN_MODIFIED;
					++fs->lfs_uinodes;
				}
			}
			(void) lfs_writeinode(fs, sp, ip);
			inodes_written++;
		}

		if(vp->v_flag & VDIROP) {
			--fs->lfs_dirvcount;
			vp->v_flag &= ~VDIROP;
			wakeup(&fs->lfs_dirvcount);
			lfs_vunref(vp);
		}

		if(lfs_clean_vnhead && only_cleaning)
			lfs_vunref_head(vp);
		else
			lfs_vunref(vp);
	}
	return inodes_written;
}

int
lfs_segwrite(mp, flags)
	struct mount *mp;
	int flags;			/* Do a checkpoint. */
{
	struct buf *bp;
	struct inode *ip;
	struct lfs *fs;
	struct segment *sp;
	struct vnode *vp;
	SEGUSE *segusep;
	ufs_daddr_t ibno;
	int do_ckp, error, i;
	int writer_set = 0;
	int need_unlock = 0;
	
	fs = VFSTOUFS(mp)->um_lfs;

	lfs_imtime(fs);
	
	/*
	 * If we are not the cleaner, and we have fewer than MIN_FREE_SEGS
	 * clean segments, wait until cleaner writes.
	 */
	if(!(flags & SEGM_CLEAN)
	   && (!fs->lfs_seglock || !(fs->lfs_sp->seg_flags & SEGM_CLEAN)))
	{
		do {
			if (fs->lfs_nclean <= MIN_FREE_SEGS
			    || fs->lfs_avail <= 0)
			{
				wakeup(&lfs_allclean_wakeup);
				wakeup(&fs->lfs_nextseg);
				error = tsleep(&fs->lfs_avail, PRIBIO + 1,
					       "lfs_avail", 0);
				if (error) {
					return (error);
				}
			}
		} while (fs->lfs_nclean <= MIN_FREE_SEGS || fs->lfs_avail <= 0);
	}

	/*
	 * Allocate a segment structure and enough space to hold pointers to
	 * the maximum possible number of buffers which can be described in a
	 * single summary block.
	 */
	do_ckp = (flags & SEGM_CKP) || fs->lfs_nactive > LFS_MAX_ACTIVE;
	lfs_seglock(fs, flags | (do_ckp ? SEGM_CKP : 0));
	sp = fs->lfs_sp;

	/*
	 * If lfs_flushvp is non-NULL, we are called from lfs_vflush,
	 * in which case we have to flush *all* buffers off of this vnode.
	 */
	if((sp->seg_flags & SEGM_CLEAN) && !(fs->lfs_flushvp))
		lfs_writevnodes(fs, mp, sp, VN_CLEAN);
	else {
		lfs_writevnodes(fs, mp, sp, VN_REG);
		/*
		 * XXX KS - If we're cleaning, we can't wait for dirops,
		 * because they might be waiting on us.  The downside of this
		 * is that, if we write anything besides cleaning blocks
		 * while cleaning, the checkpoint is not completely
		 * consistent.
		 */
		if(!(sp->seg_flags & SEGM_CLEAN)) {
			while(fs->lfs_dirops)
				if((error = tsleep(&fs->lfs_writer, PRIBIO + 1,
						"lfs writer", 0)))
				{
					free(sp->bpp, M_SEGMENT);
					free(sp, M_SEGMENT); 
					return (error);
				}
			fs->lfs_writer++;
			writer_set=1;
			lfs_writevnodes(fs, mp, sp, VN_DIROP);
			((SEGSUM *)(sp->segsum))->ss_flags &= ~(SS_CONT);
		}
	}	

	/*
	 * If we are doing a checkpoint, mark everything since the
	 * last checkpoint as no longer ACTIVE.
	 */
	if (do_ckp) {
		for (ibno = fs->lfs_cleansz + fs->lfs_segtabsz;
		     --ibno >= fs->lfs_cleansz; ) {
			if (bread(fs->lfs_ivnode, ibno, fs->lfs_bsize, NOCRED, &bp))

				panic("lfs_segwrite: ifile read");
			segusep = (SEGUSE *)bp->b_data;
			for (i = fs->lfs_sepb; i--; segusep++)
				segusep->su_flags &= ~SEGUSE_ACTIVE;
				
			/* But the current segment is still ACTIVE */
			if (fs->lfs_curseg/fs->lfs_sepb==(ibno-fs->lfs_cleansz))
				((SEGUSE *)(bp->b_data))[fs->lfs_curseg%fs->lfs_sepb].su_flags |= SEGUSE_ACTIVE;
			error = VOP_BWRITE(bp);
		}
	}
	
	if (do_ckp || fs->lfs_doifile) {
	redo:
		vp = fs->lfs_ivnode;
		/*
		 * Depending on the circumstances of our calling, the ifile
		 * inode might be locked.  If it is, and if it is locked by
		 * us, we should VREF instead of vget here.
		 */
		need_unlock = 0;
		if(VOP_ISLOCKED(vp)
		   && vp->v_lock.lk_lockholder == curproc->p_pid) {
			VREF(vp);
		} else {
			while (vget(vp, LK_EXCLUSIVE))
				continue;
			need_unlock = 1;
		}
		ip = VTOI(vp);
		if (vp->v_dirtyblkhd.lh_first != NULL)
			lfs_writefile(fs, sp, vp);
		(void)lfs_writeinode(fs, sp, ip);

		/* Only vput if we used vget() above. */
		if(need_unlock)
			vput(vp);
		else
			vrele(vp);

		if (lfs_writeseg(fs, sp) && do_ckp)
			goto redo;
	} else {
		(void) lfs_writeseg(fs, sp);
	}
	
	/*
	 * If the I/O count is non-zero, sleep until it reaches zero.
	 * At the moment, the user's process hangs around so we can
	 * sleep. 
	 */
	fs->lfs_doifile = 0;
	if(writer_set && --fs->lfs_writer==0)
		wakeup(&fs->lfs_dirops);
	
	if(lfs_dostats) {
		++lfs_stats.nwrites;
		if (sp->seg_flags & SEGM_SYNC)
			++lfs_stats.nsync_writes;
		if (sp->seg_flags & SEGM_CKP)
			++lfs_stats.ncheckpoints;
	}
	lfs_segunlock(fs);
	return (0);
}

/*
 * Write the dirty blocks associated with a vnode.
 */
void
lfs_writefile(fs, sp, vp)
	struct lfs *fs;
	struct segment *sp;
	struct vnode *vp;
{
	struct buf *bp;
	struct finfo *fip;
	IFILE *ifp;
	
	
	if (sp->seg_bytes_left < fs->lfs_bsize ||
	    sp->sum_bytes_left < sizeof(struct finfo))
		(void) lfs_writeseg(fs, sp);
	
	sp->sum_bytes_left -= sizeof(struct finfo) - sizeof(ufs_daddr_t);
	++((SEGSUM *)(sp->segsum))->ss_nfinfo;

	if(vp->v_flag & VDIROP)
		((SEGSUM *)(sp->segsum))->ss_flags |= (SS_DIROP|SS_CONT);
	
	fip = sp->fip;
	fip->fi_nblocks = 0;
	fip->fi_ino = VTOI(vp)->i_number;
	LFS_IENTRY(ifp, fs, fip->fi_ino, bp);
	fip->fi_version = ifp->if_version;
	brelse(bp);
	
	/*
	 * It may not be necessary to write the meta-data blocks at this point,
	 * as the roll-forward recovery code should be able to reconstruct the
	 * list.
	 *
	 * We have to write them anyway, though, under two conditions: (1) the
	 * vnode is being flushed (for reuse by vinvalbuf); or (2) we are
	 * checkpointing.
	 */
	if((sp->seg_flags & SEGM_CLEAN)
	   && VTOI(vp)->i_number != LFS_IFILE_INUM
	   && !IS_FLUSHING(fs,vp))
	{
		lfs_gather(fs, sp, vp, lfs_match_fake);
	} else
		lfs_gather(fs, sp, vp, lfs_match_data);

	if(lfs_writeindir
	   || IS_FLUSHING(fs,vp)
	   || (sp->seg_flags & SEGM_CKP))
	{
		lfs_gather(fs, sp, vp, lfs_match_indir);
		lfs_gather(fs, sp, vp, lfs_match_dindir);
/* XXX KS - when is TRIPLE not true? */ /* #ifdef TRIPLE */
		lfs_gather(fs, sp, vp, lfs_match_tindir);
/* #endif */
	}
	fip = sp->fip;
	if (fip->fi_nblocks != 0) {
		sp->fip = (FINFO*)((caddr_t)fip + sizeof(struct finfo) +
				   sizeof(ufs_daddr_t) * (fip->fi_nblocks-1));
		sp->start_lbp = &sp->fip->fi_blocks[0];
	} else {
		sp->sum_bytes_left += sizeof(FINFO) - sizeof(ufs_daddr_t);
		--((SEGSUM *)(sp->segsum))->ss_nfinfo;
	}
}

int
lfs_writeinode(fs, sp, ip)
	struct lfs *fs;
	struct segment *sp;
	struct inode *ip;
{
	struct buf *bp, *ibp;
	IFILE *ifp;
	SEGUSE *sup;
	ufs_daddr_t daddr;
	ino_t ino;
	int error, i, ndx;
	int redo_ifile = 0;
	struct timespec ts;
	int gotblk=0;
	
	if (!(ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE | IN_CLEANING)))
		return(0);
	
	/* Allocate a new inode block if necessary. */
	if ((ip->i_number != LFS_IFILE_INUM || sp->idp==NULL) && sp->ibp == NULL) {
		/* Allocate a new segment if necessary. */
		if (sp->seg_bytes_left < fs->lfs_bsize ||
		    sp->sum_bytes_left < sizeof(ufs_daddr_t))
			(void) lfs_writeseg(fs, sp);

		/* Get next inode block. */
		daddr = fs->lfs_offset;
		fs->lfs_offset += fsbtodb(fs, 1);
		sp->ibp = *sp->cbpp++ =
			getblk(VTOI(fs->lfs_ivnode)->i_devvp, daddr, fs->lfs_bsize, 0, 0);
		gotblk++;

		/* Zero out inode numbers */
		for (i = 0; i < INOPB(fs); ++i)
			((struct dinode *)sp->ibp->b_data)[i].di_inumber = 0;

		++sp->start_bpp;
		fs->lfs_avail -= fsbtodb(fs, 1);
		/* Set remaining space counters. */
		sp->seg_bytes_left -= fs->lfs_bsize;
		sp->sum_bytes_left -= sizeof(ufs_daddr_t);
		ndx = LFS_SUMMARY_SIZE / sizeof(ufs_daddr_t) -
			sp->ninodes / INOPB(fs) - 1;
		((ufs_daddr_t *)(sp->segsum))[ndx] = daddr;
	}

	/* Update the inode times and copy the inode onto the inode page. */
	if (ip->i_flag & (IN_CLEANING|IN_MODIFIED))
		--fs->lfs_uinodes;
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	LFS_ITIMES(ip, &ts, &ts, &ts);

	if(ip->i_flag & IN_CLEANING)
		ip->i_flag &= ~IN_CLEANING;
	else
		ip->i_flag &= ~(IN_ACCESS|IN_CHANGE|IN_MODIFIED|IN_UPDATE);

	/*
	 * If this is the Ifile, and we've already written the Ifile in this
	 * partial segment, just overwrite it (it's not on disk yet) and
	 * continue.
	 *
	 * XXX we know that the bp that we get the second time around has
	 * already been gathered.
	 */
	if(ip->i_number == LFS_IFILE_INUM && sp->idp) {
		*(sp->idp) = ip->i_din.ffs_din;
		return 0;
	}

	bp = sp->ibp;
	((struct dinode *)bp->b_data)[sp->ninodes % INOPB(fs)] =
		ip->i_din.ffs_din;
	
	if(ip->i_number == LFS_IFILE_INUM) /* We know sp->idp == NULL */
		sp->idp = ((struct dinode *)bp->b_data)+(sp->ninodes % INOPB(fs));
	if(gotblk) {
		bp->b_flags |= B_LOCKED;
		brelse(bp);
	}
	
	/* Increment inode count in segment summary block. */
	++((SEGSUM *)(sp->segsum))->ss_ninos;
	
	/* If this page is full, set flag to allocate a new page. */
	if (++sp->ninodes % INOPB(fs) == 0)
		sp->ibp = NULL;
	
	/*
	 * If updating the ifile, update the super-block.  Update the disk
	 * address and access times for this inode in the ifile.
	 */
	ino = ip->i_number;
	if (ino == LFS_IFILE_INUM) {
		daddr = fs->lfs_idaddr;
		fs->lfs_idaddr = bp->b_blkno;
	} else {
		LFS_IENTRY(ifp, fs, ino, ibp);
		daddr = ifp->if_daddr;
		ifp->if_daddr = bp->b_blkno;
#ifdef LFS_DEBUG_NEXTFREE
		if(ino > 3 && ifp->if_nextfree) {
			vprint("lfs_writeinode",ITOV(ip));
			printf("lfs_writeinode: updating free ino %d\n",
				ip->i_number);
		}
#endif
		error = VOP_BWRITE(ibp);
	}
	
	/*
	 * No need to update segment usage if there was no former inode address 
	 * or if the last inode address is in the current partial segment.
	 */
	if (daddr != LFS_UNUSED_DADDR &&
	    !(daddr >= fs->lfs_lastpseg && daddr <= bp->b_blkno)) {
		LFS_SEGENTRY(sup, fs, datosn(fs, daddr), bp);
#ifdef DIAGNOSTIC
		if (sup->su_nbytes < DINODE_SIZE) {
			/* XXX -- Change to a panic. */
			printf("lfs_writeinode: negative bytes (segment %d short by %d)\n",
			       datosn(fs, daddr), (int)DINODE_SIZE - sup->su_nbytes);
			panic("lfs_writeinode: negative bytes");
			sup->su_nbytes = DINODE_SIZE;
		}
#endif
		sup->su_nbytes -= DINODE_SIZE;
		redo_ifile =
			(ino == LFS_IFILE_INUM && !(bp->b_flags & B_GATHERED));
		error = VOP_BWRITE(bp);
	}
	return (redo_ifile);
}

int
lfs_gatherblock(sp, bp, sptr)
	struct segment *sp;
	struct buf *bp;
	int *sptr;
{
	struct lfs *fs;
	int version;
	
	/*
	 * If full, finish this segment.  We may be doing I/O, so
	 * release and reacquire the splbio().
	 */
#ifdef DIAGNOSTIC
	if (sp->vp == NULL)
		panic ("lfs_gatherblock: Null vp in segment");
#endif
	fs = sp->fs;
	if (sp->sum_bytes_left < sizeof(ufs_daddr_t) ||
	    sp->seg_bytes_left < bp->b_bcount) {
		if (sptr)
			splx(*sptr);
		lfs_updatemeta(sp);
		
		version = sp->fip->fi_version;
		(void) lfs_writeseg(fs, sp);
		
		sp->fip->fi_version = version;
		sp->fip->fi_ino = VTOI(sp->vp)->i_number;
		/* Add the current file to the segment summary. */
		++((SEGSUM *)(sp->segsum))->ss_nfinfo;
		sp->sum_bytes_left -= 
			sizeof(struct finfo) - sizeof(ufs_daddr_t);
		
		if (sptr)
			*sptr = splbio();
		return(1);
	}
	
#ifdef DEBUG
	if(bp->b_flags & B_GATHERED) {
		printf("lfs_gatherblock: already gathered! Ino %d, lbn %d\n",
		       sp->fip->fi_ino, bp->b_lblkno);
		return(0);
	}
#endif
	/* Insert into the buffer list, update the FINFO block. */
	bp->b_flags |= B_GATHERED;
	*sp->cbpp++ = bp;
	sp->fip->fi_blocks[sp->fip->fi_nblocks++] = bp->b_lblkno;
	
	sp->sum_bytes_left -= sizeof(ufs_daddr_t);
	sp->seg_bytes_left -= bp->b_bcount;
	return(0);
}

int
lfs_gather(fs, sp, vp, match)
	struct lfs *fs;
	struct segment *sp;
	struct vnode *vp;
	int (*match) __P((struct lfs *, struct buf *));
{
	struct buf *bp;
	int s, count=0;
	
	sp->vp = vp;
	s = splbio();

#ifndef LFS_NO_BACKBUF_HACK
loop:	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = bp->b_vnbufs.le_next) {
#else /* LFS_NO_BACKBUF_HACK */
/* This is a hack to see if ordering the blocks in LFS makes a difference. */
# define	BUF_OFFSET	(((void *)&bp->b_vnbufs.le_next) - (void *)bp)
# define	BACK_BUF(BP)	((struct buf *)(((void *)BP->b_vnbufs.le_prev) - BUF_OFFSET))
# define	BEG_OF_LIST	((struct buf *)(((void *)&vp->v_dirtyblkhd.lh_first) - BUF_OFFSET))
/* Find last buffer. */
loop:	for (bp = vp->v_dirtyblkhd.lh_first; bp && bp->b_vnbufs.le_next != NULL;
	    bp = bp->b_vnbufs.le_next);
	for (; bp && bp != BEG_OF_LIST; bp = BACK_BUF(bp)) {
#endif /* LFS_NO_BACKBUF_HACK */
		if ((bp->b_flags & (B_BUSY|B_GATHERED)) || !match(fs, bp))
			continue;
		if(vp->v_type == VBLK) {
			/* For block devices, just write the blocks. */
			/* XXX Do we really need to even do this? */
#ifdef DEBUG_LFS
			if(count==0)
				printf("BLK(");
			printf(".");
#endif
			/* Get the block before bwrite, so we don't corrupt the free list */
			bp->b_flags |= B_BUSY;
			bremfree(bp);
			bwrite(bp);
		} else {
#ifdef DIAGNOSTIC
			if (!(bp->b_flags & B_DELWRI))
				panic("lfs_gather: bp not B_DELWRI");
			if (!(bp->b_flags & B_LOCKED)) {
				printf("lfs_gather: lbn %d blk %d not B_LOCKED\n", bp->b_lblkno, bp->b_blkno);
				VOP_PRINT(bp->b_vp);
				panic("lfs_gather: bp not B_LOCKED");
			}
#endif
			if (lfs_gatherblock(sp, bp, &s)) {
				goto loop;
			}
		}
		count++;
	}
	splx(s);
#ifdef DEBUG_LFS
	if(vp->v_type == VBLK && count)
		printf(")\n");
#endif
	lfs_updatemeta(sp);
	sp->vp = NULL;
	return count;
}

/*
 * Update the metadata that points to the blocks listed in the FINFO
 * array.
 */
void
lfs_updatemeta(sp)
	struct segment *sp;
{
	SEGUSE *sup;
	struct buf *bp;
	struct lfs *fs;
	struct vnode *vp;
	struct indir a[NIADDR + 2], *ap;
	struct inode *ip;
	ufs_daddr_t daddr, lbn, off;
	int error, i, nblocks, num;
	
	vp = sp->vp;
	nblocks = &sp->fip->fi_blocks[sp->fip->fi_nblocks] - sp->start_lbp;
	if (nblocks < 0)
		panic("This is a bad thing\n");
	if (vp == NULL || nblocks == 0) 
		return;
	
	/* Sort the blocks. */
	/*
	 * XXX KS - We have to sort even if the blocks come from the
	 * cleaner, because there might be other pending blocks on the
	 * same inode...and if we don't sort, and there are fragments
	 * present, blocks may be written in the wrong place.
	 */
	/* if (!(sp->seg_flags & SEGM_CLEAN)) */
	lfs_shellsort(sp->start_bpp, sp->start_lbp, nblocks);
	
	/*
	 * Record the length of the last block in case it's a fragment.
	 * If there are indirect blocks present, they sort last.  An
	 * indirect block will be lfs_bsize and its presence indicates
	 * that you cannot have fragments.
	 */
	sp->fip->fi_lastlength = sp->start_bpp[nblocks - 1]->b_bcount;
	
	/*
	 * Assign disk addresses, and update references to the logical
	 * block and the segment usage information.
	 */
	fs = sp->fs;
	for (i = nblocks; i--; ++sp->start_bpp) {
		lbn = *sp->start_lbp++;

		(*sp->start_bpp)->b_blkno = off = fs->lfs_offset;
		if((*sp->start_bpp)->b_blkno == (*sp->start_bpp)->b_lblkno) {
			printf("lfs_updatemeta: ino %d blk %d has same lbn and daddr\n", VTOI(vp)->i_number, off);
		}
		fs->lfs_offset +=
			fragstodb(fs, numfrags(fs, (*sp->start_bpp)->b_bcount));
		error = ufs_bmaparray(vp, lbn, &daddr, a, &num, NULL);
		if (error)
			panic("lfs_updatemeta: ufs_bmaparray %d", error);
		ip = VTOI(vp);
		switch (num) {
		case 0:
			ip->i_ffs_db[lbn] = off;
			break;
		case 1:
			ip->i_ffs_ib[a[0].in_off] = off;
			break;
		default:
			ap = &a[num - 1];
			if (bread(vp, ap->in_lbn, fs->lfs_bsize, NOCRED, &bp))
				panic("lfs_updatemeta: bread bno %d",
				      ap->in_lbn);
			/*
			 * Bread may create a new (indirect) block which needs
			 * to get counted for the inode.
			 */
			if (/* bp->b_blkno == -1 && */
			    !(bp->b_flags & (B_DELWRI|B_DONE))) {
				ip->i_ffs_blocks += fsbtodb(fs, 1);
				fs->lfs_bfree -= fragstodb(fs, fs->lfs_frag);
			}
			((ufs_daddr_t *)bp->b_data)[ap->in_off] = off;
			VOP_BWRITE(bp);
		}
		/* Update segment usage information. */
		if (daddr != UNASSIGNED && !(daddr >= fs->lfs_lastpseg && daddr <= off)) {
			LFS_SEGENTRY(sup, fs, datosn(fs, daddr), bp);
#ifdef DIAGNOSTIC
			if (sup->su_nbytes < (*sp->start_bpp)->b_bcount) {
				/* XXX -- Change to a panic. */
				printf("lfs_updatemeta: negative bytes (segment %d short by %ld)\n",
				       datosn(fs, daddr), (*sp->start_bpp)->b_bcount - sup->su_nbytes);
				printf("lfs_updatemeta: ino %d, lbn %d, addr = %x\n",
				       VTOI(sp->vp)->i_number, (*sp->start_bpp)->b_lblkno, daddr);
				panic("lfs_updatemeta: negative bytes");
				sup->su_nbytes = (*sp->start_bpp)->b_bcount;
			}
#endif
			sup->su_nbytes -= (*sp->start_bpp)->b_bcount;
			error = VOP_BWRITE(bp);
		}
	}
}

/*
 * Start a new segment.
 */
int
lfs_initseg(fs)
	struct lfs *fs;
{
	struct segment *sp;
	SEGUSE *sup;
	SEGSUM *ssp;
	struct buf *bp;
	int repeat;
	
	sp = fs->lfs_sp;
	
	repeat = 0;
	/* Advance to the next segment. */
	if (!LFS_PARTIAL_FITS(fs)) {
		/* Wake up any cleaning procs waiting on this file system. */
		wakeup(&lfs_allclean_wakeup);
		wakeup(&fs->lfs_nextseg);
		lfs_newseg(fs);
		repeat = 1;
		fs->lfs_offset = fs->lfs_curseg;
		sp->seg_number = datosn(fs, fs->lfs_curseg);
		sp->seg_bytes_left = fs->lfs_dbpseg * DEV_BSIZE;
		/*
		 * If the segment contains a superblock, update the offset
		 * and summary address to skip over it.
		 */
		LFS_SEGENTRY(sup, fs, sp->seg_number, bp);
		if (sup->su_flags & SEGUSE_SUPERBLOCK) {
			fs->lfs_offset += LFS_SBPAD / DEV_BSIZE;
			sp->seg_bytes_left -= LFS_SBPAD;
		}
		brelse(bp);
	} else {
		sp->seg_number = datosn(fs, fs->lfs_curseg);
		sp->seg_bytes_left = (fs->lfs_dbpseg -
				      (fs->lfs_offset - fs->lfs_curseg)) * DEV_BSIZE;
	}
	fs->lfs_lastpseg = fs->lfs_offset;
	
	sp->fs = fs;
	sp->ibp = NULL;
	sp->idp = NULL;
	sp->ninodes = 0;
	
	/* Get a new buffer for SEGSUM and enter it into the buffer list. */
	sp->cbpp = sp->bpp;
	*sp->cbpp = lfs_newbuf(VTOI(fs->lfs_ivnode)->i_devvp,
			       fs->lfs_offset, LFS_SUMMARY_SIZE);
	sp->segsum = (*sp->cbpp)->b_data;
	bzero(sp->segsum, LFS_SUMMARY_SIZE);
	sp->start_bpp = ++sp->cbpp;
	fs->lfs_offset += LFS_SUMMARY_SIZE / DEV_BSIZE;
	
	/* Set point to SEGSUM, initialize it. */
	ssp = sp->segsum;
	ssp->ss_next = fs->lfs_nextseg;
	ssp->ss_nfinfo = ssp->ss_ninos = 0;
	ssp->ss_magic = SS_MAGIC;

	/* Set pointer to first FINFO, initialize it. */
	sp->fip = (struct finfo *)((caddr_t)sp->segsum + sizeof(SEGSUM));
	sp->fip->fi_nblocks = 0;
	sp->start_lbp = &sp->fip->fi_blocks[0];
	sp->fip->fi_lastlength = 0;
	
	sp->seg_bytes_left -= LFS_SUMMARY_SIZE;
	sp->sum_bytes_left = LFS_SUMMARY_SIZE - sizeof(SEGSUM);
	
	return(repeat);
}

/*
 * Return the next segment to write.
 */
void
lfs_newseg(fs)
	struct lfs *fs;
{
	CLEANERINFO *cip;
	SEGUSE *sup;
	struct buf *bp;
	int curseg, isdirty, sn;
	
	LFS_SEGENTRY(sup, fs, datosn(fs, fs->lfs_nextseg), bp);
	sup->su_flags |= SEGUSE_DIRTY | SEGUSE_ACTIVE;
	sup->su_nbytes = 0;
	sup->su_nsums = 0;
	sup->su_ninos = 0;
	(void) VOP_BWRITE(bp);

	LFS_CLEANERINFO(cip, fs, bp);
	--cip->clean;
	++cip->dirty;
	fs->lfs_nclean = cip->clean;
	(void) VOP_BWRITE(bp);
	
	fs->lfs_lastseg = fs->lfs_curseg;
	fs->lfs_curseg = fs->lfs_nextseg;
	for (sn = curseg = datosn(fs, fs->lfs_curseg);;) {
		sn = (sn + 1) % fs->lfs_nseg;
		if (sn == curseg)
			panic("lfs_nextseg: no clean segments");
		LFS_SEGENTRY(sup, fs, sn, bp);
		isdirty = sup->su_flags & SEGUSE_DIRTY;
		brelse(bp);
		if (!isdirty)
			break;
	}
	
	++fs->lfs_nactive;
	fs->lfs_nextseg = sntoda(fs, sn);
	if(lfs_dostats) {
		++lfs_stats.segsused;
	}
}

int
lfs_writeseg(fs, sp)
	struct lfs *fs;
	struct segment *sp;
{
	extern int locked_queue_count;
	extern long locked_queue_bytes;
	struct buf **bpp, *bp, *cbp;
	SEGUSE *sup;
	SEGSUM *ssp;
	dev_t i_dev;
	u_long *datap, *dp;
	int do_again, i, nblocks, s;
#ifdef LFS_TRACK_IOS
	int j;
#endif
	int (*strategy)__P((void *));
	struct vop_strategy_args vop_strategy_a;
	u_short ninos;
	struct vnode *devvp;
	char *p;
	struct vnode *vn;
	struct inode *ip;
#if defined(DEBUG) && defined(LFS_PROPELLER)
	static int propeller;
	char propstring[4] = "-\\|/";
	
	printf("%c\b",propstring[propeller++]);
	if(propeller==4)
		propeller = 0;
#endif
	
	/*
	 * If there are no buffers other than the segment summary to write
	 * and it is not a checkpoint, don't do anything.  On a checkpoint,
	 * even if there aren't any buffers, you need to write the superblock.
	 */
	if ((nblocks = sp->cbpp - sp->bpp) == 1)
		return (0);
	
#ifdef DEBUG_LFS
	lfs_check_bpp(fs,sp,__FILE__,__LINE__);
#endif
	i_dev = VTOI(fs->lfs_ivnode)->i_dev;
	devvp = VTOI(fs->lfs_ivnode)->i_devvp;

	/* Update the segment usage information. */
	LFS_SEGENTRY(sup, fs, sp->seg_number, bp);
	
	/* Loop through all blocks, except the segment summary. */
	for (bpp = sp->bpp; ++bpp < sp->cbpp; ) {
		if((*bpp)->b_vp != devvp)
			sup->su_nbytes += (*bpp)->b_bcount;
	}
	
	ssp = (SEGSUM *)sp->segsum;
	
	ninos = (ssp->ss_ninos + INOPB(fs) - 1) / INOPB(fs);
	sup->su_nbytes += ssp->ss_ninos * DINODE_SIZE;
	/* sup->su_nbytes += LFS_SUMMARY_SIZE; */
	sup->su_lastmod = time.tv_sec;
	sup->su_ninos += ninos;
	++sup->su_nsums;

	do_again = !(bp->b_flags & B_GATHERED);
	(void)VOP_BWRITE(bp);
	/*
	 * Compute checksum across data and then across summary; the first
	 * block (the summary block) is skipped.  Set the create time here
	 * so that it's guaranteed to be later than the inode mod times.
	 *
	 * XXX
	 * Fix this to do it inline, instead of malloc/copy.
	 */
	datap = dp = malloc(nblocks * sizeof(u_long), M_SEGMENT, M_WAITOK);
	for (bpp = sp->bpp, i = nblocks - 1; i--;) {
		if (((*++bpp)->b_flags & (B_CALL|B_INVAL)) == (B_CALL|B_INVAL)) {
			if (copyin((*bpp)->b_saveaddr, dp++, sizeof(u_long)))
				panic("lfs_writeseg: copyin failed [1]: ino %d blk %d", VTOI((*bpp)->b_vp)->i_number, (*bpp)->b_lblkno);
		} else {
			if( !((*bpp)->b_flags & B_CALL) ) {
				/*
				 * Before we record data for a checksm,
				 * make sure the data won't change in between
				 * the checksum calculation and the write,
				 * by marking the buffer B_BUSY.  It will
				 * be freed later by brelse().
				 */
			again:
				s = splbio();
				if((*bpp)->b_flags & B_BUSY) {
#ifdef DEBUG
					printf("lfs_writeseg: avoiding potential data summary corruption for ino %d, lbn %d\n",
					       VTOI((*bpp)->b_vp)->i_number,
					       bp->b_lblkno);
#endif
					(*bpp)->b_flags |= B_WANTED;
					tsleep((*bpp), (PRIBIO + 1),
					       "lfs_writeseg", 0);
					splx(s);
					goto again;
				}
				(*bpp)->b_flags |= B_BUSY;
				splx(s);
			}
			*dp++ = ((u_long *)(*bpp)->b_data)[0];
		}
	}
	ssp->ss_create = time.tv_sec;
	ssp->ss_datasum = cksum(datap, (nblocks - 1) * sizeof(u_long));
	ssp->ss_sumsum =
	    cksum(&ssp->ss_datasum, LFS_SUMMARY_SIZE - sizeof(ssp->ss_sumsum));
	free(datap, M_SEGMENT);
#ifdef DIAGNOSTIC
	if (fs->lfs_bfree < fsbtodb(fs, ninos) + LFS_SUMMARY_SIZE / DEV_BSIZE)
		panic("lfs_writeseg: No diskspace for summary");
#endif
	fs->lfs_bfree -= (fsbtodb(fs, ninos) + LFS_SUMMARY_SIZE / DEV_BSIZE);

	strategy = devvp->v_op[VOFFSET(vop_strategy)];

	/*
	 * When we simply write the blocks we lose a rotation for every block
	 * written.  To avoid this problem, we allocate memory in chunks, copy
	 * the buffers into the chunk and write the chunk.  CHUNKSIZE is the
	 * largest size I/O devices can handle.
	 * When the data is copied to the chunk, turn off the the B_LOCKED bit
	 * and brelse the buffer (which will move them to the LRU list).  Add
	 * the B_CALL flag to the buffer header so we can count I/O's for the
	 * checkpoints and so we can release the allocated memory.
	 *
	 * XXX
	 * This should be removed if the new virtual memory system allows us to
	 * easily make the buffers contiguous in kernel memory and if that's
	 * fast enough.
	 */

#define CHUNKSIZE MAXPHYS

	if(devvp==NULL)
		panic("devvp is NULL");
	for (bpp = sp->bpp,i = nblocks; i;) {
		cbp = lfs_newbuf(devvp, (*bpp)->b_blkno, CHUNKSIZE);
		cbp->b_dev = i_dev;
		cbp->b_flags |= B_ASYNC | B_BUSY;
		cbp->b_bcount = 0;

#ifdef DIAGNOSTIC
		if(datosn(fs,(*bpp)->b_blkno + ((*bpp)->b_bcount - 1)/DEV_BSIZE) != datosn(fs,cbp->b_blkno)) {
			panic("lfs_writeseg: Segment overwrite");
		}
#endif

		if(fs->lfs_iocount >= LFS_THROTTLE) {
			tsleep(&fs->lfs_iocount, PRIBIO+1, "lfs throttle", 0);
		}
		s = splbio();
		++fs->lfs_iocount;
#ifdef LFS_TRACK_IOS
		for(j=0;j<LFS_THROTTLE;j++) {
			if(fs->lfs_pending[j]==LFS_UNUSED_DADDR) {
				fs->lfs_pending[j] = cbp->b_blkno;
				break;
			}
		}
#endif /* LFS_TRACK_IOS */
		for (p = cbp->b_data; i && cbp->b_bcount < CHUNKSIZE; i--) {
			bp = *bpp;

			if (bp->b_bcount > (CHUNKSIZE - cbp->b_bcount))
				break;

			/*
			 * Fake buffers from the cleaner are marked as B_INVAL.
			 * We need to copy the data from user space rather than
			 * from the buffer indicated.
			 * XXX == what do I do on an error?
			 */
			if ((bp->b_flags & (B_CALL|B_INVAL)) == (B_CALL|B_INVAL)) {
				if (copyin(bp->b_saveaddr, p, bp->b_bcount))
					panic("lfs_writeseg: copyin failed [2]");
			} else
				bcopy(bp->b_data, p, bp->b_bcount);
			p += bp->b_bcount;
			cbp->b_bcount += bp->b_bcount;
			if (bp->b_flags & B_LOCKED) {
				--locked_queue_count;
				locked_queue_bytes -= bp->b_bufsize;
			}
			bp->b_flags &= ~(B_ERROR | B_READ | B_DELWRI |
					 B_LOCKED | B_GATHERED);
			vn = bp->b_vp;
			if (bp->b_flags & B_CALL) {
				/* if B_CALL, it was created with newbuf */
				lfs_freebuf(bp);
			} else {
				bremfree(bp);
				bp->b_flags |= B_DONE;
				if(vn)
					reassignbuf(bp, vn);
				brelse(bp);
			}
			if(bp->b_flags & B_NEEDCOMMIT) { /* XXX */
				bp->b_flags &= ~B_NEEDCOMMIT;
				wakeup(bp);
			}

			bpp++;

			/*
			 * If this is the last block for this vnode, but
			 * there are other blocks on its dirty list,
			 * set IN_MODIFIED/IN_CLEANING depending on what
			 * sort of block.  Only do this for our mount point,
			 * not for, e.g., inode blocks that are attached to
			 * the devvp.
			 */
			if(i>1 && vn && *bpp && (*bpp)->b_vp != vn
			   && (*bpp)->b_vp && (bp=vn->v_dirtyblkhd.lh_first)!=NULL &&
			   vn->v_mount == fs->lfs_ivnode->v_mount)
			{
				ip = VTOI(vn);
#ifdef DEBUG_LFS
				printf("lfs_writeseg: marking ino %d\n",ip->i_number);
#endif
		       		if(!(ip->i_flag & (IN_CLEANING|IN_MODIFIED))) {
					fs->lfs_uinodes++;
					if(bp->b_flags & B_CALL)
						ip->i_flag |= IN_CLEANING;
					else
						ip->i_flag |= IN_MODIFIED;
				}
			}
			/* if(vn->v_dirtyblkhd.lh_first == NULL) */
				wakeup(vn);
		}
		++cbp->b_vp->v_numoutput;
		splx(s);
		/*
		 * XXXX This is a gross and disgusting hack.  Since these
		 * buffers are physically addressed, they hang off the
		 * device vnode (devvp).  As a result, they have no way
		 * of getting to the LFS superblock or lfs structure to
		 * keep track of the number of I/O's pending.  So, I am
		 * going to stuff the fs into the saveaddr field of
		 * the buffer (yuk).
		 */
		cbp->b_saveaddr = (caddr_t)fs;
		vop_strategy_a.a_desc = VDESC(vop_strategy);
		vop_strategy_a.a_bp = cbp;
		(strategy)(&vop_strategy_a);
	}
	/*
	 * XXX
	 * Vinvalbuf can move locked buffers off the locked queue
	 * and we have no way of knowing about this.  So, after
	 * doing a big write, we recalculate how many buffers are
	 * really still left on the locked queue.
	 */
	lfs_countlocked(&locked_queue_count,&locked_queue_bytes);
	wakeup(&locked_queue_count);
	if(lfs_dostats) {
		++lfs_stats.psegwrites;
		lfs_stats.blocktot += nblocks - 1;
		if (fs->lfs_sp->seg_flags & SEGM_SYNC)
			++lfs_stats.psyncwrites;
		if (fs->lfs_sp->seg_flags & SEGM_CLEAN) {
			++lfs_stats.pcleanwrites;
			lfs_stats.cleanblocks += nblocks - 1;
		}
	}
	return (lfs_initseg(fs) || do_again);
}

void
lfs_writesuper(fs, daddr)
	struct lfs *fs;
	daddr_t daddr;
{
	struct buf *bp;
	dev_t i_dev;
	int (*strategy) __P((void *));
	int s;
	struct vop_strategy_args vop_strategy_a;

#ifdef LFS_CANNOT_ROLLFW
	/*
	 * If we can write one superblock while another is in
	 * progress, we risk not having a complete checkpoint if we crash.
	 * So, block here if a superblock write is in progress.
	 *
	 * XXX - should be a proper lock, not this hack
	 */
	while(fs->lfs_sbactive) {
		tsleep(&fs->lfs_sbactive, PRIBIO+1, "lfs sb", 0);
	}
	fs->lfs_sbactive = daddr;
#endif
	i_dev = VTOI(fs->lfs_ivnode)->i_dev;
	strategy = VTOI(fs->lfs_ivnode)->i_devvp->v_op[VOFFSET(vop_strategy)];

	/* Set timestamp of this version of the superblock */
	fs->lfs_tstamp = time.tv_sec;

	/* Checksum the superblock and copy it into a buffer. */
	fs->lfs_cksum = lfs_sb_cksum(&(fs->lfs_dlfs));
	bp = lfs_newbuf(VTOI(fs->lfs_ivnode)->i_devvp, daddr, LFS_SBPAD);
	*(struct dlfs *)bp->b_data = fs->lfs_dlfs;
	
	bp->b_dev = i_dev;
	bp->b_flags |= B_BUSY | B_CALL | B_ASYNC;
	bp->b_flags &= ~(B_DONE | B_ERROR | B_READ | B_DELWRI);
	bp->b_iodone = lfs_supercallback;
	/* XXX KS - same nasty hack as above */
	bp->b_saveaddr = (caddr_t)fs;

	vop_strategy_a.a_desc = VDESC(vop_strategy);
	vop_strategy_a.a_bp = bp;
	s = splbio();
	++bp->b_vp->v_numoutput;
	splx(s);
	(strategy)(&vop_strategy_a);
}

/*
 * Logical block number match routines used when traversing the dirty block
 * chain.
 */
int
lfs_match_fake(fs, bp)
	struct lfs *fs;
	struct buf *bp;
{
	return (bp->b_flags & B_CALL);
}

int
lfs_match_data(fs, bp)
	struct lfs *fs;
	struct buf *bp;
{
	return (bp->b_lblkno >= 0);
}

int
lfs_match_indir(fs, bp)
	struct lfs *fs;
	struct buf *bp;
{
	int lbn;

	lbn = bp->b_lblkno;
	return (lbn < 0 && (-lbn - NDADDR) % NINDIR(fs) == 0);
}

int
lfs_match_dindir(fs, bp)
	struct lfs *fs;
	struct buf *bp;
{
	int lbn;

	lbn = bp->b_lblkno;
	return (lbn < 0 && (-lbn - NDADDR) % NINDIR(fs) == 1);
}

int
lfs_match_tindir(fs, bp)
	struct lfs *fs;
	struct buf *bp;
{
	int lbn;

	lbn = bp->b_lblkno;
	return (lbn < 0 && (-lbn - NDADDR) % NINDIR(fs) == 2);
}

/*
 * XXX - The only buffers that are going to hit these functions are the
 * segment write blocks, or the segment summaries, or the superblocks.
 * 
 * All of the above are created by lfs_newbuf, and so do not need to be
 * released via brelse.
 */
void
lfs_callback(bp)
	struct buf *bp;
{
	struct lfs *fs;
#ifdef LFS_TRACK_IOS
	int j;
#endif

	fs = (struct lfs *)bp->b_saveaddr;
#ifdef DIAGNOSTIC
	if (fs->lfs_iocount == 0)
		panic("lfs_callback: zero iocount\n");
#endif
	if (--fs->lfs_iocount < LFS_THROTTLE)
		wakeup(&fs->lfs_iocount);
#ifdef LFS_TRACK_IOS
	for(j=0;j<LFS_THROTTLE;j++) {
		if(fs->lfs_pending[j]==bp->b_blkno) {
			fs->lfs_pending[j] = LFS_UNUSED_DADDR;
			wakeup(&(fs->lfs_pending[j]));
			break;
		}
	}
#endif /* LFS_TRACK_IOS */

	lfs_freebuf(bp);
}

void
lfs_supercallback(bp)
	struct buf *bp;
{
#ifdef LFS_CANNOT_ROLLFW
	struct lfs *fs;

	fs = (struct lfs *)bp->b_saveaddr;
	fs->lfs_sbactive=NULL;
	wakeup(&fs->lfs_sbactive);
#endif
	lfs_freebuf(bp);
}

/*
 * Shellsort (diminishing increment sort) from Data Structures and
 * Algorithms, Aho, Hopcraft and Ullman, 1983 Edition, page 290;
 * see also Knuth Vol. 3, page 84.  The increments are selected from
 * formula (8), page 95.  Roughly O(N^3/2).
 */
/*
 * This is our own private copy of shellsort because we want to sort
 * two parallel arrays (the array of buffer pointers and the array of
 * logical block numbers) simultaneously.  Note that we cast the array
 * of logical block numbers to a unsigned in this routine so that the
 * negative block numbers (meta data blocks) sort AFTER the data blocks.
 */

void
lfs_shellsort(bp_array, lb_array, nmemb)
	struct buf **bp_array;
	ufs_daddr_t *lb_array;
	register int nmemb;
{
	static int __rsshell_increments[] = { 4, 1, 0 };
	register int incr, *incrp, t1, t2;
	struct buf *bp_temp;
	u_long lb_temp;

	for (incrp = __rsshell_increments; (incr = *incrp++) != 0;)
		for (t1 = incr; t1 < nmemb; ++t1)
			for (t2 = t1 - incr; t2 >= 0;)
				if (lb_array[t2] > lb_array[t2 + incr]) {
					lb_temp = lb_array[t2];
					lb_array[t2] = lb_array[t2 + incr];
					lb_array[t2 + incr] = lb_temp;
					bp_temp = bp_array[t2];
					bp_array[t2] = bp_array[t2 + incr];
					bp_array[t2 + incr] = bp_temp;
					t2 -= incr;
				} else
					break;
}

/*
 * Check VXLOCK.  Return 1 if the vnode is locked.  Otherwise, vget it.
 */
int
lfs_vref(vp)
	register struct vnode *vp;
{
	/*
	 * If we return 1 here during a flush, we risk vinvalbuf() not
	 * being able to flush all of the pages from this vnode, which
	 * will cause it to panic.  So, return 0 if a flush is in progress.
	 */
	if (vp->v_flag & VXLOCK) {
		if(IS_FLUSHING(VTOI(vp)->i_lfs,vp)) {
			return 0;
		}
		return(1);
	}
	return (vget(vp, 0));
}

/*
 * This is vrele except that we do not want to VOP_INACTIVE this vnode. We
 * inline vrele here to avoid the vn_lock and VOP_INACTIVE call at the end.
 */
void
lfs_vunref(vp)
	register struct vnode *vp;
{
	/*
	 * Analogous to lfs_vref, if the node is flushing, fake it.
	 */
	if((vp->v_flag & VXLOCK) && IS_FLUSHING(VTOI(vp)->i_lfs,vp)) {
		return;
	}

	simple_lock(&vp->v_interlock);
#ifdef DIAGNOSTIC
	if(vp->v_usecount<=0) {
		printf("lfs_vunref: flags are 0x%lx\n", vp->v_flag);
		printf("lfs_vunref: usecount = %ld\n", vp->v_usecount);
		panic("lfs_vunref: v_usecount<0");
	}
#endif
	vp->v_usecount--;
	if (vp->v_usecount > 0) {
		simple_unlock(&vp->v_interlock);
		return;
	}
#ifdef DIAGNOSTIC
	if(VOP_ISLOCKED(vp))
		panic("lfs_vunref: vnode locked");
#endif
	/*
	 * insert at tail of LRU list
	 */
	simple_lock(&vnode_free_list_slock);
	TAILQ_INSERT_TAIL(&vnode_free_list, vp, v_freelist);
	simple_unlock(&vnode_free_list_slock);
	simple_unlock(&vp->v_interlock);
}

/*
 * We use this when we have vnodes that were loaded in solely for cleaning.
 * There is no reason to believe that these vnodes will be referenced again
 * soon, since the cleaning process is unrelated to normal filesystem
 * activity.  Putting cleaned vnodes at the tail of the list has the effect
 * of flushing the vnode LRU.  So, put vnodes that were loaded only for
 * cleaning at the head of the list, instead.
 */
void
lfs_vunref_head(vp)
	register struct vnode *vp;
{
	simple_lock(&vp->v_interlock);
#ifdef DIAGNOSTIC
	if(vp->v_usecount==0) {
		panic("lfs_vunref: v_usecount<0");
	}
#endif
	vp->v_usecount--;
	if (vp->v_usecount > 0) {
		simple_unlock(&vp->v_interlock);
		return;
	}
#ifdef DIAGNOSTIC
	if(VOP_ISLOCKED(vp))
		panic("lfs_vunref_head: vnode locked");
#endif
	/*
	 * insert at head of LRU list
	 */
	simple_lock(&vnode_free_list_slock);
	TAILQ_INSERT_HEAD(&vnode_free_list, vp, v_freelist);
	simple_unlock(&vnode_free_list_slock);
	simple_unlock(&vp->v_interlock);
}

