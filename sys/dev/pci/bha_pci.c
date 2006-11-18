/*	$NetBSD: bha_pci.c,v 1.28.20.1 2006/11/18 21:34:29 ad Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
__KERNEL_RCSID(0, "$NetBSD: bha_pci.c,v 1.28.20.1 2006/11/18 21:34:29 ad Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/bhareg.h>
#include <dev/ic/bhavar.h>

#define	PCI_CBIO	0x10

/*
 * Check the slots looking for a board we recognise
 * If we find one, note it's address (slot) and call
 * the actual probe routine to check it out.
 */
static int
bha_pci_match(struct device *parent, struct cfdata *match,
    void *aux)
{
	struct pci_attach_args *pa = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t iosize;
	int rv;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_BUSLOGIC)
		return (0);

	if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_BUSLOGIC_MULTIMASTER_NC &&
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_BUSLOGIC_MULTIMASTER)
		return (0);

	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0, &iot, &ioh,
	    NULL, &iosize))
		return (0);

	rv = bha_find(iot, ioh);

	bus_space_unmap(iot, ioh, iosize);

	return (rv);
}

/*
 * Attach all the sub-devices we can find
 */
static void
bha_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct bha_softc *sc = (void *)self;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	pcireg_t csr;
	const char *model, *intrstr;

	aprint_naive(": SCSI controller\n");

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BUSLOGIC_MULTIMASTER_NC)
		model = "BusLogic 9xxC SCSI";
	else if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BUSLOGIC_MULTIMASTER)
		model = "BusLogic 9xxC SCSI";
	else
		model = "unknown model!";
	aprint_normal(": %s\n", model);

	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0, &iot, &ioh,
	    NULL, NULL)) {
		aprint_error("%s: unable to map device registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = pa->pa_dmat;
	if (!bha_find(iot, ioh))
		panic("bha_pci_attach: bha_find failed");

	sc->sc_dmaflags = 0;

	csr = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_IO_ENABLE);

	if (pci_intr_map(pa, &ih)) {
		aprint_error("%s: couldn't map interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, bha_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			aprint_normal(" at %s", intrstr);
		aprint_normal("\n");
		return;
	}
	aprint_normal("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	bha_attach(sc);

	bha_disable_isacompat(sc);
}

CFATTACH_DECL(bha_pci, sizeof(struct bha_softc),
    bha_pci_match, bha_pci_attach, NULL, NULL);
