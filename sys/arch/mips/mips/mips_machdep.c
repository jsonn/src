/*	$NetBSD: mips_machdep.c,v 1.36.2.4 1998/11/16 10:41:32 nisimura Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Chris Demetriou.
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

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: mips_machdep.c,v 1.36.2.4 1998/11/16 10:41:32 nisimura Exp $");

#include "opt_compat_netbsd.h"
#include "opt_compat_ultrix.h"
#include "opt_uvm.h"
#include "opt_sysv.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/reboot.h>
#include <sys/mount.h>			/* fsid_t for syscallargs */
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/clist.h>  
#include <sys/callout.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/syscallargs.h>
#include <sys/user.h>
#include <sys/msgbuf.h>
#include <sys/conf.h>
#include <sys/core.h>
#include <sys/kcore.h>  
#include <machine/kcore.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm.h>

#if defined(UVM)
#include <uvm/uvm_extern.h>
#endif

#include <mips/regnum.h>		/* symbolic register indices */
#include <mips/locore.h>
#include <mips/psl.h>
#include <mips/pte.h>
#include <machine/cpu.h>		/* declaration of of cpu_id */

/* Internal routines. */
int	cpu_dumpsize __P((void));
u_long	cpu_dump_mempagecnt __P((void));
int	cpu_dump __P((void));

#ifdef MIPS1
static void	mips1_vector_init __P((void));
#endif

#ifdef MIPS3
static void	mips3_vector_init __P((void));
#endif


mips_locore_jumpvec_t mips_locore_jumpvec = {
  NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL,
  NULL, NULL
};

/*
 * Declare these as initialized data so we can patch them.
 */
#ifndef NBUF
#define NBUF		0
#endif
#ifndef BUFPAGES
#define BUFPAGES	0
#endif
#ifndef BUFCACHE
#define BUFCACHE	10
#endif

int	nswbuf = 0;
int	nbuf = NBUF;
int	bufpages = BUFPAGES;	/* optional hardwired count */
int	bufcache = BUFCACHE;	/* % of RAM to use for buffer cache */

int cpu_mhz;
int mips_num_tlb_entries;

#ifdef MIPS3
u_int	mips_L2CacheSize;
int	mips_L2CacheIsSnooping;	/* Set if L2 cache snoops uncached writes */
int	mips_L2CacheMixed;
#endif

struct	user *proc0paddr;
struct	proc nullproc;		/* for use by switch_exit() */
struct	proc *fpcurproc;
struct	pcb  *curpcb;

caddr_t	msgbufaddr;

#ifdef MIPS1
/*
 * MIPS-I (r2000 and r3000) locore-function vector.
 */
mips_locore_jumpvec_t mips1_locore_vec =
{
	mips1_ConfigCache,
	mips1_FlushCache,
	mips1_FlushDCache,
	mips1_FlushICache,
	/*mips1_FlushICache*/ mips1_FlushCache,
	mips1_SetPID,
	mips1_TLBFlush,
	mips1_TLBFlushAddr,
	mips1_TLBUpdate,
	mips1_wbflush,
	mips1_proc_trampoline,
	mips1_switch_exit,
	mips1_cpu_switch_resume
};

static void
mips1_vector_init()
{
	extern char mips1_UTLBMiss[], mips1_UTLBMissEnd[];
	extern char mips1_exception[], mips1_exceptionEnd[];

	/*
	 * Copy down exception vector code.
	 */
	if (mips1_UTLBMissEnd - mips1_UTLBMiss > 0x80)
		panic("startup: UTLB code too large");
	memcpy((void *)MIPS_UTLB_MISS_EXC_VEC, mips1_UTLBMiss,
		mips1_UTLBMissEnd - mips1_UTLBMiss);
	memcpy((void *)MIPS1_GEN_EXC_VEC, mips1_exception,
		mips1_exceptionEnd - mips1_exception);

	/*
	 * Copy locore-function vector.
	 */
	memcpy(&mips_locore_jumpvec, &mips1_locore_vec,
		sizeof(mips_locore_jumpvec_t));

	/*
	 * Clear out the I and D caches.
	 */
	mips1_ConfigCache();
	mips1_FlushCache();
}
#endif /* MIPS1 */


#ifdef MIPS3
/*
 * MIPS-III (r4000) locore-function vector.
 */
mips_locore_jumpvec_t mips3_locore_vec =
{
	mips3_ConfigCache,
	mips3_FlushCache,
	mips3_FlushDCache,
	mips3_FlushICache,
#if 0
	 /*
	  * No such vector exists, perhaps it was meant to be HitFlushDCache?
	  */
	mips3_ForceCacheUpdate,
#else
	mips3_FlushCache,
#endif
	mips3_SetPID,
	mips3_TLBFlush,
	mips3_TLBFlushAddr,
	mips3_TLBUpdate,
	mips3_wbflush,
	mips3_proc_trampoline,
	mips3_switch_exit,
	mips3_cpu_switch_resume
};

/*----------------------------------------------------------------------------
 *
 * mips3_ConfigCache --
 *
 *	Size the caches.
 *	NOTE: should only be called from mach_init().
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	The size of the data cache is stored into mips_L1DCacheSize.
 *	The size of instruction cache is stored into mips_L1ICacheSize.
 *	Alignment mask for cache aliasing test is stored in mips_CacheAliasMask.
 *
 * XXX: method to retrieve mips_L2CacheSize is port dependent.
 *
 *----------------------------------------------------------------------------
 */
void
mips3_ConfigCache()
{
	u_int32_t config = mips3_read_config();
	static int snoop_check = 0;
	register int i;

	mips_L1ICacheSize = MIPS3_CONFIG_CACHE_SIZE(config,
	    MIPS3_CONFIG_IC_MASK, MIPS3_CONFIG_IC_SHIFT);
	mips_L1ICacheLSize = MIPS3_CONFIG_CACHE_L1_LSIZE(config,
	    MIPS3_CONFIG_IB);
	mips_L1DCacheSize = MIPS3_CONFIG_CACHE_SIZE(config,
	    MIPS3_CONFIG_DC_MASK, MIPS3_CONFIG_DC_SHIFT);
	mips_L1DCacheLSize = MIPS3_CONFIG_CACHE_L1_LSIZE(config,
	    MIPS3_CONFIG_DB);

	mips_CacheAliasMask = (mips_L1DCacheLSize - 1) & ~(NBPG - 1);

	/*
	 * Clear out the I and D caches.
	 */
	mips_L2CacheSize = 0; /* kluge to skip L2 cache flush */
	mips3_FlushCache();

	i = *(volatile int *)&snoop_check;	/* Read and cache */
	mips3_FlushCache();			/* Flush */
	*(volatile int *)MIPS_PHYS_TO_KSEG1(MIPS_KSEG0_TO_PHYS(&snoop_check))
	    = ~i;				/* Write uncached */
	mips_L2CacheIsSnooping = *(volatile int *)&snoop_check == ~i;
	*(volatile int *)&snoop_check = i;	/* Write uncached */
	mips3_FlushCache();			/* Flush */


	mips_L2CachePresent = (config & MIPS3_CONFIG_SC) == 0;
	mips_L2CacheLSize = MIPS3_CONFIG_CACHE_L2_LSIZE(config);
	mips_L2CacheMixed = (config & MIPS3_CONFIG_SS) == 0;
}

static void
mips3_vector_init()
{

	/* r4000 exception handler address and end */
	extern char mips3_exception[], mips3_exceptionEnd[];

	/* TLB miss handler address and end */
	extern char mips3_TLBMiss[], mips3_TLBMissEnd[];

	/*
	 * Copy down exception vector code.
	 */
#if 0  /* XXX: this should be checked, if we will handle XTLB miss. */
	if (mips3_TLBMissEnd - mips3_TLBMiss > 0x80)
		panic("startup: UTLB code too large");
#endif
	memcpy((void *)MIPS_UTLB_MISS_EXC_VEC, mips3_TLBMiss,
	      mips3_TLBMissEnd - mips3_TLBMiss);

	memcpy((void *)MIPS3_GEN_EXC_VEC, mips3_exception,
	      mips3_exceptionEnd - mips3_exception);

	/*
	 * Copy locore-function vector.
	 */
	memcpy(&mips_locore_jumpvec, &mips3_locore_vec,
	      sizeof(mips_locore_jumpvec_t));

	/*
	 * Clear out the I and D caches.
	 */
	mips3_ConfigCache();

#ifdef __pmax__		/* XXX */
	mips_L2CachePresent = 1;
	mips_L2CacheSize = 1024 * 1024;
#endif
#ifdef __arc__		/* XXX */
	mips_L2CacheSize = mips_L2CachePresent ? 1024 * 1024 : 0;
#endif
#ifdef	GEO		/* XXX */
	mips_L2_CacheSize = mips_L2CachePresent = 0;
#endif

	mips3_FlushCache();
}
#endif	/* MIPS3 */


/*
 * Do all the stuff that locore normally does before calling main(),
 * that is common to all mips-CPU NetBSD ports.
 *
 * The principal purpose of this function is to examine the
 * variable cpu_id, into which the kernel locore start code
 * writes the cpu ID register, and to then copy appropriate
 * cod into the CPU exception-vector entries and the jump tables
 * used to  hide the differences in cache and TLB handling in
 * different MIPS CPUs.
 * 
 * This should be the very first thing called by each port's
 * init_main() function.
 */

/*
 * Initialize the hardware exception vectors, and the jump table used to
 * call locore cache and TLB management functions, based on the kind
 * of CPU the kernel is running on.
 */
void
mips_vector_init()
{
	int i;

	(void) &i;		/* shut off gcc unused-variable warnings */

	/*
	 * Copy exception-dispatch code down to exception vector.
	 * Initialize locore-function vector.
	 * Clear out the I and D caches.
	 */

	switch (cpu_id.cpu.cp_imp) {
#ifdef MIPS1
	case MIPS_R2000:
	case MIPS_R3000:
		cpu_arch = 1;
		mips_num_tlb_entries = MIPS1_TLB_NUM_TLB_ENTRIES;
		break;
#endif /* MIPS1 */

#ifdef MIPS3
	case MIPS_R4000:
		cpu_arch = 3;
		mips_num_tlb_entries = MIPS3_TLB_NUM_TLB_ENTRIES;
		mips3_L1TwoWayCache = 0;
		mips3_cacheflush_bug = 0;
#if 1  /* XXX FIXME: avoid hangs in mips3_vector_init() */
		mips3_cacheflush_bug = 1;
#endif
		break;
	case MIPS_R4300:
		cpu_arch = 3;
		mips_num_tlb_entries = MIPS_R4300_TLB_NUM_TLB_ENTRIES;
		mips3_L1TwoWayCache = 0;
		mips3_cacheflush_bug = 0;
		break;
	case MIPS_R4600:
		cpu_arch = 3;
		mips_num_tlb_entries = MIPS3_TLB_NUM_TLB_ENTRIES;
		mips3_L1TwoWayCache = 1;
		/* disable interrupt while cacheflush to workaround the bug */
		mips3_cacheflush_bug = 1;	/* R4600 only??? */
		break;
#ifdef ENABLE_MIPS_R4700 /* ID conflict */
	case MIPS_R4700:
		cpu_arch = 3;
		mips_num_tlb_entries = MIPS3_TLB_NUM_TLB_ENTRIES;
		mips3_L1TwoWayCache = 1;
		mips3_cacheflush_bug = 0;
		break;
#endif
#ifndef ENABLE_MIPS_R3NKK /* ID conflict */
	case MIPS_R5000:
#endif
	case MIPS_RM5230:
		cpu_arch = 4; 
		mips_num_tlb_entries = MIPS3_TLB_NUM_TLB_ENTRIES;
		mips3_L1TwoWayCache = 1;
		mips3_cacheflush_bug = 0;
		break;
#endif /* MIPS3 */

	default:
		printf("CPU type (%d) not supported\n", cpu_id.cpu.cp_imp);
		cpu_reboot(RB_HALT, NULL);
	}

	switch (cpu_arch) {
#ifdef MIPS1
	case 1:
		mips1_TLBFlush(MIPS1_TLB_NUM_TLB_ENTRIES);
		for (i = 0; i < MIPS1_TLB_FIRST_RAND_ENTRY; ++i)
			mips1_TLBWriteIndexed(i, MIPS_KSEG0_START, 0);
		mips1_vector_init();
		break;
#endif
#if (MIPS3 + MIPS4) > 0
	case 3:
	case 4:
		mips3_SetWIRED(0);
		mips3_TLBFlush(mips_num_tlb_entries);
		mips3_SetWIRED(MIPS3_TLB_WIRED_ENTRIES);
		mips3_vector_init();
		break;
#endif
	default:
		printf("MIPS ISA %d: not supported\n", cpu_arch);
		cpu_reboot(RB_HALT, NULL);
	}
}

void
mips_set_wbflush(flush_fn)
	void (*flush_fn) __P((void));
{
#undef wbflush
	mips_locore_jumpvec.wbflush = flush_fn;
	(*flush_fn)();
}

struct pridtab {
	int	cpu_imp;
	char	*cpu_name;
	int	cpu_isa;
};
struct pridtab cputab[] = {
	{ MIPS_R2000,	"MIPS R2000 CPU",	1 },
	{ MIPS_R3000,	"MIPS R3000 CPU",	1 },
	{ MIPS_R6000,	"MIPS R6000 CPU",	2 },
	{ MIPS_R4000,	"MIPS R4000 CPU",	3 },
	{ MIPS_R3LSI,	"LSI Logic R3000 derivative", 1 },
	{ MIPS_R6000A,	"MIPS R6000A CPU",	2 },
	{ MIPS_R3IDT,	"IDT R3041 or RC36100 CPU", 1 },
	{ MIPS_R10000,	"MIPS R10000/T5 CPU",	4 },
	{ MIPS_R4200,	"NEC VR4200 CPU",	3 },
	{ MIPS_R4300,	"NEC VR4300 CPU",	3 },
	{ MIPS_R4100,	"NEC VR4100 CPU",	3 },
	{ MIPS_R8000,	"MIPS R8000 Blackbird/TFP CPU", 4 },
	{ MIPS_R4600,	"QED R4600 Orion CPU",	3 },
	{ MIPS_R4700,	"QED R4700 Orion CPU",	3 },
	{ MIPS_TX3900,	"Toshiba TX3900 or QED R4650 CPU", 1 }, /* see below */
	{ MIPS_R5000,	"MIPS R5000 CPU",	4 },
	{ MIPS_RC32364,	"IDT RC32364 CPU",	3 },
	{ MIPS_RM5230,	"QED RM5200 CPU",	3 },
	{ MIPS_RC64470,	"IDT RC64474/RC64475 CPU",	3 },
#if 0 /* ID crashs */
	/*
	 * According to documents from Toshiba and QED, PRid 0x22 is
	 * used by both of TX3900 (ISA-I) and QED4640/4650 (ISA-III).
	 * Two PRid conflicts below have not been confirmed this time.
	 */
	{ MIPS_R3SONY,	"SONY R3000 derivative", 1},  /* 0x21; crash R4700? */
	{ MIPS_R3NKK,	"NKK R3000 derivative",	1},   /* 0x23; crash R5000? */
#endif
};
struct pridtab fputab[] = {
	{ MIPS_SOFT,	"software emulated floating point", },
	{ MIPS_R2360,	"MIPS R2360 Floating Point Board", },
	{ MIPS_R2010,	"MIPS R2010 FPC", },
	{ MIPS_R3010,	"MIPS R3010 FPC", },
	{ MIPS_R6010,	"MIPS R6010 FPC", },
	{ MIPS_R4010,	"MIPS R4010 FPC", },
	{ MIPS_R4210,	"NEC VR4210 FPC", },
	{ MIPS_R5010,	"MIPS R5010 FPC", },
};

/*
 * Identify product revision IDs of cpu and fpu.
 */
void
cpu_identify()
{
	int i;
	char *cpuname, *fpuname;

	cpuname = NULL;
	for (i = 0; i < sizeof(cputab)/sizeof(cputab[0]); i++) {
		if (cpu_id.cpu.cp_imp == cputab[i].cpu_imp) {
			cpuname = cputab[i].cpu_name;
			break;
		}
	}
	if (cpu_id.cpu.cp_imp == MIPS_R4000 && mips_L1ICacheSize == 16384)
		cpuname = "MIPS R4400 CPU";

	fpuname = NULL;
	for (i = 0; i < sizeof(fputab)/sizeof(fputab[0]); i++) {
		if (fpu_id.cpu.cp_imp == fputab[i].cpu_imp) {
			fpuname = fputab[i].cpu_name;
			break;
		}
	}
	if (fpuname == NULL && fpu_id.cpu.cp_imp == cpu_id.cpu.cp_imp)
		fpuname = "built in floating point processor";
	if (cpu_id.cpu.cp_imp == MIPS_R4700)	/* FPU PRid is 0x20 */
		fpuname = "built in floating point processor";
	if (cpu_id.cpu.cp_imp == MIPS_RC64470)	/* FPU PRid is 0x21 */
		fpuname = "built in floating point processor";

	printf("cpu0: ");
	if (cpuname != NULL)
		printf(cpuname);
	else
		printf("unknown CPU type (0x%x)", cpu_id.cpu.cp_imp);
	printf(" Rev. %d.%d", cpu_id.cpu.cp_majrev, cpu_id.cpu.cp_minrev);

	if (fpuname != NULL)
		printf(" with %s", fpuname);
	else
		printf(" with unknown FPC type (0x%x)", fpu_id.cpu.cp_imp);
	printf(" Rev. %d.%d", fpu_id.cpu.cp_majrev, fpu_id.cpu.cp_minrev);
	printf("\n");

	printf("cpu0: ");
#ifdef MIPS1
	if (cpu_arch == 1) {
		printf("%dkb Instruction, %dkb Data, direct mapped cache",
		    mips_L1ICacheSize / 1024, mips_L1DCacheSize / 1024);	
	}
#endif
#ifdef MIPS3
	if (cpu_arch >= 3) {
		printf("L1 cache: %dkb/%db Instruction, %dkb/%db Data",
		    mips_L1ICacheSize / 1024, mips_L1ICacheLSize, 
		    mips_L1DCacheSize / 1024, mips_L1DCacheLSize);
		if (mips3_L1TwoWayCache)
			printf(", two way set associative");
		else
			printf(", direct mapped");
		printf("\n");
		printf("cpu0: ");
		if (!mips_L2CachePresent) {
			printf("No L2 cache");
		}
		else {
			printf("L2 cache: ");
			if (mips_L2CacheSize)
				printf("%dkb", mips_L2CacheSize / 1024);
			else
				printf("unknown size");
			printf("/%db %s, %s",
			    mips_L2CacheLSize,
			    mips_L2CacheMixed ? "mixed" : "separated",
			    mips_L2CacheIsSnooping? "snooping" : "no snooping");
		}
	}
#endif
	printf("\n");

#ifdef MIPS3
	/*
	 * sanity check.
	 * good place to do this is mips_vector_init(),
	 * but printf() doesn't work in it.
	 */
#if !defined(MIPS3_FLUSH)
	if (cpu_arch == 3 && !mips_L2CachePresent) {
		printf("This kernel doesn't work without L2 cache.\n"
		    "Please add \"options MIPS3_FLUSH\""
		    "to the kernel config file.\n");
		cpu_reboot(RB_HALT, NULL);
	}
#endif
	if (mips3_L1TwoWayCache &&
	    (mips_L1ICacheLSize < 32 || mips_L1DCacheLSize < 32)) {
		/*
		 * current implementation of mips3_FlushCache(),
		 * mips3_FlushICache(), mips3_FlushDCache() and
		 * mips3_HitFlushDCache() assume that
		 * if the CPU has two way L1 cache, line size >= 32.
		 */
		printf("L1 cache: two way, but Inst/Data line size = %d/%d\n",
		    mips_L1ICacheLSize, mips_L1DCacheLSize);
		printf("Please fix implementation of mips3_*Flush*Cache\n");
		cpu_reboot(RB_HALT, NULL);
	}
	if (mips_L2CachePresent && mips_L2CacheLSize < 32) {
		/*
		 * current implementation of mips3_FlushCache(),
		 * mips3_FlushDCache() and mips3_HitFlushDCache() assume
		 * that if the CPU has L2 cache, line size >= 32.
		 */
		printf("L2 cache line size = %d\n", mips_L2CacheLSize);
		printf("Please fix implementation of mips3_*Flush*Cache\n");
		cpu_reboot(RB_HALT, NULL);
	}
#endif
	/* XXX cache sizes for MIPS1? */
	/* XXX hardware mcclock CPU-speed computation */
}

/*
 * Set registers on exec.
 * Clear all registers except sp, pc, and t9.
 * $sp is set to the stack pointer passed in.  $pc is set to the entry
 * point given by the exec_package passed in, as is $t9 (used for PIC
 * code by the MIPS elf abi).
 */
void
setregs(p, pack, stack)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
{
	extern struct proc *fpcurproc;

	memset(p->p_md.md_regs, 0, sizeof(struct frame));
	memset(&p->p_addr->u_pcb.pcb_fpregs, 0, sizeof(struct fpreg));
	p->p_md.md_regs[SP] = stack;
	p->p_md.md_regs[PC] = pack->ep_entry & ~3;
	p->p_md.md_regs[T9] = pack->ep_entry & ~3; /* abicall requirement */
	p->p_md.md_regs[SR] = PSL_USERSET;
	p->p_md.md_flags &= ~MDP_FPUSED;
	if (fpcurproc == p)
		fpcurproc = (struct proc *)0;
	p->p_md.md_ss_addr = 0;

	/*
	 * Set up arguments for the dld-capable crt0:
	 *
	 *	a0	stack pointer
	 *	a1	rtld cleanup (filled in by dynamic loader)
	 *	a2	rtld object (filled in by dynamic loader)
	 *	a3	ps_strings
	 */
	p->p_md.md_regs[A0] = stack;
	p->p_md.md_regs[A1] = 0;
	p->p_md.md_regs[A2] = 0;
	p->p_md.md_regs[A3] = (u_long)PS_STRINGS;
}

/*
 * WARNING: code in locore.s assumes the layout shown for sf_signum
 * thru sf_handler so... don't screw with them!
 */
struct sigframe {
	int	sf_signum;		/* signo for handler */
	int	sf_code;		/* additional info for handler */
	struct	sigcontext *sf_scp;	/* context ptr for handler */
	sig_t	sf_handler;		/* handler addr for u_sigc */
	struct	sigcontext sf_sc;	/* actual context */
};

#ifdef DEBUG
int sigdebug = 0;
int sigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig;
	sigset_t *mask;
	u_long code;
{
	struct proc *p = curproc;
	struct sigframe *fp;
	mips_reg_t *regs;
	struct sigacts *psp = p->p_sigacts;
	int onstack;
	struct sigcontext ksc;

	regs = p->p_md.md_regs;

	/* Do we need to jump onto the signal stack? */
	onstack = 
	    (psp->ps_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0 &&
	    (psp->ps_sigact[sig].sa_flags & SA_ONSTACK) != 0;

	/* Allocate space for the signal handler context. */
	if (onstack)
		fp = (struct sigframe *)((caddr_t)psp->ps_sigstk.ss_sp +
						  psp->ps_sigstk.ss_size);
	else
		fp = (struct sigframe *)regs[SP];
	fp--;

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid))
		printf("sendsig(%d): sig %d ssp %p usp %p scp %p\n",
		       p->p_pid, sig, &onstack, fp, &fp->sf_sc);
#endif

	/* Build stack frame for signal trampoline. */
	ksc.sc_pc = regs[PC];
	ksc.mullo = regs[MULLO];
	ksc.mulhi = regs[MULHI];

	/* Save register context. */
	ksc.sc_regs[ZERO] = 0xACEDBADE;		/* magic number */
	memcpy(&ksc.sc_regs[1], &regs[1],
	    sizeof(ksc.sc_regs) - sizeof(ksc.sc_regs[0]));

	/* Save the floating-pointstate, if necessary, then copy it. */
	ksc.sc_fpused = p->p_md.md_flags & MDP_FPUSED;
	if (ksc.sc_fpused) {
		extern struct proc *fpcurproc;

		/* if FPU has current state, save it first */
		if (p == fpcurproc)
			savefpregs(p);
		*(struct fpreg *)ksc.sc_fpregs = p->p_addr->u_pcb.pcb_fpregs;
	}

	/* Save signal stack. */
	ksc.sc_onstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/* Save signal mask. */
	ksc.sc_mask = *mask;

#if defined(COMPAT_13) || defined(COMPAT_ULTRIX)
	/*
	 * XXX We always have to save an old style signal mask because
	 * XXX we might be delivering a signal to a process which will
	 * XXX escape from the signal in a non-standard way and invoke
	 * XXX sigreturn() directly.
	 */
	native_sigset_to_sigset13(mask, &ksc.__sc_mask13);
#endif

	if (copyout(&ksc, &fp->sf_sc, sizeof(ksc))) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG
		if ((sigdebug & SDB_FOLLOW) ||
		    ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid))
			printf("sendsig(%d): copyout failed on sig %d\n",
			    p->p_pid, sig);
#endif
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/* Set up the registers to return to sigcode. */
	regs[A0] = sig;
	regs[A1] = code;
	regs[A2] = (int)&fp->sf_sc;
	regs[A3] = (int)catcher;

	regs[PC] = (int)catcher;
	regs[T9] = (int)catcher;
	regs[SP] = (int)fp;

	/* Signal trampoline code is at base of user stack. */
	regs[RA] = (int)psp->ps_sigcode;

	/* Remember that we're now on the signal stack. */
	if (onstack)
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid))
		printf("sendsig(%d): sig %d returns\n",
		       p->p_pid, sig);
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper priviledges or to cause
 * a machine fault.
 */
/* ARGSUSED */
int
sys___sigreturn14(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys___sigreturn14_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext *scp;
	mips_reg_t error, *regs;
	struct sigcontext ksc;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: pid %d, scp %p\n", p->p_pid, scp);
#endif
	if ((error = copyin(scp, &ksc, sizeof(ksc))) != 0)
		return (error);

	if (ksc.sc_regs[ZERO] != 0xACEDBADE)		/* magic number */
		return (EINVAL);

	/* Resture the register context. */
	regs = p->p_md.md_regs;
	regs[PC] = ksc.sc_pc;
	regs[MULLO] = ksc.mullo;
	regs[MULHI] = ksc.mulhi;
	memcpy(&regs[1], &scp->sc_regs[1],
	    sizeof(scp->sc_regs) - sizeof(scp->sc_regs[0]));
	if (scp->sc_fpused)
		p->p_addr->u_pcb.pcb_fpregs = *(struct fpreg *)scp->sc_fpregs;

	/* Restore signal stack. */
	if (ksc.sc_onstack & SS_ONSTACK)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask. */
	(void) sigprocmask1(p, SIG_SETMASK, &ksc.sc_mask, 0);

	return (EJUSTRETURN);
}

/*
 * These are imported from platform-specific code.
 */
extern phys_ram_seg_t mem_clusters[];
extern int mem_cluster_cnt;

/*
 * These are imported from pmap.c
 */
extern pt_entry_t *Sysmap;
extern u_int Sysmapsize;

/*
 * These variables are needed by /sbin/savecore.
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int	dumpsize = 0;		/* pages */
long	dumplo = 0;		/* blocks */

struct user dumppcb;		/* Actually, struct pcb would do. */

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
 * cpu_dump_mempagecnt: calculate size of RAM (in pages) to be dumped.
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
 * cpu_dump: dump machine-dependent kernel core dump headers.
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
	if (CPUISMIPS3) {
		cpuhdrp->archlevel = 3;
		cpuhdrp->pg_shift  = MIPS3_PG_SHIFT;
		cpuhdrp->pg_frame  = MIPS3_PG_FRAME;
		cpuhdrp->pg_v      = MIPS3_PG_V;
	} else {
		cpuhdrp->archlevel = 1;
		cpuhdrp->pg_shift  = MIPS1_PG_SHIFT;
		cpuhdrp->pg_frame  = MIPS1_PG_FRAME;
		cpuhdrp->pg_v      = MIPS1_PG_V;
	}
	cpuhdrp->sysmappa   = MIPS_KSEG0_TO_PHYS(Sysmap);
	cpuhdrp->sysmapsize = Sysmapsize;
	cpuhdrp->nmemsegs   = mem_cluster_cnt;

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
 * Dumps always skip the first CLBYTES of disk space
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
 * Dump the kernel's image to the swap partition.
 */
#define	BYTES_PER_DUMP	NBPG

void
dumpsys()
{
	u_long totalbytesleft, bytes, i, n, memcl;
	u_long maddr;
	int psize;
	daddr_t blkno;
	int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	int error;

	/* Save registers. */
	savectx(&dumppcb);

	msgbufenabled = 0;	/* don't record dump msgs in msgbuf */
	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();
	if (dumplo <= 0) {
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

	/* XXX should purge all outstanding keystrokes. */

	if ((error = cpu_dump()) != 0)
		goto err;

	totalbytesleft = ptoa(cpu_dump_mempagecnt());
	blkno = dumplo + cpu_dumpsize();
	dump = bdevsw[major(dumpdev)].d_dump;
	error = 0;

	for (memcl = 0; memcl < mem_cluster_cnt; memcl++) {
		maddr = mem_clusters[memcl].start;
		bytes = mem_clusters[memcl].size;

		for (i = 0; i < bytes; i += n, totalbytesleft -= n) {
			/* Print out how many MBs we have left to go. */
			if ((totalbytesleft % (1024*1024)) == 0)
				printf("%ld ", totalbytesleft / (1024 * 1024));

			/* Limit size for next transfer. */
			n = bytes - i;
			if (n > BYTES_PER_DUMP)
				n = BYTES_PER_DUMP;

			error = (*dump)(dumpdev, blkno,
			    (caddr_t)MIPS_PHYS_TO_KSEG0(maddr), n);
			if (error)
				goto err;
			maddr += n;
			blkno += btodb(n);		/* XXX? */

			/* XXX should look for keystrokes, to cancel. */
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
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way, we set each data structure pointer.
 *
 * We call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and the call
 * allocsys() again with the correct base virtual address.
 */
caddr_t  
allocsys(v) 
	caddr_t v;
{                       

#define valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)ALIGN((name)+(num))

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
	 * Determine how many buffers to allocate.
	 * We allocate bufcache % of memory for buffer space.  Ensure a
	 * minimum of 16 buffers.  We allocate 1/2 as many swap buffer
	 * headers as file i/o buffers.
	 */
	if (bufpages == 0)
		bufpages = physmem / CLSIZE * bufcache / 100;
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
#if !defined(UVM)
	valloc(swbuf, struct buf, nswbuf);
#endif
	valloc(buf, struct buf, nbuf);

	return (v);
#undef valloc
}

void
mips_init_msgbuf()
{
	size_t sz = round_page(MSGBUFSIZE);
	struct vm_physseg *vps;

	vps = &vm_physmem[vm_nphysseg - 1];

	/* shrink so that it'll fit in the last segment */
	if ((vps->avail_end - vps->avail_start) < atop(sz))
		sz = ptoa(vps->avail_end - vps->avail_start);

	vps->end -= atop(sz);
	vps->avail_end -= atop(sz);
	msgbufaddr = (caddr_t) MIPS_PHYS_TO_KSEG0(ptoa(vps->end));
	initmsgbuf(msgbufaddr, sz);

	/* Remove the last segment if it now has no pages. */
	if (vps->start == vps->end)
		vm_nphysseg--;

	/* warn if the message buffer had to be shrunk */
	if (sz != round_page(MSGBUFSIZE))
		printf("WARNING: %ld bytes not available for msgbuf "
		    "in last cluster (%d used)\n",
		    round_page(MSGBUFSIZE), sz);
}

/*
 * Initialize the U-area for proc0 and for nullproc.  Since these
 * need to be set up before we can probe for memory, we have to use
 * stolen pages before they're loaded into the VM system.
 *
 * "space" is 2 * USPACE in size, must be page aligned,
 * and in KSEG0.
 */
void
mips_init_proc0(space)
	caddr_t space;
{
	struct tlb tlb;
	u_long pa;
	int i;

	memset(space, 0, 2 * USPACE);

	proc0.p_addr = proc0paddr = (struct user *)space;
	proc0.p_md.md_regs = proc0paddr->u_pcb.pcb_regs;
	curpcb = &proc0.p_addr->u_pcb;

	pa = MIPS_KSEG0_TO_PHYS(proc0.p_addr);

	MachSetPID(1);

	if (CPUISMIPS3) {
		for (i = 0; i < UPAGES; i += 2) {
			tlb.tlb_mask = MIPS3_PG_SIZE_4K;
			tlb.tlb_hi = mips3_vad_to_vpn((UADDR +
			    (i << PGSHIFT))) | 1;
			tlb.tlb_lo0 = vad_to_pfn(pa) |
			    MIPS3_PG_V | MIPS3_PG_M | MIPS3_PG_CACHED;
			tlb.tlb_lo1 = vad_to_pfn(pa + PAGE_SIZE) |
			    MIPS3_PG_V | MIPS3_PG_M | MIPS3_PG_CACHED;
			proc0.p_md.md_upte[i] = tlb.tlb_lo0;
			proc0.p_md.md_upte[i + 1] = tlb.tlb_lo1;
			mips3_TLBWriteIndexedVPS(i, &tlb);
			pa += PAGE_SIZE * 2;
		}

		mips3_FlushDCache(MIPS_KSEG0_TO_PHYS(proc0.p_addr), USPACE);
		mips3_HitFlushDCache(UADDR, USPACE);
	} else {
		for (i = 0; i < UPAGES; i++) {
			proc0.p_md.md_upte[i] =
			    pa | MIPS1_PG_V | MIPS1_PG_M;
			mips1_TLBWriteIndexed(i, (UADDR + (i << PGSHIFT)) |
			    (1 << MIPS1_TLB_PID_SHIFT),
			    proc0.p_md.md_upte[i]);
			pa += PAGE_SIZE;
		}
	}

	nullproc.p_addr = (struct user *)(space + USPACE);
	nullproc.p_md.md_regs = nullproc.p_addr->u_pcb.pcb_regs;

	memcpy(nullproc.p_comm, "nullproc", sizeof("nullproc"));

	pa = MIPS_KSEG0_TO_PHYS(nullproc.p_addr);

	if (CPUISMIPS3) {
		for (i = 0; i < UPAGES; i += 2) {
			nullproc.p_md.md_upte[i] = vad_to_pfn(pa) |
			    MIPS3_PG_V | MIPS3_PG_M | MIPS3_PG_CACHED;
			nullproc.p_md.md_upte[i + 1] =
			    vad_to_pfn(pa + PAGE_SIZE) |
			    MIPS3_PG_V | MIPS3_PG_M | MIPS3_PG_CACHED;
			pa += PAGE_SIZE * 2;
		}
		mips3_FlushDCache(MIPS_KSEG0_TO_PHYS(nullproc.p_addr), USPACE);
	} else {
		for (i = 0; i < UPAGES; i++) {
			nullproc.p_md.md_upte[i] = pa | MIPS1_PG_V | MIPS1_PG_M;
			pa += PAGE_SIZE;
		}
	}
}
