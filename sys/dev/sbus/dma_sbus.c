/*	$NetBSD: dma_sbus.c,v 1.5.2.4 2002/02/28 04:14:20 nathanw Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
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
 *	This product includes software developed by Peter Galbavy.
 * 4. The name of the author may not be used to endorse or promote products
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
__KERNEL_RCSID(0, "$NetBSD: dma_sbus.c,v 1.5.2.4 2002/02/28 04:14:20 nathanw Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>

#include <dev/sbus/sbusvar.h>

#include <dev/ic/lsi64854reg.h>
#include <dev/ic/lsi64854var.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

struct dma_softc {
	struct lsi64854_softc	sc_lsi64854;	/* base device */
	struct sbusdev	sc_sd;			/* sbus device */
};

int	dmamatch_sbus	__P((struct device *, struct cfdata *, void *));
void	dmaattach_sbus	__P((struct device *, struct device *, void *));

int	dmaprint_sbus	__P((void *, const char *));

void	*dmabus_intr_establish __P((
		bus_space_tag_t,
		int,			/*bus interrupt priority*/
		int,			/*`device class' level*/
		int,			/*flags*/
		int (*) __P((void *)),	/*handler*/
		void *));		/*handler arg*/

static	bus_space_tag_t dma_alloc_bustag __P((struct dma_softc *sc));

struct cfattach dma_sbus_ca = {
	sizeof(struct dma_softc), dmamatch_sbus, dmaattach_sbus
};

struct cfattach ledma_ca = {
	sizeof(struct dma_softc), dmamatch_sbus, dmaattach_sbus
};

int
dmaprint_sbus(aux, busname)
	void *aux;
	const char *busname;
{
	struct sbus_attach_args *sa = aux;
	bus_space_tag_t t = sa->sa_bustag;
	struct dma_softc *sc = t->cookie;

	sa->sa_bustag = sc->sc_lsi64854.sc_bustag;	/* XXX */
	sbus_print(aux, busname);	/* XXX */
	sa->sa_bustag = t;		/* XXX */
	return (UNCONF);
}

int
dmamatch_sbus(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct sbus_attach_args *sa = aux;

	return (strcmp(cf->cf_driver->cd_name, sa->sa_name) == 0 ||
		strcmp("espdma", sa->sa_name) == 0);
}

void
dmaattach_sbus(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbus_attach_args *sa = aux;
	struct dma_softc *dsc = (void *)self;
	struct lsi64854_softc *sc = &dsc->sc_lsi64854;
	bus_space_handle_t bh;
	bus_space_tag_t sbt;
	int sbusburst, burst;
	int node;

	node = sa->sa_node;

	sc->sc_bustag = sa->sa_bustag;
	sc->sc_dmatag = sa->sa_dmatag;

	/* Map registers */
	if (sa->sa_npromvaddrs != 0)
		sc->sc_regs = (bus_space_handle_t)sa->sa_promvaddrs[0];
	else {
		if (sbus_bus_map(sa->sa_bustag, sa->sa_slot,
				 sa->sa_offset,
				 sa->sa_size,
				 0, 0, &bh) != 0) {
			printf("%s: cannot map registers\n", self->dv_xname);
			return;
		}
		sc->sc_regs = bh;
	}

	/*
	 * Get transfer burst size from PROM and plug it into the
	 * controller registers. This is needed on the Sun4m; do
	 * others need it too?
	 */
	sbusburst = ((struct sbus_softc *)parent)->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1; /* 1->16 */

	burst = PROM_getpropint(node,"burst-sizes", -1);
	if (burst == -1)
		/* take SBus burst sizes */
		burst = sbusburst;

	/* Clamp at parent's burst sizes */
	burst &= sbusburst;
	sc->sc_burst = (burst & SBUS_BURST_32) ? 32 :
		       (burst & SBUS_BURST_16) ? 16 : 0;

	if (sc->sc_dev.dv_cfdata->cf_attach == &ledma_ca) {
		char *cabletype;
		u_int32_t csr;
		/*
		 * Check to see which cable type is currently active and
		 * set the appropriate bit in the ledma csr so that it
		 * gets used. If we didn't netboot, the PROM won't have
		 * the "cable-selection" property; default to TP and then
		 * the user can change it via a "media" option to ifconfig.
		 */
		cabletype = PROM_getpropstring(node, "cable-selection");
		csr = L64854_GCSR(sc);
		if (strcmp(cabletype, "tpe") == 0) {
			csr |= E_TP_AUI;
		} else if (strcmp(cabletype, "aui") == 0) {
			csr &= ~E_TP_AUI;
		} else {
			/* assume TP if nothing there */
			csr |= E_TP_AUI;
		}
		L64854_SCSR(sc, csr);
		delay(20000);	/* manual says we need a 20ms delay */
		sc->sc_channel = L64854_CHANNEL_ENET;
	} else {
		sc->sc_channel = L64854_CHANNEL_SCSI;
	}

	sbus_establish(&dsc->sc_sd, &sc->sc_dev);
	sbt = dma_alloc_bustag(dsc);
	lsi64854_attach(sc);

	/* Attach children */
	for (node = firstchild(sa->sa_node); node; node = nextsibling(node)) {
		struct sbus_attach_args sa;
		sbus_setup_attach_args((struct sbus_softc *)parent,
				       sbt, sc->sc_dmatag, node, &sa);
		(void) config_found(&sc->sc_dev, (void *)&sa, dmaprint_sbus);
		sbus_destroy_attach_args(&sa);
	}
}

void *
dmabus_intr_establish(t, pri, level, flags, handler, arg)
	bus_space_tag_t t;
	int pri;
	int level;
	int flags;
	int (*handler) __P((void *));
	void *arg;
{
	struct lsi64854_softc *sc = t->cookie;

	/* XXX - for now only le; do esp later */
	if (sc->sc_channel == L64854_CHANNEL_ENET) {
		sc->sc_intrchain = handler;
		sc->sc_intrchainarg = arg;
		handler = lsi64854_enet_intr;
		arg = sc;
	}
	return (bus_intr_establish(sc->sc_bustag, pri, level, flags,
				   handler, arg));
}

bus_space_tag_t
dma_alloc_bustag(sc)
	struct dma_softc *sc;
{
	bus_space_tag_t sbt;

	sbt = (bus_space_tag_t) malloc(sizeof(struct sparc_bus_space_tag),
	    M_DEVBUF, M_NOWAIT|M_ZERO);
	if (sbt == NULL)
		return (NULL);

	sbt->cookie = sc;
	sbt->parent = sc->sc_lsi64854.sc_bustag;
	sbt->sparc_intr_establish = dmabus_intr_establish;
	return (sbt);
}
