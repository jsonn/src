/* -*-C++-*-	$NetBSD: sh3.h,v 1.1.2.2 2001/02/11 19:10:11 bouyer Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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

#ifndef _HPCBOOT_SH3_H_
#define _HPCBOOT_SH3_H_
/*
 * SH3 for Windows CE(SH7709, SH7709A) common defines.
 */

/*
 * Address space.
 */
#define SH_P0_START		0x0
#define SH_P1_START		0x80000000
#define SH_P2_START		0xa0000000
#define SH_P3_START		0xc0000000
#define SH_P4_START		0xe0000000

/* 
 * D-RAM(CS3)
 */
#define DRAM_BANK_NUM		2
#define DRAM_BANK_SIZE		0x02000000	/* 32MByte */

#define DRAM_BANK0_START	0x0c000000
#define DRAM_BANK0_SIZE		DRAM_BANK_SIZE
#define DRAM_BANK1_START	0x0e000000
#define DRAM_BANK1_SIZE		DRAM_BANK_SIZE

/* 
 * MMU 
 */
/* 4-way set-associative 32-entry(total 128 entries) */
#define MMU_WAY			4
#define MMU_ENTRY		32

/* Windows CE uses 1Kbyte page */
#define PAGE_SIZE		0x400
#define PAGE_MASK		(~(PAGE_SIZE - 1))

#define MMUPTEH			0xfffffff0
#define MMUPTEH_ASID_MASK	0x0000000f
#define MMUPTEH_VPN_MASK	0xfffffc00
#define MMUCR			0xffffffe0
#define MMUCR_AT		0x00000001
#define MMUCR_IX		0x00000002
#define MMUCR_TF		0x00000004
#define MMUCR_RC		0x00000030
#define MMUCR_SV		0x00000100

#define MMUAA			0xf2000000	/* Address array */
#define MMUDA			0xf3000000	/* Data array */

#define MMUAA_VPN_MASK		0x0001f000
#define MMUAA_WAY_SHIFT		8
#define MMUAA_D_VALID		0x00000100
#define MMUAA_D_VPN_MASK	0xfffe0c00
#define MMUAA_D_ASID_MASK	0x0000000f

#define MMUDA_D_PPN_MASK	0xfffffc00

/* 
 * Cache (Windows CE uses normal-mode.)
 */
#define CCR			0xffffffec
#define CCR_CE			0x00000001
#define CCR_WT			0x00000002
#define CCR_CB			0x00000004
#define CCR_CF			0x00000008
#define CCR_RA			0x00000020

#define CCA			0xf0000000
#define CCD			0xf1000000

/*
 * Interrupt
 */
/* R/W 16bit */
#define ICU_ICR0_REG16		0xfffffee0
#define ICU_ICR1_REG16		0x04000010
#define ICU_ICR2_REG16		0x04000012
#define ICU_PINTER_REG16	0x04000014
#define ICU_IPRA_REG16		0xfffffee2
#define ICU_IPRB_REG16		0xfffffee4
#define ICU_IPRC_REG16		0x04000016
#define ICU_IPRD_REG16		0x04000018
#define ICU_IPRE_REG16		0x0400001a
/* R/W 8bit */
#define ICU_IRR0_REG8		0x04000004
/* R 8bit */
#define ICU_IRR1_REG8		0x04000006
#define ICU_IRR2_REG8		0x04000008

#define ICU_ICR0_NMIL		0x8000
#define ICU_ICR0_NMIE		0x0100

#define ICU_ICR1_MAI		0x8000
#define ICU_ICR1_IRQLVL		0x4000
#define ICU_ICR1_BLMSK		0x2000
#define ICU_ICR1_IRLSEN		0x1000
#define ICU_ICR1_IRQ51S		0x0800
#define ICU_ICR1_IRQ50S		0x0400
#define ICU_ICR1_IRQ41S		0x0200
#define ICU_ICR1_IRQ40S		0x0100
#define ICU_ICR1_IRQ31S		0x0080
#define ICU_ICR1_IRQ30S		0x0040
#define ICU_ICR1_IRQ21S		0x0020
#define ICU_ICR1_IRQ20S		0x0010
#define ICU_ICR1_IRQ11S		0x0008
#define ICU_ICR1_IRQ10S		0x0004
#define ICU_ICR1_IRQ01S		0x0002
#define ICU_ICR1_IRQ00S		0x0001

#define ICU_SENSE_SELECT_MASK		0x3
#define ICU_SENSE_SELECT_FALLING_EDGE	0x0
#define ICU_SENSE_SELECT_RAISING_EDGE	0x1
#define ICU_SENSE_SELECT_LOW_LEVEL	0x2
#define ICU_SENSE_SELECT_RESERVED	0x3

#define ICU_ICR2_PINT15S	0x8000
#define ICU_ICR2_PINT14S	0x4000
#define ICU_ICR2_PINT13S	0x2000
#define ICU_ICR2_PINT12S	0x1000
#define ICU_ICR2_PINT11S	0x0800
#define ICU_ICR2_PINT10S	0x0400
#define ICU_ICR2_PINT9S		0x0200
#define ICU_ICR2_PINT8S		0x0100
#define ICU_ICR2_PINT7S		0x0080
#define ICU_ICR2_PINT6S		0x0040
#define ICU_ICR2_PINT5S		0x0020
#define ICU_ICR2_PINT4S		0x0010
#define ICU_ICR2_PINT3S		0x0008
#define ICU_ICR2_PINT2S		0x0004
#define ICU_ICR2_PINT1S		0x0002
#define ICU_ICR2_PINT0S		0x0001


#define ICU_IPR_MASK		0xf

/*   suspend/resume external Interrupt. 
 *  (don't block) use under privilege mode.
 */
__BEGIN_DECLS
u_int32_t suspendIntr(void);
void resumeIntr(u_int32_t);
__END_DECLS

/* 
 * SCIF (Windows CE's COM1)
 */
#define SCIF_SCSMR2_REG8		0xa4000150	/* R/W */
#define SCIF_SCBRR2_REG8		0xa4000152	/* R/W */
#define SCIF_SCSCR2_REG8		0xa4000154	/* R/W */
#define SCIF_SCFTDR2_REG8		0xa4000156	/* W */
#define SCIF_SCSSR2_REG16		0xa4000158	/* R/W(0 write only) */
#define SCIF_SCFRDR2_REG8		0xa400015a	/* R */
#define SCIF_SCFCR2_REG8		0xa400015c	/* R/W */
#define SCIF_SCFDR2_REG16		0xa400015e	/* R */

/* Transmit FIFO Data Empty */
#define SCIF_SCSSR2_TDFE		0x00000020
/* Transmit End */
#define SCIF_SCSSR2_TEND		0x00000040

/* simple serial console macros. */
#define TX_BUSY()							\
  while ((VOLATILE_REF16(SCIF_SCSSR2_REG16) & SCIF_SCSSR2_TDFE) == 0)

#define PUTC(c)								\
__BEGIN_MACRO								\
	TX_BUSY();							\
	/*  wait until previous transmit done. */			\
	VOLATILE_REF8(SCIF_SCFTDR2_REG8) =(c);				\
	/* Clear transmit FIFO empty flag */				\
	VOLATILE_REF16(SCIF_SCSSR2_REG16) &=				\
	~(SCIF_SCSSR2_TDFE | SCIF_SCSSR2_TEND);				\
__END_MACRO

#define PRINT(s)							\
__BEGIN_MACRO								\
	char *__s =(char *)(s);						\
	int __i;							\
	for (__i = 0; __s[__i] != '\0'; __i++) {			\
		char __c = __s[__i];					\
		if (__c == '\n')					\
			PUTC('\r');					\
		PUTC(__c);						\
	}								\
__END_MACRO

#define PRINT_HEX(h)							\
__BEGIN_MACRO								\
	u_int32_t __h =(u_int32_t)(h);					\
	int __i;							\
	PUTC('0'); PUTC('x');						\
	for (__i = 0; __i < 8; __i++, __h <<= 4) {			\
		int __n =(__h >> 28) & 0xf;				\
		char __c = __n > 9 ? 'A' + __n - 10 : '0' + __n;	\
		PUTC(__c);						\
	}								\
	PUTC('\r'); PUTC('\n');						\
__END_MACRO

/* 
 * Product dependent headers
 */
#include <sh3/sh_7707.h>
#include <sh3/sh_7709.h>
#include <sh3/sh_7709a.h>

/*
 * HD64461 (SH companion chip for Windows CE)
 */
#define	HD64461_FB_ADDR		0xb2000000

#endif // _HPCBOOT_SH3_H_
