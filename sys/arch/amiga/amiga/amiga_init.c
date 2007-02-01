/*	$NetBSD: amiga_init.c,v 1.92.12.1 2007/02/01 08:47:58 ad Exp $	*/

/*
 * Copyright (c) 1994 Michael L. Hitch
 * Copyright (c) 1993 Markus Wild
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
 *      This product includes software developed by Markus Wild.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include "opt_amigaccgrf.h"
#include "opt_p5ppc68kboard.h"
#include "opt_devreload.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: amiga_init.c,v 1.92.12.1 2007/02/01 08:47:58 ad Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <uvm/uvm_extern.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/buf.h>
#include <sys/msgbuf.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/dkbad.h>
#include <sys/reboot.h>
#include <sys/exec.h>
#include <machine/pte.h>
#include <machine/cpu.h>
#include <amiga/amiga/cc.h>
#include <amiga/amiga/cia.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cfdev.h>
#include <amiga/amiga/drcustom.h>
#include <amiga/amiga/gayle.h>
#include <amiga/amiga/memlist.h>
#include <amiga/dev/zbusvar.h>

#define RELOC(v, t)	*((t*)((u_int)&(v) + loadbase))

extern u_int	lowram;
extern u_int	Sysptmap, Sysptsize, Sysseg, Umap, proc0paddr;
extern u_int	Sysseg_pa;
extern u_int	virtual_avail;
#if defined(M68040) || defined(M68060)
extern int	protostfree;
#endif
extern u_long boot_partition;
vaddr_t		amiga_uptbase;
#ifdef P5PPC68KBOARD
extern int	p5ppc;
#endif

extern char *esym;

#ifdef GRF_AGA
extern u_long aga_enable;
#endif

extern u_long noncontig_enable;

/*
 * some addresses used in locore
 */
vaddr_t INTREQRaddr;
vaddr_t INTREQWaddr;

/*
 * these are used by the extended spl?() macros.
 */
volatile unsigned short *amiga_intena_read, *amiga_intena_write;

/*
 * the number of pages in our hw mapping and the start address
 */
vaddr_t amigahwaddr;
u_int namigahwpg;

vaddr_t CHIPMEMADDR;
vaddr_t chipmem_start;
vaddr_t chipmem_end;

vaddr_t z2mem_start;		/* XXX */
static vaddr_t z2mem_end;		/* XXX */
int use_z2_mem = 1;			/* XXX */

u_long boot_fphystart, boot_fphysize, boot_cphysize;
static u_int start_c_fphystart;
static u_int start_c_pstart;

static u_long boot_flags;

struct boot_memlist *memlist;

struct cfdev *cfdev;
int ncfdev;

u_long scsi_nosync;
int shift_nosync;

void  start_c(int, u_int, u_int, u_int, char *, u_int, u_long, u_long, u_int);
void rollcolor(int);
#ifdef DEVRELOAD
static int kernel_image_magic_size(void);
static void kernel_image_magic_copy(u_char *);
int kernel_reload_write(struct uio *);
extern void kernel_reload(char *, u_long, u_long, u_long, u_long,
	u_long, u_long, u_long, u_long, u_long, u_long);
#endif
extern void etext(void);
void start_c_finish(void);

void *
chipmem_steal(long amount)
{
	/*
	 * steal from top of chipmem, so we don't collide with
	 * the kernel loaded into chipmem in the not-yet-mapped state.
	 */
	vaddr_t p = chipmem_end - amount;
	if (p & 1)
		p = p - 1;
	chipmem_end = p;
	if(chipmem_start > chipmem_end)
		panic("not enough chip memory");
	return((void *)p);
}

/*
 * XXX
 * used by certain drivers currently to allocate zorro II memory
 * for bounce buffers, if use_z2_mem is NULL, chipmem will be
 * returned instead.
 * XXX
 */
void *
alloc_z2mem(amount)
	long amount;
{
	if (use_z2_mem && z2mem_end && (z2mem_end - amount) >= z2mem_start) {
		z2mem_end -= amount;
		return ((void *)z2mem_end);
	}
	return (alloc_chipmem(amount));
}


/*
 * this is the C-level entry function, it's called from locore.s.
 * Preconditions:
 *	Interrupts are disabled
 *	PA may not be == VA, so we may have to relocate addresses
 *		before enabling the MMU
 * 	Exec is no longer available (because we're loaded all over
 *		low memory, no ExecBase is available anymore)
 *
 * It's purpose is:
 *	Do the things that are done in locore.s in the hp300 version,
 *		this includes allocation of kernel maps and enabling the MMU.
 *
 * Some of the code in here is `stolen' from Amiga MACH, and was
 * written by Bryan Ford and Niklas Hallqvist.
 *
 * Very crude 68040 support by Michael L. Hitch.
 *
 */

int kernel_copyback = 1;

__attribute__ ((no_instrument_function))
void
start_c(id, fphystart, fphysize, cphysize, esym_addr, flags, inh_sync,
	boot_part, loadbase)
	int id;
	u_int fphystart, fphysize, cphysize;
	char *esym_addr;
	u_int flags;
	u_long inh_sync;
	u_long boot_part;
	u_int loadbase;
{
	extern char end[];
	extern u_int protorp[2];
	struct cfdev *cd;
	u_int pstart, pend, vstart, vend, avail;
	u_int pt, ptpa, ptsize, ptextra, kstsize;
	u_int Sysptmap_pa;
	register st_entry_t sg_proto, *sg, *esg;
	register pt_entry_t pg_proto, *pg;
	u_int end_loaded, ncd, i;
	struct boot_memlist *ml;

#ifdef DEBUG_KERNEL_START
	/* XXX this only is valid if Altais is in slot 0 */
	volatile u_int8_t *altaiscolpt = (u_int8_t *)0x200003c8;
	volatile u_int8_t *altaiscol = (u_int8_t *)0x200003c9;
#endif

#ifdef DEBUG_KERNEL_START
	if ((id>>24)==0x7D) {
		*altaiscolpt = 0;
		*altaiscol = 40;
		*altaiscol = 0;
		*altaiscol = 0;
	} else
((volatile struct Custom *)0xdff000)->color[0] = 0xa00;		/* RED */
#endif

#ifdef LIMITMEM
	if (fphysize > LIMITMEM*1024*1024)
		fphysize = LIMITMEM*1024*1024;
#endif

	RELOC(boot_fphystart, u_long) = fphystart;
	RELOC(boot_fphysize, u_long) = fphysize;
	RELOC(boot_cphysize, u_long) = cphysize;

	RELOC(machineid, int) = id;
	RELOC(chipmem_end, vaddr_t) = cphysize;
	RELOC(esym, char *) = esym_addr;
	RELOC(boot_flags, u_long) = flags;
	RELOC(boot_partition, u_long) = boot_part;
#ifdef GRF_AGA
	if (flags & 1)
		RELOC(aga_enable, u_long) |= 1;
#endif
	if (flags & (3 << 1))
		RELOC(noncontig_enable, u_long) = (flags >> 1) & 3;

	RELOC(scsi_nosync, u_long) = inh_sync;

	/*
	 * the kernel ends at end(), plus the cfdev and memlist structures
	 * we placed there in the loader.  Correct for this now.  Also,
	 * account for kernel symbols if they are present.
	 */
	if (esym_addr == NULL)
		end_loaded = (u_int) &end;
	else
		end_loaded = (u_int) esym_addr;
	RELOC(ncfdev, int) = *(int *)(&RELOC(*(u_int *)end_loaded, u_int));
	RELOC(cfdev, struct cfdev *) = (struct cfdev *) ((int)end_loaded + 4);
	end_loaded += 4 + RELOC(ncfdev, int) * sizeof(struct cfdev);

	RELOC(memlist, struct boot_memlist *) =
	    (struct boot_memlist *)end_loaded;
	ml = &RELOC(*(struct boot_memlist *)end_loaded, struct boot_memlist);
	end_loaded = (u_int) &((RELOC(memlist, struct boot_memlist *))->
	    m_seg[ml->m_nseg]);

	/*
	 * Get ZorroII (16-bit) memory if there is any and it's not where the
	 * kernel is loaded.
	 */
	if (ml->m_nseg > 0 && ml->m_nseg < 16 && RELOC(use_z2_mem, int)) {
		struct boot_memseg *sp, *esp;

		sp = ml->m_seg;
		esp = sp + ml->m_nseg;
		for (; sp < esp; sp++) {
			if ((sp->ms_attrib & (MEMF_FAST | MEMF_24BITDMA))
			    != (MEMF_FAST|MEMF_24BITDMA))
				continue;
			if (sp->ms_start == fphystart)
				continue;
			RELOC(z2mem_end, paddr_t) =
			    sp->ms_start + sp->ms_size;
			RELOC(z2mem_start, paddr_t) =
			    RELOC(z2mem_end, paddr_t) - MAXPHYS *
			    RELOC(use_z2_mem, int) * 7;
			RELOC(NZTWOMEMPG, u_int) =
			    (RELOC(z2mem_end, paddr_t) -
			    RELOC(z2mem_start, paddr_t)) / PAGE_SIZE;
			if ((RELOC(z2mem_end, paddr_t) -
			    RELOC(z2mem_start, paddr_t)) > sp->ms_size) {
				RELOC(NZTWOMEMPG, u_int) = sp->ms_size /
				    PAGE_SIZE;
				RELOC(z2mem_start, paddr_t) =
				    RELOC(z2mem_end, paddr_t) - sp->ms_size;
			}
			break;
		}
	}

	/*
	 * Scan ConfigDev list and get size of Zorro I/O boards that are
	 * outside the Zorro II I/O area.
	 */
	for (RELOC(ZBUSAVAIL, u_int) = 0, cd =
	    &RELOC(*RELOC(cfdev, struct cfdev *),struct cfdev),
	    ncd = RELOC(ncfdev, int); ncd > 0; ncd--, cd++) {
		int bd_type = cd->rom.type & (ERT_TYPEMASK | ERTF_MEMLIST);

		if (bd_type != ERT_ZORROIII &&
		    (bd_type != ERT_ZORROII || isztwopa(cd->addr)))
			continue;	/* It's not Z2 or Z3 I/O board */
		/*
		 *  Hack to adjust board size for Zorro III boards that
		 *  do not specify an extended size or subsize.  This is
		 *  specifically for the GVP Spectrum and hopefully won't
		 *  break with other boards that configure like this.
		 */
		if (bd_type == ERT_ZORROIII &&
		    !(cd->rom.flags & ERFF_EXTENDED) &&
		    (cd->rom.flags & ERT_Z3_SSMASK) == 0)
			cd->size = 0x10000 <<
			    ((cd->rom.type - 1) & ERT_MEMMASK);
		RELOC(ZBUSAVAIL, u_int) += m68k_round_page(cd->size);
	}

	/*
	 * assume KVA_MIN == 0.  We subtract the kernel code (and
	 * the configdev's and memlists) from the virtual and
	 * phsical starts and ends.
	 */
	vend   = fphysize;
	avail  = vend;
	vstart = (u_int) end_loaded;
	vstart = m68k_round_page (vstart);
	pstart = vstart + fphystart;
	pend   = vend   + fphystart;
	avail -= vstart;

#if defined(M68040) || defined(M68060)
	if (RELOC(mmutype, int) == MMU_68040)
		kstsize = MAXKL2SIZE / (NPTEPG/SG4_LEV2SIZE);
	else
#endif
		kstsize = 1;

	/*
	 * allocate the kernel segment table
	 */
	RELOC(Sysseg_pa, u_int) = pstart;
	RELOC(Sysseg, u_int) = vstart;
	vstart += PAGE_SIZE * kstsize;
	pstart += PAGE_SIZE * kstsize;
	avail -= PAGE_SIZE * kstsize;

	/*
	 * allocate initial page table pages
	 */
	pt = vstart;
	ptpa = pstart;
#ifdef DRACO
	if ((id>>24)==0x7D) {
		ptextra = NDRCCPG
		    + RELOC(NZTWOMEMPG, u_int)
		    + btoc(RELOC(ZBUSAVAIL, u_int));
	} else
#endif
	ptextra = NCHIPMEMPG + NCIAPG + NZTWOROMPG + RELOC(NZTWOMEMPG, u_int) +
	    btoc(RELOC(ZBUSAVAIL, u_int)) + NPCMCIAPG;

	ptsize = (RELOC(Sysptsize, u_int) +
	    howmany(ptextra, NPTEPG)) << PGSHIFT;

	vstart += ptsize;
	pstart += ptsize;
	avail -= ptsize;

	/*
	 * allocate kernel page table map
	 */
	RELOC(Sysptmap, u_int) = vstart;
	Sysptmap_pa = pstart;
	vstart += PAGE_SIZE;
	pstart += PAGE_SIZE;
	avail -= PAGE_SIZE;

	/*
	 * pt maps the first N megs of ram Sysptmap comes directly
	 * after pt (ptpa) and so it must map >= N meg + Its one
	 * page and so it must map 8M of space.  Specifically
	 * Sysptmap holds the pte's that map the kerne page tables.
	 *
	 * We want Sysmap to be the first address mapped by Sysptmap.
	 * this will be the address just above what pt,pt+ptsize maps.
	 * pt[0] maps address 0 so:
	 *
	 *		ptsize
	 * Sysmap  =	------ * PAGE_SIZE
	 *		  4
	 */
	RELOC(Sysmap, u_int *) = (u_int *)(ptsize * (PAGE_SIZE / 4));

	/*
	 * initialize segment table and page table map
	 */
#if defined(M68040) || defined(M68060)
	if (RELOC(mmutype, int) == MMU_68040) {
		/*
		 * First invalidate the entire "segment table" pages
		 * (levels 1 and 2 have the same "invalid" values).
		 */
		sg = (u_int *)RELOC(Sysseg_pa, u_int);
		esg = &sg[kstsize * NPTEPG];
		while (sg < esg)
			*sg++ = SG_NV;
		/*
		 * Initialize level 2 descriptors (which immediately
		 * follow the level 1 table).  We need:
		 *	NPTEPG / SG4_LEV3SIZE
		 * level 2 descriptors to map each of the nptpages + 1
		 * pages of PTEs.  Note that we set the "used" bit
		 * now to save the HW the expense of doing it.
		 */
		i = ((ptsize >> PGSHIFT) + 1) * (NPTEPG / SG4_LEV3SIZE);
		sg = &((u_int *)(RELOC(Sysseg_pa, u_int)))[SG4_LEV1SIZE];
		esg = &sg[i];
		sg_proto = ptpa | SG_U | SG_RW | SG_V;
		while (sg < esg) {
			*sg++ = sg_proto;
			sg_proto += (SG4_LEV3SIZE * sizeof (st_entry_t));
		}
		/*
		 * Initialize level 1 descriptors.  We need:
		 *	roundup(num, SG4_LEV2SIZE) / SG4_LEVEL2SIZE
		 * level 1 descriptors to map the 'num' level 2's.
		 */
		i = roundup(i, SG4_LEV2SIZE) / SG4_LEV2SIZE;
		RELOC(protostfree, u_int) =
		    (-1 << (i + 1)) /* & ~(-1 << MAXKL2SIZE) */;
		sg = (u_int *) RELOC(Sysseg_pa, u_int);
		esg = &sg[i];
		sg_proto = (u_int)&sg[SG4_LEV1SIZE] | SG_U | SG_RW |SG_V;
		while (sg < esg) {
			*sg++ = sg_proto;
			sg_proto += (SG4_LEV2SIZE * sizeof(st_entry_t));
		}
		/*
		 * Initialize Sysptmap
		 */
		sg = (u_int *) Sysptmap_pa;
		esg = &sg[(ptsize >> PGSHIFT) + 1];
		pg_proto = ptpa | PG_RW | PG_CI | PG_V;
		while (sg < esg) {
			*sg++ = pg_proto;
			pg_proto += PAGE_SIZE;
		}
		/*
		 * Invalidate rest of Sysptmap page
		 */
		esg = (u_int *)(Sysptmap_pa + PAGE_SIZE);
		while (sg < esg)
			*sg++ = SG_NV;
	} else
#endif /* M68040 */
	{
		/*
		 * Map the page table pages in both the HW segment table
		 * and the software Sysptmap.  Note that Sysptmap is also
		 * considered a PT page, hence the +1.
		 */
		sg = (u_int *)RELOC(Sysseg_pa, u_int);
		pg = (u_int *)Sysptmap_pa;
		esg = &pg[(ptsize >> PGSHIFT) + 1];
		sg_proto = ptpa | SG_RW | SG_V;
		pg_proto = ptpa | PG_RW | PG_CI | PG_V;
		while (pg < esg) {
			*sg++ = sg_proto;
			*pg++ = pg_proto;
			sg_proto += PAGE_SIZE;
			pg_proto += PAGE_SIZE;
		}
		/*
		 * invalidate the remainder of each table
		 */
		esg = (u_int *)(Sysptmap_pa + PAGE_SIZE);
		while (pg < esg) {
			*sg++ = SG_NV;
			*pg++ = PG_NV;
		}
	}

	/*
	 * initialize kernel page table page(s) (assume load at VA 0)
	 */
	pg_proto = fphystart | PG_RO | PG_V;	/* text pages are RO */
	pg       = (u_int *) ptpa;
	*pg++ = PG_NV;				/* Make page 0 invalid */
	pg_proto += PAGE_SIZE;
	for (i = PAGE_SIZE; i < (u_int) etext;
	     i += PAGE_SIZE, pg_proto += PAGE_SIZE)
		*pg++ = pg_proto;

	/*
	 * data, bss and dynamic tables are read/write
	 */
	pg_proto = (pg_proto & PG_FRAME) | PG_RW | PG_V;

#if defined(M68040) || defined(M68060)
	/*
	 * map the kernel segment table cache invalidated for
	 * these machines (for the 68040 not strictly necessary, but
	 * recommended by Motorola; for the 68060 mandatory)
	 */
	if (RELOC(mmutype, int) == MMU_68040) {

		if (RELOC(kernel_copyback, int))
			pg_proto |= PG_CCB;

		/*
		 * ASSUME: segment table and statically allocated page tables
		 * of the kernel are contiguously allocated, start at
		 * Sysseg and end at the current value of vstart.
		 */
		for (; i<RELOC(Sysseg, u_int);
		     i+= PAGE_SIZE, pg_proto += PAGE_SIZE)
			*pg++ = pg_proto;

		pg_proto = (pg_proto & ~PG_CCB) | PG_CI;
		for (; i < vstart; i += PAGE_SIZE, pg_proto += PAGE_SIZE)
			*pg++ = pg_proto;

		pg_proto = (pg_proto & ~PG_CI);
		if (RELOC(kernel_copyback, int))
			pg_proto |= PG_CCB;
	}
#endif
	/*
	 * go till end of data allocated so far
	 * plus proc0 u-area (to be allocated)
	 */
	for (; i < vstart + USPACE; i += PAGE_SIZE, pg_proto += PAGE_SIZE)
		*pg++ = pg_proto;
	/*
	 * invalidate remainder of kernel PT
	 */
	while (pg < (u_int *) (ptpa + ptsize))
		*pg++ = PG_NV;

	/*
	 * go back and validate internal IO PTEs
	 * at end of allocated PT space
	 */
	pg      -= ptextra;
#ifdef DRACO
	if ((id >> 24) == 0x7D) {
		pg_proto = DRCCBASE | PG_RW | PG_CI | PG_V;
		while (pg_proto < DRZ2BASE) {
			*pg++ = pg_proto;
			pg_proto += DRCCSTRIDE;
		}

		/* NCR 53C710 chip */
		*pg++ = DRSCSIBASE | PG_RW | PG_CI | PG_V;

#ifdef DEBUG_KERNEL_START
		/*
		 * early rollcolor Altais mapping
		 * XXX (only works if in slot 0)
		 */
		*pg++ = 0x20000000 | PG_RW | PG_CI | PG_V;
#endif
	} else
#endif
	{
		pg_proto = CHIPMEMBASE | PG_RW | PG_CI | PG_V;
						/* CI needed here?? */
		while (pg_proto < CHIPMEMTOP) {
			*pg++     = pg_proto;
			pg_proto += PAGE_SIZE;
		}
	}
	if (RELOC(z2mem_end, paddr_t)) {			/* XXX */
		pg_proto = RELOC(z2mem_start, paddr_t) |	/* XXX */
		    PG_RW | PG_V;				/* XXX */
		while (pg_proto < RELOC(z2mem_end, paddr_t)) { /* XXX */
			*pg++ = pg_proto;			/* XXX */
			pg_proto += PAGE_SIZE;			/* XXX */
		}						/* XXX */
	}							/* XXX */
#ifdef DRACO
	if ((id >> 24) != 0x7D)
#endif
	{
		pg_proto = CIABASE | PG_RW | PG_CI | PG_V;
		while (pg_proto < CIATOP) {
			*pg++     = pg_proto;
			pg_proto += PAGE_SIZE;
		}
		pg_proto  = ZTWOROMBASE | PG_RW | PG_CI | PG_V;
		while (pg_proto < ZTWOROMTOP) {
			*pg++     = pg_proto;
			pg_proto += PAGE_SIZE;
		}
	}

	/*
	 *[ following page tables MAY be allocated to ZORRO3 space,
	 * but they're then later mapped in autoconf.c ]
	 */

	/* zero out proc0 user area */
/*	bzero ((u_char *)pstart, USPACE);*/	/* XXXXXXXXXXXXXXXXXXXXX */

	/*
	 * save KVA of proc0 u-area and allocate it.
	 */
	RELOC(proc0paddr, u_int) = vstart;
	pstart += USPACE;
	vstart += USPACE;
	avail -= USPACE;

	/*
	 * init mem sizes
	 */
	RELOC(maxmem, u_int)  = pend >> PGSHIFT;
	RELOC(lowram, u_int)  = fphystart;
	RELOC(physmem, u_int) = fphysize >> PGSHIFT;

	/*
	 * Put user page tables starting at next 16MB boundary, to make kernel
	 * dumps more readable, with guaranteed 16MB of.
	 * XXX depends on Sysmap being last.
	 * XXX 16 MB instead of 256 MB should be enough, but...
	 * we need to fix the fastmem loading first. (see comment at line 375)
	 */
	RELOC(amiga_uptbase, vaddr_t) =
	    roundup(RELOC(Sysmap, u_int) + 0x10000000, 0x10000000);

	/*
	 * get the pmap module in sync with reality.
	 */
/*	pmap_bootstrap(pstart, fphystart);*/	/* XXXXXXXXXXXXXXXXXXXXXXx*/

	/*
	 * record base KVA of IO spaces which are just before Sysmap
	 */
#ifdef DRACO
	if ((id >> 24) == 0x7D) {
		RELOC(DRCCADDR, u_int) =
		    (u_int)RELOC(Sysmap, u_int) - ptextra * PAGE_SIZE;

		RELOC(CIAADDR, vaddr_t) =
		    RELOC(DRCCADDR, u_int) + DRCIAPG * PAGE_SIZE;

		if (RELOC(z2mem_end, vaddr_t)) {		/* XXX */
			RELOC(ZTWOMEMADDR, vaddr_t) =
			    RELOC(DRCCADDR, u_int) + NDRCCPG * PAGE_SIZE;

			RELOC(ZBUSADDR, vaddr_t) =
			    RELOC(ZTWOMEMADDR, vaddr_t) +
			    RELOC(NZTWOMEMPG, u_int)*PAGE_SIZE;
		} else {
			RELOC(ZBUSADDR, vaddr_t) =
			    RELOC(DRCCADDR, u_int) + NDRCCPG * PAGE_SIZE;
		}

		/*
		 * some nice variables for pmap to use
		 */
		RELOC(amigahwaddr, vaddr_t) = RELOC(DRCCADDR, u_int);
	} else
#endif
	{
		RELOC(CHIPMEMADDR, vaddr_t) =
		    (u_int)RELOC(Sysmap, u_int) - ptextra * PAGE_SIZE;
		if (RELOC(z2mem_end, u_int) == 0)
			RELOC(CIAADDR, vaddr_t) =
			    RELOC(CHIPMEMADDR, vaddr_t) +
			    NCHIPMEMPG * PAGE_SIZE;
		else {
			RELOC(ZTWOMEMADDR, vaddr_t) =
			    RELOC(CHIPMEMADDR, vaddr_t) +
			    NCHIPMEMPG * PAGE_SIZE;
			RELOC(CIAADDR, vaddr_t) = RELOC(ZTWOMEMADDR, vaddr_t) +
				RELOC(NZTWOMEMPG, u_int) * PAGE_SIZE;
		}
		RELOC(ZTWOROMADDR, vaddr_t)  =
		    RELOC(CIAADDR, vaddr_t) + NCIAPG * PAGE_SIZE;
		RELOC(ZBUSADDR, vaddr_t) =
		    RELOC(ZTWOROMADDR, vaddr_t) + NZTWOROMPG * PAGE_SIZE;
		/* not on 8k boundary :-( */
		RELOC(CIAADDR, vaddr_t) += PAGE_SIZE/2;
		RELOC(CUSTOMADDR, vaddr_t)  =
		    RELOC(ZTWOROMADDR, vaddr_t) - ZTWOROMBASE + CUSTOMBASE;
		/*
		 * some nice variables for pmap to use
		 */
		RELOC(amigahwaddr, vaddr_t) = RELOC(CHIPMEMADDR, vaddr_t);
	}

	/* Set number of pages to reserve for mapping Amiga hardware pages */
	RELOC(namigahwpg, u_int) = ptextra;

	/*
	 * set this before copying the kernel, so the variable is updated in
	 * the `real' place too. protorp[0] is already preset to the
	 * CRP setting.
	 */
	RELOC(protorp[1], u_int) = RELOC(Sysseg_pa, u_int);

	RELOC(start_c_fphystart, u_int) = fphystart;
	RELOC(start_c_pstart, u_int) = pstart;

	/*
	 * copy over the kernel (and all now initialized variables)
	 * to fastram.  DONT use bcopy(), this beast is much larger
	 * than 128k !
	 */
	if (loadbase == 0) {
		register u_int *lp, *le, *fp;

		lp = 0;
		le = (u_int *)end_loaded;
		fp = (u_int *)fphystart;
		while (lp < le)
			*fp++ = *lp++;
	}

#ifdef DEBUG_KERNEL_START
	if ((id>>24)==0x7D) {
		*altaiscolpt = 0;
		*altaiscol = 40;
		*altaiscol = 40;
		*altaiscol = 0;
	} else
((volatile struct Custom *)0xdff000)->color[0] = 0xAA0;		/* YELLOW */
#endif
	/*
	 * prepare to enable the MMU
	 */
#if defined(M68040) || defined(M68060)
	if (RELOC(mmutype, int) == MMU_68040) {
		if (id & AMIGA_68060) {
			/* do i need to clear the branch cache? */
			__asm volatile (	".word 0x4e7a,0x0002;"
					"orl #0x400000,%%d0;"
					".word 0x4e7b,0x0002" : : : "d0");
		}

		/*
		 * movel Sysseg_pa,%a0;
		 * movec %a0,%srp;
		 */

		__asm volatile ("movel %0,%%a0; .word 0x4e7b,0x8807"
		    : : "a" (RELOC(Sysseg_pa, u_int)) : "a0");

#ifdef DEBUG_KERNEL_START
		if ((id>>24)==0x7D) {
			*altaiscolpt = 0;
			*altaiscol = 40;
			*altaiscol = 33;
			*altaiscol = 0;
		} else
((volatile struct Custom *)0xdff000)->color[0] = 0xA70;		/* ORANGE */
#endif
	} else
#endif
	{
		/*
		 * setup and load SRP
		 * nolimit, share global, 4 byte PTE's
		 */
		(RELOC(protorp[0], u_int)) = 0x80000202;
		__asm volatile ("pmove %0@,%%srp":: "a" (&RELOC(protorp, u_int)));
	}
}

void
start_c_finish()
{
	extern u_int32_t delaydivisor;
#ifdef	P5PPC68KBOARD
        struct cfdev *cdp, *ecdp;
#endif

#ifdef DEBUG_KERNEL_START
#ifdef DRACO
	if ((id >> 24) == 0x7D) { /* mapping on, is_draco() is valid */
		int i;
		/* XXX experimental Altais register mapping only */
		altaiscolpt = (volatile u_int8_t *)(DRCCADDR+PAGE_SIZE*9+0x3c8);
		altaiscol = altaiscolpt + 1;
		for (i=0; i<140000; i++) {
			*altaiscolpt = 0;
			*altaiscol = 0;
			*altaiscol = 40;
			*altaiscol = 0;
		}
	} else
#endif
((volatile struct Custom *)CUSTOMADDR)->color[0] = 0x0a0;	/* GREEN */
#endif

	bzero ((u_char *)proc0paddr, USPACE);
	pmap_bootstrap(start_c_pstart, start_c_fphystart);

	/*
	 * to make life easier in locore.s, set these addresses explicitly
	 */
	CIAAbase = CIAADDR + 0x1001;	/* CIA-A at odd addresses ! */
	CIABbase = CIAADDR;
	CUSTOMbase = CUSTOMADDR;
#ifdef DRACO
	if (is_draco()) {
		draco_intena = (volatile u_int8_t *)DRCCADDR+1;
		draco_intpen = draco_intena + PAGE_SIZE;
		draco_intfrc = draco_intpen + PAGE_SIZE;
		draco_misc = draco_intfrc + PAGE_SIZE;
		draco_ioct = (struct drioct *)(DRCCADDR + DRIOCTLPG*PAGE_SIZE);
	} else
#endif
	{
		INTREQRaddr = (vaddr_t)&custom.intreqr;
		INTREQWaddr = (vaddr_t)&custom.intreq;
	}
	/*
	 * Get our chip memory allocation system working
	 */
	chipmem_start += CHIPMEMADDR;
	chipmem_end   += CHIPMEMADDR;

	/* XXX is: this MUST NOT BE DONE before the pmap_bootstrap() call */
	if (z2mem_end) {
		z2mem_end = ZTWOMEMADDR + NZTWOMEMPG * PAGE_SIZE;
		z2mem_start = ZTWOMEMADDR;
	}

#if 0
	i = *(int *)proc0paddr;
	*(volatile int *)proc0paddr = i;
#endif

	/*
	 * disable all interrupts but enable allow them to be enabled
	 * by specific driver code (global int enable bit)
	 */
#ifdef DRACO
	if (is_draco()) {
		/* XXX to be done. For now, just: */
		*draco_intena = 0;
		*draco_intpen = 0;
		*draco_intfrc = 0;
		ciaa.icr = 0x7f;			/* and keyboard */
		ciab.icr = 0x7f;			/* and again */

		draco_ioct->io_control &=
		    ~(DRCNTRL_KBDINTENA|DRCNTRL_FDCINTENA); /* and another */

		draco_ioct->io_status2 &=
		    ~(DRSTAT2_PARIRQENA|DRSTAT2_TMRINTENA); /* some more */

		*(volatile u_int8_t *)(DRCCADDR + 1 +
		    DRSUPIOPG*PAGE_SIZE + 4*(0x3F8 + 1)) = 0; /* and com0 */

		*(volatile u_int8_t *)(DRCCADDR + 1 +
		    DRSUPIOPG*PAGE_SIZE + 4*(0x2F8 + 1)) = 0; /* and com1 */

		draco_ioct->io_control |= DRCNTRL_WDOGDIS; /* stop Fido */
		*draco_misc &= ~1/*DRMISC_FASTZ2*/;

	} else
#endif
	{
		custom.intena = 0x7fff;			/* disable ints */
		custom.intena = INTF_SETCLR | INTF_INTEN;
							/* but allow them */
		custom.intreq = 0x7fff;			/* clear any current */
		ciaa.icr = 0x7f;			/* and keyboard */
		ciab.icr = 0x7f;			/* and again */

		/*
		 * remember address of read and write intena register for use
		 * by extended spl?() macros.
		 */
		amiga_intena_read  = &custom.intenar;
		amiga_intena_write = &custom.intena;
	}

	/*
	 * This is needed for 3000's with superkick ROM's. Bit 7 of
	 * 0xde0002 enables the ROM if set. If this isn't set the machine
	 * has to be powercycled in order for it to boot again. ICKA! RFH
	 */
	if (is_a3000()) {
		volatile unsigned char *a3000_magic_reset;

		a3000_magic_reset = (volatile unsigned char *)ztwomap(0xde0002);

		/* Turn SuperKick ROM (V36) back on */
		*a3000_magic_reset |= 0x80;
	}

#ifdef	P5PPC68KBOARD
	/*
	 * Are we an P5 PPC/68K board? install different reset
	 * routine.
	 */

        for (cdp = cfdev, ecdp = &cfdev[ncfdev]; cdp < ecdp; cdp++) {
		if (cdp->rom.manid == 8512 &&
		    (cdp->rom.prodid == 100 || cdp->rom.prodid == 110)) {
		    		p5ppc = 1;
				break;
			}
        }
#endif
	/*
	 * preliminary delay divisor value
	 */

	if (machineid & AMIGA_68060)
		delaydivisor = (1024 * 1) / 80;	/* 80 MHz 68060 w. BTC */

	else if (machineid & AMIGA_68040)
		delaydivisor = (1024 * 3) / 40;	/* 40 MHz 68040 */

	else if (machineid & AMIGA_68030)
		delaydivisor = (1024 * 8) / 50;	/* 50 MHz 68030 */

	else
		delaydivisor = (1024 * 8) / 33; /* 33 MHz 68020 */
}

void
rollcolor(color)
	int color;
{
	int s, i;

	s = splhigh();
	/*
	 * need to adjust count -
	 * too slow when cache off, too fast when cache on
	 */
	for (i = 0; i < 400000; i++)
		((volatile struct Custom *)CUSTOMbase)->color[0] = color;
	splx(s);
}

#ifdef DEVRELOAD
/*
 * Kernel reloading code
 */

static struct exec kernel_exec;
static u_char *kernel_image;
static u_long kernel_text_size, kernel_load_ofs;
static u_long kernel_load_phase;
static u_long kernel_load_endseg;
static u_long kernel_symbol_size, kernel_symbol_esym;

/* This supports the /dev/reload device, major 2, minor 20,
   hooked into mem.c.  Author: Bryan Ford.  */

/*
 * This is called below to find out how much magic storage
 * will be needed after a kernel image to be reloaded.
 */
static int
kernel_image_magic_size()
{
	int sz;

	/* 4 + cfdev's + Mem_Seg's + 4 */
	sz = 8 + ncfdev * sizeof(struct cfdev)
	    + memlist->m_nseg * sizeof(struct boot_memseg);
	return(sz);
}

/* This actually copies the magic information.  */
static void
kernel_image_magic_copy(dest)
	u_char *dest;
{
	*((int*)dest) = ncfdev;
	dest += 4;
	bcopy(cfdev, dest, ncfdev * sizeof(struct cfdev)
	    + memlist->m_nseg * sizeof(struct boot_memseg) + 4);
}

#undef AOUT_LDPGSZ
#define AOUT_LDPGSZ 8192 /* XXX ??? */

int
kernel_reload_write(uio)
	struct uio *uio;
{
	extern int eclockfreq;
	struct iovec *iov;
	int error, c;

	iov = uio->uio_iov;

	if (kernel_image == 0) {
		/*
		 * We have to get at least the whole exec header
		 * in the first write.
		 */
		if (iov->iov_len < sizeof(kernel_exec))
			return ENOEXEC;		/* XXX */

		/*
		 * Pull in the exec header and check it.
		 */
		if ((error = uiomove((caddr_t)&kernel_exec, sizeof(kernel_exec),
		     uio)) != 0)
			return(error);
		printf("loading kernel %ld+%ld+%ld+%ld\n", kernel_exec.a_text,
		    kernel_exec.a_data, kernel_exec.a_bss,
		    esym == NULL ? 0 : kernel_exec.a_syms);
		/*
		 * Looks good - allocate memory for a kernel image.
		 */
		kernel_text_size = (kernel_exec.a_text
			+ AOUT_LDPGSZ - 1) & (-AOUT_LDPGSZ);
		/*
		 * Estimate space needed for symbol names, since we don't
		 * know how big it really is.
		 */
		if (esym != NULL) {
			kernel_symbol_size = kernel_exec.a_syms;
			kernel_symbol_size += 16 * (kernel_symbol_size / 12);
		}
		/*
		 * XXX - should check that image will fit in CHIP memory
		 * XXX return an error if it doesn't
		 */
		if ((kernel_text_size + kernel_exec.a_data +
		    kernel_exec.a_bss + kernel_symbol_size +
		    kernel_image_magic_size()) > boot_cphysize)
			return (EFBIG);
		kernel_image = malloc(kernel_text_size + kernel_exec.a_data
			+ kernel_exec.a_bss
			+ kernel_symbol_size
			+ kernel_image_magic_size(),
			M_TEMP, M_WAITOK);
		kernel_load_ofs = 0;
		kernel_load_phase = 0;
		kernel_load_endseg = kernel_exec.a_text;
		return(0);
	}
	/*
	 * Continue loading in the kernel image.
	 */
	c = min(iov->iov_len, kernel_load_endseg - kernel_load_ofs);
	c = min(c, MAXPHYS);
	if ((error = uiomove(kernel_image + kernel_load_ofs, (int)c, uio)) != 0)
		return(error);
	kernel_load_ofs += c;

	/*
	 * Fun and games to handle loading symbols - the length of the
	 * string table isn't know until after the symbol table has
	 * been loaded.  We have to load the kernel text, data, and
	 * the symbol table, then get the size of the strings.  A
	 * new kernel image is then allocated and the data currently
	 * loaded moved to the new image.  Then continue reading the
	 * string table.  This has problems if there isn't enough
	 * room to allocate space for the two copies of the kernel
	 * image.  So the approach I took is to guess at the size
	 * of the symbol strings.  If the guess is wrong, the symbol
	 * table is ignored.
	 */

	if (kernel_load_ofs != kernel_load_endseg)
		return(0);

	switch (kernel_load_phase) {
	case 0:		/* done loading kernel text */
		kernel_load_ofs = kernel_text_size;
		kernel_load_endseg = kernel_load_ofs + kernel_exec.a_data;
		kernel_load_phase = 1;
		break;
	case 1:		/* done loading kernel data */
		for(c = 0; c < kernel_exec.a_bss; c++)
			kernel_image[kernel_load_ofs + c] = 0;
		kernel_load_ofs += kernel_exec.a_bss;
		if (esym) {
			kernel_load_endseg = kernel_load_ofs
			    + kernel_exec.a_syms + 8;
			*((u_long *)(kernel_image + kernel_load_ofs)) =
			    kernel_exec.a_syms;
			kernel_load_ofs += 4;
			kernel_load_phase = 3;
			break;
		}
		/*FALLTHROUGH*/
	case 2:		/* done loading kernel */

		/*
		 * Put the finishing touches on the kernel image.
		 */
		kernel_image_magic_copy(kernel_image + kernel_load_ofs);
		/*
		 * Start the new kernel with code in locore.s.
		 */
		kernel_reload(kernel_image,
		    kernel_load_ofs + kernel_image_magic_size(),
		    kernel_exec.a_entry, boot_fphystart, boot_fphysize,
		    boot_cphysize, kernel_symbol_esym, eclockfreq,
		    boot_flags, scsi_nosync, boot_partition);
		/*
		 * kernel_reload() now checks to see if the reload_code
		 * is at the same location in the new kernel.
		 * If it isn't, it will return and we will return
		 * an error.
		 */
		free(kernel_image, M_TEMP);
		kernel_image = NULL;
		return (ENODEV);	/* Say operation not supported */
	case 3:		/* done loading kernel symbol table */
		c = *((u_long *)(kernel_image + kernel_load_ofs - 4));
		if (c > 16 * (kernel_exec.a_syms / 12))
			c = 16 * (kernel_exec.a_syms / 12);
		kernel_load_endseg += c - 4;
		kernel_symbol_esym = kernel_load_endseg;
#ifdef notyet
		kernel_image_copy = kernel_image;
		kernel_image = malloc(kernel_load_ofs + c
		    + kernel_image_magic_size(), M_TEMP, M_WAITOK);
		if (kernel_image == NULL)
			panic("kernel_reload failed second malloc");
		for (c = 0; c < kernel_load_ofs; c += MAXPHYS)
			bcopy(kernel_image_copy + c, kernel_image + c,
			    (kernel_load_ofs - c) > MAXPHYS ? MAXPHYS :
			    kernel_load_ofs - c);
#endif
		kernel_load_phase = 2;
	}
	return(0);
}
#endif
