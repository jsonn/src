/*	$NetBSD: portal_vfsops.c,v 1.37.2.2 2003/07/03 01:23:44 wrstuden Exp $	*/

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
 *	from: Id: portal_vfsops.c,v 1.5 1992/05/30 10:25:27 jsp Exp
 *	@(#)portal_vfsops.c	8.11 (Berkeley) 5/14/95
 */

/*
 * Portal Filesystem
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: portal_vfsops.c,v 1.37.2.2 2003/07/03 01:23:44 wrstuden Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
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
#include <sys/un.h>
#include <miscfs/portal/portal.h>

void	portal_init __P((void));
void	portal_done __P((void));
int	portal_mount __P((struct mount *, const char *, void *,
			  struct nameidata *, struct lwp *));
int	portal_start __P((struct mount *, int, struct lwp *));
int	portal_unmount __P((struct mount *, int, struct lwp *));
int	portal_root __P((struct mount *, struct vnode **, struct lwp *));
int	portal_quotactl __P((struct mount *, int, uid_t, caddr_t,
			     struct lwp *));
int	portal_statfs __P((struct mount *, struct statfs *, struct lwp *));
int	portal_sync __P((struct mount *, int, struct ucred *, struct lwp *));
int	portal_vget __P((struct mount *, ino_t, struct vnode **, struct lwp *));
int	portal_fhtovp __P((struct mount *, struct fid *, struct vnode **, struct lwp *));
int	portal_checkexp __P((struct mount *, struct mbuf *, int *,
			   struct ucred **));
int	portal_vptofh __P((struct vnode *, struct fid *));
int	portal_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
			   struct lwp *));

void
portal_init()
{
}

void
portal_done()
{
}

/*
 * Mount the per-process file descriptors (/dev/fd)
 */
int
portal_mount(mp, path, data, ndp, l)
	struct mount *mp;
	const char *path;
	void *data;
	struct nameidata *ndp;
	struct lwp *l;
{
	struct file *fp;
	struct portal_args args;
	struct portalmount *fmp;
	struct socket *so;
	struct vnode *rvp;
	struct proc *p;
	int error;

	p = l->l_proc;
	if (mp->mnt_flag & MNT_GETARGS) {
		fmp = VFSTOPORTAL(mp);
		if (fmp == NULL)
			return EIO;
		args.pa_config = NULL;
		args.pa_socket = 0;	/* XXX */
		return copyout(&args, data, sizeof(args));
	}
	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	error = copyin(data, (caddr_t) &args, sizeof(struct portal_args));
	if (error)
		return (error);

	/* getsock() will use the descriptor for us */
	if ((error = getsock(p->p_fd, args.pa_socket, &fp)) != 0)
		return (error);
	so = (struct socket *) fp->f_data;
	FILE_UNUSE(fp, NULL);
	if (so->so_proto->pr_domain->dom_family != AF_LOCAL)
		return (ESOCKTNOSUPPORT);

	error = getnewvnode(VT_PORTAL, mp, portal_vnodeop_p, &rvp); /* XXX */
	if (error)
		return (error);
	MALLOC(rvp->v_data, void *, sizeof(struct portalnode),
		M_TEMP, M_WAITOK);

	fmp = (struct portalmount *) malloc(sizeof(struct portalmount),
				 M_UFSMNT, M_WAITOK);	/* XXX */
	rvp->v_type = VDIR;
	rvp->v_flag |= VROOT;
	VTOPORTAL(rvp)->pt_arg = 0;
	VTOPORTAL(rvp)->pt_size = 0;
	VTOPORTAL(rvp)->pt_fileid = PORTAL_ROOTFILEID;
	fmp->pm_root = rvp;
	fmp->pm_server = fp;
	simple_lock(&fp->f_slock);
	fp->f_count++;
	simple_unlock(&fp->f_slock);

	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = fmp;
	vfs_getnewfsid(mp);

	return set_statfs_info(path, UIO_USERSPACE, args.pa_config,
	    UIO_USERSPACE, mp, l);
}

int
portal_start(mp, flags, l)
	struct mount *mp;
	int flags;
	struct lwp *l;
{

	return (0);
}

int
portal_unmount(mp, mntflags, l)
	struct mount *mp;
	int mntflags;
	struct lwp *l;
{
	struct vnode *rootvp = VFSTOPORTAL(mp)->pm_root;
	int error, flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/*
	 * Clear out buffer cache.  I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 */
#ifdef notyet
	mntflushbuf(mp, 0); 
	if (mntinvalbuf(mp, 1))
		return (EBUSY);
#endif
	if (rootvp->v_usecount > 1)
		return (EBUSY);
	if ((error = vflush(mp, rootvp, flags)) != 0)
		return (error);

	/*
	 * Release reference on underlying root vnode
	 */
	vrele(rootvp);
	/*
	 * And blow it away for future re-use
	 */
	vgone(rootvp);
	/*
	 * Shutdown the socket.  This will cause the select in the
	 * daemon to wake up, and then the accept will get ECONNABORTED
	 * which it interprets as a request to go and bury itself.
	 */
	simple_lock(&VFSTOPORTAL(mp)->pm_server->f_slock);
	FILE_USE(VFSTOPORTAL(mp)->pm_server);
	soshutdown((struct socket *) VFSTOPORTAL(mp)->pm_server->f_data, 2);
	/*
	 * Discard reference to underlying file.  Must call closef because
	 * this may be the last reference.
	 */
	closef(VFSTOPORTAL(mp)->pm_server, (struct lwp *) 0);
	/*
	 * Finally, throw away the portalmount structure
	 */
	free(mp->mnt_data, M_UFSMNT);	/* XXX */
	mp->mnt_data = 0;
	return (0);
}

int
portal_root(mp, vpp, l)
	struct mount *mp;
	struct vnode **vpp;
	struct lwp *l;
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
portal_quotactl(mp, cmd, uid, arg, l)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct lwp *l;
{

	return (EOPNOTSUPP);
}

int
portal_statfs(mp, sbp, l)
	struct mount *mp;
	struct statfs *sbp;
	struct lwp *l;
{

	sbp->f_bsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
	sbp->f_blocks = 2;		/* 1K to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 1;		/* Allow for "." */
	sbp->f_ffree = 0;		/* See comments above */
#ifdef COMPAT_09
	sbp->f_type = 12;
#else
	sbp->f_type = 0;
#endif
	copy_statfs_info(sbp, mp);
	return (0);
}

/*ARGSUSED*/
int
portal_sync(mp, waitfor, uc, l)
	struct mount *mp;
	int waitfor;
	struct ucred *uc;
	struct lwp *l;
{

	return (0);
}

int
portal_vget(mp, ino, vpp, l)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
	struct lwp *l;
{

	return (EOPNOTSUPP);
}

int
portal_fhtovp(mp, fhp, vpp, l)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
	struct lwp *l;
{

	return (EOPNOTSUPP);
}

int
portal_checkexp(mp, mb, what, anon)
	struct mount *mp;
	struct mbuf *mb;
	int *what;
	struct ucred **anon;
{

	return (EOPNOTSUPP);
}

int
portal_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{

	return (EOPNOTSUPP);
}

int
portal_sysctl(name, namelen, oldp, oldlenp, newp, newlen, l)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct lwp *l;
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
	portal_mount,
	portal_start,
	portal_unmount,
	portal_root,
	portal_quotactl,
	portal_statfs,
	portal_sync,
	portal_vget,
	portal_fhtovp,
	portal_vptofh,
	portal_init,
	NULL,
	portal_done,
	portal_sysctl,
	NULL,				/* vfs_mountroot */
	portal_checkexp,
	portal_vnodeopv_descs,
};
