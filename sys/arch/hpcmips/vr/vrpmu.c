/*	$NetBSD: vrpmu.c,v 1.1.1.1 1999/09/16 12:23:33 takemura Exp $	*/

/*
 * Copyright (c) 1999 M. Warner Losh.  All rights reserved.
 * Copyright (c) 1999 PocketBSD Project. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <hpcmips/vr/vripvar.h>
#include <hpcmips/vr/vrpmuvar.h>
#include <hpcmips/vr/vrpmureg.h>

static int vrpmumatch __P((struct device *, struct cfdata *, void *));
static void vrpmuattach __P((struct device *, struct device *, void *));

static void vrpmu_write __P((struct vrpmu_softc *, int, unsigned short));
static unsigned short vrpmu_read __P((struct vrpmu_softc *, int));

int vrpmu_intr __P((void *));

struct cfattach vrpmu_ca = {
	sizeof(struct vrpmu_softc), vrpmumatch, vrpmuattach
};

static int
vrpmumatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	return 1;
}

static void
vrpmuattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct vrpmu_softc *sc = (struct vrpmu_softc *)self;
	struct vrip_attach_args *va = aux;

	bus_space_tag_t iot = va->va_iot;
	bus_space_handle_t ioh;

	if (bus_space_map(iot, va->va_addr, 1, 0, &ioh)) {
		printf(": can't map bus space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;

	if (!(sc->sc_handler = 
	      vrip_intr_establish(va->va_vc, va->va_intr, IPL_TTY,
				  vrpmu_intr, sc))) {
		printf (": can't map interrupt line.\n");
		return;
	}

	printf("\n");
}

static inline void
vrpmu_write(sc, port, val)
	struct vrpmu_softc *sc;
	int port;
	unsigned short val;
{
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, port, val);
}

static inline unsigned short
vrpmu_read(sc, port)
	struct vrpmu_softc *sc;
	int port;
{
	return bus_space_read_2(sc->sc_iot, sc->sc_ioh, port);
}

/*
 * XXX
 *
 * In the following interrupt routine we should actually DO something
 * with the knowledge that we've gained.  For now we just report it.
 */
int
vrpmu_intr(arg)
	void *arg;
{
        struct vrpmu_softc *sc = arg;
	unsigned int intmask;

	intmask = vrpmu_read(sc, PMUINT_REG_W);
	vrpmu_write(sc, PMUINT_REG_W, intmask);

	if (intmask & PMUINT_GPIO3)
		printf("vrpmu: GPIO[3] activation\n");
	if (intmask & PMUINT_GPIO2)
		printf("vrpmu: GPIO[2] activation\n");
	if (intmask & PMUINT_GPIO1)
		printf("vrpmu: GPIO[1] activation\n");
	if (intmask & PMUINT_GPIO0)
		printf("vrpmu: GPIO[0] activation\n");

	if (intmask & PMUINT_RTC)
		printf("vrpmu: RTC alarm detected\n");
	if (intmask & PMUINT_BATT)
		printf("vrpmu: Battery low during activation\n");

	if (intmask & PMUINT_TIMOUTRST)
		printf("vrpmu: HAL timer reset\n");
	if (intmask & PMUINT_RTCRST)
		printf("vrpmu: RTC reset detected\n");
	if (intmask & PMUINT_RSTSWRST)
		printf("vrpmu: RESET switch detected\n");
	if (intmask & PMUINT_DMSWRST)
		printf("vrpmu: Deadman's switch detected\n");
	if (intmask & PMUINT_BATTINTR)
		printf("vrpmu: Battery low during normal ops\n");
	if (intmask & PMUINT_POWERSW)
		printf("vrpmu: POWER switch detected\n");

	intmask = vrpmu_read(sc, PMUINT2_REG_W);
	vrpmu_write(sc, PMUINT2_REG_W, intmask);

	if (intmask & PMUINT_GPIO12)
		printf("vrpmu: GPIO[12] activation\n");
	if (intmask & PMUINT_GPIO11)
		printf("vrpmu: GPIO[11] activation\n");
	if (intmask & PMUINT_GPIO10)
		printf("vrpmu: GPIO[10] activation\n");
	if (intmask & PMUINT_GPIO9)
		printf("vrpmu: GPIO[9] activation\n");

	return 0;
}
