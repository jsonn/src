/*	$NetBSD: cpu.h,v 1.28.2.3 2002/06/23 17:42:11 jdolecek Exp $ */

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

#if !defined(_LKM)
#include "opt_multiprocessor.h"
#include "opt_lockdebug.h"
#endif

#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/intr.h>
#include <sparc64/sparc64/intreg.h>

#include <sys/sched.h>
/*
 * The cpu_info structure is part of a 64KB structure mapped both the kernel
 * pmap and a single locked TTE a CPUINFO_VA for that particular processor.
 * Each processor's cpu_info is accessible at CPUINFO_VA only for that
 * processor.  Other processors can access that through an additional mapping
 * in the kernel pmap.
 *
 * The 64KB page contains:
 *
 * cpu_info
 * interrupt stack (all remaining space)
 * idle PCB
 * idle stack (STACKSPACE - sizeof(PCB))
 * 32KB TSB
 */

struct cpu_info {
	/* Most important fields first */
	struct proc		*ci_curproc;
	struct pcb		*ci_cpcb;	/* also initial stack */
	struct cpu_info		*ci_next;

	struct proc		*ci_fpproc;
	int			ci_number;
	int			ci_upaid;
	struct schedstate_percpu ci_schedstate; /* scheduler state */

	/* DEBUG/DIAGNOSTIC stuff */
	u_long			ci_spin_locks;	/* # of spin locks held */
	u_long			ci_simple_locks;/* # of simple locks held */

	/* Spinning up the CPU */
	void			(*ci_spinup) __P((void)); /* spinup routine */
	void			*ci_initstack;
	paddr_t			ci_paddr;	/* Phys addr of this structure. */
};

extern struct cpu_info *cpus;
extern struct cpu_info cpu_info_store;

#if 1
#define	curcpu()	(&cpu_info_store)
#else
#define	curcpu()	((struct cpu_info *)CPUINFO_VA)
#endif

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_swapin(p)	/* nothing */
#define	cpu_swapout(p)	/* nothing */
#define	cpu_wait(p)	/* nothing */
#if 1
#define cpu_number()	0
#else
#define	cpu_number()	(curcpu()->ci_number)
#endif

/*
 * Arguments to hardclock, softclock and gatherstats encapsulate the
 * previous machine state in an opaque clockframe.  The ipl is here
 * as well for strayintr (see locore.s:interrupt and intr.c:strayintr).
 * Note that CLKF_INTR is valid only if CLKF_USERMODE is false.
 */
extern int intstack[];
extern int eintstack[];
struct clockframe {
	struct trapframe64 t;
};

#define	CLKF_USERMODE(framep)	(((framep)->t.tf_tstate & TSTATE_PRIV) == 0)
/*
 * XXX Disable CLKF_BASEPRI() for now.  If we use a counter-timer for
 * the clock, the interrupt remains blocked until the interrupt handler
 * returns and we write to the clear interrupt register.  If we use 
 * %tick for the clock, we could get multiple interrupts, but the 
 * currently enabled INTR_INTERLOCK will prevent the interrupt from being
 * posted twice anyway.
 * 
 * Switching to %tick for all machines and disabling INTR_INTERLOCK
 * in locore.s would allow us to take advantage of CLKF_BASEPRI().
 */
#if 0
#define	CLKF_BASEPRI(framep)	(((framep)->t.tf_oldpil) == 0)
#else
#define	CLKF_BASEPRI(framep)	(0)
#endif
#define	CLKF_PC(framep)		((framep)->t.tf_pc)
/* Since some files in sys/kern do not know BIAS, I'm using 0x7ff here */
#define	CLKF_INTR(framep)						\
	((!CLKF_USERMODE(framep))&&					\
		(((framep)->t.tf_out[6] & 1 ) ?				\
			(((vaddr_t)(framep)->t.tf_out[6] <		\
				(vaddr_t)EINTSTACK-0x7ff) &&		\
			((vaddr_t)(framep)->t.tf_out[6] >		\
				(vaddr_t)INTSTACK-0x7ff)) :		\
			(((vaddr_t)(framep)->t.tf_out[6] <		\
				(vaddr_t)EINTSTACK) &&			\
			((vaddr_t)(framep)->t.tf_out[6] >		\
				(vaddr_t)INTSTACK))))

/*
 * Software interrupt request `register'.
 */
#ifdef DEPRECATED
union sir {
	int	sir_any;
	char	sir_which[4];
} sir;

#define SIR_NET		0
#define SIR_CLOCK	1
#endif

extern struct intrhand soft01intr, soft01net, soft01clock;

#if 0
#define setsoftint()	send_softint(-1, IPL_SOFTINT, &soft01intr)
#define setsoftnet()	send_softint(-1, IPL_SOFTNET, &soft01net)
#else
void setsoftint __P((void));
void setsoftnet __P((void));
#endif

int	want_ast;

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
int	want_resched;		/* resched() was called */
#define	need_resched(ci)	(want_resched = 1, want_ast = 1)

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

/*
 * Only one process may own the FPU state.
 *
 * XXX this must be per-cpu (eventually)
 */
struct	proc *fpproc;		/* FPU owner */
int	foundfpu;		/* true => we have an FPU */

/*
 * Interrupt handler chains.  Interrupt handlers should return 0 for
 * ``not me'' or 1 (``I took care of it'').  intr_establish() inserts a
 * handler into the list.  The handler is called with its (single)
 * argument, or with a pointer to a clockframe if ih_arg is NULL.
 */
struct intrhand {
	int			(*ih_fun) __P((void *));
	void			*ih_arg;
	short			ih_number;	/* interrupt number */
						/* the H/W provides */
	char			ih_pil;		/* interrupt priority */
	struct intrhand		*ih_next;	/* global list */
	struct intrhand		*ih_pending;	/* interrupt queued */
	volatile u_int64_t	*ih_map;	/* Interrupt map reg */
	volatile u_int64_t	*ih_clr;	/* clear interrupt reg */
};
extern struct intrhand *intrhand[];
extern struct intrhand *intrlev[MAXINTNUM];

void	intr_establish __P((int level, struct intrhand *));

/* cpu.c */
paddr_t cpu_alloc __P((void));
u_int64_t cpu_init __P((paddr_t, int));
/* disksubr.c */
struct dkbad;
int isbad __P((struct dkbad *bt, int, int, int));
/* machdep.c */
int	ldcontrolb __P((caddr_t));
void	dumpconf __P((void));
caddr_t	reserve_dumppages __P((caddr_t));
/* clock.c */
struct timeval;
int	tickintr __P((void *)); /* level 10 (tick) interrupt code */
int	clockintr __P((void *));/* level 10 (clock) interrupt code */
int	statintr __P((void *));	/* level 14 (statclock) interrupt code */
/* locore.s */
struct fpstate64;
void	savefpstate __P((struct fpstate64 *));
void	loadfpstate __P((struct fpstate64 *));
u_int64_t	probeget __P((paddr_t, int, int));
int	probeset __P((paddr_t, int, int, u_int64_t));
#if 0
void	write_all_windows __P((void));
void	write_user_windows __P((void));
#else
#define	 write_all_windows() __asm __volatile("flushw" : : )
#define	 write_user_windows() __asm __volatile("flushw" : : )
#endif
void 	proc_trampoline __P((void));
struct pcb;
void	snapshot __P((struct pcb *));
struct frame *getfp __P((void));
int	xldcontrolb __P((caddr_t, struct pcb *));
void	copywords __P((const void *, void *, size_t));
void	qcopy __P((const void *, void *, size_t));
void	qzero __P((void *, size_t));
void	switchtoctx __P((int));
/* locore2.c */
void	remrq __P((struct proc *));
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
/* kgdb_stub.c */
#ifdef KGDB
void kgdb_attach __P((int (*)(void *), void (*)(void *, int), void *));
void kgdb_connect __P((int));
void kgdb_panic __P((void));
#endif
/* emul.c */
int	fixalign __P((struct proc *, struct trapframe64 *));
int	emulinstr __P((vaddr_t, struct trapframe64 *));

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
	int	tv_instr[8];		/* the eight instructions */
};
extern struct trapvec *trapbase;	/* the 256 vectors */

extern void wzero __P((void *, u_int));
extern void wcopy __P((const void *, void *, u_int));

#endif /* _KERNEL */
#endif /* _CPU_H_ */
