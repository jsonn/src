/*	$NetBSD: machdep.c,v 1.376.2.30 2001/12/29 21:09:06 sommerfeld Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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

/*-
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: machdep.c,v 1.376.2.30 2001/12/29 21:09:06 sommerfeld Exp $");

#include "opt_cputype.h"
#include "opt_ddb.h"
#include "opt_ipkdb.h"
#include "opt_kgdb.h"
#include "opt_vm86.h"
#include "opt_user_ldt.h"
#include "opt_compat_netbsd.h"
#include "opt_cpureset_delay.h"
#include "opt_compat_svr4.h"
#include "opt_realmem.h"
#include "opt_compat_mach.h"	/* need to get the right segment def */
#include "opt_mtrr.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/extent.h>
#include <sys/syscallargs.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <machine/kcore.h>

#ifdef IPKDB
#include <ipkdb/ipkdb.h>
#endif

#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <dev/cons.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_page.h>

#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cpuvar.h>
#include <machine/gdt.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#include <machine/bootinfo.h>
#include <machine/mtrr.h>

#include <dev/isa/isareg.h>
#include <machine/isa_machdep.h>
#include <dev/ic/i8042reg.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#endif

#ifdef VM86
#include <machine/vm86.h>
#endif

#include "apm.h"
#include "bioscall.h"

#if NBIOSCALL > 0
#include <machine/bioscall.h>
#endif

#if NAPM > 0
#include <machine/apmvar.h>
#endif

#include "isa.h"
#include "isadma.h"
#include "npx.h"

#include "mca.h"
#if NMCA > 0
#include <machine/mca_machdep.h>	/* for mca_busprobe() */
#endif

/* the following is used externally (sysctl_hw) */
char machine[] = "i386";		/* cpu "architecture" */
char machine_arch[] = "i386";		/* machine == machine_arch */

char bootinfo[BOOTINFO_MAXSIZE];

struct bi_devmatch *i386_alldisks = NULL;
int i386_ndisks = 0;

#ifdef CPURESET_DELAY
int	cpureset_delay = CPURESET_DELAY;
#else
int     cpureset_delay = 2000; /* default to 2s */
#endif

#ifdef MTRR
struct mtrr_funcs *mtrr_funcs;
#endif


int	physmem;
int	dumpmem_low;
int	dumpmem_high;
int	boothowto;
int	cpu_class;
int	i386_fpu_present;
int	i386_fpu_exception;
int	i386_fpu_fdivbug;

int	i386_use_fxsave;
int	i386_has_sse;
int	i386_has_sse2;

int	tmx86_has_longrun;

#define	CPUID2FAMILY(cpuid)	(((cpuid) >> 8) & 15)
#define	CPUID2MODEL(cpuid)	(((cpuid) >> 4) & 15)
#define	CPUID2STEPPING(cpuid)	((cpuid) & 15)

vaddr_t	msgbuf_vaddr;
paddr_t msgbuf_paddr;

vaddr_t	idt_vaddr;
paddr_t	idt_paddr;

#ifdef I586_CPU
vaddr_t	pentium_idt_vaddr;
#endif

struct vm_map *exec_map = NULL;
struct vm_map *mb_map = NULL;
struct vm_map *phys_map = NULL;

extern	paddr_t avail_start, avail_end;

void (*delay_func) __P((int)) = i8254_delay;
void (*microtime_func) __P((struct timeval *)) = i8254_microtime;
void (*initclock_func) __P((void)) = i8254_initclocks;

/*
 * Size of memory segments, before any memory is stolen.
 */
phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];
int	mem_cluster_cnt;

int	cpu_dump __P((void));
int	cpu_dumpsize __P((void));
u_long	cpu_dump_mempagecnt __P((void));
void	dumpsys __P((void));
void	init386 __P((paddr_t));

#if !defined(REALBASEMEM) && !defined(REALEXTMEM)
void	add_mem_cluster	__P((u_int64_t, u_int64_t, u_int32_t));
#endif /* !defnied(REALBASEMEM) && !defined(REALEXTMEM) */

static const struct i386_cache_info
intel_cpuid_cache_info[] = {
	{ CAI_ITLB, 	0x01,	 4, 32, 4 * 1024 },
	{ CAI_ITLB2, 	0x02, 0xff,  2, 4 * 1024 * 1024 },
	{ CAI_DTLB, 	0x03,    4, 64, 4 * 1024 },
	{ CAI_DTLB2,    0x04,    4,  8, 4 * 1024 * 1024 },
	{ CAI_ITLB,     0x50, 0xff, 64, 4 * 1024, "4K/4M: 64 entries" },
	{ CAI_ITLB,     0x51, 0xff, 64, 4 * 1024, "4K/4M: 128 entries" },
	{ CAI_ITLB,     0x52, 0xff, 64, 4 * 1024, "4K/4M: 256 entries" },
	{ CAI_DTLB,     0x5b, 0xff, 64, 4 * 1024, "4K/4M: 64 entries" },
	{ CAI_DTLB,     0x5c, 0xff, 64, 4 * 1024, "4K/4M: 128 entries" },
	{ CAI_DTLB,     0x5d, 0xff, 64, 4 * 1024, "4K/4M: 256 entries" },

	{ CAI_ICACHE,   0x06,  4,        8 * 1024, 32 },
	{ CAI_ICACHE,   0x08,  4,       16 * 1024, 32 },
	{ CAI_DCACHE,   0x0a,  2,        8 * 1024, 32 },
	{ CAI_DCACHE,   0x0c,  4,       16 * 1024, 32 },
	{ CAI_L2CACHE,  0x40,  0,               0,  0, "not present" },
	{ CAI_L2CACHE,  0x41,  4,      128 * 1024, 32 },
	{ CAI_L2CACHE,  0x42,  4,      256 * 1024, 32 },
	{ CAI_L2CACHE,  0x43,  4,      512 * 1024, 32 },
	{ CAI_L2CACHE,  0x44,  4, 1 * 1024 * 1024, 32 },
	{ CAI_L2CACHE,  0x45,  4, 2 * 1024 * 1024, 32 },
	{ CAI_DCACHE,   0x66,  4,        8 * 1024, 64 },
	{ CAI_DCACHE,   0x67,  4,       16 * 1024, 64 },
	{ CAI_DCACHE,   0x68,  4,  	32 * 1024, 64 },
	{ CAI_ICACHE,   0x70,  8,       12 * 1024, 64, "12K uOp cache 8-way"},
	{ CAI_ICACHE,   0x71,  8,       16 * 1024, 64, "16K uOp cache 8-way"},
	{ CAI_ICACHE,   0x72,  8,       32 * 1024, 64, "32K uOp cache 8-way"},
	{ CAI_L2CACHE,  0x79,  8,      128 * 1024, 64 },
	{ CAI_L2CACHE,  0x7a,  8,      256 * 1024, 64 },
	{ CAI_L2CACHE,  0x7b,  8,      512 * 1024, 64 },
	{ CAI_L2CACHE,  0x7c,  8, 1 * 1024 * 1024, 64 },
	{ CAI_L2CACHE,  0x82,  8,      256 * 1024, 32 },
	{ CAI_L2CACHE,  0x84,  8, 1 * 1024 * 1024, 32 },
	{ CAI_L2CACHE,  0x85,  8, 2 * 1024 * 1024, 32 },
	{ 0,               0,  0,	        0,  0 },
};

static const struct i386_cache_info *
cache_info_lookup(const struct i386_cache_info *cai, u_int8_t desc);

/*
 * Map Brand ID from cpuid instruction to brand name.
 * Source: Intel Processor Identification and the CPUID Instruction, AP-485
 */
static const char * const i386_intel_brand[] = {
	"",		    /* Unsupported */
	"Celeron",	    /* Intel (R) Celeron (TM) processor */
	"Pentium III",      /* Intel (R) Pentium (R) III processor */
	"Pentium III Xeon", /* Intel (R) Pentium (R) III Xeon (TM) processor */
	"", "", "",	    /* Reserved */
	"Pentium 4"	    /* Intel (R) Pentium (R) 4 processor */
};

#ifdef COMPAT_NOMID
static int exec_nomid	__P((struct proc *, struct exec_package *));
#endif

void cyrix6x86_cpu_setup __P((struct cpu_info *));
void winchip_cpu_setup __P((struct cpu_info *));
void amd_family5_setup __P((struct cpu_info *));
void transmeta_cpu_setup __P((struct cpu_info *));

static void transmeta_cpu_info __P((struct cpu_info *));
static void amd_cpuid_cpu_cacheinfo __P((struct cpu_info *));

static __inline u_char
cyrix_read_reg(u_char reg)
{
	outb(0x22, reg);
	return inb(0x23);
}

static __inline void
cyrix_write_reg(u_char reg, u_char data)
{
	outb(0x22, reg);
	outb(0x23, data);
}

static char *
print_cache_config(struct cpu_info *ci, int cache_tag, char *name, char *sep)
{
	char cbuf[7];
	struct i386_cache_info *cai = &ci->ci_cinfo[cache_tag];

	if (cai->cai_totalsize == 0)
		return sep;

	if (sep == NULL)
		printf("%s: ", ci->ci_dev->dv_xname);
	else
		printf("%s", sep);
	if (name != NULL)
		printf("%s ", name);

	if (cai->cai_string != NULL) {
		printf("%s", cai->cai_string);
	} else {
		format_bytes(cbuf, sizeof(cbuf), cai->cai_totalsize);
		printf("%s %db/line ", cbuf, cai->cai_linesize);
	}
	switch (cai->cai_associativity) {
	case    0:
		printf("disabled");
		break;
	case    1:
		printf("direct-mapped");
		break;
	case 0xff:
		printf("fully associative");
		break;
	default:
		printf("%d-way", cai->cai_associativity);
		break;
	}
	return ", ";
}

static char *
print_tlb_config(struct cpu_info *ci, int cache_tag, char *name, char *sep)
{
	char cbuf[7];
	struct i386_cache_info *cai = &ci->ci_cinfo[cache_tag];

	if (cai->cai_totalsize == 0)
		return sep;

	if (sep == NULL)
		printf("%s: ", ci->ci_dev->dv_xname);
	else
		printf("%s", sep);
	if (name != NULL)
		printf("%s ", name);

	if (cai->cai_string != NULL) {
		printf("%s", cai->cai_string);
	} else {
		format_bytes(cbuf, sizeof(cbuf), cai->cai_linesize);
		printf("%d %s entries ", cai->cai_totalsize, cbuf);
		switch (cai->cai_associativity) {
		case 0:
			printf("disabled");
			break;
		case 1:
			printf("direct-mapped");
			break;
		case 0xff:
			printf("fully associative");
			break;
		default:
			printf("%d-way", cai->cai_associativity);
			break;
		}
	}
	return ", ";
}

/*
 * Machine-dependent startup code
 */
void
cpu_startup()
{
#if 0
	struct cpu_info *ci = curcpu();
#endif
	caddr_t v;
	int sz, x;
	vaddr_t minaddr, maxaddr;
	vsize_t size;
	char pbuf[9];
#if 0
	int bigcache, cachesize;
#endif

	/*
	 * Initialize error message buffer (et end of core).
	 */
	msgbuf_vaddr = uvm_km_valloc(kernel_map, i386_round_page(MSGBUFSIZE));
	if (msgbuf_vaddr == 0)
		panic("failed to valloc msgbuf_vaddr");

	/* msgbuf_paddr was init'd in pmap */
	for (x = 0; x < btoc(MSGBUFSIZE); x++)
		pmap_kenter_pa((vaddr_t)msgbuf_vaddr + x * PAGE_SIZE,
		    msgbuf_paddr + x * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE);
	pmap_update(pmap_kernel());

	initmsgbuf((caddr_t)msgbuf_vaddr, round_page(MSGBUFSIZE));

	printf("%s", version);

#if 0
#ifdef MTRR
	if (cpu_feature & CPUID_MTRR) {
		mtrr_funcs = &i686_mtrr_funcs;
		i686_mtrr_init_first();
		mtrr_init_cpu(ci);
	} else if (strcmp(cpu_vendor, "AuthenticAMD") == 0) {
		/*
		 * Must be a K6-2 Step >= 7 or a K6-III.
		 */
		if (CPUID2FAMILY(cpu_id) == 5) {
			if (CPUID2MODEL(cpu_id) > 8 ||
			    (CPUID2MODEL(cpu_id) == 8 &&
			     CPUID2STEPPING(cpu_id) >= 7)) {
				mtrr_funcs = &k6_mtrr_funcs;
				k6_mtrr_init_first();
				mtrr_init_cpu(ci);
			}
		}
	}
#endif
#endif
	format_bytes(pbuf, sizeof(pbuf), ptoa(physmem));
	printf("total memory = %s\n", pbuf);

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys(NULL, NULL);
	if ((v = (caddr_t)uvm_km_zalloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v, NULL) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Allocate virtual address space for the buffers.  The area
	 * is not managed by the VM system.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				UVM_ADV_NORMAL, 0)) != 0)
		panic("cpu_startup: cannot allocate VM for buffers");
	minaddr = (vaddr_t)buffers;
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}

	/*
	 * XXX We defer allocation of physical pages for buffers until
	 * XXX after autoconfiguration has run.  We must do this because
	 * XXX on system with large amounts of memory or with large
	 * XXX user-configured buffer caches, the buffer cache will eat
	 * XXX up all of the lower 16M of RAM.  This prevents ISA DMA
	 * XXX maps from allocating bounce pages.
	 *
	 * XXX Note that nothing can use buffer cache buffers until after
	 * XXX autoconfiguration completes!!
	 *
	 * XXX This is a hack, and needs to be replaced with a better
	 * XXX solution!  --thorpej@netbsd.org, December 6, 1997
	 */

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, 0, FALSE, NULL);

	/*
	 * Finally, allocate mbuf cluster submap.
	 */
	mb_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    nmbclusters * mclbytes, VM_MAP_INTRSAFE, FALSE, NULL);

	/*
	 * XXX Buffer cache pages haven't yet been allocated, so
	 * XXX we need to account for those pages when printing
	 * XXX the amount of free memory.
	 */
	format_bytes(pbuf, sizeof(pbuf), ptoa(uvmexp.free - bufpages));
	printf("avail memory = %s\n", pbuf);
	format_bytes(pbuf, sizeof(pbuf), bufpages * PAGE_SIZE);
	printf("using %d buffers containing %s of memory\n", nbuf, pbuf);

	/* Safe for i/o port / memory space allocation to use malloc now. */
	i386_bus_space_mallocok();
}

/*
 * Set up proc0's TSS and LDT.
 */
void
i386_proc0_tss_ldt_init()
{
	struct pcb *pcb;
	int x;

	curpcb = pcb = &proc0.p_addr->u_pcb;

	pcb->pcb_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	pcb->pcb_tss.tss_esp0 = (int)proc0.p_addr + USPACE - 16;

	pcb->pcb_flags = 0;
	pcb->pcb_tss.tss_ioopt =
	    ((caddr_t)pcb->pcb_iomap - (caddr_t)&pcb->pcb_tss) << 16;

	for (x = 0; x < sizeof(pcb->pcb_iomap) / 4; x++)
		pcb->pcb_iomap[x] = 0xffffffff;

	pcb->pcb_ldt_sel = pmap_kernel()->pm_ldt_sel = GSEL(GLDT_SEL, SEL_KPL);
	pcb->pcb_cr0 = rcr0();

	proc0.p_md.md_regs = (struct trapframe *)pcb->pcb_tss.tss_esp0 - 1;
	proc0.p_md.md_tss_sel = tss_alloc(pcb);

	ltr(proc0.p_md.md_tss_sel);
	lldt(pcb->pcb_ldt_sel);
}

/*
 * Set up TSS and LDT for a new PCB.
 */

void
i386_init_pcb_tss_ldt(ci)
	struct cpu_info *ci;
{
	int x;
	struct pcb *pcb = ci->ci_idle_pcb;

	pcb->pcb_flags = 0;
	pcb->pcb_tss.tss_ioopt =
	    ((caddr_t)pcb->pcb_iomap - (caddr_t)&pcb->pcb_tss) << 16;
	for (x = 0; x < sizeof(pcb->pcb_iomap) / 4; x++)
		pcb->pcb_iomap[x] = 0xffffffff;

	pcb->pcb_ldt_sel = pmap_kernel()->pm_ldt_sel = GSEL(GLDT_SEL, SEL_KPL);
	pcb->pcb_cr0 = rcr0();

	ci->ci_idle_tss_sel = tss_alloc(pcb);
}

/*
 * XXX Finish up the deferred buffer cache allocation and initialization.
 */
void
i386_bufinit()
{
	int i, base, residual;

	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vsize_t curbufsize;
		vaddr_t curbuf;
		struct vm_page *pg;

		/*
		 * Each buffer has MAXBSIZE bytes of VM space allocated.  Of
		 * that MAXBSIZE space, we allocate and map (base+1) pages
		 * for the first "residual" buffers, and then we allocate
		 * "base" pages for the rest.
		 */
		curbuf = (vaddr_t) buffers + (i * MAXBSIZE);
		curbufsize = PAGE_SIZE * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			/*
			 * Attempt to allocate buffers from the first
			 * 16M of RAM to avoid bouncing file system
			 * transfers.
			 */
			pg = uvm_pagealloc_strat(NULL, 0, NULL, 0,
			    UVM_PGA_STRAT_FALLBACK, VM_FREELIST_FIRST16);
			if (pg == NULL)
				panic("cpu_startup: not enough memory for "
				    "buffer cache");
			pmap_kenter_pa(curbuf, VM_PAGE_TO_PHYS(pg),
			    VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	pmap_update(pmap_kernel());

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
}

/*
 * Info for CTL_HW
 */
char	cpu_model[120];

/*
 * Note: these are just the ones that may not have a cpuid instruction.
 * We deal with the rest in a different way.
 */
const struct cpu_nocpuid_nameclass i386_nocpuid_cpus[] = {
	{ CPUVENDOR_INTEL, "Intel", "386SX",	CPUCLASS_386,
		NULL, NULL},			/* CPU_386SX */
	{ CPUVENDOR_INTEL, "Intel", "386DX",	CPUCLASS_386,
		NULL, NULL},			/* CPU_386   */
	{ CPUVENDOR_INTEL, "Intel", "486SX",	CPUCLASS_486,
		NULL, NULL},			/* CPU_486SX */
	{ CPUVENDOR_INTEL, "Intel", "486DX",	CPUCLASS_486,
		NULL, NULL},			/* CPU_486   */
	{ CPUVENDOR_CYRIX, "Cyrix", "486DLC",	CPUCLASS_486,
		NULL, NULL},			/* CPU_486DLC */
	{ CPUVENDOR_CYRIX, "Cyrix", "6x86",	CPUCLASS_486,
		cyrix6x86_cpu_setup, NULL},	/* CPU_6x86 */
	{ CPUVENDOR_NEXGEN,"NexGen","586",      CPUCLASS_386,
		NULL, NULL},			/* CPU_NX586 */
};

const char *classnames[] = {
	"386",
	"486",
	"586",
	"686"
};

const char *modifiers[] = {
	"",
	"OverDrive",
	"Dual",
	""
};

const struct cpu_cpuid_nameclass i386_cpuid_cpus[] = {
	{
		"GenuineIntel",
		CPUVENDOR_INTEL,
		"Intel",
		/* Family 4 */
		{ {
			CPUCLASS_486,
			{
				"486DX", "486DX", "486SX", "486DX2", "486SL",
				"486SX2", 0, "486DX2 W/B Enhanced",
				"486DX4", 0, 0, 0, 0, 0, 0, 0,
				"486"		/* Default */
			},
			NULL,
			NULL,
			NULL,
		},
		/* Family 5 */
		{
			CPUCLASS_586,
			{
				"Pentium (P5 A-step)", "Pentium (P5)",
				"Pentium (P54C)", "Pentium (P24T)",
				"Pentium/MMX", "Pentium", 0,
				"Pentium (P54C)", "Pentium/MMX (Tillamook)",
				0, 0, 0, 0, 0, 0, 0,
				"Pentium"	/* Default */
			},
			NULL,
			NULL,
			NULL,
		},
		/* Family 6 */
		{
			CPUCLASS_686,
			{
				"Pentium Pro (A-step)", "Pentium Pro", 0,
				"Pentium II (Klamath)", "Pentium Pro",
				"Pentium II/Celeron (Deschutes)",
				"Celeron (Mendocino)",
				"Pentium III (Katmai)",
				"Pentium III (Coppermine)",
				0, "Pentium III (Cascades)",
				"Pentium III (Tualatin)", 0, 0, 0, 0,
				"Pentium Pro, II or III"	/* Default */
			},
			NULL,
			NULL,
			NULL,
		},
		/* Family > 6 */
		{
			CPUCLASS_686,
			{
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				"Pentium 4"	/* Default */
			},
			NULL,
			NULL,
			NULL,
		} }
	},
	{
		"AuthenticAMD",
		CPUVENDOR_AMD,
		"AMD",
		/* Family 4 */
		{ {
			CPUCLASS_486,
			{
				0, 0, 0, "Am486DX2 W/T",
				0, 0, 0, "Am486DX2 W/B",
				"Am486DX4 W/T or Am5x86 W/T 150",
				"Am486DX4 W/B or Am5x86 W/B 150", 0, 0,
				0, 0, "Am5x86 W/T 133/160",
				"Am5x86 W/B 133/160",
				"Am486 or Am5x86"	/* Default */
			},
			NULL,
			NULL,
			NULL,
		},
		/* Family 5 */
		{
			CPUCLASS_586,
			{
				"K5", "K5", "K5", "K5", 0, 0, "K6",
				"K6", "K6-2", "K6-III", 0, 0, 0,
				"K6-2+/III+", 0, 0,
				"K5 or K6"		/* Default */
			},
			amd_family5_setup,
			amd_cpuid_cpu_cacheinfo,
			NULL,
		},
		/* Family 6 */
		{
			CPUCLASS_686,
			{
				0, "Athlon Model 1", "Athlon Model 2",
				"Duron", "Athlon Model 4 (Thunderbird)",
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				"K7 (Athlon)"	/* Default */
			},
			NULL,
			amd_cpuid_cpu_cacheinfo,
			NULL,
		},
		/* Family > 6 */
		{
			CPUCLASS_686,
			{
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				"Unknown K7 (Athlon)"	/* Default */
			},
			NULL,
			amd_cpuid_cpu_cacheinfo,
			NULL,
		} }
	},
	{
		"CyrixInstead",
		CPUVENDOR_CYRIX,
		"Cyrix",
		/* Family 4 */
		{ {
			CPUCLASS_486,
			{
				0, 0, 0,
				"MediaGX",
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				"486"		/* Default */
			},
			cyrix6x86_cpu_setup, /* XXX ?? */
			NULL,
			NULL,
		},
		/* Family 5 */
		{
			CPUCLASS_586,
			{
				0, 0, "6x86", 0,
				"MMX-enhanced MediaGX (GXm)", /* or Geode? */
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				"6x86"		/* Default */
			},
			cyrix6x86_cpu_setup,
			NULL,
			NULL,
		},
		/* Family 6 */
		{
			CPUCLASS_686,
			{
				"6x86MX", 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				"6x86MX"		/* Default */
			},
			cyrix6x86_cpu_setup,
			NULL,
			NULL,
		},
		/* Family > 6 */
		{
			CPUCLASS_686,
			{
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				"Unknown 6x86MX"		/* Default */
			},
			NULL,
			NULL,
			NULL,
		} }
	},
	{
		"CentaurHauls",
		CPUVENDOR_IDT,
		"IDT",
		/* Family 4, IDT never had any of these */
		{ {
			CPUCLASS_486,
			{
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				"486 compatible"	/* Default */
			},
			NULL,
			NULL,
			NULL,
		},
		/* Family 5 */
		{
			CPUCLASS_586,
			{
				0, 0, 0, 0, "WinChip C6", 0, 0, 0,
				"WinChip 2", "WinChip 3", 0, 0, 0, 0, 0, 0,
				"WinChip"		/* Default */
			},
			winchip_cpu_setup,
			NULL,
			NULL,
		},
		/* Family 6, not yet available from IDT */
		{
			CPUCLASS_686,
			{
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				"Pentium Pro compatible"	/* Default */
			},
			NULL,
			NULL,
			NULL,
		},
		/* Family > 6, not yet available from IDT */
		{
			CPUCLASS_686,
			{
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				"Pentium Pro compatible"	/* Default */
			},
			NULL,
			NULL,
			NULL,
		} }
	},
	{
		"GenuineTMx86",
		CPUVENDOR_TRANSMETA,
		"Transmeta",
		/* Family 4, Transmeta never had any of these */
		{ {
			CPUCLASS_486, 
			{
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				"486 compatible"	/* Default */
			},
			NULL,
			NULL,
			NULL,
		},
		/* Family 5 */
		{
			CPUCLASS_586,
			{
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				"Crusoe"		/* Default */
			},
			transmeta_cpu_setup,
			NULL,
			transmeta_cpu_info,
		},
		/* Family 6, not yet available from Transmeta */
		{
			CPUCLASS_686,
			{
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				"Pentium Pro compatible"	/* Default */
			},
			NULL,
			NULL,
			NULL,
		},
		/* Family > 6, not yet available from Transmeta */
		{
			CPUCLASS_686,
			{
				0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,
				"Pentium Pro compatible"	/* Default */
			},
			NULL,
			NULL,
			NULL,
		} }
	}
};

void
cyrix6x86_cpu_setup(ci)
	struct cpu_info *ci;
{
	/*
	 * i8254 latch check routine:
	 *     National Geode (formerly Cyrix MediaGX) has a serious bug in
	 *     its built-in i8254-compatible clock module.
	 *     Set the variable 'clock_broken_latch' to indicate it.
	 */

	extern int clock_broken_latch;

	switch (ci->ci_signature) {
	case 0x440:     /* Cyrix MediaGX */
	case 0x540:     /* GXm */
		clock_broken_latch = 1;
		break;
	}

	/* set up various cyrix registers */
	/* Enable suspend on halt */
	cyrix_write_reg(0xc2, cyrix_read_reg(0xc2) | 0x08);
	/* enable access to ccr4/ccr5 */
	cyrix_write_reg(0xC3, cyrix_read_reg(0xC3) | 0x10);
	/* cyrix's workaround  for the "coma bug" */
	cyrix_write_reg(0x31, cyrix_read_reg(0x31) | 0xf8);
	cyrix_write_reg(0x32, cyrix_read_reg(0x32) | 0x7f);
	cyrix_write_reg(0x33, cyrix_read_reg(0x33) & ~0xff);
	cyrix_write_reg(0x3c, cyrix_read_reg(0x3c) | 0x87);
	/* disable access to ccr4/ccr5 */
	cyrix_write_reg(0xC3, cyrix_read_reg(0xC3) & ~0x10);

	/*
	 * XXX disable page zero in the idle loop, it seems to
	 * cause panics on these CPUs.
	 */
	vm_page_zero_enable = FALSE;
}

void
winchip_cpu_setup(ci)
	struct cpu_info *ci;
{
#if defined(I586_CPU)
	switch (CPUID2MODEL(ci->ci_signature)) { /* model */
	case 4:	/* WinChip C6 */
		cpu_feature &= ~CPUID_TSC;
		printf("WARNING: WinChip C6: broken TSC disabled\n");
	}
#endif
}

#define CPUID(code, eax, ebx, ecx, edx) 			\
	asm ("cpuid"						\
	    : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)	\
	    : "a" (code));

static void
cpu_probe_base_features(struct cpu_info *ci)
{
	const struct i386_cache_info *cai;
	u_int descs[4];
	int iterations, i, j;
	u_int8_t desc;
	u_int32_t dummy1, dummy2, miscbytes;

	if (ci->ci_cpuid_level < 0)
		return;

	CPUID(0, ci->ci_cpuid_level,
	    ci->ci_vendor[0],
	    ci->ci_vendor[2],
	    ci->ci_vendor[1]);
	ci->ci_vendor[3] = 0;

	if (ci->ci_cpuid_level < 1)
		return;

	CPUID(1, ci->ci_signature, miscbytes, dummy1, ci->ci_feature_flags);

	/* Brand is low order 8 bits of ebx */
	ci->ci_brand_id = miscbytes & 0xff;

	/* CLFLUSH line size is next 8 bits */
	if (ci->ci_feature_flags & CPUID_CFLUSH)
		ci->ci_cflush_lsize = ((miscbytes >> 8) & 0xff) << 3;

	if (ci->ci_cpuid_level < 2)
		return;

	/*
	 * Parse the cache info from `cpuid', if we have it.
	 * XXX This is kinda ugly, but hey, so is the architecture...
	 */

	CPUID(2, descs[0], descs[1], descs[2], descs[3]);

	iterations = descs[0] & 0xff;
	while (iterations-- > 0) {
		for (i = 0; i < 4; i++) {
			if (descs[i] & 0x80000000)
				continue;
			for (j = 0; j < 4; j++) {
				if (i == 0 && j == 0)
					continue;
				desc = (descs[i] >> (j * 8)) & 0xff;
				if (desc == 0)
					continue;
				cai = cache_info_lookup(intel_cpuid_cache_info,
				    desc);
				if (cai != NULL)
					ci->ci_cinfo[cai->cai_index] = *cai;
			}
		}
		CPUID(2, descs[0], descs[1], descs[2], descs[3]);
	}

	if (ci->ci_cpuid_level < 3)
		return;

	/*
	 * If the processor serial number misfeature is present and supported,
	 * extract it here.
	 */
	if ((ci->ci_feature_flags & CPUID_PN) != 0)
	{
		ci->ci_cpu_serial[0] = ci->ci_signature;
		CPUID(3, dummy1, dummy2,
		    ci->ci_cpu_serial[2],
		    ci->ci_cpu_serial[1]);
	}
}

void
cpu_probe_features(struct cpu_info *ci)
{
	const struct cpu_cpuid_nameclass *cpup = NULL;
	int i, max, family;

	cpu_probe_base_features(ci);

	if (ci->ci_cpuid_level < 1)
		return;

	max = sizeof (i386_cpuid_cpus) / sizeof (i386_cpuid_cpus[0]);
	for (i = 0; i < max; i++) {
		if (!strncmp((char *)ci->ci_vendor,
		    i386_cpuid_cpus[i].cpu_id, 12)) {
			cpup = &i386_cpuid_cpus[i];
			break;
		}
	}

	if (cpup == NULL)
		return;

	family = (ci->ci_signature >> 8) & 0xf;

	if (family > CPU_MAXFAMILY) {
		family = CPU_MAXFAMILY;
	}
	i = family - CPU_MINFAMILY;

	if (cpup->cpu_family[i].cpu_probe == NULL)
		return;

	(*cpup->cpu_family[i].cpu_probe)(ci);
}


void
amd_family5_setup(struct cpu_info *ci)
{

	switch (CPUID2MODEL(ci->ci_signature)) {
	case 0:		/* AMD-K5 Model 0 */
		/*
		 * According to the AMD Processor Recognition App Note,
		 * the AMD-K5 Model 0 uses the wrong bit to indicate
		 * support for global PTEs, instead using bit 9 (APIC)
		 * rather than bit 13 (i.e. "0x200" vs. 0x2000".  Oops!).
		 */
		if (cpu_feature & CPUID_APIC)
			cpu_feature = (cpu_feature & ~CPUID_APIC) | CPUID_PGE;
		/*
		 * XXX But pmap_pg_g is already initialized -- need to kick
		 * XXX the pmap somehow.  How does the MP branch do this?
		 */
		break;
	}
}

/*
 * Transmeta Crusoe LongRun Support by Tamotsu Hattori.
 * Port from FreeBSD-current(August, 2001) to NetBSD by tshiozak.
 */

#define	MSR_TMx86_LONGRUN		0x80868010
#define	MSR_TMx86_LONGRUN_FLAGS		0x80868011

#define	LONGRUN_MODE_MASK(x)		((x) & 0x000000007f)
#define	LONGRUN_MODE_RESERVED(x)	((x) & 0xffffff80)
#define	LONGRUN_MODE_WRITE(x, y)	(LONGRUN_MODE_RESERVED(x) | \
					    LONGRUN_MODE_MASK(y))

#define	LONGRUN_MODE_MINFREQUENCY	0x00
#define	LONGRUN_MODE_ECONOMY		0x01
#define	LONGRUN_MODE_PERFORMANCE	0x02
#define	LONGRUN_MODE_MAXFREQUENCY	0x03
#define	LONGRUN_MODE_UNKNOWN		0x04
#define	LONGRUN_MODE_MAX		0x04

union msrinfo {
	u_int64_t	msr;
	u_int32_t	regs[2];
};

u_int32_t longrun_modes[LONGRUN_MODE_MAX][3] = {
	/*  MSR low, MSR high, flags bit0 */
	{	  0,	  0,		0},	/* LONGRUN_MODE_MINFREQUENCY */
	{	  0,	100,		0},	/* LONGRUN_MODE_ECONOMY */
	{	  0,	100,		1},	/* LONGRUN_MODE_PERFORMANCE */
	{	100,	100,		1},	/* LONGRUN_MODE_MAXFREQUENCY */
};

static u_int 
tmx86_get_longrun_mode(void)
{
	u_long		eflags;
	union msrinfo	msrinfo;
	u_int		low, high, flags, mode;

	eflags = read_eflags();
	disable_intr();

	msrinfo.msr = rdmsr(MSR_TMx86_LONGRUN);
	low = LONGRUN_MODE_MASK(msrinfo.regs[0]);
	high = LONGRUN_MODE_MASK(msrinfo.regs[1]);
	flags = rdmsr(MSR_TMx86_LONGRUN_FLAGS) & 0x01;

	for (mode = 0; mode < LONGRUN_MODE_MAX; mode++) {
		if (low   == longrun_modes[mode][0] &&
		    high  == longrun_modes[mode][1] &&
		    flags == longrun_modes[mode][2]) {
			goto out;
		}
	}
	mode = LONGRUN_MODE_UNKNOWN;
out:
	write_eflags(eflags);
	return (mode);
}

static u_int 
tmx86_get_longrun_status(u_int *frequency, u_int *voltage, u_int *percentage)
{
	u_long		eflags;
	u_int		eax, ebx, ecx, edx;

	eflags = read_eflags();
	disable_intr();

	CPUID(0x80860007, eax, ebx, ecx, edx);
	*frequency = eax;
	*voltage = ebx;
	*percentage = ecx;

	write_eflags(eflags);
	return (1);
}

static u_int 
tmx86_set_longrun_mode(u_int mode)
{
	u_long		eflags;
	union msrinfo	msrinfo;

	if (mode >= LONGRUN_MODE_UNKNOWN) {
		return (0);
	}

	eflags = read_eflags();
	disable_intr();

	/* Write LongRun mode values to Model Specific Register. */
	msrinfo.msr = rdmsr(MSR_TMx86_LONGRUN);
	msrinfo.regs[0] = LONGRUN_MODE_WRITE(msrinfo.regs[0],
	    longrun_modes[mode][0]);
	msrinfo.regs[1] = LONGRUN_MODE_WRITE(msrinfo.regs[1],
	    longrun_modes[mode][1]);
	wrmsr(MSR_TMx86_LONGRUN, msrinfo.msr);

	/* Write LongRun mode flags to Model Specific Register. */
	msrinfo.msr = rdmsr(MSR_TMx86_LONGRUN_FLAGS);
	msrinfo.regs[0] = (msrinfo.regs[0] & ~0x01) | longrun_modes[mode][2];
	wrmsr(MSR_TMx86_LONGRUN_FLAGS, msrinfo.msr);

	write_eflags(eflags);
	return (1);
}

static u_int			 crusoe_longrun;
static u_int			 crusoe_frequency;
static u_int	 		 crusoe_voltage;
static u_int	 		 crusoe_percentage;

static void
tmx86_get_longrun_status_all(void)
{

	tmx86_get_longrun_status(&crusoe_frequency,
	    &crusoe_voltage, &crusoe_percentage);
}


static void
transmeta_cpu_info(struct cpu_info *ci)
{
	u_int eax, ebx, ecx, edx, nreg = 0;

	CPUID(0x80860000, eax, ebx, ecx, edx);
	nreg = eax;
	if (nreg >= 0x80860001) {
		CPUID(0x80860001, eax, ebx, ecx, edx);		
		printf("%s: Processor revision %u.%u.%u.%u\n",
		    ci->ci_dev->dv_xname,
		    (ebx >> 24) & 0xff,
		    (ebx >> 16) & 0xff,
		    (ebx >> 8) & 0xff,
		    ebx & 0xff);
	}
	if (nreg >= 0x80860002) {
		CPUID(0x80860002, eax, ebx, ecx, edx);				
		printf("%s: Code Morphing Software Rev: %u.%u.%u-%u-%u\n",
		    ci->ci_dev->dv_xname, (ebx >> 24) & 0xff,
		    (ebx >> 16) & 0xff,
		    (ebx >> 8) & 0xff,
		    ebx & 0xff,
		    ecx);
	}
	if (nreg >= 0x80860006) {
		union {
			char text[65];
			struct 
			{
				u_int eax;
				u_int ebx;
				u_int ecx;
				u_int edx;
			} regs[4];
		} info;
		int i;

		for (i=0; i<4; i++) {
			CPUID(0x80860003 + i,
			    info.regs[i].eax, info.regs[i].ebx,
			    info.regs[i].ecx, info.regs[i].edx);
		}
		info.text[64] = 0;
		printf("%s: %s\n", ci->ci_dev->dv_xname, info.text);
	}

	crusoe_longrun = tmx86_get_longrun_mode();
	tmx86_get_longrun_status(&crusoe_frequency,
	    &crusoe_voltage, &crusoe_percentage);
	printf("%s: LongRun mode: %d  <%dMHz %dmV %d%%>\n", ci->ci_dev->dv_xname,
	    crusoe_longrun, crusoe_frequency, crusoe_voltage,
	    crusoe_percentage);
}

void
transmeta_cpu_setup(struct cpu_info *ci)
{

	tmx86_has_longrun = 1;
}


/* ---------------------------------------------------------------------- */

static const struct i386_cache_info *
cache_info_lookup(const struct i386_cache_info *cai, u_int8_t desc)
{
	int i;

	for (i = 0; cai[i].cai_desc != 0; i++) {
		if (cai[i].cai_desc == desc)
			return (&cai[i]);
	}

	return (NULL);
}

/*
 * AMD Cache Info:
 *
 *	Athlon, Duron:
 *
 *		Function 8000.0005 L1 TLB/Cache Information
 *		EAX -- L1 TLB 2/4MB pages
 *		EBX -- L1 TLB 4K pages
 *		ECX -- L1 D-cache
 *		EDX -- L1 I-cache
 *
 *		Function 8000.0006 L2 TLB/Cache Information
 *		EAX -- L2 TLB 2/4MB pages
 *		EBX -- L2 TLB 4K pages
 *		ECX -- L2 Unified cache
 *		EDX -- reserved
 *
 *	K5, K6:
 *
 *		Function 8000.0005 L1 TLB/Cache Information
 *		EAX -- reserved
 *		EBX -- TLB 4K pages
 *		ECX -- L1 D-cache
 *		EDX -- L1 I-cache
 *
 *	K6-III:
 *
 *		Function 8000.0006 L2 Cache Information
 *		EAX -- reserved
 *		EBX -- reserved
 *		ECX -- L2 Unified cache
 *		EDX -- reserved
 */

/* L1 TLB 2/4MB pages */
#define	AMD_L1_EAX_DTLB_ASSOC(x)	(((x) >> 24) & 0xff)
#define	AMD_L1_EAX_DTLB_ENTRIES(x)	(((x) >> 16) & 0xff)
#define	AMD_L1_EAX_ITLB_ASSOC(x)	(((x) >> 8)  & 0xff)
#define	AMD_L1_EAX_ITLB_ENTRIES(x)	( (x)        & 0xff)

/* L1 TLB 4K pages */
#define	AMD_L1_EBX_DTLB_ASSOC(x)	(((x) >> 24) & 0xff)
#define	AMD_L1_EBX_DTLB_ENTRIES(x)	(((x) >> 16) & 0xff)
#define	AMD_L1_EBX_ITLB_ASSOC(x)	(((x) >> 8)  & 0xff)
#define	AMD_L1_EBX_ITLB_ENTRIES(x)	( (x)        & 0xff)

/* L1 Data Cache */
#define	AMD_L1_ECX_DC_SIZE(x)		((((x) >> 24) & 0xff) * 1024)
#define	AMD_L1_ECX_DC_ASSOC(x)		 (((x) >> 16) & 0xff)
#define	AMD_L1_ECX_DC_LPT(x)		 (((x) >> 8)  & 0xff)
#define	AMD_L1_ECX_DC_LS(x)		 ( (x)        & 0xff)

/* L1 Instruction Cache */
#define	AMD_L1_EDX_IC_SIZE(x)		((((x) >> 24) & 0xff) * 1024)
#define	AMD_L1_EDX_IC_ASSOC(x)		 (((x) >> 16) & 0xff)
#define	AMD_L1_EDX_IC_LPT(x)		 (((x) >> 8)  & 0xff)
#define	AMD_L1_EDX_IC_LS(x)		 ( (x)        & 0xff)

/* Note for L2 TLB -- if the upper 16 bits are 0, it is a unified TLB */

/* L2 TLB 2/4MB pages */
#define	AMD_L2_EAX_DTLB_ASSOC(x)	(((x) >> 28)  & 0xf)
#define	AMD_L2_EAX_DTLB_ENTRIES(x)	(((x) >> 16)  & 0xfff)
#define	AMD_L2_EAX_IUTLB_ASSOC(x)	(((x) >> 12)  & 0xf)
#define	AMD_L2_EAX_IUTLB_ENTRIES(x)	( (x)         & 0xfff)

/* L2 TLB 4K pages */
#define	AMD_L2_EBX_DTLB_ASSOC(x)	(((x) >> 28)  & 0xf)
#define	AMD_L2_EBX_DTLB_ENTRIES(x)	(((x) >> 16)  & 0xfff)
#define	AMD_L2_EBX_IUTLB_ASSOC(x)	(((x) >> 12)  & 0xf)
#define	AMD_L2_EBX_IUTLB_ENTRIES(x)	( (x)         & 0xfff)

/* L2 Cache */
#define	AMD_L2_ECX_C_SIZE(x)		((((x) >> 16) & 0xffff) * 1024)
#define	AMD_L2_ECX_C_ASSOC(x)		 (((x) >> 12) & 0xf)
#define	AMD_L2_ECX_C_LPT(x)		 (((x) >> 8)  & 0xf)
#define	AMD_L2_ECX_C_LS(x)		 ( (x)        & 0xff)

static const struct i386_cache_info amd_cpuid_l2cache_assoc_info[] = {
	{ 0, 0x01,    1 },
	{ 0, 0x02,    2 },
	{ 0, 0x04,    4 },
	{ 0, 0x06,    8 },
	{ 0, 0x08,   16 },
	{ 0, 0x0f, 0xff },
	{ 0, 0x00,    0 },
};

void
amd_cpuid_cpu_cacheinfo(struct cpu_info *ci)
{
	const struct i386_cache_info *cp;
	struct i386_cache_info *cai;
	int family, model;
	u_int descs[4];
	u_int lfunc;

	family = (ci->ci_signature >> 8) & 15;
	if (family < CPU_MINFAMILY)
		panic("amd_cpuid_cpu_cacheinfo: strange family value");
	model = CPUID2MODEL(ci->ci_signature);

	/*
	 * K5 model 0 has none of this info.
	 */
	if (family == 5 && model == 0)
		return;

	/*
	 * Determine the largest extended function value.
	 */
	CPUID(0x80000000, descs[0], descs[1], descs[2], descs[3]);
	lfunc = descs[0];

	/*
	 * Determine L1 cache/TLB info.
	 */
	if (lfunc < 0x80000005) {
		/* No L1 cache info available. */
		return;
	}

	CPUID(0x80000005, descs[0], descs[1], descs[2], descs[3]);

	/*
	 * K6-III and higher have large page TLBs.
	 */
	if ((family == 5 && model >= 9) || family >= 6) {
		cai = &ci->ci_cinfo[CAI_ITLB2];
		cai->cai_totalsize = AMD_L1_EAX_ITLB_ENTRIES(descs[0]);
		cai->cai_associativity = AMD_L1_EAX_ITLB_ASSOC(descs[0]);
		cai->cai_linesize = (4 * 1024 * 1024);

		cai = &ci->ci_cinfo[CAI_DTLB2];
		cai->cai_totalsize = AMD_L1_EAX_DTLB_ENTRIES(descs[0]);
		cai->cai_associativity = AMD_L1_EAX_DTLB_ASSOC(descs[0]);
		cai->cai_linesize = (4 * 1024 * 1024);
	}

	cai = &ci->ci_cinfo[CAI_ITLB];
	cai->cai_totalsize = AMD_L1_EBX_ITLB_ENTRIES(descs[1]);
	cai->cai_associativity = AMD_L1_EBX_ITLB_ASSOC(descs[1]);
	cai->cai_linesize = (4 * 1024);

	cai = &ci->ci_cinfo[CAI_DTLB];
	cai->cai_totalsize = AMD_L1_EBX_DTLB_ENTRIES(descs[1]);
	cai->cai_associativity = AMD_L1_EBX_DTLB_ASSOC(descs[1]);
	cai->cai_linesize = (4 * 1024);

	cai = &ci->ci_cinfo[CAI_DCACHE];
	cai->cai_totalsize = AMD_L1_ECX_DC_SIZE(descs[2]);
	cai->cai_associativity = AMD_L1_ECX_DC_ASSOC(descs[2]);
	cai->cai_linesize = AMD_L1_EDX_IC_LS(descs[2]);

	cai = &ci->ci_cinfo[CAI_ICACHE];
	cai->cai_totalsize = AMD_L1_EDX_IC_SIZE(descs[3]);
	cai->cai_associativity = AMD_L1_EDX_IC_ASSOC(descs[3]);
	cai->cai_linesize = AMD_L1_EDX_IC_LS(descs[3]);

	/*
	 * Determine L2 cache/TLB info.
	 */
	if (lfunc < 0x80000006) {
		/* No L2 cache info available. */
		return;
	}

	CPUID(0x80000006, descs[0], descs[1], descs[2], descs[3]);

	cai = &ci->ci_cinfo[CAI_L2CACHE];
	cai->cai_totalsize = AMD_L2_ECX_C_SIZE(descs[2]);
	cai->cai_associativity = AMD_L2_ECX_C_ASSOC(descs[2]);
	cai->cai_linesize = AMD_L2_ECX_C_LS(descs[2]);

	cp = cache_info_lookup(amd_cpuid_l2cache_assoc_info,
	    cai->cai_associativity);
	if (cp != NULL)
		cai->cai_associativity = cp->cai_associativity;
	else
		cai->cai_associativity = 0;	/* XXX Unknown/reserved */
}

/*
 * Print identification for the given CPU.
 */

void
identifycpu(struct cpu_info *ci)
{
	const char *name, *modifier, *vendorname, *brand = "";
	int class = CPUCLASS_386, vendor, i, max;
	int family, model, step, modif;
	const struct cpu_cpuid_nameclass *cpup = NULL;
	const struct cpu_cpuid_family *cpufam;
	char *cpuname = ci->ci_dev->dv_xname;
	char buf[1024];
	char *sep;

	if (ci->ci_cpuid_level == -1) {
#ifdef DIAGNOSTIC
		if (cpu < 0 || cpu >=
		    sizeof(i386_nocpuid_cpus) / sizeof(i386_nocpuid_cpus[0]))
			panic("unknown cpu type %d\n", cpu);
#endif
		name = i386_nocpuid_cpus[cpu].cpu_name;
		vendor = i386_nocpuid_cpus[cpu].cpu_vendor;
		vendorname = i386_nocpuid_cpus[cpu].cpu_vendorname;
		class = i386_nocpuid_cpus[cpu].cpu_class;
		ci->cpu_setup = i386_nocpuid_cpus[cpu].cpu_setup;
		ci->ci_info = i386_nocpuid_cpus[cpu].cpu_info;
		modifier = "";
	} else {
		max = sizeof (i386_cpuid_cpus) / sizeof (i386_cpuid_cpus[0]);
		modif = (ci->ci_signature >> 12) & 0x3;
		family = (ci->ci_signature >> 8) & 0xf;
		if (family < CPU_MINFAMILY)
			panic("identifycpu: strange family value");
		model = CPUID2MODEL(ci->ci_signature);
		step = ci->ci_signature & 0xf;
#ifdef CPUDEBUG
		printf("%s: family %x model %x step %x\n", cpuname, family,
			model, step);
#endif

		for (i = 0; i < max; i++) {
			if (!strncmp((char *)ci->ci_vendor,
			    i386_cpuid_cpus[i].cpu_id, 12)) {
				cpup = &i386_cpuid_cpus[i];
				break;
			}
		}

		if (cpup == NULL) {
			vendor = CPUVENDOR_UNKNOWN;
			if (ci->ci_vendor[0] != '\0')
				vendorname = (char *)&ci->ci_vendor[0];
			else
				vendorname = "Unknown";
			if (family > CPU_MAXFAMILY)
				family = CPU_MAXFAMILY;
			class = family - 3;
			modifier = "";
			name = "";
			ci->cpu_setup = NULL;
			ci->ci_info = NULL;
		} else {
			vendor = cpup->cpu_vendor;
			vendorname = cpup->cpu_vendorname;
			modifier = modifiers[modif];
			if (family > CPU_MAXFAMILY) {
				family = CPU_MAXFAMILY;
				model = CPU_DEFMODEL;
			} else if (model > CPU_MAXMODEL)
				model = CPU_DEFMODEL;
			cpufam = &cpup->cpu_family[family - CPU_MINFAMILY];
			name = cpufam->cpu_models[model];
			if (name == NULL)
			    name = cpufam->cpu_models[CPU_DEFMODEL];
			class = cpufam->cpu_class;
			ci->cpu_setup = cpufam->cpu_setup;
			ci->ci_info = cpufam->cpu_info;

			/*
			 * Intel processors family >= 6, model 8 allow to
			 * recognize brand by Brand ID value.
			 */
			if (vendor == CPUVENDOR_INTEL && family >= 6 &&
			    model >= 8 && ci->ci_brand_id &&
			    ci->ci_brand_id <= 8)
				brand = i386_intel_brand[ci->ci_brand_id];
		}
	}

	cpu_class = class;
	ci->ci_cpu_class = class;

#if defined(I586_CPU) || defined(I686_CPU)
	/*
	 * If we have a cycle counter, compute the approximate
	 * CPU speed in MHz.
	 * XXX this needs to run on the CPU being probed..
	 */
	if (ci->ci_feature_flags & CPUID_TSC) {
		u_int64_t last_tsc;

		last_tsc = rdtsc();
		delay(100000);
		ci->ci_tsc_freq = (rdtsc() - last_tsc) * 10;
		microtime_func = tsc_microtime;
	}
	/* XXX end XXX */
#endif

	snprintf(cpu_model, sizeof(cpu_model), "%s%s%s%s%s%s%s (%s-class)",
	    vendorname,
	    *modifier ? " " : "", modifier,
	    *name ? " " : "", name,
	    *brand ? " " : "", brand,
	    classnames[class]);
	printf("%s: %s", cpuname, cpu_model);

	if (ci->ci_tsc_freq != 0)
		printf(", %qd.%02qd MHz", (ci->ci_tsc_freq + 4999) / 1000000,
		    ((ci->ci_tsc_freq + 4999) / 10000) % 100);
	printf("\n");

	if (ci->ci_info)
		(*ci->ci_info)(ci);
	
	if (ci->ci_feature_flags) {
		if ((ci->ci_feature_flags & CPUID_MASK1) != 0) {
			bitmask_snprintf(ci->ci_feature_flags, CPUID_FLAGS1,
			    buf, sizeof(buf));
			printf("%s: features %s\n", cpuname, buf);
		}
		if ((ci->ci_feature_flags & CPUID_MASK2) != 0) {
			bitmask_snprintf(ci->ci_feature_flags, CPUID_FLAGS2,
			    buf, sizeof(buf));
			printf("%s: features %s\n", cpuname, buf);
		}
	}


	if (ci->ci_cinfo[CAI_ICACHE].cai_totalsize != 0 ||
	    ci->ci_cinfo[CAI_DCACHE].cai_totalsize != 0) {
		sep = print_cache_config(ci, CAI_ICACHE, "I-cache", NULL);
		sep = print_cache_config(ci, CAI_DCACHE, "D-cache", sep);
		if (sep != NULL)
			printf("\n");
	}
	if (ci->ci_cinfo[CAI_L2CACHE].cai_totalsize != 0) {
		sep = print_cache_config(ci, CAI_L2CACHE, "L2 cache", NULL);
		if (sep != NULL)
			printf("\n");
	}
	if (ci->ci_cinfo[CAI_ITLB].cai_totalsize != 0) {
		sep = print_tlb_config(ci, CAI_ITLB, "ITLB", NULL);
		sep = print_tlb_config(ci, CAI_ITLB2, NULL, sep);
		if (sep != NULL)
			printf("\n");
	}
	if (ci->ci_cinfo[CAI_DTLB].cai_totalsize != 0) {
		sep = print_tlb_config(ci, CAI_DTLB, "DTLB", NULL);
		sep = print_tlb_config(ci, CAI_DTLB2, NULL, sep);
		if (sep != NULL)
			printf("\n");
	}

	if (ci->ci_cpuid_level >= 3 && (ci->ci_feature_flags & CPUID_PN)) {
		printf("%s: serial number %04X-%04X-%04X-%04X-%04X-%04X\n",
		    cpuname,
		    ci->ci_cpu_serial[0] / 65536, ci->ci_cpu_serial[0] % 65536,
		    ci->ci_cpu_serial[1] / 65536, ci->ci_cpu_serial[1] % 65536,
		    ci->ci_cpu_serial[2] / 65536, ci->ci_cpu_serial[2] % 65536);
	}

	/*
	 * Now that we have told the user what they have,
	 * let them know if that machine type isn't configured.
	 */
	switch (cpu_class) {
#if !defined(I386_CPU) && !defined(I486_CPU) && !defined(I586_CPU) && !defined(I686_CPU)
#error No CPU classes configured.
#endif
#ifndef I686_CPU
	case CPUCLASS_686:
		printf("NOTICE: this kernel does not support Pentium Pro CPU class\n");
#ifdef I586_CPU
		printf("NOTICE: lowering CPU class to i586\n");
		cpu_class = CPUCLASS_586;
		break;
#endif
#endif
#ifndef I586_CPU
	case CPUCLASS_586:
		printf("NOTICE: this kernel does not support Pentium CPU class\n");
#ifdef I486_CPU
		printf("NOTICE: lowering CPU class to i486\n");
		cpu_class = CPUCLASS_486;
		break;
#endif
#endif
#ifndef I486_CPU
	case CPUCLASS_486:
		printf("NOTICE: this kernel does not support i486 CPU class\n");
#ifdef I386_CPU
		printf("NOTICE: lowering CPU class to i386\n");
		cpu_class = CPUCLASS_386;
		break;
#endif
#endif
#ifndef I386_CPU
	case CPUCLASS_386:
		printf("NOTICE: this kernel does not support i386 CPU class\n");
		panic("no appropriate CPU class available");
#endif
	default:
		break;
	}

	/*
	 * Now plug in optimized versions of various routines we
	 * might have.
	 */
	switch (cpu_class) {
#if defined(I686_CPU)
	case CPUCLASS_686:
		copyout_func = i486_copyout;
		break;
#endif
#if defined(I586_CPU)
	case CPUCLASS_586:
		copyout_func = i486_copyout;
		break;
#endif
#if defined(I486_CPU)
	case CPUCLASS_486:
		copyout_func = i486_copyout;
		break;
#endif
	default:
		/* We just inherit the default i386 versions. */
		break;
	}

	if (cpu == CPU_486DLC) {
#ifndef CYRIX_CACHE_WORKS
		printf("WARNING: CYRIX 486DLC CACHE UNCHANGED.\n");
#else
#ifndef CYRIX_CACHE_REALLY_WORKS
		printf("WARNING: CYRIX 486DLC CACHE ENABLED IN HOLD-FLUSH MODE.\n");
#else
		printf("WARNING: CYRIX 486DLC CACHE ENABLED.\n");
#endif
#endif
	}

#if defined(I686_CPU)
	/*
	 * If we have FXSAVE/FXRESTOR, use them.
	 */
	if (cpu_feature & CPUID_FXSR) {
		i386_use_fxsave = 1;

		/*
		 * If we have SSE/SSE2, enable XMM exceptions, and
		 * notify userland.
		 */
		if (cpu_feature & (CPUID_SSE|CPUID_SSE2)) {
			if (cpu_feature & CPUID_SSE)
				i386_has_sse = 1;
			if (cpu_feature & CPUID_SSE2)
				i386_has_sse2 = 1;
		}
	} else
		i386_use_fxsave = 0;
#endif /* I686_CPU */
}

/*
 * machine dependent system variables.
 */
int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	dev_t consdev;
	struct btinfo_bootpath *bibp;
	int error, mode;

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case CPU_CONSDEV:
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
		    sizeof consdev));

	case CPU_BIOSBASEMEM:
		return (sysctl_rdint(oldp, oldlenp, newp, biosbasemem));

	case CPU_BIOSEXTMEM:
		return (sysctl_rdint(oldp, oldlenp, newp, biosextmem));

	case CPU_NKPDE:
		return (sysctl_rdint(oldp, oldlenp, newp, nkpde));

	case CPU_FPU_PRESENT:
		return (sysctl_rdint(oldp, oldlenp, newp, i386_fpu_present));

	case CPU_BOOTED_KERNEL:
	        bibp = lookup_bootinfo(BTINFO_BOOTPATH);
	        if(!bibp)
			return(ENOENT); /* ??? */
		return (sysctl_rdstring(oldp, oldlenp, newp, bibp->bootpath));
	case CPU_DISKINFO:
		if (i386_alldisks == NULL)
			return (ENOENT);
		return (sysctl_rdstruct(oldp, oldlenp, newp, i386_alldisks,
		    sizeof (struct disklist) +
			(i386_ndisks - 1) * sizeof (struct nativedisk_info)));
	case CPU_OSFXSR:
		return (sysctl_rdint(oldp, oldlenp, newp, i386_use_fxsave));
	case CPU_SSE:
		return (sysctl_rdint(oldp, oldlenp, newp, i386_has_sse));
	case CPU_SSE2:
		return (sysctl_rdint(oldp, oldlenp, newp, i386_has_sse2));
	case CPU_TMLR_MODE:
		if (!tmx86_has_longrun)
			return (EOPNOTSUPP);
		mode = (int)(crusoe_longrun = tmx86_get_longrun_mode());
		error = sysctl_int(oldp, oldlenp, newp, newlen, &mode);
		if (!error && (u_int)mode != crusoe_longrun) {
			if (tmx86_set_longrun_mode(mode)) {
				crusoe_longrun = (u_int)mode;
			} else {
				error = EINVAL;
			}
		}
		return (error);
	case CPU_TMLR_FREQUENCY:
		if (!tmx86_has_longrun)
			return (EOPNOTSUPP);
		tmx86_get_longrun_status_all();
		return (sysctl_rdint(oldp, oldlenp, newp, crusoe_frequency));
	case CPU_TMLR_VOLTAGE:
		if (!tmx86_has_longrun)
			return (EOPNOTSUPP);
		tmx86_get_longrun_status_all();
		return (sysctl_rdint(oldp, oldlenp, newp, crusoe_voltage));
	case CPU_TMLR_PERCENTAGE:
		if (!tmx86_has_longrun)
			return (EOPNOTSUPP);
		tmx86_get_longrun_status_all();
		return (sysctl_rdint(oldp, oldlenp, newp, crusoe_percentage));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig;
	sigset_t *mask;
	u_long code;
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	int onstack;

	tf = p->p_md.md_regs;

	/* Do we need to jump onto the signal stack? */
	onstack =
	    (p->p_sigctx.ps_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0 &&
	    (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;

	/* Allocate space for the signal handler context. */
	if (onstack)
		fp = (struct sigframe *)((caddr_t)p->p_sigctx.ps_sigstk.ss_sp +
					  p->p_sigctx.ps_sigstk.ss_size);
	else
		fp = (struct sigframe *)tf->tf_esp;
	fp--;

	/* Build stack frame for signal trampoline. */
	frame.sf_signum = sig;
	frame.sf_code = code;
	frame.sf_scp = &fp->sf_sc;
	frame.sf_handler = catcher;

	/* Save register context. */
#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		frame.sf_sc.sc_gs = tf->tf_vm86_gs;
		frame.sf_sc.sc_fs = tf->tf_vm86_fs;
		frame.sf_sc.sc_es = tf->tf_vm86_es;
		frame.sf_sc.sc_ds = tf->tf_vm86_ds;
		frame.sf_sc.sc_eflags = get_vflags(p);
		(*p->p_emul->e_syscall_intern)(p);
	} else
#endif
	{
		frame.sf_sc.sc_gs = tf->tf_gs;
		frame.sf_sc.sc_fs = tf->tf_fs;
		frame.sf_sc.sc_es = tf->tf_es;
		frame.sf_sc.sc_ds = tf->tf_ds;
		frame.sf_sc.sc_eflags = tf->tf_eflags;
	}
	frame.sf_sc.sc_edi = tf->tf_edi;
	frame.sf_sc.sc_esi = tf->tf_esi;
	frame.sf_sc.sc_ebp = tf->tf_ebp;
	frame.sf_sc.sc_ebx = tf->tf_ebx;
	frame.sf_sc.sc_edx = tf->tf_edx;
	frame.sf_sc.sc_ecx = tf->tf_ecx;
	frame.sf_sc.sc_eax = tf->tf_eax;
	frame.sf_sc.sc_eip = tf->tf_eip;
	frame.sf_sc.sc_cs = tf->tf_cs;
	frame.sf_sc.sc_esp = tf->tf_esp;
	frame.sf_sc.sc_ss = tf->tf_ss;
	frame.sf_sc.sc_trapno = tf->tf_trapno;
	frame.sf_sc.sc_err = tf->tf_err;

	/* Save signal stack. */
	frame.sf_sc.sc_onstack = p->p_sigctx.ps_sigstk.ss_flags & SS_ONSTACK;

	/* Save signal mask. */
	frame.sf_sc.sc_mask = *mask;

#ifdef COMPAT_13
	/*
	 * XXX We always have to save an old style signal mask because
	 * XXX we might be delivering a signal to a process which will
	 * XXX escape from the signal in a non-standard way and invoke
	 * XXX sigreturn() directly.
	 */
	native_sigset_to_sigset13(mask, &frame.sf_sc.__sc_mask13);
#endif

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Build context to run handler in.
	 */
	tf->tf_gs = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_fs = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_eip = (int)p->p_sigctx.ps_sigcode;
	tf->tf_cs = GSEL(GUCODE_SEL, SEL_UPL);
	tf->tf_eflags &= ~(PSL_T|PSL_VM|PSL_AC);
	tf->tf_esp = (int)fp;
	tf->tf_ss = GSEL(GUDATA_SEL, SEL_UPL);

	/* Remember that we're now on the signal stack. */
	if (onstack)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper privileges or to cause
 * a machine fault.
 */
int
sys___sigreturn14(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys___sigreturn14_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext *scp, context;
	struct trapframe *tf;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = SCARG(uap, sigcntxp);
	if (copyin((caddr_t)scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

	/* Restore register context. */
	tf = p->p_md.md_regs;
#ifdef VM86
	if (context.sc_eflags & PSL_VM) {
		void syscall_vm86 __P((struct trapframe));

		tf->tf_vm86_gs = context.sc_gs;
		tf->tf_vm86_fs = context.sc_fs;
		tf->tf_vm86_es = context.sc_es;
		tf->tf_vm86_ds = context.sc_ds;
		set_vflags(p, context.sc_eflags);
		p->p_md.md_syscall = syscall_vm86;
	} else
#endif
	{
		/*
		 * Check for security violations.  If we're returning to
		 * protected mode, the CPU will validate the segment registers
		 * automatically and generate a trap on violations.  We handle
		 * the trap, rather than doing all of the checking here.
		 */
		if (((context.sc_eflags ^ tf->tf_eflags) & PSL_USERSTATIC) != 0 ||
		    !USERMODE(context.sc_cs, context.sc_eflags))
			return (EINVAL);

		tf->tf_gs = context.sc_gs;
		tf->tf_fs = context.sc_fs;
		tf->tf_es = context.sc_es;
		tf->tf_ds = context.sc_ds;
		tf->tf_eflags = context.sc_eflags;
	}
	tf->tf_edi = context.sc_edi;
	tf->tf_esi = context.sc_esi;
	tf->tf_ebp = context.sc_ebp;
	tf->tf_ebx = context.sc_ebx;
	tf->tf_edx = context.sc_edx;
	tf->tf_ecx = context.sc_ecx;
	tf->tf_eax = context.sc_eax;
	tf->tf_eip = context.sc_eip;
	tf->tf_cs = context.sc_cs;
	tf->tf_esp = context.sc_esp;
	tf->tf_ss = context.sc_ss;

	/* Restore signal stack. */
	if (context.sc_onstack & SS_ONSTACK)
		p->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask. */
	(void) sigprocmask1(p, SIG_SETMASK, &context.sc_mask, 0);

	return (EJUSTRETURN);
}

int	waittime = -1;
struct pcb dumppcb;

void
cpu_reboot(howto, bootstr)
	int howto;
	char *bootstr;
{

	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}

	/* Disable interrupts. */
	splhigh();

	/* Do a dump if requested. */
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
		dumpsys();

haltsys:
	doshutdownhooks();

#ifdef MULTIPROCESSOR
	i386_broadcast_ipi(I386_IPI_HALT);
#endif

	if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
#if NAPM > 0 && !defined(APM_NO_POWEROFF)
		/* turn off, if we can.  But try to turn disk off and
		 * wait a bit first--some disk drives are slow to clean up
		 * and users have reported disk corruption.
		 */
		delay(500000);
		apm_set_powstate(APM_DEV_DISK(0xff), APM_SYS_OFF);
		delay(500000);
		apm_set_powstate(APM_DEV_ALLDEVS, APM_SYS_OFF);
		printf("WARNING: powerdown failed!\n");
		/*
		 * RB_POWERDOWN implies RB_HALT... fall into it...
		 */
#endif
	}

	if (howto & RB_HALT) {
		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cnpollc(1);	/* for proper keyboard command handling */
		cngetc();
		cnpollc(0);
	}

	printf("rebooting...\n");
	if (cpureset_delay > 0)
		delay(cpureset_delay * 1000);
	cpu_reset();
	for(;;) ;
	/*NOTREACHED*/
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int 	dumpsize = 0;		/* pages */
long	dumplo = 0; 		/* blocks */

/*
 * cpu_dumpsize: calculate size of machine-dependent kernel core dump headers.
 */
int
cpu_dumpsize()
{
	int size;

	size = ALIGN(sizeof(kcore_seg_t)) + ALIGN(sizeof(cpu_kcore_hdr_t)) +
	    ALIGN(mem_cluster_cnt * sizeof(phys_ram_seg_t));
	if (roundup(size, dbtob(1)) != dbtob(1))
		return (-1);

	return (1);
}

/*
 * cpu_dump_mempagecnt: calculate the size of RAM (in pages) to be dumped.
 */
u_long
cpu_dump_mempagecnt()
{
	u_long i, n;

	n = 0;
	for (i = 0; i < mem_cluster_cnt; i++)
		n += atop(mem_clusters[i].size);
	return (n);
}

/*
 * cpu_dump: dump the machine-dependent kernel core dump headers.
 */
int
cpu_dump()
{
	int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	char buf[dbtob(1)];
	kcore_seg_t *segp;
	cpu_kcore_hdr_t *cpuhdrp;
	phys_ram_seg_t *memsegp;
	int i;

	dump = bdevsw[major(dumpdev)].d_dump;

	memset(buf, 0, sizeof buf);
	segp = (kcore_seg_t *)buf;
	cpuhdrp = (cpu_kcore_hdr_t *)&buf[ALIGN(sizeof(*segp))];
	memsegp = (phys_ram_seg_t *)&buf[ ALIGN(sizeof(*segp)) +
	    ALIGN(sizeof(*cpuhdrp))];

	/*
	 * Generate a segment header.
	 */
	CORE_SETMAGIC(*segp, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	segp->c_size = dbtob(1) - ALIGN(sizeof(*segp));

	/*
	 * Add the machine-dependent header info.
	 */
	cpuhdrp->ptdpaddr = PTDpaddr;
	cpuhdrp->nmemsegs = mem_cluster_cnt;

	/*
	 * Fill in the memory segment descriptors.
	 */
	for (i = 0; i < mem_cluster_cnt; i++) {
		memsegp[i].start = mem_clusters[i].start;
		memsegp[i].size = mem_clusters[i].size;
	}

	return (dump(dumpdev, dumplo, (caddr_t)buf, dbtob(1)));
}

/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first PAGE_SIZE of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
cpu_dumpconf()
{
	int nblks, dumpblks;	/* size of dump area */
	int maj;

	if (dumpdev == NODEV)
		goto bad;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		goto bad;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		goto bad;

	dumpblks = cpu_dumpsize();
	if (dumpblks < 0)
		goto bad;
	dumpblks += ctod(cpu_dump_mempagecnt());

	/* If dump won't fit (incl. room for possible label), punt. */
	if (dumpblks > (nblks - ctod(1)))
		goto bad;

	/* Put dump at end of partition */
	dumplo = nblks - dumpblks;

	/* dumpsize is in page units, and doesn't include headers. */
	dumpsize = cpu_dump_mempagecnt();
	return;

 bad:
	dumpsize = 0;
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
#define BYTES_PER_DUMP  PAGE_SIZE /* must be a multiple of pagesize XXX small */
static vaddr_t dumpspace;

vaddr_t
reserve_dumppages(p)
	vaddr_t p;
{

	dumpspace = p;
	return (p + BYTES_PER_DUMP);
}

void
dumpsys()
{
	u_long totalbytesleft, bytes, i, n, memseg;
	u_long maddr;
	int psize;
	daddr_t blkno;
	int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	int error;

	/* Save registers. */
	savectx(&dumppcb);

	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();
	if (dumplo <= 0 || dumpsize == 0) {
		printf("\ndump to dev %u,%u not possible\n", major(dumpdev),
		    minor(dumpdev));
		return;
	}
	printf("\ndumping to dev %u,%u offset %ld\n", major(dumpdev),
	    minor(dumpdev), dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

#if 0	/* XXX this doesn't work.  grr. */
        /* toss any characters present prior to dump */
	while (sget() != NULL); /*syscons and pccons differ */
#endif

	if ((error = cpu_dump()) != 0)
		goto err;

	totalbytesleft = ptoa(cpu_dump_mempagecnt());
	blkno = dumplo + cpu_dumpsize();
	dump = bdevsw[major(dumpdev)].d_dump;
	error = 0;

	for (memseg = 0; memseg < mem_cluster_cnt; memseg++) {
		maddr = mem_clusters[memseg].start;
		bytes = mem_clusters[memseg].size;

		for (i = 0; i < bytes; i += n, totalbytesleft -= n) {
			/* Print out how many MBs we have left to go. */
			if ((totalbytesleft % (1024*1024)) == 0)
				printf("%ld ", totalbytesleft / (1024 * 1024));

			/* Limit size for next transfer. */
			n = bytes - i;
			if (n > BYTES_PER_DUMP)
				n = BYTES_PER_DUMP;

			(void) pmap_map(dumpspace, maddr, maddr + n,
			    VM_PROT_READ);

			error = (*dump)(dumpdev, blkno, (caddr_t)dumpspace, n);
			if (error)
				goto err;
			maddr += n;
			blkno += btodb(n);		/* XXX? */

#if 0	/* XXX this doesn't work.  grr. */
			/* operator aborting dump? */
			if (sget() != NULL) {
				error = EINTR;
				break;
			}
#endif
		}
	}

 err:
	switch (error) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case EINTR:
		printf("aborted from console\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
	printf("\n\n");
	delay(5000000);		/* 5 seconds */
}

/*
 * Clear registers on exec
 */
void
setregs(p, pack, stack)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct trapframe *tf;

#if NNPX > 0
	/* If we were using the FPU, forget about it. */
	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		npxsave_proc(p, 0);
#endif

#ifdef USER_LDT
	pmap_ldt_cleanup(p);
#endif

	p->p_md.md_flags &= ~MDP_USEDFPU;
	pcb->pcb_flags = 0;
	if (i386_use_fxsave) {
		pcb->pcb_savefpu.sv_xmm.sv_env.en_cw = __NetBSD_NPXCW__;
		pcb->pcb_savefpu.sv_xmm.sv_env.en_mxcsr = __INITIAL_MXCSR__;
	} else
		pcb->pcb_savefpu.sv_87.sv_env.en_cw = __NetBSD_NPXCW__;

	tf = p->p_md.md_regs;
	tf->tf_gs = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_fs = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_es = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_ds = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_edi = 0;
	tf->tf_esi = 0;
	tf->tf_ebp = 0;
	tf->tf_ebx = (int)PS_STRINGS;
	tf->tf_edx = 0;
	tf->tf_ecx = 0;
	tf->tf_eax = 0;
	tf->tf_eip = pack->ep_entry;
	tf->tf_cs = LSEL(LUCODE_SEL, SEL_UPL);
	tf->tf_eflags = PSL_USERSET;
	tf->tf_esp = stack;
	tf->tf_ss = LSEL(LUDATA_SEL, SEL_UPL);
}

/*
 * Initialize segments and descriptor tables
 */

union	descriptor *idt, *gdt, *ldt;
#ifdef I586_CPU
union	descriptor *pentium_idt;
#endif
extern  struct user *proc0paddr;

void
setgate(gd, func, args, type, dpl)
	struct gate_descriptor *gd;
	void *func;
	int args, type, dpl;
{

	gd->gd_looffset = (int)func;
	gd->gd_selector = GSEL(GCODE_SEL, SEL_KPL);
	gd->gd_stkcpy = args;
	gd->gd_xx = 0;
	gd->gd_type = type;
	gd->gd_dpl = dpl;
	gd->gd_p = 1;
	gd->gd_hioffset = (int)func >> 16;
}

void
unsetgate(gd)
	struct gate_descriptor *gd;
{
	gd->gd_p = 0;
	gd->gd_hioffset = 0;
	gd->gd_looffset = 0;
	gd->gd_selector = 0;
	gd->gd_xx = 0;
	gd->gd_stkcpy = 0;
	gd->gd_type = 0;
	gd->gd_dpl = 0;
}


void
setregion(rd, base, limit)
	struct region_descriptor *rd;
	void *base;
	size_t limit;
{

	rd->rd_limit = (int)limit;
	rd->rd_base = (int)base;
}

void
setsegment(sd, base, limit, type, dpl, def32, gran)
	struct segment_descriptor *sd;
	void *base;
	size_t limit;
	int type, dpl, def32, gran;
{

	sd->sd_lolimit = (int)limit;
	sd->sd_lobase = (int)base;
	sd->sd_type = type;
	sd->sd_dpl = dpl;
	sd->sd_p = 1;
	sd->sd_hilimit = (int)limit >> 16;
	sd->sd_xx = 0;
	sd->sd_def32 = def32;
	sd->sd_gran = gran;
	sd->sd_hibase = (int)base >> 24;
}

#define	IDTVEC(name)	__CONCAT(X, name)
typedef void (vector) __P((void));
extern vector IDTVEC(syscall);
extern vector IDTVEC(osyscall);
extern vector *IDTVEC(exceptions)[];
#ifdef COMPAT_SVR4
extern vector IDTVEC(svr4_fasttrap);
#endif /* COMPAT_SVR4 */
#ifdef COMPAT_MACH
extern vector IDTVEC(mach_trap);
#endif

#define	KBTOB(x)	((size_t)(x) * 1024UL)

void cpu_init_idt()
{
	struct region_descriptor region;
#ifdef I586_CPU
	setregion(&region, pentium_idt, NIDT * sizeof(idt[0]) - 1);
#else
	setregion(&region, idt, NIDT * sizeof(idt[0]) - 1);
#endif
        lidt(&region);
}

#if !defined(REALBASEMEM) && !defined(REALEXTMEM)
void
add_mem_cluster(seg_start, seg_end, type)
	u_int64_t seg_start, seg_end;
	u_int32_t type;
{
	extern struct extent *iomem_ex;

	if (seg_end > 0x100000000ULL) {
		printf("WARNING: skipping large "
		    "memory map entry: "
		    "0x%qx/0x%qx/0x%x\n",
		    seg_start,
		    (seg_end - seg_start),
		    type);
		return;
	}

	/*
	 * XXX Chop the last page off the size so that
	 * XXX it can fit in avail_end.
	 */
	if (seg_end == 0x100000000ULL)
		seg_end -= PAGE_SIZE;

	if (seg_end <= seg_start)
		return;

	/*
	 * Allocate the physical addresses used by RAM
	 * from the iomem extent map.  This is done before
	 * the addresses are page rounded just to make
	 * sure we get them all.
	 */
	if (extent_alloc_region(iomem_ex, seg_start,
	    seg_end - seg_start, EX_NOWAIT)) {
		/* XXX What should we do? */
		printf("WARNING: CAN'T ALLOCATE "
		    "MEMORY SEGMENT "
		    "(0x%qx/0x%qx/0x%x) FROM "
		    "IOMEM EXTENT MAP!\n",
		    seg_start, seg_end - seg_start, type);
	}

	/*
	 * If it's not free memory, skip it.
	 */
	if (type != BIM_Memory)
		return;

	/* XXX XXX XXX */
	if (mem_cluster_cnt >= VM_PHYSSEG_MAX)
		panic("init386: too many memory segments");

	seg_start = round_page(seg_start);
	seg_end = trunc_page(seg_end);

	if (seg_start == seg_end)
		return;

	mem_clusters[mem_cluster_cnt].start = seg_start;
	mem_clusters[mem_cluster_cnt].size =
	    seg_end - seg_start;

	if (avail_end < seg_end)
		avail_end = seg_end;
	physmem += atop(mem_clusters[mem_cluster_cnt].size);
	mem_cluster_cnt++;
}
#endif /* !defined(REALBASEMEM) && !defined(REALEXTMEM) */

void
init386(first_avail)
	vaddr_t first_avail;
{
	extern void consinit __P((void));
	extern struct extent *iomem_ex;
#if !defined(REALBASEMEM) && !defined(REALEXTMEM)
	struct btinfo_memmap *bim;
#endif
	struct region_descriptor region;
	int x, first16q;
	u_int64_t seg_start, seg_end;
	u_int64_t seg_start1, seg_end1;
#if NBIOSCALL > 0
	extern int biostramp_image_size;
	extern u_char biostramp_image[];
#endif

	cpu_probe_features(&cpu_info_primary);
	cpu_feature = cpu_info_primary.ci_feature_flags;

	proc0.p_addr = proc0paddr;
	curpcb = &proc0.p_addr->u_pcb;

	i386_bus_space_init();

	consinit();	/* XXX SHOULD NOT BE DONE HERE */

	/*
	 * Initailize PAGE_SIZE-dependent variables.
	 */
	uvm_setpagesize();

	/*
	 * A quick sanity check.
	 */
	if (PAGE_SIZE != NBPG)
		panic("init386: PAGE_SIZE != NBPG");

	/*
	 * Saving SSE registers won't work if the save area isn't
	 * 16-byte aligned.
	 */
	if (offsetof(struct user, u_pcb.pcb_savefpu) & 0xf)
		panic("init386: pcb_savefpu not 16-byte aligned");

	/*
	 * Start with 2 color bins -- this is just a guess to get us
	 * started.  We'll recolor when we determine the largest cache
	 * sizes on the system.
	 */
	uvmexp.ncolors = 2;

	/*
	 * Save a few pages at the start of physical memory so we play nice
	 * with various BIOS features.
	 * Page 0 contains miscellaneous BIOS data.
	 * Even if it didn't, our VM system doesn't like using zero as a
	 * physical page number.
	 * We also need pages in low memory (one each) for secondary CPU
	 * startup and for BIOS calls, plus a page table page to map
	 * them into the first few pages of the kernel's pmap.
	 */
#ifdef MULTIPROCESSOR
	avail_start = 4*PAGE_SIZE;
#else
#if NBIOSCALL > 0
	avail_start = 3*PAGE_SIZE;
#else
	avail_start = PAGE_SIZE;
#endif
#endif

	/*
	 * Call pmap initialization to make new kernel address space.
	 * We must do this before loading pages into the VM system.
	 */
	pmap_bootstrap((vaddr_t)atdevbase + IOM_SIZE);

#if !defined(REALBASEMEM) && !defined(REALEXTMEM)
	/*
	 * Check to see if we have a memory map from the BIOS (passed
	 * to us by the boot program.
	 */
	bim = lookup_bootinfo(BTINFO_MEMMAP);
	if (bim != NULL && bim->num > 0) {
#ifdef DEBUG_MEMLOAD
		printf("BIOS MEMORY MAP (%d ENTRIES):\n", bim->num);
#endif
		for (x = 0; x < bim->num; x++) {
#ifdef DEBUG_MEMLOAD
			printf("    addr 0x%qx  size 0x%qx  type 0x%x\n",
			    bim->entry[x].addr,
			    bim->entry[x].size,
			    bim->entry[x].type);
#endif

			/*
			 * If the segment is not memory, skip it.
			 */
			switch (bim->entry[x].type) {
			case BIM_Memory:
			case BIM_ACPI:
			case BIM_NVS:
				break;
			default:
				continue;
			}

			/*
			 * Sanity check the entry.
			 * XXX Need to handle uint64_t in extent code
			 * XXX and 64-bit physical addresses in i386
			 * XXX port.
			 */
			seg_start = bim->entry[x].addr;
			seg_end = bim->entry[x].addr + bim->entry[x].size;

			/*
			 *   Avoid Compatibility Holes.
			 * XXX  Holes within memory space that allow access
			 * XXX to be directed to the PC-compatible frame buffer
			 * XXX (0xa0000-0xbffff),to adapter ROM space
			 * XXX (0xc0000-0xdffff), and to system BIOS space
			 * XXX (0xe0000-0xfffff).
			 * XXX  Some laptop(for example,Toshiba Satellite2550X)
			 * XXX report this area and occurred problems,
			 * XXX so we avoid this area.
			 */
			if (seg_start < 0x100000 && seg_end > 0xa0000) {
				printf("WARNING: memory map entry overlaps "
				    "with ``Compatibility Holes'': "
				    "0x%qx/0x%qx/0x%x\n", seg_start,
				    seg_end - seg_start, bim->entry[x].type);
				add_mem_cluster(seg_start, 0xa0000,
				    bim->entry[x].type);
				add_mem_cluster(0x100000, seg_end,
				    bim->entry[x].type);
			} else
				add_mem_cluster(seg_start, seg_end,
				    bim->entry[x].type);
		}
	}
#endif /* ! REALBASEMEM && ! REALEXTMEM */

	/*
	 * If the loop above didn't find any valid segment, fall back to
	 * former code.
	 */
	if (mem_cluster_cnt == 0) {
		/*
		 * Allocate the physical addresses used by RAM from the iomem
		 * extent map.  This is done before the addresses are
		 * page rounded just to make sure we get them all.
		 */
		if (extent_alloc_region(iomem_ex, 0, KBTOB(biosbasemem),
		    EX_NOWAIT)) {
			/* XXX What should we do? */
			printf("WARNING: CAN'T ALLOCATE BASE MEMORY FROM "
			    "IOMEM EXTENT MAP!\n");
		}
		mem_clusters[0].start = 0;
		mem_clusters[0].size = trunc_page(KBTOB(biosbasemem));
		physmem += atop(mem_clusters[0].size);
		if (extent_alloc_region(iomem_ex, IOM_END, KBTOB(biosextmem),
		    EX_NOWAIT)) {
			/* XXX What should we do? */
			printf("WARNING: CAN'T ALLOCATE EXTENDED MEMORY FROM "
			    "IOMEM EXTENT MAP!\n");
		}
#if NISADMA > 0
		/*
		 * Some motherboards/BIOSes remap the 384K of RAM that would
		 * normally be covered by the ISA hole to the end of memory
		 * so that it can be used.  However, on a 16M system, this
		 * would cause bounce buffers to be allocated and used.
		 * This is not desirable behaviour, as more than 384K of
		 * bounce buffers might be allocated.  As a work-around,
		 * we round memory down to the nearest 1M boundary if
		 * we're using any isadma devices and the remapped memory
		 * is what puts us over 16M.
		 */
		if (biosextmem > (15*1024) && biosextmem < (16*1024)) {
			char pbuf[9];

			format_bytes(pbuf, sizeof(pbuf),
			    biosextmem - (15*1024));
			printf("Warning: ignoring %s of remapped memory\n",
			    pbuf);
			biosextmem = (15*1024);
		}
#endif
		mem_clusters[1].start = IOM_END;
		mem_clusters[1].size = trunc_page(KBTOB(biosextmem));
		physmem += atop(mem_clusters[1].size);

		mem_cluster_cnt = 2;

		avail_end = IOM_END + trunc_page(KBTOB(biosextmem));
	}

	/*
	 * If we have 16M of RAM or less, just put it all on
	 * the default free list.  Otherwise, put the first
	 * 16M of RAM on a lower priority free list (so that
	 * all of the ISA DMA'able memory won't be eaten up
	 * first-off).
	 */
	if (avail_end <= (16 * 1024 * 1024))
		first16q = VM_FREELIST_DEFAULT;
	else
		first16q = VM_FREELIST_FIRST16;

	/* Make sure the end of the space used by the kernel is rounded. */
	first_avail = round_page(first_avail);

	/*
	 * Now, load the memory clusters (which have already been
	 * rounded and truncated) into the VM system.
	 *
	 * NOTE: WE ASSUME THAT MEMORY STARTS AT 0 AND THAT THE KERNEL
	 * IS LOADED AT IOM_END (1M).
	 */
	for (x = 0; x < mem_cluster_cnt; x++) {
		seg_start = mem_clusters[x].start;
		seg_end = mem_clusters[x].start + mem_clusters[x].size;
		seg_start1 = 0;
		seg_end1 = 0;

		/*
		 * Skip memory before our available starting point.
		 */
		if (seg_end <= avail_start)
			continue;

		if (avail_start >= seg_start && avail_start < seg_end) {
			if (seg_start != 0)
				panic("init386: memory doesn't start at 0");
			seg_start = avail_start;
			if (seg_start == seg_end)
				continue;
		}

		/*
		 * If this segment contains the kernel, split it
		 * in two, around the kernel.
		 */
		if (seg_start <= IOM_END && first_avail <= seg_end) {
			seg_start1 = first_avail;
			seg_end1 = seg_end;
			seg_end = IOM_END;
		}

		/* First hunk */
		if (seg_start != seg_end) {
			if (seg_start <= (16 * 1024 * 1024) &&
			    first16q != VM_FREELIST_DEFAULT) {
				u_int64_t tmp;

				if (seg_end > (16 * 1024 * 1024))
					tmp = (16 * 1024 * 1024);
				else
					tmp = seg_end;
#ifdef DEBUG_MEMLOAD
				printf("loading 0x%qx-0x%qx (0x%lx-0x%lx)\n",
				    seg_start, tmp,
				    atop(seg_start), atop(tmp));
#endif
				uvm_page_physload(atop(seg_start),
				    atop(tmp), atop(seg_start),
				    atop(tmp), first16q);
				seg_start = tmp;
			}

			if (seg_start != seg_end) {
#ifdef DEBUG_MEMLOAD
				printf("loading 0x%qx-0x%qx (0x%lx-0x%lx)\n",
				    seg_start, seg_end,
				    atop(seg_start), atop(seg_end));
#endif
				uvm_page_physload(atop(seg_start),
				    atop(seg_end), atop(seg_start),
				    atop(seg_end), VM_FREELIST_DEFAULT);
			}
		}

		/* Second hunk */
		if (seg_start1 != seg_end1) {
			if (seg_start1 <= (16 * 1024 * 1024) &&
			    first16q != VM_FREELIST_DEFAULT) {
				u_int64_t tmp;

				if (seg_end1 > (16 * 1024 * 1024))
					tmp = (16 * 1024 * 1024);
				else
					tmp = seg_end1;
#ifdef DEBUG_MEMLOAD
				printf("loading 0x%qx-0x%qx (0x%lx-0x%lx)\n",
				    seg_start1, tmp,
				    atop(seg_start1), atop(tmp));
#endif
				uvm_page_physload(atop(seg_start1),
				    atop(tmp), atop(seg_start1),
				    atop(tmp), first16q);
				seg_start1 = tmp;
			}

			if (seg_start1 != seg_end1) {
#ifdef DEBUG_MEMLOAD
				printf("loading 0x%qx-0x%qx (0x%lx-0x%lx)\n",
				    seg_start1, seg_end1,
				    atop(seg_start1), atop(seg_end1));
#endif
				uvm_page_physload(atop(seg_start1),
				    atop(seg_end1), atop(seg_start1),
				    atop(seg_end1), VM_FREELIST_DEFAULT);
			}
		}
	}

	/*
	 * Steal memory for the message buffer (at end of core).
	 */
	{
		struct vm_physseg *vps;
		psize_t sz = round_page(MSGBUFSIZE);
		psize_t reqsz = sz;

		for (x = 0; x < vm_nphysseg; x++) {
			vps = &vm_physmem[x];
			if (ptoa(vps->avail_end) == avail_end)
				break;
		}
		if (x == vm_nphysseg)
			panic("init386: can't find end of memory");

		/* Shrink so it'll fit in the last segment. */
		if ((vps->avail_end - vps->avail_start) < atop(sz))
			sz = ptoa(vps->avail_end - vps->avail_start);

		vps->avail_end -= atop(sz);
		vps->end -= atop(sz);
		msgbuf_paddr = ptoa(vps->avail_end);

		/* Remove the last segment if it now has no pages. */
		if (vps->start == vps->end) {
			for (vm_nphysseg--; x < vm_nphysseg; x++)
				vm_physmem[x] = vm_physmem[x + 1];
		}

		/* Now find where the new avail_end is. */
		for (avail_end = 0, x = 0; x < vm_nphysseg; x++)
			if (vm_physmem[x].avail_end > avail_end)
				avail_end = vm_physmem[x].avail_end;
		avail_end = ptoa(avail_end);

		/* Warn if the message buffer had to be shrunk. */
		if (sz != reqsz)
			printf("WARNING: %ld bytes not available for msgbuf "
			    "in last cluster (%ld used)\n", reqsz, sz);
	}

#if NBIOSCALL > 0 || defined(MULTIPROCESSOR)
	/* install page 2 (reserved above) as PT page for first 4M */
	pmap_enter(pmap_kernel(), (vaddr_t)vtopte(0), 2*PAGE_SIZE,
	    VM_PROT_READ|VM_PROT_WRITE, PMAP_WIRED|VM_PROT_READ|VM_PROT_WRITE);
	pmap_update(pmap_kernel());
	memset(vtopte(0), 0, PAGE_SIZE);/* make sure it is clean before using */

	/*
	 * this should be caught at kernel build time, but put it here
	 * in case someone tries to fake it out...
	 */
#ifdef DIAGNOSTIC
	if (biostramp_image_size > PAGE_SIZE)
	    panic("biostramp_image_size too big: %x vs. %x\n",
		  biostramp_image_size, PAGE_SIZE);
#endif
	pmap_kenter_pa((vaddr_t)BIOSTRAMP_BASE,	/* virtual */
		       (paddr_t)BIOSTRAMP_BASE,	/* physical */
		       VM_PROT_ALL);		/* protection */
	pmap_update(pmap_kernel());
	memcpy((caddr_t)BIOSTRAMP_BASE, biostramp_image, biostramp_image_size);
#ifdef DEBUG_BIOSCALL
	printf("biostramp installed @ %x\n", BIOSTRAMP_BASE);
#endif
#endif

	pmap_enter(pmap_kernel(), idt_vaddr, idt_paddr,
	    VM_PROT_READ|VM_PROT_WRITE, PMAP_WIRED|VM_PROT_READ|VM_PROT_WRITE);
	idt = (union descriptor *)idt_vaddr;

#ifdef I586_CPU
	pmap_enter(pmap_kernel(), pentium_idt_vaddr, idt_paddr,
	    VM_PROT_READ, PMAP_WIRED|VM_PROT_READ);
	pentium_idt = (union descriptor *)pentium_idt_vaddr;
#endif
	pmap_update(pmap_kernel());

	gdt = idt + NIDT;
	ldt = gdt + NGDT;


	/* make gdt gates and memory segments */
	setsegment(&gdt[GCODE_SEL].sd, 0, 0xfffff, SDT_MEMERA, SEL_KPL, 1, 1);
	setsegment(&gdt[GDATA_SEL].sd, 0, 0xfffff, SDT_MEMRWA, SEL_KPL, 1, 1);
	setsegment(&gdt[GLDT_SEL].sd, ldt, NLDT * sizeof(ldt[0]) - 1,
	    SDT_SYSLDT, SEL_KPL, 0, 0);
	setsegment(&gdt[GUCODE_SEL].sd, 0, i386_btop(VM_MAXUSER_ADDRESS) - 1,
	    SDT_MEMERA, SEL_UPL, 1, 1);
	setsegment(&gdt[GUDATA_SEL].sd, 0, i386_btop(VM_MAXUSER_ADDRESS) - 1,
	    SDT_MEMRWA, SEL_UPL, 1, 1);
#if NBIOSCALL > 0
	/* bios trampoline GDT entries */
	setsegment(&gdt[GBIOSCODE_SEL].sd, 0, 0xfffff, SDT_MEMERA, SEL_KPL, 0,
	    0);
	setsegment(&gdt[GBIOSDATA_SEL].sd, 0, 0xfffff, SDT_MEMRWA, SEL_KPL, 0,
	    0);
#endif
#ifdef GMACHCALLS_SEL
	setgate(&gdt[GMACHCALLS_SEL].gd, &IDTVEC(mach_trap), 1,
	    SDT_SYS386CGT, SEL_UPL);
#endif

	/* make ldt gates and memory segments */
	setgate(&ldt[LSYS5CALLS_SEL].gd, &IDTVEC(osyscall), 1,
	    SDT_SYS386CGT, SEL_UPL);

	ldt[LUCODE_SEL] = gdt[GUCODE_SEL];
	ldt[LUDATA_SEL] = gdt[GUDATA_SEL];
	ldt[LSOL26CALLS_SEL] = ldt[LBSDICALLS_SEL] = ldt[LSYS5CALLS_SEL];

	/* exceptions */
	for (x = 0; x < 32; x++)
		setgate(&idt[x].gd, IDTVEC(exceptions)[x], 0, SDT_SYS386TGT,
		    (x == 3 || x == 4) ? SEL_UPL : SEL_KPL);

	/* new-style interrupt gate for syscalls */
	setgate(&idt[128].gd, &IDTVEC(syscall), 0, SDT_SYS386TGT, SEL_UPL);
#ifdef COMPAT_SVR4
	setgate(&idt[0xd2].gd, &IDTVEC(svr4_fasttrap), 0, SDT_SYS386TGT,
	    SEL_UPL);
#endif /* COMPAT_SVR4 */

	setregion(&region, gdt, NGDT * sizeof(gdt[0]) - 1);
	lgdt(&region);

	cpu_init_idt();

#ifdef I586_CPU
	setregion(&region, pentium_idt, NIDT * sizeof(idt[0]) - 1);
#else
	setregion(&region, idt, NIDT * sizeof(idt[0]) - 1);
#endif
	lidt(&region);


#ifdef DDB
	{
		extern int end;
		extern int *esym;
		struct btinfo_symtab *symtab;

		db_machine_init();

		symtab = lookup_bootinfo(BTINFO_SYMTAB);

		if (symtab) {
			symtab->ssym += KERNBASE;
			symtab->esym += KERNBASE;
			ddb_init(symtab->nsym, (int *)symtab->ssym,
			    (int *)symtab->esym);
		}
		else
			ddb_init(*(int *)&end, ((int *)&end) + 1, esym);
	}
	if (boothowto & RB_KDB)
		Debugger();
#endif
#ifdef IPKDB
	ipkdb_init();
	if (boothowto & RB_KDB)
		ipkdb_connect(0);
#endif
#ifdef KGDB
	kgdb_port_init();
	if (boothowto & RB_KDB) {
		kgdb_debug_init = 1;
		kgdb_connect(1);
	}
#endif

#if NMCA > 0
	/* check for MCA bus, needed to be done before ISA stuff - if
	 * MCA is detected, ISA needs to use level triggered interrupts
	 * by default */
	mca_busprobe();
#endif

#if NISA > 0
	isa_defaultirq();
#endif

	/* Initialize software interrupts. */
	softintr_init();

	splraise(IPL_SERIAL);	/* XXX MP clean me */
	enable_intr();

	if (physmem < btoc(2 * 1024 * 1024)) {
		printf("warning: too little memory available; "
		       "have %lu bytes, want %lu bytes\n"
		       "running in degraded mode\n"
		       "press a key to confirm\n\n",
		       ptoa(physmem), 2*1024*1024UL);
		cngetc();
	}
}

#ifdef COMPAT_NOMID
static int
exec_nomid(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error;
	u_long midmag, magic;
	u_short mid;
	struct exec *execp = epp->ep_hdr;

	/* check on validity of epp->ep_hdr performed by exec_out_makecmds */

	midmag = ntohl(execp->a_midmag);
	mid = (midmag >> 16) & 0xffff;
	magic = midmag & 0xffff;

	if (magic == 0) {
		magic = (execp->a_midmag & 0xffff);
		mid = MID_ZERO;
	}

	midmag = mid << 16 | magic;

	switch (midmag) {
	case (MID_ZERO << 16) | ZMAGIC:
		/*
		 * 386BSD's ZMAGIC format:
		 */
		error = exec_aout_prep_oldzmagic(p, epp);
		break;

	case (MID_ZERO << 16) | QMAGIC:
		/*
		 * BSDI's QMAGIC format:
		 * same as new ZMAGIC format, but with different magic number
		 */
		error = exec_aout_prep_zmagic(p, epp);
		break;

	case (MID_ZERO << 16) | NMAGIC:
		/*
		 * BSDI's NMAGIC format:
		 * same as NMAGIC format, but with different magic number
		 * and with text starting at 0.
		 */
		error = exec_aout_prep_oldnmagic(p, epp);
		break;

	case (MID_ZERO << 16) | OMAGIC:
		/*
		 * BSDI's OMAGIC format:
		 * same as OMAGIC format, but with different magic number
		 * and with text starting at 0.
		 */
		error = exec_aout_prep_oldomagic(p, epp);
		break;

	default:
		error = ENOEXEC;
	}

	return error;
}
#endif

/*
 * cpu_exec_aout_makecmds():
 *	cpu-dependent a.out format hook for execve().
 *
 * Determine of the given exec package refers to something which we
 * understand and, if so, set up the vmcmds for it.
 *
 * On the i386, old (386bsd) ZMAGIC binaries and BSDI QMAGIC binaries
 * if COMPAT_NOMID is given as a kernel option.
 */
int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error = ENOEXEC;

#ifdef COMPAT_NOMID
	if ((error = exec_nomid(p, epp)) == 0)
		return error;
#endif /* ! COMPAT_NOMID */

	return error;
}

void *
lookup_bootinfo(type)
int type;
{
	struct btinfo_common *help;
	int n = *(int*)bootinfo;
	help = (struct btinfo_common *)(bootinfo + sizeof(int));
	while(n--) {
		if(help->type == type)
			return(help);
		help = (struct btinfo_common *)((char*)help + help->len);
	}
	return(0);
}

#include <dev/ic/mc146818reg.h>		/* for NVRAM POST */
#include <i386/isa/nvram.h>		/* for NVRAM POST */

void
cpu_reset()
{

	disable_intr();

	/*
	 * Ensure the NVRAM reset byte contains something vaguely sane.
	 */

	outb(IO_RTC, NVRAM_RESET);
	outb(IO_RTC+1, NVRAM_RESET_RST);

	/*
	 * The keyboard controller has 4 random output pins, one of which is
	 * connected to the RESET pin on the CPU in many PCs.  We tell the
	 * keyboard controller to pulse this line a couple of times.
	 */
	outb(IO_KBD + KBCMDP, KBC_PULSE0);
	delay(100000);
	outb(IO_KBD + KBCMDP, KBC_PULSE0);
	delay(100000);

	/*
	 * Try to cause a triple fault and watchdog reset by making the IDT
	 * invalid and causing a fault.
	 */
	memset((caddr_t)idt, 0, NIDT * sizeof(idt[0]));
	__asm __volatile("divl %0,%1" : : "q" (0), "a" (0));

#if 0
	/*
	 * Try to cause a triple fault and watchdog reset by unmapping the
	 * entire address space and doing a TLB flush.
	 */
	memset((caddr_t)PTD, 0, PAGE_SIZE);
	tlbflush();
#endif

	for (;;);
}

void cpu_initclocks()
{
	(*initclock_func)();
}

#ifdef MULTIPROCESSOR
void need_resched(struct cpu_info *ci)
{
	ci->ci_want_resched = 1;
	aston(ci);
}
#endif

/*
 * Allocate an IDT vector slot within the given range.
 * XXX needs locking to avoid MP allocation races.
 */

int
idt_vec_alloc (low, high)
	int low;
	int high;
{
	int vec;

	for (vec=low; vec<=high; vec++)
		if (idt[vec].gd.gd_p == 0)
			return vec;
	return 0;
}

void idt_vec_set (vec, function)
	int vec;
	void (*function) __P((void));
{
	setgate (&idt[vec].gd, function, 0, SDT_SYS386IGT, SEL_KPL);
}

void
idt_vec_free (vec)
	int vec;
{
	unsetgate(&idt[vec].gd);
}

extern void xxx_lapic_time(void);

void
xxx_lapic_time(void)
{
	uint64_t before, after;
	int i, sum;

	before=rdtsc();
	for (i=0; i<1000000; i++) {
		sum += lapic_tpr;
	}
	after=rdtsc();
	printf("1000000 lapic_tpr reads: %llx\n", after-before);
}

