/*	$NetBSD: vfs.c,v 1.11.4.1 2007/11/06 23:34:38 matt Exp $	*/

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
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/lock.h>
#include <sys/lockf.h>
#include <sys/stat.h>
#include <sys/namei.h>
#include <sys/queue.h>
#include <sys/filedesc.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/specfs/specdev.h>
#include <miscfs/syncfs/syncfs.h>

#include "rump_private.h"
#include "rumpuser.h"

int (**dead_vnodeop_p)(void *);
const struct vnodeopv_entry_desc dead_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ NULL, NULL }
};
const struct vnodeopv_desc dead_vnodeop_opv_desc =
	{ &dead_vnodeop_p, dead_vnodeop_entries };

int (**sync_vnodeop_p)(void *);
const struct vnodeopv_entry_desc sync_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ NULL, NULL }
};
const struct vnodeopv_desc sync_vnodeop_opv_desc =
	{ &sync_vnodeop_p, sync_vnodeop_entries };

int (**fifo_vnodeop_p)(void *);
const struct vnodeopv_entry_desc fifo_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ NULL, NULL }
};
const struct vnodeopv_desc fifo_vnodeop_opv_desc =
	{ &fifo_vnodeop_p, fifo_vnodeop_entries };


int
getnewvnode(enum vtagtype tag, struct mount *mp, int (**vops)(void *),
	struct vnode **vpp)
{
	struct vnode *vp;
	struct uvm_object *uobj;

	vp = rumpuser_malloc(sizeof(struct vnode), 0);
	memset(vp, 0, sizeof(struct vnode));
	vp->v_mount = mp;
	vp->v_tag = tag;
	vp->v_op = vops;
	vp->v_vnlock = &vp->v_lock;
	vp->v_usecount = 1;
	TAILQ_INSERT_TAIL(&mp->mnt_vnodelist, vp, v_mntvnodes);

	uobj = &vp->v_uobj;
	uobj->pgops = &uvm_vnodeops;
	TAILQ_INIT(&uobj->memq);

	*vpp = vp;

	return 0;
}

void
rump_putnode(struct vnode *vp)
{

	if (vp->v_specinfo)
		rumpuser_free(vp->v_specinfo);
	rumpuser_free(vp);
}

void
ungetnewvnode(struct vnode *vp)
{

	rump_putnode(vp);
}

int
vflush(struct mount *mp, struct vnode *skipvp, int flags)
{

	return 0;
}

void
vref(struct vnode *vp)
{

}

int
vget(struct vnode *vp, int lockflag)
{

	if (lockflag & LK_TYPE_MASK)
		vn_lock(vp, lockflag & LK_TYPE_MASK);
	return 0;
}

void
vrele(struct vnode *vp)
{

}

void
vrele2(struct vnode *vp, int onhead)
{

}

void
vput(struct vnode *vp)
{

	VOP_UNLOCK(vp, 0);
}

void
vgone(struct vnode *vp)
{

	vgonel(vp, curlwp);
}

void
vgonel(struct vnode *vp, struct lwp *l)
{

}

void
vholdl(struct vnode *vp)
{

}

void
holdrelel(struct vnode *vp)
{

}

int
vrecycle(struct vnode *vp, struct simplelock *inter_lkp, struct lwp *l)
{
	struct mount *mp = vp->v_mount;

	if (vp->v_usecount == 1) {
		vp->v_usecount = 0;
		simple_lock(&vp->v_interlock);
		if (inter_lkp)
			simple_unlock(inter_lkp);
		VOP_LOCK(vp, LK_EXCLUSIVE | LK_INTERLOCK);
		vinvalbuf(vp, V_SAVE, NOCRED, l, 0, 0);
		VOP_INACTIVE(vp, l);

		VOP_RECLAIM(vp, l);
		TAILQ_REMOVE(&mp->mnt_vnodelist, vp, v_mntvnodes);
	}

	return 0;
}

int
vcount(struct vnode *vp)
{

	return 1;
}


int
vfs_stdextattrctl(struct mount *mp, int cmt, struct vnode *vp,
	int attrnamespace, const char *attrname, struct lwp *l)
{

	if (vp != NULL)
		VOP_UNLOCK(vp, 0);
	return EOPNOTSUPP;
}

struct mount mnt_dummy;

/* from libpuffs, but let's decouple this from that */
static enum vtype
mode2vt(mode_t mode)
{

	switch (mode & S_IFMT) {
	case S_IFIFO:
		return VFIFO;
	case S_IFCHR:
		return VCHR;
	case S_IFDIR:
		return VDIR;
	case S_IFBLK:
		return VBLK;
	case S_IFREG:
		return VREG;
	case S_IFLNK:
		return VLNK;
	case S_IFSOCK:
		return VSOCK;
	default:
		return VBAD; /* XXX: not really true, but ... */
	}
}

static struct vnode *
makevnode(struct stat *sb, const char *path)
{
	struct vnode *vp;
	struct rump_specpriv *sp;

	vp = rumpuser_malloc(sizeof(struct vnode), 0);
	vp->v_size = vp->v_writesize = sb->st_size;
	vp->v_type = mode2vt(sb->st_mode);
	if (vp->v_type != VBLK)
		if (rump_fakeblk_find(path))
			vp->v_type = VBLK;

	if (vp->v_type != VBLK)
		panic("namei: only VBLK results supported currently");

	vp->v_specinfo = rumpuser_malloc(sizeof(struct specinfo), 0);
	vp->v_rdev = sb->st_dev;
	sp = rumpuser_malloc(sizeof(struct rump_specpriv), 0);
	strcpy(sp->rsp_path, path);
	vp->v_data = sp;
	vp->v_op = spec_vnodeop_p;
	vp->v_mount = &mnt_dummy;
	vp->v_vnlock = &vp->v_lock;

	return vp;
}

int
namei(struct nameidata *ndp)
{
	struct componentname *cnp = &ndp->ni_cnd;
	struct stat sb_node;
	struct vnode *vp;
	int rv, error;

	if (cnp->cn_flags & LOCKPARENT)
		panic("%s: LOCKPARENT not supported", __func__);

	if ((cnp->cn_flags & HASBUF) == 0)
		cnp->cn_pnbuf = PNBUF_GET();

	error = copystr(ndp->ni_dirp, cnp->cn_pnbuf,
	    MAXPATHLEN, &ndp->ni_pathlen);

#if 0
	/* uh, why did I put this here originally? */
	if (!error && ndp->ni_pathlen == 1)
		error = ENOENT;
#endif

	if (error) {
		PNBUF_PUT(cnp->cn_pnbuf);
		ndp->ni_vp = NULL;
		return error;
	}

	if (cnp->cn_flags & FOLLOW)
		rv = rumpuser_stat(cnp->cn_pnbuf, &sb_node, &error);
	else
		rv = rumpuser_lstat(cnp->cn_pnbuf, &sb_node, &error);

	if (rv == -1) {
		PNBUF_PUT(cnp->cn_pnbuf);
		return error;
	}

	vp = makevnode(&sb_node, cnp->cn_pnbuf);
	if (cnp->cn_flags & LOCKLEAF)
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	ndp->ni_vp = vp;

	return 0;
}

int
relookup(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp)
{

	return VOP_LOOKUP(dvp, vpp, cnp);
}

/*
 * Ok, we can't actually do getcwd() for lvp, so just pretend we can.
 * Currently this is called only from set_statvfs_info().  If would be
 * nice if we could assert on that.
 */
int
getcwd_common(struct vnode *lvp, struct vnode *rvp, char **bpp, char *bufp,
	int limit, int flags, struct lwp *l)
{

	assert(rvp == rootvnode);

	**bpp = '/';
	(*bpp)--;
	return 0;
}

int
lf_advlock(struct vop_advlock_args *ap, struct lockf **head, off_t size)
{

	return 0;
}

int
vfs_rootmountalloc(const char *fstypename, const char *devname,
	struct mount **mpp)
{

	panic("%s: not supported", __func__);
}

int
vfs_busy(struct mount *mp, int flags, kmutex_t *interlck)
{

	return 0;
}

void
vfs_unbusy(struct mount *mp)
{

	return;
}

struct vnode *
checkalias(struct vnode *nvp, dev_t nvp_rdev, struct mount *mp)
{

	/* Can this cause any funnies? */

	nvp->v_specinfo = rumpuser_malloc(sizeof(struct specinfo), 0);
	nvp->v_rdev = nvp_rdev;
	return NULLVP;
}

void
fifo_printinfo(struct vnode *vp)
{

	return;
}

int
vfs_mountedon(struct vnode *vp)
{

	return 0;
}
