/* $NetBSD: cia.c,v 1.27.2.1 1997/10/27 01:11:04 thorpej Exp $ */

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

#include "opt_dec_eb164.h"
#include "opt_dec_kn20aa.h"

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: cia.c,v 1.27.2.1 1997/10/27 01:11:04 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <machine/autoconf.h>
#include <machine/rpb.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>
#ifdef DEC_KN20AA
#include <alpha/pci/pci_kn20aa.h>
#endif
#ifdef DEC_EB164
#include <alpha/pci/pci_eb164.h>
#endif

int	ciamatch __P((struct device *, struct cfdata *, void *));
void	ciaattach __P((struct device *, struct device *, void *));

struct cfattach cia_ca = {
	sizeof(struct cia_softc), ciamatch, ciaattach,
};

struct cfdriver cia_cd = {
	NULL, "cia", DV_DULL,
};

static int	ciaprint __P((void *, const char *pnp));

/* There can be only one. */
int ciafound;
struct cia_config cia_configuration;

int
ciamatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct confargs *ca = aux;

	/* Make sure that we're looking for a CIA. */
	if (strcmp(ca->ca_name, cia_cd.cd_name) != 0)
		return (0);

	if (ciafound)
		return (0);

	return (1);
}

/*
 * Set up the chipset's function pointers.
 */
void
cia_init(ccp, mallocsafe)
	struct cia_config *ccp;
	int mallocsafe;
{

	ccp->cc_hae_mem = REGVAL(CIA_CSR_HAE_MEM);
	ccp->cc_hae_io = REGVAL(CIA_CSR_HAE_IO);
	ccp->cc_rev = REGVAL(CIA_CSR_REV);

	/*
	 * Revisions >= 2 have the CNFG register.
	 */
	if (ccp->cc_rev >= 2)
		ccp->cc_cnfg = REGVAL(CIA_CSR_CNFG);
	else
		ccp->cc_cnfg = 0;

	if (!ccp->cc_initted) {
		/* don't do these twice since they set up extents */
		cia_swiz_bus_io_init(&ccp->cc_iot, ccp);
		cia_swiz_bus_mem_init(&ccp->cc_memt, ccp);
	}
	ccp->cc_mallocsafe = mallocsafe;

	cia_pci_init(&ccp->cc_pc, ccp);

	cia_dma_init(ccp);

	ccp->cc_initted = 1;
}

void
ciaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cia_softc *sc = (struct cia_softc *)self;
	struct cia_config *ccp;
	struct pcibus_attach_args pba;
	char bits[64];

	/* note that we've attached the chipset; can't have 2 CIAs. */
	ciafound = 1;

	/*
	 * set up the chipset's info; done once at console init time
	 * (maybe), but we must do it here as well to take care of things
	 * that need to use memory allocation.
	 */
	ccp = sc->sc_ccp = &cia_configuration;
	cia_init(ccp, 1);

	printf(": DECchip 2117%d Core Logic chipset\n", ccp->cc_rev);
	if (ccp->cc_cnfg)
		printf("%s: extended capabilities: %s\n", self->dv_xname,
		    bitmask_snprintf(ccp->cc_cnfg, CIA_CSR_CNFG_BITS,
		    bits, sizeof(bits)));

	switch (hwrpb->rpb_type) {
#ifdef DEC_KN20AA
	case ST_DEC_KN20AA:
		pci_kn20aa_pickintr(ccp);
#ifdef EVCNT_COUNTERS
		evcnt_attach(self, "intr", &kn20aa_intr_evcnt);
#endif
		break;
#endif

#ifdef DEC_EB164
	case ST_EB164:
		pci_eb164_pickintr(ccp);
#ifdef EVCNT_COUNTERS
		evcnt_attach(self, "intr", &eb164_intr_evcnt);
#endif
		break;
#endif

	default:
		panic("ciaattach: shouldn't be here, really...");
	}

	pba.pba_busname = "pci";
	pba.pba_iot = &ccp->cc_iot;
	pba.pba_memt = &ccp->cc_memt;
	pba.pba_dmat = &ccp->cc_dmat_direct;
	pba.pba_pc = &ccp->cc_pc;
	pba.pba_bus = 0;
	pba.pba_flags = PCI_FLAGS_IO_ENABLED | PCI_FLAGS_MEM_ENABLED;
	config_found(self, &pba, ciaprint);
}

static int
ciaprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	register struct pcibus_attach_args *pba = aux;

	/* only PCIs can attach to CIAs; easy. */
	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);
	return (UNCONF);
}
