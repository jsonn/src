/*	$NetBSD: cpu.c,v 1.6.16.4 2007/12/07 17:26:09 yamt Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cpu.c,v 1.6.16.4 2007/12/07 17:26:09 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <sh3/clock.h>
#include <sh3/cache.h>
#include <sh3/mmu.h>

#include <machine/autoconf.h>

extern struct cfdriver cpu_cd;

static int cpu_match(struct device *, struct cfdata *, void *);
static void cpu_attach(struct device *, struct device *, void *);

CFATTACH_DECL(cpu, sizeof (struct device),
    cpu_match, cpu_attach, NULL, NULL);


static int
cpu_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, cpu_cd.cd_name) != 0)
		return (0);

	return (1);
}

static void
cpu_attach(struct device *parent, struct device *self, void *aux)
{

#define	MHZ(x) ((x) / 1000000), (((x) % 1000000) / 1000)

	aprint_naive("\n");
	aprint_normal(": SH%d %d.%02d MHz PCLOCK %d.%02d MHz\n",
	       CPU_IS_SH3 ? 3 : 4,
	       MHZ(sh_clock_get_cpuclock()),
	       MHZ(sh_clock_get_pclock()));

#undef MHZ

	sh_cache_information();
	sh_mmu_information();
}
