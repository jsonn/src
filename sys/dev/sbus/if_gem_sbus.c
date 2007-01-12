/*	$NetBSD: if_gem_sbus.c,v 1.1.8.2 2007/01/12 00:57:48 ad Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 * SBus front-end for the GEM network driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_gem_sbus.c,v 1.1.8.2 2007/01/12 00:57:48 ad Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>

#include <dev/sbus/sbusvar.h>

#include <dev/ic/gemreg.h>
#include <dev/ic/gemvar.h>

struct gem_sbus_softc {
	struct	gem_softc	gsc_gem;	/* GEM device */
	struct sbusdev		gsc_sd;
	void			*gsc_ih;
	bus_space_handle_t	gsc_sbus_regs_h;
};

int	gemmatch_sbus(struct device *, struct cfdata *, void *);
void	gemattach_sbus(struct device *, struct device *, void *);

CFATTACH_DECL(gem_sbus, sizeof(struct gem_sbus_softc),
    gemmatch_sbus, gemattach_sbus, NULL, NULL);

int
gemmatch_sbus(struct device *parent, struct cfdata *cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	return (strcmp("network", sa->sa_name) == 0);
}

void
gemattach_sbus(struct device *parent, struct device *self, void *aux)
{
	struct sbus_attach_args *sa = aux;
	struct gem_sbus_softc *gsc = (void *)self;
	struct gem_softc *sc = &gsc->gsc_gem;
	uint8_t enaddr[ETHER_ADDR_LEN];

	/* Pass on the bus tags */
	sc->sc_bustag = sa->sa_bustag;
	sc->sc_dmatag = sa->sa_dmatag;

	printf(": GEM Ethernet controller (%s), version %s\n",
	    sa->sa_name, prom_getpropstring(sa->sa_node, "version"));

	if (sa->sa_nreg < 2) {
		printf("%s: only %d register sets\n",
			self->dv_xname, sa->sa_nreg);
		return;
	}

	/*
	 * Map two register banks:
	 *
	 *	bank 0: status, config, reset
	 *	bank 1: various gem parts
	 *
	 */
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_reg[0].oa_space,
			 sa->sa_reg[0].oa_base,
			 (bus_size_t)sa->sa_reg[0].oa_size,
			 0, &sc->sc_h2) != 0) {
		printf("%s: cannot map registers\n", self->dv_xname);
		return;
	}
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_reg[1].oa_space,
			 sa->sa_reg[1].oa_base,
			 (bus_size_t)sa->sa_reg[1].oa_size,
			 0, &sc->sc_h1) != 0) {
		printf("%s: cannot map registers\n", self->dv_xname);
		return;
	}
	sbus_establish(&gsc->gsc_sd, self);
	prom_getether(sa->sa_node, enaddr);

	/*
	 * SBUS config
	 */
	bus_space_write_4(sa->sa_bustag, sc->sc_h2, GEM_SBUS_CONFIG,
	    GEM_SBUS_CFG_PARITY|GEM_SBUS_CFG_BMODE64);

	gem_attach(sc, enaddr);

	/* Establish interrupt handler */
	if (sa->sa_nintr != 0)
		gsc->gsc_ih = bus_intr_establish(sa->sa_bustag, sa->sa_pri, IPL_NET,
					gem_intr, sc);
}
