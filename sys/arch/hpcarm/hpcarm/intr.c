/*	$NetBSD: intr.c,v 1.7.6.1 2004/08/03 10:35:04 skrll Exp $	*/

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Soft interrupt and other generic interrupt functions.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: intr.c,v 1.7.6.1 2004/08/03 10:35:04 skrll Exp $");

#include "opt_irqstats.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/cpu.h>

#include <net/netisr.h>

u_int soft_interrupts = 0;

extern int current_spl_level;
extern int softintr_dispatch(int);

/* Generate soft interrupt counts if IRQSTATS is defined */
#ifdef IRQSTATS
extern u_int sintrcnt[];
#define INC_SINTRCNT(x) ++sintrcnt[x]
#else
#define INC_SINTRCNT(x)
#endif	/* IRQSTATS */

#define	COUNT	uvmexp.softs;

/* Prototypes */

#include "com.h"
#if NCOM > 0
extern void comsoft	__P((void));
#endif	/* NCOM > 0 */

#include "sacom.h"
#if NSACOM > 0
extern void sacomsoft	__P((void));
#endif	/* NSACOM > 0 */

/* Eventually these will become macros */

void setsoftintr(u_int);
void clearsoftintr(u_int);
void dosoftints(void);

void
setsoftintr(intrmask)
	u_int intrmask;
{
	atomic_set_bit(&soft_interrupts, intrmask);
}

void
clearsoftintr(intrmask)
	u_int intrmask;
{
	atomic_clear_bit(&soft_interrupts, intrmask);
}

void
setsoftnet()
{
	atomic_set_bit(&soft_interrupts, SOFTIRQ_BIT(SOFTIRQ_NET));
}

int astpending;

/* Handle software interrupts */

void
dosoftints()
{
	u_int softints;
	int s;

	softintr_dispatch(current_spl_level);

	softints = soft_interrupts & spl_smasks[current_spl_level];
	if (softints == 0) return;

	/*
	 * Network software interrupts
	 */

	if (softints & SOFTIRQ_BIT(SOFTIRQ_NET)) {
		s = splsoftnet();
		++COUNT;
		INC_SINTRCNT(SOFTIRQ_NET);
		clearsoftintr(SOFTIRQ_BIT(SOFTIRQ_NET));

#define DONETISR(bit, fn) do {					\
		if (netisr & (1 << bit)) {			\
			atomic_clear_bit(&netisr, (1 << bit));	\
			fn();					\
		}						\
} while (0)

#include <net/netisr_dispatch.h>

#undef DONETISR

		(void)splx(s);
	}
}

/* End of intr.c */
