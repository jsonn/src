/*	$NetBSD: locore.s,v 1.69.2.4 2001/03/12 13:29:41 bouyer Exp $	*/

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1980, 1990, 1993
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
 *	from: Utah $Hdr: locore.s 1.66 92/12/22$
 *	@(#)locore.s	8.6 (Berkeley) 5/27/94
 */

#include "opt_compat_netbsd.h"
#include "opt_compat_svr4.h"
#include "opt_compat_sunos.h"
#include "opt_lockdebug.h"

#include "assym.h"
#include <machine/asm.h>
#include <machine/trap.h>

| Remember this is a fun project!

| This is for kvm_mkdb, and should be the address of the beginning
| of the kernel text segment (not necessarily the same as kernbase).
	.text
GLOBAL(kernel_text)

| This is the entry point, as well as the end of the temporary stack
| used during process switch (one 8K page ending at start)
ASGLOBAL(tmpstk)
ASGLOBAL(start)

| First we need to set it up so we can access the sun MMU, and be otherwise
| undisturbed.  Until otherwise noted, all code must be position independent
| as the boot loader put us low in memory, but we are linked high.
	movw	#PSL_HIGHIPL,%sr	| no interrupts
	moveq	#FC_CONTROL,%d0		| make movs access "control"
	movc	%d0,%sfc		| space where the sun3 designers
	movc	%d0,%dfc		| put all the "useful" stuff

| Set context zero and stay there until pmap_bootstrap.
	moveq	#0,%d0
	movsb	%d0,CONTEXT_REG

| In order to "move" the kernel to high memory, we are going to copy the
| first 4 Mb of pmegs such that we will be mapped at the linked address.
| This is all done by copying in the segment map (top-level MMU table).
| We will unscramble which PMEGs we actually need later.

	movl	#(SEGMAP_BASE+0),%a0		| src
	movl	#(SEGMAP_BASE+KERNBASE),%a1	| dst
	movl	#(0x400000/NBSG),%d0		| count

L_per_pmeg:
	movsb	%a0@,%d1		| copy segmap entry
	movsb	%d1,%a1@
	addl	#NBSG,%a0		| increment pointers
	addl	#NBSG,%a1
	subql	#1,%d0			| decrement count
	bgt	L_per_pmeg

| Kernel is now double mapped at zero and KERNBASE.
| Force a long jump to the relocated code (high VA).
	movl	#IC_CLEAR,%d0		| Flush the I-cache
	movc	%d0,%cacr
	jmp	L_high_code:l		| long jump

L_high_code:
| We are now running in the correctly relocated kernel, so
| we are no longer restricted to position-independent code.

| Do bootstrap stuff needed before main() gets called.
| Make sure the initial frame pointer is zero so that
| the backtrace algorithm used by KGDB terminates nicely.
	lea	_ASM_LABEL(tmpstk),%sp
	movl	#0,%a6
	jsr	_C_LABEL(_bootstrap)	| See locore2.c

| Now that _bootstrap() is done using the PROM functions,
| we can safely set the %sfc/dfc to something != FC_CONTROL
	moveq	#FC_USERD,%d0		| make movs access "user data"
	movc	%d0,%sfc		| space for copyin/copyout
	movc	%d0,%dfc

| Setup process zero user/kernel stacks.
	movl	_C_LABEL(proc0paddr),%a1 | get proc0 pcb addr
	lea	%a1@(USPACE-4),%sp	| set SSP to last word
	movl	#USRSTACK-4,%a2
	movl	%a2,%usp		| init user SP

| Note curpcb was already set in _bootstrap().
| Will do fpu initialization during autoconfig (see fpu.c)
| The interrupt vector table and stack are now ready.
| Interrupts will be enabled later, AFTER  autoconfiguration
| is finished, to avoid spurrious interrupts.

/*
 * Final preparation for calling main.
 *
 * Create a fake exception frame that returns to user mode,
 * and save its address in p->p_md.md_regs for cpu_fork().
 * The new frames for process 1 and 2 will be adjusted by
 * cpu_set_kpc() to arrange for a call to a kernel function
 * before the new process does its rte out to user mode.
 */
	clrw	%sp@-			| tf_format,tf_vector
	clrl	%sp@-			| tf_pc (filled in later)
	movw	#PSL_USER,%sp@-		| tf_sr for user mode
	clrl	%sp@-			| tf_stackadj
	lea	%sp@(-64),%sp		| tf_regs[16]
	movl	%sp,%a1			| %a1=trapframe
	lea	_C_LABEL(proc0),%a0	| proc0.p_md.md_regs = 
	movl	%a1,%a0@(P_MDREGS)	|   trapframe
	movl	%a2,%a1@(FR_SP)		| a2 == usp (from above)
	pea	%a1@			| push &trapframe
	jbsr	_C_LABEL(main)		| main(&trapframe)
	addql	#4,%sp			| help DDB backtrace
	trap	#15			| should not get here

| This is used by cpu_fork() to return to user mode.
| It is called with SP pointing to a struct trapframe.
GLOBAL(proc_do_uret)
	movl	%sp@(FR_SP),%a0		| grab and load
	movl	%a0,%usp		|   user SP
	moveml	%sp@+,#0x7FFF		| load most registers (all but SSP)
	addql	#8,%sp			| pop SSP and stack adjust count
	rte

/*
 * proc_trampoline:
 * This is used by cpu_set_kpc() to "push" a function call onto the
 * kernel stack of some process, very much like a signal delivery.
 * When we get here, the stack has:
 *
 * SP+8:	switchframe from before cpu_set_kpc
 * SP+4:	void *arg;
 * SP:  	u_long func;
 *
 * On entry, the switchframe pushed by cpu_set_kpc has already been
 * popped off the stack, so all this needs to do is pop the function
 * pointer into a register, call it, then pop the arg, and finally
 * return using the switchframe that remains on the stack.
 */
GLOBAL(proc_trampoline)
	movl	%sp@+,%a0		| function pointer
	jbsr	%a0@			| (*func)(arg)
	addql	#4,%sp			| toss the arg
	rts				| as cpu_switch would do

| That is all the assembly startup code we need on the sun3!
| The rest of this is like the hp300/locore.s where possible.

/*
 * Trap/interrupt vector routines
 */
#include <m68k/m68k/trap_subr.s>

GLOBAL(buserr)
	tstl	_C_LABEL(nofault)	| device probe?
	jeq	_C_LABEL(addrerr)	| no, handle as usual
	movl	_C_LABEL(nofault),%sp@-	| yes,
	jbsr	_C_LABEL(longjmp)	|  longjmp(nofault)
GLOBAL(addrerr)
	clrl	%sp@-			| stack adjust count
	moveml	#0xFFFF,%sp@-		| save user registers
	movl	%usp,%a0		| save the user SP
	movl	%a0,%sp@(FR_SP)		|   in the savearea
	lea	%sp@(FR_HW),%a1		| grab base of HW berr frame
	moveq	#0,%d0
	movw	%a1@(10),%d0		| grab SSW for fault processing
	btst	#12,%d0			| RB set?
	jeq	LbeX0			| no, test RC
	bset	#14,%d0			| yes, must set FB
	movw	%d0,%a1@(10)		| for hardware too
LbeX0:
	btst	#13,%d0			| RC set?
	jeq	LbeX1			| no, skip
	bset	#15,%d0			| yes, must set FC
	movw	%d0,%a1@(10)		| for hardware too
LbeX1:
	btst	#8,%d0			| data fault?
	jeq	Lbe0			| no, check for hard cases
	movl	%a1@(16),%d1		| fault address is as given in frame
	jra	Lbe10			| thats it
Lbe0:
	btst	#4,%a1@(6)		| long (type B) stack frame?
	jne	Lbe4			| yes, go handle
	movl	%a1@(2),%d1		| no, can use save PC
	btst	#14,%d0			| FB set?
	jeq	Lbe3			| no, try FC
	addql	#4,%d1			| yes, adjust address
	jra	Lbe10			| done
Lbe3:
	btst	#15,%d0			| FC set?
	jeq	Lbe10			| no, done
	addql	#2,%d1			| yes, adjust address
	jra	Lbe10			| done
Lbe4:
	movl	%a1@(36),%d1		| long format, use stage B address
	btst	#15,%d0			| FC set?
	jeq	Lbe10			| no, all done
	subql	#2,%d1			| yes, adjust address
Lbe10:
	movl	%d1,%sp@-		| push fault VA
	movl	%d0,%sp@-		| and padded SSW
	movw	%a1@(6),%d0		| get frame format/vector offset
	andw	#0x0FFF,%d0		| clear out frame format
	cmpw	#12,%d0			| address error vector?
	jeq	Lisaerr			| yes, go to it

/*
 * the sun3 specific code
 *
 * our mission: figure out whether what we are looking at is
 *              bus error in the UNIX sense, or
 *	        a memory error i.e a page fault
 *
 * [this code replaces similarly mmu specific code in the hp300 code]
 */
sun3_mmu_specific:
	clrl %d0			| make sure top bits are cleard too
	movl %d1,%sp@-			| save %d1
	movc %sfc,%d1			| save %sfc to %d1
	moveq #FC_CONTROL,%d0		| %sfc = FC_CONTROL
	movc %d0,%sfc
	movsb BUSERR_REG,%d0		| get value of bus error register
	movc %d1,%sfc			| restore %sfc
	movl %sp@+,%d1			| restore %d1
	andb #BUSERR_MMU,%d0 		| is this an MMU fault?
	jeq Lisberr			| non-MMU bus error
/* End of sun3 specific code. */

Lismerr:
	movl	#T_MMUFLT,%sp@-		| show that we are an MMU fault
	jra	_ASM_LABEL(faultstkadj)	| and deal with it
Lisaerr:
	movl	#T_ADDRERR,%sp@-	| mark address error
	jra	_ASM_LABEL(faultstkadj)	| and deal with it
Lisberr:
	movl	#T_BUSERR,%sp@-		| mark bus error
	jra	_ASM_LABEL(faultstkadj)	| and deal with it

/*
 * FP exceptions.
 */
GLOBAL(fpfline)
	clrl	%sp@-			| stack adjust count
	moveml	#0xFFFF,%sp@-		| save registers
	moveq	#T_FPEMULI,%d0		| denote as FP emulation trap
	jra	_ASM_LABEL(fault)	| do it

GLOBAL(fpunsupp)
	clrl	%sp@-			| stack adjust count
	moveml	#0xFFFF,%sp@-		| save registers
	moveq	#T_FPEMULD,%d0		| denote as FP emulation trap
	jra	_ASM_LABEL(fault)	| do it

/*
 * Handles all other FP coprocessor exceptions.
 * Note that since some FP exceptions generate mid-instruction frames
 * and may cause signal delivery, we need to test for stack adjustment
 * after the trap call.
 */
GLOBAL(fpfault)
	clrl	%sp@-		| stack adjust count
	moveml	#0xFFFF,%sp@-	| save user registers
	movl	%usp,%a0	| and save
	movl	%a0,%sp@(FR_SP)	|   the user stack pointer
	clrl	%sp@-		| no VA arg
	movl	_C_LABEL(curpcb),%a0	| current pcb
	lea	%a0@(PCB_FPCTX),%a0 | address of FP savearea
	fsave	%a0@		| save state
	tstb	%a0@		| null state frame?
	jeq	Lfptnull	| yes, safe
	clrw	%d0		| no, need to tweak BIU
	movb	%a0@(1),%d0	| get frame size
	bset	#3,%a0@(0,%d0:w) | set exc_pend bit of BIU
Lfptnull:
	fmovem	%fpsr,%sp@-	| push fpsr as code argument
	frestore %a0@		| restore state
	movl	#T_FPERR,%sp@-	| push type arg
	jra	_ASM_LABEL(faultstkadj) | call trap and deal with stack cleanup

/*
 * Other exceptions only cause four and six word stack frame and require
 * no post-trap stack adjustment.
 */
GLOBAL(badtrap)
	clrl	%sp@-			| stack adjust count
	moveml	#0xFFFF,%sp@-		| save std frame regs
	jbsr	_C_LABEL(straytrap)	| report
	moveml	%sp@+,#0xFFFF		| restore regs
	addql	#4,%sp			| stack adjust count
	jra	_ASM_LABEL(rei)		| all done

/*
 * Trap 0 is for system calls
 */
GLOBAL(trap0)
	clrl	%sp@-			| stack adjust count
	moveml	#0xFFFF,%sp@-		| save user registers
	movl	%usp,%a0		| save the user SP
	movl	%a0,%sp@(FR_SP)		|   in the savearea
	movl	%d0,%sp@-		| push syscall number
	jbsr	_C_LABEL(syscall)	| handle it
	addql	#4,%sp			| pop syscall arg
	movl	%sp@(FR_SP),%a0		| grab and restore
	movl	%a0,%usp		|   user SP
	moveml	%sp@+,#0x7FFF		| restore most registers
	addql	#8,%sp			| pop SP and stack adjust
	jra	_ASM_LABEL(rei)		| all done

/*
 * Trap 12 is the entry point for the cachectl "syscall"
 *	cachectl(command, addr, length)
 * command in %d0, addr in %a1, length in %d1
 */
GLOBAL(trap12)
	movl	_C_LABEL(curproc),%sp@-	| push curproc pointer
	movl	%d1,%sp@-		| push length
	movl	%a1,%sp@-		| push addr
	movl	%d0,%sp@-		| push command
	jbsr	_C_LABEL(cachectl1)	| do it
	lea	%sp@(16),%sp		| pop args
	jra	_ASM_LABEL(rei)		| all done

/*
 * Trace (single-step) trap.  Kernel-mode is special.
 * User mode traps are simply passed on to trap().
 */
GLOBAL(trace)
	clrl	%sp@-			| stack adjust count
	moveml	#0xFFFF,%sp@-
	moveq	#T_TRACE,%d0

	| Check PSW and see what happen.
	|   T=0 S=0	(should not happen)
	|   T=1 S=0	trace trap from user mode
	|   T=0 S=1	trace trap on a trap instruction
	|   T=1 S=1	trace trap from system mode (kernel breakpoint)

	movw	%sp@(FR_HW),%d1		| get PSW
	notw	%d1			| XXX no support for T0 on 680[234]0
	andw	#PSL_TS,%d1		| from system mode (T=1, S=1)?
	jeq	_ASM_LABEL(kbrkpt)	|  yes, kernel brkpt
	jra	_ASM_LABEL(fault)	| no, user-mode fault

/*
 * Trap 15 is used for:
 *	- GDB breakpoints (in user programs)
 *	- KGDB breakpoints (in the kernel)
 *	- trace traps for SUN binaries (not fully supported yet)
 * User mode traps are simply passed to trap().
 */
GLOBAL(trap15)
	clrl	%sp@-			| stack adjust count
	moveml	#0xFFFF,%sp@-
	moveq	#T_TRAP15,%d0
	btst	#5,%sp@(FR_HW)		| was supervisor mode?
	jne	_ASM_LABEL(kbrkpt)	|  yes, kernel brkpt
	jra	_ASM_LABEL(fault)	| no, user-mode fault

ASLOCAL(kbrkpt)
	| Kernel-mode breakpoint or trace trap. (%d0=trap_type)
	| Save the system sp rather than the user sp.
	movw	#PSL_HIGHIPL,%sr	| lock out interrupts
	lea	%sp@(FR_SIZE),%a6	| Save stack pointer
	movl	%a6,%sp@(FR_SP)		|  from before trap

	| If we are not on tmpstk switch to it.
	| (so debugger can change the stack pointer)
	movl	%a6,%d1
	cmpl	#_ASM_LABEL(tmpstk),%d1
	jls	Lbrkpt2 		| already on tmpstk
	| Copy frame to the temporary stack
	movl	%sp,%a0			| %a0=src
	lea	_ASM_LABEL(tmpstk)-96,%a1	| %a1=dst
	movl	%a1,%sp			| sp=new frame
	moveq	#FR_SIZE,%d1
Lbrkpt1:
	movl	%a0@+,%a1@+
	subql	#4,%d1
	bgt	Lbrkpt1

Lbrkpt2:
	| Call the trap handler for the kernel debugger.
	| Do not call trap() to handle it, so that we can
	| set breakpoints in trap() if we want.  We know
	| the trap type is either T_TRACE or T_BREAKPOINT.
	movl	%d0,%sp@-		| push trap type
	jbsr	_C_LABEL(trap_kdebug)
	addql	#4,%sp			| pop args

	| The stack pointer may have been modified, or
	| data below it modified (by kgdb push call),
	| so push the hardware frame at the current sp
	| before restoring registers and returning.
	movl	%sp@(FR_SP),%a0		| modified sp
	lea	%sp@(FR_SIZE),%a1	| end of our frame
	movl	%a1@-,%a0@-		| copy 2 longs with
	movl	%a1@-,%a0@-		| ... predecrement
	movl	%a0,%sp@(FR_SP)		| sp = h/w frame
	moveml	%sp@+,#0x7FFF		| restore all but sp
	movl	%sp@,%sp		| ... and sp
	rte				| all done

/* Use common m68k sigreturn */
#include <m68k/m68k/sigreturn.s>

/*
 * Interrupt handlers.  Most are auto-vectored,
 * and hard-wired the same way on all sun3 models.
 * Format in the stack is:
 *   %d0,%d1,%a0,%a1, sr, pc, vo
 */

#define INTERRUPT_SAVEREG \
	moveml	#0xC0C0,%sp@-

#define INTERRUPT_RESTORE \
	moveml	%sp@+,#0x0303

/*
 * This is the common auto-vector interrupt handler,
 * for which the CPU provides the vector=0x18+level.
 * These are installed in the interrupt vector table.
 */
	.align	2
GLOBAL(_isr_autovec)
	INTERRUPT_SAVEREG
	jbsr	_C_LABEL(isr_autovec)
	INTERRUPT_RESTORE
	jra	_ASM_LABEL(rei)

/* clock: see clock.c */
	.align	2
GLOBAL(_isr_clock)
	INTERRUPT_SAVEREG
	jbsr	_C_LABEL(clock_intr)
	INTERRUPT_RESTORE
	jra	_ASM_LABEL(rei)

| Handler for all vectored interrupts (i.e. VME interrupts)
	.align	2
GLOBAL(_isr_vectored)
	INTERRUPT_SAVEREG
	jbsr	_C_LABEL(isr_vectored)
	INTERRUPT_RESTORE
	jra	_ASM_LABEL(rei)

#undef	INTERRUPT_SAVEREG
#undef	INTERRUPT_RESTORE

/* interrupt counters (needed by vmstat) */
GLOBAL(intrnames)
	.asciz	"spur"	| 0
	.asciz	"lev1"	| 1
	.asciz	"lev2"	| 2
	.asciz	"lev3"	| 3
	.asciz	"lev4"	| 4
	.asciz	"clock"	| 5
	.asciz	"lev6"	| 6
	.asciz	"nmi"	| 7
GLOBAL(eintrnames)

	.data
	.even
GLOBAL(intrcnt)
	.long	0,0,0,0,0,0,0,0,0,0
GLOBAL(eintrcnt)
	.text

/*
 * Emulation of VAX REI instruction.
 *
 * This code is (mostly) un-altered from the hp300 code,
 * except that sun machines do not need a simulated SIR
 * because they have a real software interrupt register.
 *
 * This code deals with checking for and servicing ASTs
 * (profiling, scheduling) and software interrupts (network, softclock).
 * We check for ASTs first, just like the VAX.  To avoid excess overhead
 * the T_ASTFLT handling code will also check for software interrupts so we
 * do not have to do it here.  After identifying that we need an AST we
 * drop the IPL to allow device interrupts.
 *
 * This code is complicated by the fact that sendsig may have been called
 * necessitating a stack cleanup.
 */

ASGLOBAL(rei)
#ifdef	DIAGNOSTIC
	tstl	_C_LABEL(panicstr)	| have we paniced?
	jne	Ldorte			| yes, do not make matters worse
#endif
	tstl	_C_LABEL(astpending)	| AST pending?
	jeq	Ldorte			| no, done
Lrei1:
	btst	#5,%sp@			| yes, are we returning to user mode?
	jne	Ldorte			| no, done
	movw	#PSL_LOWIPL,%sr		| lower SPL
	clrl	%sp@-			| stack adjust
	moveml	#0xFFFF,%sp@-		| save all registers
	movl	%usp,%a1		| including
	movl	%a1,%sp@(FR_SP)		|    the users SP
	clrl	%sp@-			| VA == none
	clrl	%sp@-			| code == none
	movl	#T_ASTFLT,%sp@-		| type == async system trap
	jbsr	_C_LABEL(trap)		| go handle it
	lea	%sp@(12),%sp		| pop value args
	movl	%sp@(FR_SP),%a0		| restore user SP
	movl	%a0,%usp		|   from save area
	movw	%sp@(FR_ADJ),%d0	| need to adjust stack?
	jne	Laststkadj		| yes, go to it
	moveml	%sp@+,#0x7FFF		| no, restore most user regs
	addql	#8,%sp			| toss SP and stack adjust
	rte				| and do real RTE
Laststkadj:
	lea	%sp@(FR_HW),%a1		| pointer to HW frame
	addql	#8,%a1			| source pointer
	movl	%a1,%a0			| source
	addw	%d0,%a0			|  + hole size = dest pointer
	movl	%a1@-,%a0@-		| copy
	movl	%a1@-,%a0@-		|  8 bytes
	movl	%a0,%sp@(FR_SP)		| new SSP
	moveml	%sp@+,#0x7FFF		| restore user registers
	movl	%sp@,%sp		| and our SP
Ldorte:
	rte				| real return

/*
 * Initialization is at the beginning of this file, because the
 * kernel entry point needs to be at zero for compatibility with
 * the Sun boot loader.  This works on Sun machines because the
 * interrupt vector table for reset is NOT at address zero.
 * (The MMU has a "boot" bit that forces access to the PROM)
 */

/*
 * Use common m68k sigcode.
 */
#include <m68k/m68k/sigcode.s>
#ifdef COMPAT_SUNOS
#include <m68k/m68k/sunos_sigcode.s>
#endif
#ifdef COMPAT_SVR4
#include <m68k/m68k/svr4_sigcode.s>
#endif

	.text

/*
 * Primitives
 */

/*
 * Use common m68k support routines.
 */
#include <m68k/m68k/support.s>

BSS(want_resched,4)

/*
 * Use common m68k process manipulation routines.
 */
#include <m68k/m68k/proc_subr.s>

| Message for Lbadsw panic
Lsw0:
	.asciz	"cpu_switch"
	.even

	.data
GLOBAL(masterpaddr)		| XXX compatibility (debuggers)
GLOBAL(curpcb)
	.long	0
ASBSS(nullpcb,SIZEOF_PCB)
	.text

/*
 * At exit of a process, do a cpu_switch for the last time.
 * Switch to a safe stack and PCB, and select a new process to run.  The
 * old stack and u-area will be freed by the reaper.
 */
ENTRY(switch_exit)
	movl	%sp@(4),%a0		| struct proc *p
					| save state into garbage pcb
	movl	#_ASM_LABEL(nullpcb),_C_LABEL(curpcb)
	lea	_ASM_LABEL(tmpstk),%sp	| goto a tmp stack

	/* Schedule the vmspace and stack to be freed. */
	movl	%a0,%sp@-		| exit2(p)
	jbsr	_C_LABEL(exit2)
	lea	%sp@(4),%sp		| pop args

#if defined(LOCKDEBUG)
	/* Acquire sched_lock */
	jbsr	_C_LABEL(sched_lock_idle)
#endif

	jra	_C_LABEL(cpu_switch)

/*
 * When no processes are on the runq, cpu_switch() branches to idle
 * to wait for something to come ready.
 */
Lidle:
#if defined(LOCKDEBUG)
	/* Release sched_lock */
	jbsr	_C_LABEL(sched_unlock_idle)
#endif
	stop	#PSL_LOWIPL
GLOBAL(_Idle)				| See clock.c
	movw	#PSL_HIGHIPL,%sr
#if defined(LOCKDEBUG)
	/* Acquire sched_lock */
	jbsr	_C_LABEL(sched_lock_idle)
#endif
	movl	_C_LABEL(sched_whichqs),%d0
	jeq	Lidle
	jra	Lsw1

Lbadsw:
	movl	#Lsw0,%sp@-
	jbsr	_C_LABEL(panic)
	/*NOTREACHED*/

/*
 * cpu_switch()
 * Hacked for sun3
 */
ENTRY(cpu_switch)
	movl	_C_LABEL(curpcb),%a0	| current pcb
	movw	%sr,%a0@(PCB_PS)	| save sr before changing ipl
#ifdef notyet
	movl	_C_LABEL(curproc),%sp@-	| remember last proc running
#endif
	clrl	_C_LABEL(curproc)

	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */
	movl	_C_LABEL(sched_whichqs),%d0
	jeq	Lidle
Lsw1:
	/*
	 * Interrupts are blocked, sched_lock is held.  If
	 * we come here via Idle, %d0 contains the contents
	 * of a non-zero sched_whichqs.
	 */
	movl	%d0,%d1
	negl	%d0
	andl	%d1,%d0
	bfffo	%d0{#0:#32},%d1
	eorib	#31,%d1

	movl	%d1,%d0
	lslb	#3,%d1			| convert queue number to index
	addl	#_C_LABEL(sched_qs),%d1	| locate queue (q)
	movl	%d1,%a1
	movl	%a1@(P_FORW),%a0	| p = q->p_forw
	cmpal	%d1,%a0			| anyone on queue?
	jeq	Lbadsw			| no, panic
#ifdef DIAGNOSTIC
	tstl	%a0@(P_WCHAN)
	jne	Lbadsw
	cmpb	#SRUN,%a0@(P_STAT)
	jne	Lbadsw
#endif
	movl	%a0@(P_FORW),%a1@(P_FORW)	| q->p_forw = p->p_forw
	movl	%a0@(P_FORW),%a1		| n = p->p_forw
	movl	%a0@(P_BACK),%a1@(P_BACK)	| n->p_back = q
	cmpal	%d1,%a1			| anyone left on queue?
	jne	Lsw2			| yes, skip
	movl	_C_LABEL(sched_whichqs),%d1
	bclr	%d0,%d1			| no, clear bit
	movl	%d1,_C_LABEL(sched_whichqs)
Lsw2:
	/* p->p_cpu initialized in fork1() for single-processor */
	movb	#SONPROC,%a0@(P_STAT)	| p->p_stat = SONPROC
	movl	%a0,_C_LABEL(curproc)
	clrl	_C_LABEL(want_resched)
#ifdef notyet
	movl	%sp@+,%a1		| XXX - Make this work!
	cmpl	%a0,%a1			| switching to same proc?
	jeq	Lswdone			| yes, skip save and restore
#endif
	/*
	 * Save state of previous process in its pcb.
	 */
	movl	_C_LABEL(curpcb),%a1
	moveml	#0xFCFC,%a1@(PCB_REGS)	| save non-scratch registers
	movl	%usp,%a2		| grab USP (a2 has been saved)
	movl	%a2,%a1@(PCB_USP)	| and save it

	tstl	_C_LABEL(fputype)	| Do we have an fpu?
	jeq	Lswnofpsave		| No?  Then don't try save.
	lea	%a1@(PCB_FPCTX),%a2	| pointer to FP save area
	fsave	%a2@			| save FP state
	tstb	%a2@			| null state frame?
	jeq	Lswnofpsave		| yes, all done
	fmovem	%fp0-%fp7,%a2@(FPF_REGS)	| save FP general regs
	fmovem	%fpcr/%fpsr/%fpi,%a2@(FPF_FPCR)	| save FP control regs
Lswnofpsave:

	/*
	 * Now that we have saved all the registers that must be
	 * preserved, we are free to use those registers until
	 * we load the registers for the switched-to process.
	 * In this section, keep:  %a0=curproc, %a1=curpcb
	 */

	clrl	%a0@(P_BACK)		| clear back link
	movl	%a0@(P_ADDR),%a1	| get p_addr
	movl	%a1,_C_LABEL(curpcb)

#if defined(LOCKDEBUG)
	/*
	 * Done mucking with the run queues, release the
	 * scheduler lock, but keep interrupts out.
	 */
	movl	%a0,%sp@-		| not args...
	movl	%a1,%sp@-		| ...just saving
	jbsr	_C_LABEL(sched_unlock_idle)
	movl	%sp@+,%a1
	movl	%sp@+,%a0
#endif

	/*
	 * Load the new VM context (new MMU root pointer)
	 */
	movl	%a0@(P_VMSPACE),%a2	| vm = p->p_vmspace
#ifdef DIAGNOSTIC
	tstl	%a2			| vm == VM_MAP_NULL?
	jeq	Lbadsw			| panic
#endif
#if 1	/* XXX: PMAP_DEBUG */
	/*
	 * Just call _pmap_switch() for now.  Later on,
	 * use the in-line version below (for speed).
	 */
	movl	%a2@(VM_PMAP),%a2 	| pmap = vm->vm_map.pmap
	pea	%a2@			| push pmap
	jbsr	_C_LABEL(_pmap_switch)	| _pmap_switch(pmap)
	addql	#4,%sp
	movl	_C_LABEL(curpcb),%a1	| restore p_addr
| Note: pmap_switch will clear the cache if needed.
#else
	/* XXX - Later, use this unfinished inline.. */
	XXX	XXX	(PM_CTXNUM)
	movl	#IC_CLEAR,%d0
	movc	%d0,%cacr		| invalidate cache(s)
#endif

	/*
	 * Reload the registers for the new process.
	 * After this point we can only use %d0,%d1,%a0,%a1
	 */
	moveml	%a1@(PCB_REGS),#0xFCFC	| reload registers
	movl	%a1@(PCB_USP),%a0
	movl	%a0,%usp		| and USP

	tstl	_C_LABEL(fputype)	| If we don't have an fpu,
	jeq	Lres_skip		|  don't try to restore it.
	lea	%a1@(PCB_FPCTX),%a0	| pointer to FP save area
	tstb	%a0@			| null state frame?
	jeq	Lresfprest		| yes, easy
	fmovem	%a0@(FPF_FPCR),%fpcr/%fpsr/%fpi	| restore FP control regs
	fmovem	%a0@(FPF_REGS),%fp0-%fp7	| restore FP general regs
Lresfprest:
	frestore %a0@			| restore state
Lres_skip:
	movw	%a1@(PCB_PS),%d0	| no, restore PS
#ifdef DIAGNOSTIC
	btst	#13,%d0			| supervisor mode?
	jeq	Lbadsw			| no? panic!
#endif
	movw	%d0,%sr			| OK, restore PS
	moveq	#1,%d0			| return 1 (for alternate returns)
	movl	%d0,%a0
	rts

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	movl	%sp@(4),%a1
	movw	%sr,%a1@(PCB_PS)
	movl	%usp,%a0		| grab USP
	movl	%a0,%a1@(PCB_USP)	| and save it
	moveml	#0xFCFC,%a1@(PCB_REGS)	| save non-scratch registers

	tstl	_C_LABEL(fputype)	| Do we have FPU?
	jeq	Lsavedone		| No?  Then don't save state.
	lea	%a1@(PCB_FPCTX),%a0	| pointer to FP save area
	fsave	%a0@			| save FP state
	tstb	%a0@			| null state frame?
	jeq	Lsavedone		| yes, all done
	fmovem	%fp0-%fp7,%a0@(FPF_REGS)	| save FP general regs
	fmovem	%fpcr/%fpsr/%fpi,%a0@(FPF_FPCR)	| save FP control regs
Lsavedone:
	moveq	#0,%d0			| return 0
	movl	%d0,%a0
	rts

/* suline() */
/* TBIA, TBIS, TBIAS, TBIAU */

/*
 * Invalidate instruction cache
 */
ENTRY(ICIA)
	movl	#IC_CLEAR,%d0
	movc	%d0,%cacr		| invalidate i-cache
	rts

/* DCIA, DCIS */

/*
 * Invalidate data cache.
 */
ENTRY(DCIU)
	rts

/* ICPL, ICPP, DCPL, DCPP, DCPA, DCFL, DCFP */
/* PCIA, ecacheon, ecacheoff */

/*
 * Get callers current SP value.
 * Note that simply taking the address of a local variable in a C function
 * doesn't work because callee saved registers may be outside the stack frame
 * defined by A6 (e.g. GCC generated code).
 *
 * [I don't think the ENTRY() macro will do the right thing with this -- glass]
 */
GLOBAL(getsp)
	movl	%sp,%d0			| get current SP
	addql	#4,%d0			| compensate for return address
	movl	%d0,%a0
	rts

ENTRY(getsfc)
	movc	%sfc,%d0
	movl	%d0,%a0
	rts

ENTRY(getdfc)
	movc	%dfc,%d0
	movl	%d0,%a0
	rts

ENTRY(getvbr)
	movc	%vbr,%a0
	rts

ENTRY(setvbr)
	movl	%sp@(4),%d0
	movc	%d0,%vbr
	rts

/* loadustp, ptest_addr */

/*
 * Set processor priority level calls.  Most are implemented with
 * inline asm expansions.  However, we need one instantiation here
 * in case some non-optimized code makes external references.
 * Most places will use the inlined functions param.h supplies.
 */

ENTRY(_getsr)
	clrl	%d0
	movw	%sr,%d0
	movl	%d0,%a0
	rts

ENTRY(_spl)
	clrl	%d0
	movw	%sr,%d0
	movl	%sp@(4),%d1
	movw	%d1,%sr
	rts

ENTRY(_splraise)
	clrl	%d0
	movw	%sr,%d0
	movl	%d0,%d1
	andl	#PSL_HIGHIPL,%d1 	| old &= PSL_HIGHIPL
	cmpl	%sp@(4),%d1		| (old - new)
	bge	Lsplr
	movl	%sp@(4),%d1
	movw	%d1,%sr
Lsplr:
	rts

/*
 * Save and restore 68881 state.
 */
ENTRY(m68881_save)
	movl	%sp@(4),%a0		| save area pointer
	fsave	%a0@			| save state
	tstb	%a0@			| null state frame?
	jeq	Lm68881sdone		| yes, all done
	fmovem	%fp0-%fp7,%a0@(FPF_REGS)	| save FP general regs
	fmovem	%fpcr/%fpsr/%fpi,%a0@(FPF_FPCR)	| save FP control regs
Lm68881sdone:
	rts

ENTRY(m68881_restore)
	movl	%sp@(4),%a0		| save area pointer
	tstb	%a0@			| null state frame?
	jeq	Lm68881rdone		| yes, easy
	fmovem	%a0@(FPF_FPCR),%fpcr/%fpsr/%fpi	| restore FP control regs
	fmovem	%a0@(FPF_REGS),%fp0-%fp7	| restore FP general regs
Lm68881rdone:
	frestore %a0@			| restore state
	rts

/*
 * _delay(unsigned N)
 * Delay for at least (N/256) microseconds.
 * This routine depends on the variable:  delay_divisor
 * which should be set based on the CPU clock rate.
 * XXX: Currently this is set based on the CPU model,
 * XXX: but this should be determined at run time...
 */
GLOBAL(_delay)
	| %d0 = arg = (usecs << 8)
	movl	%sp@(4),%d0
	| %d1 = delay_divisor;
	movl	_C_LABEL(delay_divisor),%d1
	jra	L_delay			/* Jump into the loop! */

	/*
	 * Align the branch target of the loop to a half-line (8-byte)
	 * boundary to minimize cache effects.  This guarantees both
	 * that there will be no prefetch stalls due to cache line burst
	 * operations and that the loop will run from a single cache
	 * half-line.
	 */
	.align	8
L_delay:
	subl	%d1,%d0
	jgt	L_delay
	rts

| Define some addresses, mostly so DDB can print useful info.
| Not using _C_LABEL() here because these symbols are never
| referenced by any C code, and if the leading underscore
| ever goes away, these lines turn into syntax errors...
	.set	_KERNBASE,KERNBASE
	.set	_MONSTART,SUN3_MONSTART
	.set	_PROM_BASE,SUN3_PROM_BASE
	.set	_MONEND,SUN3_MONEND

|The end!
