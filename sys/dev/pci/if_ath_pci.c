/*	$NetBSD: if_ath_pci.c,v 1.5.2.1 2004/07/02 22:28:31 he Exp $	*/

/*-
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
#ifdef __FreeBSD__
__FBSDID("$FreeBSD: src/sys/dev/ath/if_ath_pci.c,v 1.6 2003/11/28 05:28:27 imp Exp $");
#endif
#ifdef __NetBSD__
__KERNEL_RCSID(0, "$NetBSD: if_ath_pci.c,v 1.5.2.1 2004/07/02 22:28:31 he Exp $");
#endif

/*
 * PCI/Cardbus front-end for the Atheros Wireless LAN controller driver.
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>

#include <machine/bus.h>
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>
#include <net/if_llc.h>
#include <net/if_arp.h>

#include <net80211/ieee80211_compat.h>
#include <net80211/ieee80211_var.h>

#ifdef INET
#include <netinet/in.h> 
#endif

#include <dev/ic/athcompat.h>
#include <dev/ic/athvar.h>
#include <../contrib/sys/dev/ic/athhal.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <sys/device.h>

/*
 * PCI glue.
 */

struct ath_pci_softc {
	struct ath_softc	sc_sc;
#ifdef __FreeBSD__
	struct resource		*sc_sr;		/* memory resource */
	struct resource		*sc_irq;	/* irq resource */
#else
	pci_chipset_tag_t	sc_pc;
#endif
	void			*sc_ih;		/* intererupt handler */
	u_int8_t		sc_saved_intline;
	u_int8_t		sc_saved_cachelinesz;
	u_int8_t		sc_saved_lattimer;
};

#define	BS_BAR	0x10

static int ath_pci_match(struct device *, struct cfdata *, void *);
static void ath_pci_attach(struct device *, struct device *, void *);
static void ath_pci_shutdown(void *);
static int ath_pci_detach(struct device *, int);

CFATTACH_DECL(ath_pci,
    sizeof(struct ath_pci_softc),
    ath_pci_match,
    ath_pci_attach,
    ath_pci_detach,
    NULL);

/*
 * translate some product code.  it is a workaround until HAL gets updated.
 */
static u_int16_t
ath_product(pcireg_t pa_id)
{
	u_int16_t prodid;

	prodid = PCI_PRODUCT(pa_id);
	switch (prodid) {
	case 0x1014:	/* IBM 31P9702 minipci a/b/g card */
		prodid = PCI_PRODUCT_ATHEROS_AR5212;
		break;
	default:
		break;
	}
	return prodid;
}

static int
ath_pci_match(struct device *parent, struct cfdata *match, void *aux)
{
	const char* devname;
	struct pci_attach_args *pa = aux;
	pci_vendor_id_t vendor;

	vendor = PCI_VENDOR(pa->pa_id);
	/* XXX HACK until HAL is updated. */
	if (vendor == 0x128c)
		vendor = PCI_VENDOR_ATHEROS;
	devname = ath_hal_probe(vendor, ath_product(pa->pa_id));
	if (devname) 
		return 1;

	return 0;
}

static void
ath_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct ath_pci_softc *psc = (struct ath_pci_softc *)self;
	struct ath_softc *sc = &psc->sc_sc;
	u_int32_t res;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	pci_intr_handle_t ih;
	void *hook;
	const char *intrstr = NULL;

	psc->sc_pc = pc;

	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	        PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_MEM_ENABLE);
	res = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if ((res & PCI_COMMAND_MEM_ENABLE) == 0) {
		aprint_error("couldn't enable memory mapping\n");
		goto bad;
	}

	if ((res & PCI_COMMAND_MASTER_ENABLE) == 0) {
		aprint_error("couldn't enable bus mastering\n");
		goto bad;
	}

	if (pci_mapreg_map(pa, BS_BAR, PCI_MAPREG_TYPE_MEM, 0, &iot, &ioh, 
	    NULL, NULL)) {
		aprint_error("cannot map register space\n");
		goto bad;
	}
	sc->sc_st = iot;
	sc->sc_sh = ioh;

	sc->sc_invalid = 1;

	if (pci_intr_map(pa, &ih)) {
		aprint_error("couldn't map interrupt\n");
		goto bad1;
	}

	intrstr = pci_intr_string(pc, ih); 
	psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, ath_intr, sc);
	if (psc->sc_ih == NULL) {
		aprint_error("couldn't map interrupt\n");
		goto bad2;
	}

	printf("\n");
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	sc->sc_dmat = pa->pa_dmat;

	hook = shutdownhook_establish(ath_pci_shutdown, psc);
	if (hook == NULL) {
		aprint_error("couldn't make shutdown hook\n");
		goto bad3;
	}

	if (ath_attach(ath_product(pa->pa_id), sc) == 0)
		return;

	shutdownhook_disestablish(hook);

bad3:	pci_intr_disestablish(pc, psc->sc_ih);
bad2:	/* XXX */
bad1:	/* XXX */
bad:
	return;
}

static int
ath_pci_detach(struct device *self, int flags)
{
	struct ath_pci_softc *psc = (struct ath_pci_softc *)self;

	ath_detach(&psc->sc_sc);
	pci_intr_disestablish(psc->sc_pc, psc->sc_ih);

	return (0);
}

static void
ath_pci_shutdown(void *self)
{
	struct ath_pci_softc *psc = (struct ath_pci_softc *)self;

	ath_shutdown(&psc->sc_sc);
}
