/*	$NetBSD: bha_pci.c,v 1.6.4.1 1997/03/12 16:20:00 is Exp $	*/

/*
 * Copyright (c) 1994, 1996 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/bhareg.h>
#include <dev/ic/bhavar.h>

#define	PCI_CBIO	0x10

int	bha_pci_match __P((struct device *, void *, void *));
void	bha_pci_attach __P((struct device *, struct device *, void *));

struct cfattach bha_pci_ca = {
	sizeof(struct bha_softc), bha_pci_match, bha_pci_attach
};

/*
 * Check the slots looking for a board we recognise
 * If we find one, note it's address (slot) and call
 * the actual probe routine to check it out.
 */
int
bha_pci_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = aux;
	bus_space_tag_t iot = pa->pa_iot;
	bus_addr_t iobase;
	bus_size_t iosize;
	bus_space_handle_t ioh;
	pci_chipset_tag_t pc = pa->pa_pc;
	int rv;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_BUSLOGIC)
		return (0);

	if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_BUSLOGIC_MULTIMASTER_NC &&
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_BUSLOGIC_MULTIMASTER)
		return (0);

	if (pci_io_find(pc, pa->pa_tag, PCI_CBIO, &iobase, &iosize))
		return (0);
	if (bus_space_map(iot, iobase, iosize, 0, &ioh))
		return (0);

	rv = bha_find(iot, ioh, NULL);

	bus_space_unmap(iot, ioh, iosize);

	return (rv);
}

/*
 * Attach all the sub-devices we can find
 */
void
bha_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct bha_softc *sc = (void *)self;
	bus_space_tag_t iot = pa->pa_iot;
	bus_addr_t iobase;
	bus_size_t iosize;
	bus_space_handle_t ioh;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	pcireg_t csr;
	const char *model, *intrstr;

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BUSLOGIC_MULTIMASTER_NC)
		model = "BusLogic 9xxC SCSI";
	else if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BUSLOGIC_MULTIMASTER)
		model = "BusLogic 9xxC SCSI";
	else
		model = "unknown model!";
	printf(": %s\n", model);

	if (pci_io_find(pc, pa->pa_tag, PCI_CBIO, &iobase, &iosize))
		panic("bha_attach: pci_io_find failed!");
	if (bus_space_map(iot, iobase, iosize, 0, &ioh))
		panic("bha_attach: bus_space_map failed!");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	if (!bha_find(iot, ioh, sc))
		panic("bha_attach: bha_find failed!");

	csr = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_IO_ENABLE);

	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, bha_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	bha_attach(sc);

	bha_disable_isacompat(sc);
}
