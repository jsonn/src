/*	$NetBSD: pmap.c,v 1.44.2.2 1997/11/19 21:22:22 mellon Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
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
 *	@(#)pmap.c	7.7 (Berkeley)	5/12/91
 */

/*
 * Derived originally from an old hp300 version by Mike Hibler.  The version
 * by William Jolitz has been heavily modified to allow non-contiguous
 * mapping of physical memory by Wolfgang Solfrank, and to fix several bugs
 * and greatly speedup it up by Charles Hannum.
 * 
 * A recursive map [a pde which points to the page directory] is used to map
 * the page tables using the pagetables themselves. This is done to reduce
 * the impact on kernel virtual memory for lots of sparse address space, and
 * to reduce the cost of memory to each process.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>

#include <dev/isa/isareg.h>
#include <i386/isa/isa_machdep.h>

#ifdef DEBUG
void	pmap_pvdump __P((vm_offset_t pa));
void	pads __P((pmap_t pm));	/* print address space of pmap*/

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
} enter_stats;
struct {
	int calls;
	int removes;
	int pvfirst;
	int pvsearch;
	int ptinvalid;
	int uflushes;
	int sflushes;
} remove_stats;

int pmapdebug = 0 /* 0xffff */;
#define	PDB_FOLLOW	0x0001
#define	PDB_INIT	0x0002
#define	PDB_ENTER	0x0004
#define	PDB_REMOVE	0x0008
#define	PDB_CREATE	0x0010
#define	PDB_PTPAGE	0x0020
#define	PDB_CACHE	0x0040
#define	PDB_BITS	0x0080
#define	PDB_COLLECT	0x0100
#define	PDB_PROTECT	0x0200
#define	PDB_PDRTAB	0x0400
#define	PDB_PARANOIA	0x2000
#define	PDB_WIRING	0x4000
#define	PDB_PVDUMP	0x8000
#endif

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define	pmap_pde(m, v)	(&((m)->pm_pdir[((vm_offset_t)(v) >> PDSHIFT)&1023]))

/*
 * Empty PTEs and PDEs are always 0, but checking only the valid bit allows
 * the compiler to generate `testb' rather than `testl'.
 */
#define	pmap_pde_v(pde)			(*(pde) & PG_V)
#define	pmap_pte_pa(pte)		(*(pte) & PG_FRAME)
#define	pmap_pte_w(pte)			(*(pte) & PG_W)
#define	pmap_pte_m(pte)			(*(pte) & PG_M)
#define	pmap_pte_u(pte)			(*(pte) & PG_U)
#define	pmap_pte_v(pte)			(*(pte) & PG_V)
#define	pmap_pte_set_w(pte, v)		((v) ? (*(pte) |= PG_W) : (*(pte) &= ~PG_W))
#define	pmap_pte_set_prot(pte, v)	((*(pte) &= ~PG_PROT), (*(pte) |= (v)))

/*
 * Given a map and a machine independent protection code,
 * convert to a vax protection code.
 */
pt_entry_t	protection_codes[8];

struct pmap	kernel_pmap_store;

vm_offset_t    	avail_start;	/* PA of first available physical page */
vm_offset_t	avail_end;	/* PA of last available physical page */
vm_offset_t	virtual_avail;  /* VA of first avail page (after kernel bss)*/
vm_offset_t	virtual_end;	/* VA of last avail page (end of kernel AS) */
int		npages;

boolean_t	pmap_initialized = FALSE;	/* Has pmap_init completed? */
char		*pmap_attributes;	/* reference and modify bits */
TAILQ_HEAD(pv_page_list, pv_page) pv_page_freelist;
int		pv_nfree;

pt_entry_t *pmap_pte __P((pmap_t, vm_offset_t));
struct pv_entry * pmap_alloc_pv __P((void));
void pmap_free_pv __P((struct pv_entry *));
void i386_protection_init __P((void));
void pmap_collect_pv __P((void));
__inline void pmap_remove_pv __P((pmap_t, vm_offset_t, u_int));
__inline void pmap_enter_pv __P((pmap_t, vm_offset_t, u_int));
void pmap_deactivate __P((pmap_t, struct pcb *));
void pmap_remove_all __P((vm_offset_t));

#ifdef	NKPDE
int	nkpde = NKPDE;
#else
int	nkpde = 0;
#endif

/*
 * All those kernel PT submaps that BSD is so fond of
 */
pt_entry_t	*CMAP1, *CMAP2;
caddr_t		CADDR1, CADDR2, vmmap;
extern vm_offset_t msgbuf_vaddr, msgbuf_paddr;
#ifdef I586_CPU
extern vm_offset_t pentium_trap_vaddr, pentium_trap_paddr;
extern int pentium_trap_fixup;
#endif

/*
 *	Bootstrap the system enough to run with virtual memory.
 *	Map the kernel's code and data, and allocate the system page table.
 *
 *	On the I386 this is called after mapping has already been enabled
 *	and just syncs the pmap module with what has already been done.
 *	[We can't call it easily with mapping off since the kernel is not
 *	mapped with PA == VA, hence we would have to relocate every address
 *	from the linked base (virtual) address to the actual (physical)
 *	address starting relative to 0]
 */

void
pmap_bootstrap(virtual_start)
	vm_offset_t virtual_start;
{
	vm_offset_t va;
	pt_entry_t *pte, *junk;

	virtual_avail = virtual_start;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize protection array.
	 */
	i386_protection_init();

#ifdef notdef
	/*
	 * Create Kernel page directory table and page maps.
	 * [ currently done in locore. i have wild and crazy ideas -wfj ]
	 */
	bzero(firstaddr, (1+nkpde)*NBPG);
	pmap_kernel()->pm_pdir = firstaddr + VM_MIN_KERNEL_ADDRESS;
	pmap_kernel()->pm_ptab = firstaddr + VM_MIN_KERNEL_ADDRESS + NBPG;

	firstaddr += NBPG;
	for (x = i386_btod(VM_MIN_KERNEL_ADDRESS);
	     x < i386_btod(VM_MIN_KERNEL_ADDRESS) + nkpde; x++) {
		pd_entry_t *pde;
		pde = pmap_kernel()->pm_pdir + x;
		*pde = (firstaddr + x*NBPG) | PG_V | PG_KW;
	}
#else
	pmap_kernel()->pm_pdir =
	    (pd_entry_t *)(proc0.p_addr->u_pcb.pcb_cr3 + KERNBASE);
#endif

	simple_lock_init(&pmap_kernel()->pm_lock);
	pmap_kernel()->pm_count = 1;

	/*
	 * Allocate all the submaps we need
	 */
	va = virtual_avail;
	pte = pmap_pte(pmap_kernel(), va);

#define	SYSMAP(c, p, v, n) \
	do {			\
		v = (c)va;	\
		va += ctob(n);	\
		p = pte;	\
		pte += (n);	\
	} while (0)

	SYSMAP(caddr_t, CMAP1, CADDR1, 1);
	SYSMAP(caddr_t, CMAP2, CADDR2, 1);
	SYSMAP(caddr_t, junk, vmmap, 1);
	SYSMAP(vm_offset_t, junk, msgbuf_vaddr, btoc(MSGBUFSIZE));
	avail_end -= round_page(MSGBUFSIZE);
	msgbuf_paddr = avail_end;
#ifdef I586_CPU
	if (pentium_trap_fixup) {
		SYSMAP(vm_offset_t, junk, pentium_trap_vaddr, 1);
		avail_end -= ctob(1);
		pentium_trap_paddr = avail_end;
	}
#endif

	virtual_avail = va;

	/*
	 * Reserve pmap space for mapping physical pages during dump.
	 */
	virtual_avail = reserve_dumppages(virtual_avail);

	pmap_update();
}

void
pmap_virtual_space(startp, endp)
	vm_offset_t *startp;
	vm_offset_t *endp;
{

	*startp = virtual_avail;
	*endp = virtual_end;
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init()
{
	vm_offset_t addr;
	vm_size_t s;

	if (PAGE_SIZE != NBPG)
		panic("pmap_init: CLSIZE != 1");

	npages = pmap_page_index(avail_end - 1) + 1;
	s = (vm_size_t) (sizeof(struct pv_entry) * npages + npages);
	s = round_page(s);
	addr = (vm_offset_t) kmem_alloc(kernel_map, s);
	pv_table = (struct pv_entry *) addr;
	addr += sizeof(struct pv_entry) * npages;
	pmap_attributes = (char *) addr;
	TAILQ_INIT(&pv_page_freelist);

#ifdef DEBUG
	if (pmapdebug & PDB_INIT)
		printf("pmap_init: %lx bytes (%x pgs): tbl %p attr %p\n",
		       s, npages, pv_table, pmap_attributes);
#endif

	/*
	 * Now it is safe to enable pv_table recording.
	 */
	pmap_initialized = TRUE;
}

struct pv_entry *
pmap_alloc_pv()
{
	struct pv_page *pvp;
	struct pv_entry *pv;
	int i;

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
#endif
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
		kmem_free(kernel_map, (vm_offset_t)pvp, NBPG);
	}
}

__inline void
pmap_enter_pv(pmap, va, pind)
	register pmap_t pmap;
	vm_offset_t va;
	u_int pind;
{	
	register struct pv_entry *pv, *npv;
	int s;

	if (!pmap_initialized)
		return;

	pv = &pv_table[pind];
	s = splimp();

#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("pmap_enter_pv: pv %p: %lx/%p/%p\n",
		       pv, pv->pv_va, pv->pv_pmap, pv->pv_next);
#endif

	if (pv->pv_pmap == NULL) {
		/*
		 * No entries yet, use header as the first entry
		 */
#ifdef DEBUG
		enter_stats.firstpv++;
#endif
		pv->pv_va = va;
		pv->pv_pmap = pmap;
		pv->pv_next = NULL;
	} else {
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
#ifdef DEBUG
		for (npv = pv; npv; npv = npv->pv_next)
			if (pmap == npv->pv_pmap && va == npv->pv_va)
				panic("pmap_enter_pv: already in pv_tab");
#endif
		npv = pmap_alloc_pv();
		npv->pv_va = va;
		npv->pv_pmap = pmap;
		npv->pv_next = pv->pv_next;
		pv->pv_next = npv;
#ifdef DEBUG
		if (!npv->pv_next)
			enter_stats.secondpv++;
#endif
	}
	splx(s);
}

__inline void
pmap_remove_pv(pmap, va, pind)
	register pmap_t pmap;
	vm_offset_t va;
	u_int pind;
{	
	register struct pv_entry *pv, *npv;
	int s;

	/*
	 * Remove from the PV table (raise IPL since we
	 * may be called at interrupt time).
	 */
	pv = &pv_table[pind];
	s = splimp();

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
			pmap_free_pv(npv);
		}
	}
	splx(s);
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

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_map(%lx, %lx, %lx, %x)\n", va, spa, epa, prot);
#endif

	while (spa < epa) {
		pmap_enter(pmap_kernel(), va, spa, prot, FALSE);
		va += NBPG;
		spa += NBPG;
	}
	return va;
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
 *
 * [ just allocate a ptd and mark it uninitialize -- should we track
 *   with a table which process has which ptd? -wfj ]
 */
pmap_t
pmap_create(size)
	vm_size_t size;
{
	register pmap_t pmap;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_create(%lx)\n", size);
#endif

	/*
	 * Software use map does not need a pmap
	 */
	if (size)
		return NULL;

	pmap = (pmap_t) malloc(sizeof *pmap, M_VMPMAP, M_WAITOK);
	bzero(pmap, sizeof(*pmap));
	pmap_pinit(pmap);
	return pmap;
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(pmap)
	register struct pmap *pmap;
{

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_CREATE))
		printf("pmap_pinit(%p)\n", pmap);
#endif

	/*
	 * No need to allocate page table space yet but we do need a
	 * valid page directory table.
	 */
	pmap->pm_pdir = (pd_entry_t *) kmem_alloc(kernel_map, NBPG);

	/* wire in kernel global address entries */
	bcopy(&PTD[KPTDI], &pmap->pm_pdir[KPTDI], nkpde * sizeof(pd_entry_t));

	/* install self-referential address mapping entry */
	pmap->pm_pdir[PTDPTDI] =
	    pmap_extract(pmap_kernel(), (vm_offset_t)pmap->pm_pdir) | PG_V | PG_KW;

	pmap->pm_count = 1;
	simple_lock_init(&pmap->pm_lock);
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap)
	register pmap_t pmap;
{
	int count;

	if (pmap == NULL)
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_destroy(%p)\n", pmap);
#endif

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
	register struct pmap *pmap;
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_release(%p)\n", pmap);
#endif

#ifdef notdef /* DIAGNOSTIC */
	/* sometimes 1, sometimes 0; could rearrange pmap_destroy */
	if (pmap->pm_count != 1)
		panic("pmap_release count");
#endif

	kmem_free(kernel_map, (vm_offset_t)pmap->pm_pdir, NBPG);
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap)
	pmap_t pmap;
{

	if (pmap == NULL)
		return;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_reference(%p)", pmap);
#endif

	simple_lock(&pmap->pm_lock);
	pmap->pm_count++;
	simple_unlock(&pmap->pm_lock);
}

void
pmap_activate(pmap, pcb)
	pmap_t pmap;
	struct pcb *pcb;
{

	if (pmap /*&& pmap->pm_pdchanged */) {
		pcb->pcb_cr3 =
		    pmap_extract(pmap_kernel(), (vm_offset_t)pmap->pm_pdir);
		if (pmap == curproc->p_vmspace->vm_map.pmap)
			lcr3(pcb->pcb_cr3);
		pmap->pm_pdchanged = FALSE;
	}
}

void
pmap_deactivate(pmap, pcb)
	pmap_t pmap;
	struct pcb *pcb;
{
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap, sva, eva)
	struct pmap *pmap;
	register vm_offset_t sva, eva;
{
	register pt_entry_t *pte;
	vm_offset_t pa;
	u_int pind;
	int flush = 0;

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
		/* only check once in a while */
		if ((sva & PT_MASK) == 0) {
			if (!pmap_pde_v(pmap_pde(pmap, sva))) {
				/* We can race ahead here, to the next pde. */
				sva += NBPD;
				pte += i386_btop(NBPD);
				continue;
			}
		}

		if (!pmap_pte_v(pte)) {
#ifdef __GNUC__
			/*
			 * Scan ahead in a tight loop for the next used PTE in
			 * this page.  We don't scan the whole region here
			 * because we don't want to zero-fill unused page table
			 * pages.
			 */
			int n, m;

			n = min(eva - sva, NBPD - (sva & PT_MASK)) >> PGSHIFT;
			__asm __volatile(
			    "cld\n\trepe\n\tscasl\n\tje 1f\n\tincl %1\n\t1:"
			    : "=D" (pte), "=c" (m)
			    : "0" (pte), "1" (n), "a" (0));
			sva += (n - m) << PGSHIFT;
			if (!m)
				continue;
			/* Overshot. */
			--pte;
#else
			goto next;
#endif
		}

		flush = 1;

		/*
		 * Update statistics
		 */
		if (pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		pa = pmap_pte_pa(pte);

		/*
		 * Invalidate the PTEs.
		 * XXX: should cluster them up and invalidate as many
		 * as possible at once.
		 */
#ifdef DEBUG
		if (pmapdebug & PDB_REMOVE)
			printf("remove: inv pte at %p(%x) ", pte, *pte);
#endif

#ifdef needednotdone
reduce wiring count on page table pages as references drop
#endif

		if ((pind = pmap_page_index(pa)) != -1) {
			pmap_attributes[pind] |= *pte & (PG_M | PG_U);
			pmap_remove_pv(pmap, sva, pind);
		}

		*pte = 0;

#ifndef __GNUC__
	next:
#endif
		sva += NBPG;
		pte++;
	}

	if (flush)
		pmap_update();
}

/*
 *	Routine:	pmap_remove_all
 *	Function:
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
	u_int pind;
	int s;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_REMOVE|PDB_PROTECT))
		printf("pmap_remove_all(%lx)", pa);
	/*pmap_pvdump(pa);*/
#endif

	if ((pind = pmap_page_index(pa)) == -1)
		return;

	pv = ph = &pv_table[pind];
	s = splimp();

	if (ph->pv_pmap == NULL) {
		splx(s);
		return;
	}

	while (pv) {
		pmap = pv->pv_pmap;
		pte = pmap_pte(pmap, pv->pv_va);

#ifdef DEBUG
		if (!pte || !pmap_pte_v(pte) || pmap_pte_pa(pte) != pa)
			panic("pmap_remove_all: bad mapping");
#endif

		/*
		 * Update statistics
		 */
		if (pmap_pte_w(pte))
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		/*
		 * Invalidate the PTEs.
		 * XXX: should cluster them up and invalidate as many
		 * as possible at once.
		 */
#ifdef DEBUG
		if (pmapdebug & PDB_REMOVE)
			printf("remove: inv pte at %p(%x) ", pte, *pte);
#endif

#ifdef needednotdone
reduce wiring count on page table pages as references drop
#endif

		/*
		 * Update saved attributes for managed page
		 */
		pmap_attributes[pind] |= *pte & (PG_M | PG_U);

		*pte = 0;

		npv = pv->pv_next;
		if (pv == ph)
			ph->pv_pmap = NULL;
		else
			pmap_free_pv(pv);
		pv = npv;
	}
	splx(s);

	pmap_update();
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap, sva, eva, prot)
	register pmap_t pmap;
	vm_offset_t sva, eva;
	vm_prot_t prot;
{
	register pt_entry_t *pte;
	register int i386prot;
	int flush = 0;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_PROTECT))
		printf("pmap_protect(%p, %lx, %lx, %x)", pmap, sva, eva, prot);
#endif

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	if (prot & VM_PROT_WRITE)
		return;

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
		/* only check once in a while */
		if ((sva & PT_MASK) == 0) {
			if (!pmap_pde_v(pmap_pde(pmap, sva))) {
				/* We can race ahead here, to the next pde. */
				sva += NBPD;
				pte += i386_btop(NBPD);
				continue;
			}
		}

		if (!pmap_pte_v(pte)) {
#ifdef __GNUC__
			/*
			 * Scan ahead in a tight loop for the next used PTE in
			 * this page.  We don't scan the whole region here
			 * because we don't want to zero-fill unused page table
			 * pages.
			 */
			int n, m;

			n = min(eva - sva, NBPD - (sva & PT_MASK)) >> PGSHIFT;
			__asm __volatile(
			    "cld\n\trepe\n\tscasl\n\tje 1f\n\tincl %1\n\t1:"
			    : "=D" (pte), "=c" (m)
			    : "0" (pte), "1" (n), "a" (0));
			sva += (n - m) << PGSHIFT;
			if (!m)
				continue;
			/* Overshot. */
			--pte;
#else
			goto next;
#endif
		}

		flush = 1;

		i386prot = protection_codes[prot];
		if (sva < VM_MAXUSER_ADDRESS)	/* see also pmap_enter() */
			i386prot |= PG_u;
		else if (sva < VM_MAX_ADDRESS)
			i386prot |= PG_u | PG_RW;
		pmap_pte_set_prot(pte, i386prot);

#ifndef __GNUC__
	next:
#endif
		sva += NBPG;
		pte++;
	}

	if (flush)
		pmap_update();
}

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
void
pmap_enter(pmap, va, pa, prot, wired)
	register pmap_t pmap;
	vm_offset_t va;
	register vm_offset_t pa;
	vm_prot_t prot;
	boolean_t wired;
{
	register pt_entry_t *pte;
	register pt_entry_t npte;
	u_int pind;
	int flush = 0;
	boolean_t cacheable;

#ifdef DEBUG
	if (pmapdebug & (PDB_FOLLOW|PDB_ENTER))
		printf("pmap_enter(%p, %lx, %lx, %x, %x)",
		       pmap, va, pa, prot, wired);
#endif

	if (pmap == NULL)
		return;

	if (va >= VM_MAX_KERNEL_ADDRESS)
		panic("pmap_enter: too big");
	/* also, should not muck with PTD va! */

#ifdef DEBUG
	if (pmap == pmap_kernel())
		enter_stats.kernel++;
	else
		enter_stats.user++;
#endif

	/*
	 * Page Directory table entry not valid, we need a new PT page
	 */
	pte = pmap_pte(pmap, va);
	if (!pte)
		panic("ptdi %x", pmap->pm_pdir[PTDPTDI]);

#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("enter: pte %p, *pte %x ", pte, *pte);
#endif

	if (pmap_pte_v(pte)) {
		register vm_offset_t opa;

		/*
		 * Check for wiring change and adjust statistics.
		 */
		if ((wired && !pmap_pte_w(pte)) ||
		    (!wired && pmap_pte_w(pte))) {
			/*
			 * We don't worry about wiring PT pages as they remain
			 * resident as long as there are valid mappings in them.
			 * Hence, if a user page is wired, the PT page will be also.
			 */
#ifdef DEBUG
			if (pmapdebug & PDB_ENTER)
				printf("enter: wiring change -> %x ", wired);
#endif
			if (wired)
				pmap->pm_stats.wired_count++;
			else
				pmap->pm_stats.wired_count--;
#ifdef DEBUG
			enter_stats.wchange++;
#endif
		}

		flush = 1;
		opa = pmap_pte_pa(pte);

		/*
		 * Mapping has not changed, must be protection or wiring change.
		 */
		if (opa == pa) {
#ifdef DEBUG
			enter_stats.pwchange++;
#endif
			goto validate;
		}
		
		/*
		 * Mapping has changed, invalidate old range and fall through to
		 * handle validating new mapping.
		 */
#ifdef DEBUG
		if (pmapdebug & PDB_ENTER)
			printf("enter: removing old mapping %lx pa %lx ",
			       va, opa);
#endif
		if ((pind = pmap_page_index(opa)) != -1) {
			pmap_attributes[pind] |= *pte & (PG_M | PG_U);
			pmap_remove_pv(pmap, va, pind);
		}
#ifdef DEBUG
		enter_stats.mchange++;
#endif
	} else {
		/*
		 * Increment counters
		 */
		pmap->pm_stats.resident_count++;
		if (wired)
			pmap->pm_stats.wired_count++;
	}

	/*
	 * Enter on the PV list if part of our managed memory
	 */
	if ((pind = pmap_page_index(pa)) != -1) {
#ifdef DEBUG
		enter_stats.managed++;
#endif
		pmap_enter_pv(pmap, va, pind);
		cacheable = TRUE;
	} else if (pmap_initialized) {
#ifdef DEBUG
		enter_stats.unmanaged++;
#endif
		/*
		 * Assumption: if it is not part of our managed memory
		 * then it must be device memory which may be volatile.
		 */
		cacheable = FALSE;
	}

validate:
	/*
	 * Now validate mapping with desired protection/wiring.
	 * Assume uniform modified and referenced status for all
	 * I386 pages in a MACH page.
	 */
	npte = (pa & PG_FRAME) | protection_codes[prot] | PG_V;
	if (wired)
		npte |= PG_W;

	if (va < VM_MAXUSER_ADDRESS)	/* i.e. below USRSTACK */
		npte |= PG_u;
	else if (va < VM_MAX_ADDRESS)
		/*
		 * Page tables need to be user RW, for some reason, and the
		 * user area must be writable too.  Anything above
		 * VM_MAXUSER_ADDRESS is protected from user access by
		 * the user data and code segment descriptors, so this is OK.
		 */
		npte |= PG_u | PG_RW;

#ifdef DEBUG
	if (pmapdebug & PDB_ENTER)
		printf("enter: new pte value %x ", npte);
#endif

	*pte = npte;
	if (flush)
		pmap_update();
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(phys, prot)
	vm_offset_t     phys;
	vm_prot_t       prot;
{

	switch (prot) {
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
 *	Routine:	pmap_change_wiring
 *	Function:	Change the wiring attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void
pmap_change_wiring(pmap, va, wired)
	register pmap_t pmap;
	vm_offset_t va;
	boolean_t wired;
{
	register pt_entry_t *pte;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_change_wiring(%p, %lx, %x)", pmap, va, wired);
#endif

	pte = pmap_pte(pmap, va);
	if (!pte)
		return;

#ifdef DEBUG
	/*
	 * Page not valid.  Should this ever happen?
	 * Just continue and change wiring anyway.
	 */
	if (!pmap_pte_v(pte)) {
		if (pmapdebug & PDB_PARANOIA)
			printf("pmap_change_wiring: invalid PTE for %lx ", va);
	}
#endif

	if ((wired && !pmap_pte_w(pte)) || (!wired && pmap_pte_w(pte))) {
		if (wired)
			pmap->pm_stats.wired_count++;
		else
			pmap->pm_stats.wired_count--;
		pmap_pte_set_w(pte, wired);
	}
}

/*
 *	Routine:	pmap_pte
 *	Function:
 *		Extract the page table entry associated
 *		with the given map/virtual_address pair.
 */
pt_entry_t *
pmap_pte(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{
	pt_entry_t *ptp;

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_pte(%p, %lx) ->\n", pmap, va);
#endif

	if (!pmap || !pmap_pde_v(pmap_pde(pmap, va)))
		return NULL;

	if ((pmap->pm_pdir[PTDPTDI] & PG_FRAME) == (PTDpde & PG_FRAME) ||
	    pmap == pmap_kernel())
		/* current address space or kernel */
		ptp = PTmap;
	else {
		/* alternate address space */
		if ((pmap->pm_pdir[PTDPTDI] & PG_FRAME) != (APTDpde & PG_FRAME)) {
			APTDpde = pmap->pm_pdir[PTDPTDI];
			pmap_update();
		}
		ptp = APTmap;
	}

	return ptp + i386_btop(va);
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
vm_offset_t
pmap_extract(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{
	register pt_entry_t *pte;
	register vm_offset_t pa;

#ifdef DEBUGx
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_extract(%x, %x) -> ", pmap, va);
#endif

	pte = pmap_pte(pmap, va);
	if (!pte)
		return NULL;
	if (!pmap_pte_v(pte))
		return NULL;

	pa = pmap_pte_pa(pte);
#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("%lx\n", pa);
#endif
	return pa | (va & ~PG_FRAME);
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
	pmap_t dst_pmap, src_pmap;
	vm_offset_t dst_addr, src_addr;
	vm_size_t len;
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy(%p, %p, %lx, %lx, %lx)",
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
 * [ needs to be written -wfj ]  XXXX
 */
void
pmap_collect(pmap)
	pmap_t pmap;
{
#ifdef DEBUG
	if (pmapdebug & PDB_COLLECT)
		printf("pmap_collect(%p)\n", pmap);
#endif

	if (pmap != pmap_kernel())
		return;

}

#if 0
void
pmap_dump_pvlist(phys, m)
	vm_offset_t phys;
	char *m;
{
	register struct pv_entry *pv;

	if (!pmap_initialized)
		return;
	printf("%s %08lx:", m, phys);
	pv = &pv_table[pmap_page_index(phys)];
	if (pv->pv_pmap == NULL) {
		printf(" no mappings\n");
		return;
	}
	for (; pv; pv = pv->pv_next)
		printf(" pmap %08p va %08lx", pv->pv_pmap, pv->pv_va);
	printf("\n");
}
#else
#define	pmap_dump_pvlist(a,b)
#endif

/*
 *	pmap_zero_page zeros the specified by mapping it into
 *	virtual memory and using bzero to clear its contents.
 */
void
pmap_zero_page(phys)
	register vm_offset_t phys;
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_zero_page(%lx)", phys);
#endif

	pmap_dump_pvlist(phys, "pmap_zero_page: phys");
	*CMAP2 = (phys & PG_FRAME) | PG_V | PG_KW /*| PG_N*/;
	pmap_update();
	bzero(CADDR2, NBPG);
}

/*
 *	pmap_copy_page copies the specified page by mapping
 *	it into virtual memory and using bcopy to copy its
 *	contents.
 */
void
pmap_copy_page(src, dst)
	register vm_offset_t src, dst;
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_copy_page(%lx, %lx)", src, dst);
#endif

	pmap_dump_pvlist(src, "pmap_copy_page: src");
	pmap_dump_pvlist(dst, "pmap_copy_page: dst");
	*CMAP1 = (src & PG_FRAME) | PG_V | PG_KR;
	*CMAP2 = (dst & PG_FRAME) | PG_V | PG_KW /*| PG_N*/;
	pmap_update();
	bcopy(CADDR1, CADDR2, NBPG);
}

/*
 *	Routine:	pmap_pageable
 *	Function:
 *		Make the specified pages (by pmap, offset)
 *		pageable (or not) as requested.
 *
 *		A page which is not pageable may not take
 *		a fault; therefore, its page table entry
 *		must remain valid for the duration.
 *
 *		This routine is merely advisory; pmap_enter
 *		will specify that these pages are to be wired
 *		down (or not) as appropriate.
 */
void
pmap_pageable(pmap, sva, eva, pageable)
	pmap_t pmap;
	vm_offset_t sva, eva;
	boolean_t pageable;
{

#ifdef DEBUG
	if (pmapdebug & PDB_FOLLOW)
		printf("pmap_pageable(%p, %lx, %lx, %x)",
		       pmap, sva, eva, pageable);
#endif

	/*
	 * If we are making a PT page pageable then all valid
	 * mappings must be gone from that page.  Hence it should
	 * be all zeros and there is no need to clean it.
	 * Assumptions:
	 *	- we are called with only one page at a time
	 *	- PT pages have only one pv_table entry
	 */
	if (pmap == pmap_kernel() && pageable && sva + NBPG == eva) {
		register vm_offset_t pa;
		register pt_entry_t *pte;

#ifdef DEBUG
		u_int pind;
		register struct pv_entry *pv;

		if ((pmapdebug & (PDB_FOLLOW|PDB_PTPAGE)) == PDB_PTPAGE)
			printf("pmap_pageable(%p, %lx, %lx, %x)",
			       pmap, sva, eva, pageable);
#endif

		pte = pmap_pte(pmap, sva);
		if (!pte)
			return;
		if (!pmap_pte_v(pte))
			return;

		pa = pmap_pte_pa(pte);

#ifdef DEBUG
		if ((pind = pmap_page_index(pa)) == -1)
			return;

		pv = &pv_table[pind];
		if (pv->pv_va != sva || pv->pv_next) {
			printf("pmap_pageable: bad PT page va %lx next %p\n",
			       pv->pv_va, pv->pv_next);
			return;
		}
#endif

		/*
		 * Mark it unmodified to avoid pageout
		 */
		pmap_clear_modify(pa);

#ifdef needsomethinglikethis
		if (pmapdebug & PDB_PTPAGE)
			printf("pmap_pageable: PT page %lx(%lx) unmodified\n",
			       sva, *pmap_pte(pmap, sva));
		if (pmapdebug & PDB_WIRING)
			pmap_check_wiring("pageable", sva);
#endif
	}
}

/*
 * Miscellaneous support routines follow
 */

void
i386_protection_init()
{

	protection_codes[VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE] = 0;
	protection_codes[VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE] =
	protection_codes[VM_PROT_NONE | VM_PROT_READ | VM_PROT_NONE] =
	protection_codes[VM_PROT_NONE | VM_PROT_READ | VM_PROT_EXECUTE] = PG_RO;
	protection_codes[VM_PROT_WRITE | VM_PROT_NONE | VM_PROT_NONE] =
	protection_codes[VM_PROT_WRITE | VM_PROT_NONE | VM_PROT_EXECUTE] =
	protection_codes[VM_PROT_WRITE | VM_PROT_READ | VM_PROT_NONE] =
	protection_codes[VM_PROT_WRITE | VM_PROT_READ | VM_PROT_EXECUTE] = PG_RW;
}

boolean_t
pmap_testbit(pa, setbits)
	register vm_offset_t pa;
	int setbits;
{
	register struct pv_entry *pv;
	register pt_entry_t *pte;
	u_int pind;
	int s;

	if ((pind = pmap_page_index(pa)) == -1)
		return FALSE;

	pv = &pv_table[pind];
	s = splimp();

	/*
	 * Check saved info first
	 */
	if (pmap_attributes[pind] & setbits) {
		splx(s);
		return TRUE;
	}

	/*
	 * Not found, check current mappings returning
	 * immediately if found.
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			pte = pmap_pte(pv->pv_pmap, pv->pv_va);
			if (*pte & setbits) {
				splx(s);
				return TRUE;
			}
		}
	}
	splx(s);
	return FALSE;
}

/*
 * Modify pte bits for all ptes corresponding to the given physical address.
 * We use `maskbits' rather than `clearbits' because we're always passing
 * constants and the latter would require an extra inversion at run-time.
 */
void
pmap_changebit(pa, setbits, maskbits)
	register vm_offset_t pa;
	int setbits, maskbits;
{
	register struct pv_entry *pv;
	register pt_entry_t *pte;
	vm_offset_t va;
	u_int pind;
	int s;

#ifdef DEBUG
	if (pmapdebug & PDB_BITS)
		printf("pmap_changebit(%lx, %x, %x)",
		       pa, setbits, ~maskbits);
#endif

	if ((pind = pmap_page_index(pa)) == -1)
		return;

	pv = &pv_table[pind];
	s = splimp();

	/*
	 * Clear saved attributes (modify, reference)
	 */
	if (~maskbits)
		pmap_attributes[pind] &= maskbits;

	/*
	 * Loop over all current mappings setting/clearing as appropos
	 * If setting RO do we need to clear the VAC?
	 */
	if (pv->pv_pmap != NULL) {
		for (; pv; pv = pv->pv_next) {
			va = pv->pv_va;

			/*
			 * XXX don't write protect pager mappings
			 */
			if ((PG_RO && setbits == PG_RO) ||
			    (PG_RW && maskbits == ~PG_RW)) {
				extern vm_offset_t pager_sva, pager_eva;

				if (va >= pager_sva && va < pager_eva)
					continue;
			}

			pte = pmap_pte(pv->pv_pmap, va);
			*pte = (*pte & maskbits) | setbits;
		}
		pmap_update();
	}
	splx(s);
}

#ifdef DEBUG
void
pmap_pvdump(pa)
	vm_offset_t pa;
{
	register struct pv_entry *pv;

	printf("pa %lx", pa);
	for (pv = &pv_table[pmap_page_index(pa)]; pv; pv = pv->pv_next) {
		printf(" -> pmap %p, va %lx", pv->pv_pmap, pv->pv_va);
		pads(pv->pv_pmap);
	}
	printf(" ");
}

#ifdef notyet
void
pmap_check_wiring(str, va)
	char *str;
	vm_offset_t va;
{
	vm_map_entry_t entry;
	register int count, *pte;

	va = trunc_page(va);
	if (!pmap_pde_v(pmap_pde(pmap_kernel(), va)) ||
	    !pmap_pte_v(pmap_pte(pmap_kernel(), va)))
		return;

	if (!vm_map_lookup_entry(pt_map, va, &entry)) {
		printf("wired_check: entry for %lx not found\n", va);
		return;
	}
	count = 0;
	for (pte = (int *)va; pte < (int *)(va + NBPG); pte++)
		if (*pte)
			count++;
	if (entry->wired_count != count)
		printf("*%s*: %lx: w%d/a%d\n",
		       str, va, entry->wired_count, count);
}
#endif

/* print address space of pmap*/
void
pads(pm)
	pmap_t pm;
{
	unsigned va, i, j;
	register pt_entry_t *pte;

	if (pm == pmap_kernel())
		return;
	for (i = 0; i < 1024; i++) 
		if (pmap_pde_v(&pm->pm_pdir[i]))
			for (j = 0; j < 1024 ; j++) {
				va = (i << PDSHIFT) | (j << PGSHIFT);
				if (pm == pmap_kernel() &&
				    va < VM_MIN_KERNEL_ADDRESS)
					continue;
				if (pm != pmap_kernel() &&
				    va > VM_MAX_ADDRESS)
					continue;
				pte = pmap_pte(pm, va);
				if (pmap_pte_v(pte)) 
					printf("%x:%x ", va, *pte); 
			}
}
#endif
