/*	$NetBSD: if_ral_cardbus.c,v 1.2.8.2 2005/11/10 14:03:54 skrll Exp $ */
/*	$OpenBSD: if_ral_cardbus.c,v 1.5 2005/05/16 01:36:25 brad Exp $  */

/*-
 * Copyright (c) 2005
 *	Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * CardBus front-end for the Ralink RT2500 driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ral_cardbus.c,v 1.2.8.2 2005/11/10 14:03:54 skrll Exp $");

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <netinet/in.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_rssadapt.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/ralvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>

struct ral_cardbus_softc {
	struct ral_softc	sc_sc;

	/* cardbus specific goo */
	cardbus_devfunc_t	sc_ct;
	cardbustag_t		sc_tag;
	void			*sc_ih;
	bus_size_t		sc_mapsize;
	pcireg_t		sc_bar_val;
	int			sc_intrline;
};

int	ral_cardbus_match(struct device *, struct cfdata *, void *);
void	ral_cardbus_attach(struct device *, struct device *, void *);
int	ral_cardbus_detach(struct device *, int);

CFATTACH_DECL(ral_cardbus, sizeof (struct ral_cardbus_softc),
    ral_cardbus_match, ral_cardbus_attach, ral_cardbus_detach, NULL);

int	ral_cardbus_enable(struct ral_softc *);
void	ral_cardbus_disable(struct ral_softc *);
void	ral_cardbus_power(struct ral_softc *, int);
void	ral_cardbus_setup(struct ral_cardbus_softc *);

int
ral_cardbus_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (CARDBUS_VENDOR(ca->ca_id) == PCI_VENDOR_RALINK &&
	    CARDBUS_PRODUCT(ca->ca_id) == PCI_PRODUCT_RALINK_RT2560)
		return 1;

	return 0;
}

void
ral_cardbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct ral_cardbus_softc *csc = (struct ral_cardbus_softc *)self;
	struct ral_softc *sc = &csc->sc_sc;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	bus_addr_t base;
	int error;

	sc->sc_dmat = ca->ca_dmat;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;
	csc->sc_intrline = ca->ca_intrline;

	/* power management hooks */
	sc->sc_enable = ral_cardbus_enable;
	sc->sc_disable = ral_cardbus_disable;
	sc->sc_power = ral_cardbus_power;

	/* map control/status registers */
	error = Cardbus_mapreg_map(ct, CARDBUS_BASE0_REG,
	    CARDBUS_MAPREG_TYPE_MEM, 0, &sc->sc_st, &sc->sc_sh, &base,
	    &csc->sc_mapsize);
	if (error != 0) {
		printf(": could not map memory space\n");
		return;
	}

#if rbus
#else
	(*cf->cardbus_mem_open)(cc, 0, base, base + csc->sc_mapsize);
#endif

	csc->sc_bar_val = base | CARDBUS_MAPREG_TYPE_MEM;

	/* set up the PCI configuration registers */
	ral_cardbus_setup(csc);

	printf(": irq %d\n", csc->sc_intrline);

	ral_attach(sc);

	Cardbus_function_disable(ct);
}

int
ral_cardbus_detach(struct device *self, int flags)
{
	struct ral_cardbus_softc *csc = (struct ral_cardbus_softc *)self;
	struct ral_softc *sc = &csc->sc_sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	int error;

	error = ral_detach(sc);
	if (error != 0)
		return error;

	/* unhook the interrupt handler */
	if (csc->sc_ih != NULL) {
		cardbus_intr_disestablish(cc, cf, csc->sc_ih);
		csc->sc_ih = NULL;
	}

	/* release bus space and close window */
	Cardbus_mapreg_unmap(ct, CARDBUS_BASE0_REG, sc->sc_st, sc->sc_sh,
	    csc->sc_mapsize);

	return 0;
}

int
ral_cardbus_enable(struct ral_softc *sc)
{
	struct ral_cardbus_softc *csc = (struct ral_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/* power on the socket */
	Cardbus_function_enable(ct);

	/* setup the PCI configuration registers */
	ral_cardbus_setup(csc);

	/* map and establish the interrupt handler */
	csc->sc_ih = cardbus_intr_establish(cc, cf, csc->sc_intrline, IPL_NET,
	    ral_intr, sc);
	if (csc->sc_ih == NULL) {
		printf("%s: could not establish interrupt at %d\n",
		    sc->sc_dev.dv_xname, csc->sc_intrline);
		Cardbus_function_disable(ct);
		return 1;
	}

	return 0;
}

void
ral_cardbus_disable(struct ral_softc *sc)
{
	struct ral_cardbus_softc *csc = (struct ral_cardbus_softc *)sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/* unhook the interrupt handler */
	cardbus_intr_disestablish(cc, cf, csc->sc_ih);
	csc->sc_ih = NULL;

	/* power down the socket */
	Cardbus_function_disable(ct);
}

void
ral_cardbus_power(struct ral_softc *sc, int why)
{
	struct ral_cardbus_softc *csc = (struct ral_cardbus_softc *)sc;

	if (why == PWR_RESUME) {
		/* kick the PCI configuration registers */
		ral_cardbus_setup(csc);
	}
}

void
ral_cardbus_setup(struct ral_cardbus_softc *csc)
{
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	pcireg_t reg;

	/* program the BAR */
	cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_BASE0_REG,
	    csc->sc_bar_val);

	/* make sure the right access type is on the cardbus bridge */
	(*cf->cardbus_ctrl)(cc, CARDBUS_MEM_ENABLE);
	(*cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);

	/* enable the appropriate bits in the PCI CSR */
	reg = cardbus_conf_read(cc, cf, csc->sc_tag,
	    CARDBUS_COMMAND_STATUS_REG);
	reg |= CARDBUS_COMMAND_MASTER_ENABLE | CARDBUS_COMMAND_MEM_ENABLE;
	cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_COMMAND_STATUS_REG,
	    reg);
}
