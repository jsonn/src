/*	$NetBSD: if_ep_isa.c,v 1.8.4.2 1997/02/27 19:17:32 is Exp $	*/

/*
 * Copyright (c) 1996 Jason R. Thorpe <thorpej@beer.org>
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
#include <net/if_types.h>

#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
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

int ep_isa_probe __P((struct device *, void *, void *));
void ep_isa_attach __P((struct device *, struct device *, void *));

struct cfattach ep_isa_ca = {
	sizeof(struct ep_softc), ep_isa_probe, ep_isa_attach
};

static	void epaddcard __P((int, int, int));

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
} epcards[MAXEPCARDS];
static int nepcards;

static void
epaddcard(bus, iobase, irq)
	int bus, iobase, irq;
{

	if (nepcards >= MAXEPCARDS)
		return;
	epcards[nepcards].bus = bus;
	epcards[nepcards].iobase = iobase;
	epcards[nepcards].irq = (irq == 2) ? 9 : irq;
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
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
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
		if ((model & 0xfff0) != PROD_ID) {
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
		epaddcard(bus, iobase, irq);

		/* so card will not respond to contention again */
		bus_space_write_1(iot, ioh, 0, TAG_ADAPTER + 1);

		/*
		 * XXX: this should probably not be done here
		 * because it enables the drq/irq lines from
		 * the board. Perhaps it should be done after
		 * we have checked for irq/drq collisions?
		 */
		bus_space_write_1(iot, ioh, 0, ACTIVATE_ADAPTER_TO_CONFIG);
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

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, ia->ia_iosize, 0, &ioh))
		panic("ep_isa_attach: can't map i/o space");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->bustype = EP_BUS_ISA;

	printf(": 3Com 3C509 Ethernet\n");

	/* we can't easily tell if this is a 3c509, 3c509B, or 3c515 */
	epconfig(sc, EP_CHIPSET_UNKNOWN);	/* XXX */

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, epintr, sc);
}
