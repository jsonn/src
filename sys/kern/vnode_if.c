/*	$NetBSD: vnode_if.c,v 1.44.2.6 2005/01/17 19:32:26 skrll Exp $	*/

/*
 * Warning: This file is generated automatically.
 * (Modifications made here may easily be lost!)
 *
 * Created from the file:
 *	NetBSD: vnode_if.src,v 1.35.2.5 2004/09/24 10:53:43 skrll Exp 
 * by the script:
 *	NetBSD: vnode_if.sh,v 1.32.2.5 2004/09/24 10:53:43 skrll Exp 
 */

/*
 * Copyright (c) 1992, 1993, 1994, 1995
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vnode_if.c,v 1.44.2.6 2005/01/17 19:32:26 skrll Exp $");


/*
 * If we have LKM support, always include the non-inline versions for
 * LKMs.  Otherwise, do it based on the option.
 */
#ifdef LKM
#define	VNODE_OP_NOINLINE
#else
#include "opt_vnode_op_noinline.h"
#endif
#include "opt_vnode_lockdebug.h"

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/vnode.h>

const struct vnodeop_desc vop_default_desc = {
	0,
	"default",
	0,
	NULL,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};


/* Special cases: */

const int vop_bwrite_vp_offsets[] = {
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_bwrite_desc = {
	1,
	"vop_bwrite",
	0,
	vop_bwrite_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_BWRITE(bp)
	struct buf *bp;
{
	struct vop_bwrite_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_bwrite);
	a.a_bp = bp;
	return (VCALL(bp->b_vp, VOFFSET(vop_bwrite), &a));
}
#endif

/* End of special cases */

const int vop_lookup_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_lookup_args,a_dvp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_lookup_desc = {
	2,
	"vop_lookup",
	0,
	vop_lookup_vp_offsets,
	VOPARG_OFFSETOF(struct vop_lookup_args, a_vpp),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_lookup_args, a_cnp),
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_LOOKUP(dvp, vpp, cnp)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	struct vop_lookup_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_lookup);
	a.a_dvp = dvp;
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_lookup), &a));
}
#endif

const int vop_create_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_create_args,a_dvp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_create_desc = {
	3,
	"vop_create",
	0 | VDESC_VP0_WILLPUT,
	vop_create_vp_offsets,
	VOPARG_OFFSETOF(struct vop_create_args, a_vpp),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_create_args, a_cnp),
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_CREATE(dvp, vpp, cnp, vap)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
{
	struct vop_create_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_dvp;
#endif
	a.a_desc = VDESC(vop_create);
	a.a_dvp = dvp;
#ifdef VNODE_LOCKDEBUG
	islocked_dvp = (dvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(dvp) == LK_EXCLUSIVE) : 1;
	if (islocked_dvp != 1)
		panic("vop_create: dvp: locked %d, expected %d", islocked_dvp, 1);
#endif
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	return (VCALL(dvp, VOFFSET(vop_create), &a));
}
#endif

const int vop_mknod_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_mknod_args,a_dvp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_mknod_desc = {
	4,
	"vop_mknod",
	0 | VDESC_VP0_WILLPUT,
	vop_mknod_vp_offsets,
	VOPARG_OFFSETOF(struct vop_mknod_args, a_vpp),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_mknod_args, a_cnp),
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_MKNOD(dvp, vpp, cnp, vap)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
{
	struct vop_mknod_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_dvp;
#endif
	a.a_desc = VDESC(vop_mknod);
	a.a_dvp = dvp;
#ifdef VNODE_LOCKDEBUG
	islocked_dvp = (dvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(dvp) == LK_EXCLUSIVE) : 1;
	if (islocked_dvp != 1)
		panic("vop_mknod: dvp: locked %d, expected %d", islocked_dvp, 1);
#endif
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	return (VCALL(dvp, VOFFSET(vop_mknod), &a));
}
#endif

const int vop_open_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_open_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_open_desc = {
	5,
	"vop_open",
	0,
	vop_open_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_open_args, a_cred),
	VOPARG_OFFSETOF(struct vop_open_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_OPEN(vp, mode, cred, l)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_open_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_open);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_open: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_mode = mode;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_open), &a));
}
#endif

const int vop_close_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_close_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_close_desc = {
	6,
	"vop_close",
	0,
	vop_close_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_close_args, a_cred),
	VOPARG_OFFSETOF(struct vop_close_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_CLOSE(vp, fflag, cred, l)
	struct vnode *vp;
	int fflag;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_close_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_close);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_close: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_fflag = fflag;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_close), &a));
}
#endif

const int vop_access_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_access_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_access_desc = {
	7,
	"vop_access",
	0,
	vop_access_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_access_args, a_cred),
	VOPARG_OFFSETOF(struct vop_access_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_ACCESS(vp, mode, cred, l)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_access_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_access);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_access: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_mode = mode;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_access), &a));
}
#endif

const int vop_getattr_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_getattr_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_getattr_desc = {
	8,
	"vop_getattr",
	0,
	vop_getattr_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_getattr_args, a_cred),
	VOPARG_OFFSETOF(struct vop_getattr_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_GETATTR(vp, vap, cred, l)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_getattr_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_getattr);
	a.a_vp = vp;
	a.a_vap = vap;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_getattr), &a));
}
#endif

const int vop_setattr_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_setattr_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_setattr_desc = {
	9,
	"vop_setattr",
	0,
	vop_setattr_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_setattr_args, a_cred),
	VOPARG_OFFSETOF(struct vop_setattr_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_SETATTR(vp, vap, cred, l)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_setattr_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_setattr);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_setattr: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_vap = vap;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_setattr), &a));
}
#endif

const int vop_read_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_read_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_read_desc = {
	10,
	"vop_read",
	0,
	vop_read_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_read_args, a_cred),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_READ(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct vop_read_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_read);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_read: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_uio = uio;
	a.a_ioflag = ioflag;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_read), &a));
}
#endif

const int vop_write_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_write_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_write_desc = {
	11,
	"vop_write",
	0,
	vop_write_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_write_args, a_cred),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_WRITE(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct vop_write_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_write);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_write: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_uio = uio;
	a.a_ioflag = ioflag;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_write), &a));
}
#endif

const int vop_ioctl_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_ioctl_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_ioctl_desc = {
	12,
	"vop_ioctl",
	0,
	vop_ioctl_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_ioctl_args, a_cred),
	VOPARG_OFFSETOF(struct vop_ioctl_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_IOCTL(vp, command, data, fflag, cred, l)
	struct vnode *vp;
	u_long command;
	void *data;
	int fflag;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_ioctl_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_ioctl);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 0;
	if (islocked_vp != 0)
		panic("vop_ioctl: vp: locked %d, expected %d", islocked_vp, 0);
#endif
	a.a_command = command;
	a.a_data = data;
	a.a_fflag = fflag;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_ioctl), &a));
}
#endif

const int vop_fcntl_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_fcntl_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_fcntl_desc = {
	13,
	"vop_fcntl",
	0,
	vop_fcntl_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_fcntl_args, a_cred),
	VOPARG_OFFSETOF(struct vop_fcntl_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_FCNTL(vp, command, data, fflag, cred, l)
	struct vnode *vp;
	u_int command;
	void *data;
	int fflag;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_fcntl_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_fcntl);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_fcntl: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_command = command;
	a.a_data = data;
	a.a_fflag = fflag;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_fcntl), &a));
}
#endif

const int vop_poll_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_poll_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_poll_desc = {
	14,
	"vop_poll",
	0,
	vop_poll_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_poll_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_POLL(vp, events, l)
	struct vnode *vp;
	int events;
	struct lwp *l;
{
	struct vop_poll_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_poll);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 0;
	if (islocked_vp != 0)
		panic("vop_poll: vp: locked %d, expected %d", islocked_vp, 0);
#endif
	a.a_events = events;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_poll), &a));
}
#endif

const int vop_kqfilter_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_kqfilter_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_kqfilter_desc = {
	15,
	"vop_kqfilter",
	0,
	vop_kqfilter_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_KQFILTER(vp, kn)
	struct vnode *vp;
	struct knote *kn;
{
	struct vop_kqfilter_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_kqfilter);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 0;
	if (islocked_vp != 0)
		panic("vop_kqfilter: vp: locked %d, expected %d", islocked_vp, 0);
#endif
	a.a_kn = kn;
	return (VCALL(vp, VOFFSET(vop_kqfilter), &a));
}
#endif

const int vop_revoke_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_revoke_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_revoke_desc = {
	16,
	"vop_revoke",
	0,
	vop_revoke_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_REVOKE(vp, flags)
	struct vnode *vp;
	int flags;
{
	struct vop_revoke_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_revoke);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 0;
	if (islocked_vp != 0)
		panic("vop_revoke: vp: locked %d, expected %d", islocked_vp, 0);
#endif
	a.a_flags = flags;
	return (VCALL(vp, VOFFSET(vop_revoke), &a));
}
#endif

const int vop_mmap_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_mmap_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_mmap_desc = {
	17,
	"vop_mmap",
	0,
	vop_mmap_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_mmap_args, a_cred),
	VOPARG_OFFSETOF(struct vop_mmap_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_MMAP(vp, fflags, cred, l)
	struct vnode *vp;
	int fflags;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_mmap_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_mmap);
	a.a_vp = vp;
	a.a_fflags = fflags;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_mmap), &a));
}
#endif

const int vop_fsync_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_fsync_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_fsync_desc = {
	18,
	"vop_fsync",
	0,
	vop_fsync_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_fsync_args, a_cred),
	VOPARG_OFFSETOF(struct vop_fsync_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_FSYNC(vp, cred, flags, offlo, offhi, l)
	struct vnode *vp;
	struct ucred *cred;
	int flags;
	off_t offlo;
	off_t offhi;
	struct lwp *l;
{
	struct vop_fsync_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_fsync);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_fsync: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_cred = cred;
	a.a_flags = flags;
	a.a_offlo = offlo;
	a.a_offhi = offhi;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_fsync), &a));
}
#endif

const int vop_seek_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_seek_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_seek_desc = {
	19,
	"vop_seek",
	0,
	vop_seek_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_seek_args, a_cred),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_SEEK(vp, oldoff, newoff, cred)
	struct vnode *vp;
	off_t oldoff;
	off_t newoff;
	struct ucred *cred;
{
	struct vop_seek_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_seek);
	a.a_vp = vp;
	a.a_oldoff = oldoff;
	a.a_newoff = newoff;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_seek), &a));
}
#endif

const int vop_remove_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_remove_args,a_dvp),
	VOPARG_OFFSETOF(struct vop_remove_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_remove_desc = {
	20,
	"vop_remove",
	0 | VDESC_VP0_WILLPUT | VDESC_VP1_WILLPUT,
	vop_remove_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_remove_args, a_cnp),
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_REMOVE(dvp, vp, cnp)
	struct vnode *dvp;
	struct vnode *vp;
	struct componentname *cnp;
{
	struct vop_remove_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_dvp;
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_remove);
	a.a_dvp = dvp;
#ifdef VNODE_LOCKDEBUG
	islocked_dvp = (dvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(dvp) == LK_EXCLUSIVE) : 1;
	if (islocked_dvp != 1)
		panic("vop_remove: dvp: locked %d, expected %d", islocked_dvp, 1);
#endif
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_remove: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_remove), &a));
}
#endif

const int vop_link_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_link_args,a_dvp),
	VOPARG_OFFSETOF(struct vop_link_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_link_desc = {
	21,
	"vop_link",
	0 | VDESC_VP0_WILLPUT,
	vop_link_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_link_args, a_cnp),
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_LINK(dvp, vp, cnp)
	struct vnode *dvp;
	struct vnode *vp;
	struct componentname *cnp;
{
	struct vop_link_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_dvp;
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_link);
	a.a_dvp = dvp;
#ifdef VNODE_LOCKDEBUG
	islocked_dvp = (dvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(dvp) == LK_EXCLUSIVE) : 1;
	if (islocked_dvp != 1)
		panic("vop_link: dvp: locked %d, expected %d", islocked_dvp, 1);
#endif
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 0;
	if (islocked_vp != 0)
		panic("vop_link: vp: locked %d, expected %d", islocked_vp, 0);
#endif
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_link), &a));
}
#endif

const int vop_rename_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_rename_args,a_fdvp),
	VOPARG_OFFSETOF(struct vop_rename_args,a_fvp),
	VOPARG_OFFSETOF(struct vop_rename_args,a_tdvp),
	VOPARG_OFFSETOF(struct vop_rename_args,a_tvp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_rename_desc = {
	22,
	"vop_rename",
	0 | VDESC_VP0_WILLRELE | VDESC_VP1_WILLRELE | VDESC_VP2_WILLPUT | VDESC_VP3_WILLPUT,
	vop_rename_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_rename_args, a_fcnp),
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_RENAME(fdvp, fvp, fcnp, tdvp, tvp, tcnp)
	struct vnode *fdvp;
	struct vnode *fvp;
	struct componentname *fcnp;
	struct vnode *tdvp;
	struct vnode *tvp;
	struct componentname *tcnp;
{
	struct vop_rename_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_fdvp;
	int islocked_fvp;
	int islocked_tdvp;
#endif
	a.a_desc = VDESC(vop_rename);
	a.a_fdvp = fdvp;
#ifdef VNODE_LOCKDEBUG
	islocked_fdvp = (fdvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(fdvp) == LK_EXCLUSIVE) : 0;
	if (islocked_fdvp != 0)
		panic("vop_rename: fdvp: locked %d, expected %d", islocked_fdvp, 0);
#endif
	a.a_fvp = fvp;
#ifdef VNODE_LOCKDEBUG
	islocked_fvp = (fvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(fvp) == LK_EXCLUSIVE) : 0;
	if (islocked_fvp != 0)
		panic("vop_rename: fvp: locked %d, expected %d", islocked_fvp, 0);
#endif
	a.a_fcnp = fcnp;
	a.a_tdvp = tdvp;
#ifdef VNODE_LOCKDEBUG
	islocked_tdvp = (tdvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE) : 1;
	if (islocked_tdvp != 1)
		panic("vop_rename: tdvp: locked %d, expected %d", islocked_tdvp, 1);
#endif
	a.a_tvp = tvp;
	a.a_tcnp = tcnp;
	return (VCALL(fdvp, VOFFSET(vop_rename), &a));
}
#endif

const int vop_mkdir_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_mkdir_args,a_dvp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_mkdir_desc = {
	23,
	"vop_mkdir",
	0 | VDESC_VP0_WILLPUT,
	vop_mkdir_vp_offsets,
	VOPARG_OFFSETOF(struct vop_mkdir_args, a_vpp),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_mkdir_args, a_cnp),
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_MKDIR(dvp, vpp, cnp, vap)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
{
	struct vop_mkdir_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_dvp;
#endif
	a.a_desc = VDESC(vop_mkdir);
	a.a_dvp = dvp;
#ifdef VNODE_LOCKDEBUG
	islocked_dvp = (dvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(dvp) == LK_EXCLUSIVE) : 1;
	if (islocked_dvp != 1)
		panic("vop_mkdir: dvp: locked %d, expected %d", islocked_dvp, 1);
#endif
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	return (VCALL(dvp, VOFFSET(vop_mkdir), &a));
}
#endif

const int vop_rmdir_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_rmdir_args,a_dvp),
	VOPARG_OFFSETOF(struct vop_rmdir_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_rmdir_desc = {
	24,
	"vop_rmdir",
	0 | VDESC_VP0_WILLPUT | VDESC_VP1_WILLPUT,
	vop_rmdir_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_rmdir_args, a_cnp),
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_RMDIR(dvp, vp, cnp)
	struct vnode *dvp;
	struct vnode *vp;
	struct componentname *cnp;
{
	struct vop_rmdir_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_dvp;
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_rmdir);
	a.a_dvp = dvp;
#ifdef VNODE_LOCKDEBUG
	islocked_dvp = (dvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(dvp) == LK_EXCLUSIVE) : 1;
	if (islocked_dvp != 1)
		panic("vop_rmdir: dvp: locked %d, expected %d", islocked_dvp, 1);
#endif
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_rmdir: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_rmdir), &a));
}
#endif

const int vop_symlink_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_symlink_args,a_dvp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_symlink_desc = {
	25,
	"vop_symlink",
	0 | VDESC_VP0_WILLPUT,
	vop_symlink_vp_offsets,
	VOPARG_OFFSETOF(struct vop_symlink_args, a_vpp),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_symlink_args, a_cnp),
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_SYMLINK(dvp, vpp, cnp, vap, target)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
	char *target;
{
	struct vop_symlink_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_dvp;
#endif
	a.a_desc = VDESC(vop_symlink);
	a.a_dvp = dvp;
#ifdef VNODE_LOCKDEBUG
	islocked_dvp = (dvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(dvp) == LK_EXCLUSIVE) : 1;
	if (islocked_dvp != 1)
		panic("vop_symlink: dvp: locked %d, expected %d", islocked_dvp, 1);
#endif
	a.a_vpp = vpp;
	a.a_cnp = cnp;
	a.a_vap = vap;
	a.a_target = target;
	return (VCALL(dvp, VOFFSET(vop_symlink), &a));
}
#endif

const int vop_readdir_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_readdir_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_readdir_desc = {
	26,
	"vop_readdir",
	0,
	vop_readdir_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_readdir_args, a_cred),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_READDIR(vp, uio, cred, eofflag, cookies, ncookies)
	struct vnode *vp;
	struct uio *uio;
	struct ucred *cred;
	int *eofflag;
	off_t **cookies;
	int *ncookies;
{
	struct vop_readdir_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_readdir);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_readdir: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_uio = uio;
	a.a_cred = cred;
	a.a_eofflag = eofflag;
	a.a_cookies = cookies;
	a.a_ncookies = ncookies;
	return (VCALL(vp, VOFFSET(vop_readdir), &a));
}
#endif

const int vop_readlink_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_readlink_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_readlink_desc = {
	27,
	"vop_readlink",
	0,
	vop_readlink_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_readlink_args, a_cred),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_READLINK(vp, uio, cred)
	struct vnode *vp;
	struct uio *uio;
	struct ucred *cred;
{
	struct vop_readlink_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_readlink);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_readlink: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_uio = uio;
	a.a_cred = cred;
	return (VCALL(vp, VOFFSET(vop_readlink), &a));
}
#endif

const int vop_abortop_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_abortop_args,a_dvp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_abortop_desc = {
	28,
	"vop_abortop",
	0,
	vop_abortop_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_abortop_args, a_cnp),
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_ABORTOP(dvp, cnp)
	struct vnode *dvp;
	struct componentname *cnp;
{
	struct vop_abortop_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_abortop);
	a.a_dvp = dvp;
	a.a_cnp = cnp;
	return (VCALL(dvp, VOFFSET(vop_abortop), &a));
}
#endif

const int vop_inactive_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_inactive_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_inactive_desc = {
	29,
	"vop_inactive",
	0 | VDESC_VP0_WILLUNLOCK,
	vop_inactive_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_inactive_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_INACTIVE(vp, l)
	struct vnode *vp;
	struct lwp *l;
{
	struct vop_inactive_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_inactive);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_inactive: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_inactive), &a));
}
#endif

const int vop_reclaim_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_reclaim_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_reclaim_desc = {
	30,
	"vop_reclaim",
	0,
	vop_reclaim_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_reclaim_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_RECLAIM(vp, l)
	struct vnode *vp;
	struct lwp *l;
{
	struct vop_reclaim_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_reclaim);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 0;
	if (islocked_vp != 0)
		panic("vop_reclaim: vp: locked %d, expected %d", islocked_vp, 0);
#endif
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_reclaim), &a));
}
#endif

const int vop_lock_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_lock_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_lock_desc = {
	31,
	"vop_lock",
	0,
	vop_lock_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_LOCK(vp, flags)
	struct vnode *vp;
	int flags;
{
	struct vop_lock_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_lock);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 0;
	if (islocked_vp != 0)
		panic("vop_lock: vp: locked %d, expected %d", islocked_vp, 0);
#endif
	a.a_flags = flags;
	return (VCALL(vp, VOFFSET(vop_lock), &a));
}
#endif

const int vop_unlock_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_unlock_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_unlock_desc = {
	32,
	"vop_unlock",
	0,
	vop_unlock_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_UNLOCK(vp, flags)
	struct vnode *vp;
	int flags;
{
	struct vop_unlock_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_unlock);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_unlock: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_flags = flags;
	return (VCALL(vp, VOFFSET(vop_unlock), &a));
}
#endif

const int vop_bmap_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_bmap_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_bmap_desc = {
	33,
	"vop_bmap",
	0,
	vop_bmap_vp_offsets,
	VOPARG_OFFSETOF(struct vop_bmap_args, a_vpp),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_BMAP(vp, bn, vpp, bnp, runp)
	struct vnode *vp;
	daddr_t bn;
	struct vnode **vpp;
	daddr_t *bnp;
	int *runp;
{
	struct vop_bmap_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_bmap);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_bmap: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_bn = bn;
	a.a_vpp = vpp;
	a.a_bnp = bnp;
	a.a_runp = runp;
	return (VCALL(vp, VOFFSET(vop_bmap), &a));
}
#endif

const int vop_strategy_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_strategy_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_strategy_desc = {
	34,
	"vop_strategy",
	0,
	vop_strategy_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_STRATEGY(vp, bp)
	struct vnode *vp;
	struct buf *bp;
{
	struct vop_strategy_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_strategy);
	a.a_vp = vp;
	a.a_bp = bp;
	return (VCALL(vp, VOFFSET(vop_strategy), &a));
}
#endif

const int vop_print_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_print_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_print_desc = {
	35,
	"vop_print",
	0,
	vop_print_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_PRINT(vp)
	struct vnode *vp;
{
	struct vop_print_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_print);
	a.a_vp = vp;
	return (VCALL(vp, VOFFSET(vop_print), &a));
}
#endif

const int vop_islocked_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_islocked_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_islocked_desc = {
	36,
	"vop_islocked",
	0,
	vop_islocked_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_ISLOCKED(vp)
	struct vnode *vp;
{
	struct vop_islocked_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_islocked);
	a.a_vp = vp;
	return (VCALL(vp, VOFFSET(vop_islocked), &a));
}
#endif

const int vop_pathconf_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_pathconf_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_pathconf_desc = {
	37,
	"vop_pathconf",
	0,
	vop_pathconf_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_PATHCONF(vp, name, retval)
	struct vnode *vp;
	int name;
	register_t *retval;
{
	struct vop_pathconf_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_pathconf);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_pathconf: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_name = name;
	a.a_retval = retval;
	return (VCALL(vp, VOFFSET(vop_pathconf), &a));
}
#endif

const int vop_advlock_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_advlock_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_advlock_desc = {
	38,
	"vop_advlock",
	0,
	vop_advlock_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_ADVLOCK(vp, id, op, fl, flags)
	struct vnode *vp;
	void *id;
	int op;
	struct flock *fl;
	int flags;
{
	struct vop_advlock_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_advlock);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 0;
	if (islocked_vp != 0)
		panic("vop_advlock: vp: locked %d, expected %d", islocked_vp, 0);
#endif
	a.a_id = id;
	a.a_op = op;
	a.a_fl = fl;
	a.a_flags = flags;
	return (VCALL(vp, VOFFSET(vop_advlock), &a));
}
#endif

const int vop_blkatoff_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_blkatoff_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_blkatoff_desc = {
	39,
	"vop_blkatoff",
	0,
	vop_blkatoff_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_BLKATOFF(vp, offset, res, bpp)
	struct vnode *vp;
	off_t offset;
	char **res;
	struct buf **bpp;
{
	struct vop_blkatoff_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_blkatoff);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_blkatoff: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_offset = offset;
	a.a_res = res;
	a.a_bpp = bpp;
	return (VCALL(vp, VOFFSET(vop_blkatoff), &a));
}
#endif

const int vop_valloc_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_valloc_args,a_pvp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_valloc_desc = {
	40,
	"vop_valloc",
	0,
	vop_valloc_vp_offsets,
	VOPARG_OFFSETOF(struct vop_valloc_args, a_vpp),
	VOPARG_OFFSETOF(struct vop_valloc_args, a_cred),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_VALLOC(pvp, mode, cred, vpp)
	struct vnode *pvp;
	int mode;
	struct ucred *cred;
	struct vnode **vpp;
{
	struct vop_valloc_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_pvp;
#endif
	a.a_desc = VDESC(vop_valloc);
	a.a_pvp = pvp;
#ifdef VNODE_LOCKDEBUG
	islocked_pvp = (pvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(pvp) == LK_EXCLUSIVE) : 1;
	if (islocked_pvp != 1)
		panic("vop_valloc: pvp: locked %d, expected %d", islocked_pvp, 1);
#endif
	a.a_mode = mode;
	a.a_cred = cred;
	a.a_vpp = vpp;
	return (VCALL(pvp, VOFFSET(vop_valloc), &a));
}
#endif

const int vop_balloc_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_balloc_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_balloc_desc = {
	41,
	"vop_balloc",
	0,
	vop_balloc_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_balloc_args, a_cred),
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_BALLOC(vp, startoffset, size, cred, flags, bpp)
	struct vnode *vp;
	off_t startoffset;
	int size;
	struct ucred *cred;
	int flags;
	struct buf **bpp;
{
	struct vop_balloc_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_balloc);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_balloc: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_startoffset = startoffset;
	a.a_size = size;
	a.a_cred = cred;
	a.a_flags = flags;
	a.a_bpp = bpp;
	return (VCALL(vp, VOFFSET(vop_balloc), &a));
}
#endif

const int vop_reallocblks_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_reallocblks_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_reallocblks_desc = {
	42,
	"vop_reallocblks",
	0,
	vop_reallocblks_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_REALLOCBLKS(vp, buflist)
	struct vnode *vp;
	struct cluster_save *buflist;
{
	struct vop_reallocblks_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_reallocblks);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_reallocblks: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_buflist = buflist;
	return (VCALL(vp, VOFFSET(vop_reallocblks), &a));
}
#endif

const int vop_vfree_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_vfree_args,a_pvp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_vfree_desc = {
	43,
	"vop_vfree",
	0,
	vop_vfree_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_VFREE(pvp, ino, mode)
	struct vnode *pvp;
	ino_t ino;
	int mode;
{
	struct vop_vfree_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_pvp;
#endif
	a.a_desc = VDESC(vop_vfree);
	a.a_pvp = pvp;
#ifdef VNODE_LOCKDEBUG
	islocked_pvp = (pvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(pvp) == LK_EXCLUSIVE) : 1;
	if (islocked_pvp != 1)
		panic("vop_vfree: pvp: locked %d, expected %d", islocked_pvp, 1);
#endif
	a.a_ino = ino;
	a.a_mode = mode;
	return (VCALL(pvp, VOFFSET(vop_vfree), &a));
}
#endif

const int vop_truncate_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_truncate_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_truncate_desc = {
	44,
	"vop_truncate",
	0,
	vop_truncate_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_truncate_args, a_cred),
	VOPARG_OFFSETOF(struct vop_truncate_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_TRUNCATE(vp, length, flags, cred, l)
	struct vnode *vp;
	off_t length;
	int flags;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_truncate_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_truncate);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_truncate: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_length = length;
	a.a_flags = flags;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_truncate), &a));
}
#endif

const int vop_update_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_update_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_update_desc = {
	45,
	"vop_update",
	0,
	vop_update_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_UPDATE(vp, access, modify, flags)
	struct vnode *vp;
	struct timespec *access;
	struct timespec *modify;
	int flags;
{
	struct vop_update_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_update);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_update: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_access = access;
	a.a_modify = modify;
	a.a_flags = flags;
	return (VCALL(vp, VOFFSET(vop_update), &a));
}
#endif

const int vop_lease_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_lease_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_lease_desc = {
	46,
	"vop_lease",
	0,
	vop_lease_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_lease_args, a_cred),
	VOPARG_OFFSETOF(struct vop_lease_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_LEASE(vp, l, cred, flag)
	struct vnode *vp;
	struct lwp *l;
	struct ucred *cred;
	int flag;
{
	struct vop_lease_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_lease);
	a.a_vp = vp;
	a.a_l = l;
	a.a_cred = cred;
	a.a_flag = flag;
	return (VCALL(vp, VOFFSET(vop_lease), &a));
}
#endif

const int vop_whiteout_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_whiteout_args,a_dvp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_whiteout_desc = {
	47,
	"vop_whiteout",
	0,
	vop_whiteout_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_whiteout_args, a_cnp),
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_WHITEOUT(dvp, cnp, flags)
	struct vnode *dvp;
	struct componentname *cnp;
	int flags;
{
	struct vop_whiteout_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_dvp;
#endif
	a.a_desc = VDESC(vop_whiteout);
	a.a_dvp = dvp;
#ifdef VNODE_LOCKDEBUG
	islocked_dvp = (dvp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(dvp) == LK_EXCLUSIVE) : 1;
	if (islocked_dvp != 1)
		panic("vop_whiteout: dvp: locked %d, expected %d", islocked_dvp, 1);
#endif
	a.a_cnp = cnp;
	a.a_flags = flags;
	return (VCALL(dvp, VOFFSET(vop_whiteout), &a));
}
#endif

const int vop_getpages_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_getpages_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_getpages_desc = {
	48,
	"vop_getpages",
	0,
	vop_getpages_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_GETPAGES(vp, offset, m, count, centeridx, access_type, advice, flags)
	struct vnode *vp;
	voff_t offset;
	struct vm_page **m;
	int *count;
	int centeridx;
	vm_prot_t access_type;
	int advice;
	int flags;
{
	struct vop_getpages_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_getpages);
	a.a_vp = vp;
	a.a_offset = offset;
	a.a_m = m;
	a.a_count = count;
	a.a_centeridx = centeridx;
	a.a_access_type = access_type;
	a.a_advice = advice;
	a.a_flags = flags;
	return (VCALL(vp, VOFFSET(vop_getpages), &a));
}
#endif

const int vop_putpages_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_putpages_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_putpages_desc = {
	49,
	"vop_putpages",
	0,
	vop_putpages_vp_offsets,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_PUTPAGES(vp, offlo, offhi, flags)
	struct vnode *vp;
	voff_t offlo;
	voff_t offhi;
	int flags;
{
	struct vop_putpages_args a;
#ifdef VNODE_LOCKDEBUG
#endif
	a.a_desc = VDESC(vop_putpages);
	a.a_vp = vp;
	a.a_offlo = offlo;
	a.a_offhi = offhi;
	a.a_flags = flags;
	return (VCALL(vp, VOFFSET(vop_putpages), &a));
}
#endif

const int vop_closeextattr_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_closeextattr_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_closeextattr_desc = {
	50,
	"vop_closeextattr",
	0,
	vop_closeextattr_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_closeextattr_args, a_cred),
	VOPARG_OFFSETOF(struct vop_closeextattr_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_CLOSEEXTATTR(vp, commit, cred, l)
	struct vnode *vp;
	int commit;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_closeextattr_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_closeextattr);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_closeextattr: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_commit = commit;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_closeextattr), &a));
}
#endif

const int vop_getextattr_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_getextattr_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_getextattr_desc = {
	51,
	"vop_getextattr",
	0,
	vop_getextattr_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_getextattr_args, a_cred),
	VOPARG_OFFSETOF(struct vop_getextattr_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_GETEXTATTR(vp, attrnamespace, name, uio, size, cred, l)
	struct vnode *vp;
	int attrnamespace;
	const char *name;
	struct uio *uio;
	size_t *size;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_getextattr_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_getextattr);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_getextattr: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_attrnamespace = attrnamespace;
	a.a_name = name;
	a.a_uio = uio;
	a.a_size = size;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_getextattr), &a));
}
#endif

const int vop_listextattr_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_listextattr_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_listextattr_desc = {
	52,
	"vop_listextattr",
	0,
	vop_listextattr_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_listextattr_args, a_cred),
	VOPARG_OFFSETOF(struct vop_listextattr_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_LISTEXTATTR(vp, attrnamespace, uio, size, cred, l)
	struct vnode *vp;
	int attrnamespace;
	struct uio *uio;
	size_t *size;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_listextattr_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_listextattr);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_listextattr: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_attrnamespace = attrnamespace;
	a.a_uio = uio;
	a.a_size = size;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_listextattr), &a));
}
#endif

const int vop_openextattr_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_openextattr_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_openextattr_desc = {
	53,
	"vop_openextattr",
	0,
	vop_openextattr_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_openextattr_args, a_cred),
	VOPARG_OFFSETOF(struct vop_openextattr_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_OPENEXTATTR(vp, cred, l)
	struct vnode *vp;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_openextattr_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_openextattr);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_openextattr: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_openextattr), &a));
}
#endif

const int vop_deleteextattr_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_deleteextattr_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_deleteextattr_desc = {
	54,
	"vop_deleteextattr",
	0,
	vop_deleteextattr_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_deleteextattr_args, a_cred),
	VOPARG_OFFSETOF(struct vop_deleteextattr_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_DELETEEXTATTR(vp, attrnamespace, name, cred, l)
	struct vnode *vp;
	int attrnamespace;
	const char *name;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_deleteextattr_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_deleteextattr);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_deleteextattr: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_attrnamespace = attrnamespace;
	a.a_name = name;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_deleteextattr), &a));
}
#endif

const int vop_setextattr_vp_offsets[] = {
	VOPARG_OFFSETOF(struct vop_setextattr_args,a_vp),
	VDESC_NO_OFFSET
};
const struct vnodeop_desc vop_setextattr_desc = {
	55,
	"vop_setextattr",
	0,
	vop_setextattr_vp_offsets,
	VDESC_NO_OFFSET,
	VOPARG_OFFSETOF(struct vop_setextattr_args, a_cred),
	VOPARG_OFFSETOF(struct vop_setextattr_args, a_l),
	VDESC_NO_OFFSET,
	NULL,
};
#ifdef VNODE_OP_NOINLINE
int
VOP_SETEXTATTR(vp, attrnamespace, name, uio, cred, l)
	struct vnode *vp;
	int attrnamespace;
	const char *name;
	struct uio *uio;
	struct ucred *cred;
	struct lwp *l;
{
	struct vop_setextattr_args a;
#ifdef VNODE_LOCKDEBUG
	int islocked_vp;
#endif
	a.a_desc = VDESC(vop_setextattr);
	a.a_vp = vp;
#ifdef VNODE_LOCKDEBUG
	islocked_vp = (vp->v_flag & VLOCKSWORK) ? (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) : 1;
	if (islocked_vp != 1)
		panic("vop_setextattr: vp: locked %d, expected %d", islocked_vp, 1);
#endif
	a.a_attrnamespace = attrnamespace;
	a.a_name = name;
	a.a_uio = uio;
	a.a_cred = cred;
	a.a_l = l;
	return (VCALL(vp, VOFFSET(vop_setextattr), &a));
}
#endif

/* End of special cases. */

const struct vnodeop_desc * const vfs_op_descs[] = {
	&vop_default_desc,	/* MUST BE FIRST */
	&vop_bwrite_desc,	/* XXX: SPECIAL CASE */

	&vop_lookup_desc,
	&vop_create_desc,
	&vop_mknod_desc,
	&vop_open_desc,
	&vop_close_desc,
	&vop_access_desc,
	&vop_getattr_desc,
	&vop_setattr_desc,
	&vop_read_desc,
	&vop_write_desc,
	&vop_ioctl_desc,
	&vop_fcntl_desc,
	&vop_poll_desc,
	&vop_kqfilter_desc,
	&vop_revoke_desc,
	&vop_mmap_desc,
	&vop_fsync_desc,
	&vop_seek_desc,
	&vop_remove_desc,
	&vop_link_desc,
	&vop_rename_desc,
	&vop_mkdir_desc,
	&vop_rmdir_desc,
	&vop_symlink_desc,
	&vop_readdir_desc,
	&vop_readlink_desc,
	&vop_abortop_desc,
	&vop_inactive_desc,
	&vop_reclaim_desc,
	&vop_lock_desc,
	&vop_unlock_desc,
	&vop_bmap_desc,
	&vop_strategy_desc,
	&vop_print_desc,
	&vop_islocked_desc,
	&vop_pathconf_desc,
	&vop_advlock_desc,
	&vop_blkatoff_desc,
	&vop_valloc_desc,
	&vop_balloc_desc,
	&vop_reallocblks_desc,
	&vop_vfree_desc,
	&vop_truncate_desc,
	&vop_update_desc,
	&vop_lease_desc,
	&vop_whiteout_desc,
	&vop_getpages_desc,
	&vop_putpages_desc,
	&vop_closeextattr_desc,
	&vop_getextattr_desc,
	&vop_listextattr_desc,
	&vop_openextattr_desc,
	&vop_deleteextattr_desc,
	&vop_setextattr_desc,
	NULL
};

