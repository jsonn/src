/*	$NetBSD: softintr.c,v 1.1.6.3 2002/01/08 00:25:29 nathanw Exp $	*/

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
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

/*
 * Generic soft interrupt implementation for NetBSD/i386.
 *
 * Note: This code is expected to go away when the i386-MP
 * branch is merged.
 *
 * Also note: the netisr processing is left in i386/isa/icu.s
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: softintr.c,v 1.1.6.3 2002/01/08 00:25:29 nathanw Exp $");

#include <sys/param.h>
#include <sys/malloc.h>

#include <machine/intr.h>

#include <uvm/uvm_extern.h>

struct i386_soft_intr i386_soft_intrs[I386_NSOFTINTR];

const int i386_soft_intr_to_ssir[I386_NSOFTINTR] = {
	SIR_CLOCK,
	SIR_NET,
	SIR_SERIAL,
};

/*
 * softintr_init:
 *
 *	Initialize the software interrupt system.
 */
void
softintr_init(void)
{
	struct i386_soft_intr *si;
	int i;

	for (i = 0; i < I386_NSOFTINTR; i++) {
		si = &i386_soft_intrs[i];
		TAILQ_INIT(&si->softintr_q);
		si->softintr_ssir = i386_soft_intr_to_ssir[i];
	}
}

/*
 * softintr_dispatch:
 *
 *	Process pending software interrupts.
 */
void
softintr_dispatch(int which)
{
	struct i386_soft_intr *si = &i386_soft_intrs[which];
	struct i386_soft_intrhand *sih;
	int s;

	for (;;) {
		i386_softintr_lock(si, s);
		sih = TAILQ_FIRST(&si->softintr_q);
		if (sih == NULL) {
			i386_softintr_unlock(si, s);
			break;
		}
		TAILQ_REMOVE(&si->softintr_q, sih, sih_q);
		sih->sih_pending = 0;
		i386_softintr_unlock(si, s);

		uvmexp.softs++;
		(*sih->sih_fn)(sih->sih_arg);
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
	struct i386_soft_intr *si;
	struct i386_soft_intrhand *sih;
	int which;

	switch (ipl) {
	case IPL_SOFTCLOCK:
		which = I386_SOFTINTR_SOFTCLOCK;
		break;

	case IPL_SOFTNET:
		which = I386_SOFTINTR_SOFTNET;
		break;

	case IPL_SOFTSERIAL:
		which = I386_SOFTINTR_SOFTSERIAL;
		break;

	default:
		panic("softintr_establish");
	}

	si = &i386_soft_intrs[which];

	sih = malloc(sizeof(*sih), M_DEVBUF, M_NOWAIT);
	if (__predict_true(sih != NULL)) {
		sih->sih_intrhead = si;
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
	struct i386_soft_intrhand *sih = arg;
	struct i386_soft_intr *si = sih->sih_intrhead;
	int s;

	i386_softintr_lock(si, s);
	if (sih->sih_pending) {
		TAILQ_REMOVE(&si->softintr_q, sih, sih_q);
		sih->sih_pending = 0;
	}
	i386_softintr_unlock(si, s);

	free(sih, M_DEVBUF);
}
