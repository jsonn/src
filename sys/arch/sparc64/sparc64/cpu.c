/*	$NetBSD: cpu.c,v 1.16.4.2 2002/01/08 00:28:01 nathanw Exp $ */

/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
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
 *	@(#)cpu.c	8.5 (Berkeley) 11/23/93
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/pmap.h>

#include <sparc64/sparc64/cache.h>

/* This is declared here so that you must include a CPU for the cache code. */
struct cacheinfo cacheinfo;

/* Our exported CPU info; we have only one for now. */  
struct cpu_info cpu_info_store;

/* Linked list of all CPUs in system. */
struct cpu_info *cpus = NULL;

/* The following are used externally (sysctl_hw). */
char	machine[] = MACHINE;		/* from <machine/param.h> */
char	machine_arch[] = MACHINE_ARCH;	/* from <machine/param.h> */
char	cpu_model[100];

/* The CPU configuration driver. */
static void cpu_attach __P((struct device *, struct device *, void *));
int  cpu_match __P((struct device *, struct cfdata *, void *));

struct cfattach cpu_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

extern struct cfdriver cpu_cd;

#if defined(SUN4C) || defined(SUN4M)
static char *psrtoname __P((int, int, int, char *));
#endif
static char *fsrtoname __P((int, int, int, char *));

#define	IU_IMPL(v)	((((u_int64_t)(v))&VER_IMPL) >> VER_IMPL_SHIFT)
#define	IU_VERS(v)	((((u_int64_t)(v))&VER_MASK) >> VER_MASK_SHIFT)

#ifdef notdef
/*
 * IU implementations are parceled out to vendors (with some slight
 * glitches).  Printing these is cute but takes too much space.
 */
static char *iu_vendor[16] = {
	"Fujitsu",	/* and also LSI Logic */
	"ROSS",		/* ROSS (ex-Cypress) */
	"BIT",
	"LSIL",		/* LSI Logic finally got their own */
	"TI",		/* Texas Instruments */
	"Matsushita",
	"Philips",
	"Harvest",	/* Harvest VLSI Design Center */
	"SPEC",		/* Systems and Processes Engineering Corporation */
	"Weitek",
	"vendor#10",
	"vendor#11",
	"vendor#12",
	"vendor#13",
	"vendor#14",
	"vendor#15"
};
#endif

/*
 * Overhead involved in firing up a new CPU:
 * 
 *	Allocate a cpuinfo/interrupt stack
 *	Map that into the kernel
 *	Initialize the cpuinfo
 *	Return the TLB entry for the cpuinfo.
 */
u_int64_t
cpu_init(pa, cpu_num)
	paddr_t pa;
	int cpu_num;
{
	struct cpu_info *ci;
	u_int64_t pagesize;
	u_int64_t pte;
	struct vm_page *m;
	psize_t size;
	vaddr_t va;
	struct pglist mlist;
	int error;

	size = NBPG; /* XXXX 8K, 64K, 512K, or 4MB */
	TAILQ_INIT(&mlist);
	if ((error = uvm_pglistalloc((psize_t)size, (paddr_t)0, (paddr_t)-1,
		(paddr_t)size, (paddr_t)0, &mlist, 1, 0)) != 0)
		panic("cpu_start: no memory, error %d", error);

	va = uvm_km_valloc(kernel_map, size);
	if (va == 0)
		panic("cpu_start: no memory");

	m = TAILQ_FIRST(&mlist);
	pa = VM_PAGE_TO_PHYS(m);
	pte = TSB_DATA(0 /* global */,
		pagesize,
		pa,
		1 /* priv */,
		1 /* Write */,
		1 /* Cacheable */,
		1 /* ALIAS -- Disable D$ */,
		1 /* valid */,
		0 /* IE */);

	/* Map the pages */
	for (; m != NULL; m = TAILQ_NEXT(m,pageq)) {
		pa = VM_PAGE_TO_PHYS(m);
		pmap_zero_page(pa);
		pmap_enter(pmap_kernel(), va, pa | PMAP_NVC,
			VM_PROT_READ|VM_PROT_WRITE,
			VM_PROT_READ|VM_PROT_WRITE|PMAP_WIRED);
		va += NBPG;
	}
	pmap_update(pmap_kernel());

	if (!cpus) cpus = (struct cpu_info *)va;
	else {
		for (ci = cpus; ci->ci_next; ci=ci->ci_next);
		ci->ci_next = (struct cpu_info *)va;
	}

	switch (size) {
#define K	*1024
	case 8 K:
		pagesize = TLB_8K;
		break;
	case 64 K:
		pagesize = TLB_64K;
		break;
	case 512 K:
		pagesize = TLB_512K;
		break;
	case 4 K K:
		pagesize = TLB_4M;
		break;
	default:
		panic("cpu_start: stack size %x not a machine page size\n",
			(unsigned)size);
	}
	return (pte|TLB_L);
}


int
cpu_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;

	return (strcmp(cf->cf_driver->cd_name, ma->ma_name) == 0);
}

/*
 * Attach the CPU.
 * Discover interesting goop about the virtual address cache
 * (slightly funny place to do it, but this is where it is to be found).
 */
static void
cpu_attach(parent, dev, aux)
	struct device *parent;
	struct device *dev;
	void *aux;
{
	int node;
	long clk;
	int impl, vers, fver;
	char *fpuname;
	struct mainbus_attach_args *ma = aux;
	struct fpstate64 *fpstate;
	struct fpstate64 fps[2];
	char *sep;
	char fpbuf[40];
	register int i, l;
	u_int64_t ver;
	int bigcache, cachesize;
	extern u_int64_t cpu_clockrate[];

	/* This needs to be 64-bit aligned */
	fpstate = ALIGNFPSTATE(&fps[1]);
	/*
	 * Get the FSR and clear any exceptions.  If we do not unload
	 * the queue here and it is left over from a previous crash, we
	 * will panic in the first loadfpstate(), due to a sequence error,
	 * so we need to dump the whole state anyway.
	 *
	 * If there is no FPU, trap.c will advance over all the stores,
	 * so we initialize fs_fsr here.
	 */
	fpstate->fs_fsr = 7 << FSR_VER_SHIFT;	/* 7 is reserved for "none" */
	savefpstate(fpstate);
	fver = (fpstate->fs_fsr >> FSR_VER_SHIFT) & (FSR_VER >> FSR_VER_SHIFT);
	ver = getver();
	impl = IU_IMPL(ver);
	vers = IU_VERS(ver);
	if (fver != 7) {
		foundfpu = 1;
		fpuname = fsrtoname(impl, vers, fver, fpbuf);
	} else
		fpuname = "no";

	/* tell them what we have */
	node = ma->ma_node;

	clk = PROM_getpropint(node, "clock-frequency", 0);
	if (clk == 0) {
		/*
		 * Try to find it in the OpenPROM root...
		 */
		clk = PROM_getpropint(findroot(), "clock-frequency", 0);
	}
	if (clk) {
		cpu_clockrate[0] = clk; /* Tell OS what frequency we run on */
		cpu_clockrate[1] = clk/1000000;
	}
	sprintf(cpu_model, "%s @ %s MHz, %s FPU",
		PROM_getpropstring(node, "name"),
		clockfreq(clk), fpuname);
	printf(": %s\n", cpu_model);

	bigcache = 0;

	cacheinfo.c_physical = 1; /* Dunno... */
	cacheinfo.c_split = 1;
	cacheinfo.ic_linesize = l = PROM_getpropint(node, "icache-line-size", 0);
	for (i = 0; (1 << i) < l && l; i++)
		/* void */;
	if ((1 << i) != l && l)
		panic("bad icache line size %d", l);
	cacheinfo.ic_l2linesize = i;
	cacheinfo.ic_totalsize =
		PROM_getpropint(node, "icache-size", 0) *
		PROM_getpropint(node, "icache-associativity", 1);
	if (cacheinfo.ic_totalsize == 0)
		cacheinfo.ic_totalsize = l *
			PROM_getpropint(node, "icache-nlines", 64) *
			PROM_getpropint(node, "icache-associativity", 1);

	cachesize = cacheinfo.ic_totalsize /
	    PROM_getpropint(node, "icache-associativity", 1);
	bigcache = cachesize;

	cacheinfo.dc_linesize = l =
		PROM_getpropint(node, "dcache-line-size",0);
	for (i = 0; (1 << i) < l && l; i++)
		/* void */;
	if ((1 << i) != l && l)
		panic("bad dcache line size %d", l);
	cacheinfo.dc_l2linesize = i;
	cacheinfo.dc_totalsize =
		PROM_getpropint(node, "dcache-size", 0) *
		PROM_getpropint(node, "dcache-associativity", 1);
	if (cacheinfo.dc_totalsize == 0)
		cacheinfo.dc_totalsize = l *
			PROM_getpropint(node, "dcache-nlines", 128) *
			PROM_getpropint(node, "dcache-associativity", 1);

	cachesize = cacheinfo.dc_totalsize /
	    PROM_getpropint(node, "dcache-associativity", 1);
	if (cachesize > bigcache)
		bigcache = cachesize;

	cacheinfo.ec_linesize = l =
		PROM_getpropint(node, "ecache-line-size", 0);
	for (i = 0; (1 << i) < l && l; i++)
		/* void */;
	if ((1 << i) != l && l)
		panic("bad ecache line size %d", l);
	cacheinfo.ec_l2linesize = i;
	cacheinfo.ec_totalsize = 
		PROM_getpropint(node, "ecache-size", 0) *
		PROM_getpropint(node, "ecache-associativity", 1);
	if (cacheinfo.ec_totalsize == 0)
		cacheinfo.ec_totalsize = l *
			PROM_getpropint(node, "ecache-nlines", 32768) *
			PROM_getpropint(node, "ecache-associativity", 1);

	cachesize = cacheinfo.ec_totalsize /
	     PROM_getpropint(node, "ecache-associativity", 1);
	if (cachesize > bigcache)
		bigcache = cachesize;

	/*
	 * XXX - The following will have to do until
	 * we have per-cpu cache handling.
	 */
	cacheinfo.c_l2linesize =
		min(cacheinfo.ic_l2linesize,
		    cacheinfo.dc_l2linesize);
	cacheinfo.c_linesize =
		min(cacheinfo.ic_linesize,
		    cacheinfo.dc_linesize);
	cacheinfo.c_totalsize =
		cacheinfo.ic_totalsize +
		cacheinfo.dc_totalsize;

	if (cacheinfo.c_totalsize == 0)
		return;
	
	sep = " ";
	printf("%s: physical", dev->dv_xname);
	if (cacheinfo.ic_totalsize > 0) {
		printf("%s%ldK instruction (%ld b/l)", sep,
		       (long)cacheinfo.ic_totalsize/1024,
		       (long)cacheinfo.ic_linesize);
		sep = ", ";
	}
	if (cacheinfo.dc_totalsize > 0) {
		printf("%s%ldK data (%ld b/l)", sep,
		       (long)cacheinfo.dc_totalsize/1024,
		       (long)cacheinfo.dc_linesize);
		sep = ", ";
	}
	if (cacheinfo.ec_totalsize > 0) {
		printf("%s%ldK external (%ld b/l)", sep,
		       (long)cacheinfo.ec_totalsize/1024,
		       (long)cacheinfo.ec_linesize);
	}
	printf("\n");
	cache_enable();

	/*
	 * Now that we know the size of the largest cache on this CPU,
	 * re-color our pages.
	 */
	uvm_page_recolor(atop(bigcache));
}

/*
 * The following tables convert <IU impl, IU version, FPU version> triples
 * into names for the CPU and FPU chip.  In most cases we do not need to
 * inspect the FPU version to name the IU chip, but there is one exception
 * (for Tsunami), and this makes the tables the same.
 *
 * The table contents (and much of the structure here) are from Guy Harris.
 *
 */
struct info {
	u_char	valid;
	u_char	iu_impl;
	u_char	iu_vers;
	u_char	fpu_vers;
	char	*name;
};

#define	ANY	0xff	/* match any FPU version (or, later, IU version) */

#if defined(SUN4C) || defined(SUN4M)
static struct info iu_types[] = {
	{ 1, 0x0, 0x4, 4,   "MB86904" },
	{ 1, 0x0, 0x0, ANY, "MB86900/1A or L64801" },
	{ 1, 0x1, 0x0, ANY, "RT601 or L64811 v1" },
	{ 1, 0x1, 0x1, ANY, "RT601 or L64811 v2" },
	{ 1, 0x1, 0x3, ANY, "RT611" },
	{ 1, 0x1, 0xf, ANY, "RT620" },
	{ 1, 0x2, 0x0, ANY, "B5010" },
	{ 1, 0x4, 0x0,   0, "TMS390Z50 v0 or TMS390Z55" },
	{ 1, 0x4, 0x1,   0, "TMS390Z50 v1" },
	{ 1, 0x4, 0x1,   4, "TMS390S10" },
	{ 1, 0x5, 0x0, ANY, "MN10501" },
	{ 1, 0x9, 0x0, ANY, "W8601/8701 or MB86903" },
	{ 0 }
};

static char *
psrtoname(impl, vers, fver, buf)
	register int impl, vers, fver;
	char *buf;
{
	register struct info *p;

	for (p = iu_types; p->valid; p++)
		if (p->iu_impl == impl && p->iu_vers == vers &&
		    (p->fpu_vers == fver || p->fpu_vers == ANY))
			return (p->name);

	/* Not found. */
	sprintf(buf, "IU impl 0x%x vers 0x%x", impl, vers);
	return (buf);
}
#endif /* SUN4C || SUN4M */

/* NB: table order matters here; specific numbers must appear before ANY. */
static struct info fpu_types[] = {
	/*
	 * Vendor 0, IU Fujitsu0.
	 */
	{ 1, 0x0, ANY, 0, "MB86910 or WTL1164/5" },
	{ 1, 0x0, ANY, 1, "MB86911 or WTL1164/5" },
	{ 1, 0x0, ANY, 2, "L64802 or ACT8847" },
	{ 1, 0x0, ANY, 3, "WTL3170/2" },
	{ 1, 0x0, 4,   4, "on-chip" },		/* Swift */
	{ 1, 0x0, ANY, 4, "L64804" },

	/*
	 * Vendor 1, IU ROSS0/1 or Pinnacle.
	 */
	{ 1, 0x1, 0xf, 0, "on-chip" },		/* Pinnacle */
	{ 1, 0x1, ANY, 0, "L64812 or ACT8847" },
	{ 1, 0x1, ANY, 1, "L64814" },
	{ 1, 0x1, ANY, 2, "TMS390C602A" },
	{ 1, 0x1, ANY, 3, "RT602 or WTL3171" },

	/*
	 * Vendor 2, IU BIT0.
	 */
	{ 1, 0x2, ANY, 0, "B5010 or B5110/20 or B5210" },

	/*
	 * Vendor 4, Texas Instruments.
	 */
	{ 1, 0x4, ANY, 0, "on-chip" },		/* Viking */
	{ 1, 0x4, ANY, 4, "on-chip" },		/* Tsunami */

	/*
	 * Vendor 5, IU Matsushita0.
	 */
	{ 1, 0x5, ANY, 0, "on-chip" },

	/*
	 * Vendor 9, Weitek.
	 */
	{ 1, 0x9, ANY, 3, "on-chip" },

	{ 0 }
};

static char *
fsrtoname(impl, vers, fver, buf)
	register int impl, vers, fver;
	char *buf;
{
	register struct info *p;

	for (p = fpu_types; p->valid; p++)
		if (p->iu_impl == impl &&
		    (p->iu_vers == vers || p->iu_vers == ANY) &&
		    (p->fpu_vers == fver))
			return (p->name);
	sprintf(buf, "version %x", fver);
	return (buf);
}
