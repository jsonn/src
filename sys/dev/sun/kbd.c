/*	$NetBSD: kbd.c,v 1.37.2.1 2004/08/03 10:51:16 skrll Exp $	*/

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
 * Keyboard driver (/dev/kbd -- note that we do not have minor numbers
 * [yet?]).  Translates incoming bytes to ASCII or to `firm_events' and
 * passes them up to the appropriate reader.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kbd.c,v 1.37.2.1 2004/08/03 10:51:16 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/file.h>

#include <dev/wscons/wsksymdef.h>

#include <dev/sun/kbd_reg.h>
#include <dev/sun/kbio.h>
#include <dev/sun/vuid_event.h>
#include <dev/sun/event_var.h>
#include <dev/sun/kbd_xlate.h>
#include <dev/sun/kbdvar.h>

#include "locators.h"

extern struct cfdriver kbd_cd;

dev_type_open(kbdopen);
dev_type_close(kbdclose);
dev_type_read(kbdread);
dev_type_ioctl(kbdioctl);
dev_type_poll(kbdpoll);
dev_type_kqfilter(kbdkqfilter);

const struct cdevsw kbd_cdevsw = {
	kbdopen, kbdclose, kbdread, nowrite, kbdioctl,
	nostop, notty, kbdpoll, nommap, kbdkqfilter
};

#if NWSKBD > 0
int	wssunkbd_enable __P((void *, int));
void	wssunkbd_set_leds __P((void *, int));
int	wssunkbd_ioctl __P((void *, u_long, caddr_t, int, struct proc *));

void    sunkbd_wskbd_cngetc __P((void *, u_int *, int *));
void    sunkbd_wskbd_cnpollc __P((void *, int));
void    sunkbd_wskbd_cnbell __P((void *, u_int, u_int, u_int));
static void sunkbd_bell_off(void *v);

const struct wskbd_accessops sunkbd_wskbd_accessops = {
	wssunkbd_enable,
	wssunkbd_set_leds,
	wssunkbd_ioctl,
};

extern const struct wscons_keydesc wssun_keydesctab[];
const struct wskbd_mapdata sunkbd_wskbd_keymapdata = {
	wssun_keydesctab,
#ifdef SUNKBD_LAYOUT
	SUNKBD_LAYOUT,
#else
	KB_US,
#endif
};

const struct wskbd_consops sunkbd_wskbd_consops = {
        sunkbd_wskbd_cngetc,
        sunkbd_wskbd_cnpollc,
        sunkbd_wskbd_cnbell,
};

void kbd_wskbd_attach(struct kbd_softc *k, int isconsole);
#endif

/* ioctl helpers */
static int kbd_iockeymap(struct kbd_state *ks,
			 u_long cmd, struct kiockeymap *kio);
#ifdef KIOCGETKEY
static int kbd_oldkeymap(struct kbd_state *ks,
			 u_long cmd, struct okiockey *okio);
#endif


/* callbacks for console driver */
static int kbd_cc_open(struct cons_channel *);
static int kbd_cc_close(struct cons_channel *);

/* console input */
static void	kbd_input_console(struct kbd_softc *, int);
static void	kbd_repeat(void *);
static int	kbd_input_keysym(struct kbd_softc *, int);
static void	kbd_input_string(struct kbd_softc *, char *);
static void	kbd_input_funckey(struct kbd_softc *, int);
static void	kbd_update_leds(struct kbd_softc *);

#if NWSKBD > 0
static void	kbd_input_wskbd(struct kbd_softc *, int);
#endif

/* firm events input */
static void	kbd_input_event(struct kbd_softc *, int);



/****************************************************************
 *  Entry points for /dev/kbd
 *  (open,close,read,write,...)
 ****************************************************************/

/*
 * Open:
 * Check exclusion, open actual device (_iopen),
 * setup event channel, clear ASCII repeat stuff.
 */
int
kbdopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	struct kbd_softc *k;
	int error, unit;

	/* locate device */
	unit = minor(dev);
	if (unit >= kbd_cd.cd_ndevs)
		return (ENXIO);
	k = kbd_cd.cd_devs[unit];
	if (k == NULL)
		return (ENXIO);

	/*
	 * NB: wscons support: while we can track if wskbd has called
	 * enable(), we can't tell if that's for console input or for
	 * events input, so we should probably just let the open to
	 * always succeed regardless (e.g. Xsun opening /dev/kbd).
	 */

	/* exclusive open required for /dev/kbd */
	if (k->k_events.ev_io)
		return (EBUSY);
	k->k_events.ev_io = p;

	/* stop pending autorepeat of console input */
	if (k->k_repeating) {
		k->k_repeating = 0;
		callout_stop(&k->k_repeat_ch);
	}

	/* open actual underlying device */
	if (k->k_ops != NULL && k->k_ops->open != NULL)
		if ((error = (*k->k_ops->open)(k)) != 0) {
			k->k_events.ev_io = NULL;
			return (error);
		}

	ev_init(&k->k_events);
	k->k_evmode = 0;	/* XXX: OK? */

	return (0);
}


/*
 * Close:
 * Turn off event mode, dump the queue, and close the keyboard
 * unless it is supplying console input.
 */
int
kbdclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	struct kbd_softc *k;

	k = kbd_cd.cd_devs[minor(dev)];
	k->k_evmode = 0;
	ev_fini(&k->k_events);
	k->k_events.ev_io = NULL;

	if (k->k_ops != NULL && k->k_ops->close != NULL) {
		int error;
		if ((error = (*k->k_ops->close)(k)) != 0)
			return (error);
	}
	return (0);
}


int
kbdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct kbd_softc *k;

	k = kbd_cd.cd_devs[minor(dev)];
	return (ev_read(&k->k_events, uio, flags));
}


int
kbdpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	struct kbd_softc *k;

	k = kbd_cd.cd_devs[minor(dev)];
	return (ev_poll(&k->k_events, events, p));
}

int
kbdkqfilter(dev, kn)
	dev_t dev;
	struct knote *kn;
{
	struct kbd_softc *k;

	k = kbd_cd.cd_devs[minor(dev)];
	return (ev_kqfilter(&k->k_events, kn));
}

int
kbdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct kbd_softc *k;
	struct kbd_state *ks;
	int error = 0;

	k = kbd_cd.cd_devs[minor(dev)];
	ks = &k->k_state;

	switch (cmd) {

	case KIOCTRANS: 	/* Set translation mode */
		/* We only support "raw" mode on /dev/kbd */
		if (*(int *)data != TR_UNTRANS_EVENT)
			error = EINVAL;
		break;

	case KIOCGTRANS:	/* Get translation mode */
		/* We only support "raw" mode on /dev/kbd */
		*(int *)data = TR_UNTRANS_EVENT;
		break;

#ifdef KIOCGETKEY
	case KIOCGETKEY:	/* Get keymap entry (old format) */
		error = kbd_oldkeymap(ks, cmd, (struct okiockey *)data);
		break;
#endif /* KIOCGETKEY */

	case KIOCSKEY:  	/* Set keymap entry */
		/* FALLTHROUGH */
	case KIOCGKEY:  	/* Get keymap entry */
		error = kbd_iockeymap(ks, cmd, (struct kiockeymap *)data);
		break;

	case KIOCCMD:		/* Send a command to the keyboard */
		/* pass it to the middle layer */
		if (k->k_ops != NULL && k->k_ops->docmd != NULL)
			error = (*k->k_ops->docmd)(k, *(int *)data, 1);
		break;

	case KIOCTYPE:		/* Get keyboard type */
		*(int *)data = ks->kbd_id;
		break;

	case KIOCSDIRECT:	/* Where to send input */
		k->k_evmode = *(int *)data;
		break;

	case KIOCLAYOUT:	/* Get keyboard layout */
		*(int *)data = ks->kbd_layout;
		break;

	case KIOCSLED:		/* Set keyboard LEDs */
		/* pass the request to the middle layer */
		if (k->k_ops != NULL && k->k_ops->setleds != NULL)
			error = (*k->k_ops->setleds)(k, *(char *)data, 1);
		break;

	case KIOCGLED:		/* Get keyboard LEDs */
		*(char *)data = ks->kbd_leds;
		break;

	case FIONBIO:		/* we will remove this someday (soon???) */
		break;

	case FIOASYNC:
		k->k_events.ev_async = (*(int *)data != 0);
		break;

	case FIOSETOWN:
		if (-*(int *)data != k->k_events.ev_io->p_pgid
		    && *(int *)data != k->k_events.ev_io->p_pid)
			error = EPERM;
		break;

	case TIOCSPGRP:
		if (*(int *)data != k->k_events.ev_io->p_pgid)
			error = EPERM;
		break;

	default:
		error = ENOTTY;
		break;
	}

	return (error);
}


/****************************************************************
 * ioctl helpers
 ****************************************************************/

/*
 * Get/Set keymap entry
 */
static int
kbd_iockeymap(ks, cmd, kio)
	struct kbd_state *ks;
	u_long cmd;
	struct kiockeymap *kio;
{
	u_short *km;
	u_int station;

	switch (kio->kio_tablemask) {
	case KIOC_NOMASK:
		km = ks->kbd_k.k_normal;
		break;
	case KIOC_SHIFTMASK:
		km = ks->kbd_k.k_shifted;
		break;
	case KIOC_CTRLMASK:
		km = ks->kbd_k.k_control;
		break;
	case KIOC_UPMASK:
		km = ks->kbd_k.k_release;
		break;
	default:
		/* Silently ignore unsupported masks */
		return (0);
	}

	/* Range-check the table position. */
	station = kio->kio_station;
	if (station >= KEYMAP_SIZE)
		return (EINVAL);

	switch (cmd) {

	case KIOCGKEY:	/* Get keymap entry */
		kio->kio_entry = km[station];
		break;

	case KIOCSKEY:	/* Set keymap entry */
		km[station] = kio->kio_entry;
		break;

	default:
		return(ENOTTY);
	}
	return (0);
}


#ifdef KIOCGETKEY
/*
 * Get/Set keymap entry,
 * old format (compatibility)
 */
int
kbd_oldkeymap(ks, cmd, kio)
	struct kbd_state *ks;
	u_long cmd;
	struct okiockey *kio;
{
	int error = 0;

	switch (cmd) {

	case KIOCGETKEY:
		if (kio->kio_station == 118) {
			/*
			 * This is X11 asking if a type 3 keyboard is
			 * really a type 3 keyboard.  Say yes, it is,
			 * by reporting key station 118 as a "hole".
			 * Note old (SunOS 3.5) definition of HOLE!
			 */
			kio->kio_entry = 0xA2;
			break;
		}
		/* fall through */

	default:
		error = ENOTTY;
		break;
	}

	return (error);
}
#endif /* KIOCGETKEY */



/****************************************************************
 *  Keyboard input - called by middle layer at spltty().
 ****************************************************************/

void
kbd_input(k, code)
	struct kbd_softc *k;
	int code;
{
	if (k->k_evmode) {

#ifdef KBD_IDLE_EVENTS
		/*
		 * XXX: is this still true?
		 * IDLEs confuse the MIT X11R4 server badly, so we must drop them.
		 * This is bad as it means the server will not automatically resync
		 * on all-up IDLEs, but I did not drop them before, and the server
		 * goes crazy when it comes time to blank the screen....
		 */
		if (code == KBD_IDLE)
			return;
#endif

		/*
		 * Keyboard is generating firm events.  Turn this keystroke
		 * into an event and put it in the queue.
		 */
		kbd_input_event(k, code);
		return;
	}

#if NWSKBD > 0
	if (k->k_wskbd != NULL && k->k_wsenabled) {
		/*
		 * We are using wskbd input mode, pass the event up.
		 */
		kbd_input_wskbd(k, code);
		return;
	}
#endif

	/*
	 * If /dev/kbd is not connected in event mode, or wskbd mode,
	 * translate and send upstream (to console).
	 */
	kbd_input_console(k, code);
}



/****************************************************************
 *  Open/close routines called upon opening /dev/console
 *  if we serve console input.
 ****************************************************************/

struct cons_channel *
kbd_cc_alloc(k)
	struct kbd_softc *k;
{
	struct cons_channel *cc;

	if ((cc = malloc(sizeof *cc, M_DEVBUF, M_NOWAIT)) == NULL)
		return (NULL);

	/* our callbacks for the console driver */
	cc->cc_dev = k;
	cc->cc_iopen = kbd_cc_open;
	cc->cc_iclose = kbd_cc_close;

	/* will be provided by the console driver so that we can feed input */
	cc->cc_upstream = NULL;

	/*
	 * TODO: clean up cons_attach_input() vs kd_attach_input() in
	 * lower layers and move that code here.
	 */

	k->k_cc = cc;
	return (cc);
}


static int
kbd_cc_open(cc)
	struct cons_channel *cc;
{
	struct kbd_softc *k;
	int ret;

	if (cc == NULL)
		return (0);

	k = (struct kbd_softc *)cc->cc_dev;
	if (k == NULL)
		return (0);

	if (k->k_ops != NULL && k->k_ops->open != NULL)
		ret = (*k->k_ops->open)(k);
	else
		ret = 0;

	/* XXX: verify that callout is not active? */
	k->k_repeat_start = hz/2;
	k->k_repeat_step = hz/20;
	callout_init(&k->k_repeat_ch);

	return (ret);
}


static int
kbd_cc_close(cc)
	struct cons_channel *cc;
{
	struct kbd_softc *k;
	int ret;

	if (cc == NULL)
		return (0);

	k = (struct kbd_softc *)cc->cc_dev;
	if (k == NULL)
		return (0);

	if (k->k_ops != NULL && k->k_ops->close != NULL)
		ret = (*k->k_ops->close)(k);
	else
		ret = 0;

	/* stop any pending auto-repeat */
	if (k->k_repeating) {
		k->k_repeating = 0;
		callout_stop(&k->k_repeat_ch);
	}

	return (ret);
}



/****************************************************************
 *  Console input - called by middle layer at spltty().
 ****************************************************************/

static void
kbd_input_console(k, code)
	struct kbd_softc *k;
	int code;
{
	struct kbd_state *ks= &k->k_state;
	int keysym;

	/* any input stops auto-repeat (i.e. key release) */
	if (k->k_repeating) {
		k->k_repeating = 0;
		callout_stop(&k->k_repeat_ch);
	}

	keysym = kbd_code_to_keysym(ks, code);

	/* pass to console */
	if (kbd_input_keysym(k, keysym)) {
		log(LOG_WARNING, "%s: code=0x%x with mod=0x%x"
		    " produced unexpected keysym 0x%x\n",
		    k->k_dev.dv_xname,
		    code, ks->kbd_modbits, keysym);
		return;		/* no point in auto-repeat here */
	}

	if (KEYSYM_NOREPEAT(keysym))
		return;

	/* setup for auto-repeat after initial delay */
	k->k_repeating = 1;
	k->k_repeatsym = keysym;
	callout_reset(&k->k_repeat_ch, k->k_repeat_start,
		      kbd_repeat, k);
}


/*
 * This is the autorepeat callout function scheduled by kbd_input() above.
 * Called at splsoftclock().
 */
static void
kbd_repeat(arg)
	void *arg;
{
	struct kbd_softc *k = (struct kbd_softc *)arg;
	int s;

	s = spltty();
	if (k->k_repeating && k->k_repeatsym >= 0) {
		/* feed typematic keysym to the console */
		(void)kbd_input_keysym(k, k->k_repeatsym);

		/* reschedule next repeat */
		callout_reset(&k->k_repeat_ch, k->k_repeat_step,
			      kbd_repeat, k);
	}
	splx(s);
}



/*
 * Supply keysym as console input.  Convert keysym to character(s) and
 * pass them up to cons_channel's upstream hook.
 *
 * Return zero on success, else the keysym that we could not handle
 * (so that the caller may complain).
 */
static int
kbd_input_keysym(k, keysym)
	struct kbd_softc *k;
	int keysym;
{
	struct kbd_state *ks = &k->k_state;
	int data;

	/* Check if a recipient has been configured */
	if (k->k_cc == NULL || k->k_cc->cc_upstream == NULL)
		return (0);

	switch (KEYSYM_CLASS(keysym)) {

	case KEYSYM_ASCII:
		data = KEYSYM_DATA(keysym);
		if (ks->kbd_modbits & KBMOD_META_MASK)
			data |= 0x80;
		(*k->k_cc->cc_upstream)(data);
		break;

	case KEYSYM_STRING:
		data = keysym & 0xF;
		kbd_input_string(k, kbd_stringtab[data]);
		break;

	case KEYSYM_FUNC:
		kbd_input_funckey(k, keysym);
		break;

	case KEYSYM_CLRMOD:
		data = 1 << (keysym & 0x1F);
		ks->kbd_modbits &= ~data;
		break;

	case KEYSYM_SETMOD:
		data = 1 << (keysym & 0x1F);
		ks->kbd_modbits |= data;
		break;

	case KEYSYM_INVMOD:
		data = 1 << (keysym & 0x1F);
		ks->kbd_modbits ^= data;
		kbd_update_leds(k);
		break;

	case KEYSYM_ALL_UP:
		ks->kbd_modbits &= ~0xFFFF;
		break;

	case KEYSYM_SPECIAL:
		if (keysym == KEYSYM_NOP)
			break;
		/* FALLTHROUGH */
	default:
		/* We could not handle it. */
		return (keysym);
	}

	return (0);
}


/*
 * Send string upstream.
 */
static void
kbd_input_string(k, str)
	struct kbd_softc *k;
	char *str;
{

	while (*str) {
		(*k->k_cc->cc_upstream)(*str);
		++str;
	}
}


/*
 * Format the F-key sequence and send as a string.
 * XXX: Ugly compatibility mappings.
 */
static void
kbd_input_funckey(k, keysym)
	struct kbd_softc *k;
	int keysym;
{
	int n;
	char str[12];

	n = 0xC0 + (keysym & 0x3F);
	snprintf(str, sizeof(str), "\033[%dz", n);
	kbd_input_string(k, str);
}


/*
 * Update LEDs to reflect console input state.
 */
static void
kbd_update_leds(k)
	struct kbd_softc *k;
{
	struct kbd_state *ks = &k->k_state;
	char leds;

	leds = ks->kbd_leds;
	leds &= ~(LED_CAPS_LOCK|LED_NUM_LOCK);

	if (ks->kbd_modbits & (1 << KBMOD_CAPSLOCK))
		leds |= LED_CAPS_LOCK;
	if (ks->kbd_modbits & (1 << KBMOD_NUMLOCK))
		leds |= LED_NUM_LOCK;

	if (k->k_ops != NULL && k->k_ops->setleds != NULL)
		(void)(*k->k_ops->setleds)(k, leds, 0);
}



/****************************************************************
 *  Events input - called by middle layer at spltty().
 ****************************************************************/

/*
 * Supply raw keystrokes when keyboard is open in firm event mode.
 *
 * Turn the keystroke into an event and put it in the queue.
 * If the queue is full, the keystroke is lost (sorry!).
 */
static void
kbd_input_event(k, code)
	struct kbd_softc *k;
	int code;
{
	struct firm_event *fe;
	int put;

#ifdef DIAGNOSTIC
	if (!k->k_evmode) {
		printf("%s: kbd_input_event called when not in event mode\n",
		       k->k_dev.dv_xname);
		return;
	}
#endif
	put = k->k_events.ev_put;
	fe = &k->k_events.ev_q[put];
	put = (put + 1) % EV_QSIZE;
	if (put == k->k_events.ev_get) {
		log(LOG_WARNING, "%s: event queue overflow\n",
		    k->k_dev.dv_xname);
		return;
	}

	fe->id = KEY_CODE(code);
	fe->value = KEY_UP(code) ? VKEY_UP : VKEY_DOWN;
	fe->time = time;
	k->k_events.ev_put = put;
	EV_WAKEUP(&k->k_events);
}



/****************************************************************
 *  Translation stuff declared in kbd_xlate.h
 ****************************************************************/

/*
 * Initialization - called by either lower layer attach or by kdcninit.
 */
void
kbd_xlate_init(ks)
	struct kbd_state *ks;
{
	struct keyboard *ktbls;
	int id;

	id = ks->kbd_id;
	if (id < KBD_MIN_TYPE)
		id = KBD_MIN_TYPE;
	if (id > kbd_max_type)
		id = kbd_max_type;
	ktbls = keyboards[id];

	ks->kbd_k = *ktbls; 	/* struct assignment */
	ks->kbd_modbits = 0;
}

/*
 * Turn keyboard up/down codes into a KEYSYM.
 * Note that the "kd" driver (on sun3 and sparc64) uses this too!
 */
int
kbd_code_to_keysym(ks, c)
	struct kbd_state *ks;
	int c;
{
	u_short *km;
	int keysym;

	/*
	 * Get keymap pointer.  One of these:
	 * release, control, shifted, normal, ...
	 */
	if (KEY_UP(c))
		km = ks->kbd_k.k_release;
	else if (ks->kbd_modbits & KBMOD_CTRL_MASK)
		km = ks->kbd_k.k_control;
	else if (ks->kbd_modbits & KBMOD_SHIFT_MASK)
		km = ks->kbd_k.k_shifted;
	else
		km = ks->kbd_k.k_normal;

	if (km == NULL) {
		/*
		 * Do not know how to translate yet.
		 * We will find out when a RESET comes along.
		 */
		return (KEYSYM_NOP);
	}
	keysym = km[KEY_CODE(c)];

	/*
	 * Post-processing for Caps-lock
	 */
	if ((ks->kbd_modbits & (1 << KBMOD_CAPSLOCK)) &&
		(KEYSYM_CLASS(keysym) == KEYSYM_ASCII) )
	{
		if (('a' <= keysym) && (keysym <= 'z'))
			keysym -= ('a' - 'A');
	}

	/*
	 * Post-processing for Num-lock.  All "function"
	 * keysyms get indirected through another table.
	 * (XXX: Only if numlock on.  Want off also!)
	 */
	if ((ks->kbd_modbits & (1 << KBMOD_NUMLOCK)) &&
		(KEYSYM_CLASS(keysym) == KEYSYM_FUNC) )
	{
		keysym = kbd_numlock_map[keysym & 0x3F];
	}

	return (keysym);
}


/*
 * Back door for rcons (fb.c)
 */
void
kbd_bell(on)
	int on;
{
	struct kbd_softc *k = kbd_cd.cd_devs[0]; /* XXX: hardcoded minor */

	if (k == NULL || k->k_ops == NULL || k->k_ops->docmd == NULL)
		return;

	(void)(*k->k_ops->docmd)(k, on ? KBD_CMD_BELL : KBD_CMD_NOBELL, 0);
}

#if NWSKBD > 0
static void
kbd_input_wskbd(struct kbd_softc *k, int code)
{
	int type, key;

	type = KEY_UP(code) ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN;
	key = KEY_CODE(code);
	wskbd_input(k->k_wskbd, type, key);
}

int
wssunkbd_enable(v, on)
	void *v;
	int on;
{
	struct kbd_softc *k = v;

	k->k_wsenabled = on;
	if (on) {
		/* open actual underlying device */
		if (k->k_ops != NULL && k->k_ops->open != NULL)
			(*k->k_ops->open)(k);
		ev_init(&k->k_events);
		k->k_evmode = 0;	/* XXX: OK? */
	} else {
		/* close underlying device */
		if (k->k_ops != NULL && k->k_ops->close != NULL)
			(*k->k_ops->close)(k);
	}
	
	return 0;
}

void
wssunkbd_set_leds(v, leds)
	void *v;
	int leds;
{
	struct kbd_softc *k = v;
	int l = 0;

	if (leds & WSKBD_LED_CAPS)
		l |= LED_CAPS_LOCK;
	if (leds & WSKBD_LED_NUM)
		l |= LED_NUM_LOCK;
	if (leds & WSKBD_LED_SCROLL)
		l |= LED_SCROLL_LOCK;
	if (leds & WSKBD_LED_COMPOSE)
		l |= LED_COMPOSE;
	if (k->k_ops != NULL && k->k_ops->setleds != NULL)
		(*k->k_ops->setleds)(k, l, 0);
}

int
wssunkbd_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	return EPASSTHROUGH;
}

void
sunkbd_wskbd_cngetc(v, type, data)
	void *v;
	u_int *type;
	int *data;
{
	/* struct kbd_sun_softc *k = v; */
}

void
sunkbd_wskbd_cnpollc(v, on)
	void *v;
	int on;
{
}

static void
sunkbd_bell_off(v)
	void *v;
{
	struct kbd_softc *k = v;
	k->k_ops->docmd(k, KBD_CMD_NOBELL, 0);
}

void
sunkbd_wskbd_cnbell(v, pitch, period, volume)
	void *v;
	u_int pitch, period, volume;
{
	struct kbd_softc *k = v;

	callout_reset(&k->k_wsbell, period*1000/hz, sunkbd_bell_off, v);
	k->k_ops->docmd(k, KBD_CMD_BELL, 0);
}

void
kbd_wskbd_attach(k, isconsole)
	struct kbd_softc *k;
	int isconsole;
{
	struct wskbddev_attach_args a;

	a.console = isconsole;

	if (a.console)
		wskbd_cnattach(&sunkbd_wskbd_consops, k, &sunkbd_wskbd_keymapdata);

	a.keymap = &sunkbd_wskbd_keymapdata;

	a.accessops = &sunkbd_wskbd_accessops;
	a.accesscookie = k;

	/* Attach the wskbd */
	k->k_wskbd = config_found(&k->k_dev, &a, wskbddevprint);
	k->k_wsenabled = 0;
	callout_init(&k->k_wsbell);
}
#endif
