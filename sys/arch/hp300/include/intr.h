/*	$NetBSD: intr.h,v 1.2.2.1 1997/10/14 08:55:55 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#ifndef _HP300_INTR_H_
#define	_HP300_INTR_H_

#include <machine/psl.h>

#ifdef _HP300_INTR_H_PRIVATE
#include <sys/queue.h>

/*
 * The location and size of the autovectored interrupt portion
 * of the vector table.
 */
#define ISRLOC		0x18
#define NISR		8

struct isr {
	LIST_ENTRY(isr) isr_link;
	int		(*isr_func) __P((void *));
	void		*isr_arg;
	int		isr_ipl;
	int		isr_priority;
};
#endif /* _HP300_INTR_H_PRIVATE */

/*
 * Interrupt "levels".  These are a more abstract representation
 * of interrupt levels, and do not have the same meaning as m68k
 * CPU interrupt levels.  They serve two purposes:
 *
 *	- properly order ISRs in the list for that CPU ipl
 *	- compute CPU PSL values for the spl*() calls.
 */
#define	IPL_NONE	0	/* disable only this interrupt */
#define	IPL_BIO		1	/* disable block I/O interrupts */
#define	IPL_NET		2	/* disable network interrupts */
#define	IPL_TTY		3	/* disable terminal interrupts */
#define	IPL_TTYNOBUF	4	/* IPL_TTY + higher ISR priority */
#define	IPL_CLOCK	5	/* disable clock interrupts */
#define	IPL_HIGH	6	/* disable all interrupts */

/*
 * Convert PSL values to CPU IPLs and vice-versa.
 */
#define	PSLTOIPL(x)	(((x) >> 8) & 0xf)
#define	IPLTOPSL(x)	((((x) & 0xf) << 8) | PSL_S)

#ifdef _KERNEL
/*
 * spl functions; all but spl0 are done in-line
 */

#define	_spl(s)								\
({									\
	register int _spl_r;						\
									\
	__asm __volatile ("clrl %0; movew sr,%0; movew %1,sr" :		\
	    "&=d" (_spl_r) : "di" (s));					\
	_spl_r;								\
})

#define	_splraise(s)							\
({									\
	int _spl_r;							\
									\
	__asm __volatile ("						\
		clrl	d0					;	\
		movw	sr,d0					;	\
		movl	d0,%0					;	\
		andw	#0x700,d0				;	\
		movw	%1,d1					;	\
		andw	#0x700,d1				;	\
		cmpw	d0,d1					;	\
		jle	1f					;	\
		movw	%1,sr					;	\
	    1:"							:	\
		    "&=d" (_spl_r)				:	\
		    "di" (s)					:	\
		    "d0", "d1");					\
	_spl_r;								\
})

/* spl0 requires checking for software interrupts */
#define	spl1()	_spl(PSL_S|PSL_IPL1)
#define	spl2()	_spl(PSL_S|PSL_IPL2)
#define	spl3()	_spl(PSL_S|PSL_IPL3)
#define	spl4()	_spl(PSL_S|PSL_IPL4)
#define	spl5()	_spl(PSL_S|PSL_IPL5)
#define	spl6()	_spl(PSL_S|PSL_IPL6)
#define	spl7()	_spl(PSL_S|PSL_IPL7)

/*
 * These four globals contain the appropriate PSL_S|PSL_IPL? values
 * to raise interupt priority to the requested level.
 */
extern	unsigned short hp300_bioipl;
extern	unsigned short hp300_netipl;
extern	unsigned short hp300_ttyipl;
extern	unsigned short hp300_impipl;

/* These spl calls are _not_ to be used by machine-independent code. */
#define	splhil()	_splraise(PSL_S|PSL_IPL1)
#define	splkbd()	splhil()
#define	splsoft()	spl1()

/* These spl calls are used by machine-independent code. */
#define	splsoftclock()	splsoft()
#define	splsoftnet()	splsoft()
#define	splbio()	_splraise(hp300_bioipl)
#define	splnet()	_splraise(hp300_netipl)
#define	spltty()	_splraise(hp300_ttyipl)
#define	splimp()	_splraise(hp300_impipl)
#define	splclock()	spl6()
#define	splstatclock()	spl6()
#define	splvm()		spl6()
#define	splhigh()	spl7()
#define	splsched()	spl7()

/* watch out for side effects */
#define	splx(s)		((s) & PSL_IPL ? _spl((s)) : spl0())

/*
 * Simulated software interrupt register.
 */
extern volatile u_int8_t ssir;

#define	SIR_NET		0x01
#define	SIR_CLOCK	0x02

#define	siron(mask)	\
	__asm __volatile ( "orb %0,_ssir" : : "i" ((mask)))
#define	siroff(mask)	\
	__asm __volatile ( "andb %0,_ssir" : : "ir" (~(mask)));

#define	setsoftnet()	siron(SIR_NET)
#define	setsoftclock()	siron(SIR_CLOCK)

/* locore.s */
int	spl0 __P((void));

/* intr.c */
void	intr_init __P((void));
void	*intr_establish __P((int (*)(void *), void *, int, int));
void	intr_disestablish __P((void *));
void	intr_dispatch __P((int));
void	intr_printlevels __P((void));
#endif /* _KERNEL */

#endif /* _HP300_INTR_H_ */
