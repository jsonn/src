/*	$NetBSD: interrupt.c,v 1.5.24.1 2004/08/03 10:31:02 skrll Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: interrupt.c,v 1.5.24.1 2004/08/03 10:31:02 skrll Exp $");

#include "opt_algor_p4032.h"
#include "opt_algor_p5064.h" 
#include "opt_algor_p6032.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/locore.h>

#ifdef ALGOR_P4032
#include <algor/algor/algor_p4032var.h>
#endif

#ifdef ALGOR_P5064
#include <algor/algor/algor_p5064var.h>
#endif  
 
#ifdef ALGOR_P6032
#include <algor/algor/algor_p6032var.h>
#endif

void	*(*algor_intr_establish)(int, int (*)(void *), void *);
void	(*algor_intr_disestablish)(void *);

void	(*algor_iointr)(u_int32_t, u_int32_t, u_int32_t, u_int32_t);

struct algor_soft_intrhand *softnet_intrhand;

u_long	cycles_per_hz;

/*
 * This is a mask of bits to clear in the SR when we go to a
 * given interrupt priority level.
 */
const u_int32_t ipl_sr_bits[_IPL_N] = {
	0,					/* IPL_NONE */

	MIPS_SOFT_INT_MASK_0,			/* IPL_SOFT */

	MIPS_SOFT_INT_MASK_0,			/* IPL_SOFTCLOCK */

	MIPS_SOFT_INT_MASK_0|
		MIPS_SOFT_INT_MASK_1,		/* IPL_SOFTNET */

	MIPS_SOFT_INT_MASK_0|
		MIPS_SOFT_INT_MASK_1,		/* IPL_SOFTSERIAL */

	MIPS_SOFT_INT_MASK_0|
		MIPS_SOFT_INT_MASK_1|
		MIPS_INT_MASK_0|
		MIPS_INT_MASK_1|
		MIPS_INT_MASK_2|
		MIPS_INT_MASK_3,		/* IPL_BIO */

	MIPS_SOFT_INT_MASK_0|
		MIPS_SOFT_INT_MASK_1|
		MIPS_INT_MASK_0|
		MIPS_INT_MASK_1|
		MIPS_INT_MASK_2|
		MIPS_INT_MASK_3,		/* IPL_NET */

	MIPS_SOFT_INT_MASK_0|
		MIPS_SOFT_INT_MASK_1|
		MIPS_INT_MASK_0|
		MIPS_INT_MASK_1|
		MIPS_INT_MASK_2|
		MIPS_INT_MASK_3,		/* IPL_{TTY,SERIAL} */

	MIPS_SOFT_INT_MASK_0|
		MIPS_SOFT_INT_MASK_1|
		MIPS_INT_MASK_0|
		MIPS_INT_MASK_1|
		MIPS_INT_MASK_2|
		MIPS_INT_MASK_3|
		MIPS_INT_MASK_4|
		MIPS_INT_MASK_5,		/* IPL_{CLOCK,HIGH} */
};

const u_int32_t ipl_si_to_sr[_IPL_NSOFT] = {
	MIPS_SOFT_INT_MASK_0,			/* IPL_SOFT */
	MIPS_SOFT_INT_MASK_0,			/* IPL_SOFTCLOCK */
	MIPS_SOFT_INT_MASK_1,			/* IPL_SOFTNET */
	MIPS_SOFT_INT_MASK_1,			/* IPL_SOFTSERIAL */
};

struct algor_soft_intr algor_soft_intrs[_IPL_NSOFT];

struct evcnt mips_int5_evcnt =
    EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "mips", "int 5 (clock)");

void
intr_init(void)
{

#if defined(ALGOR_P4032)
	algor_p4032_intr_init(&p4032_configuration);
#elif defined(ALGOR_P5064)
	algor_p5064_intr_init(&p5064_configuration);
#elif defined(ALGOR_P6032)
	algor_p6032_intr_init(&p6032_configuration);
#endif

	softintr_init();
}

void
cpu_intr(u_int32_t status, u_int32_t cause, u_int32_t pc, u_int32_t ipending)
{
	struct clockframe cf;
	struct algor_soft_intr *asi;
	struct algor_soft_intrhand *sih;
	int i, s;

	uvmexp.intrs++;

	if (ipending & MIPS_INT_MASK_5) {
		u_int32_t cycles = mips3_cp0_count_read();
		mips3_cp0_compare_write(cycles + cycles_per_hz);

		cf.pc = pc;
		cf.sr = status;
		hardclock(&cf);

		mips_int5_evcnt.ev_count++;

		/* Re-enable clock interrupts. */
		cause &= ~MIPS_INT_MASK_5;
		_splset(MIPS_SR_INT_IE |
		    ((status & ~cause) & MIPS_HARD_INT_MASK));
	}

	if (ipending & (MIPS_INT_MASK_0|MIPS_INT_MASK_1|MIPS_INT_MASK_2|
			MIPS_INT_MASK_3|MIPS_INT_MASK_4)) {
		/* Process I/O and error interrupts. */
		(*algor_iointr)(status, cause, pc, ipending);
	}

	ipending &= (MIPS_SOFT_INT_MASK_1|MIPS_SOFT_INT_MASK_0);
	if (ipending == 0)
		return;

	_clrsoftintr(ipending);

	for (i = _IPL_NSOFT - 1; i >= 0; i--) {
		if ((ipending & ipl_si_to_sr[i]) == 0)
			continue;

		asi = &algor_soft_intrs[i];

		if (TAILQ_FIRST(&asi->softintr_q) != NULL)
			asi->softintr_evcnt.ev_count++;

		for (;;) {
			s = splhigh();

			sih = TAILQ_FIRST(&asi->softintr_q);
			if (sih != NULL) {
				TAILQ_REMOVE(&asi->softintr_q, sih, sih_q);
				sih->sih_pending = 0;
			}

			splx(s);

			if (sih == NULL)
				break;

			uvmexp.softs++;
			(*sih->sih_fn)(sih->sih_arg);
		}
	}
}

/*
 * softintr_init:
 *
 *	Initialize the software interrupt system.
 */
void
softintr_init(void)
{
	static const char *softintr_names[] = IPL_SOFTNAMES;
	struct algor_soft_intr *asi;
	int i;

	for (i = 0; i < _IPL_NSOFT; i++) {
		asi = &algor_soft_intrs[i];
		TAILQ_INIT(&asi->softintr_q);
		asi->softintr_ipl = IPL_SOFT + i;
		evcnt_attach_dynamic(&asi->softintr_evcnt, EVCNT_TYPE_INTR,
		    NULL, "soft", softintr_names[i]);
	}

	/* XXX Establish legacy soft interrupt handlers. */
	softnet_intrhand = softintr_establish(IPL_SOFTNET,
	    (void (*)(void *))netintr, NULL);

	assert(softnet_intrhand != NULL);
}

/*
 * softintr_establish:		[interface]
 *
 *	Register a software interrupt handler.
 */
void *
softintr_establish(int ipl, void (*func)(void *), void *arg)
{
	struct algor_soft_intr *asi;
	struct algor_soft_intrhand *sih;

	if (__predict_false(ipl >= (IPL_SOFT + _IPL_NSOFT) ||
			    ipl < IPL_SOFT))
		panic("softintr_establish");

	asi = &algor_soft_intrs[ipl - IPL_SOFT];

	sih = malloc(sizeof(*sih), M_DEVBUF, M_NOWAIT);
	if (__predict_true(sih != NULL)) {
		sih->sih_intrhead = asi;
		sih->sih_fn = func;
		sih->sih_arg = arg;
		sih->sih_pending = 0;
	}
	return (sih);
}

/*
 * softintr_disestablish:	[interface]
 *
 *	Unregister a software interrupt handler.
 */
void
softintr_disestablish(void *arg)
{
	struct algor_soft_intrhand *sih = arg;
	struct algor_soft_intr *asi = sih->sih_intrhead;
	int s;

	s = splhigh();
	if (sih->sih_pending) {
		TAILQ_REMOVE(&asi->softintr_q, sih, sih_q);
		sih->sih_pending = 0;
	}
	splx(s);

	free(sih, M_DEVBUF);
}
