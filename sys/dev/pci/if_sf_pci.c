/*	$NetBSD: if_sf_pci.c,v 1.1.2.6 2002/12/29 20:49:23 thorpej Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * PCI bus front-end for the Adaptec AIC-6915 (``Starfire'')
 * 10/100 Ethernet controller.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_sf_pci.c,v 1.1.2.6 2002/12/29 20:49:23 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/mii/miivar.h>

#include <dev/ic/aic6915reg.h>
#include <dev/ic/aic6915var.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

struct sf_pci_softc {
	struct sf_softc sc_starfire;	/* read Starfire softc */

	/* PCI-specific goo. */
	void	*sc_ih;			/* interrupt handle */
};

int	sf_pci_match(struct device *, struct cfdata *, void *);
void	sf_pci_attach(struct device *, struct device *, void *);

CFATTACH_DECL(sf_pci, sizeof(struct sf_pci_softc),
    sf_pci_match, sf_pci_attach, NULL, NULL);

struct sf_pci_product {
	uint32_t	spp_vendor;	/* PCI vendor ID */
	uint32_t	spp_product;	/* PCI product ID */
	const char	*spp_name;	/* product name */
	const struct sf_pci_product *spp_subsys; /* subsystm IDs */
};

const struct sf_pci_product sf_subsys_adaptec[] = {
	/* ANA-62011 (rev 0) Single port 10/100 64-bit */
	{ PCI_VENDOR_ADP,			0x0008,
	  "ANA-62011 (rev 0) 10/100 Ethernet",	NULL },

	/* ANA-62011 (rev 1) Single port 10/100 64-bit */
	{ PCI_VENDOR_ADP,			0x0009,
	  "ANA-62011 (rev 1) 10/100 Ethernet",	NULL },

	/* ANA-62022 Dual port 10/100 64-bit */
	{ PCI_VENDOR_ADP,			0x0010,
	  "ANA-62022 10/100 Ethernet",		NULL },

	/* ANA-62044 (rev 0) Quad port 10/100 64-bit */
	{ PCI_VENDOR_ADP,			0x0018,
	  "ANA-62044 (rev 0) 10/100 Ethernet",	NULL },

	/* ANA-62044 (rev 1) Quad port 10/100 64-bit */
	{ PCI_VENDOR_ADP,			0x0019,
	  "ANA-62044 (rev 1) 10/100 Ethernet",	NULL },

	/* ANA-62020 Single port 100baseFX 64-bit */
	{ PCI_VENDOR_ADP,			0x0020,
	  "ANA-62020 100baseFX Ethernet",	NULL },

	/* ANA-69011 Single port 10/100 32-bit */
	{ PCI_VENDOR_ADP,			0x0028,
	  "ANA-69011 10/100 Ethernet",		NULL },

	{ 0, 					0,
	  NULL,					NULL },
};

const struct sf_pci_product sf_pci_products[] = {
	{ PCI_VENDOR_ADP,			PCI_PRODUCT_ADP_AIC6915,
	  "AIC-6915 10/100 Ethernet",		sf_subsys_adaptec },

	{ 0,					0,
	  NULL,					NULL },
};

static const struct sf_pci_product *
sf_pci_lookup(const struct pci_attach_args *pa)
{
	const struct sf_pci_product *spp, *subspp;
	pcireg_t subsysid;

	for (spp = sf_pci_products; spp->spp_name != NULL; spp++) {
		if (PCI_VENDOR(pa->pa_id) == spp->spp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == spp->spp_product) {
			subsysid = pci_conf_read(pa->pa_pc, pa->pa_tag,
			    PCI_SUBSYS_ID_REG);
			for (subspp = spp->spp_subsys;
			     subspp->spp_name != NULL; subspp++) {
				if (PCI_VENDOR(subsysid) ==
					subspp->spp_vendor ||
				    PCI_PRODUCT(subsysid) ==
					subspp->spp_product) {
					return (subspp);
				}
			}
			return (spp);
		}
	}

	return (NULL);
}

int
sf_pci_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (sf_pci_lookup(pa) != NULL)
		return (1);

	return (0);
}

void
sf_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct sf_pci_softc *psc = (void *) self;
	struct sf_softc *sc = &psc->sc_starfire;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	const struct sf_pci_product *spp;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	pcireg_t reg;
	int pmreg, ioh_valid, memh_valid;

	spp = sf_pci_lookup(pa);
	if (spp == NULL) {
		printf("\n");
		panic("sf_pci_attach: impossible");
	}

	printf(": %s, rev. %d\n", spp->spp_name, PCI_REVISION(pa->pa_class));

	if (pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_PWRMGMT,
	    &pmreg, 0)) {
		reg = pci_conf_read(pa->pa_pc, pa->pa_tag, pmreg + PCI_PMCSR);
		switch (reg & PCI_PMCSR_STATE_MASK) {
		case PCI_PMCSR_STATE_D1:
		case PCI_PMCSR_STATE_D2:
			printf(": waking up from power state D%d\n%s",
			    reg & PCI_PMCSR_STATE_MASK, sc->sc_dev.dv_xname);
			pci_conf_write(pa->pa_pc, pa->pa_tag, pmreg + PCI_PMCSR,
			    (reg & ~PCI_PMCSR_STATE_MASK) |
			    PCI_PMCSR_STATE_D0);
			break;

		case PCI_PMCSR_STATE_D3:
			printf("%s: unable to wake up from power state D3\n",
			    sc->sc_dev.dv_xname);
			pci_conf_write(pa->pa_pc, pa->pa_tag, pmreg + PCI_PMCSR,
			    (reg & ~PCI_PMCSR_STATE_MASK) |
			    PCI_PMCSR_STATE_D0);
			return;
		}
	}

	/*
	 * Map the device.
	 */
	reg = pci_mapreg_type(pa->pa_pc, pa->pa_tag, SF_PCI_MEMBA);
	switch (reg) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		memh_valid = (pci_mapreg_map(pa, SF_PCI_MEMBA,
		    reg, 0, &memt, &memh, NULL, NULL) == 0);
		break;
	default:
		memh_valid = 0;
	}

	ioh_valid = (pci_mapreg_map(pa,
	    (reg == (PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT)) ?
		SF_PCI_IOBA : SF_PCI_IOBA - 0x04,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL) == 0);

	if (memh_valid) {
		sc->sc_st = memt;
		sc->sc_sh = memh;
		sc->sc_iomapped = 0;
	} else if (ioh_valid) {
		sc->sc_st = iot;
		sc->sc_sh = ioh;
		sc->sc_iomapped = 1;
	} else {
		printf("%s: unable to map device registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	/* Make sure bus mastering is enabled. */
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		printf("%s: unable to map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	psc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_NET, sf_intr, sc);
	if (psc->sc_ih == NULL) {
		printf("%s: unable to establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	/*
	 * Finish off the attach.
	 */
	sf_attach(sc);
}
