/*
 * Copyright (c) 1996, 1997, 1998 Eduardo Horvath
 * Copyright (c) 1996 Paul Kranenburg
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
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
 *	This product includes software developed by Harvard University.
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
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by Paul Kranenburg.
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
 *	@(#)locore.s	8.4 (Berkeley) 12/10/93
 */

#undef NO_VCACHE
#define TRAPTRACE
#define TRAPSTATS
#undef TRAPS_USE_IG
#undef LOCKED_PCB
#define HWREF

#include "opt_ddb.h"
#include "opt_uvm.h"
#include "opt_compat_svr4.h"

#include "assym.h"
#include <machine/param.h>
#include <sparc64/sparc64/intreg.h>
#include <sparc64/sparc64/timerreg.h>
#include <sparc64/sparc64/vaddrs.h>
#ifdef notyet
#include <sparc64/dev/zsreg.h>
#endif
#include <machine/ctlreg.h>
#include <machine/psl.h>
#include <machine/signal.h>
#include <machine/trap.h>
#include <machine/frame.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#ifdef COMPAT_SVR4
#include <compat/svr4/svr4_syscall.h>
#endif
#include <machine/asm.h>

/* Let us use same syntax as C code */
#define Debugger()	ta	1; nop

#if 0
/*
 * Try to issue an elf note to align the kernel properly.
 */
	.section	.note
	.word	1f-0f
	.word	4		! Dunno why
	.word	1
0:	.asciz	"SUNW Solaris"
1:	
	.align	4
	.word	0x0400000
#endif
/*
 * GNU assembler does not understand `.empty' directive; Sun assembler
 * gripes about labels without it.  To allow cross-compilation using
 * the Sun assembler, and because .empty directives are useful documentation,
 * we use this trick.
 */
#ifdef SUN_AS
#define	EMPTY	.empty
#else
#define	EMPTY	/* .empty */
#endif

/* use as needed to align things on longword boundaries */
#define	_ALIGN	.align 4
#define ICACHE_ALIGN	.align	32

/* Give this real authority: reset the machine */
#if 1
#define NOTREACHED	sir
#else	
#define NOTREACHED
#endif

/*
 * A handy macro for maintaining instrumentation counters.
 * Note that this clobbers %o0 and %o1.  Normal usage is
 * something like:
 *	foointr:
 *		TRAP_SETUP(...)		! makes %o registers safe
 *		INCR(_C_LABEL(cnt)+V_FOO)	! count a foo
 */
#define INCR(what) \
	sethi	%hi(what), %o0; \
	ldsw	[%o0 + %lo(what)], %o1; \
	inc	%o1; \
	stw	%o1, [%o0 + %lo(what)]

/*
 * A couple of handy macros to save and restore globals to/from
 * locals.  Since udivrem uses several globals, and it's called
 * from vsprintf, we need to do this before and after doing a printf.
 */
#define GLOBTOLOC \
	mov	%g1, %l1; \
	mov	%g2, %l2; \
	mov	%g3, %l3; \
	mov	%g4, %l4; \
	mov	%g5, %l5; \
	mov	%g6, %l6; \
	mov	%g7, %l7

#define LOCTOGLOB \
	mov	%l1, %g1; \
	mov	%l2, %g2; \
	mov	%l3, %g3; \
	mov	%l4, %g4; \
	mov	%l5, %g5; \
	mov	%l6, %g6; \
	mov	%l7, %g7

/*
 * Handy stack conversion macros.
 * They correctly switch to requested stack type
 * regardless of the current stack.
 */

#define TO_STACK64(size) \
	andcc	%sp, 1, %g0; /* 64-bit stack? */ \
	save	%sp, size, %sp; \
	beq,a	9f; \
	 add	%sp, -BIAS, %sp; /* Convert to 64-bits */ \
9:	

#define TO_STACK32(size) \
	andcc	%sp, 1, %g0; /* 64-bit stack? */ \
	save	%sp, size, %sp; \
	bne,a	9f; \
	 add	%sp, +BIAS, %sp; /* Convert to 32-bits */ \
9:	

	.data
	
/*
 * The interrupt stack.
 *
 * This is the very first thing in the data segment, and therefore has
 * the lowest kernel stack address.  We count on this in the interrupt
 * trap-frame setup code, since we may need to switch from the kernel
 * stack to the interrupt stack (iff we are not already on the interrupt
 * stack).  One sethi+cmp is all we need since this is so carefully
 * arranged.
 */
	.globl	_C_LABEL(intstack)
	.globl	_C_LABEL(eintstack)
_C_LABEL(intstack):
	.space	(2*USPACE)
_C_LABEL(eintstack):

/*
 * When a process exits and its u. area goes away, we set cpcb to point
 * to this `u.', leaving us with something to use for an interrupt stack,
 * and letting all the register save code have a pcb_uw to examine.
 * This is also carefully arranged (to come just before u0, so that
 * process 0's kernel stack can quietly overrun into it during bootup, if
 * we feel like doing that).
 */
	.globl	_C_LABEL(idle_u)
_C_LABEL(idle_u):
	.space	USPACE

/*
 * Process 0's u.
 *
 * This must be aligned on an 8 byte boundary.
 */
	.globl	_C_LABEL(u0)
_C_LABEL(u0):	.space	(2*USPACE)
estack0:

#ifdef KGDB
/*
 * Another item that must be aligned, easiest to put it here.
 */
KGDB_STACK_SIZE = 2048
	.globl	_C_LABEL(kgdb_stack)
_C_LABEL(kgdb_stack):
	.space	KGDB_STACK_SIZE		! hope this is enough
#endif

#ifdef DEBUG
/*
 * This is an emergency stack used when we overflow the normal kernel stack.
 */
	.space	USPACE
	.align	16
panicstack:
#endif

/*
 * _cpcbtte is the TTE for cpcb which allows the data fault code to not fault
 * before it's in a safe condidion.
 */
	.globl	_C_LABEL(cpcbtte)
_C_LABEL(cpcbtte):	.xword	0
	
/*
 * _cpcb points to the current pcb (and hence u. area).
 * Initially this is the special one.
 */
	.globl	_C_LABEL(cpcb)
_C_LABEL(cpcb):	.word	_C_LABEL(u0)

/*
 * _cputyp is the current cpu type, used to distinguish between
 * the many variations of different sun4* machines. It contains
 * the value CPU_SUN4, CPU_SUN4C, or CPU_SUN4M.
 */
	.globl	_C_LABEL(cputyp)
_C_LABEL(cputyp):
	.word	1
/*
 * _cpumod is the current cpu model, used to distinguish between variants
 * in the Sun4 and Sun4M families. See /sys/arch/sparc64/include/param.h for
 * possible values.
 */
	.globl	_C_LABEL(cpumod)
_C_LABEL(cpumod):
	.word	1
/*
 * _mmumod is the current mmu model, used to distinguish between the
 * various implementations of the SRMMU in the sun4m family of machines.
 * See /sys/arch/sparc64/include/param.h for possible values.
 */
	.globl	_C_LABEL(mmumod)
_C_LABEL(mmumod):
	.word	0


/*
 * There variables are pointed to by the cpp symbols PGSHIFT, NBPG,
 * and PGOFSET.
 */
	.globl	_C_LABEL(pgshift), _C_LABEL(nbpg), _C_LABEL(pgofset)
_C_LABEL(pgshift):
	.word	0
_C_LABEL(nbpg):
	.word	0
_C_LABEL(pgofset):
	.word	0

	_ALIGN

	.text

/*
 * The first thing in the real text segment is the trap vector table,
 * which must be aligned on a 4096 byte boundary.  The text segment
 * starts beyond page 0 of KERNBASE so that there is a red zone
 * between user and kernel space.  Since the boot ROM loads us at
 * 0x4000, it is far easier to start at KERNBASE+0x4000 than to
 * buck the trend.  This is two or four pages in (depending on if
 * pagesize is 8192 or 4096).    We place two items in this area:
 * the message buffer (phys addr 0) and the IE_reg (phys addr 0x2000).
 * because the message buffer is in our "red zone" between user and
 * kernel space we remap it in configure() to another location and
 * invalidate the mapping at KERNBASE.
 */
	.globl _C_LABEL(msgbuf)
_C_LABEL(msgbuf) = KERNBASE

/*
 * The v9 trap frame is stored in the special trap registers.  The
 * register window is only modified on window overflow, underflow,
 * and clean window traps, where it points to the register window
 * needing service.  Traps have space for 8 instructions, except for
 * the window overflow, underflow, and clean window traps which are
 * 32 instructions long, large enough to in-line.
 *
 * The spitfire CPU (Ultra I) has 4 different sets of global registers. (blah blah...)
 *
 * I used to generate these numbers by address arithmetic, but gas's
 * expression evaluator has about as much sense as your average slug
 * (oddly enough, the code looks about as slimy too).  Thus, all the
 * trap numbers are given as arguments to the trap macros.  This means
 * there is one line per trap.  Sigh.
 *
 * Hardware interrupt vectors can be `linked'---the linkage is to regular
 * C code---or rewired to fast in-window handlers.  The latter are good
 * for unbuffered hardware like the Zilog serial chip and the AMD audio
 * chip, where many interrupts can be handled trivially with pseudo-DMA or
 * similar.  Only one `fast' interrupt can be used per level, however, and
 * direct and `fast' interrupts are incompatible.  Routines in intr.c
 * handle setting these, with optional paranoia.
 */

/*
 *	TA8 -- trap align for 8 instruction traps
 *	TA32 -- trap align for 32 instruction traps
 */
#define TA8	.align 32
#define TA32	.align 128

/*
 * v9 trap macros:
 *
 *	We have a problem with v9 traps; we have no registers to put the
 *	trap type into.  But we do have a %tt register which already has
 *	that information.  Tryap types in these macros are all dummys.
 */
	/* regular vectored traps */
#ifdef DEBUG
#ifdef TRAPTRACE
#define TRACEME		sethi %hi(1f), %g1; ba,pt %icc,traceit; or %g1, %lo(1f), %g1; 1:
#if 0
#define TRACEWIN	sethi %hi(9f), %l6; ba,pt %icc,traceitwin; or %l6, %lo(9f), %l6; 9:
#endif
#ifdef TRAPS_USE_IG
#define TRACEWIN	wrpr %g0, PSTATE_KERN|PSTATE_AG, %pstate; sethi %hi(9f), %g1; ba,pt %icc,traceit; or %g1, %lo(9f), %g1; 9:
#else
#define TRACEWIN	wrpr %g0, PSTATE_KERN|PSTATE_IG, %pstate; sethi %hi(9f), %g1; ba,pt %icc,traceit; or %g1, %lo(9f), %g1; 9:
#endif	
#define TRACERELOAD32	ba reload32; nop;
#define TRACERELOAD64	ba reload64; nop;
#define TRACEFLT	TRACEME
#define	VTRAP(type, label) \
	sethi %hi(label), %g1; ba,pt %icc,traceit; or %g1, %lo(label), %g1; NOTREACHED; TA8
#else
#define TRACEME
#define TRACEWIN	TRACEME
#define TRACERELOAD32
#define TRACERELOAD64
#define TRACEFLT	TRACEME
#define	VTRAP(type, label) \
	set KERNBASE+0x28, %g1; rdpr %tt, %g2; b label; stx %g2, [%g1]; NOTREACHED; TA8
#endif
#else
#define TRACEME
#define TRACEWIN	TRACEME
#define TRACERELOAD32
#define TRACERELOAD64
#define TRACEFLT	TRACEME
#define	VTRAP(type, label) \
	ba,a,pt	%icc,label; nop; NOTREACHED; TA8
#endif

	/* hardware interrupts (can be linked or made `fast') */
#define	HARDINT4U(lev) \
	VTRAP(lev, _C_LABEL(sparc_interrupt))

	/* software interrupts (may not be made direct, sorry---but you
	   should not be using them trivially anyway) */
#define	SOFTINT4U(lev, bit) \
	HARDINT4U(lev) 

	/* traps that just call trap() */
#define	TRAP(type)	VTRAP(type, slowtrap)

	/* architecturally undefined traps (cause panic) */
#ifndef DEBUG
#define	UTRAP(type)	sir; VTRAP(type, slowtrap)
#else
#define	UTRAP(type)	VTRAP(type, slowtrap)
#endif
	
	/* software undefined traps (may be replaced) */
#define	STRAP(type)	VTRAP(type, slowtrap)

/* breakpoint acts differently under kgdb */
#ifdef KGDB
#define	BPT		VTRAP(T_BREAKPOINT, bpt)
#define	BPT_KGDB_EXEC	VTRAP(T_KGDB_EXEC, bpt)
#else
#define	BPT		TRAP(T_BREAKPOINT)
#define	BPT_KGDB_EXEC	TRAP(T_KGDB_EXEC)
#endif

/* special high-speed 1-instruction-shaved-off traps (get nothing in %l3) */
#define	SYSCALL		VTRAP(0x100, syscall_setup)
#ifdef notyet
#define	ZS_INTERRUPT	b zshard; nop; TA8
#else
#define	ZS_INTERRUPT4U	HARDINT4U(12)
#endif


/*
 * Macro to clear %tt so we don't get confused with old traps.
 */
#ifdef DEBUG
#define CLRTT	wrpr	%g0,0x1ff,%tt
#else
#define CLRTT
#endif
/*
 * Here are some oft repeated traps as macros.
 */

	/* spill a 64-bit register window */
#define SPILL64(label,as) \
	TRACEWIN; \
label:	\
	wr	%g0, as, %asi; \
	stxa	%l0, [%sp+BIAS+0x00]%asi; \
	stxa	%l1, [%sp+BIAS+0x08]%asi; \
	stxa	%l2, [%sp+BIAS+0x10]%asi; \
	stxa	%l3, [%sp+BIAS+0x18]%asi; \
	stxa	%l4, [%sp+BIAS+0x20]%asi; \
	stxa	%l5, [%sp+BIAS+0x28]%asi; \
	stxa	%l6, [%sp+BIAS+0x30]%asi; \
	\
	stxa	%l7, [%sp+BIAS+0x38]%asi; \
	stxa	%i0, [%sp+BIAS+0x40]%asi; \
	stxa	%i1, [%sp+BIAS+0x48]%asi; \
	stxa	%i2, [%sp+BIAS+0x50]%asi; \
	stxa	%i3, [%sp+BIAS+0x58]%asi; \
	stxa	%i4, [%sp+BIAS+0x60]%asi; \
	stxa	%i5, [%sp+BIAS+0x68]%asi; \
	stxa	%i6, [%sp+BIAS+0x70]%asi; \
	\
	stxa	%i7, [%sp+BIAS+0x78]%asi; \
	saved; \
	CLRTT; \
	retry; \
	NOTREACHED; \
	TA32		

	/* spill a 32-bit register window */
#define SPILL32(label,as) \
	TRACEWIN; \
label:	\
	wr	%g0, as, %asi; \
	srl	%sp, 0, %sp; /* fixup 32-bit pointers */ \
	stwa	%l0, [%sp+0x00]%asi; \
	stwa	%l1, [%sp+0x04]%asi; \
	stwa	%l2, [%sp+0x08]%asi; \
	stwa	%l3, [%sp+0x0c]%asi; \
	stwa	%l4, [%sp+0x10]%asi; \
	stwa	%l5, [%sp+0x14]%asi; \
	\
	stwa	%l6, [%sp+0x18]%asi; \
	stwa	%l7, [%sp+0x1c]%asi; \
	stwa	%i0, [%sp+0x20]%asi; \
	stwa	%i1, [%sp+0x24]%asi; \
	stwa	%i2, [%sp+0x28]%asi; \
	stwa	%i3, [%sp+0x2c]%asi; \
	stwa	%i4, [%sp+0x30]%asi; \
	stwa	%i5, [%sp+0x34]%asi; \
	\
	stwa	%i6, [%sp+0x38]%asi; \
	stwa	%i7, [%sp+0x3c]%asi; \
	saved; \
	CLRTT; \
	retry; \
	NOTREACHED; \
	TA32	

	/* Spill either 32-bit or 64-bit register window. */
#define SPILLBOTH(label64,label32,as) \
	TRACEWIN; \
	andcc	%sp, 1, %g0; \
	bnz,pt	%xcc, label64+4;	/* See if it's a v9 stack or v8 */ \
	 wr	%g0, as, %asi; \
	ba,pt	%xcc, label32+8; \
	 srl	%sp, 0, %sp; /* fixup 32-bit pointers */ \
	NOTREACHED; \
	TA32

	/* fill a 64-bit register window */
#define FILL64(label,as) \
	TRACEWIN; \
label: \
	wr	%g0, as, %asi; \
	ldxa	[%sp+BIAS+0x00]%asi, %l0; \
	ldxa	[%sp+BIAS+0x08]%asi, %l1; \
	ldxa	[%sp+BIAS+0x10]%asi, %l2; \
	ldxa	[%sp+BIAS+0x18]%asi, %l3; \
	ldxa	[%sp+BIAS+0x20]%asi, %l4; \
	ldxa	[%sp+BIAS+0x28]%asi, %l5; \
	ldxa	[%sp+BIAS+0x30]%asi, %l6; \
	\
	ldxa	[%sp+BIAS+0x38]%asi, %l7; \
	ldxa	[%sp+BIAS+0x40]%asi, %i0; \
	ldxa	[%sp+BIAS+0x48]%asi, %i1; \
	ldxa	[%sp+BIAS+0x50]%asi, %i2; \
	ldxa	[%sp+BIAS+0x58]%asi, %i3; \
	ldxa	[%sp+BIAS+0x60]%asi, %i4; \
	ldxa	[%sp+BIAS+0x68]%asi, %i5; \
	ldxa	[%sp+BIAS+0x70]%asi, %i6; \
	\
	ldxa	[%sp+BIAS+0x78]%asi, %i7; \
	restored; \
	CLRTT; \
	retry; \
	NOTREACHED; \
	TA32

	/* fill a 32-bit register window */
#define FILL32(label,as) \
	TRACEWIN; \
label:	\
	wr	%g0, as, %asi; \
	srl	%sp, 0, %sp; /* fixup 32-bit pointers */ \
	lda	[%sp+0x00]%asi, %l0; \
	lda	[%sp+0x04]%asi, %l1; \
	lda	[%sp+0x08]%asi, %l2; \
	lda	[%sp+0x0c]%asi, %l3; \
	lda	[%sp+0x10]%asi, %l4; \
	lda	[%sp+0x14]%asi, %l5; \
	\
	lda	[%sp+0x18]%asi, %l6; \
	lda	[%sp+0x1c]%asi, %l7; \
	lda	[%sp+0x20]%asi, %i0; \
	lda	[%sp+0x24]%asi, %i1; \
	lda	[%sp+0x28]%asi, %i2; \
	lda	[%sp+0x2c]%asi, %i3; \
	lda	[%sp+0x30]%asi, %i4; \
	lda	[%sp+0x34]%asi, %i5; \
	\
	lda	[%sp+0x38]%asi, %i6; \
	lda	[%sp+0x3c]%asi, %i7; \
	restored; \
	CLRTT; \
	retry; \
	NOTREACHED; \
	TA32
	
	/* Spill either 32-bit or 64-bit register window. */
#define FILLBOTH(label64,label32,as) \
	TRACEWIN; \
	andcc	%sp, 1, %i0; \
	bnz	(label64)+4; /* See if it's a v9 stack or v8 */ \
	 wr	%g0, as, %asi; \
	ba	(label32)+8; \
	 srl	%sp, 0, %sp; /* fixup 32-bit pointers */ \
	NOTREACHED; \
	TA32

	.globl	start, _C_LABEL(kernel_text)
	_C_LABEL(kernel_text) = start		! for kvm_mkdb(8)
start:
/*
 * Put sun4u traptable first, since it needs the most stringent aligment (32K)
 */
	/* Traps from TL=0 -- traps from user mode */
#define TABLE	user_
	.globl	_C_LABEL(trapbase)
_C_LABEL(trapbase):
	b dostart; nop; TA8	! 000 = reserved -- Use it to boot
	/* We should not get the next 5 traps */
	UTRAP(0x001)		! 001 = POR Reset -- ROM should get this
	UTRAP(0x002)		! 002 = WDR Watchdog -- ROM should get this
	UTRAP(0x003)		! 003 = XIR -- ROM should get this
	UTRAP(0x004)		! 004 = SIR -- ROM should get this
	UTRAP(0x005)		! 005 = RED state exception
	UTRAP(0x006); UTRAP(0x007)
	VTRAP(T_INST_EXCEPT, textfault)	! 008 = instr. access exept
	VTRAP(T_TEXTFAULT, textfault)	! 009 = instr access MMU miss -- no MMU
	VTRAP(T_INST_ERROR, textfault)	! 00a = instr. access err
	UTRAP(0x00b); UTRAP(0x00c); UTRAP(0x00d); UTRAP(0x00e); UTRAP(0x00f)
	TRAP(T_ILLINST)			! 010 = illegal instruction
	TRAP(T_PRIVINST)		! 011 = privileged instruction
	UTRAP(0x012)			! 012 = unimplemented LDD
	UTRAP(0x013)			! 013 = unimplemented STD
	UTRAP(0x014); UTRAP(0x015); UTRAP(0x016); UTRAP(0x017); UTRAP(0x018)
	UTRAP(0x019); UTRAP(0x01a); UTRAP(0x01b); UTRAP(0x01c); UTRAP(0x01d)
	UTRAP(0x01e); UTRAP(0x01f)
	TRAP(T_FPDISABLED)		! 020 = fp instr, but EF bit off in psr
	VTRAP(T_FP_IEEE_754, fp_exception)		! 021 = ieee 754 exception
	VTRAP(T_FP_OTHER, fp_exception)		! 022 = other fp exception
	TRAP(T_TAGOF)			! 023 = tag overflow
	TRACEWIN			! DEBUG
	clr	%l0
!	set	0xbadcafe, %l0		! DEBUG
	mov %l0,%l1; mov %l0,%l2	! 024-027 = clean window trap
	rdpr %cleanwin, %o7		!	Clear out %l0-%l8 and %o0-%o8 and inc %cleanwin and done	
	inc %o7; mov %l0,%l3; mov %l0,%l4	!	This handler is in-lined and cannot fault
	wrpr %g0, %o7, %cleanwin	!       Nucleus (trap&IRQ) code does not need clean windows
	
	mov %l0,%l5
#ifdef NOT_DEBUG
	set	u0, %o7			! Check for kernel stack overflow
	cmp	%fp, %o7
	bl	trap_enter
#endif
	mov %l0, %l6; mov %l0, %l7; mov %l0, %o0; mov %l0, %o1
	
	mov %l0, %o2; mov %l0, %o3; mov %l0, %o4; mov %l0, %o5; 
	mov %l0, %o6; mov %l0, %o7
	CLRTT
	retry; nop; NOTREACHED; TA32
	TRAP(T_DIV0)			! 028 = divide by zero
	UTRAP(0x029)			! 029 = internal processor error
	UTRAP(0x02a); UTRAP(0x02b); UTRAP(0x02c); UTRAP(0x02d); UTRAP(0x02e); UTRAP(0x02f)
	VTRAP(T_DATAFAULT, winfault)	! 030 = data fetch fault
	UTRAP(0x031)			! 031 = data MMU miss -- no MMU
	VTRAP(T_DATA_ERROR, winfault)	! 032 = data access error
	VTRAP(T_DATA_PROT, winfault)	! 033 = data protection fault
	TRAP(T_ALIGN)			! 034 = address alignment error -- we could fix it inline...
	TRAP(T_LDDF_ALIGN)		! 035 = LDDF address alignment error -- we could fix it inline...
	TRAP(T_STDF_ALIGN)		! 036 = STDF address alignment error -- we could fix it inline...
	TRAP(T_PRIVACT)			! 037 = privileged action
	UTRAP(0x038); UTRAP(0x039); UTRAP(0x03a); UTRAP(0x03b); UTRAP(0x03c); 
	UTRAP(0x03d); UTRAP(0x03e); UTRAP(0x03f); 
	VTRAP(T_ASYNC_ERROR, winfault)	! 040 = data fetch fault
	SOFTINT4U(1, IE_L1)		! 041 = level 1 interrupt
	HARDINT4U(2)			! 042 = level 2 interrupt
	HARDINT4U(3)			! 043 = level 3 interrupt
	SOFTINT4U(4, IE_L4)		! 044 = level 4 interrupt
	HARDINT4U(5)			! 045 = level 5 interrupt
	SOFTINT4U(6, IE_L6)		! 046 = level 6 interrupt
	HARDINT4U(7)			! 047 = level 7 interrupt
	HARDINT4U(8)			! 048 = level 8 interrupt
	HARDINT4U(9)			! 049 = level 9 interrupt
	HARDINT4U(10)			! 04a = level 10 interrupt
	HARDINT4U(11)			! 04b = level 11 interrupt
	ZS_INTERRUPT4U			! 04c = level 12 (zs) interrupt
	HARDINT4U(13)			! 04d = level 13 interrupt
	HARDINT4U(14)			! 04e = level 14 interrupt
	VTRAP(15, winfault)		! 04f = nonmaskable interrupt
	UTRAP(0x050); UTRAP(0x051); UTRAP(0x052); UTRAP(0x053); UTRAP(0x054); UTRAP(0x055)
	UTRAP(0x056); UTRAP(0x057); UTRAP(0x058); UTRAP(0x059); UTRAP(0x05a); UTRAP(0x05b)
	UTRAP(0x05c); UTRAP(0x05d); UTRAP(0x05e); UTRAP(0x05f)
	VTRAP(0x060, interrupt_vector); ! 060 = interrupt vector
	UTRAP(T_PA_WATCHPT)		! 061 = physical address data watchpoint
	UTRAP(T_VA_WATCHPT)		! 062 = virtual address data watchpoint
	UTRAP(T_ECCERR)			! We'll implement this one later
ufast_IMMU_miss:			! 063 = fast instr access MMU miss
	TRACEFLT			! DEBUG
	ldxa	[%g0] ASI_IMMU_8KPTR, %g2	!				Load IMMU 8K TSB pointer
	ldxa	[%g0] ASI_IMMU, %g1	! Hard coded for unified 8K TSB		Load IMMU tag target register
	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g4	!				Load TSB tag and data into %g4 and %g5
	rdpr	%tstate, %g7				! DEBUG record if we're on MMU globals
	srlx	%g7, TSTATE_PSTATE_SHIFT, %g7		! DEBUG
	btst	PSTATE_MG, %g7				! DEBUG
	bz	0f					! DEBUG
	 set	_C_LABEL(missmmu), %g7			! DEBUG
	lduw	[%g7], %g6				! DEBUG
	inc	%g6					! DEBUG
	stw	%g6, [%g7]				! DEBUG
0:							! DEBUG
	brgez,pn %g5, instr_miss	!					Entry invalid?  Punt
	 xor	%g1, %g4, %g4		!					Compare TLB tags
	brnz,pn %g4, instr_miss		!					Got right tag?
	 nop
	CLRTT
	stxa	%g5, [%g0] ASI_IMMU_DATA_IN!					Enter new mapping
	retry				!					Try new mapping
1:
	sir
	TA32
ufast_DMMU_miss:			! 068 = fast data access MMU miss
	TRACEFLT			! DEBUG
	ldxa	[%g0] ASI_DMMU_8KPTR, %g2!					Load DMMU 8K TSB pointer
	ldxa	[%g0] ASI_DMMU, %g1	! Hard coded for unified 8K TSB		Load DMMU tag target register
	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g4	!				Load TSB tag and data into %g4 and %g5
	rdpr	%tstate, %g7				! DEBUG record if we're on MMU globals
	srlx	%g7, TSTATE_PSTATE_SHIFT, %g7		! DEBUG
	btst	PSTATE_MG, %g7				! DEBUG
	bz	0f					! DEBUG
	 set	_C_LABEL(missmmu), %g7			! DEBUG
	lduw	[%g7], %g6				! DEBUG
	inc	%g6					! DEBUG
	stw	%g6, [%g7]				! DEBUG
0:							! DEBUG
	brgez,pn %g5, data_miss		!					Entry invalid?  Punt
	 xor	%g1, %g4, %g4		!					Compare TLB tags
	brnz,pn	%g4, data_miss		!					Got right tag?
	 nop
	CLRTT
#ifdef TRAPSTATS
	sethi	%hi(_C_LABEL(udhit)), %g1
	lduw	[%g1+%lo(_C_LABEL(udhit))], %g2
	inc	%g2
	stw	%g2, [%g1+%lo(_C_LABEL(udhit))]
#endif
	stxa	%g5, [%g0] ASI_DMMU_DATA_IN!					Enter new mapping
	retry				!					Try new mapping
1:
	sir
	TA32
ufast_DMMU_protection:			! 06c = fast data access MMU protection
	TRACEFLT			! DEBUG -- we're perilously close to 32 insns
#ifdef TRAPSTATS
	sethi	%hi(_C_LABEL(udprot)), %g1
	lduw	[%g1+%lo(_C_LABEL(udprot))], %g2
	inc	%g2
	stw	%g2, [%g1+%lo(_C_LABEL(udprot))]
#endif
	rdpr	%tstate, %g7				! DEBUG record if we're on MMU globals
	srlx	%g7, TSTATE_PSTATE_SHIFT, %g7		! DEBUG
	btst	PSTATE_MG, %g7				! DEBUG
	bz	0f					! DEBUG
	 set	_C_LABEL(protmmu), %g7			! DEBUG
	lduw	[%g7], %g6				! DEBUG
	inc	%g6					! DEBUG
	stw	%g6, [%g7]				! DEBUG
0:							! DEBUG
#ifdef HWREF
	ba,a,pt	%xcc, dmmu_write_fault
#else
	ba,a,pt	%xcc, winfault
#endif
	nop
	TA32
	UTRAP(0x070)			! Implementation dependent traps
	UTRAP(0x071); UTRAP(0x072); UTRAP(0x073); UTRAP(0x074); UTRAP(0x075); UTRAP(0x076)
	UTRAP(0x077); UTRAP(0x078); UTRAP(0x079); UTRAP(0x07a); UTRAP(0x07b); UTRAP(0x07c)
	UTRAP(0x07d); UTRAP(0x07e); UTRAP(0x07f)
TABLE/**/uspill:	
	SPILL64(uspill8,ASI_AIUS)	! 0x080 spill_0_normal -- used to save user windows in user mode
	SPILL32(uspill4,ASI_AIUS)	! 0x084 spill_1_normal
	SPILLBOTH(uspill8,uspill4,ASI_AIUS)		! 0x088 spill_2_normal
#ifdef DEBUG
	sir
#endif
	UTRAP(0x08c); TA32	! 0x08c spill_3_normal
TABLE/**/kspill:
	SPILL64(kspill8,ASI_N)	! 0x090 spill_4_normal -- used to save supervisor windows
	SPILL32(kspill4,ASI_N)	! 0x094 spill_5_normal
	SPILLBOTH(kspill8,kspill4,ASI_N)	! 0x098 spill_6_normal
	UTRAP(0x09c); TA32	! 0x09c spill_7_normal
TABLE/**/uspillk:
	SPILL64(uspillk8,ASI_AIUS)	! 0x0a0 spill_0_other -- used to save user windows in supervisor mode
	SPILL32(uspillk4,ASI_AIUS)	! 0x0a4 spill_1_other
	SPILLBOTH(uspillk8,uspillk4,ASI_AIUS)	! 0x0a8 spill_2_other
	UTRAP(0x0ac); TA32	! 0x0ac spill_3_other
	UTRAP(0x0b0); TA32	! 0x0b0 spill_4_other
	UTRAP(0x0b4); TA32	! 0x0b4 spill_5_other
	UTRAP(0x0b8); TA32	! 0x0b8 spill_6_other
	UTRAP(0x0bc); TA32	! 0x0bc spill_7_other
TABLE/**/ufill:	
	FILL64(ufill8,ASI_AIUS) ! 0x0c0 fill_0_normal -- used to fill windows when running user mode
	FILL32(ufill4,ASI_AIUS)	! 0x0c4 fill_1_normal
	FILLBOTH(ufill8,ufill4,ASI_AIUS)	! 0x0c8 fill_2_normal
	UTRAP(0x0cc); TA32	! 0x0cc fill_3_normal
TABLE/**/kfill:	
	FILL64(kfill8,ASI_N)	! 0x0d0 fill_4_normal -- used to fill windows when running supervisor mode
	FILL32(kfill4,ASI_N)	! 0x0d4 fill_5_normal
	FILLBOTH(kfill8,kfill4,ASI_N)	! 0x0d8 fill_6_normal
	UTRAP(0x0dc); TA32	! 0x0dc fill_7_normal
TABLE/**/ufillk:	
	FILL64(ufillk8,ASI_AIUS)	! 0x0e0 fill_0_other
	FILL32(ufillk4,ASI_AIUS)	! 0x0e4 fill_1_other
	FILLBOTH(ufillk8,ufillk4,ASI_AIUS)	! 0x0e8 fill_2_other
	UTRAP(0x0ec); TA32	! 0x0ec fill_3_other
	UTRAP(0x0f0); TA32	! 0x0f0 fill_4_other
	UTRAP(0x0f4); TA32	! 0x0f4 fill_5_other
	UTRAP(0x0f8); TA32	! 0x0f8 fill_6_other
	UTRAP(0x0fc); TA32	! 0x0fc fill_7_other
TABLE/**/syscall:	
	SYSCALL			! 0x100 = sun syscall
	BPT			! 0x101 = pseudo breakpoint instruction
	STRAP(0x102); STRAP(0x103); STRAP(0x104); STRAP(0x105); STRAP(0x106); STRAP(0x107)
	SYSCALL			! 0x108 = svr4 syscall
	SYSCALL			! 0x109 = bsd syscall
	BPT_KGDB_EXEC		! 0x10a = enter kernel gdb on kernel startup
	STRAP(0x10b); STRAP(0x10c); STRAP(0x10d); STRAP(0x10e); STRAP(0x10f); 
	STRAP(0x110); STRAP(0x111); STRAP(0x112); STRAP(0x113); STRAP(0x114); STRAP(0x115); STRAP(0x116); STRAP(0x117)
	STRAP(0x118); STRAP(0x119); STRAP(0x11a); STRAP(0x11b); STRAP(0x11c); STRAP(0x11d); STRAP(0x11e); STRAP(0x11f)
	STRAP(0x120); STRAP(0x121); STRAP(0x122); STRAP(0x123); STRAP(0x124); STRAP(0x125); STRAP(0x126); STRAP(0x127)
	STRAP(0x128); STRAP(0x129); STRAP(0x12a); STRAP(0x12b); STRAP(0x12c); STRAP(0x12d); STRAP(0x12e); STRAP(0x12f)
	STRAP(0x130); STRAP(0x131); STRAP(0x132); STRAP(0x133); STRAP(0x134); STRAP(0x135); STRAP(0x136); STRAP(0x137)
	STRAP(0x138); STRAP(0x139); STRAP(0x13a); STRAP(0x13b); STRAP(0x13c); STRAP(0x13d); STRAP(0x13e); STRAP(0x13f)
	STRAP(0x140); STRAP(0x141); STRAP(0x142); STRAP(0x143); STRAP(0x144); STRAP(0x145); STRAP(0x146); STRAP(0x147)
	STRAP(0x148); STRAP(0x149); STRAP(0x14a); STRAP(0x14b); STRAP(0x14c); STRAP(0x14d); STRAP(0x14e); STRAP(0x14f)
	STRAP(0x150); STRAP(0x151); STRAP(0x152); STRAP(0x153); STRAP(0x154); STRAP(0x155); STRAP(0x156); STRAP(0x157)
	STRAP(0x158); STRAP(0x159); STRAP(0x15a); STRAP(0x15b); STRAP(0x15c); STRAP(0x15d); STRAP(0x15e); STRAP(0x15f)
	STRAP(0x160); STRAP(0x161); STRAP(0x162); STRAP(0x163); STRAP(0x164); STRAP(0x165); STRAP(0x166); STRAP(0x167)
	STRAP(0x168); STRAP(0x169); STRAP(0x16a); STRAP(0x16b); STRAP(0x16c); STRAP(0x16d); STRAP(0x16e); STRAP(0x16f)
	STRAP(0x170); STRAP(0x171); STRAP(0x172); STRAP(0x173); STRAP(0x174); STRAP(0x175); STRAP(0x176); STRAP(0x177)
	STRAP(0x178); STRAP(0x179); STRAP(0x17a); STRAP(0x17b); STRAP(0x17c); STRAP(0x17d); STRAP(0x17e); STRAP(0x17f)
	! Traps beyond 0x17f are reserved
	UTRAP(0x180); UTRAP(0x181); UTRAP(0x182); UTRAP(0x183); UTRAP(0x184); UTRAP(0x185); UTRAP(0x186); UTRAP(0x187)
	UTRAP(0x188); UTRAP(0x189); UTRAP(0x18a); UTRAP(0x18b); UTRAP(0x18c); UTRAP(0x18d); UTRAP(0x18e); UTRAP(0x18f)
	UTRAP(0x190); UTRAP(0x191); UTRAP(0x192); UTRAP(0x193); UTRAP(0x194); UTRAP(0x195); UTRAP(0x196); UTRAP(0x197)
	UTRAP(0x198); UTRAP(0x199); UTRAP(0x19a); UTRAP(0x19b); UTRAP(0x19c); UTRAP(0x19d); UTRAP(0x19e); UTRAP(0x19f)
	UTRAP(0x1a0); UTRAP(0x1a1); UTRAP(0x1a2); UTRAP(0x1a3); UTRAP(0x1a4); UTRAP(0x1a5); UTRAP(0x1a6); UTRAP(0x1a7)
	UTRAP(0x1a8); UTRAP(0x1a9); UTRAP(0x1aa); UTRAP(0x1ab); UTRAP(0x1ac); UTRAP(0x1ad); UTRAP(0x1ae); UTRAP(0x1af)
	UTRAP(0x1b0); UTRAP(0x1b1); UTRAP(0x1b2); UTRAP(0x1b3); UTRAP(0x1b4); UTRAP(0x1b5); UTRAP(0x1b6); UTRAP(0x1b7)
	UTRAP(0x1b8); UTRAP(0x1b9); UTRAP(0x1ba); UTRAP(0x1bb); UTRAP(0x1bc); UTRAP(0x1bd); UTRAP(0x1be); UTRAP(0x1bf)
	UTRAP(0x1c0); UTRAP(0x1c1); UTRAP(0x1c2); UTRAP(0x1c3); UTRAP(0x1c4); UTRAP(0x1c5); UTRAP(0x1c6); UTRAP(0x1c7)
	UTRAP(0x1c8); UTRAP(0x1c9); UTRAP(0x1ca); UTRAP(0x1cb); UTRAP(0x1cc); UTRAP(0x1cd); UTRAP(0x1ce); UTRAP(0x1cf)
	UTRAP(0x1d0); UTRAP(0x1d1); UTRAP(0x1d2); UTRAP(0x1d3); UTRAP(0x1d4); UTRAP(0x1d5); UTRAP(0x1d6); UTRAP(0x1d7)
	UTRAP(0x1d8); UTRAP(0x1d9); UTRAP(0x1da); UTRAP(0x1db); UTRAP(0x1dc); UTRAP(0x1dd); UTRAP(0x1de); UTRAP(0x1df)
	UTRAP(0x1e0); UTRAP(0x1e1); UTRAP(0x1e2); UTRAP(0x1e3); UTRAP(0x1e4); UTRAP(0x1e5); UTRAP(0x1e6); UTRAP(0x1e7)
	UTRAP(0x1e8); UTRAP(0x1e9); UTRAP(0x1ea); UTRAP(0x1eb); UTRAP(0x1ec); UTRAP(0x1ed); UTRAP(0x1ee); UTRAP(0x1ef)
	UTRAP(0x1f0); UTRAP(0x1f1); UTRAP(0x1f2); UTRAP(0x1f3); UTRAP(0x1f4); UTRAP(0x1f5); UTRAP(0x1f6); UTRAP(0x1f7)
	UTRAP(0x1f8); UTRAP(0x1f9); UTRAP(0x1fa); UTRAP(0x1fb); UTRAP(0x1fc); UTRAP(0x1fd); UTRAP(0x1fe); UTRAP(0x1ff)

	/* Traps from TL>0 -- traps from supervisor mode */
#undef TABLE
#define TABLE	nucleus_
trapbase_priv:
	UTRAP(0x000)		! 000 = reserved -- Use it to boot
	/* We should not get the next 5 traps */
	UTRAP(0x001)		! 001 = POR Reset -- ROM should get this
	UTRAP(0x002)		! 002 = WDR Watchdog -- ROM should get this
	UTRAP(0x003)		! 003 = XIR -- ROM should get this
	UTRAP(0x004)		! 004 = SIR -- ROM should get this
	UTRAP(0x005)		! 005 = RED state exception
	UTRAP(0x006); UTRAP(0x007)
ktextfault:	
	VTRAP(T_INST_EXCEPT, textfault)	! 008 = instr. access exept
	VTRAP(T_TEXTFAULT, textfault)	! 009 = instr access MMU miss -- no MMU
	VTRAP(T_INST_ERROR, textfault)	! 00a = instr. access err
	UTRAP(0x00b); UTRAP(0x00c); UTRAP(0x00d); UTRAP(0x00e); UTRAP(0x00f)
	TRAP(T_ILLINST)			! 010 = illegal instruction
	TRAP(T_PRIVINST)		! 011 = privileged instruction
	UTRAP(0x012)			! 012 = unimplemented LDD
	UTRAP(0x013)			! 013 = unimplemented STD
	UTRAP(0x014); UTRAP(0x015); UTRAP(0x016); UTRAP(0x017); UTRAP(0x018)
	UTRAP(0x019); UTRAP(0x01a); UTRAP(0x01b); UTRAP(0x01c); UTRAP(0x01d)
	UTRAP(0x01e); UTRAP(0x01f)
	TRAP(T_FPDISABLED)		! 020 = fp instr, but EF bit off in psr
	VTRAP(T_FP_IEEE_754, fp_exception)		! 021 = ieee 754 exception
	VTRAP(T_FP_OTHER, fp_exception)		! 022 = other fp exception
	TRAP(T_TAGOF)			! 023 = tag overflow
	TRACEWIN			! DEBUG
	clr	%l0
!	set	0xbadbeef, %l0		! DEBUG
	mov %l0, %l1; mov %l0, %l2	! 024-027 = clean window trap
	rdpr %cleanwin, %o7		!	This handler is in-lined and cannot fault
	inc %o7; mov %l0, %l3; mov %l0, %l4	!       Nucleus (trap&IRQ) code does not need clean windows
	wrpr %g0, %o7, %cleanwin	!	Clear out %l0-%l8 and %o0-%o8 and inc %cleanwin and done
	
	mov %l0, %l5; mov %l0, %l6; mov %l0, %l7
	mov %l0, %o0; mov %l0, %o1; mov %l0, %o2; mov %l0, %o3
	
	mov %l0, %o4; mov %l0, %o5; mov %l0, %o6; mov %l0, %o7
	CLRTT
	retry; nop; TA32
	TRAP(T_DIV0)			! 028 = divide by zero
	UTRAP(0x029)			! 029 = internal processor error
	UTRAP(0x02a); UTRAP(0x02b); UTRAP(0x02c); UTRAP(0x02d); UTRAP(0x02e); UTRAP(0x02f)
kdatafault:	
	VTRAP(T_DATAFAULT, winfault)	! 030 = data fetch fault
	UTRAP(0x031)			! 031 = data MMU miss -- no MMU
	VTRAP(T_DATA_ERROR, winfault)	! 032 = data fetch fault
	VTRAP(T_DATA_PROT, winfault)	! 033 = data fetch fault
!	TRAP(T_ALIGN)			! 034 = address alignment error -- we could fix it inline...
	sir; nop; TA8	! DEBUG -- trap all kernel alignment errors
	TRAP(T_LDDF_ALIGN)		! 035 = LDDF address alignment error -- we could fix it inline...
	TRAP(T_STDF_ALIGN)		! 036 = STDF address alignment error -- we could fix it inline...
	TRAP(T_PRIVACT)			! 037 = privileged action
	UTRAP(0x038); UTRAP(0x039); UTRAP(0x03a); UTRAP(0x03b); UTRAP(0x03c); 
	UTRAP(0x03d); UTRAP(0x03e); UTRAP(0x03f); 
	VTRAP(T_ASYNC_ERROR, winfault)	! 040 = data fetch fault
	SOFTINT4U(1, IE_L1)		! 041 = level 1 interrupt
	HARDINT4U(2)			! 042 = level 2 interrupt
	HARDINT4U(3)			! 043 = level 3 interrupt
	SOFTINT4U(4, IE_L4)		! 044 = level 4 interrupt
	HARDINT4U(5)			! 045 = level 5 interrupt
	SOFTINT4U(6, IE_L6)		! 046 = level 6 interrupt
	HARDINT4U(7)			! 047 = level 7 interrupt
	HARDINT4U(8)			! 048 = level 8 interrupt
	HARDINT4U(9)			! 049 = level 9 interrupt
	HARDINT4U(10)			! 04a = level 10 interrupt
	HARDINT4U(11)			! 04b = level 11 interrupt
	ZS_INTERRUPT4U			! 04c = level 12 (zs) interrupt
	HARDINT4U(13)			! 04d = level 13 interrupt
	HARDINT4U(14)			! 04e = level 14 interrupt
	VTRAP(15, winfault)		! 04f = nonmaskable interrupt
	UTRAP(0x050); UTRAP(0x051); UTRAP(0x052); UTRAP(0x053); UTRAP(0x054); UTRAP(0x055)
	UTRAP(0x056); UTRAP(0x057); UTRAP(0x058); UTRAP(0x059); UTRAP(0x05a); UTRAP(0x05b)
	UTRAP(0x05c); UTRAP(0x05d); UTRAP(0x05e); UTRAP(0x05f)
	VTRAP(0x060, interrupt_vector); ! 060 = interrupt vector
	UTRAP(T_PA_WATCHPT)		! 061 = physical address data watchpoint
	UTRAP(T_VA_WATCHPT)		! 062 = virtual address data watchpoint
	UTRAP(T_ECCERR)			! We'll implement this one later
kfast_IMMU_miss:			! 063 = fast instr access MMU miss
	TRACEFLT			! DEBUG
	ldxa	[%g0] ASI_IMMU_8KPTR, %g2	!				Load IMMU 8K TSB pointer
	ldxa	[%g0] ASI_IMMU, %g1	! Hard coded for unified 8K TSB		Load IMMU tag target register
	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g4	!				Load TSB tag and data into %g4 and %g5
	rdpr	%tstate, %g7				! DEBUG record if we're on MMU globals
	srlx	%g7, TSTATE_PSTATE_SHIFT, %g7		! DEBUG
	btst	PSTATE_MG, %g7				! DEBUG
	bz	0f					! DEBUG
	 set	_C_LABEL(missmmu), %g7			! DEBUG
	lduw	[%g7], %g6				! DEBUG
	inc	%g6					! DEBUG
	stw	%g6, [%g7]				! DEBUG
0:							! DEBUG
	brgez,pn %g5, instr_miss	!					Entry invalid?  Punt
	 xor	%g1, %g4, %g4		!					Compare TLB tags
	brnz,pn %g4, instr_miss		!					Got right tag?
	 nop
	CLRTT
	stxa	%g5, [%g0] ASI_IMMU_DATA_IN!					Enter new mapping
	retry				!					Try new mapping
1:
	sir
	TA32
kfast_DMMU_miss:			! 068 = fast data access MMU miss
	TRACEFLT			! DEBUG
	ldxa	[%g0] ASI_DMMU_8KPTR, %g2!					Load DMMU 8K TSB pointer
	ldxa	[%g0] ASI_DMMU, %g1	! Hard coded for unified 8K TSB		Load DMMU tag target register
	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g4	!				Load TSB tag and data into %g4 and %g5
	rdpr	%tstate, %g7				! DEBUG record if we're on MMU globals
	srlx	%g7, TSTATE_PSTATE_SHIFT, %g7		! DEBUG
	btst	PSTATE_MG, %g7				! DEBUG
	bz	0f					! DEBUG
	 set	_C_LABEL(missmmu), %g7			! DEBUG
	lduw	[%g7], %g6				! DEBUG
	inc	%g6					! DEBUG
	stw	%g6, [%g7]				! DEBUG
0:							! DEBUG
	brgez,pn %g5, data_miss		!					Entry invalid?  Punt
	 xor	%g1, %g4, %g4		!					Compare TLB tags
	brnz,pn	%g4, data_miss		!					Got right tag?
	 nop
	CLRTT
#ifdef TRAPSTATS
	sethi	%hi(_C_LABEL(kdhit)), %g1
	lduw	[%g1+%lo(_C_LABEL(kdhit))], %g2
	inc	%g2
	stw	%g2, [%g1+%lo(_C_LABEL(kdhit))]
#endif
	stxa	%g5, [%g0] ASI_DMMU_DATA_IN!					Enter new mapping
	retry				!					Try new mapping
1:
	sir
	TA32
kfast_DMMU_protection:			! 06c = fast data access MMU protection
	TRACEFLT			! DEBUG
#ifdef TRAPSTATS
	sethi	%hi(_C_LABEL(kdprot)), %g1
	lduw	[%g1+%lo(_C_LABEL(kdprot))], %g2
	inc	%g2
	stw	%g2, [%g1+%lo(_C_LABEL(kdprot))]
#endif
	rdpr	%tstate, %g7				! DEBUG record if we're on MMU globals
	srlx	%g7, TSTATE_PSTATE_SHIFT, %g7		! DEBUG
	btst	PSTATE_MG, %g7				! DEBUG
	bz	0f					! DEBUG
	 set	_C_LABEL(protmmu), %g7			! DEBUG
	lduw	[%g7], %g6				! DEBUG
	inc	%g6					! DEBUG
	stw	%g6, [%g7]				! DEBUG
0:							! DEBUG
#ifdef HWREF
	ba,a,pt	%xcc, dmmu_write_fault
#else
	ba,a,pt	%xcc, winfault
#endif
	nop
	TA32
	UTRAP(0x070)			! Implementation dependent traps
	UTRAP(0x071); UTRAP(0x072); UTRAP(0x073); UTRAP(0x074); UTRAP(0x075); UTRAP(0x076)
	UTRAP(0x077); UTRAP(0x078); UTRAP(0x079); UTRAP(0x07a); UTRAP(0x07b); UTRAP(0x07c)
	UTRAP(0x07d); UTRAP(0x07e); UTRAP(0x07f)
TABLE/**/uspill:	
	SPILL64(1,ASI_AIUS)	! 0x080 spill_0_normal -- used to save user windows
	SPILL32(2,ASI_AIUS)	! 0x084 spill_1_normal
	SPILLBOTH(1b,2b,ASI_AIUS)	! 0x088 spill_2_normal
	UTRAP(0x08c); TA32	! 0x08c spill_3_normal
TABLE/**/kspill:
	SPILL64(1,ASI_N)	! 0x090 spill_4_normal -- used to save supervisor windows
	SPILL32(2,ASI_N)	! 0x094 spill_5_normal
	SPILLBOTH(1b,2b,ASI_N)	! 0x098 spill_6_normal
	UTRAP(0x09c); TA32	! 0x09c spill_7_normal
TABLE/**/uspillk:
	SPILL64(1,ASI_AIUS)	! 0x0a0 spill_0_other -- used to save user windows in nucleus mode
	SPILL32(2,ASI_AIUS)	! 0x0a4 spill_1_other
	SPILLBOTH(1b,2b,ASI_AIUS)	! 0x0a8 spill_2_other
	UTRAP(0x0ac); TA32	! 0x0ac spill_3_other
	UTRAP(0x0b0); TA32	! 0x0b0 spill_4_other
	UTRAP(0x0b4); TA32	! 0x0b4 spill_5_other
	UTRAP(0x0b8); TA32	! 0x0b8 spill_6_other
	UTRAP(0x0bc); TA32	! 0x0bc spill_7_other
TABLE/**/ufill:	
	FILL64(1,ASI_AIUS)	! 0x0c0 fill_0_normal -- used to fill windows when running nucleus mode from user
	FILL32(2,ASI_AIUS)	! 0x0c4 fill_1_normal
	FILLBOTH(1b,2b,ASI_AIUS)	! 0x0c8 fill_2_normal
	UTRAP(0x0cc); TA32	! 0x0cc fill_3_normal
TABLE/**/sfill:	
	FILL64(1,ASI_N)		! 0x0d0 fill_4_normal -- used to fill windows when running nucleus mode from supervisor
	FILL32(2,ASI_N)		! 0x0d4 fill_5_normal
	FILLBOTH(1b,2b,ASI_N)	! 0x0d8 fill_6_normal
	UTRAP(0x0dc); TA32	! 0x0dc fill_7_normal
TABLE/**/kfill:	
	FILL64(1,ASI_AIUS)	! 0x0e0 fill_0_other -- used to fill user windows when running nucleus mode -- will we ever use this?
	FILL32(2,ASI_AIUS)	! 0x0e4 fill_1_other
	FILLBOTH(1b,2b,ASI_AIUS)! 0x0e8 fill_2_other
	UTRAP(0x0ec); TA32	! 0x0ec fill_3_other
	UTRAP(0x0f0); TA32	! 0x0f0 fill_4_other
	UTRAP(0x0f4); TA32	! 0x0f4 fill_5_other
	UTRAP(0x0f8); TA32	! 0x0f8 fill_6_other
	UTRAP(0x0fc); TA32	! 0x0fc fill_7_other
TABLE/**/syscall:	
	SYSCALL			! 0x100 = sun syscall
	BPT			! 0x101 = pseudo breakpoint instruction
	STRAP(0x102); STRAP(0x103); STRAP(0x104); STRAP(0x105); STRAP(0x106); STRAP(0x107)
	SYSCALL			! 0x108 = svr4 syscall
	SYSCALL			! 0x109 = bsd syscall
	BPT_KGDB_EXEC		! 0x10a = enter kernel gdb on kernel startup
	STRAP(0x10b); STRAP(0x10c); STRAP(0x10d); STRAP(0x10e); STRAP(0x10f); 
	STRAP(0x110); STRAP(0x111); STRAP(0x112); STRAP(0x113); STRAP(0x114); STRAP(0x115); STRAP(0x116); STRAP(0x117)
	STRAP(0x118); STRAP(0x119); STRAP(0x11a); STRAP(0x11b); STRAP(0x11c); STRAP(0x11d); STRAP(0x11e); STRAP(0x11f)
	STRAP(0x120); STRAP(0x121); STRAP(0x122); STRAP(0x123); STRAP(0x124); STRAP(0x125); STRAP(0x126); STRAP(0x127)
	STRAP(0x128); STRAP(0x129); STRAP(0x12a); STRAP(0x12b); STRAP(0x12c); STRAP(0x12d); STRAP(0x12e); STRAP(0x12f)
	STRAP(0x130); STRAP(0x131); STRAP(0x132); STRAP(0x133); STRAP(0x134); STRAP(0x135); STRAP(0x136); STRAP(0x137)
	STRAP(0x138); STRAP(0x139); STRAP(0x13a); STRAP(0x13b); STRAP(0x13c); STRAP(0x13d); STRAP(0x13e); STRAP(0x13f)
	STRAP(0x140); STRAP(0x141); STRAP(0x142); STRAP(0x143); STRAP(0x144); STRAP(0x145); STRAP(0x146); STRAP(0x147)
	STRAP(0x148); STRAP(0x149); STRAP(0x14a); STRAP(0x14b); STRAP(0x14c); STRAP(0x14d); STRAP(0x14e); STRAP(0x14f)
	STRAP(0x150); STRAP(0x151); STRAP(0x152); STRAP(0x153); STRAP(0x154); STRAP(0x155); STRAP(0x156); STRAP(0x157)
	STRAP(0x158); STRAP(0x159); STRAP(0x15a); STRAP(0x15b); STRAP(0x15c); STRAP(0x15d); STRAP(0x15e); STRAP(0x15f)
	STRAP(0x160); STRAP(0x161); STRAP(0x162); STRAP(0x163); STRAP(0x164); STRAP(0x165); STRAP(0x166); STRAP(0x167)
	STRAP(0x168); STRAP(0x169); STRAP(0x16a); STRAP(0x16b); STRAP(0x16c); STRAP(0x16d); STRAP(0x16e); STRAP(0x16f)
	STRAP(0x170); STRAP(0x171); STRAP(0x172); STRAP(0x173); STRAP(0x174); STRAP(0x175); STRAP(0x176); STRAP(0x177)
	STRAP(0x178); STRAP(0x179); STRAP(0x17a); STRAP(0x17b); STRAP(0x17c); STRAP(0x17d); STRAP(0x17e); STRAP(0x17f)
	! Traps beyond 0x17f are reserved
	UTRAP(0x180); UTRAP(0x181); UTRAP(0x182); UTRAP(0x183); UTRAP(0x184); UTRAP(0x185); UTRAP(0x186); UTRAP(0x187)
	UTRAP(0x188); UTRAP(0x189); UTRAP(0x18a); UTRAP(0x18b); UTRAP(0x18c); UTRAP(0x18d); UTRAP(0x18e); UTRAP(0x18f)
	UTRAP(0x190); UTRAP(0x191); UTRAP(0x192); UTRAP(0x193); UTRAP(0x194); UTRAP(0x195); UTRAP(0x196); UTRAP(0x197)
	UTRAP(0x198); UTRAP(0x199); UTRAP(0x19a); UTRAP(0x19b); UTRAP(0x19c); UTRAP(0x19d); UTRAP(0x19e); UTRAP(0x19f)
	UTRAP(0x1a0); UTRAP(0x1a1); UTRAP(0x1a2); UTRAP(0x1a3); UTRAP(0x1a4); UTRAP(0x1a5); UTRAP(0x1a6); UTRAP(0x1a7)
	UTRAP(0x1a8); UTRAP(0x1a9); UTRAP(0x1aa); UTRAP(0x1ab); UTRAP(0x1ac); UTRAP(0x1ad); UTRAP(0x1ae); UTRAP(0x1af)
	UTRAP(0x1b0); UTRAP(0x1b1); UTRAP(0x1b2); UTRAP(0x1b3); UTRAP(0x1b4); UTRAP(0x1b5); UTRAP(0x1b6); UTRAP(0x1b7)
	UTRAP(0x1b8); UTRAP(0x1b9); UTRAP(0x1ba); UTRAP(0x1bb); UTRAP(0x1bc); UTRAP(0x1bd); UTRAP(0x1be); UTRAP(0x1bf)
	UTRAP(0x1c0); UTRAP(0x1c1); UTRAP(0x1c2); UTRAP(0x1c3); UTRAP(0x1c4); UTRAP(0x1c5); UTRAP(0x1c6); UTRAP(0x1c7)
	UTRAP(0x1c8); UTRAP(0x1c9); UTRAP(0x1ca); UTRAP(0x1cb); UTRAP(0x1cc); UTRAP(0x1cd); UTRAP(0x1ce); UTRAP(0x1cf)
	UTRAP(0x1d0); UTRAP(0x1d1); UTRAP(0x1d2); UTRAP(0x1d3); UTRAP(0x1d4); UTRAP(0x1d5); UTRAP(0x1d6); UTRAP(0x1d7)
	UTRAP(0x1d8); UTRAP(0x1d9); UTRAP(0x1da); UTRAP(0x1db); UTRAP(0x1dc); UTRAP(0x1dd); UTRAP(0x1de); UTRAP(0x1df)
	UTRAP(0x1e0); UTRAP(0x1e1); UTRAP(0x1e2); UTRAP(0x1e3); UTRAP(0x1e4); UTRAP(0x1e5); UTRAP(0x1e6); UTRAP(0x1e7)
	UTRAP(0x1e8); UTRAP(0x1e9); UTRAP(0x1ea); UTRAP(0x1eb); UTRAP(0x1ec); UTRAP(0x1ed); UTRAP(0x1ee); UTRAP(0x1ef)
	UTRAP(0x1f0); UTRAP(0x1f1); UTRAP(0x1f2); UTRAP(0x1f3); UTRAP(0x1f4); UTRAP(0x1f5); UTRAP(0x1f6); UTRAP(0x1f7)
	UTRAP(0x1f8); UTRAP(0x1f9); UTRAP(0x1fa); UTRAP(0x1fb); UTRAP(0x1fc); UTRAP(0x1fd); UTRAP(0x1fe); UTRAP(0x1ff)

#ifdef DEBUG
#define CHKREG(r) \
	ldx	[%o0 + 8*1], %o1; \
	cmp	r, %o1; \
	stx	%o0, [%o0]; \
	tne	1
globreg_debug:
	.xword	-1, 0, 0, 0, 0, 0, 0, 0
globreg_set:
	save	%sp, -CC64FSZ, %sp
	set	globreg_debug, %o0
	stx	%g0, [%o0]
	stx	%g1, [%o0 + 8*1]
	stx	%g2, [%o0 + 8*2]
	stx	%g3, [%o0 + 8*3]
	stx	%g4, [%o0 + 8*4]
	stx	%g5, [%o0 + 8*5]
	stx	%g6, [%o0 + 8*6]
	stx	%g7, [%o0 + 8*7]
	ret
	 restore
globreg_check:	
	save	%sp, -CC64FSZ, %sp
	rd	%pc, %o7
	set	globreg_debug, %o0
	ldx	[%o0], %o1
	brnz,pn	%o1, 1f		! Don't re-execute this
	CHKREG(%g1)
	CHKREG(%g2)
	CHKREG(%g3)
	CHKREG(%g4)
	CHKREG(%g5)
	CHKREG(%g6)
	CHKREG(%g7)
	nop
1:	ret
	 restore

	/*
	 * Checkpoint:	 store a byte value at KERNBASE+0x21
	 *		uses two temp regs
	 */
#define CHKPT(r1,r2,val) \
	sethi	%hi(KERNBASE), r1; \
	mov	val, r2; \
	stb	r2, [r1 + 0x21]

	/*
	 * Debug routine:
	 *
	 * If datafault manages to get an unaligned pmap entry
	 * we come here.  We want to save as many regs as we can.
	 * %g3 has the sfsr, and %g7 the result of the wstate 
	 * both of which we can toast w/out much lossage.
	 *
	 */
pmap_dumpflag:	
	.xword	0		! semaphore
	.globl	pmap_dumparea	! Get this into the kernel syms
pmap_dumparea:
	.space	(32*8)		! room to save 32 registers
pmap_screwup:
	rd	%pc, %g3
	sub	%g3, (pmap_screwup-pmap_dumparea), %g3! pc relative addressing 8^)
	ldstub	[%g3+( 0*0x8)], %g3
	tst	%g3		! Semaphore set?
	tnz	%xcc, 1; nop		! Then trap
	set	pmap_dumparea, %g3
	stx	%g3, [%g3+( 0*0x8)]	! set semaphore
	stx	%g1, [%g3+( 1*0x8)]	! Start saving regs
	stx	%g2, [%g3+( 2*0x8)]
	stx	%g3, [%g3+( 3*0x8)]	! Redundant, I know...
	stx	%g4, [%g3+( 4*0x8)]
	stx	%g5, [%g3+( 5*0x8)]
	stx	%g6, [%g3+( 6*0x8)]
	stx	%g7, [%g3+( 7*0x8)]
	stx	%i0, [%g3+( 8*0x8)]	
	stx	%i1, [%g3+( 9*0x8)]	
	stx	%i2, [%g3+(10*0x8)]
	stx	%i3, [%g3+(11*0x8)]	
	stx	%i4, [%g3+(12*0x8)]
	stx	%i5, [%g3+(13*0x8)]
	stx	%i6, [%g3+(14*0x8)]
	stx	%i7, [%g3+(15*0x8)]
	stx	%l0, [%g3+(16*0x8)]	
	stx	%l1, [%g3+(17*0x8)]	
	stx	%l2, [%g3+(18*0x8)]
	stx	%l3, [%g3+(19*0x8)]	
	stx	%l4, [%g3+(20*0x8)]
	stx	%l5, [%g3+(21*0x8)]
	stx	%l6, [%g3+(22*0x8)]
	stx	%l7, [%g3+(23*0x8)]
	stx	%o0, [%g3+(24*0x8)]	
	stx	%o1, [%g3+(25*0x8)]	
	stx	%o2, [%g3+(26*0x8)]
	stx	%o3, [%g3+(27*0x8)]	
	stx	%o4, [%g3+(28*0x8)]
	stx	%o5, [%g3+(29*0x8)]
	stx	%o6, [%g3+(30*0x8)]
	stx	%o7, [%g3+(31*0x8)]
	ta	1; nop		! Break into the debugger

#else
#define	CHKPT(r1,r2,val)
#define CHKREG(r)
#endif
		
#ifdef DEBUG_NOTDEF
/*
 * A hardware red zone is impossible.  We simulate one in software by
 * keeping a `red zone' pointer; if %sp becomes less than this, we panic.
 * This is expensive and is only enabled when debugging.
 */
#define	REDSIZE	(8*96)		/* some room for bouncing */
#define	REDSTACK 2048		/* size of `panic: stack overflow' region */
	.data
_C_LABEL(redzone):
	.word	_C_LABEL(idle_u) + REDSIZE
_C_LABEL(redstack):
	.space	REDSTACK
	.text
Lpanic_red:
	.asciz	"stack overflow"
	_ALIGN

	/* set stack pointer redzone to base+minstack; alters base */
#define	SET_SP_REDZONE(base, tmp) \
	add	base, REDSIZE, base; \
	sethi	%hi(_C_LABEL(redzone)), tmp; \
	st	base, [tmp + %lo(_C_LABEL(redzone))]

	/* variant with a constant */
#define	SET_SP_REDZONE_CONST(const, tmp1, tmp2) \
	set	(const) + REDSIZE, tmp1; \
	sethi	%hi(_C_LABEL(redzone)), tmp2; \
	st	tmp1, [tmp2 + %lo(_C_LABEL(redzone))]

	/* check stack pointer against redzone (uses two temps) */
#define	CHECK_SP_REDZONE(t1, t2) \
	sethi	%hi(_C_LABEL(redzone)), t1; \
	ld	[t1 + %lo(_C_LABEL(redzone))], t2; \
	cmp	%sp, t2;	/* if sp >= t2, not in red zone */ \
	bgeu	7f; nop;	/* and can continue normally */ \
	/* move to panic stack */ \
	st	%g0, [t1 + %lo(_C_LABEL(redzone))]; \
	set	_C_LABEL(redstack) + REDSTACK - 96, %sp; \
	/* prevent panic() from lowering ipl */ \
	sethi	%hi(_C_LABEL(panicstr)), t2; \
	set	Lpanic_red, t2; \
	st	t2, [t1 + %lo(_C_LABEL(panicstr))]; \
	rdpr	%pil, t1;		/* t1 = splhigh() */ \
	or	t1, PSR_PIL, t2; \
	wrpr	t2, 0, %pil; \
	save	%sp, -CCFSZ, %sp;	/* preserve current window */ \
	sethi	%hi(Lpanic_red), %o0; \
	call	_C_LABEL(panic); or %o0, %lo(Lpanic_red), %o0; \
7:

#else

#define	SET_SP_REDZONE(base, tmp)
#define	SET_SP_REDZONE_CONST(const, t1, t2)
#define	CHECK_SP_REDZONE(t1, t2)
#endif

#define TRACESIZ	0x01000
	.globl	_C_LABEL(trap_trace)
	.globl	_C_LABEL(trap_trace_ptr)
	.globl	_C_LABEL(trap_trace_end)
	.globl	_C_LABEL(trap_trace_dis)
_C_LABEL(trap_trace_dis):
	.word	1, 1		! Starts disabled.  DDB turns it on.
_C_LABEL(trap_trace_ptr):	
	.word	0, 0, 0, 0
_C_LABEL(trap_trace):
	.space	TRACESIZ
_C_LABEL(trap_trace_end):
	.space	0x20		! safety margin
#ifdef	TRAPTRACE
#define TRACEPTR	(_C_LABEL(trap_trace_ptr)-_C_LABEL(trap_trace))
#define TRACEDIS	(_C_LABEL(trap_trace_dis)-_C_LABEL(trap_trace))
traceit:
	set	trap_trace, %g2
	lduw	[%g2+TRACEDIS], %g4
	brnz,pn	%g4, 1f
	 lduw	[%g2+TRACEPTR], %g3
	rdpr	%tl, %g4
	rdpr	%tt, %g5
	set	_C_LABEL(curproc), %g6
	sllx	%g4, 13, %g4
	cmp	%g6, 0x68
	lduw	[%g6], %g6
	movz	%icc, %g0, %g6		! DISABLE PID
	or	%g4, %g5, %g4
	mov	%g0, %g5
	brz,pn	%g6, 2f
	 andncc	%g3, (TRACESIZ-1), %g0
!	ldsw	[%g6+P_PID], %g5	! Load PID
2:
	
	set	_C_LABEL(cpcb), %g6	! Load up nsaved
	lduw	[%g6], %g6
	ldub	[%g6 + PCB_NSAVED], %g6
	sllx	%g6, 9, %g6
	or	%g6, %g4, %g4
	
	rdpr	%tstate, %g6
	movnz	%icc, %g0, %g3
	rdpr	%tpc, %g7
	sth	%g4, [%g2+%g3]
	inc	2, %g3
	sth	%g5, [%g2+%g3]
	inc	2, %g3
	stw	%g6, [%g2+%g3]
	inc	4, %g3
	stw	%sp, [%g2+%g3]
	inc	4, %g3
	stw	%g7, [%g2+%g3]
	inc	4, %g3
1:	
	jmpl	%g1, %g0
	 stw	%g3, [%g2+TRACEPTR]
traceitwin:
	set	trap_trace, %l2
	lduw	[%l2+TRACEDIS], %l4
	brnz,pn	%l4, 1f
	 nop
	lduw	[%l2+TRACEPTR], %l3
	rdpr	%tl, %l4
	rdpr	%tt, %l5
	sllx	%l4, 13, %l4
	or	%l4, %l5, %l4
	clr	%l5		! Don't load PID
	andncc	%l3, (TRACESIZ-1), %g0
	movnz	%icc, %g0, %l3

	clr	%l0		! Don't load nsaved
	sllx	%l0, 9, %l1
	or	%l1, %l4, %l4
	rdpr	%tpc, %l7
	
	sth	%l4, [%l2+%l3]
	inc	2, %l3
	sth	%l5, [%l2+%l3]
	inc	2, %l3
	stw	%l0, [%l2+%l3]
	inc	4, %l3
	stw	%sp, [%l2+%l3]
	inc	4, %l3
	stw	%l7, [%l2+%l3]
	inc	4, %l3
	stw	%l3, [%l2+TRACEPTR]
1:
	jmpl	%l6, %g0
	 nop
reload64:		
	ldxa	[%sp+BIAS+0x00]%asi, %l0
	ldxa	[%sp+BIAS+0x08]%asi, %l1
	ldxa	[%sp+BIAS+0x10]%asi, %l2
	ldxa	[%sp+BIAS+0x18]%asi, %l3
	ldxa	[%sp+BIAS+0x20]%asi, %l4
	ldxa	[%sp+BIAS+0x28]%asi, %l5
	ldxa	[%sp+BIAS+0x30]%asi, %l6
	ldxa	[%sp+BIAS+0x38]%asi, %l7
	CLRTT
	retry
reload32:
	lda	[%sp+0x00]%asi, %l0
	lda	[%sp+0x04]%asi, %l1
	lda	[%sp+0x08]%asi, %l2
	lda	[%sp+0x0c]%asi, %l3
	lda	[%sp+0x10]%asi, %l4
	lda	[%sp+0x14]%asi, %l5
	lda	[%sp+0x18]%asi, %l6
	lda	[%sp+0x1c]%asi, %l7
	CLRTT
	retry
#endif	

/*
 * Every trap that enables traps must set up stack space.
 * If the trap is from user mode, this involves switching to the kernel
 * stack for the current process, which means putting WSTATE_KERN in
 * %wstate, moving the contents of %canrestore to %otherwin and clearing
 * %canrestore.
 *
 * Things begin to grow uglier....
 *
 * It is possible that the user stack is invalid or unmapped.  This
 * happens right after a fork() before the stack is faulted in to the
 * child process address space, or when growing the stack, or when paging
 * the stack in from swap.  In this case we can't save the contents of the
 * CPU to the stack, so we must save it to the PCB.  
 *
 * ASSUMPTIONS: TRAP_SETUP() is called with:
 *	%i0-%o7 - previous stack frame
 *	%g1-%g7 - One of: interrupt globals, alternate globals, MMU globals.
 *
 * We need to allocate a trapframe, generate a new stack window then 
 * switch to the normal globals, not necessarily in that order.  The
 * trapframe is allocated on either kernel (or interrupt for INTR_SETUP())
 * stack.  
 *
 * The `stackspace' argument is the number of stack bytes to allocate
 * for register-saving, and must be at least -64 (and typically more,
 * for global registers and %y).
 *
 * Trapframes should use -CCFSZ-TF_SIZE.  (TF_SIZE = sizeof(struct trapframe);
 * see trap.h.  This basically means EVERYONE.  Interrupt frames could
 * get away with less, but currently do not.)
 *
 * The basic outline here is:
 *
 *	if (trap came from kernel mode) {
 *		%sp = %fp - stackspace;
 *	} else {
 *		%sp = (top of kernel stack) - stackspace;
 *	}
 *
 * NOTE: if you change this code, you will have to look carefully
 * at the window overflow and underflow handlers and make sure they
 * have similar changes made as needed.
 */

/*
 * v9 machines do not have a trap window. 
 *
 * When we take a trap the trap state is pushed on to the stack of trap
 * registers, interrupts are disabled, then we switch to an alternate set 
 * of global registers.
 *
 * The trap handling code needs to allocate a trap frame on the kernel, or
 * for interrupts, the interrupt stack, save the out registers to the trap
 * frame, then switch to the normal globals and save them to the trap frame
 * too.  
 *
 * Since kernel stacks are all on one page and the interrupt stack is entirely
 * within the locked TLB, we can use physical addressing to save out our
 * trap frame so we don't trap during the TRAP_SETUP() operation.  There
 * is unfortunately no supportable method for issuing a non-trapping save.
 *
 * However, if we use physical addresses to save our trapframe, we will need
 * to clear out the data cache before continuing much further.
 * 
 * In short, what we need to do is:
 *
 *	all preliminary processing is done using the alternate globals
 *
 *	When we allocate our trap windows we must give up our globals because
 *	their state may have changed during the save operation
 *
 *	we need to save our normal globals as soon as we have a stack
 *
 * Finally, we may now call C code.
 *
 * This macro will destroy %g5-%g7.  %g0-%g4 remain unchanged.
 *
 * In order to properly handle nested traps without lossage, alternate
 * global %g6 is used as a kernel stack pointer.  It is set to the last
 * allocated stack pointer (trapframe) and the old value is stored in
 * tf_kstack.  It is restored when returning from a trap.  It is cleared
 * on entering user mode.
 */

 /*
  * Other misc. design criteria:
  *
  * When taking an address fault, fault info is in the sfsr, sfar,
  * TLB_TAG_ACCESS registers.  If we take another address fault
  * while trying to handle the first fault then that information,
  * the only information that tells us what address we trapped on,
  * can be potentially lost.  This trap can be caused when allocating
  * a register window with which to handle the trap because the save
  * may try to store or restore a register window that corresponds
  * to part of the stack that is not mapped.  Preventing this trap,
  * while possible, is much too complicated to do in a trap handler,
  * and then we will need to do just as much work to restore the processor
  * window state.
  *
  * Possible solutions to the problem:	
  *
  * Since we have separate AG, MG, and IG, we could have all traps
  * above level-1 preserve AG and use other registers.  This causes
  * a problem for the return from trap code which is coded to use
  * alternate globals only.
  *
  * We could store the trapframe and trap address info to the stack
  * using physical addresses.  Then we need to read it back using
  * physical addressing, or flush the D$.
  *
  * We could identify certain registers to hold address fault info.
  * this means that these registers need to be preserved across all
  * fault handling.  But since we only have 7 useable globals, that
  * really puts a cramp in our style.
  *
  * Finally, there is the issue of returning from kernel mode to user
  * mode.  If we need to issue a restore of a user window in kernel
  * mode, we need the window control registers in a user mode setup.
  * If the trap handlers notice the register windows are in user mode,
  * they will allocate a trapframe at the bottom of the kernel stack,
  * overwriting the frame we were trying to return to.  This means that
  * we must complete the restoration of all registers *before* switching
  * to a user-mode window configuration.
  *
  * Essentially we need to be able to write re-entrant code w/no stack.
  */
trap_setup_msg:
	.asciz	"TRAP_SETUP: tt=%x osp=%x nsp=%x tl=%x tpc=%x\n"
	_ALIGN
intr_setup_msg:
	.asciz	"INTR_SETUP: tt=%x osp=%x nsp=%x tl=%x tpc=%x\n"
	_ALIGN
#ifdef TRAPWIN
#define	TRAP_SETUP(stackspace) \
	sethi	%hi(USPACE), %g7; \
	sethi	%hi(_C_LABEL(cpcb)), %g6; \
	or	%g7, %lo(USPACE), %g7; \
	sethi	%hi((stackspace)), %g5; \
	lduw	[%g6 + %lo(_C_LABEL(cpcb))], %g6; \
	or	%g5, %lo((stackspace)), %g5; \
	add	%g6, %g7, %g6; \
	rdpr	%wstate, %g7;					/* Find if we're from user mode */ \
	\
	sra	%g5, 0, %g5;					/* Sign extend the damn thing */ \
	subcc	%g7, WSTATE_KERN, %g7;				/* Compare & leave in register */ \
	movz	%icc, %sp, %g6;					/* Select old (kernel) stack or base of kernel stack */ \
	add	%g6, %g5, %g6;					/* Allocate a stack frame */ \
	btst	1, %g6;						/* Fixup 64-bit stack if necessary */ \
	add	%g6, BIAS, %g5; \
	srl	%g6, 0, %g6;					/* truncate at 32-bits */ \
	movne	%icc, %g5, %g6; \
	\
	stx	%g1, [%g6 + CCFSZ + TF_FAULT]; \
	stx	%l0, [%g6 + CCFSZ + TF_L + (0*8)];		/* Save local registers to trap frame */ \
	stx	%l1, [%g6 + CCFSZ + TF_L + (1*8)]; \
	stx	%l2, [%g6 + CCFSZ + TF_L + (2*8)]; \
	stx	%l3, [%g6 + CCFSZ + TF_L + (3*8)]; \
	stx	%l4, [%g6 + CCFSZ + TF_L + (4*8)]; \
	stx	%l5, [%g6 + CCFSZ + TF_L + (5*8)]; \
	stx	%l6, [%g6 + CCFSZ + TF_L + (6*8)]; \
	\
	stx	%l7, [%g6 + CCFSZ + TF_L + (7*8)]; \
	stx	%i0, [%g6 + CCFSZ + TF_I + (0*8)];		/* Save in registers to trap frame */ \
	stx	%i1, [%g6 + CCFSZ + TF_I + (1*8)]; \
	stx	%i2, [%g6 + CCFSZ + TF_I + (2*8)]; \
	stx	%i3, [%g6 + CCFSZ + TF_I + (3*8)]; \
	stx	%i4, [%g6 + CCFSZ + TF_I + (4*8)]; \
	stx	%i5, [%g6 + CCFSZ + TF_I + (5*8)]; \
	stx	%i6, [%g6 + CCFSZ + TF_I + (6*8)]; \
	\
	stx	%i7, [%g6 + CCFSZ + TF_I + (7*8)]; \
	save	%g6, 0, %sp;					/* If we fault we should come right back here */ \
	stx	%i0, [%sp + CCFSZ + TF_O + (0*8)];		/* Save out registers to trap frame */ \
	stx	%i1, [%sp + CCFSZ + TF_O + (1*8)]; \
	stx	%i2, [%sp + CCFSZ + TF_O + (2*8)]; \
	stx	%i3, [%sp + CCFSZ + TF_O + (3*8)]; \
	stx	%i4, [%sp + CCFSZ + TF_O + (4*8)]; \
	stx	%i5, [%sp + CCFSZ + TF_O + (5*8)]; \
	stx	%i6, [%sp + CCFSZ + TF_O + (6*8)]; \
	\
	stx	%i7, [%sp + CCFSZ + TF_O + (7*8)]; \
	rdpr	%wstate, %g7; sub %g7, WSTATE_KERN, %g7; /* DEBUG */ \
	brz,pn	%g7, 1f;					/* If we were in kernel mode start saving globals */ \
	 rdpr	%canrestore, %g5;				/* Fixup register window state registers */ \
	/* came from user mode -- switch to kernel mode stack */ \
	wrpr	%g0, 0, %canrestore; \
	wrpr	%g0, %g5, %otherwin; \
	mov	CTX_PRIMARY, %g7; \
	wrpr	%g0, WSTATE_KERN, %wstate;			/* Enable kernel mode window traps -- now we can trap again */ \
	\
	stxa	%g0, [%g7] ASI_DMMU; 				/* Switch MMU to kernel primary context */ \
	sethi	%hi(KERNBASE), %g5; \
	membar	#Sync;						/* XXXX Should be taken care of by flush */ \
	flush	%g5;						/* Some convenient address that won't trap */ \
1: 

/*
 * Interrupt setup is almost exactly like trap setup, but we need to
 * go to the interrupt stack if (a) we came from user mode or (b) we
 * came from kernel mode on the kernel stack.
 *
 * We don't guarantee any registers are preserved during this operation.
 */
#define	INTR_SETUP(stackspace) \
	sethi	%hi(_C_LABEL(eintstack)), %g6; \
	sethi	%hi((stackspace)), %g5; \
	btst	1, %sp; \
	bz,pt	%icc, 0f; \
	 mov	%sp, %g1; \
	add	%sp, BIAS, %g1; \
0: \
	srl	%g1, 0, %g1;					/* truncate at 32-bits */ \
	or	%g6, %lo(_C_LABEL(eintstack)), %g6; \
	set	(_C_LABEL(eintstack)-_C_LABEL(intstack)), %g7;	/* XXXXXXXXXX This assumes kernel addresses are unique from user addresses */ \
	or	%g5, %lo((stackspace)), %g5; \
	sub	%g6, %g1, %g2;					/* Determine if we need to switch to intr stack or not */ \
	dec	%g7;						/* Make it into a mask */ \
	andncc	%g2, %g7, %g0;					/* XXXXXXXXXX This assumes kernel addresses are unique from user addresses */ \
	sra	%g5, 0, %g5;					/* Sign extend the damn thing */ \
	movz	%icc, %g1, %g6;					/* Stay on interrupt stack? */ \
	add	%g6, %g5, %g6;					/* Allocate a stack frame */ \
	stx	%l0, [%g6 + CCFSZ + TF_L + (0*8)];		/* Save local registers to trap frame */ \
	stx	%l1, [%g6 + CCFSZ + TF_L + (1*8)]; \
	stx	%l2, [%g6 + CCFSZ + TF_L + (2*8)]; \
	stx	%l3, [%g6 + CCFSZ + TF_L + (3*8)]; \
	stx	%l4, [%g6 + CCFSZ + TF_L + (4*8)]; \
	stx	%l5, [%g6 + CCFSZ + TF_L + (5*8)]; \
	stx	%l6, [%g6 + CCFSZ + TF_L + (6*8)]; \
	stx	%l7, [%g6 + CCFSZ + TF_L + (7*8)]; \
	stx	%i0, [%g6 + CCFSZ + TF_I + (0*8)];		/* Save in registers to trap frame */ \
	stx	%i1, [%g6 + CCFSZ + TF_I + (1*8)]; \
	stx	%i2, [%g6 + CCFSZ + TF_I + (2*8)]; \
	stx	%i3, [%g6 + CCFSZ + TF_I + (3*8)]; \
	stx	%i4, [%g6 + CCFSZ + TF_I + (4*8)]; \
	stx	%i5, [%g6 + CCFSZ + TF_I + (5*8)]; \
	stx	%i6, [%g6 + CCFSZ + TF_I + (6*8)]; \
	stx	%i7, [%g6 + CCFSZ + TF_I + (7*8)]; \
	save	%g6, 0, %sp;					/* If we fault we should come right back here */ \
	stx	%i0, [%sp + CCFSZ + TF_O + (0*8)];		/* Save out registers to trap frame */ \
	stx	%i1, [%sp + CCFSZ + TF_O + (1*8)]; \
	stx	%i2, [%sp + CCFSZ + TF_O + (2*8)]; \
	stx	%i3, [%sp + CCFSZ + TF_O + (3*8)]; \
	stx	%i4, [%sp + CCFSZ + TF_O + (4*8)]; \
	stx	%i5, [%sp + CCFSZ + TF_O + (5*8)]; \
	stx	%i6, [%sp + CCFSZ + TF_O + (6*8)]; \
	stx	%i6, [%sp + CCFSZ + TF_G + (0*8)];		/* Save fp in clockframe->cf_fp */ \
	stx	%i7, [%sp + CCFSZ + TF_O + (7*8)]; \
	rdpr	%wstate, %g7;					/* Find if we're from user mode */ \
	cmp	%g7, WSTATE_KERN;				/* Compare & leave in register */ \
	be,pn	%icc, 1f;					/* If we were in kernel mode start saving globals */ \
	/* came from user mode -- switch to kernel mode stack */ \
	 rdpr	%otherwin, %g5;					/* Has this already been done? */ \
	tst	%g5; tnz %xcc, 1; nop; /* DEBUG -- this should _NEVER_ happen */ \
	brnz,pn	%g5, 1f;					/* Don't set this twice */ \
	 rdpr	%canrestore, %g5;				/* Fixup register window state registers */ \
	wrpr	%g0, 0, %canrestore; \
	mov	CTX_PRIMARY, %g7; \
	wrpr	%g0, %g5, %otherwin; \
	sethi	%hi(KERNBASE), %g5; \
	wrpr	%g0, WSTATE_KERN, %wstate;			/* Enable kernel mode window traps -- now we can trap again */ \
	stxa	%g0, [%g7] ASI_DMMU; 				/* Switch MMU to kernel primary context */ \
	membar	#Sync;						/* XXXX Should be taken care of by flush */ \
	flush	%g5;						/* Some convenient address that won't trap */ \
1:
#else
#if 1
#define	TRAP_SETUP(stackspace) \
/*	save	%sp,-CC64FSZ,%sp; set (panicstack-CC64FSZ),%sp; set trap_setup_msg,%o0; rdpr %tt,%o1; mov %i6,%o2; rdpr %tl,%o4; rdpr %tpc,%o5; GLOBTOLOC; call printf; mov %o6,%o3; LOCTOGLOB; restore; /* DEBUG */ \
	sethi	%hi(USPACE), %g7; \
	sethi	%hi(_C_LABEL(cpcb)), %g6; \
	or	%g7, %lo(USPACE), %g7; \
	sethi	%hi((stackspace)), %g5; \
	lduw	[%g6 + %lo(_C_LABEL(cpcb))], %g6; \
	or	%g5, %lo((stackspace)), %g5; \
	add	%g6, %g7, %g6; \
	rdpr	%wstate, %g7;					/* Find if we're from user mode */ \
	sra	%g5, 0, %g5;					/* Sign extend the damn thing */ \
	sub	%g7, WSTATE_KERN, %g7;				/* Compare & leave in register */ \
	movrz	%g7, %sp, %g6;					/* Select old (kernel) stack or base of kernel stack */ \
	add	%g6, %g5, %g6;					/* Allocate a stack frame */ \
	btst	1, %g6;						/* Fixup 64-bit stack if necessary */ \
	add	%g6, BIAS, %g5; \
	movne	%xcc, %g5, %g6; \
	srl	%g6, 0, %g6;					/* XXXXXXXXXX truncate at 32-bits */ \
	stx	%g1, [%g6 + CCFSZ + TF_FAULT]; \
	stx	%o0, [%g6 + CCFSZ + TF_O + (0*8)];		/* Save out registers to trap frame */ \
	stx	%o1, [%g6 + CCFSZ + TF_O + (1*8)]; \
	stx	%o2, [%g6 + CCFSZ + TF_O + (2*8)]; \
	stx	%o3, [%g6 + CCFSZ + TF_O + (3*8)]; \
	stx	%o4, [%g6 + CCFSZ + TF_O + (4*8)]; \
	stx	%o5, [%g6 + CCFSZ + TF_O + (5*8)]; \
	stx	%o6, [%g6 + CCFSZ + TF_O + (6*8)]; \
	stx	%o7, [%g6 + CCFSZ + TF_O + (7*8)]; \
	stx	%l0, [%g6 + CCFSZ + TF_L + (0*8)];		/* Save local registers to trap frame */ \
	stx	%l1, [%g6 + CCFSZ + TF_L + (1*8)]; \
	stx	%l2, [%g6 + CCFSZ + TF_L + (2*8)]; \
	stx	%l3, [%g6 + CCFSZ + TF_L + (3*8)]; \
	stx	%l4, [%g6 + CCFSZ + TF_L + (4*8)]; \
	stx	%l5, [%g6 + CCFSZ + TF_L + (5*8)]; \
	stx	%l6, [%g6 + CCFSZ + TF_L + (6*8)]; \
	stx	%l7, [%g6 + CCFSZ + TF_L + (7*8)]; \
	stx	%i0, [%g6 + CCFSZ + TF_I + (0*8)];		/* Save in registers to trap frame */ \
	stx	%i1, [%g6 + CCFSZ + TF_I + (1*8)]; \
	stx	%i2, [%g6 + CCFSZ + TF_I + (2*8)]; \
	stx	%i3, [%g6 + CCFSZ + TF_I + (3*8)]; \
	stx	%i4, [%g6 + CCFSZ + TF_I + (4*8)]; \
	stx	%i5, [%g6 + CCFSZ + TF_I + (5*8)]; \
	stx	%i6, [%g6 + CCFSZ + TF_I + (6*8)]; \
	stx	%i7, [%g6 + CCFSZ + TF_I + (7*8)]; \
	brz,pn	%g7, 1f;					/* If we were in kernel mode start saving globals */ \
	/* came from user mode -- switch to kernel mode stack */ \
	 rdpr	%otherwin, %g5;					/* Has this already been done? */ \
	brnz,pn	%g5, 1f;					/* Don't set this twice */ \
	 rdpr	%canrestore, %g5;				/* Fixup register window state registers */ \
	wrpr	%g0, 0, %canrestore; \
	set	CTX_PRIMARY, %g7; \
	wrpr	%g0, %g5, %otherwin; \
	wrpr	%g0, WSTATE_KERN, %wstate;			/* Enable kernel mode window traps -- now we can trap again */ \
	mov	%g6, %sp;					/* Truly switch to new stack frame */ \
	sethi	%hi(KERNBASE), %g5; \
	stxa	%g0, [%g7] ASI_DMMU; 				/* Switch MMU to kernel primary context */ \
	membar	#Sync; \
	flush	%g5;						/* Some convenient address that won't trap */ \
1: \
	mov	%g6, %sp;					/* Truly switch to new stack frame.  This insn is idempotent */

#else
	/* This is the physically addressed version */
#define	TRAP_SETUP(stackspace) \
/*	save	%sp,-CC64FSZ,%sp; set (panicstack-CC64FSZ),%sp; set trap_setup_msg,%o0; rdpr %tt,%o1; mov %i6,%o2; rdpr %tl,%o4; rdpr %tpc,%o5; GLOBTOLOC; call printf; mov %o6,%o3; LOCTOGLOB; restore; /* DEBUG */ \
	sethi	%hi(USPACE), %g7; \
	sethi	%hi(_C_LABEL(cpcb)), %g6; \
	or	%g7, %lo(USPACE), %g7; \
	sethi	%hi((stackspace)), %g5; \
	lduw	[%g6 + %lo(_C_LABEL(cpcb))], %g6; \
	or	%g5, %lo((stackspace)), %g5; \
	add	%g6, %g7, %g6; \
	rdpr	%wstate, %g7;					/* Find if we're from user mode */ \
	sra	%g5, 0, %g5;					/* Sign extend the damn thing */ \
	sub	%g7, WSTATE_KERN, %g7;				/* Compare & leave in register */ \
	movrz	%g7, %sp, %g6;					/* Select old (kernel) stack or base of kernel stack */ \
	add	%g6, %g5, %g6;					/* Allocate a stack frame */ \
	btst	1, %g6;						/* Fixup 64-bit stack if necessary */ \
	add	%g6, BIAS, %g5; \
	movne	%xcc, %g5, %g6; \
	set	KERNBASE, %g5;					/* Is it in the locked TTE? */ \
	sub	%g5, %g6, %g5; \
	set	(4*1024*1024), %g7; \
	cmp	%g5, %g7; \
	mov	%g6, %g7;					/* We will use %g7 as the phys addr */ \
	blu,pt	%icc, 0f;					/* Yes */ \
	/* Now try to lookup a physical mapping for the pcb */ \
	 wr	%g0, ASI_NUCLEUS, %asi;				/* In case of problems finding PA */ \
	sethi	%hi(_C_LABEL(ctxbusy)), %g5; \
	lduw	[%g5 + %lo(_C_LABEL(ctxbusy))], %g5; \
	srlx	%g6, STSHIFT-2, %g7; \
	lduw	[%g5], %g5; \
	andn	%g7, 3, %g7; \
	add	%g7, %g5, %g5; \
	lduwa	[%g5] ASI_PHYS_CACHED, %g5; \
	srlx	%g6, PTSHIFT, %g7;				/* Convert to ptab offset */ \
	and	%g7, PTMASK, %g7; \
	brz,a,pn	%g5, 0f; \
	 mov	%g6, %g7; \
	sll	%g7, 3, %g7; \
	add	%g7, %g5, %g7; \
	ldxa	[%g7] ASI_PHYS_CACHED, %g7;			/* This one is physaddr */ \
	brgez,a,pn	%g7, 0f; \
	 mov	%g6, %g7; \
	srlx	%g7, PGSHIFT, %g7;				/* Isolate PA part */ \
	sll	%g6, 32-PGSHIFT, %g5;				/* And offset */ \
	sllx	%g7, PGSHIFT+23, %g7; \
	srl	%g5, 32-PGSHIFT, %g5; \
	srax	%g7, 23, %g7; \
	or	%g7, %g5, %g7;					/* Then combine them to form PA */ \
	wr	%g0, ASI_PHYS_CACHED, %asi;			/* Use ASI_PHYS_CACHED to prevent possible page faults */ \
0:\
	srl	%g6, 0, %g6;					/* XXXXXXXXXX truncate at 32-bits */ \
	srl	%g7, 0, %g7; \
	stxa	%g1, [%g6 + CCFSZ + TF_FAULT] %asi; \
	stxa	%o0, [%g7 + CCFSZ + TF_O + (0*8)] %asi; \
	stxa	%o1, [%g7 + CCFSZ + TF_O + (1*8)] %asi; \
	stxa	%o2, [%g7 + CCFSZ + TF_O + (2*8)] %asi; \
	stxa	%o3, [%g7 + CCFSZ + TF_O + (3*8)] %asi; \
	stxa	%o4, [%g7 + CCFSZ + TF_O + (4*8)] %asi; \
	stxa	%o5, [%g7 + CCFSZ + TF_O + (5*8)] %asi; \
	rdpr	%wstate, %g5;					/* Find if we're from user mode again */ \
	stxa	%o6, [%g7 + CCFSZ + TF_O + (6*8)] %asi; \
	sub	%g5, WSTATE_KERN, %g5;				/* Compare & leave in register again */ \
	stxa	%o7, [%g7 + CCFSZ + TF_O + (7*8)] %asi; \
	stxa	%l0, [%g7 + CCFSZ + TF_L + (0*8)] %asi; \
	stxa	%l1, [%g7 + CCFSZ + TF_L + (1*8)] %asi; \
	stxa	%l2, [%g7 + CCFSZ + TF_L + (2*8)] %asi; \
	stxa	%l3, [%g7 + CCFSZ + TF_L + (3*8)] %asi; \
	stxa	%l4, [%g7 + CCFSZ + TF_L + (4*8)] %asi; \
	stxa	%l5, [%g7 + CCFSZ + TF_L + (5*8)] %asi; \
	stxa	%l6, [%g7 + CCFSZ + TF_L + (6*8)] %asi; \
	stxa	%l7, [%g7 + CCFSZ + TF_L + (7*8)] %asi; \
	stxa	%i0, [%g7 + CCFSZ + TF_I + (0*8)] %asi; \
	stxa	%i1, [%g7 + CCFSZ + TF_I + (1*8)] %asi; \
	stxa	%i2, [%g7 + CCFSZ + TF_I + (2*8)] %asi; \
	stxa	%i3, [%g7 + CCFSZ + TF_I + (3*8)] %asi; \
	stxa	%i4, [%g7 + CCFSZ + TF_I + (4*8)] %asi; \
	stxa	%i5, [%g7 + CCFSZ + TF_I + (5*8)] %asi; \
	stxa	%i6, [%g7 + CCFSZ + TF_I + (6*8)] %asi; \
	stxa	%i7, [%g7 + CCFSZ + TF_I + (7*8)] %asi; \
	brz,pn	%g5, 1f;					/* If we were in kernel mode start saving globals */ \
	/* came from user mode -- switch to kernel mode stack */ \
	 rdpr	%otherwin, %g5;					/* Has this already been done? */ \
	brnz,pn	%g5, 1f;					/* Don't set this twice */ \
	 rdpr	%canrestore, %g5;				/* Fixup register window state registers */ \
	wrpr	%g0, 0, %canrestore; \
	set	CTX_PRIMARY, %g7; \
	wrpr	%g0, %g5, %otherwin; \
	wrpr	%g0, WSTATE_KERN, %wstate;			/* Enable kernel mode window traps -- now we can trap again */ \
	mov	%g6, %sp;					/* Truly switch to new stack frame */ \
	sethi	%hi(KERNBASE), %g5; \
	stxa	%g0, [%g7] ASI_DMMU; 				/* Switch MMU to kernel primary context */ \
	membar	#Sync; \
	flush	%g6;						/* Some convenient address that won't trap */ \
1: \
	mov	%g6, %sp;					/* Truly switch to new stack frame.  This insn is idempotent */

#endif
/*
 * Interrupt setup is almost exactly like trap setup, but we need to
 * go to the interrupt stack if (a) we came from user mode or (b) we
 * came from kernel mode on the kernel stack.
 */
#define	INTR_SETUP(stackspace) \
	sethi	%hi(_C_LABEL(eintstack)), %g6; \
	sethi	%hi((stackspace)), %g5; \
	btst	1, %sp; \
	bz,pt	%icc, 0f; \
	 mov	%sp, %g1; \
	add	%sp, BIAS, %g1; \
0: \
	srl	%g1, 0, %g1;					/* truncate at 32-bits */ \
	or	%g6, %lo(_C_LABEL(eintstack)), %g6; \
	set	(_C_LABEL(eintstack)-_C_LABEL(intstack)), %g7;	/* XXXXXXXXXX This assumes kernel addresses are unique from user addresses */ \
	or	%g5, %lo((stackspace)), %g5; \
	sub	%g6, %g1, %g2;					/* Determine if we need to switch to intr stack or not */ \
	dec	%g7;						/* Make it into a mask */ \
	andncc	%g2, %g7, %g0;					/* XXXXXXXXXX This assumes kernel addresses are unique from user addresses */ \
	sra	%g5, 0, %g5;					/* Sign extend the damn thing */ \
	movz	%icc, %g1, %g6;					/* Stay on interrupt stack? */ \
	add	%g6, %g5, %g6;					/* Allocate a stack frame */ \
	stx	%o0, [%g6 + CCFSZ + TF_O + (0*8)];		/* Save out registers to trap frame */ \
	stx	%o1, [%g6 + CCFSZ + TF_O + (1*8)]; \
	stx	%o2, [%g6 + CCFSZ + TF_O + (2*8)]; \
	stx	%o3, [%g6 + CCFSZ + TF_O + (3*8)]; \
	stx	%o4, [%g6 + CCFSZ + TF_O + (4*8)]; \
	stx	%o5, [%g6 + CCFSZ + TF_O + (5*8)]; \
	stx	%o6, [%g6 + CCFSZ + TF_O + (6*8)]; \
	stx	%o6, [%g6 + CCFSZ + TF_G + (0*8)];		/* Save fp in clockframe->cf_fp */ \
	stx	%o7, [%g6 + CCFSZ + TF_O + (7*8)]; \
	stx	%l0, [%g6 + CCFSZ + TF_L + (0*8)];		/* Save local registers to trap frame */ \
	stx	%l1, [%g6 + CCFSZ + TF_L + (1*8)]; \
	stx	%l2, [%g6 + CCFSZ + TF_L + (2*8)]; \
	stx	%l3, [%g6 + CCFSZ + TF_L + (3*8)]; \
	stx	%l4, [%g6 + CCFSZ + TF_L + (4*8)]; \
	stx	%l5, [%g6 + CCFSZ + TF_L + (5*8)]; \
	stx	%l6, [%g6 + CCFSZ + TF_L + (6*8)]; \
	stx	%l7, [%g6 + CCFSZ + TF_L + (7*8)]; \
	stx	%i0, [%g6 + CCFSZ + TF_I + (0*8)];		/* Save in registers to trap frame */ \
	stx	%i1, [%g6 + CCFSZ + TF_I + (1*8)]; \
	stx	%i2, [%g6 + CCFSZ + TF_I + (2*8)]; \
	stx	%i3, [%g6 + CCFSZ + TF_I + (3*8)]; \
	stx	%i4, [%g6 + CCFSZ + TF_I + (4*8)]; \
	stx	%i5, [%g6 + CCFSZ + TF_I + (5*8)]; \
	stx	%i6, [%g6 + CCFSZ + TF_I + (6*8)]; \
	stx	%i7, [%g6 + CCFSZ + TF_I + (7*8)]; \
	rdpr	%wstate, %g7;					/* Find if we're from user mode */ \
	sub	%g7, WSTATE_KERN, %g7;				/* Compare & leave in register */ \
	brz,pn	%g7, 1f;					/* If we were in kernel mode start saving globals */ \
	/* came from user mode -- switch to kernel mode stack */ \
	 rdpr	%otherwin, %g5;					/* Has this already been done? */ \
	brnz,pn	%g5, 1f;					/* Don't set this twice */ \
	 rdpr	%canrestore, %g5;				/* Fixup register window state registers */ \
	wrpr	%g0, 0, %canrestore; \
	set	CTX_PRIMARY, %g1; \
	wrpr	%g0, %g5, %otherwin; \
	wrpr	%g0, WSTATE_KERN, %wstate;			/* Enable kernel mode window traps -- now we can trap again */ \
	mov	%g6, %sp;					/* Truly switch to new stack frame */ \
	sethi	%hi(KERNBASE), %g5; \
	stxa	%g0, [%g1] ASI_DMMU; 				/* Switch MMU to kernel primary context */ \
	membar	#Sync; \
	flush	%g5;						/* Some convenient address that won't trap */ \
1: \
	mov	%g6, %sp;					/* Truly switch to new stack frame.  This insn is idempotent */

#endif

#ifdef DEBUG
	
	/* Look up kpte to test algorithm */
	.globl	asmptechk
asmptechk:
	mov	%o0, %g4	! pmap->pm_segs
	mov	%o1, %g3	! Addr to lookup -- mind the context
	
	srlx	%g3, 32, %g6
	brnz,pn	%g6, 1f		! >32 bits? not here
	 srlx	%g3, STSHIFT-2, %g5
	andn	%g5, 3, %g5
	lduw	[%g4+%g5], %g4				! Remember -- UNSIGNED
	brz,pn	%g4, 1f					! NULL entry? check somewhere else
	 srlx	%g3, PTSHIFT, %g5			! Convert to ptab offset
	and	%g5, PTMASK, %g5
	sll	%g5, 3, %g5
	ldx	[%g4+%g5], %g6
	brgez,pn %g6, 1f				! Entry invalid?  Punt
	 srlx	%g6, 32, %o0	
	retl
	 srl	%g6, 0, %o1	
1:
	mov	%g0, %o1
	retl
	 mov	%g0, %o0

2:
	.asciz	"asmptechk: %x %x %x %x:%x\r\n"
	_ALIGN
#endif

/*
 * This is the MMU protection handler.  It's too big to fit
 * in the trap table so I moved it here.  It's relatively simple.
 * It looks up the page mapping in the page table associated with
 * the trapping context.  It checks to see if the S/W writable bit
 * is set.  If so, it sets the H/W write bit, marks the tte modified,
 * and enters the mapping into the MMU.  Otherwise it does a regular
 * data fault.
 *
 *
 */
	ICACHE_ALIGN
dmmu_write_fault:
	mov	TLB_TAG_ACCESS, %g3			! Get real fault page
	ldxa	[%g3] ASI_DMMU, %g3			! from tag access register
 	nop; nop; nop		! Linux sez we need this after reading TAG_ACCESS	
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	lduw	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4
	sllx	%g3, (64-13), %g6			! Mask away address
	srlx	%g6, (64-13-2), %g6			! This is now the offset into ctxbusy
	lduw	[%g4+%g6], %g4				! Load up our page table.
	
#if DEBUG
	/* Make sure we don't try to replace a kernel translation */
	/* This should not be necessary */
	sethi	%hi(KERNBASE), %g5			! Don't need %lo
	set	0x0400000, %g6				! 4MB
	sub	%g3, %g5, %g5
	cmp	%g5, %g6
	tlu	%xcc, 1; nop
	blu,pn	%xcc, winfix				! Next insn in delay slot is unimportant
#endif

	srlx	%g3, 32, %g6
	brnz,pn	%g6, winfix				! >32 bits? not here
	 srlx	%g3, STSHIFT-2, %g5
	andn	%g5, 3, %g5
	add	%g5, %g4, %g4
	lduwa	[%g4] ASI_PHYS_CACHED, %g4		! Remember -- UNSIGNED
	brz,pn	%g4, winfix				! NULL entry? check somewhere else
	 srlx	%g3, PTSHIFT, %g5			! Convert to ptab offset
	and	%g5, PTMASK, %g5
	sll	%g5, 3, %g5
	add	%g5, %g4, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, winfix				! Entry invalid?  Punt
	 btst	TTE_REAL_W|TTE_W, %g4			! Is it a ref fault?
	bz,pn	%xcc, winfix				! No -- really fault
	 or	%g4, TTE_MODIFY|TTE_ACCESS|TTE_W, %g4	! Update the modified bit

	/* Need to check for and handle large pages. */
	srlx	%g4, 61, %g5				! Isolate the size bits
	andcc	%g5, 0x3, %g5				! 8K?
	bnz,pn	%icc, winfix				! We punt to the pmap code since we can't handle policy
	
	 ldxa	[%g0] ASI_DMMU_8KPTR, %g2		! Load DMMU 8K TSB pointer
	ldxa	[%g0] ASI_DMMU, %g1			! Hard coded for unified 8K TSB		Load DMMU tag target register
	stxa	%g4, [%g6] ASI_PHYS_CACHED		!  and write it out
	stx	%g1, [%g2]				! Update TSB entry tag
	stx	%g4, [%g2+8]				! Update TSB entry data


	set	trapbase, %g6	! debug
	stx	%g1, [%g6+0x40]	! debug
	set	0x88, %g5	! debug
	stx	%g4, [%g6+0x48]	! debug -- what we tried to enter in TLB
	stb	%g5, [%g6+0x8]	! debug
#ifdef TRAPSTATS
	sethi	%hi(_C_LABEL(protfix)), %g1
	lduw	[%g1+%lo(_C_LABEL(protfix))], %g2
	inc	%g2
	stw	%g2, [%g1+%lo(_C_LABEL(protfix))]
#endif
	wr	%g0, ASI_DMMU, %asi
	stxa	%g0, [SFSR] %asi			! clear out the fault
	membar	#Sync
	
	sllx	%g3, (64-12), %g6			! Need to demap old entry first
	mov	0x010, %g1				! Secondary flush
	mov	0x020, %g5				! Nucleus flush
	movrz	%g6, %g5, %g1				! Pick one
	andn	%g3, 0xfff, %g6
	or	%g6, %g1, %g6
	stxa	%g6, [%g6] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync					! No real reason for this XXXX
	
	stxa	%g4, [%g0] ASI_DMMU_DATA_IN		! Enter new mapping
	membar	#Sync
	retry
	
/*
 * Each memory data access fault from a fast access miss handler comes here.
 * We will quickly check if this is an original prom mapping before going
 * to the generic fault handler
 *
 * We will assume that %pil is not lost so we won't bother to save it
 * unless we're in an interrupt handler.
 *
 * On entry:
 *	We are on one of the alternate set of globals
 *	%g1 = MMU tag target
 *	%g2 = 8Kptr
 *	%g3 = TLB TAG ACCESS
 *
 * On return:
 *
 */
	ICACHE_ALIGN
data_miss:
#ifdef TRAPSTATS
	set	_C_LABEL(kdmiss), %g3
	set	_C_LABEL(udmiss), %g4
	rdpr	%tl, %g6
	dec	%g6
	movrz	%g6, %g4, %g3
	lduw	[%g3], %g4
	inc	%g4
	stw	%g4, [%g3]
#endif
	
	mov	TLB_TAG_ACCESS, %g3			! Get real fault page
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	ldxa	[%g3] ASI_DMMU, %g3			! from tag access register
 	nop; nop; nop		! Linux sez we need this after reading TAG_ACCESS
	lduw	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4
	sllx	%g3, (64-13), %g6			! Mask away address
	srlx	%g6, (64-13-2), %g6			! This is now the offset into ctxbusy
	lduw	[%g4+%g6], %g4				! Load up our page table.

#if DEBUG
	/* Make sure we don't try to replace a kernel translation */
	/* This should not be necessary */
	brnz,pt	%g6, Ludata_miss			! If user context continue miss
	sethi	%hi(KERNBASE), %g5			! Don't need %lo
	set	0x0400000, %g6				! 4MB
	sub	%g3, %g5, %g5
	cmp	%g5, %g6
	sethi	%hi(KERNBASE), %g7
	mov	6, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
	tlu	%xcc, 1; nop
	blu,pn	%xcc, winfix				! Next insn in delay slot is unimportant
	 mov	7, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
#endif
	
	/*
	 * Try to parse our page table. 
	 */
Ludata_miss:
	srlx	%g3, 32, %g6
	brnz,pn	%g6, winfix				! >32 bits? not here
	 srlx	%g3, STSHIFT-2, %g5
	set	8, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
	andn	%g5, 3, %g5
	add	%g5, %g4, %g4
	lduwa	[%g4] ASI_PHYS_CACHED, %g4		! Remember -- UNSIGNED
	brz,pn	%g4, winfix				! NULL entry? check somewhere else
	 srlx	%g3, PTSHIFT, %g5			! Convert to ptab offset
	set	9, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
	and	%g5, PTMASK, %g5
	sll	%g5, 3, %g5
	add	%g5, %g4, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, winfix				! Entry invalid?  Punt
	 bset	TTE_ACCESS, %g4				! Update the modified bit
	stxa	%g4, [%g6] ASI_PHYS_CACHED		!  and write it out
	stx	%g1, [%g2]				! Update TSB entry tag
	stx	%g4, [%g2+8]				! Update TSB entry data
	set	trapbase, %g6	! debug
	stx	%g3, [%g6+8]	! debug
	set	0xa, %g5	! debug
	stx	%g4, [%g6]	! debug -- what we tried to enter in TLB
	stb	%g5, [%g6+0x20]	! debug
	
	sllx	%g3, (64-12), %g6			! Need to demap old entry first
	mov	0x010, %g1				! Secondary flush
	mov	0x020, %g5				! Nucleus flush
	movrz	%g6, %g5, %g1				! Pick one
	andn	%g3, 0xfff, %g6
	or	%g6, %g1, %g6
	stxa	%g6, [%g6] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync					! No real reason for this XXXX
	
	stxa	%g4, [%g0] ASI_DMMU_DATA_IN		! Enter new mapping
	membar	#Sync
	CLRTT
	retry
	NOTREACHED
	!!
	!!  Check our prom mappings -- temporary
	!!
/*
 * Handler for making the trap window shiny clean.
 *
 * If the store that trapped was to a kernel address, panic.
 *
 * If the store that trapped was to a user address, stick it in the PCB.
 * Since we don't want to force user code to use the standard register
 * convention if we don't have to, we will not assume that %fp points to
 * anything valid.
 *
 * On entry:
 *	We are on one of the alternate set of globals
 *	%g1 = %tl - 1, tstate[tl-1], scratch	- local
 *	%g2 = %tl				- local
 *	%g3 = MMU tag access			- in
 *	%g4 = %cwp				- local
 *	%g5 = scratch				- local
 *	%g6 = cpcb				- local
 *	%g7 = scratch				- local
 *
 * On return:
 *
 * NB:	 remove most of this from main codepath & cleanup I$
 */
winfault:
#ifdef DEBUG
	sethi	%hi(trapbase), %g7			! debug
!	stx	%g0, [%g7]				! debug This is a real fault -- prevent another trap from watchdoging
	set	0x10, %g4				! debug
	stb	%g4, [%g7 + 0x20]			! debug
	CHKPT(%g4,%g7,0x19)
#endif
	mov	TLB_TAG_ACCESS, %g3			! Get real fault page from tag access register
	ldxa	[%g3] ASI_DMMU, %g3			! And put it into the non-MMU alternate regs
 	nop; nop; nop		! Linux sez we need this after reading TAG_ACCESS
winfix:	
	rdpr	%tl, %g2
	subcc	%g2, 1, %g1
	brlez,pt	%g1, datafault			! Don't go below trap level 1
	 sethi	%hi(_C_LABEL(cpcb)), %g6		! get current pcb

	
	CHKPT(%g4,%g7,0x20)
	wrpr	%g1, 0, %tl				! Pop a trap level
	rdpr	%tt, %g7				! Read type of prev. trap
	rdpr	%tstate, %g4				! Try to restore prev %cwp if we were executing a restore
	andn	%g7, 0x3f, %g5				!   window fill traps are all 0b 0000 11xx xxxx
	
	cmp	%g5, 0x0c0				!   so we mask lower bits & compare to 0b 0000 1100 0000
	bne,pt	%icc, winfixspill			! Dump our trap frame -- we will retry the fill when the page is loaded
	 cmp	%g5, 0x080				!   window spill traps are all 0b 0000 10xx xxxx

	!!
	!! This was a fill
	!!
winfixfill:
#ifdef TRAPSTATS
	set	_C_LABEL(wfill), %g1
	lduw	[%g1], %g5
	inc	%g5
	stw	%g5, [%g1]
#endif
	btst	TSTATE_PRIV, %g4			! User mode?
	and	%g4, CWP, %g5				! %g4 = %cwp of trap
	wrpr	%g5, %cwp				! Restore cwp from before fill trap -- regs should now be consisent
#ifdef NOTDEF_DEBUG
	set	panicstack, %g5
	saved
	save	%g5, -CC64FSZ, %sp
	GLOBTOLOC
	rdpr	%wstate, %l0
	wrpr	%g0, WSTATE_KERN, %wstate
	set	8f, %o0
	call	printf
	 mov	%fp, %o1
	wrpr	%l0, 0, %wstate
	LOCTOGLOB
	restore
	ba	9f
	 nop
8:
	.asciz	"winfix: fill fixup sp=%p\n"
	_ALIGN
9:	
#endif
	CHKPT(%g5,%g4,0xd)
	bz,pt	%icc, datafault				! We were in user mode -- normal fault
	 wrpr	%g7, 0, %tt
	
	/* 
	 * We're in a pickle here.  We were trying to return to user mode
	 * and the restore of the user window failed, so now we have one valid
	 * kernel window and a user window state.  If we do a TRAP_SETUP() now,
	 * our kernel window will be considered a user window and cause a
	 * fault when we try to save it later due to an invalid user address.
	 * If we return to where we faulted, our window state will not be valid
	 * and we will fault trying to enter user with our primary context of zero.
	 *
	 * What we'll do is arrange to have us return to return_from_trap so we will
	 * start the whole business over again.  But first, switch to a kernel window
	 * setup.  Let's see, canrestore and otherwin are zero.  Set WSTATE_KERN and
	 * make sure we're in kernel context and we're done.
	 */
	
#ifdef TRAPSTATS
	set	_C_LABEL(kwfill), %g4
	lduw	[%g4], %g7
	inc	%g7
	stw	%g7, [%g4]
#endif
	set	return_from_trap, %g4
	set	CTX_PRIMARY, %g7
	wrpr	%g4, 0, %tpc
	stxa	%g0, [%g7] ASI_DMMU
	inc	4, %g4
	membar	#Sync
	flush	%g4					! Isn't this convenient?
	wrpr	%g0, WSTATE_KERN, %wstate
	wrpr	%g0, 0, %canrestore			! These should be zero but
	wrpr	%g0, 0, %otherwin			! clear them just in case
	rdpr	%ver, %g5
	and	%g5, CWP, %g5
	wrpr	%g0, 0, %cleanwin
	dec	1, %g5					! NWINDOWS-1-1
	wrpr	%g5, 0, %cansave			! Invalidate all windows
	CHKPT(%g5,%g7,0xe)
!	flushw						! DEBUG
	ba,pt	%icc, datafault
	 wrpr	%g4, 0, %tnpc
winfixspill:	
	bne,a,pt	%xcc, datafault				! Was not a spill -- handle it normally
	 wrpr	%g2, 0, %tl				! Restore trap level for now XXXX

	!! 
	!! This was a spill
	!!
#if 1
	btst	TSTATE_PRIV, %g4			! From user mode?
!	cmp	%g2, 2					! From normal execution? take a fault.
	wrpr	%g2, 0, %tl				! We need to load the fault type so we can
	rdpr	%tt, %g5				! overwrite the lower trap and get it to the fault handler
	wrpr	%g1, 0, %tl
	wrpr	%g5, 0, %tt				! Copy over trap type for the fault handler
	and	%g4, CWP, %g5				! find %cwp from trap
#ifndef TRAPTRACE
	be,a,pt	%xcc, datafault				! Let's do a regular datafault.  When we try a save in datafault we'll
	 wrpr	%g5, 0, %cwp				!  return here and write out all dirty windows.
#else
	bne,pt	%xcc, 3f				! Let's do a regular datafault.  When we try a save in datafault we'll
	 nop
	wrpr	%g5, 0, %cwp				!  return here and write out all dirty windows.
	set	trap_trace, %g2
	lduw	[%g2+TRACEDIS], %g4
	brnz,pn	%g4, 1f
	 nop
	lduw	[%g2+TRACEPTR], %g3
	rdpr	%tl, %g4
	mov	2, %g5
	set	_C_LABEL(curproc), %g6
	sllx	%g4, 13, %g4
	lduw	[%g6], %g6
	clr	%g6		! DISABLE PID
	or	%g4, %g5, %g4
	mov	%g0, %g5
	brz,pn	%g6, 2f
	 andncc	%g3, (TRACESIZ-1), %g0
!	ldsw	[%g6+P_PID], %g5	! Load PID
2:	
	movnz	%icc, %g0, %g3
	
	set	_C_LABEL(cpcb), %g6	! Load up nsaved
	lduw	[%g6], %g6
	ldub	[%g6 + PCB_NSAVED], %g6
	sllx	%g6, 9, %g6
	or	%g6, %g4, %g4
	
	rdpr	%tstate, %g6
	rdpr	%tpc, %g7
	sth	%g4, [%g2+%g3]
	inc	2, %g3
	sth	%g5, [%g2+%g3]
	inc	2, %g3
	stw	%g6, [%g2+%g3]
	inc	4, %g3
	stw	%sp, [%g2+%g3]
	inc	4, %g3
	stw	%g7, [%g2+%g3]
	inc	4, %g3
	stw	%g3, [%g2+TRACEPTR]
1:
	ba	datafault
	 nop
3:	
#endif
#endif
	wrpr	%g2, 0, %tl				! Restore trap level for now XXXX	
	lduw	[%g6 + %lo(_C_LABEL(cpcb))], %g6	! This is in the locked TLB and should not fault
#ifdef TRAPSTATS
	set	_C_LABEL(wspill), %g7
	lduw	[%g7], %g5
	inc	%g5
	stw	%g5, [%g7]
#endif
	set	0x12, %g5				! debug
	sethi	%hi(trapbase), %g7			! debug
	stb	%g5, [%g7 + 0x20]			! debug
	CHKPT(%g5,%g7,0x11)

	/*
	 * Traverse kernel map to find paddr of cpcb and only us ASI_PHYS_CACHED to
	 * prevent any faults while saving the windows.  BTW if it isn't mapped, we
	 * will trap and hopefully panic.
	 */

!	ba	0f					! DEBUG -- don't use phys addresses
	 wr	%g0, ASI_NUCLEUS, %asi			! In case of problems finding PA
	sethi	%hi(_C_LABEL(ctxbusy)), %g1
	lduw	[%g1 + %lo(_C_LABEL(ctxbusy))], %g1
	srlx	%g6, STSHIFT-2, %g7
	lduw	[%g1], %g1
	andn	%g7, 3, %g7
	add	%g7, %g1, %g1
	lduwa	[%g1] ASI_PHYS_CACHED, %g1		! Also in locked tlb
	srlx	%g6, PTSHIFT, %g7			! Convert to ptab offset
	and	%g7, PTMASK, %g7
	brz	%g1, 0f
	 sll	%g7, 3, %g7
	add	%g1, %g7, %g7
	ldxa	[%g7] ASI_PHYS_CACHED, %g7		! This one is not
	brgez	%g7, 0f
	 srlx	%g7, PGSHIFT, %g7			! Isolate PA part
	sll	%g6, 32-PGSHIFT, %g6			! And offset
	sllx	%g7, PGSHIFT+23, %g7
	srl	%g6, 32-PGSHIFT, %g6
	srax	%g7, 23, %g7
	or	%g7, %g6, %g6				! Then combine them to form PA

	wr	%g0, ASI_PHYS_CACHED, %asi		! Use ASI_PHYS_CACHED to prevent possible page faults
0:
	/*
	 * Now save all user windows to cpcb.
	 */
#ifdef NOTDEF_DEBUG
	lduba	[%g6 + PCB_NSAVED] %asi, %g7		! make sure that pcb_nsaved
	brz,pt	%g7, 1f					! is zero, else
	 nop
	wrpr	%g0, 4, %tl
	sir						! Force a watchdog
1:	
#endif
	CHKPT(%g5,%g7,0x12)
	rdpr	%otherwin, %g7
	brnz,pt	%g7, 1f
	 rdpr	%canrestore, %g5
	rdpr	%cansave, %g1
	add	%g5, 1, %g7				! add the %cwp window to the list to save
!	movrnz	%g1, %g5, %g7				! If we're issuing a save
!	mov	%g5, %g7				! DEBUG
	wrpr	%g0, 0, %canrestore
	wrpr	%g7, 0, %otherwin			! Still in user mode -- need to switch to kernel mode
1:
	mov	%g7, %g1
	CHKPT(%g5,%g7,0x13)
	lduba	[%g6 + PCB_NSAVED] %asi, %g7		! Start incrementing pcb_nsaved

#ifdef DEBUG
	wrpr	%g0, 5, %tl
#endif
	mov	%g6, %g5
	brz,pt	%g7, winfixsave				! If it's in use, panic
	 saved						! frob window registers

	/* PANIC */
!	CHKPT(%g4,%g7,0x10)	! Checkpoint
!	sir						! Force a watchdog
#ifdef DEBUG
	wrpr	%g2, 0, %tl
#endif	
	mov	%g7, %o2
	rdpr	%ver, %o1
	sethi	%hi(2f), %o0
	and	%o1, CWP, %o1
	wrpr	%g0, %o1, %cleanwin
	dec	1, %o1
	wrpr	%g0, %o1, %cansave			! kludge away any more window problems
	wrpr	%g0, 0, %canrestore
	wrpr	%g0, 0, %otherwin
	or	%lo(2f), %o0, %o0
	wrpr	%g0, WSTATE_KERN, %wstate
	set	panicstack-CC64FSZ, %sp
	ta	1; nop					! This helps out traptrace.
	call	_C_LABEL(panic)				! This needs to be fixed properly but we should panic here
	 mov	%g1, %o1
	NOTREACHED
2:
	.asciz	"winfault: double invalid window at %p, nsaved=%d"
	_ALIGN
3:
	saved	
	save
winfixsave:	
	stxa	%l0, [%g5 + PCB_RW + ( 0*8)] %asi	! Save the window in the pcb, we can schedule other stuff in here
	stxa	%l1, [%g5 + PCB_RW + ( 1*8)] %asi
	stxa	%l2, [%g5 + PCB_RW + ( 2*8)] %asi
	stxa	%l3, [%g5 + PCB_RW + ( 3*8)] %asi
	stxa	%l4, [%g5 + PCB_RW + ( 4*8)] %asi
	stxa	%l5, [%g5 + PCB_RW + ( 5*8)] %asi
	stxa	%l6, [%g5 + PCB_RW + ( 6*8)] %asi
	stxa	%l7, [%g5 + PCB_RW + ( 7*8)] %asi

	stxa	%i0, [%g5 + PCB_RW + ( 8*8)] %asi
	stxa	%i1, [%g5 + PCB_RW + ( 9*8)] %asi
	stxa	%i2, [%g5 + PCB_RW + (10*8)] %asi
	stxa	%i3, [%g5 + PCB_RW + (11*8)] %asi
	stxa	%i4, [%g5 + PCB_RW + (12*8)] %asi
	stxa	%i5, [%g5 + PCB_RW + (13*8)] %asi
	stxa	%i6, [%g5 + PCB_RW + (14*8)] %asi
	stxa	%i7, [%g5 + PCB_RW + (15*8)] %asi

!	rdpr	%otherwin, %g1	! Check to see if we's done
	dec	%g1
	wrpr	%g0, 7, %cleanwin			! BUGBUG -- we should not hardcode this, but I have no spare globals
	inc	16*8, %g5				! Move to next window
	inc	%g7					! inc pcb_nsaved
	brnz,pt	%g1, 3b
	 stxa	%o6, [%g5 + PCB_RW + (14*8)] %asi	! Save %sp so we can write these all out
	
	/* fix up pcb fields */
	stba	%g7, [%g6 + PCB_NSAVED] %asi		! cpcb->pcb_nsaved = n
	CHKPT(%g5,%g1,0x14)
#if 0
	mov	%g7, %g5				! fixup window registers
5:
	dec	%g5
	brgz,a,pt	%g5, 5b
	 restore
#ifdef NOT_DEBUG
	rdpr	%wstate, %g5				! DEBUG
	wrpr	%g0, WSTATE_KERN, %wstate		! DEBUG
	wrpr	%g0, 4, %tl
	rdpr	%cansave, %g7
	rdpr	%canrestore, %g6
	flushw						! DEBUG
	wrpr	%g2, 0, %tl
	wrpr	%g5, 0, %wstate				! DEBUG
#endif
#else
	/*
	 * We just issued a bunch of saves, so %cansave is now 0,
	 * probably (if we were doing a flushw then we may have
	 * come in with only partially full register windows and
	 * it may not be 0).
	 *
	 * %g7 contains the count of the windows we just finished
	 * saving.
	 *
	 * What we need to do now is move some of the windows from
	 * %canrestore to %cansave.  What we should do is take
	 * min(%canrestore, %g7) and move that over to %cansave.
	 *
	 * %g7 is the number of windows we flushed, so we should
	 * use that as a base.  Clear out %otherwin, set %cansave 
	 * to min(%g7, NWINDOWS - 2), set %cleanwin to %canrestore
	 * + %cansave and the rest follows:
	 *
	 * %otherwin = 0
	 * %cansave = NWINDOWS - 2 - %canrestore
	 */
	wrpr	%g0, 0, %otherwin
	rdpr	%canrestore, %g1
	sub	%g1, %g7, %g1				! Calculate %canrestore - %g7
	movrlz	%g1, %g0, %g1				! Clamp at zero
	wrpr	%g1, 0, %canrestore			! This is the new canrestore
	rdpr	%ver, %g5
	and	%g5, CWP, %g5				! NWINDOWS-1
	dec	%g5					! NWINDOWS-2
	wrpr	%g5, 0, %cleanwin			! Set cleanwin to max, since we're in-kernel
	sub	%g5, %g1, %g5				! NWINDOWS-2-%canrestore
#ifdef TRAPTRACE
	wrpr	%g5, 0, %cleanwin			! Force cleanwindow faults
#endif
	wrpr	%g5, 0, %cansave
#ifdef NOT_DEBUG
	rdpr	%wstate, %g5				! DEBUG
	wrpr	%g0, WSTATE_KERN, %wstate		! DEBUG
	wrpr	%g0, 4, %tl
	flushw						! DEBUG
	wrpr	%g2, 0, %tl
	wrpr	%g5, 0, %wstate				! DEBUG
#endif
#endif
	
#ifdef NOTDEF_DEBUG
	set	panicstack-CC64FSZ, %g1
	save	%g1, 0, %sp
	GLOBTOLOC
	rdpr	%wstate, %l0
	wrpr	%g0, WSTATE_KERN, %wstate
	set	8f, %o0
	mov	%g7, %o1
	call	printf
	 mov	%g5, %o2
	wrpr	%l0, 0, %wstate
	LOCTOGLOB
	restore
	ba	9f
	 nop
8:
	.asciz	"winfix: spill fixup\n"
	_ALIGN
9:	
#endif
	CHKPT(%g5,%g1,0x15)
!	rdpr	%tl, %g2				! DEBUG DEBUG -- did we trap somewhere?
	sub	%g2, 1, %g1
	rdpr	%tt, %g2
	wrpr	%g1, 0, %tl				! We will not attempt to re-execute the spill, so dump our trap frame permanently
	wrpr	%g2, 0, %tt				! Move trap type from fault frame here, overwriting spill
	CHKPT(%g2,%g5,0x16)

	/* Did we save a user or kernel window ? */
!	srax	%g3, 48, %g7				! User or kernel store? (TAG TARGET)
	sllx	%g3, (64-13), %g7			! User or kernel store? (TAG ACCESS)
	brnz,pt	%g7, 1f					! User fault -- save windows to pcb
	 set	(2*NBPG)-8, %g7

	set	trapbase, %g7				! debug
	and	%g4, CWP, %g4				! %g4 = %cwp of trap
	wrpr	%g4, 0, %cwp				! Kernel fault -- restore %cwp and force and trap to debugger
	set	0x11, %g6				! debug
	stb	%g6, [%g7 + 0x20]			! debug
	CHKPT(%g2,%g1,0x17)
	sir
	ta	1; nop					! Enter debugger
	NOTREACHED
1:
#if 1
	/* Now we need to blast away the D$ to make sure we're in sync */
	stxa	%g0, [%g7] ASI_DCACHE_TAG
	brnz,pt	%g7, 1b
	 dec	8, %g7
#endif
	
	CHKPT(%g2,%g1,0x18)
	set	trapbase, %g7				! debug
	set	0x19, %g6				! debug
	stb	%g6, [%g7 + 0x20]			! debug
#ifdef NOTDEF_DEBUG
	set	panicstack-CC64FSZ, %g5
	save	%g5, 0, %sp
	GLOBTOLOC
	rdpr	%wstate, %l0
	wrpr	%g0, WSTATE_KERN, %wstate
	set	8f, %o0
	call	printf
	 mov	%fp, %o1
	wrpr	%l0, 0, %wstate
	LOCTOGLOB
	restore
	ba	9f
	 nop
8:
	.asciz	"winfix: kernel spill retry\n"
	_ALIGN
9:	
#endif
#ifdef TRAPTRACE
	and	%g4, CWP, %g2	! Point our regwin at right place
	wrpr	%g2, %cwp

	set	trap_trace, %g2
	lduw	[%g2+TRACEDIS], %g4
	brnz,pn	%g4, 1f
	 nop
	lduw	[%g2+TRACEPTR], %g3
	rdpr	%tl, %g4
	mov	3, %g5
	set	_C_LABEL(curproc), %g6
	sllx	%g4, 13, %g4
	lduw	[%g6], %g6
	clr	%g6		! DISABLE PID
	or	%g4, %g5, %g4
	mov	%g0, %g5
	brz,pn	%g6, 2f
	 andncc	%g3, (TRACESIZ-1), %g0
!	ldsw	[%g6+P_PID], %g5	! Load PID
2:	
	movnz	%icc, %g0, %g3
	
	set	_C_LABEL(cpcb), %g6	! Load up nsaved
	lduw	[%g6], %g6
	ldub	[%g6 + PCB_NSAVED], %g6
	sllx	%g6, 9, %g6
	or	%g6, %g4, %g4
	
	rdpr	%tstate, %g6
	rdpr	%tpc, %g7
	sth	%g4, [%g2+%g3]
	inc	2, %g3
	sth	%g5, [%g2+%g3]
	inc	2, %g3
	stw	%g6, [%g2+%g3]
	inc	4, %g3
	stw	%sp, [%g2+%g3]
	inc	4, %g3
	stw	%g7, [%g2+%g3]
	inc	4, %g3
	stw	%g3, [%g2+TRACEPTR]
1:	
#endif
#ifdef TRAPSTATS
	set	_C_LABEL(wspillskip), %g4
	lduw	[%g4], %g5
	inc	%g5
	stw	%g5, [%g4]
#endif
	/*
	 * If we had WSTATE_KERN then we had at least one valid kernel window.
	 * We should re-execute the trapping save.
	 */
	rdpr	%wstate, %g3
	cmp	%g3, WSTATE_KERN
	bne,pt	%icc, 1f
	 nop
	retry						! Now we can complete the save
1:	
	/*
	 * Since we had a WSTATE_USER, we had no valid kernel windows.  This should
	 * only happen inside TRAP_SETUP or INTR_SETUP. Emulate
	 * the instruction, clean up the register windows, then done.
	 */
	rdpr	%cwp, %g1
	inc	%g1
	rdpr	%tstate, %g2
	wrpr	%g1, %cwp
	andn	%g2, CWP, %g2
	wrpr	%g1, %g2, %tstate
	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate
#ifdef TRAPS_USE_IG
	wrpr	%g0, PSTATE_KERN|PSTATE_IG, %pstate	! DEBUG
#endif
	mov	%g6, %sp
	done

/*
 * Each memory data access fault, from user or kernel mode,
 * comes here. 
 *
 * We will assume that %pil is not lost so we won't bother to save it
 * unless we're in an interrupt handler.
 *
 * On entry:
 *	We are on one of the alternate set of globals
 *	%g1 = MMU tag target
 *	%g2 = %tl
 *
 * On return:
 *
 */
datafault:
	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate	! We need to save volatile stuff to AG regs
#ifdef TRAPS_USE_IG
	wrpr	%g0, PSTATE_KERN|PSTATE_IG, %pstate	! We need to save volatile stuff to AG regs
#endif
	set	trapbase, %g7				! debug
	set	0x20, %g6				! debug
	stx	%g0, [%g7]				! debug
	stb	%g6, [%g7 + 0x20]			! debug
	CHKPT(%g4,%g7,0xf)
	wr	%g0, ASI_DMMU, %asi			! We need to re-load trap info
	ldxa	[%g0 + TLB_TAG_ACCESS] %asi, %g1	! Get fault address from tag access register
 	nop; nop; nop		! Linux sez we need this after reading TAG_ACCESS
	ldxa	[SFAR] %asi, %g2			! sync virt addr; must be read first
	ldxa	[SFSR] %asi, %g3			! get sync fault status register
	stxa	%g0, [SFSR] %asi			! Clear out fault now
	membar	#Sync					! No real reason for this XXXX
	
	TRAP_SETUP(-CCFSZ-TF_SIZE)
#if defined(UVM)
	INCR(_C_LABEL(uvmexp)+V_FAULTS)			! cnt.v_faults++ (clobbers %o0,%o1) should not fault
#else
	INCR(_C_LABEL(cnt)+V_FAULTS)			! cnt.v_faults++ (clobbers %o0,%o1) should not fault
#endif

!	ldx	[%sp + CCFSZ + TF_FAULT], %g1		! DEBUG make sure this has not changed
	mov	%g1, %o5				! Move these to the out regs so we can save the globals
	mov	%g2, %o1
	mov	%g3, %o2

	ldxa	[%g0] ASI_AFSR, %o3			! get async fault status
	ldxa	[%g0] ASI_AFAR, %o4			! get async fault address
	mov	-1, %g7
	stxa	%g7, [%g0] ASI_AFSR			! And clear this out, too
	membar	#Sync					! No real reason for this XXXX

#ifdef TRAPTRACE
	rdpr	%tt, %o0				! find out what trap brought us here
	wrpr	%g0, 0x69, %tt	! We claim to be trap type 69, not a valid trap
	TRACEME
	wrpr	%g0, PSTATE_KERN, %pstate		! Get back to normal globals
	
	stx	%g1, [%sp + CCFSZ + TF_G + (1*8)]	! save g1
#else
	wrpr	%g0, PSTATE_KERN, %pstate		! Get back to normal globals
	
	stx	%g1, [%sp + CCFSZ + TF_G + (1*8)]	! save g1
	rdpr	%tt, %o0				! find out what trap brought us here
#endif
	stx	%g2, [%sp + CCFSZ + TF_G + (2*8)]	! save g2
	rdpr	%tstate, %g1
	stx	%g3, [%sp + CCFSZ + TF_G + (3*8)]	! (sneak g3 in here)
	rdpr	%tpc, %g2
	stx	%g4, [%sp + CCFSZ + TF_G + (4*8)]	! sneak in g4
	rdpr	%tnpc, %g3
	stx	%g5, [%sp + CCFSZ + TF_G + (5*8)]	! sneak in g5
	rd	%y, %g4					! save y
	stx	%g6, [%sp + CCFSZ + TF_G + (6*8)]	! sneak in g6
	mov	%g2, %o7				! Make the fault address look like the return address
	stx	%g7, [%sp + CCFSZ + TF_G + (7*8)]	! sneak in g7

#if 1
	set	trapbase, %g7				! debug
	set	0x21, %g6				! debug
	stb	%g6, [%g7 + 0x20]			! debug
#endif
	sth	%o0, [%sp + CCFSZ + TF_TT]! debug
	stx	%g1, [%sp + CCFSZ + TF_TSTATE]		! set tf.tf_psr, tf.tf_pc
	stx	%g2, [%sp + CCFSZ + TF_PC]		! set tf.tf_npc
	stx	%g3, [%sp + CCFSZ + TF_NPC]
	
	rdpr	%pil, %g5
	stb	%g5, [%sp + CCFSZ + TF_PIL]
	stb	%g5, [%sp + CCFSZ + TF_OLDPIL]

#if 1
	rdpr	%tl, %g7
	dec	%g7	
	movrlz	%g7, %g0, %g7
	CHKPT(%g1,%g2,0x21)
	wrpr	%g0, %g7, %tl		! Revert to kernel mode
#else
	CHKPT(%g1,%g2,0x21)
	wrpr	%g0, 0, %tl		! Revert to kernel mode
#endif
	/* Finish stackframe, call C trap handler */
	flushw						! Get this clean so we won't take any more user faults
#ifdef NOTDEF_DEBUG
	set	_C_LABEL(cpcb), %o7
	lduw	[%o7], %o7
	ldub	[%o7 + PCB_NSAVED], %o7
	brz,pt	%o7, 2f
	 nop
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	call printf
	 mov	%i7, %o1
	ta	1; nop
	ba	2f
	 restore
1:	.asciz	"datafault: nsaved = %d\n"
	_ALIGN
2:
#endif
	/* Use trap type to see what handler to call */
	cmp	%o0, T_FIMMU_MISS
	st	%g4, [%sp + CCFSZ + TF_Y]		! set tf.tf_y	
	bl	data_error
	 wrpr	%g0, PSTATE_INTR, %pstate		! reenable interrupts
	
	mov	%o5, %o1
	mov	%g2, %o2
	call	_C_LABEL(data_access_fault)		! data_access_fault(type, addr, pc, &tf);
	 add	%sp, CCFSZ, %o3				! (argument: &tf)

	ba	data_recover
	 nop

data_error:
	wrpr	%g0, PSTATE_INTR, %pstate		! reenable interrupts
	call	_C_LABEL(data_access_error)		! data_access_error(type, sfva, sfsr,
							!		afva, afsr, &tf);
	 add	%sp, CCFSZ, %o5				! (argument: &tf)

data_recover:
	CHKPT(%o1,%o2,1)
	wrpr	%g0, PSTATE_KERN, %pstate		! disable interrupts
#if 1
	rdpr	%tl, %g1			! DEBUG Make sure we have one trap level avail
	inc	%g1				! DEBUG
	wrpr	%g0, %g1, %tl			! DEBUG
#endif
#ifdef TRAPSTATS
	set	_C_LABEL(uintrcnt), %g1
	stw	%g0, [%g1]
	set	_C_LABEL(iveccnt), %g1
	stw	%g0, [%g1]
#endif
	b	return_from_trap			! go return
	 ldx	[%sp + CCFSZ + TF_TSTATE], %g1		! Load this for return_from_trap
	NOTREACHED
/*
 * Each memory instruction access fault from a fast access handler comes here.
 * We will quickly check if this is an original prom mapping before going
 * to the generic fault handler
 *
 * We will assume that %pil is not lost so we won't bother to save it
 * unless we're in an interrupt handler.
 *
 * On entry:
 *	We are on one of the alternate set of globals
 *	%g1 = MMU tag target
 *	%g2 = TSB entry ptr
 *	%g3 = TLB Tag Access
 *
 * On return:
 *
 */

	ICACHE_ALIGN
instr_miss:
#ifdef TRAPSTATS
	set	_C_LABEL(ktmiss), %g3
	set	_C_LABEL(utmiss), %g4
	rdpr	%tl, %g6
	dec	%g6
	movrz	%g6, %g4, %g3
	lduw	[%g3], %g4
	inc	%g4
	stw	%g4, [%g3]
#endif
	
#if 1
	mov	TLB_TAG_ACCESS, %g3			! Get real fault page
	ldxa	[%g3] ASI_IMMU, %g3			! from tag access register
 	nop; nop; nop		! Linux sez we need this after reading TAG_ACCESS
#endif
	
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	sllx	%g3, (64-13), %g6			! Mask away address
	lduw	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4
	srlx	%g6, (64-13-2), %g6			! This is now the offset into ctxbusy
	lduw	[%g4+%g6], %g4				! Load up our page table.

#if 1
	/* Make sure we don't try to replace a kernel translation */
	/* This should not be necessary */
	brnz,pt	%g6, Lutext_miss			! If user context continue miss
	sethi	%hi(KERNBASE), %g5			! Don't need %lo
	set	0x0400000, %g6				! 4MB
	sub	%g3, %g5, %g5
	cmp	%g5, %g6
	set	KERNBASE+16, %g7
	mov	6, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
	tlu	%xcc, 1; nop
	blu,pn	%xcc, textfault				! Next insn in delay slot is unimportant
	 mov	7, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
#endif
	
#if 1
	/*
	 * Try to parse our page table. 
	 */
Lutext_miss:
	srlx	%g3, 32, %g6
	brnz,pn	%g6, prom_textfault			! >32 bits? not here
	 srlx	%g3, STSHIFT-2, %g5
	set	8, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
	andn	%g5, 3, %g5
	add	%g5, %g4, %g4
	lduwa	[%g4] ASI_PHYS_CACHED, %g4		! Remember -- UNSIGNED
	brz,pn	%g4, prom_textfault			! NULL entry? check somewhere else
	 srlx	%g3, PTSHIFT, %g5			! Convert to ptab offset
	set	9, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
	and	%g5, PTMASK, %g5
	sll	%g5, 3, %g5
	add	%g5, %g4, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, prom_textfault			! Entry invalid?  Punt
	 bset	TTE_ACCESS, %g4				! Update accessed bit
	stxa	%g4, [%g6] ASI_PHYS_CACHED		!  and store it
	stx	%g1, [%g2]				! Update TSB entry tag
	stx	%g4, [%g2+8]				! Update TSB entry data
	set	trapbase, %g6	! debug
	stx	%g3, [%g6+8]	! debug
	set	0xaa, %g3	! debug
	stx	%g4, [%g6]	! debug -- what we tried to enter in TLB
	stb	%g3, [%g6+0x20]	! debug
	
	sllx	%g3, (64-12), %g6			! Need to demap old entry first
	mov	0x010, %g1				! Secondary flush
	mov	0x020, %g5				! Nucleus flush
	movrz	%g6, %g5, %g1				! Pick one
	andn	%g3, 0xfff, %g6
	or	%g6, %g1, %g6
	stxa	%g6, [%g6] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync					! No real reason for this XXXX
	
	stxa	%g4, [%g0] ASI_IMMU_DATA_IN		! Enter new mapping
	membar	#Sync
	CLRTT
	retry
	NOTREACHED
	!!
	!!  Check our prom mappings -- temporary
	!!
#endif
	
/*
 * Each memory data access fault from a fast access handler comes here.
 * We will quickly check if this is an original prom mapping before going
 * to the generic fault handler
 *
 * We will assume that %pil is not lost so we won't bother to save it
 * unless we're in an interrupt handler.
 *
 * On entry:
 *	We are on one of the alternate set of globals
 *	%g1 = MMU tag target
 *	%g2 = %tl
 *	%g3 = %tl - 1
 *
 * On return:
 *
 */

prom_textfault:
	mov	TLB_TAG_ACCESS, %g3			! Get real fault page
	ldxa	[%g3] ASI_IMMU, %g3			! from tag access register
 	nop; nop; nop		! Linux sez we need this after reading TAG_ACCESS
#if 0
	!!
	!!  Check our prom mappings -- temporary
	!! 
	sll	%g3, (32-13), %g6			! Check context
	brnz,pt	%g6, textfault				! not kernel context -- not prom mapping
	 set	prom_map, %g4
	lduw	[%g4], %g4
!	andn	%g3, 0x0fff, %g5			! save va
	srlx	%g3, 13, %g5				! save va
	sllx	%g5, 13, %g5
2:	
	ldx	[%g4], %g6				! Load entry addr
	subcc	%g5, %g6, %g6				
	bl,a,pt	%xcc, 2b				! No match; next entry
	 inc	(3*8), %g4
	ldx	[%g4+8], %g7				! Load entry size
	cmp	%g7, %g6
	blt,pt	%xcc, textfault				! In range?
	 ldx	[%g4+(2*8)], %g7
	add	%g7, %g6, %g6				! Add in page offset into region
!	stx	%g1, [%g2]				! Update TSB entry tag
!	stx	%g6, [%g2+8]				! Update TSB entry data
	stxa	%g6, [%g0] ASI_IMMU_DATA_IN		! Yes, store and retry
	membar	#Sync
	CLRTT
	retry
	NOTREACHED
#endif
/*
 * Each memory text access fault, from user or kernel mode,
 * comes here. 
 *
 * We will assume that %pil is not lost so we won't bother to save it
 * unless we're in an interrupt handler.
 *
 * On entry:
 *	We are on one of the alternate set of globals
 *	%g1 = MMU tag target
 *	%g2 = %tl
 *	%g3 = %tl - 1
 *
 * On return:
 *
 */


textfault:
	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate	! We need to save volatile stuff to AG regs
#ifdef TRAPS_USE_IG
	wrpr	%g0, PSTATE_KERN|PSTATE_IG, %pstate	! We need to save volatile stuff to AG regs
#endif
	wr	%g0, ASI_IMMU, %asi
	ldxa	[%g0 + TLB_TAG_ACCESS] %asi, %g1	! Get fault address from tag access register
 	nop; nop; nop		! Linux sez we need this after reading TAG_ACCESS
	ldxa	[SFSR] %asi, %g3			! get sync fault status register
	stxa	%g0, [SFSR] %asi			! Clear out old info
	membar	#Sync					! No real reason for this XXXX
	
	TRAP_SETUP(-CCFSZ-TF_SIZE)
#if defined(UVM)
	INCR(_C_LABEL(uvmexp)+V_FAULTS)			! cnt.v_faults++ (clobbers %o0,%o1)
#else
	INCR(_C_LABEL(cnt)+V_FAULTS)			! cnt.v_faults++ (clobbers %o0,%o1)
#endif	

	mov	%g3, %o2

	wrpr	%g0, PSTATE_KERN, %pstate		! Switch to normal globals
	ldxa	[%g0] ASI_AFSR, %o3			! get async fault status
	ldxa	[%g0] ASI_AFAR, %o4			! get async fault address
	mov	-1, %o0
	stxa	%o0, [%g0] ASI_AFSR			! Clear this out
	membar	#Sync					! No real reason for this XXXX
	stx	%g1, [%sp + CCFSZ + TF_G + (1*8)]	! save g1
	stx	%g2, [%sp + CCFSZ + TF_G + (2*8)]	! save g2
	stx	%g3, [%sp + CCFSZ + TF_G + (3*8)]	! (sneak g3 in here)
	rdpr	%tt, %o0				! Find out what caused this trap
	stx	%g4, [%sp + CCFSZ + TF_G + (4*8)]	! sneak in g4
	rdpr	%tstate, %g1
	stx	%g5, [%sp + CCFSZ + TF_G + (5*8)]	! sneak in g5
	rdpr	%tpc, %o1				! sync virt addr; must be read first
	stx	%g6, [%sp + CCFSZ + TF_G + (6*8)]	! sneak in g6
	rdpr	%tnpc, %g3
	stx	%g7, [%sp + CCFSZ + TF_G + (7*8)]	! sneak in g7
	rd	%y, %g7					! save y

	/* Finish stackframe, call C trap handler */
	stx	%g1, [%sp + CCFSZ + TF_TSTATE]		! set tf.tf_psr, tf.tf_pc
	sth	%o0, [%sp + CCFSZ + TF_TT]! debug

	stx	%o1, [%sp + CCFSZ + TF_PC]
	stx	%g3, [%sp + CCFSZ + TF_NPC]		! set tf.tf_npc

	rdpr	%pil, %g5
	stb	%g5, [%sp + CCFSZ + TF_PIL]
	stb	%g5, [%sp + CCFSZ + TF_OLDPIL]
	
#if 1
	rdpr	%tl, %g7
	dec	%g7	
	movrlz	%g7, %g0, %g7
	CHKPT(%g1,%g3,0x22)
	wrpr	%g0, %g7, %tl		! Revert to kernel mode
#endif
	/* Now we need to blast away the D$ to make sure we're in sync */
	set	(2*NBPG)-8, %g7
1:
	stxa	%g0, [%g7] ASI_DCACHE_TAG
	brnz,pt	%g7, 1b
	 dec	8, %g7
	
	/* Use trap type to see what handler to call */
	flushw						! Get rid of any user windows so we don't deadlock
	cmp	%o0, T_FIMMU_MISS
	bl	text_error
	 st	%g7, [%sp + CCFSZ + TF_Y]		! set tf.tf_y
	
	wrpr	%g0, PSTATE_INTR, %pstate	! reenable interrupts
	call	_C_LABEL(text_access_fault)	! mem_access_fault(type, pc, &tf);
	 add	%sp, CCFSZ, %o2			! (argument: &tf)

	ba	text_recover
	 nop
	
text_error:	
	wrpr	%g0, PSTATE_INTR, %pstate	! reenable interrupts
	call	_C_LABEL(text_access_error)	! mem_access_fault(type, sfva [pc], sfsr,
						!		afva, afsr, &tf);
	 add	%sp, CCFSZ, %o5			! (argument: &tf)

text_recover:	
	CHKPT(%o1,%o2,2)
	wrpr	%g0, PSTATE_KERN, %pstate		! disable interrupts
#if 1
	rdpr	%tl, %g1			! DEBUG Make sure we have one trap level avail
	inc	%g1				! DEBUG
	wrpr	%g0, %g1, %tl			! DEBUG
#endif
	b	return_from_trap		! go return
	 ldx	[%sp + CCFSZ + TF_TSTATE], %g1	! Load this for return_from_trap
	NOTREACHED

/*
 * fp_exception has to check to see if we are trying to save
 * the FP state, and if so, continue to save the FP state.
 *
 * We do not even bother checking to see if we were in kernel mode,
 * since users have no access to the special_fp_store instruction.
 *
 * This whole idea was stolen from Sprite.
 */
fp_exception:
	rdpr	%tpc, %g1
	set	special_fp_store, %g4	! see if we came from the special one
	cmp	%g1, %g4		! pc == special_fp_store?
	bne,a	slowtrap		! no, go handle per usual
	 sethi	%hi(savefpcont), %g4	! yes, "return" to the special code
	or	%lo(savefpcont), %g4, %g4
	wrpr	%g0, %g4, %tnpc
	 done
	NOTREACHED

#if 0
/*
 * print out trap then halt then continue
 */
Ltrapped_once:
	.word	0
0:	
	.asciz	"1 utrap_halt: OF_enter failed\r\n"
	_ALIGN
2:	
	.asciz	"2 sfsr=%08x sfar=%08x afsr=%08x afar=%08x TTR=%08x\r\n"
	_ALIGN
3:	
	.asciz	"3 %%tl=%3x %%tt=%3x %%tpc=%08x %%tnpc=%08x from %08x\r\n"
	_ALIGN
4:
	.asciz	"4 %%tstate=%08x:%08x addr=%08x:%08x ctx=%x\r\n"
	_ALIGN
5:
	.asciz	"5 tsb[i]=%08x tag=%08x:%08x data=%08x:%08x\r\n"
	_ALIGN
6:
	.asciz	"6 pmap_kern segtab[%x]=%08x ptab[%x]=%08x:%08x\r\n"
	_ALIGN

/* utrap_panic: */
	!!
	!! Switch to backup stack and then call utrap_halt
	!!
 	set	panicstack - CCFSZ - 80, %g1
	save	%g1, 0, %sp
	mov	%i7, %o7
	
utrap_panic:
	.globl	_C_LABEL(utrap_halt)
_C_LABEL(utrap_halt):	
 	set	panicstack - CCFSZ - 80, %g1
	save	%g1, -CC64FSZ, %sp
!	wrpr	%g0, 4, %tl	! force watchdogs
	set	2b, %o0					! Print out fault info
	ldxa	[%g0] ASI_DMMU, %l0			! Load DMMU tag target register
	set	trapbase, %l2
	mov	TLB_TAG_ACCESS, %l3	! debug
	ldxa	[%l3] ASI_DMMU, %l3	! debug
 	nop; nop; nop		! Linux sez we need this after reading TAG_ACCESS
	ldxa	[%g0] ASI_DMMU_8KPTR, %l1		! Load DMMU 8K TSB pointer
!	ldx	[%l2], %l0

#ifdef DEBUG
	set	Ltrapped_once, %o5
	ld	[%o5], %o0
	mov	-1, %o4
	brnz,pn	%o0, 8f
	 st	%o4, [%o5]
#endif
	
#if 0
	mov	%i1, %o1
	mov	%i2, %o2
	mov	%i3, %o3
	mov	%i4, %o4
	call	prom_printf
	 sllx	%l0, 22, %o5
#endif

1:	
	set	3b, %o0					! Print %tl, %tt, %tpc %tnpc
	rdpr	%tl, %o1
	rdpr	%tt, %o2
	rdpr	%tpc, %o3
	rdpr	%tnpc, %o4
	call	prom_printf
	 mov	%i7, %o5

!	ba	OF_halt		! skip all this other stuff
	
	set	4b, %o0					! Print %tstate, trap addr, trap ctx
	rdpr	%tstate, %o1
	srl	%o1, 0, %o2
	srlx	%o1, 32, %o1
	mov	%l3, %o4
	srl	%o4, 32, %o3
	call	prom_printf
	 srlx	%l0, 48, %o5

	rdpr	%tl, %o0				! Iterate over all traps
	dec	%o0
	CLRTT
	CHKPT(%o1,%o2,0x23)
	brnz,pt	%o0, 1b
	 wrpr	%o0, 0, %tl

	ta	1; nop		! Debugger!
#if 0	
	set	5b, %o0					! Print tsb[i] data and tag
	ldx	[%l1], %o2
	mov	%l1, %o1
	srl	%o2, 0, %o3
	srlx	%o2, 32, %o2
	ldx	[%l1+8], %o4
	srl	%o4, 0, %o5
	call	prom_printf
	 srlx	%o4, 32, %o4
#endif
#if 0
	set	Ltrapped_once, %o5
	ld	[%o5], %o0
	mov	-1, %o4
	brnz,pn	%o0, 8f
	 st	%o4, [%o5]
	clr	%o1					! Make sure we have known values in these regs
	clr	%o2
	clr	%o3
	clr	%o4
	clr	%o5

	
	srlx	%l0, 48, %o2				! Wrong context?
	brnz,pn	%o2, 8f					! Skip entire thing if needed
	 set	_C_LABEL(kernel_pmap_)+PM_SEGS, %o3
!	andn	%l3, 0x0fff, %o1			! stab[%o1]
	srlx	%l3, 13, %o1				! stab[%o1]
	sllx	%o1, 13, %o1
	srlx	%o1, 32, %o2
	brnz,pn	%o2, 8f					! >32 bits? not here
	 srlx	%o1, STSHIFT, %o1
	sllx	%o1, 2, %o2				! %o2=&stab[%o1]
	lduw	[%o3+%o2], %o2				! pte
	srlx	%l3, PTSHIFT, %o3
	brz,pn	%o2, 7f
	 and	%o3, PTMASK, %o3
	sll	%o3, 3, %o5
	ldx	[%o2+%o5], %o5
7:	
	set	6b, %o0					! Print pmap segs & stuff
	call	prom_printf
 	 sllx	%o5, 32, %o4

#endif
8:
	.globl	_C_LABEL(OF_halt)
_C_LABEL(OF_halt):	
	call	_C_LABEL(OF_enter)			! Jump to prom
	 wrpr	%g0, 0, %tl				! Get out of nucleus mode

#if 0
	set	0b, %o0
	call	prom_printf
	 nop
#endif
	
	ret
	 restore
#if 0
	set	romitsbp, %g2				! Restore PROM trap state
	ldx	[%g2], %g5				! Restore TSB pointers
	set	TSB, %g3
	stxa	%g5, [%g3] ASI_IMMU
	membar	#Sync
	set	romdtsbp, %g2
	ldx	[%g2], %g5
	stxa	%g5, [%g3] ASI_DMMU
	membar	#Sync
	
	set	romtrapbase, %g3			! Restore trapbase
	ldx	[%g3], %g5
	wrpr	%g5, 0, %tba
	set	romwstate, %g3				! Restore wstate
	ldx	[%g3], %g5
	call	OF_enter				! Jump to prom
	 wrpr	%g5, 0, %wstate
#endif	
	NOTREACHED

#endif
/*
 * slowtrap() builds a trap frame and calls trap().
 * This is called `slowtrap' because it *is*....
 * We have to build a full frame for ptrace(), for instance.
 *
 * Registers:
 *	
 */
slowtrap:
#ifdef TRAPS_USE_IG
	wrpr	%g0, PSTATE_KERN|PSTATE_IG, %pstate	! DEBUG
#endif
	rdpr	%tt, %g4
	rdpr	%tstate, %g1
	rdpr	%tpc, %g2
	rdpr	%tnpc, %g3

#ifdef TRAPWIN
	TRAP_SETUP(-CCFSZ-TF_SIZE)
Lslowtrap_reenter:	
#else
Lslowtrap_reenter:	
	TRAP_SETUP(-CCFSZ-TF_SIZE)
#endif
	stx	%g1, [%sp + CCFSZ + TF_TSTATE]
	mov	%g4, %o0		! (type)
	stx	%g2, [%sp + CCFSZ + TF_PC]
	rd	%y, %g5
	stx	%g3, [%sp + CCFSZ + TF_NPC]
	mov	%g1, %o1		! (pstate)
	st	%g5, [%sp + CCFSZ + TF_Y]
	mov	%g2, %o2		! (pc)
	sth	%o0, [%sp + CCFSZ + TF_TT]! debug
		
	wrpr	%g0, PSTATE_KERN, %pstate		! Get back to normal globals	
	stx	%g1, [%sp + CCFSZ + TF_G + (1*8)]
	stx	%g2, [%sp + CCFSZ + TF_G + (2*8)]
	add	%sp, CCFSZ, %o3		! (&tf)
	stx	%g3, [%sp + CCFSZ + TF_G + (3*8)]
	stx	%g4, [%sp + CCFSZ + TF_G + (4*8)]
	stx	%g5, [%sp + CCFSZ + TF_G + (5*8)]
	stx	%g6, [%sp + CCFSZ + TF_G + (6*8)]
	stx	%g7, [%sp + CCFSZ + TF_G + (7*8)]
	rdpr	%pil, %g5
	stb	%g5, [%sp + CCFSZ + TF_PIL]
	stb	%g5, [%sp + CCFSZ + TF_OLDPIL]
	/*
	 * Phew, ready to enable traps and call C code.
	 */
#if 1
	rdpr	%tl, %g1
	dec	%g1
	movrlz	%g1, %g0, %g1
	CHKPT(%g2,%g3,0x24)
	wrpr	%g0, %g1, %tl		! Revert to kernel mode
#endif
!	flushw			! DEBUG
	call	_C_LABEL(trap)			! trap(type, pstate, pc, &tf)
	 wrpr	%g0, PSTATE_INTR, %pstate	! traps on again

!	wrpr	%g0, PSTATE_KERN, %pstate	! traps off again
#if 1
	rdpr	%tl, %g1			! DEBUG Make sure we have one trap level avail
	inc	%g1				! DEBUG
	CHKPT(%g2,%g3,0x25)
	wrpr	%g0, %g1, %tl			! DEBUG
#endif
	CLRTT					! Clear out old handled trap
	CHKPT(%o1,%o2,3)
	b	return_from_trap
	 nop
	NOTREACHED
#if 1
/*
 * This code is no longer needed.
 */
/*
 * Do a `software' trap by re-entering the trap code, possibly first
 * switching from interrupt stack to kernel stack.  This is used for
 * scheduling and signal ASTs (which generally occur from softclock or
 * tty or net interrupts).
 *
 * We enter with the trap type in %g1.  All we have to do is jump to
 * Lslowtrap_reenter above, but maybe after switching stacks....
 *
 * We should be running alternate globals.  The normal globals and
 * out registers were just loaded from the old trap frame.  
 *
 *	Input Params:
 *	%g1 = tstate
 *	%g2 = tpc
 *	%g3 = tnpc
 *	%g4 = tt == T_AST
 */
softtrap:
	sethi	%hi(_C_LABEL(eintstack)), %g7
	or	%g7, %lo(_C_LABEL(eintstack)), %g7
	cmp	%g6, %g7
	bgeu,pt	%icc, Lslowtrap_reenter
	 nop
	sethi	%hi(_C_LABEL(cpcb)), %g7
	lduw	[%g7 + %lo(_C_LABEL(cpcb))], %g7
	set	USPACE-CCFSZ-TF_SIZE, %g5
	add	%g7, %g5, %g6
	SET_SP_REDZONE(%g7, %g5)
	stx	%g1, [%g6 + CCFSZ + TF_FAULT]		! Generate a new trapframe 
	stx	%i0, [%g6 + CCFSZ + TF_O + (0*8)]	!	but don't bother with
	stx	%i1, [%g6 + CCFSZ + TF_O + (1*8)]	!	locals and ins
	stx	%i2, [%g6 + CCFSZ + TF_O + (2*8)]
	stx	%i3, [%g6 + CCFSZ + TF_O + (3*8)]
	stx	%i4, [%g6 + CCFSZ + TF_O + (4*8)]
	stx	%i5, [%g6 + CCFSZ + TF_O + (5*8)]
	stx	%i6, [%g6 + CCFSZ + TF_O + (6*8)]
	stx	%i7, [%g6 + CCFSZ + TF_O + (7*8)]
#ifdef DEBUG
	ldx	[%sp + CCFSZ + TF_I + (0*8)], %l0	! Copy over the rest of the regs
	ldx	[%sp + CCFSZ + TF_I + (1*8)], %l1	! But just dirty the locals
	ldx	[%sp + CCFSZ + TF_I + (2*8)], %l2
	ldx	[%sp + CCFSZ + TF_I + (3*8)], %l3
	ldx	[%sp + CCFSZ + TF_I + (4*8)], %l4
	ldx	[%sp + CCFSZ + TF_I + (5*8)], %l5
	ldx	[%sp + CCFSZ + TF_I + (6*8)], %l6
	ldx	[%sp + CCFSZ + TF_I + (7*8)], %l7
	stx	%l0, [%g6 + CCFSZ + TF_I + (0*8)]
	stx	%l1, [%g6 + CCFSZ + TF_I + (1*8)]
	stx	%l2, [%g6 + CCFSZ + TF_I + (2*8)]
	stx	%l3, [%g6 + CCFSZ + TF_I + (3*8)]
	stx	%l4, [%g6 + CCFSZ + TF_I + (4*8)]
	stx	%l5, [%g6 + CCFSZ + TF_I + (5*8)]
	stx	%l6, [%g6 + CCFSZ + TF_I + (6*8)]
	stx	%l7, [%g6 + CCFSZ + TF_I + (7*8)]
	ldx	[%sp + CCFSZ + TF_L + (0*8)], %l0
	ldx	[%sp + CCFSZ + TF_L + (1*8)], %l1
	ldx	[%sp + CCFSZ + TF_L + (2*8)], %l2
	ldx	[%sp + CCFSZ + TF_L + (3*8)], %l3
	ldx	[%sp + CCFSZ + TF_L + (4*8)], %l4
	ldx	[%sp + CCFSZ + TF_L + (5*8)], %l5
	ldx	[%sp + CCFSZ + TF_L + (6*8)], %l6
	ldx	[%sp + CCFSZ + TF_L + (7*8)], %l7
	stx	%l0, [%g6 + CCFSZ + TF_L + (0*8)]
	stx	%l1, [%g6 + CCFSZ + TF_L + (1*8)]	
	stx	%l2, [%g6 + CCFSZ + TF_L + (2*8)]
	stx	%l3, [%g6 + CCFSZ + TF_L + (3*8)]
	stx	%l4, [%g6 + CCFSZ + TF_L + (4*8)]
	stx	%l5, [%g6 + CCFSZ + TF_L + (5*8)]
	stx	%l6, [%g6 + CCFSZ + TF_L + (6*8)]
	stx	%l7, [%g6 + CCFSZ + TF_L + (7*8)]
#endif
	ba,pt	%xcc, Lslowtrap_reenter
	 mov	%g6, %sp
#endif

#if 0
/*
 * breakpoint:	capture as much info as possible and then call DDB
 * or trap, as the case may be.
 *
 * First, we switch to interrupt globals, and blow away %g7.  Then
 * switch down one stackframe -- just fiddle w/cwp, don't save or
 * we'll trap.  Then slowly save all the globals into our static
 * register buffer.  etc. etc.
 */

breakpoint:
	wrpr	%g0, PSTATE_KERN|PSTATE_IG, %pstate	! Get IG to use
	rdpr	%cwp, %g7
	inc	1, %g7					! Equivalent of save
	wrpr	%g7, 0, %cwp				! Now we have some unused locals to fiddle with
	set	_C_LABEL(ddb_regs), %l0
	stx	%g1, [%l0+DBR_IG+(1*8)]			! Save IGs
	stx	%g2, [%l0+DBR_IG+(2*8)]
	stx	%g3, [%l0+DBR_IG+(3*8)]
	stx	%g4, [%l0+DBR_IG+(4*8)]
	stx	%g5, [%l0+DBR_IG+(5*8)]
	stx	%g6, [%l0+DBR_IG+(6*8)]
	stx	%g7, [%l0+DBR_IG+(7*8)]
	wrpr	%g0, PSTATE_KERN|PSTATE_MG, %pstate	! Get MG to use
	stx	%g1, [%l0+DBR_MG+(1*8)]			! Save MGs
	stx	%g2, [%l0+DBR_MG+(2*8)]
	stx	%g3, [%l0+DBR_MG+(3*8)]
	stx	%g4, [%l0+DBR_MG+(4*8)]
	stx	%g5, [%l0+DBR_MG+(5*8)]
	stx	%g6, [%l0+DBR_MG+(6*8)]
	stx	%g7, [%l0+DBR_MG+(7*8)]
	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate	! Get AG to use
	stx	%g1, [%l0+DBR_AG+(1*8)]			! Save AGs
	stx	%g2, [%l0+DBR_AG+(2*8)]
	stx	%g3, [%l0+DBR_AG+(3*8)]
	stx	%g4, [%l0+DBR_AG+(4*8)]
	stx	%g5, [%l0+DBR_AG+(5*8)]
	stx	%g6, [%l0+DBR_AG+(6*8)]
	stx	%g7, [%l0+DBR_AG+(7*8)]
	wrpr	%g0, PSTATE_KERN, %pstate	! Get G to use
	stx	%g1, [%l0+DBR_G+(1*8)]			! Save Gs
	stx	%g2, [%l0+DBR_G+(2*8)]
	stx	%g3, [%l0+DBR_G+(3*8)]
	stx	%g4, [%l0+DBR_G+(4*8)]
	stx	%g5, [%l0+DBR_G+(5*8)]
	stx	%g6, [%l0+DBR_G+(6*8)]
	stx	%g7, [%l0+DBR_G+(7*8)]
	rdpr	%canrestore, %l1
	stb	%l1, [%l0+DBR_CANRESTORE]
	rdpr	%cansave, %l2
	stb	%l2, [%l0+DBR_CANSAVE]
	rdpr	%cleanwin, %l3
	stb	%l3, [%l0+DBR_CLEANWIN]
	rdpr	%wstate, %l4
	stb	%l4, [%l0+DBR_WSTATE]
	rd	%y, %l5
	stw	%l5, [%l0+DBR_Y]
	rdpr	%tl, %l6
	stb	%l6, [%l0+DBR_TL]
	dec	1, %g7
#endif
	
/*
 * I will not touch any of the DDB or KGDB stuff until I know what's going
 * on with the symbol table.  This is all still v7/v8 code and needs to be fixed.
 */	
#ifdef KGDB
/*
 * bpt is entered on all breakpoint traps.
 * If this is a kernel breakpoint, we do not want to call trap().
 * Among other reasons, this way we can set breakpoints in trap().
 */
bpt:
	set	TSTATE_PRIV, %l4
	andcc	%l4, %l0, %g0		! breakpoint from kernel?
	bz	slowtrap		! no, go do regular trap
	 nop

	/*
	 * Build a trap frame for kgdb_trap_glue to copy.
	 * Enable traps but set ipl high so that we will not
	 * see interrupts from within breakpoints.
	 */
	save	%sp, -CCFSZ-TF_SIZE, %sp		! allocate a trap frame
	TRAP_SETUP(-CCFSZ-TF_SIZE)
	or	%l0, PSR_PIL, %l4	! splhigh()
	wr	%l4, 0, %psr		! the manual claims that this
	wr	%l4, PSR_ET, %psr	! song and dance is necessary
	std	%l0, [%sp + CCFSZ + 0]	! tf.tf_psr, tf.tf_pc
	mov	%l3, %o0		! trap type arg for kgdb_trap_glue
	rd	%y, %l3
	std	%l2, [%sp + CCFSZ + 8]	! tf.tf_npc, tf.tf_y
	rd	%wim, %l3
	st	%l3, [%sp + CCFSZ + 16]	! tf.tf_wim (a kgdb-only r/o field)
	st	%g1, [%sp + CCFSZ + 20]	! tf.tf_global[1]
	std	%g2, [%sp + CCFSZ + 24]	! etc
	std	%g4, [%sp + CCFSZ + 32]
	std	%g6, [%sp + CCFSZ + 40]
	std	%i0, [%sp + CCFSZ + 48]	! tf.tf_in[0..1]
	std	%i2, [%sp + CCFSZ + 56]	! etc
	std	%i4, [%sp + CCFSZ + 64]
	std	%i6, [%sp + CCFSZ + 72]

	/*
	 * Now call kgdb_trap_glue(); if it returns, call trap().
	 */
	mov	%o0, %l3		! gotta save trap type
	call	_C_LABEL(kgdb_trap_glue)		! kgdb_trap_glue(type, &trapframe)
	 add	%sp, CCFSZ, %o1		! (&trapframe)

	/*
	 * Use slowtrap to call trap---but first erase our tracks
	 * (put the registers back the way they were).
	 */
	mov	%l3, %o0		! slowtrap will need trap type
	ld	[%sp + CCFSZ + 12], %l3
	wr	%l3, 0, %y
	ld	[%sp + CCFSZ + 20], %g1
	ldd	[%sp + CCFSZ + 24], %g2
	ldd	[%sp + CCFSZ + 32], %g4
	b	Lslowtrap_reenter
	 ldd	[%sp + CCFSZ + 40], %g6

/*
 * Enter kernel breakpoint.  Write all the windows (not including the
 * current window) into the stack, so that backtrace works.  Copy the
 * supplied trap frame to the kgdb stack and switch stacks.
 *
 * kgdb_trap_glue(type, tf0)
 *	int type;
 *	struct trapframe *tf0;
 */
	.globl	_C_LABEL(kgdb_trap_glue)
_C_LABEL(kgdb_trap_glue):
	save	%sp, -CCFSZ, %sp

	flushw				! flush all windows
	mov	%sp, %l4		! %l4 = current %sp

	/* copy trapframe to top of kgdb stack */
	set	_C_LABEL(kgdb_stack) + KGDB_STACK_SIZE - 80, %l0
					! %l0 = tfcopy -> end_of_kgdb_stack
	mov	80, %l1
1:	ldd	[%i1], %l2
	inc	8, %i1
	deccc	8, %l1
	std	%l2, [%l0]
	bg	1b
	 inc	8, %l0

#ifdef DEBUG
	/* save old red zone and then turn it off */
	sethi	%hi(_C_LABEL(redzone)), %l7
	ld	[%l7 + %lo(_C_LABEL(redzone))], %l6
	st	%g0, [%l7 + %lo(_C_LABEL(redzone))]
#endif
	/* switch to kgdb stack */
	add	%l0, -CCFSZ-TF_SIZE, %sp

	/* if (kgdb_trap(type, tfcopy)) kgdb_rett(tfcopy); */
	mov	%i0, %o0
	call	_C_LABEL(kgdb_trap)
	add	%l0, -80, %o1
	tst	%o0
	bnz,a	kgdb_rett
	 add	%l0, -80, %g1

	/*
	 * kgdb_trap() did not handle the trap at all so the stack is
	 * still intact.  A simple `restore' will put everything back,
	 * after we reset the stack pointer.
	 */
	mov	%l4, %sp
#ifdef DEBUG
	st	%l6, [%l7 + %lo(_C_LABEL(redzone))]	! restore red zone
#endif
	ret
	 restore

/*
 * Return from kgdb trap.  This is sort of special.
 *
 * We know that kgdb_trap_glue wrote the window above it, so that we will
 * be able to (and are sure to have to) load it up.  We also know that we
 * came from kernel land and can assume that the %fp (%i6) we load here
 * is proper.  We must also be sure not to lower ipl (it is at splhigh())
 * until we have traps disabled, due to the SPARC taking traps at the
 * new ipl before noticing that PSR_ET has been turned off.  We are on
 * the kgdb stack, so this could be disastrous.
 *
 * Note that the trapframe argument in %g1 points into the current stack
 * frame (current window).  We abandon this window when we move %g1->tf_psr
 * into %psr, but we will not have loaded the new %sp yet, so again traps
 * must be disabled.
 */
kgdb_rett:
	rd	%psr, %g4		! turn off traps
	wr	%g4, PSR_ET, %psr
	/* use the three-instruction delay to do something useful */
	ld	[%g1], %g2		! pick up new %psr
	ld	[%g1 + 12], %g3		! set %y
	wr	%g3, 0, %y
#ifdef DEBUG
	st	%l6, [%l7 + %lo(_C_LABEL(redzone))] ! and restore red zone
#endif
	wr	%g0, 0, %wim		! enable window changes
	nop; nop; nop
	/* now safe to set the new psr (changes CWP, leaves traps disabled) */
	wr	%g2, 0, %psr		! set rett psr (including cond codes)
	/* 3 instruction delay before we can use the new window */
/*1*/	ldd	[%g1 + 24], %g2		! set new %g2, %g3
/*2*/	ldd	[%g1 + 32], %g4		! set new %g4, %g5
/*3*/	ldd	[%g1 + 40], %g6		! set new %g6, %g7

	/* now we can use the new window */
	mov	%g1, %l4
	ld	[%l4 + 4], %l1		! get new pc
	ld	[%l4 + 8], %l2		! get new npc
	ld	[%l4 + 20], %g1		! set new %g1

	/* set up returnee's out registers, including its %sp */
	ldd	[%l4 + 48], %i0
	ldd	[%l4 + 56], %i2
	ldd	[%l4 + 64], %i4
	ldd	[%l4 + 72], %i6

	/* load returnee's window, making the window above it be invalid */
	restore
	restore	%g0, 1, %l1		! move to inval window and set %l1 = 1
	rd	%psr, %l0
	srl	%l1, %l0, %l1
	wr	%l1, 0, %wim		! %wim = 1 << (%psr & 31)
	sethi	%hi(_C_LABEL(cpcb)), %l1
	ld	[%l1 + %lo(_C_LABEL(cpcb))], %l1
	and	%l0, 31, %l0		! CWP = %psr & 31;
	st	%l0, [%l1 + PCB_WIM]	! cpcb->pcb_wim = CWP;
	save	%g0, %g0, %g0		! back to window to reload
	LOADWIN(%sp)
	save	%g0, %g0, %g0		! back to trap window
	/* note, we have not altered condition codes; safe to just rett */
	RETT
#endif

/*
 * syscall_setup() builds a trap frame and calls syscall().
 * sun_syscall is same but delivers sun system call number
 * XXX	should not have to save&reload ALL the registers just for
 *	ptrace...
 */
syscall_setup:
#ifdef TRAPS_USE_IG
	wrpr	%g0, PSTATE_KERN|PSTATE_IG, %pstate	! DEBUG
#endif
	TRAP_SETUP(-CCFSZ-TF_SIZE)

	rdpr	%tt, %o0	! debug
	sth	%o0, [%sp + CCFSZ + TF_TT]! debug
		
	wrpr	%g0, PSTATE_KERN, %pstate		! Get back to normal globals
	stx	%g1, [%sp + CCFSZ + TF_G + ( 1*8)]
	mov	%g1, %o0				! code
	rdpr	%tpc, %o2				! (pc)
	stx	%g2, [%sp + CCFSZ + TF_G + ( 2*8)]	
	rdpr	%tstate, %g1
	stx	%g3, [%sp + CCFSZ + TF_G + ( 3*8)]	
	rdpr	%tnpc, %g3
	stx	%g4, [%sp + CCFSZ + TF_G + ( 4*8)]	
	rd	%y, %g4
	stx	%g5, [%sp + CCFSZ + TF_G + ( 5*8)]	
	stx	%g6, [%sp + CCFSZ + TF_G + ( 6*8)]	
	CHKPT(%g5,%g6,0x31)
	wrpr	%g0, 0, %tl			! return to tl=0
	stx	%g7, [%sp + CCFSZ + TF_G + ( 7*8)]
	add	%sp, CCFSZ, %o1				! (&tf)
	
	stx	%g1, [%sp + CCFSZ + TF_TSTATE]	
	stx	%o2, [%sp + CCFSZ + TF_PC]
	stx	%g3, [%sp + CCFSZ + TF_NPC]
	st	%g4, [%sp + CCFSZ + TF_Y]

	rdpr	%pil, %g5
	stb	%g5, [%sp + CCFSZ + TF_PIL]
	stb	%g5, [%sp + CCFSZ + TF_OLDPIL]
	
!	flushw			! DEBUG
!	ldx	[%sp + CCFSZ + TF_G + ( 1*8)], %o0	! (code)
	call	_C_LABEL(syscall)		! syscall(code, &tf, pc, suncompat)
	 wrpr	%g0, PSTATE_INTR, %pstate	! turn on interrupts

	/* see `proc_trampoline' for the reason for this label */
return_from_syscall:
	wrpr	%g0, PSTATE_KERN, %pstate		! Disable intterrupts
	CHKPT(%o1,%o2,0x32)
	wrpr	%g0, 1, %tl			! Return to tl==1
	CLRTT
	CHKPT(%o1,%o2,4)
	b	return_from_trap
	 nop
	NOTREACHED

/*
 * interrupt_vector:	
 *
 * Spitfire chips never get level interrupts directly from H/W.
 * Instead, all interrupts come in as interrupt_vector traps.
 * The interrupt number or handler address is an 11 bit number 
 * encoded in the first interrupt data word.  Additional words 
 * are application specific and used primarily for cross-calls.
 *
 * The interrupt vector handler then needs to identify the
 * interrupt source from the interrupt number and arrange to
 * invoke the interrupt handler.  This can either be done directly
 * from here, or a softint at a particular level can be issued.
 *
 * To call an interrupt directly and not overflow the trap stack,
 * the trap registers should be saved on the stack, registers 
 * cleaned, trap-level decremented, the handler called, and then
 * the process must be reversed.
 *
 * To simplify life all we do here is issue an appropriate sofint.
 *
 * Note:	It is impossible to identify or change a device's 
 *		interrupt number until it is probed.  That's the
 *		purpose for all the funny interrupt acknowledge
 *		code.
 *
 */
#define INTRDEBUG_VECTOR	0x1
#define INTRDEBUG_LEVEL		0x2	
#define INTRDEBUG_FUNC		0x4
#define INTRDEBUG_SPUR		0x8
intrdebug:	.word 0
interrupt_vector:
#ifdef TRAPSTATS
	set	_C_LABEL(kiveccnt), %g1
	set	_C_LABEL(iveccnt), %g2
	rdpr	%tl, %g3
	dec	%g3
	movrz	%g3, %g2, %g1
	lduw	[%g1], %g2
	inc	%g2
	stw	%g2, [%g1]
#endif
	ldxa	[%g0] ASI_IRSR, %g1
	mov	IRDR_0H, %g2
	ldxa	[%g2] ASI_IRDR, %g2	! Get interrupt number
	btst	IRSR_BUSY, %g1
	set	intrlev, %g3
	bz,pn	%icc, 3f		! spurious interrupt
	 cmp	%g2, MAXINTNUM
	bgeu	iv_halt			! 3f
	 sllx	%g2, 2, %g5		! Calculate entry number -- 32-bit offset
	lduw	[%g3+%g5], %g5		! We have a pointer to the handler
	brz,pn	%g5, iv_halt	/*3f*/	! NULL means it isn't registered yet.  Skip it.
	 nop
	lduh	[%g5+IH_PIL], %g6	! Read interrupt mask
#ifdef NOT_DEBUG
	set	intrdebug, %g7
	ld	[%g7], %g7
	btst	INTRDEBUG_VECTOR, %g7
	bz,pt	%icc, 1f
	 nop
	TO_STACK32(-CC64FSZ)		! Get a clean register window
	set	4f, %o0
	mov	%g2, %o1
	rdpr	%pil, %o3
	GLOBTOLOC
	call	prom_printf
	 mov	%g6, %o2
	LOCTOGLOB
	ba	1f
	 restore
#endif
1:
	stxa	%g0, [%g0] ASI_IRSR	! Ack IRQ
	membar	#Sync			! Should not be needed due to retry
	wr	%g6, 0, SET_SOFTINT	! Invoke a softint
2:	
	CLRTT
	retry
	NOTREACHED

3:
#ifdef DEBUG
	set	intrdebug, %g7
	ld	[%g7], %g7
	btst	INTRDEBUG_SPUR, %g7
	bz,pt	%icc, 2b
	 nop
	TO_STACK32(-CC64FSZ)		! Get a clean register window
	set	5f, %o0
	mov	%g1, %o1
	GLOBTOLOC
	call	prom_printf
	 rdpr	%pil, %o2
	LOCTOGLOB
	ba	2b
	 restore
#endif
	ba	2b
	 nop
	
iv_halt:
	sir
	ta	1; nop
	save	%sp, -CC64FSZ, %sp	! Get a clean register window
	set	panicstr, %o3
	sllx	%g2, 32, %o1
	set	2f, %o0
	srl	%g2, 0, %o2
	wrpr	%g0, PSTATE_KERN, %pstate! Switch to normal globals
	call	prom_printf
	 st	%o0, [%o3]
	CHKPT(%o1,%o2,0x33)
	call	OF_enter
	 wrpr	%g0, 0, %tl
	ba	1b
	 restore
2:
	.asciz	"interrupt_vector: received %08x:%08x\r\n"
4:	.asciz	"interrupt_vector: number %x softint mask %x pil %d\r\n"
5:	.asciz	"interrupt_vector: spurious vector %x at pil %d\r\n"
	_ALIGN
3:
	
/*
 * Ultra1 and Ultra2 CPUs use soft interrupts for everything.  What we do
 * on a soft interrupt, is we should check which bits in ASR_SOFTINT(0x16)
 * are set, handle those interrupts, then clear them by setting the
 * appropriate bits in ASR_CLEAR_SOFTINT(0x15).
 *
 * After preliminary setup work, the interrupt is passed to each
 * registered handler in turn.  These are expected to return nonzero if
 * they took care of the interrupt.  If a handler claims the interrupt,
 * we exit (hardware interrupts are latched in the requestor so we'll
 * just take another interrupt in the unlikely event of simultaneous
 * interrupts from two different devices at the same level).  If we go
 * through all the registered handlers and no one claims it, we report a
 * stray interrupt.  This is more or less done as:
 *
 *	for (ih = intrhand[intlev]; ih; ih = ih->ih_next)
 *		if ((*ih->ih_fun)(ih->ih_arg ? ih->ih_arg : &frame))
 *			return;
 *	strayintr(&frame);
 *
 * Inputs:
 *	%l0 = %tstate
 *	%l1 = return pc
 *	%l2 = return npc
 *	%l3 = interrupt level
 *	(software interrupt only) %l4 = bits to clear in interrupt register
 *
 * Internal:
 *	%l4, %l5: local variables
 *	%l6 = %y
 *	%l7 = %g1
 *	%g2..%g7 go to stack
 *
 * An interrupt frame is built in the space for a full trapframe;
 * this contains the psr, pc, npc, and interrupt level.
 *
 * The level of this interrupt is determined by:
 *
 *       IRQ# = %tt - 0x40
 */
	.comm	_C_LABEL(intrhand), 15 * 4	! intrhand[0..14]; 0 => error
	.globl _C_LABEL(sparc_interrupt)		! This is for interrupt debugging
_C_LABEL(sparc_interrupt):
#ifdef TRAPS_USE_IG
	wrpr	%g0, PSTATE_KERN|PSTATE_IG, %pstate	! DEBUG
#endif
#ifdef TRAPSTATS
	set	_C_LABEL(kintrcnt), %g1
	set	_C_LABEL(uintrcnt), %g2
	rdpr	%tl, %g3
	dec	%g3
	movrz	%g3, %g2, %g1
	lduw	[%g1], %g2
	inc	%g2
	stw	%g2, [%g1]
	/* See if we're on the interrupt stack already. */
	set	_C_LABEL(eintstack), %g2
	set	(_C_LABEL(eintstack)-_C_LABEL(intstack)), %g1
	btst	1, %sp
	add	%sp, BIAS, %g3
	movz	%icc, %sp, %g3
	srl	%g3, 0, %g3
	sub	%g2, %g3, %g3
	cmp	%g3, %g1
	bgu	1f
	 set	_C_LABEL(intristk), %g1
	lduw	[%g1], %g2
	inc	%g2
	stw	%g2, [%g1]	
1:	
#endif
	INTR_SETUP(-CCFSZ-TF_SIZE)
	wrpr	%g0, PSTATE_KERN, %pstate		! Switch to normal globals so we can save them
	stx	%g1, [%sp + CCFSZ + TF_G + ( 1*8)]
	stx	%g2, [%sp + CCFSZ + TF_G + ( 2*8)]
	stx	%g3, [%sp + CCFSZ + TF_G + ( 3*8)]
	stx	%g4, [%sp + CCFSZ + TF_G + ( 4*8)]
	stx	%g5, [%sp + CCFSZ + TF_G + ( 5*8)]
	stx	%g6, [%sp + CCFSZ + TF_G + ( 6*8)]
	stx	%g7, [%sp + CCFSZ + TF_G + ( 7*8)]
	
	flushw			! DEBUG
	rd	%y, %l6
#if defined(UVM)
	INCR(_C_LABEL(uvmexp)+V_INTR)		! cnt.v_intr++; (clobbers %o0,%o1)
#else
	INCR(_C_LABEL(cnt)+V_INTR)		! cnt.v_intr++; (clobbers %o0,%o1)
#endif	
	rdpr	%tt, %l5			! Find out our current IPL
	rdpr	%tstate, %l0
	rdpr	%tpc, %l1
	rdpr	%tnpc, %l2
	rdpr	%tl, %l3			! Dump our trap frame now we have taken the IRQ
	dec	%l3
	CHKPT(%l4,%l7,0x26)
	wrpr	%g0, %l3, %tl
	sth	%l5, [%sp + CCFSZ + TF_TT]! debug
	stx	%l0, [%sp + CCFSZ + TF_TSTATE]	! set up intrframe/clockframe
	stx	%l1, [%sp + CCFSZ + TF_PC]
	stx	%l2, [%sp + CCFSZ + TF_NPC]
	stx	%fp, [%sp + CCFSZ + TF_KSTACK]	!  old frame pointer
	
	sub	%l5, 0x40, %l5			! Convert to interrupt level
	mov	1, %l3				! Ack softint
	sll	%l3, %l5, %l3			! Generate IRQ mask
	wr	%l3, 1, CLEAR_SOFTINT		! (also clear possible %tick IRQ)
!	wr	%l3, 0, CLEAR_SOFTINT		! (don't clear possible %tick IRQ)

	set	_C_LABEL(intrcnt), %l4		! intrcnt[intlev]++;
	stb	%l5, [%sp + CCFSZ + TF_PIL]	! set up intrframe/clockframe
	rdpr	%pil, %o1
	sll	%l5, 2, %l3
	stb	%o1, [%sp + CCFSZ + TF_OLDPIL]	! old %pil
	ld	[%l4 + %l3], %o0
	inc	%o0
	st	%o0, [%l4 + %l3]
	wrpr	%l5, %pil
	set	_C_LABEL(intrhand), %l4		! %l4 = intrhand[intlev];
	ld	[%l4 + %l3], %l4
	wrpr	%g0, PSTATE_INTR, %pstate	! Reenable interrupts
	clr	%l3
#ifdef DEBUG
	set	trapdebug, %o2
	ld	[%o2], %o2
	btst	0x80, %o2			! (trapdebug & TDB_TL) ?
	bz	1f
	rdpr	%tl, %o2			! Trap if we're not at TL=0 since that's an error condition
	tst	%o2
	tnz	%icc, 1; nop
1:	
	set	_C_LABEL(eintstack), %o2
	cmp	%sp, %o2
	bleu	0f
	 set	7f, %o0
	call	prom_printf
	 mov	%sp, %o1
	ta	1; nop
0:	
	set	intrdebug, %o0			! Check intrdebug
	ld	[%o0], %o0
	btst	INTRDEBUG_LEVEL, %o0
	bz,a,pt	%icc, 3f
	 clr	%l5
	set	8f, %o0
	call	prom_printf
	 mov	%l5, %o1
#endif
	b	3f
	 clr	%l5
#ifdef DEBUG
7:	.asciz	"sparc_interrupt: stack %p eintstack %p\r\n"
8:	.asciz	"sparc_interrupt: got lev %d\r\n"
9:	.asciz	"sparc_interrupt:            calling %x:%x(%x:%x) sp = %p\r\n"
	_ALIGN
#endif	

1:	ld	[%l4 + IH_FUN], %o1	! do {
	ld	[%l4 + IH_ARG], %o0
#ifdef DEBUG
	set	intrdebug, %o2
	ld	[%o2], %o2
	btst	INTRDEBUG_FUNC, %o2
	bz,a,pt	%icc, 7f
	 nop
	
	save	%sp, -CC64FSZ, %sp
	set	9b, %o0
	srax	%i0, 32, %o1
	sra	%i0, 0, %o2
	srax	%i1, 32, %o3
	mov	%i6, %o5
	GLOBTOLOC
	call	prom_printf
	 srax	%i1, 0, %o4
	LOCTOGLOB
	restore
7:	
#endif
#if 0
	ld	[%l4 + IH_CLR], %l3	!		load up the clrintr ptr for later
	add	%sp, CCFSZ, %o2		!	tf = %sp + CCFSZ
	brnz,a,pt	%l3, 5f		!		Clear this intr?
	 stx	%g0, [%l3]		!		Yes
5:	
2:	jmpl	%o1, %o7		!	handled = (*ih->ih_fun)(...)
	 movrz	%o0, %o2, %o0		!	arg = (arg == 0) ? arg : tf
	movrnz	%o0, %o0, %l5		! Store the success somewhere
	ld	[%l4 + IH_NEXT], %l4	!	and ih = ih->ih_next
3:	brnz,pt	%l4, 1b			! } while (ih)
	 clr	%l3			! Make sure we don't have a valid pointer
	brnz,pn	%l5, 4f			!	if (handled) break
	 nop
!	call	_C_LABEL(strayintr)	!	strayintr(&intrframe)
	 add	%sp, CCFSZ, %o0
	/* all done: restore registers and go return */
#else
	brz,a,pn	%o0, 2f
	 add	%sp, CCFSZ, %o0
2:	jmpl	%o1, %o7		!	handled = (*ih->ih_fun)(...)
	 ld	[%l4 + IH_CLR], %l3
	brnz,a,pt	%l3, 5f		! Clear intr?
	 stx	%g0, [%l3]		! Yes
5:	brnz,pn	%o0, 4f			! if (handled) break
	 ld	[%l4 + IH_NEXT], %l4	!	and ih = ih->ih_next
3:	brnz,pt	%l4, 1b			! while (ih)
	 clr	%l3			! Make sure we don't have a valid pointer
	call	_C_LABEL(strayintr)		!	strayintr(&intrframe)
	 add	%sp, CCFSZ, %o0
	/* all done: restore registers and go return */
#endif
4:
	ldub	[%sp + CCFSZ + TF_OLDPIL], %l3	! restore old %pil
	wrpr	%g0, PSTATE_KERN, %pstate	! Disable interrupts	
	stw	%l6, [%sp + CCFSZ + TF_Y]	! Silly, but we need to save this for rft
	wrpr	%l3, 0, %pil
	
	rdpr	%tl, %l3			! Restore old trap frame
	inc	%l3
	CHKPT(%l1,%l2,0x27)
	wrpr	%g0, %l3, %tl

	CLRTT
	CHKPT(%o1,%o2,5)
	b	return_from_trap
	 nop

#ifdef notyet
/*
 * Level 12 (ZS serial) interrupt.  Handle it quickly, schedule a
 * software interrupt, and get out.  Do the software interrupt directly
 * if we would just take it on the way out.
 *
 * Input:
 *	%l0 = %psr
 *	%l1 = return pc
 *	%l2 = return npc
 * Internal:
 *	%l3 = zs device
 *	%l4, %l5 = temporary
 *	%l6 = rr3 (or temporary data) + 0x100 => need soft int
 *	%l7 = zs soft status
 */
zshard:
#endif /* notyet */

	.globl	return_from_trap, rft_kernel, rft_user, rft_invalid
	.globl	softtrap, slowtrap
	.globl	clean_trap_window, syscall

	
/*
 * Various return-from-trap routines (see return_from_trap).
 */

/*
 * Return from trap.
 * registers are:
 *
 *	[%sp + CCFSZ] => trap frame
 *
 * We must load all global, out, and trap registers from the trap frame.
 *
 * If returning to kernel, we should be at the proper trap level because
 * we don't touch %tl.
 *
 * When returning to user mode, the trap level does not matter, as it
 * will be set explicitly.
 *
 * If we are returning to user code, we must:
 *  1.  Check for register windows in the pcb that belong on the stack.
 *	If there are any, reload them
 */
return_from_trap:
#ifdef NOTDEF_DEBUG
	mov	%i6, %o1
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	mov	%i1, %o1
	ldx	[%fp + CCFSZ + TF_PC], %o3
	ldx	[%fp + CCFSZ + TF_NPC], %o4
	GLOBTOLOC
	call	printf
	 mov	%i6, %o2
	LOCTOGLOB
	ba	2f
	restore
1:	.asciz	"rft[%x,%x,%p,%p]"
3:	.asciz	"return_from_trap: fp=%x sp=%x pc=%x\n"
	_ALIGN
2:	
#endif

#ifdef NOTDEF_DEBUG
	ldx	[%sp + CCFSZ + TF_TSTATE], %g2
	set	TSTATE_AG, %g3
	set	4f, %g4
	and	%g2, %g3, %g3
	clr	%o1
	movrnz	%g3, %g4, %o1
	set	TSTATE_MG, %g3
	set	3f, %g4
	and	%g2, %g3, %g3
	movrnz	%g3, %g4, %o1
	set	TSTATE_IG, %g3
	set	5f, %g4
	and	%g2, %g3, %g3
	movrnz	%g3, %g4, %o1
	brz,pt	%o1, 2f
	 set	1f, %o0
	call	printf
	 nop
	ta	1; nop
1:	.asciz	"Returning to trap from %s globals\n"
3:	.asciz	"MMU"
4:	.asciz	"Altermate"
5:	.asciz	"Interrupt"
	_ALIGN
2:
#endif
	!!
	!! We'll make sure we flush our pcb here, rather than later.
	!!
	ldx	[%sp + CCFSZ + TF_TSTATE], %g1
#if 0
	btst	TSTATE_PRIV, %g1			! returning to userland?
	bnz,pt	%icc, 0f
	 sethi	%hi(_C_LABEL(curproc)), %o1
	call	_C_LABEL(rwindow_save)			! Flush out our pcb
	 lduw	[%o1 + %lo(_C_LABEL(curproc))], %o0
0:
#endif
	wrpr	%g0, PSTATE_KERN, %pstate		! Make sure we have normal globals & no IRQs
	/* First restore normal globals */
	ldx	[%sp + CCFSZ + TF_G + (1*8)], %g1	! restore g1
	ldx	[%sp + CCFSZ + TF_G + (2*8)], %g2	! restore g2
	ldx	[%sp + CCFSZ + TF_G + (3*8)], %g3	! restore g3
	ldx	[%sp + CCFSZ + TF_G + (4*8)], %g4	! restore g4
	ldx	[%sp + CCFSZ + TF_G + (5*8)], %g5	! restore g5
	ldx	[%sp + CCFSZ + TF_G + (6*8)], %g6	! restore g6
	ldx	[%sp + CCFSZ + TF_G + (7*8)], %g7	! restore g7
	/* Then switch to alternate globals and load outs */
	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate
#ifdef TRAPS_USE_IG
	wrpr	%g0, PSTATE_KERN|PSTATE_IG, %pstate	! DEBUG
#endif
	mov	%sp, %g6
#ifdef TRAPWIN
	ldx	[%g6 + CCFSZ + TF_O + (0*8)], %i0	! tf.tf_out[0], etc
	ldx	[%g6 + CCFSZ + TF_O + (1*8)], %i1
	ldx	[%g6 + CCFSZ + TF_O + (2*8)], %i2
	ldx	[%g6 + CCFSZ + TF_O + (3*8)], %i3
	ldx	[%g6 + CCFSZ + TF_O + (4*8)], %i4
	ldx	[%g6 + CCFSZ + TF_O + (5*8)], %i5
	ldx	[%g6 + CCFSZ + TF_O + (6*8)], %i6
	ldx	[%g6 + CCFSZ + TF_O + (7*8)], %i7
#else
	ldx	[%g6 + CCFSZ + TF_O + (0*8)], %o0	! tf.tf_out[0], etc
	ldx	[%g6 + CCFSZ + TF_O + (1*8)], %o1
	ldx	[%g6 + CCFSZ + TF_O + (2*8)], %o2
	ldx	[%g6 + CCFSZ + TF_O + (3*8)], %o3
	ldx	[%g6 + CCFSZ + TF_O + (4*8)], %o4
	ldx	[%g6 + CCFSZ + TF_O + (5*8)], %o5
	ldx	[%g6 + CCFSZ + TF_O + (6*8)], %o6
	ldx	[%g6 + CCFSZ + TF_O + (7*8)], %o7
	ldx	[%g6 + CCFSZ + TF_L + (0*8)], %l0	! tf.tf_local[0], etc
	ldx	[%g6 + CCFSZ + TF_L + (1*8)], %l1
	ldx	[%g6 + CCFSZ + TF_L + (2*8)], %l2
	ldx	[%g6 + CCFSZ + TF_L + (3*8)], %l3
	ldx	[%g6 + CCFSZ + TF_L + (4*8)], %l4
	ldx	[%g6 + CCFSZ + TF_L + (5*8)], %l5
	ldx	[%g6 + CCFSZ + TF_L + (6*8)], %l6
	ldx	[%g6 + CCFSZ + TF_L + (7*8)], %l7
	ldx	[%g6 + CCFSZ + TF_I + (0*8)], %i0	! tf.tf_in[0], etc
	ldx	[%g6 + CCFSZ + TF_I + (1*8)], %i1
	ldx	[%g6 + CCFSZ + TF_I + (2*8)], %i2
	ldx	[%g6 + CCFSZ + TF_I + (3*8)], %i3
	ldx	[%g6 + CCFSZ + TF_I + (4*8)], %i4
	ldx	[%g6 + CCFSZ + TF_I + (5*8)], %i5
	ldx	[%g6 + CCFSZ + TF_I + (6*8)], %i6
	ldx	[%g6 + CCFSZ + TF_I + (7*8)], %i7
#endif
	/* Now load trap registers into alternate globals */
	ld	[%g6 + CCFSZ + TF_Y], %g4
	ldx	[%g6 + CCFSZ + TF_TSTATE], %g1		! load new values
	wr	%g4, 0, %y
	ldx	[%g6 + CCFSZ + TF_PC], %g2
	ldx	[%g6 + CCFSZ + TF_NPC], %g3

#ifdef NOTDEF_DEBUG
	ldub	[%g6 + CCFSZ + TF_PIL], %g5		! restore %pil
	wrpr	%g5, %pil				! DEBUG
#endif
	
	/* Returning to user mode or kernel mode? */	
	btst	TSTATE_PRIV, %g1		! returning to userland?
	CHKPT(%g4, %g7, 6)
	bz,pt	%icc, rft_user
	 sethi	%hi(_C_LABEL(want_ast)), %g7	! first instr of rft_user

/*
 * Return from trap, to kernel.
 *
 * We will assume, for the moment, that all kernel traps are properly stacked
 * in the trap registers, so all we have to do is insert the (possibly modified)
 * register values into the trap registers then do a retry.
 *
 */
rft_kernel:
	wrpr	%g3, 0, %tnpc
	wrpr	%g2, 0, %tpc
	wrpr	%g1, 0, %tstate
	CHKPT(%g1,%g2,7)
#ifdef TRAPWIN
	restore
	CHKPT(%g1,%g2,0)			! Clear this out
	rdpr	%tstate, %g1			! Since we may have trapped our regs may be toast
	rdpr	%cwp, %g2
	andn	%g1, CWP, %g1
	wrpr	%g1, %g2, %tstate		! Put %cwp in %tstate
#endif
	CLRTT
#ifdef TRAPTRACE
	set	trap_trace, %g2
	lduw	[%g2+TRACEDIS], %g4
	brnz,pn	%g4, 1f
	 nop
	lduw	[%g2+TRACEPTR], %g3
	rdpr	%tl, %g4
	set	_C_LABEL(curproc), %g6
	sllx	%g4, 13, %g4
	lduw	[%g6], %g6
	clr	%g6		! DISABLE PID
	mov	%g0, %g5
	brz,pn	%g6, 2f
	 andncc	%g3, (TRACESIZ-1), %g0
!	ldsw	[%g6+P_PID], %g5	! Load PID
2:	
	
	set	_C_LABEL(cpcb), %g6	! Load up nsaved
	lduw	[%g6], %g6
	ldub	[%g6 + PCB_NSAVED], %g6
	sllx	%g6, 9, %g6
	or	%g6, %g4, %g4
	
	rdpr	%tstate, %g6
	movnz	%icc, %g0, %g3
	rdpr	%tpc, %g7
	sth	%g4, [%g2+%g3]
	inc	2, %g3
	sth	%g5, [%g2+%g3]
	inc	2, %g3
	stw	%g6, [%g2+%g3]
	inc	4, %g3
	stw	%sp, [%g2+%g3]
	inc	4, %g3
	stw	%g7, [%g2+%g3]
	inc	4, %g3
	stw	%g3, [%g2+TRACEPTR]
1:	
#endif
#ifdef TRAPSTATS
	rdpr	%tl, %g2
	set	_C_LABEL(rftkcnt), %g1
	sllx	%g2, 2, %g2
	add	%g1, %g2, %g1	
	lduw	[%g1], %g2
	inc	%g2
	stw	%g2, [%g1]
#endif	
!	wrpr	%g0, 0, %cleanwin	! DEBUG
	retry					! We should allow some way to distinguish retry/done
	NOTREACHED
/*
 * Return from trap, to user.  Checks for scheduling trap (`ast') first;
 * will re-enter trap() if set.  Note that we may have to switch from
 * the interrupt stack to the kernel stack in this case.
 *	%g1 = %tstate
 *	%g2 = return %pc
 *	%g3 = return %npc
 * If returning to a valid window, just set psr and return.
 */
rft_wcnt:	.word 0

rft_user:
!	sethi	%hi(_C_LABEL(want_ast)), %g7	! (done above)
	lduw	[%g7 + %lo(_C_LABEL(want_ast))], %g7! want AST trap?
	/* This is probably necessary */
	wrpr	%g3, 0, %tnpc
	wrpr	%g2, 0, %tpc
	wrpr	%g1, 0, %tstate
#ifdef TRAPWIN
	brnz,pn	%g7, softtrap			! yes, re-enter trap with type T_AST
#else
	wrpr	%g0, WSTATE_USER, %wstate	! Need to know where our sp points
	brnz,pn	%g7, Lslowtrap_reenter		! yes, re-enter trap with type T_AST
#endif
	 mov	T_AST, %g4

	CHKPT(%g4,%g7,8)
#ifdef NOTDEF_DEBUG
	sethi	%hi(_C_LABEL(cpcb)), %g4
	lduw	[%g4 + %lo(_C_LABEL(cpcb))], %g4
	ldub	[%g4 + PCB_NSAVED], %g4		! nsaved
	brz,pt	%g4, 2f		! Only print if nsaved <> 0
	 nop
#if 0
	set	panicstack-CC64FSZ, %g7
	mov	%sp, %g6
	mov	%g7, %sp
	save	%sp, -CC64FSZ, %sp
	wrpr	%g0, WSTATE_KERN, %wstate
#endif
	set	1f, %o0
	mov	%g4, %o1
	mov	%g2, %o2			! pc
	wr	%g0, ASI_DMMU, %asi		! restore the user context
	ldxa	[CTX_SECONDARY] %asi, %o3	! ctx
	GLOBTOLOC
	mov	%g3, %o5
	call	printf
	 mov	%i6, %o4			! sp
!	wrpr	%g0, PSTATE_INTR, %pstate		! Allow IRQ service
!	wrpr	%g0, PSTATE_KERN, %pstate		! DenyIRQ service
	LOCTOGLOB
#if 0
	restore
!	wrpr	%g0, WSTATE_USER, %wstate	! Comment
	mov	%g6, %sp
	Debugger()
#endif
	ba	2f
	 nop
1:
	.asciz	"rft_user: nsaved=%x pc=%d ctx=%x sp=%x npc=%p\n"
	_ALIGN
2:
#endif

#ifdef TRAPWIN
	/*
	 * First: blast away our caches
	 */
!	call	_C_LABEL(blast_vcache)		! Clear any possible cache conflict
	/*
	 * NB: only need to do this after a cache miss
	 */
#else
	/*
	 * First: blast away our caches
	 *  Unfortunately we can't touch our outs so we
	 *  need a special in-lined version here.
	 */
	set	(2*NBPG)-8, %g7
1:	
	stxa	%g0, [%g7] ASI_ICACHE_TAG
	stxa	%g0, [%g7] ASI_DCACHE_TAG
	brnz,pt	%g7, 1b
	 dec	8, %g7
	sethi	%hi(KERNBASE), %g7
	flush	%g7

#endif
#ifdef TRAPSTATS
	set	_C_LABEL(rftucnt), %g6
	lduw	[%g6], %g7
	inc	%g7
	stw	%g7, [%g6]
#endif	
	/* Here we need to undo the damage caused by switching to a kernel stack */
	
	rdpr	%otherwin, %g7			! restore register window controls
	rdpr	%canrestore, %g5		! DEBUG
	tst	%g5				! DEBUG
	tnz	%icc, 1; nop			! DEBUG
!	mov	%g0, %g5			! There shoud be *NO* %canrestore
	add	%g7, %g5, %g7			! DEBUG
	wrpr	%g0, %g7, %canrestore
	wrpr	%g0, 0, %otherwin

	CHKPT(%g4,%g7,9)
	wrpr	%g0, WSTATE_USER, %wstate	! Need to know where our sp points

	/*
	 * Now check to see if any regs are saved in the pcb and restore them.
	 *
	 * We will use alternate globals %g4..%g7 because %g1..%g3 are used
	 * by the data fault trap handlers and we don't want possible conflict.
	 */
	sethi	%hi(_C_LABEL(cpcb)), %g6
	lduw	[%g6 + %lo(_C_LABEL(cpcb))], %g6
	ldub	[%g6 + PCB_NSAVED], %g7			! Any saved reg windows?
#ifdef DEBUG
	set	rft_wcnt, %g4	! Keep track of all the windows we restored
	stw	%g7, [%g4]
#endif
	brz,a,pt	%g7, 5f				! No
#ifdef TRAPWIN
	 restore					! This may fault, but we should return here.
#else
	 nop
#endif
	dec	%g7					! We can do this now or later.  Move to last entry
	sll	%g7, 7, %g5				! calculate ptr into rw64 array 8*16 == 128 or 7 bits

	rdpr	%canrestore, %g4			! DEBUG Make sure we've restored everything
	brnz,a,pn	%g4, 0f				! DEBUG
	 sir						! DEBUG we should NOT have any usable windows here
0:							! DEBUG
#ifdef DEBUG
	wrpr	%g0, 5, %tl
#endif
	rdpr	%otherwin, %g4
!	wrpr	%g0, 4, %tl				! DEBUG -- don't allow *any* traps in this section
	brz,pt	%g4, 6f					! We should not have any user windows left
	 add	%g5, %g6, %g5

!	wrpr	%g0, 0, %tl				! DEBUG -- allow traps again while we panic
	set	1f, %o0
	mov	%g7, %o1
	mov	%g4, %o2
	call	printf
	 wrpr	%g0, PSTATE_KERN, %pstate
	set	2f, %o0
	call	panic
	 nop
	NOTREACHED
1:	.asciz	"pcb_nsaved=%x and otherwin=%x\n"
2:	.asciz	"rft_user\n"
	_ALIGN
6:
#ifndef TRAPWIN
	wrpr	%g0, 0, %canrestore			! Make sure we have no freeloaders
#endif
3:
	restored					! Load in the window
	restore						! This should not trap!
	ldx	[%g5 + PCB_RW + ( 0*8)], %l0		! Load the window from the pcb
	ldx	[%g5 + PCB_RW + ( 1*8)], %l1 
	ldx	[%g5 + PCB_RW + ( 2*8)], %l2 
	ldx	[%g5 + PCB_RW + ( 3*8)], %l3 
	ldx	[%g5 + PCB_RW + ( 4*8)], %l4 
	ldx	[%g5 + PCB_RW + ( 5*8)], %l5 
	ldx	[%g5 + PCB_RW + ( 6*8)], %l6 
	ldx	[%g5 + PCB_RW + ( 7*8)], %l7 

	ldx	[%g5 + PCB_RW + ( 8*8)], %i0 
	ldx	[%g5 + PCB_RW + ( 9*8)], %i1 
	ldx	[%g5 + PCB_RW + (10*8)], %i2 
	ldx	[%g5 + PCB_RW + (11*8)], %i3 
	ldx	[%g5 + PCB_RW + (12*8)], %i4 
	ldx	[%g5 + PCB_RW + (13*8)], %i5 
	ldx	[%g5 + PCB_RW + (14*8)], %i6 
	ldx	[%g5 + PCB_RW + (15*8)], %i7

	stx	%g0, [%g5 + PCB_RW + (14*8)]		! DEBUG mark that we've saved this one
	
	cmp	%g5, %g6
	bgu,pt	%xcc, 3b				! Next one?
	 dec	8*16, %g5

	stb	%g0, [%g6 + PCB_NSAVED]			! Clear them out so we won't do this again
	rdpr	%ver, %g5
	and	%g5, CWP, %g5
	add	%g5, %g7, %g4
	dec	1, %g5					! NWINDOWS-1-1
	wrpr	%g5, 0, %cansave
	wrpr	%g0, 0, %canrestore			! Make sure we have no freeloaders XXX
	wrpr	%g0, WSTATE_USER, %wstate		! Save things to user space
	mov	%g7, %g5				! We already did one restore
4:
	rdpr	%canrestore, %g4
	inc	%g4
	deccc	%g5
	wrpr	%g4, 0, %cleanwin			! Make *sure* we don't trap to cleanwin
	bge,a,pt	%xcc, 4b				! return to starting regwin
	 save	%g0, %g0, %g0				! This may force a datafault

#ifdef DEBUG
	wrpr	%g0, 0, %tl
#endif
#ifdef TRAPSTATS
	set	_C_LABEL(rftuld), %g5
	lduw	[%g5], %g4
	inc	%g4
	stw	%g4, [%g5]
#endif	
	!!
	!! We can't take any save faults in here 'cause they will never be serviced
	!! 

#ifdef DEBUG
	sethi	%hi(_C_LABEL(cpcb)), %g5
	lduw	[%g5 + %lo(_C_LABEL(cpcb))], %g5
	ldub	[%g5 + PCB_NSAVED], %g5		! Any saved reg windows?
	tst	%g5
	wrpr	%g0, 0, %tl	! DEBUG
	tnz	%icc, 1; nop			! Debugger if we still have saved windows
	bne,a	rft_user			! Try starting over again
	 sethi	%hi(_C_LABEL(want_ast)), %g7
#endif

5:
	CHKPT(%g4,%g7,0xa)
	rdpr	%canrestore, %g5
	wrpr	%g5, 0, %cleanwin			! Force cleanup of kernel windows
	
#ifdef NOTDEF_DEBUG
	ldx	[%g6 + CCFSZ + TF_L + (0*8)], %g5! DEBUG -- get proper value for %l0
	cmp	%l0, %g5
	be,a,pt %icc, 1f
	 nop
!	sir			! WATCHDOG
	set	badregs, %g1	! Save the suspect regs
	stw	%l0, [%g1+(4*0)]
	stw	%l1, [%g1+(4*1)]
	stw	%l2, [%g1+(4*2)]
	stw	%l3, [%g1+(4*3)]
	stw	%l4, [%g1+(4*4)]
	stw	%l5, [%g1+(4*5)]
	stw	%l6, [%g1+(4*6)]
	stw	%l7, [%g1+(4*7)]
	stw	%i0, [%g1+(4*8)+(4*0)]
	stw	%i1, [%g1+(4*8)+(4*1)]
	stw	%i2, [%g1+(4*8)+(4*2)]
	stw	%i3, [%g1+(4*8)+(4*3)]
	stw	%i4, [%g1+(4*8)+(4*4)]
	stw	%i5, [%g1+(4*8)+(4*5)]
	stw	%i6, [%g1+(4*8)+(4*6)]
	stw	%i7, [%g1+(4*8)+(4*7)]
	save
	inc	%g7
	wrpr	%g7, 0, %otherwin
	wrpr	%g0, 0, %canrestore
	wrpr	%g0, WSTATE_KERN, %wstate	! Need to know where our sp points
	set	rft_wcnt, %g4	! Restore nsaved before trapping
	sethi	%hi(_C_LABEL(cpcb)), %g6
	lduw	[%g6 + %lo(_C_LABEL(cpcb))], %g6
	lduw	[%g4], %g4
	stb	%g4, [%g6 + PCB_NSAVED]
	ta	1
	sir
badregs:
	.space	16*4
1:
#endif

	rdpr	%cwp, %g7			! Find our cur window
	andn	%g1, CWP, %g1			! Clear it from %tstate
	wrpr	%g0, 1, %tl			! Set up the trap state
	wrpr	%g2, 0, %tpc
	wrpr	%g3, 0, %tnpc
	wrpr	%g1, %g7, %tstate		! Set %tstate with %cwp
	CHKPT(%g4,%g7,0xb)

	wr	%g0, ASI_DMMU, %asi		! restore the user context
	ldxa	[CTX_SECONDARY] %asi, %g4
	stxa	%g4, [CTX_PRIMARY] %asi
	sethi	%hi(KERNBASE), %g7		! Should not be needed due to retry
	membar	#Sync				! Should not be needed due to retry
	flush	%g7				! Should not be needed due to retry
	CLRTT
	CHKPT(%g4,%g7,0xc)
#ifdef TRAPTRACE
	set	trap_trace, %g2
	lduw	[%g2+TRACEDIS], %g4
	brnz,pn	%g4, 1f
	 nop
	lduw	[%g2+TRACEPTR], %g3
	rdpr	%tl, %g4
	mov	1, %g5
	set	_C_LABEL(curproc), %g6
	sllx	%g4, 13, %g4
	lduw	[%g6], %g6
	clr	%g6		! DISABLE PID
	or	%g4, %g5, %g4
	mov	%g0, %g5
	brz,pn	%g6, 2f
	 andncc	%g3, (TRACESIZ-1), %g0
!	ldsw	[%g6+P_PID], %g5	! Load PID
2:	
	
	set	_C_LABEL(cpcb), %g6	! Load up nsaved
	lduw	[%g6], %g6
	ldub	[%g6 + PCB_NSAVED], %g6
	sllx	%g6, 9, %g6
	or	%g6, %g4, %g4
	
	rdpr	%tstate, %g6
	movnz	%icc, %g0, %g3
	rdpr	%tpc, %g7
	sth	%g4, [%g2+%g3]
	inc	2, %g3
	sth	%g5, [%g2+%g3]
	inc	2, %g3
	stw	%g6, [%g2+%g3]
	inc	4, %g3
	stw	%sp, [%g2+%g3]
	inc	4, %g3
	stw	%g7, [%g2+%g3]
	inc	4, %g3
	stw	%g3, [%g2+TRACEPTR]
1:	
#endif
#ifdef TRAPSTATS
	set	_C_LABEL(rftudone), %g1
	lduw	[%g1], %g2
	inc	%g2
	stw	%g2, [%g1]
#endif
#ifdef DEBUG
	sethi	%hi(_C_LABEL(cpcb)), %g5
	lduw	[%g5 + %lo(_C_LABEL(cpcb))], %g5
	ldub	[%g5 + PCB_NSAVED], %g5		! Any saved reg windows?
	tst	%g5
	tnz	%icc, 1; nop			! Debugger if we still have saved windows!
#endif
	wrpr	%g0, 0, %pil			! Enable all interrupts
	retry
	
! exported end marker for kernel gdb
	.globl	_C_LABEL(endtrapcode)
_C_LABEL(endtrapcode):

#ifdef not4u
#ifdef SUN4
/*
 * getidprom(struct idprom *, sizeof(struct idprom))
 */
	.global _C_LABEL(getidprom)
_C_LABEL(getidprom):
	set	AC_IDPROM, %o2
1:	lduba	[%o2] ASI_CONTROL, %o3
	stb	%o3, [%o0]
	inc	%o0
	inc	%o2
	dec	%o1
	cmp	%o1, 0
	bne	1b
	 nop
	retl
	 nop
#endif
#endif

#if 1
#define	xword	word	0, 
	.data
	.align	8
Lcons:
	.xword	0		! This is our device handle
	
	.align	8
of_finddev:			! Here are the commands themselves.  64 bits each field.
	.xword	Lfinddevice	! command name
	.xword	1		! # params
	.xword	1		! # returns
	.xword	Lchosen		! Name of device
	.xword	0		! handle -- return value
	.align	8
of_getprop:
	.xword	Lgetprop
	.xword	4
	.xword	1
	.xword	0		! handle to "/chosen"
	.xword	Lstdout
	.xword	Lcons		! buffer
	.xword	4		! size of buffer value
	.xword	0
	.align	8
of_write:	
	.xword	Lwrite
	.xword	3
	.xword	1
	.xword	0		! the handle
of_mesg:	
	.xword	Lstring
	.xword	(Lfinddevice-Lstring-1)! strlen
	.xword	0		!  number written
	.align	8

of_enter:
	.xword	Lenter
	.xword	0
	.xword	0
	
	.text
	.align 	8

	.global	start
	.type	start, #function

	/*
	 * Yet another debug rom_halt
	 */
trap_enter:
	set	panicstack - CCFSZ - BIAS, %sp
	mov	7, %o3
	wrpr	%g0, %o3, %cleanwin	
	set	of_enter, %o1
	set	romp, %o2
	CHKPT(%o3,%o4,0x34)
	jmpl	%o2, %o7	! Call prom
	 wrpr	%g0, 0, %tl
	
	/*
	 * Panic to prom -- panic str in %g1
	 */
	.global _C_LABEL(prom_panic)
_C_LABEL(prom_panic):
	set	romitsbp, %g2			
	ldx	[%g2], %g5			! Restore TSB pointers
	set	TSB, %g3
	stxa	%g5, [%g3] ASI_IMMU
	membar	#Sync
	set	romdtsbp, %g2
	ldx	[%g2], %g5
	stxa	%g5, [%g3] ASI_DMMU
	membar	#Sync
	
	set	romtrapbase, %g3		! Restore trapbase
	ldx	[%g3], %g5
	wrpr	%g5, 0, %tba
	set	romwstate, %g3			! Restore wstate
	ldx	[%g3], %g5
	wrpr	%g5, 0, %wstate
	
#ifndef NOTDEF
	set	.ebootstack, %o1
	and	%o1, ~(STACK_ALIGN64-1), %o1
	sub	%o1, SA64(CC64FSZ), %o1
	save	%o1, -SA64(CC64FSZ), %sp
	sub	%sp, BIAS, %sp		! delay; Now a 64 bit frame ????????
#else
	save	%sp, -CC64FSZ, %sp
	andcc	%sp, 1, %g0
	bz,a	0f
	 sub	%sp, BIAS, %sp
0:	
#endif

	mov	%g1, %i0		! Save our input param
	set	of_mesg, %l1
	stx	%i0, [%l1]	! Save str
	set	romp, %l0
	ld	[%l0], %i4	! Load romp

	clr	%i1
1:
	ldub	[%i0+%i1], %i2	! Calculate strlen
	brnz,a,pt	%i2, 1b
	 inc	%i1

	stx	%i1, [%l1+8]	! Save strlen
	
	/*
	 * Set the psr into a known state:
	 * Set supervisor mode, interrupt level >= 13, traps enabled
	 */
	wrpr	%g0, 13, %pil
	wrpr	%g0, PSTATE_KERN, %pstate
	
	set	of_finddev, %o0
	jmpl	%i4, %o7	! Call prom
	mov	%o0, %l1
	
	ldx	[%l1+(4*8)], %l2	! get retval
	set	of_getprop,%o0
	jmpl	%i4, %o7		! Call prom
	stx	%l2, [%o0+(3*8)]	! store handle

	set	Lcons, %l1
	ld	[%l1], %l2		! get fd
	set	of_write, %o0		! Store fd in command
	jmpl	%i4, %o7		! Call prom
	stx	%l2, [%o0+(3*8)]

	mov	%i4, %o4	! Set things up again like they were when we started

	ret			! Looks like we got problems w/the stack here.
	 restore
	
	set	of_enter, %o0	! Halt
	jmpl	%i4, %o7
	 nop
	
	ret
	 restore
	
	.data
	.align 8
#define STACK_SIZE	0x14000
	.space	STACK_SIZE
.ebootstack:			! end (top) of boot stack

	.align	8
Lstring:	
	.asciz	"This is a test message from Eduardo\r\n"
	.align	8
Lfinddevice:
	.asciz	"finddevice"
	.align	8
Lchosen:
	.asciz	"/chosen"
	.align	8
Lgetprop:
	.asciz	"getprop"
	.align	8
Lstdout:
	.asciz "stdout"
	.align	8
Lwrite:
	.asciz "write"
	.align	8
Lenter:
	.asciz "exit"
	.align 8
romp:	.xword 0		! ROM interface pointer
romtrapbase:
	.xword 0
romitsbp:
	.xword 0
romdtsbp:
	.xword 0
romwstate:
	.xword 0
	.text
#endif
!!!
!!! Dump the DTLB to phys address in %o0 and print it
!!!
!!! Only toast a few %o registers
!!! 
	.globl	dump_dtlb
dump_dtlb:
	clr	%o1
	add	%o1, (64*8), %o3	
1:	
	ldxa	[%o1] ASI_DMMU_TLB_TAG, %o2
	membar	#Sync
	stx	%o2, [%o0]
	membar	#Sync
	inc	8, %o0
	ldxa	[%o1] ASI_DMMU_TLB_DATA, %o4
	membar	#Sync
	inc	8, %o1
	stx	%o4, [%o0]
	cmp	%o1, %o3
	membar	#Sync
	bl	1b
	 inc	8, %o0

	retl
	 nop
	.globl	print_dtlb
print_dtlb:
	save	%sp, -CCFSZ, %sp
	clr	%l1
	add	%l1, (64*8), %l3
	clr	%l2
1:	
	ldxa	[%l1] ASI_DMMU_TLB_TAG, %o2
	membar	#Sync
	srl	%o2, 0, %o3
	mov	%l2, %o1
	srax	%o2, 32, %o2
	ldxa	[%l1] ASI_DMMU_TLB_DATA, %o4
	membar	#Sync
	srl	%o4, 0, %o5
	inc	%l2
	srax	%o4, 32, %o4
	set	2f, %o0
	call	db_printf
	 inc	8, %l1
	
	ldxa	[%l1] ASI_DMMU_TLB_TAG, %o2
	membar	#Sync
	srl	%o2, 0, %o3
	mov	%l2, %o1
	srax	%o2, 32, %o2
	ldxa	[%l1] ASI_DMMU_TLB_DATA, %o4
	membar	#Sync
	srl	%o4, 0, %o5
	inc	%l2
	srax	%o4, 32, %o4
	set	3f, %o0
	call	db_printf
	 inc	8, %l1

	cmp	%l1, %l3
	bl	1b
	 inc	8, %l0

	ret
	 restore
2:
	.asciz	"%2d:%08x:%08x %08x:%08x "
3:
	.asciz	"%2d:%08x:%08x %08x:%08x\r\n"
	.align	8
dostart:
	/*
	 * Startup.
	 *
	 * The Sun FCODE bootloader is nice and loads us where we want
	 * to be.  We have a full set of mappings already set up for us.
	 *
	 * I think we end up having an entire 16M allocated to us.
	 *
	 * We enter with the prom entry vector in %o0, dvec in %o1,
	 * and the bootops vector in %o2.
	 *
	 * All we need to do is:
	 *
	 *	1:	Save the prom vector
	 *
	 *	2:	Create a decent stack for ourselves
	 *
	 *	3:	Install the permanent 4MB kernel mapping
	 *
	 *	4:	Call the C language initialization code
	 *
	 */
	
	/*
	 * Set the psr into a known state:
	 * Set supervisor mode, interrupt level >= 13, traps enabled
	 */
	wrpr	%g0, 13, %pil
	wrpr	%g0, PSTATE_INTR|PSTATE_PEF, %pstate
#ifdef DDB
	/*
	 * First, check for DDB arguments.  A pointer to an argument 
	 * is passed in %o1 who's length is passed in %o2.  Our 
	 * bootloader passes in a magic number as the first argument,
	 * followed by esym as argument 2, so check that %o2 == 8, 
	 * then extract esym and check the magic number.
	 *
	 *  Oh, yeah, start of elf symtab is arg 3.
	 */
	cmp	%o2, 8
	blt	1f			! Not enuff args
	
	 set	0x44444230, %l3
	lduw	[%o1], %l4
	cmp	%l3, %l4		! chk magic
	bne	1f
	 nop
	
	lduw	[%o1+4], %l4	
	sethi	%hi(_C_LABEL(esym)), %l3	! store _esym
	st	%l4, [%l3 + %lo(_C_LABEL(esym))]

	cmp	%o2, 12
	blt	1f
	 nop
	
	lduw	[%o1+8], %l4	
	sethi	%hi(_C_LABEL(ssym)), %l3	! store _esym
	st	%l4, [%l3 + %lo(_C_LABEL(ssym))]
1:
#endif
	/*
	 * Step 1: Save rom entry pointer
	 */
	
	mov	%o4, %g7	! save prom vector pointer
	set	romp, %o5
	st	%o4, [%o5]	! It's initialized data, I hope

	/*
	 * Step 2: Set up a v8-like stack
	 */

!!! Make sure our stack's OK.
#define SAVE	save %sp, -CC64FSZ, %sp
	SAVE;	SAVE;	SAVE;	SAVE;	SAVE;	SAVE;	SAVE;	SAVE;	SAVE;	SAVE
	restore;restore;restore;restore;restore;restore;restore;restore;restore;restore
 	set	estack0 - CCFSZ - 80, %l0	! via syscall(boot_me_up) or somesuch
	save	%g0, %l0, %sp
!!! Make sure our stack's OK.
	SAVE;	SAVE;	SAVE;	SAVE;	SAVE;	SAVE;	SAVE;	SAVE;	SAVE;	SAVE
	restore;restore;restore;restore;restore;restore;restore;restore;restore;restore

#if 0
	/*
	 * This code sets up a proper v9 stack.  We won't need it till we get a
	 * 64-bit kernel.
	 */
	set	SA64(USRSTACK-CC64FSZ)-BIAS, %fp
	set	SA64(estack0-CC64FSZ)-BIAS, %sp
	
	/* If we supported multiple arch then we would Export actual trapbase */
	sethi	%hi(_C_LABEL(trapbase)), %o0
	st	%g6, [%o0+%lo(_C_LABEL(trapbase))]
#endif

	/*
	 * Step 3: clear BSS.  This may just be paranoia; the boot
	 * loader might already do it for us; but what the hell.
	 */
	set	_C_LABEL(edata), %o0		! bzero(edata, end - edata)
	set	_C_LABEL(end), %o1
	call	_C_LABEL(bzero)
	 sub	%o1, %o0, %o1

	/*
	 * Step 4: compute number of windows and set up tables.
	 * We could do some of this later.
	 */
	rdpr	%ver, %g1
	and	%g1, 0x0f, %g1		! want just the CWP bits
	add	%g1, 1, %o0		! compute nwindows
	sethi	%hi(_C_LABEL(nwindows)), %o1	! may as well tell everyone
	st	%o0, [%o1 + %lo(_C_LABEL(nwindows))]

#ifdef DEBUG
	/*
	 * Step 5: save prom configuration so we can panic properly.
	 */	
	set	TSB, %o0
	ldxa	[%o0] ASI_IMMU, %o1		! Get ITSB pointer
	set	romitsbp, %o2
	stx	%o1, [%o2]			! save it
	ldxa	[%o0] ASI_DMMU, %o1		! Get DTSB pointer
	set	romdtsbp, %o2
	stx	%o1, [%o2]			! save it
	
	set	romtrapbase, %o0
	rdpr	%tba, %o1			! Save ROM trapbase
	stx	%o1, [%o0]
	set	romwstate, %o0
	rdpr	%wstate, %o1			! Save ROM wstate
	stx	%o1, [%o0]	
#endif

	/*
	 * Ready to run C code; finish bootstrap.
	 */
	set	CTX_SECONDARY, %o1		! Store -1 in the context register
	set	-1, %o2
	stxa	%o2, [%o1] ASI_DMMU
	membar	#Sync
	ldxa	[%o1] ASI_DMMU, %o0		! then read it back
	stxa	%g0, [%o1] ASI_DMMU
	membar	#Sync
	call	_C_LABEL(bootstrap)
	 inc	%o0				! and add 1 to discover maxctx

	/*
	 * Step 3: install the permanent 4MB kernel mapping in both the
	 * immu and dmmu.  We will clear out other mappings later.
	 *
	 * Register usage in this section:
	 *
	 *	%l0 = KERNBASE
	 *	%l1 = TLB Data w/o low bits
	 *	%l2 = TLB Data w/low bits
	 *	%l3 = tmp
	 *	%l4 = tmp
	 *	%l5 = tmp && TLB_TAG_ACCESS
	 *	%l6 = tmp && CTX_PRIMARY
	 *	%l7 = tmp && demap address
	 */
	wrpr	%g0, 0, %tl			! Make sure we're not in NUCLEUS mode
	sethi	%hi(KERNBASE), %l0		! Find our xlation
	
	set	_C_LABEL(ksegp), %l1		! Find phys addr
	ldx	[%l1], %l1			! The following gets ugly:	We need to load the following mask
	
	sethi	%hi(0xe0000000), %l2		! V=1|SZ=11|NFO=0|IE=0
	sllx	%l2, 32, %l2			! Shift it into place
	
	mov	-1, %l3				! Create a nice mask
	sllx	%l3, 41, %l4			! Mask off high bits
	or	%l4, 0xfff, %l4			! We can just load this in 12 (of 13) bits
	
	andn	%l1, %l4, %l1			! Mask the phys page number
	
	or	%l2, %l1, %l1			! Now take care of the high bits
!	or	%l1, 0x076, %l1			! And low bits:	L=1|CP=1|CV=1|E=0|P=1|W=1(ugh)|G=0

	wrpr	%g0, PSTATE_KERN, %pstate	! Disable interrupts

!	call	print_dtlb			! Debug printf
!	 nop					! delay
#ifdef NOT_DEBUG	
	set	1f, %o0		! Debug printf
	srax	%l0, 32, %o1
	srl	%l0, 0, %o2
	or	%l1, TTE_L|TTE_CP|TTE_CV|TTE_P|TTE_W, %l2	! And low bits:	L=1|CP=1|CV=1|E=0|P=1|W=1(ugh)|G=0
	srax	%l2, 32, %o3
	call	_C_LABEL(prom_printf)
	 srl	%l2, 0, %o4
	ba	2f
	 nop
1:
	.asciz	"Setting DTLB entry %08x %08x data %08x %08x\r\n"
	_ALIGN
2:
#endif
	set	0x400000, %l3			! Demap all of kernel dmmu segment
	mov	%l0, %l5
	set	0x2000, %l4			! 8K page size
	add	%l0, %l3, %l3
0:
	stxa	%l5, [%l5] ASI_DMMU_DEMAP	! Demap it
	membar	#Sync
	cmp	%l5, %l3
	bleu	0b
	 add	%l5, %l4, %l5

	set	(1<<14)-8, %o0			! Clear out DCACHE
1:
	stxa	%g0, [%o0] ASI_DCACHE_TAG	! clear DCACHE line
	membar	#Sync
	brnz,pt	%o0, 1b
	 dec	8, %o0
	
	set	TLB_TAG_ACCESS, %l5
#ifdef NO_VCACHE
	or	%l1, TTE_L|TTE_CP|TTE_P|TTE_W, %l2	! And low bits:	L=1|CP=1|CV=0(ugh)|E=0|P=1|W=1(ugh)|G=0
#else
	or	%l1, TTE_L|TTE_CP|TTE_CV|TTE_P|TTE_W, %l2	! And low bits:	L=1|CP=1|CV=1|E=0|P=1|W=1(ugh)|G=0
#endif
	set	1f, %l7
	stxa	%l0, [%l5] ASI_DMMU		! Same for DMMU
	membar	#Sync				! We may need more membar #Sync in here
	stxa	%l2, [%g0] ASI_DMMU_DATA_IN	! Same for DMMU
	membar	#Sync				! We may need more membar #Sync in here
	flush	%l7				! Make IMMU see this too
1:	
#ifdef NOT_DEBUG
	set	1f, %o0		! Debug printf
	srax	%l0, 32, %o1
	srl	%l0, 0, %o2
	srax	%l2, 32, %o3
	call	_C_LABEL(prom_printf)
	 srl	%l2, 0, %o4
	ba	2f
	 nop
1:
	.asciz	"Setting ITLB entry %08x %08x data %08x %08x\r\n"
	_ALIGN
2:
#endif
#if 1
	!!
	!!  First, map in the kernel as context==1
	!! 
	set	TLB_TAG_ACCESS, %l5
	or	%l1, TTE_CP|TTE_P|TTE_W, %l2	! And low bits:	L=0|CP=1|CV=0|E=0|P=1|W=1(ugh)|G=0
	or	%l0, 1, %l4			! Context = 1
	set	1f, %l7
	stxa	%l4, [%l5] ASI_DMMU		! Make DMMU point to it
	membar	#Sync				! We may need more membar #Sync in here
	stxa	%l2, [%g0] ASI_DMMU_DATA_IN	! Store it
	membar	#Sync				! We may need more membar #Sync in here
	stxa	%l4, [%l5] ASI_IMMU		! Make IMMU point to it
	membar	#Sync				! We may need more membar #Sync in here
	flush	%l0				! Make IMMU see this too
	stxa	%l2, [%g0] ASI_IMMU_DATA_IN	! Store it
	membar	#Sync				! We may need more membar #Sync in here
	flush	%l7				! Make IMMU see this too
1:	
	!!
	!! Now load 1 as primary context
	!!
	mov	1, %l4
	mov	CTX_PRIMARY, %l6
	set	1f, %l7
	stxa	%l4, [%l6] ASI_DMMU
	membar	#Sync				! This probably should be a flush, but it works
	flush	%l7				! This should be KERNBASE
1:	

	!!
	!! Now demap entire context 0 kernel
	!!
	set	0x400000, %l3			! Demap all of kernel immu segment
	or	%l0, 0x020, %l5			! Context = Nucleus
	set	0x2000, %l4			! 8K page size
	add	%l0, %l3, %l3
0:
	stxa	%l5, [%l5] ASI_IMMU_DEMAP	! Demap it
	membar	#Sync
	flush	%l7				! Assume low bits are benign
	cmp	%l5, %l3
	bleu	0b				! Next page
	 add	%l5, %l4, %l5

#endif
	!!
	!!  Now, map in the kernel as context==0
	!! 
	set	TLB_TAG_ACCESS, %l5
#ifdef NO_VCACHE
	or	%l1, TTE_L|TTE_CP|TTE_P|TTE_W, %l2	! And low bits:	L=1|CP=1|CV=0|E=0|P=1|W=1(ugh)|G=0
#else
	or	%l1, TTE_L|TTE_CP|TTE_CV|TTE_P|TTE_W, %l2	! And low bits:	L=1|CP=1|CV=1|E=0|P=1|W=1(ugh)|G=0
#endif
	set	1f, %l7
	stxa	%l0, [%l5] ASI_IMMU		! Make IMMU point to it
	membar	#Sync				! We may need more membar #Sync in here
	stxa	%l2, [%g0] ASI_IMMU_DATA_IN	! Store it
	membar	#Sync				! We may need more membar #Sync in here
	flush	%l7
1:	
	!!
	!! Restore 0 as primary context
	!!
	mov	CTX_PRIMARY, %l6
	set	1f, %l7
	stxa	%g0, [%l6] ASI_DMMU
	membar	#Sync					! No real reason for this XXXX
	flush	%l7
1:	
	!!
	!! Now demap context 1
	!! 
	mov	1, %l4
	mov	CTX_SECONDARY, %l6
	stxa	%l4, [%l6] ASI_DMMU
	membar	#Sync				! This probably should be a flush, but it works
	flush	%l0
	set	0x030, %l4
	stxa	%l4, [%l4] ASI_DMMU_DEMAP
	membar	#Sync
	stxa	%l4, [%l4] ASI_IMMU_DEMAP
	membar	#Sync
	flush	%l0
	stxa	%g0, [%l6] ASI_DMMU
	membar	#Sync
	flush	%l0
	
#if 1
	/*
	 * This should be handled in the CPU probe code
	 */
	/*
	 * Step 4: change the trap base register, and install our TSB
	 */
	set	_C_LABEL(tsb), %l0
	ld	[%l0], %l0
	set	_C_LABEL(tsbsize), %l1
	ld	[%l1], %l1
	andn	%l0, 0xfff, %l0			! Mask off size bits
	or	%l0, %l1, %l0			! Make a TSB pointer

#ifdef NOT_DEBUG
	set	1f, %o0		! Debug printf
	srax	%l0, 32, %o1
	call	_C_LABEL(prom_printf)
	 srl	%l0, 0, %o2
	ba	2f
	 nop
1:
	.asciz	"Setting TSB pointer %08x %08x\r\n"
	_ALIGN
2:	
#endif
		
	set	TSB, %l2
	stxa	%l0, [%l2] ASI_IMMU		! Install insn TSB pointer
	membar	#Sync				! We may need more membar #Sync in here
	stxa	%l0, [%l2] ASI_DMMU		! Install data TSB pointer
	membar	#Sync
	set	_C_LABEL(trapbase), %l1
!	wrpr	%l1, 0, %tba			! Now we should be running 100% from our handlers
	call	_C_LABEL(prom_set_trap_table)	! ditto
	 mov	%l1, %o0
	wrpr	%g0, WSTATE_KERN, %wstate

#ifdef NOT_DEBUG
	wrpr	%g0, 1, %tl			! Debug -- start at tl==3 so we'll watchdog
	wrpr	%g0, 0x1ff, %tt			! Debug -- clear out unused trap regs
	wrpr	%g0, 0, %tpc
	wrpr	%g0, 0, %tnpc
	wrpr	%g0, 0, %tstate
#endif
#endif

#ifdef NOT_DEBUG		
	set	1f, %o0		! Debug printf
	srax	%l0, 32, %o1
	call	_C_LABEL(prom_printf)
	 srl	%l0, 0, %o2
	ba	2f
	 nop
1:
	.asciz	"Our trap handler is enabled\r\n"
	_ALIGN
2:
#endif

	
	
	/*
	 * Call main.  This returns to us after loading /sbin/init into
	 * user space.  (If the exec fails, main() does not return.)
	 */
	call	_C_LABEL(main)
	 clr	%o0				! our frame arg is ignored
	NOTREACHED
	
	set	1f, %o0				! Main should never come back here
	call	_C_LABEL(panic)
	 nop
1:
	.asciz	"main() returned\r\n"
	
/*
 * openfirmware(cell* param);
 *
 * OpenFirmware entry point 
 * 
 * If we're running in 32-bit mode we need to convert to a 64-bit stack
 * and 64-bit cells.  The cells we'll allocate off the stack for simplicity.
 */
	.align 8
	.globl	_C_LABEL(openfirmware)
	.proc 1
	FTYPE(openfirmware)
_C_LABEL(openfirmware):
#if 0
	set	panicstr, %o1			! Check if we're panicing
	ld	[%o1], %o1
	brz,pt	%o1, 0f				! if not, continue

	 rdpr	%pstate, %o4			! Else, restore prom state
	flushw					! flushw may be dangerous at this time
	or	%o4, PSTATE_IE, %o5		! Disable interrupts
	wrpr	%o5, 0, %pstate
	
	set	romitsbp, %o2			
	ldx	[%o2], %o1			! Restore TSB pointers
	set	TSB, %o3
	stxa	%o1, [%o3] ASI_IMMU
	membar	#Sync
	set	romdtsbp, %o2
	ldx	[%o2], %o1
	stxa	%o1, [%o3] ASI_DMMU
	membar	#Sync
	
	set	romtrapbase, %o3		! Restore trapbase
	ldx	[%o3], %o1
	wrpr	%o1, 0, %tba
	set	romwstate, %o3			! Restore wstate
	ldx	[%o3], %o1
	wrpr	%o1, 0, %wstate
	wrpr	%o4, 0, %pstate			! Restore interrupt state
0:	
#endif
	andcc	%sp, 1, %g0
	bz,pt	%icc, 1f
	 sethi	%hi(romp), %l7
	
	ld	[%l7+%lo(romp)], %o4		! v9 stack, just load the addr and callit
	save	%sp, -CC64FSZ, %sp
	rdpr	%pil, %i2
	wrpr	%g0, PIL_IMP, %pil
#if 0
!!!
!!! Since prom addresses overlap user addresses
!!! we need to clear out the dcache on the way
!!! in and out of the prom to make sure
!!! there is no prom/user data confusion
!!! 
	call	_C_LABEL(blast_vcache)
	 nop
#endif
	mov	%i0, %o0
	mov	%g1, %l1
	mov	%g2, %l2
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	rdpr	%pstate, %l0
	jmpl	%o4, %o7
	 wrpr	%g0, PSTATE_PROM|PSTATE_IE, %pstate
	wrpr	%l0, %g0, %pstate
	mov	%l1, %g1
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7
#if 0
!!!
!!! Since prom addresses overlap user addresses
!!! we need to clear out the dcache on the way
!!! in and out of the prom to make sure
!!! there is no prom/user data confusion
!!! 
	call	_C_LABEL(blast_vcache)
	 nop
#endif
	wrpr	%i2, 0, %pil
	ret
	 restore	%o0, %g0, %o0

1:						! v8 -- need to screw with stack & params
#ifdef NOTDEF_DEBUG
	mov	%o7, %o5
	call	globreg_check
	 nop
	mov	%o5, %o7
#endif
	save	%sp, -CC64FSZ, %sp			! Get a new 64-bit stack frame
#if 0
	call	_C_LABEL(blast_vcache)
	 nop
#endif
	add	%sp, -BIAS, %sp
	sethi	%hi(romp), %o1
	rdpr	%pstate, %l0
	ld	[%o1+%lo(romp)], %o1		! Do the actual call
	srl	%sp, 0, %sp
	rdpr	%pil, %i2
	mov	%i0, %o0
	mov	%g1, %l1
	mov	%g2, %l2
	mov	%g3, %l3
	wrpr	%g0, PIL_IMP, %pil
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	jmpl	%o1, %o7
	 wrpr	%g0, PSTATE_PROM|PSTATE_IE, %pstate	! Enable 64-bit addresses for the prom
	wrpr	%l0, 0, %pstate
	wrpr	%i2, 0, %pil
#if 0
	call	_C_LABEL(blast_vcache)
	 nop
#endif
	mov	%l1, %g1
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7
	ret
	 restore	%o0, %g0, %o0
	
/*
 * tlb_flush_pte(vaddr_t va, int ctx)
 * 
 * Flush tte from both IMMU and DMMU.
 *
 */
	.align 8
	.globl	_C_LABEL(tlb_flush_pte)
	.proc 1
	FTYPE(tlb_flush_pte)
_C_LABEL(tlb_flush_pte):
#ifdef DEBUG
	set	trapbase, %o4				! Forget any recent TLB misses
	stx	%g0, [%o4]
	stx	%g0, [%o4+16]
#endif
#ifdef DEBUG
	set	pmapdebug, %o3
	lduw	[%o3], %o3
!	movrz	%o1, -1, %o3				! Print on either pmapdebug & PDB_DEMAP or ctx == 0
	btst	0x0020, %o3
	bz,pt	%icc, 2f
	 nop
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	mov	%i1, %o1
	andn	%i0, 0xfff, %o3
	or	%o3, 0x010, %o3
	call	printf
	 mov	%i0, %o2
	restore
	set	KERNBASE, %o4
	brz,pt	%o1, 2f					! If ctx != 0
	 cmp	%o0, %o4				! and va > KERNBASE
	ba	2f
	 tgu	1; nop					! Debugger
1:
	.asciz	"tlb_flush_pte:	demap ctx=%x va=%08x res=%x\r\n"
	_ALIGN
2:	
#endif
	wr	%g0, ASI_DMMU, %asi
	ldxa	[CTX_SECONDARY] %asi, %g1		! Save secondary context
	andn	%o0, 0xfff, %g2				! drop unused va bits
	sethi	%hi(KERNBASE), %o4
	stxa	%o1, [CTX_SECONDARY] %asi		! Insert context to demap
	membar	#Sync
	or	%g2, 0x010, %g2				! Demap page from secondary context only
	stxa	%g2, [%g2] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync
	stxa	%g2, [%g2] ASI_IMMU_DEMAP		! to both TLBs
	membar	#Sync					! No real reason for this XXXX
	flush	%o4
	srl	%g2, 0, %g2				! and make sure it's both 32- and 64-bit entries
	stxa	%g2, [%g2] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync
	stxa	%g2, [%g2] ASI_IMMU_DEMAP		! Do the demap
	membar	#Sync					! No real reason for this XXXX
	flush	%o4
	stxa	%g1, [CTX_SECONDARY] %asi		! Restore secondary asi
	membar	#Sync					! No real reason for this XXXX
	flush	%o4
	retl
	 nop
	
/*
 * tlb_flush_ctx(int ctx)
 * 
 * Flush entire context from both IMMU and DMMU.
 *
 */
	.align 8
	.globl	_C_LABEL(tlb_flush_ctx)
	.proc 1
	FTYPE(tlb_flush_ctx)
_C_LABEL(tlb_flush_ctx):
#ifdef DEBUG
	set	trapbase, %o4				! Forget any recent TLB misses
	stx	%g0, [%o4]
#endif
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	call	printf
	 mov	%i0, %o1
	ba	2f
	 restore
1:	
	.asciz	"tlb_flush_ctx:	context flush of %d attempted\r\n"
	_ALIGN
2:	
#endif
#ifdef DIAGNOSTIC
	brnz,pt	%o0, 2f
	 nop
	set	1f, %o0
	call	panic
	 nop	
1:
	.asciz	"tlb_flush_ctx:	attempted demap of NUCLEUS context\r\n"
	_ALIGN
2:		
#endif
	wr	%g0, ASI_DMMU, %asi
	ldxa	[CTX_SECONDARY] %asi, %g1		! Save secondary context
	sethi	%hi(KERNBASE), %o4
	stxa	%o0, [CTX_SECONDARY] %asi		! Insert context to demap
	membar	#Sync
	set	0x030, %g2				! Demap context from secondary context only
	stxa	%g2, [%g2] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync					! No real reason for this XXXX
	stxa	%g2, [%g2] ASI_IMMU_DEMAP		! Do the demap
	membar	#Sync
	stxa	%g1, [CTX_SECONDARY] %asi		! Restore secondary asi
	membar	#Sync					! No real reason for this XXXX
	flush	%o4
	retl
	 nop
	
/*
 * blast_vcache()
 *
 * Clear out all of both I$ and D$ regardless of contents
 * Does not modify %o0
 *
 */
	.align 8
	.globl	_C_LABEL(blast_vcache)
	.proc 1
	FTYPE(blast_vcache)
_C_LABEL(blast_vcache):
	set	(2*NBPG)-8, %o1
1:	
	stxa	%g0, [%o1] ASI_ICACHE_TAG
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	brnz,pt	%o1, 1b
	 dec	8, %o1
	sethi	%hi(KERNBASE), %o2
	flush	%o2
	retl
	 nop
/*
 * dcache_flush_page()
 *
 * Clear one page from D$.  We should do one for the I$,
 * but it does not alias and is not likely as large a problem.
 *
 */
	.align 8
	.globl	_C_LABEL(dcache_flush_page)
	.proc 1
	FTYPE(dcache_flush_page)
_C_LABEL(dcache_flush_page):
	mov	-1, %g1
	srlx	%o0, 13-2, %g2
	srl	%g1, 3, %g1	! Generate mask for tag: bits [29..2]
	sllx	%g1, 1, %g1
		
	set	(2*NBPG)-8, %o3
1:
	ldxa	[%o3] ASI_DCACHE_TAG, %g3
	xor	%g3, %g2, %g3
	andcc	%g3, %g1, %g0
	bne,pt	%xcc, 2f
	 nop	
	stxa	%g0, [%o3] ASI_DCACHE_TAG
2:	
	brnz,pt	%o3, 1b
	 dec	8, %o3
	sethi	%hi(KERNBASE), %o5
	flush	%o5
	retl
	 nop	

/*
 * The following code is copied to the top of the user stack when each
 * process is exec'ed, and signals are `trampolined' off it.
 *
 * When this code is run, the stack looks like:
 *	[%sp]		64 bytes to which registers can be dumped
 *	[%sp + 64]	signal number (goes in %o0)
 *	[%sp + 64 + 4]	signal code (goes in %o1)
 *	[%sp + 64 + 8]	placeholder
 *	[%sp + 64 + 12]	argument for %o3, currently unsupported (always 0)
 *	[%sp + 64 + 16]	first word of saved state (sigcontext)
 *	    .
 *	    .
 *	    .
 *	[%sp + NNN]	last word of saved state
 * (followed by previous stack contents or top of signal stack).
 * The address of the function to call is in %g1; the old %g1 and %o0
 * have already been saved in the sigcontext.  We are running in a clean
 * window, all previous windows now being saved to the stack.
 *
 * Note that [%sp + 64 + 8] == %sp + 64 + 16.  The copy at %sp+64+8
 * will eventually be removed, with a hole left in its place, if things
 * work out.
 */
	.globl	_C_LABEL(sigcode)
	.globl	_C_LABEL(esigcode)
_C_LABEL(sigcode):
	/*
	 * XXX  the `save' and `restore' below are unnecessary: should
	 *	replace with simple arithmetic on %sp
	 *
	 * Make room on the stack for 32 %f registers + %fsr.  This comes
	 * out to 33*4 or 132 bytes, but this must be aligned to a multiple
	 * of 8, or 136 bytes.
	 */
	save	%sp, -CCFSZ - 136, %sp
	mov	%g2, %l2		! save globals in %l registers
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	/*
	 * Saving the fpu registers is expensive, so do it iff it is
	 * enabled and dirty.  We only deal w/lower 32 regs
	 */
	rd	%fprs, %l0
	btst	1, %l0			! test dl
	bz,pt	%icc, 1f
	 rd	%y, %l1			! in any case, save %y

	! fpu is enabled, oh well
	st	%fsr, [%sp + CCFSZ + 0]
	std	%f0, [%sp + CCFSZ + 8]
	std	%f2, [%sp + CCFSZ + 16]
	std	%f4, [%sp + CCFSZ + 24]
	std	%f6, [%sp + CCFSZ + 32]
	std	%f8, [%sp + CCFSZ + 40]
	std	%f10, [%sp + CCFSZ + 48]
	std	%f12, [%sp + CCFSZ + 56]
	std	%f14, [%sp + CCFSZ + 64]
	std	%f16, [%sp + CCFSZ + 72]
	std	%f18, [%sp + CCFSZ + 80]
	std	%f20, [%sp + CCFSZ + 88]
	std	%f22, [%sp + CCFSZ + 96]
	std	%f24, [%sp + CCFSZ + 104]
	std	%f26, [%sp + CCFSZ + 112]
	std	%f28, [%sp + CCFSZ + 120]
	std	%f30, [%sp + CCFSZ + 128]
1:
	ldd	[%fp + 64], %o0		! sig, code
	ld	[%fp + 76], %o3		! arg3
	call	%g1			! (*sa->sa_handler)(sig,code,scp,arg3)
	 add	%fp, 64 + 16, %o2	! scp

	/*
	 * Now that the handler has returned, re-establish all the state
	 * we just saved above, then do a sigreturn.
	 */
	btst	1, %l0			! test dl
	bz,pt	%icc, 1f
	 wr	%l1, %g0, %y		! in any case, restore %y

	ld	[%sp + CCFSZ + 0], %fsr
	ldd	[%sp + CCFSZ + 8], %f0
	ldd	[%sp + CCFSZ + 16], %f2
	ldd	[%sp + CCFSZ + 24], %f4
	ldd	[%sp + CCFSZ + 32], %f6
	ldd	[%sp + CCFSZ + 40], %f8
	ldd	[%sp + CCFSZ + 48], %f10
	ldd	[%sp + CCFSZ + 56], %f12
	ldd	[%sp + CCFSZ + 64], %f14
	ldd	[%sp + CCFSZ + 72], %f16
	ldd	[%sp + CCFSZ + 80], %f18
	ldd	[%sp + CCFSZ + 88], %f20
	ldd	[%sp + CCFSZ + 96], %f22
	ldd	[%sp + CCFSZ + 104], %f24
	ldd	[%sp + CCFSZ + 112], %f26
	ldd	[%sp + CCFSZ + 120], %f28
	ldd	[%sp + CCFSZ + 128], %f30

1:
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7

	restore	%g0, SYS_sigreturn, %g1	! get registers back & set syscall #
	add	%sp, 64 + 16, %o0	! compute scp
	t	ST_SYSCALL		! sigreturn(scp)
	! sigreturn does not return unless it fails
	mov	SYS_exit, %g1		! exit(errno)
	t	ST_SYSCALL
_C_LABEL(esigcode):

#ifdef COMPAT_SVR4
/*
 * The following code is copied to the top of the user stack when each
 * process is exec'ed, and signals are `trampolined' off it.
 *
 * When this code is run, the stack looks like:
 *	[%sp]		64 bytes to which registers can be dumped
 *	[%sp + 64]	signal number (goes in %o0)
 *	[%sp + 64 + 4]	pointer to saved siginfo
 *	[%sp + 64 + 8]	pointer to saved context
 *	[%sp + 64 + 12]	address of the user's handler
 *	[%sp + 64 + 16]	first word of saved state (context)
 *	    .
 *	    .
 *	    .
 *	[%sp + NNN]	last word of saved state (siginfo)
 * (followed by previous stack contents or top of signal stack).
 * The address of the function to call is in %g1; the old %g1 and %o0
 * have already been saved in the sigcontext.  We are running in a clean
 * window, all previous windows now being saved to the stack.
 *
 * Note that [%sp + 64 + 8] == %sp + 64 + 16.  The copy at %sp+64+8
 * will eventually be removed, with a hole left in its place, if things
 * work out.
 */
	.globl	_C_LABEL(svr4_sigcode)
	.globl	_C_LABEL(svr4_esigcode)
_C_LABEL(svr4_sigcode):
	/*
	 * XXX  the `save' and `restore' below are unnecessary: should
	 *	replace with simple arithmetic on %sp
	 *
	 * Make room on the stack for 32 %f registers + %fsr.  This comes
	 * out to 33*4 or 132 bytes, but this must be aligned to a multiple
	 * of 8, or 136 bytes.
	 */
	save	%sp, -CCFSZ - 136, %sp
	mov	%g2, %l2		! save globals in %l registers
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	/*
	 * Saving the fpu registers is expensive, so do it iff the fsr
	 * stored in the sigcontext shows that the fpu is enabled.
	 */
	ld	[%fp + 64 + 16 + SC_PSR_OFFSET], %l0
	sethi	%hi(PSR_EF), %l1	! FPU enable bit is too high for andcc
	andcc	%l0, %l1, %l0		! %l0 = fpu enable bit
	be	1f			! if not set, skip the saves
	 rd	%y, %l1			! in any case, save %y

	! fpu is enabled, oh well
	st	%fsr, [%sp + CCFSZ + 0]
	std	%f0, [%sp + CCFSZ + 8]
	std	%f2, [%sp + CCFSZ + 16]
	std	%f4, [%sp + CCFSZ + 24]
	std	%f6, [%sp + CCFSZ + 32]
	std	%f8, [%sp + CCFSZ + 40]
	std	%f10, [%sp + CCFSZ + 48]
	std	%f12, [%sp + CCFSZ + 56]
	std	%f14, [%sp + CCFSZ + 64]
	std	%f16, [%sp + CCFSZ + 72]
	std	%f18, [%sp + CCFSZ + 80]
	std	%f20, [%sp + CCFSZ + 88]
	std	%f22, [%sp + CCFSZ + 96]
	std	%f24, [%sp + CCFSZ + 104]
	std	%f26, [%sp + CCFSZ + 112]
	std	%f28, [%sp + CCFSZ + 120]
	std	%f30, [%sp + CCFSZ + 128]

1:
	ldd	[%fp + 64], %o0		! sig, siginfo
	ld	[%fp + 72], %o2		! uctx
	call	%g1			! (*sa->sa_handler)(sig,siginfo,uctx)
	 nop

	/*
	 * Now that the handler has returned, re-establish all the state
	 * we just saved above, then do a sigreturn.
	 */
	tst	%l0			! reload fpu registers?
	be	1f			! if not, skip the loads
	 wr	%l1, %g0, %y		! in any case, restore %y

	ld	[%sp + CCFSZ + 0], %fsr
	ldd	[%sp + CCFSZ + 8], %f0
	ldd	[%sp + CCFSZ + 16], %f2
	ldd	[%sp + CCFSZ + 24], %f4
	ldd	[%sp + CCFSZ + 32], %f6
	ldd	[%sp + CCFSZ + 40], %f8
	ldd	[%sp + CCFSZ + 48], %f10
	ldd	[%sp + CCFSZ + 56], %f12
	ldd	[%sp + CCFSZ + 64], %f14
	ldd	[%sp + CCFSZ + 72], %f16
	ldd	[%sp + CCFSZ + 80], %f18
	ldd	[%sp + CCFSZ + 88], %f20
	ldd	[%sp + CCFSZ + 96], %f22
	ldd	[%sp + CCFSZ + 104], %f24
	ldd	[%sp + CCFSZ + 112], %f26
	ldd	[%sp + CCFSZ + 120], %f28
	ldd	[%sp + CCFSZ + 128], %f30

1:
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7

	restore	%g0, SVR4_SYS_context, %g1	! get registers & set syscall #
	mov	1, %o0
	add	%sp, 64 + 16, %o1	! compute ucontextp
	t	ST_SYSCALL		! svr4_context(1, ucontextp)
	! setcontext does not return unless it fails
	mov	SYS_exit, %g1		! exit(errno)
	t	ST_SYSCALL
_C_LABEL(svr4_esigcode):
#endif

/*
 * Primitives
 */
#ifdef ENTRY
#undef ENTRY
#endif
	
#ifdef GPROF
	.globl	mcount
#define	ENTRY(x) \
	.globl _C_LABEL(x); _C_LABEL(x): ; \
	save	%sp, -CC64FSZ, %sp; \
	call	mcount; \
	nop; \
	restore
#else
#define	ENTRY(x)	.globl _C_LABEL(x); _C_LABEL(x):
#endif
#define	ALTENTRY(x)	.globl _C_LABEL(x); _C_LABEL(x):

/*
 * getfp() - get stack frame pointer
 */
ENTRY(getfp)
	retl
	 mov %fp, %o0

/*
 * copyinstr(fromaddr, toaddr, maxlength, &lencopied)
 *
 * Copy a null terminated string from the user address space into
 * the kernel address space.
 */
ENTRY(copyinstr)
	! %o0 = fromaddr, %o1 = toaddr, %o2 = maxlen, %o3 = &lencopied
#ifdef DIAGNOSTIC
	tst	%o2			! kernel should never give maxlen <= 0
	ble	1f
	 EMPTY
#endif
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	8f, %o0
	mov	%i0, %o1
	mov	%i1, %o2
	mov	%i2, %o3
	call	printf
	 mov	%i3, %o4
	ba	9f
	 restore
8:	.asciz	"copyinstr: from=%x to=%x max=%x &len=%x\n"
	_ALIGN
9:	
#endif
	sethi	%hi(_C_LABEL(cpcb)), %o4		! (first instr of copy)
	ld	[%o4 + %lo(_C_LABEL(cpcb))], %o4	! catch faults
	set	Lcsfault, %o5
	st	%o5, [%o4 + PCB_ONFAULT]

	mov	%o1, %o5		!	save = toaddr;
! XXX should do this in bigger chunks when possible
0:					! loop:
	ldsba	[%o0] ASI_AIUS, %g1	!	c = *fromaddr;
	stb	%g1, [%o1]		!	*toaddr++ = c;
	inc	%o1
	brz,a,pn	%g1, Lcsdone	!	if (c == NULL)
	 clr	%o0			!		{ error = 0; done; }
	deccc	%o2			!	if (--len > 0) {
	bg,pt	%icc, 0b		!		fromaddr++;
	 inc	%o0			!		goto loop;
	ba,pt	%xcc,Lcsdone		!	}
	 mov	ENAMETOOLONG, %o0	!	error = ENAMETOOLONG;
	NOTREACHED
1:
	sethi	%hi(2f), %o0
	call	_C_LABEL(panic)
	 or	%lo(2f), %o0, %o0
2:	.asciz	"copyinstr"
	_ALIGN

/*
 * copyoutstr(fromaddr, toaddr, maxlength, &lencopied)
 *
 * Copy a null terminated string from the kernel
 * address space to the user address space.
 */
ENTRY(copyoutstr)
	! %o0 = fromaddr, %o1 = toaddr, %o2 = maxlen, %o3 = &lencopied
#ifdef DIAGNOSTIC
	tst	%o2
	ble	2f
	 EMPTY
#endif
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	8f, %o0
	mov	%i0, %o1
	mov	%i1, %o2
	mov	%i2, %o3
	call	printf
	 mov	%i3, %o4
	ba	9f
	 restore
8:	.asciz	"copyoutstr: from=%x to=%x max=%x &len=%x\n"
	_ALIGN
9:	
#endif
	sethi	%hi(_C_LABEL(cpcb)), %o4		! (first instr of copy)
	ld	[%o4 + %lo(_C_LABEL(cpcb))], %o4	! catch faults
	set	Lcsfault, %o5
	st	%o5, [%o4 + PCB_ONFAULT]

	mov	%o1, %o5		!	save = toaddr;
! XXX should do this in bigger chunks when possible
0:					! loop:
	ldsb	[%o0], %g1		!	c = *fromaddr;
	stba	%g1, [%o1] ASI_AIUS	!	*toaddr++ = c;
	inc	%o1
	brz,a,pn	%g1, Lcsdone	!	if (c == NULL)
	 clr	%o0			!		{ error = 0; done; }
	deccc	%o2			!	if (--len > 0) {
	bg,pt	%icc, 0b		!		fromaddr++;
	 inc	%o0			!		goto loop;
					!	}
	mov	ENAMETOOLONG, %o0	!	error = ENAMETOOLONG;
Lcsdone:				! done:
	sub	%o1, %o5, %o1		!	len = to - save;
	brnz,a	%o3, 1f			!	if (lencopied)
	 st	%o1, [%o3]		!		*lencopied = len;
1:
	retl				! cpcb->pcb_onfault = 0;
	 st	%g0, [%o4 + PCB_ONFAULT]! return (error);

Lcsfault:
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	5f, %o0
	call	printf
	 nop
	ba	6f
	 restore
5:	.asciz	"Lcsfault: recovering\n"
	_ALIGN
6:	
#endif
	b	Lcsdone			! error = EFAULT;
	 mov	EFAULT, %o0		! goto ret;

2:
	sethi	%hi(3f), %o0
	call	_C_LABEL(panic)
	 or	%lo(3f), %o0, %o0
3:	.asciz	"copyoutstr"
	_ALIGN


/*
 * copystr(fromaddr, toaddr, maxlength, &lencopied)
 *
 * Copy a null terminated string from one point to another in
 * the kernel address space.  (This is a leaf procedure, but
 * it does not seem that way to the C compiler.)
 */
ENTRY(copystr)
#ifdef DIAGNOSTIC
	tst	%o2			! 	if (maxlength <= 0)
	ble	4f			!		panic(...);
	 EMPTY
#endif
	mov	%o1, %o5		!	to0 = to;
0:					! loop:
	ldsb	[%o0], %o4		!	c = *from;
	tst	%o4
	stb	%o4, [%o1]		!	*to++ = c;
	be	1f			!	if (c == 0)
	 inc	%o1			!		goto ok;
	deccc	%o2			!	if (--len > 0) {
	bg,a	0b			!		from++;
	 inc	%o0			!		goto loop;
	b	2f			!	}
	 mov	ENAMETOOLONG, %o0	!	ret = ENAMETOOLONG; goto done;
1:					! ok:
	clr	%o0			!	ret = 0;
2:
	sub	%o1, %o5, %o1		!	len = to - to0;
	tst	%o3			!	if (lencopied)
	bnz,a	3f
	 st	%o1, [%o3]		!		*lencopied = len;
3:
	retl
	 nop
#ifdef DIAGNOSTIC
4:
	sethi	%hi(5f), %o0
	call	_C_LABEL(panic)
	 or	%lo(5f), %o0, %o0
5:
	.asciz	"copystr"
	_ALIGN
#endif

/*
 * Copyin(src, dst, len)
 *
 * Copy specified amount of data from user space into the kernel.
 *
 * This is a modified version of bcopy that uses ASI_AIUS.  When
 * bcopy is optimized to use block copy ASIs, this should be also.
 */

#define	BCOPY_SMALL	32	/* if < 32, copy by bytes */

ENTRY(copyin)
!	flushw			! Make sure we don't have stack probs & lose hibits of %o
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	mov	%i0, %o1
	mov	%i1, %o2
	call	printf
	 mov	%i2, %o3
	ba	2f
	 restore
1:	.asciz	"copyin: src=%x dest=%x len=%x\n"
	_ALIGN
2:	
#endif
	sethi	%hi(_C_LABEL(cpcb)), %o3
	wr	%g0, ASI_AIUS, %asi
	ld	[%o3 + %lo(_C_LABEL(cpcb))], %o3
	set	Lcopyfault, %o4
!	mov	%o7, %g7		! save return address
	st	%o4, [%o3 + PCB_ONFAULT]
	cmp	%o2, BCOPY_SMALL
Lcopyin_start:
	bge,a	Lcopyin_fancy	! if >= this many, go be fancy.
	 btst	7, %o0		! (part of being fancy)

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
0:
	 inc	%o0
	ldsba	[%o0 - 1] %asi, %o4!	*dst++ = (++src)[-1];
	stb	%o4, [%o1]
	deccc	%o2
	bge	0b
	 inc	%o1
1:
	ba	Lcopyin_done
	 clr	%o0
	NOTREACHED

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
Lcopyin_fancy:
	! check for common case first: everything lines up.
!	btst	7, %o0		! done already
	bne	1f
	 EMPTY
	btst	7, %o1
	be,a	Lcopyin_doubles
	 dec	8, %o2		! if all lined up, len -= 8, goto copyin_doubes

	! If the low bits match, we can make these line up.
1:
	xor	%o0, %o1, %o3	! t = src ^ dst;
	btst	1, %o3		! if (t & 1) {
	be,a	1f
	 btst	1, %o0		! [delay slot: if (src & 1)]

	! low bits do not match, must copy by bytes.
0:
	ldsba	[%o0] %asi, %o4	!	do {
	inc	%o0		!		(++dst)[-1] = *src++;
	inc	%o1
	deccc	%o2
	bnz	0b		!	} while (--len != 0);
	 stb	%o4, [%o1 - 1]
	ba	Lcopyin_done
	 clr	%o0
	NOTREACHED

	! lowest bit matches, so we can copy by words, if nothing else
1:
	be,a	1f		! if (src & 1) {
	 btst	2, %o3		! [delay slot: if (t & 2)]

	! although low bits match, both are 1: must copy 1 byte to align
	ldsba	[%o0] %asi, %o4	!	*dst++ = *src++;
	stb	%o4, [%o1]
	inc	%o0
	inc	%o1
	dec	%o2		!	len--;
	btst	2, %o3		! } [if (t & 2)]
1:
	be,a	1f		! if (t & 2) {
	 btst	2, %o0		! [delay slot: if (src & 2)]
	dec	2, %o2		!	len -= 2;
0:
	ldsha	[%o0] %asi, %o4	!	do {
	sth	%o4, [%o1]	!		*(short *)dst = *(short *)src;
	inc	2, %o0		!		dst += 2, src += 2;
	deccc	2, %o2		!	} while ((len -= 2) >= 0);
	bge	0b
	 inc	2, %o1
	b	Lcopyin_mopb	!	goto mop_up_byte;
	 btst	1, %o2		! } [delay slot: if (len & 1)]
	NOTREACHED

	! low two bits match, so we can copy by longwords
1:
	be,a	1f		! if (src & 2) {
	 btst	4, %o3		! [delay slot: if (t & 4)]

	! although low 2 bits match, they are 10: must copy one short to align
	ldsha	[%o0] %asi, %o4	!	(*short *)dst = *(short *)src;
	sth	%o4, [%o1]
	inc	2, %o0		!	dst += 2;
	inc	2, %o1		!	src += 2;
	dec	2, %o2		!	len -= 2;
	btst	4, %o3		! } [if (t & 4)]
1:
	be,a	1f		! if (t & 4) {
	 btst	4, %o0		! [delay slot: if (src & 4)]
	dec	4, %o2		!	len -= 4;
0:
	lduwa	[%o0] %asi, %o4	!	do {
	st	%o4, [%o1]	!		*(int *)dst = *(int *)src;
	inc	4, %o0		!		dst += 4, src += 4;
	deccc	4, %o2		!	} while ((len -= 4) >= 0);
	bge	0b
	 inc	4, %o1
	b	Lcopyin_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! } [delay slot: if (len & 2)]
	NOTREACHED

	! low three bits match, so we can copy by doublewords
1:
	be	1f		! if (src & 4) {
	 dec	8, %o2		! [delay slot: len -= 8]
	lduwa	[%o0] %asi, %o4	!	*(int *)dst = *(int *)src;
	st	%o4, [%o1]
	inc	4, %o0		!	dst += 4, src += 4, len -= 4;
	inc	4, %o1
	dec	4, %o2		! }
1:
Lcopyin_doubles:
	ldxa	[%o0] %asi, %g1	! do {
	stx	%g1, [%o1]	!	*(double *)dst = *(double *)src;
	inc	8, %o0		!	dst += 8, src += 8;
	deccc	8, %o2		! } while ((len -= 8) >= 0);
	bge	Lcopyin_doubles
	 inc	8, %o1

	! check for a usual case again (save work)
	btst	7, %o2		! if ((len & 7) == 0)
	be	Lcopyin_done	!	goto copyin_done;

	 btst	4, %o2		! if ((len & 4)) == 0)
	be,a	Lcopyin_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! [delay slot: if (len & 2)]
	lduwa	[%o0] %asi, %o4	!	*(int *)dst = *(int *)src;
	st	%o4, [%o1]
	inc	4, %o0		!	dst += 4;
	inc	4, %o1		!	src += 4;
	btst	2, %o2		! } [if (len & 2)]

1:
	! mop up trailing word (if present) and byte (if present).
Lcopyin_mopw:
	be	Lcopyin_mopb	! no word, go mop up byte
	 btst	1, %o2		! [delay slot: if (len & 1)]
	ldsha	[%o0] %asi, %o4	! *(short *)dst = *(short *)src;
	be	Lcopyin_done	! if ((len & 1) == 0) goto done;
	 sth	%o4, [%o1]
	ldsba	[%o0 + 2] %asi, %o4	! dst[2] = src[2];
	stb	%o4, [%o1 + 2]
	ba	Lcopyin_done
	 clr	%o0
	NOTREACHED

	! mop up trailing byte (if present).
Lcopyin_mopb:
	be,a	Lcopyin_done
	 nop
	ldsba	[%o0] %asi, %o4
	stb	%o4, [%o1]
	
Lcopyin_done:
	sethi	%hi(_C_LABEL(cpcb)), %o3
!	stb	%o4,[%o1]	! Store last byte -- should not be needed
	ld	[%o3 + %lo(_C_LABEL(cpcb))], %o3
	st	%g0, [%o3 + PCB_ONFAULT]
	retl
	 clr	%o0			! return 0

/*
 * Copyout(src, dst, len)
 *
 * Copy specified amount of data from kernel to user space.
 * Just like copyin, except that the `dst' addresses are user space
 * rather than the `src' addresses.
 *
 * This is a modified version of bcopy that uses ASI_AIUS.  When
 * bcopy is optimized to use block copy ASIs, this should be also.
 */
 /*
  * This needs to be reimplemented to really do the copy.
  */
ENTRY(copyout)
	/*
	 * ******NOTE****** this depends on bcopy() not using %g7
	 */
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	mov	%i0, %o1
	set	CTX_SECONDARY, %o4
	mov	%i1, %o2
	ldxa	[%o4] ASI_DMMU, %o4
	call	printf
	 mov	%i2, %o3
	ba	Ldocopy
	 restore
1:	.asciz	"copyout: src=%x dest=%x len=%x ctx=%d\n"
	_ALIGN
#endif
Ldocopy:
	sethi	%hi(_C_LABEL(cpcb)), %o3
	wr	%g0, ASI_AIUS, %asi
	ld	[%o3 + %lo(_C_LABEL(cpcb))], %o3
	set	Lcopyfault, %o4
!	mov	%o7, %g7		! save return address
	st	%o4, [%o3 + PCB_ONFAULT]
	cmp	%o2, BCOPY_SMALL
Lcopyout_start:
	bge,a	Lcopyout_fancy	! if >= this many, go be fancy.
	 btst	7, %o0		! (part of being fancy)

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
	 EMPTY
0:
	inc	%o0
	ldsb	[%o0 - 1], %o4!	(++dst)[-1] = *src++;
	stba	%o4, [%o1] %asi
	deccc	%o2
	bge	0b
	 inc	%o1
1:
	ba	Lcopyout_done
	 clr	%o0
	NOTREACHED

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
Lcopyout_fancy:
	! check for common case first: everything lines up.
!	btst	7, %o0		! done already
	bne	1f
	 EMPTY
	btst	7, %o1
	be,a	Lcopyout_doubles
	 dec	8, %o2		! if all lined up, len -= 8, goto copyout_doubes

	! If the low bits match, we can make these line up.
1:
	xor	%o0, %o1, %o3	! t = src ^ dst;
	btst	1, %o3		! if (t & 1) {
	be,a	1f
	 btst	1, %o0		! [delay slot: if (src & 1)]

	! low bits do not match, must copy by bytes.
0:
	ldsb	[%o0], %o4	!	do {
	inc	%o0		!		(++dst)[-1] = *src++;
	inc	%o1
	deccc	%o2
	bnz	0b		!	} while (--len != 0);
	 stba	%o4, [%o1 - 1] %asi
	ba	Lcopyout_done
	 clr	%o0
	NOTREACHED

	! lowest bit matches, so we can copy by words, if nothing else
1:
	be,a	1f		! if (src & 1) {
	 btst	2, %o3		! [delay slot: if (t & 2)]

	! although low bits match, both are 1: must copy 1 byte to align
	ldsb	[%o0], %o4	!	*dst++ = *src++;
	stba	%o4, [%o1] %asi
	inc	%o0
	inc	%o1
	dec	%o2		!	len--;
	btst	2, %o3		! } [if (t & 2)]
1:
	be,a	1f		! if (t & 2) {
	 btst	2, %o0		! [delay slot: if (src & 2)]
	dec	2, %o2		!	len -= 2;
0:
	ldsh	[%o0], %o4	!	do {
	stha	%o4, [%o1] %asi	!		*(short *)dst = *(short *)src;
	inc	2, %o0		!		dst += 2, src += 2;
	deccc	2, %o2		!	} while ((len -= 2) >= 0);
	bge	0b
	 inc	2, %o1
	b	Lcopyout_mopb	!	goto mop_up_byte;
	 btst	1, %o2		! } [delay slot: if (len & 1)]
	NOTREACHED

	! low two bits match, so we can copy by longwords
1:
	be,a	1f		! if (src & 2) {
	 btst	4, %o3		! [delay slot: if (t & 4)]

	! although low 2 bits match, they are 10: must copy one short to align
	ldsh	[%o0], %o4	!	(*short *)dst = *(short *)src;
	stha	%o4, [%o1] %asi
	inc	2, %o0		!	dst += 2;
	inc	2, %o1		!	src += 2;
	dec	2, %o2		!	len -= 2;
	btst	4, %o3		! } [if (t & 4)]
1:
	be,a	1f		! if (t & 4) {
	 btst	4, %o0		! [delay slot: if (src & 4)]
	dec	4, %o2		!	len -= 4;
0:
	lduw	[%o0], %o4	!	do {
	sta	%o4, [%o1] %asi	!		*(int *)dst = *(int *)src;
	inc	4, %o0		!		dst += 4, src += 4;
	deccc	4, %o2		!	} while ((len -= 4) >= 0);
	bge	0b
	 inc	4, %o1
	b	Lcopyout_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! } [delay slot: if (len & 2)]
	NOTREACHED

	! low three bits match, so we can copy by doublewords
1:
	be	1f		! if (src & 4) {
	 dec	8, %o2		! [delay slot: len -= 8]
	lduw	[%o0], %o4	!	*(int *)dst = *(int *)src;
	sta	%o4, [%o1] %asi
	inc	4, %o0		!	dst += 4, src += 4, len -= 4;
	inc	4, %o1
	dec	4, %o2		! }
1:
Lcopyout_doubles:
	ldx	[%o0], %g1	! do {
	stxa	%g1, [%o1] %asi	!	*(double *)dst = *(double *)src;
	inc	8, %o0		!	dst += 8, src += 8;
	deccc	8, %o2		! } while ((len -= 8) >= 0);
	bge	Lcopyout_doubles
	 inc	8, %o1

	! check for a usual case again (save work)
	btst	7, %o2		! if ((len & 7) == 0)
	be	Lcopyout_done	!	goto copyout_done;

	 btst	4, %o2		! if ((len & 4)) == 0)
	be,a	Lcopyout_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! [delay slot: if (len & 2)]
	lduw	[%o0], %o4	!	*(int *)dst = *(int *)src;
	sta	%o4, [%o1] %asi
	inc	4, %o0		!	dst += 4;
	inc	4, %o1		!	src += 4;
	btst	2, %o2		! } [if (len & 2)]

1:
	! mop up trailing word (if present) and byte (if present).
Lcopyout_mopw:
	be	Lcopyout_mopb	! no word, go mop up byte
	 btst	1, %o2		! [delay slot: if (len & 1)]
	ldsh	[%o0], %o4	! *(short *)dst = *(short *)src;
	be	Lcopyout_done	! if ((len & 1) == 0) goto done;
	 stha	%o4, [%o1] %asi
	ldsb	[%o0 + 2], %o4	! dst[2] = src[2];
	stba	%o4, [%o1 + 2] %asi
	ba	Lcopyout_done
	 clr	%o0
	NOTREACHED

	! mop up trailing byte (if present).
Lcopyout_mopb:
	be,a	Lcopyout_done
	 nop
	ldsb	[%o0], %o4
	stba	%o4, [%o1] %asi
	
Lcopyout_done:
	sethi	%hi(_C_LABEL(cpcb)), %o3
	ld	[%o3 + %lo(_C_LABEL(cpcb))], %o3
	st	%g0, [%o3 + PCB_ONFAULT]
!	jmp	%g7 + 8		! Original instr
	retl			! New instr
	 clr	%o0			! return 0

! Copyin or copyout fault.  Clear cpcb->pcb_onfault and return EFAULT.
! Note that although we were in bcopy, there is no state to clean up;
! the only special thing is that we have to return to [g7 + 8] rather than
! [o7 + 8].
Lcopyfault:
	sethi	%hi(_C_LABEL(cpcb)), %o3
	ld	[%o3 + %lo(_C_LABEL(cpcb))], %o3
	st	%g0, [%o3 + PCB_ONFAULT]
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	call	printf
	 nop
	ba	2f
	 restore
1:	.asciz	"copyfault: fault occured\n"
	_ALIGN
2:	
#endif
	retl
	 mov	EFAULT, %o0


#if 0
/*
 * Write all user windows presently in the CPU back to the user's stack.
 */
ENTRY(write_all_windows)
#if 0
ALTENTRY(write_user_windows)
	save	%sp, -CC64FSZ, %sp
	flushw
	ret
	 restore
#else
ENTRY(write_user_windows)
	retl
	 flushw
#endif
#endif
	
	.comm	_C_LABEL(want_resched),4
/*
 * Masterpaddr is the p->p_addr of the last process on the processor.
 * XXX masterpaddr is almost the same as cpcb
 * XXX should delete this entirely
 */
#if 0
	.comm	_C_LABEL(masterpaddr), 4
#else
	.globl	_C_LABEL(masterpaddr)
_C_LABEL(masterpaddr):
	.word	proc0
#endif

/*
 * Switch statistics (for later tweaking):
 *	nswitchdiff = p1 => p2 (i.e., chose different process)
 *	nswitchexit = number of calls to switchexit()
 *	_cnt.v_swtch = total calls to swtch+swtchexit
 */
	.comm	_C_LABEL(nswitchdiff), 4
	.comm	_C_LABEL(nswitchexit), 4

/*
 * REGISTER USAGE IN cpu_switch AND switchexit:
 * This is split into two phases, more or less
 * `before we locate a new proc' and `after'.
 * Some values are the same in both phases.
 * Note that the %o0-registers are not preserved across
 * the psr change when entering a new process, since this
 * usually changes the CWP field (hence heavy usage of %g's).
 *
 *	%g1 = <free>; newpcb -- WARNING this register tends to get trashed
 *	%g2 = %hi(_whichqs); newpsr
 *	%g3 = p
 *	%g4 = lastproc
 *	%g5 = oldpsr (excluding ipl bits)
 *	%g6 = %hi(cpcb)
 *	%g7 = %hi(curproc)
 *	%o0 = tmp 1
 *	%o1 = tmp 2
 *	%o2 = tmp 3
 *	%o3 = tmp 4; whichqs; vm
 *	%o4 = tmp 4; which; sswap
 *	%o5 = tmp 5; q; <free>
 */

/*
 * switchexit is called only from cpu_exit() before the current process
 * has freed its kernel stack; we must free it.  (curproc is already NULL.)
 *
 * We lay the process to rest by changing to the `idle' kernel stack,
 * and note that the `last loaded process' is nonexistent.
 */
ENTRY(switchexit)
	flushw				! We don't have anything else to run, so why not
#ifdef DEBUG
	save	%sp, -CC64FSZ, %sp
	flushw
	restore
#endif
	wrpr	%g0, PSTATE_KERN, %pstate ! Make sure we're on the right globals
	mov	%o0, %g2		! save the
	mov	%o1, %g3		! ... three parameters
	mov	%o2, %g4		! ... to kmem_free

#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	call	prom_printf
	 nop
	LOCTOGLOB
	ba	2f
	 restore
1:	.asciz	"switchexit()\r\n"
	_ALIGN
2:
#endif
	/*
	 * Change pcb to idle u. area, i.e., set %sp to top of stack
	 * and %psr to PSR_S|PSR_ET, and set cpcb to point to _idle_u.
	 * Once we have left the old stack, we can call kmem_free to
	 * destroy it.  Call it any sooner and the register windows
	 * go bye-bye.
	 */
	set	_C_LABEL(idle_u), %g1
	sethi	%hi(_C_LABEL(cpcb)), %g6
#if 0
	/* Get rid of the stack	*/
	rdpr	%ver, %o0
	wrpr	%g0, 0, %canrestore	! Fixup window state regs
	and	%o0, 0x0f, %o0
	wrpr	%g0, 0, %otherwin
	wrpr	%g0, %o0, %cleanwin	! kernel don't care, but user does
	dec	1, %o0			! What happens if we don't subtract 2?
	wrpr	%g0, %o0, %cansave
	flushw						! DEBUG
#endif
	
	st	%g1, [%g6 + %lo(_C_LABEL(cpcb))]	! cpcb = &idle_u
	set	_C_LABEL(idle_u) + USPACE-CCFSZ, %o0	! set new %sp
	mov	%o0, %sp		! Maybe this should be a save?
	save	%sp,-CC64FSZ,%sp	! Get an extra frame for good measure
	flushw				! DEBUG this should not be needed
	wrpr	%g0, 0, %canrestore
	wrpr	%g0, 0, %otherwin
	rdpr	%ver, %l7
	and	%l7, CWP, %l7
	wrpr	%l7, 0, %cleanwin
	dec	1, %l7					! NWINDOWS-1-1
	wrpr	%l7, %cansave
	flushw						! DEBUG
#ifdef DEBUG
	set	_C_LABEL(idle_u), %l6
	SET_SP_REDZONE(%l6, %l5)
#endif
	wrpr	%g0, PSTATE_INTR, %pstate	! and then enable traps
	mov	%g2, %o0		! now ready to call kmem_free
	mov	%g3, %o1
#if defined(UVM)
	call	_C_LABEL(uvm_km_free)
#else
	call	_C_LABEL(kmem_free)
#endif
	 mov	%g4, %o2

	/*
	 * Now fall through to `the last switch'.  %g6 was set to
	 * %hi(cpcb), but may have been clobbered in kmem_free,
	 * so all the registers described below will be set here.
	 *
	 * REGISTER USAGE AT THIS POINT:
	 *	%g2 = %hi(_whichqs)
	 *	%g4 = lastproc
	 *	%g5 = oldpsr (excluding ipl bits)
	 *	%g6 = %hi(cpcb)
	 *	%g7 = %hi(curproc)
	 *	%o0 = tmp 1
	 *	%o1 = tmp 2
	 *	%o3 = whichqs
	 */

	INCR(_C_LABEL(nswitchexit))		! nswitchexit++;
#if defined(UVM)
	INCR(_C_LABEL(uvmexp)+V_SWTCH)		! cnt.v_switch++;
#else
	INCR(_C_LABEL(cnt)+V_SWTCH)		! cnt.v_switch++;
#endif	

	sethi	%hi(_C_LABEL(whichqs)), %g2
	clr	%g4			! lastproc = NULL;
	sethi	%hi(_C_LABEL(cpcb)), %g6
	sethi	%hi(_C_LABEL(curproc)), %g7
	ld	[%g6 + %lo(_C_LABEL(cpcb))], %g5
	wr	%g0, ASI_DMMU, %asi
	ldxa	[CTX_SECONDARY] %asi, %g1	! Don't demap the kernel
	brz,pn	%g1, 1f
	 set	0x030, %g1			! Demap secondary context
	stxa	%g1, [%g1] ASI_DMMU_DEMAP
	stxa	%g1, [%g1] ASI_IMMU_DEMAP
	membar	#Sync
1:	
	stxa	%g0, [CTX_SECONDARY] %asi	! Clear out our context
	membar	#Sync
	/* FALLTHROUGH */

/*
 * When no processes are on the runq, switch
 * idles here waiting for something to come ready.
 * The registers are set up as noted above.
 */
	.globl	idle
idle:
	st	%g0, [%g7 + %lo(_C_LABEL(curproc))] ! curproc = NULL;
1:					! spin reading _whichqs until nonzero
	wrpr	%g0, PSTATE_INTR, %pstate		! Make sure interrupts are enabled
	wrpr	%g0, 0, %pil		! (void) spl0();
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	idlemsg, %o0
	mov	%g1, %o1
	mov	%g2, %o2
	mov	%g3, %o3
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	call	_C_LABEL(prom_printf)
	 mov	%g4, %o4
	set	idlemsg1, %o0
	mov	%l5, %o1
	mov	%l6, %o2
	call	_C_LABEL(prom_printf)
	 mov	%l7, %o3
	LOCTOGLOB
	restore
#endif
	ld	[%g2 + %lo(_C_LABEL(whichqs))], %o3
	brnz,a,pt	%o3, Lsw_scan
	 wrpr	%g0, PIL_CLOCK, %pil	! (void) splclock();
	b	1b

Lsw_panic_rq:
	sethi	%hi(1f), %o0
	call	_C_LABEL(panic)
	 or	%lo(1f), %o0, %o0
Lsw_panic_wchan:
	sethi	%hi(2f), %o0
	call	_C_LABEL(panic)
	 or	%lo(2f), %o0, %o0
Lsw_panic_srun:
	sethi	%hi(3f), %o0
	call	_C_LABEL(panic)
	 or	%lo(3f), %o0, %o0
1:	.asciz	"switch rq"
2:	.asciz	"switch wchan"
3:	.asciz	"switch SRUN"
idlemsg:	.asciz	"idle %x %x %x %x"
idlemsg1:	.asciz	" %x %x %x\r\n"
	_ALIGN

/*
 * cpu_switch() picks a process to run and runs it, saving the current
 * one away.  On the assumption that (since most workstations are
 * single user machines) the chances are quite good that the new
 * process will turn out to be the current process, we defer saving
 * it here until we have found someone to load.  If that someone
 * is the current process we avoid both store and load.
 *
 * cpu_switch() is always entered at splstatclock or splhigh.
 *
 * IT MIGHT BE WORTH SAVING BEFORE ENTERING idle TO AVOID HAVING TO
 * SAVE LATER WHEN SOMEONE ELSE IS READY ... MUST MEASURE!
 *
 * Apparently cpu_switch() is called with curproc as the first argument,
 * but no port seems to make use of that parameter.
 */
	.globl	_C_LABEL(runtime)
	.globl	_C_LABEL(time)
ENTRY(cpu_switch)
	/*
	 * REGISTER USAGE AT THIS POINT:
	 *	%g1 = tmp 0
	 *	%g2 = %hi(_C_LABEL(whichqs))
	 *	%g3 = p
	 *	%g4 = lastproc
	 *	%g5 = cpcb
	 *	%g6 = %hi(_C_LABEL(cpcb))
	 *	%g7 = %hi(_C_LABEL(curproc))
	 *	%o0 = tmp 1
	 *	%o1 = tmp 2
	 *	%o2 = tmp 3
	 *	%o3 = tmp 4, then at Lsw_scan, whichqs
	 *	%o4 = tmp 5, then at Lsw_scan, which
	 *	%o5 = tmp 6, then at Lsw_scan, q
	 */
#ifdef DEBUG
	set	swdebug, %o1
	ld	[%o1], %o1
	brz,pt	%o1, 2f
	 set	1f, %o0
	call	printf
	 nop
	ba	2f
	 nop
1:	.asciz	"s"
	_ALIGN
	.globl	swdebug
swdebug:	.word 0
2:
#endif
#ifdef NOTDEF_DEBUG
	set	intrdebug, %g1
	mov	INTRDEBUG_FUNC, %o1
	st	%o1, [%g1]
#endif
	flushw				! We don't have anything else to run, so why not flush
#ifdef DEBUG
	save	%sp, -CC64FSZ, %sp
	flushw
	restore
#endif
	rdpr	%pstate, %o1		! oldpstate = %pstate;
	wrpr	%g0, PSTATE_INTR, %pstate ! make sure we're on normal globals
	sethi	%hi(_C_LABEL(cpcb)), %g6
	sethi	%hi(_C_LABEL(whichqs)), %g2	! set up addr regs
	ld	[%g6 + %lo(_C_LABEL(cpcb))], %g5
	sethi	%hi(_C_LABEL(curproc)), %g7
	stx	%o7, [%g5 + PCB_PC]	! cpcb->pcb_pc = pc;
	ld	[%g7 + %lo(_C_LABEL(curproc))], %g4	! lastproc = curproc;
	sth	%o1, [%g5 + PCB_PSTATE]	! cpcb->pcb_pstate = oldpstate;

	/*
	 * In all the fiddling we did to get this far, the thing we are
	 * waiting for might have come ready, so let interrupts in briefly
	 * before checking for other processes.  Note that we still have
	 * curproc set---we have to fix this or we can get in trouble with
	 * the run queues below.
	 */
	rdpr	%pil, %g3		! %g3 has not been used yet
	st	%g0, [%g7 + %lo(_C_LABEL(curproc))]	! curproc = NULL;
	wrpr	%g0, 0, %pil			! (void) spl0();
	stb	%g3, [%g5 + PCB_PIL]	! save old %pil
	wrpr	%g0, PIL_CLOCK, %pil	! (void) splclock();

Lsw_scan:
	/*
	 * We're about to run a (possibly) new process.  Set runtime
	 * to indicate its start time.
	 */
	sethi	%hi(_C_LABEL(time)), %o0
	ld	[%o0 + %lo(_C_LABEL(time))], %o2! Need to do this in 2 steps cause time may not be aligned
	ld	[%o0 + %lo(_C_LABEL(time))+4], %o3
	sethi	%hi(_C_LABEL(runtime)), %o0
	st	%o2, [%o0 + %lo(_C_LABEL(runtime))]
	st	%o3, [%o0 + %lo(_C_LABEL(runtime))+4]
	
	ld	[%g2 + %lo(_C_LABEL(whichqs))], %o3

#ifndef POPC
	.globl	ffstab
	/*
	 * Optimized inline expansion of `which = ffs(whichqs) - 1';
	 * branches to idle if ffs(whichqs) was 0.
	 */
	set	ffstab, %o2
	andcc	%o3, 0xff, %o1		! byte 0 zero?
	bz,a,pn	%icc, 1f		! yes, try byte 1
	 srl	%o3, 8, %o0
	ba,pt	%icc, 2f		! ffs = ffstab[byte0]; which = ffs - 1;
	 ldsb	[%o2 + %o1], %o0
1:	andcc	%o0, 0xff, %o1		! byte 1 zero?
	bz,a,pn	%icc, 1f		! yes, try byte 2
	 srl	%o0, 8, %o0
	ldsb	[%o2 + %o1], %o0	! which = ffstab[byte1] + 7;
	ba,pt	%icc, 3f
	 add	%o0, 7, %o4
1:	andcc	%o0, 0xff, %o1		! byte 2 zero?
	bz,a,pn	%icc, 1f		! yes, try byte 3
	 srl	%o0, 8, %o0
	ldsb	[%o2 + %o1], %o0	! which = ffstab[byte2] + 15;
	ba,pt	%icc, 3f
	 add	%o0, 15, %o4
1:	ldsb	[%o2 + %o0], %o0	! ffs = ffstab[byte3] + 24
	addcc	%o0, 24, %o0		! (note that ffstab[0] == -24)
	bz,pn	%icc, idle		! if answer was 0, go idle
	 EMPTY
2:	sub	%o0, 1, %o4
3:	/* end optimized inline expansion */

#else
	/*
	 * Optimized inline expansion of `which = ffs(whichqs) - 1';
	 * branches to idle if ffs(whichqs) was 0.
	 *
	 * This version uses popc.
	 *
	 * XXXX spitfires and blackbirds don't implement popc.
	 *
	 */
	brz,pn	%o3, idle				! Don't bother if queues are empty
	 neg	%o3, %o1				! %o1 = -zz
	xnor	%o3, %o1, %o2				! %o2 = zz ^ ~ -zz
	popc	%o2, %o4				! which = popc(whichqs)
	dec	%o4					! which = ffs(whichqs) - 1

#endif
	/*
	 * We found a nonempty run queue.  Take its first process.
	 */
	set	_C_LABEL(qs), %o5	! q = &qs[which];
	sll	%o4, 3, %o0
	add	%o0, %o5, %o5
	ld	[%o5], %g3		! p = q->ph_link;
	cmp	%g3, %o5		! if (p == q)
	be,pn	%icc, Lsw_panic_rq	!	panic("switch rq");
	 EMPTY
	ld	[%g3], %o0		! tmp0 = p->p_forw;
	st	%o0, [%o5]		! q->ph_link = tmp0;
	st	%o5, [%o0 + 4]		! tmp0->p_back = q;
	cmp	%o0, %o5		! if (tmp0 == q)
	bne	1f
	 EMPTY
	mov	1, %o1			!	whichqs &= ~(1 << which);
	sll	%o1, %o4, %o1
	andn	%o3, %o1, %o3
	st	%o3, [%g2 + %lo(_C_LABEL(whichqs))]
1:
	/*
	 * PHASE TWO: NEW REGISTER USAGE:
	 *	%g1 = newpcb
	 *	%g2 = newpstate
	 *	%g3 = p
	 *	%g4 = lastproc
	 *	%g5 = cpcb
	 *	%g6 = %hi(_cpcb)
	 *	%g7 = %hi(_curproc)
	 *	%o0 = tmp 1
	 *	%o1 = tmp 2
	 *	%o2 = tmp 3
	 *	%o3 = vm
	 *	%o4 = sswap
	 *	%o5 = <free>
	 */

	/* firewalls */
	ld	[%g3 + P_WCHAN], %o0	! if (p->p_wchan)
	brnz,pn	%o0, Lsw_panic_wchan	!	panic("switch wchan");
	 EMPTY
	ldsb	[%g3 + P_STAT], %o0	! if (p->p_stat != SRUN)
	cmp	%o0, SRUN
	bne	Lsw_panic_srun		!	panic("switch SRUN");
	 EMPTY

	/*
	 * Committed to running process p.
	 * It may be the same as the one we were running before.
	 */
	sethi	%hi(_C_LABEL(want_resched)), %o0
	st	%g0, [%o0 + %lo(_C_LABEL(want_resched))]	! want_resched = 0;
	ld	[%g3 + P_ADDR], %g1		! newpcb = p->p_addr;
	st	%g0, [%g3 + 4]			! p->p_back = NULL;
	ldub	[%g1 + PCB_PIL], %g2		! newpil = newpcb->pcb_pil;
	st	%g4, [%g7 + %lo(_C_LABEL(curproc))]	! restore old proc so we can save it

	cmp	%g3, %g4		! p == lastproc?
	be,a	Lsw_sameproc		! yes, go return 0
	 wrpr	%g2, 0, %pil		! (after restoring pil)

	/*
	 * Not the old process.  Save the old process, if any;
	 * then load p.
	 */
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	mov	%g4, %o1
	ld	[%o1+P_PID], %o2
	mov	%g3, %o3
	call	prom_printf
	 ld	[%o3+P_PID], %o4
	LOCTOGLOB
	ba	2f
	 restore
1:	.asciz	"cpu_switch: %x(%d)->%x(%d)\r\n"
	_ALIGN
	Debugger();
2:
#endif
	flushw				! DEBUG -- make sure we don't hold on to any garbage
	brz,pn	%g4, Lsw_load		! if no old process, go load
	 wrpr	%g0, PSTATE_KERN, %pstate
	
	INCR(_C_LABEL(nswitchdiff))	! clobbers %o0,%o1
wb1:
	flushw				! save all register windows except this one
	save	%sp, -CC64FSZ, %sp	! Get space for this one
	stx	%i7, [%g5 + PCB_PC]	! Save rpc
	flushw				! save this window, too
	stx	%i6, [%g5 + PCB_SP]
	rdpr	%cwp, %o2
	stb	%o2, [%g5 + PCB_CWP]
#ifdef LOCKED_PCB
	/*
	 * Flush the locked entry from the TLB if needed.
	 */
	set	_C_LABEL(u0), %o0
	cmp	%g5, %o0
	beq	Lsw_load
	 andn	%g5, 0x0fff, %o0	! Drop unused bits
	or	%o0, 0x010, %o0
	stxa	%o0, [%o0] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync
	srl	%o0, 0, %o0				! and make sure it's both 32- and 64-bit entries
	stxa	%o0, [%o0] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync
#endif	
	
	/*
	 * Load the new process.  To load, we must change stacks and
	 * alter cpcb and the window control registers, hence we must 
	 * disable interrupts.
	 *
	 * We also must load up the `in' and `local' registers.
	 */
Lsw_load:
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	call	prom_printf
	 nop
	LOCTOGLOB
	ba	2f
	 restore
1:	.asciz	"cpu_switch: loading the new process:\r\n"
	_ALIGN
2:
#endif
	/* set new cpcb */
	st	%g3, [%g7 + %lo(_C_LABEL(curproc))]	! curproc = p;
	st	%g1, [%g6 + %lo(_C_LABEL(cpcb))]	! cpcb = newpcb;
	/* XXX update masterpaddr too */
	sethi	%hi(_C_LABEL(masterpaddr)), %g7
	st	%g1, [%g7 + %lo(_C_LABEL(masterpaddr))]
#ifdef LOCKED_PCB
	/* Now lock the cpcb page into the DTSB */
	set	_C_LABEL(u0), %o1	! Don't screw with %o0
	cmp	%o1, %g1
	beq	1f
	
	 sethi	%hi(_C_LABEL(ctxbusy)), %o4
	lduw	[%o4 + %lo(_C_LABEL(ctxbusy))], %o4
	lduw	[%o4], %o4				! Load up our page table.
	
	srlx	%g1, 32, %o0
	brnz,pn	%o0, 1f				! >32 bits? not here
	 srlx	%g1, STSHIFT-2, %o1
	andn	%o1, 3, %o1
	add	%o1, %o4, %o4
	lduwa	[%o4] ASI_PHYS_CACHED, %o4		! Remember -- UNSIGNED
	brz,pn	%o4, 1f				! NULL entry? check somewhere else
	 srlx	%g1, PTSHIFT, %o1			! Convert to ptab offset
	and	%o1, PTMASK, %o1
	sll	%o1, 3, %o1
	add	%o1, %o4, %o0
	ldxa	[%o0] ASI_PHYS_CACHED, %o4
	brgez,pn %o4, 1f				! Entry invalid?  Punt
	 mov	TLB_TAG_ACCESS, %o2
	or	%o4, TTE_L|TTE_W, %o4
	stxa	%o1, [%o2] ASI_DMMU			! Update TSB entry tag
	stxa	%o4, [%g0] ASI_DMMU_DATA_IN		! Enter new mapping
	membar	#Sync
1:
#endif
#ifdef NOT_DEBUG
	ldx	[%g1 + PCB_SP], %o0
	brnz,pt	%o0, 2f
	 ldx	[%o0], %o0			! Force a fault if needed
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	call	prom_printf
	 nop
	LOCTOGLOB
	call	utrap_halt
	 restore
1:	.asciz	"cpu_switch: NULL %sp\r\n"
	_ALIGN
2:
#endif
	ldx	[%g1 + PCB_SP], %i6
	call	blast_vcache		! Clear out I$ and D$
	ldx	[%g1 + PCB_PC], %i7
	wrpr	%g0, 0, %otherwin	! These two insns should be redundant
	wrpr	%g0, 0, %canrestore
	rdpr	%ver, %l7
	and	%l7, CWP, %l7
	wrpr	%g0, %l7, %cleanwin
!	wrpr	%g0, 0, %cleanwin	! DEBUG
	dec	1, %l7					! NWINDOWS-1-1
	wrpr	%l7, %cansave
	wrpr	%g0, 4, %tl				! DEBUG -- force watchdog
	flushw						! DEBUG
	wrpr	%g0, 0, %tl				! DEBUG
	/* load window */
	restore				! The logic is just too complicated to handle here.  Let the traps deal with the problem
!	flushw						! DEBUG
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	call	prom_printf
	 mov	%fp, %o1
	LOCTOGLOB
	ba	2f
	 restore
1:	.asciz	"cpu_switch: setup new process stack regs at %08x\r\n"
	_ALIGN
2:
#endif
#ifdef DEBUG
	mov	%g1, %o0
	SET_SP_REDZONE(%o0, %o1)
	CHECK_SP_REDZONE(%o0, %o1)
#endif
	/* finally, enable traps */
!	lduh	[%g1 + PCB_PSTATE], %o3	! Load newpstate
!	wrpr	%o3, 0, %pstate		! psr = newpsr;
	wrpr	%g0, PSTATE_INTR, %pstate

	/*
	 * Now running p.  Make sure it has a context so that it
	 * can talk about user space stuff.  (Its pcb_uw is currently
	 * zero so it is safe to have interrupts going here.)
	 */
	save	%sp, -CC64FSZ, %sp
	ld	[%g3 + P_VMSPACE], %o3	! vm = p->p_vmspace;
	set	_C_LABEL(kernel_pmap_), %o1
	ld	[%o3 + VM_PMAP], %o0		! if (vm->vm_pmap.pm_ctx != NULL)
	cmp	%o0, %o1
	bz,pn	%xcc, Lsw_havectx		! Don't replace kernel context!
	 ld	[%o0 + PM_CTX], %o0
	brnz,pt	%o0, Lsw_havectx		!	goto havecontext;
	 nop

	/* p does not have a context: call ctx_alloc to get one */
	call	_C_LABEL(ctx_alloc)		! ctx_alloc(&vm->vm_pmap);
	 ld	[%o3 + VM_PMAP], %o0
	
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	call	prom_printf
 	 mov	%i0, %o1
	LOCTOGLOB
	ba	2f
	 restore
1:	.asciz	"cpu_switch: got new ctx %d in new process\r\n"
	_ALIGN
2:
#endif
	/* p does have a context: just switch to it */
Lsw_havectx:
	! context is in %o0
	/*
	 * We probably need to flush the cache here.
	 */
	wr	%g0, ASI_DMMU, %asi		! restore the user context
	ldxa	[CTX_SECONDARY] %asi, %o1
	brz,pn	%o1, 1f
	 set	0x030, %o1
	stxa	%o1, [%o1] ASI_DMMU_DEMAP
	stxa	%o1, [%o1] ASI_IMMU_DEMAP
	membar	#Sync
1:	
	stxa	%o0, [CTX_SECONDARY] %asi	! Maybe we should invalidate the old context?
	membar	#Sync				! Maybe we should use flush here?
	flush	%sp

!	call	blast_vcache	! Maybe we don't need to do this now
	 nop
	
	restore
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	mov	%i0, %o2
	call	prom_printf
	 mov	%i7, %o1
	LOCTOGLOB
	ba	2f
	 restore
1:	.asciz	"cpu_switch: in new process pc=%08x ctx %d\r\n"
	_ALIGN
2:
#endif
#ifdef TRAPTRACE
	set	trap_trace, %o2
	lduw	[%o2+TRACEDIS], %o4
	brnz,pn	%o4, 1f
	 nop
	lduw	[%o2+TRACEPTR], %o3
	rdpr	%tl, %o4
	mov	4, %o5
	set	_C_LABEL(curproc), %o0
	sllx	%o4, 13, %o4
	lduw	[%o0], %o0
!	clr	%o0		! DISABLE PID
	or	%o4, %o5, %o4
	mov	%g0, %o5
	brz,pn	%o0, 2f
	 andncc	%o3, (TRACESIZ-1), %g0
!	ldsw	[%o0+P_PID], %o5	!  Load PID
2:	
	movnz	%icc, %g0, %o3
	
	set	_C_LABEL(cpcb), %o0	! Load up nsaved
	ld	[%o0], %o0
	ldub	[%o0 + PCB_NSAVED], %o0
	sllx	%o0, 9, %o1
	or	%o1, %o4, %o4
	
	sth	%o4, [%o2+%o3]
	inc	2, %o3
	sth	%o5, [%o2+%o3]
	inc	2, %o3
	stw	%o0, [%o2+%o3]
	inc	4, %o3
	stw	%sp, [%o2+%o3]
	inc	4, %o3
	stw	%o7, [%o2+%o3]
	inc	4, %o3
	stw	%o3, [%o2+TRACEPTR]
1:	
#endif


Lsw_sameproc:
	/*
	 * We are resuming the process that was running at the
	 * call to switch().  Just set psr ipl and return.
	 */
#ifdef NOTDEF_DEBUG
	mov	%l0, %o0
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	mov	%i0, %o2
	set	_C_LABEL(curproc), %o3
	ld	[%o3], %o3
	ld	[%o3 + P_VMSPACE], %o3
	call	prom_printf
	 mov	%i7, %o1
	set	swtchdelay, %o0
	call	delay
	 ld	[%o0], %o0
	set	pmapdebug, %o0
	ld	[%o0], %o0
	tst	%o0
	tnz	%icc, 1; nop	! Call debugger if we're in pmapdebug
	LOCTOGLOB
	ba	2f
	 restore
1:	.asciz	"cpu_switch: vectoring to pc=%08x thru %08x vmspace=%p\r\n"
	_ALIGN
	.globl	swtchdelay
swtchdelay:	
	.word	1000
2:
	Debugger();
#endif
!	wrpr	%g0, 0, %cleanwin	! DEBUG
	retl
	 wrpr	%g0, PSTATE_INTR, %pstate


/*
 * Snapshot the current process so that stack frames are up to date.
 * Only used just before a crash dump.
 */
ENTRY(snapshot)
	rdpr	%pstate, %o1		! save psr
	stx	%o6, [%o0 + PCB_SP]	! save sp
	rdpr	%pil, %o2
	sth	%o1, [%o0 + PCB_PSTATE]
	rdpr	%cwp, %o3
	stb	%o2, [%o0 + PCB_PIL]
	stb	%o3, [%o0 + PCB_CWP]

	flushw
	save	%sp, -CC64FSZ, %sp
	flushw
	ret
	 restore

/*
 * cpu_set_kpc() and cpu_fork() arrange for proc_trampoline() to run
 * after after a process gets chosen in switch(). The stack frame will
 * contain a function pointer in %l0, and an argument to pass to it in %l2.
 *
 * If the function *(%l0) returns, we arrange for an immediate return
 * to user mode. This happens in two known cases: after execve(2) of init,
 * and when returning a child to user mode after a fork(2).
 */
	nop; nop					! Make sure we don't get lost getting here.
ENTRY(proc_trampoline)
#ifdef NOTDEF_DEBUG
	nop; nop; nop; nop				! Try to make sure we don't vector into the wrong instr
	mov	%l0, %o0
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	mov	%i6, %o2
	call	prom_printf
	 mov	%i0, %o1
	ba	2f
	 restore
1:	.asciz	"proc_trampoline: calling %x sp %x\r\n"
	_ALIGN
	Debugger()
2:
#endif
	call	%l0			! re-use current frame
	 mov	%l1, %o0

	/*
	 * Here we finish up as in syscall, but simplified.  We need to
	 * fiddle pc and npc a bit, as execve() / setregs() /cpu_set_kpc()
	 * have only set npc, in anticipation that trap.c will advance past
	 * the trap instruction; but we bypass that, so we must do it manually.
	 */
!	save	%sp, -CC64FSZ, %sp		! Save a kernel frame to emulate a syscall
	mov	PSTATE_USER, %g1		! user pstate (no need to load it)
	ldx	[%sp + CCFSZ + TF_NPC], %g2	! pc = tf->tf_npc from execve/fork
	sllx	%g1, TSTATE_PSTATE_SHIFT, %g1	! Shift it into place
	add	%g2, 4, %g3			! npc = pc+4
	rdpr	%cwp, %g4			! Fixup %cwp in %tstate
	stx	%g3, [%sp + CCFSZ + TF_NPC]
	or	%g1, %g4, %g1
	stx	%g2, [%sp + CCFSZ + TF_PC]
	stx	%g1, [%sp + CCFSZ + TF_TSTATE]
#ifdef NOTDEF_DEBUG
!	set	panicstack-CC64FSZ, %o0! DEBUG
!	save	%g0, %o0, %sp	! DEBUG
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	ldx	[%fp + CCFSZ + TF_O + ( 6*8)], %o2
	mov	%fp, %o2
	add	%fp, CCFSZ, %o3
	GLOBTOLOC
	call	prom_printf
	 mov	%g2, %o1
	set	3f, %o0
	mov	%g1, %o1
	mov	%g2, %o2
	mov	CTX_SECONDARY, %o4
	ldxa	[%o4] ASI_DMMU, %o4
	call	prom_printf
	 mov	%g3, %o3
	LOCTOGLOB
	ba	2f
	 restore
1:	.asciz	"proc_trampoline: returning to %x, sp=%x, tf=%x\r\n"
3:	.asciz	"tstate=%p tpc=%p tnpc=%p ctx=%x\r\n"
	_ALIGN
2:
	Debugger()
#endif
	CHKPT(%o3,%o4,0x35)
	b	return_from_trap
	 wrpr	%g0, 1, %tl			! Return to tl==1
		
/*
 * {fu,su}{,i}{byte,word}
 */
ALTENTRY(fuiword)
ENTRY(fuword)
	btst	3, %o0			! has low bits set...
	bnz	Lfsbadaddr		!	go return -1
	EMPTY
	sethi	%hi(_C_LABEL(cpcb)), %o2		! cpcb->pcb_onfault = Lfserr;
	set	Lfserr, %o3
	ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2
	st	%o3, [%o2 + PCB_ONFAULT]
	lda	[%o0] ASI_AIUS, %o0	! fetch the word
	retl				! phew, made it, return the word
	st	%g0, [%o2 + PCB_ONFAULT]! but first clear onfault

Lfserr:
	st	%g0, [%o2 + PCB_ONFAULT]! error in r/w, clear pcb_onfault
Lfsbadaddr:
	retl				! and return error indicator
	 mov	-1, %o0

	/*
	 * This is just like Lfserr, but it's a global label that allows
	 * mem_access_fault() to check to see that we don't want to try to
	 * page in the fault.  It's used by fuswintr() etc.
	 */
	.globl	_C_LABEL(Lfsbail)
_C_LABEL(Lfsbail):
	st	%g0, [%o2 + PCB_ONFAULT]! error in r/w, clear pcb_onfault
	retl				! and return error indicator
	 mov	-1, %o0

	/*
	 * Like fusword but callable from interrupt context.
	 * Fails if data isn't resident.
	 */
ENTRY(fuswintr)
	sethi	%hi(_C_LABEL(cpcb)), %o2		! cpcb->pcb_onfault = _Lfsbail;
	ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2
	set	_C_LABEL(Lfsbail), %o3
	st	%o3, [%o2 + PCB_ONFAULT]
	lduha	[%o0] ASI_AIUS, %o0	! fetch the halfword
	retl				! made it
	st	%g0, [%o2 + PCB_ONFAULT]! but first clear onfault

ENTRY(fusword)
	sethi	%hi(_C_LABEL(cpcb)), %o2		! cpcb->pcb_onfault = Lfserr;
	ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2
	set	Lfserr, %o3
	st	%o3, [%o2 + PCB_ONFAULT]
	lduha	[%o0] ASI_AIUS, %o0		! fetch the halfword
	retl				! made it
	st	%g0, [%o2 + PCB_ONFAULT]! but first clear onfault

ALTENTRY(fuibyte)
ENTRY(fubyte)
	sethi	%hi(_C_LABEL(cpcb)), %o2		! cpcb->pcb_onfault = Lfserr;
	ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2
	set	Lfserr, %o3
	st	%o3, [%o2 + PCB_ONFAULT]
	lduba	[%o0] ASI_AIUS, %o0	! fetch the byte
	retl				! made it
	st	%g0, [%o2 + PCB_ONFAULT]! but first clear onfault

ALTENTRY(suiword)
ENTRY(suword)
	btst	3, %o0			! or has low bits set ...
	bnz	Lfsbadaddr		!	go return error
	EMPTY
	sethi	%hi(_C_LABEL(cpcb)), %o2		! cpcb->pcb_onfault = Lfserr;
	ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2
	set	Lfserr, %o3
	st	%o3, [%o2 + PCB_ONFAULT]
	sta	%o1, [%o0] ASI_AIUS	! store the word
	st	%g0, [%o2 + PCB_ONFAULT]! made it, clear onfault
	retl				! and return 0
	clr	%o0

ENTRY(suswintr)
	sethi	%hi(_C_LABEL(cpcb)), %o2		! cpcb->pcb_onfault = _Lfsbail;
	ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2
	set	_C_LABEL(Lfsbail), %o3
	st	%o3, [%o2 + PCB_ONFAULT]
	stha	%o1, [%o0] ASI_AIUS	! store the halfword
	st	%g0, [%o2 + PCB_ONFAULT]! made it, clear onfault
	retl				! and return 0
	clr	%o0

ENTRY(susword)
	sethi	%hi(_C_LABEL(cpcb)), %o2		! cpcb->pcb_onfault = Lfserr;
	ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2
	set	Lfserr, %o3
	st	%o3, [%o2 + PCB_ONFAULT]
	stha	%o1, [%o0] ASI_AIUS	! store the halfword
	st	%g0, [%o2 + PCB_ONFAULT]! made it, clear onfault
	retl				! and return 0
	clr	%o0

ALTENTRY(suibyte)
ENTRY(subyte)
	sethi	%hi(_C_LABEL(cpcb)), %o2		! cpcb->pcb_onfault = Lfserr;
	ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2
	set	Lfserr, %o3
	st	%o3, [%o2 + PCB_ONFAULT]
	stba	%o1, [%o0] ASI_AIUS	! store the byte
	st	%g0, [%o2 + PCB_ONFAULT]! made it, clear onfault
	retl				! and return 0
	clr	%o0

/* probeget and probeset are meant to be used during autoconfiguration */
/*
 * The following probably need to be changed, but to what I don't know.
 */
	
/*
 * probeget(addr, size) caddr_t addr; int size;
 *
 * Read or write a (byte,word,longword) from the given address.
 * Like {fu,su}{byte,halfword,word} but our caller is supposed
 * to know what he is doing... the address can be anywhere.
 *
 * We optimize for space, rather than time, here.
 */
ENTRY(probeget)
	! %o0 = addr, %o1 = (1,2,4)
	sethi	%hi(_C_LABEL(cpcb)), %o2
	ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2	! cpcb->pcb_onfault = Lfserr;
	set	Lfserr, %o5
	st	%o5, [%o2 + PCB_ONFAULT]
	btst	1, %o1
	bnz,a	0f			! if (len & 1)
	 ldub	[%o0], %o0		!	value = *(char *)addr;
0:	btst	2, %o1
	bnz,a	0f			! if (len & 2)
	 lduh	[%o0], %o0		!	value = *(short *)addr;
0:	btst	4, %o1
	bnz,a	0f			! if (len & 4)
	 ld	[%o0], %o0		!	value = *(int *)addr;
0:	retl				! made it, clear onfault and return
	 st	%g0, [%o2 + PCB_ONFAULT]

/*
 * probeset(addr, size, val) caddr_t addr; int size, val;
 *
 * As above, but we return 0 on success.
 */
ENTRY(probeset)
	! %o0 = addr, %o1 = (1,2,4), %o2 = val
	sethi	%hi(_C_LABEL(cpcb)), %o3
	ld	[%o3 + %lo(_C_LABEL(cpcb))], %o3	! cpcb->pcb_onfault = Lfserr;
	set	Lfserr, %o5
	st	%o5, [%o3 + PCB_ONFAULT]
	btst	1, %o1
	bnz,a	0f			! if (len & 1)
	 stb	%o2, [%o0]		!	*(char *)addr = value;
0:	btst	2, %o1
	bnz,a	0f			! if (len & 2)
	 sth	%o2, [%o0]		!	*(short *)addr = value;
0:	btst	4, %o1
	bnz,a	0f			! if (len & 4)
	 st	%o2, [%o0]		!	*(int *)addr = value;
0:	clr	%o0			! made it, clear onfault and return 0
	retl
	 st	%g0, [%o3 + PCB_ONFAULT]

#ifdef not4u
/*
 * int xldcontrolb(caddr_t, pcb)
 *		    %o0     %o1
 *
 * read a byte from the specified address in ASI_CONTROL space.
 */
ENTRY(xldcontrolb)
	!sethi	%hi(_C_LABEL(cpcb)), %o2
	!ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2	! cpcb->pcb_onfault = Lfsbail;
	or	%o1, %g0, %o2		! %o2 = %o1
	set	_C_LABEL(Lfsbail), %o5
	st	%o5, [%o2 + PCB_ONFAULT]
	lduba	[%o0] ASI_CONTROL, %o0	! read
0:	retl
	 st	%g0, [%o2 + PCB_ONFAULT]
#endif
	
/*
 * Insert entry into doubly-linked queue.
 * We could just do this in C, but gcc does not do leaves well (yet).
 */
ENTRY(insque)
ENTRY(_insque)
	! %o0 = e = what to insert; %o1 = after = entry to insert after
	st	%o1, [%o0 + 4]		! e->prev = after;
	ld	[%o1], %o2		! tmp = after->next;
	st	%o2, [%o0]		! e->next = tmp;
	st	%o0, [%o1]		! after->next = e;
	retl
	st	%o0, [%o2 + 4]		! tmp->prev = e;


/*
 * Remove entry from doubly-linked queue.
 */
ENTRY(remque)
ENTRY(_remque)
	! %o0 = e = what to remove
	ld	[%o0], %o1		! n = e->next;
	ld	[%o0 + 4], %o2		! p = e->prev;
	st	%o2, [%o1 + 4]		! n->prev = p;
	retl
	st	%o1, [%o2]		! p->next = n;

/*
 * pmap_zero_page(pa)
 *
 * Zero one page physically addressed
 *
 * Block load/store ASIs do not exist for physical addresses,
 * so we won't use them.
 *
 * While we do the zero operation, we also need to blast away
 * the contents of the D$.  We will execute a flush at the end
 * to sync the I$.
 */
ENTRY(pmap_zero_page)
	!!
	!! If we have 64-bit physical addresses (and we do now)
	!! we need to move the pointer from %o0:%o1 to %o0
	!!
#if PADDRT == 8
	sllx	%o0, 32, %o0
	or	%o0, %o1, %o0
#endif
#ifdef DEBUG
	set	pmapdebug, %o4
	ld	[%o4], %o4
	btst	0x80, %o4	! PDB_COPY
	bz,pt	%icc, 3f
	 nop
	save	%sp, -CC64FSZ, %sp
	set	2f, %o0
	call	printf
	 mov	%i0, %o1
!	ta	1; nop
	ba	3f
	 restore
2:	.asciz	"pmap_zero_page(%p)\n"
	_ALIGN
3:
#endif
#if 0
	set	NBPG, %o2	! Start of upper D$
	sub	%o2, 8, %o1	! End of lower D$ and bytes to clear
1:
	stxa	%g0, [%o0] ASI_PHYS_CACHED
	inc	8, %o0
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	dec	8, %o1
	stxa	%g0, [%o2] ASI_DCACHE_TAG
	brnz	%o1, 1b
	 inc	8, %o2
	sethi	%hi(KERNBASE), %o3
	flush	%o3
	retl
	 nop
#else
	set	NBPG-8, %o1
	add	%o1, %o0, %o1
1:
	stxa	%g0, [%o0] ASI_PHYS_CACHED
	cmp	%o0, %o1
	blt	1b
	 inc	8, %o0
	ba	_C_LABEL(blast_vcache)	! Clear out D$ and return
	 nop
	retl
	 nop
#endif
/*
 * pmap_copy_page(src, dst)
 *
 * Copy one page physically addressed
 * We need to use a global reg for ldxa/stxa
 * so the top 32-bits cannot be lost if we take
 * a trap and need to save our stack frame to a
 * 32-bit stack.  We will unroll the loop by 8 to
 * improve performance.
 *
 * We also need to blast the D$ and flush like
 * pmap_zero_page.
 */
ENTRY(pmap_copy_page)
	!!
	!! If we have 64-bit physical addresses (and we do now)
	!! we need to move the pointer from %o0:%o1 to %o0 and
	!! %o2:%o3 to %o1
	!!
#if PADDRT == 8
	sllx	%o0, 32, %o0
	or	%o0, %o1, %o0
	sllx	%o2, 32, %o1
	or	%o3, %o1, %o1
#endif
#ifdef DEBUG
	set	pmapdebug, %o4
	ld	[%o4], %o4
	btst	0x80, %o4	! PDB_COPY
	bz,pt	%icc, 3f
	 nop
	save	%sp, -CC64FSZ, %sp
	mov	%i0, %o1
	set	2f, %o0
	call	printf
	 mov	%i1, %o2
!	ta	1; nop
	ba	3f
	 restore
2:	.asciz	"pmap_copy_page(%p,%p)\n"
	_ALIGN
3:
#endif
#if 0
#if 0
	save	%sp, -CC64FSZ, %sp	! Get 8 locals for scratch
	set	NBPG, %o1
	sub	%o1, 8, %o0
1:
	ldxa	[%i0] ASI_PHYS_CACHED, %l0
	inc	8, %i0
	ldxa	[%i0] ASI_PHYS_CACHED, %l1
	inc	8, %i0
	ldxa	[%i0] ASI_PHYS_CACHED, %l2
	inc	8, %i0
	ldxa	[%i0] ASI_PHYS_CACHED, %l3
	inc	8, %i0
	ldxa	[%i0] ASI_PHYS_CACHED, %l4
	inc	8, %i0
	ldxa	[%i0] ASI_PHYS_CACHED, %l5
	inc	8, %i0
	ldxa	[%i0] ASI_PHYS_CACHED, %l6
	inc	8, %i0
	ldxa	[%i0] ASI_PHYS_CACHED, %l7
	inc	8, %i0
	stxa	%l0, [%i1] ASI_PHYS_CACHED
	inc	8, %i1
	stxa	%l1, [%i1] ASI_PHYS_CACHED
	inc	8, %i1
	stxa	%l2, [%i1] ASI_PHYS_CACHED
	inc	8,%i1
	stxa	%l3, [%i1] ASI_PHYS_CACHED
	inc	8, %i1
	stxa	%l4, [%i1] ASI_PHYS_CACHED
	inc	8, %i1
	stxa	%l5, [%i1] ASI_PHYS_CACHED
	inc	8, %i1
	stxa	%l6, [%i1] ASI_PHYS_CACHED
	inc	8, %i1
	stxa	%l7, [%i1] ASI_PHYS_CACHED
	inc	8, %i1
	
	stxa	%g0, [%o0] ASI_DCACHE_TAG! Blast away at the D$
	dec	8, %o0
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	inc	8, %o1
	stxa	%g0, [%o0] ASI_DCACHE_TAG! Blast away at the D$
	dec	8, %o0
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	inc	8, %o1
	stxa	%g0, [%o0] ASI_DCACHE_TAG! Blast away at the D$
	dec	8, %o0
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	inc	8, %o1
	stxa	%g0, [%o0] ASI_DCACHE_TAG! Blast away at the D$
	dec	8, %o0
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	inc	8, %o1
	stxa	%g0, [%o0] ASI_DCACHE_TAG! Blast away at the D$
	dec	8, %o0
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	inc	8, %o1
	stxa	%g0, [%o0] ASI_DCACHE_TAG! Blast away at the D$
	dec	8, %o0
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	inc	8, %o1
	stxa	%g0, [%o0] ASI_DCACHE_TAG! Blast away at the D$
	dec	8, %o0
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	inc	8, %o1
	stxa	%g0, [%o0] ASI_DCACHE_TAG! Blast away at the D$
	dec	8, %o0
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	brnz,pt	%o0, 1b
	 inc	8, %o1
	sethi	%hi(KERNBASE), %o2
	flush	%o2
	return
	 nop
#else
	/* This is the short, slow, safe version that uses %g1 */
	
	set	NBPG, %o3
	sub	%o3, 8, %o2
	mov	%g1, %o4		! Save g1
1:
	ldxa	[%o0] ASI_PHYS_CACHED, %g1
	inc	8, %o0
	stxa	%g1, [%o1] ASI_PHYS_CACHED
	inc	8, %o1
	
	stxa	%g0, [%o2] ASI_DCACHE_TAG! Blast away at the D$
	dec	8, %o2
	stxa	%g0, [%o3] ASI_DCACHE_TAG
	brnz,pt	%o2, 1b
	 inc	8, %o3
	mov	%o4, %g1
	sethi	%hi(KERNBASE), %o5
	flush	%o5
	retl
	 nop
#endif
#else
	set	NBPG, %o3
	add	%o3, %o0, %o3
	mov	%g1, %o4		! Save g1
1:
	ldxa	[%o0] ASI_PHYS_CACHED, %g1
	inc	8, %o0
	cmp	%o0, %o3
	stxa	%g1, [%o1] ASI_PHYS_CACHED
	bl,pt	%icc, 1b		! We don't care about pages >4GB
	 inc	8, %o1
	ba	_C_LABEL(blast_vcache)	! Clear out D$ and return
	 mov	%o4, %g1		! Restore g1
	retl
	 mov	%o4, %g1		! Restore g1
#endif
	
/*
 * extern int64_t pseg_get(struct pmap* %o0, vaddr_t addr %o1);
 *
 * Return TTE at addr in pmap.  Uses physical addressing only.
 * pmap->pm_physaddr must by the physical address of pm_segs
 *
 */
ENTRY(pseg_get)
!	flushw			! Make sure we don't have stack probs & lose hibits of %o
#if PADDRT == 8	
	ldx	[%o0 + PM_PHYS], %o0			! pmap->pm_segs
#else
	lduw	[%o0 + PM_PHYS], %o0			! pmap->pm_segs
#endif
	srlx	%o1, 32, %o2
	brnz,pn	%o2, 1f					! >32 bits? not here
	 srlx	%o1, STSHIFT-2, %o3
	andn	%o3, 3, %o3
	add	%o0, %o3, %o0
	lduwa	[%o0] ASI_PHYS_CACHED, %o0		! Remember -- UNSIGNED
	srlx	%o1, PTSHIFT, %o3			! Convert to ptab offset
	brz,pn	%o0, 1f					! NULL entry? check somewhere else
	 and	%o3, PTMASK, %o3
	sll	%o3, 3, %o3
	add	%o0, %o3, %o0
	ldxa	[%o0] ASI_PHYS_CACHED, %g1		! Use %g1 so we won't lose the top 1/2
	brgez,pn %g1, 1f				! Entry invalid?  Punt
	 srlx	%g1, 32, %o0	
	retl
	 srl	%g1, 0, %o1	
1:
	clr	%o1
	retl
	 clr	%o0

/*
 * extern void pseg_set(struct pmap* %o0, vaddr_t addr %o1, int64_t tte %o2:%o3);
 *
 * Set a pseg entry to a particular TTE value.  Returns 0 on success, else the pointer
 * to the empty pseg entry.  Allocate a page, put the phys addr in the returned location,
 * and try again.
 *
 */
ENTRY(pseg_set)
	flushw			! Make sure we don't have stack probs & lose hibits of %o
#if PADDRT == 8	
	ldx	[%o0 + PM_PHYS], %o4			! pmap->pm_segs
#else
	lduw	[%o0 + PM_PHYS], %o4			! pmap->pm_segs
#endif
#ifdef DEBUG
	mov	%o0, %g1				! DEBUG
#endif
	srlx	%o1, 32, %o5
	brnz,pn	%o5, 1f					! >32 bits? not here
	 srlx	%o1, STSHIFT-2, %o5
	andn	%o5, 3, %o5
	add	%o4, %o5, %o4
#ifdef DEBUG
	mov	%o4, %g4				! DEBUG
	mov	%o5, %g5				! DEBUG
#endif
	lduwa	[%o4] ASI_PHYS_CACHED, %o4		! Remember -- UNSIGNED
	add	%o0, %o5, %o0				! Calculate virt addr of this pseg entry
	brz,pn	%o4, 1f					! NULL entry? Allocate a page
	 srlx	%o1, PTSHIFT, %o5			! Convert to ptab offset
	and	%o5, PTMASK, %o5
	sll	%o5, 3, %o5
	add	%o5, %o4, %o4
	stda	%o2, [%o4] ASI_PHYS_CACHED		! Easier than shift+or
	retl
	 clr	%o0
1:
	retl
	 add	%o0, PM_SEGS, %o0

/*
 * copywords(src, dst, nbytes)
 *
 * Copy `nbytes' bytes from src to dst, both of which are word-aligned;
 * nbytes is a multiple of four.  It may, however, be zero, in which case
 * nothing is to be copied.
 */
ENTRY(copywords)
	! %o0 = src, %o1 = dst, %o2 = nbytes
	b	1f
	 deccc	4, %o2
0:
	st	%o3, [%o1 + %o2]
	deccc	4, %o2			! while ((n -= 4) >= 0)
1:
	bge,a	0b			!    *(int *)(dst+n) = *(int *)(src+n);
	 ld	[%o0 + %o2], %o3
	retl
	 nop

/*
 * qcopy(src, dst, nbytes)
 *
 * (q for `quad' or `quick', as opposed to b for byte/block copy)
 *
 * Just like copywords, but everything is multiples of 8.
 */
ENTRY(qcopy)
	ba,pt	%icc, 1f
	 deccc	8, %o2
0:
	stx	%g1, [%o1 + %o2]
	deccc	8, %o2
1:
	bge,a,pt	%icc, 0b
	 ldx	[%o0 + %o2], %g1
	retl
	nop

/*
 * qzero(addr, nbytes)
 *
 * Zeroes `nbytes' bytes of a quad-aligned virtual address,
 * where nbytes is itself a multiple of 8.
 */
ENTRY(qzero)
	! %o0 = addr, %o1 = len (in bytes)
0:
	deccc	8, %o1			! while ((n =- 8) >= 0)
	bge,a,pt	%icc, 0b
	 stx	%g0, [%o0 + %o1]	!	*(quad *)(addr + n) = 0;
	retl
	nop

/*
 * kernel bcopy/memcpy
 * Assumes regions do not overlap; has no useful return value.
 *
 * Must not use %g7 (see copyin/copyout above).
 */
#if 0
ENTRY(memcpy) /* dest, src, size */
	/*
	 * Swap args for bcopy.  Gcc generates calls to memcpy for
	 * structure assignments.
	 */
	mov	%o0, %o3
	mov	%o1, %o0
	mov	%o3, %o1
#endif
ENTRY(bcopy) /* src, dest, size */
#ifdef DEBUG
	set	pmapdebug, %o4
	ld	[%o4], %o4
	btst	0x80, %o4	! PDB_COPY
	bz,pt	%icc, 3f
	 nop
	save	%sp, -CC64FSZ, %sp
	mov	%i0, %o1
	set	2f, %o0
	mov	%i1, %o2
	call	printf
	 mov	%i2, %o3
!	ta	1; nop
	ba	3f
	 restore
2:	.asciz	"bcopy(%p->%p,%x)\n"
	_ALIGN
3:
#endif
	cmp	%o2, BCOPY_SMALL
Lbcopy_start:
	bge,a	Lbcopy_fancy	! if >= this many, go be fancy.
	 btst	7, %o0		! (part of being fancy)

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
	 EMPTY
0:
	inc	%o0
	ldsb	[%o0 - 1], %o4	!	(++dst)[-1] = *src++;
	stb	%o4, [%o1]
	deccc	%o2
	bge	0b
	 inc	%o1
1:
	retl
	 nop
	NOTREACHED

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
Lbcopy_fancy:
	! check for common case first: everything lines up.
!	btst	7, %o0		! done already
	bne	1f
	 EMPTY
	btst	7, %o1
	be,a	Lbcopy_doubles
	 dec	8, %o2		! if all lined up, len -= 8, goto bcopy_doubes

	! If the low bits match, we can make these line up.
1:
	xor	%o0, %o1, %o3	! t = src ^ dst;
	btst	1, %o3		! if (t & 1) {
	be,a	1f
	 btst	1, %o0		! [delay slot: if (src & 1)]

	! low bits do not match, must copy by bytes.
0:
	ldsb	[%o0], %o4	!	do {
	inc	%o0		!		(++dst)[-1] = *src++;
	inc	%o1
	deccc	%o2
	bnz	0b		!	} while (--len != 0);
	 stb	%o4, [%o1 - 1]
	retl
	 nop
	NOTREACHED

	! lowest bit matches, so we can copy by words, if nothing else
1:
	be,a	1f		! if (src & 1) {
	 btst	2, %o3		! [delay slot: if (t & 2)]

	! although low bits match, both are 1: must copy 1 byte to align
	ldsb	[%o0], %o4	!	*dst++ = *src++;
	stb	%o4, [%o1]
	inc	%o0
	inc	%o1
	dec	%o2		!	len--;
	btst	2, %o3		! } [if (t & 2)]
1:
	be,a	1f		! if (t & 2) {
	 btst	2, %o0		! [delay slot: if (src & 2)]
	dec	2, %o2		!	len -= 2;
0:
	ldsh	[%o0], %o4	!	do {
	sth	%o4, [%o1]	!		*(short *)dst = *(short *)src;
	inc	2, %o0		!		dst += 2, src += 2;
	deccc	2, %o2		!	} while ((len -= 2) >= 0);
	bge	0b
	 inc	2, %o1
	b	Lbcopy_mopb	!	goto mop_up_byte;
	 btst	1, %o2		! } [delay slot: if (len & 1)]
	NOTREACHED

	! low two bits match, so we can copy by longwords
1:
	be,a	1f		! if (src & 2) {
	 btst	4, %o3		! [delay slot: if (t & 4)]

	! although low 2 bits match, they are 10: must copy one short to align
	ldsh	[%o0], %o4	!	(*short *)dst = *(short *)src;
	sth	%o4, [%o1]
	inc	2, %o0		!	dst += 2;
	inc	2, %o1		!	src += 2;
	dec	2, %o2		!	len -= 2;
	btst	4, %o3		! } [if (t & 4)]
1:
	be,a	1f		! if (t & 4) {
	 btst	4, %o0		! [delay slot: if (src & 4)]
	dec	4, %o2		!	len -= 4;
0:
	ld	[%o0], %o4	!	do {
	st	%o4, [%o1]	!		*(int *)dst = *(int *)src;
	inc	4, %o0		!		dst += 4, src += 4;
	deccc	4, %o2		!	} while ((len -= 4) >= 0);
	bge	0b
	 inc	4, %o1
	b	Lbcopy_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! } [delay slot: if (len & 2)]
	NOTREACHED

	! low three bits match, so we can copy by doublewords
1:
	be	1f		! if (src & 4) {
	 dec	8, %o2		! [delay slot: len -= 8]
	ld	[%o0], %o4	!	*(int *)dst = *(int *)src;
	st	%o4, [%o1]
	inc	4, %o0		!	dst += 4, src += 4, len -= 4;
	inc	4, %o1
	dec	4, %o2		! }
1:
Lbcopy_doubles:
	ldx	[%o0], %g5	! do {
	stx	%g5, [%o1]	!	*(double *)dst = *(double *)src;
	inc	8, %o0		!	dst += 8, src += 8;
	deccc	8, %o2		! } while ((len -= 8) >= 0);
	bge	Lbcopy_doubles
	 inc	8, %o1

	! check for a usual case again (save work)
	btst	7, %o2		! if ((len & 7) == 0)
	be	Lbcopy_done	!	goto bcopy_done;

	 btst	4, %o2		! if ((len & 4)) == 0)
	be,a	Lbcopy_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! [delay slot: if (len & 2)]
	ld	[%o0], %o4	!	*(int *)dst = *(int *)src;
	st	%o4, [%o1]
	inc	4, %o0		!	dst += 4;
	inc	4, %o1		!	src += 4;
	btst	2, %o2		! } [if (len & 2)]

1:
	! mop up trailing word (if present) and byte (if present).
Lbcopy_mopw:
	be	Lbcopy_mopb	! no word, go mop up byte
	 btst	1, %o2		! [delay slot: if (len & 1)]
	ldsh	[%o0], %o4	! *(short *)dst = *(short *)src;
	be	Lbcopy_done	! if ((len & 1) == 0) goto done;
	 sth	%o4, [%o1]
	ldsb	[%o0 + 2], %o4	! dst[2] = src[2];
	retl
	 stb	%o4, [%o1 + 2]
	NOTREACHED

	! mop up trailing byte (if present).
Lbcopy_mopb:
	bne,a	1f
	 ldsb	[%o0], %o4

Lbcopy_done:
	retl
	 nop
1:
	retl
	 stb	%o4,[%o1]


/*
 * kcopy() is exactly like bcopy except that it set pcb_onfault such that
 * when a fault occurs, it is able to return -1 to indicate this to the
 * caller.
 */
ENTRY(kcopy)
#ifdef DEBUG
	set	pmapdebug, %o4
	ld	[%o4], %o4
	btst	0x80, %o4	! PDB_COPY
	bz,pt	%icc, 3f
	 nop
	save	%sp, -CC64FSZ, %sp
	mov	%i0, %o1
	set	2f, %o0
	mov	%i1, %o2
	call	printf
	 mov	%i2, %o3
!	ta	1; nop
	ba	3f
	 restore
2:	.asciz	"kcopy(%p->%p,%x)\n"
	_ALIGN
3:
#endif
	sethi	%hi(_C_LABEL(cpcb)), %o5		! cpcb->pcb_onfault = Lkcerr;
	ld	[%o5 + %lo(_C_LABEL(cpcb))], %o5
	set	Lkcerr, %o3
	st	%o3, [%o5 + PCB_ONFAULT]
	cmp	%o2, BCOPY_SMALL
Lkcopy_start:
	bge,a	Lkcopy_fancy	! if >= this many, go be fancy.
	 btst	7, %o0		! (part of being fancy)

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
	 EMPTY
0:
	inc	%o0
	ldsb	[%o0 - 1], %o4	!	(++dst)[-1] = *src++;
	stb	%o4, [%o1]
	deccc	%o2
	bge	0b
	 inc	%o1
1:
	st	%g0, [%o5 + PCB_ONFAULT]
	retl
	 clr	%o0
	NOTREACHED

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
Lkcopy_fancy:
	! check for common case first: everything lines up.
!	btst	7, %o0		! done already
	bne	1f
	 EMPTY
	btst	7, %o1
	be,a	Lkcopy_doubles
	 dec	8, %o2		! if all lined up, len -= 8, goto kcopy_doubes

	! If the low bits match, we can make these line up.
1:
	xor	%o0, %o1, %o3	! t = src ^ dst;
	btst	1, %o3		! if (t & 1) {
	be,a	1f
	 btst	1, %o0		! [delay slot: if (src & 1)]

	! low bits do not match, must copy by bytes.
0:
	ldsb	[%o0], %o4	!	do {
	inc	%o0		!		(++dst)[-1] = *src++;
	inc	%o1
	deccc	%o2
	bnz	0b		!	} while (--len != 0);
	 stb	%o4, [%o1 - 1]
	membar	#Sync		! Make sure all traps are taken
	clr	%o0
	retl
	 st	%g0, [%o5 + PCB_ONFAULT]
	NOTREACHED

	! lowest bit matches, so we can copy by words, if nothing else
1:
	be,a	1f		! if (src & 1) {
	 btst	2, %o3		! [delay slot: if (t & 2)]

	! although low bits match, both are 1: must copy 1 byte to align
	ldsb	[%o0], %o4	!	*dst++ = *src++;
	stb	%o4, [%o1]
	inc	%o0
	inc	%o1
	dec	%o2		!	len--;
	btst	2, %o3		! } [if (t & 2)]
1:
	be,a	1f		! if (t & 2) {
	 btst	2, %o0		! [delay slot: if (src & 2)]
	dec	2, %o2		!	len -= 2;
0:
	ldsh	[%o0], %o4	!	do {
	sth	%o4, [%o1]	!		*(short *)dst = *(short *)src;
	inc	2, %o0		!		dst += 2, src += 2;
	deccc	2, %o2		!	} while ((len -= 2) >= 0);
	bge	0b
	 inc	2, %o1
	b	Lkcopy_mopb	!	goto mop_up_byte;
	 btst	1, %o2		! } [delay slot: if (len & 1)]
	NOTREACHED

	! low two bits match, so we can copy by longwords
1:
	be,a	1f		! if (src & 2) {
	 btst	4, %o3		! [delay slot: if (t & 4)]

	! although low 2 bits match, they are 10: must copy one short to align
	ldsh	[%o0], %o4	!	(*short *)dst = *(short *)src;
	sth	%o4, [%o1]
	inc	2, %o0		!	dst += 2;
	inc	2, %o1		!	src += 2;
	dec	2, %o2		!	len -= 2;
	btst	4, %o3		! } [if (t & 4)]
1:
	be,a	1f		! if (t & 4) {
	 btst	4, %o0		! [delay slot: if (src & 4)]
	dec	4, %o2		!	len -= 4;
0:
	ld	[%o0], %o4	!	do {
	st	%o4, [%o1]	!		*(int *)dst = *(int *)src;
	inc	4, %o0		!		dst += 4, src += 4;
	deccc	4, %o2		!	} while ((len -= 4) >= 0);
	bge	0b
	 inc	4, %o1
	b	Lkcopy_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! } [delay slot: if (len & 2)]
	NOTREACHED

	! low three bits match, so we can copy by doublewords
1:
	be	1f		! if (src & 4) {
	 dec	8, %o2		! [delay slot: len -= 8]
	ld	[%o0], %o4	!	*(int *)dst = *(int *)src;
	st	%o4, [%o1]
	inc	4, %o0		!	dst += 4, src += 4, len -= 4;
	inc	4, %o1
	dec	4, %o2		! }
1:
Lkcopy_doubles:
	ldx	[%o0], %g5	! do {
	stx	%g5, [%o1]	!	*(double *)dst = *(double *)src;
	inc	8, %o0		!	dst += 8, src += 8;
	deccc	8, %o2		! } while ((len -= 8) >= 0);
	bge	Lkcopy_doubles
	 inc	8, %o1

	! check for a usual case again (save work)
	btst	7, %o2		! if ((len & 7) == 0)
	be	Lkcopy_done	!	goto kcopy_done;

	 btst	4, %o2		! if ((len & 4)) == 0)
	be,a	Lkcopy_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! [delay slot: if (len & 2)]
	ld	[%o0], %o4	!	*(int *)dst = *(int *)src;
	st	%o4, [%o1]
	inc	4, %o0		!	dst += 4;
	inc	4, %o1		!	src += 4;
	btst	2, %o2		! } [if (len & 2)]

1:
	! mop up trailing word (if present) and byte (if present).
Lkcopy_mopw:
	be	Lkcopy_mopb	! no word, go mop up byte
	 btst	1, %o2		! [delay slot: if (len & 1)]
	ldsh	[%o0], %o4	! *(short *)dst = *(short *)src;
	be	Lkcopy_done	! if ((len & 1) == 0) goto done;
	 sth	%o4, [%o1]
	ldsb	[%o0 + 2], %o4	! dst[2] = src[2];
	stb	%o4, [%o1 + 2]
	membar	#Sync		! Make sure all traps are taken
	clr	%o0
	retl
	 st	%g0, [%o5 + PCB_ONFAULT]
	NOTREACHED

	! mop up trailing byte (if present).
Lkcopy_mopb:
	bne,a	1f
	 ldsb	[%o0], %o4

Lkcopy_done:
	clr	%o0
	membar	#Sync		! Make sure all traps are taken
	retl
	 st	%g0, [%o5 + PCB_ONFAULT]! clear onfault

1:
	stb	%o4,[%o1]
	clr	%o0
	membar	#Sync		! Make sure all traps are taken
	retl
	 st	%g0, [%o5 + PCB_ONFAULT]! clear onfault
	
Lkcerr:
#ifdef DEBUG
	set	pmapdebug, %o4
	ld	[%o4], %o4
	btst	0x80, %o4	! PDB_COPY
	bz,pt	%icc, 3f
	 nop
	save	%sp, -CC64FSZ, %sp
	set	2f, %o0
	call	printf
	 nop
!	ta	1; nop
	ba	3f
	 restore
2:	.asciz	"kcopy error\n"
	_ALIGN
3:
#endif
	st	%g0, [%o5 + PCB_ONFAULT]! clear onfault
	retl				! and return error indicator
	 mov	-1, %o0

#if 0
/*
 * This version uses the UltraSPARC's v9 block copy extensions.
 * We need to use the floating point registers.  However, they
 * get disabled on entering the kernel.  When we try to access
 * them we should trap, which will result in saving them.
 *
 * Otherwise we can simply save them to the stack.
 */
Lbcopy_vis:

		
/*	2) Align source & dest		*/
	alignaddr	%o1, x, %o4	! This is our destination
	alignaddr	%o0, x, %o3
/*	3) load in the first batch	*/
	ldda		[%o3] ASI_BLK_P, %f0
loop:
	faligndata	%f0, %f2, %f34
	faligndata	%f2, %f4, %f36
	faligndata	%f4, %f6, %f38
	faligndata	%f6, %f8, %f40
	faligndata	%f8, %f10, %f42
	faligndata	%f10, %f12, %f44
	faligndata	%f12, %f14, %f46
	addcc		%l0, -1, %l0
	bg,pt		l1
	 fmovd		%f14, %f48
	/* end of loop handling */
l1:
	ldda		[regaddr] ASI_BLK_P, %f0
	stda		%f32, [regaddr] ASI_BLK_P
	faligndata	%f48, %f16, %f32
	faligndata	%f16, %f18, %f34
	faligndata	%f18, %f20, %f36
	faligndata	%f20, %f22, %f38
	faligndata	%f22, %f24, %f40
	faligndata	%f24, %f26, %f42
	faligndata	%f26, %f28, %f44
	faligndata	%f28, %f30, %f46
	addcc		%l0, -1, %l0
	bne,pt		done
	 fmovd		%f30, %f48
	ldda		[regaddr] ASI_BLK_P, %f16
	stda		%f32, [regaddr] ASI_BLK_P
	ba		loop
	 faligndata	%f48, %f0, %f32
done:	

#endif
	
/*
 * ovbcopy(src, dst, len): like bcopy, but regions may overlap.
 */
ENTRY(ovbcopy)
	cmp	%o0, %o1	! src < dst?
	bgeu	Lbcopy_start	! no, go copy forwards as via bcopy
	 cmp	%o2, BCOPY_SMALL! (check length for doublecopy first)

	/*
	 * Since src comes before dst, and the regions might overlap,
	 * we have to do the copy starting at the end and working backwards.
	 */
	add	%o2, %o0, %o0	! src += len
	add	%o2, %o1, %o1	! dst += len
	bge,a	Lback_fancy	! if len >= BCOPY_SMALL, go be fancy
	 btst	3, %o0

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
	 EMPTY
0:
	dec	%o0		!	*--dst = *--src;
	ldsb	[%o0], %o4
	dec	%o1
	deccc	%o2
	bge	0b
	 stb	%o4, [%o1]
1:
	retl
	 nop

	/*
	 * Plenty to copy, try to be optimal.
	 * We only bother with word/halfword/byte copies here.
	 */
Lback_fancy:
!	btst	3, %o0		! done already
	bnz	1f		! if ((src & 3) == 0 &&
	 btst	3, %o1		!     (dst & 3) == 0)
	bz,a	Lback_words	!	goto words;
	 dec	4, %o2		! (done early for word copy)

1:
	/*
	 * See if the low bits match.
	 */
	xor	%o0, %o1, %o3	! t = src ^ dst;
	btst	1, %o3
	bz,a	3f		! if (t & 1) == 0, can do better
	 btst	1, %o0

	/*
	 * Nope; gotta do byte copy.
	 */
2:
	dec	%o0		! do {
	ldsb	[%o0], %o4	!	*--dst = *--src;
	dec	%o1
	deccc	%o2		! } while (--len != 0);
	bnz	2b
	 stb	%o4, [%o1]
	retl
	 nop

3:
	/*
	 * Can do halfword or word copy, but might have to copy 1 byte first.
	 */
!	btst	1, %o0		! done earlier
	bz,a	4f		! if (src & 1) {	/* copy 1 byte */
	 btst	2, %o3		! (done early)
	dec	%o0		!	*--dst = *--src;
	ldsb	[%o0], %o4
	dec	%o1
	stb	%o4, [%o1]
	dec	%o2		!	len--;
	btst	2, %o3		! }

4:
	/*
	 * See if we can do a word copy ((t&2) == 0).
	 */
!	btst	2, %o3		! done earlier
	bz,a	6f		! if (t & 2) == 0, can do word copy
	 btst	2, %o0		! (src&2, done early)

	/*
	 * Gotta do halfword copy.
	 */
	dec	2, %o2		! len -= 2;
5:
	dec	2, %o0		! do {
	ldsh	[%o0], %o4	!	src -= 2;
	dec	2, %o1		!	dst -= 2;
	deccc	2, %o0		!	*(short *)dst = *(short *)src;
	bge	5b		! } while ((len -= 2) >= 0);
	 sth	%o4, [%o1]
	b	Lback_mopb	! goto mop_up_byte;
	 btst	1, %o2		! (len&1, done early)

6:
	/*
	 * We can do word copies, but we might have to copy
	 * one halfword first.
	 */
!	btst	2, %o0		! done already
	bz	7f		! if (src & 2) {
	 dec	4, %o2		! (len -= 4, done early)
	dec	2, %o0		!	src -= 2, dst -= 2;
	ldsh	[%o0], %o4	!	*(short *)dst = *(short *)src;
	dec	2, %o1
	sth	%o4, [%o1]
	dec	2, %o2		!	len -= 2;
				! }

7:
Lback_words:
	/*
	 * Do word copies (backwards), then mop up trailing halfword
	 * and byte if any.
	 */
!	dec	4, %o2		! len -= 4, done already
0:				! do {
	dec	4, %o0		!	src -= 4;
	dec	4, %o1		!	src -= 4;
	ld	[%o0], %o4	!	*(int *)dst = *(int *)src;
	deccc	4, %o2		! } while ((len -= 4) >= 0);
	bge	0b
	 st	%o4, [%o1]

	/*
	 * Check for trailing shortword.
	 */
	btst	2, %o2		! if (len & 2) {
	bz,a	1f
	 btst	1, %o2		! (len&1, done early)
	dec	2, %o0		!	src -= 2, dst -= 2;
	ldsh	[%o0], %o4	!	*(short *)dst = *(short *)src;
	dec	2, %o1
	sth	%o4, [%o1]	! }
	btst	1, %o2

	/*
	 * Check for trailing byte.
	 */
1:
Lback_mopb:
!	btst	1, %o2		! (done already)
	bnz,a	1f		! if (len & 1) {
	 ldsb	[%o0 - 1], %o4	!	b = src[-1];
	retl
	 nop
1:
	retl			!	dst[-1] = b;
	 stb	%o4, [%o1 - 1]	! }


/*
 * savefpstate(f) struct fpstate *f;
 *
 * Store the current FPU state.  The first `st %fsr' may cause a trap;
 * our trap handler knows how to recover (by `returning' to savefpcont).
 */
 /* XXXXXXXXXX  Assume called created a proper stack frame */
ENTRY(savefpstate)
	flushw			! Make sure we don't have stack probs & lose hibits of %o
	rdpr	%pstate, %o1		! enable FP before we begin
	wr	%g0, FPRS_FEF, %fprs
	or	%o1, PSTATE_PEF, %o1
	wrpr	%o1, 0, %pstate
	/* do some setup work while we wait for PSR_EF to turn on */
	set	FSR_QNE, %o5		! QNE = 0x2000, too big for immediate
	clr	%o3			! qsize = 0;
special_fp_store:
	/* This may need to be done w/rdpr/stx combo */
	stx	%fsr, [%o0 + FS_FSR]	! f->fs_fsr = getfsr();
	/*
	 * Even if the preceding instruction did not trap, the queue
	 * is not necessarily empty: this state save might be happening
	 * because user code tried to store %fsr and took the FPU
	 * from `exception pending' mode to `exception' mode.
	 * So we still have to check the blasted QNE bit.
	 * With any luck it will usually not be set.
	 */
	ldx	[%o0 + FS_FSR], %o4	! if (f->fs_fsr & QNE)
	add	%o0, FS_REGS, %o2
	btst	%o5, %o4
	bnz	Lfp_storeq		!	goto storeq;
Lfp_finish:
	 btst	BLOCK_ALIGN, %o2	! Needs to be re-executed
	bnz,pn	%icc, 1f		! Check alignment
	 st	%o3, [%o0 + FS_QSIZE]	! f->fs_qsize = qsize;
	stda	%f0, [%o2] ASI_BLK_P	! f->fs_f0 = etc;
	inc	BLOCK_SIZE, %o2
	stda	%f16, [%o2] ASI_BLK_P
	inc	BLOCK_SIZE, %o2
	stda	%f32, [%o2] ASI_BLK_P
	inc	BLOCK_SIZE, %o2
	retl
	 stda	%f48, [%o2] ASI_BLK_COMMIT_P
1:
	std	%f0, [%o0 + FS_REGS + (4*0)]	! f->fs_f0 = etc;
	std	%f2, [%o0 + FS_REGS + (4*2)]
	std	%f4, [%o0 + FS_REGS + (4*4)]
	std	%f6, [%o0 + FS_REGS + (4*6)]
	std	%f8, [%o0 + FS_REGS + (4*8)]
	std	%f10, [%o0 + FS_REGS + (4*10)]
	std	%f12, [%o0 + FS_REGS + (4*12)]
	std	%f14, [%o0 + FS_REGS + (4*14)]
	std	%f16, [%o0 + FS_REGS + (4*16)]
	std	%f18, [%o0 + FS_REGS + (4*18)]
	std	%f20, [%o0 + FS_REGS + (4*20)]
	std	%f22, [%o0 + FS_REGS + (4*22)]
	std	%f24, [%o0 + FS_REGS + (4*24)]
	std	%f26, [%o0 + FS_REGS + (4*26)]
	std	%f28, [%o0 + FS_REGS + (4*28)]
	std	%f30, [%o0 + FS_REGS + (4*30)]
	std	%f32, [%o0 + FS_REGS + (4*32)]
	std	%f34, [%o0 + FS_REGS + (4*34)]
	std	%f36, [%o0 + FS_REGS + (4*36)]
	std	%f38, [%o0 + FS_REGS + (4*38)]
	std	%f40, [%o0 + FS_REGS + (4*40)]
	std	%f42, [%o0 + FS_REGS + (4*42)]
	std	%f44, [%o0 + FS_REGS + (4*44)]
	std	%f46, [%o0 + FS_REGS + (4*46)]
	std	%f48, [%o0 + FS_REGS + (4*48)]
	std	%f50, [%o0 + FS_REGS + (4*50)]
	std	%f52, [%o0 + FS_REGS + (4*52)]
	std	%f54, [%o0 + FS_REGS + (4*54)]
	std	%f56, [%o0 + FS_REGS + (4*56)]
	std	%f58, [%o0 + FS_REGS + (4*58)]
	std	%f60, [%o0 + FS_REGS + (4*60)]
	retl
	 std	%f62, [%o0 + FS_REGS + (4*62)]
	
/*
 * Store the (now known nonempty) FP queue.
 * We have to reread the fsr each time in order to get the new QNE bit.
 *
 * UltraSPARCs don't have floating point queues.
 */
Lfp_storeq:
	add	%o0, FS_QUEUE, %o1	! q = &f->fs_queue[0];
1:
	rdpr	%fq, %o4
	stx	%o4, [%o1 + %o3]	! q[qsize++] = fsr_qfront();
	stx	%fsr, [%o0 + FS_FSR] 	! reread fsr
	ldx	[%o0 + FS_FSR], %o4	! if fsr & QNE, loop
	btst	%o5, %o4
	bnz	1b
	 inc	8, %o3
	b	Lfp_finish		! set qsize and finish storing fregs
	 srl	%o3, 3, %o3		! (but first fix qsize)

/*
 * The fsr store trapped.  Do it again; this time it will not trap.
 * We could just have the trap handler return to the `st %fsr', but
 * if for some reason it *does* trap, that would lock us into a tight
 * loop.  This way we panic instead.  Whoopee.
 */
savefpcont:
	b	special_fp_store + 4	! continue
	 stx	%fsr, [%o0 + FS_FSR]	! but first finish the %fsr store

/*
 * Load FPU state.
 */
ENTRY(loadfpstate)
	flushw			! Make sure we don't have stack probs & lose hibits of %o
	rdpr	%pstate, %o1		! enable FP before we begin
	set	PSTATE_PEF, %o2
	wr	%g0, FPRS_FEF, %fprs
	or	%o1, %o2, %o1
	wrpr	%o1, 0, %pstate
	ldx	[%o0 + FS_FSR], %fsr	! setfsr(f->fs_fsr);
	add	%o0, FS_REGS, %o3
	btst	BLOCK_ALIGN, %o3
	bne,a,pt	%icc, 1f	! Only use block loads on aligned blocks
	 ldd	[%o0 + FS_REGS + (4*0)], %f0
	ldda	[%o3] ASI_BLK_P, %f0
	inc	BLOCK_SIZE, %o0
	ldda	[%o3] ASI_BLK_P, %f16
	inc	BLOCK_SIZE, %o0
	ldda	[%o3] ASI_BLK_P, %f32
	inc	BLOCK_SIZE, %o0
	retl
	 ldda	[%o3] ASI_BLK_P, %f48	
1:
	/* Unaligned -- needs to be done the long way
	ldd	[%o0 + FS_REGS + (4*0)], %f0
	ldd	[%o0 + FS_REGS + (4*2)], %f2
	ldd	[%o0 + FS_REGS + (4*4)], %f4
	ldd	[%o0 + FS_REGS + (4*6)], %f6
	ldd	[%o0 + FS_REGS + (4*8)], %f8
	ldd	[%o0 + FS_REGS + (4*10)], %f10
	ldd	[%o0 + FS_REGS + (4*12)], %f12
	ldd	[%o0 + FS_REGS + (4*14)], %f14
	ldd	[%o0 + FS_REGS + (4*16)], %f16
	ldd	[%o0 + FS_REGS + (4*18)], %f18
	ldd	[%o0 + FS_REGS + (4*20)], %f20
	ldd	[%o0 + FS_REGS + (4*22)], %f22
	ldd	[%o0 + FS_REGS + (4*24)], %f24
	ldd	[%o0 + FS_REGS + (4*26)], %f26
	ldd	[%o0 + FS_REGS + (4*28)], %f28
	ldd	[%o0 + FS_REGS + (4*30)], %f30
	ldd	[%o0 + FS_REGS + (4*32)], %f32
	ldd	[%o0 + FS_REGS + (4*34)], %f34
	ldd	[%o0 + FS_REGS + (4*36)], %f36
	ldd	[%o0 + FS_REGS + (4*38)], %f38
	ldd	[%o0 + FS_REGS + (4*40)], %f40
	ldd	[%o0 + FS_REGS + (4*42)], %f42
	ldd	[%o0 + FS_REGS + (4*44)], %f44
	ldd	[%o0 + FS_REGS + (4*46)], %f46
	ldd	[%o0 + FS_REGS + (4*48)], %f48
	ldd	[%o0 + FS_REGS + (4*50)], %f50
	ldd	[%o0 + FS_REGS + (4*52)], %f52
	ldd	[%o0 + FS_REGS + (4*54)], %f54
	ldd	[%o0 + FS_REGS + (4*56)], %f56
	ldd	[%o0 + FS_REGS + (4*58)], %f58
	ldd	[%o0 + FS_REGS + (4*60)], %f60
	retl
 	 ldd	[%o0 + FS_REGS + (4*62)], %f62
	
/*
 * ienab_bis(bis) int bis;
 * ienab_bic(bic) int bic;
 *
 * Set and clear bits in the interrupt register.
 */

/*
 * sun4u has separate asr's for clearing/setting the interrupt mask.
 */
ENTRY(ienab_bis)
	retl
	 wr	%o0, 0, SET_SOFTINT	! SET_SOFTINT

ENTRY(ienab_bic)
	retl
	 wr	%o0, 0, CLEAR_SOFTINT	! CLEAR_SOFTINT
#if 0
#ifndef POPC
/*
 * ffs(), using table lookup.
 * The process switch code shares the table, so we just put the
 * whole thing here.
 */
ffstab:
	.byte	-24,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1 /* 00-0f */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 10-1f */
	.byte	6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 20-2f */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 30-3f */
	.byte	7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 40-4f */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 50-5f */
	.byte	6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 60-6f */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 70-7f */
	.byte	8,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 80-8f */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 10-9f */
	.byte	6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* a0-af */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* b0-bf */
	.byte	7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* c0-cf */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* d0-df */
	.byte	6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* e0-ef */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* f0-ff */

/*
 * We use a table lookup on each byte.
 *
 * In each section below, %o1 is the current byte (0, 1, 2, or 3).
 * The last byte is handled specially: for the first three,
 * if that byte is nonzero, we return the table value
 * (plus 0, 8, or 16 for the byte number), but for the last
 * one, we just return the table value plus 24.  This means
 * that ffstab[0] must be -24 so that ffs(0) will return 0.
 */
ENTRY(ffs)
	set	ffstab, %o2
	andcc	%o0, 0xff, %o1	! get low byte
	bz,a	1f		! try again if 0
	srl	%o0, 8, %o0	! delay slot, get ready for next byte

	retl			! return ffstab[%o1]
	ldsb	[%o2 + %o1], %o0

1:
	andcc	%o0, 0xff, %o1	! byte 1 like byte 0...
	bz,a	2f
	srl	%o0, 8, %o0	! (use delay to prepare for byte 2)

	ldsb	[%o2 + %o1], %o0
	retl			! return ffstab[%o1] + 8
	add	%o0, 8, %o0

2:
	andcc	%o0, 0xff, %o1
	bz,a	3f
	srl	%o0, 8, %o0	! (prepare for byte 3)

	ldsb	[%o2 + %o1], %o0
	retl			! return ffstab[%o1] + 16
	add	%o0, 16, %o0

3:				! just return ffstab[%o0] + 24
	ldsb	[%o2 + %o0], %o0
	retl
	add	%o0, 24, %o0
#else
	/*
	 * We have a popcount instruction:	 use it.
	 * only uses %o0, %o1, %o2
	 *
	 * Here's the pseudo-code from the v9 spec:
	 *
	 * int ffs(unsigned zz) {
	 *	return popc( zz ^ ( ~ (-zz)));
	 * }
	 *
	 * XXXX sptifires don't implement popc.
	 */
ENTRY(ffs)
	neg	%o0, %o1				! %o1 = -zz
	xnor	%o0, %o1, %o2				! %o2 = zz ^ ~ -zz
	popc	%o2, %o1
	movrz	%o0, %g0, %o1				! result of ffs(0) should be zero
	retl
	 mov	%o1, %o0

	
#endif
/*
 * Here is a very good random number generator.  This implementation is
 * based on _Two Fast Implementations of the `Minimal Standard' Random
 * Number Generator_, David G. Carta, Communications of the ACM, Jan 1990,
 * Vol 33 No 1.
 */
/*
 * This should be rewritten using the mul instr. if I ever understand what it
 * does.
 */
	.data
randseed:
	.word	1
	.text
ENTRY(random)
	sethi	%hi(16807), %o1
	wr	%o1, %lo(16807), %y
	 sethi	%hi(randseed), %o5
	 ld	[%o5 + %lo(randseed)], %o0
	 andcc	%g0, 0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %o0, %o2
	mulscc  %o2, %g0, %o2
	rd	%y, %o3
	srl	%o2, 16, %o1
	set	0xffff, %o4
	and	%o4, %o2, %o0
	sll	%o0, 15, %o0
	srl	%o3, 17, %o3
	or	%o3, %o0, %o0
	addcc	%o0, %o1, %o0
	bneg	1f
	 sethi	%hi(0x7fffffff), %o1
	retl
	 st	%o0, [%o5 + %lo(randseed)]
1:
	or	%o1, %lo(0x7fffffff), %o1
	add	%o0, 1, %o0
	and	%o1, %o0, %o0
	retl
	 st	%o0, [%o5 + %lo(randseed)]
#endif
/*
 * void microtime(struct timeval *tv)
 *
 * LBL's sparc bsd 'microtime': We don't need to spl (so this routine
 * can be a leaf routine) and we don't keep a 'last' timeval (there
 * can't be two calls to this routine in a microsecond).  This seems to
 * be about 20 times faster than the Sun code on an SS-2. - vj
 *
 * Read time values from slowest-changing to fastest-changing,
 * then re-read out to slowest.  If the values read before
 * the innermost match those read after, the innermost value
 * is consistent with the outer values.  If not, it may not
 * be and we must retry.  Typically this loop runs only once;
 * occasionally it runs twice, and only rarely does it run longer.
 *
 * If we used the %tick register we could go into the nano-seconds,
 * and it must run for at least 10 years according to the v9 spec.
 */
#define MICROPERSEC	(1000000)
	
	.data
	.align	8
	.globl	_C_LABEL(cpu_clockrate)
_C_LABEL(cpu_clockrate):	
	.xword	142857143					! 1/7ns or ~ 143MHz  Really should be 142857142.85
	.text
	
ENTRY(microtime)
#ifdef TRY_TICK
	rdpr	%tick, %g1
	sethi	%hi(_C_LABEL(cpu_clockrate)), %o4
	sethi	%hi(MICROPERSEC), %o2
	ldx	[%o4 + %lo(_C_LABEL(cpu_clockrate))], %o4	! Get scale factor
	or	%o2, %lo(MICROPERSEC), %o2
!	sethi	%hi(_C_LABEL(timerblurb), %o5			! This is if we plan to tune the clock
!	ld	[%o5 + %lo(_C_LABEL(timerblurb))], %o5		!  with respect to the counter/timer
	udivx	%o4, %o2, %o4
	mulx	%g1, %o4, %g1					! Scale it: N * Hz / 1 x 10^6 = ticks

	srlx	%g1, 32, %o3					! Isolate high word
	st	%o3, [%o0]					! and store it 
	retl
	 st	%g1, [%o0+4]					! Save time_t low word
#else
	sethi	%hi(timerreg_4u), %g3
	sethi	%hi(_C_LABEL(time)), %g2
	ld	[%g3+%lo(timerreg_4u)], %g3		! usec counter
2:
	ld	[%g2+%lo(_C_LABEL(time))], %o2		! time.tv_sec & time.tv_usec
	ld	[%g2+%lo(_C_LABEL(time))+4], %o3	! time.tv_sec & time.tv_usec
	ldx	[%g3], %g7				! Load usec timer valuse
	ld	[%g2+%lo(_C_LABEL(time))], %g4		! see if time values changed
	ld	[%g2+%lo(_C_LABEL(time))+4], %g5	! see if time values changed
	cmp	%g4, %o2
	bne	2b				! if time.tv_sec changed
	 cmp	%g5, %o3
	bne	2b				! if time.tv_usec changed
	 tst	%g7

	bpos	3f				! reached limit?
	 srl	%g7, TMR_SHIFT, %g7		! convert counter to usec
	sethi	%hi(_C_LABEL(tick)), %g4			! bump usec by 1 tick
	ld	[%g4+%lo(_C_LABEL(tick))], %o1
	set	TMR_MASK, %g5
	add	%o1, %o3, %o3
	and	%g7, %g5, %g7
3:
	add	%g7, %o3, %o3
	set	1000000, %g5			! normalize usec value
	cmp	%o3, %g5
	bl,a	4f
	 st	%o2, [%o0]			! (should be able to std here)
	add	%o2, 1, %o2			! overflow
	sub	%o3, %g5, %o3
	st	%o2, [%o0]			! (should be able to std here)
4:
	retl
	 st	%o3, [%o0+4]
#endif
/*
 * delay function
 *
 * void delay(N)  -- delay N microseconds
 *
 * Register usage: %o0 = "N" number of usecs to go (counts down to zero)
 *		   %o1 = "timerblurb" (stays constant)
 *		   %o2 = counter for 1 usec (counts down from %o1 to zero)
 *
 *
 *	cpu_clockrate should be tuned during CPU probe to the CPU clockrate in Hz
 *
 */
ENTRY(delay)			! %o0 = n
#ifdef _not44u_
	subcc	%o0, %g0, %g0
	be	2f

	sethi	%hi(_C_LABEL(timerblurb)), %o1
	ld	[%o1 + %lo(_C_LABEL(timerblurb))], %o1	! %o1 = timerblurb

	 addcc	%o1, %g0, %o2		! %o2 = cntr (start @ %o1), clear CCs
					! first time through only

					! delay 1 usec
1:	bne	1b			! come back here if not done
	 subcc	%o2, 1, %o2		! %o2 = %o2 - 1 [delay slot]

	subcc	%o0, 1, %o0		! %o0 = %o0 - 1
	bne	1b			! done yet?
	 addcc	%o1, %g0, %o2		! reinit %o2 and CCs  [delay slot]
					! harmless if not branching
2:
	retl				! return
	 nop				! [delay slot]
#else
	rdpr	%tick, %g1					! Take timer snapshot
	sethi	%hi(_C_LABEL(cpu_clockrate)), %g2
	sethi	%hi(MICROPERSEC), %o2
	ldx	[%g2 + %lo(_C_LABEL(cpu_clockrate))], %g2	! Get scale factor
	or	%o2, %lo(MICROPERSEC), %o2
!	sethi	%hi(_C_LABEL(timerblurb), %o5			! This is if we plan to tune the clock
!	ld	[%o5 + %lo(_C_LABEL(timerblurb))], %o5		!  with respect to the counter/timer
	mulx	%o0, %g2, %g2					! Scale it: N * Hz / 1 x 10^6 = ticks
	udivx	%g2, %o2, %g2
	add	%g1, %g2, %g2
!	add	%o5, %g2, %g2					! But this gets complicated
	rdpr	%tick, %g1					! Top of next itr
1:
	cmp	%g1, %g2
	bl,a,pn %xcc, 1b					! Done?
	 rdpr	%tick, %g1

	retl
	 nop
	
	
#endif
ENTRY(setjmp)
	save	%sp, -CC64FSZ, %sp	! Need a frame to return to.
	flushw
	stx	%fp, [%i0+0]	! 64-bit stack pointer
	stx	%i7, [%i0+8]	! 64-bit return pc
	ret
	 restore	%g0, 0, %o0

Lpanic_ljmp:
	.asciz	"longjmp botch"
	_ALIGN

ENTRY(longjmp)
	save	%sp, -CC64FSZ, %sp	! prepare to restore to (old) frame
	flushw
	mov	1, %i2
	ldx	[%i0+0], %fp	! get return stack
	movrz	%i1, %i1, %i2	! compute v ? v : 1 
	ldx	[%i0+8], %i7	! get rpc
	ret
	 restore	%i2, 0, %o0

#ifdef DDB
	/*
	 * Debug stuff.  Dump the trap registers into buffer & set tl=0.
	 *
	 *  %o0 = *ts
	 */
	ENTRY(savetstate)
	mov	%o0, %o1
	CHKPT(%o4,%o3,0x28)
	rdpr	%tl, %o0
	brz	%o0, 2f
	 mov	%o0, %o2
1:	
	rdpr	%tstate, %o3
	stx	%o3, [%o1]
	deccc	%o2
	inc	8, %o1
	rdpr	%tpc, %o4
	stx	%o4, [%o1]
	inc	8, %o1
	rdpr	%tnpc, %o5
	stx	%o5, [%o1]
	inc	8, %o1
	rdpr	%tt, %o4
	stx	%o4, [%o1]
	inc	8, %o1
	bnz	1b
	 wrpr	%o2, 0, %tl
2:	
	retl
	 nop

	/*
	 * Debug stuff.  Resore trap registers from buffer.
	 *
	 *  %o0 = %tl
	 *  %o1 = *ts
	 *
	 * Maybe this should be re-written to increment tl instead of decrementing.
	 */
	ENTRY(restoretstate)
	CHKPT(%o4,%o3,0x36)
	flushw			! Make sure we don't have stack probs & lose hibits of %o
	brz,pn	%o0, 2f
	 mov	%o0, %o2	
	CHKPT(%o4,%o3,0x29)
	wrpr	%o0, 0, %tl
1:	
	ldx	[%o1], %o3
	deccc	%o2
	inc	8, %o1
	wrpr	%o3, 0, %tstate
	ldx	[%o1], %o4
	inc	8, %o1
	wrpr	%o4, 0, %tpc
	ldx	[%o1], %o5
	inc	8, %o1
	wrpr	%o5, 0, %tnpc
	ldx	[%o1], %o4
	inc	8, %o1
	wrpr	%o4, 0, %tt
	bnz	1b
	 wrpr	%o2, 0, %tl
2:	
	CHKPT(%o4,%o3,0x30)
	retl
	 wrpr	%o0, 0, %tl

	/*
	 * Switch to context in %o0
	 */
	ENTRY(switchtoctx)
	set	0x030, %o3
	stxa	%o3, [%o3] ASI_DMMU_DEMAP
	membar	#Sync
	stxa	%o3, [%o3] ASI_IMMU_DEMAP
	membar	#Sync
	wr	%g0, ASI_DMMU, %asi
	stxa	%o0, [CTX_SECONDARY] %asi	! Maybe we should invali
	membar	#Sync					! No real reason for this XXXX
	sethi	%hi(KERNBASE), %o2
	flush	%o2
	retl
	 nop
	
#endif /* DDB */
		
	.data
#ifdef DDB
	.globl	_C_LABEL(esym)
_C_LABEL(esym):
	.word	0
	.globl	_C_LABEL(ssym)
_C_LABEL(ssym):
	.word	0
#endif
	.globl	_C_LABEL(cold)
_C_LABEL(cold):
	.word	1		! cold start flag

	.globl	_C_LABEL(proc0paddr)
_C_LABEL(proc0paddr):
	.word	_C_LABEL(u0)		! KVA of proc0 uarea

/* interrupt counters	XXX THESE BELONG ELSEWHERE (if anywhere) */
	.globl	_C_LABEL(intrcnt), _C_LABEL(eintrcnt), _C_LABEL(intrnames), _C_LABEL(eintrnames)
_C_LABEL(intrnames):
	.asciz	"spur"
	.asciz	"lev1"
	.asciz	"lev2"
	.asciz	"lev3"
	.asciz	"lev4"
	.asciz	"lev5"
	.asciz	"lev6"
	.asciz	"lev7"
	.asciz  "lev8"
	.asciz	"lev9"
	.asciz	"clock"
	.asciz	"lev11"
	.asciz	"lev12"
	.asciz	"lev13"
	.asciz	"prof"
	.asciz  "lev15"
_C_LABEL(eintrnames):
	_ALIGN
_C_LABEL(intrcnt):
	.space	4*15
_C_LABEL(eintrcnt):

	.comm	_C_LABEL(nwindows), 4
	.comm	_C_LABEL(promvec), 4
	.comm	_C_LABEL(curproc), 4
	.comm	_C_LABEL(qs), 32 * 8
	.comm	_C_LABEL(whichqs), 4
