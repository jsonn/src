/*	$NetBSD: cpu.h,v 1.2.2.1 2000/11/20 20:24:29 bouyer Exp $	*/

/*-
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
 *	@(#)cpu.h	5.4 (Berkeley) 5/9/91
 */

/*
 * SH3 Version
 *
 *  T.Horiuchi    Brains Corp.   5/22/98
 */

#ifndef _SH3_CPU_H_
#define _SH3_CPU_H_

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_lockdebug.h"
#endif

/*
 * Definitions unique to sh3 cpu support.
 */
#include <machine/psl.h>
#include <machine/frame.h>
#include <machine/segments.h>

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
#endif

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_swapin(p)			/* nothing */
#define	cpu_wait(p)			/* nothing */
#define	cpu_number()			0

/*
 * Arguments to hardclock, softclock and statclock
 * encapsulate the previous machine state in an opaque
 * clockframe; for now, use generic intrframe.
 *
 * XXX intrframe has a lot of gunk we don't need.
 */
#define clockframe intrframe

#if 1
#define	CLKF_USERMODE(frame)	(!KERNELMODE((frame)->if_r15))
#else
#define	CLKF_USERMODE(frame)	USERMODE((frame)->if_spc, (frame)->if_ssr)
#endif
#if 0
#define	CLKF_BASEPRI(frame)	((frame)->if_pri == 0)
#else
/* XXX we should fix this */
#define	CLKF_BASEPRI(frame)	(0)
#endif
#define	CLKF_PC(frame)		((frame)->if_spc)
#define	CLKF_INTR(frame)	(0)	/* XXX should have an interrupt stack */

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
int	want_resched;		/* resched() was called */
#define	need_resched(ci)	(want_resched = 1, setsoftast())

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the i386, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, setsoftast())

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)		setsoftast()

/*
 * We need a machine-independent name for this.
 */
#define	DELAY(x)		delay(x)
void	delay __P((int));

/*
 * Logical address space of SH3 CPU.
 */
#define SH3_P0SEG_BASE	0x00000000
#define SH3_P0SEG_END	0x7fffffff
#define SH3_P1SEG_BASE	0x80000000	/* pa == va */
#define SH3_P1SEG_END	0x9fffffff
#define SH3_P2SEG_BASE	0xa0000000	/* pa == va, non-cacheable */
#define SH3_P2SEG_END	0xbfffffff
#define SH3_P3SEG_BASE	0xc0000000
#define SH3_P3SEG_END	0xdfffffff
#define SH3_P4SEG_BASE	0xe0000000
#define SH3_P4SEG_END	0xffffffff

#define SH3_PHYS_MASK	0x1fffffff
#define SH3_P1234SEG_SIZE	0x20000000

#define SH3_P1SEG_TO_PHYS(x)	((unsigned)(x) & SH3_PHYS_MASK)
#define SH3_P2SEG_TO_PHYS(x)	((unsigned)(x) & SH3_PHYS_MASK)
#define SH3_PHYS_TO_P1SEG(x)	((unsigned)(x) | SH3_P1SEG_BASE)
#define SH3_PHYS_TO_P2SEG(x)	((unsigned)(x) | SH3_P2SEG_BASE)
#define SH3_P1SEG_TO_P2SEG(x)	((unsigned)(x) | SH3_P1234SEG_SIZE)

/*
 * pull in #defines for kinds of processors
 */
#include <machine/cputypes.h>


#ifdef _KERNEL
extern int cpu;
extern int cpu_class;
extern struct cpu_nocpuid_nameclass sh3_nocpuid_cpus[];
extern struct cpu_cpuid_nameclass sh3_cpuid_cpus[];

/* autoconf.c */
void	configure __P((void));

/* sh3_machdep.c */
void sh3_startup __P((void));

/* machdep.c */
void	delay __P((int));
void	dumpconf __P((void));
void	cpu_reset __P((void));

/* locore.s */
struct region_descriptor;
void	lgdt __P((struct region_descriptor *));
void	fillw __P((short, void *, size_t));
void
bcopyb  __P((caddr_t from, caddr_t to, size_t len));
void
bcopyw __P((caddr_t from, caddr_t to, size_t len));
void
setPageDirReg __P((int pgdir));


struct pcb;
void	savectx __P((struct pcb *));
void	switch_exit __P((struct proc *));
void	proc_trampoline __P((void));

/* clock.c */
void	startrtclock __P((void));

/* npx.c */
void	npxdrop __P((void));
void	npxsave __P((void));

/* vm_machdep.c */
int kvtop __P((caddr_t));

#ifdef MATH_EMULATE
/* math_emulate.c */
int	math_emulate __P((struct trapframe *));
#endif

#endif /* _KERNEL */

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_NKPDE		2	/* int: number of kernel PDEs */
#define	CPU_BOOTED_KERNEL	3	/* string: booted kernel name */
#define	CPU_SETPRIVPROC		4	/* set current proc to piviledged proc
					   */
#define	CPU_DEBUGMODE		5	/* set debug mode */
#define	CPU_LOADANDRESET	6	/* load kernel image and reset */
#define	CPU_MAXID		7	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
	{ "nkpde", CTLTYPE_INT }, \
	{ "booted_kernel", CTLTYPE_STRING }, \
	{ "set_priv_proc", CTLTYPE_INT }, \
	{ "debug_mode", CTLTYPE_INT }, \
	{ "load_and_reset", CTLTYPE_INT }, \
}

#endif /* !_SH3_CPU_H_ */
