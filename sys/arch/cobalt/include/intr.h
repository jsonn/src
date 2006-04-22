/*	$NetBSD: intr.h,v 1.17.4.1 2006/04/22 11:37:21 simonb Exp $	*/

/*
 * Copyright (c) 2000 Soren S. Jorvang.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_COBALT_INTR_H_
#define	_COBALT_INTR_H_

#define	IPL_NONE	0	/* Disable only this interrupt. */
#define	IPL_BIO		1	/* Disable block I/O interrupts. */
#define	IPL_NET		2	/* Disable network interrupts. */
#define	IPL_TTY		3	/* Disable terminal interrupts. */
#define	IPL_SERIAL	3	/* Disable serial hardware interrupts. */
#define	IPL_VM		4	/* Memory allocation */
#define	IPL_CLOCK	5	/* Disable clock interrupts. */
#define	IPL_STATCLOCK	6	/* Disable profiling interrupts. */
#define	IPL_HIGH	7	/* Disable all interrupts. */
#define NIPL		8

/* Interrupt sharing types. */
#define IST_NONE	0	/* none */
#define IST_PULSE	1	/* pulsed */
#define IST_EDGE	2	/* edge-triggered */
#define IST_LEVEL	3	/* level-triggered */

/* Soft interrupt numbers. */
#define	IPL_SOFT	0	/* generic software interrupts */
#define	IPL_SOFTSERIAL	1	/* serial software interrupts */
#define	IPL_SOFTNET	2	/* network software interrupts */
#define	IPL_SOFTCLOCK	3	/* clock software interrupts */
#define	_IPL_NSOFT	4

#define	IPL_SOFTNAMES {							\
	"misc",								\
	"serial",							\
	"net",								\
	"clock",							\
}

#ifdef _KERNEL
#ifndef _LOCORE

#include <sys/device.h>
#include <mips/cpuregs.h>

int  _splraise(int);
int  _spllower(int);
int  _splset(int);
int  _splget(void);
void _splnone(void);
void _setsoftintr(int);
void _clrsoftintr(int);

#define splhigh()       _splraise(MIPS_INT_MASK)
#define spl0()          (void)_spllower(0)
#define splx(s)         (void)_splset(s)
#define SPLSOFT		(MIPS_SOFT_INT_MASK_0 | MIPS_SOFT_INT_MASK_1)
#define SPLBIO		(SPLSOFT | MIPS_INT_MASK_4)
#define SPLNET		(SPLBIO | MIPS_INT_MASK_1 | MIPS_INT_MASK_2)
#define SPLTTY		(SPLNET | MIPS_INT_MASK_3)
#define SPLCLOCK	(SPLTTY | MIPS_INT_MASK_0 | MIPS_INT_MASK_5)
#define splbio()	_splraise(SPLBIO)
#define splnet()	_splraise(SPLNET)
#define spltty()	_splraise(SPLTTY)
#define spllpt()	spltty()
#define splserial()	_splraise(SPLTTY)
#define splclock()	_splraise(SPLCLOCK)
#define splvm()		splclock()
#define splstatclock()	splclock()
#define spllowersoftclock() _spllower(MIPS_SOFT_INT_MASK_0)

#define	splsched()	splhigh()
#define	spllock()	splhigh()

#define splsoft()	_splraise(MIPS_SOFT_INT_MASK_0)
#define splsoftclock()	_splraise(MIPS_SOFT_INT_MASK_0)
#define splsoftnet()	_splraise(MIPS_SOFT_INT_MASK_0|MIPS_SOFT_INT_MASK_1)
#define splsoftserial()	_splraise(MIPS_SOFT_INT_MASK_0|MIPS_SOFT_INT_MASK_1)

struct cobalt_intrhand {
	LIST_ENTRY(cobalt_intrhand) ih_q;
	int (*ih_func)(void *);
	void *ih_arg;
	int ih_cookie_type;
#define	COBALT_COOKIE_TYPE_CPU	0x1
#define	COBALT_COOKIE_TYPE_ICU	0x2

	struct evcnt ih_evcnt;
	char ih_evname[32];
};

#include <mips/softintr.h>

void *cpu_intr_establish(int, int, int (*)(void *), void *);
void *icu_intr_establish(int, int, int, int (*)(void *), void *);
void cpu_intr_disestablish(void *);
void icu_intr_disestablish(void *);

#endif /* !_LOCORE */
#endif /* _LOCORE */

#endif	/* !_COBALT_INTR_H_ */
