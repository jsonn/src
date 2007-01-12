/*	$NetBSD: interrupt.c,v 1.9.20.1 2007/01/12 01:00:49 ad Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: interrupt.c,v 1.9.20.1 2007/01/12 01:00:49 ad Exp $");

#include "opt_vr41xx.h"
#include "opt_tx39xx.h"

#include <sys/param.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/sysconf.h>

extern const u_int32_t __ipl_sr_bits_vr[];
extern const u_int32_t __ipl_sr_bits_tx[];

const u_int32_t ipl_si_to_sr[SI_NQUEUES] = {
	[SI_SOFT] = MIPS_SOFT_INT_MASK_0,
	[SI_SOFTCLOCK] = MIPS_SOFT_INT_MASK_0,
	[SI_SOFTNET] = MIPS_SOFT_INT_MASK_1,
	[SI_SOFTSERIAL] = MIPS_SOFT_INT_MASK_1,
};

const u_int32_t *ipl_sr_bits;
struct hpcmips_soft_intrhand *softnet_intrhand;
struct hpcmips_soft_intr hpcmips_soft_intrs[SI_NQUEUES];

void
intr_init()
{

	ipl_sr_bits = CPUISMIPS3 ? __ipl_sr_bits_vr : __ipl_sr_bits_tx;
}

#if defined(VR41XX) && defined(TX39XX)
/*
 * cpu_intr:
 *
 *	handle MIPS CPU interrupt.
 *	if VR41XX only or TX39XX only kernel, directly jump to each handler
 *	(tx/tx39icu.c, vr/vr.c), don't use this dispather.
 * 
 */
void
cpu_intr(u_int32_t status, u_int32_t cause, u_int32_t pc, u_int32_t ipending)
{

	(*platform.cpu_intr)(status, cause, pc, ipending);
}
#endif /* VR41XX && TX39XX */

/*
 * softintr:
 *
 *	dispatch pending software interrupt handler.
 */
void
softintr(u_int32_t ipending)
{
	struct hpcmips_soft_intr *asi;
	struct hpcmips_soft_intrhand *sih;
	int i, s;

	ipending &= (MIPS_SOFT_INT_MASK_1 | MIPS_SOFT_INT_MASK_0);
	if (ipending == 0)
		return;

	_clrsoftintr(ipending);

	for (i = SI_NQUEUES - 1; i >= 0; i--) {
		if ((ipending & ipl_si_to_sr[i]) == 0)
			continue;

		asi = &hpcmips_soft_intrs[i];

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
	static const char *softintr_names[] = SI_QUEUENAMES;
	struct hpcmips_soft_intr *asi;
	int i;

	for (i = 0; i < SI_NQUEUES; i++) {
		asi = &hpcmips_soft_intrs[i];
		TAILQ_INIT(&asi->softintr_q);
		asi->softintr_siq = i;
		simple_lock_init(&asi->softintr_slock);
		evcnt_attach_dynamic(&asi->softintr_evcnt, EVCNT_TYPE_INTR,
		    NULL, "soft", softintr_names[i]);
	}

	/* XXX Establish legacy soft interrupt handlers. */
	softnet_intrhand = softintr_establish(IPL_SOFTNET,
	    (void (*)(void *))netintr, NULL);

	assert(softnet_intrhand != NULL);
}

static int
ipl2si(ipl_t ipl)
{
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
		panic("ipl2si: %d", ipl);
	}
	return si;
}

/*
 * softintr_establish:		[interface]
 *
 *	Register a software interrupt handler.
 */
void *
softintr_establish(int ipl, void (*func)(void *), void *arg)
{
	struct hpcmips_soft_intr *asi;
	struct hpcmips_soft_intrhand *sih;
	int si;

	si = ipl2si(ipl);
	asi = &hpcmips_soft_intrs[si];

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
	struct hpcmips_soft_intrhand *sih = arg;
	struct hpcmips_soft_intr *asi = sih->sih_intrhead;
	int s;

	s = splhigh();
	if (sih->sih_pending) {
		TAILQ_REMOVE(&asi->softintr_q, sih, sih_q);
		sih->sih_pending = 0;
	}
	splx(s);

	free(sih, M_DEVBUF);
}
