/*	$NetBSD: isr.c,v 1.50.22.1 2006/09/19 10:12:37 yamt Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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

/*
 * This handles multiple attach of autovectored interrupts,
 * and the handy software interrupt request register.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isr.c,v 1.50.22.1 2006/09/19 10:12:37 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/vmmeter.h>

#include <uvm/uvm_extern.h>

#include <net/netisr.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/mon.h>

#include <sun3/sun3/machdep.h>
#include <sun3/sun3/vector.h>

extern int intrcnt[];	/* statistics */

#define NUM_LEVELS 8

struct isr {
	struct	isr *isr_next;
	isr_func_t isr_intr;
	void *isr_arg;
	int	isr_ipl;
};

void set_vector_entry(int, void *);
void * get_vector_entry(int);

/*
 * These are called from locore.  The "struct clockframe" arg
 * is really just the normal H/W interrupt frame format.
 * (kern_clock really wants it to be named that...)
 */
void	isr_autovec (struct clockframe);
void	isr_vectored(struct clockframe);


void 
isr_add_custom(int level, void *handler)
{
	set_vector_entry(AUTOVEC_BASE + level, handler);
}


/*
 * netisr junk...
 * should use an array of chars instead of
 * a bitmask to avoid atomicity locking issues.
 */

void 
netintr(void)
{
	int n, s;

	s = splhigh();
	n = netisr;
	netisr = 0;
	splx(s);

#define DONETISR(bit, fn) do {		\
	if (n & (1 << bit))		\
		fn();			\
} while (0)

#include <net/netisr_dispatch.h>

#undef DONETISR
}


static struct isr *isr_autovec_list[NUM_LEVELS];

/*
 * This is called by the assembly routines
 * for handling auto-vectored interrupts.
 */
void
isr_autovec(struct clockframe cf)
{
	struct isr *isr;
	int n, ipl, vec;

	vec = (cf.cf_vo & 0xFFF) >> 2;
	if ((vec < AUTOVEC_BASE) || (vec >= (AUTOVEC_BASE+8)))
		panic("isr_autovec: bad vec");
	ipl = vec - AUTOVEC_BASE;

	n = intrcnt[ipl];
	intrcnt[ipl] = n+1;
	uvmexp.intrs++;

	isr = isr_autovec_list[ipl];
	if (isr == NULL) {
		if (n == 0)
			printf("isr_autovec: ipl %d unexpected\n", ipl);
		return;
	}

	/* Give all the handlers a chance. */
	n = 0;
	while (isr) {
		n |= isr->isr_intr(isr->isr_arg);
		isr = isr->isr_next;
	}
	if (!n)
		printf("isr_autovec: ipl %d not claimed\n", ipl);
}

/*
 * Establish an interrupt handler.
 * Called by driver attach functions.
 */
void
isr_add_autovect(isr_func_t handler, void *arg, int level)
{
	struct isr *new_isr;

	if ((level < 0) || (level >= NUM_LEVELS))
		panic("isr_add: bad level=%d", level);
	new_isr = (struct isr *)
		malloc(sizeof(struct isr), M_DEVBUF, M_NOWAIT);
	if (!new_isr)
		panic("isr_add: malloc failed");

	new_isr->isr_intr = handler;
	new_isr->isr_arg = arg;
	new_isr->isr_ipl = level;
	new_isr->isr_next = isr_autovec_list[level];
	isr_autovec_list[level] = new_isr;
}

struct vector_handler {
	isr_func_t func;
	void *arg;
};
static struct vector_handler isr_vector_handlers[192];

/*
 * This is called by the assembly glue
 * for handling vectored interrupts.
 */
void
isr_vectored(struct clockframe cf)
{
	struct vector_handler *vh;
	int ipl, vec;

	vec = (cf.cf_vo & 0xFFF) >> 2;
	ipl = getsr();
	ipl = (ipl >> 8) & 7;

	intrcnt[ipl]++;
	uvmexp.intrs++;

	if (vec < 64 || vec >= 256) {
		printf("isr_vectored: vector=0x%x (invalid)\n", vec);
		return;
	}
	vh = &isr_vector_handlers[vec - 64];
	if (vh->func == NULL) {
		printf("isr_vectored: vector=0x%x (nul func)\n", vec);
		set_vector_entry(vec, (void *)badtrap);
		return;
	}

	/* OK, call the isr function. */
	if (vh->func(vh->arg) == 0)
		printf("isr_vectored: vector=0x%x (not claimed)\n", vec);
}

/*
 * Establish an interrupt handler.
 * Called by driver attach functions.
 */
extern void _isr_vectored(void);
void
isr_add_vectored(isr_func_t func, void *arg, int level, int vec)
{
	struct vector_handler *vh;

	if (vec < 64 || vec >= 256) {
		printf("isr_add_vectored: vect=0x%x (invalid)\n", vec);
		return;
	}
	vh = &isr_vector_handlers[vec - 64];
	if (vh->func) {
		printf("isr_add_vectored: vect=0x%x (in use)\n", vec);
		return;
	}
	vh->func = func;
	vh->arg = arg;
	set_vector_entry(vec, (void *)_isr_vectored);
}

/*
 * XXX - could just kill these...
 */
void 
set_vector_entry(int entry, void *handler)
{
	if ((entry <0) || (entry >= NVECTORS))
	panic("set_vector_entry: setting vector too high or low");
	vector_table[entry] = handler;
}

void *
get_vector_entry(int entry)
{
	if ((entry <0) || (entry >= NVECTORS))
	panic("get_vector_entry: setting vector too high or low");
	return ((void *) vector_table[entry]);
}

static const int ipl2psl_table[] = {
	[IPL_NONE] = PSL_IPL0,
#if 0
	[IPL_SOFTCLOCK] = PSL_IPL1,
	[IPL_SOFTNET] = PSL_IPL1,
#endif
	[IPL_BIO] = PSL_IPL2,
	[IPL_NET] = PSL_IPL3,
#if 0
	[IPL_SOFTSERIAL] = PSL_IPL3,
#endif
	[IPL_TTY] = PSL_IPL4,
	[IPL_LPT] = PSL_IPL4,
	[IPL_VM] = PSL_IPL4,
#if 0
	[IPL_AUDIO] =
#endif
	[IPL_CLOCK] = PSL_IPL5,
	[IPL_STATCLOCK] = PSL_IPL5,
	[IPL_SERIAL] = PSL_IPL6,
	[IPL_SCHED] = PSL_IPL7,
	[IPL_HIGH] = PSL_IPL7,
	[IPL_LOCK] = PSL_IPL7,
#if 0
	[IPL_IPI] =
#endif
};

ipl_cookie_t
makeiplcookie(ipl_t ipl)
{

	return (ipl_cookie_t){._psl = ipl2psl_table[ipl] | PSL_S};
}
