/*	$NetBSD: locore.s,v 1.10.8.4 2001/02/15 13:36:10 bouyer Exp $	*/
/*	$OpenBSD: locore.S,v 1.4 1997/01/26 09:06:38 rahnds Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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

#include "opt_ddb.h"
#include "fs_kernfs.h"
#include "opt_ipkdb.h"
#include "opt_lockdebug.h"
#include "opt_multiprocessor.h"
#include "assym.h"

#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/asm.h>

/*
 * Some instructions gas doesn't understand (yet?)
 */
#define	bdneq	bdnzf 2,

#define	INTSTK	(8*1024)	/* 8K interrupt stack */
#define	SPILLSTK 1024		/* 1K spill stack */

/*
 * Globals
 */
GLOBAL(startsym)
	.long	0			/* start symbol table */
GLOBAL(endsym)
	.long	0			/* end symbol table */
GLOBAL(proc0paddr)
	.long	0			/* proc0 p_addr */

GLOBAL(intrnames)
	.asciz	"clock", "irq1", "irq2", "irq3"
	.asciz	"irq4", "irq5", "irq6", "irq7"
	.asciz	"irq8", "irq9", "irq10", "irq11"
	.asciz	"irq12", "irq13", "irq14", "irq15"
	.asciz	"irq16", "irq17", "irq18", "irq19"
	.asciz	"irq20", "irq21", "irq22", "irq23"
	.asciz	"irq24", "irq25", "irq26", "irq27"
	.asciz	"irq28", "softnet", "softclock", "softserial"
GLOBAL(eintrnames)
	.align	4
GLOBAL(intrcnt)
	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
GLOBAL(eintrcnt)

/*
 * File-scope for locore.S
 */
	.data
idle_u:
	.long	0			/* fake uarea during idle after exit */

/*
 * This symbol is here for the benefit of kvm_mkdb, and is supposed to
 * mark the start of kernel text.
 */
	.text
	.globl	_C_LABEL(kernel_text)
_C_LABEL(kernel_text):

/*
 * Startup entry.  Note, this must be the first thing in the text
 * segment!
 */
	.text
	.globl	__start
__start:
	li	0,0
	mtmsr	0			/* Disable FPU/MMU/exceptions */
	isync

/*
 * Cpu detect.
 * 
 */
	lis	8, 0x7FFF
	ori	8, 8, 0xF3F0
	lwz	9, 0(8)		/* read processor ID */
	andis.	9, 9, 0x0200	/* bit 6: */
	cmpwi	0, 9, 0		/* 0 if read by CPU 0, 1 if read by CPU 1 */
	bne	__start_cpu1
	b	__start_cpu0

__start_cpu1:
#ifdef CPU1_SLEEP
	lis	8, 0x0020	/* SLEEP */
	mfspr	7, 1008		/* get HID0 */
	or	7, 7, 8
	mtspr	1008, 7		/* set HID0 */
	lis	8, 0x0004	/* POW */
	sync
	mtmsr	8
	isync			/* zzz... */
#else
	addi	9, 9, 1
	lis	7, 0x100
__start_cpu1_loop:
	subi	7, 7, 1
	cmpi	0, 7, 0
	bne	__start_cpu1_loop
	lis	8, 0x8000
	ori	8, 8, 0x0c00
	lis	9,_C_LABEL(cpl)@ha
	lwz	9,_C_LABEL(cpl)@l(9)
	stb	9, 0(8)
#endif
	b	__start_cpu1

__start_cpu0:

/* compute end of kernel memory */
	lis	8,_C_LABEL(end)@ha
	addi	8,8,_C_LABEL(end)@l
#if defined(DDB) || defined(KERNFS)
	lis	7,_C_LABEL(startsym)@ha
	addi	7,7,_C_LABEL(startsym)@l
	stw	3,0(7)
	lis	7,_C_LABEL(endsym)@ha
	addi	7,7,_C_LABEL(endsym)@l
	stw	4,0(7)
	mr	8,4			/* end of sysbol table */
#endif
	li	9,PGOFSET
	add	8,8,9
	andc	8,8,9
	addi	8,8,NBPG
	lis	9,idle_u@ha
	stw	8,idle_u@l(9)
	addi	8,8,USPACE		/* space for idle_u */
	lis	9,_C_LABEL(proc0paddr)@ha
	stw	8,_C_LABEL(proc0paddr)@l(9)
	addi	1,8,USPACE-FRAMELEN	/* stackpointer for proc0 */
	mr	4,1			/* end of mem reserved for kernel */
	xor	0,0,0
	stwu	0,-16(1)		/* end of stack chain */
	
	lis	3,__start@ha
	addi	3,3,__start@l

	bl	_C_LABEL(initppc)
	bl	_C_LABEL(main)

loop:	b	loop			/* XXX not reached */
/*
 * No processes are runnable, so loop waiting for one.
 * Separate label here for accounting purposes.
 * When we get here, interrupts are off (MSR[EE]=0) and sched_lock is held.
 */
ASENTRY(Idle)
	lis	8,_C_LABEL(sched_whichqs)@ha
	lwz	9,_C_LABEL(sched_whichqs)@l(8)

	or.	9,9,9
	bne-	.Lsw1			/* at least one queue non-empty */
	
#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	bl	_C_LABEL(sched_unlock_idle)
#endif

	mfmsr	3
	ori	3,3,PSL_EE@l		/* reenable ints again */
	mtmsr	3
	isync
	
/* May do some power saving here? */

	andi.	3,3,~PSL_EE@l		/* disable interrupts while
					   manipulating runque */
	mtmsr	3

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	bl	_C_LABEL(sched_lock_idle)
#endif
	b	_ASM_LABEL(Idle)

/*
 * switchexit gets called from cpu_exit to complete the exit procedure.
 */
ENTRY(switchexit)
/* First switch to the idle pcb/kernel stack */
	lis	6,idle_u@ha
	lwz	6,idle_u@l(6)
	lis	7,_C_LABEL(curpcb)@ha
	stw	6,_C_LABEL(curpcb)@l(7)
	addi	1,6,USPACE-16		/* 16 bytes are reserved at stack top */
	/*
	 * Schedule the vmspace and stack to be freed (the proc arg is
	 * already in r3).
	 */
	bl	_C_LABEL(exit2)

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	bl	_C_LABEL(sched_lock_idle)
#endif

/* Fall through to cpu_switch to actually select another proc */
	li	3,0			/* indicate exited process */

/*
 * void cpu_switch(struct proc *p)
 * Find a runnable process and switch to it.
 */
/* XXX noprofile?  --thorpej@netbsd.org */
ENTRY(cpu_switch)
	mflr	0			/* save lr */
	stw	0,4(1)
	stwu	1,-16(1)
	stw	31,12(1)
	stw	30,8(1)

	mr	30,3
	lis	3,_C_LABEL(curproc)@ha
	xor	31,31,31
	stw	31,_C_LABEL(curproc)@l(3) /* Zero to not accumulate cpu time */
	lis	3,_C_LABEL(curpcb)@ha
	lwz	31,_C_LABEL(curpcb)@l(3)

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
/* Release the sched_lock before processing interrupts. */
	bl	_C_LABEL(sched_unlock_idle)
#endif

	xor	3,3,3
	bl	_C_LABEL(lcsplx)
	stw	3,PCB_SPL(31)		/* save spl */

/* Lock the scheduler. */
	mfmsr	3
	andi.	3,3,~PSL_EE@l		/* disable interrupts while
					   manipulating runque */
	mtmsr	3
	isync
#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	bl	_C_LABEL(sched_lock_idle)
#endif

/* Find a new process */
	lis	8,_C_LABEL(sched_whichqs)@ha
	lwz	9,_C_LABEL(sched_whichqs)@l(8)

	or.	9,9,9
	beq-	_ASM_LABEL(Idle)	/* all queues empty */
.Lsw1:
	cntlzw	10,9
	lis	4,_C_LABEL(sched_qs)@ha
	addi	4,4,_C_LABEL(sched_qs)@l
	slwi	3,10,3
	add	3,3,4			/* select queue */
	
	lwz	31,P_FORW(3)		/* unlink first proc from queue */
	lwz	4,P_FORW(31)
	stw	4,P_FORW(3)
	stw	3,P_BACK(4)

	cmpl	0,3,4			/* queue empty? */
	bne	1f

	lis	3,0x80000000@h
	srw	3,3,10
	andc	9,9,3
	stw	9,_C_LABEL(sched_whichqs)@l(8) /* mark it empty */

1:
	/* just did this resched thing */
	xor	3,3,3
	lis	4,_C_LABEL(want_resched)@ha
	stw	3,_C_LABEL(want_resched)@l(4)

	stw	3,P_BACK(31)		/* probably superfluous */

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	/* Unlock the sched_lock, but leave interrupts off, for now. */
	bl	_C_LABEL(sched_unlock_idle)
#endif

#if defined(MULTIPROCESSOR)
	/*
	 * XXXSMP
	 * p->p_cpu = curcpu();
	 */
#endif

	/* Process now running on a processor. */
	li	3,SONPROC		/* p->p_stat = SONPROC */
	stb	3,P_STAT(31)

	/* record new process */
	lis	4,_C_LABEL(curproc)@ha
	stw	31,_C_LABEL(curproc)@l(4)

	mfmsr	3
	ori	3,3,PSL_EE@l		/* Now we can interrupt again */
	mtmsr	3

	cmpl	0,31,30			/* is it the same process? */
	beq	switch_return

	or.	30,30,30		/* old process was exiting? */
	beq	switch_exited

	mfsr	10,USER_SR		/* save USER_SR for copyin/copyout */
	mfcr	11			/* save cr */
	mr	12,2			/* save r2 */
	stwu	1,-SFRAMELEN(1)		/* still running on old stack */
	stmw	10,8(1)
	lwz	3,P_ADDR(30)
	stw	1,PCB_SP(3)		/* save SP */

switch_exited:
	mfmsr	3
	andi.	3,3,~PSL_EE@l		/* disable interrupts while
					   actually switching */
	mtmsr	3

	/* indicate new pcb */
	lwz	4,P_ADDR(31)
	lis	5,_C_LABEL(curpcb)@ha
	stw	4,_C_LABEL(curpcb)@l(5)

	/* save real pmap pointer for spill fill */
	lwz	5,PCB_PMR(4)
	lis	6,_C_LABEL(curpm)@ha
	stwu	5,_C_LABEL(curpm)@l(6)
	stwcx.	5,0,6			/* clear possible reservation */

	addic.	5,5,64
	li	6,0
	mfsr	8,KERNEL_SR		/* save kernel SR */
1:
	addis	6,6,-0x10000000@ha	/* set new procs segment registers */
	or.	6,6,6			/* This is done from the real
					   address pmap */
	lwzu	7,-4(5)			/* so we don't have to worry */
	mtsrin	7,6			/* about accessibility */
	bne	1b
	mtsr	KERNEL_SR,8		/* restore kernel SR */
	isync

	lwz	1,PCB_SP(4)		/* get new procs SP */

	ori	3,3,PSL_EE@l		/* interrupts are okay again */
	mtmsr	3

	lmw	10,8(1)			/* get other regs */
	lwz	1,0(1)			/* get saved SP */
	mr	2,12			/* get saved r2 */
	mtcr	11			/* get saved cr */
	isync
	mtsr	USER_SR,10		/* get saved USER_SR */
	isync

switch_return:
	mr	30,7			/* save proc pointer */
	lwz	3,PCB_SPL(4)
	bl	_C_LABEL(lcsplx)

	mr	3,30			/* get curproc for special fork
					   returns */

	lwz	31,12(1)
	lwz	30,8(1)
	addi	1,1,16
	lwz	0,4(1)
	mtlr	0
	blr

/*
 * Child comes here at the end of a fork.
 * Return to userspace via the trap return path.
 */
	.globl	_C_LABEL(fork_trampoline)
_C_LABEL(fork_trampoline):
	xor	3,3,3
	bl	_C_LABEL(lcsplx)
	mtlr	31
	mr	3,30
	blrl				/* jump indirect to r31 */
	b	trapexit

/*
 * Pull in common trap vector code.
 */
#include <powerpc/powerpc/trap_subr.S>

/*
 * int setfault()
 *
 * Similar to setjmp to setup for handling faults on accesses to user memory.
 * Any routine using this may only call bcopy, either the form below,
 * or the (currently used) C code optimized, so it doesn't use any non-volatile
 * registers.
 */
	.globl	_C_LABEL(setfault)
_C_LABEL(setfault):
	mflr	0
	mfcr	12
	lis	4,_C_LABEL(curpcb)@ha
	lwz	4,_C_LABEL(curpcb)@l(4)
	stw	3,PCB_FAULT(4)
	stw	0,0(3)
	stw	1,4(3)
	stw	2,8(3)
	stmw	12,12(3)
	xor	3,3,3
	blr

	.globl	_C_LABEL(enable_intr)
_C_LABEL(enable_intr):
	mfmsr	3
	ori	3,3,PSL_EE@l
	mtmsr	3
	blr

	.globl	_C_LABEL(disable_intr)
_C_LABEL(disable_intr):
	mfmsr	3
	andi.	3,3,~PSL_EE@l
	mtmsr	3
	blr

	.globl	_C_LABEL(debug_led)
_C_LABEL(debug_led):
	lis	4, 0x8000
	ori	4, 4, 0x0c00
	stb	3, 0(4)
	blr
