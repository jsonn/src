/*	$NetBSD: pmap.c,v 1.72.2.1.2.2 1999/08/02 19:46:11 thorpej Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	@(#)pmap.c	8.6 (Berkeley) 5/27/94
 */

/*
 * HP9000/300 series physical map management code.
 *
 * Supports:
 *	68020 with HP MMU	models 320, 350
 *	68020 with 68551 MMU	models 318, 319, 330
 *	68030 with on-chip MMU	models 340, 360, 370, 345, 375, 400
 *	68040 with on-chip MMU	models 380, 425, 433
 *
 * Notes:
 *	Don't even pay lip service to multiprocessor support.
 *
 *	We assume TLB entries don't have process tags (except for the
 *	supervisor/user distinction) so we only invalidate TLB entries
 *	when changing mappings for the current (or kernel) pmap.  This is
 *	technically not true for the 68551 but we flush the TLB on every
 *	context switch, so it effectively winds up that way.
 *
 *	Bitwise and/or operations are significantly faster than bitfield
 *	references so we use them when accessing STE/PTEs in the pmap_pte_*
 *	macros.  Note also that the two are not always equivalent; e.g.:
 *		(*pte & PG_PROT) [4] != pte->pg_prot [1]
 *	and a couple of routines that deal with protection and wiring take
 *	some shortcuts that assume the and/or definitions.
 *
 *	This implementation will only work for PAGE_SIZE == NBPG
 *	(i.e. 4096 bytes).
 */

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

#include "opt_compat_hpux.h"

#include <machine/hp300spu.h>	/* XXX param.h includes cpu.h */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/user.h>
#include <sys/pool.h>

#include <machine/pte.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <uvm/uvm.h>

#include <machine/cpu.h>

#ifdef DEBUG
#define PDB_FOLLOW	0x0001
#define PDB_INIT	0x0002
#define PDB_ENTER	0x0004
#define PDB_REMOVE	0x0008
#define PDB_CREATE	0x0010
#define PDB_PTPAGE	0x0020
#define PDB_CACHE	0x0040
#define PDB_BITS	0x0080
#define PDB_COLLECT	0x0100
#define PDB_PROTECT	0x0200
#define PDB_SEGTAB	0x0400
#define PDB_MULTIMAP	0x0800
#define PDB_PARANOIA	0x2000
#define PDB_WIRING	0x4000
#define PDB_PVDUMP	0x8000

int debugmap = 0;
int pmapdebug = PDB_PARANOIA;

#define	PMAP_DPRINTF(l, x)	if (pmapdebug & (l)) printf x

#if defined(M68040)
int dowriteback = 1;	/* 68040: enable writeback caching */
int dokwriteback = 1;	/* 68040: enable writeback caching of kernel AS */
#endif
#else /* ! DEBUG */
#define	PMAP_DPRINTF(l, x)	/* nothing */
#endif /* DEBUG */

/*
 * Get STEs and PTEs for user/kernel address space
 */
#if defined(M68040)
#define	pmap_ste1(m, v)	\
	(&((m)->pm_stab[(vaddr_t)(v) >> SG4_SHIFT1]))
/* XXX assumes physically contiguous ST pages (if more than one) */
#define pmap_ste2(m, v) \
	(&((m)->pm_stab[(st_entry_t *)(*(u_int *)pmap_ste1(m, v) & SG4_ADDR1) \
			- (m)->pm_stpa + (((v) & SG4_MASK2) >> SG4_SHIFT2)]))
#define	pmap_ste(m, v)	\
	(&((m)->pm_stab[(vaddr_t)(v) \
			>> (mmutype == MMU_68040 ? SG4_SHIFT1 : SG_ISHIFT)]))
#define pmap_ste_v(m, v) \
	(mmutype == MMU_68040 \
	 ? ((*pmap_ste1(m, v) & SG_V) && \
	    (*pmap_ste2(m, v) & SG_V)) \
	 : (*pmap_ste(m, v) & SG_V))
#else
#define	pmap_ste(m, v)	 (&((m)->pm_stab[(vaddr_t)(v) >> SG_ISHIFT]))
#define pmap_ste_v(m, v) (*pmap_ste(m, v) & SG_V)
#endif

#define pmap_pte(m, v)	(&((m)->pm_ptab[(vaddr_t)(v) >> PG_SHIFT]))
#define pmap_pte_pa(pte)	(*(pte) & PG_FRAME)
#define pmap_pte_w(pte)		(*(pte) & PG_W)
#define pmap_pte_ci(pte)	(*(pte) & PG_CI)
#define pmap_pte_m(pte)		(*(pte) & PG_M)
#define pmap_pte_u(pte)		(*(pte) & PG_U)
#define pmap_pte_prot(pte)	(*(pte) & PG_PROT)
#define pmap_pte_v(pte)		(*(pte) & PG_V)

#define pmap_pte_set_w(pte, v) \
	if (v) *(pte) |= PG_W; else *(pte) &= ~PG_W
#define pmap_pte_set_prot(pte, v) \
	if (v) *(pte) |= PG_PROT; else *(pte) &= ~PG_PROT
#define pmap_pte_w_chg(pte, nw)		((nw) ^ pmap_pte_w(pte))
#define pmap_pte_prot_chg(pte, np)	((np) ^ pmap_pte_prot(pte))

/*
 * Given a map and a machine independent protection code,
 * convert to an hp300 protection code.
 */
#define pte_prot(m, p)	(protection_codes[p])
int	protection_codes[8];

/*
 * Kernel page table page management.
 */
struct kpt_page {
	struct kpt_page *kpt_next;	/* link on either used or free list */
	vaddr_t		kpt_va;		/* always valid kernel VA */
	paddr_t		kpt_pa;		/* PA of this page (for speed) */
};
struct kpt_page *kpt_free_list, *kpt_used_list;
struct kpt_page *kpt_pages;

/*
 * Kernel segment/page table and page table map.
 * The page table map gives us a level of indirection we need to dynamically
 * expand the page table.  It is essentially a copy of the segment table
 * with PTEs instead of STEs.  All are initialized in locore at boot time.
 * Sysmap will initially contain VM_KERNEL_PT_PAGES pages of PTEs.
 * Segtabzero is an empty segment table which all processes share til they
 * reference something.
 */
st_entry_t	*Sysseg;
pt_entry_t	*Sysmap, *Sysptmap;
st_entry_t	*Segtabzero, *Segtabzeropa;
vsize_t		Sysptsize = VM_KERNEL_PT_PAGES;

struct pmap	kernel_pmap_store;
vm_map_t	st_map, pt_map;
struct vm_map	st_map_store, pt_map_store;

paddr_t		avail_start;	/* PA of first available physical page */
paddr_t		avail_end;	/* PA of last available physical page */
vsize_t		mem_size;	/* memory size in bytes */
vaddr_t		virtual_avail;  /* VA of first avail page (after kernel bss)*/
vaddr_t		virtual_end;	/* VA of last avail page (end of kernel AS) */
int		page_cnt;	/* number of pages managed by VM system */

boolean_t	pmap_initialized = FALSE;	/* Has pmap_init completed? */
struct pv_entry	*pv_table;
char		*pmap_attributes;	/* reference and modify bits */
TAILQ_HEAD(pv_page_list, pv_page) pv_page_freelist;
int		pv_nfree;

#ifdef M68K_MMU_HP
int		pmap_aliasmask;	/* seperation at which VA aliasing ok */
#endif
#if defined(M68040)
int		protostfree;	/* prototype (default) free ST map */
#endif

extern caddr_t	CADDR1, CADDR2;

pt_entry_t	*caddr1_pte;	/* PTE for CADDR1 */
pt_entry_t	*caddr2_pte;	/* PTE for CADDR2 */

struct pool	pmap_pmap_pool;	/* memory pool for pmap structures */

struct pv_entry *pmap_alloc_pv __P((void));
void	pmap_free_pv __P((struct pv_entry *));
void	pmap_collect_pv __P((void));
#ifdef COMPAT_HPUX
int	pmap_mapmulti __P((pmap_t, vaddr_t));
#endif /* COMPAT_HPUX */

#define	PAGE_IS_MANAGED(pa)	(pmap_initialized &&			\
				 vm_physseg_find(atop((pa)), NULL) != -1)

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
	&vm_physmem[bank_].pmseg.attrs[pg_];				\
})

/*
 * Internal routines
 */
void	pmap_remove_mapping __P((pmap_t, vaddr_t, pt_entry_t *, int));
boolean_t pmap_testbit	__P((paddr_t, int));
void	pmap_changebit	__P((paddr_t, int, int));
void	pmap_enter_ptpage	__P((pmap_t, vaddr_t));
void	pmap_ptpage_addref __P((vaddr_t));
int	pmap_ptpage_delref __P((vaddr_t));
void	pmap_collect1	__P((pmap_t, paddr_t, paddr_t));
void	pmap_pinit __P((pmap_t));
void	pmap_release __P((pmap_t));

#ifdef DEBUG
void pmap_pvdump	__P((paddr_t));
void pmap_check_wiring	__P((char *, vaddr_t));
#endif

/* pmap_remove_mapping flags */
#define	PRM_TFLUSH	0x01
#define	PRM_CFLUSH	0x02
#define	PRM_KEEPPTPAGE	0x04

/*
 * pmap_virtual_space:		[ INTERFACE ]
 *
 *	Report the range of available kernel virtual address
 *	space to the VM system during bootstrap.
 *
 *	This is only an interface function if we do not use
 *	pmap_steal_memory()!
 *
 *	Note: no locking is necessary in this function.
 */
void
pmap_virtual_space(vstartp, vendp)
	vaddr_t	*vstartp, *vendp;
{

	*vstartp = virtual_avail;
	*vendp = virtual_end;
}

/*
 * pmap_procwr:			[ INTERFACE ]
 * 
 *	Synchronize caches corresponding to [addr, addr+len) in p.
 *
 *	Note: no locking is necessary in this function.
 */   
void
pmap_procwr(p, va, len)
	struct proc *p;
	vaddr_t va;
	u_long len;
{

	(void)cachectl1(0x80000004, va, len, p);
}

/*
 * pmap_init:			[ INTERFACE ]
 *
 *	Initialize the pmap module.  Called by vm_init(), to initialize any
 *	structures that the pmap system needs to map virtual memory.
 *
 *	Note: no locking is necessary in this function.
 */
void
pmap_init()
{
	vaddr_t		addr, addr2;
	vsize_t		s;
	struct pv_entry	*pv;
	char		*attr;
	int		rv;
	int		npages;
	int		bank;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_init()\n"));

	/*
	 * Before we do anything else, initialize the PTE pointers
	 * used by pmap_zero_page() and pmap_copy_page().
	 */
	caddr1_pte = pmap_pte(pmap_kernel(), CADDR1);
	caddr2_pte = pmap_pte(pmap_kernel(), CADDR2);

	/*
	 * Now that kernel map has been allocated, we can mark as
	 * unavailable regions which we have mapped in pmap_bootstrap().
	 */
	addr = (vaddr_t) intiobase;
	if (uvm_map(kernel_map, &addr,
		    m68k_ptob(IIOMAPSIZE+EIOMAPSIZE),
		    NULL, UVM_UNKNOWN_OFFSET,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE,
				UVM_INH_NONE, UVM_ADV_RANDOM,
				UVM_FLAG_FIXED)) != KERN_SUCCESS)
		goto bogons;
	addr = (vaddr_t) Sysmap;
	if (uvm_map(kernel_map, &addr, HP_MAX_PTSIZE,
		    NULL, UVM_UNKNOWN_OFFSET,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE,
				UVM_INH_NONE, UVM_ADV_RANDOM,
				UVM_FLAG_FIXED)) != KERN_SUCCESS) {
		/*
		 * If this fails, it is probably because the static
		 * portion of the kernel page table isn't big enough
		 * and we overran the page table map.
		 */
 bogons:
		panic("pmap_init: bogons in the VM system!\n");
	}

	PMAP_DPRINTF(PDB_INIT,
	    ("pmap_init: Sysseg %p, Sysmap %p, Sysptmap %p\n",
	    Sysseg, Sysmap, Sysptmap));
	PMAP_DPRINTF(PDB_INIT,
	    ("  pstart %lx, pend %lx, vstart %lx, vend %lx\n",
	    avail_start, avail_end, virtual_avail, virtual_end));

	/*
	 * Allocate memory for random pmap data structures.  Includes the
	 * initial segment table, pv_head_table and pmap_attributes.
	 */
	for (page_cnt = 0, bank = 0; bank < vm_nphysseg; bank++)
		page_cnt += vm_physmem[bank].end - vm_physmem[bank].start;
	s = HP_STSIZE;					/* Segtabzero */
	s += page_cnt * sizeof(struct pv_entry);	/* pv table */
	s += page_cnt * sizeof(char);			/* attribute table */
	s = round_page(s);
	addr = uvm_km_zalloc(kernel_map, s);
	if (addr == 0)
		panic("pmap_init: can't allocate data structures");

	Segtabzero = (st_entry_t *) addr;
	(void) pmap_extract(pmap_kernel(), addr, (paddr_t *)&Segtabzeropa);
	addr += HP_STSIZE;

	pv_table = (struct pv_entry *) addr;
	addr += page_cnt * sizeof(struct pv_entry);

	pmap_attributes = (char *) addr;

	PMAP_DPRINTF(PDB_INIT, ("pmap_init: %lx bytes: page_cnt %x s0 %p(%p) "
	    "tbl %p atr %p\n",
	    s, page_cnt, Segtabzero, Segtabzeropa,
	    pv_table, pmap_attributes));

	/*
	 * Now that the pv and attribute tables have been allocated,
	 * assign them to the memory segments.
	 */
	pv = pv_table;
	attr = pmap_attributes;
	for (bank = 0; bank < vm_nphysseg; bank++) {
		npages = vm_physmem[bank].end - vm_physmem[bank].start;
		vm_physmem[bank].pmseg.pvent = pv;
		vm_physmem[bank].pmseg.attrs = attr;
		pv += npages;
		attr += npages;
	}

	/*
	 * Allocate physical memory for kernel PT pages and their management.
	 * We need 1 PT page per possible task plus some slop.
	 */
	npages = min(atop(HP_MAX_KPTSIZE), maxproc+16);
	s = ptoa(npages) + round_page(npages * sizeof(struct kpt_page));

	/*
	 * Verify that space will be allocated in region for which
	 * we already have kernel PT pages.
	 */
	addr = 0;
	rv = uvm_map(kernel_map, &addr, s, NULL, UVM_UNKNOWN_OFFSET,
		     UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				 UVM_ADV_RANDOM, UVM_FLAG_NOMERGE));
	if (rv != KERN_SUCCESS || (addr + s) >= (vaddr_t)Sysmap)
		panic("pmap_init: kernel PT too small");
	rv = uvm_unmap(kernel_map, addr, addr + s);
	if (rv != KERN_SUCCESS)
		panic("pmap_init: uvm_unmap failed");

	/*
	 * Now allocate the space and link the pages together to
	 * form the KPT free list.
	 */
	addr = uvm_km_zalloc(kernel_map, s);
	if (addr == 0)
		panic("pmap_init: cannot allocate KPT free list");
	s = ptoa(npages);
	addr2 = addr + s;
	kpt_pages = &((struct kpt_page *)addr2)[npages];
	kpt_free_list = (struct kpt_page *) 0;
	do {
		addr2 -= NBPG;
		(--kpt_pages)->kpt_next = kpt_free_list;
		kpt_free_list = kpt_pages;
		kpt_pages->kpt_va = addr2;
		(void) pmap_extract(pmap_kernel(), addr2,
		    (paddr_t *)&kpt_pages->kpt_pa);
	} while (addr != addr2);

	PMAP_DPRINTF(PDB_INIT, ("pmap_init: KPT: %ld pages from %lx to %lx\n",
	    atop(s), addr, addr + s));

	/*
	 * Allocate the segment table map and the page table map.
	 */
	s = maxproc * HP_STSIZE;
	st_map = uvm_km_suballoc(kernel_map, &addr, &addr2, s, 0, FALSE,
	    &st_map_store);

	addr = HP_PTBASE;
	if ((HP_PTMAXSIZE / HP_MAX_PTSIZE) < maxproc) {
		s = HP_PTMAXSIZE;
		/*
		 * XXX We don't want to hang when we run out of
		 * page tables, so we lower maxproc so that fork()
		 * will fail instead.  Note that root could still raise
		 * this value via sysctl(2).
		 */
		maxproc = (HP_PTMAXSIZE / HP_MAX_PTSIZE);
	} else
		s = (maxproc * HP_MAX_PTSIZE);
	pt_map = uvm_km_suballoc(kernel_map, &addr, &addr2, s, VM_MAP_PAGEABLE,
	    TRUE, &pt_map_store);

#if defined(M68040)
	if (mmutype == MMU_68040) {
		protostfree = ~l2tobm(0);
		for (rv = MAXUL2SIZE; rv < sizeof(protostfree)*NBBY; rv++)
			protostfree &= ~l2tobm(rv);
	}
#endif

	/*
	 * Initialize the pmap pools.
	 */
	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    0, pool_page_alloc_nointr, pool_page_free_nointr, M_VMPMAP);

	/*
	 * Now it is safe to enable pv_table recording.
	 */
	pmap_initialized = TRUE;
}

/*
 * pmap_alloc_pv:
 *
 *	Allocate a pv_entry.
 */
struct pv_entry *
pmap_alloc_pv()
{
	struct pv_page *pvp;
	struct pv_entry *pv;
	int i;

	if (pv_nfree == 0) {
		pvp = (struct pv_page *)uvm_km_zalloc(kernel_map, NBPG);
		if (pvp == 0)
			panic("pmap_alloc_pv: uvm_km_zalloc() failed");
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
#endif
		pvp->pvp_pgi.pgi_freelist = pv->pv_next;
	}
	return pv;
}

/*
 * pmap_free_pv:
 *
 *	Free a pv_entry.
 */
void
pmap_free_pv(pv)
	struct pv_entry *pv;
{
	struct pv_page *pvp;

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
		uvm_km_free(kernel_map, (vaddr_t)pvp, NBPG);
		break;
	}
}

/*
 * pmap_collect_pv:
 *
 *	Perform compaction on the PV list, called via pmap_collect().
 */
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
			TAILQ_INSERT_TAIL(&pv_page_collectlist, pvp,
			    pvp_pgi.pgi_list);
			pv_nfree -= NPVPPG;
			pvp->pvp_pgi.pgi_nfree = -1;
		}
	}

	if (pv_page_collectlist.tqh_first == 0)
		return;

	for (ph = &pv_table[page_cnt - 1]; ph >= &pv_table[0]; ph--) {
		if (ph->pv_pmap == 0)
			continue;
		s = splimp();
		for (ppv = ph; (pv = ppv->pv_next) != 0; ) {
			pvp = (struct pv_page *) trunc_page(pv);
			if (pvp->pvp_pgi.pgi_nfree == -1) {
				pvp = pv_page_freelist.tqh_first;
				if (--pvp->pvp_pgi.pgi_nfree == 0) {
					TAILQ_REMOVE(&pv_page_freelist, pvp,
					    pvp_pgi.pgi_list);
				}
				npv = pvp->pvp_pgi.pgi_freelist;
#ifdef DIAGNOSTIC
				if (npv == 0)
					panic("pmap_collect_pv: pgi_nfree inconsistent");
#endif
				pvp->pvp_pgi.pgi_freelist = npv->pv_next;
				*npv = *pv;
				ppv->pv_next = npv;
				ppv = npv;
			} else
				ppv = pv;
		}
		splx(s);
	}

	for (pvp = pv_page_collectlist.tqh_first; pvp; pvp = npvp) {
		npvp = pvp->pvp_pgi.pgi_list.tqe_next;
		uvm_km_free(kernel_map, (vaddr_t)pvp, NBPG);
	}
}

/*
 * pmap_map:
 *
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 *
 *	Note: THIS FUNCTION IS DEPRECATED, AND SHOULD BE REMOVED!
 */
vaddr_t
pmap_map(va, spa, epa, prot)
	vaddr_t va;
	paddr_t spa, epa;
	int prot;
{

	PMAP_DPRINTF(PDB_FOLLOW,
	    ("pmap_map(%lx, %lx, %lx, %x)\n", va, spa, epa, prot));

	while (spa < epa) {
		pmap_enter(pmap_kernel(), va, spa, prot, FALSE, 0);
		va += NBPG;
		spa += NBPG;
	}
	return (va);
}

/*
 * pmap_create:			[ INTERFACE ]
 *
 *	Create and return a physical map.
 *
 *	Note: no locking is necessary in this function.
 */
pmap_t
pmap_create(size)
	vsize_t	size;
{
	pmap_t pmap;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_CREATE,
	    ("pmap_create(%lx)\n", size));

	/*
	 * Software use map does not need a pmap
	 */
	if (size)
		return (NULL);

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);

	bzero(pmap, sizeof(*pmap));
	pmap_pinit(pmap);
	return (pmap);
}

/*
 * pmap_pinit:
 *
 *	Initialize a preallocated and zeroed pmap structure.
 *
 *	Note: THIS FUNCTION SHOULD BE MOVED INTO pmap_create()!
 */
void
pmap_pinit(pmap)
	struct pmap *pmap;
{

	PMAP_DPRINTF(PDB_FOLLOW|PDB_CREATE,
	    ("pmap_pinit(%p)\n", pmap));

	/*
	 * No need to allocate page table space yet but we do need a
	 * valid segment table.  Initially, we point everyone at the
	 * "null" segment table.  On the first pmap_enter, a real
	 * segment table will be allocated.
	 */
	pmap->pm_stab = Segtabzero;
	pmap->pm_stpa = Segtabzeropa;
#if defined(M68040)
	if (mmutype == MMU_68040)
		pmap->pm_stfree = protostfree;
#endif
	pmap->pm_count = 1;
	simple_lock_init(&pmap->pm_lock);
}

/*
 * pmap_destroy:		[ INTERFACE ]
 *
 *	Drop the reference count on the specified pmap, releasing
 *	all resources if the reference count drops to zero.
 */
void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int count;

	if (pmap == NULL)
		return;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_destroy(%p)\n", pmap));

	simple_lock(&pmap->pm_lock);
	count = --pmap->pm_count;
	simple_unlock(&pmap->pm_lock);
	if (count == 0) {
		pmap_release(pmap);
		pool_put(&pmap_pmap_pool, pmap);
	}
}

/*
 * pmap_release:
 *
 *	Relese the resources held by a pmap.
 *
 *	Note: THIS FUNCTION SHOULD BE MOVED INTO pmap_destroy().
 */
void
pmap_release(pmap)
	struct pmap *pmap;
{

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_release(%p)\n", pmap));

#ifdef notdef /* DIAGNOSTIC */
	/* count would be 0 from pmap_destroy... */
	simple_lock(&pmap->pm_lock);
	if (pmap->pm_count != 1)
		panic("pmap_release count");
#endif

	if (pmap->pm_ptab)
		uvm_km_free_wakeup(pt_map, (vaddr_t)pmap->pm_ptab,
				   HP_MAX_PTSIZE);
	if (pmap->pm_stab != Segtabzero)
		uvm_km_free_wakeup(st_map, (vaddr_t)pmap->pm_stab,
				   HP_STSIZE);
}

/*
 * pmap_reference:		[ INTERFACE ]
 *
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t	pmap;
{

	if (pmap == NULL)
		return;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_reference(%p)\n", pmap));

	simple_lock(&pmap->pm_lock);
	pmap->pm_count++;
	simple_unlock(&pmap->pm_lock);
}

/*
 * pmap_activate:		[ INTERFACE ]
 *
 *	Activate the pmap used by the specified process.  This includes
 *	reloading the MMU context if the current process, and marking
 *	the pmap in use by the processor.
 *
 *	Note: we may only use spin locks here, since we are called
 *	by a critical section in cpu_switch()!
 */
void
pmap_activate(p)
	struct proc *p;
{
	pmap_t pmap = p->p_vmspace->vm_map.pmap;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_SEGTAB,
	    ("pmap_activate(%p)\n", p));

	PMAP_ACTIVATE(pmap, p == curproc);
}

/*
 * pmap_deactivate:		[ INTERFACE ]
 *
 *	Mark that the pmap used by the specified process is no longer
 *	in use by the processor.
 *
 *	The comment above pmap_activate() wrt. locking applies here,
 *	as well.
 */
void
pmap_deactivate(p)
	struct proc *p;
{

	/* No action necessary in this pmap implementation. */
}

/*
 * pmap_remove:			[ INTERFACE ]
 *
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
	boolean_t firstpage, needcflush;
	int flags;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT,
	    ("pmap_remove(%p, %lx, %lx)\n", pmap, sva, eva));

	if (pmap == NULL)
		return;

	firstpage = TRUE;
	needcflush = FALSE;
	flags = active_pmap(pmap) ? PRM_TFLUSH : 0;
	while (sva < eva) {
		nssva = hp300_trunc_seg(sva) + HP_SEG_SIZE;
		if (nssva == 0 || nssva > eva)
			nssva = eva;
		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		if (!pmap_ste_v(pmap, sva)) {
			sva = nssva;
			continue;
		}
		/*
		 * Invalidate every valid mapping within this segment.
		 */
		pte = pmap_pte(pmap, sva);
		while (sva < nssva) {
			if (pmap_pte_v(pte)) {
#ifdef M68K_MMU_HP
				if (pmap_aliasmask) {
					/*
					 * Purge kernel side of VAC to ensure
					 * we get the correct state of any
					 * hardware maintained bits.
					 */
					if (firstpage) {
						DCIS();
					}
					/*
					 * Remember if we may need to
					 * flush the VAC due to a non-CI
					 * mapping.
					 */
					if (!needcflush && !pmap_pte_ci(pte))
						needcflush = TRUE;

				}
#endif
				pmap_remove_mapping(pmap, sva, pte, flags);
				firstpage = FALSE;
			}
			pte++;
			sva += NBPG;
		}
	}
	/*
	 * Didn't do anything, no need for cache flushes
	 */
	if (firstpage)
		return;
#ifdef M68K_MMU_HP
	/*
	 * In a couple of cases, we don't need to worry about flushing
	 * the VAC:
	 * 	1. if this is a kernel mapping,
	 *	   we have already done it
	 *	2. if it is a user mapping not for the current process,
	 *	   it won't be there
	 */
	if (pmap_aliasmask && !active_user_pmap(pmap))
		needcflush = FALSE;
	if (needcflush) {
		if (pmap == pmap_kernel()) {
			DCIS();
		} else {
			DCIU();
		}
	}
#endif
}

/*
 * pmap_page_protect:		[ INTERFACE ]
 *
 *	Lower the permission for all mappings to a given page to
 *	the permissions specified.
 */
void
pmap_page_protect(pa, prot)
	paddr_t		pa;
	vm_prot_t	prot;
{
	struct pv_entry *pv;
	int s;

#ifdef DEBUG
	if ((pmapdebug & (PDB_FOLLOW|PDB_PROTECT)) ||
	    (prot == VM_PROT_NONE && (pmapdebug & PDB_REMOVE)))
		printf("pmap_page_protect(%lx, %x)\n", pa, prot);
#endif
	if (PAGE_IS_MANAGED(pa) == 0)
		return;

	switch (prot) {
	case VM_PROT_READ|VM_PROT_WRITE:
	case VM_PROT_ALL:
		return;
	/* copy_on_write */
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		pmap_changebit(pa, PG_RO, ~0);
		return;
	/* remove_all */
	default:
		break;
	}
	pv = pa_to_pvh(pa);
	s = splimp();
	while (pv->pv_pmap != NULL) {
		pt_entry_t *pte;

		pte = pmap_pte(pv->pv_pmap, pv->pv_va);
#ifdef DEBUG
		if (!pmap_ste_v(pv->pv_pmap, pv->pv_va) ||
		    pmap_pte_pa(pte) != pa)
			panic("pmap_page_protect: bad mapping");
#endif
		if (!pmap_pte_w(pte))
			pmap_remove_mapping(pv->pv_pmap, pv->pv_va,
					    pte, PRM_TFLUSH|PRM_CFLUSH);
		else {
			pv = pv->pv_next;
#ifdef DEBUG
			if (pmapdebug & PDB_PARANOIA)
				printf("%s wired mapping for %lx not removed\n",
				       "pmap_page_protect:", pa);
#endif
			if (pv == NULL)
				break;
		}
	}
	splx(s);
}

/*
 * pmap_protect:		[ INTERFACE ]
 *
 *	Set the physical protectoin on the specified range of this map
 *	as requested.
 */
void
pmap_protect(pmap, sva, eva, prot)
	pmap_t		pmap;
	vaddr_t		sva, eva;
	vm_prot_t	prot;
{
	vaddr_t nssva;
	pt_entry_t *pte;
	boolean_t firstpage, needtflush;
	int isro;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_PROTECT,
	    ("pmap_protect(%p, %lx, %lx, %x)\n",
	    pmap, sva, eva, prot));

	if (pmap == NULL)
		return;

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}
	if (prot & VM_PROT_WRITE)
		return;

	isro = pte_prot(pmap, prot);
	needtflush = active_pmap(pmap);
	firstpage = TRUE;
	while (sva < eva) {
		nssva = hp300_trunc_seg(sva) + HP_SEG_SIZE;
		if (nssva == 0 || nssva > eva)
			nssva = eva;
		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		if (!pmap_ste_v(pmap, sva)) {
			sva = nssva;
			continue;
		}
		/*
		 * Change protection on mapping if it is valid and doesn't
		 * already have the correct protection.
		 */
		pte = pmap_pte(pmap, sva);
		while (sva < nssva) {
			if (pmap_pte_v(pte) && pmap_pte_prot_chg(pte, isro)) {
#ifdef M68K_MMU_HP
				/*
				 * Purge kernel side of VAC to ensure we
				 * get the correct state of any hardware
				 * maintained bits.
				 *
				 * XXX do we need to clear the VAC in
				 * general to reflect the new protection?
				 */
				if (firstpage && pmap_aliasmask)
					DCIS();
#endif
#if defined(M68040)
				/*
				 * Clear caches if making RO (see section
				 * "7.3 Cache Coherency" in the manual).
				 */
				if (isro && mmutype == MMU_68040) {
					paddr_t pa = pmap_pte_pa(pte);

					DCFP(pa);
					ICPP(pa);
				}
#endif
				pmap_pte_set_prot(pte, isro);
				if (needtflush)
					TBIS(sva);
				firstpage = FALSE;
			}
			pte++;
			sva += NBPG;
		}
	}
}

/*
 * pmap_enter:			[ INTERFACE ]
 *
 *	Insert the given physical page (pa) at
 *	the specified virtual address (va) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte cannot be reclaimed.
 *
 *	Note: This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  Thatis, this routine must actually
 *	insert this page into the given map NOW.
 */
void
pmap_enter(pmap, va, pa, prot, wired, access_type)
	pmap_t pmap;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	boolean_t wired;
	vm_prot_t access_type;
{
	pt_entry_t *pte;
	int npte;
	paddr_t opa;
	boolean_t cacheable = TRUE;
	boolean_t checkpv = TRUE;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_ENTER,
	    ("pmap_enter(%p, %lx, %lx, %x, %x)\n",
	    pmap, va, pa, prot, wired));

	if (pmap == NULL)
		return;

#ifdef DIAGNOSTIC
	/*
	 * pmap_enter() should never be used for CADDR1 and CADDR2.
	 */
	if (pmap == pmap_kernel() &&
	    (va == (vaddr_t)CADDR1 || va == (vaddr_t)CADDR2))
		panic("pmap_enter: used for CADDR1 or CADDR2");
#endif

	/*
	 * For user mapping, allocate kernel VM resources if necessary.
	 */
	if (pmap->pm_ptab == NULL)
		pmap->pm_ptab = (pt_entry_t *)
			uvm_km_valloc_wait(pt_map, HP_MAX_PTSIZE);

	/*
	 * Segment table entry not valid, we need a new PT page
	 */
	if (!pmap_ste_v(pmap, va))
		pmap_enter_ptpage(pmap, va);

	pa = m68k_trunc_page(pa);
	pte = pmap_pte(pmap, va);
	opa = pmap_pte_pa(pte);

	PMAP_DPRINTF(PDB_ENTER, ("enter: pte %p, *pte %x\n", pte, *pte));

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (opa == pa) {
		/*
		 * Wiring change, just update stats.
		 * We don't worry about wiring PT pages as they remain
		 * resident as long as there are valid mappings in them.
		 * Hence, if a user page is wired, the PT page will be also.
		 */
		if (pmap_pte_w_chg(pte, wired ? PG_W : 0)) {
			PMAP_DPRINTF(PDB_ENTER,
			    ("enter: wiring change -> %x\n", wired));
			if (wired)
				pmap->pm_stats.wired_count++;
			else
				pmap->pm_stats.wired_count--;
		}
		/*
		 * Retain cache inhibition status
		 */
		checkpv = FALSE;
		if (pmap_pte_ci(pte))
			cacheable = FALSE;
		goto validate;
	}

	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
		PMAP_DPRINTF(PDB_ENTER,
		    ("enter: removing old mapping %lx\n", va));
		pmap_remove_mapping(pmap, va, pte,
		    PRM_TFLUSH|PRM_CFLUSH|PRM_KEEPPTPAGE);
	}

	/*
	 * If this is a new user mapping, increment the wiring count
	 * on this PT page.  PT pages are wired down as long as there
	 * is a valid mapping in the page.
	 */
	if (pmap != pmap_kernel())
		pmap_ptpage_addref(trunc_page(pte));

	/*
	 * Enter on the PV list if part of our managed memory
	 * Note that we raise IPL while manipulating pv_table
	 * since pmap_enter can be called at interrupt time.
	 */
	if (PAGE_IS_MANAGED(pa)) {
		struct pv_entry *pv, *npv;
		int s;

		pv = pa_to_pvh(pa);
		s = splimp();
		PMAP_DPRINTF(PDB_ENTER,
		    ("enter: pv at %p: %lx/%p/%p\n",
		    pv, pv->pv_va, pv->pv_pmap, pv->pv_next));
		/*
		 * No entries yet, use header as the first entry
		 */
		if (pv->pv_pmap == NULL) {
			pv->pv_va = va;
			pv->pv_pmap = pmap;
			pv->pv_next = NULL;
			pv->pv_ptste = NULL;
			pv->pv_ptpmap = NULL;
			pv->pv_flags = 0;
		}
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
		else {
#ifdef DEBUG
			for (npv = pv; npv; npv = npv->pv_next)
				if (pmap == npv->pv_pmap && va == npv->pv_va)
					panic("pmap_enter: already in pv_tab");
#endif
			npv = pmap_alloc_pv();
			npv->pv_va = va;
			npv->pv_pmap = pmap;
			npv->pv_next = pv->pv_next;
			npv->pv_ptste = NULL;
			npv->pv_ptpmap = NULL;
			npv->pv_flags = 0;
			pv->pv_next = npv;
#ifdef M68K_MMU_HP
			/*
			 * Since there is another logical mapping for the
			 * same page we may need to cache-inhibit the
			 * descriptors on those CPUs with external VACs.
			 * We don't need to CI if:
			 *
			 * - No two mappings belong to the same user pmaps.
			 *   Since the cache is flushed on context switches
			 *   there is no problem between user processes.
			 *
			 * - Mappings within a single pmap are a certain
			 *   magic distance apart.  VAs at these appropriate
			 *   boundaries map to the same cache entries or
			 *   otherwise don't conflict.
			 *
			 * To keep it simple, we only check for these special
			 * cases if there are only two mappings, otherwise we
			 * punt and always CI.
			 *
			 * Note that there are no aliasing problems with the
			 * on-chip data-cache when the WA bit is set.
			 */
			if (pmap_aliasmask) {
				if (pv->pv_flags & PV_CI) {
					PMAP_DPRINTF(PDB_CACHE,
					    ("enter: pa %lx already CI'ed\n",
					    pa));
					checkpv = cacheable = FALSE;
				} else if (npv->pv_next ||
					   ((pmap == pv->pv_pmap ||
					     pmap == pmap_kernel() ||
					     pv->pv_pmap == pmap_kernel()) &&
					    ((pv->pv_va & pmap_aliasmask) !=
					     (va & pmap_aliasmask)))) {
					PMAP_DPRINTF(PDB_CACHE,
					    ("enter: pa %lx CI'ing all\n",
					    pa));
					cacheable = FALSE;
					pv->pv_flags |= PV_CI;
				}
			}
#endif
		}

		/*
		 * Speed pmap_is_referenced() or pmap_is_modified() based
		 * on the hint provided in access_type.
		 */
#ifdef DIAGNOSTIC
		if (access_type & ~prot)
			panic("pmap_enter: access_type exceeds prot");
#endif
		if (access_type & VM_PROT_WRITE)
			*pa_to_attribute(pa) |= (PG_U|PG_M);
		else if (access_type & VM_PROT_ALL)
			*pa_to_attribute(pa) |= PG_U;

		splx(s);
	}
	/*
	 * Assumption: if it is not part of our managed memory
	 * then it must be device memory which may be volitile.
	 */
	else if (pmap_initialized) {
		checkpv = cacheable = FALSE;
	}

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;
	if (wired)
		pmap->pm_stats.wired_count++;

validate:
#ifdef M68K_MMU_HP
	/*
	 * Purge kernel side of VAC to ensure we get correct state
	 * of HW bits so we don't clobber them.
	 */
	if (pmap_aliasmask)
		DCIS();
#endif
	/*
	 * Build the new PTE.
	 */
	npte = pa | pte_prot(pmap, prot) | (*pte & (PG_M|PG_U)) | PG_V;
	if (wired)
		npte |= PG_W;
	if (!checkpv && !cacheable)
		npte |= PG_CI;
#if defined(M68040)
	if (mmutype == MMU_68040 && (npte & (PG_PROT|PG_CI)) == PG_RW)
#ifdef DEBUG
		if (dowriteback && (dokwriteback || pmap != pmap_kernel()))
#endif
		npte |= PG_CCB;
#endif

	PMAP_DPRINTF(PDB_ENTER, ("enter: new pte value %x\n", npte));

	/*
	 * Remember if this was a wiring-only change.
	 * If so, we need not flush the TLB and caches.
	 */
	wired = ((*pte ^ npte) == PG_W);
#if defined(M68040)
	if (mmutype == MMU_68040 && !wired) {
		DCFP(pa);
		ICPP(pa);
	}
#endif
	*pte = npte;
	if (!wired && active_pmap(pmap))
		TBIS(va);
#ifdef M68K_MMU_HP
	/*
	 * The following is executed if we are entering a second
	 * (or greater) mapping for a physical page and the mappings
	 * may create an aliasing problem.  In this case we must
	 * cache inhibit the descriptors involved and flush any
	 * external VAC.
	 */
	if (checkpv && !cacheable) {
		pmap_changebit(pa, PG_CI, ~0);
		DCIA();
#ifdef DEBUG
		if ((pmapdebug & (PDB_CACHE|PDB_PVDUMP)) ==
		    (PDB_CACHE|PDB_PVDUMP))
			pmap_pvdump(pa);
#endif
	}
#endif
#ifdef DEBUG
	if ((pmapdebug & PDB_WIRING) && pmap != pmap_kernel())
		pmap_check_wiring("enter", trunc_page(pte));
#endif
}

/*
 * pmap_unwire:			[ INTERFACE ]
 *
 *	Clear the wired attribute for a map/virtual-address pair.
 *
 *	The mapping must already exist in the pmap.
 */
void
pmap_unwire(pmap, va)
	pmap_t		pmap;
	vaddr_t		va;
{
	pt_entry_t *pte;

	PMAP_DPRINTF(PDB_FOLLOW,
	    ("pmap_unwire(%p, %lx)\n", pmap, va));

	if (pmap == NULL)
		return;

	pte = pmap_pte(pmap, va);
#ifdef DEBUG
	/*
	 * Page table page is not allocated.
	 * Should this ever happen?  Ignore it for now,
	 * we don't want to force allocation of unnecessary PTE pages.
	 */
	if (!pmap_ste_v(pmap, va)) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_unwire: invalid STE for %lx\n", va);
		return;
	}
	/*
	 * Page not valid.  Should this ever happen?
	 * Just continue and change wiring anyway.
	 */
	if (!pmap_pte_v(pte)) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_unwire: invalid PTE for %lx\n", va);
	}
#endif
	/*
	 * If wiring actually changed (always?) clear the wire bit and
	 * update the wire count.  Note that wiring is not a hardware
	 * characteristic so there is no need to invalidate the TLB.
	 */
	if (pmap_pte_w_chg(pte, 0)) {
		pmap_pte_set_w(pte, FALSE);
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
 * pmap_extract:		[ INTERFACE ]
 *
 *	Extract the physical address associated with the given
 *	pmap/virtual address pair.
 */
boolean_t
pmap_extract(pmap, va, pap)
	pmap_t	pmap;
	vaddr_t va;
	paddr_t *pap;
{
	boolean_t rv = FALSE;
	paddr_t pa;
	u_int pte;

	PMAP_DPRINTF(PDB_FOLLOW,
	    ("pmap_extract(%p, %lx) -> ", pmap, va));

	if (pmap && pmap_ste_v(pmap, va)) {
		pte = *(u_int *)pmap_pte(pmap, va);
		if (pte) {
			pa = (pte & PG_FRAME) | (va & ~PG_FRAME);
			if (pap != NULL)
				*pap = pa;
			rv = TRUE;
		}
	}
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		if (rv)
			printf("%lx\n", pa);
		else
			printf("failed\n");
	}
#endif

	return (rv);
}

/*
 * pmap_copy:		[ INTERFACE ]
 *
 *	Copy the mapping range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t		dst_pmap;
	pmap_t		src_pmap;
	vaddr_t		dst_addr;
	vsize_t		len;
	vaddr_t		src_addr;
{

	PMAP_DPRINTF(PDB_FOLLOW,
	    ("pmap_copy(%p, %p, %lx, %lx, %lx)\n",
	    dst_pmap, src_pmap, dst_addr, len, src_addr));
}

/*
 * pmap_update:
 *
 *	Require that all active physical maps contain no
 *	incorrect entires NOW, by processing any deferred
 *	pmap operations.
 */
void
pmap_update()
{

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_update()\n"));

	TBIA();		/* XXX should not be here. */
}

/*
 * pmap_collect:		[ INTERFACE ]
 *
 *	Garbage collects the physical map system for pages which are no
 *	longer used.  Success need not be guaranteed -- that is, there
 *	may well be pages which are not referenced, but others may be
 *	collected.
 *
 *	Called by the pageout daemon when pages are scarce.
 */
void
pmap_collect(pmap)
	pmap_t		pmap;
{

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_collect(%p)\n", pmap));

	if (pmap == pmap_kernel()) {
		int bank, s;

		/*
		 * XXX This is very bogus.  We should handle kernel PT
		 * XXX pages much differently.
		 */

		s = splimp();
		for (bank = 0; bank < vm_nphysseg; bank++)
			pmap_collect1(pmap, ptoa(vm_physmem[bank].start),
			    ptoa(vm_physmem[bank].end));
		splx(s);
	} else {
		/*
		 * This process is about to be swapped out; free all of
		 * the PT pages by removing the physical mappings for its
		 * entire address space.  Note: pmap_remove() performs
		 * all necessary locking.
		 */
		pmap_remove(pmap, VM_MIN_ADDRESS, VM_MAX_ADDRESS);
	}

#ifdef notyet
	/* Go compact and garbage-collect the pv_table. */
	pmap_collect_pv();
#endif
}

/*
 * pmap_collect1():
 *
 *	Garbage-collect KPT pages.  Helper for the above (bogus)
 *	pmap_collect().
 *
 *	Note: THIS SHOULD GO AWAY, AND BE REPLACED WITH A BETTER
 *	WAY OF HANDLING PT PAGES!
 */
void
pmap_collect1(pmap, startpa, endpa)
	pmap_t		pmap;
	paddr_t		startpa, endpa;
{
	paddr_t pa;
	struct pv_entry *pv;
	pt_entry_t *pte;
	paddr_t kpa;
#ifdef DEBUG
	st_entry_t *ste;
	int opmapdebug = 0 /* XXX initialize to quiet gcc -Wall */;
#endif

	for (pa = startpa; pa < endpa; pa += NBPG) {
		struct kpt_page *kpt, **pkpt;

		/*
		 * Locate physical pages which are being used as kernel
		 * page table pages.
		 */
		pv = pa_to_pvh(pa);
		if (pv->pv_pmap != pmap_kernel() || !(pv->pv_flags & PV_PTPAGE))
			continue;
		do {
			if (pv->pv_ptste && pv->pv_ptpmap == pmap_kernel())
				break;
		} while ((pv = pv->pv_next));
		if (pv == NULL)
			continue;
#ifdef DEBUG
		if (pv->pv_va < (vaddr_t)Sysmap ||
		    pv->pv_va >= (vaddr_t)Sysmap + HP_MAX_PTSIZE)
			printf("collect: kernel PT VA out of range\n");
		else
			goto ok;
		pmap_pvdump(pa);
		continue;
ok:
#endif
		pte = (pt_entry_t *)(pv->pv_va + NBPG);
		while (--pte >= (pt_entry_t *)pv->pv_va && *pte == PG_NV)
			;
		if (pte >= (pt_entry_t *)pv->pv_va)
			continue;

#ifdef DEBUG
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT)) {
			printf("collect: freeing KPT page at %lx (ste %x@%p)\n",
			       pv->pv_va, *pv->pv_ptste, pv->pv_ptste);
			opmapdebug = pmapdebug;
			pmapdebug |= PDB_PTPAGE;
		}

		ste = pv->pv_ptste;
#endif
		/*
		 * If all entries were invalid we can remove the page.
		 * We call pmap_remove_entry to take care of invalidating
		 * ST and Sysptmap entries.
		 */
		(void) pmap_extract(pmap, pv->pv_va, (paddr_t *)&kpa);
		pmap_remove_mapping(pmap, pv->pv_va, PT_ENTRY_NULL,
				    PRM_TFLUSH|PRM_CFLUSH);
		/*
		 * Use the physical address to locate the original
		 * (kmem_alloc assigned) address for the page and put
		 * that page back on the free list.
		 */
		for (pkpt = &kpt_used_list, kpt = *pkpt;
		     kpt != (struct kpt_page *)0;
		     pkpt = &kpt->kpt_next, kpt = *pkpt)
			if (kpt->kpt_pa == kpa)
				break;
#ifdef DEBUG
		if (kpt == (struct kpt_page *)0)
			panic("pmap_collect: lost a KPT page");
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT))
			printf("collect: %lx (%lx) to free list\n",
			       kpt->kpt_va, kpa);
#endif
		*pkpt = kpt->kpt_next;
		kpt->kpt_next = kpt_free_list;
		kpt_free_list = kpt;
#ifdef DEBUG
		if (pmapdebug & (PDB_PTPAGE|PDB_COLLECT))
			pmapdebug = opmapdebug;

		if (*ste != SG_NV)
			printf("collect: kernel STE at %p still valid (%x)\n",
			       ste, *ste);
		ste = &Sysptmap[ste - pmap_ste(pmap_kernel(), 0)];
		if (*ste != SG_NV)
			printf("collect: kernel PTmap at %p still valid (%x)\n",
			       ste, *ste);
#endif
	}
}

/*
 * pmap_zero_page:		[ INTERFACE ]
 *
 *	Zero the specified (machine independent) page by mapping the page
 *	into virtual memory and using bzero to clear its contents, one
 *	machine dependent page at a time.
 *
 *	Note: WE DO NOT CURRENTLY LOCK THE TEMPORARY ADDRESSES!
 *	      (Actually, we go to splimp(), and since we don't
 *	      support multiple processors, this is sufficient.)
 */
void
pmap_zero_page(phys)
	paddr_t phys;
{
	int s, npte;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_zero_page(%lx)\n", phys));

	npte = phys | PG_V;
#ifdef M68K_MMU_HP
	if (pmap_aliasmask) {
		/*
		 * Cache-inhibit the mapping on VAC machines, as we would
		 * be wasting the cache load.
		 */
		npte |= PG_CI;
	}
#endif

#if defined(M68040) || defined(M68060)
	if (mmutype == MMU_68040) {
		/*
		 * Set copyback caching on the page; this is required
		 * for cache consistency (since regular mappings are
		 * copyback as well).
		 */
		npte |= PG_CCB;
	}
#endif

	s = splimp();

	*caddr1_pte = npte;
	TBIS((vaddr_t)CADDR1);

	zeropage(CADDR1);

#ifdef DEBUG
	*caddr1_pte = PG_NV;
	TBIS((vaddr_t)CADDR1);
#endif

	splx(s);
}

/*
 * pmap_copy_page:		[ INTERFACE ]
 *
 *	Copy the specified (machine independent) page by mapping the page
 *	into virtual memory and using bcopy to copy the page, one machine
 *	dependent page at a time.
 *
 *	Note: WE DO NOT CURRENTLY LOCK THE TEMPORARY ADDRESSES!
 *	      (Actually, we go to splimp(), and since we don't
 *	      support multiple processors, this is sufficient.)
 */
void
pmap_copy_page(src, dst)
	paddr_t src, dst;
{
	int s, npte1, npte2;

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_copy_page(%lx, %lx)\n", src, dst));

	npte1 = src | PG_RO | PG_V;
	npte2 = dst | PG_V;
#ifdef M68K_MMU_HP
	if (pmap_aliasmask) {
		/*
		 * Cache-inhibit the mapping on VAC machines, as we would
		 * be wasting the cache load.
		 */
		npte1 |= PG_CI;
		npte2 |= PG_CI;
	}
#endif

#if defined(M68040) || defined(M68060)
	if (mmutype == MMU_68040) {
		/*
		 * Set copyback caching on the pages; this is required
		 * for cache consistency (since regular mappings are
		 * copyback as well).
		 */
		npte1 |= PG_CCB;
		npte2 |= PG_CCB;
	}
#endif

	s = splimp();

	*caddr1_pte = npte1;
	TBIS((vaddr_t)CADDR1);

	*caddr2_pte = npte2;
	TBIS((vaddr_t)CADDR2);

	copypage(CADDR1, CADDR2);

#ifdef DEBUG
	*caddr1_pte = PG_NV;
	TBIS((vaddr_t)CADDR1);

	*caddr2_pte = PG_NV;
	TBIS((vaddr_t)CADDR2);
#endif

	splx(s);
}

/*
 * pmap_clear_modify:		[ INTERFACE ]
 *
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(pa)
	paddr_t	pa;
{

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_clear_modify(%lx)\n", pa));

	pmap_changebit(pa, 0, ~PG_M);
}

/*
 * pmap_clear_reference:	[ INTERFACE ]
 *
 *	Clear the reference bit on the specified physical page.
 */
void
pmap_clear_reference(pa)
	paddr_t	pa;
{

	PMAP_DPRINTF(PDB_FOLLOW, ("pmap_clear_reference(%lx)\n", pa));

	pmap_changebit(pa, 0, ~PG_U);
}

/*
 * pmap_is_referenced:		[ INTERFACE ]
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */
boolean_t
pmap_is_referenced(pa)
	paddr_t	pa;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		boolean_t rv = pmap_testbit(pa, PG_U);
		printf("pmap_is_referenced(%lx) -> %c\n", pa, "FT"[rv]);
		return(rv);
	}
#endif
	return(pmap_testbit(pa, PG_U));
}

/*
 * pmap_is_modified:		[ INTERFACE ]
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */
boolean_t
pmap_is_modified(pa)
	paddr_t	pa;
{
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW) {
		boolean_t rv = pmap_testbit(pa, PG_M);
		printf("pmap_is_modified(%lx) -> %c\n", pa, "FT"[rv]);
		return(rv);
	}
#endif
	return(pmap_testbit(pa, PG_M));
}

/*
 * pmap_phys_address:		[ INTERFACE ]
 *
 *	Return the physical address corresponding to the specified
 *	cookie.  Used by the device pager to decode a device driver's
 *	mmap entry point return value.
 *
 *	Note: no locking is necessary in this function.
 */
paddr_t
pmap_phys_address(ppn)
	int ppn;
{
	return(m68k_ptob(ppn));
}

#ifdef M68K_MMU_HP
/*
 * pmap_prefer:			[ INTERFACE ]
 *
 *	Find the first virtual address >= *vap that does not
 *	cause a virtually-tagged cache alias problem.
 */
void
pmap_prefer(foff, vap)
	vaddr_t foff, *vap;
{
	vaddr_t va;
	vsize_t d;

#ifdef M68K_MMU_MOTOROLA
	if (pmap_aliasmask)
#endif
	{
		va = *vap;
		d = foff - va;
		d &= pmap_aliasmask;
		*vap = va + d;
	}
}
#endif /* M68K_MMU_HP */

#ifdef COMPAT_HPUX
/*
 * pmap_mapmulti:
 *
 *	'PUX hack for dealing with the so called multi-mapped address space.
 *	The first 256mb is mapped in at every 256mb region from 0x10000000
 *	up to 0xF0000000.  This allows for 15 bits of tag information.
 *
 *	We implement this at the segment table level, the machine independent
 *	VM knows nothing about it.
 */
int
pmap_mapmulti(pmap, va)
	pmap_t pmap;
	vaddr_t va;
{
	st_entry_t *ste, *bste;

#ifdef DEBUG
	if (pmapdebug & PDB_MULTIMAP) {
		ste = pmap_ste(pmap, HPMMBASEADDR(va));
		printf("pmap_mapmulti(%p, %lx): bste %p(%x)",
		       pmap, va, ste, *ste);
		ste = pmap_ste(pmap, va);
		printf(" ste %p(%x)\n", ste, *ste);
	}
#endif
	bste = pmap_ste(pmap, HPMMBASEADDR(va));
	ste = pmap_ste(pmap, va);
	if (*ste == SG_NV && (*bste & SG_V)) {
		*ste = *bste;
		TBIAU();
		return (KERN_SUCCESS);
	}
	return (KERN_INVALID_ADDRESS);
}
#endif /* COMPAT_HPUX */

/*
 * Miscellaneous support routines follow
 */

/*
 * pmap_remove_mapping:
 *
 *	Invalidate a single page denoted by pmap/va.
 *
 *	If (pte != NULL), it is the already computed PTE for the page.
 *
 *	If (flags & PRM_TFLUSH), we must invalidate any TLB information.
 *
 *	If (flags & PRM_CFLUSH), we must flush/invalidate any cache
 *	information.
 *
 *	If (flags & PRM_KEEPPTPAGE), we don't free the page table page
 *	if the reference drops to zero.
 */
/* static */
void
pmap_remove_mapping(pmap, va, pte, flags)
	pmap_t pmap;
	vaddr_t va;
	pt_entry_t *pte;
	int flags;
{
	paddr_t pa;
	struct pv_entry *pv, *npv;
	pmap_t ptpmap;
	st_entry_t *ste;
	int s, bits;
#ifdef DEBUG
	pt_entry_t opte;
#endif

	PMAP_DPRINTF(PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT,
	    ("pmap_remove_mapping(%p, %lx, %p, %x)\n",
	    pmap, va, pte, flags));

	/*
	 * PTE not provided, compute it from pmap and va.
	 */
	if (pte == PT_ENTRY_NULL) {
		pte = pmap_pte(pmap, va);
		if (*pte == PG_NV)
			return;
	}
#ifdef M68K_MMU_HP
	if (pmap_aliasmask && (flags & PRM_CFLUSH)) {
		/*
		 * Purge kernel side of VAC to ensure we get the correct
		 * state of any hardware maintained bits.
		 */
		DCIS();
		/*
		 * If this is a non-CI user mapping for the current process,
		 * flush the VAC.  Note that the kernel side was flushed
		 * above so we don't worry about non-CI kernel mappings.
		 */
		if (active_user_pmap(pmap) && !pmap_pte_ci(pte)) {
			DCIU();
		}
	}
#endif
	pa = pmap_pte_pa(pte);
#ifdef DEBUG
	opte = *pte;
#endif
	/*
	 * Update statistics
	 */
	if (pmap_pte_w(pte))
		pmap->pm_stats.wired_count--;
	pmap->pm_stats.resident_count--;

	/*
	 * Invalidate the PTE after saving the reference modify info.
	 */
	PMAP_DPRINTF(PDB_REMOVE, ("remove: invalidating pte at %p\n", pte));
	bits = *pte & (PG_U|PG_M);
	*pte = PG_NV;
	if ((flags & PRM_TFLUSH) && active_pmap(pmap))
		TBIS(va);
	/*
	 * For user mappings decrement the wiring count on
	 * the PT page.
	 */
	if (pmap != pmap_kernel()) {
		vaddr_t ptpva = trunc_page(pte);
		int refs = pmap_ptpage_delref(ptpva);
#ifdef DEBUG
		if (pmapdebug & PDB_WIRING)
			pmap_check_wiring("remove", ptpva);
#endif
		/*
		 * If reference count drops to 1, and we're not instructed
		 * to keep it around, free the PT page.
		 *
		 * Note: refcnt == 1 comes from the fact that we allocate
		 * the page with uvm_fault_wire(), which initially wires
		 * the page.  The first reference we actually add causes
		 * the refcnt to be 2.
		 */
		if (refs == 1 && (flags & PRM_KEEPPTPAGE) == 0) {
			struct pv_entry *pv;
			paddr_t pa;

			pa = pmap_pte_pa(pmap_pte(pmap_kernel(), ptpva));
#ifdef DIAGNOSTIC
			if (PAGE_IS_MANAGED(pa) == 0)
				panic("pmap_remove_mapping: unmanaged PT page");
#endif
			pv = pa_to_pvh(pa);
#ifdef DIAGNOSTIC
			if (pv->pv_ptste == NULL)
				panic("pmap_remove_mapping: ptste == NULL");
			if (pv->pv_pmap != pmap_kernel() ||
			    pv->pv_va != ptpva ||
			    pv->pv_next != NULL)
				panic("pmap_remove_mapping: "
				    "bad PT page pmap %p, va 0x%lx, next %p",
				    pv->pv_pmap, pv->pv_va, pv->pv_next);
#endif
			pmap_remove_mapping(pv->pv_pmap, pv->pv_va,
			    NULL, PRM_TFLUSH|PRM_CFLUSH);
			uvm_pagefree(PHYS_TO_VM_PAGE(pa));
			PMAP_DPRINTF(PDB_REMOVE|PDB_PTPAGE,
			    ("remove: PT page 0x%lx (0x%lx) freed\n",
			    ptpva, pa));
		}
	}
	/*
	 * If this isn't a managed page, we are all done.
	 */
	if (PAGE_IS_MANAGED(pa) == 0)
		return;
	/*
	 * Otherwise remove it from the PV table
	 * (raise IPL since we may be called at interrupt time).
	 */
	pv = pa_to_pvh(pa);
	ste = ST_ENTRY_NULL;
	s = splimp();
	/*
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry.  In either case we free the now unused entry.
	 */
	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		ste = pv->pv_ptste;
		ptpmap = pv->pv_ptpmap;
		npv = pv->pv_next;
		if (npv) {
			npv->pv_flags = pv->pv_flags;
			*pv = *npv;
			pmap_free_pv(npv);
		} else
			pv->pv_pmap = NULL;
	} else {
		for (npv = pv->pv_next; npv; npv = npv->pv_next) {
			if (pmap == npv->pv_pmap && va == npv->pv_va)
				break;
			pv = npv;
		}
#ifdef DEBUG
		if (npv == NULL)
			panic("pmap_remove: PA not in pv_tab");
#endif
		ste = npv->pv_ptste;
		ptpmap = npv->pv_ptpmap;
		pv->pv_next = npv->pv_next;
		pmap_free_pv(npv);
		pv = pa_to_pvh(pa);
	}
#ifdef M68K_MMU_HP
	/*
	 * If only one mapping left we no longer need to cache inhibit
	 */
	if (pmap_aliasmask &&
	    pv->pv_pmap && pv->pv_next == NULL && (pv->pv_flags & PV_CI)) {
		PMAP_DPRINTF(PDB_CACHE,
		    ("remove: clearing CI for pa %lx\n", pa));
		pv->pv_flags &= ~PV_CI;
		pmap_changebit(pa, 0, ~PG_CI);
#ifdef DEBUG
		if ((pmapdebug & (PDB_CACHE|PDB_PVDUMP)) ==
		    (PDB_CACHE|PDB_PVDUMP))
			pmap_pvdump(pa);
#endif
	}
#endif
	/*
	 * If this was a PT page we must also remove the
	 * mapping from the associated segment table.
	 */
	if (ste) {
		PMAP_DPRINTF(PDB_REMOVE|PDB_PTPAGE,
		    ("remove: ste was %x@%p pte was %x@%p\n",
		    *ste, ste, opte, pmap_pte(pmap, va)));
#if defined(M68040)
		if (mmutype == MMU_68040) {
			st_entry_t *este = &ste[NPTEPG/SG4_LEV3SIZE];

			while (ste < este)
				*ste++ = SG_NV;
#ifdef DEBUG
			ste -= NPTEPG/SG4_LEV3SIZE;
#endif
		} else
#endif
		*ste = SG_NV;
		/*
		 * If it was a user PT page, we decrement the
		 * reference count on the segment table as well,
		 * freeing it if it is now empty.
		 */
		if (ptpmap != pmap_kernel()) {
			PMAP_DPRINTF(PDB_REMOVE|PDB_SEGTAB,
			    ("remove: stab %p, refcnt %d\n",
			    ptpmap->pm_stab, ptpmap->pm_sref - 1));
#ifdef DEBUG
			if ((pmapdebug & PDB_PARANOIA) &&
			    ptpmap->pm_stab != (st_entry_t *)trunc_page(ste))
				panic("remove: bogus ste");
#endif
			if (--(ptpmap->pm_sref) == 0) {
				PMAP_DPRINTF(PDB_REMOVE|PDB_SEGTAB,
				    ("remove: free stab %p\n",
				    ptpmap->pm_stab));
				uvm_km_free_wakeup(st_map,
						 (vaddr_t)ptpmap->pm_stab,
						 HP_STSIZE);
				ptpmap->pm_stab = Segtabzero;
				ptpmap->pm_stpa = Segtabzeropa;
#if defined(M68040)
				if (mmutype == MMU_68040)
					ptpmap->pm_stfree = protostfree;
#endif
				/*
				 * XXX may have changed segment table
				 * pointer for current process so
				 * update now to reload hardware.
				 */
				if (active_user_pmap(ptpmap))
					PMAP_ACTIVATE(ptpmap, 1);
			}
#ifdef DEBUG
			else if (ptpmap->pm_sref < 0)
				panic("remove: sref < 0");
#endif
		}
#if 0
		/*
		 * XXX this should be unnecessary as we have been
		 * flushing individual mappings as we go.
		 */
		if (ptpmap == pmap_kernel())
			TBIAS();
		else
			TBIAU();
#endif
		pv->pv_flags &= ~PV_PTPAGE;
		ptpmap->pm_ptpages--;
	}
	/*
	 * Update saved attributes for managed page
	 */
	*pa_to_attribute(pa) |= bits;
	splx(s);
}

/*
 * pmap_testbit:
 *
 *	Test the modified/referenced bits of a physical page.
 */
/* static */
boolean_t
pmap_testbit(pa, bit)
	paddr_t pa;
	int bit;
{
	struct pv_entry *pv;
	pt_entry_t *pte;
	int s;

	if (PAGE_IS_MANAGED(pa) == 0)
		return(FALSE);

	pv = pa_to_pvh(pa);
	s = splimp();
	/*
	 * Check saved info first
	 */
	if (*pa_to_attribute(pa) & bit) {
		splx(s);
		return(TRUE);
	}
#ifdef M68K_MMU_HP
	/*
	 * Flush VAC to get correct state of any hardware maintained bits.
	 */
	if (pmap_aliasmask && (bit & (PG_U|PG_M)))
		DCIS();
#endif
	/*
	 * Not found.  Check current mappings, returning immediately if
	 * found.  Cache a hit to speed future lookups.
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			pte = pmap_pte(pv->pv_pmap, pv->pv_va);
			if (*pte & bit) {
				*pa_to_attribute(pa) |= bit;
				splx(s);
				return(TRUE);
			}
		}
	}
	splx(s);
	return(FALSE);
}

/*
 * pmap_changebit:
 *
 *	Change the modified/referenced bits, or other PTE bits,
 *	for a physical page.
 */
/* static */
void
pmap_changebit(pa, set, mask)
	paddr_t pa;
	int set, mask;
{
	struct pv_entry *pv;
	pt_entry_t *pte, npte;
	vaddr_t va;
	int s;
#if defined(M68K_MMU_HP) || defined(M68040)
	boolean_t firstpage = TRUE;
#endif

	PMAP_DPRINTF(PDB_BITS,
	    ("pmap_changebit(%lx, %x, %x)\n", pa, set, mask));

	if (PAGE_IS_MANAGED(pa) == 0)
		return;

	pv = pa_to_pvh(pa);
	s = splimp();

	/*
	 * Clear saved attributes (modify, reference)
	 */
	*pa_to_attribute(pa) &= mask;

	/*
	 * Loop over all current mappings setting/clearing as appropos
	 * If setting RO do we need to clear the VAC?
	 */
	if (pv->pv_pmap != NULL) {
#ifdef DEBUG
		int toflush = 0;
#endif
		for (; pv; pv = pv->pv_next) {
#ifdef DEBUG
			toflush |= (pv->pv_pmap == pmap_kernel()) ? 2 : 1;
#endif
			va = pv->pv_va;

			/*
			 * XXX don't write protect pager mappings
			 */
			if (set == PG_RO) {
				if (va >= uvm.pager_sva && va < uvm.pager_eva)
					continue;
			}

			pte = pmap_pte(pv->pv_pmap, va);
#ifdef M68K_MMU_HP
			/*
			 * Flush VAC to ensure we get correct state of HW bits
			 * so we don't clobber them.
			 */
			if (firstpage && pmap_aliasmask) {
				firstpage = FALSE;
				DCIS();
			}
#endif
			npte = (*pte | set) & mask;
			if (*pte != npte) {
#if defined(M68040)
				/*
				 * If we are changing caching status or
				 * protection make sure the caches are
				 * flushed (but only once).
				 */
				if (firstpage && (mmutype == MMU_68040) &&
				    ((set == PG_RO) ||
				     (set & PG_CMASK) ||
				     (mask & PG_CMASK) == 0)) {
					firstpage = FALSE;
					DCFP(pa);
					ICPP(pa);
				}
#endif
				*pte = npte;
				if (active_pmap(pv->pv_pmap))
					TBIS(va);
			}
		}
	}
	splx(s);
}

/*
 * pmap_enter_ptpage:
 *
 *	Allocate and map a PT page for the specified pmap/va pair.
 */
/* static */
void
pmap_enter_ptpage(pmap, va)
	pmap_t pmap;
	vaddr_t va;
{
	paddr_t ptpa;
	struct pv_entry *pv;
	st_entry_t *ste;
	int s;

	PMAP_DPRINTF(PDB_FOLLOW|PDB_ENTER|PDB_PTPAGE,
	    ("pmap_enter_ptpage: pmap %p, va %lx\n", pmap, va));

	/*
	 * Allocate a segment table if necessary.  Note that it is allocated
	 * from a private map and not pt_map.  This keeps user page tables
	 * aligned on segment boundaries in the kernel address space.
	 * The segment table is wired down.  It will be freed whenever the
	 * reference count drops to zero.
	 */
	if (pmap->pm_stab == Segtabzero) {
		pmap->pm_stab = (st_entry_t *)
			uvm_km_zalloc(st_map, HP_STSIZE);
		(void) pmap_extract(pmap_kernel(), (vaddr_t)pmap->pm_stab,
		    (paddr_t *)&pmap->pm_stpa);
#if defined(M68040)
		if (mmutype == MMU_68040) {
#ifdef DEBUG
			if (dowriteback && dokwriteback)
#endif
			pmap_changebit((paddr_t)pmap->pm_stpa, 0, ~PG_CCB);
			pmap->pm_stfree = protostfree;
		}
#endif
		/*
		 * XXX may have changed segment table pointer for current
		 * process so update now to reload hardware.
		 */
		if (active_user_pmap(pmap))
			PMAP_ACTIVATE(pmap, 1);

		PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB,
		    ("enter: pmap %p stab %p(%p)\n",
		    pmap, pmap->pm_stab, pmap->pm_stpa));
	}

	ste = pmap_ste(pmap, va);
#if defined(M68040)
	/*
	 * Allocate level 2 descriptor block if necessary
	 */
	if (mmutype == MMU_68040) {
		if (*ste == SG_NV) {
			int ix;
			caddr_t addr;
			
			ix = bmtol2(pmap->pm_stfree);
			if (ix == -1)
				panic("enter: out of address space"); /* XXX */
			pmap->pm_stfree &= ~l2tobm(ix);
			addr = (caddr_t)&pmap->pm_stab[ix*SG4_LEV2SIZE];
			bzero(addr, SG4_LEV2SIZE*sizeof(st_entry_t));
			addr = (caddr_t)&pmap->pm_stpa[ix*SG4_LEV2SIZE];
			*ste = (u_int)addr | SG_RW | SG_U | SG_V;

			PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB,
			    ("enter: alloc ste2 %d(%p)\n", ix, addr));
		}
		ste = pmap_ste2(pmap, va);
		/*
		 * Since a level 2 descriptor maps a block of SG4_LEV3SIZE
		 * level 3 descriptors, we need a chunk of NPTEPG/SG4_LEV3SIZE
		 * (16) such descriptors (NBPG/SG4_LEV3SIZE bytes) to map a
		 * PT page--the unit of allocation.  We set `ste' to point
		 * to the first entry of that chunk which is validated in its
		 * entirety below.
		 */
		ste = (st_entry_t *)((int)ste & ~(NBPG/SG4_LEV3SIZE-1));

		PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB,
		    ("enter: ste2 %p (%p)\n", pmap_ste2(pmap, va), ste));
	}
#endif
	va = trunc_page((vaddr_t)pmap_pte(pmap, va));

	/*
	 * In the kernel we allocate a page from the kernel PT page
	 * free list and map it into the kernel page table map (via
	 * pmap_enter).
	 */
	if (pmap == pmap_kernel()) {
		struct kpt_page *kpt;

		s = splimp();
		if ((kpt = kpt_free_list) == (struct kpt_page *)0) {
			/*
			 * No PT pages available.
			 * Try once to free up unused ones.
			 */
			PMAP_DPRINTF(PDB_COLLECT,
			    ("enter: no KPT pages, collecting...\n"));
			pmap_collect(pmap_kernel());
			if ((kpt = kpt_free_list) == (struct kpt_page *)0)
				panic("pmap_enter_ptpage: can't get KPT page");
		}
		kpt_free_list = kpt->kpt_next;
		kpt->kpt_next = kpt_used_list;
		kpt_used_list = kpt;
		ptpa = kpt->kpt_pa;
		bzero((caddr_t)kpt->kpt_va, NBPG);
		pmap_enter(pmap, va, ptpa, VM_PROT_DEFAULT, TRUE,
		    VM_PROT_DEFAULT);
#ifdef DEBUG
		if (pmapdebug & (PDB_ENTER|PDB_PTPAGE)) {
			int ix = pmap_ste(pmap, va) - pmap_ste(pmap, 0);

			printf("enter: add &Sysptmap[%d]: %x (KPT page %lx)\n",
			       ix, Sysptmap[ix], kpt->kpt_va);
		}
#endif
		splx(s);
	}
	/*
	 * For user processes we just simulate a fault on that location
	 * letting the VM system allocate a zero-filled page.
	 *
	 * Note we use a wire-fault to keep the page off the paging
	 * queues.  This sets our PT page's reference (wire) count to
	 * 1, which is what we use to check if the page can be freed.
	 * See pmap_remove_mapping().
	 */
	else {
		/*
		 * Count the segment table reference now so that we won't
		 * lose the segment table when low on memory.
		 */
		pmap->pm_sref++;
		PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE,
		    ("enter: about to fault UPT pg at %lx\n", va));
		s = uvm_fault_wire(pt_map, va, va + PAGE_SIZE,
		    VM_PROT_READ|VM_PROT_WRITE);
		if (s != KERN_SUCCESS) {
			printf("uvm_fault_wire(pt_map, 0x%lx, 0x%lx, RW) "
			    "-> %d\n", va, va + PAGE_SIZE, s);
			panic("pmap_enter: uvm_fault_wire failed");
		}
		ptpa = pmap_pte_pa(pmap_pte(pmap_kernel(), va));
	}
#if defined(M68040)
	/*
	 * Turn off copyback caching of page table pages,
	 * could get ugly otherwise.
	 */
#ifdef DEBUG
	if (dowriteback && dokwriteback)
#endif
	if (mmutype == MMU_68040) {
#ifdef DEBUG
		pt_entry_t *pte = pmap_pte(pmap_kernel(), va);
		if ((pmapdebug & PDB_PARANOIA) && (*pte & PG_CCB) == 0)
			printf("%s PT no CCB: kva=%lx ptpa=%lx pte@%p=%x\n",
			       pmap == pmap_kernel() ? "Kernel" : "User",
			       va, ptpa, pte, *pte);
#endif
		pmap_changebit(ptpa, 0, ~PG_CCB);
	}
#endif
	/*
	 * Locate the PV entry in the kernel for this PT page and
	 * record the STE address.  This is so that we can invalidate
	 * the STE when we remove the mapping for the page.
	 */
	pv = pa_to_pvh(ptpa);
	s = splimp();
	if (pv) {
		pv->pv_flags |= PV_PTPAGE;
		do {
			if (pv->pv_pmap == pmap_kernel() && pv->pv_va == va)
				break;
		} while ((pv = pv->pv_next));
	}
#ifdef DEBUG
	if (pv == NULL)
		panic("pmap_enter_ptpage: PT page not entered");
#endif
	pv->pv_ptste = ste;
	pv->pv_ptpmap = pmap;

	PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE,
	    ("enter: new PT page at PA %lx, ste at %p\n", ptpa, ste));

	/*
	 * Map the new PT page into the segment table.
	 * Also increment the reference count on the segment table if this
	 * was a user page table page.  Note that we don't use vm_map_pageable
	 * to keep the count like we do for PT pages, this is mostly because
	 * it would be difficult to identify ST pages in pmap_pageable to
	 * release them.  We also avoid the overhead of vm_map_pageable.
	 */
#if defined(M68040)
	if (mmutype == MMU_68040) {
		st_entry_t *este;

		for (este = &ste[NPTEPG/SG4_LEV3SIZE]; ste < este; ste++) {
			*ste = ptpa | SG_U | SG_RW | SG_V;
			ptpa += SG4_LEV3SIZE * sizeof(st_entry_t);
		}
	} else
#endif
	*ste = (ptpa & SG_FRAME) | SG_RW | SG_V;
	if (pmap != pmap_kernel()) {
		PMAP_DPRINTF(PDB_ENTER|PDB_PTPAGE|PDB_SEGTAB,
		    ("enter: stab %p refcnt %d\n",
		    pmap->pm_stab, pmap->pm_sref));
	}
#if 0
	/*
	 * Flush stale TLB info.
	 */
	if (pmap == pmap_kernel())
		TBIAS();
	else
		TBIAU();
#endif
	pmap->pm_ptpages++;
	splx(s);
}

/*
 * pmap_ptpage_addref:
 *
 *	Add a reference to the specified PT page.
 */
void
pmap_ptpage_addref(ptpva)
	vaddr_t ptpva;
{
	vm_page_t m;

	simple_lock(&uvm.kernel_object->vmobjlock);
	m = uvm_pagelookup(uvm.kernel_object, ptpva - vm_map_min(kernel_map));
	m->wire_count++;
	simple_unlock(&uvm.kernel_object->vmobjlock);
}

/*
 * pmap_ptpage_delref:
 *
 *	Delete a reference to the specified PT page.
 */
int
pmap_ptpage_delref(ptpva)
	vaddr_t ptpva;
{
	vm_page_t m;
	int rv;

	simple_lock(&uvm.kernel_object->vmobjlock);
	m = uvm_pagelookup(uvm.kernel_object, ptpva - vm_map_min(kernel_map));
	rv = --m->wire_count;
	simple_unlock(&uvm.kernel_object->vmobjlock);
	return (rv);
}

#ifdef DEBUG
/*
 * pmap_pvdump:
 *
 *	Dump the contents of the PV list for the specified physical page.
 */
/* static */
void
pmap_pvdump(pa)
	paddr_t pa;
{
	struct pv_entry *pv;

	printf("pa %lx", pa);
	for (pv = pa_to_pvh(pa); pv; pv = pv->pv_next)
		printf(" -> pmap %p, va %lx, ptste %p, ptpmap %p, flags %x",
		       pv->pv_pmap, pv->pv_va, pv->pv_ptste, pv->pv_ptpmap,
		       pv->pv_flags);
	printf("\n");
}

/* static */
/*
 * pmap_check_wiring:
 *
 *	Count the number of valid mappings in the specified PT page,
 *	and ensure that it is consistent with the number of wirings
 *	to that page that the VM system has.
 */
void
pmap_check_wiring(str, va)
	char *str;
	vaddr_t va;
{
	pt_entry_t *pte;
	paddr_t pa;
	vm_page_t m;
	int count;

	if (!pmap_ste_v(pmap_kernel(), va) ||
	    !pmap_pte_v(pmap_pte(pmap_kernel(), va)))
		return;

	pa = pmap_pte_pa(pmap_pte(pmap_kernel(), va));
	m = PHYS_TO_VM_PAGE(pa);
	if (m->wire_count < 1) {
		printf("*%s*: 0x%lx: wire count %d\n", str, va, m->wire_count);
		return;
	}

	count = 0;
	for (pte = (pt_entry_t *)va; pte < (pt_entry_t *)(va + NBPG); pte++)
		if (*pte)
			count++;
	if ((m->wire_count - 1) != count)
		printf("*%s*: 0x%lx: w%d/a%d\n",
		       str, va, (m->wire_count - 1), count);
}
#endif /* DEBUG */
