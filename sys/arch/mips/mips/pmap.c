/*	$NetBSD: pmap.c,v 1.97.2.1 2000/06/22 17:01:37 minoura Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 *	@(#)pmap.c	8.4 (Berkeley) 1/26/94
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: pmap.c,v 1.97.2.1 2000/06/22 17:01:37 minoura Exp $");

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

#include "opt_sysv.h"
#include "opt_cputype.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/buf.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <uvm/uvm.h>

#include <mips/cpuregs.h>
#include <mips/locore.h>
#include <mips/pte.h>

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

struct pmap	kernel_pmap_store;

paddr_t avail_start;	/* PA of first available physical page */
paddr_t avail_end;	/* PA of last available physical page */
vaddr_t virtual_avail;	/* VA of first avail page (after kernel bss)*/
vaddr_t virtual_end;	/* VA of last avail page (end of kernel AS) */

struct pv_entry	*pv_table;
int		 pv_table_npages;

struct segtab	*free_segtab;		/* free list kept locally */
pt_entry_t	*Sysmap;		/* kernel pte table */
unsigned	Sysmapsize;		/* number of pte's in Sysmap */

unsigned pmap_max_asid;			/* max ASID supported by the system */
unsigned pmap_next_asid;		/* next free ASID to use */
unsigned pmap_asid_generation;		/* current ASID generation */
#define PMAP_ASID_RESERVED 0

boolean_t	pmap_initialized = FALSE;

#define PAGE_IS_MANAGED(pa)	\
	    (pmap_initialized == TRUE && vm_physseg_find(atop(pa), NULL) != -1)

#define PMAP_IS_ACTIVE(pm)	\
	    (curproc != NULL && (pm) == curproc->p_vmspace->vm_map.pmap)

#define	pa_to_pvh(pa)							\
({									\
	int bank_, pg_;							\
									\
	bank_ = vm_physseg_find(atop((pa)), &pg_);			\
	&vm_physmem[bank_].pmseg.pvent[pg_];				\
})

#define	pa_to_attribute(pa)						\
({									\
	int bank_, pg_;							\
									\
	bank_ = vm_physseg_find(atop((pa)), &pg_);			\
	&vm_physmem[bank_].pmseg.pvent[pg_].pv_flags; 				\
})

/* Forward function declarations */
void pmap_remove_pv __P((pmap_t pmap, vaddr_t va, paddr_t pa));
void pmap_asid_alloc __P((pmap_t pmap));
void pmap_enter_pv __P((pmap_t, vaddr_t, paddr_t, u_int *));
pt_entry_t *pmap_pte __P((pmap_t, vaddr_t));

#ifdef MIPS3
void pmap_page_cache __P((paddr_t, int));
void mips_dump_segtab __P((struct proc *));
#endif

void pmap_pinit __P((pmap_t));
void pmap_release __P((pmap_t));

#if defined(MIPS3_L2CACHE_ABSENT)
static void mips_flushcache_allpvh __P((paddr_t));

/*
 * Flush virtual addresses associated with a given physical address
 */
static void
mips_flushcache_allpvh(paddr_t pa)
{
	struct pv_entry *pv = pa_to_pvh(pa);

	while (pv) {
		MachFlushDCache(pv->pv_va, NBPG);
		pv = pv->pv_next;
	}
}
#endif

/*
 *	Bootstrap the system enough to run with virtual memory.
 *	firstaddr is the first unused kseg0 address (not page aligned).
 */
void
pmap_bootstrap()
{

	/*
	 * Compute the number of pages kmem_map will have.
	 */
	kmeminit_nkmempages();

	/*
	 * Figure out how many PTE's are necessary to map the kernel.
	 * The '2048' comes from PAGER_MAP_SIZE in vm_pager_init().
	 * This should be kept in sync.
	 * We also reserve space for kmem_alloc_pageable() for vm_fork().
	 */
	Sysmapsize = (VM_PHYS_SIZE +
		nbuf * MAXBSIZE + 16 * NCARGS) / NBPG + 2048 +
		(maxproc * UPAGES) + nkmempages;

#ifdef SYSVSHM
	Sysmapsize += shminfo.shmall;
#endif
#ifdef KSEG2IOBUFSIZE
	Sysmapsize += (KSEG2IOBUFSIZE >> PGSHIFT);
#endif
	Sysmap = (pt_entry_t *)
	    pmap_steal_memory(sizeof(pt_entry_t) * Sysmapsize, NULL, NULL);

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
	    pmap_steal_memory(sizeof(struct pv_entry) * pv_table_npages,
		NULL, NULL);

	/*
	 * Initialize `FYI' variables.	Note we're relying on
	 * the fact that BSEARCH sorts the vm_physmem[] array
	 * for us.
	 */
	avail_start = ptoa(vm_physmem[0].start);
	avail_end = ptoa(vm_physmem[vm_nphysseg - 1].end);
	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MIN_KERNEL_ADDRESS + Sysmapsize * NBPG;

	/*
	 * Initialize the kernel pmap.
	 */
	simple_lock_init(&pmap_kernel()->pm_lock);
	pmap_kernel()->pm_count = 1;
	pmap_kernel()->pm_asid = PMAP_ASID_RESERVED;
	pmap_kernel()->pm_asidgen = 0;

	pmap_max_asid = MIPS_TLB_NUM_PIDS;
	pmap_next_asid = 1;
	pmap_asid_generation = 0;

	MachSetPID(0);

#ifdef MIPS3
	/*
	 * The R4?00 stores only one copy of the Global bit in the
	 * translation lookaside buffer for each 2 page entry.
	 * Thus invalid entrys must have the Global bit set so
	 * when Entry LO and Entry HI G bits are anded together
	 * they will produce a global bit to store in the tlb.
	 */
	if (CPUISMIPS3) {
		int i;
		pt_entry_t *spte;

		for (i = 0, spte = Sysmap; i < Sysmapsize; i++, spte++)
			spte->pt_entry = MIPS3_PG_G;
	}
#endif
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
 */
vaddr_t
pmap_steal_memory(size, vstartp, vendp)
	vsize_t size;
	vaddr_t *vstartp, *vendp;
{
	int bank, npgs, x;
	paddr_t pa;
	vaddr_t va;

	size = round_page(size);
	npgs = atop(size);

	for (bank = 0; bank < vm_nphysseg; bank++) {
		if (uvm.page_init_done == TRUE)
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
			for (x = bank; x < vm_nphysseg; x++) {
				/* structure copy */
				vm_physmem[x] = vm_physmem[x + 1];
			}
		}

		/*
		 * Fill these in for the caller; we don't modify them,
		 * but thge upper layers still want to know.
		 */
		if (vstartp)
			*vstartp = round_page(virtual_avail);
		if (vendp)
			*vendp = trunc_page(virtual_end);

		va = MIPS_PHYS_TO_KSEG0(pa);
		memset((caddr_t)va, 0, size);
		return (va);
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
pmap_init()
{
	vsize_t		s;
	int		bank;
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
		vm_physmem[bank].pmseg.pvent = pv;
		pv += s;
	}

	/*
	 * Now it is safe to enable pv entry recording.
	 */
	pmap_initialized = TRUE;
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
pmap_create()
{
	pmap_t pmap;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_create()\n");
#endif

	pmap = (pmap_t)malloc(sizeof *pmap, M_VMPMAP, M_WAITOK);
	memset(pmap, 0, sizeof(*pmap));
	pmap_pinit(pmap);
	return (pmap);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	struct pmap *pmap;
{
	int i, s;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_pinit(%p)\n", pmap);
#endif
	simple_lock_init(&pmap->pm_lock);
	pmap->pm_count = 1;
	if (free_segtab) {
		s = splimp();
		pmap->pm_segtab = free_segtab;
		free_segtab = *(struct segtab **)free_segtab;
		pmap->pm_segtab->seg_tab[0] = NULL;
		splx(s);
	} else {
		struct segtab *stp;
		vm_page_t mem;

		do {
			mem = uvm_pagealloc(NULL, 0, NULL,
			    UVM_PGA_USERESERVE|UVM_PGA_ZERO);
			if (mem == NULL) {
				/*
				 * XXX What else can we do?  Could we
				 * XXX deadlock here?
				 */
				uvm_wait("pmap_pinit");
			}
		} while (mem == NULL);

		pmap->pm_segtab = stp = (struct segtab *)
			MIPS_PHYS_TO_KSEG0(VM_PAGE_TO_PHYS(mem));
		i = NBPG / sizeof(struct segtab);
		s = splimp();
		while (--i != 0) {
			stp++;
			*(struct segtab **)stp = free_segtab;
			free_segtab = stp;
		}
		splx(s);
	}
#ifdef PARANOIADIAG
	for (i = 0; i < PMAP_SEGTABSIZE; i++)
		if (pmap->pm_segtab->seg_tab[i] != 0)
			panic("pmap_pinit: pm_segtab != 0");
#endif
	pmap->pm_asid = PMAP_ASID_RESERVED;
	pmap->pm_asidgen = pmap_asid_generation;
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int count;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_destroy(%p)\n", pmap);
#endif
	if (pmap == NULL)
		return;

	simple_lock(&pmap->pm_lock);
	count = --pmap->pm_count;
	simple_unlock(&pmap->pm_lock);
	if (count == 0) {
		pmap_release(pmap);
		free((caddr_t)pmap, M_VMPMAP);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	pmap_t pmap;
{

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_release(%p)\n", pmap);
#endif

	if (pmap->pm_segtab) {
		pt_entry_t *pte;
		int i;
		int s;
#ifdef PARANOIADIAG
		int j;
#endif

		for (i = 0; i < PMAP_SEGTABSIZE; i++) {
			/* get pointer to segment map */
			pte = pmap->pm_segtab->seg_tab[i];
			if (!pte)
				continue;
#ifdef PARANOIADIAG
			for (j = 0; j < NPTEPG; j++) {
				if ((pte+j)->pt_entry)
					panic("pmap_release: segmap not empty");
			}
#endif

#ifdef MIPS3
			/*
			 * The pica pmap.c flushed the segmap pages here.  I'm
			 * not sure why, but I suspect it's because the page(s)
			 * were being accessed by KSEG0 (cached) addresses and
			 * may cause cache coherency problems when the page
			 * is reused with KSEG2 (mapped) addresses.  This may
			 * cause problems on machines without secondary caches.
			 */
			if (CPUISMIPS3)
				MachHitFlushDCache((vaddr_t)pte, PAGE_SIZE);
#endif
			uvm_pagefree(PHYS_TO_VM_PAGE(MIPS_KSEG0_TO_PHYS(pte)));

			pmap->pm_segtab->seg_tab[i] = NULL;
		}
		s = splimp();
		*(struct segtab **)pmap->pm_segtab = free_segtab;
		free_segtab = pmap->pm_segtab;
		splx(s);
		pmap->pm_segtab = NULL;
	}
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t pmap;
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_reference(%p)\n", pmap);
#endif
	if (pmap != NULL) {
		simple_lock(&pmap->pm_lock);
		pmap->pm_count++;
		simple_unlock(&pmap->pm_lock);
	}
}

/*
 *	Make a new pmap (vmspace) active for the given process.
 */
void
pmap_activate(p)
	struct proc *p;
{
	pmap_t pmap = p->p_vmspace->vm_map.pmap;

	pmap_asid_alloc(pmap);
	if (p == curproc) {
#ifdef	MIPS3
		if (CPUISMIPS3) {
			mips3_write_xcontext_upper((u_int32_t)pmap->pm_segtab);
		}
#endif
		MachSetPID(pmap->pm_asid);
	}
	p->p_addr->u_pcb.pcb_segtab = pmap->pm_segtab; /* XXX */
}

/*
 *	Make a previously active pmap (vmspace) inactive.
 */
void
pmap_deactivate(p)
	struct proc *p;
{

	/* Nothing to do. */
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap, sva, eva)
	pmap_t pmap;
	vaddr_t sva, eva;
{
	vaddr_t nssva;
	pt_entry_t *pte;
	unsigned entry;
	unsigned asid, needflush;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove(%p, %lx, %lx)\n", pmap, sva, eva);
	remove_stats.calls++;
#endif
	if (pmap == NULL)
		return;

	if (pmap == pmap_kernel()) {
		pt_entry_t *pte;

		/* remove entries from kernel pmap */
#ifdef PARANOIADIAG
		if (sva < VM_MIN_KERNEL_ADDRESS || eva >= virtual_end)
			panic("pmap_remove: kva not in range");
#endif
		pte = kvtopte(sva);
		for (; sva < eva; sva += NBPG, pte++) {
			entry = pte->pt_entry;
			if (!mips_pg_v(entry))
				continue;
			if (mips_pg_wired(entry))
				pmap->pm_stats.wired_count--;
			pmap->pm_stats.resident_count--;
			pmap_remove_pv(pmap, sva, mips_tlbpfn_to_paddr(entry));
			if (CPUISMIPS3)
				/* See above about G bit */
				pte->pt_entry = MIPS3_PG_NV | MIPS3_PG_G;
			else
				pte->pt_entry = MIPS1_PG_NV;

			/*
			 * Flush the TLB for the given address.
			 */
			MIPS_TBIS(sva);
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
		unsigned asid;

		__asm __volatile("mfc0 %0,$10; nop" : "=r"(asid));
		asid = (CPUISMIPS3) ? (asid & 0xff) : (asid & 0xfc0) >> 6;
		if (asid != pmap->pm_asid) {
			panic("inconsistency for active TLB flush: %d <-> %d",
				asid, pmap->pm_asid);
		}
	}
#endif
	asid = pmap->pm_asid << MIPS_TLB_PID_SHIFT;
	needflush = (pmap->pm_asidgen == pmap_asid_generation);
	while (sva < eva) {
		nssva = mips_trunc_seg(sva) + NBSEG;
		if (nssva == 0 || nssva > eva)
			nssva = eva;
		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		if (!(pte = pmap_segmap(pmap, sva))) {
			sva = nssva;
			continue;
		}
		/*
		 * Invalidate every valid mapping within this segment.
		 */
		pte += (sva >> PGSHIFT) & (NPTEPG - 1);
		for (; sva < nssva; sva += NBPG, pte++) {
			entry = pte->pt_entry;
			if (!mips_pg_v(entry))
				continue;
			if (mips_pg_wired(entry))
				pmap->pm_stats.wired_count--;
			pmap->pm_stats.resident_count--;
			pmap_remove_pv(pmap, sva, mips_tlbpfn_to_paddr(entry));
			pte->pt_entry = mips_pg_nv_bit();
			/*
			 * Flush the TLB for the given address.
			 */
			if (needflush) {
				MIPS_TBIS(sva | asid);
#ifdef DEBUG
				remove_stats.flushes++;
#endif
			}
		}
	}
}

/*
 *	pmap_page_protect:
 *
 *	Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(pg, prot)
	struct vm_page *pg;
	vm_prot_t prot;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	pv_entry_t pv;
	vaddr_t va;
	int s;

#ifdef DEBUG
	if ((pmapdebug & (PDB_FOLLOW|PDB_PROTECT)) ||
	    (prot == VM_PROT_NONE && (pmapdebug & PDB_REMOVE)))
		printf("pmap_page_protect(%lx, %x)\n", pa, prot);
#endif
	if (!PAGE_IS_MANAGED(pa))
		return;

	switch (prot) {
	case VM_PROT_READ|VM_PROT_WRITE:
	case VM_PROT_ALL:
		break;

	/* copy_on_write */
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		pv = pa_to_pvh(pa);
		s = splimp();
		/*
		 * Loop over all current mappings setting/clearing as appropos.
		 */
		if (pv->pv_pmap != NULL) {
			for (; pv; pv = pv->pv_next) {
				va = pv->pv_va;

				/*
				 * XXX don't write protect pager mappings
				 */
				if (va >= uvm.pager_sva && va < uvm.pager_eva)
					continue;
				pmap_protect(pv->pv_pmap, va, va + PAGE_SIZE,
					prot);
			}
		}
		splx(s);
		break;

	/* remove_all */
	default:
		pv = pa_to_pvh(pa);
		s = splimp();
		while (pv->pv_pmap != NULL) {
			pmap_remove(pv->pv_pmap, pv->pv_va,
				    pv->pv_va + PAGE_SIZE);
		}
		splx(s);
	}
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap, sva, eva, prot)
	pmap_t pmap;
	vaddr_t sva, eva;
	vm_prot_t prot;
{
	vaddr_t nssva;
	pt_entry_t *pte;
	unsigned entry;
	u_int p;
	unsigned asid, needupdate;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PROTECT))
		printf("pmap_protect(%p, %lx, %lx, %x)\n",
		    pmap, sva, eva, prot);
#endif
	if (pmap == NULL)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	p = (prot & VM_PROT_WRITE) ? mips_pg_rw_bit() : mips_pg_ro_bit();

	if (pmap == pmap_kernel()) {
		/*
		 * Change entries in kernel pmap.
		 * This will trap if the page is writeable (in order to set
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
			entry = pte->pt_entry;
			if (!mips_pg_v(entry))
				continue;
			entry &= ~(mips_pg_m_bit() | mips_pg_ro_bit());
			entry |= p;
			pte->pt_entry = entry;
#if defined(MIPS1) && !defined(MIPS3)
			{
			extern void mips1_TBRPL(vaddr_t, vaddr_t, paddr_t);
			/* replace PTE iff sva is found in TLB */
			mips1_TBRPL(sva, sva, entry);
			}
#else
			MachTLBUpdate(sva, entry);
#endif
		}
		return;
	}

#ifdef PARANOIADIAG
	if (eva > VM_MAXUSER_ADDRESS)
		panic("pmap_protect: uva not in range");
	if (PMAP_IS_ACTIVE(pmap)) {
		unsigned asid;

		__asm __volatile("mfc0 %0,$10; nop" : "=r"(asid));
		asid = (CPUISMIPS3) ? (asid & 0xff) : (asid & 0xfc0) >> 6;
		if (asid != pmap->pm_asid) {
			panic("inconsistency for active TLB update: %d <-> %d",
				asid, pmap->pm_asid);
		}
	}
#endif
	asid = pmap->pm_asid << MIPS_TLB_PID_SHIFT;
	needupdate = (pmap->pm_asidgen == pmap_asid_generation);
	while (sva < eva) {
		nssva = mips_trunc_seg(sva) + NBSEG;
		if (nssva == 0 || nssva > eva)
			nssva = eva;
		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		if (!(pte = pmap_segmap(pmap, sva))) {
			sva = nssva;
			continue;
		}
		/*
		 * Change protection on every valid mapping within this segment.
		 */
		pte += (sva >> PGSHIFT) & (NPTEPG - 1);
		for (; sva < nssva; sva += NBPG, pte++) {
			entry = pte->pt_entry;
			if (!mips_pg_v(entry))
				continue;
			entry = (entry & ~(mips_pg_m_bit() |
			    mips_pg_ro_bit())) | p;
			pte->pt_entry = entry;
			/*
			 * Update the TLB if the given address is in the cache.
			 */
			if (needupdate)
				MachTLBUpdate(sva | asid, entry);
		}
	}
}

void
pmap_procwr(p, va, len)
	struct proc	*p;
	vaddr_t		va;
	size_t		len;
{
	pmap_t pmap;

	pmap = p->p_vmspace->vm_map.pmap;

	if (CPUISMIPS3) {
#ifdef MIPS3
#if 0
		printf("pmap_procwr: va %lx len %lx\n", va, len);
#endif
		MachFlushDCache(va, len);
		MachFlushICache(MIPS_PHYS_TO_KSEG0(va &
		    (mips_L1ICacheSize - 1)), len);
#endif /* MIPS3 */
	} else {
#ifdef MIPS1
		pt_entry_t *pte;
		unsigned entry;

#if 0
printf("pmap_procwr: va %lx", va);
#endif
		if (!(pte = pmap_segmap(pmap, va)))
			return;
		pte += (va >> PGSHIFT) & (NPTEPG - 1);
		entry = pte->pt_entry;
		if (!mips_pg_v(entry))
			return;
#if 0
printf(" flush %llx", (long long)mips_tlbpfn_to_paddr(entry) + (va & PGOFSET));
#endif
		mips1_FlushICache(
		    MIPS_PHYS_TO_KSEG0(mips1_tlbpfn_to_paddr(entry)
		    + (va & PGOFSET)),
		    len);
#if 0
printf("\n");
#endif
#endif /* MIPS1 */
	}
}

/*
 *	Return RO protection of page.
 */
int
pmap_is_page_ro(pmap, va, entry)
	pmap_t pmap;
	vaddr_t	va;
	int entry;
{
	return (entry & mips_pg_ro_bit());
}

#ifdef MIPS3
/*
 *	pmap_page_cache:
 *
 *	Change all mappings of a page to cached/uncached.
 */
void
pmap_page_cache(pa, mode)
	paddr_t pa;
{
	pv_entry_t pv;
	pt_entry_t *pte;
	unsigned entry;
	unsigned newmode;
	int s;
	unsigned asid, needupdate;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_page_uncache(%lx)\n", pa);
#endif
	if (!PAGE_IS_MANAGED(pa))
		return;

	newmode = mode & PV_UNCACHED ? MIPS3_PG_UNCACHED : MIPS3_PG_CACHED;
	pv = pa_to_pvh(pa);
	asid = pv->pv_pmap->pm_asid;
	needupdate = (pv->pv_pmap->pm_asidgen == pmap_asid_generation);

	s = splimp();
	while (pv) {
		pv->pv_flags = (pv->pv_flags & ~PV_UNCACHED) | mode;
		if (pv->pv_pmap == pmap_kernel()) {
		/*
		 * Change entries in kernel pmap.
		 */
			pte = kvtopte(pv->pv_va);
			entry = pte->pt_entry;
			if (entry & MIPS3_PG_V) {
				entry = (entry & ~MIPS3_PG_CACHEMODE) | newmode;
				pte->pt_entry = entry;
				MachTLBUpdate(pv->pv_va, entry);
			}
		}
		else {

			pte = pmap_segmap(pv->pv_pmap, pv->pv_va);
			if (pte == NULL)
				continue;
			pte += (pv->pv_va >> PGSHIFT) & (NPTEPG - 1);
			entry = pte->pt_entry;
			if (entry & MIPS3_PG_V) {
				entry = (entry & ~MIPS3_PG_CACHEMODE) | newmode;
				pte->pt_entry = entry;
				if (needupdate)
					MachTLBUpdate(pv->pv_va | asid, entry);
			}
		}
		pv = pv->pv_next;
	}

	splx(s);
}
#endif

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
pmap_enter(pmap, va, pa, prot, flags)
	pmap_t pmap;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int flags;
{
	pt_entry_t *pte;
	u_int npte;
	vm_page_t mem;
	unsigned asid;
	boolean_t wired = (flags & PMAP_WIRED) != 0;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_enter(%p, %lx, %lx, %x, %x)\n",
		    pmap, va, pa, prot, wired);
#endif
#ifdef PARANOIADIAG
	if (!pmap)
		panic("pmap_enter: pmap");
#endif
#if defined(DEBUG) || defined(DIAGNOSTIC) || defined(PARANOIADIAG)
	if (pmap == pmap_kernel()) {
#ifdef DEBUG
		enter_stats.kernel++;
#endif
		if (va < VM_MIN_KERNEL_ADDRESS || va >= virtual_end)
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
	if (!(prot & VM_PROT_READ))
		panic("pmap_enter: prot");
#endif

	if (PAGE_IS_MANAGED(pa)) {
		int *attrs = pa_to_attribute(pa);

		/* Set page referenced/modified status based on flags */
		if (flags & VM_PROT_WRITE)
			*attrs |= PV_MODIFIED | PV_REFERENCED;
		else if (flags & VM_PROT_ALL)
			*attrs |= PV_REFERENCED;
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
			if ((int)va < 0) {
				/*
				 * Don't bother to trap on kernel writes,
				 * just record page as dirty.
				 */
				npte = mips_pg_rwpage_bit();
				*attrs |= PV_MODIFIED | PV_REFERENCED;
			} else {
				if (*attrs & PV_MODIFIED) {
					npte = mips_pg_rwpage_bit();
				} else {
					npte = mips_pg_cwpage_bit();
				}
			}
		}
#ifdef DEBUG
		enter_stats.managed++;
#endif
		pmap_enter_pv(pmap, va, pa, &npte);
	} else {
		/*
		 * Assumption: if it is not part of our managed memory
		 * then it must be device memory which may be volatile.
		 */
#ifdef DEBUG
		enter_stats.unmanaged++;
#endif
		if (CPUISMIPS3) {
			npte = (prot & VM_PROT_WRITE) ?
			    (MIPS3_PG_IOPAGE & ~MIPS3_PG_G) :
			    ((MIPS3_PG_IOPAGE | MIPS3_PG_RO) &
			    ~(MIPS3_PG_G | MIPS3_PG_D));
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
	if ((!CPUISMIPS3) && prot == (VM_PROT_READ | VM_PROT_EXECUTE)) {
		MachFlushICache(MIPS_PHYS_TO_KSEG0(pa), PAGE_SIZE);
	}
#endif

	if (pmap == pmap_kernel()) {
		/* enter entries into kernel pmap */
		pte = kvtopte(va);

		if (CPUISMIPS3)
			npte |= mips_paddr_to_tlbpfn(pa) | MIPS3_PG_G;
		else
			npte |= mips_paddr_to_tlbpfn(pa) | MIPS1_PG_V | MIPS1_PG_G;

		if (wired) {
			pmap->pm_stats.wired_count++;
			npte |= mips_pg_wired_bit();
		}
#ifdef PARANOIADIAG
		if (mips_pg_wired(pte->pt_entry))
			panic("pmap_enter: kernel wired");
#endif
		if (mips_tlbpfn_to_paddr(pte->pt_entry) != pa) {
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
		MachTLBUpdate(va, npte);
		return (KERN_SUCCESS);
	}

	if (!(pte = pmap_segmap(pmap, va))) {
		do {
			mem = uvm_pagealloc(NULL, 0, NULL,
			    UVM_PGA_USERESERVE|UVM_PGA_ZERO);
			if (mem == NULL) {
				/*
				 * XXX What else can we do?  Could we
				 * XXX deadlock here?
				 */
				uvm_wait("pmap_enter");
			}
		} while (mem == NULL);

		pmap_segmap(pmap, va) = pte = (pt_entry_t *)
			MIPS_PHYS_TO_KSEG0(VM_PAGE_TO_PHYS(mem));
#ifdef PARANOIADIAG
	    { int i;
		for (i = 0; i < NPTEPG; i++) {
			if ((pte+i)->pt_entry)
				panic("pmap_enter: new segmap not empty");
		}
	    }
#endif
	}
	pte += (va >> PGSHIFT) & (NPTEPG - 1);

	/*
	 * Now validate mapping with desired protection/wiring.
	 * Assume uniform modified and referenced status for all
	 * MIPS pages in a MACH page.
	 */
	if (CPUISMIPS3)
		npte |= mips_paddr_to_tlbpfn(pa);
	else
		npte |= mips_paddr_to_tlbpfn(pa) | MIPS1_PG_V;

	if (wired) {
		pmap->pm_stats.wired_count++;
		npte |= mips_pg_wired_bit();
	}
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER) {
		printf("pmap_enter: new pte %x", npte);
		if (pmap->pm_asidgen == pmap_asid_generation)
			printf(" asid %d", pmap->pm_asid);
		printf("\n");
	}
#endif

#ifdef PARANOIADIAG
	if (PMAP_IS_ACTIVE(pmap)) {
		unsigned asid;

		__asm __volatile("mfc0 %0,$10; nop" : "=r"(asid));
		asid = (CPUISMIPS3) ? (asid & 0xff) : (asid & 0xfc0) >> 6;
		if (asid != pmap->pm_asid) {
			panic("inconsistency for active TLB update: %d <-> %d",
				asid, pmap->pm_asid);
		}
	}
#endif

	asid = pmap->pm_asid << MIPS_TLB_PID_SHIFT;
	if (mips_tlbpfn_to_paddr(pte->pt_entry) != pa) {
		pmap_remove(pmap, va, va + NBPG);
#ifdef DEBUG
		enter_stats.mchange++;
#endif
	}

	if (!mips_pg_v(pte->pt_entry))
		pmap->pm_stats.resident_count++;
	pte->pt_entry = npte;

	if (pmap->pm_asidgen == pmap_asid_generation)
		MachTLBUpdate(va | asid, npte);

#ifdef MIPS3
	if (CPUISMIPS3 && (prot == (VM_PROT_READ | VM_PROT_EXECUTE))) {
#ifdef DEBUG
		if (pmapdebug & PDB_ENTER)
			printf("pmap_enter: flush I cache va %lx (%lx)\n",
			    va - NBPG, pa);
#endif
		MachFlushICache(va, PAGE_SIZE);
	}
#endif

	return (KERN_SUCCESS);
}

void
pmap_kenter_pa(va, pa, prot)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
{
	pmap_enter(pmap_kernel(), va, pa, prot, PMAP_WIRED);
}

void
pmap_kenter_pgs(va, pgs, npgs)
	vaddr_t va;
	struct vm_page **pgs;
	int npgs;
{
	int i;

	for (i = 0; i < npgs; i++, va += PAGE_SIZE) {
		pmap_enter(pmap_kernel(), va, VM_PAGE_TO_PHYS(pgs[i]),
				VM_PROT_READ|VM_PROT_WRITE, PMAP_WIRED);
	}
}

void
pmap_kremove(va, len)
	vaddr_t va;
	vsize_t len;
{
	for (len >>= PAGE_SHIFT; len > 0; len--, va += PAGE_SIZE) {
		pmap_remove(pmap_kernel(), va, va + PAGE_SIZE);
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
pmap_unwire(pmap, va)
	pmap_t pmap;
	vaddr_t va;
{
	pt_entry_t *pte;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_WIRING))
		printf("pmap_unwire(%p, %lx)\n", pmap, va);
#endif
	if (pmap == NULL)
		return;

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
		pte = pmap_segmap(pmap, va);
#ifdef DIAGNOSTIC
		if (pte == NULL)
			panic("pmap_unwire: pmap %p va 0x%lx invalid STE",
			    pmap, va);
#endif
		pte += (va >> PGSHIFT) & (NPTEPG - 1);
	}

#ifdef DIAGNOSTIC
	if (mips_pg_v(pte->pt_entry) == 0)
		panic("pmap_unwire: pmap %p va 0x%lx invalid PTE",
		    pmap, va);
#endif

	if (mips_pg_wired(pte->pt_entry)) {
		pte->pt_entry &= ~mips_pg_wired_bit();
		pmap->pm_stats.wired_count--;
	}
#ifdef DIAGNOSTIC
	else {
		printf("pmap_unwire: wiring for pmap %p va 0x%lx "
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
boolean_t
pmap_extract(pmap, va, pap)
	pmap_t pmap;
	vaddr_t va;
	paddr_t *pap;
{
	paddr_t pa;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_extract(%p, %lx) -> ", pmap, va);
#endif

	if (pmap == pmap_kernel()) {
#ifdef PARANOIADIAG
		if (va < VM_MIN_KERNEL_ADDRESS || va >= virtual_end)
			panic("pmap_extract");
#endif
		pa = mips_tlbpfn_to_paddr(kvtopte(va)->pt_entry);
	} else {
		pt_entry_t *pte;

		if (!(pte = pmap_segmap(pmap, va)))
			return (FALSE);
		else {
			pte += (va >> PGSHIFT) & (NPTEPG - 1);
			pa = mips_tlbpfn_to_paddr(pte->pt_entry);
		}
	}
	pa |= va & PGOFSET;
	if (pap != NULL)
		*pap = pa;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_extract: pa %lx\n", pa);
#endif
	return (TRUE);
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t dst_pmap;
	pmap_t src_pmap;
	vaddr_t dst_addr;
	vsize_t len;
	vaddr_t src_addr;
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy(%p, %p, %lx, %lx, %lx)\n",
		     dst_pmap, src_pmap, dst_addr, len, src_addr);
#endif
}

/*
 *	Require that all active physical maps contain no
 *	incorrect entries NOW.  [This update includes
 *	forcing updates of any address map caching.]
 *
 *	Generally used to insure that a thread about
 *	to run will see a semantically correct world.
 */
void
pmap_update()
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_update()\n");
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
pmap_collect(pmap)
	pmap_t pmap;
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
pmap_zero_page(phys)
	paddr_t phys;
{
	int *p, *end;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_zero_page(%lx)\n", phys);
#endif
#ifdef PARANOIADIAG
	if (! (phys < MIPS_MAX_MEM_ADDR))
		printf("pmap_zero_page(%lx) nonphys\n", phys);
#endif

	p = (int *)MIPS_PHYS_TO_KSEG0(phys);
	end = p + PAGE_SIZE / sizeof(int);
	/* XXX blkclr()? */
	do {
		p[0] = 0;
		p[1] = 0;
		p[2] = 0;
		p[3] = 0;

		p[4] = 0;
		p[5] = 0;
		p[6] = 0;
		p[7] = 0;

		p[8] = 0;
		p[9] = 0;
		p[10] = 0;
		p[11] = 0;

		p[12] = 0;
		p[13] = 0;
		p[14] = 0;
		p[15] = 0;
		p += 16;
	} while (p != end);
#if defined(MIPS3) && defined(MIPS3_L2CACHE_ABSENT)
	/*
	 * If we have a virtually-indexed, physically-tagged WB cache,
	 * and no L2 cache to warn of aliased mappings,	we must force a
	 * writeback of the destination out of the L1 cache.  If we don't,
	 * later reads (from virtual addresses mapped to the destination PA)
	 * might read old stale DRAM footprint, not the just-written data.
	 */
	if (CPUISMIPS3 && !mips_L2CachePresent) {
		/*XXX FIXME Not very sophisticated */
		/*	MachFlushCache();*/
		MachFlushDCache(MIPS_PHYS_TO_KSEG0(phys), NBPG);
	}
#endif
}

/*
 *	pmap_zero_page_uncached zeros the specified page
 *	using uncached accesses.
 */
void
pmap_zero_page_uncached(phys)
	paddr_t phys;
{
	int *p, *end;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_zero_page_uncached(%lx)\n", phys);
#endif
#ifdef PARANOIADIAG
	if (! (phys < MIPS_MAX_MEM_ADDR))
		printf("pmap_zero_page_uncached(%lx) nonphys\n", phys);
#endif

	p = (int *)MIPS_PHYS_TO_KSEG1(phys);
	end = p + PAGE_SIZE / sizeof(int);
	/* XXX blkclr()? */
	do {
		p[0] = 0;
		p[1] = 0;
		p[2] = 0;
		p[3] = 0;

		p[4] = 0;
		p[5] = 0;
		p[6] = 0;
		p[7] = 0;

		p[8] = 0;
		p[9] = 0;
		p[10] = 0;
		p[11] = 0;

		p[12] = 0;
		p[13] = 0;
		p[14] = 0;
		p[15] = 0;
		p += 16;
	} while (p != end);
}

/*
 *	pmap_copy_page copies the specified page.
 */
void
pmap_copy_page(src, dst)
	paddr_t src, dst;
{
	int *s, *d, *end;
	int tmp0, tmp1, tmp2, tmp3;
	int tmp4, tmp5, tmp6, tmp7;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy_page(%lx, %lx)\n", src, dst);
#endif
#ifdef PARANOIADIAG
	if (! (src < MIPS_MAX_MEM_ADDR))
		printf("pmap_copy_page(%lx) src nonphys\n", src);
	if (! (dst < MIPS_MAX_MEM_ADDR))
		printf("pmap_copy_page(%lx) dst nonphys\n", dst);
#endif

#if defined(MIPS3) && defined(MIPS3_L2CACHE_ABSENT)
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
	if (CPUISMIPS3 && !mips_L2CachePresent) {
		/*XXX FIXME Not very sophisticated */
		mips_flushcache_allpvh(src);
/*		mips_flushcache_allpvh(dst); */
	}
#endif
	s = (int *)MIPS_PHYS_TO_KSEG0(src);
	d = (int *)MIPS_PHYS_TO_KSEG0(dst);
	end = s + PAGE_SIZE / sizeof(int);
	do {
		tmp0 = s[0];
		tmp1 = s[1];
		tmp2 = s[2];
		tmp3 = s[3];
		d[0] = tmp0;
		d[1] = tmp1;
		d[2] = tmp2;
		d[3] = tmp3;

		tmp4 = s[4];
		tmp5 = s[5];
		tmp6 = s[6];
		tmp7 = s[7];
		d[4] = tmp4;
		d[5] = tmp5;
		d[6] = tmp6;
		d[7] = tmp7;

		tmp0 = s[8];
		tmp1 = s[9];
		tmp2 = s[10];
		tmp3 = s[11];
		d[8] = tmp0;
		d[9] = tmp1;
		d[10] = tmp2;
		d[11] = tmp3;

		tmp4 = s[12];
		tmp5 = s[13];
		tmp6 = s[14];
		tmp7 = s[15];
		d[12] = tmp4;
		d[13] = tmp5;
		d[14] = tmp6;
		d[15] = tmp7;

		s += 16;
		d += 16;
	} while (s != end);
#if defined(MIPS3) && defined(MIPS3_L2CACHE_ABSENT)
	/*
	 * If we have a virtually-indexed, physically-tagged WB cache,
	 * and no L2 cache to warn of aliased mappings,	we must force a
	 * writeback of the destination out of the L1 cache.  If we don't,
	 * later reads (from virtual addresses mapped to the destination PA)
	 * might read old stale DRAM footprint, not the just-written data.
	 * XXX  Do we need to also invalidate any cache lines matching
	 *      the destination as well?
	 */
	if (CPUISMIPS3) {
		/*XXX FIXME Not very sophisticated */
		/*	MachFlushCache();*/
		MachFlushDCache(dst, NBPG);
	}
#endif
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
boolean_t
pmap_clear_reference(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	boolean_t rv;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_reference(%lx)\n", pa);
#endif
	rv = FALSE;
	if (PAGE_IS_MANAGED(pa)) {
		rv = *pa_to_attribute(pa) & PV_REFERENCED;
		*pa_to_attribute(pa) &= ~PV_REFERENCED;
	}
	return rv;
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */
boolean_t
pmap_is_referenced(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);

	if (PAGE_IS_MANAGED(pa))
		return (*pa_to_attribute(pa) & PV_REFERENCED);
#ifdef DEBUG
	else
		printf("pmap_is_referenced: pa %lx\n", pa);
#endif
	return (FALSE);
}

/*
 *	Clear the modify bits on the specified physical page.
 */
boolean_t
pmap_clear_modify(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	boolean_t rv;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_clear_modify(%lx)\n", pa);
#endif
	rv = FALSE;
	if (PAGE_IS_MANAGED(pa)) {
		rv = *pa_to_attribute(pa) & PV_MODIFIED;
		*pa_to_attribute(pa) &= ~PV_MODIFIED;
	}
	return rv;
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */
boolean_t
pmap_is_modified(pg)
	struct vm_page *pg;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);

	if (PAGE_IS_MANAGED(pa))
		return (*pa_to_attribute(pa) & PV_MODIFIED);
#ifdef DEBUG
	else
		printf("pmap_is_modified: pa %lx\n", pa);
#endif
	return (FALSE);
}

/*
 *	pmap_set_modified:
 *
 *	Sets the page modified reference bit for the specified page.
 */
void
pmap_set_modified(pa)
	paddr_t pa;
{
	if (PAGE_IS_MANAGED(pa))
		*pa_to_attribute(pa) |= PV_MODIFIED | PV_REFERENCED;
#ifdef DEBUG
	else
		printf("pmap_set_modified: pa %lx\n", pa);
#endif
}

paddr_t
pmap_phys_address(ppn)
	int ppn;
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_phys_address(%x)\n", ppn);
#endif
	return (mips_ptob(ppn));
}

/*
 * Miscellaneous support routines
 */

/*
 * Allocate TLB address space tag (called ASID or TLBPID) and return it.
 * It takes almost as much or more time to search the TLB for a
 * specific ASID and flush those entries as it does to flush the entire TLB.
 * Therefore, when we allocate a new ASID, we just take the next number. When
 * we run out of numbers, we flush the TLB, increment the generation count
 * and start over. ASID zero is reserved for kernel use.
 */
void
pmap_asid_alloc(pmap)
	pmap_t pmap;
{
	if (pmap->pm_asid != PMAP_ASID_RESERVED &&
	    pmap->pm_asidgen == pmap_asid_generation)
		;
	else {
		if (pmap_next_asid == pmap_max_asid) {
			MIPS_TBIAP();
			pmap_asid_generation++; /* ok to wrap to 0 */
			pmap_next_asid = 1;	/* 0 means invalid */
		}
		pmap->pm_asid = pmap_next_asid++;
		pmap->pm_asidgen = pmap_asid_generation;
	}

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_TLBPID)) {
		if (curproc)
			printf("pmap_asid_alloc: curproc %d '%s' ",
				curproc->p_pid, curproc->p_comm);
		else
			printf("pmap_asid_alloc: curproc <none> ");
		printf("segtab %p asid %d\n", pmap->pm_segtab, pmap->pm_asid);
	}
#endif
}

/*
 * Enter the pmap and virtual address into the
 * physical to virtual map table.
 */
void
pmap_enter_pv(pmap, va, pa, npte)
	pmap_t pmap;
	vaddr_t va;
	paddr_t pa;
	u_int *npte;
{
	pv_entry_t pv, npv;
	int s;

	pv = pa_to_pvh(pa);
	s = splimp();
#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter: pv %p: was %lx/%p/%p\n",
		    pv, pv->pv_va, pv->pv_pmap, pv->pv_next);
#endif
	if (pv->pv_pmap == NULL) {
		/*
		 * No entries yet, use header as the first entry
		 */
#ifdef DEBUG
		if (pmapdebug & PDB_PVENTRY)
			printf("pmap_enter: first pv: pmap %p va %lx\n",
				pmap, va);
		enter_stats.firstpv++;
#endif
		pv->pv_va = va;
		pv->pv_flags &= ~PV_UNCACHED;
		pv->pv_pmap = pmap;
		pv->pv_next = NULL;
	} else {
#if defined(MIPS3) && defined(MIPS3_L2CACHE_ABSENT)
		if (CPUISMIPS3 && !mips_L2CachePresent) {
			if (!(pv->pv_flags & PV_UNCACHED)) {
			/*
			 * There is at least one other VA mapping this page.
			 * Check if they are cache index compatible. If not
			 * remove all mappings, flush the cache and set page
			 * to be mapped uncached. Caching will be restored
			 * when pages are mapped compatible again.
			 * XXX - caching is not currently being restored, but
			 * XXX - I haven't seen the pages uncached since
			 * XXX - using pmap_prefer().	mhitch
			 */
				for (npv = pv; npv; npv = npv->pv_next) {
					/*
					 * Check cache aliasing incompatibility
					 */
					if ((npv->pv_va & mips_CacheAliasMask)
					    != (va & mips_CacheAliasMask)) {
						pmap_page_cache(pa,PV_UNCACHED);
						MachFlushDCache(pv->pv_va, PAGE_SIZE);
						*npte = (*npte & ~MIPS3_PG_CACHEMODE) | MIPS3_PG_UNCACHED;
#ifdef DEBUG
						enter_stats.ci++;
#endif
						break;
					}
				}
			}
			else {
				*npte = (*npte & ~MIPS3_PG_CACHEMODE) | MIPS3_PG_UNCACHED;
			}
		}
#endif
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
					pte = pmap_segmap(pmap, va);
					if (pte) {
						pte += (va >> PGSHIFT) &
						    (NPTEPG - 1);
						entry = pte->pt_entry;
					} else
						entry = 0;
				}
				if (!mips_pg_v(entry) ||
				    mips_tlbpfn_to_paddr(entry) != pa)
					printf(
		"pmap_enter: found va %lx pa %lx in pv_table but != %x\n",
						va, pa, entry);
#endif
				goto fnd;
			}
		}
#ifdef DEBUG
		if (pmapdebug & PDB_PVENTRY)
			printf("pmap_enter: new pv: pmap %p va %lx\n",
				pmap, va);
#endif
		/* can this cause us to recurse forever? */
		npv = (pv_entry_t)
			malloc(sizeof *npv, M_VMPVENT, M_NOWAIT);
		if (npv == NULL)
			panic("pmap_enter: new pv malloc() failed");
		npv->pv_va = va;
		npv->pv_pmap = pmap;
		npv->pv_flags = pv->pv_flags;
		npv->pv_next = pv->pv_next;
		pv->pv_next = npv;
#ifdef DEBUG
		if (!npv->pv_next)
			enter_stats.secondpv++;
#endif
	fnd:
		;
	}
	splx(s);
}

/*
 * Remove a physical to virtual address translation.
 * If cache was inhibited on this page, and there are no more cache
 * conflicts, restore caching.
 * Flush the cache if the last page is removed (should always be cached
 * at this point).
 */
void
pmap_remove_pv(pmap, va, pa)
	pmap_t pmap;
	vaddr_t va;
	paddr_t pa;
{
	pv_entry_t pv, npv;
	int s, last;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PVENTRY))
		printf("pmap_remove_pv(%p, %lx, %lx)\n", pmap, va, pa);
#endif
	/*
	 * Remove page from the PV table (raise IPL since we
	 * may be called at interrupt time).
	 */
	if (!PAGE_IS_MANAGED(pa))
		return;
	pv = pa_to_pvh(pa);
	s = splimp();
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
			free((caddr_t)npv, M_VMPVENT);
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
			free((caddr_t)npv, M_VMPVENT);
		}
	}
	splx(s);
#ifdef MIPS1
	if (CPUISMIPS3 == 0 && last != 0) {
		MachFlushDCache(MIPS_PHYS_TO_KSEG0(pa), PAGE_SIZE);
	}
#endif
#ifdef MIPS3
	if (CPUISMIPS3 && pv->pv_flags & PV_UNCACHED) {
		/*
		 * Page is currently uncached, check if alias mapping has been
		 * removed.  If it was, then reenable caching.
		 */
		pv = pa_to_pvh(pa);
		for (npv = pv->pv_next; npv; npv = npv->pv_next) {
			if ((pv->pv_va ^ npv->pv_va) & mips_CacheAliasMask)
				break;
		}
		if (npv == NULL)
			pmap_page_cache(pa, 0);
	}
	if (CPUISMIPS3 && last != 0) {
		MachFlushDCache(va, PAGE_SIZE);
		if (mips_L2CachePresent)
			/*
			 * mips3_MachFlushDCache() converts the address to a
			 * KSEG0 address, and won't properly flush the Level 2
			 * cache.  Do another flush using the physical adddress
			 * to make sure the proper secondary cache lines are
			 * flushed.  Ugh!
			 */
			MachFlushDCache(pa, PAGE_SIZE);
	}
#endif
	return;
}

pt_entry_t *
pmap_pte(pmap, va)
	pmap_t pmap;
	vaddr_t va;
{
	pt_entry_t *pte = NULL;

	if (pmap == pmap_kernel())
		pte = kvtopte(va);
	else if ((pte = pmap_segmap(pmap, va)) != NULL)
		pte += (va >> PGSHIFT) & (NPTEPG - 1);
	return (pte);
}

#ifdef MIPS3
/*
 * Find first virtual address >= *vap that doesn't cause
 * a cache alias conflict.
 */
void
pmap_prefer(foff, vap)
	vaddr_t foff;
	vaddr_t *vap;
{
	vaddr_t	va = *vap;
	vsize_t d;

	if (CPUISMIPS3) {
		d = foff - va;
		/* Use 64K to prevent virtual coherency exceptions */
		d &= (0x10000 - 1);
		*vap = va + d;
	}
}
#endif
