/*	$NetBSD: types.h,v 1.7.2.2 2004/09/18 14:40:08 skrll Exp $	*/

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_SH5_TYPES_H_
#define	_SH5_TYPES_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>
#include <sh5/int_types.h>

/* NB: This should probably be if defined(_KERNEL) */
#if defined(_NETBSD_SOURCE)
typedef	unsigned long	vm_offset_t;
typedef	unsigned long	vm_size_t;

typedef unsigned long	paddr_t;
typedef unsigned long	psize_t;
typedef unsigned long	vaddr_t;
typedef unsigned long	vsize_t;
#endif

typedef __uint64_t	register_t;

#if defined(_KERNEL)
/*
 * Define register save area for kernel setjmp()/longjmp().
 * Reserve enough space for all callee-saved registers:
 *
 *  r10 - r15	(6)
 *  r18		(1)
 *  r28 - r35	(8)
 *  r44 - r59	(16)
 *  tr5 - tr7	(3)
 */
typedef struct label_t {
	register_t val[34];
} label_t;
#endif

typedef	__volatile __int64_t	__cpu_simple_lock_t;

#define	__SIMPLELOCK_LOCKED	1
#define	__SIMPLELOCK_UNLOCKED	0

#define	__HAVE_DEVICE_REGISTER
#define	__HAVE_GENERIC_SOFT_INTERRUPTS
#define	__HAVE_SYSCALL_INTERN
#define	__HAVE_FUNCTION_DESCRIPTORS

#if defined(_KERNEL)
#define	__HAVE_RAS
#endif

#ifdef notyet
#define	__HAVE_AST_PERPROC	/* Not reqd. until we support SMP */
#endif

#endif	/* _SH5_TYPES_H_ */
