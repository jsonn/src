/*	$NetBSD: uvm_amap.c,v 1.57.4.1 2005/11/07 17:33:17 jmc Exp $	*/

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
 */

/*
 * uvm_amap.c: amap operations
 */

/*
 * this file contains functions that perform operations on amaps.  see
 * uvm_amap.h for a brief explanation of the role of amaps in uvm.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_amap.c,v 1.57.4.1 2005/11/07 17:33:17 jmc Exp $");

#undef UVM_AMAP_INLINE		/* enable/disable amap inlines */

#include "opt_uvmhist.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/pool.h>

#define UVM_AMAP_C		/* ensure disabled inlines are in */
#include <uvm/uvm.h>
#include <uvm/uvm_swap.h>

/*
 * pool for allocation of vm_map structures.  note that the pool has
 * its own simplelock for its protection.  also note that in order to
 * avoid an endless loop, the amap pool's allocator cannot allocate
 * memory from an amap (it currently goes through the kernel uobj, so
 * we are ok).
 */
POOL_INIT(uvm_amap_pool, sizeof(struct vm_amap), 0, 0, 0, "amappl",
    &pool_allocator_nointr);

MALLOC_DEFINE(M_UVMAMAP, "UVM amap", "UVM amap and related structures");

/*
 * local functions
 */

static struct vm_amap *amap_alloc1(int, int, int);

#ifdef UVM_AMAP_PPREF
/*
 * what is ppref?   ppref is an _optional_ amap feature which is used
 * to keep track of reference counts on a per-page basis.  it is enabled
 * when UVM_AMAP_PPREF is defined.
 *
 * when enabled, an array of ints is allocated for the pprefs.  this
 * array is allocated only when a partial reference is added to the
 * map (either by unmapping part of the amap, or gaining a reference
 * to only a part of an amap).  if the malloc of the array fails
 * (M_NOWAIT), then we set the array pointer to PPREF_NONE to indicate
 * that we tried to do ppref's but couldn't alloc the array so just
 * give up (after all, this is an optional feature!).
 *
 * the array is divided into page sized "chunks."   for chunks of length 1,
 * the chunk reference count plus one is stored in that chunk's slot.
 * for chunks of length > 1 the first slot contains (the reference count
 * plus one) * -1.    [the negative value indicates that the length is
 * greater than one.]   the second slot of the chunk contains the length
 * of the chunk.   here is an example:
 *
 * actual REFS:  2  2  2  2  3  1  1  0  0  0  4  4  0  1  1  1
 *       ppref: -3  4  x  x  4 -2  2 -1  3  x -5  2  1 -2  3  x
 *              <----------><-><----><-------><----><-><------->
 * (x = don't care)
 *
 * this allows us to allow one int to contain the ref count for the whole
 * chunk.    note that the "plus one" part is needed because a reference
 * count of zero is neither positive or negative (need a way to tell
 * if we've got one zero or a bunch of them).
 *
 * here are some in-line functions to help us.
 */

static __inline void pp_getreflen(int *, int, int *, int *);
static __inline void pp_setreflen(int *, int, int, int);

/*
 * pp_getreflen: get the reference and length for a specific offset
 *
 * => ppref's amap must be locked
 */
static __inline void
pp_getreflen(ppref, offset, refp, lenp)
	int *ppref, offset, *refp, *lenp;
{

	if (ppref[offset] > 0) {		/* chunk size must be 1 */
		*refp = ppref[offset] - 1;	/* don't forget to adjust */
		*lenp = 1;
	} else {
		*refp = (ppref[offset] * -1) - 1;
		*lenp = ppref[offset+1];
	}
}

/*
 * pp_setreflen: set the reference and length for a specific offset
 *
 * => ppref's amap must be locked
 */
static __inline void
pp_setreflen(ppref, offset, ref, len)
	int *ppref, offset, ref, len;
{
	if (len == 0)
		return;
	if (len == 1) {
		ppref[offset] = ref + 1;
	} else {
		ppref[offset] = (ref + 1) * -1;
		ppref[offset+1] = len;
	}
}
#endif

/*
 * amap_alloc1: internal function that allocates an amap, but does not
 *	init the overlay.
 *
 * => lock on returned amap is init'd
 */
static inline struct vm_amap *
amap_alloc1(slots, padslots, waitf)
	int slots, padslots, waitf;
{
	struct vm_amap *amap;
	int totalslots;

	amap = pool_get(&uvm_amap_pool, (waitf == M_WAITOK) ? PR_WAITOK : 0);
	if (amap == NULL)
		return(NULL);

	totalslots = malloc_roundup((slots + padslots) * sizeof(int)) /
	    sizeof(int);
	simple_lock_init(&amap->am_l);
	amap->am_ref = 1;
	amap->am_flags = 0;
#ifdef UVM_AMAP_PPREF
	amap->am_ppref = NULL;
#endif
	amap->am_maxslot = totalslots;
	amap->am_nslot = slots;
	amap->am_nused = 0;

	amap->am_slots = malloc(totalslots * sizeof(int), M_UVMAMAP,
	    waitf);
	if (amap->am_slots == NULL)
		goto fail1;

	amap->am_bckptr = malloc(totalslots * sizeof(int), M_UVMAMAP, waitf);
	if (amap->am_bckptr == NULL)
		goto fail2;

	amap->am_anon = malloc(totalslots * sizeof(struct vm_anon *),
	    M_UVMAMAP, waitf);
	if (amap->am_anon == NULL)
		goto fail3;

	return(amap);

fail3:
	free(amap->am_bckptr, M_UVMAMAP);
fail2:
	free(amap->am_slots, M_UVMAMAP);
fail1:
	pool_put(&uvm_amap_pool, amap);

	/*
	 * XXX hack to tell the pagedaemon how many pages we need,
	 * since we can need more than it would normally free.
	 */
	if (waitf == M_NOWAIT) {
		extern int uvm_extrapages;
		uvm_extrapages += ((sizeof(int) * 2 +
				    sizeof(struct vm_anon *)) *
				   totalslots) >> PAGE_SHIFT;
	}
	return (NULL);
}

/*
 * amap_alloc: allocate an amap to manage "sz" bytes of anonymous VM
 *
 * => caller should ensure sz is a multiple of PAGE_SIZE
 * => reference count to new amap is set to one
 * => new amap is returned unlocked
 */

struct vm_amap *
amap_alloc(sz, padsz, waitf)
	vaddr_t sz, padsz;
	int waitf;
{
	struct vm_amap *amap;
	int slots, padslots;
	UVMHIST_FUNC("amap_alloc"); UVMHIST_CALLED(maphist);

	AMAP_B2SLOT(slots, sz);
	AMAP_B2SLOT(padslots, padsz);

	amap = amap_alloc1(slots, padslots, waitf);
	if (amap)
		memset(amap->am_anon, 0,
		    amap->am_maxslot * sizeof(struct vm_anon *));

	UVMHIST_LOG(maphist,"<- done, amap = 0x%x, sz=%d", amap, sz, 0, 0);
	return(amap);
}


/*
 * amap_free: free an amap
 *
 * => the amap must be unlocked
 * => the amap should have a zero reference count and be empty
 */
void
amap_free(amap)
	struct vm_amap *amap;
{
	UVMHIST_FUNC("amap_free"); UVMHIST_CALLED(maphist);

	KASSERT(amap->am_ref == 0 && amap->am_nused == 0);
	LOCK_ASSERT(!simple_lock_held(&amap->am_l));
	free(amap->am_slots, M_UVMAMAP);
	free(amap->am_bckptr, M_UVMAMAP);
	free(amap->am_anon, M_UVMAMAP);
#ifdef UVM_AMAP_PPREF
	if (amap->am_ppref && amap->am_ppref != PPREF_NONE)
		free(amap->am_ppref, M_UVMAMAP);
#endif
	pool_put(&uvm_amap_pool, amap);
	UVMHIST_LOG(maphist,"<- done, freed amap = 0x%x", amap, 0, 0, 0);
}

/*
 * amap_extend: extend the size of an amap (if needed)
 *
 * => called from uvm_map when we want to extend an amap to cover
 *    a new mapping (rather than allocate a new one)
 * => amap should be unlocked (we will lock it)
 * => to safely extend an amap it should have a reference count of
 *    one (thus it can't be shared)
 */
int
amap_extend(entry, addsize, flags)
	struct vm_map_entry *entry;
	vsize_t addsize;
	int flags;
{
	struct vm_amap *amap = entry->aref.ar_amap;
	int slotoff = entry->aref.ar_pageoff;
	int slotmapped, slotadd, slotneed, slotadded, slotalloc;
	int slotadj, slotspace;
#ifdef UVM_AMAP_PPREF
	int *newppref, *oldppref;
#endif
	int i, *newsl, *newbck, *oldsl, *oldbck;
	struct vm_anon **newover, **oldover;
	int mflag = (flags & AMAP_EXTEND_NOWAIT) ? M_NOWAIT :
		        (M_WAITOK | M_CANFAIL);

	UVMHIST_FUNC("amap_extend"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "  (entry=0x%x, addsize=0x%x, flags=0x%x)",
	    entry, addsize, flags, 0);

	/*
	 * first, determine how many slots we need in the amap.  don't
	 * forget that ar_pageoff could be non-zero: this means that
	 * there are some unused slots before us in the amap.
	 */

	amap_lock(amap);
	KASSERT(amap_refs(amap) == 1); /* amap can't be shared */
	AMAP_B2SLOT(slotmapped, entry->end - entry->start); /* slots mapped */
	AMAP_B2SLOT(slotadd, addsize);			/* slots to add */
	if (flags & AMAP_EXTEND_FORWARDS) {
		slotneed = slotoff + slotmapped + slotadd;
		slotadj = 0;
		slotspace = 0;
	}
	else {
		slotneed = slotadd + slotmapped;
		slotadj = slotadd - slotoff;
		slotspace = amap->am_maxslot - slotmapped;
	}

	/*
	 * case 1: we already have enough slots in the map and thus
	 * only need to bump the reference counts on the slots we are
	 * adding.
	 */

	if (flags & AMAP_EXTEND_FORWARDS) {
		if (amap->am_nslot >= slotneed) {
#ifdef UVM_AMAP_PPREF
			if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
				amap_pp_adjref(amap, slotoff + slotmapped,
				    slotadd, 1);
			}
#endif
			amap_unlock(amap);
			UVMHIST_LOG(maphist,
			    "<- done (case 1f), amap = 0x%x, sltneed=%d",
			    amap, slotneed, 0, 0);
			return 0;
		}
	} else {
		if (slotadj <= 0) {
			slotoff -= slotadd;
			entry->aref.ar_pageoff = slotoff;
#ifdef UVM_AMAP_PPREF
			if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
				amap_pp_adjref(amap, slotoff, slotadd, 1);
			}
#endif
			amap_unlock(amap);
			UVMHIST_LOG(maphist,
			    "<- done (case 1b), amap = 0x%x, sltneed=%d",
			    amap, slotneed, 0, 0);
			return 0;
		}
	}

	/*
	 * case 2: we pre-allocated slots for use and we just need to
	 * bump nslot up to take account for these slots.
	 */

	if (amap->am_maxslot >= slotneed) {
		if (flags & AMAP_EXTEND_FORWARDS) {
#ifdef UVM_AMAP_PPREF
			if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
				if ((slotoff + slotmapped) < amap->am_nslot)
					amap_pp_adjref(amap,
					    slotoff + slotmapped,
					    (amap->am_nslot -
					    (slotoff + slotmapped)), 1);
				pp_setreflen(amap->am_ppref, amap->am_nslot, 1,
				    slotneed - amap->am_nslot);
			}
#endif
			amap->am_nslot = slotneed;
			amap_unlock(amap);

			/*
			 * no need to zero am_anon since that was done at
			 * alloc time and we never shrink an allocation.
			 */

			UVMHIST_LOG(maphist,"<- done (case 2f), amap = 0x%x, "
			    "slotneed=%d", amap, slotneed, 0, 0);
			return 0;
		} else {
#ifdef UVM_AMAP_PPREF
			if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
				/*
				 * Slide up the ref counts on the pages that
				 * are actually in use.
				 */
				memmove(amap->am_ppref + slotspace,
				    amap->am_ppref + slotoff,
				    slotmapped * sizeof(int));
				/*
				 * Mark the (adjusted) gap at the front as
				 * referenced/not referenced.
				 */
				pp_setreflen(amap->am_ppref,
				    0, 0, slotspace - slotadd);
				pp_setreflen(amap->am_ppref,
				    slotspace - slotadd, 1, slotadd);
			}
#endif

			/*
			 * Slide the anon pointers up and clear out
			 * the space we just made.
			 */
			memmove(amap->am_anon + slotspace,
			    amap->am_anon + slotoff,
			    slotmapped * sizeof(struct vm_anon*));
			memset(amap->am_anon + slotoff, 0,
			    (slotspace - slotoff) * sizeof(struct vm_anon *));

			/*
			 * Slide the backpointers up, but don't bother
			 * wiping out the old slots.
			 */
			memmove(amap->am_bckptr + slotspace,
			    amap->am_bckptr + slotoff,
			    slotmapped * sizeof(int));

			/*
			 * Adjust all the useful active slot numbers.
			 */
			for (i = 0; i < amap->am_nused; i++)
				amap->am_slots[i] += (slotspace - slotoff);

			/*
			 * We just filled all the empty space in the
			 * front of the amap by activating a few new
			 * slots.
			 */
			amap->am_nslot = amap->am_maxslot;
			entry->aref.ar_pageoff = slotspace - slotadd;
			amap_unlock(amap);

			UVMHIST_LOG(maphist,"<- done (case 2b), amap = 0x%x, "
			    "slotneed=%d", amap, slotneed, 0, 0);
			return 0;
		}
	}

	/*
	 * case 3: we need to malloc a new amap and copy all the amap
	 * data over from old amap to the new one.
	 *
	 * note that the use of a kernel realloc() probably would not
	 * help here, since we wish to abort cleanly if one of the
	 * three (or four) mallocs fails.
	 */

	amap_unlock(amap);	/* unlock in case we sleep in malloc */
	slotalloc = malloc_roundup(slotneed * sizeof(int)) / sizeof(int);
#ifdef UVM_AMAP_PPREF
	newppref = NULL;
	if (amap->am_ppref && amap->am_ppref != PPREF_NONE)
		newppref = malloc(slotalloc * sizeof(int), M_UVMAMAP, mflag);
#endif
	newsl = malloc(slotalloc * sizeof(int), M_UVMAMAP, mflag);
	newbck = malloc(slotalloc * sizeof(int), M_UVMAMAP, mflag);
	newover = malloc(slotalloc * sizeof(struct vm_anon *), M_UVMAMAP,
		    mflag);
	if (newsl == NULL || newbck == NULL || newover == NULL) {
#ifdef UVM_AMAP_PPREF
		if (newppref != NULL) {
			free(newppref, M_UVMAMAP);
		}
#endif
		if (newsl != NULL) {
			free(newsl, M_UVMAMAP);
		}
		if (newbck != NULL) {
			free(newbck, M_UVMAMAP);
		}
		if (newover != NULL) {
			free(newover, M_UVMAMAP);
		}
		return ENOMEM;
	}
	amap_lock(amap);
	KASSERT(amap->am_maxslot < slotneed);

	/*
	 * now copy everything over to new malloc'd areas...
	 */

	slotadded = slotalloc - amap->am_nslot;
	if (!(flags & AMAP_EXTEND_FORWARDS))
		slotspace = slotalloc - slotmapped;

	/* do am_slots */
	oldsl = amap->am_slots;
	if (flags & AMAP_EXTEND_FORWARDS)
		memcpy(newsl, oldsl, sizeof(int) * amap->am_nused);
	else
		for (i = 0; i < amap->am_nused; i++)
			newsl[i] = oldsl[i] + slotspace - slotoff;
	amap->am_slots = newsl;

	/* do am_anon */
	oldover = amap->am_anon;
	if (flags & AMAP_EXTEND_FORWARDS) {
		memcpy(newover, oldover,
		    sizeof(struct vm_anon *) * amap->am_nslot);
		memset(newover + amap->am_nslot, 0,
		    sizeof(struct vm_anon *) * slotadded);
	} else {
		memcpy(newover + slotspace, oldover + slotoff,
		    sizeof(struct vm_anon *) * slotmapped);
		memset(newover, 0,
		    sizeof(struct vm_anon *) * slotspace);
	}
	amap->am_anon = newover;

	/* do am_bckptr */
	oldbck = amap->am_bckptr;
	if (flags & AMAP_EXTEND_FORWARDS)
		memcpy(newbck, oldbck, sizeof(int) * amap->am_nslot);
	else
		memcpy(newbck + slotspace, oldbck + slotoff,
		    sizeof(int) * slotmapped);
	amap->am_bckptr = newbck;

#ifdef UVM_AMAP_PPREF
	/* do ppref */
	oldppref = amap->am_ppref;
	if (newppref) {
		if (flags & AMAP_EXTEND_FORWARDS) {
			memcpy(newppref, oldppref,
			    sizeof(int) * amap->am_nslot);
			memset(newppref + amap->am_nslot, 0,
			    sizeof(int) * slotadded);
		} else {
			memcpy(newppref + slotspace, oldppref + slotoff,
			    sizeof(int) * slotmapped);
		}
		amap->am_ppref = newppref;
		if ((flags & AMAP_EXTEND_FORWARDS) &&
		    (slotoff + slotmapped) < amap->am_nslot)
			amap_pp_adjref(amap, slotoff + slotmapped,
			    (amap->am_nslot - (slotoff + slotmapped)), 1);
		if (flags & AMAP_EXTEND_FORWARDS)
			pp_setreflen(newppref, amap->am_nslot, 1,
			    slotneed - amap->am_nslot);
		else {
			pp_setreflen(newppref, 0, 0,
			    slotalloc - slotneed);
			pp_setreflen(newppref, slotalloc - slotneed, 1,
			    slotneed - slotmapped);
		}
	} else {
		if (amap->am_ppref)
			amap->am_ppref = PPREF_NONE;
	}
#endif

	/* update master values */
	if (flags & AMAP_EXTEND_FORWARDS)
		amap->am_nslot = slotneed;
	else {
		entry->aref.ar_pageoff = slotspace - slotadd;
		amap->am_nslot = slotalloc;
	}
	amap->am_maxslot = slotalloc;

	amap_unlock(amap);
	free(oldsl, M_UVMAMAP);
	free(oldbck, M_UVMAMAP);
	free(oldover, M_UVMAMAP);
#ifdef UVM_AMAP_PPREF
	if (oldppref && oldppref != PPREF_NONE)
		free(oldppref, M_UVMAMAP);
#endif
	UVMHIST_LOG(maphist,"<- done (case 3), amap = 0x%x, slotneed=%d",
	    amap, slotneed, 0, 0);
	return 0;
}

/*
 * amap_share_protect: change protection of anons in a shared amap
 *
 * for shared amaps, given the current data structure layout, it is
 * not possible for us to directly locate all maps referencing the
 * shared anon (to change the protection).  in order to protect data
 * in shared maps we use pmap_page_protect().  [this is useful for IPC
 * mechanisms like map entry passing that may want to write-protect
 * all mappings of a shared amap.]  we traverse am_anon or am_slots
 * depending on the current state of the amap.
 *
 * => entry's map and amap must be locked by the caller
 */
void
amap_share_protect(entry, prot)
	struct vm_map_entry *entry;
	vm_prot_t prot;
{
	struct vm_amap *amap = entry->aref.ar_amap;
	int slots, lcv, slot, stop;

	LOCK_ASSERT(simple_lock_held(&amap->am_l));

	AMAP_B2SLOT(slots, (entry->end - entry->start));
	stop = entry->aref.ar_pageoff + slots;

	if (slots < amap->am_nused) {
		/* cheaper to traverse am_anon */
		for (lcv = entry->aref.ar_pageoff ; lcv < stop ; lcv++) {
			if (amap->am_anon[lcv] == NULL)
				continue;
			if (amap->am_anon[lcv]->u.an_page != NULL)
				pmap_page_protect(amap->am_anon[lcv]->u.an_page,
						  prot);
		}
		return;
	}

	/* cheaper to traverse am_slots */
	for (lcv = 0 ; lcv < amap->am_nused ; lcv++) {
		slot = amap->am_slots[lcv];
		if (slot < entry->aref.ar_pageoff || slot >= stop)
			continue;
		if (amap->am_anon[slot]->u.an_page != NULL)
			pmap_page_protect(amap->am_anon[slot]->u.an_page, prot);
	}
}

/*
 * amap_wipeout: wipeout all anon's in an amap; then free the amap!
 *
 * => called from amap_unref when the final reference to an amap is
 *	discarded (i.e. when reference count == 1)
 * => the amap should be locked (by the caller)
 */

void
amap_wipeout(amap)
	struct vm_amap *amap;
{
	int lcv, slot;
	struct vm_anon *anon;
	UVMHIST_FUNC("amap_wipeout"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"(amap=0x%x)", amap, 0,0,0);

	amap_unlock(amap);
	for (lcv = 0 ; lcv < amap->am_nused ; lcv++) {
		int refs;

		slot = amap->am_slots[lcv];
		anon = amap->am_anon[slot];

		if (anon == NULL || anon->an_ref == 0)
			panic("amap_wipeout: corrupt amap");

		simple_lock(&anon->an_lock);
		UVMHIST_LOG(maphist,"  processing anon 0x%x, ref=%d", anon,
		    anon->an_ref, 0, 0);
		refs = --anon->an_ref;
		simple_unlock(&anon->an_lock);
		if (refs == 0) {

			/*
			 * we had the last reference to a vm_anon. free it.
			 */

			uvm_anfree(anon);
		}

		/*
		 * XXX
		 * releasing the swap space held by an N anons is an O(N^2)
		 * operation because of the implementation of extents.
		 * if there are many anons, tearing down an exiting process'
		 * address space can take many seconds, which causes very
		 * annoying pauses.  we yield here to give other processes
		 * a chance to run.  this should be removed once the performance
		 * of swap space management is improved.
		 */

		if (curlwp->l_cpu->ci_schedstate.spc_flags & SPCF_SHOULDYIELD)
			preempt(1);
	}

	/*
	 * now we free the map
	 */

	amap->am_ref = 0;	/* ... was one */
	amap->am_nused = 0;
	amap_free(amap);	/* will unlock and free amap */
	UVMHIST_LOG(maphist,"<- done!", 0,0,0,0);
}

/*
 * amap_copy: ensure that a map entry's "needs_copy" flag is false
 *	by copying the amap if necessary.
 *
 * => an entry with a null amap pointer will get a new (blank) one.
 * => the map that the map entry belongs to must be locked by caller.
 * => the amap currently attached to "entry" (if any) must be unlocked.
 * => if canchunk is true, then we may clip the entry into a chunk
 * => "startva" and "endva" are used only if canchunk is true.  they are
 *     used to limit chunking (e.g. if you have a large space that you
 *     know you are going to need to allocate amaps for, there is no point
 *     in allowing that to be chunked)
 */

void
amap_copy(map, entry, waitf, canchunk, startva, endva)
	struct vm_map *map;
	struct vm_map_entry *entry;
	int waitf;
	boolean_t canchunk;
	vaddr_t startva, endva;
{
	struct vm_amap *amap, *srcamap;
	int slots, lcv;
	vaddr_t chunksize;
	UVMHIST_FUNC("amap_copy"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist, "  (map=%p, entry=%p, waitf=%d)",
		    map, entry, waitf, 0);

	/*
	 * is there a map to copy?   if not, create one from scratch.
	 */

	if (entry->aref.ar_amap == NULL) {

		/*
		 * check to see if we have a large amap that we can
		 * chunk.  we align startva/endva to chunk-sized
		 * boundaries and then clip to them.
		 */

		if (canchunk && atop(entry->end - entry->start) >=
		    UVM_AMAP_LARGE) {
			/* convert slots to bytes */
			chunksize = UVM_AMAP_CHUNK << PAGE_SHIFT;
			startva = (startva / chunksize) * chunksize;
			endva = roundup(endva, chunksize);
			UVMHIST_LOG(maphist, "  chunk amap ==> clip 0x%x->0x%x"
			    "to 0x%x->0x%x", entry->start, entry->end, startva,
			    endva);
			UVM_MAP_CLIP_START(map, entry, startva, NULL);
			/* watch out for endva wrap-around! */
			if (endva >= startva)
				UVM_MAP_CLIP_END(map, entry, endva, NULL);
		}

		UVMHIST_LOG(maphist, "<- done [creating new amap 0x%x->0x%x]",
		entry->start, entry->end, 0, 0);
		entry->aref.ar_pageoff = 0;
		entry->aref.ar_amap = amap_alloc(entry->end - entry->start, 0,
		    waitf);
		if (entry->aref.ar_amap != NULL)
			entry->etype &= ~UVM_ET_NEEDSCOPY;
		return;
	}

	/*
	 * first check and see if we are the only map entry
	 * referencing the amap we currently have.  if so, then we can
	 * just take it over rather than copying it.  note that we are
	 * reading am_ref with the amap unlocked... the value can only
	 * be one if we have the only reference to the amap (via our
	 * locked map).  if we are greater than one we fall through to
	 * the next case (where we double check the value).
	 */

	if (entry->aref.ar_amap->am_ref == 1) {
		entry->etype &= ~UVM_ET_NEEDSCOPY;
		UVMHIST_LOG(maphist, "<- done [ref cnt = 1, took it over]",
		    0, 0, 0, 0);
		return;
	}

	/*
	 * looks like we need to copy the map.
	 */

	UVMHIST_LOG(maphist,"  amap=%p, ref=%d, must copy it",
	    entry->aref.ar_amap, entry->aref.ar_amap->am_ref, 0, 0);
	AMAP_B2SLOT(slots, entry->end - entry->start);
	amap = amap_alloc1(slots, 0, waitf);
	if (amap == NULL) {
		UVMHIST_LOG(maphist, "  amap_alloc1 failed", 0,0,0,0);
		return;
	}
	srcamap = entry->aref.ar_amap;
	amap_lock(srcamap);

	/*
	 * need to double check reference count now that we've got the
	 * src amap locked down.  the reference count could have
	 * changed while we were in malloc.  if the reference count
	 * dropped down to one we take over the old map rather than
	 * copying the amap.
	 */

	if (srcamap->am_ref == 1) {		/* take it over? */
		entry->etype &= ~UVM_ET_NEEDSCOPY;
		amap->am_ref--;		/* drop final reference to map */
		amap_free(amap);	/* dispose of new (unused) amap */
		amap_unlock(srcamap);
		return;
	}

	/*
	 * we must copy it now.
	 */

	UVMHIST_LOG(maphist, "  copying amap now",0, 0, 0, 0);
	for (lcv = 0 ; lcv < slots; lcv++) {
		amap->am_anon[lcv] =
		    srcamap->am_anon[entry->aref.ar_pageoff + lcv];
		if (amap->am_anon[lcv] == NULL)
			continue;
		simple_lock(&amap->am_anon[lcv]->an_lock);
		amap->am_anon[lcv]->an_ref++;
		simple_unlock(&amap->am_anon[lcv]->an_lock);
		amap->am_bckptr[lcv] = amap->am_nused;
		amap->am_slots[amap->am_nused] = lcv;
		amap->am_nused++;
	}
	memset(&amap->am_anon[lcv], 0,
	    (amap->am_maxslot - lcv) * sizeof(struct vm_anon *));

	/*
	 * drop our reference to the old amap (srcamap) and unlock.
	 * we know that the reference count on srcamap is greater than
	 * one (we checked above), so there is no way we could drop
	 * the count to zero.  [and no need to worry about freeing it]
	 */

	srcamap->am_ref--;
	if (srcamap->am_ref == 1 && (srcamap->am_flags & AMAP_SHARED) != 0)
		srcamap->am_flags &= ~AMAP_SHARED;   /* clear shared flag */
#ifdef UVM_AMAP_PPREF
	if (srcamap->am_ppref && srcamap->am_ppref != PPREF_NONE) {
		amap_pp_adjref(srcamap, entry->aref.ar_pageoff,
		    (entry->end - entry->start) >> PAGE_SHIFT, -1);
	}
#endif

	amap_unlock(srcamap);

	/*
	 * install new amap.
	 */

	entry->aref.ar_pageoff = 0;
	entry->aref.ar_amap = amap;
	entry->etype &= ~UVM_ET_NEEDSCOPY;
	UVMHIST_LOG(maphist, "<- done",0, 0, 0, 0);
}

/*
 * amap_cow_now: resolve all copy-on-write faults in an amap now for fork(2)
 *
 *	called during fork(2) when the parent process has a wired map
 *	entry.   in that case we want to avoid write-protecting pages
 *	in the parent's map (e.g. like what you'd do for a COW page)
 *	so we resolve the COW here.
 *
 * => assume parent's entry was wired, thus all pages are resident.
 * => assume pages that are loaned out (loan_count) are already mapped
 *	read-only in all maps, and thus no need for us to worry about them
 * => assume both parent and child vm_map's are locked
 * => caller passes child's map/entry in to us
 * => if we run out of memory we will unlock the amap and sleep _with_ the
 *	parent and child vm_map's locked(!).    we have to do this since
 *	we are in the middle of a fork(2) and we can't let the parent
 *	map change until we are done copying all the map entrys.
 * => XXXCDC: out of memory should cause fork to fail, but there is
 *	currently no easy way to do this (needs fix)
 * => page queues must be unlocked (we may lock them)
 */

void
amap_cow_now(map, entry)
	struct vm_map *map;
	struct vm_map_entry *entry;
{
	struct vm_amap *amap = entry->aref.ar_amap;
	int lcv, slot;
	struct vm_anon *anon, *nanon;
	struct vm_page *pg, *npg;

	/*
	 * note that if we unlock the amap then we must ReStart the "lcv" for
	 * loop because some other process could reorder the anon's in the
	 * am_anon[] array on us while the lock is dropped.
	 */

ReStart:
	amap_lock(amap);
	for (lcv = 0 ; lcv < amap->am_nused ; lcv++) {

		/*
		 * get the page
		 */

		slot = amap->am_slots[lcv];
		anon = amap->am_anon[slot];
		simple_lock(&anon->an_lock);

		/*
		 * If the anon has only one ref, we must have already copied it.
		 * This can happen if we needed to sleep waiting for memory
		 * in a previous run through this loop.  The new page might
		 * even have been paged out, since the new page is not wired.
		 */

		if (anon->an_ref == 1) {
			KASSERT(anon->u.an_page != NULL ||
				anon->an_swslot != 0);
			simple_unlock(&anon->an_lock);
			continue;
		}

		/*
		 * The old page must be resident since the parent is wired.
		 */

		pg = anon->u.an_page;
		KASSERT(pg != NULL);
		KASSERT(pg->wire_count > 0);

		/*
		 * If the page is loaned then it must already be mapped
		 * read-only and we don't need to copy it.
		 */

		if (pg->loan_count != 0) {
			simple_unlock(&anon->an_lock);
			continue;
		}
		KASSERT(pg->uanon == anon && pg->uobject == NULL);

		/*
		 * if the page is busy then we have to unlock, wait for
		 * it and then restart.
		 */

		if (pg->flags & PG_BUSY) {
			pg->flags |= PG_WANTED;
			amap_unlock(amap);
			UVM_UNLOCK_AND_WAIT(pg, &anon->an_lock, FALSE,
			    "cownow", 0);
			goto ReStart;
		}

		/*
		 * ok, time to do a copy-on-write to a new anon
		 */

		nanon = uvm_analloc();
		if (nanon) {
			npg = uvm_pagealloc(NULL, 0, nanon, 0);
		} else
			npg = NULL;	/* XXX: quiet gcc warning */
		if (nanon == NULL || npg == NULL) {

			/*
			 * XXXCDC: we should cause fork to fail, but we can't.
			 */

			if (nanon) {
				nanon->an_ref--;
				simple_unlock(&nanon->an_lock);
				uvm_anfree(nanon);
			}
			simple_unlock(&anon->an_lock);
			amap_unlock(amap);
			uvm_wait("cownowpage");
			goto ReStart;
		}

		/*
		 * got it... now we can copy the data and replace anon
		 * with our new one...
		 */

		uvm_pagecopy(pg, npg);		/* old -> new */
		anon->an_ref--;			/* can't drop to zero */
		amap->am_anon[slot] = nanon;	/* replace */

		/*
		 * drop PG_BUSY on new page ... since we have had its owner
		 * locked the whole time it can't be PG_RELEASED or PG_WANTED.
		 */

		uvm_lock_pageq();
		uvm_pageactivate(npg);
		uvm_unlock_pageq();
		npg->flags &= ~(PG_BUSY|PG_FAKE);
		UVM_PAGE_OWN(npg, NULL);
		simple_unlock(&nanon->an_lock);
		simple_unlock(&anon->an_lock);
	}
	amap_unlock(amap);
}

/*
 * amap_splitref: split a single reference into two separate references
 *
 * => called from uvm_map's clip routines
 * => origref's map should be locked
 * => origref->ar_amap should be unlocked (we will lock)
 */
void
amap_splitref(origref, splitref, offset)
	struct vm_aref *origref, *splitref;
	vaddr_t offset;
{
	int leftslots;

	AMAP_B2SLOT(leftslots, offset);
	if (leftslots == 0)
		panic("amap_splitref: split at zero offset");

	amap_lock(origref->ar_amap);

	/*
	 * now: amap is locked and we have a valid am_mapped array.
	 */

	if (origref->ar_amap->am_nslot - origref->ar_pageoff - leftslots <= 0)
		panic("amap_splitref: map size check failed");

#ifdef UVM_AMAP_PPREF
        /*
	 * establish ppref before we add a duplicate reference to the amap
	 */
	if (origref->ar_amap->am_ppref == NULL)
		amap_pp_establish(origref->ar_amap, origref->ar_pageoff);
#endif

	splitref->ar_amap = origref->ar_amap;
	splitref->ar_amap->am_ref++;		/* not a share reference */
	splitref->ar_pageoff = origref->ar_pageoff + leftslots;

	amap_unlock(origref->ar_amap);
}

#ifdef UVM_AMAP_PPREF

/*
 * amap_pp_establish: add a ppref array to an amap, if possible
 *
 * => amap locked by caller
 */
void
amap_pp_establish(amap, offset)
	struct vm_amap *amap;
	vaddr_t offset;
{
	amap->am_ppref = malloc(sizeof(int) * amap->am_maxslot,
	    M_UVMAMAP, M_NOWAIT);

	/*
	 * if we fail then we just won't use ppref for this amap
	 */

	if (amap->am_ppref == NULL) {
		amap->am_ppref = PPREF_NONE;	/* not using it */
		return;
	}
	memset(amap->am_ppref, 0, sizeof(int) * amap->am_maxslot);
	pp_setreflen(amap->am_ppref, 0, 0, offset);
	pp_setreflen(amap->am_ppref, offset, amap->am_ref,
	    amap->am_nslot - offset);
	return;
}

/*
 * amap_pp_adjref: adjust reference count to a part of an amap using the
 * per-page reference count array.
 *
 * => map and amap locked by caller
 * => caller must check that ppref != PPREF_NONE before calling
 */
void
amap_pp_adjref(amap, curslot, slotlen, adjval)
	struct vm_amap *amap;
	int curslot;
	vsize_t slotlen;
	int adjval;
{
	int stopslot, *ppref, lcv, prevlcv;
	int ref, len, prevref, prevlen;

	stopslot = curslot + slotlen;
	ppref = amap->am_ppref;
	prevlcv = 0;

	/*
	 * first advance to the correct place in the ppref array,
	 * fragment if needed.
	 */

	for (lcv = 0 ; lcv < curslot ; lcv += len) {
		pp_getreflen(ppref, lcv, &ref, &len);
		if (lcv + len > curslot) {     /* goes past start? */
			pp_setreflen(ppref, lcv, ref, curslot - lcv);
			pp_setreflen(ppref, curslot, ref, len - (curslot -lcv));
			len = curslot - lcv;   /* new length of entry @ lcv */
		}
		prevlcv = lcv;
	}
	if (lcv != 0)
		pp_getreflen(ppref, prevlcv, &prevref, &prevlen);
	else {
		/* Ensure that the "prevref == ref" test below always
		 * fails, since we're starting from the beginning of
		 * the ppref array; that is, there is no previous
		 * chunk.
		 */
		prevref = -1;
		prevlen = 0;
	}

	/*
	 * now adjust reference counts in range.  merge the first
	 * changed entry with the last unchanged entry if possible.
	 */

	if (lcv != curslot)
		panic("amap_pp_adjref: overshot target");

	for (/* lcv already set */; lcv < stopslot ; lcv += len) {
		pp_getreflen(ppref, lcv, &ref, &len);
		if (lcv + len > stopslot) {     /* goes past end? */
			pp_setreflen(ppref, lcv, ref, stopslot - lcv);
			pp_setreflen(ppref, stopslot, ref,
			    len - (stopslot - lcv));
			len = stopslot - lcv;
		}
		ref += adjval;
		if (ref < 0)
			panic("amap_pp_adjref: negative reference count");
		if (lcv == prevlcv + prevlen && ref == prevref) {
			pp_setreflen(ppref, prevlcv, ref, prevlen + len);
		} else {
			pp_setreflen(ppref, lcv, ref, len);
		}
		if (ref == 0)
			amap_wiperange(amap, lcv, len);
	}

}

/*
 * amap_wiperange: wipe out a range of an amap
 * [different from amap_wipeout because the amap is kept intact]
 *
 * => both map and amap must be locked by caller.
 */
void
amap_wiperange(amap, slotoff, slots)
	struct vm_amap *amap;
	int slotoff, slots;
{
	int byanon, lcv, stop, curslot, ptr, slotend;
	struct vm_anon *anon;

	/*
	 * we can either traverse the amap by am_anon or by am_slots depending
	 * on which is cheaper.    decide now.
	 */

	if (slots < amap->am_nused) {
		byanon = TRUE;
		lcv = slotoff;
		stop = slotoff + slots;
		slotend = 0;
	} else {
		byanon = FALSE;
		lcv = 0;
		stop = amap->am_nused;
		slotend = slotoff + slots;
	}

	while (lcv < stop) {
		int refs;

		if (byanon) {
			curslot = lcv++;	/* lcv advances here */
			if (amap->am_anon[curslot] == NULL)
				continue;
		} else {
			curslot = amap->am_slots[lcv];
			if (curslot < slotoff || curslot >= slotend) {
				lcv++;		/* lcv advances here */
				continue;
			}
			stop--;	/* drop stop, since anon will be removed */
		}
		anon = amap->am_anon[curslot];

		/*
		 * remove it from the amap
		 */

		amap->am_anon[curslot] = NULL;
		ptr = amap->am_bckptr[curslot];
		if (ptr != (amap->am_nused - 1)) {
			amap->am_slots[ptr] =
			    amap->am_slots[amap->am_nused - 1];
			amap->am_bckptr[amap->am_slots[ptr]] =
			    ptr;    /* back ptr. */
		}
		amap->am_nused--;

		/*
		 * drop anon reference count
		 */

		simple_lock(&anon->an_lock);
		refs = --anon->an_ref;
		simple_unlock(&anon->an_lock);
		if (refs == 0) {

			/*
			 * we just eliminated the last reference to an anon.
			 * free it.
			 */

			uvm_anfree(anon);
		}
	}
}

#endif
