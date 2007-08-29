/*	$NetBSD: locore.s,v 1.232.2.1 2007/08/29 15:21:07 liamjfoy Exp $	*/

/*
 * Copyright (c) 1996-2002 Eduardo Horvath
 * Copyright (c) 1996 Paul Kranenburg
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College.
 *	All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.
 *	All rights reserved.
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
 *    documentation and/or other materials provided with the
 *    distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by Paul Kranenburg.
 * 4. Neither the name of the University nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 *	@(#)locore.s	8.4 (Berkeley) 12/10/93
 */

#define	SPITFIRE		/* We don't support Cheetah (USIII) yet */
#undef	PARANOID		/* Extremely expensive consistency checks */
#undef	NO_VCACHE		/* Map w/D$ disabled */
#undef	TRAPSTATS		/* Count traps */
#undef	TRAPS_USE_IG		/* Use Interrupt Globals for all traps */
#define	HWREF			/* Track ref/mod bits in trap handlers */
#undef	DCACHE_BUG		/* Flush D$ around ASI_PHYS accesses */
#undef	NO_TSB			/* Don't use TSB */
#undef	SCHED_DEBUG
#define	USE_BLOCK_STORE_LOAD	/* enable block load/store ops */

#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "opt_multiprocessor.h"
#include "opt_compat_netbsd.h"
#include "opt_compat_netbsd32.h"
#include "opt_lockdebug.h"

#include "assym.h"
#include <machine/param.h>
#include <sparc64/sparc64/intreg.h>
#include <sparc64/sparc64/timerreg.h>
#include <machine/ctlreg.h>
#include <machine/psl.h>
#include <machine/signal.h>
#include <machine/trap.h>
#include <machine/frame.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/asm.h>
#include <sys/syscall.h>

#include "ksyms.h"

/* A few convenient abbreviations for trapframe fields. */
#define	TF_G	TF_GLOBAL
#define	TF_O	TF_OUT
#define	TF_L	TF_LOCAL
#define	TF_I	TF_IN

#undef	CURLWP
#undef	CPCB
#undef	FPLWP

#define	CURLWP	(CPUINFO_VA + CI_CURLWP)
#define	CPCB	(CPUINFO_VA + CI_CPCB)
#define	FPLWP	(CPUINFO_VA + CI_FPLWP)
#define	IDLE_U  (CPUINFO_VA + CI_IDLE_U)

/* Let us use same syntax as C code */
#define Debugger()	ta	1; nop

#if 1
/*
 * Try to issue an elf note to ask the Solaris
 * bootloader to align the kernel properly.
 */
	.section	.note
	.word	0x0d
	.word	4		! Dunno why
	.word	1
0:	.asciz	"SUNW Solaris"
1:
	.align	4
	.word	0x0400000
#endif

	.register	%g2,#scratch
	.register	%g3,#scratch

/*
 * Here are some defines to try to maintain consistency but still
 * support 32-and 64-bit compilers.
 */
#ifdef _LP64
/* reg that points to base of data/text segment */
#define	BASEREG	%g4
/* first constants for storage allocation */
#define LNGSZ		8
#define LNGSHFT		3
#define PTRSZ		8
#define PTRSHFT		3
#define POINTER		.xword
#define ULONG		.xword
/* Now instructions to load/store pointers & long ints */
#define LDLNG		ldx
#define LDULNG		ldx
#define STLNG		stx
#define STULNG		stx
#define LDPTR		ldx
#define LDPTRA		ldxa
#define STPTR		stx
#define STPTRA		stxa
#define	CASPTR		casxa
/* Now something to calculate the stack bias */
#define STKB		BIAS
#define	CCCR		%xcc
#else
#define	BASEREG		%g0
#define LNGSZ		4
#define LNGSHFT		2
#define PTRSZ		4
#define PTRSHFT		2
#define POINTER		.word
#define ULONG		.word
/* Instructions to load/store pointers & long ints */
#define LDLNG		ldsw
#define LDULNG		lduw
#define STLNG		stw
#define STULNG		stw
#define LDPTR		lduw
#define LDPTRA		lduwa
#define STPTR		stw
#define STPTRA		stwa
#define	CASPTR		casa
#define STKB		0
#define	CCCR		%icc
#endif

/*
 * GNU assembler does not understand `.empty' directive; Sun assembler
 * gripes about labels without it.  To allow cross-compilation using
 * the Sun assembler, and because .empty directives are useful
 * documentation, we use this trick.
 */
#ifdef SUN_AS
#define	EMPTY	.empty
#else
#define	EMPTY	/* .empty */
#endif

/* use as needed to align things on longword boundaries */
#define	_ALIGN	.align 8
#define ICACHE_ALIGN	.align	32

/* Give this real authority: reset the machine */
#define NOTREACHED	sir

/*
 * This macro will clear out a cache line before an explicit
 * access to that location.  It's mostly used to make certain
 * loads bypassing the D$ do not get stale D$ data.
 *
 * It uses a register with the address to clear and a temporary
 * which is destroyed.
 */
#ifdef DCACHE_BUG
#define DLFLUSH(a,t) \
	andn	a, 0x1f, t; \
	stxa	%g0, [ t ] ASI_DCACHE_TAG; \
	membar	#Sync
/* The following can be used if the pointer is 16-byte aligned */
#define DLFLUSH2(t) \
	stxa	%g0, [ t ] ASI_DCACHE_TAG; \
	membar	#Sync
#else
#define DLFLUSH(a,t)
#define DLFLUSH2(t)
#endif


/*
 * Combine 2 regs -- used to convert 64-bit ILP32
 * values to LP64.
 */
#define	COMBINE(r1, r2, d)	\
	sllx	r1, 32, d;	\
	or	d, r2, d

/*
 * Split 64-bit value in 1 reg into high and low halves.
 * Used for ILP32 return values.
 */
#define	SPLIT(r0, r1)		\
	srl	r0, 0, r1;	\
	srlx	r0, 32, r0


/*
 * A handy macro for maintaining instrumentation counters.
 * Note that this clobbers %o0, %o1 and %o2.  Normal usage is
 * something like:
 *	foointr:
 *		TRAP_SETUP(...)		! makes %o registers safe
 *		INCR(_C_LABEL(cnt)+V_FOO)	! count a foo
 */
#define INCR(what) \
	sethi	%hi(what), %o0; \
	or	%o0, %lo(what), %o0; \
99:	\
	lduw	[%o0], %o1; \
	add	%o1, 1, %o2; \
	casa	[%o0] ASI_P, %o1, %o2; \
	cmp	%o1, %o2; \
	bne,pn	%icc, 99b; \
	 nop

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

/* Load strings address into register; NOTE: hidden local label 99 */
#define LOAD_ASCIZ(reg, s)	\
	set	99f, reg ;	\
	.data ;			\
99:	.asciz	s ;		\
	_ALIGN ;		\
	.text

/*
 * Handy stack conversion macros.
 * They correctly switch to requested stack type
 * regardless of the current stack.
 */

#define TO_STACK64(size)					\
	save	%sp, size, %sp;					\
	add	%sp, -BIAS, %o0; /* Convert to 64-bits */	\
	andcc	%sp, 1, %g0; /* 64-bit stack? */		\
	movz	%icc, %o0, %sp

#define TO_STACK32(size)					\
	save	%sp, size, %sp;					\
	add	%sp, +BIAS, %o0; /* Convert to 32-bits */	\
	andcc	%sp, 1, %g0; /* 64-bit stack? */		\
	movnz	%icc, %o0, %sp

#ifdef _LP64
#define	STACKFRAME(size)	TO_STACK64(size)
#else
#define	STACKFRAME(size)	TO_STACK32(size)
#endif

/*
 * The following routines allow fpu use in the kernel.
 *
 * They allocate a stack frame and use all local regs.  Extra
 * local storage can be requested by setting the siz parameter,
 * and can be accessed at %sp+CC64FSZ.
 */

#define ENABLE_FPU(siz)									     \
	save	%sp, -(CC64FSZ), %sp;		/* Allocate a stack frame */		     \
	sethi	%hi(FPLWP), %l1;							     \
	add	%fp, STKB-FS_SIZE, %l0;		/* Allocate a fpstate */		     \
	LDPTR	[%l1 + %lo(FPLWP)], %l2;	/* Load fplwp */			     \
	andn	%l0, BLOCK_ALIGN, %l0;		/* Align it */				     \
	clr	%l3;				/* NULL fpstate */			     \
	brz,pt	%l2, 1f;			/* fplwp == NULL? */			     \
	 add	%l0, -STKB-CC64FSZ-(siz), %sp;	/* Set proper %sp */			     \
	LDPTR	[%l2 + L_FPSTATE], %l3;						    	     \
	brz,pn	%l3, 1f;			/* Make sure we have an fpstate */	     \
	 mov	%l3, %o0;								     \
	call	_C_LABEL(savefpstate);		/* Save the old fpstate */		     \
1:											     \
	 set	EINTSTACK-STKB, %l4;		/* Are we on intr stack? */		     \
	cmp	%sp, %l4;								     \
	bgu,pt	%xcc, 1f;								     \
	 set	INTSTACK-STKB, %l4;							     \
	cmp	%sp, %l4;								     \
	blu	%xcc, 1f;								     \
0:											     \
	 sethi	%hi(_C_LABEL(lwp0)), %l4;	/* Yes, use proc0 */			     \
	ba,pt	%xcc, 2f;			/* XXXX needs to change to CPUs idle proc */ \
	 or	%l4, %lo(_C_LABEL(lwp0)), %l5;						     \
1:											     \
	sethi	%hi(CURLWP), %l4;		/* Use curlwp */			     \
	LDPTR	[%l4 + %lo(CURLWP)], %l5;						     \
	brz,pn	%l5, 0b; nop;			/* If curlwp is NULL need to use lwp0 */     \
2:											     \
	LDPTR	[%l5 + L_FPSTATE], %l6;		/* Save old fpstate */			     \
	STPTR	%l0, [%l5 + L_FPSTATE];		/* Insert new fpstate */		     \
	STPTR	%l5, [%l1 + %lo(FPLWP)];	/* Set new fplwp */			     \
	wr	%g0, FPRS_FEF, %fprs		/* Enable FPU */

/*
 * Weve saved our possible fpstate, now disable the fpu
 * and continue with life.
 */
#ifdef DEBUG
#define __CHECK_FPU				\
	LDPTR	[%l5 + L_FPSTATE], %l7;		\
	cmp	%l7, %l0;			\
	tnz	1;
#else
#define	__CHECK_FPU
#endif
	
#define RESTORE_FPU							     \
	__CHECK_FPU							     \
	STPTR	%l2, [%l1 + %lo(FPLWP)];	/* Restore old fproc */	     \
	wr	%g0, 0, %fprs;			/* Disable fpu */	     \
	brz,pt	%l3, 1f;			/* Skip if no fpstate */     \
	 STPTR	%l6, [%l5 + L_FPSTATE];		/* Restore old fpstate */    \
									     \
	mov	%l3, %o0;						     \
	call	_C_LABEL(loadfpstate);		/* Re-load orig fpstate */   \
1: \
	 membar	#Sync;				/* Finish all FP ops */

	

	.data
	.globl	_C_LABEL(data_start)
_C_LABEL(data_start):					! Start of data segment
#define DATA_START	_C_LABEL(data_start)

#if 1
/* XXX this shouldn't be needed... but kernel usually hangs without it */
/*
 * When a process exits and its u. area goes away, we set cpcb to point
 * to this `u.', leaving us with something to use for an interrupt stack,
 * and letting all the register save code have a pcb_uw to examine.
 * This is also carefully arranged (to come just before u0, so that
 * process 0's kernel stack can quietly overrun into it during bootup, if
 * we feel like doing that).
 */
	.globl	_C_LABEL(__idle_u)
_C_LABEL(__idle_u):
	.space	USPACE
#endif

#ifdef KGDB
/*
 * Another item that must be aligned, easiest to put it here.
 */
KGDB_STACK_SIZE = 2048
	.globl	_C_LABEL(kgdb_stack)
_C_LABEL(kgdb_stack):
	.space	KGDB_STACK_SIZE		! hope this is enough
#endif

#ifdef NOTDEF_DEBUG
/*
 * This stack is used when we detect kernel stack corruption.
 */
	.space	USPACE
	.align	16
panicstack:
#endif

/*
 * romp is the prom entry pointer
 * romtba is the prom trap table base address
 */
	.globl	romp
romp:	POINTER	0
	.globl	romtba
romtba:	POINTER	0

	_ALIGN
	.text

/*
 * The v9 trap frame is stored in the special trap registers.  The
 * register window is only modified on window overflow, underflow,
 * and clean window traps, where it points to the register window
 * needing service.  Traps have space for 8 instructions, except for
 * the window overflow, underflow, and clean window traps which are
 * 32 instructions long, large enough to in-line.
 *
 * The spitfire CPU (Ultra I) has 4 different sets of global registers.
 * (blah blah...)
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
 * chip, where many interrupts can be handled trivially with pseudo-DMA
 * or similar.  Only one `fast' interrupt can be used per level, however,
 * and direct and `fast' interrupts are incompatible.  Routines in intr.c
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
 *	that information.  Trap types in these macros are all dummys.
 */
	/* regular vectored traps */

#if KTR_COMPILE & KTR_TRAP
#if 0
#define TRACEWIN	wrpr %g0, PSTATE_KERN|PSTATE_IG, %pstate;\
			sethi %hi(9f), %g1; ba,pt %icc,ktr_trap_gen; or %g1, %lo(9f), %g1; 9:
#else
#define TRACEWIN
#endif
#define TRACEFLT 	sethi %hi(1f), %g1; ba,pt %icc,ktr_trap_gen;\
			or %g1, %lo(1f), %g1; 1:
#define	VTRAP(type, label) \
	sethi %hi(label), %g1; ba,pt %icc,ktr_trap_gen;\
	or %g1, %lo(label), %g1; NOTREACHED; TA8
#else	
#define TRACEWIN
#define TRACEFLT
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

#define	SYSCALL		VTRAP(0x100, syscall_setup)
#ifdef notyet
#define	ZS_INTERRUPT	ba,a,pt %icc, zshard; nop; TA8
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
	bnz,pt	%xcc, label64+4;	/* Is it a v9 or v8 stack? */ \
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

	/* fill either 32-bit or 64-bit register window. */
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
	_C_LABEL(kernel_text) = kernel_start		! for kvm_mkdb(8)
kernel_start:
	/* Traps from TL=0 -- traps from user mode */
#ifdef __STDC__
#define TABLE(name)	user_ ## name
#else
#define	TABLE(name)	user_/**/name
#endif
	.globl	_C_LABEL(trapbase)
_C_LABEL(trapbase):
	b dostart; nop; TA8	! 000 = reserved -- Use it to boot
	/* We should not get the next 5 traps */
	UTRAP(0x001)		! 001 = POR Reset -- ROM should get this
	UTRAP(0x002)		! 002 = WDR -- ROM should get this
	UTRAP(0x003)		! 003 = XIR -- ROM should get this
	UTRAP(0x004)		! 004 = SIR -- ROM should get this
	UTRAP(0x005)		! 005 = RED state exception
	UTRAP(0x006); UTRAP(0x007)
	VTRAP(T_INST_EXCEPT, textfault)	! 008 = instr. access exept
	VTRAP(T_TEXTFAULT, textfault)	! 009 = instr access MMU miss
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
	VTRAP(T_FP_IEEE_754, fp_exception)	! 021 = ieee 754 exception
	VTRAP(T_FP_OTHER, fp_exception)		! 022 = other fp exception
	TRAP(T_TAGOF)			! 023 = tag overflow
	TRACEWIN			! DEBUG -- 4 insns
	rdpr %cleanwin, %o7		! 024-027 = clean window trap
	inc %o7				!	This handler is in-lined and cannot fault
#ifdef DEBUG
	set	0xbadcafe, %l0		! DEBUG -- compiler should not rely on zero-ed registers.
#else
	clr	%l0
#endif
	wrpr %g0, %o7, %cleanwin	!       Nucleus (trap&IRQ) code does not need clean windows

	mov %l0,%l1; mov %l0,%l2	!	Clear out %l0-%l8 and %o0-%o8 and inc %cleanwin and done
	mov %l0,%l3; mov %l0,%l4
#if 0
#ifdef DIAGNOSTIC
	!!
	!! Check the sp redzone
	!!
	!! Since we can't spill the current window, we'll just keep
	!! track of the frame pointer.  Problems occur when the routine
	!! allocates and uses stack storage.
	!!
!	rdpr	%wstate, %l5	! User stack?
!	cmp	%l5, WSTATE_KERN
!	bne,pt	%icc, 7f
	 sethi	%hi(CPCB), %l5
	LDPTR	[%l5 + %lo(CPCB)], %l5	! If pcb < fp < pcb+sizeof(pcb)
	inc	PCB_SIZE, %l5		! then we have a stack overflow
	btst	%fp, 1			! 64-bit stack?
	sub	%fp, %l5, %l7
	bnz,a,pt	%icc, 1f
	 inc	BIAS, %l7		! Remove BIAS
1:
	cmp	%l7, PCB_SIZE
	blu	%xcc, cleanwin_overflow
#endif
#endif
	mov %l0, %l5
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
	HARDINT4U(15)			! 04f = nonmaskable interrupt
	UTRAP(0x050); UTRAP(0x051); UTRAP(0x052); UTRAP(0x053); UTRAP(0x054); UTRAP(0x055)
	UTRAP(0x056); UTRAP(0x057); UTRAP(0x058); UTRAP(0x059); UTRAP(0x05a); UTRAP(0x05b)
	UTRAP(0x05c); UTRAP(0x05d); UTRAP(0x05e); UTRAP(0x05f)
	VTRAP(0x060, interrupt_vector); ! 060 = interrupt vector
	TRAP(T_PA_WATCHPT)		! 061 = physical address data watchpoint
	TRAP(T_VA_WATCHPT)		! 062 = virtual address data watchpoint
	UTRAP(T_ECCERR)			! We'll implement this one later
ufast_IMMU_miss:			! 064 = fast instr access MMU miss
	TRACEFLT			! DEBUG
	ldxa	[%g0] ASI_IMMU_8KPTR, %g2 ! Load IMMU 8K TSB pointer
#ifdef NO_TSB
	ba,a	%icc, instr_miss
#endif
	ldxa	[%g0] ASI_IMMU, %g1	! Load IMMU tag target register
	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g4	! Load TSB tag:data into %g4:%g5
	brgez,pn %g5, instr_miss	! Entry invalid?  Punt
	 cmp	%g1, %g4		! Compare TLB tags
	bne,pn %xcc, instr_miss		! Got right tag?
	 nop
	CLRTT
	stxa	%g5, [%g0] ASI_IMMU_DATA_IN ! Enter new mapping
	retry				! Try new mapping
1:
	sir
	TA32
ufast_DMMU_miss:			! 068 = fast data access MMU miss
	TRACEFLT			! DEBUG
	ldxa	[%g0] ASI_DMMU_8KPTR, %g2! Load DMMU 8K TSB pointer

#ifdef NO_TSB
	ba,a	%icc, data_miss
#endif
	ldxa	[%g0] ASI_DMMU, %g1	! Load DMMU tag target register
	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g4	! Load TSB tag and data into %g4 and %g5
	brgez,pn %g5, data_miss		! Entry invalid?  Punt
	 cmp	%g1, %g4		! Compare TLB tags
	bnz,pn	%xcc, data_miss		! Got right tag?
	 nop
	CLRTT
#ifdef TRAPSTATS
	sethi	%hi(_C_LABEL(udhit)), %g1
	lduw	[%g1+%lo(_C_LABEL(udhit))], %g2
	inc	%g2
	stw	%g2, [%g1+%lo(_C_LABEL(udhit))]
#endif
	stxa	%g5, [%g0] ASI_DMMU_DATA_IN ! Enter new mapping
	retry				! Try new mapping
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
TABLE(uspill):
	SPILL64(uspill8,ASI_AIUS)	! 0x080 spill_0_normal -- used to save user windows in user mode
	SPILL32(uspill4,ASI_AIUS)	! 0x084 spill_1_normal
	SPILLBOTH(uspill8,uspill4,ASI_AIUS)	 ! 0x088 spill_2_normal
	UTRAP(0x08c); TA32		! 0x08c spill_3_normal
TABLE(kspill):
	SPILL64(kspill8,ASI_N)		! 0x090 spill_4_normal -- used to save supervisor windows
	SPILL32(kspill4,ASI_N)		! 0x094 spill_5_normal
	SPILLBOTH(kspill8,kspill4,ASI_N) ! 0x098 spill_6_normal
	UTRAP(0x09c); TA32		! 0x09c spill_7_normal
TABLE(uspillk):
	SPILL64(uspillk8,ASI_AIUS)	! 0x0a0 spill_0_other -- used to save user windows in supervisor mode
	SPILL32(uspillk4,ASI_AIUS)	! 0x0a4 spill_1_other
	SPILLBOTH(uspillk8,uspillk4,ASI_AIUS) ! 0x0a8 spill_2_other
	UTRAP(0x0ac); TA32		! 0x0ac spill_3_other
	UTRAP(0x0b0); TA32		! 0x0b0 spill_4_other
	UTRAP(0x0b4); TA32		! 0x0b4 spill_5_other
	UTRAP(0x0b8); TA32		! 0x0b8 spill_6_other
	UTRAP(0x0bc); TA32		! 0x0bc spill_7_other
TABLE(ufill):
	FILL64(ufill8,ASI_AIUS)		! 0x0c0 fill_0_normal -- used to fill windows when running user mode
	FILL32(ufill4,ASI_AIUS)		! 0x0c4 fill_1_normal
	FILLBOTH(ufill8,ufill4,ASI_AIUS) ! 0x0c8 fill_2_normal
	UTRAP(0x0cc); TA32		! 0x0cc fill_3_normal
TABLE(kfill):
	FILL64(kfill8,ASI_N)		! 0x0d0 fill_4_normal -- used to fill windows when running supervisor mode
	FILL32(kfill4,ASI_N)		! 0x0d4 fill_5_normal
	FILLBOTH(kfill8,kfill4,ASI_N)	! 0x0d8 fill_6_normal
	UTRAP(0x0dc); TA32		! 0x0dc fill_7_normal
TABLE(ufillk):
	FILL64(ufillk8,ASI_AIUS)	! 0x0e0 fill_0_other
	FILL32(ufillk4,ASI_AIUS)	! 0x0e4 fill_1_other
	FILLBOTH(ufillk8,ufillk4,ASI_AIUS) ! 0x0e8 fill_2_other
	UTRAP(0x0ec); TA32		! 0x0ec fill_3_other
	UTRAP(0x0f0); TA32		! 0x0f0 fill_4_other
	UTRAP(0x0f4); TA32		! 0x0f4 fill_5_other
	UTRAP(0x0f8); TA32		! 0x0f8 fill_6_other
	UTRAP(0x0fc); TA32		! 0x0fc fill_7_other
TABLE(syscall):
	SYSCALL				! 0x100 = sun syscall
	BPT				! 0x101 = pseudo breakpoint instruction
	STRAP(0x102); STRAP(0x103); STRAP(0x104); STRAP(0x105); STRAP(0x106); STRAP(0x107)
	SYSCALL				! 0x108 = svr4 syscall
	SYSCALL				! 0x109 = bsd syscall
	BPT_KGDB_EXEC			! 0x10a = enter kernel gdb on kernel startup
	STRAP(0x10b); STRAP(0x10c); STRAP(0x10d); STRAP(0x10e); STRAP(0x10f);
	STRAP(0x110); STRAP(0x111); STRAP(0x112); STRAP(0x113); STRAP(0x114); STRAP(0x115); STRAP(0x116); STRAP(0x117)
	STRAP(0x118); STRAP(0x119); STRAP(0x11a); STRAP(0x11b); STRAP(0x11c); STRAP(0x11d); STRAP(0x11e); STRAP(0x11f)
	STRAP(0x120); STRAP(0x121); STRAP(0x122); STRAP(0x123); STRAP(0x124); STRAP(0x125); STRAP(0x126); STRAP(0x127)
	STRAP(0x128); STRAP(0x129); STRAP(0x12a); STRAP(0x12b); STRAP(0x12c); STRAP(0x12d); STRAP(0x12e); STRAP(0x12f)
	STRAP(0x130); STRAP(0x131); STRAP(0x132); STRAP(0x133); STRAP(0x134); STRAP(0x135); STRAP(0x136); STRAP(0x137)
	STRAP(0x138); STRAP(0x139); STRAP(0x13a); STRAP(0x13b); STRAP(0x13c); STRAP(0x13d); STRAP(0x13e); STRAP(0x13f)
	SYSCALL				! 0x140 SVID syscall (Solaris 2.7)
	SYSCALL				! 0x141 SPARC International syscall
	SYSCALL				! 0x142	OS Vendor syscall
	SYSCALL				! 0x143 HW OEM syscall
	STRAP(0x144); STRAP(0x145); STRAP(0x146); STRAP(0x147)
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
#ifdef __STDC__
#define	TABLE(name)	nucleus_ ## name
#else
#define	TABLE(name)	nucleus_/**/name
#endif
trapbase_priv:
	UTRAP(0x000)			! 000 = reserved -- Use it to boot
	/* We should not get the next 5 traps */
	UTRAP(0x001)			! 001 = POR Reset -- ROM should get this
	UTRAP(0x002)			! 002 = WDR Watchdog -- ROM should get this
	UTRAP(0x003)			! 003 = XIR -- ROM should get this
	UTRAP(0x004)			! 004 = SIR -- ROM should get this
	UTRAP(0x005)			! 005 = RED state exception
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
	VTRAP(T_FP_IEEE_754, fp_exception) ! 021 = ieee 754 exception
	VTRAP(T_FP_OTHER, fp_exception)	! 022 = other fp exception
	TRAP(T_TAGOF)			! 023 = tag overflow
	TRACEWIN			! DEBUG
	clr	%l0
#ifdef DEBUG
	set	0xbadbeef, %l0		! DEBUG
#endif
	mov %l0, %l1; mov %l0, %l2	! 024-027 = clean window trap
	rdpr %cleanwin, %o7		!	This handler is in-lined and cannot fault
	inc %o7; mov %l0, %l3		!       Nucleus (trap&IRQ) code does not need clean windows
	wrpr %g0, %o7, %cleanwin	!	Clear out %l0-%l8 and %o0-%o8 and inc %cleanwin and done
#ifdef NOT_DEBUG
	!!
	!! Check the sp redzone
	!!
	rdpr	%wstate, t1
	cmp	t1, WSTATE_KERN
	bne,pt	icc, 7f
	 sethi	%hi(_C_LABEL(redzone)), t1
	ldx	[t1 + %lo(_C_LABEL(redzone))], t2
	cmp	%sp, t2			! if sp >= t2, not in red zone
	blu	panic_red		! and can continue normally
7:
#endif
	mov %l0, %l4; mov %l0, %l5; mov %l0, %l6; mov %l0, %l7
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
	VTRAP(T_ALIGN, checkalign)	! 034 = address alignment error -- we could fix it inline...
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
	HARDINT4U(15)			! 04f = nonmaskable interrupt
	UTRAP(0x050); UTRAP(0x051); UTRAP(0x052); UTRAP(0x053); UTRAP(0x054); UTRAP(0x055)
	UTRAP(0x056); UTRAP(0x057); UTRAP(0x058); UTRAP(0x059); UTRAP(0x05a); UTRAP(0x05b)
	UTRAP(0x05c); UTRAP(0x05d); UTRAP(0x05e); UTRAP(0x05f)
	VTRAP(0x060, interrupt_vector); ! 060 = interrupt vector
	TRAP(T_PA_WATCHPT)		! 061 = physical address data watchpoint
	TRAP(T_VA_WATCHPT)		! 062 = virtual address data watchpoint
	UTRAP(T_ECCERR)			! We'll implement this one later
kfast_IMMU_miss:			! 064 = fast instr access MMU miss
	TRACEFLT			! DEBUG
	ldxa	[%g0] ASI_IMMU_8KPTR, %g2 ! Load IMMU 8K TSB pointer
#ifdef NO_TSB
	ba,a	%icc, instr_miss
#endif
	ldxa	[%g0] ASI_IMMU, %g1	! Load IMMU tag target register
	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g4	! Load TSB tag:data into %g4:%g5
	brgez,pn %g5, instr_miss	! Entry invalid?  Punt
	 cmp	%g1, %g4		! Compare TLB tags
	bne,pn %xcc, instr_miss		! Got right tag?
	 nop
	CLRTT
	stxa	%g5, [%g0] ASI_IMMU_DATA_IN ! Enter new mapping
	retry				! Try new mapping
1:
	sir
	TA32
kfast_DMMU_miss:			! 068 = fast data access MMU miss
	TRACEFLT			! DEBUG
	ldxa	[%g0] ASI_DMMU_8KPTR, %g2! Load DMMU 8K TSB pointer
#ifdef NO_TSB
	ba,a	%icc, data_miss
#endif
	ldxa	[%g0] ASI_DMMU, %g1	! Load DMMU tag target register
	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g4	! Load TSB tag and data into %g4 and %g5
	brgez,pn %g5, data_miss		! Entry invalid?  Punt
	 cmp	%g1, %g4		! Compare TLB tags
	bnz,pn	%xcc, data_miss		! Got right tag?
	 nop
	CLRTT
#ifdef TRAPSTATS
	sethi	%hi(_C_LABEL(kdhit)), %g1
	lduw	[%g1+%lo(_C_LABEL(kdhit))], %g2
	inc	%g2
	stw	%g2, [%g1+%lo(_C_LABEL(kdhit))]
#endif
	stxa	%g5, [%g0] ASI_DMMU_DATA_IN ! Enter new mapping
	retry				! Try new mapping
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
TABLE(uspill):
	SPILL64(1,ASI_AIUS)		! 0x080 spill_0_normal -- used to save user windows
	SPILL32(2,ASI_AIUS)		! 0x084 spill_1_normal
	SPILLBOTH(1b,2b,ASI_AIUS)	! 0x088 spill_2_normal
	UTRAP(0x08c); TA32		! 0x08c spill_3_normal
TABLE(kspill):
	SPILL64(1,ASI_N)		! 0x090 spill_4_normal -- used to save supervisor windows
	SPILL32(2,ASI_N)		! 0x094 spill_5_normal
	SPILLBOTH(1b,2b,ASI_N)		! 0x098 spill_6_normal
	UTRAP(0x09c); TA32		! 0x09c spill_7_normal
TABLE(uspillk):
	SPILL64(1,ASI_AIUS)		! 0x0a0 spill_0_other -- used to save user windows in nucleus mode
	SPILL32(2,ASI_AIUS)		! 0x0a4 spill_1_other
	SPILLBOTH(1b,2b,ASI_AIUS)	! 0x0a8 spill_2_other
	UTRAP(0x0ac); TA32		! 0x0ac spill_3_other
	UTRAP(0x0b0); TA32		! 0x0b0 spill_4_other
	UTRAP(0x0b4); TA32		! 0x0b4 spill_5_other
	UTRAP(0x0b8); TA32		! 0x0b8 spill_6_other
	UTRAP(0x0bc); TA32		! 0x0bc spill_7_other
TABLE(ufill):
	FILL64(nufill8,ASI_AIUS)	! 0x0c0 fill_0_normal -- used to fill windows when running nucleus mode from user
	FILL32(nufill4,ASI_AIUS)	! 0x0c4 fill_1_normal
	FILLBOTH(nufill8,nufill4,ASI_AIUS) ! 0x0c8 fill_2_normal
	UTRAP(0x0cc); TA32		! 0x0cc fill_3_normal
TABLE(sfill):
	FILL64(sfill8,ASI_N)		! 0x0d0 fill_4_normal -- used to fill windows when running nucleus mode from supervisor
	FILL32(sfill4,ASI_N)		! 0x0d4 fill_5_normal
	FILLBOTH(sfill8,sfill4,ASI_N)	! 0x0d8 fill_6_normal
	UTRAP(0x0dc); TA32		! 0x0dc fill_7_normal
TABLE(kfill):
	FILL64(nkfill8,ASI_AIUS)	! 0x0e0 fill_0_other -- used to fill user windows when running nucleus mode -- will we ever use this?
	FILL32(nkfill4,ASI_AIUS)	! 0x0e4 fill_1_other
	FILLBOTH(nkfill8,nkfill4,ASI_AIUS)! 0x0e8 fill_2_other
	UTRAP(0x0ec); TA32		! 0x0ec fill_3_other
	UTRAP(0x0f0); TA32		! 0x0f0 fill_4_other
	UTRAP(0x0f4); TA32		! 0x0f4 fill_5_other
	UTRAP(0x0f8); TA32		! 0x0f8 fill_6_other
	UTRAP(0x0fc); TA32		! 0x0fc fill_7_other
TABLE(syscall):
	SYSCALL				! 0x100 = sun syscall
	BPT				! 0x101 = pseudo breakpoint instruction
	STRAP(0x102); STRAP(0x103); STRAP(0x104); STRAP(0x105); STRAP(0x106); STRAP(0x107)
	SYSCALL				! 0x108 = svr4 syscall
	SYSCALL				! 0x109 = bsd syscall
	BPT_KGDB_EXEC			! 0x10a = enter kernel gdb on kernel startup
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

/*
 * If the cleanwin trap handler detects an overfow we come here.
 * We need to fix up the window registers, switch to the interrupt
 * stack, and then trap to the debugger.
 */
cleanwin_overflow:
	!! We've already incremented %cleanwin
	!! So restore %cwp
	rdpr	%cwp, %l0
	dec	%l0
	wrpr	%l0, %g0, %cwp
	set	EINTSTACK-STKB-CC64FSZ, %l0
	save	%l0, 0, %sp

	ta	1		! Enter debugger
	sethi	%hi(1f), %o0
	call	_C_LABEL(panic)
	 or	%o0, %lo(1f), %o0
	restore
	retry
	.data
1:
	.asciz	"Kernel stack overflow!"
	_ALIGN
	.text

#ifdef DEBUG
#define CHKREG(r) \
	ldx	[%o0 + 8*1], %o1; \
	cmp	r, %o1; \
	stx	%o0, [%o0]; \
	tne	1
	.data
globreg_debug:
	.xword	-1, 0, 0, 0, 0, 0, 0, 0
	.text
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
	 * Checkpoint:	 store a byte value at DATA_START+0x21
	 *		uses two temp regs
	 */
#define CHKPT(r1,r2,val) \
	sethi	%hi(DATA_START), r1; \
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
	.data
pmap_dumpflag:
	.xword	0		! semaphore
	.globl	pmap_dumparea	! Get this into the kernel syms
pmap_dumparea:
	.space	(32*8)		! room to save 32 registers
pmap_edumparea:
	.text
pmap_screwup:
	rd	%pc, %g3
	sub	%g3, (pmap_edumparea-pmap_dumparea), %g3! pc relative addressing 8^)
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

#ifdef NOTDEF_DEBUG
/*
 * A hardware red zone is impossible.  We simulate one in software by
 * keeping a `red zone' pointer; if %sp becomes less than this, we panic.
 * This is expensive and is only enabled when debugging.
 */
#define	REDSIZE	(USIZ)		/* Mark used portion of user structure out of bounds */
#define	REDSTACK 2048		/* size of `panic: stack overflow' region */
	.data
	_ALIGN
redzone:
	.xword	_C_LABEL(idle_u) + REDSIZE
redstack:
	.space	REDSTACK
eredstack:
Lpanic_red:
	.asciz	"kernel stack overflow"
	_ALIGN
	.text

	/* set stack pointer redzone to base+minstack; alters base */
#define	SET_SP_REDZONE(base, tmp) \
	add	base, REDSIZE, base; \
	sethi	%hi(_C_LABEL(redzone)), tmp; \
	stx	base, [tmp + %lo(_C_LABEL(redzone))]

	/* variant with a constant */
#define	SET_SP_REDZONE_CONST(const, tmp1, tmp2) \
	set	(const) + REDSIZE, tmp1; \
	sethi	%hi(_C_LABEL(redzone)), tmp2; \
	stx	tmp1, [tmp2 + %lo(_C_LABEL(redzone))]

	/* check stack pointer against redzone (uses two temps) */
#define	CHECK_SP_REDZONE(t1, t2) \
	sethi	KERNBASE, t1;	\
	cmp	%sp, t1;	\
	blu,pt	%xcc, 7f;	\
	 sethi	%hi(_C_LABEL(redzone)), t1; \
	ldx	[t1 + %lo(_C_LABEL(redzone))], t2; \
	cmp	%sp, t2;	/* if sp >= t2, not in red zone */ \
	blu	panic_red; nop;	/* and can continue normally */ \
7:

panic_red:
	/* move to panic stack */
	stx	%g0, [t1 + %lo(_C_LABEL(redzone))];
	set	eredstack - BIAS, %sp;
	/* prevent panic() from lowering ipl */
	sethi	%hi(_C_LABEL(panicstr)), t2;
	set	Lpanic_red, t2;
	st	t2, [t1 + %lo(_C_LABEL(panicstr))];
	wrpr	g0, 15, %pil		/* t1 = splhigh() */
	save	%sp, -CCF64SZ, %sp;	/* preserve current window */
	sethi	%hi(Lpanic_red), %o0;
	call	_C_LABEL(panic);
	 or %o0, %lo(Lpanic_red), %o0;


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
	.data
_C_LABEL(trap_trace_dis):
	.word	1, 1		! Starts disabled.  DDB turns it on.
_C_LABEL(trap_trace_ptr):
	.word	0, 0, 0, 0
_C_LABEL(trap_trace):
	.space	TRACESIZ
_C_LABEL(trap_trace_end):
	.space	0x20		! safety margin

#if KTR_COMPILE
	.text
ktr_trap_gen:
	CATR(KTR_TRAP, "TRAP: tl=%d tt=%p tstate=%p tpc=%p sp=%p",
		 %g2, %g3, %g4, 10, 11, 12)
	rdpr	%tl, %g3
	stx	%g3, [%g2 + KTR_PARM1]
	rdpr	%tt, %g3
	stx	%g3, [%g2 + KTR_PARM2]
	rdpr	%tstate, %g3
	stx	%g3, [%g2 + KTR_PARM3]
	rdpr	%tpc, %g3
	stx	%g3, [%g2 + KTR_PARM4]
	stx	%sp, [%g2 + KTR_PARM5]
12:
	jmp	%g1			! return to processing the trap
	 nop
#endif


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
 * XXX it would be good to save the interrupt stack frame to the kernel
 * stack so we wouldn't have to copy it later if we needed to handle a AST.
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
  * can potentially be lost.  This trap can be caused when allocating
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
	.data
trap_setup_msg:
	.asciz	"TRAP_SETUP: tt=%x osp=%x nsp=%x tl=%x tpc=%x\n"
	_ALIGN
intr_setup_msg:
	.asciz	"INTR_SETUP: tt=%x osp=%x nsp=%x tl=%x tpc=%x\n"
	_ALIGN
	.text

#ifdef DEBUG
	/* Only save a snapshot of locals and ins in DEBUG kernels */
#define	SAVE_LOCALS_INS	\
	/* Save local registers to trap frame */ \
	stx	%l0, [%g6 + CC64FSZ + STKB + TF_L + (0*8)]; \
	stx	%l1, [%g6 + CC64FSZ + STKB + TF_L + (1*8)]; \
	stx	%l2, [%g6 + CC64FSZ + STKB + TF_L + (2*8)]; \
	stx	%l3, [%g6 + CC64FSZ + STKB + TF_L + (3*8)]; \
	stx	%l4, [%g6 + CC64FSZ + STKB + TF_L + (4*8)]; \
	stx	%l5, [%g6 + CC64FSZ + STKB + TF_L + (5*8)]; \
	stx	%l6, [%g6 + CC64FSZ + STKB + TF_L + (6*8)]; \
	stx	%l7, [%g6 + CC64FSZ + STKB + TF_L + (7*8)]; \
\
	/* Save in registers to trap frame */ \
	stx	%i0, [%g6 + CC64FSZ + STKB + TF_I + (0*8)]; \
	stx	%i1, [%g6 + CC64FSZ + STKB + TF_I + (1*8)]; \
	stx	%i2, [%g6 + CC64FSZ + STKB + TF_I + (2*8)]; \
	stx	%i3, [%g6 + CC64FSZ + STKB + TF_I + (3*8)]; \
	stx	%i4, [%g6 + CC64FSZ + STKB + TF_I + (4*8)]; \
	stx	%i5, [%g6 + CC64FSZ + STKB + TF_I + (5*8)]; \
	stx	%i6, [%g6 + CC64FSZ + STKB + TF_I + (6*8)]; \
	stx	%i7, [%g6 + CC64FSZ + STKB + TF_I + (7*8)]; \
\
	stx	%g1, [%g6 + CC64FSZ + STKB + TF_FAULT];
#else
#define	SAVE_LOCALS_INS
#endif

#ifdef _LP64
#define	FIXUP_TRAP_STACK \
	btst	1, %g6;						/* Fixup 64-bit stack if necessary */ \
	bnz,pt	%icc, 1f; \
	 add	%g6, %g5, %g6;					/* Allocate a stack frame */ \
	inc	-BIAS, %g6; \
1:
#else
#define	FIXUP_TRAP_STACK \
	srl	%g6, 0, %g6;					/* truncate at 32-bits */ \
	btst	1, %g6;						/* Fixup 64-bit stack if necessary */ \
	add	%g6, %g5, %g6;					/* Allocate a stack frame */ \
	add	%g6, BIAS, %g5; \
	movne	%icc, %g5, %g6;
#endif

#ifdef _LP64
#define	TRAP_SETUP(stackspace) \
	sethi	%hi(CPCB), %g6; \
	sethi	%hi((stackspace)), %g5; \
	LDPTR	[%g6 + %lo(CPCB)], %g6; \
	sethi	%hi(USPACE), %g7;				/* Always multiple of page size */ \
	or	%g5, %lo((stackspace)), %g5; \
	add	%g6, %g7, %g6; \
	rdpr	%wstate, %g7;					/* Find if we're from user mode */ \
	sra	%g5, 0, %g5;					/* Sign extend the damn thing */ \
	\
	sub	%g7, WSTATE_KERN, %g7;				/* Compare & leave in register */ \
	movrz	%g7, %sp, %g6;					/* Select old (kernel) stack or base of kernel stack */ \
	FIXUP_TRAP_STACK \
	SAVE_LOCALS_INS	\
	save	%g6, 0, %sp;					/* If we fault we should come right back here */ \
	stx	%i0, [%sp + CC64FSZ + BIAS + TF_O + (0*8)];		/* Save out registers to trap frame */ \
	stx	%i1, [%sp + CC64FSZ + BIAS + TF_O + (1*8)]; \
	stx	%i2, [%sp + CC64FSZ + BIAS + TF_O + (2*8)]; \
	stx	%i3, [%sp + CC64FSZ + BIAS + TF_O + (3*8)]; \
	stx	%i4, [%sp + CC64FSZ + BIAS + TF_O + (4*8)]; \
	stx	%i5, [%sp + CC64FSZ + BIAS + TF_O + (5*8)]; \
\
	stx	%i6, [%sp + CC64FSZ + BIAS + TF_O + (6*8)]; \
	brz,pt	%g7, 1f;					/* If we were in kernel mode start saving globals */ \
	 stx	%i7, [%sp + CC64FSZ + BIAS + TF_O + (7*8)]; \
	mov	CTX_PRIMARY, %g7; \
	/* came from user mode -- switch to kernel mode stack */ \
	rdpr	%canrestore, %g5;				/* Fixup register window state registers */ \
	wrpr	%g0, 0, %canrestore; \
	wrpr	%g0, %g5, %otherwin; \
	wrpr	%g0, WSTATE_KERN, %wstate;			/* Enable kernel mode window traps -- now we can trap again */ \
\
	stxa	%g0, [%g7] ASI_DMMU; 				/* Switch MMU to kernel primary context */ \
	sethi	%hi(KERNBASE), %g5; \
	flush	%g5;						/* Some convenient address that won't trap */ \
1:
	
/*
 * Interrupt setup is almost exactly like trap setup, but we need to
 * go to the interrupt stack if (a) we came from user mode or (b) we
 * came from kernel mode on the kernel stack.
 *
 * We don't guarantee any registers are preserved during this operation.
 * So we can be more efficient.
 */
#define	INTR_SETUP(stackspace) \
	rdpr	%wstate, %g7;					/* Find if we're from user mode */ \
	\
	sethi	%hi(EINTSTACK-BIAS), %g6; \
	sethi	%hi(EINTSTACK-INTSTACK), %g4; \
	\
	or	%g6, %lo(EINTSTACK-BIAS), %g6;			/* Base of interrupt stack */ \
	dec	%g4;						/* Make it into a mask */ \
	\
	sub	%g6, %sp, %g1;					/* Offset from interrupt stack */ \
	sethi	%hi((stackspace)), %g5; \
	\
	or	%g5, %lo((stackspace)), %g5; \
\
	andn	%g1, %g4, %g4;					/* Are we out of the interrupt stack range? */ \
	xor	%g7, WSTATE_KERN, %g3;				/* Are we on the user stack ? */ \
	\
	sra	%g5, 0, %g5;					/* Sign extend the damn thing */ \
	or	%g3, %g4, %g4;					/* Definitely not off the interrupt stack */ \
	\
	movrz	%g4, %sp, %g6; \
	\
	add	%g6, %g5, %g5;					/* Allocate a stack frame */ \
	btst	1, %g6; \
	bnz,pt	%icc, 1f; \
\
	 mov	%g5, %g6; \
	\
	add	%g5, -BIAS, %g6; \
	\
1:	SAVE_LOCALS_INS	\
	save	%g6, 0, %sp;					/* If we fault we should come right back here */ \
	stx	%i0, [%sp + CC64FSZ + BIAS + TF_O + (0*8)];		/* Save out registers to trap frame */ \
	stx	%i1, [%sp + CC64FSZ + BIAS + TF_O + (1*8)]; \
	stx	%i2, [%sp + CC64FSZ + BIAS + TF_O + (2*8)]; \
	stx	%i3, [%sp + CC64FSZ + BIAS + TF_O + (3*8)]; \
	stx	%i4, [%sp + CC64FSZ + BIAS + TF_O + (4*8)]; \
\
	stx	%i5, [%sp + CC64FSZ + BIAS + TF_O + (5*8)]; \
	stx	%i6, [%sp + CC64FSZ + BIAS + TF_O + (6*8)]; \
	stx	%i6, [%sp + CC64FSZ + BIAS + TF_G + (0*8)];		/* Save fp in clockframe->cf_fp */ \
	brz,pt	%g3, 1f;					/* If we were in kernel mode start saving globals */ \
	 stx	%i7, [%sp + CC64FSZ + BIAS + TF_O + (7*8)]; \
	/* came from user mode -- switch to kernel mode stack */ \
	 rdpr	%otherwin, %g5;					/* Has this already been done? */ \
	\
	brnz,pn	%g5, 1f;					/* Don't set this twice */ \
	\
	 rdpr	%canrestore, %g5;				/* Fixup register window state registers */ \
\
	wrpr	%g0, 0, %canrestore; \
	\
	wrpr	%g0, %g5, %otherwin; \
	\
	sethi	%hi(KERNBASE), %g5; \
	mov	CTX_PRIMARY, %g7; \
	\
	wrpr	%g0, WSTATE_KERN, %wstate;			/* Enable kernel mode window traps -- now we can trap again */ \
	\
	stxa	%g0, [%g7] ASI_DMMU; 				/* Switch MMU to kernel primary context */ \
	\
	flush	%g5;						/* Some convenient address that won't trap */ \
1:
	
#else /* _LP64 */

#define	TRAP_SETUP(stackspace) \
	sethi	%hi(CPCB), %g6; \
	sethi	%hi((stackspace)), %g5; \
	LDPTR	[%g6 + %lo(CPCB)], %g6; \
	sethi	%hi(USPACE), %g7; \
	or	%g5, %lo((stackspace)), %g5; \
	add	%g6, %g7, %g6; \
	rdpr	%wstate, %g7;					/* Find if we're from user mode */ \
	\
	sra	%g5, 0, %g5;					/* Sign extend the damn thing */ \
	subcc	%g7, WSTATE_KERN, %g7;				/* Compare & leave in register */ \
	movz	%icc, %sp, %g6;					/* Select old (kernel) stack or base of kernel stack */ \
	FIXUP_TRAP_STACK \
	SAVE_LOCALS_INS \
	save	%g6, 0, %sp;					/* If we fault we should come right back here */ \
	stx	%i0, [%sp + CC64FSZ + STKB + TF_O + (0*8)];		/* Save out registers to trap frame */ \
	stx	%i1, [%sp + CC64FSZ + STKB + TF_O + (1*8)]; \
	stx	%i2, [%sp + CC64FSZ + STKB + TF_O + (2*8)]; \
	stx	%i3, [%sp + CC64FSZ + STKB + TF_O + (3*8)]; \
	stx	%i4, [%sp + CC64FSZ + STKB + TF_O + (4*8)]; \
	stx	%i5, [%sp + CC64FSZ + STKB + TF_O + (5*8)]; \
	\
	stx	%i6, [%sp + CC64FSZ + STKB + TF_O + (6*8)]; \
	brz,pn	%g7, 1f;					/* If we were in kernel mode start saving globals */ \
	 stx	%i7, [%sp + CC64FSZ + STKB + TF_O + (7*8)]; \
	mov	CTX_PRIMARY, %g7; \
	/* came from user mode -- switch to kernel mode stack */ \
	rdpr	%canrestore, %g5;				/* Fixup register window state registers */ \
	wrpr	%g0, 0, %canrestore; \
	wrpr	%g0, %g5, %otherwin; \
	wrpr	%g0, WSTATE_KERN, %wstate;			/* Enable kernel mode window traps -- now we can trap again */ \
	\
	stxa	%g0, [%g7] ASI_DMMU; 				/* Switch MMU to kernel primary context */ \
	sethi	%hi(KERNBASE), %g5; \
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
	sethi	%hi(EINTSTACK), %g1; \
	sethi	%hi((stackspace)), %g5; \
	btst	1, %sp; \
	add	%sp, BIAS, %g6; \
	movz	%icc, %sp, %g6; \
	or	%g1, %lo(EINTSTACK), %g1; \
	srl	%g6, 0, %g6;					/* truncate at 32-bits */ \
	set	(EINTSTACK-INTSTACK), %g7; \
	or	%g5, %lo((stackspace)), %g5; \
	sub	%g1, %g6, %g2;					/* Determine if we need to switch to intr stack or not */ \
	dec	%g7;						/* Make it into a mask */ \
	andncc	%g2, %g7, %g0;					/* XXXXXXXXXX This assumes kernel addresses are unique from user addresses */ \
	rdpr	%wstate, %g7;					/* Find if we're from user mode */ \
	sra	%g5, 0, %g5;					/* Sign extend the damn thing */ \
	movnz	%xcc, %g1, %g6;					/* Stay on interrupt stack? */ \
	cmp	%g7, WSTATE_KERN;				/* User or kernel sp? */ \
	movnz	%icc, %g1, %g6;					/* Stay on interrupt stack? */ \
	add	%g6, %g5, %g6;					/* Allocate a stack frame */ \
	\
	SAVE_LOCALS_INS \
	save	%g6, 0, %sp;					/* If we fault we should come right back here */ \
	stx	%i0, [%sp + CC64FSZ + STKB + TF_O + (0*8)];		/* Save out registers to trap frame */ \
	stx	%i1, [%sp + CC64FSZ + STKB + TF_O + (1*8)]; \
	stx	%i2, [%sp + CC64FSZ + STKB + TF_O + (2*8)]; \
	stx	%i3, [%sp + CC64FSZ + STKB + TF_O + (3*8)]; \
	stx	%i4, [%sp + CC64FSZ + STKB + TF_O + (4*8)]; \
	stx	%i5, [%sp + CC64FSZ + STKB + TF_O + (5*8)]; \
	stx	%i6, [%sp + CC64FSZ + STKB + TF_O + (6*8)]; \
	stx	%i6, [%sp + CC64FSZ + STKB + TF_G + (0*8)];		/* Save fp in clockframe->cf_fp */ \
	rdpr	%wstate, %g7;					/* Find if we're from user mode */ \
	stx	%i7, [%sp + CC64FSZ + STKB + TF_O + (7*8)]; \
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
	flush	%g5;						/* Some convenient address that won't trap */ \
1:
#endif /* _LP64 */

#ifdef DEBUG

	/* Look up kpte to test algorithm */
	.globl	asmptechk
asmptechk:
	mov	%o0, %g4	! pmap->pm_segs
	mov	%o1, %g3	! Addr to lookup -- mind the context

	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	brz,pt	%g5, 0f					! Should be zero or -1
	 inc	%g5					! Make -1 -> 0
	brnz,pn	%g5, 1f					! Error!
0:
	 srlx	%g3, STSHIFT, %g5
	and	%g5, STMASK, %g5
	sll	%g5, 3, %g5
	add	%g4, %g5, %g4
	DLFLUSH(%g4,%g5)
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Remember -- UNSIGNED
	DLFLUSH2(%g5)
	brz,pn	%g4, 1f					! NULL entry? check somewhere else

	 srlx	%g3, PDSHIFT, %g5
	and	%g5, PDMASK, %g5
	sll	%g5, 3, %g5
	add	%g4, %g5, %g4
	DLFLUSH(%g4,%g5)
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Remember -- UNSIGNED
	DLFLUSH2(%g5)
	brz,pn	%g4, 1f					! NULL entry? check somewhere else

	 srlx	%g3, PTSHIFT, %g5			! Convert to ptab offset
	and	%g5, PTMASK, %g5
	sll	%g5, 3, %g5
	add	%g4, %g5, %g4
	DLFLUSH(%g4,%g5)
	ldxa	[%g4] ASI_PHYS_CACHED, %g6
	DLFLUSH2(%g5)
	brgez,pn %g6, 1f				! Entry invalid?  Punt
	 srlx	%g6, 32, %o0
	retl
	 srl	%g6, 0, %o1
1:
	mov	%g0, %o1
	retl
	 mov	%g0, %o0

	.data
2:
	.asciz	"asmptechk: %x %x %x %x:%x\r\n"
	_ALIGN
	.text
#endif

/*
 * This is the MMU protection handler.  It's too big to fit
 * in the trap table so I moved it here.  It's relatively simple.
 * It looks up the page mapping in the page table associated with
 * the trapping context.  It checks to see if the S/W writable bit
 * is set.  If so, it sets the H/W write bit, marks the tte modified,
 * and enters the mapping into the MMU.  Otherwise it does a regular
 * data fault.
 */
	ICACHE_ALIGN
dmmu_write_fault:
	mov	TLB_TAG_ACCESS, %g3
	sethi	%hi(0x1fff), %g6			! 8K context mask
	ldxa	[%g3] ASI_DMMU, %g3			! Get fault addr from Tag Target
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	or	%g6, %lo(0x1fff), %g6
	LDPTR	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4
	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	and	%g3, %g6, %g6				! Isolate context

	inc	%g5					! (0 or -1) -> (1 or 0)
	sllx	%g6, 3, %g6				! Make it into an offset into ctxbusy
	ldx	[%g4+%g6], %g4				! Load up our page table.
	srlx	%g3, STSHIFT, %g6
	cmp	%g5, 1
	bgu,pn %xcc, winfix				! Error!
	 srlx	%g3, PDSHIFT, %g5
	and	%g6, STMASK, %g6
	sll	%g6, 3, %g6

	and	%g5, PDMASK, %g5
	sll	%g5, 3, %g5
	add	%g6, %g4, %g4
	DLFLUSH(%g4,%g6)
	ldxa	[%g4] ASI_PHYS_CACHED, %g4
	DLFLUSH2(%g6)
	srlx	%g3, PTSHIFT, %g6			! Convert to ptab offset
	and	%g6, PTMASK, %g6
	add	%g5, %g4, %g5
	brz,pn	%g4, winfix				! NULL entry? check somewhere else
	 nop	

	ldxa	[%g5] ASI_PHYS_CACHED, %g4
	sll	%g6, 3, %g6
	brz,pn	%g4, winfix				! NULL entry? check somewhere else
	 add	%g6, %g4, %g6
1:
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, winfix				! Entry invalid?  Punt
	 or	%g4, TTE_MODIFY|TTE_ACCESS|TTE_W, %g7	! Update the modified bit

	btst	TTE_REAL_W|TTE_W, %g4			! Is it a ref fault?
	bz,pn	%xcc, winfix				! No -- really fault
#ifdef DEBUG
	/* Make sure we don't try to replace a kernel translation */
	/* This should not be necessary */
	sllx	%g3, 64-13, %g2				! Isolate context bits
	sethi	%hi(KERNBASE), %g5			! Don't need %lo
	brnz,pt	%g2, 0f					! Ignore context != 0
	 set	0x0800000, %g2				! 8MB
	sub	%g3, %g5, %g5
	cmp	%g5, %g2
	tlu	%xcc, 1; nop
	blu,pn	%xcc, winfix				! Next insn in delay slot is unimportant
0:
#endif
	/* Need to check for and handle large pages. */
	 srlx	%g4, 61, %g5				! Isolate the size bits
	ldxa	[%g0] ASI_DMMU_8KPTR, %g2		! Load DMMU 8K TSB pointer
	andcc	%g5, 0x3, %g5				! 8K?
	bnz,pn	%icc, winfix				! We punt to the pmap code since we can't handle policy
	 ldxa	[%g0] ASI_DMMU, %g1			! Load DMMU tag target register
	casxa	[%g6] ASI_PHYS_CACHED, %g4, %g7		!  and write it out
	membar	#StoreLoad
	cmp	%g4, %g7
	bne,pn	%xcc, 1b
	 or	%g4, TTE_MODIFY|TTE_ACCESS|TTE_W, %g4	! Update the modified bit
	stx	%g1, [%g2]				! Update TSB entry tag
	mov	SFSR, %g7
	stx	%g4, [%g2+8]				! Update TSB entry data
	nop

#ifdef DEBUG
	set	DATA_START, %g6	! debug
	stx	%g1, [%g6+0x40]	! debug
	set	0x88, %g5	! debug
	stx	%g4, [%g6+0x48]	! debug -- what we tried to enter in TLB
	stb	%g5, [%g6+0x8]	! debug
#endif
#ifdef TRAPSTATS
	sethi	%hi(_C_LABEL(protfix)), %g1
	lduw	[%g1+%lo(_C_LABEL(protfix))], %g2
	inc	%g2
	stw	%g2, [%g1+%lo(_C_LABEL(protfix))]
#endif
	mov	DEMAP_PAGE_SECONDARY, %g1		! Secondary flush
	mov	DEMAP_PAGE_NUCLEUS, %g5			! Nucleus flush
	stxa	%g0, [%g7] ASI_DMMU			! clear out the fault
	sllx	%g3, (64-13), %g7			! Need to demap old entry first
	andn	%g3, 0xfff, %g6
	movrz	%g7, %g5, %g1				! Pick one
	or	%g6, %g1, %g6
	membar	#Sync
	stxa	%g6, [%g6] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync
	
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
#if 0 & KTR_COMPILE & KTR_TRAP
	CATR(KTR_TRAP, "data_miss:", %g3, %g4, %g5, 10, 11, 12)
12:
#endif
	mov	TLB_TAG_ACCESS, %g3			! Get real fault page
	sethi	%hi(0x1fff), %g6			! 8K context mask
	ldxa	[%g3] ASI_DMMU, %g3			! from tag access register
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	or	%g6, %lo(0x1fff), %g6
	LDPTR	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4
	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	and	%g3, %g6, %g6				! Isolate context
	
	inc	%g5					! (0 or -1) -> (1 or 0)
	sllx	%g6, 3, %g6				! Make it into an offset into ctxbusy
	ldx	[%g4+%g6], %g4				! Load up our page table.
#ifdef DEBUG
	/* Make sure we don't try to replace a kernel translation */
	/* This should not be necessary */
	brnz,pt	%g6, 1f			! If user context continue miss
	sethi	%hi(KERNBASE), %g7			! Don't need %lo
	set	0x0800000, %g6				! 8MB
	sub	%g3, %g7, %g7
	cmp	%g7, %g6
	sethi	%hi(DATA_START), %g7
	mov	6, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
	tlu	%xcc, 1; nop
	blu,pn	%xcc, winfix				! Next insn in delay slot is unimportant
	 mov	7, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
1:	
#endif
	srlx	%g3, STSHIFT, %g6
	cmp	%g5, 1
	bgu,pn %xcc, winfix				! Error!
	 srlx	%g3, PDSHIFT, %g5
	and	%g6, STMASK, %g6
	
	sll	%g6, 3, %g6
	and	%g5, PDMASK, %g5
	sll	%g5, 3, %g5
	add	%g6, %g4, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4
	srlx	%g3, PTSHIFT, %g6			! Convert to ptab offset
	and	%g6, PTMASK, %g6
	add	%g5, %g4, %g5
	brz,pn	%g4, data_nfo				! NULL entry? check somewhere else
	
	 nop
	ldxa	[%g5] ASI_PHYS_CACHED, %g4
	sll	%g6, 3, %g6
	brz,pn	%g4, data_nfo				! NULL entry? check somewhere else
	 add	%g6, %g4, %g6

1:
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, data_nfo				! Entry invalid?  Punt
	 or	%g4, TTE_ACCESS, %g7			! Update the access bit
	
	btst	TTE_ACCESS, %g4				! Need to update access git?
	bne,pt	%xcc, 1f
	 nop
	casxa	[%g6] ASI_PHYS_CACHED, %g4, %g7		!  and write it out
	cmp	%g4, %g7
	bne,pn	%xcc, 1b
	 or	%g4, TTE_ACCESS, %g4			! Update the access bit

1:	
	stx	%g1, [%g2]				! Update TSB entry tag
	stx	%g4, [%g2+8]				! Update TSB entry data
#ifdef DEBUG
	set	DATA_START, %g6	! debug
	stx	%g3, [%g6+8]	! debug
	set	0xa, %g5	! debug
	stx	%g4, [%g6]	! debug -- what we tried to enter in TLB
	stb	%g5, [%g6+0x20]	! debug
#endif
	stxa	%g4, [%g0] ASI_DMMU_DATA_IN		! Enter new mapping
	membar	#Sync
	CLRTT
	retry
	NOTREACHED
/*
 * We had a data miss but did not find a mapping.  Insert
 * a NFO mapping to satisfy speculative loads and return.
 * If this had been a real load, it will re-execute and
 * result in a data fault or protection fault rather than
 * a TLB miss.  We insert an 8K TTE with the valid and NFO
 * bits set.  All others should zero.  The TTE looks like this:
 *
 *	0x9000000000000000
 *
 */
data_nfo:
	sethi	%hi(0x90000000), %g4			! V(0x8)|NFO(0x1)
	sllx	%g4, 32, %g4
	stxa	%g4, [%g0] ASI_DMMU_DATA_IN		! Enter new mapping
	membar	#Sync
	CLRTT
	retry	

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
	mov	TLB_TAG_ACCESS, %g3	! Get real fault page from tag access register
	ldxa	[%g3] ASI_DMMU, %g3	! And put it into the non-MMU alternate regs
winfix:
	rdpr	%tl, %g2
	subcc	%g2, 1, %g1
	ble,pt	%icc, datafault		! Don't go below trap level 1
	 sethi	%hi(CPCB), %g6		! get current pcb


	CHKPT(%g4,%g7,0x20)
	wrpr	%g1, 0, %tl		! Pop a trap level
	rdpr	%tt, %g7		! Read type of prev. trap
	rdpr	%tstate, %g4		! Try to restore prev %cwp if we were executing a restore
	andn	%g7, 0x3f, %g5		!   window fill traps are all 0b 0000 11xx xxxx

#if 1
	cmp	%g7, 0x30		! If we took a datafault just before this trap
	bne,pt	%icc, winfixfill	! our stack's probably bad so we need to switch somewhere else
	 nop

	!!
	!! Double data fault -- bad stack?
	!!
	wrpr	%g2, %tl		! Restore trap level.
	sir				! Just issue a reset and don't try to recover.
	mov	%fp, %l6		! Save the frame pointer
	set	EINTSTACK+USPACE+CC64FSZ-STKB, %fp ! Set the frame pointer to the middle of the idle stack
	add	%fp, -CC64FSZ, %sp	! Create a stackframe
	wrpr	%g0, 15, %pil		! Disable interrupts, too
	wrpr	%g0, %g0, %canrestore	! Our stack is hozed and our PCB
	wrpr	%g0, 7, %cansave	!  probably is too, so blow away
	ba	slowtrap		!  all our register windows.
	 wrpr	%g0, 0x101, %tt
#endif

winfixfill:
	cmp	%g5, 0x0c0		!   so we mask lower bits & compare to 0b 0000 1100 0000
	bne,pt	%icc, winfixspill	! Dump our trap frame -- we will retry the fill when the page is loaded
	 cmp	%g5, 0x080		!   window spill traps are all 0b 0000 10xx xxxx

	!!
	!! This was a fill
	!!
#ifdef TRAPSTATS
	set	_C_LABEL(wfill), %g1
	lduw	[%g1], %g5
	inc	%g5
	stw	%g5, [%g1]
#endif
	btst	TSTATE_PRIV, %g4	! User mode?
	and	%g4, CWP, %g5		! %g4 = %cwp of trap
	wrpr	%g7, 0, %tt
	bz,a,pt	%icc, datafault		! We were in user mode -- normal fault
	 wrpr	%g5, %cwp		! Restore cwp from before fill trap -- regs should now be consisent

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
#if 0 /* Need to switch over to new stuff to fix WDR bug */
	wrpr	%g5, %cwp				! Restore cwp from before fill trap -- regs should now be consisent
	wrpr	%g2, %g0, %tl				! Restore trap level -- we need to reuse it
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
#else
	wrpr	%g2, %g0, %tl				! Restore trap level
	cmp	%g2, 3
	tne	%icc, 1
	rdpr	%tt, %g5
	wrpr	%g0, 1, %tl				! Revert to TL==1 XXX what if this wasn't in rft_user? Oh well.
	wrpr	%g5, %g0, %tt				! Set trap type correctly
	CHKPT(%g5,%g7,0xe)
/*
 * Here we need to implement the beginning of datafault.
 * TRAP_SETUP expects to come from either kernel mode or
 * user mode with at least one valid register window.  It
 * will allocate a trap frame, save the out registers, and
 * fix the window registers to think we have one user
 * register window.
 *
 * However, under these circumstances we don't have any
 * valid register windows, so we need to clean up the window
 * registers to prevent garbage from being saved to either
 * the user stack or the PCB before calling the datafault
 * handler.
 *
 * We could simply jump to datafault if we could somehow
 * make the handler issue a `saved' instruction immediately
 * after creating the trapframe.
 *
 * The following is duplicated from datafault:
 */
	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate	! We need to save volatile stuff to AG regs
#ifdef TRAPS_USE_IG
	wrpr	%g0, PSTATE_KERN|PSTATE_IG, %pstate	! We need to save volatile stuff to AG regs
#endif
#ifdef DEBUG
	set	DATA_START, %g7				! debug
	set	0x20, %g6				! debug
	stx	%g0, [%g7]				! debug
	stb	%g6, [%g7 + 0x20]			! debug
	CHKPT(%g4,%g7,0xf)
#endif
	wr	%g0, ASI_DMMU, %asi			! We need to re-load trap info
	ldxa	[%g0 + TLB_TAG_ACCESS] %asi, %g1	! Get fault address from tag access register
	ldxa	[SFAR] %asi, %g2			! sync virt addr; must be read first
	ldxa	[SFSR] %asi, %g3			! get sync fault status register
	stxa	%g0, [SFSR] %asi			! Clear out fault now

	TRAP_SETUP(-CC64FSZ-TF_SIZE)
	saved						! Blow away that one register window we didn't ever use.
	ba,a,pt	%icc, Ldatafault_internal		! Now we should return directly to user mode
	 nop
#endif
winfixspill:
	bne,a,pt	%xcc, datafault			! Was not a spill -- handle it normally
	 wrpr	%g2, 0, %tl				! Restore trap level for now XXXX

	!!
	!! This was a spill
	!!
#if 1
	btst	TSTATE_PRIV, %g4	! From user mode?
	wrpr	%g2, 0, %tl		! We need to load the fault type so we can
	rdpr	%tt, %g5		! overwrite the lower trap and get it to the fault handler
	wrpr	%g1, 0, %tl
	wrpr	%g5, 0, %tt		! Copy over trap type for the fault handler
	and	%g4, CWP, %g5		! find %cwp from trap
	be,a,pt	%xcc, datafault		! Let's do a regular datafault.  When we try a save in datafault we'll
	 wrpr	%g5, 0, %cwp		!  return here and write out all dirty windows.
#endif
	wrpr	%g2, 0, %tl				! Restore trap level for now XXXX
	LDPTR	[%g6 + %lo(CPCB)], %g6	! This is in the locked TLB and should not fault
#ifdef TRAPSTATS
	set	_C_LABEL(wspill), %g7
	lduw	[%g7], %g5
	inc	%g5
	stw	%g5, [%g7]
#endif
#ifdef DEBUG
	set	0x12, %g5				! debug
	sethi	%hi(DATA_START), %g7			! debug
	stb	%g5, [%g7 + 0x20]			! debug
	CHKPT(%g5,%g7,0x11)
#endif

	/*
	 * Traverse kernel map to find paddr of cpcb and only us ASI_PHYS_CACHED to
	 * prevent any faults while saving the windows.  BTW if it isn't mapped, we
	 * will trap and hopefully panic.
	 */

!	ba	0f					! DEBUG -- don't use phys addresses
	 wr	%g0, ASI_NUCLEUS, %asi			! In case of problems finding PA
	sethi	%hi(_C_LABEL(ctxbusy)), %g1
	LDPTR	[%g1 + %lo(_C_LABEL(ctxbusy))], %g1	! Load start of ctxbusy
#ifdef DEBUG
	srax	%g6, HOLESHIFT, %g7			! Check for valid address
	brz,pt	%g7, 1f					! Should be zero or -1
	 addcc	%g7, 1, %g7					! Make -1 -> 0
	tnz	%xcc, 1					! Invalid address??? How did this happen?
1:
#endif
	srlx	%g6, STSHIFT, %g7
	ldx	[%g1], %g1				! Load pointer to kernel_pmap
	and	%g7, STMASK, %g7
	sll	%g7, 3, %g7
	add	%g7, %g1, %g1
	DLFLUSH(%g1,%g7)
	ldxa	[%g1] ASI_PHYS_CACHED, %g1		! Load pointer to directory
	DLFLUSH2(%g7)

	srlx	%g6, PDSHIFT, %g7			! Do page directory
	and	%g7, PDMASK, %g7
	sll	%g7, 3, %g7
	brz,pn	%g1, 0f
	 add	%g7, %g1, %g1
	DLFLUSH(%g1,%g7)
	ldxa	[%g1] ASI_PHYS_CACHED, %g1
	DLFLUSH2(%g7)

	srlx	%g6, PTSHIFT, %g7			! Convert to ptab offset
	and	%g7, PTMASK, %g7
	brz	%g1, 0f
	 sll	%g7, 3, %g7
	add	%g1, %g7, %g7
	DLFLUSH(%g7,%g1)
	ldxa	[%g7] ASI_PHYS_CACHED, %g7		! This one is not
	DLFLUSH2(%g1)
	brgez	%g7, 0f
	 srlx	%g7, PGSHIFT, %g7			! Isolate PA part
	sll	%g6, 32-PGSHIFT, %g6			! And offset
	sllx	%g7, PGSHIFT+23, %g7			! There are 23 bits to the left of the PA in the TTE
	srl	%g6, 32-PGSHIFT, %g6
	srax	%g7, 23, %g7
	or	%g7, %g6, %g6				! Then combine them to form PA

	wr	%g0, ASI_PHYS_CACHED, %asi		! Use ASI_PHYS_CACHED to prevent possible page faults
0:
	/*
	 * Now save all user windows to cpcb.
	 */
#ifdef NOTDEF_DEBUG
	add	%g6, PCB_NSAVED, %g7
	DLFLUSH(%g7,%g5)
	lduba	[%g6 + PCB_NSAVED] %asi, %g7		! make sure that pcb_nsaved
	DLFLUSH2(%g5)
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
	add	%g6, PCB_NSAVED, %g7
	DLFLUSH(%g7,%g5)
	lduba	[%g6 + PCB_NSAVED] %asi, %g7		! Start incrementing pcb_nsaved
	DLFLUSH2(%g5)

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
	sethi	%hi(PANICSTACK), %sp
	LDPTR	[%sp + %lo(PANICSTACK)], %sp
	add	%sp, -CC64FSZ-STKB, %sp
	ta	1; nop					! This helps out traptrace.
	call	_C_LABEL(panic)				! This needs to be fixed properly but we should panic here
	 mov	%g1, %o1
	NOTREACHED
	.data
2:
	.asciz	"winfault: double invalid window at %p, nsaved=%d"
	_ALIGN
	.text
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
	.data
8:
	.asciz	"winfix: spill fixup\n"
	_ALIGN
	.text
#endif
	CHKPT(%g5,%g1,0x15)
!	rdpr	%tl, %g2				! DEBUG DEBUG -- did we trap somewhere?
	sub	%g2, 1, %g1
	rdpr	%tt, %g2
	wrpr	%g1, 0, %tl				! We will not attempt to re-execute the spill, so dump our trap frame permanently
	wrpr	%g2, 0, %tt				! Move trap type from fault frame here, overwriting spill
	CHKPT(%g2,%g5,0x16)

	/* Did we save a user or kernel window ? */
!	srax	%g3, 48, %g5				! User or kernel store? (TAG TARGET)
	sllx	%g3, (64-13), %g5			! User or kernel store? (TAG ACCESS)
	sethi	%hi((2*NBPG)-8), %g7
	brnz,pt	%g5, 1f					! User fault -- save windows to pcb
	 or	%g7, %lo((2*NBPG)-8), %g7

	and	%g4, CWP, %g4				! %g4 = %cwp of trap
	wrpr	%g4, 0, %cwp				! Kernel fault -- restore %cwp and force and trap to debugger
#ifdef DEBUG
	set	DATA_START, %g7				! debug
	set	0x11, %g6				! debug
	stb	%g6, [%g7 + 0x20]			! debug
	CHKPT(%g2,%g1,0x17)
!	sir
#endif
	!!
	!! Here we managed to fault trying to access a kernel window
	!! This is a bug.  Switch to the interrupt stack if we aren't
	!! there already and then trap into the debugger or panic.
	!!
	sethi	%hi(EINTSTACK-BIAS), %g6
	btst	1, %sp
	bnz,pt	%icc, 0f
	 mov	%sp, %g1
	add	%sp, -BIAS, %g1
0:
	or	%g6, %lo(EINTSTACK-BIAS), %g6
	set	(EINTSTACK-INTSTACK), %g7	! XXXXXXXXXX This assumes kernel addresses are unique from user addresses
	sub	%g6, %g1, %g2				! Determine if we need to switch to intr stack or not
	dec	%g7					! Make it into a mask
	andncc	%g2, %g7, %g0				! XXXXXXXXXX This assumes kernel addresses are unique from user addresses */ \
	movz	%xcc, %g1, %g6				! Stay on interrupt stack?
	add	%g6, -CCFSZ, %g6			! Allocate a stack frame
	mov	%sp, %l6				! XXXXX Save old stack pointer
	mov	%g6, %sp
	ta	1; nop					! Enter debugger
	NOTREACHED
1:
#if 1
	/* Now we need to blast away the D$ to make sure we're in sync */
	stxa	%g0, [%g7] ASI_DCACHE_TAG
	brnz,pt	%g7, 1b
	 dec	8, %g7
#endif

#ifdef DEBUG
	CHKPT(%g2,%g1,0x18)
	set	DATA_START, %g7				! debug
	set	0x19, %g6				! debug
	stb	%g6, [%g7 + 0x20]			! debug
#endif
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
	.data
8:
	.asciz	"winfix: kernel spill retry\n"
	_ALIGN
	.text
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
	mov	%g3, %g3
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
#ifdef DEBUG
	set	DATA_START, %g7				! debug
	set	0x20, %g6				! debug
	stx	%g0, [%g7]				! debug
	stb	%g6, [%g7 + 0x20]			! debug
	CHKPT(%g4,%g7,0xf)
#endif
	wr	%g0, ASI_DMMU, %asi			! We need to re-load trap info
	ldxa	[%g0 + TLB_TAG_ACCESS] %asi, %g1	! Get fault address from tag access register
	ldxa	[SFAR] %asi, %g2			! sync virt addr; must be read first
	ldxa	[SFSR] %asi, %g3			! get sync fault status register
	stxa	%g0, [SFSR] %asi			! Clear out fault now

	TRAP_SETUP(-CC64FSZ-TF_SIZE)
Ldatafault_internal:
	INCR(_C_LABEL(uvmexp)+V_FAULTS)			! cnt.v_faults++ (clobbers %o0,%o1,%o2) should not fault
!	ldx	[%sp + CC64FSZ + STKB + TF_FAULT], %g1		! DEBUG make sure this has not changed
	mov	%g1, %o0				! Move these to the out regs so we can save the globals
	mov	%g2, %o4
	mov	%g3, %o5

	ldxa	[%g0] ASI_AFAR, %o2			! get async fault address
	ldxa	[%g0] ASI_AFSR, %o3			! get async fault status
	mov	-1, %g7
	stxa	%g7, [%g0] ASI_AFSR			! And clear this out, too

	wrpr	%g0, PSTATE_KERN, %pstate		! Get back to normal globals

	stx	%g1, [%sp + CC64FSZ + STKB + TF_G + (1*8)]	! save g1
	rdpr	%tt, %o1					! find out what trap brought us here
	stx	%g2, [%sp + CC64FSZ + STKB + TF_G + (2*8)]	! save g2
	rdpr	%tstate, %g1
	stx	%g3, [%sp + CC64FSZ + STKB + TF_G + (3*8)]	! (sneak g3 in here)
	rdpr	%tpc, %g2
	stx	%g4, [%sp + CC64FSZ + STKB + TF_G + (4*8)]	! sneak in g4
	rdpr	%tnpc, %g3
	stx	%g5, [%sp + CC64FSZ + STKB + TF_G + (5*8)]	! sneak in g5
	mov	%g2, %o7					! Make the fault address look like the return address
	stx	%g6, [%sp + CC64FSZ + STKB + TF_G + (6*8)]	! sneak in g6
	rd	%y, %g5						! save y
	stx	%g7, [%sp + CC64FSZ + STKB + TF_G + (7*8)]	! sneak in g7

#ifdef DEBUG
	set	DATA_START, %g7					! debug
	set	0x21, %g6					! debug
	stb	%g6, [%g7 + 0x20]				! debug
#endif
	sth	%o1, [%sp + CC64FSZ + STKB + TF_TT]
	stx	%g1, [%sp + CC64FSZ + STKB + TF_TSTATE]		! set tf.tf_psr, tf.tf_pc
	stx	%g2, [%sp + CC64FSZ + STKB + TF_PC]		! set tf.tf_npc
	stx	%g3, [%sp + CC64FSZ + STKB + TF_NPC]

	rdpr	%pil, %g4
	stb	%g4, [%sp + CC64FSZ + STKB + TF_PIL]
	stb	%g4, [%sp + CC64FSZ + STKB + TF_OLDPIL]

#if 1
	rdpr	%tl, %g7
	dec	%g7
	movrlz	%g7, %g0, %g7
	CHKPT(%g1,%g3,0x21)
	wrpr	%g0, %g7, %tl		! Revert to kernel mode
#else
	CHKPT(%g1,%g3,0x21)
	wrpr	%g0, 0, %tl		! Revert to kernel mode
#endif
	/* Finish stackframe, call C trap handler */
	flushw						! Get this clean so we won't take any more user faults
#ifdef NOTDEF_DEBUG
	set	CPCB, %o7
	LDPTR	[%o7], %o7
	ldub	[%o7 + PCB_NSAVED], %o7
	brz,pt	%o7, 2f
	 nop
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	call printf
	 mov	%i7, %o1
	ta	1; nop
	 restore
	.data
1:	.asciz	"datafault: nsaved = %d\n"
	_ALIGN
	.text
2:
#endif
	!! In the EMBEDANY memory model %g4 points to the start of the data segment.
	!! In our case we need to clear it before calling any C-code
	clr	%g4

	/*
	 * Right now the registers have the following values:
	 *
	 *	%o0 -- MMU_TAG_ACCESS
	 *	%o1 -- TT
	 *	%o2 -- afar
	 *	%o3 -- afsr
	 *	%o4 -- sfar
	 *	%o5 -- sfsr
	 */

	cmp	%o1, T_DATA_ERROR
	st	%g5, [%sp + CC64FSZ + STKB + TF_Y]
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi	! Restore default ASI
	be,pn	%icc, data_error
	 wrpr	%g0, PSTATE_INTR, %pstate	! reenable interrupts

	mov	%o0, %o3			! (argument: trap address)
	mov	%g2, %o2			! (argument: trap pc)
	call	_C_LABEL(data_access_fault)	! data_access_fault(&tf, type, 
						!	pc, addr, sfva, sfsr)
	 add	%sp, CC64FSZ + STKB, %o0	! (argument: &tf)
	wrpr	%g0, PSTATE_KERN, %pstate		! disable interrupts

data_recover:
	CHKPT(%o1,%o2,1)
#ifdef TRAPSTATS
	set	_C_LABEL(uintrcnt), %g1
	stw	%g0, [%g1]
	set	_C_LABEL(iveccnt), %g1
	stw	%g0, [%g1]
#endif
	b	return_from_trap			! go return
	 ldx	[%sp + CC64FSZ + STKB + TF_TSTATE], %g1		! Load this for return_from_trap
	NOTREACHED

data_error:
	call	_C_LABEL(data_access_error)	! data_access_error(&tf, type, 
						!	afva, afsr, sfva, sfsr)
	 add	%sp, CC64FSZ + STKB, %o0	! (argument: &tf)
	ba	data_recover
	 nop
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
	mov	TLB_TAG_ACCESS, %g3			! Get real fault page
	sethi	%hi(0x1fff), %g7			! 8K context mask
	ldxa	[%g3] ASI_IMMU, %g3			! from tag access register
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	or	%g7, %lo(0x1fff), %g7
	LDPTR	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4
	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	and	%g3, %g7, %g6				! Isolate context
	sllx	%g6, 3, %g6				! Make it into an offset into ctxbusy
	inc	%g5					! (0 or -1) -> (1 or 0)
	ldx	[%g4+%g6], %g4				! Load up our page table.
#ifdef DEBUG
	/* Make sure we don't try to replace a kernel translation */
	/* This should not be necessary */
	brnz,pt	%g6, 1f					! If user context continue miss
	sethi	%hi(KERNBASE), %g7			! Don't need %lo
	set	0x0800000, %g6				! 8MB
	sub	%g3, %g7, %g7
	cmp	%g7, %g6
	mov	6, %g6		! debug
	sethi	%hi(DATA_START), %g7
	stb	%g6, [%g7+0x30]	! debug
	tlu	%xcc, 1; nop
	blu,pn	%xcc, textfault				! Next insn in delay slot is unimportant
	 mov	7, %g6		! debug
	stb	%g6, [%g7+0x30]	! debug
1:	
#endif
	srlx	%g3, STSHIFT, %g6
	cmp	%g5, 1
	bgu,pn %xcc, textfault				! Error!
	 srlx	%g3, PDSHIFT, %g5
	and	%g6, STMASK, %g6
	sll	%g6, 3, %g6
	and	%g5, PDMASK, %g5
	nop

	sll	%g5, 3, %g5
	add	%g6, %g4, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4
	srlx	%g3, PTSHIFT, %g6			! Convert to ptab offset
	and	%g6, PTMASK, %g6
	add	%g5, %g4, %g5
	brz,pn	%g4, textfault				! NULL entry? check somewhere else
	 nop
	
	ldxa	[%g5] ASI_PHYS_CACHED, %g4
	sll	%g6, 3, %g6
	brz,pn	%g4, textfault				! NULL entry? check somewhere else
	 add	%g6, %g4, %g6		
1:
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, textfault
	 nop

	/* Check if it's an executable mapping. */
	andcc	%g4, TTE_EXEC, %g0
	bz,pn	%xcc, textfault
	 nop

	or	%g4, TTE_ACCESS, %g7			! Update accessed bit
	btst	TTE_ACCESS, %g4				! Need to update access git?
	bne,pt	%xcc, 1f
	 nop
	casxa	[%g6] ASI_PHYS_CACHED, %g4, %g7		!  and store it
	cmp	%g4, %g7
	bne,pn	%xcc, 1b
	 or	%g4, TTE_ACCESS, %g4			! Update accessed bit
1:
	stx	%g1, [%g2]				! Update TSB entry tag
	stx	%g4, [%g2+8]				! Update TSB entry data
#ifdef DEBUG
	set	DATA_START, %g6	! debug
	stx	%g3, [%g6+8]	! debug
	set	0xaa, %g3	! debug
	stx	%g4, [%g6]	! debug -- what we tried to enter in TLB
	stb	%g3, [%g6+0x20]	! debug
#endif
	stxa	%g4, [%g0] ASI_IMMU_DATA_IN		! Enter new mapping
	membar	#Sync
	CLRTT
	retry
	NOTREACHED
	!!
	!!  Check our prom mappings -- temporary
	!!

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
	ldxa	[SFSR] %asi, %g3			! get sync fault status register
	membar	#LoadStore
	stxa	%g0, [SFSR] %asi			! Clear out old info

	TRAP_SETUP(-CC64FSZ-TF_SIZE)
	INCR(_C_LABEL(uvmexp)+V_FAULTS)			! cnt.v_faults++ (clobbers %o0,%o1,%o2)

	mov	%g3, %o3

	wrpr	%g0, PSTATE_KERN, %pstate		! Switch to normal globals
	ldxa	[%g0] ASI_AFSR, %o4			! get async fault status
	ldxa	[%g0] ASI_AFAR, %o5			! get async fault address
	mov	-1, %o0
	stxa	%o0, [%g0] ASI_AFSR			! Clear this out
	stx	%g1, [%sp + CC64FSZ + STKB + TF_G + (1*8)]	! save g1
	stx	%g2, [%sp + CC64FSZ + STKB + TF_G + (2*8)]	! save g2
	stx	%g3, [%sp + CC64FSZ + STKB + TF_G + (3*8)]	! (sneak g3 in here)
	rdpr	%tt, %o1					! Find out what caused this trap
	stx	%g4, [%sp + CC64FSZ + STKB + TF_G + (4*8)]	! sneak in g4
	rdpr	%tstate, %g1
	stx	%g5, [%sp + CC64FSZ + STKB + TF_G + (5*8)]	! sneak in g5
	rdpr	%tpc, %o2					! sync virt addr; must be read first
	stx	%g6, [%sp + CC64FSZ + STKB + TF_G + (6*8)]	! sneak in g6
	rdpr	%tnpc, %g3
	stx	%g7, [%sp + CC64FSZ + STKB + TF_G + (7*8)]	! sneak in g7
	rd	%y, %g5						! save y

	/* Finish stackframe, call C trap handler */
	stx	%g1, [%sp + CC64FSZ + STKB + TF_TSTATE]		! set tf.tf_psr, tf.tf_pc
	sth	%o1, [%sp + CC64FSZ + STKB + TF_TT]		! debug

	stx	%o2, [%sp + CC64FSZ + STKB + TF_PC]
	stx	%g3, [%sp + CC64FSZ + STKB + TF_NPC]		! set tf.tf_npc

	rdpr	%pil, %g4
	stb	%g4, [%sp + CC64FSZ + STKB + TF_PIL]
	stb	%g4, [%sp + CC64FSZ + STKB + TF_OLDPIL]

	rdpr	%tl, %g7
	dec	%g7
	movrlz	%g7, %g0, %g7
	CHKPT(%g1,%g3,0x22)
	wrpr	%g0, %g7, %tl		! Revert to kernel mode

	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore default ASI
	flushw						! Get rid of any user windows so we don't deadlock
	
	!! In the EMBEDANY memory model %g4 points to the start of the data segment.
	!! In our case we need to clear it before calling any C-code
	clr	%g4

	/* Use trap type to see what handler to call */
	cmp	%o1, T_INST_ERROR
	be,pn	%xcc, text_error
	 st	%g5, [%sp + CC64FSZ + STKB + TF_Y]		! set tf.tf_y

	wrpr	%g0, PSTATE_INTR, %pstate	! reenable interrupts
	call	_C_LABEL(text_access_fault)	! mem_access_fault(&tf, type, pc, sfsr)
	 add	%sp, CC64FSZ + STKB, %o0	! (argument: &tf)
text_recover:
	CHKPT(%o1,%o2,2)
	wrpr	%g0, PSTATE_KERN, %pstate	! disable interrupts
	b	return_from_trap		! go return
	 ldx	[%sp + CC64FSZ + STKB + TF_TSTATE], %g1	! Load this for return_from_trap
	NOTREACHED

text_error:
	wrpr	%g0, PSTATE_INTR, %pstate	! reenable interrupts
	call	_C_LABEL(text_access_error)	! mem_access_fault(&tfm type, sfva [pc], sfsr,
						!		afva, afsr);
	 add	%sp, CC64FSZ + STKB, %o0	! (argument: &tf)
	ba	text_recover
	 nop
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
/*
 * XXX I don't think this is at all revelent for V9.
 */
fp_exception:
	rdpr	%tpc, %g1
	set	special_fp_store, %g4	! see if we came from the special one
	cmp	%g1, %g4		! pc == special_fp_store?
	bne	slowtrap		! no, go handle per usual
	 sethi	%hi(savefpcont), %g4	! yes, "return" to the special code
	or	%lo(savefpcont), %g4, %g4
	wrpr	%g0, %g4, %tnpc
	 done
	NOTREACHED


/*
 * We're here because we took an alignment fault in NUCLEUS context.
 * This could be a kernel bug or it could be due to saving a user
 * window to an invalid stack pointer.  
 * 
 * If the latter is the case, we could try to emulate unaligned accesses, 
 * but we really don't know where to store the registers since we can't 
 * determine if there's a stack bias.  Or we could store all the regs 
 * into the PCB and punt, until the user program uses up all the CPU's
 * register windows and we run out of places to store them.  So for
 * simplicity we'll just blow them away and enter the trap code which
 * will generate a bus error.  Debugging the problem will be a bit
 * complicated since lots of register windows will be lost, but what
 * can we do?
 */
checkalign:
	rdpr	%tl, %g2
	subcc	%g2, 1, %g1
	bneg,pn	%icc, slowtrap		! Huh?
	 sethi	%hi(CPCB), %g6		! get current pcb

	wrpr	%g1, 0, %tl
	rdpr	%tt, %g7
	rdpr	%tstate, %g4
	andn	%g7, 0x3f, %g5
	cmp	%g5, 0x080		!   window spill traps are all 0b 0000 10xx xxxx
	bne,a,pn	%icc, slowtrap
	 wrpr	%g1, 0, %tl		! Revert TL  XXX wrpr in a delay slot...

#ifdef DEBUG
	cmp	%g7, 0x34		! If we took a datafault just before this trap
	bne,pt	%icc, checkalignspill	! our stack's probably bad so we need to switch somewhere else
	 nop

	!!
	!! Double data fault -- bad stack?
	!!
	wrpr	%g2, %tl		! Restore trap level.
	sir				! Just issue a reset and don't try to recover.
	mov	%fp, %l6		! Save the frame pointer
	set	EINTSTACK+USPACE+CC64FSZ-STKB, %fp ! Set the frame pointer to the middle of the idle stack
	add	%fp, -CC64FSZ, %sp	! Create a stackframe
	wrpr	%g0, 15, %pil		! Disable interrupts, too
	wrpr	%g0, %g0, %canrestore	! Our stack is hozed and our PCB
	wrpr	%g0, 7, %cansave	!  probably is too, so blow away
	ba	slowtrap		!  all our register windows.
	 wrpr	%g0, 0x101, %tt
#endif
checkalignspill:
	/*
         * %g1 -- current tl
	 * %g2 -- original tl
	 * %g4 -- tstate
         * %g7 -- tt
	 */

	and	%g4, CWP, %g5
	wrpr	%g5, %cwp		! Go back to the original register win

	/*
	 * Remember:
	 * 
	 * %otherwin = 0
	 * %cansave = NWINDOWS - 2 - %canrestore
	 */

	rdpr	%otherwin, %g6
	rdpr	%canrestore, %g3
	rdpr	%ver, %g5
	sub	%g3, %g6, %g3		! Calculate %canrestore - %g7
	and	%g5, CWP, %g5		! NWINDOWS-1
	movrlz	%g3, %g0, %g3		! Clamp at zero
	wrpr	%g0, 0, %otherwin
	wrpr	%g3, 0, %canrestore	! This is the new canrestore
	dec	%g5			! NWINDOWS-2
	wrpr	%g5, 0, %cleanwin	! Set cleanwin to max, since we're in-kernel
	sub	%g5, %g3, %g5		! NWINDOWS-2-%canrestore
	wrpr	%g5, 0, %cansave

	wrpr	%g0, T_ALIGN, %tt	! This was an alignment fault 
	/*
	 * Now we need to determine if this was a userland store or not.
	 * Userland stores occur in anything other than the kernel spill
	 * handlers (trap type 09x).
	 */
	and	%g7, 0xff0, %g5
	cmp	%g5, 0x90
	bz,pn	%icc, slowtrap
	 nop
	bclr	TSTATE_PRIV, %g4
	wrpr	%g4, 0, %tstate
	ba,a,pt	%icc, slowtrap
	 nop
	
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
#ifdef DIAGNOSTIC
	/* Make sure kernel stack is aligned */
	btst	0x03, %sp		! 32-bit stack OK?
	 and	%sp, 0x07, %g4		! 64-bit stack OK?
	bz,pt	%icc, 1f
	cmp	%g4, 0x1		! Must end in 0b001
	be,pt	%icc, 1f
	 rdpr	%wstate, %g7
	cmp	%g7, WSTATE_KERN
	bnz,pt	%icc, 1f		! User stack -- we'll blow it away
	 nop
	sethi	%hi(PANICSTACK), %sp
	LDPTR	[%sp + %lo(PANICSTACK)], %sp
	add	%sp, -CC64FSZ-STKB, %sp	
1:
#endif
	rdpr	%tt, %g4
	rdpr	%tstate, %g1
	rdpr	%tpc, %g2
	rdpr	%tnpc, %g3

	TRAP_SETUP(-CC64FSZ-TF_SIZE)
Lslowtrap_reenter:
	stx	%g1, [%sp + CC64FSZ + STKB + TF_TSTATE]
	mov	%g4, %o1		! (type)
	stx	%g2, [%sp + CC64FSZ + STKB + TF_PC]
	rd	%y, %g5
	stx	%g3, [%sp + CC64FSZ + STKB + TF_NPC]
	mov	%g1, %o3		! (pstate)
	st	%g5, [%sp + CC64FSZ + STKB + TF_Y]
	mov	%g2, %o2		! (pc)
	sth	%o1, [%sp + CC64FSZ + STKB + TF_TT]! debug

	wrpr	%g0, PSTATE_KERN, %pstate		! Get back to normal globals
	stx	%g1, [%sp + CC64FSZ + STKB + TF_G + (1*8)]
	stx	%g2, [%sp + CC64FSZ + STKB + TF_G + (2*8)]
	add	%sp, CC64FSZ + STKB, %o0		! (&tf)
	stx	%g3, [%sp + CC64FSZ + STKB + TF_G + (3*8)]
	stx	%g4, [%sp + CC64FSZ + STKB + TF_G + (4*8)]
	stx	%g5, [%sp + CC64FSZ + STKB + TF_G + (5*8)]
	rdpr	%pil, %g5
	stx	%g6, [%sp + CC64FSZ + STKB + TF_G + (6*8)]
	stx	%g7, [%sp + CC64FSZ + STKB + TF_G + (7*8)]
	stb	%g5, [%sp + CC64FSZ + STKB + TF_PIL]
	stb	%g5, [%sp + CC64FSZ + STKB + TF_OLDPIL]
	/*
	 * Phew, ready to enable traps and call C code.
	 */
	rdpr	%tl, %g1
	dec	%g1
	movrlz	%g1, %g0, %g1
	CHKPT(%g2,%g3,0x24)
	wrpr	%g0, %g1, %tl		! Revert to kernel mode
	!! In the EMBEDANY memory model %g4 points to the start of the data segment.
	!! In our case we need to clear it before calling any C-code
	clr	%g4

	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore default ASI
	wrpr	%g0, PSTATE_INTR, %pstate	! traps on again
	call	_C_LABEL(trap)			! trap(tf, type, pc, pstate)
	 nop

	CHKPT(%o1,%o2,3)
	ba,a,pt	%icc, return_from_trap
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
	sethi	%hi(EINTSTACK-STKB), %g5
	sethi	%hi(EINTSTACK-INTSTACK), %g7
	or	%g5, %lo(EINTSTACK-STKB), %g5
	dec	%g7
	sub	%g5, %g6, %g5
	sethi	%hi(CPCB), %g6
	andncc	%g5, %g7, %g0
	bnz,pt	%xcc, Lslowtrap_reenter
	 LDPTR	[%g6 + %lo(CPCB)], %g7
	set	USPACE-CC64FSZ-TF_SIZE-STKB, %g5
	add	%g7, %g5, %g6
	SET_SP_REDZONE(%g7, %g5)
#ifdef DEBUG
	stx	%g1, [%g6 + CC64FSZ + STKB + TF_FAULT]		! Generate a new trapframe
#endif
	stx	%i0, [%g6 + CC64FSZ + STKB + TF_O + (0*8)]	!	but don't bother with
	stx	%i1, [%g6 + CC64FSZ + STKB + TF_O + (1*8)]	!	locals and ins
	stx	%i2, [%g6 + CC64FSZ + STKB + TF_O + (2*8)]
	stx	%i3, [%g6 + CC64FSZ + STKB + TF_O + (3*8)]
	stx	%i4, [%g6 + CC64FSZ + STKB + TF_O + (4*8)]
	stx	%i5, [%g6 + CC64FSZ + STKB + TF_O + (5*8)]
	stx	%i6, [%g6 + CC64FSZ + STKB + TF_O + (6*8)]
	stx	%i7, [%g6 + CC64FSZ + STKB + TF_O + (7*8)]
#ifdef DEBUG
	ldx	[%sp + CC64FSZ + STKB + TF_I + (0*8)], %l0	! Copy over the rest of the regs
	ldx	[%sp + CC64FSZ + STKB + TF_I + (1*8)], %l1	! But just dirty the locals
	ldx	[%sp + CC64FSZ + STKB + TF_I + (2*8)], %l2
	ldx	[%sp + CC64FSZ + STKB + TF_I + (3*8)], %l3
	ldx	[%sp + CC64FSZ + STKB + TF_I + (4*8)], %l4
	ldx	[%sp + CC64FSZ + STKB + TF_I + (5*8)], %l5
	ldx	[%sp + CC64FSZ + STKB + TF_I + (6*8)], %l6
	ldx	[%sp + CC64FSZ + STKB + TF_I + (7*8)], %l7
	stx	%l0, [%g6 + CC64FSZ + STKB + TF_I + (0*8)]
	stx	%l1, [%g6 + CC64FSZ + STKB + TF_I + (1*8)]
	stx	%l2, [%g6 + CC64FSZ + STKB + TF_I + (2*8)]
	stx	%l3, [%g6 + CC64FSZ + STKB + TF_I + (3*8)]
	stx	%l4, [%g6 + CC64FSZ + STKB + TF_I + (4*8)]
	stx	%l5, [%g6 + CC64FSZ + STKB + TF_I + (5*8)]
	stx	%l6, [%g6 + CC64FSZ + STKB + TF_I + (6*8)]
	stx	%l7, [%g6 + CC64FSZ + STKB + TF_I + (7*8)]
	ldx	[%sp + CC64FSZ + STKB + TF_L + (0*8)], %l0
	ldx	[%sp + CC64FSZ + STKB + TF_L + (1*8)], %l1
	ldx	[%sp + CC64FSZ + STKB + TF_L + (2*8)], %l2
	ldx	[%sp + CC64FSZ + STKB + TF_L + (3*8)], %l3
	ldx	[%sp + CC64FSZ + STKB + TF_L + (4*8)], %l4
	ldx	[%sp + CC64FSZ + STKB + TF_L + (5*8)], %l5
	ldx	[%sp + CC64FSZ + STKB + TF_L + (6*8)], %l6
	ldx	[%sp + CC64FSZ + STKB + TF_L + (7*8)], %l7
	stx	%l0, [%g6 + CC64FSZ + STKB + TF_L + (0*8)]
	stx	%l1, [%g6 + CC64FSZ + STKB + TF_L + (1*8)]
	stx	%l2, [%g6 + CC64FSZ + STKB + TF_L + (2*8)]
	stx	%l3, [%g6 + CC64FSZ + STKB + TF_L + (3*8)]
	stx	%l4, [%g6 + CC64FSZ + STKB + TF_L + (4*8)]
	stx	%l5, [%g6 + CC64FSZ + STKB + TF_L + (5*8)]
	stx	%l6, [%g6 + CC64FSZ + STKB + TF_L + (6*8)]
	stx	%l7, [%g6 + CC64FSZ + STKB + TF_L + (7*8)]
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
XXX ddb_regs is now ddb-regp and is a pointer not a symbol.
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
ENTRY_NOPROFILE(kgdb_trap_glue)
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

#ifdef NOTDEF_DEBUG
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
#ifdef NOTDEF_DEBUG
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
#ifdef NOTDEF_DEBUG
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
	sethi	%hi(CPCB), %l1
	LDPTR	[%l1 + %lo(CPCB)], %l1
	and	%l0, 31, %l0		! CWP = %psr & 31;
!	st	%l0, [%l1 + PCB_WIM]	! cpcb->pcb_wim = CWP;
	save	%g0, %g0, %g0		! back to window to reload
!	LOADWIN(%sp)
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
	TRAP_SETUP(-CC64FSZ-TF_SIZE)

#ifdef DEBUG
	rdpr	%tt, %o1	! debug
	sth	%o1, [%sp + CC64FSZ + STKB + TF_TT]! debug
#endif

	wrpr	%g0, PSTATE_KERN, %pstate	! Get back to normal globals
	stx	%g1, [%sp + CC64FSZ + STKB + TF_G + ( 1*8)]
	mov	%g1, %o1			! code
	rdpr	%tpc, %o2			! (pc)
	stx	%g2, [%sp + CC64FSZ + STKB + TF_G + ( 2*8)]
	rdpr	%tstate, %g1
	stx	%g3, [%sp + CC64FSZ + STKB + TF_G + ( 3*8)]
	rdpr	%tnpc, %o3
	stx	%g4, [%sp + CC64FSZ + STKB + TF_G + ( 4*8)]
	rd	%y, %o4
	stx	%g5, [%sp + CC64FSZ + STKB + TF_G + ( 5*8)]
	stx	%g6, [%sp + CC64FSZ + STKB + TF_G + ( 6*8)]
	CHKPT(%g5,%g6,0x31)
	wrpr	%g0, 0, %tl			! return to tl=0
	stx	%g7, [%sp + CC64FSZ + STKB + TF_G + ( 7*8)]
	add	%sp, CC64FSZ + STKB, %o0	! (&tf)

	stx	%g1, [%sp + CC64FSZ + STKB + TF_TSTATE]
	stx	%o2, [%sp + CC64FSZ + STKB + TF_PC]
	stx	%o3, [%sp + CC64FSZ + STKB + TF_NPC]
	st	%o4, [%sp + CC64FSZ + STKB + TF_Y]

	rdpr	%pil, %g5
	stb	%g5, [%sp + CC64FSZ + STKB + TF_PIL]
	stb	%g5, [%sp + CC64FSZ + STKB + TF_OLDPIL]

	!! In the EMBEDANY memory model %g4 points to the start of the data segment.
	!! In our case we need to clear it before calling any C-code
	clr	%g4
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi	! Restore default ASI

	sethi	%hi(CURLWP), %l1
	LDPTR	[%l1 + %lo(CURLWP)], %l1
	LDPTR	[%l1 + L_PROC], %l1		! now %l1 points to p
	LDPTR	[%l1 + P_MD_SYSCALL], %l1
	call	%l1
	 wrpr	%g0, PSTATE_INTR, %pstate	! turn on interrupts

	/* see `proc_trampoline' for the reason for this label */
return_from_syscall:
	wrpr	%g0, PSTATE_KERN, %pstate	! Disable intterrupts
	CHKPT(%o1,%o2,0x32)
	wrpr	%g0, 0, %tl			! Return to tl==0
	CHKPT(%o1,%o2,4)
	ba,a,pt	%icc, return_from_trap
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
 * To simplify life all we do here is issue an appropriate softint.
 *
 * Note:	It is impossible to identify or change a device's
 *		interrupt number until it is probed.  That's the
 *		purpose for all the funny interrupt acknowledge
 *		code.
 *
 */

/*
 * Vectored interrupts:
 *
 * When an interrupt comes in, interrupt_vector uses the interrupt
 * vector number to lookup the appropriate intrhand from the intrlev
 * array.  It then looks up the interrupt level from the intrhand
 * structure.  It uses the level to index the intrpending array,
 * which is 8 slots for each possible interrupt level (so we can
 * shift instead of multiply for address calculation).  It hunts for
 * any available slot at that level.  Available slots are NULL.
 *
 * Then interrupt_vector uses the interrupt level in the intrhand
 * to issue a softint of the appropriate level.  The softint handler
 * figures out what level interrupt it's handling and pulls the first
 * intrhand pointer out of the intrpending array for that interrupt
 * level, puts a NULL in its place, clears the interrupt generator,
 * and invokes the interrupt handler.
 */

	.data
	.globl	intrpending
intrpending:
	.space	16 * 8 * PTRSZ, -1

#ifdef DEBUG
#define INTRDEBUG_VECTOR	0x1
#define INTRDEBUG_LEVEL		0x2
#define INTRDEBUG_FUNC		0x4
#define INTRDEBUG_SPUR		0x8
	.globl	_C_LABEL(intrdebug)
_C_LABEL(intrdebug):	.word 0x0
/*
 * Note: we use the local label `97' to branch forward to, to skip
 * actual debugging code following a `intrdebug' bit test.
 */
#endif
	.text
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
	membar	#Sync
	stxa	%g0, [%g0] ASI_IRSR	! Ack IRQ
	membar	#Sync			! Should not be needed due to retry

#if KTR_COMPILE & KTR_INTR
	CATR(KTR_TRAP, "interrupt_vector: tl %d ASI_IRSR %p ASI_IRDR %p",
		 %g3, %g5, %g6, 10, 11, 12)
	rdpr	%tl, %g5
	stx	%g5, [%g3 + KTR_PARM1]
	stx	%g1, [%g3 + KTR_PARM2]
	stx	%g2, [%g3 + KTR_PARM3]
12:
#endif

	btst	IRSR_BUSY, %g1
	bz,pn	%icc, 3f		! spurious interrupt
	 sllx	%g2, PTRSHFT, %g5	! Calculate entry number

	brnz,pt	%g2, Lsoftint_regular	! interrupt #0 is a fast cross-call
	 cmp	%g2, MAXINTNUM

	mov	IRDR_1H, %g1
	ldxa	[%g1] ASI_IRDR, %g1	! Get IPI handler address
	brz,pn  %g1, ret_from_intr_vector
	 mov	IRDR_2H, %g2

	jmpl	%g1, %g0
	 ldxa	[%g2] ASI_IRDR, %g2	! Get IPI handler argument

Lsoftint_regular:
	sethi	%hi(_C_LABEL(intrlev)), %g3
	bgeu,pn	%xcc, 3f
	 or	%g3, %lo(_C_LABEL(intrlev)), %g3
	LDPTR	[%g3 + %g5], %g5	! We have a pointer to the handler
	brz,pn	%g5, 3f			! NULL means it isn't registered yet.  Skip it.
	 nop

setup_sparcintr:
	LDPTR	[%g5+IH_PEND], %g6	! Read pending flag
	brnz,pn	%g6, ret_from_intr_vector ! Skip it if it's running
	 ldub	[%g5+IH_PIL], %g6	! Read interrupt mask
	sethi	%hi(intrpending), %g1
	sll	%g6, PTRSHFT+3, %g3	! Find start of table for this IPL
	or	%g1, %lo(intrpending), %g1
	 add	%g1, %g3, %g1
1:
	LDPTR	[%g1], %g3		! Load list head
	STPTR	%g3, [%g5+IH_PEND]	! Link our intrhand node in
	mov	%g5, %g7
	CASPTR	[%g1] ASI_N, %g3, %g7
	cmp	%g7, %g3		! Did it work?
	bne,pn	%xcc, 1b		! No, try again
	 nop
2:
	mov	1, %g7
	sll	%g7, %g6, %g6
	wr	%g6, 0, SET_SOFTINT	! Invoke a softint

ret_from_intr_vector:
#if KTR_COMPILE & KTR_INTR
	CATR(KTR_TRAP, "ret_from_intr_vector: tl %d, tstate %p, tpc %p",
		 %g3, %g4, %g5, 10, 11, 12)
	rdpr	%tl, %g5
	stx	%g5, [%g3 + KTR_PARM1]
	rdpr	%tstate, %g5
	stx	%g5, [%g3 + KTR_PARM2]
	rdpr	%tpc, %g5
	stx	%g5, [%g3 + KTR_PARM3]
12:
#endif
	retry
	NOTREACHED

3:
#ifdef NOT_DEBUG
	set	_C_LABEL(intrdebug), %g7
	ld	[%g7], %g7
	btst	INTRDEBUG_SPUR, %g7
	bz,pt	%icc, 97f
	 nop
#endif
#if 1
	STACKFRAME(-CC64FSZ)		! Get a clean register window
	LOAD_ASCIZ(%o0, "interrupt_vector: spurious vector %lx at pil %d\r\n")
	mov	%g2, %o1
	GLOBTOLOC
	clr	%g4
	call	prom_printf
	 rdpr	%pil, %o2
	LOCTOGLOB
	restore
97:
#endif
	ba,a	ret_from_intr_vector
	 nop				! XXX spitfire bug?

#if defined(MULTIPROCESSOR)
/*
 * IPI handler to halt the CPU.  Just calls the C vector.
 * void sparc64_ipi_halt(void *);
 */
ENTRY(sparc64_ipi_halt)
	call	_C_LABEL(sparc64_ipi_halt_thiscpu)
	 clr	%g4
	sir

/*
 * IPI handler to pause the CPU.  We just trap to the debugger if it
 * is configured, otherwise just return.
 */
ENTRY(sparc64_ipi_pause)
#if defined(DDB)
sparc64_ipi_pause_trap_point:
	ta	1
	 nop
#endif
	ba,a	ret_from_intr_vector
	 nop

/*
 * IPI handler to flush single pte.
 * void sparc64_ipi_flush_pte(void *);
 *
 * On Entry:
 *
 * %g2	- pointer to 'ipi_tlb_args' structure
 */
ENTRY(sparc64_ipi_flush_pte)
#if  KTR_COMPILE & KTR_PMAP
	CATR(KTR_TRAP, "sparc64_ipi_flush_pte:",
		 %g1, %g3, %g4, 10, 11, 12)
12:
#endif
#if 0
	! save %o0 - %o5
	mov	%o0, %g1
	mov	%o1, %g3
	mov	%o2, %g4
	mov	%o3, %g5
	mov	%o4, %g6
	mov	%o5, %g7
	LDPTR	[%g2 + ITA_VADDR], %o0
	call	sp_tlb_flush_pte
	 ld	[%g2 + ITA_CTX], %o1
	! restore %o0 - %o5
	mov	%g1, %o0
	mov	%g3, %o1
	mov	%g4, %o2
	mov	%g5, %o3
	mov	%g6, %o4 
	mov	%g7, %o5
#endif
	 
	ba,a	ret_from_intr_vector
	 restore

/*
 * IPI handler to flush single context.
 * void sparc64_ipi_flush_ctx(void *);
 *
 * On Entry:
 *
 * %g2	- pointer to 'ipi_tlb_args' structure
 */
ENTRY(sparc64_ipi_flush_ctx)
#if KTR_COMPILE & KTR_PMAP
	CATR(KTR_TRAP, "sparc64_ipi_flush_ctx:",
		 %g1, %g3, %g4, 10, 11, 12)
12:
#endif
#if 0
	! save %o0 - %o5
	mov	%o0, %g1
	mov	%o1, %g3
	mov	%o2, %g4
	mov	%o3, %g5
	mov	%o4, %g6
	call	sp_tlb_flush_ctx
	 ld	[%g2 + ITA_CTX], %o0
	! restore %o0 - %o5
	mov	%g1, %o0
	mov	%g3, %o1
	mov	%g4, %o2
	mov	%g5, %o3
	mov	%g6, %o4 
	mov	%g7, %o5
#endif
	 
	ba,a	ret_from_intr_vector
	 restore

/*
 * IPI handler to flush the whole TLB.
 * void sparc64_ipi_flush_all(void *);
 */
ENTRY(sparc64_ipi_flush_all)
#if KTR_COMPILE & KTR_PMAP
	CATR(KTR_TRAP, "sparc64_ipi_flush_all: %p %p",
		 %g1, %g4, %g5, 10, 11, 12)
	stx	%g3, [%g1 + KTR_PARM1]
	stx	%g2, [%g1 + KTR_PARM2]
12:
#endif

	set	(63 * 8), %g1				! last TLB entry
	membar	#Sync

	! %g1 = loop counter
	! %g2 = TLB data value

0:
	ldxa	[%g1] ASI_DMMU_TLB_DATA, %g2		! fetch the TLB data
	btst	TTE_L, %g2				! locked entry?
	bnz,pt	%icc, 1f				! if so, skip
	 nop

	stxa	%g0, [%g1] ASI_DMMU_TLB_DATA		! zap it
	membar	#Sync

1:
	dec	8, %g1
	brgz,pt %g1, 0b					! loop over all entries
	 nop

	set	(63 * 8), %g1				! last TLB entry

0:
	ldxa	[%g1] ASI_IMMU_TLB_DATA, %g2		! fetch the TLB data
	btst	TTE_L, %g2				! locked entry?
	bnz,pt	%icc, 1f				! if so, skip
	 nop

	stxa	%g0, [%g1] ASI_IMMU_TLB_DATA		! zap it
	membar	#Sync

1:
	dec	8, %g1
	brgz,pt %g1, 0b					! loop over all entries
	 nop

	sethi	%hi(KERNBASE), %g4
	membar	#Sync
	flush	%g4

	ba,a	ret_from_intr_vector
	 nop

/*
 * Secondary CPU bootstrap code.
 */
	.text
	.align 32
1:	rd	%pc, %l0
	ldx	[%l0 + (4f-1b)], %l1
	add	%l0, (6f-1b), %l2
	clr	%l3
2:	cmp	%l3, %l1
	be	%xcc, 3f
	 nop
	ldx	[%l2 + TTE_VPN], %l4
	ldx	[%l2 + TTE_DATA], %l5
	wr	%g0, ASI_DMMU, %asi
	stxa	%l4, [%g0 + TLB_TAG_ACCESS] %asi
	stxa	%l5, [%g0] ASI_DMMU_DATA_IN
	wr	%g0, ASI_IMMU, %asi
	stxa	%l4, [%g0 + TLB_TAG_ACCESS] %asi
	stxa	%l5, [%g0] ASI_IMMU_DATA_IN
	membar	#Sync
	flush	%l4
	add	%l2, PTE_SIZE, %l2
	add	%l3, 1, %l3
	ba	%xcc, 2b
	 nop
3:	ldx	[%l0 + (5f-1b)], %l1
	ldx	[%l0 + (7f-1b)], %g2	! Load cpu_info address.
	jmpl	%l1, %g0
	 nop

	.align PTRSZ
4:	ULONG	0x0
5:	ULONG	0x0
7:	ULONG	0x0
6:

#define DATA(name) \
        .data ; \
        .align PTRSZ ; \
        .globl  name ; \
name:

DATA(mp_tramp_code)
	POINTER	1b
DATA(mp_tramp_code_len)
	ULONG	6b-1b
DATA(mp_tramp_tlb_slots)
	ULONG	4b-1b
DATA(mp_tramp_func)
	ULONG	5b-1b
DATA(mp_tramp_ci)
	ULONG	7b-1b

	.text
	.align 32
#endif				/* MULTIPROCESSOR */

/*
 * Ultra1 and Ultra2 CPUs use soft interrupts for everything.  What we do
 * on a soft interrupt, is we should check which bits in ASR_SOFTINT(0x16)
 * are set, handle those interrupts, then clear them by setting the
 * appropriate bits in ASR_CLEAR_SOFTINT(0x15).
 *
 * We have an array of 8 interrupt vector slots for each of 15 interrupt
 * levels.  If a vectored interrupt can be dispatched, the dispatch
 * routine will place a pointer to an intrhand structure in one of
 * the slots.  The interrupt handler will go through the list to look
 * for an interrupt to dispatch.  If it finds one it will pull it off
 * the list, free the entry, and call the handler.  The code is like
 * this:
 *
 *	for (i=0; i<8; i++)
 *		if (ih = intrpending[intlev][i]) {
 *			intrpending[intlev][i] = NULL;
 *			if ((*ih->ih_fun)(ih->ih_arg ? ih->ih_arg : &frame))
 *				return;
 *			strayintr(&frame);
 *			return;
 *		}
 *
 * Otherwise we go back to the old style of polled interrupts.
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

ENTRY_NOPROFILE(sparc_interrupt)
#ifdef TRAPS_USE_IG
	! This is for interrupt debugging
	wrpr	%g0, PSTATE_KERN|PSTATE_IG, %pstate	! DEBUG
#endif
	/*
	 * If this is a %tick softint, clear it then call interrupt_vector.
	 */
	rd	SOFTINT, %g1
	btst	1, %g1
	bz,pt	%icc, 0f
	 set	_C_LABEL(intrlev), %g3
	wr	%g0, 1, CLEAR_SOFTINT
	DLFLUSH(%g3, %g2)
	ba,pt	%icc, setup_sparcintr
	 LDPTR	[%g3 + PTRSZ], %g5	! intrlev[1] is reserved for %tick intr.
0:
#ifdef TRAPSTATS
	sethi	%hi(_C_LABEL(kintrcnt)), %g1
	sethi	%hi(_C_LABEL(uintrcnt)), %g2
	or	%g1, %lo(_C_LABEL(kintrcnt)), %g1
	or	%g1, %lo(_C_LABEL(uintrcnt)), %g2
	rdpr	%tl, %g3
	dec	%g3
	movrz	%g3, %g2, %g1
	lduw	[%g1], %g2
	inc	%g2
	stw	%g2, [%g1]
	/* See if we're on the interrupt stack already. */
	set	EINTSTACK, %g2
	set	(EINTSTACK-INTSTACK), %g1
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
	INTR_SETUP(-CC64FSZ-TF_SIZE)
	! Switch to normal globals so we can save them
	wrpr	%g0, PSTATE_KERN, %pstate
	stx	%g1, [%sp + CC64FSZ + STKB + TF_G + ( 1*8)]
	stx	%g2, [%sp + CC64FSZ + STKB + TF_G + ( 2*8)]
	stx	%g3, [%sp + CC64FSZ + STKB + TF_G + ( 3*8)]
	stx	%g4, [%sp + CC64FSZ + STKB + TF_G + ( 4*8)]
	stx	%g5, [%sp + CC64FSZ + STKB + TF_G + ( 5*8)]
	stx	%g6, [%sp + CC64FSZ + STKB + TF_G + ( 6*8)]
	stx	%g7, [%sp + CC64FSZ + STKB + TF_G + ( 7*8)]

	/*
	 * In the EMBEDANY memory model %g4 points to the start of the
	 * data segment.  In our case we need to clear it before calling
	 * any C-code.
	 */
	clr	%g4

	flushw			! Do not remove this insn -- causes interrupt loss
	rd	%y, %l6
	INCR(_C_LABEL(uvmexp)+V_INTR)	! cnt.v_intr++; (clobbers %o0,%o1,%o2)
	rdpr	%tt, %l5		! Find out our current IPL
	rdpr	%tstate, %l0
	rdpr	%tpc, %l1
	rdpr	%tnpc, %l2
	rdpr	%tl, %l3		! Dump our trap frame now we have taken the IRQ
	stw	%l6, [%sp + CC64FSZ + STKB + TF_Y]	! Silly, but we need to save this for rft
	dec	%l3
	CHKPT(%l4,%l7,0x26)
	wrpr	%g0, %l3, %tl
	sth	%l5, [%sp + CC64FSZ + STKB + TF_TT]! debug
	stx	%l0, [%sp + CC64FSZ + STKB + TF_TSTATE]	! set up intrframe/clockframe
	stx	%l1, [%sp + CC64FSZ + STKB + TF_PC]
	btst	TSTATE_PRIV, %l0		! User mode?
	stx	%l2, [%sp + CC64FSZ + STKB + TF_NPC]
	
	sub	%l5, 0x40, %l6			! Convert to interrupt level
	sethi	%hi(_C_LABEL(intr_evcnts)), %l4
	stb	%l6, [%sp + CC64FSZ + STKB + TF_PIL]	! set up intrframe/clockframe
	rdpr	%pil, %o1
	mulx	%l6, EVC_SIZE, %l3
	or	%l4, %lo(_C_LABEL(intr_evcnts)), %l4	! intrcnt[intlev]++;
	stb	%o1, [%sp + CC64FSZ + STKB + TF_OLDPIL]	! old %pil
	ldx	[%l4 + %l3], %o0
	add	%l4, %l3, %l4
	clr	%l5			! Zero handled count
	inc	%o0	
	mov	1, %l3			! Ack softint
	stx	%o0, [%l4]
	sll	%l3, %l6, %l3		! Generate IRQ mask
	
	wrpr	%l6, %pil

sparc_intr_retry:
	wr	%l3, 0, CLEAR_SOFTINT	! (don't clear possible %tick IRQ)
	sll	%l6, PTRSHFT+3, %l2
	sethi	%hi(intrpending), %l4
	or	%l4, %lo(intrpending), %l4
	mov	8, %l7
	add	%l2, %l4, %l4

1:
	membar	#StoreLoad		! Make sure any failed casxa insns complete
	LDPTR	[%l4], %l2		! Check a slot
	cmp	%l2, -1
	beq,pn	CCCR, intrcmplt		! Empty list?
	 mov	-1, %l7
	membar	#LoadStore
	CASPTR	[%l4] ASI_N, %l2, %l7	! Grab the entire list
	cmp	%l7, %l2
	bne,pn	%icc, 1b
	 add	%sp, CC64FSZ+STKB, %o2	! tf = %sp + CC64FSZ + STKB
2:
	LDPTR	[%l2 + IH_PEND], %l7	! save ih->ih_pending
	membar	#LoadStore
	STPTR	%g0, [%l2 + IH_PEND]	! Clear pending flag
	membar	#Sync
	LDPTR	[%l2 + IH_FUN], %o4	! ih->ih_fun
	LDPTR	[%l2 + IH_ARG], %o0	! ih->ih_arg

	wrpr	%g0, PSTATE_INTR, %pstate	! Reenable interrupts
	jmpl	%o4, %o7		! handled = (*ih->ih_fun)(...)
	 movrz	%o0, %o2, %o0		! arg = (arg == 0) ? arg : tf
	wrpr	%g0, PSTATE_KERN, %pstate	! Disable interrupts
	LDPTR	[%l2 + IH_CLR], %l1
	membar	#Sync

	brz,pn	%l1, 0f
	 add	%l5, %o0, %l5
	stx	%g0, [%l1]		! Clear intr source
	membar	#Sync			! Should not be needed
0:
	cmp	%l7, -1
	bne,pn	CCCR, 2b		! 'Nother?
	 mov	%l7, %l2

intrcmplt:
	/*
	 * Re-read SOFTINT to see if any new  pending interrupts
	 * at this level.
	 */
	mov	1, %l3			! Ack softint
	rd	SOFTINT, %l7		! %l5 contains #intr handled.
	sll	%l3, %l6, %l3		! Generate IRQ mask
	btst	%l3, %l7		! leave mask in %l3 for retry code
	bnz,pn	%icc, sparc_intr_retry
	 mov	1, %l5			! initialize intr count for next run

#ifdef DEBUG
	set	_C_LABEL(intrdebug), %o2
	ld	[%o2], %o2
	btst	INTRDEBUG_FUNC, %o2
	bz,a,pt	%icc, 97f
	 nop

	STACKFRAME(-CC64FSZ)		! Get a clean register window
	LOAD_ASCIZ(%o0, "sparc_interrupt:  done\r\n")
	GLOBTOLOC
	call	prom_printf
	 nop
	LOCTOGLOB
	restore
97:
#endif

	ldub	[%sp + CC64FSZ + STKB + TF_OLDPIL], %l3	! restore old %pil
	wrpr	%l3, 0, %pil

	CHKPT(%o1,%o2,5)
	ba,a,pt	%icc, return_from_trap
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

	.globl	return_from_trap, rft_kernel, rft_user
	.globl	softtrap, slowtrap
	.globl	syscall


/*
 * Various return-from-trap routines (see return_from_trap).
 */

/*
 * Return from trap.
 * registers are:
 *
 *	[%sp + CC64FSZ + STKB] => trap frame
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
#ifdef DEBUG
	!! Make sure we don't have pc == npc == 0 or we suck.
	ldx	[%sp + CC64FSZ + STKB + TF_PC], %g2
	ldx	[%sp + CC64FSZ + STKB + TF_NPC], %g3
	orcc	%g2, %g3, %g0
	tz	%icc, 1
#endif

#if KTR_COMPILE & KTR_TRAP
	CATR(KTR_TRAP, "rft: sp=%p pc=%p npc=%p tstate=%p",
		 %g2, %g3, %g4, 10, 11, 12)
	stx	%i6, [%g2 + KTR_PARM1]
	ldx	[%sp + CC64FSZ + STKB + TF_PC], %g3
	stx	%g3, [%g2 + KTR_PARM2]
	ldx	[%sp + CC64FSZ + STKB + TF_NPC], %g3
	stx	%g3, [%g2 + KTR_PARM3]
	ldx	[%sp + CC64FSZ + STKB + TF_TSTATE], %g3
	stx	%g3, [%g2 + KTR_PARM4]
12:
#endif

	!!
	!! We'll make sure we flush our pcb here, rather than later.
	!!
	ldx	[%sp + CC64FSZ + STKB + TF_TSTATE], %g1
	btst	TSTATE_PRIV, %g1			! returning to userland?

	!!
	!! Let all pending interrupts drain before returning to userland
	!!
	bnz,pn	%icc, 1f				! Returning to userland?
	 nop
	wrpr	%g0, PSTATE_INTR, %pstate
	wrpr	%g0, %g0, %pil				! Lower IPL
1:
	wrpr	%g0, PSTATE_KERN, %pstate		! Make sure we have normal globals & no IRQs

	/* Restore normal globals */
	ldx	[%sp + CC64FSZ + STKB + TF_G + (1*8)], %g1
	ldx	[%sp + CC64FSZ + STKB + TF_G + (2*8)], %g2
	ldx	[%sp + CC64FSZ + STKB + TF_G + (3*8)], %g3
	ldx	[%sp + CC64FSZ + STKB + TF_G + (4*8)], %g4
	ldx	[%sp + CC64FSZ + STKB + TF_G + (5*8)], %g5
	ldx	[%sp + CC64FSZ + STKB + TF_G + (6*8)], %g6
	ldx	[%sp + CC64FSZ + STKB + TF_G + (7*8)], %g7
	/* Switch to alternate globals and load outs */
	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate
#ifdef TRAPS_USE_IG
	wrpr	%g0, PSTATE_KERN|PSTATE_IG, %pstate	! DEBUG
#endif
	ldx	[%sp + CC64FSZ + STKB + TF_O + (0*8)], %i0
	ldx	[%sp + CC64FSZ + STKB + TF_O + (1*8)], %i1
	ldx	[%sp + CC64FSZ + STKB + TF_O + (2*8)], %i2
	ldx	[%sp + CC64FSZ + STKB + TF_O + (3*8)], %i3
	ldx	[%sp + CC64FSZ + STKB + TF_O + (4*8)], %i4
	ldx	[%sp + CC64FSZ + STKB + TF_O + (5*8)], %i5
	ldx	[%sp + CC64FSZ + STKB + TF_O + (6*8)], %i6
	ldx	[%sp + CC64FSZ + STKB + TF_O + (7*8)], %i7
	/* Now load trap registers into alternate globals */
	ld	[%sp + CC64FSZ + STKB + TF_Y], %g4
	ldx	[%sp + CC64FSZ + STKB + TF_TSTATE], %g1		! load new values
	wr	%g4, 0, %y
	ldx	[%sp + CC64FSZ + STKB + TF_PC], %g2
	ldx	[%sp + CC64FSZ + STKB + TF_NPC], %g3

#ifdef NOTDEF_DEBUG
	ldub	[%sp + CC64FSZ + STKB + TF_PIL], %g5		! restore %pil
	wrpr	%g5, %pil				! DEBUG
#endif

	/* Returning to user mode or kernel mode? */
	btst	TSTATE_PRIV, %g1		! returning to userland?
	CHKPT(%g4, %g7, 6)
	bz,pt	%icc, rft_user
	 sethi	%hi(CPUINFO_VA+CI_WANT_AST), %g7	! first instr of rft_user

/*
 * Return from trap, to kernel.
 *
 * We will assume, for the moment, that all kernel traps are properly stacked
 * in the trap registers, so all we have to do is insert the (possibly modified)
 * register values into the trap registers then do a retry.
 *
 */
rft_kernel:
	rdpr	%tl, %g4				! Grab a set of trap registers
	inc	%g4
	wrpr	%g4, %g0, %tl
	wrpr	%g3, 0, %tnpc
	wrpr	%g2, 0, %tpc
	wrpr	%g1, 0, %tstate
	CHKPT(%g1,%g2,7)
	restore
	CHKPT(%g1,%g2,0)			! Clear this out
	rdpr	%tstate, %g1			! Since we may have trapped our regs may be toast
	rdpr	%cwp, %g2
	andn	%g1, CWP, %g1
	wrpr	%g1, %g2, %tstate		! Put %cwp in %tstate
	CLRTT
#ifdef TRAPSTATS
	rdpr	%tl, %g2
	set	_C_LABEL(rftkcnt), %g1
	sllx	%g2, 2, %g2
	add	%g1, %g2, %g1
	lduw	[%g1], %g2
	inc	%g2
	stw	%g2, [%g1]
#endif
#if	0
	wrpr	%g0, 0, %cleanwin	! DEBUG
#endif
#if defined(DDB) && defined(MULTIPROCESSOR)
	set	sparc64_ipi_pause_trap_point, %g1
	rdpr	%tpc, %g2
	cmp	%g1, %g2
	bne,pt	%icc, 0f
	 nop
	done
0:
#endif
	retry
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
	.data
rft_wcnt:	.word 0
	.text

rft_user:
!	sethi	%hi(CPUINFO_VA+CI_WANT_AST), %g7	! (done above)
	lduw	[%g7 + %lo(CPUINFO_VA+CI_WANT_AST)], %g7! want AST trap?
	brnz,pn	%g7, softtrap			! yes, re-enter trap with type T_AST
	 mov	T_AST, %g4

	CHKPT(%g4,%g7,8)
#ifdef NOTDEF_DEBUG
	sethi	%hi(CPCB), %g4
	LDPTR	[%g4 + %lo(CPCB)], %g4
	ldub	[%g4 + PCB_NSAVED], %g4		! nsaved
	brz,pt	%g4, 2f		! Only print if nsaved <> 0
	 nop

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
1:
	.data
	.asciz	"rft_user: nsaved=%x pc=%d ctx=%x sp=%x npc=%p\n"
	_ALIGN
	.text
#endif

	/*
	 * NB: only need to do this after a cache miss
	 */
#ifdef TRAPSTATS
	set	_C_LABEL(rftucnt), %g6
	lduw	[%g6], %g7
	inc	%g7
	stw	%g7, [%g6]
#endif
	/*
	 * Now check to see if any regs are saved in the pcb and restore them.
	 *
	 * Here we need to undo the damage caused by switching to a kernel 
	 * stack.
	 *
	 * We will use alternate globals %g4..%g7 because %g1..%g3 are used
	 * by the data fault trap handlers and we don't want possible conflict.
	 */

	sethi	%hi(CPCB), %g6
	rdpr	%otherwin, %g7			! restore register window controls
#ifdef DEBUG
	rdpr	%canrestore, %g5		! DEBUG
	tst	%g5				! DEBUG
	tnz	%icc, 1; nop			! DEBUG
!	mov	%g0, %g5			! There should be *NO* %canrestore
	add	%g7, %g5, %g7			! DEBUG
#endif
	wrpr	%g0, %g7, %canrestore
	LDPTR	[%g6 + %lo(CPCB)], %g6
	wrpr	%g0, 0, %otherwin

	CHKPT(%g4,%g7,9)
	ldub	[%g6 + PCB_NSAVED], %g7		! Any saved reg windows?
	wrpr	%g0, WSTATE_USER, %wstate	! Need to know where our sp points

#ifdef DEBUG
	set	rft_wcnt, %g4	! Keep track of all the windows we restored
	stw	%g7, [%g4]
#endif

	brz,pt	%g7, 5f				! No saved reg wins
	 nop
	dec	%g7				! We can do this now or later.  Move to last entry

#ifdef DEBUG
	rdpr	%canrestore, %g4			! DEBUG Make sure we've restored everything
	brnz,a,pn	%g4, 0f				! DEBUG
	 sir						! DEBUG we should NOT have any usable windows here
0:							! DEBUG
	wrpr	%g0, 5, %tl
#endif
	rdpr	%otherwin, %g4
	sll	%g7, 7, %g5			! calculate ptr into rw64 array 8*16 == 128 or 7 bits
	brz,pt	%g4, 6f				! We should not have any user windows left
	 add	%g5, %g6, %g5

	set	1f, %o0
	mov	%g7, %o1
	mov	%g4, %o2
	call	printf
	 wrpr	%g0, PSTATE_KERN, %pstate
	set	2f, %o0
	call	panic
	 nop
	NOTREACHED
	.data
1:	.asciz	"pcb_nsaved=%x and otherwin=%x\n"
2:	.asciz	"rft_user\n"
	_ALIGN
	.text
6:
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

#ifdef DEBUG
	stx	%g0, [%g5 + PCB_RW + (14*8)]		! DEBUG mark that we've saved this one
#endif

	cmp	%g5, %g6
	bgu,pt	%xcc, 3b				! Next one?
	 dec	8*16, %g5

	rdpr	%ver, %g5
	stb	%g0, [%g6 + PCB_NSAVED]			! Clear them out so we won't do this again
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
	sethi	%hi(CPCB), %g5
	LDPTR	[%g5 + %lo(CPCB)], %g5
	ldub	[%g5 + PCB_NSAVED], %g5		! Any saved reg windows?
	tst	%g5
	tnz	%icc, 1; nop			! Debugger if we still have saved windows
	bne,a	rft_user			! Try starting over again
	 sethi	%hi(CPUINFO_VA+CI_WANT_AST), %g7
#endif
	/*
	 * Set up our return trapframe so we can recover if we trap from here
	 * on in.
	 */
	wrpr	%g0, 1, %tl			! Set up the trap state
	wrpr	%g2, 0, %tpc
	wrpr	%g3, 0, %tnpc
	ba,pt	%icc, 6f
	 wrpr	%g1, %g0, %tstate

5:
	/*
	 * Set up our return trapframe so we can recover if we trap from here
	 * on in.
	 */
	wrpr	%g0, 1, %tl			! Set up the trap state
	wrpr	%g2, 0, %tpc
	wrpr	%g3, 0, %tnpc
	wrpr	%g1, %g0, %tstate
	restore
6:
	CHKPT(%g4,%g7,0xa)
	rdpr	%canrestore, %g5
	wrpr	%g5, 0, %cleanwin			! Force cleanup of kernel windows

#ifdef NOTDEF_DEBUG
	ldx	[%g6 + CC64FSZ + STKB + TF_L + (0*8)], %g5! DEBUG -- get proper value for %l0
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
	sethi	%hi(CPCB), %g6
	LDPTR	[%g6 + %lo(CPCB)], %g6
	lduw	[%g4], %g4
	stb	%g4, [%g6 + PCB_NSAVED]
	ta	1
	sir
	.data
badregs:
	.space	16*4
	.text
1:
#endif

	rdpr	%tstate, %g1
	rdpr	%cwp, %g7			! Find our cur window
	andn	%g1, CWP, %g1			! Clear it from %tstate
	wrpr	%g1, %g7, %tstate		! Set %tstate with %cwp
	CHKPT(%g4,%g7,0xb)

	wr	%g0, ASI_DMMU, %asi		! restore the user context
	ldxa	[CTX_SECONDARY] %asi, %g4
	sethi	%hi(KERNBASE), %g7		! Should not be needed due to retry
	stxa	%g4, [CTX_PRIMARY] %asi
	membar	#Sync				! Should not be needed due to retry
	flush	%g7				! Should not be needed due to retry
	CLRTT
	CHKPT(%g4,%g7,0xd)
#ifdef TRAPSTATS
	set	_C_LABEL(rftudone), %g1
	lduw	[%g1], %g2
	inc	%g2
	stw	%g2, [%g1]
#endif
#ifdef DEBUG
	sethi	%hi(CPCB), %g5
	LDPTR	[%g5 + %lo(CPCB)], %g5
	ldub	[%g5 + PCB_NSAVED], %g5		! Any saved reg windows?
	tst	%g5
	tnz	%icc, 1; nop			! Debugger if we still have saved windows!
#endif
	wrpr	%g0, 0, %pil			! Enable all interrupts
	retry

! exported end marker for kernel gdb
	.globl	_C_LABEL(endtrapcode)
_C_LABEL(endtrapcode):

#ifdef DDB
!!!
!!! Dump the DTLB to phys address in %o0 and print it
!!!
!!! Only toast a few %o registers
!!!

ENTRY_NOPROFILE(dump_dtlb)
	clr	%o1
	add	%o1, (64 * 8), %o3
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

ENTRY_NOPROFILE(dump_itlb)
	clr	%o1
	add	%o1, (64 * 8), %o3
1:
	ldxa	[%o1] ASI_IMMU_TLB_TAG, %o2
	membar	#Sync
	stx	%o2, [%o0]
	membar	#Sync
	inc	8, %o0
	ldxa	[%o1] ASI_IMMU_TLB_DATA, %o4
	membar	#Sync
	inc	8, %o1
	stx	%o4, [%o0]
	cmp	%o1, %o3
	membar	#Sync
	bl	1b
	 inc	8, %o0

	retl
	 nop

#ifdef _LP64
ENTRY_NOPROFILE(print_dtlb)
	save	%sp, -CC64FSZ, %sp
	clr	%l1
	add	%l1, (64 * 8), %l3
	clr	%l2
1:
	ldxa	[%l1] ASI_DMMU_TLB_TAG, %o2
	membar	#Sync
	mov	%l2, %o1
	ldxa	[%l1] ASI_DMMU_TLB_DATA, %o3
	membar	#Sync
	inc	%l2
	set	2f, %o0
	call	_C_LABEL(db_printf)
	 inc	8, %l1

	ldxa	[%l1] ASI_DMMU_TLB_TAG, %o2
	membar	#Sync
	mov	%l2, %o1
	ldxa	[%l1] ASI_DMMU_TLB_DATA, %o3
	membar	#Sync
	inc	%l2
	set	3f, %o0
	call	_C_LABEL(db_printf)
	 inc	8, %l1

	cmp	%l1, %l3
	bl	1b
	 inc	8, %l0

	ret
	 restore


ENTRY_NOPROFILE(print_itlb)
	save	%sp, -CC64FSZ, %sp
	clr	%l1
	add	%l1, (64 * 8), %l3
	clr	%l2
1:
	ldxa	[%l1] ASI_IMMU_TLB_TAG, %o2
	membar	#Sync
	mov	%l2, %o1
	ldxa	[%l1] ASI_IMMU_TLB_DATA, %o3
	membar	#Sync
	inc	%l2
	set	2f, %o0
	call	_C_LABEL(db_printf)
	 inc	8, %l1

	ldxa	[%l1] ASI_IMMU_TLB_TAG, %o2
	membar	#Sync
	mov	%l2, %o1
	ldxa	[%l1] ASI_IMMU_TLB_DATA, %o3
	membar	#Sync
	inc	%l2
	set	3f, %o0
	call	_C_LABEL(db_printf)
	 inc	8, %l1

	cmp	%l1, %l3
	bl	1b
	 inc	8, %l0

	ret
	 restore

	.data
2:
	.asciz	"%2d:%016lx %016lx "
3:
	.asciz	"%2d:%016lx %016lx\r\n"
	.text
#else
ENTRY_NOPROFILE(print_dtlb)
	save	%sp, -CC64FSZ, %sp
	clr	%l1
	add	%l1, (64 * 8), %l3
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
	call	_C_LABEL(db_printf)
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
	call	_C_LABEL(db_printf)
	 inc	8, %l1

	cmp	%l1, %l3
	bl	1b
	 inc	8, %l0

	ret
	 restore

ENTRY_NOPROFILE(print_itlb)
	save	%sp, -CC64FSZ, %sp
	clr	%l1
	add	%l1, (64 * 8), %l3
	clr	%l2
1:
	ldxa	[%l1] ASI_IMMU_TLB_TAG, %o2
	membar	#Sync
	srl	%o2, 0, %o3
	mov	%l2, %o1
	srax	%o2, 32, %o2
	ldxa	[%l1] ASI_IMMU_TLB_DATA, %o4
	membar	#Sync
	srl	%o4, 0, %o5
	inc	%l2
	srax	%o4, 32, %o4
	set	2f, %o0
	call	_C_LABEL(db_printf)
	 inc	8, %l1

	ldxa	[%l1] ASI_IMMU_TLB_TAG, %o2
	membar	#Sync
	srl	%o2, 0, %o3
	mov	%l2, %o1
	srax	%o2, 32, %o2
	ldxa	[%l1] ASI_IMMU_TLB_DATA, %o4
	membar	#Sync
	srl	%o4, 0, %o5
	inc	%l2
	srax	%o4, 32, %o4
	set	3f, %o0
	call	_C_LABEL(db_printf)
	 inc	8, %l1

	cmp	%l1, %l3
	bl	1b
	 inc	8, %l0

	ret
	 restore

	.data
2:
	.asciz	"%2d:%08x:%08x %08x:%08x "
3:
	.asciz	"%2d:%08x:%08x %08x:%08x\r\n"
	.text
#endif
#endif

/*
 * Kernel entry point.
 *
 * The contract between bootloader and kernel is:
 *
 * %o0		OpenFirmware entry point, to keep Sun's updaters happy
 * %o1		Address of boot information vector (see bootinfo.h)
 * %o2		Length of the vector, in bytes
 * %o3		OpenFirmware entry point, to mimic Sun bootloader behavior
 * %o4		OpenFirmware, to meet earlier NetBSD kernels expectations
 */
	.align	8
start:
dostart:
	wrpr	%g0, 0, %tick	! XXXXXXX clear %tick register for now
	mov	1, %g1
	sllx	%g1, 63, %g1
	wr	%g1, TICK_CMPR	! XXXXXXX clear and disable %tick_cmpr as well
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
	wr	%g0, FPRS_FEF, %fprs		! Turn on FPU

	/*
	 * Step 2: Set up a v8-like stack if we need to
	 */

#ifdef _LP64
	btst	1, %sp
	bnz,pt	%icc, 0f
	 nop
	add	%sp, -BIAS, %sp
#else
	btst	1, %sp
	bz,pt	%icc, 0f
	 nop
	add	%sp, BIAS, %sp
#endif
0:

	call	_C_LABEL(bootstrap)
	 clr	%g4				! Clear data segment pointer

/*
 * Initialize the boot CPU.  Basically:
 *
 *	Locate the cpu_info structure for this CPU.
 *	Establish a locked mapping for interrupt stack.
 *	Switch to the initial stack.
 *	Call the routine passed in in cpu_info->ci_spinup
 */

#ifdef NO_VCACHE
#define	TTE_DATABITS	TTE_L|TTE_CP|TTE_P|TTE_W
#else
#define	TTE_DATABITS	TTE_L|TTE_CP|TTE_CV|TTE_P|TTE_W
#endif


_C_LABEL(cpu_initialize):
	/*
	 * Step 5: is no more.
	 */
	
	/*
	 * Step 6: hunt through cpus list and find the one that
	 * matches our UPAID.
	 */
	sethi	%hi(_C_LABEL(cpus)), %l1
	ldxa	[%g0] ASI_MID_REG, %l2
	LDPTR	[%l1 + %lo(_C_LABEL(cpus))], %l1
	srax	%l2, 17, %l2			! Isolate UPAID from CPU reg
	and	%l2, 0x1f, %l2
0:
	ld	[%l1 + CI_UPAID], %l3		! Load UPAID
	cmp	%l3, %l2			! Does it match?
	bne,a,pt	%icc, 0b		! no
	 ld	[%l1 + CI_NEXT], %l1		! Load next cpu_info pointer


	/*
	 * Get pointer to our cpu_info struct
	 */

	ldx	[%l1 + CI_PADDR], %l1		! Load the interrupt stack's PA

	sethi	%hi(0xa0000000), %l2		! V=1|SZ=01|NFO=0|IE=0
	sllx	%l2, 32, %l2			! Shift it into place

	mov	-1, %l3				! Create a nice mask
	sllx	%l3, 41, %l4			! Mask off high bits
	or	%l4, 0xfff, %l4			! We can just load this in 12 (of 13) bits

	set	64*1024, %l5			! calculate pa of second mapping
	add	%l1, %l5, %l6
	andn	%l1, %l4, %l1			! Mask the phys page number
	andn	%l6, %l4, %l6			! for both mappings

	or	%l2, %l1, %l1			! Now take care of the high bits
	or	%l2, %l6, %l6
	or	%l1, TTE_DATABITS, %l2		! And low bits:	L=1|CP=1|CV=?|E=0|P=1|W=0|G=0
	or	%l6, TTE_DATABITS, %l6

	!!
	!!  Now, map in the interrupt stack as context==0
	!!
	set	TLB_TAG_ACCESS, %l5
	set	1f, %o5
	sethi	%hi(INTSTACK), %l0
	stxa	%l0, [%l5] ASI_DMMU		! Make DMMU point to it
	membar	#Sync				! We may need more membar #Sync in here
	stxa	%l2, [%g0] ASI_DMMU_DATA_IN	! Store it
	membar	#Sync				! We may need more membar #Sync in here
	flush	%o5
1:
	!!
	!! Map in idle u area and kernel stack
	!!
	sethi	%hi(KSTACK_VA), %l0
	stxa	%l0, [%l5] ASI_DMMU		! Make DMMU point to it
	membar	#Sync
	stxa	%l6, [%g0] ASI_DMMU_DATA_IN	! Store it
	membar	#Sync
	flush	%o5

!!! Make sure our stack's OK.
	flushw
	sethi	%hi(CPUINFO_VA+CI_INITSTACK), %l0
	LDPTR	[%l0 + %lo(CPUINFO_VA+CI_INITSTACK)], %l0
 	add	%l0, - CC64FSZ - 80, %l0	! via syscall(boot_me_up) or somesuch
#ifdef _LP64
	andn	%l0, 0x0f, %l0			! Needs to be 16-byte aligned
	sub	%l0, BIAS, %l0			! and biased
#endif
	mov	%l0, %sp
	flushw

#ifdef DEBUG
	set	_C_LABEL(pmapdebug), %o1
	ld	[%o1], %o1
	sethi	%hi(0x40000), %o2
	btst	%o2, %o1
	bz	0f
	
	set	1f, %o0		! Debug printf
	call	_C_LABEL(prom_printf)
	.data
1:
	.asciz	"Setting trap base...\r\n"
	_ALIGN
	.text
0:	
#endif
	/*
	 * Step 7: change the trap base register, and install our TSB pointers
	 */
	sethi	%hi(0x1fff), %l2
	set	_C_LABEL(tsb_dmmu), %l0
	LDPTR	[%l0], %l0
	set	_C_LABEL(tsbsize), %l1
	or	%l2, %lo(0x1fff), %l2
	ld	[%l1], %l1
	andn	%l0, %l2, %l0			! Mask off size and split bits
	or	%l0, %l1, %l0			! Make a TSB pointer
	set	TSB, %l2
	stxa	%l0, [%l2] ASI_DMMU		! Install data TSB pointer
	membar	#Sync

	sethi	%hi(0x1fff), %l2
	set	_C_LABEL(tsb_immu), %l0
	LDPTR	[%l0], %l0
	set	_C_LABEL(tsbsize), %l1
	or	%l2, %lo(0x1fff), %l2
	ld	[%l1], %l1
	andn	%l0, %l2, %l0			! Mask off size and split bits
	or	%l0, %l1, %l0			! Make a TSB pointer
	set	TSB, %l2
	stxa	%l0, [%l2] ASI_IMMU		! Install instruction TSB pointer
	membar	#Sync

	set	_C_LABEL(trapbase), %l1
	call	_C_LABEL(prom_set_trap_table)	! Now we should be running 100% from our handlers
	 mov	%l1, %o0
	wrpr	%l1, 0, %tba			! Make sure the PROM didn't foul up.

	/*
	 * Switch to the kernel mode and run away.
	 */
	wrpr	%g0, WSTATE_KERN, %wstate

#ifdef DEBUG
	wrpr	%g0, 1, %tl			! Debug -- start at tl==3 so we'll watchdog
	wrpr	%g0, 0x1ff, %tt			! Debug -- clear out unused trap regs
	wrpr	%g0, 0, %tpc
	wrpr	%g0, 0, %tnpc
	wrpr	%g0, 0, %tstate
	wrpr	%g0, 0, %tl
#endif

#ifdef DEBUG
	set	_C_LABEL(pmapdebug), %o1
	ld	[%o1], %o1
	sethi	%hi(0x40000), %o2
	btst	%o2, %o1
	bz	0f
	
	set	1f, %o0		! Debug printf
	call	_C_LABEL(prom_printf)
	.data
1:
	.asciz	"Calling startup routine...\r\n"
	_ALIGN
	.text
0:	
#endif
	/*
	 * Call our startup routine.
	 */

	sethi	%hi(CPUINFO_VA+CI_SPINUP), %l0
	LDPTR	[%l0 + %lo(CPUINFO_VA+CI_SPINUP)], %o1

	call	%o1				! Call routine
	 clr	%o0				! our frame arg is ignored

	set	1f, %o0				! Main should never come back here
	call	_C_LABEL(panic)
	 nop
	.data
1:
	.asciz	"main() returned\r\n"
	_ALIGN
	.text

#if defined(MULTIPROCESSOR)
	/*
	 * cpu_mp_startup is called with:
	 *
	 *	%g2 = cpu_args
	 */
ENTRY(cpu_mp_startup)
	wrpr    %g0, 0, %cleanwin
	wrpr	%g0, 13, %pil
	wrpr	%g0, PSTATE_INTR|PSTATE_PEF, %pstate
	wr	%g0, FPRS_FEF, %fprs		! Turn on FPU

	wrpr	%g0, 0, %tl			! Make sure we're not in NUCLEUS mode

	flushw

	/*
	 * Get pointer to our cpu_info struct
	 */
	ldx	[%g2 + CBA_CPUINFO], %l1	! Load the interrupt stack's PA
	sethi	%hi(0xa0000000), %l2		! V=1|SZ=01|NFO=0|IE=0
	sllx	%l2, 32, %l2			! Shift it into place
	mov	-1, %l3				! Create a nice mask
	sllx	%l3, 41, %l4			! Mask off high bits
	or	%l4, 0xfff, %l4			! We can just load this in 12 (of 13) bits
	andn	%l1, %l4, %l1			! Mask the phys page number
	or	%l2, %l1, %l1			! Now take care of the high bits
#ifdef NO_VCACHE
	or	%l1, TTE_L|TTE_CP|TTE_P|TTE_W, %l2	! And low bits:	L=1|CP=1|CV=0|E=0|P=1|W=0|G=0
#else
	or	%l1, TTE_L|TTE_CP|TTE_CV|TTE_P|TTE_W, %l2	! And low bits:	L=1|CP=1|CV=1|E=0|P=1|W=0|G=0
#endif

	!!
	!!  Now, map in the interrupt stack & cpu_info as context==0
	!!
	set	TLB_TAG_ACCESS, %l5
	set	1f, %o5
	set	INTSTACK, %l0
	stxa	%l0, [%l5] ASI_DMMU		! Make DMMU point to it
	membar	#Sync
	stxa	%l2, [%g0] ASI_DMMU_DATA_IN	! Store it
	membar	#Sync
	flush	%o5
	flush	%l0
1:
	!!
	!! Map in idle u area and kernel stack
	!!
	set	KSTACK_VA, %l0
	stxa	%l0, [%l5] ASI_DMMU		! Make DMMU point to it
	membar	#Sync
	stxa	%l2, [%g0] ASI_DMMU_DATA_IN	! Store it
	membar	#Sync
	flush	%o5
	flush	%l0

	!!
	!! Set 0 as primary context XXX
	!!
	mov	CTX_PRIMARY, %o0
	stxa	%g0, [%o0] ASI_DMMU
	flush	%o5

!!! Make sure our stack's OK. 
	LDPTR	[%g2 + CBA_INITSTACK], %l0
 	add	%l0, - CC64FSZ - 80, %l0
#ifdef _LP64
	andn	%l0, 0x0f, %l0			! Needs to be 16-byte aligned
	sub	%l0, BIAS, %l0			! and biased
#endif
	mov	%l0, %sp
	set	1, %fp
	clr	%i7

	/*
	 * install our TSB pointers
	 */
	sethi	%hi(0x1fff), %l2
	set	_C_LABEL(tsb_dmmu), %l0
	LDPTR	[%l0], %l0
	set	_C_LABEL(tsbsize), %l1
	or	%l2, %lo(0x1fff), %l2
	ld	[%l1], %l1
	andn	%l0, %l2, %l0			! Mask off size and split bits
	or	%l0, %l1, %l0			! Make a TSB pointer
	set	TSB, %l2
	stxa	%l0, [%l2] ASI_DMMU		! Install data TSB pointer
	membar	#Sync

	sethi	%hi(0x1fff), %l2
	set	_C_LABEL(tsb_immu), %l0
	LDPTR	[%l0], %l0
	set	_C_LABEL(tsbsize), %l1
	or	%l2, %lo(0x1fff), %l2
	ld	[%l1], %l1
	andn	%l0, %l2, %l0			! Mask off size and split bits
	or	%l0, %l1, %l0			! Make a TSB pointer
	set	TSB, %l2
	stxa	%l0, [%l2] ASI_IMMU		! Install instruction TSB pointer
	membar	#Sync

	/* set trap table */
	set	_C_LABEL(trapbase), %l1
	call	_C_LABEL(prom_set_trap_table)
	 mov	%l1, %o0
	wrpr	%l1, 0, %tba			! Make sure the PROM didn't foul up.

	/*
	 * Switch to the kernel mode and run away.
	 */
	wrpr	%g0, WSTATE_KERN, %wstate

	call	_C_LABEL(cpu_hatch)
	 clr %g4

	/* set up state required by idle */
	sethi	%hi(_C_LABEL(sched_whichqs)), %l2
	sethi	%hi(CURLWP), %l7
	sethi	%hi(CPCB), %l6
	LDPTR	[%l6 + %lo(CPCB)], %l5

	b	_C_LABEL(idle_nolock)
	 clr	%l4

	NOTREACHED

	.globl cpu_mp_startup_end
cpu_mp_startup_end:
#endif	/* MULTIPROCESSOR */

	.align 8
ENTRY(get_romtba)
	retl
	 rdpr	%tba, %o0
/*
 * int get_maxctx(void)
 *
 * Get number of available contexts.
 *
 */
	.align 8
ENTRY(get_maxctx)
	set	CTX_SECONDARY, %o1		! Store -1 in the context register
	mov	-1, %o2
	stxa	%o2, [%o1] ASI_DMMU
	membar	#Sync
	ldxa	[%o1] ASI_DMMU, %o0		! then read it back
	membar	#Sync
	stxa	%g0, [%o1] ASI_DMMU
	membar	#Sync
	retl
	 inc	%o0

/*
 * openfirmware(cell* param);
 *
 * OpenFirmware entry point
 *
 * If we're running in 32-bit mode we need to convert to a 64-bit stack
 * and 64-bit cells.  The cells we'll allocate off the stack for simplicity.
 */
	.align 8
ENTRY(openfirmware)
	sethi	%hi(romp), %o4
	andcc	%sp, 1, %g0
	bz,pt	%icc, 1f
	 LDPTR	[%o4+%lo(romp)], %o4		! v9 stack, just load the addr and callit
	save	%sp, -CC64FSZ, %sp
	rdpr	%pil, %i2
	mov	PIL_HIGH, %i3
	cmp	%i3, %i2
	movle	%icc, %i2, %i3
	wrpr	%g0, %i3, %pil
	mov	%i0, %o0
	mov	%g1, %l1
	mov	%g2, %l2
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	rdpr	%pstate, %l0
	jmpl	%i4, %o7
#if !defined(_LP64)
	 wrpr	%g0, PSTATE_PROM, %pstate
#else
	 wrpr	%g0, PSTATE_PROM|PSTATE_IE, %pstate
#endif
	wrpr	%l0, %g0, %pstate
	mov	%l1, %g1
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7
	wrpr	%i2, 0, %pil
	ret
	 restore	%o0, %g0, %o0

1:	! v8 -- need to screw with stack & params
#ifdef NOTDEF_DEBUG
	mov	%o7, %o5
	call	globreg_check
	 nop
	mov	%o5, %o7
#endif
	save	%sp, -CC64FSZ, %sp		! Get a new 64-bit stack frame
	add	%sp, -BIAS, %sp
	rdpr	%pstate, %l0
	srl	%sp, 0, %sp
	rdpr	%pil, %i2	! s = splx(level)
	mov	%i0, %o0
	mov	PIL_HIGH, %i3
	mov	%g1, %l1
	mov	%g2, %l2
	cmp	%i3, %i2
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	movle	%icc, %i2, %i3
	mov	%g6, %l6
	mov	%g7, %l7
	wrpr	%i3, %g0, %pil
	jmpl	%i4, %o7
	! Enable 64-bit addresses for the prom
#if defined(_LP64)
	 wrpr	%g0, PSTATE_PROM, %pstate
#else
	 wrpr	%g0, PSTATE_PROM|PSTATE_IE, %pstate
#endif
	wrpr	%l0, 0, %pstate
	wrpr	%i2, 0, %pil
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
 * void ofw_exit(cell_t args[])
 */
ENTRY(openfirmware_exit)
	STACKFRAME(-CC64FSZ)
	flushw					! Flush register windows

	wrpr	%g0, PIL_HIGH, %pil		! Disable interrupts
	set	romtba, %l5
	wrpr	%l5, 0, %tba			! restore the ofw trap table

	/* Arrange locked kernel stack as PROM stack */
	sethi	%hi(CPUINFO_VA+CI_INITSTACK), %l5
	LDPTR	[%l5 + %lo(CPUINFO_VA+CI_INITSTACK)], %l5
 	add	%l5, - CC64FSZ - 80, %l5	! via syscall(boot_me_up) or somesuch

#ifdef _LP64
	andn	%l5, 0x0f, %l5			! Needs to be 16-byte aligned
	sub	%l5, BIAS, %l5			! and biased
#endif
	mov	%l5, %sp
	flushw

	set	romp, %l6
	LDPTR	[%l6], %l6

	mov     CTX_PRIMARY, %l3		! set context 0
	stxa    %g0, [%l3] ASI_DMMU
	membar	#Sync

	wrpr	%g0, 0, %tl			! force trap level 0
	call	%l6
	 mov	%i0, %o0
	NOTREACHED

/*
 * sp_tlb_flush_pte(vaddr_t va, int ctx)
 *
 * Flush tte from both IMMU and DMMU.
 *
 * This uses %o0-%o5
 */
	.align 8
ENTRY(sp_tlb_flush_pte)
#ifdef DEBUG
	set	DATA_START, %o4				! Forget any recent TLB misses
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
	call	_C_LABEL(printf)
	 mov	%i0, %o2
	restore
	.data
1:
	.asciz	"sp_tlb_flush_pte:	demap ctx=%x va=%08x res=%x\r\n"
	_ALIGN
	.text
2:
#endif
#ifdef	SPITFIRE
	mov	CTX_SECONDARY, %o2
	andn	%o0, 0xfff, %o0				! drop unused va bits
	ldxa	[%o2] ASI_DMMU, %g1			! Save secondary context
	sethi	%hi(KERNBASE), %o4
	membar	#LoadStore
	stxa	%o1, [%o2] ASI_DMMU			! Insert context to demap
	membar	#Sync
	or	%o0, DEMAP_PAGE_SECONDARY, %o0		! Demap page from secondary context only
	stxa	%o0, [%o0] ASI_DMMU_DEMAP		! Do the demap
	stxa	%o0, [%o0] ASI_IMMU_DEMAP		! to both TLBs
	srl	%o0, 0, %o0				! and make sure it's both 32- and 64-bit entries
	stxa	%o0, [%o0] ASI_DMMU_DEMAP		! Do the demap
	stxa	%o0, [%o0] ASI_IMMU_DEMAP		! Do the demap
	flush	%o4
	stxa	%g1, [%o2] ASI_DMMU			! Restore secondary context
	membar	#Sync
	retl
	 nop
#else
	!!
	!! Cheetahs do not support flushing the IMMU from secondary context
	!!
	rdpr	%tl, %o3
	mov	CTX_PRIMARY, %o2
	brnz,pt	%o3, 1f
	 andn	%o0, 0xfff, %o0				! drop unused va bits
	wrpr	%g0, 1, %tl				! Make sure we're NUCLEUS
1:	
	ldxa	[%o2] ASI_DMMU, %o5			! Save primary context
	sethi	%hi(KERNBASE), %o4
	membar	#LoadStore
	stxa	%o1, [%o2] ASI_DMMU			! Insert context to demap
	membar	#Sync
	or	%o0, DEMAP_PAGE_PRIMARY, %o0
	stxa	%o0, [%o0] ASI_DMMU_DEMAP		! Do the demap
	stxa	%o0, [%o0] ASI_IMMU_DEMAP		! to both TLBs
	srl	%o0, 0, %o0				! and make sure it's both 32- and 64-bit entries
	stxa	%o0, [%o0] ASI_DMMU_DEMAP		! Do the demap
	stxa	%o0, [%o0] ASI_IMMU_DEMAP		! Do the demap
	flush	%o4
	stxa	%o5, [%o2] ASI_DMMU			! Restore primary context
	brz,pt	%o3, 1f
	 flush	%o4
	retl
	 nop
1:	
	retl
	 wrpr	%g0, 0, %tl				! Return to kernel mode.
#endif

/*
 * sp_tlb_flush_ctx(int ctx)
 *
 * Flush entire context from both IMMU and DMMU.
 *
 * This uses %o0-%o5
 */
	.align 8
ENTRY(sp_tlb_flush_ctx)
#ifdef DEBUG
	set	DATA_START, %o4				! Forget any recent TLB misses
	stx	%g0, [%o4]
#endif
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	call	printf
	 mov	%i0, %o1
	restore
	.data
1:
	.asciz	"sp_tlb_flush_ctx:	context flush of %d attempted\r\n"
	_ALIGN
	.text
#endif
#ifdef DIAGNOSTIC
	brnz,pt	%o0, 2f
	 nop
	set	1f, %o0
	call	panic
	 nop
	.data
1:
	.asciz	"sp_tlb_flush_ctx:	attempted demap of NUCLEUS context\r\n"
	_ALIGN
	.text
2:
#endif
#ifdef SPITFIRE
	mov	CTX_SECONDARY, %o2
	ldxa	[%o2] ASI_DMMU, %o1		! Save secondary context
	sethi	%hi(KERNBASE), %o4
	membar	#LoadStore
	stxa	%o0, [%o2] ASI_DMMU		! Insert context to demap
	set	DEMAP_CTX_SECONDARY, %o5
	membar	#Sync
	stxa	%o5, [%o5] ASI_DMMU_DEMAP	! Do the demap
	stxa	%o5, [%o5] ASI_IMMU_DEMAP	! Do the demap
	membar	#Sync
	stxa	%o1, [%o2] ASI_DMMU		! Restore secondary asi
	flush	%o4
	retl
	 nop
#else
	rdpr	%tl, %o3
	mov	CTX_PRIMARY, %o2
	brnz	%o3, 1f
	 sethi	%hi(KERNBASE), %o4
	wrpr	%g0, 1, %tl
1:	
	ldxa	[%o2] ASI_DMMU, %o1		! Save secondary context
	membar	#LoadStore
	stxa	%o0, [%o2] ASI_DMMU		! Insert context to demap
	membar	#Sync
	set	DEMAP_CTX_PRIMARY, %o5
	stxa	%o5, [%o5] ASI_DMMU_DEMAP	! Do the demap
	stxa	%o5, [%o5] ASI_IMMU_DEMAP	! Do the demap
	membar	#Sync
	stxa	%o1, [%o2] ASI_DMMU		! Restore secondary asi
	membar	#Sync
	brz,pt	%o3, 1f
	 flush	%o4
	retl
	 nop
1:	
	retl
	 wrpr	%g0, 0, %tl			! Return to kernel mode.
#endif

/*
 * sp_tlb_flush_all(void)
 *
 * Flush all user TLB entries from both IMMU and DMMU.
 */
	.align 8
ENTRY(sp_tlb_flush_all)
#ifdef SPITFIRE
	save	%sp, -CC64FSZ, %sp
	rdpr	%pstate, %o3
	andn	%o3, PSTATE_IE, %o4			! disable interrupts
	wrpr	%o4, 0, %pstate
	set	(63 * 8), %o0				! last TLB entry
	set	CTX_SECONDARY, %o4
	ldxa	[%o4] ASI_DMMU, %o4			! save secondary context
	set	CTX_MASK, %o5
	membar	#Sync

	! %o0 = loop counter
	! %o1 = ctx value
	! %o2 = TLB tag value
	! %o3 = saved %pstate
	! %o4 = saved secondary ctx
	! %o5 = CTX_MASK

0:
	ldxa	[%o0] ASI_DMMU_TLB_TAG, %o2		! fetch the TLB tag
	andcc	%o2, %o5, %o1				! context 0?
	bz,pt	%xcc, 1f				! if so, skip
	 mov	CTX_SECONDARY, %o2

	stxa	%o1, [%o2] ASI_DMMU			! set the context
	set	DEMAP_CTX_SECONDARY, %o2
	membar	#Sync
	stxa	%o2, [%o2] ASI_DMMU_DEMAP		! do the demap
	membar	#Sync

1:
	dec	8, %o0
	brgz,pt %o0, 0b					! loop over all entries
	 nop

/*
 * now do the IMMU
 */

	set	(63 * 8), %o0				! last TLB entry

0:
	ldxa	[%o0] ASI_IMMU_TLB_TAG, %o2		! fetch the TLB tag
	andcc	%o2, %o5, %o1				! context 0?
	bz,pt	%xcc, 1f				! if so, skip
	 mov	CTX_SECONDARY, %o2

	stxa	%o1, [%o2] ASI_DMMU			! set the context
	set	DEMAP_CTX_SECONDARY, %o2
	membar	#Sync
	stxa	%o2, [%o2] ASI_IMMU_DEMAP		! do the demap
	membar	#Sync

1:
	dec	8, %o0
	brgz,pt %o0, 0b					! loop over all entries
	 nop

	set	CTX_SECONDARY, %o2
	stxa	%o4, [%o2] ASI_DMMU			! restore secondary ctx
	sethi	%hi(KERNBASE), %o4
	membar	#Sync
	flush	%o4
!	retl
	 wrpr	%o3, %pstate

	ret
	 restore
#else
	WRITEME
#endif

/*
 * blast_dcache()
 *
 * Clear out all of D$ regardless of contents
 * Does not modify %o0
 *
 */
	.align 8
ENTRY(blast_dcache)
/*
 * We turn off interrupts for the duration to prevent RED exceptions.
 */
#ifdef PROF
	save	%sp, -CC64FSZ, %sp
#endif

	rdpr	%pstate, %o3
	set	(2 * NBPG) - 8, %o1
	andn	%o3, PSTATE_IE, %o4			! Turn off PSTATE_IE bit
	wrpr	%o4, 0, %pstate
1:
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	brnz,pt	%o1, 1b
	 dec	8, %o1
	sethi	%hi(KERNBASE), %o2
	flush	%o2
#ifdef PROF
	wrpr	%o3, %pstate
	ret
	 restore
#else
	retl
	 wrpr	%o3, %pstate
#endif

/*
 * blast_icache()
 *
 * Clear out all of I$ regardless of contents
 * Does not modify %o0
 *
 */
	.align 8
ENTRY(blast_icache)
/*
 * We turn off interrupts for the duration to prevent RED exceptions.
 */
	rdpr	%pstate, %o3
	set	(2 * NBPG) - 8, %o1
	andn	%o3, PSTATE_IE, %o4			! Turn off PSTATE_IE bit
	wrpr	%o4, 0, %pstate
1:
	stxa	%g0, [%o1] ASI_ICACHE_TAG
	brnz,pt	%o1, 1b
	 dec	8, %o1
	sethi	%hi(KERNBASE), %o2
	flush	%o2
	retl
	 wrpr	%o3, %pstate

/*
 * dcache_flush_page(paddr_t pa)
 *
 * Clear one page from D$.
 *
 */
	.align 8
ENTRY(dcache_flush_page)
#ifndef _LP64
	COMBINE(%o0, %o1, %o0)
#endif
	mov	-1, %o1		! Generate mask for tag: bits [29..2]
	srlx	%o0, 13-2, %o2	! Tag is PA bits <40:13> in bits <29:2>
	clr	%o4
	srl	%o1, 2, %o1	! Now we have bits <29:0> set
	set	(2*NBPG), %o5
	ba,pt	%icc, 1f
	 andn	%o1, 3, %o1	! Now we have bits <29:2> set

	.align 8
1:
	ldxa	[%o4] ASI_DCACHE_TAG, %o3
	mov	%o4, %o0
	deccc	32, %o5
	bl,pn	%icc, 2f
	 inc	32, %o4

	xor	%o3, %o2, %o3
	andcc	%o3, %o1, %g0
	bne,pt	%xcc, 1b
	 membar	#LoadStore

	stxa	%g0, [%o0] ASI_DCACHE_TAG
	ba,pt	%icc, 1b
	 membar	#StoreLoad
2:

	wr	%g0, ASI_PRIMARY_NOFAULT, %asi
	sethi	%hi(KERNBASE), %o5
	flush	%o5
	retl
	 membar	#Sync

/*
 * icache_flush_page(paddr_t pa)
 *
 * Clear one page from I$.
 *
 */
	.align 8
ENTRY(icache_flush_page)
#ifndef _LP64
	COMBINE(%o0, %o1, %o0)
#endif

#ifdef SPITFIRE
	!!
	!! Linux sez that I$ flushes are not needed for cheetah.
	!!
	
	!! Now do the I$
	srlx	%o0, 13-8, %o2
	mov	-1, %o1		! Generate mask for tag: bits [35..8]
	srl	%o1, 32-35+7, %o1
	clr	%o4
	sll	%o1, 7, %o1	! Mask
	set	(2*NBPG), %o5
	
1:
	ldda	[%o4] ASI_ICACHE_TAG, %g0	! Tag goes in %g1
	dec	16, %o5
	xor	%g1, %o2, %g1
	andcc	%g1, %o1, %g0
	bne,pt	%xcc, 2f
	 membar	#LoadStore
	stxa	%g0, [%o4] ASI_ICACHE_TAG
	membar	#StoreLoad
2:
	brnz,pt	%o5, 1b
	 inc	16, %o4
#endif
	sethi	%hi(KERNBASE), %o5
	flush	%o5
	membar	#Sync
	retl
	 nop

/*
 *	cache_flush_phys(paddr_t, psize_t, int);
 *
 *	Clear a set of paddrs from the D$, I$ and if param3 is
 *	non-zero, E$.  (E$ is not supported yet).
 */

	.align 8
ENTRY(cache_flush_phys)
#ifndef _LP64
	COMBINE(%o0, %o1, %o0)
	COMBINE(%o2, %o3, %o1)
	mov	%o4, %o2
#endif
#ifdef DEBUG
	tst	%o2		! Want to clear E$?
	tnz	1		! Error!
#endif
	add	%o0, %o1, %o1	! End PA

	!!
	!! Both D$ and I$ tags match pa bits 40-13, but
	!! they are shifted different amounts.  So we'll
	!! generate a mask for bits 40-13.
	!!

	mov	-1, %o2		! Generate mask for tag: bits [40..13]
	srl	%o2, 5, %o2	! 32-5 = [27..0]
	sllx	%o2, 13, %o2	! 27+13 = [40..13]

	and	%o2, %o0, %o0	! Mask away uninteresting bits
	and	%o2, %o1, %o1	! (probably not necessary)

	set	(2*NBPG), %o5
	clr	%o4
1:
	ldxa	[%o4] ASI_DCACHE_TAG, %o3
#ifdef SPITFIRE
	ldda	[%o4] ASI_ICACHE_TAG, %g0	! Tag goes in %g1 -- not on cheetah
#endif
	sllx	%o3, 40-29, %o3	! Shift D$ tag into place
	and	%o3, %o2, %o3	! Mask out trash
	cmp	%o0, %o3
	blt,pt	%xcc, 2f	! Too low
	 sllx	%g1, 40-35, %g1	! Shift I$ tag into place
	cmp	%o1, %o3
	bgt,pt	%xcc, 2f	! Too high
	 nop

	membar	#LoadStore
	stxa	%g0, [%o4] ASI_DCACHE_TAG ! Just right
2:
#ifndef SPITFIRE
	cmp	%o0, %g1
	blt,pt	%xcc, 3f
	 cmp	%o1, %g1
	bgt,pt	%icc, 3f
	 nop
	stxa	%g0, [%o4] ASI_ICACHE_TAG
3:
#endif
	membar	#StoreLoad
	dec	16, %o5
	brgz,pt	%o5, 1b
	 inc	16, %o4

	sethi	%hi(KERNBASE), %o5
	flush	%o5
	membar	#Sync
	retl
	 nop

#ifdef COMPAT_16
#ifdef _LP64
/*
 * XXXXX Still needs lotsa cleanup after sendsig is complete and offsets are known
 *
 * The following code is copied to the top of the user stack when each
 * process is exec'ed, and signals are `trampolined' off it.
 *
 * When this code is run, the stack looks like:
 *	[%sp]			128 bytes to which registers can be dumped
 *	[%sp + 128]		signal number (goes in %o0)
 *	[%sp + 128 + 4]		signal code (goes in %o1)
 *	[%sp + 128 + 8]		first word of saved state (sigcontext)
 *	    .
 *	    .
 *	    .
 *	[%sp + NNN]	last word of saved state
 * (followed by previous stack contents or top of signal stack).
 * The address of the function to call is in %g1; the old %g1 and %o0
 * have already been saved in the sigcontext.  We are running in a clean
 * window, all previous windows now being saved to the stack.
 *
 * Note that [%sp + 128 + 8] == %sp + 128 + 16.  The copy at %sp+128+8
 * will eventually be removed, with a hole left in its place, if things
 * work out.
 */
ENTRY_NOPROFILE(sigcode)
	/*
	 * XXX  the `save' and `restore' below are unnecessary: should
	 *	replace with simple arithmetic on %sp
	 *
	 * Make room on the stack for 64 %f registers + %fsr.  This comes
	 * out to 64*4+8 or 264 bytes, but this must be aligned to a multiple
	 * of 64, or 320 bytes.
	 */
	save	%sp, -CC64FSZ - 320, %sp
	mov	%g2, %l2		! save globals in %l registers
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	/*
	 * Saving the fpu registers is expensive, so do it iff it is
	 * enabled and dirty.
	 */
	rd	%fprs, %l0
	btst	FPRS_DL|FPRS_DU, %l0	! All clean?
	bz,pt	%icc, 2f
	 btst	FPRS_DL, %l0		! test dl
	bz,pt	%icc, 1f
	 btst	FPRS_DU, %l0		! test du

	! fpu is enabled, oh well
	stx	%fsr, [%sp + CC64FSZ + BIAS + 0]
	add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block store
	stda	%f0, [%l0] ASI_BLK_P
	inc	BLOCK_SIZE, %l0
	stda	%f16, [%l0] ASI_BLK_P
1:
	bz,pt	%icc, 2f
	 add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block store
	add	%l0, 2*BLOCK_SIZE, %l0	! and skip what we already stored
	stda	%f32, [%l0] ASI_BLK_P
	inc	BLOCK_SIZE, %l0
	stda	%f48, [%l0] ASI_BLK_P
2:
	membar	#Sync
	rd	%fprs, %l0		! reload fprs copy, for checking after
	rd	%y, %l1			! in any case, save %y
	lduw	[%fp + BIAS + 128], %o0	! sig
	lduw	[%fp + BIAS + 128 + 4], %o1	! code
	call	%g1			! (*sa->sa_handler)(sig,code,scp)
	 add	%fp, BIAS + 128 + 8, %o2	! scp
	wr	%l1, %g0, %y		! in any case, restore %y

	/*
	 * Now that the handler has returned, re-establish all the state
	 * we just saved above, then do a sigreturn.
	 */
	btst	FPRS_DL|FPRS_DU, %l0	! All clean?
	bz,pt	%icc, 2f
	 btst	FPRS_DL, %l0		! test dl
	bz,pt	%icc, 1f
	 btst	FPRS_DU, %l0		! test du

	ldx	[%sp + CC64FSZ + BIAS + 0], %fsr
	add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block load
	ldda	[%l0] ASI_BLK_P, %f0
	inc	BLOCK_SIZE, %l0
	ldda	[%l0] ASI_BLK_P, %f16
1:
	bz,pt	%icc, 2f
	 nop
	add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block load
	inc	2*BLOCK_SIZE, %l0	! and skip what we already loaded
	ldda	[%l0] ASI_BLK_P, %f32
	inc	BLOCK_SIZE, %l0
	ldda	[%l0] ASI_BLK_P, %f48
2:
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7
	membar	#Sync

	restore	%g0, SYS_compat_16___sigreturn14, %g1 ! get registers back & set syscall #
	add	%sp, BIAS + 128 + 8, %o0! compute scp
!	andn	%o0, 0x0f, %o0
	t	ST_SYSCALL		! sigreturn(scp)
	! sigreturn does not return unless it fails
	mov	SYS_exit, %g1		! exit(errno)
	t	ST_SYSCALL
	/* NOTREACHED */

	.globl	_C_LABEL(esigcode)
_C_LABEL(esigcode):
#endif

#if !defined(_LP64)

#define SIGCODE_NAME		sigcode
#define ESIGCODE_NAME		esigcode
#define SIGRETURN_NAME		SYS_compat_16___sigreturn14
#define EXIT_NAME		SYS_exit

#include "sigcode32.s"

#endif
#endif

/*
 * Primitives
 */
#ifdef ENTRY
#undef ENTRY
#endif

#ifdef GPROF
	.globl	_mcount
#define	ENTRY(x) \
	.globl _C_LABEL(x); .proc 1; .type _C_LABEL(x),@function; \
_C_LABEL(x): ; \
	.data; \
	.align 8; \
0:	.uaword 0; .uaword 0; \
	.text;	\
	save	%sp, -CC64FSZ, %sp; \
	sethi	%hi(0b), %o0; \
	call	_mcount; \
	or	%o0, %lo(0b), %o0; \
	restore
#else
#define	ENTRY(x)	.globl _C_LABEL(x); .proc 1; \
	.type _C_LABEL(x),@function; _C_LABEL(x):
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
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	8f, %o0
	mov	%i0, %o1
	mov	%i1, %o2
	mov	%i2, %o3
	call	printf
	 mov	%i3, %o4
	restore
	.data
8:	.asciz	"copyinstr: from=%x to=%x max=%x &len=%x\n"
	_ALIGN
	.text
#endif
	brgz,pt	%o2, 1f					! Make sure len is valid
	 sethi	%hi(CPCB), %o4		! (first instr of copy)
	retl
	 mov	ENAMETOOLONG, %o0
1:
	LDPTR	[%o4 + %lo(CPCB)], %o4	! catch faults
	set	Lcsfault, %o5
	membar	#Sync
	STPTR	%o5, [%o4 + PCB_ONFAULT]

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
	ba,pt	%xcc, Lcsdone		!	}
	 mov	ENAMETOOLONG, %o0	!	error = ENAMETOOLONG;
	NOTREACHED

/*
 * copyoutstr(fromaddr, toaddr, maxlength, &lencopied)
 *
 * Copy a null terminated string from the kernel
 * address space to the user address space.
 */
ENTRY(copyoutstr)
	! %o0 = fromaddr, %o1 = toaddr, %o2 = maxlen, %o3 = &lencopied
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	8f, %o0
	mov	%i0, %o1
	mov	%i1, %o2
	mov	%i2, %o3
	call	printf
	 mov	%i3, %o4
	restore
	.data
8:	.asciz	"copyoutstr: from=%x to=%x max=%x &len=%x\n"
	_ALIGN
	.text
#endif
	brgz,pt	%o2, 1f					! Make sure len is valid
	 sethi	%hi(CPCB), %o4		! (first instr of copy)
	retl
	 mov	ENAMETOOLONG, %o0
1:
	LDPTR	[%o4 + %lo(CPCB)], %o4	! catch faults
	set	Lcsfault, %o5
	membar	#Sync
	STPTR	%o5, [%o4 + PCB_ONFAULT]

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
	 STPTR	%o1, [%o3]		!		*lencopied = len;
1:
	retl				! cpcb->pcb_onfault = 0;
	 STPTR	%g0, [%o4 + PCB_ONFAULT]! return (error);

Lcsfault:
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	5f, %o0
	call	printf
	 nop
	restore
	.data
5:	.asciz	"Lcsfault: recovering\n"
	_ALIGN
	.text
#endif
	b	Lcsdone			! error = EFAULT;
	 mov	EFAULT, %o0		! goto ret;

/*
 * copystr(fromaddr, toaddr, maxlength, &lencopied)
 *
 * Copy a null terminated string from one point to another in
 * the kernel address space.  (This is a leaf procedure, but
 * it does not seem that way to the C compiler.)
 */
ENTRY(copystr)
	brgz,pt	%o2, 0f	! Make sure len is valid
	 mov	%o1, %o5		!	to0 = to;
	retl
	 mov	ENAMETOOLONG, %o0
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
	 STPTR	%o1, [%o3]		!		*lencopied = len;
3:
	retl
	 nop
#ifdef DIAGNOSTIC
4:
	sethi	%hi(5f), %o0
	call	_C_LABEL(panic)
	 or	%lo(5f), %o0, %o0
	.data
5:
	.asciz	"copystr"
	_ALIGN
	.text
#endif

/*
 * copyin(src, dst, len)
 *
 * Copy specified amount of data from user space into the kernel.
 *
 * This is a modified version of memcpy that uses ASI_AIUS.  When
 * memcpy is optimized to use block copy ASIs, this should be also.
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
	restore
	.data
1:	.asciz	"copyin: src=%x dest=%x len=%x\n"
	_ALIGN
	.text
#endif
	sethi	%hi(CPCB), %o3
	wr	%g0, ASI_AIUS, %asi
	LDPTR	[%o3 + %lo(CPCB)], %o3
	set	Lcopyfault, %o4
!	mov	%o7, %g7		! save return address
	membar	#Sync
	STPTR	%o4, [%o3 + PCB_ONFAULT]
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
	sethi	%hi(CPCB), %o3
!	stb	%o4,[%o1]	! Store last byte -- should not be needed
	LDPTR	[%o3 + %lo(CPCB)], %o3
	membar	#Sync
	STPTR	%g0, [%o3 + PCB_ONFAULT]
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore ASI
	retl
	 clr	%o0			! return 0

/*
 * copyout(src, dst, len)
 *
 * Copy specified amount of data from kernel to user space.
 * Just like copyin, except that the `dst' addresses are user space
 * rather than the `src' addresses.
 *
 * This is a modified version of memcpy that uses ASI_AIUS.  When
 * memcpy is optimized to use block copy ASIs, this should be also.
 */
 /*
  * This needs to be reimplemented to really do the copy.
  */
ENTRY(copyout)
	/*
	 * ******NOTE****** this depends on memcpy() not using %g7
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
	restore
	.data
1:	.asciz	"copyout: src=%x dest=%x len=%x ctx=%d\n"
	_ALIGN
	.text
#endif
Ldocopy:
	sethi	%hi(CPCB), %o3
	wr	%g0, ASI_AIUS, %asi
	LDPTR	[%o3 + %lo(CPCB)], %o3
	set	Lcopyfault, %o4
!	mov	%o7, %g7		! save return address
	membar	#Sync
	STPTR	%o4, [%o3 + PCB_ONFAULT]
	cmp	%o2, BCOPY_SMALL
Lcopyout_start:
	membar	#StoreStore
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
	sethi	%hi(CPCB), %o3
	LDPTR	[%o3 + %lo(CPCB)], %o3
	membar	#Sync
	STPTR	%g0, [%o3 + PCB_ONFAULT]
!	jmp	%g7 + 8		! Original instr
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore ASI
	membar	#StoreStore|#StoreLoad
	retl			! New instr
	 clr	%o0			! return 0

! Copyin or copyout fault.  Clear cpcb->pcb_onfault and return EFAULT.
! Note that although we were in memcpy, there is no state to clean up;
! the only special thing is that we have to return to [g7 + 8] rather than
! [o7 + 8].
Lcopyfault:
	sethi	%hi(CPCB), %o3
	LDPTR	[%o3 + %lo(CPCB)], %o3
	STPTR	%g0, [%o3 + PCB_ONFAULT]
	membar	#StoreStore|#StoreLoad
#ifdef NOTDEF_DEBUG
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	call	printf
	 nop
	restore
	.data
1:	.asciz	"copyfault: fault occurred\n"
	_ALIGN
	.text
#endif
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore ASI
	retl
	 mov	EFAULT, %o0


/*
 * Switch statistics (for later tweaking):
 *	nswitchdiff = p1 => p2 (i.e., chose different process)
 *	nswitchexit = number of calls to switchexit()
 *	_cnt.v_swtch = total calls to swtch+swtchexit
 */
	.data
	_ALIGN
	.comm	_C_LABEL(nswitchdiff), 4
	.comm	_C_LABEL(nswitchexit), 4
	.text
/*
 * REGISTER USAGE IN cpu_switch AND switchexit:
 * This is split into two phases, more or less
 * `before we locate a new proc' and `after'.
 * Some values are the same in both phases.
 * Note that the %o0-registers are not preserved across
 * the psr change when entering a new process, since this
 * usually changes the CWP field (hence heavy usage of %g's).
 *
 *	%l1 = <free>; newpcb
 *	%l2 = %hi(_whichqs); newpsr
 *	%l3 = p
 *	%l4 = lastproc
 *	%l5 = cpcb
 *	%l6 = %hi(cpcb)
 *	%l7 = %hi(curlwp)
 *	%o0 = tmp 1
 *	%o1 = tmp 2
 *	%o2 = tmp 3
 *	%o3 = tmp 4; whichqs; vm
 *	%o4 = tmp 4; which; sswap
 *	%o5 = tmp 5; q; <free>
 */

/*
 * cpu_exit is called as the last action during exit, before the current
 * process has freed its vmspace and kernel stack; we must schedule them
 * to be freed.  (curlwp is already NULL.)
 *
 * We lay the process to rest by changing to the `idle' kernel stack,
 * and note that the `last loaded process' is nonexistent.
 */
ENTRY(cpu_exit)
	flushw				! We don't have anything else to run, so why not
	wrpr	%g0, PSTATE_KERN, %pstate ! Make sure we're on the right globals
	mov	%o0, %l2		! save l arg for lwp_exit2() call

#if KTR_COMPILE & KTR_TRAP
	CATR(KTR_TRAP, "cpu_exit:",
		 %g2, %g3, %g4, 10, 11, 12)
12:
#endif
	/*
	 * Change pcb to idle u. area, i.e., set %sp to top of stack and
	 * %psr to PSR_S|PSR_ET, and set cpcb to point to curcpu()->ci_idle_u.
	 * Once we have left the old stack, we can free it.
	 * Free it any sooner and the register windows go bye-bye.
	 */
	sethi	%hi(IDLE_U), %l1
	LDPTR	[%l1 + %lo(IDLE_U)], %l1
	sethi	%hi(CPCB), %l6

	STPTR	%l1, [%l6 + %lo(CPCB)]	! cpcb = curcpu()->ci_idle_u
	set	USPACE - CC64FSZ, %o0	! set new %sp
	add	%l1, %o0, %o0
#ifdef _LP64
	sub	%o0, BIAS, %sp		! Maybe this should be a save?
#else
	mov	%o0, %sp		! Maybe this should be a save?
#endif
	wrpr	%g0, 0, %canrestore
	wrpr	%g0, 0, %otherwin
	rdpr	%ver, %l7
	and	%l7, CWP, %l7
	wrpr	%l7, 0, %cleanwin
	dec	1, %l7				! NWINDOWS - 1 - 1
	wrpr	%l7, %cansave
	clr	%fp				! End of stack.
#ifdef DEBUG
	flushw					! DEBUG
	sethi	%hi(IDLE_U), %l6
	LDPTR	[%l6 + %lo(IDLE_U)], %l6
	SET_SP_REDZONE(%l6, %o0)
#endif

	wrpr	%g0, PSTATE_INTR, %pstate	! and then enable traps
	call	_C_LABEL(lwp_exit2)		! lwp_exit2(l)
	 mov	%l2, %o0

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	call	_C_LABEL(sched_lock_idle)	! Acquire sched_lock
#endif

	 wrpr	%g0, PIL_SCHED, %pil		! Set splsched()

	/*
	 * Now fall through to `the last switch'.  %g6 was set to
	 * %hi(cpcb), but may have been clobbered in kmem_free,
	 * so all the registers described below will be set here.
	 *
	 * Since the process has exited we can blow its context
	 * out of the MMUs now to free up those TLB entries rather
	 * than have more useful ones replaced.
	 *
	 * REGISTER USAGE AT THIS POINT:
	 *	%l2 = %hi(_whichqs)
	 *	%l4 = lastproc
	 *	%l5 = cpcb
	 *	%l6 = %hi(cpcb)
	 *	%l7 = %hi(curlwp)
	 *	%o0 = tmp 1
	 *	%o1 = tmp 2
	 *	%o3 = whichqs
	 */

	INCR(_C_LABEL(nswitchexit))		! nswitchexit++;
	INCR(_C_LABEL(uvmexp)+V_SWTCH)		! cnt.v_switch++;

	mov	CTX_SECONDARY, %o0
	sethi	%hi(_C_LABEL(sched_whichqs)), %l2
	sethi	%hi(CPCB), %l6
	sethi	%hi(CURLWP), %l7
	ldxa	[%o0] ASI_DMMU, %l1		! Don't demap the kernel
#if 0
	LDPTR	[%l6 + %lo(CPCB)], %l5
#endif
	clr	%l4				! lastproc = NULL;
	brz,pn	%l1, 1f
#ifdef SPITFIRE
	 mov	DEMAP_CTX_SECONDARY, %l1	! Demap secondary context
	stxa	%g1, [%l1] ASI_DMMU_DEMAP
	stxa	%g1, [%l1] ASI_IMMU_DEMAP
	membar	#Sync
#else
	 mov	CTX_PRIMARY, %o0
	wrpr	%g0, 1, %tl
	stxa	%l1, [%o0] ASI_DMMU
	set	DEMAP_CTX_PRIMARY, %l1		! Demap secondary context
	stxa	%g1, [%l1] ASI_DMMU_DEMAP
	stxa	%g1, [%l1] ASI_IMMU_DEMAP
	membar	#Sync
	stxa	%g0, [%o0] ASI_DMMU
	membar	#Sync
	wrpr	%g0, 0, %tl
#endif
1:
	stxa	%g0, [%o0] ASI_DMMU		! Clear out our context
	membar	#Sync
	/* FALLTHROUGH */

/*
 * When no processes are on the runq, switch
 * idles here waiting for something to come ready.
 * The registers are set up as noted above.
 * We are running on this CPU's idle stack.
 *
 * we expect the follow at this point:
 *	%l4 to be NULL
 *	%l6 to be %hi(cpcb)
 *	%l7 to be %hi(curlwp)
 */
ENTRY_NOPROFILE(idle_switch)
	sethi	%hi(IDLE_U), %l1
	LDPTR	[%l1 + %lo(IDLE_U)], %l1

	STPTR	%l1, [%l6 + %lo(CPCB)]		! cpcb = curcpu()->ci_idle_u
	set	USPACE - CC64FSZ - 80, %o0	! set new %sp
	mov	%l1, %l5
	add	%l1, %o0, %o0
#ifdef _LP64
	sub	%o0, BIAS, %sp
#else
	mov	%o0, %sp
#endif

ENTRY_NOPROFILE(idle)
#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	call	_C_LABEL(sched_unlock_idle)	! Release sched_lock
#endif
idle_nolock:
	 STPTR	%g0, [%l7 + %lo(CURLWP)] ! curlwp = NULL;

#if KTR_COMPILE & KTR_PROC
	CATR(KTR_TRAP, "idle: pcb %p, idle_u %p",
		 %g2, %g3, %g4, 10, 11, 12)
	stx	%l5, [%g2 + KTR_PARM1]
	stx	%l1, [%g2 + KTR_PARM2]
12:
#endif
1:					! spin reading _whichqs until nonzero
	wrpr	%g0, PSTATE_INTR, %pstate ! Make sure interrupts are enabled
	wrpr	%g0, 0, %pil		! (void) spl0();

	ld	[%l2 + %lo(_C_LABEL(sched_whichqs))], %o3
	brnz,pt	%o3, notidle		! Something to run
	 nop
#ifdef UVM_PAGE_IDLE_ZERO
	! Check uvm.page_idle_zero
	sethi	%hi(_C_LABEL(uvm) + UVM_PAGE_IDLE_ZERO), %o3
	ld	[%o3 + %lo(_C_LABEL(uvm) + UVM_PAGE_IDLE_ZERO)], %o3
	brz,pn	%o3, 1b
	 nop

	! zero some pages
	call	_C_LABEL(uvm_pageidlezero)
	 nop
#endif
	ba,a,pt	%xcc, 1b
	 nop				! spitfire bug
notidle:
	wrpr	%g0, PIL_SCHED, %pil	! (void) splhigh();
#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	call	_C_LABEL(sched_lock_idle)	! Grab sched_lock
	 add	%o7, (Lsw_scan-.-4), %o7	! Return to Lsw_scan directly
#endif
	ba,a,pt	%xcc, Lsw_scan
	 nop				! spitfire bug

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
	mov	%l3, %o1
	call	_C_LABEL(panic)
	 or	%lo(3f), %o0, %o0
	.data
1:	.asciz	"switch rq"
2:	.asciz	"switch wchan"
3:	.asciz	"switch LSRUN %p %x"
	_ALIGN
	.text

	
/*
 * cpu_switch() picks a lwp to run and runs it, saving the current
 * one away.  On the assumption that (since most workstations are
 * single user machines) the chances are quite good that the new
 * lwp will turn out to be the current lwp, we defer saving
 * it here until we have found someone to load.  If that someone
 * is the current lwp we avoid both store and load.
 *
 * cpu_switch() is always entered at splstatclock or splhigh.
 *
 * IT MIGHT BE WORTH SAVING BEFORE ENTERING idle TO AVOID HAVING TO
 * SAVE LATER WHEN SOMEONE ELSE IS READY ... MUST MEASURE!
 */
	.globl	_C_LABEL(time)
ENTRY(cpu_switch)
	/*
	 * REGISTER USAGE AT THIS POINT:
	 *	%l1 = tmp 0
	 *	%l2 = %hi(_C_LABEL(whichqs))
	 *	%l3 = lwp
	 *	%l4 = lastlwp
	 *	%l5 = cpcb
	 *	%l6 = %hi(CPCB)
	 *	%l7 = %hi(CURLWP)
	 *	%o0 = tmp 1
	 *	%o1 = tmp 2
	 *	%o2 = tmp 3
	 *	%o3 = tmp 4, then at Lsw_scan, whichqs
	 *	%o4 = tmp 5, then at Lsw_scan, which
	 *	%o5 = tmp 6, then at Lsw_scan, q
	 */
	save	%sp, -CC64FSZ, %sp
	mov	%i0, %l4			! save lwp
#ifdef NOTDEF_DEBUG
	set	_C_LABEL(intrdebug), %l1
	mov	INTRDEBUG_FUNC, %o1
	st	%o1, [%l1]
#endif
	flushw			! XXX We don't have anything else to run, so why not flush

	rdpr	%pstate, %o1		! oldpstate = %pstate;
	wrpr	%g0, PSTATE_INTR, %pstate ! make sure we're on normal globals
	sethi	%hi(CPCB), %l6
	sethi	%hi(_C_LABEL(sched_whichqs)), %l2	! set up addr regs
	LDPTR	[%l6 + %lo(CPCB)], %l5

	sethi	%hi(CURLWP), %l7
	sth	%o1, [%l5 + PCB_PSTATE]		! cpcb->pcb_pstate = oldpstate;

	STPTR	%g0, [%l7 + %lo(CURLWP)]	! curlwp = NULL;

	stx	%i7, [%l5 + PCB_PC]		! cpcb->pcb_pc = pc;
	stx	%i6, [%l5 + PCB_SP]

#if KTR_COMPILE & KTR_PROC
	CATR(KTR_TRAP, "cpu_switch: %p",
		 %g2, %g3, %g1, 10, 11, 12)
	stx	%l4, [%g2 + KTR_PARM1]
12:
#endif

#if defined(MULTIPROCESSOR)
	ld	[%l2 + %lo(_C_LABEL(sched_whichqs))], %o3
	brz,pt	%o3, idle_switch
	 clr	%l4				! lastproc = NULL;
#endif

Lsw_scan:
	ld	[%l2 + %lo(_C_LABEL(sched_whichqs))], %o3

#ifndef POPC
	.globl	_C_LABEL(__ffstab)
	/*
	 * Optimized inline expansion of `which = ffs(whichqs) - 1';
	 * branches to idle if ffs(whichqs) was 0.
	 */
	set	_C_LABEL(__ffstab), %o2
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
	 * We found a nonempty run queue.  Take its first lwp.
	 */
	set	_C_LABEL(sched_qs), %o5	! q = &qs[which];
	sll	%o4, PTRSHFT+1, %o0
	add	%o0, %o5, %o5
	LDPTR	[%o5], %l3		! p = q->ph_link;
	cmp	%l3, %o5		! if (p == q)
	be,pn	%icc, Lsw_panic_rq	!	panic("switch rq");
	 EMPTY

Lcpu_ok:
	LDPTR	[%l3 + L_FORW], %o0	! tmp0 = l->l_forw;
	STPTR	%o0, [%o5]		! q->ph_link = tmp0;
	STPTR	%o5, [%o0 + L_BACK]	! tmp0->l_back = q;
	cmp	%o0, %o5		! if (tmp0 == q)
	bne	1f
	 EMPTY
	mov	1, %o1			!	whichqs &= ~(1 << which);
	sll	%o1, %o4, %o1
	andn	%o3, %o1, %o3
	st	%o3, [%l2 + %lo(_C_LABEL(sched_whichqs))]
1:
cpu_loadproc:
	/*
	 * PHASE TWO: NEW REGISTER USAGE:
	 *	%l1 = newpcb
	 *	%l2 = newpstate
	 *	%l3 = l
	 *	%l4 = lastlwp(proc)
	 *	%l5 = cpcb
	 *	%l6 = %hi(CPCB)
	 *	%l7 = %hi(CURLWP)
	 *	%o0 = tmp 1
	 *	%o1 = tmp 2
	 *	%o2 = tmp 3
	 *	%o3 = vm
	 *	%o4 = sswap
	 *	%o5 = <free>
	 */

	/* firewalls */
	LDPTR	[%l3 + L_WCHAN], %o0	! if (l->l_wchan)
	brnz,pn	%o0, Lsw_panic_wchan	!	panic("switch wchan");
	 EMPTY
	ld	[%l3 + L_STAT], %o2	! if (l->l_stat != LSRUN)
	cmp	%o2, LSRUN
	bne	Lsw_panic_srun		!	panic("switch LSRUN");
	 EMPTY

	/*
	 * Committed to running LWP l.
	 * It may be the same as the one we were running before.
	 */
#if defined(MULTIPROCESSOR)
	/*
	 * l->l_cpu = curcpu();
	 */
	set	CPUINFO_VA, %o0
	LDPTR	[%o0 + CI_SELF], %o0
	STPTR	%o0, [%l3 + L_CPU]
#endif
	mov	LSONPROC, %o0			! l->l_stat = SONPROC
	st	%o0, [%l3 + L_STAT]
	sethi	%hi(CPUINFO_VA+CI_WANT_RESCHED), %o0
	st	%g0, [%o0 + %lo(CPUINFO_VA+CI_WANT_RESCHED)]	! want_resched = 0;
	LDPTR	[%l3 + L_ADDR], %l1		! newpcb = l->l_addr;
	STPTR	%g0, [%l3 + L_BACK]		! l->l_back = NULL;
#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
	/*
	 * Done mucking with the run queues, release the
	 * scheduler lock, but keep interrupts out.
	 */
	call	_C_LABEL(sched_unlock_idle)
#endif
	 STPTR	%l3, [%l7 + %lo(CURLWP)]	! store new lwp

#if KTR_COMPILE & KTR_PROC
	CATR(KTR_TRAP, "cpu_switch: %p->%p",
		 %g2, %g3, %g1, 10, 11, 12)
	stx	%l4, [%g2 + KTR_PARM1]
	stx	%l3, [%g2 + KTR_PARM2]
12:
#endif

	cmp	%l3, %l4			! new lwp == curlwp?
#if !defined(MULTIPROCESSOR)
	be,a,pt	%xcc, Lsw_sameproc		! yes, go return 0
	 clr	%i0
#endif
	mov	1, %i0

	/*
	 * Not the old lwp.  Save the old lwp, if any;
	 * then load l.
	 */
	flushw				! DEBUG -- make sure we don't hold on to any garbage
	brz,pn	%l4, Lsw_load		! if no old lwp, go load
	 wrpr	%g0, PSTATE_KERN, %pstate

	INCR(_C_LABEL(nswitchdiff))	! clobbers %o0,%o1,%o2
wb1:
	flushw				! save all register windows except this one
	stx	%i7, [%l5 + PCB_PC]	! Save pc
	stx	%i6, [%l5 + PCB_SP]
	rdpr	%cwp, %o2		! Useless
	stb	%o2, [%l5 + PCB_CWP]
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_TRAP, "cpu_switch: (wb1) saved pc=%p, fp=%p",
		 %g2, %g3, %g1, 10, 11, 12)
	stx	%i7, [%g2 + KTR_PARM1]
	stx	%i6, [%g2 + KTR_PARM2]
12:
#endif

	/*
	 * Load the new lwp.  To load, we must change stacks and
	 * alter cpcb and the window control registers, hence we must
	 * disable interrupts.
	 *
	 * We also must load up the `in' and `local' registers.
	 */
Lsw_load:
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_TRAP, "cpu_switch: (Lsw_load) %p, pcb %p",
		 %g2, %g3, %g1, 10, 11, 12)
	stx	%l3, [%g2 + KTR_PARM1]
	stx	%l1, [%g2 + KTR_PARM2]
12:
#endif
	/* set new cpcb */
	STPTR	%l1, [%l6 + %lo(CPCB)]		! cpcb = newpcb;

	ldx	[%l1 + PCB_SP], %i6
	ldx	[%l1 + PCB_PC], %i7
	wrpr	%g0, 0, %otherwin	! These two insns should be redundant
	wrpr	%g0, 0, %canrestore
	rdpr	%ver, %l7
	and	%l7, CWP, %l7
	wrpr	%g0, %l7, %cleanwin
!	wrpr	%g0, 0, %cleanwin	! DEBUG
	dec	1, %l7					! NWINDOWS-1-1
	wrpr	%l7, %cansave
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_TRAP, "cpu_switch: new lwp pc=%p, sp=%p",
		 %g2, %g3, %g1, 10, 11, 12)
	stx	%i7, [%g2 + KTR_PARM1]
	stx	%i6, [%g2 + KTR_PARM2]
12:
#endif
#ifdef DEBUG
	wrpr	%g0, 4, %tl				! DEBUG -- force watchdog
	flushw						! DEBUG
	wrpr	%g0, 0, %tl				! DEBUG
	/* load window */
!	restore				! The logic is just too complicated to handle here.  Let the traps deal with the problem
!	flushw						! DEBUG
#endif
#ifdef SCHED_DEBUG
	mov	%fp, %i1
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	call	printf
	 mov	%i1, %o1
	LOCTOGLOB
	restore
	.data
1:	.asciz	"cpu_switch: setup new lwp stack regs at %08x\r\n"
	_ALIGN
	.text
#endif

#ifdef DEBUG
	mov	%l1, %o0
	SET_SP_REDZONE(%o0, %o1)
	CHECK_SP_REDZONE(%o0, %o1)
#endif
	/* finally, enable traps */
	wrpr	%g0, PSTATE_INTR, %pstate

	/*
	 * Now running p.  Make sure it has a context so that it
	 * can talk about user space stuff.  (Its pcb_uw is currently
	 * zero so it is safe to have interrupts going here.)
	 */
	LDPTR	[%l3 + L_PROC], %l4		! now %l4 points to p
	LDPTR	[%l4 + P_VMSPACE], %o3		! vm = p->p_vmspace;
	sethi	%hi(_C_LABEL(kernel_pmap_)), %o1
	mov	CTX_SECONDARY, %l5		! Recycle %l5
	LDPTR	[%o3 + VM_PMAP], %o2		! if (vm->vm_pmap.pm_ctx != NULL)
	or	%o1, %lo(_C_LABEL(kernel_pmap_)), %o1
	cmp	%o2, %o1
	bz,pn	%xcc, Lsw_havectx		! Don't replace kernel context!
	 ld	[%o2 + PM_CTX], %o0
	brnz,pt	%o0, Lsw_havectx		!	goto havecontext;
	 nop

	/* p does not have a context: call ctx_alloc to get one */
	call	_C_LABEL(ctx_alloc)		! ctx_alloc(&vm->vm_pmap);
	 mov	%o2, %o0

#if KTR_COMPILE & KTR_PROC
	CATR(KTR_TRAP, "cpu_switch: new ctx %d",
		 %g2, %g3, %g1, 10, 11, 12)
	stx	%o0, [%g2 + KTR_PARM1]
12:
#endif
#ifdef SCHED_DEBUG
	mov	%o0, %g1
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	call	printf
 	 mov	%g1, %o1
	LOCTOGLOB
	restore
	.data
1:	.asciz	"cpu_switch: got new ctx %d in new lwp\r\n"
	_ALIGN
	.text
#endif

	/* p does have a context: just switch to it */
Lsw_havectx:
	! context is in %o0
	/*
	 * We probably need to flush the cache here.
	 */
	stxa	%o0, [%l5] ASI_DMMU		! Maybe we should invalidate the old context?
	membar	#Sync				! Maybe we should use flush here?
	flush	%sp

#ifdef SCHED_DEBUG
	mov	%o0, %g1
	mov	%i7, %g1
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	mov	%g1, %o2
	call	printf
	 mov	%g2, %o1
	LOCTOGLOB
	restore
	.data
1:	.asciz	"cpu_switch: in new lwp pc=%08x ctx %d\r\n"
	_ALIGN
	.text
#endif

	/*
	 * Check for restartable atomic sequences (RAS)
	 */
	mov	%l4, %o0		! p is first arg to ras_lookup
	LDPTR	[%o0 + P_RASLIST], %o1	! any RAS in p?
	brz,pt	%o1, Lsw_noras		! no, skip RAS check
	 LDPTR	[%l3 + L_TF], %l3	! pointer to trap frame
	call	_C_LABEL(ras_lookup)
	 LDPTR	[%l3 + TF_PC], %o1
	cmp	%o0, -1
	be,pt	%xcc, Lsw_noras
	 add	%o0, 4, %o1
	STPTR	%o0, [%l3 + TF_PC]	! store rewound %pc
	STPTR	%o1, [%l3 + TF_NPC]	! and %npc
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_TRAP, "cpu_switch: ras pc=%p",
		 %g2, %g3, %g1, 10, 11, 12)
	stx	%o0, [%g2 + KTR_PARM1]
12:
#endif

Lsw_noras:

Lsw_sameproc:
	/*
	 * We are resuming the process that was running at the
	 * call to switch().  Just set psr ipl and return.
	 */
#ifdef SCHED_DEBUG
	mov	%l0, %o0		! XXXXX
	save	%sp, -CC64FSZ, %sp
	GLOBTOLOC
	set	1f, %o0
	mov	%i0, %o2
	set	CURLWP, %o3
	LDPTR	[%o3], %o3
	ld	[%o3 + P_VMSPACE], %o3
	call	printf
	 mov	%i7, %o1
#ifdef DEBUG
	set	swtchdelay, %o0
	call	delay
	 ld	[%o0], %o0
	set	pmapdebug, %o0
	ld	[%o0], %o0
	tst	%o0
	tnz	%icc, 1; nop	! Call debugger if we're in pmapdebug
#endif
	LOCTOGLOB
	ba	2f		! Skip debugger
	 restore
	.data
1:	.asciz	"cpu_switch: vectoring to pc=%08x thru %08x vmspace=%p\r\n"
	_ALIGN
	.globl	swtchdelay
swtchdelay:
	.word	1000
	.text
	Debugger();
2:
#endif

!	wrpr	%g0, 0, %cleanwin	! DEBUG
	clr	%g4		! This needs to point to the base of the data segment
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore default ASI
	wrpr	%g0, PSTATE_INTR, %pstate
	ret
	 restore

/*
 * cpu_switchto(struct lwp *current, struct lwp *next)
 * Switch to the specified next LWP
 * Arguments:
 *	i0	'struct lwp *' of the current LWP
 *	i1	'struct lwp *' of the LWP to switch to
 */
ENTRY(cpu_switchto)
	save	%sp, -CC64FSZ, %sp
	/*
	 * REGISTER USAGE AT THIS POINT:
	 *	%l1 = tmp 0
	 *	%l3 = newlwp
	 *	%l4 = lastlwp
	 *	%l5 = cpcb
	 *	%l6 = %hi(CPCB)
	 *	%l7 = %hi(CURLWP)
	 *	%o0 = tmp 1
	 *	%o1 = tmp 2
	 *	%o2 = tmp 3
	 */
	flushw	
	rdpr	%pstate, %o1			! oldpstate = %pstate;
	wrpr	%g0, PSTATE_KERN, %pstate	! make sure we're on normal globals
	sethi	%hi(CPCB), %l6
	mov	%i1, %l3			! new lwp -> %l3

	sethi	%hi(CURLWP), %l7
	LDPTR	[%l6 + %lo(CPCB)], %l5

	stx	%o7, [%l5 + PCB_PC]		! cpcb->pcb_pc = pc;
	stx	%o6, [%l5 + PCB_SP]		! cpcb->pcb_sp = sp;

	mov	%i0, %l4			! lastproc = curlwp; (hope he's right)
	sth	%o1, [%l5 + PCB_PSTATE]		! cpcb->pcb_pstate = oldpstate;

	STPTR	%g0, [%l7 + %lo(CURLWP)]	! curlwp = NULL;

	ba,pt	%icc, cpu_loadproc
	 nop

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
 * cpu_setfunc() and cpu_lwp_fork() arrange for proc_trampoline() to run
 * after after a process gets chosen in switch(). The stack frame will
 * contain a function pointer in %l0, and an argument to pass to it in %l1.
 *
 * If the function *(%l0) returns, we arrange for an immediate return
 * to user mode. This happens in two known cases: after execve(2) of init,
 * and when returning a child to user mode after a fork(2).
 */
ENTRY(proc_trampoline)
#ifdef SCHED_DEBUG
	nop; nop; nop; nop				! Try to make sure we don't vector into the wrong instr
	mov	%l0, %o0
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	mov	%i6, %o2
	call	printf
	 mov	%i0, %o1
	ba	2f
	 restore
	.data
1:	.asciz	"proc_trampoline: calling %x sp %x\r\n"
	_ALIGN
	.text
	Debugger()
2:
#endif

#ifdef MULTIPROCESSOR
	/* Finish setup in SMP environment: acquire locks etc. */
	call _C_LABEL(proc_trampoline_mp)
	 nop
#endif

	wrpr	%g0, 0, %pil		! Reset interrupt level
	call	%l0			! re-use current frame
	 mov	%l1, %o0

	/*
	 * Here we finish up as in syscall, but simplified.
	 */
	ldx	[%sp + CC64FSZ + STKB + TF_TSTATE], %g1	! Load this for return_from_trap
#ifdef SCHED_DEBUG
	ldx	[%sp + CC64FSZ + STKB + TF_PC], %g2	! pc = tf->tf_pc from execve/fork
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	ldx	[%fp + CC64FSZ + STKB + TF_O + ( 6*8)], %o2
	mov	%fp, %o2
	add	%fp, CC64FSZ + STKB, %o3
	GLOBTOLOC
	call	printf
	 mov	%g2, %o1
	LOCTOGLOB
	set	3f, %o0
	mov	%g1, %o1
	mov	%g2, %o2
	mov	CTX_SECONDARY, %o4
	ldxa	[%o4] ASI_DMMU, %o4
	call	printf
	 mov	%g3, %o3
	LOCTOGLOB
	ba 2f
	restore
	.data
1:	.asciz	"proc_trampoline: returning to %p, sp=%p, tf=%p\r\n"
3:	.asciz	"tstate=%p tpc=%p tnpc=%p ctx=%x\r\n"
	_ALIGN
	.text
	Debugger()
2:
#endif
	CHKPT(%o3,%o4,0x35)
	ba,a,pt	%icc, return_from_trap
	 nop

/*
 * {fu,su}{,i}{byte,word}
 */
ALTENTRY(fuiword)
ENTRY(fuword)
	btst	3, %o0			! has low bits set...
	bnz	Lfsbadaddr		!	go return -1
	EMPTY
	sethi	%hi(CPCB), %o2		! cpcb->pcb_onfault = Lfserr;
	set	Lfserr, %o3
	LDPTR	[%o2 + %lo(CPCB)], %o2
	membar	#LoadStore
	STPTR	%o3, [%o2 + PCB_ONFAULT]
	membar	#Sync
	LDPTRA	[%o0] ASI_AIUS, %o0	! fetch the word
	membar	#Sync
	STPTR	%g0, [%o2 + PCB_ONFAULT]! but first clear onfault
	retl				! phew, made it, return the word
	 membar	#StoreStore|#StoreLoad

Lfserr:
	STPTR	%g0, [%o2 + PCB_ONFAULT]! error in r/w, clear pcb_onfault
	membar	#StoreStore|#StoreLoad
Lfsbadaddr:
#ifndef _LP64
	mov	-1, %o1
#endif
	retl				! and return error indicator
	 mov	-1, %o0

	/*
	 * This is just like Lfserr, but it's a global label that allows
	 * mem_access_fault() to check to see that we don't want to try to
	 * page in the fault.  It's used by fuswintr() etc.
	 */
	.globl	_C_LABEL(Lfsbail)
_C_LABEL(Lfsbail):
	STPTR	%g0, [%o2 + PCB_ONFAULT]! error in r/w, clear pcb_onfault
	membar	#StoreStore|#StoreLoad
	retl				! and return error indicator
	 mov	-1, %o0

	/*
	 * Like fusword but callable from interrupt context.
	 * Fails if data isn't resident.
	 */
ENTRY(fuswintr)
	sethi	%hi(CPCB), %o2		! cpcb->pcb_onfault = _Lfsbail;
	LDPTR	[%o2 + %lo(CPCB)], %o2
	set	_C_LABEL(Lfsbail), %o3
	STPTR	%o3, [%o2 + PCB_ONFAULT]
	membar	#Sync
	lduha	[%o0] ASI_AIUS, %o0	! fetch the halfword
	membar	#Sync
	STPTR	%g0, [%o2 + PCB_ONFAULT]! but first clear onfault
	retl				! made it
	 membar	#StoreStore|#StoreLoad

ENTRY(fusword)
	sethi	%hi(CPCB), %o2		! cpcb->pcb_onfault = Lfserr;
	LDPTR	[%o2 + %lo(CPCB)], %o2
	set	Lfserr, %o3
	STPTR	%o3, [%o2 + PCB_ONFAULT]
	membar	#Sync
	lduha	[%o0] ASI_AIUS, %o0		! fetch the halfword
	membar	#Sync
	STPTR	%g0, [%o2 + PCB_ONFAULT]! but first clear onfault
	retl				! made it
	 membar	#StoreStore|#StoreLoad

ALTENTRY(fuibyte)
ENTRY(fubyte)
	sethi	%hi(CPCB), %o2		! cpcb->pcb_onfault = Lfserr;
	LDPTR	[%o2 + %lo(CPCB)], %o2
	set	Lfserr, %o3
	STPTR	%o3, [%o2 + PCB_ONFAULT]
	membar	#Sync
	lduba	[%o0] ASI_AIUS, %o0	! fetch the byte
	membar	#Sync
	STPTR	%g0, [%o2 + PCB_ONFAULT]! but first clear onfault
	retl				! made it
	 membar	#StoreStore|#StoreLoad

ALTENTRY(suiword)
ENTRY(suword)
	btst	3, %o0			! or has low bits set ...
	bnz	Lfsbadaddr		!	go return error
	EMPTY
	sethi	%hi(CPCB), %o2		! cpcb->pcb_onfault = Lfserr;
	LDPTR	[%o2 + %lo(CPCB)], %o2
	set	Lfserr, %o3
	STPTR	%o3, [%o2 + PCB_ONFAULT]
	membar	#Sync
	STPTRA	%o1, [%o0] ASI_AIUS	! store the word
	membar	#Sync
	STPTR	%g0, [%o2 + PCB_ONFAULT]! made it, clear onfault
	membar	#StoreStore|#StoreLoad
	retl				! and return 0
	 clr	%o0

ENTRY(suswintr)
	sethi	%hi(CPCB), %o2		! cpcb->pcb_onfault = _Lfsbail;
	LDPTR	[%o2 + %lo(CPCB)], %o2
	set	_C_LABEL(Lfsbail), %o3
	STPTR	%o3, [%o2 + PCB_ONFAULT]
	membar	#Sync
	stha	%o1, [%o0] ASI_AIUS	! store the halfword
	membar	#Sync
	STPTR	%g0, [%o2 + PCB_ONFAULT]! made it, clear onfault
	membar	#StoreStore|#StoreLoad
	retl				! and return 0
	 clr	%o0

ENTRY(susword)
	sethi	%hi(CPCB), %o2		! cpcb->pcb_onfault = Lfserr;
	LDPTR	[%o2 + %lo(CPCB)], %o2
	set	Lfserr, %o3
	STPTR	%o3, [%o2 + PCB_ONFAULT]
	membar	#Sync
	stha	%o1, [%o0] ASI_AIUS	! store the halfword
	membar	#Sync
	STPTR	%g0, [%o2 + PCB_ONFAULT]! made it, clear onfault
	membar	#StoreStore|#StoreLoad
	retl				! and return 0
	 clr	%o0

ALTENTRY(suibyte)
ENTRY(subyte)
	sethi	%hi(CPCB), %o2		! cpcb->pcb_onfault = Lfserr;
	LDPTR	[%o2 + %lo(CPCB)], %o2
	set	Lfserr, %o3
	STPTR	%o3, [%o2 + PCB_ONFAULT]
	membar	#Sync
	stba	%o1, [%o0] ASI_AIUS	! store the byte
	membar	#Sync
	STPTR	%g0, [%o2 + PCB_ONFAULT]! made it, clear onfault
	membar	#StoreStore|#StoreLoad
	retl				! and return 0
	 clr	%o0

/* probeget and probeset are meant to be used during autoconfiguration */
/*
 * The following probably need to be changed, but to what I don't know.
 */

/*
 * uint64_t
 * probeget(addr, asi, size)
 *	paddr_t addr;
 *	int asi;
 *	int size;
 *
 * Read or write a (byte,word,longword) from the given address.
 * Like {fu,su}{byte,halfword,word} but our caller is supposed
 * to know what he is doing... the address can be anywhere.
 *
 * We optimize for space, rather than time, here.
 */
ENTRY(probeget)
#ifndef _LP64
	!! Shuffle the args around into LP64 format
	COMBINE(%o0, %o1, %o0)
	mov	%o2, %o1
	mov	%o3, %o2
#endif
	mov	%o2, %o4
	! %o0 = addr, %o1 = asi, %o4 = (1,2,4)
	sethi	%hi(CPCB), %o2
	LDPTR	[%o2 + %lo(CPCB)], %o2	! cpcb->pcb_onfault = Lfserr;
#ifdef _LP64
	set	_C_LABEL(Lfsbail), %o5
#else
	set	_C_LABEL(Lfsprobe), %o5
#endif
	STPTR	%o5, [%o2 + PCB_ONFAULT]
	or	%o0, 0x9, %o3		! if (PHYS_ASI(asi)) {
	sub	%o3, 0x1d, %o3
	brz,a	%o3, 0f
	 mov	%g0, %o5
	DLFLUSH(%o0,%o5)		!	flush cache line
					! }
0:
#ifndef _LP64
	rdpr	%pstate, %g1
	wrpr	%g1, PSTATE_AM, %pstate
#endif
	btst	1, %o4
	wr	%o1, 0, %asi
	membar	#Sync
	bz	0f			! if (len & 1)
	 btst	2, %o4
	ba,pt	%icc, 1f
	 lduba	[%o0] %asi, %o0		!	value = *(char *)addr;
0:
	bz	0f			! if (len & 2)
	 btst	4, %o4
	ba,pt	%icc, 1f
	 lduha	[%o0] %asi, %o0		!	value = *(short *)addr;
0:
	bz	0f			! if (len & 4)
	 btst	8, %o4
	ba,pt	%icc, 1f
	 lda	[%o0] %asi, %o0		!	value = *(int *)addr;
0:
	ldxa	[%o0] %asi, %o0		!	value = *(long *)addr;
1:	
#ifndef _LP64
	SPLIT(%o0, %o1)
#endif
	membar	#Sync
#ifndef _LP64
	wrpr	%g1, 0, %pstate
#endif
	brz	%o5, 1f			! if (cache flush addr != 0)
	 nop
	DLFLUSH2(%o5)			!	flush cache line again
1:
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore default ASI	
	STPTR	%g0, [%o2 + PCB_ONFAULT]
	retl				! made it, clear onfault and return
	 membar	#StoreStore|#StoreLoad

	/*
	 * Fault handler for probeget
	 */
_C_LABEL(Lfsprobe):
#ifndef _LP64
	wrpr	%g1, 0, %pstate
#endif
	STPTR	%g0, [%o2 + PCB_ONFAULT]! error in r/w, clear pcb_onfault
	mov	-1, %o1
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore default ASI	
	membar	#StoreStore|#StoreLoad
	retl				! and return error indicator
	 mov	-1, %o0

/*
 * probeset(addr, asi, size, val)
 *	paddr_t addr;
 *	int asi;
 *	int size;
 *	long val;
 *
 * As above, but we return 0 on success.
 */
ENTRY(probeset)
#ifndef _LP64
	!! Shuffle the args around into LP64 format
	COMBINE(%o0, %o1, %o0)
	mov	%o2, %o1
	mov	%o3, %o2
	COMBINE(%o4, %o5, %o3)
#endif
	mov	%o2, %o4
	! %o0 = addr, %o1 = asi, %o4 = (1,2,4), %o3 = val
	sethi	%hi(CPCB), %o2		! Lfserr requires CPCB in %o2
	LDPTR	[%o2 + %lo(CPCB)], %o2	! cpcb->pcb_onfault = Lfserr;
	set	_C_LABEL(Lfsbail), %o5
	STPTR	%o5, [%o2 + PCB_ONFAULT]
	btst	1, %o4
	wr	%o1, 0, %asi
	membar	#Sync
	bz	0f			! if (len & 1)
	 btst	2, %o4
	ba,pt	%icc, 1f
	 stba	%o3, [%o0] %asi		!	*(char *)addr = value;
0:
	bz	0f			! if (len & 2)
	 btst	4, %o4
	ba,pt	%icc, 1f
	 stha	%o3, [%o0] %asi		!	*(short *)addr = value;
0:
	bz	0f			! if (len & 4)
	 btst	8, %o4
	ba,pt	%icc, 1f
	 sta	%o3, [%o0] %asi		!	*(int *)addr = value;
0:
	bz	Lfserr			! if (len & 8)
	ba,pt	%icc, 1f
	 sta	%o3, [%o0] %asi		!	*(int *)addr = value;
1:	membar	#Sync
	clr	%o0			! made it, clear onfault and return 0
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore default ASI	
	STPTR	%g0, [%o2 + PCB_ONFAULT]
	retl
	 membar	#StoreStore|#StoreLoad

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
	.data
paginuse:
	.word	0
	.text
ENTRY(pmap_zero_page)
#ifndef _LP64
	COMBINE(%o0, %o1, %o0)
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
	restore
	.data
2:	.asciz	"pmap_zero_page(%p)\n"
	_ALIGN
	.text
3:
#endif
	set	NBPG, %o2		! Loop count
	wr	%g0, ASI_PHYS_CACHED, %asi
1:
	/* Unroll the loop 8 times */
	stxa	%g0, [%o0 + 0x00] %asi
	deccc	0x40, %o2
	stxa	%g0, [%o0 + 0x08] %asi
	stxa	%g0, [%o0 + 0x10] %asi
	stxa	%g0, [%o0 + 0x18] %asi
	stxa	%g0, [%o0 + 0x20] %asi
	stxa	%g0, [%o0 + 0x28] %asi
	stxa	%g0, [%o0 + 0x30] %asi
	stxa	%g0, [%o0 + 0x38] %asi
	bg,pt	%icc, 1b
	 inc	0x40, %o0

	sethi	%hi(KERNBASE), %o3
	flush	%o3
	retl
	 wr	%g0, ASI_PRIMARY_NOFAULT, %asi	! Make C code happy

/*
 * pmap_copy_page(paddr_t src, paddr_t dst)
 *
 * Copy one page physically addressed
 * We need to use a global reg for ldxa/stxa
 * so the top 32-bits cannot be lost if we take
 * a trap and need to save our stack frame to a
 * 32-bit stack.  We will unroll the loop by 4 to
 * improve performance.
 *
 * XXX We also need to blast the D$ and flush like
 * XXX pmap_zero_page.
 */
ENTRY(pmap_copy_page)
#ifndef _LP64
	COMBINE(%o0, %o1, %o0)
	COMBINE(%o2, %o3, %o1)
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
	restore
	.data
2:	.asciz	"pmap_copy_page(%p,%p)\n"
	_ALIGN
	.text
3:
#endif
#if 1
	set	NBPG, %o2
	wr	%g0, ASI_PHYS_CACHED, %asi
1:
	ldxa	[%o0 + 0x00] %asi, %g1
	ldxa	[%o0 + 0x08] %asi, %o3
	ldxa	[%o0 + 0x10] %asi, %o4
	ldxa	[%o0 + 0x18] %asi, %o5
	inc	0x20, %o0
	deccc	0x20, %o2
	stxa	%g1, [%o1 + 0x00] %asi
	stxa	%o3, [%o1 + 0x08] %asi
	stxa	%o4, [%o1 + 0x10] %asi
	stxa	%o5, [%o1 + 0x18] %asi
	bg,pt	%icc, 1b		! We don't care about pages >4GB
	 inc	0x20, %o1
	retl
	 wr	%g0, ASI_PRIMARY_NOFAULT, %asi
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
	retl
	 mov	%o4, %g1		! Restore g1
#endif
/*
 * extern int64_t pseg_get(struct pmap *pm, vaddr_t addr);
 *
 * Return TTE at addr in pmap.  Uses physical addressing only.
 * pmap->pm_physaddr must by the physical address of pm_segs
 *
 */
ENTRY(pseg_get)
!	flushw			! Make sure we don't have stack probs & lose hibits of %o
	ldx	[%o0 + PM_PHYS], %o2			! pmap->pm_segs

	srax	%o1, HOLESHIFT, %o3			! Check for valid address
	brz,pt	%o3, 0f					! Should be zero or -1
	 inc	%o3					! Make -1 -> 0
	brnz,pn	%o3, 1f					! Error! In hole!
0:
	srlx	%o1, STSHIFT, %o3
	and	%o3, STMASK, %o3			! Index into pm_segs
	sll	%o3, 3, %o3
	add	%o2, %o3, %o2
	DLFLUSH(%o2,%o3)
	ldxa	[%o2] ASI_PHYS_CACHED, %o2		! Load page directory pointer
	DLFLUSH2(%o3)

	srlx	%o1, PDSHIFT, %o3
	and	%o3, PDMASK, %o3
	sll	%o3, 3, %o3
	brz,pn	%o2, 1f					! NULL entry? check somewhere else
	 add	%o2, %o3, %o2
	DLFLUSH(%o2,%o3)
	ldxa	[%o2] ASI_PHYS_CACHED, %o2		! Load page table pointer
	DLFLUSH2(%o3)

	srlx	%o1, PTSHIFT, %o3			! Convert to ptab offset
	and	%o3, PTMASK, %o3
	sll	%o3, 3, %o3
	brz,pn	%o2, 1f					! NULL entry? check somewhere else
	 add	%o2, %o3, %o2
	DLFLUSH(%o2,%o3)
	ldxa	[%o2] ASI_PHYS_CACHED, %o0
	DLFLUSH2(%o3)
	brgez,pn %o0, 1f				! Entry invalid?  Punt
	 btst	1, %sp
	bz,pn	%icc, 0f				! 64-bit mode?
	 nop
	retl						! Yes, return full value
	 nop
0:
#if 1
	srl	%o0, 0, %o1
	retl						! No, generate a %o0:%o1 double
	 srlx	%o0, 32, %o0
#else
	DLFLUSH(%o2,%o3)
	ldda	[%o2] ASI_PHYS_CACHED, %o0
	DLFLUSH2(%o3)
	retl						! No, generate a %o0:%o1 double
	 nop
#endif
1:
#ifndef _LP64
	clr	%o1
#endif
	retl
	 clr	%o0

/*
 * In 32-bit mode:
 *
 * extern int pseg_set(struct pmap* %o0, vaddr_t addr %o1, int64_t tte %o2:%o3,
 *			 paddr_t spare %o4:%o5);
 *
 * In 64-bit mode:
 *
 * extern int pseg_set(struct pmap* %o0, vaddr_t addr %o1, int64_t tte %o2,
 *			paddr_t spare %o3);
 *
 * Set a pseg entry to a particular TTE value.  Return values are:
 *
 *	-2	addr in hole
 *	0	success	(spare was not used if given)
 *	1	failure	(spare was not given, but one is needed)
 *	2	success	(spare was given, used for L2)
 *	3	failure	(spare was given, used for L2, another is needed for L3)
 *	4	success	(spare was given, used for L3)
 *
 *	rv == 0	success, spare not used if one was given
 *	rv & 4	spare was used for L3
 *	rv & 2	spare was used for L2
 *	rv & 1	failure, spare is needed
 *
 * (NB: nobody in pmap checks for the virtual hole, so the system will hang.)
 * The way to call this is:  first just call it without a spare page.
 * If that fails, allocate a page and try again, passing the paddr of the
 * new page as the spare.
 * If spare is non-zero it is assumed to be the address of a zeroed physical
 * page that can be used to generate a directory table or page table if needed.
 *
 * We keep track of valid (A_TLB_V bit set) and wired (A_TLB_TSB_LOCK bit set)
 * mappings that are set here. We check both bits on the new data entered
 * and increment counts, as well as decrementing counts if the bits are set
 * in the value replaced by this call.
 * The counters are 32 bit or 64 bit wide, depending on the kernel type we are
 * running!
 */
ENTRY(pseg_set)
#ifndef _LP64
	sllx	%o4, 32, %o4				! Put args into 64-bit format
	sllx	%o2, 32, %o2				! Shift to high 32-bits
	clruw	%o3					! Zero extend
	clruw	%o5
	clruw	%o1
	or	%o2, %o3, %o2
	or	%o4, %o5, %o3
#endif
	!!
	!! However we managed to get here we now have:
	!!
	!! %o0 = *pmap
	!! %o1 = addr
	!! %o2 = tte
	!! %o3 = paddr of spare page
	!!
	srax	%o1, HOLESHIFT, %o4			! Check for valid address
	brz,pt	%o4, 0f					! Should be zero or -1
	 inc	%o4					! Make -1 -> 0
	brz,pt	%o4, 0f
	 nop
#ifdef DEBUG
	ta	1					! Break into debugger
#endif
	retl
	 mov -2, %o0					! Error -- in hole!

0:
	ldx	[%o0 + PM_PHYS], %o4			! pmap->pm_segs
	clr	%g1
	srlx	%o1, STSHIFT, %o5
	and	%o5, STMASK, %o5
	sll	%o5, 3, %o5
	add	%o4, %o5, %o4
0:
	DLFLUSH(%o4,%g5)
	ldxa	[%o4] ASI_PHYS_CACHED, %o5		! Load page directory pointer
	DLFLUSH2(%g5)

	brnz,a,pt %o5, 0f				! Null pointer?
	 mov	%o5, %o4
	brz,pn	%o3, 9f					! Have a spare?
	 mov	%o3, %o5
	casxa	[%o4] ASI_PHYS_CACHED, %g0, %o5
	brnz,pn	%o5, 0b					! Something changed?
	DLFLUSH(%o4, %o5)
	mov	%o3, %o4
	mov	2, %g1					! record spare used for L2
	clr	%o3					! and not available for L3
0:
	srlx	%o1, PDSHIFT, %o5
	and	%o5, PDMASK, %o5
	sll	%o5, 3, %o5
	add	%o4, %o5, %o4
0:
	DLFLUSH(%o4,%g5)
	ldxa	[%o4] ASI_PHYS_CACHED, %o5		! Load table directory pointer
	DLFLUSH2(%g5)

	brnz,a,pt %o5, 0f				! Null pointer?
	 mov	%o5, %o4
	brz,pn	%o3, 9f					! Have a spare?
	 mov	%o3, %o5
	casxa	[%o4] ASI_PHYS_CACHED, %g0, %o5
	brnz,pn	%o5, 0b					! Something changed?
	DLFLUSH(%o4, %o4)
	mov	%o3, %o4
	mov	4, %g1					! record spare used for L3
0:
	srlx	%o1, PTSHIFT, %o5			! Convert to ptab offset
	and	%o5, PTMASK, %o5
	sll	%o5, 3, %o5
	add	%o5, %o4, %o4

	DLFLUSH(%o4,%g5)
	ldxa	[%o4] ASI_PHYS_CACHED, %o5		! save old value in %o5
	stxa	%o2, [%o4] ASI_PHYS_CACHED		! Easier than shift+or
	DLFLUSH2(%g5)

	!! at this point we have:
	!!  %g1 = return value
	!!  %o0 = struct pmap * (where the counts are)
	!!  %o2 = new TTE
	!!  %o5 = old TTE

	!! see if stats needs an update
	set	A_TLB_TSB_LOCK, %g5
	xor	%o2, %o5, %o3			! %o3 - what changed

	brgez,pn %o3, 5f			! has resident changed? (we predict it has)
	 btst	%g5, %o3			! has wired changed?

	LDPTR	[%o0 + PM_RESIDENT], %o1	! gonna update resident count
	brlz	%o2, 0f
	 mov	1, %o4
	neg	%o4				! new is not resident -> decrement
0:	add	%o1, %o4, %o1
	STPTR	%o1, [%o0 + PM_RESIDENT]
	btst	%g5, %o3			! has wired changed?
5:	bz,pt	%xcc, 8f			! we predict it's not
	 btst	%g5, %o2			! don't waste delay slot, check if new one is wired
	LDPTR	[%o0 + PM_WIRED], %o1		! gonna update wired count
	bnz,pt	%xcc, 0f			! if wired changes, we predict it increments
	 mov	1, %o4
	neg	%o4				! new is not wired -> decrement
0:	add	%o1, %o4, %o1
	STPTR	%o1, [%o0 + PM_WIRED]
8:	retl
	 mov	%g1, %o0			! return %g1

9:	retl
	 or	%g1, 1, %o0			! spare needed, return flags + 1


/*
 * Use block_disable to turn off block insns for
 * memcpy/memset
 */
	.data
	.align	8
	.globl	block_disable
block_disable:	.xword	1
	.text

#if 0
#define ASI_STORE	ASI_BLK_COMMIT_P
#else
#define ASI_STORE	ASI_BLK_P
#endif
	
#if 1
/*
 * kernel memcpy
 * Assumes regions do not overlap; has no useful return value.
 *
 * Must not use %g7 (see copyin/copyout above).
 */
ENTRY(memcpy) /* dest, src, size */
	/*
	 * Swap args for bcopy.  Gcc generates calls to memcpy for
	 * structure assignments.
	 */
	mov	%o0, %o3
	mov	%o1, %o0
	mov	%o3, %o1
#endif
! ENTRY(bcopy) /* src, dest, size */
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
	restore
	.data
2:	.asciz	"memcpy(%p<-%p,%x)\n"
	_ALIGN
	.text
3:
#endif

	cmp	%o2, BCOPY_SMALL

Lmemcpy_start:
	bge,pt	CCCR, 2f	! if >= this many, go be fancy.
	 cmp	%o2, 256

	mov	%o1, %o5	! Save memcpy return value
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
	 mov	%o5, %o0
	NOTREACHED

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
2:
#ifdef USE_BLOCK_STORE_LOAD
	! If it is big enough, use VIS instructions
	bge	Lmemcpy_block
	 nop
#endif /* USE_BLOCK_STORE_LOAD */
Lmemcpy_fancy:

	!!
	!! First align the output to a 8-byte entity
	!! 

	save	%sp, -CC64FSZ, %sp
	
	mov	%i0, %l0
	mov	%i1, %l1
	
	mov	%i2, %l2
	btst	1, %l1
	
	bz,pt	%icc, 4f
	 btst	2, %l1
	ldub	[%l0], %l4				! Load 1st byte
	
	deccc	1, %l2
	ble,pn	CCCR, Lmemcpy_finish			! XXXX
	 inc	1, %l0
	
	stb	%l4, [%l1]				! Store 1st byte
	inc	1, %l1					! Update address
	btst	2, %l1
4:	
	bz,pt	%icc, 4f
	
	 btst	1, %l0
	bz,a	1f
	 lduh	[%l0], %l4				! Load short

	ldub	[%l0], %l4				! Load bytes
	
	ldub	[%l0+1], %l3
	sllx	%l4, 8, %l4
	or	%l3, %l4, %l4
	
1:	
	deccc	2, %l2
	ble,pn	CCCR, Lmemcpy_finish			! XXXX
	 inc	2, %l0
	sth	%l4, [%l1]				! Store 1st short
	
	inc	2, %l1
4:
	btst	4, %l1
	bz,pt	CCCR, 4f
	
	 btst	3, %l0
	bz,a,pt	CCCR, 1f
	 lduw	[%l0], %l4				! Load word -1

	btst	1, %l0
	bz,a,pt	%icc, 2f
	 lduh	[%l0], %l4
	
	ldub	[%l0], %l4
	
	lduh	[%l0+1], %l3
	sllx	%l4, 16, %l4
	or	%l4, %l3, %l4
	
	ldub	[%l0+3], %l3
	sllx	%l4, 8, %l4
	ba,pt	%icc, 1f
	 or	%l4, %l3, %l4
	
2:
	lduh	[%l0+2], %l3
	sllx	%l4, 16, %l4
	or	%l4, %l3, %l4
	
1:	
	deccc	4, %l2
	ble,pn	CCCR, Lmemcpy_finish		! XXXX
	 inc	4, %l0
	
	st	%l4, [%l1]				! Store word
	inc	4, %l1
4:
	!!
	!! We are now 32-bit aligned in the dest.
	!!
Lmemcpy_common:	

	and	%l0, 7, %l4				! Shift amount
	andn	%l0, 7, %l0				! Source addr
	
	brz,pt	%l4, Lmemcpy_noshift8			! No shift version...

	 sllx	%l4, 3, %l4				! In bits
	mov	8<<3, %l3
	
	ldx	[%l0], %o0				! Load word -1
	sub	%l3, %l4, %l3				! Reverse shift
	deccc	12*8, %l2				! Have enough room?
	
	sllx	%o0, %l4, %o0
	bl,pn	CCCR, 2f
	 and	%l3, 0x38, %l3
Lmemcpy_unrolled8:

	/*
	 * This is about as close to optimal as you can get, since
	 * the shifts require EU0 and cannot be paired, and you have
	 * 3 dependent operations on the data.
	 */ 

!	ldx	[%l0+0*8], %o0				! Already done
!	sllx	%o0, %l4, %o0				! Already done
	ldx	[%l0+1*8], %o1
	ldx	[%l0+2*8], %o2
	ldx	[%l0+3*8], %o3
	ldx	[%l0+4*8], %o4
	ba,pt	%icc, 1f
	 ldx	[%l0+5*8], %o5
	.align	8
1:
	srlx	%o1, %l3, %g1
	inc	6*8, %l0
	
	sllx	%o1, %l4, %o1
	or	%g1, %o0, %g6
	ldx	[%l0+0*8], %o0
	
	stx	%g6, [%l1+0*8]
	srlx	%o2, %l3, %g1

	sllx	%o2, %l4, %o2
	or	%g1, %o1, %g6
	ldx	[%l0+1*8], %o1
	
	stx	%g6, [%l1+1*8]
	srlx	%o3, %l3, %g1
	
	sllx	%o3, %l4, %o3
	or	%g1, %o2, %g6
	ldx	[%l0+2*8], %o2
	
	stx	%g6, [%l1+2*8]
	srlx	%o4, %l3, %g1
	
	sllx	%o4, %l4, %o4	
	or	%g1, %o3, %g6
	ldx	[%l0+3*8], %o3
	
	stx	%g6, [%l1+3*8]
	srlx	%o5, %l3, %g1
	
	sllx	%o5, %l4, %o5
	or	%g1, %o4, %g6
	ldx	[%l0+4*8], %o4

	stx	%g6, [%l1+4*8]
	srlx	%o0, %l3, %g1
	deccc	6*8, %l2				! Have enough room?

	sllx	%o0, %l4, %o0				! Next loop
	or	%g1, %o5, %g6
	ldx	[%l0+5*8], %o5
	
	stx	%g6, [%l1+5*8]
	bge,pt	CCCR, 1b
	 inc	6*8, %l1

Lmemcpy_unrolled8_cleanup:	
	!!
	!! Finished 8 byte block, unload the regs.
	!! 
	srlx	%o1, %l3, %g1
	inc	5*8, %l0
	
	sllx	%o1, %l4, %o1
	or	%g1, %o0, %g6
		
	stx	%g6, [%l1+0*8]
	srlx	%o2, %l3, %g1
	
	sllx	%o2, %l4, %o2
	or	%g1, %o1, %g6
		
	stx	%g6, [%l1+1*8]
	srlx	%o3, %l3, %g1
	
	sllx	%o3, %l4, %o3
	or	%g1, %o2, %g6
		
	stx	%g6, [%l1+2*8]
	srlx	%o4, %l3, %g1
	
	sllx	%o4, %l4, %o4	
	or	%g1, %o3, %g6
		
	stx	%g6, [%l1+3*8]
	srlx	%o5, %l3, %g1
	
	sllx	%o5, %l4, %o5
	or	%g1, %o4, %g6
		
	stx	%g6, [%l1+4*8]
	inc	5*8, %l1
	
	mov	%o5, %o0				! Save our unused data
	dec	5*8, %l2
2:
	inccc	12*8, %l2
	bz,pn	%icc, Lmemcpy_complete
	
	!! Unrolled 8 times
Lmemcpy_aligned8:	
!	ldx	[%l0], %o0				! Already done
!	sllx	%o0, %l4, %o0				! Shift high word
	
	 deccc	8, %l2					! Pre-decrement
	bl,pn	CCCR, Lmemcpy_finish
1:
	ldx	[%l0+8], %o1				! Load word 0
	inc	8, %l0
	
	srlx	%o1, %l3, %g6
	or	%g6, %o0, %g6				! Combine
	
	stx	%g6, [%l1]				! Store result
	 inc	8, %l1
	
	deccc	8, %l2
	bge,pn	CCCR, 1b
	 sllx	%o1, %l4, %o0	

	btst	7, %l2					! Done?
	bz,pt	CCCR, Lmemcpy_complete

	!!
	!! Loadup the last dregs into %o0 and shift it into place
	!! 
	 srlx	%l3, 3, %g6				! # bytes in %o0
	dec	8, %g6					!  - 8
	!! n-8 - (by - 8) -> n - by
	subcc	%l2, %g6, %g0				! # bytes we need
	ble,pt	%icc, Lmemcpy_finish
	 nop
	ldx	[%l0+8], %o1				! Need another word
	srlx	%o1, %l3, %o1
	ba,pt	%icc, Lmemcpy_finish
	 or	%o0, %o1, %o0				! All loaded up.
	
Lmemcpy_noshift8:
	deccc	6*8, %l2				! Have enough room?
	bl,pn	CCCR, 2f
	 nop
	ba,pt	%icc, 1f
	 nop
	.align	32
1:	
	ldx	[%l0+0*8], %o0
	ldx	[%l0+1*8], %o1
	ldx	[%l0+2*8], %o2
	stx	%o0, [%l1+0*8]
	stx	%o1, [%l1+1*8]
	stx	%o2, [%l1+2*8]

	
	ldx	[%l0+3*8], %o3
	ldx	[%l0+4*8], %o4
	ldx	[%l0+5*8], %o5
	inc	6*8, %l0
	stx	%o3, [%l1+3*8]
	deccc	6*8, %l2
	stx	%o4, [%l1+4*8]
	stx	%o5, [%l1+5*8]
	bge,pt	CCCR, 1b
	 inc	6*8, %l1
2:
	inc	6*8, %l2
1:	
	deccc	8, %l2
	bl,pn	%icc, 1f				! < 0 --> sub word
	 nop
	ldx	[%l0], %g6
	inc	8, %l0
	stx	%g6, [%l1]
	bg,pt	%icc, 1b				! Exactly 0 --> done
	 inc	8, %l1
1:
	btst	7, %l2					! Done?
	bz,pt	CCCR, Lmemcpy_complete
	 clr	%l4
	ldx	[%l0], %o0
Lmemcpy_finish:
	
	brz,pn	%l2, 2f					! 100% complete?
	 cmp	%l2, 8					! Exactly 8 bytes?
	bz,a,pn	CCCR, 2f
	 stx	%o0, [%l1]

	btst	4, %l2					! Word store?
	bz	CCCR, 1f
	 srlx	%o0, 32, %g6				! Shift high word down
	stw	%g6, [%l1]
	inc	4, %l1
	mov	%o0, %g6				! Operate on the low bits
1:
	btst	2, %l2
	mov	%g6, %o0
	bz	1f
	 srlx	%o0, 16, %g6
	
	sth	%g6, [%l1]				! Store short
	inc	2, %l1
	mov	%o0, %g6				! Operate on low bytes
1:
	mov	%g6, %o0
	btst	1, %l2					! Byte aligned?
	bz	2f
	 srlx	%o0, 8, %g6

	stb	%g6, [%l1]				! Store last byte
	inc	1, %l1					! Update address
2:	
Lmemcpy_complete:
#if 0
	!!
	!! verify copy success.
	!! 

	mov	%i0, %o2
	mov	%i1, %o4
	mov	%i2, %l4
0:	
	ldub	[%o2], %o1
	inc	%o2
	ldub	[%o4], %o3
	inc	%o4
	cmp	%o3, %o1
	bnz	1f
	 dec	%l4
	brnz	%l4, 0b
	 nop
	ba	2f
	 nop

1:
	set	0f, %o0
	call	printf
	 sub	%i2, %l4, %o5
	set	1f, %o0
	mov	%i0, %o2
	mov	%i1, %o1
	call	printf
	 mov	%i2, %o3
	ta	1
	.data
0:	.asciz	"memcpy failed: %x@%p != %x@%p byte %d\n"
1:	.asciz	"memcpy(%p, %p, %lx)\n"
	.align 8
	.text
2:	
#endif
	ret
	 restore %i1, %g0, %o0

#ifdef USE_BLOCK_STORE_LOAD

/*
 * Block copy.  Useful for >256 byte copies.
 *
 * Benchmarking has shown this always seems to be slower than
 * the integer version, so this is disabled.  Maybe someone will
 * figure out why sometime.
 */
	
Lmemcpy_block:
	sethi	%hi(block_disable), %o3
	ldx	[ %o3 + %lo(block_disable) ], %o3
	brnz,pn	%o3, Lmemcpy_fancy
	!! Make sure our trap table is installed
	set	_C_LABEL(trapbase), %o5
	rdpr	%tba, %o3
	sub	%o3, %o5, %o3
	brnz,pn	%o3, Lmemcpy_fancy	! No, then don't use block load/store
	 nop
#ifdef _KERNEL
/*
 * Kernel:
 *
 * Here we use VIS instructions to do a block clear of a page.
 * But before we can do that we need to save and enable the FPU.
 * The last owner of the FPU registers is fplwp, and
 * fplwp->l_md.md_fpstate is the current fpstate.  If that's not
 * null, call savefpstate() with it to store our current fp state.
 *
 * Next, allocate an aligned fpstate on the stack.  We will properly
 * nest calls on a particular stack so this should not be a problem.
 *
 * Now we grab either curlwp (or if we're on the interrupt stack
 * lwp0).  We stash its existing fpstate in a local register and
 * put our new fpstate in curlwp->p_md.md_fpstate.  We point
 * fplwp at curlwp (or lwp0) and enable the FPU.
 *
 * If we are ever preempted, our FPU state will be saved in our
 * fpstate.  Then, when we're resumed and we take an FPDISABLED
 * trap, the trap handler will be able to fish our FPU state out
 * of curlwp (or lwp0).
 *
 * On exiting this routine we undo the damage: restore the original
 * pointer to curlwp->p_md.md_fpstate, clear our fplwp, and disable
 * the MMU.
 *
 *
 * Register usage, Kernel only (after save):
 *
 * %i0		src
 * %i1		dest
 * %i2		size
 *
 * %l0		XXXX DEBUG old fpstate
 * %l1		fplwp (hi bits only)
 * %l2		orig fplwp
 * %l3		orig fpstate
 * %l5		curlwp
 * %l6		old fpstate
 *
 * Register ussage, Kernel and user:
 *
 * %g1		src (retval for memcpy)
 *
 * %o0		src
 * %o1		dest
 * %o2		end dest
 * %o5		last safe fetchable address
 */

	ENABLE_FPU(0)

	mov	%i0, %o0				! Src addr.
	mov	%i1, %o1				! Store our dest ptr here.
	mov	%i2, %o2				! Len counter
#endif	/* _KERNEL */

	!!
	!! First align the output to a 64-bit entity
	!! 

	mov	%o1, %g1				! memcpy retval
	add	%o0, %o2, %o5				! End of source block

	andn	%o0, 7, %o3				! Start of block
	dec	%o5
	fzero	%f0

	andn	%o5, BLOCK_ALIGN, %o5			! Last safe addr.
	ldd	[%o3], %f2				! Load 1st word

	dec	8, %o3					! Move %o3 1 word back
	btst	1, %o1
	bz	4f
	
	 mov	-7, %o4					! Lowest src addr possible
	alignaddr %o0, %o4, %o4				! Base addr for load.

	cmp	%o3, %o4
	be,pt	CCCR, 1f				! Already loaded?
	 mov	%o4, %o3
	fmovd	%f2, %f0				! No. Shift
	ldd	[%o3+8], %f2				! And load
1:	

	faligndata	%f0, %f2, %f4			! Isolate 1st byte

	stda	%f4, [%o1] ASI_FL8_P			! Store 1st byte
	inc	1, %o1					! Update address
	inc	1, %o0
	dec	1, %o2
4:	
	btst	2, %o1
	bz	4f

	 mov	-6, %o4					! Calculate src - 6
	alignaddr %o0, %o4, %o4				! calculate shift mask and dest.

	cmp	%o3, %o4				! Addresses same?
	be,pt	CCCR, 1f
	 mov	%o4, %o3
	fmovd	%f2, %f0				! Shuffle data
	ldd	[%o3+8], %f2				! Load word 0
1:	
	faligndata %f0, %f2, %f4			! Move 1st short low part of f8

	stda	%f4, [%o1] ASI_FL16_P			! Store 1st short
	dec	2, %o2
	inc	2, %o1
	inc	2, %o0
4:
	brz,pn	%o2, Lmemcpy_blockfinish			! XXXX

	 btst	4, %o1
	bz	4f

	mov	-4, %o4
	alignaddr %o0, %o4, %o4				! calculate shift mask and dest.

	cmp	%o3, %o4				! Addresses same?
	beq,pt	CCCR, 1f
	 mov	%o4, %o3
	fmovd	%f2, %f0				! Shuffle data
	ldd	[%o3+8], %f2				! Load word 0
1:	
	faligndata %f0, %f2, %f4			! Move 1st short low part of f8

	st	%f5, [%o1]				! Store word
	dec	4, %o2
	inc	4, %o1
	inc	4, %o0
4:
	brz,pn	%o2, Lmemcpy_blockfinish			! XXXX
	!!
	!! We are now 32-bit aligned in the dest.
	!!
Lmemcpy_block_common:	

	 mov	-0, %o4
	alignaddr %o0, %o4, %o4				! base - shift

	cmp	%o3, %o4				! Addresses same?
	beq,pt	CCCR, 1f
	 mov	%o4, %o3
	fmovd	%f2, %f0				! Shuffle data
	ldd	[%o3+8], %f2				! Load word 0
1:	
	add	%o3, 8, %o0				! now use %o0 for src
	
	!!
	!! Continue until our dest is block aligned
	!! 
Lmemcpy_block_aligned8:	
1:
	brz	%o2, Lmemcpy_blockfinish
	 btst	BLOCK_ALIGN, %o1			! Block aligned?
	bz	1f
	
	 faligndata %f0, %f2, %f4			! Generate result
	deccc	8, %o2
	ble,pn	%icc, Lmemcpy_blockfinish		! Should never happen
	 fmovd	%f4, %f48
	
	std	%f4, [%o1]				! Store result
	inc	8, %o1
	
	fmovd	%f2, %f0
	inc	8, %o0
	ba,pt	%xcc, 1b				! Not yet.
	 ldd	[%o0], %f2				! Load next part
Lmemcpy_block_aligned64:	
1:

/*
 * 64-byte aligned -- ready for block operations.
 *
 * Here we have the destination block aligned, but the
 * source pointer may not be.  Sub-word alignment will
 * be handled by faligndata instructions.  But the source
 * can still be potentially aligned to 8 different words
 * in our 64-bit block, so we have 8 different copy routines.
 *
 * Once we figure out our source alignment, we branch
 * to the appropriate copy routine, which sets up the
 * alignment for faligndata and loads (sets) the values
 * into the source registers and does the copy loop.
 *
 * When were down to less than 1 block to store, we
 * exit the copy loop and execute cleanup code.
 *
 * Block loads and stores are not properly interlocked.
 * Stores save one reg/cycle, so you can start overwriting
 * registers the cycle after the store is issued.  
 * 
 * Block loads require a block load to a different register
 * block or a membar #Sync before accessing the loaded
 * data.
 *	
 * Since the faligndata instructions may be offset as far
 * as 7 registers into a block (if you are shifting source 
 * 7 -> dest 0), you need 3 source register blocks for full 
 * performance: one you are copying, one you are loading, 
 * and one for interlocking.  Otherwise, we would need to
 * sprinkle the code with membar #Sync and lose the advantage
 * of running faligndata in parallel with block stores.  This 
 * means we are fetching a full 128 bytes ahead of the stores.  
 * We need to make sure the prefetch does not inadvertently 
 * cross a page boundary and fault on data that we will never 
 * store.
 *
 */
#if 1
	and	%o0, BLOCK_ALIGN, %o3
	srax	%o3, 3, %o3				! Isolate the offset

	brz	%o3, L100				! 0->0
	 btst	4, %o3
	bnz	%xcc, 4f
	 btst	2, %o3
	bnz	%xcc, 2f
	 btst	1, %o3
	ba,pt	%xcc, L101				! 0->1
	 nop	/* XXX spitfire bug */
2:
	bz	%xcc, L102				! 0->2
	 nop
	ba,pt	%xcc, L103				! 0->3
	 nop	/* XXX spitfire bug */
4:	
	bnz	%xcc, 2f
	 btst	1, %o3
	bz	%xcc, L104				! 0->4
	 nop
	ba,pt	%xcc, L105				! 0->5
	 nop	/* XXX spitfire bug */
2:
	bz	%xcc, L106				! 0->6
	 nop
	ba,pt	%xcc, L107				! 0->7
	 nop	/* XXX spitfire bug */
#else

	!!
	!! Isolate the word offset, which just happens to be
	!! the slot in our jump table.
	!!
	!! This is 6 insns, most of which cannot be paired,
	!! which is about the same as the above version.
	!!
	rd	%pc, %o4
1:	
	and	%o0, 0x31, %o3
	add	%o3, (Lmemcpy_block_jmp - 1b), %o3
	jmpl	%o4 + %o3, %g0
	 nop

	!!
	!! Jump table
	!!
	
Lmemcpy_block_jmp:
	ba,a,pt	%xcc, L100
	 nop
	ba,a,pt	%xcc, L101
	 nop
	ba,a,pt	%xcc, L102
	 nop
	ba,a,pt	%xcc, L103
	 nop
	ba,a,pt	%xcc, L104
	 nop
	ba,a,pt	%xcc, L105
	 nop
	ba,a,pt	%xcc, L106
	 nop
	ba,a,pt	%xcc, L107
	 nop
#endif

	!!
	!! Source is block aligned.
	!!
	!! Just load a block and go.
	!!
L100:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L100"
	.align	8
2:	
#endif
	fmovd	%f0 , %f62
	ldda	[%o0] ASI_BLK_P, %f0
	inc	BLOCK_SIZE, %o0
	cmp	%o0, %o5
	bleu,a,pn	%icc, 3f
	 ldda	[%o0] ASI_BLK_P, %f16
	ba,pt	%icc, 3f
	 membar #Sync
	
	.align	32					! ICache align.
3:
	faligndata	%f62, %f0, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f0, %f2, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f2, %f4, %f36
	cmp	%o0, %o5
	faligndata	%f4, %f6, %f38
	faligndata	%f6, %f8, %f40
	faligndata	%f8, %f10, %f42
	faligndata	%f10, %f12, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f12, %f14, %f46
	
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:	
	stda	%f32, [%o1] ASI_STORE
	faligndata	%f14, %f16, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f16, %f18, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f18, %f20, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f20, %f22, %f38
	cmp	%o0, %o5
	faligndata	%f22, %f24, %f40
	faligndata	%f24, %f26, %f42
	faligndata	%f26, %f28, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f28, %f30, %f46
	
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:
	stda	%f32, [%o1] ASI_STORE
	faligndata	%f30, %f48, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f48, %f50, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f50, %f52, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f52, %f54, %f38
	cmp	%o0, %o5
	faligndata	%f54, %f56, %f40
	faligndata	%f56, %f58, %f42
	faligndata	%f58, %f60, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f60, %f62, %f46
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16			! Increment is at top
	membar	#Sync
2:	
	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1
	
	!!
	!! Source at BLOCK_ALIGN+8
	!!
	!! We need to load almost 1 complete block by hand.
	!! 
L101:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L101"
	.align	8
2:	
#endif
!	fmovd	%f0, %f0				! Hoist fmovd
	ldd	[%o0], %f2
	inc	8, %o0
	ldd	[%o0], %f4
	inc	8, %o0
	ldd	[%o0], %f6
	inc	8, %o0
	ldd	[%o0], %f8
	inc	8, %o0
	ldd	[%o0], %f10
	inc	8, %o0
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0
	
	cmp	%o0, %o5
	bleu,a,pn	%icc, 3f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
3:	
	faligndata	%f0, %f2, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f2, %f4, %f34
	cmp	%o0, %o5
	faligndata	%f4, %f6, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f6, %f8, %f38
	faligndata	%f8, %f10, %f40
	faligndata	%f10, %f12, %f42
	faligndata	%f12, %f14, %f44
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f14, %f16, %f46

	stda	%f32, [%o1] ASI_STORE
	
	faligndata	%f16, %f18, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f18, %f20, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f20, %f22, %f36
	cmp	%o0, %o5
	faligndata	%f22, %f24, %f38
	dec	BLOCK_SIZE, %o2
	faligndata	%f24, %f26, %f40
	faligndata	%f26, %f28, %f42
	faligndata	%f28, %f30, %f44
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:	
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f30, %f48, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f48, %f50, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f50, %f52, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f52, %f54, %f36
	cmp	%o0, %o5
	faligndata	%f54, %f56, %f38
	dec	BLOCK_SIZE, %o2
	faligndata	%f56, %f58, %f40
	faligndata	%f58, %f60, %f42
	faligndata	%f60, %f62, %f44
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:	
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f62, %f0, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1

	!!
	!! Source at BLOCK_ALIGN+16
	!!
	!! We need to load 6 doubles by hand.
	!! 
L102:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L102"
	.align	8
2:	
#endif
	ldd	[%o0], %f4
	inc	8, %o0
	fmovd	%f0, %f2				! Hoist fmovd
	ldd	[%o0], %f6
	inc	8, %o0
	
	ldd	[%o0], %f8
	inc	8, %o0
	ldd	[%o0], %f10
	inc	8, %o0
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0
	
	cmp	%o0, %o5
	bleu,a,pn	%icc, 3f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
3:	
	faligndata	%f2, %f4, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f4, %f6, %f34
	cmp	%o0, %o5
	faligndata	%f6, %f8, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f8, %f10, %f38
	faligndata	%f10, %f12, %f40
	faligndata	%f12, %f14, %f42
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f44

	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f16, %f18, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f18, %f20, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f20, %f22, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f22, %f24, %f36
	cmp	%o0, %o5
	faligndata	%f24, %f26, %f38
	dec	BLOCK_SIZE, %o2
	faligndata	%f26, %f28, %f40
	faligndata	%f28, %f30, %f42
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:	
	faligndata	%f30, %f48, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f48, %f50, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f50, %f52, %f32
	inc	BLOCK_SIZE, %o0
	faligndata	%f52, %f54, %f34
	inc	BLOCK_SIZE, %o1
	faligndata	%f54, %f56, %f36
	cmp	%o0, %o5
	faligndata	%f56, %f58, %f38
	dec	BLOCK_SIZE, %o2
	faligndata	%f58, %f60, %f40
	faligndata	%f60, %f62, %f42
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:	
	faligndata	%f62, %f0, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f0, %f2, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1
	
	!!
	!! Source at BLOCK_ALIGN+24
	!!
	!! We need to load 5 doubles by hand.
	!! 
L103:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L103"
	.align	8
2:	
#endif
	fmovd	%f0, %f4
	ldd	[%o0], %f6
	inc	8, %o0
	ldd	[%o0], %f8
	inc	8, %o0
	ldd	[%o0], %f10
	inc	8, %o0
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0

	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
2:	
	inc	BLOCK_SIZE, %o0
3:	
	faligndata	%f4, %f6, %f32
	cmp	%o0, %o5
	faligndata	%f6, %f8, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f8, %f10, %f36
	faligndata	%f10, %f12, %f38
	faligndata	%f12, %f14, %f40
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f16, %f18, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f18, %f20, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f20, %f22, %f32
	cmp	%o0, %o5
	faligndata	%f22, %f24, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f24, %f26, %f36
	inc	BLOCK_SIZE, %o1
	faligndata	%f26, %f28, %f38
	faligndata	%f28, %f30, %f40
	ble,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:	
	faligndata	%f30, %f48, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f48, %f50, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f50, %f52, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f52, %f54, %f32
	cmp	%o0, %o5
	faligndata	%f54, %f56, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f56, %f58, %f36
	faligndata	%f58, %f60, %f38
	inc	BLOCK_SIZE, %o1
	faligndata	%f60, %f62, %f40
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:	
	faligndata	%f62, %f0, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f0, %f2, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f2, %f4, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1

	!!
	!! Source at BLOCK_ALIGN+32
	!!
	!! We need to load 4 doubles by hand.
	!! 
L104:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L104"
	.align	8
2:	
#endif
	fmovd	%f0, %f6
	ldd	[%o0], %f8
	inc	8, %o0
	ldd	[%o0], %f10
	inc	8, %o0
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0
	
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
2:	
	inc	BLOCK_SIZE, %o0
3:	
	faligndata	%f6, %f8, %f32
	cmp	%o0, %o5
	faligndata	%f8, %f10, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f10, %f12, %f36
	faligndata	%f12, %f14, %f38
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f40
	faligndata	%f16, %f18, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f18, %f20, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f20, %f22, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f22, %f24, %f32
	cmp	%o0, %o5
	faligndata	%f24, %f26, %f34
	faligndata	%f26, %f28, %f36
	inc	BLOCK_SIZE, %o1
	faligndata	%f28, %f30, %f38
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:	
	faligndata	%f30, %f48, %f40
	dec	BLOCK_SIZE, %o2
	faligndata	%f48, %f50, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f50, %f52, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f52, %f54, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f54, %f56, %f32
	cmp	%o0, %o5
	faligndata	%f56, %f58, %f34
	faligndata	%f58, %f60, %f36
	inc	BLOCK_SIZE, %o1
	faligndata	%f60, %f62, %f38
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:	
	faligndata	%f62, %f0, %f40
	dec	BLOCK_SIZE, %o2
	faligndata	%f0, %f2, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f2, %f4, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f4, %f6, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1

	!!
	!! Source at BLOCK_ALIGN+40
	!!
	!! We need to load 3 doubles by hand.
	!! 
L105:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L105"
	.align	8
2:	
#endif
	fmovd	%f0, %f8
	ldd	[%o0], %f10
	inc	8, %o0
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0
	
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
2:	
	inc	BLOCK_SIZE, %o0
3:	
	faligndata	%f8, %f10, %f32
	cmp	%o0, %o5
	faligndata	%f10, %f12, %f34
	faligndata	%f12, %f14, %f36
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f38
	dec	BLOCK_SIZE, %o2
	faligndata	%f16, %f18, %f40
	inc	BLOCK_SIZE, %o0
	faligndata	%f18, %f20, %f42
	faligndata	%f20, %f22, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f22, %f24, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f24, %f26, %f32
	cmp	%o0, %o5
	faligndata	%f26, %f28, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f28, %f30, %f36
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:
	faligndata	%f30, %f48, %f38
	inc	BLOCK_SIZE, %o1
	faligndata	%f48, %f50, %f40
	inc	BLOCK_SIZE, %o0
	faligndata	%f50, %f52, %f42
	faligndata	%f52, %f54, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f54, %f56, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f56, %f58, %f32
	cmp	%o0, %o5
	faligndata	%f58, %f60, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f60, %f62, %f36
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:
	faligndata	%f62, %f0, %f38
	inc	BLOCK_SIZE, %o1
	faligndata	%f0, %f2, %f40
	inc	BLOCK_SIZE, %o0
	faligndata	%f2, %f4, %f42
	faligndata	%f4, %f6, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f6, %f8, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1


	!!
	!! Source at BLOCK_ALIGN+48
	!!
	!! We need to load 2 doubles by hand.
	!! 
L106:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L106"
	.align	8
2:	
#endif
	fmovd	%f0, %f10
	ldd	[%o0], %f12
	inc	8, %o0
	ldd	[%o0], %f14
	inc	8, %o0
	
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
2:	
	inc	BLOCK_SIZE, %o0
3:	
	faligndata	%f10, %f12, %f32
	cmp	%o0, %o5
	faligndata	%f12, %f14, %f34
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f16, %f18, %f38
	inc	BLOCK_SIZE, %o0
	faligndata	%f18, %f20, %f40
	faligndata	%f20, %f22, %f42
	faligndata	%f22, %f24, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f24, %f26, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f26, %f28, %f32
	cmp	%o0, %o5
	faligndata	%f28, %f30, %f34
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:
	faligndata	%f30, %f48, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f48, %f50, %f38
	inc	BLOCK_SIZE, %o1
	faligndata	%f50, %f52, %f40
	faligndata	%f52, %f54, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f54, %f56, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f56, %f58, %f46

	stda	%f32, [%o1] ASI_STORE

	faligndata	%f58, %f60, %f32
	cmp	%o0, %o5
	faligndata	%f60, %f62, %f34
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:
	faligndata	%f62, %f0, %f36
	dec	BLOCK_SIZE, %o2
	faligndata	%f0, %f2, %f38
	inc	BLOCK_SIZE, %o1
	faligndata	%f2, %f4, %f40
	faligndata	%f4, %f6, %f42
	inc	BLOCK_SIZE, %o0
	faligndata	%f6, %f8, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f8, %f10, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1


	!!
	!! Source at BLOCK_ALIGN+56
	!!
	!! We need to load 1 double by hand.
	!! 
L107:
#ifdef RETURN_NAME
	sethi	%hi(1f), %g1
	ba,pt	%icc, 2f
	 or	%g1, %lo(1f), %g1
1:	
	.asciz	"L107"
	.align	8
2:	
#endif
	fmovd	%f0, %f12
	ldd	[%o0], %f14
	inc	8, %o0

	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar #Sync
2:	
	inc	BLOCK_SIZE, %o0
3:	
	faligndata	%f12, %f14, %f32
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f48
	membar	#Sync
2:
	faligndata	%f14, %f16, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f16, %f18, %f36
	inc	BLOCK_SIZE, %o0
	faligndata	%f18, %f20, %f38
	faligndata	%f20, %f22, %f40
	faligndata	%f22, %f24, %f42
	faligndata	%f24, %f26, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f26, %f28, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f28, %f30, %f32
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f0
	membar	#Sync
2:
	faligndata	%f30, %f48, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f48, %f50, %f36
	inc	BLOCK_SIZE, %o1
	faligndata	%f50, %f52, %f38
	faligndata	%f52, %f54, %f40
	inc	BLOCK_SIZE, %o0
	faligndata	%f54, %f56, %f42
	faligndata	%f56, %f58, %f44
	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f58, %f60, %f46
	
	stda	%f32, [%o1] ASI_STORE

	faligndata	%f60, %f62, %f32
	cmp	%o0, %o5
	bleu,a,pn	%icc, 2f
	 ldda	[%o0] ASI_BLK_P, %f16
	membar	#Sync
2:
	faligndata	%f62, %f0, %f34
	dec	BLOCK_SIZE, %o2
	faligndata	%f0, %f2, %f36
	inc	BLOCK_SIZE, %o1
	faligndata	%f2, %f4, %f38
	faligndata	%f4, %f6, %f40
	inc	BLOCK_SIZE, %o0
	faligndata	%f6, %f8, %f42
	faligndata	%f8, %f10, %f44

	brlez,pn	%o2, Lmemcpy_blockdone
	 faligndata	%f10, %f12, %f46

	stda	%f32, [%o1] ASI_STORE
	ba	3b
	 inc	BLOCK_SIZE, %o1
	
Lmemcpy_blockdone:
	inc	BLOCK_SIZE, %o2				! Fixup our overcommit
	membar	#Sync					! Finish any pending loads
#define	FINISH_REG(f)				\
	deccc	8, %o2;				\
	bl,a	Lmemcpy_blockfinish;		\
	 fmovd	f, %f48;			\
	std	f, [%o1];			\
	inc	8, %o1

	FINISH_REG(%f32)
	FINISH_REG(%f34)
	FINISH_REG(%f36)
	FINISH_REG(%f38)
	FINISH_REG(%f40)
	FINISH_REG(%f42)
	FINISH_REG(%f44)
	FINISH_REG(%f46)
	FINISH_REG(%f48)
#undef FINISH_REG
	!! 
	!! The low 3 bits have the sub-word bits needed to be
	!! stored [because (x-8)&0x7 == x].
	!!
Lmemcpy_blockfinish:
	brz,pn	%o2, 2f					! 100% complete?
	 fmovd	%f48, %f4
	cmp	%o2, 8					! Exactly 8 bytes?
	bz,a,pn	CCCR, 2f
	 std	%f4, [%o1]

	btst	4, %o2					! Word store?
	bz	CCCR, 1f
	 nop
	st	%f4, [%o1]
	inc	4, %o1
1:
	btst	2, %o2
	fzero	%f0
	bz	1f

	 mov	-6, %o4
	alignaddr %o1, %o4, %g0

	faligndata %f0, %f4, %f8
	
	stda	%f8, [%o1] ASI_FL16_P			! Store short
	inc	2, %o1
1:
	btst	1, %o2					! Byte aligned?
	bz	2f

	 mov	-7, %o0					! Calculate dest - 7
	alignaddr %o1, %o0, %g0				! Calculate shift mask and dest.

	faligndata %f0, %f4, %f8			! Move 1st byte to low part of f8

	stda	%f8, [%o1] ASI_FL8_P			! Store 1st byte
	inc	1, %o1					! Update address
2:
	membar	#Sync
#if 0
	!!
	!! verify copy success.
	!! 

	mov	%i0, %o2
	mov	%i1, %o4
	mov	%i2, %l4
0:	
	ldub	[%o2], %o1
	inc	%o2
	ldub	[%o4], %o3
	inc	%o4
	cmp	%o3, %o1
	bnz	1f
	 dec	%l4
	brnz	%l4, 0b
	 nop
	ba	2f
	 nop

1:
	set	block_disable, %o0
	stx	%o0, [%o0]
	
	set	0f, %o0
	call	prom_printf
	 sub	%i2, %l4, %o5
	set	1f, %o0
	mov	%i0, %o2
	mov	%i1, %o1
	call	prom_printf
	 mov	%i2, %o3
	ta	1
	.data
	_ALIGN
0:	.asciz	"block memcpy failed: %x@%p != %x@%p byte %d\r\n"
1:	.asciz	"memcpy(%p, %p, %lx)\r\n"
	_ALIGN
	.text
2:	
#endif
#ifdef _KERNEL		

/*
 * Weve saved our possible fpstate, now disable the fpu
 * and continue with life.
 */
	RESTORE_FPU
	ret
	 restore	%g1, 0, %o0			! Return DEST for memcpy
#endif
 	retl
	 mov	%g1, %o0
#endif	/* USE_BLOCK_STORE_LOAD */

	
#if 1
/*
 * XXXXXXXXXXXXXXXXXXXX
 * We need to make sure that this doesn't use floating point
 * before our trap handlers are installed or we could panic
 * XXXXXXXXXXXXXXXXXXXX
 */
/*
 * memset(addr, c, len)
 *
 * We want to use VIS instructions if we're clearing out more than
 * 256 bytes, but to do that we need to properly save and restore the
 * FP registers.  Unfortunately the code to do that in the kernel needs
 * to keep track of the current owner of the FPU, hence the different
 * code.
 *
 * XXXXX To produce more efficient code, we do not allow lengths
 * greater than 0x80000000000000000, which are negative numbers.
 * This should not really be an issue since the VA hole should
 * cause any such ranges to fail anyway.
 */
ENTRY(memset)
	! %o0 = addr, %o1 = pattern, %o2 = len
	mov	%o0, %o4		! Save original pointer

Lmemset_internal:
	btst	7, %o0			! Word aligned?
	bz,pn	%xcc, 0f
	 nop
	inc	%o0
	deccc	%o2			! Store up to 7 bytes
	bge,a,pt	CCCR, Lmemset_internal
	 stb	%o1, [%o0 - 1]

	retl				! Duplicate Lmemset_done
	 mov	%o4, %o0
0:
	/*
	 * Duplicate the pattern so it fills 64-bits.
	 */
	andcc	%o1, 0x0ff, %o1		! No need to extend zero
	bz,pt	%icc, 1f
	 sllx	%o1, 8, %o3		! sigh.  all dependent insns.
	or	%o1, %o3, %o1
	sllx	%o1, 16, %o3
	or	%o1, %o3, %o1
	sllx	%o1, 32, %o3
	 or	%o1, %o3, %o1
1:	
#ifdef USE_BLOCK_STORE_LOAD
	!! Now we are 64-bit aligned
	cmp	%o2, 256		! Use block clear if len > 256
	bge,pt	CCCR, Lmemset_block	! use block store insns
#endif	/* USE_BLOCK_STORE_LOAD */
	 deccc	8, %o2
Lmemset_longs:
	bl,pn	CCCR, Lmemset_cleanup	! Less than 8 bytes left
	 nop
3:	
	inc	8, %o0
	deccc	8, %o2
	bge,pt	CCCR, 3b
	 stx	%o1, [%o0 - 8]		! Do 1 longword at a time

	/*
	 * Len is in [-8..-1] where -8 => done, -7 => 1 byte to zero,
	 * -6 => two bytes, etc.  Mop up this remainder, if any.
	 */
Lmemset_cleanup:	
	btst	4, %o2
	bz,pt	CCCR, 5f		! if (len & 4) {
	 nop
	stw	%o1, [%o0]		!	*(int *)addr = 0;
	inc	4, %o0			!	addr += 4;
5:	
	btst	2, %o2
	bz,pt	CCCR, 7f		! if (len & 2) {
	 nop
	sth	%o1, [%o0]		!	*(short *)addr = 0;
	inc	2, %o0			!	addr += 2;
7:	
	btst	1, %o2
	bnz,a	%icc, Lmemset_done	! if (len & 1)
	 stb	%o1, [%o0]		!	*addr = 0;
Lmemset_done:
	retl
	 mov	%o4, %o0		! Restore ponter for memset (ugh)

#ifdef USE_BLOCK_STORE_LOAD
Lmemset_block:
	sethi	%hi(block_disable), %o3
	ldx	[ %o3 + %lo(block_disable) ], %o3
	brnz,pn	%o3, Lmemset_longs
	!! Make sure our trap table is installed
	set	_C_LABEL(trapbase), %o5
	rdpr	%tba, %o3
	sub	%o3, %o5, %o3
	brnz,pn	%o3, Lmemset_longs	! No, then don't use block load/store
	 nop
/*
 * Kernel:
 *
 * Here we use VIS instructions to do a block clear of a page.
 * But before we can do that we need to save and enable the FPU.
 * The last owner of the FPU registers is fplwp, and
 * fplwp->l_md.md_fpstate is the current fpstate.  If that's not
 * null, call savefpstate() with it to store our current fp state.
 *
 * Next, allocate an aligned fpstate on the stack.  We will properly
 * nest calls on a particular stack so this should not be a problem.
 *
 * Now we grab either curlwp (or if we're on the interrupt stack
 * lwp0).  We stash its existing fpstate in a local register and
 * put our new fpstate in curlwp->p_md.md_fpstate.  We point
 * fplwp at curlwp (or lwp0) and enable the FPU.
 *
 * If we are ever preempted, our FPU state will be saved in our
 * fpstate.  Then, when we're resumed and we take an FPDISABLED
 * trap, the trap handler will be able to fish our FPU state out
 * of curlwp (or lwp0).
 *
 * On exiting this routine we undo the damage: restore the original
 * pointer to curlwp->p_md.md_fpstate, clear our fplwp, and disable
 * the MMU.
 *
 */

	ENABLE_FPU(0)

	!! We are now 8-byte aligned.  We need to become 64-byte aligned.
	btst	63, %i0
	bz,pt	CCCR, 2f
	 nop
1:
	stx	%i1, [%i0]
	inc	8, %i0
	btst	63, %i0
	bnz,pt	%xcc, 1b
	 dec	8, %i2

2:
	brz	%i1, 3f					! Skip the memory op
	 fzero	%f0					! if pattern is 0

#ifdef _LP64
	stx	%i1, [%i0]				! Flush this puppy to RAM
	membar	#StoreLoad
	ldd	[%i0], %f0
#else
	stw	%i1, [%i0]				! Flush this puppy to RAM
	membar	#StoreLoad
	ld	[%i0], %f0
	fmovsa	%icc, %f0, %f1
#endif
	
3:	
	fmovd	%f0, %f2				! Duplicate the pattern
	fmovd	%f0, %f4
	fmovd	%f0, %f6
	fmovd	%f0, %f8
	fmovd	%f0, %f10
	fmovd	%f0, %f12
	fmovd	%f0, %f14

	!! Remember: we were 8 bytes too far
	dec	56, %i2					! Go one iteration too far
5:
	stda	%f0, [%i0] ASI_STORE			! Store 64 bytes
	deccc	BLOCK_SIZE, %i2
	bg,pt	%icc, 5b
	 inc	BLOCK_SIZE, %i0

	membar	#Sync
/*
 * We've saved our possible fpstate, now disable the fpu
 * and continue with life.
 */
	RESTORE_FPU
	addcc	%i2, 56, %i2				! Restore the count
	ba,pt	%xcc, Lmemset_longs			! Finish up the remainder
	 restore
#endif	/* USE_BLOCK_STORE_LOAD */
#endif

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
	restore
	.data
2:	.asciz	"kcopy(%p->%p,%x)\n"
	_ALIGN
	.text
3:
#endif
	sethi	%hi(CPCB), %o5		! cpcb->pcb_onfault = Lkcerr;
	LDPTR	[%o5 + %lo(CPCB)], %o5
	set	Lkcerr, %o3
	LDPTR	[%o5 + PCB_ONFAULT], %g1! save current onfault handler
	membar	#LoadStore
	STPTR	%o3, [%o5 + PCB_ONFAULT]
	membar	#StoreStore|#StoreLoad

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
	ldsb	[%o0], %o4	!	*dst++ = *src++;
	inc	%o0
	stb	%o4, [%o1]
	deccc	%o2
	bge	0b
	 inc	%o1
1:
	membar	#Sync		! Make sure all fauls are processed
	STPTR	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
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
	inc	%o0		!		*dst++ = *src++;
	stb	%o4, [%o1]
	deccc	%o2
	bnz	0b		!	} while (--len != 0);
	 inc	%o1
	membar	#Sync		! Make sure all traps are taken
	STPTR	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
	retl
	 clr	%o0
	NOTREACHED

	! lowest bit matches, so we can copy by words, if nothing else
1:
	be,a	1f		! if (src & 1) {
	 btst	2, %o3		! [delay slot: if (t & 2)]

	! although low bits match, both are 1: must copy 1 byte to align
	ldsb	[%o0], %o4	!	*dst++ = *src++;
	inc	%o0
	stb	%o4, [%o1]
	dec	%o2		!	len--;
	inc	%o1
	btst	2, %o3		! } [if (t & 2)]
1:
	be,a	1f		! if (t & 2) {
	 btst	2, %o0		! [delay slot: if (src & 2)]
	dec	2, %o2		!	len -= 2;
0:
	ldsh	[%o0], %o4	!	do {
	inc	2, %o0		!		dst += 2, src += 2;
	sth	%o4, [%o1]	!		*(short *)dst = *(short *)src;
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
	inc	2, %o0		!	dst += 2;
	sth	%o4, [%o1]
	dec	2, %o2		!	len -= 2;
	inc	2, %o1		!	src += 2;
	btst	4, %o3		! } [if (t & 4)]
1:
	be,a	1f		! if (t & 4) {
	 btst	4, %o0		! [delay slot: if (src & 4)]
	dec	4, %o2		!	len -= 4;
0:
	ld	[%o0], %o4	!	do {
	inc	4, %o0		!		dst += 4, src += 4;
	st	%o4, [%o1]	!		*(int *)dst = *(int *)src;
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
	inc	4, %o0		!	dst += 4, src += 4, len -= 4;
	st	%o4, [%o1]
	dec	4, %o2		! }
	inc	4, %o1
1:
Lkcopy_doubles:
	ldx	[%o0], %g5	! do {
	inc	8, %o0		!	dst += 8, src += 8;
	stx	%g5, [%o1]	!	*(double *)dst = *(double *)src;
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
	inc	4, %o0		!	dst += 4;
	st	%o4, [%o1]
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
	STPTR	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
	retl
	 clr	%o0
	NOTREACHED

	! mop up trailing byte (if present).
Lkcopy_mopb:
	bne,a	1f
	 ldsb	[%o0], %o4

Lkcopy_done:
	membar	#Sync		! Make sure all traps are taken
	STPTR	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
	retl
	 clr	%o0
	NOTREACHED

1:
	stb	%o4, [%o1]
	membar	#Sync		! Make sure all traps are taken
	STPTR	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
	retl
	 clr	%o0
	NOTREACHED

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
	restore
	.data
2:	.asciz	"kcopy error\n"
	_ALIGN
	.text
3:
#endif
	STPTR	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
	retl				! and return error indicator
	 mov	EFAULT, %o0
	NOTREACHED

#ifdef MULTIPROCESSOR
ENTRY(sparc64_ipi_save_fpstate)
	mov	%o0, %g1		! save registers used by savefpstate
	mov	%o1, %g2		! to alternate globals
	mov	%o2, %g3
	mov	%o3, %g4
	mov	%o4, %g5
	mov	%o5, %g6
	sethi	%hi(CPUINFO_VA + CI_FPLWP), %o0
	ldx	[%o0 + %lo(CPUINFO_VA + CI_FPLWP)], %o0
	call	savefpstate
	 ldx	[%o0 + L_FPSTATE], %l1
	stx	%g0, [%o0]		! fplwp = NULL
	mov	%g6, %o5		! restore saved registers
	mov	%g5, %o4
	mov	%g4, %o3
	mov	%g3, %o2
	mov	%g2, %o1
	ba	ret_from_intr_vector
	 mov	%g1, %o0

ENTRY(sparc64_ipi_drop_fpstate)
	mov	%o0, %g1		! save registers used here
	mov	%o1, %g2		! to alternate globals
	rdpr	%pstate, %o1
	wr	%g0, FPRS_FEF, %fprs
	or	%o1, PSTATE_PEF, %o1
	wrpr	%o1, 0, %pstate
	sethi	%hi(CPUINFO_VA + CI_FPLWP), %o0
	stx	%g0, [%o0 + %lo(CPUINFO_VA + CI_FPLWP)]	! fplwp = NULL
	mov	%g2, %o1		! restore saved registers
	ba	ret_from_intr_vector
	 mov	%g1, %o0
#endif

/*
 * clearfpstate()
 *
 * Drops the current fpu state, without saving it.
 */
ENTRY(clearfpstate)
	rdpr	%pstate, %o1		! enable FPU
	wr	%g0, FPRS_FEF, %fprs
	or	%o1, PSTATE_PEF, %o1
	retl
	 wrpr	%o1, 0, %pstate

/*
 * savefpstate(f) struct fpstate *f;
 *
 * Store the current FPU state.  The first `st %fsr' may cause a trap;
 * our trap handler knows how to recover (by `returning' to savefpcont).
 *
 * Since the kernel may need to use the FPU and we have problems atomically
 * testing and enabling the FPU, we leave here with the FPRS_FEF bit set.
 * Normally this should be turned on in loadfpstate().
 */
 /* XXXXXXXXXX  Assume caller created a proper stack frame */
ENTRY(savefpstate)
!	flushw			! Make sure we don't have stack probs & lose hibits of %o
	rdpr	%pstate, %o1		! enable FP before we begin
	rd	%fprs, %o5
	wr	%g0, FPRS_FEF, %fprs
	or	%o1, PSTATE_PEF, %o1
	wrpr	%o1, 0, %pstate
	/* do some setup work while we wait for PSR_EF to turn on */
	set	FSR_QNE, %o2		! QNE = 0x2000, too big for immediate
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
	rd	%gsr, %o4		! Save %gsr
	st	%o4, [%o0 + FS_GSR]

	ldx	[%o0 + FS_FSR], %o4	! if (f->fs_fsr & QNE)
	btst	%o2, %o4
	add	%o0, FS_REGS, %o2
	bnz	Lfp_storeq		!	goto storeq;
Lfp_finish:
	 btst	BLOCK_ALIGN, %o2	! Needs to be re-executed
	bnz,pn	%icc, 3f		! Check alignment
	 st	%o3, [%o0 + FS_QSIZE]	! f->fs_qsize = qsize;
	btst	FPRS_DL, %o5		! Lower FPU clean?
	bz,a,pt	%icc, 1f		! Then skip it
	 add	%o2, 128, %o2		! Skip a block

	membar	#Sync
	stda	%f0, [%o2] ASI_BLK_COMMIT_P	! f->fs_f0 = etc;
	inc	BLOCK_SIZE, %o2
	stda	%f16, [%o2] ASI_BLK_COMMIT_P
	inc	BLOCK_SIZE, %o2
1:
	btst	FPRS_DU, %o5		! Upper FPU clean?
	bz,pt	%icc, 2f		! Then skip it
	 nop

	membar	#Sync
	stda	%f32, [%o2] ASI_BLK_COMMIT_P
	inc	BLOCK_SIZE, %o2
	stda	%f48, [%o2] ASI_BLK_COMMIT_P
2:
	membar	#Sync			! Finish operation so we can
	retl
	 wr	%g0, FPRS_FEF, %fprs	! Mark FPU clean
3:
#ifdef DIAGONSTIC
	btst	7, %o2			! 32-bit aligned!?!?
	bnz,pn	%icc, 6f
#endif
	 btst	FPRS_DL, %o5		! Lower FPU clean?
	bz,a,pt	%icc, 4f		! Then skip it
	 add	%o0, 128, %o0

	membar	#Sync
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
4:
	btst	FPRS_DU, %o5		! Upper FPU clean?
	bz,pt	%icc, 5f		! Then skip it
	 nop

	membar	#Sync
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
	std	%f62, [%o0 + FS_REGS + (4*62)]
5:
	membar	#Sync
	retl
	 wr	%g0, FPRS_FEF, %fprs		! Mark FPU clean

	!!
	!! Damn thing is *NOT* aligned on a 64-bit boundary
	!! 
6:
	wr	%g0, FPRS_FEF, %fprs
	ta	1
	retl
	 nop
	
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
 /* XXXXXXXXXX  Should test to see if we only need to do a partial restore */
ENTRY(loadfpstate)
	flushw			! Make sure we don't have stack probs & lose hibits of %o
	rdpr	%pstate, %o1		! enable FP before we begin
	ld	[%o0 + FS_GSR], %o4	! Restore %gsr
	set	PSTATE_PEF, %o2
	wr	%g0, FPRS_FEF, %fprs
	or	%o1, %o2, %o1
	wrpr	%o1, 0, %pstate
	ldx	[%o0 + FS_FSR], %fsr	! setfsr(f->fs_fsr);
	add	%o0, FS_REGS, %o3	! This is zero...
	btst	BLOCK_ALIGN, %o3
	bne,pt	%icc, 1f	! Only use block loads on aligned blocks
	 wr	%o4, %g0, %gsr
	membar	#Sync
	ldda	[%o3] ASI_BLK_P, %f0
	inc	BLOCK_SIZE, %o3
	ldda	[%o3] ASI_BLK_P, %f16
	inc	BLOCK_SIZE, %o3
	ldda	[%o3] ASI_BLK_P, %f32
	inc	BLOCK_SIZE, %o3
	ldda	[%o3] ASI_BLK_P, %f48
	membar	#Sync			! Make sure loads are complete
	retl
	 wr	%g0, FPRS_FEF, %fprs	! Clear dirty bits
1:
#ifdef DIAGNOSTIC
	btst	7, %o3
	bne,pn	%icc, 1f
	 nop
#endif
	/* Unaligned -- needs to be done the long way
	membar	#Sync
	ldd	[%o3 + (4*0)], %f0
	ldd	[%o3 + (4*2)], %f2
	ldd	[%o3 + (4*4)], %f4
	ldd	[%o3 + (4*6)], %f6
	ldd	[%o3 + (4*8)], %f8
	ldd	[%o3 + (4*10)], %f10
	ldd	[%o3 + (4*12)], %f12
	ldd	[%o3 + (4*14)], %f14
	ldd	[%o3 + (4*16)], %f16
	ldd	[%o3 + (4*18)], %f18
	ldd	[%o3 + (4*20)], %f20
	ldd	[%o3 + (4*22)], %f22
	ldd	[%o3 + (4*24)], %f24
	ldd	[%o3 + (4*26)], %f26
	ldd	[%o3 + (4*28)], %f28
	ldd	[%o3 + (4*30)], %f30
	ldd	[%o3 + (4*32)], %f32
	ldd	[%o3 + (4*34)], %f34
	ldd	[%o3 + (4*36)], %f36
	ldd	[%o3 + (4*38)], %f38
	ldd	[%o3 + (4*40)], %f40
	ldd	[%o3 + (4*42)], %f42
	ldd	[%o3 + (4*44)], %f44
	ldd	[%o3 + (4*46)], %f46
	ldd	[%o3 + (4*48)], %f48
	ldd	[%o3 + (4*50)], %f50
	ldd	[%o3 + (4*52)], %f52
	ldd	[%o3 + (4*54)], %f54
	ldd	[%o3 + (4*56)], %f56
	ldd	[%o3 + (4*58)], %f58
	ldd	[%o3 + (4*60)], %f60
 	ldd	[%o3 + (4*62)], %f62
	membar	#Sync
	retl
	 wr	%g0, FPRS_FEF, %fprs	! Clear dirty bits

1:
	wr	%g0, FPRS_FEF, %fprs	! Clear dirty bits
	ta	1
	retl
	 nop
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

/*
 * send_softint(cpu, level, intrhand)
 *
 * Send a softint with an intrhand pointer so we can cause a vectored
 * interrupt instead of a polled interrupt.  This does pretty much the same
 * as interrupt_vector.  If cpu is -1 then send it to this CPU, if it's -2
 * send it to any CPU, otherwise send it to a particular CPU.
 *
 * XXXX Dispatching to different CPUs is not implemented yet.
 */
ENTRY(send_softint)
	rdpr	%pstate, %g1
	andn	%g1, PSTATE_IE, %g2	! clear PSTATE.IE
	wrpr	%g2, 0, %pstate

	set	intrpending, %o3
	LDPTR	[%o2 + IH_PEND], %o5
	brnz	%o5, 1f
	 sll	%o1, PTRSHFT+3, %o5	! Find start of table for this IPL
	add	%o3, %o5, %o3
2:
	LDPTR	[%o3], %o5		! Load list head
	STPTR	%o5, [%o2+IH_PEND]	! Link our intrhand node in
	mov	%o2, %o4
	CASPTR	[%o3] ASI_N, %o5, %o4
	cmp	%o4, %o5		! Did it work?
	bne,pn	%xcc, 2b		! No, try again
	 nop

	mov	1, %o3			! Change from level to bitmask
	sllx	%o3, %o1, %o3
	wr	%o3, 0, SET_SOFTINT	! SET_SOFTINT
1:
	retl
	 wrpr	%g1, 0, %pstate		! restore PSTATE.IE

/*
 * Here is a very good random number generator.  This implementation is
 * based on _Two Fast Implementations of the `Minimal Standard' Random
 * Number Generator_, David G. Carta, Communications of the ACM, Jan 1990,
 * Vol 33 No 1.
 */
/*
 * This should be rewritten using the mulx instr. if I ever understand what it
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
 *
 * For some insane reason timeval structure members are `long's so
 * we need to change this code depending on the memory model.
 *
 * NB: if somehow time was 128-bit aligned we could use an atomic
 * quad load to read it in and not bother de-bouncing it.
 */
#define MICROPERSEC	(1000000)

	.data
	.align	8
	.globl	_C_LABEL(cpu_clockrate)
_C_LABEL(cpu_clockrate):
	!! Pretend we have a 200MHz clock -- cpu_attach will fix this
	.xword	200000000
	!! Here we'll store cpu_clockrate/1000000 so we can calculate usecs
	.xword	0
	.text


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
#if 1
	rdpr	%tick, %o1					! Take timer snapshot
	sethi	%hi(_C_LABEL(cpu_clockrate)), %o2
	sethi	%hi(MICROPERSEC), %o3
	ldx	[%o2 + %lo(_C_LABEL(cpu_clockrate) + 8)], %o4	! Get scale factor
	brnz,pt	%o4, 0f
	 or	%o3, %lo(MICROPERSEC), %o3

	!! Calculate ticks/usec
	ldx	[%o2 + %lo(_C_LABEL(cpu_clockrate))], %o4	! No, we need to calculate it
	udivx	%o4, %o3, %o4
	stx	%o4, [%o2 + %lo(_C_LABEL(cpu_clockrate) + 8)]	! Save it so we don't need to divide again
0:

	mulx	%o0, %o4, %o0					! Convert usec -> ticks
	rdpr	%tick, %o2					! Top of next itr
1:
	sub	%o2, %o1, %o3					! How many ticks have gone by?
	sub	%o0, %o3, %o4					! Decrement count by that much
	movrgz	%o3, %o4, %o0					! But only if we're decrementing
	mov	%o2, %o1					! Remember last tick
	brgz,pt	%o0, 1b						! Done?
	 rdpr	%tick, %o2					! Get new tick

	retl
	 nop
#else
/* This code only works if %tick does not wrap */
	rdpr	%tick, %g1					! Take timer snapshot
	sethi	%hi(_C_LABEL(cpu_clockrate)), %g2
	sethi	%hi(MICROPERSEC), %o2
	ldx	[%g2 + %lo(_C_LABEL(cpu_clockrate))], %g2	! Get scale factor
	or	%o2, %lo(MICROPERSEC), %o2
!	sethi	%hi(_C_LABEL(timerblurb), %o5			! This is if we plan to tune the clock
!	ld	[%o5 + %lo(_C_LABEL(timerblurb))], %o5		!  with respect to the counter/timer
	mulx	%o0, %g2, %g2					! Scale it: (usec * Hz) / 1 x 10^6 = ticks
	udivx	%g2, %o2, %g2
	add	%g1, %g2, %g2
!	add	%o5, %g2, %g2			5, %g2, %g2					! But this gets complicated
	rdpr	%tick, %g1					! Top of next itr
	mov	%g1, %g1	! Erratum 50
1:
	cmp	%g1, %g2
	bl,a,pn %xcc, 1b					! Done?
	 rdpr	%tick, %g1

	retl
	 nop
#endif
	/*
	 * If something's wrong with the standard setup do this stupid loop
	 * calibrated for a 143MHz processor.
	 */
Lstupid_delay:
	set	142857143/MICROPERSEC, %o1
Lstupid_loop:
	brnz,pt	%o1, Lstupid_loop
	 dec	%o1
	brnz,pt	%o0, Lstupid_delay
	 dec	%o0
	retl
	 nop

/*
 * next_tick(long increment)
 *
 * Sets the %tick_cmpr register to fire off in `increment' machine
 * cycles in the future.  Also handles %tick wraparound.  In 32-bit
 * mode we're limited to a 32-bit increment.
 */
	.data
	.align	8
tlimit:
	.xword	0
	.text
ENTRY(next_tick)
	rd	TICK_CMPR, %o2
	rdpr	%tick, %o1

	mov	1, %o3		! Mask off high bits of these registers
	sllx	%o3, 63, %o3
	andn	%o1, %o3, %o1
	andn	%o2, %o3, %o2
	cmp	%o1, %o2	! Did we wrap?  (tick < tick_cmpr)
	bgt,pt	%icc, 1f
	 add	%o1, 1000, %o1	! Need some slack so we don't lose intrs.

	/*
	 * Handle the unlikely case of %tick wrapping.
	 *
	 * This should only happen every 10 years or more.
	 *
	 * We need to increment the time base by the size of %tick in
	 * microseconds.  This will require some divides and multiplies
	 * which can take time.  So we re-read %tick.
	 *
	 */

	/* XXXXX NOT IMPLEMENTED */



1:
	add	%o2, %o0, %o2
	andn	%o2, %o3, %o4
	brlz,pn	%o4, Ltick_ovflw
	 cmp	%o2, %o1	! Has this tick passed?
	blt,pn	%xcc, 1b	! Yes
	 nop

	retl
	 wr	%o2, TICK_CMPR

Ltick_ovflw:
/*
 * When we get here tick_cmpr has wrapped, but we don't know if %tick
 * has wrapped.  If bit 62 is set then we have not wrapped and we can
 * use the current value of %o4 as %tick.  Otherwise we need to return
 * to our loop with %o4 as %tick_cmpr (%o2).
 */
	srlx	%o3, 1, %o5
	btst	%o5, %o1
	bz,pn	%xcc, 1b
	 mov	%o4, %o2
	retl
	 wr	%o2, TICK_CMPR


ENTRY(setjmp)
	save	%sp, -CC64FSZ, %sp	! Need a frame to return to.
	flushw
	stx	%fp, [%i0+0]	! 64-bit stack pointer
	stx	%i7, [%i0+8]	! 64-bit return pc
	ret
	 restore	%g0, 0, %o0

	.data
Lpanic_ljmp:
	.asciz	"longjmp botch"
	_ALIGN
	.text

ENTRY(longjmp)
	save	%sp, -CC64FSZ, %sp	! prepare to restore to (old) frame
	flushw
	mov	1, %i2
	ldx	[%i0+0], %fp	! get return stack
	movrz	%i1, %i1, %i2	! compute v ? v : 1
	ldx	[%i0+8], %i7	! get rpc
	ret
	 restore	%i2, 0, %o0

#if defined(DDB) || defined(KGDB)
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
#ifdef SPITFIRE
	set	DEMAP_CTX_SECONDARY, %o3
	stxa	%o3, [%o3] ASI_DMMU_DEMAP
	mov	CTX_SECONDARY, %o4
	stxa	%o3, [%o3] ASI_IMMU_DEMAP
	membar	#Sync
	stxa	%o0, [%o4] ASI_DMMU		! Maybe we should invali
	sethi	%hi(KERNBASE), %o2
	membar	#Sync
	flush	%o2
	retl
	 nop
#else
	/* UNIMPLEMENTED */
	retl
	 nop
#endif

#ifndef _LP64
	/*
	 * Convert to 32-bit stack then call OF_sym2val()
	 */
ENTRY(OF_sym2val32)
	save	%sp, -CC64FSZ, %sp
	btst	7, %i0
	bnz,pn	%icc, 1f
	 add	%sp, BIAS, %o1
	btst	1, %sp
	movnz	%icc, %o1, %sp
	call	_C_LABEL(OF_sym2val)
	 mov	%i0, %o0
1:
	ret
	 restore	%o0, 0, %o0

	/*
	 * Convert to 32-bit stack then call OF_val2sym()
	 */
ENTRY(OF_val2sym32)
	save	%sp, -CC64FSZ, %sp
	btst	7, %i0
	bnz,pn	%icc, 1f
	 add	%sp, BIAS, %o1
	btst	1, %sp
	movnz	%icc, %o1, %sp
	call	_C_LABEL(OF_val2sym)
	 mov	%i0, %o0
1:
	ret
	 restore	%o0, 0, %o0
#endif /* _LP64 */
#endif /* DDB */

	.data
	_ALIGN
#if NKSYMS || defined(DDB) || defined(LKM)
	.globl	_C_LABEL(esym)
_C_LABEL(esym):
	POINTER	0
	.globl	_C_LABEL(ssym)
_C_LABEL(ssym):
	POINTER	0
#endif
	! XXX should it called lwp0paddr
	.globl	_C_LABEL(proc0paddr)
_C_LABEL(proc0paddr):
	POINTER	0

#if !defined(MULTIPROCESSOR)
	.comm	_C_LABEL(curlwp), PTRSZ
#endif
	.comm	_C_LABEL(promvec), PTRSZ

#ifdef DEBUG
	.comm	_C_LABEL(trapdebug), 4
	.comm	_C_LABEL(pmapdebug), 4
#endif
