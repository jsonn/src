/* $NetBSD: machdep.c,v 1.2.2.3 2002/08/27 23:44:29 nathanw Exp $ */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, The Mach Operating System project at
 * Carnegie-Mellon University and Ralph Campbell.
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
 *	@(#)machdep.c	8.3 (Berkeley) 1/12/94
 * 	from: Utah Hdr: machdep.c 1.63 91/04/24
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: machdep.c,v 1.2.2.3 2002/08/27 23:44:29 nathanw Exp $");

#include "opt_ddb.h"
#include "opt_kgdb.h"

#include "opt_memsize.h"
#include "opt_ethaddr.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/user.h>
#include <sys/mount.h>
#include <sys/kcore.h>
#include <sys/boot_flag.h>
#include <sys/termios.h>

#include <net/if.h>
#include <net/if_ether.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#endif

#include <mips/cache.h>
#include <mips/locore.h>
#include <machine/yamon.h>

#include <evbmips/alchemy/pb1000var.h>
#include <mips/alchemy/include/aureg.h>
#include <mips/alchemy/include/auvar.h>
#include <mips/alchemy/include/aubusvar.h>

#include "aucom.h"
#if NAUCOM > 0
#include <mips/alchemy/dev/aucomvar.h>

#ifndef CONSPEED
#define CONSPEED TTYDEF_SPEED
#endif
int	aucomcnrate = CONSPEED;
#endif /* NAUCOM > 0 */

/* The following are used externally (sysctl_hw). */
extern char	cpu_model[];

struct	user *proc0paddr;

/* Our exported CPU info; we can have only one. */  
struct cpu_info cpu_info_store;

/* Maps for VM objects. */
struct vm_map *exec_map = NULL;
struct vm_map *mb_map = NULL;
struct vm_map *phys_map = NULL;

int physmem;		/* # pages of physical memory */
int maxmem;			/* max memory per process */

int mem_cluster_cnt;
phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];

yamon_env_var *yamon_envp;
struct pb1000_config pb1000_configuration;
struct propdb *alchemy_prop_info;

void	mach_init(int, char **, yamon_env_var *, u_long); /* XXX */

void
mach_init(int argc, char **argv, yamon_env_var *envp, u_long memsize)
{
	struct pb1000_config *pbc = &pb1000_configuration;
	bus_space_handle_t sh;
	caddr_t kernend;
	const char *cp;
	u_long first, last;
	caddr_t v;
	int howto, i;

	extern char edata[], end[];	/* XXX */

	/* clear the BSS segment */
	kernend = (caddr_t)mips_round_page(end);
	memset(edata, 0, kernend - (caddr_t)edata);

	/* set cpu model info for sysctl_hw */
	strcpy(cpu_model, "Alchemy Semiconductor Pb1000");

	/* save the yamon environment pointer */
	yamon_envp = envp;

	/* Use YAMON callbacks for early console I/O */
	cn_tab = &yamon_promcd;

	/*
	 * Set up the exception vectors and cpu-specific function
	 * vectors early on.  We need the wbflush() vector set up
	 * before comcnattach() is called (or at least before the
	 * first printf() after that is called).
	 * Also clears the I+D caches.
	 */
	mips_vector_init();

	/*
	 * Set the VM page size.
	 */
	uvm_setpagesize();

	/*
	 * Initialize bus space tags.
	 */
	au_cpureg_bus_mem_init(&pbc->pc_cpuregt, pbc);
	aubus_st = &pbc->pc_cpuregt;		/* XXX: for aubus.c */

	/*
	 * Calibrate the timer, delay() relies on this.
	 */
	bus_space_map(&pbc->pc_cpuregt, PC_BASE, PC_SIZE, 0, &sh);
	au_cal_timers(&pbc->pc_cpuregt, sh);
	bus_space_unmap(&pbc->pc_cpuregt, sh, PC_SIZE);

	/*
	 * Bring up the console.
	 */
#if NAUCOM > 0
	/*
	 * Delay to allow firmware putchars to complete.
	 * FIFO depth * character time.
	 * character time = (1000000 / (defaultrate / 10))
	 */
	delay(160000000 / aucomcnrate);
	if (aucomcnattach(&pbc->pc_cpuregt, UART0_BASE, aucomcnrate,
	    curcpu()->ci_cpu_freq / 4,
	    (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8) != 0)
		panic("pb1000: unable to initialize serial console");
#else
	panic("pb1000: not configured to use serial console");
#endif /* NAUCOM > 0 */

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
	boothowto = RB_AUTOBOOT;
#ifdef KADB
	boothowto |= RB_KDB;
#endif
	for (i = 1; i < argc; i++) {
		for (cp = argv[i]; *cp; cp++) {
			/* Ignore superfluous '-', if there is one */
			if (*cp == '-')
				continue;

			howto = 0;
			BOOT_FLAG(*cp, howto);
			if (! howto)
				printf("bootflag '%c' not recognised\n", *cp);
			else
				boothowto |= howto;
		}
	}

	/*
	 * Determine the memory size.  Use the `memsize' PMON
	 * variable.  If that's not available, panic.
	 *
	 * Note: Reserve the first page!  That's where the trap
	 * vectors are located.
	 */

#if defined(MEMSIZE)
	size = MEMSIZE;
#else
	if (memsize == 0) {
		if ((cp = yamon_getenv("memsize")) != NULL)
			memsize = strtoul(cp, NULL, 0);
		else {
			printf("FATAL: `memsize' YAMON variable not set.  Set it to\n");
			printf("       the amount of memory (in MB) and try again.\n");
			printf("       Or, build a kernel with the `MEMSIZE' "
			    "option.\n");
			panic("pb1000_init");
		}
	}
#endif /* MEMSIZE */
	printf("Memory size: 0x%08lx\n", memsize);
	physmem = btoc(memsize);

	mem_clusters[mem_cluster_cnt].start = NBPG;
	mem_clusters[mem_cluster_cnt].size =
	    memsize - mem_clusters[mem_cluster_cnt].start;
	mem_cluster_cnt++;

	/*
	 * Load the rest of the available pages into the VM system.
	 */
	first = round_page(MIPS_KSEG0_TO_PHYS(kernend));
	last = mem_clusters[0].start + mem_clusters[0].size;
	uvm_page_physload(atop(first), atop(last), atop(first), atop(last),
	    VM_FREELIST_DEFAULT);

	/*
	 * Initialize message buffer (at end of core).
	 */
	mips_init_msgbuf();

	/*
	 * Compute the size of system data structures.  pmap_bootstrap()
	 * needs some of this information.
	 */
	memsize = (u_long)allocsys(NULL, NULL);

	/*
	 * Initialize the virtual memory system.
	 */
	pmap_bootstrap();

	/*
	 * Init mapping for u page(s) for proc0.
	 */
	v = (caddr_t) uvm_pageboot_alloc(USPACE);
	lwp0.l_addr = proc0paddr = (struct user *)v;
	lwp0.l_md.md_regs = (struct frame *)(v + USPACE) - 1;
	curpcb = &lwp0.l_addr->u_pcb;
	curpcb->pcb_context[11] = MIPS_INT_MASK | MIPS_SR_INT_IE; /* SR */

	/*
	 * Allocate space for system data structures.  These data structures
	 * are allocated here instead of cpu_startup() because physical
	 * memory is directly addressable.  We don't have to map these into
	 * the virtual address space.
	 */
	v = (caddr_t)uvm_pageboot_alloc(memsize);
	if ((allocsys(v, NULL) - v) != memsize)
		panic("mach_init: table size inconsistency");

	/*
	 * Initialize debuggers, and break into them, if appropriate.
	 */
#ifdef DDB
	ddb_init(0, 0, 0);
	if (boothowto & RB_KDB)
		Debugger();
#endif
}

void
consinit(void)
{

	/*
	 * Everything related to console initialization is done
	 * in mach_init().
	 */
}

void
cpu_startup(void)
{
	char pbuf[9];
	vaddr_t minaddr, maxaddr;
	vsize_t size;
	u_int i, base, residual;
#ifdef DEBUG
	extern int pmapdebug;		/* XXX */
	int opmapdebug = pmapdebug;

	pmapdebug = 0;		/* Shut up pmap debug during bootstrap */
#endif

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("%s\n", cpu_model);
	format_bytes(pbuf, sizeof(pbuf), ctob(physmem));
	printf("total memory = %s\n", pbuf);

	/*
	 * Allocate virtual address space for file I/O buffers.
	 * Note they are different than the array of headers, 'buf',
	 * and usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *)&buffers, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				UVM_ADV_NORMAL, 0)) != 0)
		panic("cpu_startup: cannot allocate VM for buffers");

	minaddr = (vaddr_t)buffers;
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		bufpages = btoc(MAXBSIZE) * nbuf; /* do not overallocate RAM */
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;

	/* now allocate RAM for buffers */
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
		curbuf = (vaddr_t)buffers + (i * MAXBSIZE);
		curbufsize = NBPG * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
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
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    16 * NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, FALSE, NULL);

	/*
	 * No need to allocate an mbuf cluster submap.  Mbuf clusters
	 * are allocated via the pool allocator, and we use KSEG to
	 * map those pages.
	 */

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	format_bytes(pbuf, sizeof(pbuf), ptoa(uvmexp.free));
	printf("avail memory = %s\n", pbuf);
	format_bytes(pbuf, sizeof(pbuf), bufpages * NBPG);
	printf("using %u buffers containing %s of memory\n", nbuf, pbuf);

	/*
	 * Set up buffers, so they can be used to read disklabels.
	 */
	bufinit();

	/*
	 * Set up the chip/board properties database.
	 */
	if (!(alchemy_prop_info = propdb_create("board info")))
		panic("Cannot create board info database");
}

void
cpu_reboot(int howto, char *bootstr)
{
	static int waittime = -1;

	/* Take a snapshot before clobbering any registers. */
	if (curproc)
		savectx((struct user *)curpcb);

	/* If "always halt" was specified as a boot flag, obey. */
	if (boothowto & RB_HALT)
		howto |= RB_HALT;

	boothowto = howto;

	/* If system is cold, just halt. */
	if (cold) {
		boothowto |= RB_HALT;
		goto haltsys;
	}

	if ((boothowto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;

		/*
		 * Synchronize the disks....
		 */
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}

	/* Disable interrupts. */
	splhigh();

	if (boothowto & RB_DUMP)
		dumpsys();

 haltsys:
	/* Run any shutdown hooks. */
	doshutdownhooks();

#if 1
	/* XXX
	 * For some reason we are leaving the ethernet MAC in a state where
	 * YAMON isn't happy with it.  So just call the reset vector (grr,
	 * Alchemy YAMON doesn't have a "reset" command).
	 */
	printf("reseting board...\n\n");
	mips_icache_sync_all();
	mips_dcache_wbinv_all();
	asm volatile("jr	%0" :: "r"(MIPS_RESET_EXC_VEC));
#else
	printf("%s\n\n", ((howto & RB_HALT) != 0) ? "halted." : "rebooting...");
	yamon_exit(boothowto);
	printf("Oops, back from yamon_exit()\n\nSpinning...");
#endif
	for (;;)
		/* spin forever */ ;	/* XXX */
	/*NOTREACHED*/
}
