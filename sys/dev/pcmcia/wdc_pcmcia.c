/*	$NetBSD: wdc_pcmcia.c,v 1.55.2.1 2004/08/03 10:50:18 skrll Exp $ */

/*-
 * Copyright (c) 1998, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, by Onno van der Linden and by Manuel Bouyer.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wdc_pcmcia.c,v 1.55.2.1 2004/08/03 10:50:18 skrll Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/ic/wdcreg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

#define WDC_PCMCIA_REG_NPORTS      8
#define WDC_PCMCIA_AUXREG_OFFSET   (WDC_PCMCIA_REG_NPORTS + 6)
#define WDC_PCMCIA_AUXREG_NPORTS   2

struct wdc_pcmcia_softc {
	struct wdc_softc sc_wdcdev;
	struct wdc_channel *wdc_chanlist[1];
	struct wdc_channel wdc_channel;
	struct ata_queue wdc_chqueue;
	struct pcmcia_io_handle sc_pioh;
	struct pcmcia_io_handle sc_auxpioh;
	struct pcmcia_mem_handle sc_pmembaseh;
	struct pcmcia_mem_handle sc_pmemh;
	struct pcmcia_mem_handle sc_auxpmemh;
	int sc_memwindow;
	int sc_iowindow;
	int sc_auxiowindow;
	void *sc_ih;
	struct pcmcia_function *sc_pf;
	int sc_flags;
#define WDC_PCMCIA_ATTACH	0x0001
#define WDC_PCMCIA_MEMMODE	0x0002
};

static int wdc_pcmcia_match	__P((struct device *, struct cfdata *, void *));
static void wdc_pcmcia_attach	__P((struct device *, struct device *, void *));
static int wdc_pcmcia_detach	__P((struct device *, int));

CFATTACH_DECL(wdc_pcmcia, sizeof(struct wdc_pcmcia_softc),
    wdc_pcmcia_match, wdc_pcmcia_attach, wdc_pcmcia_detach, wdcactivate);

const struct wdc_pcmcia_product {
	u_int32_t	wpp_vendor;	/* vendor ID */
	u_int32_t	wpp_product;	/* product ID */
	int		wpp_quirk_flag;	/* Quirk flags */
#define WDC_PCMCIA_NO_EXTRA_RESETS	0x02 /* Only reset ctrl once */
	const char	*wpp_cis_info[4];	/* XXX necessary? */
} wdc_pcmcia_products[] = {

	{ /* PCMCIA_VENDOR_DIGITAL XXX */ 0x0100,
	  PCMCIA_PRODUCT_DIGITAL_MOBILE_MEDIA_CDROM,
	  0, {NULL, "Digital Mobile Media CD-ROM", NULL, NULL} },

	{ PCMCIA_VENDOR_IBM,
	  PCMCIA_PRODUCT_IBM_PORTABLE_CDROM,
	  0, {NULL, "PCMCIA Portable CD-ROM Drive", NULL, NULL} },

	/* The TEAC IDE/Card II is used on the Sony Vaio */
	{ PCMCIA_VENDOR_TEAC,
	  PCMCIA_PRODUCT_TEAC_IDECARDII,
	  WDC_PCMCIA_NO_EXTRA_RESETS,
	  PCMCIA_CIS_TEAC_IDECARDII },

	/*
	 * A fujitsu rebranded panasonic drive that reports 
	 * itself as function "scsi", disk interface 0
	 */
	{ PCMCIA_VENDOR_PANASONIC,
	  PCMCIA_PRODUCT_PANASONIC_KXLC005,
	  0,
	  PCMCIA_CIS_PANASONIC_KXLC005 },

	/*
	 * EXP IDE/ATAPI DVD Card use with some DVD players.
	 * Does not have a vendor ID or product ID.
	 */
	{ -1, -1, 0,
	  PCMCIA_CIS_EXP_EXPMULTIMEDIA },

	/* Mobile Dock 2, neither vendor ID nor product ID */
	{ -1, -1, 0,
	  {"SHUTTLE TECHNOLOGY LTD.", "PCCARD-IDE/ATAPI Adapter", NULL, NULL} },

	/* Toshiba Portege 3110 CD, neither vendor ID nor product ID */
	{ -1, -1, 0,
	  {"FREECOM", "PCCARD-IDE", NULL, NULL} },

	/* Random CD-ROM, (badged AMACOM), neither vendor ID nor product ID */ 
	{ -1, -1, 0,
	  {"PCMCIA", "CD-ROM", NULL, NULL} },

	/* IO DATA CBIDE2, with neither vendor ID nor product ID */
	{ -1, -1, 0,
	  PCMCIA_CIS_IODATA_CBIDE2 },

	/* TOSHIBA PA2673U(IODATA_CBIDE2 OEM), */
	/*  with neither vendor ID nor product ID */
	{ -1, -1, 0,
	  PCMCIA_CIS_TOSHIBA_CBIDE2 },

	/* 
	 * Novac PCMCIA-IDE Card for HD530P IDE Box, 
	 * with neither vendor ID nor product ID
	 */
	{ -1, -1, 0,
	  {"PCMCIA", "PnPIDE", NULL, NULL} },
};

const struct wdc_pcmcia_product *
	wdc_pcmcia_lookup __P((struct pcmcia_attach_args *));

int	wdc_pcmcia_enable __P((struct device *, int));

const struct wdc_pcmcia_product *
wdc_pcmcia_lookup(pa)
	struct pcmcia_attach_args *pa;
{
	const struct wdc_pcmcia_product *wpp;
	int i, cis_match;
	int n;

	for (wpp = wdc_pcmcia_products,
	    n = sizeof(wdc_pcmcia_products) / sizeof(wdc_pcmcia_products[0]);
	    n; wpp++, n--) {
		if ((wpp->wpp_vendor == -1 ||
		     pa->manufacturer == wpp->wpp_vendor) &&
		    (wpp->wpp_product == -1 ||
		     pa->product == wpp->wpp_product)) {
			cis_match = 1;
			for (i = 0; i < 4; i++) {
				if (!(wpp->wpp_cis_info[i] == NULL ||
				      (pa->card->cis1_info[i] != NULL &&
				       strcmp(pa->card->cis1_info[i],
					      wpp->wpp_cis_info[i]) == 0)))
					cis_match = 0;
			}
			if (cis_match)
				return (wpp);
		}
	}

	return (NULL);
}

static int
wdc_pcmcia_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->pf->function == PCMCIA_FUNCTION_DISK && 
	    pa->pf->pf_funce_disk_interface == PCMCIA_TPLFE_DDI_PCCARD_ATA) {
		return 10;
	}

	if (wdc_pcmcia_lookup(pa) != NULL)
		return (1);

	return (0);
}

static void
wdc_pcmcia_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct wdc_pcmcia_softc *sc = (void *)self;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	const struct wdc_pcmcia_product *wpp;
	bus_size_t offset = 0;
	int quirks, i;

	aprint_normal("\n");

	sc->sc_pf = pa->pf;

	SIMPLEQ_FOREACH(cfe, &pa->pf->cfe_head, cfe_list) {
		if (cfe->num_iospace != 1 && cfe->num_iospace != 2)
			continue;

		if (pcmcia_io_alloc(pa->pf, cfe->iospace[0].start,
		    cfe->iospace[0].length,
		    cfe->iospace[0].start == 0 ? cfe->iospace[0].length : 0,
		    &sc->sc_pioh))
			continue;

		if (cfe->num_iospace == 2) {
			if (!pcmcia_io_alloc(pa->pf, cfe->iospace[1].start,
			    cfe->iospace[1].length, 0, &sc->sc_auxpioh))
				break;
		} else /* num_iospace == 1 */ {
			sc->sc_auxpioh.iot = sc->sc_pioh.iot;
			if (!bus_space_subregion(sc->sc_pioh.iot,
			    sc->sc_pioh.ioh, WDC_PCMCIA_AUXREG_OFFSET,
			    WDC_PCMCIA_AUXREG_NPORTS, &sc->sc_auxpioh.ioh))
				break;
		}
		pcmcia_io_free(pa->pf, &sc->sc_pioh);
	}

	/* 
	 * Compact Flash memory mapped mode
	 * CF+ and CompactFlash Spec. Rev 1.4, 6.1.3 Memory Mapped Addressing.
	 * http://www.compactflash.org/cfspc1_4.pdf
	 */
	if (cfe == NULL) {
		SIMPLEQ_FOREACH(cfe, &pa->pf->cfe_head, cfe_list) {
			if (cfe->iftype != PCMCIA_IFTYPE_MEMORY)
				continue;
			if (pcmcia_mem_alloc(pa->pf, cfe->memspace[0].length,
			    &sc->sc_pmembaseh) == 0) {
				sc->sc_flags |= WDC_PCMCIA_MEMMODE;
				break;
			}
		}
	}

	if (cfe == NULL) {
		aprint_error("%s: can't handle card info\n", self->dv_xname);
		goto no_config_entry;
	}

	/* Enable the card. */
	pcmcia_function_init(pa->pf, cfe);
	if (pcmcia_function_enable(pa->pf)) {
		aprint_error("%s: function enable failed\n", self->dv_xname);
		goto enable_failed;
	}

	wpp = wdc_pcmcia_lookup(pa);
	if (wpp != NULL)
		quirks = wpp->wpp_quirk_flag;
	else
		quirks = 0;

	if (sc->sc_flags & WDC_PCMCIA_MEMMODE) {
		if (pcmcia_mem_map(pa->pf, PCMCIA_MEM_COMMON, 0,
		    sc->sc_pmembaseh.size, &sc->sc_pmembaseh, &offset,
		    &sc->sc_memwindow)) {
			aprint_error("%s: can't map memory space\n",
			    self->dv_xname);
			goto map_failed;
		}

		sc->sc_pmemh.memt = sc->sc_pmembaseh.memt;
		sc->sc_pmemh.memh = sc->sc_pmembaseh.memh;

		sc->sc_auxpmemh.memt = sc->sc_pmemh.memt;
		if (bus_space_subregion(sc->sc_pmemh.memt,
		    sc->sc_pmembaseh.memh, WDC_PCMCIA_AUXREG_OFFSET + offset,
		    WDC_PCMCIA_AUXREG_NPORTS, &sc->sc_auxpmemh.memh))
			goto mapaux_failed;
		
		aprint_normal("%s: memory mapped mode\n", self->dv_xname);
	} else {
		if (pcmcia_io_map(pa->pf, PCMCIA_WIDTH_AUTO, 0,
		    sc->sc_pioh.size, &sc->sc_pioh, &sc->sc_iowindow)) {
			aprint_error("%s: can't map first I/O space\n",
			     self->dv_xname);
			goto map_failed;
		} 
	}

	if (cfe->num_iospace <= 1 || sc->sc_flags & WDC_PCMCIA_MEMMODE)
		sc->sc_auxiowindow = -1;
	else if (pcmcia_io_map(pa->pf, PCMCIA_WIDTH_AUTO, 0,
	    sc->sc_auxpioh.size, &sc->sc_auxpioh, &sc->sc_auxiowindow)) {
		aprint_error("%s: can't map second I/O space\n",
		    self->dv_xname);
		goto mapaux_failed;
	}

	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16;
	if (sc->sc_flags & WDC_PCMCIA_MEMMODE) {
		sc->wdc_channel.cmd_iot = sc->sc_pmemh.memt;
		sc->wdc_channel.cmd_baseioh = sc->sc_pmemh.memh;
		sc->wdc_channel.ctl_iot = sc->sc_auxpmemh.memt;
		sc->wdc_channel.ctl_ioh = sc->sc_auxpmemh.memh;
	} else {
		sc->wdc_channel.cmd_iot = sc->sc_pioh.iot;
		sc->wdc_channel.cmd_baseioh = sc->sc_pioh.ioh;
		sc->wdc_channel.ctl_iot = sc->sc_auxpioh.iot;
		sc->wdc_channel.ctl_ioh = sc->sc_auxpioh.ioh;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA32;
	}
	for (i = 0; i < WDC_PCMCIA_REG_NPORTS; i++) {
		if (bus_space_subregion(sc->wdc_channel.cmd_iot,
		    sc->wdc_channel.cmd_baseioh,
		    offset + i, i == 0 ? 4 : 1,
		    &sc->wdc_channel.cmd_iohs[i]) != 0) {
			aprint_error("%s: can't subregion I/O space\n",
			    self->dv_xname);
			goto mapaux_failed;
		}
	}
	wdc_init_shadow_regs(&sc->wdc_channel);
	sc->wdc_channel.data32iot = sc->wdc_channel.cmd_iot;
	sc->wdc_channel.data32ioh = sc->wdc_channel.cmd_iohs[0];
	sc->sc_wdcdev.PIO_cap = 0;
	sc->wdc_chanlist[0] = &sc->wdc_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanlist;
	sc->sc_wdcdev.nchannels = 1;
	sc->wdc_channel.ch_channel = 0;
	sc->wdc_channel.ch_wdc = &sc->sc_wdcdev;
	sc->wdc_channel.ch_queue = &sc->wdc_chqueue;
#if 0
	if (quirks & WDC_PCMCIA_NO_EXTRA_RESETS)
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_NO_EXTRA_RESETS;
#endif

	/* We can enable and disable the controller. */
	sc->sc_wdcdev.sc_atapi_adapter._generic.adapt_enable =
	    wdc_pcmcia_enable;

	sc->sc_flags |= WDC_PCMCIA_ATTACH;
	wdcattach(&sc->wdc_channel);

	return;

 mapaux_failed:
	/* Unmap our i/o window. */
	if (sc->sc_flags & WDC_PCMCIA_MEMMODE)
		pcmcia_mem_unmap(sc->sc_pf, sc->sc_memwindow);
	else
		pcmcia_io_unmap(sc->sc_pf, sc->sc_iowindow);

 map_failed:
	/* Disable the function */
	pcmcia_function_disable(sc->sc_pf);

 enable_failed:
	/* Unmap our i/o space. */
	if (sc->sc_flags & WDC_PCMCIA_MEMMODE) {
		pcmcia_mem_free(sc->sc_pf, &sc->sc_pmembaseh);
	} else  {
		pcmcia_io_free(sc->sc_pf, &sc->sc_pioh);
		if (cfe->num_iospace == 2)
		    pcmcia_io_free(sc->sc_pf, &sc->sc_auxpioh);
	}
 no_config_entry:
	sc->sc_iowindow = -1;
}

int
wdc_pcmcia_detach(self, flags)
	struct device *self;
	int flags;
{
	struct wdc_pcmcia_softc *sc = (struct wdc_pcmcia_softc *)self;
	int error;

	if (sc->sc_iowindow == -1)
		/* Nothing to detach */
		return (0);

	/*
	 * If the WDC_PCMCIA_ATTACH flag is still set, then we didn't get
	 * a chance * enable/disable the card in the wdc/atabus layer, so
	 * we still need to disable the function here.
	 */
	if (sc->sc_flags & WDC_PCMCIA_ATTACH) {
		sc->sc_flags &= ~WDC_PCMCIA_ATTACH;
		pcmcia_function_disable(sc->sc_pf);
	}

	if ((error = wdcdetach(self, flags)) != 0)
		return (error);

	/* Unmap our i/o window and i/o space. */
	if (sc->sc_flags & WDC_PCMCIA_MEMMODE) {
		pcmcia_mem_unmap(sc->sc_pf, sc->sc_memwindow);
		pcmcia_mem_free(sc->sc_pf, &sc->sc_pmembaseh);
	} else {
		pcmcia_io_unmap(sc->sc_pf, sc->sc_iowindow);
		pcmcia_io_free(sc->sc_pf, &sc->sc_pioh);
		if (sc->sc_auxiowindow != -1) {
			pcmcia_io_unmap(sc->sc_pf, sc->sc_auxiowindow);
			pcmcia_io_free(sc->sc_pf, &sc->sc_auxpioh);
		}
	}

	return (0);
}

int
wdc_pcmcia_enable(self, onoff)
	struct device *self;
	int onoff;
{
	struct wdc_pcmcia_softc *sc = (void *)self;

	if (onoff) {
		/* Establish the interrupt handler. */
		sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_BIO,
		    wdcintr, &sc->wdc_channel);
		if (sc->sc_ih == NULL) {
			printf("%s: couldn't establish interrupt handler\n",
			    sc->sc_wdcdev.sc_dev.dv_xname);
			return (EIO);
		}

		/*
		 * If the WDC_PCMCIA_ATTACH flag is set, we've already
		 * enabled the card in the attach routine, so don't
		 * re-enable it here (to save power cycle time).  Clear
		 * the flag, though, so that the next disable/enable
		 * will do the right thing.
		 */
		if (sc->sc_flags & WDC_PCMCIA_ATTACH) {
			sc->sc_flags &= ~WDC_PCMCIA_ATTACH;
		} else {
			if (pcmcia_function_enable(sc->sc_pf)) {
				printf("%s: couldn't enable PCMCIA function\n",
				    sc->sc_wdcdev.sc_dev.dv_xname);
				pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
				return (EIO);
			}
		}
	} else {
		pcmcia_function_disable(sc->sc_pf);
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
	}

	return (0);
}
