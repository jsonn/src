/*	$NetBSD: intr.h,v 1.1.4.3 2002/08/31 13:44:42 gehenna Exp $	*/

/*-
 * Copyright (c) 1998, 2001, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, and by Jason R. Thorpe, and by Matthew Fredette.
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

#ifndef _HP700_INTR_H_
#define _HP700_INTR_H_

#include <machine/psl.h>

/* Interrupt priority `levels'. */
#define	IPL_NONE	9	/* nothing */
#define	IPL_SOFTCLOCK	8	/* timeouts */
#define	IPL_SOFTNET	7	/* protocol stacks */
#define	IPL_BIO		6	/* block I/O */
#define	IPL_NET		5	/* network */
#define	IPL_SOFTSERIAL	4	/* serial */
#define	IPL_TTY		3	/* terminal */
#define	IPL_IMP		3	/* memory allocation */
#define	IPL_AUDIO	2	/* audio */
#define	IPL_CLOCK	1	/* clock */
#define	IPL_HIGH	1	/* everything */
#define	IPL_SERIAL	0	/* serial */
#define	NIPL		10

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#ifndef _LOCORE

/* The priority level masks. */
extern int imask[NIPL];

/* The current priority level. */
extern volatile int cpl;

/* The asynchronous system trap flag. */
extern volatile int astpending;

/* The softnet mask. */
extern int softnetmask;

/* 
 * Add a mask to cpl, and return the old value of cpl.
 */
static __inline int splraise __P((int)); 
static __inline int  
splraise(ncpl)
	register int ncpl;
{
	register int ocpl = cpl;

	cpl = ocpl | ncpl;      
	return (ocpl);  
}

/* spllower() is in locore.S */
void spllower __P((int));
 
/*
 * Hardware interrupt masks
 */
#define	splbio()	splraise(imask[IPL_BIO])
#define	splnet()	splraise(imask[IPL_NET])
#define	spltty()	splraise(imask[IPL_TTY])
#define	splaudio()	splraise(imask[IPL_AUDIO])
#define	splclock()	splraise(imask[IPL_CLOCK])
#define	splstatclock()	splclock()
#define	splserial()	splraise(imask[IPL_SERIAL])

#define spllpt()	spltty()

/*
 * Software interrupt masks
 *
 * NOTE: splsoftclock() is used by hardclock() to lower the priority from
 * clock to softclock before it calls softclock().
 */
#define	spllowersoftclock() spllower(imask[IPL_SOFTCLOCK])
#define	splsoftclock()	splraise(imask[IPL_SOFTCLOCK])
#define	splsoftnet()	splraise(imask[IPL_SOFTNET])
#define	splsoftserial()	splraise(imask[IPL_SOFTSERIAL])

/*
 * Miscellaneous
 */
#define	splvm()		splraise(imask[IPL_IMP])
#define	splhigh()	splraise(imask[IPL_HIGH])
#define	splsched()	splhigh()
#define	spllock()	splhigh()
#define	spl0()		spllower(0)
#define	splx(x)		spllower(x)

#define	setsoftast()	(astpending = 1)
#define	setsoftnet()	hp700_intr_schedule(softnetmask)

#endif /* !_LOCORE */

/*
 * Generic software interrupt support.
 */

#define	HP700_SOFTINTR_SOFTCLOCK	0
#define	HP700_SOFTINTR_SOFTNET		1
#define	HP700_SOFTINTR_SOFTSERIAL	2
#define	HP700_NSOFTINTR			3

#ifndef _LOCORE
#include <sys/queue.h>
#include <sys/device.h>

struct hp700_soft_intrhand {
	TAILQ_ENTRY(hp700_soft_intrhand)
		sih_q;
	struct hp700_soft_intr *sih_intrhead;
	void	(*sih_fn)(void *);
	void	*sih_arg;
	int	sih_pending;
};

struct hp700_soft_intr {
	TAILQ_HEAD(, hp700_soft_intrhand)
		softintr_q;
	int softintr_ssir;
};

#define	hp700_softintr_lock(si, s)					\
do {									\
	(s) = splhigh();						\
} while (/*CONSTCOND*/ 0)

#define	hp700_softintr_unlock(si, s)					\
do {									\
	splx((s));							\
} while (/*CONSTCOND*/ 0)

void	*softintr_establish __P((int, void (*)(void *), void *));
void	softintr_disestablish __P((void *));
void	softintr_bootstrap __P((void));
void	softintr_init __P((void));
int	softintr_dispatch __P((void *));

#define	softintr_schedule(arg)						\
do {									\
	struct hp700_soft_intrhand *__sih = (arg);			\
	struct hp700_soft_intr *__si = __sih->sih_intrhead;		\
	int __s;							\
									\
	hp700_softintr_lock(__si, __s);					\
	if (__sih->sih_pending == 0) {					\
		TAILQ_INSERT_TAIL(&__si->softintr_q, __sih, sih_q);	\
		__sih->sih_pending = 1;					\
		hp700_intr_schedule(__si->softintr_ssir);		\
	}								\
	hp700_softintr_unlock(__si, __s);				\
} while (/*CONSTCOND*/ 0)

void	hp700_intr_schedule __P((int));

#endif /* _LOCORE */

#endif /* !_HP700_INTR_H_ */
