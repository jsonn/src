/*	$NetBSD: mmu.c,v 1.9.6.1 2005/01/10 18:42:22 jmc Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mmu.c,v 1.9.6.1 2005/01/10 18:42:22 jmc Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <sh3/mmu.h>
#include <sh3/mmu_sh3.h>
#include <sh3/mmu_sh4.h>

/* Start MMU. call after exception vector is setuped. */
void (*__sh_mmu_start)(void);

/* TLB access ops. */
void (*__sh_tlb_invalidate_addr)(int, vaddr_t);
void (*__sh_tlb_invalidate_asid)(int);
void (*__sh_tlb_invalidate_all)(void);
void (*__sh_tlb_update)(int, vaddr_t, u_int32_t);

void
sh_mmu_init()
{
	/*
	 * Asign function hook. but if only defined SH3 or SH4, they are called
	 * directly. see sh3/mmu.h
	 */
#ifdef SH3
	if (CPU_IS_SH3) {
		__sh_mmu_start = sh3_mmu_start;
		__sh_tlb_invalidate_addr = sh3_tlb_invalidate_addr;
		__sh_tlb_invalidate_asid = sh3_tlb_invalidate_asid;
		__sh_tlb_invalidate_all = sh3_tlb_invalidate_all;
		__sh_tlb_update = sh3_tlb_update;
	}
#endif
#ifdef SH4
	if (CPU_IS_SH4) {
		__sh_mmu_start = sh4_mmu_start;
		__sh_tlb_invalidate_addr = sh4_tlb_invalidate_addr;
		__sh_tlb_invalidate_asid = sh4_tlb_invalidate_asid;
		__sh_tlb_invalidate_all = sh4_tlb_invalidate_all;
		__sh_tlb_update = sh4_tlb_update;
	}
#endif
}

void
sh_mmu_information()
{
	u_int32_t r;
#ifdef SH3
	if (CPU_IS_SH3) {
		printf("4-way set-associative 128 TLB entries\n");
		r = _reg_read_4(SH3_MMUCR);
		printf("%s mode, %s virtual storage mode\n",
		    r & SH3_MMUCR_IX
		    ? "ASID+VPN" : "VPN",
		    r & SH3_MMUCR_SV ? "single" : "multiple");
	}
#endif
#ifdef SH4
	if (CPU_IS_SH4) {
		printf("full-associative 4 ITLB, 64 UTLB entries\n");
		r = _reg_read_4(SH4_MMUCR);
		printf("%s virtual storage mode, SQ access: kernel%s, ",
		    r & SH3_MMUCR_SV ? "single" : "multiple",
		    r & SH4_MMUCR_SQMD ? "" : "/user");
		printf("wired %d\n", (r & SH4_MMUCR_URB_MASK) >>
		    SH4_MMUCR_URB_SHIFT);
	}
#endif
}

void
sh_tlb_set_asid(int asid)
{
	int s;

	s = _cpu_exception_suspend();
	_reg_write_4(SH_(PTEH), asid);
	_cpu_exception_resume(s);
}
