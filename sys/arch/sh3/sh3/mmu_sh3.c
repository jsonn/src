/*	$NetBSD: mmu_sh3.c,v 1.2.4.2 2002/03/16 15:59:42 jdolecek Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>

#include <sh3/mmu.h>
#include <sh3/mmu_sh3.h>

void
sh3_mmu_start()
{

	/* Zero clear all TLB entry */
	sh_tlb_reset();

	/* Set current ASID to 0 */
	sh_tlb_set_asid(0);

	/* Single virtual memory mode. */
	_reg_write_4(SH3_MMUCR, SH3_MMUCR_AT | SH3_MMUCR_TF | SH3_MMUCR_SV);
}

void
sh3_tlb_invalidate_addr(int asid, vaddr_t va)
{
	u_int32_t r, a, d;
	int w;

	d = (va & SH3_MMUAA_D_VPN_MASK_4K) | asid;  /* 4K page */
	va = SH3_MMUAA | (va & SH3_MMU_VPN_MASK);   /* [16:12] entry index */

	/* Probe entry and invalidate it. */
	for (w = 0; w < 4; w++) {
		a = va | (w << SH3_MMU_WAY_SHIFT); /* way [9:8] */
		r = _reg_read_4(a);
		if ((r & (SH3_MMUAA_D_VPN_MASK_4K | SH3_MMUAA_D_ASID_MASK))
		    == d) {
			_reg_write_4(a, 0);
			return;
		}
	}
}

void
sh3_tlb_invalidate_asid(int asid)
{
	u_int32_t aw, a;
	int e, w;
	
	/* Invalidate entry attribute to ASID */
	for (w = 0; w < SH3_MMU_WAY; w++) {
		aw = (w << SH3_MMU_WAY_SHIFT);
		for (e = 0; e < SH3_MMU_ENTRY; e++) {
			a = aw | (e << SH3_MMU_VPN_SHIFT);
			if ((_reg_read_4(SH3_MMUAA | a) & SH3_MMUAA_D_ASID_MASK)
			    == asid)
				_reg_write_4(SH3_MMUAA | a, 0);
		}
	}
}

void
sh3_tlb_invalidate_all()
{

	/* SH3 has no wired entry. so merely clear whole V bit  */
	_reg_write_4(SH3_MMUCR, _reg_read_4(SH3_MMUCR) | SH3_MMUCR_TF);
}

void
sh3_tlb_reset()
{
	u_int32_t aw, a;
	int e, w;

	/* Zero clear all TLB entry */
	for (w = 0; w < SH3_MMU_WAY; w++) {
		aw = (w << SH3_MMU_WAY_SHIFT);
		for (e = 0; e < SH3_MMU_ENTRY; e++) {
			a = aw | (e << SH3_MMU_VPN_SHIFT);
			_reg_write_4(SH3_MMUAA | a, 0);
			_reg_write_4(SH3_MMUDA | a, 0);		
		}
	}
}
