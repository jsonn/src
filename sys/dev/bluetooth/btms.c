/*	$NetBSD: btms.c,v 1.1.14.2 2006/09/09 02:49:44 rpaulo Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * based on dev/usb/ums.c
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: btms.c,v 1.1.14.2 2006/09/09 02:49:44 rpaulo Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>

#include <dev/bluetooth/bthid.h>
#include <dev/bluetooth/bthidev.h>

#include <dev/usb/hid.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#define MAX_BUTTONS	31
#define BUTTON(n)	(1 << (((n) == 1 || (n) == 2) ? 3 - (n) : (n)))
#define NOTMOUSE(f)	(((f) & (HIO_CONST | HIO_RELATIVE)) != HIO_RELATIVE)

struct btms_softc {
	struct bthidev		 sc_hidev;	/* device+ */

	struct device		*sc_wsmouse;	/* child */
	int			 sc_enabled;
	uint16_t		 sc_flags;

	/* locators */
	struct hid_location	 sc_loc_x;
	struct hid_location	 sc_loc_y;
	struct hid_location	 sc_loc_z;
	struct hid_location	 sc_loc_w;
	struct hid_location	 sc_loc_button[MAX_BUTTONS];

	int			 sc_num_buttons;
	uint32_t		 sc_buttons;
};

/* sc_flags */
#define BTMS_REVZ		(1 << 0)	/* reverse Z direction */
#define BTMS_HASZ		(1 << 1)	/* has Z direction */

/* autoconf(9) methods */
static int	btms_match(struct device *, struct cfdata *, void *);
static void	btms_attach(struct device *, struct device *, void *);
static int	btms_detach(struct device *, int);

CFATTACH_DECL(btms, sizeof(struct btms_softc),
    btms_match, btms_attach, btms_detach, NULL);

/* wsmouse(4) accessops */
static int	btms_wsmouse_enable(void *);
static int	btms_wsmouse_ioctl(void *, unsigned long, caddr_t, int, struct lwp *);
static void	btms_wsmouse_disable(void *);

static const struct wsmouse_accessops btms_wsmouse_accessops = {
	btms_wsmouse_enable,
	btms_wsmouse_ioctl,
	btms_wsmouse_disable,
};

/* bthid methods */
static void btms_input(struct bthidev *, uint8_t *, int);

/*****************************************************************************
 *
 *	btms autoconf(9) routines
 */

static int
btms_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct bthidev_attach_args *ba = aux;

	if (hid_is_collection(ba->ba_desc, ba->ba_dlen, ba->ba_id,
			    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_MOUSE)))
		return 1;

	return 0;
}

static void
btms_attach(struct device *parent, struct device *self, void *aux)
{
	struct btms_softc *sc = (struct btms_softc *)self;
	struct bthidev_attach_args *ba = aux;
	struct wsmousedev_attach_args wsma;
	struct hid_location *zloc;
	uint32_t flags;
	int i, hl;

	ba->ba_input = btms_input;

	/* control the horizontal */
	hl = hid_locate(ba->ba_desc,
			ba->ba_dlen,
			HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
			ba->ba_id,
			hid_input,
			&sc->sc_loc_x,
			&flags);

	if (hl == 0 || NOTMOUSE(flags)) {
		printf("\n%s: X report 0x%04x not supported\n",
		       sc->sc_hidev.sc_dev.dv_xname, flags);

		return;
	}

	/* control the vertical */
	hl = hid_locate(ba->ba_desc,
			ba->ba_dlen,
			HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
			ba->ba_id,
			hid_input,
			&sc->sc_loc_y,
			&flags);

	if (hl == 0 || NOTMOUSE(flags)) {
		printf("\n%s: Y report 0x%04x not supported\n",
			sc->sc_hidev.sc_dev.dv_xname, flags);

		return;
	}

	/* Try the wheel first as the Z activator since it's tradition. */
	hl = hid_locate(ba->ba_desc,
			ba->ba_dlen,
			HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_WHEEL),
			ba->ba_id,
			hid_input,
			&sc->sc_loc_z,
			&flags);

	zloc = &sc->sc_loc_z;
	if (hl) {
		if (NOTMOUSE(flags)) {
			printf("\n%s: Wheel report 0x%04x not supported\n",
				sc->sc_hidev.sc_dev.dv_xname, flags);

			/* ignore Bad Z coord */
			sc->sc_loc_z.size = 0;
		} else {
			sc->sc_flags |= BTMS_HASZ;
			/* Wheels need the Z axis reversed. */
			sc->sc_flags ^= BTMS_REVZ;
			/* Put Z on the W coordinate */
			zloc = &sc->sc_loc_w;
		}
	}

	hl = hid_locate(ba->ba_desc,
			ba->ba_dlen,
			HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Z),
			ba->ba_id,
			hid_input,
			zloc,
			&flags);

	if (hl) {
		if (NOTMOUSE(flags))
			zloc->size = 0;	/* ignore Z */
		else
			sc->sc_flags |= BTMS_HASZ;
	}

	for (i = 1 ; i <= MAX_BUTTONS ; i++) {
		hl = hid_locate(ba->ba_desc,
				ba->ba_dlen,
				HID_USAGE2(HUP_BUTTON, i),
				ba->ba_id,
				hid_input,
				&sc->sc_loc_button[i - 1],
				NULL);

		if (hl == 0)
			break;
	}
	sc->sc_num_buttons = i - 1;

	aprint_normal(": %d button%s%s.\n",
			sc->sc_num_buttons,
			sc->sc_num_buttons == 1 ? "" : "s",
			sc->sc_flags & BTMS_HASZ ? " and Z dir" : "");

	wsma.accessops = &btms_wsmouse_accessops;
	wsma.accesscookie = sc;

	sc->sc_wsmouse = config_found((struct device *)sc, &wsma, wsmousedevprint);
}

static int
btms_detach(struct device *self, int flags)
{
	struct btms_softc *sc = (struct btms_softc *)self;
	int err = 0;

	if (sc->sc_wsmouse != NULL) {
		err = config_detach(sc->sc_wsmouse, flags);
		sc->sc_wsmouse = NULL;
	}

	return err;
}

/*****************************************************************************
 *
 *	wsmouse(4) accessops
 */

static int
btms_wsmouse_enable(void *self)
{
	struct btms_softc *sc = (struct btms_softc *)self;

	if (sc->sc_enabled)
		return EBUSY;

	sc->sc_enabled = 1;
	return 0;
}

static int
btms_wsmouse_ioctl(void *self, unsigned long cmd, caddr_t data, int flag, struct lwp *l)
{
	/* struct btms_softc *sc = (struct btms_softc *)self; */

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(uint *)data = WSMOUSE_TYPE_BLUETOOTH;
		break;

	default:
		return EPASSTHROUGH;
	}

	return 0;
}

static void
btms_wsmouse_disable(void *self)
{
	struct btms_softc *sc = (struct btms_softc *)self;

	sc->sc_enabled = 0;
}

/*****************************************************************************
 *
 *	btms input routine, called from our parent
 */

static void
btms_input(struct bthidev *self, uint8_t *data, int len)
{
	struct btms_softc *sc = (struct btms_softc *)self;
	int dx, dy, dz, dw;
	uint32_t buttons;
	int i, s;

	if (sc->sc_wsmouse == NULL || sc->sc_enabled == 0)
		return;

	dx =  hid_get_data(data, &sc->sc_loc_x);
	dy = -hid_get_data(data, &sc->sc_loc_y);
	dz =  hid_get_data(data, &sc->sc_loc_z);
	dw =  hid_get_data(data, &sc->sc_loc_w);

	if (sc->sc_flags & BTMS_REVZ)
		dz = -dz;

	buttons = 0;
	for (i = 0 ; i < sc->sc_num_buttons ; i++)
		if (hid_get_data(data, &sc->sc_loc_button[i]))
			buttons |= BUTTON(i);

	if (dx != 0 || dy != 0 || dz != 0 || dw != 0 || buttons != sc->sc_buttons) {
		sc->sc_buttons = buttons;

		s = spltty();
		wsmouse_input_xyzw(sc->sc_wsmouse, buttons,
				   dx, dy, dz, dw,
				   WSMOUSE_INPUT_DELTA);
		splx(s);
	}
}
