/*	$NetBSD: pmap.c,v 1.11.8.4 1997/10/15 05:27:00 thorpej Exp $	*/

/*
 * Copyright (c) 1994-1997 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * pmap.c
 *
 * Machine dependant vm stuff
 *
 * Created      : 20/09/94
 */

/*
 * XXX XXX XXX
 *
 * There are one or two ultra gross hacks in the file. This fix some
 * nightmare problems with double mappings.
 * This file is being rewritten from the ground up and these problems
 * will disappear then. In the mean this this GROSS hack can be used.
 * Due to the serious performance hit with the SA110 and this problem
 * a temporary fix is needed prior to the complete pmap rewrite.
 */

/*
 * The dram block info is currently referenced from the bootconfig.
 * This should be placed in a separate structure
 *
 * sob sob sob - been looking at other pmap code ... don't think
 * I should have base mine of the i386 code ...
 */

/*
 * Special compilation symbols
 * PMAP_DEBUG		- Build in pmap_debug_level code
 */
    
/* Include header files */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/bootconfig.h>
#include <machine/pmap.h>
#include <machine/pcb.h>
#include <machine/param.h>
#include <machine/katelib.h>
       
#ifdef HYDRA
#include "hydrabus.h"
#endif	/* HYDRA */

/*#define bcopy_page(s, d) bcopy(s, d, NBPG)*/
/*#define bzero_page(s)    bzero(s, NBPG)*/
#define bzero_pagedir(s) bzero(s, PD_SIZE)

#define VM_KERNEL_VIRTUAL_MIN     KERNEL_VM_BASE + 0x00000000
#define VM_KERNEL_VIRTUAL_MAX     KERNEL_VM_BASE + 0x01ffffff

#define pmap_pde(m, v) (&((m)->pm_pdir[((vm_offset_t)(v) >> PDSHIFT)&4095]))

#define pmap_pte_pa(pte)	(*(pte) & PG_FRAME)
#define pmap_pde_v(pde)		(*(pde) != 0)
#define pmap_pte_v(pte)		(*(pte) != 0)

#ifdef PMAP_DEBUG
int pmap_debug_level = -2;
#endif	/* PMAP_DEBUG */

struct pmap     kernel_pmap_store;
pmap_t          kernel_pmap;

pagehook_t page_hook0;
pagehook_t page_hook1;
char *memhook;
pt_entry_t msgbufpte;
extern caddr_t msgbufaddr;

u_char *pmap_attributes = NULL;
pv_entry_t pv_table = NULL;
TAILQ_HEAD(pv_page_list, pv_page) pv_page_freelist;
int pv_nfree = 0;

vm_size_t npages;

extern vm_offset_t physical_start;
extern vm_offset_t physical_freestart;
extern vm_offset_t physical_end;
extern vm_offset_t physical_freeend;
extern int physical_memoryblock;
extern unsigned int free_pages;
extern int max_processes;

vm_offset_t virtual_start;
vm_offset_t virtual_end;

vm_offset_t avail_start;
vm_offset_t avail_end;

typedef struct {
	vm_offset_t physical;
	vm_offset_t virtual;
} pv_addr_t;

#if NHYDRABUS > 0
extern pv_addr_t hydrascratch;
#endif	/* NHYDRABUS */

#define ALLOC_PAGE_HOOK(x, s) \
	x.va = virtual_start; \
	x.pte = (pt_entry_t *)pmap_pte(kernel_pmap, virtual_start); \
	virtual_start += s; 

/* Variables used by the L1 page table queue code */
SIMPLEQ_HEAD(l1pt_queue, l1pt);
struct l1pt_queue l1pt_static_queue;	/* head of our static l1 queue */
int l1pt_static_queue_count;		/* items in the static l1 queue */
int l1pt_static_create_count;		/* static l1 items created */
struct l1pt_queue l1pt_queue;	/* head of our l1 queue */
int l1pt_queue_count;		/* items in the l1 queue */
int l1pt_create_count;		/* stat - L1's create count */
int l1pt_reuse_count;		/* stat - L1's reused count */

/* Local function prototypes (not used outside this file) */

pt_entry_t *pmap_pte __P((pmap_t pmap, vm_offset_t va));
int pmap_page_index __P((vm_offset_t pa)); 
void map_pagetable __P((vm_offset_t pagetable, vm_offset_t va,
    vm_offset_t pa, unsigned int flags));
void pmap_copy_on_write __P((vm_offset_t pa));

void bzero_page __P((vm_offset_t));
void bcopy_page __P((vm_offset_t, vm_offset_t));

struct l1pt *pmap_alloc_l1pt __P((void));

/* Function to set the debug level of the pmap code */

#ifdef PMAP_DEBUG
void
pmap_debug(level)
	int level;
{
	pmap_debug_level = level;
	printf("pmap_debug: level=%d\n", pmap_debug_level);
}
#endif	/* PMAP_DEBUG */


/*
 * Functions for manipluation pv_entry structures. These are used to keep a
 * record of the mappings of virtual addresses and the associated physical
 * pages.
 */

struct pv_entry *
pmap_alloc_pv()
{
	struct pv_page *pvp;
	struct pv_entry *pv;
	int i;

	/*
	 * Do we have any free pv_entry structures left ?
	 * If not allocate a page of them
	 */
  
	if (pv_nfree == 0) {
		pvp = (struct pv_page *)kmem_alloc(kernel_map, NBPG);
		if (pvp == 0)
			panic("pmap_alloc_pv: kmem_alloc() failed");
		pvp->pvp_pgi.pgi_freelist = pv = &pvp->pvp_pv[1];
		for (i = NPVPPG - 2; i; i--, pv++)
			pv->pv_next = pv + 1;
		pv->pv_next = 0;
		pv_nfree += pvp->pvp_pgi.pgi_nfree = NPVPPG - 1;
		TAILQ_INSERT_HEAD(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		pv = &pvp->pvp_pv[0];
	} else {
		--pv_nfree;
		pvp = pv_page_freelist.tqh_first;
		if (--pvp->pvp_pgi.pgi_nfree == 0) {
			TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		}
		pv = pvp->pvp_pgi.pgi_freelist;
#ifdef DIAGNOSTIC
		if (pv == 0)
			panic("pmap_alloc_pv: pgi_nfree inconsistent");
#endif	/* DIAGNOSTIC */
		pvp->pvp_pgi.pgi_freelist = pv->pv_next;
	}
	return pv;
}

void
pmap_free_pv(pv)
	struct pv_entry *pv;
{
	register struct pv_page *pvp;

	pvp = (struct pv_page *) trunc_page(pv);
	switch (++pvp->pvp_pgi.pgi_nfree) {
	case 1:
		TAILQ_INSERT_TAIL(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
	default:
		pv->pv_next = pvp->pvp_pgi.pgi_freelist;
		pvp->pvp_pgi.pgi_freelist = pv;
		++pv_nfree;
		break;
	case NPVPPG:
		pv_nfree -= NPVPPG - 1;
		TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		kmem_free(kernel_map, (vm_offset_t)pvp, NBPG);
		break;
	}
}


void
pmap_collect_pv()
{
	struct pv_page_list pv_page_collectlist;
	struct pv_page *pvp, *npvp;
	struct pv_entry *ph, *ppv, *pv, *npv;
	int s;

	TAILQ_INIT(&pv_page_collectlist);

	for (pvp = pv_page_freelist.tqh_first; pvp; pvp = npvp) {
		if (pv_nfree < NPVPPG)
			break;
		npvp = pvp->pvp_pgi.pgi_list.tqe_next;
		if (pvp->pvp_pgi.pgi_nfree > NPVPPG / 3) {
			TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
			TAILQ_INSERT_TAIL(&pv_page_collectlist, pvp, pvp_pgi.pgi_list);
			pv_nfree -= pvp->pvp_pgi.pgi_nfree;
			pvp->pvp_pgi.pgi_nfree = -1;
		}
	}

	if (pv_page_collectlist.tqh_first == 0)
		return;

	for (ph = &pv_table[npages - 1]; ph >= &pv_table[0]; ph--) {
		if (ph->pv_pmap == 0)
			continue;
		s = splimp();
		for (ppv = ph; (pv = ppv->pv_next) != 0; ) {
			pvp = (struct pv_page *) trunc_page(pv);
			if (pvp->pvp_pgi.pgi_nfree == -1) {
				pvp = pv_page_freelist.tqh_first;
				if (--pvp->pvp_pgi.pgi_nfree == 0) {
					TAILQ_REMOVE(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
				}
				npv = pvp->pvp_pgi.pgi_freelist;
#ifdef DIAGNOSTIC
				if (npv == 0)
					panic("pmap_collect_pv: pgi_nfree inconsistent");
#endif	/* DIAGNOSTIC */
				pvp->pvp_pgi.pgi_freelist = npv->pv_next;
				*npv = *pv;
				ppv->pv_next = npv;
				ppv = npv;
			} else
				ppv = pv;
		}
		(void)splx(s);
	}

	for (pvp = pv_page_collectlist.tqh_first; pvp; pvp = npvp) {
		npvp = pvp->pvp_pgi.pgi_list.tqe_next;
		kmem_free(kernel_map, (vm_offset_t)pvp, NBPG);
	}
}


/*__inline*/ int
pmap_enter_pv(pmap, va, pind, flags)
	pmap_t pmap;
	vm_offset_t va;
	int pind;
	u_int flags;
{
	register struct pv_entry *pv, *npv;
	u_int s;

	if (!pv_table)
		return(1);

#ifdef DIAGNOSTIC
	if (pind < 0)
		panic("pmap_enter_pv: pind < 0\n");
#endif	/* DIAGNOSTIC */

	s = splimp();
	pv = &pv_table[pind];

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 5)
		printf("pmap_enter_pv: pind=%04x pv %p: %08x/%p/%p\n",
		    pind, pv, (u_int) pv->pv_va, pv->pv_pmap, pv->pv_next);
#endif	/* PMAP_DEBUG */

	if (pv->pv_pmap == NULL) {
		/*
		 * No entries yet, use header as the first entry
		 */
		pv->pv_va = va;
		pv->pv_pmap = pmap;
		pv->pv_next = NULL;
		pv->pv_flags = flags;
		(void)splx(s);
		return(1);
	} else {
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
		for (npv = pv; npv; npv = npv->pv_next)
			if (pmap == npv->pv_pmap && va == npv->pv_va) {
				panic("pmap_enter_pv: already in pv_tab  pind=%04x pv %p: %08x/%p/%p",
				    pind, pv, (u_int) pv->pv_va, pv->pv_pmap, pv->pv_next);
#if 0
				/*
				 * HACK HACK HACK - The system vector page is
				 * wired in with pmap_enter() this mapping
				 * should be deleted during switch_exit()
				 * but at the moment it is not so a duplicate
				 * mapping is possible. For the moment we just
				 * updated flags and exit ...
				 * This should work as it is a special case.
				 *
				 * XXX - I think this problem has been
				 * fixed now - mark
				 */
				npv->pv_va = va;
				npv->pv_pmap = pmap;
				npv->pv_flags = flags;
				printf("pmap_enter_pv: already in pv_tab  pind=%04x pv %p: %p/%p/%p",
				    pind, pv, (u_int) pv->pv_va, pv->pv_pmap, pv->pv_next);
				(void)splx(s);
				return(0);
#endif
			}
		npv = pmap_alloc_pv();
		npv->pv_va = va;
		npv->pv_pmap = pmap;
		npv->pv_flags = flags;
		npv->pv_next = pv->pv_next;
		pv->pv_next = npv;
	}
	(void)splx(s);
	return(0);
}


/* __inline*/ u_int
pmap_remove_pv(pmap, va, pind)
	pmap_t pmap;
	vm_offset_t va;
	int pind;
{
	register struct pv_entry *pv, *npv;
	u_int s;
	u_int flags = 0;
    
	if (pv_table == NULL)
		return(0);

#ifdef DIAGNOSTIC
	if (pind < 0)
		panic("pmap_enter_pv: pind < 0\n");
#endif	/* DIAGNOSTIC */

	/*
	 * Remove from the PV table (raise IPL since we
	 * may be called at interrupt time).
	 */

	s = splimp();
	pv = &pv_table[pind];

	/*
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry.  In either case we free the now unused entry.
	 */

	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;

			flags = npv->pv_flags;

			pmap_attributes[pind] |= flags & (PT_M | PT_H);

			pmap_free_pv(npv);
		} else
			pv->pv_pmap = NULL;
	} else {
		for (npv = pv->pv_next; npv; pv = npv, npv = npv->pv_next) {
			if (pmap == npv->pv_pmap && va == npv->pv_va)
				break;
		}
		if (npv) {
			pv->pv_next = npv->pv_next;

			flags = npv->pv_flags;
			pmap_attributes[pind] |= flags & (PT_M | PT_H);

			pmap_free_pv(npv);
		}
	}
	(void)splx(s);
	return(flags);
}


/*__inline */ u_int
pmap_modify_pv(pmap, va, pind, bic_mask, eor_mask)
	pmap_t pmap;
	vm_offset_t va;
	int pind;
	u_int bic_mask;
	u_int eor_mask;
{
	register struct pv_entry *pv, *npv;
	u_int s;
	u_int flags;

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 5)
		printf("pmap_modify_pv(pmap=%p, va=%08x, pi=%04x, bic_mask=%08x, eor_mask=%08x)\n",
		    pmap, (u_int) va, pind, bic_mask, eor_mask);
#endif	/* PMAP_DEBUG */

	if (!pv_table)
		return(0);

#ifdef DIAGNOSTIC
	if (pind < 0)
		panic("pmap_modify_pv: pind out of range pind = %08x va=%08x\n", pind, (u_int)va);
#endif	/* DIAGNOSTIC */

	s = splimp();
	pv = &pv_table[pind];

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 5)
		printf("pmap_modify_pv: pind=%04x pv %p: %08x/%p/%p/%08x ",
		    pind, pv, (u_int)pv->pv_va, pv->pv_pmap, pv->pv_next,
		    pv->pv_flags);
#endif	/* PMAP_DEBUG */

	/*
	 * There is at least one VA mapping this page.
	 */

	for (npv = pv; npv; npv = npv->pv_next) {
		if (pmap == npv->pv_pmap && va == npv->pv_va) {
			flags = npv->pv_flags;
			npv->pv_flags = ((flags & ~bic_mask) ^ eor_mask);
#ifdef PMAP_DEBUG
			if (pmap_debug_level >= 0)
				printf("done flags=%08x\n", flags);
#endif	/* PMAP_DEBUG */
			(void)splx(s);
			return(flags);
		}
	}

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("done.\n");
#endif	/* PMAP_DEBUG */
	(void)splx(s);
	return(0);
}


/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 */

vm_offset_t
pmap_map(va, spa, epa, prot)
	vm_offset_t va, spa, epa;
	int prot;
{
	while (spa < epa) {
		pmap_enter(pmap_kernel(), va, spa, prot, FALSE);
		va += NBPG;
		spa += NBPG;
	}
	return(va);
}


/*
 * void pmap_bootstrap(pd_entry_t *kernel_l1pt)
 *
 * bootstrap the pmap system. This is called from initarm and allows
 * the pmap system to initailise any structures it requires.
 *
 * Currently this sets up the kernel_pmap that is statically allocated
 * and also allocated virtual addresses for certain page hooks.
 * Currently the only one page hook is allocated that is used
 * to zero physical pages of memory.
 * It also initialises the start and end address of the kernel data space.                 
 */

void
pmap_bootstrap(kernel_l1pt, kernel_ptpt)
	pd_entry_t *kernel_l1pt;
	pt_entry_t kernel_ptpt;
{
	kernel_pmap = &kernel_pmap_store;

	kernel_pmap->pm_pdir = kernel_l1pt;
	kernel_pmap->pm_pptpt = kernel_ptpt;
	simple_lock_init(&kernel_pmap->pm_lock);
	kernel_pmap->pm_count = 1;

	virtual_start = VM_KERNEL_VIRTUAL_MIN;
	virtual_end = VM_KERNEL_VIRTUAL_MAX;

	ALLOC_PAGE_HOOK(page_hook0, NBPG);
	ALLOC_PAGE_HOOK(page_hook1, NBPG);

	/*
	 * The mem special device needs a virtual hook but we don't
	 * need a pte
	 */

	memhook = (char *)virtual_start;
	virtual_start += NBPG;

	msgbufaddr = (caddr_t)virtual_start;
	msgbufpte = (pt_entry_t)pmap_pte(kernel_pmap, virtual_start);
	virtual_start += round_page(MSGBUFSIZE);

/*	printf("pmap_bootstrap: page_hook = V%08x pte = V%08x\n",
	    page_hook_addr0, page_hook_pte0); */

#if NHYDRABUS > 0
	hydrascratch.virtual = virtual_start;
	virtual_start += NBPG;

	*((pt_entry_t *)pmap_pte(kernel_pmap, hydrascratch.virtual)) = L2_PTE_NC_NB(hydrascratch.physical, AP_KRW);
	cpu_tlb_flushD_SE(hydrascratch.virtual);
#endif	/* NHYDRABUS */
	cpu_cache_cleanD();
}


/*
 * void pmap_init(void)
 *
 * Initialize the pmap module.
 * Called by vm_init() in vm/vm_init.c in order to initialise
 * any structures that the pmap system needs to map virtual memory.
 */

extern int physmem;

void
pmap_init()
{
	vm_size_t s;
	vm_offset_t addr;
    
/*	printf("pmap_init:\n");*/

	npages = physmem;
	printf("Number of pages to handle = %ld\n", npages);

	/*
	 * Set the available memory vars - These do not map to real memory
	 * addresses and cannot as the physical memory is fragmented.
	 * They are used by ps for %mem calculations.
	 * One could argue whether this should be the entire memory or just
	 * the memory that is useable in a user process.
	 */
 
/*	avail_start = pmap_page_index(physical_start) * NBPG;
	avail_end = pmap_page_index(physical_freeend) * NBPG;*/
	avail_start = 0;
	avail_end = physmem * NBPG;
    
	s = (vm_size_t) (sizeof(struct pv_entry) * npages + npages);
	s = round_page(s);
	addr = (vm_offset_t) kmem_alloc(kernel_map, s);
#ifdef DIAGNOSTIC	
	if (addr == 0)
		panic("pmap_init: Cannot allocate memory for pv_table\n");
#endif	/* DIAGNOSTIC */
	pv_table = (pv_entry_t) addr;
	addr += sizeof(struct pv_entry) * npages;
	pmap_attributes = (char *) addr;
	bzero(pv_table, s);

	/* Initialise our L1 page table queues and counters */
	SIMPLEQ_INIT(&l1pt_static_queue);
	l1pt_static_queue_count = 0;
	l1pt_static_create_count = 0;
	SIMPLEQ_INIT(&l1pt_queue);
	l1pt_queue_count = 0;
	l1pt_create_count = 0;
	l1pt_reuse_count = 0;
}


/*
 * pmap_postinit()
 *
 * This routine is called after the vm and kmem subsystems have been
 * initialised. This allows the pmap code to perform any initialisation
 * that can only be done one the memory allocation is in place.
 */

void
pmap_postinit()
{
	int loop;
	struct l1pt *pt;

#ifdef PMAP_STATIC_L1S
	for (loop = 0; loop < PMAP_STATIC_L1S; ++loop) {
#else	/* PMAP_STATIC_L1S */
	for (loop = 0; loop < max_processes; ++loop) {
#endif	/* PMAP_STATIC_L1S */
		/* Allocate a L1 page table */
		pt = pmap_alloc_l1pt();
		if (!pt)
			panic("Cannot allocate static L1 page tables\n");

		/* Clean it */
		bzero((void *)pt->pt_va, PD_SIZE);
		pt->pt_flags |= (PTFLAG_STATIC | PTFLAG_CLEAN);
		/* Add the page table to the queue */
		SIMPLEQ_INSERT_TAIL(&l1pt_static_queue, pt, pt_queue);
		++l1pt_static_queue_count;
		++l1pt_static_create_count;
	}
}


/*
 * Create and return a physical map.
 *
 * If the size specified for the map is zero, the map is an actual physical
 * map, and may be referenced by the hardware.
 *
 * If the size specified is non-zero, the map will be used in software only,
 * and is bounded by that size.
 */

pmap_t
pmap_create(size)
	vm_size_t size;
{
	register pmap_t pmap;

	/* Software use map does not need a pmap */

	if (size) return NULL;

	/* Allocate memory for pmap structure and zero it */
	pmap = (pmap_t) malloc(sizeof *pmap, M_VMPMAP, M_WAITOK);
	bzero(pmap, sizeof(*pmap));

	/* Now init the machine part of the pmap */

	pmap_pinit(pmap);
	return(pmap);
}

/*
 * pmap_alloc_l1pt()
 *
 * This routine allocates physical and virtual memory for a L1 page table
 * and wires in.
 * A l1pt structure is returned to describe the allocated page table.
 *
 * This routine is allowed to fail if the required memory cannot be allocated.
 * In this case NULL is returned.
 */

struct l1pt *
pmap_alloc_l1pt(void)
{
	vm_offset_t va, pa;
	struct l1pt *pt;
	int error;
	vm_page_t m;
	pt_entry_t *pte;

	/* Allocate virtual address space for the L1 page table */
	va = kmem_alloc_pageable(kernel_map, PD_SIZE);
	if (va == 0) {
#ifdef DIAGNOSTIC
		printf("pmap: Cannot allocate pageable memory for L1\n");
#endif	/* DIAGNOSTIC */
		return(NULL);
	}

	/* Allocate memory for the l1pt structure */
	pt = (struct l1pt *)malloc(sizeof(struct l1pt), M_VMPMAP, M_WAITOK);

	/*
	 * Allocate pages from the VM system.
	 */
	TAILQ_INIT(&pt->pt_plist);
	error = vm_page_alloc_memory(PD_SIZE, physical_start, physical_end,
	    PD_SIZE, 0, &pt->pt_plist, 1, M_WAITOK);
	if (error) {
#ifdef DIAGNOSTIC
		printf("pmap: Cannot allocate physical memory for L1 (%d)\n",
		    error);
#endif	/* DIAGNOSTIC */
		/* Release the resources we already have claimed */
		free(pt, M_VMPMAP);
		kmem_free(kernel_map, va, PD_SIZE);
		return(NULL);
	}

	/* Map our physical pages into our virtual space */
	pt->pt_va = va;
	m = pt->pt_plist.tqh_first;
	while (m && va < (pt->pt_va + PD_SIZE)) {
		pa = VM_PAGE_TO_PHYS(m);

		pmap_enter(pmap_kernel(), va, pa,
		    VM_PROT_READ | VM_PROT_WRITE, TRUE);

		/* Revoke cacheability and bufferability */
		/* XXX should be done better than this */
		pte = pmap_pte(pmap_kernel(), va);
		*pte = (*pte) & ~(PT_C | PT_B);		

		va += NBPG;
		m = m->pageq.tqe_next;
	}

#ifdef DIAGNOSTIC
	if (m)
		panic("pmap_alloc_l1pt: pglist not empty\n");
#endif	/* DIAGNOSTIC */

	pt->pt_flags = 0;
	return(pt);
}

void
pmap_free_l1pt(pt)
	struct l1pt *pt;
{
	/* Separate the physical memory for the virtual space */
	pmap_remove(kernel_pmap, pt->pt_va, pt->pt_va + PD_SIZE);

	/* Return the physical memory */
	vm_page_free_memory(&pt->pt_plist);

	/* Free the virtual space */
	kmem_free(kernel_map, pt->pt_va, PD_SIZE);

	/* Free the l1pt structure */
	free(pt, M_VMPMAP);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 *
 * MAJOR rewrite in progress
 */

int
pmap_allocpagedir(pmap)
	struct pmap *pmap;
{
	vm_offset_t pa, va;
	struct l1pt *pt;
	pt_entry_t *pte;

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_allocpagedir(%p)\n", pmap);
#endif	/* PMAP_DEBUG */

	/* Do we have any spare L1's lying around ? */
	if (l1pt_static_queue_count) {
		--l1pt_static_queue_count;
		pt = l1pt_static_queue.sqh_first;
		SIMPLEQ_REMOVE_HEAD(&l1pt_static_queue, pt, pt_queue);
	} else if (l1pt_queue_count) {
		--l1pt_queue_count;
		pt = l1pt_queue.sqh_first;
		SIMPLEQ_REMOVE_HEAD(&l1pt_queue, pt, pt_queue);
		++l1pt_reuse_count;
	} else {
		pt = pmap_alloc_l1pt();
		if (!pt)
			return(ENOMEM);
		++l1pt_create_count;
	}

	pmap->pm_l1pt = pt;
	pa = VM_PAGE_TO_PHYS(pt->pt_plist.tqh_first);

	/* Calculate the virtual address of the page directory */

	pmap->pm_pdir = (pd_entry_t *)pt->pt_va;

	/* Clean the L1 if it is dirty */
	if (!(pt->pt_flags & PTFLAG_CLEAN))
		bzero((void *)pmap->pm_pdir, PD_SIZE - 0x400);

	/* Do we already have the kernel mappings ? */
	if (!(pt->pt_flags & PTFLAG_KPT)) {
		/* Duplicate the kernel mapping i.e. all mappings 0xf0000000+ */

		bcopy((char *)kernel_pmap->pm_pdir + 0x3c00,
		    (char *)pmap->pm_pdir + 0x3c00, 0x400);
		pt->pt_flags |= PTFLAG_KPT;
	}

	/* Allocate a page table to map all the page tables for this pmap */

#ifdef DIAGNOSTIC
	if (pmap->pm_vptpt) {
		/* XXX What if we have one already ? */
		panic("pmap_allocpagedir: have pt already\n");
	}
#endif	/* DIAGNOSTIC */
	pmap->pm_vptpt = kmem_alloc(kernel_map, NBPG);
	pmap->pm_pptpt = pmap_extract(kernel_pmap, pmap->pm_vptpt) & PG_FRAME;
	/* Revoke cacheability and bufferability */
	/* XXX should be done better than this */
	pte = pmap_pte(kernel_pmap, pmap->pm_vptpt);
	*pte = (*pte) & ~(PT_C | PT_B);		

	/* Wire in this page table */

	pmap->pm_pdir[0xefc] = L1_PTE(pmap->pm_pptpt + 0x000);
	pmap->pm_pdir[0xefd] = L1_PTE(pmap->pm_pptpt + 0x400);
	pmap->pm_pdir[0xefe] = L1_PTE(pmap->pm_pptpt + 0x800);
	pmap->pm_pdir[0xeff] = L1_PTE(pmap->pm_pptpt + 0xc00);

	pt->pt_flags &= ~PTFLAG_CLEAN;	/* L1 is dirty now */

	/*
	 * Map the kernel page tables for 0xf0000000 +
	 * into the page table used to map the
	 * pmap's page tables
	 */

	bcopy((char *)(PROCESS_PAGE_TBLS_BASE
	    + (PROCESS_PAGE_TBLS_BASE >> (PGSHIFT-2)) + 0xf00),
	    (char *)pmap->pm_vptpt + 0xf00, 0x100);

	/* Add a self reference reference */

	*((pt_entry_t *)(pmap->pm_vptpt + 0xefc)) =
	    L2_PTE_NC_NB(pmap->pm_pptpt, AP_KRW);

	/*
	 * Now we get nasty. We need to map the page
	 * directory to a standard address in the memory
	 *  map. This means we can easily find the active
	 * page directory. This is needed by pmap_pte to
	 * hook in an alternate pmap's page tables.
	 * This means that a page table is needed in each
	 * process to map this memory as the kernel
	 * tables cannot be used as they are shared.
	 * The HACK is to borrow some of the space in
	 * the page table that maps all the pmap page
	 * tables.
	 * Mapping a 16KB page directory into that means
	 * that a 16MB chunk of the memory map will no
	 * longer be mappable. Eventually a chuck of
	 * user space (at the top end) could be reserved
	 * for this but for the moment the 16MB block at
	 * 0xf5000000 is not allocated and so has become
	 * reserved for this processes.
	 */

	/* XXX Must really clean this up - mark */

	*((pt_entry_t *)(pmap->pm_vptpt + 0xf50)) =
	    L2_PTE_NC_NB(pa + 0x0000, AP_KRW);
	*((pt_entry_t *)(pmap->pm_vptpt + 0xf54)) =
	    L2_PTE_NC_NB(pa + 0x1000, AP_KRW);
	*((pt_entry_t *)(pmap->pm_vptpt + 0xf58)) =
	    L2_PTE_NC_NB(pa + 0x2000, AP_KRW);
	*((pt_entry_t *)(pmap->pm_vptpt + 0xf5c)) =
	    L2_PTE_NC_NB(pa + 0x3000, AP_KRW);

	/* XXX - the pmap is not in use thus should not need cleaning */
	/* Also the page tables are not mapped */
	cpu_cache_purgeID();
	cpu_tlb_flushID();

	pmap->pm_count = 1;
	simple_lock_init(&pmap->pm_lock);

	return(0);
}


/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */

static int pmap_pagedir_ident;	/* tsleep() ident */

void
pmap_pinit(pmap)
	struct pmap *pmap;
{
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_pinit(%p)\n", pmap);
#endif	/* PMAP_DEBUG */

	/* Keep looping until we succeed in allocating a page directory */

	while (pmap_allocpagedir(pmap) != 0) {
		/*
		 * Ok we failed to allocate a suitable block of memory for an
		 * L1 page table. This means that :
		 * 1. 16KB of virtual address space could not be allocated
		 * 2. 16KB of physically contiguous memory on a 16KB boundary
		 *    could not be allocated.
		 *
		 * Since we cannot fail we will sleep for a while and try
		 * again. Although we will be waken with another page table
		 * is freed other memory releasing and swapping may occur
		 * that will mean we can succeed so we will keep trying
		 * regularly just in case.
		 */

		if (tsleep((caddr_t)&pmap_pagedir_ident, PZERO,
		   "l1ptwait", 1000) == EWOULDBLOCK)
			printf("pmap: Cannot allocate L1 page table, sleeping ...\n");
	}
}


void
pmap_freepagedir(pmap)
	register pmap_t pmap;
{
	/* junk the L1 page table */
	if (pmap->pm_l1pt->pt_flags & PTFLAG_STATIC) {
		/* Add the page table to the queue */
		SIMPLEQ_INSERT_TAIL(&l1pt_static_queue, pmap->pm_l1pt, pt_queue);
		++l1pt_static_queue_count;
		/* Wake up any sleeping processes waiting for a l1 page table */
		wakeup((caddr_t)&pmap_pagedir_ident);
	} else if (l1pt_queue_count < 8) {
		/* Add the page table to the queue */
		SIMPLEQ_INSERT_TAIL(&l1pt_queue, pmap->pm_l1pt, pt_queue);
		++l1pt_queue_count;
		/* Wake up any sleeping processes waiting for a l1 page table */
		wakeup((caddr_t)&pmap_pagedir_ident);
	} else
		pmap_free_l1pt(pmap->pm_l1pt);
}


/*
 * Retire the given physical map from service.
 * Should only be called if the map contains no valid mappings.
 */

void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int count;

	if (pmap == NULL)
		return;

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_destroy(%p)\n", pmap);
#endif	/* PMAP_DEBUG */

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
	register pmap_t pmap;
{
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_release(%p)\n", pmap);
#endif	/* PMAP_DEBUG */

#if 0
	if (pmap->pm_count != 1)		/* XXX: needs sorting */
		panic("pmap_release count %d", pmap->pm_count);
#endif

	/* Free the memory used for the page table mapping */

	kmem_free(kernel_map, (vm_offset_t)pmap->pm_vptpt, NBPG);

	pmap_freepagedir(pmap);
}



/*
 * void pmap_reference(pmap_t pmap)
 *
 * Add a reference to the specified pmap.
 */

void
pmap_reference(pmap)
	pmap_t pmap;
{
	if (pmap == NULL)
		return;
                                  
	simple_lock(&pmap->pm_lock);
	pmap->pm_count++;
	simple_unlock(&pmap->pm_lock);
}
                                                                                     
  
/*
 * void pmap_virtual_space(vm_offset_t *start, vm_offset_t *end)
 *
 * Return the start and end addresses of the kernel's virtual space.
 * These values are setup in pmap_bootstrap and are updated as pages
 * are allocated.
 */

void
pmap_virtual_space(start, end)
	vm_offset_t *start;
	vm_offset_t *end;
{
	*start = virtual_start;
	*end = virtual_end;
}


/*
 * void pmap_pageable(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
 *   boolean_t pageable)
 *  
 * Make the specified pages (by pmap, offset) pageable (or not) as requested.
 *
 * A page which is not pageable may not take a fault; therefore, its
 * page table entry must remain valid for the duration.
 *
 * This routine is merely advisory; pmap_enter will specify that these
 * pages are to be wired down (or not) as appropriate.
 */
 
void
pmap_pageable(pmap, sva, eva, pageable)
	pmap_t pmap;
	vm_offset_t sva;
	vm_offset_t eva;
	boolean_t pageable;
{
	/*
	 * Ok we can only make the specified pages pageable under the
	 * following conditions.
	 * 1. pageable == TRUE
	 * 2. eva = sva + NBPG
	 * 3. the pmap is the kernel_pmap ??? - got this from
	 *    i386/pmap.c ??
	 *
	 * right this will get called when making pagetables pageable
	 */
 
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 5)
		printf("pmap_pageable: pmap=%p sva=%08x eva=%08x p=%d\n",
		    pmap, (u_int) sva, (u_int) eva, (int) pageable);
#endif	/* PMAP_DEBUG */
 
	if (pmap == kernel_pmap && pageable && eva == (sva + NBPG)) {
		vm_offset_t pa;
		pt_entry_t *pte;

		pte = pmap_pte(pmap, sva);
		if (!pte)
			return;
		if (!pmap_pte_v(pte))
			return;
		pa = pmap_pte_pa(pte);

		/*
		 * Mark it unmodified to avoid pageout
		 */
		pmap_clear_modify(pa);

#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0)
			printf("pmap_pageable: ermm can't really do this yet (%08x)!\n",
			    (int) sva);
#endif	/* PMAP_DEBUG */
	}
}                                     


void
pmap_activate(pmap, pcbp)
	pmap_t pmap;
	struct pcb *pcbp;
{
	if (pmap != NULL) {
		pcbp->pcb_pagedir = (pd_entry_t *)pmap_extract(kernel_pmap,
		    (vm_offset_t)pmap->pm_pdir);
#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0)
			printf("pmap_activate: pmap=%p pcb=%p pdir=%p l1=%p\n",
			    pmap, pcbp, pmap->pm_pdir,
			    pcbp->pcb_pagedir);
#endif	/* PMAP_DEBUG */

		if (pmap == curproc->p_vmspace->vm_map.pmap) {
			printf("pmap: Setting TTB\n");
			setttb((u_int)pcbp->pcb_pagedir);
		}
/*		pmap->pm_pdchanged = FALSE;*/
	}
}


/*
 * void pmap_zero_page(vm_offset_t phys)
 *
 * zero fill the specific physical page. To do this the page must be
 * mapped into the virtual memory. Then bzero can be used to zero it.
 */
 
void
pmap_zero_page(phys)
	vm_offset_t phys;
{
	int s;
	register struct pv_entry *pv;
	vm_offset_t addr;
	int pind;

#ifdef PMAP_DEBUG
	if (pmap_debug_level > 0)
		printf("pmap_zero_page(pa=%08x)", phys);
#endif	/* PMAP_DEBUG */

	pind = pmap_page_index(phys);
#ifdef DIAGNOSTIC
	if (pind == -1)
		panic("pmap_zero_page: pind=-1 phys=%x\n", phys);
#endif	/* DIAGNOSTIC */
	s = splimp();
	if (pv_table) {
		pv = &pv_table[pind];
		if (pv) {
			if (pv->pv_pmap) {
				if (pv->pv_next)
					pv = NULL;
				else
					addr = pv->pv_va;
			} else
				addr = 0;
		}
	} else
		pv = NULL;
	(void)splx(s);
	if (pv == NULL)
		cpu_cache_purgeID();	/* We need to purge as the page may have dirty cache entries */
	else if (addr)
		cpu_cache_purgeID_rng(addr, NBPG);	/* Clean the page */

	/* Hook the physical page into the memory at our special hook point */

	*page_hook0.pte = L2_PTE(phys & PG_FRAME, AP_KRW);

	/* Flush the tlb - eventually this can be a purge tlb */

	cpu_tlb_flushD_SE(page_hook0.va);

	/* Zero the memory */

	bzero_page(page_hook0.va);

	cpu_cache_purgeD_rng(page_hook0.va, NBPG);

	/* XXX we should purge just this page */
	/* XXX we can use sync rather than clean as nothing else will use this va */
/*	sync_caches();*/
	drain_writebuf();
}


/*
 * void pmap_copy_page(vm_offset_t src, vm_offset_t dest)
 *
 * pmap_copy_page copies the specified page by mapping it into virtual
 * memory and using bcopy to copy its contents.
 */
 
void
pmap_copy_page(src, dest)
	vm_offset_t src;
	vm_offset_t dest;
{
	int s;
	register struct pv_entry *spv, *dpv;
	vm_offset_t saddr, daddr;
	int spind, dpind;

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= -1)
		printf("pmap_copy_pag(src=P%08x, dest=P%08x)\n",
		    src, dest);
#endif	/* PMAP_DEBUG */

	cpu_cache_cleanD();	/* We need to clean as the page may have dirty cache entries */

	spind = pmap_page_index(src);
	dpind = pmap_page_index(dest);
#ifdef DIAGNOSTIC
	if (spind == -1)
		panic("pmap_copy_page: pind=-1 src=%x\n", src);
	if (dpind == -1)
		panic("pmap_copy_page: pind=-1 dest=%x\n", dest);
#endif	/* DIAGNOSTIC */
	s = splimp();
	if (pv_table) {
		spv = &pv_table[spind];
		if (spv) {
			if (spv->pv_pmap) {
				if (spv->pv_next)
					spv = NULL;
				else
					saddr = spv->pv_va;
			} else
				saddr = 0;
		}
		dpv = &pv_table[dpind];
		if (dpv) {
			if (dpv->pv_pmap) {
				if (dpv->pv_next)
					dpv = NULL;
				else
					daddr = dpv->pv_va;
			} else
				daddr = 0;
		}
	} else
		spv = dpv = NULL;
	(void)splx(s);
	if (spv == NULL || dpv == NULL)
		cpu_cache_purgeID();	/* We need to purge as the page may have dirty cache entries */
	else if (saddr)
		cpu_cache_purgeID_rng(saddr, NBPG);	/* Clean the page */
	else if (daddr)
		cpu_cache_purgeID_rng(daddr, NBPG);	/* Clean the page */

	/* Hook the physical page into the memory at our special hook point */

	*page_hook0.pte = L2_PTE(src & PG_FRAME, AP_KRW);
	*page_hook1.pte = L2_PTE(dest & PG_FRAME, AP_KRW);

	/* Flush the tlb - eventually this can be a purge tlb */

	cpu_tlb_flushD_SE(page_hook0.va);
	cpu_tlb_flushD_SE(page_hook1.va);

	/* Copy the memory */

	bcopy_page(page_hook0.va, page_hook1.va);

	cpu_cache_purgeD_rng(page_hook0.va, NBPG);
	cpu_cache_purgeD_rng(page_hook1.va, NBPG);

	/* XXX we should purge just this page */
	/* XXX we can use sync rather than clean as nothing else will use this va */
/*	sync_caches();*/
	drain_writebuf();
}


/*
 * int pmap_next_page(vm_offset_t *addr)
 *
 * Allocate another physical page returning true or false depending
 * on whether a page could be allocated.
 *
 * MARK - This needs optimising ... look at the amiga version
 * but since it is only used during booting, who cares ?
 */
 
int
pmap_next_page(addr)
	vm_offset_t *addr;
{
	if (free_pages == 0 || physical_freestart == physical_freeend) {
#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0)
			printf("pmap_next_page: Trying to allocate beyond memory\n");
#endif	/* PMAP_DEBUG */
		return(FALSE);
	} 

/*	pmap_zero_page(physical_freestart);*/
	*addr = physical_freestart;
	--free_pages;
  
	physical_freestart += NBPG;
	if (physical_freestart == (bootconfig.dram[physical_memoryblock].address
	    + bootconfig.dram[physical_memoryblock].pages * NBPG)) {
		++physical_memoryblock;
		if (bootconfig.dram[physical_memoryblock].address != 0)
			physical_freestart = bootconfig.dram[physical_memoryblock].address;
	}

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 10) {
		printf("pmap_next_page: Allocated physpage %08x\n", *addr);
		printf("pmap_next_page: Next page is       %08x\n", physical_freestart);
	}
#endif	/* PMAP_DEBUG */

	return(TRUE);
}


/*
 * int pmap_next_phys_page(vm_offset_t *addr)
 *
 * Allocate another physical page returning true or false depending
 * on whether a page could be allocated.
 */
 
vm_offset_t
pmap_next_phys_page(addr)
	vm_offset_t addr;
	
{
	int loop;

	if (addr < bootconfig.dram[0].address)
		return(bootconfig.dram[0].address);

	loop = 0;

	while (bootconfig.dram[loop].address != 0
	    && addr > (bootconfig.dram[loop].address + bootconfig.dram[loop].pages * NBPG))
		++loop;

	if (bootconfig.dram[loop].address == 0)
		return(0);

	addr += NBPG;
	
	if (addr >= (bootconfig.dram[loop].address + bootconfig.dram[loop].pages * NBPG)) {
		if (bootconfig.dram[loop + 1].address == 0)
			return(0);
		addr = bootconfig.dram[loop + 1].address;
	}

	return(addr);
}


/*
 * unsigned int pmap_free_pages(void)
 *
 * Returns the number of free pages the system has.
 * free_pages is set up during initarm and decremented every time a page
 * is allocated.
 */
 
unsigned int
pmap_free_pages()
{
#ifdef PMAP_DEBUG
	if (pmap_debug_level > 5)
		printf("pmap_free_pages: %08x pages free\n", free_pages);
#endif	/* PMAP_DEBUG */
	return(free_pages);
}

/*
 * int pmap_page_index(vm_offset_t pa)
 *
 * returns a linear index to the physical page. This routine has to take
 * a physical address and work out the corresponding physical page number.
 * There does not appear to be a simple way of doing this as the memory
 * is split into a series of blocks. We search each block to determine
 * which block the physical page is. Once we have scanned the blocks to
 * this point we can calculate the physical page index.
 *
 * XXX This is called frequently can could do with some optimisation 
 */
 
int
pmap_page_index(pa)
	vm_offset_t pa;
{
	register int index;
	register int loop;
	register vm_offset_t start, end;

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 5)
		printf("pmap_page_index(pa=P%08x)", pa);
#endif	/* PMAP_DEBUG */

	index = 0;
	loop = 0;
	while (loop < bootconfig.dramblocks) {
		start = (vm_offset_t)bootconfig.dram[loop].address;
		end = start + (bootconfig.dram[loop].pages * NBPG);
		if (pa < end) {
			if (pa < start)
				return(-1);
			index += (pa - start) >> PGSHIFT;
#ifdef PMAP_DEBUG
			if (pmap_debug_level >= 5)
				printf(" index = %08x\n" ,index);
#endif	/* PMAP_DEBUG */
			return(index);
			
		} else {
			index += bootconfig.dram[loop].pages;
		}
		++loop;
	}

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 5)
		printf(" index = Invalid\n");
	if (pmap_debug_level >= 1)
		printf("page invalid - no index %08x\n", pa);
#endif	/* PMAP_DEBUG */
	return(-1);
}

void
pmap_remove(pmap, sva, eva)
	struct pmap *pmap;
	vm_offset_t sva;
	vm_offset_t eva;
{
	register pt_entry_t *pte = NULL;
	vm_offset_t pa;
	int pind;
	int flush = 0;

	/*
	 * Make sure pmap is valid. -dct
	 */
	if (pmap == NULL) {
/*		printf("pmap_remove: Called with pmap=0 sva=%08x eva=%08x\n",
		    (u_int)sva, (u_int)eva);*/
		return;
	}

	cpu_cache_cleanID_rng(sva, (eva - sva));

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_remove: pmap=%p sva=%08x eva=%08x\n",
		    pmap, (int) sva, (int) eva);
#endif	/* PMAP_DEBUG */

	sva &= PG_FRAME;
	eva &= PG_FRAME;

	/*
	 * We need to acquire a pointer to a page table page before entering
	 * the following loop.
	 */

	while (sva < eva) {
		pte = pmap_pte(pmap, sva);
		if (pte) break;
		sva = (sva & PD_MASK) + NBPD;
	}

	while (sva < eva) {
		/* only check once in a while */
		if ((sva & PT_MASK) == 0) {
			if (!pmap_pde_v(pmap_pde(pmap, sva))) {
				/* We can race ahead here, to the next pde. */
				 sva += NBPD;
				 pte += arm_byte_to_page(NBPD);
				 continue;
			 }
		}

		if (!pmap_pte_v(pte))
			goto next;

		flush = 1;

		/*
		 * Update statistics
		 */

		/* Wired bit done below */

/*
		if (pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;
*/
		pmap->pm_stats.resident_count--;

		pa = pmap_pte_pa(pte);

		/*
		 * Invalidate the PTEs.
		 * XXX: should cluster them up and invalidate as many
		 * as possible at once.
		 */

#ifdef DEBUG
		if (pmapdebug & PDB_REMOVE)
			printf("remove: inv pte at %x(%x) ", pte, *pte);
#endif	/* DEBUG */

		if ((pind = pmap_page_index(pa)) != -1) {
/*			pmap_attributes[pind] |= *pte & (PG_M | PG_U);*/
			/* pmap_remove_pv will update pmap_attributes */
#ifdef DIAGNOSTIC
			if (pind < 0) {
				printf("eerk ! pind=%08x pa=%08x\n", pind, pa);
				panic("The axe has fallen, were dead\n");
			}
#endif	/* DIAGNOSTIC */
			if (pmap_remove_pv(pmap, sva, pind) & PT_W)
				pmap->pm_stats.wired_count--;
		}

		*pte = 0;
next:
		sva += NBPG;
		pte++;
	}

	if (flush)
		cpu_tlb_flushID();
}


/*
 * Routine:	pmap_remove_all
 * Function:
 *		Removes this physical page from
 *		all physical maps in which it resides.
 *		Reflects back modify bits to the pager.
 */

void
pmap_remove_all(pa)
	vm_offset_t pa;
{
	struct pv_entry *ph, *pv, *npv;
	register pmap_t pmap;
	register pt_entry_t *pte;
	int pind;
	int s;

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_remove_all: pa=%08x ", pa);
#endif	/* PMAP_DEBUG */
#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove_all(%x)", pa);
	/*pmap_pvdump(pa);*/
#endif	/* DEBUG */

	if ((pind = pmap_page_index(pa)) == -1) {
#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0)
			printf("no accounting\n");
#endif	/* PMAP_DEBUG */
		return;
	}

	cpu_cache_purgeID();

	s = splimp();
	pv = ph = &pv_table[pind];

	if (ph->pv_pmap == NULL) {
#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0)
			printf("free page\n");
#endif	/* PMAP_DEBUG */
		(void)splx(s);
		return;
	}

	while (pv) {
		pmap = pv->pv_pmap;
		pte = pmap_pte(pmap, pv->pv_va);

#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0)
			printf("[%p,%08x,%08x,%08x] ", pmap, *pte,
			    (int) pv->pv_va, pv->pv_flags);
#endif	/* PMAP_DEBUG */
#ifdef DEBUG
		if (!pte || !pmap_pte_v(pte) || pmap_pte_pa(pte) != pa)
			panic("pmap_remove_all: bad mapping");
#endif	/* DEBUG */

		/*
		 * Update statistics
		 */
		/* Wired bit done below */

/*
		if (pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;
*/
		pmap->pm_stats.resident_count--;

		if (pv->pv_flags & PT_W)
			pmap->pm_stats.wired_count--;
          
		/*
		 * Invalidate the PTEs.
		 * XXX: should cluster them up and invalidate as many
		 * as possible at once.
		 */
#ifdef DEBUG
		if (pmapdebug & PDB_REMOVE)
			printf("remove: inv pte at %x(%x) ", pte, *pte);
#endif	/* DEBUG */

#ifdef needednotdone
reduce wiring count on page table pages as references drop
#endif

		/*
		 * Update saved attributes for managed page
		 */

/*		pmap_attributes[pind] |= *pte & (PG_M | PG_U);*/
		pmap_attributes[pind] |= pv->pv_flags & (PT_M | PT_H);

		*pte = 0;

		npv = pv->pv_next;
		if (pv == ph)
			ph->pv_pmap = NULL;
		else
			pmap_free_pv(pv);
		pv = npv;
	}
	(void)splx(s);

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("done\n");
#endif	/* PMAP_DEBUG */

	cpu_tlb_flushID();
}


/*
 * Set the physical protection on the specified range of this map as requested.
 */

void
pmap_protect(pmap, sva, eva, prot)
	pmap_t pmap;
	vm_offset_t sva;
	vm_offset_t eva;
	vm_prot_t prot;
{
	register pt_entry_t *pte = NULL;
	register int armprot;
	int flush = 0;
	vm_offset_t pa;
	int pind;

	/*
	 * Make sure pmap is valid. -dct
	 */
	if (pmap == NULL) {
/*		printf("pmap_protect: Called with pmap=0 sva=%08x eva=%08x prot=%d\n", (u_int)sva, (u_int)eva, prot);*/
		return;
	}

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_protect: pmap=%p %08x->%08x %08x\n",
		    pmap, (int) sva, (int) eva, prot);
#endif	/* PMAP_DEBUG */

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	if (prot & VM_PROT_WRITE)
		return;

	/* Since we only change protections a cache clean is not necessary ? */
/*	cpu_cache_purgeID_rng(sva, (eva - sva));*/

	sva &= PG_FRAME;
	eva &= PG_FRAME;

	/*
	 * We need to acquire a pointer to a page table page before entering
	 * the following loop.
	 */

	while (sva < eva) {
		pte = pmap_pte(pmap, sva);
		if (pte)
			break;
		sva = (sva & PD_MASK) + NBPD;
	}

	while (sva < eva) {
/*printf("pmap_protect: sva = %08x eva=%08x\n", sva, eva);*/
		/* only check once in a while */
		if ((sva & PT_MASK) == 0) {
			if (!pmap_pde_v(pmap_pde(pmap, sva))) {
			/* We can race ahead here, to the next pde. */
				sva += NBPD;
				pte += arm_byte_to_page(NBPD);
				continue;
			}
		}

		if (!pmap_pte_v(pte))
			goto next;

		flush = 1;

		armprot = 0;
/*
		if (prot & VM_PROT_WRITE)
			armprot |= PT_AP(AP_W);
*/
		if (sva < VM_MAXUSER_ADDRESS)
			armprot |= PT_AP(AP_U);
		else if (sva < VM_MAX_ADDRESS)
			armprot |= PT_AP(AP_W);  /* XXX Ekk what is this ? */
		*pte = (*pte & 0xfffff00f) | armprot;

		pa = pmap_pte_pa(pte);

#if 0
		/* Get the physical page index */

		if ((pind = pmap_page_index(pa)) == -1)
			panic("pmap_protect: pmap_page_index failed for pte %08x\n", (u_int)pte); 

		/* Clear write flag */

		pmap_modify_pv(pmap, sva, pind, PT_Wr, 0);
#else
		/* Get the physical page index */

		/* Clear write flag */
		if ((pind = pmap_page_index(pa)) != -1) 
			pmap_modify_pv(pmap, sva, pind, PT_Wr, 0);
#endif
next:
		sva += NBPG;
		pte++;
	}

	if (flush)
		cpu_tlb_flushID();
}


int
pmap_nightmare(pmap, pind, va, prot)
	pmap_t pmap;
	int pind;
	vm_offset_t va;
	vm_prot_t prot;
{
	struct pv_entry *pv, *npv;
	int entries = 0;
	int writeable = 0;
	int cacheable = 0;

	/* We may need to inhibit the cache on all the mappings */

	pv = &pv_table[pind];

	if (pv->pv_pmap == NULL)
		panic("pmap_enter: pv_entry has been lost\n");

	/*
	 * There is at least one other VA mapping this page.
	 */
	for (npv = pv; npv; npv = npv->pv_next) {
		/* Count mappings in the same pmap */
		if (pmap == npv->pv_pmap) {
			++entries;
			/* Writeable mappings */
			if (npv->pv_flags & PT_Wr)
				++writeable;
		}
	}
	if (entries > 1) {
#ifdef PORTMASTER
/*		printf("pmap_nightmare: e=%d w=%d p=%d c=%d pind=%x va=%x [", entries, writeable, (prot & VM_PROT_WRITE), cacheable, pind, (u_int)va);*/
#endif
		for (npv = pv; npv; npv = npv->pv_next) {
			/* Count mappings in the same pmap */
			if (pmap == npv->pv_pmap) {
/*				printf("va=%x ", (u_int)npv->pv_va);*/
			}
		}
/*		printf("]\n");*/
#if 0
		if (writeable || (prot & VM_PROT_WRITE))) {
			for (npv = pv; npv; npv = npv->pv_next) {
				/* Revoke cacheability */
				if (pmap == npv->pv_pmap) {
					pte = pmap_pte(pmap, npv->pv_va);
					*pte = (*pte) & ~(PT_C | PT_B);
				}
			}
		}
#endif
	} else {
		if ((prot & VM_PROT_WRITE) == 0)
			cacheable = PT_C;
#ifdef PORTMASTER
/*		if (cacheable == 0)
			printf("pmap_nightmare: w=%d p=%d va=%x c=%d\n", writeable, (prot & VM_PROT_WRITE), (u_int)va, cacheable);*/
#endif
	}

	return(cacheable);
}


int
pmap_nightmare1(pmap, pind, va, prot, cacheable)
	pmap_t pmap;
	int pind;
	vm_offset_t va;
	vm_prot_t prot;
	int cacheable;
{
	struct pv_entry *pv, *npv;
	int entries = 0;
	int writeable = 0;

	/* We may need to inhibit the cache on all the mappings */

	pv = &pv_table[pind];

	if (pv->pv_pmap == NULL)
		panic("pmap_enter: pv_entry has been lost\n");

	/*
	 * There is at least one other VA mapping this page.
	 */
	for (npv = pv; npv; npv = npv->pv_next) {
		/* Count mappings in the same pmap */
		if (pmap == npv->pv_pmap) {
			++entries;
			/* Writeable mappings */
			if (npv->pv_flags & PT_Wr)
				++writeable;
		}
	}
	if (entries > 1) {
#ifdef PORTMASTER
/*		printf("pmap_nightmare1: e=%d w=%d p=%d c=%d pind=%x va=%x [", entries, writeable, (prot & VM_PROT_WRITE), cacheable, pind, (u_int)va);*/
#endif
		for (npv = pv; npv; npv = npv->pv_next) {
			/* Count mappings in the same pmap */
			if (pmap == npv->pv_pmap) {
/*				printf("va=%x ", (u_int)npv->pv_va);*/
			}
		}
/*		printf("]\n");*/
#if 0
		if (writeable || (prot & VM_PROT_WRITE))) {
			for (npv = pv; npv; npv = npv->pv_next) {
				/* Revoke cacheability */
				if (pmap == npv->pv_pmap) {
					pte = pmap_pte(pmap, npv->pv_va);
					*pte = (*pte) & ~(PT_C | PT_B);
				}
			}
		}
#endif
	} else {
/*		cacheable = PT_C;*/
/*		printf("pmap_nightmare1: w=%d p=%d va=%x c=%d\n", writeable, (prot & VM_PROT_WRITE), (u_int)va, cacheable);*/
	}

	return(cacheable);
}


/*
 * void pmap_enter(pmap_t pmap, vm_offset_t va, vm_offset_t pa, vm_prot_t prot,
 * boolean_t wired)
 *  
 *      Insert the given physical page (p) at
 *      the specified virtual address (v) in the
 *      target physical map with the protection requested.
 *
 *      If specified, the page will be wired down, meaning
 *      that the related pte can not be reclaimed.
 *
 *      NB:  This is the only routine which MAY NOT lazy-evaluate
 *      or lose information.  That is, this routine must actually
 *      insert this page into the given map NOW.
 */

void
pmap_enter(pmap, va, pa, prot, wired)
	pmap_t pmap;
	vm_offset_t va;
	vm_offset_t pa;
	vm_prot_t prot;
	boolean_t wired;
{
	register pt_entry_t *pte;
	register u_int npte;
	int pind = -2;
	u_int cacheable = 0;
	vm_offset_t opa = -1;

#if PMAP_DEBUG > 5
	printf("pmap_enter: V%08x P%08x in pmap %p\n", va, pa, pmap);
#endif	/* PMAP_DEBUG */

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 5)
		printf("pmap_enter: V%08x P%08x in pmap %p prot=%08x, wired = %d\n",
		    (int) va, (int) pa, pmap, prot, wired);
#endif	/* PMAP_DEBUG */

	/* Valid pmap ? */

	if (pmap == NULL)
		return;

	/* Valid address ? */
                        
	if (va >= VM_KERNEL_VIRTUAL_MAX)
		panic("pmap_enter: too big");

	/*
	 * Get a pointer to the pte for this virtual address. We should
	 * always have a page table ready for us so if the page table
	 * is missing ....
	 */

	pte = pmap_pte(pmap, va);
	if (!pte) {
		printf("pmap_enter: pde = %08x\n", (u_int) pmap_pde(pmap, va));
		printf("pmap_enter: pte = %08x\n", (u_int) pmap_pte(pmap, va));
		panic("Failure 01 in pmap_enter (V%08x P%08x)\n", (u_int) va, (u_int) pa);                                               
	}

	/* More debugging info */

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 5) {
		printf("pmap_enter: pte for V%08x = V%08x", (u_int) va, (u_int) pte);
		printf(" (%08x)\n", *pte);
	}
#endif	/* PMAP_DEBUG */

	/* Is the pte valid ? If so then this page is already mapped */

	if (pmap_pte_v(pte)) {
/*		vm_offset_t opa;*/

		/* Get the physical address of the current page mapped */
        
		opa = pmap_pte_pa(pte);

		/* Are we mapping the same page ? */
        
		if (opa == pa) {
			int flags;

			/* All we must be doing is changing the protection */

#ifdef PMAP_DEBUG
			if (pmap_debug_level >= 0)
				printf("Case 02 in pmap_enter (V%08x P%08x)\n", (u_int) va, (u_int) pa);
#endif	/* PMAP_DEBUG */

			pind = pmap_page_index(pa);
			cacheable = (*pte) & PT_C;

			/* Has the wiring changed ? */

			if (pind != -1) {
				flags = pmap_modify_pv(pmap, va, pind, 0, 0) & PT_W;
				if (flags && !wired)
					--pmap->pm_stats.wired_count;
				else if (!flags && wired)
					++pmap->pm_stats.wired_count;

 				cacheable = pmap_nightmare1(pmap, pind, va, prot, cacheable);
 			}

		} else {
			/* We are replacing the page with a new one. */

/*			cache_clean();*/
			cpu_cache_purgeID_rng(va, NBPG);

#ifdef PMAP_DEBUG
			if (pmap_debug_level >= 0)
				printf("Case 03 in pmap_enter (V%08x P%08x P%08x)\n",
				    (int) va, (int) pa, (int) opa);
#endif	/* PMAP_DEBUG */

			/*
			 * If it is part of our managed memory then we
			 * must remove it from the PV list
			 */

			if ((pind = pmap_page_index(opa)) != -1) {
/*				pmap_attributes[pind] |= *pte & (PG_M | PG_U);*/
				/*
				 * pmap_remove_pv updates pmap_attribute
				 * Adjust the wiring count if the page was
				 * wired
				 */
				if (pmap_remove_pv(pmap, va, pind) & PT_W)
					--pmap->pm_stats.wired_count;

			}

			/* Update the wiring stats for the new page */

			if (wired)
				++pmap->pm_stats.wired_count;

			/*
			 * Enter on the PV list if part of our managed memory
			 */
			if ((pind = pmap_page_index(pa)) != -1) {
				if (pmap_enter_pv(pmap, va, pind, 0))
 					cacheable = PT_C;
 				else
 					cacheable = pmap_nightmare(pmap, pind, va, prot);
 			} else {
#if 0
 /*
  * Assumption: if it is not part of our managed memory
  * then it must be device memory which may be volatile.
  * The video memory may be double mapped but should only be accesses from
  * one address at a time. Will be double mapped by the X server and
  * we must keep the cache on for performance.
  */
 				if (pa >= videomemory.vidm_pbase
 				    && pa < videomemory.vidm_pbase + videomemory.vidm_size)
 					cacheable = PT_C;
 				else 
 #endif
					cacheable = 0;
			}
		}
	} else {
		/* pte is not valid so we must be hooking in a new page */

/*		cache_clean();*/
		cpu_cache_purgeID_rng(va, NBPG);

		++pmap->pm_stats.resident_count;
		if (wired)
			++pmap->pm_stats.wired_count;

		/*
		 * Enter on the PV list if part of our managed memory
		 */
		if ((pind = pmap_page_index(pa)) != -1) {
			if (pmap_enter_pv(pmap, va, pind, 0)) 
				cacheable = PT_C;
			else
				cacheable = pmap_nightmare(pmap, pind, va, prot);
		} else {
			 /*
			  * Assumption: if it is not part of our managed
			  * memory then it must be device memory which
			  * may be volatile.
			  */
#if 0
 /*
  * Bit of a hack really as all doubly mapped addresses should be uncached
  * as the cache works on virtual addresses not physical ones.
  * However for the video memory this kill performance and the screen
  * should be locked anyway so there should only be one process trying
  * to write to it at once.
  */
 
 			if (pa >= videomemory.vidm_pbase
 			    && pa < videomemory.vidm_pbase + videomemory.vidm_size)
 				cacheable = PT_C;
 			else 
#endif
 				cacheable = 0;
#ifdef PMAP_DEBUG
			if (pmap_debug_level > 0)
 				printf("pmap_enter: non-managed memory mapping va=%08x pa=%08x\n",
 				    (int) va, (int) pa);
#endif	/* PMAP_DEBUG */
		}
	}

	/* Construct the pte, giving the correct access */
      
	npte = (pa & PG_FRAME) | cacheable;
	if (pind != -1)
		npte |= PT_B;

#ifdef DIAGNOSTIC
	if (va == 0 && (prot & VM_PROT_WRITE))
		printf("va=0 prot=%d\n", prot);
#endif	/* DIAGNOSTIC */

/*
	if (va < VM_MIN_ADDRESS)
		printf("pmap_enter: va=%08x\n", (u_int)va);*/

/*	if (va >= VM_MIN_ADDRESS && va < VM_MAXUSER_ADDRESS && !wired)
 		npte |= L2_INVAL;
	else*/
		npte |= L2_SPAGE;

	if (prot & VM_PROT_WRITE)
		npte |= PT_AP(AP_W);

	if (va >= VM_MIN_ADDRESS) {
		if (va < VM_MAXUSER_ADDRESS)
			npte |= PT_AP(AP_U);
		else if (va < VM_MAX_ADDRESS) { /* This must be a page table */
			npte |= PT_AP(AP_W);
			npte &= ~(PT_C | PT_B);
		}
	}

 	if (va >= VM_MIN_ADDRESS && va < VM_MAXUSER_ADDRESS && pind != -1) /* Inhibit write access for user pages */
 		*pte = (npte & ~PT_AP(AP_W));
	else
		*pte = npte;

	if (*pte == 0)
		panic("oopss: *pte = 0 in pmap_enter() npte=%08x\n", npte);

	if (pind != -1) {
		int flags = 0;
         
		if (wired) flags |= PT_W;
			flags |= npte & (PT_Wr | PT_Us);
/*		if (flags & PT_Wr) flags |= PT_M;*/
#ifdef DIAGNOSTIC
		if (pind < 0)
			printf("pind=%08x\n", pind);
#endif	/* DIAGNOSTIC */
/*		pmap_modify_pv(pmap, va, pind, 0xffffffff, flags);*/
		pmap_modify_pv(pmap, va, pind, ~(PT_Wr | PT_Us | PT_W), flags);
	}

	/*
	 * If we are mapping in a page to where the page tables are store
	 * then we must be mapping a page table. In this case we should
	 * also map the page table into the page directory
	 */
 
	if (va >= PROCESS_PAGE_TBLS_BASE && va < 0xf0000000) {
#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0) {
			printf("Page being mapped in the page table area\n");
			printf("page P%08x will be installed in the L1 table as well\n",
			    (int) pa);
		}
#endif	/* PMAP_DEBUG */
		pa = pa & PG_FRAME;

		if (opa != pa) {
			pmap->pm_pdir[((va - PROCESS_PAGE_TBLS_BASE)>>10)+0] = L1_PTE(pa + 0x000);
			pmap->pm_pdir[((va - PROCESS_PAGE_TBLS_BASE)>>10)+1] = L1_PTE(pa + 0x400);
			pmap->pm_pdir[((va - PROCESS_PAGE_TBLS_BASE)>>10)+2] = L1_PTE(pa + 0x800);
			pmap->pm_pdir[((va - PROCESS_PAGE_TBLS_BASE)>>10)+3] = L1_PTE(pa + 0xc00);
		}
		/* Should be just a purge */
/*		cache_clean();*/
	}

	/* Better flush the TLB ... */

	cpu_tlb_flushID_SE(va);

#if PMAP_DEBUG > 5
	printf("pmap_enter: pte = V%08x %08x\n", pte, *pte);
#endif	/* PMAP_DEBUG */
}


/*
 * pmap_page_protect:
 *
 * Lower the permission for all mappings to a given page.
 */

void
pmap_page_protect(phys, prot)
	vm_offset_t phys;
	vm_prot_t prot;
{
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_page_protect(pa=%08x, prot=%d)\n", phys, prot);
#endif	/* PMAP_DEBUG */

	switch(prot) {
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		pmap_copy_on_write(phys);
		break;

	case VM_PROT_ALL:
		break;

	default:
		pmap_remove_all(phys);
		break;
	}
}


/*
 * Routine:	pmap_change_wiring
 * Function:	Change the wiring attribute for a map/virtual-address
 *		pair.
 * In/out conditions:
 *		The mapping must already exist in the pmap.
 */

void
pmap_change_wiring(pmap, va, wired)
	pmap_t pmap;
	vm_offset_t va;
	boolean_t wired;
{
	register pt_entry_t *pte;
	vm_offset_t pa;
	int pind;
	int current;

	/*
	 * Make sure pmap is valid. -dct
	 */
	if (pmap == NULL) {
/*		printf("pmap_change_wiring: Called with pmap=0 va=%08x wired=%08x\n", (u_int)va, wired);*/
		return;
	}

	/* Get the pte */

	pte = pmap_pte(pmap, va);
	if (!pte)
		return;

	/* Extract the physical address of the page */

	pa = pmap_pte_pa(pte);

	/* Get the physical page index */

	if ((pind = pmap_page_index(pa)) == -1)
		return; 

	/* Update the wired bit in the pv entry for this page. */

	current = pmap_modify_pv(pmap, va, pind, PT_W, wired ? PT_W : 0) & PT_W;

	/* Update the statistics */

	if (wired & !current)
		pmap->pm_stats.wired_count++;
	else if (!wired && current)
		pmap->pm_stats.wired_count--;
}


/*
 * pt_entry_t *pmap_pte(pmap_t pmap, vm_offset_t va)
 *
 * Return the pointer to a page table entry corresponding to the supplied
 * virtual address.
 *
 * The page directory is first checked to make sure that a page table
 * for the address in question exists and if it does a pointer to the
 * entry is returned.
 *
 * The way this works is that that the kernel page tables are mapped
 * into the memory map at 0xf3c00000 to 0xf3fffffff. This allows
 * page tables to be located quickly.
 */

int pmap_pte_entries = 0;
int pmap_pte_entries_alt = 0;
 
pt_entry_t *
pmap_pte(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	pd_entry_t *pde;
	pt_entry_t *ptp;
	pt_entry_t *result;

	/* The pmap must be valid */
                        
	if (!pmap)
		return(NULL);

	/* Return the address of the pte */

#if PMAP_DEBUG > 10
	printf("pmap_pte: pmap=%p va=V%08x pde = V%08x", pmap, va,
	   pmap_pde(pmap, va));
	printf(" (%08x)\n", *(pmap_pde(pmap, va)));
#endif	/* PMAP_DEBUG */

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 10) {
		printf("pmap_pte: pmap=%p va=V%08x pde = V%08x", pmap,
		    (int) va, (int) pmap_pde(pmap, va));
		printf(" (%08x)\n", *(pmap_pde(pmap, va)));
	}
#endif	/* PMAP_DEBUG */

	/* Do we have a valid pde ? If not we don't have a page table */

	if (!pmap_pde_v(pmap_pde(pmap, va))) {
#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0)
			printf("pmap_pte: failed - pde = %08x\n", (int) pmap_pde(pmap, va));
#endif	/* PMAP_DEBUG */
		return(NULL); 
	}

	++pmap_pte_entries;

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 10)
		printf("pmap pagetable = P%08x current = P%08x\n", (int) pmap->pm_pptpt,
		    (*((pt_entry_t *)(PROCESS_PAGE_TBLS_BASE
		    + (PROCESS_PAGE_TBLS_BASE >> (PGSHIFT-2))+0xefc)) & PG_FRAME));
#endif	/* PMAP_DEBUG */

	if (pmap == kernel_pmap || pmap->pm_pptpt
	    == (*((pt_entry_t *)(PROCESS_PAGE_TBLS_BASE
	    + ((PROCESS_PAGE_TBLS_BASE >> (PGSHIFT-2)) & ~3)+0xefc)) & PG_FRAME)) {
		ptp = (pt_entry_t *)PROCESS_PAGE_TBLS_BASE;
	} else {
		pde = (pd_entry_t *)CURRENT_PAGEDIR_BASE;        
		/*cache_clean();*/	/* XXX is this needed as we are replacing a page table which is not cached */
		++pmap_pte_entries_alt;
          
		ptp = (pt_entry_t *)ALT_PAGE_TBLS_BASE;
		pde[(ALT_PAGE_TBLS_BASE >> 20) + 0] = L1_PTE(pmap->pm_pptpt + 0x000);
		pde[(ALT_PAGE_TBLS_BASE >> 20) + 1] = L1_PTE(pmap->pm_pptpt + 0x400);
		pde[(ALT_PAGE_TBLS_BASE >> 20) + 2] = L1_PTE(pmap->pm_pptpt + 0x800);
		pde[(ALT_PAGE_TBLS_BASE >> 20) + 3] = L1_PTE(pmap->pm_pptpt + 0xc00);

		*((pt_entry_t *)(PROCESS_PAGE_TBLS_BASE + ((PROCESS_PAGE_TBLS_BASE
		    >> (PGSHIFT-2)) & ~3) + (ALT_PAGE_TBLS_BASE >> 20))) =
		    L2_PTE_NC_NB(pmap->pm_pptpt, AP_KRW);
/*		cache_clean();*/
		cpu_tlb_flushD();
	}
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 10)
		printf("page tables base = %08x\n", (int) ptp);
#endif	/* PMAP_DEBUG */
	result = (pt_entry_t *)((char *)ptp + ((va >> (PGSHIFT-2)) & ~3));
	return(result);
}                                                                         


/*
 * Routine:  pmap_extract
 * Function:
 *           Extract the physical page address associated
 *           with the given map/virtual_address pair.
 */

vm_offset_t
pmap_extract(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	register pt_entry_t *pte;
	register vm_offset_t pa;
/*
	printf("pmap_extract(pmap=%p, va=V%08x\n", pmap, va);
*/

	/*
	 * Get the pte for this virtual address. If there is no pte
	 * then there is no page table etc.
	 */
  
	pte = pmap_pte(pmap, va);
	if (!pte)
		return(0);

	/* Is the pte valid ? If not then no paged is actually mapped here */

	if (!pmap_pte_v(pte))
		return(0);

	/* Extract the physical address from the pte */

	pa = pmap_pte_pa(pte);

/*	printf("pmap_extract: pa = P%08x\n", (pa | (va & ~PG_FRAME))); */  

	return(pa | (va & ~PG_FRAME));
}


#if 0  /* Macro in pmap.h */
pt_entry_t *
vtopte(va)
	vm_offset_t va;
{
	return((pt_entry_t *)(PROCESS_PAGE_TBLS_BASE
		+ (arm_byte_to_page(va) << 2)));
}
#endif

#if 0  /* Macro in pmap.h */
u_int
vtophys(va)
	vm_offset_t va;
{
	return((*vtopte(va) & PG_FRAME) | ((unsigned)(va) & ~PG_FRAME));
}       
#endif


/*
 * Copy the range specified by src_addr/len from the source map to the
 * range dst_addr/len in the destination map.
 *
 * This routine is only advisory and need not do anything.
 */

void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t dst_pmap;
	pmap_t src_pmap;
	vm_offset_t dst_addr;
	vm_size_t len;
	vm_offset_t src_addr;
{
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_copy(%p, %p, %x, %x, %x)\n",
		    dst_pmap, src_pmap, (int) dst_addr,
		    (int) len, (int) src_addr);
#endif	/* PMAP_DEBUG */
}


void
pmap_dump_pvlist(phys, m)
	vm_offset_t phys;
	char *m;
{
	register struct pv_entry *pv;

	if (!pv_table)
		return;
 
	printf("%s %08x:", m, (int) phys);
	pv = &pv_table[pmap_page_index(phys)];
	if (pv->pv_pmap == NULL) {
		printf(" no mappings\n");
		return;
	}

	for (; pv; pv = pv->pv_next)
		printf(" pmap %p va %08x flags %08x", pv->pv_pmap,
		    (int) pv->pv_va, pv->pv_flags);

	printf("\n");
}


void
pmap_dump_pvs()
{
	register struct pv_entry *pv;
	register int loop;

	if (!pv_table)
		return;
 
	printf("pv dump\n");

	for (loop = 0; loop < npages; ++loop) {
		pv = &pv_table[loop];
		if (pv->pv_pmap != NULL) {
			printf("%4d : ", loop);
			for (; pv; pv = pv->pv_next) {
				printf(" pmap %p va %08x flags %08x", pv->pv_pmap,
				    (int) pv->pv_va, pv->pv_flags);
			}
			printf("\n");
		}
	}
  }


void
pmap_dump_mpvs()
{
	register struct pv_entry *pv;
	register int loop;

	if (!pv_table)
		return;
 
	printf("pv dump\n");

	for (loop = 0; loop < npages; ++loop) {
		pv = &pv_table[loop];
		if (pv->pv_pmap != NULL && pv->pv_next != NULL) {
			printf("%4d : ", loop);
			for (; pv; pv = pv->pv_next) {
				printf(" pmap %p va %08x flags %08x", pv->pv_pmap,
				    (int) pv->pv_va, pv->pv_flags);
			}
			printf("\n");
		}
	}
  }


boolean_t
pmap_testbit(pa, setbits)
	vm_offset_t pa;
	int setbits;
{
	register struct pv_entry *pv;
/*	register pt_entry_t *pte;*/
	int pind;
	int s;

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 1)
		printf("pmap_testbit: pa=%08x set=%08x\n", (int) pa, setbits);
#endif	/* PMAP_DEBUG */

	if (pv_table == NULL || pmap_attributes == NULL)
		return(FALSE);

	if ((pind = pmap_page_index(pa)) == -1)
		return(FALSE);

	s = splimp();
	pv = &pv_table[pind];

#ifdef PMAP_DEBUG
/*	if (pmap_debug_level >= 0)
		printf("pmap_attributes = %02x\n", pmap_attributes[pind]);
*/
#endif	/* PMAP_DEBUG */

	/*
	 * Check saved info first
	 */

	if (pmap_attributes[pind] & setbits) {
#ifdef PMAP_DEBUG
		if (pmap_debug_level >= 0)
			printf("pmap_attributes = %02x\n", pmap_attributes[pind]);
#endif	/* PMAP_DEBUG */
		(void)splx(s);
		return(TRUE);
	}

	/*
	 * Not found, check current mappings returning
	 * immediately if found.
	 */

	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
/*			pte = pmap_pte(pv->pv_pmap, pv->pv_va);*/

			/* The write bit is in the flags */

			if ((pv->pv_flags & setbits) /*|| (*pte & (setbits & PT_Wr))*/) {
				(void)splx(s);
				return(TRUE);
			}
			if ((setbits & PT_M) && pv->pv_va >= VM_MAXUSER_ADDRESS) {
				(void)splx(s);
				return(TRUE);
			}
			if ((setbits & PT_H) && pv->pv_va >= VM_MAXUSER_ADDRESS) {
				(void)splx(s);
				return(TRUE);
			}
		}
	}

	(void)splx(s);
	return(FALSE);
}


/*
 * Modify pte bits for all ptes corresponding to the given physical address.
 * We use `maskbits' rather than `clearbits' because we're always passing
 * constants and the latter would require an extra inversion at run-time.
 */

void
pmap_changebit(pa, setbits, maskbits)
	vm_offset_t pa;
	int setbits;
	int maskbits;
{
	register struct pv_entry *pv;
	register pt_entry_t *pte;
	vm_offset_t va;
	int pind;
	int s;

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 1)
		printf("pmap_changebit: pa=%08x set=%08x mask=%08x\n",
		    (int) pa, setbits, maskbits);
#endif	/* PMAP_DEBUG */
	if (pv_table == NULL || pmap_attributes == NULL)
		return;

	if ((pind = pmap_page_index(pa)) == -1)
		return;

	s = splimp();
	pv = &pv_table[pind];

	/*
	 * Clear saved attributes (modify, reference)
	 */

#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0 && pmap_attributes[pind])
		printf("pmap_attributes = %02x\n", pmap_attributes[pind]);
#endif	/* PMAP_DEBUG */

	if (~maskbits)
		 pmap_attributes[pind] &= maskbits;

	/*
	 * Loop over all current mappings setting/clearing as appropos
	 */

	if (pv->pv_pmap != NULL) {
		/* XXX do we need this flush as we are only modifying pte permissions not addresses */
/*		if (pv->pv_next)
			cpu_cache_purgeID();
		else
			cpu_cache_purgeID_rng(pv->pv_va, NBPG);*/

		for (; pv; pv = pv->pv_next) {
			va = pv->pv_va;

			/*
			 * XXX don't write protect pager mappings
			 */

			if (maskbits == ~PT_Wr) {
				extern vm_offset_t pager_sva, pager_eva;

				if (va >= pager_sva && va < pager_eva)
					continue;
			}

			pv->pv_flags = (pv->pv_flags & maskbits) | setbits;
			pte = pmap_pte(pv->pv_pmap, va);
			if ((maskbits & PT_Wr) == 0)
				*pte = (*pte) & ~PT_AP(AP_W);
			if (setbits & PT_Wr)
				*pte = (*pte) | PT_AP(AP_W);
/*
			if ((maskbits & PT_H) == 0)
				*pte = ((*pte) & ~L2_MASK) | L2_INVAL;
*/
		}
		cpu_tlb_flushID();
	}
	(void)splx(s);
}


void
pmap_clear_modify(pa)
	vm_offset_t pa;
{
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_clear_modify pa=%08x\n", (int) pa);
#endif	/* PMAP_DEBUG */
	pmap_changebit(pa, 0, ~PT_M);
}


void
pmap_clear_reference(pa)
	vm_offset_t pa;
{
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_clear_reference pa=%08x\n", (int) pa);
#endif	/* PMAP_DEBUG */
	pmap_changebit(pa, 0, ~PT_H);
}


void
pmap_copy_on_write(pa)
	vm_offset_t pa;
{
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_copy_on_write pa=%08x\n", (int) pa);
#endif	/* PMAP_DEBUG */
	pmap_changebit(pa, 0, ~PT_Wr);
}

boolean_t
pmap_is_modified(pa)
	vm_offset_t pa;
{
	boolean_t result;
    
	result = pmap_testbit(pa, PT_M);
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_is_modified pa=%08x %08x\n", (int) pa, result);
#endif	/* PMAP_DEBUG */
	return(result);
}


boolean_t
pmap_is_referenced(pa)
	vm_offset_t pa;
{
	boolean_t result;
/*	int pind;*/
	
	result = pmap_testbit(pa, PT_H);
#ifdef PMAP_DEBUG
	if (pmap_debug_level >= 0)
		printf("pmap_is_referenced pa=%08x %08x\n", (int) pa, result);
#endif	/* PMAP_DEBUG */
	return(result);
}


int
pmap_modified_emulation(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	pt_entry_t *pte;
	vm_offset_t pa;
	int pind;
	u_int flags;

	/* Get the pte */

	pte = pmap_pte(pmap, va);
	if (!pte)
		return(0);

	/* Extract the physical address of the page */

	pa = pmap_pte_pa(pte);

	/* Get the physical page index */

	if ((pind = pmap_page_index(pa)) == -1)
		return(0); 

	/* Get the current flags for this page. */

	flags = pmap_modify_pv(pmap, va, pind, 0, 0);
#ifdef PMAP_DEBUG
	if (pmap_debug_level > 2)
		printf("pmap_modified_emulation: flags = %08x\n", flags);
#endif	/* PMAP_DEBUG */

	/*
	 * Do the flags say this page is writable ? If not then it is a
	 * genuine write fault. If yes then the write fault is our fault
	 * as we did not reflect the write access in the PTE. Now we know
	 * a write has occurred we can correct this and also set the
	 * modified bit
	 */
 
	if (!(flags & PT_Wr))
		return(0);

#ifdef PMAP_DEBUG
	if (pmap_debug_level > 0) {
		printf("pmap_modified_emulation: Got a hit va=%08x\n", (int) va);
		printf("pte = %08x (%08x)", (int) pte, *pte);
	}
#endif	/* PMAP_DEBUG */
	*pte = *pte | PT_AP(AP_W);
#ifdef PMAP_DEBUG
	if (pmap_debug_level > 0)
		printf("->(%08x)\n", *pte);
#endif	/* PMAP_DEBUG */
	cpu_tlb_flushID_SE(va);
    
/*	pmap_modify_pv(pmap, va, pind, PT_M, PT_M);*/

	if (pmap_attributes)
		pmap_attributes[pind] |= PT_M;

	/* Return, indicating the problem has been dealt with */

	return(1);
}


int
pmap_handled_emulation(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	pt_entry_t *pte;
	vm_offset_t pa;
	int pind;
/*	u_int flags;*/

#ifdef PMAP_DEBUG
	if (pmap_debug_level > 2)
		printf("pmap_handled_emulation\n");
#endif	/* PMAP_DEBUG */

	/* Get the pte */

	pte = pmap_pte(pmap, va);
	if (!pte) {
#ifdef PMAP_DEBUG
		if (pmap_debug_level > 2)
			printf("no pte\n");
#endif	/* PMAP_DEBUG */
		return(0);
	}

	/* Check for a zero pte */

#ifdef PMAP_DEBUG
	if (pmap_debug_level > 1)
		printf("*pte=%08x\n", *pte);
#endif	/* PMAP_DEBUG */

	if (*pte == 0)
		return(0);

#ifdef PMAP_DEBUG
	if (pmap_debug_level > 1)
		printf("pmap_handled_emulation: non zero pte %08x\n", *pte);
#endif	/* PMAP_DEBUG */

	/* Have we marked a valid pte as invalid ? */

	if (((*pte) & L2_MASK) != L2_INVAL)
		return(0);

#ifdef PMAP_DEBUG
	if (pmap_debug_level >=-1)
		printf("Got an invalid pte\n");
#endif	/* PMAP_DEBUG */

	/* Extract the physical address of the page */

	pa = pmap_pte_pa(pte);

	/* Get the physical page index */

	if ((pind = pmap_page_index(pa)) == -1)
		return(0); 

/*
 * Ok we just enable the pte and mark the flags as handled
 */

#ifdef PMAP_DEBUG
	if (pmap_debug_level > 0) {
		printf("pmap_handled_emulation: Got a hit va=%08x\n", (int) va);
		printf("pte = %08x (%08x)", (int) pte, *pte);
	}
#endif	/* PMAP_DEBUG */
	*pte = ((*pte) & ~L2_MASK) | L2_SPAGE;
#ifdef PMAP_DEBUG
	if (pmap_debug_level > 0)
		printf("->(%08x)\n", *pte);
#endif	/* PMAP_DEBUG */

	cpu_tlb_flushID_SE(va);

	if (pmap_attributes)
		pmap_attributes[pind] |= PT_H;

	/* Return, indicating the problem has been dealt with */

	return(1);
}


int
pmap_page_attributes(va)
	vm_offset_t va;
{
	vm_offset_t pa;
	int pind;

	/* Get the physical page */

	pa = (vm_offset_t)vtopte(va);

	/* Get the physical page index */

	if ((pind = pmap_page_index(pa)) == -1)
		return(-1); 

	if (pmap_attributes)
		return((int)pmap_attributes[pind]);

	return(-1);
}

#if 0  /* Macro in pmap.h */
vm_offset_t
pmap_phys_address(ppn)
	int ppn;
{
	return(arm_page_to_byte(ppn));
}
#endif


void
pmap_collect(pmap)
	pmap_t pmap;
{
}

/* End of pmap.c */
