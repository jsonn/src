/*	$NetBSD: nfs_vnops.c,v 1.113.4.1 2000/07/30 20:38:57 jdolecek Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_vnops.c	8.19 (Berkeley) 7/31/95
 */

/*
 * vnode op calls for Sun NFS version 2 and 3
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include <vm/vm.h>

#include <uvm/uvm_extern.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nqnfs.h>
#include <nfs/nfs_var.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

/* Defs */
#define	TRUE	1
#define	FALSE	0

/*
 * Global vfs data structures for nfs
 */
int (**nfsv2_vnodeop_p) __P((void *));
struct vnodeopv_entry_desc nfsv2_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, nfs_lookup },		/* lookup */
	{ &vop_create_desc, nfs_create },		/* create */
	{ &vop_mknod_desc, nfs_mknod },			/* mknod */
	{ &vop_open_desc, nfs_open },			/* open */
	{ &vop_close_desc, nfs_close },			/* close */
	{ &vop_access_desc, nfs_access },		/* access */
	{ &vop_getattr_desc, nfs_getattr },		/* getattr */
	{ &vop_setattr_desc, nfs_setattr },		/* setattr */
	{ &vop_read_desc, nfs_read },			/* read */
	{ &vop_write_desc, nfs_write },			/* write */
	{ &vop_lease_desc, nfs_lease_check },		/* lease */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, nfs_ioctl },			/* ioctl */
	{ &vop_poll_desc, nfs_poll },			/* poll */
	{ &vop_revoke_desc, nfs_revoke },		/* revoke */
	{ &vop_mmap_desc, nfs_mmap },			/* mmap */
	{ &vop_fsync_desc, nfs_fsync },			/* fsync */
	{ &vop_seek_desc, nfs_seek },			/* seek */
	{ &vop_remove_desc, nfs_remove },		/* remove */
	{ &vop_link_desc, nfs_link },			/* link */
	{ &vop_rename_desc, nfs_rename },		/* rename */
	{ &vop_mkdir_desc, nfs_mkdir },			/* mkdir */
	{ &vop_rmdir_desc, nfs_rmdir },			/* rmdir */
	{ &vop_symlink_desc, nfs_symlink },		/* symlink */
	{ &vop_readdir_desc, nfs_readdir },		/* readdir */
	{ &vop_readlink_desc, nfs_readlink },		/* readlink */
	{ &vop_abortop_desc, nfs_abortop },		/* abortop */
	{ &vop_inactive_desc, nfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, nfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, nfs_lock },			/* lock */
	{ &vop_unlock_desc, nfs_unlock },		/* unlock */
	{ &vop_bmap_desc, nfs_bmap },			/* bmap */
	{ &vop_strategy_desc, nfs_strategy },		/* strategy */
	{ &vop_print_desc, nfs_print },			/* print */
	{ &vop_islocked_desc, nfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, nfs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, nfs_advlock },		/* advlock */
	{ &vop_blkatoff_desc, nfs_blkatoff },		/* blkatoff */
	{ &vop_valloc_desc, nfs_valloc },		/* valloc */
	{ &vop_reallocblks_desc, nfs_reallocblks },	/* reallocblks */
	{ &vop_vfree_desc, nfs_vfree },			/* vfree */
	{ &vop_truncate_desc, nfs_truncate },		/* truncate */
	{ &vop_update_desc, nfs_update },		/* update */
	{ &vop_bwrite_desc, nfs_bwrite },		/* bwrite */
	{ (struct vnodeop_desc*)NULL, (int(*) __P((void *)))NULL }
};
struct vnodeopv_desc nfsv2_vnodeop_opv_desc =
	{ &nfsv2_vnodeop_p, nfsv2_vnodeop_entries };

/*
 * Special device vnode ops
 */
int (**spec_nfsv2nodeop_p) __P((void *));
struct vnodeopv_entry_desc spec_nfsv2nodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup */
	{ &vop_create_desc, spec_create },		/* create */
	{ &vop_mknod_desc, spec_mknod },		/* mknod */
	{ &vop_open_desc, spec_open },			/* open */
	{ &vop_close_desc, nfsspec_close },		/* close */
	{ &vop_access_desc, nfsspec_access },		/* access */
	{ &vop_getattr_desc, nfs_getattr },		/* getattr */
	{ &vop_setattr_desc, nfs_setattr },		/* setattr */
	{ &vop_read_desc, nfsspec_read },		/* read */
	{ &vop_write_desc, nfsspec_write },		/* write */
	{ &vop_lease_desc, spec_lease_check },		/* lease */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, spec_ioctl },		/* ioctl */
	{ &vop_poll_desc, spec_poll },			/* poll */
	{ &vop_revoke_desc, spec_revoke },		/* revoke */
	{ &vop_mmap_desc, spec_mmap },			/* mmap */
	{ &vop_fsync_desc, nfs_fsync },			/* fsync */
	{ &vop_seek_desc, spec_seek },			/* seek */
	{ &vop_remove_desc, spec_remove },		/* remove */
	{ &vop_link_desc, spec_link },			/* link */
	{ &vop_rename_desc, spec_rename },		/* rename */
	{ &vop_mkdir_desc, spec_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, spec_rmdir },		/* rmdir */
	{ &vop_symlink_desc, spec_symlink },		/* symlink */
	{ &vop_readdir_desc, spec_readdir },		/* readdir */
	{ &vop_readlink_desc, spec_readlink },		/* readlink */
	{ &vop_abortop_desc, spec_abortop },		/* abortop */
	{ &vop_inactive_desc, nfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, nfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, nfs_lock },			/* lock */
	{ &vop_unlock_desc, nfs_unlock },		/* unlock */
	{ &vop_bmap_desc, spec_bmap },			/* bmap */
	{ &vop_strategy_desc, spec_strategy },		/* strategy */
	{ &vop_print_desc, nfs_print },			/* print */
	{ &vop_islocked_desc, nfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* advlock */
	{ &vop_blkatoff_desc, spec_blkatoff },		/* blkatoff */
	{ &vop_valloc_desc, spec_valloc },		/* valloc */
	{ &vop_reallocblks_desc, spec_reallocblks },	/* reallocblks */
	{ &vop_vfree_desc, spec_vfree },		/* vfree */
	{ &vop_truncate_desc, spec_truncate },		/* truncate */
	{ &vop_update_desc, nfs_update },		/* update */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ (struct vnodeop_desc*)NULL, (int(*) __P((void *)))NULL }
};
struct vnodeopv_desc spec_nfsv2nodeop_opv_desc =
	{ &spec_nfsv2nodeop_p, spec_nfsv2nodeop_entries };

int (**fifo_nfsv2nodeop_p) __P((void *));
struct vnodeopv_entry_desc fifo_nfsv2nodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, fifo_lookup },		/* lookup */
	{ &vop_create_desc, fifo_create },		/* create */
	{ &vop_mknod_desc, fifo_mknod },		/* mknod */
	{ &vop_open_desc, fifo_open },			/* open */
	{ &vop_close_desc, nfsfifo_close },		/* close */
	{ &vop_access_desc, nfsspec_access },		/* access */
	{ &vop_getattr_desc, nfs_getattr },		/* getattr */
	{ &vop_setattr_desc, nfs_setattr },		/* setattr */
	{ &vop_read_desc, nfsfifo_read },		/* read */
	{ &vop_write_desc, nfsfifo_write },		/* write */
	{ &vop_lease_desc, fifo_lease_check },		/* lease */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, fifo_ioctl },		/* ioctl */
	{ &vop_poll_desc, fifo_poll },			/* poll */
	{ &vop_revoke_desc, fifo_revoke },		/* revoke */
	{ &vop_mmap_desc, fifo_mmap },			/* mmap */
	{ &vop_fsync_desc, nfs_fsync },			/* fsync */
	{ &vop_seek_desc, fifo_seek },			/* seek */
	{ &vop_remove_desc, fifo_remove },		/* remove */
	{ &vop_link_desc, fifo_link },			/* link */
	{ &vop_rename_desc, fifo_rename },		/* rename */
	{ &vop_mkdir_desc, fifo_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, fifo_rmdir },		/* rmdir */
	{ &vop_symlink_desc, fifo_symlink },		/* symlink */
	{ &vop_readdir_desc, fifo_readdir },		/* readdir */
	{ &vop_readlink_desc, fifo_readlink },		/* readlink */
	{ &vop_abortop_desc, fifo_abortop },		/* abortop */
	{ &vop_inactive_desc, nfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, nfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, nfs_lock },			/* lock */
	{ &vop_unlock_desc, nfs_unlock },		/* unlock */
	{ &vop_bmap_desc, fifo_bmap },			/* bmap */
	{ &vop_strategy_desc, genfs_badop },		/* strategy */
	{ &vop_print_desc, nfs_print },			/* print */
	{ &vop_islocked_desc, nfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, fifo_pathconf },		/* pathconf */
	{ &vop_advlock_desc, fifo_advlock },		/* advlock */
	{ &vop_blkatoff_desc, fifo_blkatoff },		/* blkatoff */
	{ &vop_valloc_desc, fifo_valloc },		/* valloc */
	{ &vop_reallocblks_desc, fifo_reallocblks },	/* reallocblks */
	{ &vop_vfree_desc, fifo_vfree },		/* vfree */
	{ &vop_truncate_desc, fifo_truncate },		/* truncate */
	{ &vop_update_desc, nfs_update },		/* update */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ (struct vnodeop_desc*)NULL, (int(*) __P((void *)))NULL }
};
struct vnodeopv_desc fifo_nfsv2nodeop_opv_desc =
	{ &fifo_nfsv2nodeop_p, fifo_nfsv2nodeop_entries };

/*
 * Global variables
 */
extern u_int32_t nfs_true, nfs_false;
extern u_int32_t nfs_xdrneg1;
extern struct nfsstats nfsstats;
extern nfstype nfsv3_type[9];
struct proc *nfs_iodwant[NFS_MAXASYNCDAEMON];
struct nfsmount *nfs_iodmount[NFS_MAXASYNCDAEMON];
int nfs_numasync = 0;
#define	DIRHDSIZ	(sizeof (struct dirent) - (MAXNAMLEN + 1))

/*
 * nfs null call from vfs.
 */
int
nfs_null(vp, cred, procp)
	struct vnode *vp;
	struct ucred *cred;
	struct proc *procp;
{
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	
	nfsm_reqhead(vp, NFSPROC_NULL, 0);
	nfsm_request(vp, NFSPROC_NULL, procp, cred);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs access vnode op.
 * For nfs version 2, just return ok. File accesses may fail later.
 * For nfs version 3, use the access rpc to check accessibility. If file modes
 * are changed on the server, accesses might still fail later.
 */
int
nfs_access(v)
	void *v;
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, attrflag, cachevalid;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	u_int32_t mode, rmode;
	int v3 = NFS_ISV3(vp);
	struct nfsnode *np = VTONFS(vp);

	cachevalid = (np->n_accstamp != -1 &&
	    (time.tv_sec - np->n_accstamp) < NFS_ATTRTIMEO(np) &&
	    np->n_accuid == ap->a_cred->cr_uid);

	/*
	 * Check access cache first. If this request has been made for this
	 * uid shortly before, use the cached result.
	 */
	if (cachevalid && ((np->n_accmode & ap->a_mode) == ap->a_mode))
		return np->n_accerror;

	/*
	 * For nfs v3, do an access rpc, otherwise you are stuck emulating
	 * ufs_access() locally using the vattr. This may not be correct,
	 * since the server may apply other access criteria such as
	 * client uid-->server uid mapping that we do not know about, but
	 * this is better than just returning anything that is lying about
	 * in the cache.
	 */
	if (v3) {
		nfsstats.rpccnt[NFSPROC_ACCESS]++;
		nfsm_reqhead(vp, NFSPROC_ACCESS, NFSX_FH(v3) + NFSX_UNSIGNED);
		nfsm_fhtom(vp, v3);
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		if (ap->a_mode & VREAD)
			mode = NFSV3ACCESS_READ;
		else
			mode = 0;
		if (vp->v_type != VDIR) {
			if (ap->a_mode & VWRITE)
				mode |= (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND);
			if (ap->a_mode & VEXEC)
				mode |= NFSV3ACCESS_EXECUTE;
		} else {
			if (ap->a_mode & VWRITE)
				mode |= (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND |
					 NFSV3ACCESS_DELETE);
			if (ap->a_mode & VEXEC)
				mode |= NFSV3ACCESS_LOOKUP;
		}
		*tl = txdr_unsigned(mode);
		nfsm_request(vp, NFSPROC_ACCESS, ap->a_p, ap->a_cred);
		nfsm_postop_attr(vp, attrflag);
		if (!error) {
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			rmode = fxdr_unsigned(u_int32_t, *tl);
			/*
			 * The NFS V3 spec does not clarify whether or not
			 * the returned access bits can be a superset of
			 * the ones requested, so...
			 */
			if ((rmode & mode) != mode)
				error = EACCES;
		}
		nfsm_reqdone;
	} else
		return (nfsspec_access(ap));
	/*
	 * Disallow write attempts on filesystems mounted read-only;
	 * unless the file is a socket, fifo, or a block or character
	 * device resident on the filesystem.
	 */
	if (!error && (ap->a_mode & VWRITE) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
			error = EROFS;
		default:
			break;
		}
	}

	if (!error || error == EACCES) {
		/*
		 * If we got the same result as for a previous,
		 * different request, OR it in. Don't update
		 * the timestamp in that case.
		 */
		if (cachevalid && error == np->n_accerror)
			np->n_accmode |= ap->a_mode;
		else {
			np->n_accstamp = time.tv_sec;
			np->n_accuid = ap->a_cred->cr_uid;
			np->n_accmode = ap->a_mode;
			np->n_accerror = error;
		}
	}

	return (error);
}

/*
 * nfs open vnode op
 * Check to see if the type is ok
 * and that deletion is not in progress.
 * For paged in text files, you will need to flush the page cache
 * if consistency is lost.
 */
/* ARGSUSED */
int
nfs_open(v)
	void *v;
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct vattr vattr;
	int error;

	if (vp->v_type != VREG && vp->v_type != VDIR && vp->v_type != VLNK) {
#ifdef DIAGNOSTIC
		printf("open eacces vtyp=%d\n",vp->v_type);
#endif
		return (EACCES);
	}
	/*
	 * Get a valid lease. If cached data is stale, flush it.
	 */
	if (nmp->nm_flag & NFSMNT_NQNFS) {
		if (NQNFS_CKINVALID(vp, np, ND_READ)) {
		    do {
			error = nqnfs_getlease(vp, ND_READ, ap->a_cred,
			    ap->a_p);
		    } while (error == NQNFS_EXPIRED);
		    if (error)
			return (error);
		    if (np->n_lrev != np->n_brev ||
			(np->n_flag & NQNFSNONCACHE)) {
			if ((error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred,
				ap->a_p, 1)) == EINTR)
				return (error);
			(void) uvm_vnp_uncache(vp);
			np->n_brev = np->n_lrev;
		    }
		}
	} else {
		if (np->n_flag & NMODIFIED) {
			if ((error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred,
				ap->a_p, 1)) == EINTR)
				return (error);
			(void) uvm_vnp_uncache(vp);
			np->n_attrstamp = 0;
			if (vp->v_type == VDIR) {
				nfs_invaldircache(vp, 0);
				np->n_direofoffset = 0;
			}
			error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
			if (error)
				return (error);
			np->n_mtime = vattr.va_mtime.tv_sec;
		} else {
			error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
			if (error)
				return (error);
			if (np->n_mtime != vattr.va_mtime.tv_sec) {
				if (vp->v_type == VDIR) {
					nfs_invaldircache(vp, 0);
					np->n_direofoffset = 0;
				}
				if ((error = nfs_vinvalbuf(vp, V_SAVE,
					ap->a_cred, ap->a_p, 1)) == EINTR)
					return (error);
				(void) uvm_vnp_uncache(vp);
				np->n_mtime = vattr.va_mtime.tv_sec;
			}
		}
	}
	if ((nmp->nm_flag & NFSMNT_NQNFS) == 0)
		np->n_attrstamp = 0; /* For Open/Close consistency */
	return (0);
}

/*
 * nfs close vnode op
 * What an NFS client should do upon close after writing is a debatable issue.
 * Most NFS clients push delayed writes to the server upon close, basically for
 * two reasons:
 * 1 - So that any write errors may be reported back to the client process
 *     doing the close system call. By far the two most likely errors are
 *     NFSERR_NOSPC and NFSERR_DQUOT to indicate space allocation failure.
 * 2 - To put a worst case upper bound on cache inconsistency between
 *     multiple clients for the file.
 * There is also a consistency problem for Version 2 of the protocol w.r.t.
 * not being able to tell if other clients are writing a file concurrently,
 * since there is no way of knowing if the changed modify time in the reply
 * is only due to the write for this client.
 * (NFS Version 3 provides weak cache consistency data in the reply that
 *  should be sufficient to detect and handle this case.)
 *
 * The current code does the following:
 * for NFS Version 2 - play it safe and flush/invalidate all dirty buffers
 * for NFS Version 3 - flush dirty buffers to the server but don't invalidate
 *                     or commit them (this satisfies 1 and 2 except for the
 *                     case where the server crashes after this close but
 *                     before the commit RPC, which is felt to be "good
 *                     enough". Changing the last argument to nfs_flush() to
 *                     a 1 would force a commit operation, if it is felt a
 *                     commit is necessary now.
 * for NQNFS         - do nothing now, since 2 is dealt with via leases and
 *                     1 should be dealt with via an fsync() system call for
 *                     cases where write errors are important.
 */
/* ARGSUSED */
int
nfs_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	int error = 0;

	if (vp->v_type == VREG) {
	    if ((VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NQNFS) == 0 &&
		(np->n_flag & NMODIFIED)) {
		if (NFS_ISV3(vp)) {
		    error = nfs_flush(vp, ap->a_cred, MNT_WAIT, ap->a_p, 0);
		    np->n_flag &= ~NMODIFIED;
		} else
		    error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 1);
		np->n_attrstamp = 0;
	    }
	    if (np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		error = np->n_error;
	    }
	}
	return (error);
}

/*
 * nfs getattr call from vfs.
 */
int
nfs_getattr(v)
	void *v;
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	caddr_t cp;
	u_int32_t *tl;
	int32_t t1, t2;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(vp);
	
	/*
	 * Update local times for special files.
	 */
	if (np->n_flag & (NACC | NUPD))
		np->n_flag |= NCHG;
	/*
	 * First look in the cache.
	 */
	if (nfs_getattrcache(vp, ap->a_vap) == 0)
		return (0);
	nfsstats.rpccnt[NFSPROC_GETATTR]++;
	nfsm_reqhead(vp, NFSPROC_GETATTR, NFSX_FH(v3));
	nfsm_fhtom(vp, v3);
	nfsm_request(vp, NFSPROC_GETATTR, ap->a_p, ap->a_cred);
	if (!error) {
		nfsm_loadattr(vp, ap->a_vap);
		if (vp->v_type == VDIR &&
		    ap->a_vap->va_blocksize < NFS_DIRFRAGSIZ)
			ap->a_vap->va_blocksize = NFS_DIRFRAGSIZ;
	}
	nfsm_reqdone;
	return (error);
}

/*
 * nfs setattr call.
 */
int
nfs_setattr(v)
	void *v;
{
	struct vop_setattr_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr *vap = ap->a_vap;
	int error = 0;
	u_quad_t tsize = 0;

	/*
	 * Setting of flags is not supported.
	 */
	if (vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

	/*
	 * Disallow write attempts if the filesystem is mounted read-only.
	 */
  	if ((vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		case VDIR:
 			return (EISDIR);
 		case VCHR:
 		case VBLK:
 		case VSOCK:
 		case VFIFO:
			if (vap->va_mtime.tv_sec == VNOVAL &&
			    vap->va_atime.tv_sec == VNOVAL &&
			    vap->va_mode == (mode_t)VNOVAL &&
			    vap->va_uid == (uid_t)VNOVAL &&
			    vap->va_gid == (gid_t)VNOVAL)
				return (0);
 			vap->va_size = VNOVAL;
 			break;
 		default:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
 			uvm_vnp_setsize(vp, vap->va_size);
 			if (vap->va_size == 0)
 				error = nfs_vinvalbuf(vp, 0,
 				     ap->a_cred, ap->a_p, 1);
			else
				error = nfs_vinvalbuf(vp, V_SAVE,
				     ap->a_cred, ap->a_p, 1);
			if (error) {
				uvm_vnp_setsize(vp, np->n_size);
				return (error);
			}
 			tsize = np->n_size;
 			np->n_size = np->n_vattr->va_size = vap->va_size;
  		}
  	} else if ((vap->va_mtime.tv_sec != VNOVAL ||
		vap->va_atime.tv_sec != VNOVAL) &&
		vp->v_type == VREG &&
  		(error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred,
		 ap->a_p, 1)) == EINTR)
		return (error);
	error = nfs_setattrrpc(vp, vap, ap->a_cred, ap->a_p);
	if (error && vap->va_size != VNOVAL) {
		np->n_size = np->n_vattr->va_size = tsize;
		uvm_vnp_setsize(vp, np->n_size);
	}
	return (error);
}

/*
 * Do an nfs setattr rpc.
 */
int
nfs_setattrrpc(vp, vap, cred, procp)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct proc *procp;
{
	struct nfsv2_sattr *sp;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	u_int32_t *tl;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(vp);

	nfsstats.rpccnt[NFSPROC_SETATTR]++;
	nfsm_reqhead(vp, NFSPROC_SETATTR, NFSX_FH(v3) + NFSX_SATTR(v3));
	nfsm_fhtom(vp, v3);
	if (v3) {
		nfsm_v3attrbuild(vap, TRUE);
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = nfs_false;
	} else {
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		if (vap->va_mode == (mode_t)VNOVAL)
			sp->sa_mode = nfs_xdrneg1;
		else
			sp->sa_mode = vtonfsv2_mode(vp->v_type, vap->va_mode);
		if (vap->va_uid == (uid_t)VNOVAL)
			sp->sa_uid = nfs_xdrneg1;
		else
			sp->sa_uid = txdr_unsigned(vap->va_uid);
		if (vap->va_gid == (gid_t)VNOVAL)
			sp->sa_gid = nfs_xdrneg1;
		else
			sp->sa_gid = txdr_unsigned(vap->va_gid);
		sp->sa_size = txdr_unsigned(vap->va_size);
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(vp, NFSPROC_SETATTR, procp, cred);
	if (v3) {
		nfsm_wcc_data(vp, wccflag);
	} else
		nfsm_loadattr(vp, (struct vattr *)0);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs lookup call, one step at a time...
 * First look in cache
 * If not found, unlock the directory nfsnode and do the rpc
 */
int
nfs_lookup(v)
	void *v;
{
	struct vop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	int flags;
	struct vnode *newvp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	struct nfsmount *nmp;
	caddr_t bpos, dpos, cp2;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	long len;
	nfsfh_t *fhp;
	struct nfsnode *np;
	int lockparent, wantparent, error = 0, attrflag, fhsize;
	int v3 = NFS_ISV3(dvp);
	cnp->cn_flags &= ~PDIRUNLOCK;
	flags = cnp->cn_flags;

	*vpp = NULLVP;
	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);
	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT|WANTPARENT);
	nmp = VFSTONFS(dvp->v_mount);
	np = VTONFS(dvp);

	/*
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 * If the directory/name pair is found in the name cache,
	 * we have to ensure the directory has not changed from
	 * the time the cache entry has been created. If it has,
	 * the cache entry has to be ignored 
	 */
	if ((error = cache_lookup(dvp, vpp, cnp)) >= 0) {
		struct vattr vattr;
		int err2;

		if (error && error != ENOENT) {
			*vpp = NULLVP;
			return (error);
		}

		err2 = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, cnp->cn_proc);
		if (err2) {
			*vpp = NULLVP;
			return (err2);
		}

		if (error == ENOENT) {
			if (!VOP_GETATTR(dvp, &vattr, cnp->cn_cred,
			    cnp->cn_proc) && vattr.va_mtime.tv_sec ==
			    VTONFS(dvp)->n_nctime)
				return (ENOENT);
			cache_purge(dvp);
			np->n_nctime = 0;
			goto dorpc;
		} else if (error > 0) {
			*vpp = NULLVP;
			return error;
		}

		newvp = *vpp;
		if (!VOP_GETATTR(newvp, &vattr, cnp->cn_cred, cnp->cn_proc)
			&& vattr.va_ctime.tv_sec == VTONFS(newvp)->n_ctime)
		{
			nfsstats.lookupcache_hits++;
			if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
				cnp->cn_flags |= SAVENAME;
			return (0);
		}
		/* XXX cache_lookup() returns the vnode locked; if nfs
		 * would have real vnode locking, we should call VOP_UNLOCK()
		 * here; as it has no real locking, don't bother to do
		 * anything */
		/* VOP_UNLOCK(newvp, 0); */
		cache_purge(newvp);
		vrele(newvp);
		*vpp = NULLVP;
	}
dorpc:
	error = 0;
	newvp = NULLVP;
	nfsstats.lookupcache_misses++;
	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	len = cnp->cn_namelen;
	nfsm_reqhead(dvp, NFSPROC_LOOKUP,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_LOOKUP, cnp->cn_proc, cnp->cn_cred);
	if (error) {
		nfsm_postop_attr(dvp, attrflag);
		m_freem(mrep);
		goto nfsmout;
	}
	nfsm_getfh(fhp, fhsize, v3);

	/*
	 * Handle RENAME case...
	 */
	if (cnp->cn_nameiop == RENAME && wantparent && (flags & ISLASTCN)) {
		if (NFS_CMPFH(np, fhp, fhsize)) {
			m_freem(mrep);
			return (EISDIR);
		}
		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np);
		if (error) {
			m_freem(mrep);
			return (error);
		}
		newvp = NFSTOV(np);
		if (v3) {
			nfsm_postop_attr(newvp, attrflag);
			nfsm_postop_attr(dvp, attrflag);
		} else
			nfsm_loadattr(newvp, (struct vattr *)0);
		*vpp = newvp;
		m_freem(mrep);
		cnp->cn_flags |= SAVENAME;
		if (!lockparent || !(flags & ISLASTCN))
			cnp->cn_flags |= PDIRUNLOCK;
		return (0);
	}

	if (NFS_CMPFH(np, fhp, fhsize)) {
		VREF(dvp);
		newvp = dvp;
	} else {
		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np);
		if (error) {
			m_freem(mrep);
			return (error);
		}
		newvp = NFSTOV(np);
	}
	if (v3) {
		nfsm_postop_attr(newvp, attrflag);
		nfsm_postop_attr(dvp, attrflag);
	} else
		nfsm_loadattr(newvp, (struct vattr *)0);
	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
		cnp->cn_flags |= SAVENAME;
	if ((cnp->cn_flags & MAKEENTRY) &&
	    (cnp->cn_nameiop != DELETE || !(flags & ISLASTCN))) {
		np->n_ctime = np->n_vattr->va_ctime.tv_sec;
		cache_enter(dvp, newvp, cnp);
	}
	*vpp = newvp;
	nfsm_reqdone;
	if (error) {
		if (error == ENOENT && (cnp->cn_flags & MAKEENTRY) &&
		    cnp->cn_nameiop != CREATE) {
			if (VTONFS(dvp)->n_nctime == 0)
				VTONFS(dvp)->n_nctime =
				    VTONFS(dvp)->n_vattr->va_mtime.tv_sec;
			cache_enter(dvp, NULL, cnp);
		}
		if (newvp != NULLVP)
			vrele(newvp);
		if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
		    (flags & ISLASTCN) && error == ENOENT) {
			if (dvp->v_mount->mnt_flag & MNT_RDONLY)
				error = EROFS;
			else
				error = EJUSTRETURN;
		}
		if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
			cnp->cn_flags |= SAVENAME;
	} else {
		if (!lockparent || !(flags & ISLASTCN))
			cnp->cn_flags |= PDIRUNLOCK;
	}
	return (error);
}

/*
 * nfs read call.
 * Just call nfs_bioread() to do the work.
 */
int
nfs_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_type != VREG)
		return (EPERM);
	return (nfs_bioread(vp, ap->a_uio, ap->a_ioflag, ap->a_cred, 0));
}

/*
 * nfs readlink call
 */
int
nfs_readlink(v)
	void *v;
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_type != VLNK)
		return (EPERM);
	return (nfs_bioread(vp, ap->a_uio, 0, ap->a_cred, 0));
}

/*
 * Do a readlink rpc.
 * Called by nfs_doio() from below the buffer cache.
 */
int
nfs_readlinkrpc(vp, uiop, cred)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, len, attrflag;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(vp);

	nfsstats.rpccnt[NFSPROC_READLINK]++;
	nfsm_reqhead(vp, NFSPROC_READLINK, NFSX_FH(v3));
	nfsm_fhtom(vp, v3);
	nfsm_request(vp, NFSPROC_READLINK, uiop->uio_procp, cred);
	if (v3)
		nfsm_postop_attr(vp, attrflag);
	if (!error) {
		nfsm_strsiz(len, NFS_MAXPATHLEN);
		nfsm_mtouio(uiop, len);
	}
	nfsm_reqdone;
	return (error);
}

/*
 * nfs read rpc call
 * Ditto above
 */
int
nfs_readrpc(vp, uiop, cred)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsmount *nmp;
	int error = 0, len, retlen, tsiz, eof, attrflag;
	int v3 = NFS_ISV3(vp);

#ifndef nolint
	eof = 0;
#endif
	nmp = VFSTONFS(vp->v_mount);
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > nmp->nm_maxfilesize)
		return (EFBIG);
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_READ]++;
		len = (tsiz > nmp->nm_rsize) ? nmp->nm_rsize : tsiz;
		nfsm_reqhead(vp, NFSPROC_READ, NFSX_FH(v3) + NFSX_UNSIGNED * 3);
		nfsm_fhtom(vp, v3);
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED * 3);
		if (v3) {
			txdr_hyper(uiop->uio_offset, tl);
			*(tl + 2) = txdr_unsigned(len);
		} else {
			*tl++ = txdr_unsigned(uiop->uio_offset);
			*tl++ = txdr_unsigned(len);
			*tl = 0;
		}
		nfsm_request(vp, NFSPROC_READ, uiop->uio_procp, cred);
		if (v3) {
			nfsm_postop_attr(vp, attrflag);
			if (error) {
				m_freem(mrep);
				goto nfsmout;
			}
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			eof = fxdr_unsigned(int, *(tl + 1));
		} else
			nfsm_loadattr(vp, (struct vattr *)0);
		nfsm_strsiz(retlen, nmp->nm_rsize);
		nfsm_mtouio(uiop, retlen);
		m_freem(mrep);
		tsiz -= retlen;
		if (v3) {
			if (eof || retlen == 0)
				tsiz = 0;
		} else if (retlen < len)
			tsiz = 0;
	}
nfsmout:
	return (error);
}

/*
 * nfs write call
 */
int
nfs_writerpc(vp, uiop, cred, iomode, must_commit)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
	int *iomode, *must_commit;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2, backup;
	caddr_t bpos, dpos, cp2;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, len, tsiz, wccflag = NFSV3_WCCRATTR, rlen, commit;
	int v3 = NFS_ISV3(vp), committed = NFSV3WRITE_FILESYNC;

#ifndef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1)
		panic("nfs: writerpc iovcnt > 1");
#endif
	*must_commit = 0;
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > nmp->nm_maxfilesize)
		return (EFBIG);
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_WRITE]++;
		len = (tsiz > nmp->nm_wsize) ? nmp->nm_wsize : tsiz;
		nfsm_reqhead(vp, NFSPROC_WRITE,
			NFSX_FH(v3) + 5 * NFSX_UNSIGNED + nfsm_rndup(len));
		nfsm_fhtom(vp, v3);
		if (v3) {
			nfsm_build(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			txdr_hyper(uiop->uio_offset, tl);
			tl += 2;
			*tl++ = txdr_unsigned(len);
			*tl++ = txdr_unsigned(*iomode);
			*tl = txdr_unsigned(len);
		} else {
			u_int32_t x;

			nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
			/* Set both "begin" and "current" to non-garbage. */
			x = txdr_unsigned((u_int32_t)uiop->uio_offset);
			*tl++ = x;      /* "begin offset" */
			*tl++ = x;      /* "current offset" */
			x = txdr_unsigned(len);
			*tl++ = x;      /* total to this offset */
			*tl = x;        /* size of this write */

		}
		nfsm_uiotom(uiop, len);
		nfsm_request(vp, NFSPROC_WRITE, uiop->uio_procp, cred);
		if (v3) {
			wccflag = NFSV3_WCCCHK;
			nfsm_wcc_data(vp, wccflag);
			if (!error) {
				nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED
					+ NFSX_V3WRITEVERF);
				rlen = fxdr_unsigned(int, *tl++);
				if (rlen == 0) {
					error = NFSERR_IO;
					m_freem(mrep);
					break;
				} else if (rlen < len) {
					backup = len - rlen;
					(caddr_t)uiop->uio_iov->iov_base -=
					    backup;
					uiop->uio_iov->iov_len += backup;
					uiop->uio_offset -= backup;
					uiop->uio_resid += backup;
					len = rlen;
				}
				commit = fxdr_unsigned(int, *tl++);

				/*
				 * Return the lowest committment level
				 * obtained by any of the RPCs.
				 */
				if (committed == NFSV3WRITE_FILESYNC)
					committed = commit;
				else if (committed == NFSV3WRITE_DATASYNC &&
					commit == NFSV3WRITE_UNSTABLE)
					committed = commit;
				if ((nmp->nm_iflag & NFSMNT_HASWRITEVERF) == 0){
				    memcpy((caddr_t)nmp->nm_verf, (caddr_t)tl,
					NFSX_V3WRITEVERF);
				    nmp->nm_iflag |= NFSMNT_HASWRITEVERF;
				} else if (memcmp((caddr_t)tl,
				    (caddr_t)nmp->nm_verf, NFSX_V3WRITEVERF)) {
				    *must_commit = 1;
				    memcpy((caddr_t)nmp->nm_verf, (caddr_t)tl,
					NFSX_V3WRITEVERF);
				}
			}
		} else
		    nfsm_loadattr(vp, (struct vattr *)0);
		if (wccflag)
		    VTONFS(vp)->n_mtime = VTONFS(vp)->n_vattr->va_mtime.tv_sec;
		m_freem(mrep);
		if (error)
			break;
		tsiz -= len;
	}
nfsmout:
	*iomode = committed;
	if (error)
		uiop->uio_resid = tsiz;
	return (error);
}

/*
 * nfs mknod rpc
 * For NFS v2 this is a kludge. Use a create rpc but with the IFMT bits of the
 * mode set to specify the file type and the size field for rdev.
 */
int
nfs_mknodrpc(dvp, vpp, cnp, vap)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
	struct vattr *vap;
{
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	struct vnode *newvp = (struct vnode *)0;
	struct nfsnode *np;
	char *cp2;
	caddr_t bpos, dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR, gotvp = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	u_int32_t rdev;
	int v3 = NFS_ISV3(dvp);

	if (vap->va_type == VCHR || vap->va_type == VBLK)
		rdev = txdr_unsigned(vap->va_rdev);
	else if (vap->va_type == VFIFO || vap->va_type == VSOCK)
		rdev = nfs_xdrneg1;
	else {
		VOP_ABORTOP(dvp, cnp);
		vput(dvp);
		return (EOPNOTSUPP);
	}
	nfsstats.rpccnt[NFSPROC_MKNOD]++;
	nfsm_reqhead(dvp, NFSPROC_MKNOD, NFSX_FH(v3) + 4 * NFSX_UNSIGNED +
		+ nfsm_rndup(cnp->cn_namelen) + NFSX_SATTR(v3));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	if (v3) {
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl++ = vtonfsv3_type(vap->va_type);
		nfsm_v3attrbuild(vap, FALSE);
		if (vap->va_type == VCHR || vap->va_type == VBLK) {
			nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(major(vap->va_rdev));
			*tl = txdr_unsigned(minor(vap->va_rdev));
		}
	} else {
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = rdev;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dvp, NFSPROC_MKNOD, cnp->cn_proc, cnp->cn_cred);
	if (!error) {
		nfsm_mtofh(dvp, newvp, v3, gotvp);
		if (!gotvp) {
			if (newvp) {
				vrele(newvp);
				newvp = (struct vnode *)0;
			}
			error = nfs_lookitup(dvp, cnp->cn_nameptr,
			    cnp->cn_namelen, cnp->cn_cred, cnp->cn_proc, &np);
			if (!error)
				newvp = NFSTOV(np);
		}
	}
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	nfsm_reqdone;
	if (error) {
		if (newvp)
			vrele(newvp);
	} else {
		if (cnp->cn_flags & MAKEENTRY)
			cache_enter(dvp, newvp, cnp);
		*vpp = newvp;
	}
	FREE(cnp->cn_pnbuf, M_NAMEI);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	vrele(dvp);
	return (error);
}

/*
 * nfs mknod vop
 * just call nfs_mknodrpc() to do the work.
 */
/* ARGSUSED */
int
nfs_mknod(v)
	void *v;
{
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode *newvp;
	int error;

	error = nfs_mknodrpc(ap->a_dvp, &newvp, ap->a_cnp, ap->a_vap);
	if (!error)
		vrele(newvp);
	return (error);
}

static u_long create_verf;
/*
 * nfs file create call
 */
int
nfs_create(v)
	void *v;
{
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	struct nfsnode *np = (struct nfsnode *)0;
	struct vnode *newvp = (struct vnode *)0;
	caddr_t bpos, dpos, cp2;
	int error, wccflag = NFSV3_WCCRATTR, gotvp = 0, fmode = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(dvp);

	/*
	 * Oops, not for me..
	 */
	if (vap->va_type == VSOCK)
		return (nfs_mknodrpc(dvp, ap->a_vpp, cnp, vap));

#ifdef VA_EXCLUSIVE
	if (vap->va_vaflags & VA_EXCLUSIVE)
		fmode |= O_EXCL;
#endif
again:
	error = 0;
	nfsstats.rpccnt[NFSPROC_CREATE]++;
	nfsm_reqhead(dvp, NFSPROC_CREATE, NFSX_FH(v3) + 2 * NFSX_UNSIGNED +
		nfsm_rndup(cnp->cn_namelen) + NFSX_SATTR(v3));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	if (v3) {
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		if (fmode & O_EXCL) {
			*tl = txdr_unsigned(NFSV3CREATE_EXCLUSIVE);
			nfsm_build(tl, u_int32_t *, NFSX_V3CREATEVERF);
			if (in_ifaddr.tqh_first)
				*tl++ = in_ifaddr.tqh_first->ia_addr.sin_addr.s_addr;
			else
				*tl++ = create_verf;
			*tl = ++create_verf;
		} else {
			*tl = txdr_unsigned(NFSV3CREATE_UNCHECKED);
			nfsm_v3attrbuild(vap, FALSE);
		}
	} else {
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = 0;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dvp, NFSPROC_CREATE, cnp->cn_proc, cnp->cn_cred);
	if (!error) {
		nfsm_mtofh(dvp, newvp, v3, gotvp);
		if (!gotvp) {
			if (newvp) {
				vrele(newvp);
				newvp = (struct vnode *)0;
			}
			error = nfs_lookitup(dvp, cnp->cn_nameptr,
			    cnp->cn_namelen, cnp->cn_cred, cnp->cn_proc, &np);
			if (!error)
				newvp = NFSTOV(np);
		}
	}
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	nfsm_reqdone;
	if (error) {
		if (v3 && (fmode & O_EXCL) && error == NFSERR_NOTSUPP) {
			fmode &= ~O_EXCL;
			goto again;
		}
		if (newvp)
			vrele(newvp);
	} else if (v3 && (fmode & O_EXCL))
		error = nfs_setattrrpc(newvp, vap, cnp->cn_cred, cnp->cn_proc);
	if (!error) {
		if (cnp->cn_flags & MAKEENTRY)
			cache_enter(dvp, newvp, cnp);
		*ap->a_vpp = newvp;
	}
	FREE(cnp->cn_pnbuf, M_NAMEI);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	vrele(dvp);
	return (error);
}

/*
 * nfs file remove call
 * To try and make nfs semantics closer to ufs semantics, a file that has
 * other processes using the vnode is renamed instead of removed and then
 * removed later on the last close.
 * - If v_usecount > 1
 *	  If a rename is not already in the works
 *	     call nfs_sillyrename() to set it up
 *     else
 *	  do the remove rpc
 */
int
nfs_remove(v)
	void *v;
{
	struct vop_remove_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode * a_vp;
		struct componentname * a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nfsnode *np = VTONFS(vp);
	int error = 0;
	struct vattr vattr;

#ifndef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("nfs_remove: no name");
	if (vp->v_usecount < 1)
		panic("nfs_remove: bad v_usecount");
#endif
	if (vp->v_type == VDIR)
		error = EPERM;
	else if (vp->v_usecount == 1 || (np->n_sillyrename &&
	    VOP_GETATTR(vp, &vattr, cnp->cn_cred, cnp->cn_proc) == 0 &&
	    vattr.va_nlink > 1)) {
		/*
		 * Purge the name cache so that the chance of a lookup for
		 * the name succeeding while the remove is in progress is
		 * minimized. Without node locking it can still happen, such
		 * that an I/O op returns ESTALE, but since you get this if
		 * another host removes the file..
		 */
		cache_purge(vp);
		/*
		 * throw away biocache buffers, mainly to avoid
		 * unnecessary delayed writes later.
		 */
		error = nfs_vinvalbuf(vp, 0, cnp->cn_cred, cnp->cn_proc, 1);
		/* Do the rpc */
		if (error != EINTR)
			error = nfs_removerpc(dvp, cnp->cn_nameptr,
				cnp->cn_namelen, cnp->cn_cred, cnp->cn_proc);
		/*
		 * Kludge City: If the first reply to the remove rpc is lost..
		 *   the reply to the retransmitted request will be ENOENT
		 *   since the file was in fact removed
		 *   Therefore, we cheat and return success.
		 */
		if (error == ENOENT)
			error = 0;
	} else if (!np->n_sillyrename)
		error = nfs_sillyrename(dvp, vp, cnp);
	FREE(cnp->cn_pnbuf, M_NAMEI);
	np->n_attrstamp = 0;
	vrele(dvp);
	vrele(vp);
	return (error);
}

/*
 * nfs file remove rpc called from nfs_inactive
 */
int
nfs_removeit(sp)
	struct sillyrename *sp;
{

	return (nfs_removerpc(sp->s_dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		(struct proc *)0));
}

/*
 * Nfs remove rpc, called from nfs_remove() and nfs_removeit().
 */
int
nfs_removerpc(dvp, name, namelen, cred, proc)
	struct vnode *dvp;
	const char *name;
	int namelen;
	struct ucred *cred;
	struct proc *proc;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_REMOVE]++;
	nfsm_reqhead(dvp, NFSPROC_REMOVE,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(namelen));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(name, namelen, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_REMOVE, proc, cred);
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	nfsm_reqdone;
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	return (error);
}

/*
 * nfs file rename call
 */
int
nfs_rename(v)
	void *v;
{
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	int error;

#ifndef DIAGNOSTIC
	if ((tcnp->cn_flags & HASBUF) == 0 ||
	    (fcnp->cn_flags & HASBUF) == 0)
		panic("nfs_rename: no name");
#endif
	/* Check for cross-device rename */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
	}

	/*
	 * If the tvp exists and is in use, sillyrename it before doing the
	 * rename of the new file over it.
	 */
	if (tvp && tvp->v_usecount > 1 && !VTONFS(tvp)->n_sillyrename &&
		tvp->v_type != VDIR && !nfs_sillyrename(tdvp, tvp, tcnp)) {
		vrele(tvp);
		tvp = NULL;
	}

	error = nfs_renamerpc(fdvp, fcnp->cn_nameptr, fcnp->cn_namelen,
		tdvp, tcnp->cn_nameptr, tcnp->cn_namelen, tcnp->cn_cred,
		tcnp->cn_proc);

	if (fvp->v_type == VDIR) {
		if (tvp != NULL && tvp->v_type == VDIR)
			cache_purge(tdvp);
		cache_purge(fdvp);
	}
out:
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);
	vrele(fdvp);
	vrele(fvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that it is a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs file rename rpc called from nfs_remove() above
 */
int
nfs_renameit(sdvp, scnp, sp)
	struct vnode *sdvp;
	struct componentname *scnp;
	struct sillyrename *sp;
{
	return (nfs_renamerpc(sdvp, scnp->cn_nameptr, scnp->cn_namelen,
		sdvp, sp->s_name, sp->s_namlen, scnp->cn_cred, scnp->cn_proc));
}

/*
 * Do an nfs rename rpc. Called from nfs_rename() and nfs_renameit().
 */
int
nfs_renamerpc(fdvp, fnameptr, fnamelen, tdvp, tnameptr, tnamelen, cred, proc)
	struct vnode *fdvp;
	const char *fnameptr;
	int fnamelen;
	struct vnode *tdvp;
	const char *tnameptr;
	int tnamelen;
	struct ucred *cred;
	struct proc *proc;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, fwccflag = NFSV3_WCCRATTR, twccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(fdvp);

	nfsstats.rpccnt[NFSPROC_RENAME]++;
	nfsm_reqhead(fdvp, NFSPROC_RENAME,
		(NFSX_FH(v3) + NFSX_UNSIGNED)*2 + nfsm_rndup(fnamelen) +
		nfsm_rndup(tnamelen));
	nfsm_fhtom(fdvp, v3);
	nfsm_strtom(fnameptr, fnamelen, NFS_MAXNAMLEN);
	nfsm_fhtom(tdvp, v3);
	nfsm_strtom(tnameptr, tnamelen, NFS_MAXNAMLEN);
	nfsm_request(fdvp, NFSPROC_RENAME, proc, cred);
	if (v3) {
		nfsm_wcc_data(fdvp, fwccflag);
		nfsm_wcc_data(tdvp, twccflag);
	}
	nfsm_reqdone;
	VTONFS(fdvp)->n_flag |= NMODIFIED;
	VTONFS(tdvp)->n_flag |= NMODIFIED;
	if (!fwccflag)
		VTONFS(fdvp)->n_attrstamp = 0;
	if (!twccflag)
		VTONFS(tdvp)->n_attrstamp = 0;
	return (error);
}

/*
 * nfs hard link create call
 */
int
nfs_link(v)
	void *v;
{
	struct vop_link_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR, attrflag = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3;

	if (dvp->v_mount != vp->v_mount) {
		VOP_ABORTOP(vp, cnp);
		vput(dvp);
		return (EXDEV);
	}

	/*
	 * Push all writes to the server, so that the attribute cache
	 * doesn't get "out of sync" with the server.
	 * XXX There should be a better way!
	 */
	VOP_FSYNC(vp, cnp->cn_cred, FSYNC_WAIT, cnp->cn_proc);

	v3 = NFS_ISV3(vp);
	nfsstats.rpccnt[NFSPROC_LINK]++;
	nfsm_reqhead(vp, NFSPROC_LINK,
		NFSX_FH(v3)*2 + NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen));
	nfsm_fhtom(vp, v3);
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_request(vp, NFSPROC_LINK, cnp->cn_proc, cnp->cn_cred);
	if (v3) {
		nfsm_postop_attr(vp, attrflag);
		nfsm_wcc_data(dvp, wccflag);
	}
	nfsm_reqdone;
	FREE(cnp->cn_pnbuf, M_NAMEI);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!attrflag)
		VTONFS(vp)->n_attrstamp = 0;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	vput(dvp);
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 */
	if (error == EEXIST)
		error = 0;
	return (error);
}

/*
 * nfs symbolic link create call
 */
int
nfs_symlink(v)
	void *v;
{
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int slen, error = 0, wccflag = NFSV3_WCCRATTR, gotvp;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct vnode *newvp = (struct vnode *)0;
	int v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_SYMLINK]++;
	slen = strlen(ap->a_target);
	nfsm_reqhead(dvp, NFSPROC_SYMLINK, NFSX_FH(v3) + 2*NFSX_UNSIGNED +
	    nfsm_rndup(cnp->cn_namelen) + nfsm_rndup(slen) + NFSX_SATTR(v3));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	if (v3)
		nfsm_v3attrbuild(vap, FALSE);
	nfsm_strtom(ap->a_target, slen, NFS_MAXPATHLEN);
	if (!v3) {
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(VLNK, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = nfs_xdrneg1;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dvp, NFSPROC_SYMLINK, cnp->cn_proc, cnp->cn_cred);
	if (v3) {
		if (!error)
			nfsm_mtofh(dvp, newvp, v3, gotvp);
		nfsm_wcc_data(dvp, wccflag);
	}
	nfsm_reqdone;
	if (newvp)
		vrele(newvp);
	FREE(cnp->cn_pnbuf, M_NAMEI);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	vrele(dvp);
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 */
	if (error == EEXIST)
		error = 0;
	return (error);
}

/*
 * nfs make dir call
 */
int
nfs_mkdir(v)
	void *v;
{
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	int len;
	struct nfsnode *np = (struct nfsnode *)0;
	struct vnode *newvp = (struct vnode *)0;
	caddr_t bpos, dpos, cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	int gotvp = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(dvp);

	len = cnp->cn_namelen;
	nfsstats.rpccnt[NFSPROC_MKDIR]++;
	nfsm_reqhead(dvp, NFSPROC_MKDIR,
	  NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len) + NFSX_SATTR(v3));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);
	if (v3) {
		nfsm_v3attrbuild(vap, FALSE);
	} else {
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(VDIR, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = nfs_xdrneg1;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dvp, NFSPROC_MKDIR, cnp->cn_proc, cnp->cn_cred);
	if (!error)
		nfsm_mtofh(dvp, newvp, v3, gotvp);
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	nfsm_reqdone;
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	/*
	 * Kludge: Map EEXIST => 0 assuming that you have a reply to a retry
	 * if we can succeed in looking up the directory.
	 */
	if (error == EEXIST || (!error && !gotvp)) {
		if (newvp) {
			vrele(newvp);
			newvp = (struct vnode *)0;
		}
		error = nfs_lookitup(dvp, cnp->cn_nameptr, len, cnp->cn_cred,
			cnp->cn_proc, &np);
		if (!error) {
			newvp = NFSTOV(np);
			if (newvp->v_type != VDIR)
				error = EEXIST;
		}
	}
	if (error) {
		if (newvp)
			vrele(newvp);
	} else {
		if (cnp->cn_flags & MAKEENTRY)
			cache_enter(dvp, newvp, cnp);
		*ap->a_vpp = newvp;
	}
	FREE(cnp->cn_pnbuf, M_NAMEI);
	vrele(dvp);
	return (error);
}

/*
 * nfs remove directory call
 */
int
nfs_rmdir(v)
	void *v;
{
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int v3 = NFS_ISV3(dvp);

	if (dvp == vp) {
		vrele(dvp);
		vrele(dvp);
		FREE(cnp->cn_pnbuf, M_NAMEI);
		return (EINVAL);
	}
	nfsstats.rpccnt[NFSPROC_RMDIR]++;
	nfsm_reqhead(dvp, NFSPROC_RMDIR,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_RMDIR, cnp->cn_proc, cnp->cn_cred);
	if (v3)
		nfsm_wcc_data(dvp, wccflag);
	nfsm_reqdone;
	FREE(cnp->cn_pnbuf, M_NAMEI);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		VTONFS(dvp)->n_attrstamp = 0;
	cache_purge(dvp);
	cache_purge(vp);
	vrele(vp);
	vrele(dvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that you have a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs readdir call
 */
int
nfs_readdir(v)
	void *v;
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		off_t **a_cookies;
		int *a_ncookies;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	char *base = uio->uio_iov->iov_base;
	int tresid, error;
	size_t count, lost;
	struct dirent *dp;
	off_t *cookies = NULL;
	int ncookies = 0, nc;

	if (vp->v_type != VDIR)
		return (EPERM);

	lost = uio->uio_resid & (NFS_DIRFRAGSIZ - 1);
	count = uio->uio_resid - lost;
	if (count <= 0)
		return (EINVAL);

	/*
	 * Call nfs_bioread() to do the real work.
	 */
	tresid = uio->uio_resid = count;
	error = nfs_bioread(vp, uio, 0, ap->a_cred,
		    ap->a_cookies ? NFSBIO_CACHECOOKIES : 0);

	if (!error && ap->a_cookies) {
		ncookies = count / 16;
		MALLOC(cookies, off_t *, sizeof (off_t) * ncookies, M_TEMP,
		    M_WAITOK);
		*ap->a_cookies = cookies;
	}

	if (!error && uio->uio_resid == tresid) {
		uio->uio_resid += lost;
		nfsstats.direofcache_misses++;
		if (ap->a_cookies)
			*ap->a_ncookies = 0;
		*ap->a_eofflag = 1;
		return (0);
	}

	if (!error && ap->a_cookies) {
		/*
		 * Only the NFS server and emulations use cookies, and they
		 * load the directory block into system space, so we can
		 * just look at it directly.
		 */
		if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
			panic("nfs_readdir: lost in space");
		for (nc = 0; ncookies-- &&
		     base < (char *)uio->uio_iov->iov_base; nc++){
			dp = (struct dirent *) base;
			if (dp->d_reclen == 0)
				break;
			if (nmp->nm_flag & NFSMNT_XLATECOOKIE)
				*(cookies++) = (off_t)NFS_GETCOOKIE32(dp);
			else
				*(cookies++) = NFS_GETCOOKIE(dp);
			base += dp->d_reclen;
		}
		uio->uio_resid +=
		    ((caddr_t)uio->uio_iov->iov_base - base);
		uio->uio_iov->iov_len +=
		    ((caddr_t)uio->uio_iov->iov_base - base);
		uio->uio_iov->iov_base = base;
		*ap->a_ncookies = nc;
	}

	uio->uio_resid += lost;
	*ap->a_eofflag = 0;
	return (error);
}

/*
 * Readdir rpc call.
 * Called from below the buffer cache by nfs_doio().
 */
int
nfs_readdirrpc(vp, uiop, cred)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
	int len, left;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	caddr_t bpos, dpos, cp2;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp);
	u_quad_t fileno;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, bigenough = 1;
	int attrflag, nrpcs = 0, reclen;
	int v3 = NFS_ISV3(vp);
	nfsquad_t cookie;

#ifdef DIAGNOSTIC
	/*
	 * Should be called from buffer cache, so only amount of
	 * NFS_DIRBLKSIZ will be requested.
	 */
	if (uiop->uio_iovcnt != 1 || (uiop->uio_resid & (NFS_DIRBLKSIZ - 1)))
		panic("nfs readdirrpc bad uio");
#endif

	/*
	 * Loop around doing readdir rpc's of size nm_readdirsize
	 * truncated to a multiple of NFS_DIRFRAGSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && bigenough) {
		/*
		 * Heuristic: don't bother to do another RPC to further
		 * fill up this block if there is not much room left. (< 50%
		 * of the readdir RPC size). This wastes some buffer space
		 * but can save up to 50% in RPC calls.
		 */
		if (nrpcs > 0 && uiop->uio_resid < (nmp->nm_readdirsize / 2)) {
			bigenough = 0;
			break;
		}
		nfsstats.rpccnt[NFSPROC_READDIR]++;
		nfsm_reqhead(vp, NFSPROC_READDIR, NFSX_FH(v3) +
			NFSX_READDIR(v3));
		nfsm_fhtom(vp, v3);
		if (v3) {
			nfsm_build(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			cookie.qval = uiop->uio_offset;
			if (nmp->nm_iflag & NFSMNT_SWAPCOOKIE) {
				txdr_swapcookie3(uiop->uio_offset, tl);
			} else {
				txdr_cookie3(uiop->uio_offset, tl);
			}
			tl += 2;
			*tl++ = dnp->n_cookieverf.nfsuquad[0];
			*tl++ = dnp->n_cookieverf.nfsuquad[1];
		} else {
			nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(uiop->uio_offset);
		}
		*tl = txdr_unsigned(nmp->nm_readdirsize);
		nfsm_request(vp, NFSPROC_READDIR, uiop->uio_procp, cred);
		nrpcs++;
		if (v3) {
			nfsm_postop_attr(vp, attrflag);
			if (!error) {
				nfsm_dissect(tl, u_int32_t *,
				    2 * NFSX_UNSIGNED);
				dnp->n_cookieverf.nfsuquad[0] = *tl++;
				dnp->n_cookieverf.nfsuquad[1] = *tl;
			} else {
				m_freem(mrep);
				goto nfsmout;
			}
		}
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		more_dirs = fxdr_unsigned(int, *tl);
	
		/* loop thru the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			if (v3) {
				nfsm_dissect(tl, u_int32_t *,
				    3 * NFSX_UNSIGNED);
				fileno = fxdr_hyper(tl);
				len = fxdr_unsigned(int, *(tl + 2));
			} else {
				nfsm_dissect(tl, u_int32_t *,
				    2 * NFSX_UNSIGNED);
				fileno = fxdr_unsigned(u_quad_t, *tl++);
				len = fxdr_unsigned(int, *tl);
			}
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				m_freem(mrep);
				goto nfsmout;
			}
			tlen = nfsm_rndup(len);
			if (tlen == len)
				tlen += 4;	/* To ensure null termination */
			tlen += sizeof (off_t) + sizeof (int);
			reclen = ALIGN(tlen + DIRHDSIZ);
			tlen = reclen - DIRHDSIZ;
			left = NFS_DIRFRAGSIZ - blksiz;
			if (reclen > left) {
				dp->d_reclen += left;
				(caddr_t)uiop->uio_iov->iov_base += left;
				uiop->uio_iov->iov_len -= left;
				uiop->uio_resid -= left;
				blksiz = 0;
				NFS_STASHCOOKIE(dp, uiop->uio_offset);
			}
			if (reclen > uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				dp = (struct dirent *)uiop->uio_iov->iov_base;
				dp->d_fileno = (int)fileno;
				dp->d_namlen = len;
				dp->d_reclen = reclen;
				dp->d_type = DT_UNKNOWN;
				blksiz += dp->d_reclen;
				if (blksiz == NFS_DIRFRAGSIZ)
					blksiz = 0;
				uiop->uio_resid -= DIRHDSIZ;
				(caddr_t)uiop->uio_iov->iov_base += DIRHDSIZ;
				uiop->uio_iov->iov_len -= DIRHDSIZ;
				nfsm_mtouio(uiop, len);
				cp = uiop->uio_iov->iov_base;
				tlen -= len;
				*cp = '\0';	/* null terminate */
				(caddr_t)uiop->uio_iov->iov_base += tlen;
				uiop->uio_iov->iov_len -= tlen;
				uiop->uio_resid -= tlen;
			} else
				nfsm_adv(nfsm_rndup(len));
			if (v3) {
				nfsm_dissect(tl, u_int32_t *,
				    3 * NFSX_UNSIGNED);
			} else {
				nfsm_dissect(tl, u_int32_t *,
				    2 * NFSX_UNSIGNED);
			}
			if (bigenough) {
				if (v3) {
					if (nmp->nm_iflag & NFSMNT_SWAPCOOKIE)
						uiop->uio_offset =
						    fxdr_swapcookie3(tl);
					else
						uiop->uio_offset = 
						    fxdr_cookie3(tl);
				}
				else {
					uiop->uio_offset =
					    fxdr_unsigned(off_t, *tl);
				}
				NFS_STASHCOOKIE(dp, uiop->uio_offset);
			}
			if (v3)
				tl += 2;
			else
				tl++;
			more_dirs = fxdr_unsigned(int, *tl);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *tl) == 0);
		}
		m_freem(mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of NFS_DIRFRAGSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = NFS_DIRFRAGSIZ - blksiz;
		dp->d_reclen += left;
		NFS_STASHCOOKIE(dp, uiop->uio_offset);
		(caddr_t)uiop->uio_iov->iov_base += left;
		uiop->uio_iov->iov_len -= left;
		uiop->uio_resid -= left;
	}

	/*
	 * We are now either at the end of the directory or have filled the
	 * block.
	 */
	if (bigenough)
		dnp->n_direofoffset = uiop->uio_offset;
nfsmout:
	return (error);
}

/*
 * NFS V3 readdir plus RPC. Used in place of nfs_readdirrpc().
 */
int
nfs_readdirplusrpc(vp, uiop, cred)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
	int len, left;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	struct vnode *newvp;
	caddr_t bpos, dpos, cp2;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nameidata nami, *ndp = &nami;
	struct componentname *cnp = &ndp->ni_cnd;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp), *np;
	const unsigned char *hcp;
	nfsfh_t *fhp;
	u_quad_t fileno;
	int error = 0, tlen, more_dirs = 1, blksiz = 0, doit, bigenough = 1, i;
	int attrflag, fhsize, nrpcs = 0, reclen;
	struct nfs_fattr fattr, *fp;

#ifdef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1 || (uiop->uio_resid & (NFS_DIRBLKSIZ - 1)))
		panic("nfs readdirplusrpc bad uio");
#endif
	ndp->ni_dvp = vp;
	newvp = NULLVP;

	/*
	 * Loop around doing readdir rpc's of size nm_readdirsize
	 * truncated to a multiple of NFS_DIRFRAGSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && bigenough) {
		if (nrpcs > 0 && uiop->uio_resid < (nmp->nm_readdirsize / 2)) {
			bigenough = 0;
			break;
		}
		nfsstats.rpccnt[NFSPROC_READDIRPLUS]++;
		nfsm_reqhead(vp, NFSPROC_READDIRPLUS,
			NFSX_FH(1) + 6 * NFSX_UNSIGNED);
		nfsm_fhtom(vp, 1);
 		nfsm_build(tl, u_int32_t *, 6 * NFSX_UNSIGNED);
		if (nmp->nm_iflag & NFSMNT_SWAPCOOKIE) {
			txdr_swapcookie3(uiop->uio_offset, tl);
		} else {
			txdr_cookie3(uiop->uio_offset, tl);
		}
		tl += 2;
		*tl++ = dnp->n_cookieverf.nfsuquad[0];
		*tl++ = dnp->n_cookieverf.nfsuquad[1];
		*tl++ = txdr_unsigned(nmp->nm_readdirsize);
		*tl = txdr_unsigned(nmp->nm_rsize);
		nfsm_request(vp, NFSPROC_READDIRPLUS, uiop->uio_procp, cred);
		nfsm_postop_attr(vp, attrflag);
		if (error) {
			m_freem(mrep);
			goto nfsmout;
		}
		nrpcs++;
		nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
		dnp->n_cookieverf.nfsuquad[0] = *tl++;
		dnp->n_cookieverf.nfsuquad[1] = *tl++;
		more_dirs = fxdr_unsigned(int, *tl);
	
		/* loop thru the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			fileno = fxdr_hyper(tl);
			len = fxdr_unsigned(int, *(tl + 2));
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				m_freem(mrep);
				goto nfsmout;
			}
			tlen = nfsm_rndup(len);
			if (tlen == len)
				tlen += 4;	/* To ensure null termination*/
			tlen += sizeof (off_t) + sizeof (int);
			reclen = ALIGN(tlen + DIRHDSIZ);
			tlen = reclen - DIRHDSIZ;
			left = NFS_DIRFRAGSIZ - blksiz;
			if (reclen > left) {
				/*
				 * DIRFRAGSIZ is aligned, no need to align
				 * again here.
				 */
				dp->d_reclen += left;
				(caddr_t)uiop->uio_iov->iov_base += left;
				uiop->uio_iov->iov_len -= left;
				uiop->uio_resid -= left;
				NFS_STASHCOOKIE(dp, uiop->uio_offset);
				blksiz = 0;
			}
			if (reclen > uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				dp = (struct dirent *)uiop->uio_iov->iov_base;
				dp->d_fileno = (int)fileno;
				dp->d_namlen = len;
				dp->d_reclen = reclen;
				dp->d_type = DT_UNKNOWN;
				blksiz += dp->d_reclen;
				if (blksiz == NFS_DIRFRAGSIZ)
					blksiz = 0;
				uiop->uio_resid -= DIRHDSIZ;
				(caddr_t)uiop->uio_iov->iov_base += DIRHDSIZ;
				uiop->uio_iov->iov_len -= DIRHDSIZ;
				cnp->cn_nameptr = uiop->uio_iov->iov_base;
				cnp->cn_namelen = len;
				nfsm_mtouio(uiop, len);
				cp = uiop->uio_iov->iov_base;
				tlen -= len;
				*cp = '\0';
				(caddr_t)uiop->uio_iov->iov_base += tlen;
				uiop->uio_iov->iov_len -= tlen;
				uiop->uio_resid -= tlen;
			} else
				nfsm_adv(nfsm_rndup(len));
			nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			if (bigenough) {
				if (nmp->nm_iflag & NFSMNT_SWAPCOOKIE)
					uiop->uio_offset =
						fxdr_swapcookie3(tl);
				else
					uiop->uio_offset =
						fxdr_cookie3(tl);
				NFS_STASHCOOKIE(dp, uiop->uio_offset);
			}
			tl += 2;

			/*
			 * Since the attributes are before the file handle
			 * (sigh), we must skip over the attributes and then
			 * come back and get them.
			 */
			attrflag = fxdr_unsigned(int, *tl);
			if (attrflag) {
			    nfsm_dissect(fp, struct nfs_fattr *, NFSX_V3FATTR);
			    memcpy(&fattr, fp, NFSX_V3FATTR);
			    nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			    doit = fxdr_unsigned(int, *tl);
			    if (doit) {
				nfsm_getfh(fhp, fhsize, 1);
				if (NFS_CMPFH(dnp, fhp, fhsize)) {
				    VREF(vp);
				    newvp = vp;
				    np = dnp;
				} else {
				    error = nfs_nget(vp->v_mount, fhp,
					fhsize, &np);
				    if (!error)
					newvp = NFSTOV(np);
				}
				if (!error) {
				    nfs_loadattrcache(&newvp, &fattr, 0);
				    dp->d_type =
				        IFTODT(VTTOIF(np->n_vattr->va_type));
				    ndp->ni_vp = newvp;
				    cnp->cn_hash = 0;
				    for (hcp = cnp->cn_nameptr, i = 1; i <= len;
				        i++, hcp++)
				        cnp->cn_hash += *hcp * i;
				    if (cnp->cn_namelen <= NCHNAMLEN)
				        cache_enter(ndp->ni_dvp, ndp->ni_vp,
						    cnp);
				}
			   }
			} else {
			    /* Just skip over the file handle */
			    nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			    i = fxdr_unsigned(int, *tl);
			    nfsm_adv(nfsm_rndup(i));
			}
			if (newvp != NULLVP) {
			    vrele(newvp);
			    newvp = NULLVP;
			}
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			more_dirs = fxdr_unsigned(int, *tl);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *tl) == 0);
		}
		m_freem(mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of NFS_DIRFRAGSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = NFS_DIRFRAGSIZ - blksiz;
		dp->d_reclen += left;
		NFS_STASHCOOKIE(dp, uiop->uio_offset);
		(caddr_t)uiop->uio_iov->iov_base += left;
		uiop->uio_iov->iov_len -= left;
		uiop->uio_resid -= left;
	}

	/*
	 * We are now either at the end of the directory or have filled the
	 * block.
	 */
	if (bigenough)
		dnp->n_direofoffset = uiop->uio_offset;
nfsmout:
	if (newvp != NULLVP)
		vrele(newvp);
	return (error);
}
static char hextoasc[] = "0123456789abcdef";

/*
 * Silly rename. To make the NFS filesystem that is stateless look a little
 * more like the "ufs" a remove of an active vnode is translated to a rename
 * to a funny looking filename that is removed by nfs_inactive on the
 * nfsnode. There is the potential for another process on a different client
 * to create the same funny name between the nfs_lookitup() fails and the
 * nfs_rename() completes, but...
 */
int
nfs_sillyrename(dvp, vp, cnp)
	struct vnode *dvp, *vp;
	struct componentname *cnp;
{
	struct sillyrename *sp;
	struct nfsnode *np;
	int error;
	short pid;

	cache_purge(dvp);
	np = VTONFS(vp);
#ifndef DIAGNOSTIC
	if (vp->v_type == VDIR)
		panic("nfs: sillyrename dir");
#endif
	MALLOC(sp, struct sillyrename *, sizeof (struct sillyrename),
		M_NFSREQ, M_WAITOK);
	sp->s_cred = crdup(cnp->cn_cred);
	sp->s_dvp = dvp;
	VREF(dvp);

	/* Fudge together a funny name */
	pid = cnp->cn_proc->p_pid;
	memcpy(sp->s_name, ".nfsAxxxx4.4", 13);
	sp->s_namlen = 12;
	sp->s_name[8] = hextoasc[pid & 0xf];
	sp->s_name[7] = hextoasc[(pid >> 4) & 0xf];
	sp->s_name[6] = hextoasc[(pid >> 8) & 0xf];
	sp->s_name[5] = hextoasc[(pid >> 12) & 0xf];

	/* Try lookitups until we get one that isn't there */
	while (nfs_lookitup(dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		cnp->cn_proc, (struct nfsnode **)0) == 0) {
		sp->s_name[4]++;
		if (sp->s_name[4] > 'z') {
			error = EINVAL;
			goto bad;
		}
	}
	error = nfs_renameit(dvp, cnp, sp);
	if (error)
		goto bad;
	error = nfs_lookitup(dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		cnp->cn_proc, &np);
	np->n_sillyrename = sp;
	return (0);
bad:
	vrele(sp->s_dvp);
	crfree(sp->s_cred);
	free((caddr_t)sp, M_NFSREQ);
	return (error);
}

/*
 * Look up a file name and optionally either update the file handle or
 * allocate an nfsnode, depending on the value of npp.
 * npp == NULL	--> just do the lookup
 * *npp == NULL --> allocate a new nfsnode and make sure attributes are
 *			handled too
 * *npp != NULL --> update the file handle in the vnode
 */
int
nfs_lookitup(dvp, name, len, cred, procp, npp)
	struct vnode *dvp;
	const char *name;
	int len;
	struct ucred *cred;
	struct proc *procp;
	struct nfsnode **npp;
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	struct vnode *newvp = (struct vnode *)0;
	struct nfsnode *np, *dnp = VTONFS(dvp);
	caddr_t bpos, dpos, cp2;
	int error = 0, fhlen, attrflag;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	nfsfh_t *nfhp;
	int v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	nfsm_reqhead(dvp, NFSPROC_LOOKUP,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len));
	nfsm_fhtom(dvp, v3);
	nfsm_strtom(name, len, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_LOOKUP, procp, cred);
	if (npp && !error) {
		nfsm_getfh(nfhp, fhlen, v3);
		if (*npp) {
		    np = *npp;
		    if (np->n_fhsize > NFS_SMALLFH && fhlen <= NFS_SMALLFH) {
			free((caddr_t)np->n_fhp, M_NFSBIGFH);
			np->n_fhp = &np->n_fh;
		    } else if (np->n_fhsize <= NFS_SMALLFH && fhlen>NFS_SMALLFH)
			np->n_fhp =(nfsfh_t *)malloc(fhlen,M_NFSBIGFH,M_WAITOK);
		    memcpy((caddr_t)np->n_fhp, (caddr_t)nfhp, fhlen);
		    np->n_fhsize = fhlen;
		    newvp = NFSTOV(np);
		} else if (NFS_CMPFH(dnp, nfhp, fhlen)) {
		    VREF(dvp);
		    newvp = dvp;
		} else {
		    error = nfs_nget(dvp->v_mount, nfhp, fhlen, &np);
		    if (error) {
			m_freem(mrep);
			return (error);
		    }
		    newvp = NFSTOV(np);
		}
		if (v3) {
			nfsm_postop_attr(newvp, attrflag);
			if (!attrflag && *npp == NULL) {
				m_freem(mrep);
				vrele(newvp);
				return (ENOENT);
			}
		} else
			nfsm_loadattr(newvp, (struct vattr *)0);
	}
	nfsm_reqdone;
	if (npp && *npp == NULL) {
		if (error) {
			if (newvp)
				vrele(newvp);
		} else
			*npp = np;
	}
	return (error);
}

/*
 * Nfs Version 3 commit rpc
 */
int
nfs_commit(vp, offset, cnt, cred, procp)
	struct vnode *vp;
	u_quad_t offset;
	int cnt;
	struct ucred *cred;
	struct proc *procp;
{
	caddr_t cp;
	u_int32_t *tl;
	int32_t t1, t2;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	caddr_t bpos, dpos, cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	
	if ((nmp->nm_iflag & NFSMNT_HASWRITEVERF) == 0)
		return (0);
	nfsstats.rpccnt[NFSPROC_COMMIT]++;
	nfsm_reqhead(vp, NFSPROC_COMMIT, NFSX_FH(1));
	nfsm_fhtom(vp, 1);
	nfsm_build(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
	txdr_hyper(offset, tl);
	tl += 2;
	*tl = txdr_unsigned(cnt);
	nfsm_request(vp, NFSPROC_COMMIT, procp, cred);
	nfsm_wcc_data(vp, wccflag);
	if (!error) {
		nfsm_dissect(tl, u_int32_t *, NFSX_V3WRITEVERF);
		if (memcmp((caddr_t)nmp->nm_verf, (caddr_t)tl,
			NFSX_V3WRITEVERF)) {
			memcpy((caddr_t)nmp->nm_verf, (caddr_t)tl,
				NFSX_V3WRITEVERF);
			error = NFSERR_STALEWRITEVERF;
		}
	}
	nfsm_reqdone;
	return (error);
}

/*
 * Kludge City..
 * - make nfs_bmap() essentially a no-op that does no translation
 * - do nfs_strategy() by doing I/O with nfs_readrpc/nfs_writerpc
 *   (Maybe I could use the process's page mapping, but I was concerned that
 *    Kernel Write might not be enabled and also figured copyout() would do
 *    a lot more work than memcpy() and also it currently happens in the
 *    context of the swapper process (2).
 */
int
nfs_bmap(v)
	void *v;
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn * btodb(vp->v_mount->mnt_stat.f_iosize);
	return (0);
}

/*
 * Strategy routine.
 * For async requests when nfsiod(s) are running, queue the request by
 * calling nfs_asyncio(), otherwise just all nfs_doio() to do the
 * request.
 */
int
nfs_strategy(v)
	void *v;
{
	struct vop_strategy_args *ap = v;
	struct buf *bp = ap->a_bp;
	struct ucred *cr;
	struct proc *p;
	int error = 0;

	if ((bp->b_flags & (B_PHYS|B_ASYNC)) == (B_PHYS|B_ASYNC))
		panic("nfs physio/async");
	if (bp->b_flags & B_ASYNC)
		p = (struct proc *)0;
	else
		p = curproc;	/* XXX */
	if (bp->b_flags & B_READ)
		cr = bp->b_rcred;
	else
		cr = bp->b_wcred;
	/*
	 * If the op is asynchronous and an i/o daemon is waiting
	 * queue the request, wake it up and wait for completion
	 * otherwise just do it ourselves.
	 */
	if ((bp->b_flags & B_ASYNC) == 0 ||
		nfs_asyncio(bp, NOCRED))
		error = nfs_doio(bp, cr, p);
	return (error);
}

/*
 * Mmap a file
 *
 * NB Currently unsupported.
 */
/* ARGSUSED */
int
nfs_mmap(v)
	void *v;
{
#if 0
	struct vop_mmap_args /* {
		struct vnode *a_vp;
		int a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
#endif

	return (EINVAL);
}

/*
 * fsync vnode op. Just call nfs_flush() with commit == 1.
 */
/* ARGSUSED */
int
nfs_fsync(v)
	void *v;
{
	struct vop_fsync_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_vp;
		struct ucred * a_cred;
		int  a_flags;
		struct proc * a_p;
	} */ *ap = v;

	return (nfs_flush(ap->a_vp, ap->a_cred,
	    (ap->a_flags & FSYNC_WAIT) != 0 ? MNT_WAIT : 0, ap->a_p, 1));
}

/*
 * Flush all the blocks associated with a vnode.
 * 	Walk through the buffer pool and push any dirty pages
 *	associated with the vnode.
 */
int
nfs_flush(vp, cred, waitfor, p, commit)
	struct vnode *vp;
	struct ucred *cred;
	int waitfor;
	struct proc *p;
	int commit;
{
	struct nfsnode *np = VTONFS(vp);
	struct buf *bp;
	int i;
	struct buf *nbp;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int s, error = 0, slptimeo = 0, slpflag = 0, retv, bvecpos;
	int passone = 1;
	u_quad_t off, endoff, toff;
	struct ucred* wcred;
#ifndef NFS_COMMITBVECSIZ
#define NFS_COMMITBVECSIZ	20
#endif
	struct buf *bvec[NFS_COMMITBVECSIZ];

	if (nmp->nm_flag & NFSMNT_INT)
		slpflag = PCATCH;
	if (!commit)
		passone = 0;
	/*
	 * A b_flags == (B_DELWRI | B_NEEDCOMMIT) block has been written to the
	 * server, but nas not been committed to stable storage on the server
	 * yet. On the first pass, the byte range is worked out and the commit
	 * rpc is done. On the second pass, nfs_writebp() is called to do the
	 * job.
	 */
again:
	bvecpos = 0;
	off = (u_quad_t)-1;
	endoff = 0;
	wcred = NULL;
	if (NFS_ISV3(vp) && commit) {
		s = splbio();
		for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = nbp) {
			nbp = bp->b_vnbufs.le_next;
			if (bvecpos >= NFS_COMMITBVECSIZ)
				break;
			if ((bp->b_flags & (B_BUSY | B_DELWRI | B_NEEDCOMMIT))
				!= (B_DELWRI | B_NEEDCOMMIT))
				continue;
			bremfree(bp);
			/*
			 * Work out if all buffers are using the same cred
			 * so we can deal with them all with one commit.
			 */
			if (wcred == NULL)
				wcred = bp->b_wcred;
			else if (wcred != bp->b_wcred)
				wcred = NOCRED;
			bp->b_flags |= (B_BUSY | B_WRITEINPROG);
			/*
			 * A list of these buffers is kept so that the
			 * second loop knows which buffers have actually
			 * been committed. This is necessary, since there
			 * may be a race between the commit rpc and new
			 * uncommitted writes on the file.
			 */
			bvec[bvecpos++] = bp;
			toff = ((u_quad_t)bp->b_blkno) * DEV_BSIZE +
				bp->b_dirtyoff;
			if (toff < off)
				off = toff;
			toff += (u_quad_t)(bp->b_dirtyend - bp->b_dirtyoff);
			if (toff > endoff)
				endoff = toff;
		}
		splx(s);
	}
	if (bvecpos > 0) {
		/*
		 * Commit data on the server, as required.
		 * If all bufs are using the same wcred, then use that with
		 * one call for all of them, otherwise commit each one
		 * separately.
		 */
		if (wcred != NOCRED)
			retv = nfs_commit(vp, off, (int)(endoff - off),
					  wcred, p);
		else {
			retv = 0;
			for (i = 0; i < bvecpos; i++) {
				off_t off, size;
				bp = bvec[i];
				off = ((u_quad_t)bp->b_blkno) * DEV_BSIZE +
					bp->b_dirtyoff;
				size = (u_quad_t)(bp->b_dirtyend
						  - bp->b_dirtyoff);
				retv = nfs_commit(vp, off, (int)size,
						  bp->b_wcred, p);
				if (retv) break;
			}
		}

		if (retv == NFSERR_STALEWRITEVERF)
			nfs_clearcommit(vp->v_mount);
		/*
		 * Now, either mark the blocks I/O done or mark the
		 * blocks dirty, depending on whether the commit
		 * succeeded.
		 */
		for (i = 0; i < bvecpos; i++) {
			bp = bvec[i];
			bp->b_flags &= ~(B_NEEDCOMMIT | B_WRITEINPROG);
			if (retv)
			    brelse(bp);
			else {
			    s = splbio();
			    vp->v_numoutput++;
			    bp->b_flags |= B_ASYNC;
			    bp->b_flags &= ~(B_READ|B_DONE|B_ERROR|B_DELWRI);
			    bp->b_dirtyoff = bp->b_dirtyend = 0;
			    reassignbuf(bp, vp);
			    splx(s);
			    biodone(bp);
			}
		}

		/*
		 * If there may be more uncommitted buffer, try to
		 * commit them unless write verf isn't changed.
		 */
		if (retv != NFSERR_STALEWRITEVERF &&
		    bvecpos == NFS_COMMITBVECSIZ)
			goto again;
	}

	/*
	 * Start/do any write(s) that are required.
	 */
loop:
	s = splbio();
	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = nbp) {
		nbp = bp->b_vnbufs.le_next;
		if (bp->b_flags & B_BUSY) {
			if (waitfor != MNT_WAIT || passone)
				continue;
			bp->b_flags |= B_WANTED;
			error = tsleep((caddr_t)bp, slpflag | (PRIBIO + 1),
				"nfsfsync", slptimeo);
			splx(s);
			if (error) {
			    if (nfs_sigintr(nmp, (struct nfsreq *)0, p))
				return (EINTR);
			    if (slpflag == PCATCH) {
				slpflag = 0;
				slptimeo = 2 * hz;
			    }
			}
			goto loop;
		}
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("nfs_fsync: not dirty");
		if ((passone || !commit) && (bp->b_flags & B_NEEDCOMMIT))
			continue;
		bremfree(bp);
		if (passone || !commit)
		    bp->b_flags |= (B_BUSY|B_ASYNC);
		else
		    bp->b_flags |= (B_BUSY|B_ASYNC|B_WRITEINPROG|B_NEEDCOMMIT);
		splx(s);
		VOP_BWRITE(bp);
		goto loop;
	}
	splx(s);
	if (passone) {
		passone = 0;
		goto again;
	}
	if (waitfor == MNT_WAIT) {
		s = splbio();
		while (vp->v_numoutput) {
			vp->v_flag |= VBWAIT;
			error = tsleep((caddr_t)&vp->v_numoutput,
				slpflag | (PRIBIO + 1), "nfsfsync", slptimeo);
			if (error) {
			    splx(s);
			    if (nfs_sigintr(nmp, (struct nfsreq *)0, p))
				return (EINTR);
			    if (slpflag == PCATCH) {
				slpflag = 0;
				slptimeo = 2 * hz;
			    }
			    s = splbio();
			}
		}
		splx(s);
		if (vp->v_dirtyblkhd.lh_first && commit) {
#if 0
			vprint("nfs_fsync: dirty", vp);
#endif
			goto loop;
		}
	}
	if (np->n_flag & NWRITEERR) {
		error = np->n_error;
		np->n_flag &= ~NWRITEERR;
	}
	return (error);
}

/*
 * Return POSIX pathconf information applicable to nfs.
 *
 * N.B. The NFS V2 protocol doesn't support this RPC.
 */
/* ARGSUSED */
int
nfs_pathconf(v)
	void *v;
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;
	struct nfsv3_pathconf *pcp;
	struct vnode *vp = ap->a_vp;
	struct nfsmount *nmp;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int32_t t1, t2;
	u_int32_t *tl;
	caddr_t bpos, dpos, cp, cp2;
	int error = 0, attrflag;
	unsigned int l;
	u_int64_t maxsize;
	int v3 = NFS_ISV3(vp);

	switch (ap->a_name) {
		/* Names that can be resolved locally. */
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		break;
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		break;
	/* Names that cannot be resolved locally; do an RPC, if possible. */
	case _PC_LINK_MAX:
	case _PC_NAME_MAX:
	case _PC_CHOWN_RESTRICTED:
	case _PC_NO_TRUNC:
		if (!v3) {
			error = EINVAL;
			break;
		}
		nfsstats.rpccnt[NFSPROC_PATHCONF]++;
		nfsm_reqhead(vp, NFSPROC_PATHCONF, NFSX_FH(1));
		nfsm_fhtom(vp, 1);
		nfsm_request(vp, NFSPROC_PATHCONF,
		    curproc, curproc->p_ucred);	/* XXX */
		nfsm_postop_attr(vp, attrflag);
		if (!error) {
			nfsm_dissect(pcp, struct nfsv3_pathconf *,
			    NFSX_V3PATHCONF);
			switch (ap->a_name) {
			case _PC_LINK_MAX:
				*ap->a_retval =
				    fxdr_unsigned(register_t, pcp->pc_linkmax);
				break;
			case _PC_NAME_MAX:
				*ap->a_retval =
				    fxdr_unsigned(register_t, pcp->pc_namemax);
				break;
			case _PC_CHOWN_RESTRICTED:
				*ap->a_retval =
				    (pcp->pc_chownrestricted == nfs_true);
				break;
			case _PC_NO_TRUNC:
				*ap->a_retval =
				    (pcp->pc_notrunc == nfs_true);
				break;
			}
		}
		nfsm_reqdone;
		break;
	case _PC_FILESIZEBITS:
		if (v3) {
			nmp = VFSTONFS(vp->v_mount);
			if ((nmp->nm_iflag & NFSMNT_GOTFSINFO) == 0)
				if ((error = nfs_fsinfo(nmp, vp,
				    curproc->p_ucred, curproc)) != 0) /* XXX */
					break;
			for (l = 0, maxsize = nmp->nm_maxfilesize;
			    (maxsize >> l) > 0; l++)
				;
			*ap->a_retval = l + 1;
		} else {
			*ap->a_retval = 32;	/* NFS V2 limitation */
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * NFS advisory byte-level locks.
 */
int
nfs_advlock(v)
	void *v;
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	return lf_advlock(ap, &np->n_lockf, np->n_size);
}

/*
 * Print out the contents of an nfsnode.
 */
int
nfs_print(v)
	void *v;
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);

	printf("tag VT_NFS, fileid %ld fsid 0x%lx",
	    np->n_vattr->va_fileid, np->n_vattr->va_fsid);
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
	printf("\n");
	return (0);
}

/*
 * NFS file truncation.
 */
int
nfs_truncate(v)
	void *v;
{
#if 0
	struct vop_truncate_args /* {
		struct vnode *a_vp;
		off_t a_length;
		int a_flags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
#endif

	/* Use nfs_setattr */
	return (EOPNOTSUPP);
}

/*
 * NFS update.
 */
int
nfs_update(v)
	void *v;
#if 0
	struct vop_update_args /* {
		struct vnode *a_vp;
		struct timespec *a_ta;
		struct timespec *a_tm;
		int a_waitfor;
	} */ *ap = v;
#endif
{

	/* Use nfs_setattr */
	return (EOPNOTSUPP);
}

/*
 * Just call nfs_writebp() with the force argument set to 1.
 */
int
nfs_bwrite(v)
	void *v;
{
	struct vop_bwrite_args /* {
		struct vnode *a_bp;
	} */ *ap = v;

	return (nfs_writebp(ap->a_bp, 1));
}

/*
 * This is a clone of vn_bwrite(), except that B_WRITEINPROG isn't set unless
 * the force flag is one and it also handles the B_NEEDCOMMIT flag.
 */
int
nfs_writebp(bp, force)
	struct buf *bp;
	int force;
{
	int oldflags = bp->b_flags, retv = 1, s;
	struct proc *p = curproc;	/* XXX */
	off_t off;

	if(!(bp->b_flags & B_BUSY))
		panic("bwrite: buffer is not busy???");

#ifdef fvdl_debug
	printf("nfs_writebp(%x): vp %x voff %d vend %d doff %d dend %d\n",
	    bp, bp->b_vp, bp->b_validoff, bp->b_validend, bp->b_dirtyoff,
	    bp->b_dirtyend);
#endif
	bp->b_flags &= ~(B_READ|B_DONE|B_ERROR|B_DELWRI|B_AGE);

	s = splbio();
	if (oldflags & B_ASYNC) {
		if (oldflags & B_DELWRI) {
			reassignbuf(bp, bp->b_vp);
		} else if (p) {
			++p->p_stats->p_ru.ru_oublock;
		}
	}
	bp->b_vp->v_numoutput++;
	splx(s);

	/*
	 * If B_NEEDCOMMIT is set, a commit rpc may do the trick. If not
	 * an actual write will have to be scheduled via. VOP_STRATEGY().
	 * If B_WRITEINPROG is already set, then push it with a write anyhow.
	 */
	if ((oldflags & (B_NEEDCOMMIT | B_WRITEINPROG)) == B_NEEDCOMMIT) {
		off = ((u_quad_t)bp->b_blkno) * DEV_BSIZE + bp->b_dirtyoff;
		bp->b_flags |= B_WRITEINPROG;
		retv = nfs_commit(bp->b_vp, off, bp->b_dirtyend-bp->b_dirtyoff,
			bp->b_wcred, bp->b_proc);
		bp->b_flags &= ~B_WRITEINPROG;
		if (!retv) {
			bp->b_dirtyoff = bp->b_dirtyend = 0;
			bp->b_flags &= ~B_NEEDCOMMIT;
			biodone(bp);
		} else if (retv == NFSERR_STALEWRITEVERF)
			nfs_clearcommit(bp->b_vp->v_mount);
	}
	if (retv) {
		if (force)
			bp->b_flags |= B_WRITEINPROG;
		VOP_STRATEGY(bp);
	}

	if( (oldflags & B_ASYNC) == 0) {
		int rtval = biowait(bp);
		if (oldflags & B_DELWRI) {
			s = splbio();
			reassignbuf(bp, bp->b_vp);
			splx(s);
		} else if (p) {
			++p->p_stats->p_ru.ru_oublock;
		}
		brelse(bp);
		return (rtval);
	} 

	return (0);
}

/*
 * nfs special file access vnode op.
 * Essentially just get vattr and then imitate iaccess() since the device is
 * local to the client.
 */
int
nfsspec_access(v)
	void *v;
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vattr va;
	struct vnode *vp = ap->a_vp;
	int error;

	error = VOP_GETATTR(vp, &va, ap->a_cred, ap->a_p);
	if (error)
		return (error);

        /*
	 * Disallow write attempts on filesystems mounted read-only;
	 * unless the file is a socket, fifo, or a block or character
	 * device resident on the filesystem.
	 */
	if ((ap->a_mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
			return (EROFS);
		default:
			break;
		}
	}

	return (vaccess(va.va_type, va.va_mode,
	    va.va_uid, va.va_gid, ap->a_mode, ap->a_cred));
}

/*
 * Read wrapper for special devices.
 */
int
nfsspec_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set access flag.
	 */
	np->n_flag |= NACC;
	np->n_atim.tv_sec = time.tv_sec;
	np->n_atim.tv_nsec = time.tv_usec * 1000;
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for special devices.
 */
int
nfsspec_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set update flag.
	 */
	np->n_flag |= NUPD;
	np->n_mtim.tv_sec = time.tv_sec;
	np->n_mtim.tv_nsec = time.tv_usec * 1000;
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the nfsnode then do device close.
 */
int
nfsspec_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;

	if (np->n_flag & (NACC | NUPD)) {
		np->n_flag |= NCHG;
		if (vp->v_usecount == 1 &&
		    (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
			VATTR_NULL(&vattr);
			if (np->n_flag & NACC)
				vattr.va_atime = np->n_atim;
			if (np->n_flag & NUPD)
				vattr.va_mtime = np->n_mtim;
			(void)VOP_SETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		}
	}
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_close), ap));
}

/*
 * Read wrapper for fifos.
 */
int
nfsfifo_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set access flag.
	 */
	np->n_flag |= NACC;
	np->n_atim.tv_sec = time.tv_sec;
	np->n_atim.tv_nsec = time.tv_usec * 1000;
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for fifos.
 */
int
nfsfifo_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set update flag.
	 */
	np->n_flag |= NUPD;
	np->n_mtim.tv_sec = time.tv_sec;
	np->n_mtim.tv_nsec = time.tv_usec * 1000;
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for fifos.
 *
 * Update the times on the nfsnode then do fifo close.
 */
int
nfsfifo_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;

	if (np->n_flag & (NACC | NUPD)) {
		if (np->n_flag & NACC) {
			np->n_atim.tv_sec = time.tv_sec;
			np->n_atim.tv_nsec = time.tv_usec * 1000;
		}
		if (np->n_flag & NUPD) {
			np->n_mtim.tv_sec = time.tv_sec;
			np->n_mtim.tv_nsec = time.tv_usec * 1000;
		}
		np->n_flag |= NCHG;
		if (vp->v_usecount == 1 &&
		    (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
			VATTR_NULL(&vattr);
			if (np->n_flag & NACC)
				vattr.va_atime = np->n_atim;
			if (np->n_flag & NUPD)
				vattr.va_mtime = np->n_mtim;
			(void)VOP_SETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		}
	}
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_close), ap));
}
