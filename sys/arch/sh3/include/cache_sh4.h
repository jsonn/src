/*	$NetBSD: cache_sh4.h,v 1.3.8.3 2002/06/23 17:40:37 jdolecek Exp $	*/

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

/*
 * SH4: SH7750 SH7750S
 */

#ifndef _SH3_CACHE_SH4_H_
#define	_SH3_CACHE_SH4_H_
#include <sh3/devreg.h>
#ifdef _KERNEL

#define	SH4_ICACHE_SIZE		8192
#define	SH4_DCACHE_SIZE		16384
#define	SH4_CACHE_LINESZ	32

#define	SH4_CCR			0xff00001c
#define	  SH4_CCR_IIX		  0x00008000
#define	  SH4_CCR_ICI		  0x00000800
#define	  SH4_CCR_ICE		  0x00000100
#define	  SH4_CCR_OIX		  0x00000080
#define	  SH4_CCR_ORA		  0x00000020
#define	  SH4_CCR_OCI		  0x00000008
#define	  SH4_CCR_CB		  0x00000004
#define	  SH4_CCR_WT		  0x00000002
#define	  SH4_CCR_OCE		  0x00000001

#define	SH4_QACR0		0xff000038
#define	SH4_QACR1		0xff00003c
#define	  SH4_QACR_AREA_SHIFT	  2
#define	  SH4_QACR_AREA_MASK	  0x0000001c

/* I-cache address/data array  */
#define	SH4_CCIA		0xf0000000
/* address specification */
#define	  CCIA_A		  0x00000008	/* associate bit */
#define	  CCIA_ENTRY_SHIFT	  5		/* line size 32B */
#define	  CCIA_ENTRY_MASK	  0x00001fe0	/* [12:5] 256-entries */
/* data specification */
#define	  CCIA_V		  0x00000001
#define	  CCIA_TAGADDR_MASK	  0xfffffc00	/* [31:10] */

#define	SH4_CCID		0xf1000000
/* address specification */
#define	  CCID_L_SHIFT		  2
#define	  CCID_L_MASK		  0x1c		/* line-size is 32B */
#define	  CCID_ENTRY_MASK	  0x00001fe0	/* [12:5] 128-entries */

/* D-cache address/data array  */
#define	SH4_CCDA		0xf4000000
/* address specification */
#define	  CCDA_A		  0x00000008	/* associate bit */
#define	  CCDA_ENTRY_SHIFT	  5		/* line size 32B */
#define	  CCDA_ENTRY_MASK	  0x00003fe0	/* [13:5] 512-entries */
/* data specification */
#define	  CCDA_V		  0x00000001
#define	  CCDA_U		  0x00000002
#define	  CCDA_TAGADDR_MASK	  0xfffffc00	/* [31:10] */

#define	SH4_CCDD		0xf5000000

/* Store Queue */
#define	SH4_SQ			0xe0000000

/*
 * cache flush macro for locore level code.
 */
#define	SH4_CACHE_FLUSH()						\
do {									\
	u_int32_t __e, __a;						\
									\
	/* D-cache */							\
	for (__e = 0; __e < (SH4_DCACHE_SIZE / SH4_CACHE_LINESZ); __e++) {\
		__a = SH4_CCDA | (__e << CCDA_ENTRY_SHIFT);		\
		(*(__volatile__ u_int32_t *)__a) &= ~(CCDA_U | CCDA_V);	\
	}								\
	/* I-cache */							\
	for (__e = 0; __e < (SH4_ICACHE_SIZE / SH4_CACHE_LINESZ); __e++) {\
		__a = SH4_CCIA | (__e << CCIA_ENTRY_SHIFT);		\
		(*(__volatile__ u_int32_t *)__a) &= ~(CCIA_V);		\
	}								\
} while(/*CONSTCOND*/0)

#define	SH7750_CACHE_FLUSH()		SH4_CACHE_FLUSH()
#define	SH7750S_CACHE_FLUSH()		SH4_CACHE_FLUSH()

#ifndef _LOCORE
extern void sh4_cache_config(void);
#endif
#endif /* _KERNEL */
#endif /* !_SH3_CACHE_SH4_H_ */
