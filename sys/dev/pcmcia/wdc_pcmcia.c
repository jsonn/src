/*	$NetBSD: wdc_pcmcia.c,v 1.55.2.2 2004/08/12 11:42:05 skrll Exp $ */

/*-
 * Copyright (c) 1998, 2003, 2004 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: wdc_pcmcia.c,v 1.55.2.2 2004/08/12 11:42:05 skrll Exp $");

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

	struct pcmcia_function *sc_pf;
	void *sc_ih;

	int sc_state;
#define WDC_PCMCIA_ATTACHED	3
};

static int wdc_pcmcia_match	__P((struct device *, struct cfdata *, void *));
static int wdc_pcmcia_validate_config_io __P((struct pcmcia_config_entry *));
static int wdc_pcmcia_validate_config_memory __P((struct pcmcia_config_entry *));
static void wdc_pcmcia_attach	__P((struct device *, struct device *, void *));
static int wdc_pcmcia_detach	__P((struct device *, int));

CFATTACH_DECL(wdc_pcmcia, sizeof(struct wdc_pcmcia_softc),
    wdc_pcmcia_match, wdc_pcmcia_attach, wdc_pcmcia_detach, wdcactivate);

const struct pcmcia_product wdc_pcmcia_products[] = {
	{ PCMCIA_VENDOR_DIGITAL,
	  PCMCIA_PRODUCT_DIGITAL_MOBILE_MEDIA_CDROM,
	  {NULL, "Digital Mobile Media CD-ROM", NULL, NULL} },

	{ PCMCIA_VENDOR_IBM,
	  PCMCIA_PRODUCT_IBM_PORTABLE_CDROM,
	  {NULL, "PCMCIA Portable CD-ROM Drive", NULL, NULL} },

	/* The TEAC IDE/Card II is used on the Sony Vaio */
	{ PCMCIA_VENDOR_TEAC,
	  PCMCIA_PRODUCT_TEAC_IDECARDII,
	  PCMCIA_CIS_TEAC_IDECARDII },

	/*
	 * A fujitsu rebranded panasonic drive that reports 
	 * itself as function "scsi", disk interface 0
	 */
	{ PCMCIA_VENDOR_PANASONIC,
	  PCMCIA_PRODUCT_PANASONIC_KXLC005,
	  PCMCIA_CIS_PANASONIC_KXLC005 },

	/*
	 * EXP IDE/ATAPI DVD Card use with some DVD players.
	 * Does not have a vendor ID or product ID.
	 */
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_EXP_EXPMULTIMEDIA },

	/* Mobile Dock 2, neither vendor ID nor product ID */
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  {"SHUTTLE TECHNOLOGY LTD.", "PCCARD-IDE/ATAPI Adapter", NULL, NULL} },

	/* Toshiba Portege 3110 CD, neither vendor ID nor product ID */
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  {"FREECOM", "PCCARD-IDE", NULL, NULL} },

	/* Random CD-ROM, (badged AMACOM), neither vendor ID nor product ID */ 
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  {"PCMCIA", "CD-ROM", NULL, NULL} },

	/* IO DATA CBIDE2, with neither vendor ID nor product ID */
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_IODATA_CBIDE2 },

	/* TOSHIBA PA2673U(IODATA_CBIDE2 OEM), */
	/*  with neither vendor ID nor product ID */
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_TOSHIBA_CBIDE2 },

	/* 
	 * Novac PCMCIA-IDE Card for HD530P IDE Box, 
	 * with neither vendor ID nor product ID
	 */
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  {"PCMCIA", "PnPIDE", NULL, NULL} },
};
const size_t wdc_pcmcia_nproducts =
    sizeof(wdc_pcmcia_products) / sizeof(wdc_pcmcia_products[0]);

int	wdc_pcmcia_enable __P((struct device *, int));
void	wdc_pcmcia_datain_memory __P((struct wdc_channel *, int, void *,
	    size_t));
void	wdc_pcmcia_dataout_memory __P((struct wdc_channel *, int, void *,
	    size_t));

static int
wdc_pcmcia_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->pf->function == PCMCIA_FUNCTION_DISK && 
	    pa->pf->pf_funce_disk_interface == PCMCIA_TPLFE_DDI_PCCARD_ATA)
		return (1);
	if (pcmcia_product_lookup(pa, wdc_pcmcia_products, wdc_pcmcia_nproducts,
	    sizeof(wdc_pcmcia_products[0]), NULL))
		return (2);
	return (0);
}

static int
wdc_pcmcia_validate_config_io(cfe)
	struct pcmcia_config_entry *cfe;
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_iospace < 1 || cfe->num_iospace > 2)
		return (EINVAL);
	cfe->num_memspace = 0;
	return (0);
}

static int
wdc_pcmcia_validate_config_memory(cfe)
	struct pcmcia_config_entry *cfe;
{
	if (cfe->iftype != PCMCIA_IFTYPE_MEMORY ||
	    cfe->num_memspace > 1 ||
	    cfe->memspace[0].length < 2048)
		return (EINVAL);
	cfe->num_iospace = 0;
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
	bus_size_t offset;
	int i;
	int error;

	sc->sc_pf = pa->pf;

	error = pcmcia_function_configure(pa->pf,
	    wdc_pcmcia_validate_config_io);
	if (error)
		/*XXXmem16|common*/
		error = pcmcia_function_configure(pa->pf,
		    wdc_pcmcia_validate_config_memory);
	if (error) {
		aprint_error("%s: configure failed, error=%d\n", self->dv_xname,
		    error);
		return;
	}

	cfe = pa->pf->cfe;
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16;

	if (cfe->iftype == PCMCIA_IFTYPE_MEMORY) {
		sc->wdc_channel.cmd_iot = cfe->memspace[0].handle.memt;
		sc->wdc_channel.cmd_baseioh = cfe->memspace[0].handle.memh;
		offset = cfe->memspace[0].offset;
		sc->wdc_channel.ctl_iot = cfe->memspace[0].handle.memt;
		if (bus_space_subregion(cfe->memspace[0].handle.memt,
		    cfe->memspace[0].handle.memh,
		    offset + WDC_PCMCIA_AUXREG_OFFSET, WDC_PCMCIA_AUXREG_NPORTS,
		    &sc->wdc_channel.ctl_ioh))
			goto fail;
	} else {
		sc->wdc_channel.cmd_iot = cfe->iospace[0].handle.iot;
		sc->wdc_channel.cmd_baseioh = cfe->iospace[0].handle.ioh;
		offset = 0;
		if (cfe->num_iospace == 1) {
			sc->wdc_channel.ctl_iot = cfe->iospace[0].handle.iot;
			if (bus_space_subregion(cfe->iospace[0].handle.iot,
			    cfe->iospace[0].handle.ioh,
			    WDC_PCMCIA_AUXREG_OFFSET, WDC_PCMCIA_AUXREG_NPORTS,
			    &sc->wdc_channel.ctl_ioh))
				goto fail;
		} else {
			sc->wdc_channel.ctl_iot = cfe->iospace[1].handle.iot;
			sc->wdc_channel.ctl_ioh = cfe->iospace[1].handle.ioh;
		}
	}

	for (i = 0; i < WDC_PCMCIA_REG_NPORTS; i++) {
		if (bus_space_subregion(sc->wdc_channel.cmd_iot,
		    sc->wdc_channel.cmd_baseioh,
		    offset + i, i == 0 ? 4 : 1,
		    &sc->wdc_channel.cmd_iohs[i]) != 0) {
			aprint_error("%s: can't subregion I/O space\n",
			    self->dv_xname);
			goto fail2;
		}
	}

	if (cfe->iftype == PCMCIA_IFTYPE_MEMORY) {
		aprint_normal("%s: memory mapped mode\n", self->dv_xname);
		sc->wdc_channel.data32iot = cfe->memspace[0].handle.memt;
		if (bus_space_subregion(cfe->memspace[0].handle.memt,
		    cfe->memspace[0].handle.memh, offset + 1024, 1024,
		    &sc->wdc_channel.data32ioh))
			goto fail;
		sc->sc_wdcdev.datain_pio = wdc_pcmcia_datain_memory;
		sc->sc_wdcdev.dataout_pio = wdc_pcmcia_dataout_memory;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_NOIRQ;
	} else {
		aprint_normal("%s: i/o mapped mode\n", self->dv_xname);
		sc->wdc_channel.data32iot = sc->wdc_channel.cmd_iot;
		sc->wdc_channel.data32ioh = sc->wdc_channel.cmd_iohs[wd_data];
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA32;
	}

	error = wdc_pcmcia_enable(self, 1);
	if (error)
		goto fail;

	wdc_init_shadow_regs(&sc->wdc_channel);
	sc->sc_wdcdev.PIO_cap = 0;
	sc->wdc_chanlist[0] = &sc->wdc_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanlist;
	sc->sc_wdcdev.nchannels = 1;
	sc->wdc_channel.ch_channel = 0;
	sc->wdc_channel.ch_wdc = &sc->sc_wdcdev;
	sc->wdc_channel.ch_queue = &sc->wdc_chqueue;

	/* We can enable and disable the controller. */
	sc->sc_wdcdev.sc_atapi_adapter._generic.adapt_enable =
	    wdc_pcmcia_enable;
	sc->sc_wdcdev.sc_atapi_adapter._generic.adapt_refcnt = 1;

	wdcattach(&sc->wdc_channel);
	wdc_delref(&sc->wdc_channel);
	sc->sc_state = WDC_PCMCIA_ATTACHED;
	return;

fail2:
	wdc_pcmcia_enable(self, 0);
fail:
	pcmcia_function_unconfigure(pa->pf);
}

int
wdc_pcmcia_detach(self, flags)
	struct device *self;
	int flags;
{
	struct wdc_pcmcia_softc *sc = (struct wdc_pcmcia_softc *)self;
	int error;

	if (sc->sc_state != WDC_PCMCIA_ATTACHED)
		return (0);

	if ((error = wdcdetach(self, flags)) != 0)
		return (error);

	pcmcia_function_unconfigure(sc->sc_pf);

	return (0);
}

int
wdc_pcmcia_enable(self, onoff)
	struct device *self;
	int onoff;
{
	struct wdc_pcmcia_softc *sc = (void *)self;
	int error;

	if (onoff) {
		/* Establish the interrupt handler. */
		sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_BIO,
		    wdcintr, &sc->wdc_channel);
		if (!sc->sc_ih)
			return (EIO);

		error = pcmcia_function_enable(sc->sc_pf);
		if (error) {
			pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
			sc->sc_ih = 0;
			return (error);
		}
	} else {
		pcmcia_function_disable(sc->sc_pf);
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		sc->sc_ih = 0;
	}

	return (0);
}

void
wdc_pcmcia_datain_memory(chp, flags, buf, len)
	struct wdc_channel *chp;
	int flags;
	void *buf;
	size_t len;
{

	while (len > 0) {
		size_t n;

		n = min(len, 1024);
		if ((flags & DRIVE_CAP32) && (n & 3) == 0)
			bus_space_read_region_stream_4(chp->data32iot,
			    chp->data32ioh, 0, buf, n >> 2);
		else
			bus_space_read_region_stream_2(chp->data32iot,
			    chp->data32ioh, 0, buf, n >> 1);
		buf = (char *)buf + n;
		len -= n;
	}
}

void
wdc_pcmcia_dataout_memory(chp, flags, buf, len)
	struct wdc_channel *chp;
	int flags;
	void *buf;
	size_t len;
{

	while (len > 0) {
		size_t n;

		n = min(len, 1024);
		if ((flags & DRIVE_CAP32) && (n & 3) == 0)
			bus_space_write_region_stream_4(chp->data32iot,
			    chp->data32ioh, 0, buf, n >> 2);
		else
			bus_space_write_region_stream_2(chp->data32iot,
			    chp->data32ioh, 0, buf, n >> 1);
		buf = (char *)buf + n;
		len -= n;
	}
}
