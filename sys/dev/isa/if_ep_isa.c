/*	$NetBSD: if_ep_isa.c,v 1.15.4.3 1997/10/14 10:23:48 thorpej Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

/*
 * Copyright (c) 1997 Jonathan Stone <jonathan@NetBSD.org>
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@beer.org>
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
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>


#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#include <dev/isa/isavar.h>
#include <dev/isa/elink.h>

#ifdef __BROKEN_INDIRECT_CONFIG
int ep_isa_probe __P((struct device *, void *, void *));
#else
int ep_isa_probe __P((struct device *, struct cfdata *, void *));
#endif
void ep_isa_attach __P((struct device *, struct device *, void *));

struct cfattach ep_isa_ca = {
	sizeof(struct ep_softc), ep_isa_probe, ep_isa_attach
};

static	void epaddcard __P((int, int, int, int));

/*
 * This keeps track of which ISAs have been through an ep probe sequence.
 * A simple static variable isn't enough, since it's conceivable that
 * a system might have more than one ISA bus.
 *
 * The "er_bus" member is the unit number of the parent ISA bus, e.g. "0"
 * for "isa0".
 */
struct ep_isa_done_probe {
	LIST_ENTRY(ep_isa_done_probe)	er_link;
	int				er_bus;
};
static LIST_HEAD(, ep_isa_done_probe) ep_isa_all_probes;
static int ep_isa_probes_initialized;

#define MAXEPCARDS	20	/* if you have more than 20, you lose */

static struct epcard {
	int	bus;
	int	iobase;
	int	irq;
	char	available;
	long	model;
} epcards[MAXEPCARDS];
static int nepcards;

static void
epaddcard(bus, iobase, irq, model)
	int bus, iobase, irq, model;
{

	if (nepcards >= MAXEPCARDS)
		return;
	epcards[nepcards].bus = bus;
	epcards[nepcards].iobase = iobase;
	epcards[nepcards].irq = (irq == 2) ? 9 : irq;
	epcards[nepcards].model = model;
	epcards[nepcards].available = 1;
	nepcards++;
}

/*
 * 3c509 cards on the ISA bus are probed in ethernet address order.
 * The probe sequence requires careful orchestration, and we'd like
 * like to allow the irq and base address to be wildcarded. So, we
 * probe all the cards the first time epprobe() is called. On subsequent
 * calls we look for matching cards.
 */
int
ep_isa_probe(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh, ioh2;
	int slot, iobase, irq, i;
	u_int16_t vendor, model;
	struct ep_isa_done_probe *er;
	int bus = parent->dv_unit;

	if (ep_isa_probes_initialized == 0) {
		LIST_INIT(&ep_isa_all_probes);
		ep_isa_probes_initialized = 1;
	}

	/*
	 * Probe this bus if we haven't done so already.
	 */
	for (er = ep_isa_all_probes.lh_first; er != NULL;
	    er = er->er_link.le_next)
		if (er->er_bus == parent->dv_unit)
			goto bus_probed;

	/*
	 * Mark this bus so we don't probe it again.
	 */
	er = (struct ep_isa_done_probe *)
	    malloc(sizeof(struct ep_isa_done_probe), M_DEVBUF, M_NOWAIT);
	if (er == NULL)
		panic("ep_isa_probe: can't allocate state storage");

	er->er_bus = bus;
	LIST_INSERT_HEAD(&ep_isa_all_probes, er, er_link);

	/*
	 * Map the Etherlink ID port for the probe sequence.
	 */
	if (bus_space_map(iot, ELINK_ID_PORT, 1, 0, &ioh)) {
		printf("ep_isa_probe: can't map Etherlink ID port\n");
		return 0;
	}

	for (slot = 0; slot < MAXEPCARDS; slot++) {
		elink_reset(iot, ioh, parent->dv_unit);
		elink_idseq(iot, ioh, ELINK_509_POLY);

		/* Untag all the adapters so they will talk to us. */
		if (slot == 0)
			bus_space_write_1(iot, ioh, 0, TAG_ADAPTER + 0);

		vendor = htons(epreadeeprom(iot, ioh, EEPROM_MFG_ID));
		if (vendor != MFG_ID)
			continue;

		model = htons(epreadeeprom(iot, ioh, EEPROM_PROD_ID));
		/*
		 * XXX: Add a new product id to check for other cards
		 * (3c515?) and fix the check in ep_isa_attach.
		 */
		if ((model & 0xfff0) != PROD_ID_3C509) {
#ifndef trusted
			printf(
			 "ep_isa_probe: ignoring model %04x\n", model);
#endif
			continue;
			}

		iobase = epreadeeprom(iot, ioh, EEPROM_ADDR_CFG);
		iobase = (iobase & 0x1f) * 0x10 + 0x200;

		irq = epreadeeprom(iot, ioh, EEPROM_RESOURCE_CFG);
		irq >>= 12;

		/* so card will not respond to contention again */
		bus_space_write_1(iot, ioh, 0, TAG_ADAPTER + 1);

		/*
		 * XXX: this should probably not be done here
		 * because it enables the drq/irq lines from
		 * the board. Perhaps it should be done after
		 * we have checked for irq/drq collisions?
		 */
		bus_space_write_1(iot, ioh, 0, ACTIVATE_ADAPTER_TO_CONFIG);

		/*
		 * Don't attach a 3c509 in PnP mode.
		 */
		if ((model & 0xfff0) == PROD_ID_3C509) {
			if (bus_space_map(iot, iobase, 1, 0, &ioh2)) {
				printf(
				"ep_isa_probe: can't map Etherlink iobase\n");
				return 0;
			}
			if (bus_space_read_2(iot, ioh2, EP_W0_EEPROM_COMMAND)
			    & EEPROM_TST_MODE) {
				printf(
				 "3COM 3C509 Ethernet card in PnP mode\n");
				continue;
			}
			bus_space_unmap(iot, ioh2, 1);
		}
		epaddcard(bus, iobase, irq, model);
	}
	/* XXX should we sort by ethernet address? */

	bus_space_unmap(iot, ioh, 1);

bus_probed:

	for (i = 0; i < nepcards; i++) {
		if (epcards[i].bus != bus)
			continue;
		if (epcards[i].available == 0)
			continue;
		if (ia->ia_iobase != IOBASEUNK &&
		    ia->ia_iobase != epcards[i].iobase)
			continue;
		if (ia->ia_irq != IRQUNK &&
		    ia->ia_irq != epcards[i].irq)
			continue;
		goto good;
	}
	return 0;

good:
	epcards[i].available = 0;
	ia->ia_iobase = epcards[i].iobase;
	ia->ia_irq = epcards[i].irq;
	ia->ia_iosize = 0x10;
	ia->ia_msize = 0;
	ia->ia_aux = (void *)epcards[i].model;
	return 1;
}

void
ep_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ep_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int chipset;

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, ia->ia_iosize, 0, &ioh))
		panic("ep_isa_attach: can't map i/o space");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->bustype = EP_BUS_ISA;

	sc->enable = NULL;
	sc->disable = NULL;
	sc->enabled = 1;

	chipset = (int)(long)ia->ia_aux;
	if ((chipset & 0xfff0) == PROD_ID_3C509) {
		printf(": 3Com 3C509 Ethernet\n");
		epconfig(sc, EP_CHIPSET_3C509, NULL);
	} else {
		/*
		 * XXX: Maybe a 3c515, but the check in ep_isa_probe looks
		 * at the moment only for a 3c509.
		 */
		printf(": unknown 3Com Ethernet card: %04x\n", chipset);
		epconfig(sc, EP_CHIPSET_UNKNOWN, NULL);
	}

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, epintr, sc);
}
