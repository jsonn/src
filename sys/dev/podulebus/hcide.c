/*	$NetBSD: hcide.c,v 1.5.6.1 2004/08/03 10:50:27 skrll Exp $	*/

/*-
 * Copyright (c) 2000, 2001 Ben Harris
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
/*
 * hcide.c - Driver for the HCCS 16-bit IDE interface.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hcide.c,v 1.5.6.1 2004/08/03 10:50:27 skrll Exp $");

#include <sys/param.h>

#include <sys/device.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/podulebus/podulebus.h>
#include <dev/podulebus/podules.h>
#include <dev/podulebus/hcidereg.h>

#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

struct hcide_softc {
	struct wdc_softc sc_wdc;
	struct wdc_channel *sc_chp[HCIDE_NCHANNELS];/* pointers to sc_chan */
	struct wdc_channel sc_chan[HCIDE_NCHANNELS];
	struct ata_queue sc_chq[HCIDE_NCHANNELS];
};

static int  hcide_match  (struct device *, struct cfdata *, void *);
static void hcide_attach (struct device *, struct device *, void *);

CFATTACH_DECL(hcide, sizeof(struct hcide_softc),
    hcide_match, hcide_attach, NULL, NULL);

static const int hcide_cmdoffsets[] = { HCIDE_CMD0, HCIDE_CMD1, HCIDE_CMD2 };
static const int hcide_ctloffsets[] = { HCIDE_CTL, HCIDE_CTL, HCIDE_CTL };

static int
hcide_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct podulebus_attach_args *pa = aux;

	if (pa->pa_product == PODULE_HCCS_IDESCSI &&
	    strncmp(pa->pa_descr, "IDE", 3) == 0)
		return 1;
	return 0;
}

static void
hcide_attach(struct device *parent, struct device *self, void *aux)
{
	struct hcide_softc *sc = (void *)self;
	struct podulebus_attach_args *pa = aux;
	struct wdc_channel *ch;
	int i, j;

	sc->sc_wdc.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_NOIRQ;
	sc->sc_wdc.PIO_cap = 0; /* XXX correct? */
	sc->sc_wdc.DMA_cap = 0; /* XXX correct? */
	sc->sc_wdc.UDMA_cap = 0;
	sc->sc_wdc.nchannels = HCIDE_NCHANNELS;
	sc->sc_wdc.channels = sc->sc_chp;
	printf("\n");
	for (i = 0; i < HCIDE_NCHANNELS; i++) {
		ch = sc->sc_chp[i] = &sc->sc_chan[i];
		ch->ch_channel = i;
		ch->ch_wdc = &sc->sc_wdc;
		ch->cmd_iot = pa->pa_mod_t;
		ch->ctl_iot = pa->pa_mod_t;
		ch->ch_queue = &sc->sc_chq[i];
		bus_space_map(pa->pa_fast_t,
		    pa->pa_fast_base + hcide_cmdoffsets[i], 0, 8,
		    &ch->cmd_baseioh);
		for (j = 0; j < WDC_NREG; j++)
			bus_space_subregion(ch->cmd_iot, ch->cmd_baseioh,
			    j, j == 0 ? 4 : 1, &ch->cmd_iohs[j]);
		wdc_init_shadow_regs(ch);
		bus_space_map(pa->pa_fast_t,
		    pa->pa_fast_base + hcide_ctloffsets[i], 0, 8,
		    &ch->ctl_ioh);
		wdcattach(ch);
	}

}
