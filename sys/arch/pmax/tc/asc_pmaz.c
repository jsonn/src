/* $NetBSD: asc_pmaz.c,v 1.1.2.7 2000/03/14 09:59:37 nisimura Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: asc_pmaz.c,v 1.1.2.7 2000/03/14 09:59:37 nisimura Exp $");

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

struct asc_softc {
	struct ncr53c9x_softc sc_ncr53c9x;	/* glue to MI code */
	bus_space_tag_t sc_bst;
	bus_space_handle_t sc_bsh;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmamap;
	caddr_t *sc_dmaaddr;
	size_t	*sc_dmalen;
	size_t	sc_dmasize;
	int	sc_active;			/* DMA active ? */
	int	sc_ispullup;			/* DMA into main memory? */

	/* XXX XXX XXX */
	caddr_t sc_base, sc_bounce, sc_target;
};

static int  asc_pmaz_match __P((struct device *, struct cfdata *, void *));
static void asc_pmaz_attach __P((struct device *, struct device *, void *));

struct cfattach asc_pmaz_ca = {
	sizeof(struct asc_softc), asc_pmaz_match, asc_pmaz_attach
};

static struct scsipi_device asc_pmaz_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

static u_char	asc_read_reg __P((struct ncr53c9x_softc *, int));
static void	asc_write_reg __P((struct ncr53c9x_softc *, int, u_char));
static int	asc_dma_isintr __P((struct ncr53c9x_softc *));
static void	asc_pmaz_reset __P((struct ncr53c9x_softc *));
static int	asc_pmaz_intr __P((struct ncr53c9x_softc *));
static int	asc_pmaz_setup __P((struct ncr53c9x_softc *, caddr_t *,
						size_t *, int, size_t *));
static void	asc_pmaz_go __P((struct ncr53c9x_softc *));
static void	asc_pmaz_stop __P((struct ncr53c9x_softc *));
static int	asc_dma_isactive __P((struct ncr53c9x_softc *));
static void	asc_clear_latched_intr __P((struct ncr53c9x_softc *));

static struct ncr53c9x_glue asc_pmaz_glue = {
        asc_read_reg,
        asc_write_reg,
        asc_dma_isintr,
        asc_pmaz_reset,
        asc_pmaz_intr,
        asc_pmaz_setup,
        asc_pmaz_go,
        asc_pmaz_stop,
        asc_dma_isactive,
        asc_clear_latched_intr,
};

/*
 * Parameters specific to PMAZ-A TC option card.
 */
#define PMAZ_OFFSET_53C94	0x0		/* from module base */
#define PMAZ_OFFSET_DMAR	0x40000		/* DMA Address Register */
#define PMAZ_OFFSET_RAM		0x80000		/* 128KB SRAM buffer */
#define PMAZ_OFFSET_ROM		0xc0000		/* diagnostic ROM */

#define PMAZ_RAM_SIZE		0x20000		/* 128k (32k*32) */
#define PER_TGT_DMA_SIZE	((PMAZ_RAM_SIZE/7) & ~(sizeof(int)-1))

#define PMAZ_DMAR_WRITE		0x80000000	/* DMA direction bit */
#define PMAZ_DMAR_MASK		0x1ffff		/* 17 bits, 128k */
#define PMAZ_DMA_ADDR(x)	((unsigned)(x) & PMAZ_DMAR_MASK)

static int
asc_pmaz_match(parent, cfdata, aux)
	struct device *parent;
	struct cfdata *cfdata;
	void *aux;
{
	struct tc_attach_args *d = aux;
	
	if (strncmp("PMAZ-AA ", d->ta_modname, TC_ROM_LLEN))
		return (0);

	return (1);
}

static void
asc_pmaz_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct tc_attach_args *ta = aux;
	struct asc_softc *asc = (struct asc_softc *)self;	
	struct ncr53c9x_softc *sc = &asc->sc_ncr53c9x;
/* XXX Hook into dk_establish() to determine boot device */
	extern int booted_slot;		/* TC slot of boot device */
	extern struct device *booted_controller;

	/* Is this the controller we booted from? */
	if (booted_slot == ta->ta_slot)
		booted_controller = self;
/* XXX */

	/*
	 * Set up glue for MI code early; we use some of it here.
	 */
	sc->sc_glue = &asc_pmaz_glue;
	asc->sc_bst = ta->ta_memt;
	asc->sc_dmat = ta->ta_dmat;
	if (bus_space_map(asc->sc_bst, ta->ta_addr,
		PMAZ_OFFSET_RAM + PMAZ_RAM_SIZE, 0, &asc->sc_bsh)) {
		printf("%s: unable to map device\n", sc->sc_dev.dv_xname);
		return;
	}
	asc->sc_base = (caddr_t)ta->ta_addr;	/* XXX XXX XXX */

	tc_intr_establish(parent, ta->ta_cookie, IPL_BIO,
		(int (*)(void *))ncr53c9x_intr, sc);
	
	sc->sc_id = 7;
	sc->sc_freq = (ta->ta_busspeed) ? 25000000 : 12500000;

	/* gimme Mhz */
	sc->sc_freq /= 1000000;

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
	sc->sc_minsync = (1000 / sc->sc_freq) * 5 / 4;

	sc->sc_maxxfer = 64 * 1024;

	/* Do the common parts of attachment. */
	sc->sc_adapter.scsipi_cmd = ncr53c9x_scsi_cmd;
	sc->sc_adapter.scsipi_minphys = minphys;
	ncr53c9x_attach(sc, &asc_pmaz_dev);
}

static void
asc_pmaz_reset(sc)
	struct ncr53c9x_softc *sc;
{
	struct asc_softc *asc = (struct asc_softc *)sc;

	asc->sc_active = 0;
}

static int
asc_pmaz_intr(sc)
	struct ncr53c9x_softc *sc;
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	int trans, resid;

	resid = 0;
	if (!asc->sc_ispullup &&
	    (resid = (NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF)) != 0) {
		NCR_DMA(("pmaz_intr: empty FIFO of %d ", resid));
		DELAY(1);
	}

	resid += NCR_READ_REG(sc, NCR_TCL);
	resid += NCR_READ_REG(sc, NCR_TCM) << 8;

	trans = asc->sc_dmasize - resid;

	if (asc->sc_ispullup)
		memcpy(asc->sc_target, asc->sc_bounce, trans);
	*asc->sc_dmalen -= trans;
	*asc->sc_dmaaddr += trans;
	asc->sc_active = 0;

	return (0);
}

static int
asc_pmaz_setup(sc, addr, len, datain, dmasize)
	struct ncr53c9x_softc *sc;
	caddr_t *addr;
	size_t *len;
	int datain;
	size_t *dmasize;
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	u_int32_t tc_dmar;
	size_t size;

	asc->sc_dmaaddr = addr;
	asc->sc_dmalen = len;
	asc->sc_ispullup = datain;

	NCR_DMA(("pmaz_setup: start %d@%p, %s\n",
		*asc->sc_dmalen, *asc->sc_dmaaddr, datain ? "IN" : "OUT"));

	size = *dmasize;
	if (size > PER_TGT_DMA_SIZE)
		size = PER_TGT_DMA_SIZE;
	*dmasize = asc->sc_dmasize = size;

	NCR_DMA(("pmaz_setup: dmasize = %d\n", asc->sc_dmasize));

	asc->sc_bounce = asc->sc_base + PMAZ_OFFSET_RAM;
	asc->sc_bounce += PER_TGT_DMA_SIZE *
	    sc->sc_nexus->xs->sc_link->scsipi_scsi.target;
	asc->sc_target = *addr;

	if (!asc->sc_ispullup)
		memcpy(asc->sc_bounce, asc->sc_target, size);

#if 1
	if (asc->sc_ispullup)
		tc_dmar = PMAZ_DMA_ADDR(asc->sc_bounce);
	else
		tc_dmar = PMAZ_DMAR_WRITE | PMAZ_DMA_ADDR(asc->sc_bounce);
	bus_space_write_4(asc->sc_bst, asc->sc_bsh, PMAZ_OFFSET_DMAR, tc_dmar);
	asc->sc_active = 1;
#endif
	return (0);
}

static void
asc_pmaz_go(sc)
	struct ncr53c9x_softc *sc;
{
#if 0
	struct asc_softc *asc = (struct asc_softc *)sc;
	u_int32_t tc_dmar;

	if (asc->sc_ispullup)
		tc_dmar = PMAZ_DMA_ADDR(asc->sc_bounce);
	else
		tc_dmar = PMAZ_DMAR_WRITE | PMAZ_DMA_ADDR(asc->sc_bounce);
	bus_space_write_4(asc->sc_bst, asc->sc_bsh, PMAZ_OFFSET_DMAR, tc_dmar);
	asc->sc_active = 1;
#endif
}

/* NEVER CALLED BY MI 53C9x ENGINE INDEED */
static void
asc_pmaz_stop(sc)
	struct ncr53c9x_softc *sc;
{
#if 0
	struct asc_softc *asc = (struct asc_softc *)sc;

	if (asc->sc_ispullup)
		memcpy(asc->sc_target, asc->sc_bounce, asc->sc_dmasize);
	asc->sc_active = 0;
#endif
}

/*
 * Glue functions.
 */
static u_char
asc_read_reg(sc, reg)
	struct ncr53c9x_softc *sc;
	int reg;
{
	struct asc_softc *asc = (struct asc_softc *)sc;
	u_char v;

	v = bus_space_read_4(asc->sc_bst, asc->sc_bsh,
	    reg * sizeof(u_int32_t)) & 0xff;

	return (v);
}

static void
asc_write_reg(sc, reg, val)
	struct ncr53c9x_softc *sc;
	int reg;
	u_char val;
{
	struct asc_softc *asc = (struct asc_softc *)sc;

	bus_space_write_4(asc->sc_bst, asc->sc_bsh,
	    reg * sizeof(u_int32_t), val);
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

	return (asc->sc_active);
}

static void
asc_clear_latched_intr(sc)
	struct ncr53c9x_softc *sc;
{
}
