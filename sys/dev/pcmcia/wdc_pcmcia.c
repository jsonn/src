/*	$NetBSD: wdc_pcmcia.c,v 1.21.2.2 1999/10/20 22:38:06 thorpej Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

#define WDC_PCMCIA_REG_NPORTS      8
#define WDC_PCMCIA_AUXREG_OFFSET   (WDC_PCMCIA_REG_NPORTS + 6)
#define WDC_PCMCIA_AUXREG_NPORTS   2

struct wdc_pcmcia_softc {
	struct wdc_softc sc_wdcdev;
	struct channel_softc *wdc_chanptr;
	struct channel_softc wdc_channel;
	struct pcmcia_io_handle sc_pioh;
	struct pcmcia_io_handle sc_auxpioh;
	int sc_iowindow;
	int sc_auxiowindow;
	void *sc_ih;
	struct pcmcia_function *sc_pf;
	int sc_flags;
#define	WDC_PCMCIA_ATTACH	0x0001
};

static int wdc_pcmcia_match	__P((struct device *, struct cfdata *, void *));
static void wdc_pcmcia_attach	__P((struct device *, struct device *, void *));
static int wdc_pcmcia_detach	__P((struct device *, int));

struct cfattach wdc_pcmcia_ca = {
	sizeof(struct wdc_pcmcia_softc), wdc_pcmcia_match, wdc_pcmcia_attach,
	wdc_pcmcia_detach, wdcactivate
};

struct wdc_pcmcia_product {
	u_int32_t	wpp_vendor;	/* vendor ID */
	u_int32_t	wpp_product;	/* product ID */
	int		wpp_quirk_flag;	/* Quirk flags */
#define WDC_PCMCIA_FORCE_16BIT_IO	0x01 /* Don't use PCMCIA_WIDTH_AUTO */
#define WDC_PCMCIA_NO_EXTRA_RESETS	0x02 /* Only reset ctrl once */
	const char	*wpp_cis_info[4];	/* XXX necessary? */
	const char	*wpp_name;	/* product name */
} wdc_pcmcia_products[] = {

	{ /* PCMCIA_VENDOR_DIGITAL XXX */ 0x0100,
	  PCMCIA_PRODUCT_DIGITAL_MOBILE_MEDIA_CDROM,
	  0, { NULL, "Digital Mobile Media CD-ROM", NULL, NULL },
	  PCMCIA_STR_DIGITAL_MOBILE_MEDIA_CDROM },

	{ PCMCIA_VENDOR_IBM,
	  PCMCIA_PRODUCT_IBM_PORTABLE_CDROM_DRIVE,
	  0, { NULL, "PCMCIA Portable CD-ROM Drive", NULL, NULL },
	  PCMCIA_STR_IBM_PORTABLE_CDROM_DRIVE },

	{ PCMCIA_VENDOR_HAGIWARASYSCOM,
	  -1,			/* XXX */
	  WDC_PCMCIA_FORCE_16BIT_IO,
	  { NULL, NULL, NULL, NULL },
	  "Hagiwara SYS-COM CompactFlash Card" },

	/* The TEAC IDE/Card II is used on the Sony Vaio */
	{ PCMCIA_VENDOR_TEAC,
	  PCMCIA_PRODUCT_TEAC_IDECARDII,
	  WDC_PCMCIA_NO_EXTRA_RESETS,
	  PCMCIA_CIS_TEAC_IDECARDII,
	  PCMCIA_STR_TEAC_IDECARDII },

	{ 0, 0, 0, { NULL, NULL, NULL, NULL}, NULL }
};

struct wdc_pcmcia_disk_device_interface_args {
	int ddi_type;		/* interface type */
	int ddi_reqfn;		/* function we are requesting iftype */
	int ddi_curfn;		/* function we are currently parsing in CIS */
};

int	wdc_pcmcia_disk_device_interface_callback __P((struct pcmcia_tuple *,
	    void *));
int	wdc_pcmcia_disk_device_interface __P((struct pcmcia_function *));
struct wdc_pcmcia_product *
	wdc_pcmcia_lookup __P((struct pcmcia_attach_args *));

int	wdc_pcmcia_enable __P((struct device *, int));

int
wdc_pcmcia_disk_device_interface_callback(tuple, arg)
	struct pcmcia_tuple *tuple;
	void *arg;
{
	struct wdc_pcmcia_disk_device_interface_args *ddi = arg;

	switch (tuple->code) {
	case PCMCIA_CISTPL_FUNCID:
		ddi->ddi_curfn++;
		break;

	case PCMCIA_CISTPL_FUNCE:
		if (ddi->ddi_reqfn != ddi->ddi_curfn)
			break;

		/* subcode (disk device interface), data (interface type) */
		if (tuple->length < 2)
			break;

		/* check type */
		if (pcmcia_tuple_read_1(tuple, 0) !=
		    PCMCIA_TPLFE_TYPE_DISK_DEVICE_INTERFACE)
			break;

		ddi->ddi_type = pcmcia_tuple_read_1(tuple, 1);
		return (1);
	}
	return (0);
}

int
wdc_pcmcia_disk_device_interface(pf)
	struct pcmcia_function *pf;
{
	struct wdc_pcmcia_disk_device_interface_args ddi;

	ddi.ddi_reqfn = pf->number;
	ddi.ddi_curfn = -1;
	if (pcmcia_scan_cis((struct device *)pf->sc,
	    wdc_pcmcia_disk_device_interface_callback, &ddi) > 0)
		return (ddi.ddi_type);
	else
		return (-1);
}

struct wdc_pcmcia_product *
wdc_pcmcia_lookup(pa)
	struct pcmcia_attach_args *pa;
{
	struct wdc_pcmcia_product *wpp;
	int i, cis_match;

	for (wpp = wdc_pcmcia_products; wpp->wpp_name != NULL; wpp++)
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

	return (NULL);
}

static int
wdc_pcmcia_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_softc *sc;
	int iftype;

	if (wdc_pcmcia_lookup(pa) != NULL)
		return (1);

	if (pa->pf->function == PCMCIA_FUNCTION_DISK) {
		sc = pa->pf->sc;

		pcmcia_chip_socket_enable(sc->pct, sc->pch);
		iftype = wdc_pcmcia_disk_device_interface(pa->pf);
		pcmcia_chip_socket_disable(sc->pct, sc->pch);

		if (iftype == PCMCIA_TPLFE_DDI_PCCARD_ATA)
			return (1);
	}

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
	struct wdc_pcmcia_product *wpp;
	int quirks;

	sc->sc_pf = pa->pf;

	for (cfe = SIMPLEQ_FIRST(&pa->pf->cfe_head); cfe != NULL;
	    cfe = SIMPLEQ_NEXT(cfe, cfe_list)) {
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

	if (cfe == NULL) {
		printf(": can't handle card info\n");
		return;
	}

	/* Enable the card. */
	pcmcia_function_init(pa->pf, cfe);
	if (pcmcia_function_enable(pa->pf)) {
		printf(": function enable failed\n");
		return;
	}

	/*
	 * XXX  DEC Mobile Media CDROM is not yet tested whether it works
	 * XXX  with PCMCIA_WIDTH_IO16.  HAGIWARA SYS-COM HPC-CF32 doesn't
	 * XXX  work with PCMCIA_WIDTH_AUTO.
	 * XXX  CANON FC-8M (SANDISK SDCFB 8M) works for both _AUTO and IO16.
	 * XXX  So, here is temporary work around.
	 */
	wpp = wdc_pcmcia_lookup(pa);
	if (wpp != NULL)
		quirks = wpp->wpp_quirk_flag;
	else
		quirks = 0;

	if (pcmcia_io_map(pa->pf, quirks & WDC_PCMCIA_FORCE_16BIT_IO ?
	    PCMCIA_WIDTH_IO16 : PCMCIA_WIDTH_AUTO, 0,
	    sc->sc_pioh.size, &sc->sc_pioh, &sc->sc_iowindow)) {
		printf(": can't map first I/O space\n");
		return;
	} 

	/*
	 * Currently, # of iospace is 1 except DIGITAL Mobile Media CD-ROM.
	 * So whether the work around like above is necessary or not
	 * is unknown.  XXX.
	 */
	if (cfe->num_iospace > 1 &&
	    pcmcia_io_map(pa->pf, PCMCIA_WIDTH_AUTO, 0,
	    sc->sc_auxpioh.size, &sc->sc_auxpioh, &sc->sc_auxiowindow)) {
		printf(": can't map second I/O space\n");
		return;
	} else
		sc->sc_auxiowindow = -1;

	printf("\n");

	sc->wdc_channel.cmd_iot = sc->sc_pioh.iot;
	sc->wdc_channel.cmd_ioh = sc->sc_pioh.ioh;
	sc->wdc_channel.ctl_iot = sc->sc_auxpioh.iot;
	sc->wdc_channel.ctl_ioh = sc->sc_auxpioh.ioh;
	sc->wdc_channel.data32iot = sc->wdc_channel.cmd_iot;
	sc->wdc_channel.data32ioh = sc->wdc_channel.cmd_ioh;
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32;
	sc->sc_wdcdev.PIO_cap = 0;
	sc->wdc_chanptr = &sc->wdc_channel;
	sc->sc_wdcdev.channels = &sc->wdc_chanptr;
	sc->sc_wdcdev.nchannels = 1;
	sc->wdc_channel.channel = 0;
	sc->wdc_channel.wdc = &sc->sc_wdcdev;
	sc->wdc_channel.ch_queue = malloc(sizeof(struct channel_queue),
	    M_DEVBUF, M_NOWAIT);
	if (sc->wdc_channel.ch_queue == NULL) {
		printf("%s: can't allocate memory for command queue\n",
		    sc->sc_wdcdev.sc_dev.dv_xname);
		return;
	}
	if (quirks & WDC_PCMCIA_NO_EXTRA_RESETS)
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_NO_EXTRA_RESETS;

	/* We can enable and disable the controller. */
	sc->sc_wdcdev.sc_atapi_adapter.adapt_enable = wdc_pcmcia_enable;

	sc->sc_flags |= WDC_PCMCIA_ATTACH;
	wdcattach(&sc->wdc_channel);
	sc->sc_flags &= ~WDC_PCMCIA_ATTACH;
}

int
wdc_pcmcia_detach(self, flags)
	struct device *self;
	int flags;
{
	struct wdc_pcmcia_softc *sc = (struct wdc_pcmcia_softc *)self;
	int error;

	if ((error = wdcdetach(self, flags)) != 0)
		return (error);

	free(sc->wdc_channel.ch_queue, M_DEVBUF);

	/* Unmap our i/o window and i/o space. */
	pcmcia_io_unmap(sc->sc_pf, sc->sc_iowindow);
	pcmcia_io_free(sc->sc_pf, &sc->sc_pioh);
	if (sc->sc_auxiowindow != -1) {
		pcmcia_io_unmap(sc->sc_pf, sc->sc_auxiowindow);
		pcmcia_io_free(sc->sc_pf, &sc->sc_auxpioh);
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
		sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_BIO, wdcintr,
		    &sc->wdc_channel);
		if (sc->sc_ih == NULL) {
			printf("%s: couldn't establish interrupt handler\n",
			    sc->sc_wdcdev.sc_dev.dv_xname);
			return (EIO);
		}

		if ((sc->sc_flags & WDC_PCMCIA_ATTACH) == 0) {
			if (pcmcia_function_enable(sc->sc_pf)) {
				printf("%s: couldn't enable PCMCIA function\n",
				    sc->sc_wdcdev.sc_dev.dv_xname);
				pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
				return (EIO);
			}
			wdcreset(&sc->wdc_channel, VERBOSE);
		}
	} else {
		pcmcia_function_disable(sc->sc_pf);
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
	}

	return (0);
}
