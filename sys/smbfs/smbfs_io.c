/*	$NetBSD: smbfs_io.c,v 1.1.2.2 2001/01/08 14:58:18 bouyer Exp $	*/

/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>	/* defines plimit structure in proc struct */
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/signalvar.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>

#ifndef NetBSD
#if __FreeBSD_version < 400000
#include <vm/vm_prot.h>
#endif
#endif
#include <uvm/uvm_page.h>
#include <uvm/uvm_object.h>
#include <uvm/uvm_pager.h>
#ifndef NetBSD
#include <vm/vnode_pager.h>
#endif
/*
#include <sys/ioccom.h>
*/
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_lock.h>

#include <smbfs/smbfs.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>

#ifdef FB_CURRENT
#include <sys/bio.h>
#else
#define	bufdone	biodone
#endif
#include <sys/buf.h>

/*#define SMBFS_RWGENERIC*/

extern int smbfs_pbuf_freecnt;

static int smbfs_fastlookup = 1;

extern struct linker_set sysctl_vfs_smbfs;

#ifdef SYSCTL_DECL
SYSCTL_DECL(_vfs_smbfs);
#endif
#ifndef NetBSD
SYSCTL_INT(_vfs_smbfs, OID_AUTO, fastlookup, CTLFLAG_RW, &smbfs_fastlookup, 0, "");
#endif


#define DE_SIZE	(sizeof(struct dirent))

static int
smbfs_readvdir(struct vnode *vp, struct uio *uio, struct ucred *cred)
{
	struct dirent de;
	struct componentname cn;
	struct smb_cred scred;
	struct smbfs_fctx *ctx;
	struct vnode *newvp;
	struct smbnode *np = VTOSMB(vp);
	int error/*, *eofflag = ap->a_eofflag*/;
	long offset, limit;
	const unsigned char *hcp;
	int i;

	np = VTOSMB(vp);
	SMBVDEBUG("dirname='%s'\n", np->n_name);
	if (uio->uio_resid < DE_SIZE || uio->uio_offset < 0)
		return EINVAL;
	smb_makescred(&scred, uio->uio_procp, cred);
	offset = uio->uio_offset / DE_SIZE; 	/* offset in the directory */
	limit = uio->uio_resid / DE_SIZE;
	while (limit && offset < 2) {
		limit--;
		bzero((caddr_t)&de, DE_SIZE);
		de.d_reclen = DE_SIZE;
		de.d_fileno = (offset == 0) ? np->n_ino :
		    (np->n_parent ? np->n_parent->n_ino : 2);
		if (de.d_fileno == 0)
			de.d_fileno = 0x7ffffffd + offset;
		de.d_namlen = offset + 1;
		de.d_name[0] = '.';
		de.d_name[1] = '.';
		de.d_name[offset + 1] = '\0';
		de.d_type = DT_DIR;
		error = uiomove((caddr_t)&de, DE_SIZE, uio);
		if (error)
			return error;
		offset++;
		uio->uio_offset += DE_SIZE;
	}
	if (limit == 0)
		return 0;
	if (offset != np->n_dirofs || np->n_dirseq == NULL) {
		SMBVDEBUG("Reopening search %ld:%ld\n", offset, np->n_dirofs);
		if (np->n_dirseq) {
			smbfs_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
		}
		np->n_dirofs = 2;
		error = smbfs_findopen(np, "*", 1,
		    SMB_FA_SYSTEM | SMB_FA_HIDDEN | SMB_FA_DIR,
		    &scred, &ctx);
		if (error) {
			SMBVDEBUG("can not open search, error = %d", error);
			return error;
		}
		np->n_dirseq = ctx;
	} else
		ctx = np->n_dirseq;
	while (np->n_dirofs < offset) {
		error = smbfs_findnext(ctx, offset - np->n_dirofs++, &scred);
		if (error) {
			smbfs_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
			return error == ENOENT ? 0 : error;
		}
	}
	error = 0;
	for (; limit; limit--, offset++) {
		bzero((caddr_t)&de, DE_SIZE);
		de.d_reclen = DE_SIZE;
		error = smbfs_findnext(ctx, limit, &scred);
		if (error)
			break;
		np->n_dirofs++;
		de.d_fileno = ctx->f_attr.fa_ino;
		de.d_type = (ctx->f_attr.fa_attr & SMB_FA_DIR) ? DT_DIR : DT_REG;
		de.d_namlen = ctx->f_nmlen;
		bcopy(ctx->f_name, de.d_name, de.d_namlen);
		de.d_name[de.d_namlen] = '\0';
		if (smbfs_fastlookup && de.d_namlen <= NCHNAMLEN) {
			error = smbfs_nget(vp->v_mount, vp, ctx->f_name,
			    ctx->f_nmlen, &ctx->f_attr, &newvp);
			if (!error) {
				cn.cn_nameptr = de.d_name;
				cn.cn_namelen = de.d_namlen;
				cn.cn_hash = 0;
				for (hcp = cn.cn_nameptr, i = 1;
                                     i <= cn.cn_namelen;
				     i++, hcp++)
					cn.cn_hash += *hcp * i;
		    		cache_enter(vp, newvp, &cn);
				vput(newvp);
			}
		}
		error = uiomove((caddr_t)&de, DE_SIZE, uio);
		if (error)
			break;
	}
	if (error == ENOENT)
		error = 0;
	uio->uio_offset = offset * DE_SIZE;
	return error;
}

int
smbfs_readvnode(struct vnode *vp, struct uio *uiop, struct ucred *cred) {
	struct smbmount *smp = VFSTOSMBFS(vp->v_mount);
	struct smbnode *np = VTOSMB(vp);
	struct proc *p;
	struct vattr vattr;
	struct smb_cred scred;
	int error;

	if (vp->v_type != VREG && vp->v_type != VDIR) {
		SMBFSERR("vn types other than VREG or VDIR are unsupported !\n");
		return EIO;
	}
	if (uiop->uio_resid == 0)
		return 0;
	if (uiop->uio_offset < 0)
		return EINVAL;
/*	if (uiop->uio_offset + uiop->uio_resid > smp->nm_maxfilesize)
		return EFBIG;*/
	if (vp->v_type == VDIR) {
		error = simple_spinlock(&np->n_rdirlock, 0);
		if (error)
			return error;
		error = smbfs_readvdir(vp, uiop, cred);
		simple_spinunlock(&np->n_rdirlock);
		return error;
	}

	p = uiop->uio_procp;
/*	biosize = SSTOCN(smp->sm_share)->sc_txmax;*/
	if (np->n_flag & NMODIFIED) {
		smbfs_attr_cacheremove(vp);
		error = VOP_GETATTR(vp, &vattr, cred, p);
		if (error)
			return error;
		np->n_mtime.tv_sec = vattr.va_mtime.tv_sec;
	} else {
		error = VOP_GETATTR(vp, &vattr, cred, p);
		if (error)
			return error;
		if (np->n_mtime.tv_sec != vattr.va_mtime.tv_sec) {
			error = smbfs_vinvalbuf(vp, V_SAVE, cred, p, 1);
			if (error)
				return error;
			np->n_mtime.tv_sec = vattr.va_mtime.tv_sec;
		}
	}
	smb_makescred(&scred, p, cred);
	return smb_read(smp->sm_share, np->n_fid, uiop, &scred);
}

int
smbfs_writevnode(struct vnode *vp, struct uio *uiop,
	struct ucred *cred, int ioflag)
{
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	struct proc *p;
	int error = 0;

	if (vp->v_type != VREG) {
		SMBERROR("vn types other than VREG unsupported !\n");
		return EIO;
	}
	SMBVDEBUG("ofs=%d,resid=%d\n",(int)uiop->uio_offset, uiop->uio_resid);
	if (uiop->uio_offset < 0)
		return EINVAL;
/*	if (uiop->uio_offset + uiop->uio_resid > smp->nm_maxfilesize)
		return (EFBIG);*/
	p = uiop->uio_procp;
	if (ioflag & (IO_APPEND | IO_SYNC)) {
		if (np->n_flag & NMODIFIED) {
			smbfs_attr_cacheremove(vp);
			error = smbfs_vinvalbuf(vp, V_SAVE, cred, p, 1);
			if (error)
				return error;
		}
		if (ioflag & IO_APPEND) {
#if notyet
			/*
			 * File size can be changed by another client
			 */
			smbfs_attr_cacheremove(vp);
			error = VOP_GETATTR(vp, &vattr, cred, p);
			if (error) return (error);
#endif
			uiop->uio_offset = np->n_size;
		}
	}
	if (uiop->uio_resid == 0)
		return 0;
	if (p && uiop->uio_offset + uiop->uio_resid > p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		psignal(p, SIGXFSZ);
		return EFBIG;
	}
	smb_makescred(&scred, p, cred);
	error = smb_write(smp->sm_share, np->n_fid, uiop, &scred);
	SMBVDEBUG("after: ofs=%d,resid=%d\n",(int)uiop->uio_offset, uiop->uio_resid);
	if (!error) {
		if (uiop->uio_offset > np->n_size) {
			np->n_size = uiop->uio_offset;
#ifndef NetBSD
			vnode_pager_setsize(vp, np->n_size);
#else
			uvm_vnp_setsize(vp, np->n_size);
#endif
		}
	}
	return error;
}

/*
 * Do an I/O operation to/from a cache block.
 */
int
smbfs_doio(struct buf *bp, struct ucred *cr, struct proc *p)
{
	struct vnode *vp = bp->b_vp;
	struct smbmount *smp = VFSTOSMBFS(vp->v_mount);
	struct smbnode *np = VTOSMB(vp);
	struct uio uio, *uiop = &uio;
	struct iovec io;
	struct smb_cred scred;
	int error = 0;

	uiop->uio_iov = &io;
	uiop->uio_iovcnt = 1;
	uiop->uio_segflg = UIO_SYSSPACE;
	uiop->uio_procp = p;

	smb_makescred(&scred, p, cr);

#ifdef FB_CURRENT
	if (bp->b_iocmd == BIO_READ) {
#else
	if (bp->b_flags & B_READ) {
#endif
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    io.iov_base = bp->b_data;
	    uiop->uio_rw = UIO_READ;
	    switch (vp->v_type) {
	      case VREG:
		uiop->uio_offset = ((off_t)bp->b_blkno) << DEV_BSHIFT;
		error = smb_read(smp->sm_share, np->n_fid, uiop, &scred);
		if (error)
			break;
		if (uiop->uio_resid) {
			int left = uiop->uio_resid;
			int nread = bp->b_bcount - left;
			if (left > 0)
			    bzero((char *)bp->b_data + nread, left);
		}
		break;
/*	    case VDIR:
		nfsstats.readdir_bios++;
		uiop->uio_offset = ((u_quad_t)bp->b_lblkno) * NFS_DIRBLKSIZ;
		if (smp->nm_flag & NFSMNT_RDIRPLUS) {
			error = nfs_readdirplusrpc(vp, uiop, cr);
			if (error == NFSERR_NOTSUPP)
				smp->nm_flag &= ~NFSMNT_RDIRPLUS;
		}
		if ((smp->nm_flag & NFSMNT_RDIRPLUS) == 0)
			error = nfs_readdirrpc(vp, uiop, cr);
		if (error == 0 && uiop->uio_resid == bp->b_bcount)
			bp->b_ioflags |= BIO_INVAL;
		break;
*/
	    default:
		printf("smbfs_doio:  type %x unexpected\n",vp->v_type);
		break;
	    };
	    if (error) {
		bp->b_error = error;
#ifdef FB_CURRENT
		bp->b_ioflags |= BIO_ERROR;
#else
		bp->b_flags |= B_ERROR;
#endif
	    }
	} else { /* write */
	    io.iov_base = bp->b_data;
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    uiop->uio_offset = (((off_t)bp->b_blkno) << DEV_BSHIFT);
	    uiop->uio_rw = UIO_WRITE;
	    /* bp->b_flags |= B_WRITEINPROG; */
	    error = smb_write(smp->sm_share, np->n_fid, uiop, &scred);
	    /* bp->b_flags &= ~B_WRITEINPROG; */
	}

	bp->b_resid = uiop->uio_resid;
	bufdone(bp);
	return error;
}

#ifndef NetBSD
/*
 * Vnode op for VM getpages.
 * Wish wish .... get rid from multiple IO routines
 */
int
smbfs_getpages(ap)
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_reqpage;
		vm_ooffset_t a_offset;
	} */ *ap;
{
#ifdef SMBFS_RWGENERIC
	return vnode_pager_generic_getpages(ap->a_vp, ap->a_m, ap->a_count,
		ap->a_reqpage);
#else
	int i, error, nextoff, size, toff, npages, count;
	struct uio uio;
	struct iovec iov;
	vm_offset_t kva;
	struct buf *bp;
	struct vnode *vp;
	struct proc *p;
	struct ucred *cred;
	struct smbmount *smp;
	struct smbnode *np;
	struct smb_cred scred;
	vm_page_t *pages;

	vp = ap->a_vp;
	p = curproc;				/* XXX */
	cred = curproc->p_ucred;		/* XXX */
	np = VTOSMB(vp);
	smp = VFSTOSMBFS(vp->v_mount);
	pages = ap->a_m;
	count = ap->a_count;

	if (vp->v_object == NULL) {
		printf("smbfs_getpages: called with non-merged cache vnode??\n");
		return VM_PAGER_ERROR;
	}
	smb_makescred(&scred, p, cred);

#if __FreeBSD_version >= 400000
	bp = getpbuf(&smbfs_pbuf_freecnt);
#else
	bp = getpbuf();
#endif
	npages = btoc(count);
	kva = (vm_offset_t) bp->b_data;
	pmap_qenter(kva, pages, npages);

	iov.iov_base = (caddr_t) kva;
	iov.iov_len = count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = IDX_TO_OFF(pages[0]->pindex);
	uio.uio_resid = count;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = p;

	error = smb_read(smp->sm_share, np->n_fid, &uio, &scred);
	pmap_qremove(kva, npages);

#if __FreeBSD_version >= 400000
	relpbuf(bp, &smbfs_pbuf_freecnt);
#else
	relpbuf(bp);
#endif

	if (error && (uio.uio_resid == count)) {
		printf("smbfs_getpages: error %d\n",error);
		for (i = 0; i < npages; i++) {
			if (ap->a_reqpage != i)
				vnode_pager_freepage(pages[i]);
		}
		return VM_PAGER_ERROR;
	}

	size = count - uio.uio_resid;

	for (i = 0, toff = 0; i < npages; i++, toff = nextoff) {
		vm_page_t m;
		nextoff = toff + PAGE_SIZE;
		m = pages[i];

		m->flags &= ~PG_ZERO;

		if (nextoff <= size) {
			m->valid = VM_PAGE_BITS_ALL;
			m->dirty = 0;
		} else {
			int nvalid = ((size + DEV_BSIZE - 1) - toff) & ~(DEV_BSIZE - 1);
			vm_page_set_validclean(m, 0, nvalid);
		}
		
		if (i != ap->a_reqpage) {
			/*
			 * Whether or not to leave the page activated is up in
			 * the air, but we should put the page on a page queue
			 * somewhere (it already is in the object).  Result:
			 * It appears that emperical results show that
			 * deactivating pages is best.
			 */

			/*
			 * Just in case someone was asking for this page we
			 * now tell them that it is ok to use.
			 */
			if (!error) {
				if (m->flags & PG_WANTED)
					vm_page_activate(m);
				else
					vm_page_deactivate(m);
				vm_page_wakeup(m);
			} else {
				vnode_pager_freepage(m);
			}
		}
	}
	return 0;
#endif /* SMBFS_RWGENERIC */
}
#endif

#ifndef NetBSD
/*
 * Vnode op for VM putpages.
 * possible bug: all IO done in sync mode
 * Note that vop_close always invalidate pages before close, so it's
 * not necessary to open vnode.
 */
int
smbfs_putpages(ap)
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_sync;
		int *a_rtvals;
		vm_ooffset_t a_offset;
	} */ *ap;
{
	int error;
	struct vnode *vp = ap->a_vp;
	struct proc *p;
	struct ucred *cred;

#ifdef SMBFS_RWGENERIC
	p = curproc;			/* XXX */
	cred = p->p_ucred;		/* XXX */
	VOP_OPEN(vp, FWRITE, cred, p);
	error = vnode_pager_generic_putpages(ap->a_vp, ap->a_m, ap->a_count,
		ap->a_sync, ap->a_rtvals);
	VOP_CLOSE(vp, FWRITE, cred, p);
	return error;
#else
	struct uio uio;
	struct iovec iov;
	vm_offset_t kva;
	struct buf *bp;
	int i, npages, count;
	int *rtvals;
	struct smbmount *smp;
	struct smbnode *np;
	struct smb_cred scred;
	vm_page_t *pages;

	p = curproc;			/* XXX */
	cred = p->p_ucred;		/* XXX */
/*	VOP_OPEN(vp, FWRITE, cred, p);*/
	np = VTOSMB(vp);
	smp = VFSTOSMBFS(vp->v_mount);
	pages = ap->a_m;
	count = ap->a_count;
	rtvals = ap->a_rtvals;
	npages = btoc(count);

	for (i = 0; i < npages; i++) {
		rtvals[i] = VM_PAGER_AGAIN;
	}

#if __FreeBSD_version >= 400000
	bp = getpbuf(&smbfs_pbuf_freecnt);
#else
	bp = getpbuf();
#endif
	kva = (vm_offset_t) bp->b_data;
	pmap_qenter(kva, pages, npages);

	iov.iov_base = (caddr_t) kva;
	iov.iov_len = count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = IDX_TO_OFF(pages[0]->pindex);
	uio.uio_resid = count;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_procp = p;
	SMBVDEBUG("ofs=%d,resid=%d\n",(int)uio.uio_offset, uio.uio_resid);

	smb_makescred(&scred, p, cred);
	error = smb_write(smp->sm_share, np->n_fid, &uio, &scred);
/*	VOP_CLOSE(vp, FWRITE, cred, p);*/
	SMBVDEBUG("paged write done: %d\n", error);

	pmap_qremove(kva, npages);
#if __FreeBSD_version >= 400000
	relpbuf(bp, &smbfs_pbuf_freecnt);
#else
	relpbuf(bp);
#endif

	if (!error) {
		int nwritten = round_page(count - uio.uio_resid) / PAGE_SIZE;
		for (i = 0; i < nwritten; i++) {
			rtvals[i] = VM_PAGER_OK;
			pages[i]->dirty = 0;
		}
	}
	return rtvals[0];
#endif /* SMBFS_RWGENERIC */
}
#endif

/*
 * Flush and invalidate all dirty buffers. If another process is already
 * doing the flush, just wait for completion.
 */
int
smbfs_vinvalbuf(vp, flags, cred, p, intrflg)
	struct vnode *vp;
	int flags;
	struct ucred *cred;
	struct proc *p;
	int intrflg;
{
	register struct smbnode *np = VTOSMB(vp);
	int error = 0, slpflag, slptimeo;

	if (vp->v_flag & VXLOCK)
		return 0;
	if (intrflg) {
		slpflag = PCATCH;
		slptimeo = 2 * hz;
	} else {
		slpflag = 0;
		slptimeo = 0;
	}
	while (np->n_flag & NFLUSHINPROG) {
		np->n_flag |= NFLUSHWANT;
		error = tsleep((caddr_t)&np->n_flag, PRIBIO + 2, "smfsvinv", slptimeo);
		error = smb_proc_intr(p);
		if (error == EINTR && intrflg)
			return EINTR;
	}
	np->n_flag |= NFLUSHINPROG;
	error = vinvalbuf(vp, flags, cred, p, slpflag, 0);
	while (error) {
		if (intrflg && (error == ERESTART || error == EINTR)) {
			np->n_flag &= ~NFLUSHINPROG;
			if (np->n_flag & NFLUSHWANT) {
				np->n_flag &= ~NFLUSHWANT;
				wakeup((caddr_t)&np->n_flag);
			}
			return EINTR;
		}
		error = vinvalbuf(vp, flags, cred, p, slpflag, 0);
	}
	np->n_flag &= ~(NMODIFIED | NFLUSHINPROG);
	if (np->n_flag & NFLUSHWANT) {
		np->n_flag &= ~NFLUSHWANT;
		wakeup((caddr_t)&np->n_flag);
	}
	return (error);
}
