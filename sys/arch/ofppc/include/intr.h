/*	$NetBSD: intr.h,v 1.1.6.1 2001/11/12 21:17:23 thorpej Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 1995-1997 Wolfgang Solfrank.
 * Copyright (C) 1995-1997 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACHINE_INTR_H_
#define	_MACHINE_INTR_H_

/* Interrupt priority "levels". */
#define	IPL_NONE	0	/* nothing */
#define	IPL_SOFT	1	/* generic software interrupts */
#define	IPL_SOFTCLOCK	2	/* software clock interrupt */
#define	IPL_SOFTNET	3	/* software network interrupt */
#define	IPL_BIO		4	/* block I/O */
#define	IPL_NET		5	/* network */
#define	IPL_SOFTSERIAL	6	/* software serial interrupt */
#define	IPL_TTY		7	/* terminals */
#define	IPL_IMP		8	/* memory allocation */
#define	IPL_AUDIO	9	/* audio device */
#define	IPL_CLOCK	10	/* clock interrupt */
#define	IPL_HIGH	11	/* everything */
#define	IPL_SERIAL	12	/* serial device */

#define	NIPL		13

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#ifdef _KERNEL
#ifndef _LOCORE

struct clockframe;

struct machvec {
	int (*mvec_splraise)(int);
	int (*mvec_spllower)(int);
	void (*mvec_splx)(int);
	void (*mvec_setsoft)(int);
	void (*mvec_clock_return)(struct clockframe *, int);
	void *(*mvec_intr_establish)(int, int, int, int (*)(void *), void *);
	void (*mvec_intr_disestablish)(void *);
};

extern struct machvec machine_interface;

#define	_splraise(x)	((*machine_interface.mvec_splraise)(x))
#define	_spllower(x)	((*machine_interface.mvec_spllower)(x))
#define	splx(x)		((*machine_interface.mvec_splx)(x))

#define	setsoftintr(x)	((*machine_interface.mvec_setsoft)(x))

#define	clock_return(x, y) ((*machine_interface.mvec_clock_return)(x, y))

#define	intr_establish(irq, lvl, ist, func, arg)			\
	((*machine_interface.mvec_intr_establish)((irq), (lvl), (ist),	\
	    (func), (arg)))
#define	intr_disestablish(cookie)					\
	((*machine_interface.mvec_intr_disestablish)((cookie)))

#define	splhigh()	_splraise(IPL_HIGH)
#define	splsoft()	_splraise(IPL_SOFT)
#define	splsoftclock()	_splraise(IPL_SOFTCLOCK)
#define	splsoftnet()	_splraise(IPL_SOFTNET)
#define	splbio()	_splraise(IPL_BIO)
#define	splnet()	_splraise(IPL_NET)
#define	spltty()	_splraise(IPL_TTY)
#define	splvm()		_splraise(IPL_IMP)
#define	splaudio()	_splraise(IPL_AUDIO)
#define	splclock()	_splraise(IPL_CLOCK)
#define	splserial()	_splraise(IPL_SERIAL)

#define	splstatclock()	splclock()

#define	spl0()		_spllower(IPL_NONE)
#define	spllowersoftclock() _spllower(IPL_SOFTCLOCK)

#define	splsched()	splhigh()
#define	spllock()	splhigh()

#define	setsoftnet()	setsoftintr(IPL_SOFTNET)
#define	setsoftclock()	setsoftintr(IPL_SOFTCLOCK)

/*
 * Software interrupt support.
 */

#define	SI_SOFT			0	/* for IPL_SOFT */
#define	SI_SOFTCLOCK		1	/* for IPL_SOFTCLOCK */
#define	SI_SOFTNET		2	/* for IPL_SOFTNET */
#define	SI_SOFTSERIAL		3	/* for IPL_SOFTSERIAL */

#define	SI_NQUEUES		4

#define	SI_QUEUENAMES {							\
	"generic",							\
	"clock",							\
	"net",								\
	"serial",							\
}

#endif /* _LOCORE */
#endif /* _KERNEL */

#endif	/* _MACHINE_INTR_H_ */
