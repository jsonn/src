/*	$NetBSD: lock_stubs.s,v 1.1.2.1 2002/03/17 21:28:53 thorpej Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

/*
 * Assembly language lock stubs.  These handle the common ("easy")
 * cases for mutexes, and provide helper routines for mutexes and
 * rwlocks.
 */

#include "opt_cputype.h"

#include <machine/asm.h>

#include "assym.h"

NENTRY(mutex_enter)
#if defined(I386_CPU)
#error "I386_CPU not handled yet"
#endif
	movl	_C_LABEL(curproc), %ecx		/* current thread */
	movl	4(%esp), %edx			/* lock address */
	xorl	%eax, %eax			/* expected contents (0) */
	lock
	cmpxchgl %ecx, (%edx)			/* compare-and-swap */
	jnz	_C_LABEL(mutex_vector_enter)	/* failed; hard case */
	ret

NENTRY(mutex_tryenter)
#if defined(I386_CPU)
#error "I386_CPU not handled yet"
#endif
	movl	_C_LABEL(curproc), %ecx		/* current thread */
	movl	4(%esp), %edx			/* lock address */
	xorl	%eax, %eax			/* expected contents (0) */
	lock
	cmpxchgl %ecx, (%edx)			/* compare-and-swap */
	jnz	_C_LABEL(mutex_vector_tryenter)	/* failed; hard case */
	movl	$1, %eax			/* succeeded; return true */
	ret

/*
 * THIS IS IMPORTANT WHEN WE ADD SUPPORT FOR KERNEL PREEMPTION.
 *
 * There is a critical section within mutex_exit(); if we are
 * preempted, between checking for waiters and releasing the
 * lock, then we must check for waiters again.
 *
 * On some architectures, we could simply disable interrupts
 * (and, thus, preemption) for the short duration of this 
 * critical section.
 *      
 * On others, where disabling interrupts might be too expensive,
 * a restartable sequence could be used; in the interrupt handler,
 * if the PC is within the critical section, then then PC should
 * be reset to the beginning of the critical section so that the
 * sequence will be restarted when we are resumed.  NOTE: In this
 * case, it is very important that the insn that actually clears
 * the lock must NEVER be executed twice.
 *
 * On the i386, we just disable interrupts for the short time we
 * manipulate the lock.
 */
NENTRY(mutex_exit)
	movl	_C_LABEL(curproc), %ecx		/* current thread */
	movl	4(%esp), %edx			/* lock address */

	/* BEGIN CRITICAL SECTION */
	cli					/* ints off */
	cmpl	%ecx, (%edx)			/* same? */
	jne	2f				/* nope, hard case */
	movl	$0, (%edx)			/* release lock */
	sti					/* ints on */
	/* END CRITICAL SECTION */
	ret

2:	sti					/* ints on */
	jmp	_C_LABEL(mutex_vector_exit)

NENTRY(_mutex_set_waiters)
#if defined(I386_CPU)
#error "I386_CPU not handled yet"
#endif  
	movl	4(%esp), %edx			/* lock address */
1:	movl	(%edx), %eax			/* expected contents */
	cmpl	$0, %eax			/* lock unowned? */
	jz	2f				/* yup, bail */
	movl	%eax, %ecx
	orl	$MUTEX_WAITERS, %ecx		/* new value */
	lock
	cmpxchgl %ecx, (%edx)			/* compare-and-swap */
	jnz	1b				/* failed, try again */
2:	ret

NENTRY(_rwlock_cas)
#if defined(I386_CPU)
#error "I386_CPU not handled yet"
#endif
	movl	4(%esp), %edx			/* lock address */
	movl	8(%esp), %eax			/* expected value */
	movl	12(%esp), %ecx			/* new value */
	lock
	cmpxchgl %ecx, (%edx)			/* compare-and-swap */
	jnz	1f				/* failed */
	movl	$1, %eax			/* succeeded */
	ret

1:	xorl	%eax, %eax
	ret

NENTRY(_rwlock_set_waiters)
#if defined(I386_CPU)
#error "I386_CPU not handled yet"
#endif
	movl	4(%esp), %edx			/* lock address */
1:	movl	12(%esp), %ecx			/* set_wait value */
	movl	(%edx), %eax			/* lock value */
	test	%eax, 8(%esp)			/* test need_wait */
	jz	2f				/* nope, don't need to */
	orl	%eax, %ecx			/* construct new value */
	lock
	cmpxchgl %ecx, (%edx)			/* compare-and-swap */
	jnz	1b				/* failed, try again */
2:	ret					/* succeeded */
