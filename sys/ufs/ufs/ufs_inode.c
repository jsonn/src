/*	$NetBSD: ufs_inode.c,v 1.13.8.4 2001/03/12 13:32:07 bouyer Exp $	*/

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
 *	@(#)ufs_inode.c	8.9 (Berkeley) 5/14/95
 */

#include "opt_quota.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/namei.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <uvm/uvm.h>

extern int prtactive;

/*
 * Last reference to an inode.  If necessary, write or delete it.
 */
int
ufs_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct proc *p = ap->a_p;
	int mode, error = 0;

	if (prtactive && vp->v_usecount != 0)
		vprint("ufs_inactive: pushing active", vp);

	/*
	 * Ignore inodes related to stale file handles.
	 */
	if (ip->i_ffs_mode == 0)
		goto out;

	if (ip->i_ffs_nlink <= 0 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
#ifdef QUOTA
		if (!getinoquota(ip))
			(void)chkiq(ip, -1, NOCRED, 0);
#endif
		if (ip->i_ffs_size != 0) {
			error = VOP_TRUNCATE(vp, (off_t)0, 0, NOCRED, p);
		}
		ip->i_ffs_rdev = 0;
		mode = ip->i_ffs_mode;
		ip->i_ffs_mode = 0;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		VOP_VFREE(vp, ip->i_number, mode);
	}

	if (ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFIED | IN_ACCESSED))
		VOP_UPDATE(vp, NULL, NULL, 0);
out:
	VOP_UNLOCK(vp, 0);
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */

	if (ip->i_ffs_mode == 0)
		vrecycle(vp, NULL, p);
	return (error);
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
ufs_reclaim(vp, p)
	struct vnode *vp;
	struct proc *p;
{
	struct inode *ip;

	if (prtactive && vp->v_usecount != 0)
		vprint("ufs_reclaim: pushing active", vp);
	/*
	 * Remove the inode from its hash chain.
	 */
	ip = VTOI(vp);
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
	return (0);
}

/*
 * allocate a range of blocks in a file.
 * after this function returns, any page entirely contained within the range
 * will map to invalid data and thus must be overwritten before it is made
 * accessible to others.
 */

int
ufs_balloc_range(vp, off, len, cred, flags)
	struct vnode *vp;
	off_t off, len;
	struct ucred *cred;
	int flags;
{
	off_t oldeof, neweof, oldeob, neweob, oldpagestart, pagestart;
	struct uvm_object *uobj;
	int i, delta, error, npages1, npages2;
	int bshift = vp->v_mount->mnt_fs_bshift;
	int bsize = 1 << bshift;
	int ppb = MAX(bsize >> PAGE_SHIFT, 1);
	struct vm_page *pgs1[ppb], *pgs2[ppb];
	UVMHIST_FUNC("ufs_balloc_range"); UVMHIST_CALLED(ubchist);
	UVMHIST_LOG(ubchist, "vp %p off 0x%x len 0x%x u_size 0x%x",
		    vp, off, len, vp->v_uvm.u_size);

	oldeof = vp->v_uvm.u_size;
	error = VOP_SIZE(vp, oldeof, &oldeob);
	if (error) {
		return error;
	}

	neweof = MAX(vp->v_uvm.u_size, off + len);
	error = VOP_SIZE(vp, neweof, &neweob);
	if (error) {
		return error;
	}

	error = 0;
	uobj = &vp->v_uvm.u_obj;
	pgs1[0] = pgs2[0] = NULL;

	/*
	 * if the last block in the file is not a full block (ie. it is a
	 * fragment), and this allocation is causing the fragment to change
	 * size (either to expand the fragment or promote it to a full block),
	 * cache the old last block (at its new size).
	 */

	oldpagestart = trunc_page(oldeof) & ~(bsize - 1);
	if ((oldeob & (bsize - 1)) != 0 && oldeob != neweob) {
		npages1 = MIN(ppb, (round_page(neweob) - oldpagestart) >>
			      PAGE_SHIFT);
		memset(pgs1, 0, npages1 * sizeof(struct vm_page *));
		simple_lock(&uobj->vmobjlock);
		error = VOP_GETPAGES(vp, oldpagestart, pgs1, &npages1,
		    0, VM_PROT_READ, 0, PGO_SYNCIO|PGO_PASTEOF);
		if (error) {
			goto out;
		}
		simple_lock(&uobj->vmobjlock);
		uvm_lock_pageq();
		for (i = 0; i < npages1; i++) {
			UVMHIST_LOG(ubchist, "got pgs1[%d] %p", i, pgs1[i],0,0);
			KASSERT((pgs1[i]->flags & PG_RELEASED) == 0);
			pgs1[i]->flags &= ~PG_CLEAN;
			uvm_pageactivate(pgs1[i]);
		}
		uvm_unlock_pageq();
		simple_unlock(&uobj->vmobjlock);
	}

	/*
	 * cache the new range as well.  this will create zeroed pages
	 * where the new block will be and keep them locked until the
	 * new block is allocated, so there will be no window where
	 * the old contents of the new block is visible to racing threads.
	 */

	pagestart = trunc_page(off) & ~(bsize - 1);
	if (pagestart != oldpagestart || pgs1[0] == NULL) {
		npages2 = MIN(ppb, (round_page(neweob) - pagestart) >>
			      PAGE_SHIFT);
		memset(pgs2, 0, npages2 * sizeof(struct vm_page *));
		simple_lock(&uobj->vmobjlock);
		error = VOP_GETPAGES(vp, pagestart, pgs2, &npages2, 0,
		    VM_PROT_READ, 0, PGO_SYNCIO|PGO_PASTEOF);
		if (error) {
			goto out;
		}
		simple_lock(&uobj->vmobjlock);
		uvm_lock_pageq();
		for (i = 0; i < npages2; i++) {
			UVMHIST_LOG(ubchist, "got pgs2[%d] %p", i, pgs2[i],0,0);
			KASSERT((pgs2[i]->flags & PG_RELEASED) == 0);
			pgs2[i]->flags &= ~PG_CLEAN;
			uvm_pageactivate(pgs2[i]);
		}
		uvm_unlock_pageq();
		simple_unlock(&uobj->vmobjlock);
	}

	/*
	 * adjust off to be block-aligned.
	 */

	delta = off & (bsize - 1);
	off -= delta;
	len += delta;

	/*
	 * now allocate the range.
	 */

	lockmgr(&vp->v_glock, LK_EXCLUSIVE, NULL);
	error = VOP_BALLOCN(vp, off, len, cred, flags);
	lockmgr(&vp->v_glock, LK_RELEASE, NULL);

	/*
	 * unbusy any pages we are holding.
	 * if we got an error, free any pages we created past the old eob.
	 */

out:
	simple_lock(&uobj->vmobjlock);
	if (error) {
		(void) (uobj->pgops->pgo_flush)(uobj, round_page(oldeob), 0,
		    PGO_FREE);
	}
	if (pgs1[0] != NULL) {
		uvm_page_unbusy(pgs1, npages1);

		/*
		 * The data in the frag might be moving to a new disk location.
		 * We need to flush pages to the new disk locations.
		 */

		(uobj->pgops->pgo_flush)(uobj, oldeof & ~(bsize - 1),
		    MIN((oldeof + bsize) & ~(bsize - 1), neweof),
		    PGO_CLEANIT | ((flags & B_SYNC) ? PGO_SYNCIO : 0));
	}
	if (pgs2[0] != NULL) {
		uvm_page_unbusy(pgs2, npages2);
	}
	simple_unlock(&uobj->vmobjlock);
	return error;
}
