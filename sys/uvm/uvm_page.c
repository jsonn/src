/*	$NetBSD: uvm_page.c,v 1.97.2.2 2004/09/11 11:00:34 he Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	This product includes software developed by Charles D. Cranor,
 *      Washington University, the University of California, Berkeley and
 *      its contributors.
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
 *	@(#)vm_page.c   8.3 (Berkeley) 3/21/94
 * from: Id: uvm_page.c,v 1.1.2.18 1998/02/06 05:24:42 chs Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * uvm_page.c: page ops.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_page.c,v 1.97.2.2 2004/09/11 11:00:34 he Exp $");

#include "opt_uvmhist.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sched.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/proc.h>

#define UVM_PAGE                /* pull in uvm_page.h functions */
#include <uvm/uvm.h>

/*
 * global vars... XXXCDC: move to uvm. structure.
 */

/*
 * physical memory config is stored in vm_physmem.
 */

struct vm_physseg vm_physmem[VM_PHYSSEG_MAX];	/* XXXCDC: uvm.physmem */
int vm_nphysseg = 0;				/* XXXCDC: uvm.nphysseg */

/*
 * Some supported CPUs in a given architecture don't support all
 * of the things necessary to do idle page zero'ing efficiently.
 * We therefore provide a way to disable it from machdep code here.
 */
/*
 * XXX disabled until we can find a way to do this without causing
 * problems for either CPU caches or DMA latency.
 */
boolean_t vm_page_zero_enable = FALSE;

/*
 * local variables
 */

/*
 * these variables record the values returned by vm_page_bootstrap,
 * for debugging purposes.  The implementation of uvm_pageboot_alloc
 * and pmap_startup here also uses them internally.
 */

static vaddr_t      virtual_space_start;
static vaddr_t      virtual_space_end;

/*
 * we use a hash table with only one bucket during bootup.  we will
 * later rehash (resize) the hash table once the allocator is ready.
 * we static allocate the one bootstrap bucket below...
 */

static struct pglist uvm_bootbucket;

/*
 * we allocate an initial number of page colors in uvm_page_init(),
 * and remember them.  We may re-color pages as cache sizes are
 * discovered during the autoconfiguration phase.  But we can never
 * free the initial set of buckets, since they are allocated using
 * uvm_pageboot_alloc().
 */

static boolean_t have_recolored_pages /* = FALSE */;

MALLOC_DEFINE(M_VMPAGE, "VM page", "VM page");

#ifdef DEBUG
vaddr_t uvm_zerocheckkva;
#endif /* DEBUG */

/*
 * local prototypes
 */

static void uvm_pageinsert(struct vm_page *);
static void uvm_pageinsert_after(struct vm_page *, struct vm_page *);
static void uvm_pageremove(struct vm_page *);

/*
 * inline functions
 */

/*
 * uvm_pageinsert: insert a page in the object and the hash table
 * uvm_pageinsert_after: insert a page into the specified place in listq
 *
 * => caller must lock object
 * => caller must lock page queues
 * => call should have already set pg's object and offset pointers
 *    and bumped the version counter
 */

__inline static void
uvm_pageinsert_after(pg, where)
	struct vm_page *pg;
	struct vm_page *where;
{
	struct pglist *buck;
	struct uvm_object *uobj = pg->uobject;

	KASSERT((pg->flags & PG_TABLED) == 0);
	KASSERT(where == NULL || (where->flags & PG_TABLED));
	KASSERT(where == NULL || (where->uobject == uobj));
	buck = &uvm.page_hash[uvm_pagehash(uobj, pg->offset)];
	simple_lock(&uvm.hashlock);
	TAILQ_INSERT_TAIL(buck, pg, hashq);
	simple_unlock(&uvm.hashlock);

	if (UVM_OBJ_IS_VNODE(uobj)) {
		if (uobj->uo_npages == 0) {
			struct vnode *vp = (struct vnode *)uobj;

			vholdl(vp);
		}
		if (UVM_OBJ_IS_VTEXT(uobj)) {
			uvmexp.execpages++;
		} else {
			uvmexp.filepages++;
		}
	} else if (UVM_OBJ_IS_AOBJ(uobj)) {
		uvmexp.anonpages++;
	}

	if (where)
		TAILQ_INSERT_AFTER(&uobj->memq, where, pg, listq);
	else
		TAILQ_INSERT_TAIL(&uobj->memq, pg, listq);
	pg->flags |= PG_TABLED;
	uobj->uo_npages++;
}

__inline static void
uvm_pageinsert(pg)
	struct vm_page *pg;
{

	uvm_pageinsert_after(pg, NULL);
}

/*
 * uvm_page_remove: remove page from object and hash
 *
 * => caller must lock object
 * => caller must lock page queues
 */

static __inline void
uvm_pageremove(pg)
	struct vm_page *pg;
{
	struct pglist *buck;
	struct uvm_object *uobj = pg->uobject;

	KASSERT(pg->flags & PG_TABLED);
	buck = &uvm.page_hash[uvm_pagehash(uobj, pg->offset)];
	simple_lock(&uvm.hashlock);
	TAILQ_REMOVE(buck, pg, hashq);
	simple_unlock(&uvm.hashlock);

	if (UVM_OBJ_IS_VNODE(uobj)) {
		if (uobj->uo_npages == 1) {
			struct vnode *vp = (struct vnode *)uobj;

			holdrelel(vp);
		}
		if (UVM_OBJ_IS_VTEXT(uobj)) {
			uvmexp.execpages--;
		} else {
			uvmexp.filepages--;
		}
	} else if (UVM_OBJ_IS_AOBJ(uobj)) {
		uvmexp.anonpages--;
	}

	/* object should be locked */
	uobj->uo_npages--;
	TAILQ_REMOVE(&uobj->memq, pg, listq);
	pg->flags &= ~PG_TABLED;
	pg->uobject = NULL;
}

static void
uvm_page_init_buckets(struct pgfreelist *pgfl)
{
	int color, i;

	for (color = 0; color < uvmexp.ncolors; color++) {
		for (i = 0; i < PGFL_NQUEUES; i++) {
			TAILQ_INIT(&pgfl->pgfl_buckets[color].pgfl_queues[i]);
		}
	}
}

/*
 * uvm_page_init: init the page system.   called from uvm_init().
 *
 * => we return the range of kernel virtual memory in kvm_startp/kvm_endp
 */

void
uvm_page_init(kvm_startp, kvm_endp)
	vaddr_t *kvm_startp, *kvm_endp;
{
	vsize_t freepages, pagecount, bucketcount, n;
	struct pgflbucket *bucketarray;
	struct vm_page *pagearray;
	int lcv;
	u_int i;
	paddr_t paddr;

	/*
	 * init the page queues and page queue locks, except the free
	 * list; we allocate that later (with the initial vm_page
	 * structures).
	 */

	TAILQ_INIT(&uvm.page_active);
	TAILQ_INIT(&uvm.page_inactive);
	simple_lock_init(&uvm.pageqlock);
	simple_lock_init(&uvm.fpageqlock);

	/*
	 * init the <obj,offset> => <page> hash table.  for now
	 * we just have one bucket (the bootstrap bucket).  later on we
	 * will allocate new buckets as we dynamically resize the hash table.
	 */

	uvm.page_nhash = 1;			/* 1 bucket */
	uvm.page_hashmask = 0;			/* mask for hash function */
	uvm.page_hash = &uvm_bootbucket;	/* install bootstrap bucket */
	TAILQ_INIT(uvm.page_hash);		/* init hash table */
	simple_lock_init(&uvm.hashlock);	/* init hash table lock */

	/*
	 * allocate vm_page structures.
	 */

	/*
	 * sanity check:
	 * before calling this function the MD code is expected to register
	 * some free RAM with the uvm_page_physload() function.   our job
	 * now is to allocate vm_page structures for this memory.
	 */

	if (vm_nphysseg == 0)
		panic("uvm_page_bootstrap: no memory pre-allocated");

	/*
	 * first calculate the number of free pages...
	 *
	 * note that we use start/end rather than avail_start/avail_end.
	 * this allows us to allocate extra vm_page structures in case we
	 * want to return some memory to the pool after booting.
	 */

	freepages = 0;
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
		freepages += (vm_physmem[lcv].end - vm_physmem[lcv].start);

	/*
	 * Let MD code initialize the number of colors, or default
	 * to 1 color if MD code doesn't care.
	 */
	if (uvmexp.ncolors == 0)
		uvmexp.ncolors = 1;
	uvmexp.colormask = uvmexp.ncolors - 1;

	/*
	 * we now know we have (PAGE_SIZE * freepages) bytes of memory we can
	 * use.   for each page of memory we use we need a vm_page structure.
	 * thus, the total number of pages we can use is the total size of
	 * the memory divided by the PAGE_SIZE plus the size of the vm_page
	 * structure.   we add one to freepages as a fudge factor to avoid
	 * truncation errors (since we can only allocate in terms of whole
	 * pages).
	 */

	bucketcount = uvmexp.ncolors * VM_NFREELIST;
	pagecount = ((freepages + 1) << PAGE_SHIFT) /
	    (PAGE_SIZE + sizeof(struct vm_page));

	bucketarray = (void *)uvm_pageboot_alloc((bucketcount *
	    sizeof(struct pgflbucket)) + (pagecount *
	    sizeof(struct vm_page)));
	pagearray = (struct vm_page *)(bucketarray + bucketcount);

	for (lcv = 0; lcv < VM_NFREELIST; lcv++) {
		uvm.page_free[lcv].pgfl_buckets =
		    (bucketarray + (lcv * uvmexp.ncolors));
		uvm_page_init_buckets(&uvm.page_free[lcv]);
	}
	memset(pagearray, 0, pagecount * sizeof(struct vm_page));

	/*
	 * init the vm_page structures and put them in the correct place.
	 */

	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++) {
		n = vm_physmem[lcv].end - vm_physmem[lcv].start;

		/* set up page array pointers */
		vm_physmem[lcv].pgs = pagearray;
		pagearray += n;
		pagecount -= n;
		vm_physmem[lcv].lastpg = vm_physmem[lcv].pgs + (n - 1);

		/* init and free vm_pages (we've already zeroed them) */
		paddr = ptoa(vm_physmem[lcv].start);
		for (i = 0 ; i < n ; i++, paddr += PAGE_SIZE) {
			vm_physmem[lcv].pgs[i].phys_addr = paddr;
#ifdef __HAVE_VM_PAGE_MD
			VM_MDPAGE_INIT(&vm_physmem[lcv].pgs[i]);
#endif
			if (atop(paddr) >= vm_physmem[lcv].avail_start &&
			    atop(paddr) <= vm_physmem[lcv].avail_end) {
				uvmexp.npages++;
				/* add page to free pool */
				uvm_pagefree(&vm_physmem[lcv].pgs[i]);
			}
		}
	}

	/*
	 * pass up the values of virtual_space_start and
	 * virtual_space_end (obtained by uvm_pageboot_alloc) to the upper
	 * layers of the VM.
	 */

	*kvm_startp = round_page(virtual_space_start);
	*kvm_endp = trunc_page(virtual_space_end);
#ifdef DEBUG
	/*
	 * steal kva for uvm_pagezerocheck().
	 */
	uvm_zerocheckkva = *kvm_startp;
	*kvm_startp += PAGE_SIZE;
#endif /* DEBUG */

	/*
	 * init locks for kernel threads
	 */

	simple_lock_init(&uvm.pagedaemon_lock);
	simple_lock_init(&uvm.aiodoned_lock);

	/*
	 * init various thresholds.
	 */

	uvmexp.reserve_pagedaemon = 1;
	uvmexp.reserve_kernel = 5;
	uvmexp.anonminpct = 10;
	uvmexp.fileminpct = 10;
	uvmexp.execminpct = 5;
	uvmexp.anonmaxpct = 80;
	uvmexp.filemaxpct = 50;
	uvmexp.execmaxpct = 30;
	uvmexp.anonmin = uvmexp.anonminpct * 256 / 100;
	uvmexp.filemin = uvmexp.fileminpct * 256 / 100;
	uvmexp.execmin = uvmexp.execminpct * 256 / 100;
	uvmexp.anonmax = uvmexp.anonmaxpct * 256 / 100;
	uvmexp.filemax = uvmexp.filemaxpct * 256 / 100;
	uvmexp.execmax = uvmexp.execmaxpct * 256 / 100;

	/*
	 * determine if we should zero pages in the idle loop.
	 */

	uvm.page_idle_zero = vm_page_zero_enable;

	/*
	 * done!
	 */

	uvm.page_init_done = TRUE;
}

/*
 * uvm_setpagesize: set the page size
 *
 * => sets page_shift and page_mask from uvmexp.pagesize.
 */

void
uvm_setpagesize()
{

	/*
	 * If uvmexp.pagesize is 0 at this point, we expect PAGE_SIZE
	 * to be a constant (indicated by being a non-zero value).
	 */
	if (uvmexp.pagesize == 0) {
		if (PAGE_SIZE == 0)
			panic("uvm_setpagesize: uvmexp.pagesize not set");
		uvmexp.pagesize = PAGE_SIZE;
	}
	uvmexp.pagemask = uvmexp.pagesize - 1;
	if ((uvmexp.pagemask & uvmexp.pagesize) != 0)
		panic("uvm_setpagesize: page size not a power of two");
	for (uvmexp.pageshift = 0; ; uvmexp.pageshift++)
		if ((1 << uvmexp.pageshift) == uvmexp.pagesize)
			break;
}

/*
 * uvm_pageboot_alloc: steal memory from physmem for bootstrapping
 */

vaddr_t
uvm_pageboot_alloc(size)
	vsize_t size;
{
	static boolean_t initialized = FALSE;
	vaddr_t addr;
#if !defined(PMAP_STEAL_MEMORY)
	vaddr_t vaddr;
	paddr_t paddr;
#endif

	/*
	 * on first call to this function, initialize ourselves.
	 */
	if (initialized == FALSE) {
		pmap_virtual_space(&virtual_space_start, &virtual_space_end);

		/* round it the way we like it */
		virtual_space_start = round_page(virtual_space_start);
		virtual_space_end = trunc_page(virtual_space_end);

		initialized = TRUE;
	}

	/* round to page size */
	size = round_page(size);

#if defined(PMAP_STEAL_MEMORY)

	/*
	 * defer bootstrap allocation to MD code (it may want to allocate
	 * from a direct-mapped segment).  pmap_steal_memory should adjust
	 * virtual_space_start/virtual_space_end if necessary.
	 */

	addr = pmap_steal_memory(size, &virtual_space_start,
	    &virtual_space_end);

	return(addr);

#else /* !PMAP_STEAL_MEMORY */

	/*
	 * allocate virtual memory for this request
	 */
	if (virtual_space_start == virtual_space_end ||
	    (virtual_space_end - virtual_space_start) < size)
		panic("uvm_pageboot_alloc: out of virtual space");

	addr = virtual_space_start;

#ifdef PMAP_GROWKERNEL
	/*
	 * If the kernel pmap can't map the requested space,
	 * then allocate more resources for it.
	 */
	if (uvm_maxkaddr < (addr + size)) {
		uvm_maxkaddr = pmap_growkernel(addr + size);
		if (uvm_maxkaddr < (addr + size))
			panic("uvm_pageboot_alloc: pmap_growkernel() failed");
	}
#endif

	virtual_space_start += size;

	/*
	 * allocate and mapin physical pages to back new virtual pages
	 */

	for (vaddr = round_page(addr) ; vaddr < addr + size ;
	    vaddr += PAGE_SIZE) {

		if (!uvm_page_physget(&paddr))
			panic("uvm_pageboot_alloc: out of memory");

		/*
		 * Note this memory is no longer managed, so using
		 * pmap_kenter is safe.
		 */
		pmap_kenter_pa(vaddr, paddr, VM_PROT_READ|VM_PROT_WRITE);
	}
	pmap_update(pmap_kernel());
	return(addr);
#endif	/* PMAP_STEAL_MEMORY */
}

#if !defined(PMAP_STEAL_MEMORY)
/*
 * uvm_page_physget: "steal" one page from the vm_physmem structure.
 *
 * => attempt to allocate it off the end of a segment in which the "avail"
 *    values match the start/end values.   if we can't do that, then we
 *    will advance both values (making them equal, and removing some
 *    vm_page structures from the non-avail area).
 * => return false if out of memory.
 */

/* subroutine: try to allocate from memory chunks on the specified freelist */
static boolean_t uvm_page_physget_freelist(paddr_t *, int);

static boolean_t
uvm_page_physget_freelist(paddrp, freelist)
	paddr_t *paddrp;
	int freelist;
{
	int lcv, x;

	/* pass 1: try allocating from a matching end */
#if (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST)
	for (lcv = vm_nphysseg - 1 ; lcv >= 0 ; lcv--)
#else
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
#endif
	{

		if (uvm.page_init_done == TRUE)
			panic("uvm_page_physget: called _after_ bootstrap");

		if (vm_physmem[lcv].free_list != freelist)
			continue;

		/* try from front */
		if (vm_physmem[lcv].avail_start == vm_physmem[lcv].start &&
		    vm_physmem[lcv].avail_start < vm_physmem[lcv].avail_end) {
			*paddrp = ptoa(vm_physmem[lcv].avail_start);
			vm_physmem[lcv].avail_start++;
			vm_physmem[lcv].start++;
			/* nothing left?   nuke it */
			if (vm_physmem[lcv].avail_start ==
			    vm_physmem[lcv].end) {
				if (vm_nphysseg == 1)
				    panic("uvm_page_physget: out of memory!");
				vm_nphysseg--;
				for (x = lcv ; x < vm_nphysseg ; x++)
					/* structure copy */
					vm_physmem[x] = vm_physmem[x+1];
			}
			return (TRUE);
		}

		/* try from rear */
		if (vm_physmem[lcv].avail_end == vm_physmem[lcv].end &&
		    vm_physmem[lcv].avail_start < vm_physmem[lcv].avail_end) {
			*paddrp = ptoa(vm_physmem[lcv].avail_end - 1);
			vm_physmem[lcv].avail_end--;
			vm_physmem[lcv].end--;
			/* nothing left?   nuke it */
			if (vm_physmem[lcv].avail_end ==
			    vm_physmem[lcv].start) {
				if (vm_nphysseg == 1)
				    panic("uvm_page_physget: out of memory!");
				vm_nphysseg--;
				for (x = lcv ; x < vm_nphysseg ; x++)
					/* structure copy */
					vm_physmem[x] = vm_physmem[x+1];
			}
			return (TRUE);
		}
	}

	/* pass2: forget about matching ends, just allocate something */
#if (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST)
	for (lcv = vm_nphysseg - 1 ; lcv >= 0 ; lcv--)
#else
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
#endif
	{

		/* any room in this bank? */
		if (vm_physmem[lcv].avail_start >= vm_physmem[lcv].avail_end)
			continue;  /* nope */

		*paddrp = ptoa(vm_physmem[lcv].avail_start);
		vm_physmem[lcv].avail_start++;
		/* truncate! */
		vm_physmem[lcv].start = vm_physmem[lcv].avail_start;

		/* nothing left?   nuke it */
		if (vm_physmem[lcv].avail_start == vm_physmem[lcv].end) {
			if (vm_nphysseg == 1)
				panic("uvm_page_physget: out of memory!");
			vm_nphysseg--;
			for (x = lcv ; x < vm_nphysseg ; x++)
				/* structure copy */
				vm_physmem[x] = vm_physmem[x+1];
		}
		return (TRUE);
	}

	return (FALSE);        /* whoops! */
}

boolean_t
uvm_page_physget(paddrp)
	paddr_t *paddrp;
{
	int i;

	/* try in the order of freelist preference */
	for (i = 0; i < VM_NFREELIST; i++)
		if (uvm_page_physget_freelist(paddrp, i) == TRUE)
			return (TRUE);
	return (FALSE);
}
#endif /* PMAP_STEAL_MEMORY */

/*
 * uvm_page_physload: load physical memory into VM system
 *
 * => all args are PFs
 * => all pages in start/end get vm_page structures
 * => areas marked by avail_start/avail_end get added to the free page pool
 * => we are limited to VM_PHYSSEG_MAX physical memory segments
 */

void
uvm_page_physload(start, end, avail_start, avail_end, free_list)
	paddr_t start, end, avail_start, avail_end;
	int free_list;
{
	int preload, lcv;
	psize_t npages;
	struct vm_page *pgs;
	struct vm_physseg *ps;

	if (uvmexp.pagesize == 0)
		panic("uvm_page_physload: page size not set!");
	if (free_list >= VM_NFREELIST || free_list < VM_FREELIST_DEFAULT)
		panic("uvm_page_physload: bad free list %d", free_list);
	if (start >= end)
		panic("uvm_page_physload: start >= end");

	/*
	 * do we have room?
	 */

	if (vm_nphysseg == VM_PHYSSEG_MAX) {
		printf("uvm_page_physload: unable to load physical memory "
		    "segment\n");
		printf("\t%d segments allocated, ignoring 0x%llx -> 0x%llx\n",
		    VM_PHYSSEG_MAX, (long long)start, (long long)end);
		printf("\tincrease VM_PHYSSEG_MAX\n");
		return;
	}

	/*
	 * check to see if this is a "preload" (i.e. uvm_mem_init hasn't been
	 * called yet, so malloc is not available).
	 */

	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++) {
		if (vm_physmem[lcv].pgs)
			break;
	}
	preload = (lcv == vm_nphysseg);

	/*
	 * if VM is already running, attempt to malloc() vm_page structures
	 */

	if (!preload) {
#if defined(VM_PHYSSEG_NOADD)
		panic("uvm_page_physload: tried to add RAM after vm_mem_init");
#else
		/* XXXCDC: need some sort of lockout for this case */
		paddr_t paddr;
		npages = end - start;  /* # of pages */
		pgs = malloc(sizeof(struct vm_page) * npages,
		    M_VMPAGE, M_NOWAIT);
		if (pgs == NULL) {
			printf("uvm_page_physload: can not malloc vm_page "
			    "structs for segment\n");
			printf("\tignoring 0x%lx -> 0x%lx\n", start, end);
			return;
		}
		/* zero data, init phys_addr and free_list, and free pages */
		memset(pgs, 0, sizeof(struct vm_page) * npages);
		for (lcv = 0, paddr = ptoa(start) ;
				 lcv < npages ; lcv++, paddr += PAGE_SIZE) {
			pgs[lcv].phys_addr = paddr;
			pgs[lcv].free_list = free_list;
			if (atop(paddr) >= avail_start &&
			    atop(paddr) <= avail_end)
				uvm_pagefree(&pgs[lcv]);
		}
		/* XXXCDC: incomplete: need to update uvmexp.free, what else? */
		/* XXXCDC: need hook to tell pmap to rebuild pv_list, etc... */
#endif
	} else {
		pgs = NULL;
		npages = 0;
	}

	/*
	 * now insert us in the proper place in vm_physmem[]
	 */

#if (VM_PHYSSEG_STRAT == VM_PSTRAT_RANDOM)
	/* random: put it at the end (easy!) */
	ps = &vm_physmem[vm_nphysseg];
#elif (VM_PHYSSEG_STRAT == VM_PSTRAT_BSEARCH)
	{
		int x;
		/* sort by address for binary search */
		for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
			if (start < vm_physmem[lcv].start)
				break;
		ps = &vm_physmem[lcv];
		/* move back other entries, if necessary ... */
		for (x = vm_nphysseg ; x > lcv ; x--)
			/* structure copy */
			vm_physmem[x] = vm_physmem[x - 1];
	}
#elif (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST)
	{
		int x;
		/* sort by largest segment first */
		for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
			if ((end - start) >
			    (vm_physmem[lcv].end - vm_physmem[lcv].start))
				break;
		ps = &vm_physmem[lcv];
		/* move back other entries, if necessary ... */
		for (x = vm_nphysseg ; x > lcv ; x--)
			/* structure copy */
			vm_physmem[x] = vm_physmem[x - 1];
	}
#else
	panic("uvm_page_physload: unknown physseg strategy selected!");
#endif

	ps->start = start;
	ps->end = end;
	ps->avail_start = avail_start;
	ps->avail_end = avail_end;
	if (preload) {
		ps->pgs = NULL;
	} else {
		ps->pgs = pgs;
		ps->lastpg = pgs + npages - 1;
	}
	ps->free_list = free_list;
	vm_nphysseg++;

	if (!preload)
		uvm_page_rehash();
}

/*
 * uvm_page_rehash: reallocate hash table based on number of free pages.
 */

void
uvm_page_rehash()
{
	int freepages, lcv, bucketcount, oldcount;
	struct pglist *newbuckets, *oldbuckets;
	struct vm_page *pg;
	size_t newsize, oldsize;

	/*
	 * compute number of pages that can go in the free pool
	 */

	freepages = 0;
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
		freepages +=
		    (vm_physmem[lcv].avail_end - vm_physmem[lcv].avail_start);

	/*
	 * compute number of buckets needed for this number of pages
	 */

	bucketcount = 1;
	while (bucketcount < freepages)
		bucketcount = bucketcount * 2;

	/*
	 * compute the size of the current table and new table.
	 */

	oldbuckets = uvm.page_hash;
	oldcount = uvm.page_nhash;
	oldsize = round_page(sizeof(struct pglist) * oldcount);
	newsize = round_page(sizeof(struct pglist) * bucketcount);

	/*
	 * allocate the new buckets
	 */

	newbuckets = (struct pglist *) uvm_km_alloc(kernel_map, newsize);
	if (newbuckets == NULL) {
		printf("uvm_page_physrehash: WARNING: could not grow page "
		    "hash table\n");
		return;
	}
	for (lcv = 0 ; lcv < bucketcount ; lcv++)
		TAILQ_INIT(&newbuckets[lcv]);

	/*
	 * now replace the old buckets with the new ones and rehash everything
	 */

	simple_lock(&uvm.hashlock);
	uvm.page_hash = newbuckets;
	uvm.page_nhash = bucketcount;
	uvm.page_hashmask = bucketcount - 1;  /* power of 2 */

	/* ... and rehash */
	for (lcv = 0 ; lcv < oldcount ; lcv++) {
		while ((pg = oldbuckets[lcv].tqh_first) != NULL) {
			TAILQ_REMOVE(&oldbuckets[lcv], pg, hashq);
			TAILQ_INSERT_TAIL(
			  &uvm.page_hash[uvm_pagehash(pg->uobject, pg->offset)],
			  pg, hashq);
		}
	}
	simple_unlock(&uvm.hashlock);

	/*
	 * free old bucket array if is not the boot-time table
	 */

	if (oldbuckets != &uvm_bootbucket)
		uvm_km_free(kernel_map, (vaddr_t) oldbuckets, oldsize);
}

/*
 * uvm_page_recolor: Recolor the pages if the new bucket count is
 * larger than the old one.
 */

void
uvm_page_recolor(int newncolors)
{
	struct pgflbucket *bucketarray, *oldbucketarray;
	struct pgfreelist pgfl;
	struct vm_page *pg;
	vsize_t bucketcount;
	int s, lcv, color, i, ocolors;

	if (newncolors <= uvmexp.ncolors)
		return;

	if (uvm.page_init_done == FALSE) {
		uvmexp.ncolors = newncolors;
		return;
	}

	bucketcount = newncolors * VM_NFREELIST;
	bucketarray = malloc(bucketcount * sizeof(struct pgflbucket),
	    M_VMPAGE, M_NOWAIT);
	if (bucketarray == NULL) {
		printf("WARNING: unable to allocate %ld page color buckets\n",
		    (long) bucketcount);
		return;
	}

	s = uvm_lock_fpageq();

	/* Make sure we should still do this. */
	if (newncolors <= uvmexp.ncolors) {
		uvm_unlock_fpageq(s);
		free(bucketarray, M_VMPAGE);
		return;
	}

	oldbucketarray = uvm.page_free[0].pgfl_buckets;
	ocolors = uvmexp.ncolors;

	uvmexp.ncolors = newncolors;
	uvmexp.colormask = uvmexp.ncolors - 1;

	for (lcv = 0; lcv < VM_NFREELIST; lcv++) {
		pgfl.pgfl_buckets = (bucketarray + (lcv * newncolors));
		uvm_page_init_buckets(&pgfl);
		for (color = 0; color < ocolors; color++) {
			for (i = 0; i < PGFL_NQUEUES; i++) {
				while ((pg = TAILQ_FIRST(&uvm.page_free[
				    lcv].pgfl_buckets[color].pgfl_queues[i]))
				    != NULL) {
					TAILQ_REMOVE(&uvm.page_free[
					    lcv].pgfl_buckets[
					    color].pgfl_queues[i], pg, pageq);
					TAILQ_INSERT_TAIL(&pgfl.pgfl_buckets[
					    VM_PGCOLOR_BUCKET(pg)].pgfl_queues[
					    i], pg, pageq);
				}
			}
		}
		uvm.page_free[lcv].pgfl_buckets = pgfl.pgfl_buckets;
	}

	if (have_recolored_pages) {
		uvm_unlock_fpageq(s);
		free(oldbucketarray, M_VMPAGE);
		return;
	}

	have_recolored_pages = TRUE;
	uvm_unlock_fpageq(s);
}

/*
 * uvm_pagealloc_pgfl: helper routine for uvm_pagealloc_strat
 */

static __inline struct vm_page *
uvm_pagealloc_pgfl(struct pgfreelist *pgfl, int try1, int try2,
    int *trycolorp)
{
	struct pglist *freeq;
	struct vm_page *pg;
	int color, trycolor = *trycolorp;

	color = trycolor;
	do {
		if ((pg = TAILQ_FIRST((freeq =
		    &pgfl->pgfl_buckets[color].pgfl_queues[try1]))) != NULL)
			goto gotit;
		if ((pg = TAILQ_FIRST((freeq =
		    &pgfl->pgfl_buckets[color].pgfl_queues[try2]))) != NULL)
			goto gotit;
		color = (color + 1) & uvmexp.colormask;
	} while (color != trycolor);

	return (NULL);

 gotit:
	TAILQ_REMOVE(freeq, pg, pageq);
	uvmexp.free--;

	/* update zero'd page count */
	if (pg->flags & PG_ZERO)
		uvmexp.zeropages--;

	if (color == trycolor)
		uvmexp.colorhit++;
	else {
		uvmexp.colormiss++;
		*trycolorp = color;
	}

	return (pg);
}

/*
 * uvm_pagealloc_strat: allocate vm_page from a particular free list.
 *
 * => return null if no pages free
 * => wake up pagedaemon if number of free pages drops below low water mark
 * => if obj != NULL, obj must be locked (to put in hash)
 * => if anon != NULL, anon must be locked (to put in anon)
 * => only one of obj or anon can be non-null
 * => caller must activate/deactivate page if it is not wired.
 * => free_list is ignored if strat == UVM_PGA_STRAT_NORMAL.
 * => policy decision: it is more important to pull a page off of the
 *	appropriate priority free list than it is to get a zero'd or
 *	unknown contents page.  This is because we live with the
 *	consequences of a bad free list decision for the entire
 *	lifetime of the page, e.g. if the page comes from memory that
 *	is slower to access.
 */

struct vm_page *
uvm_pagealloc_strat(obj, off, anon, flags, strat, free_list)
	struct uvm_object *obj;
	voff_t off;
	int flags;
	struct vm_anon *anon;
	int strat, free_list;
{
	int lcv, try1, try2, s, zeroit = 0, color;
	struct vm_page *pg;
	boolean_t use_reserve;

	KASSERT(obj == NULL || anon == NULL);
	KASSERT(off == trunc_page(off));
	LOCK_ASSERT(obj == NULL || simple_lock_held(&obj->vmobjlock));
	LOCK_ASSERT(anon == NULL || simple_lock_held(&anon->an_lock));

	s = uvm_lock_fpageq();

	/*
	 * This implements a global round-robin page coloring
	 * algorithm.
	 *
	 * XXXJRT: Should we make the `nextcolor' per-CPU?
	 * XXXJRT: What about virtually-indexed caches?
	 */

	color = uvm.page_free_nextcolor;

	/*
	 * check to see if we need to generate some free pages waking
	 * the pagedaemon.
	 */

	UVM_KICK_PDAEMON();

	/*
	 * fail if any of these conditions is true:
	 * [1]  there really are no free pages, or
	 * [2]  only kernel "reserved" pages remain and
	 *        the page isn't being allocated to a kernel object.
	 * [3]  only pagedaemon "reserved" pages remain and
	 *        the requestor isn't the pagedaemon.
	 */

	use_reserve = (flags & UVM_PGA_USERESERVE) ||
		(obj && UVM_OBJ_IS_KERN_OBJECT(obj));
	if ((uvmexp.free <= uvmexp.reserve_kernel && !use_reserve) ||
	    (uvmexp.free <= uvmexp.reserve_pagedaemon &&
	     !(use_reserve && curproc == uvm.pagedaemon_proc)))
		goto fail;

#if PGFL_NQUEUES != 2
#error uvm_pagealloc_strat needs to be updated
#endif

	/*
	 * If we want a zero'd page, try the ZEROS queue first, otherwise
	 * we try the UNKNOWN queue first.
	 */
	if (flags & UVM_PGA_ZERO) {
		try1 = PGFL_ZEROS;
		try2 = PGFL_UNKNOWN;
	} else {
		try1 = PGFL_UNKNOWN;
		try2 = PGFL_ZEROS;
	}

 again:
	switch (strat) {
	case UVM_PGA_STRAT_NORMAL:
		/* Check all freelists in descending priority order. */
		for (lcv = 0; lcv < VM_NFREELIST; lcv++) {
			pg = uvm_pagealloc_pgfl(&uvm.page_free[lcv],
			    try1, try2, &color);
			if (pg != NULL)
				goto gotit;
		}

		/* No pages free! */
		goto fail;

	case UVM_PGA_STRAT_ONLY:
	case UVM_PGA_STRAT_FALLBACK:
		/* Attempt to allocate from the specified free list. */
		KASSERT(free_list >= 0 && free_list < VM_NFREELIST);
		pg = uvm_pagealloc_pgfl(&uvm.page_free[free_list],
		    try1, try2, &color);
		if (pg != NULL)
			goto gotit;

		/* Fall back, if possible. */
		if (strat == UVM_PGA_STRAT_FALLBACK) {
			strat = UVM_PGA_STRAT_NORMAL;
			goto again;
		}

		/* No pages free! */
		goto fail;

	default:
		panic("uvm_pagealloc_strat: bad strat %d", strat);
		/* NOTREACHED */
	}

 gotit:
	/*
	 * We now know which color we actually allocated from; set
	 * the next color accordingly.
	 */

	uvm.page_free_nextcolor = (color + 1) & uvmexp.colormask;

	/*
	 * update allocation statistics and remember if we have to
	 * zero the page
	 */

	if (flags & UVM_PGA_ZERO) {
		if (pg->flags & PG_ZERO) {
			uvmexp.pga_zerohit++;
			zeroit = 0;
		} else {
			uvmexp.pga_zeromiss++;
			zeroit = 1;
		}
	}
	uvm_unlock_fpageq(s);

	pg->offset = off;
	pg->uobject = obj;
	pg->uanon = anon;
	pg->flags = PG_BUSY|PG_CLEAN|PG_FAKE;
	if (anon) {
		anon->u.an_page = pg;
		pg->pqflags = PQ_ANON;
		uvmexp.anonpages++;
	} else {
		if (obj) {
			uvm_pageinsert(pg);
		}
		pg->pqflags = 0;
	}
#if defined(UVM_PAGE_TRKOWN)
	pg->owner_tag = NULL;
#endif
	UVM_PAGE_OWN(pg, "new alloc");

	if (flags & UVM_PGA_ZERO) {
		/*
		 * A zero'd page is not clean.  If we got a page not already
		 * zero'd, then we have to zero it ourselves.
		 */
		pg->flags &= ~PG_CLEAN;
		if (zeroit)
			pmap_zero_page(VM_PAGE_TO_PHYS(pg));
	}

	return(pg);

 fail:
	uvm_unlock_fpageq(s);
	return (NULL);
}

/*
 * uvm_pagereplace: replace a page with another
 *
 * => object must be locked
 */

void
uvm_pagereplace(oldpg, newpg)
	struct vm_page *oldpg;
	struct vm_page *newpg;
{

	KASSERT((oldpg->flags & PG_TABLED) != 0);
	KASSERT(oldpg->uobject != NULL);
	KASSERT((newpg->flags & PG_TABLED) == 0);
	KASSERT(newpg->uobject == NULL);
	LOCK_ASSERT(simple_lock_held(&oldpg->uobject->vmobjlock));

	newpg->uobject = oldpg->uobject;
	newpg->offset = oldpg->offset;

	uvm_pageinsert_after(newpg, oldpg);
	uvm_pageremove(oldpg);
}

/*
 * uvm_pagerealloc: reallocate a page from one object to another
 *
 * => both objects must be locked
 */

void
uvm_pagerealloc(pg, newobj, newoff)
	struct vm_page *pg;
	struct uvm_object *newobj;
	voff_t newoff;
{
	/*
	 * remove it from the old object
	 */

	if (pg->uobject) {
		uvm_pageremove(pg);
	}

	/*
	 * put it in the new object
	 */

	if (newobj) {
		pg->uobject = newobj;
		pg->offset = newoff;
		uvm_pageinsert(pg);
	}
}

#ifdef DEBUG
/*
 * check if page is zero-filled
 *
 *  - called with free page queue lock held.
 */
void
uvm_pagezerocheck(struct vm_page *pg)
{
	int *p, *ep;

	KASSERT(uvm_zerocheckkva != 0);
	LOCK_ASSERT(simple_lock_held(&uvm.fpageqlock));

	/*
	 * XXX assuming pmap_kenter_pa and pmap_kremove never call
	 * uvm page allocator.
	 *
	 * it might be better to have "CPU-local temporary map" pmap interface.
	 */
	pmap_kenter_pa(uvm_zerocheckkva, VM_PAGE_TO_PHYS(pg), VM_PROT_READ);
	p = (int *)uvm_zerocheckkva;
	ep = (int *)((char *)p + PAGE_SIZE);
	pmap_update(pmap_kernel());
	while (p < ep) {
		if (*p != 0)
			panic("PG_ZERO page isn't zero-filled");
		p++;
	}
	pmap_kremove(uvm_zerocheckkva, PAGE_SIZE);
}
#endif /* DEBUG */

/*
 * uvm_pagefree: free page
 *
 * => erase page's identity (i.e. remove from hash/object)
 * => put page on free list
 * => caller must lock owning object (either anon or uvm_object)
 * => caller must lock page queues
 * => assumes all valid mappings of pg are gone
 */

void
uvm_pagefree(pg)
	struct vm_page *pg;
{
	int s;
	struct pglist *pgfl;
	boolean_t iszero;

	KASSERT((pg->flags & PG_PAGEOUT) == 0);
	LOCK_ASSERT(simple_lock_held(&uvm.pageqlock) ||
		    (pg->pqflags & (PQ_ACTIVE|PQ_INACTIVE)) == 0);
	LOCK_ASSERT(pg->uobject == NULL ||
		    simple_lock_held(&pg->uobject->vmobjlock));
	LOCK_ASSERT(pg->uobject != NULL || pg->uanon == NULL ||
		    simple_lock_held(&pg->uanon->an_lock));

#ifdef DEBUG
	if (pg->uobject == (void *)0xdeadbeef &&
	    pg->uanon == (void *)0xdeadbeef) {
		panic("uvm_pagefree: freeing free page %p", pg);
	}
#endif /* DEBUG */

	/*
	 * if the page is loaned, resolve the loan instead of freeing.
	 */

	if (pg->loan_count) {
		KASSERT(pg->wire_count == 0);

		/*
		 * if the page is owned by an anon then we just want to
		 * drop anon ownership.  the kernel will free the page when
		 * it is done with it.  if the page is owned by an object,
		 * remove it from the object and mark it dirty for the benefit
		 * of possible anon owners.
		 *
		 * regardless of previous ownership, wakeup any waiters,
		 * unbusy the page, and we're done.
		 */

		if (pg->uobject != NULL) {
			uvm_pageremove(pg);
			pg->flags &= ~PG_CLEAN;
		} else if (pg->uanon != NULL) {
			if ((pg->pqflags & PQ_ANON) == 0) {
				pg->loan_count--;
			} else {
				pg->pqflags &= ~PQ_ANON;
				uvmexp.anonpages--;
			}
			pg->uanon->u.an_page = NULL;
			pg->uanon = NULL;
		}
		if (pg->flags & PG_WANTED) {
			wakeup(pg);
		}
		pg->flags &= ~(PG_WANTED|PG_BUSY|PG_RELEASED|PG_PAGER1);
#ifdef UVM_PAGE_TRKOWN
		pg->owner_tag = NULL;
#endif
		if (pg->loan_count) {
			uvm_pagedequeue(pg);
			return;
		}
	}

	/*
	 * remove page from its object or anon.
	 */

	if (pg->uobject != NULL) {
		uvm_pageremove(pg);
	} else if (pg->uanon != NULL) {
		pg->uanon->u.an_page = NULL;
		uvmexp.anonpages--;
	}

	/*
	 * now remove the page from the queues.
	 */

	uvm_pagedequeue(pg);

	/*
	 * if the page was wired, unwire it now.
	 */

	if (pg->wire_count) {
		pg->wire_count = 0;
		uvmexp.wired--;
	}

	/*
	 * and put on free queue
	 */

	iszero = (pg->flags & PG_ZERO);
	pgfl = &uvm.page_free[uvm_page_lookup_freelist(pg)].
	    pgfl_buckets[VM_PGCOLOR_BUCKET(pg)].
	    pgfl_queues[iszero ? PGFL_ZEROS : PGFL_UNKNOWN];

	pg->pqflags = PQ_FREE;
#ifdef DEBUG
	pg->uobject = (void *)0xdeadbeef;
	pg->offset = 0xdeadbeef;
	pg->uanon = (void *)0xdeadbeef;
#endif

	s = uvm_lock_fpageq();

#ifdef DEBUG
	if (iszero)
		uvm_pagezerocheck(pg);
#endif /* DEBUG */

	TAILQ_INSERT_TAIL(pgfl, pg, pageq);
	uvmexp.free++;
	if (iszero)
		uvmexp.zeropages++;

	if (uvmexp.zeropages < UVM_PAGEZERO_TARGET)
		uvm.page_idle_zero = vm_page_zero_enable;

	uvm_unlock_fpageq(s);
}

/*
 * uvm_page_unbusy: unbusy an array of pages.
 *
 * => pages must either all belong to the same object, or all belong to anons.
 * => if pages are object-owned, object must be locked.
 * => if pages are anon-owned, anons must be locked.
 * => caller must lock page queues if pages may be released.
 * => caller must make sure that anon-owned pages are not PG_RELEASED.
 */

void
uvm_page_unbusy(pgs, npgs)
	struct vm_page **pgs;
	int npgs;
{
	struct vm_page *pg;
	int i;
	UVMHIST_FUNC("uvm_page_unbusy"); UVMHIST_CALLED(ubchist);

	for (i = 0; i < npgs; i++) {
		pg = pgs[i];
		if (pg == NULL || pg == PGO_DONTCARE) {
			continue;
		}

		LOCK_ASSERT(pg->uobject == NULL ||
		    simple_lock_held(&pg->uobject->vmobjlock));
		LOCK_ASSERT(pg->uobject != NULL ||
		    (pg->uanon != NULL &&
		    simple_lock_held(&pg->uanon->an_lock)));

		KASSERT(pg->flags & PG_BUSY);
		KASSERT((pg->flags & PG_PAGEOUT) == 0);
		if (pg->flags & PG_WANTED) {
			wakeup(pg);
		}
		if (pg->flags & PG_RELEASED) {
			UVMHIST_LOG(ubchist, "releasing pg %p", pg,0,0,0);
			KASSERT(pg->uobject != NULL ||
			    (pg->uanon != NULL && pg->uanon->an_ref > 0));
			pg->flags &= ~PG_RELEASED;
			uvm_pagefree(pg);
		} else {
			UVMHIST_LOG(ubchist, "unbusying pg %p", pg,0,0,0);
			pg->flags &= ~(PG_WANTED|PG_BUSY);
			UVM_PAGE_OWN(pg, NULL);
		}
	}
}

#if defined(UVM_PAGE_TRKOWN)
/*
 * uvm_page_own: set or release page ownership
 *
 * => this is a debugging function that keeps track of who sets PG_BUSY
 *	and where they do it.   it can be used to track down problems
 *	such a process setting "PG_BUSY" and never releasing it.
 * => page's object [if any] must be locked
 * => if "tag" is NULL then we are releasing page ownership
 */
void
uvm_page_own(pg, tag)
	struct vm_page *pg;
	char *tag;
{
	KASSERT((pg->flags & (PG_PAGEOUT|PG_RELEASED)) == 0);

	/* gain ownership? */
	if (tag) {
		if (pg->owner_tag) {
			printf("uvm_page_own: page %p already owned "
			    "by proc %d [%s]\n", pg,
			    pg->owner, pg->owner_tag);
			panic("uvm_page_own");
		}
		pg->owner = (curproc) ? curproc->p_pid :  (pid_t) -1;
		pg->owner_tag = tag;
		return;
	}

	/* drop ownership */
	if (pg->owner_tag == NULL) {
		printf("uvm_page_own: dropping ownership of an non-owned "
		    "page (%p)\n", pg);
		panic("uvm_page_own");
	}
	KASSERT((pg->pqflags & (PQ_ACTIVE|PQ_INACTIVE)) ||
	    (pg->uanon == NULL && pg->uobject == NULL) ||
	    pg->uobject == uvm.kernel_object ||
	    pg->wire_count > 0 ||
	    (pg->loan_count == 1 && pg->uanon == NULL) ||
	    pg->loan_count > 1);
	pg->owner_tag = NULL;
}
#endif

/*
 * uvm_pageidlezero: zero free pages while the system is idle.
 *
 * => try to complete one color bucket at a time, to reduce our impact
 *	on the CPU cache.
 * => we loop until we either reach the target or whichqs indicates that
 *	there is a process ready to run.
 */
void
uvm_pageidlezero()
{
	struct vm_page *pg;
	struct pgfreelist *pgfl;
	int free_list, s, firstbucket;
	static int nextbucket;

	s = uvm_lock_fpageq();
	firstbucket = nextbucket;
	do {
		if (sched_whichqs != 0) {
			uvm_unlock_fpageq(s);
			return;
		}
		if (uvmexp.zeropages >= UVM_PAGEZERO_TARGET) {
			uvm.page_idle_zero = FALSE;
			uvm_unlock_fpageq(s);
			return;
		}
		for (free_list = 0; free_list < VM_NFREELIST; free_list++) {
			pgfl = &uvm.page_free[free_list];
			while ((pg = TAILQ_FIRST(&pgfl->pgfl_buckets[
			    nextbucket].pgfl_queues[PGFL_UNKNOWN])) != NULL) {
				if (sched_whichqs != 0) {
					uvm_unlock_fpageq(s);
					return;
				}

				TAILQ_REMOVE(&pgfl->pgfl_buckets[
				    nextbucket].pgfl_queues[PGFL_UNKNOWN],
				    pg, pageq);
				uvmexp.free--;
				uvm_unlock_fpageq(s);
#ifdef PMAP_PAGEIDLEZERO
				if (!PMAP_PAGEIDLEZERO(VM_PAGE_TO_PHYS(pg))) {

					/*
					 * The machine-dependent code detected
					 * some reason for us to abort zeroing
					 * pages, probably because there is a
					 * process now ready to run.
					 */

					s = uvm_lock_fpageq();
					TAILQ_INSERT_HEAD(&pgfl->pgfl_buckets[
					    nextbucket].pgfl_queues[
					    PGFL_UNKNOWN], pg, pageq);
					uvmexp.free++;
					uvmexp.zeroaborts++;
					uvm_unlock_fpageq(s);
					return;
				}
#else
				pmap_zero_page(VM_PAGE_TO_PHYS(pg));
#endif /* PMAP_PAGEIDLEZERO */
				pg->flags |= PG_ZERO;

				s = uvm_lock_fpageq();
				TAILQ_INSERT_HEAD(&pgfl->pgfl_buckets[
				    nextbucket].pgfl_queues[PGFL_ZEROS],
				    pg, pageq);
				uvmexp.free++;
				uvmexp.zeropages++;
			}
		}
		nextbucket = (nextbucket + 1) & uvmexp.colormask;
	} while (nextbucket != firstbucket);
	uvm_unlock_fpageq(s);
}
