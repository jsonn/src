/*	$NetBSD: pmap.c,v 1.4.2.3 2002/10/10 18:35:55 jdolecek Exp $	*/

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com> of Allegro Networks, Inc.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_ddb.h"
#include "opt_kernel_ipt.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <sys/user.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <uvm/uvm.h>

#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/memregion.h>
#include <machine/cacheops.h>


#define PMAP_DIAG
#ifdef PMAP_DIAG
#ifndef DDB
#define pmap_debugger()	panic("")
#else
#include <machine/db_machdep.h>
#define	pmap_debugger() asm volatile("trapa r63");
int validate_kipt(int);
#endif
#endif

#ifdef DEBUG
static int pmap_debug = 0;
#define	PMPRINTF(x)	do { if (pmap_debug) printf x; } while (0)
#else
#define	PMPRINTF(x)
#endif


/*
 * The basic SH5 MMU is just about as simple as these things can be,
 * having two 64-entry TLBs; one for Data and one for Instructions.
 *
 * That's it. End of story. Move along now, nothing more to see.
 *
 * According to the SH5 architecture manual, future silicon may well
 * expand the number of TLB slots, and the number of bits allocated to
 * various fields within the PTEs. Conversely, future silicon may also
 * scrap the separate instruction and data TLBs. Regardless, we defer
 * TLB manipulation to a cpu-specific backend so these details shouldn't
 * affect this pmap.
 * 
 * Given the limited options, the following VM layout is used:
 *
 * 32-bit kernel:
 * ~~~~~~~~~~~~~~
 * 00000000 - BFFFFFFF
 * 	User virtual address space.
 * 
 * 	[DI]TLB misses are looked up in a single, user Inverted Page
 * 	Table Group (pmap_pteg_table) using a hash of the VPN and VSID.
 * 
 * 	The IPT will be big enough to contain one PTEG for every 2 physical
 * 	pages of RAM in the system.
 * 
 * 	PTEG entries will contain 8 PTEs, consisting of PTEL and PTEH
 *	configuration register values (I.e, the bits which contain the
 *	EPN and PPN, size and protection/cache bits), and a VSID which is
 *	used in conjunction with the EPN to uniquely identify any user
 *	mapping in the system.
 * 
 * 	Hash collisions which overflow the 8 PTE group entries per bucket
 *	are dealt with using Overflow PTs.
 *
 *	For a typical NEFF=32 machine with 128MB RAM, the pmap_pteg_table will
 *	require 1.5MB of wired RAM.
 * 
 * C0000000 - DFFFFFFF  (KSEG0)
 *	The kernel lives here.
 *
 * 	Maps first 512MB of physical RAM, with an unused hole at the
 * 	top where sizeof(physical RAM) < 512MB.
 *
 * 	Uses one locked ITLB and DTLB slot for kernel text/data/bss.
 * 	The PTEH.SH bit will be set in these slots so that the kernel
 * 	VM space is mapped regardless of ASID, albeit with supervisor
 * 	access only.
 * 
 *	The size of this mapping is fixed at 512MB since that is the
 *	only suitable size available in the SH5 PTEs.
 *
 *	Note that this is effectively an unmanaged, wired mapping of
 *	the first hunk (or more likely, all) of physical RAM. As such,
 *	we can treat it like a direct-mapped memory segment from which
 *	to allocate backing pages for the pool(9) memory pool manager.
 *	This should *greatly* reduce TLB pressure for many kernel
 *	data structures.
 *
 *	Where physical memory is larger than 512MB, we can still try
 *	to allocate from this segment, and failing that, fall back to
 *	the traditional way.
 * 
 * E0000000 - FFFFFFFF  (KSEG1)
 * 	KSEG1 is basically the `managed' kernel virtual address space
 *	as reported to uvm(9) by pmap_virtual_space().
 * 
 * 	It uses regular TLB mappings, but backed by a dedicated IPT
 * 	with 1-1 V2Phys PTELs. The IPT will be up to 512KB (for NEFF=32)
 *	in size and allocated from KSEG0. IPT entries will be handled in
 *	the same way as the user IPT except for the PTEH, which will be
 * 	synthesised in the TLB handler. No VSID is required.
 *
 * 	Yes, this means device register access can cause a DTLB miss.
 *	In fact, accessing the kernel stack, even while dealing with
 *	an interrupt/exception, can cause a TLB miss!
 *
 * 	However, due to the dedicated 1-1 IPT, these should be handled
 * 	*very* quickly as we won't need to hash the VPN/VSID and deal
 * 	with hash collisions.
 * 
 * 	To reduce TLB misses on hardware registers, it will be possible to
 * 	request mappings which use a larger page size than the default
 *	4KB.
 *
 *	Also note that by reducing the size of managed KVA in KSEG1 from
 *	the default of 512MB to, say 128MB, the IPT size drops to 128KB.
 *
 * 64-bit kernel:
 * ~~~~~~~~~~~~~~
 * TBD.
 */

/*
 * The Primary IPT consists of an array of Hash Buckets, called PTE Groups,
 * where each group is 8 PTEs in size. The number of groups is calculated
 * at boot time such that there is one group for every two NBPG-sized pages
 * of physical RAM.
 */
pteg_t *pmap_pteg_table;	/* Primary IPT group */
int pmap_pteg_cnt;		/* Number of PTE groups. A power of two */
u_int pmap_pteg_mask;		/* Basically, just (pmap_pteg_cnt - 1) */
u_int pmap_pteg_bits;		/* Number of bits set in pmap_pteg_mask */

/*
 * The kernel IPT is a simple array of PTELs, indexed by virtual page
 * number.
 */
ptel_t pmap_kernel_ipt[KERNEL_IPT_SIZE];

/*
 * These are initialised, at boot time by cpu-specific code, to point
 * to cpu-specific functions.
 */
void	(*__cpu_tlbinv)(pteh_t, pteh_t);
void	(*__cpu_tlbinv_cookie)(pteh_t, u_int);
void	(*__cpu_tlbinv_all)(void);
void	(*__cpu_tlbload)(void);		/* Not callable from C */


/*
 * This structure serves a double purpose as over-flow table entry
 * and for tracking phys->virt mappings.
 */
struct pvo_entry {
	LIST_ENTRY(pvo_entry) pvo_vlink;	/* Link to common virt page */
	LIST_ENTRY(pvo_entry) pvo_olink;	/* Link to overflow entry */
	pmap_t pvo_pmap;			/* ptr to owning pmap */
	vaddr_t pvo_vaddr;			/* VA of entry */
#define	PVO_PTEGIDX_MASK	0x0007		/* which PTEG slot */
#define	PVO_PTEGIDX_VALID	0x0008		/* slot is valid */
#define	PVO_WIRED		0x0010		/* PVO entry is wired */
#define	PVO_MANAGED		0x0020		/* PVO e. for managed page */
#define	PVO_WRITABLE		0x0040		/* PVO e. for writable page */
#define	PVO_CACHEABLE		0x0080		/* PVO e. for cacheable page */
/* Note: These three fields must match the values for SH5_PTEL_[RM] */
#define	PVO_REFERENCED		0x0400		/* Cached referenced bit */
#define	PVO_MODIFIED		0x0800		/* Cached modified bit */
#define	PVO_REFMOD_MASK		0x0c00

	ptel_t pvo_ptel;			/* PTEL for this mapping */
};

#define	PVO_VADDR(pvo)		((pvo)->pvo_vaddr & SH5_PTEH_EPN_MASK)
#define	PVO_ISEXECUTABLE(pvo)	((pvo)->pvo_ptel & SH5_PTEL_PR_X)
#define	PVO_ISWIRED(pvo)	((pvo)->pvo_vaddr & PVO_WIRED)
#define	PVO_ISMANAGED(pvo)	((pvo)->pvo_vaddr & PVO_MANAGED)
#define	PVO_ISWRITABLE(pvo)	((pvo)->pvo_vaddr & PVO_WRITABLE)
#define	PVO_ISCACHEABLE(pvo)	((pvo)->pvo_vaddr & PVO_CACHEABLE)
#define	PVO_PTEGIDX_GET(pvo)	((pvo)->pvo_vaddr & PVO_PTEGIDX_MASK)
#define	PVO_PTEGIDX_ISSET(pvo)	((pvo)->pvo_vaddr & PVO_PTEGIDX_VALID)
#define	PVO_PTEGIDX_CLR(pvo)	\
	((void)((pvo)->pvo_vaddr &= ~(PVO_PTEGIDX_VALID|PVO_PTEGIDX_MASK)))
#define	PVO_PTEGIDX_SET(pvo,i)	\
	((void)((pvo)->pvo_vaddr |= (i)|PVO_PTEGIDX_VALID))


/*
 * This array is allocated at boot time and contains one entry per
 * PTE group.
 */
struct pvo_head *pmap_upvo_table;	/* pvo entries by ptegroup index */

static struct evcnt pmap_pteg_idx_events[SH5_PTEG_SIZE] = {
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap",
	    "ptes added at pteg group [0]"),
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap",
	    "ptes added at pteg group [1]"),
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap",
	    "ptes added at pteg group [2]"),
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap",
	    "ptes added at pteg group [3]"),
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap",
	    "ptes added at pteg group [4]"),
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap",
	    "ptes added at pteg group [5]"),
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap",
	    "ptes added at pteg group [6]"),
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap",
	    "ptes added at pteg group [7]")
};

static struct evcnt pmap_pte_spill_events =
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap", "spills");
static struct evcnt pmap_pte_spill_evict_events =
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap", "spill evictions");

static struct evcnt pmap_shared_cache_downgrade_events =
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap", "cache downgrades");
static struct evcnt pmap_shared_cache_upgrade_events =
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap", "cache upgrades");

/*
 * This array contains one entry per kernel IPT entry.
 */
struct pvo_head pmap_kpvo_table[KERNEL_IPT_SIZE];

static struct pvo_head pmap_pvo_unmanaged =
	    LIST_HEAD_INITIALIZER(pmap_pvo_unmanaged);

static struct pool pmap_pool;	   /* pool of pmap structures */
static struct pool pmap_mpvo_pool; /* pool of pvo entries for managed pages */


void pmap_bootstrap(vaddr_t, struct mem_region *);
volatile pte_t *pmap_pte_spill(u_int, vsid_t, vaddr_t);

static volatile pte_t * pmap_pvo_to_pte(const struct pvo_entry *, int);
static struct pvo_entry * pmap_pvo_find_va(pmap_t, vaddr_t, int *);
static void pmap_pinit(pmap_t);
static void pmap_release(pmap_t);
static void pmap_pa_map_kva(vaddr_t, paddr_t, ptel_t);
static ptel_t pmap_pa_unmap_kva(vaddr_t, ptel_t *);
static int pmap_pvo_enter(pmap_t, struct pvo_head *,
	vaddr_t, paddr_t, ptel_t, int);
static void pmap_pvo_remove(struct pvo_entry *, int);
static void pmap_remove_update(struct pvo_head *, int);

static u_int	pmap_asid_next;
static u_int	pmap_asid_max;
static u_int	pmap_asid_generation;
static void	pmap_asid_alloc(pmap_t);

extern void	pmap_asm_zero_page(vaddr_t);
extern void	pmap_asm_copy_page(vaddr_t, vaddr_t);

#define	NPMAPS		16384
#define	VSID_NBPW	(sizeof(uint32_t) * 8)
static uint32_t pmap_vsid_bitmap[NPMAPS / VSID_NBPW];


/*
 * Various pmap related variables
 */
int physmem;
paddr_t avail_start, avail_end;

static struct mem_region *mem;
struct pmap kernel_pmap_store;

static vaddr_t pmap_zero_page_kva;
static vaddr_t pmap_copy_page_src_kva;
static vaddr_t pmap_copy_page_dst_kva;
static vaddr_t pmap_kva_avail_start;
vaddr_t vmmap;

int pmap_initialized;

#ifdef PMAP_DIAG
int pmap_pvo_enter_depth;
int pmap_pvo_remove_depth;
#endif


/*
 * Returns non-zero if the given pmap is `current'.
 */
static __inline int
pmap_is_curpmap(pmap_t pm)
{
	if ((curproc && curproc->p_vmspace->vm_map.pmap == pm) ||
	    pm == pmap_kernel())
		return (1);

	return (0);
}

/*
 * Given a Kernel Virtual Address in KSEG1, return the
 * corresponding index into pmap_kernel_ipt[].
 */
static __inline int
kva_to_iptidx(vaddr_t kva)
{
	int idx;

	if (kva < SH5_KSEG1_BASE)
		return (-1);

	idx = (int) sh5_btop(kva - SH5_KSEG1_BASE);
	if (idx >= KERNEL_IPT_SIZE)
		return (-1);

	return (idx);
}

/*
 * Given a user virtual address, return the corresponding
 * index of the PTE group which is likely to contain the mapping.
 */
static __inline u_int
va_to_pteg(vsid_t vsid, vaddr_t va)
{

	return (pmap_ipt_hash(vsid, va));
}

/*
 * Given a physical address, return a pointer to the head of the
 * list of virtual address which map to that physical address.
 */
static __inline struct pvo_head *
pa_to_pvoh(paddr_t pa, struct vm_page **pg_p)
{
	struct vm_page *pg;

	pg = PHYS_TO_VM_PAGE(pa);

	if (pg_p != NULL)
		*pg_p = pg;

	if (pg == NULL)
		return (&pmap_pvo_unmanaged);

	return (&pg->mdpage.mdpg_pvoh);
}

/*
 * Given a vm_page, return a pointer to the head of the list
 * of virtual addresses which map to that page.
 */
static __inline struct pvo_head *
vm_page_to_pvoh(struct vm_page *pg)
{

	return (&pg->mdpage.mdpg_pvoh);
}

/*
 * Clear the specified bits in the cached attributes of a physical page.
 */
static __inline void
pmap_attr_clear(struct vm_page *pg, int ptebit)
{

	pg->mdpage.mdpg_attrs &= ~ptebit;
}

/*
 * Return the cached attributes of a physical page
 */
static __inline int
pmap_attr_fetch(struct vm_page *pg)
{

	return (pg->mdpage.mdpg_attrs);
}

/*
 * Cache the specified attributes for the physical page "pg"
 */
static __inline void
pmap_attr_save(struct vm_page *pg, int ptebit)
{

	pg->mdpage.mdpg_attrs |= ptebit;
}

/*
 * Returns non-zero if the VSID and VA combination matches the
 * pvo_entry "pvo".
 *
 * That is, the mapping decribed by `pvo' is exactly the same
 * as described by `vsid' and `va'.
 */
static __inline int
pmap_pteh_match(struct pvo_entry *pvo, vsid_t vsid, vaddr_t va)
{

	if (pvo->pvo_pmap->pm_vsid == vsid && PVO_VADDR(pvo) == va)
		return (1);

	return (0);
}

/*
 * Recover the Referenced/Modified bits from the specified PTEH value
 * and cache them temporarily in the PVO.
 */
static __inline void
pmap_pteg_synch(ptel_t ptel, struct pvo_entry *pvo)
{

	pvo->pvo_ptel |= (ptel & SH5_PTEL_RM_MASK);
}

/*
 * We're about to raise the protection of a mapping.
 * Make sure the cache is synchronised before the protection changes.
 */
static void
pmap_cache_sync_raise(vaddr_t va, ptel_t ptel, ptel_t clrbits)
{
	paddr_t pa;

	/*
	 * Just return if the mapping is not cacheable.
	 */
	if ((ptel & SH5_PTEL_CB_MASK) <= SH5_PTEL_CB_DEVICE)
		return;

	/*
	 * Also just return if the page has never been referenced.
	 */
	if ((ptel & SH5_PTEL_R) == 0)
		return;

	pa = (paddr_t)(ptel & SH5_PTEL_PPN_MASK);

	switch ((ptel & clrbits) & (SH5_PTEL_PR_W | SH5_PTEL_PR_X)) {
	case SH5_PTEL_PR_W | SH5_PTEL_PR_X:
		/*
		 * The page is being made no-exec, rd-only.
		 * Purge the data cache and invalidate insn cache.
		 */
		__cpu_cache_dpurge_iinv(va, pa, NBPG);
		break;

	case SH5_PTEL_PR_W:
		/*
		 * The page is being made read-only.
		 * Purge the data-cache.
		 */
		__cpu_cache_dpurge(va, pa, NBPG);
		break;

	case SH5_PTEL_PR_X:
		/*
		 * The page is being made no-exec.
		 * Invalidate the instruction cache.
		 */
		__cpu_cache_iinv(va, pa, NBPG);
		break;

	case 0:
		/*
		 * The page already has the required protection.
		 * No need to touch the cache.
		 */
		break;
	}
}

/*
 * We're about to delete a mapping.
 * Make sure the cache is synchronised before the mapping disappears.
 */
static void
pmap_cache_sync_unmap(vaddr_t va, ptel_t ptel)
{
	paddr_t pa;

	/*
	 * Just return if the mapping was not cacheable.
	 */
	if ((ptel & SH5_PTEL_CB_MASK) <= SH5_PTEL_CB_DEVICE)
		return;

	/*
	 * Also just return if the page has never been referenced.
	 */
	if ((ptel & SH5_PTEL_R) == 0)
		return;

	pa = (paddr_t)(ptel & SH5_PTEL_PPN_MASK);

	switch (ptel & (SH5_PTEL_PR_W | SH5_PTEL_PR_X)) {
	case SH5_PTEL_PR_W | SH5_PTEL_PR_X:
	case SH5_PTEL_PR_X:
		/*
		 * The page was executable, and possibly writable.
		 * Purge the data cache and invalidate insn cache.
		 */
		__cpu_cache_dpurge_iinv(va, pa, NBPG);
		break;

	case SH5_PTEL_PR_W:
		/*
		 * The page was writable.
		 * Purge the data-cache.
		 */
		__cpu_cache_dpurge(va, pa, NBPG);
		break;

	case 0:
		/*
		 * The page was read-only.
		 * Just invalidate the data cache.
		 */
		__cpu_cache_dpurge(va, pa, NBPG);
		break;
	}
}

/*
 * Clear the specified bit(s) in the PTE (actually, the PTEL)
 *
 * In this case, the pte MUST be resident in the ipt, and it may
 * also be resident in the TLB.
 */
static void
pmap_pteg_clear_bit(volatile pte_t *pt, struct pvo_entry *pvo, u_int ptebit)
{
	pmap_t pm;
	ptel_t ptel;
	pteh_t pteh;

	pm = pvo->pvo_pmap;
	pteh = pt->pteh;
	ptel = pt->ptel;
	pt->ptel = ptel & ~ptebit;

	if (pm->pm_asid == PMAP_ASID_KERNEL ||
	    pm->pm_asidgen == pmap_asid_generation) {
		/*
		 * Before raising the protection of the mapping,
		 * make sure the cache is synchronised.
		 *
		 * Note: The cpu-specific cache handling code will ensure
		 * this doesn't cause a TLB miss exception.
		 */
		pmap_cache_sync_raise(PVO_VADDR(pvo), ptel, ptebit);

		/*
		 * The mapping may be cached in the TLB. Call cpu-specific
		 * code to check and invalidate if necessary.
		 */
		__cpu_tlbinv_cookie((pteh & SH5_PTEH_EPN_MASK) |
		    (pm->pm_asid << SH5_PTEH_ASID_SHIFT),
		    SH5_PTEH_TLB_COOKIE(pteh));
	}

	pmap_pteg_synch(ptel, pvo);
}

static void
pmap_kpte_clear_bit(int idx, struct pvo_entry *pvo, ptel_t ptebit)
{
	ptel_t ptel;

	ptel = pmap_kernel_ipt[idx];

	/*
	 * Syncronise the cache in readiness for raising the protection.
	 */
	pmap_cache_sync_raise(PVO_VADDR(pvo), ptel, ptebit);

	/*
	 * Echo the change in the TLB.
	 */
	pmap_kernel_ipt[idx] = ptel & ~ptebit;

	__cpu_tlbinv(PVO_VADDR(pvo) | SH5_PTEH_SH,
	    SH5_PTEH_EPN_MASK | SH5_PTEH_SH);

	pmap_pteg_synch(ptel, pvo);
}

/*
 * Copy the mapping specified in pvo to the PTE structure `pt'.
 * `pt' is assumed to be an entry in the pmap_pteg_table array.
 * 
 * This makes the mapping directly available to the TLB miss
 * handler.
 *
 * It is assumed that the mapping is not currently in the TLB, and
 * hence not in the cache either. Therefore neither need to be
 * synchronised.
 */
static __inline void
pmap_pteg_set(volatile pte_t *pt, struct pvo_entry *pvo)
{
	pt->ptel = pvo->pvo_ptel;
	pt->pteh = PVO_VADDR(pvo);
	pt->vsid = pvo->pvo_pmap->pm_vsid;
}

/*
 * Remove the mapping specified by `pt', which is assumed to point to
 * an entry in the pmap_pteg_table.
 *
 * This function will preserve Referenced/Modified state from the PTE
 * before ensuring there is no reference to the mapping in the TLB.
 */
static void
pmap_pteg_unset(volatile pte_t *pt, struct pvo_entry *pvo)
{
	pmap_t pm;
	pteh_t pteh;
	ptel_t ptel;

	pm = pvo->pvo_pmap;
	ptel = pt->ptel;
	pteh = pt->pteh;
	pt->pteh = 0;

	if (pm->pm_asid == PMAP_ASID_KERNEL ||
	    pm->pm_asidgen == pmap_asid_generation) {
		/*
		 * Before deleting the mapping from the PTEG/TLB,
		 * make sure the cache is synchronised.
		 *
		 * Note: The cpu-specific cache handling code must ensure
		 * this doesn't cause a TLB miss exception.
		 */
		pmap_cache_sync_unmap(PVO_VADDR(pvo), ptel);

		/*
		 * The mapping may be cached in the TLB. Call cpu-specific
		 * code to check and invalidate if necessary.
		 */
		__cpu_tlbinv_cookie((pteh & SH5_PTEH_EPN_MASK) |
		    (pm->pm_asid << SH5_PTEH_ASID_SHIFT),
		    SH5_PTEH_TLB_COOKIE(pteh));
	}

	pmap_pteg_synch(ptel, pvo);
}

/*
 * This function is called when the mapping described by pt/pvo has
 * had its attributes modified in some way. For example, it may have
 * been a read-only mapping but is now read/write.
 */
static __inline void
pmap_pteg_change(volatile pte_t *pt, struct pvo_entry *pvo)
{

	pmap_pteg_unset(pt, pvo);
	pmap_pteg_set(pt, pvo);
}

/*
 * This function inserts the mapping described by `pvo' into the first
 * available vacant slot of the specified PTE group.
 *
 * If a vacant slot was found, this function returns its index.
 * Otherwise, -1 is returned.
 */
static int
pmap_pteg_insert(pteg_t *pteg, struct pvo_entry *pvo)
{
	volatile pte_t *pt;
	int i;

	for (pt = &pteg->pte[0], i = 0; i < SH5_PTEG_SIZE; i++, pt++) {
		if (pt->pteh == 0) {
			pmap_pteg_set(pt, pvo);
			return (i);
		}
	}

	return (-1);
}

/*
 * Spill handler.
 *
 * Tries to spill a page table entry from the overflow area into the
 * main pmap_pteg_table array.
 *
 * This runs on a dedicated TLB miss stack, with exceptions disabled.
 *
 * Note: This is only called for mappings in user virtual address space.
 */
volatile pte_t *
pmap_pte_spill(u_int ptegidx, vsid_t vsid, vaddr_t va)
{
	struct pvo_entry *source_pvo, *victim_pvo;
	struct pvo_entry *pvo;
	int idx, j;
	pteg_t *ptg;
	volatile pte_t *pt;

	/*
	 * Have to substitute some entry.
	 *
	 * Use low bits of the tick counter as random generator
	 */
	idx = (sh5_getctc() >> 2) & 7;
	ptg = &pmap_pteg_table[ptegidx];
	pt = &ptg->pte[idx];

	source_pvo = NULL;
	victim_pvo = NULL;

	pmap_pte_spill_events.ev_count++;

	LIST_FOREACH(pvo, &pmap_upvo_table[ptegidx], pvo_olink) {
		if (source_pvo == NULL && pmap_pteh_match(pvo, vsid, va)) {
			/*
			 * Found the entry to be spilled into the pteg.
			 */
			if (pt->pteh == 0) {
				/* Our victim was already available */
				pmap_pteg_set(pt, pvo);
				j = idx;
			} else {
				/* See if the pteg has a free entry */
				j = pmap_pteg_insert(ptg, pvo);
			}

			if (j >= 0) {
				/* Excellent. No need to evict anyone! */
				PVO_PTEGIDX_SET(pvo, j);
				pmap_pteg_idx_events[j].ev_count++;
				return (&ptg->pte[j]);
			}

			/*
			 * Found a match, but no free entries in the pte group.
			 * This means we have to search for the victim's pvo
			 * if we haven't already found it.
			 */
			source_pvo = pvo;
			if (victim_pvo != NULL)
				break;
		}

		/*
		 * We also need the pvo entry of the victim we are replacing
		 * to save the R & M bits of the PTE.
		 */
		if (victim_pvo == NULL &&
		    PVO_PTEGIDX_ISSET(pvo) && PVO_PTEGIDX_GET(pvo) == idx) {
			/* Found the victim's pvo */
			victim_pvo = pvo;

			/*
			 * If we already have the source pvo, we're all set.
			 */
			if (source_pvo != NULL)
				break;
		}
	}

	/*
	 * If source_pvo is NULL, we have no mapping at the target address
	 * for the current VSID.
	 *
	 * Returning NULL will force the TLB miss code to invoke the full-
	 * blown page fault handler.
	 */
	if (source_pvo == NULL)
		return (NULL);

	/*
	 * If we couldn't find the victim, however, then the pmap module
	 * has become very confused...
	 *
	 * XXX: This panic is pointless; SR.BL is set ...
	 */
	if (victim_pvo == NULL)
		panic("pmap_pte_spill: victim p-pte (%p) has no pvo entry!",pt);

	/*
	 * Update the cached Referenced/Modified bits for the victim and
	 * remove it from the TLB/cache.
	 */
	pmap_pteg_unset(pt, victim_pvo);
	PVO_PTEGIDX_CLR(victim_pvo);

	/*
	 * Copy the source pvo's details into the PTE.
	 */
	pmap_pteg_set(pt, source_pvo);
	PVO_PTEGIDX_SET(source_pvo, idx);

	pmap_pte_spill_evict_events.ev_count++;

	return (pt);
}

void
pmap_bootstrap(vaddr_t avail, struct mem_region *mr)
{
	struct mem_region *mp;
	psize_t size;
	int i;

	uvmexp.pagesize = NBPG;
	uvm_setpagesize();

	for (mp = mr; mp->mr_size; mp++)
		physmem += btoc(mp->mr_size);

	/*
	 * Allow for either a hard-coded size for pmap_pteg_table, or
	 * calculate it based on the physical memory in the system.
	 * We allocate one pteg_t for every two physical pages of
	 * memory.
	 */
#ifdef PTEGCOUNT
	pmap_pteg_cnt = PTEGCOUNT;
#else
	pmap_pteg_cnt = 0x1000;

	while (pmap_pteg_cnt < physmem)
		pmap_pteg_cnt <<= 1;
	pmap_pteg_cnt >>= 1;
#endif

	/*
	 * Calculate some important constants.
	 * These help to speed up TLB lookups, hash and vsid generation.
	 */
	pmap_pteg_mask = pmap_pteg_cnt - 1;
	pmap_pteg_bits = ffs(pmap_pteg_cnt) - 1;

	/*
	 * Allocate wired memory in KSEG0 for pmap_pteg_table.
	 */
	size = pmap_pteg_cnt * sizeof(pteg_t);
	pmap_pteg_table = (pteg_t *)avail;
	memset(pmap_pteg_table, 0, size);
	avail += size;
	mr[0].mr_start += size;
	mr[0].mr_kvastart += size;
	mr[0].mr_size -= size;

	/*
	 * Allocate and initialise PVO structures now
	 */
	size = sh5_round_page(sizeof(struct pvo_head) * pmap_pteg_cnt);
	pmap_upvo_table = (struct pvo_head *) avail;

	for (i = 0; i < pmap_pteg_cnt; i++)
		LIST_INIT(&pmap_upvo_table[i]);

	for (i = 0; i < KERNEL_IPT_SIZE; i++)
		LIST_INIT(&pmap_kpvo_table[i]);

	avail = sh5_round_page(avail + size);
	mr[0].mr_start += size;
	mr[0].mr_kvastart += size;
	mr[0].mr_size -= size;

	/*
	 * Allocate the kernel message buffer
	 */
	size = sh5_round_page(MSGBUFSIZE);
	initmsgbuf((caddr_t)avail, size);

	avail = sh5_round_page(avail + size);
	mr[0].mr_start += size;
	mr[0].mr_kvastart += size;
	mr[0].mr_size -= size;

	mem = mr;

	/*
	 * XXX: Need to sort the memory regions by start address here...
	 */

	avail_start = mr[0].mr_start;

	/*
	 * Tell UVM about physical memory
	 */
	for (mp = mr; mp->mr_size; mp++) {
		uvm_page_physload(atop(mp->mr_start),
		    atop(mp->mr_start + mp->mr_size),
		    atop(mp->mr_start), atop(mp->mr_start + mp->mr_size),
		    VM_FREELIST_DEFAULT);
		avail_end = mp->mr_start + mp->mr_size;
	}

	pmap_zero_page_kva = SH5_KSEG1_BASE;
	pmap_copy_page_src_kva = pmap_zero_page_kva + NBPG;
	pmap_copy_page_dst_kva = pmap_copy_page_src_kva + NBPG;
	vmmap = pmap_copy_page_dst_kva + NBPG;

	pmap_kva_avail_start = vmmap + NBPG;
}

/*
 * This function is used, before pmap_init() is called, to set up fixed
 * wired mappings mostly for the benefit of the bus_space(9) implementation.
 *
 * XXX: Make this more general; it's useful for more than just pre-init
 * mappings. Particularly the big page support.
 */
vaddr_t
pmap_map_device(paddr_t pa, u_int len)
{
	vaddr_t va, rv;
	u_int l = len;
#if 0
	ptel_t *ptelp;
	ptel_t ptel;
	int idx;
#endif

	l = len = sh5_round_page(len);

	if (pmap_initialized == 0) {
		/*
		 * Steal some KVA
		 */
		rv = va = pmap_kva_avail_start;
	} else
		rv = va = uvm_km_valloc(kernel_map, len);

#if 1
	while (len) {
		pmap_kenter_pa(va, pa, VM_PROT_ALL);
		va += NBPG;
		pa += NBPG;
		len -= NBPG;
	}

	if (pmap_initialized == 0)
		pmap_kva_avail_start += l;
#else
	/*
	 * Get the index into pmap_kernel_ipt.
	 */
	if ((idx = kva_to_iptidx(va)) < 0)
		panic("pmap_map_device: Invalid KVA %p", (void *)va);

	ptelp = &pmap_kernel_ipt[idx];
	ptel = (ptel_t)(pa & SH5_PTEL_PPN_MASK) |
	    SH5_PTEL_CB_DEVICE | SH5_PTEL_PR_R | SH5_PTEL_PR_W;

	/*
	 * We optimise the page size for these mappings according to the
	 * requested length. This helps reduce TLB thrashing for large
	 * regions of device memory, for example.
	 */
	while (len) {
		ptel_t pgsize, pgend, mask;
		if (len >= 0x20000000) {
			pgsize = SH5_PTEL_SZ_512MB;	/* Impossible?!?! */
			pgend = ptel + 0x20000000;
			mask = SH5_PTEL_PPN_MASK & ~(0x20000000 - 1);
		} else
		if (len >= 0x100000) {
			pgsize = SH5_PTEL_SZ_1MB;
			pgend = ptel + 0x100000;
			mask = SH5_PTEL_PPN_MASK & ~(0x100000 - 1);
		} else
		if (len >= 0x10000) {
			pgsize = SH5_PTEL_SZ_64KB;
			pgend = ptel + 0x10000;
			mask = SH5_PTEL_PPN_MASK & ~(0x10000 - 1);
		} else {
			pgsize = SH5_PTEL_SZ_4KB;
			pgend = ptel + 0x1000;
			mask = SH5_PTEL_PPN_MASK & ~(0x1000 - 1);
		}

		while (ptel < pgend && len) {
			*ptelp++ = (ptel & mask) | pgsize;
			ptel += NBPG;
			len -= NBPG;
		}
	}
#endif

	return (rv);
}

/*
 * Returns non-zero if the specified mapping is cacheable
 */
int
pmap_page_is_cacheable(pmap_t pm, vaddr_t va)
{
	struct pvo_entry *pvo;
	ptel_t ptel = 0;
	int s;

	if (va < SH5_KSEG1_BASE && va >= SH5_KSEG0_BASE)
		return (1);

	s = splhigh();
	pvo = pmap_pvo_find_va(pm, va, NULL);
	if (pvo != NULL)
		ptel = pvo->pvo_ptel;
	else
	if (pm == pmap_kernel()) {
		int idx = kva_to_iptidx(va);
		if (idx >= 0 && pmap_kernel_ipt[idx])
			ptel = pmap_kernel_ipt[idx];
	}
	splx(s);

	return ((ptel & SH5_PTEL_CB_MASK) > SH5_PTEL_CB_NOCACHE);
}

void
pmap_init(void)
{
	int s, i;

	s = splvm();

	pool_init(&pmap_pool, sizeof(struct pmap),
	    sizeof(void *), 0, 0, "pmap_pl", NULL);

	pool_init(&pmap_mpvo_pool, sizeof(struct pvo_entry),
	    0, 0, 0, "pmap_mpvopl", NULL);

	pool_setlowat(&pmap_mpvo_pool, 1008);

	pmap_asid_next = PMAP_ASID_USER_START;
	pmap_asid_max = SH5_PTEH_ASID_MASK;	/* XXX Should be cpu specific */

	pmap_pinit(pmap_kernel());
	pmap_kernel()->pm_asid = PMAP_ASID_KERNEL;
	pmap_kernel()->pm_asidgen = 0;

	pmap_initialized = 1;

	splx(s);

	evcnt_attach_static(&pmap_pte_spill_events);
	evcnt_attach_static(&pmap_pte_spill_evict_events);
	evcnt_attach_static(&pmap_shared_cache_downgrade_events);
	evcnt_attach_static(&pmap_shared_cache_upgrade_events);
	for (i = 0; i < SH5_PTEG_SIZE; i++)
		evcnt_attach_static(&pmap_pteg_idx_events[i]);
}

/*
 * How much virtual space does the kernel get?
 */
void
pmap_virtual_space(vaddr_t *start, vaddr_t *end)
{

	*start = pmap_kva_avail_start;
	*end = SH5_KSEG1_BASE + ((KERNEL_IPT_SIZE - 1) * NBPG);
}

/*
 * Allocate, initialize, and return a new physical map.
 */
pmap_t
pmap_create(void)
{
	pmap_t pm;
	
	pm = pool_get(&pmap_pool, PR_WAITOK);
	memset((caddr_t)pm, 0, sizeof *pm);
	pmap_pinit(pm);
	return (pm);
}

/*
 * Initialize a preallocated and zeroed pmap structure.
 */
static void
pmap_pinit(pmap_t pm)
{
	u_int entropy = (sh5_getctc() >> 2) + (u_int)time.tv_sec;
	int i;

	pm->pm_refs = 1;
	pm->pm_asid = PMAP_ASID_UNASSIGNED;

	for (i = 0; i < NPMAPS; i += VSID_NBPW) {
		static u_int pmap_vsidcontext;
		u_int32_t mask;
		u_int hash, n;

		pmap_vsidcontext = (pmap_vsidcontext * 0x1105) + entropy;

		/*
		 * The hash only has to be unique in *at least* the
		 * first log2(NPMAPS) bits. However, if there are
		 * more than log2(NPMAPS) significant bits in the
		 * pmap_pteg_table index, then we can make use of
		 * higher-order bits in this hash as additional
		 * entropy for the pmap_pteg hashing function.
		 */
		hash = pmap_vsidcontext & ((pmap_pteg_mask < (NPMAPS - 1)) ?
		    (NPMAPS - 1) : pmap_pteg_mask);
		if (hash == 0)
			continue;

		/*
		 * n    == The index into the pmap_vsid_bitmap[] array.
		 *         This narrows things down to a single uint32_t.
		 * mask == The bit within that u_int32_t which we're
		 *         going to try to allocate.
		 */
		n = (hash & (NPMAPS - 1)) / VSID_NBPW;
		mask = 1 << (hash & (VSID_NBPW - 1));

		if (pmap_vsid_bitmap[n] & mask) {
			/*
			 * Hash collision.
			 * Anything free in this bucket?
			 */
			if (pmap_vsid_bitmap[n] == 0xffffffffu) {
				entropy = pmap_vsidcontext >> 20;
				continue;
			}

			/*
			 * Yup. Adjust the low-order bits of the hash
			 * to match the index of the free slot.
			 */
			i = ffs(~pmap_vsid_bitmap[n]) - 1;
			hash = (hash & ~(VSID_NBPW - 1)) | i;
			mask = 1 << i;
		}

		pmap_vsid_bitmap[n] |= mask;
		pm->pm_vsid = hash;
		return;
	}

	panic("pmap_pinit: out of VSIDs!");
}

/*
 * Add a reference to the given pmap.
 */
void
pmap_reference(pmap_t pm)
{
	pm->pm_refs++;
}

/*
 * Retire the given pmap from service.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_destroy(pmap_t pm)
{
	if (--pm->pm_refs == 0) {
		pmap_release(pm);
		pool_put(&pmap_pool, pm);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 */
void
pmap_release(pmap_t pm)
{
	int idx;
	uint32_t mask;

	idx = (pm->pm_vsid & (NPMAPS - 1)) / VSID_NBPW;
	mask = 1 << (pm->pm_vsid & (VSID_NBPW - 1));

	pmap_vsid_bitmap[idx] &= ~mask;
}

/*
 * Copy the range specified by src_addr/len
 * from the source map to the range dst_addr/len
 * in the destination map.
 *
 * This routine is only advisory and need not do anything.
 */
void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vaddr_t dst_addr,
	vsize_t len, vaddr_t src_addr)
{
}

/*
 * Require that all active physical maps contain no
 * incorrect entries NOW.
 */
void
pmap_update(struct pmap *pmap)
{
}

/*
 * Garbage collects the physical map system for
 * pages which are no longer used.
 * Success need not be guaranteed -- that is, there
 * may well be pages which are not referenced, but
 * others may be collected.
 * Called by the pageout daemon when pages are scarce.
 */
void
pmap_collect(pmap_t pm)
{
}

/*
 * Fill the given physical page with zeroes.
 */
void
pmap_zero_page(paddr_t pa)
{

	if (!pmap_initialized)
		panic("pmap_zero_page: pmap_initialized is false!");

	pmap_pa_map_kva(pmap_zero_page_kva, pa, SH5_PTEL_PR_W);

	pmap_asm_zero_page(pmap_zero_page_kva);

	(void) pmap_pa_unmap_kva(pmap_zero_page_kva, NULL);
}

/*
 * Copy the given physical source page to its destination.
 */
void
pmap_copy_page(paddr_t src, paddr_t dst)
{
	if (!pmap_initialized)
		panic("pmap_copy_page: pmap_initialized is false!");

	PMPRINTF(("pmap_copy_page: copying 0x%08lx -> 0x%08lx\n", src, dst));

	pmap_pa_map_kva(pmap_copy_page_src_kva, src, 0);
	pmap_pa_map_kva(pmap_copy_page_dst_kva, dst, SH5_PTEL_PR_W);

	pmap_asm_copy_page(pmap_copy_page_dst_kva, pmap_copy_page_src_kva);

	(void) pmap_pa_unmap_kva(pmap_copy_page_src_kva, NULL);
	(void) pmap_pa_unmap_kva(pmap_copy_page_dst_kva, NULL);
}

/*
 * Find the PTE described by "pvo" and "ptegidx" within the
 * pmap_pteg_table[] array.
 *
 * If the PTE is not resident, return NULL.
 */
static volatile pte_t *
pmap_pvo_to_pte(const struct pvo_entry *pvo, int ptegidx)
{
	if (!PVO_PTEGIDX_ISSET(pvo))
		return (NULL);

	/*
	 * If we haven't been supplied the ptegidx, calculate it.
	 */
	if (ptegidx < 0) {
		ptegidx = va_to_pteg(pvo->pvo_pmap->pm_vsid, pvo->pvo_vaddr);
		if (ptegidx < 0)
			return (NULL);
	}

	return (&pmap_pteg_table[ptegidx].pte[PVO_PTEGIDX_GET(pvo)]);
}

/*
 * Find the pvo_entry associated with the given virtual address and pmap.
 * If no mapping is found, the function returns NULL. Otherwise:
 *
 * For user pmaps, if pteidx_p is non-NULL and the PTE for the mapping
 * is resident, the index of the PTE in pmap_pteg_table is written
 * to *pteidx_p.
 *
 * For kernel pmap, if pteidx_p is non-NULL, the index of the PTEL in
 * pmap_kernel_ipt is written to *pteidx_p.
 */
static struct pvo_entry *
pmap_pvo_find_va(pmap_t pm, vaddr_t va, int *ptegidx_p)
{
	struct pvo_head *pvo_head;
	struct pvo_entry *pvo;
	int idx;

	va &= SH5_PTEH_EPN_MASK;

	if (va < SH5_KSEG0_BASE) {
		idx = va_to_pteg(pm->pm_vsid, va);
		pvo_head = &pmap_upvo_table[idx];
	} else {
		if ((idx = kva_to_iptidx(va)) < 0)
			return (NULL);
		pvo_head = &pmap_kpvo_table[idx];
	}

	LIST_FOREACH(pvo, pvo_head, pvo_olink) {
		if (pvo->pvo_pmap == pm && PVO_VADDR(pvo) == va) {
			if (ptegidx_p)
				*ptegidx_p = idx;
			return (pvo);
		}
	}

	return (NULL);
}

/*
 * Sets up a single-page kernel mapping such that the Kernel Virtual
 * Address `kva' maps to the specific physical address `pa'.
 *
 * Note: The mapping is set up for read/write, with write-back cacheing
 * enabled,
 */
static void
pmap_pa_map_kva(vaddr_t kva, paddr_t pa, ptel_t wprot)
{
	ptel_t prot;
	int idx;

	/*
	 * Get the index into pmap_kernel_ipt.
	 */
	if ((idx = kva_to_iptidx(kva)) < 0)
		panic("pmap_pa_map_kva: Invalid KVA %p", (void *)kva);

	prot = SH5_PTEL_CB_WRITEBACK | SH5_PTEL_SZ_4KB | SH5_PTEL_PR_R;

	pmap_kernel_ipt[idx] = (ptel_t)(pa & SH5_PTEL_PPN_MASK) | prot | wprot;
}

/*
 * The opposite of the previous function, this removes temporary and/or
 * unmanaged mappings.
 *
 * The contents of the PTEL which described the mapping are returned.
 */
static ptel_t
pmap_pa_unmap_kva(vaddr_t kva, ptel_t *ptel)
{
	ptel_t oldptel;
	int idx;

	if (ptel == NULL) {
		if ((idx = kva_to_iptidx(kva)) < 0)
			panic("pmap_pa_unmap_kva: Invalid KVA %p", (void *)kva);

		ptel = &pmap_kernel_ipt[idx];
	}

	oldptel = *ptel;

	/*
	 * Synchronise the cache before deleting the mapping.
	 */
	pmap_cache_sync_unmap(kva, oldptel);

	/*
	 * Now safe to delete the mapping.
	 */
	*ptel = 0;

	__cpu_tlbinv((kva & SH5_PTEH_EPN_MASK) | SH5_PTEH_SH,
	    SH5_PTEH_EPN_MASK | SH5_PTEH_SH);

	return (oldptel);
}

/*
 * This returns whether this is the first mapping of a page.
 */
static int
pmap_pvo_enter(pmap_t pm, struct pvo_head *pvo_head,
	vaddr_t va, paddr_t pa, ptel_t ptel, int flags)
{
	struct pvo_head *pvo_table_head;
	struct pvo_entry *pvo, *pvop;
	int idx;
	int i, s;
	int poolflags = PR_NOWAIT;

	va &= SH5_PTEH_EPN_MASK;

#ifdef PMAP_DIAG
	if (pm == pmap_kernel() && va < SH5_KSEG1_BASE) {
		printf("pmap_pvo_enter: pmap_kernel() with va 0x%lx!\n", va);
		pmap_debugger();
	}

	if (pmap_pvo_remove_depth > 0) {
		printf("pmap_pvo_enter: pmap_pvo_remove active, for va 0x%lx\n",
		    va);
		pmap_debugger();
	}
	if (pmap_pvo_enter_depth) {
		printf("pmap_pvo_enter: called recursively for va 0x%lx\n", va);
		pmap_debugger();
	}
	pmap_pvo_enter_depth++;
#endif

	if (va < SH5_KSEG1_BASE) {
		KDASSERT(va < SH5_KSEG0_BASE);
		idx = va_to_pteg(pm->pm_vsid, va);
		pvo_table_head = &pmap_upvo_table[idx];
	} else {
		idx = kva_to_iptidx(va);
		pvo_table_head = &pmap_kpvo_table[idx];
	}

	s = splhigh();

	/*
	 * Remove any existing mapping for this virtual page.
	 */
	LIST_FOREACH(pvo, pvo_table_head, pvo_olink) {
		if (pvo->pvo_pmap == pm && PVO_VADDR(pvo) == va)
			break;
	}

	if (pvo != NULL)
		pmap_pvo_remove(pvo, idx);

	splx(s);

	/*
	 * If we aren't overwriting a mapping, try to allocate
	 */
	pvo = pool_get(&pmap_mpvo_pool, poolflags);

	s = splhigh();

	if (pvo == NULL) {
		if ((flags & PMAP_CANFAIL) == 0)
			panic("pmap_pvo_enter: failed");
#ifdef PMAP_DIAG
		pmap_pvo_enter_depth--;
#endif
		splx(s);
		return (ENOMEM);
	}

	ptel |= (ptel_t) (pa & SH5_PTEL_PPN_MASK);
	pvo->pvo_vaddr = va;
	pvo->pvo_pmap = pm;

	if (flags & VM_PROT_WRITE)
		pvo->pvo_vaddr |= PVO_WRITABLE;

	if (flags & PMAP_WIRED) {
		pvo->pvo_vaddr |= PVO_WIRED;
		pm->pm_stats.wired_count++;
	}

	if (pvo_head != &pmap_pvo_unmanaged)
		pvo->pvo_vaddr |= PVO_MANAGED; 

	if ((ptel & SH5_PTEL_CB_MASK) > SH5_PTEL_CB_NOCACHE)
		pvo->pvo_vaddr |= PVO_CACHEABLE;

	/*
	 * Check that the new mapping's cache-mode and VA/writable-attribute
	 * agree with any existing mappings for this physical page.
	 */
	if ((pvop = LIST_FIRST(pvo_head)) != NULL) {
		ptel_t cache_mode, pvo_cache_mode;

		/*
		 * XXX: This should not happen.
		 */
		KDASSERT(pvo_head != &pmap_pvo_unmanaged);

#define _SH5_CM(p)	((p) & SH5_PTEL_CB_MASK)

		pvo_cache_mode = _SH5_CM(pvop->pvo_ptel);
		cache_mode = _SH5_CM(ptel);

		if (cache_mode > pvo_cache_mode) {
			/*
			 * The new mapping is "cacheable", the existing
			 * mappings are "uncacheable". Simply downgrade
			 * the new mapping to match.
			 */
			if (cache_mode > SH5_PTEL_CB_NOCACHE)
				cache_mode = SH5_PTEL_CB_NOCACHE;
		} else
		if (cache_mode < pvo_cache_mode) {
			/*
			 * The new mapping is "uncachable", the existing
			 * mappings are "cacheable". We need to downgrade
			 * all the existing mappings...
			 */
			pvo_cache_mode = SH5_PTEL_CB_NOCACHE;
		}

		if (va != PVO_VADDR(pvop) && PVO_ISWRITABLE(pvo)) {
			/*
			 * Adding a writable mapping at different VA.
			 * Downgrade all the mappings to uncacheable.
			 */
			if (cache_mode > SH5_PTEL_CB_NOCACHE)
				cache_mode = SH5_PTEL_CB_NOCACHE;
			pvo_cache_mode = SH5_PTEL_CB_NOCACHE;
		}

		if (pvo_cache_mode != _SH5_CM(pvop->pvo_ptel)) {
			/*
			 * Downgrade the cache-mode of the existing mappings.
			 */
			LIST_FOREACH(pvop, pvo_head, pvo_vlink) {
				if (_SH5_CM(pvop->pvo_ptel) > pvo_cache_mode) {
					volatile pte_t *pt;
					pt = pmap_pvo_to_pte(pvo, -1);
					if (pt)
						pmap_pteg_unset(pt, pvo);

					pvop->pvo_ptel &= ~SH5_PTEL_CB_MASK;
					pvop->pvo_ptel |= pvo_cache_mode;

					if (pt)
						pmap_pteg_set(pt, pvo);
				  pmap_shared_cache_downgrade_events.ev_count++;
				}
			}
		}

		ptel &= ~SH5_PTEL_CB_MASK;
		ptel |= cache_mode;
#undef _SH5_CM
	}

	pvo->pvo_ptel = ptel;

	LIST_INSERT_HEAD(pvo_table_head, pvo, pvo_olink);
	LIST_INSERT_HEAD(pvo_head, pvo, pvo_vlink);

	pm->pm_stats.resident_count++;

	if (va < SH5_KSEG0_BASE) {
		/*
		 * We hope this succeeds but it isn't required.
		 */
		i = pmap_pteg_insert(&pmap_pteg_table[idx], pvo);
		PMPRINTF((
	  "pmap_pvo_enter: va:0x%lx vsid:%04x ptel:0x%lx group:0x%x idx:%d\n",
		    va, pm->pm_vsid, (u_long)ptel, idx, i));
		if (i >= 0) {
			pmap_pteg_idx_events[i].ev_count++;
			PVO_PTEGIDX_SET(pvo, i);
		}
	} else {
		pmap_kernel_ipt[idx] = ptel;
		PMPRINTF((
		    "pmap_pvo_enter: va 0x%lx, ptel 0x%lx, kipt (idx %d)\n",
		    va, (u_long)ptel, idx));
	}

#ifdef PMAP_DIAG
	pmap_pvo_enter_depth--;
#endif
	splx(s);

	return (0);
}

static void
pmap_pvo_remove(struct pvo_entry *pvo, int idx)
{
	struct vm_page *pg = NULL;

#ifdef PMAP_DIAG
	if (pmap_pvo_remove_depth > 0) {
		printf("pmap_pvo_remove: called recusively, for va 0x%lx\n",
		    PVO_VADDR(pvo));
		pmap_debugger();
	}
	pmap_pvo_remove_depth++;
#endif

	if (PVO_VADDR(pvo) < SH5_KSEG1_BASE) {
		volatile pte_t *pt;

		KDASSERT(PVO_VADDR(pvo) < SH5_KSEG0_BASE);

		/*
		 * Lookup the pte and unmap it.
		 */
		pt = pmap_pvo_to_pte(pvo, idx);
		if (pt != NULL) {
			pmap_pteg_unset(pt, pvo);
			PVO_PTEGIDX_CLR(pvo);
		}
	} else {
		pvo->pvo_ptel |=
		    pmap_pa_unmap_kva(PVO_VADDR(pvo), &pmap_kernel_ipt[idx]) &
		        PVO_REFMOD_MASK;
	}

	/*
	 * Update our statistics
	 */
	pvo->pvo_pmap->pm_stats.resident_count--;
	if (PVO_ISWIRED(pvo))
		pvo->pvo_pmap->pm_stats.wired_count--;

	/*
	 * Save the REF/CHG bits into their cache if the page is managed.
	 */
	if (PVO_ISMANAGED(pvo)) {
		pg = PHYS_TO_VM_PAGE(pvo->pvo_ptel & SH5_PTEL_PPN_MASK);
		if (pg != NULL)	/* XXX: Could assert this ... */
			pmap_attr_save(pg,
			    pvo->pvo_ptel & (SH5_PTEL_R | SH5_PTEL_M));
	}

	/*
	 * Remove this PVO from the PV list
	 */
	LIST_REMOVE(pvo, pvo_vlink);

	/*
	 * Remove this from the Overflow list and return it to the pool...
	 * ... if we aren't going to reuse it.
	 */
	LIST_REMOVE(pvo, pvo_olink);

	pool_put(&pmap_mpvo_pool, pvo);

	/*
	 * If there are/were multiple mappings for the same physical
	 * page, we may just have removed one which had caused the
	 * others to be made uncacheable.
	 */
	if (pg)
		pmap_remove_update(&pg->mdpage.mdpg_pvoh, 1);

#ifdef PMAP_DIAG
	pmap_pvo_remove_depth--;
#endif
}

void
pmap_remove_update(struct pvo_head *pvo_head, int update_pte)
{
	struct pvo_entry *pvo;
	volatile pte_t *pt;
	vaddr_t va;
	int writable;

	/*
	 * XXX: Make sure this doesn't happen
	 */
	KDASSERT(pvo_head != &pmap_pvo_unmanaged);

	if ((pvo = LIST_FIRST(pvo_head)) == NULL || !PVO_ISCACHEABLE(pvo)) {
		/*
		 * There are no more mappings for this physical page,
		 * or the existing mappings are all non-cacheable anyway.
		 */
		return;
	}

	if ((pvo->pvo_ptel & SH5_PTEL_CB_MASK) > SH5_PTEL_CB_NOCACHE) {
		/*
		 * The remaining mappings must already be cacheable, so
		 * nothing more needs doing.
		 */
		return;
	}

	/*
	 * We have to scan the remaining mappings to see if they can all
	 * be made cacheable.
	 */

	va = PVO_VADDR(pvo);
	writable = 0;

	/*
	 * First pass: See if all the mappings are cache-coherent
	 */
	LIST_FOREACH(pvo, pvo_head, pvo_vlink) {
		if (!PVO_ISCACHEABLE(pvo)) {
			/*
			 * At least one of the mappings really is
			 * non-cacheable. This means all the others
			 * must remain uncacheable. Since this must
			 * already be the case, there's nothing more
			 * to do.
			 */
			return;
		}

		if (PVO_ISWRITABLE(pvo)) {
			/*
			 * Record the fact that at least one of the
			 * mappings is writable.
			 */
			writable = 1;
		}

		if (va != PVO_VADDR(pvo) && writable) {
			/*
			 * Oops, one or more of the mappings are writable
			 * and one or more of the VAs are different.
			 * There's nothing more to do.
			 */
			return;
		}
	}

	/*
	 * Looks like we can re-enable cacheing for all the
	 * remaining mappings.
	 */
	LIST_FOREACH(pvo, pvo_head, pvo_vlink) {
		if (update_pte && (pt = pmap_pvo_to_pte(pvo, -1)) != NULL)
			pmap_pteg_unset(pt, pvo);

		pvo->pvo_ptel &= ~SH5_PTEL_CB_MASK;
		pvo->pvo_ptel |= SH5_PTEL_CB_WRITEBACK;

		if (update_pte && pt)
			pmap_pteg_set(pt, pvo);

		pmap_shared_cache_upgrade_events.ev_count++;
	}
}

int
pmap_enter(pmap_t pm, vaddr_t va, paddr_t pa, vm_prot_t prot, int flags)
{
	struct pvo_head *pvo_head;
	struct mem_region *mp;
	struct vm_page *pg;
	ptel_t ptel;
	int error;
	int s;

	PMPRINTF((
	    "pmap_enter: %p: va=0x%lx, pa=0x%lx, prot=0x%x, flags=0x%x\n",
	    pm, va, pa, (u_int)prot, (u_int)flags));

#ifdef PMAP_DIAG
	if (pm == pmap_kernel() && va < SH5_KSEG1_BASE) {
		printf("pmap_enter: pmap_kernel() with va 0x%08x!!\n",
		    (u_int)va);
		pmap_debugger();
	}
#endif

	pvo_head = pa_to_pvoh(pa, &pg);

	/*
	 * Mark the page as Device memory, unless it's in our
	 * available memory array.
	 */
	ptel = SH5_PTEL_CB_DEVICE;
	for (mp = mem; mp->mr_size; mp++) {
		if (pa >= mp->mr_start &&
		    pa < (mp->mr_start + mp->mr_size)) {
			if ((flags & PMAP_NC) == 0)
				ptel = SH5_PTEL_CB_WRITEBACK;
			else
				ptel = SH5_PTEL_CB_NOCACHE;
			break;
		}
	}

	/* Pages are always readable */
	ptel |= SH5_PTEL_PR_R;

	if (pg) {
		/* Only managed pages can be user-accessible */
		if (va <= VM_MAXUSER_ADDRESS)
			ptel |= SH5_PTEL_PR_U;

		/*
		 * Pre-load mod/ref status according to the hint in `flags'.
		 *
		 * Note that managed pages are initially read-only, unless
		 * the hint indicates they are writable. This allows us to
		 * track page modification status by taking a write-protect
		 * fault later on.
		 */
		if (flags & VM_PROT_WRITE)
			ptel |= SH5_PTEL_R | SH5_PTEL_M | SH5_PTEL_PR_W;
		else
		if (flags & VM_PROT_ALL)
			ptel |= SH5_PTEL_R;
	} else
	if (prot & VM_PROT_WRITE) {
		/* Unmanaged pages are writeable from the start.  */
		ptel |= SH5_PTEL_PR_W;
	}

	if (prot & VM_PROT_EXECUTE)
		ptel |= SH5_PTEL_PR_X;

	flags |= (prot & (VM_PROT_EXECUTE | VM_PROT_WRITE));

	s = splvm();
	if (pg)
		pmap_attr_save(pg, ptel & (SH5_PTEL_R | SH5_PTEL_M));
	error = pmap_pvo_enter(pm, pvo_head, va, pa, ptel, flags);

	splx(s);

	return (error);
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot)
{
	struct mem_region *mp;
	ptel_t ptel;
	int idx;

	if (va < pmap_kva_avail_start)
		panic("pmap_kenter_pa: Entering non-kernel VA: 0x%lx", va);

	idx = kva_to_iptidx(va);

	ptel = SH5_PTEL_CB_DEVICE;
	for (mp = mem; mp->mr_size; mp++) {
		if (pa >= mp->mr_start && pa < mp->mr_start + mp->mr_size) {
			ptel = SH5_PTEL_CB_WRITEBACK;
			break;
		}
	}

	ptel |= SH5_PTEL_PR_R | SH5_PTEL_PR_W | (pa & SH5_PTEL_PPN_MASK);

	PMPRINTF((
	    "pmap_kenter_pa: va 0x%lx pa 0x%lx prot 0x%x ptel 0x%x idx %d\n",
	    va, pa, (u_int)prot, ptel, idx));

	KDASSERT(pmap_kernel_ipt[idx] == 0);

	pmap_kernel_ipt[idx] = ptel;
}

void
pmap_kremove(vaddr_t va, vsize_t len)
{

	if (va < pmap_kva_avail_start)
		panic("pmap_kremove: Entering non-kernel VA: 0x%lx", va);

	PMPRINTF(("pmap_kremove: va=0x%lx, len=0x%lx\n", va, len));

	pmap_remove(pmap_kernel(), va, va + len);
}

/*
 * Remove the given range of mapping entries.
 */
void
pmap_remove(pmap_t pm, vaddr_t va, vaddr_t endva)
{
	struct pvo_entry *pvo;
	int idx;
	int s;

	PMPRINTF(("pmap_remove: %p: va=0x%lx, endva=0x%lx\n", pm, va, endva));

#ifdef PMAP_DIAG
	if (pm == pmap_kernel() && va < SH5_KSEG1_BASE) {
		printf("pmap_remove: pmap_kernel() with va 0x%08x!!\n",
		    (u_int)va);
		pmap_debugger();
	}
#endif

	for (; va < endva; va += PAGE_SIZE) {
		s = splhigh();
		pvo = pmap_pvo_find_va(pm, va, &idx);
		if (pvo != NULL)
			pmap_pvo_remove(pvo, idx);
		else
		if (pm == pmap_kernel() && (idx = kva_to_iptidx(va)) >= 0) {
			if (pmap_kernel_ipt[idx])
		    		pmap_pa_unmap_kva(va, &pmap_kernel_ipt[idx]);
		}
		splx(s);
	}
}

/*
 * Get the physical page address for the given pmap/virtual address.
 */
boolean_t
pmap_extract(pmap_t pm, vaddr_t va, paddr_t *pap)
{
	struct pvo_entry *pvo;
	int s, idx;
	boolean_t found = FALSE;

	PMPRINTF(("pmap_extract: %p: va 0x%lx - ", pm, va));

	/*
	 * We can be called from the bus_dma code for addresses in KSEG0.
	 * This would be the case, for example, of bus_dmamap_load() being
	 * called for a buffer allocated by pool(9).
	 */
	if (pm == pmap_kernel() &&
	    va < SH5_KSEG1_BASE && va >= SH5_KSEG0_BASE) {
		struct mem_region *mr;
		for (mr = mem; mr->mr_size; mr++) {
			if (va >= mr->mr_kvastart &&
			    va < (mr->mr_kvastart + mr->mr_size)) {
				*pap = mr->mr_start +
				    (paddr_t)(va - mr->mr_kvastart);
				return (TRUE);
			}
		}

		return (FALSE);		/* Should probably panic here ... */
	}

	s = splhigh();
	pvo = pmap_pvo_find_va(pm, va, NULL);
	if (pvo != NULL) {
		*pap = (pvo->pvo_ptel & SH5_PTEL_PPN_MASK) |
		    (va & ~SH5_PTEH_EPN_MASK);
		found = TRUE;
		PMPRINTF(("managed pvo. pa 0x%lx\n", *pap));
	} else
	if (pm == pmap_kernel()) {
		idx = kva_to_iptidx(va);
		if (idx >= 0 && pmap_kernel_ipt[idx]) {
			*pap = (pmap_kernel_ipt[idx] & SH5_PTEL_PPN_MASK) |
			    (va & ~SH5_PTEH_EPN_MASK);
			found = TRUE;
			PMPRINTF(("no pvo, but kipt pa 0x%lx\n", *pap));
		}
	}
	splx(s);

#ifdef DEBUG
	if (!found)
		PMPRINTF(("not found.\n"));
#endif

	return (found);
}

/*
 * Lower the protection on the specified range of this pmap.
 *
 * There are only two cases: either the protection is going to 0,
 * or it is going to read-only.
 */
void
pmap_protect(pmap_t pm, vaddr_t va, vaddr_t endva, vm_prot_t prot)
{
	struct pvo_entry *pvo;
	volatile pte_t *pt;
	ptel_t clrbits;
	int s, idx;

	PMPRINTF(("pmap_protect: %p: va=0x%lx, endva=0x%lx, prot=0x%x\n",
	    pm, va, endva, (u_int)prot));

#ifdef PMAP_DIAG
	if (pm == pmap_kernel() && va < SH5_KSEG1_BASE) {
		printf("pmap_protect: pmap_kernel() with va 0x%lx!!\n", va);
		pmap_debugger();
	}
#endif

	/*
	 * If there is no protection, this is equivalent to
	 * removing the VA range from the pmap.
	 */
	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pm, va, endva);
		PMPRINTF(("pmap_protect: done removing mapping\n"));
		return;
	}

	if ((prot & VM_PROT_EXECUTE) == 0)
		clrbits = SH5_PTEL_PR_W | SH5_PTEL_PR_X;
	else
		clrbits = SH5_PTEL_PR_W;

	/*
	 * We're doing a write-protect, and perhaps an execute-revoke.
	 */

	for (; va < endva; va += NBPG) {
		s = splhigh();
		pvo = pmap_pvo_find_va(pm, va, &idx);
		if (pvo == NULL) {
			splx(s);
			continue;
		}

		pvo->pvo_ptel &= ~clrbits;
		pvo->pvo_vaddr &= ~PVO_WRITABLE;

		/*
		 * Since the mapping is being made readonly,
		 * if there are other mappings for the same
		 * physical page, we may now be able to
		 * make them cacheable.
		 */
		pmap_remove_update(
		    pa_to_pvoh((paddr_t)(pvo->pvo_ptel & SH5_PTEL_PPN_MASK),
				NULL),
		    0);

		/*
		 * Propagate the change to the page-table/TLB
		 */
		if (PVO_VADDR(pvo) < SH5_KSEG1_BASE) {
			KDASSERT(PVO_VADDR(pvo) < SH5_KSEG0_BASE);
			pt = pmap_pvo_to_pte(pvo, idx);
			if (pt != NULL)
				pmap_pteg_change(pt, pvo);
		} else {
			KDASSERT(idx >= 0);
			if (pmap_kernel_ipt[idx] & clrbits)
				pmap_kpte_clear_bit(idx, pvo, clrbits);
		}

		splx(s);
	}

	PMPRINTF(("pmap_protect: done lowering protection.\n"));
}

void
pmap_unwire(pmap_t pm, vaddr_t va)
{
	struct pvo_entry *pvo;
	int s;

	PMPRINTF(("pmap_unwire: %p: va 0x%lx\n", pm, va));

#ifdef PMAP_DIAG
	if (pm == pmap_kernel() && va < SH5_KSEG1_BASE) {
		printf("pmap_unwire: pmap_kernel() with va 0x%lx!!\n", va);
		pmap_debugger();
	}
#endif

	s = splhigh();

	pvo = pmap_pvo_find_va(pm, va, NULL);
	if (pvo != NULL) {
		if (PVO_ISWIRED(pvo)) {
			pvo->pvo_vaddr &= ~PVO_WIRED;
			pm->pm_stats.wired_count--;
		}
	}

	splx(s);
}

/*
 * Lower the protection on the specified physical page.
 *
 * There are only two cases: either the protection is going to 0,
 * or it is going to read-only.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	struct pvo_head *pvo_head;
	struct pvo_entry *pvo, *next_pvo;
	volatile pte_t *pt;
	ptel_t clrbits;
	int idx, s;

	PMPRINTF(("pmap_page_protect: %p prot=%x: ", pg, prot));

	/*
	 * Since this routine only downgrades protection, if the
	 * maximal protection is desired, there isn't any change
	 * to be made.
	 */
	if ((prot & (VM_PROT_READ|VM_PROT_WRITE)) ==
	    (VM_PROT_READ|VM_PROT_WRITE)) {
		PMPRINTF(("maximum requested. all done.\n"));
		return;
	}

	if ((prot & VM_PROT_EXECUTE) == 0)
		clrbits = SH5_PTEL_PR_W | SH5_PTEL_PR_X;
	else
		clrbits = SH5_PTEL_PR_W;

	s = splhigh();

	pvo_head = vm_page_to_pvoh(pg);
	for (pvo = LIST_FIRST(pvo_head); pvo != NULL; pvo = next_pvo) {
		next_pvo = LIST_NEXT(pvo, pvo_vlink);

		/*
		 * Downgrading to no mapping at all, we just remove the entry.
		 */
		if ((prot & VM_PROT_READ) == 0) {
			pmap_pvo_remove(pvo, -1);
			continue;
		} 

		pvo->pvo_ptel &= ~clrbits;
		pvo->pvo_vaddr &= ~PVO_WRITABLE;

		/*
		 * Since the mapping is being made readonly,
		 * if there are other mappings for the same
		 * physical page, we may now be able to
		 * make them cacheable.
		 */
		pmap_remove_update(pvo_head, 0);

		/*
		 * Propagate the change to the page-table/TLB
		 */
		if (PVO_VADDR(pvo) < SH5_KSEG1_BASE) {
			KDASSERT(PVO_VADDR(pvo) < SH5_KSEG0_BASE);
			pt = pmap_pvo_to_pte(pvo, -1);
			if (pt != NULL)
				pmap_pteg_change(pt, pvo);
		} else {
			idx = kva_to_iptidx(PVO_VADDR(pvo));
			KDASSERT(idx >= 0);
			if (pmap_kernel_ipt[idx] & clrbits)
				pmap_kpte_clear_bit(idx, pvo, clrbits);
		}
	}

	splx(s);

	PMPRINTF(("all done.\n"));
}

/*
 * Activate the address space for the specified process.  If the process
 * is the current process, load the new MMU context.
 */
void
pmap_activate(struct proc *p)
{
	pmap_t pm = p->p_vmspace->vm_map.pmap;
	vsid_t old_vsid;

	pmap_asid_alloc(pm);

	if (p == curproc) {
		old_vsid = curcpu()->ci_curvsid;
		curcpu()->ci_curvsid = pm->pm_vsid;
		sh5_setasid(pm->pm_asid);

		/*
		 * If the CPU has to invalidate the instruction cache
		 * on a context switch, do it now. But only if we're
		 * not resuming in the same pmap context.
		 */
		if (__cpu_cache_iinv_all && old_vsid != pm->pm_vsid)
			__cpu_cache_iinv_all();
	}
}


/*
 * Deactivate the specified process's address space.
 */
void
pmap_deactivate(struct proc *p)
{
}

boolean_t
pmap_query_bit(struct vm_page *pg, ptel_t ptebit)
{
	struct pvo_entry *pvo;
	volatile pte_t *pt;
	int s, idx;

	PMPRINTF(("pmap_query_bit: pa 0x%0lx %s? - ", pg->phys_addr,
	    (ptebit == SH5_PTEL_M) ? "modified" : "referenced"));

	if ((ptel_t)pmap_attr_fetch(pg) & ptebit) {
		PMPRINTF(("yes. Cached in pg attr.\n"));
		return (TRUE);
	}

	s = splhigh();

	LIST_FOREACH(pvo, vm_page_to_pvoh(pg), pvo_vlink) {
		/*
		 * See if we saved the bit off.  If so cache, it and return
		 * success.
		 */
		if (pvo->pvo_ptel & ptebit) {
			pmap_attr_save(pg, ptebit);
			splx(s);
			PMPRINTF(("yes. Cached in pvo for 0x%lx.\n",
			    PVO_VADDR(pvo)));
			return (TRUE);
		}
	}

	/*
	 * No luck, now go thru the hard part of looking at the ptes
	 * themselves.  Sync so any pending REF/CHG bits are flushed
	 * to the PTEs.
	 */
	LIST_FOREACH(pvo, vm_page_to_pvoh(pg), pvo_vlink) {
		if (PVO_VADDR(pvo) < SH5_KSEG1_BASE) {
			/*
			 * See if this pvo have a valid PTE.  If so, fetch the
			 * REF/CHG bits from the valid PTE.  If the appropriate
			 * ptebit is set, cache, it and return success.
			 */
			KDASSERT(PVO_VADDR(pvo) < SH5_KSEG0_BASE);
			pt = pmap_pvo_to_pte(pvo, -1);
			if (pt != NULL) {
				pmap_pteg_synch(pt->ptel, pvo);
				if (pvo->pvo_ptel & ptebit) {
					pmap_attr_save(pg, ptebit);
					splx(s);
					PMPRINTF((
					    "yes. Cached in ptel for 0x%lx.\n",
					    PVO_VADDR(pvo)));
					return (TRUE);
				}
			}
		} else {
			idx = kva_to_iptidx(PVO_VADDR(pvo));
			KDASSERT(idx >= 0);
			if (pmap_kernel_ipt[idx] & ptebit) {
				pmap_pteg_synch(pmap_kernel_ipt[idx], pvo);
				pmap_attr_save(pg, ptebit);
				splx(s);
				PMPRINTF(("yes. Cached in kipt for 0x%lx.\n",
				    PVO_VADDR(pvo)));
				return (TRUE);
			}
		}
	}

	splx(s);

	PMPRINTF(("no.\n"));

	return (FALSE);
}

boolean_t
pmap_clear_bit(struct vm_page *pg, ptel_t ptebit)
{
	struct pvo_entry *pvo;
	volatile pte_t *pt;
	int rv = 0;
	int s, idx;

	PMPRINTF(("pmap_clear_bit: pa 0x%lx %s - ", pg->phys_addr,
	    (ptebit == SH5_PTEL_M) ? "modified" : "referenced"));

	s = splhigh();

	/*
	 * Clear the cached value.
	 */
	rv |= pmap_attr_fetch(pg);
	pmap_attr_clear(pg, ptebit);

	/*
	 * If clearing the Modified bit, make sure to clear the
	 * writable bit in the PTEL/TLB so we can track future
	 * modifications to the page via pmap_write_trap().
	 */
	if (ptebit & SH5_PTEL_M)
		ptebit |= SH5_PTEL_PR_W;

	/*
	 * For each pvo entry, clear pvo's ptebit.  If this pvo has a
	 * valid PTE.  If so, clear the ptebit from the valid PTE.
	 */
	LIST_FOREACH(pvo, vm_page_to_pvoh(pg), pvo_vlink) {
		if (PVO_VADDR(pvo) < SH5_KSEG1_BASE) {
			KDASSERT(PVO_VADDR(pvo) < SH5_KSEG0_BASE);
			pt = pmap_pvo_to_pte(pvo, -1);
			if (pt != NULL) {
				if ((pvo->pvo_ptel & ptebit) == 0)
					pmap_pteg_synch(pt->ptel, pvo);
				if (pvo->pvo_ptel & ptebit)
					pmap_pteg_clear_bit(pt, pvo, ptebit);
			}
		} else {
			idx = kva_to_iptidx(PVO_VADDR(pvo));
			KDASSERT(idx >= 0);
			if (pmap_kernel_ipt[idx] != 0) {
				if ((pvo->pvo_ptel & ptebit) == 0)
					pmap_pteg_synch(pmap_kernel_ipt[idx],
					    pvo);
				if (pvo->pvo_ptel & ptebit)
					pmap_kpte_clear_bit(idx, pvo, ptebit);
			}
		}

		rv |= pvo->pvo_ptel & ptebit;
		pvo->pvo_ptel &= ~ptebit;
	}

	splx(s);

	ptebit &= ~SH5_PTEL_PR_W;
	PMPRINTF(("was %s.\n", (rv & ptebit) ? "cleared" : "not set"));

	return ((rv & ptebit) != 0);
}

static void
pmap_asid_alloc(pmap_t pm)
{
	int s;

	if (pm == pmap_kernel())
		return;

	if (pm->pm_asid == PMAP_ASID_UNASSIGNED ||
	    pm->pm_asidgen != pmap_asid_generation) {
		s = splhigh();
		if (pmap_asid_next > pmap_asid_max) {
			PMPRINTF(("pmap_asid_alloc: wrapping ASIDs\n"));

			/*
			 * Note: Current SH5 cpus do not use the ASID when
			 * matching cache tags (even though they keep a copy
			 * of the ASID which allocated each cacheline, in
			 * the tag itself).
			 *
			 * Therefore, we don't need to purge the cache since
			 * the virtual to physical mappings themselves haven't
			 * changed. So the cache contents are still valid.
			 *
			 * We do need to invalidate the TLB, however.
			 */
			__cpu_tlbinv_all();

			pmap_asid_generation++;
			pmap_asid_next = PMAP_ASID_USER_START;
		}

		pm->pm_asid = pmap_asid_next++;
		pm->pm_asidgen = pmap_asid_generation;
		splx(s);
	}
}

/*
 * Called from trap() when a write-protection trap occurs.
 * Return non-zero if we made the page writable so that the faulting
 * instruction can be restarted.
 * Otherwise, the page really is read-only.
 */
int
pmap_write_trap(int usermode, vaddr_t va)
{
	struct pvo_entry *pvo;
	volatile pte_t *pt;
	pmap_t pm;
	int s, idx;

	if (curproc)
		pm = curproc->p_vmspace->vm_map.pmap;
	else {
		KDASSERT(usermode == 0);
		pm = pmap_kernel();
	}

	PMPRINTF(("pmap_write_trap: %p: %smode va 0x%lx - ",
	    pm, usermode ? "user" : "kernel", va));

	s = splhigh();
	pvo = pmap_pvo_find_va(pm, va, &idx);
	if (pvo == NULL) {
		splx(s);
		PMPRINTF(("page has no pvo.\n"));
		return (0);
	}

	if (!PVO_ISWRITABLE(pvo)) {
		splx(s);
		PMPRINTF(("page is read-only.\n"));
		return (0);
	}

	pvo->pvo_ptel |= SH5_PTEL_PR_W | SH5_PTEL_R | SH5_PTEL_M;
	pvo->pvo_vaddr |= PVO_MODIFIED | PVO_REFERENCED;

	if (PVO_VADDR(pvo) < SH5_KSEG1_BASE) {
		KDASSERT(PVO_VADDR(pvo) < SH5_KSEG0_BASE);
		pt = pmap_pvo_to_pte(pvo, idx);
		if (pt != NULL)
			pmap_pteg_change(pt, pvo);
	} else {
		KDASSERT(idx >= 0);
		/*
		 * XXX: Technically, we shouldn't need to purge the dcache
		 * here as we're lowering the protection of this page
		 * (enabling write).
		 *
		 * See SH-5 Volume 1: Architecture, Section 17.8.3, Point 5.
		 *
		 * However, since we're also going to invalidate the TLB
		 * entry, we're technically *increasing* the protection of
		 * the page, albeit temporarily, so the purge is required.
		 *
		 * If we were able to poke the new PTEL right into the same
		 * TLB slot at this point, we could avoid messing with the
		 * cache (assuming the entry hasn't been evicted by some
		 * other TLB miss in the meantime).
		 */
		if ((pvo->pvo_ptel & SH5_PTEL_CB_MASK) > SH5_PTEL_CB_NOCACHE) {
			__cpu_cache_dpurge(PVO_VADDR(pvo),
			    (paddr_t)(pmap_kernel_ipt[idx] & SH5_PTEL_PPN_MASK),
			    NBPG);
		}

		pmap_kernel_ipt[idx] = pvo->pvo_ptel;
		__cpu_tlbinv(PVO_VADDR(pvo) | SH5_PTEH_SH,
		    SH5_PTEH_EPN_MASK | SH5_PTEH_SH);
	}

	splx(s);

	PMPRINTF((" page made writable.\n"));

	return (1);
}

vaddr_t
pmap_map_poolpage(paddr_t pa)
{
	struct mem_region *mp;

	for (mp = mem; mp->mr_size; mp++) {
		if (pa >= mp->mr_start && pa < (mp->mr_start + mp->mr_size))
			break;
	}

	if (mp->mr_size && mp->mr_kvastart < SH5_KSEG1_BASE)
		return (mp->mr_kvastart + (vaddr_t)(pa - mp->mr_start));

	/*
	 * We should really be allowed to fail here to force allocation
	 * using the default method.
	 */
	panic("pmap_map_poolpage: non-kseg0 page: pa = 0x%lx", pa);
	return (0);
}

paddr_t
pmap_unmap_poolpage(vaddr_t va)
{
	struct mem_region *mp;
	paddr_t pa;

	for (mp = mem; mp->mr_size; mp++) {
		if (va >= mp->mr_kvastart &&
		    va < (mp->mr_kvastart + mp->mr_size))
			break;
	}

	if (mp->mr_size && mp->mr_kvastart < SH5_KSEG1_BASE) {
		pa = mp->mr_start + (paddr_t)(va - mp->mr_kvastart);
		__cpu_cache_dpurge(va, pa, NBPG);
		return (pa);
	}

	/*
	 * We should really be allowed to fail here
	 */
	panic("pmap_unmap_poolpage: non-kseg0 page: va = 0x%lx", va);
	return (0);
}

#ifdef DDB
void dump_kipt(void);
void
dump_kipt(void)
{
	vaddr_t va;
	ptel_t *pt;

	printf("\nKernel KSEG1 mappings:\n\n");

	for (pt = &pmap_kernel_ipt[0], va = SH5_KSEG1_BASE;
	    pt != &pmap_kernel_ipt[KERNEL_IPT_SIZE]; pt++, va += NBPG) {
		if (*pt && *pt < 0x80000000u)
			printf("KVA: 0x%lx -> PTEL: 0x%lx\n", va, (u_long)*pt);
	}

	printf("\n");
}
#endif

#if defined(PMAP_DIAG) && defined(DDB)
int
validate_kipt(int cookie)
{
	ptel_t *ptp, pt;
	vaddr_t va;
	paddr_t pa;
	int errors = 0;

	for (ptp = &pmap_kernel_ipt[0], va = SH5_KSEG1_BASE;
	    ptp != &pmap_kernel_ipt[KERNEL_IPT_SIZE]; ptp++, va += NBPG) {
		if ((pt = *ptp) == 0)
			continue;

		pa = (paddr_t)(pt & SH5_PTEL_PPN_MASK);

		if (pt & 0x24) {
			printf("kva 0x%lx has reserved bits set: 0x%lx\n",
			    va, (u_long)pt);
			errors++;
		}

		switch (pt & SH5_PTEL_CB_MASK) {
		case SH5_PTEL_CB_NOCACHE:
		case SH5_PTEL_CB_WRITEBACK:
		case SH5_PTEL_CB_WRITETHRU:
			if (pa < 0x80000000ul || pa >= 0x88000000) {
				printf("kva 0x%lx has bad DRAM pa: 0x%lx\n",
				    va, (u_long)pt);
				errors++;
			}
			break;

		case SH5_PTEL_CB_DEVICE:
			if (pa >= 0x80000000) {
				printf("kva 0x%lx has bad device pa: 0x%lx\n",
				    va, (u_long)pt);
				errors++;
			}
			break;
		}

		if ((pt & SH5_PTEL_SZ_MASK) != SH5_PTEL_SZ_4KB) {
			printf("kva 0x%lx has bad page size: 0x%lx\n",
			    va, (u_long)pt);
			errors++;
		}

		if (pt & SH5_PTEL_PR_U) {
			printf("kva 0x%lx has usermode bit set: 0x%lx\n",
			    va, (u_long)pt);
			errors++;
		}

		switch (pt & (SH5_PTEL_PR_R|SH5_PTEL_PR_X|SH5_PTEL_PR_W)) {
		case SH5_PTEL_PR_R:
		case SH5_PTEL_PR_R|SH5_PTEL_PR_W:
		case SH5_PTEL_PR_R|SH5_PTEL_PR_W|SH5_PTEL_PR_X:
			break;

		case SH5_PTEL_PR_W:
			printf("kva 0x%lx has only write bit set: 0x%lx\n",
			    va, (u_long)pt);
			errors++;
			break;

		case SH5_PTEL_PR_X:
			printf("kva 0x%lx has only execute bit set: 0x%lx\n",
			    va, (u_long)pt);
			errors++;
			break;

		case SH5_PTEL_PR_W|SH5_PTEL_PR_X:
			printf("kva 0x%lx is not readable: 0x%lx\n",
			    va, (u_long)pt);
			errors++;
			break;

		default:
			printf("kva 0x%lx has no protection bits set: 0x%lx\n",
			    va, (u_long)pt);
			errors++;
			break;
		}
	}

	if (errors)
		printf("Reference cookie: 0x%x\n", cookie);
	return (errors);
}
#endif
