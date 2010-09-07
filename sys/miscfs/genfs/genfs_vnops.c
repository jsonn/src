/*	$NetBSD: genfs_vnops.c,v 1.167.10.1 2010/09/07 19:33:35 bouyer Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: genfs_vnops.c,v 1.167.10.1 2010/09/07 19:33:35 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/kauth.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>
#include <miscfs/specfs/specdev.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pager.h>

static void filt_genfsdetach(struct knote *);
static int filt_genfsread(struct knote *, long);
static int filt_genfsvnode(struct knote *, long);

int
genfs_poll(void *v)
{
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int a_events;
		struct lwp *a_l;
	} */ *ap = v;

	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

int
genfs_seek(void *v)
{
	struct vop_seek_args /* {
		struct vnode *a_vp;
		off_t a_oldoff;
		off_t a_newoff;
		kauth_cred_t cred;
	} */ *ap = v;

	if (ap->a_newoff < 0)
		return (EINVAL);

	return (0);
}

int
genfs_abortop(void *v)
{
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap = v;

	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		PNBUF_PUT(ap->a_cnp->cn_pnbuf);
	return (0);
}

int
genfs_fcntl(void *v)
{
	struct vop_fcntl_args /* {
		struct vnode *a_vp;
		u_int a_command;
		void *a_data;
		int a_fflag;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;

	if (ap->a_command == F_SETFL)
		return (0);
	else
		return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
genfs_badop(void *v)
{

	panic("genfs: bad op");
}

/*ARGSUSED*/
int
genfs_nullop(void *v)
{

	return (0);
}

/*ARGSUSED*/
int
genfs_einval(void *v)
{

	return (EINVAL);
}

/*
 * Called when an fs doesn't support a particular vop.
 * This takes care to vrele, vput, or vunlock passed in vnodes.
 */
int
genfs_eopnotsupp(void *v)
{
	struct vop_generic_args /*
		struct vnodeop_desc *a_desc;
		/ * other random data follows, presumably * /
	} */ *ap = v;
	struct vnodeop_desc *desc = ap->a_desc;
	struct vnode *vp, *vp_last = NULL;
	int flags, i, j, offset;

	flags = desc->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; flags >>=1, i++) {
		if ((offset = desc->vdesc_vp_offsets[i]) == VDESC_NO_OFFSET)
			break;	/* stop at end of list */
		if ((j = flags & VDESC_VP0_WILLPUT)) {
			vp = *VOPARG_OFFSETTO(struct vnode **, offset, ap);

			/* Skip if NULL */
			if (!vp)
				continue;

			switch (j) {
			case VDESC_VP0_WILLPUT:
				/* Check for dvp == vp cases */
				if (vp == vp_last)
					vrele(vp);
				else {
					vput(vp);
					vp_last = vp;
				}
				break;
			case VDESC_VP0_WILLUNLOCK:
				VOP_UNLOCK(vp, 0);
				break;
			case VDESC_VP0_WILLRELE:
				vrele(vp);
				break;
			}
		}
	}

	return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
genfs_ebadf(void *v)
{

	return (EBADF);
}

/* ARGSUSED */
int
genfs_enoioctl(void *v)
{

	return (EPASSTHROUGH);
}


/*
 * Eliminate all activity associated with the requested vnode
 * and with all vnodes aliased to the requested vnode.
 */
int
genfs_revoke(void *v)
{
	struct vop_revoke_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;

#ifdef DIAGNOSTIC
	if ((ap->a_flags & REVOKEALL) == 0)
		panic("genfs_revoke: not revokeall");
#endif
	vrevoke(ap->a_vp);
	return (0);
}

/*
 * Lock the node.
 */
int
genfs_lock(void *v)
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int flags = ap->a_flags;

	if ((flags & LK_INTERLOCK) != 0) {
		flags &= ~LK_INTERLOCK;
		mutex_exit(&vp->v_interlock);
	}

	return (vlockmgr(vp->v_vnlock, flags));
}

/*
 * Unlock the node.
 */
int
genfs_unlock(void *v)
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	KASSERT(ap->a_flags == 0);

	return (vlockmgr(vp->v_vnlock, LK_RELEASE));
}

/*
 * Return whether or not the node is locked.
 */
int
genfs_islocked(void *v)
{
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	return (vlockstatus(vp->v_vnlock));
}

/*
 * Stubs to use when there is no locking to be done on the underlying object.
 */
int
genfs_nolock(void *v)
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct lwp *a_l;
	} */ *ap = v;

	/*
	 * Since we are not using the lock manager, we must clear
	 * the interlock here.
	 */
	if (ap->a_flags & LK_INTERLOCK)
		mutex_exit(&ap->a_vp->v_interlock);
	return (0);
}

int
genfs_nounlock(void *v)
{

	return (0);
}

int
genfs_noislocked(void *v)
{

	return (0);
}

int
genfs_mmap(void *v)
{

	return (0);
}

void
genfs_node_init(struct vnode *vp, const struct genfs_ops *ops)
{
	struct genfs_node *gp = VTOG(vp);

	rw_init(&gp->g_glock);
	gp->g_op = ops;
}

void
genfs_node_destroy(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	rw_destroy(&gp->g_glock);
}

void
genfs_size(struct vnode *vp, off_t size, off_t *eobp, int flags)
{
	int bsize;

	bsize = 1 << vp->v_mount->mnt_fs_bshift;
	*eobp = (size + bsize - 1) & ~(bsize - 1);
}

static void
filt_genfsdetach(struct knote *kn)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;

	mutex_enter(&vp->v_interlock);
	SLIST_REMOVE(&vp->v_klist, kn, knote, kn_selnext);
	mutex_exit(&vp->v_interlock);
}

static int
filt_genfsread(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	int rv;

	/*
	 * filesystem is gone, so set the EOF flag and schedule
	 * the knote for deletion.
	 */
	switch (hint) {
	case NOTE_REVOKE:
		KASSERT(mutex_owned(&vp->v_interlock));
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	case 0:
		mutex_enter(&vp->v_interlock);
		kn->kn_data = vp->v_size - ((file_t *)kn->kn_obj)->f_offset;
		rv = (kn->kn_data != 0);
		mutex_exit(&vp->v_interlock);
		return rv;
	default:
		KASSERT(mutex_owned(&vp->v_interlock));
		kn->kn_data = vp->v_size - ((file_t *)kn->kn_obj)->f_offset;
		return (kn->kn_data != 0);
	}
}

static int
filt_genfsvnode(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	int fflags;

	switch (hint) {
	case NOTE_REVOKE:
		KASSERT(mutex_owned(&vp->v_interlock));
		kn->kn_flags |= EV_EOF;
		if ((kn->kn_sfflags & hint) != 0)
			kn->kn_fflags |= hint;
		return (1);
	case 0:
		mutex_enter(&vp->v_interlock);
		fflags = kn->kn_fflags;
		mutex_exit(&vp->v_interlock);
		break;
	default:
		KASSERT(mutex_owned(&vp->v_interlock));
		if ((kn->kn_sfflags & hint) != 0)
			kn->kn_fflags |= hint;
		fflags = kn->kn_fflags;
		break;
	}

	return (fflags != 0);
}

static const struct filterops genfsread_filtops =
	{ 1, NULL, filt_genfsdetach, filt_genfsread };
static const struct filterops genfsvnode_filtops =
	{ 1, NULL, filt_genfsdetach, filt_genfsvnode };

int
genfs_kqfilter(void *v)
{
	struct vop_kqfilter_args /* {
		struct vnode	*a_vp;
		struct knote	*a_kn;
	} */ *ap = v;
	struct vnode *vp;
	struct knote *kn;

	vp = ap->a_vp;
	kn = ap->a_kn;
	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &genfsread_filtops;
		break;
	case EVFILT_VNODE:
		kn->kn_fop = &genfsvnode_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = vp;

	mutex_enter(&vp->v_interlock);
	SLIST_INSERT_HEAD(&vp->v_klist, kn, kn_selnext);
	mutex_exit(&vp->v_interlock);

	return (0);
}

void
genfs_node_wrlock(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	rw_enter(&gp->g_glock, RW_WRITER);
}

void
genfs_node_rdlock(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	rw_enter(&gp->g_glock, RW_READER);
}

int
genfs_node_rdtrylock(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	return rw_tryenter(&gp->g_glock, RW_READER);
}

void
genfs_node_unlock(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	rw_exit(&gp->g_glock);
}

int
genfs_node_wrlocked(struct vnode *vp)
{
	struct genfs_node *gp = VTOG(vp);

	return rw_write_held(&gp->g_glock);
}
