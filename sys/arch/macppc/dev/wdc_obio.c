/*	$NetBSD: wdc_obio.c,v 1.19.10.2 2003/06/24 09:21:52 grant Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Onno van der Linden.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/autoconf.h>

#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

#include <dev/ofw/openfirm.h>

#include <macppc/dev/dbdma.h>

#define WDC_REG_NPORTS		8
#define WDC_AUXREG_OFFSET	0x16
#define WDC_DEFAULT_PIO_IRQ	13	/* XXX */
#define WDC_DEFAULT_DMA_IRQ	2	/* XXX */

#define WDC_OPTIONS_DMA 0x01

/*
 * XXX This code currently doesn't even try to allow 32-bit data port use.
 */

struct wdc_obio_softc {
	struct wdc_softc sc_wdcdev;
	struct channel_softc *wdc_chanptr;
	struct channel_softc wdc_channel;
	dbdma_regmap_t *sc_dmareg;
	dbdma_command_t	*sc_dmacmd;
	u_int sc_dmaconf[2];	/* per target value of CONFIG_REG */
	void *sc_ih;
};

int wdc_obio_probe __P((struct device *, struct cfdata *, void *));
void wdc_obio_attach __P((struct device *, struct device *, void *));
int wdc_obio_detach __P((struct device *, int));
int wdc_obio_dma_init __P((void *, int, int, void *, size_t, int));
void wdc_obio_dma_start __P((void *, int, int));
int wdc_obio_dma_finish __P((void *, int, int, int));

static void wdc_obio_select __P((struct channel_softc *, int));
static void adjust_timing __P((struct channel_softc *));
static void ata4_adjust_timing __P((struct channel_softc *));

struct cfattach wdc_obio_ca = {
	sizeof(struct wdc_obio_softc), wdc_obio_probe, wdc_obio_attach,
	wdc_obio_detach, wdcactivate
};


int
wdc_obio_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct confargs *ca = aux;
	char compat[32];

	/* XXX should not use name */
	if (strcmp(ca->ca_name, "ATA") == 0 ||
	    strcmp(ca->ca_name, "ata") == 0 ||
	    strcmp(ca->ca_name, "ata0") == 0 ||
	    strcmp(ca->ca_name, "ide") == 0)
		return 1;

	memset(compat, 0, sizeof(compat));
	OF_getprop(ca->ca_node, "compatible", compat, sizeof(compat));
	if (strcmp(compat, "heathrow-ata") == 0 ||
	    strcmp(compat, "keylargo-ata") == 0)
		return 1;

	return 0;
}

void
wdc_obio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wdc_obio_softc *sc = (void *)self;
	struct confargs *ca = aux;
	struct channel_softc *chp = &sc->wdc_channel;
	int intr;
	int use_dma = 0;
	char path[80];

	if (sc->sc_wdcdev.sc_dev.dv_cfdata->cf_flags & WDC_OPTIONS_DMA) {
		if (ca->ca_nreg >= 16 || ca->ca_nintr == -1)
			use_dma = 1;	/* XXX Don't work yet. */
	}

	if (ca->ca_nintr >= 4 && ca->ca_nreg >= 8) {
		intr = ca->ca_intr[0];
		printf(" irq %d", intr);
	} else if (ca->ca_nintr == -1) {
		intr = WDC_DEFAULT_PIO_IRQ;
		printf(" irq property not found; using %d", intr);
	} else {
		printf(": couldn't get irq property\n");
		return;
	}

	if (use_dma)
		printf(": DMA transfer");

	printf("\n");

	chp->cmd_iot = chp->ctl_iot =
		macppc_make_bus_space_tag(ca->ca_baseaddr + ca->ca_reg[0], 4);

	if (bus_space_map(chp->cmd_iot, 0, WDC_REG_NPORTS, 0, &chp->cmd_ioh) ||
	    bus_space_subregion(chp->cmd_iot, chp->cmd_ioh,
			WDC_AUXREG_OFFSET, 1, &chp->ctl_ioh)) {
		printf("%s: couldn't map registers\n",
			sc->sc_wdcdev.sc_dev.dv_xname);
		return;
	}
#if 0
	chp->data32iot = chp->cmd_iot;
	chp->data32ioh = chp->cmd_ioh;
#endif

	sc->sc_ih = intr_establish(intr, IST_LEVEL, IPL_BIO, wdcintr, chp);

	if (use_dma) {
		sc->sc_dmacmd = dbdma_alloc(sizeof(dbdma_command_t) * 20);
		sc->sc_dmareg = mapiodev(ca->ca_baseaddr + ca->ca_reg[2],
					 ca->ca_reg[3]);
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA;
		sc->sc_wdcdev.DMA_cap = 2;
		if (strcmp(ca->ca_name, "ata-4") == 0) {
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
			sc->sc_wdcdev.UDMA_cap = 4;
			sc->sc_wdcdev.set_modes = ata4_adjust_timing;
		} else {
			sc->sc_wdcdev.set_modes = adjust_timing;
		}
#ifdef notyet
		/* Minimum cycle time is 150ns (DMA MODE 1) on ohare. */
		if (ohare) {
			sc->sc_wdcdev.PIO_cap = 3;
			sc->sc_wdcdev.DMA_cap = 1;
		}
#endif
	} else {
		/* all non-dma controllers can use adjust_timing */
		sc->sc_wdcdev.set_modes = adjust_timing;
	}

	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_MODE;
	sc->wdc_chanptr = chp;
	sc->sc_wdcdev.channels = &sc->wdc_chanptr;
	sc->sc_wdcdev.nchannels = 1;
	sc->sc_wdcdev.dma_arg = sc;
	sc->sc_wdcdev.dma_init = wdc_obio_dma_init;
	sc->sc_wdcdev.dma_start = wdc_obio_dma_start;
	sc->sc_wdcdev.dma_finish = wdc_obio_dma_finish;
	chp->channel = 0;
	chp->wdc = &sc->sc_wdcdev;
	chp->ch_queue = malloc(sizeof(struct channel_queue),
		M_DEVBUF, M_NOWAIT);
	if (chp->ch_queue == NULL) {
		printf("%s: can't allocate memory for command queue",
		sc->sc_wdcdev.sc_dev.dv_xname);
		return;
	}

#define OHARE_FEATURE_REG	0xf3000038

	/* XXX Enable wdc1 by feature reg. */
	memset(path, 0, sizeof(path));
	OF_package_to_path(ca->ca_node, path, sizeof(path));
	if (strcmp(path, "/bandit@F2000000/ohare@10/ata@21000") == 0) {
		u_int x;

		x = in32rb(OHARE_FEATURE_REG);
		x |= 8;
		out32rb(OHARE_FEATURE_REG, x);
	}

	wdcattach(chp);
	sc->sc_wdcdev.set_modes(chp);

}

/* Multiword DMA transfer timings */
struct ide_timings {
	int cycle;	/* minimum cycle time [ns] */
	int active;	/* minimum command active time [ns] */
};
static struct ide_timings pio_timing[5] = {
	{ 600, 180 },    /* Mode 0 */
	{ 390, 150 },    /*      1 */
	{ 240, 105 },    /*      2 */
	{ 180,  90 },    /*      3 */
	{ 120,  75 }     /*      4 */
};
static struct ide_timings dma_timing[3] = {
	{ 480, 240 },	/* Mode 0 */
	{ 165,  90 },	/* Mode 1 */
	{ 120,  75 }	/* Mode 2 */
};

static struct ide_timings udma_timing[5] = {
	{120, 180},	/* Mode 0 */
	{ 90, 150},	/* Mode 1 */
	{ 60, 120},	/* Mode 2 */
	{ 45, 90},	/* Mode 3 */
	{ 30, 90}	/* Mode 4 */
};

#define TIME_TO_TICK(time) howmany((time), 30)
#define PIO_REC_OFFSET 4
#define PIO_REC_MIN 1
#define PIO_ACT_MIN 1
#define DMA_REC_OFFSET 1
#define DMA_REC_MIN 1
#define DMA_ACT_MIN 1

#define ATA4_TIME_TO_TICK(time)  howmany((time), 15) /* 15 ns clock */

#define CONFIG_REG (0x200 >> 4)		/* IDE access timing register */

void
wdc_obio_select(chp, drive)
	struct channel_softc *chp;
	int drive;
{
	struct wdc_obio_softc *sc = (struct wdc_obio_softc *)chp->wdc;
	bus_space_write_4(chp->cmd_iot, chp->cmd_ioh,
			CONFIG_REG, sc->sc_dmaconf[drive]);
}

void
adjust_timing(chp)
	struct channel_softc *chp;
{
	struct wdc_obio_softc *sc = (struct wdc_obio_softc *)chp->wdc;
	int drive;
	int min_cycle, min_active;
	int cycle_tick, act_tick, inact_tick, half_tick;

	for (drive = 0; drive < 2; drive++) {
		u_int conf = 0;
		struct ata_drive_datas *drvp;
		
		drvp = &chp->ch_drive[drive];
		/* set up pio mode timings */
		if (drvp->drive_flags & DRIVE) {
			int piomode = drvp->PIO_mode;
			min_cycle = pio_timing[piomode].cycle;
			min_active = pio_timing[piomode].active;
			
			cycle_tick = TIME_TO_TICK(min_cycle);
			act_tick = TIME_TO_TICK(min_active);
			if (act_tick < PIO_ACT_MIN)
				act_tick = PIO_ACT_MIN;
			inact_tick = cycle_tick - act_tick - PIO_REC_OFFSET;
			if (inact_tick < PIO_REC_MIN)
				inact_tick = PIO_REC_MIN;
			/* mask: 0x000007ff */
			conf |= (inact_tick << 5) | act_tick;
		}
		/* Set up dma mode timings */
		if (drvp->drive_flags & DRIVE_DMA) {
			int dmamode = drvp->DMA_mode;
			min_cycle = dma_timing[dmamode].cycle;
			min_active = dma_timing[dmamode].active;
			cycle_tick = TIME_TO_TICK(min_cycle);
			act_tick = TIME_TO_TICK(min_active);
			inact_tick = cycle_tick - act_tick - DMA_REC_OFFSET;
			if (inact_tick < DMA_REC_MIN)
				inact_tick = DMA_REC_MIN;
			half_tick = 0;	/* XXX */
			/* mask: 0xfffff800 */
			conf |=
					(half_tick << 21) |
					(inact_tick << 16) | (act_tick << 11);
		}
#ifdef DEBUG
		if (conf) {
			printf("conf[%d] = 0x%x, cyc = %d (%d ns), act = %d (%d ns), inact = %d\n",
					drive, conf, cycle_tick, min_cycle, act_tick, min_active, inact_tick);
		}
#endif
		sc->sc_dmaconf[drive] = conf;
	}
	sc->sc_wdcdev.cap &= ~WDC_CAPABILITY_SELECT;
	sc->sc_wdcdev.select = 0;
	if (sc->sc_dmaconf[0]) {
		wdc_obio_select(chp,0);
		if (sc->sc_dmaconf[1] && (sc->sc_dmaconf[0] != sc->sc_dmaconf[1])) {
			sc->sc_wdcdev.select = wdc_obio_select;
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_SELECT;
		}
	} else if (sc->sc_dmaconf[1]) {
		wdc_obio_select(chp,1);
	}
	wdc_print_modes(chp);
}

void
ata4_adjust_timing(chp)
	struct channel_softc *chp;
{
	struct wdc_obio_softc *sc = (struct wdc_obio_softc *)chp->wdc;
	int drive;
	int min_cycle, min_active;
	int cycle_tick, act_tick, inact_tick;

	for (drive = 0; drive < 2; drive++) {
		u_int conf = 0;
		struct ata_drive_datas *drvp;

		drvp = &chp->ch_drive[drive];
		/* set up pio mode timings */

		if (drvp->drive_flags & DRIVE) {
			int piomode = drvp->PIO_mode;
			min_cycle = pio_timing[piomode].cycle;
			min_active = pio_timing[piomode].active;

			cycle_tick = ATA4_TIME_TO_TICK(min_cycle);
			act_tick = ATA4_TIME_TO_TICK(min_active);
			inact_tick = cycle_tick - act_tick;
			/* mask: 0x000003ff */
			conf |= (inact_tick << 5) | act_tick;
		}
		/* set up dma mode timings */
		if (drvp->drive_flags & DRIVE_DMA) {
			int dmamode = drvp->DMA_mode;
			min_cycle = dma_timing[dmamode].cycle;
			min_active = dma_timing[dmamode].active;
			cycle_tick = ATA4_TIME_TO_TICK(min_cycle);
			act_tick = ATA4_TIME_TO_TICK(min_active);
			inact_tick = cycle_tick - act_tick;
			/* mask: 0x001ffc00 */
			conf |= (act_tick << 10) | (inact_tick << 15);
		}
		/* set up udma mode timings */
		if (drvp->drive_flags & DRIVE_UDMA) {
			int udmamode = drvp->UDMA_mode;
			min_cycle = udma_timing[udmamode].cycle;
			min_active = udma_timing[udmamode].active;
			act_tick = ATA4_TIME_TO_TICK(min_active);
			cycle_tick = ATA4_TIME_TO_TICK(min_cycle);
			/* mask: 0x1ff00000 */
			conf |= (cycle_tick << 21) | (act_tick << 25) | 0x100000;
		}
#ifdef DEBUG
		if (conf) {
			printf("ata4 conf[%d] = 0x%x, cyc = %d (%d ns), act = %d (%d ns), inact = %d\n",
					drive, conf, cycle_tick, min_cycle, act_tick, min_active, inact_tick);
		}
#endif
		sc->sc_dmaconf[drive] = conf;
	}
	sc->sc_wdcdev.cap &= ~WDC_CAPABILITY_SELECT;
	sc->sc_wdcdev.select = 0;
	if (sc->sc_dmaconf[0]) {
		wdc_obio_select(chp,0);
		if (sc->sc_dmaconf[1] && (sc->sc_dmaconf[0] != sc->sc_dmaconf[1])) {
			sc->sc_wdcdev.select = wdc_obio_select;
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_SELECT;
		}
	} else if (sc->sc_dmaconf[1]) {
		wdc_obio_select(chp,1);
	}
	wdc_print_modes(chp);
}

int
wdc_obio_detach(self, flags)
	struct device *self;
	int flags;
{
	struct wdc_obio_softc *sc = (void *)self;
	int error;

	if ((error = wdcdetach(self, flags)) != 0)
		return error;

	intr_disestablish(sc->sc_ih);

	free(sc->wdc_channel.ch_queue, M_DEVBUF);

	/* Unmap our i/o space. */
	bus_space_unmap(chp->cmd_iot, chp->cmd_ioh, WDC_REG_NPORTS);

	/* Unmap DMA registers. */
	/* XXX unmapiodev(sc->sc_dmareg); */
	/* XXX free(sc->sc_dmacmd); */

	return 0;
}

int
wdc_obio_dma_init(v, channel, drive, databuf, datalen, flags)
	void *v;
	void *databuf;
	size_t datalen;
	int flags;
{
	struct wdc_obio_softc *sc = v;
	vaddr_t va = (vaddr_t)databuf;
	dbdma_command_t *cmdp;
	u_int cmd, offset;
	int read = flags & WDC_DMA_READ;

	cmdp = sc->sc_dmacmd;
	cmd = read ? DBDMA_CMD_IN_MORE : DBDMA_CMD_OUT_MORE;

	offset = va & PGOFSET;

	/* if va is not page-aligned, setup the first page */
	if (offset != 0) {
		int rest = NBPG - offset;	/* the rest of the page */

		if (datalen > rest) {		/* if continues to next page */
			DBDMA_BUILD(cmdp, cmd, 0, rest, vtophys(va),
				DBDMA_INT_NEVER, DBDMA_WAIT_NEVER,
				DBDMA_BRANCH_NEVER);
			datalen -= rest;
			va += rest;
			cmdp++;
		}
	}

	/* now va is page-aligned */
	while (datalen > NBPG) {
		DBDMA_BUILD(cmdp, cmd, 0, NBPG, vtophys(va),
			DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
		datalen -= NBPG;
		va += NBPG;
		cmdp++;
	}

	/* the last page (datalen <= NBPG here) */
	cmd = read ? DBDMA_CMD_IN_LAST : DBDMA_CMD_OUT_LAST;
	DBDMA_BUILD(cmdp, cmd, 0, datalen, vtophys(va),
		DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
	cmdp++;

	DBDMA_BUILD(cmdp, DBDMA_CMD_STOP, 0, 0, 0,
		DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);

	return 0;
}

void
wdc_obio_dma_start(v, channel, drive)
	void *v;
	int channel, drive;
{
	struct wdc_obio_softc *sc = v;

	dbdma_start(sc->sc_dmareg, sc->sc_dmacmd);
}

int
wdc_obio_dma_finish(v, channel, drive, read)
	void *v;
	int channel, drive;
	int read;
{
	struct wdc_obio_softc *sc = v;

	dbdma_stop(sc->sc_dmareg);
	return 0;
}
