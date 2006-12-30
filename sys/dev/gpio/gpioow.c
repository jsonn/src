/* $NetBSD: gpioow.c,v 1.1.14.3 2006/12/30 20:48:00 yamt Exp $ */
/*	$OpenBSD: gpioow.c,v 1.1 2006/03/04 16:27:03 grange Exp $	*/

/*
 * Copyright (c) 2006 Alexander Yurchenko <grange@openbsd.org>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gpioow.c,v 1.1.14.3 2006/12/30 20:48:00 yamt Exp $");

/*
 * 1-Wire bus bit-banging through GPIO pin.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>

#include <dev/gpio/gpiovar.h>

#include <dev/onewire/onewirevar.h>

#define GPIOOW_NPINS		1
#define GPIOOW_PIN_DATA		0

struct gpioow_softc {
	struct device		sc_dev;

	void *			sc_gpio;
	struct gpio_pinmap	sc_map;
	int			__map[GPIOOW_NPINS];

	struct onewire_bus	sc_ow_bus;
	struct device *		sc_ow_dev;

	int			sc_data;
	int			sc_dying;
};

int	gpioow_match(struct device *, struct cfdata *, void *);
void	gpioow_attach(struct device *, struct device *, void *);
int	gpioow_detach(struct device *, int);
int	gpioow_activate(struct device *, enum devact);

int	gpioow_ow_reset(void *);
int	gpioow_ow_bit(void *, int);

void	gpioow_bb_rx(void *);
void	gpioow_bb_tx(void *);
int	gpioow_bb_get(void *);
void	gpioow_bb_set(void *, int);

CFATTACH_DECL(gpioow, sizeof(struct gpioow_softc),
	gpioow_match, gpioow_attach, gpioow_detach, gpioow_activate);

extern struct cfdriver gpioow_cd;

static const struct onewire_bbops gpioow_bbops = {
	gpioow_bb_rx,
	gpioow_bb_tx,
	gpioow_bb_get,
	gpioow_bb_set
};

int
gpioow_match(struct device *parent, struct cfdata *cf,
    void *aux)
{
	return 1;
}

void
gpioow_attach(struct device *parent, struct device *self, void *aux)
{
	struct gpioow_softc *sc = device_private(self);
	struct gpio_attach_args *ga = aux;
	struct onewirebus_attach_args oba;
	int caps;

	/* Check that we have enough pins */
	if (gpio_npins(ga->ga_mask) != GPIOOW_NPINS) {
		printf(": invalid pin mask\n");
		return;
	}

	/* Map pins */
	sc->sc_gpio = ga->ga_gpio;
	sc->sc_map.pm_map = sc->__map;
	if (gpio_pin_map(sc->sc_gpio, ga->ga_offset, ga->ga_mask,
	    &sc->sc_map)) {
		printf(": can't map pins\n");
		return;
	}

	/* Configure data pin */
	caps = gpio_pin_caps(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA);
	if (!(caps & GPIO_PIN_OUTPUT)) {
		printf(": data pin is unable to drive output\n");
		goto fail;
	}
	if (!(caps & GPIO_PIN_INPUT)) {
		printf(": data pin is unable to read input\n");
		goto fail;
	}
	printf(": DATA[%d]", sc->sc_map.pm_map[GPIOOW_PIN_DATA]);
	sc->sc_data = GPIO_PIN_OUTPUT;
	if (caps & GPIO_PIN_OPENDRAIN) {
		printf(" open-drain");
		sc->sc_data |= GPIO_PIN_OPENDRAIN;
	} else if ((caps & GPIO_PIN_PUSHPULL) && (caps & GPIO_PIN_TRISTATE)) {
		printf(" push-pull tri-state");
		sc->sc_data |= GPIO_PIN_PUSHPULL;
	}
	if (caps & GPIO_PIN_PULLUP) {
		printf(" pull-up");
		sc->sc_data |= GPIO_PIN_PULLUP;
	}
	gpio_pin_ctl(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA, sc->sc_data);

	printf("\n");

	/* Attach 1-Wire bus */
	sc->sc_ow_bus.bus_cookie = sc;
	sc->sc_ow_bus.bus_reset = gpioow_ow_reset;
	sc->sc_ow_bus.bus_bit = gpioow_ow_bit;

	bzero(&oba, sizeof(oba));
	oba.oba_bus = &sc->sc_ow_bus;
	sc->sc_ow_dev = config_found(self, &oba, onewirebus_print);

	return;

fail:
	gpio_pin_unmap(sc->sc_gpio, &sc->sc_map);
}

int
gpioow_detach(struct device *self, int flags)
{
	struct gpioow_softc *sc = device_private(self);
	int rv = 0;

	if (sc->sc_ow_dev != NULL)
		rv = config_detach(sc->sc_ow_dev, flags);

	return (rv);
}

int
gpioow_activate(struct device *self, enum devact act)
{
	struct gpioow_softc *sc = device_private(self);
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		if (sc->sc_ow_dev != NULL)
			rv = config_deactivate(sc->sc_ow_dev);
		break;
	}

	return (rv);
}

int
gpioow_ow_reset(void *arg)
{
	return (onewire_bb_reset(&gpioow_bbops, arg));
}

int
gpioow_ow_bit(void *arg, int value)
{
	return (onewire_bb_bit(&gpioow_bbops, arg, value));
}

void
gpioow_bb_rx(void *arg)
{
	struct gpioow_softc *sc = arg;
	int data = sc->sc_data;

	data &= ~(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | GPIO_PIN_TRISTATE);
	data |= GPIO_PIN_INPUT;
	if (data & GPIO_PIN_PUSHPULL)
		data |= GPIO_PIN_TRISTATE;
	if (sc->sc_data != data) {
		sc->sc_data = data;
		gpio_pin_ctl(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA,
		    sc->sc_data);
	}
}

void
gpioow_bb_tx(void *arg)
{
	struct gpioow_softc *sc = arg;
	int data = sc->sc_data;

	data &= ~(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | GPIO_PIN_TRISTATE);
	data |= GPIO_PIN_OUTPUT;
	if (sc->sc_data != data) {
		sc->sc_data = data;
		gpio_pin_ctl(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA,
		    sc->sc_data);
	}
}

int
gpioow_bb_get(void *arg)
{
	struct gpioow_softc *sc = arg;

	return (gpio_pin_read(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA) ==
	    GPIO_PIN_HIGH ? 1 : 0);
}

void
gpioow_bb_set(void *arg, int value)
{
	struct gpioow_softc *sc = arg;

	gpio_pin_write(sc->sc_gpio, &sc->sc_map, GPIOOW_PIN_DATA,
	    value ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
}
