/* $NetBSD: pckbd.c,v 1.17.2.1 1999/06/25 20:59:16 perry Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)pccons.c	5.11 (Berkeley) 5/21/91
 */

/*
 * code to work keyboard for PC-style console
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/pckbcvar.h>

#include <dev/pckbc/pckbdreg.h>
#include <dev/pckbc/pckbdvar.h>
#include <dev/pckbc/wskbdmap_mfii.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#if defined(__i386__) || defined(__alpha__)
#include <sys/kernel.h> /* XXX for hz */
#endif

#include "locators.h"

#include "opt_pckbd_layout.h"
#include "opt_wsdisplay_compat.h"

struct pckbd_internal {
	int t_isconsole;
	pckbc_tag_t t_kbctag;
	pckbc_slot_t t_kbcslot;

	int t_lastchar;
	int t_extended;
	int t_extended1;

	struct pckbd_softc *t_sc; /* back pointer */
};

struct pckbd_softc {
        struct  device sc_dev;

	struct pckbd_internal *id;
	int sc_enabled;

	int sc_ledstate;

	struct device *sc_wskbddev;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int rawkbd;
#endif
};

static int pckbd_is_console __P((pckbc_tag_t, pckbc_slot_t));

int pckbdprobe __P((struct device *, struct cfdata *, void *));
void pckbdattach __P((struct device *, struct device *, void *));

struct cfattach pckbd_ca = {
	sizeof(struct pckbd_softc), pckbdprobe, pckbdattach,
};

int	pckbd_enable __P((void *, int));
void	pckbd_set_leds __P((void *, int));
int	pckbd_ioctl __P((void *, u_long, caddr_t, int, struct proc *));

const struct wskbd_accessops pckbd_accessops = {
	pckbd_enable,
	pckbd_set_leds,
	pckbd_ioctl,
};

void	pckbd_cngetc __P((void *, u_int *, int *));
void	pckbd_cnpollc __P((void *, int));

const struct wskbd_consops pckbd_consops = {
	pckbd_cngetc,
	pckbd_cnpollc,
};

const struct wskbd_mapdata pckbd_keymapdata = {
	pckbd_keydesctab,
#ifdef PCKBD_LAYOUT
	PCKBD_LAYOUT,
#else
	KB_US,
#endif
};

int	pckbd_set_xtscancode __P((pckbc_tag_t, pckbc_slot_t));
int	pckbd_init __P((struct pckbd_internal *, pckbc_tag_t, pckbc_slot_t,
			int));
void	pckbd_input __P((void *, int));

static int	pckbd_decode __P((struct pckbd_internal *, int,
				  u_int *, int *));
static int	pckbd_led_encode __P((int));
static int	pckbd_led_decode __P((int));

struct pckbd_internal pckbd_consdata;

int
pckbd_set_xtscancode(kbctag, kbcslot)
	pckbc_tag_t kbctag;
	pckbc_slot_t kbcslot;
{
	u_char cmd[2];
	int res;

	/*
	 * Some keyboard/8042 combinations do not seem to work if the keyboard
	 * is set to table 1; in fact, it would appear that some keyboards just
	 * ignore the command altogether.  So by default, we use the AT scan
	 * codes and have the 8042 translate them.  Unfortunately, this is
	 * known to not work on some PS/2 machines.  We try desparately to deal
	 * with this by checking the (lack of a) translate bit in the 8042 and
	 * attempting to set the keyboard to XT mode.  If this all fails, well,
	 * tough luck.
	 *
	 * XXX It would perhaps be a better choice to just use AT scan codes
	 * and not bother with this.
	 */
	if (pckbc_xt_translation(kbctag, kbcslot, 1)) {
		/* The 8042 is translating for us; use AT codes. */
		cmd[0] = KBC_SETTABLE;
		cmd[1] = 2;
		res = pckbc_poll_cmd(kbctag, kbcslot, cmd, 2, 0, 0, 0);
		if (res) {
			u_char cmd[1];
#ifdef DEBUG
			printf("pckbd: error setting scanset 2\n");
#endif
			/*
			 * XXX at least one keyboard is reported to lock up
			 * if a "set table" is attempted, thus the "reset".
			 * XXX ignore errors, scanset 2 should be
			 * default anyway.
			 */
			cmd[0] = KBC_RESET;
			(void)pckbc_poll_cmd(kbctag, kbcslot, cmd, 1, 1, 0, 1);
			pckbc_flush(kbctag, kbcslot);
			res = 0;
		}
	} else {
		/* Stupid 8042; set keyboard to XT codes. */
		cmd[0] = KBC_SETTABLE;
		cmd[1] = 1;
		res = pckbc_poll_cmd(kbctag, kbcslot, cmd, 2, 0, 0, 0);
#ifdef DEBUG
		if (res)
			printf("pckbd: error setting scanset 1\n");
#endif
	}
	return (res);
}

static int
pckbd_is_console(tag, slot)
	pckbc_tag_t tag;
	pckbc_slot_t slot;
{
	return (pckbd_consdata.t_isconsole &&
		(tag == pckbd_consdata.t_kbctag) &&
		(slot == pckbd_consdata.t_kbcslot));
}

/*
 * these are both bad jokes
 */
int
pckbdprobe(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct pckbc_attach_args *pa = aux;
	u_char cmd[1], resp[1];
	int res;

	/*
	 * XXX There are rumours that a keyboard can be connected
	 * to the aux port as well. For me, this didn't work.
	 * For further experiments, allow it if explicitely
	 * wired in the config file.
	 */
	if ((pa->pa_slot != PCKBC_KBD_SLOT) &&
	    (cf->cf_loc[PCKBCCF_SLOT] == PCKBCCF_SLOT_DEFAULT))
		return (0);

	/* Flush any garbage. */
	pckbc_flush(pa->pa_tag, pa->pa_slot);

	/* Reset the keyboard. */
	cmd[0] = KBC_RESET;
	res = pckbc_poll_cmd(pa->pa_tag, pa->pa_slot, cmd, 1, 1, resp, 1);
	if (res) {
#ifdef DEBUG
		printf("pckbdprobe: reset error %d\n", res);
#endif
		/*
		 * There is probably no keyboard connected.
		 * Let the probe succeed if the keyboard is used
		 * as console input - it can be connected later.
		 */
		return (pckbd_is_console(pa->pa_tag, pa->pa_slot) ? 1 : 0);
	}
	if (resp[0] != KBR_RSTDONE) {
		printf("pckbdprobe: reset response 0x%x\n", resp[0]);
		return (0);
	}

	/*
	 * Some keyboards seem to leave a second ack byte after the reset.
	 * This is kind of stupid, but we account for them anyway by just
	 * flushing the buffer.
	 */
	pckbc_flush(pa->pa_tag, pa->pa_slot);

	if (pckbd_set_xtscancode(pa->pa_tag, pa->pa_slot))
		return (0);

	return (2);
}

void
pckbdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pckbd_softc *sc = (void *)self;
	struct pckbc_attach_args *pa = aux;
	int isconsole;
	struct wskbddev_attach_args a;

	printf("\n");

	isconsole = pckbd_is_console(pa->pa_tag, pa->pa_slot);

	if (isconsole) {
		sc->id = &pckbd_consdata;
		sc->sc_enabled = 1;
	} else {
		u_char cmd[1];

		sc->id = malloc(sizeof(struct pckbd_internal),
				M_DEVBUF, M_WAITOK);
		(void) pckbd_init(sc->id, pa->pa_tag, pa->pa_slot, 0);

		/* no interrupts until enabled */
		cmd[0] = KBC_DISABLE;
		(void) pckbc_poll_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
				      cmd, 1, 0, 0, 0);
		sc->sc_enabled = 0;
	}

	sc->id->t_sc = sc;

	pckbc_set_inputhandler(sc->id->t_kbctag, sc->id->t_kbcslot,
			       pckbd_input, sc);

	a.console = isconsole;

	a.keymap = &pckbd_keymapdata;

	a.accessops = &pckbd_accessops;
	a.accesscookie = sc;

	/*
	 * Attach the wskbd, saving a handle to it.
	 * XXX XXX XXX
	 */
	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);
}

int
pckbd_enable(v, on)
	void *v;
	int on;
{
	struct pckbd_softc *sc = v;
	u_char cmd[1];
	int res;

	if (on) {
		if (sc->sc_enabled)
			return (EBUSY);

		pckbc_slot_enable(sc->id->t_kbctag, sc->id->t_kbcslot, 1);

		cmd[0] = KBC_ENABLE;
		res = pckbc_enqueue_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
					cmd, 1, 0, 0, 0);
		if (res) {
			printf("pckbd_enable: command error\n");
			return (res);
		}

		res = pckbd_set_xtscancode(sc->id->t_kbctag,
					   sc->id->t_kbcslot);
		if (res)
			return (res);

		sc->sc_enabled = 1;
	} else {
		if (sc->id->t_isconsole)
			return (EBUSY);

		cmd[0] = KBC_DISABLE;
		res = pckbc_enqueue_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
					cmd, 1, 0, 0, 0);
		if (res) {
			printf("pckbd_disable: command error\n");
			return (res);
		}

		pckbc_slot_enable(sc->id->t_kbctag, sc->id->t_kbcslot, 0);

		sc->sc_enabled = 0;
	}

	return (0);
}

static int
pckbd_decode(id, datain, type, dataout)
	struct pckbd_internal *id;
	int datain;
	u_int *type;
	int *dataout;
{
	int key;

	if (datain == KBR_EXTENDED0) {
		id->t_extended = 1;
		return(0);
	} else if (datain == KBR_EXTENDED1) {
		id->t_extended1 = 2;
		return(0);
	}

 	/* map extended keys to (unused) codes 128-254 */
	key = (datain & 0x7f) | (id->t_extended ? 0x80 : 0);
	id->t_extended = 0;

	/*
	 * process BREAK key (EXT1 1D 45  EXT1 9D C5):
	 * map to (unused) code 7F
	 */
	if (id->t_extended1 == 2 && (datain == 0x1d || datain == 0x9d)) {
		id->t_extended1 = 1;
		return(0);
	} else if (id->t_extended1 == 1 &&
		   (datain == 0x45 || datain == 0xc5)) {
		id->t_extended1 = 0;
		key = 0x7f;
	} else if (id->t_extended1 > 0) {
		id->t_extended1 = 0;
	}

	if (datain & 0x80) {
		id->t_lastchar = 0;
		*type = WSCONS_EVENT_KEY_UP;
	} else {
		/* Always ignore typematic keys */
		if (key == id->t_lastchar)
			return(0);
		id->t_lastchar = key;
		*type = WSCONS_EVENT_KEY_DOWN;
	}

	*dataout = key;
	return(1);
}

int
pckbd_init(t, kbctag, kbcslot, console)
	struct pckbd_internal *t;
	pckbc_tag_t kbctag;
	pckbc_slot_t kbcslot;
	int console;
{
	bzero(t, sizeof(struct pckbd_internal));

	t->t_isconsole = console;
	t->t_kbctag = kbctag;
	t->t_kbcslot = kbcslot;

	return (pckbd_set_xtscancode(kbctag, kbcslot));
}

static int
pckbd_led_encode(led)
	int led;
{
	int res;

	res = 0;

	if (led & WSKBD_LED_SCROLL)
		res |= 0x01;
	if (led & WSKBD_LED_NUM)
		res |= 0x02;
	if (led & WSKBD_LED_CAPS)
		res |= 0x04;
	return(res);
}

static int
pckbd_led_decode(led)
	int led;
{
	int res;

	res = 0;
	if (led & 0x01)
		res |= WSKBD_LED_SCROLL;
	if (led & 0x02)
		res |= WSKBD_LED_NUM;
	if (led & 0x04)
		res |= WSKBD_LED_CAPS;
	return(res);
}

void
pckbd_set_leds(v, leds)
	void *v;
	int leds;
{
	struct pckbd_softc *sc = v;
	u_char cmd[2];

	cmd[0] = KBC_MODEIND;
	cmd[1] = pckbd_led_encode(leds);
	sc->sc_ledstate = cmd[1];

	(void) pckbc_enqueue_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
				 cmd, 2, 0, 0, 0);
}

/*
 * Got a console receive interrupt -
 * the console processor wants to give us a character.
 */
void
pckbd_input(vsc, data)
	void *vsc;
	int data;
{
	struct pckbd_softc *sc = vsc;
	int type, key;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (sc->rawkbd) {
		char d = data;
		wskbd_rawinput(sc->sc_wskbddev, &d, 1);
		return;
	}
#endif
	if (pckbd_decode(sc->id, data, &type, &key))
		wskbd_input(sc->sc_wskbddev, type, key);
}

int
pckbd_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct pckbd_softc *sc = v;

	switch (cmd) {
	    case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_PC_XT;
		return 0;
	    case WSKBDIO_SETLEDS: {
		char cmd[2];
		int res;
		cmd[0] = KBC_MODEIND;
		cmd[1] = pckbd_led_encode(*(int *)data);
		sc->sc_ledstate = cmd[1];
		res = pckbc_enqueue_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
					cmd, 2, 0, 1, 0);
		return (res);
		}
	    case WSKBDIO_GETLEDS:
		*(int *)data = pckbd_led_decode(sc->sc_ledstate);
		return (0);
	    case WSKBDIO_COMPLEXBELL:
#define d ((struct wskbd_bell_data *)data)
		/* keyboard can't beep - use md code */
#ifdef __i386__
		sysbeep(d->pitch, d->period * hz / 1000);
		/* comes in as ms, goes out as ticks; volume ignored */
#endif
#ifdef __alpha__
		isabeep(d->pitch, d->period * hz / 1000);
		/* comes in as ms, goes out as ticks; volume ignored */
#endif
#undef d
		return (0);
#ifdef WSDISPLAY_COMPAT_RAWKBD
	    case WSKBDIO_SETMODE:
		sc->rawkbd = (*(int *)data == WSKBD_RAW);
		return (0);
#endif
	}
	return -1;
}

int
pckbd_cnattach(kbctag, kbcslot)
	pckbc_tag_t kbctag;
	int kbcslot;
{
	char cmd[1];
	int res;

	res = pckbd_init(&pckbd_consdata, kbctag, kbcslot, 1);
#if 0 /* we allow the console to be attached if no keyboard is present */
	if (res)
		return (res);
#endif

	/* Just to be sure. */
	cmd[0] = KBC_ENABLE;
	res = pckbc_poll_cmd(kbctag, kbcslot, cmd, 1, 0, 0, 0);
#if 0
	if (res)
		return (res);
#endif

	wskbd_cnattach(&pckbd_consops, &pckbd_consdata, &pckbd_keymapdata);

	return (0);
}

/* ARGSUSED */
void
pckbd_cngetc(v, type, data)
	void *v;
	u_int *type;
	int *data;
{
        struct pckbd_internal *t = v;
	int val;

	for (;;) {
		val = pckbc_poll_data(t->t_kbctag, t->t_kbcslot);
		if ((val != -1) && pckbd_decode(t, val, type, data))
			return;
	}
}

void
pckbd_cnpollc(v, on)
	void *v;
        int on;
{
	struct pckbd_internal *t = v;

	pckbc_set_poll(t->t_kbctag, t->t_kbcslot, on);
}
