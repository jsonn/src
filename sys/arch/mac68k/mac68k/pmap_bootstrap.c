/*	$NetBSD: pmap_bootstrap.c,v 1.58.2.5 2005/02/06 08:59:22 skrll Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: pmap_bootstrap.c,v 1.58.2.5 2005/02/06 08:59:22 skrll Exp $");

#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "zsc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>

#include <uvm/uvm_extern.h>

#include <machine/pte.h>
#include <mac68k/mac68k/clockreg.h>
#include <machine/vmparam.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/autoconf.h>

#include <ufs/mfs/mfs_extern.h>

#include <mac68k/mac68k/macrom.h>

#define PA2VA(v, t)	(t)((u_int)(v) - firstpa)

extern char *etext;
extern int Sysptsize;
extern char *extiobase, *proc0paddr;
extern st_entry_t *Sysseg;
extern pt_entry_t *Sysptmap, *Sysmap;

extern int physmem;
extern paddr_t avail_start;
extern paddr_t avail_end;
extern vaddr_t virtual_avail, virtual_end;
extern vsize_t mem_size;
extern int protection_codes[];

#if NZSC > 0
extern	int	zsinited;
#endif

/*
 * These are used to map the RAM:
 */
int	numranges;	/* = 0 == don't use the ranges */
u_long	low[8];
u_long	high[8];
u_long	maxaddr;	/* PA of the last physical page */
int	vidlen;
#define VIDMAPSIZE	btoc(vidlen)
extern u_int32_t	mac68k_vidphys;
extern u_int32_t	videoaddr;
extern u_int32_t	videorowbytes;
extern u_int32_t	videosize;
static u_int32_t	newvideoaddr;

extern caddr_t	ROMBase;

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

void	pmap_bootstrap(paddr_t, paddr_t);
void	bootstrap_mac68k(int);

/*
 * Bootstrap the VM system.
 *
 * This is called with the MMU either on or off.  If it's on, we assume
 * that it's mapped with the same PA <=> LA mapping that we eventually
 * want.  The page sizes and the protections will be wrong, anyway.
 *
 * nextpa is the first address following the loaded kernel.  On a IIsi
 * on 12 May 1996, that was 0xf9000 beyond firstpa.
 */
void
pmap_bootstrap(paddr_t nextpa, paddr_t firstpa)
{
	paddr_t kstpa, kptpa, vidpa, iiopa, rompa, kptmpa, lkptpa, p0upa;
	u_int nptpages, kstsize;
	paddr_t avail_next;
	int avail_remaining;
	int avail_range;
	int i;
	st_entry_t protoste, *ste;
	pt_entry_t protopte, *pte, *epte;
	extern char start[];

	vidlen = m68k_round_page(((videosize >> 16) & 0xffff) * videorowbytes +
	    m68k_page_offset(mac68k_vidphys));

	/*
	 * Calculate important physical addresses:
	 *
	 *	kstpa		kernel segment table	1 page (!040)
	 *						N pages (040)
	 *
	 *	kptpa		statically allocated
	 *			kernel PT pages		Sysptsize+ pages
	 *
	 *	vidpa		internal video space for some machines
	 *			PT pages		VIDMAPSIZE pages
	 *
	 *	rompa 		ROM space
	 *			PT pages		ROMMAPSIZE pages
	 *
	 *	iiopa		internal IO space
	 *			PT pages		IIOMAPSIZE pages
	 *
	 * [ Sysptsize is the number of pages of PT, IIOMAPSIZE and
	 *   NBMAPSIZE are the number of PTEs, hence we need to round
	 *   the total to a page boundary with IO maps at the end. ]
	 *
	 *	kptmpa		kernel PT map		1 page
	 *
	 *	lkptpa		last kernel PT page	1 page
	 *
	 *	p0upa		proc 0 u-area		UPAGES pages
	 *
	 */
	if (mmutype == MMU_68040)
		kstsize = MAXKL2SIZE / (NPTEPG/SG4_LEV2SIZE);
	else
		kstsize = 1;
	kstpa = nextpa;
	nextpa += kstsize * PAGE_SIZE;
	kptpa = nextpa;
	nptpages = Sysptsize +
		(IIOMAPSIZE + ROMMAPSIZE + VIDMAPSIZE + NPTEPG - 1) / NPTEPG;
	nextpa += nptpages * PAGE_SIZE;
	vidpa = nextpa - VIDMAPSIZE * sizeof(pt_entry_t);
	rompa = vidpa  - ROMMAPSIZE * sizeof(pt_entry_t);
	iiopa = rompa  - IIOMAPSIZE * sizeof(pt_entry_t);
	kptmpa = nextpa;
	nextpa += PAGE_SIZE;
	lkptpa = nextpa;
	nextpa += PAGE_SIZE;
	p0upa = nextpa;
	nextpa += USPACE;

	
	for (i = 0; i < numranges; i++)
		if (low[i] <= firstpa && firstpa < high[i])
			break;
	if (i >= numranges || nextpa > high[i]) {
		if (mac68k_machine.do_graybars) {
			printf("Failure in NetBSD boot; ");
			if (i < numranges)
				printf("nextpa=0x%lx, high[%d]=0x%lx.\n",
				    nextpa, i, high[i]);
			else
				printf("can't find kernel RAM segment.\n");
			printf("You're hosed!  Try booting with 32-bit ");
			printf("addressing enabled in the memory control ");
			printf("panel.\n");
			printf("Older machines may need Mode32 to get that ");
			printf("option.\n");
		}
		panic("Cannot work with the current memory mappings.");
	}

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
	if (mmutype == MMU_68040) {
		int num;

		/*
		 * First invalidate the entire "segment table" pages
		 * (levels 1 and 2 have the same "invalid" value).
		 */
		pte = PA2VA(kstpa, u_int *);
		epte = &pte[kstsize * NPTEPG];
		while (pte < epte)
			*pte++ = SG_NV;
		/*
		 * Initialize level 2 descriptors (which immediately
		 * follow the level 1 table).  We need:
		 *	NPTEPG / SG4_LEV3SIZE
		 * level 2 descriptors to map each of the nptpages+1
		 * pages of PTEs.  Note that we set the "used" bit
		 * now to save the HW the expense of doing it.
		 */
		num = (nptpages + 1) * (NPTEPG / SG4_LEV3SIZE);
		pte = &(PA2VA(kstpa, u_int *))[SG4_LEV1SIZE];
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
		pte = PA2VA(kstpa, u_int *);
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
		ste = &(PA2VA(kstpa, u_int*))[SG4_LEV1SIZE-1];
		pte = &(PA2VA(kstpa, u_int*))[kstsize*NPTEPG - SG4_LEV2SIZE];
		*ste = (u_int)pte | SG_U | SG_RW | SG_V;
		/*
		 * Now initialize the final portion of that block of
		 * descriptors to map the "last PT page".
		 */
		pte = &(PA2VA(kstpa, u_int*))
				[kstsize*NPTEPG - NPTEPG/SG4_LEV3SIZE];
		epte = &pte[NPTEPG/SG4_LEV3SIZE];
		protoste = lkptpa | SG_U | SG_RW | SG_V;
		while (pte < epte) {
			*pte++ = protoste;
			protoste += (SG4_LEV3SIZE * sizeof(st_entry_t));
		}
		/*
		 * Initialize Sysptmap
		 */
		pte = PA2VA(kptmpa, u_int *);
		epte = &pte[nptpages+1];
		protopte = kptpa | PG_RW | PG_CI | PG_V;
		while (pte < epte) {
			*pte++ = protopte;
			protopte += PAGE_SIZE;
		}
		/*
		 * Invalidate all but the last remaining entries in both.
		 */
		epte = &(PA2VA(kptmpa, u_int *))[NPTEPG-1];
		while (pte < epte) {
			*pte++ = PG_NV;
		}
		/*
		 * Initialize the last to point to the page
		 * table page allocated earlier.
		 */
		*pte = lkptpa | PG_RW | PG_CI | PG_V;
	} else {
		/*
		 * Map the page table pages in both the HW segment table
		 * and the software Sysptmap.  Note that Sysptmap is also
		 * considered a PT page hence the +1.
		 */
		ste = PA2VA(kstpa, u_int*);
		pte = PA2VA(kptmpa, u_int*);
		epte = &pte[nptpages+1];
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
		epte = &(PA2VA(kptmpa, u_int *))[NPTEPG-1];
		while (pte < epte) {
			*ste++ = SG_NV;
			*pte++ = PG_NV;
		}
		/*
		 * Initialize the last to point to point to the page
		 * table page allocated earlier.
		 */
		*ste = lkptpa | SG_RW | SG_V;
		*pte = lkptpa | PG_RW | PG_CI | PG_V;
	}
	/*
	 * Invalidate all entries in the last kernel PT page
	 * (u-area PTEs will be validated later).
	 */
	pte = PA2VA(lkptpa, u_int *);
	epte = &pte[NPTEPG];
	while (pte < epte)
		*pte++ = PG_NV;

	/*
	 * Initialize kernel page table.
	 * Start by invalidating the `nptpages' that we have allocated.
	 */
	pte = PA2VA(kptpa, u_int *);
	epte = &pte[nptpages * NPTEPG];
	while (pte < epte)
		*pte++ = PG_NV;

	/*
	 * Validate PTEs for kernel text (RO).
	 * Pages up to "start" must be writable for the ROM.
	 */
	pte = &(PA2VA(kptpa, u_int *))[m68k_btop(KERNBASE)];
	epte = &pte[m68k_btop(m68k_round_page(start))];
	protopte = firstpa | PG_RW | PG_V;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += PAGE_SIZE;
	}
	epte = &pte[m68k_btop(m68k_trunc_page(&etext))];
	protopte = (protopte & ~PG_PROT) | PG_RO;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += PAGE_SIZE;
	}
	/*
	 * Validate PTEs for kernel data/bss, dynamic data allocated
	 * by us so far (nextpa - firstpa bytes), and pages for proc0
	 * u-area and page table allocated below (RW).
	 */
	epte = &(PA2VA(kptpa, u_int *))[m68k_btop(nextpa - firstpa)];
	protopte = (protopte & ~PG_PROT) | PG_RW;
	/*
	 * Enable copy-back caching of data pages
	 */
	if (mmutype == MMU_68040)
		protopte |= PG_CCB;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += PAGE_SIZE;
	}
	/*
	 * Finally, validate the internal IO space, ROM space, and
	 * framebuffer PTEs (RW+CI).
	 */
	pte = PA2VA(iiopa, u_int *);
	epte = PA2VA(rompa, u_int *);
	protopte = IOBase | PG_RW | PG_CI | PG_V;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += PAGE_SIZE;
	}

	pte = PA2VA(rompa, u_int *);
	epte = PA2VA(vidpa, u_int *);
	protopte = ((u_int) ROMBase) | PG_RO | PG_V;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += PAGE_SIZE;
	}

	if (vidlen) {
		pte = PA2VA(vidpa, u_int *);
		epte = pte + VIDMAPSIZE;
		protopte = m68k_trunc_page(mac68k_vidphys) |
		    PG_RW | PG_V | PG_CI;
		while (pte < epte) {
			*pte++ = protopte;
			protopte += PAGE_SIZE;
		}
	}

	/*
	 * Calculate important exported kernel virtual addresses
	 */
	/*
	 * Sysseg: base of kernel segment table
	 */
	Sysseg = PA2VA(kstpa, st_entry_t *);
	/*
	 * Sysptmap: base of kernel page table map
	 */
	Sysptmap = PA2VA(kptmpa, pt_entry_t *);
	/*
	 * Sysmap: kernel page table (as mapped through Sysptmap)
	 * Immediately follows `nptpages' of static kernel page table.
	 */
	Sysmap = (pt_entry_t *)m68k_ptob(nptpages * NPTEPG);

	IOBase = (u_long)m68k_ptob(nptpages * NPTEPG -
	    (IIOMAPSIZE + ROMMAPSIZE + VIDMAPSIZE));

	ROMBase = (char *)m68k_ptob(nptpages * NPTEPG -
	    (ROMMAPSIZE + VIDMAPSIZE));

	if (vidlen) {
		newvideoaddr = (u_int32_t)m68k_ptob(nptpages * NPTEPG -
		    VIDMAPSIZE) + m68k_page_offset(mac68k_vidphys);
	}

	/*
	 * Setup u-area for process 0.
	 */
	/*
	 * Zero the u-area.
	 * NOTE: `pte' and `epte' aren't PTEs here.
	 */
	pte = PA2VA(p0upa, u_int *);
	epte = (u_int *)(PA2VA(p0upa, u_int) + USPACE);
	while (pte < epte)
		*pte++ = 0;
	/*
	 * Remember the u-area address so it can be loaded in the
	 * proc struct p_addr field later.
	 */
	proc0paddr = PA2VA(p0upa, char *);

	/*
	 * VM data structures are now initialized, set up data for
	 * the pmap module.
	 *
	 * Note about avail_end: msgbuf is initialized just after
	 * avail_end in machdep.c.  Since the last page is used
	 * for rebooting the system (code is copied there and
	 * excution continues from copied code before the MMU
	 * is disabled), the msgbuf will get trounced between
	 * reboots if it's placed in the last physical page.
	 * To work around this, we move avail_end back one more
	 * page so the msgbuf can be preserved.
	 */
	avail_next = avail_start = m68k_round_page(nextpa);
	avail_remaining = 0;
	avail_range = -1;
	for (i = 0; i < numranges; i++) {
		if (low[i] <= avail_next && avail_next < high[i]) {
			avail_range = i;
			avail_remaining = high[i] - avail_next;
		} else if (avail_range != -1) {
			avail_remaining += (high[i] - low[i]);
		}
	}
	physmem = m68k_btop(avail_remaining + nextpa - firstpa);

	maxaddr = high[numranges - 1] - m68k_ptob(1);
	high[numranges - 1] -= (m68k_round_page(MSGBUFSIZE) + m68k_ptob(1));
	avail_end = high[numranges - 1];
	mem_size = m68k_ptob(physmem);
	virtual_avail = VM_MIN_KERNEL_ADDRESS + (nextpa - firstpa);
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize protection array.
	 * XXX don't use a switch statement, it might produce an
	 * absolute "jmp" table.
	 */
	{
		int *kp;

		kp = (int *)&protection_codes;
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
	 * Kernel page/segment table allocated in locore,
	 * just initialize pointers.
	 */
	{
		struct pmap *kpm = (struct pmap *)&kernel_pmap_store;

		kpm->pm_stab = Sysseg;
		kpm->pm_ptab = Sysmap;
		simple_lock_init(&kpm->pm_lock);
		kpm->pm_count = 1;
		kpm->pm_stpa = (st_entry_t *)kstpa;
		/*
		 * For the 040 we also initialize the free level 2
		 * descriptor mask noting that we have used:
		 *	0:		level 1 table
		 *	1 to `num':	map page tables
		 *	MAXKL2SIZE-1:	maps last-page page table
		 */
		if (mmutype == MMU_68040) {
			int num;
			
			kpm->pm_stfree = ~l2tobm(0);
			num = roundup((nptpages + 1) * (NPTEPG / SG4_LEV3SIZE),
				      SG4_LEV2SIZE) / SG4_LEV2SIZE;
			while (num)
				kpm->pm_stfree &= ~l2tobm(num--);
			kpm->pm_stfree &= ~l2tobm(MAXKL2SIZE-1);
			for (num = MAXKL2SIZE;
			     num < sizeof(kpm->pm_stfree)*NBBY;
			     num++)
				kpm->pm_stfree &= ~l2tobm(num);
		}
	}

	/*
	 * Allocate some fixed, special purpose kernel virtual addresses
	 */
	{
		vaddr_t va = virtual_avail;

		CADDR1 = (caddr_t)va;
		va += PAGE_SIZE;
		CADDR2 = (caddr_t)va;
		va += PAGE_SIZE;
		vmmap = (caddr_t)va;
		va += PAGE_SIZE;
		msgbufaddr = (caddr_t)va;
		va += m68k_round_page(MSGBUFSIZE);
		virtual_avail = va;
	}
}

void
bootstrap_mac68k(int tc)
{
#if NZSC > 0
	extern void zs_init(void);
#endif
	extern int *esym;
	paddr_t nextpa;
	caddr_t oldROMBase;

	if (mac68k_machine.do_graybars)
		printf("Bootstrapping NetBSD/mac68k.\n");

	oldROMBase = ROMBase;
	mac68k_vidphys = videoaddr;

	if (((tc & 0x80000000) && (mmutype == MMU_68030)) ||
	    ((tc & 0x8000) && (mmutype == MMU_68040))) {
		if (mac68k_machine.do_graybars)
			printf("Getting mapping from MMU.\n");
		(void) get_mapping();
		if (mac68k_machine.do_graybars)
			printf("Done.\n");
	} else {
		/* MMU not enabled.  Fake up ranges. */
		numranges = 1;
		low[0] = 0;
		high[0] = mac68k_machine.mach_memsize * (1024 * 1024);
		if (mac68k_machine.do_graybars)
			printf("Faked range to byte 0x%lx.\n", high[0]);
	}
	nextpa = load_addr + m68k_round_page(esym);

	if (mac68k_machine.do_graybars)
		printf("Bootstrapping the pmap system.\n");

	pmap_bootstrap(nextpa, load_addr);

	if (mac68k_machine.do_graybars)
		printf("Pmap bootstrapped.\n");

	if (!vidlen)
		panic("Don't know how to relocate video!");

	if (mac68k_machine.do_graybars)
		printf("Moving ROMBase from %p to %p.\n", oldROMBase, ROMBase);

	mrg_fixupROMBase(oldROMBase, ROMBase);

	if (mac68k_machine.do_graybars)
		printf("Video address 0x%lx -> 0x%lx.\n",
		    (unsigned long)videoaddr, (unsigned long)newvideoaddr);

	mac68k_set_io_offsets(IOBase);

	/*
	 * If the serial ports are going (for console or 'echo'), then
	 * we need to make sure the IO change gets propagated properly.
	 * This resets the base addresses for the 8530 (serial) driver.
	 *
	 * WARNING!!! No printfs() (etc) BETWEEN zs_init() and the end
	 * of this function (where we start using the MMU, so the new
	 * address is correct.
	 */
#if NZSC > 0
	if (zsinited != 0)
		zs_init();
#endif

	videoaddr = newvideoaddr;
}

void
pmap_init_md(void)
{
	vaddr_t addr;

	addr = (vaddr_t)IOBase;
	if (uvm_map(kernel_map, &addr,
		    m68k_ptob(IIOMAPSIZE + ROMMAPSIZE + VIDMAPSIZE),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE,
				UVM_INH_NONE, UVM_ADV_RANDOM,
				UVM_FLAG_FIXED)) != 0)
		panic("pmap_init_md: uvm_map failed");
}
