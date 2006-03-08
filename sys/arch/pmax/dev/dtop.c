/*	$NetBSD: dtop.c,v 1.71.10.1 2006/03/08 00:43:07 elad Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	@(#)dtop.c	8.2 (Berkeley) 11/30/93
 */

/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *
 *	Hardware-level operations for the Desktop serial line
 *	bus (i2c aka ACCESS).
 */
/************************************************************
Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts,
and the Massachusetts Institute of Technology, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the names of Digital or MIT not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

********************************************************/

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: dtop.c,v 1.71.10.1 2006/03/08 00:43:07 elad Exp $");

#include "opt_ddb.h"
#include "rasterconsole.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/cons.h>
#include <dev/dec/lk201.h>
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicreg.h>
#include <dev/tc/ioasicvar.h>

#include <machine/dc7085cons.h>		/*  mdmctl bits same on dtop and dc? */

#include <machine/pmioctl.h>
#include <dev/sun/fbio.h>
#include <machine/fbvar.h>

#include <pmax/dev/dtopreg.h>
#include <pmax/dev/dtopvar.h>
#include <pmax/dev/fbreg.h>
#include <pmax/dev/lk201var.h>
#include <pmax/dev/rconsvar.h>

#include <pmax/pmax/maxine.h>


#define	DTOP_MAX_POLL	0x70000		/* about half a sec */

typedef volatile unsigned int	*data_reg_t;	/* uC  */
#define	DTOP_GET_BYTE(data)	(((*(data)) >> 8) & 0xff)
#define	DTOP_PUT_BYTE(data,c)	{ *(data) = (c) << 8; }

typedef volatile unsigned int	*poll_reg_t;	/* SIR */
#define	DTOP_RX_AVAIL(poll)	(*(poll) & 1)
#define	DTOP_TX_AVAIL(poll)	(*(poll) & 2)

#define	GET_SHORT(b0,b1)	(((b0)<<8)|(b1))

/*
 * Driver status
 */
struct dtop_softc {
	struct device	sc_dv;
	struct tty	*dtop_tty;
	data_reg_t	data;
	poll_reg_t	poll;
	char		polling_mode;
	char		probed_once;
	short		bad_pkts;

	struct dtop_ds {
		int		(*handler) __P((dtop_device_t, dtop_message_t,
				    int, int));
		dtop_device	status;
	} device[(DTOP_ADDR_DEFAULT - DTOP_ADDR_FIRST) >> 1];

#	define	DTOP_DEVICE_NO(address)	(((address)-DTOP_ADDR_FIRST)>>1)

};

#define DTOP_TTY(unit) \
	( ((struct dtop_softc*) dtop_cd.cd_devs[(unit)]) -> dtop_tty)
typedef struct dtop_softc *dtop_softc_t;

/*
 * Forward/prototyped declarations
 */
static int	dtop_get_packet __P((dtop_softc_t dtop, dtop_message_t pkt));
static int	dtop_escape __P((int c));
static void	dtop_keyboard_repeat __P((void *));
static int	dtop_null_device_handler __P((dtop_device_t, dtop_message_t, int, int));
static int	dtop_locator_handler __P((dtop_device_t, dtop_message_t, int, int));
static int	dtop_keyboard_handler __P((dtop_device_t, dtop_message_t, int, int));
static int	dtopparam __P((struct tty *, struct termios *));
static void	dtopstart __P((struct tty *));

/*
 * lk201 keyboard divisions and up/down mode key bitmap.
 */
#define NUMDIVS 14
/*
 * NOTE:  the right shift key was included in division 8 with the up/down
 *        arrow keys for some odd reason.  This causes a problem for
 *        the X11R5 server code, as it controls the key modes by divisions.
 *        The fix is to adjust the division table by changing the start
 *        of division 6 (which includes the shift and control keys) to
 *        include the right shift [0xab] and changing the end of division
 *        8 to be the up arrow [0xaa].  The initial key mode is also
 *        modified to include the right shift key as up/down.
 */
static u_char divbeg[NUMDIVS] = {0xbf, 0x91, 0xbc, 0xbd, 0xb0, 0xab, 0xa6,
				 0xa9, 0x88, 0x56, 0x63, 0x6f, 0x7b, 0x7e};
static u_char divend[NUMDIVS] = {0xff, 0xa5, 0xbc, 0xbe, 0xb2, 0xaf, 0xa8,
				 0xaa, 0x90, 0x62, 0x6e, 0x7a, 0x7d, 0x87};
/*
 * Initial defaults, groups 5 and 6 are up/down
 */
static u_long keymodes[8] = {0, 0, 0, 0, 0, 0x0003e800, 0, 0};

struct consdev dtopcons = { 
	NULL, NULL, (void *)dtopKBDGetc, NULL, NULL, NULL, NULL, NULL,
	NODEV, 0
};
 
void dtikbd_cnattach __P((void));		/* XXX */

void  
dtikbd_cnattach()
{
	cn_tab = &dtopcons;
	cn_tab->cn_pri = CN_NORMAL;
	rcons_indev(cn_tab); /* cn_dev & cn_putc */
}

/*
 * Autoconfiguration data for config.new.
 * Use the statically-allocated softc until old autoconfig code and
 * config.old are completely gone.
 *
 */
static int	dtopmatch __P((struct device * parent, struct cfdata *match, void *aux));
static void	dtopattach __P((struct device *parent, struct device *self, void *aux));
static int	dtopintr __P((void *sc));

CFATTACH_DECL(dtop, sizeof(struct dtop_softc),
    dtopmatch, dtopattach, NULL, NULL);

extern struct cfdriver dtop_cd;

dev_type_open(dtopopen);
dev_type_close(dtopclose);
dev_type_read(dtopread);
dev_type_write(dtopwrite);
dev_type_ioctl(dtopioctl);
dev_type_stop(dtopstop);
dev_type_tty(dtoptty);
dev_type_poll(dtoppoll);

const struct cdevsw dtop_cdevsw = {
	dtopopen, dtopclose, dtopread, dtopwrite, dtopioctl,
	dtopstop, dtoptty, dtoppoll, nommap, ttykqfilter, D_TTY
};

/* QVSS-compatible in-kernel X input event parser, pointer tracker */
void	(*dtopDivertXInput) __P((int));
void	(*dtopMouseEvent) __P((void *));
void	(*dtopMouseButtons) __P((void *));


/*
 * Match driver based on name
 */
static int
dtopmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct ioasicdev_attach_args *d = aux;

	if (badaddr((caddr_t)(d->iada_addr), 2))
		return (0);

	if (strcmp(d->iada_modname, "dtop") != 0)
		return (0);

	return (1);
}

static void
dtopattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ioasicdev_attach_args *d = aux;
	struct dtop_softc *sc = (struct dtop_softc*) self;
	int i;

	sc->poll = (poll_reg_t)MIPS_PHYS_TO_KSEG1(XINE_REG_INTR);
	sc->data = (data_reg_t)d->iada_addr;

	for (i = 0; i < DTOP_MAX_DEVICES; i++) {
		sc->device[i].handler = dtop_null_device_handler;
		callout_init(&sc->device[i].status.keyboard.repeat_ch);
	}

	/* a lot more needed here, fornow: */
	sc->device[DTOP_DEVICE_NO(0x6a)].handler = dtop_locator_handler;
	sc->device[DTOP_DEVICE_NO(0x6c)].handler = dtop_keyboard_handler;
	sc->device[DTOP_DEVICE_NO(0x6c)].status.keyboard.k_ar_state =
		K_AR_IDLE;

	sc->probed_once = 1;

	/* tie pseudo-slot to device */
	ioasic_intr_establish(parent, d->iada_cookie, TC_IPL_NONE, dtopintr, sc);
	printf("\n");
}


int
dtopopen(dev, flag, mode, l)
	dev_t dev;
	int flag, mode;
	struct lwp *l;
{
	struct tty *tp;
	int unit;
	int s, error = 0;
	int firstopen = 0;

	unit = minor(dev);
	if (unit >= dtop_cd.cd_ndevs)
		return (ENXIO);
	tp = DTOP_TTY(unit);
	if (tp == NULL) {
		tp = DTOP_TTY(unit) = ttymalloc();
		tty_attach(tp);
	}
	tp->t_oproc = dtopstart;
	tp->t_param = dtopparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0 && tp->t_wopen == 0) {
		ttychars(tp);
		firstopen = 1;
		if (tp->t_ispeed == 0) {
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		}
		(void) dtopparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if ((tp->t_state & TS_XCLUDE)
	    && generic_authorize(l->l_proc->p_cred, KAUTH_GENERIC_ISSUSER, &l->l_proc->p_acflag) != 0)
		return (EBUSY);
	s = spltty();
	while (!(flag & O_NONBLOCK) && !(tp->t_cflag & CLOCAL) &&
	       !(tp->t_state & TS_CARR_ON)) {
		tp->t_wopen++;
		error = ttysleep(tp, &tp->t_rawq, TTIPRI | PCATCH, ttopen, 0);
		tp->t_wopen--;
		if (error != 0)
			break;
	}
	splx(s);
	if (error)
		return (error);
	error = (*tp->t_linesw->l_open)(dev, tp);

#if (RASTERCONSOLE > 0) && defined(RCONS_BRAINDAMAGE)
	/* handle raster console specially */
	if (tp == DTOP_TTY(0) && firstopen) {
		tp->t_winsize = fbconstty->t_winsize;
	}
#endif /* HAVE_RCONS */

	return (error);
}

/*ARGSUSED*/
int
dtopclose(dev, flag, mode, l)
	dev_t dev;
	int flag, mode;
	struct lwp *l;
{
	struct tty *tp;
	int unit;

	unit = minor(dev);
	tp = DTOP_TTY(unit);
	(*tp->t_linesw->l_close)(tp, flag);
	return (ttyclose(tp));
}

int
dtopread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	struct tty *tp;

	tp = DTOP_TTY(minor(dev));
	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}

int
dtopwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	struct tty *tp;

	tp = DTOP_TTY(minor(dev));
	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}

int
dtoppoll(dev, events, l)
	dev_t dev;
	int events;
	struct lwp *l;
{
	struct tty *tp;

	tp = DTOP_TTY(minor(dev));
	return ((*tp->t_linesw->l_poll)(tp, events, l));
}

struct tty *
dtoptty(dev)
        dev_t dev;
{
        struct tty *tp = DTOP_TTY(minor(dev));
        return (tp);
}

/*ARGSUSED*/
int
dtopioctl(dev, cmd, data, flag, l)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct lwp *l;
{
	struct tty *tp;
	int unit = minor(dev);
	int error;

	tp = DTOP_TTY(unit);

	error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return (error);

	error = ttioctl(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return (error);

	switch (cmd) {

	case TIOCSBRK:
		ttyoutput(0, tp);
		break;

	case TIOCCBRK:
		ttyoutput(0, tp);
		break;

	case TIOCMGET:
		*(int *)data = DML_DTR | DML_DSR | DML_CAR;
		break;

	default:
		return (EPASSTHROUGH);
	}
	return (0);
}

/*
 * Interrupt routine
 */
static int
dtopintr(sc)
	void *sc;
{
	dtop_message msg;
	int devno;
	dtop_softc_t dtop;
	int s;

	dtop = sc;
	s = spltty();

	if (dtop_get_packet(dtop, &msg) < 0) {
#if defined(DIAGNOSTIC) || defined (DEBUG) || defined(RCONS_DEBUG)
	    printf("dtop: overrun (or stray)\n");
#endif
	    /*
	     * Ugh! The most common occurrence of a data overrun is upon a
	     * key press and the result is a software generated "stuck key".
	     * All I can think to do is fake an "all keys up" whenever a
	     * data overrun occurs.
	     */
	    msg.src_address = 0x6c;
	    msg.code.val.len = 1;
	    msg.body[0] = DTOP_KBD_EMPTY;
	}

	/*
	 * If not probed yet, just throw the data away.
	 */
	if (!dtop->probed_once)
		goto out;

	devno = DTOP_DEVICE_NO(msg.src_address);
	if (devno < 0 || devno > 15)
		goto out;

	(void) (*dtop->device[devno].handler)
			(&dtop->device[devno].status, &msg,
			 DTOP_EVENT_RECEIVE_PACKET, 0);
out:
	splx(s);
	return(0);
}

static void
dtopstart(tp)
	struct tty *tp;
{
	int s;

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))
		goto out;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup(&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	if (tp->t_outq.c_cc == 0)
		goto out;
#ifdef RCONS_BRAINDAMAGE
	/* handle console specially */
	if (tp == DTOP_TTY(0)) {
		int cc;

		while (tp->t_outq.c_cc > 0) {
			cc = getc(&tp->t_outq) & 0x7f;
			cnputc(cc);
		}
		/*
		 * After we flush the output queue we may need to wake
		 * up the process that made the output.
		 */
		if (tp->t_outq.c_cc <= tp->t_lowat) {
			if (tp->t_state & TS_ASLEEP) {
				tp->t_state &= ~TS_ASLEEP;
				wakeup(&tp->t_outq);
			}
			selwakeup(&tp->t_wsel);
		}
	}
#endif	/* RCONS_BRAINDAMAGE */

out:
	splx(s);
}

void
dtopKBDPutc(dev, c)
	dev_t dev;
	int c;
{
	int i;
	static int param = 0, cmd, mod, typ;
	static u_char parms[2];

	/*
	 * Emulate the lk201 command codes.
	 */
	if (param == 0) {
		typ = (c & 0x1);
		cmd = ((c >> 3) & 0xf);
		mod = ((c >> 1) & 0x3);
	} else
		parms[param - 1] = (c & 0x7f);
	if (c & 0x80) {
		if (typ) {
			/*
			 * A peripheral command code. Someday this driver
			 * should know how to send commands to the lk501,
			 * but until then this is all essentially a no-op.
			 */
			;
		} else {
			/*
			 * Set modes. These have to be emulated in software.
			 */
			if (cmd > 0 && cmd < 15) {
				cmd--;
				if (mod & 0x2)
				   for (i = divbeg[cmd]; i <= divend[cmd]; i++)
					keymodes[i >> 5] |=
						(1 << (i & 0x1f));
				else
				   for (i = divbeg[cmd]; i <= divend[cmd]; i++)
					keymodes[i >> 5] &=
						~(1 << (i & 0x1f));
			}
		}
		param = 0;
	} else if (++param > 2)
		param = 2;
}

/*
 * Take a packet off dtop interface
 * A packet MUST be there, this is not checked for.
 */
#define	DTOP_ESC_CHAR		0xf8
static int
dtop_escape(c)
	int c;
{
	/* I donno much about this stuff.. */
	switch (c) {
	case 0xe8:	return (0xf8);
	case 0xe9:	return (0xf9);
	case 0xea:	return (0xfa);
	case 0xeb:	return (0xfb);
	default:	/* printf("{esc %x}", c); */
			return (c);
	}
}

static int
dtop_get_packet(dtop, pkt)
	dtop_softc_t	dtop;
	dtop_message_t	pkt;
{
	poll_reg_t	poll;
	data_reg_t	data;
	int		ctr, i, len;
	unsigned char	c;
	int state;
	int escaped;

	poll = dtop->poll;
	data = dtop->data;

	/*
	 * The interface does not handle us the first byte,
	 * which is our address and cannot ever be anything
	 * else but 0x50.  This is a good thing, it makes
	 * the average packet exactly one word long, too.
	 */
	state = 0;		/* input packet state */
	escaped = 0;		/* getting escaped code */
	len = 0;		/* packet data length */
	i = 0;			/* packet data index */
	while (1) {
		for (ctr = 0; (ctr < DTOP_MAX_POLL) && !DTOP_RX_AVAIL(poll);
		    ctr++)
			DELAY(1);
		if (ctr == DTOP_MAX_POLL) {
			++dtop->bad_pkts;
			return (-1);
		}
		c = DTOP_GET_BYTE(data);
		if (escaped) {
			c = dtop_escape(c);
			if (c == 'O') {
				++dtop->bad_pkts;
				return (-1);
			}
			escaped = 0;
		} else {
			c = DTOP_GET_BYTE(data);
			if (c == DTOP_ESC_CHAR) {
				escaped = 1;
				continue;
			}
		}
		if (state == 0) {
			pkt->src_address = c;
			state = 1;
			continue;
		}
		if (state == 1) {
			pkt->code.bits = c;
			state = 2;
			len = pkt->code.val.len + 1;
			continue;
		}
		pkt->body[i] = c;
		++i;
		if (i >= len)
			break;
	}
	return (len);
}

/*
 * Get a keyboard char for the console
 */
int
dtopKBDGetc(dev)
	dev_t dev;
{
	int c;
	dtop_softc_t dtop;
	int s;

	dtop = dtop_cd.cd_devs[0];
	s = spltty();
again:
	c = -1;

	/*
	 * Now check keyboard
	 */
	if (DTOP_RX_AVAIL(dtop->poll)) {

		dtop_message	msg;
		struct dtop_ds	*ds;

		if (dtop_get_packet(dtop, &msg) >= 0) {

		    ds = &dtop->device[DTOP_DEVICE_NO(msg.src_address)];
		    if (ds->handler == dtop_keyboard_handler) {

			c = dtop_keyboard_handler(
					&ds->status, &msg,
					DTOP_EVENT_RECEIVE_PACKET, -1);


			if (c > 0) {
				splx(s);
				return c;
			}

			c = -1;
		    }
		}
	}

	if (c == -1) {
		DELAY(100);
		goto again;
	}

	splx(s);
	return c;
}

static int
dtopparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	if (tp->t_ispeed == 0)
		ttymodem(tp, 0);
	else
		/* called too early to invoke ttymodem, sigh */
		tp->t_state |= TS_CARR_ON;
	return (0);
}

/*
 * Stop output on a line.
 */
/*ARGSUSED*/
void
dtopstop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY) {
		if (!(tp->t_state & TS_TTSTOP))
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}

/*
 * Default handler function
 */
static int
dtop_null_device_handler(dev, msg, event, outc)
	 dtop_device_t	dev;
	 dtop_message_t	msg;
	 int		event;
	 int		outc;
{
	/* See if the message was to the default address (powerup) */

	/* Uhmm, donno how to handle this. Drop it */
	if (event == DTOP_EVENT_RECEIVE_PACKET)
		dev->unknown_report = *msg;
	return 0;
}

/*
 * Handler for locator devices (mice)
 */
static int
dtop_locator_handler(dev, msg, event, outc)
	 dtop_device_t	dev;
	 dtop_message_t	msg;
	 int		event;
	 int		outc;
{
	unsigned short	buttons;
	short coord;
	int moved = 0;
	static MouseReport currentRep;
	MouseReport *mrp = &currentRep;

	if (dtopMouseButtons) {
		mrp->state = 0;
		/*
		 * Do the position first
		 */
		coord = GET_SHORT(msg->body[2], msg->body[3]);
		if (coord < 0) {
			coord = -coord;
			moved = 1;
		} else if (coord > 0) {
			mrp->state |= MOUSE_X_SIGN;
			moved = 1;
		}
		mrp->dx = (coord & 0x1f);
		coord = GET_SHORT(msg->body[4], msg->body[5]);
		if (coord < 0) {
			coord = -coord;
			moved = 1;
		} else if (coord > 0) {
			mrp->state |= MOUSE_Y_SIGN;
			moved = 1;
		}
		mrp->dy = (coord & 0x1f);

		/*
		 * Time for the buttons now
		 * Shuffle button bits around to serial mouse order.
		 */
		buttons = GET_SHORT(msg->body[0], msg->body[1]);
		mrp->state |= (((buttons >> 1) & 0x3) | ((buttons << 2) & 0x4));
		if (moved)
			(*dtopMouseEvent)(mrp);
		(*dtopMouseButtons)(mrp);
	}
	return (0);
}

/*
 * Handler for keyboard devices
 * Special case: outc set for recv packet means
 * we are inside the kernel debugger
 */
static int
dtop_keyboard_handler(dev, msg, event, outc)
	dtop_device_t dev;
	dtop_message_t msg;
	int event;
	int outc;
{
	u_char *ls, *le, *ns, *ne;
	u_char save[11], retc;
	int msg_len, c, s, cl;
	const char *cp;
#ifdef RCONS_BRAINDAMAGE
	struct tty *tp = DTOP_TTY(0);
#endif

	/*
	 * Fiddle about emulating an lk201 keyboard. The lk501
	 * designers carefully ensured that keyboard handlers could be
	 * stateless, then we turn around and use lots of state to
	 * emulate the stateful lk201, since the X11R5 X servers
	 * only know about the lk201... (oh well)
	 */
	/*
	 * Turn off any autorepeat timeout.
	 */
	s = splhigh();
	if (dev->keyboard.k_ar_state != K_AR_IDLE) {
		dev->keyboard.k_ar_state = K_AR_IDLE;
		callout_stop(&dev->keyboard.repeat_ch);
	}
	splx(s);
	msg_len = msg->code.val.len;

	/* Check for errors */
	c = msg->body[0];
	if ((c < DTOP_KBD_KEY_MIN) && (c != DTOP_KBD_EMPTY)) {
		printf("Keyboard error: %x %x %x..\n", msg_len, c, msg->body[1]);
#ifdef notdef
		if (c != DTOP_KBD_OUT_ERR) return -1;
#endif
		/*
		 * Fake an "all ups" to avoid the stuck key syndrome.
		 */
		c = msg->body[0] = DTOP_KBD_EMPTY;
		msg_len = 1;
	}

	dev->keyboard.last_msec = TO_MS(time);
	/*
	 * To make things readable, do a first pass cancelling out
	 * all keys that are still pressed, and a second one generating
	 * events.  While generating events, do the upstrokes first
	 * from oldest to youngest, then the downstrokes from oldest
	 * to youngest.  This copes with lost packets and provides
	 * a reasonable model even if scans are too slow.
	 */

	/* make a copy of new state first */
	if (msg_len == 1)
		save[0] = msg->body[0];
	else if (msg_len > 0)
		memcpy(save, msg->body, msg_len);

	/*
	 * Cancel out any keys in both the last and current message as
	 * they are unchanged.
	 */
	if (msg_len > 0 && dev->keyboard.last_codes_count > 0) {
		ls = dev->keyboard.last_codes;
		le = &dev->keyboard.last_codes[ ((u_int)dev->keyboard.
							last_codes_count) ];
		ne = &msg->body[msg_len];
		for (; ls < le; ls++) {
			for (ns = msg->body; ns < ne; ns++)
				if (*ls == *ns) {
					*ls = *ns = 0;
					break;
				}
		}
	}

	/*
	 * Now generate all upstrokes
	 */
	le = dev->keyboard.last_codes;
	ls = &dev->keyboard.last_codes[dev->keyboard.last_codes_count - 1];
	for ( ; ls >= le; ls--)
	    if ((c = *ls) != 0) {
		(void) lk_mapchar(c, &cl);

		if (outc == 0 && dtopDivertXInput &&
		    (keymodes[(c >> 5) & 0x7] & (1 << (c & 0x1f))))
			(*dtopDivertXInput)(c);
	    }
	/*
	 * And finally the downstrokes
	 */
	ne = (char*)msg->body;
	ns = (char*)&msg->body[msg_len - 1];
	retc = 0;
	for ( ; ns >= ne; ns--)
	    if (*ns) {
		cp = lk_mapchar(*ns, &cl);
#ifdef DDB
		if (*ns == LK_DO) {
			spl0();
			Debugger();
		}
#endif
		if (outc == 0) {
		    if (dtopDivertXInput) {
			(*dtopDivertXInput)(*ns);
			c = -1; /* consumed by X */
		    } else if (cp /*&& tp != NULL*/) {
			for (; cl; cl--, cp++) {
#if 0
				(*tp->t_linesw->l_rint)(*cp, tp);
#else
				rcons_input(0, *cp);
#endif
			}
		    }
		    dev->keyboard.k_ar_state = K_AR_ACTIVE;
		}
		/*
		 * return the related keycode anyways
		 * XXX when in debugger, don't return multi-char sequences
		 */
		if (cp && (cp[1] == '\0') &&(retc == 0))
		    retc = cp[0];
	    }
	outc = retc;
	/* install new scan state */
	if (msg_len == 1)
		dev->keyboard.last_codes[0] = save[0];
	else if (msg_len > 0)
		memcpy(dev->keyboard.last_codes, save, msg_len);
	dev->keyboard.last_codes_count = msg_len;
	if (dev->keyboard.k_ar_state == K_AR_ACTIVE)
		callout_reset(&dev->keyboard.repeat_ch, hz / 2,
		    dtop_keyboard_repeat, (void *)dev);
	return (outc);
}

/*
 * Do an autorepeat as required.
 */
static void
dtop_keyboard_repeat(arg)
	void *arg;
{
	dtop_device_t dev = (dtop_device_t)arg;
	int i, c, cl;
	const char *cp;
#if 0
	struct tty *tp = DTOP_TTY(0);
#endif
	int s = spltty(), gotone = 0;

	for (i = 0; i < dev->keyboard.last_codes_count; i++) {
		c = (int)dev->keyboard.last_codes[i];
		if (c != DTOP_KBD_EMPTY &&
		    (keymodes[(c >> 5) & 0x7] & (1 << (c & 0x1f))) == 0) {
			dev->keyboard.k_ar_state = K_AR_TRIGGER;
			if (dtopDivertXInput) {
				(*dtopDivertXInput)(KEY_REPEAT);
				gotone = 1;
				continue;
			}

			if ((cp = lk_mapchar(KEY_REPEAT, &cl)) != NULL) {
				for (; cl; cl--, cp++) {
#if 0
					(*tp->t_linesw->l_rint)(*cp, tp);
#else
					rcons_input(0, *cp);
#endif
				}
				gotone = 1;
			}
		}
	}
	if (gotone)
		callout_reset(&dev->keyboard.repeat_ch, hz / 20,
		    dtop_keyboard_repeat, dev);
	else
		dev->keyboard.k_ar_state = K_AR_IDLE;
	splx(s);
}
