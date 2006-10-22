/*	$NetBSD: fwohci_cardbus.c,v 1.16.10.1 2006/10/22 06:05:35 yamt Exp $	*/

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: fwohci_cardbus.c,v 1.16.10.1 2006/10/22 06:05:35 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/select.h>

#include <machine/bus.h>

#if defined pciinc
#include <dev/pci/pcidevs.h>
#endif

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ieee1394/fw_port.h>
#include <dev/ieee1394/firewire.h>
#include <dev/ieee1394/firewirereg.h>
#include <dev/ieee1394/fwdma.h>
#include <dev/ieee1394/fwohcireg.h>
#include <dev/ieee1394/fwohcivar.h>

struct fwohci_cardbus_softc {
	struct fwohci_softc	sc_sc;
	cardbus_chipset_tag_t	sc_cc;
	cardbus_function_tag_t	sc_cf;
	cardbus_devfunc_t	sc_ct;
	void		       *sc_ih;
};

static int fwohci_cardbus_match(struct device *, struct cfdata *, void *);
static void fwohci_cardbus_attach(struct device *, struct device *, void *);
static int fwohci_cardbus_detach(struct device *, int);

CFATTACH_DECL(fwohci_cardbus, sizeof(struct fwohci_cardbus_softc),
    fwohci_cardbus_match, fwohci_cardbus_attach,
    fwohci_cardbus_detach, NULL);

#define CARDBUS_INTERFACE_OHCI PCI_INTERFACE_OHCI
#define CARDBUS_OHCI_MAP_REGISTER PCI_OHCI_MAP_REGISTER
#define cardbus_devinfo pci_devinfo

static int
fwohci_cardbus_match(struct device *parent __unused,
    struct cfdata *match __unused, void *aux)
{
	struct cardbus_attach_args *ca = (struct cardbus_attach_args *)aux;

	if (CARDBUS_CLASS(ca->ca_class) == CARDBUS_CLASS_SERIALBUS &&
	    CARDBUS_SUBCLASS(ca->ca_class) ==
	        CARDBUS_SUBCLASS_SERIALBUS_FIREWIRE &&
	    CARDBUS_INTERFACE(ca->ca_class) == CARDBUS_INTERFACE_OHCI)
		return 1;

	return 0;
}

static void
fwohci_cardbus_attach(struct device *parent __unused, struct device *self,
    void *aux)
{
	struct cardbus_attach_args *ca = aux;
	struct fwohci_cardbus_softc *sc = device_private(self);
	cardbus_devfunc_t ct = ca->ca_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	cardbusreg_t csr;
	char devinfo[256];
	const char *devname = self->dv_xname;

	cardbus_devinfo(ca->ca_id, ca->ca_class, 0, devinfo, sizeof(devinfo));
	printf(": %s (rev. 0x%02x)\n", devinfo,
	       CARDBUS_REVISION(ca->ca_class));

	/* Map I/O registers */
	if (Cardbus_mapreg_map(ct, CARDBUS_OHCI_MAP_REGISTER,
	      CARDBUS_MAPREG_TYPE_MEM, 0,
	      &sc->sc_sc.bst, &sc->sc_sc.bsh,
	      NULL, &sc->sc_sc.bssize)) {
		printf("%s: can't map OHCI register space\n", devname);
		return;
	}

	sc->sc_cc = cc;
	sc->sc_cf = cf;
	sc->sc_ct = ct;
	sc->sc_sc.fc.dmat = ca->ca_dmat;

#if rbus
#else
XXX	(ct->ct_cf->cardbus_mem_open)(cc, 0, iob, iob + 0x40);
#endif
	(ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_MEM_ENABLE);
	(ct->ct_cf->cardbus_ctrl)(cc, CARDBUS_BM_ENABLE);

	/* Disable interrupts, so we don't get any spurious ones. */
	OHCI_CSR_WRITE(&sc->sc_sc, FWOHCI_INTMASKCLR, OHCI_INT_EN);

	/* Enable the device. */
	csr = cardbus_conf_read(cc, cf, ca->ca_tag,
	    CARDBUS_COMMAND_STATUS_REG);
	cardbus_conf_write(cc, cf, ca->ca_tag, CARDBUS_COMMAND_STATUS_REG,
	    csr | CARDBUS_COMMAND_MASTER_ENABLE | CARDBUS_COMMAND_MEM_ENABLE);

	sc->sc_ih = cardbus_intr_establish(cc, cf, ca->ca_intrline,
					   IPL_BIO, fwohci_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n", devname);
		return;
	}
	printf("%s: interrupting at %d\n", devname, ca->ca_intrline);

	/* XXX NULL should be replaced by some call to Cardbus coed */
	if (fwohci_init(&sc->sc_sc, &(sc->sc_sc.fc._dev)) != 0) {
		cardbus_intr_disestablish(cc, cf, sc->sc_ih);
		bus_space_unmap(sc->sc_sc.bst, sc->sc_sc.bsh,
		    sc->sc_sc.bssize);
	}
}

int
fwohci_cardbus_detach(struct device *self, int flags)
{
	struct fwohci_cardbus_softc *sc = device_private(self);
	cardbus_devfunc_t ct = sc->sc_ct;
	int rv;

	rv = fwohci_detach(&sc->sc_sc, flags);

	if (rv)
		return (rv);
	if (sc->sc_ih != NULL) {
		cardbus_intr_disestablish(ct->ct_cc, ct->ct_cf, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (sc->sc_sc.bssize) {
		Cardbus_mapreg_unmap(ct, CARDBUS_OHCI_MAP_REGISTER,
			sc->sc_sc.bst, sc->sc_sc.bsh,
			sc->sc_sc.bssize);
		sc->sc_sc.bssize = 0;
	}
	return (0);
}
