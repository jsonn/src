/*	$NetBSD: machdep.c,v 1.60.2.6 1999/06/25 20:40:14 perry Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
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
 * from: Utah $Hdr: machdep.c 1.74 92/12/20$
 *
 *	@(#)machdep.c	8.10 (Berkeley) 4/20/94
 */

#include "opt_bufcache.h"
#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_atalk.h"
#include "opt_ccitt.h"
#include "opt_iso.h"
#include "opt_ns.h"
#include "opt_compat_hpux.h"
#include "opt_compat_netbsd.h"
#include "opt_fpuemulate.h"
#include "opt_m060sp.h"
#include "opt_panicbutton.h"
#include "opt_sysv.h"
#include "opt_extmem.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/mount.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/syscallargs.h>
#include <sys/core.h>
#include <sys/kcore.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/kcore.h>

#include <net/netisr.h>
#include <dev/cons.h>

#define	MAXMEM	64*1024*CLSIZE	/* XXX - from cmap.h */
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <uvm/uvm_extern.h>

#include <sys/sysctl.h>

#include <sys/device.h>

#include <machine/bus.h>
#include <arch/x68k/dev/intiovar.h>

void initcpu __P((void));
void identifycpu __P((void));
void doboot __P((void))
    __attribute__((__noreturn__));
int badaddr __P((caddr_t));
int badbaddr __P((caddr_t));

/* the following is used externally (sysctl_hw) */
char	machine[] = MACHINE;	/* from <machine/param.h> */

vm_map_t exec_map = NULL;  
vm_map_t mb_map = NULL;
vm_map_t phys_map = NULL;

extern paddr_t avail_start, avail_end;
extern vaddr_t virtual_avail;

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
#ifdef BUFCACHE
int	bufcache = BUFCACHE;	/* % of RAM to use for buffer cache */
#else
int	bufcache = 0;		/* fallback to old algorithm */
#endif

caddr_t	msgbufaddr;
int	maxmem;			/* max memory per process */
int	physmem = MAXMEM;	/* max supported memory, changes to actual */
/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int	safepri = PSL_LOWIPL;

extern	u_int lowram;
extern	short exframesize[];

#ifdef COMPAT_HPUX
extern struct emul emul_hpux;
#endif

/* prototypes for local functions */
void    identifycpu __P((void));
void    initcpu __P((void));
int	cpu_dumpsize __P((void));
int	cpu_dump __P((int (*)(dev_t, daddr_t, caddr_t, size_t), daddr_t *));
void	cpu_init_kcore_hdr __P((void));
#ifdef EXTENDED_MEMORY
static int mem_exists __P((caddr_t, u_long));
static void setmemrange __P((void));
#endif

/* functions called from locore.s */
void	dumpsys __P((void));
void    straytrap __P((int, u_short));
void	nmihand __P((struct frame));
void	intrhand __P((int));

/*
 * On the 68020/68030, the value of delay_divisor is roughly
 * 2048 / cpuspeed (where cpuspeed is in MHz).
 *
 * On the 68040, the value of delay_divisor is roughly
 * 759 / cpuspeed (where cpuspeed is in MHz).
 *
 * On the 68060, the value of delay_divisor is reported to be
 * 128 / cpuspeed (where cpuspeed is in MHz).
 */
int	delay_divisor = 140;	/* assume some reasonable value to start */
static int cpuspeed;		/* MPU clock (in MHz) */

/*
 * Machine-dependent crash dump header info.
 */
cpu_kcore_hdr_t cpu_kcore_hdr;

/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */
void
consinit()
{
	/*
	 * bring graphics layer up.
	 */
	config_console();

	/*
	 * Initialize the console before we print anything out.
	 */
	cninit();

#ifdef KGDB
	zs_kgdb_init();			/* XXX */
#endif
#ifdef DDB
	{
		extern int end;
		extern int *esym;

		ddb_init(*(int *)&end, ((int *)&end) + 1, esym);
	}
	if (boothowto & RB_KDB)
		Debugger();
#endif

	/*
	 * Tell the VM system about available physical memory.
	 */
	uvm_page_physload(atop(avail_start), atop(avail_end),
			atop(avail_start), atop(avail_end),
			VM_FREELIST_DEFAULT);
#ifdef EXTENDED_MEMORY
	setmemrange();
#endif
}

/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup()
{
	unsigned i;
	caddr_t v, firstaddr;
	int base, residual;
	vaddr_t minaddr, maxaddr;
	vsize_t size;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif
#if 0
	rtclockinit(); /* XXX */
#endif

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in pmap_bootstrap to compensate.
	 */
	for (i = 0; i < btoc(MSGBUFSIZE); i++)
		pmap_enter(pmap_kernel(), (vaddr_t)msgbufaddr + i * NBPG,
		    avail_end + i * NBPG, VM_PROT_READ|VM_PROT_WRITE, TRUE,
		    VM_PROT_READ|VM_PROT_WRITE);
	initmsgbuf(msgbufaddr, m68k_round_page(MSGBUFSIZE));

	/*
	 * Initialize the kernel crash dump header.
	 */
	cpu_init_kcore_hdr();

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();
	printf("real mem  = %d\n", ctob(physmem));

	/*
	 * Allocate space for system data structures.
	 * The first available real memory address is in "firstaddr".
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 * As pages of memory are allocated and cleared,
	 * "firstaddr" is incremented.
	 * An index into the kernel page table corresponding to the
	 * virtual memory address maintained in "v" is kept in "mapaddr".
	 */
	/*
	 * Make two passes.  The first pass calculates how much memory is
	 * needed and allocates it.  The second pass assigns virtual
	 * addresses to the various data structures.
	 */
	firstaddr = 0;
again:
	v = (caddr_t)firstaddr;

#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))
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
	
	if (bufpages == 0) {
		if (bufcache == 0) {		/* use old algorithm */
			/*
			 * Determine how many buffers to allocate.
			 * Since an x68k tends to be long on memory and short
			 * on disk speed, we allocate more buffer space than
			 * the BSD standard of use 10% of memory for the first
			 * 2 Meg, 5% of remaining.
			 * We just allocate a flat 10%.  Insure a minimum of 16
			 * buffers.
			 * We allocate 1/2 as many swap buffer headers as file
			 * i/o buffers.
			 */
			bufpages = physmem / 10 / CLSIZE;
			if (physmem < btoc(2 * 1024 * 1024))
				bufpages = physmem / 10 / CLSIZE;
			else
				bufpages = (btoc(2 * 1024 * 1024) + physmem) /
				    (20 * CLSIZE);
		} else {
			/*
			 * Set size of buffer cache to physmem/bufcache * 100
			 * (i.e., bufcache % of physmem).
			 */
			if (bufcache < 5 || bufcache > 95) {
				printf(
		"warning: unable to set bufcache to %d%% of RAM, using 10%%",
				    bufcache);
				bufcache = 10;
			}
			bufpages= physmem / (CLSIZE * 100) * bufcache;
		}
	}

	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(buf, struct buf, nbuf);
	/*
	 * End of first pass, size has been calculated so allocate memory
	 */
	if (firstaddr == 0) {
		size = (vsize_t)(v - firstaddr);
		firstaddr = (caddr_t) uvm_km_zalloc(kernel_map, round_page(size));
		if (firstaddr == 0)
			panic("startup: no room for tables");
		goto again;
	}
	/*
	 * End of second pass, addresses have been assigned
	 */
	if ((vsize_t)(v - firstaddr) != size)
		panic("startup: table size inconsistency");
	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				UVM_ADV_NORMAL, 0)) != KERN_SUCCESS)
		panic("startup: cannot allocate VM for buffers");
	minaddr = (vaddr_t)buffers;
#if 0
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
#endif
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
		curbuf = (vsize_t) buffers + (i * MAXBSIZE);
		curbufsize = CLBYTES * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL) 
				panic("cpu_startup: not enough memory for "
				    "buffer cache");
#if defined(PMAP_NEW)
			pmap_kenter_pgs(curbuf, &pg, 1);
#else
			pmap_enter(kernel_map->pmap, curbuf,
			    VM_PAGE_TO_PHYS(pg), VM_PROT_READ|VM_PROT_WRITE,
			    TRUE, VM_PROT_READ|VM_PROT_WRITE);
#endif
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16*NCARGS, TRUE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, TRUE, FALSE, NULL);

	/*
	 * Finally, allocate mbuf cluster submap.
	 */
	mb_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_MBUF_SIZE, FALSE, FALSE, NULL);

	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];
	callout[i-1].c_next = NULL;

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %ld\n", ptoa(uvmexp.free));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);
	/*
	 * Set up CPU-specific registers, cache, etc.
	 */
	initcpu();

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
}

/*
 * Set registers on exec.
 */
void
setregs(p, pack, stack)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
{
	struct frame *frame = (struct frame *)p->p_md.md_regs;

	frame->f_sr = PSL_USERSET;
	frame->f_pc = pack->ep_entry & ~1;
	frame->f_regs[D0] = 0;
	frame->f_regs[D1] = 0;
	frame->f_regs[D2] = 0;
	frame->f_regs[D3] = 0;
	frame->f_regs[D4] = 0;
	frame->f_regs[D5] = 0;
	frame->f_regs[D6] = 0;
	frame->f_regs[D7] = 0;
	frame->f_regs[A0] = 0;
	frame->f_regs[A1] = 0;
	frame->f_regs[A2] = (int)PS_STRINGS;
	frame->f_regs[A3] = 0;
	frame->f_regs[A4] = 0;
	frame->f_regs[A5] = 0;
	frame->f_regs[A6] = 0;
	frame->f_regs[SP] = stack;

	/* restore a null state frame */
	p->p_addr->u_pcb.pcb_fpregs.fpf_null = 0;
	if (fputype)
		m68881_restore(&p->p_addr->u_pcb.pcb_fpregs);
}

/*
 * Info for CTL_HW
 */
char	cpu_model[96];		/* max 85 chars */
extern	char version[];
static char *fpu_descr[] = {
#ifdef	FPU_EMULATE
	", emulator FPU", 	/* 0 */
#else
	", no math support",	/* 0 */
#endif
	", m68881 FPU",		/* 1 */
	", m68882 FPU",		/* 2 */
	"/FPU",			/* 3 */
	"/FPU",			/* 4 */
	};

void
identifycpu()
{
        /* there's alot of XXX in here... */
	char *cpu_type, *mach, *mmu, *fpu;
	char clock[16];

	/*
	 * check machine type constant
	 */
	switch (intio_get_sysport_mpustat()) {
	case 0xdc:
		/*
		 * CPU Type == 68030, Clock == 25MHz
		 */
		mach = "030";
		break;
	case 0xfe:
		/*
		 * CPU Type == 68000, Clock == 16MHz
		 */
		mach = "000XVI";
		break;
	case 0xff:
		/*
		 * CPU Type == 68000, Clock == 10MHz
		 */
		mach = "000/ACE/PRO/EXPERT/SUPER";
		break;
	default:
		/*
		 * unknown type
		 */
		mach = "000?(unknown model)";
		break;
	}

	cpuspeed = 2048 / delay_divisor;
	sprintf(clock, "%dMHz", cpuspeed);
	switch (cputype) {
	case CPU_68060:
		cpu_type = "m68060";
		mmu = "/MMU";
		cpuspeed = 128 / delay_divisor;
		sprintf(clock, "%d/%dMHz", cpuspeed*2, cpuspeed);
		break;
	case CPU_68040:
		cpu_type = "m68040";
		mmu = "/MMU";
		cpuspeed = 759 / delay_divisor;
		sprintf(clock, "%d/%dMHz", cpuspeed*2, cpuspeed);
		break;
	case CPU_68030:
		cpu_type = "m68030";
		mmu = "/MMU";
		break;
	case CPU_68020:
		cpu_type = "m68020";
		mmu = ", m68851 MMU";
		break;
	default:
		cpu_type = "unknown";
		mmu = ", unknown MMU";
		break;
	}
	fputype = fpu_probe();
	if (fputype >= 0 && fputype < sizeof(fpu_descr)/sizeof(fpu_descr[0]))
		fpu = fpu_descr[fputype];
	else
		fpu = ", unknown FPU";
	sprintf(cpu_model, "X68%s (%s CPU%s%s, %s clock)",
		mach, cpu_type, mmu, fpu, clock);
	printf("%s\n", cpu_model);
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
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

int	waittime = -1;
int	power_switch_is_off = 0;

void
cpu_reboot(howto, bootstr)
	int howto;
	char *bootstr;
{
	/* take a snap shot before clobbering any registers */
	if (curproc && curproc->p_addr)
		savectx(&curproc->p_addr->u_pcb);

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		/*resettodr();*/
	}

	/* Disable interrputs. */
	splhigh();

	if (howto & RB_DUMP)
		dumpsys();

	/* Run any shutdown hooks. */
	doshutdownhooks();

#if defined(PANICWAIT) && !defined(DDB)
	if ((howto & RB_HALT) == 0 && panicstr) {
		printf("hit any key to reboot...\n");
		(void)cngetc();
		printf("\n");
	}
#endif

	/* Finally, halt/reboot the system. */
	/* a) RB_POWERDOWN
	 *  a1: the power switch is still on
	 *	Power cannot be removed; simply halt the system (b)
	 *	Power switch state is checked in shutdown hook
	 *  a2: the power switch is off
	 *	Remove the power; the simplest way is go back to ROM eg. reboot
	 * b) RB_HALT
	 *      call cngetc
         * c) otherwise
	 *	Reboot
	*/
	if (((howto & RB_POWERDOWN) == RB_POWERDOWN) && power_switch_is_off)
		doboot();
	else if (/*((howto & RB_POWERDOWN) == RB_POWERDOWN) ||*/
		 ((howto & RB_HALT) == RB_HALT)) {
		printf("System halted.  Hit any key to reboot.\n\n");
		(void)cngetc();
	}

	printf("rebooting...\n");
	DELAY(1000000);
	doboot();
	/*NOTREACHED*/
}

/*
 * Initialize the kernel crash dump header.
 */
void
cpu_init_kcore_hdr()
{
	extern int end;
	cpu_kcore_hdr_t *h = &cpu_kcore_hdr;
	struct m68k_kcore_hdr *m = &h->un._m68k;
	int i;

	bzero(&cpu_kcore_hdr, sizeof(cpu_kcore_hdr));

	/*
	 * Initialize the `dispatcher' portion of the header.
	 */
	strcpy(h->name, machine);
	h->page_size = NBPG;
	h->kernbase = KERNBASE;

	/*
	 * Fill in information about our MMU configuration.
	 */
	m->mmutype	= mmutype;
	m->sg_v		= SG_V;
	m->sg_frame	= SG_FRAME;
	m->sg_ishift	= SG_ISHIFT;
	m->sg_pmask	= SG_PMASK;
	m->sg40_shift1	= SG4_SHIFT1;
	m->sg40_mask2	= SG4_MASK2;
	m->sg40_shift2	= SG4_SHIFT2;
	m->sg40_mask3	= SG4_MASK3;
	m->sg40_shift3	= SG4_SHIFT3;
	m->sg40_addr1	= SG4_ADDR1;
	m->sg40_addr2	= SG4_ADDR2;
	m->pg_v		= PG_V;
	m->pg_frame	= PG_FRAME;

	/*
	 * Initialize pointer to kernel segment table.
	 */
	m->sysseg_pa = (u_int32_t)(pmap_kernel()->pm_stpa);

	/*
	 * Initialize relocation value such that:
	 *
	 *	pa = (va - KERNBASE) + reloc
	 */
	m->reloc = lowram;

	/*
	 * Define the end of the relocatable range.
	 */
	m->relocend = (u_int32_t)&end;

	/*
	 * X68k has multiple RAM segments on some models.
	 */
	m->ram_segs[0].start = lowram;
	m->ram_segs[0].size = mem_size - lowram;
	for (i = 1; i < vm_nphysseg; i++) {
		m->ram_segs[i].start = ctob(vm_physmem[i].start);
		m->ram_segs[i].size  = ctob(vm_physmem[i].end)
		    - vm_physmem[i].start;
	}
}

/*
 * Compute the size of the machine-dependent crash dump header.
 * Returns size in disk blocks.
 */
int
cpu_dumpsize()
{
	int size;

	size = ALIGN(sizeof(kcore_seg_t)) + ALIGN(sizeof(cpu_kcore_hdr_t));
	return (btodb(roundup(size, dbtob(1))));
}

/*
 * Called by dumpsys() to dump the machine-dependent header.
 */
int
cpu_dump(dump, blknop)
	int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	daddr_t *blknop;
{
	int buf[dbtob(1) / sizeof(int)];
	cpu_kcore_hdr_t *chdr;
	kcore_seg_t *kseg;
	int error;

	kseg = (kcore_seg_t *)buf;
	chdr = (cpu_kcore_hdr_t *)&buf[ALIGN(sizeof(kcore_seg_t)) /
	    sizeof(int)];

	/* Create the segment header. */
	CORE_SETMAGIC(*kseg, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	kseg->c_size = dbtob(1) - ALIGN(sizeof(kcore_seg_t));

	bcopy(&cpu_kcore_hdr, chdr, sizeof(cpu_kcore_hdr_t));
	error = (*dump)(dumpdev, *blknop, (caddr_t)buf, sizeof(buf));
	*blknop += btodb(sizeof(buf));
	return (error);
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int	dumpsize = 0;		/* pages */
long	dumplo = 0;		/* blocks */

/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space in
 * case there might be a disk label stored there.  If there
 * is extra space, put dump at the end to reduce the chance
 * that swapping trashes it.
 */
void
cpu_dumpconf()
{
	cpu_kcore_hdr_t *h = &cpu_kcore_hdr;
	struct m68k_kcore_hdr *m = &h->un._m68k;
	int chdrsize;	/* size of dump header */
	int nblks;	/* size of dump area */
	int maj;
	int i;

	if (dumpdev == NODEV)
		return;

	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	chdrsize = cpu_dumpsize();

	dumpsize = 0;
	for (i = 0; m->ram_segs[i].size && i < M68K_NPHYS_RAM_SEGS; i++)
		dumpsize += btoc(m->ram_segs[i].size);
	/*
	 * Check to see if we will fit.  Note we always skip the
	 * first CLBYTES in case there is a disk label there.
	 */
	if (nblks < (ctod(dumpsize) + chdrsize + ctod(1))) {
		dumpsize = 0;
		dumplo = -1;
		return;
	}

	/*
	 * Put dump at the end of the partition.
	 */
	dumplo = (nblks - 1) - ctod(dumpsize) - chdrsize;
}

void
dumpsys()
{
	cpu_kcore_hdr_t *h = &cpu_kcore_hdr;
	struct m68k_kcore_hdr *m = &h->un._m68k;
	daddr_t blkno;		/* current block to write */
				/* dump routine */
	int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	int pg;			/* page being dumped */
	paddr_t maddr;		/* PA being dumped */
	int seg;		/* RAM segment being dumped */
	int error;		/* error code from (*dump)() */

	/* XXX initialized here because of gcc lossage */
	seg = 0;
	maddr = m->ram_segs[seg].start;
	pg = 0;

	/* Don't put dump messages in msgbuf. */
	msgbufenabled = 0;

	/* Make sure dump device is valid. */
	if (dumpdev == NODEV)
		return;
	if (dumpsize == 0) {
		cpu_dumpconf();
		if (dumpsize == 0)
			return;
	}
	if (dumplo <= 0) {
		printf("\ndump to dev %u,%u not possible\n", major(dumpdev),
		    minor(dumpdev));
		return;
	}
	dump = bdevsw[major(dumpdev)].d_dump;
	blkno = dumplo;

	printf("\ndumping to dev %u,%u offset %ld\n", major(dumpdev),
	    minor(dumpdev), dumplo);

	printf("dump ");

	/* Write the dump header. */
	error = cpu_dump(dump, &blkno);
	if (error)
		goto bad;

	for (pg = 0; pg < dumpsize; pg++) {
#define NPGMB	(1024*1024/NBPG)
		/* print out how many MBs we have dumped */
		if (pg && (pg % NPGMB) == 0)
			printf("%d ", pg / NPGMB);
#undef NPGMB
		if (maddr == 0) {
			/* Skip first page */
			maddr += NBPG;
			blkno += btodb(NBPG);
			continue;
		}
		while (maddr >=
		    (m->ram_segs[seg].start + m->ram_segs[seg].size)) {
			if (++seg >= M68K_NPHYS_RAM_SEGS ||
			    m->ram_segs[seg].size == 0) {
				error = EINVAL;		/* XXX ?? */
				goto bad;
			}
			maddr = m->ram_segs[seg].start;
		}
		pmap_enter(pmap_kernel(), (vaddr_t)vmmap, maddr,
		    VM_PROT_READ, TRUE, VM_PROT_READ);

		error = (*dump)(dumpdev, blkno, vmmap, NBPG);
 bad:
		switch (error) {
		case 0:
			maddr += NBPG;
			blkno += btodb(NBPG);
			break;

		case ENXIO:
			printf("device bad\n");
			return;

		case EFAULT:
			printf("device not ready\n");
			return;

		case EINVAL:
			printf("area improper\n");
			return;

		case EIO:
			printf("i/o error\n");
			return;

		case EINTR:
			printf("aborted from console\n");
			return;

		default:
			printf("error %d\n", error);
			return;
		}
	}
	printf("succeeded\n");
}

void
initcpu()
{
	/* XXX should init '40 vecs here, too */
#if defined(M68060)
	extern caddr_t vectab[256];
#if defined(M060SP)
	extern u_int8_t I_CALL_TOP[];
	extern u_int8_t FP_CALL_TOP[];
#else
	extern u_int8_t illinst;
#endif
	extern u_int8_t fpfault;
#endif

#ifdef MAPPEDCOPY
	extern u_int mappedcopysize;

	/*
	 * Initialize lower bound for doing copyin/copyout using
	 * page mapping (if not already set).  We don't do this on
	 * VAC machines as it loses big time.
	 */
	if ((int) mappedcopysize == -1) {
		mappedcopysize = NBPG;
	}
#endif

#if defined(M68060)
	if (cputype == CPU_68060) {
#if defined(M060SP)
		/* integer support */
		vectab[61] = &I_CALL_TOP[128 + 0x00];

		/* floating point support */
		vectab[11] = &FP_CALL_TOP[128 + 0x30];
		vectab[55] = &FP_CALL_TOP[128 + 0x38];
		vectab[60] = &FP_CALL_TOP[128 + 0x40];

		vectab[54] = &FP_CALL_TOP[128 + 0x00];
		vectab[52] = &FP_CALL_TOP[128 + 0x08];
		vectab[53] = &FP_CALL_TOP[128 + 0x10];
		vectab[51] = &FP_CALL_TOP[128 + 0x18];
		vectab[50] = &FP_CALL_TOP[128 + 0x20];
		vectab[49] = &FP_CALL_TOP[128 + 0x28];
#else
		vectab[61] = &illinst;
#endif
		vectab[48] = &fpfault;
	}
	DCIS();
#endif
}

void
straytrap(pc, evec)
	int pc;
	u_short evec;
{
	printf("unexpected trap (vector offset %x) from %x\n",
	       evec & 0xFFF, pc);
#if defined(DDB)
	Debugger();
#endif
}

int	*nofault;

int
badaddr(addr)
	caddr_t addr;
{
	int i;
	label_t	faultbuf;

	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *) 0;
		return(1);
	}
	i = *(volatile short *)addr;
	nofault = (int *) 0;
	return(0);
}

int
badbaddr(addr)
	caddr_t addr;
{
	int i;
	label_t	faultbuf;

	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *) 0;
		return(1);
	}
	i = *(volatile char *)addr;
	nofault = (int *) 0;
	return(0);
}

/*
 * XXX Why on earth isn't this in a common file?!
 */
void	netintr __P((void));
void	arpintr __P((void));
void	atintr __P((void));
void	ipintr __P((void));
void	nsintr __P((void));
void	clnlintr __P((void));
void	ccittintr __P((void));
void	pppintr __P((void));

void
netintr()
{
#ifdef INET
#include "arp.h"
#if NARP > 0
	if (netisr & (1 << NETISR_ARP)) {
		netisr &= ~(1 << NETISR_ARP);
		arpintr();
	}
#endif
	if (netisr & (1 << NETISR_IP)) {
		netisr &= ~(1 << NETISR_IP);
		ipintr();
	}
#endif
#ifdef NETATALK
	if (netisr & (1 << NETISR_ATALK)) {
		netisr &= ~(1 << NETISR_ATALK);
		atintr();
	}
#endif
#ifdef NS
	if (netisr & (1 << NETISR_NS)) {
		netisr &= ~(1 << NETISR_NS);
		nsintr();
	}
#endif
#ifdef ISO
	if (netisr & (1 << NETISR_ISO)) {
		netisr &= ~(1 << NETISR_ISO);
		clnlintr();
	}
#endif
#ifdef CCITT
	if (netisr & (1 << NETISR_CCITT)) {
		netisr &= ~(1 << NETISR_CCITT);
		ccittintr();
	}
#endif
#include "ppp.h"
#if NPPP > 0
	if (netisr & (1 << NETISR_PPP)) {
		netisr &= ~(1 << NETISR_PPP);
		pppintr();
	}
#endif
}

void
intrhand(sr)
	int sr;
{
	printf("intrhand: unexpected sr 0x%x\n", sr);
}

#if (defined(DDB) || defined(DEBUG)) && !defined(PANICBUTTON)
#define PANICBUTTON
#endif

#ifdef PANICBUTTON
int panicbutton = 1;	/* non-zero if panic buttons are enabled */
int crashandburn = 0;
int candbdelay = 50;	/* give em half a second */
void candbtimer __P((void *));

void
candbtimer(arg)
	void *arg;
{

	crashandburn = 0;
}
#endif

/*
 * Level 7 interrupts can be caused by the keyboard or parity errors.
 */
void
nmihand(frame)
	struct frame frame;
{
	intio_set_sysport_keyctrl(intio_get_sysport_keyctrl() | 0x04);

	if (1) {
#ifdef PANICBUTTON
		static int innmihand = 0;

		/*
		 * Attempt to reduce the window of vulnerability for recursive
		 * NMIs (e.g. someone holding down the keyboard reset button).
		 */
		if (innmihand == 0) {
			innmihand = 1;
			printf("Got a keyboard NMI\n");
			innmihand = 0;
		}
#ifdef DDB
		Debugger();
#else
		if (panicbutton) {
			if (crashandburn) {
				crashandburn = 0;
				panic(panicstr ?
				      "forced crash, nosync" : "forced crash");
			}
			crashandburn++;
			timeout(candbtimer, (void *)0, candbdelay);
		}
#endif /* DDB */
#endif /* PANICBUTTON */
		return;
	}
	/* panic?? */
	printf("unexpected level 7 interrupt ignored\n");
}

/*
 * cpu_exec_aout_makecmds():
 *	cpu-dependent a.out format hook for execve().
 * 
 * Determine of the given exec package refers to something which we
 * understand and, if so, set up the vmcmds for it.
 *
 * XXX what are the special cases for the hp300?
 * XXX why is this COMPAT_NOMID?  was something generating
 *	hp300 binaries with an a_mid of 0?  i thought that was only
 *	done on little-endian machines...  -- cgd
 */
int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
#if defined(COMPAT_NOMID) || defined(COMPAT_44)
	u_long midmag, magic;
	u_short mid;
	int error;
	struct exec *execp = epp->ep_hdr;

	midmag = ntohl(execp->a_midmag);
	mid = (midmag >> 16) & 0xffff;
	magic = midmag & 0xffff;

	midmag = mid << 16 | magic;

	switch (midmag) {
#ifdef COMPAT_NOMID
	case (MID_ZERO << 16) | ZMAGIC:
		error = exec_aout_prep_oldzmagic(p, epp);
		break;
#endif
#ifdef COMPAT_44
	case (MID_HP300 << 16) | ZMAGIC:
		error = exec_aout_prep_oldzmagic(p, epp);
		break;
#endif
	default:
		error = ENOEXEC;
	}

	return error;
#else /* !(defined(COMPAT_NOMID) || defined(COMPAT_44)) */
	return ENOEXEC;
#endif
}

#ifdef EXTENDED_MEMORY
#ifdef EM_DEBUG
static int em_debug = 0;
#define DPRINTF(str) do{ if (em_debug) printf str; } while (0);
#else
#define DPRINTF(str)
#endif

static struct memlist {
	caddr_t base;
	psize_t min;
	psize_t max;
} memlist[] = {
	/* TS-6BE16 16MB memory */
	{(caddr_t)0x01000000, 0x01000000, 0x01000000},
	/* 060turbo SIMM slot (4--128MB) */
	{(caddr_t)0x10000000, 0x00400000, 0x08000000},
};
static vaddr_t mem_v, base_v;

/*
 * check memory existency
 */
static int
mem_exists(mem, basemax)
	caddr_t mem;
	u_long basemax;
{
	/* most variables must be register! */
	register volatile unsigned char *m, *b;
	register unsigned char save_m, save_b;
	register int baseismem;
	register int exists = 0;
	caddr_t base;
	caddr_t begin_check, end_check;
	label_t	faultbuf;

	DPRINTF (("Enter mem_exists(%p, %x)\n", mem, basemax));
	DPRINTF ((" pmap_enter(%p, %p) for target... ", mem_v, mem));
	pmap_enter(pmap_kernel(), mem_v, (paddr_t)mem,
		   VM_PROT_READ|VM_PROT_WRITE, TRUE, VM_PROT_READ);
	DPRINTF ((" done.\n"));

	/* only 24bits are significant on normal X680x0 systems */
	base = (caddr_t)((u_long)mem & 0x00FFFFFF);
	DPRINTF ((" pmap_enter(%p, %p) for shadow... ", base_v, base));
	pmap_enter(pmap_kernel(), base_v, (paddr_t)base,
		   VM_PROT_READ|VM_PROT_WRITE, TRUE, VM_PROT_READ);
	DPRINTF ((" done.\n"));

	m = (void*)mem_v;
	b = (void*)base_v;

	/* This is somewhat paranoid -- avoid overwriting myself */
	asm("lea pc@(begin_check_mem),%0" : "=a"(begin_check));
	asm("lea pc@(end_check_mem),%0" : "=a"(end_check));
	if (base >= begin_check && base < end_check) {
		size_t off = end_check - begin_check;

		DPRINTF ((" Adjusting the testing area.\n"));
		m -= off;
		b -= off;
	}

	nofault = (int *) &faultbuf;
	if (setjmp ((label_t *)nofault)) {
		nofault = (int *) 0;
		pmap_remove(pmap_kernel(), mem_v, mem_v+NBPG);
		pmap_remove(pmap_kernel(), base_v, base_v+NBPG);
		DPRINTF (("Fault!!! Returning 0.\n"));
		return 0;
	}

	DPRINTF ((" Let's begin. mem=%p, base=%p, m=%p, b=%p\n",
		  mem, base, m, b));

	(void) *m; 
	/*
	 * Can't check by writing if the corresponding
	 * base address isn't memory.
	 *
	 * I hope this would be no harm....
	 */
	baseismem = base < (caddr_t)basemax;

	/* save original value (base must be saved first) */
	if (baseismem)
		save_b = *b;
	save_m = *m;

asm("begin_check_mem:");
	/*
	 * stack and other data segment variables are unusable
	 * til end_check_mem, because they may be clobbered.
	 */

	/*
	 * check memory by writing/reading
	 */
	if (baseismem)
		*b = 0x55;
	*m = 0xAA;
	if ((baseismem && *b != 0x55) || *m != 0xAA)
		goto out;

	*m = 0x55;
	if (baseismem)
		*b = 0xAA;
	if (*m != 0x55 || (baseismem && *b != 0xAA))
		goto out;

	exists = 1;
out:
	*m = save_m;
	if (baseismem)
		*b = save_b;

asm("end_check_mem:");

	nofault = (int *)0;
	pmap_remove(pmap_kernel(), mem_v, mem_v+NBPG);
	pmap_remove(pmap_kernel(), base_v, base_v+NBPG);

	DPRINTF ((" End.\n"));

	DPRINTF (("Returning from mem_exists. result = %d\n", exists));

	return exists;
}

static void
setmemrange(void)
{
	int i;
	psize_t s, min, max;
	int basemax = ctob(physmem);
	struct memlist *mlist = memlist;
	u_long h;

	/*
	 * VM system is not started yet.  Use the first and second avalable
	 * pages to map the (possible) target memory and its shadow.
	 */
	mem_v = virtual_avail;	/* target */
	base_v = mem_v + NBPG;	/* shadow */

	{	/* Turn off the processor cache. */
		register int cacr;
		PCIA();		/* cpusha dc */
		switch (cputype) {
		case CPU_68030:
			cacr = CACHE_OFF;
			break;
		case CPU_68040:
			cacr = CACHE40_OFF;
			break;
		case CPU_68060:
			cacr = CACHE60_OFF;
			break;
		}
		asm volatile ("movc %0,cacr"::"d"(cacr));
	}

	/* discover extended memory */
	for (i = 0; i < sizeof(memlist) / sizeof(memlist[0]); i++) {
		min = mlist[i].min;
		max = mlist[i].max;
		/*
		 * Normally, x68k hardware is NOT 32bit-clean.
		 * But some type of extended memory is in 32bit address space.
		 * Check whether.
		 */
		if (!mem_exists(mlist[i].base, basemax))
			continue;
		h = 0;
		/* range check */
		for (s = min; s <= max; s += 0x00100000) {
			if (!mem_exists(mlist[i].base + s - 4, basemax))
				break;
			h = (u_long)(mlist[i].base + s);
		}
		if ((u_long)mlist[i].base < h) {
			uvm_page_physload(atop(mlist[i].base), atop(h),
					  atop(mlist[i].base), atop(h),
					  VM_FREELIST_DEFAULT);
			mem_size += h - (u_long) mlist[i].base;
		}
			
	}

	{	/* Re-enable the processor cache. */
		register int cacr;
		ICIA();
		switch (cputype) {
		case CPU_68030:
			cacr = CACHE_ON;
			break;
		case CPU_68040:
			cacr = CACHE40_ON;
			break;
		case CPU_68060:
			cacr = CACHE60_ON;
			break;
		}
		asm volatile ("movc %0,cacr"::"d"(cacr));
	}

	physmem = m68k_btop(mem_size);
}
#endif
