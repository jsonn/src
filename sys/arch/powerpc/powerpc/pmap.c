/*	$NetBSD: pmap.c,v 1.26.8.1 1999/12/27 18:33:40 wrstuden Exp $	*/

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

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/queue.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <uvm/uvm.h>

#include <machine/pcb.h>
#include <machine/powerpc.h>

pte_t *ptable;
int ptab_cnt;
u_int ptab_mask;
#define	HTABSIZE	(ptab_cnt * 64)

struct pte_ovfl {
	LIST_ENTRY(pte_ovfl) po_list;	/* Linked list of overflow entries */
	struct pte po_pte;		/* PTE for this mapping */
};

LIST_HEAD(pte_ovtab, pte_ovfl) *potable; /* Overflow entries for ptable */

struct pmap kernel_pmap_;

int physmem;
static int npgs;
static u_int nextavail;
#ifndef MSGBUFADDR
extern paddr_t msgbuf_paddr;
#endif

static struct mem_region *mem, *avail;

/*
 * This is a cache of referenced/modified bits.
 * Bits herein are shifted by ATTRSHFT.
 */
static char *pmap_attrib;
#define	ATTRSHFT	4

struct pv_entry {
	struct pv_entry *pv_next;	/* Linked list of mappings */
	int pv_idx;			/* Index into ptable */
	vaddr_t pv_va;			/* virtual address of mapping */
};

struct pv_entry *pv_table;

struct pv_page;
struct pv_page_info {
	LIST_ENTRY(pv_page) pgi_list;
	struct pv_entry *pgi_freelist;
	int pgi_nfree;
};
#define	NPVPPG	((NBPG - sizeof(struct pv_page_info)) / sizeof(struct pv_entry))
struct pv_page {
	struct pv_page_info pvp_pgi;
	struct pv_entry pvp_pv[NPVPPG];
};
LIST_HEAD(pv_page_list, pv_page) pv_page_freelist;
int pv_nfree;
int pv_pcnt;
static struct pv_entry *pmap_alloc_pv __P((void));
static void pmap_free_pv __P((struct pv_entry *));
void pmap_pinit __P((pmap_t));
void pmap_release __P((pmap_t));

struct po_page;
struct po_page_info {
	LIST_ENTRY(po_page) pgi_list;
	vm_page_t pgi_page;
	LIST_HEAD(po_freelist, pte_ovfl) pgi_freelist;
	int pgi_nfree;
};
#define	NPOPPG	((NBPG - sizeof(struct po_page_info)) / sizeof(struct pte_ovfl))
struct po_page {
	struct po_page_info pop_pgi;
	struct pte_ovfl pop_po[NPOPPG];
};
LIST_HEAD(po_page_list, po_page) po_page_freelist;
int po_nfree;
int po_pcnt;
static struct pte_ovfl *poalloc __P((void));
static void pofree __P((struct pte_ovfl *, int));

static u_int usedsr[NPMAPS / sizeof(u_int) / 8];

static int pmap_initialized;

/*
 * These small routines may have to be replaced,
 * if/when we support processors other that the 604.
 */
static inline void
tlbie(ea)
	caddr_t ea;
{
	asm volatile ("tlbie %0" :: "r"(ea));
}

static inline void
tlbsync()
{
	asm volatile ("sync; tlbsync; sync");
}

static void
tlbia()
{
	caddr_t i;
	
	asm volatile ("sync");
	for (i = 0; i < (caddr_t)0x00040000; i += 0x00001000)
		tlbie(i);
	tlbsync();
}

static inline int
ptesr(sr, addr)
	sr_t *sr;
	vaddr_t addr;
{
	return sr[(u_int)addr >> ADDR_SR_SHFT];
}

static inline int
pteidx(sr, addr)
	sr_t sr;
	vaddr_t addr;
{
	int hash;
	
	hash = (sr & SR_VSID) ^ (((u_int)addr & ADDR_PIDX) >> ADDR_PIDX_SHFT);
	return hash & ptab_mask;
}

static inline int
ptematch(ptp, sr, va, which)
	pte_t *ptp;
	sr_t sr;
	vaddr_t va;
	int which;
{
	return ptp->pte_hi
		== (((sr & SR_VSID) << PTE_VSID_SHFT)
		    | (((u_int)va >> ADDR_API_SHFT) & PTE_API)
		    | which);
}

static __inline struct pv_entry *
pa_to_pv(pa)
	paddr_t pa;
{
	int bank, pg;

	bank = vm_physseg_find(atop(pa), &pg);
	if (bank == -1)
		return NULL;
	return &vm_physmem[bank].pmseg.pvent[pg];
}

static __inline char *
pa_to_attr(pa)
	paddr_t pa;
{
	int bank, pg;

	bank = vm_physseg_find(atop(pa), &pg);
	if (bank == -1)
		return NULL;
	return &vm_physmem[bank].pmseg.attrs[pg];
}

/*
 * Try to insert page table entry *pt into the ptable at idx.
 *
 * Note: *pt mustn't have PTE_VALID set.
 * This is done here as required by Book III, 4.12.
 */
static int
pte_insert(idx, pt)
	int idx;
	pte_t *pt;
{
	pte_t *ptp;
	int i;
	
	/*
	 * First try primary hash.
	 */
	for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++)
		if (!(ptp->pte_hi & PTE_VALID)) {
			*ptp = *pt;
			ptp->pte_hi &= ~PTE_HID;
			asm volatile ("sync");
			ptp->pte_hi |= PTE_VALID;
			return 1;
		}
	idx ^= ptab_mask;
	for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++)
		if (!(ptp->pte_hi & PTE_VALID)) {
			*ptp = *pt;
			ptp->pte_hi |= PTE_HID;
			asm volatile ("sync");
			ptp->pte_hi |= PTE_VALID;
			return 1;
		}
	return 0;
}

/*
 * Spill handler.
 *
 * Tries to spill a page table entry from the overflow area.
 * Note that this routine runs in real mode on a separate stack,
 * with interrupts disabled.
 */
int
pte_spill(addr)
	vaddr_t addr;
{
	int idx, i;
	sr_t sr;
	struct pte_ovfl *po;
	pte_t ps;
	pte_t *pt;

	asm ("mfsrin %0,%1" : "=r"(sr) : "r"(addr));
	idx = pteidx(sr, addr);
	for (po = potable[idx].lh_first; po; po = po->po_list.le_next)
		if (ptematch(&po->po_pte, sr, addr, 0)) {
			/*
			 * Now found an entry to be spilled into the real ptable.
			 */
			if (pte_insert(idx, &po->po_pte)) {
				LIST_REMOVE(po, po_list);
				pofree(po, 0);
				return 1;
			}
			/*
			 * Have to substitute some entry. Use the primary hash for this.
			 *
			 * Use low bits of timebase as random generator
			 */
			asm ("mftb %0" : "=r"(i));
			pt = ptable + idx * 8 + (i & 7);
			pt->pte_hi &= ~PTE_VALID;
			ps = *pt;
			asm volatile ("sync");
			tlbie(addr);
			tlbsync();
			*pt = po->po_pte;
			asm volatile ("sync");
			pt->pte_hi |= PTE_VALID;
			po->po_pte = ps;
			if (ps.pte_hi & PTE_HID) {
				/*
				 * We took an entry that was on the alternate hash
				 * chain, so move it to it's original chain.
				 */
				po->po_pte.pte_hi &= ~PTE_HID;
				LIST_REMOVE(po, po_list);
				LIST_INSERT_HEAD(potable + (idx ^ ptab_mask),
						 po, po_list);
			}
			return 1;
		}
	return 0;
}

/*
 * This is called during initppc, before the system is really initialized.
 */
void
pmap_bootstrap(kernelstart, kernelend)
	u_int kernelstart, kernelend;
{
	struct mem_region *mp, *mp1;
	int cnt, i;
	u_int s, e, sz;

	/*
	 * Get memory.
	 */
	mem_regions(&mem, &avail);
	for (mp = mem; mp->size; mp++)
		physmem += btoc(mp->size);

	/*
	 * Count the number of available entries.
	 */
	for (cnt = 0, mp = avail; mp->size; mp++)
		cnt++;

	/*
	 * Page align all regions.
	 * Non-page aligned memory isn't very interesting to us.
	 * Also, sort the entries for ascending addresses.
	 */
	kernelstart &= ~PGOFSET;
	kernelend = (kernelend + PGOFSET) & ~PGOFSET;
	for (mp = avail; mp->size; mp++) {
		s = mp->start;
		e = mp->start + mp->size;
		/*
		 * Check whether this region holds all of the kernel.
		 */
		if (s < kernelstart && e > kernelend) {
			avail[cnt].start = kernelend;
			avail[cnt++].size = e - kernelend;
			e = kernelstart;
		}
		/*
		 * Look whether this regions starts within the kernel.
		 */
		if (s >= kernelstart && s < kernelend) {
			if (e <= kernelend)
				goto empty;
			s = kernelend;
		}
		/*
		 * Now look whether this region ends within the kernel.
		 */
		if (e > kernelstart && e <= kernelend) {
			if (s >= kernelstart)
				goto empty;
			e = kernelstart;
		}
		/*
		 * Now page align the start and size of the region.
		 */
		s = round_page(s);
		e = trunc_page(e);
		if (e < s)
			e = s;
		sz = e - s;
		/*
		 * Check whether some memory is left here.
		 */
		if (sz == 0) {
		empty:
			bcopy(mp + 1, mp,
			      (cnt - (mp - avail)) * sizeof *mp);
			cnt--;
			mp--;
			continue;
		}
		/*
		 * Do an insertion sort.
		 */
		npgs += btoc(sz);
		for (mp1 = avail; mp1 < mp; mp1++)
			if (s < mp1->start)
				break;
		if (mp1 < mp) {
			bcopy(mp1, mp1 + 1, (char *)mp - (char *)mp1);
			mp1->start = s;
			mp1->size = sz;
		} else {
			mp->start = s;
			mp->size = sz;
		}
	}

#ifdef	HTABENTS
	ptab_cnt = HTABENTS;
#else /* HTABENTS */
	ptab_cnt = 1024;
	while ((HTABSIZE << 7) < ctob(physmem))
		ptab_cnt <<= 1;
#endif /* HTABENTS */

	/*
	 * Find suitably aligned memory for HTAB.
	 */
	for (mp = avail; mp->size; mp++) {
		s = roundup(mp->start, HTABSIZE) - mp->start;
		if (mp->size < s + HTABSIZE)
			continue;
		ptable = (pte_t *)(mp->start + s);
		if (mp->size == s + HTABSIZE) {
			if (s)
				mp->size = s;
			else {
				bcopy(mp + 1, mp,
				      (cnt - (mp - avail)) * sizeof *mp);
				mp = avail;
			}
			break;
		}
		if (s != 0) {
			bcopy(mp, mp + 1,
			      (cnt - (mp - avail)) * sizeof *mp);
			mp++->size = s;
			cnt++;
		}
		mp->start += s + HTABSIZE;
		mp->size -= s + HTABSIZE;
		break;
	}
	if (!mp->size)
		panic("not enough memory?");

	npgs -= btoc(HTABSIZE);
	bzero((void *)ptable, HTABSIZE);
	ptab_mask = ptab_cnt - 1;

	/*
	 * We cannot do pmap_steal_memory here,
	 * since we don't run with translation enabled yet.
	 */
	s = sizeof(struct pte_ovtab) * ptab_cnt;
	sz = round_page(s);
	for (mp = avail; mp->size; mp++)
		if (mp->size >= sz)
			break;
	if (!mp->size)
		panic("not enough memory?");

	npgs -= btoc(sz);
	potable = (struct pte_ovtab *)mp->start;
	mp->size -= sz;
	mp->start += sz;
	if (mp->size <= 0)
		bcopy(mp + 1, mp, (cnt - (mp - avail)) * sizeof *mp);
	for (i = 0; i < ptab_cnt; i++)
		LIST_INIT(potable + i);
	LIST_INIT(&pv_page_freelist);

#ifndef MSGBUFADDR
	/*
	 * allow for msgbuf
	 */
	sz = round_page(MSGBUFSIZE);
	mp = NULL;
	for (mp1 = avail; mp1->size; mp1++)
		if (mp1->size >= sz)
			mp = mp1;
	if (mp == NULL)
		panic("not enough memory?");

	npgs -= btoc(sz);
	msgbuf_paddr = mp->start + mp->size - sz;
	mp->size -= sz;
	if (mp->size <= 0)
		bcopy(mp + 1, mp, (cnt - (mp - avail)) * sizeof *mp);
#endif

	for (mp = avail; mp->size; mp++)
		uvm_page_physload(atop(mp->start), atop(mp->start + mp->size),
			atop(mp->start), atop(mp->start + mp->size),
			VM_FREELIST_DEFAULT);

	/*
	 * Initialize kernel pmap and hardware.
	 */
#if NPMAPS >= KERNEL_SEGMENT / 16
	usedsr[KERNEL_SEGMENT / 16 / (sizeof usedsr[0] * 8)]
		|= 1 << ((KERNEL_SEGMENT / 16) % (sizeof usedsr[0] * 8));
#endif
	for (i = 0; i < 16; i++) {
		pmap_kernel()->pm_sr[i] = EMPTY_SEGMENT;
		asm volatile ("mtsrin %0,%1"
			      :: "r"(EMPTY_SEGMENT), "r"(i << ADDR_SR_SHFT));
	}
	pmap_kernel()->pm_sr[KERNEL_SR] = KERNEL_SEGMENT;
	asm volatile ("mtsr %0,%1"
		      :: "n"(KERNEL_SR), "r"(KERNEL_SEGMENT));
	asm volatile ("sync; mtsdr1 %0; isync"
		      :: "r"((u_int)ptable | (ptab_mask >> 10)));
	tlbia();
	nextavail = avail->start;
}

/*
 * Restrict given range to physical memory
 */
void
pmap_real_memory(start, size)
	paddr_t *start;
	psize_t *size;
{
	struct mem_region *mp;
	
	for (mp = mem; mp->size; mp++) {
		if (*start + *size > mp->start
		    && *start < mp->start + mp->size) {
			if (*start < mp->start) {
				*size -= mp->start - *start;
				*start = mp->start;
			}
			if (*start + *size > mp->start + mp->size)
				*size = mp->start + mp->size - *start;
			return;
		}
	}
	*size = 0;
}

/*
 * Initialize anything else for pmap handling.
 * Called during vm_init().
 */
void
pmap_init()
{
	struct pv_entry *pv;
	vsize_t sz;
	vaddr_t addr;
	int i, s;
	int bank;
	char *attr;

	sz = (vsize_t)((sizeof(struct pv_entry) + 1) * npgs);
	sz = round_page(sz);
	/* XXXCDC: ABSOLUTELY WRONG!   uvm_km_zalloc() _CAN_
		return 0 if out of VM */
	addr = uvm_km_zalloc(kernel_map, sz);
	s = splimp();
	pv = pv_table = (struct pv_entry *)addr;
	for (i = npgs; --i >= 0;)
		pv++->pv_idx = -1;
	LIST_INIT(&pv_page_freelist);
	pmap_attrib = (char *)pv;
	bzero(pv, npgs);

	pv = pv_table;
	attr = pmap_attrib;
	for (bank = 0; bank < vm_nphysseg; bank++) {
		sz = vm_physmem[bank].end - vm_physmem[bank].start;
		vm_physmem[bank].pmseg.pvent = pv;
		vm_physmem[bank].pmseg.attrs = attr;
		pv += sz;
		attr += sz;
	}

	pmap_initialized = 1;
	splx(s);
}

/*
 * How much virtual space is available to the kernel?
 */
void
pmap_virtual_space(start, end)
	vaddr_t *start, *end;
{
	/*
	 * Reserve one segment for kernel virtual memory
	 */
	*start = (vaddr_t)(KERNEL_SR << ADDR_SR_SHFT);
	*end = *start + SEGMENT_LENGTH;
}

/*
 * Create and return a physical map.
 */
struct pmap *
pmap_create()
{
	struct pmap *pm;
	
	pm = (struct pmap *)malloc(sizeof *pm, M_VMPMAP, M_WAITOK);
	bzero((caddr_t)pm, sizeof *pm);
	pmap_pinit(pm);
	return pm;
}

/*
 * Initialize a preallocated and zeroed pmap structure.
 */
void
pmap_pinit(pm)
	struct pmap *pm;
{
	int i, j;
	
	/*
	 * Allocate some segment registers for this pmap.
	 */
	pm->pm_refs = 1;
	for (i = 0; i < sizeof usedsr / sizeof usedsr[0]; i++)
		if (usedsr[i] != 0xffffffff) {
			j = ffs(~usedsr[i]) - 1;
			usedsr[i] |= 1 << j;
			pm->pm_sr[0] = (i * sizeof usedsr[0] * 8 + j) * 16;
			for (i = 1; i < 16; i++)
				pm->pm_sr[i] = pm->pm_sr[i - 1] + 1;
			return;
		}
	panic("out of segments");
}

/*
 * Add a reference to the given pmap.
 */
void
pmap_reference(pm)
	struct pmap *pm;
{
	pm->pm_refs++;
}

/*
 * Retire the given pmap from service.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_destroy(pm)
	struct pmap *pm;
{
	if (--pm->pm_refs == 0) {
		pmap_release(pm);
		free((caddr_t)pm, M_VMPMAP);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 */
void
pmap_release(pm)
	struct pmap *pm;
{
	int i, j;
	
	if (!pm->pm_sr[0])
		panic("pmap_release");
	i = pm->pm_sr[0] / 16;
	j = i % (sizeof usedsr[0] * 8);
	i /= sizeof usedsr[0] * 8;
	usedsr[i] &= ~(1 << j);
}

/*
 * Copy the range specified by src_addr/len
 * from the source map to the range dst_addr/len
 * in the destination map.
 *
 * This routine is only advisory and need not do anything.
 */
void
pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	struct pmap *dst_pmap, *src_pmap;
	vaddr_t dst_addr, src_addr;
	vsize_t len;
{
}

/*
 * Require that all active physical maps contain no
 * incorrect entries NOW.
 */
void
pmap_update()
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
pmap_collect(pm)
	struct pmap *pm;
{
}

/*
 * Fill the given physical page with zeroes.
 */
void
pmap_zero_page(pa)
	paddr_t pa;
{
#if 0
	bzero((caddr_t)pa, NBPG);
#else
	int i;

	for (i = NBPG/CACHELINESIZE; i > 0; i--) {
		__asm __volatile ("dcbz 0,%0" :: "r"(pa));
		pa += CACHELINESIZE;
	}
#endif
}

/*
 * Copy the given physical source page to its destination.
 */
void
pmap_copy_page(src, dst)
	paddr_t src, dst;
{
	bcopy((caddr_t)src, (caddr_t)dst, NBPG);
}

static struct pv_entry *
pmap_alloc_pv()
{
	struct pv_page *pvp;
	struct pv_entry *pv;
	int i;
	
	if (pv_nfree == 0) {
		if (!(pvp = (struct pv_page *)uvm_km_zalloc(kernel_map, NBPG)))
			panic("pmap_alloc_pv: uvm_km_zalloc() failed");
		pv_pcnt++;
		pvp->pvp_pgi.pgi_freelist = pv = &pvp->pvp_pv[1];
		for (i = NPVPPG - 2; --i >= 0; pv++)
			pv->pv_next = pv + 1;
		pv->pv_next = 0;
		pv_nfree += pvp->pvp_pgi.pgi_nfree = NPVPPG - 1;
		LIST_INSERT_HEAD(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
		pv = pvp->pvp_pv;
	} else {
		pv_nfree--;
		pvp = pv_page_freelist.lh_first;
		if (--pvp->pvp_pgi.pgi_nfree <= 0)
			LIST_REMOVE(pvp, pvp_pgi.pgi_list);
		pv = pvp->pvp_pgi.pgi_freelist;
		pvp->pvp_pgi.pgi_freelist = pv->pv_next;
	}
	return pv;
}

static void
pmap_free_pv(pv)
	struct pv_entry *pv;
{
	struct pv_page *pvp;
	
	pvp = (struct pv_page *)trunc_page(pv);
	switch (++pvp->pvp_pgi.pgi_nfree) {
	case 1:
		LIST_INSERT_HEAD(&pv_page_freelist, pvp, pvp_pgi.pgi_list);
	default:
		pv->pv_next = pvp->pvp_pgi.pgi_freelist;
		pvp->pvp_pgi.pgi_freelist = pv;
		pv_nfree++;
		break;
	case NPVPPG:
		pv_nfree -= NPVPPG - 1;
		pv_pcnt--;
		LIST_REMOVE(pvp, pvp_pgi.pgi_list);
		uvm_km_free(kernel_map, (vaddr_t)pvp, NBPG);
		break;
	}
}

/*
 * We really hope that we don't need overflow entries
 * before the VM system is initialized!							XXX
 */
static struct pte_ovfl *
poalloc()
{
	struct po_page *pop;
	struct pte_ovfl *po;
	vm_page_t mem;
	int i;
	
	if (!pmap_initialized)
		panic("poalloc");
	
	if (po_nfree == 0) {
		/*
		 * Since we cannot use maps for potable allocation,
		 * we have to steal some memory from the VM system.			XXX
		 */
		mem = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_USERESERVE);
		po_pcnt++;
		pop = (struct po_page *)VM_PAGE_TO_PHYS(mem);
		pop->pop_pgi.pgi_page = mem;
		LIST_INIT(&pop->pop_pgi.pgi_freelist);
		for (i = NPOPPG - 1, po = pop->pop_po + 1; --i >= 0; po++)
			LIST_INSERT_HEAD(&pop->pop_pgi.pgi_freelist, po, po_list);
		po_nfree += pop->pop_pgi.pgi_nfree = NPOPPG - 1;
		LIST_INSERT_HEAD(&po_page_freelist, pop, pop_pgi.pgi_list);
		po = pop->pop_po;
	} else {
		po_nfree--;
		pop = po_page_freelist.lh_first;
		if (--pop->pop_pgi.pgi_nfree <= 0)
			LIST_REMOVE(pop, pop_pgi.pgi_list);
		po = pop->pop_pgi.pgi_freelist.lh_first;
		LIST_REMOVE(po, po_list);
	}
	return po;
}

static void
pofree(po, freepage)
	struct pte_ovfl *po;
	int freepage;
{
	struct po_page *pop;
	
	pop = (struct po_page *)trunc_page(po);
	switch (++pop->pop_pgi.pgi_nfree) {
	case NPOPPG:
		if (!freepage)
			break;
		po_nfree -= NPOPPG - 1;
		po_pcnt--;
		LIST_REMOVE(pop, pop_pgi.pgi_list);
		uvm_pagefree(pop->pop_pgi.pgi_page);
		return;
	case 1:
		LIST_INSERT_HEAD(&po_page_freelist, pop, pop_pgi.pgi_list);
	default:
		break;
	}
	LIST_INSERT_HEAD(&pop->pop_pgi.pgi_freelist, po, po_list);
	po_nfree++;
}

/*
 * This returns whether this is the first mapping of a page.
 */
static inline int
pmap_enter_pv(pteidx, va, pa)
	int pteidx;
	vaddr_t va;
	paddr_t pa;
{
	struct pv_entry *pv, *npv;
	int s, first;
	
	if (!pmap_initialized)
		return 0;

	s = splimp();

	pv = pa_to_pv(pa);
	if (first = pv->pv_idx == -1) {
		/*
		 * No entries yet, use header as the first entry.
		 */
		pv->pv_va = va;
		pv->pv_idx = pteidx;
		pv->pv_next = NULL;
	} else {
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
		npv = pmap_alloc_pv();
		npv->pv_va = va;
		npv->pv_idx = pteidx;
		npv->pv_next = pv->pv_next;
		pv->pv_next = npv;
	}
	splx(s);
	return first;
}

static void
pmap_remove_pv(pteidx, va, pa, pte)
	int pteidx;
	vaddr_t va;
	paddr_t pa;
	struct pte *pte;
{
	struct pv_entry *pv, *npv;
	char *attr;

	/*
	 * First transfer reference/change bits to cache.
	 */
	attr = pa_to_attr(pa);
	if (attr == NULL)
		return;
	*attr |= (pte->pte_lo & (PTE_REF | PTE_CHG)) >> ATTRSHFT;
	
	/*
	 * Remove from the PV table.
	 */
	pv = pa_to_pv(pa);
	
	/*
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry.  In either case we free the now unused entry.
	 */
	if (pteidx == pv->pv_idx && va == pv->pv_va) {
		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;
			pmap_free_pv(npv);
		} else
			pv->pv_idx = -1;
	} else {
		for (; npv = pv->pv_next; pv = npv)
			if (pteidx == npv->pv_idx && va == npv->pv_va)
				break;
		if (npv) {
			pv->pv_next = npv->pv_next;
			pmap_free_pv(npv);
		}
#ifdef	DIAGNOSTIC
		else
			panic("pmap_remove_pv: not on list\n");
#endif
	}
}

/*
 * Insert physical page at pa into the given pmap at virtual address va.
 */
int
pmap_enter(pm, va, pa, prot, flags)
	struct pmap *pm;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int flags;
{
	sr_t sr;
	int idx, i, s;
	pte_t pte;
	struct pte_ovfl *po;
	int managed;
	struct mem_region *mp;
	boolean_t wired = (flags & PMAP_WIRED) != 0;

	/*
	 * Have to remove any existing mapping first.
	 */
	pmap_remove(pm, va, va + NBPG);

	/*
	 * Compute the HTAB index.
	 */
	idx = pteidx(sr = ptesr(pm->pm_sr, va), va);
	/*
	 * Construct the PTE.
	 *
	 * Note: Don't set the valid bit for correct operation of tlb update.
	 */
	pte.pte_hi = ((sr & SR_VSID) << PTE_VSID_SHFT)
		| ((va & ADDR_PIDX) >> ADDR_API_SHFT);
	pte.pte_lo = (pa & PTE_RPGN) | PTE_M | PTE_I | PTE_G;

	managed = 0;
	if (vm_physseg_find(atop(pa), NULL) != -1)
		managed = 1;
	for (mp = mem; mp->size; mp++) {
		if (pa >= mp->start && pa < mp->start + mp->size) {
			pte.pte_lo &= ~(PTE_I | PTE_G);
			break;
		}
	}
	if (prot & VM_PROT_WRITE)
		pte.pte_lo |= PTE_RW;
	else
		pte.pte_lo |= PTE_RO;

	/*
	 * Now record mapping for later back-translation.
	 */
	if (pmap_initialized && managed)
		if (pmap_enter_pv(idx, va, pa)) {
			/* 
			 * Flush the real memory from the cache.
			 */
			__syncicache((void *)pa, NBPG);
		}

	s = splimp();
	pm->pm_stats.resident_count++;
	/*
	 * Try to insert directly into HTAB.
	 */
	if (pte_insert(idx, &pte)) {
		splx(s);
		return (KERN_SUCCESS);
	}

	/*
	 * Have to allocate overflow entry.
	 *
	 * Note, that we must use real addresses for these.
	 */
	po = poalloc();
	po->po_pte = pte;
	LIST_INSERT_HEAD(potable + idx, po, po_list);
	splx(s);

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
 * Remove the given range of mapping entries.
 */
void
pmap_remove(pm, va, endva)
	struct pmap *pm;
	vaddr_t va, endva;
{
	int idx, i, s;
	sr_t sr;
	pte_t *ptp;
	struct pte_ovfl *po, *npo;

	s = splimp();
	while (va < endva) {
		idx = pteidx(sr = ptesr(pm->pm_sr, va), va);
		for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++)
			if (ptematch(ptp, sr, va, PTE_VALID)) {
				pmap_remove_pv(idx, va, ptp->pte_lo, ptp);
				ptp->pte_hi &= ~PTE_VALID;
				asm volatile ("sync");
				tlbie(va);
				tlbsync();
				pm->pm_stats.resident_count--;
			}
		for (ptp = ptable + (idx ^ ptab_mask) * 8, i = 8; --i >= 0; ptp++)
			if (ptematch(ptp, sr, va, PTE_VALID | PTE_HID)) {
				pmap_remove_pv(idx, va, ptp->pte_lo, ptp);
				ptp->pte_hi &= ~PTE_VALID;
				asm volatile ("sync");
				tlbie(va);
				tlbsync();
				pm->pm_stats.resident_count--;
			}
		for (po = potable[idx].lh_first; po; po = npo) {
			npo = po->po_list.le_next;
			if (ptematch(&po->po_pte, sr, va, 0)) {
				pmap_remove_pv(idx, va, po->po_pte.pte_lo,
					       &po->po_pte);
				LIST_REMOVE(po, po_list);
				pofree(po, 1);
				pm->pm_stats.resident_count--;
			}
		}
		va += NBPG;
	}
	splx(s);
}

static pte_t *
pte_find(pm, va)
	struct pmap *pm;
	vaddr_t va;
{
	int idx, i;
	sr_t sr;
	pte_t *ptp;
	struct pte_ovfl *po;

	idx = pteidx(sr = ptesr(pm->pm_sr, va), va);
	for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++)
		if (ptematch(ptp, sr, va, PTE_VALID))
			return ptp;
	for (ptp = ptable + (idx ^ ptab_mask) * 8, i = 8; --i >= 0; ptp++)
		if (ptematch(ptp, sr, va, PTE_VALID | PTE_HID))
			return ptp;
	for (po = potable[idx].lh_first; po; po = po->po_list.le_next)
		if (ptematch(&po->po_pte, sr, va, 0))
			return &po->po_pte;
	return 0;
}

/*
 * Get the physical page address for the given pmap/virtual address.
 */
boolean_t
pmap_extract(pm, va, pap)
	struct pmap *pm;
	vaddr_t va;
	paddr_t *pap;
{
	pte_t *ptp;
	int s = splimp();
	
	if (!(ptp = pte_find(pm, va))) {
		splx(s);
		return (FALSE);
	}
	*pap = (ptp->pte_lo & PTE_RPGN) | (va & ADDR_POFF);
	splx(s);
	return (TRUE);
}

/*
 * Lower the protection on the specified range of this pmap.
 *
 * There are only two cases: either the protection is going to 0,
 * or it is going to read-only.
 */
void
pmap_protect(pm, sva, eva, prot)
	struct pmap *pm;
	vaddr_t sva, eva;
	vm_prot_t prot;
{
	pte_t *ptp;
	int valid, s;
	
	if (prot & VM_PROT_READ) {
		s = splimp();
		while (sva < eva) {
			if (ptp = pte_find(pm, sva)) {
				valid = ptp->pte_hi & PTE_VALID;
				ptp->pte_hi &= ~PTE_VALID;
				asm volatile ("sync");
				tlbie(sva);
				tlbsync();
				ptp->pte_lo &= ~PTE_PP;
				ptp->pte_lo |= PTE_RO;
				asm volatile ("sync");
				ptp->pte_hi |= valid;
			}
			sva += NBPG;
		}
		splx(s);
		return;
	}
	pmap_remove(pm, sva, eva);
}

boolean_t
ptemodify(pg, mask, val)
	struct vm_page *pg;
	u_int mask;
	u_int val;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	struct pv_entry *pv;
	pte_t *ptp;
	struct pte_ovfl *po;
	int i, s;
	char *attr;
	int rv;

	/*
	 * First modify bits in cache.
	 */
	attr = pa_to_attr(pa);
	if (attr == NULL)
		return FALSE;

	*attr &= ~mask >> ATTRSHFT;
	*attr |= val >> ATTRSHFT;
	
	pv = pa_to_pv(pa);
	if (pv->pv_idx < 0)
		return FALSE;

	rv = FALSE;
	s = splimp();
	for (; pv; pv = pv->pv_next) {
		for (ptp = ptable + pv->pv_idx * 8, i = 8; --i >= 0; ptp++)
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				ptp->pte_hi &= ~PTE_VALID;
				asm volatile ("sync");
				tlbie(pv->pv_va);
				tlbsync();
				rv |= ptp->pte_lo & mask; 
				ptp->pte_lo &= ~mask;
				ptp->pte_lo |= val;
				asm volatile ("sync");
				ptp->pte_hi |= PTE_VALID;
			}
		for (ptp = ptable + (pv->pv_idx ^ ptab_mask) * 8, i = 8; --i >= 0; ptp++)
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				ptp->pte_hi &= ~PTE_VALID;
				asm volatile ("sync");
				tlbie(pv->pv_va);
				tlbsync();
				rv |= ptp->pte_lo & mask; 
				ptp->pte_lo &= ~mask;
				ptp->pte_lo |= val;
				asm volatile ("sync");
				ptp->pte_hi |= PTE_VALID;
			}
		for (po = potable[pv->pv_idx].lh_first; po; po = po->po_list.le_next)
			if ((po->po_pte.pte_lo & PTE_RPGN) == pa) {
				rv |= ptp->pte_lo & mask; 
				po->po_pte.pte_lo &= ~mask;
				po->po_pte.pte_lo |= val;
			}
	}
	splx(s);
	return rv != 0;
}

int
ptebits(pg, bit)
	struct vm_page *pg;
	int bit;
{
	struct pv_entry *pv;
	pte_t *ptp;
	struct pte_ovfl *po;
	int i, s, bits = 0;
	char *attr;
	paddr_t pa = VM_PAGE_TO_PHYS(pg);

	/*
	 * First try the cache.
	 */
	attr = pa_to_attr(pa);
	if (attr == NULL)
		return 0;
	bits |= (*attr << ATTRSHFT) & bit;
	if (bits == bit)
		return bits;

	pv = pa_to_pv(pa);
	if (pv->pv_idx < 0)
		return 0;
	
	s = splimp();
	for (; pv; pv = pv->pv_next) {
		for (ptp = ptable + pv->pv_idx * 8, i = 8; --i >= 0; ptp++)
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				bits |= ptp->pte_lo & bit;
				if (bits == bit) {
					splx(s);
					return bits;
				}
			}
		for (ptp = ptable + (pv->pv_idx ^ ptab_mask) * 8, i = 8; --i >= 0; ptp++)
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				bits |= ptp->pte_lo & bit;
				if (bits == bit) {
					splx(s);
					return bits;
				}
			}
		for (po = potable[pv->pv_idx].lh_first; po; po = po->po_list.le_next)
			if ((po->po_pte.pte_lo & PTE_RPGN) == pa) {
				bits |= po->po_pte.pte_lo & bit;
				if (bits == bit) {
					splx(s);
					return bits;
				}
			}
	}
	splx(s);
	return bits;
}

/*
 * Lower the protection on the specified physical page.
 *
 * There are only two cases: either the protection is going to 0,
 * or it is going to read-only.
 */
void
pmap_page_protect(pg, prot)
	struct vm_page *pg;
	vm_prot_t prot;
{
	paddr_t pa = VM_PAGE_TO_PHYS(pg);
	vaddr_t va;
	pte_t *ptp;
	struct pte_ovfl *po, *npo;
	int i, s, idx;
	struct pv_entry *pv;

	pa &= ~ADDR_POFF;
	if (prot & VM_PROT_READ) {
		ptemodify(pg, PTE_PP, PTE_RO);
		return;
	}

	pv = pa_to_pv(pa);
	if (pv == NULL)
		return;

	s = splimp();
	while (pv->pv_idx >= 0) {
		idx = pv->pv_idx;
		va = pv->pv_va;
		for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++)
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				pmap_remove_pv(idx, va, pa, ptp);
				ptp->pte_hi &= ~PTE_VALID;
				asm volatile ("sync");
				tlbie(va);
				tlbsync();
				goto next;
			}
		for (ptp = ptable + (idx ^ ptab_mask) * 8, i = 8; --i >= 0; ptp++)
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				pmap_remove_pv(idx, va, pa, ptp);
				ptp->pte_hi &= ~PTE_VALID;
				asm volatile ("sync");
				tlbie(va);
				tlbsync();
				goto next;
			}
		for (po = potable[idx].lh_first; po; po = npo) {
			npo = po->po_list.le_next;
			if ((po->po_pte.pte_lo & PTE_RPGN) == pa) {
				pmap_remove_pv(idx, va, pa, &po->po_pte);
				LIST_REMOVE(po, po_list);
				pofree(po, 1);
				goto next;
			}
		}
next:
	}
	splx(s);
}

/*
 * Activate the address space for the specified process.  If the process
 * is the current process, load the new MMU context.
 */
void
pmap_activate(p)
	struct proc *p;
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	pmap_t pmap = p->p_vmspace->vm_map.pmap, rpm;
	int psl, i, ksr, seg;

	/*
	 * XXX Normally performed in cpu_fork().
	 */
	if (pcb->pcb_pm != pmap) {
		pcb->pcb_pm = pmap;
		(void) pmap_extract(pmap_kernel(), (vaddr_t)pcb->pcb_pm,
		    (paddr_t *)&pcb->pcb_pmreal);
	}

	if (p == curproc) {
		/* Disable interrupts while switching. */
		__asm __volatile("mfmsr %0" : "=r"(psl) :);
		psl &= ~PSL_EE;
		__asm __volatile("mtmsr %0" :: "r"(psl));

		/* Store pointer to new current pmap. */
		curpm = pcb->pcb_pmreal;

		/* Save kernel SR. */
		__asm __volatile("mfsr %0,14" : "=r"(ksr) :);

		/*
		 * Set new segment registers.  We use the pmap's real
		 * address to avoid accessibility problems.
		 */
		rpm = pcb->pcb_pmreal;
		for (i = 0; i < 16; i++) {
			seg = rpm->pm_sr[i];
			__asm __volatile("mtsrin %0,%1"
			    :: "r"(seg), "r"(i << ADDR_SR_SHFT));
		}

		/* Restore kernel SR. */
		__asm __volatile("mtsr 14,%0" :: "r"(ksr));

		/* Interrupts are OK again. */
		psl |= PSL_EE;
		__asm __volatile("mtmsr %0" :: "r"(psl));
	}
}

/*
 * Deactivate the specified process's address space.
 */
void
pmap_deactivate(p)
	struct proc *p;
{
}

/*
 * Synchronize caches corresponding to [addr, addr+len) in p.
 */
void
pmap_procwr(p, va, len)
	struct proc *p;
	vaddr_t va;
	size_t len;
{
	paddr_t pa;

	(void) pmap_extract(p->p_vmspace->vm_map.pmap, va, &pa);
	__syncicache((void *)pa, len);
}
