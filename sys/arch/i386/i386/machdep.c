/*	$NetBSD: machdep.c,v 1.241.2.5 1997/09/22 06:31:04 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * Copyright (c) 1993, 1994, 1995, 1996 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992 Terrence R. Lambert.
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
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <dev/cons.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <sys/sysctl.h>

#define _I386_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#include <machine/bootinfo.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/i8042reg.h>
#include <dev/ic/mc146818reg.h>
#include <i386/isa/isa_machdep.h>
#include <i386/isa/nvram.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

#ifdef VM86
#include <machine/vm86.h>
#endif

#include "apm.h"

#if NAPM > 0
#include <machine/apmvar.h>
#endif

#include "isa.h"
#include "isadma.h"
#include "npx.h"
#if NNPX > 0
extern struct proc *npxproc;
#endif

#include "pc.h"
#if (NPC > 0)
#include <machine/pccons.h>
#endif

#include "vt.h"
#if (NVT > 0)
#include <i386/isa/pcvt/pcvt_cons.h>
#endif

#include "com.h"
#if (NCOM > 0)
#include <sys/termios.h>
#include <dev/isa/comreg.h>
#include <dev/isa/comvar.h>
#endif

/* the following is used externally (sysctl_hw) */
char machine[] = "i386";		/* cpu "architecture" */
char machine_arch[] = "i386";		/* machine == machine_arch */

char bootinfo[BOOTINFO_MAXSIZE];

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif

int	physmem;
int	dumpmem_low;
int	dumpmem_high;
int	boothowto;
int	cpu_class;

caddr_t	msgbufaddr;

vm_map_t buffer_map;

extern	int biosbasemem, biosextmem;
extern	vm_offset_t avail_start, avail_end;
static	vm_offset_t hole_start, hole_end;
static	vm_offset_t avail_next;

/*
 * Extent maps to manage I/O and ISA memory hole space.  Allocate
 * storage for 8 regions in each, initially.  Later, ioport_malloc_safe
 * will indicate that it's safe to use malloc() to dynamically allocate
 * region descriptors.
 *
 * N.B. At least two regions are _always_ allocated from the iomem
 * extent map; (0 -> ISA hole) and (end of ISA hole -> end of RAM).
 *
 * The extent maps are not static!  Machine-dependent ISA and EISA
 * routines need access to them for bus address space allocation.
 */
static	long ioport_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];
static	long iomem_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];
struct	extent *ioport_ex;
struct	extent *iomem_ex;
static	ioport_malloc_safe;

caddr_t	allocsys __P((caddr_t));
void	dumpsys __P((void));
void	identifycpu __P((void));
void	init386 __P((vm_offset_t));

#ifndef CONSDEVNAME
#define CONSDEVNAME "pc"
#endif
#ifndef CONADDR
#define CONADDR 0x3f8
#endif
#ifndef CONSPEED
#define CONSPEED TTYDEF_SPEED
#endif
#ifndef CONMODE
#define CONMODE ((TTYDEF_CFLAG & ~(CSIZE | CSTOPB | PARENB)) | CS8) /* 8N1 */
#endif
struct btinfo_console default_consinfo = {
	{0, 0},
	CONSDEVNAME,
	CONADDR, CONSPEED
};
int comcnmode = CONMODE;
void	consinit __P((void));

#ifdef KGDB
#ifndef KGDB_DEVNAME
#define KGDB_DEVNAME "com"
#endif
char kgdb_devname[] = KGDB_DEVNAME;
#if (NCOM > 0)
#ifndef KGDBADDR
#define KGDBADDR 0x3f8
#endif
int comkgdbaddr = KGDBADDR;
#ifndef KGDBRATE
#define KGDBRATE TTYDEF_SPEED
#endif
int comkgdbrate = KGDBRATE;
#ifndef KGDBMODE
#define KGDBMODE ((TTYDEF_CFLAG & ~(CSIZE | CSTOPB | PARENB)) | CS8) /* 8N1 */
#endif
int comkgdbmode = KGDBMODE;
#endif /* NCOM */
void kgdb_port_init __P((void));
#endif /* KGDB */

#ifdef COMPAT_NOMID
static int exec_nomid	__P((struct proc *, struct exec_package *));
#endif

int	i386_mem_add_mapping __P((bus_addr_t, bus_size_t,
	    int, bus_space_handle_t *));

/*
 * Machine-dependent startup code
 */
void
cpu_startup()
{
	unsigned i;
	caddr_t v;
	int sz;
	int base, residual;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
	struct pcb *pcb;
	int x;
#if NAPM > 0
	extern int biostramp_image_size;
	extern u_char biostramp_image[];
#endif

	/*
	 * Initialize error message buffer (at end of core).
	 */
	/* avail_end was pre-decremented in pmap_bootstrap to compensate */
	for (i = 0; i < btoc(MSGBUFSIZE); i++)
		pmap_enter(pmap_kernel(), (vm_offset_t)msgbufaddr + i * NBPG,
			avail_end + i * NBPG, VM_PROT_ALL, TRUE);
	initmsgbuf(msgbufaddr, round_page(MSGBUFSIZE));

	printf(version);
	identifycpu();
	printf("real mem  = %d\n", ctob(physmem));

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys((caddr_t)0);
	if ((v = (caddr_t)kmem_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
				   &maxaddr, size, TRUE);
	minaddr = (vm_offset_t)buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
			&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vm_size_t curbufsize;
		vm_offset_t curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base+1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16*NCARGS, TRUE);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE);

	/*
	 * Finally, allocate mbuf cluster submap.
	 */
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
	    VM_MBUF_SIZE, FALSE);

	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];

	printf("avail mem = %ld\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

#if NAPM > 0
	/*
	 * this should be caught at kernel build time, but put it here
	 * in case someone tries to fake it out...
	 */
#ifdef DIAGNOSTIC
	if (biostramp_image_size > NBPG)
	    panic("biostramp_image_size too big: %x vs. %x\n",
		  biostramp_image_size, NBPG);
#endif
	pmap_enter(pmap_kernel(),
		   (vm_offset_t)APM_BIOSTRAMP, /* virtual */
		   (vm_offset_t)APM_BIOSTRAMP, /* physical */
		   VM_PROT_ALL,		/* protection */
		   TRUE);		/* wired down */
	bcopy(biostramp_image, (caddr_t)APM_BIOSTRAMP, biostramp_image_size);
#ifdef DEBUG
	printf("biostramp installed @ %x\n", APM_BIOSTRAMP);
#endif
#endif
	/*
	 * Configure the system.
	 */
	ioport_malloc_safe = 1;
	configure();

	/*
	 * Set up proc0's TSS and LDT.
	 */
	curpcb = pcb = &proc0.p_addr->u_pcb;
	pcb->pcb_flags = 0;
	pcb->pcb_tss.tss_ioopt =
	    ((caddr_t)pcb->pcb_iomap - (caddr_t)&pcb->pcb_tss) << 16;
	for (x = 0; x < sizeof(pcb->pcb_iomap) / 4; x++)
		pcb->pcb_iomap[x] = 0xffffffff;

	pcb->pcb_ldt_sel = GSEL(GLDT_SEL, SEL_KPL);
	pcb->pcb_cr0 = rcr0();
	pcb->pcb_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	pcb->pcb_tss.tss_esp0 = (int)proc0.p_addr + USPACE - 16;
	tss_alloc(pcb);

	ltr(pcb->pcb_tss_sel);
	lldt(pcb->pcb_ldt_sel);

	proc0.p_md.md_regs = (struct trapframe *)pcb->pcb_tss.tss_esp0 - 1;

}

/*
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way we set each data structure pointer.
 *
 * We call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 */
caddr_t
allocsys(v)
	register caddr_t v;
{

#define	valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))
#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
	valloc(callout, struct callout, ncallout);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	/* This is pretty disgusting! */
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Determine how many buffers to allocate.  We use 10% of the
	 * first 2MB of memory, and 5% of the rest, with a minimum of 16
	 * buffers.  We allocate 1/2 as many swap buffer headers as file
	 * i/o buffers.
	 */
	if (bufpages == 0)
		if (physmem < btoc(2 * 1024 * 1024))
			bufpages = physmem / (10 * CLSIZE);
		else
			bufpages = (btoc(2 * 1024 * 1024) + physmem) /
			    (20 * CLSIZE);
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}

	/*
	 * XXX stopgap measure to prevent wasting too much KVM on
	 * the sparsely filled buffer cache.
	 */
	if (nbuf * MAXBSIZE > VM_MAX_KERNEL_BUF)
		nbuf = VM_MAX_KERNEL_BUF / MAXBSIZE;

	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);
	return v;
}

/*  
 * Info for CTL_HW
 */
char	cpu_model[120];
extern	char version[];

/*
 * Note: these are just the ones that may not have a cpuid instruction.
 * We deal with the rest in a different way.
 */
struct cpu_nocpuid_nameclass i386_nocpuid_cpus[] = {
	{ CPUVENDOR_INTEL, "Intel", "386SX",	CPUCLASS_386 },	/* CPU_386SX */
	{ CPUVENDOR_INTEL, "Intel", "386DX",	CPUCLASS_386 },	/* CPU_386   */
	{ CPUVENDOR_INTEL, "Intel", "486SX",	CPUCLASS_486 },	/* CPU_486SX */
	{ CPUVENDOR_INTEL, "Intel", "486DX",	CPUCLASS_486 },	/* CPU_486   */
	{ CPUVENDOR_CYRIX, "Cyrix", "486DLC",	CPUCLASS_486 },	/* CPU_486DLC */
	{ CPUVENDOR_NEXGEN,"NexGen","586",      CPUCLASS_386 }, /* CPU_NX586 */
};

const char *classnames[] = {
	"386",
	"486",
	"586",
	"686"
};

const char *modifiers[] = {
	"",
	"OverDrive ",
	"Dual ",
	""
};

struct cpu_cpuid_nameclass i386_cpuid_cpus[] = {
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
			}
		},
		/* Family 5 */
		{
			CPUCLASS_586,
			{
				0, "Pentium", "Pentium (P54C)",
				"Pentium (P24T)", "Pentium/MMX", "Pentium", 0,
				"Pentium (P54C)", 0, 0, 0, 0, 0, 0, 0, 0,
				"Pentium"	/* Default */
			}
		},
		/* Family 6 */
		{
			CPUCLASS_686,
			{
				0, "Pentium Pro", 0, "Pentium II",
				"Pentium Pro", 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0,
				"Pentium Pro"	/* Default */
			}
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
		},
		/* Family 5 */
		{
			CPUCLASS_586,
			{
				"K5", "K5", "K5", "K5", 0, 0, "K6",
				0, 0, 0, 0, 0, 0, 0, 0, 0,
				"K5 or K6",		/* Default */
			},
		},
		/* Family 6, not yet available from AMD */
		{
			CPUCLASS_686,
			{
				0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0,
				"Pentium Pro compatible"	/* Default */
			},
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
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				"486"		/* Default */
			},
		},
		/* Family 5 */
		{
			CPUCLASS_586,
			{
				0, 0, "6x86", 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0,
				"6x86"		/* Default */
			}
		},
		/* Family 6, not yet available from Cyrix */
		{
			CPUCLASS_686,
			{
				"M2", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				"M2"		/* Default */
			}
		} }
	}
};

#define CPUDEBUG

void
identifycpu()
{
	extern char cpu_vendor[];
	extern int cpu_id;
	const char *name, *modifier, *vendorname;
	int class = CPUCLASS_386, vendor, i, max;
	int family, model, step, modif;
	struct cpu_cpuid_nameclass *cpup = NULL;

	if (cpuid_level == -1) {
#ifdef DIAGNOSTIC
		if (cpu < 0 || cpu >=
		    (sizeof i386_nocpuid_cpus/sizeof(struct cpu_nocpuid_nameclass)))
			panic("unknown cpu type %d\n", cpu);
#endif
		name = i386_nocpuid_cpus[cpu].cpu_name;
		vendor = i386_nocpuid_cpus[cpu].cpu_vendor;
		vendorname = i386_nocpuid_cpus[cpu].cpu_vendorname;
		class = i386_nocpuid_cpus[cpu].cpu_class;
		modifier = "";
	} else {
		max = sizeof (i386_cpuid_cpus) / sizeof (i386_cpuid_cpus[0]);
		modif = (cpu_id >> 12) & 3;
		family = (cpu_id >> 8) & 15;
		if (family < CPU_MINFAMILY)
			panic("identifycpu: strange family value");
		model = (cpu_id >> 4) & 15;
		step = cpu_id & 15;
#ifdef CPUDEBUG
		printf("cpu0: family %x model %x step %x\n", family, model,
			step);
#endif

		for (i = 0; i < max; i++) {
			if (!strncmp(cpu_vendor,
			    i386_cpuid_cpus[i].cpu_id, 12)) {
				cpup = &i386_cpuid_cpus[i];
				break;
			}
		}

		if (cpup == NULL) {
			vendor = CPUVENDOR_UNKNOWN;
			if (cpu_vendor[0] != '\0')
				vendorname = &cpu_vendor[0];
			else
				vendorname = "Unknown";
			if (family > CPU_MAXFAMILY)
				family = CPU_MAXFAMILY;
			class = family - 3;
			modifier = "";
			name = "";
		} else {
			vendor = cpup->cpu_vendor;
			vendorname = cpup->cpu_vendorname;
			modifier = modifiers[modif];
			if (family > CPU_MAXFAMILY) {
				family = CPU_MAXFAMILY;
				model = CPU_DEFMODEL;
			} else if (model > CPU_MAXMODEL)
				model = CPU_DEFMODEL;
			i = family - CPU_MINFAMILY;
			name = cpup->cpu_family[i].cpu_models[model];
			if (name == NULL)
			    name = cpup->cpu_family[i].cpu_models[CPU_DEFMODEL];
			class = cpup->cpu_family[i].cpu_class;
		}
	}

	sprintf(cpu_model, "%s %s%s (%s-class)", vendorname, modifier, name,
		classnames[class]);
	printf("cpu0: %s\n", cpu_model);

	cpu_class = class;

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

#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	/*
	 * On a 486 or above, enable ring 0 write protection.
	 */
	if (cpu_class >= CPUCLASS_486)
		lcr0(rcr0() | CR0_WP);
#endif
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

	case CPU_BOOTED_KERNEL:
	        bibp = lookup_bootinfo(BTINFO_BOOTPATH);
	        if(!bibp)
			return(ENOENT); /* ??? */
		return (sysctl_rdstring(oldp, oldlenp, newp, bibp->bootpath));

	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

#ifdef COMPAT_IBCS2
void ibcs2_sendsig __P((sig_t, int, int, u_long));

void
ibcs2_sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	u_long code;
{
	extern int bsd_to_ibcs2_sig[];

	sendsig(catcher, bsd_to_ibcs2_sig[sig], mask, code);
}
#endif

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
	int sig, mask;
	u_long code;
{
	register struct proc *p = curproc;
	register struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack;
	extern char sigcode[], esigcode[];

	/* 
	 * Build the argument list for the signal handler.
	 */
	frame.sf_signum = sig;

	tf = p->p_md.md_regs;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate space for the signal handler context.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - sizeof(struct sigframe));
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else {
		fp = (struct sigframe *)tf->tf_esp - 1;
	}

	frame.sf_code = code;
	frame.sf_scp = &fp->sf_sc;
	frame.sf_handler = catcher;

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	frame.sf_sc.sc_err = tf->tf_err;
	frame.sf_sc.sc_trapno = tf->tf_trapno;
	frame.sf_sc.sc_onstack = oonstack;
	frame.sf_sc.sc_mask = mask;
#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		frame.sf_sc.sc_gs = tf->tf_vm86_gs;
		frame.sf_sc.sc_fs = tf->tf_vm86_fs;
		frame.sf_sc.sc_es = tf->tf_vm86_es;
		frame.sf_sc.sc_ds = tf->tf_vm86_ds;
		frame.sf_sc.sc_eflags = get_vflags(p);
	} else
#endif
	{
		__asm("movl %%gs,%w0" : "=r" (frame.sf_sc.sc_gs));
		__asm("movl %%fs,%w0" : "=r" (frame.sf_sc.sc_fs));
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
	__asm("movl %w0,%%gs" : : "r" (GSEL(GUDATA_SEL, SEL_UPL)));
	__asm("movl %w0,%%fs" : : "r" (GSEL(GUDATA_SEL, SEL_UPL)));
	tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_eip = (int)(((char *)PS_STRINGS) - (esigcode - sigcode));
	tf->tf_cs = GSEL(GUCODE_SEL, SEL_UPL);
	tf->tf_eflags &= ~(PSL_T|PSL_VM|PSL_AC);
	tf->tf_esp = (int)fp;
	tf->tf_ss = GSEL(GUDATA_SEL, SEL_UPL);
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
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext *scp, context;
	register struct trapframe *tf;

	tf = p->p_md.md_regs;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = SCARG(uap, sigcntxp);
	if (copyin((caddr_t)scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

	/*
	 * Restore signal context.
	 */
#ifdef VM86
	if (context.sc_eflags & PSL_VM) {
		tf->tf_vm86_gs = context.sc_gs;
		tf->tf_vm86_fs = context.sc_fs;
		tf->tf_vm86_es = context.sc_es;
		tf->tf_vm86_ds = context.sc_ds;
		set_vflags(p, context.sc_eflags);
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

		/* %fs and %gs were restored by the trampoline. */
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

	if (context.sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = context.sc_mask & ~sigcantmask;

	return (EJUSTRETURN);
}

int	waittime = -1;
struct pcb dumppcb;

void
cpu_reboot(howto, bootstr)
	int howto;
	char *bootstr;
{
	extern int cold;

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

	if (howto & RB_HALT) {
#if NAPM > 0 && !defined(APM_NO_POWEROFF)
		/* turn off, if we can.  But try to turn disk off and
		 * wait a bit first--some disk drives are slow to clean up
		 * and users have reported disk corruption.
		 */
		delay(500000);
		apm_set_powstate(APM_DEV_DISK(0xff), APM_SYS_OFF);
		delay(500000);
		apm_set_powstate(APM_DEV_ALLDEVS, APM_SYS_OFF);
#endif
		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cngetc();
	}

	printf("rebooting...\n");
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
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
cpu_dumpconf()
{
	int nblks;	/* size of dump area */
	int maj;

	if (dumpdev == NODEV)
		return;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		return;

	dumpsize = btoc(IOM_END + ctob(dumpmem_high));

	/* Always skip the first CLBYTES, in case there is a label there. */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
#define BYTES_PER_DUMP  NBPG	/* must be a multiple of pagesize XXX small */
static vm_offset_t dumpspace;

vm_offset_t
reserve_dumppages(p)
	vm_offset_t p;
{

	dumpspace = p;
	return (p + BYTES_PER_DUMP);
}

void
dumpsys()
{
	unsigned bytes, i, n;
	int maddr, psize;
	daddr_t blkno;
	int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	int error;

	/* Save registers. */
	savectx(&dumppcb);

	msgbufmapped = 0;	/* don't record dump msgs in msgbuf */
	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();
	if (dumplo < 0)
		return;
	printf("\ndumping to dev %x, offset %ld\n", dumpdev, dumplo);

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

	bytes = ctob(dumpmem_high) + IOM_END;
	maddr = 0;
	blkno = dumplo;
	dump = bdevsw[major(dumpdev)].d_dump;
	error = 0;
	for (i = 0; i < bytes; i += n) {
		/*
		 * Avoid dumping the ISA memory hole, and areas that
		 * BIOS claims aren't in low memory.
		 */
		if (i >= ctob(dumpmem_low) && i < IOM_END) {
			n = IOM_END - i;
			maddr += n;
			blkno += btodb(n);
			continue;
		}

		/* Print out how many MBs we to go. */
		n = bytes - i;
		if (n && (n % (1024*1024)) == 0)
			printf("%d ", n / (1024 * 1024));

		/* Limit size for next transfer. */
		if (n > BYTES_PER_DUMP)
			n =  BYTES_PER_DUMP;

		(void) pmap_map(dumpspace, maddr, maddr + n, VM_PROT_READ);
		error = (*dump)(dumpdev, blkno, (caddr_t)dumpspace, n);
		if (error)
			break;
		maddr += n;
		blkno += btodb(n);			/* XXX? */

#if 0	/* XXX this doesn't work.  grr. */
		/* operator aborting dump? */
		if (sget() != NULL) {
			error = EINTR;
			break;
		}
#endif
	}

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
	register struct pcb *pcb = &p->p_addr->u_pcb;
	register struct trapframe *tf;

#if NNPX > 0
	/* If we were using the FPU, forget about it. */
	if (npxproc == p)
		npxdrop();
#endif

#ifdef USER_LDT
	if (pcb->pcb_flags & PCB_USER_LDT)
		i386_user_cleanup(pcb);
#endif

	p->p_md.md_flags &= ~MDP_USEDFPU;
	pcb->pcb_flags = 0;

	tf = p->p_md.md_regs;
	__asm("movl %w0,%%gs" : : "r" (LSEL(LUDATA_SEL, SEL_UPL)));
	__asm("movl %w0,%%fs" : : "r" (LSEL(LUDATA_SEL, SEL_UPL)));
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

union descriptor gdt[NGDT];
union descriptor ldt[NLDT];
struct gate_descriptor idt[NIDT];

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
extern	IDTVEC(syscall), IDTVEC(osyscall);
extern	*IDTVEC(exceptions)[];

void
init386(first_avail)
	vm_offset_t first_avail;
{
	int x;
	struct region_descriptor region;
	extern void consinit __P((void));

	proc0.p_addr = proc0paddr;

	/*
	 * Initialize the I/O port and I/O mem extent maps.
	 * Note: we don't have to check the return value since
	 * creation of a fixed extent map will never fail (since
	 * descriptor storage has already been allocated).
	 *
	 * N.B. The iomem extent manages _all_ physical addresses
	 * on the machine.  When the amount of RAM is found, the two
	 * extents of RAM are allocated from the map (0 -> ISA hole
	 * and end of ISA hole -> end of RAM).
	 */
	ioport_ex = extent_create("ioport", 0x0, 0xffff, M_DEVBUF,
	    (caddr_t)ioport_ex_storage, sizeof(ioport_ex_storage),
	    EX_NOCOALESCE|EX_NOWAIT);
	iomem_ex = extent_create("iomem", 0x0, 0xffffffff, M_DEVBUF,
	    (caddr_t)iomem_ex_storage, sizeof(iomem_ex_storage),
	    EX_NOCOALESCE|EX_NOWAIT);

	consinit();	/* XXX SHOULD NOT BE DONE HERE */

	/* make gdt gates and memory segments */
	setsegment(&gdt[GCODE_SEL].sd, 0, 0xfffff, SDT_MEMERA, SEL_KPL, 1, 1);
	setsegment(&gdt[GDATA_SEL].sd, 0, 0xfffff, SDT_MEMRWA, SEL_KPL, 1, 1);
	setsegment(&gdt[GLDT_SEL].sd, ldt, sizeof(ldt) - 1, SDT_SYSLDT, SEL_KPL,
	    0, 0);
	setsegment(&gdt[GUCODE_SEL].sd, 0, i386_btop(VM_MAXUSER_ADDRESS) - 1,
	    SDT_MEMERA, SEL_UPL, 1, 1);
	setsegment(&gdt[GUDATA_SEL].sd, 0, i386_btop(VM_MAXUSER_ADDRESS) - 1,
	    SDT_MEMRWA, SEL_UPL, 1, 1);
	/* bios trampoline GDT entries */
	setsegment(&gdt[GBIOSCODE_SEL].sd, 0, 0xfffff, SDT_MEMERA, SEL_KPL, 0, 0);
	setsegment(&gdt[GBIOSDATA_SEL].sd, 0, 0xfffff, SDT_MEMRWA, SEL_KPL, 0, 0);

	/* make ldt gates and memory segments */
	setgate(&ldt[LSYS5CALLS_SEL].gd, &IDTVEC(osyscall), 1, SDT_SYS386CGT,
	    SEL_UPL);
	ldt[LUCODE_SEL] = gdt[GUCODE_SEL];
	ldt[LUDATA_SEL] = gdt[GUDATA_SEL];
	ldt[LBSDICALLS_SEL] = ldt[LSYS5CALLS_SEL];

	/* exceptions */
	for (x = 0; x < 32; x++)
		setgate(&idt[x], IDTVEC(exceptions)[x], 0, SDT_SYS386TGT,
		    x == 3 ? SEL_UPL : SEL_KPL);

	/* new-style interrupt gate for syscalls */
	setgate(&idt[128], &IDTVEC(syscall), 0, SDT_SYS386TGT, SEL_UPL);

	setregion(&region, gdt, sizeof(gdt) - 1);
	lgdt(&region);
	setregion(&region, idt, sizeof(idt) - 1);
	lidt(&region);

#if NISA > 0
	isa_defaultirq();
#endif

	splraise(-1);
	enable_intr();

	/*
	 * Use BIOS values passed in from the boot program.
	 *
	 * XXX Not only does probing break certain 386 AT relics, but
	 * not all BIOSes (Dell, Compaq, others) report the correct
	 * amount of extended memory.
	 */
	avail_end = biosextmem ? IOM_END + biosextmem * 1024
	    : biosbasemem * 1024;	/* just temporary use */

	/*
	 * Allocate the physical addresses used by RAM from the iomem
	 * extent map.  This is done before the addresses are
	 * page rounded just to make sure we get them all.
	 */
	if (extent_alloc_region(iomem_ex, 0, IOM_BEGIN, EX_NOWAIT)) {
		/* XXX What should we do? */
		printf("WARNING: CAN'T ALLOCATE BASE RAM FROM IOMEM EXTENT MAP!\n");
	}
	if (avail_end > IOM_END && extent_alloc_region(iomem_ex, IOM_END,
	    (avail_end - IOM_END), EX_NOWAIT)) {
		/* XXX What should we do? */
		printf("WARNING: CAN'T ALLOCATE EXTENDED MEMORY FROM IOMEM EXTENT MAP!\n");
	}

	/* Round down to whole pages. */
	biosbasemem &= -(NBPG / 1024);
	biosextmem &= -(NBPG / 1024);

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
		printf("Warning: ignoring %dk of remapped memory\n",
		    biosextmem - (15*1024));
		biosextmem = (15*1024);
	}
#endif

#if NAPM > 0
	avail_start = 2*NBPG;	/* save us a page! */
#else
	avail_start = NBPG;	/* BIOS leaves data in low memory */
				/* and VM system doesn't work with phys 0 */
#endif
	avail_end = biosextmem ? IOM_END + biosextmem * 1024
	    : biosbasemem * 1024;

	/* number of pages of physmem addr space */
	physmem = btoc((biosbasemem + biosextmem) * 1024);
	dumpmem_low = btoc(biosbasemem * 1024);
	dumpmem_high = btoc(biosextmem * 1024);

	/*
	 * Initialize for pmap_free_pages and pmap_next_page.
	 * These guys should be page-aligned.
	 */
	hole_start = biosbasemem * 1024;
	/* we load right after the I/O hole; adjust hole_end to compensate */
	hole_end = round_page(first_avail);
	avail_next = avail_start;

	if (physmem < btoc(2 * 1024 * 1024)) {
		printf("warning: too little memory available; "
		       "have %d bytes, want %d bytes\n"
		       "running in degraded mode\n"
		       "press a key to confirm\n\n",
		       ctob(physmem), 2*1024*1024);
		cngetc();
	}

	/* call pmap initialization to make new kernel address space */
	pmap_bootstrap((vm_offset_t)atdevbase + IOM_SIZE);

#ifdef DDB
	ddb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif
#ifdef KGDB
	kgdb_port_init();
	if (boothowto & RB_KDB) {
		kgdb_debug_init = 1;
		kgdb_connect(1);
	}
#endif
}

struct queue {
	struct queue *q_next, *q_prev;
};

/*
 * insert an element into a queue
 */
void
_insque(v1, v2)
	void *v1;
	void *v2;
{
	register struct queue *elem = v1, *head = v2;
	register struct queue *next;

	next = head->q_next;
	elem->q_next = next;
	head->q_next = elem;
	elem->q_prev = head;
	next->q_prev = elem;
}

/*
 * remove an element from a queue
 */
void
_remque(v)
	void *v;
{
	register struct queue *elem = v;
	register struct queue *next, *prev;

	next = elem->q_next;
	prev = elem->q_prev;
	next->q_prev = prev;
	prev->q_next = next;
	elem->q_prev = 0;
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

u_int
pmap_free_pages()
{

	if (avail_next <= hole_start)
		return ((hole_start - avail_next) / NBPG +
			(avail_end - hole_end) / NBPG);
	else
		return ((avail_end - avail_next) / NBPG);
}

int
pmap_next_page(addrp)
	vm_offset_t *addrp;
{

	if (avail_next + NBPG > avail_end)
		return FALSE;

	if (avail_next + NBPG > hole_start && avail_next < hole_end)
		avail_next = hole_end;

	*addrp = avail_next;
	avail_next += NBPG;
	return TRUE;
}

int
pmap_page_index(pa)
	vm_offset_t pa;
{

	if (pa >= avail_start && pa < hole_start)
		return i386_btop(pa - avail_start);
	if (pa >= hole_end && pa < avail_end)
		return i386_btop(pa - hole_end + hole_start - avail_start);
	return -1;
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

/*
 * consinit:
 * initialize the system console.
 * XXX - shouldn't deal with this initted thing, but then,
 * it shouldn't be called from init386 either.
 */
void
consinit()
{
	struct btinfo_console *consinfo;
	static int initted;

	if (initted)
		return;
	initted = 1;

#ifndef CONS_OVERRIDE
	consinfo = lookup_bootinfo(BTINFO_CONSOLE);
	if(!consinfo)
#endif
		consinfo = &default_consinfo;

#if (NPC > 0) || (NVT > 0)
	if(!strcmp(consinfo->devname, "pc")) {
		pccnattach();
		return;
	}
#endif
#if (NCOM > 0)
	if(!strcmp(consinfo->devname, "com")) {
		bus_space_tag_t tag = I386_BUS_SPACE_IO;

		if(comcnattach(tag, consinfo->addr, consinfo->speed,
			       COM_FREQ, CONMODE))
			panic("can't init serial console @%x", consinfo->addr);

		return;
	}
#endif
	panic("invalid console device %s", consinfo->devname);
}

#ifdef KGDB
void
kgdb_port_init()
{
#if (NCOM > 0)
	if(!strcmp(kgdb_devname, "com")) {
		bus_space_tag_t tag = I386_BUS_SPACE_IO;

		com_kgdb_attach(tag, comkgdbaddr, comkgdbrate, COM_FREQ, 
		    comkgdbmode);
	}
#endif
}
#endif

void
cpu_reset()
{
	struct region_descriptor region;

	disable_intr();

	/*
	 * The keyboard controller has 4 random output pins, one of which is
	 * connected to the RESET pin on the CPU in many PCs.  We tell the
	 * keyboard controller to pulse this line a couple of times.
	 */
	outb(KBCMDP, KBC_PULSE0);
	delay(100000);
	outb(KBCMDP, KBC_PULSE0);
	delay(100000);

	/*
	 * Try to cause a triple fault and watchdog reset by making the IDT
	 * invalid and causing a fault.
	 */
	bzero((caddr_t)idt, sizeof(idt));
	setregion(&region, idt, sizeof(idt) - 1);
	lidt(&region);
	__asm __volatile("divl %0,%1" : : "q" (0), "a" (0)); 

#if 0
	/*
	 * Try to cause a triple fault and watchdog reset by unmapping the
	 * entire address space and doing a TLB flush.
	 */
	bzero((caddr_t)PTD, NBPG);
	pmap_update(); 
#endif

	for (;;);
}

int
i386_memio_map(t, bpa, size, flags, bshp)
	bus_space_tag_t t;
	bus_addr_t bpa;
	bus_size_t size;
	int flags;
	bus_space_handle_t *bshp;
{
	int error;
	struct extent *ex;

	/*
	 * Pick the appropriate extent map.
	 */
	if (t == I386_BUS_SPACE_IO) {
		if (flags & BUS_SPACE_MAP_LINEAR)
			return (EOPNOTSUPP);
		ex = ioport_ex;
	} else if (t == I386_BUS_SPACE_MEM)
		ex = iomem_ex;
	else
		panic("i386_memio_map: bad bus space tag");

	/*
	 * Before we go any further, let's make sure that this
	 * region is available.
	 */
	error = extent_alloc_region(ex, bpa, size,
	    EX_NOWAIT | (ioport_malloc_safe ? EX_MALLOCOK : 0));
	if (error)
		return (error);

	/*
	 * For I/O space, that's all she wrote.
	 */
	if (t == I386_BUS_SPACE_IO) {
		*bshp = bpa;
		return (0);
	}

	/*
	 * For memory space, map the bus physical address to
	 * a kernel virtual address.
	 */
	error = i386_mem_add_mapping(bpa, size,
		(flags & BUS_SPACE_MAP_CACHEABLE) != 0, bshp);
	if (error) {
		if (extent_free(ex, bpa, size, EX_NOWAIT |
		    (ioport_malloc_safe ? EX_MALLOCOK : 0))) {
			printf("i386_memio_map: pa 0x%lx, size 0x%lx\n",
			    bpa, size);
			printf("i386_memio_map: can't free region\n");
		}
	}

	return (error);
}

int
_i386_memio_map(t, bpa, size, flags, bshp)
	bus_space_tag_t t;
	bus_addr_t bpa;
	bus_size_t size;
	int flags;
	bus_space_handle_t *bshp;
{

	/*
	 * For I/O space, just fill in the handle.
	 */
	if (t == I386_BUS_SPACE_IO) {
		if (flags & BUS_SPACE_MAP_LINEAR)
			return (EOPNOTSUPP);
		*bshp = bpa;
		return (0);
	}

	/*
	 * For memory space, map the bus physical address to
	 * a kernel virtual address.
	 */
	return (i386_mem_add_mapping(bpa, size,
	    (flags & BUS_SPACE_MAP_CACHEABLE) != 0, bshp));
}

int
i386_memio_alloc(t, rstart, rend, size, alignment, boundary, flags,
    bpap, bshp)
	bus_space_tag_t t;
	bus_addr_t rstart, rend;
	bus_size_t size, alignment, boundary;
	int flags;
	bus_addr_t *bpap;
	bus_space_handle_t *bshp;
{
	struct extent *ex;
	u_long bpa;
	int error;

	/*
	 * Pick the appropriate extent map.
	 */
	if (t == I386_BUS_SPACE_IO) {
		if (flags & BUS_SPACE_MAP_LINEAR)
			return (EOPNOTSUPP);
		ex = ioport_ex;
	} else if (t == I386_BUS_SPACE_MEM)
		ex = iomem_ex;
	else
		panic("i386_memio_alloc: bad bus space tag");

	/*
	 * Sanity check the allocation against the extent's boundaries.
	 */
	if (rstart < ex->ex_start || rend > ex->ex_end)
		panic("i386_memio_alloc: bad region start/end");

	/*
	 * Do the requested allocation.
	 */
	error = extent_alloc_subregion(ex, rstart, rend, size, alignment,
	    boundary, EX_NOWAIT | (ioport_malloc_safe ?  EX_MALLOCOK : 0),
	    &bpa);

	if (error)
		return (error);

	/*
	 * For I/O space, that's all she wrote.
	 */
	if (t == I386_BUS_SPACE_IO) {
		*bshp = *bpap = bpa;
		return (0);
	}

	/*
	 * For memory space, map the bus physical address to
	 * a kernel virtual address.
	 */
	error = i386_mem_add_mapping(bpa, size,
	    (flags & BUS_SPACE_MAP_CACHEABLE) != 0, bshp);
	if (error) {
		if (extent_free(iomem_ex, bpa, size, EX_NOWAIT |
		    (ioport_malloc_safe ? EX_MALLOCOK : 0))) {
			printf("i386_memio_alloc: pa 0x%lx, size 0x%lx\n",
			    bpa, size);
			printf("i386_memio_alloc: can't free region\n");
		}
	}

	*bpap = bpa;

	return (error);
}

int
i386_mem_add_mapping(bpa, size, cacheable, bshp)
	bus_addr_t bpa;
	bus_size_t size;
	int cacheable;
	bus_space_handle_t *bshp;
{
	u_long pa, endpa;
	vm_offset_t va;

	pa = i386_trunc_page(bpa);
	endpa = i386_round_page(bpa + size);

#ifdef DIAGNOSTIC
	if (endpa <= pa)
		panic("i386_mem_add_mapping: overflow");
#endif

	va = kmem_alloc_pageable(kernel_map, endpa - pa);
	if (va == 0)
		return (ENOMEM);

	*bshp = (bus_space_handle_t)(va + (bpa & PGOFSET));

	for (; pa < endpa; pa += NBPG, va += NBPG) {
		pmap_enter(pmap_kernel(), va, pa,
		    VM_PROT_READ | VM_PROT_WRITE, TRUE);
#if 0
		/* 
		 * Not done for two reasons:
		 *
		 *	(1) PG_N doesn't exist on the 386.
		 *
		 *	(2) pmap_changebit() only deals with
		 *	    managed pages.
		 */
		if (!cacheable)
			pmap_changebit(pa, PG_N, ~0);
		else
			pmap_changebit(pa, 0, ~PG_N);
#endif
	}
 
	return 0;
}

void
i386_memio_unmap(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	struct extent *ex;
	u_long va, endva;
	bus_addr_t bpa;

	/*
	 * Find the correct extent and bus physical address.
	 */
	if (t == I386_BUS_SPACE_IO) {
		ex = ioport_ex;
		bpa = bsh;
	} else if (t == I386_BUS_SPACE_MEM) {
		ex = iomem_ex;
		va = i386_trunc_page(bsh);
		endva = i386_round_page(bsh + size);

#ifdef DIAGNOSTIC
		if (endva <= va)
			panic("i386_memio_unmap: overflow");
#endif

		bpa = pmap_extract(pmap_kernel(), va) + (bsh & PGOFSET);

		/*
		 * Free the kernel virtual mapping.
		 */
		kmem_free(kernel_map, va, endva - va);
	} else
		panic("i386_memio_unmap: bad bus space tag");

	if (extent_free(ex, bpa, size,
	    EX_NOWAIT | (ioport_malloc_safe ? EX_MALLOCOK : 0))) {
		printf("i386_memio_unmap: %s 0x%lx, size 0x%lx\n",
		    (t == I386_BUS_SPACE_IO) ? "port" : "pa", bpa, size);
		printf("i386_memio_unmap: can't free region\n");
	}
}

void    
i386_memio_free(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	/* i386_memio_unmap() does all that we need to do. */
	i386_memio_unmap(t, bsh, size);
}

int
i386_memio_subregion(t, bsh, offset, size, nbshp)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t offset, size;
	bus_space_handle_t *nbshp;
{

	*nbshp = bsh + offset;
	return (0);
}

/*
 * Common function for DMA map creation.  May be called by bus-specific
 * DMA map creation functions.
 */
int
_bus_dmamap_create(t, size, nsegments, maxsegsz, boundary, flags, dmamp)
	bus_dma_tag_t t;
	bus_size_t size;
	int nsegments;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	struct i386_bus_dmamap *map;
	void *mapstore;
	size_t mapsize;

	/*
	 * Allocate and initialize the DMA map.  The end of the map
	 * is a variable-sized array of segments, so we allocate enough
	 * room for them in one shot.
	 *
	 * Note we don't preserve the WAITOK or NOWAIT flags.  Preservation
	 * of ALLOCNOW notifies others that we've reserved these resources,
	 * and they are not to be freed.
	 *
	 * The bus_dmamap_t includes one bus_dma_segment_t, hence
	 * the (nsegments - 1).
	 */
	mapsize = sizeof(struct i386_bus_dmamap) +
	    (sizeof(bus_dma_segment_t) * (nsegments - 1));
	if ((mapstore = malloc(mapsize, M_DEVBUF,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL)
		return (ENOMEM);

	bzero(mapstore, mapsize);
	map = (struct i386_bus_dmamap *)mapstore;
	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxsegsz = maxsegsz;
	map->_dm_boundary = boundary;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK|BUS_DMA_NOWAIT);
	map->dm_nsegs = 0;		/* no valid mappings */

	*dmamp = map;
	return (0);
}

/*
 * Common function for DMA map destruction.  May be called by bus-specific
 * DMA map destruction functions.
 */
void
_bus_dmamap_destroy(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{

	free(map, M_DEVBUF);
}

/*
 * Common function for loading a DMA map with a linear buffer.  May
 * be called by bus-specific DMA map load functions.
 */
int
_bus_dmamap_load(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	bus_size_t sgsize;
	bus_addr_t curaddr, lastaddr;
	caddr_t vaddr = buf;
	int first, seg;
	pmap_t pmap;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;

	if (buflen > map->_dm_size)
		return (EINVAL);

	/*
	 * XXX Need to implement "don't dma across this boundry".
	 */

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	lastaddr = ~0;		/* XXX gcc */
	for (first = 1, seg = 0; buflen > 0 && seg < map->_dm_segcnt; ) {
		/*
		 * Get the physical address for this segment.
		 */
		curaddr = (bus_addr_t)pmap_extract(pmap, (vm_offset_t)vaddr);

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = NBPG - ((u_long)vaddr & PGOFSET);
		if (buflen < sgsize)
			sgsize = buflen;

		/*
		 * Insert chunk into a segment, coalescing with
		 * previous segment if possible.
		 */
		if (first) {
			map->dm_segs[seg].ds_addr = curaddr;
			map->dm_segs[seg].ds_len = sgsize;
			first = 0;
		} else {
			if (curaddr == lastaddr &&
			    (map->dm_segs[seg].ds_len + sgsize) <=
			     map->_dm_maxsegsz)
				map->dm_segs[seg].ds_len += sgsize;
			else {
				seg++;
				map->dm_segs[seg].ds_addr = curaddr;
				map->dm_segs[seg].ds_len = sgsize;
			}
		}

		lastaddr = curaddr + sgsize;
		vaddr += sgsize;
		buflen -= sgsize;
	}

	/*
	 * Did we fit?
	 */
	if (buflen != 0)
		return (EFBIG);		/* XXX better return value here? */

	map->dm_nsegs = seg + 1;
	return (0);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
int
_bus_dmamap_load_mbuf(t, map, m, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{

	panic("_bus_dmamap_load: not implemented");
}

/*
 * Like _bus_dmamap_load(), but for uios.
 */
int
_bus_dmamap_load_uio(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{

	panic("_bus_dmamap_load_uio: not implemented");
}

/*
 * Like _bus_dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
_bus_dmamap_load_raw(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{

	panic("_bus_dmamap_load_raw: not implemented");
}

/*
 * Common function for unloading a DMA map.  May be called by
 * bus-specific DMA map unload functions.
 */
void
_bus_dmamap_unload(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{

	/*
	 * No resources to free; just mark the mappings as
	 * invalid.
	 */
	map->dm_nsegs = 0;
}

/*
 * Common function for DMA map synchronization.  May be called
 * by bus-specific DMA map synchronization functions.
 */
void
_bus_dmamap_sync(t, map, op)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dmasync_op_t op;
{

	/* Nothing to do here. */
}

/*
 * Common function for DMA-safe memory allocation.  May be called
 * by bus-specific DMA memory allocation functions.
 */
int
_bus_dmamem_alloc(t, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{

	return (_bus_dmamem_alloc_range(t, size, alignment, boundary,
	    segs, nsegs, rsegs, flags, 0, trunc_page(avail_end)));
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
void
_bus_dmamem_free(t, segs, nsegs)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
{
	vm_page_t m;
	bus_addr_t addr;
	struct pglist mlist;
	int curseg;

	/*
	 * Build a list of pages to free back to the VM system.
	 */
	TAILQ_INIT(&mlist);
	for (curseg = 0; curseg < nsegs; curseg++) {
		for (addr = segs[curseg].ds_addr;
		    addr < (segs[curseg].ds_addr + segs[curseg].ds_len);
		    addr += PAGE_SIZE) {
			m = PHYS_TO_VM_PAGE(addr);
			TAILQ_INSERT_TAIL(&mlist, m, pageq);
		}
	}

	vm_page_free_memory(&mlist);
}

/*
 * Common function for mapping DMA-safe memory.  May be called by
 * bus-specific DMA memory map functions.
 */
int
_bus_dmamem_map(t, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	vm_offset_t va;
	bus_addr_t addr;
	int curseg, s;

	size = round_page(size);

	s = splimp();
	va = kmem_alloc_pageable(kmem_map, size);
	splx(s);

	if (va == 0)
		return (ENOMEM);

	*kvap = (caddr_t)va;

	for (curseg = 0; curseg < nsegs; curseg++) {
		for (addr = segs[curseg].ds_addr;
		    addr < (segs[curseg].ds_addr + segs[curseg].ds_len);
		    addr += NBPG, va += NBPG, size -= NBPG) {
			if (size == 0)
				panic("_bus_dmamem_map: size botch");
			pmap_enter(pmap_kernel(), va, addr,
			    VM_PROT_READ | VM_PROT_WRITE, TRUE);
#if 0
			/*
			 * This is not necessary on x86-family
			 * processors.
			 */
			if (flags & BUS_DMAMEM_NOSYNC)
				pmap_changebit(addr, PG_N, ~0);
			else
				pmap_changebit(addr, 0, ~PG_N);
#endif
		}
	}

	return (0);
}

/*
 * Common function for unmapping DMA-safe memory.  May be called by
 * bus-specific DMA memory unmapping functions.
 */
void
_bus_dmamem_unmap(t, kva, size)
	bus_dma_tag_t t;
	caddr_t kva;
	size_t size;
{
	int s;

#ifdef DIAGNOSTIC
	if ((u_long)kva & PGOFSET)
		panic("_bus_dmamem_unmap");
#endif

	size = round_page(size);
	s = splimp();
	kmem_free(kmem_map, (vm_offset_t)kva, size);
	splx(s);
}

/*
 * Common functin for mmap(2)'ing DMA-safe memory.  May be called by
 * bus-specific DMA mmap(2)'ing functions.
 */
int
_bus_dmamem_mmap(t, segs, nsegs, off, prot, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs, off, prot, flags;
{
	int i;

	for (i = 0; i < nsegs; i++) {
#ifdef DIAGNOSTIC
		if (off & PGOFSET)
			panic("_bus_dmamem_mmap: offset unaligned");
		if (segs[i].ds_addr & PGOFSET)
			panic("_bus_dmamem_mmap: segment unaligned");
		if (segs[i].ds_len & PGOFSET)
			panic("_bus_dmamem_mmap: segment size not multiple"
			    " of page size");
#endif
		if (off >= segs[i].ds_len) {
			off -= segs[i].ds_len;
			continue;
		}

		return (i386_btop((caddr_t)segs[i].ds_addr + off));
	}

	/* Page not found. */
	return (-1);
}

/**********************************************************************
 * DMA utility functions
 **********************************************************************/

/*
 * Allocate physical memory from the given physical address range.
 * Called by DMA-safe memory allocation methods.
 */
int
_bus_dmamem_alloc_range(t, size, alignment, boundary, segs, nsegs, rsegs,
    flags, low, high)
	bus_dma_tag_t t;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
	vm_offset_t low;
	vm_offset_t high;
{
	vm_offset_t curaddr, lastaddr;
	vm_page_t m;
	struct pglist mlist;
	int curseg, error;

	/* Always round the size. */
	size = round_page(size);

	/*
	 * Allocate pages from the VM system.
	 */
	TAILQ_INIT(&mlist);
	error = vm_page_alloc_memory(size, low, high,
	    alignment, boundary, &mlist, nsegs, (flags & BUS_DMA_NOWAIT) == 0);
	if (error)
		return (error);

	/*
	 * Compute the location, size, and number of segments actually
	 * returned by the VM code.
	 */
	m = mlist.tqh_first;
	curseg = 0;
	lastaddr = segs[curseg].ds_addr = VM_PAGE_TO_PHYS(m);
	segs[curseg].ds_len = PAGE_SIZE;
	m = m->pageq.tqe_next;

	for (; m != NULL; m = m->pageq.tqe_next) {
		curaddr = VM_PAGE_TO_PHYS(m);
#ifdef DIAGNOSTIC
		if (curaddr < low || curaddr >= high) {
			printf("vm_page_alloc_memory returned non-sensical"
			    " address 0x%lx\n", curaddr);
			panic("_bus_dmamem_alloc_range");
		}
#endif
		if (curaddr == (lastaddr + PAGE_SIZE))
			segs[curseg].ds_len += PAGE_SIZE;
		else {
			curseg++;
			segs[curseg].ds_addr = curaddr;
			segs[curseg].ds_len = PAGE_SIZE;
		}
		lastaddr = curaddr;
	}

	*rsegs = curseg + 1;

	return (0);
}
