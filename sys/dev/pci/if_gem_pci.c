/*	$NetBSD: if_gem_pci.c,v 1.2.2.2 2001/10/01 12:45:54 fvdl Exp $ */

/*
 * 
 * Copyright (C) 2001 Eduardo Horvath.
 * All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * PCI bindings for Sun GEM ethernet controllers.
 */

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/endian.h>

#include <uvm/uvm_extern.h>
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#if NBPFILTER > 0 
#include <net/bpf.h>
#endif 

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/ic/gemreg.h>
#include <dev/ic/gemvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

struct gem_pci_softc {
	struct	gem_softc	gsc_gem;	/* GEM device */
	bus_space_tag_t		gsc_memt;
	bus_space_handle_t	gsc_memh;
	void			*gsc_ih;
};

int	gem_match_pci __P((struct device *, struct cfdata *, void *));
void	gem_attach_pci __P((struct device *, struct device *, void *));

struct cfattach gem_pci_ca = {
	sizeof(struct gem_pci_softc), gem_match_pci, gem_attach_pci
};

/*
 * Attach routines need to be split out to different bus-specific files.
 */

int
gem_match_pci(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SUN && 
	       (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SUN_ERINETWORK ||
		PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SUN_GEMNETWORK))
		return (1);

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_APPLE && 
	       (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_APPLE_GMAC ||
		PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_APPLE_GMAC2))
		return (1);


	return (0);
}

void
gem_attach_pci(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct gem_pci_softc *gsc = (void *)self;
	struct gem_softc *sc = &gsc->gsc_gem;
	pci_intr_handle_t intrhandle;
#ifdef __sparc__
	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr __P((u_char *));
#endif
	const char *intrstr;
	int type;
	char devinfo[256];

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(pa->pa_class));

	if (pa->pa_memt) {
		type = PCI_MAPREG_TYPE_MEM;
		sc->sc_bustag = pa->pa_memt;
	} else {
		type = PCI_MAPREG_TYPE_IO;
		sc->sc_bustag = pa->pa_iot;
	}

	sc->sc_dmatag = pa->pa_dmat;

	sc->sc_pci = 1; /* XXXXX should all be done in bus_dma. */

#define PCI_GEM_BASEADDR	0x10
	if (pci_mapreg_map(pa, PCI_GEM_BASEADDR, type, 0,
	    &gsc->gsc_memt, &gsc->gsc_memh, NULL, NULL) != 0)
	{
		printf(": could not map gem registers\n");
		return;
	}

	sc->sc_bustag = gsc->gsc_memt;
	sc->sc_h = gsc->gsc_memh;

#if 0
/* SBUS compatible stuff? */
	sc->sc_seb = gsc->gsc_memh;
	sc->sc_etx = gsc->gsc_memh + 0x2000;
	sc->sc_erx = gsc->gsc_memh + 0x4000;
	sc->sc_mac = gsc->gsc_memh + 0x6000;
	sc->sc_mif = gsc->gsc_memh + 0x7000;
#endif
#ifdef __sparc__
	myetheraddr(sc->sc_enaddr);
#endif

	sc->sc_burst = 16;	/* XXX */

	/*
	 * call the main configure
	 */
	gem_config(sc);

	if (pci_intr_map(pa, &intrhandle) != 0) {
		printf("%s: couldn't map interrupt\n",
		    sc->sc_dev.dv_xname);
		return;	/* bus_unmap ? */
	}	
	intrstr = pci_intr_string(pa->pa_pc, intrhandle);
	gsc->gsc_ih = pci_intr_establish(pa->pa_pc,
	    intrhandle, IPL_NET, gem_intr, sc);
	if (gsc->gsc_ih != NULL) {
		printf("%s: using %s for interrupt\n",
		    sc->sc_dev.dv_xname,
		    intrstr ? intrstr : "unknown interrupt");
	} else {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;	/* bus_unmap ? */
	}
}
