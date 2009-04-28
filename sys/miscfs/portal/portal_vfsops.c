/*	$NetBSD: portal_vfsops.c,v 1.76.4.2 2009/04/28 07:37:15 skrll Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
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
 *	from: Id: portal_vfsops.c,v 1.5 1992/05/30 10:25:27 jsp Exp
 *	@(#)portal_vfsops.c	8.11 (Berkeley) 5/14/95
 */

/*
 * Portal Filesystem
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: portal_vfsops.c,v 1.76.4.2 2009/04/28 07:37:15 skrll Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/dirent.h>
#include <sys/un.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <miscfs/genfs/genfs.h>

#include <miscfs/portal/portal.h>

MODULE(MODULE_CLASS_VFS, portal, NULL);

VFS_PROTOS(portal);

static struct sysctllog *portal_sysctl_log;

void
portal_init(void)
{
}

void
portal_done(void)
{
}

/*
 * Mount the per-process file descriptors (/dev/fd)
 */
int
portal_mount(
    struct mount *mp,
    const char *path,
    void *data,
    size_t *data_len)
{
	struct lwp *l = curlwp;
	struct file *fp;
	struct portal_args *args = data;
	struct portalmount *fmp;
	struct socket *so;
	struct vnode *rvp;
	struct proc *p;
	int error;

	if (*data_len < sizeof *args)
		return EINVAL;

	p = l->l_proc;
	if (mp->mnt_flag & MNT_GETARGS) {
		fmp = VFSTOPORTAL(mp);
		if (fmp == NULL)
			return EIO;
		args->pa_config = NULL;
		args->pa_socket = 0;	/* XXX */
		*data_len = sizeof *args;
		return 0;
	}
	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	/* getsock() will use the descriptor for us */
	if ((fp = fd_getfile(args->pa_socket)) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_SOCKET) {
		fd_putfile(args->pa_socket);
		return (ENOTSOCK);
	}
	so = (struct socket *) fp->f_data;
	if (so->so_proto->pr_domain->dom_family != AF_LOCAL) {
		fd_putfile(args->pa_socket);
		return (ESOCKTNOSUPPORT);
	}

	error = getnewvnode(VT_PORTAL, mp, portal_vnodeop_p, &rvp); /* XXX */
	if (error) {
		fd_putfile(args->pa_socket);
		return (error);
	}
	rvp->v_data = malloc(sizeof(struct portalnode), M_TEMP, M_WAITOK);

	fmp = (struct portalmount *) malloc(sizeof(struct portalmount),
				 M_UFSMNT, M_WAITOK);	/* XXX */
	rvp->v_type = VDIR;
	rvp->v_vflag |= VV_ROOT;
	VTOPORTAL(rvp)->pt_arg = 0;
	VTOPORTAL(rvp)->pt_size = 0;
	VTOPORTAL(rvp)->pt_fileid = PORTAL_ROOTFILEID;
	fmp->pm_root = rvp;
	fmp->pm_server = fp;
	mutex_enter(&fp->f_lock);
	fp->f_count++;
	mutex_exit(&fp->f_lock);
	fd_putfile(args->pa_socket);

	mp->mnt_stat.f_namemax = MAXNAMLEN;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = fmp;
	vfs_getnewfsid(mp);

	return set_statvfs_info(path, UIO_USERSPACE, args->pa_config,
	    UIO_USERSPACE, mp->mnt_op->vfs_name, mp, l);
}

int
portal_start(struct mount *mp, int flags)
{

	return (0);
}

int
portal_unmount(struct mount *mp, int mntflags)
{
	struct vnode *rtvp = VFSTOPORTAL(mp)->pm_root;
	int error, flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if (rtvp->v_usecount > 1 && (mntflags & MNT_FORCE) == 0)
		return (EBUSY);
	if ((error = vflush(mp, rtvp, flags)) != 0)
		return (error);

	/*
	 * Blow it away for future re-use
	 */
	vgone(rtvp);
	/*
	 * Shutdown the socket.  This will cause the select in the
	 * daemon to wake up, and then the accept will get ECONNABORTED
	 * which it interprets as a request to go and bury itself.
	 */
	soshutdown((struct socket *) VFSTOPORTAL(mp)->pm_server->f_data, 2);
	/*
	 * Discard reference to underlying file.  Must call closef because
	 * this may be the last reference.
	 */
	closef(VFSTOPORTAL(mp)->pm_server);
	/*
	 * Finally, throw away the portalmount structure
	 */
	free(mp->mnt_data, M_UFSMNT);	/* XXX */
	mp->mnt_data = NULL;
	return (0);
}

int
portal_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *vp;

	/*
	 * Return locked reference to root.
	 */
	vp = VFSTOPORTAL(mp)->pm_root;
	VREF(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	*vpp = vp;
	return (0);
}

int
portal_statvfs(struct mount *mp, struct statvfs *sbp)
{

	sbp->f_bsize = DEV_BSIZE;
	sbp->f_frsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
	sbp->f_blocks = 2;		/* 1K to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_bresvd = 0;
	sbp->f_files = 1;		/* Allow for "." */
	sbp->f_ffree = 0;		/* See comments above */
	sbp->f_favail = 0;		/* See comments above */
	sbp->f_fresvd = 0;
	copy_statvfs_info(sbp, mp);
	return (0);
}

/*ARGSUSED*/
int
portal_sync(struct mount *mp, int waitfor,
    kauth_cred_t uc)
{

	return (0);
}

int
portal_vget(struct mount *mp, ino_t ino,
    struct vnode **vpp)
{

	return (EOPNOTSUPP);
}

extern const struct vnodeopv_desc portal_vnodeop_opv_desc;

const struct vnodeopv_desc * const portal_vnodeopv_descs[] = {
	&portal_vnodeop_opv_desc,
	NULL,
};

struct vfsops portal_vfsops = {
	MOUNT_PORTAL,
	sizeof (struct portal_args),
	portal_mount,
	portal_start,
	portal_unmount,
	portal_root,
	(void *)eopnotsupp,		/* vfs_quotactl */
	portal_statvfs,
	portal_sync,
	portal_vget,
	(void *)eopnotsupp,		/* vfs_fhtovp */
	(void *)eopnotsupp,		/* vfs_vptofh */
	portal_init,
	NULL,
	portal_done,
	NULL,				/* vfs_mountroot */
	(int (*)(struct mount *, struct vnode *, struct timespec *)) eopnotsupp,
	vfs_stdextattrctl,
	(void *)eopnotsupp,		/* vfs_suspendctl */
	genfs_renamelock_enter,
	genfs_renamelock_exit,
	(void *)eopnotsupp,
	portal_vnodeopv_descs,
	0,
	{ NULL, NULL },
};

static int
portal_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&portal_vfsops);
		if (error != 0)
			break;
		sysctl_createv(&portal_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "vfs", NULL,
			       NULL, 0, NULL, 0,
			       CTL_VFS, CTL_EOL);
		sysctl_createv(&portal_sysctl_log, 0, NULL, NULL,
			       CTLFLAG_PERMANENT,
			       CTLTYPE_NODE, "portal",
			       SYSCTL_DESCR("Portal daemon file system"),
			       NULL, 0, NULL, 0,
			       CTL_VFS, 8, CTL_EOL);
		/*
		 * XXX the "8" above could be dynamic, thereby eliminating one
		 * more instance of the "number to vfs" mapping problem, but
		 * "8" is the order as taken from sys/mount.h
		 */
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&portal_vfsops);
		if (error != 0)
			break;
		sysctl_teardown(&portal_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}
