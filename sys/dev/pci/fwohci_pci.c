/*	$NetBSD: fwohci_pci.c,v 1.21.4.3 2007/10/27 11:32:46 yamt Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
__KERNEL_RCSID(0, "$NetBSD: fwohci_pci.c,v 1.21.4.3 2007/10/27 11:32:46 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/select.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ieee1394/fw_port.h>
#include <dev/ieee1394/firewire.h>
#include <dev/ieee1394/firewirereg.h>
#include <dev/ieee1394/fwdma.h>
#include <dev/ieee1394/fwohcireg.h>
#include <dev/ieee1394/fwohcivar.h>

struct fwohci_pci_softc {
	struct fwohci_softc psc_sc;
	pci_chipset_tag_t psc_pc;
	void *psc_ih;
};

static int fwohci_pci_match(struct device *, struct cfdata *, void *);
static void fwohci_pci_attach(struct device *, struct device *, void *);

CFATTACH_DECL(fwohci_pci, sizeof(struct fwohci_pci_softc),
    fwohci_pci_match, fwohci_pci_attach, NULL, NULL);

static int
fwohci_pci_match(struct device *parent, struct cfdata *match,
    void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SERIALBUS &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SERIALBUS_FIREWIRE &&
	    PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_OHCI)
		return 1;

	return 0;
}

static void
fwohci_pci_attach(struct device *parent, struct device *self,
    void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;
	struct fwohci_pci_softc *psc = (struct fwohci_pci_softc *) self;
	char devinfo[256];
	char const *intrstr;
	pci_intr_handle_t ih;
	u_int32_t csr;

	aprint_naive(": IEEE 1394 Controller\n");

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo, sizeof(devinfo));
	aprint_normal(": %s (rev. 0x%02x)\n", devinfo,
	    PCI_REVISION(pa->pa_class));

	psc->psc_sc.fc.dmat = pa->pa_dmat;
	psc->psc_pc = pa->pa_pc;

	/* Map I/O registers */
	if (pci_mapreg_map(pa, PCI_OHCI_MAP_REGISTER, PCI_MAPREG_TYPE_MEM, 0,
	    &psc->psc_sc.bst, &psc->psc_sc.bsh,
	    NULL, &psc->psc_sc.bssize)) {
		aprint_error("%s: can't map OHCI register space\n",
		    self->dv_xname);
		return;
	}

	/* Disable interrupts, so we don't get any spurious ones. */
	OHCI_CSR_WRITE(&psc->psc_sc, FWOHCI_INTMASKCLR, OHCI_INT_EN);

	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error("%s: couldn't map interrupt\n", self->dv_xname);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	psc->psc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, fwohci_intr,
	    &psc->psc_sc);
	if (psc->psc_ih == NULL) {
		aprint_error("%s: couldn't establish interrupt",
		    self->dv_xname);
		if (intrstr != NULL)
			aprint_normal(" at %s", intrstr);
		aprint_normal("\n");
		return;
	}
	aprint_normal("%s: interrupting at %s\n", self->dv_xname, intrstr);

	if (fwohci_init(&(psc->psc_sc), &(psc->psc_sc.fc._dev)) != 0) {
		pci_intr_disestablish(pa->pa_pc, psc->psc_ih);
		bus_space_unmap(psc->psc_sc.bst, psc->psc_sc.bsh,
		    psc->psc_sc.bssize);
	}
}
