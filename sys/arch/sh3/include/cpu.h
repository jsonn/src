/*	$NetBSD: cpu.h,v 1.43.6.1 2007/05/27 12:28:08 ad Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc. All rights reserved.
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	@(#)cpu.h	5.4 (Berkeley) 5/9/91
 */

/*
 * SH3/SH4 support.
 *
 *  T.Horiuchi    Brains Corp.   5/22/98
 */

#ifndef _SH3_CPU_H_
#define	_SH3_CPU_H_

#if defined(_KERNEL_OPT)
#include "opt_lockdebug.h"
#endif

#include <sh3/psl.h>
#include <sh3/frame.h>

#ifdef _KERNEL
#include <sys/cpu_data.h>
struct cpu_info {
	struct cpu_data ci_data;	/* MI per-cpu data */
	int	ci_mtx_count;
	int	ci_mtx_oldspl;
};

extern struct cpu_info cpu_info_store;
#define	curcpu()			(&cpu_info_store)

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_number()			0
/*
 * Can't swapout u-area, (__SWAP_BROKEN)
 * since we use P1 converted address for trapframe.
 */
#define	cpu_swapin(p)			/* nothing */
#define	cpu_swapout(p)			panic("cpu_swapout: can't get here");
#define	cpu_proc_fork(p1, p2)		/* nothing */

/*
 * Arguments to hardclock and gatherstats encapsulate the previous
 * machine state in an opaque clockframe.
 */
struct clockframe {
	int	spc;	/* program counter at time of interrupt */
	int	ssr;	/* status register at time of interrupt */
	int	ssp;	/* stack pointer at time of interrupt */
};

#define	CLKF_USERMODE(cf)	(!KERNELMODE((cf)->ssr))
#define	CLKF_PC(cf)		((cf)->spc)
#define	CLKF_INTR(cf)		0	/* XXX */

/*
 * This is used during profiling to integrate system time.  It can safely
 * assume that the process is resident.
 */
#define	PROC_PC(p)							\
	(((struct trapframe *)(p)->p_md.md_regs)->tf_spc)

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
#define	cpu_need_resched(ci, flags)					\
do {									\
	want_resched = 1;						\
	if (curlwp != ci->ci_data.cpu_idlelwp)				\
		aston(curlwp);						\
} while (/*CONSTCOND*/0)

/*
 * MI code calls this with proper locking.
 */
#define	cpu_did_resched()						\
do {									\
	want_resched = 0;						\
} while (0)


/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the MIPS, request an ast to send us
 * through trap, marking the proc as needing a profiling tick.
 */
#define	cpu_need_proftick(l)						\
do {									\
	(l)->l_pflag |= LP_OWEUPC;					\
	aston(l);							\
} while (/*CONSTCOND*/0)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	cpu_signotify(l)	aston(l)

#define	aston(l)		((l)->l_md.md_astpending = 1)

extern int want_resched;		/* need_resched() was called */

/*
 * We need a machine-independent name for this.
 */
#define	DELAY(x)		delay(x)
#endif /* _KERNEL */

/*
 * Logical address space of SH3/SH4 CPU.
 */
#define	SH3_PHYS_MASK	0x1fffffff

#define	SH3_P0SEG_BASE	0x00000000	/* TLB mapped, also U0SEG */
#define	SH3_P0SEG_END	0x7fffffff
#define	SH3_P1SEG_BASE	0x80000000	/* pa == va */
#define	SH3_P1SEG_END	0x9fffffff
#define	SH3_P2SEG_BASE	0xa0000000	/* pa == va, non-cacheable */
#define	SH3_P2SEG_END	0xbfffffff
#define	SH3_P3SEG_BASE	0xc0000000	/* TLB mapped, kernel mode */
#define	SH3_P3SEG_END	0xdfffffff
#define	SH3_P4SEG_BASE	0xe0000000	/* peripheral space */
#define	SH3_P4SEG_END	0xffffffff

#define	SH3_P1SEG_TO_PHYS(x)	((uint32_t)(x) & SH3_PHYS_MASK)
#define	SH3_P2SEG_TO_PHYS(x)	((uint32_t)(x) & SH3_PHYS_MASK)
#define	SH3_PHYS_TO_P1SEG(x)	((uint32_t)(x) | SH3_P1SEG_BASE)
#define	SH3_PHYS_TO_P2SEG(x)	((uint32_t)(x) | SH3_P2SEG_BASE)
#define	SH3_P1SEG_TO_P2SEG(x)	((uint32_t)(x) | 0x20000000)
#define	SH3_P2SEG_TO_P1SEG(x)	((uint32_t)(x) & ~0x20000000)

#ifndef __lint__

/* switch from P1 to P2 */
#define	RUN_P2 do {							\
		void *p;						\
		p = &&P2;						\
		goto *(void *)SH3_P1SEG_TO_P2SEG(p);			\
	    P2:	(void)0;						\
	} while (0)

/* switch from P2 to P1 */
#define	RUN_P1 do {							\
		void *p;						\
		p = &&P1;						\
		__asm volatile("nop;nop;nop;nop;nop;nop;nop;nop");	\
		goto *(void *)SH3_P2SEG_TO_P1SEG(p);			\
	    P1:	(void)0;						\
	} while (0)

#else  /* __lint__ */
#define	RUN_P2	do {} while (/* CONSTCOND */ 0)
#define	RUN_P1	do {} while (/* CONSTCOND */ 0)
#endif

#if defined(SH4)
/* SH4 Processor Version Register */
#define	SH4_PVR_ADDR	0xff000030	/* P4  address */
#define	SH4_PVR		(*(volatile uint32_t *) SH4_PVR_ADDR)
#define	SH4_PRR_ADDR	0xff000044	/* P4  address */
#define	SH4_PRR		(*(volatile uint32_t *) SH4_PRR_ADDR)

#define	SH4_PVR_MASK	0xffffff00
#define	SH4_PVR_SH7750	0x04020500	/* SH7750  */
#define	SH4_PVR_SH7750S	0x04020600	/* SH7750S */
#define	SH4_PVR_SH775xR	0x04050000	/* SH775xR */
#define	SH4_PVR_SH7751	0x04110000	/* SH7751  */

#define	SH4_PRR_MASK	0xfffffff0
#define SH4_PRR_7750R	0x00000100	/* SH7750R */
#define SH4_PRR_7751R	0x00000110	/* SH7751R */
#endif

/*
 * pull in #defines for kinds of processors
 */
#include <machine/cputypes.h>

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_LOADANDRESET	2	/* load kernel image and reset */
#define	CPU_MAXID		3	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES {						\
	{ 0, 0 },							\
	{ "console_device",	CTLTYPE_STRUCT },			\
	{ "load_and_reset",	CTLTYPE_INT },				\
}

#ifdef _KERNEL
void sh_cpu_init(int, int);
void sh_startup(void);
void cpu_reset(void) __attribute__((__noreturn__)); /* soft reset */
void _cpu_spin(uint32_t);	/* for delay loop. */
void delay(int);
struct pcb;
void savectx(struct pcb *);
void dumpsys(void);
#endif /* _KERNEL */
#endif /* !_SH3_CPU_H_ */
