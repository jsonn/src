/*	$NetBSD: pcppi.c,v 1.2.2.3 1997/02/01 02:17:32 cgd Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <alpha/isa/pcppivar.h>

#include "pckbd.h"

struct pcppi_softc {
	struct device sc_dv;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_pit1_ioh, sc_ppi_ioh;
};

int	pcppi_match __P((struct device *, struct cfdata *, void *));
void	pcppi_attach __P((struct device *, struct device *, void *));

struct cfattach pcppi_ca = {
	sizeof(struct pcppi_softc), pcppi_match, pcppi_attach,
};

struct cfdriver pcppi_cd = {
	NULL, "pcppi", DV_DULL,
};

int	pcppiprint __P((void *, const char *));

static int	pcppi_console, pcppi_console_attached;
bus_space_tag_t	pcppi_console_iot;
bus_space_handle_t pcppi_console_pit1_ioh, pcppi_console_ppi_ioh;

int
pcppi_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ppi_ioh, pit1_ioh;
	int have_pit1, have_ppi, rv;
	u_int8_t v, nv;

	/* If values are hardwired to something that they can't be, punt. */
	if (ia->ia_iobase != IOBASEUNK || /* ia->ia_iosize != 0 || XXX isa.c */
	    ia->ia_maddr != MADDRUNK || ia->ia_msize != 0 ||
	    ia->ia_irq != IRQUNK || ia->ia_drq != DRQUNK)
		return (0);

	rv = 0;
	have_pit1 = have_ppi = 0;

	if (pcppi_console && !pcppi_console_attached &&
	    pcppi_console_iot == ia->ia_iot) {
		rv = 1;
		goto lose;	/* XXX misnomer */
	}

	if (bus_space_map(ia->ia_iot, 0x40, 4, 0, &pit1_ioh))	/* XXX */
		goto lose;
	have_pit1 = 1;
	if (bus_space_map(ia->ia_iot, 0x60, 4, 0, &ppi_ioh))	/* XXX */
		goto lose;
	have_ppi = 1;

	/*
	 * Check for existence of PPI.  Realistically, this is either going to
	 * be here or nothing is going to be here.
	 *
	 * We don't want to have any chance of changing speaker output (which
	 * this test might, if it crashes in the middle, or something;
	 * normally it's be to quick to produce anthing audible), but
	 * many "combo chip" mock-PPI's don't seem to support the top bit
	 * of Port B as a settable bit.  The bottom bit has to be settable,
	 * since the speaker driver hardware still uses it.
	 */
	v = bus_space_read_1(ia->ia_iot, ppi_ioh, 1);		/* XXX */
	bus_space_write_1(ia->ia_iot, ppi_ioh, 1, v ^ 0x01);	/* XXX */
	nv = bus_space_read_1(ia->ia_iot, ppi_ioh, 1);		/* XXX */
	if (((nv ^ v) & 0x01) == 0x01)
		rv = 1;
	bus_space_write_1(ia->ia_iot, ppi_ioh, 1, v);		/* XXX */
	nv = bus_space_read_1(ia->ia_iot, ppi_ioh, 1);		/* XXX */
	if (((nv ^ v) & 0x01) != 0x00)
		rv = 0;

	/*
	 * We assume that the programmable interval timer is there.
	 */

lose:
	if (have_pit1)
		bus_space_unmap(ia->ia_iot, pit1_ioh, 4);
	if (have_ppi)
		bus_space_unmap(ia->ia_iot, ppi_ioh, 4);
	if (rv) {
		ia->ia_iobase = 0x60;
		ia->ia_iosize = 0x5;
		ia->ia_msize = 0x0;
	}
	return (rv);
}

void
pcppi_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcppi_softc *sc = (struct pcppi_softc *)self;
	struct isa_attach_args *ia = aux;
	struct pcppi_attach_args pa;
	bus_space_tag_t iot;

	sc->sc_iot = iot = ia->ia_iot;

	if (pcppi_console && pcppi_console_iot == ia->ia_iot) {
		KASSERT(!pcppi_console_attached);
		pcppi_console_attached = 1;

		sc->sc_pit1_ioh = pcppi_console_pit1_ioh;
		sc->sc_ppi_ioh = pcppi_console_ppi_ioh;
	} else if (bus_space_map(iot, 0x40, 4, 0, &sc->sc_pit1_ioh) ||	/*XXX*/
	    bus_space_map(iot, 0x60, 5, 0, &sc->sc_ppi_ioh))		/*XXX*/
		panic("pcppi_attach: couldn't map");

	printf("\n");

	pa.pa_slot = PCPPI_KBD_SLOT;
	pa.pa_iot = iot;				/* XXX */
	pa.pa_ioh = sc->sc_ppi_ioh;			/* XXX */
	pa.pa_pit_ioh = sc->sc_pit1_ioh;		/* XXX */
	pa.pa_delaybah = ia->ia_delaybah;		/* XXX */
	pa.pa_ic = ia->ia_ic;				/* XXX */
	config_found(self, &pa, pcppiprint);

	/* XXX SHOULD ONLY ATTACH IF SOMETHING IS THERE */
	pa.pa_slot = PCPPI_AUX_SLOT;
	pa.pa_iot = iot;				/* XXX */
	pa.pa_ioh = sc->sc_ppi_ioh;			/* XXX */
	pa.pa_pit_ioh = sc->sc_pit1_ioh;		/* XXX */
	pa.pa_delaybah = ia->ia_delaybah;		/* XXX */
	pa.pa_ic = ia->ia_ic;				/* XXX */
	config_found(self, &pa, pcppiprint);
}

int
pcppiprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct pcppi_attach_args *pa = aux;
	const char *type;

	switch (pa->pa_slot) {
	case PCPPI_KBD_SLOT:
		type = "pckbd";
		break;
	case PCPPI_AUX_SLOT:
		type = "pms";
		/* XXX XXX XXX should make sure it's there before configuring */
		return (QUIET);
		break;
	default:
		panic("pcppiprint: bad slot");
	}

        if (pnp)
                printf("%s at %s", type, pnp);
        return (UNCONF);
}

void
pcppi_attach_console(iot)
	bus_space_tag_t iot;
{
	extern void pckbd_attach_console __P((bus_space_tag_t,
	    bus_space_handle_t, bus_space_handle_t));

	pcppi_console = 1;

	pcppi_console_iot = iot;

	if (bus_space_map(iot, 0x40, 4, 0, &pcppi_console_pit1_ioh) ||	/*XXX*/
            bus_space_map(iot, 0x60, 5, 0, &pcppi_console_ppi_ioh))	/*XXX*/
                panic("pcppi_attach_console: couldn't map");

	pckbd_attach_console(iot, pcppi_console_ppi_ioh,
	    pcppi_console_pit1_ioh);
}
