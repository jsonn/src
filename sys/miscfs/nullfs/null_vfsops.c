/*	$NetBSD: null_vfsops.c,v 1.15.4.2 1997/10/14 10:28:31 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
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
 *	@(#)null_vfsops.c	8.2 (Berkeley) 1/21/94
 */

/*
 * Null Layer
 * (See null_vnops.c for a description of what this does.)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <miscfs/nullfs/null.h>

int	nullfs_mount __P((struct mount *, const char *, void *,
			  struct nameidata *, struct proc *));
int	nullfs_start __P((struct mount *, int, struct proc *));
int	nullfs_unmount __P((struct mount *, int, struct proc *));
int	nullfs_root __P((struct mount *, struct vnode **));
int	nullfs_quotactl __P((struct mount *, int, uid_t, caddr_t,
			     struct proc *));
int	nullfs_statfs __P((struct mount *, struct statfs *, struct proc *));
int	nullfs_sync __P((struct mount *, int, struct ucred *, struct proc *));
int	nullfs_vget __P((struct mount *, ino_t, struct vnode **));
int	nullfs_fhtovp __P((struct mount *, struct fid *, struct mbuf *,
			   struct vnode **, int *, struct ucred **));
int	nullfs_vptofh __P((struct vnode *, struct fid *));
/*
 * Mount null layer
 */
int
nullfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	const char *path;
	void *data;
	struct nameidata *ndp;
	struct proc *p;
{
	int error = 0;
	struct null_args args;
	struct vnode *lowerrootvp, *vp;
	struct vnode *nullm_rootvp;
	struct null_mount *xmp;
	size_t size;

#ifdef NULLFS_DIAGNOSTIC
	printf("nullfs_mount(mp = %p)\n", mp);
#endif

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		return (EOPNOTSUPP);
		/* return VFS_MOUNT(MOUNTTONULLMOUNT(mp)->nullm_vfs, path, data, ndp, p);*/
	}

	/*
	 * Get argument
	 */
	error = copyin(data, (caddr_t)&args, sizeof(struct null_args));
	if (error)
		return (error);

	/*
	 * Find lower node
	 */
	NDINIT(ndp, LOOKUP, FOLLOW|WANTPARENT|LOCKLEAF,
		UIO_USERSPACE, args.target, p);
	if ((error = namei(ndp)) != 0)
		return (error);

	/*
	 * Sanity check on lower vnode
	 */
	lowerrootvp = ndp->ni_vp;

	vrele(ndp->ni_dvp);
	ndp->ni_dvp = NULL;

	xmp = (struct null_mount *) malloc(sizeof(struct null_mount),
				M_UFSMNT, M_WAITOK);	/* XXX */

	/*
	 * Save reference to underlying FS
	 */
	xmp->nullm_vfs = lowerrootvp->v_mount;

	/*
	 * Save reference.  Each mount also holds
	 * a reference on the root vnode.
	 */
	error = null_node_create(mp, lowerrootvp, &vp, 1);
	/*
	 * Unlock the node (either the lower or the alias)
	 */
	VOP_UNLOCK(vp);
	/*
	 * Make sure the node alias worked
	 */
	if (error) {
		vrele(lowerrootvp);
		free(xmp, M_UFSMNT);	/* XXX */
		return (error);
	}

	/*
	 * Keep a held reference to the root vnode.
	 * It is vrele'd in nullfs_unmount.
	 */
	nullm_rootvp = vp;
	nullm_rootvp->v_flag |= VROOT;
	xmp->nullm_rootvp = nullm_rootvp;
	if (NULLVPTOLOWERVP(nullm_rootvp)->v_mount->mnt_flag & MNT_LOCAL)
		mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t) xmp;
	getnewfsid(mp, makefstype(MOUNT_NULL));

	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	(void) copyinstr(args.target, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, 
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
#ifdef NULLFS_DIAGNOSTIC
	printf("nullfs_mount: lower %s, alias at %s\n",
	    mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);
#endif
	return (0);
}

/*
 * VFS start.  Nothing needed here - the start routine
 * on the underlying filesystem will have been called
 * when that filesystem was mounted.
 */
int
nullfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{

	return (0);
	/* return VFS_START(MOUNTTONULLMOUNT(mp)->nullm_vfs, flags, p); */
}

/*
 * Free reference to null layer
 */
int
nullfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct vnode *nullm_rootvp = MOUNTTONULLMOUNT(mp)->nullm_rootvp;
	int error;
	int flags = 0;
	extern int doforce;

#ifdef NULLFS_DIAGNOSTIC
	printf("nullfs_unmount(mp = %p)\n", mp);
#endif

	if (mntflags & MNT_FORCE) {
		/* lofs can never be rootfs so don't check for it */
		if (!doforce)
			return (EINVAL);
		flags |= FORCECLOSE;
	}

	/*
	 * Clear out buffer cache.  I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 */
#if 0
	mntflushbuf(mp, 0); 
	if (mntinvalbuf(mp, 1))
		return (EBUSY);
#endif
	if (nullm_rootvp->v_usecount > 1)
		return (EBUSY);
	if ((error = vflush(mp, nullm_rootvp, flags)) != 0)
		return (error);

#ifdef NULLFS_DIAGNOSTIC
	vprint("alias root of lower", nullm_rootvp);
#endif	 
	/*
	 * Release reference on underlying root vnode
	 */
	vrele(nullm_rootvp);
	/*
	 * And blow it away for future re-use
	 */
	vgone(nullm_rootvp);
	/*
	 * Finally, throw away the null_mount structure
	 */
	free(mp->mnt_data, M_UFSMNT);	/* XXX */
	mp->mnt_data = 0;
	return 0;
}

int
nullfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct vnode *vp;

#ifdef NULLFS_DIAGNOSTIC
	printf("nullfs_root(mp = %p, vp = %p->%p)\n", mp,
	    MOUNTTONULLMOUNT(mp)->nullm_rootvp,
	    NULLVPTOLOWERVP(MOUNTTONULLMOUNT(mp)->nullm_rootvp));
#endif

	/*
	 * Return locked reference to root.
	 */
	vp = MOUNTTONULLMOUNT(mp)->nullm_rootvp;
	VREF(vp);
	VOP_LOCK(vp);
	*vpp = vp;
	return 0;
}

int
nullfs_quotactl(mp, cmd, uid, arg, p)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{

	return VFS_QUOTACTL(MOUNTTONULLMOUNT(mp)->nullm_vfs, cmd, uid, arg, p);
}

int
nullfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	int error;
	struct statfs mstat;

#ifdef NULLFS_DIAGNOSTIC
	printf("nullfs_statfs(mp = %p, vp = %p->%p)\n", mp,
	    MOUNTTONULLMOUNT(mp)->nullm_rootvp,
	    NULLVPTOLOWERVP(MOUNTTONULLMOUNT(mp)->nullm_rootvp));
#endif

	bzero(&mstat, sizeof(mstat));

	error = VFS_STATFS(MOUNTTONULLMOUNT(mp)->nullm_vfs, &mstat, p);
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
	if (sbp != &mp->mnt_stat) {
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(sbp->f_fstypename, mp->mnt_op->vfs_name, MFSNAMELEN);
	return (0);
}

int
nullfs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{

	/*
	 * XXX - Assumes no data cached at null layer.
	 */
	return (0);
}

int
nullfs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	
	return VFS_VGET(MOUNTTONULLMOUNT(mp)->nullm_vfs, ino, vpp);
}

int
nullfs_fhtovp(mp, fidp, nam, vpp, exflagsp, credanonp)
	struct mount *mp;
	struct fid *fidp;
	struct mbuf *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred**credanonp;
{

	return (EOPNOTSUPP);
}

int
nullfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{

	return (EOPNOTSUPP);
}

struct vfsops nullfs_vfsops = {
	MOUNT_NULL,
	nullfs_mount,
	nullfs_start,
	nullfs_unmount,
	nullfs_root,
	nullfs_quotactl,
	nullfs_statfs,
	nullfs_sync,
	nullfs_vget,
	nullfs_fhtovp,
	nullfs_vptofh,
	nullfs_init,
};
