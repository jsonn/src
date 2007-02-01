/*	$NetBSD: sysvbfs_vnops.c,v 1.5.4.2 2007/02/01 08:48:33 ad Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysvbfs_vnops.c,v 1.5.4.2 2007/02/01 08:48:33 ad Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/resource.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/malloc.h>
#include <sys/lockf.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>

#include <fs/sysvbfs/sysvbfs.h>
#include <fs/sysvbfs/bfs.h>

#ifdef SYSVBFS_VNOPS_DEBUG
#define	DPRINTF(fmt, args...)	printf(fmt, ##args)
#else
#define	DPRINTF(arg...)		((void)0)
#endif
#define	ROUND_SECTOR(x)		(((x) + 511) & ~511)

MALLOC_DEFINE(M_SYSVBFS_VNODE, "sysvbfs vnode", "sysvbfs vnode structures");

int
sysvbfs_lookup(void *arg)
{
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *a = arg;
	struct vnode *v = a->a_dvp;
	struct sysvbfs_node *bnode = v->v_data;
	struct bfs *bfs = bnode->bmp->bfs;	/* my filesystem */
	struct vnode *vpp = NULL;
	struct bfs_dirent *dirent = NULL;
	struct componentname *cnp = a->a_cnp;
	int nameiop = cnp->cn_nameiop;
	const char *name = cnp->cn_nameptr;
	int namelen = cnp->cn_namelen;
	int error;
	boolean_t islastcn = cnp->cn_flags & ISLASTCN;

	DPRINTF("%s: %s op=%d %ld\n", __FUNCTION__, name, nameiop,
	    cnp->cn_flags);

	KASSERT((cnp->cn_flags & ISDOTDOT) == 0);
	if ((error = VOP_ACCESS(a->a_dvp, VEXEC, cnp->cn_cred,
	    cnp->cn_lwp)) != 0) {
		return error;	/* directory permittion. */
	}


	if (namelen == 1 && name[0] == '.') {	/* "." */
		VREF(v);
		*a->a_vpp = v;
	} else {				/* Regular file */
		if (!bfs_dirent_lookup_by_name(bfs, cnp->cn_nameptr,
		    &dirent)) {
			if (nameiop != CREATE && nameiop != RENAME) {
				DPRINTF("%s: no such a file. (1)\n",
				    __FUNCTION__);
				return ENOENT;
			}
			if ((error = VOP_ACCESS(v, VWRITE, cnp->cn_cred,
			    cnp->cn_lwp)) != 0)
				return error;
			cnp->cn_flags |= SAVENAME;
			return EJUSTRETURN;
		}

		/* Allocate v-node */
		if ((error = sysvbfs_vget(v->v_mount, dirent->inode, &vpp)) != 0) {
			DPRINTF("%s: can't get vnode.\n", __FUNCTION__);
			return error;
		}
		*a->a_vpp = vpp;
	}

	if (cnp->cn_nameiop != LOOKUP && islastcn)
		cnp->cn_flags |= SAVENAME;

	return 0;
}

int
sysvbfs_create(void *arg)
{
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *a = arg;
	struct sysvbfs_node *bnode = a->a_dvp->v_data;
	struct sysvbfs_mount *bmp = bnode->bmp;
	struct bfs *bfs = bmp->bfs;
	struct mount *mp = bmp->mountp;
	struct bfs_dirent *dirent;
	struct bfs_fileattr attr;
	struct vattr *va = a->a_vap;
	kauth_cred_t cr = a->a_cnp->cn_cred;
	int err = 0;

	DPRINTF("%s: %s\n", __FUNCTION__, a->a_cnp->cn_nameptr);
	KDASSERT(a->a_vap->va_type == VREG);
	attr.uid = kauth_cred_geteuid(cr);
	attr.gid = kauth_cred_getegid(cr);
	attr.mode = va->va_mode;

	if ((err = bfs_file_create(bfs, a->a_cnp->cn_nameptr, 0, 0, &attr))
	    != 0) {
		DPRINTF("%s: bfs_file_create failed.\n", __FUNCTION__);
		goto unlock_exit;
	}

	if (!bfs_dirent_lookup_by_name(bfs, a->a_cnp->cn_nameptr, &dirent))
		panic("no dirent for created file.");

	if ((err = sysvbfs_vget(mp, dirent->inode, a->a_vpp)) != 0) {
		DPRINTF("%s: sysvbfs_vget failed.\n", __FUNCTION__);
		goto unlock_exit;
	}
	bnode = (*a->a_vpp)->v_data;
	bnode->update_ctime = TRUE;
	bnode->update_mtime = TRUE;
	bnode->update_atime = TRUE;

 unlock_exit:
	/* unlock parent directory */
	vput(a->a_dvp);	/* locked at sysvbfs_lookup(); */

	return err;
}

int
sysvbfs_open(void *arg)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct sysvbfs_node *bnode = v->v_data;
	struct bfs_inode *inode = bnode->inode;
	struct bfs *bfs = bnode->bmp->bfs;
	struct bfs_dirent *dirent;

	DPRINTF("%s:\n", __FUNCTION__);
	KDASSERT(v->v_type == VREG || v->v_type == VDIR);

	if (!bfs_dirent_lookup_by_inode(bfs, inode->number, &dirent))
		return ENOENT;
	bnode->update_atime = TRUE;
	if ((a->a_mode & FWRITE) && !(a->a_mode & O_APPEND)) {
		bnode->size = 0;
	} else {
		bnode->size = bfs_file_size(inode);
	}
	bnode->data_block = inode->start_sector;

	return 0;
}

int
sysvbfs_close(void *arg)
{
	struct vop_close_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int  a_fflag;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct sysvbfs_node *bnode = v->v_data;
	struct bfs_fileattr attr;

	DPRINTF("%s:\n", __FUNCTION__);
	uvm_vnp_setsize(v, bnode->size);

	memset(&attr, 0xff, sizeof attr);	/* Set VNOVAL all */
	if (bnode->update_atime)
		attr.atime = time_second;
	if (bnode->update_ctime)
		attr.ctime = time_second;
	if (bnode->update_mtime)
		attr.mtime = time_second;
	bfs_inode_set_attr(bnode->bmp->bfs, bnode->inode, &attr);

	VOP_FSYNC(a->a_vp, a->a_cred, FSYNC_WAIT, 0, 0, a->a_l);

	return 0;
}

int
sysvbfs_access(void *arg)
{
	struct vop_access_args /* {
		struct vnode	*a_vp;
		int		a_mode;
		kauth_cred_t	a_cred;
		struct lwp	*a_l;
	} */ *ap = arg;
	struct vnode *vp = ap->a_vp;
	struct sysvbfs_node *bnode = vp->v_data;
	struct bfs_fileattr *attr = &bnode->inode->attr;

	DPRINTF("%s:\n", __FUNCTION__);
	if ((ap->a_mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY))
		return EROFS;

	return vaccess(vp->v_type, attr->mode, attr->uid, attr->gid,
	    ap->a_mode, ap->a_cred);
}

int
sysvbfs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct sysvbfs_node *bnode = vp->v_data;
	struct bfs_inode *inode = bnode->inode;
	struct bfs_fileattr *attr = &inode->attr;
	struct sysvbfs_mount *bmp = bnode->bmp;
	struct vattr *vap = ap->a_vap;

	DPRINTF("%s:\n", __FUNCTION__);

	vap->va_type = vp->v_type;
	vap->va_mode = attr->mode;
	vap->va_nlink = attr->nlink;
	vap->va_uid = attr->uid;
	vap->va_gid = attr->gid;
	vap->va_fsid = bmp->devvp->v_rdev;
	vap->va_fileid = inode->number;
	vap->va_size = bfs_file_size(inode);
	vap->va_blocksize = BFS_BSIZE;
	vap->va_atime.tv_sec = attr->atime;
	vap->va_mtime.tv_sec = attr->mtime;
	vap->va_ctime.tv_sec = attr->ctime;
	vap->va_birthtime.tv_sec = 0;
	vap->va_gen = 1;
	vap->va_flags = 0;
	vap->va_rdev = 0;	/* No device file */
	vap->va_bytes = vap->va_size;
	vap->va_filerev = 0;
	vap->va_vaflags = 0;

	return 0;
}

int
sysvbfs_setattr(void *arg)
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct proc *p;
	} */ *ap = arg;
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	struct sysvbfs_node *bnode = vp->v_data;
	struct bfs_inode *inode = bnode->inode;
	struct bfs_fileattr *attr = &inode->attr;
	struct bfs *bfs = bnode->bmp->bfs;

	DPRINTF("%s:\n", __FUNCTION__);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL))
		return EINVAL;

	if (vap->va_uid != (uid_t)VNOVAL)
		attr->uid = vap->va_uid;
	if (vap->va_gid != (uid_t)VNOVAL)
		attr->gid = vap->va_gid;
	if (vap->va_mode != (mode_t)VNOVAL)
		attr->mode = vap->va_mode;
	if (vap->va_atime.tv_sec != VNOVAL)
		attr->atime = vap->va_atime.tv_sec;
	if (vap->va_mtime.tv_sec != VNOVAL)
		attr->mtime = vap->va_mtime.tv_sec;
	if (vap->va_ctime.tv_sec != VNOVAL)
		attr->ctime = vap->va_ctime.tv_sec;

	bfs_inode_set_attr(bfs, inode, attr);

	return 0;
}

int
sysvbfs_read(void *arg)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct uio *uio = a->a_uio;
	struct sysvbfs_node *bnode = v->v_data;
	struct bfs_inode *inode = bnode->inode;
	vsize_t sz, filesz = bfs_file_size(inode);
	int err;
	void *win;
	const int advice = IO_ADV_DECODE(a->a_ioflag);

	DPRINTF("%s: type=%d\n", __FUNCTION__, v->v_type);
	if (v->v_type != VREG)
		return EINVAL;

	while (uio->uio_resid > 0) {
		if ((sz = MIN(filesz - uio->uio_offset, uio->uio_resid)) == 0)
			break;

		win = ubc_alloc(&v->v_uobj, uio->uio_offset, &sz, advice,
		    UBC_READ);
		err = uiomove(win, sz, uio);
		ubc_release(win, 0);
		if (err)
			break;
		DPRINTF("%s: read %ldbyte\n", __FUNCTION__, sz);
	}

	return  sysvbfs_update(v, NULL, NULL, UPDATE_WAIT);
}

int
sysvbfs_write(void *arg)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct uio *uio = a->a_uio;
	struct sysvbfs_node *bnode = v->v_data;
	struct bfs_inode *inode = bnode->inode;
	boolean_t extended = FALSE;
	vsize_t sz;
	void *win;
	int err = 0;

	if (a->a_vp->v_type != VREG)
		return EISDIR;

	if (a->a_ioflag & IO_APPEND)
		uio->uio_offset = bnode->size;

	if (uio->uio_resid == 0)
		return 0;

	if (bnode->size < uio->uio_offset + uio->uio_resid) {
		bnode->size = uio->uio_offset + uio->uio_resid;
		uvm_vnp_setsize(v, bnode->size);
		extended = TRUE;
	}

	while (uio->uio_resid > 0) {
		sz = uio->uio_resid;
		win = ubc_alloc(&v->v_uobj, uio->uio_offset, &sz,
		    UVM_ADV_NORMAL, UBC_WRITE);
		err = uiomove(win, sz, uio);
		ubc_release(win, 0);
		if (err)
			break;
		DPRINTF("%s: write %ldbyte\n", __FUNCTION__, sz);
	}
	inode->end_sector = bnode->data_block +
	    (ROUND_SECTOR(bnode->size) >> DEV_BSHIFT) - 1;
	inode->eof_offset_byte = bnode->data_block * DEV_BSIZE +
	    bnode->size - 1;
	bnode->update_mtime = TRUE;

	VN_KNOTE(v, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));

	return err;
}

int
sysvbfs_remove(void *arg)
{
	struct vop_remove_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode * a_vp;
		struct componentname * a_cnp;
	} */ *ap = arg;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct sysvbfs_node *bnode = vp->v_data;
	struct sysvbfs_mount *bmp = bnode->bmp;
	struct bfs *bfs = bmp->bfs;
	int err;

	DPRINTF("%s: delete %s\n", __FUNCTION__, ap->a_cnp->cn_nameptr);

	if (vp->v_type == VDIR)
		return EPERM;

	if ((err = bfs_file_delete(bfs, ap->a_cnp->cn_nameptr)) != 0)
		DPRINTF("%s: bfs_file_delete failed.\n", __FUNCTION__);

	VN_KNOTE(ap->a_vp, NOTE_DELETE);
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);

	return err;
}

int
sysvbfs_rename(void *arg)
{
	struct vop_rename_args /* {
		struct vnode *a_fdvp;	from parent-directory v-node
		struct vnode *a_fvp;	from file v-node
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;	to parent-directory
		struct vnode *a_tvp;	to file v-node
		struct componentname *a_tcnp;
	} */ *ap = arg;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct sysvbfs_node *bnode = fvp->v_data;
	struct bfs *bfs = bnode->bmp->bfs;
	const char *from_name = ap->a_fcnp->cn_nameptr;
	const char *to_name = ap->a_tcnp->cn_nameptr;
	int error;

	DPRINTF("%s: %s->%s\n", __FUNCTION__, from_name, to_name);
	if ((fvp->v_mount != ap->a_tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		printf("cross-device link\n");
		goto out;
	}

	KDASSERT(fvp->v_type == VREG);
	KDASSERT(tvp == NULL ? TRUE : tvp->v_type == VREG);

	error = bfs_file_rename(bfs, from_name, to_name);
 out:
	vput(ap->a_tdvp);
	if (tvp)
		vput(ap->a_tvp);  /* locked on entry */
	if (ap->a_tdvp != ap->a_fdvp)
		vrele(ap->a_fdvp);
	vrele(ap->a_fvp); /* unlocked and refcnt is incremented on entry. */

	return 0;
}

int
sysvbfs_readdir(void *v)
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		int *a_eofflag;
		off_t **a_cookies;
		int *a_ncookies;
	} */ *ap = v;
	struct uio *uio = ap->a_uio;
	struct vnode *vp = ap->a_vp;
	struct sysvbfs_node *bnode = vp->v_data;
	struct bfs *bfs = bnode->bmp->bfs;
	struct dirent d;
	struct bfs_dirent *file;
	int i, n, error;

	DPRINTF("%s: offset=%lld residue=%d\n", __FUNCTION__,
	    uio->uio_offset, uio->uio_resid);

	KDASSERT(vp->v_type == VDIR);
	KDASSERT(uio->uio_offset >= 0);

	i = uio->uio_offset / sizeof(struct dirent);
	n = uio->uio_resid / sizeof(struct dirent);
	if ((i + n) > bfs->n_dirent)
		n = bfs->n_dirent - i;

	for (file = &bfs->dirent[i]; i < n; file++) {
		if (file->inode == 0)
			continue;
		if (i == bfs->max_dirent) {
			DPRINTF("%s: file system inconsistent.\n",
			    __FUNCTION__);
			break;
		}
		i++;
		memset(&d, 0, sizeof d);
		d.d_fileno = file->inode;
		d.d_type = file->inode == BFS_ROOT_INODE ? DT_DIR : DT_REG;
		d.d_namlen = strlen(file->name);
		strncpy(d.d_name, file->name, BFS_FILENAME_MAXLEN);
		d.d_reclen = sizeof(struct dirent);
		if ((error = uiomove(&d, d.d_reclen, uio)) != 0) {
			DPRINTF("%s: uiomove failed.\n", __FUNCTION__);
			return error;
		}
	}
	DPRINTF("%s: %d %d %d\n", __FUNCTION__, i, n, bfs->n_dirent);
	*ap->a_eofflag = i == bfs->n_dirent;

	return 0;
}

int
sysvbfs_inactive(void *arg)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct lwp *a_l;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct lwp *l = a->a_l;

	DPRINTF("%s:\n", __FUNCTION__);
	VOP_UNLOCK(v, 0);
	vrecycle(v, NULL, l);

	return 0;
}

int
sysvbfs_reclaim(void *v)
{
	extern struct pool sysvbfs_node_pool;
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct sysvbfs_node *bnode = vp->v_data;

	DPRINTF("%s:\n", __FUNCTION__);
	simple_lock(&mntvnode_slock);
	LIST_REMOVE(bnode, link);
	simple_unlock(&mntvnode_slock);
	cache_purge(vp);
	pool_put(&sysvbfs_node_pool, bnode);
	vp->v_data = NULL;

	return 0;
}

int
sysvbfs_bmap(void *arg)
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *a = arg;
	struct vnode *v = a->a_vp;
	struct sysvbfs_node *bnode = v->v_data;
	struct sysvbfs_mount *bmp = bnode->bmp;
	struct bfs_inode *inode = bnode->inode;
	daddr_t blk;

	DPRINTF("%s:\n", __FUNCTION__);
	/* BFS algorithm is contiguous allocation */
	blk = inode->start_sector + a->a_bn;

	if (blk * BFS_BSIZE > bmp->bfs->data_end)
		return ENOSPC;

	*a->a_vpp = bmp->devvp;
	*a->a_runp = 0;
	DPRINTF("%s: %d + %lld\n", __FUNCTION__, inode->start_sector, a->a_bn);

	*a->a_bnp = blk;


	return 0;
}

int
sysvbfs_strategy(void *arg)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *a = arg;
	struct buf *b = a->a_bp;
	struct vnode *v = a->a_vp;
	struct sysvbfs_node *bnode = v->v_data;
	struct sysvbfs_mount *bmp = bnode->bmp;
	int error;

	DPRINTF("%s:\n", __FUNCTION__);
	KDASSERT(v->v_type == VREG);
	if (b->b_blkno == b->b_lblkno) {
		error = VOP_BMAP(v, b->b_lblkno, NULL, &b->b_blkno, NULL);
		if (error) {
			b->b_error = error;
			b->b_flags |= B_ERROR;
			biodone(b);
			return error;
		}
		if ((long)b->b_blkno == -1)
			clrbuf(b);
	}
	if ((long)b->b_blkno == -1) {
		biodone(b);
		return 0;
	}

	return VOP_STRATEGY(bmp->devvp, b);
}

int
sysvbfs_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct sysvbfs_node *bnode = ap->a_vp->v_data;

	DPRINTF("%s:\n", __FUNCTION__);
	bfs_dump(bnode->bmp->bfs);

	return 0;
}

int
sysvbfs_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		void *a_id;
		int a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap = v;
	struct sysvbfs_node *bnode = ap->a_vp->v_data;

	DPRINTF("%s: op=%d\n", __FUNCTION__, ap->a_op);

	return lf_advlock(ap, &bnode->lockf, bfs_file_size(bnode->inode));
}

int
sysvbfs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;
	int err = 0;

	DPRINTF("%s:\n", __FUNCTION__);

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 1;
		break;;
	case _PC_NAME_MAX:
		*ap->a_retval = BFS_FILENAME_MAXLEN;
		break;;
	case _PC_PATH_MAX:
		*ap->a_retval = BFS_FILENAME_MAXLEN;
		break;;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		break;;
	case _PC_NO_TRUNC:
		*ap->a_retval = 0;
		break;;
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		break;;
	case _PC_FILESIZEBITS:
		*ap->a_retval = 32;
		break;;
	default:
		err = EINVAL;
		break;
	}

	return err;
}

int
sysvbfs_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t offlo;
		off_t offhi;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int error, wait;

	if (ap->a_flags & FSYNC_CACHE) {
		return EOPNOTSUPP;
	}

	wait = (ap->a_flags & FSYNC_WAIT) != 0;
	vflushbuf(vp, wait);

	if ((ap->a_flags & FSYNC_DATAONLY) != 0)
		error = 0;
	else
		error = sysvbfs_update(vp, NULL, NULL, wait ? UPDATE_WAIT : 0);

	return error;
}

int
sysvbfs_update(struct vnode *vp, const struct timespec *acc,
    const struct timespec *mod, int flags)
{
	struct sysvbfs_node *bnode = vp->v_data;
	struct bfs_fileattr attr;

	DPRINTF("%s:\n", __FUNCTION__);
	memset(&attr, 0xff, sizeof attr);	/* Set VNOVAL all */
	if (bnode->update_atime) {
		attr.atime = acc ? acc->tv_sec : time_second;
		bnode->update_atime = FALSE;
	}
	if (bnode->update_ctime) {
		attr.ctime = time_second;
		bnode->update_ctime = FALSE;
	}
	if (bnode->update_mtime) {
		attr.mtime = mod ? mod->tv_sec : time_second;
		bnode->update_mtime = FALSE;
	}
	bfs_inode_set_attr(bnode->bmp->bfs, bnode->inode, &attr);

	return 0;
}
