/*	$NetBSD: ufs_vnops.c,v 1.28.4.1 1997/10/14 16:06:30 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ufs_vnops.c	8.14 (Berkeley) 10/26/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/lockf.h>

#include <vm/vm.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ext2fs/ext2fs_extern.h>

static int ufs_chmod __P((struct vnode *, int, struct ucred *, struct proc *));
static int ufs_chown
	__P((struct vnode *, uid_t, gid_t, struct ucred *, struct proc *));

union _qcvt {
	int64_t	qcvt;
	int32_t val[2];
};
#define SETHIGH(q, h) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_HIGHWORD] = (h); \
	(q) = tmp.qcvt; \
}
#define SETLOW(q, l) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_LOWWORD] = (l); \
	(q) = tmp.qcvt; \
}

/*
 * Create a regular file
 */
int
ufs_create(v)
	void *v;
{
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	return
	    ufs_makeinode(MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode),
			  ap->a_dvp, ap->a_vpp, ap->a_cnp);
}

/*
 * Mknod vnode call
 */
/* ARGSUSED */
int
ufs_mknod(v)
	void *v;
{
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	register struct vattr *vap = ap->a_vap;
	register struct vnode **vpp = ap->a_vpp;
	register struct inode *ip;
	int error;

	if ((error =
	    ufs_makeinode(MAKEIMODE(vap->va_type, vap->va_mode),
	    ap->a_dvp, vpp, ap->a_cnp)) != 0)
		return (error);
	ip = VTOI(*vpp);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	if (vap->va_rdev != VNOVAL) {
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
		ip->i_ffs_rdev = vap->va_rdev;
	}
	/*
	 * Remove inode so that it will be reloaded by VFS_VGET and
	 * checked to see if it is an alias of an existing entry in
	 * the inode cache.
	 */
	vput(*vpp);
	(*vpp)->v_type = VNON;
	vgone(*vpp);
	*vpp = 0;
	return (0);
}

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
int
ufs_open(v)
	void *v;
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;

	/*
	 * Files marked append-only must be opened for appending.
	 */
	if ((VTOI(ap->a_vp)->i_ffs_flags & APPEND) &&
	    (ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		return (EPERM);
	return (0);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
/* ARGSUSED */
int
ufs_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip = VTOI(vp);
	struct timespec ts;

	if (vp->v_usecount > 1 && !(ip->i_flag & IN_LOCKED)) {
		TIMEVAL_TO_TIMESPEC(&time, &ts);
		ITIMES(ip, &ts, &ts, &ts);
	}
	return (0);
}

int
ufs_access(v)
	void *v;
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip = VTOI(vp);
	mode_t mode = ap->a_mode;

#ifdef DIAGNOSTIC
	if (!VOP_ISLOCKED(vp)) {
		vprint("ufs_access: not locked", vp);
		panic("ufs_access: not locked");
	}
#endif
#ifdef QUOTA
	if (mode & VWRITE)
		switch (vp->v_type) {
			int error;
		case VDIR:
		case VLNK:
		case VREG:
			if ((error = getinoquota(ip)) != 0)
				return (error);
			break;
		case VBAD:
		case VBLK:
		case VCHR:
		case VSOCK:
		case VFIFO:
		case VNON:
			break;
		}
#endif

	/* If immutable bit set, nobody gets to write it. */
	if ((mode & VWRITE) && (ip->i_ffs_flags & IMMUTABLE))
		return (EPERM);

	return (vaccess(vp->v_type, ip->i_ffs_mode & ALLPERMS,
		ip->i_ffs_uid, ip->i_ffs_gid, mode, ap->a_cred));
}

/* ARGSUSED */
int
ufs_getattr(v)
	void *v;
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip = VTOI(vp);
	register struct vattr *vap = ap->a_vap;
	struct timespec ts;

	TIMEVAL_TO_TIMESPEC(&time, &ts);
	FFS_ITIMES(ip, &ts, &ts, &ts);
	/*
	 * Copy from inode table
	 */
	vap->va_fsid = ip->i_dev;
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_ffs_mode & ALLPERMS;
	vap->va_nlink = ip->i_ffs_nlink;
	vap->va_uid = ip->i_ffs_uid;
	vap->va_gid = ip->i_ffs_gid;
	vap->va_rdev = (dev_t)ip->i_ffs_rdev;
	vap->va_size = ip->i_ffs_size;
	vap->va_atime.tv_sec = ip->i_ffs_atime;
	vap->va_atime.tv_nsec = ip->i_ffs_atimensec;
	vap->va_mtime.tv_sec = ip->i_ffs_mtime;
	vap->va_mtime.tv_nsec = ip->i_ffs_mtimensec;
	vap->va_ctime.tv_sec = ip->i_ffs_ctime;
	vap->va_ctime.tv_nsec = ip->i_ffs_ctimensec;
	vap->va_flags = ip->i_ffs_flags;
	vap->va_gen = ip->i_ffs_gen;
	/* this doesn't belong here */
	if (vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;
	else
		vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_bytes = dbtob(ip->i_ffs_blocks);
	vap->va_type = vp->v_type;
	vap->va_filerev = ip->i_modrev;
	return (0);
}

/*
 * Set attribute vnode op. called from several syscalls
 */
int
ufs_setattr(v)
	void *v;
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	register struct vattr *vap = ap->a_vap;
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip = VTOI(vp);
	register struct ucred *cred = ap->a_cred;
	register struct proc *p = ap->a_p;
	int error;

	/*
	 * Check for unsettable attributes.
	 */
	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL)) {
		return (EINVAL);
	}
	if (vap->va_flags != VNOVAL) {
		if (cred->cr_uid != ip->i_ffs_uid &&
		    (error = suser(cred, &p->p_acflag)))
			return (error);
		if (cred->cr_uid == 0) {
			if ((ip->i_ffs_flags & (SF_IMMUTABLE | SF_APPEND)) &&
			    securelevel > 0)
				return (EPERM);
			ip->i_ffs_flags = vap->va_flags;
		} else {
			if (ip->i_ffs_flags & (SF_IMMUTABLE | SF_APPEND))
				return (EPERM);
			if ((ip->i_ffs_flags & SF_SETTABLE) !=
			    (vap->va_flags & SF_SETTABLE))
				return (EPERM);
			ip->i_ffs_flags &= SF_SETTABLE;
			ip->i_ffs_flags |= (vap->va_flags & UF_SETTABLE);
		}
		ip->i_flag |= IN_CHANGE;
		if (vap->va_flags & (IMMUTABLE | APPEND))
			return (0);
	}
	if (ip->i_ffs_flags & (IMMUTABLE | APPEND))
		return (EPERM);
	/*
	 * Go through the fields and update iff not VNOVAL.
	 */
	if (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL) {
		error = ufs_chown(vp, vap->va_uid, vap->va_gid, cred, p);
		if (error)
			return (error);
	}
	if (vap->va_size != VNOVAL) {
		if (vp->v_type == VDIR)
			return (EISDIR);
		error = VOP_TRUNCATE(vp, vap->va_size, 0, cred, p);
		if (error)
			return (error);
	}
	ip = VTOI(vp);
	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL) {
		if (cred->cr_uid != ip->i_ffs_uid &&
		    (error = suser(cred, &p->p_acflag)) &&
		    ((vap->va_vaflags & VA_UTIMES_NULL) == 0 || 
		    (error = VOP_ACCESS(vp, VWRITE, cred, p))))
			return (error);
		if (vap->va_atime.tv_sec != VNOVAL)
			if (!(vp->v_mount->mnt_flag & MNT_NOATIME))
				ip->i_flag |= IN_ACCESS;
		if (vap->va_mtime.tv_sec != VNOVAL)
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
		error = VOP_UPDATE(vp, &vap->va_atime, &vap->va_mtime, 1);
		if (error)
			return (error);
	}
	error = 0;
	if (vap->va_mode != (mode_t)VNOVAL)
		error = ufs_chmod(vp, (int)vap->va_mode, cred, p);
	return (error);
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
static int
ufs_chmod(vp, mode, cred, p)
	register struct vnode *vp;
	register int mode;
	register struct ucred *cred;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);
	int error;

	if (cred->cr_uid != ip->i_ffs_uid &&
	    (error = suser(cred, &p->p_acflag)))
		return (error);
	if (cred->cr_uid) {
		if (vp->v_type != VDIR && (mode & S_ISTXT))
			return (EFTYPE);
		if (!groupmember(ip->i_ffs_gid, cred) && (mode & ISGID))
			return (EPERM);
	}
	ip->i_ffs_mode &= ~ALLPERMS;
	ip->i_ffs_mode |= (mode & ALLPERMS);
	ip->i_flag |= IN_CHANGE;
	if ((vp->v_flag & VTEXT) && (ip->i_ffs_mode & S_ISTXT) == 0)
		(void) vnode_pager_uncache(vp);
	return (0);
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
static int
ufs_chown(vp, uid, gid, cred, p)
	register struct vnode *vp;
	uid_t uid;
	gid_t gid;
	struct ucred *cred;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);
	uid_t ouid;
	gid_t ogid;
	int error = 0;
#ifdef QUOTA
	register int i;
	long change;
#endif

	if (uid == (uid_t)VNOVAL)
		uid = ip->i_ffs_uid;
	if (gid == (gid_t)VNOVAL)
		gid = ip->i_ffs_gid;
	/*
	 * If we don't own the file, are trying to change the owner
	 * of the file, or are not a member of the target group,
	 * the caller must be superuser or the call fails.
	 */
	if ((cred->cr_uid != ip->i_ffs_uid || uid != ip->i_ffs_uid ||
	    (gid != ip->i_ffs_gid && !groupmember((gid_t)gid, cred))) &&
	    (error = suser(cred, &p->p_acflag)))
		return (error);
	ogid = ip->i_ffs_gid;
	ouid = ip->i_ffs_uid;
#ifdef QUOTA
	if ((error = getinoquota(ip)) != 0)
		return (error);
	if (ouid == uid) {
		dqrele(vp, ip->i_dquot[USRQUOTA]);
		ip->i_dquot[USRQUOTA] = NODQUOT;
	}
	if (ogid == gid) {
		dqrele(vp, ip->i_dquot[GRPQUOTA]);
		ip->i_dquot[GRPQUOTA] = NODQUOT;
	}
	change = ip->i_ffs_blocks;
	(void) chkdq(ip, -change, cred, CHOWN);
	(void) chkiq(ip, -1, cred, CHOWN);
	for (i = 0; i < MAXQUOTAS; i++) {
		dqrele(vp, ip->i_dquot[i]);
		ip->i_dquot[i] = NODQUOT;
	}
#endif
	ip->i_ffs_gid = gid;
	ip->i_ffs_uid = uid;
#ifdef QUOTA
	if ((error = getinoquota(ip)) == 0) {
		if (ouid == uid) {
			dqrele(vp, ip->i_dquot[USRQUOTA]);
			ip->i_dquot[USRQUOTA] = NODQUOT;
		}
		if (ogid == gid) {
			dqrele(vp, ip->i_dquot[GRPQUOTA]);
			ip->i_dquot[GRPQUOTA] = NODQUOT;
		}
		if ((error = chkdq(ip, change, cred, CHOWN)) == 0) {
			if ((error = chkiq(ip, 1, cred, CHOWN)) == 0)
				goto good;
			else
				(void) chkdq(ip, -change, cred, CHOWN|FORCE);
		}
		for (i = 0; i < MAXQUOTAS; i++) {
			dqrele(vp, ip->i_dquot[i]);
			ip->i_dquot[i] = NODQUOT;
		}
	}
	ip->i_ffs_gid = ogid;
	ip->i_ffs_uid = ouid;
	if (getinoquota(ip) == 0) {
		if (ouid == uid) {
			dqrele(vp, ip->i_dquot[USRQUOTA]);
			ip->i_dquot[USRQUOTA] = NODQUOT;
		}
		if (ogid == gid) {
			dqrele(vp, ip->i_dquot[GRPQUOTA]);
			ip->i_dquot[GRPQUOTA] = NODQUOT;
		}
		(void) chkdq(ip, change, cred, FORCE|CHOWN);
		(void) chkiq(ip, 1, cred, FORCE|CHOWN);
		(void) getinoquota(ip);
	}
	return (error);
good:
	if (getinoquota(ip))
		panic("chown: lost quota");
#endif /* QUOTA */
	if (ouid != uid || ogid != gid)
		ip->i_flag |= IN_CHANGE;
	if (ouid != uid && cred->cr_uid != 0)
		ip->i_ffs_mode &= ~ISUID;
	if (ogid != gid && cred->cr_uid != 0)
		ip->i_ffs_mode &= ~ISGID;
	return (0);
}

/* ARGSUSED */
int
ufs_ioctl(v)
	void *v;
{
#if 0
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
#endif
	return (ENOTTY);
}

/*
 * Mmap a file
 *
 * NB Currently unsupported.
 */
/* ARGSUSED */
int
ufs_mmap(v)
	void *v;
{
#if 0
	struct vop_mmap_args /* {
		struct vnode *a_vp;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
#endif

	return (EINVAL);
}

int
ufs_remove(v)
	void *v;
{
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	register struct inode *ip;
	register struct vnode *vp = ap->a_vp;
	register struct vnode *dvp = ap->a_dvp;
	int error;

	ip = VTOI(vp);
	if (vp->v_type == VDIR || (ip->i_ffs_flags & (IMMUTABLE | APPEND)) ||
	    (VTOI(dvp)->i_ffs_flags & APPEND)) {
		error = EPERM;
		goto out;
	}
    error = ufs_dirremove(dvp, ap->a_cnp);
    if (error == 0) {
		ip->i_ffs_nlink--;
		ip->i_flag |= IN_CHANGE;
	}
out:
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);
	return (error);
}

/*
 * link vnode call
 */
int
ufs_link(v)
	void *v;
{
	struct vop_link_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	register struct vnode *dvp = ap->a_dvp;
	register struct vnode *vp = ap->a_vp;
	register struct componentname *cnp = ap->a_cnp;
	register struct inode *ip;
	struct timespec ts;
	int error;

#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ufs_link: no name");
#endif
	if (vp->v_type == VDIR) {
		VOP_ABORTOP(dvp, cnp);
		error = EPERM;
		goto out2;
	}
	if (dvp->v_mount != vp->v_mount) {
		VOP_ABORTOP(dvp, cnp);
		error = EXDEV;
		goto out2;
	}
	if (dvp != vp && (error = VOP_LOCK(vp))) {
		VOP_ABORTOP(dvp, cnp);
		goto out2;
	}
	ip = VTOI(vp);
	if ((nlink_t)ip->i_ffs_nlink >= LINK_MAX) {
		VOP_ABORTOP(dvp, cnp);
		error = EMLINK;
		goto out1;
	}
	if (ip->i_ffs_flags & (IMMUTABLE | APPEND)) {
		VOP_ABORTOP(dvp, cnp);
		error = EPERM;
		goto out1;
	}
	ip->i_ffs_nlink++;
	ip->i_flag |= IN_CHANGE;
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	error = VOP_UPDATE(vp, &ts, &ts, 1);
	if (!error)
		error = ufs_direnter(ip, dvp, cnp);
	if (error) {
		ip->i_ffs_nlink--;
		ip->i_flag |= IN_CHANGE;
	}
	FREE(cnp->cn_pnbuf, M_NAMEI);
out1:
	if (dvp != vp)
		VOP_UNLOCK(vp);
out2:
	vput(dvp);
	return (error);
}

/*
 * whiteout vnode call
 */
int
ufs_whiteout(v)
	void *v;
{
	struct vop_whiteout_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
		int a_flags;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct direct newdir;
	int error = 0;

	switch (ap->a_flags) {
	case LOOKUP:
		/* 4.4 format directories support whiteout operations */
		if (dvp->v_mount->mnt_maxsymlinklen > 0)
			return (0);
		return (EOPNOTSUPP);

	case CREATE:
		/* create a new directory whiteout */
#ifdef DIAGNOSTIC
		if ((cnp->cn_flags & SAVENAME) == 0)
			panic("ufs_whiteout: missing name");
		if (dvp->v_mount->mnt_maxsymlinklen <= 0)
			panic("ufs_whiteout: old format filesystem");
#endif

		newdir.d_ino = WINO;
		newdir.d_namlen = cnp->cn_namelen;
		bcopy(cnp->cn_nameptr, newdir.d_name, (unsigned)cnp->cn_namelen + 1);
		newdir.d_type = DT_WHT;
		error = ufs_direnter2(dvp, &newdir, cnp->cn_cred, cnp->cn_proc);
		break;

	case DELETE:
		/* remove an existing directory whiteout */
#ifdef DIAGNOSTIC
		if (dvp->v_mount->mnt_maxsymlinklen <= 0)
			panic("ufs_whiteout: old format filesystem");
#endif

		cnp->cn_flags &= ~DOWHITEOUT;
		error = ufs_dirremove(dvp, cnp);
		break;
	}
	if (cnp->cn_flags & HASBUF) {
		FREE(cnp->cn_pnbuf, M_NAMEI);
		cnp->cn_flags &= ~HASBUF;
	}
	return (error);
}


/*
 * Rename system call.
 * 	rename("foo", "bar");
 * is essentially
 *	unlink("bar");
 *	link("foo", "bar");
 *	unlink("foo");
 * but ``atomically''.  Can't do full commit without saving state in the
 * inode on disk which isn't feasible at this time.  Best we can do is
 * always guarantee the target exists.
 *
 * Basic algorithm is:
 *
 * 1) Bump link count on source while we're linking it to the
 *    target.  This also ensure the inode won't be deleted out
 *    from underneath us while we work (it may be truncated by
 *    a concurrent `trunc' or `open' for creation).
 * 2) Link source to destination.  If destination already exists,
 *    delete it first.
 * 3) Unlink source reference to inode if still around. If a
 *    directory was moved and the parent of the destination
 *    is different from the source, patch the ".." entry in the
 *    directory.
 */
int
ufs_rename(v)
	void *v;
{
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	struct vnode *tvp = ap->a_tvp;
	register struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	register struct vnode *fdvp = ap->a_fdvp;
	register struct componentname *tcnp = ap->a_tcnp;
	register struct componentname *fcnp = ap->a_fcnp;
	register struct inode *ip, *xp, *dp;
	struct dirtemplate dirbuf;
	struct timespec ts;
	int doingdirectory = 0, oldparent = 0, newparent = 0;
	int error = 0;
	u_char namlen;

#ifdef DIAGNOSTIC
	if ((tcnp->cn_flags & HASBUF) == 0 ||
	    (fcnp->cn_flags & HASBUF) == 0)
		panic("ufs_rename: no name");
#endif
	/*
	 * Check for cross-device rename.
	 */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
abortit:
		VOP_ABORTOP(tdvp, tcnp); /* XXX, why not in NFS? */
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(fdvp, fcnp); /* XXX, why not in NFS? */
		vrele(fdvp);
		vrele(fvp);
		return (error);
	}

	/*
	 * Check if just deleting a link name.
	 */
	if (tvp && ((VTOI(tvp)->i_ffs_flags & (IMMUTABLE | APPEND)) ||
	    (VTOI(tdvp)->i_ffs_flags & APPEND))) {
		error = EPERM;
		goto abortit;
	}
	if (fvp == tvp) {
		if (fvp->v_type == VDIR) {
			error = EINVAL;
			goto abortit;
		}

		/* Release destination completely. */
		VOP_ABORTOP(tdvp, tcnp);
		vput(tdvp);
		vput(tvp);

		/* Delete source. */
		vrele(fdvp);
		vrele(fvp);
		fcnp->cn_flags &= ~MODMASK;
		fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
		if ((fcnp->cn_flags & SAVESTART) == 0)
			panic("ufs_rename: lost from startdir");
		fcnp->cn_nameiop = DELETE;
		(void) relookup(fdvp, &fvp, fcnp);
		return (VOP_REMOVE(fdvp, fvp, fcnp));
	}
	if ((error = VOP_LOCK(fvp)) != 0)
		goto abortit;
	dp = VTOI(fdvp);
	ip = VTOI(fvp);
	if ((ip->i_ffs_flags & (IMMUTABLE | APPEND)) ||
		(dp->i_ffs_flags & APPEND)) {
		VOP_UNLOCK(fvp);
		error = EPERM;
		goto abortit;
	}
	if ((ip->i_ffs_mode & IFMT) == IFDIR) {
		/*
		 * Avoid ".", "..", and aliases of "." for obvious reasons.
		 */
		if ((fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.') ||
		    dp == ip ||
		    (fcnp->cn_flags & ISDOTDOT) ||
		    (tcnp->cn_flags & ISDOTDOT) ||
		    (ip->i_flag & IN_RENAME)) {
			VOP_UNLOCK(fvp);
			error = EINVAL;
			goto abortit;
		}
		ip->i_flag |= IN_RENAME;
		oldparent = dp->i_number;
		doingdirectory++;
	}
	vrele(fdvp);

	/*
	 * When the target exists, both the directory
	 * and target vnodes are returned locked.
	 */
	dp = VTOI(tdvp);
	xp = NULL;
	if (tvp)
		xp = VTOI(tvp);

	/*
	 * 1) Bump link count while we're moving stuff
	 *    around.  If we crash somewhere before
	 *    completing our work, the link count
	 *    may be wrong, but correctable.
	 */
	ip->i_ffs_nlink++;
	ip->i_flag |= IN_CHANGE;
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	if ((error = VOP_UPDATE(fvp, &ts, &ts, 1)) != 0) {
		VOP_UNLOCK(fvp);
		goto bad;
	}

	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) then the source directory must not be in the
	 * directory heirarchy above the target, as this would
	 * orphan everything below the source directory. Also
	 * the user must have write permission in the source so
	 * as to be able to change "..". We must repeat the call 
	 * to namei, as the parent directory is unlocked by the
	 * call to checkpath().
	 */
	error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred, tcnp->cn_proc);
	VOP_UNLOCK(fvp);
	if (oldparent != dp->i_number)
		newparent = dp->i_number;
	if (doingdirectory && newparent) {
		if (error)	/* write access check above */
			goto bad;
		if (xp != NULL)
			vput(tvp);
		if ((error = ufs_checkpath(ip, dp, tcnp->cn_cred)) != 0)
			goto out;
		if ((tcnp->cn_flags & SAVESTART) == 0)
			panic("ufs_rename: lost to startdir");
		if ((error = relookup(tdvp, &tvp, tcnp)) != 0)
			goto out;
		dp = VTOI(tdvp);
		xp = NULL;
		if (tvp)
			xp = VTOI(tvp);
	}
	/*
	 * 2) If target doesn't exist, link the target
	 *    to the source and unlink the source. 
	 *    Otherwise, rewrite the target directory
	 *    entry to reference the source inode and
	 *    expunge the original entry's existence.
	 */
	if (xp == NULL) {
		if (dp->i_dev != ip->i_dev)
			panic("rename: EXDEV");
		/*
		 * Account for ".." in new directory.
		 * When source and destination have the same
		 * parent we don't fool with the link count.
		 */
		if (doingdirectory && newparent) {
			if ((nlink_t)dp->i_ffs_nlink >= LINK_MAX) {
				error = EMLINK;
				goto bad;
			}
			dp->i_ffs_nlink++;
			dp->i_flag |= IN_CHANGE;
			if ((error = VOP_UPDATE(tdvp, &ts, &ts, 1)) != 0)
				goto bad;
		}
		if ((error = ufs_direnter(ip, tdvp, tcnp)) != 0) {
			if (doingdirectory && newparent) {
				dp->i_ffs_nlink--;
				dp->i_flag |= IN_CHANGE;
				(void)VOP_UPDATE(tdvp, &ts, &ts, 1);
			}
			goto bad;
		}
		vput(tdvp);
	} else {
		if (xp->i_dev != dp->i_dev || xp->i_dev != ip->i_dev)
			panic("rename: EXDEV");
		/*
		 * Short circuit rename(foo, foo).
		 */
		if (xp->i_number == ip->i_number)
			panic("rename: same file");
		/*
		 * If the parent directory is "sticky", then the user must
		 * own the parent directory, or the destination of the rename,
		 * otherwise the destination may not be changed (except by
		 * root). This implements append-only directories.
		 */
		if ((dp->i_ffs_mode & S_ISTXT) && tcnp->cn_cred->cr_uid != 0 &&
		    tcnp->cn_cred->cr_uid != dp->i_ffs_uid &&
		    xp->i_ffs_uid != tcnp->cn_cred->cr_uid) {
			error = EPERM;
			goto bad;
		}
		/*
		 * Target must be empty if a directory and have no links
		 * to it. Also, ensure source and target are compatible
		 * (both directories, or both not directories).
		 */
		if ((xp->i_ffs_mode & IFMT) == IFDIR) {
			if (!ufs_dirempty(xp, dp->i_number, tcnp->cn_cred) || 
				xp->i_ffs_nlink > 2) {
				error = ENOTEMPTY;
				goto bad;
			}
			if (!doingdirectory) {
				error = ENOTDIR;
				goto bad;
			}
			cache_purge(tdvp);
		} else if (doingdirectory) {
			error = EISDIR;
			goto bad;
		}
		if ((error = ufs_dirrewrite(dp, ip, tcnp)) != 0)
			goto bad;
		/*
		 * If the target directory is in the same
		 * directory as the source directory,
		 * decrement the link count on the parent
		 * of the target directory.
		 */
		 if (doingdirectory && !newparent) {
			dp->i_ffs_nlink--;
			dp->i_flag |= IN_CHANGE;
		}
		vput(tdvp);
		/*
		 * Adjust the link count of the target to
		 * reflect the dirrewrite above.  If this is
		 * a directory it is empty and there are
		 * no links to it, so we can squash the inode and
		 * any space associated with it.  We disallowed
		 * renaming over top of a directory with links to
		 * it above, as the remaining link would point to
		 * a directory without "." or ".." entries.
		 */
		xp->i_ffs_nlink--;
		if (doingdirectory) {
			if (--xp->i_ffs_nlink != 0)
				panic("rename: linked directory");
			error = VOP_TRUNCATE(tvp, (off_t)0, IO_SYNC,
			    tcnp->cn_cred, tcnp->cn_proc);
		}
		xp->i_flag |= IN_CHANGE;
		vput(tvp);
		xp = NULL;
	}

	/*
	 * 3) Unlink the source.
	 */
	fcnp->cn_flags &= ~MODMASK;
	fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
	if ((fcnp->cn_flags & SAVESTART) == 0)
		panic("ufs_rename: lost from startdir");
	(void) relookup(fdvp, &fvp, fcnp);
	if (fvp != NULL) {
		xp = VTOI(fvp);
		dp = VTOI(fdvp);
	} else {
		/*
		 * From name has disappeared.
		 */
		if (doingdirectory)
			panic("rename: lost dir entry");
		vrele(ap->a_fvp);
		return (0);
	}
	/*
	 * Ensure that the directory entry still exists and has not
	 * changed while the new name has been entered. If the source is
	 * a file then the entry may have been unlinked or renamed. In
	 * either case there is no further work to be done. If the source
	 * is a directory then it cannot have been rmdir'ed; its link
	 * count of three would cause a rmdir to fail with ENOTEMPTY.
	 * The IRENAME flag ensures that it cannot be moved by another
	 * rename.
	 */
	if (xp != ip) {
		if (doingdirectory)
			panic("rename: lost dir entry");
	} else {
		/*
		 * If the source is a directory with a
		 * new parent, the link count of the old
		 * parent directory must be decremented
		 * and ".." set to point to the new parent.
		 */
		if (doingdirectory && newparent) {
			dp->i_ffs_nlink--;
			dp->i_flag |= IN_CHANGE;
			error = vn_rdwr(UIO_READ, fvp, (caddr_t)&dirbuf,
				sizeof (struct dirtemplate), (off_t)0,
				UIO_SYSSPACE, IO_NODELOCKED, 
				tcnp->cn_cred, (int *)0, (struct proc *)0);
			if (error == 0) {
#				if (BYTE_ORDER == LITTLE_ENDIAN)
					if (fvp->v_mount->mnt_maxsymlinklen <= 0)
						namlen = dirbuf.dotdot_type;
					else
						namlen = dirbuf.dotdot_namlen;
#				else
					namlen = dirbuf.dotdot_namlen;
#				endif
				if (namlen != 2 ||
				    dirbuf.dotdot_name[0] != '.' ||
				    dirbuf.dotdot_name[1] != '.') {
					ufs_dirbad(xp, (doff_t)12,
					    "rename: mangled dir");
				} else {
					dirbuf.dotdot_ino = newparent;
					(void) vn_rdwr(UIO_WRITE, fvp,
					    (caddr_t)&dirbuf,
					    sizeof (struct dirtemplate),
					    (off_t)0, UIO_SYSSPACE,
					    IO_NODELOCKED|IO_SYNC,
					    tcnp->cn_cred, (int *)0,
					    (struct proc *)0);
					cache_purge(fdvp);
				}
			}
		}
		error = ufs_dirremove(fdvp, fcnp);
		if (!error) {
			xp->i_ffs_nlink--;
			xp->i_flag |= IN_CHANGE;
		}
		xp->i_flag &= ~IN_RENAME;
	}
	if (dp)
		vput(fdvp);
	if (xp)
		vput(fvp);
	vrele(ap->a_fvp);
	return (error);

bad:
	if (xp)
		vput(ITOV(xp));
	vput(ITOV(dp));
out:
	if (doingdirectory)
		ip->i_flag &= ~IN_RENAME;
	if (VOP_LOCK(fvp) == 0) {
		ip->i_ffs_nlink--;
		ip->i_flag |= IN_CHANGE;
		vput(fvp);
	} else
		vrele(fvp);
	return (error);
}

/*
 * A virgin directory (no blushing please).
 */
static struct dirtemplate mastertemplate = {
	0, 12, DT_DIR, 1, ".",
	0, DIRBLKSIZ - 12, DT_DIR, 2, ".."
};
static struct odirtemplate omastertemplate = {
	0, 12, 1, ".",
	0, DIRBLKSIZ - 12, 2, ".."
};

/*
 * Mkdir system call
 */
int
ufs_mkdir(v)
	void *v;
{
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	register struct vnode *dvp = ap->a_dvp;
	register struct vattr *vap = ap->a_vap;
	register struct componentname *cnp = ap->a_cnp;
	register struct inode *ip, *dp;
	struct vnode *tvp;
	struct dirtemplate dirtemplate, *dtp;
	struct timespec ts;
	int error, dmode;

#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ufs_mkdir: no name");
#endif
	dp = VTOI(dvp);
	if ((nlink_t)dp->i_ffs_nlink >= LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	dmode = vap->va_mode & ACCESSPERMS;
	dmode |= IFDIR;
	/*
	 * Must simulate part of ufs_makeinode here to acquire the inode,
	 * but not have it entered in the parent directory. The entry is
	 * made later after writing "." and ".." entries.
	 */
	if ((error = VOP_VALLOC(dvp, dmode, cnp->cn_cred, &tvp)) != 0)
		goto out;
	ip = VTOI(tvp);
	ip->i_ffs_uid = cnp->cn_cred->cr_uid;
	ip->i_ffs_gid = dp->i_ffs_gid;
#ifdef QUOTA
	if ((error = getinoquota(ip)) ||
	    (error = chkiq(ip, 1, cnp->cn_cred, 0))) {
		free(cnp->cn_pnbuf, M_NAMEI);
		VOP_VFREE(tvp, ip->i_number, dmode);
		vput(tvp);
		vput(dvp);
		return (error);
	}
#endif
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_ffs_mode = dmode;
	tvp->v_type = VDIR;	/* Rest init'd in getnewvnode(). */
	ip->i_ffs_nlink = 2;
	if (cnp->cn_flags & ISWHITEOUT)
		ip->i_ffs_flags |= UF_OPAQUE;
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	error = VOP_UPDATE(tvp, &ts, &ts, 1);

	/*
	 * Bump link count in parent directory
	 * to reflect work done below.  Should
	 * be done before reference is created
	 * so reparation is possible if we crash.
	 */
	dp->i_ffs_nlink++;
	dp->i_flag |= IN_CHANGE;
	if ((error = VOP_UPDATE(dvp, &ts, &ts, 1)) != 0)
		goto bad;

	/* Initialize directory with "." and ".." from static template. */
	if (dvp->v_mount->mnt_maxsymlinklen > 0)
		dtp = &mastertemplate;
	else
		dtp = (struct dirtemplate *)&omastertemplate;
	dirtemplate = *dtp;
	dirtemplate.dot_ino = ip->i_number;
	dirtemplate.dotdot_ino = dp->i_number;
	error = vn_rdwr(UIO_WRITE, tvp, (caddr_t)&dirtemplate,
	    sizeof (dirtemplate), (off_t)0, UIO_SYSSPACE,
	    IO_NODELOCKED|IO_SYNC, cnp->cn_cred, (int *)0, (struct proc *)0);
	if (error) {
		dp->i_ffs_nlink--;
		dp->i_flag |= IN_CHANGE;
		goto bad;
	}
	if (DIRBLKSIZ > VFSTOUFS(dvp->v_mount)->um_mountp->mnt_stat.f_bsize)
		panic("ufs_mkdir: blksize"); /* XXX should grow with balloc() */
	else {
		ip->i_ffs_size = DIRBLKSIZ;
		ip->i_flag |= IN_CHANGE;
	}

	/* Directory set up, now install it's entry in the parent directory. */
	if ((error = ufs_direnter(ip, dvp, cnp)) != 0) {
		dp->i_ffs_nlink--;
		dp->i_flag |= IN_CHANGE;
	}
bad:
	/*
	 * No need to do an explicit VOP_TRUNCATE here, vrele will do this
	 * for us because we set the link count to 0.
	 */
	if (error) {
		ip->i_ffs_nlink = 0;
		ip->i_flag |= IN_CHANGE;
		vput(tvp);
	} else
		*ap->a_vpp = tvp;
out:
	FREE(cnp->cn_pnbuf, M_NAMEI);
	vput(dvp);
	return (error);
}

/*
 * Rmdir system call.
 */
int
ufs_rmdir(v)
	void *v;
{
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct vnode *dvp = ap->a_dvp;
	register struct componentname *cnp = ap->a_cnp;
	register struct inode *ip, *dp;
	int error;

	ip = VTOI(vp);
	dp = VTOI(dvp);
	/*
	 * No rmdir "." please.
	 */
	if (dp == ip) {
		vrele(dvp);
		vput(vp);
		return (EINVAL);
	}
	/*
	 * Verify the directory is empty (and valid).
	 * (Rmdir ".." won't be valid since
	 *  ".." will contain a reference to
	 *  the current directory and thus be
	 *  non-empty.)
	 */
	error = 0;
	if (ip->i_ffs_nlink != 2 ||
	    !ufs_dirempty(ip, dp->i_number, cnp->cn_cred)) {
		error = ENOTEMPTY;
		goto out;
	}
	if ((dp->i_ffs_flags & APPEND) ||
		(ip->i_ffs_flags & (IMMUTABLE | APPEND))) {
		error = EPERM;
		goto out;
	}
	/*
	 * Delete reference to directory before purging
	 * inode.  If we crash in between, the directory
	 * will be reattached to lost+found,
	 */
	if ((error = ufs_dirremove(dvp, cnp)) != 0)
		goto out;
	dp->i_ffs_nlink--;
	dp->i_flag |= IN_CHANGE;
	cache_purge(dvp);
	vput(dvp);
	dvp = NULL;
	/*
	 * Truncate inode.  The only stuff left
	 * in the directory is "." and "..".  The
	 * "." reference is inconsequential since
	 * we're quashing it.  The ".." reference
	 * has already been adjusted above.  We've
	 * removed the "." reference and the reference
	 * in the parent directory, but there may be
	 * other hard links so decrement by 2 and
	 * worry about them later.
	 */
	ip->i_ffs_nlink -= 2;
	error = VOP_TRUNCATE(vp, (off_t)0, IO_SYNC, cnp->cn_cred,
	    cnp->cn_proc);
	cache_purge(ITOV(ip));
out:
	if (dvp)
		vput(dvp);
	vput(vp);
	return (error);
}

/*
 * symlink -- make a symbolic link
 */
int
ufs_symlink(v)
	void *v;
{
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
	register struct vnode *vp, **vpp = ap->a_vpp;
	register struct inode *ip;
	int len, error;

	error = ufs_makeinode(IFLNK | ap->a_vap->va_mode, ap->a_dvp,
			      vpp, ap->a_cnp);
	if (error)
		return (error);
	vp = *vpp;
	len = strlen(ap->a_target);
	if (len < vp->v_mount->mnt_maxsymlinklen) {
		ip = VTOI(vp);
		bcopy(ap->a_target, (char *)ip->i_ffs_shortlink, len);
		ip->i_ffs_size = len;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	} else
		error = vn_rdwr(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED, ap->a_cnp->cn_cred, (int *)0,
		    (struct proc *)0);
	vput(vp);
	return (error);
}

/*
 * Vnode op for reading directories.
 * 
 * The routine below assumes that the on-disk format of a directory
 * is the same as that defined by <sys/dirent.h>. If the on-disk
 * format changes, then it will be necessary to do a conversion
 * from the on-disk format that read returns to the format defined
 * by <sys/dirent.h>.
 */
int
ufs_readdir(v)
	void *v;
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		off_t *a_cookies;
		int ncookies;
	} */ *ap = v;
	register struct uio *uio = ap->a_uio;
	int error;
	size_t count, lost;
	off_t off = uio->uio_offset;

	count = uio->uio_resid;
	/* Make sure we don't return partial entries. */
	count -= (uio->uio_offset + count) & (DIRBLKSIZ -1);
	if (count <= 0)
		return (EINVAL);
	lost = uio->uio_resid - count;
	uio->uio_resid = count;
	uio->uio_iov->iov_len = count;
#	if (BYTE_ORDER == LITTLE_ENDIAN)
		if (ap->a_vp->v_mount->mnt_maxsymlinklen > 0) {
			error = VOP_READ(ap->a_vp, uio, 0, ap->a_cred);
		} else {
			struct dirent *dp, *edp;
			struct uio auio;
			struct iovec aiov;
			caddr_t dirbuf;
			int readcnt;
			u_char tmp;

			auio = *uio;
			auio.uio_iov = &aiov;
			auio.uio_iovcnt = 1;
			auio.uio_segflg = UIO_SYSSPACE;
			aiov.iov_len = count;
			MALLOC(dirbuf, caddr_t, count, M_TEMP, M_WAITOK);
			aiov.iov_base = dirbuf;
			error = VOP_READ(ap->a_vp, &auio, 0, ap->a_cred);
			if (error == 0) {
				readcnt = count - auio.uio_resid;
				edp = (struct dirent *)&dirbuf[readcnt];
				for (dp = (struct dirent *)dirbuf; dp < edp; ) {
					tmp = dp->d_namlen;
					dp->d_namlen = dp->d_type;
					dp->d_type = tmp;
					if (dp->d_reclen > 0) {
						dp = (struct dirent *)
						    ((char *)dp + dp->d_reclen);
					} else {
						error = EIO;
						break;
					}
				}
				if (dp >= edp)
					error = uiomove(dirbuf, readcnt, uio);
			}
			FREE(dirbuf, M_TEMP);
		}
#	else
		error = VOP_READ(ap->a_vp, uio, 0, ap->a_cred);
#	endif
	if (!error && ap->a_ncookies) {
		register struct dirent *dp;
		register off_t *cookies = ap->a_cookies;
		register int ncookies = ap->a_ncookies;

		/*
		 * Only the NFS server and emulations use cookies, and they
		 * load the directory block into system space, so we can
		 * just look at it directly.
		 */
		if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
			panic("ufs_readdir: lost in space");
		dp = (struct dirent *)
		     (uio->uio_iov->iov_base - (uio->uio_offset - off));
		while (ncookies-- && off < uio->uio_offset) {
			if (dp->d_reclen == 0)
				break;
			off += dp->d_reclen;
			*(cookies++) = off;
			dp = (struct dirent *)((caddr_t)dp + dp->d_reclen);
		}
		lost += uio->uio_offset - off;
		uio->uio_offset = off;
	}
	uio->uio_resid += lost;
	*ap->a_eofflag = VTOI(ap->a_vp)->i_ffs_size <= uio->uio_offset;
	return (error);
}

/*
 * Return target name of a symbolic link
 */
int
ufs_readlink(v)
	void *v;
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip = VTOI(vp);
	int isize;

	isize = ip->i_ffs_size;
	if (isize < vp->v_mount->mnt_maxsymlinklen ||
	    (vp->v_mount->mnt_maxsymlinklen == 0 && ip->i_ffs_blocks == 0)) {
		uiomove((char *)ip->i_ffs_shortlink, isize, ap->a_uio);
		return (0);
	}
	return (VOP_READ(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Lock an inode. If its already locked, set the WANT bit and sleep.
 */
int
ufs_lock(v)
	void *v;
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip;
#ifdef DIAGNOSTIC
	struct proc *p = curproc;	/* XXX */
#endif

start:
	while (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		sleep((caddr_t)vp, PINOD);
	}
	if (vp->v_tag == VT_NON)
		return (ENOENT);
	ip = VTOI(vp);
	if (ip->i_flag & IN_LOCKED) {
		ip->i_flag |= IN_WANTED;
#ifdef DIAGNOSTIC
		if (p) {
			if (p->p_pid == ip->i_lockholder)
				panic("locking against myself");
			ip->i_lockwaiter = p->p_pid;
		} else
			ip->i_lockwaiter = -1;
#endif
		(void) sleep((caddr_t)ip, PINOD);
		goto start;
	}
#ifdef DIAGNOSTIC
	ip->i_lockwaiter = 0;
	if (ip->i_lockholder != 0)
		panic("lockholder (%d) != 0", ip->i_lockholder);
	if (p && p->p_pid == 0)
		printf("locking by process 0\n");
	if (p)
		ip->i_lockholder = p->p_pid;
	else
		ip->i_lockholder = -1;
#endif
	ip->i_flag |= IN_LOCKED;
	return (0);
}

/*
 * Unlock an inode.  If WANT bit is on, wakeup.
 */
int lockcount = 90;
int
ufs_unlock(v)
	void *v;
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	register struct inode *ip = VTOI(ap->a_vp);
#ifdef DIAGNOSTIC
	struct proc *p = curproc;	/* XXX */
#endif

#ifdef DIAGNOSTIC
	if ((ip->i_flag & IN_LOCKED) == 0) {
		vprint("ufs_unlock: unlocked inode", ap->a_vp);
		panic("ufs_unlock NOT LOCKED");
	}
	if (p && p->p_pid != ip->i_lockholder && p->p_pid > -1 &&
	    ip->i_lockholder > -1 && lockcount++ < 100)
		panic("unlocker (%d) != lock holder (%d)",
		    p->p_pid, ip->i_lockholder);
	ip->i_lockholder = 0;
#endif
	ip->i_flag &= ~IN_LOCKED;
	if (ip->i_flag & IN_WANTED) {
		ip->i_flag &= ~IN_WANTED;
		wakeup((caddr_t)ip);
	}
	return (0);
}

/*
 * Check for a locked inode.
 */
int
ufs_islocked(v)
	void *v;
{
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap = v;

	if (VTOI(ap->a_vp)->i_flag & IN_LOCKED)
		return (1);
	return (0);
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
ufs_strategy(v)
	void *v;
{
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap = v;
	register struct buf *bp = ap->a_bp;
	register struct vnode *vp = bp->b_vp;
	register struct inode *ip;
	int error;

	ip = VTOI(vp);
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("ufs_strategy: spec");
	if (bp->b_blkno == bp->b_lblkno) {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno,
				 NULL);
		if (error) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			biodone(bp);
			return (error);
		}
		if ((long)bp->b_blkno == -1)
			clrbuf(bp);
	}
	if ((long)bp->b_blkno == -1) {
		biodone(bp);
		return (0);
	}
	vp = ip->i_devvp;
	bp->b_dev = vp->v_rdev;
	VOCALL (vp->v_op, VOFFSET(vop_strategy), ap);
	return (0);
}

/*
 * Print out the contents of an inode.
 */
int
ufs_print(v)
	void *v;
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip = VTOI(vp);

	printf("tag VT_UFS, ino %d, on dev %d, %d", ip->i_number,
	    major(ip->i_dev), minor(ip->i_dev));
#ifdef FIFO
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
#endif /* FIFO */
	printf("%s\n", (ip->i_flag & IN_LOCKED) ? " (LOCKED)" : "");
	if (ip->i_lockholder == 0)
		return (0);
	printf("\towner pid %d", ip->i_lockholder);
	if (ip->i_lockwaiter)
		printf(" waiting pid %d", ip->i_lockwaiter);
	printf("\n");
	return (0);
}

/*
 * Read wrapper for special devices.
 */
int
ufsspec_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;

	/*
	 * Set access flag.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_ACCESS;
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for special devices.
 */
int
ufsspec_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;

	/*
	 * Set update and change flags.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_CHANGE | IN_UPDATE;
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the inode then do device close.
 */
int
ufsspec_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	register struct inode *ip = VTOI(ap->a_vp);
	struct timespec ts;

	if (ap->a_vp->v_usecount > 1 && !(ip->i_flag & IN_LOCKED)) {
		TIMEVAL_TO_TIMESPEC(&time, &ts);
		ITIMES(ip, &ts, &ts, &ts);
	}
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_close), ap));
}

#ifdef FIFO
/*
 * Read wrapper for fifo's
 */
int
ufsfifo_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	extern int (**fifo_vnodeop_p) __P((void *));

	/*
	 * Set access flag.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_ACCESS;
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for fifo's.
 */
int
ufsfifo_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	extern int (**fifo_vnodeop_p) __P((void *));

	/*
	 * Set update and change flags.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_CHANGE | IN_UPDATE;
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for fifo's.
 *
 * Update the times on the inode then do device close.
 */
int
ufsfifo_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	extern int (**fifo_vnodeop_p) __P((void *));
	register struct inode *ip = VTOI(ap->a_vp);
	struct timespec ts;

	if (ap->a_vp->v_usecount > 1 && !(ip->i_flag & IN_LOCKED)) {
		TIMEVAL_TO_TIMESPEC(&time, &ts);
		ITIMES(ip, &ts, &ts, &ts);
	}
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_close), ap));
}
#endif /* FIFO */

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
int
ufs_pathconf(v)
	void *v;
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Advisory record locking support
 */
int
ufs_advlock(v)
	void *v;
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap = v;
	register struct inode *ip = VTOI(ap->a_vp);

	return (lf_advlock(&ip->i_lockf, ip->i_ffs_size, ap->a_id, ap->a_op,
	    ap->a_fl, ap->a_flags));
}

/*
 * Initialize the vnode associated with a new inode, handle aliased
 * vnodes.
 */
int
ufs_vinit(mntp, specops, fifoops, vpp)
	struct mount *mntp;
	int (**specops) __P((void *));
	int (**fifoops) __P((void *));
	struct vnode **vpp;
{
	struct inode *ip;
	struct vnode *vp, *nvp;

	vp = *vpp;
	ip = VTOI(vp);
	switch(vp->v_type = IFTOVT(ip->i_ffs_mode)) {
	case VCHR:
	case VBLK:
		vp->v_op = specops;
		if ((nvp = checkalias(vp, ip->i_ffs_rdev, mntp)) != NULL) {
			/*
			 * Discard unneeded vnode, but save its inode.
			 */
			ufs_ihashrem(ip);
			VOP_UNLOCK(vp);
			nvp->v_data = vp->v_data;
			vp->v_data = NULL;
			vp->v_op = spec_vnodeop_p;
			vrele(vp);
			vgone(vp);
			/*
			 * Reinitialize aliased inode.
			 */
			vp = nvp;
			ip->i_vnode = vp;
			ufs_ihashins(ip);
		}
		break;
	case VFIFO:
#ifdef FIFO
		vp->v_op = fifoops;
		break;
#else
		return (EOPNOTSUPP);
#endif
	case VNON:
	case VBAD:
	case VSOCK:
	case VLNK:
	case VDIR:
	case VREG:
		break;
	}
	if (ip->i_number == ROOTINO)
                vp->v_flag |= VROOT;
	/*
	 * Initialize modrev times
	 */
	SETHIGH(ip->i_modrev, mono_time.tv_sec);
	SETLOW(ip->i_modrev, mono_time.tv_usec * 4294);
	*vpp = vp;
	return (0);
}

/*
 * Allocate a new inode.
 */
int
ufs_makeinode(mode, dvp, vpp, cnp)
	int mode;
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	register struct inode *ip, *pdir;
	struct timespec ts;
	struct vnode *tvp;
	int error;

	pdir = VTOI(dvp);
#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ufs_makeinode: no name");
#endif
	*vpp = NULL;
	if ((mode & IFMT) == 0)
		mode |= IFREG;

	if ((error = VOP_VALLOC(dvp, mode, cnp->cn_cred, &tvp)) != 0) {
		free(cnp->cn_pnbuf, M_NAMEI);
		vput(dvp);
		return (error);
	}
	ip = VTOI(tvp);
	ip->i_ffs_gid = pdir->i_ffs_gid;
	ip->i_ffs_uid = cnp->cn_cred->cr_uid;
#ifdef QUOTA
	if ((error = getinoquota(ip)) ||
	    (error = chkiq(ip, 1, cnp->cn_cred, 0))) {
		free(cnp->cn_pnbuf, M_NAMEI);
		VOP_VFREE(tvp, ip->i_number, mode);
		vput(tvp);
		vput(dvp);
		return (error);
	}
#endif
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_ffs_mode = mode;
	tvp->v_type = IFTOVT(mode);	/* Rest init'd in getnewvnode(). */
	ip->i_ffs_nlink = 1;
	if ((ip->i_ffs_mode & ISGID) &&
		!groupmember(ip->i_ffs_gid, cnp->cn_cred) &&
	    suser(cnp->cn_cred, NULL))
		ip->i_ffs_mode &= ~ISGID;

	if (cnp->cn_flags & ISWHITEOUT)
		ip->i_ffs_flags |= UF_OPAQUE;

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	if ((error = VOP_UPDATE(tvp, &ts, &ts, 1)) != 0)
		goto bad;
	if ((error = ufs_direnter(ip, dvp, cnp)) != 0)
		goto bad;
	if ((cnp->cn_flags & SAVESTART) == 0)
		FREE(cnp->cn_pnbuf, M_NAMEI);
	vput(dvp);
	*vpp = tvp;
	return (0);

bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
	free(cnp->cn_pnbuf, M_NAMEI);
	vput(dvp);
	ip->i_ffs_nlink = 0;
	ip->i_flag |= IN_CHANGE;
	vput(tvp);
	return (error);
}
