/*	$NetBSD: if_lc_isa.c,v 1.4.2.2 1997/12/22 00:13:46 perry Exp $ */

/*-
 * Copyright (c) 1994, 1995, 1997 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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

/*
 * DEC EtherWORKS 3 Ethernet Controllers
 *
 * Written by Matt Thomas
 *
 *   This driver supports the LEMAC (DE203, DE204, and DE205) cards.
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

#include <dev/ic/lemacreg.h>
#include <dev/ic/lemacvar.h>

#include <dev/isa/isavar.h>

extern struct cfdriver lc_cd;

static int lemac_isa_find __P((lemac_softc_t *, struct isa_attach_args *,
    int));
#ifdef __BROKEN_INDIRECT_CONFIG
static int lemac_isa_probe __P((struct device *, void *, void *));
#else
static int lemac_isa_probe __P((struct device *, struct cfdata *, void *));
#endif
static void lemac_isa_attach __P((struct device *, struct device *, void *));

struct cfattach lc_isa_ca = {
	sizeof(lemac_softc_t), lemac_isa_probe, lemac_isa_attach
};

static int
lemac_isa_find(sc, ia, attach)
	lemac_softc_t *sc;
	struct isa_attach_args *ia;
	int attach;
{
	bus_addr_t maddr;
	bus_addr_t msize;
	int rv = 0, irq;

	/*
	 * Disallow wildcarded i/o addresses.
	 */
	if (ia->ia_iobase == IOBASEUNK)
		return 0;

	/*
	 * Make sure this is a valid LEMAC address.
	 */
	if (ia->ia_iobase & (LEMAC_IOSIZE - 1))
		return 0;

	sc->sc_iot = ia->ia_iot;

	/*
	 * Map the LEMAC's port space for the probe sequence.
	 */
	ia->ia_iosize = LEMAC_IOSIZE;

	if (bus_space_map(sc->sc_iot, ia->ia_iobase, ia->ia_iosize, 0,
	    &sc->sc_ioh)) {
		if (attach)
			printf(": can't map i/o space\n");
		return 0;
	}

	/*
	 * Read the Ethernet address from the EEPROM.
	 * It must start with one of the DEC OUIs and pass the
	 * DEC ethernet checksum test.
	 */
	if (lemac_port_check(sc->sc_iot, sc->sc_ioh) == 0)
		goto outio;

	/*
	 * Get information about memory space and attempt to map it.
	 */
	lemac_info_get(sc->sc_iot, sc->sc_ioh, &maddr, &msize, &irq);

	if (ia->ia_maddr != maddr && ia->ia_maddr != MADDRUNK)
		goto outio;

	sc->sc_memt = ia->ia_memt;
	if (bus_space_map(ia->ia_memt, maddr, msize, 0, &sc->sc_memh)) {
		if (attach)
			printf(": can't map mem space\n");
		goto outio;
	}


	/*
	 * Double-check IRQ configuration.
	 */
	if (ia->ia_irq != irq && ia->ia_irq != IRQUNK)
		printf("%s: overriding IRQ %d to %d\n", sc->sc_dv.dv_xname,
		       ia->ia_irq, irq);

	if (attach) {
		sc->sc_ats = shutdownhook_establish(lemac_shutdown, sc);
		if (sc->sc_ats == NULL)
			printf("\n%s: warning: can't establish shutdown hook\n",
			    sc->sc_dv.dv_xname);

		lemac_ifattach(sc);

		sc->sc_ih = isa_intr_establish(ia->ia_ic, irq, IST_EDGE,
		    IPL_NET, lemac_intr, sc);
	}

	/*
	 * I guess we've found one.
	 */
	rv = 1;

	ia->ia_maddr = maddr;
	ia->ia_msize = msize;
	ia->ia_irq = irq;

	if (rv == 0 || !attach)
		bus_space_unmap(sc->sc_memt, sc->sc_memh, msize);
outio:
	if (rv == 0 || !attach)
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, LEMAC_IOSIZE);
	return rv;
}

static int
lemac_isa_probe(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct isa_attach_args *ia = aux;
#ifdef __BROKEN_INDIRECT_CONFIG
	struct cfdata *cf = ((struct device *)match)->dv_cfdata;
#else
	struct cfdata *cf = match;
#endif
	lemac_softc_t sc;
	(void)sprintf(sc.sc_dv.dv_xname, "%s%d", lc_cd.cd_name, cf->cf_unit);
    
	return lemac_isa_find(&sc, ia, 0);
}

static void
lemac_isa_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	lemac_softc_t *sc = (void *)self;
	struct isa_attach_args *ia = aux;

	(void) lemac_isa_find(sc, ia, 1);
}
