/* $NetBSD: ipifuncs.c,v 1.6.8.1 1999/12/27 18:31:21 wrstuden Exp $ */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: ipifuncs.c,v 1.6.8.1 1999/12/27 18:31:21 wrstuden Exp $");

/*
 * Interprocessor interrupt handlers.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <machine/atomic.h>
#include <machine/alpha_cpu.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/rpb.h>

void	alpha_ipi_halt __P((void));
void	alpha_ipi_tbia __P((void));
void	alpha_ipi_tbiap __P((void));
void	alpha_ipi_imb __P((void));
void	alpha_ipi_ast __P((void));

/*
 * NOTE: This table must be kept in order with the bit definitions
 * in <machine/intr.h>.
 */
ipifunc_t ipifuncs[ALPHA_NIPIS] = {
	alpha_ipi_halt,
	alpha_ipi_tbia,
	alpha_ipi_tbiap,
	pmap_do_tlb_shootdown,
	alpha_ipi_imb,
	alpha_ipi_ast,
};

/*
 * Send an interprocessor interrupt.
 */
void
alpha_send_ipi(cpu_id, ipimask)
	u_long cpu_id, ipimask;
{

#ifdef DIAGNOSTIC
	if (cpu_id >= hwrpb->rpb_pcs_cnt ||
	    cpu_info[cpu_id].ci_dev == NULL)
		panic("alpha_sched_ipi: bogus cpu_id");
#endif

	alpha_atomic_setbits_q(&cpu_info[cpu_id].ci_ipis, ipimask);
printf("SENDING IPI TO %lu\n", cpu_id);
	alpha_pal_wripir(cpu_id);
printf("IPI SENT\n");
}

/*
 * Broadcast an IPI to all but ourselves.
 */
void
alpha_broadcast_ipi(ipimask)
	u_long ipimask;
{
	u_long i;

	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		if (cpu_info[i].ci_dev == NULL)
			continue;
		alpha_send_ipi(i, ipimask);
	}
}

void
alpha_ipi_halt()
{
	u_long cpu_id = alpha_pal_whami();
	struct pcs *pcsp = LOCATE_PCS(hwrpb, cpu_id);

	/* Disable interrupts. */
	(void) splhigh();

	printf("%s: shutting down...\n", cpu_info[cpu_id].ci_dev->dv_xname);
	alpha_atomic_clearbits_q(&cpus_running, (1UL << cpu_id));

	pcsp->pcs_flags &= ~(PCS_RC | PCS_HALT_REQ);
	pcsp->pcs_flags |= PCS_HALT_STAY_HALTED;
	alpha_pal_halt();
	/* NOTREACHED */
}

void
alpha_ipi_tbia()
{
	u_long cpu_id = alpha_pal_whami();

	/* If we're doing a TBIA, we don't need to do a TBIAP or a SHOOTDOWN. */
	alpha_atomic_clearbits_q(&cpu_info[cpu_id].ci_ipis,
	    ALPHA_IPI_TBIAP|ALPHA_IPI_SHOOTDOWN);

	ALPHA_TBIA();
}

void
alpha_ipi_tbiap()
{

	/* Can't clear SHOOTDOWN here; might have PG_ASM mappings. */

	ALPHA_TBIAP();
}

void
alpha_ipi_imb()
{

	alpha_pal_imb();
}

void
alpha_ipi_ast()
{

	aston();
}
