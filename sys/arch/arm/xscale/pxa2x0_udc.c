/*	$NetBSD: pxa2x0_udc.c,v 1.1.4.2 2006/12/30 20:45:38 yamt Exp $	*/
/*	$OpenBSD: pxa27x_udc.c,v 1.5 2005/03/30 14:24:39 dlg Exp $ */

/*
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <arm/xscale/pxa2x0cpu.h>
#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>

struct pxaudc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_size;

	void 			*sc_powerhook;
};

static int	pxaudc_match(struct device *, struct cfdata *, void *);
static void	pxaudc_attach(struct device *, struct device *, void *);
static int	pxaudc_detach(struct device *, int);

CFATTACH_DECL(pxaudc, sizeof(struct pxaudc_softc),
    pxaudc_match, pxaudc_attach, pxaudc_detach, NULL);

static void	pxaudc_power(int, void *);
static void	pxaudc_enable(struct pxaudc_softc *);

static int
pxaudc_match(struct device *parent, struct cfdata *cf, void *aux)
{

	if (CPU_IS_PXA270)
		return 1;
	return 0;
}

static void
pxaudc_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)self;
	struct pxaip_attach_args *pxa = (struct pxaip_attach_args *)aux;

	sc->sc_iot = pxa->pxa_iot;
	sc->sc_size = 0;
	sc->sc_powerhook = NULL;

	if (bus_space_map(sc->sc_iot, PXA2X0_USBDC_BASE, PXA2X0_USBDC_SIZE, 0,
	    &sc->sc_ioh)) {
		aprint_error(": couldn't map memory space\n");
		return;
	}
	sc->sc_size = PXA2X0_USBDC_SIZE;

	printf(": PXA2x0 USB Device Controller\n");

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, 0, sc->sc_size,
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);

	pxa2x0_gpio_set_function(35, GPIO_ALT_FN_2_IN); /* USB_P2_1 */
	pxa2x0_gpio_set_function(37, GPIO_ALT_FN_1_OUT); /* USB_P2_8 */
	pxa2x0_gpio_set_function(41, GPIO_ALT_FN_2_IN); /* USB_P2_7 */
	pxa2x0_gpio_set_function(89, GPIO_ALT_FN_2_OUT); /* USBHPEN<1> */
	pxa2x0_gpio_set_function(120, GPIO_ALT_FN_2_OUT); /* USBHPEN<2> */

	pxa2x0_clkman_config(CKEN_USBDC, 1);

	pxaudc_enable(sc);

	pxa2x0_gpio_set_bit(37); /* USB_P2_8 */

	sc->sc_powerhook = powerhook_establish(sc->sc_dev.dv_xname,
	    pxaudc_power, sc);
	if (sc->sc_powerhook == NULL) {
		aprint_error("%s: unable to establish powerhook.\n",
		    sc->sc_dev.dv_xname);
	}
}

static int
pxaudc_detach(struct device *self, int flags)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)self;

	if (sc->sc_powerhook)
		powerhook_disestablish(sc->sc_powerhook);

	if (sc->sc_size) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		sc->sc_size = 0;
	}

	return 0;
}

static void
pxaudc_power(int why, void *arg)
{
	struct pxaudc_softc *sc = (struct pxaudc_softc *)arg;

	switch (why) {
	case PWR_RESUME:
		pxaudc_enable(sc);
		break;
	}
}

static void
pxaudc_enable(struct pxaudc_softc *sc)
{
	uint32_t hr;

	/* disable the controller */
	hr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, USBDC_UDCCR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UDCCR,
	    hr & ~USBDC_UDCCR_UDE);

	hr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, USBDC_UDCICR1);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UDCICR1,
	    hr | USBDC_UDCICR1_IERS);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR, 0);
	hr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR,
	    hr | USBDC_UP2OCR_HXS);
	hr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR,
	    hr | USBDC_UP2OCR_HXOE);
	hr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, USBDC_UP2OCR,
	    hr | USBDC_UP2OCR_DPPDE|USBDC_UP2OCR_DMPDE);
}
