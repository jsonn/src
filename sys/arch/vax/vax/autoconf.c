/*	$NetBSD: autoconf.c,v 1.37.4.1 1999/06/21 01:03:44 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/conf.h>

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <machine/nexus.h>
#include <machine/ioa.h>
#include <machine/ka820.h>
#include <machine/ka750.h>
#include <machine/ka650.h>
#include <machine/clock.h>
#include <machine/rpb.h>

#include <vax/vax/gencons.h>

#include <vax/bi/bireg.h>

#include "locators.h"

void	gencnslask __P((void));

struct cpu_dep *dep_call;
int	mastercpu;	/* chief of the system */
struct device *booted_from;

#define MAINBUS	0

void
configure()
{

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("mainbus not configured");

	/*
	 * We're ready to start up. Clear CPU and soft cold start flags.
	 */
	cold = 0;
	if (dep_call->cpu_clrf) 
		(*dep_call->cpu_clrf)();
}

void
cpu_rootconf()
{
	struct device *booted_device = NULL;
	int booted_partition = 0;

	/*
	 * The device we booted from are looked for during autoconfig.
	 * There can only be one match.
	 */
	if ((bootdev & B_MAGICMASK) == (u_long)B_DEVMAGIC) {
		booted_device = booted_from;
		booted_partition = B_PARTITION(bootdev);
	}

	printf("boot device: %s\n",
	    booted_device ? booted_device->dv_xname : "<unknown>");

	setroot(booted_device, booted_partition);
}

int	mainbus_print __P((void *, const char *));
int	mainbus_match __P((struct device *, struct cfdata *, void *));
void	mainbus_attach __P((struct device *, struct device *, void *));

int
mainbus_print(aux, hej)
	void *aux;
	const char *hej;
{
	struct bp_conf *bp = aux;
	if (hej)
		printf("%s%d at %s", bp->type, bp->num, hej);
	return (UNCONF);
}

int
mainbus_match(parent, cf, aux)
	struct	device	*parent;
	struct cfdata *cf;
	void	*aux;
{
	if (cf->cf_unit == 0 &&
	    strcmp(cf->cf_driver->cd_name, "mainbus") == 0)
		return 1; /* First (and only) mainbus */

	return (0);
}

#if VAX8600
static	void find_sbi __P((struct device *, struct bp_conf *,
	    int (*) __P((void *, const char *))));
#endif

void
mainbus_attach(parent, self, hej)
	struct	device	*parent, *self;
	void	*hej;
{
	struct bp_conf bp;

	printf("\n");
	bp.partyp = MAINBUS;

	if (vax_bustype & VAX_CPUBUS) {
		bp.type = "cpu";
		bp.num = 0;
		config_found(self, &bp, mainbus_print);
	}
	if (vax_bustype & VAX_VSBUS) {
		bp.type = "vsbus";
		bp.num = 0;
		config_found(self, &bp, mainbus_print);
	}
	if (vax_bustype & VAX_SBIBUS) {
		bp.type = "sbi";
		bp.num = 0;
		config_found(self, &bp, mainbus_print);
	}
	if (vax_bustype & VAX_CMIBUS) {
		bp.type = "cmi";
		bp.num = 0;
		config_found(self, &bp, mainbus_print);
	}
	if (vax_bustype & VAX_UNIBUS) {
		bp.type = "uba";
		bp.num = 0;
		config_found(self, &bp, mainbus_print);
	}
#if VAX8600
	if (vax_bustype & VAX_MEMBUS) {
		bp.type = "mem";
		bp.num = 0;
		config_found(self, &bp, mainbus_print);
	}
	if (vax_cputype == VAX_8600)
		find_sbi(self, &bp, mainbus_print);
#endif

#if VAX8200 || VAX8800
	bp.type = "bi";
	if (vax_bustype & VAX_BIBUS) {

		switch (vax_cputype) {
#if VAX8200
		case VAX_8200: {
			bp.bp_addr = BI_BASE(0,0);
			config_found(self, &bp, mainbus_print);
			break;
		}
#endif
#ifdef notyet
		case VAX_8800: {
			int bi, biaddr;

			for (bi = 0; bi < MAXNBI; bi++) {
				biaddr = BI_BASE(bi) + BI_PROBE;
				if (badaddr((caddr_t)biaddr, 4))
					continue;

				bp.bp_addr = BI_BASE(bi);
				config_found(self, &bp, mainbus_print);
			}
			break;
		}
#endif
		}
	}
#endif
	if (dep_call->cpu_subconf)
		(*dep_call->cpu_subconf)(self);
}

#if VAX8600
void
find_sbi(self, bp, print)
	struct	device *self;
	struct	bp_conf *bp;
	int	(*print) __P((void *, const char *));
{
	volatile int tmp;
	volatile struct sbia_regs *sbiar;
	struct ioa *ioa;
	int	type, i;

	for (i = 0; i < MAXNIOA; i++) {
		ioa = (struct ioa *)vax_map_physmem((paddr_t)IOA8600(0),
		    (IOAMAPSIZ / VAX_NBPG));
		if (badaddr((caddr_t)ioa, 4)) {
			vax_unmap_physmem((vaddr_t)ioa, (IOAMAPSIZ / VAX_NBPG));
			continue;
		}
		tmp = ioa->ioacsr.ioa_csr;
		type = tmp & IOA_TYPMSK;

		switch (type) {

		case IOA_SBIA:
			bp->type = "sbi";
			bp->num = i;
			config_found(self, bp, mainbus_print);
			sbiar = (void *)ioa;
			sbiar->sbi_errsum = -1;
			sbiar->sbi_error = 0x1000;
			sbiar->sbi_fltsts = 0xc0000;
			break;

		default:
			printf("IOAdapter %x unsupported\n", type);
			break;
		}
		vax_unmap_physmem((vaddr_t)ioa, (IOAMAPSIZ / VAX_NBPG));
	}
}
#endif

int	cpu_match __P((struct  device  *, struct cfdata *, void *));
void	cpu_attach __P((struct	device	*, struct  device  *, void *));


int
cpu_match(parent, cf, aux)
	struct	device	*parent;
	struct cfdata *cf;
	void *aux;
{
	struct bp_conf *bp = aux;

	if (strcmp(bp->type, "cpu"))
		return 0;

	switch (vax_cputype) {
#if VAX750 || VAX630 || VAX650 || VAX780 || VAX8600 || VAX410
	case VAX_750:
	case VAX_78032:
	case VAX_650:
	case VAX_780:
	case VAX_8600:
	default:
		if(cf->cf_unit == 0 && bp->partyp == MAINBUS)
			return 1;
		break;
#endif
	};

	return 0;
}

void
cpu_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	(*dep_call->cpu_conf)(parent, self, aux);
}

int	mem_match __P((struct  device  *, struct cfdata *, void *));
void	mem_attach __P((struct	device	*, struct  device  *, void *));

int
mem_match(parent, cf, aux)
	struct	device	*parent;
	struct cfdata *cf;
	void	*aux;
{
	struct	sbi_attach_args *sa = (struct sbi_attach_args *)aux;
#if VAX8600
	struct	bp_conf *bp = aux;

	if (vax_cputype == VAX_8600 && !strcmp(parent->dv_xname, "mainbus0")) {
		if (strcmp(bp->type, "mem"))
			return 0;
		return 1;
	}
#endif
	if (cf->cf_loc[SBICF_TR] != sa->nexnum && cf->cf_loc[SBICF_TR] > -1)
		return 0;

	switch (sa->type) {
	case NEX_MEM4:
	case NEX_MEM4I:
	case NEX_MEM16:
	case NEX_MEM16I:
		sa->nexinfo = M780C;
		break;

	case NEX_MEM64I:
	case NEX_MEM64L:
	case NEX_MEM64LI:
	case NEX_MEM256I:
	case NEX_MEM256L:
	case NEX_MEM256LI:
		sa->nexinfo = M780EL;
		break;

	case NEX_MEM64U:
	case NEX_MEM64UI:
	case NEX_MEM256U:
	case NEX_MEM256UI:
		sa->nexinfo = M780EU;
		break;

	default:
		return 0;
	}
	return 1;
}

void	ka86_memenable __P((int, int));		/* XXX */
void	ka780_memenable __P((void *, void *));	/* XXX */

void
mem_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	struct	sbi_attach_args *sa = (struct sbi_attach_args *)aux;
	struct	mem_softc *sc = (void *)self;

#if VAX8600
	if (vax_cputype == VAX_8600) {
		ka86_memenable(0, 0);
		printf("\n");
		return;
	}
#endif
	sc->sc_memaddr = sa->nexaddr;
	sc->sc_memtype = sa->nexinfo;
	sc->sc_memnr = sa->type;
#ifdef VAX780
	ka780_memenable(sa, sc);
#endif
}

struct	cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct	cfattach cpu_mainbus_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

struct	cfattach mem_mainbus_ca = {
	sizeof(struct mem_softc), mem_match, mem_attach
};

struct	cfattach mem_sbi_ca = {
	sizeof(struct mem_softc), mem_match, mem_attach
};

void
device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	if ((B_TYPE(bootdev) == BDEV_QE) &&
	    !strcmp("qe", dev->dv_cfdata->cf_driver->cd_name))
		booted_from = dev;
}
