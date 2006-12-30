/* $NetBSD: isic_pci.c,v 1.24.2.1 2006/12/30 20:48:46 yamt Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@NetBSD.org>.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isic_pci.c,v 1.24.2.1 2006/12/30 20:48:46 yamt Exp $");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/callout.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>


#include <machine/bus.h>
#include <machine/intr.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_global.h>
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_trace.h>
#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>

#include <dev/ic/isic_l1.h>
#include <dev/ic/ipac.h>
#include <dev/ic/isac.h>
#include <dev/ic/hscx.h>
#include <dev/pci/isic_pci.h>

extern const struct isdn_layer1_isdnif_driver isic_std_driver;

static int isic_pci_match(struct device *, struct cfdata *, void *);
static void isic_pci_attach(struct device *, struct device *, void *);
static const struct isic_pci_product * find_matching_card(struct pci_attach_args *pa);

static void isic_pci_isdn_attach(struct pci_isic_softc *psc, struct pci_attach_args *pa, const char *cardname);
static int isic_pci_detach(struct device *self, int flags);
static int isic_pci_activate(struct device *self, enum devact act);

CFATTACH_DECL(isic_pci, sizeof(struct pci_isic_softc),
    isic_pci_match, isic_pci_attach, isic_pci_detach, isic_pci_activate);

static const struct isic_pci_product {
	pci_vendor_id_t npp_vendor;
	pci_product_id_t npp_product;
	int cardtype;
	const char * name;
	int (*attach)(struct pci_isic_softc *psc, struct pci_attach_args *pa);
	void (*pciattach)(struct pci_isic_softc *psc, struct pci_attach_args *pa, const char *cardname);
} isic_pci_products[] = {
	{ PCI_VENDOR_ELSA, PCI_PRODUCT_ELSA_QS1PCI,
	  CARD_TYPEP_ELSAQS1PCI,
	  "ELSA QuickStep 1000pro/PCI",
	  isic_attach_Eqs1pp,	/* card specific initialization */
	  isic_pci_isdn_attach	/* generic setup for ISAC/HSCX or IPAC boards */
	 },
	{ 0, 0, 0, NULL, NULL, NULL },
};

static const struct isic_pci_product * find_matching_card(pa)
	struct pci_attach_args *pa;
{
	const struct isic_pci_product * pp = NULL;

	for (pp = isic_pci_products; pp->npp_vendor; pp++)
		if (PCI_VENDOR(pa->pa_id) == pp->npp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == pp->npp_product)
			return pp;

	return NULL;
}

/*
 * Match card
 */
static int
isic_pci_match(struct device *parent,
	struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (!find_matching_card(pa))
		return 0;

	return 1;
}

/*
 * Attach the card
 */
static void
isic_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_isic_softc *psc = (void*) self;
	struct isic_softc *sc = &psc->sc_isic;
	struct pci_attach_args *pa = aux;
	const struct isic_pci_product * prod;

	/* Redo probe */
	prod = find_matching_card(pa);
	if (prod == NULL) return; /* oops - not found?!? */

	printf(": %s\n", prod->name);

	callout_init(&sc->sc_T3_callout);
	callout_init(&sc->sc_T4_callout);

	/* card initilization and sc setup */
	if (!prod->attach(psc, pa))
		return;

	/* generic setup, if needed for this card */
	if (prod->pciattach) prod->pciattach(psc, pa, prod->name);
}

/*---------------------------------------------------------------------------*
 *	isic - pci device driver attach routine
 *---------------------------------------------------------------------------*/
static void
isic_pci_isdn_attach(psc, pa, cardname)
	struct pci_isic_softc *psc;
	struct pci_attach_args *pa;
	const char *cardname;
{
	struct isic_softc *sc = &psc->sc_isic;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr;

  	static const char *ISACversion[] = {
  		"2085 Version A1/A2 or 2086/2186 Version 1.1",
		"2085 Version B1",
		"2085 Version B2",
		"2085 Version V2.3 (B3)",
		"Unknown Version"
	};

	static const char *HSCXversion[] = {
		"82525 Version A1",
		"Unknown (0x01)",
		"82525 Version A2",
		"Unknown (0x03)",
		"82525 Version A3",
		"82525 or 21525 Version 2.1",
		"Unknown Version"
	};

	sc->sc_isac_version = 0;
	sc->sc_hscx_version = 0;

	if(sc->sc_ipac)
	{
		u_int ret = IPAC_READ(IPAC_ID);

		switch(ret)
		{
			case 0x01:
				printf("%s: IPAC PSB2115 Version 1.1\n", sc->sc_dev.dv_xname);
				break;

			case 0x02:
				printf("%s: IPAC PSB2115 Version 1.2\n", sc->sc_dev.dv_xname);
				break;

			default:
				printf("%s: Error, IPAC version %d unknown!\n",
					sc->sc_dev.dv_xname, ret);
				return;
		}
	}
	else
	{
		sc->sc_isac_version = ((ISAC_READ(I_RBCH)) >> 5) & 0x03;

		switch(sc->sc_isac_version)
		{
			case ISAC_VA:
			case ISAC_VB1:
	                case ISAC_VB2:
			case ISAC_VB3:
				printf("%s: ISAC %s (IOM-%c)\n",
					sc->sc_dev.dv_xname,
					ISACversion[sc->sc_isac_version],
					sc->sc_bustyp == BUS_TYPE_IOM1 ? '1' : '2');
				break;

			default:
				printf("%s: Error, ISAC version %d unknown!\n",
					sc->sc_dev.dv_xname, sc->sc_isac_version);
				return;
		}

		sc->sc_hscx_version = HSCX_READ(0, H_VSTR) & 0xf;

		switch(sc->sc_hscx_version)
		{
			case HSCX_VA1:
			case HSCX_VA2:
			case HSCX_VA3:
			case HSCX_V21:
				printf("%s: HSCX %s\n",
					sc->sc_dev.dv_xname,
					HSCXversion[sc->sc_hscx_version]);
				break;

			default:
				printf("%s: Error, HSCX version %d unknown!\n",
					sc->sc_dev.dv_xname, sc->sc_hscx_version);
				return;
		}
	}

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, isic_intr_qs1p, psc);
	if (psc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	psc->sc_pc = pc;
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	sc->sc_intr_valid = ISIC_INTR_DISABLED;

	/* HSCX setup */

	isic_bchannel_setup(sc, HSCX_CH_A, BPROT_NONE, 0);

	isic_bchannel_setup(sc, HSCX_CH_B, BPROT_NONE, 0);

	/* setup linktab */

	isic_init_linktab(sc);

	/* set trace level */

	sc->sc_trace = TRACE_OFF;

	sc->sc_state = ISAC_IDLE;

	sc->sc_ibuf = NULL;
	sc->sc_ib = NULL;
	sc->sc_ilen = 0;

	sc->sc_obuf = NULL;
	sc->sc_op = NULL;
	sc->sc_ol = 0;
	sc->sc_freeflag = 0;

	sc->sc_obuf2 = NULL;
	sc->sc_freeflag2 = 0;

	/* init higher protocol layers */
	isic_attach_bri(sc, cardname, &isic_std_driver);
}


static int
isic_pci_detach(struct device *self, int flags)
{
	struct pci_isic_softc *psc = (struct pci_isic_softc *)self;

	bus_space_unmap(psc->sc_isic.sc_maps[0].t, psc->sc_isic.sc_maps[0].h, psc->sc_size);
	bus_space_free(psc->sc_isic.sc_maps[0].t, psc->sc_isic.sc_maps[0].h, psc->sc_size);
	pci_intr_disestablish(psc->sc_pc, psc->sc_ih);

	return (0);
}

static int
isic_pci_activate(struct device *self, enum devact act)
{
	struct pci_isic_softc *psc = (struct pci_isic_softc *)self;
	int error = 0, s;

	s = splnet();
	switch (act) {
	case DVACT_ACTIVATE:
		error = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		psc->sc_isic.sc_intr_valid = ISIC_INTR_DYING;
		isic_detach_bri(&psc->sc_isic);
		break;
	}
	splx(s);
	return (error);
}
