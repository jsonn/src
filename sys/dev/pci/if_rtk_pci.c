/*	$NetBSD: if_rtk_pci.c,v 1.8.2.3 2002/02/11 20:09:58 jdolecek Exp $	*/

/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	FreeBSD Id: if_rl.c,v 1.17 1999/06/19 20:17:37 wpaul Exp
 */

/*
 * RealTek 8129/8139 PCI NIC driver
 *
 * Supports several extremely cheap PCI 10/100 adapters based on
 * the RealTek chipset. Datasheets can be obtained from
 * www.realtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_rtk_pci.c,v 1.8.2.3 2002/02/11 20:09:58 jdolecek Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_ether.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

/*
 * Default to using PIO access for this driver. On SMP systems,
 * there appear to be problems with memory mapped mode: it looks like
 * doing too many memory mapped access back to back in rapid succession
 * can hang the bus. I'm inclined to blame this on crummy design/construction
 * on the part of RealTek. Memory mapped mode does appear to work on
 * uniprocessor systems though.
 */
#ifndef dreamcast		/* XXX */
#define RTK_USEIOSPACE
#endif

#include <dev/ic/rtl81x9reg.h>
#include <dev/ic/rtl81x9var.h>

struct rtk_pci_softc {
	struct rtk_softc sc_rtk;	/* real rtk softc */

	/* PCI-specific goo.*/
	void *sc_ih;
	pci_chipset_tag_t sc_pc; 	/* PCI chipset */
	pcitag_t sc_pcitag;		/* PCI tag */
};

static const struct rtk_type rtk_pci_devs[] = {
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8129,
		"RealTek 8129 10/100BaseTX",
		RTK_8129 },
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8139,
		"RealTek 8139 10/100BaseTX",
		RTK_8139 },
	{ PCI_VENDOR_ACCTON, PCI_PRODUCT_ACCTON_MPX5030,
		"Accton MPX 5030/5038 10/100BaseTX",
		RTK_8139 },
	{ PCI_VENDOR_DELTA, PCI_PRODUCT_DELTA_8139,
		"Delta Electronics 8139 10/100BaseTX",
		RTK_8139 },
	{ PCI_VENDOR_ADDTRON, PCI_PRODUCT_ADDTRON_8139,
		"Addtron Technology 8139 10/100BaseTX",
		RTK_8139 },
	{ PCI_VENDOR_SEGA, PCI_PRODUCT_SEGA_BROADBAND,
		"SEGA Broadband Adapter",
		RTK_8139 },
	{ PCI_VENDOR_DLINK, PCI_PRODUCT_DLINK_DFE530TXPLUS,
		"D-Link Systems DFE 530TX+",
		RTK_8139 },
	{ 0, 0, NULL, 0 }
};

const struct rtk_type *rtk_pci_lookup __P((const struct pci_attach_args *));

int	rtk_pci_match __P((struct device *, struct cfdata *, void *));
void	rtk_pci_attach __P((struct device *, struct device *, void *));

struct cfattach rtk_pci_ca = {
	sizeof(struct rtk_pci_softc), rtk_pci_match, rtk_pci_attach,
};

const struct rtk_type *
rtk_pci_lookup(pa)
	const struct pci_attach_args *pa;
{
	const struct rtk_type *t;

	for (t = rtk_pci_devs; t->rtk_name != NULL; t++) {
		if (PCI_VENDOR(pa->pa_id) == t->rtk_vid &&
		    PCI_PRODUCT(pa->pa_id) == t->rtk_did) {
			return (t);
		}
	}
	return (NULL);
}

int
rtk_pci_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (rtk_pci_lookup(pa) != NULL)
		return (1);

	return (0);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
rtk_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct rtk_pci_softc *psc = (struct rtk_pci_softc *)self;
	struct rtk_softc *sc = &psc->sc_rtk;
	pcireg_t command;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	const struct rtk_type *t;
	int pmreg;

	psc->sc_pc = pa->pa_pc;
	psc->sc_pcitag = pa->pa_tag;

	t = rtk_pci_lookup(pa);
	if (t == NULL) {
		printf("\n");
		panic("rtk_pci_attach: impossible");
	}
	printf(": %s\n", t->rtk_name);

	/*
	 * Handle power management nonsense.
	 */

	if (pci_get_capability(pc, pa->pa_tag, PCI_CAP_PWRMGMT, &pmreg, 0)) {
		command = pci_conf_read(pc, pa->pa_tag, pmreg + 4);
		if (command & RTK_PSTATE_MASK) {
			pcireg_t iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(pc, pa->pa_tag, RTK_PCI_LOIO);
			membase = pci_conf_read(pc, pa->pa_tag, RTK_PCI_LOMEM);
			irq = pci_conf_read(pc, pa->pa_tag,
			    PCI_PRODUCT_DELTA_8139);

			/* Reset the power state. */
			printf("%s: chip is is in D%d power mode "
			    "-- setting to D0\n", sc->sc_dev.dv_xname,
			    command & RTK_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(pc, pa->pa_tag, pmreg + 4, command);

			/* Restore PCI config data. */
			pci_conf_write(pc, pa->pa_tag, RTK_PCI_LOIO, iobase);
			pci_conf_write(pc, pa->pa_tag, RTK_PCI_LOMEM, membase);
			pci_conf_write(pc, pa->pa_tag,
			    PCI_PRODUCT_DELTA_8139, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */
#ifdef RTK_USEIOSPACE
	if (pci_mapreg_map(pa, RTK_PCI_LOIO, PCI_MAPREG_TYPE_IO, 0,
	    &sc->rtk_btag, &sc->rtk_bhandle, NULL, NULL)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}
#else
	if (pci_mapreg_map(pa, RTK_PCI_LOMEM, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->rtk_btag, &sc->rtk_bhandle, NULL, NULL)) {
		printf("%s: can't map mem space\n", sc->sc_dev.dv_xname);
		return;
	}
#endif

	/* Allocate interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, rtk_intr, sc);
	if (psc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	sc->rtk_type = t->rtk_type;

	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	sc->sc_dmat = pa->pa_dmat;

	rtk_attach(sc);
}
