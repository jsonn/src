/*	$NetBSD: cpu.h,v 1.4.4.1 2000/12/24 07:18:24 jhawk Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
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
 * from: Utah $Hdr: cpu.h 1.16 91/03/25$
 *
 *	@(#)cpu.h	8.4 (Berkeley) 1/5/94
 */

#ifndef _NEWS68K_CPU_H_
#define _NEWS68K_CPU_H_

/*
 * Exported definitions unique to news68k cpu support.
 */

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_lockdebug.h"
#endif

/*
 * Get common m68k CPU definitions.
 */
#include <m68k/cpu.h>

#ifdef news1700
/*
 * XXX news1700 L2 cache would be corrupted with DC_BE and IC_BE...
 * XXX Should these be defined in machine/cpu.h?
 */
#undef CACHE_ON
#undef CACHE_CLR
#undef IC_CLEAR
#undef DC_CLEAR
#define CACHE_ON	(DC_WA|DC_CLR|DC_ENABLE|IC_CLR|IC_ENABLE)
#define CACHE_CLR	CACHE_ON
#define IC_CLEAR	(DC_WA|DC_ENABLE|IC_CLR|IC_ENABLE)
#define DC_CLEAR	(DC_WA|DC_CLR|DC_ENABLE|IC_ENABLE)

#define DCIC_CLR	(DC_CLR|IC_CLR)
#define CACHE_BE	(DC_BE|IC_BE)

#endif

/*
 * Get interrupt glue.
 */
#include <machine/intr.h>

#include <sys/sched.h>
struct cpu_info {
	struct schedstate_percpu ci_schedstate; /* scheduler state */
#if defined(DIAGNOSTIC) || defined(LOCKDEBUG)
	u_long ci_spin_locks;		/* # of spin locks held */
	u_long ci_simple_locks;		/* # of simple locks held */
#endif
};

#ifdef _KERNEL
extern struct cpu_info cpu_info_store;

#define	curcpu()			(&cpu_info_store)

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define cpu_swapin(p)			/* nothing */
#define cpu_wait(p)			/* nothing */
#define cpu_swapout(p)			/* nothing */
#define cpu_number()			0

/*
 * Arguments to hardclock and gatherstats encapsulate the previous
 * machine state in an opaque clockframe.  One the hp300, we use
 * what the hardware pushes on an interrupt (frame format 0).
 */
struct clockframe {
	u_short	sr;		/* sr at time of interrupt */
	u_long	pc;		/* pc at time of interrupt */
	u_short	vo;		/* vector offset (4-word frame) */
};

#define CLKF_USERMODE(framep)	(((framep)->sr & PSL_S) == 0)
#define CLKF_BASEPRI(framep)	(((framep)->sr & PSL_IPL) == 0)
#define CLKF_PC(framep)		((framep)->pc)
#if 0
/* We would like to do it this way... */
#define CLKF_INTR(framep)	(((framep)->sr & PSL_M) == 0)
#else
/* but until we start using PSL_M, we have to do this instead */
#define CLKF_INTR(framep)	(0)	/* XXX */
#endif


/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
extern int want_resched;	/* resched() was called */
#define need_resched()		do { want_resched++; aston(); } while(0)

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the hp300, request an ast to send us
 * through trap, marking the proc as needing a profiling tick.
 */
#define need_proftick(p)	do { (p)->p_flag |= P_OWEUPC; aston(); } while(0)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define signotify(p)	aston()

extern int astpending;		/* need to trap before returning to user mode */
extern volatile u_char *ctrl_ast;
#define aston()		do { astpending++; *ctrl_ast = 0xff; } while(0)

#endif /* _KERNEL */

/*
 * CTL_MACHDEP definitions.
 */
#define CPU_CONSDEV		1	/* dev_t: console terminal device */
#define CPU_MAXID		2	/* number of valid machdep ids */

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}

#ifdef _KERNEL

#if defined(news1700) || defined(news1200)
#ifndef M68030
#define M68030
#endif
#define M68K_MMU_MOTOROLA
#endif

#if defined(news1700)
#define CACHE_HAVE_PAC
#endif

#endif

#ifdef _KERNEL
extern int systype;
#define NEWS1700	0
#define NEWS1200	1

extern int cpuspeed;
extern char *intiobase, *intiolimit, *extiobase;
extern u_int intiobase_phys, intiotop_phys;
extern u_int extiobase_phys, extiotop_phys;
extern u_int intrcnt[];

extern void (*vectab[]) __P((void));

struct frame;
struct fpframe;
struct pcb;

/* locore.s functions */
void m68881_save __P((struct fpframe *));
void m68881_restore __P((struct fpframe *));
void DCIA __P((void));
void DCIS __P((void));
void DCIU __P((void));
void ICIA __P((void));
void ICPA __P((void));
void PCIA __P((void));
void TBIA __P((void));
void TBIS __P((vaddr_t));
void TBIAS __P((void));
void TBIAU __P((void));

int suline __P((caddr_t, caddr_t));
void savectx __P((struct pcb *));
void switch_exit __P((struct proc *));
void proc_trampoline __P((void));
void loadustp __P((int));
void badtrap __P((void));
void intrhand_vectored __P((void));
int getsr __P((void));


void doboot __P((int))
	__attribute__((__noreturn__));
void nmihand __P((struct frame *));
void ecacheon __P((void));
void ecacheoff __P((void));

/* machdep.c functions */
int badaddr __P((caddr_t, int));
int badbaddr __P((caddr_t));

/* sys_machdep.c functions */
int cachectl1 __P((unsigned long, vaddr_t, size_t, struct proc *));

/* vm_machdep.c functions */
void physaccess __P((caddr_t, caddr_t, int, int));
void physunaccess __P((caddr_t, int));
int kvtop __P((caddr_t));

/* trap.c functions */
void child_return __P((void *));

#endif

/* physical memory sections */
#define ROMBASE		(0xe0000000)

#define INTIOBASE1700	(0xe0c00000)
#define INTIOTOP1700	(0xe1d00000) /* XXX */
#define EXTIOBASE1700	(0xf0f00000)
#define EXTIOTOP1700	(0xf1000000) /* XXX */

#define INTIOBASE1200	(0xe1000000)
#define INTIOTOP1200	(0xe1d00000) /* XXX */
#define EXTIOBASE1200	(0xe4000000)
#define EXTIOTOP1200	(0xe4020000) /* XXX */

#define MAXADDR		(0xfffff000)

/*
 * Internal IO space:
 *
 * Internal IO space is mapped in the kernel from ``intiobase'' to
 * ``intiolimit'' (defined in locore.s).  Since it is always mapped,
 * conversion between physical and kernel virtual addresses is easy.
 */
#define ISIIOVA(va) \
	((char *)(va) >= intiobase && (char *)(va) < intiolimit)
#define IIOV(pa)	(((u_int)(pa) - intiobase_phys) + (u_int)intiobase)
#define ISIIOPA(pa) \
	((u_int)(pa) >= intiobase_phys && (u_int)(pa) < intiotop_phys)
#define IIOP(va)	(((u_int)(va) - (u_int)intiobase) + intiobase_phys)
#define IIOPOFF(pa)	((u_int)(pa) - intiobase_phys)

#endif /* !_NEWS68K_CPU_H_ */
