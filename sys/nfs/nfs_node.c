/*	$NetBSD: nfs_node.c,v 1.17.4.1 1997/03/12 21:24:52 is Exp $	*/

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
 *	@(#)nfs_node.c	8.6 (Berkeley) 5/22/95
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>
#include <nfs/nqnfs.h>
#include <nfs/nfs_var.h>

LIST_HEAD(nfsnodehashhead, nfsnode) *nfsnodehashtbl;
u_long nfsnodehash;

#define TRUE	1
#define	FALSE	0

/*
 * Initialize hash links for nfsnodes
 * and build nfsnode free list.
 */
void
nfs_nhinit()
{

	nfsnodehashtbl = hashinit(desiredvnodes, M_NFSNODE, &nfsnodehash);
}

/*
 * Compute an entry in the NFS hash table structure
 */
u_long
nfs_hash(fhp, fhsize)
	register nfsfh_t *fhp;
	int fhsize;
{
	register u_char *fhpp;
	register u_long fhsum;
	register int i;

	fhpp = &fhp->fh_bytes[0];
	fhsum = 0;
	for (i = 0; i < fhsize; i++)
		fhsum += *fhpp++;
	return (fhsum);
}

/*
 * Look up a vnode/nfsnode by file handle.
 * Callers must check for mount points!!
 * In all cases, a pointer to a
 * nfsnode structure is returned.
 */
int
nfs_nget(mntp, fhp, fhsize, npp)
	struct mount *mntp;
	register nfsfh_t *fhp;
	int fhsize;
	struct nfsnode **npp;
{
#ifdef Lite2_integrated
	struct proc *p = curproc;	/* XXX */
#endif
	register struct nfsnode *np;
	struct nfsnodehashhead *nhpp;
	register struct vnode *vp;
	extern int (**nfsv2_vnodeop_p)__P((void *));
	struct vnode *nvp;
	int error;

	nhpp = NFSNOHASH(nfs_hash(fhp, fhsize));
loop:
	for (np = nhpp->lh_first; np != 0; np = np->n_hash.le_next) {
		if (mntp != NFSTOV(np)->v_mount || np->n_fhsize != fhsize ||
		    bcmp((caddr_t)fhp, (caddr_t)np->n_fhp, fhsize))
			continue;
		vp = NFSTOV(np);
#ifdef Lite2_integrated
		if (vget(vp, LK_EXCLUSIVE, p))
#else
		if (vget(vp, 1))
#endif
			goto loop;
		*npp = np;
		return(0);
	}
	error = getnewvnode(VT_NFS, mntp, nfsv2_vnodeop_p, &nvp);
	if (error) {
		*npp = 0;
		return (error);
	}
	vp = nvp;
	MALLOC(np, struct nfsnode *, sizeof *np, M_NFSNODE, M_WAITOK);
	bzero((caddr_t)np, sizeof *np);
	vp->v_data = np;
	np->n_vnode = vp;
	/*
	 * Insert the nfsnode in the hash queue for its new file handle
	 */
	LIST_INSERT_HEAD(nhpp, np, n_hash);
	if (fhsize > NFS_SMALLFH) {
		MALLOC(np->n_fhp, nfsfh_t *, fhsize, M_NFSBIGFH, M_WAITOK);
	} else
		np->n_fhp = &np->n_fh;
	bcopy((caddr_t)fhp, (caddr_t)np->n_fhp, fhsize);
	np->n_fhsize = fhsize;
	*npp = np;
	return (0);
}

int
nfs_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
#ifdef Lite2_integrated
		struct proc *a_p;
#endif
	} */ *ap = v;
	register struct nfsnode *np;
	register struct sillyrename *sp;
	struct proc *p = curproc;	/* XXX */
	extern int prtactive;

	np = VTONFS(ap->a_vp);
	if (prtactive && ap->a_vp->v_usecount != 0)
		vprint("nfs_inactive: pushing active", ap->a_vp);
	if (ap->a_vp->v_type != VDIR) {
		sp = np->n_sillyrename;
		np->n_sillyrename = (struct sillyrename *)0;
	} else
		sp = (struct sillyrename *)0;
#ifdef Lite2_integrated
	VOP_UNLOCK(ap->a_vp, 0, ap->a_p);
#else
	VOP_UNLOCK(ap->a_vp);
#endif
	if (sp) {
		/*
		 * If the usecount is greater than zero, then we are
		 * being inactivated by a forcible unmount and do not
		 * have to get our own reference. In the normal case,
		 * we need a reference to keep the vnode from being
		 * recycled by getnewvnode while we do the I/O
		 * associated with discarding the buffers.
		 */
		if (ap->a_vp->v_usecount > 0)
			(void) nfs_vinvalbuf(ap->a_vp, 0, sp->s_cred, p, 1);
#ifdef Lite2_integrated
		else if (vget(ap->a_vp, 0, p))
#else
		else if (vget(ap->a_vp, 0))
#endif
                        panic("nfs_inactive: lost vnode");
		else {
			(void) nfs_vinvalbuf(ap->a_vp, 0, sp->s_cred, p, 1);
			vrele(ap->a_vp);
		}


		/*
		 * Remove the silly file that was rename'd earlier
		 */
		nfs_removeit(sp);
		crfree(sp->s_cred);
		vrele(sp->s_dvp);
		FREE((caddr_t)sp, M_NFSREQ);
	}
	np->n_flag &= (NMODIFIED | NFLUSHINPROG | NFLUSHWANT | NQNFSEVICTED |
		NQNFSNONCACHE | NQNFSWRITE);
	return (0);
}

/*
 * Reclaim an nfsnode so that it can be used for other purposes.
 */
int
nfs_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct nfsnode *np = VTONFS(vp);
	register struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	register struct nfsdmap *dp, *dp2;
	extern int prtactive;

	if (prtactive && vp->v_usecount != 0)
		vprint("nfs_reclaim: pushing active", vp);

	LIST_REMOVE(np, n_hash);

	/*
	 * For nqnfs, take it off the timer queue as required.
	 */
	if ((nmp->nm_flag & NFSMNT_NQNFS) && np->n_timer.cqe_next != 0) {
		CIRCLEQ_REMOVE(&nmp->nm_timerhead, np, n_timer);
	}

	/*
	 * Free up any directory cookie structures and
	 * large file handle structures that might be associated with
	 * this nfs node.
	 */
	if (vp->v_type == VDIR) {
		dp = np->n_cookies.lh_first;
		while (dp) {
			dp2 = dp;
			dp = dp->ndm_list.le_next;
			FREE((caddr_t)dp2, M_NFSDIROFF);
		}
	}
	if (np->n_fhsize > NFS_SMALLFH) {
		FREE((caddr_t)np->n_fhp, M_NFSBIGFH);
	}

	cache_purge(vp);
	FREE(vp->v_data, M_NFSNODE);
	vp->v_data = (void *)0;
	return (0);
}

#ifndef Lite2_integrated
/*
 * Lock an nfsnode
 */
int
nfs_lock(v)
	void *v;
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;

	/*
	 * Ugh, another place where interruptible mounts will get hung.
	 * If you make this sleep interruptible, then you have to fix all
	 * the VOP_LOCK() calls to expect interruptibility.
	 */
	while (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		(void) tsleep((caddr_t)vp, PINOD, "nfslck", 0);
	}
	if (vp->v_tag == VT_NON)
		return (ENOENT);
	return (0);
}

/*
 * Unlock an nfsnode
 */
int
nfs_unlock(v)
	void *v;
{
#if 0
	struct vop_unlock_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
#endif
	return (0);
}

/*
 * Check for a locked nfsnode
 */
int
nfs_islocked(v)
	void *v;
{
#if 0
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
#endif
	return (0);
}
#endif /* Lite2_integrated */
