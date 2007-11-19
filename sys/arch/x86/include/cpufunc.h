/*	$NetBSD: cpufunc.h,v 1.2 2007/09/26 23:48:37 ad Exp $	*/

/*-
 * Copyright (c) 1998, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, and by Andrew Doran.
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

#ifndef _X86_CPUFUNC_H_
#define	_X86_CPUFUNC_H_

/*
 * Functions to provide access to x86-specific instructions.
 */

#include <sys/cdefs.h>
#include <sys/types.h>

#include <machine/segments.h>
#include <machine/specialreg.h>

#ifdef _KERNEL

void	x86_pause(void);
void	x86_lfence(void);
void	x86_sfence(void);
void	x86_mfence(void);
void	x86_flush(void);
void	x86_patch(void);
void	invlpg(vaddr_t);
void	lidt(struct region_descriptor *);
void	lldt(u_short);
void	ltr(u_short);
void	lcr0(u_int);
u_int	rcr0(void);
vaddr_t	rcr2(void);
void	lcr3(vaddr_t);
vaddr_t	rcr3(void);
void	lcr4(vaddr_t);
vaddr_t	rcr4(void);
void	lcr8(vaddr_t);
vaddr_t	rcr8(void);
void	tlbflush(void);
void	tlbflushg(void);
void	dr0(void *, uint32_t, uint32_t, uint32_t);
vaddr_t	rdr6(void);
void	ldr6(vaddr_t);
void	wbinvd(void);
void	breakpoint(void);
void	x86_hlt(void);
void	x86_stihlt(void);
u_int	x86_getss(void);
void	fldcw(void *);
void	fnclex(void);
void	fninit(void);
void	fnsave(void *);
void	fnstcw(void *);
void	fnstsw(void *);
void	fp_divide_by_0(void);
void	frstor(void *);
void	fwait(void);
void	clts(void);
void	stts(void);
void	fldummy(const double *);
void	fxsave(void *);
void	fxrstor(void *);
void	x86_monitor(const void *, uint32_t, uint32_t);
void	x86_mwait(uint32_t, uint32_t);
void	x86_ldmxcsr(void *);
void	x86_cpuid(unsigned, unsigned *);

/* Use read_psl, write_psl when saving and restoring interrupt state. */
void	x86_disable_intr(void);
void	x86_enable_intr(void);
u_long	x86_read_psl(void);
void	x86_write_psl(u_long);

/* Use read_flags, write_flags to adjust other members of %eflags. */
u_long	x86_read_flags(void);
void	x86_write_flags(u_long);

/* 
 * Some of the undocumented AMD64 MSRs need a 'passcode' to access.
 *
 * See LinuxBIOSv2: src/cpu/amd/model_fxx/model_fxx_init.c
 */

#define	OPTERON_MSR_PASSCODE	0x9c5a203aU

uint64_t	rdmsr(u_int);
u_int64_t	rdmsr_locked(u_int, u_int);
uint64_t	rdtsc(void);
uint64_t	rdpmc(u_int);
void		wrmsr(u_int, uint64_t);
void		wrmsr_locked(u_int, u_int, u_int64_t);

/*
 * XXX Maybe these don't belong here...
 */

extern int (*copyout_func)(const void *, void *, size_t);
extern int (*copyin_func)(const void *, void *, size_t);

int	i386_copyout(const void *, void *, size_t);
int	i486_copyout(const void *, void *, size_t);

int	i386_copyin(const void *, void *, size_t);

#endif /* _KERNEL */

#endif /* !_X86_CPUFUNC_H_ */
