/*	$NetBSD: softintr.c,v 1.3.26.1 2007/02/27 16:49:17 yamt Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: softintr.c,v 1.3.26.1 2007/02/27 16:49:17 yamt Exp $");

#include <sys/param.h>
#include <sys/malloc.h>

/* XXX Network interrupts should be converted to new softintrs. */
#include <net/netisr.h>

#include <uvm/uvm_extern.h>

#include <machine/intr.h>

struct soft_intrq soft_intrq[SI_NQUEUES];

struct soft_intrhand *softnet_intrhand;

void	netintr(void);

/*
 * softintr_init:
 *
 *	Initialize the software interrupt system.
 */
void
softintr_init(void)
{
	static const char * const softintr_names[] = SI_QUEUENAMES;
	struct soft_intrq *siq;
	int i;

	for (i = 0; i < SI_NQUEUES; i++) {
		siq = &soft_intrq[i];
		TAILQ_INIT(&siq->siq_list);
		evcnt_attach_dynamic(&siq->siq_evcnt, EVCNT_TYPE_INTR,
		    NULL, "soft", softintr_names[i]);
		siq->siq_si = i;
	}

	/* XXX Establish legacy software interrupt handlers. */
	softnet_intrhand = softintr_establish(IPL_SOFTNET,
	    (void (*)(void *))netintr, NULL);

	assert(softnet_intrhand != NULL);
}

/*
 * softintr_dispatch:
 *
 *	Process pending software interrupts on the specified queue.
 *
 *	NOTE: We must already be at the correct interrupt priority level.
 */
void
softintr_dispatch(int si)
{
	struct soft_intrq *siq = &soft_intrq[si];
	struct soft_intrhand *sih;
	int oldirqstate;

	siq->siq_evcnt.ev_count++;
	for (;;) {
		oldirqstate = disable_interrupts(I32_bit);
		sih = TAILQ_FIRST(&siq->siq_list);
		if (sih == NULL) {
			restore_interrupts(oldirqstate);
			break;
		}

		TAILQ_REMOVE(&siq->siq_list, sih, sih_list);
		sih->sih_pending = 0;

		uvmexp.softs++;

		restore_interrupts(oldirqstate);

		(*sih->sih_func)(sih->sih_arg);
	}
}

/*
 * softintr_establish:		[interface]
 *
 *	Register a software interrupt handler.
 */
void *
softintr_establish(int ipl, void (*func)(void *), void *arg)
{
	struct soft_intrhand *sih;
	int si;

	switch (ipl) {
	case IPL_SOFT:
		si = SI_SOFT;
		break;

	case IPL_SOFTCLOCK:
		si = SI_SOFTCLOCK;
		break;

	case IPL_SOFTNET:
		si = SI_SOFTNET;
		break;

	case IPL_SOFTSERIAL:
		si = SI_SOFTSERIAL;
		break;

	default:
		panic("softintr_establish: unknown soft IPL %d", ipl);
	}

	sih = malloc(sizeof(*sih), M_DEVBUF, M_NOWAIT);
	if (__predict_true(sih != NULL)) {
		sih->sih_func = func;
		sih->sih_arg = arg;
		sih->sih_siq = &soft_intrq[si];
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
	struct soft_intrhand *sih = arg;
	struct soft_intrq *siq = sih->sih_siq;
	int oldirqstate;

	oldirqstate = disable_interrupts(I32_bit);
	if (sih->sih_pending) {
		TAILQ_REMOVE(&siq->siq_list, sih, sih_list);
		sih->sih_pending = 0;
	}
	restore_interrupts(oldirqstate);

	free(sih, M_DEVBUF);
}

void
netintr(void)
{
	int n, s;

	s = splhigh();
	n = netisr;
	netisr = 0;
	splx(s);

#define	DONETISR(bit, fn)						\
	do {								\
		if (n & (1 << (bit)))					\
			fn();						\
	} while (/*CONSTCOND*/0)

#include <net/netisr_dispatch.h>

#undef DONETISR
}
