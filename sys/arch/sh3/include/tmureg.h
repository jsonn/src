/*	$NetBSD: tmureg.h,v 1.7.6.2 2002/04/28 17:10:38 uch Exp $	*/

/*-
 * Copyright (C) 1999 SAITOH Masanobu.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SH3_TMUREG_H_
#define	_SH3_TMUREG_H_
#include <sh3/devreg.h>

/*
 * TMU
 */
#define	SH3_TOCR			0xfffffe90
#define	SH3_TSTR			0xfffffe92
#define	SH3_TCOR0			0xfffffe94
#define	SH3_TCNT0			0xfffffe98
#define	SH3_TCR0			0xfffffe9c
#define	SH3_TCOR1			0xfffffea0
#define	SH3_TCNT1			0xfffffea4
#define	SH3_TCR1			0xfffffea8
#define	SH3_TCOR2			0xfffffeac
#define	SH3_TCNT2			0xfffffeb0
#define	SH3_TCR2			0xfffffeb4
#define	SH3_TCPR2			0xfffffeb8

#define	SH4_TOCR			0xffd80000
#define	SH4_TSTR			0xffd80004
#define	SH4_TCOR0			0xffd80008
#define	SH4_TCNT0			0xffd8000c
#define	SH4_TCR0			0xffd80010
#define	SH4_TCOR1			0xffd80014
#define	SH4_TCNT1			0xffd80018
#define	SH4_TCR1			0xffd8001c
#define	SH4_TCOR2			0xffd80020
#define	SH4_TCNT2			0xffd80024
#define	SH4_TCR2			0xffd80028
#define	SH4_TCPR2			0xffd8002c

#define	TOCR_TCOE			  0x01
#define	TSTR_STR2			  0x04
#define	TSTR_STR1			  0x02
#define	TSTR_STR0			  0x01
#define	TCR_ICPF			  0x0200
#define	TCR_UNF				  0x0100
#define	TCR_ICPE1			  0x0080
#define	TCR_ICPE0			  0x0040
#define	TCR_UNIE			  0x0020
#define	TCR_CKEG1			  0x0010
#define	TCR_CKEG0			  0x0008
#define	TCR_TPSC2			  0x0004
#define	TCR_TPSC1			  0x0002
#define	TCR_TPSC0			  0x0001
#define	TCR_TPSC_P4			  0x0000
#define	TCR_TPSC_P16			  0x0001
#define	TCR_TPSC_P64			  0x0002
#define	TCR_TPSC_P256			  0x0003
#define	SH3_TCR_TPSC_RTC		  0x0004
#define	SH3_TCR_TPSC_TCLK		  0x0005
#define	SH4_TCR_TPSC_P512		  0x0004
#define	SH4_TCR_TPSC_RTC		  0x0006
#define	SH4_TCR_TPSC_TCLK		  0x0007

#ifndef _LOCORE
#if defined(SH3) && defined(SH4)
extern u_int32_t __sh_TOCR;
extern u_int32_t __sh_TSTR;
extern u_int32_t __sh_TCOR0;
extern u_int32_t __sh_TCNT0;
extern u_int32_t __sh_TCR0;
extern u_int32_t __sh_TCOR1;
extern u_int32_t __sh_TCNT1;
extern u_int32_t __sh_TCR1;
extern u_int32_t __sh_TCOR2;
extern u_int32_t __sh_TCNT2;
extern u_int32_t __sh_TCR2;
extern u_int32_t __sh_TCPR2;
#endif /* SH3 && SH4 */
#endif /* !_LOCORE */

#endif	/* !_SH3_TMUREG_H_ */
