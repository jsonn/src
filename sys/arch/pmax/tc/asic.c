/*	$NetBSD: asic.c,v 1.34.4.1 1999/06/21 00:59:18 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Keith Bostic, Chris G. Demetriou, Jonathan Stone
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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>

#include <machine/bus.h>			/* wbflush() */
#include <machine/autoconf.h>

#ifdef alpha
#include <machine/rpb.h>
#include <alpha/tc/tc.h>
#include <alpha/tc/asic.h>
#endif

#ifdef pmax
#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/asic.h>
#include <pmax/pmax/kmin.h>
#include <pmax/pmax/maxine.h>
#include <pmax/pmax/kn03.h>
#include <pmax/pmax/turbochannel.h>	/* interrupt enable declaration */

#include <pmax/pmax/kmin.h>
#include <pmax/pmax/nameglue.h>

/*
 * Which system models were configured?
 */
#include "opt_dec_3max.h"
#include "opt_dec_3min.h"
#include "opt_dec_3maxplus.h"
#include "opt_dec_maxine.h"

/* We  support only one ioctl asic chip, and this is its address. */
tc_addr_t	ioasic_base = 0;

/* tables of child devices on an ioasic bus. */
#define	C(x)	((void*)(x))
#define	ASIC_NDEVS(x)	(sizeof(x)/sizeof(x[0]))
#define ARRAY_SIZEOF(x) (sizeof((x)) / sizeof ((x)[0]))

#if defined(DEC_3MAXPLUS) || defined(DEC_3MIN)
struct ioasic_dev kn03_ioasic_devs[] = {
	{ "lance",	0x0C0000, C(KN03_LANCE_SLOT),	IOASIC_INTR_LANCE },
	{ "scc",	0x100000, C(KN03_SCC0_SLOT),	IOASIC_INTR_SCC_0 },
	{ "scc",	0x180000, C(KN03_SCC1_SLOT),	IOASIC_INTR_SCC_1 },
	{ "mc146818",	0x200000, C(0),			0 },
	{ "asc",	0x300000, C(KN03_SCSI_SLOT),	IOASIC_INTR_SCSI },
};
const int kn03_ioasic_ndevs  =  ARRAY_SIZEOF(kn03_ioasic_devs);
#endif /* DEC_3MAXPLUS || DEC_3MIN */

#if defined(DEC_MAXINE)
struct ioasic_dev xine_ioasic_devs[] = {
	{ "lance",	0x0C0000, C(XINE_LANCE_SLOT),	IOASIC_INTR_LANCE },
	{ "scc",	0x100000, C(XINE_SCC0_SLOT),	IOASIC_INTR_SCC_0 },
	{ "mc146818",	0x200000, C(0),			0 },
	{ "isdn",	0x240000, C(XINE_ISDN_SLOT),	XINE_INTR_ISDN },
	{ "dtop",	0x280000, C(XINE_DTOP_SLOT),	XINE_INTR_DTOP },
	{ "fdc",	0x2C0000, C(XINE_FLOPPY_SLOT),	0 },
	{ "asc",	0x300000, C(XINE_SCSI_SLOT),	IOASIC_INTR_SCSI },
};
const int xine_ioasic_ndevs  =  ARRAY_SIZEOF(xine_ioasic_devs);
#endif /* DEC_MAXINE */

#if defined(DEC_3MAX)
struct ioasic_dev kn02_ioasic_devs[] = {
	{ "dc",		0x200000, C(7), 0 },
	{ "mc146818",	0x280000, C(0), 0 },
};
const int kn02_ioasic_ndevs  =  ARRAY_SIZEOF(kn02_ioasic_devs);
#endif /* DEC_3MAX */
#endif /* pmax */


/* Definition of the driver for autoconfig. */
int	ioasicmatch __P((struct device *, struct cfdata *, void *));
void	ioasicattach __P((struct device *, struct device *, void *));

/* Device locators. */
#include "locators.h"
#define	ioasiccf_offset	cf_loc[IOASICCF_OFFSET]		/* offset */

struct cfattach ioasic_ca = {
	sizeof(struct ioasic_softc), ioasicmatch, ioasicattach
};

#ifdef	pmax
struct ioasic_dev *ioasic_devs;
#endif	/*pmax*/


int
ioasicmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct tc_attach_args *ta = aux;

	/* An IOCTL asic can only occur on the turbochannel, anyway. */
#ifdef notyet
	if (parent != &tccd)
		return (0);
#endif

	/*
	 * XXX This is wrong.
	 * XXX this file will be re-implemented, anyway.
	 */
	/* The 3MAX (kn02) is special. */
	if (TC_BUS_MATCHNAME(ta, "KN02    ")) {
#if 0
		printf("(configuring KN02 system slot as asic)\n");
#endif
		goto gotasic;
	}

	/* Make sure that we're looking for this type of device. */
	if (!TC_BUS_MATCHNAME(ta, "IOCTL   "))
		return (0);
gotasic:

	if (cf->cf_unit > 0)
		return (0);

	return (1);
}

void
ioasicattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ioasic_softc *sc = (struct ioasic_softc *)self;
	struct tc_attach_args *ta = aux;
	int ndevs;

	/* See if the unit number is valid. */
	switch (systype) {
#if defined(DEC_3MAXPLUS) || defined(DEC_3MIN)
	case DS_3MIN:
	case DS_3MAXPLUS:
		/* 3min ioasic addressees are the same as 3maxplus. */
		ioasic_devs = kn03_ioasic_devs;
		ndevs = kn03_ioasic_ndevs;
		break;
#endif

#ifdef DEC_MAXINE
	case DS_MAXINE:
		ioasic_devs = xine_ioasic_devs;
		ndevs = xine_ioasic_ndevs;
		break;
#endif

#ifdef DEC_3MAX
	case DS_3MAX:
		ioasic_devs = kn02_ioasic_devs;
		ndevs = kn02_ioasic_ndevs;
		break;
#endif

	default:
		panic("ioasicattach: shouldn't be here, really...");
	}

	sc->sc_base = ta->ta_addr;

	ioasic_base = sc->sc_base;			/* XXX XXX XXX */

#ifdef pmax
	printf("\n");
#else	/* Alpha AXP: select ASIC speed  */
#ifdef DEC_3000_300
	if (systype == ST_DEC_3000_300) {
		*(volatile u_int *)IOASIC_REG_CSR(sc->sc_base) |=
		    IOASIC_CSR_FASTMODE;
		MB();
		printf(": slow mode\n");
	} else
#endif /*DEC_3000_300*/
		printf(": fast mode\n");

	/* Decstations use hand-craft code to enable asic interrupts */
	BUS_INTR_ESTABLISH(ta, asic_intr, sc);

#endif 	/* Alpha AXP: select ASIC speed  */


	/*
	 * Try to configure each ioctl asic baseboard device.
	 */
        ioasic_attach_devs(sc, ioasic_devs, ndevs);
}


void
ioasic_intr_establish(dev, cookie, level, handler, val)
    struct device *dev;
    void *cookie;
    tc_intrlevel_t level;
    intr_handler_t handler;
    void *val;
{

	(*tc_enable_interrupt)((int)cookie, handler, val, 1);
}


/* XXX */
char *
ioasic_lance_ether_address()
{
	return (u_char *)IOASIC_SYS_ETHER_ADDRESS(ioasic_base);
}

void
ioasic_lance_dma_setup(v)
	void *v;
{
	volatile u_int32_t *ldp;
	tc_addr_t tca;

	tca = (tc_addr_t)v;

	ldp = (volatile u_int *)IOASIC_REG_LANCE_DMAPTR(ioasic_base);
	*ldp = ((tca << 3) & ~(tc_addr_t)0x1f) | ((tca >> 29) & 0x1f);
	tc_wmb();

	*(volatile u_int32_t *)IOASIC_REG_CSR(ioasic_base) |=
	    IOASIC_CSR_DMAEN_LANCE;
	tc_mb();
}
