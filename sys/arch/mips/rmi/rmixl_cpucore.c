/*	$NetBSD: rmixl_cpucore.c,v 1.1.2.3 2010/01/20 20:48:12 matt Exp $	*/

/*
 * Copyright 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Simon Burge for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
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

#include "locators.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rmixl_cpucore.c,v 1.1.2.3 2010/01/20 20:48:12 matt Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/cpu.h>
#include <mips/rmi/rmixlvar.h>
#include <mips/rmi/rmixl_cpunodevar.h>
#include <mips/rmi/rmixl_cpucorevar.h>

static int	cpucore_rmixl_match(device_t, cfdata_t, void *);
static void	cpucore_rmixl_attach(device_t, device_t, void *);
static int	cpucore_rmixl_print(void *, const char *);

CFATTACH_DECL_NEW(cpucore_rmixl, sizeof(struct cpucore_softc),
	cpucore_rmixl_match, cpucore_rmixl_attach, NULL, NULL);

static int
cpucore_rmixl_match(device_t parent, cfdata_t cf, void *aux)
{
	struct cpunode_attach_args *na = aux;
	int core = cf->cf_loc[CPUNODECF_CORE];

	if (!cpu_rmixl(mips_options.mips_cpu))
		return 0;

	if (strncmp(na->na_name, cf->cf_name, strlen(cf->cf_name)) == 0
#ifndef MULTIPROCESSOR
	    && na->na_core == 0
#endif
	    && (core == CPUNODECF_CORE_DEFAULT || core == na->na_core))
		return 1;

	return 0;
}

static void
cpucore_rmixl_attach(device_t parent, device_t self, void *aux)
{
	struct cpucore_softc * const sc = device_private(self);
	struct cpunode_attach_args *na = aux;
	struct cpucore_attach_args ca;
	u_int nthreads;

	sc->sc_dev = self;
	sc->sc_core = na->na_core;

	aprint_normal("\n");
	aprint_normal_dev(self, "%lu.%02luMHz (hz cycles = %lu, "
	    "delay divisor = %lu)\n",
	    curcpu()->ci_cpu_freq / 1000000,
	    (curcpu()->ci_cpu_freq % 1000000) / 10000,
	    curcpu()->ci_cycles_per_hz, curcpu()->ci_divisor_delay);

	aprint_normal("%s: ", device_xname(self));
	cpu_identify(self);

	nthreads = MIPS_CIDFL_RMI_NTHREADS(mips_options.mips_cpu->cpu_cidflags);
	aprint_normal_dev(self, "%d %s on core\n", nthreads,
		nthreads == 1 ? "thread" : "threads");

	/*
	 * Attach CPU (RMI thread contexts) devices
	 */
	for (int i=0; i < nthreads; i++) {
		ca.ca_name = "cpu";
		ca.ca_thread = i;
		ca.ca_core = sc->sc_core;
		config_found(self, &ca, cpucore_rmixl_print);
	}
}

static int
cpucore_rmixl_print(void *aux, const char *pnp)
{
	struct cpucore_attach_args *ca = aux;

	if (pnp != NULL)
		aprint_normal("%s:", pnp);
	aprint_normal(" thread %d", ca->ca_thread);

	return (UNCONF);
}
