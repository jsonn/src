/*	$NetBSD: cpu.h,v 1.44.2.1 2002/01/10 19:48:50 thorpej Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)cpu.h	8.4 (Berkeley) 1/5/94
 */

#ifndef _CPU_H_
#define _CPU_H_

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_BOOTED_KERNEL	1	/* string: booted kernel name */
#define	CPU_MAXID		2	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES {			\
	{ 0, 0 },				\
	{ "booted_kernel", CTLTYPE_STRING },	\
}

#ifdef _KERNEL
/*
 * Exported definitions unique to SPARC cpu support.
 */

#if !defined(_LKM) && defined(_KERNEL_OPT)
#include "opt_multiprocessor.h"
#include "opt_lockdebug.h"
#include "opt_sparc_arch.h"
#endif

#include <machine/psl.h>
#include <machine/intr.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/sparc/intreg.h>

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	curcpu()		(cpuinfo.ci_self)
#define	curproc			(curcpu()->ci_curproc)
#define	CPU_IS_PRIMARY(ci)	((ci)->master)

#define	cpu_swapin(p)	/* nothing */
#define	cpu_swapout(p)	/* nothing */
#define	cpu_wait(p)	/* nothing */
#define	cpu_number()	(cpuinfo.ci_cpuid)

#if defined(MULTIPROCESSOR)
void	cpu_boot_secondary_processors __P((void));
#endif

/*
 * Arguments to hardclock, softclock and gatherstats encapsulate the
 * previous machine state in an opaque clockframe.  The ipl is here
 * as well for strayintr (see locore.s:interrupt and intr.c:strayintr).
 * Note that CLKF_INTR is valid only if CLKF_USERMODE is false.
 */
struct clockframe {
	u_int	psr;		/* psr before interrupt, excluding PSR_ET */
	u_int	pc;		/* pc at interrupt */
	u_int	npc;		/* npc at interrupt */
	u_int	ipl;		/* actual interrupt priority level */
	u_int	fp;		/* %fp at interrupt */
};
typedef struct clockframe clockframe;

extern int eintstack[];

#define	CLKF_USERMODE(framep)	(((framep)->psr & PSR_PS) == 0)
#define	CLKF_BASEPRI(framep)	(((framep)->psr & PSR_PIL) == 0)
#define	CLKF_PC(framep)		((framep)->pc)
#if defined(MULTIPROCESSOR)
#define	CLKF_INTR(framep)						\
	((framep)->fp > (u_int)cpuinfo.eintstack - INT_STACK_SIZE &&	\
	 (framep)->fp < (u_int)cpuinfo.eintstack)
#else
#define	CLKF_INTR(framep)	((framep)->fp < (u_int)eintstack)
#endif

#if defined(SUN4M)
extern void	raise __P((int, int));
#if !(defined(SUN4) || defined(SUN4C))
#define setsoftint()	raise(0,1)
#else /* both defined */
#define setsoftint()	(cputyp == CPU_SUN4M ? raise(0,1) : ienab_bis(IE_L1))
#endif /* !4,!4c */
#else	/* 4m not defined */
#define setsoftint()	ienab_bis(IE_L1)
#endif /* SUN4M */

void	softintr_init __P((void));
void	*softnet_cookie;

#define setsoftnet()	softintr_schedule(softnet_cookie);

extern int	want_ast;

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
extern int	want_resched;		/* resched() was called */
#define	need_resched(ci)		(want_resched = 1, want_ast = 1)

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the sparc, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, want_ast = 1)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)		(want_ast = 1)

/* Number of CPUs in the system */
extern int ncpu;

/*
 * Only one process may own the FPU state.
 *
 * XXX this must be per-cpu (eventually)
 */
extern struct proc *fpproc;	/* FPU owner */
extern int foundfpu;		/* true => we have an FPU */

/*
 * Interrupt handler chains.  Interrupt handlers should return 0 for
 * ``not me'' or 1 (``I took care of it'').  intr_establish() inserts a
 * handler into the list.  The handler is called with its (single)
 * argument, or with a pointer to a clockframe if ih_arg is NULL.
 */
extern struct intrhand {
	int	(*ih_fun) __P((void *));
	void	*ih_arg;
	struct	intrhand *ih_next;
} *intrhand[15];

void	intr_establish __P((int level, struct intrhand *));
void	intr_disestablish __P((int level, struct intrhand *));

/*
 * intr_fasttrap() is a lot like intr_establish, but is used for ``fast''
 * interrupt vectors (vectors that are not shared and are handled in the
 * trap window).  Such functions must be written in assembly.
 */
void	intr_fasttrap __P((int level, void (*vec)(void)));

void	intr_lock_kernel __P((void));
void	intr_unlock_kernel __P((void));

/* disksubr.c */
struct dkbad;
int isbad __P((struct dkbad *bt, int, int, int));
/* machdep.c */
int	ldcontrolb __P((caddr_t));
void	dumpconf __P((void));
caddr_t	reserve_dumppages __P((caddr_t));
/* clock.c */
struct timeval;
void	lo_microtime __P((struct timeval *));
int	clockintr __P((void *));/* level 10 (clock) interrupt code */
int	statintr __P((void *));	/* level 14 (statclock) interrupt code */
/* locore.s */
struct fpstate;
void	savefpstate __P((struct fpstate *));
void	loadfpstate __P((struct fpstate *));
int	probeget __P((caddr_t, int));
void	write_all_windows __P((void));
void	write_user_windows __P((void));
void 	proc_trampoline __P((void));
void	switchexit __P((struct proc *));
struct pcb;
void	snapshot __P((struct pcb *));
struct frame *getfp __P((void));
int	xldcontrolb __P((caddr_t, struct pcb *));
void	copywords __P((const void *, void *, size_t));
void	qcopy __P((const void *, void *, size_t));
void	qzero __P((void *, size_t));
/* trap.c */
void	kill_user_windows __P((struct proc *));
int	rwindow_save __P((struct proc *));
/* amd7930intr.s */
void	amd7930_trap __P((void));
/* cons.c */
int	cnrom __P((void));
/* zs.c */
void zsconsole __P((struct tty *, int, int, void (**)(struct tty *, int)));
#ifdef KGDB
void zs_kgdb_init __P((void));
#endif
/* fb.c */
void	fb_unblank __P((void));
/* cache.c */
void cache_flush __P((caddr_t, u_int));
/* kgdb_stub.c */
#ifdef KGDB
void kgdb_attach __P((int (*)(void *), void (*)(void *, int), void *));
void kgdb_connect __P((int));
void kgdb_panic __P((void));
#endif
/* emul.c */
struct trapframe;
int fixalign __P((struct proc *, struct trapframe *));
int emulinstr __P((int, struct trapframe *));
/* cpu.c */
void mp_pause_cpus __P((void));
void mp_resume_cpus __P((void));
void mp_halt_cpus __P((void));
/* msiiep.c */
void msiiep_swap_endian __P((int));

/*
 *
 * The SPARC has a Trap Base Register (TBR) which holds the upper 20 bits
 * of the trap vector table.  The next eight bits are supplied by the
 * hardware when the trap occurs, and the bottom four bits are always
 * zero (so that we can shove up to 16 bytes of executable code---exactly
 * four instructions---into each trap vector).
 *
 * The hardware allocates half the trap vectors to hardware and half to
 * software.
 *
 * Traps have priorities assigned (lower number => higher priority).
 */

struct trapvec {
	int	tv_instr[4];		/* the four instructions */
};
extern struct trapvec *trapbase;	/* the 256 vectors */

extern void wzero __P((void *, u_int));
extern void wcopy __P((const void *, void *, u_int));

#endif /* _KERNEL */
#endif /* _CPU_H_ */
