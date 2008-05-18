/* $NetBSD: if_atw_cardbus.c,v 1.21.2.1 2008/05/18 12:33:34 yamt Exp $ */

/*-
 * Copyright (c) 1999, 2000, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.  This code was adapted for the ADMtek ADM8211
 * by David Young.
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
 * CardBus bus front-end for the ADMtek ADM8211 802.11 MAC/BBP driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_atw_cardbus.c,v 1.21.2.1 2008/05/18 12:33:34 yamt Exp $");

#include "opt_inet.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_var.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif


#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/atwreg.h>
#include <dev/ic/rf3000reg.h>
#include <dev/ic/si4136reg.h>
#include <dev/ic/atwvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

/*
 * PCI configuration space registers used by the ADM8211.
 */
#define	ATW_PCI_IOBA		0x10	/* i/o mapped base */
#define	ATW_PCI_MMBA		0x14	/* memory mapped base */

struct atw_cardbus_softc {
	struct atw_softc sc_atw;	/* real ADM8211 softc */

	/* CardBus-specific goo. */
	void	*sc_ih;			/* interrupt handle */
	cardbus_devfunc_t sc_ct;	/* our CardBus devfuncs */
	cardbustag_t sc_tag;		/* our CardBus tag */
	int	sc_csr;			/* CSR bits */
	bus_size_t sc_mapsize;		/* the size of mapped bus space
					   region */

	int	sc_cben;		/* CardBus enables */
	int	sc_bar_reg;		/* which BAR to use */
	pcireg_t sc_bar_val;		/* value of the BAR */

	int	sc_intrline;		/* interrupt line */
};

int	atw_cardbus_match(struct device *, struct cfdata *, void *);
void	atw_cardbus_attach(struct device *, struct device *, void *);
int	atw_cardbus_detach(struct device *, int);

CFATTACH_DECL(atw_cardbus, sizeof(struct atw_cardbus_softc),
    atw_cardbus_match, atw_cardbus_attach, atw_cardbus_detach, atw_activate);

void	atw_cardbus_setup(struct atw_cardbus_softc *);

int	atw_cardbus_enable(struct atw_softc *);
void	atw_cardbus_disable(struct atw_softc *);

static void atw_cardbus_intr_ack(struct atw_softc *);

const struct atw_cardbus_product *atw_cardbus_lookup
   (const struct cardbus_attach_args *);

const struct atw_cardbus_product {
	u_int32_t	 acp_vendor;	/* PCI vendor ID */
	u_int32_t	 acp_product;	/* PCI product ID */
	const char	*acp_product_name;
} atw_cardbus_products[] = {
	{ PCI_VENDOR_ADMTEK,		PCI_PRODUCT_ADMTEK_ADM8211,
	  "ADMtek ADM8211 802.11 MAC/BBP" },

	{ 0,				0,	NULL },
};

const struct atw_cardbus_product *
atw_cardbus_lookup(const struct cardbus_attach_args *ca)
{
	const struct atw_cardbus_product *acp;

	for (acp = atw_cardbus_products;
	     acp->acp_product_name != NULL;
	     acp++) {
		if (PCI_VENDOR(ca->ca_id) == acp->acp_vendor &&
		    PCI_PRODUCT(ca->ca_id) == acp->acp_product)
			return (acp);
	}
	return (NULL);
}

int
atw_cardbus_match(struct device *parent, struct cfdata *match,
    void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (atw_cardbus_lookup(ca) != NULL)
		return (1);

	return (0);
}

void
atw_cardbus_attach(struct device *parent, struct device *self,
    void *aux)
{
	struct atw_cardbus_softc *csc = device_private(self);
	struct atw_softc *sc = &csc->sc_atw;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	const struct atw_cardbus_product *acp;
	bus_addr_t adr;

	sc->sc_dmat = ca->ca_dmat;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;

	acp = atw_cardbus_lookup(ca);
	if (acp == NULL) {
		printf("\n");
		panic("atw_cardbus_attach: impossible");
	}

	/*
	 * Power management hooks.
	 */
	sc->sc_enable = atw_cardbus_enable;
	sc->sc_disable = atw_cardbus_disable;

	sc->sc_intr_ack = atw_cardbus_intr_ack;

	/* Get revision info. */
	sc->sc_rev = PCI_REVISION(ca->ca_class);

	printf(": %s, revision %d.%d\n", acp->acp_product_name,
	    (sc->sc_rev >> 4) & 0xf, sc->sc_rev & 0xf);

#if 0
	printf("%s: signature %08x\n", device_xname(&sc->sc_dev),
	    (rev >> 4) & 0xf, rev & 0xf,
	    cardbus_conf_read(ct->ct_cc, ct->ct_cf, csc->sc_tag, 0x80));
#endif

	/*
	 * Map the device.
	 */
	csc->sc_csr = CARDBUS_COMMAND_MASTER_ENABLE;
	if (Cardbus_mapreg_map(ct, ATW_PCI_MMBA,
	    CARDBUS_MAPREG_TYPE_MEM, 0, &sc->sc_st, &sc->sc_sh, &adr,
	    &csc->sc_mapsize) == 0) {
#if 0
		printf("%s: atw_cardbus_attach mapped %d bytes mem space\n",
		    device_xname(&sc->sc_dev), csc->sc_mapsize);
#endif
#if rbus
#else
		(*ct->ct_cf->cardbus_mem_open)(cc, 0, adr, adr+csc->sc_mapsize);
#endif
		csc->sc_cben = CARDBUS_MEM_ENABLE;
		csc->sc_csr |= CARDBUS_COMMAND_MEM_ENABLE;
		csc->sc_bar_reg = ATW_PCI_MMBA;
		csc->sc_bar_val = adr | CARDBUS_MAPREG_TYPE_MEM;
	} else if (Cardbus_mapreg_map(ct, ATW_PCI_IOBA,
	    CARDBUS_MAPREG_TYPE_IO, 0, &sc->sc_st, &sc->sc_sh, &adr,
	    &csc->sc_mapsize) == 0) {
#if 0
		printf("%s: atw_cardbus_attach mapped %d bytes I/O space\n",
		    device_xname(&sc->sc_dev), csc->sc_mapsize);
#endif
#if rbus
#else
		(*ct->ct_cf->cardbus_io_open)(cc, 0, adr, adr+csc->sc_mapsize);
#endif
		csc->sc_cben = CARDBUS_IO_ENABLE;
		csc->sc_csr |= CARDBUS_COMMAND_IO_ENABLE;
		csc->sc_bar_reg = ATW_PCI_IOBA;
		csc->sc_bar_val = adr | CARDBUS_MAPREG_TYPE_IO;
	} else {
		aprint_error_dev(&sc->sc_dev, "unable to map device registers\n");
		return;
	}

	/*
	 * Bring the chip out of powersave mode and initialize the
	 * configuration registers.
	 */
	atw_cardbus_setup(csc);

	/* Remember which interrupt line. */
	csc->sc_intrline = ca->ca_intrline;

	printf("%s: interrupting at %d\n", device_xname(&sc->sc_dev),
	    csc->sc_intrline);
#if 0
	/*
	 * The CardBus cards will make it to store-and-forward mode as
	 * soon as you put them under any kind of load, so just start
	 * out there.
	 */
	sc->sc_txthresh = 3; /* TBD name constant */
#endif

	/*
	 * Finish off the attach.
	 */
	atw_attach(sc);

	ATW_WRITE(sc, ATW_FER, ATW_FER_INTR);

	/*
	 * Power down the socket.
	 */
	Cardbus_function_disable(csc->sc_ct);
}

static void
atw_cardbus_intr_ack(struct atw_softc *sc)
{
	ATW_WRITE(sc, ATW_FER, ATW_FER_INTR);
}

int
atw_cardbus_detach(struct device *self, int flags)
{
	struct atw_cardbus_softc *csc = device_private(self);
	struct atw_softc *sc = &csc->sc_atw;
	struct cardbus_devfunc *ct = csc->sc_ct;
	int rv;

#if defined(DIAGNOSTIC)
	if (ct == NULL)
		panic("%s: data structure lacks", device_xname(&sc->sc_dev));
#endif

	rv = atw_detach(sc);
	if (rv)
		return (rv);

	/*
	 * Unhook the interrupt handler.
	 */
	if (csc->sc_ih != NULL)
		cardbus_intr_disestablish(ct->ct_cc, ct->ct_cf, csc->sc_ih);

	/*
	 * Release bus space and close window.
	 */
	if (csc->sc_bar_reg != 0)
		Cardbus_mapreg_unmap(ct, csc->sc_bar_reg,
		    sc->sc_st, sc->sc_sh, csc->sc_mapsize);

	return (0);
}

int
atw_cardbus_enable(struct atw_softc *sc)
{
	struct atw_cardbus_softc *csc = (void *) sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/*
	 * Power on the socket.
	 */
	Cardbus_function_enable(ct);

	/*
	 * Set up the PCI configuration registers.
	 */
	atw_cardbus_setup(csc);

	/*
	 * Map and establish the interrupt.
	 */
	csc->sc_ih = cardbus_intr_establish(cc, cf, csc->sc_intrline, IPL_NET,
	    atw_intr, sc);
	if (csc->sc_ih == NULL) {
		aprint_error_dev(&sc->sc_dev, "unable to establish interrupt at %d\n",
		    csc->sc_intrline);
		Cardbus_function_disable(csc->sc_ct);
		return (1);
	}

	return (0);
}

void
atw_cardbus_disable(struct atw_softc *sc)
{
	struct atw_cardbus_softc *csc = (void *) sc;
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	/* Unhook the interrupt handler. */
	cardbus_intr_disestablish(cc, cf, csc->sc_ih);
	csc->sc_ih = NULL;

	/* Power down the socket. */
	Cardbus_function_disable(ct);
}

void
atw_cardbus_setup(struct atw_cardbus_softc *csc)
{
	cardbus_devfunc_t ct = csc->sc_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	pcireg_t reg;

	(void)cardbus_set_powerstate(ct, csc->sc_tag, PCI_PWR_D0);

	/* Program the BAR. */
	cardbus_conf_write(cc, cf, csc->sc_tag, csc->sc_bar_reg,
	    csc->sc_bar_val);

	/* Make sure the right access type is on the CardBus bridge. */
	(*ct->ct_cf->cardbus_ctrl)(cc, csc->sc_cben);
	(*ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);

	/* Enable the appropriate bits in the PCI CSR. */
	reg = cardbus_conf_read(cc, cf, csc->sc_tag,
	    CARDBUS_COMMAND_STATUS_REG);
	reg &= ~(CARDBUS_COMMAND_IO_ENABLE|CARDBUS_COMMAND_MEM_ENABLE);
	reg |= csc->sc_csr;
	cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_COMMAND_STATUS_REG,
	    reg);

	/*
	 * Make sure the latency timer is set to some reasonable
	 * value.
	 */
	reg = cardbus_conf_read(cc, cf, csc->sc_tag, CARDBUS_BHLC_REG);
	if (CARDBUS_LATTIMER(reg) < 0x20) {
		reg &= ~(CARDBUS_LATTIMER_MASK << CARDBUS_LATTIMER_SHIFT);
		reg |= (0x20 << CARDBUS_LATTIMER_SHIFT);
		cardbus_conf_write(cc, cf, csc->sc_tag, CARDBUS_BHLC_REG, reg);
	}
}
