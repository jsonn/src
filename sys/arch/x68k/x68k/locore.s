/*	$NetBSD: locore.s,v 1.37.4.5 1999/02/13 18:01:42 minoura Exp $	*/

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
 * from: Utah $Hdr: locore.s 1.66 92/12/22$
 *
 *	@(#)locore.s	8.6 (Berkeley) 5/27/94
 */

#include "opt_compat_netbsd.h"
#include "opt_ddb.h"
#include "opt_uvm.h"

#include "ite.h"
#include "mha.h"
#include "fd.h"
#include "par.h"
#include "assym.h"

#include <machine/asm.h>

| This is for kvm_mkdb, and should be the address of the beginning
| of the kernel text segment (not necessarily the same as kernbase).
	.text
GLOBAL(kernel_text)

/*
 * Temporary stack for a variety of purposes.
 * Try and make this the first thing is the data segment so it
 * is page aligned.  Note that if we overflow here, we run into
 * our text segment.
 */
	.data
	.space	NBPG
ASLOCAL(tmpstk)

#include <x68k/x68k/vectors.s>

	.text
/*
 * This is where we wind up if the kernel jumps to location 0.
 * (i.e. a bogus PC)  This is known to immediately follow the vector
 * table and is hence at 0x400 (see reset vector in vectors.s).
 */
	.globl	_panic
	pea	Ljmp0panic
	jbsr	_panic
	/* NOTREACHED */
Ljmp0panic:
	.asciz	"kernel jump to zero"
	.even

/*
 * Do a dump.
 * Called by auto-restart.
 */
	.globl	_dumpsys
	.globl	_doadump
_doadump:
	jbsr	_dumpsys
	jbsr	_doboot
	/*NOTREACHED*/

/*
 * Trap/interrupt vector routines
 */ 
#include <m68k/m68k/trap_subr.s>

	.globl	_trap, _nofault, _longjmp
	.globl	_buserr60		| for 060SP
_buserr60:
_buserr:
	tstl	_nofault		| device probe?
	jeq	Lberr			| no, handle as usual
	movl	_nofault,sp@-		| yes,
	jbsr	_longjmp		|  longjmp(nofault)
Lberr:
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_mmutype	| 68040/060?
	jne	_addrerr		| no, skip
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	lea	sp@(FR_HW),a1		| grab base of HW berr frame
#if defined(M68060)
	cmpl	#CPU_68060,_cputype	| 68060?
	jne	Lbenot060
	movel	a1@(12),d0		| grap FSLW
	btst	#2,d0			| branch prediction error?
	jeq	Lnobpe			| no, skip
	movc	cacr,d1
	orl	#IC60_CABC,d1		| clear all branch cache entries
	movc	d1,cacr
	movl	d0,d1
	andl	#0x7ffd,d1		| check other faults
	jeq	_ASM_LABEL(faultstkadjnotrap)
Lnobpe:
| XXX this is not needed.
|	movl	d0,sp@			| code is FSLW now.

| we need to adjust for misaligned addresses
	movl	a1@(8),d1		| grab VA
	btst	#27,d0			| check for mis-aligned access
	jeq	Lberr3			| no, skip
	addl	#28,d1			| yes, get into next page
					| operand case: 3,
					| instruction case: 4+12+12
					| XXX instr. case not done yet
	andl	#PG_FRAME,d1            | and truncate
Lberr3:
	movl	d1,sp@-			| push fault VA
	movl	d0,sp@-			| and FSLW
	andw	#0x1f80,d0 
	jeq	Lisberr
	jra	Lismerr
Lbenot060:
#endif
	moveq	#0,d0
	movw	a1@(12),d0		| grab SSW
	movl	a1@(20),d1		| and fault VA
	btst	#11,d0			| check for mis-aligned access
	jeq	Lberr2			| no, skip
	addl	#3,d1			| yes, get into next page
	andl	#PG_FRAME,d1		| and truncate
Lberr2:
	movl	d1,sp@-			| push fault VA
	movl	d0,sp@-			| and padded SSW
	btst	#10,d0			| ATC bit set?
	jeq	Lisberr			| no, must be a real bus error
	movc	dfc,d1			| yes, get MMU fault
	movc	d0,dfc			| store faulting function code
	movl	sp@(4),a0		| get faulting address
	.word	0xf568			| ptestr a0@
	movc	d1,dfc
	.long	0x4e7a0805		| movc mmusr,d0
	movw	d0,sp@			| save (ONLY LOW 16 BITS!)
	jra	Lismerr
#endif
_addrerr:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	lea	sp@(FR_HW),a1		| grab base of HW berr frame
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lbenot040		| no, skip
	movl	a1@(8),sp@-		| yes, push fault address
	clrl	sp@-			| no SSW for address fault
	jra	Lisaerr			| go deal with it
Lbenot040:
#endif
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
Lismerr:
	movl	#T_MMUFLT,sp@-		| show that we are an MMU fault
	jra	_ASM_LABEL(faultstkadj)	| and deal with it
Lisaerr:
	movl	#T_ADDRERR,sp@-		| mark address error
	jra	_ASM_LABEL(faultstkadj)	| and deal with it
Lisberr1:
	clrw	sp@			| re-clear pad word
Lisberr:
	movl	#T_BUSERR,sp@-		| mark bus error
	jra	_ASM_LABEL(faultstkadj)	| and deal with it

/*
 * FP exceptions.
 */
_fpfline:
#if defined(M68040)
	cmpw	#0x202c,sp@(6)		| format type 2?
	jne	_illinst		| no, not an FP emulation
#ifdef FPSP
	.globl fpsp_unimp
	jmp	fpsp_unimp		| yes, go handle it
#else
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULI,d0		| denote as FP emulation trap
	jra	fault			| do it
#endif
#else
	jra	_illinst
#endif

_fpunsupp:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	_illinst		| no, treat as illinst
#ifdef FPSP
	.globl	fpsp_unsupp
	jmp	fpsp_unsupp		| yes, go handle it
#else
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULD,d0		| denote as FP emulation trap
	jra	fault			| do it
#endif
#else
	jra	_illinst
#endif

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
#if defined(M68040) || defined(M68060)
	/* always null state frame on 68040, 68060 */
	cmpl	#FPU_68040,_fputype
	jle	Lfptnull
#endif
	tstb	a0@		| null state frame?
	jeq	Lfptnull	| yes, safe
	clrw	d0		| no, need to tweak BIU
	movb	a0@(1),d0	| get frame size
	bset	#3,a0@(0,d0:w)	| set exc_pend bit of BIU
Lfptnull:
	fmovem	fpsr,sp@-	| push fpsr as code argument
	frestore a0@		| restore state
	movl	#T_FPERR,sp@-	| push type arg
	jra	_ASM_LABEL(faultstkadj)	| call trap and deal with stack cleanup

/*
 * Other exceptions only cause four and six word stack frame and require
 * no post-trap stack adjustment.
 */

	.globl	_straytrap
_badtrap:
	moveml	#0xC0C0,sp@-		| save scratch regs
	movw	sp@(22),sp@-		| push exception vector info
	clrw	sp@-
	movl	sp@(22),sp@-		| and PC
	jbsr	_straytrap		| report
	addql	#8,sp			| pop args
	moveml	sp@+,#0x0303		| restore regs
	jra	rei			| all done

	.globl	_syscall
_trap0:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	d0,sp@-			| push syscall number
	jbsr	_syscall		| handle it
	addql	#4,sp			| pop syscall arg
	tstl	_astpending
	jne	Lrei2
	tstb	_ssir
	jeq	Ltrap1
	movw	#SPL1,sr
	tstb	_ssir
	jne	Lsir1
Ltrap1:	
	movl	sp@(FR_SP),a0		| grab and restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most registers
	addql	#8,sp			| pop SP and stack adjust
	rte

/*
 * Trap 12 is the entry point for the cachectl "syscall" (both HPUX & BSD)
 *	cachectl(command, addr, length)
 * command in d0, addr in a1, length in d1
 */
	.globl	_cachectl
ENTRY_NOPROFILE(trap12)
	movl	d1,sp@-			| push length
	movl	a1,sp@-			| push addr
	movl	d0,sp@-			| push command
	jbsr	_C_LABEL(cachectl)	| do it
	lea	sp@(12),sp		| pop args
	jra	_ASM_LABEL(rei)		| all done

/*
 * Trace (single-step) trap.  Kernel-mode is special.
 * User mode traps are simply passed on to trap().
 */
ENTRY_NOPROFILE(trace)
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-
	moveq	#T_TRACE,d0
	movw	sp@(FR_HW),d1		| get PSW
	andw	#PSL_S,d1		| from system mode?
	jne	Lkbrkpt			| yes, kernel breakpoint
	jra	fault			| no, user-mode fault

/*
 * Trap 15 is used for:
 *	- GDB breakpoints (in user programs)
 *	- KGDB breakpoints (in the kernel)
 *	- trace traps for SUN binaries (not fully supported yet)
 * User mode traps are simply passed to trap().
 */
ENTRY_NOPROFILE(trap15)
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-
	moveq	#T_TRAP15,d0
	movw	sp@(FR_HW),d1		| get PSW
	andw	#PSL_S,d1		| from system mode?
	jne	Lkbrkpt			| yes, kernel breakpoint
	jra	_ASM_LABEL(fault)	| no, user-mode fault

Lkbrkpt: | Kernel-mode breakpoint or trace trap. (d0=trap_type)
	| Save the system sp rather than the user sp.
	movw	#PSL_HIGHIPL,sr		| lock out interrupts
	lea	sp@(FR_SIZE),a6		| Save stack pointer
	movl	a6,sp@(FR_SP)		|  from before trap

	| If were are not on tmpstk switch to it.
	| (so debugger can change the stack pointer)
	movl	a6,d1
	cmpl	#tmpstk,d1
	jls	Lbrkpt2			| already on tmpstk
	| Copy frame to the temporary stack
	movl	sp,a0			| a0=src
	lea	tmpstk-96,a1		| a1=dst
	movl	a1,sp			| sp=new frame
	moveq	#FR_SIZE,d1
Lbrkpt1:
	movl	a0@+,a1@+
	subql	#4,d1
	jgt	Lbrkpt1

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
#ifdef KGDB
	| Let KGDB handle it (if connected)
	movl	a2,sp@-			| push frame ptr
	movl	d2,sp@-			| push trap type
	jbsr	_kgdb_trap		| handle the trap
	addql	#8,sp			| pop args
	cmpl	#0,d0			| did kgdb handle it?
	jne	Lbrkpt3			| yes, done
#endif
#ifdef DDB
	| Let DDB handle it
	movl	a2,sp@-			| push frame ptr
	movl	d2,sp@-			| push trap type
	jbsr	_kdb_trap		| handle the trap
	addql	#8,sp			| pop args
#if 0	/* not needed on hp300 */
	cmpl	#0,d0			| did ddb handle it?
	jne	Lbrkpt3			| yes, done
#endif
#endif
	/* Sun 3 drops into PROM here. */
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

/* Use common m68k sigreturn */
#include <m68k/m68k/sigreturn.s>

/*
 * Interrupt handlers. (auto vector.... not used)
 * original(amiga) routines:
 *	Level 0:	Spurious: ignored.
 *	Level 1:	builtin-RS232 TBE, softint (not used yet)
 *	Level 2:	keyboard (CIA-A) + DMA + SCSI
 *	Level 3:	VBL
 *	Level 4:	not used
 *	Level 5:	builtin-RS232 RBF
 *	Level 6:	Clock (CIA-B-Timers)
 *	Level 7:	Non-maskable: shouldn't be possible. ignore.
 */

/* Provide a generic interrupt dispatcher, only handle hardclock (int6)
 * specially, to improve performance
 */

#define INTERRUPT_SAVEREG	moveml	#0xC0C0,sp@-
#define INTERRUPT_RESTOREREG	moveml	sp@+,#0x0303

	/* Externs. */
	.globl	_intrhand, _hardclock

ENTRY_NOPROFILE(spurintr)	/* level 0 */
	rte				| XXX mfpcure (x680x0 hardware bug)

_kbdtimer:
	rte

_fdctrap:
#if NFD > 0
	INTERRUPT_SAVEREG
	jbsr	_C_LABEL(fdcintr)
	INTERRUPT_RESTOREREG
#endif
	addql	#1,_C_LABEL(intrcnt)+20
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei

#if 0
_fdcdmatrap:
#if NFD > 0
	INTERRUPT_SAVEREG
	jbsr	_C_LABEL(fdcdmaintr)
	INTERRUPT_RESTOREREG
#endif
	addql	#1,_C_LABEL(intrcnt)+20
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei


_fdcdmaerrtrap:
#if NFD > 0
	INTERRUPT_SAVEREG
	jbsr	_C_LABEL(fdcdmaerrintr)
	INTERRUPT_RESTOREREG
#endif
	addql	#1,_C_LABEL(intrcnt)+20
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei
#endif

_audiotrap:
#if 0
#if NADPCM > 0
	INTERRUPT_SAVEREG
	jbsr	_audiointr
	INTERRUPT_RESTOREREG
#endif
#endif
	addql	#1,_C_LABEL(intrcnt)+52
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei

_partrap:
#if NPAR > 0
	INTERRUPT_SAVEREG
	movel	#1,sp@-
	jbsr	_parintr
	addql	#4,sp
	INTERRUPT_RESTOREREG
#endif
	addql	#1,_C_LABEL(intrcnt)+56
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei

_audioerrtrap:
#if NADPCM > 0
	INTERRUPT_SAVEREG
	jbsr	_audioerrintr
	INTERRUPT_RESTOREREG
#endif
	addql	#1,_C_LABEL(intrcnt)+20
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei

_exspctrap:
	INTERRUPT_SAVEREG
#if NMHA > 0
	movel	#0,sp@-
	jbsr	_mhaintr		| handle interrupt
	addql	#4,sp
#endif
#if 0				/* XXX: NSPC > 1 */
	movel	#1,sp@-
	jbsr	_spcintr		| handle interrupt
	addql	#4,sp
#endif
	INTERRUPT_RESTOREREG
	addql	#1,_C_LABEL(intrcnt)+44
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei

_powtrap:
#include "pow.h"
#if NPOW > 0
	INTERRUPT_SAVEREG
	jbsr	_C_LABEL(powintr)
	INTERRUPT_RESTOREREG
#endif
	addql	#1,_C_LABEL(intrcnt)+60
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei

_com0trap:
#include "com.h"
#if NXCOM > 0
	INTERRUPT_SAVEREG
	movel	#0,sp@-
	jbsr	_comintr
	addql	#4,sp
	INTERRUPT_RESTOREREG
#endif
	addql	#1,_C_LABEL(intrcnt)+68
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei

_com1trap:
#if NXCOM > 1
	INTERRUPT_SAVEREG
	movel	#1,sp@-
	jbsr	_comintr
	addql	#4,sp
	INTERRUPT_RESTOREREG
#endif
	addql	#1,_C_LABEL(intrcnt)+68
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei

_intiotrap:
	INTERRUPT_SAVEREG
#if 0
	movw	#PSL_HIGHIPL,sr		| XXX
#endif
	pea	sp@(16-(FR_HW))		| XXX
	jbsr	_C_LABEL(intio_intr)
	addql	#4,sp
	INTERRUPT_RESTOREREG
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei

_lev1intr:
_lev2intr:
_lev3intr:
_lev4intr:
_lev5intr:
_lev6intr:
	INTERRUPT_SAVEREG
Lnotdma:
	lea	_C_LABEL(intrcnt),a0
	movw	sp@(22),d0		| use vector offset
	andw	#0xfff,d0		|   sans frame type
	addql	#1,a0@(-0x60,d0:w)	|     to increment apropos counter
	movw	sr,sp@-			| push current SR value
	clrw	sp@-			|    padded to longword
	jbsr	_intrhand		| handle interrupt
	addql	#4,sp			| pop SR
	INTERRUPT_RESTOREREG
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS
#else
	addql	#1,_C_LABEL(cnt)+V_INTR
#endif
	jra	rei

_timertrap:
	movw	#SPL4,sr		| XXX?
	moveml	#0xC0C0,sp@-		| save scratch registers
	addql	#1,_C_LABEL(intrcnt)+28	| count hardclock interrupts
	lea	sp@(16),a1		| a1 = &clockframe
	movl	a1,sp@-
	jbsr	_hardclock		| hardclock(&frame)
	addql	#4,sp
#include "ms.h"
#if NMS > 0
	jbsr	_ms_modem
#endif
#if defined(UVM)
	addql	#1,_C_LABEL(uvmexp)+UVMEXP_INTRS | chalk up another interrupt
#else
	addql	#1,_C_LABEL(cnt)+V_INTR	| chalk up another interrupt
#endif
	moveml	sp@+,#0x0303		| restore scratch registers
	jra	rei			| all done

_lev7intr:
	addql	#1,_C_LABEL(intrcnt)+36
	clrl	sp@-
	moveml	#0xFFFF,sp@-		| save registers
	movl	usp,a0			| and save
	movl	a0,sp@(FR_SP)		|   the user stack pointer
	jbsr	_nmihand		| call handler
	movl	sp@(FR_SP),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| and remaining registers
	addql	#8,sp			| pop SP and stack adjust
	jra	rei			| all done

/*
 * floppy ejection trap
 */

_fdeject:
	jra	rei

/*
 * Emulation of VAX REI instruction.
 *
 * This code deals with checking for and servicing ASTs
 * (profiling, scheduling) and software interrupts (network, softclock).
 * We check for ASTs first, just like the VAX.  To avoid excess overhead
 * the T_ASTFLT handling code will also check for software interrupts so we
 * do not have to do it here.  After identifing that we need an AST we
 * drop the IPL to allow device interrupts.
 *
 * This code is complicated by the fact that sendsig may have been called
 * necessitating a stack cleanup.
 */
	.comm	_ssir,1
	.globl	_astpending
	.globl	rei
rei:
	tstl	_astpending		| AST pending?
	jeq	Lchksir			| no, go check for SIR
Lrei1:
	btst	#5,sp@			| yes, are we returning to user mode?
	jne	Lchksir			| no, go check for SIR
	movw	#PSL_LOWIPL,sr		| lower SPL
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
Lrei2:
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
	rte				| and do real RTE
Lchksir:
	tstb	_ssir			| SIR pending?
	jeq	Ldorte			| no, all done
	movl	d0,sp@-			| need a scratch register
	movw	sp@(4),d0		| get SR
	andw	#PSL_IPL7,d0		| mask all but IPL
	jne	Lnosir			| came from interrupt, no can do
	movl	sp@+,d0			| restore scratch register
Lgotsir:
	movw	#SPL1,sr		| prevent others from servicing int
	tstb	_ssir			| too late?
	jeq	Ldorte			| yes, oh well...
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
Lsir1:	
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_SSIR,sp@-		| type == software interrupt
	jbsr	_trap			| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| and all remaining registers
	addql	#8,sp			| pop SP and stack adjust
	rte
Lnosir:
	movl	sp@+,d0			| restore scratch register
Ldorte:
	rte				| real return

/*
 * Macro to relocate a symbol, used before MMU is enabled.
 */
#define	RELOC(var, ar)	\
	lea	var,ar;	\
	addl	a5,ar

/*
 * Initialization
 *
 * A4 contains the address of the end of the symtab
 * A5 contains physical load point from boot
 * VBR contains zero from ROM.  Exceptions will continue to vector
 * through ROM until MMU is turned on at which time they will vector
 * through our table (vectors.s).
 */
BSS(lowram,4)
BSS(esym,4)

	.text
	.globl	_edata
	.globl	_etext,_end

ASENTRY_NOPROFILE(start)
	movw	#PSL_HIGHIPL,sr		| no interrupts

	addql	#4,sp
	movel	sp@+,a5			| firstpa
	movel	sp@+,d5			| fphysize -- last page
	movel	sp@,a4			| esym

	RELOC(_vectab, a0)		| set Vector Base Register temporaly
	movc	a0,vbr

#if 0	/* XXX this should be done by the boot loader */
	RELOC(_edata, a0)		| clear out BSS
	movl	#_end-4,d0		| (must be <= 256 kB)
	subl	#_edata,d0
	lsrl	#2,d0
1:	clrl	a0@+
	dbra	d0,1b
#endif

	RELOC(tmpstk, a0)
	movl	a0,sp			| give ourselves a temporary stack
	RELOC(_esym, a0)
#if 1
	movl	a4,a0@			| store end of symbol table
#else
	clrl	a0@			| no symbol table, yet
#endif
	RELOC(_lowram, a0)
	movl	a5,a0@			| store start of physical memory

	RELOC(_intr_reset, a0)
	jbsr	a0@			| XXX

	movl	#CACHE_OFF,d0
	movc	d0,cacr			| clear and disable on-chip cache(s)

/* determine our CPU/MMU combo - check for all regardless of kernel config */
	movl	#0x200,d0		| data freeze bit
	movc	d0,cacr			|   only exists on 68030
	movc	cacr,d0			| read it back
	tstl	d0			| zero?
	jeq	Lnot68030		| yes, we have 68020/68040/68060
	jra	Lstart1			| no, we have 68030
Lnot68030:
	bset	#31,d0			| data cache enable bit
	movc	d0,cacr			|   only exists on 68040/68060
	movc	cacr,d0			| read it back
	tstl	d0			| zero?
	jeq	Lis68020		| yes, we have 68020
	moveq	#0,d0			| now turn it back off
	movec	d0,cacr			|   before we access any data
	.word	0xf4d8			| cinva bc - invalidate caches XXX
	bset	#30,d0			| data cache no allocate mode bit
	movc	d0,cacr			|   only exists on 68060
	movc	cacr,d0			| read it back
	tstl	d0			| zero?
	jeq	Lis68040		| yes, we have 68040
	RELOC(_mmutype, a0)		| no, we have 68060
	movl	#MMU_68040,a0@		| with a 68040 compatible MMU 
	RELOC(_cputype, a0)
	movl	#CPU_68060,a0@		| and a 68060 CPU
	RELOC(_fputype, a0)
	movl	#FPU_68060,a0@		| and a 68060 FPU
	jra	Lstart1
Lis68040:
	RELOC(_mmutype, a0)
	movl	#MMU_68040,a0@		| with a 68040 MMU
	RELOC(_cputype, a0)
	movl	#CPU_68040,a0@		| and a 68040 CPU
	RELOC(_fputype, a0)
	movl	#FPU_68040,a0@		| and a 68040 FPU
	jra	Lstart1
Lis68020:
	RELOC(_mmutype, a0)
	movl	#MMU_68851,a0@		| we have PMMU
	RELOC(_cputype, a0)
	movl	#CPU_68020,a0@		| and a 68020 CPU
	RELOC(_fputype, a0)
	movl	#FPU_68881,a0@		| and a 68881 FPU

Lstart1:
/* initialize source/destination control registers for movs */
	moveq	#FC_USERD,d0		| user space
	movc	d0,sfc			|   as source
	movc	d0,dfc			|   and destination of transfers
/* initialize memory sizes (for pmap_bootstrap) */
	movl	d5,d1			| last page
	moveq	#PGSHIFT,d2
	lsrl	d2,d1			| convert to page (click) number
	RELOC(_maxmem, a0)
	movl	d1,a0@			| save as maxmem
	movl	a5,d0			| lowram value from ROM via boot
	lsrl	d2,d0			| convert to page number
	subl	d0,d1			| compute amount of RAM present
	RELOC(_physmem, a0)
	movl	d1,a0@			| and physmem
/* configure kernel and proc0 VA space so we can get going */
	.globl	_Sysseg, _pmap_bootstrap, _avail_start
#ifdef DDB
	RELOC(_esym,a0)			| end of static kernel test/data/syms
	movl	a0@,d5
	jne	Lstart2
#endif
	movl	#_end,d5		| end of static kernel text/data
Lstart2:
	addl	#NBPG-1,d5
	andl	#PG_FRAME,d5		| round to a page
	movl	d5,a4
	addl	a5,a4			| convert to PA
	pea	a5@			| firstpa
	pea	a4@			| nextpa
	RELOC(_pmap_bootstrap,a0)
	jbsr	a0@			| pmap_bootstrap(firstpa, nextpa)
	addql	#8,sp

/*
 * Prepare to enable MMU.
 * Since the kernel is not mapped logical == physical we must insure
 * that when the MMU is turned on, all prefetched addresses (including
 * the PC) are valid.  In order guarentee that, we use the last physical
 * page (which is conveniently mapped == VA) and load it up with enough
 * code to defeat the prefetch, then we execute the jump back to here.
 *
 * Is this all really necessary, or am I paranoid??
 */
	RELOC(_Sysseg, a0)		| system segment table addr
	movl	a0@,d1			| read value (a KVA)
	addl	a5,d1			| convert to PA
	RELOC(_mmutype, a0)
	cmpl	#MMU_68040,a0@		| 68040?
	jne	Lmotommu1		| no, skip
	.long	0x4e7b1807		| movc d1,srp
	jra	Lstploaddone
Lmotommu1:
	RELOC(_protorp, a0)
	movl	#0x80000202,a0@		| nolimit + share global + 4 byte PTEs
	movl	d1,a0@(4)		| + segtable address
	pmove	a0@,srp			| load the supervisor root pointer
	movl	#0x80000002,a0@		| reinit upper half for CRP loads
Lstploaddone:
	RELOC(_mmutype, a0)
	cmpl	#MMU_68040,a0@		| 68040?
	jne	Lmotommu2		| no, skip
#include "opt_jupiter.h"
#ifdef JUPITER
	/* JUPITER-X: set system register "SUPER" bit */
	movl	#0x0200a240,d0		| translate DRAM area transparently
	.long	0x4e7b0006		| movc d0,dtt0
	lea	0x00c00000,a0		| a0: graphic VRAM
	lea	0x02c00000,a1		| a1: graphic VRAM ( not JUPITER-X )
					|     DRAM ( JUPITER-X )
	movw	a0@,d0
	movw	d0,d1
	notw	d1
	movw	d1,a1@
	movw	d0,a0@
	cmpw	a1@,d1			| JUPITER-X?
	jne	Ljupiterdone		| no, skip
	movl	#0x0100a240,d0		| to access system register
	.long	0x4e7b0006		| movc d0,dtt0
	movb	#0x01,0x01800003@	| set "SUPER" bit	
Ljupiterdone:
#endif /* JUPITER */
	moveq	#0,d0			| ensure TT regs are disabled
	.long	0x4e7b0004		| movc d0,itt0
	.long	0x4e7b0005		| movc d0,itt1
	.long	0x4e7b0006		| movc d0,dtt0
	.long	0x4e7b0007		| movc d0,dtt1
	.word	0xf4d8			| cinva bc
	.word	0xf518			| pflusha
	movl	#0x8000,d0
	.long	0x4e7b0003		| movc d0,tc
#ifdef M68060
	RELOC(_cputype, a0)
	cmpl	#CPU_68060,a0@		| 68060?
	jne	Lnot060cache
	movl	#1,d0
	.long	0x4e7b0808		| movcl d0,pcr
	movl	#0xa0808000,d0
	movc	d0,cacr			| enable store buffer, both caches
	jmp	Lenab1
Lnot060cache:
#endif
	movl	#0x80008000,d0
	movc	d0,cacr			| turn on both caches
	jmp	Lenab1
Lmotommu2:
	movl	#0x82c0aa00,sp@-	| value to load TC with
	pmove	sp@,tc			| load it

/*
 * Should be running mapped from this point on
 */
Lenab1:
/* set vector base in virtual address */
	movl	#_C_LABEL(vectab),d0	| set Vector Base Register
	movc	d0,vbr
/* select the software page size now */
	lea	_ASM_LABEL(tmpstk),sp	| temporary stack
#if defined(UVM)
	jbsr	_C_LABEL(uvm_setpagesize)  | select software page size
#else
	jbsr	_C_LABEL(vm_set_page_size) | select software page size
#endif
/* set kernel stack, user SP, and initial pcb */
	movl	_C_LABEL(proc0paddr),a1	| get proc0 pcb addr
	lea	a1@(USPACE-4),sp	| set kernel stack to end of area
	lea	_C_LABEL(proc0),a2	| initialize proc0.p_addr so that
	movl	a1,a2@(P_ADDR)		|   we don't deref NULL in trap()
	movl	#USRSTACK-4,a2
	movl	a2,usp			| init user SP
	movl	a1,_C_LABEL(curpcb)	| proc0 is running

	tstl	_C_LABEL(fputype)	| Have an FPU?
	jeq	Lenab2			| No, skip.
	clrl	a1@(PCB_FPCTX)		| ensure null FP context
	movl	a1,sp@-
	jbsr	_C_LABEL(m68881_restore) | restore it (does not kill a1)
	addql	#4,sp
Lenab2:
/* flush TLB and turn on caches */
	jbsr	_TBIA			| invalidate TLB
	cmpl	#MMU_68040,_mmutype	| 68040?
	jeq	Lnocache0		| yes, cache already on
	movl	#CACHE_ON,d0
	movc	d0,cacr			| clear cache(s)
	jra	Lnocache0
Lnocache0:
/* final setup for C code */
	movl	d7,_boothowto		| save reboot flags
	movl	d6,_bootdev		|   and boot device

/*
 * Create a fake exception frame so that cpu_fork() can copy it.
 * main() nevers returns; we exit to user mode from a forked process
 * later on.
 */
	clrw	sp@-			| vector offset/frame type
	clrl	sp@-			| PC - filled in by "execve"
	movw	#PSL_USER,sp@-		| in user mode
	clrl	sp@-			| stack adjust count and padding
	lea	sp@(-64),sp		| construct space for D0-D7/A0-A7
	lea	_proc0,a0		| save pointer to frame
	movl	sp,a0@(P_MD_REGS)	|   in proc0.p_md.md_regs

	jra	_main			| main()

	pea	Lmainreturned		| Yow!  Main returned!
	jbsr	_panic
	/* NOTREACHED */
Lmainreturned:
	.asciz	"main() returned"
	.even

/*
 * proc_trampoline: call the function in register a2 with a3 as an arg
 * and then rei.
 */
	.globl	_proc_trampoline
_proc_trampoline:
	movl	a3,sp@-			| push function arg
	jbsr	a2@			| call function
	addql	#4,sp			| pop arg
	movl	sp@(FR_SP),a0		| grab and load
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most user regs
	addql	#8,sp			| toss SP and stack adjust
	jra	rei			| and return

/*
 * Use common m68k sigcode.
 */
#include <m68k/m68k/sigcode.s>

/*
 * Primitives
 */ 

/*
 * Use common m68k support routines.
 */
#include <m68k/m68k/support.s>

	.globl	_whichqs,_qs,_panic
	.globl	_curproc,_want_resched
#if defined(UVM)
	.globl	_uvmexp
#else
	.globl	_cnt
#endif

/*
 * Use common m68k process manipulation routines.
 */
#include <m68k/m68k/proc_subr.s>

Lsw0:
	.asciz	"switch"
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
 * At exit of a process, do a switch for the last time.
 * Switch to a safe stack and PCB, and select a new process to run.  The
 * old stack and u-area will be freed by the reaper.
 */
ENTRY(switch_exit)
	movl	sp@(4),a0
	movl	#nullpcb,_curpcb	| save state into garbage pcb
	lea	tmpstk,sp		| goto a tmp stack

	/* Schedule the vmspace and stack to be freed. */
	movl	a0,sp@-			| exit2(p)
	jbsr	_C_LABEL(exit2)
	lea	sp@(4),sp		| pop args

	jra	_C_LABEL(cpu_switch)

/*
 * When no processes are on the runq, Swtch branches to Idle
 * to wait for something to come ready.
 */
	.globl	Idle
Idle:
	stop	#PSL_LOWIPL
	movw	#PSL_HIGHIPL,sr
	movl	_whichqs,d0
	jeq	Idle
	jra	Lsw1

Lbadsw:
	movl	#Lsw0,sp@-
	jbsr	_panic
	/*NOTREACHED*/

/*
 * cpu_switch()
 *
 * NOTE: On the mc68851 (318/319/330) we attempt to avoid flushing the
 * entire ATC.  The effort involved in selective flushing may not be
 * worth it, maybe we should just flush the whole thing?
 *
 * NOTE 2: With the new VM layout we now no longer know if an inactive
 * user's PTEs have been changed (formerly denoted by the SPTECHG p_flag
 * bit).  For now, we just always flush the full ATC.
 */
ENTRY(cpu_switch)
	movl	_curpcb,a0		| current pcb
	movw	sr,a0@(PCB_PS)		| save sr before changing ipl
#ifdef notyet
	movl	_curproc,sp@-		| remember last proc running
#endif
	clrl	_curproc

	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */
	movw	#PSL_HIGHIPL,sr		| lock out interrupts
	movl	_whichqs,d0
	jeq	Idle
Lsw1:
	movl	d0,d1
	negl	d0
	andl	d1,d0
	bfffo	d0{#0:#32},d1
	eorib	#31,d1

	movl	d1,d0
	lslb	#3,d1			| convert queue number to index
	addl	#_qs,d1			| locate queue (q)
	movl	d1,a1
	movl	a1@(P_FORW),a0		| p = q->p_forw
	cmpal	d1,a0			| anyone on queue?
	jeq	Lbadsw			| no, panic
	movl	a0@(P_FORW),a1@(P_FORW)	| q->p_forw = p->p_forw
	movl	a0@(P_FORW),a1		| n = p->p_forw
	movl	d1,a1@(P_BACK)		| n->p_back = q
	cmpal	d1,a1			| anyone left on queue?
	jne	Lsw2			| yes, skip
	movl	_whichqs,d1
	bclr	d0,d1			| no, clear bit
	movl	d1,_whichqs
Lsw2:
	movl	a0,_curproc
	clrl	_want_resched
#ifdef notyet
	movl	sp@+,a1
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

	tstl	_fputype		| Do we have an FPU?
	jeq	Lswnofpsave		| No  Then don't attempt save.
	lea	a1@(PCB_FPCTX),a2	| pointer to FP save area
	fsave	a2@			| save FP state
#if defined(M68020) || defined(M68030) || defined(M68040)
#if defined(M68060)
	cmpl	#FPU_68060,_fputype
	jeq	Lsavfp60
#endif
	tstb	a2@			| null state frame?
	jeq	Lswnofpsave		| yes, all done
	fmovem	fp0-fp7,a2@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a2@(312)	| save FP control registers
#if defined(M68060)
	jra	Lswnofpsave
Lsavfp60:
#endif
#endif
#if defined(M68060)
	tstb	a2@(2)			| null state frame?
	jeq	Lswnofpsave		| yes, all done
	fmovem	fp0-fp7,a2@(216)	| save FP general registers
	fmovem	fpcr,a2@(312)		| save FP control registers
	fmovem	fpsr,a2@(316)
	fmovem	fpi,a2@(320)
#endif
Lswnofpsave:

#ifdef DIAGNOSTIC
	tstl	a0@(P_WCHAN)
	jne	Lbadsw
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lbadsw
#endif
	clrl	a0@(P_BACK)		| clear back link
	movb	a0@(P_MD_FLAGS+3),mdpflag | low byte of p_md.md_flags
	movl	a0@(P_ADDR),a1		| get p_addr
	movl	a1,_curpcb

	/*
	 * Activate the process's address space.
	 * XXX Should remember the last USTP value loaded, and call this
	 * XXX only if it has changed.
	 */
	pea	a0@			| push proc
	jbsr	_pmap_activate		| pmap_activate(p)
	addql	#4,sp
	movl	_curpcb,a1		| restore p_addr

	lea	tmpstk,sp		| now goto a tmp stack for NMI

	moveml	a1@(PCB_REGS),#0xFCFC	| and registers
	movl	a1@(PCB_USP),a0
	movl	a0,usp			| and USP

	tstl	_fputype		| If we don't have an FPU,
	jeq	Lnofprest		|  don't try to restore it.
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
#if defined(M68020) || defined(M68030) || defined(M68040)
#if defined(M68060)
	cmpl	#FPU_68060,_fputype
	jeq	Lresfp60rest1
#endif
	tstb	a0@			| null state frame?
	jeq	Lresfprest		| yes, easy
#if defined(M68040)
	cmpl	#FPU_68040,_fputype	| 68040?
	jne	Lresnot040		| no, skip
	clrl	sp@-			| yes...
	frestore sp@+			| ...magic!
Lresnot040:
#endif
	fmovem	a0@(312),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
#if defined(M68060)
	jra	Lresfprest
Lresfp60rest1:
#endif
#endif
#if defined(M68060)
	tstb	a0@(2)			| null state frame?
	jeq	Lresfprest		| yes, easy
	fmovem	a0@(312),fpcr		| restore FP control registers
	fmovem	a0@(316),fpsr
	fmovem	a0@(320),fpi
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
#endif
Lresfprest:
	frestore a0@			| restore state
Lnofprest:
	movw	a1@(PCB_PS),sr		| no, restore PS
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

	tstl	_fputype		| Do we have FPU?
	jeq	Lsvnofpsave		| No?  Then don't save state.
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	fsave	a0@			| save FP state
#if defined(M68020) || defined(M68030) || defined(M68040)
#if defined(M68060)
	cmpl	#FPU_68060,_fputype
	jeq	Lsvsavfp60
#endif
	tstb	a0@			| null state frame?
	jeq	Lsvnofpsave		| yes, all done
	fmovem	fp0-fp7,a0@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a0@(312)	| save FP control registers
#if defined(M68060)
	jra	Lsvnofpsave
Lsvsavfp60:
#endif
#endif
#if defined(M68060)
	tstb	a0@(2)			| null state frame?
	jeq	Lsvnofpsave		| yes, all done
	fmovem	fp0-fp7,a0@(216)	| save FP general registers
	fmovem	fpcr,a0@(312)		| save FP control registers
	fmovem	fpsr,a0@(316)
	fmovem	fpi,a0@(320)
#endif
Lsvnofpsave:
	moveq	#0,d0			| return 0
	rts

#if defined(M68040) || defined(M68060)
ENTRY(suline)
	movl	sp@(4),a0		| address to write
	movl	_curpcb,a1		| current pcb
	movl	#Lslerr,a1@(PCB_ONFAULT) | where to return to on a fault
	movl	sp@(8),a1		| address of line
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	moveq	#0,d0			| indicate no fault
	jra	Lsldone
Lslerr:
	moveq	#-1,d0
Lsldone:
	movl	_curpcb,a1		| current pcb
	clrl	a1@(PCB_ONFAULT) 	| clear fault address
	rts
#endif

/*
 * Invalidate entire TLB.
 */
ENTRY(TBIA)
__TBIA:
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu3		| no, skip
	.word	0xf518			| yes, pflusha
#ifdef M68060
	cmpl	#CPU_68060,_cputype
	jne	Ltbiano60
	movc	cacr,d0
	orl	#IC60_CABC,d0		| clear all branch cache entries
	movc	d0,cacr
Ltbiano60:
#endif
	rts
Lmotommu3:
#endif
	pflusha				| flush entire TLB
	tstl	_mmutype
	jpl	Lmc68851a		| 68851 implies no d-cache
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
Lmc68851a:
	rts

/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
ENTRY(TBIS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush entire TLB
#endif
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu4		| no, skip
	movl	sp@(4),a0
	movc	dfc,d1
	moveq	#FC_USERD,d0		| user space
	movc	d0,dfc
	.word	0xf508			| pflush a0@
	moveq	#FC_SUPERD,d0		| supervisor space
	movc	d0,dfc
	.word	0xf508			| pflush a0@
	movc	d1,dfc
#ifdef M68060
	cmpl	#CPU_68060,_cputype
	jne	Ltbisno60
	movc	cacr,d0
	orl	#IC60_CABC,d0		| clear all branch cache entries
	movc	d0,cacr
Ltbisno60:
#endif
	rts
Lmotommu4:
#endif
	movl	sp@(4),a0		| get addr to flush
	tstl	_mmutype
	jpl	Lmc68851b		| is 68851?
	pflush	#0,#0,a0@		| flush address from both sides
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip data cache
	rts
Lmc68851b:
	pflushs	#0,#0,a0@		| flush address from both sides
	rts

/*
 * Invalidate supervisor side of TLB
 */
ENTRY(TBIAS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu5		| no, skip
	.word	0xf518			| yes, pflusha (for now) XXX
#ifdef M68060
	cmpl	#CPU_68060,_cputype
	jne	Ltbiasno60
	movc	cacr,d0
	orl	#IC60_CABC,d0		| clear all branch cache entries
	movc	d0,cacr
Ltbiasno60:
#endif
	rts
Lmotommu5:
#endif
	tstl	_mmutype
	jpl	Lmc68851c		| 68851?
	pflush #4,#4			| flush supervisor TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts
Lmc68851c:
	pflushs #4,#4			| flush supervisor TLB entries
	rts

/*
 * Invalidate user side of TLB
 */
ENTRY(TBIAU)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu6		| no, skip
	.word	0xf518			| yes, pflusha (for now) XXX
#ifdef M68060
	cmpl	#CPU_68060,_cputype
	jne	Ltbiauno60
	movc	cacr,d0
	orl	#IC60_CUBC,d0		| clear user branch cache entries
	movc	d0,cacr
Ltbiauno60:
#endif
	rts
Lmotommu6:
#endif
	tstl	_mmutype
	jpl	Lmc68851d		| 68851?
	pflush	#0,#4			| flush user TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts
Lmc68851d:
	pflushs	#0,#4			| flush user TLB entries
	rts

/*
 * Invalidate instruction cache
 */
ENTRY(ICIA)
#if defined(M68040) || defined(M68060)
ENTRY(ICPA)
	cmpl	#MMU_68040,_mmutype	| 68040
	jne	Lmotommu7		| no, skip
	.word	0xf498			| cinva ic
	rts
Lmotommu7:
#endif
	movl	#IC_CLEAR,d0
	movc	d0,cacr			| invalidate i-cache
	rts

/*
 * Invalidate data cache.
 * HP external cache allows for invalidation of user/supervisor portions.
 * NOTE: we do not flush 68030 on-chip cache as there are no aliasing
 * problems with DC_WA.  The only cases we have to worry about are context
 * switch and TLB changes, both of which are handled "in-line" in resume
 * and TBI*.
 */
ENTRY(DCIA)
__DCIA:
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_mmutype	| 68040
	jne	Lmotommu8		| no, skip
	/* XXX implement */
Lmotommu8:
#endif
	rts

ENTRY(DCIS)
__DCIS:
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_mmutype	| 68040
	jne	Lmotommu9		| no, skip
	/* XXX implement */
Lmotommu9:
#endif
	rts

ENTRY(DCIU)
__DCIU:
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_mmutype	| 68040
	jne	LmotommuA		| no, skip
	/* XXX implement */
LmotommuA:
#endif
	rts

#if defined(M68040) || defined(M68060)
ENTRY(ICPL)
	movl	sp@(4),a0		| address
	.word	0xf488			| cinvl ic,a0@
	rts
ENTRY(ICPP)
	movl	sp@(4),a0		| address
	.word	0xf490			| cinvp ic,a0@
	rts
ENTRY(DCPL)
	movl	sp@(4),a0		| address
	.word	0xf448			| cinvl dc,a0@
	rts
ENTRY(DCPP)
	movl	sp@(4),a0		| address
	.word	0xf450			| cinvp dc,a0@
	rts
ENTRY(DCPA)
	.word	0xf458			| cinva dc
	rts
ENTRY(DCFL)
	movl	sp@(4),a0		| address
	.word	0xf468			| cpushl dc,a0@
	rts
ENTRY(DCFP)
	movl	sp@(4),a0		| address
	.word	0xf470			| cpushp dc,a0@
	rts
#endif

ENTRY(PCIA)
#if defined(M68040) || defined(M68060)
ENTRY(DCFA)
	cmpl	#MMU_68040,_mmutype	| 68040
	jne	LmotommuB		| no, skip
	.word	0xf478			| cpusha dc
	rts
LmotommuB:
#endif
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

ENTRY(ecacheon)
	rts

ENTRY(ecacheoff)
	rts

	.globl	_getsfc, _getdfc
_getsfc:
	movc	sfc,d0
	rts
_getdfc:
	movc	dfc,d0
	rts

/*
 * Load a new user segment table pointer.
 */
ENTRY(loadustp)
	movl	sp@(4),d0		| new USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	LmotommuC		| no, skip
	.word	0xf518			| pflusha
	.long	0x4e7b0806		| movc d0,urp
#ifdef M68060
	cmpl	#CPU_68060,_cputype
	jne	Lldno60
	movc	cacr,d0
	orl	#IC60_CUBC,d0		| clear user branch cache entries
	movc	d0,cacr
Lldno60:
#endif
	rts
LmotommuC:
#endif
	pflusha				| flush entire TLB
	lea	_protorp,a0		| CRP prototype
	movl	d0,a0@(4)		| stash USTP
	pmove	a0@,crp			| load root pointer
	movl	#CACHE_CLR,d0
	movc	d0,cacr			| invalidate cache(s)
	rts

ENTRY(ploadw)
#if defined(M68030)
	movl	sp@(4),a0		| address to load
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jeq	Lploadwskp		| yes, skip
#endif
	ploadw	#1,a0@			| pre-load translation
Lploadwskp:
#endif
	rts

/*
 * Set processor priority level calls.  Most are implemented with
 * inline asm expansions.  However, spl0 requires special handling
 * as we need to check for our emulated software interrupts.
 */

ENTRY(spl0)
	moveq	#0,d0
	movw	sr,d0			| get old SR for return
	movw	#PSL_LOWIPL,sr		| restore new SR
	tstb	_ssir			| software interrupt pending?
	jeq	Lspldone		| no, all done
	subql	#4,sp			| make room for RTE frame
	movl	sp@(4),sp@(2)		| position return address
	clrw	sp@(6)			| set frame type 0
	movw	#PSL_LOWIPL,sp@		| and new SR
	jra	Lgotsir			| go handle it
Lspldone:
	rts

/*
 * Save and restore 68881 state.
 */
ENTRY(m68881_save)
	movl	sp@(4),a0		| save area pointer
	fsave	a0@			| save state
#if defined(M68020) || defined(M68030) || defined(M68040)
#if defined(M68060)
	cmpl	#FPU_68060,_fputype
	jeq	Lm68060fpsave
#endif
Lm68881fpsave:
	tstb	a0@			| null state frame?
	jeq	Lm68881sdone		| yes, all done
	fmovem	fp0-fp7,a0@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a0@(312)	| save FP control registers
Lm68881sdone:
	rts
#endif
#if defined(M68060)
Lm68060fpsave:
	tstb	a0@(2)			| null state frame?
	jeq	Lm68060sdone		| yes, all done
	fmovem	fp0-fp7,a0@(216)	| save FP general registers
	fmovem	fpcr,a0@(312)		| save FP control registers
	fmovem	fpsr,a0@(316)
	fmovem	fpi,a0@(320)
Lm68060sdone:
	rts
#endif

ENTRY(m68881_restore)
	movl	sp@(4),a0		| save area pointer
#if defined(M68020) || defined(M68030) || defined(M68040)
#if defined(M68060)
	cmpl	#FPU_68060,_fputype
	jeq	Lm68060fprestore
#endif
Lm68881fprestore:
	tstb	a0@			| null state frame?
	jeq	Lm68881rdone		| yes, easy
	fmovem	a0@(312),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
Lm68881rdone:
	frestore a0@			| restore state
	rts
#endif
#if defined(M68060)
Lm68060fprestore:
	tstb	a0@(2)			| null state frame?
	jeq	Lm68060fprdone		| yes, easy
	fmovem	a0@(312),fpcr		| restore FP control registers
	fmovem	a0@(316),fpsr
	fmovem	a0@(320),fpi
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
Lm68060fprdone:
	frestore a0@			| restore state
	rts
#endif

/*
 * Handle the nitty-gritty of rebooting the machine.
 * Basically we just turn off the MMU and jump to the appropriate ROM routine.
 * Note that we must be running in an address range that is mapped one-to-one
 * logical to physical so that the PC is still valid immediately after the MMU
 * is turned off.  We have conveniently mapped the last page of physical
 * memory this way.
 */
	.globl	_doboot
_doboot:
	movw	#PSL_HIGHIPL,sr		| cut off any interrupts
	subal	a1,a1			| a1 = 0
	moveq	#0,d1			| d1 = 0
	movl	_mmutype,d2		| d2 = 0

	movl	#CACHE_OFF,d0
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,d2		| 68040?
	jne	Ldoboot0		| no, skip
	.word	0xf4f8			| cpusha bc - push and invalidate caches
	nop
	movl	#CACHE40_OFF,d0
Ldoboot0:
#endif
	movc	d0,cacr			| disable on-chip cache(s)

	| ok, turn off MMU..
Ldoreboot:
#if defined(M68040) || defined(M68060)
	cmpl	#MMU_68040,d2		| 68040?
	jne	LmotommuF		| no, skip
	movc	d1,cacr			| caches off
	.long	0x4e7b1003		| movc d1(=0),tc ; disable MMU
	jra	Ldoreboot1
LmotommuF:
#endif
	movl	d1,sp@
	pmove	sp@,tc			| disable MMU
Ldoreboot1:
	moveml	0x00ff0000,#0x0101	| get RESET vectors in ROM
					|	(d0: ssp, a0: pc)
	moveml	#0x0101,a1@		| put them at 0x0000 (for Xellent30)
	movc	a1,vbr			| reset Vector Base Register
	jmp	a0@			| reboot X680x0
Lebootcode:

	.data
	.globl	_machineid
_machineid:
	.long	0		| default to X68030
	.globl	_mmutype,_cputype,_fputype,_protorp
_mmutype:
	.long	MMU_68030	| default to 030 internal MMU
_cputype:
	.long	CPU_68030	| default to 68030 CPU
_fputype:
	.long	FPU_NONE
#ifdef M68K_MMU_HP
	.globl	_ectype
_ectype:
	.long	EC_NONE		| external cache type, default to none
#endif
_protorp:
	.long	0,0		| prototype root pointer
	.globl	_cold
_cold:
	.long	1		| cold start flag
	.globl	_want_resched
_want_resched:
	.long	0
	.globl	_intiolimit,_extiobase
	.globl	_proc0paddr
_proc0paddr:
	.long	0		| KVA of proc0 u-area
_intiolimit:
	.long	0		| KVA of end of internal IO space
_extiobase:
	.long	0		| KVA of base of external IO space
#ifdef DEBUG
	.globl	fulltflush, fullcflush
fulltflush:
	.long	0
fullcflush:
	.long	0
#endif
/* interrupt counters */
	.globl	_intrcnt,_eintrcnt,_intrnames,_eintrnames
_intrnames:
	.asciz	"spur"
	.asciz	"lev1"
	.asciz	"lev2"
	.asciz	"lev3"
	.asciz	"lev4"
	.asciz	"lev5"
	.asciz	"lev6"
	.asciz	"clock"
	.asciz	"statclock"
	.asciz	"nmi"
	.asciz	"kbd"
	.asciz	"scsi"
	.asciz	"zs"
	.asciz	"audio"
	.asciz	"ppi"
	.asciz	"pow"
	.asciz	"ed"
	.asciz	"com"
_eintrnames:
	.even
_intrcnt:
	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
_eintrcnt:
