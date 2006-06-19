/*	$NetBSD: if_epic_pci.c,v 1.29.14.1 2006/06/19 04:01:35 chap Exp $	*/

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * PCI bus front-end for the Standard Microsystems Corp. 83C170
 * Ethernet PCI Integrated Controller (EPIC/100) driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_epic_pci.c,v 1.29.14.1 2006/06/19 04:01:35 chap Exp $");

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

#include <dev/ic/smc83c170reg.h>
#include <dev/ic/smc83c170var.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

/*
 * PCI configuration space registers used by the EPIC.
 */
#define	EPIC_PCI_IOBA		0x10	/* i/o mapped base */
#define	EPIC_PCI_MMBA		0x14	/* memory mapped base */

struct epic_pci_softc {
	struct epic_softc sc_epic;	/* real EPIC softc */

	/* PCI-specific goo. */
	void	*sc_ih;			/* interrupt handle */
};

static int	epic_pci_match(struct device *, struct cfdata *, void *);
static void	epic_pci_attach(struct device *, struct device *, void *);

CFATTACH_DECL(epic_pci, sizeof(struct epic_pci_softc),
    epic_pci_match, epic_pci_attach, NULL, NULL);

static const struct epic_pci_product {
	u_int32_t	epp_prodid;	/* PCI product ID */
	const char	*epp_name;	/* device name */
} epic_pci_products[] = {
	{ PCI_PRODUCT_SMC_83C170,	"SMC 83c170 Fast Ethernet" },
	{ PCI_PRODUCT_SMC_83C175,	"SMC 83c175 Fast Ethernet" },
	{ 0,				NULL },
};

static const struct epic_pci_product *
epic_pci_lookup(const struct pci_attach_args *pa)
{
	const struct epic_pci_product *epp;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_SMC)
		return (NULL);

	for (epp = epic_pci_products; epp->epp_name != NULL; epp++)
		if (PCI_PRODUCT(pa->pa_id) == epp->epp_prodid)
			return (epp);

	return (NULL);
}

static const struct epic_pci_subsys_info {
	pcireg_t subsysid;
	int flags;
} epic_pci_subsys_info[] = {
	{ PCI_ID_CODE(PCI_VENDOR_SMC, 0xa015), /* SMC9432BTX */
	  EPIC_HAS_BNC },
	{ PCI_ID_CODE(PCI_VENDOR_SMC, 0xa024), /* SMC9432BTX1 */
	  EPIC_HAS_BNC },
	{ PCI_ID_CODE(PCI_VENDOR_SMC, 0xa016), /* SMC9432FTX */
	  EPIC_HAS_MII_FIBER | EPIC_DUPLEXLED_ON_694 },
	{ 0xffffffff,
	  0 }
};

static const struct epic_pci_subsys_info *
epic_pci_subsys_lookup(const struct pci_attach_args *pa)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcireg_t reg;
	const struct epic_pci_subsys_info *esp;

	reg = pci_conf_read(pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	for (esp = epic_pci_subsys_info; esp->subsysid != 0xffffffff; esp++)
		if (esp->subsysid == reg)
			return (esp);

	return (NULL);
}

static int
epic_pci_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (epic_pci_lookup(pa) != NULL)
		return (1);

	return (0);
}

static void
epic_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct epic_pci_softc *psc = (struct epic_pci_softc *)self;
	struct epic_softc *sc = &psc->sc_epic;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	const struct epic_pci_product *epp;
	const struct epic_pci_subsys_info *esp;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	int ioh_valid, memh_valid;
	int error;

	aprint_naive(": Ethernet controller\n");

	epp = epic_pci_lookup(pa);
	if (epp == NULL) {
		printf("\n");
		panic("epic_pci_attach: impossible");
	}

	aprint_normal(": %s, rev. %d\n", epp->epp_name,
	    PCI_REVISION(pa->pa_class));

	/* power up chip */
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, sc,
	    NULL)) && error != EOPNOTSUPP) {
		aprint_error("%s: cannot activate %d\n", sc->sc_dev.dv_xname,
		    error);
		return;
	}

	/*
	 * Map the device.
	 */
	ioh_valid = (pci_mapreg_map(pa, EPIC_PCI_IOBA,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL) == 0);
	memh_valid = (pci_mapreg_map(pa, EPIC_PCI_MMBA,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &memt, &memh, NULL, NULL) == 0);

	if (memh_valid) {
		sc->sc_st = memt;
		sc->sc_sh = memh;
	} else if (ioh_valid) {
		sc->sc_st = iot;
		sc->sc_sh = ioh;
	} else {
		aprint_error("%s: unable to map device registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	/* Make sure bus mastering is enabled. */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error("%s: unable to map interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, epic_intr, sc);
	if (psc->sc_ih == NULL) {
		aprint_error("%s: unable to establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			aprint_normal(" at %s", intrstr);
		aprint_normal("\n");
		return;
	}
	aprint_normal("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	esp = epic_pci_subsys_lookup(pa);
	if (esp)
		sc->sc_hwflags = esp->flags;

	/*
	 * Finish off the attach.
	 */
	epic_attach(sc);
}
