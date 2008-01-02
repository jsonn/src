/*	$NetBSD: nfs_node.c,v 1.96.6.1 2008/01/02 21:57:42 bouyer Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_node.c,v 1.96.6.1 2008/01/02 21:57:42 bouyer Exp $");

#include "opt_nfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/lock.h>
#include <sys/hash.h>
#include <sys/kauth.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>
#include <nfs/nfs_var.h>

struct nfsnodehashhead *nfsnodehashtbl;
u_long nfsnodehash;
struct lock nfs_hashlock;

POOL_INIT(nfs_node_pool, sizeof(struct nfsnode), 0, 0, 0, "nfsnodepl",
    &pool_allocator_nointr, IPL_NONE);
POOL_INIT(nfs_vattr_pool, sizeof(struct vattr), 0, 0, 0, "nfsvapl",
    &pool_allocator_nointr, IPL_NONE);

MALLOC_DEFINE(M_NFSNODE, "NFS node", "NFS vnode private part");

extern int prtactive;

#define	nfs_hash(x,y)	hash32_buf((x), (y), HASH32_BUF_INIT)

void nfs_gop_size(struct vnode *, off_t, off_t *, int);
int nfs_gop_alloc(struct vnode *, off_t, off_t, int, kauth_cred_t);
int nfs_gop_write(struct vnode *, struct vm_page **, int, int);

static const struct genfs_ops nfs_genfsops = {
	.gop_size = nfs_gop_size,
	.gop_alloc = nfs_gop_alloc,
	.gop_write = nfs_gop_write,
};

/*
 * Initialize hash links for nfsnodes
 * and build nfsnode free list.
 */
void
nfs_nhinit()
{

	nfsnodehashtbl = hashinit(desiredvnodes, HASH_LIST, M_NFSNODE,
	    M_WAITOK, &nfsnodehash);
	lockinit(&nfs_hashlock, PINOD, "nfs_hashlock", 0, 0);
}

/*
 * Reinitialize inode hash table.
 */

void
nfs_nhreinit()
{
	struct nfsnode *np;
	struct nfsnodehashhead *oldhash, *hash;
	u_long oldmask, mask, val;
	int i;

	hash = hashinit(desiredvnodes, HASH_LIST, M_NFSNODE, M_WAITOK,
	    &mask);

	lockmgr(&nfs_hashlock, LK_EXCLUSIVE, NULL);
	oldhash = nfsnodehashtbl;
	oldmask = nfsnodehash;
	nfsnodehashtbl = hash;
	nfsnodehash = mask;
	for (i = 0; i <= oldmask; i++) {
		while ((np = LIST_FIRST(&oldhash[i])) != NULL) {
			LIST_REMOVE(np, n_hash);
			val = NFSNOHASH(nfs_hash(np->n_fhp, np->n_fhsize));
			LIST_INSERT_HEAD(&hash[val], np, n_hash);
		}
	}
	lockmgr(&nfs_hashlock, LK_RELEASE, NULL);
	hashdone(oldhash, M_NFSNODE);
}

/*
 * Free resources previoslu allocated in nfs_nhinit().
 */
void
nfs_nhdone()
{
	hashdone(nfsnodehashtbl, M_NFSNODE);
	pool_destroy(&nfs_node_pool);
	pool_destroy(&nfs_vattr_pool);
}

/*
 * Look up a vnode/nfsnode by file handle.
 * Callers must check for mount points!!
 * In all cases, a pointer to a
 * nfsnode structure is returned.
 */
int
nfs_nget1(mntp, fhp, fhsize, npp, lkflags)
	struct mount *mntp;
	nfsfh_t *fhp;
	int fhsize;
	struct nfsnode **npp;
	int lkflags;
{
	struct nfsnode *np;
	struct nfsnodehashhead *nhpp;
	struct vnode *vp;
	int error;

	nhpp = &nfsnodehashtbl[NFSNOHASH(nfs_hash(fhp, fhsize))];
loop:
	LIST_FOREACH(np, nhpp, n_hash) {
		if (mntp != NFSTOV(np)->v_mount || np->n_fhsize != fhsize ||
		    memcmp(fhp, np->n_fhp, fhsize))
			continue;
		vp = NFSTOV(np);
		error = vget(vp, LK_EXCLUSIVE | lkflags);
		if (error == EBUSY)
			return error;
		if (error)
			goto loop;
		*npp = np;
		return(0);
	}
	if (lockmgr(&nfs_hashlock, LK_EXCLUSIVE|LK_SLEEPFAIL, 0))
		goto loop;
	error = getnewvnode(VT_NFS, mntp, nfsv2_vnodeop_p, &vp);
	if (error) {
		*npp = 0;
		lockmgr(&nfs_hashlock, LK_RELEASE, 0);
		return (error);
	}
	np = pool_get(&nfs_node_pool, PR_WAITOK);
	memset(np, 0, sizeof *np);
	vp->v_data = np;
	np->n_vnode = vp;
	genfs_node_init(vp, &nfs_genfsops);

	/*
	 * Insert the nfsnode in the hash queue for its new file handle
	 */

	LIST_INSERT_HEAD(nhpp, np, n_hash);
	if (fhsize > NFS_SMALLFH) {
		np->n_fhp = kmem_alloc(fhsize, KM_SLEEP);
	} else
		np->n_fhp = &np->n_fh;
	memcpy(np->n_fhp, fhp, fhsize);
	np->n_fhsize = fhsize;
	np->n_accstamp = -1;
	np->n_vattr = pool_get(&nfs_vattr_pool, PR_WAITOK);

	/*
	 * Initalize read/write creds to useful values. VOP_OPEN will
	 * overwrite these.
	 */
	np->n_rcred = curlwp->l_cred;
	kauth_cred_hold(np->n_rcred);
	np->n_wcred = curlwp->l_cred;
	kauth_cred_hold(np->n_wcred);
	lockmgr(&vp->v_lock, LK_EXCLUSIVE, NULL);
	lockmgr(&nfs_hashlock, LK_RELEASE, NULL);
	NFS_INVALIDATE_ATTRCACHE(np);
	uvm_vnp_setsize(vp, 0);
	*npp = np;
	return (0);
}

int
nfs_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		bool *a_recycle;
	} */ *ap = v;
	struct nfsnode *np;
	struct sillyrename *sp;
	struct vnode *vp = ap->a_vp;

	np = VTONFS(vp);
	if (prtactive && vp->v_usecount != 0)
		vprint("nfs_inactive: pushing active", vp);
	if (vp->v_type != VDIR) {
		sp = np->n_sillyrename;
		np->n_sillyrename = (struct sillyrename *)0;
	} else
		sp = NULL;
	if (sp != NULL)
		nfs_vinvalbuf(vp, 0, sp->s_cred, curlwp, 1);
	*ap->a_recycle = (np->n_flag & NREMOVED) != 0;
	np->n_flag &=
	    (NMODIFIED | NFLUSHINPROG | NFLUSHWANT | NEOFVALID | NTRUNCDELAYED);

	if (vp->v_type == VDIR && np->n_dircache)
		nfs_invaldircache(vp,
		    NFS_INVALDIRCACHE_FORCE | NFS_INVALDIRCACHE_KEEPEOF);

	VOP_UNLOCK(vp, 0);

	if (sp != NULL) {
		int error;

		/*
		 * Remove the silly file that was rename'd earlier
		 *
		 * Just in case our thread also has the parent node locked,
		 * we use LK_CANRECURSE.
		 */

		error = vn_lock(sp->s_dvp, LK_EXCLUSIVE | LK_CANRECURSE);
		if (error || sp->s_dvp->v_data == NULL) {
			/* XXX should recover */
			printf("%s: vp=%p error=%d\n",
			    __func__, sp->s_dvp, error);
		} else {
			nfs_removeit(sp);
		}
		kauth_cred_free(sp->s_cred);
		vput(sp->s_dvp);
		kmem_free(sp, sizeof(*sp));
	}

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
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);

	if (prtactive && vp->v_usecount != 0)
		vprint("nfs_reclaim: pushing active", vp);

	LIST_REMOVE(np, n_hash);

	/*
	 * Free up any directory cookie structures and
	 * large file handle structures that might be associated with
	 * this nfs node.
	 */
	if (vp->v_type == VDIR && np->n_dircache)
		hashdone(np->n_dircache, M_NFSDIROFF);
	KASSERT(np->n_dirgens == NULL);

	if (np->n_fhsize > NFS_SMALLFH)
		kmem_free(np->n_fhp, np->n_fhsize);

	pool_put(&nfs_vattr_pool, np->n_vattr);
	if (np->n_rcred)
		kauth_cred_free(np->n_rcred);

	if (np->n_wcred)
		kauth_cred_free(np->n_wcred);

	cache_purge(vp);
	if (vp->v_type == VREG) {
		mutex_destroy(&np->n_commitlock);
	}
	genfs_node_destroy(vp);
	pool_put(&nfs_node_pool, np);
	vp->v_data = NULL;
	return (0);
}

void
nfs_gop_size(struct vnode *vp, off_t size, off_t *eobp, int flags)
{

	*eobp = MAX(size, vp->v_size);
}

int
nfs_gop_alloc(struct vnode *vp, off_t off, off_t len, int flags,
    kauth_cred_t cred)
{

	return 0;
}

int
nfs_gop_write(struct vnode *vp, struct vm_page **pgs, int npages, int flags)
{
	int i;

	for (i = 0; i < npages; i++) {
		pmap_page_protect(pgs[i], VM_PROT_READ);
	}
	return genfs_gop_write(vp, pgs, npages, flags);
}
