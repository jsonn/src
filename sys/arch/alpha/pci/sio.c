/* $NetBSD: sio.c,v 1.14.2.3 1997/06/01 04:13:47 cgd Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <machine/options.h>		/* Config options headers */
#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: sio.c,v 1.14.2.3 1997/06/01 04:13:47 cgd Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/eisa/eisavar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <alpha/pci/siovar.h>

struct sio_softc {
	struct device	sc_dv;

	bus_space_tag_t sc_iot, sc_memt;
	int		sc_haseisa;
};

int	siomatch __P((struct device *, struct cfdata *, void *));
void	sioattach __P((struct device *, struct device *, void *));

struct cfattach sio_ca = {
	sizeof(struct sio_softc), siomatch, sioattach,
};

struct cfdriver sio_cd = {
	NULL, "sio", DV_DULL,
};

int	pcebmatch __P((struct device *, struct cfdata *, void *));

struct cfattach pceb_ca = {
	sizeof(struct device), pcebmatch, sioattach,
};

struct cfdriver pceb_cd = {
	NULL, "pceb", DV_DULL,
};

union sio_attach_args {
	const char *sa_name;			/* XXX should be common */
	struct isabus_attach_args sa_iba;
	struct eisabus_attach_args sa_eba;
};

int	sioprint __P((void *, const char *pnp));
void	sio_isa_attach_hook __P((struct device *, struct device *,
	    struct isabus_attach_args *));
void	sio_eisa_attach_hook __P((struct device *, struct device *,
	    struct eisabus_attach_args *));
int	sio_eisa_maxslots __P((void *));
int	sio_eisa_intr_map __P((void *, u_int, eisa_intr_handle_t *));

void	sio_bridge_callback __P((void *));

int
siomatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL ||
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_INTEL_SIO)
		return (0);

	return (1);
}

int
pcebmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL ||
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_INTEL_PCEB)
		return (0);

	return (1);
}

void
sioattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sio_softc *sc = (struct sio_softc *)self;
	struct pci_attach_args *pa = aux;
	char devinfo[256];

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo,
	    PCI_REVISION(pa->pa_class));

	sc->sc_iot = pa->pa_iot;
	sc->sc_memt = pa->pa_memt;
	sc->sc_haseisa = (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PCEB);

#ifdef EVCNT_COUNTERS
	evcnt_attach(&sc->sc_dv, "intr", &sio_intr_evcnt);
#endif

	set_pci_isa_bridge_callback(sio_bridge_callback, sc);
}

void
sio_bridge_callback(v)
	void *v;
{
	struct sio_softc *sc = v;
	struct alpha_eisa_chipset ec;
	struct alpha_isa_chipset ic;
	union sio_attach_args sa;

	if (sc->sc_haseisa) {
		ec.ec_v = NULL;
		ec.ec_attach_hook = sio_eisa_attach_hook;
		ec.ec_maxslots = sio_eisa_maxslots;
		ec.ec_intr_map = sio_eisa_intr_map;
		ec.ec_intr_string = sio_intr_string;
		ec.ec_intr_establish = sio_intr_establish;
		ec.ec_intr_disestablish = sio_intr_disestablish;

		sa.sa_eba.eba_busname = "eisa";
		sa.sa_eba.eba_iot = sc->sc_iot;
		sa.sa_eba.eba_memt = sc->sc_memt;
		sa.sa_eba.eba_ec = &ec;
		config_found(&sc->sc_dv, &sa.sa_eba, sioprint);
	}

	ic.ic_v = NULL;
	ic.ic_attach_hook = sio_isa_attach_hook;
	ic.ic_intr_establish = sio_intr_establish;
	ic.ic_intr_disestablish = sio_intr_disestablish;

	sa.sa_iba.iba_busname = "isa";
	sa.sa_iba.iba_iot = sc->sc_iot;
	sa.sa_iba.iba_memt = sc->sc_memt;
	sa.sa_iba.iba_ic = &ic;
	config_found(&sc->sc_dv, &sa.sa_iba, sioprint);
}

int
sioprint(aux, pnp)
	void *aux;
	const char *pnp;
{
        register union sio_attach_args *sa = aux;

        if (pnp)
                printf("%s at %s", sa->sa_name, pnp);
        return (UNCONF);
}

void
sio_isa_attach_hook(parent, self, iba)
	struct device *parent, *self;
	struct isabus_attach_args *iba;
{

	/* Nothing to do. */
}

void
sio_eisa_attach_hook(parent, self, eba)
	struct device *parent, *self;
	struct eisabus_attach_args *eba;
{

	/* Nothing to do. */
}

int
sio_eisa_maxslots(v)
	void *v;
{

	return 16;		/* as good a number as any.  only 8, maybe? */
}

int
sio_eisa_intr_map(v, irq, ihp)
	void *v;
	u_int irq;
	eisa_intr_handle_t *ihp;
{

#define	ICU_LEN		16	/* number of ISA IRQs (XXX) */

	if (irq >= ICU_LEN) {
		printf("sio_eisa_intr_map: bad IRQ %d\n", irq);
		*ihp = -1;
		return 1;
	}
	if (irq == 2) {
		printf("sio_eisa_intr_map: changed IRQ 2 to IRQ 9\n");
		irq = 9;
	}

	*ihp = irq;
	return 0;
}
