/*	$NetBSD: uvm_km.c,v 1.62.2.5 2005/03/04 16:55:00 skrll Exp $	*/

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
 *	@(#)vm_kern.c   8.3 (Berkeley) 1/12/94
 * from: Id: uvm_km.c,v 1.1.2.14 1998/02/06 05:19:27 chs Exp
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
 * uvm_km.c: handle kernel memory allocation and management
 */

/*
 * overview of kernel memory management:
 *
 * the kernel virtual address space is mapped by "kernel_map."   kernel_map
 * starts at VM_MIN_KERNEL_ADDRESS and goes to VM_MAX_KERNEL_ADDRESS.
 * note that VM_MIN_KERNEL_ADDRESS is equal to vm_map_min(kernel_map).
 *
 * the kernel_map has several "submaps."   submaps can only appear in
 * the kernel_map (user processes can't use them).   submaps "take over"
 * the management of a sub-range of the kernel's address space.  submaps
 * are typically allocated at boot time and are never released.   kernel
 * virtual address space that is mapped by a submap is locked by the
 * submap's lock -- not the kernel_map's lock.
 *
 * thus, the useful feature of submaps is that they allow us to break
 * up the locking and protection of the kernel address space into smaller
 * chunks.
 *
 * the vm system has several standard kernel submaps, including:
 *   kmem_map => contains only wired kernel memory for the kernel
 *		malloc.   *** access to kmem_map must be protected
 *		by splvm() because we are allowed to call malloc()
 *		at interrupt time ***
 *   mb_map => memory for large mbufs,  *** protected by splvm ***
 *   pager_map => used to map "buf" structures into kernel space
 *   exec_map => used during exec to handle exec args
 *   etc...
 *
 * the kernel allocates its private memory out of special uvm_objects whose
 * reference count is set to UVM_OBJ_KERN (thus indicating that the objects
 * are "special" and never die).   all kernel objects should be thought of
 * as large, fixed-sized, sparsely populated uvm_objects.   each kernel
 * object is equal to the size of kernel virtual address space (i.e. the
 * value "VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS").
 *
 * most kernel private memory lives in kernel_object.   the only exception
 * to this is for memory that belongs to submaps that must be protected
 * by splvm().  pages in these submaps are not assigned to an object.
 *
 * note that just because a kernel object spans the entire kernel virutal
 * address space doesn't mean that it has to be mapped into the entire space.
 * large chunks of a kernel object's space go unused either because
 * that area of kernel VM is unmapped, or there is some other type of
 * object mapped into that range (e.g. a vnode).    for submap's kernel
 * objects, the only part of the object that can ever be populated is the
 * offsets that are managed by the submap.
 *
 * note that the "offset" in a kernel object is always the kernel virtual
 * address minus the VM_MIN_KERNEL_ADDRESS (aka vm_map_min(kernel_map)).
 * example:
 *   suppose VM_MIN_KERNEL_ADDRESS is 0xf8000000 and the kernel does a
 *   uvm_km_alloc(kernel_map, PAGE_SIZE) [allocate 1 wired down page in the
 *   kernel map].    if uvm_km_alloc returns virtual address 0xf8235000,
 *   then that means that the page at offset 0x235000 in kernel_object is
 *   mapped at 0xf8235000.
 *
 * kernel object have one other special property: when the kernel virtual
 * memory mapping them is unmapped, the backing memory in the object is
 * freed right away.   this is done with the uvm_km_pgremove() function.
 * this has to be done because there is no backing store for kernel pages
 * and no need to save them after they are no longer referenced.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_km.c,v 1.62.2.5 2005/03/04 16:55:00 skrll Exp $");

#include "opt_uvmhist.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/pool.h>

#include <uvm/uvm.h>

/*
 * global data structures
 */

struct vm_map *kernel_map = NULL;

/*
 * local data structues
 */

static struct vm_map_kernel	kernel_map_store;
static struct vm_map_entry	kernel_first_mapent_store;

#if !defined(PMAP_MAP_POOLPAGE)

/*
 * kva cache
 *
 * XXX maybe it's better to do this at the uvm_map layer.
 */

#define	KM_VACACHE_SIZE	(32 * PAGE_SIZE) /* XXX tune */

static void *km_vacache_alloc(struct pool *, int);
static void km_vacache_free(struct pool *, void *);
static void km_vacache_init(struct vm_map *, const char *, size_t);

/* XXX */
#define	KM_VACACHE_POOL_TO_MAP(pp) \
	((struct vm_map *)((char *)(pp) - \
	    offsetof(struct vm_map_kernel, vmk_vacache)))

static void *
km_vacache_alloc(struct pool *pp, int flags)
{
	vaddr_t va;
	size_t size;
	struct vm_map *map;
#if defined(DEBUG)
	vaddr_t loopva;
#endif
	size = pp->pr_alloc->pa_pagesz;

	map = KM_VACACHE_POOL_TO_MAP(pp);

	va = vm_map_min(map); /* hint */
	if (uvm_map(map, &va, size, NULL, UVM_UNKNOWN_OFFSET, size,
	    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
	    UVM_ADV_RANDOM, UVM_FLAG_QUANTUM |
	    ((flags & PR_WAITOK) ? 0 : UVM_FLAG_TRYLOCK | UVM_FLAG_NOWAIT))))
		return NULL;

#if defined(DEBUG)
	for (loopva = va; loopva < va + size; loopva += PAGE_SIZE) {
		if (pmap_extract(pmap_kernel(), loopva, NULL))
			panic("km_vacache_free: has mapping");
	}
#endif

	return (void *)va;
}

static void
km_vacache_free(struct pool *pp, void *v)
{
	vaddr_t va = (vaddr_t)v;
	size_t size = pp->pr_alloc->pa_pagesz;
	struct vm_map *map;
#if defined(DEBUG)
	vaddr_t loopva;

	for (loopva = va; loopva < va + size; loopva += PAGE_SIZE) {
		if (pmap_extract(pmap_kernel(), loopva, NULL))
			panic("km_vacache_free: has mapping");
	}
#endif
	map = KM_VACACHE_POOL_TO_MAP(pp);
	uvm_unmap1(map, va, va + size, UVM_FLAG_QUANTUM);
}

/*
 * km_vacache_init: initialize kva cache.
 */

static void
km_vacache_init(struct vm_map *map, const char *name, size_t size)
{
	struct vm_map_kernel *vmk;
	struct pool *pp;
	struct pool_allocator *pa;

	KASSERT(VM_MAP_IS_KERNEL(map));
	KASSERT(size < (vm_map_max(map) - vm_map_min(map)) / 2); /* sanity */

	vmk = vm_map_to_kernel(map);
	pp = &vmk->vmk_vacache;
	pa = &vmk->vmk_vacache_allocator;
	memset(pa, 0, sizeof(*pa));
	pa->pa_alloc = km_vacache_alloc;
	pa->pa_free = km_vacache_free;
	pa->pa_pagesz = (unsigned int)size;
	pool_init(pp, PAGE_SIZE, 0, 0, PR_NOTOUCH | PR_RECURSIVE, name, pa);

	/* XXX for now.. */
	pool_sethiwat(pp, 0);
}

void
uvm_km_vacache_init(struct vm_map *map, const char *name, size_t size)
{

	map->flags |= VM_MAP_VACACHE;
	if (size == 0)
		size = KM_VACACHE_SIZE;
	km_vacache_init(map, name, size);
}

#else /* !defined(PMAP_MAP_POOLPAGE) */

void
uvm_km_vacache_init(struct vm_map *map, const char *name, size_t size)
{

	/* nothing */
}

#endif /* !defined(PMAP_MAP_POOLPAGE) */

/*
 * uvm_km_init: init kernel maps and objects to reflect reality (i.e.
 * KVM already allocated for text, data, bss, and static data structures).
 *
 * => KVM is defined by VM_MIN_KERNEL_ADDRESS/VM_MAX_KERNEL_ADDRESS.
 *    we assume that [min -> start] has already been allocated and that
 *    "end" is the end.
 */

void
uvm_km_init(start, end)
	vaddr_t start, end;
{
	vaddr_t base = VM_MIN_KERNEL_ADDRESS;

	/*
	 * next, init kernel memory objects.
	 */

	/* kernel_object: for pageable anonymous kernel memory */
	uao_init();
	uvm.kernel_object = uao_create(VM_MAX_KERNEL_ADDRESS -
				 VM_MIN_KERNEL_ADDRESS, UAO_FLAG_KERNOBJ);

	/*
	 * init the map and reserve any space that might already
	 * have been allocated kernel space before installing.
	 */

	uvm_map_setup_kernel(&kernel_map_store, base, end, VM_MAP_PAGEABLE);
	kernel_map_store.vmk_map.pmap = pmap_kernel();
	if (start != base) {
		int error;
		struct uvm_map_args args;

		error = uvm_map_prepare(&kernel_map_store.vmk_map,
		    base, start - base,
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
		    		UVM_ADV_RANDOM, UVM_FLAG_FIXED), &args);
		if (!error) {
			kernel_first_mapent_store.flags =
			    UVM_MAP_KERNEL | UVM_MAP_FIRST;
			error = uvm_map_enter(&kernel_map_store.vmk_map, &args,
			    &kernel_first_mapent_store);
		}

		if (error)
			panic(
			    "uvm_km_init: could not reserve space for kernel");
	}

	/*
	 * install!
	 */

	kernel_map = &kernel_map_store.vmk_map;
	uvm_km_vacache_init(kernel_map, "kvakernel", 0);
}

/*
 * uvm_km_suballoc: allocate a submap in the kernel map.   once a submap
 * is allocated all references to that area of VM must go through it.  this
 * allows the locking of VAs in kernel_map to be broken up into regions.
 *
 * => if `fixed' is true, *min specifies where the region described
 *      by the submap must start
 * => if submap is non NULL we use that as the submap, otherwise we
 *	alloc a new map
 */
struct vm_map *
uvm_km_suballoc(map, min, max, size, flags, fixed, submap)
	struct vm_map *map;
	vaddr_t *min, *max;		/* IN/OUT, OUT */
	vsize_t size;
	int flags;
	boolean_t fixed;
	struct vm_map_kernel *submap;
{
	int mapflags = UVM_FLAG_NOMERGE | (fixed ? UVM_FLAG_FIXED : 0);

	KASSERT(vm_map_pmap(map) == pmap_kernel());

	size = round_page(size);	/* round up to pagesize */

	/*
	 * first allocate a blank spot in the parent map
	 */

	if (uvm_map(map, min, size, NULL, UVM_UNKNOWN_OFFSET, 0,
	    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
	    UVM_ADV_RANDOM, mapflags)) != 0) {
	       panic("uvm_km_suballoc: unable to allocate space in parent map");
	}

	/*
	 * set VM bounds (min is filled in by uvm_map)
	 */

	*max = *min + size;

	/*
	 * add references to pmap and create or init the submap
	 */

	pmap_reference(vm_map_pmap(map));
	if (submap == NULL) {
		submap = malloc(sizeof(*submap), M_VMMAP, M_WAITOK);
		if (submap == NULL)
			panic("uvm_km_suballoc: unable to create submap");
	}
	uvm_map_setup_kernel(submap, *min, *max, flags);
	submap->vmk_map.pmap = vm_map_pmap(map);

	/*
	 * now let uvm_map_submap plug in it...
	 */

	if (uvm_map_submap(map, *min, *max, &submap->vmk_map) != 0)
		panic("uvm_km_suballoc: submap allocation failed");

	return(&submap->vmk_map);
}

/*
 * uvm_km_pgremove: remove pages from a kernel uvm_object.
 *
 * => when you unmap a part of anonymous kernel memory you want to toss
 *    the pages right away.    (this gets called from uvm_unmap_...).
 */

void
uvm_km_pgremove(uobj, start, end)
	struct uvm_object *uobj;
	vaddr_t start, end;
{
	struct vm_page *pg;
	voff_t curoff, nextoff;
	int swpgonlydelta = 0;
	UVMHIST_FUNC("uvm_km_pgremove"); UVMHIST_CALLED(maphist);

	KASSERT(uobj->pgops == &aobj_pager);
	simple_lock(&uobj->vmobjlock);

	for (curoff = start; curoff < end; curoff = nextoff) {
		nextoff = curoff + PAGE_SIZE;
		pg = uvm_pagelookup(uobj, curoff);
		if (pg != NULL && pg->flags & PG_BUSY) {
			pg->flags |= PG_WANTED;
			UVM_UNLOCK_AND_WAIT(pg, &uobj->vmobjlock, 0,
				    "km_pgrm", 0);
			simple_lock(&uobj->vmobjlock);
			nextoff = curoff;
			continue;
		}

		/*
		 * free the swap slot, then the page.
		 */

		if (pg == NULL &&
		    uao_find_swslot(uobj, curoff >> PAGE_SHIFT) > 0) {
			swpgonlydelta++;
		}
		uao_dropswap(uobj, curoff >> PAGE_SHIFT);
		if (pg != NULL) {
			uvm_lock_pageq();
			uvm_pagefree(pg);
			uvm_unlock_pageq();
		}
	}
	simple_unlock(&uobj->vmobjlock);

	if (swpgonlydelta > 0) {
		simple_lock(&uvm.swap_data_lock);
		KASSERT(uvmexp.swpgonly >= swpgonlydelta);
		uvmexp.swpgonly -= swpgonlydelta;
		simple_unlock(&uvm.swap_data_lock);
	}
}


/*
 * uvm_km_pgremove_intrsafe: like uvm_km_pgremove(), but for "intrsafe"
 *    maps
 *
 * => when you unmap a part of anonymous kernel memory you want to toss
 *    the pages right away.    (this is called from uvm_unmap_...).
 * => none of the pages will ever be busy, and none of them will ever
 *    be on the active or inactive queues (because they have no object).
 */

void
uvm_km_pgremove_intrsafe(start, end)
	vaddr_t start, end;
{
	struct vm_page *pg;
	paddr_t pa;
	UVMHIST_FUNC("uvm_km_pgremove_intrsafe"); UVMHIST_CALLED(maphist);

	for (; start < end; start += PAGE_SIZE) {
		if (!pmap_extract(pmap_kernel(), start, &pa)) {
			continue;
		}
		pg = PHYS_TO_VM_PAGE(pa);
		KASSERT(pg);
		KASSERT(pg->uobject == NULL && pg->uanon == NULL);
		uvm_pagefree(pg);
	}
}


/*
 * uvm_km_kmemalloc: lower level kernel memory allocator for malloc()
 *
 * => we map wired memory into the specified map using the obj passed in
 * => NOTE: we can return NULL even if we can wait if there is not enough
 *	free VM space in the map... caller should be prepared to handle
 *	this case.
 * => we return KVA of memory allocated
 * => align,prefer - passed on to uvm_map()
 * => flags: NOWAIT, VALLOC - just allocate VA, TRYLOCK - fail if we can't
 *	lock the map
 */

vaddr_t
uvm_km_kmemalloc1(map, obj, size, align, prefer, flags)
	struct vm_map *map;
	struct uvm_object *obj;
	vsize_t size;
	vsize_t align;
	voff_t prefer;
	int flags;
{
	vaddr_t kva, loopva;
	vaddr_t offset;
	vsize_t loopsize;
	struct vm_page *pg;
	UVMHIST_FUNC("uvm_km_kmemalloc"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"  (map=0x%x, obj=0x%x, size=0x%x, flags=%d)",
		    map, obj, size, flags);
	KASSERT(vm_map_pmap(map) == pmap_kernel());

	/*
	 * setup for call
	 */

	size = round_page(size);
	kva = vm_map_min(map);	/* hint */

	/*
	 * allocate some virtual space
	 */

	if (__predict_false(uvm_map(map, &kva, size, obj, prefer, align,
		UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
			    UVM_ADV_RANDOM,
			    (flags & (UVM_KMF_TRYLOCK | UVM_KMF_NOWAIT))
			    | UVM_FLAG_QUANTUM))
			!= 0)) {
		UVMHIST_LOG(maphist, "<- done (no VM)",0,0,0,0);
		return(0);
	}

	/*
	 * if all we wanted was VA, return now
	 */

	if (flags & UVM_KMF_VALLOC) {
		UVMHIST_LOG(maphist,"<- done valloc (kva=0x%x)", kva,0,0,0);
		return(kva);
	}

	/*
	 * recover object offset from virtual address
	 */

	offset = kva - vm_map_min(kernel_map);
	UVMHIST_LOG(maphist, "  kva=0x%x, offset=0x%x", kva, offset,0,0);

	/*
	 * now allocate and map in the memory... note that we are the only ones
	 * whom should ever get a handle on this area of VM.
	 */

	loopva = kva;
	loopsize = size;
	while (loopsize) {
		if (obj) {
			simple_lock(&obj->vmobjlock);
		}
		pg = uvm_pagealloc(obj, offset, NULL, UVM_PGA_USERESERVE);
		if (__predict_true(pg != NULL)) {
			pg->flags &= ~PG_BUSY;	/* new page */
			UVM_PAGE_OWN(pg, NULL);
		}
		if (obj) {
			simple_unlock(&obj->vmobjlock);
		}

		/*
		 * out of memory?
		 */

		if (__predict_false(pg == NULL)) {
			if ((flags & UVM_KMF_NOWAIT) ||
			    ((flags & UVM_KMF_CANFAIL) && uvm_swapisfull())) {
				/* free everything! */
				uvm_unmap1(map, kva, kva + size,
				    UVM_FLAG_QUANTUM);
				return (0);
			} else {
				uvm_wait("km_getwait2");	/* sleep here */
				continue;
			}
		}

		/*
		 * map it in
		 */

		if (obj == NULL) {
			pmap_kenter_pa(loopva, VM_PAGE_TO_PHYS(pg),
			    VM_PROT_READ | VM_PROT_WRITE);
		} else {
			pmap_enter(map->pmap, loopva, VM_PAGE_TO_PHYS(pg),
			    UVM_PROT_ALL,
			    PMAP_WIRED | VM_PROT_READ | VM_PROT_WRITE);
		}
		loopva += PAGE_SIZE;
		offset += PAGE_SIZE;
		loopsize -= PAGE_SIZE;
	}

       	pmap_update(pmap_kernel());

	UVMHIST_LOG(maphist,"<- done (kva=0x%x)", kva,0,0,0);
	return(kva);
}

/*
 * uvm_km_free: free an area of kernel memory
 */

void
uvm_km_free(map, addr, size)
	struct vm_map *map;
	vaddr_t addr;
	vsize_t size;
{
	uvm_unmap1(map, trunc_page(addr), round_page(addr+size),
	    UVM_FLAG_QUANTUM);
}

/*
 * uvm_km_alloc1: allocate wired down memory in the kernel map.
 *
 * => we can sleep if needed
 */

vaddr_t
uvm_km_alloc1(map, size, zeroit)
	struct vm_map *map;
	vsize_t size;
	boolean_t zeroit;
{
	vaddr_t kva, loopva, offset;
	struct vm_page *pg;
	UVMHIST_FUNC("uvm_km_alloc1"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"(map=0x%x, size=0x%x)", map, size,0,0);
	KASSERT(vm_map_pmap(map) == pmap_kernel());

	size = round_page(size);
	kva = vm_map_min(map);		/* hint */

	/*
	 * allocate some virtual space
	 */

	if (__predict_false(uvm_map(map, &kva, size, uvm.kernel_object,
	      UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL,
					      UVM_INH_NONE, UVM_ADV_RANDOM,
					      UVM_FLAG_QUANTUM)) != 0)) {
		UVMHIST_LOG(maphist,"<- done (no VM)",0,0,0,0);
		return(0);
	}

	/*
	 * recover object offset from virtual address
	 */

	offset = kva - vm_map_min(kernel_map);
	UVMHIST_LOG(maphist,"  kva=0x%x, offset=0x%x", kva, offset,0,0);

	/*
	 * now allocate the memory.
	 */

	loopva = kva;
	while (size) {
		simple_lock(&uvm.kernel_object->vmobjlock);
		KASSERT(uvm_pagelookup(uvm.kernel_object, offset) == NULL);
		pg = uvm_pagealloc(uvm.kernel_object, offset, NULL, 0);
		if (pg) {
			pg->flags &= ~PG_BUSY;
			UVM_PAGE_OWN(pg, NULL);
		}
		simple_unlock(&uvm.kernel_object->vmobjlock);
		if (pg == NULL) {
			uvm_wait("km_alloc1w");
			continue;
		}
		pmap_enter(map->pmap, loopva, VM_PAGE_TO_PHYS(pg),
		    UVM_PROT_ALL, PMAP_WIRED | VM_PROT_READ | VM_PROT_WRITE);
		loopva += PAGE_SIZE;
		offset += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	pmap_update(map->pmap);

	/*
	 * zero on request (note that "size" is now zero due to the above loop
	 * so we need to subtract kva from loopva to reconstruct the size).
	 */

	if (zeroit)
		memset((caddr_t)kva, 0, loopva - kva);
	UVMHIST_LOG(maphist,"<- done (kva=0x%x)", kva,0,0,0);
	return(kva);
}

/*
 * uvm_km_valloc1: allocate zero-fill memory in the kernel's address space
 *
 * => memory is not allocated until fault time
 * => the align, prefer and flags parameters are passed on to uvm_map().
 *
 * Note: this function is also the backend for these macros:
 *	uvm_km_valloc
 *	uvm_km_valloc_wait
 *	uvm_km_valloc_prefer
 *	uvm_km_valloc_prefer_wait
 *	uvm_km_valloc_align
 */

vaddr_t
uvm_km_valloc1(map, size, align, prefer, flags)
	struct vm_map *map;
	vsize_t size;
	vsize_t align;
	voff_t prefer;
	uvm_flag_t flags;
{
	vaddr_t kva;
	int error;
	UVMHIST_FUNC("uvm_km_valloc1"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "(map=0x%x, size=0x%x, align=0x%x, prefer=0x%x)",
		    map, size, align, prefer);

	KASSERT(vm_map_pmap(map) == pmap_kernel());

	size = round_page(size);
	/*
	 * Check if requested size is larger than the map, in which
	 * case we can't succeed.
	 */
	if (size > vm_map_max(map) - vm_map_min(map))
		return (0);

	flags |= UVM_FLAG_QUANTUM;
	if ((flags & UVM_KMF_NOWAIT) == 0) /* XXX */
		flags |= UVM_FLAG_WAITVA;  /* XXX */

	kva = vm_map_min(map);		/* hint */

	/*
	 * allocate some virtual space.   will be demand filled
	 * by kernel_object.
	 */

	error = uvm_map(map, &kva, size, uvm.kernel_object,
	    prefer, align, UVM_MAPFLAG(UVM_PROT_ALL,
	    UVM_PROT_ALL, UVM_INH_NONE, UVM_ADV_RANDOM, flags));

	KASSERT(error == 0 || (flags & UVM_KMF_NOWAIT) != 0);

	UVMHIST_LOG(maphist,"<- done (kva=0x%x)", kva,0,0,0);

	return (kva);
}

/* Function definitions for binary compatibility */
vaddr_t
uvm_km_kmemalloc(struct vm_map *map, struct uvm_object *obj,
		 vsize_t sz, int flags)
{
	return uvm_km_kmemalloc1(map, obj, sz, 0, UVM_UNKNOWN_OFFSET, flags);
}

vaddr_t uvm_km_valloc(struct vm_map *map, vsize_t sz)
{
	return uvm_km_valloc1(map, sz, 0, UVM_UNKNOWN_OFFSET, UVM_KMF_NOWAIT);
}

vaddr_t uvm_km_valloc_align(struct vm_map *map, vsize_t sz, vsize_t align)
{
	return uvm_km_valloc1(map, sz, align, UVM_UNKNOWN_OFFSET, UVM_KMF_NOWAIT);
}

vaddr_t uvm_km_valloc_prefer_wait(struct vm_map *map, vsize_t sz, voff_t prefer)
{
	return uvm_km_valloc1(map, sz, 0, prefer, 0);
}

vaddr_t uvm_km_valloc_wait(struct vm_map *map, vsize_t sz)
{
	return uvm_km_valloc1(map, sz, 0, UVM_UNKNOWN_OFFSET, 0);
}

/* Sanity; must specify both or none. */
#if (defined(PMAP_MAP_POOLPAGE) || defined(PMAP_UNMAP_POOLPAGE)) && \
    (!defined(PMAP_MAP_POOLPAGE) || !defined(PMAP_UNMAP_POOLPAGE))
#error Must specify MAP and UNMAP together.
#endif

/*
 * uvm_km_alloc_poolpage: allocate a page for the pool allocator
 *
 * => if the pmap specifies an alternate mapping method, we use it.
 */

/* ARGSUSED */
vaddr_t
uvm_km_alloc_poolpage_cache(map, obj, waitok)
	struct vm_map *map;
	struct uvm_object *obj;
	boolean_t waitok;
{
#if defined(PMAP_MAP_POOLPAGE)
	return uvm_km_alloc_poolpage1(map, obj, waitok);
#else
	struct vm_page *pg;
	struct pool *pp = &vm_map_to_kernel(map)->vmk_vacache;
	vaddr_t va;
	int s = 0xdeadbeaf; /* XXX: gcc */
	const boolean_t intrsafe = (map->flags & VM_MAP_INTRSAFE) != 0;

	if ((map->flags & VM_MAP_VACACHE) == 0)
		return uvm_km_alloc_poolpage1(map, obj, waitok);

	if (intrsafe)
		s = splvm();
	va = (vaddr_t)pool_get(pp, waitok ? PR_WAITOK : PR_NOWAIT);
	if (intrsafe)
		splx(s);
	if (va == 0)
		return 0;
	KASSERT(!pmap_extract(pmap_kernel(), va, NULL));
again:
	pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_USERESERVE);
	if (__predict_false(pg == NULL)) {
		if (waitok) {
			uvm_wait("plpg");
			goto again;
		} else {
			if (intrsafe)
				s = splvm();
			pool_put(pp, (void *)va);
			if (intrsafe)
				splx(s);
			return 0;
		}
	}
	pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
	    VM_PROT_READ|VM_PROT_WRITE);
	pmap_update(pmap_kernel());

	return va;
#endif /* PMAP_MAP_POOLPAGE */
}

vaddr_t
uvm_km_alloc_poolpage1(map, obj, waitok)
	struct vm_map *map;
	struct uvm_object *obj;
	boolean_t waitok;
{
#if defined(PMAP_MAP_POOLPAGE)
	struct vm_page *pg;
	vaddr_t va;

 again:
	pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_USERESERVE);
	if (__predict_false(pg == NULL)) {
		if (waitok) {
			uvm_wait("plpg");
			goto again;
		} else
			return (0);
	}
	va = PMAP_MAP_POOLPAGE(VM_PAGE_TO_PHYS(pg));
	if (__predict_false(va == 0))
		uvm_pagefree(pg);
	return (va);
#else
	vaddr_t va;
	int s = 0xdeadbeaf; /* XXX: gcc */
	const boolean_t intrsafe = (map->flags & VM_MAP_INTRSAFE) != 0;

	if (intrsafe)
		s = splvm();
	va = uvm_km_kmemalloc(map, obj, PAGE_SIZE,
	    waitok ? 0 : UVM_KMF_NOWAIT | UVM_KMF_TRYLOCK);
	if (intrsafe)
		splx(s);
	return (va);
#endif /* PMAP_MAP_POOLPAGE */
}

/*
 * uvm_km_free_poolpage: free a previously allocated pool page
 *
 * => if the pmap specifies an alternate unmapping method, we use it.
 */

/* ARGSUSED */
void
uvm_km_free_poolpage_cache(map, addr)
	struct vm_map *map;
	vaddr_t addr;
{
#if defined(PMAP_UNMAP_POOLPAGE)
	uvm_km_free_poolpage1(map, addr);
#else
	struct pool *pp;
	int s = 0xdeadbeaf; /* XXX: gcc */
	const boolean_t intrsafe = (map->flags & VM_MAP_INTRSAFE) != 0;

	if ((map->flags & VM_MAP_VACACHE) == 0) {
		uvm_km_free_poolpage1(map, addr);
		return;
	}

	KASSERT(pmap_extract(pmap_kernel(), addr, NULL));
	uvm_km_pgremove_intrsafe(addr, addr + PAGE_SIZE);
	pmap_kremove(addr, PAGE_SIZE);
#if defined(DEBUG)
	pmap_update(pmap_kernel());
#endif
	KASSERT(!pmap_extract(pmap_kernel(), addr, NULL));
	pp = &vm_map_to_kernel(map)->vmk_vacache;
	if (intrsafe)
		s = splvm();
	pool_put(pp, (void *)addr);
	if (intrsafe)
		splx(s);
#endif
}

/* ARGSUSED */
void
uvm_km_free_poolpage1(map, addr)
	struct vm_map *map;
	vaddr_t addr;
{
#if defined(PMAP_UNMAP_POOLPAGE)
	paddr_t pa;

	pa = PMAP_UNMAP_POOLPAGE(addr);
	uvm_pagefree(PHYS_TO_VM_PAGE(pa));
#else
	int s = 0xdeadbeaf; /* XXX: gcc */
	const boolean_t intrsafe = (map->flags & VM_MAP_INTRSAFE) != 0;

	if (intrsafe)
		s = splvm();
	uvm_km_free(map, addr, PAGE_SIZE);
	if (intrsafe)
		splx(s);
#endif /* PMAP_UNMAP_POOLPAGE */
}
