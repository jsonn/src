/*	$NetBSD: profile.h,v 1.26.8.1 2007/07/11 20:00:09 mjf Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)profile.h	8.1 (Berkeley) 6/11/93
 */

#ifdef _KERNEL_OPT
#include "opt_multiprocessor.h"
#endif

#include <machine/cpufunc.h>
#include <machine/atomic.h>

#define	_MCOUNT_DECL static __inline void _mcount

#ifdef __ELF__
#define MCOUNT_ENTRY	"__mcount"
#define MCOUNT_COMPAT	__weak_alias(mcount, __mcount)
#else
#define MCOUNT_ENTRY	"mcount"
#define MCOUNT_COMPAT	/* nothing */
#endif

#define	MCOUNT \
MCOUNT_COMPAT								\
extern void mcount(void) __asm(MCOUNT_ENTRY)				\
	__attribute__((__no_instrument_function__));			\
void									\
mcount(void)								\
{									\
	int selfpc, frompcindex;					\
	int eax, ecx, edx;						\
									\
	__asm volatile("movl %%eax,%0" : "=g" (eax));			\
	__asm volatile("movl %%ecx,%0" : "=g" (ecx));			\
	__asm volatile("movl %%edx,%0" : "=g" (edx));			\
	/*								\
	 * find the return address for mcount,				\
	 * and the return address for mcount's caller.			\
	 *								\
	 * selfpc = pc pushed by mcount call				\
	 */								\
	__asm volatile("movl 4(%%ebp),%0" : "=r" (selfpc));		\
	/*								\
	 * frompcindex = pc pushed by call into self.			\
	 */								\
	__asm volatile("movl (%%ebp),%0;movl 4(%0),%0"			\
	    : "=r" (frompcindex));					\
	_mcount((u_long)frompcindex, (u_long)selfpc);			\
									\
	__asm volatile("movl %0,%%edx" : : "g" (edx));			\
	__asm volatile("movl %0,%%ecx" : : "g" (ecx));			\
	__asm volatile("movl %0,%%eax" : : "g" (eax));			\
}

#ifdef _KERNEL
#ifdef MULTIPROCESSOR
__cpu_simple_lock_t __mcount_lock;

static inline void
MCOUNT_ENTER_MP(void)
{
	while (x86_atomic_testset_b(&__mcount_lock, __SIMPLELOCK_LOCKED)
	    != __SIMPLELOCK_UNLOCKED) {
		while (__mcount_lock == __SIMPLELOCK_LOCKED)
			;
	}
	__insn_barrier();
}

static inline void
MCOUNT_EXIT_MP(void)
{
	__insn_barrier();
	__mcount_lock = __SIMPLELOCK_UNLOCKED;
}
#else
#define MCOUNT_ENTER_MP()
#define MCOUNT_EXIT_MP()
#endif

#define	MCOUNT_ENTER							\
	s = (int)read_psl();						\
	disable_intr();							\
	MCOUNT_ENTER_MP();

#define	MCOUNT_EXIT							\
	MCOUNT_EXIT_MP();						\
	write_psl(s);

#endif /* _KERNEL */
