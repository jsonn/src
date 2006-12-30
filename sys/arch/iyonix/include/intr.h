/*	$NetBSD: intr.h,v 1.1.14.2 2006/12/30 20:46:21 yamt Exp $	*/

/*
 * Copyright (c) 2001, 2003 Wasabi Systems, Inc.
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

#ifndef	_IYONIX_INTR_H_
#define	_IYONIX_INTR_H_

#ifdef _KERNEL

/* Interrupt priority "levels". */
#define	IPL_NONE	0	/* nothing */
#define	IPL_SOFT	1	/* generic software interrupts */
#define	IPL_SOFTCLOCK	2	/* software clock interrupt */
#define	IPL_SOFTNET	3	/* software network interrupt */
#define	IPL_BIO		4	/* block I/O */
#define	IPL_NET		5	/* network */
#define	IPL_SOFTSERIAL	6	/* software serial interrupt */
#define	IPL_TTY		7	/* terminals */
#define	IPL_VM		8	/* memory allocation */
#define	IPL_AUDIO	9	/* audio device */
#define	IPL_CLOCK	10	/* clock interrupt */
#define	IPL_STATCLOCK	11	/* statistics clock interrupt */
#define	IPL_HIGH	12	/* everything */
#define	IPL_SCHED	IPL_HIGH
#define	IPL_LOCK	IPL_HIGH
#define	IPL_SERIAL	13	/* serial device */

#define	NIPL		14

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#define IST_LEVEL_LOW	 IST_LEVEL
#define IST_LEVEL_HIGH   4
#define IST_EDGE_FALLING IST_EDGE
#define IST_EDGE_RISING  5
#define IST_EDGE_BOTH    6

#define	__NEWINTR	/* enables new hooks in cpu_fork()/cpu_switch() */

#ifndef _LOCORE

#include <sys/device.h>
#include <sys/queue.h>

#if defined(_LKM)

int	_splraise(int);
int	_spllower(int);
void	splx(int);
void	_setsoftintr(int);

#else	/* _LKM */

#include "opt_arm_intr_impl.h"

#if defined(ARM_INTR_IMPL)

/*
 * Each board needs to define the following functions:
 *
 * int	_splraise(int);
 * int	_spllower(int);
 * void	splx(int);
 * void	_setsoftintr(int);
 *
 * These may be defined as functions, static inline functions, or macros,
 * but there must be a _spllower() and splx() defined as functions callable
 * from assembly language (for cpu_switch()).  However, since it's quite
 * useful to be able to inline splx(), you could do something like the
 * following:
 *
 * in <boardtype>_intr.h:
 * 	static inline int
 *	boardtype_splx(int spl)
 *	{...}
 *
 *	#define splx(nspl)	boardtype_splx(nspl)
 *	...
 * and in boardtype's machdep code:
 *
 *	...
 *	#undef splx
 *	int
 *	splx(int spl)
 *	{
 *		return boardtype_splx(spl);
 *	}
 */

#include ARM_INTR_IMPL

#else /* ARM_INTR_IMPL */

#error ARM_INTR_IMPL not defined.

#endif	/* ARM_INTR_IMPL */

#endif /* _LKM */

#define	splsoft()	_splraise(IPL_SOFT)

typedef int ipl_t;
typedef struct {
	ipl_t _ipl;
} ipl_cookie_t;

static inline ipl_cookie_t
makeiplcookie(ipl_t ipl)
{

	return (ipl_cookie_t){._ipl = ipl};
}

static inline int
splraiseipl(ipl_cookie_t icookie)
{

	return _splraise(icookie._ipl);
}

#define	spl0()		_spllower(IPL_NONE)
#define	spllowersoftclock() _spllower(IPL_SOFTCLOCK)

#include <sys/spl.h>

/* Use generic software interrupt support. */
#include <arm/softintr.h>

#endif /* ! _LOCORE */

#endif /* _KERNEL */

#endif	/* _IYONIX_INTR_H_ */
