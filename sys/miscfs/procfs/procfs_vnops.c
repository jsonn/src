/*	$NetBSD: procfs_vnops.c,v 1.106.2.5 2004/09/21 13:36:32 skrll Exp $	*/

/*
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
 *	@(#)procfs_vnops.c	8.18 (Berkeley) 5/21/95
 */

/*
 * Copyright (c) 1993 Jan-Simon Pendry
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
__KERNEL_RCSID(0, "$NetBSD: procfs_vnops.c,v 1.106.2.5 2004/09/21 13:36:32 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/resourcevar.h>
#include <sys/stat.h>
#include <sys/ptrace.h>

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
static const struct proc_target {
	u_char	pt_type;
	u_char	pt_namlen;
	const char	*pt_name;
	pfstype	pt_pfstype;
	int	(*pt_valid) __P((struct lwp *, struct mount *));
} proc_targets[] = {
#define N(s) sizeof(s)-1, s
	/*	  name		type		validp */
	{ DT_DIR, N("."),	PFSproc,		NULL },
	{ DT_DIR, N(".."),	PFSroot,		NULL },
	{ DT_DIR, N("fd"),	PFSfd,		NULL },
	{ DT_REG, N("file"),	PFSfile,		procfs_validfile },
	{ DT_REG, N("mem"),	PFSmem,		NULL },
	{ DT_REG, N("regs"),	PFSregs,		procfs_validregs },
	{ DT_REG, N("fpregs"),	PFSfpregs,	procfs_validfpregs },
	{ DT_REG, N("ctl"),	PFSctl,		NULL },
	{ DT_REG, N("stat"),	PFSstat,		procfs_validfile_linux },
	{ DT_REG, N("status"),	PFSstatus,	NULL },
	{ DT_REG, N("note"),	PFSnote,		NULL },
	{ DT_REG, N("notepg"),	PFSnotepg,	NULL },
	{ DT_REG, N("map"),	PFSmap,		procfs_validmap },
	{ DT_REG, N("maps"),	PFSmaps,		procfs_validmap },
	{ DT_REG, N("cmdline"), PFScmdline,	NULL },
	{ DT_REG, N("exe"),	PFSfile,		procfs_validfile_linux },
#ifdef __HAVE_PROCFS_MACHDEP
	PROCFS_MACHDEP_NODETYPE_DEFNS
#endif
#undef N
};
static const int nproc_targets = sizeof(proc_targets) / sizeof(proc_targets[0]);

/*
 * List of files in the root directory. Note: the validate function will
 * be called with p == NULL for these ones.
 */
static const struct proc_target proc_root_targets[] = {
#define N(s) sizeof(s)-1, s
	/*	  name		    type	    validp */
	{ DT_REG, N("meminfo"),     PFSmeminfo,        procfs_validfile_linux },
	{ DT_REG, N("cpuinfo"),     PFScpuinfo,        procfs_validfile_linux },
	{ DT_REG, N("uptime"),      PFSuptime,         procfs_validfile_linux },
#undef N
};
static const int nproc_root_targets =
    sizeof(proc_root_targets) / sizeof(proc_root_targets[0]);

int	procfs_lookup	__P((void *));
#define	procfs_create	genfs_eopnotsupp
#define	procfs_mknod	genfs_eopnotsupp
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
#define	procfs_remove	genfs_eopnotsupp
int	procfs_link	__P((void *));
#define	procfs_rename	genfs_eopnotsupp
#define	procfs_mkdir	genfs_eopnotsupp
#define	procfs_rmdir	genfs_eopnotsupp
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
#define procfs_putpages	genfs_null_putpages

static int atoi __P((const char *, size_t));

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
	{ &vop_putpages_desc, procfs_putpages },	/* putpages */
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
		struct lwp *a_l;
	} */ *ap = v;
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct lwp *l1;
	struct proc *p2;
	int error;

	l1 = ap->a_l;				/* tracer */
	p2 = PFIND(pfs->pfs_pid);		/* traced */

	if (p2 == NULL)
		return (ENOENT);		/* was ESRCH, jsp */

	switch (pfs->pfs_type) {
	case PFSmem:
		if (((pfs->pfs_flags & FWRITE) && (ap->a_mode & O_EXCL)) ||
		    ((pfs->pfs_flags & O_EXCL) && (ap->a_mode & FWRITE)))
			return (EBUSY);

		if ((error = process_checkioperm(l1, p2)) != 0)
			return (error);

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
	case PFSmem:
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
	if (PFIND(pfs->pfs_pid) == NULL)
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
	int error;

	/* first check the process still exists */
	switch (pfs->pfs_type) {
	case PFSroot:
	case PFScurproc:
	case PFSself:
		procp = 0;
		break;

	default:
		procp = PFIND(pfs->pfs_pid);
		if (procp == NULL)
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
	 * Make all times be current TOD.  Avoid microtime(9), it's slow.
	 * We don't guard the read from time(9) with splclock(9) since we
	 * don't actually need to be THAT sure the access is atomic. 
	 *
	 * It would be possible to get the process start
	 * time from the p_stat structure, but there's
	 * no "file creation" time stamp anyway, and the
	 * p_stat structure is not addressible if u. gets
	 * swapped out for that process.
	 */
	TIMEVAL_TO_TIMESPEC(&time, &vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;

	switch (pfs->pfs_type) {
	case PFSmem:
	case PFSregs:
	case PFSfpregs:
#if defined(__HAVE_PROCFS_MACHDEP) && defined(PROCFS_MACHDEP_PROTECT_CASES)
	PROCFS_MACHDEP_PROTECT_CASES
#endif
		/*
		 * If the process has exercised some setuid or setgid
		 * privilege, then rip away read/write permission so
		 * that only root can gain access.
		 */
		if (procp->p_flag & P_SUGID)
			vap->va_mode &= ~(S_IRUSR|S_IWUSR);
		/* FALLTHROUGH */
	case PFSctl:
	case PFSstatus:
	case PFSstat:
	case PFSnote:
	case PFSnotepg:
	case PFSmap:
	case PFSmaps:
	case PFScmdline:
		vap->va_nlink = 1;
		vap->va_uid = procp->p_ucred->cr_uid;
		vap->va_gid = procp->p_ucred->cr_gid;
		break;
	case PFSmeminfo:
	case PFScpuinfo:
	case PFSuptime:
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
	case PFSroot:
		/*
		 * Set nlink to 1 to tell fts(3) we don't actually know.
		 */
		vap->va_nlink = 1;
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_bytes = vap->va_size = DEV_BSIZE;
		break;

	case PFScurproc: {
		char buf[16];		/* should be enough */
		vap->va_nlink = 1;
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_bytes = vap->va_size =
		    snprintf(buf, sizeof(buf), "%ld", (long)curproc->p_pid);
		break;
	}

	case PFSself:
		vap->va_nlink = 1;
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_bytes = vap->va_size = sizeof("curproc");
		break;

	case PFSfd:
		if (pfs->pfs_fd != -1) {
			struct file *fp;
			struct proc *pown;

			if ((error = procfs_getfp(pfs, &pown, &fp)) != 0)
				return error;
			FILE_USE(fp);
			vap->va_nlink = 1;
			vap->va_uid = fp->f_cred->cr_uid;
			vap->va_gid = fp->f_cred->cr_gid;
			switch (fp->f_type) {
			case DTYPE_VNODE:
				vap->va_bytes = vap->va_size =
				    ((struct vnode *)fp->f_data)->v_size;
				break;
			default:
				vap->va_bytes = vap->va_size = 0;
				break;
			}
			FILE_UNUSE(fp, proc_representative_lwp(pown));
			break;
		}
		/*FALLTHROUGH*/
	case PFSproc:
		vap->va_nlink = 2;
		vap->va_uid = procp->p_ucred->cr_uid;
		vap->va_gid = procp->p_ucred->cr_gid;
		vap->va_bytes = vap->va_size = DEV_BSIZE;
		break;

	case PFSfile:
		error = EOPNOTSUPP;
		break;

	case PFSmem:
		vap->va_bytes = vap->va_size =
			ctob(procp->p_vmspace->vm_tsize +
				    procp->p_vmspace->vm_dsize +
				    procp->p_vmspace->vm_ssize);
		break;

#if defined(PT_GETREGS) || defined(PT_SETREGS)
	case PFSregs:
		vap->va_bytes = vap->va_size = sizeof(struct reg);
		break;
#endif

#if defined(PT_GETFPREGS) || defined(PT_SETFPREGS)
	case PFSfpregs:
		vap->va_bytes = vap->va_size = sizeof(struct fpreg);
		break;
#endif

	case PFSctl:
	case PFSstatus:
	case PFSstat:
	case PFSnote:
	case PFSnotepg:
	case PFScmdline:
	case PFSmeminfo:
	case PFScpuinfo:
	case PFSuptime:
		vap->va_bytes = vap->va_size = 0;
		break;
	case PFSmap:
	case PFSmaps:
		/*
		 * Advise a larger blocksize for the map files, so that
		 * they may be read in one pass.
		 */
		vap->va_blocksize = 4 * PAGE_SIZE;
		vap->va_bytes = vap->va_size = 0;
		break;

#ifdef __HAVE_PROCFS_MACHDEP
	PROCFS_MACHDEP_NODETYPE_CASES
		error = procfs_machdep_getattr(ap->a_vp, vap, procp);
		break;
#endif

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
		struct lwp *a_l;
	} */ *ap = v;
	struct vattr va;
	int error;

	if ((error = VOP_GETATTR(ap->a_vp, &va, ap->a_cred, ap->a_l)) != 0)
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
	struct lwp *l = NULL;
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
	case PFSroot:
		/*
		 * Shouldn't get here with .. in the root node.
		 */
		if (cnp->cn_flags & ISDOTDOT) 
			return (EIO);

		iscurproc = CNEQ(cnp, "curproc", 7);
		isself = CNEQ(cnp, "self", 4);

		if (iscurproc || isself) {
			error = procfs_allocvp(dvp->v_mount, vpp, 0,
			    iscurproc ? PFScurproc : PFSself, -1);
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
			     (*pt->pt_valid)(cnp->cn_lwp, dvp->v_mount)))
				break;
		}

		if (i != nproc_root_targets) {
			error = procfs_allocvp(dvp->v_mount, vpp, 0,
			    pt->pt_pfstype, -1);
			if ((error == 0) && (wantpunlock)) {
				VOP_UNLOCK(dvp, 0);
				cnp->cn_flags |= PDIRUNLOCK;
			}
			return (error);
		}

		pid = (pid_t)atoi(pname, cnp->cn_namelen);

		p = PFIND(pid);
		if (p == NULL)
			break;

		error = procfs_allocvp(dvp->v_mount, vpp, pid, PFSproc, -1);
		if ((error == 0) && (wantpunlock)) {
			VOP_UNLOCK(dvp, 0);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return (error);

	case PFSproc:
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
		if (p == NULL)
			break;
		l = proc_representative_lwp(p);

		for (pt = proc_targets, i = 0; i < nproc_targets; pt++, i++) {
			if (cnp->cn_namelen == pt->pt_namlen &&
			    memcmp(pt->pt_name, pname, cnp->cn_namelen) == 0 &&
			    (pt->pt_valid == NULL ||
			     (*pt->pt_valid)(cnp->cn_lwp, dvp->v_mount)))
				goto found;
		}
		break;

	found:
		if (pt->pt_pfstype == PFSfile) {
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
		    pt->pt_pfstype, -1);
		if ((error == 0) && (wantpunlock)) {
			VOP_UNLOCK(dvp, 0);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return (error);

	case PFSfd: {
		int fd;
		struct file *fp;
		/*
		 * do the .. dance. We unlock the directory, and then
		 * get the proc dir. That will automatically return ..
		 * locked. Then if the caller wanted dvp locked, we
		 * re-lock.
		 */
		if (cnp->cn_flags & ISDOTDOT) {
			VOP_UNLOCK(dvp, 0);
			cnp->cn_flags |= PDIRUNLOCK;
			error = procfs_allocvp(dvp->v_mount, vpp, pfs->pfs_pid,
			    PFSproc, -1);
			if ((error == 0) && (wantpunlock == 0) &&
				    ((error = vn_lock(dvp, LK_EXCLUSIVE)) == 0))
				cnp->cn_flags &= ~PDIRUNLOCK;
			return (error);
		}
		fd = atoi(pname, cnp->cn_namelen);
		p = PFIND(pfs->pfs_pid);
		if (p == NULL || (fp = fd_getfile(p->p_fd, fd)) == NULL)
			return ENOENT;
		FILE_USE(fp);

		switch (fp->f_type) {
		case DTYPE_VNODE:
			fvp = (struct vnode *)fp->f_data;

			/* Don't show directories */
			if (fvp->v_type == VDIR)
				goto symlink;

			VREF(fvp);
			FILE_UNUSE(fp, l);
			vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY | 
			    (p == curproc ? LK_CANRECURSE : 0));
			*vpp = fvp;
			error = 0;
			break;
		default:
		symlink:
			FILE_UNUSE(fp, l);
			error = procfs_allocvp(dvp->v_mount, vpp, pfs->pfs_pid,
			    PFSfd, fd);
			break;
		}
		if ((error == 0) && (wantpunlock)) {
			VOP_UNLOCK(dvp, 0);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return error;
	}
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
	    (l == NULL || l->l_proc == NULL || procfs_validfile(l, mp)));
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
	memset(&d, 0, UIO_MX);
	d.d_reclen = UIO_MX;
	ncookies = uio->uio_resid / UIO_MX;

	switch (pfs->pfs_type) {
	/*
	 * this is for the process-specific sub-directories.
	 * all that is needed to is copy out all the entries
	 * from the procent[] table (top of this file).
	 */
	case PFSproc: {
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
			    (*pt->pt_valid)(proc_representative_lwp(p), vp->v_mount) == 0)
				continue;
			
			d.d_fileno = PROCFS_FILENO(pfs->pfs_pid,
			    pt->pt_pfstype, -1);
			d.d_namlen = pt->pt_namlen;
			memcpy(d.d_name, pt->pt_name, pt->pt_namlen + 1);
			d.d_type = pt->pt_type;

			if ((error = uiomove(&d, UIO_MX, uio)) != 0)
				break;
			if (cookies)
				*cookies++ = i + 1;
		}

	    	break;
	}
	case PFSfd: {
		struct proc *p;
		struct filedesc	*fdp;
		struct file *fp;
		int lim, nc = 0;

		p = PFIND(pfs->pfs_pid);
		if (p == NULL)
			return ESRCH;

		fdp = p->p_fd;

		lim = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfiles);
		if (i >= lim)
			return 0;

		if (ap->a_ncookies) {
			ncookies = min(ncookies, (fdp->fd_nfiles + 2 - i));
			cookies = malloc(ncookies * sizeof (off_t),
			    M_TEMP, M_WAITOK);
			*ap->a_cookies = cookies;
		}

		for (; i < 2 && uio->uio_resid >= UIO_MX; i++) {
			pt = &proc_targets[i];
			d.d_namlen = pt->pt_namlen;
			d.d_fileno = PROCFS_FILENO(pfs->pfs_pid,
			    pt->pt_pfstype, -1);
			(void)memcpy(d.d_name, pt->pt_name, pt->pt_namlen + 1);
			d.d_type = pt->pt_type;
			if ((error = uiomove(&d, UIO_MX, uio)) != 0)
				break;
			if (cookies)
				*cookies++ = i + 1;
			nc++;
		}
		if (error) {
			ncookies = nc;
			break;
		}
		for (; uio->uio_resid >= UIO_MX && i < fdp->fd_nfiles; i++) {
			/* check the descriptor exists */
			if ((fp = fd_getfile(fdp, i - 2)) == NULL)
				continue;
			simple_unlock(&fp->f_slock);

			d.d_fileno = PROCFS_FILENO(pfs->pfs_pid, PFSfd, i - 2);
			d.d_namlen = snprintf(d.d_name, sizeof(d.d_name),
			    "%lld", (long long)(i - 2));
			d.d_type = VREG;
			if ((error = uiomove(&d, UIO_MX, uio)) != 0)
				break;
			if (cookies)
				*cookies++ = i + 1;
			nc++;
		}
		ncookies = nc;
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

	case PFSroot: {
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
				d.d_fileno = PROCFS_FILENO(0, PFSroot, -1);
				d.d_namlen = i + 1;
				memcpy(d.d_name, "..", d.d_namlen);
				d.d_name[i + 1] = '\0';
				d.d_type = DT_DIR;
				break;

			case 2:
				d.d_fileno = PROCFS_FILENO(0, PFScurproc, -1);
				d.d_namlen = sizeof("curproc") - 1;
				memcpy(d.d_name, "curproc", sizeof("curproc"));
				d.d_type = DT_LNK;
				break;

			case 3:
				d.d_fileno = PROCFS_FILENO(0, PFSself, -1);
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
				d.d_fileno = PROCFS_FILENO(p->p_pid, PFSproc, -1);
				d.d_namlen = snprintf(d.d_name,
				    sizeof(d.d_name), "%ld", (long)p->p_pid);
				d.d_type = DT_DIR;
				p = p->p_list.le_next;
				break;
			}

			if ((error = uiomove(&d, UIO_MX, uio)) != 0)
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
			d.d_fileno = PROCFS_FILENO(0, pt->pt_pfstype, -1);
			d.d_namlen = pt->pt_namlen;
			memcpy(d.d_name, pt->pt_name, pt->pt_namlen + 1);
			d.d_type = pt->pt_type;

			if ((error = uiomove(&d, UIO_MX, uio)) != 0)
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
	char *bp = buf;
	char *path = NULL;
	int len;
	int error = 0;
	struct pfsnode *pfs = VTOPFS(ap->a_vp);

	if (pfs->pfs_fileno == PROCFS_FILENO(0, PFScurproc, -1))
		len = snprintf(buf, sizeof(buf), "%ld", (long)curproc->p_pid);
	else if (pfs->pfs_fileno == PROCFS_FILENO(0, PFSself, -1))
		len = snprintf(buf, sizeof(buf), "%s", "curproc");
	else {
		struct file *fp;
		struct proc *pown;
		struct vnode *vxp, *vp;

		if ((error = procfs_getfp(pfs, &pown, &fp)) != 0)
			return error;
		FILE_USE(fp);
		switch (fp->f_type) {
		case DTYPE_VNODE:
			vxp = (struct vnode *)fp->f_data;
			if (vxp->v_type != VDIR) {
				FILE_UNUSE(fp, proc_representative_lwp(pown));
				return EINVAL;
			}
			if ((path = malloc(MAXPATHLEN, M_TEMP, M_WAITOK))
			    == NULL) {
				FILE_UNUSE(fp, proc_representative_lwp(pown));
				return ENOMEM;
			}
			bp = path + MAXPATHLEN;
			*--bp = '\0';
			vp = curproc->p_cwdi->cwdi_rdir;
			if (vp == NULL)
				vp = rootvnode;
			error = getcwd_common(vxp, vp, &bp, path,
			    MAXPATHLEN / 2, 0, curlwp);
			FILE_UNUSE(fp, proc_representative_lwp(pown));
			if (error) {
				free(path, M_TEMP);
				return error;
			}
			len = strlen(bp);
			break;

		case DTYPE_MISC:
			len = snprintf(buf, sizeof(buf), "%s", "[misc]");
			break;

		case DTYPE_KQUEUE:
			len = snprintf(buf, sizeof(buf), "%s", "[kqueue]");
			break;

		default:
			return EINVAL;
		}
	}

	error = uiomove(bp, len, ap->a_uio);
	if (path)
		free(path, M_TEMP);
	return error;
}

/*
 * convert decimal ascii to int
 */
static int
atoi(b, len)
	const char *b;
	size_t len;
{
	int p = 0;

	while (len--) {
		char c = *b++;
		if (c < '0' || c > '9')
			return -1;
		p = 10 * p + (c - '0');
	}

	return p;
}
