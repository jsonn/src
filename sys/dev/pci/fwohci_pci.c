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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ieee1394/ieee1394reg.h>
#include <dev/ieee1394/ieee1394var.h>
#include <dev/ieee1394/fwohcireg.h>
#include <dev/ieee1394/fwohcivar.h>

struct fwohci_pci_softc {
	struct fwohci_softc psc_sc;
	pci_chipset_tag_t psc_pc;
	void *psc_ih;
};

static int fwohci_pci_match __P((struct device *, struct cfdata *, void *));
static void fwohci_pci_attach __P((struct device *, struct device *, void *));

struct cfattach fwohci_pci_ca = {
	sizeof(struct fwohci_pci_softc), fwohci_pci_match, fwohci_pci_attach,
#if 0
	fwohci_pci_detach, fwohci_activate
#endif
};

static int
fwohci_pci_match(struct device *parent, struct cfdata *match, void *aux)
{
        struct pci_attach_args *pa = (struct pci_attach_args *) aux;

        if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SERIALBUS &&
            PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SERIALBUS_FIREWIRE &&
            PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_OHCI)
                return 1;
 
        return 0;
}

static void
fwohci_pci_attach(struct device *parent, struct device *self, void *aux)
{
        struct pci_attach_args *pa = (struct pci_attach_args *) aux;
	struct fwohci_pci_softc *psc = (struct fwohci_pci_softc *) self;
        char devinfo[256];
	char const *intrstr;
	pci_intr_handle_t ih;
	u_int32_t csr;

        pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
        printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(pa->pa_class));

	psc->psc_sc.sc_dmat = pa->pa_dmat;
	psc->psc_pc = pa->pa_pc;

        /* Map I/O registers */
        if (pci_mapreg_map(pa, PCI_OHCI_MAP_REGISTER, PCI_MAPREG_TYPE_MEM, 0,
                           &psc->psc_sc.sc_memt, &psc->psc_sc.sc_memh,
			   NULL, &psc->psc_sc.sc_memsize)) {
                printf("%s: can't map OCHI register space\n", self->dv_xname);
                return;
        }

        /* Disable interrupts, so we don't get any spurious ones. */
        OHCI_CSR_WRITE(&psc->psc_sc, OHCI_REG_IntEventClear, OHCI_Int_MasterEnable);

        /* Enable the device. */
        csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
        pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
                       csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
        	printf("%s: couldn't map interrupt\n", self->dv_xname);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	psc->psc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, fwohci_intr, &psc->psc_sc);
	if (psc->psc_ih == NULL) {
		printf("%s: couldn't establish interrupt", self->dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", self->dv_xname, intrstr);

	fwohci_init(&psc->psc_sc);
}
