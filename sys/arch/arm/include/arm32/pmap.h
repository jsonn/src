/*	$NetBSD: pmap.h,v 1.10.2.6 2002/03/16 15:56:09 jdolecek Exp $	*/

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe
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

#ifndef	_ARM32_PMAP_H_
#define	_ARM32_PMAP_H_

#ifdef _KERNEL

#include <arm/cpufunc.h>
#include <arm/arm32/pte.h>
#include <uvm/uvm_object.h>

/*
 * a pmap describes a processes' 4GB virtual address space.  this
 * virtual address space can be broken up into 4096 1MB regions which
 * are described by PDEs in the PDP.  the PDEs are defined as follows:
 *
 * (ranges are inclusive -> exclusive, just like vm_map_entry start/end)
 * (the following assumes that KERNBASE is 0xf0000000)
 *
 * PDE#s	VA range		usage
 * 0->3835	0x0 -> 0xefc00000	user address space
 * 3836->3839	0xefc00000->		recursive mapping of PDP (used for
 *			0xf0000000	linear mapping of PTPs)
 * 3840->3851	0xf0000000->		kernel text address space (constant
 *			0xf0c00000	across all pmap's/processes)
 * 3852->3855	0xf0c00000->		"alternate" recursive PDP mapping
 *			0xf1000000	(for other pmaps)
 * 3856->4095	0xf1000000->		KVM and device mappings, constant
 *			0x00000000	across all pmaps
 *
 * The maths works out that to then map each 1MB block into 4k pages requires
 * 256 entries, of 4 bytes each, totaling 1k per 1MB.  However as we use 4k
 * pages we allocate 4 PDE's at a time, allocating the same access permissions
 * to them all.  This means we only need 1024 entries in the page table page
 * table, IE we use 1 4k page to linearly map all the other page tables used.
 */

/*
 * Data structures used by pmap
 */

/*
 * Structure that describes a Level 1 page table and the flags
 * associated with it.
 */
struct l1pt {
	SIMPLEQ_ENTRY(l1pt)	pt_queue;	/* Queue pointers */
	struct pglist		pt_plist;	/* Allocated page list */
	vaddr_t			pt_va;		/* Allocated virtual address */
	int	                pt_flags;	/* Flags */
};
#define	PTFLAG_STATIC		1		/* Statically allocated */
#define PTFLAG_KPT		2		/* Kernel pt's are mapped */
#define PTFLAG_CLEAN		4		/* L1 is clean */

/*
 * we maintain a list of all non-kernel pmaps
 */

LIST_HEAD(pmap_head, pmap); /* struct pmap_head: head of a pmap list */

/*
 * The pmap structure itself.
 */
struct pmap {
	struct uvm_object	pm_obj;		/* uvm_object */
#define	pm_lock	pm_obj.vmobjlock	
	LIST_ENTRY(pmap)	pm_list;	/* list (lck by pm_list lock) */
	pd_entry_t		*pm_pdir;	/* KVA of page directory */
	struct l1pt		*pm_l1pt;	/* L1 descriptor */
	paddr_t                 pm_pptpt;	/* PA of pt's page table */
	vaddr_t                 pm_vptpt;	/* VA of pt's page table */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	struct vm_page *pm_ptphint;		/* pointer to a PTP in our pmap */
};

typedef struct pmap *pmap_t;

/*
 * Page hooks. I'll eliminate these sometime soon :-)
 *
 * For speed we store the both the virtual address and the page table
 * entry address for each page hook.
 */
typedef struct {
	vaddr_t va;
	pt_entry_t *pte;
} pagehook_t;

/*
 * Physical / virtual address structure. In a number of places (particularly
 * during bootstrapping) we need to keep track of the physical and virtual
 * addresses of various pages
 */
typedef struct pv_addr {
	SLIST_ENTRY(pv_addr) pv_list;
	paddr_t pv_pa;
	vaddr_t pv_va;
} pv_addr_t;

/*
 * Determine various modes for PTEs (user vs. kernel, cacheable
 * vs. non-cacheable).
 */
#define	PTE_KERNEL	0
#define	PTE_USER	1
#define	PTE_NOCACHE	0
#define	PTE_CACHE	1

/*
 * _KERNEL specific macros, functions and prototypes
 */

#ifdef  _KERNEL

/*
 * Commonly referenced structures
 */
extern struct pmap	kernel_pmap_store;
extern int		pmap_debug_level; /* Only exists if PMAP_DEBUG */

/*
 * Macros that we need to export
 */
#define pmap_kernel()			(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)

#define	pmap_is_modified(pg)		(((pg)->mdpage.pvh_attrs & PT_M) != 0)
#define	pmap_is_referenced(pg)		(((pg)->mdpage.pvh_attrs & PT_H) != 0)

#define pmap_phys_address(ppn)		(arm_page_to_byte((ppn)))

/*
 * Functions that we need to export
 */
extern vaddr_t pmap_map __P((vaddr_t, vaddr_t, vaddr_t, int));
extern void pmap_procwr __P((struct proc *, vaddr_t, int));
#define	PMAP_NEED_PROCWR
#define PMAP_GROWKERNEL		/* turn on pmap_growkernel interface */

/*
 * Functions we use internally
 */
void pmap_bootstrap __P((pd_entry_t *, pv_addr_t));
void pmap_debug	__P((int));
int pmap_handled_emulation __P((struct pmap *, vaddr_t));
int pmap_modified_emulation __P((struct pmap *, vaddr_t));
void pmap_postinit __P((void));
pt_entry_t *pmap_pte __P((struct pmap *, vaddr_t));

/* Bootstrapping routines. */
void	pmap_map_section(vaddr_t, vaddr_t, paddr_t, int, int);
void	pmap_map_entry(vaddr_t, vaddr_t, paddr_t, int, int);
vsize_t	pmap_map_chunk(vaddr_t, vaddr_t, paddr_t, vsize_t, int, int);
void	pmap_link_l2pt(vaddr_t, vaddr_t, pv_addr_t *);

/*
 * Special page zero routine for use by the idle loop (no cache cleans). 
 */
boolean_t	pmap_pageidlezero __P((paddr_t));
#define PMAP_PAGEIDLEZERO(pa)	pmap_pageidlezero((pa))

/*
 * The current top of kernel VM
 */
extern vaddr_t	pmap_curmaxkvaddr;

#endif	/* _KERNEL */

/*
 * Useful macros and constants 
 */

/* Virtual address to page table entry */
#define vtopte(va) \
	((pt_entry_t *)(PROCESS_PAGE_TBLS_BASE + \
	(arm_byte_to_page((unsigned int)(va)) << 2)))

/* Virtual address to physical address */
#define vtophys(va) \
	((*vtopte(va) & PG_FRAME) | ((unsigned int)(va) & ~PG_FRAME))

/* L1 and L2 page table macros */
#define pmap_pdei(v)	((v & PD_MASK) >> PDSHIFT)
#define pmap_pde(m, v) (&((m)->pm_pdir[pmap_pdei(v)]))
#define pmap_pte_pa(pte)	(*(pte) & PG_FRAME)
#define pmap_pde_v(pde)		(*(pde) != 0)
#define pmap_pde_section(pde)	((*(pde) & L1_MASK) == L1_SECTION)
#define pmap_pde_page(pde)	((*(pde) & L1_MASK) == L1_PAGE)
#define pmap_pde_fpage(pde)	((*(pde) & L1_MASK) == L1_FPAGE)

#define pmap_pte_v(pte)		(*(pte) != 0)

/* Size of the kernel part of the L1 page table */
#define KERNEL_PD_SIZE	\
	(PD_SIZE - (KERNEL_SPACE_START >> PDSHIFT) * sizeof(pd_entry_t))

/*
 * tell MI code that the cache is virtually-indexed *and* virtually-tagged.
 */

#define PMAP_CACHE_VIVT

#endif /* _KERNEL */

#endif	/* _ARM32_PMAP_H_ */
