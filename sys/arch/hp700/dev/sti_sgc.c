/*	$NetBSD: sti_sgc.c,v 1.1.2.4 2002/10/10 18:32:46 jdolecek Exp $	*/

/*	$OpenBSD: sti_sgc.c,v 1.6 2001/09/11 20:05:24 miod Exp $	*/

/*
 * Copyright (c) 2000 Michael Shalayeff
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
/*
 * These cards has to be known to work so far:
 *	- HPA1991AGrayscale rev 0.02	(705/35) (byte-wide)
 *	- HPA1991AC19       rev 0.02	(715/33) (byte-wide)
 *	- HPA208LC1280      rev 8.04	(712/80) just works
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <uvm/uvm.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>

#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>

#include <hp700/hp700/machdep.h>
#include <hp700/dev/cpudevs.h>

#define	STI_MEMSIZE	0x1000000
#define	STI_ROMSIZE	0x0004000

int  sti_sgc_probe __P((struct device *, struct cfdata *, void *));
void sti_sgc_attach __P((struct device *, struct device *, void *));

CFATTACH_DECL(sti_sgc, sizeof(struct sti_softc),
    sti_sgc_probe, sti_sgc_attach, NULL, NULL);

int
sti_sgc_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	register struct confargs *ca = aux;
	bus_space_handle_t ioh, romh;
	u_int rom;
	u_char devtype;
	int rv = 0, romh_is_subregion;
	int pagezero_cookie;

	if (ca->ca_type.iodc_type != HPPA_TYPE_FIO ||
	    (ca->ca_type.iodc_sv_model != HPPA_FIO_GSGC &&
	     ca->ca_type.iodc_sv_model != HPPA_FIO_SGC))
		return 0;

	if ((rv = bus_space_map(ca->ca_iot, ca->ca_hpa, STI_MEMSIZE, 0, &ioh))) {
#ifdef STIDEBUG
		printf("st: cannot map io space (%d)\n", rv);
#endif
		return 0;
	}

	/*
	 * Locate STI ROM.
	 * On some machines it may not be part of the HPA space.
	 */
	pagezero_cookie = hp700_pagezero_map();
	if (PAGE0->pd_resv2[1] < HPPA_IOBEGIN) {
		rom = ca->ca_hpa;
	} else
		rom = PAGE0->pd_resv2[1];
	hp700_pagezero_unmap(pagezero_cookie);

#ifdef STIDEBUG
	printf ("sti: hpa=%x, rom=%x\n", ca->ca_hpa, rom);
#endif

	/*
	 * Map the ROM.
	 */
	if (ca->ca_hpa <= rom &&
	    (rom + STI_ROMSIZE) <= (ca->ca_hpa + STI_MEMSIZE)) {
		romh_is_subregion = TRUE;
		rv = bus_space_subregion(ca->ca_iot, ioh,
			rom - ca->ca_hpa, STI_ROMSIZE, &romh);
	} else {
		romh_is_subregion = FALSE;
		rv = bus_space_map(ca->ca_iot, rom, STI_ROMSIZE, 0, &romh);
	}
	if (rv) {
#ifdef STIDEBUG
		printf ("sti: cannot map rom space (%d)\n", rv);
#endif
		bus_space_unmap(ca->ca_iot, ioh,  STI_MEMSIZE);
		return 0;
	}

#ifdef STIDEBUG
	printf("sti: ioh=%x, romh=%x\n", ioh, romh);
#endif

	devtype = bus_space_read_1(ca->ca_iot, romh, 3);

#ifdef STIDEBUG
	printf("sti: devtype=%d\n", devtype);
#endif
#if 0 /* ignore this for now */
	if ((ca->ca_type.iodc_sv_model == HPPA_FIO_SGC &&
	     STI_ID_HI(STI_TYPE_BWGRF, rioh) == STI_ID_FDDI) ||
	    (devtype != STI_TYPE_BWGRF && devtype != STI_TYPE_WWGRF)) {
#ifdef STIDEBUG
		printf("sti: not a graphics device (%x)\n", devtype);
#endif
	} else
#endif
		rv = 1;

	bus_space_unmap(ca->ca_iot, ioh,  STI_MEMSIZE);
	if (!romh_is_subregion)
		bus_space_unmap(ca->ca_iot, romh, STI_ROMSIZE);
	return rv;
}

void
sti_sgc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sti_softc *sc = (void *)self;
	struct confargs *ca = aux;
	bus_addr_t addr;
	int rv;
	int pagezero_cookie;

	pagezero_cookie = hp700_pagezero_map();
	if (PAGE0->pd_resv2[1] < HPPA_IOBEGIN)
		addr = ca->ca_hpa;
	else
		addr = PAGE0->pd_resv2[1];
	hp700_pagezero_unmap(pagezero_cookie);

	sc->memt = sc->iot = ca->ca_iot;

	if ((rv = bus_space_map(ca->ca_iot, ca->ca_hpa, STI_MEMSIZE, 0,
	    &sc->ioh))) {
#ifdef STIDEBUG
		printf("st: cannot map io space (%d)\n", rv);
#endif
		return;
	}

	/*
	 * Map the ROM.
	 */
	if (ca->ca_hpa <= addr &&
	    (addr + STI_ROMSIZE) <= (ca->ca_hpa + STI_MEMSIZE)) {
		rv = bus_space_subregion(ca->ca_iot, sc->ioh,
			addr - ca->ca_hpa, STI_ROMSIZE, &sc->romh);
	} else {
		rv = bus_space_map(ca->ca_iot, addr, STI_ROMSIZE, 0, &sc->romh);
	}
	if (rv) {
#ifdef STIDEBUG
		printf ("sti: cannot map rom space (%d)\n", rv);
#endif
		bus_space_unmap(ca->ca_iot, sc->ioh,  STI_MEMSIZE);
		return;
	}

	sc->sc_devtype = bus_space_read_1(sc->iot, sc->romh, 3);
#ifdef STIDEBUG
	printf("sti: hpa=%x, rom=%x\n", ca->ca_hpa, addr);
	printf("sti: ioh=%x, romh=%x\n", sc->ioh, sc->romh);
#endif
	sti_attach_common(sc);
}

