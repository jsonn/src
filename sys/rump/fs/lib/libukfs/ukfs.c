/*	$NetBSD: ukfs.c,v 1.5.2.2 2007/08/20 22:07:23 ad Exp $	*/

/*
 * Copyright (c) 2007 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Finnish Cultural Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This library enables access to files systems directly without
 * involving system calls.
 */

#define __UIO_EXPOSE
#define __VFSOPS_EXPOSE
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>

#include <assert.h>
#include <err.h>
#define _KERNEL
#include <errno.h>
#undef _KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rump.h"
#include "ukfs.h"

struct ukfs {
	struct mount *ukfs_mp;
};

struct mount *
ukfs_getmp(struct ukfs *ukfs)
{

	return ukfs->ukfs_mp;
}

struct vnode *
ukfs_getrvp(struct ukfs *ukfs)
{
	struct vnode *rvp;
	int rv;

	rv = VFS_ROOT(ukfs->ukfs_mp, &rvp);
	assert(rv == 0);
	assert(rvp->v_flag & VROOT);

	return rvp;
}

int
ukfs_init()
{

	rump_init();

	return 0;
}

struct ukfs *
ukfs_mount(const char *vfsname, const char *devpath, const char *mountpath,
	int mntflags, void *arg, size_t alen)
{
	struct ukfs *fs;
	struct vfsops *vfsops;
	struct mount *mp;
	int rv = 0;

	vfsops = rump_vfs_getopsbyname(vfsname);
	if (vfsops == NULL)
		return NULL;

	rump_mountinit(&mp, vfsops);

	fs = malloc(sizeof(struct ukfs));
	if (fs == NULL)
		return NULL;
	memset(fs, 0, sizeof(struct ukfs));

	mp->mnt_flag = mntflags;
	rump_fakeblk_register(devpath);
	rv = VFS_MOUNT(mp, mountpath, arg, &alen, curlwp);
	if (rv) {
		warnx("VFS_MOUNT %d", rv);
		goto out;
	}

	/* XXX: this doesn't belong here, but it'll be gone soon altogether */
	if ((1<<mp->mnt_fs_bshift) < getpagesize()
	    && (mntflags & MNT_RDONLY) == 0) {
		rv = EOPNOTSUPP;
		warnx("Sorry, fs bsize < PAGE_SIZE not yet supported for rw");
		goto out;
	}
	fs->ukfs_mp = mp;
	rump_fakeblk_deregister(devpath);

	rv = VFS_STATVFS(mp, &mp->mnt_stat, NULL);
	if (rv) {
		warnx("VFS_STATVFS %d", rv);
		goto out;
	}

 out:
	if (rv) {
		if (fs->ukfs_mp)
			rump_mountdestroy(fs->ukfs_mp);
		free(fs);
		errno = rv;
		fs = NULL;
	}

	return fs;
}

void
ukfs_release(struct ukfs *fs, int dounmount)
{
	int rv;

	if (dounmount) {
		rv = VFS_SYNC(fs->ukfs_mp, MNT_WAIT, rump_cred, curlwp);
		rv += VFS_UNMOUNT(fs->ukfs_mp, 0, curlwp);
		assert(rv == 0);
	}

	rump_mountdestroy(fs->ukfs_mp);

	free(fs);
}

/* don't need vn_lock(), since we don't have VXLOCK */
#define VLE(a) VOP_LOCK(a, LK_EXCLUSIVE)
#define VLS(a) VOP_LOCK(a, LK_SHARED)
#define VUL(a) VOP_UNLOCK(a, 0);
#define AUL(a) assert(VOP_ISLOCKED(a) == 0)

static void
recycle(struct vnode *vp)
{

	/* XXXXX */
	if (vp == NULL || vp->v_usecount != 0)
		return;

	VLE(vp);
	VOP_FSYNC(vp, NULL, 0, 0, 0, curlwp);
	VOP_INACTIVE(vp, curlwp);
	rump_recyclenode(vp);
	rump_putnode(vp);
}

/*
 * simplo (well, horrid) namei.  doesn't do symlinks & anything else
 * hard, though.  (ok, ok, it's a mess, it's a messssss!)
 *
 * XXX: maybe I should just try running the kernel namei(), although
 * it would require a wrapping due to the name collision in
 * librump vfs.c
 */
static int
ukfs_namei(struct vnode *rvp, const char **pnp, u_long op,
	struct vnode **dvpp, struct vnode **vpp)
{
	struct vnode *dvp, *vp;
	struct componentname *cnp;
	const char *pn, *p_next, *p_end;
	size_t pnlen;
	u_long flags;
	int rv;

	/* remove trailing slashes */
	pn = *pnp;
	assert(strlen(pn) > 0);
	p_end = pn + strlen(pn)-1;
	while (*p_end == '/' && p_end != *pnp)
		p_end--;

	/* caller wanted root? */
	if (p_end == *pnp) {
		if (dvpp)
			*dvpp = rvp;
		if (vpp)
			*vpp = rvp;

		*pnp = p_end;
		return 0;
	}
		
	dvp = NULL;
	vp = rvp;
	p_end++;
	for (;;) {
		while (*pn == '/')
			pn++;
		assert(*pn != '\0');

		flags = 0;
		dvp = vp;
		vp = NULL;

		p_next = strchr(pn, '/');
		if (p_next == NULL || p_next == p_end) {
			p_next = p_end;
			flags |= NAMEI_ISLASTCN;
		}
		pnlen = p_next - pn;

		if (pnlen == 2 && strcmp(pn, "..") == 0)
			flags |= NAMEI_ISDOTDOT;

		VLE(dvp);
		cnp = rump_makecn(op, flags, pn, pnlen, curlwp);
		rv = VOP_LOOKUP(dvp, &vp, cnp);
		rump_freecn(cnp, 1);
		VUL(dvp);
		if (rv == 0)
			VUL(vp);

		if (!((flags & NAMEI_ISLASTCN) && dvpp))
			recycle(dvp);

		if (rv || (flags & NAMEI_ISLASTCN))
			break;

		pn += pnlen;
	}
	assert(flags & NAMEI_ISLASTCN);
	if (vp && vpp == NULL)
		recycle(vp);

	if (dvpp)
		*dvpp = dvp;
	if (vpp)
		*vpp = vp;
	*pnp = pn;

	return rv;
}

int
ukfs_getdents(struct ukfs *ukfs, const char *dirname, off_t off,
	uint8_t *buf, size_t bufsize)
{
	struct uio uio;
	struct iovec iov;
	struct vnode *vp;
	int rv, eofflag;

	UKFS_UIOINIT(uio, iov, buf, bufsize, off, UIO_READ);

	rv = ukfs_namei(ukfs_getrvp(ukfs), &dirname, NAMEI_LOOKUP, NULL, &vp);
	if (rv)
		goto out;
		
	VLE(vp);
	rv = VOP_READDIR(vp, &uio, NULL, &eofflag, NULL, NULL);
	VUL(vp);

 out:
	recycle(vp);

	if (rv) {
		errno = rv;
		return -1;
	}

	return bufsize - uio.uio_resid;
}

static ssize_t
readwrite(struct ukfs *ukfs, const char *filename, off_t off,
	uint8_t *buf, size_t bufsize, enum uio_rw rw,
	int (*do_fn)(struct vnode *, struct uio *, int, kauth_cred_t))
{
	struct uio uio;
	struct iovec iov;
	struct vnode *vp;
	int rv;

	UKFS_UIOINIT(uio, iov, buf, bufsize, off, rw);

	rv = ukfs_namei(ukfs_getrvp(ukfs), &filename, NAMEI_LOOKUP, NULL, &vp);
	if (rv)
		goto out;

	VLS(vp);
	rv = do_fn(vp, &uio, 0, NULL);
	VUL(vp);

 out:
	recycle(vp);

	if (rv) {
		errno = rv;
		return -1;
	}

	return bufsize - uio.uio_resid;
}

ssize_t
ukfs_read(struct ukfs *ukfs, const char *filename, off_t off,
	uint8_t *buf, size_t bufsize)
{

	return readwrite(ukfs, filename, off, buf, bufsize, UIO_READ, VOP_READ);
}

ssize_t
ukfs_write(struct ukfs *ukfs, const char *filename, off_t off,
	uint8_t *buf, size_t bufsize)
{

	return readwrite(ukfs, filename, off, buf, bufsize,UIO_WRITE,VOP_WRITE);
}

static int
create(struct ukfs *ukfs, const char *filename, mode_t mode, dev_t dev,
	int (*do_fn)(struct vnode *, struct vnode **,
		     struct componentname *, struct vattr *))
{
	struct componentname *cnp;
	struct vnode *dvp = NULL, *vp = NULL;
	struct vattr va;
	struct timeval tv_now;
	int rv;

	rv = ukfs_namei(ukfs_getrvp(ukfs), &filename, NAMEI_CREATE, &dvp, NULL);
	if (rv == 0)
		rv = EEXIST;
	if (rv != EJUSTRETURN)
		goto out;

	gettimeofday(&tv_now, NULL);

	rump_vattr_null(&va);
	va.va_mode = mode;
	va.va_rdev = dev;

	cnp = rump_makecn(NAMEI_CREATE, NAMEI_HASBUF|NAMEI_SAVENAME, filename,
	    strlen(filename), curlwp);
	VLE(dvp);
	rv = do_fn(dvp, &vp, cnp, &va);
	rump_freecn(cnp, 0);

 out:
	recycle(dvp);
	recycle(vp);

	if (rv) {
		errno = rv;
		return -1;
	}

	return 0;
}

int
ukfs_create(struct ukfs *ukfs, const char *filename, mode_t mode)
{

	return create(ukfs, filename, mode, /*XXX*/(dev_t)-1, VOP_CREATE);
}

int
ukfs_mknod(struct ukfs *ukfs, const char *filename, mode_t mode, dev_t dev)
{

	return create(ukfs, filename, mode, dev, VOP_MKNOD);
}

int
ukfs_mkdir(struct ukfs *ukfs, const char *filename, mode_t mode)
{

	return create(ukfs, filename, mode, (dev_t)-1, VOP_MKDIR);
}

static int
doremove(struct ukfs *ukfs, const char *filename,
	int (*do_fn)(struct vnode *, struct vnode *, struct componentname *))
{
	struct componentname *cnp;
	struct vnode *dvp = NULL, *vp = NULL;
	int rv;

	rv = ukfs_namei(ukfs_getrvp(ukfs), &filename, NAMEI_DELETE, &dvp, &vp);
	if (rv)
		goto out;

	cnp = rump_makecn(NAMEI_DELETE, 0, filename, strlen(filename), curlwp);
	VLE(dvp);
	VLE(vp);
	rv = do_fn(dvp, vp, cnp);
	rump_freecn(cnp, 0);

 out:
	recycle(dvp);
	recycle(vp);

	if (rv) {
		errno = rv;
		return -1;
	}

	return 0;
}

int
ukfs_remove(struct ukfs *ukfs, const char *filename)
{

	return doremove(ukfs, filename, VOP_REMOVE);
}

int
ukfs_rmdir(struct ukfs *ukfs, const char *filename)
{

	return doremove(ukfs, filename, VOP_RMDIR);
}

int
ukfs_link(struct ukfs *ukfs, const char *filename, const char *f_create)
{
	struct vnode *dvp = NULL, *vp = NULL;
	struct componentname *cnp;
	int rv;

	rv = ukfs_namei(ukfs_getrvp(ukfs), &filename, NAMEI_LOOKUP, NULL, &vp);
	if (rv)
		goto out;

	vp->v_usecount = 1; /* XXX kludge of the year */
	rv = ukfs_namei(ukfs_getrvp(ukfs), &f_create, NAMEI_CREATE, &dvp, NULL);
	vp->v_usecount = 0; /* XXX */

	if (rv == 0)
		rv = EEXIST;
	if (rv != EJUSTRETURN)
		goto out;

	cnp = rump_makecn(NAMEI_CREATE, NAMEI_HASBUF | NAMEI_SAVENAME,
	    f_create, strlen(f_create), curlwp);
	VLE(dvp);
	rv = VOP_LINK(dvp, vp, cnp);
	rump_freecn(cnp, 0);

 out:
	recycle(dvp);
	recycle(vp);

	if (rv) {
		errno = rv;
		return -1;
	}

	return 0;
}
