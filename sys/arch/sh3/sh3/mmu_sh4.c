/*	$NetBSD: mmu_sh4.c,v 1.6.6.3 2004/09/21 13:21:34 skrll Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: mmu_sh4.c,v 1.6.6.3 2004/09/21 13:21:34 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <sh3/pte.h>	/* NetBSD/sh3 specific PTE */
#include <sh3/mmu.h>
#include <sh3/mmu_sh4.h>

#define	SH4_MMU_HAZARD	__asm__ __volatile__("nop;nop;nop;nop;nop;nop;nop;nop;")

static __inline__ void __sh4_itlb_invalidate_all(void);

static __inline__ void
__sh4_itlb_invalidate_all()
{

	_reg_write_4(SH4_ITLB_AA, 0);
	_reg_write_4(SH4_ITLB_AA | (1 << SH4_ITLB_E_SHIFT), 0);
	_reg_write_4(SH4_ITLB_AA | (2 << SH4_ITLB_E_SHIFT), 0);
	_reg_write_4(SH4_ITLB_AA | (3 << SH4_ITLB_E_SHIFT), 0);
}

void
sh4_mmu_start()
{

	/* Zero clear all TLB entry */
	_reg_write_4(SH4_MMUCR, 0);	/* zero wired entry */
	sh_tlb_invalidate_all();

	/* Set current ASID to 0 */
	sh_tlb_set_asid(0);

	/*
	 * User can't access store queue
	 * make wired entry for u-area.
	 */
	_reg_write_4(SH4_MMUCR, SH4_MMUCR_AT | SH4_MMUCR_TI | SH4_MMUCR_SQMD |
	    (SH4_UTLB_ENTRY - UPAGES) << SH4_MMUCR_URB_SHIFT);

	SH4_MMU_HAZARD;
}

void
sh4_tlb_invalidate_addr(int asid, vaddr_t va)
{
	u_int32_t pteh;
	va &= SH4_PTEH_VPN_MASK;

	/* Save current ASID */
	pteh = _reg_read_4(SH4_PTEH);
	/* Set ASID for associative write */
	_reg_write_4(SH4_PTEH, asid);

	/* Associative write(UTLB/ITLB). not required ITLB invalidate. */
	RUN_P2;
	_reg_write_4(SH4_UTLB_AA | SH4_UTLB_A, va); /* Clear D, V */
	RUN_P1;
	/* Restore ASID */
	_reg_write_4(SH4_PTEH, pteh);
}

void
sh4_tlb_invalidate_asid(int asid)
{
	u_int32_t a;
	int e;

	/* Invalidate entry attribute to ASID */
	RUN_P2;
	for (e = 0; e < SH4_UTLB_ENTRY; e++) {
		a = SH4_UTLB_AA | (e << SH4_UTLB_E_SHIFT);
		if ((_reg_read_4(a) & SH4_UTLB_AA_ASID_MASK) == asid)
			_reg_write_4(a, 0);
	}

	__sh4_itlb_invalidate_all();
	RUN_P1;
}

void
sh4_tlb_invalidate_all()
{
	u_int32_t a;
	int e, eend;

	/* If non-wired entry limit is zero, clear all entry. */
	a = _reg_read_4(SH4_MMUCR) & SH4_MMUCR_URB_MASK;
	eend = a ? (a >> SH4_MMUCR_URB_SHIFT) : SH4_UTLB_ENTRY;

	RUN_P2;
	for (e = 0; e < eend; e++) {
		a = SH4_UTLB_AA | (e << SH4_UTLB_E_SHIFT);
		_reg_write_4(a, 0);
		a = SH4_UTLB_DA1 | (e << SH4_UTLB_E_SHIFT);
		_reg_write_4(a, 0);
	}
	__sh4_itlb_invalidate_all();
	_reg_write_4(SH4_ITLB_DA1, 0);
	_reg_write_4(SH4_ITLB_DA1 | (1 << SH4_ITLB_E_SHIFT), 0);
	_reg_write_4(SH4_ITLB_DA1 | (2 << SH4_ITLB_E_SHIFT), 0);
	_reg_write_4(SH4_ITLB_DA1 | (3 << SH4_ITLB_E_SHIFT), 0);
	RUN_P1;
}

void
sh4_tlb_update(int asid, vaddr_t va, u_int32_t pte)
{
	u_int32_t oasid;
	u_int32_t ptel;

	KDASSERT(asid < 0x100 && (pte & ~PGOFSET) != 0 && va != 0);

	/* Save old ASID */
	oasid = _reg_read_4(SH4_PTEH) & SH4_PTEH_ASID_MASK;

	/* Invalidate old entry (if any) */
	sh4_tlb_invalidate_addr(asid, va);

	_reg_write_4(SH4_PTEH, asid);
	/* Load new entry */
	_reg_write_4(SH4_PTEH, (va & ~PGOFSET) | asid);
	ptel = pte & PG_HW_BITS;
	if (pte & _PG_PCMCIA) {
		_reg_write_4(SH4_PTEA,
		    (pte >> _PG_PCMCIA_SHIFT) & SH4_PTEA_SA_MASK);
	} else {
		_reg_write_4(SH4_PTEA, 0);
	}
	_reg_write_4(SH4_PTEL, ptel);
	__asm__ __volatile__("ldtlb; nop");

	/* Restore old ASID */
	if (asid != oasid)
		_reg_write_4(SH4_PTEH, oasid);
}
