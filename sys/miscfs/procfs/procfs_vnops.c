/*	$NetBSD: procfs_vnops.c,v 1.78.2.5 2001/11/14 19:17:12 nathanw Exp $	*/

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_vnops.c	8.18 (Berkeley) 5/21/95
 */

/*
 * procfs vnode interface
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: procfs_vnops.c,v 1.78.2.5 2001/11/14 19:17:12 nathanw Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/resourcevar.h>
#include <sys/ptrace.h>
#include <sys/stat.h>

#include <uvm/uvm_extern.h>	/* for PAGE_SIZE */

#include <machine/reg.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/procfs/procfs.h>

/*
 * Vnode Operations.
 *
 */

static int procfs_validfile_linux __P((struct lwp *, struct mount *));

/*
 * This is a list of the valid names in the
 * process-specific sub-directories.  It is
 * used in procfs_lookup and procfs_readdir
 */
const struct proc_target {
	u_char	pt_type;
	u_char	pt_namlen;
	char	*pt_name;
	pfstype	pt_pfstype;
	int	(*pt_valid) __P((struct lwp *, struct mount *));
} proc_targets[] = {
#define N(s) sizeof(s)-1, s
	/*	  name		type		validp */
	{ DT_DIR, N("."),	Pproc,		NULL },
	{ DT_DIR, N(".."),	Proot,		NULL },
	{ DT_REG, N("file"),	Pfile,		procfs_validfile },
	{ DT_REG, N("mem"),	Pmem,		NULL },
	{ DT_REG, N("regs"),	Pregs,		procfs_validregs },
	{ DT_REG, N("fpregs"),	Pfpregs,	procfs_validfpregs },
	{ DT_REG, N("ctl"),	Pctl,		NULL },
	{ DT_REG, N("status"),	Pstatus,	NULL },
	{ DT_REG, N("note"),	Pnote,		NULL },
	{ DT_REG, N("notepg"),	Pnotepg,	NULL },
	{ DT_REG, N("map"),	Pmap,		procfs_validmap },
	{ DT_REG, N("maps"),	Pmaps,		procfs_validmap },
	{ DT_REG, N("cmdline"), Pcmdline,	NULL },
	{ DT_REG, N("exe"),	Pfile,		procfs_validfile_linux },
#undef N
};
static int nproc_targets = sizeof(proc_targets) / sizeof(proc_targets[0]);

/*
 * List of files in the root directory. Note: the validate function will
 * be called with p == NULL for these ones.
 */
struct proc_target proc_root_targets[] = {
#define N(s) sizeof(s)-1, s
	/*	  name		    type	    validp */
	{ DT_REG, N("meminfo"),     Pmeminfo,        procfs_validfile_linux },
	{ DT_REG, N("cpuinfo"),     Pcpuinfo,        procfs_validfile_linux },
#undef N
};
static int nproc_root_targets =
    sizeof(proc_root_targets) / sizeof(proc_root_targets[0]);

int	procfs_lookup	__P((void *));
#define	procfs_create	genfs_eopnotsupp_rele
#define	procfs_mknod	genfs_eopnotsupp_rele
int	procfs_open	__P((void *));
int	procfs_close	__P((void *));
int	procfs_access	__P((void *));
int	procfs_getattr	__P((void *));
int	procfs_setattr	__P((void *));
#define	procfs_read	procfs_rw
#define	procfs_write	procfs_rw
#define	procfs_fcntl	genfs_fcntl
#define	procfs_ioctl	genfs_enoioctl
#define	procfs_poll	genfs_poll
#define procfs_revoke	genfs_revoke
#define	procfs_fsync	genfs_nullop
#define	procfs_seek	genfs_nullop
#define	procfs_remove	genfs_eopnotsupp_rele
int	procfs_link	__P((void *));
#define	procfs_rename	genfs_eopnotsupp_rele
#define	procfs_mkdir	genfs_eopnotsupp_rele
#define	procfs_rmdir	genfs_eopnotsupp_rele
int	procfs_symlink	__P((void *));
int	procfs_readdir	__P((void *));
int	procfs_readlink	__P((void *));
#define	procfs_abortop	genfs_abortop
int	procfs_inactive	__P((void *));
int	procfs_reclaim	__P((void *));
#define	procfs_lock	genfs_lock
#define	procfs_unlock	genfs_unlock
#define	procfs_bmap	genfs_badop
#define	procfs_strategy	genfs_badop
int	procfs_print	__P((void *));
int	procfs_pathconf	__P((void *));
#define	procfs_islocked	genfs_islocked
#define	procfs_advlock	genfs_einval
#define	procfs_blkatoff	genfs_eopnotsupp
#define	procfs_valloc	genfs_eopnotsupp
#define	procfs_vfree	genfs_nullop
#define	procfs_truncate	genfs_eopnotsupp
#define	procfs_update	genfs_nullop
#define	procfs_bwrite	genfs_eopnotsupp

static pid_t atopid __P((const char *, u_int));

/*
 * procfs vnode operations.
 */
int (**procfs_vnodeop_p) __P((void *));
const struct vnodeopv_entry_desc procfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, procfs_lookup },		/* lookup */
	{ &vop_create_desc, procfs_create },		/* create */
	{ &vop_mknod_desc, procfs_mknod },		/* mknod */
	{ &vop_open_desc, procfs_open },		/* open */
	{ &vop_close_desc, procfs_close },		/* close */
	{ &vop_access_desc, procfs_access },		/* access */
	{ &vop_getattr_desc, procfs_getattr },		/* getattr */
	{ &vop_setattr_desc, procfs_setattr },		/* setattr */
	{ &vop_read_desc, procfs_read },		/* read */
	{ &vop_write_desc, procfs_write },		/* write */
	{ &vop_fcntl_desc, procfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, procfs_ioctl },		/* ioctl */
	{ &vop_poll_desc, procfs_poll },		/* poll */
	{ &vop_revoke_desc, procfs_revoke },		/* revoke */
	{ &vop_fsync_desc, procfs_fsync },		/* fsync */
	{ &vop_seek_desc, procfs_seek },		/* seek */
	{ &vop_remove_desc, procfs_remove },		/* remove */
	{ &vop_link_desc, procfs_link },		/* link */
	{ &vop_rename_desc, procfs_rename },		/* rename */
	{ &vop_mkdir_desc, procfs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, procfs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, procfs_symlink },		/* symlink */
	{ &vop_readdir_desc, procfs_readdir },		/* readdir */
	{ &vop_readlink_desc, procfs_readlink },	/* readlink */
	{ &vop_abortop_desc, procfs_abortop },		/* abortop */
	{ &vop_inactive_desc, procfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, procfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, procfs_lock },		/* lock */
	{ &vop_unlock_desc, procfs_unlock },		/* unlock */
	{ &vop_bmap_desc, procfs_bmap },		/* bmap */
	{ &vop_strategy_desc, procfs_strategy },	/* strategy */
	{ &vop_print_desc, procfs_print },		/* print */
	{ &vop_islocked_desc, procfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, procfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, procfs_advlock },		/* advlock */
	{ &vop_blkatoff_desc, procfs_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, procfs_valloc },		/* valloc */
	{ &vop_vfree_desc, procfs_vfree },		/* vfree */
	{ &vop_truncate_desc, procfs_truncate },	/* truncate */
	{ &vop_update_desc, procfs_update },		/* update */
	{ NULL, NULL }
};
const struct vnodeopv_desc procfs_vnodeop_opv_desc =
	{ &procfs_vnodeop_p, procfs_vnodeop_entries };
/*
 * set things up for doing i/o on
 * the pfsnode (vp).  (vp) is locked
 * on entry, and should be left locked
 * on exit.
 *
 * for procfs we don't need to do anything
 * in particular for i/o.  all that is done
 * is to support exclusive open on process
 * memory images.
 */
int
procfs_open(v)
	void *v;
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct proc *p1, *p2;

	p1 = ap->a_p;				/* tracer */
	p2 = PFIND(pfs->pfs_pid);		/* traced */

	if (p2 == NULL)
		return (ENOENT);		/* was ESRCH, jsp */

	switch (pfs->pfs_type) {
	case Pmem:
		if (((pfs->pfs_flags & FWRITE) && (ap->a_mode & O_EXCL)) ||
		    ((pfs->pfs_flags & O_EXCL) && (ap->a_mode & FWRITE)))
			return (EBUSY);

		if (procfs_checkioperm(p1, p2) != 0)
			return (EPERM);

		if (ap->a_mode & FWRITE)
			pfs->pfs_flags = ap->a_mode & (FWRITE|O_EXCL);

		return (0);

	default:
		break;
	}

	return (0);
}

/*
 * close the pfsnode (vp) after doing i/o.
 * (vp) is not locked on entry or exit.
 *
 * nothing to do for procfs other than undo
 * any exclusive open flag (see _open above).
 */
int
procfs_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct pfsnode *pfs = VTOPFS(ap->a_vp);

	switch (pfs->pfs_type) {
	case Pmem:
		if ((ap->a_fflag & FWRITE) && (pfs->pfs_flags & O_EXCL))
			pfs->pfs_flags &= ~(FWRITE|O_EXCL);
		break;

	default:
		break;
	}

	return (0);
}

/*
 * _inactive is called when the pfsnode
 * is vrele'd and the reference count goes
 * to zero.  (vp) will be on the vnode free
 * list, so to get it back vget() must be
 * used.
 *
 * for procfs, check if the process is still
 * alive and if it isn't then just throw away
 * the vnode by calling vgone().  this may
 * be overkill and a waste of time since the
 * chances are that the process will still be
 * there and PFIND is not free.
 *
 * (vp) is locked on entry, but must be unlocked on exit.
 */
int
procfs_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;
	struct pfsnode *pfs = VTOPFS(ap->a_vp);

	VOP_UNLOCK(ap->a_vp, 0);
	if (PFIND(pfs->pfs_pid) == 0)
		vgone(ap->a_vp);

	return (0);
}

/*
 * _reclaim is called when getnewvnode()
 * wants to make use of an entry on the vnode
 * free list.  at this time the filesystem needs
 * to free any private data and remove the node
 * from any private lists.
 */
int
procfs_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;

	return (procfs_freevp(ap->a_vp));
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
int
procfs_pathconf(v)
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
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * _print is used for debugging.
 * just print a readable description
 * of (vp).
 */
int
procfs_print(v)
	void *v;
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct pfsnode *pfs = VTOPFS(ap->a_vp);

	printf("tag VT_PROCFS, type %d, pid %d, mode %x, flags %lx\n",
	    pfs->pfs_type, pfs->pfs_pid, pfs->pfs_mode, pfs->pfs_flags);
	return 0;
}

int
procfs_link(v) 
	void *v;
{
	struct vop_link_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;  
		struct componentname *a_cnp;
	} */ *ap = v;
 
	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	vput(ap->a_dvp);
	return (EROFS);
}

int
procfs_symlink(v)
	void *v;
{
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
  
	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	vput(ap->a_dvp);
	return (EROFS);
}

/*
 * Invent attributes for pfsnode (vp) and store
 * them in (vap).
 * Directories lengths are returned as zero since
 * any real length would require the genuine size
 * to be computed, and nothing cares anyway.
 *
 * this is relatively minimal for procfs.
 */
int
procfs_getattr(v)
	void *v;
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct vattr *vap = ap->a_vap;
	struct proc *procp;
	struct timeval tv;
	int error;

	/* first check the process still exists */
	switch (pfs->pfs_type) {
	case Proot:
	case Pcurproc:
	case Pself:
		procp = 0;
		break;

	default:
		procp = PFIND(pfs->pfs_pid);
		if (procp == 0)
			return (ENOENT);
		break;
	}

	error = 0;

	/* start by zeroing out the attributes */
	VATTR_NULL(vap);

	/* next do all the common fields */
	vap->va_type = ap->a_vp->v_type;
	vap->va_mode = pfs->pfs_mode;
	vap->va_fileid = pfs->pfs_fileno;
	vap->va_flags = 0;
	vap->va_blocksize = PAGE_SIZE;

	/*
	 * Make all times be current TOD.
	 * It would be possible to get the process start
	 * time from the p_stat structure, but there's
	 * no "file creation" time stamp anyway, and the
	 * p_stat structure is not addressible if u. gets
	 * swapped out for that process.
	 */
	microtime(&tv);
	TIMEVAL_TO_TIMESPEC(&tv, &vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;

	switch (pfs->pfs_type) {
	case Pmem:
	case Pregs:
	case Pfpregs:
		/*
		 * If the process has exercised some setuid or setgid
		 * privilege, then rip away read/write permission so
		 * that only root can gain access.
		 */
		if (procp->p_flag & P_SUGID)
			vap->va_mode &= ~(S_IRUSR|S_IWUSR);
		/* FALLTHROUGH */
	case Pctl:
	case Pstatus:
	case Pnote:
	case Pnotepg:
	case Pmap:
	case Pmaps:
	case Pcmdline:
		vap->va_nlink = 1;
		vap->va_uid = procp->p_ucred->cr_uid;
		vap->va_gid = procp->p_ucred->cr_gid;
		break;
	case Pmeminfo:
	case Pcpuinfo:
		vap->va_nlink = 1;
		vap->va_uid = vap->va_gid = 0;
		break;

	default:
		break;
	}

	/*
	 * now do the object specific fields
	 *
	 * The size could be set from struct reg, but it's hardly
	 * worth the trouble, and it puts some (potentially) machine
	 * dependent data into this machine-independent code.  If it
	 * becomes important then this function should break out into
	 * a per-file stat function in the corresponding .c file.
	 */

	switch (pfs->pfs_type) {
	case Proot:
		/*
		 * Set nlink to 1 to tell fts(3) we don't actually know.
		 */
		vap->va_nlink = 1;
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_bytes = vap->va_size = DEV_BSIZE;
		break;

	case Pcurproc: {
		char buf[16];		/* should be enough */
		vap->va_nlink = 1;
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_bytes = vap->va_size =
		    sprintf(buf, "%ld", (long)curproc->l_proc->p_pid);
		break;
	}

	case Pself:
		vap->va_nlink = 1;
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_bytes = vap->va_size = sizeof("curproc");
		break;

	case Pproc:
		vap->va_nlink = 2;
		vap->va_uid = procp->p_ucred->cr_uid;
		vap->va_gid = procp->p_ucred->cr_gid;
		vap->va_bytes = vap->va_size = DEV_BSIZE;
		break;

	case Pfile:
		error = EOPNOTSUPP;
		break;

	case Pmem:
		vap->va_bytes = vap->va_size =
			ctob(procp->p_vmspace->vm_tsize +
				    procp->p_vmspace->vm_dsize +
				    procp->p_vmspace->vm_ssize);
		break;

#if defined(PT_GETREGS) || defined(PT_SETREGS)
	case Pregs:
		vap->va_bytes = vap->va_size = sizeof(struct reg);
		break;
#endif

#if defined(PT_GETFPREGS) || defined(PT_SETFPREGS)
	case Pfpregs:
		vap->va_bytes = vap->va_size = sizeof(struct fpreg);
		break;
#endif

	case Pctl:
	case Pstatus:
	case Pnote:
	case Pnotepg:
	case Pcmdline:
	case Pmeminfo:
	case Pcpuinfo:
		vap->va_bytes = vap->va_size = 0;
		break;
	case Pmap:
	case Pmaps:
		/*
		 * Advise a larger blocksize for the map files, so that
		 * they may be read in one pass.
		 */
		vap->va_blocksize = 4 * PAGE_SIZE;
		vap->va_bytes = vap->va_size = 0;
		break;

	default:
		panic("procfs_getattr");
	}

	return (error);
}

/*ARGSUSED*/
int
procfs_setattr(v)
	void *v;
{
	/*
	 * just fake out attribute setting
	 * it's not good to generate an error
	 * return, otherwise things like creat()
	 * will fail when they try to set the
	 * file length to 0.  worse, this means
	 * that echo $note > /proc/$pid/note will fail.
	 */

	return (0);
}

/*
 * implement access checking.
 *
 * actually, the check for super-user is slightly
 * broken since it will allow read access to write-only
 * objects.  this doesn't cause any particular trouble
 * but does mean that the i/o entry points need to check
 * that the operation really does make sense.
 */
int
procfs_access(v)
	void *v;
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vattr va;
	int error;

	if ((error = VOP_GETATTR(ap->a_vp, &va, ap->a_cred, ap->a_p)) != 0)
		return (error);

	return (vaccess(va.va_type, va.va_mode,
	    va.va_uid, va.va_gid, ap->a_mode, ap->a_cred));
}

/*
 * lookup.  this is incredibly complicated in the
 * general case, however for most pseudo-filesystems
 * very little needs to be done.
 *
 * Locking isn't hard here, just poorly documented.
 *
 * If we're looking up ".", just vref the parent & return it. 
 *
 * If we're looking up "..", unlock the parent, and lock "..". If everything
 * went ok, and we're on the last component and the caller requested the
 * parent locked, try to re-lock the parent. We do this to prevent lock
 * races.
 *
 * For anything else, get the needed node. Then unlock the parent if not
 * the last component or not LOCKPARENT (i.e. if we wouldn't re-lock the
 * parent in the .. case).
 *
 * We try to exit with the parent locked in error cases.
 */
int
procfs_lookup(v)
	void *v;
{
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	const char *pname = cnp->cn_nameptr;
	const struct proc_target *pt = NULL;
	struct vnode *fvp;
	pid_t pid;
	struct pfsnode *pfs;
	struct proc *p = NULL;
	int i, error, wantpunlock, iscurproc = 0, isself = 0;

	*vpp = NULL;
	cnp->cn_flags &= ~PDIRUNLOCK;

	if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
		return (EROFS);

	if (cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		VREF(dvp);
		return (0);
	}

	wantpunlock = (~cnp->cn_flags & (LOCKPARENT | ISLASTCN));
	pfs = VTOPFS(dvp);
	switch (pfs->pfs_type) {
	case Proot:
		/*
		 * Shouldn't get here with .. in the root node.
		 */
		if (cnp->cn_flags & ISDOTDOT) 
			return (EIO);

		iscurproc = CNEQ(cnp, "curproc", 7);
		isself = CNEQ(cnp, "self", 4);

		if (iscurproc || isself) {
			error = procfs_allocvp(dvp->v_mount, vpp, 0,
			    iscurproc ? Pcurproc : Pself);
			if ((error == 0) && (wantpunlock)) {
				VOP_UNLOCK(dvp, 0);
				cnp->cn_flags |= PDIRUNLOCK;
			}
			return (error);
		}

		for (i = 0; i < nproc_root_targets; i++) {
			pt = &proc_root_targets[i];
			if (cnp->cn_namelen == pt->pt_namlen &&
			    memcmp(pt->pt_name, pname, cnp->cn_namelen) == 0 &&
			    (pt->pt_valid == NULL ||
			     (*pt->pt_valid)(LIST_FIRST(&p->p_lwps), 
				 dvp->v_mount)))
				break;
		}

		if (i != nproc_root_targets) {
			error = procfs_allocvp(dvp->v_mount, vpp, 0,
			    pt->pt_pfstype);
			if ((error == 0) && (wantpunlock)) {
				VOP_UNLOCK(dvp, 0);
				cnp->cn_flags |= PDIRUNLOCK;
			}
			return (error);
		}

		pid = atopid(pname, cnp->cn_namelen);
		if (pid == NO_PID)
			break;

		p = PFIND(pid);
		if (p == 0)
			break;

		error = procfs_allocvp(dvp->v_mount, vpp, pid, Pproc);
		if ((error == 0) && (wantpunlock)) {
			VOP_UNLOCK(dvp, 0);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return (error);

	case Pproc:
		/*
		 * do the .. dance. We unlock the directory, and then
		 * get the root dir. That will automatically return ..
		 * locked. Then if the caller wanted dvp locked, we
		 * re-lock.
		 */
		if (cnp->cn_flags & ISDOTDOT) {
			VOP_UNLOCK(dvp, 0);
			cnp->cn_flags |= PDIRUNLOCK;
			error = procfs_root(dvp->v_mount, vpp);
			if ((error == 0) && (wantpunlock == 0) &&
				    ((error = vn_lock(dvp, LK_EXCLUSIVE)) == 0))
				cnp->cn_flags &= ~PDIRUNLOCK;
			return (error);
		}

		p = PFIND(pfs->pfs_pid);
		if (p == 0)
			break;

		for (pt = proc_targets, i = 0; i < nproc_targets; pt++, i++) {
			if (cnp->cn_namelen == pt->pt_namlen &&
			    memcmp(pt->pt_name, pname, cnp->cn_namelen) == 0 &&
			    (pt->pt_valid == NULL ||
			     (*pt->pt_valid)(LIST_FIRST(&p->p_lwps), 
				 dvp->v_mount)))
				goto found;
		}
		break;

	found:
		if (pt->pt_pfstype == Pfile) {
			fvp = p->p_textvp;
			/* We already checked that it exists. */
			VREF(fvp);
			vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY);
			if (wantpunlock) {
				VOP_UNLOCK(dvp, 0);
				cnp->cn_flags |= PDIRUNLOCK;
			}
			*vpp = fvp;
			return (0);
		}

		error = procfs_allocvp(dvp->v_mount, vpp, pfs->pfs_pid,
					pt->pt_pfstype);
		if ((error == 0) && (wantpunlock)) {
			VOP_UNLOCK(dvp, 0);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return (error);

	default:
		return (ENOTDIR);
	}

	return (cnp->cn_nameiop == LOOKUP ? ENOENT : EROFS);
}

int
procfs_validfile(l, mp)
	struct lwp *l;
	struct mount *mp;
{
	return (l->l_proc->p_textvp != NULL);
}

static int
procfs_validfile_linux(l, mp)
	struct lwp *l;
	struct mount *mp;
{
	int flags;

	flags = VFSTOPROC(mp)->pmnt_flags;
	return ((flags & PROCFSMNT_LINUXCOMPAT) &&
	    (l == NULL || procfs_validfile(l, mp)));
}

/*
 * readdir returns directory entries from pfsnode (vp).
 *
 * the strategy here with procfs is to generate a single
 * directory entry at a time (struct dirent) and then
 * copy that out to userland using uiomove.  a more efficent
 * though more complex implementation, would try to minimize
 * the number of calls to uiomove().  for procfs, this is
 * hardly worth the added code complexity.
 *
 * this should just be done through read()
 */
int
procfs_readdir(v)
	void *v;
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		off_t **a_cookies;
		int *a_ncookies;
	} */ *ap = v;
	struct uio *uio = ap->a_uio;
	struct dirent d;
	struct pfsnode *pfs;
	off_t i;
	int error;
	off_t *cookies = NULL;
	int ncookies, left, skip, j;
	struct vnode *vp;
	const struct proc_target *pt;

	vp = ap->a_vp;
	pfs = VTOPFS(vp);

	if (uio->uio_resid < UIO_MX)
		return (EINVAL);
	if (uio->uio_offset < 0)
		return (EINVAL);

	error = 0;
	i = uio->uio_offset;
	memset((caddr_t)&d, 0, UIO_MX);
	d.d_reclen = UIO_MX;
	ncookies = uio->uio_resid / UIO_MX;

	switch (pfs->pfs_type) {
	/*
	 * this is for the process-specific sub-directories.
	 * all that is needed to is copy out all the entries
	 * from the procent[] table (top of this file).
	 */
	case Pproc: {
		struct proc *p;

		if (i >= nproc_targets)
			return 0;

		p = PFIND(pfs->pfs_pid);
		if (p == NULL)
			break;

		if (ap->a_ncookies) {
			ncookies = min(ncookies, (nproc_targets - i));
			cookies = malloc(ncookies * sizeof (off_t),
			    M_TEMP, M_WAITOK);
			*ap->a_cookies = cookies;
		}

		for (pt = &proc_targets[i];
		     uio->uio_resid >= UIO_MX && i < nproc_targets; pt++, i++) {
			if (pt->pt_valid &&
			    (*pt->pt_valid)(LIST_FIRST(&p->p_lwps), 
				vp->v_mount) == 0)
				continue;
			
			d.d_fileno = PROCFS_FILENO(pfs->pfs_pid, pt->pt_pfstype);
			d.d_namlen = pt->pt_namlen;
			memcpy(d.d_name, pt->pt_name, pt->pt_namlen + 1);
			d.d_type = pt->pt_type;

			if ((error = uiomove((caddr_t)&d, UIO_MX, uio)) != 0)
				break;
			if (cookies)
				*cookies++ = i + 1;
		}

	    	break;
	}

	/*
	 * this is for the root of the procfs filesystem
	 * what is needed are special entries for "curproc"
	 * and "self" followed by an entry for each process
	 * on allproc
#ifdef PROCFS_ZOMBIE
	 * and deadproc and zombproc.
#endif
	 */

	case Proot: {
		int pcnt = i, nc = 0;
		const struct proclist_desc *pd;
		volatile struct proc *p;

		if (pcnt > 3)
			pcnt = 3;
		if (ap->a_ncookies) {
			/*
			 * XXX Potentially allocating too much space here,
			 * but I'm lazy. This loop needs some work.
			 */
			cookies = malloc(ncookies * sizeof (off_t),
			    M_TEMP, M_WAITOK);
			*ap->a_cookies = cookies;
		}
		/*
		 * XXX: THIS LOOP ASSUMES THAT allproc IS THE FIRST
		 * PROCLIST IN THE proclists!
		 */
		proclist_lock_read();
		pd = proclists;
#ifdef PROCFS_ZOMBIE
	again:
#endif
		for (p = LIST_FIRST(pd->pd_list);
		     p != NULL && uio->uio_resid >= UIO_MX; i++, pcnt++) {
			switch (i) {
			case 0:		/* `.' */
			case 1:		/* `..' */
				d.d_fileno = PROCFS_FILENO(0, Proot);
				d.d_namlen = i + 1;
				memcpy(d.d_name, "..", d.d_namlen);
				d.d_name[i + 1] = '\0';
				d.d_type = DT_DIR;
				break;

			case 2:
				d.d_fileno = PROCFS_FILENO(0, Pcurproc);
				d.d_namlen = sizeof("curproc") - 1;
				memcpy(d.d_name, "curproc", sizeof("curproc"));
				d.d_type = DT_LNK;
				break;

			case 3:
				d.d_fileno = PROCFS_FILENO(0, Pself);
				d.d_namlen = sizeof("self") - 1;
				memcpy(d.d_name, "self", sizeof("self"));
				d.d_type = DT_LNK;
				break;

			default:
				while (pcnt < i) {
					pcnt++;
					p = LIST_NEXT(p, p_list);
					if (!p)
						goto done;
				}
				d.d_fileno = PROCFS_FILENO(p->p_pid, Pproc);
				d.d_namlen = sprintf(d.d_name, "%ld",
				    (long)p->p_pid);
				d.d_type = DT_DIR;
				p = p->p_list.le_next;
				break;
			}

			if ((error = uiomove((caddr_t)&d, UIO_MX, uio)) != 0)
				break;
			nc++;
			if (cookies)
				*cookies++ = i + 1;
		}
	done:

#ifdef PROCFS_ZOMBIE
		pd++;
		if (p == NULL && pd->pd_list != NULL)
			goto again;
#endif
		proclist_unlock_read();

		skip = i - pcnt;
		if (skip >= nproc_root_targets)
			break;
		left = nproc_root_targets - skip;
		for (j = 0, pt = &proc_root_targets[0];
		     uio->uio_resid >= UIO_MX && j < left;
		     pt++, j++, i++) {
			if (pt->pt_valid &&
			    (*pt->pt_valid)(NULL, vp->v_mount) == 0)
				continue;
			d.d_fileno = PROCFS_FILENO(0, pt->pt_pfstype);
			d.d_namlen = pt->pt_namlen;
			memcpy(d.d_name, pt->pt_name, pt->pt_namlen + 1);
			d.d_type = pt->pt_type;

			if ((error = uiomove((caddr_t)&d, UIO_MX, uio)) != 0)
				break;
			nc++;
			if (cookies)
				*cookies++ = i + 1;
		}

		ncookies = nc;
		break;
	}

	default:
		error = ENOTDIR;
		break;
	}

	if (ap->a_ncookies) {
		if (error) {
			if (cookies)
				free(*ap->a_cookies, M_TEMP);
			*ap->a_ncookies = 0;
			*ap->a_cookies = NULL;
		} else
			*ap->a_ncookies = ncookies;
	}
	uio->uio_offset = i;
	return (error);
}

/*
 * readlink reads the link of `curproc'
 */
int
procfs_readlink(v)
	void *v;
{
	struct vop_readlink_args *ap = v;
	char buf[16];		/* should be enough */
	int len;

	if (VTOPFS(ap->a_vp)->pfs_fileno == PROCFS_FILENO(0, Pcurproc))
		len = sprintf(buf, "%ld", (long)curproc->l_proc->p_pid);
	else if (VTOPFS(ap->a_vp)->pfs_fileno == PROCFS_FILENO(0, Pself))
		len = sprintf(buf, "%s", "curproc");
	else
		return (EINVAL);

	return (uiomove((caddr_t)buf, len, ap->a_uio));
}

/*
 * convert decimal ascii to pid_t
 */
static pid_t
atopid(b, len)
	const char *b;
	u_int len;
{
	pid_t p = 0;

	while (len--) {
		char c = *b++;
		if (c < '0' || c > '9')
			return (NO_PID);
		p = 10 * p + (c - '0');
		if (p > PID_MAX)
			return (NO_PID);
	}

	return (p);
}
