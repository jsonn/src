/*	$NetBSD: pmap_bootstrap.c,v 1.27.2.2 2005/02/23 10:03:04 yamt Exp $	*/

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
 *	@(#)pmap_bootstrap.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pmap_bootstrap.c,v 1.27.2.2 2005/02/23 10:03:04 yamt Exp $");

#include "opt_m680x0.h"

#include <sys/param.h>
#include <uvm/uvm_extern.h>
#include <machine/pte.h>
#include <machine/vmparam.h>
#include <machine/cpu.h>
#include <arch/x68k/x68k/iodevice.h>


#define RELOC(v, t)	*((t*)((caddr_t)&(v) + firstpa))

extern char *etext;
extern int Sysptsize;
extern char *proc0paddr;
extern st_entry_t *Sysseg;
extern pt_entry_t *Sysptmap, *Sysmap;

extern int maxmem, physmem;
extern paddr_t avail_start, avail_end;
extern vaddr_t virtual_avail, virtual_end;
extern psize_t mem_size;
extern int protection_codes[];

u_int8_t *intiobase = (u_int8_t *) PHYS_IODEV;

void	pmap_bootstrap(paddr_t, paddr_t);

/*
 * Special purpose kernel virtual addresses, used for mapping
 * physical pages for a variety of temporary or permanent purposes:
 *
 *	CADDR1, CADDR2:	pmap zero/copy operations
 *	vmmap:		/dev/mem, crash dumps, parity error checking
 *	msgbufaddr:	kernel message buffer
 */
caddr_t		CADDR1, CADDR2, vmmap;
extern caddr_t	msgbufaddr;

/*
 * Bootstrap the VM system.
 *
 * Called with MMU off so we must relocate all global references by `firstpa'
 * (don't call any functions here!)  `nextpa' is the first available physical
 * memory address.  Returns an updated first PA reflecting the memory we
 * have allocated.  MMU is still off when we return.
 *
 * XXX assumes sizeof(u_int) == sizeof(pt_entry_t)
 * XXX a PIC compiler would make this much easier.
 */
void
pmap_bootstrap(paddr_t nextpa, paddr_t firstpa)
{
	paddr_t kstpa, kptpa, kptmpa, p0upa;
	u_int nptpages, kstsize;
	st_entry_t protoste, *ste;
	pt_entry_t protopte, *pte, *epte;

	/*
	 * Calculate important physical addresses:
	 *
	 *	kstpa		kernel segment table	1 page (!040)
	 *						N pages (040)
	 *
	 *	kptpa		statically allocated
	 *			kernel PT pages		Sysptsize+ pages
	 *
	 * [ Sysptsize is the number of pages of PT, and IIOMAPSIZE
	 *   is the number of PTEs, hence we need to round
	 *   the total to a page boundary with IO maps at the end. ]
	 *
	 *	kptmpa		kernel PT map		1 page
	 *
	 *	p0upa		proc 0 u-area		UPAGES pages
	 *
	 * The KVA corresponding to any of these PAs is:
	 *	(PA - firstpa + KERNBASE).
	 */
	if (RELOC(mmutype, int) == MMU_68040)
		kstsize = MAXKL2SIZE / (NPTEPG/SG4_LEV2SIZE);
	else
		kstsize = 1;
	kstpa = nextpa;
	nextpa += kstsize * PAGE_SIZE;
	kptmpa = nextpa;
	nextpa += PAGE_SIZE;
	p0upa = nextpa;
	nextpa += USPACE;
	kptpa = nextpa;
	nptpages = RELOC(Sysptsize, int) +
		(IIOMAPSIZE + NPTEPG - 1) / NPTEPG;
	nextpa += nptpages * PAGE_SIZE;

	/*
	 * Clear all PTEs to zero
	 */
	for (pte = (pt_entry_t *)kstpa; pte < (pt_entry_t *)nexpta; pte++)
		*pte = 0;

	/*
	 * Initialize segment table and kernel page table map.
	 *
	 * On 68030s and earlier MMUs the two are identical except for
	 * the valid bits so both are initialized with essentially the
	 * same values.  On the 68040, which has a mandatory 3-level
	 * structure, the segment table holds the level 1 table and part
	 * (or all) of the level 2 table and hence is considerably
	 * different.  Here the first level consists of 128 descriptors
	 * (512 bytes) each mapping 32mb of address space.  Each of these
	 * points to blocks of 128 second level descriptors (512 bytes)
	 * each mapping 256kb.  Note that there may be additional "segment
	 * table" pages depending on how large MAXKL2SIZE is.
	 *
	 * XXX cramming two levels of mapping into the single "segment"
	 * table on the 68040 is intended as a temporary hack to get things
	 * working.  The 224mb of address space that this allows will most
	 * likely be insufficient in the future (at least for the kernel).
	 */
#if defined(M68040) || defined(M68060)
	if (RELOC(mmutype, int) == MMU_68040) {
		int num;

		/*
		 * First invalidate the entire "segment table" pages
		 * (levels 1 and 2 have the same "invalid" value).
		 */
		pte = (u_int *)kstpa;
		epte = &pte[kstsize * NPTEPG];
		while (pte < epte)
			*pte++ = SG_NV;
		/*
		 * Initialize level 2 descriptors (which immediately
		 * follow the level 1 table).  We need:
		 *	NPTEPG / SG4_LEV3SIZE
		 * level 2 descriptors to map each of the nptpages
		 * pages of PTEs.  Note that we set the "used" bit
		 * now to save the HW the expense of doing it.
		 */
		num = nptpages * (NPTEPG / SG4_LEV3SIZE);
		pte = &((u_int *)kstpa)[SG4_LEV1SIZE];
		epte = &pte[num];
		protoste = kptpa | SG_U | SG_RW | SG_V;
		while (pte < epte) {
			*pte++ = protoste;
			protoste += (SG4_LEV3SIZE * sizeof(st_entry_t));
		}
		/*
		 * Initialize level 1 descriptors.  We need:
		 *	roundup(num, SG4_LEV2SIZE) / SG4_LEV2SIZE
		 * level 1 descriptors to map the `num' level 2's.
		 */
		pte = (u_int *)kstpa;
		epte = &pte[roundup(num, SG4_LEV2SIZE) / SG4_LEV2SIZE];
		protoste = (u_int)&pte[SG4_LEV1SIZE] | SG_U | SG_RW | SG_V;
		while (pte < epte) {
			*pte++ = protoste;
			protoste += (SG4_LEV2SIZE * sizeof(st_entry_t));
		}
		/*
		 * Initialize the final level 1 descriptor to map the last
		 * block of level 2 descriptors.
		 */
		ste = ((u_int *)kstpa)[SG4_LEV1SIZE-1];
		pte = ((u_int *)kstpa)[kstsize*NPTEPG - SG4_LEV2SIZE];
		*ste = (u_int)pte | SG_U | SG_RW | SG_V;
		/*
		 * Now initialize the final portion of that block of
		 * descriptors to map kptmpa.
		 */
		pte = &((u_int *)kstpa)[kstsize*NPTEPG - NPTEPG/SG4_LEV3SIZE];
		epte = &pte[NPTEPG/SG4_LEV3SIZE];
		protoste = kptmpa | SG_U | SG_RW | SG_V;
		while (pte < epte) {
			*pte++ = protoste;
			protoste += (SG4_LEV3SIZE * sizeof(st_entry_t));
		}
		/*
		 * Initialize Sysptmap
		 */
		pte = (u_int *)kptmpa;
		epte = &pte[nptpages];
		protopte = kptpa | PG_RW | PG_CI | PG_V;
		while (pte < epte) {
			*pte++ = protopte;
			protopte += PAGE_SIZE;
		}
		/*
		 * Invalidate all but the last remaining entry.
		 */
		epte = &((u_int *)kptmpa)[NPTEPG-1];
		while (pte < epte) {
			*pte++ = PG_NV;
		}
		/*
		 * Initialize the last one to point to Sysptmap.
		 */
		*pte = kptmpa | PG_RW | PG_CI | PG_V;
	} else
#endif /* M68040 || M68060 */
	{
		/*
		 * Map the page table pages in both the HW segment table
		 * and the software Sysptmap.
		 */
		ste = (u_int *)kstpa;
		pte = (u_int *)kptmpa;
		epte = &pte[nptpages];
		protoste = kptpa | SG_RW | SG_V;
		protopte = kptpa | PG_RW | PG_CI | PG_V;
		while (pte < epte) {
			*ste++ = protoste;
			*pte++ = protopte;
			protoste += PAGE_SIZE;
			protopte += PAGE_SIZE;
		}
		/*
		 * Invalidate all but the last remaining entries in both.
		 */
		epte = &((u_int *)kptmpa)[NPTEPG-1];
		while (pte < epte) {
			*ste++ = SG_NV;
			*pte++ = PG_NV;
		}
 		/*
		 * Initialize the last one to point to Sysptmap.
 		 */
		*pte = kptmpa | PG_RW | PG_CI | PG_V;
	}

	/*
	 * Initialize kernel page table.
	 * Start by invalidating the `nptpages' that we have allocated.
	 */
	pte = (u_int *)kptpa;
	epte = &pte[nptpages * NPTEPG];
	while (pte < epte)
		*pte++ = PG_NV;
	/*
	 * Validate PTEs for kernel text (RO)
	 */
	pte = &((u_int *)kptpa)[m68k_btop(KERNBASE)];
	/* XXX why KERNBASE relative? */
	epte = &pte[m68k_btop(m68k_trunc_page(&etext))];
	protopte = firstpa | PG_RO | PG_V;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += PAGE_SIZE;
	}
	/*
	 * Validate PTEs for kernel data/bss, dynamic data allocated
	 * by us so far (kstpa - firstpa bytes), and pages for proc0
	 * u-area and page table allocated below (RW).
	 */
	epte = &((u_int *)kptpa)[m68k_btop(kstpa - firstpa)];
	protopte = (protopte & ~PG_PROT) | PG_RW;
	/*
	 * Enable copy-back caching of data pages
	 */
	if (RELOC(mmutype, int) == MMU_68040)
		protopte |= PG_CCB;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += PAGE_SIZE;
	}
	/*
	 * map the kernel segment table cache invalidated for 
	 * these machines (for the 68040 not strictly necessary, but
	 * recommended by Motorola; for the 68060 mandatory)
	 * XXX this includes p0upa.  why?
	 */
	epte = &((u_int *)kptpa)[m68k_btop(nextpa - firstpa)];
	protopte = (protopte & ~PG_PROT) | PG_RW;
	if (RELOC(mmutype, int) == MMU_68040) {
		protopte &= ~PG_CCB;
		protopte |= PG_CIN;
	}
	while (pte < epte) {
		*pte++ = protopte;
		protopte += PAGE_SIZE;
	}

#define	PTE2VA(pte)	m68k_ptob(pte - ((pt_entry_t *)kptpa))

	protopte = INTIOBASE | PG_RW | PG_CI | PG_V;
	RELOC(IODEVbase, char *) = PTE2VA(pte);
	epte = &pte[IIOMAPSIZE];
	while (pte < epte) {
		*pte++ = protopte;
		protopte += PAGE_SIZE;
	}
	RELOC(virtual_avail, vaddr_t) = PTE2VA(pte);

	/*
	 * Calculate important exported kernel virtual addresses
	 */
	/*
	 * Sysseg: base of kernel segment table
	 */
	RELOC(Sysseg, st_entry_t *) =
		(st_entry_t *)(kstpa - firstpa);
	/*
	 * Sysptmap: base of kernel page table map
	 */
	RELOC(Sysptmap, pt_entry_t *) =
		(pt_entry_t *)(kptmpa - firstpa);
	/*
	 * Sysmap: kernel page table (as mapped through Sysptmap)
	 * Immediately follows `nptpages' of static kernel page table.
	 */
	RELOC(Sysmap, pt_entry_t *) =
	    (pt_entry_t *)m68k_ptob((NPTEPG - 1) * NPTEPG);
	/*
	 * IODEVbase, intiolimit: base and end of internal (DIO) IO space.
	 * IIOMAPSIZE pages prior to external IO space at end of static
	 * kernel page table.
	 */
	RELOC(intiobase, u_int8_t *) = RELOC(IODEVbase, u_int8_t *); /* XXX */
	RELOC(intiolimit, char *) =
	    RELOC(intiobase, u_int8_t *) + m68k_ptob(IIOMAPSIZE);

	/*
	 * Setup u-area for process 0.
	 */
	/*
	 * Zero the u-area.
	 * NOTE: `pte' and `epte' aren't PTEs here.
	 */
	pte = (u_int *)p0upa;
	epte = (u_int *)(p0upa + USPACE);
	while (pte < epte)
		*pte++ = 0;
	/*
	 * Remember the u-area address so it can be loaded in the
	 * proc struct p_addr field later.
	 */
	RELOC(proc0paddr, char *) = (char *)(p0upa - firstpa);

	/*
	 * VM data structures are now initialized, set up data for
	 * the pmap module.
	 */
	RELOC(avail_start, paddr_t) = nextpa;
	RELOC(avail_end, paddr_t) =
		m68k_ptob(RELOC(maxmem, int))
			/* XXX allow for msgbuf */
			- m68k_round_page(MSGBUFSIZE);
	RELOC(mem_size, psize_t) = m68k_ptob(RELOC(physmem, int));
	RELOC(virtual_end, vaddr_t) = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize protection array.
	 * XXX don't use a switch statement, it might produce an
	 * absolute "jmp" table.
	 */
	{
		int *kp;

		kp = &RELOC(protection_codes, int);
		kp[VM_PROT_NONE|VM_PROT_NONE|VM_PROT_NONE] = 0;
		kp[VM_PROT_READ|VM_PROT_NONE|VM_PROT_NONE] = PG_RO;
		kp[VM_PROT_READ|VM_PROT_NONE|VM_PROT_EXECUTE] = PG_RO;
		kp[VM_PROT_NONE|VM_PROT_NONE|VM_PROT_EXECUTE] = PG_RO;
		kp[VM_PROT_NONE|VM_PROT_WRITE|VM_PROT_NONE] = PG_RW;
		kp[VM_PROT_NONE|VM_PROT_WRITE|VM_PROT_EXECUTE] = PG_RW;
		kp[VM_PROT_READ|VM_PROT_WRITE|VM_PROT_NONE] = PG_RW;
		kp[VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE] = PG_RW;
	}

	/*
	 * Kernel page/segment table allocated above,
	 * just initialize pointers.
	 */
	{
		struct pmap *kpm = &RELOC(kernel_pmap_store, struct pmap);

		kpm->pm_stab = RELOC(Sysseg, st_entry_t *);
		kpm->pm_ptab = RELOC(Sysmap, pt_entry_t *);
		simple_lock_init(&kpm->pm_lock);
		kpm->pm_count = 1;
		kpm->pm_stpa = (st_entry_t *)kstpa;
#if defined(M68040) || defined(M68060)
		/*
		 * For the 040 we also initialize the free level 2
		 * descriptor mask noting that we have used:
		 *	0:		level 1 table
		 *	1 to `num':	map page tables
		 *	MAXKL2SIZE-1:	maps last-page page table
		 */
		if (RELOC(mmutype, int) == MMU_68040) {
			int num;
			
			kpm->pm_stfree = ~l2tobm(0);
			num = roundup(nptpages * (NPTEPG / SG4_LEV3SIZE),
				      SG4_LEV2SIZE) / SG4_LEV2SIZE;
			while (num)
				kpm->pm_stfree &= ~l2tobm(num--);
			kpm->pm_stfree &= ~l2tobm(MAXKL2SIZE-1);
			for (num = MAXKL2SIZE;
			     num < sizeof(kpm->pm_stfree)*NBBY;
			     num++)
				kpm->pm_stfree &= ~l2tobm(num);
		}
#endif
	}

	/*
	 * Allocate some fixed, special purpose kernel virtual addresses
	 */
	{
		vaddr_t va = RELOC(virtual_avail, vaddr_t);

		RELOC(CADDR1, caddr_t) = (caddr_t)va;
		va += PAGE_SIZE;
		RELOC(CADDR2, caddr_t) = (caddr_t)va;
		va += PAGE_SIZE;
		RELOC(vmmap, caddr_t) = (caddr_t)va;
		va += PAGE_SIZE;
		RELOC(msgbufaddr, caddr_t) = (caddr_t)va;
		va += m68k_round_page(MSGBUFSIZE);
		RELOC(virtual_avail, vaddr_t) = va;
	}
}
