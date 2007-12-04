/*	$NetBSD: vfs_subr.c,v 1.308.2.1 2007/12/04 13:03:21 ad Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 2004, 2005, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, by Charles M. Hannum, and by Andrew Doran.
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
 * Copyright (c) 1989, 1993
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
 *	@(#)vfs_subr.c	8.13 (Berkeley) 4/18/94
 */

/*
 * External virtual filesystem routines.
 *
 * This file contains vfs subroutines which are heavily dependant on
 * the kernel and are not suitable for standalone use.  Examples include
 * routines involved vnode and mountpoint management.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_subr.c,v 1.308.2.1 2007/12/04 13:03:21 ad Exp $");

#include "opt_inet.h"
#include "opt_ddb.h"
#include "opt_compat_netbsd.h"
#include "opt_compat_43.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/namei.h>
#include <sys/ucred.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/syscallargs.h>
#include <sys/device.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/atomic.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/syncfs/syncfs.h>

#include <uvm/uvm.h>
#include <uvm/uvm_readahead.h>
#include <uvm/uvm_ddb.h>

#include <sys/sysctl.h>

extern int dovfsusermount;	/* 1 => permit any user to mount filesystems */
extern int vfs_magiclinks;	/* 1 => expand "magic" symlinks */

static vnodelst_t vnode_free_list = TAILQ_HEAD_INITIALIZER(vnode_free_list);
static vnodelst_t vnode_hold_list = TAILQ_HEAD_INITIALIZER(vnode_hold_list);

pool_cache_t vnode_cache;

MALLOC_DEFINE(M_VNODE, "vnodes", "Dynamically allocated vnodes");

/*
 * Local declarations.
 */

static void insmntque(vnode_t *, struct mount *);
static int getdevvp(dev_t, vnode_t **, enum vtype);
static vnode_t *getcleanvnode(void);;
void vpanic(vnode_t *, const char *);

#ifdef DIAGNOSTIC
void
vpanic(vnode_t *vp, const char *msg)
{

	vprint(NULL, vp);
	panic("%s\n", msg);
}
#else
#define	vpanic(vp, msg)	/* nothing */
#endif

int
vfs_drainvnodes(long target, struct lwp *l)
{

	while (numvnodes > target) {
		vnode_t *vp;

		mutex_enter(&vnode_free_list_lock);
		vp = getcleanvnode();
		if (vp == NULL)
			return EBUSY; /* give up */
		ungetnewvnode(vp);
	}

	return 0;
}

/*
 * grab a vnode from freelist and clean it.
 */
vnode_t *
getcleanvnode(void)
{
	vnode_t *vp;
	vnodelst_t *listhd;

	KASSERT(mutex_owned(&vnode_free_list_lock));

retry:
	listhd = &vnode_free_list;
try_nextlist:
	TAILQ_FOREACH(vp, listhd, v_freelist) {
		/*
		 * It's safe to test v_usecount and v_iflag
		 * without holding the interlock here, since
		 * these vnodes should never appear on the
		 * lists.
		 */
		if (vp->v_usecount != 0) {
			vpanic(vp, "free vnode isn't");
		}
		if ((vp->v_iflag & VI_CLEAN) != 0) {
			vpanic(vp, "clean vnode on freelist");
		}
		if (vp->v_freelisthd != listhd) {
			printf("vnode sez %p, listhd %p\n", vp->v_freelisthd, listhd);
			vpanic(vp, "list head mismatch");
		}
		if (!mutex_tryenter(&vp->v_interlock))
			continue;
		/*
		 * Our lwp might hold the underlying vnode
		 * locked, so don't try to reclaim a VLAYER
		 * node if it's locked.
		 */
		if ((vp->v_iflag & VI_XLOCK) == 0 &&
		    ((vp->v_iflag & VI_LAYER) == 0 || VOP_ISLOCKED(vp) == 0)) {
			break;
		}
		mutex_exit(&vp->v_interlock);
	}

	if (vp == NULL) {
		if (listhd == &vnode_free_list) {
			listhd = &vnode_hold_list;
			goto try_nextlist;
		}
		mutex_exit(&vnode_free_list_lock);
		return NULL;
	}

	/* Remove it from the freelist. */
	TAILQ_REMOVE(listhd, vp, v_freelist);
	vp->v_freelisthd = NULL;
	mutex_exit(&vnode_free_list_lock);

	/*
	 * The vnode is still associated with a file system, so we must
	 * clean it out before reusing it.  We need to add a reference
	 * before doing this.  If the vnode gains another reference while
	 * being cleaned out then we lose - retry.
	 */
	vp->v_usecount++;
	vclean(vp, DOCLOSE);
	if (vp->v_usecount == 1) {
		/* We're about to dirty it. */
		vp->v_iflag &= ~VI_CLEAN;
		mutex_exit(&vp->v_interlock);
	} else {
		/*
		 * Don't return to freelist - the holder of the last
		 * reference will destroy it.
		 */
		vp->v_usecount--;
		mutex_exit(&vp->v_interlock);
		mutex_enter(&vnode_free_list_lock);
		goto retry;
	}

	if (vp->v_data != NULL || vp->v_uobj.uo_npages != 0 ||
	    !TAILQ_EMPTY(&vp->v_uobj.memq)) {
		vpanic(vp, "cleaned vnode isn't");
	}
	if (vp->v_numoutput != 0) {
		vpanic(vp, "clean vnode has pending I/O's");
	}
	if ((vp->v_iflag & VI_ONWORKLST) != 0) {
		vpanic(vp, "clean vnode on syncer list");
	}

	return vp;
}

/*
 * Mark a mount point as busy. Used to synchronize access and to delay
 * unmounting. Interlock is not released on failure.
 */
int
vfs_busy(struct mount *mp, int flags, kmutex_t *interlkp)
{
	int lkflags;

	while (mp->mnt_iflag & IMNT_UNMOUNT) {
		int gone, n;

		if (flags & LK_NOWAIT)
			return (ENOENT);
		if ((flags & LK_RECURSEFAIL) && mp->mnt_unmounter != NULL
		    && mp->mnt_unmounter == curlwp)
			return (EDEADLK);
		if (interlkp)
			mutex_exit(interlkp);
		/*
		 * Since all busy locks are shared except the exclusive
		 * lock granted when unmounting, the only place that a
		 * wakeup needs to be done is at the release of the
		 * exclusive lock at the end of dounmount.
		 */
		mutex_enter(&mp->mnt_mutex);
		mp->mnt_wcnt++;
		mtsleep((void *)mp, PVFS, "vfs_busy", 0, &mp->mnt_mutex);
		n = --mp->mnt_wcnt;
		mutex_exit(&mp->mnt_mutex);
		gone = mp->mnt_iflag & IMNT_GONE;

		if (n == 0)
			wakeup(&mp->mnt_wcnt);
		if (interlkp)
			mutex_enter(interlkp);
		if (gone)
			return (ENOENT);
	}
	lkflags = LK_SHARED;
	if (interlkp)
		lkflags |= LK_INTERLOCK;
	if (lockmgr(&mp->mnt_lock, lkflags, interlkp))
		panic("vfs_busy: unexpected lock failure");
	return (0);
}

/*
 * Free a busy filesystem.
 */
void
vfs_unbusy(struct mount *mp)
{

	lockmgr(&mp->mnt_lock, LK_RELEASE, NULL);
}

/*
 * Lookup a filesystem type, and if found allocate and initialize
 * a mount structure for it.
 *
 * Devname is usually updated by mount(8) after booting.
 */
int
vfs_rootmountalloc(const char *fstypename, const char *devname,
    struct mount **mpp)
{
	struct vfsops *vfsp = NULL;
	struct mount *mp;

	mutex_enter(&vfs_list_lock);
	LIST_FOREACH(vfsp, &vfs_list, vfs_list)
		if (!strncmp(vfsp->vfs_name, fstypename, 
		    sizeof(mp->mnt_stat.f_fstypename)))
			break;
	if (vfsp == NULL)
		return (ENODEV);
	vfsp->vfs_refcount++;
	mutex_exit(&vfs_list_lock);

	mp = malloc((u_long)sizeof(struct mount), M_MOUNT, M_WAITOK);
	memset((char *)mp, 0, (u_long)sizeof(struct mount));
	lockinit(&mp->mnt_lock, PVFS, "vfslock", 0, 0);
	mutex_init(&mp->mnt_mutex, MUTEX_DEFAULT, IPL_NONE);
	(void)vfs_busy(mp, LK_NOWAIT, 0);
	TAILQ_INIT(&mp->mnt_vnodelist);
	mp->mnt_op = vfsp;
	mp->mnt_flag = MNT_RDONLY;
	mp->mnt_vnodecovered = NULL;
	(void)strlcpy(mp->mnt_stat.f_fstypename, vfsp->vfs_name,
	    sizeof(mp->mnt_stat.f_fstypename));
	mp->mnt_stat.f_mntonname[0] = '/';
	mp->mnt_stat.f_mntonname[0] = '\0';
	mp->mnt_stat.f_mntfromname[sizeof(mp->mnt_stat.f_mntfromname) - 1] =
	    '\0';
	(void)copystr(devname, mp->mnt_stat.f_mntfromname,
	    sizeof(mp->mnt_stat.f_mntfromname) - 1, 0);
	mount_initspecific(mp);
	*mpp = mp;
	return (0);
}

/*
 * Routines having to do with the management of the vnode table.
 */
extern int (**dead_vnodeop_p)(void *);

/*
 * Return the next vnode from the free list.
 */
int
getnewvnode(enum vtagtype tag, struct mount *mp, int (**vops)(void *),
	    vnode_t **vpp)
{
	struct uvm_object *uobj;
	static int toggle;
	vnode_t *vp;
	int error = 0, tryalloc;

 try_again:
	if (mp) {
		/*
		 * Mark filesystem busy while we're creating a vnode.
		 * If unmount is in progress, this will wait; if the
		 * unmount succeeds (only if umount -f), this will
		 * return an error.  If the unmount fails, we'll keep
		 * going afterwards.
		 * (This puts the per-mount vnode list logically under
		 * the protection of the vfs_busy lock).
		 */
		error = vfs_busy(mp, LK_RECURSEFAIL, 0);
		if (error && error != EDEADLK)
			return error;
	}

	/*
	 * We must choose whether to allocate a new vnode or recycle an
	 * existing one. The criterion for allocating a new one is that
	 * the total number of vnodes is less than the number desired or
	 * there are no vnodes on either free list. Generally we only
	 * want to recycle vnodes that have no buffers associated with
	 * them, so we look first on the vnode_free_list. If it is empty,
	 * we next consider vnodes with referencing buffers on the
	 * vnode_hold_list. The toggle ensures that half the time we
	 * will use a buffer from the vnode_hold_list, and half the time
	 * we will allocate a new one unless the list has grown to twice
	 * the desired size. We are reticent to recycle vnodes from the
	 * vnode_hold_list because we will lose the identity of all its
	 * referencing buffers.
	 */

	vp = NULL;

	mutex_enter(&vnode_free_list_lock);

	toggle ^= 1;
	if (numvnodes > 2 * desiredvnodes)
		toggle = 0;

	tryalloc = numvnodes < desiredvnodes ||
	    (TAILQ_FIRST(&vnode_free_list) == NULL &&
	     (TAILQ_FIRST(&vnode_hold_list) == NULL || toggle));

	if (tryalloc) {
		numvnodes++;
		mutex_exit(&vnode_free_list_lock);
		if ((vp = valloc(NULL)) == NULL) {
			mutex_enter(&vnode_free_list_lock);
			numvnodes--;
		} else
			vp->v_usecount = 1;
	}

	if (vp == NULL) {
		vp = getcleanvnode();
		if (vp == NULL) {
			if (mp && error != EDEADLK)
				vfs_unbusy(mp);
			if (tryalloc) {
				printf("WARNING: unable to allocate new "
				    "vnode, retrying...\n");
				(void) tsleep(&lbolt, PRIBIO, "newvn", hz);
				goto try_again;
			}
			tablefull("vnode", "increase kern.maxvnodes or NVNODE");
			*vpp = 0;
			return (ENFILE);
		}
		vp->v_iflag = 0;
		vp->v_vflag = 0;
		vp->v_uflag = 0;
		vp->v_socket = NULL;
	}

	KASSERT(vp->v_usecount == 1);
	KASSERT(vp->v_freelisthd == NULL);
	KASSERT(LIST_EMPTY(&vp->v_nclist));
	KASSERT(LIST_EMPTY(&vp->v_dnclist));

	vp->v_type = VNON;
	vp->v_vnlock = &vp->v_lock;
	lockinit(vp->v_vnlock, PVFS, "vnlock", 0, 0);
	vp->v_tag = tag;
	vp->v_op = vops;
	insmntque(vp, mp);
	*vpp = vp;
	vp->v_data = 0;

	/*
	 * initialize uvm_object within vnode.
	 */

	uobj = &vp->v_uobj;
	KASSERT(uobj->pgops == &uvm_vnodeops);
	KASSERT(uobj->uo_npages == 0);
	KASSERT(TAILQ_FIRST(&uobj->memq) == NULL);
	vp->v_size = vp->v_writesize = VSIZENOTSET;

	if (mp != NULL) {
		if ((mp->mnt_iflag & IMNT_MPSAFE) != 0)
			vp->v_vflag |= VV_MPSAFE;
		if (error != EDEADLK)
			vfs_unbusy(mp);
	}

	return (0);
}

/*
 * This is really just the reverse of getnewvnode(). Needed for
 * VFS_VGET functions who may need to push back a vnode in case
 * of a locking race.
 */
void
ungetnewvnode(vnode_t *vp)
{

	KASSERT(vp->v_usecount == 1);
	KASSERT(vp->v_data == NULL);
	KASSERT(vp->v_freelisthd == NULL);

	mutex_enter(&vp->v_interlock);
	vp->v_iflag |= VI_CLEAN;
	vrelel(vp, 0, 0);
}

/*
 * Allocate a new, uninitialized vnode.  If 'mp' is non-NULL, this is a
 * marker vnode and we are prepared to wait for the allocation.
 */
vnode_t *
valloc(struct mount *mp)
{
	vnode_t *vp;

	vp = pool_cache_get(vnode_cache, (mp != NULL ? PR_WAITOK : PR_NOWAIT));
	if (vp == NULL) {
		return NULL;
	}

	memset(vp, 0, sizeof(*vp));
	UVM_OBJ_INIT(&vp->v_uobj, &uvm_vnodeops, 0);
	cv_init(&vp->v_cv, "vnode");
	/*
	 * done by memset() above.
	 *	LIST_INIT(&vp->v_nclist);
	 *	LIST_INIT(&vp->v_dnclist);
	 */

	if (mp != NULL) {
		vp->v_mount = mp;
		vp->v_type = VBAD;
		vp->v_iflag = VI_MARKER;
	}

	return vp;
}

/*
 * Free an unused, unreferenced vnode.
 */
void
vfree(vnode_t *vp)
{

	KASSERT(vp->v_usecount == 0);

	if ((vp->v_iflag & VI_MARKER) == 0) {
		lockdestroy(&vp->v_lock);
		mutex_enter(&vnode_free_list_lock);
		numvnodes--;
		mutex_exit(&vnode_free_list_lock);
	}

	UVM_OBJ_DESTROY(&vp->v_uobj);
	cv_destroy(&vp->v_cv);
	pool_cache_put(vnode_cache, vp);
}

/*
 * Insert a marker vnode into a mount's vnode list, after the
 * specified vnode.  mntvnode_lock must be held.
 */
void
vmark(vnode_t *mvp, vnode_t *vp)
{
	struct mount *mp;

	mp = mvp->v_mount;

	KASSERT(mutex_owned(&mntvnode_lock));
	KASSERT((mvp->v_iflag & VI_MARKER) != 0);
	KASSERT(vp->v_mount == mp);

	TAILQ_INSERT_AFTER(&mp->mnt_vnodelist, vp, mvp, v_mntvnodes);
}

/*
 * Remove a marker vnode from a mount's vnode list, and return
 * a pointer to the next vnode in the list.  mntvnode_lock must
 * be held.
 */
vnode_t *
vunmark(vnode_t *mvp)
{
	vnode_t *vp;
	struct mount *mp;

	mp = mvp->v_mount;

	KASSERT(mutex_owned(&mntvnode_lock));
	KASSERT((mvp->v_iflag & VI_MARKER) != 0);

	vp = TAILQ_NEXT(mvp, v_mntvnodes);
	TAILQ_REMOVE(&mp->mnt_vnodelist, mvp, v_mntvnodes); 

	KASSERT(vp == NULL || vp->v_mount == mp);

	return vp;
}

/*
 * Remove a vnode from its freelist.
 */
static inline void
vremfree(vnode_t *vp)
{

	KASSERT(mutex_owned(&vp->v_interlock));
	KASSERT(vp->v_usecount == 0);

	/*
	 * Note that the reference count must not change until
	 * the vnode is removed.
	 */
	mutex_enter(&vnode_free_list_lock);
	if (vp->v_holdcnt > 0) {
		KASSERT(vp->v_freelisthd == &vnode_hold_list);
	} else {
		KASSERT(vp->v_freelisthd == &vnode_free_list);
	}
	TAILQ_REMOVE(vp->v_freelisthd, vp, v_freelist);
	vp->v_freelisthd = NULL;
	mutex_exit(&vnode_free_list_lock);
}

/*
 * Move a vnode from one mount queue to another.
 */
static void
insmntque(vnode_t *vp, struct mount *mp)
{

#ifdef DIAGNOSTIC
	if ((mp != NULL) &&
	    (mp->mnt_iflag & IMNT_UNMOUNT) &&
	    !(mp->mnt_flag & MNT_SOFTDEP) &&
	    vp->v_tag != VT_VFS) {
		panic("insmntque into dying filesystem");
	}
#endif

	mutex_enter(&mntvnode_lock);
	/*
	 * Delete from old mount point vnode list, if on one.
	 */
	if (vp->v_mount != NULL)
		TAILQ_REMOVE(&vp->v_mount->mnt_vnodelist, vp, v_mntvnodes);
	/*
	 * Insert into list of vnodes for the new mount point, if available.
	 */
	if ((vp->v_mount = mp) != NULL)
		TAILQ_INSERT_TAIL(&mp->mnt_vnodelist, vp, v_mntvnodes);
	mutex_exit(&mntvnode_lock);
}

/*
 * Create a vnode for a block device.
 * Used for root filesystem and swap areas.
 * Also used for memory file system special devices.
 */
int
bdevvp(dev_t dev, vnode_t **vpp)
{

	return (getdevvp(dev, vpp, VBLK));
}

/*
 * Create a vnode for a character device.
 * Used for kernfs and some console handling.
 */
int
cdevvp(dev_t dev, vnode_t **vpp)
{

	return (getdevvp(dev, vpp, VCHR));
}

/*
 * Create a vnode for a device.
 * Used by bdevvp (block device) for root file system etc.,
 * and by cdevvp (character device) for console and kernfs.
 */
static int
getdevvp(dev_t dev, vnode_t **vpp, enum vtype type)
{
	vnode_t *vp;
	vnode_t *nvp;
	int error;

	if (dev == NODEV) {
		*vpp = NULL;
		return (0);
	}
	error = getnewvnode(VT_NON, NULL, spec_vnodeop_p, &nvp);
	if (error) {
		*vpp = NULL;
		return (error);
	}
	vp = nvp;
	vp->v_type = type;
	uvm_vnp_setsize(vp, 0);
	if ((nvp = checkalias(vp, dev, NULL)) != 0) {
		vput(vp);
		vp = nvp;
	}
	*vpp = vp;
	return (0);
}

/*
 * Check to see if the new vnode represents a special device
 * for which we already have a vnode (either because of
 * bdevvp() or because of a different vnode representing
 * the same block device). If such an alias exists, deallocate
 * the existing contents and return the aliased vnode. The
 * caller is responsible for filling it with its new contents.
 */
vnode_t *
checkalias(vnode_t *nvp, dev_t nvp_rdev, struct mount *mp)
{
	vnode_t *vp;
	vnode_t **vpp;

	if (nvp->v_type != VBLK && nvp->v_type != VCHR)
		return (NULL);

	vpp = &speclisth[SPECHASH(nvp_rdev)];
loop:
	mutex_enter(&spechash_lock);
	for (vp = *vpp; vp; vp = vp->v_specnext) {
		if (nvp_rdev != vp->v_rdev || nvp->v_type != vp->v_type)
			continue;
		/*
		 * Alias, but not in use, so flush it out.
		 */
		mutex_enter(&vp->v_interlock);
		mutex_exit(&spechash_lock);
		if (vp->v_usecount == 0) {
			vremfree(vp);
			vp->v_usecount++;
			vclean(vp, DOCLOSE);
			vrelel(vp, 1, 1);
			goto loop;
		}
		/*
		 * What we're interested to know here is if someone else has
		 * removed this vnode from the device hash list while we were
		 * waiting.  This can only happen if vclean() did it, and
		 * this requires the vnode to be locked.
		 */
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK))
			goto loop;
		if (vp->v_specinfo == NULL) {
			vput(vp);
			goto loop;
		}
		mutex_enter(&spechash_lock);
		break;
	}
	if (vp == NULL || vp->v_tag != VT_NON || vp->v_type != VBLK) {
		MALLOC(nvp->v_specinfo, struct specinfo *,
			sizeof(struct specinfo), M_VNODE, M_NOWAIT);
		/* XXX Erg. */
		if (nvp->v_specinfo == NULL) {
			mutex_exit(&spechash_lock);
			uvm_wait("checkalias");
			goto loop;
		}

		nvp->v_rdev = nvp_rdev;
		nvp->v_hashchain = vpp;
		nvp->v_specnext = *vpp;
		nvp->v_specmountpoint = NULL;
		mutex_exit(&spechash_lock);
		nvp->v_speclockf = NULL;

		*vpp = nvp;
		if (vp != NULL) {
			/* XXX locking */
			nvp->v_iflag |= VI_ALIASED;
			vp->v_iflag |= VI_ALIASED;
			vput(vp);
		}
		return (NULL);
	}
	mutex_exit(&spechash_lock);
	VOP_UNLOCK(vp, 0);
	mutex_enter(&vp->v_interlock);
	vclean(vp, 0);
	mutex_exit(&vp->v_interlock);
	vp->v_op = nvp->v_op;
	vp->v_tag = nvp->v_tag;
	vp->v_vnlock = &vp->v_lock;
	lockdestroy(vp->v_vnlock);
	lockinit(vp->v_vnlock, PVFS, "vnlock", 0, 0);
	nvp->v_type = VNON;
	insmntque(vp, mp);
	return (vp);
}

/*
 * Wait for a vnode (typically with VI_XLOCK set) to be cleaned or
 * recycled.
 */
void
vwait(vnode_t *vp, int flags)
{

	KASSERT(mutex_owned(&vp->v_interlock));
	KASSERT(vp->v_usecount != 0);

	while ((vp->v_iflag & flags) != 0)
		cv_wait(&vp->v_cv, &vp->v_interlock);
}

/*
 * Grab a particular vnode from the free list, increment its
 * reference count and lock it. If the vnode lock bit is set the
 * vnode is being eliminated in vgone. In that case, we can not
 * grab the vnode, so the process is awakened when the transition is
 * completed, and an error returned to indicate that the vnode is no
 * longer usable (possibly having been changed to a new file system type).
 */
int
vget(vnode_t *vp, int flags)
{
	int error;

	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	if ((flags & LK_INTERLOCK) == 0)
		mutex_enter(&vp->v_interlock);

	/*
	 * Before adding a reference, we must remove the vnode
	 * from its freelist.
	 */
	if (vp->v_usecount == 0) {
		vremfree(vp);
	}
	if (++vp->v_usecount == 0) {
		vpanic(vp, "vget: usecount overflow");
	}

	/*
	 * If the vnode is in the process of being cleaned out for
	 * another use, we wait for the cleaning to finish and then
	 * return failure. Cleaning is determined by checking that
	 * the VI_XLOCK flag is set.
	 */
	if ((vp->v_iflag & VI_XLOCK) != 0) {
		if (flags & LK_NOWAIT) {
			mutex_exit(&vp->v_interlock);
			return EBUSY;
		}
		vwait(vp, VI_XLOCK);
		vrelel(vp, 1, 0);
		return (ENOENT);
	}
	if (flags & LK_TYPE_MASK) {
		if ((error = vn_lock(vp, flags | LK_INTERLOCK))) {
			vrele(vp);
		}
		return (error);
	}
	mutex_exit(&vp->v_interlock);
	return (0);
}

/*
 * vput(), just unlock and vrele()
 */
void
vput(vnode_t *vp)
{

	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	VOP_UNLOCK(vp, 0);
	vrele(vp);
}

/*
 * Vnode release.  If reference count drops to zero, call inactive
 * routine and either return to freelist or free to the pool.
 */
void
vrelel(vnode_t *vp, int doinactive, int onhead)
{
	bool recycle;

	KASSERT(mutex_owned(&vp->v_interlock));
	KASSERT((vp->v_iflag & VI_MARKER) == 0);
	KASSERT(vp->v_freelisthd == NULL);

	if (vp->v_op == dead_vnodeop_p && (vp->v_iflag & VI_CLEAN) == 0) {
		vpanic(vp, "dead but not clean");
	}

	/*
	 * If not the last reference, just drop the reference count
	 * and unlock.
	 */
	if (vp->v_usecount > 1) {
		vp->v_usecount--;
		mutex_exit(&vp->v_interlock);
		return;
	}
	if (vp->v_usecount <= 0 || vp->v_writecount != 0) {
		vpanic(vp, "vput: bad ref count");
	}

	/*
	 * If not clean, deactivate the vnode, but preserve our reference
	 * across the call to VOP_INACTIVE() to prevent another thread from
	 * trying to do the same.
	 */
	recycle = false;
	if ((vp->v_iflag & VI_CLEAN) == 0) {
		if (vn_lock(vp, LK_EXCLUSIVE | LK_INTERLOCK) == 0) {
			VOP_INACTIVE(vp, &recycle);
		}
		mutex_enter(&vp->v_interlock);
		if (vp->v_usecount > 1) {
			/*
			 * Gained another reference while being
			 * deactivated.
			 */
			vp->v_usecount--;
			mutex_exit(&vp->v_interlock);
			return;
		}
	}

	/*
	 * Recycle the vnode if the file is now unused (unlinked),
	 * otherwise just free it.
	 *
	 * XXXAD may need to re-inactivate due to race w/another
	 * thread gaining and dropping a reference above.
	 */
	if (vp->v_iflag & VI_EXECMAP) {
		atomic_add_int(&uvmexp.execpages, -vp->v_uobj.uo_npages);
		atomic_add_int(&uvmexp.filepages, vp->v_uobj.uo_npages);
	}
	vp->v_iflag &= ~(VI_TEXT|VI_EXECMAP|VI_WRMAP|VI_MAPPED);
	vp->v_vflag &= ~VV_MAPPED;
	if (recycle) {
		vclean(vp, DOCLOSE);
	}

	KASSERT(vp->v_usecount > 0);
	KASSERT(vp->v_freelisthd == NULL);

	if (vp->v_op == dead_vnodeop_p && (vp->v_iflag & VI_CLEAN) == 0) {
		vpanic(vp, "dead but not clean");
	}

	if (--vp->v_usecount != 0) {
		/* Gained another reference while being reclaimed. */
		mutex_exit(&vp->v_interlock);
		return;
	}

	if ((vp->v_iflag & VI_CLEAN) != 0) {
		/*
		 * It's clean so destroy it.  It isn't referenced
		 * anywhere since it has been reclaimed.
		 */
		KASSERT(vp->v_holdcnt == 0);
		KASSERT(vp->v_writecount == 0);
		mutex_exit(&vp->v_interlock);
		insmntque(vp, NULL);
		vfree(vp);
	} else {
		/*
		 * Otherwise, put it back onto the freelist.  It
		 * can't be destroyed while still associated with
		 * a file system.
		 */
		mutex_enter(&vnode_free_list_lock);
		if (vp->v_holdcnt > 0) {
			vp->v_freelisthd = &vnode_hold_list;
		} else {
			vp->v_freelisthd = &vnode_free_list;
		}
		TAILQ_INSERT_TAIL(vp->v_freelisthd, vp, v_freelist);
		mutex_exit(&vnode_free_list_lock);
		mutex_exit(&vp->v_interlock);
	}
}

void
vrele(vnode_t *vp)
{

	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	mutex_enter(&vp->v_interlock);
	vrelel(vp, 1, 0);
}

/*
 * Page or buffer structure gets a reference.
 * Called with v_interlock held.
 */
void
vholdl(vnode_t *vp)
{

	KASSERT(mutex_owned(&vp->v_interlock));
	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	if (vp->v_holdcnt++ == 0 && vp->v_usecount == 0) {
		mutex_enter(&vnode_free_list_lock);
		KASSERT(vp->v_freelisthd == &vnode_free_list);
		TAILQ_REMOVE(vp->v_freelisthd, vp, v_freelist);
		vp->v_freelisthd = &vnode_hold_list;
		TAILQ_INSERT_TAIL(vp->v_freelisthd, vp, v_freelist);
		mutex_exit(&vnode_free_list_lock);
	}
}

/*
 * Page or buffer structure frees a reference.
 * Called with v_interlock held.
 */
void
holdrelel(vnode_t *vp)
{

	KASSERT(mutex_owned(&vp->v_interlock));
	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	if (vp->v_holdcnt <= 0) {
		vpanic(vp, "holdrelel: holdcnt vp %p");
	}

	vp->v_holdcnt--;
	if (vp->v_holdcnt == 0 && vp->v_usecount == 0) {
		mutex_enter(&vnode_free_list_lock);
		KASSERT(vp->v_freelisthd == &vnode_hold_list);
		TAILQ_REMOVE(vp->v_freelisthd, vp, v_freelist);
		vp->v_freelisthd = &vnode_free_list;
		TAILQ_INSERT_TAIL(vp->v_freelisthd, vp, v_freelist);
		mutex_exit(&vnode_free_list_lock);
	}
}

/*
 * Vnode reference, where a reference is already held by some other
 * object (for example, a file structure).
 */
void
vref(vnode_t *vp)
{

	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	mutex_enter(&vp->v_interlock);
	if (vp->v_usecount <= 0) {
		vpanic(vp, "vref used where vget required");
	}
	if (++vp->v_usecount == 0) {
		vpanic(vp, "vref: usecount overflow");
	}
	mutex_exit(&vp->v_interlock);

	KASSERT(vp->v_freelisthd == NULL);
}

/*
 * Remove any vnodes in the vnode table belonging to mount point mp.
 *
 * If FORCECLOSE is not specified, there should not be any active ones,
 * return error if any are found (nb: this is a user error, not a
 * system error). If FORCECLOSE is specified, detach any active vnodes
 * that are found.
 *
 * If WRITECLOSE is set, only flush out regular file vnodes open for
 * writing.
 *
 * SKIPSYSTEM causes any vnodes marked V_SYSTEM to be skipped.
 */
#ifdef DEBUG
int busyprt = 0;	/* print out busy vnodes */
struct ctldebug debug1 = { "busyprt", &busyprt };
#endif

int
vflush(struct mount *mp, vnode_t *skipvp, int flags)
{
	vnode_t *vp, *mvp;
	int busy = 0;

	/* Allocate a marker vnode. */
	if ((mvp = valloc(mp)) == NULL)
		return (ENOMEM);

	mutex_enter(&mntvnode_lock);
	/*
	 * NOTE: not using the TAILQ_FOREACH here since in this loop vgone()
	 * and vclean() are called
	 */
	for (vp = TAILQ_FIRST(&mp->mnt_vnodelist); vp; vp = vunmark(mvp)) {
		vmark(mvp, vp);
		if (vp->v_mount != mp || vismarker(vp))
			continue;
		/*
		 * Skip over a selected vnode.
		 */
		if (vp == skipvp)
			continue;
		mutex_enter(&vp->v_interlock);
		/*
		 * Skip over a vnodes marked VSYSTEM.
		 */
		if ((flags & SKIPSYSTEM) && (vp->v_vflag & VV_SYSTEM)) {
			mutex_exit(&vp->v_interlock);
			continue;
		}
		/*
		 * If WRITECLOSE is set, only flush out regular file
		 * vnodes open for writing.
		 */
		if ((flags & WRITECLOSE) &&
		    (vp->v_writecount == 0 || vp->v_type != VREG)) {
			mutex_exit(&vp->v_interlock);
			continue;
		}
		/*
		 * With v_usecount == 0, all we need to do is clear
		 * out the vnode data structures and we are done.
		 */
		if (vp->v_usecount == 0) {
			mutex_exit(&mntvnode_lock);
			vremfree(vp);
			vp->v_usecount++;
			vclean(vp, DOCLOSE);
			vrelel(vp, 1, 0);
			mutex_enter(&mntvnode_lock);
			continue;
		}
		KASSERT(vp->v_freelisthd == NULL);
		/*
		 * If FORCECLOSE is set, forcibly close the vnode.
		 * For block or character devices, revert to an
		 * anonymous device. For all other files, just kill them.
		 * XXXAD what?
		 */
		if (flags & FORCECLOSE) {
			mutex_exit(&mntvnode_lock);
			vp->v_usecount++;
			if (vp->v_type != VBLK && vp->v_type != VCHR) {
				vclean(vp, DOCLOSE);
			} else {
				vclean(vp, 0);
				vp->v_op = spec_vnodeop_p;
			}
			vrelel(vp, 1, 0);
			mutex_enter(&mntvnode_lock);
			continue;
		}
#ifdef DEBUG
		if (busyprt)
			vprint("vflush: busy vnode", vp);
#endif
		mutex_exit(&vp->v_interlock);
		busy++;
	}
	mutex_exit(&mntvnode_lock);
	vfree(mvp);
	if (busy)
		return (EBUSY);
	return (0);
}

/*
 * Disassociate the underlying file system from a vnode.
 *
 * Must be called with the interlock held, and will return with it held.
 */
void
vclean(vnode_t *vp, int flags)
{
	lwp_t *l = curlwp;
	bool recycle, active;

	KASSERT(mutex_owned(&vp->v_interlock));
	KASSERT((vp->v_iflag & VI_MARKER) == 0);
	KASSERT(vp->v_usecount != 0);
	KASSERT(vp->v_freelisthd == NULL);

	/* If cleaning is already in progress wait until done and return. */
	if (vp->v_iflag & VI_XLOCK) {
		vwait(vp, VI_XLOCK);
		return;
	}

	/* If already clean, nothing to do. */
	if ((vp->v_iflag & VI_CLEAN) != 0) {
		return;
	}

	/*
	 * Prevent the vnode from being recycled or brought into use
	 * while we clean it out.
	 */
	if (vp->v_iflag & VI_XLOCK) {
		vpanic(vp, "vclean: deadlock");
	}
	vp->v_iflag |= VI_XLOCK;
	if (vp->v_iflag & VI_EXECMAP) {
		atomic_add_int(&uvmexp.execpages, -vp->v_uobj.uo_npages);
		atomic_add_int(&uvmexp.filepages, vp->v_uobj.uo_npages);
	}
	vp->v_iflag &= ~(VI_TEXT|VI_EXECMAP);
	active = (vp->v_usecount > 1);

	/*
	 * Even if the count is zero, the VOP_INACTIVE routine may still
	 * have the object locked while it cleans it out.  For
	 * active vnodes, it ensures that no other activity can
	 * occur while the underlying object is being cleaned out.
	 *
	 * We drain the lock to make sure we are the last one trying to
	 * get it and immediately resurrect the lock.  Future accesses
	 * for locking this _vnode_ will be protected by VXLOCK.  However,
	 * upper layers might be using the _lock_ in case the file system
	 * exported it and might access it while the vnode lingers in
	 * deadfs.
	 *
	 * XXXAD not true any more.
	 */
	VOP_LOCK(vp, LK_DRAIN | LK_RESURRECT | LK_INTERLOCK);

	/*
	 * Clean out any cached data associated with the vnode.
	 * If special device, remove it from special device alias list.
	 * if it is on one.
	 */
	if (flags & DOCLOSE) {
		int error;
		vnode_t *vq, *vx;

		error = vinvalbuf(vp, V_SAVE, NOCRED, l, 0, 0);
		if (error)
			error = vinvalbuf(vp, 0, NOCRED, l, 0, 0);
		KASSERT(error == 0);
		KASSERT((vp->v_iflag & VI_ONWORKLST) == 0);

		if (active)
			VOP_CLOSE(vp, FNONBLOCK, NOCRED);

		if ((vp->v_type == VBLK || vp->v_type == VCHR) &&
		    vp->v_specinfo != 0) {
			mutex_enter(&spechash_lock);
			if (vp->v_hashchain != NULL) {
				if (*vp->v_hashchain == vp) {
					*vp->v_hashchain = vp->v_specnext;
				} else {
					for (vq = *vp->v_hashchain; vq;
					     vq = vq->v_specnext) {
						if (vq->v_specnext != vp)
							continue;
						vq->v_specnext = vp->v_specnext;
						break;
					}
					if (vq == NULL)
						panic("missing bdev");
				}
				if (vp->v_iflag & VI_ALIASED) {
					vx = NULL;
					for (vq = *vp->v_hashchain; vq;
					     vq = vq->v_specnext) {
						if (vq->v_rdev != vp->v_rdev ||
						    vq->v_type != vp->v_type)
							continue;
						if (vx)
							break;
						vx = vq;
					}
					if (vx == NULL)
						panic("missing alias");
					if (vq == NULL)
						vx->v_iflag &= ~VI_ALIASED;
					vp->v_iflag &= ~VI_ALIASED;
				}
			}
			mutex_exit(&spechash_lock);
			FREE(vp->v_specinfo, M_VNODE);
			vp->v_specinfo = NULL;
		}
	}

	/*
	 * If purging an active vnode, it must be closed and
	 * deactivated before being reclaimed. Note that the
	 * VOP_INACTIVE will unlock the vnode.
	 */
	if (active) {
		VOP_INACTIVE(vp, &recycle);
	} else {
		/*
		 * Any other processes trying to obtain this lock must first
		 * wait for VI_XLOCK to clear, then call the new lock operation.
		 */
		VOP_UNLOCK(vp, 0);
	}

	/* Disassociate the underlying file system from the vnode. */
	if (VOP_RECLAIM(vp)) {
		vpanic(vp, "vclean: cannot reclaim");
	}

	KASSERT(vp->v_uobj.uo_npages == 0);
	if (vp->v_type == VREG && vp->v_ractx != NULL) {
		uvm_ra_freectx(vp->v_ractx);
		vp->v_ractx = NULL;
	}
	cache_purge(vp);

	/* Done with purge, notify sleepers of the grim news. */
	vp->v_op = dead_vnodeop_p;
	vp->v_tag = VT_NON;
	mutex_enter(&vp->v_interlock);
	vp->v_vnlock = NULL;
	VN_KNOTE(vp, NOTE_REVOKE);
	vp->v_iflag &= ~VI_XLOCK;
	vp->v_iflag |= VI_CLEAN;
	vp->v_vflag &= ~VV_LOCKSWORK;
	cv_broadcast(&vp->v_cv);

	KASSERT((vp->v_iflag & VI_ONWORKLST) == 0);
}

/*
 * Recycle an unused vnode to the front of the free list.
 * Release the passed interlock if the vnode will be recycled.
 */
int
vrecycle(vnode_t *vp, kmutex_t *inter_lkp, struct lwp *l)
{

	KASSERT((vp->v_iflag & VI_MARKER) == 0);

	mutex_enter(&vp->v_interlock);
	if (vp->v_usecount != 0) {
		mutex_exit(&vp->v_interlock);
		return (0);
	}
	if (inter_lkp)
		mutex_exit(inter_lkp);
	vremfree(vp);
	vp->v_usecount++;
	vclean(vp, DOCLOSE);
	vrelel(vp, 0, 0);
	return (1);
}

/*
 * Eliminate all activity associated with a vnode in preparation for
 * reuse.  Drops a reference from the vnode.
 */
void
vgone(vnode_t *vp)
{

	mutex_enter(&vp->v_interlock);
	vclean(vp, DOCLOSE);
	vrelel(vp, 0, 0);
}

/*
 * Lookup a vnode by device number.
 */
int
vfinddev(dev_t dev, enum vtype type, vnode_t **vpp)
{
	vnode_t *vp;
	int rc = 0;

	mutex_enter(&spechash_lock);
	for (vp = speclisth[SPECHASH(dev)]; vp; vp = vp->v_specnext) {
		if (dev != vp->v_rdev || type != vp->v_type)
			continue;
		*vpp = vp;
		rc = 1;
		break;
	}
	mutex_exit(&spechash_lock);
	return (rc);
}

/*
 * Revoke all the vnodes corresponding to the specified minor number
 * range (endpoints inclusive) of the specified major.
 */
void
vdevgone(int maj, int minl, int minh, enum vtype type)
{
	vnode_t *vp;
	int mn;

	vp = NULL;	/* XXX gcc */

	for (mn = minl; mn <= minh; mn++)
		if (vfinddev(makedev(maj, mn), type, &vp))
			VOP_REVOKE(vp, REVOKEALL);
}

/*
 * Calculate the total number of references to a special device.
 */
int
vcount(vnode_t *vp)
{
	vnode_t *vq, *vnext;
	int count;

loop:
	mutex_enter(&spechash_lock);
	if ((vp->v_iflag & VI_ALIASED) == 0) {
		mutex_exit(&spechash_lock);
		return (vp->v_usecount);
	}
	for (count = 0, vq = *vp->v_hashchain; vq; vq = vnext) {
		vnext = vq->v_specnext;
		if (vq->v_rdev != vp->v_rdev || vq->v_type != vp->v_type)
			continue;
		/*
		 * Alias, but not in use, so flush it out.
		 */
	    	mutex_enter(&vq->v_interlock);
		if (vq->v_usecount == 0 && vq != vp &&
		    (vq->v_iflag & VI_XLOCK) == 0) {
			mutex_exit(&spechash_lock);
			vremfree(vq);
			vq->v_usecount++;
			vclean(vq, DOCLOSE);
			vrelel(vq, 1, 0);
			goto loop;
		}
		count += vq->v_usecount;
	    	mutex_exit(&vq->v_interlock);
	}
	mutex_exit(&spechash_lock);
	return (count);
}

/*
 * sysctl helper routine to return list of supported fstypes
 */
static int
sysctl_vfs_generic_fstypes(SYSCTLFN_ARGS)
{
	char bf[sizeof(((struct statvfs *)NULL)->f_fstypename)];
	char *where = oldp;
	struct vfsops *v;
	size_t needed, left, slen;
	int error, first;

	if (newp != NULL)
		return (EPERM);
	if (namelen != 0)
		return (EINVAL);

	first = 1;
	error = 0;
	needed = 0;
	left = *oldlenp;

	mutex_enter(&vfs_list_lock);
	LIST_FOREACH(v, &vfs_list, vfs_list) {
		if (where == NULL)
			needed += strlen(v->vfs_name) + 1;
		else {
			memset(bf, 0, sizeof(bf));
			if (first) {
				strncpy(bf, v->vfs_name, sizeof(bf));
				first = 0;
			} else {
				bf[0] = ' ';
				strncpy(bf + 1, v->vfs_name, sizeof(bf) - 1);
			}
			bf[sizeof(bf)-1] = '\0';
			slen = strlen(bf);
			if (left < slen + 1)
				break;
			/* +1 to copy out the trailing NUL byte */
			v->vfs_refcount++;
			mutex_exit(&vfs_list_lock);
			error = copyout(bf, where, slen + 1);
			mutex_enter(&vfs_list_lock);
			v->vfs_refcount--;
			if (error)
				break;
			where += slen;
			needed += slen;
			left -= slen;
		}
	}
	mutex_exit(&vfs_list_lock);
	*oldlenp = needed;
	return (error);
}

/*
 * Top level filesystem related information gathering.
 */
SYSCTL_SETUP(sysctl_vfs_setup, "sysctl vfs subtree setup")
{
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "vfs", NULL,
		       NULL, 0, NULL, 0,
		       CTL_VFS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "generic",
		       SYSCTL_DESCR("Non-specific vfs related information"),
		       NULL, 0, NULL, 0,
		       CTL_VFS, VFS_GENERIC, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "usermount",
		       SYSCTL_DESCR("Whether unprivileged users may mount "
				    "filesystems"),
		       NULL, 0, &dovfsusermount, 0,
		       CTL_VFS, VFS_GENERIC, VFS_USERMOUNT, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRING, "fstypes",
		       SYSCTL_DESCR("List of file systems present"),
		       sysctl_vfs_generic_fstypes, 0, NULL, 0,
		       CTL_VFS, VFS_GENERIC, CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "magiclinks",
		       SYSCTL_DESCR("Whether \"magic\" symlinks are expanded"),
		       NULL, 0, &vfs_magiclinks, 0,
		       CTL_VFS, VFS_GENERIC, VFS_MAGICLINKS, CTL_EOL);
}


int kinfo_vdebug = 1;
int kinfo_vgetfailed;
#define KINFO_VNODESLOP	10
/*
 * Dump vnode list (via sysctl).
 * Copyout address of vnode followed by vnode.
 */
/* ARGSUSED */
int
sysctl_kern_vnode(SYSCTLFN_ARGS)
{
	char *where = oldp;
	size_t *sizep = oldlenp;
	struct mount *mp, *nmp;
	vnode_t *vp, *mvp;
	char *bp = where, *savebp;
	char *ewhere;
	int error;

	if (namelen != 0)
		return (EOPNOTSUPP);
	if (newp != NULL)
		return (EPERM);

#define VPTRSZ	sizeof(vnode_t *)
#define VNODESZ	sizeof(vnode_t)
	if (where == NULL) {
		*sizep = (numvnodes + KINFO_VNODESLOP) * (VPTRSZ + VNODESZ);
		return (0);
	}
	ewhere = where + *sizep;


	mutex_enter(&mountlist_lock);
	for (mp = CIRCLEQ_FIRST(&mountlist); mp != (void *)&mountlist;
	     mp = nmp) {
		if (vfs_busy(mp, LK_NOWAIT, &mountlist_lock)) {
			nmp = CIRCLEQ_NEXT(mp, mnt_list);
			continue;
		}
		savebp = bp;
		/* Allocate a marker vnode. */
		if ((mvp = valloc(mp)) == NULL)
			return (ENOMEM);
		mutex_enter(&mntvnode_lock);
		for (vp = TAILQ_FIRST(&mp->mnt_vnodelist); vp; vp = vunmark(mvp)) {
			vmark(mvp, vp);
			/*
			 * Check that the vp is still associated with
			 * this filesystem.  RACE: could have been
			 * recycled onto the same filesystem.
			 */
			if (vp->v_mount != mp || vismarker(vp))
				continue;
			if (bp + VPTRSZ + VNODESZ > ewhere) {
				(void)vunmark(mvp);
				mutex_exit(&mntvnode_lock);
				vfree(mvp);
				*sizep = bp - where;
				return (ENOMEM);
			}
			/* XXXAD copy to temporary buffer */
			mutex_exit(&mntvnode_lock);
			if ((error = copyout((void *)&vp, bp, VPTRSZ)) ||
			   (error = copyout((void *)vp, bp + VPTRSZ, VNODESZ))) {
			   	mutex_enter(&mntvnode_lock);
				(void)vunmark(mvp);
				mutex_exit(&mntvnode_lock);
				vfree(mvp);
				return (error);
			}
			bp += VPTRSZ + VNODESZ;
			mutex_enter(&mntvnode_lock);
		}
		mutex_exit(&mntvnode_lock);
		mutex_enter(&mountlist_lock);
		nmp = CIRCLEQ_NEXT(mp, mnt_list);
		vfs_unbusy(mp);
		vfree(mvp);
	}
	mutex_exit(&mountlist_lock);

	*sizep = bp - where;
	return (0);
}

/*
 * Remove clean vnodes from a mountpoint's vnode list.
 */
void
vfs_scrubvnlist(struct mount *mp)
{
	vnode_t *vp, *nvp;

	mutex_enter(&mntvnode_lock);
	for (vp = TAILQ_FIRST(&mp->mnt_vnodelist); vp; vp = nvp) {
		nvp = TAILQ_NEXT(vp, v_mntvnodes);
		mutex_enter(&vp->v_interlock);
		if ((vp->v_iflag & VI_CLEAN) != 0)
			TAILQ_REMOVE(&mp->mnt_vnodelist, vp, v_mntvnodes);
		mutex_exit(&vp->v_interlock);
	}
	mutex_exit(&mntvnode_lock);
}

/*
 * Check to see if a filesystem is mounted on a block device.
 */
int
vfs_mountedon(vnode_t *vp)
{
	vnode_t *vq;
	int error = 0;

	if (vp->v_type != VBLK)
		return ENOTBLK;
	if (vp->v_specmountpoint != NULL)
		return (EBUSY);
	if (vp->v_iflag & VI_ALIASED) {
		mutex_enter(&spechash_lock);
		for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
			if (vq->v_rdev != vp->v_rdev ||
			    vq->v_type != vp->v_type)
				continue;
			if (vq->v_specmountpoint != NULL) {
				error = EBUSY;
				break;
			}
		}
		mutex_exit(&spechash_lock);
	}
	return (error);
}

/*
 * Unmount all file systems.
 * We traverse the list in reverse order under the assumption that doing so
 * will avoid needing to worry about dependencies.
 */
void
vfs_unmountall(struct lwp *l)
{
	struct mount *mp, *nmp;
	int allerror, error;

	printf("unmounting file systems...");
	for (allerror = 0,
	     mp = mountlist.cqh_last; mp != (void *)&mountlist; mp = nmp) {
		nmp = mp->mnt_list.cqe_prev;
#ifdef DEBUG
		printf("\nunmounting %s (%s)...",
		    mp->mnt_stat.f_mntonname, mp->mnt_stat.f_mntfromname);
#endif
		/*
		 * XXX Freeze syncer.  Must do this before locking the
		 * mount point.  See dounmount() for details.
		 */
		mutex_enter(&syncer_mutex);
		if (vfs_busy(mp, 0, 0)) {
			mutex_exit(&syncer_mutex);
			continue;
		}
		if ((error = dounmount(mp, MNT_FORCE, l)) != 0) {
			printf("unmount of %s failed with error %d\n",
			    mp->mnt_stat.f_mntonname, error);
			allerror = 1;
		}
	}
	printf(" done\n");
	if (allerror)
		printf("WARNING: some file systems would not unmount\n");
}

/*
 * Sync and unmount file systems before shutting down.
 */
void
vfs_shutdown(void)
{
	struct lwp *l;

	/* XXX we're certainly not running in lwp0's context! */
	l = curlwp;
	if (l == NULL)
		l = &lwp0;

	printf("syncing disks... ");

	/* remove user processes from run queue */
	suspendsched();
	(void) spl0();

	/* avoid coming back this way again if we panic. */
	doing_shutdown = 1;

	sys_sync(l, NULL, NULL);

	/* Wait for sync to finish. */
	if (buf_syncwait() != 0) {
#if defined(DDB) && defined(DEBUG_HALT_BUSY)
		Debugger();
#endif
		printf("giving up\n");
		return;
	} else
		printf("done\n");

	/*
	 * If we've panic'd, don't make the situation potentially
	 * worse by unmounting the file systems.
	 */
	if (panicstr != NULL)
		return;

	/* Release inodes held by texts before update. */
#ifdef notdef
	vnshutdown();
#endif
	/* Unmount file systems. */
	vfs_unmountall(l);
}

/*
 * Mount the root file system.  If the operator didn't specify a
 * file system to use, try all possible file systems until one
 * succeeds.
 */
int
vfs_mountroot(void)
{
	struct vfsops *v;
	int error = ENODEV;

	if (root_device == NULL)
		panic("vfs_mountroot: root device unknown");

	switch (device_class(root_device)) {
	case DV_IFNET:
		if (rootdev != NODEV)
			panic("vfs_mountroot: rootdev set for DV_IFNET "
			    "(0x%08x -> %d,%d)", rootdev,
			    major(rootdev), minor(rootdev));
		break;

	case DV_DISK:
		if (rootdev == NODEV)
			panic("vfs_mountroot: rootdev not set for DV_DISK");
	        if (bdevvp(rootdev, &rootvp))
	                panic("vfs_mountroot: can't get vnode for rootdev");
		error = VOP_OPEN(rootvp, FREAD, FSCRED);
		if (error) {
			printf("vfs_mountroot: can't open root device\n");
			return (error);
		}
		break;

	default:
		printf("%s: inappropriate for root file system\n",
		    root_device->dv_xname);
		return (ENODEV);
	}

	/*
	 * If user specified a file system, use it.
	 */
	if (mountroot != NULL) {
		error = (*mountroot)();
		goto done;
	}

	/*
	 * Try each file system currently configured into the kernel.
	 */
	mutex_enter(&vfs_list_lock);
	LIST_FOREACH(v, &vfs_list, vfs_list) {
		if (v->vfs_mountroot == NULL)
			continue;
#ifdef DEBUG
		aprint_normal("mountroot: trying %s...\n", v->vfs_name);
#endif
		v->vfs_refcount++;
		mutex_exit(&vfs_list_lock);
		error = (*v->vfs_mountroot)();
		mutex_enter(&vfs_list_lock);
		v->vfs_refcount--;
		if (!error) {
			aprint_normal("root file system type: %s\n",
			    v->vfs_name);
			break;
		}
	}
	mutex_exit(&vfs_list_lock);

	if (v == NULL) {
		printf("no file system for %s", root_device->dv_xname);
		if (device_class(root_device) == DV_DISK)
			printf(" (dev 0x%x)", rootdev);
		printf("\n");
		error = EFTYPE;
	}

done:
	if (error && device_class(root_device) == DV_DISK) {
		VOP_CLOSE(rootvp, FREAD, FSCRED);
		vrele(rootvp);
	}
	return (error);
}
