/*	$NetBSD: amd7930intr.s,v 1.17.12.3 2002/12/11 06:12:10 thorpej Exp $	*/
/*
 * Copyright (c) 1995 Rolf Grossmann.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)bsd_audiointr.s	8.1 (Berkeley) 6/11/93
 */

#ifndef AUDIO_C_HANDLER
#include "assym.h"
#include <machine/param.h>
#include <machine/asm.h>
#include <sparc/sparc/intreg.h>
#include <machine/intr.h>
#include <machine/psl.h>
#include <dev/ic/am7930reg.h>

#define AUDIO_SET_SWINTR_4C				\
	sethi	%hi(INTRREG_VA), %l5;			\
	ldub	[%l5 + %lo(INTRREG_VA)], %l6;		\
	or	%l6, IE_L4, %l6;			\
	stb	%l6, [%l5 + %lo(INTRREG_VA)]

! raise(0,IPL_SOFTAUDIO)	! NOTE: CPU#0
#define AUDIO_SET_SWINTR_4M				\
	sethi	%hi(PINTR_SINTRLEV(IPL_SOFTAUDIO)), %l5;\
	set	ICR_PI_SET, %l6;			\
	st	%l5, [%l6]

/* set software interrupt */
#if (defined(SUN4) || defined(SUN4C)) && !defined(SUN4M)
#define AUDIO_SET_SWINTR	AUDIO_SET_SWINTR_4C
#elif !(defined(SUN4) || defined(SUN4C)) && defined(SUN4M)
#define AUDIO_SET_SWINTR	AUDIO_SET_SWINTR_4M
#else
#define AUDIO_SET_SWINTR				\
	sethi	%hi(_C_LABEL(cputyp)), %l5;		\
	ld	[%l5 + %lo(_C_LABEL(cputyp))], %l5;	\
	cmp	%l5, CPU_SUN4M;				\
	be	8f;					\
	AUDIO_SET_SWINTR_4C;				\
	ba,a	9f;					\
8:							\
	AUDIO_SET_SWINTR_4M;				\
9:
#endif

#define R_amd	%l2
#define R_data	%l3
#define R_end	%l4

	.seg	"data"
	.align	8
savepc:
	.word	0

	.seg	"text"
	.align	4

_ENTRY(_C_LABEL(amd7930_trap))
	sethi	%hi(savepc), %l7
	st	%l2, [%l7 + %lo(savepc)]

	! tally interrupt (uvmexp.intrs++)
	sethi	%hi(_C_LABEL(uvmexp)+V_INTR), %l7
	ld	[%l7 + %lo(_C_LABEL(uvmexp)+V_INTR)], %l6
	inc	%l6
	st	%l6, [%l7 + %lo(_C_LABEL(uvmexp)+V_INTR)]

	sethi	%hi(_C_LABEL(auiop)), %l7
	ld	[%l7 + %lo(_C_LABEL(auiop))], %l7

	! tally interrupt (au_intrcnt.ev_count++)
	ldd	[%l7 + AU_EVCNT], %l4
	inccc	%l5
	addx	%l4, 0, %l4
	std	%l4, [%l7 + AU_EVCNT]

	ld	[%l7 + AU_BH], R_amd
	ldub    [R_amd + AM7930_DREG_IR], %g0	! clear interrupt

	! receive incoming data
	ld	[%l7 + AU_RDATA], R_data
	ld	[%l7 + AU_REND], R_end

	cmp	R_data, 0			! if (d && d <= e)
	be	1f
	cmp	R_data, R_end
	bgu	1f
	 nop

	ldub	[R_amd + AM7930_DREG_BBRB], %l6	! *d = amd->bbrb
	stb	%l6, [R_data]

	cmp	R_data, R_end
	inc	R_data				! au->au_rdata++
	bne	1f				! if (d == e)
	 st	R_data, [%l7 + AU_RDATA]

	AUDIO_SET_SWINTR

1:
	! write outgoing data
	ld	[%l7 + AU_PDATA], R_data
	ld	[%l7 + AU_PEND], R_end

	cmp	R_data, 0			! if (d && d <= e)
	be	2f
	cmp	R_data, R_end
	bgu	2f
	 nop

	ldub	[R_data], %l6			! amd->bbtb = *d
	stb	%l6, [R_amd + AM7930_DREG_BBTB]

	cmp	R_data, R_end
	inc	R_data				! au->au_pdata++
	bne	2f				! if (d == e)
	 st	R_data, [%l7 + AU_PDATA]

	AUDIO_SET_SWINTR

2:
	/*
	 * Restore psr -- note: psr delay honored by pc restore loads.
	 */
	mov	%l0, %psr
	sethi	%hi(savepc), %l7
	ld	[%l7 + %lo(savepc)], %l2
	jmp	%l1
	rett	%l2
#endif /* !AUDIO_C_HANDLER */
