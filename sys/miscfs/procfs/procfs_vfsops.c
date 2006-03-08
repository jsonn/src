/*	$NetBSD: procfs_vfsops.c,v 1.63.10.1 2006/03/08 01:34:34 elad Exp $	*/

/*
 * Copyright (c) 1993
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
 *	@(#)procfs_vfsops.c	8.7 (Berkeley) 5/10/95
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
 *	@(#)procfs_vfsops.c	8.7 (Berkeley) 5/10/95
 */

/*
 * procfs VFS interface
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: procfs_vfsops.c,v 1.63.10.1 2006/03/08 01:34:34 elad Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/syslog.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <miscfs/procfs/procfs.h>

#include <uvm/uvm_extern.h>			/* for PAGE_SIZE */

void	procfs_init(void);
void	procfs_reinit(void);
void	procfs_done(void);
int	procfs_mount(struct mount *, const char *, void *,
			  struct nameidata *, struct lwp *);
int	procfs_start(struct mount *, int, struct lwp *);
int	procfs_unmount(struct mount *, int, struct lwp *);
int	procfs_quotactl(struct mount *, int, uid_t, void *,
			     struct lwp *);
int	procfs_statvfs(struct mount *, struct statvfs *, struct lwp *);
int	procfs_sync(struct mount *, int, kauth_cred_t, struct lwp *);
int	procfs_vget(struct mount *, ino_t, struct vnode **);

/*
 * VFS Operations.
 *
 * mount system call
 */
/* ARGSUSED */
int
procfs_mount(mp, path, data, ndp, l)
	struct mount *mp;
	const char *path;
	void *data;
	struct nameidata *ndp;
	struct lwp *l;
{
	struct procfsmount *pmnt;
	struct procfs_args args;
	int error;

	if (UIO_MX & (UIO_MX-1)) {
		log(LOG_ERR, "procfs: invalid directory entry size");
		return (EINVAL);
	}

	if (mp->mnt_flag & MNT_GETARGS) {
		pmnt = VFSTOPROC(mp);
		if (pmnt == NULL)
			return EIO;
		args.version = PROCFS_ARGSVERSION;
		args.flags = pmnt->pmnt_flags;
		return copyout(&args, data, sizeof(args));
	}

	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	if (data != NULL) {
		error = copyin(data, &args, sizeof args);
		if (error != 0)
			return error;

		if (args.version != PROCFS_ARGSVERSION)
			return EINVAL;
	} else
		args.flags = 0;

	pmnt = (struct procfsmount *) malloc(sizeof(struct procfsmount),
	    M_UFSMNT, M_WAITOK);   /* XXX need new malloc type */

	mp->mnt_stat.f_namemax = MAXNAMLEN;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = pmnt;
	vfs_getnewfsid(mp);

	error = set_statvfs_info(path, UIO_USERSPACE, "procfs", UIO_SYSSPACE,
	    mp, l);
	pmnt->pmnt_exechook = exechook_establish(procfs_revoke_vnodes, mp);
	pmnt->pmnt_flags = args.flags;

	return error;
}

/*
 * unmount system call
 */
int
procfs_unmount(mp, mntflags, l)
	struct mount *mp;
	int mntflags;
	struct lwp *l;
{
	int error;
	int flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if ((error = vflush(mp, 0, flags)) != 0)
		return (error);

	exechook_disestablish(VFSTOPROC(mp)->pmnt_exechook);

	free(mp->mnt_data, M_UFSMNT);
	mp->mnt_data = 0;

	return (0);
}

int
procfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{

	return (procfs_allocvp(mp, vpp, 0, PFSroot, -1));
}

/* ARGSUSED */
int
procfs_start(mp, flags, l)
	struct mount *mp;
	int flags;
	struct lwp *l;
{

	return (0);
}

/*
 * Get file system statistics.
 */
int
procfs_statvfs(mp, sbp, l)
	struct mount *mp;
	struct statvfs *sbp;
	struct lwp *l;
{

	sbp->f_bsize = PAGE_SIZE;
	sbp->f_frsize = PAGE_SIZE;
	sbp->f_iosize = PAGE_SIZE;
	sbp->f_blocks = 1;	/* avoid divide by zero in some df's */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_bresvd = 0;
	sbp->f_files = maxproc;			/* approx */
	sbp->f_ffree = maxproc - nprocs;	/* approx */
	sbp->f_favail = maxproc - nprocs;	/* approx */
	sbp->f_fresvd = 0;
	copy_statvfs_info(sbp, mp);
	return (0);
}

/*ARGSUSED*/
int
procfs_quotactl(mp, cmds, uid, arg, l)
	struct mount *mp;
	int cmds;
	uid_t uid;
	void *arg;
	struct lwp *l;
{

	return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
procfs_sync(mp, waitfor, uc, l)
	struct mount *mp;
	int waitfor;
	kauth_cred_t uc;
	struct lwp *l;
{

	return (0);
}

/*ARGSUSED*/
int
procfs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}

void
procfs_init()
{
	procfs_hashinit();
}

void
procfs_reinit()
{
	procfs_hashreinit();
}

void
procfs_done()
{
	procfs_hashdone();
}

SYSCTL_SETUP(sysctl_vfs_procfs_setup, "sysctl vfs.procfs subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "vfs", NULL,
		       NULL, 0, NULL, 0,
		       CTL_VFS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "procfs",
		       SYSCTL_DESCR("Process file system"),
		       NULL, 0, NULL, 0,
		       CTL_VFS, 12, CTL_EOL);
	/*
	 * XXX the "12" above could be dynamic, thereby eliminating
	 * one more instance of the "number to vfs" mapping problem,
	 * but "12" is the order as taken from sys/mount.h
	 */
}

extern const struct vnodeopv_desc procfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const procfs_vnodeopv_descs[] = {
	&procfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops procfs_vfsops = {
	MOUNT_PROCFS,
	procfs_mount,
	procfs_start,
	procfs_unmount,
	procfs_root,
	procfs_quotactl,
	procfs_statvfs,
	procfs_sync,
	procfs_vget,
	NULL,				/* vfs_fhtovp */
	NULL,				/* vfs_vptofh */
	procfs_init,
	procfs_reinit,
	procfs_done,
	NULL,				/* vfs_mountroot */
	(int (*)(struct mount *, struct vnode *, struct timespec *)) eopnotsupp,
	vfs_stdextattrctl,
	procfs_vnodeopv_descs,
};
VFS_ATTACH(procfs_vfsops);
