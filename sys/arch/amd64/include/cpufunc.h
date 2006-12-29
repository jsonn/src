/*	$NetBSD: cpufunc.h,v 1.9.2.1 2006/12/29 20:27:41 ad Exp $	*/

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

#ifndef _AMD64_CPUFUNC_H_
#define	_AMD64_CPUFUNC_H_

/*
 * Functions to provide access to i386-specific instructions.
 */

#include <sys/cdefs.h>
#include <sys/types.h>

#include <machine/segments.h>
#include <machine/specialreg.h>

static __inline void
x86_pause(void)
{
	extern int nullop(void *);
	(void)nullop((void *)0);
}

/*
 * XXX if lfence isn't available...
 *
 * memory clobber to avoid compiler reordering.
 */
static __inline void
x86_lfence(void)
{

	__asm volatile("lfence" : : : "memory");
}

static __inline void
x86_sfence(void)
{

	__asm volatile("sfence" : : : "memory");
}

static __inline void
x86_mfence(void)
{

	__asm volatile("mfence" : : : "memory");
}

#ifdef _KERNEL

extern int cpu_feature;

static __inline void 
invlpg(u_int64_t addr)
{ 
        __asm volatile("invlpg (%0)" : : "r" (addr) : "memory");
}  

static __inline void
lidt(struct region_descriptor *region)
{
	__asm volatile("lidt %0" : : "m" (*region));
}

static __inline void
lldt(u_short sel)
{
	__asm volatile("lldt %0" : : "r" (sel));
}

static __inline void
ltr(u_short sel)
{
	__asm volatile("ltr %0" : : "r" (sel));
}

static __inline void
lcr8(u_int val)
{
	u_int64_t val64 = val;
	__asm volatile("movq %0,%%cr8" : : "r" (val64));
}

/*
 * Upper 32 bits are reserved anyway, so just keep this 32bits.
 */
static __inline void
lcr0(u_int val)
{
	u_int64_t val64 = val;
	__asm volatile("movq %0,%%cr0" : : "r" (val64));
}

static __inline u_int
rcr0(void)
{
	u_int64_t val64;
	u_int val;
	__asm volatile("movq %%cr0,%0" : "=r" (val64));
	val = val64;
	return val;
}

static __inline u_int64_t
rcr2(void)
{
	u_int64_t val;
	__asm volatile("movq %%cr2,%0" : "=r" (val));
	return val;
}

static __inline void
lcr3(u_int64_t val)
{
	__asm volatile("movq %0,%%cr3" : : "r" (val));
}

static __inline u_int64_t
rcr3(void)
{
	u_int64_t val;
	__asm volatile("movq %%cr3,%0" : "=r" (val));
	return val;
}

/*
 * Same as for cr0. Don't touch upper 32 bits.
 */
static __inline void
lcr4(u_int val)
{
	u_int64_t val64 = val;

	__asm volatile("movq %0,%%cr4" : : "r" (val64));
}

static __inline u_int
rcr4(void)
{
	u_int val;
	u_int64_t val64;
	__asm volatile("movq %%cr4,%0" : "=r" (val64));
	val = val64;
	return val;
}

static __inline void
tlbflush(void)
{
	u_int64_t val;
	__asm volatile("movq %%cr3,%0" : "=r" (val));
	__asm volatile("movq %0,%%cr3" : : "r" (val));
}

static __inline void
tlbflushg(void)
{
	/*
	 * Big hammer: flush all TLB entries, including ones from PTE's
	 * with the G bit set.  This should only be necessary if TLB
	 * shootdown falls far behind.
	 *
	 * Intel Architecture Software Developer's Manual, Volume 3,
	 *	System Programming, section 9.10, "Invalidating the
	 * Translation Lookaside Buffers (TLBS)":
	 * "The following operations invalidate all TLB entries, irrespective
	 * of the setting of the G flag:
	 * ...
	 * "(P6 family processors only): Writing to control register CR4 to
	 * modify the PSE, PGE, or PAE flag."
	 *
	 * (the alternatives not quoted above are not an option here.)
	 *
	 * If PGE is not in use, we reload CR3 for the benefit of
	 * pre-P6-family processors.
	 */

	if (cpu_feature & CPUID_PGE) {
		u_int cr4 = rcr4();
		lcr4(cr4 & ~CR4_PGE);
		lcr4(cr4);
	} else
		tlbflush();
}

#ifdef notyet
void	setidt	__P((int idx, /*XXX*/caddr_t func, int typ, int dpl));
#endif


/* XXXX ought to be in psl.h with spl() functions */

static __inline void
disable_intr(void)
{
	__asm volatile("cli");
}

static __inline void
enable_intr(void)
{
	__asm volatile("sti");
}

static __inline u_long
read_rflags(void)
{
	u_long	ef;

	__asm volatile("pushfq; popq %0" : "=r" (ef));
	return (ef);
}

static __inline void
write_rflags(u_long ef)
{
	__asm volatile("pushq %0; popfq" : : "r" (ef));
}

static __inline u_int64_t
rdmsr(u_int msr)
{
	uint32_t hi, lo;
	__asm volatile("rdmsr" : "=d" (hi), "=a" (lo) : "c" (msr));
	return (((uint64_t)hi << 32) | (uint64_t) lo);
}

static __inline void
wrmsr(u_int msr, u_int64_t newval)
{
	__asm volatile("wrmsr" :
	    : "a" (newval & 0xffffffff), "d" (newval >> 32), "c" (msr));
}

static __inline void
wbinvd(void)
{
	__asm volatile("wbinvd");
}

static __inline u_int64_t
rdtsc(void)
{
	uint32_t hi, lo;

	__asm volatile("rdtsc" : "=d" (hi), "=a" (lo));
	return (((uint64_t)hi << 32) | (uint64_t) lo);
}

static __inline u_int64_t
rdpmc(u_int pmc)
{
	uint32_t hi, lo;

	__asm volatile("rdpmc" : "=d" (hi), "=a" (lo) : "c" (pmc));
	return (((uint64_t)hi << 32) | (uint64_t) lo);
}

/* Break into DDB/KGDB. */
static __inline void
breakpoint(void)
{
	__asm volatile("int $3");
}

#define read_psl()	read_rflags()
#define write_psl(x)	write_rflags(x)

#endif /* _KERNEL */

#endif /* !_AMD64_CPUFUNC_H_ */
