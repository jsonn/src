/*	$NetBSD: ipifuncs.c,v 1.16.6.2 2009/01/17 13:27:48 mjf Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: ipifuncs.c,v 1.16.6.2 2009/01/17 13:27:48 mjf Exp $");

/*
 * Interprocessor interrupt handlers.
 */

#include "opt_ddb.h"
#include "opt_mtrr.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/intr.h>

#include <uvm/uvm_extern.h>

#include <machine/cpuvar.h>
#include <machine/i82093var.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#include <machine/mtrr.h>
#include <machine/gdt.h>
#include <machine/fpu.h>

#include <x86/cpu_msr.h>

#include <ddb/db_output.h>

#include "acpi.h"

void x86_64_ipi_halt(struct cpu_info *);
void x86_64_ipi_kpreempt(struct cpu_info *);

void x86_64_ipi_synch_fpu(struct cpu_info *);

#ifdef MTRR
void x86_64_reload_mtrr(struct cpu_info *);
#else
#define x86_64_reload_mtrr NULL
#endif

#if NACPI > 0
void acpi_cpu_sleep(struct cpu_info *);
#else
#define	acpi_cpu_sleep NULL
#endif

void (*ipifunc[X86_NIPI])(struct cpu_info *) =
{
	x86_64_ipi_halt,
	NULL,
	NULL,
	x86_64_ipi_synch_fpu,
	x86_64_reload_mtrr,
	gdt_reload_cpu,
	msr_write_ipi,
	acpi_cpu_sleep,
	x86_64_ipi_kpreempt,
};

void
x86_64_ipi_halt(struct cpu_info *ci)
{
	x86_disable_intr();

	for(;;) {
		x86_hlt();
	}
}

void
x86_64_ipi_synch_fpu(struct cpu_info *ci)
{
	fpusave_cpu(true);
}

#ifdef MTRR

/*
 * mtrr_reload_cpu() is a macro in mtrr.h which picks the appropriate
 * function to use..
 */

void
x86_64_reload_mtrr(struct cpu_info *ci)
{
	if (mtrr_funcs != NULL)
		mtrr_reload_cpu(ci);
}
#endif

void
x86_64_ipi_kpreempt(struct cpu_info *ci)
{

	softint_trigger(1 << SIR_PREEMPT);
}
