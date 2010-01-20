/*	$NetBSD: pmap.c,v 1.179.16.11 2010/01/20 06:58:36 matt Exp $	*/

/*-
 * Copyright (c) 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Chris G. Demetriou.
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

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 *	@(#)pmap.c	8.4 (Berkeley) 1/26/94
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: pmap.c,v 1.179.16.11 2010/01/20 06:58:36 matt Exp $");

/*
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

/* XXX simonb 2002/02/26
 *
 * MIPS3_PLUS is used to conditionally compile the r4k MMU support.
 * This is bogus - for example, some IDT MIPS-II CPUs have r4k style
 * MMUs (and 32-bit ones at that).
 *
 * On the other hand, it's not likely that we'll ever support the R6000
 * (is it?), so maybe that can be an "if MIPS2 or greater" check.
 *
 * Also along these lines are using totally separate functions for
 * r3k-style and r4k-style MMUs and removing all the MIPS_HAS_R4K_MMU
 * checks in the current functions.
 *
 * These warnings probably applies to other files under sys/arch/mips.
 */

#include "opt_sysv.h"
#include "opt_cputype.h"
#include "opt_mips_cache.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/pool.h>
#include <sys/atomic.h>
#include <sys/mutex.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#include <sys/socketvar.h>	/* XXX: for sock_loan_thresh */

#include <uvm/uvm.h>

#include <mips/cache.h>
#include <mips/cpuregs.h>
#include <mips/locore.h>
#include <mips/pte.h>

CTASSERT(MIPS_KSEG0_START < 0);
CTASSERT((intptr_t)MIPS_PHYS_TO_KSEG0(0x1000) < 0);
CTASSERT(MIPS_KSEG1_START < 0);
CTASSERT((intptr_t)MIPS_PHYS_TO_KSEG1(0x1000) < 0);
CTASSERT(MIPS_KSEG2_START < 0);
CTASSERT(MIPS_MAX_MEM_ADDR < 0);
CTASSERT(MIPS_RESERVED_ADDR < 0);
CTASSERT((uint32_t)MIPS_KSEG0_START == 0x80000000);
CTASSERT((uint32_t)MIPS_KSEG1_START == 0xa0000000);
CTASSERT((uint32_t)MIPS_KSEG2_START == 0xc0000000);
CTASSERT((uint32_t)MIPS_MAX_MEM_ADDR == 0xbe000000);
CTASSERT((uint32_t)MIPS_RESERVED_ADDR == 0xbfc80000);
CTASSERT(MIPS_KSEG0_P(MIPS_PHYS_TO_KSEG0(0)));
CTASSERT(MIPS_KSEG1_P(MIPS_PHYS_TO_KSEG1(0)));

#ifdef DEBUG
struct {
	int kernel;	/* entering kernel mapping */
	int user;	/* entering user mapping */
	int ptpneeded;	/* needed to allocate a PT page */
	int pwchange;	/* no mapping change, just wiring or protection */
	int wchange;	/* no mapping change, just wiring */
	int mchange;	/* was mapped but mapping to different page */
	int managed;	/* a managed page */
	int firstpv;	/* first mapping for this PA */
	int secondpv;	/* second mapping for this PA */
	int ci;		/* cache inhibited */
	int unmanaged;	/* not a managed page */
	int flushes;	/* cache flushes */
	int cachehit;	/* new entry forced valid entry out */
} enter_stats;
struct {
	int calls;
	int removes;
	int flushes;
	int pidflushes;	/* HW pid stolen */
	int pvfirst;
	int pvsearch;
} remove_stats;

#define PDB_FOLLOW	0x0001
#define PDB_INIT	0x0002
#define PDB_ENTER	0x0004
#define PDB_REMOVE	0x0008
#define PDB_CREATE	0x0010
#define PDB_PTPAGE	0x0020
#define PDB_PVENTRY	0x0040
#define PDB_BITS	0x0080
#define PDB_COLLECT	0x0100
#define PDB_PROTECT	0x0200
#define PDB_TLBPID	0x0400
#define PDB_PARANOIA	0x2000
#define PDB_WIRING	0x4000
#define PDB_PVDUMP	0x8000
int pmapdebug = 0;

#endif

#define PMAP_ASID_RESERVED 0

CTASSERT(PMAP_ASID_RESERVED == 0);
/*
 * Initialize the kernel pmap.
 */
#ifdef MULTIPROCESSOR
#define	PMAP_SIZE	offsetof(struct pmap, pm_pai[MAXCPUS])
#else
#define	PMAP_SIZE	sizeof(struct pmap)
#endif
struct pmap_kernel kernel_pmap_store = {
	.kernel_pmap = {
		.pm_count = 1,
		.pm_segtab = (void *)(MIPS_KSEG2_START + 0x1eadbeef),
	},
};

paddr_t mips_avail_start;	/* PA of first available physical page */
paddr_t mips_avail_end;		/* PA of last available physical page */
vaddr_t mips_virtual_end;	/* VA of last avail page (end of kernel AS) */

struct pv_entry	*pv_table;
int		 pv_table_npages;

pt_entry_t	*Sysmap;		/* kernel pte table */
unsigned int	Sysmapsize;		/* number of pte's in Sysmap */

/*
 * The pools from which pmap structures and sub-structures are allocated.
 */
struct pool pmap_pmap_pool;
struct pool pmap_pv_pool;

#ifndef PMAP_PV_LOWAT
#define	PMAP_PV_LOWAT	16
#endif
int		pmap_pv_lowat = PMAP_PV_LOWAT;

bool		pmap_initialized = false;

#define PAGE_IS_MANAGED(pa)	\
	(pmap_initialized == true && vm_physseg_find(atop(pa), NULL) != -1)

#define PMAP_IS_ACTIVE(pm)						\
	((pm) == pmap_kernel() || 					\
	 (pm) == curlwp->l_proc->p_vmspace->vm_map.pmap)

/* Forward function declarations */
void pmap_remove_pv(pmap_t, vaddr_t, struct vm_page *);
uint32_t pmap_tlb_asid_alloc(pmap_t pmap, struct cpu_info *ci);
void pmap_enter_pv(pmap_t, vaddr_t, struct vm_page *, u_int *);
pt_entry_t *pmap_pte(pmap_t, vaddr_t);

/*
 * PV table management functions.
 */
void	*pmap_pv_page_alloc(struct pool *, int);
void	pmap_pv_page_free(struct pool *, void *);

struct pool_allocator pmap_pv_page_allocator = {
	pmap_pv_page_alloc, pmap_pv_page_free, 0,
};

#define	pmap_pv_alloc()		pool_get(&pmap_pv_pool, PR_NOWAIT)
#define	pmap_pv_free(pv)	pool_put(&pmap_pv_pool, (pv))

/*
 * Misc. functions.
 */

static inline bool
pmap_clear_page_attributes(struct vm_page *pg, u_int attributes)
{
	volatile u_int * const attrp = &pg->mdpage.pvh_attrs;
#ifdef MULTIPROCESSOR
	for (;;) {
		u_int old_attr = *attrp;
		if ((old_attr & attributes) == 0)
			return false;
		u_int new_attr = old_attr & ~attributes;
		if (old_attr == atomic_cas_uint(attrp, old_attr, new_attr))
			return true;
	}
#else
	u_int old_attr = *attrp;
	if ((old_attr & attributes) == 0)
		return false;
	*attrp &= ~attributes;
	return true;
#endif
}

static inline void
pmap_set_page_attributes(struct vm_page *pg, u_int attributes)
{
#ifdef MULTIPROCESSOR
	atomic_or_uint(&pg->mdpage.pvh_attrs, attributes);
#else
	pg->mdpage.pvh_attrs |= attributes;
#endif
}

#if defined(MIPS3_PLUS)	/* XXX mmu XXX */
void mips_dump_segtab(struct proc *);
static void mips_flushcache_allpvh(paddr_t);

/*
 * Flush virtual addresses associated with a given physical address
 */
static void
mips_flushcache_allpvh(paddr_t pa)
{
	struct vm_page *pg;
	struct pv_entry *pv;

	pg = PHYS_TO_VM_PAGE(pa);
	if (pg == NULL) {
		/* page is unmanaged */
#ifdef DIAGNOSTIC
		printf("mips_flushcache_allpvh(): unmanaged pa = %#"PRIxPADDR"\n",
		    pa);
#endif
		return;
	}

	pv = pg->mdpage.pvh_list;

#if defined(MIPS3_NO_PV_UNCACHED)
	/* No current mapping.  Cache was flushed by pmap_remove_pv() */
	if (pv->pv_pmap == NULL)
		return;

	/* Only one index is allowed at a time */
	if (mips_cache_indexof(pa) != mips_cache_indexof(pv->pv_va))
		mips_dcache_wbinv_range_index(pv->pv_va, NBPG);
#else
	while (pv) {
		mips_dcache_wbinv_range_index(pv->pv_va, NBPG);
		pv = pv->pv_next;
	}
#endif
}
#endif /* MIPS3_PLUS */

/*
 *	Bootstrap the system enough to run with virtual memory.
 *	firstaddr is the first unused kseg0 address (not page aligned).
 */
void
pmap_bootstrap(void)
{
	vsize_t bufsz;

	/*
	 * Compute the number of pages kmem_map will have.
	 */
	kmeminit_nkmempages();

	/*
	 * Figure out how many PTE's are necessary to map the kernel.
	 * We also reserve space for kmem_alloc_pageable() for vm_fork().
	 */

	/* Get size of buffer cache and set an upper limit */
	bufsz = buf_memcalc();
	buf_setvalimit(bufsz);

	Sysmapsize = (VM_PHYS_SIZE + (ubc_nwins << ubc_winshift) +
	    bufsz + 16 * NCARGS + pager_map_size) / NBPG +
	    (maxproc * UPAGES) + nkmempages;

#ifdef SYSVSHM
	Sysmapsize += shminfo.shmall;
#endif
#ifdef KSEG2IOBUFSIZE
	Sysmapsize += (KSEG2IOBUFSIZE >> PGSHIFT);
#endif
	/* XXX: else runs out of space on 256MB sbmips!! */
	Sysmapsize += 20000;

	/*
	 * Initialize `FYI' variables.	Note we're relying on
	 * the fact that BSEARCH sorts the vm_physmem[] array
	 * for us.  Must do this before uvm_pageboot_alloc()
	 * can be called.
	 */
	mips_avail_start = ptoa(vm_physmem[0].start);
	mips_avail_end = ptoa(vm_physmem[vm_nphysseg - 1].end);
	mips_virtual_end = VM_MIN_KERNEL_ADDRESS + Sysmapsize * NBPG;

	/*
	 * Now actually allocate the kernel PTE array (must be done
	 * after virtual_end is initialized).
	 */
	Sysmap = (pt_entry_t *)
	    uvm_pageboot_alloc(sizeof(pt_entry_t) * Sysmapsize);

	/*
	 * Allocate memory for the pv_heads.  (A few more of the latter
	 * are allocated than are needed.)
	 *
	 * We could do this in pmap_init when we know the actual
	 * managed page pool size, but its better to use kseg0
	 * addresses rather than kernel virtual addresses mapped
	 * through the TLB.
	 */
	pv_table_npages = physmem;
	pv_table = (struct pv_entry *)
	    uvm_pageboot_alloc(sizeof(struct pv_entry) * pv_table_npages);

	/*
	 * Initialize the pools.
	 */
	pool_init(&pmap_pmap_pool, PMAP_SIZE, 0, 0, 0, "pmappl",
	    &pool_allocator_nointr, IPL_NONE);
	pool_init(&pmap_pv_pool, sizeof(struct pv_entry), 0, 0, 0, "pvpl",
	    &pmap_pv_page_allocator, IPL_NONE);

	tlb_set_asid(0);

#ifdef MIPS3_PLUS	/* XXX mmu XXX */
	/*
	 * The R4?00 stores only one copy of the Global bit in the
	 * translation lookaside buffer for each 2 page entry.
	 * Thus invalid entrys must have the Global bit set so
	 * when Entry LO and Entry HI G bits are anded together
	 * they will produce a global bit to store in the tlb.
	 */
	if (MIPS_HAS_R4K_MMU) {
		u_int i;
		pt_entry_t *spte;

		for (i = 0, spte = Sysmap; i < Sysmapsize; i++, spte++)
			spte->pt_entry = MIPS3_PG_G;
	}
#endif	/* MIPS3_PLUS */
}

/*
 * Define the initial bounds of the kernel virtual address space.
 */
void
pmap_virtual_space(vaddr_t *vstartp, vaddr_t *vendp)
{

	*vstartp = VM_MIN_KERNEL_ADDRESS;	/* kernel is in K0SEG */
	*vendp = trunc_page(mips_virtual_end);	/* XXX need pmap_growkernel() */
}

/*
 * Bootstrap memory allocator (alternative to vm_bootstrap_steal_memory()).
 * This function allows for early dynamic memory allocation until the virtual
 * memory system has been bootstrapped.  After that point, either kmem_alloc
 * or malloc should be used.  This function works by stealing pages from the
 * (to be) managed page pool, then implicitly mapping the pages (by using
 * their k0seg addresses) and zeroing them.
 *
 * It may be used once the physical memory segments have been pre-loaded
 * into the vm_physmem[] array.  Early memory allocation MUST use this
 * interface!  This cannot be used after vm_page_startup(), and will
 * generate a panic if tried.
 *
 * Note that this memory will never be freed, and in essence it is wired
 * down.
 *
 * We must adjust *vstartp and/or *vendp iff we use address space
 * from the kernel virtual address range defined by pmap_virtual_space().
 */
vaddr_t
pmap_steal_memory(vsize_t size, vaddr_t *vstartp, vaddr_t *vendp)
{
	u_int npgs;
	paddr_t pa;
	vaddr_t va;

	size = round_page(size);
	npgs = atop(size);

	for (u_int bank = 0; bank < vm_nphysseg; bank++) {
		if (uvm.page_init_done == true)
			panic("pmap_steal_memory: called _after_ bootstrap");

		if (vm_physmem[bank].avail_start != vm_physmem[bank].start ||
		    vm_physmem[bank].avail_start >= vm_physmem[bank].avail_end)
			continue;

		if ((vm_physmem[bank].avail_end - vm_physmem[bank].avail_start)
		    < npgs)
			continue;

		/*
		 * There are enough pages here; steal them!
		 */
		pa = ptoa(vm_physmem[bank].avail_start);
		vm_physmem[bank].avail_start += npgs;
		vm_physmem[bank].start += npgs;

		/*
		 * Have we used up this segment?
		 */
		if (vm_physmem[bank].avail_start == vm_physmem[bank].end) {
			if (vm_nphysseg == 1)
				panic("pmap_steal_memory: out of memory!");

			/* Remove this segment from the list. */
			vm_nphysseg--;
			for (u_int x = bank; x < vm_nphysseg; x++) {
				/* structure copy */
				vm_physmem[x] = vm_physmem[x + 1];
			}
		}

#ifdef _LP64
		/*
		 * Use the same CCA as used to access KSEG0 for XKPHYS.
		 */
		uint32_t v;

		__asm __volatile("mfc0 %0,$%1"
		    : "=r"(v)
		    : "n"(MIPS_COP_0_CONFIG));

		va = MIPS_PHYS_TO_XKPHYS(v & MIPS3_CONFIG_K0_MASK, pa);
#else
		if (pa + size > MIPS_PHYS_MASK + 1)
			panic("pmap_steal_memory: pa %"PRIxPADDR
			    " can not be mapped into KSEG0", pa);
		va = MIPS_PHYS_TO_KSEG0(pa);
#endif

		memset((void *)va, 0, size);
		return va;
	}

	/*
	 * If we got here, there was no memory left.
	 */
	panic("pmap_steal_memory: no memory to steal");
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(void)
{
	vsize_t		s;
	int		bank, i;
	pv_entry_t	pv;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_INIT))
		printf("pmap_init()\n");
#endif

	/*
	 * Memory for the pv entry heads has
	 * already been allocated.  Initialize the physical memory
	 * segments.
	 */
	pv = pv_table;
	for (bank = 0; bank < vm_nphysseg; bank++) {
		s = vm_physmem[bank].end - vm_physmem[bank].start;
		for (i = 0; i < s; i++)
			vm_physmem[bank].pgs[i].mdpage.pvh_list = pv++;
	}

	/*
	 * Set a low water mark on the pv_entry pool, so that we are
	 * more likely to have these around even in extreme memory
	 * starvation.
	 */
	pool_setlowat(&pmap_pv_pool, pmap_pv_lowat);

	/*
	 * Now it is safe to enable pv entry recording.
	 */
	pmap_initialized = true;

#ifdef MIPS3
	if (MIPS_HAS_R4K_MMU) {
		/*
		 * XXX
		 * Disable sosend_loan() in src/sys/kern/uipc_socket.c
		 * on MIPS3 CPUs to avoid possible virtual cache aliases
		 * and uncached mappings in pmap_enter_pv().
		 * 
		 * Ideally, read only shared mapping won't cause aliases
		 * so pmap_enter_pv() should handle any shared read only
		 * mappings without uncached ops like ARM pmap.
		 * 
		 * On the other hand, R4000 and R4400 have the virtual
		 * coherency exceptions which will happen even on read only
		 * mappings, so we always have to disable sosend_loan()
		 * on such CPUs.
		 */
		sock_loan_thresh = -1;
	}
#endif
}

/*
 *	Create and return a physical map.
 *
 *	If the size specified for the map
 *	is zero, the map is an actual physical
 *	map, and may be referenced by the
 *	hardware.
 *
 *	If the size specified is non-zero,
 *	the map will be used in software only, and
 *	is bounded by that size.
 */
pmap_t
pmap_create(void)
{
	pmap_t pmap;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_create()\n");
#endif

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);
	memset(pmap, 0, PMAP_SIZE);

	pmap->pm_count = 1;

	pmap_segtab_alloc(pmap);

	return pmap;
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap_t pmap)
{
	int count;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_destroy(%p)\n", pmap);
#endif
	count = --pmap->pm_count;
	if (count > 0)
		return;

	pmap_segtab_free(pmap);

	pool_put(&pmap_pmap_pool, pmap);
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap_t pmap)
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_reference(%p)\n", pmap);
#endif
	if (pmap != NULL) {
		pmap->pm_count++;
	}
}

/*
 *	Make a new pmap (vmspace) active for the given process.
 */
void
pmap_activate(struct lwp *l)
{
	pmap_t pmap = l->l_proc->p_vmspace->vm_map.pmap;
	unsigned int asid;

	asid = pmap_tlb_asid_alloc(pmap, l->l_cpu);
	if (l == curlwp) {
		pmap_segtab_activate(l);
		tlb_set_asid(asid);
	}
}

/*
 *	Make a previously active pmap (vmspace) inactive.
 */
void
pmap_deactivate(struct lwp *l)
{

	/* Nothing to do. */
}

void
pmap_update(struct pmap *pmap)
{
#if 0
	__asm __volatile(
		"mtc0\t$ra,$%0; nop; eret"
	    :
	    : "n"(MIPS_COP_0_ERROR_PC));
#endif
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */

static bool
pmap_pte_remove(pmap_t pmap, vaddr_t sva, vaddr_t eva, pt_entry_t *pte,
	uintptr_t flags)
{
	struct cpu_info * const ci = curcpu();
	struct pmap_asid_info * const pai = PMAP_PAI(pmap, ci);
	const uint32_t asid = pai->pai_asid << MIPS_TLB_PID_SHIFT;
	const bool needflush = PMAP_PAI_ASIDVALID_P(pai, ci);

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT)) {
		printf("%s: %p, %"PRIxVADDR", %"PRIxVADDR", %p, %"PRIxPTR"\n",
		   __func__, pmap, sva, eva, pte, flags);
	}
#endif

	for (; sva < eva; sva += NBPG, pte++) {
		struct vm_page *pg;
		uint32_t entry = pte->pt_entry;
		if (!mips_pg_v(entry))
			continue;
		if (mips_pg_wired(entry))
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;
		pg = PHYS_TO_VM_PAGE(mips_tlbpfn_to_paddr(entry));
		if (pg)
			pmap_remove_pv(pmap, sva, pg);
		pte->pt_entry = mips_pg_nv_bit();
		/*
		 * Flush the TLB for the given address.
		 */
		if (needflush) {
			tlb_invalidate_addr(sva | asid);
#ifdef DEBUG
			remove_stats.flushes++;
#endif
		}
	}
	return false;
}

void
pmap_remove(pmap_t pmap, vaddr_t sva, vaddr_t eva)
{
	struct vm_page *pg;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove(%p, %#"PRIxVADDR", %#"PRIxVADDR")\n", pmap, sva, eva);
	remove_stats.calls++;
#endif
	if (pmap == pmap_kernel()) {
		/* remove entries from kernel pmap */
#ifdef PARANOIADIAG
		if (sva < VM_MIN_KERNEL_ADDRESS || eva >= virtual_end)
			panic("pmap_remove: kva not in range");
#endif
		pt_entry_t *pte = kvtopte(sva);
		for (; sva < eva; sva += NBPG, pte++) {
			uint32_t entry = pte->pt_entry;
			if (!mips_pg_v(entry))
				continue;
			if (mips_pg_wired(entry))
				pmap->pm_stats.wired_count--;
			pmap->pm_stats.resident_count--;
			pg = PHYS_TO_VM_PAGE(mips_tlbpfn_to_paddr(entry));
			if (pg)
				pmap_remove_pv(pmap, sva, pg);
			if (MIPS_HAS_R4K_MMU)
				/* See above about G bit */
				pte->pt_entry = MIPS3_PG_NV | MIPS3_PG_G;
			else
				pte->pt_entry = MIPS1_PG_NV;

			/*
			 * Flush the TLB for the given address.
			 */
			tlb_invalidate_addr(sva);
#ifdef DEBUG
			remove_stats.flushes++;
#endif
		}
		return;
	}

#ifdef PARANOIADIAG
	if (eva > VM_MAXUSER_ADDRESS)
		panic("pmap_remove: uva not in range");
	if (PMAP_IS_ACTIVE(pmap)) {
		struct pmap_asid_info * const pai = PMAP_PAI(pmap, curcpu());
		uint32_t asid;

		__asm volatile("mfc0 %0,$10; nop" : "=r"(asid));
		asid = (MIPS_HAS_R4K_MMU) ? (asid & 0xff) : (asid & 0xfc0) >> 6;
		if (asid != pai->pai_asid) {
			panic("inconsistency for active TLB flush: %d <-> %d",
			    asid, pai->pai_asid);
		}
	}
#endif
	pmap_pte_process(pmap, sva, eva, pmap_pte_remove, 0);
}

/*
 *	pmap_page_protect:
 *
 *	Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	pv_entry_t pv;
	vaddr_t va;

#ifdef DEBUG
	if ((pmapdebug & (PDB_FOLLOW|PDB_PROTECT)) ||
	    (prot == VM_PROT_NONE && (pmapdebug & PDB_REMOVE)))
		printf("pmap_page_protect(%#"PRIxPADDR", %x)\n",
		    VM_PAGE_TO_PHYS(pg), prot);
#endif
	switch (prot) {
	case VM_PROT_READ|VM_PROT_WRITE:
	case VM_PROT_ALL:
		break;

	/* copy_on_write */
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		pv = pg->mdpage.pvh_list;
		/*
		 * Loop over all current mappings setting/clearing as appropos.
		 */
		if (pv->pv_pmap != NULL) {
			for (; pv; pv = pv->pv_next) {
				va = pv->pv_va;
				pmap_protect(pv->pv_pmap, va, va + PAGE_SIZE,
				    prot);
				pmap_update(pv->pv_pmap);
			}
		}
		break;

	/* remove_all */
	default:
		pv = pg->mdpage.pvh_list;
		while (pv->pv_pmap != NULL) {
			pmap_remove(pv->pv_pmap, pv->pv_va,
			    pv->pv_va + PAGE_SIZE);
		}
		pmap_update(pv->pv_pmap);
	}
}

static bool
pmap_pte_protect(pmap_t pmap, vaddr_t sva, vaddr_t eva, pt_entry_t *pte,
	uintptr_t flags)
{
	struct cpu_info * const ci = curcpu();
	struct pmap_asid_info * const pai = PMAP_PAI(pmap, ci);
	const bool needupdate = PMAP_PAI_ASIDVALID_P(pai, ci);
	const uint32_t asid = pai->pai_asid << MIPS_TLB_PID_SHIFT;
	const uint32_t p = flags;

	/*
	 * Change protection on every valid mapping within this segment.
	 */
	for (; sva < eva; sva += NBPG, pte++) {
		uint32_t entry = pte->pt_entry;
		if (!mips_pg_v(entry))
			continue;
		if (MIPS_HAS_R4K_MMU && entry & mips_pg_m_bit())
			mips_dcache_wbinv_range_index(sva, PAGE_SIZE);
		entry = (entry & ~(mips_pg_m_bit() | mips_pg_ro_bit())) | p;
		pte->pt_entry = entry;
		/*
		 * Update the TLB if the given address is in the cache.
		 */
		if (needupdate)
			tlb_update(sva | asid, entry);
#ifdef MULTIPROCESSORX
#error TLB shootdown needed
#endif
	}
	return false;
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	pt_entry_t *pte;
	u_int p;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PROTECT))
		printf("pmap_protect(%p, %#"PRIxVADDR", %#"PRIxVADDR", %x)\n",
		    pmap, sva, eva, prot);
#endif
	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	p = (prot & VM_PROT_WRITE) ? mips_pg_rw_bit() : mips_pg_ro_bit();

	if (pmap == pmap_kernel()) {
		/*
		 * Change entries in kernel pmap.
		 * This will trap if the page is writable (in order to set
		 * the dirty bit) even if the dirty bit is already set. The
		 * optimization isn't worth the effort since this code isn't
		 * executed much. The common case is to make a user page
		 * read-only.
		 */
#ifdef PARANOIADIAG
		if (sva < VM_MIN_KERNEL_ADDRESS || eva >= virtual_end)
			panic("pmap_protect: kva not in range");
#endif
		pte = kvtopte(sva);
		for (; sva < eva; sva += NBPG, pte++) {
			uint32_t entry = pte->pt_entry;
			if (!mips_pg_v(entry))
				continue;
			if (MIPS_HAS_R4K_MMU && entry & mips_pg_m_bit())
				mips_dcache_wb_range(sva, PAGE_SIZE);
			entry &= ~(mips_pg_m_bit() | mips_pg_ro_bit());
			entry |= p;
			pte->pt_entry = entry;
			tlb_update(sva, entry);
		}
		return;
	}

#ifdef PARANOIADIAG
	if (eva > VM_MAXUSER_ADDRESS)
		panic("pmap_protect: uva not in range");
	if (PMAP_IS_ACTIVE(pmap)) {
		struct pmap_asid_info * const pai = PMAP_PAI(pmap, curcpu());
		uint32_t asid;

		__asm volatile("mfc0 %0,$10; nop" : "=r"(asid));
		asid = (MIPS_HAS_R4K_MMU) ? (asid & 0xff) : (asid & 0xfc0) >> 6;
		if (asid != pai->pai_asid) {
			panic("inconsistency for active TLB update: %d <-> %d",
			    asid, pai->pai_asid);
		}
	}
#endif

	/*
	 * Change protection on every valid mapping within this segment.
	 */
	pmap_pte_process(pmap, sva, eva, pmap_pte_protect, p);
}

/*
 * XXXJRT -- need a version for each cache type.
 */
void
pmap_procwr(struct proc *p, vaddr_t va, size_t len)
{
#ifdef MIPS1
	pmap_t pmap;

	pmap = p->p_vmspace->vm_map.pmap;
#endif /* MIPS1 */

	if (MIPS_HAS_R4K_MMU) {
#ifdef MIPS3_PLUS	/* XXX mmu XXX */
		/*
		 * XXX
		 * shouldn't need to do this for physical d$?
		 * should need to do this for virtual i$ if prot == EXEC?
		 */
		if (p == curlwp->l_proc
		    && mips_cache_info.mci_pdcache_way_mask < PAGE_SIZE)
		    /* XXX check icache mask too? */
			mips_icache_sync_range(va, len);
		else
			mips_icache_sync_range_index(va, len);
#endif /* MIPS3_PLUS */	/* XXX mmu XXX */
	} else {
#ifdef MIPS1
		pt_entry_t *pte;
		unsigned entry;

		if (pmap == pmap_kernel()) {
			pte = kvtopte(va);
		} else {
			pte = pmap_pte_lookup(pmap, va);
		}
		entry = pte->pt_entry;
		if (!mips_pg_v(entry))
			return;

		/*
		 * XXXJRT -- Wrong -- since page is physically-indexed, we
		 * XXXJRT need to loop.
		 */
		mips_icache_sync_range(
		    MIPS_PHYS_TO_KSEG0(mips1_tlbpfn_to_paddr(entry)
		    + (va & PGOFSET)),
		    len);
#endif /* MIPS1 */
	}
}

/*
 *	Return RO protection of page.
 */
int
pmap_is_page_ro(pmap_t pmap, vaddr_t va, int entry)
{

	return entry & mips_pg_ro_bit();
}

#if defined(MIPS3_PLUS) && !defined(MIPS3_NO_PV_UNCACHED)	/* XXX mmu XXX */
/*
 *	pmap_page_cache:
 *
 *	Change all mappings of a managed page to cached/uncached.
 */
static void
pmap_page_cache(struct vm_page *pg, int mode)
{
	pv_entry_t pv;
	unsigned newmode;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_page_uncache(%#"PRIxPADDR")\n", VM_PAGE_TO_PHYS(pg));
#endif
	newmode = mode & PV_UNCACHED ? MIPS3_PG_UNCACHED : MIPS3_PG_CACHED;
	pv = pg->mdpage.pvh_list;

	struct cpu_info * const ci = curcpu();
	while (pv) {
		pmap_t pmap = pv->pv_pmap;
		struct pmap_asid_info * const pai = PMAP_PAI(pmap, ci);
		uint32_t asid = pai->pai_asid;
		bool needupdate;
		pt_entry_t *pte;
		unsigned entry;

		pv->pv_flags = (pv->pv_flags & ~PV_UNCACHED) | mode;
		if (pv->pv_pmap == pmap_kernel()) {
			/*
			 * Change entries in kernel pmap.
			 */
			pte = kvtopte(pv->pv_va);
			entry = pte->pt_entry;
			needupdate = true;
		} else {
			pte = pmap_pte_lookup(pv->pv_pmap, pv->pv_va);
			if (pte == NULL)
				continue;
			entry = pte->pt_entry;
			needupdate = PMAP_PAI_ASIDVALID_P(pai, ci);
		}
		if (entry & MIPS3_PG_V) {
			entry = (entry & ~MIPS3_PG_CACHEMODE) | newmode;
			pte->pt_entry = entry;
			if (needupdate)
				tlb_update(pv->pv_va | asid, entry);
		}
		pv = pv->pv_next;
	}
}
#endif	/* MIPS3_PLUS && !MIPS3_NO_PV_UNCACHED */

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
int
pmap_enter(pmap_t pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	pt_entry_t *pte;
	u_int npte;
	struct vm_page *pg;
	unsigned asid;
#if defined(_MIPS_PADDR_T_64BIT) || defined(_LP64)
	int cached = 1;
#endif
	bool wired = (flags & PMAP_WIRED) != 0;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_enter(%p, %#"PRIxVADDR", %#"PRIxPADDR", %x, %x)\n",
		    pmap, va, pa, prot, wired);
#endif
#if defined(DEBUG) || defined(DIAGNOSTIC) || defined(PARANOIADIAG)
	if (pmap == pmap_kernel()) {
#ifdef DEBUG
		enter_stats.kernel++;
#endif
		if (va < VM_MIN_KERNEL_ADDRESS || va >= mips_virtual_end)
			panic("pmap_enter: kva too big");
	} else {
#ifdef DEBUG
		enter_stats.user++;
#endif
		if (va >= VM_MAXUSER_ADDRESS)
			panic("pmap_enter: uva too big");
	}
#endif
#ifdef PARANOIADIAG
#if defined(cobalt) || defined(newsmips) || defined(pmax) /* otherwise ok */
	if (pa & 0x80000000)	/* this is not error in general. */
		panic("pmap_enter: pa");
#endif

#if defined(_MIPS_PADDR_T_64BIT) || defined(_LP64)
	if (pa & PMAP_NOCACHE) {
		cached = 0;
		pa &= ~PMAP_NOCACHE;
	}
#endif

	if (!(prot & VM_PROT_READ))
		panic("pmap_enter: prot");
#endif
	pg = PHYS_TO_VM_PAGE(pa);

	if (pg) {
		/* Set page referenced/modified status based on flags */
		if (flags & VM_PROT_WRITE)
			pmap_set_page_attributes(pg, PV_MODIFIED|PV_REFERENCED);
		else if (flags & VM_PROT_ALL)
			pmap_set_page_attributes(pg, PV_REFERENCED);
		if (!(prot & VM_PROT_WRITE))
			/*
			 * If page is not yet referenced, we could emulate this
			 * by not setting the page valid, and setting the
			 * referenced status in the TLB fault handler, similar
			 * to how page modified status is done for UTLBmod
			 * exceptions.
			 */
			npte = mips_pg_ropage_bit();
		else {
#if defined(_MIPS_PADDR_T_64BIT) || defined(_LP64)
			if (cached == 0) {
				if (pg->mdpage.pvh_attrs & PV_MODIFIED) {
					npte = mips_pg_rwncpage_bit();
				} else {
					npte = mips_pg_cwncpage_bit();
				}
			} else {
#endif
				if (pg->mdpage.pvh_attrs & PV_MODIFIED) {
					npte = mips_pg_rwpage_bit();
				} else {
					npte = mips_pg_cwpage_bit();
				}
#if defined(_MIPS_PADDR_T_64BIT) || defined(_LP64)
			}
#endif
		}
#ifdef DEBUG
		enter_stats.managed++;
#endif
	} else {
		/*
		 * Assumption: if it is not part of our managed memory
		 * then it must be device memory which may be volatile.
		 */
#ifdef DEBUG
		enter_stats.unmanaged++;
#endif
		if (MIPS_HAS_R4K_MMU) {
			npte = MIPS3_PG_IOPAGE(PMAP_CCA_FOR_PA(pa)) &
			    ~MIPS3_PG_G;
			if ((prot & VM_PROT_WRITE) == 0) {
				npte |= MIPS3_PG_RO;
				npte &= ~MIPS3_PG_D;
			}
		} else {
			npte = (prot & VM_PROT_WRITE) ?
			    (MIPS1_PG_D | MIPS1_PG_N) :
			    (MIPS1_PG_RO | MIPS1_PG_N);
		}
	}

	/*
	 * The only time we need to flush the cache is if we
	 * execute from a physical address and then change the data.
	 * This is the best place to do this.
	 * pmap_protect() and pmap_remove() are mostly used to switch
	 * between R/W and R/O pages.
	 * NOTE: we only support cache flush for read only text.
	 */
#ifdef MIPS1
	if ((!MIPS_HAS_R4K_MMU) && prot == (VM_PROT_READ | VM_PROT_EXECUTE)) {
		mips_icache_sync_range(MIPS_PHYS_TO_KSEG0(pa), PAGE_SIZE);
	}
#endif

	if (pmap == pmap_kernel()) {
		if (pg)
			pmap_enter_pv(pmap, va, pg, &npte);

		/* enter entries into kernel pmap */
		pte = kvtopte(va);

		if (MIPS_HAS_R4K_MMU)
			npte |= mips3_paddr_to_tlbpfn(pa) | MIPS3_PG_G;
		else
			npte |= mips1_paddr_to_tlbpfn(pa) |
			    MIPS1_PG_V | MIPS1_PG_G;

		if (wired) {
			pmap->pm_stats.wired_count++;
			npte |= mips_pg_wired_bit();
		}
		if (mips_pg_v(pte->pt_entry) &&
		    mips_tlbpfn_to_paddr(pte->pt_entry) != pa) {
			pmap_remove(pmap, va, va + NBPG);
#ifdef DEBUG
			enter_stats.mchange++;
#endif
		}
		if (!mips_pg_v(pte->pt_entry))
			pmap->pm_stats.resident_count++;
		pte->pt_entry = npte;

		/*
		 * Update the same virtual address entry.
		 */

		tlb_update(va, npte);
		return 0;
	}

	pte = pmap_pte_reserve(pmap, va, flags);
	if (__predict_false(pte == NULL)) {
		return ENOMEM;
	}

	/* Done after case that may sleep/return. */
	if (pg)
		pmap_enter_pv(pmap, va, pg, &npte);

	/*
	 * Now validate mapping with desired protection/wiring.
	 * Assume uniform modified and referenced status for all
	 * MIPS pages in a MACH page.
	 */

	if (MIPS_HAS_R4K_MMU)
		npte |= mips3_paddr_to_tlbpfn(pa);
	else
		npte |= mips1_paddr_to_tlbpfn(pa) | MIPS1_PG_V;

	if (wired) {
		pmap->pm_stats.wired_count++;
		npte |= mips_pg_wired_bit();
	}
	struct pmap_asid_info * const pai = PMAP_PAI(pmap, curcpu());
	bool needsupdate = PMAP_PAI_ASIDVALID_P(pai, curcpu());
#if defined(DEBUG)
	if (pmapdebug & PDB_ENTER) {
		printf("pmap_enter: %p: %#"PRIxVADDR": new pte %#x (pa %#"PRIxPADDR")", pmap, va, npte, pa);
		if (needsupdate)
			printf(" asid %u (%#x)", pai->pai_asid, pai->pai_asid);
		printf("\n");
	}
#endif

#ifdef PARANOIADIAG
	if (PMAP_IS_ACTIVE(pmap)) {
		uint32_t asid;

		__asm volatile("mfc0 %0,$10; nop" : "=r"(asid));
		asid = (MIPS_HAS_R4K_MMU) ? (asid & 0xff) : (asid & 0xfc0) >> 6;
		if (asid != pai->pai_asid) {
			panic("inconsistency for active TLB update: %u <-> %u",
			    asid, pai->pai_asid);
		}
	}
#endif

	asid = pai->pai_asid << MIPS_TLB_PID_SHIFT;
	if (mips_pg_v(pte->pt_entry) &&
	    mips_tlbpfn_to_paddr(pte->pt_entry) != pa) {
		pmap_remove(pmap, va, va + NBPG);
#ifdef DEBUG
		enter_stats.mchange++;
#endif
	}

	if (!mips_pg_v(pte->pt_entry))
		pmap->pm_stats.resident_count++;
	pte->pt_entry = npte;

	if (needsupdate)
		tlb_update(va | asid, npte);

#ifdef MIPS3_PLUS	/* XXX mmu XXX */
	if (MIPS_HAS_R4K_MMU && (prot == (VM_PROT_READ | VM_PROT_EXECUTE))) {
#ifdef DEBUG
		if (pmapdebug & PDB_ENTER)
			printf("pmap_enter: flush I cache va %#"PRIxVADDR" (%#"PRIxPADDR")\n",
			    va - NBPG, pa);
#endif
		/* XXXJRT */
		mips_icache_sync_range_index(va, PAGE_SIZE);
	}
#endif

	return 0;
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	pt_entry_t *pte;
	u_int npte;
	bool managed = PAGE_IS_MANAGED(pa);

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_kenter_pa(%#"PRIxVADDR", %#"PRIxPADDR", %x)\n", va, pa, prot);
#endif

	if (MIPS_HAS_R4K_MMU) {
		npte = mips3_paddr_to_tlbpfn(pa) | MIPS3_PG_WIRED;
		if (prot & VM_PROT_WRITE) {
			npte |= MIPS3_PG_D;
		} else {
			npte |= MIPS3_PG_RO;
		}
		if (managed) {
			npte |= MIPS3_PG_CACHED;
		} else {
			npte |= MIPS3_PG_UNCACHED;
		}
		npte |= MIPS3_PG_V | MIPS3_PG_G;
	} else {
		npte = mips1_paddr_to_tlbpfn(pa) | MIPS1_PG_WIRED;
		if (prot & VM_PROT_WRITE) {
			npte |= MIPS1_PG_D;
		} else {
			npte |= MIPS1_PG_RO;
		}
		if (managed) {
			npte |= 0;
		} else {
			npte |= MIPS1_PG_N;
		}
		npte |= MIPS1_PG_V | MIPS1_PG_G;
	}
	pte = kvtopte(va);
	KASSERT(!mips_pg_v(pte->pt_entry));
	pte->pt_entry = npte;
	tlb_update(va, npte);
}

void
pmap_kremove(vaddr_t va, vsize_t len)
{
	pt_entry_t *pte;
	vaddr_t eva;
	u_int entry;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE))
		printf("pmap_kremove(%#"PRIxVADDR", %#"PRIxVSIZE")\n", va, len);
#endif

	pte = kvtopte(va);
	eva = va + len;
	for (; va < eva; va += PAGE_SIZE, pte++) {
		entry = pte->pt_entry;
		if (!mips_pg_v(entry)) {
			continue;
		}
		if (MIPS_HAS_R4K_MMU) {
#ifndef sbmips	/* XXX XXX if (dcache_is_virtual) - should also check icache virtual && EXEC mapping */
			mips_dcache_wbinv_range(va, PAGE_SIZE);
#endif
			pte->pt_entry = MIPS3_PG_NV | MIPS3_PG_G;
		} else {
			pte->pt_entry = MIPS1_PG_NV;
		}
		tlb_invalidate_addr(va);
	}
}

/*
 *	Routine:	pmap_unwire
 *	Function:	Clear the wired attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void
pmap_unwire(pmap_t pmap, vaddr_t va)
{
	pt_entry_t *pte;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_WIRING))
		printf("pmap_unwire(%p, %#"PRIxVADDR")\n", pmap, va);
#endif
	/*
	 * Don't need to flush the TLB since PG_WIRED is only in software.
	 */
	if (pmap == pmap_kernel()) {
		/* change entries in kernel pmap */
#ifdef PARANOIADIAG
		if (va < VM_MIN_KERNEL_ADDRESS || va >= virtual_end)
			panic("pmap_unwire");
#endif
		pte = kvtopte(va);
	} else {
		pte = pmap_pte_lookup(pmap, va);
#ifdef DIAGNOSTIC
		if (pte == NULL)
			panic("pmap_unwire: pmap %p va %#"PRIxVADDR" invalid STE",
			    pmap, va);
#endif
	}

#ifdef DIAGNOSTIC
	if (mips_pg_v(pte->pt_entry) == 0)
		panic("pmap_unwire: pmap %p va %#"PRIxVADDR" invalid PTE",
		    pmap, va);
#endif

	if (mips_pg_wired(pte->pt_entry)) {
		pte->pt_entry &= ~mips_pg_wired_bit();
		pmap->pm_stats.wired_count--;
	}
#ifdef DIAGNOSTIC
	else {
		printf("pmap_unwire: wiring for pmap %p va %#"PRIxVADDR" "
		    "didn't change!\n", pmap, va);
	}
#endif
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
bool
pmap_extract(pmap_t pmap, vaddr_t va, paddr_t *pap)
{
	paddr_t pa;
	pt_entry_t *pte;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_extract(%p, %#"PRIxVADDR") -> ", pmap, va);
#endif
	if (pmap == pmap_kernel()) {
		if (MIPS_KSEG0_P(va)) {
			pa = MIPS_KSEG0_TO_PHYS(va);
			goto done;
		}
#ifdef _LP64
		if (MIPS_XKPHYS_P(va)) {
			pa = MIPS_XKPHYS_TO_PHYS(va);
			goto done;
		}
#endif
#ifdef DIAGNOSTIC
		if (MIPS_KSEG1_P(va))
			panic("pmap_extract: kseg1 address %#"PRIxVADDR"", va);
#endif
		else
			pte = kvtopte(va);
	} else {
		if (!(pte = pmap_pte_lookup(pmap, va))) {
#ifdef DEBUG
			if (pmapdebug & PDB_FOLLOW)
				printf("not in segmap\n");
#endif
			return false;
		}
	}
	if (!mips_pg_v(pte->pt_entry)) {
#ifdef DEBUG
		if (pmapdebug & PDB_FOLLOW)
			printf("PTE not valid\n");
#endif
		return false;
	}
	pa = mips_tlbpfn_to_paddr(pte->pt_entry) | (va & PGOFSET);
done:
	if (pap != NULL) {
		*pap = pa;
	}
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pa %#"PRIxPADDR"\n", pa);
#endif
	return true;
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vaddr_t dst_addr, vsize_t len,
    vaddr_t src_addr)
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy(%p, %p, %#"PRIxVADDR", %#"PRIxVSIZE", %#"PRIxVADDR")\n",
		    dst_pmap, src_pmap, dst_addr, len, src_addr);
#endif
}

/*
 *	Routine:	pmap_collect
 *	Function:
 *		Garbage collects the physical map system for
 *		pages which are no longer used.
 *		Success need not be guaranteed -- that is, there
 *		may well be pages which are not referenced, but
 *		others may be collected.
 *	Usage:
 *		Called by the pageout daemon when pages are scarce.
 */
void
pmap_collect(pmap_t pmap)
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_collect(%p)\n", pmap);
#endif
}

/*
 *	pmap_zero_page zeros the specified page.
 */
void
pmap_zero_page(paddr_t phys)
{
	vaddr_t va;
#if defined(MIPS3_PLUS)
	struct vm_page *pg;
	pv_entry_t pv;
#endif

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_zero_page(%#"PRIxPADDR")\n", phys);
#endif
#ifdef PARANOIADIAG
	if (!(phys < MIPS_MAX_MEM_ADDR))
		printf("pmap_zero_page(%#"PRIxPADDR") nonphys\n", phys);
#endif
#ifdef _LP64
	KASSERT(mips_options.mips3_xkphys_cached);
	va = MIPS_PHYS_TO_XKPHYS_CACHED(phys);
#else
	va = MIPS_PHYS_TO_KSEG0(phys);
#endif

#if defined(MIPS3_PLUS)	/* XXX mmu XXX */
	pg = PHYS_TO_VM_PAGE(phys);
	if (mips_cache_info.mci_cache_virtual_alias) {
		pv = pg->mdpage.pvh_list;
		if ((pv->pv_flags & PV_UNCACHED) == 0 &&
		    mips_cache_indexof(pv->pv_va) != mips_cache_indexof(va))
			mips_dcache_wbinv_range_index(pv->pv_va, PAGE_SIZE);
	}
#endif

	mips_pagezero((void *)va);

#if defined(MIPS3_PLUS)	/* XXX mmu XXX */
	/*
	 * If we have a virtually-indexed, physically-tagged WB cache,
	 * and no L2 cache to warn of aliased mappings,	we must force a
	 * writeback of the destination out of the L1 cache.  If we don't,
	 * later reads (from virtual addresses mapped to the destination PA)
	 * might read old stale DRAM footprint, not the just-written data.
	 *
	 * XXXJRT This is totally disgusting.
	 */
	if (MIPS_HAS_R4K_MMU)	/* XXX VCED on kernel stack is not allowed */
		mips_dcache_wbinv_range(va, PAGE_SIZE);
#endif	/* MIPS3_PLUS */
}

/*
 *	pmap_copy_page copies the specified page.
 */
void
pmap_copy_page(paddr_t src, paddr_t dst)
{
	vaddr_t src_va, dst_va;
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy_page(%#"PRIxPADDR", %#"PRIxPADDR")\n", src, dst);
#endif
#ifdef _LP64
	KASSERT(mips_options.mips3_xkphys_cached);
	src_va = MIPS_PHYS_TO_XKPHYS_CACHED(src);
	dst_va = MIPS_PHYS_TO_XKPHYS_CACHED(dst);
#else
	src_va = MIPS_PHYS_TO_KSEG0(src);
	dst_va = MIPS_PHYS_TO_KSEG0(dst);
#endif
#if !defined(_LP64) && defined(PARANOIADIAG)
	if (!(src < MIPS_MAX_MEM_ADDR))
		printf("pmap_copy_page(%#"PRIxPADDR") src nonphys\n", src);
	if (!(dst < MIPS_MAX_MEM_ADDR))
		printf("pmap_copy_page(%#"PRIxPADDR") dst nonphys\n", dst);
#endif

#if defined(MIPS3_PLUS) /* XXX mmu XXX */
	/*
	 * If we have a virtually-indexed, physically-tagged cache,
	 * and no L2 cache to warn of aliased mappings, we must force an
	 * write-back of all L1 cache lines of the source physical address,
	 * irrespective of their virtual address (cache indexes).
	 * If we don't, our copy loop might read and copy stale DRAM
	 * footprint instead of the fresh (but dirty) data in a WB cache.
	 * XXX invalidate any cached lines of the destination PA
	 *     here also?
	 *
	 * It would probably be better to map the destination as a
	 * write-through no allocate to reduce cache thrash.
	 */
	if (mips_cache_info.mci_cache_virtual_alias) {
		/*XXX FIXME Not very sophisticated */
		mips_flushcache_allpvh(src);
#if 0
		mips_flushcache_allpvh(dst);
#endif
	}
#endif	/* MIPS3_PLUS */

	mips_pagecopy((void *)dst_va, (void *)src_va);

#if defined(MIPS3_PLUS) /* XXX mmu XXX */
	/*
	 * If we have a virtually-indexed, physically-tagged WB cache,
	 * and no L2 cache to warn of aliased mappings,	we must force a
	 * writeback of the destination out of the L1 cache.  If we don't,
	 * later reads (from virtual addresses mapped to the destination PA)
	 * might read old stale DRAM footprint, not the just-written data.
	 * XXX  Do we need to also invalidate any cache lines matching
	 *      the destination as well?
	 *
	 * XXXJRT -- This is totally disgusting.
	 */
	if (mips_cache_info.mci_cache_virtual_alias) {
		mips_dcache_wbinv_range(src_va, PAGE_SIZE);
		mips_dcache_wbinv_range(dst_va, PAGE_SIZE);
	}
#endif	/* MIPS3_PLUS */
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
bool
pmap_clear_reference(struct vm_page *pg)
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_reference(%#"PRIxPADDR")\n",
		    VM_PAGE_TO_PHYS(pg));
#endif
	return pmap_clear_page_attributes(pg, PV_REFERENCED);
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */
bool
pmap_is_referenced(struct vm_page *pg)
{

	return pg->mdpage.pvh_attrs & PV_REFERENCED;
}

/*
 *	Clear the modify bits on the specified physical page.
 */
bool
pmap_clear_modify(struct vm_page *pg)
{
	struct pv_entry *pv;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_modify(%#"PRIxPADDR")\n", VM_PAGE_TO_PHYS(pg));
#endif
	if (!pmap_clear_page_attributes(pg, PV_MODIFIED))
		return false;
	pv = pg->mdpage.pvh_list;
	if (pv->pv_pmap == NULL) {
		return true;
	}

	/*
	 * remove write access from any pages that are dirty
	 * so we can tell if they are written to again later.
	 * flush the VAC first if there is one.
	 */
	struct cpu_info * const ci = curcpu();
	for (; pv; pv = pv->pv_next) {
		pmap_t pmap = pv->pv_pmap;
		struct pmap_asid_info * const pai = PMAP_PAI(pmap, ci);
		vaddr_t va = pv->pv_va;
		pt_entry_t *pte;
		uint32_t asid;
		uint32_t pt_entry;
		if (pmap == pmap_kernel()) {
			pte = kvtopte(va);
			asid = 0;
		} else {
			pte = pmap_pte_lookup(pmap, va);
			KASSERT(pte);
			asid = pai->pai_asid << MIPS_TLB_PID_SHIFT;
		}
		pt_entry = pte->pt_entry & ~mips_pg_m_bit();
		if (pte->pt_entry == pt_entry) {
			continue;
		}
		if (MIPS_HAS_R4K_MMU &&
		    (1 || mips_cache_info.mci_cache_virtual_alias
		     || (mips_options.mips_cpu_flags & CPU_MIPS_D_CACHE_COHERENT) == 0)) {
#ifdef MULTIPROCESSORX
#error fix me
#endif
			if (PMAP_IS_ACTIVE(pmap)) {
				mips_dcache_wbinv_range(va, PAGE_SIZE);
			} else {
				mips_dcache_wbinv_range_index(va, PAGE_SIZE);
			}
		}
		pte->pt_entry = pt_entry;
		if (PMAP_PAI_ASIDVALID_P(pai, ci)) {
			tlb_invalidate_addr(va | asid);
		}
	}
	return true;
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */
bool
pmap_is_modified(struct vm_page *pg)
{

	return pg->mdpage.pvh_attrs & PV_MODIFIED;
}

/*
 *	pmap_set_modified:
 *
 *	Sets the page modified reference bit for the specified page.
 */
void
pmap_set_modified(paddr_t pa)
{
	struct vm_page *pg = PHYS_TO_VM_PAGE(pa);
	pmap_set_page_attributes(pg, PV_MODIFIED | PV_REFERENCED);
}

/******************** misc. functions ********************/

/*
 * Allocate a TLB address space tag (called ASID or TLBPID) and return it.
 * Each cpu has its own ASID space which might be a subset of the entire
 * ASID available (A core might have 256 ASIDs shared among N hw-threads).
 * To avoid dealing with locking, we just partition the ASIDs among the
 * hw-threads so each has its own independent space.
 *
 * Since all hw-threads share the TLB, we can't invalidate all non-global TLB
 * entries.  Instead we need to make sure they match the proper range of ASIDs
 * reserved for that CPU.
 *
 * Therefore, when we allocate a new ASID, we just take the next number. When
 * we run out of numbers, we flush the ASIDs the TLB, increment the generation
 * count and start over. The low ASID of the range is reserved for kernel use
 * (even though we only use 0 for that purpose).
 */
uint32_t
pmap_tlb_asid_alloc(pmap_t pmap, struct cpu_info *ci)
{
	struct pmap_asid_info * const pai = PMAP_PAI(pmap, ci);

	if (!PMAP_PAI_ASIDVALID_P(pai, ci)) {
		if (ci->ci_pmap_asid_next == ci->ci_pmap_asid_max) {
			tlb_invalidate_asids(ci->ci_pmap_asid_reserved + 1,
			    ci->ci_pmap_asid_max);
			ci->ci_pmap_asid_generation++; /* ok to wrap to 0 */
			ci->ci_pmap_asid_next =		/* 0 means invalid */
			    ci->ci_pmap_asid_reserved + 1;
		}
		pai->pai_asid = ci->ci_pmap_asid_next++;
		pai->pai_asid_generation = ci->ci_pmap_asid_generation;
	}

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_TLBPID)) {
		printf("pmap_tlb_asid_alloc: curlwp %d.%d '%s' ",
		    curlwp->l_proc->p_pid, curlwp->l_lid,
		    curlwp->l_proc->p_comm);
		printf("segtab %p asid %d\n", pmap->pm_segtab, pai->pai_asid);
	}
#endif
	return pai->pai_asid;
}

/******************** pv_entry management ********************/

/*
 * Enter the pmap and virtual address into the
 * physical to virtual map table.
 */
void
pmap_enter_pv(pmap_t pmap, vaddr_t va, struct vm_page *pg, u_int *npte)
{
	pv_entry_t pv, npv;

	pv = pg->mdpage.pvh_list;
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter: pv %p: was %#"PRIxVADDR"/%p/%p\n",
		    pv, pv->pv_va, pv->pv_pmap, pv->pv_next);
#endif
#if defined(MIPS3_NO_PV_UNCACHED)
again:
#endif
	if (pv->pv_pmap == NULL) {

		/*
		 * No entries yet, use header as the first entry
		 */

#ifdef DEBUG
		if (pmapdebug & PDB_PVENTRY)
			printf("pmap_enter: first pv: pmap %p va %#"PRIxVADDR"\n",
			    pmap, va);
		enter_stats.firstpv++;
#endif
		pv->pv_va = va;
		pv->pv_flags &= ~PV_UNCACHED;
		pv->pv_pmap = pmap;
		pv->pv_next = NULL;
	} else {
#if defined(MIPS3_PLUS) /* XXX mmu XXX */
		if (mips_cache_info.mci_cache_virtual_alias) {
			/*
			 * There is at least one other VA mapping this page.
			 * Check if they are cache index compatible.
			 */

#if defined(MIPS3_NO_PV_UNCACHED)

			/*
			 * Instead of mapping uncached, which some platforms
			 * cannot support, remove the mapping from the pmap.
			 * When this address is touched again, the uvm will
			 * fault it in.  Because of this, each page will only
			 * be mapped with one index at any given time.
			 */

			for (npv = pv; npv; npv = npv->pv_next) {
				if (mips_cache_indexof(npv->pv_va) !=
				    mips_cache_indexof(va)) {
					pmap_remove(npv->pv_pmap, npv->pv_va,
					    npv->pv_va + PAGE_SIZE);
					pmap_update(npv->pv_pmap);
					goto again;
				}
			}
#else	/* !MIPS3_NO_PV_UNCACHED */
			if (!(pv->pv_flags & PV_UNCACHED)) {
				for (npv = pv; npv; npv = npv->pv_next) {

					/*
					 * Check cache aliasing incompatibility.
					 * If one exists, re-map this page
					 * uncached until all mappings have
					 * the same index again.
					 */
					if (mips_cache_indexof(npv->pv_va) !=
					    mips_cache_indexof(va)) {
						pmap_page_cache(pg,PV_UNCACHED);
						mips_dcache_wbinv_range_index(
						    pv->pv_va, PAGE_SIZE);
						*npte = (*npte &
						    ~MIPS3_PG_CACHEMODE) |
						    MIPS3_PG_UNCACHED;
#ifdef DEBUG
						enter_stats.ci++;
#endif
						break;
					}
				}
			} else {
				*npte = (*npte & ~MIPS3_PG_CACHEMODE) |
				    MIPS3_PG_UNCACHED;
			}
#endif	/* !MIPS3_NO_PV_UNCACHED */
		}
#endif /* MIPS3_PLUS */

		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 *
		 * Note: the entry may already be in the table if
		 * we are only changing the protection bits.
		 */

		for (npv = pv; npv; npv = npv->pv_next) {
			if (pmap == npv->pv_pmap && va == npv->pv_va) {
#ifdef PARANOIADIAG
				pt_entry_t *pte;
				unsigned entry;

				if (pmap == pmap_kernel())
					entry = kvtopte(va)->pt_entry;
				else {
					pte = pmap_pte_lookup(pmap, va);
					if (pte) {
						entry = pte->pt_entry;
					} else
						entry = 0;
				}
				if (!mips_pg_v(entry) ||
				    mips_tlbpfn_to_paddr(entry) !=
				    VM_PAGE_TO_PHYS(pg))
					printf(
		"pmap_enter: found va %#"PRIxVADDR" pa %#"PRIxPADDR" in pv_table but != %x\n",
					    va, VM_PAGE_TO_PHYS(pg),
					    entry);
#endif
				return;
			}
		}
#ifdef DEBUG
		if (pmapdebug & PDB_PVENTRY)
			printf("pmap_enter: new pv: pmap %p va %#"PRIxVADDR"\n",
			    pmap, va);
#endif
		npv = (pv_entry_t)pmap_pv_alloc();
		if (npv == NULL)
			panic("pmap_enter_pv: pmap_pv_alloc() failed");
		npv->pv_va = va;
		npv->pv_pmap = pmap;
		npv->pv_flags = pv->pv_flags;
		npv->pv_next = pv->pv_next;
		pv->pv_next = npv;
#ifdef DEBUG
		if (!npv->pv_next)
			enter_stats.secondpv++;
#endif
	}
}

/*
 * Remove a physical to virtual address translation.
 * If cache was inhibited on this page, and there are no more cache
 * conflicts, restore caching.
 * Flush the cache if the last page is removed (should always be cached
 * at this point).
 */
void
pmap_remove_pv(pmap_t pmap, vaddr_t va, struct vm_page *pg)
{
	pv_entry_t pv, npv;
	int last;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PVENTRY))
		printf("pmap_remove_pv(%p, %#"PRIxVADDR", %#"PRIxPADDR")\n", pmap, va,
		    VM_PAGE_TO_PHYS(pg));
#endif

	pv = pg->mdpage.pvh_list;

	/*
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry.  In either case we free the now unused entry.
	 */

	last = 0;
	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		npv = pv->pv_next;
		if (npv) {

			/*
			 * Copy current modified and referenced status to
			 * the following entry before copying.
			 */
			npv->pv_flags |=
			    pv->pv_flags & (PV_MODIFIED | PV_REFERENCED);
			*pv = *npv;
			pmap_pv_free(npv);
		} else {
			pv->pv_pmap = NULL;
			last = 1;	/* Last mapping removed */
		}
#ifdef DEBUG
		remove_stats.pvfirst++;
#endif
	} else {
		for (npv = pv->pv_next; npv; pv = npv, npv = npv->pv_next) {
#ifdef DEBUG
			remove_stats.pvsearch++;
#endif
			if (pmap == npv->pv_pmap && va == npv->pv_va)
				break;
		}
		if (npv) {
			pv->pv_next = npv->pv_next;
			pmap_pv_free(npv);
		}
	}
#ifdef MIPS3_PLUS	/* XXX mmu XXX */
#if !defined(MIPS3_NO_PV_UNCACHED)
	if (MIPS_HAS_R4K_MMU && pv->pv_flags & PV_UNCACHED) {

		/*
		 * Page is currently uncached, check if alias mapping has been
		 * removed.  If it was, then reenable caching.
		 */

		pv = pg->mdpage.pvh_list;
		for (npv = pv->pv_next; npv; npv = npv->pv_next) {
			if (mips_cache_indexof(pv->pv_va ^ npv->pv_va))
				break;
		}
		if (npv == NULL)
			pmap_page_cache(pg, 0);
	}
#endif
	if (MIPS_HAS_R4K_MMU && last != 0)
		mips_dcache_wbinv_range_index(va, PAGE_SIZE);
#endif	/* MIPS3_PLUS */
}

/*
 * pmap_pv_page_alloc:
 *
 *	Allocate a page for the pv_entry pool.
 */
void *
pmap_pv_page_alloc(struct pool *pp, int flags)
{
	const struct vm_page *pg = PMAP_ALLOC_POOLPAGE(UVM_PGA_USERESERVE);
	if (pg == NULL)
		return NULL;

	const paddr_t pa = VM_PAGE_TO_PHYS(pg);
#ifdef _LP64
	KASSERT(mips_options.mips3_xkphys_cached);
	const vaddr_t va = MIPS_PHYS_TO_XKPHYS_CACHED(pa);
#else
	const vaddr_t va = MIPS_PHYS_TO_KSEG0(pa);
#endif
#if defined(MIPS3_PLUS)
	if (mips_cache_info.mci_cache_virtual_alias) {
		pv_entry_t pv = pg->mdpage.pvh_list;
		if ((pv->pv_flags & PV_UNCACHED) == 0 &&
		    mips_cache_indexof(pv->pv_va) != mips_cache_indexof(va))
			mips_dcache_wbinv_range_index(pv->pv_va, PAGE_SIZE);
	}
#endif
	return (void *)va;
}

/*
 * pmap_pv_page_free:
 *
 *	Free a pv_entry pool page.
 */
void
pmap_pv_page_free(struct pool *pp, void *v)
{
	paddr_t phys;

#ifdef MIPS3_PLUS
	if (mips_cache_info.mci_cache_virtual_alias)
		mips_dcache_inv_range((vaddr_t)v, PAGE_SIZE);
#endif
#ifdef _LP64
	KASSERT(MIPS_XKPHYS_P(v));
	phys = MIPS_XKPHYS_TO_PHYS((vaddr_t)v);
#else
	phys = MIPS_KSEG0_TO_PHYS((vaddr_t)v);
#endif
	uvm_pagefree(PHYS_TO_VM_PAGE(phys));
}

pt_entry_t *
pmap_pte(pmap_t pmap, vaddr_t va)
{
	pt_entry_t *pte;

	if (pmap == pmap_kernel())
		pte = kvtopte(va);
	else
		pte = pmap_pte_lookup(pmap, va);
	return pte;
}

#ifdef MIPS3_PLUS	/* XXX mmu XXX */
/*
 * Find first virtual address >= *vap that doesn't cause
 * a cache alias conflict.
 */
void
pmap_prefer(vaddr_t foff, vaddr_t *vap, int td)
{
	const struct mips_cache_info * const mci = &mips_cache_info;
	vaddr_t	va;
	vsize_t d;

	if (MIPS_HAS_R4K_MMU) {
		va = *vap;

		d = foff - va;
		d &= mci->mci_cache_prefer_mask;
		if (td && d)
			d = -((-d) & mci->mci_cache_prefer_mask);
		*vap = va + d;
	}
}
#endif	/* MIPS3_PLUS */

struct vm_page *
mips_pmap_alloc_poolpage(int flags)
{
	/*
	 * On 32bit kernels, we must make sure that we only allocate pages that
	 * can be mapped via KSEG0.  On 64bit kernels, try to allocated from
	 * the first 4G.  If all memory is in KSEG0/4G, then we can just
	 * use the default freelist otherwise we must use the pool page list.
	 */
	if (mips_poolpage_vmfreelist != VM_FREELIST_DEFAULT)
		return uvm_pagealloc_strat(NULL, 0, NULL, flags,
		    UVM_PGA_STRAT_ONLY, mips_poolpage_vmfreelist);

	return uvm_pagealloc(NULL, 0, NULL, flags);
}

vaddr_t
mips_pmap_map_poolpage(paddr_t pa)
{
	vaddr_t va;
#if defined(MIPS3_PLUS)
	struct vm_page *pg;
	pv_entry_t pv;
#endif

#ifdef _LP64
	KASSERT(mips_options.mips3_xkphys_cached);
	va = MIPS_PHYS_TO_XKPHYS_CACHED(pa);
#else
	if (pa > MIPS_PHYS_MASK)
		panic("mips_pmap_map_poolpage: "
		    "pa #%"PRIxPADDR" can not be mapped into KSEG0", pa);

	va = MIPS_PHYS_TO_KSEG0(pa);
#endif
#if defined(MIPS3_PLUS)
	if (mips_cache_info.mci_cache_virtual_alias) {
		pg = PHYS_TO_VM_PAGE(pa);
		pv = pg->mdpage.pvh_list;
		if ((pv->pv_flags & PV_UNCACHED) == 0 &&
		    mips_cache_indexof(pv->pv_va) != mips_cache_indexof(va))
			mips_dcache_wbinv_range_index(pv->pv_va, PAGE_SIZE);
	}
#endif
	return va;
}

paddr_t
mips_pmap_unmap_poolpage(vaddr_t va)
{
	paddr_t pa;

#ifdef _LP64
	KASSERT(MIPS_XKPHYS_P(va));
	pa = MIPS_XKPHYS_TO_PHYS(va);
#else
	pa = MIPS_KSEG0_TO_PHYS(va);
#endif
#if defined(MIPS3_PLUS)
	if (mips_cache_info.mci_cache_virtual_alias) {
		mips_dcache_inv_range(va, PAGE_SIZE);
	}
#endif
	return pa;
}

/******************** page table page management ********************/

/* TO BE DONE */
