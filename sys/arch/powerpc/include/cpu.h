/*	$NetBSD: cpu.h,v 1.10.6.2 2001/11/05 19:46:16 briggs Exp $	*/

/*
 * Copyright (C) 1999 Wolfgang Solfrank.
 * Copyright (C) 1999 TooLs GmbH.
 * Copyright (C) 1995-1997 Wolfgang Solfrank.
 * Copyright (C) 1995-1997 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_POWERPC_CPU_H_
#define	_POWERPC_CPU_H_

#if defined(_KERNEL_OPT)
#include "opt_lockdebug.h"
#include "opt_multiprocessor.h"
#endif

#include <sys/device.h>
#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/intr.h>

#ifdef _KERNEL
#include <sys/sched.h>
struct cpu_info {
	struct schedstate_percpu ci_schedstate; /* scheduler state */
#if defined(DIAGNOSTIC) || defined(LOCKDEBUG)
	u_long ci_spin_locks;		/* # of spin locks held */
	u_long ci_simple_locks;		/* # of simple locks held */
#endif
	struct device *ci_dev;		/* device of corresponding cpu */
	struct lwp *ci_curproc;		/* current owner of the processor */

	struct pcb *ci_curpcb;
	struct pmap *ci_curpm;
	struct lwp *ci_fpuproc;
	struct pcb *ci_idle_pcb;	/* PA of our idle pcb */
	int ci_cpuid;

	int ci_astpending;
	int ci_want_resched;
	u_long ci_lasttb;
	int ci_tickspending;
	int ci_cpl;
	int ci_ipending;
	int ci_intrdepth;
	char *ci_intstk;
	char *ci_spillstk;
	int ci_tempsave[8];
	int ci_ddbsave[8];
	int ci_ipkdbsave[8];
	int ci_disisave[4];
	struct evcnt ci_ev_traps;	/* calls to trap() */
	struct evcnt ci_ev_kdsi;	/* kernel DSI traps */
	struct evcnt ci_ev_udsi;	/* user DSI traps */
	struct evcnt ci_ev_udsi_fatal;	/* user DSI trap failures */
	struct evcnt ci_ev_isi;		/* user ISI traps */
	struct evcnt ci_ev_isi_fatal;	/* user ISI trap failures */
	struct evcnt ci_ev_pgm;		/* user PGM traps */
	struct evcnt ci_ev_fpu;		/* FPU traps */
	struct evcnt ci_ev_fpusw;	/* FPU context switch */
	struct evcnt ci_ev_ali;		/* Alignment traps */
	struct evcnt ci_ev_ali_fatal;	/* Alignment fatal trap */
	struct evcnt ci_ev_scalls;	/* system call traps */
	struct evcnt ci_ev_vec;		/* Altivec traps */
	struct evcnt ci_ev_vecsw;	/* Altivec context switches */
};

#ifdef MULTIPROCESSOR
static __inline int
cpu_number()
{
	int pir;

	asm ("mfspr %0,1023" : "=r"(pir));
	return pir;
}

static __inline struct cpu_info *
curcpu()
{
	struct cpu_info *ci;

	asm volatile ("mfsprg %0,0" : "=r"(ci));
	return ci;
}

extern struct cpu_info cpu_info[];

#define CPU_IS_PRIMARY(ci)	((ci)->ci_cpuid == 0)
#define curproc			curcpu()->ci_curproc
#define fpuproc			curcpu()->ci_fpuproc
#define curpcb			curcpu()->ci_curpcb
#define curpm			curcpu()->ci_curpm
#define want_resched		curcpu()->ci_want_resched
#define astpending		curcpu()->ci_astpending
#define	intr_depth		curcpu()->ci_intrdepth

#else
extern struct cpu_info cpu_info_store;
extern volatile int want_resched;
extern volatile int astpending;
extern volatile int intr_depth;

#define curcpu()		(&cpu_info_store)
#define cpu_number()		0

#endif /* MULTIPROCESSOR */

#define ppc_cpuid(ci)		((ci)->ci_cpuid)

#define	CLKF_USERMODE(frame)	(((frame)->srr1 & PSL_PR) != 0)
#define	CLKF_BASEPRI(frame)	((frame)->pri == 0)
#define	CLKF_PC(frame)		((frame)->srr0)
#define	CLKF_INTR(frame)	((frame)->depth > 0)

#define	PROC_PC(p)		(trapframe(p)->srr0)

#define	cpu_swapout(p)
#define cpu_wait(p)

extern int powersave;
extern int cpu_timebase;
extern int cpu_printfataltraps;

extern struct cpu_info *cpu_attach_common(struct device *, int);
extern void cpu_identify(char *, size_t);
extern void delay (unsigned int);
#define	DELAY(n)		delay(n)

#define	need_resched(ci)	(want_resched = 1, astpending = 1)
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, astpending = 1)
#define	signotify(p)		(astpending = 1)

#endif /* _KERNEL */

#if defined(_KERNEL) || defined(_STANDALONE)
#if !defined(CACHELINESIZE)
#define	CACHELINESIZE	32
#endif
#endif

void __syncicache(void *, int);

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CACHELINE		1
#define	CPU_TIMEBASE		2
#define	CPU_CPUTEMP		3
#define	CPU_PRINTFATALTRAPS	4
#define	CPU_MAXID		5

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "cachelinesize", CTLTYPE_INT }, \
	{ "timebase", CTLTYPE_INT }, \
	{ "cputempature", CTLTYPE_INT }, \
	{ "printfataltraps", CTLTYPE_INT }, \
}

#endif	/* _POWERPC_CPU_H_ */
