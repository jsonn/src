/*	$NetBSD: icu.s,v 1.63.2.4 2002/11/11 21:59:20 nathanw Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_vm86.h"

#include <net/netisr.h>

	.data
	.globl	_C_LABEL(imen),_C_LABEL(ipending)
	.globl	_C_LABEL(netisr)
_C_LABEL(imen):
	.long	0xffff		# interrupt mask enable (all off)

	.text

#if defined(PROF) || defined(GPROF)
	.globl	_C_LABEL(splhigh), _C_LABEL(splx)

	ALIGN_TEXT
_C_LABEL(splhigh):
	movl	$IPL_HIGH,%eax
	xchgl	%eax,CPL
	ret

	ALIGN_TEXT
_C_LABEL(splx):
	movl	4(%esp),%eax
	movl	%eax,CPL
	testl	%eax,%eax
	jnz	_C_LABEL(Xspllower)
	ret
#endif /* PROF || GPROF */
	
/*
 * Process pending interrupts.
 *
 * Important registers:
 *   ebx - cpl
 *   esi - address to resume loop at
 *   edi - scratch for Xsoftnet
 */
IDTVEC(spllower)
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movzbl	CPL,%ebx		# save priority
	movl	$1f,%esi		# address to resume loop at
1:	movl	%ebx,%eax		# get cpl
	shrl	$4,%eax			# find its mask.
	movl	_C_LABEL(iunmask)(,%eax,4),%eax
	andl	_C_LABEL(ipending),%eax		# any non-masked bits left?
	jz	2f
	bsfl	%eax,%eax
#ifdef MULTIPROCESSOR
	lock
#endif
	btrl	%eax,_C_LABEL(ipending)
	jnc	1b
	jmp	*_C_LABEL(Xrecurse)(,%eax,4)
2:	popl	%edi
	popl	%esi
	popl	%ebx
	ret

/*
 * Handle return from interrupt after device handler finishes.
 *
 * Important registers:
 *   ebx - cpl to restore
 *   esi - address to resume loop at
 *   edi - scratch for Xsoftnet
 */
IDTVEC(doreti)
	popl	%ebx			# get previous priority
	movl	%ebx,CPL
	movl	$1f,%esi		# address to resume loop at
1:	movl	%ebx,%eax
	shrl	$4,%eax
	movl	_C_LABEL(iunmask)(,%eax,4),%eax
	andl	_C_LABEL(ipending),%eax
	jz	2f
	bsfl    %eax,%eax               # slow, but not worth optimizing
#ifdef MULTIPROCESSOR
	lock
#endif
	btrl    %eax,_C_LABEL(ipending)
	jnc     1b			# some intr cleared the in-memory bit
	jmp	*_C_LABEL(Xresume)(,%eax,4)
2:	/* Check for ASTs on exit to user mode. */
	CHECK_ASTPENDING()
	cli
	je	3f
	testb   $SEL_RPL,TF_CS(%esp)
#ifdef VM86
	jnz	4f
	testl	$PSL_VM,TF_EFLAGS(%esp)
#endif
	jz	3f
4:	CLEAR_ASTPENDING()
	sti
	movl	$T_ASTFLT,TF_TRAPNO(%esp)	/* XXX undo later.. */
	/* Pushed T_ASTFLT into tf_trapno on entry. */
	call	_C_LABEL(trap)
	cli
	jmp	2b
3:
	INTRFASTEXIT


/*
 * Soft interrupt handlers
 */

IDTVEC(softserial)
	movl	$IPL_SOFTSERIAL,CPL
#ifdef MULTIPROCESSOR
	call	_C_LABEL(apic_intlock)
#endif
	pushl	$I386_SOFTINTR_SOFTSERIAL
	call	_C_LABEL(softintr_dispatch)
	addl	$4,%esp
#ifdef MULTIPROCESSOR	
	call	_C_LABEL(apic_intunlock)
#endif
	movl	%ebx,CPL
	jmp	*%esi

IDTVEC(softnet)
	movl	$IPL_SOFTNET,CPL
#ifdef MULTIPROCESSOR	
	call	_C_LABEL(apic_intlock)
#endif
	xorl	%edi,%edi
	xchgl	_C_LABEL(netisr),%edi

	/* XXX Do the legacy netisrs here for now. */
#define DONETISR(s, c) \
	.globl  _C_LABEL(c)	;\
	testl	$(1 << s),%edi	;\
	jz	1f		;\
	call	_C_LABEL(c)	;\
1:
#include <net/netisr_dispatch.h>
	
	pushl	$I386_SOFTINTR_SOFTNET
	call	_C_LABEL(softintr_dispatch)
	addl	$4,%esp
#ifdef MULTIPROCESSOR	
	call	_C_LABEL(apic_intunlock)	
#endif
	movl	%ebx,CPL
	jmp	*%esi

IDTVEC(softclock)
	movl	$IPL_SOFTCLOCK,CPL
#ifdef MULTIPROCESSOR	
	call	_C_LABEL(apic_intlock)
#endif
	pushl	$I386_SOFTINTR_SOFTCLOCK
	call	_C_LABEL(softintr_dispatch)
	addl	$4,%esp
#ifdef MULTIPROCESSOR	
	call	_C_LABEL(apic_intunlock)		
#endif
	movl	%ebx,CPL
	jmp	*%esi
