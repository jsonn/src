/*	$NetBSD: ms.c,v 1.3.10.1 1997/10/14 10:20:29 thorpej Exp $ */

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
 *	@(#)ms.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Mouse driver.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/tty.h>

#include <x68k/dev/event_var.h>
#include <machine/vuid_event.h>

#include <x68k/x68k/iodevice.h>

/*
 * Mouse state.  A SHARP X1/X680x0 mouse is a fairly simple device,
 * producing three-byte blobs of the form:
 *
 *	b dx dy
 *
 * where b is the button state, encoded as 0x80|(buttons)---there are
 * two buttons (2=left, 1=right)---and dx,dy are X and Y delta values.
 */
struct ms_softc {
	short	ms_byteno;		/* input byte number, for decode */
	char	ms_mb;			/* mouse button state */
	char	ms_ub;			/* user button state */
	int	ms_dx;			/* delta-x */
	int	ms_dy;			/* delta-y */
	struct	tty *ms_mouse;		/* downlink for output to mouse */
	void	(*ms_open) __P((struct tty *));	/* enable dataflow */
	void	(*ms_close) __P((struct tty *));/* disable dataflow */
	volatile int ms_ready;		/* event queue is ready */
	struct	evvar ms_events;	/* event queue state */
} ms_softc;

cdev_decl(ms);

void ms_serial __P((struct tty *, void(*)(struct tty *),
		    void(*)(struct tty *)));
void ms_modem __P((int));
void ms_rint __P((int));
void mouseattach __P((void));

/*
 * Attach the mouse serial (down-link) interface.
 * Do we need to set it to 4800 baud, 8 bits?
 * Test by power cycling and not booting Human68k before BSD?
 */
void
ms_serial(tp, iopen, iclose)
	struct tty *tp;
	void (*iopen) __P((struct tty *));
	void (*iclose) __P((struct tty *));
{

	ms_softc.ms_mouse = tp;
	ms_softc.ms_open = iopen;
	ms_softc.ms_close = iclose;
}

void
ms_modem(onoff)
	register int onoff;
{
	static int oonoff;

	if (ms_softc.ms_ready == 1) {
		if (ms_softc.ms_byteno == -1)
			ms_softc.ms_byteno = onoff = 0;
		if (oonoff != onoff) {
			zs_msmodem(onoff);
			oonoff = onoff;
		}
	}
}

void
ms_rint(c)
	register int c;
{
	register struct firm_event *fe;
	register struct ms_softc *ms = &ms_softc;
	register int mb, ub, d, get, put, any;
	static const char to_one[] = { 1, 2, 3 };
	static const int to_id[] = { MS_LEFT, MS_RIGHT, MS_MIDDLE };

	/*
	 * Discard input if not ready.  Drop sync on parity or framing
	 * error; gain sync on button byte.
	 */
	if (ms->ms_ready == 0)
		return;
	if (c & (TTY_FE|TTY_PE)) {
		log(LOG_WARNING,
		    "mouse input parity or framing error (0x%x)\n", c);
		ms->ms_byteno = -1;
		return;
	}

	/*
	 * Run the decode loop, adding to the current information.
	 * We add, rather than replace, deltas, so that if the event queue
	 * fills, we accumulate data for when it opens up again.
	 */
	switch (ms->ms_byteno) {

	case -1:
		return;

	case 0:
		/* buttons */
		ms->ms_byteno = 1;
		ms->ms_mb = c & 0x7;
		return;

	case 1:
		/* delta-x */
		ms->ms_byteno = 2;
		ms->ms_dx += (char)c;
		return;

	case 2:
		/* delta-y */
		ms->ms_byteno = -1	/* wait for button-byte again */;
		ms->ms_dy += (char)c;
		break;

	default:
		panic("ms_rint");
		/* NOTREACHED */
	}

	/*
	 * We have at least one event (mouse button, delta-X, or
	 * delta-Y; possibly all three, and possibly three separate
	 * button events).  Deliver these events until we are out
	 * of changes or out of room.  As events get delivered,
	 * mark them `unchanged'.
	 */
	any = 0;
	get = ms->ms_events.ev_get;
	put = ms->ms_events.ev_put;
	fe = &ms->ms_events.ev_q[put];

	/* NEXT prepares to put the next event, backing off if necessary */
#define	NEXT \
	if ((++put) % EV_QSIZE == get) { \
		put--; \
		goto out; \
	}
	/* ADVANCE completes the `put' of the event */
#define	ADVANCE \
	fe++; \
	if (put >= EV_QSIZE) { \
		put = 0; \
		fe = &ms->ms_events.ev_q[0]; \
	} \
	any = 1

	mb = ms->ms_mb;
	ub = ms->ms_ub;
	while ((d = mb ^ ub) != 0) {
		/*
		 * Mouse button change.  Convert up to three changes
		 * to the `first' change, and drop it into the event queue.
		 */
		NEXT;
		d = to_one[d - 1];		/* from 1..7 to {1,2,4} */
		fe->id = to_id[d - 1];		/* from {1,2,4} to ID */
		fe->value = mb & d ? VKEY_DOWN : VKEY_UP;
		fe->time = time;
		ADVANCE;
		ub ^= d;
	}
	if (ms->ms_dx) {
		NEXT;
		fe->id = LOC_X_DELTA;
		fe->value = ms->ms_dx;
		fe->time = time;
		ADVANCE;
		ms->ms_dx = 0;
	}
	if (ms->ms_dy) {
		NEXT;
		fe->id = LOC_Y_DELTA;
		fe->value = -ms->ms_dy; /* XXX? */
		fe->time = time;
		ADVANCE;
		ms->ms_dy = 0;
	}
out:
	if (any) {
		ms->ms_ub = ub;
		ms->ms_events.ev_put = put;
		EV_WAKEUP(&ms->ms_events);
	}
}

int
msopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	if (ms_softc.ms_events.ev_io)
		return (EBUSY);
	ms_softc.ms_events.ev_io = p;
	ev_init(&ms_softc.ms_events);	/* may cause sleep */
	ms_softc.ms_ready = 1;		/* start accepting events */
	(*ms_softc.ms_open)(ms_softc.ms_mouse);
	return (0);
}
int
msclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{

	ms_modem(0);
	ms_softc.ms_ready = 0;		/* stop accepting events */
	ev_fini(&ms_softc.ms_events);
	(*ms_softc.ms_close)(ms_softc.ms_mouse);
	ms_softc.ms_events.ev_io = NULL;
	return (0);
}

int
msread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (ev_read(&ms_softc.ms_events, uio, flags));
}

/* this routine should not exist, but is convenient to write here for now */
int
mswrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (EOPNOTSUPP);
}

int
msioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	register caddr_t data;
	int flag;
	struct proc *p;
{
	switch (cmd) {

	case FIONBIO:		/* we will remove this someday (soon???) */
		return (0);

	case FIOASYNC:
		ms_softc.ms_events.ev_async = *(int *)data != 0;
		return (0);

	case TIOCSPGRP:
		if (*(int *)data != ms_softc.ms_events.ev_io->p_pgid)
			return (EPERM);
		return (0);

	case VUIDGFORMAT:
		/* we only do firm_events */
		*(int *)data = VUID_FIRM_EVENT;
		return (0);

	case VUIDSFORMAT:
		if (*(int *)data != VUID_FIRM_EVENT)
			return (EINVAL);
		return (0);
	}
	return (ENOTTY);
}

int
mspoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{

	return (ev_poll(&ms_softc.ms_events, events, p));
}

void
mouseattach(){} /* XXX pseudo-device */
