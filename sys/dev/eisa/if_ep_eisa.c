/*	$NetBSD: if_ep_eisa.c,v 1.13.4.2 1997/09/27 02:00:08 marc Exp $	*/

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

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

#ifdef __BROKEN_INDIRECT_CONFIG
int ep_eisa_match __P((struct device *, void *, void *));
#else
int ep_eisa_match __P((struct device *, struct cfdata *, void *));
#endif
void ep_eisa_attach __P((struct device *, struct device *, void *));

struct cfattach ep_eisa_ca = {
	sizeof(struct ep_softc), ep_eisa_match, ep_eisa_attach
};

/* XXX move these somewhere else */
#define EISA_CONTROL	0x0c84
#define EISA_RESET	0x04
#define EISA_ERROR	0x02
#define EISA_ENABLE	0x01

int
ep_eisa_match(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct eisa_attach_args *ea = aux;

	/* must match one of our known ID strings */
	if (strcmp(ea->ea_idstring, "TCM5091") &&
	    strcmp(ea->ea_idstring, "TCM5092") &&
	    strcmp(ea->ea_idstring, "TCM5093") &&
	    strcmp(ea->ea_idstring, "TCM5920") &&
	    strcmp(ea->ea_idstring, "TCM5970") &&
	    strcmp(ea->ea_idstring, "TCM5971") &&
	    strcmp(ea->ea_idstring, "TCM5972"))
		return (0);

	return (1);
}

void
ep_eisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ep_softc *sc = (void *)self;
	struct eisa_attach_args *ea = aux;
	bus_space_tag_t iot = ea->ea_iot;
	bus_space_handle_t ioh;
	u_int16_t k;
	eisa_chipset_tag_t ec = ea->ea_ec;
	eisa_intr_handle_t ih;
	const char *model, *intrstr;
	int chipset;
	u_int irq;

	/* Map i/o space. */
	if (bus_space_map(iot, EISA_SLOT_ADDR(ea->ea_slot),
	    EISA_SLOT_SIZE, 0, &ioh))
		panic("ep_eisa_attach: can't map i/o space");

	sc->bustype = EP_BUS_EISA;
	sc->sc_ioh = ioh;
	sc->sc_iot = iot;

	/* Reset card. */
	bus_space_write_1(iot, ioh, EISA_CONTROL, EISA_ENABLE | EISA_RESET);
	delay(10);
	bus_space_write_1(iot, ioh, EISA_CONTROL, EISA_ENABLE);
	/* Wait for reset? */
	delay(1000);

	/* XXX What is this doing?!  Reading the i/o address? */
	k = bus_space_read_2(iot, ioh, EP_W0_ADDRESS_CFG);
	k = (k & 0x1f) * 0x10 + 0x200;

	/* Read the IRQ from the card. */
	irq = bus_space_read_2(iot, ioh, EP_W0_RESOURCE_CFG) >> 12;

	chipset = EP_CHIPSET_3C509;	/* assume dumb chipset */
	if (strcmp(ea->ea_idstring, "TCM5091") == 0)
		model = EISA_PRODUCT_TCM5091;
	else if (strcmp(ea->ea_idstring, "TCM5092") == 0)
		model = EISA_PRODUCT_TCM5092;
	else if (strcmp(ea->ea_idstring, "TCM5093") == 0)
		model = EISA_PRODUCT_TCM5093;
	else if (strcmp(ea->ea_idstring, "TCM5920") == 0) {
		model = EISA_PRODUCT_TCM5920;
		chipset = EP_CHIPSET_VORTEX;
	}
	else if (strcmp(ea->ea_idstring, "TCM5970") == 0) {
		model = EISA_PRODUCT_TCM5970;
		chipset = EP_CHIPSET_VORTEX;
	}
	else if (strcmp(ea->ea_idstring, "TCM5971") == 0) {
		model = EISA_PRODUCT_TCM5971;
		chipset = EP_CHIPSET_VORTEX;
	}
	else if (strcmp(ea->ea_idstring, "TCM5972") == 0) {
		model = EISA_PRODUCT_TCM5972;
		chipset = EP_CHIPSET_VORTEX;
	}
	else
		model = "unknown model!";
	printf(": %s\n", model);

	if (eisa_intr_map(ec, irq, &ih)) {
		printf("%s: couldn't map interrupt (%u)\n",
		    sc->sc_dev.dv_xname, irq);
		return;
	}
	intrstr = eisa_intr_string(ec, ih);
	sc->sc_ih = eisa_intr_establish(ec, ih, IST_EDGE, IPL_NET,
	    epintr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	if (intrstr != NULL)
		printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname,
		    intrstr);

	sc->enable = NULL;
	sc->disable = NULL;
	sc->enabled = 1;

	epconfig(sc, chipset, NULL);
}
