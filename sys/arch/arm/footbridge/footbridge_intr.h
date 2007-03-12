/* 	$NetBSD: footbridge_intr.h,v 1.9.2.1 2007/03/12 05:47:03 rmind Exp $	*/

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
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

#ifndef _FOOTBRIDGE_INTR_H_
#define _FOOTBRIDGE_INTR_H_

#include <arm/armreg.h>

/* Define the various Interrupt Priority Levels */

/* Hardware Interrupt Priority Levels are not mutually exclusive. */

#define IPL_NONE	0	/* nothing */
#define IPL_SOFT	1	/* generic soft interrupts */
#define IPL_SOFTCLOCK	2	/* clock software interrupts */
#define IPL_SOFTNET	3	/* network software interrupts */
#define IPL_BIO		4	/* block I/O */
#define IPL_NET		5	/* network */
#define IPL_SOFTSERIAL	6	/* serial software interrupts */
#define IPL_TTY		7	/* terminal */
#define	IPL_LPT		IPL_TTY
#define IPL_VM		8	/* memory allocation */
#define IPL_AUDIO	9	/* audio */
#define IPL_CLOCK	10	/* clock */
#define IPL_STATCLOCK	11	/* statclock */
#define IPL_HIGH	12	/* everything */
#define	IPL_SCHED	IPL_HIGH
#define	IPL_LOCK	IPL_HIGH
#define IPL_SERIAL	13	/* serial */

#define NIPL		14

#define	IST_UNUSABLE	-1	/* interrupt cannot be used */
#define	IST_NONE	0	/* none (dummy) */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#define	__NEWINTR	/* enables new hooks in cpu_fork()/cpu_switch() */

#define	ARM_IRQ_HANDLER	_C_LABEL(footbridge_intr_dispatch)

#ifndef _LOCORE
#include <arm/cpufunc.h>

#include <arm/footbridge/dc21285mem.h>
#include <arm/footbridge/dc21285reg.h>

#define INT_SWMASK							\
	((1U << IRQ_SOFTINT) | (1U << IRQ_RESERVED0) |			\
	 (1U << IRQ_RESERVED1) | (1U << IRQ_RESERVED2))
#define ICU_INT_HWMASK	(0xffffffff & ~(INT_SWMASK |  (1U << IRQ_RESERVED3)))

/* only call this with interrupts off */
static inline void __attribute__((__unused__))
    footbridge_set_intrmask(void)
{
    extern volatile uint32_t intr_enabled;
    /* fetch once so we write the same number to both registers */
    uint32_t tmp = intr_enabled & ICU_INT_HWMASK;

    ((volatile uint32_t*)(DC21285_ARMCSR_VBASE))[IRQ_ENABLE_SET>>2] = tmp;
    ((volatile uint32_t*)(DC21285_ARMCSR_VBASE))[IRQ_ENABLE_CLEAR>>2] = ~tmp;
}
    
static inline void __attribute__((__unused__))
footbridge_splx(int newspl)
{
	extern volatile uint32_t intr_enabled;
	extern volatile int current_spl_level;
	extern volatile int footbridge_ipending;
	extern void footbridge_do_pending(void);
	int oldirqstate, hwpend;

	/* Don't let the compiler re-order this code with preceding code */
	__insn_barrier();

	current_spl_level = newspl;

	hwpend = (footbridge_ipending & ICU_INT_HWMASK) & ~newspl;
	if (hwpend != 0) {
		oldirqstate = disable_interrupts(I32_bit);
		intr_enabled |= hwpend;
		footbridge_set_intrmask();
		restore_interrupts(oldirqstate);
	}

	if ((footbridge_ipending & INT_SWMASK) & ~newspl)
		footbridge_do_pending();
}

static inline int __attribute__((__unused__))
footbridge_splraise(int ipl)
{
	extern volatile int current_spl_level;
	extern int footbridge_imask[];
	int	old;

	old = current_spl_level;
	current_spl_level |= footbridge_imask[ipl];

	/* Don't let the compiler re-order this code with subsequent code */
	__insn_barrier();

	return (old);
}

static inline int __attribute__((__unused__))
footbridge_spllower(int ipl)
{
	extern volatile int current_spl_level;
	extern int footbridge_imask[];
	int old = current_spl_level;

	footbridge_splx(footbridge_imask[ipl]);
	return(old);
}

/* should only be defined in footbridge_intr.c */
#if !defined(ARM_SPL_NOINLINE)

#define splx(newspl)		footbridge_splx(newspl)
#define	_spllower(ipl)		footbridge_spllower(ipl)
#define	_splraise(ipl)		footbridge_splraise(ipl)
void	_setsoftintr(int);

#else

int	_splraise(int);
int	_spllower(int);
void	splx(int);
void	_setsoftintr(int);

#endif /* ! ARM_SPL_NOINLINE */

#include <sys/device.h>
#include <sys/queue.h>
#include <machine/irqhandler.h>

#define	splsoft()	_splraise(IPL_SOFT)

#define	spl0()		(void)_spllower(IPL_NONE)
#define	spllowersoftclock() (void)_spllower(IPL_SOFTCLOCK)

typedef uint8_t ipl_t;
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

#include <sys/spl.h>

/* Use generic software interrupt support. */
#include <arm/softintr.h>

/* footbridge has 32 interrupt lines */
#define	NIRQ		32

struct intrhand {
	TAILQ_ENTRY(intrhand) ih_list;	/* link on intrq list */
	int (*ih_func)(void *);		/* handler */
	void *ih_arg;			/* arg for handler */
	int ih_ipl;			/* IPL_* */
	int ih_irq;			/* IRQ number */
};

#define	IRQNAMESIZE	sizeof("footbridge irq 31")

struct intrq {
	TAILQ_HEAD(, intrhand) iq_list;	/* handler list */
	struct evcnt iq_ev;		/* event counter */
	int iq_mask;			/* IRQs to mask while handling */
	int iq_levels;			/* IPL_*'s this IRQ has */
	int iq_ist;			/* share type */
	char iq_name[IRQNAMESIZE];	/* interrupt name */
};

#endif /* _LOCORE */

#endif	/* _FOOTBRIDGE_INTR_H */
