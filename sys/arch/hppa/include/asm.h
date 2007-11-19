/*	$NetBSD: asm.h,v 1.7 2007/04/07 08:34:17 skrll Exp $	*/

/*	$OpenBSD: asm.h,v 1.12 2001/03/29 02:15:57 mickey Exp $	*/

/* 
 * Copyright (c) 1990,1991,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *	Utah $Hdr: asm.h 1.8 94/12/14$
 */

#ifndef _HPPA_ASM_H_
#define _HPPA_ASM_H_

/*
 *	hppa assembler definitions
 */

#ifdef __STDC__
#define	__CONCAT(a,b)	a ## b
#else
#define	__CONCAT(a,b)	a/**/b
#endif

#ifdef PROF
#define	_PROF_PROLOGUE !\
	stw %rp, HPPA_FRAME_CRP(%sr0,%sp)	!\
	ldil L%_mcount,%r1		!\
	ble R%_mcount(%sr0,%r1)		!\
	ldo HPPA_FRAME_SIZE(%sp),%sp	!\
	ldw HPPA_FRAME_CRP(%sr0,%sp),%rp
#else
#define	_PROF_PROLOGUE
#endif

#define	LEAF_ENTRY(x) ! .text ! .align	4	!\
	.export	x, entry ! .label x ! .proc	!\
	.callinfo frame=0,no_calls,save_rp	!\
	.entry ! _PROF_PROLOGUE

#define	ENTRY(x,n) ! .text ! .align 4			!\
	.export	x, entry ! .label x ! .proc		!\
	.callinfo frame=n,calls, save_rp, save_sp	!\
	.entry ! _PROF_PROLOGUE

#define	ENTRY_NOPROFILE(x,n) ! .text ! .align 4		!\
	.export x, entry ! .label x ! .proc		!\
	.callinfo frame=n,calls, save_rp, save_sp	!\
	.entry

#define ALTENTRY(x) ! .export x, entry ! .label x
#define EXIT(x) ! .exit ! .procend ! .size x, .-x

#define RCSID(x)	.text				!\
			.asciz x			!\
			.align	4

#define WEAK_ALIAS(alias,sym)				\
	.weak alias !					\
	alias = sym

/*
 * STRONG_ALIAS: create a strong alias.
 */
#define STRONG_ALIAS(alias,sym)				\
	.globl alias !					\
	alias = sym

#define CALL(func,tmp)					!\
	ldil	L%func, tmp				!\
	ldo	R%func(tmp), tmp			!\
	.call						!\
	blr	%r0, %rp				!\
	bv,n	%r0(tmp)				!\
	nop

#ifdef PIC
#define PIC_CALL(func)					!\
	addil	LT%func, %r19				!\
	ldw	RT%func(%r1), %r1			!\
	.call						!\
	blr	%r0, %rp				!\
	bv,n	%r0(%r1)				!\
	nop
#else
#define PIC_CALL(func)					!\
	CALL(func,%r1)
#endif

#ifdef __STDC__
#define	WARN_REFERENCES(sym,msg)					\
	.stabs msg ## ,30,0,0,0 ;					\
	.stabs __STRING(sym) ## ,1,0,0,0
#else
#define	WARN_REFERENCES(sym,msg)					\
	.stabs msg,30,0,0,0 ;						\
	.stabs __STRING(sym),1,0,0,0
#endif

#define	BSS(n,s)	! .data ! .label n ! .comm s
#define	SZREG	4

#endif /* _HPPA_ASM_H_ */
