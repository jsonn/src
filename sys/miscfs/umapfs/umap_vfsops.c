/*	$NetBSD: umap_vfsops.c,v 1.14.4.1 1997/09/16 03:51:16 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * the UCLA Ficus project.
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
 *	from: @(#)null_vfsops.c       1.5 (Berkeley) 7/10/92
 *	@(#)umap_vfsops.c	8.3 (Berkeley) 1/21/94
 */

/*
 * Umap Layer
 * (See mount_umap(8) for a description of this layer.)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <miscfs/umapfs/umap.h>

int	umapfs_mount __P((struct mount *, const char *, void *,
			  struct nameidata *, struct proc *));
int	umapfs_start __P((struct mount *, int, struct proc *));
int	umapfs_unmount __P((struct mount *, int, struct proc *));
int	umapfs_root __P((struct mount *, struct vnode **));
int	umapfs_quotactl __P((struct mount *, int, uid_t, caddr_t,
			     struct proc *));
int	umapfs_statfs __P((struct mount *, struct statfs *, struct proc *));
int	umapfs_sync __P((struct mount *, int, struct ucred *, struct proc *));
int	umapfs_vget __P((struct mount *, ino_t, struct vnode **));
int	umapfs_fhtovp __P((struct mount *, struct fid *, struct mbuf *,
			   struct vnode **, int *, struct ucred **));
int	umapfs_vptofh __P((struct vnode *, struct fid *));

/*
 * Mount umap layer
 */
int
umapfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	const char *path;
	void *data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct umap_args args;
	struct vnode *lowerrootvp, *vp;
	struct vnode *umapm_rootvp;
	struct umap_mount *amp;
	size_t size;
	int error;

#ifdef UMAPFS_DIAGNOSTIC
	printf("umapfs_mount(mp = %p)\n", mp);
#endif

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		return (EOPNOTSUPP);
		/* return (VFS_MOUNT(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, path, data, ndp, p));*/
	}

	/*
	 * Get argument
	 */
	error = copyin(data, (caddr_t)&args, sizeof(struct umap_args));
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
#ifdef UMAPFS_DIAGNOSTIC
	printf("vp = %p, check for VDIR...\n", lowerrootvp);
#endif
	vrele(ndp->ni_dvp);
	ndp->ni_dvp = 0;

	if (lowerrootvp->v_type != VDIR) {
		vput(lowerrootvp);
		return (EINVAL);
	}

#ifdef UMAPFS_DIAGNOSTIC
	printf("mp = %p\n", mp);
#endif

	amp = (struct umap_mount *) malloc(sizeof(struct umap_mount),
				M_UFSMNT, M_WAITOK);	/* XXX */

	/*
	 * Save reference to underlying FS
	 */
	amp->umapm_vfs = lowerrootvp->v_mount;

	/* 
	 * Now copy in the number of entries and maps for umap mapping.
	 */
	amp->info_nentries = args.nentries;
	amp->info_gnentries = args.gnentries;
	error = copyin(args.mapdata, (caddr_t)amp->info_mapdata, 
	    2*sizeof(u_long)*args.nentries);
	if (error)
		return (error);

#ifdef UMAP_DIAGNOSTIC
	printf("umap_mount:nentries %d\n",args.nentries);
	for (i = 0; i < args.nentries; i++)
		printf("   %d maps to %d\n", amp->info_mapdata[i][0],
	 	    amp->info_mapdata[i][1]);
#endif

	error = copyin(args.gmapdata, (caddr_t)amp->info_gmapdata, 
	    2*sizeof(u_long)*args.gnentries);
	if (error)
		return (error);

#ifdef UMAP_DIAGNOSTIC
	printf("umap_mount:gnentries %d\n",args.gnentries);
	for (i = 0; i < args.gnentries; i++)
		printf("\tgroup %d maps to %d\n", 
		    amp->info_gmapdata[i][0],
	 	    amp->info_gmapdata[i][1]);
#endif


	/*
	 * Save reference.  Each mount also holds
	 * a reference on the root vnode.
	 */
	error = umap_node_create(mp, lowerrootvp, &vp);
	/*
	 * Unlock the node (either the lower or the alias)
	 */
	VOP_UNLOCK(vp);
	/*
	 * Make sure the node alias worked
	 */
	if (error) {
		vrele(lowerrootvp);
		free(amp, M_UFSMNT);	/* XXX */
		return (error);
	}

	/*
	 * Keep a held reference to the root vnode.
	 * It is vrele'd in umapfs_unmount.
	 */
	umapm_rootvp = vp;
	umapm_rootvp->v_flag |= VROOT;
	amp->umapm_rootvp = umapm_rootvp;
	if (UMAPVPTOLOWERVP(umapm_rootvp)->v_mount->mnt_flag & MNT_LOCAL)
		mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t) amp;
	getnewfsid(mp, makefstype(MOUNT_UMAP));

	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	(void) copyinstr(args.target, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, 
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
#ifdef UMAPFS_DIAGNOSTIC
	printf("umapfs_mount: lower %s, alias at %s\n",
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
umapfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{

	return (0);
	/* return (VFS_START(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, flags, p)); */
}

/*
 * Free reference to umap layer
 */
int
umapfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct vnode *umapm_rootvp = MOUNTTOUMAPMOUNT(mp)->umapm_rootvp;
	int error;
	int flags = 0;
	extern int doforce;

#ifdef UMAPFS_DIAGNOSTIC
	printf("umapfs_unmount(mp = %p)\n", mp);
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
#ifdef notyet
	mntflushbuf(mp, 0); 
	if (mntinvalbuf(mp, 1))
		return (EBUSY);
#endif
	if (umapm_rootvp->v_usecount > 1)
		return (EBUSY);
	if ((error = vflush(mp, umapm_rootvp, flags)) != 0)
		return (error);

#ifdef UMAPFS_DIAGNOSTIC
	vprint("alias root of lower", umapm_rootvp);
#endif	 
	/*
	 * Release reference on underlying root vnode
	 */
	vrele(umapm_rootvp);
	/*
	 * And blow it away for future re-use
	 */
	vgone(umapm_rootvp);
	/*
	 * Finally, throw away the umap_mount structure
	 */
	free(mp->mnt_data, M_UFSMNT);	/* XXX */
	mp->mnt_data = 0;
	return (0);
}

int
umapfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct vnode *vp;

#ifdef UMAPFS_DIAGNOSTIC
	printf("umapfs_root(mp = %p, vp = %p->%p)\n", mp,
	    MOUNTTOUMAPMOUNT(mp)->umapm_rootvp,
	    UMAPVPTOLOWERVP(MOUNTTOUMAPMOUNT(mp)->umapm_rootvp));
#endif

	/*
	 * Return locked reference to root.
	 */
	vp = MOUNTTOUMAPMOUNT(mp)->umapm_rootvp;
	VREF(vp);
	VOP_LOCK(vp);
	*vpp = vp;
	return (0);
}

int
umapfs_quotactl(mp, cmd, uid, arg, p)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{

	return (VFS_QUOTACTL(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, cmd, uid, arg, p));
}

int
umapfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	int error;
	struct statfs mstat;

#ifdef UMAPFS_DIAGNOSTIC
	printf("umapfs_statfs(mp = %p, vp = %p->%p)\n", mp,
	    MOUNTTOUMAPMOUNT(mp)->umapm_rootvp,
	    UMAPVPTOLOWERVP(MOUNTTOUMAPMOUNT(mp)->umapm_rootvp));
#endif

	bzero(&mstat, sizeof(mstat));

	error = VFS_STATFS(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, &mstat, p);
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
umapfs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{

	/*
	 * XXX - Assumes no data cached at umap layer.
	 */
	return (0);
}

int
umapfs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	
	return (VFS_VGET(MOUNTTOUMAPMOUNT(mp)->umapm_vfs, ino, vpp));
}

int
umapfs_fhtovp(mp, fidp, nam, vpp, exflagsp, credanonp)
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
umapfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{

	return (EOPNOTSUPP);
}

struct vfsops umap_vfsops = {
	MOUNT_UMAP,
	umapfs_mount,
	umapfs_start,
	umapfs_unmount,
	umapfs_root,
	umapfs_quotactl,
	umapfs_statfs,
	umapfs_sync,
	umapfs_vget,
	umapfs_fhtovp,
	umapfs_vptofh,
	umapfs_init,
};
