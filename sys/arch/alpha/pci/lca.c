/*	$NetBSD: lca.c,v 1.13.2.1 1996/12/07 02:09:11 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Jeffrey Hsu and Chris G. Demetriou
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
#include <alpha/pci/lcareg.h>
#include <alpha/pci/lcavar.h>
#if defined(DEC_AXPPCI_33)
#include <alpha/pci/pci_axppci_33.h>
#endif

#ifdef __BROKEN_INDIRECT_CONFIG
int	lcamatch __P((struct device *, void *, void *));
#else
int	lcamatch __P((struct device *, struct cfdata *, void *));
#endif
void	lcaattach __P((struct device *, struct device *, void *));

struct cfattach lca_ca = {
	sizeof(struct lca_softc), lcamatch, lcaattach,
};

struct cfdriver lca_cd = {
	NULL, "lca", DV_DULL,
};

static int	lcaprint __P((void *, const char *pnp));

/* There can be only one. */
int lcafound;
struct lca_config lca_configuration;

int
lcamatch(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct confargs *ca = aux;

	/* Make sure that we're looking for a LCA. */
	if (strcmp(ca->ca_name, lca_cd.cd_name) != 0)
		return (0);

	if (lcafound)
		return (0);

	return (1);
}

/*
 * Set up the chipset's function pointers.
 */
void
lca_init(lcp, mallocsafe)
	struct lca_config *lcp;
	int mallocsafe;
{

	/*
	 * Can't set up SGMAP data here; can be called before malloc().
	 */

	/*
	 * The LCA HAE register is WRITE-ONLY, so we can't tell where
	 * the second sparse window is actually mapped.  Therefore,
	 * we have to guess where it is.  This seems to be the normal
	 * address.
	 */
	lcp->lc_s_mem_w2_masked_base = 0x80000000;

	if (!lcp->lc_initted) {
		/* don't do these twice since they set up extents */
		lcp->lc_iot = lca_bus_io_init(lcp);
		lcp->lc_memt = lca_bus_mem_init(lcp);
	}
	lcp->lc_mallocsafe = mallocsafe;

	lca_pci_init(&lcp->lc_pc, lcp);

	/*
	 * Refer to ``DECchip 21066 and DECchip 21068 Alpha AXP Microprocessors
	 * Hardware Reference Manual''.
	 * ...
	 */

	/*
	 * According to section 6.4.1, all bits of the IOC_HAE register are
	 * undefined after reset.  Bits <31:27> are write-only.  However, we
	 * cannot blindly set it to zero.  The serial ROM code that initializes
	 * the PCI devices' address spaces, allocates sparse memory blocks in
	 * the range that must use the IOC_HAE register for address translation,
	 * and sets this register accordingly (see section 6.4.14).
	 *
	 *	IOC_HAE left AS IS.
	 */

	/* According to section 6.4.2, all bits of the IOC_CONF register are
	 * undefined after reset.  Bits <1:0> are write-only.  Set them to
	 * 0x00 for PCI Type 0 configuration access.
	 *
	 *	IOC_CONF set to ZERO.
	 */
	REGVAL(LCA_IOC_CONF) = 0;

	/* Turn off DMA window enables in Window Base Registers */
/*	REGVAL(LCA_IOC_W_BASE0) = 0;
	REGVAL(LCA_IOC_W_BASE1) = 0; */
	alpha_mb();

	/* XXX XXX BEGIN XXX XXX */
	{							/* XXX */
		extern vm_offset_t alpha_XXX_dmamap_or;		/* XXX */
		alpha_XXX_dmamap_or = 0x40000000;		/* XXX */
	}							/* XXX */
	/* XXX XXX END XXX XXX */

	lcp->lc_initted = 1;
}

#ifdef notdef
void
lca_init_sgmap(lcp)
	struct lca_config *lcp;
{

	/* XXX */
	lcp->lc_sgmap = malloc(1024 * 8, M_DEVBUF, M_WAITOK);
	bzero(lcp->lc_sgmap, 1024 * 8);		/* clear all entries. */

	REGVAL(LCA_IOC_W_BASE0) = 0;
	alpha_mb();

	/* Set up Translated Base Register 1; translate to sybBus addr 0. */
	/* check size against APEC XXX JH */
	REGVAL(LCA_IOC_T_BASE_0) = vtophys(lcp->lc_sgmap) >> 1;

	/* Set up PCI mask register 1; map 8MB space. */
	REGVAL(LCA_IOC_W_MASK0) = 0x00700000;

	/* Enable window 1; from PCI address 8MB, direct mapped. */
	REGVAL(LCA_IOC_W_BASE0) = 0x300800000;
	alpha_mb();
}
#endif

void
lcaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lca_softc *sc = (struct lca_softc *)self;
	struct lca_config *lcp;
	struct pcibus_attach_args pba;

	/* note that we've attached the chipset; can't have 2 LCAs. */
	/* Um, not sure about this.  XXX JH */
	lcafound = 1;

	/*
	 * set up the chipset's info; done once at console init time
	 * (maybe), but doesn't hurt to do twice.
	 */
	lcp = sc->sc_lcp = &lca_configuration;
	lca_init(lcp, 1);
#ifdef notdef
	lca_init_sgmap(lcp);
#endif

	/* XXX print chipset information */
	printf("\n");

	switch (hwrpb->rpb_type) {
#if defined(DEC_AXPPCI_33)
	case ST_DEC_AXPPCI_33:
		pci_axppci_33_pickintr(lcp);
		break;
#endif

	default:
		panic("lcaattach: shouldn't be here, really...");
	}

	pba.pba_busname = "pci";
	pba.pba_iot = lcp->lc_iot;
	pba.pba_memt = lcp->lc_memt;
	pba.pba_pc = &lcp->lc_pc;
	pba.pba_bus = 0;
	config_found(self, &pba, lcaprint);
}

static int
lcaprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	register struct pcibus_attach_args *pba = aux;

	/* only PCIs can attach to LCAes; easy. */
	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);
	return (UNCONF);
}
