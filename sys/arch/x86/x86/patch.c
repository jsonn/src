/*	$NetBSD: patch.c,v 1.14.2.2 2009/03/03 18:29:37 skrll Exp $	*/

/*-
 * Copyright (c) 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * Patch kernel code at boot time, depending on available CPU features.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: patch.c,v 1.14.2.2 2009/03/03 18:29:37 skrll Exp $");

#include "opt_lockdebug.h"

#include <sys/types.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#include <x86/cpuvar.h>
#include <x86/cputypes.h>

void	spllower(int);
void	spllower_end(void);
void	cx8_spllower(int);
void	cx8_spllower_end(void);
void	cx8_spllower_patch(void);

void	mutex_spin_exit_end(void);
void	i686_mutex_spin_exit(int);
void	i686_mutex_spin_exit_end(void);
void	i686_mutex_spin_exit_patch(void);

void	membar_consumer(void);
void	membar_consumer_end(void);
void	membar_sync(void);
void	membar_sync_end(void);
void	sse2_lfence(void);
void	sse2_lfence_end(void);
void	sse2_mfence(void);
void	sse2_mfence_end(void);

void	_atomic_cas_64(void);
void	_atomic_cas_64_end(void);
void	_atomic_cas_cx8(void);
void	_atomic_cas_cx8_end(void);

extern void	*x86_lockpatch[];
extern void	*atomic_lockpatch[];

#define	X86_NOP		0x90
#define	X86_REP		0xf3
#define	X86_RET		0xc3
#define	X86_CS		0x2e
#define	X86_DS		0x3e
#define	X86_GROUP_0F	0x0f

static void __unused
patchfunc(void *from_s, void *from_e, void *to_s, void *to_e,
	  void *pcrel)
{
	uint8_t *ptr;

	if ((uintptr_t)from_e - (uintptr_t)from_s !=
	    (uintptr_t)to_e - (uintptr_t)to_s)
		panic("patchfunc: sizes do not match (from=%p)", from_s);

	memcpy(to_s, from_s, (uintptr_t)to_e - (uintptr_t)to_s);
	if (pcrel != NULL) {
		ptr = pcrel;
		/* Branch hints */
		if (ptr[0] == X86_CS || ptr[0] == X86_DS)
			ptr++;
		/* Conditional jumps */
		if (ptr[0] == X86_GROUP_0F)
			ptr++;		
		/* 4-byte relative jump or call */
		*(uint32_t *)(ptr + 1 - (uintptr_t)from_s + (uintptr_t)to_s) +=
		    ((uint32_t)(uintptr_t)from_s - (uint32_t)(uintptr_t)to_s);
	}
}

static inline void __unused
patchbytes(void *addr, const int byte1, const int byte2)
{

	((uint8_t *)addr)[0] = (uint8_t)byte1;
	if (byte2 != -1)
		((uint8_t *)addr)[1] = (uint8_t)byte2;
}

void
x86_patch(bool early)
{
	static bool first, second;
	u_long psl;
	u_long cr0;

	if (early) {
		if (first)
			return;
		first = true;
	} else {
		if (second)
			return;
		second = true;
	}

	/* Disable interrupts. */
	psl = x86_read_psl();
	x86_disable_intr();

	/* Disable write protection in supervisor mode. */
	cr0 = rcr0();
	lcr0(cr0 & ~CR0_WP);

#if !defined(GPROF)
	if (!early && ncpu == 1) {
#ifndef LOCKDEBUG
		int i;

		/* Uniprocessor: kill LOCK prefixes. */
		for (i = 0; x86_lockpatch[i] != 0; i++)
			patchbytes(x86_lockpatch[i], X86_NOP, -1);	
		for (i = 0; atomic_lockpatch[i] != 0; i++)
			patchbytes(atomic_lockpatch[i], X86_NOP, -1);
#endif	/* !LOCKDEBUG */
	}
	if (!early && (cpu_feature & CPUID_SSE2) != 0) {
		/* Faster memory barriers. */
		patchfunc(
		    sse2_lfence, sse2_lfence_end,
		    membar_consumer, membar_consumer_end,
		    NULL
		);
		patchfunc(
		    sse2_mfence, sse2_mfence_end,
		    membar_sync, membar_sync_end,
		    NULL
		);
	}
#endif	/* GPROF */

#ifdef i386
	/*
	 * Patch early and late.  Second time around the 'lock' prefix
	 * may be gone.
	 */
	if ((cpu_feature & CPUID_CX8) != 0) {
		patchfunc(
		    _atomic_cas_cx8, _atomic_cas_cx8_end,
		    _atomic_cas_64, _atomic_cas_64_end,
		    NULL
		);
	}
#endif	/* i386 */

	if (!early && (cpu_feature & CPUID_CX8) != 0) {
		/* Faster splx(), mutex_spin_exit(). */
		patchfunc(
		    cx8_spllower, cx8_spllower_end,
		    spllower, spllower_end,
		    cx8_spllower_patch
		);
#if defined(i386) && !defined(LOCKDEBUG)
		patchfunc(
		    i686_mutex_spin_exit, i686_mutex_spin_exit_end,
		    mutex_spin_exit, mutex_spin_exit_end,
		    i686_mutex_spin_exit_patch
		);
#endif	/* !LOCKDEBUG */
	}

	/* Write back and invalidate cache, flush pipelines. */
	wbinvd();
	x86_flush();
	x86_write_psl(psl);

	/* Re-enable write protection. */
	lcr0(cr0);
}
