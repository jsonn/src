/*	$NetBSD: dzkbd.c,v 1.12.10.2 2005/03/19 08:33:58 yamt Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kbd.c	8.2 (Berkeley) 10/30/93
 */

/*
 * LK200/LK400 keyboard attached to line 0 of the DZ*-11
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dzkbd.c,v 1.12.10.2 2005/03/19 08:33:58 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/malloc.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>
#include <dev/dec/wskbdmap_lk201.h>

#include <machine/bus.h>

#include <dev/dec/dzreg.h>
#include <dev/dec/dzvar.h>
#include <dev/dec/dzkbdvar.h>
#include <dev/dec/lk201reg.h>
#include <dev/dec/lk201var.h>

#include "locators.h"

struct dzkbd_internal {
	struct dz_linestate *dzi_ls;
	struct lk201_state dzi_ks;
};

struct dzkbd_internal dzkbd_console_internal;

struct dzkbd_softc {
	struct device dzkbd_dev;	/* required first: base device */

	struct dzkbd_internal *sc_itl;

	int sc_enabled;
	int kbd_type;

	struct device *sc_wskbddev;
};

static int	dzkbd_input(void *, int);

static int	dzkbd_match(struct device *, struct cfdata *, void *);
static void	dzkbd_attach(struct device *, struct device *, void *);

CFATTACH_DECL(dzkbd, sizeof(struct dzkbd_softc),
    dzkbd_match, dzkbd_attach, NULL, NULL);

static int	dzkbd_enable(void *, int);
static void	dzkbd_set_leds(void *, int);
static int	dzkbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wskbd_accessops dzkbd_accessops = {
	dzkbd_enable,
	dzkbd_set_leds,
	dzkbd_ioctl,
};

static void	dzkbd_cngetc(void *, u_int *, int *);
static void	dzkbd_cnpollc(void *, int);

const struct wskbd_consops dzkbd_consops = {
	dzkbd_cngetc,
	dzkbd_cnpollc,
};

static int dzkbd_sendchar(void *, u_char);

const struct wskbd_mapdata dzkbd_keymapdata = {
	lkkbd_keydesctab,
#ifdef DZKBD_LAYOUT
	DZKBD_LAYOUT,
#else
	KB_US | KB_LK401,
#endif
};

/*
 * kbd_match: how is this dz line configured?
 */
static int
dzkbd_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct dzkm_attach_args *daa = aux;

	/* Exact match is better than wildcard. */
	if (cf->cf_loc[DZCF_LINE] == daa->daa_line)
		return 2;

	/* This driver accepts wildcard. */
	if (cf->cf_loc[DZCF_LINE] == DZCF_LINE_DEFAULT)
		return 1;

	return 0;
}

static void
dzkbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct dz_softc *dz = (void *)parent;
	struct dzkbd_softc *dzkbd = (void *)self;
	struct dzkm_attach_args *daa = aux;
	struct dz_linestate *ls;
	struct dzkbd_internal *dzi;
	struct wskbddev_attach_args a;
	int isconsole;

	dz->sc_dz[daa->daa_line].dz_catch = dzkbd_input;
	dz->sc_dz[daa->daa_line].dz_private = dzkbd;
	ls = &dz->sc_dz[daa->daa_line];

	isconsole = (daa->daa_flags & DZKBD_CONSOLE);

	if (isconsole) {
		dzi = &dzkbd_console_internal;
	} else {
		dzi = malloc(sizeof(struct dzkbd_internal),
				       M_DEVBUF, M_NOWAIT);
		dzi->dzi_ks.attmt.sendchar = dzkbd_sendchar;
		dzi->dzi_ks.attmt.cookie = ls;
	}
	dzi->dzi_ls = ls;
	dzkbd->sc_itl = dzi;

	printf("\n");

	if (!isconsole)
		lk201_init(&dzi->dzi_ks);

	/* XXX should identify keyboard ID here XXX */
	/* XXX layout and the number of LED is varying XXX */

	dzkbd->kbd_type = WSKBD_TYPE_LK201;

	dzkbd->sc_enabled = 1;

	a.console = isconsole;
	a.keymap = &dzkbd_keymapdata;
	a.accessops = &dzkbd_accessops;
	a.accesscookie = dzkbd;

	dzkbd->sc_wskbddev = config_found(self, &a, wskbddevprint);
}

int
dzkbd_cnattach(ls)
	struct dz_linestate *ls;
{

	dzkbd_console_internal.dzi_ks.attmt.sendchar = dzkbd_sendchar;
	dzkbd_console_internal.dzi_ks.attmt.cookie = ls;
	lk201_init(&dzkbd_console_internal.dzi_ks);
	dzkbd_console_internal.dzi_ls = ls;

	wskbd_cnattach(&dzkbd_consops, &dzkbd_console_internal,
		       &dzkbd_keymapdata);

	return 0;
}

static int
dzkbd_enable(v, on)
	void *v;
	int on;
{
	struct dzkbd_softc *sc = v;

	sc->sc_enabled = on;
	return 0;
}

static int
dzkbd_sendchar(v, c)
	void *v;
	u_char c;
{
	struct dz_linestate *ls = v;
	int s;

	s = spltty();
	dzputc(ls, c);
	splx(s);
	return (0);
}

static void
dzkbd_cngetc(v, type, data)
	void *v;
	u_int *type;
	int *data;
{
	struct dzkbd_internal *dzi = v;
	int c;

	do {
		c = dzgetc(dzi->dzi_ls);
	} while (!lk201_decode(&dzi->dzi_ks, c, type, data));
}

static void
dzkbd_cnpollc(v, on)
	void *v;
        int on;
{
#if 0
	struct dzkbd_internal *dzi = v;
#endif
}

static void
dzkbd_set_leds(v, leds)
	void *v;
	int leds;
{
	struct dzkbd_softc *sc = (struct dzkbd_softc *)v;

//printf("dzkbd_set_leds\n");
	lk201_set_leds(&sc->sc_itl->dzi_ks, leds);
}

static int
dzkbd_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct dzkbd_softc *sc = (struct dzkbd_softc *)v;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = sc->kbd_type;
		return 0;
	case WSKBDIO_SETLEDS:
		lk201_set_leds(&sc->sc_itl->dzi_ks, *(int *)data);
		return 0;
	case WSKBDIO_GETLEDS:
		/* XXX don't dig in kbd internals */
		*(int *)data = sc->sc_itl->dzi_ks.leds_state;
		return 0;
	case WSKBDIO_COMPLEXBELL:
		lk201_bell(&sc->sc_itl->dzi_ks,
			   (struct wskbd_bell_data *)data);
		return 0;
	case WSKBDIO_SETKEYCLICK:
		lk201_set_keyclick(&sc->sc_itl->dzi_ks, *(int *)data);
		return 0;
	case WSKBDIO_GETKEYCLICK:
		/* XXX don't dig in kbd internals */
		*(int *)data = sc->sc_itl->dzi_ks.kcvol;
		return 0;
	}
	return (EPASSTHROUGH);
}

static int
dzkbd_input(v, data)
	void *v;
	int data;
{
	struct dzkbd_softc *sc = (struct dzkbd_softc *)v;
	u_int type;
	int val;

	if (sc->sc_enabled == 0)
		return(0);

	if (lk201_decode(&sc->sc_itl->dzi_ks, data, &type, &val))
		wskbd_input(sc->sc_wskbddev, type, val);
	return(1);
}

