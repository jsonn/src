/*	$NetBSD: psl.h,v 1.18.14.1 1997/11/14 01:33:21 mellon Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)psl.h	5.2 (Berkeley) 1/18/91
 */

#ifndef _NS532_PSL_H_
#define _NS532_PSL_H_

/*
 * 32532 processor status longword.
 */
#define	PSL_C		0x00000001	/* carry bit */
#define	PSL_T		0x00000002	/* trace enable bit */
#define	PSL_L		0x00000004	/* less bit */
#define	PSL_V		0x00000010	/* overflow bit */
#define	PSL_F		0x00000020	/* flag bit */
#define	PSL_Z		0x00000040	/* zero bit */
#define	PSL_N		0x00000080	/* negative bit */

#define PSL_USER	0x00000100	/* User mode bit */
#define PSL_US		0x00000200	/* User stack mode bit */
#define PSL_P		0x00000400	/* Prevent TRC trap */
#define	PSL_I		0x00000800	/* interrupt enable bit */

#define	PSL_USERSET	(PSL_USER | PSL_US | PSL_I)
#define	PSL_USERSTATIC	(PSL_USER | PSL_US | PSL_I)
#define	USERMODE(psr)	(((psr) & PSL_USER) == PSL_USER)

/* The PSR versions ... */
#define PSR_USR PSL_USER

#ifdef _KERNEL
#include <machine/icu.h>
/*
 * Interrupt levels
 */
#define IPL_ZERO	0	/* level 0 */
#define IPL_HIGH	1	/* block all interrupts */
#define	IPL_BIO		2	/* block I/O */
#define	IPL_NET		3	/* network */
#define	IPL_TTY		4	/* terminal */
#define	IPL_CLOCK	5	/* clock */
#define IPL_IMP		6	/* memory allocation */
#define	NIPL		7	/* number of interrupt priority levels */
#define IPL_NAMES	{"zero", "", "bio", "net", "tty", "clock", "imp"}

/* IPL_RTTY (for the scn driver) is the same as IPL_HIGH. */
#define	IPL_RTTY	IPL_HIGH

/*
 * Preassigned software interrupts
 */
#define SOFTINT		16
#define	SIR_CLOCK	(SOFTINT+0)
#define	SIR_CLOCKMASK	((1 << SIR_CLOCK) | (1 << IR_SOFT))
#define	SIR_NET		(SOFTINT+1)
#define	SIR_NETMASK	((1 << SIR_NET) | SIR_CLOCKMASK)
#define SIR_ALLMASK	0xffff0000

#ifndef _LOCORE
/*
 * Structure of the software interrupt table
 */
struct iv {
	void (*iv_vec) __P((void *));
	void *iv_arg;
	long iv_level;
	long iv_mask;
	long iv_cnt;
	char *iv_use;
};

extern struct iv ivt[];
extern unsigned int imask[], Cur_pl, sirpending, astpending;

#if defined(NO_INLINE_SPLX) || defined(DEFINE_SPLX)
# define PSL_STATIC
#else
# define PSL_STATIC static
#endif
#if defined(NO_INLINE_SPLX)
# define PSL_INLINE
#else
# define PSL_INLINE __inline
#endif

void	intr_init __P((void));
void	check_sir __P((void *));
int	intr_establish __P((int, void (*)(void *), void *, char *,
				int, int, int));

PSL_STATIC PSL_INLINE int splraise __P((unsigned int));
PSL_STATIC PSL_INLINE int splx __P((unsigned int));

/*
 * Disable/Enable CPU-Interrupts
 */
#define di() /* Removing the nop will give you *BIG* trouble */ \
	__asm __volatile("bicpsrw 0x800 ; nop" : : : "cc")
#define ei() __asm __volatile("bispsrw 0x800" : : : "cc")

/*
 * Add a mask to Cur_pl, and return the old value of Cur_pl.
 */
#if !defined(NO_INLINE_SPLX) || defined(DEFINE_SPLX)
PSL_STATIC PSL_INLINE int
splraise(ncpl)
	register unsigned int ncpl;
{
	register unsigned int ocpl;
	di();
	ocpl = Cur_pl;
	ncpl |= ocpl;
	ICUW(IMSK) = ncpl;
	Cur_pl = ncpl;
	ei();
	return(ocpl);
}

/*
 * Restore a value to Cur_pl (cpu interrupts will get unmasked).
 *
 * NOTE: We go to the trouble of returning the old value of cpl for
 * the benefit of some splsoftclock() callers.  This extra work is
 * usually optimized away by the compiler.
 */
PSL_STATIC PSL_INLINE int
splx(ncpl)
	register unsigned int ncpl;
{
	register unsigned int ocpl;
	di();
	ocpl = Cur_pl;
	ICUW(IMSK) = ncpl;
	Cur_pl = ncpl;
	ei();
	return(ocpl);
}
#endif

/*
 * Hardware interrupt masks
 */
#define splbio()	splraise(imask[IPL_BIO])
#define splnet()	splraise(imask[IPL_NET])
#define spltty()	splraise(imask[IPL_TTY])
#define splclock()	splraise(imask[IPL_CLOCK])
#define splimp()	splraise(imask[IPL_IMP])
#define splrtty()	splraise(imask[IPL_RTTY])
#define	splstatclock()	splclock()

/*
 * Software interrupt masks
 *
 * NOTE: splsoftclock() is used by hardclock() to lower the priority from
 * clock to softclock before it calls softclock().
 */
#define	splsoftclock()	splx(SIR_CLOCKMASK | imask[IPL_ZERO])
#define	splsoftnet()	splraise(SIR_NETMASK)

/*
 * Miscellaneous
 */
#define	splhigh()	splraise(-1)
#define	spl0()		splx(imask[IPL_ZERO])
#define splnone()	spl0()

/*
 * Software interrupt registration
 */
#define	softintr(n)	((sirpending |= (1 << (n))), setsofticu(IR_SOFT))
#define	setsoftast()	(astpending = 1)
#define	setsoftclock()	softintr(SIR_CLOCK)
#define	setsoftnet()	softintr(SIR_NET)

#undef PSL_INLINE
#undef PSL_STATIC

#endif /* !_LOCORE */
#endif /* _KERNEL */

#endif /* _NS532_PSL_H_ */
