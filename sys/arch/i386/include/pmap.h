/*	$NetBSD: pmap.h,v 1.43.2.11 2001/04/30 16:23:13 sommerfeld Exp $	*/

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
 *    must display the following acknowledgment:
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
 * pmap.h: see pmap.c for the history of this pmap module.
 */

#ifndef	_I386_PMAP_H_
#define	_I386_PMAP_H_

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_user_ldt.h"
#include "opt_largepages.h"
#endif

#include <machine/cpufunc.h>
#include <machine/pte.h>
#include <machine/segments.h>
#include <uvm/uvm_object.h>

/*
 * see pte.h for a description of i386 MMU terminology and hardware
 * interface.
 *
 * a pmap describes a processes' 4GB virtual address space.  this
 * virtual address space can be broken up into 1024 4MB regions which
 * are described by PDEs in the PDP.  the PDEs are defined as follows:
 *
 * (ranges are inclusive -> exclusive, just like vm_map_entry start/end)
 * (the following assumes that KERNBASE is 0xc0000000)
 *
 * PDE#s	VA range		usage
 * 0->766	0x0 -> 0xbfc00000	user address space, note that the
 *					max user address is 0xbfbfe000
 *					the final two pages in the last 4MB
 *					used to be reserved for the UAREA
 *					but now are no longer used
 * 767		0xbfc00000->		recursive mapping of PDP (used for
 *			0xc0000000	linear mapping of PTPs)
 * 768->1023	0xc0000000->		kernel address space (constant
 *			0xffc00000	across all pmap's/processes)
 * 1023		0xffc00000->		"alternate" recursive PDP mapping
 *			<end>		(for other pmaps)
 *
 *
 * note: a recursive PDP mapping provides a way to map all the PTEs for
 * a 4GB address space into a linear chunk of virtual memory.  in other
 * words, the PTE for page 0 is the first int mapped into the 4MB recursive
 * area.  the PTE for page 1 is the second int.  the very last int in the
 * 4MB range is the PTE that maps VA 0xffffe000 (the last page in a 4GB
 * address).
 *
 * all pmap's PD's must have the same values in slots 768->1023 so that
 * the kernel is always mapped in every process.  these values are loaded
 * into the PD at pmap creation time.
 *
 * at any one time only one pmap can be active on a processor.  this is
 * the pmap whose PDP is pointed to by processor register %cr3.  this pmap
 * will have all its PTEs mapped into memory at the recursive mapping
 * point (slot #767 as show above).  when the pmap code wants to find the
 * PTE for a virtual address, all it has to do is the following:
 *
 * address of PTE = (767 * 4MB) + (VA / NBPG) * sizeof(pt_entry_t)
 *                = 0xbfc00000 + (VA / 4096) * 4
 *
 * what happens if the pmap layer is asked to perform an operation
 * on a pmap that is not the one which is currently active?  in that
 * case we take the PA of the PDP of non-active pmap and put it in
 * slot 1023 of the active pmap.  this causes the non-active pmap's
 * PTEs to get mapped in the final 4MB of the 4GB address space
 * (e.g. starting at 0xffc00000).
 *
 * the following figure shows the effects of the recursive PDP mapping:
 *
 *   PDP (%cr3)
 *   +----+
 *   |   0| -> PTP#0 that maps VA 0x0 -> 0x400000
 *   |    |
 *   |    |
 *   | 767| -> points back to PDP (%cr3) mapping VA 0xbfc00000 -> 0xc0000000
 *   | 768| -> first kernel PTP (maps 0xc0000000 -> 0xf0400000)
 *   |    |
 *   |1023| -> points to alternate pmap's PDP (maps 0xffc00000 -> end)
 *   +----+
 *
 * note that the PDE#767 VA (0xbfc00000) is defined as "PTE_BASE"
 * note that the PDE#1023 VA (0xffc00000) is defined as "APTE_BASE"
 *
 * starting at VA 0xbfc00000 the current active PDP (%cr3) acts as a
 * PTP:
 *
 * PTP#767 == PDP(%cr3) => maps VA 0xbfc00000 -> 0xc0000000
 *   +----+
 *   |   0| -> maps the contents of PTP#0 at VA 0xbfc00000->0xbfc01000
 *   |    |
 *   |    |
 *   | 767| -> maps contents of PTP#767 (the PDP) at VA 0xbffbf000
 *   | 768| -> maps contents of first kernel PTP
 *   |    |
 *   |1023|
 *   +----+
 *
 * note that mapping of the PDP at PTP#767's VA (0xbffbf000) is
 * defined as "PDP_BASE".... within that mapping there are two
 * defines:
 *   "PDP_PDE" (0xbffbfefc) is the VA of the PDE in the PDP
 *      which points back to itself.
 *   "APDP_PDE" (0xbffbfffc) is the VA of the PDE in the PDP which
 *      establishes the recursive mapping of the alternate pmap.
 *      to set the alternate PDP, one just has to put the correct
 *	PA info in *APDP_PDE.
 *
 * note that in the APTE_BASE space, the APDP appears at VA
 * "APDP_BASE" (0xfffff000).
 */
/* XXX MP should we allocate one APDP_PDE per processor?? */

/*
 * the following defines identify the slots used as described above.
 */

#define PDSLOT_PTE	((KERNBASE/NBPD)-1) /* 767: for recursive PDP map */
#define PDSLOT_KERN	(KERNBASE/NBPD)	    /* 768: start of kernel space */
#define PDSLOT_APTE	((unsigned)1023) /* 1023: alternative recursive slot */

/*
 * the following defines give the virtual addresses of various MMU
 * data structures:
 * PTE_BASE and APTE_BASE: the base VA of the linear PTE mappings
 * PTD_BASE and APTD_BASE: the base VA of the recursive mapping of the PTD
 * PDP_PDE and APDP_PDE: the VA of the PDE that points back to the PDP/APDP
 */

#define PTE_BASE	((pt_entry_t *)  (PDSLOT_PTE * NBPD) )
#define APTE_BASE	((pt_entry_t *)  (PDSLOT_APTE * NBPD) )
#define PDP_BASE ((pd_entry_t *)(((char *)PTE_BASE) + (PDSLOT_PTE * NBPG)))
#define APDP_BASE ((pd_entry_t *)(((char *)APTE_BASE) + (PDSLOT_APTE * NBPG)))
#define PDP_PDE		(PDP_BASE + PDSLOT_PTE)
#define APDP_PDE	(PDP_BASE + PDSLOT_APTE)

/*
 * XXXCDC: tmp xlate from old names:
 * PTDPTDI -> PDSLOT_PTE
 * KPTDI -> PDSLOT_KERN
 * APTDPTDI -> PDSLOT_APTE
 */

/*
 * the follow define determines how many PTPs should be set up for the
 * kernel by locore.s at boot time.  this should be large enough to
 * get the VM system running.  once the VM system is running, the
 * pmap module can add more PTPs to the kernel area on demand.
 */

#ifndef NKPTP
#define NKPTP		4	/* 16MB to start */
#endif
#define NKPTP_MIN	4	/* smallest value we allow */
#define NKPTP_MAX	(1024 - (KERNBASE/NBPD) - 1)
				/* largest value (-1 for APTP space) */

/*
 * pdei/ptei: generate index into PDP/PTP from a VA
 */
#define	pdei(VA)	(((VA) & PD_MASK) >> PDSHIFT)
#define	ptei(VA)	(((VA) & PT_MASK) >> PGSHIFT)

/*
 * PTP macros:
 *   a PTP's index is the PD index of the PDE that points to it
 *   a PTP's offset is the byte-offset in the PTE space that this PTP is at
 *   a PTP's VA is the first VA mapped by that PTP
 *
 * note that NBPG == number of bytes in a PTP (4096 bytes == 1024 entries)
 *           NBPD == number of bytes a PTP can map (4MB)
 */

#define ptp_i2o(I)	((I) * NBPG)	/* index => offset */
#define ptp_o2i(O)	((O) / NBPG)	/* offset => index */
#define ptp_i2v(I)	((I) * NBPD)	/* index => VA */
#define ptp_v2i(V)	((V) / NBPD)	/* VA => index (same as pdei) */

/*
 * PG_AVAIL usage: we make use of the ignored bits of the PTE
 */

#define PG_W		PG_AVAIL1	/* "wired" mapping */
#define PG_PVLIST	PG_AVAIL2	/* mapping has entry on pvlist */
/* PG_AVAIL3 not used */

/*
 * Number of PTE's per cache line.  4 byte pte, 32-byte cache line
 * Used to avoid false sharing of cache lines.
 */
#define NPTECL			8

#ifdef _KERNEL
/*
 * pmap data structures: see pmap.c for details of locking.
 */

struct pmap;
typedef struct pmap *pmap_t;

/*
 * we maintain a list of all non-kernel pmaps
 */

LIST_HEAD(pmap_head, pmap); /* struct pmap_head: head of a pmap list */

/*
 * the pmap structure
 *
 * note that the pm_obj contains the simple_lock, the reference count,
 * page list, and number of PTPs within the pmap.
 *
 * XXX If we ever support processor numbers higher than 31, we'll have
 * XXX to rethink the CPU mask.
 */

struct pmap {
	struct uvm_object pm_obj;	/* object (lck by object lock) */
#define	pm_lock	pm_obj.vmobjlock
	LIST_ENTRY(pmap) pm_list;	/* list (lck by pm_list lock) */
	pd_entry_t *pm_pdir;		/* VA of PD (lck by object lock) */
	u_int32_t pm_pdirpa;		/* PA of PD (read-only after create) */
	struct vm_page *pm_ptphint;	/* pointer to a PTP in our pmap */
	struct pmap_statistics pm_stats;  /* pmap stats (lck by object lock) */

	int pm_flags;			/* see below */

	union descriptor *pm_ldt;	/* user-set LDT */
	int pm_ldt_len;			/* number of LDT entries */
	int pm_ldt_sel;			/* LDT selector */
	u_int32_t pm_cpus;		/* mask of CPUs using pmap */
};

/* pm_flags */
#define	PMF_USER_LDT	0x01	/* pmap has user-set LDT */

/*
 * for each managed physical page we maintain a list of <PMAP,VA>'s
 * which it is mapped at.  the list is headed by a pv_head structure.
 * there is one pv_head per managed phys page (allocated at boot time).
 * the pv_head structure points to a list of pv_entry structures (each
 * describes one mapping).
 */

struct pv_entry;

struct pv_head {
	simple_lock_data_t pvh_lock;	/* locks every pv on this list */
	struct pv_entry *pvh_list;	/* head of list (locked by pvh_lock) */
};

struct pv_entry {			/* locked by its list's pvh_lock */
	struct pv_entry *pv_next;	/* next entry */
	struct pmap *pv_pmap;		/* the pmap */
	vaddr_t pv_va;			/* the virtual address */
	struct vm_page *pv_ptp;		/* the vm_page of the PTP */
};

/*
 * pv_entrys are dynamically allocated in chunks from a single page.
 * we keep track of how many pv_entrys are in use for each page and
 * we can free pv_entry pages if needed.  there is one lock for the
 * entire allocation system.
 */

struct pv_page_info {
	TAILQ_ENTRY(pv_page) pvpi_list;
	struct pv_entry *pvpi_pvfree;
	int pvpi_nfree;
};

/*
 * number of pv_entry's in a pv_page
 * (note: won't work on systems where NPBG isn't a constant)
 */

#define PVE_PER_PVPAGE ((NBPG - sizeof(struct pv_page_info)) / \
			sizeof(struct pv_entry))

/*
 * a pv_page: where pv_entrys are allocated from
 */

struct pv_page {
	struct pv_page_info pvinfo;
	struct pv_entry pvents[PVE_PER_PVPAGE];
};

/*
 * global kernel variables
 */

/* PTDpaddr: is the physical address of the kernel's PDP */
extern u_long PTDpaddr;

extern struct pmap kernel_pmap_store;	/* kernel pmap */
extern int nkpde;			/* current # of PDEs for kernel */
extern int pmap_pg_g;			/* do we support PG_G? */

/*
 * macros
 */

#define	pmap_kernel()			(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)
#define	pmap_update()			/* nothing (yet) */

#define pmap_clear_modify(pg)		pmap_clear_attrs(pg, PG_M)
#define pmap_clear_reference(pg)	pmap_clear_attrs(pg, PG_U)
#define pmap_copy(DP,SP,D,L,S)		
#define pmap_is_modified(pg)		pmap_test_attrs(pg, PG_M)
#define pmap_is_referenced(pg)		pmap_test_attrs(pg, PG_U)
#define pmap_move(DP,SP,D,L,S)		
#define pmap_phys_address(ppn)		i386_ptob(ppn)
#define pmap_valid_entry(E) 		((E) & PG_V) /* is PDE or PTE valid? */


/*
 * prototypes
 */

void		pmap_activate __P((struct proc *));
void		pmap_bootstrap __P((vaddr_t));
boolean_t	pmap_clear_attrs __P((struct vm_page *, int));
void		pmap_deactivate __P((struct proc *));
static void	pmap_page_protect __P((struct vm_page *, vm_prot_t));
void		pmap_page_remove  __P((struct vm_page *));
static void	pmap_protect __P((struct pmap *, vaddr_t,
				vaddr_t, vm_prot_t));
void		pmap_remove __P((struct pmap *, vaddr_t, vaddr_t));
boolean_t	pmap_test_attrs __P((struct vm_page *, int));
static void	pmap_update_pg __P((vaddr_t));
static void	pmap_update_2pg __P((vaddr_t,vaddr_t));
void		pmap_write_protect __P((struct pmap *, vaddr_t,
				vaddr_t, vm_prot_t));

vaddr_t reserve_dumppages __P((vaddr_t)); /* XXX: not a pmap fn */

void	pmap_tlb_shootdown __P((pmap_t, vaddr_t, pt_entry_t, int32_t *));
void	pmap_tlb_shootnow __P((int32_t));
void	pmap_do_tlb_shootdown __P((struct cpu_info *));

#define PMAP_GROWKERNEL		/* turn on pmap_growkernel interface */

/*
 * Do idle page zero'ing uncached to avoid polluting the cache.
 */
boolean_t			pmap_pageidlezero __P((paddr_t));
#define	PMAP_PAGEIDLEZERO(pa)	pmap_pageidlezero((pa))

/*
 * inline functions
 */

/*
 * pmap_update_pg: flush one page from the TLB (or flush the whole thing
 *	if hardware doesn't support one-page flushing)
 */

__inline static void
pmap_update_pg(va)
	vaddr_t va;
{
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386)
		tlbflush();
	else
#endif
		invlpg((u_int) va);
}

/*
 * pmap_update_2pg: flush two pages from the TLB
 */

__inline static void
pmap_update_2pg(va, vb)
	vaddr_t va, vb;
{
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386)
		tlbflush();
	else
#endif
	{
		invlpg((u_int) va);
		invlpg((u_int) vb);
	}
}

/*
 * pmap_page_protect: change the protection of all recorded mappings
 *	of a managed page
 *
 * => this function is a frontend for pmap_page_remove/pmap_clear_attrs
 * => we only have to worry about making the page more protected.
 *	unprotecting a page is done on-demand at fault time.
 */

__inline static void
pmap_page_protect(pg, prot)
	struct vm_page *pg;
	vm_prot_t prot;
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ|VM_PROT_EXECUTE)) {
			(void) pmap_clear_attrs(pg, PG_RW);
		} else {
			pmap_page_remove(pg);
		}
	}
}

/*
 * pmap_protect: change the protection of pages in a pmap
 *
 * => this function is a frontend for pmap_remove/pmap_write_protect
 * => we only have to worry about making the page more protected.
 *	unprotecting a page is done on-demand at fault time.
 */

__inline static void
pmap_protect(pmap, sva, eva, prot)
	struct pmap *pmap;
	vaddr_t sva, eva;
	vm_prot_t prot;
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ|VM_PROT_EXECUTE)) {
			pmap_write_protect(pmap, sva, eva, prot);
		} else {
			pmap_remove(pmap, sva, eva);
		}
	}
}

/*
 * various address inlines
 *
 *  vtopte: return a pointer to the PTE mapping a VA, works only for
 *  user and PT addresses
 *
 *  kvtopte: return a pointer to the PTE mapping a kernel VA
 */

#include <lib/libkern/libkern.h>

static __inline pt_entry_t *
vtopte(vaddr_t va)
{

	KASSERT(va < (PDSLOT_KERN << PDSHIFT));

	return (PTE_BASE + i386_btop(va));
}

static __inline pt_entry_t *
kvtopte(vaddr_t va)
{

	KASSERT(va >= (PDSLOT_KERN << PDSHIFT));

#ifdef LARGEPAGES
	{
		pd_entry_t *pde;

		pde = PDP_BASE + pdei(va);
		if (*pde & PG_PS)
			return ((pt_entry_t *)pde);
	}
#endif

	return (PTE_BASE + i386_btop(va));
}

paddr_t vtophys __P((vaddr_t));
vaddr_t	pmap_map __P((vaddr_t, paddr_t, paddr_t, vm_prot_t));

#if defined(USER_LDT)
void	pmap_ldt_cleanup __P((struct proc *));
#define	PMAP_FORK
#endif /* USER_LDT */

#endif /* _KERNEL */
#endif	/* _I386_PMAP_H_ */
