/*	$NetBSD: cpu.c,v 1.1.4.2 2002/07/14 17:46:19 gehenna Exp $	*/

/*	$OpenBSD: cpu.c,v 1.8 2000/08/15 20:38:24 mickey Exp $	*/

/*
 * Copyright (c) 1998,1999 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF MIND,
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/cpufunc.h>
#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <hp700/hp700/intr.h>
#include <hp700/hp700/machdep.h>
#include <hp700/dev/cpudevs.h>

struct cpu_softc {
	struct  device sc_dev;

	hppa_hpa_t sc_hpa;
	void *sc_ih;
};

int	cpumatch __P((struct device *, struct cfdata *, void *));
void	cpuattach __P((struct device *, struct device *, void *));

struct cfattach cpu_ca = {
	sizeof(struct cpu_softc), cpumatch, cpuattach
};

int
cpumatch(parent, cf, aux)   
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct confargs *ca = aux;

	/* there will be only one for now XXX */
	/* probe any 1.0, 1.1 or 2.0 */
	if (cf->cf_unit > 0 ||
	    ca->ca_type.iodc_type != HPPA_TYPE_NPROC ||
	    ca->ca_type.iodc_sv_model != HPPA_NPROC_HPPA)
		return 0;

	return 1;
}

void
cpuattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	/* machdep.c */
	extern struct pdc_cache pdc_cache;
	extern struct pdc_btlb pdc_btlb;
	extern u_int cpu_ticksnum, cpu_ticksdenom;

	struct pdc_model pdc_model PDC_ALIGNMENT;
	struct pdc_cpuid pdc_cpuid PDC_ALIGNMENT;
	u_int pdc_cversion[32] PDC_ALIGNMENT;
	register struct cpu_softc *sc = (struct cpu_softc *)self;
	register struct confargs *ca = aux;
	const char *p = NULL;
	u_int mhz = 100 * cpu_ticksnum / cpu_ticksdenom;
	int err;

	bzero (&pdc_cpuid, sizeof(pdc_cpuid));
	if (pdc_call((iodcio_t)pdc, 0, PDC_MODEL, PDC_MODEL_CPUID,
		     &pdc_cpuid, sc->sc_dev.dv_unit, 0, 0, 0) >= 0) {

		/* patch for old 8200 */
		if (pdc_cpuid.version == HPPA_CPU_PCXUP &&
		    pdc_cpuid.revision > 0x0d)
			pdc_cpuid.version = HPPA_CPU_PCXUP1;
			
		p = hppa_mod_info(HPPA_TYPE_CPU, pdc_cpuid.version);
	}
	/* otherwise try to guess on component version numbers */
	else if (pdc_call((iodcio_t)pdc, 0, PDC_MODEL, PDC_MODEL_COMP,
		     &pdc_cversion, sc->sc_dev.dv_unit) >= 0) {
		/* XXX p = hppa_mod_info(HPPA_TYPE_CPU,pdc_cversion[0]); */
	}

	printf (": %s rev %d, ", p? p : cpu_typename, (*cpu_desidhash)());

	if ((err = pdc_call((iodcio_t)pdc, 0, PDC_MODEL, PDC_MODEL_INFO,
			    &pdc_model)) < 0) {
#ifdef DEBUG
		printf("WARNING: PDC_MODEL failed (%d)\n", err);
#endif
	} else {
		static const char lvls[4][4] = { "0", "1", "1.5", "2" };

		printf("lev %s, cat %c, ",
		       lvls[pdc_model.pa_lvl], "AB"[pdc_model.mc]);
	}

	printf ("%d", mhz / 100);
	if (mhz % 100 > 9)
		printf(".%02d", mhz % 100);

	printf(" MHz clk\n%s: %s", self->dv_xname,
	       pdc_model.sh? "shadows, ": "");

	if (pdc_cache.dc_conf.cc_sh)
		printf("%uK cache", pdc_cache.dc_size / 1024);
	else
		printf("%uK/%uK D/I caches",
		       pdc_cache.dc_size / 1024,
		       pdc_cache.ic_size / 1024);
	if (pdc_cache.dt_conf.tc_sh)
		printf(", %u shared TLB", pdc_cache.dt_size);
	else
		printf(", %u/%u D/I TLBs",
		       pdc_cache.dt_size, pdc_cache.it_size);

	if (pdc_btlb.finfo.num_c)
		printf(", %u shared BTLB", pdc_btlb.finfo.num_c);
	else {
		printf(", %u/%u D/I BTLBs",
		       pdc_btlb.finfo.num_i,
		       pdc_btlb.finfo.num_d);
	}

	/*
	 * Describe the floating-point support.
	 */
#ifndef	FPEMUL
	if (!fpu_present)
		printf("\n%s: no floating point support",
		    self->dv_xname);
	else
#endif /* !FPEMUL */
		{
			printf("\n%s: %s floating point, rev %d",
			    self->dv_xname,
			    hppa_mod_info(HPPA_TYPE_FPU, 
				(fpu_version >> 16) & 0x1f),
			    (fpu_version >> 11) & 0x1f);
		}

	printf("\n");

	/* sanity against luser amongst config editors */
	if (ca->ca_irq == 31) {
		sc->sc_ih = hp700_intr_establish(&sc->sc_dev, IPL_CLOCK,
						 clock_intr, NULL /*trapframe*/,
						 &int_reg_cpu, ca->ca_irq);
	} else {
		printf ("%s: bad irq number %d\n", sc->sc_dev.dv_xname,
			ca->ca_irq);
	}
}
