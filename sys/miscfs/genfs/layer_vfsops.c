/*	$NetBSD: layer_vfsops.c,v 1.9.2.1 2003/07/02 15:26:50 darrenr Exp $	*/

/*
 * Copyright (c) 1999 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * This software was written by William Studenmund of the
 * Numerical Aerospace Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the National Aeronautics & Space Administration
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NATIONAL AERONAUTICS & SPACE ADMINISTRATION
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ADMINISTRATION OR CONTRIB-
 * UTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
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
 *	from: Id: lofs_vfsops.c,v 1.9 1992/05/30 10:26:24 jsp Exp
 *	from: @(#)lofs_vfsops.c	1.2 (Berkeley) 6/18/92
 *	@(#)null_vfsops.c	8.7 (Berkeley) 5/14/95
 */

/*
 * generic layer vfs ops.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: layer_vfsops.c,v 1.9.2.1 2003/07/02 15:26:50 darrenr Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <miscfs/genfs/layer.h>
#include <miscfs/genfs/layer_extern.h>

/*
 * VFS start.  Nothing needed here - the start routine
 * on the underlying filesystem will have been called
 * when that filesystem was mounted.
 */
int
layerfs_start(mp, flags, l)
	struct mount *mp;
	int flags;
	struct lwp *l;
{

	return (0);
	/* return VFS_START(MOUNTTOLAYERMOUNT(mp)->layerm_vfs, flags, l); */
}

int
layerfs_root(mp, vpp, l)
	struct mount *mp;
	struct vnode **vpp;
	struct lwp *l;
{
	struct vnode *vp;

#ifdef LAYERFS_DIAGNOSTIC
	printf("layerfs_root(mp = %p, vp = %p->%p)\n", mp,
	    MOUNTTOLAYERMOUNT(mp)->layerm_rootvp,
	    LAYERVPTOLOWERVP(MOUNTTOLAYERMOUNT(mp)->layerm_rootvp));
#endif

	/*
	 * Return locked reference to root.
	 */
	vp = MOUNTTOLAYERMOUNT(mp)->layerm_rootvp;
	if (vp == NULL) {
		*vpp = NULL;
		return (EINVAL);
	}
	VREF(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	*vpp = vp;
	return 0;
}

int
layerfs_quotactl(mp, cmd, uid, arg, l)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct lwp *l;
{

	return VFS_QUOTACTL(MOUNTTOLAYERMOUNT(mp)->layerm_vfs,
				cmd, uid, arg, l);
}

int
layerfs_statfs(mp, sbp, l)
	struct mount *mp;
	struct statfs *sbp;
	struct lwp *l;
{
	int error;
	struct statfs mstat;

#ifdef LAYERFS_DIAGNOSTIC
	printf("layerfs_statfs(mp = %p, vp = %p->%p)\n", mp,
	    MOUNTTOLAYERMOUNT(mp)->layerm_rootvp,
	    LAYERVPTOLOWERVP(MOUNTTOLAYERMOUNT(mp)->layerm_rootvp));
#endif

	memset(&mstat, 0, sizeof(mstat));

	error = VFS_STATFS(MOUNTTOLAYERMOUNT(mp)->layerm_vfs, &mstat, l);
	if (error)
		return (error);

	/* now copy across the "interesting" information and fake the rest */
	sbp->f_type = mstat.f_type;
	sbp->f_flags = mstat.f_flags;
	sbp->f_bsize = mstat.f_bsize;
	sbp->f_iosize = mstat.f_iosize;
	sbp->f_blocks = mstat.f_blocks;
	sbp->f_bfree = mstat.f_bfree;
	sbp->f_bavail = mstat.f_bavail;
	sbp->f_files = mstat.f_files;
	sbp->f_ffree = mstat.f_ffree;
	copy_statfs_info(sbp, mp);
	return (0);
}

int
layerfs_sync(mp, waitfor, cred, l)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct lwp *l;
{

	/*
	 * XXX - Assumes no data cached at layer.
	 */
	return (0);
}

int
layerfs_vget(mp, ino, vpp, l)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
	struct lwp *l;
{
	int error;
	struct vnode *vp;

	if ((error = VFS_VGET(MOUNTTOLAYERMOUNT(mp)->layerm_vfs,
	    ino, &vp, l))) {
		*vpp = NULL;
		return (error);
	}
	if ((error = layer_node_create(mp, vp, vpp))) {
		vput(vp);
		*vpp = NULL;
		return (error);
	}

	return (0);
}

int
layerfs_fhtovp(mp, fidp, vpp, l)
	struct mount *mp;
	struct fid *fidp;
	struct vnode **vpp;
	struct lwp *l;
{
	int error;
	struct vnode *vp;

	if ((error = VFS_FHTOVP(MOUNTTOLAYERMOUNT(mp)->layerm_vfs,
	    fidp, &vp, l)))
		return (error);

	if ((error = layer_node_create(mp, vp, vpp))) {
		vput(vp);
		*vpp = NULL;
		return (error);
	}

	return (0);
}

int
layerfs_checkexp(mp, nam, exflagsp, credanonp)
	struct mount *mp;
	struct mbuf *nam;
	int *exflagsp;
	struct ucred**credanonp;
{
	struct	netcred *np;
	struct	layer_mount *lmp = MOUNTTOLAYERMOUNT(mp);

	/*
	 * get the export permission structure for this <mp, client> tuple.
	 */
	if ((np = vfs_export_lookup(mp, &lmp->layerm_export, nam)) == NULL)
		return (EACCES);

	*exflagsp = np->netc_exflags;
	*credanonp = &np->netc_anon;
	return (0);
}

int
layerfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{

	return (VFS_VPTOFH(LAYERVPTOLOWERVP(vp), fhp));
}

int
layerfs_sysctl(name, namelen, oldp, oldlenp, newp, newlen, l)
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
