/*	$NetBSD: wdc_obio.c,v 1.2.2.2 2000/07/07 17:33:46 he Exp $	*/

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

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/autoconf.h>

#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

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
};

int	wdc_obio_probe	__P((struct device *, struct cfdata *, void *));
void	wdc_obio_attach	__P((struct device *, struct device *, void *));

struct cfattach wdc_obio_ca = {
	sizeof(struct wdc_obio_softc), wdc_obio_probe, wdc_obio_attach
};

static int	wdc_obio_dma_init __P((void *, int, int, void *, size_t, int));
static void 	wdc_obio_dma_start __P((void *, int, int));
static int	wdc_obio_dma_finish __P((void *, int, int, int));

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

	bzero(compat, sizeof(compat));
	OF_getprop(ca->ca_node, "compatible", compat, sizeof(compat));
	if (strcmp(compat, "heathrow-ata") == 0)
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
	int piointr, dmaintr;
	int use_dma = 0;

	piointr = dmaintr = -1;

	if (sc->sc_wdcdev.sc_dev.dv_cfdata->cf_flags & WDC_OPTIONS_DMA) {
		if (ca->ca_nreg >= 16 || ca->ca_nintr == -1)
			use_dma = 1;	/* XXX Don't work yet. */
	}

	if (ca->ca_nintr == -1) {
		piointr = WDC_DEFAULT_PIO_IRQ;
		dmaintr = WDC_DEFAULT_DMA_IRQ;
		printf(" irq property not found; using %d,%d",
			piointr, dmaintr);
	}

	if (ca->ca_nintr >= 4 && ca->ca_nreg >= 8) {
		piointr = ca->ca_intr[0];
		printf(" irq %d", piointr);
	}
	if (ca->ca_nintr >= 8 && use_dma) {
		dmaintr = ca->ca_intr[1];
		printf(",%d", dmaintr);
	}

	if (piointr == -1) {
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

	intr_establish(piointr, IST_LEVEL, IPL_BIO, wdcintr, chp);

	if (use_dma) {
		if (dmaintr != -1)
			intr_establish(dmaintr, IST_LEVEL, IPL_BIO,
				       wdcintr, chp);

		sc->sc_dmacmd = dbdma_alloc(sizeof(dbdma_command_t) * 20);
		sc->sc_dmareg = mapiodev(ca->ca_baseaddr + ca->ca_reg[2],
					 ca->ca_reg[3]);
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA;
	}
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16;
	sc->sc_wdcdev.PIO_cap = 0;
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

	wdcattach(chp);
}

static int
wdc_obio_dma_init(v, channel, drive, databuf, datalen, read)
	void *v;
	void *databuf;
	size_t datalen;
	int read;
{
	struct wdc_obio_softc *sc = v;
	vaddr_t va = (vaddr_t)databuf;
	dbdma_command_t *cmdp;
	u_int cmd;

#ifdef DIAGNOSTIC
	if (va & PGOFSET)
		panic("wdc_obio: databuf not page aligned");
	if (datalen > 65536)
		panic("wdc_obio: datalen too large");
#endif

	cmdp = sc->sc_dmacmd;
	cmd = read ? DBDMA_CMD_IN_MORE : DBDMA_CMD_OUT_MORE;
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
		DBDMA_INT_ALWAYS, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
	cmdp++;

	DBDMA_BUILD(cmdp, DBDMA_CMD_STOP, 0, 0, 0,
		DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);

	return 0;
}

static void
wdc_obio_dma_start(v, channel, drive)
	void *v;
	int channel, drive;
{
	struct wdc_obio_softc *sc = v;

	dbdma_start(sc->sc_dmareg, sc->sc_dmacmd);
}

static int
wdc_obio_dma_finish(v, channel, drive, read)
	void *v;
	int channel, drive;
	int read;
{
	/* nothing to do */
	return 0;
}
