/* $NetBSD: acemidi.c,v 1.2.2.2 2002/06/23 17:48:26 jdolecek Exp $ */

/*-
 * Copyright (c) 2001 Ben Harris
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acemidi.c,v 1.2.2.2 2002/06/23 17:48:26 jdolecek Exp $");

#include <sys/param.h>

#include <sys/device.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/podulebus/podulebus.h>
#include <dev/podulebus/podules.h>
#include <dev/podulebus/acemidireg.h>

#include <sys/termios.h>
#include <dev/ic/comvar.h>
#include <dev/ic/comreg.h>

struct acemidi_softc {
	struct		device sc_dev;
};

struct com_acemidi_softc {
	struct		com_softc sc_com;
	struct		evcnt sc_intrcnt;
};

static int acemidi_match(struct device *, struct cfdata *, void *);
static void acemidi_attach(struct device *, struct device *, void *);
static int com_acemidi_match(struct device *, struct cfdata *, void *);
static void com_acemidi_attach(struct device *, struct device *, void *);

struct cfattach acemidi_ca = {
	sizeof(struct acemidi_softc), acemidi_match, acemidi_attach
};

struct cfattach com_acemidi_ca = {
	sizeof(struct com_acemidi_softc), com_acemidi_match, com_acemidi_attach
};

static int
acemidi_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct podulebus_attach_args *pa = aux;

	if (pa->pa_product == PODULE_MIDICONNECT)
		return 1;
	return 0;
}

static void
acemidi_attach(struct device *parent, struct device *self, void *aux)
{
/*	struct acemidi_softc *sc = (void *)self; */
/*	struct podulebus_attach_args *pa = aux; */

	printf("\n");
	config_found_sm(self, aux, NULL, NULL);
}

static int
com_acemidi_match(struct device *parent, struct cfdata *cf, void *aux)
{

	return 1;
}

static void
com_acemidi_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_acemidi_softc *sc = (void *)self;
	struct com_softc *csc = &sc->sc_com;
	struct podulebus_attach_args *pa = aux;

	csc->sc_iobase = pa->pa_fast_base + ACEMIDI_16550_BASE;
	csc->sc_iot = pa->pa_fast_t;
	bus_space_map(csc->sc_iot, csc->sc_iobase, COM_NPORTS, 0,
	    &csc->sc_ioh);
	csc->sc_frequency = ACEMIDI_16550_FREQ;

	com_attach_subr(csc);

	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, NULL,
	    self->dv_xname, "intr");
	podulebus_irq_establish(pa->pa_ih, IPL_SERIAL, comintr, sc,
	    &sc->sc_intrcnt);
}

/*
 * Stray IRQ bug:
 * 
 * Occasionally, when receiving, we get a stray IRQ.  Sometimes, the interrupt
 * bit on the unixbp reads as clear.  In any case, comintr() gets an IIR
 * of 0xc1 (no interrupts pending).
 *
 * The behaviour can be observed with a logic probe:
 *
 * Channel 1 to PIRQ* (pin 19 on IC3 on A540 backplane)
 * Channel 2 to INTR on 16550
 * trigger on ch1 low, ch2 falling
 * 2 us/div
 *
 * This catches cases where the 16550 de-asserts the interrupt before
 * irq_handler is entered and disables the interrupt at unixbp (by calling
 * splhigh()).
 *
 * This gets us 5us pulses on INTR and PIRQ*.  Now to work out why.
 *
 * Connecting channel 3 to the CS2* pin on the 16550 shows it high throughout,
 * so the interrupt isn't being cleared by the host.  MR, similarly, is low
 * throughout, so it's not being cleared by a reset.
 */
