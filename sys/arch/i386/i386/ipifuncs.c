/* $NetBSD: ipifuncs.c,v 1.1.2.12 2001/12/29 23:31:01 sommerfeld Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
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


#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

/*
 * Interprocessor interrupt handlers.
 */

#include "opt_ddb.h"
#include "opt_mtrr.h"
#include "npx.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/intr.h>
#include <machine/atomic.h>
#include <machine/cpuvar.h>
#include <machine/i82093var.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#include <machine/mtrr.h>
#include <machine/gdt.h>

#include <ddb/db_output.h>

void i386_ipi_halt(struct cpu_info *);

#if NNPX > 0
void i386_ipi_synch_fpu(struct cpu_info *);
void i386_ipi_flush_fpu(struct cpu_info *);
#else
#define i386_ipi_synch_fpu 0
#define i386_ipi_flush_fpu 0
#endif

#ifdef MTRR
void i386_reload_mtrr(struct cpu_info *);
#else
#define i386_reload_mtrr NULL
#endif

void (*ipifunc[I386_NIPI])(struct cpu_info *) =
{
	i386_ipi_halt,
#if defined(I586_CPU) || defined(I686_CPU)
	tsc_microset,
#else
	0,
#endif
	i386_ipi_flush_fpu,
	i386_ipi_synch_fpu,
	pmap_do_tlb_shootdown,
	i386_reload_mtrr,
	gdt_reload_cpu,
};

void
i386_ipi_halt(struct cpu_info *ci)
{
	disable_intr();

	printf("%s: shutting down\n", ci->ci_dev->dv_xname);
	for(;;) {
		asm volatile("hlt");
	}
}

#if NNPX > 0
void
i386_ipi_flush_fpu(struct cpu_info *ci)
{
	npxsave_cpu(ci, 0);
}

void
i386_ipi_synch_fpu(struct cpu_info *ci)
{
	npxsave_cpu(ci, 1);
}
#endif

void
i386_spurious (void)
{
}

#ifdef MTRR

/*
 * mtrr_reload_cpu() is a macro in mtrr.h which picks the appropriate
 * function to use..
 */

void
i386_reload_mtrr(struct cpu_info *ci)
{
	if (mtrr_funcs != NULL)
		mtrr_reload_cpu(ci);
}
#endif

void
i386_send_ipi (struct cpu_info *ci, int ipimask)
{
	int ret;

	i386_atomic_setbits_l(&ci->ci_ipis, ipimask);

	/* Don't send IPI to cpu which isn't (yet) running. */
	if (!(ci->ci_flags & CPUF_RUNNING))
		return;

	ret = i386_ipi(LAPIC_IPI_VECTOR, ci->ci_cpuid, LAPIC_DLMODE_FIXED);
	if (ret != 0) {
		printf("ipi of %x from %s to %s failed\n",
		    ipimask,
		    curcpu()->ci_dev->dv_xname,
		    ci->ci_dev->dv_xname);
	}

}

void
i386_self_ipi (int vector)
{
	i82489_writereg(LAPIC_ICRLO,
	    vector | LAPIC_DLMODE_FIXED | LAPIC_LVL_ASSERT | LAPIC_DEST_SELF);
}


void
i386_broadcast_ipi (int ipimask)
{
	struct cpu_info *ci, *self = curcpu();
	int count = 0;

	CPU_INFO_ITERATOR cii;

	for (CPU_INFO_FOREACH(cii, ci)) {
		if (ci == self)
			continue;
		if ((ci->ci_flags & CPUF_RUNNING) == 0)
			continue;
		i386_atomic_setbits_l(&ci->ci_ipis, ipimask);
		count++;
	}
	if (!count)
		return;

	i82489_writereg(LAPIC_ICRLO,
	    LAPIC_IPI_VECTOR | LAPIC_DLMODE_FIXED | LAPIC_LVL_ASSERT |
	    LAPIC_DEST_ALLEXCL);
}

void
i386_multicast_ipi (int cpumask, int ipimask)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	cpumask &= ~(1U << cpu_number());
	if (cpumask == 0)
		return;

	for (CPU_INFO_FOREACH(cii, ci)) {
		if ((cpumask & (1U << ci->ci_cpuid)) == 0)
			continue;
		i386_send_ipi(ci, ipimask);
	}
}

void
i386_ipi_handler(void)
{
	struct cpu_info *ci = curcpu();
	u_int32_t pending;
	int bit;

	pending = i386_atomic_testset_ul(&ci->ci_ipis, 0);

#if 0
	printf("%s: pending IPIs: %x\n", ci->ci_dev->dv_xname, pending);
#endif

	for (bit = 0; bit < I386_NIPI && pending; bit++) {
		if (pending & (1<<bit)) {
			pending &= ~(1<<bit);
			(*ipifunc[bit])(ci);
		}
	}
#if 0
	Debugger();
#endif
}
