/*	$NetBSD: types.h,v 1.42.56.1 2007/12/08 18:17:23 mjf Exp $	*/

/*-
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)types.h	8.3 (Berkeley) 1/5/94
 */

#ifndef	_MACHTYPES_H_
#define	_MACHTYPES_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>
#include <mips/int_types.h>

/*
 * Note that mips_reg_t is distinct from the register_t defined
 * in <types.h> to allow these structures to be as hidden from
 * the rest of the operating system as possible.
 */

#if defined(_MIPS_BSD_API) && _MIPS_BSD_API != _MIPS_BSD_API_LP32
typedef long long	mips_reg_t;
typedef unsigned long long mips_ureg_t;
#if _MIPS_BSD_API != _MIPS_BSD_API_LP32 && _MIPS_BSD_API != _MIPS_BSD_API_LP32_64CLEAN
typedef	long long	mips_fpreg_t;
#else
typedef	int		mips_fpreg_t;
#endif
#else
typedef long		mips_reg_t;
typedef unsigned long	mips_ureg_t;
typedef	long		mips_fpreg_t;
#endif

/* NB: This should probably be if defined(_KERNEL) */
#if defined(_NETBSD_SOURCE)
#if defined(_MIPS_PADDR_T_64BIT) && !defined(_LP64)
typedef unsigned long long	paddr_t;
typedef unsigned long long	psize_t;
#else
typedef unsigned long	paddr_t;
typedef unsigned long	psize_t;
#endif
typedef unsigned long	vaddr_t;
typedef unsigned long	vsize_t;
#endif

/* Make sure this is signed; we need pointers to be sign-extended. */
#if defined(__mips_n32)
typedef long long	register_t;
#else
typedef long		register_t;
#endif /* __mips_n32 */

#if defined(_KERNEL)
typedef struct label_t {
	register_t val[12];
} label_t;
#endif

typedef	volatile int		__cpu_simple_lock_t;

#define	__SIMPLELOCK_LOCKED	1
#define	__SIMPLELOCK_UNLOCKED	0

#define	__SWAP_BROKEN

#define	__HAVE_AST_PERPROC
#define	__HAVE_SYSCALL_INTERN
#ifdef MIPS3_PLUS	/* XXX bogus! */
#define	__HAVE_CPU_COUNTER
#endif

#ifdef _LP64
#define	__HAVE_ATOMIC64_OPS
#endif

#if defined(_KERNEL)
#define	__HAVE_RAS
#endif

#endif	/* _MACHTYPES_H_ */
