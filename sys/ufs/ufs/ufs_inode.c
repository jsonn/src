/*	$NetBSD: ufs_inode.c,v 1.57.12.2 2006/05/24 15:50:48 tron Exp $	*/

/*
 * Copyright (c) 1991, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)ufs_inode.c	8.9 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ufs_inode.c,v 1.57.12.2 2006/05/24 15:50:48 tron Exp $");

#if defined(_KERNEL_OPT)
#include "opt_ffs.h"
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/namei.h>
#include <sys/kauth.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#ifdef UFS_DIRHASH
#include <ufs/ufs/dirhash.h>
#endif
#ifdef UFS_EXTATTR
#include <ufs/ufs/extattr.h>
#endif

#include <uvm/uvm.h>

extern int prtactive;

/*
 * Last reference to an inode.  If necessary, write or delete it.
 */
int
ufs_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct mount *mp;
	struct lwp *l = ap->a_l;
	mode_t mode;
	int error = 0;

	if (prtactive && vp->v_usecount != 0)
		vprint("ufs_inactive: pushing active", vp);

	/*
	 * Ignore inodes related to stale file handles.
	 */
	if (ip->i_mode == 0)
		goto out;
	if (ip->i_ffs_effnlink == 0 && DOINGSOFTDEP(vp))
		softdep_releasefile(ip);

	if (ip->i_nlink <= 0 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
		vn_start_write(vp, &mp, V_WAIT | V_LOWER);
#ifdef QUOTA
		if (!getinoquota(ip))
			(void)chkiq(ip, -1, NOCRED, 0);
#endif
#ifdef UFS_EXTATTR
		ufs_extattr_vnode_inactive(vp, l);
#endif
		if (ip->i_size != 0) {
			error = UFS_TRUNCATE(vp, (off_t)0, 0, NOCRED, l);
		}
		/*
		 * Setting the mode to zero needs to wait for the inode
		 * to be written just as does a change to the link count.
		 * So, rather than creating a new entry point to do the
		 * same thing, we just use softdep_change_linkcnt().
		 */
		DIP_ASSIGN(ip, rdev, 0);
		mode = ip->i_mode;
		ip->i_mode = 0;
		DIP_ASSIGN(ip, mode, 0);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		simple_lock(&vp->v_interlock);
		vp->v_flag |= VFREEING;
		simple_unlock(&vp->v_interlock);
		if (DOINGSOFTDEP(vp))
			softdep_change_linkcnt(ip);
		UFS_VFREE(vp, ip->i_number, mode);
		vn_finished_write(mp, V_LOWER);
	}

	if (ip->i_flag & (IN_CHANGE | IN_UPDATE | IN_MODIFIED)) {
		vn_start_write(vp, &mp, V_WAIT | V_LOWER);
		UFS_UPDATE(vp, NULL, NULL, 0);
		vn_finished_write(mp, V_LOWER);
	}
out:
	VOP_UNLOCK(vp, 0);
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */

	if (ip->i_mode == 0)
		vrecycle(vp, NULL, l);
	return (error);
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
ufs_reclaim(struct vnode *vp, struct lwp *l)
{
	struct inode *ip = VTOI(vp);
	struct mount *mp;

	if (prtactive && vp->v_usecount != 0)
		vprint("ufs_reclaim: pushing active", vp);

	vn_start_write(vp, &mp, V_WAIT | V_LOWER);
	UFS_UPDATE(vp, NULL, NULL, UPDATE_CLOSE);
	vn_finished_write(mp, V_LOWER);

	/*
	 * Remove the inode from its hash chain.
	 */
	ufs_ihashrem(ip);
	/*
	 * Purge old data structures associated with the inode.
	 */
	cache_purge(vp);
	if (ip->i_devvp) {
		vrele(ip->i_devvp);
		ip->i_devvp = 0;
	}
#ifdef QUOTA
	{
		int i;
		for (i = 0; i < MAXQUOTAS; i++) {
			if (ip->i_dquot[i] != NODQUOT) {
				dqrele(vp, ip->i_dquot[i]);
				ip->i_dquot[i] = NODQUOT;
			}
		}
	}
#endif
#ifdef UFS_DIRHASH
	if (ip->i_dirhash != NULL)
		ufsdirhash_free(ip);
#endif
	return (0);
}

/*
 * allocate a range of blocks in a file.
 * after this function returns, any page entirely contained within the range
 * will map to invalid data and thus must be overwritten before it is made
 * accessible to others.
 */

int
ufs_balloc_range(struct vnode *vp, off_t off, off_t len, kauth_cred_t cred,
    int flags)
{
	off_t neweof;	/* file size after the operation */
	off_t neweob;	/* offset next to the last block after the operation */
	off_t pagestart; /* starting offset of range covered by pgs */
	off_t eob;	/* offset next to allocated blocks */
	struct uvm_object *uobj;
	struct genfs_node *gp = VTOG(vp);
	int i, delta, error, npages;
	int bshift = vp->v_mount->mnt_fs_bshift;
	int bsize = 1 << bshift;
	int ppb = MAX(bsize >> PAGE_SHIFT, 1);
	struct vm_page *pgs[ppb];
	UVMHIST_FUNC("ufs_balloc_range"); UVMHIST_CALLED(ubchist);
	UVMHIST_LOG(ubchist, "vp %p off 0x%x len 0x%x u_size 0x%x",
		    vp, off, len, vp->v_size);

	neweof = MAX(vp->v_size, off + len);
	GOP_SIZE(vp, neweof, &neweob, 0);

	error = 0;
	uobj = &vp->v_uobj;
	pgs[0] = NULL;

	/*
	 * read or create pages covering the range of the allocation and
	 * keep them locked until the new block is allocated, so there
	 * will be no window where the old contents of the new block are
	 * visible to racing threads.
	 */

	pagestart = trunc_page(off) & ~(bsize - 1);
	npages = MIN(ppb, (round_page(neweob) - pagestart) >> PAGE_SHIFT);
	memset(pgs, 0, npages * sizeof(struct vm_page *));
	simple_lock(&uobj->vmobjlock);
	error = VOP_GETPAGES(vp, pagestart, pgs, &npages, 0,
	    VM_PROT_WRITE, 0,
	    PGO_SYNCIO|PGO_PASTEOF|PGO_NOBLOCKALLOC|PGO_NOTIMESTAMP);
	if (error) {
		return error;
	}
	simple_lock(&uobj->vmobjlock);
	uvm_lock_pageq();
	for (i = 0; i < npages; i++) {
		UVMHIST_LOG(ubchist, "got pgs[%d] %p", i, pgs[i],0,0);
		KASSERT((pgs[i]->flags & PG_RELEASED) == 0);
		pgs[i]->flags &= ~PG_CLEAN;
		uvm_pageactivate(pgs[i]);
	}
	uvm_unlock_pageq();
	simple_unlock(&uobj->vmobjlock);

	/*
	 * adjust off to be block-aligned.
	 */

	delta = off & (bsize - 1);
	off -= delta;
	len += delta;

	/*
	 * now allocate the range.
	 */

	lockmgr(&gp->g_glock, LK_EXCLUSIVE, NULL);
	error = GOP_ALLOC(vp, off, len, flags, cred);
	lockmgr(&gp->g_glock, LK_RELEASE, NULL);

	/*
	 * clear PG_RDONLY on any pages we are holding
	 * (since they now have backing store) and unbusy them.
	 */

	GOP_SIZE(vp, off + len, &eob, 0);
	simple_lock(&uobj->vmobjlock);
	for (i = 0; i < npages; i++) {
		if (error) {
			pgs[i]->flags |= PG_RELEASED;
		} else if (off <= pagestart + (i << PAGE_SHIFT) &&
		    pagestart + ((i + 1) << PAGE_SHIFT) <= eob) {
			pgs[i]->flags &= ~PG_RDONLY;
		}
	}
	if (error) {
		uvm_lock_pageq();
		uvm_page_unbusy(pgs, npages);
		uvm_unlock_pageq();
	} else {
		uvm_page_unbusy(pgs, npages);
	}
	simple_unlock(&uobj->vmobjlock);
	return error;
}
