/*	$NetBSD: locore.s,v 1.1.1.1.2.2 1997/01/14 20:57:10 gwr Exp $	*/

/*
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

#include "assym.h"
#include <machine/trap.h>

| Remember this is a fun project!

	.data
	.globl	_mon_crp
_mon_crp:
	.long	0,0

| This is for kvm_mkdb, and should be the address of the beginning
| of the kernel text segment (not necessarily the same as kernbase).
	.text
	.globl	_kernel_text
_kernel_text:

| This is the entry point, as well as the end of the temporary stack
| used during process switch (one 8K page ending at start)
	.globl tmpstk
tmpstk:
	.globl start
start:
| The first step, after disabling interrupts, is to map enough of the kernel
| into high virtual address space so that we can use position dependent code.
| This is a tricky task on the sun3x because the MMU is already enabled and
| the ROM monitor provides no indication of where the root MMU table is mapped.
| Therefore we must use one of the 68030's 'transparent translation' registers
| to define a range in the address space where the MMU translation is
| turned off.  Once this is complete we can modify the MMU table directly
| without the need for it to be mapped into virtual memory.
| All code must be position independent until otherwise noted, as the
| boot loader has loaded us into low memory but all the symbols in this
| code have been linked high.
	movw	#PSL_HIGHIPL, sr	| no interrupts
	movl	#KERNBASE, a5		| for vtop conversion
	lea	_mon_crp, a0		| where to store the CRP
	subl	a5, a0
	| Note: borrowing mon_crp for tt0 setup...
	movl	#0x3F8107, a0@		| map the low 1GB v=p with the
	pmove	a0@, tt0		| transparent translation reg0

| In order to map the kernel into high memory we will copy the root table
| entry which maps the 16 megabytes of memory starting at 0x0 into the
| entry which maps the 16 megabytes starting at KERNBASE.
	pmove	crp, a0@		| Get monitor CPU root pointer
	movl	a0@(4), a1		| 2nd word is PA of level A table

	movl	a1, a0			| compute the descriptor address
	addl	#0x3e0, a1		| for VA starting at KERNBASE
	movl	a0@, a1@		| copy descriptor type
	movl	a0@(4), a1@(4)		| copy physical address

| Kernel is now double mapped at zero and KERNBASE.
| Force a long jump to the relocated code (high VA).
	movl	#IC_CLEAR, d0		| Flush the I-cache
	movc	d0, cacr
	jmp L_high_code:l		| long jump

L_high_code:
| We are now running in the correctly relocated kernel, so
| we are no longer restricted to position-independent code.
| It is handy to leave transparent translation enabled while
| for the low 1GB while __bootstrap() is doing its thing.

| Do bootstrap stuff needed before main() gets called.
| Our boot loader leaves a copy of the kernel's exec header
| just before the start of the kernel text segment, so the
| kernel can sanity-check the DDB symbols at [end...esym].
| Pass the struct exec at tmpstk-32 to __bootstrap().
	lea	tmpstk-32, sp
	jsr	__bootstrap		| See _startup.c

| Now turn off the transparent translation of the low 1GB.
| (this also flushes the ATC)
	clrl	sp@-
	pmove	sp@,tt0
	addql	#4,sp

| Now that __bootstrap() is done using the PROM functions,
| we can safely set the sfc/dfc to something != FC_CONTROL
	moveq	#FC_USERD, d0		| make movs access "user data"
	movc	d0, sfc			| space for copyin/copyout
	movc	d0, dfc

| Setup process zero user/kernel stacks.
	movl	_proc0paddr,a1		| get proc0 pcb addr
	lea	a1@(USPACE-4),sp	| set SSP to last word
	movl	#USRSTACK-4,a2
	movl	a2,usp			| init user SP

| Note curpcb was already set in __bootstrap().
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
	clrw	sp@-			| vector offset/frame type
	clrl	sp@-			| PC - filled in by "execve"
	movw	#PSL_USER,sp@-		| in user mode
	clrl	sp@-			| stack adjust count and padding
	lea	sp@(-64),sp		| construct space for D0-D7/A0-A7
	lea	_proc0,a0		| proc0 in a0
	movl	sp,a0@(P_MDREGS)	| save frame for proc0
	movl	usp,a1
	movl	a1,sp@(FR_SP)		| save user stack pointer in frame
	jbsr	_main			| main()
	trap	#15			| should not get here

| This is used by cpu_fork() to return to user mode.
| It is called with SP pointing to a struct trapframe.
	.globl	_proc_do_uret
_proc_do_uret:
	movl	sp@(FR_SP),a0		| grab and load
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| load most registers (all but SSP)
	addql	#8,sp			| pop SSP and stack adjust count
	rte

/*
 * proc_trampoline:
 * This is used by cpu_set_kpc() to "push" a function call onto the
 * kernel stack of some process, very much like a signal delivery.
 * When we get here, the stack has:
 *
 * SP+8:	switchframe from before cpu_set_kpc
 * SP+4:	void *proc;
 * SP:  	u_long func;
 *
 * On entry, the switchframe pushed by cpu_set_kpc has already been
 * popped off the stack, so all this needs to do is pop the function
 * pointer into a register, call it, then pop the arg, and finally
 * return using the switchframe that remains on the stack.
 */
	.globl	_proc_trampoline
_proc_trampoline:
	movl	sp@+,a0			| function pointer
	jbsr	a0@			| (*func)(procp)
	addql	#4,sp			| toss the arg
	rts				| as cpu_switch would do

| That is all the assembly startup code we need on the sun3x!
| The rest of this is like the hp300/locore.s where possible.

/*
 * Trap/interrupt vector routines
 */

	.globl _buserr, _addrerr, _illinst, _zerodiv, _chkinst
	.globl _trapvinst, _privinst, _trace, _badtrap, _fmterr
	.globl _trap0, _trap1, _trap2, _trap12, _trap15
	.globl _coperr, _fpfline, _fpunsupp

	.globl	_trap, _nofault, _longjmp
_buserr:
	tstl	_nofault		| device probe?
	jeq	_addrerr		| no, handle as usual
	movl	_nofault,sp@-		| yes,
	jbsr	_longjmp		|  longjmp(nofault)
_addrerr:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	lea	sp@(FR_HW),a1		| grab base of HW berr frame
	moveq	#0,d0
	movw	a1@(10),d0		| grab SSW for fault processing
	btst	#12,d0			| RB set?
	jeq	LbeX0			| no, test RC
	bset	#14,d0			| yes, must set FB
	movw	d0,a1@(10)		| for hardware too
LbeX0:
	btst	#13,d0			| RC set?
	jeq	LbeX1			| no, skip
	bset	#15,d0			| yes, must set FC
	movw	d0,a1@(10)		| for hardware too
LbeX1:
	btst	#8,d0			| data fault?
	jeq	Lbe0			| no, check for hard cases
	movl	a1@(16),d1		| fault address is as given in frame
	jra	Lbe10			| thats it
Lbe0:
	btst	#4,a1@(6)		| long (type B) stack frame?
	jne	Lbe4			| yes, go handle
	movl	a1@(2),d1		| no, can use save PC
	btst	#14,d0			| FB set?
	jeq	Lbe3			| no, try FC
	addql	#4,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe3:
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, done
	addql	#2,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe4:
	movl	a1@(36),d1		| long format, use stage B address
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, all done
	subql	#2,d1			| yes, adjust address
Lbe10:
	movl	d1,sp@-			| push fault VA
	movl	d0,sp@-			| and padded SSW
	movw	a1@(6),d0		| get frame format/vector offset
	andw	#0x0FFF,d0		| clear out frame format
	cmpw	#12,d0			| address error vector?
	jeq	Lisaerr			| yes, go to it

/* MMU-specific code to determine reason for bus error. */
	movl	d1,a0			| fault address
	movl	sp@,d0			| function code from ssw
	btst	#8,d0			| data fault?
	jne	Lbe10a
	movql	#1,d0			| user program access FC
					| (we dont separate data/program)
	btst	#5,a1@			| supervisor mode?
	jeq	Lbe10a			| if no, done
	movql	#5,d0			| else supervisor program access
Lbe10a:
	ptestr	d0,a0@,#7		| do a table search
	pmove	psr,sp@			| save result
	movb	sp@,d1
	btst	#2,d1			| invalid? (incl. limit viol and berr)
	jeq	Lmightnotbemerr		| no -> wp check
	btst	#7,d1			| is it MMU table berr?
	jeq	Lismerr			| no, must be fast
	jra	Lisberr1		| real bus err needs not be fast
Lmightnotbemerr:
	btst	#3,d1			| write protect bit set?
	jeq	Lisberr1		| no, must be bus error
	movl	sp@,d0			| ssw into low word of d0
	andw	#0xc0,d0		| write protect is set on page:
	cmpw	#0x40,d0		| was it read cycle?
	jeq	Lisberr1		| yes, was not WPE, must be bus err
/* End of MMU-specific bus error code. */

Lismerr:
	movl	#T_MMUFLT,sp@-		| show that we are an MMU fault
	jra	Ltrapnstkadj		| and deal with it
Lisaerr:
	movl	#T_ADDRERR,sp@-		| mark address error
	jra	Ltrapnstkadj		| and deal with it
Lisberr1:
	clrw	sp@			| re-clear pad word
Lisberr:
	movl	#T_BUSERR,sp@-		| mark bus error
Ltrapnstkadj:
	jbsr	_trap			| handle the error
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore user SP
	movl	a0,usp			|   from save area
	movw	sp@(FR_ADJ),d0		| need to adjust stack?
	jne	Lstkadj			| yes, go to it
	moveml	sp@+,#0x7FFF		| no, restore most user regs
	addql	#8,sp			| toss SSP and stkadj
	jra	rei			| all done
Lstkadj:
	lea	sp@(FR_HW),a1		| pointer to HW frame
	addql	#8,a1			| source pointer
	movl	a1,a0			| source
	addw	d0,a0			|  + hole size = dest pointer
	movl	a1@-,a0@-		| copy
	movl	a1@-,a0@-		|  8 bytes
	movl	a0,sp@(FR_SP)		| new SSP
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
	jra	rei			| all done

/*
 * FP exceptions.
 */
_fpfline:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULI,d0		| denote as FP emulation trap
	jra	fault			| do it

_fpunsupp:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULD,d0		| denote as FP emulation trap
	jra	fault			| do it

/*
 * Handles all other FP coprocessor exceptions.
 * Note that since some FP exceptions generate mid-instruction frames
 * and may cause signal delivery, we need to test for stack adjustment
 * after the trap call.
 */
	.globl	_fpfault
_fpfault:
	clrl	sp@-		| stack adjust count
	moveml	#0xFFFF,sp@-	| save user registers
	movl	usp,a0		| and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	movl	_curpcb,a0	| current pcb
	lea	a0@(PCB_FPCTX),a0 | address of FP savearea
	fsave	a0@		| save state
	tstb	a0@		| null state frame?
	jeq	Lfptnull	| yes, safe
	clrw	d0		| no, need to tweak BIU
	movb	a0@(1),d0	| get frame size
	bset	#3,a0@(0,d0:w)	| set exc_pend bit of BIU
Lfptnull:
	fmovem	fpsr,sp@-	| push fpsr as code argument
	frestore a0@		| restore state
	movl	#T_FPERR,sp@-	| push type arg
	jra	Ltrapnstkadj	| call trap and deal with stack cleanup

/*
 * Coprocessor and format errors can generate mid-instruction stack
 * frames and cause signal delivery hence we need to check for potential
 * stack adjustment.
 */
_coperr:
	clrl	sp@-		| stack adjust count
	moveml	#0xFFFF,sp@-
	movl	usp,a0		| get and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	clrl	sp@-		| or code arg
	movl	#T_COPERR,sp@-	| push trap type
	jra	Ltrapnstkadj	| call trap and deal with stack adjustments

_fmterr:
	clrl	sp@-		| stack adjust count
	moveml	#0xFFFF,sp@-
	movl	usp,a0		| get and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	clrl	sp@-		| or code arg
	movl	#T_FMTERR,sp@-	| push trap type
	jra	Ltrapnstkadj	| call trap and deal with stack adjustments

/*
 * Other exceptions only cause four and six word stack frame and require
 * no post-trap stack adjustment.
 */
_illinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_ILLINST,d0
	jra	fault

_zerodiv:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_ZERODIV,d0
	jra	fault

_chkinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_CHKINST,d0
	jra	fault

_trapvinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_TRAPVINST,d0
	jra	fault

_privinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_PRIVINST,d0
	jra	fault

	.globl	fault
fault:
	movl	usp,a0			| get and save
	movl	a0,sp@(FR_SP)		|   the user stack pointer
	clrl	sp@-			| no VA arg
	clrl	sp@-			| or code arg
	movl	d0,sp@-			| push trap type
	jbsr	_trap			| handle trap
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most user regs
	addql	#8,sp			| pop SP and stack adjust
	jra	rei			| all done

	.globl	_straytrap
_badtrap:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save std frame regs
	jbsr	_straytrap		| report
	moveml	sp@+,#0xFFFF		| restore regs
	addql	#4, sp			| stack adjust count
	jra	rei			| all done

/*
 * Trap 0 is for system calls
 */
	.globl	_syscall
_trap0:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	d0,sp@-			| push syscall number
	jbsr	_syscall		| handle it
	addql	#4,sp			| pop syscall arg
	movl	sp@(FR_SP),a0		| grab and restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most registers
	addql	#8,sp			| pop SP and stack adjust
	jra	rei			| all done

/*
 * Trap 1 is either:
 * sigreturn (native NetBSD executable)
 * breakpoint (HPUX executable)
 */
_trap1:
#if 0 /* COMPAT_HPUX */
	/* If process is HPUX, this is a user breakpoint. */
	jne	trap15			| breakpoint
#endif
	/* fall into sigreturn */

/*
 * The sigreturn() syscall comes here.  It requires special handling
 * because we must open a hole in the stack to fill in the (possibly much
 * larger) original stack frame.
 */
sigreturn:
	lea	sp@(-84),sp		| leave enough space for largest frame
	movl	sp@(84),sp@		| move up current 8 byte frame
	movl	sp@(88),sp@(4)
	movl	#84,sp@-		| default: adjust by 84 bytes
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	#SYS_sigreturn,sp@-	| push syscall number
	jbsr	_syscall		| handle it
	addql	#4,sp			| pop syscall#
	movl	sp@(FR_SP),a0		| grab and restore
	movl	a0,usp			|   user SP
	lea	sp@(FR_HW),a1		| pointer to HW frame
	movw	sp@(FR_ADJ),d0		| do we need to adjust the stack?
	jeq	Lsigr1			| no, just continue
	moveq	#92,d1			| total size
	subw	d0,d1			|  - hole size = frame size
	lea	a1@(92),a0		| destination
	addw	d1,a1			| source
	lsrw	#1,d1			| convert to word count
	subqw	#1,d1			| minus 1 for dbf
Lsigrlp:
	movw	a1@-,a0@-		| copy a word
	dbf	d1,Lsigrlp		| continue
	movl	a0,a1			| new HW frame base
Lsigr1:
	movl	a1,sp@(FR_SP)		| new SP value
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
	jra	rei			| all done

/*
 * Trap 2 is one of:
 * NetBSD: not used (ignore)
 * SunOS:  Some obscure FPU operation
 * HPUX:   sigreturn
 */
_trap2:
#if 0 /* COMPAT_HPUX */
	/* XXX:	If HPUX, this is a user breakpoint. */
	jne	sigreturn
#endif
	/* fall into trace (NetBSD or SunOS) */

/*
 * Trace (single-step) trap.  Kernel-mode is special.
 * User mode traps are simply passed on to trap().
 */
_trace:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-
	moveq	#T_TRACE,d0
	movw	sp@(FR_HW),d1		| get PSW
	andw	#PSL_S,d1		| from system mode?
	jne	kbrkpt			| yes, kernel breakpoint
	jra	fault			| no, user-mode fault

/*
 * Trap 15 is used for:
 *	- GDB breakpoints (in user programs)
 *	- KGDB breakpoints (in the kernel)
 *	- trace traps for SUN binaries (not fully supported yet)
 * User mode traps are passed simply passed to trap()
 */
_trap15:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-
	moveq	#T_TRAP15,d0
	movw	sp@(FR_HW),d1		| get PSW
	andw	#PSL_S,d1		| from system mode?
	jne	kbrkpt			| yes, kernel breakpoint
	jra	fault			| no, user-mode fault

kbrkpt:	| Kernel-mode breakpoint or trace trap. (d0=trap_type)
	| Save the system sp rather than the user sp.
	movw	#PSL_HIGHIPL,sr		| lock out interrupts
	lea	sp@(FR_SIZE),a6		| Save stack pointer
	movl	a6,sp@(FR_SP)		|  from before trap

	| If we are not on tmpstk switch to it.
	| (so debugger can change the stack pointer)
	movl	a6,d1
	cmpl	#tmpstk,d1
	jls	Lbrkpt2 		| already on tmpstk
	| Copy frame to the temporary stack
	movl	sp,a0			| a0=src
	lea	tmpstk-96,a1		| a1=dst
	movl	a1,sp			| sp=new frame
	moveq	#FR_SIZE,d1
Lbrkpt1:
	movl	a0@+,a1@+
	subql	#4,d1
	bgt	Lbrkpt1

Lbrkpt2:
	| Call the trap handler for the kernel debugger.
	| Do not call trap() to do it, so that we can
	| set breakpoints in trap() if we want.  We know
	| the trap type is either T_TRACE or T_BREAKPOINT.
	| If we have both DDB and KGDB, let KGDB see it first,
	| because KGDB will just return 0 if not connected.
	| Save args in d2, a2
	movl	d0,d2			| trap type
	movl	sp,a2			| frame ptr
#ifdef	KGDB
	| Let KGDB handle it (if connected)
	movl	a2,sp@-			| push frame ptr
	movl	d2,sp@-			| push trap type
	jbsr	_kgdb_trap		| handle the trap
	addql	#8,sp			| pop args
	cmpl	#0,d0			| did kgdb handle it
	jne	Lbrkpt3			| yes, done
#endif
#ifdef	DDB
	| Let DDB handle it.
	movl	a2,sp@-			| push frame ptr
	movl	d2,sp@-			| push trap type
	jbsr	_kdb_trap		| handle the trap
	addql	#8,sp			| pop args
	cmpl	#0,d0			| did ddb handle it
	jne	Lbrkpt3			| yes, done
#endif
	| Drop into the PROM temporarily...
	movl	a2,sp@-			| push frame ptr
	movl	d2,sp@-			| push trap type
	jbsr	_nodb_trap		| handle the trap
	addql	#8,sp			| pop args
Lbrkpt3:
	| The stack pointer may have been modified, or
	| data below it modified (by kgdb push call),
	| so push the hardware frame at the current sp
	| before restoring registers and returning.

	movl	sp@(FR_SP),a0		| modified sp
	lea	sp@(FR_SIZE),a1		| end of our frame
	movl	a1@-,a0@-		| copy 2 longs with
	movl	a1@-,a0@-		| ... predecrement
	movl	a0,sp@(FR_SP)		| sp = h/w frame
	moveml	sp@+,#0x7FFF		| restore all but sp
	movl	sp@,sp			| ... and sp
	rte				| all done

/*
 * Trap 12 is the entry point for the cachectl "syscall"
 *	cachectl(command, addr, length)
 * command in d0, addr in a1, length in d1
 */
	.globl	_cachectl
_trap12:
	movl	d1,sp@-			| push length
	movl	a1,sp@-			| push addr
	movl	d0,sp@-			| push command
	jbsr	_cachectl		| do it
	lea	sp@(12),sp		| pop args
	jra	rei			| all done

/*
 * Interrupt handlers.  Most are auto-vectored,
 * and hard-wired the same way on all sun3 models.
 * Format in the stack is:
 *   d0,d1,a0,a1, sr, pc, vo
 */

#define INTERRUPT_SAVEREG \
	moveml	#0xC0C0,sp@-

#define INTERRUPT_RESTORE \
	moveml	sp@+,#0x0303

/*
 * This is the common auto-vector interrupt handler,
 * for which the CPU provides the vector=0x18+level.
 * These are installed in the interrupt vector table.
 */
	.align	2
	.globl	__isr_autovec, _isr_autovec
__isr_autovec:
	INTERRUPT_SAVEREG
	jbsr	_isr_autovec
	INTERRUPT_RESTORE
	jra	rei

/* clock: see clock.c */
	.align	2
	.globl	__isr_clock, _clock_intr
__isr_clock:
	INTERRUPT_SAVEREG
	jbsr	_clock_intr
	INTERRUPT_RESTORE
	jra	rei

| Handler for all vectored interrupts (i.e. VME interrupts)
	.align	2
	.globl	__isr_vectored, _isr_vectored
__isr_vectored:
	INTERRUPT_SAVEREG
	jbsr	_isr_vectored
	INTERRUPT_RESTORE
	jra	rei

#undef	INTERRUPT_SAVEREG
#undef	INTERRUPT_RESTORE

/* interrupt counters (needed by vmstat) */
	.globl	_intrcnt,_eintrcnt,_intrnames,_eintrnames
_intrnames:
	.asciz	"spur"	| 0
	.asciz	"lev1"	| 1
	.asciz	"lev2"	| 2
	.asciz	"lev3"	| 3
	.asciz	"lev4"	| 4
	.asciz	"clock"	| 5
	.asciz	"lev6"	| 6
	.asciz	"nmi"	| 7
_eintrnames:

	.data
	.even
_intrcnt:
	.long	0,0,0,0,0,0,0,0,0,0
_eintrcnt:
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

	.globl	_astpending
	.globl	rei
rei:
#ifdef	DIAGNOSTIC
	tstl	_panicstr		| have we paniced?
	jne	Ldorte			| yes, do not make matters worse
#endif
	tstl	_astpending		| AST pending?
	jeq	Ldorte			| no, done
Lrei1:
	btst	#5,sp@			| yes, are we returning to user mode?
	jne	Ldorte			| no, done
	movw	#PSL_LOWIPL,sr		| lower SPL
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_ASTFLT,sp@-		| type == async system trap
	jbsr	_trap			| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore user SP
	movl	a0,usp			|   from save area
	movw	sp@(FR_ADJ),d0		| need to adjust stack?
	jne	Laststkadj		| yes, go to it
	moveml	sp@+,#0x7FFF		| no, restore most user regs
	addql	#8,sp			| toss SP and stack adjust
	rte				| and do real RTE
Laststkadj:
	lea	sp@(FR_HW),a1		| pointer to HW frame
	addql	#8,a1			| source pointer
	movl	a1,a0			| source
	addw	d0,a0			|  + hole size = dest pointer
	movl	a1@-,a0@-		| copy
	movl	a1@-,a0@-		|  8 bytes
	movl	a0,sp@(FR_SP)		| new SSP
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
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
 * Signal "trampoline" code (18 bytes).  Invoked from RTE setup by sendsig().
 *
 * Stack looks like:
 *
 *	sp+0 ->	signal number
 *	sp+4	signal specific code
 *	sp+8	pointer to signal context frame (scp)
 *	sp+12	address of handler
 *	sp+16	saved hardware state
 *			.
 *			.
 *	scp+0->	beginning of signal context frame
 */
	.globl	_sigcode, _esigcode
	.data
	.align	2
_sigcode:	/* Found at address: 0x0DFFffdc */
	movl	sp@(12),a0		| signal handler addr	(4 bytes)
	jsr	a0@			| call signal handler	(2 bytes)
	addql	#4,sp			| pop signo		(2 bytes)
	trap	#1			| special syscall entry	(2 bytes)
	movl	d0,sp@(4)		| save errno		(4 bytes)
	moveq	#1,d0			| syscall == exit	(2 bytes)
	trap	#0			| exit(errno)		(2 bytes)
	.align	2
_esigcode:
	.text

/* XXX - hp300 still has icode here... */

/*
 * Primitives
 */
#include <machine/asm.h>

/* XXX copypage(fromaddr, toaddr) */

/*
 * non-local gotos
 */
ENTRY(setjmp)
	movl	sp@(4),a0	| savearea pointer
	moveml	#0xFCFC,a0@	| save d2-d7/a2-a7
	movl	sp@,a0@(48)	| and return address
	moveq	#0,d0		| return 0
	rts

ENTRY(longjmp)
	movl	sp@(4),a0
	moveml	a0@+,#0xFCFC
	movl	a0@,sp@
	moveq	#1,d0
	rts

/*
 * The following primitives manipulate the run queues.
 * _whichqs tells which of the 32 queues _qs have processes in them.
 * Setrunqueue puts processes into queues, Remrunqueue removes them
 * from queues.  The running process is on no queue, other processes
 * are on a queue related to p->p_priority, divided by 4 actually to
 * shrink the 0-127 range of priorities into the 32 available queues.
 */

	.globl	_whichqs,_qs,_cnt,_panic
	.globl	_curproc
	.comm	_want_resched,4

/*
 * setrunqueue(p)
 *
 * Call should be made at splclock(), and p->p_stat should be SRUN
 */
ENTRY(setrunqueue)
	movl	sp@(4),a0
#ifdef DIAGNOSTIC
	tstl	a0@(P_BACK)
	jne	Lset1
	tstl	a0@(P_WCHAN)
	jne	Lset1
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lset1
#endif
	clrl	d0
	movb	a0@(P_PRIORITY),d0
	lsrb	#2,d0
	movl	_whichqs,d1
	bset	d0,d1
	movl	d1,_whichqs
	lslb	#3,d0
	addl	#_qs,d0
	movl	d0,a0@(P_FORW)
	movl	d0,a1
	movl	a1@(P_BACK),a0@(P_BACK)
	movl	a0,a1@(P_BACK)
	movl	a0@(P_BACK),a1
	movl	a0,a1@(P_FORW)
	rts
#ifdef DIAGNOSTIC
Lset1:
	movl	#Lset2,sp@-
	jbsr	_panic
Lset2:
	.asciz	"setrunqueue"
	.even
#endif

/*
 * remrunqueue(p)
 *
 * Call should be made at splclock().
 */
ENTRY(remrunqueue)
	movl	sp@(4),a0		| proc *p
	clrl	d0
	movb	a0@(P_PRIORITY),d0
	lsrb	#2,d0
	movl	_whichqs,d1
	bclr	d0,d1			| if ((d1 & (1 << d0)) == 0)
	jeq	Lrem2			|   panic (empty queue)
	movl	d1,_whichqs
	movl	a0@(P_FORW),a1
	movl	a0@(P_BACK),a1@(P_BACK)
	movl	a0@(P_BACK),a1
	movl	a0@(P_FORW),a1@(P_FORW)
	movl	#_qs,a1
	movl	d0,d1
	lslb	#3,d1
	addl	d1,a1
	cmpl	a1@(P_FORW),a1
	jeq	Lrem1
	movl	_whichqs,d1
	bset	d0,d1
	movl	d1,_whichqs
Lrem1:
	clrl	a0@(P_BACK)
	rts
Lrem2:
	movl	#Lrem3,sp@-
	jbsr	_panic
Lrem3:
	.asciz	"remrunqueue"


| Message for Lbadsw panic
Lsw0:
	.asciz	"cpu_switch"
	.even

	.globl	_curpcb
	.globl	_masterpaddr	| XXX compatibility (debuggers)
	.data
_masterpaddr:			| XXX compatibility (debuggers)
_curpcb:
	.long	0
mdpflag:
	.byte	0		| copy of proc md_flags low byte
	.align	2
	.comm	nullpcb,SIZEOF_PCB
	.text

/*
 * At exit of a process, do a cpu_switch for the last time.
 * Switch to a safe stack and PCB, and deallocate the process's resources.
 * The ipl is high enough to prevent the memory from being reallocated.
 */
ENTRY(switch_exit)
	movl	sp@(4),a0		| struct proc *p
	movl	#nullpcb,_curpcb	| save state into garbage pcb
	lea	tmpstk,sp		| goto a tmp stack
	movl	a0,sp@-			| pass proc ptr down

	/* Free old process's u-area. */
	movl	#USPACE,sp@-		| size of u-area
	movl	a0@(P_ADDR),sp@-	| address of process's u-area
	movl	_kernel_map,sp@-	| map it was allocated in
	jbsr	_kmem_free		| deallocate it
	lea	sp@(12),sp		| pop args

	jra	_cpu_switch

/*
 * When no processes are on the runq, cpu_switch() branches to idle
 * to wait for something to come ready.
 */
	.data
	.globl _Idle_count
_Idle_count:
	.long	0
	.text

	.globl	Idle
Lidle:
	stop	#PSL_LOWIPL
Idle:
	movw	#PSL_HIGHIPL,sr
	addql	#1, _Idle_count
	tstl	_whichqs
	jeq	Lidle
	movw	#PSL_LOWIPL,sr
	jra	Lsw1

Lbadsw:
	movl	#Lsw0,sp@-
	jbsr	_panic
	/*NOTREACHED*/

/*
 * cpu_switch()
 * Hacked for sun3
 * XXX - Arg 1 is a proc pointer (curproc) but this doesn't use it.
 * XXX - Sould we use p->p_addr instead of curpcb? -gwr
 */
ENTRY(cpu_switch)
	movl	_curpcb,a1		| current pcb
	movw	sr,a1@(PCB_PS)		| save sr before changing ipl
#ifdef notyet
	movl	_curproc,sp@-		| remember last proc running
#endif
	clrl	_curproc

Lsw1:
	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */
	clrl	d0
	lea	_whichqs,a0
	movl	a0@,d1
Lswchk:
	btst	d0,d1
	jne	Lswfnd
	addqb	#1,d0
	cmpb	#32,d0
	jne	Lswchk
	jra	Idle
Lswfnd:
	movw	#PSL_HIGHIPL,sr		| lock out interrupts
	movl	a0@,d1			| and check again...
	bclr	d0,d1
	jeq	Lsw1			| proc moved, rescan
	movl	d1,a0@			| update whichqs
	moveq	#1,d1			| double check for higher priority
	lsll	d0,d1			| process (which may have snuck in
	subql	#1,d1			| while we were finding this one)
	andl	a0@,d1
	jeq	Lswok			| no one got in, continue
	movl	a0@,d1
	bset	d0,d1			| otherwise put this one back
	movl	d1,a0@
	jra	Lsw1			| and rescan
Lswok:
	movl	d0,d1
	lslb	#3,d1			| convert queue number to index
	addl	#_qs,d1			| locate queue (q)
	movl	d1,a1
	cmpl	a1@(P_FORW),a1		| anyone on queue?
	jeq	Lbadsw			| no, panic
	movl	a1@(P_FORW),a0		| p = q->p_forw
	movl	a0@(P_FORW),a1@(P_FORW)	| q->p_forw = p->p_forw
	movl	a0@(P_FORW),a1		| q = p->p_forw
	movl	a0@(P_BACK),a1@(P_BACK)	| q->p_back = p->p_back
	cmpl	a0@(P_FORW),d1		| anyone left on queue?
	jeq	Lsw2			| no, skip
	movl	_whichqs,d1
	bset	d0,d1			| yes, reset bit
	movl	d1,_whichqs
Lsw2:
	movl	a0,_curproc
	clrl	_want_resched
#ifdef notyet
	movl	sp@+,a1			| XXX - Make this work!
	cmpl	a0,a1			| switching to same proc?
	jeq	Lswdone			| yes, skip save and restore
#endif
	/*
	 * Save state of previous process in its pcb.
	 */
	movl	_curpcb,a1
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers
	movl	usp,a2			| grab USP (a2 has been saved)
	movl	a2,a1@(PCB_USP)		| and save it

	tstl	_fpu_type		| Do we have an fpu?
	jeq	Lswnofpsave		| No?  Then don't try save.
	lea	a1@(PCB_FPCTX),a2	| pointer to FP save area
	fsave	a2@			| save FP state
	tstb	a2@			| null state frame?
	jeq	Lswnofpsave		| yes, all done
	fmovem	fp0-fp7,a2@(FPF_REGS)		| save FP general regs
	fmovem	fpcr/fpsr/fpi,a2@(FPF_FPCR)	| save FP control regs
Lswnofpsave:

#ifdef DIAGNOSTIC
	tstl	a0@(P_WCHAN)
	jne	Lbadsw
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lbadsw
#endif
	clrl	a0@(P_BACK)		| clear back link
	movl	a0@(P_ADDR),a1		| get p_addr
	movl	a1,_curpcb
	movb	a0@(P_MDFLAG+3),mdpflag	| low byte of p_md.md_flags

	/* Our pmap does not need pmap_activate() */
	/* Just load the new CPU Root Pointer (MMU) */

	movl	#CACHE_CLR,d0
	movc	d0,cacr			| invalidate cache(s)
	pflusha				| flush entire TLB

	movl	a1@(PCB_MMUCTX),a0	| get CRP phys. addr.
	pmove	a0@,crp			| load new user root pointer

	| Reload registers of new process.
	moveml	a1@(PCB_REGS),#0xFCFC	| kernel registers
	movl	a1@(PCB_USP),a0
	movl	a0,usp			| and USP

	tstl	_fpu_type		| If we don't have an fpu,
	jeq	Lres_skip		|  don't try to restore it.
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	tstb	a0@			| null state frame?
	jeq	Lresfprest		| yes, easy
	fmovem	a0@(FPF_FPCR),fpcr/fpsr/fpi	| restore FP control regs
	fmovem	a0@(FPF_REGS),fp0-fp7		| restore FP general regs
Lresfprest:
	frestore a0@			| restore state
Lres_skip:
	movw	a1@(PCB_PS),d0		| no, restore PS
#ifdef DIAGNOSTIC
	btst	#13,d0			| supervisor mode?
	jeq	Lbadsw			| no? panic!
#endif
	movw	d0,sr			| OK, restore PS
	moveq	#1,d0			| return 1 (for alternate returns)
	rts

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	movl	sp@(4),a1
	movw	sr,a1@(PCB_PS)
	movl	usp,a0			| grab USP
	movl	a0,a1@(PCB_USP)		| and save it
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers

	tstl	_fpu_type		| Do we have FPU?
	jeq	Lsavedone		| No?  Then don't save state.
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	fsave	a0@			| save FP state
	tstb	a0@			| null state frame?
	jeq	Lsavedone		| yes, all done
	fmovem	fp0-fp7,a0@(FPF_REGS)		| save FP general regs
	fmovem	fpcr/fpsr/fpi,a0@(FPF_FPCR)	| save FP control regs
Lsavedone:
	moveq	#0,d0			| return 0
	rts

/* suline() `040 only */

#ifdef DEBUG
	.data
	.globl	fulltflush, fullcflush
fulltflush:
	.long	0
fullcflush:
	.long	0
	.text
#endif

/*
 * Invalidate entire TLB.
 */
ENTRY(TBIA)
__TBIA:
	pflusha
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
ENTRY(TBIS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush entire TLB
#endif
	movl	sp@(4),a0
	pflush	#0,#0,a0@		| flush address from both sides
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip data cache
	rts

/*
 * Invalidate supervisor side of TLB
 */
ENTRY(TBIAS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
	pflush	#4,#4			| flush supervisor TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

/*
 * Invalidate user side of TLB
 */
ENTRY(TBIAU)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
	pflush	#0,#4			| flush user TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

/*
 * Invalidate instruction cache
 */
ENTRY(ICIA)
	movl	#IC_CLEAR,d0
	movc	d0,cacr			| invalidate i-cache
	rts

/*
 * Invalidate data cache.
 * NOTE: we do not flush 68030 on-chip cache as there are no aliasing
 * problems with DC_WA.  The only cases we have to worry about are context
 * switch and TLB changes, both of which are handled "in-line" in resume
 * and TBI*.
 */
ENTRY(DCIA)
__DCIA:
	rts

ENTRY(DCIS)
__DCIS:
	rts

/*
 * Invalidate data cache.
 */
ENTRY(DCIU)
	rts

/* ICPL, ICPP, DCPL, DCPP, DCPA, DCFL, DCFP */

ENTRY(PCIA)
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

ENTRY(ecacheon)
	rts

ENTRY(ecacheoff)
	rts

/*
 * Get callers current SP value.
 * Note that simply taking the address of a local variable in a C function
 * doesn't work because callee saved registers may be outside the stack frame
 * defined by A6 (e.g. GCC generated code).
 *
 * [I don't think the ENTRY() macro will do the right thing with this -- glass]
 */
	.globl	_getsp
_getsp:
	movl	sp,d0			| get current SP
	addql	#4,d0			| compensate for return address
	rts

ENTRY(getsfc)
	movc	sfc,d0
	rts

ENTRY(getdfc)
	movc	dfc,d0
	rts

ENTRY(getvbr)
	movc vbr, d0
	rts

ENTRY(setvbr)
	movl sp@(4), d0
	movc d0, vbr
	rts

/*
 * Load a new CPU Root Pointer (CRP) into the MMU.
 * Arg is the address of CRP storage (2 longwords)
 * i.e.:
 *	struct { int limit, paddr; } CRP;
 *	loadcrp(&CRP);
 */
ENTRY(loadcrp)
	movl	sp@(4),a0		| arg1: &CRP
	movl	#CACHE_CLR,d0
	movc	d0,cacr			| invalidate cache(s)
	pflusha				| flush entire TLB
	pmove	a0@,crp			| load new user root pointer
	rts

/*
 * Set processor priority level calls.  Most are implemented with
 * inline asm expansions.  However, we need one instantiation here
 * in case some non-optimized code makes external references.
 * Most places will use the inlined function param.h supplies.
 */

ENTRY(_spl)
	movl	sp@(4),d1
	clrl	d0
	movw	sr,d0
	movw	d1,sr
	rts

ENTRY(getsr)
	moveq	#0, d0
	movw	sr, d0
	rts

ENTRY(_insque)
	movw	sr,d0
	movw	#PSL_HIGHIPL,sr		| atomic
	movl	sp@(8),a0		| where to insert (after)
	movl	sp@(4),a1		| element to insert (e)
	movl	a0@,a1@			| e->next = after->next
	movl	a0,a1@(4)		| e->prev = after
	movl	a1,a0@			| after->next = e
	movl	a1@,a0
	movl	a1,a0@(4)		| e->next->prev = e
	movw	d0,sr
	rts

ENTRY(_remque)
	movw	sr,d0
	movw	#PSL_HIGHIPL,sr		| atomic
	movl	sp@(4),a0		| element to remove (e)
	movl	a0@,a1
	movl	a0@(4),a0
	movl	a0,a1@(4)		| e->next->prev = e->prev
	movl	a1,a0@			| e->prev->next = e->next
	movw	d0,sr
	rts

/*
 * Save and restore 68881 state.
 */
ENTRY(m68881_save)
	movl	sp@(4),a0		| save area pointer
	fsave	a0@			| save state
	tstb	a0@			| null state frame?
	jeq	Lm68881sdone		| yes, all done
	fmovem fp0-fp7,a0@(FPF_REGS)		| save FP general regs
	fmovem fpcr/fpsr/fpi,a0@(FPF_FPCR)	| save FP control regs
Lm68881sdone:
	rts

ENTRY(m68881_restore)
	movl	sp@(4),a0		| save area pointer
	tstb	a0@			| null state frame?
	jeq	Lm68881rdone		| yes, easy
	fmovem	a0@(FPF_FPCR),fpcr/fpsr/fpi	| restore FP control regs
	fmovem	a0@(FPF_REGS),fp0-fp7		| restore FP general regs
Lm68881rdone:
	frestore a0@			| restore state
	rts

/*
 * _delay(unsigned N)
 * Delay for at least (N/256) microseconds.
 * This routine depends on the variable:  delay_divisor
 * which should be set based on the CPU clock rate.
 * XXX: Currently this is set in sun3_startup.c based on the
 * XXX: CPU model but this should be determined at run time...
 */
	.globl	__delay
__delay:
	| d0 = arg = (usecs << 8)
	movl	sp@(4),d0
	| d1 = delay_divisor;
	movl	_delay_divisor,d1
L_delay:
	subl	d1,d0
	jgt	L_delay
	rts


| Define some addresses, mostly so DDB can print useful info.
	.globl	_kernbase
	.set	_kernbase,KERNBASE
	.globl	_dvma_base
	.set	_dvma_base,DVMA_SPACE_START
	.globl	_prom_start
	.set	_prom_start,MONSTART
	.globl	_prom_base
	.set	_prom_base,PROM_BASE

|The end!
