/*	$NetBSD: intr.h,v 1.5.8.1 1997/03/12 14:34:46 is Exp $	*/

/*
 * Copyright (c) 1996, 1997 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _I386_INTR_H_
#define _I386_INTR_H_

/* Interrupt priority `levels'. */
#define	IPL_NONE	8	/* nothing */
#define	IPL_SOFTCLOCK	7	/* timeouts */
#define	IPL_SOFTNET	6	/* protocol stacks */
#define	IPL_BIO		5	/* block I/O */
#define	IPL_NET		4	/* network */
#define	IPL_SOFTSERIAL	3	/* serial */
#define	IPL_TTY		2	/* terminal */
#define	IPL_IMP		2	/* memory allocation */
#define	IPL_CLOCK	1	/* clock */
#define	IPL_HIGH	1	/* everything */
#define	IPL_SERIAL	0	/* serial */
#define	NIPL		9

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

/* Soft interrupt masks. */
#define	SIR_CLOCK	31
#define	SIR_NET		30
#define	SIR_SERIAL	29

#ifndef _LOCORE

volatile int cpl, ipending, astpending;
int imask[NIPL];

extern void Xspllower __P((void));

static __inline int splraise __P((int));
static __inline int spllower __P((int));
static __inline void splx __P((int));
static __inline void softintr __P((int));

/*
 * Add a mask to cpl, and return the old value of cpl.
 */
static __inline int
splraise(ncpl)
	register int ncpl;
{
	register int ocpl = cpl;

	cpl = ocpl | ncpl;
	return (ocpl);
}

/*
 * Restore a value to cpl (unmasking interrupts).  If any unmasked
 * interrupts are pending, call Xspllower() to process them.
 */
static __inline void
splx(ncpl)
	register int ncpl;
{

	cpl = ncpl;
	if (ipending & ~ncpl)
		Xspllower();
}

/*
 * Same as splx(), but we return the old value of spl, for the
 * benefit of some splsoftclock() callers.
 */
static __inline int
spllower(ncpl)
	register int ncpl;
{
	register int ocpl = cpl;

	cpl = ncpl;
	if (ipending & ~ncpl)
		Xspllower();
	return (ocpl);
}

/*
 * Hardware interrupt masks
 */
#define	splbio()	splraise(imask[IPL_BIO])
#define	splnet()	splraise(imask[IPL_NET])
#define	spltty()	splraise(imask[IPL_TTY])
#define	splclock()	splraise(imask[IPL_CLOCK])
#define	splimp()	splraise(imask[IPL_IMP])
#define	splserial()	splraise(imask[IPL_SERIAL])
#define	splstatclock()	splclock()

/*
 * Software interrupt masks
 *
 * NOTE: splsoftclock() is used by hardclock() to lower the priority from
 * clock to softclock before it calls softclock().
 */
#define	splsoftclock()	spllower(imask[IPL_SOFTCLOCK])
#define	splsoftnet()	splraise(imask[IPL_SOFTNET])
#define	splsoftserial()	splraise(imask[IPL_SOFTSERIAL])

/*
 * Miscellaneous
 */
#define	splhigh()	splraise(imask[IPL_HIGH])
#define	spl0()		spllower(0)

/*
 * Software interrupt registration
 *
 * We hand-code this to ensure that it's atomic.
 */
static __inline void
softintr(mask)
	register int mask;
{

	__asm __volatile("orl %0,_ipending" : : "ir" (1 << mask));
}

#define	setsoftast()	(astpending = 1)
#define	setsoftclock()	softintr(SIR_CLOCK)
#define	setsoftnet()	softintr(SIR_NET)
#define	setsoftserial()	softintr(SIR_SERIAL)

#endif /* !_LOCORE */

#endif /* !_I386_INTR_H_ */
