/*	$NetBSD: uvm_pager.c,v 1.16.4.5 1999/07/31 19:01:33 chs Exp $	*/

/*
 *
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: Id: uvm_pager.c,v 1.1.2.23 1998/02/02 20:38:06 chuck Exp
 */

#include "opt_pmap_new.h"
#include "opt_uvmhist.h"

/*
 * uvm_pager.c: generic functions used to assist the pagers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#define UVM_PAGER
#include <uvm/uvm.h>

struct pool *uvm_aiobuf_pool;

/*
 * list of uvm pagers in the system
 */

extern struct uvm_pagerops aobj_pager;
extern struct uvm_pagerops uvm_deviceops;
extern struct uvm_pagerops uvm_vnodeops;
extern struct uvm_pagerops ubc_pager;

struct uvm_pagerops *uvmpagerops[] = {
	&aobj_pager,
	&uvm_deviceops,
	&uvm_vnodeops,
	&ubc_pager,
};

/*
 * the pager map: provides KVA for I/O
 */

#define PAGER_MAP_SIZE       (4 * 1024 * 1024)
vm_map_t pager_map;		/* XXX */
simple_lock_data_t pager_map_wanted_lock;
boolean_t pager_map_wanted;	/* locked by pager map */


/*
 * uvm_pager_init: init pagers (at boot time)
 */

void
uvm_pager_init()
{
	int lcv;

	/*
	 * init pager map
	 */

	pager_map = uvm_km_suballoc(kernel_map, &uvm.pager_sva, &uvm.pager_eva,
	 			    PAGER_MAP_SIZE, 0, FALSE, NULL);
	simple_lock_init(&pager_map_wanted_lock);
	pager_map_wanted = FALSE;

	/*
	 * init ASYNC I/O queue
	 */
	
	TAILQ_INIT(&uvm.aio_done);

	/*
	 * call pager init functions
	 */
	for (lcv = 0 ; lcv < sizeof(uvmpagerops)/sizeof(struct uvm_pagerops *);
	    lcv++) {
		if (uvmpagerops[lcv]->pgo_init)
			uvmpagerops[lcv]->pgo_init();
	}
}

/*
 * uvm_pagermapin: map pages into KVA (pager_map) for I/O that needs mappings
 *
 * we basically just map in a blank map entry to reserve the space in the
 * map and then use pmap_enter() to put the mappings in by hand.
 *
 * XXX It would be nice to know the direction of the I/O, so that we can
 * XXX map only what is necessary.
 */

vaddr_t
uvm_pagermapin(pps, npages, waitf)
	struct vm_page **pps;
	int npages;
	int waitf;
{
	vsize_t size;
	vaddr_t kva;
	vaddr_t cva;
	struct vm_page *pp;
	UVMHIST_FUNC("uvm_pagermapin"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"(pps=0x%x, npages=%d, waitf=%d)",
	      pps, npages, waitf, 0);

ReStart:
	size = npages << PAGE_SHIFT;
	kva = NULL;			/* let system choose VA */

	if (uvm_map(pager_map, &kva, size, NULL, 
	      UVM_UNKNOWN_OFFSET, UVM_FLAG_NOMERGE) != KERN_SUCCESS) {
		if (waitf == M_NOWAIT) {
			UVMHIST_LOG(maphist,"<- NOWAIT failed", 0,0,0,0);
			return(0);
		}
		simple_lock(&pager_map_wanted_lock);
		pager_map_wanted = TRUE; 
		UVMHIST_LOG(maphist, "  SLEEPING on pager_map",0,0,0,0);
		UVM_UNLOCK_AND_WAIT(pager_map, &pager_map_wanted_lock, FALSE, 
		    "pager_map",0);
		goto ReStart;
	}

	/* got it */
	for (cva = kva ; size != 0 ; size -= PAGE_SIZE, cva += PAGE_SIZE) {
		pp = *pps++;
#ifdef DEBUG
		if ((pp->flags & PG_BUSY) == 0)
			panic("uvm_pagermapin: page not busy");
#endif

		/*
		 * XXX VM_PROT_DEFAULT includes VM_PROT_EXEC; is that
		 * XXX really necessary?  It could lead to unnecessary
		 * XXX instruction cache flushes.
		 */
		pmap_enter(vm_map_pmap(pager_map), cva, VM_PAGE_TO_PHYS(pp),
		    VM_PROT_DEFAULT, TRUE,
		    VM_PROT_READ | VM_PROT_WRITE);
	}

	UVMHIST_LOG(maphist, "<- done (KVA=0x%x)", kva,0,0,0);
	return(kva);
}

/*
 * uvm_pagermapout: remove pager_map mapping
 *
 * we remove our mappings by hand and then remove the mapping (waking
 * up anyone wanting space).
 */

void
uvm_pagermapout(kva, npages)
	vaddr_t kva;
	int npages;
{
	vsize_t size = npages << PAGE_SHIFT;
	vm_map_entry_t entries;
	UVMHIST_FUNC("uvm_pagermapout"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist, " (kva=0x%x, npages=%d)", kva, npages,0,0);

	/*
	 * duplicate uvm_unmap, but add in pager_map_wanted handling.
	 */

	vm_map_lock(pager_map);
	(void) uvm_unmap_remove(pager_map, kva, kva + size, &entries);
	simple_lock(&pager_map_wanted_lock);
	if (pager_map_wanted) {
		pager_map_wanted = FALSE;
		wakeup(pager_map);
	}
	simple_unlock(&pager_map_wanted_lock);
	vm_map_unlock(pager_map);
	pmap_remove(pmap_kernel(), kva, kva + (npages << PAGE_SHIFT));
	if (entries)
		uvm_unmap_detach(entries, 0);

	UVMHIST_LOG(maphist,"<- done",0,0,0,0);
}

/*
 * uvm_mk_pcluster
 *
 * generic "make 'pager put' cluster" function.  a pager can either
 * [1] set pgo_mk_pcluster to NULL (never cluster), [2] set it to this
 * generic function, or [3] set it to a pager specific function.
 *
 * => caller must lock object _and_ pagequeues (since we need to look
 *    at active vs. inactive bits, etc.)
 * => caller must make center page busy and write-protect it
 * => we mark all cluster pages busy for the caller
 * => the caller must unbusy all pages (and check wanted/released
 *    status if it drops the object lock)
 * => flags:
 *      PGO_ALLPAGES:  all pages in object are valid targets
 *      !PGO_ALLPAGES: use "lo" and "hi" to limit range of cluster
 *      PGO_DOACTCLUST: include active pages in cluster.
 *        NOTE: the caller should clear PG_CLEANCHK bits if PGO_DOACTCLUST.
 *              PG_CLEANCHK is only a hint, but clearing will help reduce
 *		the number of calls we make to the pmap layer.
 */

struct vm_page **
uvm_mk_pcluster(uobj, pps, npages, center, flags, mlo, mhi)
	struct uvm_object *uobj;	/* IN */
	struct vm_page **pps, *center;  /* IN/OUT, IN */
	int *npages, flags;		/* IN/OUT, IN */
	vaddr_t mlo, mhi;		/* IN (if !PGO_ALLPAGES) */
{
	struct vm_page **ppsp, *pclust;
	vaddr_t lo, hi, curoff;
	int center_idx, forward, incr;
	UVMHIST_FUNC("uvm_mk_pcluster"); UVMHIST_CALLED(maphist);

	/* 
	 * center page should already be busy and write protected.  XXX:
	 * suppose page is wired?  if we lock, then a process could
	 * fault/block on it.  if we don't lock, a process could write the
	 * pages in the middle of an I/O.  (consider an msync()).  let's
	 * lock it for now (better to delay than corrupt data?).
	 */

	/*
	 * get cluster boundaries, check sanity, and apply our limits as well.
	 */

	uobj->pgops->pgo_cluster(uobj, center->offset, &lo, &hi);
	if ((flags & PGO_ALLPAGES) == 0) {
		if (lo < mlo)
			lo = mlo;
		if (hi > mhi)
			hi = mhi;
	}
	if ((hi - lo) >> PAGE_SHIFT > *npages) { /* pps too small, bail out! */
#ifdef DIAGNOSTIC
		printf("uvm_mk_pcluster uobj %p npages %d lo 0x%lx hi 0x%lx flags 0x%x\n",
		       uobj, *npages, lo, hi, flags);
#endif
		pps[0] = center;
		*npages = 1;
		return(pps);
	}

	/*
	 * now determine the center and attempt to cluster around the
	 * edges
	 */

	center_idx = (center->offset - lo) >> PAGE_SHIFT;
	pps[center_idx] = center;	/* plug in the center page */
	ppsp = &pps[center_idx];
	*npages = 1;

	/*
	 * attempt to cluster around the left [backward], and then 
	 * the right side [forward].    
	 *
	 * note that for inactive pages (pages that have been deactivated)
	 * there are no valid mappings and PG_CLEAN should be up to date.
	 * [i.e. there is no need to query the pmap with pmap_is_modified
	 * since there are no mappings].
	 */

	for (forward  = 0 ; forward <= 1 ; forward++) {
		incr = forward ? PAGE_SIZE : -PAGE_SIZE;
		curoff = center->offset + incr;
		for ( ;(forward == 0 && curoff >= lo) ||
		       (forward && curoff < hi);
		      curoff += incr) {

			pclust = uvm_pagelookup(uobj, curoff); /* lookup page */
			if (pclust == NULL) {
				break;			/* no page */
			}
			/* handle active pages */
			/* NOTE: inactive pages don't have pmap mappings */
			if ((pclust->pqflags & PQ_INACTIVE) == 0) {
				if ((flags & PGO_DOACTCLUST) == 0) {
					/* dont want mapped pages at all */
					break;
				}

				/* make sure "clean" bit is sync'd */
				if ((pclust->flags & PG_CLEANCHK) == 0) {
					if ((pclust->flags & (PG_CLEAN|PG_BUSY))
					   == PG_CLEAN &&
					   pmap_is_modified(PMAP_PGARG(pclust)))
					pclust->flags &= ~PG_CLEAN;
					/* now checked */
					pclust->flags |= PG_CLEANCHK;
				}
			}

			/* is page available for cleaning and does it need it */
			if ((pclust->flags & (PG_CLEAN|PG_BUSY)) != 0) {
				break;	/* page is already clean or is busy */
			}

			/* yes!   enroll the page in our array */
			pclust->flags |= PG_BUSY;		/* busy! */
			UVM_PAGE_OWN(pclust, "uvm_mk_pcluster");

			/* XXX: protect wired page?   see above comment. */
			pmap_page_protect(PMAP_PGARG(pclust), VM_PROT_READ);
			if (!forward) {
				ppsp--;			/* back up one page */
				*ppsp = pclust;
			} else {
				/* move forward one page */
				ppsp[*npages] = pclust;
			}
			(*npages)++;
		}
	}
	
	/*
	 * done!  return the cluster array to the caller!!!
	 */

	UVMHIST_LOG(maphist, "<- done",0,0,0,0);
	return(ppsp);
}


/*
 * uvm_shareprot: generic share protect routine
 *
 * => caller must lock map entry's map
 * => caller must lock object pointed to by map entry
 */

void
uvm_shareprot(entry, prot)
	vm_map_entry_t entry;
	vm_prot_t prot;
{
	struct uvm_object *uobj = entry->object.uvm_obj;
	struct vm_page *pp;
	vaddr_t start, stop;
	UVMHIST_FUNC("uvm_shareprot"); UVMHIST_CALLED(maphist);

	if (UVM_ET_ISSUBMAP(entry)) 
		panic("uvm_shareprot: non-object attached");

	start = entry->offset;
	stop = start + (entry->end - entry->start);

	/*
	 * traverse list of pages in object.   if page in range, pmap_prot it
	 */

	for (pp = uobj->memq.tqh_first ; pp != NULL ; pp = pp->listq.tqe_next) {
		if (pp->offset >= start && pp->offset < stop)
			pmap_page_protect(PMAP_PGARG(pp), prot);
	}
	UVMHIST_LOG(maphist, "<- done",0,0,0,0);
}

/*
 * uvm_pager_put: high level pageout routine
 *
 * we want to pageout page "pg" to backing store, clustering if
 * possible.
 *
 * => page queues must be locked by caller
 * => if page is not swap-backed, then "uobj" points to the object
 *	backing it.   this object should be locked by the caller.
 * => if page is swap-backed, then "uobj" should be NULL.
 * => "pg" should be PG_BUSY (by caller), and !PG_CLEAN
 *    for swap-backed memory, "pg" can be NULL if there is no page
 *    of interest [sometimes the case for the pagedaemon]
 * => "ppsp_ptr" should point to an array of npages vm_page pointers
 *	for possible cluster building
 * => flags (first two for non-swap-backed pages)
 *	PGO_ALLPAGES: all pages in uobj are valid targets
 *	PGO_DOACTCLUST: include "PQ_ACTIVE" pages as valid targets
 *	PGO_SYNCIO: do SYNC I/O (no async)
 *	PGO_PDFREECLUST: pagedaemon: drop cluster on successful I/O
 * => start/stop: if (uobj && !PGO_ALLPAGES) limit targets to this range
 *		  if (!uobj) start is the (daddr_t) of the starting swapblk
 * => return state:
 *	1. we return the VM_PAGER status code of the pageout
 *	2. we return with the page queues unlocked
 *	3. if (uobj != NULL) [!swap_backed] we return with
 *		uobj locked _only_ if PGO_PDFREECLUST is set 
 *		AND result != VM_PAGER_PEND.   in all other cases
 *		we return with uobj unlocked.   [this is a hack
 *		that allows the pagedaemon to save one lock/unlock
 *		pair in the !swap_backed case since we have to
 *		lock the uobj to drop the cluster anyway]
 *	4. on errors we always drop the cluster.   thus, if we return
 *		!PEND, !OK, then the caller only has to worry about
 *		un-busying the main page (not the cluster pages).
 *	5. on success, if !PGO_PDFREECLUST, we return the cluster
 *		with all pages busy (caller must un-busy and check
 *		wanted/released flags).
 */

int
uvm_pager_put(uobj, pg, ppsp_ptr, npages, flags, start, stop)
	struct uvm_object *uobj;	/* IN */
	struct vm_page *pg, ***ppsp_ptr;/* IN, IN/OUT */
	int *npages;			/* IN/OUT */
	int flags;			/* IN */
	vaddr_t start, stop;		/* IN, IN */
{
	int result;
	daddr_t swblk;
	struct vm_page **ppsp = *ppsp_ptr;

	/*
	 * note that uobj is null  if we are doing a swap-backed pageout.
	 * note that uobj is !null if we are doing normal object pageout.
	 * note that the page queues must be locked to cluster.
	 */

	if (uobj) {	/* if !swap-backed */

		/*
		 * attempt to build a cluster for pageout using its
		 * make-put-cluster function (if it has one).
		 */

		if (uobj->pgops->pgo_mk_pcluster) {
			ppsp = uobj->pgops->pgo_mk_pcluster(uobj, ppsp,
			    npages, pg, flags, start, stop);
			*ppsp_ptr = ppsp;  /* update caller's pointer */
		} else {
			ppsp[0] = pg;
			*npages = 1;
		}
					  
		swblk = 0;		/* XXX: keep gcc happy */

	} else {

		/*
		 * for swap-backed pageout, the caller (the pagedaemon) has
		 * already built the cluster for us.   the starting swap
		 * block we are writing to has been passed in as "start."
		 * "pg" could be NULL if there is no page we are especially
		 * interested in (in which case the whole cluster gets dropped
		 * in the event of an error or a sync "done").
		 */
		swblk = (daddr_t) start;
		/* ppsp and npages should be ok */
	}

	/* now that we've clustered we can unlock the page queues */
	uvm_unlock_pageq();

	/*
	 * now attempt the I/O.   if we have a failure and we are
	 * clustered, we will drop the cluster and try again.
	 */

ReTry:
	if (uobj) {
		/* object is locked */
		simple_lock_assert(&uobj->vmobjlock, SLOCK_LOCKED);
		result = uobj->pgops->pgo_put(uobj, ppsp, *npages, flags);
		/* object is now unlocked */
		simple_lock_assert(&uobj->vmobjlock, SLOCK_UNLOCKED);
	} else {
		/* nothing locked */
		result = uvm_swap_put(swblk, ppsp, *npages, flags);
		/* nothing locked */
	}

	/*
	 * we have attempted the I/O.
	 *
	 * if the I/O was a success then:
	 * 	if !PGO_PDFREECLUST, we return the cluster to the 
	 *		caller (who must un-busy all pages)
	 *	else we un-busy cluster pages for the pagedaemon
	 *
	 * if I/O is pending (async i/o) then we return the pending code.
	 * [in this case the async i/o done function must clean up when
	 *  i/o is done...]
	 */

	if (result == VM_PAGER_PEND || result == VM_PAGER_OK) {
		if (result == VM_PAGER_OK && (flags & PGO_PDFREECLUST)) {
			/*
			 * drop cluster and relock object (only if I/O is
			 * not pending)
			 */
			if (uobj)
				/* required for dropcluster */
				simple_lock(&uobj->vmobjlock);
			if (*npages > 1 || pg == NULL)
				uvm_pager_dropcluster(uobj, pg, ppsp, npages,
				    PGO_PDFREECLUST, 0);
			/* if (uobj): object still locked, as per
			 * return-state item #3 */
		}
		return (result);
	}

	/*
	 * a pager error occured.    if we have clustered, we drop the 
	 * cluster and try again.
	 */

	if (*npages > 1 || pg == NULL) {
		if (uobj)
			simple_lock(&uobj->vmobjlock);
		uvm_pager_dropcluster(uobj, pg, ppsp, npages, PGO_REALLOCSWAP,
		    swblk);
		if (pg != NULL)
			goto ReTry;
	}

	/*
	 * a pager error occured (even after dropping the cluster, if there
	 * was one).    give up!   the caller only has one page ("pg")
	 * to worry about.
	 */
	
	if (uobj && (flags & PGO_PDFREECLUST) != 0)
		simple_lock(&uobj->vmobjlock);
	return(result);
}

/*
 * uvm_pager_dropcluster: drop a cluster we have built (because we 
 * got an error, or, if PGO_PDFREECLUST we are un-busying the
 * cluster pages on behalf of the pagedaemon).
 *
 * => uobj, if non-null, is a non-swap-backed object that is 
 *	locked by the caller.   we return with this object still
 *	locked.
 * => page queues are not locked
 * => pg is our page of interest (the one we clustered around, can be null)
 * => ppsp/npages is our current cluster
 * => flags: PGO_PDFREECLUST: pageout was a success: un-busy cluster
 *	pages on behalf of the pagedaemon.
 *           PGO_REALLOCSWAP: drop previously allocated swap slots for 
 *		clustered swap-backed pages (except for "pg" if !NULL)
 *		"swblk" is the start of swap alloc (e.g. for ppsp[0])
 *		[only meaningful if swap-backed (uobj == NULL)]
 */


void
uvm_pager_dropcluster(uobj, pg, ppsp, npages, flags, swblk)
	struct uvm_object *uobj;	/* IN */
	struct vm_page *pg, **ppsp;	/* IN, IN/OUT */
	int *npages;			/* IN/OUT */
	int flags;
	int swblk;			/* valid if (uobj == NULL &&
					   PGO_REALLOCSWAP) */
{
	int lcv;
	boolean_t obj_is_alive; 
	struct uvm_object *saved_uobj;

	/*
	 * if we need to reallocate swap space for the cluster we are dropping
	 * (true if swap-backed and PGO_REALLOCSWAP) then free the old
	 * allocation now.   save a block for "pg" if it is non-NULL.
	 *
	 * note that we will zap the object's pointer to swap in the "for" loop
	 * below...
	 */

	if (uobj == NULL && (flags & PGO_REALLOCSWAP)) {
		if (pg)
			uvm_swap_free(swblk + 1, *npages - 1);
		else
			uvm_swap_free(swblk, *npages);
	}

	/*
	 * drop all pages but "pg"
	 */

	for (lcv = 0 ; lcv < *npages ; lcv++) {

		/* skip "pg" or empty slot */
		if (ppsp[lcv] == pg || ppsp[lcv] == NULL)
			continue;
	
		/*
		 * if swap-backed, gain lock on object that owns page.  note
		 * that PQ_ANON bit can't change as long as we are holding
		 * the PG_BUSY bit (so there is no need to lock the page
		 * queues to test it).
		 *
		 * once we have the lock, dispose of the pointer to swap, if
		 * requested
		 */
		if (!uobj) {
			if (ppsp[lcv]->pqflags & PQ_ANON) {
				simple_lock(&ppsp[lcv]->uanon->an_lock);
				if (flags & PGO_REALLOCSWAP)
					  /* zap swap block */
					  ppsp[lcv]->uanon->an_swslot = 0;
			} else {
				simple_lock(&ppsp[lcv]->uobject->vmobjlock);
				if (flags & PGO_REALLOCSWAP)
					uao_set_swslot(ppsp[lcv]->uobject,
					    ppsp[lcv]->offset >> PAGE_SHIFT, 0);
			}
		}

		/* did someone want the page while we had it busy-locked? */
		if (ppsp[lcv]->flags & PG_WANTED) {
			/* still holding obj lock */
			wakeup(ppsp[lcv]);
		}

		/* if page was released, release it.  otherwise un-busy it */
		if (ppsp[lcv]->flags & PG_RELEASED) {

			if (ppsp[lcv]->pqflags & PQ_ANON) {
				/* so that anfree will free */
				ppsp[lcv]->flags &= ~(PG_BUSY);
				UVM_PAGE_OWN(ppsp[lcv], NULL);

				pmap_page_protect(PMAP_PGARG(ppsp[lcv]),
				    VM_PROT_NONE); /* be safe */
				simple_unlock(&ppsp[lcv]->uanon->an_lock);
				/* kills anon and frees pg */
				uvm_anfree(ppsp[lcv]->uanon);

				continue;
			}

			/*
			 * pgo_releasepg will dump the page for us
			 */

#ifdef DIAGNOSTIC
			if (ppsp[lcv]->uobject->pgops->pgo_releasepg == NULL)
				panic("uvm_pager_dropcluster: no releasepg "
				    "function");
#endif
			saved_uobj = ppsp[lcv]->uobject;
			obj_is_alive =
			    saved_uobj->pgops->pgo_releasepg(ppsp[lcv], NULL);
			
#ifdef DIAGNOSTIC
			/* for normal objects, "pg" is still PG_BUSY by us,
			 * so obj can't die */
			if (uobj && !obj_is_alive)
				panic("uvm_pager_dropcluster: object died "
				    "with active page");
#endif
			/* only unlock the object if it is still alive...  */
			if (obj_is_alive && saved_uobj != uobj)
				simple_unlock(&saved_uobj->vmobjlock);

			/*
			 * XXXCDC: suppose uobj died in the pgo_releasepg?
			 * how pass that
			 * info up to caller.  we are currently ignoring it...
			 */

			continue;		/* next page */

		} else {
			ppsp[lcv]->flags &= ~(PG_BUSY|PG_WANTED|PG_FAKE);
			UVM_PAGE_OWN(ppsp[lcv], NULL);
		}

		/*
		 * if we are operating on behalf of the pagedaemon and we 
		 * had a successful pageout update the page!
		 */
		if (flags & PGO_PDFREECLUST) {
			pmap_clear_reference(PMAP_PGARG(ppsp[lcv]));
			pmap_clear_modify(PMAP_PGARG(ppsp[lcv]));
			ppsp[lcv]->flags |= PG_CLEAN;
		}

		/* if anonymous cluster, unlock object and move on */
		if (!uobj) {
			if (ppsp[lcv]->pqflags & PQ_ANON)
				simple_unlock(&ppsp[lcv]->uanon->an_lock);
			else
				simple_unlock(&ppsp[lcv]->uobject->vmobjlock);
		}

	}

	/*
	 * drop to a cluster of 1 page ("pg") if requested
	 */

	if (pg && (flags & PGO_PDFREECLUST) == 0) {
		/*
		 * if we are not a successful pageout, we make a 1 page cluster.
		 */
		ppsp[0] = pg;
		*npages = 1;

		/*
		 * assign new swap block to new cluster, if anon backed
		 */
		if (uobj == NULL && (flags & PGO_REALLOCSWAP)) {
			if (pg->pqflags & PQ_ANON) {
				simple_lock(&pg->uanon->an_lock);
				pg->uanon->an_swslot = swblk;	/* reassign */
				simple_unlock(&pg->uanon->an_lock);
			} else {
				simple_lock(&pg->uobject->vmobjlock);
				uao_set_swslot(pg->uobject,
				    pg->offset >> PAGE_SHIFT, swblk);
				simple_unlock(&pg->uobject->vmobjlock);
			}
		}
	}
}

/*
 * interrupt-context iodone handler for nested i/o bufs.
 *
 * => must be at splbio().
 */

void
uvm_aio_biodone1(bp)
	struct buf *bp;
{
	struct buf *mbp = bp->b_private;

#ifdef DIAGNOSTIC
	if (mbp == bp) {
		panic("uvm_aio_biodone1: mbp == bp %p", bp);
	}
#endif

	if (bp->b_flags & B_ERROR) {
		mbp->b_flags |= B_ERROR;
		mbp->b_error = bp->b_error;
	}
	mbp->b_resid -= bp->b_bcount;
	pool_put(&bufpool, bp);
	if (mbp->b_resid == 0) {
		biodone(mbp);
	}
}

/*
 * interrupt-context iodone handler for single-buf i/os
 * or the top-level buf of a nested-buf i/o.
 *
 * => must be at splbio().
 */

void
uvm_aio_biodone(bp)
	struct buf *bp;
{
	/* reset b_iodone for when this is a single-buf i/o. */
	bp->b_iodone = uvm_aio_aiodone;

	simple_lock(&uvm.aiodoned_lock);	/* locks uvm.aio_done */
	TAILQ_INSERT_TAIL(&uvm.aio_done, bp, b_freelist);
	wakeup(&uvm.aiodoned);
	simple_unlock(&uvm.aiodoned_lock);
}

/*
 * uvm_aio_aiodone: do iodone processing for async i/os.
 * this should be called in thread context, not interrupt context.
 */

void
uvm_aio_aiodone(bp)
	struct buf *bp;
{
	int npages = bp->b_bufsize >> PAGE_SHIFT;
	struct vm_page *pg, *pgs[npages];
	struct uvm_object *uobj;
	int s, i;
	boolean_t release, write, swap;
	UVMHIST_FUNC("uvm_aio_aiodone"); UVMHIST_CALLED(ubchist);
	UVMHIST_LOG(ubchist, "bp %p", bp, 0,0,0);

	release = (bp->b_flags & (B_ERROR|B_READ)) == (B_ERROR|B_READ);
	write = (bp->b_flags & B_READ) == 0;
	uobj = NULL;
	for (i = 0; i < npages; i++) {
		pgs[i] = uvm_pageratop((vaddr_t)bp->b_data + (i << PAGE_SHIFT));
	}
	uvm_pagermapout((vaddr_t)bp->b_data, npages);
	for (i = 0; i < npages; i++) {
		pg = pgs[i];

		if (i == 0) {
			swap = (pg->pqflags & PQ_SWAPBACKED) != 0;
			if (!swap) {
				uobj = pg->uobject;
				simple_lock(&uobj->vmobjlock);
			}
		}
#ifdef DIAGNOSTIC
		if (!swap && pg->uobject != uobj) {
			panic("uvm_aio_aiodone: mismatched pg %d %p uobj %p",
			      i, pg, uobj);
		}
#endif

		if (swap) {
			if (pg->pqflags & PQ_ANON) {
				simple_lock(&pg->uanon->an_lock);
			} else {
				simple_lock(&pg->uobject->vmobjlock);
			}
		}

		/*
		 * if this is a read and we got an error, mark the pages
		 * PG_RELEASED so that uvm_page_unbusy() will free them.
		 */

		if (release) {
			if (pg->pqflags & PQ_ANON) {
				pg->flags &= ~(PG_BUSY);
				UVM_PAGE_OWN(pg, NULL);
				simple_unlock(&pg->uanon->an_lock);
				uvm_anfree(pg->uanon);
			} else {
				uobj->pgops->pgo_releasepg(pg, NULL);
				if (swap) {
					simple_unlock(&pg->uobject->vmobjlock);
				}
			}
			continue;
		}

#ifdef DIAGNOSTIC
		if (write && pgs[i]->flags & PG_FAKE) {
			panic("uvm_aio_aiodone: wrote PG_FAKE page %p", pgs[i]);
		}
#endif

		/*
		 * if this is a read and the page is PG_FAKE
		 * or this was a write, mark the page PG_CLEAN and not PG_FAKE.
		 */

		if (pgs[i]->flags & PG_FAKE || write) {
			pmap_clear_reference(PMAP_PGARG(pgs[i]));
			pmap_clear_modify(PMAP_PGARG(pgs[i]));
			pgs[i]->flags |= PG_CLEAN;
			pgs[i]->flags &= ~PG_FAKE;
		}
		if (swap) {
			if (pg->pqflags & PQ_ANON) {
				simple_unlock(&pg->uanon->an_lock);
			} else {
				simple_unlock(&pg->uobject->vmobjlock);
			}
		}
	}
	uvm_page_unbusy(pgs, npages);
	if (!swap) {
		simple_unlock(&uobj->vmobjlock);
	}

	s = splbio();
	pool_put(&bufpool, bp);
	splx(s);
}

/*
 * translate unix errno values to VM_PAGER_*.
 */

int
uvm_errno2vmerror(errno)
	int errno;
{
	switch (errno) {
	case 0:
		return VM_PAGER_OK;
	case EINVAL:
		return VM_PAGER_BAD;
	case EINPROGRESS:
		return VM_PAGER_PEND;
	case EIO:
		return VM_PAGER_ERROR;
	case EAGAIN:
		return VM_PAGER_AGAIN;
	case EBUSY:
		return VM_PAGER_UNLOCK;
	default:
		return VM_PAGER_ERROR;
	}
}
