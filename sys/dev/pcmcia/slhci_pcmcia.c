/* $NetBSD: slhci_pcmcia.c,v 1.1.6.1 2007/09/03 16:48:36 jmcneill Exp $ */
/*
 * Not (c) 2007 Matthew Orgass
 * This file is public domain, meaning anyone can make any use of part or all 
 * of this file including copying into other works without credit.  Any use, 
 * modified or not, is solely the responsibility of the user.  If this file is 
 * part of a collection then use in the collection is governed by the terms of 
 * the collection.
 */

/* Glue for RATOC USB HOST CF+ Card (SL811HS chip) */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: slhci_pcmcia.c,v 1.1.6.1 2007/09/03 16:48:36 jmcneill Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/gcq.h>
#include <sys/systm.h>
#include <sys/errno.h>

#include <machine/bus.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/usb/usb.h>
#include <dev/usb/usb_port.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>

#include <dev/ic/sl811hsvar.h>

struct slhci_pcmcia_softc {
	struct slhci_softc sc_slhci;

	struct pcmcia_function *sc_pf;
	void * sc_ih;
	int sc_flags;
#define PFL_ENABLED 		(0x1)
};


int slhci_pcmcia_probe(struct device *, struct cfdata *, void *);
void slhci_pcmcia_attach(struct device *, struct device *, void *);
int slhci_pcmcia_detach(struct device *, int);
int slhci_pcmcia_validate_config(struct pcmcia_config_entry *);
int slhci_pcmcia_enable(struct slhci_pcmcia_softc *, int);

CFATTACH_DECL(slhci_pcmcia, sizeof(struct slhci_pcmcia_softc), 
    slhci_pcmcia_probe, slhci_pcmcia_attach, slhci_pcmcia_detach, 
    slhci_activate);

/* Ratoc has two PCMCIA products with id 1.
 * Ratoc has a firmware update that modifies the CIS.  The new CIS may 
 * need a new entry. */
static const struct pcmcia_product slhci_pcmcia_products[] = {
	{ PCMCIA_VENDOR_RATOC, PCMCIA_PRODUCT_RATOC_REX_CFU1,
	  PCMCIA_CIS_RATOC_REX_CFU1 },
};
static const size_t slhci_pcmcia_nproducts =
	sizeof(slhci_pcmcia_products) / sizeof(slhci_pcmcia_products[0]);

int
slhci_pcmcia_probe(struct device *parent, struct cfdata *match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, slhci_pcmcia_products, 
	    slhci_pcmcia_nproducts, sizeof(slhci_pcmcia_products[0]), NULL))
		return 1;

	return 0;
}

int
slhci_pcmcia_validate_config(struct pcmcia_config_entry *cfe)
{
	if (cfe->num_iospace != 1 || cfe->num_memspace != 0)
		return EINVAL;
	return 0;
}

void
slhci_pcmcia_attach(struct device *parent, struct device *self, void *aux)
{
	struct slhci_pcmcia_softc *psc = (void *)self;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_function *pf = pa->pf;

	psc->sc_pf = pf;
	psc->sc_flags = 0;

	slhci_pcmcia_enable(psc, 1);

	return;
}

int
slhci_pcmcia_detach(struct device *self, int flags)
{
	struct slhci_pcmcia_softc *psc = (void *)self;

	slhci_pcmcia_enable(psc, 0);

	return slhci_detach(&psc->sc_slhci, flags);
}

int
slhci_pcmcia_enable(struct slhci_pcmcia_softc *psc, int enable)
{
	struct pcmcia_function *pf;
	struct pcmcia_io_handle *pioh;
	struct slhci_softc *sc;
	int error;

	pf = psc->sc_pf;
	sc = &psc->sc_slhci;

	if (enable) {
		if (psc->sc_flags & PFL_ENABLED)
			return 0;

		error = pcmcia_function_configure(pf, 
		    slhci_pcmcia_validate_config);
		if (error) {
			printf("%s: configure failed, error=%d\n", 
			    SC_NAME(sc), error);
			return 1;
		}

		pioh = &pf->cfe->iospace[0].handle;

		/* The data port is repeated three times; using a stride of 
		 * 2 prevents read/write errors on a Clio C-1000 hpcmips 
		 * system.
		 */
		slhci_preinit(sc, NULL, pioh->iot, pioh->ioh, 100, 2);

		psc->sc_ih = pcmcia_intr_establish(pf, IPL_HARDUSB, 
		    slhci_intr, sc);

		if (psc->sc_ih == NULL) {
			printf("%s: unable to establish interrupt\n", 
			    SC_NAME(sc));
			goto fail1;
		}

		if (pcmcia_function_enable(pf)) {
			printf("%s: function enable failed\n", SC_NAME(sc));
			goto fail2;
		}

		if (slhci_attach(sc)) {
			printf("%s: slhci_attach failed\n", SC_NAME(sc));
			goto fail3;
		}

		psc->sc_flags |= PFL_ENABLED;
		return 0;
	} else {
		if (!(psc->sc_flags & PFL_ENABLED))
			return 1;
		psc->sc_flags &= ~PFL_ENABLED;
fail3:
		pcmcia_function_disable(pf);
fail2:
		pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
fail1:
		pcmcia_function_unconfigure(pf);

		return 1;
	}
}


