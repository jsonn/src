/* $NetBSD: psm.c,v 1.11.8.1 2001/10/01 12:46:07 fvdl Exp $ */

/*-
 * Copyright (c) 1994 Charles M. Hannum.
 * Copyright (c) 1992, 1993 Erik Forsberg.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL I BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>

#include <machine/bus.h>

#include <dev/ic/pckbcvar.h>

#include <dev/pckbc/psmreg.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

struct pms_softc {		/* driver status information */
	struct device sc_dev;

	pckbc_tag_t sc_kbctag;
	int sc_kbcslot;

	int sc_enabled;		/* input enabled? */
	void *sc_powerhook;	/* cookie from power hook */
	int inputstate;
	u_int buttons, oldbuttons;	/* mouse button status */
	signed char dx;

	struct device *sc_wsmousedev;
};

int pmsprobe __P((struct device *, struct cfdata *, void *));
void pmsattach __P((struct device *, struct device *, void *));
void pmsinput __P((void *, int));

struct cfattach pms_ca = {
	sizeof(struct pms_softc), pmsprobe, pmsattach,
};

static void	do_enable __P((struct pms_softc *));
static void	do_disable __P((struct pms_softc *));
int	pms_enable __P((void *));
int	pms_ioctl __P((void *, u_long, caddr_t, int, struct proc *));
void	pms_disable __P((void *));
void	pms_power __P((int, void *));

const struct wsmouse_accessops pms_accessops = {
	pms_enable,
	pms_ioctl,
	pms_disable,
};

int
pmsprobe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pckbc_attach_args *pa = aux;
	u_char cmd[1], resp[2];
	int res;

	if (pa->pa_slot != PCKBC_AUX_SLOT)
		return (0);

	/* Flush any garbage. */
	pckbc_flush(pa->pa_tag, pa->pa_slot);

	/* reset the device */
	cmd[0] = PMS_RESET;
	res = pckbc_poll_cmd(pa->pa_tag, pa->pa_slot, cmd, 1, 2, resp, 1);
	if (res) {
#ifdef DEBUG
		printf("pmsprobe: reset error %d\n", res);
#endif
		return (0);
	}
	if (resp[0] != PMS_RSTDONE) {
		printf("pmsprobe: reset response 0x%x\n", resp[0]);
		return (0);
	}

	/* get type number (0 = mouse) */
	if (resp[1] != 0) {
#ifdef DEBUG
		printf("pmsprobe: type 0x%x\n", resp[1]);
#endif
		return (0);
	}

	return (10);
}

void
pmsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pms_softc *sc = (void *)self;
	struct pckbc_attach_args *pa = aux;
	struct wsmousedev_attach_args a;
	u_char cmd[1], resp[2];
	int res;

	sc->sc_kbctag = pa->pa_tag;
	sc->sc_kbcslot = pa->pa_slot;

	printf("\n");

	/* Flush any garbage. */
	pckbc_flush(pa->pa_tag, pa->pa_slot);

	/* reset the device */
	cmd[0] = PMS_RESET;
	res = pckbc_poll_cmd(pa->pa_tag, pa->pa_slot, cmd, 1, 2, resp, 1);
#ifdef DEBUG
	if (res || resp[0] != PMS_RSTDONE || resp[1] != 0) {
		printf("pmsattach: reset error\n");
		return;
	}
#endif

	sc->inputstate = 0;
	sc->oldbuttons = 0;

	pckbc_set_inputhandler(sc->sc_kbctag, sc->sc_kbcslot,
			       pmsinput, sc, sc->sc_dev.dv_xname);

	a.accessops = &pms_accessops;
	a.accesscookie = sc;

	/*
	 * Attach the wsmouse, saving a handle to it.
	 * Note that we don't need to check this pointer against NULL
	 * here or in pmsintr, because if this fails pms_enable() will
	 * never be called, so pmsinput() will never be called.
	 */
	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);

	/* no interrupts until enabled */
	cmd[0] = PMS_DEV_DISABLE;
	res = pckbc_poll_cmd(pa->pa_tag, pa->pa_slot, cmd, 1, 0, 0, 0);
	if (res)
		printf("pmsattach: disable error\n");
	pckbc_slot_enable(sc->sc_kbctag, sc->sc_kbcslot, 0);

	sc->sc_powerhook = powerhook_establish(pms_power, sc);
}

static void
do_enable(sc)
	struct pms_softc *sc;
{
	u_char cmd[1];
	int res;

	sc->inputstate = 0;
	sc->oldbuttons = 0;

	pckbc_slot_enable(sc->sc_kbctag, sc->sc_kbcslot, 1);

	cmd[0] = PMS_DEV_ENABLE;
	res = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot, cmd, 1, 0, 1, 0);
	if (res)
		printf("pms_enable: command error\n");
#if 0
	{
		u_char scmd[2];

		scmd[0] = PMS_SET_RES;
		scmd[1] = 3; /* 8 counts/mm */
		res = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot, scmd,
					2, 0, 1, 0);
		if (res)
			printf("pms_enable: setup error1 (%d)\n", res);

		scmd[0] = PMS_SET_SCALE21;
		res = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot, scmd,
					1, 0, 1, 0);
		if (res)
			printf("pms_enable: setup error2 (%d)\n", res);

		scmd[0] = PMS_SET_SAMPLE;
		scmd[1] = 100; /* 100 samples/sec */
		res = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot, scmd,
					2, 0, 1, 0);
		if (res)
			printf("pms_enable: setup error3 (%d)\n", res);
	}
#endif
}

static void
do_disable(sc)
	struct pms_softc *sc;
{
	u_char cmd[1];
	int res;

	cmd[0] = PMS_DEV_DISABLE;
	res = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot, cmd, 1, 0, 1, 0);
	if (res)
		printf("pms_disable: command error\n");

	pckbc_slot_enable(sc->sc_kbctag, sc->sc_kbcslot, 0);
}

int
pms_enable(v)
	void *v;
{
	struct pms_softc *sc = v;

	if (sc->sc_enabled)
		return EBUSY;

	sc->sc_enabled = 1;

	do_enable(sc);

	return 0;
}

void
pms_disable(v)
	void *v;
{
	struct pms_softc *sc = v;

	do_disable(sc);

	sc->sc_enabled = 0;
}

void
pms_power(why, v)
	int why;
	void *v;
{
	struct pms_softc *sc = v;

	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		if (sc->sc_enabled)
			do_disable(sc);
		break;
	case PWR_RESUME:
		if (sc->sc_enabled)
			do_enable(sc);
	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
	}
}


int
pms_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct pms_softc *sc = v;
	u_char kbcmd[2];
	int i;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_PS2;
		break;
		
	case WSMOUSEIO_SRES:
		i = (*(u_int *)data - 12) / 25;
		
		if (i < 0)
			i = 0;
			
		if (i > 3)
			i = 3;

		kbcmd[0] = PMS_SET_RES;
		kbcmd[1] = i;			
		i = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot, kbcmd, 
		    2, 0, 1, 0);
		
		if (i)
			printf("pms_ioctl: SET_RES command error\n");
		break;
		
	default:
		return (-1);
	}
	return (0);
}

/* Masks for the first byte of a packet */
#define PS2LBUTMASK 0x01
#define PS2RBUTMASK 0x02
#define PS2MBUTMASK 0x04

void pmsinput(vsc, data)
void *vsc;
int data;
{
	struct pms_softc *sc = vsc;
	signed char dy;
	u_int changed;

	if (!sc->sc_enabled) {
		/* Interrupts are not expected.  Discard the byte. */
		return;
	}

	switch (sc->inputstate) {

	case 0:
		if ((data & 0xc0) == 0) { /* no ovfl, bit 3 == 1 too? */
			sc->buttons = ((data & PS2LBUTMASK) ? 0x1 : 0) |
			    ((data & PS2MBUTMASK) ? 0x2 : 0) |
			    ((data & PS2RBUTMASK) ? 0x4 : 0);
			++sc->inputstate;
		}
		break;

	case 1:
		sc->dx = data;
		/* Bounding at -127 avoids a bug in XFree86. */
		sc->dx = (sc->dx == -128) ? -127 : sc->dx;
		++sc->inputstate;
		break;

	case 2:
		dy = data;
		dy = (dy == -128) ? -127 : dy;
		sc->inputstate = 0;

		changed = (sc->buttons ^ sc->oldbuttons);
		sc->oldbuttons = sc->buttons;

		if (sc->dx || dy || changed)
			wsmouse_input(sc->sc_wsmousedev,
				      sc->buttons, sc->dx, dy, 0,
				      WSMOUSE_INPUT_DELTA);
		break;
	}

	return;
}
