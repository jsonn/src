/*	$NetBSD: asm.h,v 1.13.10.2 1998/10/30 08:33:37 nisimura Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)machAsmDefs.h	8.1 (Berkeley) 6/10/93
 */

/*
 * machAsmDefs.h --
 *
 *	Macros used when writing assembler programs.
 *
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/machAsmDefs.h,
 *	v 1.2 89/08/15 18:28:24 rab Exp  SPRITE (DECWRL)
 */

#ifndef _MIPS_ASM_H
#define _MIPS_ASM_H

/*
 * Symbolic register names
 */
#define zero	$0	/* always zero */
#define AT	$at	/* assembler temporary */
#define v0	$2	/* return value */
#define v1	$3
#define a0	$4	/* argument registers */
#define a1	$5
#define a2	$6
#define a3	$7
#define t0	$8	/* temp registers (not saved across subroutine calls) */
#define t1	$9
#define t2	$10
#define t3	$11
#define t4	$12
#define t5	$13
#define t6	$14
#define t7	$15
#define s0	$16	/* saved across subroutine calls (callee saved) */
#define s1	$17
#define s2	$18
#define s3	$19
#define s4	$20
#define s5	$21
#define s6	$22
#define s7	$23
#define t8	$24	/* two more temporary registers */
#define t9	$25
#define k0	$26	/* kernel temporary */
#define k1	$27
#define gp	$28	/* global pointer */
#define sp	$29	/* stack pointer */
#define s8	$30	/* one more callee saved */
#define ra	$31	/* return address */

/*
 * Define -pg profile entry code.
 * XXX assume .set noreorder for kernel, .set reorder for user code.
 */
#define _KERN_MCOUNT		\
	.set	noat;		\
	move	$1,$31;		\
	jal	_mcount;	\
	subu	sp,sp,8;	\
	.set at

#ifdef GPROF
# if defined(_KERNEL) || defined(_LOCORE)
#  define MCOUNT _KERN_MCOUNT
# else
#  define MCOUNT .set noreorder; _KERN_MCOUNT ;  .set reorder;
# endif
#else
#define	MCOUNT
#endif

#ifdef __NO_LEADING_UNDERSCORES__
# define _C_LABEL(x)	x
#else
# ifdef __STDC__
#  define _C_LABEL(x)	_ ## x
# else
#  define _C_LABEL(x)	_/**/x
# endif
#endif

#ifdef USE_AENT
#define AENT(x)				\
	.aent	x, 0
#else
#define AENT(x)
#endif

/*
 * LEAF
 *	A leaf routine does
 *	- call no other function,
 *	- never use any register that callee-saved (S0-S8), and
 *	- not use any local stack storage.
 */
#define LEAF(x)				\
	.globl	_C_LABEL(x);		\
	.ent	_C_LABEL(x), 0;		\
_C_LABEL(x): ;				\
	.frame sp, 0, ra;		\
	MCOUNT

/*
 * LEAF_NOPROFILE
 *	No profilable leaf routine.
 */
#define LEAF_NOPROFILE(x)		\
	.globl	_C_LABEL(x);		\
	.ent	_C_LABEL(x), 0;		\
_C_LABEL(x): ;				\
	.frame	sp, 0, ra

/*
 * XLEAF
 *	declare alternate entry to leaf routine
 */
#define XLEAF(x)			\
	.globl	_C_LABEL(x);		\
	.aent	_C_LABEL(x),0;		\
_C_LABEL(x):

/*
 * NESTED
 *	A function calls other functions and needs
 *	therefore stack space to save/restore registers.
 */
#define NESTED(x, fsize, retpc)		\
	.globl	_C_LABEL(x);		\
	.ent	_C_LABEL(x), 0; 	\
_C_LABEL(x): ;				\
	.frame	sp, fsize, retpc;	\
	MCOUNT

/*
 * NESTED_NOPROFILE(x)
 *	No profilable nested routine.
 */
#define NESTED_NOPROFILE(x, fsize, retpc)	\
	.globl	_C_LABEL(x);		\
	.ent	_C_LABEL(x), 0;		\
_C_LABEL(x): ;				\
	.frame	sp, fsize, retpc

/*
 * XNESTED
 *	declare alternate entry point to nested routine.
 */
#define XNESTED(x)			\
	.globl	_C_LABEL(x);		\
	.aent	_C_LABEL(x),0;		\
_C_LABEL(x):

/*
 * END
 *	Mark end of a procedure.
 */
#define END(x) \
	.end _C_LABEL(x)

/*
 * IMPORT -- import external symbol
 */
#define IMPORT(sym, size)		\
	.extern sym,size

/*
 * EXPORT -- export definition of symbol
 */
#define EXPORT(x)			\
	.globl	_C_LABEL(x);		\
_C_LABEL(x):

/*
 * ALIAS
 *	Global alias for a function, or alternate entry point
 */
#define	ALIAS(x)			\
	.globl	_C_LABEL(x);		\
_C_LABEL(x):

/*
 * Macros to panic and printf from assembly language.
 */
#define PANIC(msg)			\
	la	a0, 9f;			\
	jal	_C_LABEL(panic);	\
	MSG(msg)

#define	PRINTF(msg)			\
	la	a0, 9f;			\
	jal	_C_LABEL(printf);	\
	MSG(msg)

#define	MSG(msg)			\
	.rdata;				\
9:	.asciiz	msg;			\
	.text

#define ASMSTR(str)			\
	.asciiz str;			\
	.align	3

/*
 * XXX retain dialects XXX
 */
#define ALEAF(x)			\
	.globl _C_LABEL(x);		\
	AENT (_C_LABEL(x))		\
_C_LABEL(x):

#define NLEAF(x)			\
	.globl _C_LABEL(x); 		\
	.ent _C_LABEL(x), 0;		\
_C_LABEL(x): ; \
	.frame sp, 0, ra

#define NON_LEAF(x, fsize, retpc)	\
	.globl _C_LABEL(x);		\
	.ent _C_LABEL(x), 0;		\
_C_LABEL(x): ;				\
	.frame sp, fsize, retpc;	\
	MCOUNT

#define NNON_LEAF(x, fsize, retpc)	\
	.globl _C_LABEL(x);		\
	.ent _C_LABEL(x), 0;		\
_C_LABEL(x): ;				\
	.frame sp, fsize, retpc

#endif /* _MIPS_ASM_H */
