/*	$NetBSD: lfs_alloc.c,v 1.69.2.1 2003/07/02 15:27:23 darrenr Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003 The NetBSD Foundation, Inc.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 *	@(#)lfs_alloc.c	8.4 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_alloc.c,v 1.69.2.1 2003/07/02 15:27:23 darrenr Exp $");

#if defined(_KERNEL_OPT)
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/syslog.h>
#include <sys/mount.h>
#include <sys/pool.h>
#include <sys/proc.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

extern int lfs_dirvcount;
extern struct lock ufs_hashlock;

static int extend_ifile(struct lfs *, struct ucred *);
static int lfs_ialloc(struct lfs *, struct vnode *, ino_t, int, struct vnode **);

/*
 * Allocate a particular inode with a particular version number, freeing
 * any previous versions of this inode that may have gone before.
 * Used by the roll-forward code.
 *
 * XXX this function does not have appropriate locking to be used on a live fs;
 * XXX but something similar could probably be used for an "undelete" call.
 *
 * Called with the Ifile inode locked.
 */
int
lfs_rf_valloc(struct lfs *fs, ino_t ino, int version, struct lwp *l,
	      struct vnode **vpp)
{
	IFILE *ifp;
	struct buf *bp, *cbp;
	struct vnode *vp;
	struct inode *ip;
	ino_t tino, oldnext;
	int error;
	CLEANERINFO *cip;

	/*
	 * First, just try a vget. If the version number is the one we want,
	 * we don't have to do anything else.  If the version number is wrong,
	 * take appropriate action.
	 */
	error = VFS_VGET(fs->lfs_ivnode->v_mount, ino, &vp, l);
	if (error == 0) {
		/* printf("lfs_rf_valloc[1]: ino %d vp %p\n", ino, vp); */

		*vpp = vp;
		ip = VTOI(vp);
		if (ip->i_gen == version)
			return 0;
		else if (ip->i_gen < version) {
			VOP_TRUNCATE(vp, (off_t)0, 0, NOCRED, l);
			ip->i_gen = ip->i_ffs1_gen = version;
			LFS_SET_UINO(ip, IN_CHANGE | IN_MODIFIED | IN_UPDATE);
			return 0;
		} else {
			/* printf("ino %d: asked for version %d but got %d\n",
			       ino, version, ip->i_ffs1_gen); */
			vput(vp);
			*vpp = NULLVP;
			return EEXIST;
		}
	}

	/*
	 * The inode is not in use.  Find it on the free list.
	 */
	/* If the Ifile is too short to contain this inum, extend it */
	while (VTOI(fs->lfs_ivnode)->i_size <= (ino / 
		fs->lfs_ifpb + fs->lfs_cleansz + fs->lfs_segtabsz) 
		<< fs->lfs_bshift) {
		extend_ifile(fs, NOCRED);
	}

	LFS_IENTRY(ifp, fs, ino, bp);
	oldnext = ifp->if_nextfree;
	ifp->if_version = version;
	brelse(bp);

	LFS_GET_HEADFREE(fs, cip, cbp, &ino);
	if (ino) {
		LFS_PUT_HEADFREE(fs, cip, cbp, oldnext);
	} else {
		tino = ino;
		while (1) {
			LFS_IENTRY(ifp, fs, tino, bp);
			if (ifp->if_nextfree == ino ||
			    ifp->if_nextfree == LFS_UNUSED_INUM)
				break;
			tino = ifp->if_nextfree;
			brelse(bp);
		}
		if (ifp->if_nextfree == LFS_UNUSED_INUM) {
			brelse(bp);
			return ENOENT;
		}
		ifp->if_nextfree = oldnext;
		LFS_BWRITE_LOG(bp);
	}

	error = lfs_ialloc(fs, fs->lfs_ivnode, ino, version, &vp);
	if (error == 0) {
		/*
		 * Make it VREG so we can put blocks on it.  We will change
		 * this later if it turns out to be some other kind of file.
		 */
		ip = VTOI(vp);
		ip->i_mode = ip->i_ffs1_mode = IFREG;
		ip->i_nlink = ip->i_ffs1_nlink = 1;
		ip->i_ffs_effnlink = 1;
		ufs_vinit(vp->v_mount, lfs_specop_p, lfs_fifoop_p, &vp);
		ip = VTOI(vp);

		/* printf("lfs_rf_valloc: ino %d vp %p\n", ino, vp); */

		/* The dirop-nature of this vnode is past */
		(void)lfs_vunref(vp);
		--lfs_dirvcount;
		vp->v_flag &= ~VDIROP;
		TAILQ_REMOVE(&fs->lfs_dchainhd, ip, i_lfs_dchain);
		--fs->lfs_nadirop;
		ip->i_flag &= ~IN_ADIROP;
	}
	*vpp = vp;
	return error;
}

/*
 * Add a new block to the Ifile, to accommodate future file creations.
 * Called with the segment lock held.
 */
static int
extend_ifile(struct lfs *fs, struct ucred *cred)
{
	struct vnode *vp;
	struct inode *ip;
	IFILE *ifp;
	IFILE_V1 *ifp_v1;
	struct buf *bp, *cbp;
	int error;
	daddr_t i, blkno, max;
	ino_t oldlast;
	CLEANERINFO *cip;

	vp = fs->lfs_ivnode;
	ip = VTOI(vp);
	blkno = lblkno(fs, ip->i_size);
	if ((error = VOP_BALLOC(vp, ip->i_size, fs->lfs_bsize, cred, 0,
				&bp)) != 0) {
		return (error);
	}
	ip->i_size += fs->lfs_bsize;
	ip->i_ffs1_size = ip->i_size;
	uvm_vnp_setsize(vp, ip->i_size);
	
	i = (blkno - fs->lfs_segtabsz - fs->lfs_cleansz) *
		fs->lfs_ifpb;
	LFS_GET_HEADFREE(fs, cip, cbp, &oldlast);
	LFS_PUT_HEADFREE(fs, cip, cbp, i);
#ifdef DIAGNOSTIC
	if (fs->lfs_freehd == LFS_UNUSED_INUM)
		panic("inode 0 allocated [2]");
#endif /* DIAGNOSTIC */
	max = i + fs->lfs_ifpb;
	/* printf("extend_ifile: new block ino %d--%d\n", i, max - 1); */

	if (fs->lfs_version == 1) {
		for (ifp_v1 = (IFILE_V1 *)bp->b_data; i < max; ++ifp_v1) {
			ifp_v1->if_version = 1;
			ifp_v1->if_daddr = LFS_UNUSED_DADDR;
			ifp_v1->if_nextfree = ++i;
		}
		ifp_v1--;
		ifp_v1->if_nextfree = oldlast;
	} else {
		for (ifp = (IFILE *)bp->b_data; i < max; ++ifp) {
			ifp->if_version = 1;
			ifp->if_daddr = LFS_UNUSED_DADDR;
			ifp->if_nextfree = ++i;
		}
		ifp--;
		ifp->if_nextfree = oldlast;
	}
	LFS_PUT_TAILFREE(fs, cip, cbp, max - 1);

	(void) LFS_BWRITE_LOG(bp); /* Ifile */

	return 0;
}

/* Allocate a new inode. */
/* ARGSUSED */
/* VOP_BWRITE 2i times */
int
lfs_valloc(void *v)
{
	struct vop_valloc_args /* {
				  struct vnode *a_pvp;
				  int a_mode;
				  struct ucred *a_cred;
				  struct vnode **a_vpp;
				  } */ *ap = v;
	struct lfs *fs;
	struct buf *bp, *cbp;
	struct ifile *ifp;
	ino_t new_ino;
	int error;
	int new_gen;
	CLEANERINFO *cip;

	fs = VTOI(ap->a_pvp)->i_lfs;
	if (fs->lfs_ronly)
		return EROFS;
	*ap->a_vpp = NULL;
	
	lfs_seglock(fs, SEGM_PROT);

	/* Get the head of the freelist. */
	LFS_GET_HEADFREE(fs, cip, cbp, &new_ino);

#ifdef DIAGNOSTIC
	if (new_ino == LFS_UNUSED_INUM) {
#ifdef DEBUG
		lfs_dump_super(fs);
#endif /* DEBUG */
		panic("inode 0 allocated [1]");
	}
#endif /* DIAGNOSTIC */
#ifdef ALLOCPRINT
	printf("lfs_valloc: allocate inode %d\n", new_ino);
#endif
	
	/*
	 * Remove the inode from the free list and write the new start
	 * of the free list into the superblock.
	 */
	LFS_IENTRY(ifp, fs, new_ino, bp);
	if (ifp->if_daddr != LFS_UNUSED_DADDR)
		panic("lfs_valloc: inuse inode %d on the free list", new_ino);
	LFS_PUT_HEADFREE(fs, cip, cbp, ifp->if_nextfree);
	/* printf("lfs_valloc: headfree %d -> %d\n", new_ino, ifp->if_nextfree); */

	new_gen = ifp->if_version; /* version was updated by vfree */
	brelse(bp);

	/* Extend IFILE so that the next lfs_valloc will succeed. */
	if (fs->lfs_freehd == LFS_UNUSED_INUM) {
		if ((error = extend_ifile(fs, ap->a_cred)) != 0) {
			LFS_PUT_HEADFREE(fs, cip, cbp, new_ino);
			lfs_segunlock(fs);
			return error;
		}
	}
#ifdef DIAGNOSTIC
	if (fs->lfs_freehd == LFS_UNUSED_INUM)
		panic("inode 0 allocated [3]");
#endif /* DIAGNOSTIC */

	lfs_segunlock(fs);

	return lfs_ialloc(fs, ap->a_pvp, new_ino, new_gen, ap->a_vpp);
}

/*
 * Finish allocating a new inode, given an inode and generation number.
 */
static int
lfs_ialloc(struct lfs *fs, struct vnode *pvp, ino_t new_ino, int new_gen,
	   struct vnode **vpp)
{
	struct inode *ip;
	struct vnode *vp;
	IFILE *ifp;
	struct buf *bp, *cbp;
	int error;
	CLEANERINFO *cip;

	error = getnewvnode(VT_LFS, pvp->v_mount, lfs_vnodeop_p, &vp);
	/* printf("lfs_ialloc: ino %d vp %p error %d\n", new_ino, vp, error);*/
	if (error)
		goto errout;

	lockmgr(&ufs_hashlock, LK_EXCLUSIVE, 0);
	/* Create an inode to associate with the vnode. */
	lfs_vcreate(pvp->v_mount, new_ino, vp);

	ip = VTOI(vp);
	LFS_SET_UINO(ip, IN_CHANGE | IN_MODIFIED);
	/* on-disk structure has been zeroed out by lfs_vcreate */
	ip->i_din.ffs1_din->di_inumber = new_ino;

	/* Set a new generation number for this inode. */
	if (new_gen) {
		ip->i_gen = new_gen;
		ip->i_ffs1_gen = new_gen;
	}

	/* Insert into the inode hash table. */
	ufs_ihashins(ip);
	lockmgr(&ufs_hashlock, LK_RELEASE, 0);

	ufs_vinit(vp->v_mount, lfs_specop_p, lfs_fifoop_p, &vp);
	ip = VTOI(vp);
	/* printf("lfs_ialloc[2]: ino %d vp %p\n", new_ino, vp);*/

	memset(ip->i_lfs_fragsize, 0, NDADDR * sizeof(*ip->i_lfs_fragsize));

	uvm_vnp_setsize(vp, 0);
	*vpp = vp;
	if (!(vp->v_flag & VDIROP)) {
		(void)lfs_vref(vp);
		++lfs_dirvcount;
		TAILQ_INSERT_TAIL(&fs->lfs_dchainhd, ip, i_lfs_dchain);
	}
	vp->v_flag |= VDIROP;

	if (!(ip->i_flag & IN_ADIROP))
		++fs->lfs_nadirop;
	ip->i_flag |= IN_ADIROP;
	genfs_node_init(vp, &lfs_genfsops);
	VREF(ip->i_devvp);
	/* Set superblock modified bit and increment file count. */
	fs->lfs_fmod = 1;
	++fs->lfs_nfiles;
	return (0);

    errout:
	/*
	 * Put the new inum back on the free list.
	 */
	lfs_seglock(fs, SEGM_PROT);
	LFS_IENTRY(ifp, fs, new_ino, bp);
	ifp->if_daddr = LFS_UNUSED_DADDR;
	LFS_GET_HEADFREE(fs, cip, cbp, &(ifp->if_nextfree));
	LFS_PUT_HEADFREE(fs, cip, cbp, new_ino);
	(void) LFS_BWRITE_LOG(bp); /* Ifile */
	lfs_segunlock(fs);

	*vpp = NULLVP;
	return (error);
}

/* Create a new vnode/inode pair and initialize what fields we can. */
void
lfs_vcreate(struct mount *mp, ino_t ino, struct vnode *vp)
{
	struct inode *ip;
	struct ufs1_dinode *dp;
	struct ufsmount *ump;
#ifdef QUOTA
	int i;
#endif
	
	/* Get a pointer to the private mount structure. */
	ump = VFSTOUFS(mp);
	
	/* Initialize the inode. */
	ip = pool_get(&lfs_inode_pool, PR_WAITOK);
	memset(ip, 0, sizeof(*ip));
	dp = pool_get(&lfs_dinode_pool, PR_WAITOK);
	memset(dp, 0, sizeof(*dp));
	ip->inode_ext.lfs = pool_get(&lfs_inoext_pool, PR_WAITOK);
	vp->v_data = ip;
	ip->i_din.ffs1_din = dp;
	ip->i_ump = ump;
	ip->i_vnode = vp;
	ip->i_devvp = ump->um_devvp;
	ip->i_dev = ump->um_dev;
	ip->i_number = dp->di_inumber = ino;
	ip->i_lfs = ump->um_lfs;
	ip->i_lfs_effnblks = 0;
#ifdef QUOTA
	for (i = 0; i < MAXQUOTAS; i++)
		ip->i_dquot[i] = NODQUOT;
#endif
#ifdef DEBUG_LFS_VNLOCK
	if (ino == LFS_IFILE_INUM)
		vp->v_vnlock->lk_wmesg = "inlock";
#endif
}

/* Free an inode. */
/* ARGUSED */
/* VOP_BWRITE 2i times */
int
lfs_vfree(void *v)
{
	struct vop_vfree_args /* {
				 struct vnode *a_pvp;
				 ino_t a_ino;
				 int a_mode;
				 } */ *ap = v;
	SEGUSE *sup;
	CLEANERINFO *cip;
	struct buf *cbp, *bp;
	struct ifile *ifp;
	struct inode *ip;
	struct vnode *vp;
	struct lfs *fs;
	daddr_t old_iaddr;
	ino_t ino, otail;
	extern int lfs_dirvcount;
	int s;
	
	/* Get the inode number and file system. */
	vp = ap->a_pvp;
	ip = VTOI(vp);
	fs = ip->i_lfs;
	ino = ip->i_number;

	/* Drain of pending writes */
	s = splbio();
	if (fs->lfs_version > 1 && WRITEINPROG(vp))
		tsleep(vp, (PRIBIO+1), "lfs_vfree", 0);
	splx(s);

	lfs_seglock(fs, SEGM_PROT);
	
	if (vp->v_flag & VDIROP) {
		--lfs_dirvcount;
		vp->v_flag &= ~VDIROP;
		TAILQ_REMOVE(&fs->lfs_dchainhd, ip, i_lfs_dchain);
		wakeup(&lfs_dirvcount);
		lfs_vunref(vp);
	}
	lfs_unmark_vnode(vp);

	LFS_CLR_UINO(ip, IN_ACCESSED|IN_CLEANING|IN_MODIFIED);
	ip->i_flag &= ~IN_ALLMOD;

	/*
	 * Set the ifile's inode entry to unused, increment its version number
	 * and link it onto the free chain.
	 */
	LFS_IENTRY(ifp, fs, ino, bp);
	old_iaddr = ifp->if_daddr;
	ifp->if_daddr = LFS_UNUSED_DADDR;
	++ifp->if_version;
	if (fs->lfs_version == 1) {
		LFS_GET_HEADFREE(fs, cip, cbp, &(ifp->if_nextfree));
		LFS_PUT_HEADFREE(fs, cip, cbp, ino);
		(void) LFS_BWRITE_LOG(bp); /* Ifile */
	} else {
		ifp->if_nextfree = LFS_UNUSED_INUM;
		/*
		 * XXX Writing the freed node here means that it might not
		 * XXX make it into the free list in the event of a crash
		 * XXX (the ifile could be written before the rest of this
		 * XXX completes).
		 */
		(void) LFS_BWRITE_LOG(bp); /* Ifile */
		LFS_GET_TAILFREE(fs, cip, cbp, &otail);
		LFS_IENTRY(ifp, fs, otail, bp);
		ifp->if_nextfree = ino;
		LFS_BWRITE_LOG(bp);
		LFS_PUT_TAILFREE(fs, cip, cbp, ino);
		/* printf("lfs_vfree: tailfree %d -> %d\n", otail, ino); */
	}
#ifdef DIAGNOSTIC
	if (ino == LFS_UNUSED_INUM) {
		panic("inode 0 freed");
	}
#endif /* DIAGNOSTIC */
	if (old_iaddr != LFS_UNUSED_DADDR) {
		LFS_SEGENTRY(sup, fs, dtosn(fs, old_iaddr), bp);
#ifdef DIAGNOSTIC
		if (sup->su_nbytes < sizeof (struct ufs1_dinode)) {
			printf("lfs_vfree: negative byte count"
			       " (segment %" PRIu32 " short by %d)\n",
			       dtosn(fs, old_iaddr),
			       (int)sizeof (struct ufs1_dinode) -
				    sup->su_nbytes);
			panic("lfs_vfree: negative byte count");
			sup->su_nbytes = sizeof (struct ufs1_dinode);
		}
#endif
		sup->su_nbytes -= sizeof (struct ufs1_dinode);
		LFS_WRITESEGENTRY(sup, fs, dtosn(fs, old_iaddr), bp); /* Ifile */
	}
	
	/* Set superblock modified bit and decrement file count. */
	fs->lfs_fmod = 1;
	--fs->lfs_nfiles;
	
	lfs_segunlock(fs);

	return (0);
}
