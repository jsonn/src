/*	$NetBSD: cpu.h,v 1.20.2.1 1997/03/12 21:16:09 is Exp $	*/

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

#ifndef _NS532_CPU_H_
#define _NS532_CPU_H_

/*
 * Definitions unique to ns532 cpu support.
 *
 *   modified from 386 code for the pc532 by Phil Nelson (12/92)
 */

#include <machine/psl.h>
#include <machine/frame.h>

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define cpu_swapin(p)           	/* nothing */
#define	cpu_wait(p)			/* nothing */

/*
 * Arguments to hardclock, softclock and gatherstats
 * encapsulate the previous machine state in an opaque
 * clockframe; for now, use generic intrframe.
 */

#define clockframe intrframe 

#define	CLKF_USERMODE(framep)	USERMODE((framep)->if_regs.r_psr)
#define	CLKF_BASEPRI(framep)	((framep)->if_pl == imask[IPL_ZERO])
#define	CLKF_PC(framep)		((framep)->if_regs.r_pc)
#define	CLKF_INTR(frame)	(0)	/* XXX should have an interrupt stack */

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
int	want_resched;	/* resched() was called */
#define	need_resched()	(want_resched = 1, setsoftast())

/*
 * Give a profiling tick to the current process from the softclock
 * interrupt.  On the pc532, request an ast to send us through trap(),
 * marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, setsoftast())

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)	setsoftast()

/*
 * We need a machine-independent name for this.
 */
#define	DELAY(n)	delay(n)
void	delay __P((int));

#ifdef _KERNEL
/* autoconf.c */
void	configure __P((void));

/* machdep.c */
void	delay __P((int));
void	dumpconf __P((void));
void	cpu_reset __P((void));

/* locore.s */
void	proc_trampoline __P((void));

/* clock.c */
void	startrtclock __P((void));

/* trap.c */
void	child_return __P((struct proc *, struct syscframe frame));

/* vm_machdep.c */
int kvtop __P((caddr_t));

#endif /* _KERNEL */

/* 
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_NKPDE		2	/* int: number of kernel PDEs */
#define	CPU_MAXID		3	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
	{ "nkpde", CTLTYPE_INT }, \
}

#endif /* !_NS532_CPU_H_ */
