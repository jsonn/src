/*	$NetBSD: puffs_vnops.c,v 1.98.4.1 2007/10/06 15:29:50 yamt Exp $	*/

/*
 * Copyright (c) 2005, 2006, 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Google Summer of Code program and the Ulla Tuominen Foundation.
 * The Google SoC project was mentored by Bill Studenmund.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: puffs_vnops.c,v 1.98.4.1 2007/10/06 15:29:50 yamt Exp $");

#include <sys/param.h>
#include <sys/fstrans.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/proc.h>

#include <uvm/uvm.h>

#include <fs/puffs/puffs_msgif.h>
#include <fs/puffs/puffs_sys.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

int	puffs_lookup(void *);
int	puffs_create(void *);
int	puffs_access(void *);
int	puffs_mknod(void *);
int	puffs_open(void *);
int	puffs_close(void *);
int	puffs_getattr(void *);
int	puffs_setattr(void *);
int	puffs_reclaim(void *);
int	puffs_readdir(void *);
int	puffs_poll(void *);
int	puffs_fsync(void *);
int	puffs_seek(void *);
int	puffs_remove(void *);
int	puffs_mkdir(void *);
int	puffs_rmdir(void *);
int	puffs_link(void *);
int	puffs_readlink(void *);
int	puffs_symlink(void *);
int	puffs_rename(void *);
int	puffs_read(void *);
int	puffs_write(void *);
int	puffs_fcntl(void *);
int	puffs_ioctl(void *);
int	puffs_inactive(void *);
int	puffs_print(void *);
int	puffs_pathconf(void *);
int	puffs_advlock(void *);
int	puffs_strategy(void *);
int	puffs_bmap(void *);
int	puffs_mmap(void *);
int	puffs_getpages(void *);

int	puffs_spec_read(void *);
int	puffs_spec_write(void *);
int	puffs_fifo_read(void *);
int	puffs_fifo_write(void *);

int	puffs_checkop(void *);


/* VOP_LEASE() not included */

int	puffs_generic(void *);

#if 0
#define puffs_lock genfs_lock
#define puffs_unlock genfs_unlock
#define puffs_islocked genfs_islocked
#else
int puffs_lock(void *);
int puffs_unlock(void *);
int puffs_islocked(void *);
#endif

int (**puffs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc puffs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, puffs_lookup },		/* REAL lookup */
	{ &vop_create_desc, puffs_checkop },		/* create */
        { &vop_mknod_desc, puffs_checkop },		/* mknod */
        { &vop_open_desc, puffs_open },			/* REAL open */
        { &vop_close_desc, puffs_checkop },		/* close */
        { &vop_access_desc, puffs_access },		/* REAL access */
        { &vop_getattr_desc, puffs_checkop },		/* getattr */
        { &vop_setattr_desc, puffs_checkop },		/* setattr */
        { &vop_read_desc, puffs_checkop },		/* read */
        { &vop_write_desc, puffs_checkop },		/* write */
        { &vop_fsync_desc, puffs_fsync },		/* REAL fsync */
        { &vop_seek_desc, puffs_checkop },		/* seek */
        { &vop_remove_desc, puffs_checkop },		/* remove */
        { &vop_link_desc, puffs_checkop },		/* link */
        { &vop_rename_desc, puffs_checkop },		/* rename */
        { &vop_mkdir_desc, puffs_checkop },		/* mkdir */
        { &vop_rmdir_desc, puffs_checkop },		/* rmdir */
        { &vop_symlink_desc, puffs_checkop },		/* symlink */
        { &vop_readdir_desc, puffs_checkop },		/* readdir */
        { &vop_readlink_desc, puffs_checkop },		/* readlink */
        { &vop_getpages_desc, puffs_checkop },		/* getpages */
        { &vop_putpages_desc, genfs_putpages },		/* REAL putpages */
        { &vop_pathconf_desc, puffs_checkop },		/* pathconf */
        { &vop_advlock_desc, puffs_checkop },		/* advlock */
        { &vop_strategy_desc, puffs_strategy },		/* REAL strategy */
        { &vop_revoke_desc, genfs_revoke },		/* REAL revoke */
        { &vop_abortop_desc, genfs_abortop },		/* REAL abortop */
        { &vop_inactive_desc, puffs_inactive },		/* REAL inactive */
        { &vop_reclaim_desc, puffs_reclaim },		/* REAL reclaim */
        { &vop_lock_desc, puffs_lock },			/* REAL lock */
        { &vop_unlock_desc, puffs_unlock },		/* REAL unlock */
        { &vop_bmap_desc, puffs_bmap },			/* REAL bmap */
        { &vop_print_desc, puffs_print },		/* REAL print */
        { &vop_islocked_desc, puffs_islocked },		/* REAL islocked */
        { &vop_bwrite_desc, genfs_nullop },		/* REAL bwrite */
        { &vop_mmap_desc, puffs_mmap },			/* REAL mmap */
        { &vop_poll_desc, puffs_poll },			/* REAL poll */

        { &vop_kqfilter_desc, genfs_eopnotsupp },	/* kqfilter XXX */
	{ NULL, NULL }
};
const struct vnodeopv_desc puffs_vnodeop_opv_desc =
	{ &puffs_vnodeop_p, puffs_vnodeop_entries };


int (**puffs_specop_p)(void *);
const struct vnodeopv_entry_desc puffs_specop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup, ENOTDIR */
	{ &vop_create_desc, spec_create },		/* genfs_badop */
	{ &vop_mknod_desc, spec_mknod },		/* genfs_badop */
	{ &vop_open_desc, spec_open },			/* spec_open */
	{ &vop_close_desc, spec_close },		/* spec_close */
	{ &vop_access_desc, puffs_checkop },		/* access */
	{ &vop_getattr_desc, puffs_checkop },		/* getattr */
	{ &vop_setattr_desc, puffs_checkop },		/* setattr */
	{ &vop_read_desc, puffs_spec_read },		/* update, read */
	{ &vop_write_desc, puffs_spec_write },		/* update, write */
	{ &vop_lease_desc, spec_lease_check },		/* genfs_nullop */
	{ &vop_ioctl_desc, spec_ioctl },		/* spec_ioctl */
	{ &vop_fcntl_desc, genfs_fcntl },		/* dummy */
	{ &vop_poll_desc, spec_poll },			/* spec_poll */
	{ &vop_kqfilter_desc, spec_kqfilter },		/* spec_kqfilter */
	{ &vop_revoke_desc, spec_revoke },		/* genfs_revoke */
	{ &vop_mmap_desc, spec_mmap },			/* spec_mmap */
	{ &vop_fsync_desc, spec_fsync },		/* vflushbuf */
	{ &vop_seek_desc, spec_seek },			/* genfs_nullop */
	{ &vop_remove_desc, spec_remove },		/* genfs_badop */
	{ &vop_link_desc, spec_link },			/* genfs_badop */
	{ &vop_rename_desc, spec_rename },		/* genfs_badop */
	{ &vop_mkdir_desc, spec_mkdir },		/* genfs_badop */
	{ &vop_rmdir_desc, spec_rmdir },		/* genfs_badop */
	{ &vop_symlink_desc, spec_symlink },		/* genfs_badop */
	{ &vop_readdir_desc, spec_readdir },		/* genfs_badop */
	{ &vop_readlink_desc, spec_readlink },		/* genfs_badop */
	{ &vop_abortop_desc, spec_abortop },		/* genfs_badop */
	{ &vop_inactive_desc, puffs_inactive },		/* REAL inactive */
	{ &vop_reclaim_desc, puffs_reclaim },		/* REAL reclaim */
	{ &vop_lock_desc, puffs_lock },			/* REAL lock */
	{ &vop_unlock_desc, puffs_unlock },		/* REAL unlock */
	{ &vop_bmap_desc, spec_bmap },			/* dummy */
	{ &vop_strategy_desc, spec_strategy },		/* dev strategy */
	{ &vop_print_desc, puffs_print },		/* REAL print */
	{ &vop_islocked_desc, puffs_islocked },		/* REAL islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* lf_advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, spec_getpages },		/* genfs_getpages */
	{ &vop_putpages_desc, spec_putpages },		/* genfs_putpages */
#if 0
	{ &vop_openextattr_desc, _openextattr },	/* openextattr */
	{ &vop_closeextattr_desc, _closeextattr },	/* closeextattr */
	{ &vop_getextattr_desc, _getextattr },		/* getextattr */
	{ &vop_setextattr_desc, _setextattr },		/* setextattr */
	{ &vop_listextattr_desc, _listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, _deleteextattr },	/* deleteextattr */
#endif
	{ NULL, NULL }
};
const struct vnodeopv_desc puffs_specop_opv_desc =
	{ &puffs_specop_p, puffs_specop_entries };


int (**puffs_fifoop_p)(void *);
const struct vnodeopv_entry_desc puffs_fifoop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, fifo_lookup },		/* lookup, ENOTDIR */
	{ &vop_create_desc, fifo_create },		/* genfs_badop */
	{ &vop_mknod_desc, fifo_mknod },		/* genfs_badop */
	{ &vop_open_desc, fifo_open },			/* open */
	{ &vop_close_desc, fifo_close },		/* close */
	{ &vop_access_desc, puffs_checkop },		/* access */
	{ &vop_getattr_desc, puffs_checkop },		/* getattr */
	{ &vop_setattr_desc, puffs_checkop },		/* setattr */
	{ &vop_read_desc, puffs_fifo_read },		/* read, update */
	{ &vop_write_desc, puffs_fifo_write },		/* write, update */
	{ &vop_lease_desc, fifo_lease_check },		/* genfs_nullop */
	{ &vop_ioctl_desc, fifo_ioctl },		/* ioctl */
	{ &vop_fcntl_desc, genfs_fcntl },		/* dummy */
	{ &vop_poll_desc, fifo_poll },			/* poll */
	{ &vop_kqfilter_desc, fifo_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, fifo_revoke },		/* genfs_revoke */
	{ &vop_mmap_desc, fifo_mmap },			/* genfs_badop */
	{ &vop_fsync_desc, fifo_fsync },		/* genfs_nullop*/
	{ &vop_seek_desc, fifo_seek },			/* genfs_badop */
	{ &vop_remove_desc, fifo_remove },		/* genfs_badop */
	{ &vop_link_desc, fifo_link },			/* genfs_badop */
	{ &vop_rename_desc, fifo_rename },		/* genfs_badop */
	{ &vop_mkdir_desc, fifo_mkdir },		/* genfs_badop */
	{ &vop_rmdir_desc, fifo_rmdir },		/* genfs_badop */
	{ &vop_symlink_desc, fifo_symlink },		/* genfs_badop */
	{ &vop_readdir_desc, fifo_readdir },		/* genfs_badop */
	{ &vop_readlink_desc, fifo_readlink },		/* genfs_badop */
	{ &vop_abortop_desc, fifo_abortop },		/* genfs_badop */
	{ &vop_inactive_desc, puffs_inactive },		/* REAL inactive */
	{ &vop_reclaim_desc, puffs_reclaim },		/* REAL reclaim */
	{ &vop_lock_desc, puffs_lock },			/* REAL lock */
	{ &vop_unlock_desc, puffs_unlock },		/* REAL unlock */
	{ &vop_bmap_desc, fifo_bmap },			/* dummy */
	{ &vop_strategy_desc, fifo_strategy },		/* genfs_badop */
	{ &vop_print_desc, puffs_print },		/* REAL print */
	{ &vop_islocked_desc, puffs_islocked },		/* REAL islocked */
	{ &vop_pathconf_desc, fifo_pathconf },		/* pathconf */
	{ &vop_advlock_desc, fifo_advlock },		/* genfs_einval */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_putpages_desc, fifo_putpages }, 		/* genfs_null_putpages*/
#if 0
	{ &vop_openextattr_desc, _openextattr },	/* openextattr */
	{ &vop_closeextattr_desc, _closeextattr },	/* closeextattr */
	{ &vop_getextattr_desc, _getextattr },		/* getextattr */
	{ &vop_setextattr_desc, _setextattr },		/* setextattr */
	{ &vop_listextattr_desc, _listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, _deleteextattr },	/* deleteextattr */
#endif
	{ NULL, NULL }
};
const struct vnodeopv_desc puffs_fifoop_opv_desc =
	{ &puffs_fifoop_p, puffs_fifoop_entries };


/* "real" vnode operations */
int (**puffs_msgop_p)(void *);
const struct vnodeopv_entry_desc puffs_msgop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_create_desc, puffs_create },		/* create */
        { &vop_mknod_desc, puffs_mknod },		/* mknod */
        { &vop_open_desc, puffs_open },			/* open */
        { &vop_close_desc, puffs_close },		/* close */
        { &vop_access_desc, puffs_access },		/* access */
        { &vop_getattr_desc, puffs_getattr },		/* getattr */
        { &vop_setattr_desc, puffs_setattr },		/* setattr */
        { &vop_read_desc, puffs_read },			/* read */
        { &vop_write_desc, puffs_write },		/* write */
        { &vop_seek_desc, puffs_seek },			/* seek */
        { &vop_remove_desc, puffs_remove },		/* remove */
        { &vop_link_desc, puffs_link },			/* link */
        { &vop_rename_desc, puffs_rename },		/* rename */
        { &vop_mkdir_desc, puffs_mkdir },		/* mkdir */
        { &vop_rmdir_desc, puffs_rmdir },		/* rmdir */
        { &vop_symlink_desc, puffs_symlink },		/* symlink */
        { &vop_readdir_desc, puffs_readdir },		/* readdir */
        { &vop_readlink_desc, puffs_readlink },		/* readlink */
        { &vop_print_desc, puffs_print },		/* print */
        { &vop_islocked_desc, puffs_islocked },		/* islocked */
        { &vop_pathconf_desc, puffs_pathconf },		/* pathconf */
        { &vop_advlock_desc, puffs_advlock },		/* advlock */
        { &vop_getpages_desc, puffs_getpages },		/* getpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc puffs_msgop_opv_desc =
	{ &puffs_msgop_p, puffs_msgop_entries };


#define ERROUT(err)							\
do {									\
	error = err;							\
	goto out;							\
} while (/*CONSTCOND*/0)

/*
 * This is a generic vnode operation handler.  It checks if the necessary
 * operations for the called vnode operation are implemented by userspace
 * and either returns a dummy return value or proceeds to call the real
 * vnode operation from puffs_msgop_v.
 *
 * XXX: this should described elsewhere and autogenerated, the complexity
 * of the vnode operations vectors and their interrelationships is also
 * getting a bit out of hand.  Another problem is that we need this same
 * information in the fs server code, so keeping the two in sync manually
 * is not a viable (long term) plan.
 */

/* not supported, handle locking protocol */
#define CHECKOP_NOTSUPP(op)						\
case VOP_##op##_DESCOFFSET:						\
	if (pmp->pmp_vnopmask[PUFFS_VN_##op] == 0)			\
		return genfs_eopnotsupp(v);				\
	break

/* always succeed, no locking */
#define CHECKOP_SUCCESS(op)						\
case VOP_##op##_DESCOFFSET:						\
	if (pmp->pmp_vnopmask[PUFFS_VN_##op] == 0)			\
		return 0;						\
	break

int
puffs_checkop(void *v)
{
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		spooky mystery contents;
	} */ *ap = v;
	struct vnodeop_desc *desc = ap->a_desc;
	struct puffs_mount *pmp;
	struct vnode *vp;
	int offset, rv;

	offset = ap->a_desc->vdesc_vp_offsets[0];
#ifdef DIAGNOSTIC
	if (offset == VDESC_NO_OFFSET)
		panic("puffs_checkop: no vnode, why did you call me?");
#endif
	vp = *VOPARG_OFFSETTO(struct vnode **, offset, ap);
	pmp = MPTOPUFFSMP(vp->v_mount);

	DPRINTF_VERBOSE(("checkop call %s (%d), vp %p\n",
	    ap->a_desc->vdesc_name, ap->a_desc->vdesc_offset, vp));

	if (!ALLOPS(pmp)) {
		switch (desc->vdesc_offset) {
			CHECKOP_NOTSUPP(CREATE);
			CHECKOP_NOTSUPP(MKNOD);
			CHECKOP_NOTSUPP(GETATTR);
			CHECKOP_NOTSUPP(SETATTR);
			CHECKOP_NOTSUPP(READ);
			CHECKOP_NOTSUPP(WRITE);
			CHECKOP_NOTSUPP(FCNTL);
			CHECKOP_NOTSUPP(IOCTL);
			CHECKOP_NOTSUPP(REMOVE);
			CHECKOP_NOTSUPP(LINK);
			CHECKOP_NOTSUPP(RENAME);
			CHECKOP_NOTSUPP(MKDIR);
			CHECKOP_NOTSUPP(RMDIR);
			CHECKOP_NOTSUPP(SYMLINK);
			CHECKOP_NOTSUPP(READDIR);
			CHECKOP_NOTSUPP(READLINK);
			CHECKOP_NOTSUPP(PRINT);
			CHECKOP_NOTSUPP(PATHCONF);
			CHECKOP_NOTSUPP(ADVLOCK);

			CHECKOP_SUCCESS(ACCESS);
			CHECKOP_SUCCESS(CLOSE);
			CHECKOP_SUCCESS(SEEK);

		case VOP_GETPAGES_DESCOFFSET:
			if (!EXISTSOP(pmp, READ))
				return genfs_eopnotsupp(v);
			break;

		default:
			panic("puffs_checkop: unhandled vnop %d",
			    desc->vdesc_offset);
		}
	}

	rv = VOCALL(puffs_msgop_p, ap->a_desc->vdesc_offset, v);

	DPRINTF_VERBOSE(("checkop return %s (%d), vp %p: %d\n",
	    ap->a_desc->vdesc_name, ap->a_desc->vdesc_offset, vp, rv));

	return rv;
}

static int puffs_callremove(struct puffs_mount *, void *, void *,
			    struct componentname *);
static int puffs_callrmdir(struct puffs_mount *, void *, void *,
			   struct componentname *);
static void puffs_callinactive(struct puffs_mount *, void *, int, struct lwp *);
static void puffs_callreclaim(struct puffs_mount *, void *, struct lwp *);

#define PUFFS_ABORT_LOOKUP	1
#define PUFFS_ABORT_CREATE	2
#define PUFFS_ABORT_MKNOD	3
#define PUFFS_ABORT_MKDIR	4
#define PUFFS_ABORT_SYMLINK	5

/*
 * Press the pani^Wabort button!  Kernel resource allocation failed.
 */
static void
puffs_abortbutton(struct puffs_mount *pmp, int what,
	void *dcookie, void *cookie, struct componentname *cnp)
{

	switch (what) {
	case PUFFS_ABORT_CREATE:
	case PUFFS_ABORT_MKNOD:
	case PUFFS_ABORT_SYMLINK:
		puffs_callremove(pmp, dcookie, cookie, cnp);
		break;
	case PUFFS_ABORT_MKDIR:
		puffs_callrmdir(pmp, dcookie, cookie, cnp);
		break;
	}

	puffs_callinactive(pmp, cookie, 0, cnp->cn_lwp);
	puffs_callreclaim(pmp, cookie, cnp->cn_lwp);
}

int
puffs_lookup(void *v)
{
        struct vop_lookup_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
        } */ *ap = v;
	struct puffs_mount *pmp;
	struct componentname *cnp;
	struct vnode *vp, *dvp;
	struct puffs_node *dpn;
	int isdot;
	int error;

	PUFFS_VNREQ(lookup);

	pmp = MPTOPUFFSMP(ap->a_dvp->v_mount);
	cnp = ap->a_cnp;
	dvp = ap->a_dvp;
	*ap->a_vpp = NULL;

	/* r/o fs?  we check create later to handle EEXIST */
	if ((cnp->cn_flags & ISLASTCN)
	    && (dvp->v_mount->mnt_flag & MNT_RDONLY)
	    && (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return EROFS;

	isdot = cnp->cn_namelen == 1 && *cnp->cn_nameptr == '.';

	DPRINTF(("puffs_lookup: \"%s\", parent vnode %p, op: %x\n",
	    cnp->cn_nameptr, dvp, cnp->cn_nameiop));

	/*
	 * Check if someone fed it into the cache
	 */
	if (PUFFS_USE_NAMECACHE(pmp)) {
		error = cache_lookup(dvp, ap->a_vpp, cnp);

		if (error >= 0)
			return error;
	}

	if (isdot) {
		vp = ap->a_dvp;
		vref(vp);
		*ap->a_vpp = vp;
		return 0;
	}

	puffs_makecn(&lookup_arg.pvnr_cn, &lookup_arg.pvnr_cn_cred,
	    &lookup_arg.pvnr_cn_cid, cnp, PUFFS_USE_FULLPNBUF(pmp));

	if (cnp->cn_flags & ISDOTDOT)
		VOP_UNLOCK(dvp, 0);

	error = puffs_vntouser(pmp, PUFFS_VN_LOOKUP,
	    &lookup_arg, sizeof(lookup_arg), 0, dvp, NULL);
	DPRINTF(("puffs_lookup: return of the userspace, part %d\n", error));

	/*
	 * In case of error, there is no new vnode to play with, so be
	 * happy with the NULL value given to vpp in the beginning.
	 * Also, check if this really was an error or the target was not
	 * present.  Either treat it as a non-error for CREATE/RENAME or
	 * enter the component into the negative name cache (if desired).
	 */
	if (error) {
		error = checkerr(pmp, error, __func__);
		if (error == ENOENT) {
			/* don't allow to create files on r/o fs */
			if ((dvp->v_mount->mnt_flag & MNT_RDONLY)
			    && cnp->cn_nameiop == CREATE) {
				error = EROFS;

			/* adjust values if we are creating */
			} else if ((cnp->cn_flags & ISLASTCN)
			    && (cnp->cn_nameiop == CREATE
			      || cnp->cn_nameiop == RENAME)) {
				cnp->cn_flags |= SAVENAME;
				error = EJUSTRETURN;

			/* save negative cache entry */
			} else {
				if ((cnp->cn_flags & MAKEENTRY)
				    && PUFFS_USE_NAMECACHE(pmp))
					cache_enter(dvp, NULL, cnp);
			}
		}
		goto out;
	}

	/*
	 * Check that we don't get our parent node back, that would cause
	 * a pretty obvious deadlock.
	 */
	dpn = dvp->v_data;
	if (lookup_arg.pvnr_newnode == dpn->pn_cookie) {
		puffs_errnotify(pmp, PUFFS_ERR_LOOKUP, EINVAL,
		    "lookup produced parent cookie", lookup_arg.pvnr_newnode);
		error = EPROTO;
		goto out;
	}

	error = puffs_cookie2vnode(pmp, lookup_arg.pvnr_newnode, 1, 1, &vp);
	if (error == PUFFS_NOSUCHCOOKIE) {
		error = puffs_getvnode(dvp->v_mount,
		    lookup_arg.pvnr_newnode, lookup_arg.pvnr_vtype,
		    lookup_arg.pvnr_size, lookup_arg.pvnr_rdev, &vp);
		if (error) {
			puffs_abortbutton(pmp, PUFFS_ABORT_LOOKUP, VPTOPNC(dvp),
			    lookup_arg.pvnr_newnode, ap->a_cnp);
			goto out;
		}
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	} else if (error) {
		puffs_abortbutton(pmp, PUFFS_ABORT_LOOKUP, VPTOPNC(dvp),
		    lookup_arg.pvnr_newnode, ap->a_cnp);
		goto out;
	}

	*ap->a_vpp = vp;

	if ((cnp->cn_flags & MAKEENTRY) != 0 && PUFFS_USE_NAMECACHE(pmp))
		cache_enter(dvp, vp, cnp);

	/* XXX */
	if ((lookup_arg.pvnr_cn.pkcn_flags & REQUIREDIR) == 0)
		cnp->cn_flags &= ~REQUIREDIR;
	if (lookup_arg.pvnr_cn.pkcn_consume)
		cnp->cn_consume = MIN(lookup_arg.pvnr_cn.pkcn_consume,
		    strlen(cnp->cn_nameptr) - cnp->cn_namelen);

 out:
	if (cnp->cn_flags & ISDOTDOT)
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);

	DPRINTF(("puffs_lookup: returning %d %p\n", error, *ap->a_vpp));
	return error;
}

int
puffs_create(void *v)
{
	struct vop_create_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct puffs_mount *pmp = MPTOPUFFSMP(ap->a_dvp->v_mount);
	int error;

	PUFFS_VNREQ(create);

	DPRINTF(("puffs_create: dvp %p, cnp: %s\n",
	    ap->a_dvp, ap->a_cnp->cn_nameptr));

	puffs_makecn(&create_arg.pvnr_cn, &create_arg.pvnr_cn_cred,
	    &create_arg.pvnr_cn_cid, ap->a_cnp, PUFFS_USE_FULLPNBUF(pmp));
	create_arg.pvnr_va = *ap->a_vap;

	error = puffs_vntouser(pmp, PUFFS_VN_CREATE,
	    &create_arg, sizeof(create_arg), 0, ap->a_dvp, NULL);
	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	error = puffs_newnode(ap->a_dvp->v_mount, ap->a_dvp, ap->a_vpp,
	    create_arg.pvnr_newnode, ap->a_cnp, ap->a_vap->va_type, 0);
	if (error)
		puffs_abortbutton(pmp, PUFFS_ABORT_CREATE, VPTOPNC(ap->a_dvp),
		    create_arg.pvnr_newnode, ap->a_cnp);

 out:
	if (error || (ap->a_cnp->cn_flags & SAVESTART) == 0)
		PNBUF_PUT(ap->a_cnp->cn_pnbuf);
	vput(ap->a_dvp);

	DPRINTF(("puffs_create: return %d\n", error));
	return error;
}

int
puffs_mknod(void *v)
{
	struct vop_mknod_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct puffs_mount *pmp = MPTOPUFFSMP(ap->a_dvp->v_mount);
	int error;

	PUFFS_VNREQ(mknod);

	puffs_makecn(&mknod_arg.pvnr_cn, &mknod_arg.pvnr_cn_cred,
	    &mknod_arg.pvnr_cn_cid, ap->a_cnp, PUFFS_USE_FULLPNBUF(pmp));
	mknod_arg.pvnr_va = *ap->a_vap;

	error = puffs_vntouser(pmp, PUFFS_VN_MKNOD,
	    &mknod_arg, sizeof(mknod_arg), 0, ap->a_dvp, NULL);
	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	error = puffs_newnode(ap->a_dvp->v_mount, ap->a_dvp, ap->a_vpp,
	    mknod_arg.pvnr_newnode, ap->a_cnp, ap->a_vap->va_type,
	    ap->a_vap->va_rdev);
	if (error)
		puffs_abortbutton(pmp, PUFFS_ABORT_MKNOD, VPTOPNC(ap->a_dvp),
		    mknod_arg.pvnr_newnode, ap->a_cnp);

 out:
	if (error || (ap->a_cnp->cn_flags & SAVESTART) == 0)
		PNBUF_PUT(ap->a_cnp->cn_pnbuf);
	vput(ap->a_dvp);
	return error;
}

int
puffs_open(void *v)
{
	struct vop_open_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int mode = ap->a_mode;
	int error;

	PUFFS_VNREQ(open);
	DPRINTF(("puffs_open: vp %p, mode 0x%x\n", vp, mode));

	if (vp->v_type == VREG && mode & FWRITE && !EXISTSOP(pmp, WRITE))
		ERROUT(EROFS);

	if (!EXISTSOP(pmp, OPEN))
		ERROUT(0);

	open_arg.pvnr_mode = mode;
	puffs_credcvt(&open_arg.pvnr_cred, ap->a_cred);
	puffs_cidcvt(&open_arg.pvnr_cid, ap->a_l);

	error = puffs_vntouser(MPTOPUFFSMP(vp->v_mount), PUFFS_VN_OPEN,
	    &open_arg, sizeof(open_arg), 0, vp, NULL);
	error = checkerr(pmp, error, __func__);

 out:
	DPRINTF(("puffs_open: returning %d\n", error));
	return error;
}

int
puffs_close(void *v)
{
	struct vop_close_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_fflag;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct puffs_vnreq_close *close_argp;

	close_argp = malloc(sizeof(struct puffs_vnreq_close),
	    M_PUFFS, M_WAITOK | M_ZERO);
	close_argp->pvnr_fflag = ap->a_fflag;
	puffs_credcvt(&close_argp->pvnr_cred, ap->a_cred);
	puffs_cidcvt(&close_argp->pvnr_cid, ap->a_l);

	puffs_vntouser_faf(MPTOPUFFSMP(ap->a_vp->v_mount), PUFFS_VN_CLOSE,
	    close_argp, sizeof(struct puffs_vnreq_close), ap->a_vp);

	return 0;
}

int
puffs_access(void *v)
{
	struct vop_access_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int mode = ap->a_mode;
	int error;

	PUFFS_VNREQ(access);

	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if ((vp->v_mount->mnt_flag & MNT_RDONLY)
			    || !EXISTSOP(pmp, WRITE))
				return EROFS;
			break;
		default:
			break;
		}
	}

	if (!EXISTSOP(pmp, ACCESS))
		return 0;

	access_arg.pvnr_mode = ap->a_mode;
	puffs_credcvt(&access_arg.pvnr_cred, ap->a_cred);
	puffs_cidcvt(&access_arg.pvnr_cid, ap->a_l);

	error = puffs_vntouser(pmp, PUFFS_VN_ACCESS,
	    &access_arg, sizeof(access_arg), 0, vp, NULL);
	return checkerr(pmp, error, __func__);
}

int
puffs_getattr(void *v)
{
	struct vop_getattr_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct puffs_mount *pmp;
	struct mount *mp;
	struct vnode *vp;
	struct vattr *vap, *rvap;
	struct puffs_node *pn;
	int error;

	PUFFS_VNREQ(getattr);

	vp = ap->a_vp;
	mp = vp->v_mount;
	vap = ap->a_vap;
	pmp = MPTOPUFFSMP(mp);

	vattr_null(&getattr_arg.pvnr_va);
	puffs_credcvt(&getattr_arg.pvnr_cred, ap->a_cred);
	puffs_cidcvt(&getattr_arg.pvnr_cid, ap->a_l);

	error = puffs_vntouser(MPTOPUFFSMP(vp->v_mount), PUFFS_VN_GETATTR,
	    &getattr_arg, sizeof(getattr_arg), 0, vp, NULL);
	error = checkerr(pmp, error, __func__);
	if (error)
		return error;

	rvap = &getattr_arg.pvnr_va;
	/*
	 * Don't listen to the file server regarding special device
	 * size info, the file server doesn't know anything about them.
	 */
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		rvap->va_size = vp->v_size;

	/* Ditto for blocksize (ufs comment: this doesn't belong here) */
	if (vp->v_type == VBLK)
		rvap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		rvap->va_blocksize = MAXBSIZE;

	(void) memcpy(vap, rvap, sizeof(struct vattr));
	vap->va_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];

	pn = VPTOPP(vp);
	if (pn->pn_stat & PNODE_METACACHE_ATIME)
		vap->va_atime = pn->pn_mc_atime;
	if (pn->pn_stat & PNODE_METACACHE_CTIME)
		vap->va_ctime = pn->pn_mc_ctime;
	if (pn->pn_stat & PNODE_METACACHE_MTIME)
		vap->va_mtime = pn->pn_mc_mtime;
	if (pn->pn_stat & PNODE_METACACHE_SIZE) {
		vap->va_size = pn->pn_mc_size;
	} else {
		if (rvap->va_size != VNOVAL
		    && vp->v_type != VBLK && vp->v_type != VCHR) {
			uvm_vnp_setsize(vp, rvap->va_size);
			pn->pn_serversize = rvap->va_size;
		}
	}

	return 0;
}

static int
puffs_dosetattr(struct vnode *vp, struct vattr *vap, kauth_cred_t cred,
	struct lwp *l, int chsize)
{
	struct puffs_node *pn = vp->v_data;
	int error;

	PUFFS_VNREQ(setattr);

	if ((vp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL
	    || vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL
	    || vap->va_mode != (mode_t)VNOVAL))
		return EROFS;

	if ((vp->v_mount->mnt_flag & MNT_RDONLY)
	    && vp->v_type == VREG && vap->va_size != VNOVAL)
		return EROFS;

	/*
	 * Flush metacache first.  If we are called with some explicit
	 * parameters, treat them as information overriding metacache
	 * information.
	 */
	if (pn->pn_stat & PNODE_METACACHE_MASK) {
		if ((pn->pn_stat & PNODE_METACACHE_ATIME)
		    && vap->va_atime.tv_sec == VNOVAL)
			vap->va_atime = pn->pn_mc_atime;
		if ((pn->pn_stat & PNODE_METACACHE_CTIME)
		    && vap->va_ctime.tv_sec == VNOVAL)
			vap->va_ctime = pn->pn_mc_ctime;
		if ((pn->pn_stat & PNODE_METACACHE_MTIME)
		    && vap->va_mtime.tv_sec == VNOVAL)
			vap->va_mtime = pn->pn_mc_mtime;
		if ((pn->pn_stat & PNODE_METACACHE_SIZE)
		    && vap->va_size == VNOVAL)
			vap->va_size = pn->pn_mc_size;

		pn->pn_stat &= ~PNODE_METACACHE_MASK;
	}

	(void)memcpy(&setattr_arg.pvnr_va, vap, sizeof(struct vattr));
	puffs_credcvt(&setattr_arg.pvnr_cred, cred);
	puffs_cidcvt(&setattr_arg.pvnr_cid, l);

	error = puffs_vntouser(MPTOPUFFSMP(vp->v_mount), PUFFS_VN_SETATTR,
	    &setattr_arg, sizeof(setattr_arg), 0, vp, NULL);
	error = checkerr(MPTOPUFFSMP(vp->v_mount), error, __func__);
	if (error)
		return error;

	if (vap->va_size != VNOVAL) {
		pn->pn_serversize = vap->va_size;
		if (chsize)
			uvm_vnp_setsize(vp, vap->va_size);
	}

	return 0;
}

int
puffs_setattr(void *v)
{
	struct vop_getattr_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;

	return puffs_dosetattr(ap->a_vp, ap->a_vap, ap->a_cred, ap->a_l, 1);
}

static void
puffs_callinactive(struct puffs_mount *pmp, void *cookie, int iaflag,
	struct lwp *l)
{
	int call;

	PUFFS_VNREQ(inactive);

	puffs_cidcvt(&inactive_arg.pvnr_cid, l);

	if (EXISTSOP(pmp, INACTIVE))
		if (pmp->pmp_flags & PUFFS_KFLAG_IAONDEMAND)
			if (iaflag || ALLOPS(pmp))
				call = 1;
			else
				call = 0;
		else
			call = 1;
	else
		call = 0;

	if (call)
		puffs_cookietouser(pmp, PUFFS_VN_INACTIVE,
		    &inactive_arg, sizeof(inactive_arg), cookie, 0);
}

int
puffs_inactive(void *v)
{
	struct vop_inactive_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct puffs_node *pnode;

	pnode = vp->v_data;

	puffs_callinactive(MPTOPUFFSMP(vp->v_mount), VPTOPNC(vp),
	    pnode->pn_stat & PNODE_DOINACT, ap->a_l);
	pnode->pn_stat &= ~PNODE_DOINACT;

	VOP_UNLOCK(ap->a_vp, 0);

	/*
	 * file server thinks it's gone?  then don't be afraid care,
	 * node's life was already all it would ever be
	 */
	if (pnode->pn_stat & PNODE_NOREFS) {
		pnode->pn_stat |= PNODE_DYING;
		vrecycle(ap->a_vp, NULL, ap->a_l);
	}

	return 0;
}

static void
puffs_callreclaim(struct puffs_mount *pmp, void *cookie, struct lwp *l)
{
	struct puffs_vnreq_reclaim *reclaim_argp;

	if (!EXISTSOP(pmp, RECLAIM))
		return;

	reclaim_argp = malloc(sizeof(struct puffs_vnreq_reclaim),
	    M_PUFFS, M_WAITOK | M_ZERO);
	puffs_cidcvt(&reclaim_argp->pvnr_cid, l);

	puffs_cookietouser(pmp, PUFFS_VN_RECLAIM,
	    reclaim_argp, sizeof(struct puffs_vnreq_reclaim), cookie, 1);
}

/*
 * always FAF, we don't really care if the server wants to fail to
 * reclaim the node or not
 */
int
puffs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);

	/*
	 * first things first: check if someone is trying to reclaim the
	 * root vnode.  do not allow that to travel to userspace.
	 * Note that we don't need to take the lock similarly to
	 * puffs_root(), since there is only one of us.
	 */
	if (vp->v_flag & VROOT) {
		mutex_enter(&pmp->pmp_lock);
		KASSERT(pmp->pmp_root != NULL);
		pmp->pmp_root = NULL;
		mutex_exit(&pmp->pmp_lock);
		goto out;
	}

	puffs_callreclaim(MPTOPUFFSMP(vp->v_mount), VPTOPNC(vp), ap->a_l);

 out:
	if (PUFFS_USE_NAMECACHE(pmp))
		cache_purge(vp);
	puffs_putvnode(vp);

	return 0;
}

#define CSIZE sizeof(**ap->a_cookies)
int
puffs_readdir(void *v)
{
	struct vop_readdir_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		int *a_eofflag;
		off_t **a_cookies;
		int *a_ncookies;
	} */ *ap = v;
	struct puffs_mount *pmp = MPTOPUFFSMP(ap->a_vp->v_mount);
	struct puffs_vnreq_readdir *readdir_argp;
	struct vnode *vp = ap->a_vp;
	size_t argsize, tomove, cookiemem, cookiesmax;
	struct uio *uio = ap->a_uio;
	size_t howmuch, resid;
	int error;

	/*
	 * ok, so we need: resid + cookiemem = maxreq
	 * => resid + cookiesize * (resid/minsize) = maxreq
	 * => resid + cookiesize/minsize * resid = maxreq
	 * => (cookiesize/minsize + 1) * resid = maxreq
	 * => resid = maxreq / (cookiesize/minsize + 1)
	 * 
	 * Since cookiesize <= minsize and we're not very big on floats,
	 * we approximate that to be 1.  Therefore:
	 * 
	 * resid = maxreq / 2;
	 *
	 * Well, at least we didn't have to use differential equations
	 * or the Gram-Schmidt process.
	 *
	 * (yes, I'm very afraid of this)
	 */
	KASSERT(CSIZE <= _DIRENT_MINSIZE((struct dirent *)0));

	if (ap->a_cookies) {
		KASSERT(ap->a_ncookies != NULL);
		if (pmp->pmp_args.pa_fhsize == 0)
			return EOPNOTSUPP;
		resid = PUFFS_TOMOVE(uio->uio_resid, pmp) / 2;
		cookiesmax = resid/_DIRENT_MINSIZE((struct dirent *)0);
		cookiemem = ALIGN(cookiesmax*CSIZE); /* play safe */
	} else {
		resid = PUFFS_TOMOVE(uio->uio_resid, pmp);
		cookiesmax = 0;
		cookiemem = 0;
	}

	argsize = sizeof(struct puffs_vnreq_readdir);
	tomove = resid + cookiemem;
	readdir_argp = malloc(argsize + tomove, M_PUFFS, M_ZERO | M_WAITOK);

	puffs_credcvt(&readdir_argp->pvnr_cred, ap->a_cred);
	readdir_argp->pvnr_offset = uio->uio_offset;
	readdir_argp->pvnr_resid = resid;
	readdir_argp->pvnr_ncookies = cookiesmax;
	readdir_argp->pvnr_eofflag = 0;
	readdir_argp->pvnr_dentoff = cookiemem;

	error = puffs_vntouser(pmp, PUFFS_VN_READDIR,
	    readdir_argp, argsize, tomove, vp, NULL);
	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	/* userspace is cheating? */
	if (readdir_argp->pvnr_resid > resid) {
		puffs_errnotify(pmp, PUFFS_ERR_READDIR, E2BIG,
		    "resid grew", VPTOPNC(vp));
		ERROUT(EPROTO);
	}
	if (readdir_argp->pvnr_ncookies > cookiesmax) {
		puffs_errnotify(pmp, PUFFS_ERR_READDIR, E2BIG,
		    "too many cookies", VPTOPNC(vp));
		ERROUT(EPROTO);
	}

	/* check eof */
	if (readdir_argp->pvnr_eofflag)
		*ap->a_eofflag = 1;

	/* bouncy-wouncy with the directory data */
	howmuch = resid - readdir_argp->pvnr_resid;

	/* force eof if no data was returned (getcwd() needs this) */
	if (howmuch == 0) {
		*ap->a_eofflag = 1;
		goto out;
	}

	error = uiomove(readdir_argp->pvnr_data + cookiemem, howmuch, uio);
	if (error)
		goto out;

	/* provide cookies to caller if so desired */
	if (ap->a_cookies) {
		*ap->a_cookies = malloc(readdir_argp->pvnr_ncookies*CSIZE,
		    M_TEMP, M_WAITOK);
		*ap->a_ncookies = readdir_argp->pvnr_ncookies;
		memcpy(*ap->a_cookies, readdir_argp->pvnr_data,
		    *ap->a_ncookies*CSIZE);
	}

	/* next readdir starts here */
	uio->uio_offset = readdir_argp->pvnr_offset;

 out:
	free(readdir_argp, M_PUFFS);
	return error;
}
#undef CSIZE

/*
 * poll works by consuming the bitmask in pn_revents.  If there are
 * events available, poll returns immediately.  If not, it issues a
 * poll to userspace, selrecords itself and returns with no available
 * events.  When the file server returns, it executes puffs_parkdone_poll(),
 * where available events are added to the bitmask.  selnotify() is
 * then also executed by that function causing us to enter here again
 * and hopefully find the missing bits (unless someone got them first,
 * in which case it starts all over again).
 */
int
puffs_poll(void *v)
{
	struct vop_poll_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_events;
		struct lwp *a_l;
	}�*/ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	struct puffs_vnreq_poll *poll_argp;
	struct puffs_node *pn = vp->v_data;
	int events;

	if (EXISTSOP(pmp, POLL)) {
		mutex_enter(&pn->pn_mtx);
		events = pn->pn_revents & ap->a_events;
		if (events & ap->a_events) {
			pn->pn_revents &= ~ap->a_events;
			mutex_exit(&pn->pn_mtx);

			return events;
		} else {
			puffs_referencenode(pn);
			mutex_exit(&pn->pn_mtx);

			/* freed in puffs_parkdone_poll */
			poll_argp = malloc(sizeof(struct puffs_vnreq_poll),
			    M_PUFFS, M_ZERO | M_WAITOK);

			poll_argp->pvnr_events = ap->a_events;
			puffs_cidcvt(&poll_argp->pvnr_cid, ap->a_l);

			selrecord(ap->a_l, &pn->pn_sel);
			puffs_vntouser_call(pmp, PUFFS_VN_POLL,
			    poll_argp, sizeof(struct puffs_vnreq_poll), 0,
			    puffs_parkdone_poll, pn,
			    vp, NULL);

			return 0;
		}
	} else {
		return genfs_poll(v);
	}
}

int
puffs_fsync(void *v)
{
	struct vop_fsync_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t a_offlo;
		off_t a_offhi;
		struct lwp *a_l;
	} */ *ap = v;
	struct vattr va;
	struct puffs_mount *pmp;
	struct puffs_vnreq_fsync *fsync_argp;
	struct vnode *vp;
	struct puffs_node *pn;
	int pflags, error, dofaf;

	PUFFS_VNREQ(fsync);

	vp = ap->a_vp;
	pn = VPTOPP(vp);
	pmp = MPTOPUFFSMP(vp->v_mount);

	/* flush out information from our metacache, see vop_setattr */
	if (pn->pn_stat & PNODE_METACACHE_MASK
	    && (pn->pn_stat & PNODE_DYING) == 0) {
		vattr_null(&va);
		error = VOP_SETATTR(vp, &va, FSCRED, NULL); 
		if (error)
			return error;
	}

	/*
	 * flush pages to avoid being overly dirty
	 */
	pflags = PGO_CLEANIT;
	if (ap->a_flags & FSYNC_WAIT)
		pflags |= PGO_SYNCIO;
	simple_lock(&vp->v_interlock);
	error = VOP_PUTPAGES(vp, trunc_page(ap->a_offlo),
	    round_page(ap->a_offhi), pflags);
	if (error)
		return error;

	/*
	 * HELLO!  We exit already here if the user server does not
	 * support fsync OR if we should call fsync for a node which
	 * has references neither in the kernel or the fs server.
	 * Otherwise we continue to issue fsync() forward.
	 */
	if (!EXISTSOP(pmp, FSYNC) || (pn->pn_stat & PNODE_DYING))
		return 0;

	dofaf = (ap->a_flags & FSYNC_WAIT) == 0 || ap->a_flags == FSYNC_LAZY;
	/*
	 * We abuse VXLOCK to mean "vnode is going to die", so we issue
	 * only FAFs for those.  Otherwise there's a danger of deadlock,
	 * since the execution context here might be the user server
	 * doing some operation on another fs, which in turn caused a
	 * vnode to be reclaimed from the freelist for this fs.
	 */
	if (dofaf == 0) {
		simple_lock(&vp->v_interlock);
		if (vp->v_flag & VXLOCK)
			dofaf = 1;
		simple_unlock(&vp->v_interlock);
	}

	if (dofaf == 0) {
		fsync_argp = &fsync_arg;
	} else {
		fsync_argp = malloc(sizeof(struct puffs_vnreq_fsync),
		    M_PUFFS, M_ZERO | M_NOWAIT);
		if (fsync_argp == NULL)
			return ENOMEM;
	}

	puffs_credcvt(&fsync_argp->pvnr_cred, ap->a_cred);
	fsync_argp->pvnr_flags = ap->a_flags;
	fsync_argp->pvnr_offlo = ap->a_offlo;
	fsync_argp->pvnr_offhi = ap->a_offhi;
	puffs_cidcvt(&fsync_argp->pvnr_cid, ap->a_l);

	/*
	 * XXX: see comment at puffs_getattr about locking
	 *
	 * If we are not required to wait, do a FAF operation.
	 * Otherwise block here.
	 */
	if (dofaf == 0) {
		error =  puffs_vntouser(MPTOPUFFSMP(vp->v_mount),
		    PUFFS_VN_FSYNC, fsync_argp, sizeof(*fsync_argp), 0,
		    vp, NULL);
		error = checkerr(pmp, error, __func__);
	} else {
		/* FAF is always "succesful" */
		error = 0;
		puffs_vntouser_faf(MPTOPUFFSMP(vp->v_mount),
		    PUFFS_VN_FSYNC, fsync_argp, sizeof(*fsync_argp), vp);
	}

	return error;
}

int
puffs_seek(void *v)
{
	struct vop_seek_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		off_t a_oldoff;
		off_t a_newoff;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int error;

	PUFFS_VNREQ(seek);

	seek_arg.pvnr_oldoff = ap->a_oldoff;
	seek_arg.pvnr_newoff = ap->a_newoff;
	puffs_credcvt(&seek_arg.pvnr_cred, ap->a_cred);

	/*
	 * XXX: seems like seek is called with an unlocked vp, but
	 * it can't hurt to play safe
	 */
	error = puffs_vntouser(MPTOPUFFSMP(vp->v_mount), PUFFS_VN_SEEK,
	    &seek_arg, sizeof(seek_arg), 0, vp, NULL);
	return checkerr(MPTOPUFFSMP(vp->v_mount), error, __func__);
}

static int
puffs_callremove(struct puffs_mount *pmp, void *dcookie, void *cookie,
	struct componentname *cnp)
{
	int error;

	PUFFS_VNREQ(remove);

	remove_arg.pvnr_cookie_targ = cookie;
	puffs_makecn(&remove_arg.pvnr_cn, &remove_arg.pvnr_cn_cred,
	    &remove_arg.pvnr_cn_cid, cnp, PUFFS_USE_FULLPNBUF(pmp));

	error = puffs_cookietouser(pmp, PUFFS_VN_REMOVE,
	    &remove_arg, sizeof(remove_arg), dcookie, 0);
	return checkerr(pmp, error, __func__);
}

int
puffs_remove(void *v)
{
	struct vop_remove_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(dvp->v_mount);
	int error;

	error = puffs_callremove(pmp, VPTOPNC(dvp), VPTOPNC(vp), ap->a_cnp);

	vput(vp);
	if (dvp == vp)
		vrele(dvp);
	else
		vput(dvp);

	return error;
}

int
puffs_mkdir(void *v)
{
	struct vop_mkdir_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct puffs_mount *pmp = MPTOPUFFSMP(ap->a_dvp->v_mount);
	int error;

	PUFFS_VNREQ(mkdir);

	puffs_makecn(&mkdir_arg.pvnr_cn, &mkdir_arg.pvnr_cn_cred,
	    &mkdir_arg.pvnr_cn_cid, ap->a_cnp, PUFFS_USE_FULLPNBUF(pmp));
	mkdir_arg.pvnr_va = *ap->a_vap;

	error = puffs_vntouser(pmp, PUFFS_VN_MKDIR,
	    &mkdir_arg, sizeof(mkdir_arg), 0, ap->a_dvp, NULL);
	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	error = puffs_newnode(ap->a_dvp->v_mount, ap->a_dvp, ap->a_vpp,
	    mkdir_arg.pvnr_newnode, ap->a_cnp, VDIR, 0);
	if (error)
		puffs_abortbutton(pmp, PUFFS_ABORT_MKDIR, VPTOPNC(ap->a_dvp),
		    mkdir_arg.pvnr_newnode, ap->a_cnp);

 out:
	if (error || (ap->a_cnp->cn_flags & SAVESTART) == 0)
		PNBUF_PUT(ap->a_cnp->cn_pnbuf);
	vput(ap->a_dvp);
	return error;
}

static int
puffs_callrmdir(struct puffs_mount *pmp, void *dcookie, void *cookie,
	struct componentname *cnp)
{
	int error;

	PUFFS_VNREQ(rmdir);

	rmdir_arg.pvnr_cookie_targ = cookie;
	puffs_makecn(&rmdir_arg.pvnr_cn, &rmdir_arg.pvnr_cn_cred,
	    &rmdir_arg.pvnr_cn_cid, cnp, PUFFS_USE_FULLPNBUF(pmp));

	error = puffs_cookietouser(pmp, PUFFS_VN_RMDIR,
	    &rmdir_arg, sizeof(rmdir_arg), dcookie, 0);
	return checkerr(pmp, error, __func__);
}

int
puffs_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(dvp->v_mount);
	int error;

	error = puffs_callrmdir(pmp, VPTOPNC(dvp), VPTOPNC(vp), ap->a_cnp);

	/* XXX: some call cache_purge() *for both vnodes* here, investigate */

	vput(dvp);
	vput(vp);

	return error;
}

int
puffs_link(void *v)
{
	struct vop_link_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	}�*/ *ap = v;
	struct puffs_mount *pmp = MPTOPUFFSMP(ap->a_dvp->v_mount);
	int error;

	PUFFS_VNREQ(link);

	link_arg.pvnr_cookie_targ = VPTOPNC(ap->a_vp);
	puffs_makecn(&link_arg.pvnr_cn, &link_arg.pvnr_cn_cred,
	    &link_arg.pvnr_cn_cid, ap->a_cnp, PUFFS_USE_FULLPNBUF(pmp));

	error = puffs_vntouser(pmp, PUFFS_VN_LINK,
	    &link_arg, sizeof(link_arg), 0, ap->a_dvp, ap->a_vp);
	error = checkerr(pmp, error, __func__);

	/*
	 * XXX: stay in touch with the cache.  I don't like this, but
	 * don't have a better solution either.  See also puffs_rename().
	 */
	if (error == 0)
		puffs_updatenode(ap->a_vp, PUFFS_UPDATECTIME);

	vput(ap->a_dvp);

	return error;
}

int
puffs_symlink(void *v)
{
	struct vop_symlink_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	}�*/ *ap = v;
	struct puffs_mount *pmp = MPTOPUFFSMP(ap->a_dvp->v_mount);
	struct puffs_vnreq_symlink *symlink_argp;
	int error;

	*ap->a_vpp = NULL;

	symlink_argp = malloc(sizeof(struct puffs_vnreq_symlink),
	    M_PUFFS, M_ZERO | M_WAITOK);
	puffs_makecn(&symlink_argp->pvnr_cn, &symlink_argp->pvnr_cn_cred,
		&symlink_argp->pvnr_cn_cid, ap->a_cnp, PUFFS_USE_FULLPNBUF(pmp));
	symlink_argp->pvnr_va = *ap->a_vap;
	(void)strlcpy(symlink_argp->pvnr_link, ap->a_target,
	    sizeof(symlink_argp->pvnr_link));

	error =  puffs_vntouser(pmp, PUFFS_VN_SYMLINK,
	    symlink_argp, sizeof(*symlink_argp), 0, ap->a_dvp, NULL);
	error = checkerr(pmp, error, __func__);
	if (error)
		goto out;

	error = puffs_newnode(ap->a_dvp->v_mount, ap->a_dvp, ap->a_vpp,
	    symlink_argp->pvnr_newnode, ap->a_cnp, VLNK, 0);
	if (error)
		puffs_abortbutton(pmp, PUFFS_ABORT_SYMLINK, VPTOPNC(ap->a_dvp),
		    symlink_argp->pvnr_newnode, ap->a_cnp);

 out:
	free(symlink_argp, M_PUFFS);
	if (error || (ap->a_cnp->cn_flags & SAVESTART) == 0)
		PNBUF_PUT(ap->a_cnp->cn_pnbuf);
	vput(ap->a_dvp);

	return error;
}

int
puffs_readlink(void *v)
{
	struct vop_readlink_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct puffs_mount *pmp = MPTOPUFFSMP(ap->a_vp->v_mount);
	size_t linklen;
	int error;

	PUFFS_VNREQ(readlink);

	puffs_credcvt(&readlink_arg.pvnr_cred, ap->a_cred);
	linklen = sizeof(readlink_arg.pvnr_link);
	readlink_arg.pvnr_linklen = linklen;

	error = puffs_vntouser(MPTOPUFFSMP(ap->a_vp->v_mount),
	    PUFFS_VN_READLINK, &readlink_arg, sizeof(readlink_arg), 0,
	    ap->a_vp, NULL);
	error = checkerr(pmp, error, __func__);
	if (error)
		return error;

	/* bad bad user file server */
	if (readlink_arg.pvnr_linklen > linklen) {
		puffs_errnotify(pmp, PUFFS_ERR_READLINK, E2BIG,
		    "linklen too big", VPTOPNC(ap->a_vp));
		return EPROTO;
	}

	return uiomove(&readlink_arg.pvnr_link, readlink_arg.pvnr_linklen,
	    ap->a_uio);
}

int
puffs_rename(void *v)
{
	struct vop_rename_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	}�*/ *ap = v;
	struct puffs_vnreq_rename *rename_argp = NULL;
	struct puffs_mount *pmp = MPTOPUFFSMP(ap->a_fdvp->v_mount);
	int error;

	if (ap->a_fvp->v_mount != ap->a_tdvp->v_mount)
		ERROUT(EXDEV);

	rename_argp = malloc(sizeof(struct puffs_vnreq_rename),
	    M_PUFFS, M_WAITOK | M_ZERO);

	rename_argp->pvnr_cookie_src = VPTOPNC(ap->a_fvp);
	rename_argp->pvnr_cookie_targdir = VPTOPNC(ap->a_tdvp);
	if (ap->a_tvp)
		rename_argp->pvnr_cookie_targ = VPTOPNC(ap->a_tvp);
	else
		rename_argp->pvnr_cookie_targ = NULL;
	puffs_makecn(&rename_argp->pvnr_cn_src,
	    &rename_argp->pvnr_cn_src_cred, &rename_argp->pvnr_cn_src_cid,
	    ap->a_fcnp, PUFFS_USE_FULLPNBUF(pmp));
	puffs_makecn(&rename_argp->pvnr_cn_targ,
	    &rename_argp->pvnr_cn_targ_cred, &rename_argp->pvnr_cn_targ_cid,
	    ap->a_tcnp, PUFFS_USE_FULLPNBUF(pmp));

	error = puffs_vntouser(pmp, PUFFS_VN_RENAME,
	    rename_argp, sizeof(*rename_argp), 0, ap->a_fdvp, NULL); /* XXX */
	error = checkerr(pmp, error, __func__);

	/*
	 * XXX: stay in touch with the cache.  I don't like this, but
	 * don't have a better solution either.  See also puffs_link().
	 */
	if (error == 0)
		puffs_updatenode(ap->a_fvp, PUFFS_UPDATECTIME);

 out:
	if (rename_argp)
		free(rename_argp, M_PUFFS);
	if (ap->a_tvp != NULL)
		vput(ap->a_tvp);
	if (ap->a_tdvp == ap->a_tvp)
		vrele(ap->a_tdvp);
	else
		vput(ap->a_tdvp);

	vrele(ap->a_fdvp);
	vrele(ap->a_fvp);

	return error;
}

#define RWARGS(cont, iofl, move, offset, creds)				\
	(cont)->pvnr_ioflag = (iofl);					\
	(cont)->pvnr_resid = (move);					\
	(cont)->pvnr_offset = (offset);					\
	puffs_credcvt(&(cont)->pvnr_cred, creds)

int
puffs_read(void *v)
{
	struct vop_read_args /* { 
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct puffs_vnreq_read *read_argp;
	struct puffs_mount *pmp;
	struct vnode *vp;
	struct uio *uio;
	void *win;
	size_t tomove, argsize;
	vsize_t bytelen;
	int error, ubcflags;

	uio = ap->a_uio;
	vp = ap->a_vp;
	read_argp = NULL;
	error = 0;
	pmp = MPTOPUFFSMP(vp->v_mount);

	/* std sanity */
	if (uio->uio_resid == 0)
		return 0;
	if (uio->uio_offset < 0)
		return EINVAL;

	if (vp->v_type == VREG && PUFFS_USE_PAGECACHE(pmp)) {
		const int advice = IO_ADV_DECODE(ap->a_ioflag);

		ubcflags = 0;
		if (UBC_WANT_UNMAP(vp))
			ubcflags = UBC_UNMAP;

		while (uio->uio_resid > 0) {
			bytelen = MIN(uio->uio_resid,
			    vp->v_size - uio->uio_offset);
			if (bytelen == 0)
				break;

			win = ubc_alloc(&vp->v_uobj, uio->uio_offset,
			    &bytelen, advice, UBC_READ);
			error = uiomove(win, bytelen, uio);
			ubc_release(win, ubcflags);
			if (error)
				break;
		}

		if ((vp->v_mount->mnt_flag & MNT_NOATIME) == 0)
			puffs_updatenode(vp, PUFFS_UPDATEATIME);
	} else {
		/*
		 * in case it's not a regular file or we're operating
		 * uncached, do read in the old-fashioned style,
		 * i.e. explicit read operations
		 */

		tomove = PUFFS_TOMOVE(uio->uio_resid, pmp);
		argsize = sizeof(struct puffs_vnreq_read);
		read_argp = malloc(argsize + tomove,
		    M_PUFFS, M_WAITOK | M_ZERO);

		error = 0;
		while (uio->uio_resid > 0) {
			tomove = PUFFS_TOMOVE(uio->uio_resid, pmp);
			RWARGS(read_argp, ap->a_ioflag, tomove,
			    uio->uio_offset, ap->a_cred);

			error = puffs_vntouser(pmp, PUFFS_VN_READ,
			    read_argp, argsize, tomove,
			    ap->a_vp, NULL);
			error = checkerr(pmp, error, __func__);
			if (error)
				break;

			if (read_argp->pvnr_resid > tomove) {
				puffs_errnotify(pmp, PUFFS_ERR_READ,
				    E2BIG, "resid grew", VPTOPNC(ap->a_vp));
				error = EPROTO;
				break;
			}

			error = uiomove(read_argp->pvnr_data,
			    tomove - read_argp->pvnr_resid, uio);

			/*
			 * in case the file is out of juice, resid from
			 * userspace is != 0.  and the error-case is
			 * quite obvious
			 */
			if (error || read_argp->pvnr_resid)
				break;
		}
	}

	if (read_argp)
		free(read_argp, M_PUFFS);
	return error;
}

/*
 * XXX: in case of a failure, this leaves uio in a bad state.
 * We could theoretically copy the uio and iovecs and "replay"
 * them the right amount after the userspace trip, but don't
 * bother for now.
 */
int
puffs_write(void *v)
{
	struct vop_write_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct puffs_vnreq_write *write_argp;
	struct puffs_mount *pmp;
	struct uio *uio;
	struct vnode *vp;
	size_t tomove, argsize;
	off_t oldoff, newoff, origoff;
	vsize_t bytelen;
	int error, uflags;
	int ubcflags;

	vp = ap->a_vp;
	uio = ap->a_uio;
	error = uflags = 0;
	write_argp = NULL;
	pmp = MPTOPUFFSMP(ap->a_vp->v_mount);

	if (vp->v_type == VREG && PUFFS_USE_PAGECACHE(pmp)) {
		ubcflags = UBC_WRITE | UBC_PARTIALOK;
		if (UBC_WANT_UNMAP(vp))
			ubcflags |= UBC_UNMAP;

		/*
		 * userspace *should* be allowed to control this,
		 * but with UBC it's a bit unclear how to handle it
		 */
		if (ap->a_ioflag & IO_APPEND)
			uio->uio_offset = vp->v_size;

		origoff = uio->uio_offset;
		while (uio->uio_resid > 0) {
			uflags |= PUFFS_UPDATECTIME;
			uflags |= PUFFS_UPDATEMTIME;
			oldoff = uio->uio_offset;
			bytelen = uio->uio_resid;

			newoff = oldoff + bytelen;
			if (vp->v_size < newoff) {
				uvm_vnp_setwritesize(vp, newoff);
			}
			error = ubc_uiomove(&vp->v_uobj, uio, bytelen,
			    UVM_ADV_RANDOM, ubcflags);

			/*
			 * In case of a ubc_uiomove() error,
			 * opt to not extend the file at all and
			 * return an error.  Otherwise, if we attempt
			 * to clear the memory we couldn't fault to,
			 * we might generate a kernel page fault.
			 */
			if (vp->v_size < newoff) {
				if (error == 0) {
					uflags |= PUFFS_UPDATESIZE;
					uvm_vnp_setsize(vp, newoff);
				} else {
					uvm_vnp_setwritesize(vp, vp->v_size);
				}
			}
			if (error)
				break;

			/*
			 * If we're writing large files, flush to file server
			 * every 64k.  Otherwise we can very easily exhaust
			 * kernel and user memory, as the file server cannot
			 * really keep up with our writing speed.
			 *
			 * Note: this does *NOT* honor MNT_ASYNC, because
			 * that gives userland too much say in the kernel.
			 */
			if (oldoff >> 16 != uio->uio_offset >> 16) {
				simple_lock(&vp->v_interlock);
				error = VOP_PUTPAGES(vp, oldoff & ~0xffff,
				    uio->uio_offset & ~0xffff,
				    PGO_CLEANIT | PGO_SYNCIO);
				if (error)
					break;
			}
		}

		/* synchronous I/O? */
		if (error == 0 && ap->a_ioflag & IO_SYNC) {
			simple_lock(&vp->v_interlock);
			error = VOP_PUTPAGES(vp, trunc_page(origoff),
			    round_page(uio->uio_offset),
			    PGO_CLEANIT | PGO_SYNCIO);

		/* write through page cache? */
		} else if (error == 0 && pmp->pmp_flags & PUFFS_KFLAG_WTCACHE) {
			simple_lock(&vp->v_interlock);
			error = VOP_PUTPAGES(vp, trunc_page(origoff),
			    round_page(uio->uio_offset), PGO_CLEANIT);
		}

		puffs_updatenode(vp, uflags);
	} else {
		/* tomove is non-increasing */
		tomove = PUFFS_TOMOVE(uio->uio_resid, pmp);
		argsize = sizeof(struct puffs_vnreq_write) + tomove;
		write_argp = malloc(argsize, M_PUFFS, M_WAITOK | M_ZERO);

		while (uio->uio_resid > 0) {
			/* move data to buffer */
			tomove = PUFFS_TOMOVE(uio->uio_resid, pmp);
			RWARGS(write_argp, ap->a_ioflag, tomove,
			    uio->uio_offset, ap->a_cred);
			error = uiomove(write_argp->pvnr_data, tomove, uio);
			if (error)
				break;

			/* move buffer to userspace */
			error = puffs_vntouser(MPTOPUFFSMP(ap->a_vp->v_mount),
			    PUFFS_VN_WRITE, write_argp, argsize, 0,
			    ap->a_vp, NULL);
			error = checkerr(pmp, error, __func__);
			if (error)
				break;

			if (write_argp->pvnr_resid > tomove) {
				puffs_errnotify(pmp, PUFFS_ERR_WRITE,
				    E2BIG, "resid grew", VPTOPNC(ap->a_vp));
				error = EPROTO;
				break;
			}

			/* adjust file size */
			if (vp->v_size < uio->uio_offset)
				uvm_vnp_setsize(vp, uio->uio_offset);

			/* didn't move everything?  bad userspace.  bail */
			if (write_argp->pvnr_resid != 0) {
				error = EIO;
				break;
			}
		}
	}

	if (write_argp)
		free(write_argp, M_PUFFS);
	return error;
}

int
puffs_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct puffs_mount *pmp;
	struct vnode *vp = ap->a_vp;
	struct puffs_node *pn = vp->v_data;

	PUFFS_VNREQ(print);

	pmp = MPTOPUFFSMP(vp->v_mount);

	/* kernel portion */
	printf("tag VT_PUFFS, vnode %p, puffs node: %p,\n"
	    "    userspace cookie: %p\n", vp, pn, pn->pn_cookie);
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
	lockmgr_printinfo(&vp->v_lock);

	/* userspace portion */
	if (EXISTSOP(pmp, PRINT))
		puffs_vntouser(pmp, PUFFS_VN_PRINT,
		    &print_arg, sizeof(print_arg), 0, ap->a_vp, NULL);
	
	return 0;
}

int
puffs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int error;

	PUFFS_VNREQ(pathconf);

	pathconf_arg.pvnr_name = ap->a_name;

	error = puffs_vntouser(pmp, PUFFS_VN_PATHCONF,
	    &pathconf_arg, sizeof(pathconf_arg), 0, vp, NULL);
	error = checkerr(pmp, error, __func__);
	if (error)
		return error;

	*ap->a_retval = pathconf_arg.pvnr_retval;

	return 0;
}

int
puffs_advlock(void *v)
{
	struct vop_advlock_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		void *a_id;
		int a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct puffs_mount *pmp = MPTOPUFFSMP(vp->v_mount);
	int error;

	PUFFS_VNREQ(advlock);

	error = copyin(ap->a_fl, &advlock_arg.pvnr_fl, sizeof(struct flock));
	if (error)
		return error;
	advlock_arg.pvnr_id = ap->a_id;
	advlock_arg.pvnr_op = ap->a_op;
	advlock_arg.pvnr_flags = ap->a_flags;

	error = puffs_vntouser(pmp, PUFFS_VN_ADVLOCK,
	    &advlock_arg, sizeof(advlock_arg), 0, vp, NULL);
	return checkerr(pmp, error, __func__);
}

#define BIOASYNC(bp) (bp->b_flags & B_ASYNC)
#define BIOREAD(bp) (bp->b_flags & B_READ)
#define BIOWRITE(bp) ((bp->b_flags & B_READ) == 0)

/*
 * This maps itself to PUFFS_VN_READ/WRITE for data transfer.
 */
int
puffs_strategy(void *v)
{
	struct vop_strategy_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct puffs_mount *pmp;
	struct vnode *vp = ap->a_vp;
	struct puffs_node *pn;
	struct puffs_vnreq_readwrite *rw_argp = NULL;
	struct buf *bp;
	size_t argsize;
	size_t tomove, moved;
	int error, dofaf;

	pmp = MPTOPUFFSMP(vp->v_mount);
	bp = ap->a_bp;
	error = 0;
	dofaf = 0;
	pn = VPTOPP(vp);

	if ((BIOREAD(bp) && !EXISTSOP(pmp, READ))
	    || (BIOWRITE(bp) && !EXISTSOP(pmp, WRITE)))
		ERROUT(EOPNOTSUPP);

	/*
	 * Short-circuit optimization: don't flush buffer in between
	 * VOP_INACTIVE and VOP_RECLAIM in case the node has no references.
	 */
	if (pn->pn_stat & PNODE_DYING) {
		KASSERT(BIOWRITE(bp));
		bp->b_resid = 0;
		goto out;
	}

#ifdef DIAGNOSTIC
	if (bp->b_bcount > pmp->pmp_req_maxsize - PUFFS_REQSTRUCT_MAX)
		panic("puffs_strategy: wildly inappropriate buf bcount %d",
		    bp->b_bcount);
#endif

	/*
	 * See explanation for the necessity of a FAF in puffs_fsync.
	 *
	 * Also, do FAF in case we're suspending.
	 * See puffs_vfsops.c:pageflush()
	 */
	if (BIOWRITE(bp)) {
		simple_lock(&vp->v_interlock);
		if (vp->v_flag & VXLOCK)
			dofaf = 1;
		if (pn->pn_stat & PNODE_SUSPEND)
			dofaf = 1;
		simple_unlock(&vp->v_interlock);
	}

	if (BIOASYNC(bp))
		dofaf = 1;

#ifdef DIAGNOSTIC
	if (curlwp == uvm.pagedaemon_lwp)
		KASSERT(dofaf);
#endif

	/* allocate transport structure */
	tomove = PUFFS_TOMOVE(bp->b_bcount, pmp);
	argsize = sizeof(struct puffs_vnreq_readwrite);
	rw_argp = malloc(argsize + tomove, M_PUFFS,
	    M_ZERO | (dofaf ? M_NOWAIT : M_WAITOK));
	if (rw_argp == NULL)
		ERROUT(ENOMEM);
	RWARGS(rw_argp, 0, tomove, bp->b_blkno << DEV_BSHIFT, FSCRED);

	/* 2x2 cases: read/write, faf/nofaf */
	if (BIOREAD(bp)) {
		if (dofaf) {
			puffs_vntouser_call(pmp, PUFFS_VN_READ, rw_argp,
			    argsize, tomove, puffs_parkdone_asyncbioread,
			    bp, vp, NULL);
		} else {
			error = puffs_vntouser(pmp, PUFFS_VN_READ,
			    rw_argp, argsize, tomove, vp, NULL);
			error = checkerr(pmp, error, __func__);
			if (error)
				goto out;

			if (rw_argp->pvnr_resid > tomove) {
				puffs_errnotify(pmp, PUFFS_ERR_READ,
				    E2BIG, "resid grew", VPTOPNC(vp));
				ERROUT(EPROTO);
			}

			moved = tomove - rw_argp->pvnr_resid;

			(void)memcpy(bp->b_data, rw_argp->pvnr_data, moved);
			bp->b_resid = bp->b_bcount - moved;
		}
	} else {
		/*
		 * make pages read-only before we write them if we want
		 * write caching info
		 */
		if (PUFFS_WCACHEINFO(pmp)) {
			struct uvm_object *uobj = &vp->v_uobj;
			int npages = (bp->b_bcount + PAGE_SIZE-1) >> PAGE_SHIFT;
			struct vm_page *vmp;
			int i;

			for (i = 0; i < npages; i++) {
				vmp= uvm_pageratop((vaddr_t)bp->b_data
				    + (i << PAGE_SHIFT));
				DPRINTF(("puffs_strategy: write-protecting "
				    "vp %p page %p, offset %" PRId64"\n",
				    vp, vmp, vmp->offset));
				simple_lock(&uobj->vmobjlock);
				vmp->flags |= PG_RDONLY;
				pmap_page_protect(vmp, VM_PROT_READ);
				simple_unlock(&uobj->vmobjlock);
			}
		}

		(void)memcpy(&rw_argp->pvnr_data, bp->b_data, tomove);
		if (dofaf) {
			/*
			 * assume FAF moves everything.  frankly, we don't
			 * really have a choice.
			 */
			puffs_vntouser_faf(MPTOPUFFSMP(vp->v_mount),
			    PUFFS_VN_WRITE, rw_argp, argsize + tomove, vp);
			bp->b_resid = bp->b_bcount - tomove;
		} else {
			error = puffs_vntouser(MPTOPUFFSMP(vp->v_mount),
			    PUFFS_VN_WRITE, rw_argp, argsize + tomove,
			    0, vp, NULL);
			error = checkerr(pmp, error, __func__);
			if (error)
				goto out;

			moved = tomove - rw_argp->pvnr_resid;
			if (rw_argp->pvnr_resid > tomove) {
				puffs_errnotify(pmp, PUFFS_ERR_WRITE,
				    E2BIG, "resid grew", VPTOPNC(vp));
				ERROUT(EPROTO);
			}

			bp->b_resid = bp->b_bcount - moved;
			if (rw_argp->pvnr_resid != 0) {
				ERROUT(EIO);
			}
		}
	}

 out:
	KASSERT(dofaf == 0 || error == 0 || rw_argp == NULL);
	if (rw_argp && !dofaf)
		free(rw_argp, M_PUFFS);

	if (error)
		bp->b_error = error;

	if (error || !(BIOREAD(bp) && BIOASYNC(bp)))
		biodone(bp);

	return error;
}

int
puffs_mmap(void *v)
{
	struct vop_mmap_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		vm_prot_t a_prot;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct puffs_mount *pmp;
	int error;

	PUFFS_VNREQ(mmap);

	pmp = MPTOPUFFSMP(ap->a_vp->v_mount);

	if (!PUFFS_USE_PAGECACHE(pmp))
		return genfs_eopnotsupp(v);

	if (EXISTSOP(pmp, MMAP)) {
		mmap_arg.pvnr_prot = ap->a_prot;
		puffs_credcvt(&mmap_arg.pvnr_cred, ap->a_cred);
		puffs_cidcvt(&mmap_arg.pvnr_cid, ap->a_l);

		error = puffs_vntouser(pmp, PUFFS_VN_MMAP,
		    &mmap_arg, sizeof(mmap_arg), 0,
		    ap->a_vp, NULL);
		error = checkerr(pmp, error, __func__);
	} else {
		error = genfs_mmap(v);
	}

	return error;
}


/*
 * The rest don't get a free trip to userspace and back, they
 * have to stay within the kernel.
 */

/*
 * bmap doesn't really make any sense for puffs, so just 1:1 map it.
 * well, maybe somehow, somewhere, some day ....
 */
int
puffs_bmap(void *v)
{
	struct vop_bmap_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		daddr_t a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;
	struct puffs_mount *pmp;

	pmp = MPTOPUFFSMP(ap->a_vp->v_mount);

	if (ap->a_vpp)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp)
		*ap->a_runp
		    = (PUFFS_TOMOVE(pmp->pmp_req_maxsize, pmp)>>DEV_BSHIFT) - 1;

	return 0;
}

/*
 * Handle getpages faults in puffs.  We let genfs_getpages() do most
 * of the dirty work, but we come in this route to do accounting tasks.
 * If the user server has specified functions for cache notifications
 * about reads and/or writes, we record which type of operation we got,
 * for which page range, and proceed to issue a FAF notification to the
 * server about it.
 */
int
puffs_getpages(void *v)
{
	struct vop_getpages_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		voff_t a_offset;
		struct vm_page **a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;
	struct puffs_mount *pmp;
	struct puffs_node *pn;
	struct vnode *vp;
	struct vm_page **pgs;
	struct puffs_cacheinfo *pcinfo = NULL;
	struct puffs_cacherun *pcrun;
	void *parkmem = NULL;
	size_t runsizes;
	int i, npages, si, streakon;
	int error, locked, write;

	pmp = MPTOPUFFSMP(ap->a_vp->v_mount);
	npages = *ap->a_count;
	pgs = ap->a_m;
	vp = ap->a_vp;
	pn = vp->v_data;
	locked = (ap->a_flags & PGO_LOCKED) != 0;
	write = (ap->a_access_type & VM_PROT_WRITE) != 0;

	/* ccg xnaht - gets Wuninitialized wrong */
	pcrun = NULL;
	runsizes = 0;

	/*
	 * Check that we aren't trying to fault in pages which our file
	 * server doesn't know about.  This happens if we extend a file by
	 * skipping some pages and later try to fault in pages which
	 * are between pn_serversize and vp_size.  This check optimizes
	 * away the common case where a file is being extended.
	 */
	if (ap->a_offset >= pn->pn_serversize && ap->a_offset < vp->v_size) {
		struct vattr va;

		/* try again later when we can block */
		if (locked)
			ERROUT(EBUSY);

		simple_unlock(&vp->v_interlock);
		vattr_null(&va);
		va.va_size = vp->v_size;
		error = puffs_dosetattr(vp, &va, FSCRED, NULL, 0);
		if (error)
			ERROUT(error);
		simple_lock(&vp->v_interlock);
	}

	if (write && PUFFS_WCACHEINFO(pmp)) {
		/* allocate worst-case memory */
		runsizes = ((npages / 2) + 1) * sizeof(struct puffs_cacherun);
		pcinfo = malloc(sizeof(struct puffs_cacheinfo) + runsizes,
		    M_PUFFS, M_ZERO | locked ? M_NOWAIT : M_WAITOK);

		/*
		 * can't block if we're locked and can't mess up caching
		 * information for fs server.  so come back later, please
		 */
		if (pcinfo == NULL)
			ERROUT(ENOMEM);

		parkmem = puffs_park_alloc(locked == 0);
		if (parkmem == NULL)
			ERROUT(ENOMEM);

		pcrun = pcinfo->pcache_runs;
	}

	error = genfs_getpages(v);
	if (error)
		goto out;

	if (PUFFS_WCACHEINFO(pmp) == 0)
		goto out;

	/*
	 * Let's see whose fault it was and inform the user server of
	 * possibly read/written pages.  Map pages from read faults
	 * strictly read-only, since otherwise we might miss info on
	 * when the page is actually write-faulted to.
	 */
	if (!locked)
		simple_lock(&vp->v_uobj.vmobjlock);
	for (i = 0, si = 0, streakon = 0; i < npages; i++) {
		if (pgs[i] == NULL || pgs[i] == PGO_DONTCARE) {
			if (streakon && write) {
				streakon = 0;
				pcrun[si].pcache_runend
				    = trunc_page(pgs[i]->offset) + PAGE_MASK;
				si++;
			}
			continue;
		}
		if (streakon == 0 && write) {
			streakon = 1;
			pcrun[si].pcache_runstart = pgs[i]->offset;
		}
			
		if (!write)
			pgs[i]->flags |= PG_RDONLY;
	}
	/* was the last page part of our streak? */
	if (streakon) {
		pcrun[si].pcache_runend
		    = trunc_page(pgs[i-1]->offset) + PAGE_MASK;
		si++;
	}
	if (!locked)
		simple_unlock(&vp->v_uobj.vmobjlock);

	KASSERT(si <= (npages / 2) + 1);

	/* send results to userspace */
	if (write)
		puffs_cacheop(pmp, parkmem, pcinfo,
		    sizeof(struct puffs_cacheinfo) + runsizes, VPTOPNC(vp));

 out:
	if (error) {
		if (pcinfo != NULL)
			free(pcinfo, M_PUFFS);
		if (parkmem != NULL)
			puffs_park_release(parkmem, 1);
	}

	return error;
}

int
puffs_lock(void *v)
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
	}�*/ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct mount *mp = vp->v_mount;

#if 0
	DPRINTF(("puffs_lock: lock %p, args 0x%x\n", vp, ap->a_flags));
#endif

	/*
	 * XXX: this avoids deadlocking when we're suspending.
	 * e.g. some ops holding the vnode lock might be blocked for
	 * the vfs transaction lock so we'd deadlock.
	 *
	 * Now once again this is skating on the thin ice of modern life,
	 * since we are breaking the consistency guarantee provided
	 * _to the user server_ by vnode locking.  Hopefully this will
	 * get fixed soon enough by getting rid of the dependency on
	 * vnode locks alltogether.
	 */
	if (fstrans_is_owner(mp) && fstrans_getstate(mp) == FSTRANS_SUSPENDING){
		if (ap->a_flags & LK_INTERLOCK)
			simple_unlock(&vp->v_interlock);
		return 0;
	}

	return lockmgr(&vp->v_lock, ap->a_flags, &vp->v_interlock);
}

int
puffs_unlock(void *v)
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct mount *mp = vp->v_mount;

#if 0
	DPRINTF(("puffs_unlock: lock %p, args 0x%x\n", vp, ap->a_flags));
#endif

	/* XXX: see puffs_lock() */
	if (fstrans_is_owner(mp) && fstrans_getstate(mp) == FSTRANS_SUSPENDING){
		if (ap->a_flags & LK_INTERLOCK)
			simple_unlock(&vp->v_interlock);
		return 0;
	}

	return lockmgr(&vp->v_lock, ap->a_flags | LK_RELEASE, &vp->v_interlock);
}

int
puffs_islocked(void *v)
{
	struct vop_islocked_args *ap = v;
	int rv;

	rv = lockstatus(&ap->a_vp->v_lock);
	return rv;
}

int
puffs_generic(void *v)
{
	struct vop_generic_args *ap = v;

	(void)ap;
	DPRINTF(("puffs_generic: ap->a_desc = %s\n", ap->a_desc->vdesc_name));

	return EOPNOTSUPP;
}


/*
 * spec & fifo.  These call the miscfs spec and fifo vectors, but issue
 * FAF update information for the puffs node first.
 */
int
puffs_spec_read(void *v)
{
	struct vop_read_args /* { 
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;

	puffs_updatenode(ap->a_vp, PUFFS_UPDATEATIME);
	return VOCALL(spec_vnodeop_p, VOFFSET(vop_read), v);
}

int
puffs_spec_write(void *v)
{
	struct vop_write_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	}�*/ *ap = v;

	puffs_updatenode(ap->a_vp, PUFFS_UPDATEMTIME);
	return VOCALL(spec_vnodeop_p, VOFFSET(vop_write), v);
}

int
puffs_fifo_read(void *v)
{
	struct vop_read_args /* { 
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;

	puffs_updatenode(ap->a_vp, PUFFS_UPDATEATIME);
	return VOCALL(fifo_vnodeop_p, VOFFSET(vop_read), v);
}

int
puffs_fifo_write(void *v)
{
	struct vop_write_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	}�*/ *ap = v;

	puffs_updatenode(ap->a_vp, PUFFS_UPDATEMTIME);
	return VOCALL(fifo_vnodeop_p, VOFFSET(vop_write), v);
}
