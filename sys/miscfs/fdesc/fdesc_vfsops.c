/*
 * Copyright (c) 1990, 1992 Jan-Simon Pendry
 * All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *	$Id: fdesc_vfsops.c,v 1.5.2.3 1994/01/06 15:05:18 pk Exp $
 */

/*
 * /dev/fd Filesystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <miscfs/fdesc/fdesc.h>

static u_short fdesc_mntid;

fdesc_init()
{
#ifdef FDESC_DIAGNOSTIC
	printf("fdesc_init\n");		/* printed during system boot */
#endif
}

/*
 * Mount the per-process file descriptors (/dev/fd)
 */
fdesc_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	int error = 0;
	u_int size;
	struct fdescmount *fmp;
	struct vnode *rvp;

#ifdef FDESC_DIAGNOSTIC
	printf("fdesc_mount(mp = %x)\n", mp);
#endif

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	error = getnewvnode(VT_FDESC, mp, &fdesc_vnodeops, &rvp);
	if (error)
		return (error);

	fmp = (struct fdescmount *) malloc(sizeof(struct fdescmount),
				 M_MISCFSMNT, M_WAITOK);
	rvp->v_type = VDIR;
	rvp->v_flag |= VROOT;
	/*VTOFDESC(rvp)->f_isroot = 1;*/
#ifdef FDESC_DIAGNOSTIC
	printf("fdesc_mount: root vp = %x\n", rvp);
#endif
	fmp->f_root = rvp;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t) fmp;
	getnewfsid(mp, MOUNT_FDESC);

	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy("fdesc", mp->mnt_stat.f_mntfromname, sizeof("fdesc"));
#ifdef FDESC_DIAGNOSTIC
	printf("fdesc_mount: at %s\n", mp->mnt_stat.f_mntonname);
#endif
	return (0);
}

fdesc_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	return (0);
}

fdesc_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	int error;
	int flags = 0;
	extern int doforce;
	struct vnode *rootvp = VFSTOFDESC(mp)->f_root;

#ifdef FDESC_DIAGNOSTIC
	printf("fdesc_unmount(mp = %x)\n", mp);
#endif

	if (mntflags & MNT_FORCE) {
		/* fdesc can never be rootfs so don't check for it */
		if (!doforce)
			return (EINVAL);
		flags |= FORCECLOSE;
	}

	/*
	 * Clear out buffer cache.  I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 */
#ifdef FDESC_DIAGNOSTIC
	printf("fdesc_unmount: calling mntflushbuf\n");
#endif
	mntflushbuf(mp, 0); 
#ifdef FDESC_DIAGNOSTIC
	printf("fdesc_unmount: calling mntinvalbuf\n");
#endif
	if (mntinvalbuf(mp, 1))
		return (EBUSY);
	if (rootvp->v_usecount > 1)
		return (EBUSY);
#ifdef FDESC_DIAGNOSTIC
	printf("fdesc_unmount: calling vflush\n");
#endif
	if (error = vflush(mp, rootvp, flags))
		return (error);

#ifdef FDESC_DIAGNOSTIC
	vprint("fdesc root", rootvp);
#endif	 
	/*
	 * Release reference on underlying root vnode
	 */
	vrele(rootvp);
	/*
	 * And blow it away for future re-use
	 */
	vgone(rootvp);
	/*
	 * Finally, throw away the fdescmount structure
	 */
	free(mp->mnt_data, M_MISCFSMNT);
	mp->mnt_data = 0;
	return 0;
}

fdesc_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct vnode *vp;
	int error;

#ifdef FDESC_DIAGNOSTIC
	printf("fdesc_root(mp = %x)\n", mp);
#endif

	/*
	 * Return locked reference to root.
	 */
	vp = VFSTOFDESC(mp)->f_root;
	VREF(vp);
	VOP_LOCK(vp);
	*vpp = vp;
	return (0);
}

fdesc_quotactl(mp, cmd, uid, arg, p)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
	return (EOPNOTSUPP);
}

fdesc_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	struct filedesc *fdp;
	int lim;
	int i;
	int last;
	int freefd;

#ifdef FDESC_DIAGNOSTIC
	printf("fdesc_statfs(mp = %x)\n", mp);
#endif

	/*
	 * Compute number of free file descriptors.
	 * [ Strange results will ensue if the open file
	 * limit is ever reduced below the current number
	 * of open files... ]
	 */
	lim = p->p_rlimit[RLIMIT_NOFILE].rlim_cur;
	fdp = p->p_fd;
	last = min(fdp->fd_nfiles, lim);
	freefd = 0;
	for (i = fdp->fd_freefile; i < last; i++)
		if (fdp->fd_ofiles[i] == NULL)
			freefd++;

	/*
	 * Adjust for the fact that the fdesc array may not
	 * have been fully allocated yet.
	 */
	if (fdp->fd_nfiles < lim)
		freefd += (lim - fdp->fd_nfiles);

	sbp->f_type = MOUNT_FDESC;
	sbp->f_flags = 0;
	sbp->f_fsize = DEV_BSIZE;
	sbp->f_bsize = DEV_BSIZE;
	sbp->f_blocks = 2;		/* 1K to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = lim + 1;		/* Allow for "." */
	sbp->f_ffree = freefd;		/* See comments above */
	if (sbp != &mp->mnt_stat) {
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	return (0);
}

fdesc_sync(mp, waitfor)
	struct mount *mp;
	int waitfor;
{
	return (0);
}

fdesc_fhtovp(mp, fhp, vpp)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}

fdesc_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	return (EOPNOTSUPP);
}

struct vfsops fdesc_vfsops = {
	fdesc_mount,
	fdesc_start,
	fdesc_unmount,
	fdesc_root,
	fdesc_quotactl,
	fdesc_statfs,
	fdesc_sync,
	fdesc_fhtovp,
	fdesc_vptofh,
	fdesc_init,
};
