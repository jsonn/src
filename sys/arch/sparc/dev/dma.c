/*	$NetBSD: dma.c,v 1.54.2.1 1998/08/08 03:06:40 eeh Exp $ */

/*
 * Copyright (c) 1994 Paul Kranenburg.  All rights reserved.
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
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
 *	This product includes software developed by Peter Galbavy.
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
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <sparc/sparc/cpuvar.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <dev/sbus/sbusvar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/dmavar.h>
#include <sparc/dev/espvar.h>

int	dmamatch_sbus	__P((struct device *, struct cfdata *, void *));
void	dmaattach_sbus	__P((struct device *, struct device *, void *));
int	dmamatch_obio	__P((struct device *, struct cfdata *, void *));
void	dmaattach_obio	__P((struct device *, struct device *, void *));

void	dma_identify	__P((struct dma_softc *));

int	dmaprint	__P((void *, const char *));
void	dma_reset	__P((struct dma_softc *, int));
void	espdma_reset	__P((struct dma_softc *));
void	ledma_reset	__P((struct dma_softc *));
void	dma_enintr	__P((struct dma_softc *));
int	dma_isintr	__P((struct dma_softc *));
int	espdmaintr	__P((void *));
int	ledmaintr	__P((void *));
int	dma_setup	__P((struct dma_softc *, caddr_t *, size_t *,
			     int, size_t *));
void	dma_go		__P((struct dma_softc *));

void	*dmabus_intr_establish __P((
		bus_space_tag_t,
		int,			/*level*/
		int,			/*flags*/
		int (*) __P((void *)),	/*handler*/
		void *));		/*handler arg*/

static	bus_space_tag_t dma_alloc_bustag __P((struct dma_softc *sc));

struct cfattach dma_sbus_ca = {
	sizeof(struct dma_softc), dmamatch_sbus, dmaattach_sbus
};

struct cfattach ledma_ca = {
	sizeof(struct dma_softc), dmamatch_sbus, dmaattach_sbus
};

struct cfattach dma_obio_ca = {
	sizeof(struct dma_softc), dmamatch_obio, dmaattach_obio
};

int
dmaprint(aux, busname)
	void *aux;
	const char *busname;
{
	struct sbus_attach_args *sa = aux;
	bus_space_tag_t t = sa->sa_bustag;
	struct dma_softc *sc = t->cookie;

	sa->sa_bustag = sc->sc_bustag;	/* XXX */
	sbus_print(aux, busname);	/* XXX */
	sa->sa_bustag = t;		/* XXX */
	return (UNCONF);
}

int
dmamatch_sbus(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct sbus_attach_args *sa = aux;

	return (strcmp(cf->cf_driver->cd_name, sa->sa_name) == 0 ||
		strcmp("espdma", sa->sa_name) == 0);
}

int
dmamatch_obio(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	union obio_attach_args *uoba = aux;
	struct obio4_attach_args *oba;

	if (uoba->uoba_isobio4 == 0)
		return (0);

	oba = &uoba->uoba_oba4;
	return (bus_space_probe(oba->oba_bustag, 0, oba->oba_paddr,
				4,	/* probe size */
				0,	/* offset */
				0,	/* flags */
				NULL, NULL));
}

void
dmaattach_sbus(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbus_attach_args *sa = aux;
	struct dma_softc *sc = (void *)self;
	bus_space_handle_t bh;
	struct bootpath *bp;
	bus_space_tag_t sbt;
	int sbusburst;
	int node;

	sc->sc_bustag = sa->sa_bustag;
	sc->sc_dmatag = sa->sa_dmatag;

	/* Map registers */
	if (sa->sa_npromvaddrs != 0)
		sc->sc_regs = (struct dma_regs *)sa->sa_promvaddrs[0];
	else {
		if (sbus_bus_map(sa->sa_bustag, sa->sa_slot,
				 sa->sa_offset,
				 sizeof(struct dma_regs),
				 0, 0, &bh) != 0) {
			printf("dmaattach_sbus: cannot map registers\n");
			return;
		}
		sc->sc_regs = (struct dma_regs *)bh;
	}

	/*
	 * Get transfer burst size from PROM and plug it into the
	 * controller registers. This is needed on the Sun4m; do
	 * others need it too?
	 */
	sbusburst = ((struct sbus_softc *)parent)->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1; /* 1->16 */

	sc->sc_burst = getpropint(sa->sa_node,"burst-sizes", -1);
	if (sc->sc_burst == -1)
		/* take SBus burst sizes */
		sc->sc_burst = sbusburst;

	/* Clamp at parent's burst sizes */
	sc->sc_burst &= sbusburst;

	if (sc->sc_dev.dv_cfdata->cf_attach == &ledma_ca) {
		char *cabletype;
		/*
		 * Check to see which cable type is currently active and set the
		 * appropriate bit in the ledma csr so that it gets used. If we
		 * didn't netboot, the PROM won't have the "cable-selection"
		 * property; default to TP and then the user can change it via
		 * a "media" option to ifconfig.
		 */
		cabletype = getpropstring(sa->sa_node, "cable-selection");
		if (strcmp(cabletype, "tpe") == 0) {
			sc->sc_regs->csr |= DE_AUI_TP;
		} else if (strcmp(cabletype, "aui") == 0) {
			sc->sc_regs->csr &= ~DE_AUI_TP;
		} else {
			/* assume TP if nothing there */
			sc->sc_regs->csr |= DE_AUI_TP;
		}
		delay(20000);	/* manual says we need 20ms delay */
	}


	/* Propagate bootpath */
	bp = NULL;
	if (sa->sa_bp != NULL) {
		char *bpname = sa->sa_bp->name;
		if (strcmp(bpname, "espdma") == 0)
			/* We call everything "dma" */
			bpname = "dma";

		if (strcmp(bpname, self->dv_cfdata->cf_driver->cd_name) == 0)
			bp = sa->sa_bp + 1;
	}

	/* Allocate a dmamap */
	if (bus_dmamap_create(sc->sc_dmatag, 16*1024*1024, 1, 16*1024*1024,
			  0, BUS_DMA_WAITOK, &sc->sc_dmamap) != 0) {
		printf("%s: dma map create failed\n", self->dv_xname);
		return;
	}

	sbus_establish(&sc->sc_sd, &sc->sc_dev);
	sbt = dma_alloc_bustag(sc);
	dma_identify(sc);

	/* Attach children */
	for (node = firstchild(sa->sa_node); node; node = nextsibling(node)) {
		struct sbus_attach_args sa;
		sbus_setup_attach_args((struct sbus_softc *)parent,
				       sbt, sc->sc_dmatag, node, bp, &sa);
		(void) config_found(&sc->sc_dev, (void *)&sa, dmaprint);
		sbus_destroy_attach_args(&sa);
	}
}

void
dmaattach_obio(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	union obio_attach_args *uoba = aux;
	struct obio4_attach_args *oba = &uoba->uoba_oba4;
	struct dma_softc *sc = (void *)self;
	bus_space_handle_t bh;

	sc->sc_bustag = oba->oba_bustag;
	sc->sc_dmatag = oba->oba_dmatag;

	if (obio_bus_map(oba->oba_bustag, oba->oba_paddr,
			 0, sizeof(struct dma_regs),
			 0, 0,
			 &bh) != 0) {
		printf("dmaattach_obio: cannot map registers\n");
		return;
	}
	sc->sc_regs = (struct dma_regs *)bh;

	dma_identify(sc);
}

/*
 * Attach all the sub-devices we can find
 */
void
dma_identify(sc)
	struct dma_softc *sc;
{

	printf(": rev ");
	sc->sc_rev = sc->sc_regs->csr & D_DEV_ID;
	switch (sc->sc_rev) {
	case DMAREV_0:
		printf("0");
		break;
	case DMAREV_ESC:
		printf("esc");
		break;
	case DMAREV_1:
		printf("1");
		break;
	case DMAREV_PLUS:
		printf("1+");
		break;
	case DMAREV_2:
		printf("2");
		break;
	default:
		printf("unknown (0x%x)", sc->sc_rev);
	}
	printf("\n");

	/* indirect functions */
	if (sc->sc_dev.dv_cfdata->cf_attach == &ledma_ca) {
		sc->reset = ledma_reset;
		sc->intr = ledmaintr;
	} else {
		sc->reset = espdma_reset;
		sc->intr = espdmaintr;
	}
	sc->enintr = dma_enintr;
	sc->isintr = dma_isintr;
	sc->setup = dma_setup;
	sc->go = dma_go;
}

void *
dmabus_intr_establish(t, level, flags, handler, arg)
	bus_space_tag_t t;
	int level;
	int flags;
	int (*handler) __P((void *));
	void *arg;
{
	struct dma_softc *sc = t->cookie;

	if (sc->intr == ledmaintr) { /* XXX - for now; do esp later */
		sc->sc_intrchain = handler;
		sc->sc_intrchainarg = arg;
		handler = ledmaintr;
		arg = sc;
	}
	return (bus_intr_establish(sc->sc_bustag, level, flags, handler, arg));
}




#define DMAWAIT(SC, COND, MSG, DONTPANIC) do if (COND) {		\
	int count = 500000;						\
	while ((COND) && --count > 0) DELAY(1);				\
	if (count == 0) {						\
		printf("%s: line %d: CSR = 0x%lx\n", __FILE__, __LINE__, \
			(SC)->sc_regs->csr);				\
		if (DONTPANIC)						\
			printf(MSG);					\
		else							\
			panic(MSG);					\
	}								\
} while (0)

#define DMA_DRAIN(sc, dontpanic) do {					\
	/*								\
	 * DMA rev0 & rev1: we are not allowed to touch the DMA "flush"	\
	 *     and "drain" bits while it is still thinking about a	\
	 *     request.							\
	 * other revs: D_R_PEND bit reads as 0				\
	 */								\
	DMAWAIT(sc, sc->sc_regs->csr & D_R_PEND, "R_PEND", dontpanic);	\
	/*								\
	 * Select drain bit based on revision				\
	 * also clears errors and D_TC flag				\
	 */								\
	if (sc->sc_rev == DMAREV_1 || sc->sc_rev == DMAREV_0)		\
		DMACSR(sc) |= D_DRAIN;					\
	else								\
		DMACSR(sc) |= D_INVALIDATE;				\
	/*								\
	 * Wait for draining to finish					\
	 *  rev0 & rev1 call this PACKCNT				\
	 */								\
	DMAWAIT(sc, sc->sc_regs->csr & D_DRAINING, "DRAINING", dontpanic);\
} while(0)

#define DMA_FLUSH(sc, dontpanic) do {					\
	int csr;							\
	/*								\
	 * DMA rev0 & rev1: we are not allowed to touch the DMA "flush"	\
	 *     and "drain" bits while it is still thinking about a	\
	 *     request.							\
	 * other revs: D_R_PEND bit reads as 0				\
	 */								\
	DMAWAIT(sc, sc->sc_regs->csr & D_R_PEND, "R_PEND", dontpanic);	\
	csr = DMACSR(sc);						\
	csr &= ~(D_WRITE|D_EN_DMA);					\
	csr |= D_INVALIDATE;						\
	DMACSR(sc) = csr;						\
} while(0)

void
dma_reset(sc, isledma)
	struct dma_softc *sc;
	int isledma;
{
	DMA_FLUSH(sc, 1);
	DMACSR(sc) |= D_RESET;			/* reset DMA */
	DELAY(200);				/* what should this be ? */
	/*DMAWAIT1(sc); why was this here? */
	DMACSR(sc) &= ~D_RESET;			/* de-assert reset line */
	DMACSR(sc) |= D_INT_EN;			/* enable interrupts */
	if (sc->sc_rev > DMAREV_1 && isledma == 0)
		DMACSR(sc) |= D_FASTER;

	switch (sc->sc_rev) {
	case DMAREV_2:
		sc->sc_regs->csr &= ~D_BURST_SIZE; /* must clear first */
		if (sc->sc_burst & SBUS_BURST_32) {
			DMACSR(sc) |= D_BURST_32;
		} else if (sc->sc_burst & SBUS_BURST_16) {
			DMACSR(sc) |= D_BURST_16;
		} else {
			DMACSR(sc) |= D_BURST_0;
		}
		break;
	case DMAREV_ESC:
		DMACSR(sc) |= D_AUTODRAIN;	/* Auto-drain */
		if (sc->sc_burst & SBUS_BURST_32) {
			DMACSR(sc) &= ~0x800;
		} else
			DMACSR(sc) |= 0x800;
		break;
	default:
	}

	sc->sc_active = 0;			/* and of course we aren't */
}

void
espdma_reset(sc)
	struct dma_softc *sc;
{
	dma_reset(sc, 0);
}

void
ledma_reset(sc)
	struct dma_softc *sc;
{
	dma_reset(sc, 1);
}

void
dma_enintr(sc)
	struct dma_softc *sc;
{
	sc->sc_regs->csr |= D_INT_EN;
}

int
dma_isintr(sc)
	struct dma_softc *sc;
{
	return (sc->sc_regs->csr & (D_INT_PEND|D_ERR_PEND));
}

#define DMAMAX(a)	(0x01000000 - ((a) & 0x00ffffff))


/*
 * setup a dma transfer
 */
int
dma_setup(sc, addr, len, datain, dmasize)
	struct dma_softc *sc;
	caddr_t *addr;
	size_t *len;
	int datain;
	size_t *dmasize;	/* IN-OUT */
{
	u_long csr;

	DMA_FLUSH(sc, 0);

#if 0
	DMACSR(sc) &= ~D_INT_EN;
#endif
	sc->sc_dmaaddr = addr;
	sc->sc_dmalen = len;

	NCR_DMA(("%s: start %d@%p,%d\n", sc->sc_dev.dv_xname,
		*sc->sc_dmalen, *sc->sc_dmaaddr, datain ? 1 : 0));

	/*
	 * the rules say we cannot transfer more than the limit
	 * of this DMA chip (64k for old and 16Mb for new),
	 * and we cannot cross a 16Mb boundary.
	 */
	*dmasize = sc->sc_dmasize =
		min(*dmasize, DMAMAX((size_t) *sc->sc_dmaaddr));

	NCR_DMA(("dma_setup: dmasize = %d\n", sc->sc_dmasize));

	/* Program the DMA address */
	if (sc->sc_dmasize) {
		sc->sc_dvmaaddr = *sc->sc_dmaaddr;
		if (bus_dmamap_load(sc->sc_dmatag, sc->sc_dmamap,
				*sc->sc_dmaaddr, sc->sc_dmasize,
				NULL /* kernel address */,   
				BUS_DMA_NOWAIT))
			panic("dma: cannot allocate DVMA address");
		DMADDR(sc) = (caddr_t)sc->sc_dmamap->dm_segs[0].ds_addr;
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_dmamap,
				(bus_addr_t)sc->sc_dvmaaddr, sc->sc_dmasize,
				datain
					? BUS_DMASYNC_PREREAD
					: BUS_DMASYNC_PREWRITE);
	}

	if (sc->sc_rev == DMAREV_ESC) {
		/* DMA ESC chip bug work-around */
		register long bcnt = sc->sc_dmasize;
		register long eaddr = bcnt + (long)*sc->sc_dmaaddr;
		if ((eaddr & PGOFSET) != 0)
			bcnt = roundup(bcnt, NBPG);
		DMACNT(sc) = bcnt;
	}
	/* Setup DMA control register */
	csr = DMACSR(sc);
	if (datain)
		csr |= D_WRITE;
	else
		csr &= ~D_WRITE;
	csr |= D_INT_EN;
	DMACSR(sc) = csr;

	return 0;
}

void
dma_go(sc)
	struct dma_softc *sc;
{

	/* Start DMA */
	DMACSR(sc) |= D_EN_DMA;
	sc->sc_active = 1;
}

/*
 * Pseudo (chained) interrupt from the esp driver to kick the
 * current running DMA transfer. I am replying on espintr() to
 * pickup and clean errors for now
 *
 * return 1 if it was a DMA continue.
 */
int
espdmaintr(arg)
	void *arg;
{
	struct dma_softc *sc = arg;
	struct ncr53c9x_softc *nsc = &sc->sc_esp->sc_ncr53c9x;
	char bits[64];
	int trans, resid;
	u_long csr;
	csr = DMACSR(sc);

	NCR_DMA(("%s: intr: addr %p, csr %s\n", sc->sc_dev.dv_xname,
		 DMADDR(sc), bitmask_snprintf(csr, DMACSRBITS, bits,
		 sizeof(bits))));

	if (csr & D_ERR_PEND) {
		DMACSR(sc) &= ~D_EN_DMA;	/* Stop DMA */
		DMACSR(sc) |= D_INVALIDATE;
		printf("%s: error: csr=%s\n", sc->sc_dev.dv_xname,
			bitmask_snprintf(csr, DMACSRBITS, bits, sizeof(bits)));
		return -1;
	}

	/* This is an "assertion" :) */
	if (sc->sc_active == 0)
		panic("dmaintr: DMA wasn't active");

	DMA_DRAIN(sc, 0);

	/* DMA has stopped */
	DMACSR(sc) &= ~D_EN_DMA;
	sc->sc_active = 0;

	if (sc->sc_dmasize == 0) {
		/* A "Transfer Pad" operation completed */
		NCR_DMA(("dmaintr: discarded %d bytes (tcl=%d, tcm=%d)\n",
			NCR_READ_REG(nsc, NCR_TCL) |
				(NCR_READ_REG(nsc, NCR_TCM) << 8),
			NCR_READ_REG(nsc, NCR_TCL),
			NCR_READ_REG(nsc, NCR_TCM)));
		return 0;
	}

	resid = 0;
	/*
	 * If a transfer onto the SCSI bus gets interrupted by the device
	 * (e.g. for a SAVEPOINTER message), the data in the FIFO counts
	 * as residual since the ESP counter registers get decremented as
	 * bytes are clocked into the FIFO.
	 */
	if (!(csr & D_WRITE) &&
	    (resid = (NCR_READ_REG(nsc, NCR_FFLAG) & NCRFIFO_FF)) != 0) {
		NCR_DMA(("dmaintr: empty esp FIFO of %d ", resid));
	}

	if ((nsc->sc_espstat & NCRSTAT_TC) == 0) {
		/*
		 * `Terminal count' is off, so read the residue
		 * out of the ESP counter registers.
		 */
		resid += (NCR_READ_REG(nsc, NCR_TCL) |
			  (NCR_READ_REG(nsc, NCR_TCM) << 8) |
			   ((nsc->sc_cfg2 & NCRCFG2_FE)
				? (NCR_READ_REG(nsc, NCR_TCH) << 16)
				: 0));

		if (resid == 0 && sc->sc_dmasize == 65536 &&
		    (nsc->sc_cfg2 & NCRCFG2_FE) == 0)
			/* A transfer of 64K is encoded as `TCL=TCM=0' */
			resid = 65536;
	}

	trans = sc->sc_dmasize - resid;
	if (trans < 0) {			/* transferred < 0 ? */
#if 0
		/*
		 * This situation can happen in perfectly normal operation
		 * if the ESP is reselected while using DMA to select
		 * another target.  As such, don't print the warning.
		 */
		printf("%s: xfer (%d) > req (%d)\n",
		    sc->sc_dev.dv_xname, trans, sc->sc_dmasize);
#endif
		trans = sc->sc_dmasize;
	}

	NCR_DMA(("dmaintr: tcl=%d, tcm=%d, tch=%d; trans=%d, resid=%d\n",
		NCR_READ_REG(nsc, NCR_TCL),
		NCR_READ_REG(nsc, NCR_TCM),
		(nsc->sc_cfg2 & NCRCFG2_FE)
			? NCR_READ_REG(nsc, NCR_TCH) : 0,
		trans, resid));

	if (csr & D_WRITE)
		/*XXX - flush in dmamap_load() */
		cpuinfo.cache_flush(*sc->sc_dmaaddr, trans);

	if (sc->sc_dmamap->dm_nsegs > 0) {
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_dmamap,
				(bus_addr_t)sc->sc_dvmaaddr, sc->sc_dmasize,
				(csr & D_WRITE) != 0
					? BUS_DMASYNC_POSTREAD
					: BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmatag, sc->sc_dmamap);
	}

	*sc->sc_dmalen -= trans;
	*sc->sc_dmaaddr += trans;

#if 0	/* this is not normal operation just yet */
	if (*sc->sc_dmalen == 0 ||
	    nsc->sc_phase != nsc->sc_prevphase)
		return 0;

	/* and again */
	dma_start(sc, sc->sc_dmaaddr, sc->sc_dmalen, DMACSR(sc) & D_WRITE);
	return 1;
#endif
	return 0;
}

/*
 * Pseudo (chained) interrupt to le driver to handle DMA errors.
 */
int
ledmaintr(arg)
	void	*arg;
{
	struct dma_softc *sc = arg;
	char bits[64];
	u_long csr;
static int dodrain=0;

	csr = DMACSR(sc);

	if (csr & D_ERR_PEND) {
		DMACSR(sc) &= ~D_EN_DMA;	/* Stop DMA */
		DMACSR(sc) |= D_INVALIDATE;
		printf("%s: error: csr=%s\n", sc->sc_dev.dv_xname,
			bitmask_snprintf(csr, DMACSRBITS, bits, sizeof(bits)));
		DMA_RESET(sc);
		dodrain = 1;
	}

	if (dodrain) {	/* XXX - is this necessary with D_DSBL_WRINVAL on? */
#define E_DRAIN 0x400 /* XXX: fix dmareg.h */
		int i = 10;
		while (i-- > 0 && (sc->sc_regs->csr & D_DRAINING))
			delay(1);
	}

	return (*sc->sc_intrchain)(sc->sc_intrchainarg);
}

bus_space_tag_t
dma_alloc_bustag(sc)
	struct dma_softc *sc;
{
	bus_space_tag_t sbt;

	sbt = (bus_space_tag_t)
		malloc(sizeof(struct sparc_bus_space_tag), M_DEVBUF, M_NOWAIT);
	if (sbt == NULL)
		return (NULL);

	bzero(sbt, sizeof *sbt);
	sbt->cookie = sc;
	sbt->parent = sc->sc_bustag;
	sbt->sparc_intr_establish = dmabus_intr_establish;
	return (sbt);
}
