/*	$NetBSD: softintr.c,v 1.2.2.3 2000/11/22 16:00:58 bouyer Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford and Jason R. Thorpe.
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
 * Generic soft interrupt implementation for mvme68k.
 * Based heavily on the alpha implementation by Jason Thorpe.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/sched.h>
#include <sys/vmmeter.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/intr.h>

#include <mvme68k/mvme68k/isr.h>


struct mvme68k_soft_intrhand	*softnet_intrhand;
struct mvme68k_soft_intrhand	*softclock_intrhand;

static struct mvme68k_soft_intr mvme68k_soft_intrs[IPL_NSOFT];

static __inline int
ssir_pending(volatile unsigned char *ssptr)
{
	int __rv;

	__asm __volatile(
		"	moveq	#0, %1	\n"
		"	tas	%0	\n"
		"	jne	1f	\n"
		"	moveq	#1, %1	\n"
		"1:			\n"
		: "=m" (*ssptr), "=d" (__rv));

	return (__rv);
}

/*
 * softintr_init()
 *
 *	Initialise mvme68k software interrupt subsystem.
 */
void
softintr_init()
{
	static const char *softintr_names[] = IPL_SOFTNAMES;
	struct mvme68k_soft_intr *msi;
	int i;

	for (i = 0; i < IPL_NSOFT; i++) {
		msi = &mvme68k_soft_intrs[i];
		LIST_INIT(&msi->msi_q);
		msi->msi_ssir = 1;
		evcnt_attach_dynamic(&msi->msi_evcnt, EVCNT_TYPE_INTR,
		    NULL, "soft", softintr_names[i]);
	}

	/* Establish legacy software interrupt handlers */
	softnet_intrhand = softintr_establish(IPL_SOFTNET,
	    (void (*)(void *)) netintr, NULL);
	softclock_intrhand = softintr_establish(IPL_SOFTCLOCK,
	    (void (*)(void *)) softclock, NULL);

#ifdef DEBUG
	assert(softnet_intrhand != NULL);
	assert(softclock_intrhand != NULL);
#endif
}

/*
 * softintr_dispatch()
 *
 *	Internal function for running queued soft interrupts.
 */
void
softintr_dispatch()
{
	struct mvme68k_soft_intr *msi;
	struct mvme68k_soft_intrhand *sih;
	int i;

	while (ssir_pending(&ssir)) {
		for (i = 0; i < IPL_NSOFT; i++) {
			if (ssir_pending(&mvme68k_soft_intrs[i].msi_ssir) == 0)
				continue;

			msi = &mvme68k_soft_intrs[i];

			msi->msi_evcnt.ev_count++;

			for (sih = LIST_FIRST(&msi->msi_q);
			     sih != NULL;
			     sih = LIST_NEXT(sih, sih_q)) {
				if (sih->sih_pending) {
					uvmexp.softs++;
					sih->sih_pending = 0;
					(*sih->sih_fn)(sih->sih_arg);
				}
			}
		}
	}
}

/*
 * softintr_establish()		[interface]
 *
 *	Register a software interrupt handler.
 */
void *
softintr_establish(int ipl, void (*func)(void *), void *arg)
{
	struct mvme68k_soft_intr *msi;
	struct mvme68k_soft_intrhand *sih;
	int s;

	if (__predict_false(ipl >= IPL_NSOFT || ipl < 0 || func == NULL))
		panic("softintr_establish");

	msi = &mvme68k_soft_intrs[ipl];

	sih = malloc(sizeof(*sih), M_DEVBUF, M_NOWAIT);
	if (__predict_true(sih != NULL)) {
		sih->sih_intrhead = msi;
		sih->sih_fn = func;
		sih->sih_arg = arg;
		sih->sih_pending = 0;
		s = splsoft();
		LIST_INSERT_HEAD(&msi->msi_q, sih, sih_q);
		splx(s);
	}
	return (sih);
}

/*
 * softintr_disestablish()	[interface]
 *
 *	Unregister a software interrupt handler.
 */
void
softintr_disestablish(void *arg)
{
	struct mvme68k_soft_intrhand *sih = arg;
	int s;

	s = splsoft();
	LIST_REMOVE(sih, sih_q);
	splx(s);

	free(sih, M_DEVBUF);
}
