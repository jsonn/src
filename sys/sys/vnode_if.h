/*	$NetBSD: vnode_if.h,v 1.43.2.6 2005/01/17 19:33:10 skrll Exp $	*/

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

#ifndef _SYS_VNODE_IF_H_
#define _SYS_VNODE_IF_H_

#ifdef _KERNEL
#if defined(_LKM) || defined(LKM)
/* LKMs always use non-inlined vnode ops. */
#define	VNODE_OP_NOINLINE
#else
#include "opt_vnode_op_noinline.h"
#endif /* _LKM || LKM */
#ifdef _KERNEL_OPT
#include "opt_vnode_lockdebug.h"
#endif /* _KERNEL_OPT */
#endif /* _KERNEL */

extern const struct vnodeop_desc vop_default_desc;


struct vop_lookup_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
};
extern const struct vnodeop_desc vop_lookup_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_LOOKUP(struct vnode *, struct vnode **, struct componentname *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_LOOKUP(dvp, vpp, cnp)
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

struct vop_create_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern const struct vnodeop_desc vop_create_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_CREATE(struct vnode *, struct vnode **, struct componentname *, 
    struct vattr *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_CREATE(dvp, vpp, cnp, vap)
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

struct vop_mknod_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern const struct vnodeop_desc vop_mknod_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_MKNOD(struct vnode *, struct vnode **, struct componentname *, 
    struct vattr *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_MKNOD(dvp, vpp, cnp, vap)
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

struct vop_open_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_mode;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_open_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_OPEN(struct vnode *, int, struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_OPEN(vp, mode, cred, l)
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

struct vop_close_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_fflag;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_close_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_CLOSE(struct vnode *, int, struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_CLOSE(vp, fflag, cred, l)
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

struct vop_access_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_mode;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_access_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_ACCESS(struct vnode *, int, struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_ACCESS(vp, mode, cred, l)
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

struct vop_getattr_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct vattr *a_vap;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_getattr_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_GETATTR(struct vnode *, struct vattr *, struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_GETATTR(vp, vap, cred, l)
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

struct vop_setattr_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct vattr *a_vap;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_setattr_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_SETATTR(struct vnode *, struct vattr *, struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_SETATTR(vp, vap, cred, l)
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

struct vop_read_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	int a_ioflag;
	struct ucred *a_cred;
};
extern const struct vnodeop_desc vop_read_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_READ(struct vnode *, struct uio *, int, struct ucred *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_READ(vp, uio, ioflag, cred)
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

struct vop_write_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	int a_ioflag;
	struct ucred *a_cred;
};
extern const struct vnodeop_desc vop_write_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_WRITE(struct vnode *, struct uio *, int, struct ucred *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_WRITE(vp, uio, ioflag, cred)
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

struct vop_ioctl_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	u_long a_command;
	void *a_data;
	int a_fflag;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_ioctl_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_IOCTL(struct vnode *, u_long, void *, int, struct ucred *, 
    struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_IOCTL(vp, command, data, fflag, cred, l)
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

struct vop_fcntl_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	u_int a_command;
	void *a_data;
	int a_fflag;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_fcntl_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_FCNTL(struct vnode *, u_int, void *, int, struct ucred *, 
    struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_FCNTL(vp, command, data, fflag, cred, l)
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

struct vop_poll_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_events;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_poll_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_POLL(struct vnode *, int, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_POLL(vp, events, l)
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

struct vop_kqfilter_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct knote *a_kn;
};
extern const struct vnodeop_desc vop_kqfilter_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_KQFILTER(struct vnode *, struct knote *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_KQFILTER(vp, kn)
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

struct vop_revoke_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_flags;
};
extern const struct vnodeop_desc vop_revoke_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_REVOKE(struct vnode *, int)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_REVOKE(vp, flags)
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

struct vop_mmap_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_fflags;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_mmap_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_MMAP(struct vnode *, int, struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_MMAP(vp, fflags, cred, l)
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

struct vop_fsync_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct ucred *a_cred;
	int a_flags;
	off_t a_offlo;
	off_t a_offhi;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_fsync_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_FSYNC(struct vnode *, struct ucred *, int, off_t, off_t, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_FSYNC(vp, cred, flags, offlo, offhi, l)
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

struct vop_seek_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_oldoff;
	off_t a_newoff;
	struct ucred *a_cred;
};
extern const struct vnodeop_desc vop_seek_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_SEEK(struct vnode *, off_t, off_t, struct ucred *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_SEEK(vp, oldoff, newoff, cred)
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

struct vop_remove_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
extern const struct vnodeop_desc vop_remove_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_REMOVE(struct vnode *, struct vnode *, struct componentname *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_REMOVE(dvp, vp, cnp)
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

struct vop_link_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
extern const struct vnodeop_desc vop_link_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_LINK(struct vnode *, struct vnode *, struct componentname *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_LINK(dvp, vp, cnp)
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

struct vop_rename_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_fdvp;
	struct vnode *a_fvp;
	struct componentname *a_fcnp;
	struct vnode *a_tdvp;
	struct vnode *a_tvp;
	struct componentname *a_tcnp;
};
extern const struct vnodeop_desc vop_rename_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_RENAME(struct vnode *, struct vnode *, struct componentname *, 
    struct vnode *, struct vnode *, struct componentname *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_RENAME(fdvp, fvp, fcnp, tdvp, tvp, tcnp)
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

struct vop_mkdir_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
};
extern const struct vnodeop_desc vop_mkdir_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_MKDIR(struct vnode *, struct vnode **, struct componentname *, 
    struct vattr *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_MKDIR(dvp, vpp, cnp, vap)
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

struct vop_rmdir_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode *a_vp;
	struct componentname *a_cnp;
};
extern const struct vnodeop_desc vop_rmdir_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_RMDIR(struct vnode *, struct vnode *, struct componentname *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_RMDIR(dvp, vp, cnp)
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

struct vop_symlink_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct vnode **a_vpp;
	struct componentname *a_cnp;
	struct vattr *a_vap;
	char *a_target;
};
extern const struct vnodeop_desc vop_symlink_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_SYMLINK(struct vnode *, struct vnode **, struct componentname *, 
    struct vattr *, char *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_SYMLINK(dvp, vpp, cnp, vap, target)
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

struct vop_readdir_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	struct ucred *a_cred;
	int *a_eofflag;
	off_t **a_cookies;
	int *a_ncookies;
};
extern const struct vnodeop_desc vop_readdir_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_READDIR(struct vnode *, struct uio *, struct ucred *, int *, 
    off_t **, int *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_READDIR(vp, uio, cred, eofflag, cookies, ncookies)
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

struct vop_readlink_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct uio *a_uio;
	struct ucred *a_cred;
};
extern const struct vnodeop_desc vop_readlink_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_READLINK(struct vnode *, struct uio *, struct ucred *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_READLINK(vp, uio, cred)
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

struct vop_abortop_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
};
extern const struct vnodeop_desc vop_abortop_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_ABORTOP(struct vnode *, struct componentname *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_ABORTOP(dvp, cnp)
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

struct vop_inactive_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_inactive_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_INACTIVE(struct vnode *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_INACTIVE(vp, l)
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

struct vop_reclaim_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_reclaim_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_RECLAIM(struct vnode *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_RECLAIM(vp, l)
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

struct vop_lock_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_flags;
};
extern const struct vnodeop_desc vop_lock_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_LOCK(struct vnode *, int)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_LOCK(vp, flags)
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

struct vop_unlock_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_flags;
};
extern const struct vnodeop_desc vop_unlock_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_UNLOCK(struct vnode *, int)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_UNLOCK(vp, flags)
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

struct vop_bmap_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	daddr_t a_bn;
	struct vnode **a_vpp;
	daddr_t *a_bnp;
	int *a_runp;
};
extern const struct vnodeop_desc vop_bmap_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_BMAP(struct vnode *, daddr_t, struct vnode **, daddr_t *, int *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_BMAP(vp, bn, vpp, bnp, runp)
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

struct vop_strategy_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct buf *a_bp;
};
extern const struct vnodeop_desc vop_strategy_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_STRATEGY(struct vnode *, struct buf *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_STRATEGY(vp, bp)
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

struct vop_print_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
extern const struct vnodeop_desc vop_print_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_PRINT(struct vnode *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_PRINT(vp)
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

struct vop_islocked_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
};
extern const struct vnodeop_desc vop_islocked_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_ISLOCKED(struct vnode *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_ISLOCKED(vp)
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

struct vop_pathconf_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_name;
	register_t *a_retval;
};
extern const struct vnodeop_desc vop_pathconf_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_PATHCONF(struct vnode *, int, register_t *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_PATHCONF(vp, name, retval)
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

struct vop_advlock_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	void *a_id;
	int a_op;
	struct flock *a_fl;
	int a_flags;
};
extern const struct vnodeop_desc vop_advlock_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_ADVLOCK(struct vnode *, void *, int, struct flock *, int)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_ADVLOCK(vp, id, op, fl, flags)
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

struct vop_blkatoff_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_offset;
	char **a_res;
	struct buf **a_bpp;
};
extern const struct vnodeop_desc vop_blkatoff_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_BLKATOFF(struct vnode *, off_t, char **, struct buf **)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_BLKATOFF(vp, offset, res, bpp)
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

struct vop_valloc_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_pvp;
	int a_mode;
	struct ucred *a_cred;
	struct vnode **a_vpp;
};
extern const struct vnodeop_desc vop_valloc_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_VALLOC(struct vnode *, int, struct ucred *, struct vnode **)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_VALLOC(pvp, mode, cred, vpp)
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

struct vop_balloc_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_startoffset;
	int a_size;
	struct ucred *a_cred;
	int a_flags;
	struct buf **a_bpp;
};
extern const struct vnodeop_desc vop_balloc_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_BALLOC(struct vnode *, off_t, int, struct ucred *, int, struct buf **)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_BALLOC(vp, startoffset, size, cred, flags, bpp)
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

struct vop_reallocblks_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct cluster_save *a_buflist;
};
extern const struct vnodeop_desc vop_reallocblks_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_REALLOCBLKS(struct vnode *, struct cluster_save *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_REALLOCBLKS(vp, buflist)
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

struct vop_vfree_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_pvp;
	ino_t a_ino;
	int a_mode;
};
extern const struct vnodeop_desc vop_vfree_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_VFREE(struct vnode *, ino_t, int)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_VFREE(pvp, ino, mode)
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

struct vop_truncate_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	off_t a_length;
	int a_flags;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_truncate_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_TRUNCATE(struct vnode *, off_t, int, struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_TRUNCATE(vp, length, flags, cred, l)
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

struct vop_update_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct timespec *a_access;
	struct timespec *a_modify;
	int a_flags;
};
extern const struct vnodeop_desc vop_update_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_UPDATE(struct vnode *, struct timespec *, struct timespec *, int)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_UPDATE(vp, access, modify, flags)
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

struct vop_lease_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct lwp *a_l;
	struct ucred *a_cred;
	int a_flag;
};
extern const struct vnodeop_desc vop_lease_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_LEASE(struct vnode *, struct lwp *, struct ucred *, int)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_LEASE(vp, l, cred, flag)
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

struct vop_whiteout_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_dvp;
	struct componentname *a_cnp;
	int a_flags;
};
extern const struct vnodeop_desc vop_whiteout_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_WHITEOUT(struct vnode *, struct componentname *, int)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_WHITEOUT(dvp, cnp, flags)
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

struct vop_getpages_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	voff_t a_offset;
	struct vm_page **a_m;
	int *a_count;
	int a_centeridx;
	vm_prot_t a_access_type;
	int a_advice;
	int a_flags;
};
extern const struct vnodeop_desc vop_getpages_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_GETPAGES(struct vnode *, voff_t, struct vm_page **, int *, int, 
    vm_prot_t, int, int)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_GETPAGES(vp, offset, m, count, centeridx, access_type, advice, flags)
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

struct vop_putpages_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	voff_t a_offlo;
	voff_t a_offhi;
	int a_flags;
};
extern const struct vnodeop_desc vop_putpages_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_PUTPAGES(struct vnode *, voff_t, voff_t, int)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_PUTPAGES(vp, offlo, offhi, flags)
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

struct vop_closeextattr_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_commit;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_closeextattr_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_CLOSEEXTATTR(struct vnode *, int, struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_CLOSEEXTATTR(vp, commit, cred, l)
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

struct vop_getextattr_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_attrnamespace;
	const char *a_name;
	struct uio *a_uio;
	size_t *a_size;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_getextattr_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_GETEXTATTR(struct vnode *, int, const char *, struct uio *, 
    size_t *, struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_GETEXTATTR(vp, attrnamespace, name, uio, size, cred, l)
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

struct vop_listextattr_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_attrnamespace;
	struct uio *a_uio;
	size_t *a_size;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_listextattr_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_LISTEXTATTR(struct vnode *, int, struct uio *, size_t *, 
    struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_LISTEXTATTR(vp, attrnamespace, uio, size, cred, l)
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

struct vop_openextattr_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_openextattr_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_OPENEXTATTR(struct vnode *, struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_OPENEXTATTR(vp, cred, l)
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

struct vop_deleteextattr_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_attrnamespace;
	const char *a_name;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_deleteextattr_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_DELETEEXTATTR(struct vnode *, int, const char *, struct ucred *, 
    struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_DELETEEXTATTR(vp, attrnamespace, name, cred, l)
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

struct vop_setextattr_args {
	const struct vnodeop_desc *a_desc;
	struct vnode *a_vp;
	int a_attrnamespace;
	const char *a_name;
	struct uio *a_uio;
	struct ucred *a_cred;
	struct lwp *a_l;
};
extern const struct vnodeop_desc vop_setextattr_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_SETEXTATTR(struct vnode *, int, const char *, struct uio *, 
    struct ucred *, struct lwp *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_SETEXTATTR(vp, attrnamespace, name, uio, cred, l)
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

/* Special cases: */
#include <sys/buf.h>

struct vop_bwrite_args {
	const struct vnodeop_desc *a_desc;
	struct buf *a_bp;
};
extern const struct vnodeop_desc vop_bwrite_desc;
#ifndef VNODE_OP_NOINLINE
static __inline
#endif
int VOP_BWRITE(struct buf *)
#ifndef VNODE_OP_NOINLINE
__attribute__((__unused__))
#endif
;
#ifndef VNODE_OP_NOINLINE
static __inline int VOP_BWRITE(bp)
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

#define VNODE_OPS_COUNT	56

/* End of special cases. */

#endif /* !_SYS_VNODE_IF_H_ */
