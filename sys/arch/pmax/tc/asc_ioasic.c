/* $NetBSD: asc_ioasic.c,v 1.1.2.15 2000/03/14 09:59:36 nisimura Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: asc_ioasic.c,v 1.1.2.15 2000/03/14 09:59:36 nisimura Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_message.h>

#include <machine/bus.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>
#include <dev/tc/ioasicreg.h>

struct asc_softc {
	struct ncr53c9x_softc sc_ncr53c9x;	/* glue to MI code */
	bus_space_tag_t sc_bst;
	bus_space_handle_t sc_bsh;
	bus_space_handle_t sc_scsi_bsh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmamap;
	caddr_t *sc_dmaaddr;
	size_t	*sc_dmalen;
	size_t	sc_dmasize;
	int	sc_active;			/* DMA active ? */
	int	sc_ispullup;			/* DMA into main memory? */
};

static int  asc_ioasic_match __P((struct device *, struct cfdata *, void *));
static void asc_ioasic_attach __P((struct device *, struct device *, void *));

struct cfattach asc_ioasic_ca = {
	sizeof(struct asc_softc), asc_ioasic_match, asc_ioasic_attach
};

static struct scsipi_device asc_ioasic_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

static u_char	asc_read_reg __P((struct ncr53c9x_softc *, int));
static void	asc_write_reg __P((struct ncr53c9x_softc *, int, u_char));
static int	asc_dma_isintr __P((struct ncr53c9x_softc *sc));
static void	asc_ioasic_reset __P((struct ncr53c9x_softc *));
static int	asc_ioasic_intr __P((struct ncr53c9x_softc *));
static int	asc_ioasic_setup __P((struct ncr53c9x_softc *,
				caddr_t *, size_t *, int, size_t *));
static void	asc_ioasic_go __P((struct ncr53c9x_softc *));
static void	asc_ioasic_stop __P((struct ncr53c9x_softc *));
static int	asc_dma_isactive __P((struct ncr53c9x_softc *));
static void	asc_clear_latched_intr __P((struct ncr53c9x_softc *));

static struct ncr53c9x_glue asc_ioasic_glue = {
	asc_read_reg,
	asc_write_reg,
	asc_dma_isintr,
	asc_ioasic_reset,
	asc_ioasic_intr,
	asc_ioasic_setup,
	asc_ioasic_go,
	asc_ioasic_stop,
	asc_dma_isactive,
	asc_clear_latched_intr,
};

static int
asc_ioasic_match(parent, cfdata, aux)
	struct device *parent;
	struct cfdata *cfdata;
	void *aux;
{
	struct ioasicdev_attach_args *d = aux;
	
	if (strncmp("asc", d->iada_modname, TC_ROM_LLEN))
		return 0;

	return 1;
}

static void
asc_ioasic_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ioasicdev_attach_args *d = aux;
	struct asc_softc *asc = (struct asc_softc *)self;	
	struct ncr53c9x_softc *sc = &asc->sc_ncr53c9x;
/* XXX Hook into dk_establish() to determine boot device */
 	extern int booted_slot;		/* TC slot of boot device */
 	extern struct device *booted_controller;
 
 	/* Is this the controller we booted from? */
 	if (booted_slot == 3)		/* IOASIC always resides in TC slot 3 */
 		booted_controller = self;
/* XXX */

	/*
	 * Set up glue for MI code early; we use some of it here.
	 */
	sc->sc_glue = &asc_ioasic_glue;
	asc->sc_bst = ((struct ioasic_softc *)parent)->sc_bst;
	asc->sc_bsh = ((struct ioasic_softc *)parent)->sc_bsh;
	asc->sc_dmat = ((struct ioasic_softc *)parent)->sc_dmat;
	if (bus_space_subregion(asc->sc_bst,
			asc->sc_bsh,
			IOASIC_SLOT_12_START, 0x100, &asc->sc_scsi_bsh)) {
		printf("%s: unable to map device\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_id = 7;
	sc->sc_freq = 25000000;

	/* gimme Mhz */
	sc->sc_freq /= 1000000;

	ioasic_intr_establish(parent, d->iada_cookie, TC_IPL_BIO,
		(int (*)(void *))ncr53c9x_intr, sc);

	/*
	 * XXX More of this should be in ncr53c9x_attach(), but
	 * XXX should we really poke around the chip that much in
	 * XXX the MI code?  Think about this more...
	 */

	/*
	 * Set up static configuration info.
	 */
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB;
	sc->sc_cfg2 = NCRCFG2_SCSI2;
	sc->sc_cfg3 = 0;
	sc->sc_rev = NCR_VARIANT_NCR53C94;

	/*
	 * XXX minsync and maxxfer _should_ be set up in MI code,
	 * XXX but it appears to have some dependency on what sort
	 * XXX of DMA we're hooked up to, etc.
	 */

	/*
	 * This is the value used to start sync negotiations
	 * Note that the NCR register "SYNCTP" is programmed
	 * in "clocks per byte", and has a minimum value of 4.
	 * The SCSI period used in negotiation is one-fourth
	 * of the time (in nanoseconds) needed to transfer one byte.
	 * Since the chip's clock is given in MHz, we have the following
	 * formula: 4 * period = (1000 / freq) * 4
	 */
	sc->sc_minsync = (1000 / sc->sc_freq) * 5 / 4 ;
	sc->sc_maxxfer = 64 * 1024;

	/* Do the common parts of attachment. */
	sc->sc_adapter.scsipi_cmd = ncr53c9x_scsi_cmd;
	sc->sc_adapter.scsipi_minphys = minphys;
	ncr53c9x_attach(sc, &asc_ioasic_dev);
}

void
asc_ioasic_reset(sc)
	struct ncr53c9x_softc *sc;
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	u_int32_t ssr;

	ssr = bus_space_read_4(asc->sc_bst, asc->sc_bsh, IOASIC_CSR);
	ssr &= ~IOASIC_CSR_DMAEN_SCSI;
	bus_space_write_4(asc->sc_bst, asc->sc_bsh, IOASIC_CSR, ssr);
	bus_space_write_4(asc->sc_bst, asc->sc_bsh, IOASIC_SCSI_SCR, 0);
	asc->sc_active = 0;
}

#define	SCRDEBUG(x)

static int
asc_ioasic_intr(sc)
	struct ncr53c9x_softc *sc;
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	int trans, resid;
	u_int tcl, tcm, ssr, scr, intr;
	
	if (asc->sc_active == 0)
		panic("dmaintr: DMA wasn't active");

#define	IOASIC_ASC_ERRORS \
    (IOASIC_INTR_SCSI_PTR_LOAD|IOASIC_INTR_SCSI_OVRUN|IOASIC_INTR_SCSI_READ_E)
	/*
	 * When doing polled I/O, the SCSI bits in the interrupt register won't
	 * get cleared by the interrupt processing.  This will cause the DMA
	 * address registers to not load on the next DMA transfer.
	 * Check for these bits here, and clear them if needed.
	 */
	intr = bus_space_read_4(asc->sc_bst, asc->sc_bsh, IOASIC_INTR);
	if ((intr & IOASIC_ASC_ERRORS) != 0)
		bus_space_write_4(asc->sc_bst, asc->sc_bsh, IOASIC_INTR,
		    intr & ~IOASIC_ASC_ERRORS);

	/* DMA has stopped */
	ssr = bus_space_read_4(asc->sc_bst, asc->sc_bsh, IOASIC_CSR);
	ssr &= ~IOASIC_CSR_DMAEN_SCSI;
	bus_space_write_4(asc->sc_bst, asc->sc_bsh, IOASIC_CSR, ssr);
	asc->sc_active = 0;	

	if (asc->sc_dmasize == 0) {
		/* A "Transfer Pad" operation completed */
		tcl = NCR_READ_REG(sc, NCR_TCL); 
		tcm = NCR_READ_REG(sc, NCR_TCM);
		NCR_DMA(("ioasic_intr: discarded %d bytes (tcl=%d, tcm=%d)\n",
		    tcl | (tcm << 8), tcl, tcm));
		return 0;
	}

	resid = 0;
	if (!asc->sc_ispullup &&
	    (resid = (NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF)) != 0) {
		NCR_DMA(("ioasic_intr: empty FIFO of %d ", resid));
		DELAY(1);
	}

	resid += (tcl = NCR_READ_REG(sc, NCR_TCL));
	resid += (tcm = NCR_READ_REG(sc, NCR_TCM)) << 8;

	trans = asc->sc_dmasize - resid;
	if (trans < 0) {			/* transferred < 0 ? */
		printf("ioasic_intr: xfer (%d) > req (%d)\n",
		    trans, asc->sc_dmasize);
		trans = asc->sc_dmasize;
	}
	NCR_DMA(("ioasic_intr: tcl=%d, tcm=%d; trans=%d, resid=%d\n",
	    tcl, tcm, trans, resid));

	scr = bus_space_read_4(asc->sc_bst, asc->sc_bsh, IOASIC_SCSI_SCR);
	if (asc->sc_ispullup && scr != 0) {
		u_int32_t ptr;
		u_int16_t *p;
		union {
			u_int32_t sdr[2];
			u_int16_t half[4];
		} scratch;
		scratch.sdr[0] = bus_space_read_4(asc->sc_bst, asc->sc_bsh,
						IOASIC_SCSI_SDR0);
		scratch.sdr[1] = bus_space_read_4(asc->sc_bst, asc->sc_bsh,
						IOASIC_SCSI_SDR1);
		ptr = bus_space_read_4(asc->sc_bst, asc->sc_bsh,
						IOASIC_SCSI_DMAPTR);
		ptr = (ptr >> 3) & 0x1ffffffc;
SCRDEBUG(("SCSI_SCR -> %x, DMAPTR: %p\n", scr, (void *)ptr));
		p = (u_int16_t *)MIPS_PHYS_TO_KSEG0(ptr);
		/*
		 * scr
		 *	1 -> half[0]
		 *	2 -> half[0] + half[1]
		 *	3 -> half[0] + half[1] + half[2]
		 */
		scr &= IOASIC_SCR_WORD;
		p[0] = scratch.half[0];
		if (scr > 1)
			p[1] = scratch.half[1];
		if (scr > 2)
			p[2] = scratch.half[2];
	}

	*asc->sc_dmalen -= trans;
	*asc->sc_dmaaddr += trans;
	
	return 0;
}

#define	TWOPAGE(a)	(NBPG*2 - ((a) & (NBPG-1)))

int
asc_ioasic_setup(sc, addr, len, datain, dmasize)
	struct ncr53c9x_softc *sc;
	caddr_t *addr;
	size_t *len;
	int datain;
	size_t *dmasize;
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	u_int32_t ssr, scr;
	size_t size;
	vaddr_t cp;
	paddr_t ptr0, ptr1;
	extern paddr_t kvtophys __P((vaddr_t));

	asc->sc_dmaaddr = addr;
	asc->sc_dmalen = len;
	asc->sc_ispullup = datain;

	NCR_DMA(("ioasic_setup: start %d@%p %s\n",
		*asc->sc_dmalen, *asc->sc_dmaaddr, datain ? "IN" : "OUT"));

	/* upto two 4KB pages */
	size = min(*dmasize, TWOPAGE((size_t)*addr));
	*dmasize = asc->sc_dmasize = size;

	NCR_DMA(("ioasic_setup: dmasize = %d\n", asc->sc_dmasize));

	/* stop DMA engine first */
	ssr = bus_space_read_4(asc->sc_bst, asc->sc_bsh, IOASIC_CSR);
	ssr &= ~IOASIC_CSR_DMAEN_SCSI;
	bus_space_write_4(asc->sc_bst, asc->sc_bsh, IOASIC_CSR, ssr);

	/* If R4K, writeback and invalidate the buffer */
	if (CPUISMIPS3)
		mips3_HitFlushDCache((vaddr_t)*addr, size);

	cp = (vaddr_t)*addr;
	if ((cp & 7) == 0)
		scr = 0;
	else {
		u_int32_t *p;

		p = (u_int32_t *)(cp & ~7);
		bus_space_write_4(asc->sc_bst, asc->sc_bsh,
					IOASIC_SCSI_SDR0, p[0]);
		bus_space_write_4(asc->sc_bst, asc->sc_bsh,
					IOASIC_SCSI_SDR1, p[1]);

		scr = (cp >> 1) & 3;
		cp &= ~7;
		if (asc->sc_ispullup == 0) {
			scr |= 4;
			cp += 8;
		}
SCRDEBUG(("SCSI_SCR <- %x, DMAPTR: %p\n", scr, (void *)kvtophys(cp)));
	}
	ptr0 = kvtophys(cp);
	cp = mips_trunc_page(cp + NBPG);
	ptr1 = ((vaddr_t)*addr + size > cp) ? kvtophys(cp) : ~0;

	/* If not R4K, need to invalidate cache lines for physical segments */
	if (!CPUISMIPS3 && datain) {
		if (ptr1 == ~0)
			MachFlushDCache(MIPS_PHYS_TO_KSEG0(ptr0), size);
		else {
			int size0 = NBPG - (ptr0 & (NBPG - 1));
			int size1 = size - size0;
			MachFlushDCache(MIPS_PHYS_TO_KSEG0(ptr0), size0);
			MachFlushDCache(MIPS_PHYS_TO_KSEG0(ptr1), size1);
		}
	}

	bus_space_write_4(asc->sc_bst, asc->sc_bsh, IOASIC_SCSI_SCR, scr);
	bus_space_write_4(asc->sc_bst, asc->sc_bsh,
				IOASIC_SCSI_DMAPTR, IOASIC_DMA_ADDR(ptr0));
	bus_space_write_4(asc->sc_bst, asc->sc_bsh,
				IOASIC_SCSI_NEXTPTR, IOASIC_DMA_ADDR(ptr1));
	return 0;
}

void
asc_ioasic_go(sc)
	struct ncr53c9x_softc *sc;
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	u_int32_t ssr;

	ssr = bus_space_read_4(asc->sc_bst, asc->sc_bsh, IOASIC_CSR);
	if (asc->sc_ispullup)
		ssr |= IOASIC_CSR_SCSI_DIR;
	else {
		/* ULTRIX does in this way */
		ssr &= ~IOASIC_CSR_SCSI_DIR;
		bus_space_write_4(asc->sc_bst, asc->sc_bsh, IOASIC_CSR, ssr);
		ssr = bus_space_read_4(asc->sc_bst, asc->sc_bsh, IOASIC_CSR);
	}
	ssr |= IOASIC_CSR_DMAEN_SCSI;
	bus_space_write_4(asc->sc_bst, asc->sc_bsh, IOASIC_CSR, ssr);
	asc->sc_active = 1;
}

/* NEVER CALLED BY MI 53C9x ENGINE INDEED */
void
asc_ioasic_stop(sc)
	struct ncr53c9x_softc *sc;
{
}

static u_char
asc_read_reg(sc, reg)
	struct ncr53c9x_softc *sc;
	int reg;
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	u_int32_t v;

	v = bus_space_read_4(asc->sc_bst,
		asc->sc_scsi_bsh, reg * sizeof(u_int32_t));

	return v & 0xff;
}

static void
asc_write_reg(sc, reg, val)
	struct ncr53c9x_softc *sc;
	int reg;
	u_char val;
{
	struct asc_softc *asc = (struct asc_softc *)sc;

	bus_space_write_4(asc->sc_bst,
		asc->sc_scsi_bsh, reg * sizeof(u_int32_t), val);
}

static int
asc_dma_isintr(sc)
	struct ncr53c9x_softc *sc;
{
	return !!(NCR_READ_REG(sc, NCR_STAT) & NCRSTAT_INT);
}

static int
asc_dma_isactive(sc)
	struct ncr53c9x_softc *sc;
{
	struct asc_softc *asc = (struct asc_softc *)sc;

	return asc->sc_active;
}

static void
asc_clear_latched_intr(sc)
	struct ncr53c9x_softc *sc;
{
}
