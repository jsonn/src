/*	$NetBSD: rump.c,v 1.45.2.2 2008/09/18 04:37:04 wrstuden Exp $	*/

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
#include <sys/cpu.h>
#include <sys/filedesc.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/queue.h>
#include <sys/resourcevar.h>
#include <sys/select.h>
#include <sys/vnode.h>
#include <sys/vfs_syscalls.h>
#include <sys/wapbl.h>
#include <sys/sysctl.h>

#include <miscfs/specfs/specdev.h>

#include <rump/rumpuser.h>

#include "rump_private.h"

struct proc proc0;
struct cwdinfo rump_cwdi;
struct pstats rump_stats;
struct plimit rump_limits;
kauth_cred_t rump_cred = RUMPCRED_SUSER;
struct cpu_info rump_cpu;
struct filedesc rump_filedesc0;
struct proclist allproc;
char machine[] = "rump";

kmutex_t rump_giantlock;

sigset_t sigcantmask;

#ifdef RUMP_WITHOUT_THREADS
int rump_threads = 0;
#else
int rump_threads = 1;
#endif

struct fakeblk {
	char path[MAXPATHLEN];
	LIST_ENTRY(fakeblk) entries;
};

static LIST_HEAD(, fakeblk) fakeblks = LIST_HEAD_INITIALIZER(fakeblks);

static void
rump_aiodone_worker(struct work *wk, void *dummy)
{
	struct buf *bp = (struct buf *)wk;

	KASSERT(&bp->b_work == wk);
	bp->b_iodone(bp);
}

static int rump_inited;
static struct emul emul_rump;

void
rump_init()
{
	extern char hostname[];
	extern size_t hostnamelen;
	extern kmutex_t rump_atomic_lock;
	char buf[256];
	struct proc *p;
	struct lwp *l;
	int error;

	/* XXX */
	if (rump_inited)
		return;
	rump_inited = 1;

	if (rumpuser_getenv("RUMP_NVNODES", buf, sizeof(buf), &error) == 0) {
		desiredvnodes = strtoul(buf, NULL, 10);
	} else {
		desiredvnodes = 1<<16;
	}
	if (rumpuser_getenv("RUMP_THREADS", buf, sizeof(buf), &error) == 0) {
		rump_threads = *buf != '0';
	}

	rumpvm_init();
	rump_sleepers_init();
#ifdef RUMP_USE_REAL_KMEM
	kmem_init();
#endif

	cache_cpu_init(&rump_cpu);
	rw_init(&rump_cwdi.cwdi_lock);
	l = &lwp0;
	p = &proc0;
	p->p_stats = &rump_stats;
	p->p_cwdi = &rump_cwdi;
	p->p_limit = &rump_limits;
	p->p_pid = 0;
	p->p_fd = &rump_filedesc0;
	p->p_vmspace = &rump_vmspace;
	p->p_emul = &emul_rump;
	l->l_cred = rump_cred;
	l->l_proc = p;
	l->l_lid = 1;

	LIST_INSERT_HEAD(&allproc, p, p_list);

	mutex_init(&rump_atomic_lock, MUTEX_DEFAULT, IPL_NONE);

	rump_limits.pl_rlimit[RLIMIT_FSIZE].rlim_cur = RLIM_INFINITY;
	rump_limits.pl_rlimit[RLIMIT_NOFILE].rlim_cur = RLIM_INFINITY;

	syncdelay = 0;
	dovfsusermount = 1;

	rumpuser_thrinit();

	fd_sys_init();
	module_init();
	sysctl_init();
	vfsinit();
	bufinit();
	wapbl_init();

	rumpvfs_init();

	rumpuser_mutex_recursive_init(&rump_giantlock.kmtx_mtx);

	/* aieeeedondest */
	if (rump_threads) {
		if (workqueue_create(&uvm.aiodone_queue, "aiodoned",
		    rump_aiodone_worker, NULL, 0, 0, 0))
			panic("aiodoned");
	}

	rumpuser_gethostname(hostname, MAXHOSTNAMELEN, &error);
	hostnamelen = strlen(hostname);

	sigemptyset(&sigcantmask);

	fd_init(&rump_filedesc0);
	rump_cwdi.cwdi_cdir = rootvnode;
}

struct mount *
rump_mnt_init(struct vfsops *vfsops, int mntflags)
{
	struct mount *mp;

	mp = kmem_zalloc(sizeof(struct mount), KM_SLEEP);

	mp->mnt_op = vfsops;
	mp->mnt_flag = mntflags;
	TAILQ_INIT(&mp->mnt_vnodelist);
	rw_init(&mp->mnt_unmounting);
	mutex_init(&mp->mnt_updating, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&mp->mnt_renamelock, MUTEX_DEFAULT, IPL_NONE);
	mp->mnt_refcnt = 1;

	mount_initspecific(mp);

	return mp;
}

int
rump_mnt_mount(struct mount *mp, const char *path, void *data, size_t *dlen)
{
	int rv;

	rv = VFS_MOUNT(mp, path, data, dlen);
	if (rv)
		return rv;

	(void) VFS_STATVFS(mp, &mp->mnt_stat);
	rv = VFS_START(mp, 0);
	if (rv)
		VFS_UNMOUNT(mp, MNT_FORCE);

	return rv;
}

void
rump_mnt_destroy(struct mount *mp)
{

	mount_finispecific(mp);
	kmem_free(mp, sizeof(*mp));
}

struct componentname *
rump_makecn(u_long nameiop, u_long flags, const char *name, size_t namelen,
	kauth_cred_t creds, struct lwp *l)
{
	struct componentname *cnp;
	const char *cp = NULL;

	cnp = kmem_zalloc(sizeof(struct componentname), KM_SLEEP);

	cnp->cn_nameiop = nameiop;
	cnp->cn_flags = flags;

	cnp->cn_pnbuf = PNBUF_GET();
	strcpy(cnp->cn_pnbuf, name);
	cnp->cn_nameptr = cnp->cn_pnbuf;
	cnp->cn_namelen = namelen;
	cnp->cn_hash = namei_hash(name, &cp);

	cnp->cn_cred = creds;

	return cnp;
}

void
rump_freecn(struct componentname *cnp, int flags)
{

	if (flags & RUMPCN_FREECRED)
		rump_cred_destroy(cnp->cn_cred);

	if ((flags & RUMPCN_HASNTBUF) == 0) {
		if (cnp->cn_flags & SAVENAME) {
			if (flags & RUMPCN_ISLOOKUP ||cnp->cn_flags & SAVESTART)
				PNBUF_PUT(cnp->cn_pnbuf);
		} else {
			PNBUF_PUT(cnp->cn_pnbuf);
		}
	}
	kmem_free(cnp, sizeof(*cnp));
}

/* hey baby, what's your namei? */
int
rump_namei(uint32_t op, uint32_t flags, const char *namep,
	struct vnode **dvpp, struct vnode **vpp, struct componentname **cnpp)
{
	struct nameidata nd;
	int rv;

	NDINIT(&nd, op, flags, UIO_SYSSPACE, namep);
	rv = namei(&nd);
	if (rv)
		return rv;

	if (dvpp) {
		KASSERT(flags & LOCKPARENT);
		*dvpp = nd.ni_dvp;
	} else {
		KASSERT((flags & LOCKPARENT) == 0);
	}

	if (vpp) {
		*vpp = nd.ni_vp;
	} else {
		if (nd.ni_vp) {
			if (flags & LOCKLEAF)
				vput(nd.ni_vp);
			else
				vrele(nd.ni_vp);
		}
	}

	if (cnpp) {
		struct componentname *cnp;

		cnp = kmem_alloc(sizeof(*cnp), KM_SLEEP);
		memcpy(cnp, &nd.ni_cnd, sizeof(*cnp));
		*cnpp = cnp;
	} else if (nd.ni_cnd.cn_flags & HASBUF) {
		panic("%s: pathbuf mismatch", __func__);
	}

	return rv;
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

	fblk = kmem_alloc(sizeof(struct fakeblk), KM_NOSLEEP);
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
	kmem_free(fblk, sizeof(*fblk));
}

void
rump_getvninfo(struct vnode *vp, enum vtype *vtype, voff_t *vsize, dev_t *vdev)
{

	*vtype = vp->v_type;
	*vsize = vp->v_size;
	if (vp->v_specnode)
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

struct vattr*
rump_vattr_init()
{
	struct vattr *vap;

	vap = kmem_alloc(sizeof(struct vattr), KM_SLEEP);
	vattr_null(vap);

	return vap;
}

void
rump_vattr_settype(struct vattr *vap, enum vtype vt)
{

	vap->va_type = vt;
}

void
rump_vattr_setmode(struct vattr *vap, mode_t mode)
{

	vap->va_mode = mode;
}

void
rump_vattr_setrdev(struct vattr *vap, dev_t dev)
{

	vap->va_rdev = dev;
}

void
rump_vattr_free(struct vattr *vap)
{

	kmem_free(vap, sizeof(*vap));
}

void
rump_vp_incref(struct vnode *vp)
{

	mutex_enter(&vp->v_interlock);
	++vp->v_usecount;
	mutex_exit(&vp->v_interlock);
}

int
rump_vp_getref(struct vnode *vp)
{

	return vp->v_usecount;
}

void
rump_vp_decref(struct vnode *vp)
{

	mutex_enter(&vp->v_interlock);
	--vp->v_usecount;
	mutex_exit(&vp->v_interlock);
}

/*
 * Really really recycle with a cherry on top.  We should be
 * extra-sure we can do this.  For example with p2k there is
 * no problem, since puffs in the kernel takes care of refcounting
 * for us.
 */
void
rump_vp_recycle_nokidding(struct vnode *vp)
{

	mutex_enter(&vp->v_interlock);
	vp->v_usecount = 1;
	vclean(vp, DOCLOSE);
	vrelel(vp, 0);
}

void
rump_vp_rele(struct vnode *vp)
{

	vrele(vp);
}

struct uio *
rump_uio_setup(void *buf, size_t bufsize, off_t offset, enum rump_uiorw rw)
{
	struct uio *uio;
	enum uio_rw uiorw;

	switch (rw) {
	case RUMPUIO_READ:
		uiorw = UIO_READ;
		break;
	case RUMPUIO_WRITE:
		uiorw = UIO_WRITE;
		break;
	default:
		panic("%s: invalid rw %d", __func__, rw);
	}

	uio = kmem_alloc(sizeof(struct uio), KM_SLEEP);
	uio->uio_iov = kmem_alloc(sizeof(struct iovec), KM_SLEEP);

	uio->uio_iov->iov_base = buf;
	uio->uio_iov->iov_len = bufsize;

	uio->uio_iovcnt = 1;
	uio->uio_offset = offset;
	uio->uio_resid = bufsize;
	uio->uio_rw = uiorw;
	uio->uio_vmspace = UIO_VMSPACE_SYS;

	return uio;
}

size_t
rump_uio_getresid(struct uio *uio)
{

	return uio->uio_resid;
}

off_t
rump_uio_getoff(struct uio *uio)
{

	return uio->uio_offset;
}

size_t
rump_uio_free(struct uio *uio)
{
	size_t resid;

	resid = uio->uio_resid;
	kmem_free(uio->uio_iov, sizeof(*uio->uio_iov));
	kmem_free(uio, sizeof(*uio));

	return resid;
}

void
rump_vp_lock_exclusive(struct vnode *vp)
{

	/* we can skip vn_lock() */
	VOP_LOCK(vp, LK_EXCLUSIVE);
}

void
rump_vp_lock_shared(struct vnode *vp)
{

	VOP_LOCK(vp, LK_SHARED);
}

void
rump_vp_unlock(struct vnode *vp)
{

	VOP_UNLOCK(vp, 0);
}

int
rump_vp_islocked(struct vnode *vp)
{

	return VOP_ISLOCKED(vp);
}

void
rump_vp_interlock(struct vnode *vp)
{

	mutex_enter(&vp->v_interlock);
}

int
rump_vfs_unmount(struct mount *mp, int mntflags)
{

	return VFS_UNMOUNT(mp, mntflags);
}

int
rump_vfs_root(struct mount *mp, struct vnode **vpp, int lock)
{
	int rv;

	rv = VFS_ROOT(mp, vpp);
	if (rv)
		return rv;

	if (!lock)
		VOP_UNLOCK(*vpp, 0);

	return 0;
}

int
rump_vfs_statvfs(struct mount *mp, struct statvfs *sbp)
{

	return VFS_STATVFS(mp, sbp);
}

int
rump_vfs_sync(struct mount *mp, int wait, kauth_cred_t cred)
{

	return VFS_SYNC(mp, wait ? MNT_WAIT : MNT_NOWAIT, cred);
}

int
rump_vfs_fhtovp(struct mount *mp, struct fid *fid, struct vnode **vpp)
{

	return VFS_FHTOVP(mp, fid, vpp);
}

int
rump_vfs_vptofh(struct vnode *vp, struct fid *fid, size_t *fidsize)
{

	return VFS_VPTOFH(vp, fid, fidsize);
}

/*ARGSUSED*/
void
rump_vfs_syncwait(struct mount *mp)
{
	int n;

	n = buf_syncwait();
	if (n)
		printf("syncwait: unsynced buffers: %d\n", n);
}

int
rump_vfs_load(struct modinfo **mi)
{

	if (!module_compatible((*mi)->mi_version, __NetBSD_Version__))
		return EPROGMISMATCH;

	return (*mi)->mi_modcmd(MODULE_CMD_INIT, NULL);
}

void
rump_bioops_sync()
{

	if (bioopsp)
		bioopsp->io_sync(NULL);
}

struct lwp *
rump_setup_curlwp(pid_t pid, lwpid_t lid, int set)
{
	struct lwp *l;
	struct proc *p;

	l = kmem_zalloc(sizeof(struct lwp), KM_SLEEP);
	if (pid != 0) {
		p = kmem_zalloc(sizeof(struct proc), KM_SLEEP);
		p->p_cwdi = cwdinit();

		p->p_stats = &rump_stats;
		p->p_limit = &rump_limits;
		p->p_pid = pid;
		p->p_vmspace = &rump_vmspace;
		p->p_fd = fd_init(NULL);
	} else {
		p = &proc0;
	}

	l->l_cred = rump_cred;
	l->l_proc = p;
	l->l_lid = lid;
	l->l_fd = p->p_fd;

	if (set)
		rumpuser_set_curlwp(l);

	return l;
}

void
rump_clear_curlwp()
{
	struct lwp *l;

	l = rumpuser_get_curlwp();
	if (l->l_proc->p_pid != 0) {
		fd_free();
		cwdfree(l->l_proc->p_cwdi);
		kmem_free(l->l_proc, sizeof(*l->l_proc));
	}
	kmem_free(l, sizeof(*l));
	rumpuser_set_curlwp(NULL);
}

struct lwp *
rump_get_curlwp()
{
	struct lwp *l;

	l = rumpuser_get_curlwp();
	if (l == NULL)
		l = &lwp0;

	return l;
}

int
rump_splfoo()
{

	if (rumpuser_whatis_ipl() != RUMPUSER_IPL_INTR) {
		rumpuser_rw_enter(&rumpspl, 0);
		rumpuser_set_ipl(RUMPUSER_IPL_SPLFOO);
	}

	return 0;
}

static void
rump_intr_enter(void)
{

	rumpuser_set_ipl(RUMPUSER_IPL_INTR);
	rumpuser_rw_enter(&rumpspl, 1);
}

static void
rump_intr_exit(void)
{

	rumpuser_rw_exit(&rumpspl);
	rumpuser_clear_ipl(RUMPUSER_IPL_INTR);
}

void
rump_splx(int dummy)
{

	if (rumpuser_whatis_ipl() != RUMPUSER_IPL_INTR) {
		rumpuser_clear_ipl(RUMPUSER_IPL_SPLFOO);
		rumpuser_rw_exit(&rumpspl);
	}
}

void
rump_biodone(void *arg, size_t count, int error)
{
	struct buf *bp = arg;

	bp->b_resid = bp->b_bcount - count;
	KASSERT(bp->b_resid >= 0);
	bp->b_error = error;

	rump_intr_enter();
	biodone(bp);
	rump_intr_exit();
}

int _syspuffs_stub(int, int *);
int
_syspuffs_stub(int fd, int *newfd)
{

	return ENODEV;
}

__weak_alias(syspuffs_glueinit,_syspuffs_stub);
