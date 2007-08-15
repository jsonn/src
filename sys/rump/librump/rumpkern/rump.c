/*	$NetBSD: rump.c,v 1.6.2.2 2007/08/15 13:50:40 skrll Exp $	*/

/*
 * Copyright (c) 2007 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by Google Summer of Code.
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

#include <sys/param.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/queue.h>
#include <sys/resourcevar.h>
#include <sys/vnode.h>

#include <machine/cpu.h>

#include <miscfs/specfs/specdev.h>

#include "rump.h"
#include "rumpuser.h"

struct proc rump_proc;
struct cwdinfo rump_cwdi;
struct pstats rump_stats;
struct plimit rump_limits;
kauth_cred_t rump_cred;
struct cpu_info rump_cpu;

struct fakeblk {
	char path[MAXPATHLEN];
	LIST_ENTRY(fakeblk) entries;
};

static LIST_HEAD(, fakeblk) fakeblks = LIST_HEAD_INITIALIZER(fakeblks);

void
rump_init()
{
	extern char hostname[];
	extern size_t hostnamelen;
	int error;

	curlwp = &lwp0;
	rumpvm_init();

	curlwp->l_proc = &rump_proc;
	curlwp->l_cred = rump_cred;
	rump_proc.p_stats = &rump_stats;
	rump_proc.p_cwdi = &rump_cwdi;
	rump_limits.pl_rlimit[RLIMIT_FSIZE].rlim_cur = RLIM_INFINITY;
	rump_proc.p_limit = &rump_limits;

	vfsinit();
	bufinit();

	rumpuser_gethostname(hostname, MAXHOSTNAMELEN, &error);
	hostnamelen = strlen(hostname);
}

void
rump_mountinit(struct mount **mpp, struct vfsops *vfsops)
{
	struct mount *mp;

	mp = rumpuser_malloc(sizeof(struct mount), 0);
	memset(mp, 0, sizeof(struct mount));

	mp->mnt_op = vfsops;
	TAILQ_INIT(&mp->mnt_vnodelist);
	*mpp = mp;
}

void
rump_mountdestroy(struct mount *mp)
{

	rumpuser_free(mp);
}

struct componentname *
rump_makecn(u_long nameiop, u_long flags, const char *name, size_t namelen,
	struct lwp *l)
{
	struct componentname *cnp;

	cnp = rumpuser_malloc(sizeof(struct componentname), 0);
	memset(cnp, 0, sizeof(struct componentname));

	cnp->cn_nameiop = nameiop;
	cnp->cn_flags = flags;

	cnp->cn_pnbuf = PNBUF_GET();
	strcpy(cnp->cn_pnbuf, name);
	cnp->cn_nameptr = cnp->cn_pnbuf;
	cnp->cn_namelen = namelen;

	cnp->cn_cred = l->l_cred;
	cnp->cn_lwp = l;

	return cnp;
}

void
rump_freecn(struct componentname *cnp, int islookup)
{

	if (cnp->cn_flags & SAVENAME) {
		if (islookup || cnp->cn_flags & SAVESTART)
			PNBUF_PUT(cnp->cn_pnbuf);
	} else {
		PNBUF_PUT(cnp->cn_pnbuf);
	}
	rumpuser_free(cnp);
}

int
rump_recyclenode(struct vnode *vp)
{

	return vrecycle(vp, NULL, curlwp);
}

static struct fakeblk *
_rump_fakeblk_find(const char *path)
{
	char buf[MAXPATHLEN];
	struct fakeblk *fblk;
	int error;

	if (rumpuser_realpath(path, buf, &error) == NULL)
		return NULL;

	LIST_FOREACH(fblk, &fakeblks, entries)
		if (strcmp(fblk->path, buf) == 0)
			return fblk;

	return NULL;
}

int
rump_fakeblk_register(const char *path)
{
	char buf[MAXPATHLEN];
	struct fakeblk *fblk;
	int error;

	if (_rump_fakeblk_find(path))
		return EEXIST;

	if (rumpuser_realpath(path, buf, &error) == NULL)
		return error;

	fblk = rumpuser_malloc(sizeof(struct fakeblk), 1);
	if (fblk == NULL)
		return ENOMEM;

	strlcpy(fblk->path, buf, MAXPATHLEN);
	LIST_INSERT_HEAD(&fakeblks, fblk, entries);

	return 0;
}

int
rump_fakeblk_find(const char *path)
{

	return _rump_fakeblk_find(path) != NULL;
}

void
rump_fakeblk_deregister(const char *path)
{
	struct fakeblk *fblk;

	fblk = _rump_fakeblk_find(path);
	if (fblk == NULL)
		return;

	LIST_REMOVE(fblk, entries);
	rumpuser_free(fblk);
}

void
rump_getvninfo(struct vnode *vp, enum vtype *vtype, voff_t *vsize, dev_t *vdev)
{

	*vtype = vp->v_type;
	*vsize = vp->v_size;
	if (vp->v_specinfo)
		*vdev = vp->v_rdev;
	else
		*vdev = 0;
}

struct vfsops *
rump_vfslist_iterate(struct vfsops *ops)
{

	if (ops == NULL)
		return LIST_FIRST(&vfs_list);
	else
		return LIST_NEXT(ops, vfs_list);
}

struct vfsops *
rump_vfs_getopsbyname(const char *name)
{

	return vfs_getopsbyname(name);
}
