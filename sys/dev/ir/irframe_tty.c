/*	$NetBSD: irframe_tty.c,v 1.19.2.5 2002/06/24 22:10:06 nathanw Exp $	*/

/*
 * TODO
 *  Test dongle code.
 */

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) and Tommy Bohlin
 * (tommy@gatespace.com).
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

/*
 * Loosely based on ppp_tty.c.
 * Framing and dongle handling written by Tommy Bohlin.
 */

#include <sys/param.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/ir/ir.h>
#include <dev/ir/sir.h>
#include <dev/ir/irdaio.h>
#include <dev/ir/irframevar.h>

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

#ifdef IRFRAMET_DEBUG
#define DPRINTF(x)	if (irframetdebug) printf x
#define Static
int irframetdebug = 0;
#else
#define DPRINTF(x)
#define Static static
#endif

/*****/

/* Max size with framing. */
#define MAX_IRDA_FRAME (2*IRDA_MAX_FRAME_SIZE + IRDA_MAX_EBOFS + 4)

struct frame {
	u_char *buf;
	u_int len;
};
#define MAXFRAMES 8

struct irframet_softc {
	struct irframe_softc sc_irp;
	struct tty *sc_tp;

	int sc_dongle;
	int sc_dongle_private;

	int sc_state;
#define	IRT_RSLP		0x01	/* waiting for data (read) */
#if 0
#define	IRT_WSLP		0x02	/* waiting for data (write) */
#define IRT_CLOSING		0x04	/* waiting for output to drain */
#endif
	struct lock sc_wr_lk;

	struct irda_params sc_params;

	u_char* sc_inbuf;
	int sc_framestate;
#define FRAME_OUTSIDE    0
#define FRAME_INSIDE     1
#define FRAME_ESCAPE     2
	int sc_inchars;
	int sc_inFCS;
	struct callout sc_timeout;

	u_int sc_nframes;
	u_int sc_framei;
	u_int sc_frameo;
	struct frame sc_frames[MAXFRAMES];
	struct selinfo sc_rsel;
};

/* line discipline methods */
int	irframetopen(dev_t dev, struct tty *tp);
int	irframetclose(struct tty *tp, int flag);
int	irframetioctl(struct tty *tp, u_long cmd, caddr_t data, int flag,
		      struct proc *);
int	irframetinput(int c, struct tty *tp);
int	irframetstart(struct tty *tp);

/* pseudo device init */
void	irframettyattach(int);

/* irframe methods */
Static int	irframet_open(void *h, int flag, int mode, struct proc *p);
Static int	irframet_close(void *h, int flag, int mode, struct proc *p);
Static int	irframet_read(void *h, struct uio *uio, int flag);
Static int	irframet_write(void *h, struct uio *uio, int flag);
Static int	irframet_poll(void *h, int events, struct proc *p);
Static int	irframet_set_params(void *h, struct irda_params *params);
Static int	irframet_get_speeds(void *h, int *speeds);
Static int	irframet_get_turnarounds(void *h, int *times);

/* internal */
Static int	irt_write_frame(struct tty *tp, u_int8_t *buf, size_t len);
Static int	irt_putc(struct tty *tp, int c);
Static void	irt_frame(struct irframet_softc *sc, u_char *buf, u_int len);
Static void	irt_timeout(void *v);
Static void	irt_ioctl(struct tty *tp, u_long cmd, void *arg);
Static void	irt_setspeed(struct tty *tp, u_int speed);
Static void	irt_setline(struct tty *tp, u_int line);
Static void	irt_delay(struct tty *tp, u_int delay);

Static const struct irframe_methods irframet_methods = {
	irframet_open, irframet_close, irframet_read, irframet_write,
	irframet_poll, irframet_set_params,
	irframet_get_speeds, irframet_get_turnarounds
};

Static void irts_none(struct tty *tp, u_int speed);
Static void irts_tekram(struct tty *tp, u_int speed);
Static void irts_jeteye(struct tty *tp, u_int speed);
Static void irts_actisys(struct tty *tp, u_int speed);
Static void irts_litelink(struct tty *tp, u_int speed);
Static void irts_girbil(struct tty *tp, u_int speed);

#define NORMAL_SPEEDS (IRDA_SPEEDS_SIR & ~IRDA_SPEED_2400)
#define TURNT_POS (IRDA_TURNT_10000 | IRDA_TURNT_5000 | IRDA_TURNT_1000 | \
	IRDA_TURNT_500 | IRDA_TURNT_100 | IRDA_TURNT_50 | IRDA_TURNT_10)
Static const struct dongle {
	void (*setspeed)(struct tty *tp, u_int speed);
	u_int speedmask;
	u_int turnmask;
} irt_dongles[DONGLE_MAX] = {
	/* Indexed by dongle number from irdaio.h */
	{ irts_none, IRDA_SPEEDS_SIR, IRDA_TURNT_10000 },
	{ irts_tekram, IRDA_SPEEDS_SIR, IRDA_TURNT_10000 },
	{ irts_jeteye, IRDA_SPEED_9600|IRDA_SPEED_19200|IRDA_SPEED_115200,
	  				IRDA_TURNT_10000 },
	{ irts_actisys, NORMAL_SPEEDS & ~IRDA_SPEED_38400, TURNT_POS },
	{ irts_actisys, NORMAL_SPEEDS, TURNT_POS },
	{ irts_litelink, NORMAL_SPEEDS, TURNT_POS },
	{ irts_girbil, IRDA_SPEEDS_SIR, IRDA_TURNT_10000 | IRDA_TURNT_5000 },
};

void
irframettyattach(int n)
{
}

/*
 * Line specific open routine for async tty devices.
 * Attach the given tty to the first available irframe unit.
 * Called from device open routine or ttioctl.
 */
/* ARGSUSED */
int
irframetopen(dev_t dev, struct tty *tp)
{
	struct proc *p = curproc;		/* XXX */
	struct irframet_softc *sc;
	int error, s;

	DPRINTF(("%s\n", __FUNCTION__));

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);

	s = spltty();

	DPRINTF(("%s: linesw=%p disc=%s\n", __FUNCTION__, tp->t_linesw,
		 tp->t_linesw->l_name));
	if (strcmp(tp->t_linesw->l_name, "irframe") == 0) { /* XXX */
		sc = (struct irframet_softc *)tp->t_sc;
		DPRINTF(("%s: sc=%p sc_tp=%p\n", __FUNCTION__, sc, sc->sc_tp));
		if (sc != NULL) {
			splx(s);
			return (EBUSY);
		}
	}

	tp->t_sc = irframe_alloc(sizeof (struct irframet_softc),
			&irframet_methods, tp);
	sc = (struct irframet_softc *)tp->t_sc;
	sc->sc_tp = tp;
	printf("%s attached at tty%02d\n", sc->sc_irp.sc_dev.dv_xname,
	    minor(tp->t_dev));

	DPRINTF(("%s: set sc=%p\n", __FUNCTION__, sc));

	ttyflush(tp, FREAD | FWRITE);

	sc->sc_dongle = DONGLE_NONE;
	sc->sc_dongle_private = 0;

	splx(s);

	return (0);
}

/*
 * Line specific close routine, called from device close routine
 * and from ttioctl.
 * Detach the tty from the irframe unit.
 * Mimics part of ttyclose().
 */
int
irframetclose(struct tty *tp, int flag)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int s;

	DPRINTF(("%s: tp=%p\n", __FUNCTION__, tp));

	s = spltty();
	ttyflush(tp, FREAD | FWRITE);
	tp->t_linesw = linesw[0]; /* default line discipline */
	if (sc != NULL) {
		tp->t_sc = NULL;
		printf("%s detached from tty%02d\n", sc->sc_irp.sc_dev.dv_xname,
		    minor(tp->t_dev));

		if (sc->sc_tp == tp)
			irframe_dealloc(&sc->sc_irp.sc_dev);
	}
	splx(s);
	return (0);
}

/*
 * Line specific (tty) ioctl routine.
 * This discipline requires that tty device drivers call
 * the line specific l_ioctl routine from their ioctl routines.
 */
/* ARGSUSED */
int
irframetioctl(struct tty *tp, u_long cmd, caddr_t data, int flag, 
	     struct proc *p)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int error;
	int d;

	DPRINTF(("%s: tp=%p\n", __FUNCTION__, tp));

	if (sc == NULL || tp != sc->sc_tp)
		return (EPASSTHROUGH);

	error = 0;
	switch (cmd) {
	case IRFRAMETTY_GET_DEVICE:
		*(int *)data = sc->sc_irp.sc_dev.dv_unit;
		break;
	case IRFRAMETTY_GET_DONGLE:
		*(int *)data = sc->sc_dongle;
		break;
	case IRFRAMETTY_SET_DONGLE:
		d = *(int *)data;
		if (d < 0 || d >= DONGLE_MAX)
			return (EINVAL);
		sc->sc_dongle = d;
		break;
	default:
		error = EPASSTHROUGH;
		break;
	}

	return (error);
}

/*
 * Start output on async tty interface.
 */
int
irframetstart(struct tty *tp)
{
	/*struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;*/
	int s;

	DPRINTF(("%s: tp=%p\n", __FUNCTION__, tp));

	s = spltty();
	if (tp->t_oproc != NULL)
		(*tp->t_oproc)(tp);
	splx(s);

	return (0);
}

void
irt_frame(struct irframet_softc *sc, u_char *buf, u_int len)
{
	DPRINTF(("%s: nframe=%d framei=%d frameo=%d\n",
		 __FUNCTION__, sc->sc_nframes, sc->sc_framei, sc->sc_frameo));

	if (sc->sc_inbuf == NULL) /* XXX happens if device is closed? */
		return;
	if (sc->sc_nframes >= MAXFRAMES) {
#ifdef IRFRAMET_DEBUG
		printf("%s: dropped frame\n", __FUNCTION__);
#endif
		return;
	}
	if (sc->sc_frames[sc->sc_framei].buf == NULL)
		return;
	memcpy(sc->sc_frames[sc->sc_framei].buf, buf, len);
	sc->sc_frames[sc->sc_framei].len = len;
	sc->sc_framei = (sc->sc_framei+1) % MAXFRAMES;
	sc->sc_nframes++;
	if (sc->sc_state & IRT_RSLP) {
		sc->sc_state &= ~IRT_RSLP;
		DPRINTF(("%s: waking up reader\n", __FUNCTION__));
		wakeup(sc->sc_frames);
	}
	selwakeup(&sc->sc_rsel);
}

void
irt_timeout(void *v)
{
	struct irframet_softc *sc = v;

#ifdef IRFRAMET_DEBUG
	if (sc->sc_framestate != FRAME_OUTSIDE)
		printf("%s: input frame timeout\n", __FUNCTION__);
#endif
	sc->sc_framestate = FRAME_OUTSIDE;
}

int
irframetinput(int c, struct tty *tp)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;

	c &= 0xff;

#if IRFRAMET_DEBUG
	if (irframetdebug > 1)
		DPRINTF(("%s: tp=%p c=0x%02x\n", __FUNCTION__, tp, c));
#endif

	if (sc == NULL || tp != (struct tty *)sc->sc_tp)
		return (0);

	if (sc->sc_inbuf == NULL)
		return (0);

	switch (c) {
	case SIR_BOF:
		DPRINTF(("%s: BOF\n", __FUNCTION__));
		sc->sc_framestate = FRAME_INSIDE;
		sc->sc_inchars = 0;
		sc->sc_inFCS = INITFCS;
		break;
	case SIR_EOF:
		DPRINTF(("%s: EOF state=%d inchars=%d fcs=0x%04x\n",
			 __FUNCTION__,
			 sc->sc_framestate, sc->sc_inchars, sc->sc_inFCS));
		if (sc->sc_framestate == FRAME_INSIDE &&
		    sc->sc_inchars >= 4 && sc->sc_inFCS == GOODFCS) {
			irt_frame(sc, sc->sc_inbuf, sc->sc_inchars - 2);
		} else if (sc->sc_framestate != FRAME_OUTSIDE) {
#ifdef IRFRAMET_DEBUG
			printf("%s: malformed input frame\n", __FUNCTION__);
#endif
		}
		sc->sc_framestate = FRAME_OUTSIDE;
		break;
	case SIR_CE:
		DPRINTF(("%s: CE\n", __FUNCTION__));
		if (sc->sc_framestate == FRAME_INSIDE)
			sc->sc_framestate = FRAME_ESCAPE;
		break;
	default:
#if IRFRAMET_DEBUG
	if (irframetdebug > 1)
		DPRINTF(("%s: c=0x%02x, inchar=%d state=%d\n", __FUNCTION__, c,
			 sc->sc_inchars, sc->sc_state));
#endif
		if (sc->sc_framestate != FRAME_OUTSIDE) {
			if (sc->sc_framestate == FRAME_ESCAPE) {
				sc->sc_framestate = FRAME_INSIDE;
				c ^= SIR_ESC_BIT;
			}
			if (sc->sc_inchars < sc->sc_params.maxsize + 2) {
				sc->sc_inbuf[sc->sc_inchars++] = c;
				sc->sc_inFCS = updateFCS(sc->sc_inFCS, c);
			} else {
				sc->sc_framestate = FRAME_OUTSIDE;
#ifdef IRFRAMET_DEBUG
				printf("%s: input frame overrun\n",
				       __FUNCTION__);
#endif
			}
		}
		break;
	}

#if 1
	if (sc->sc_framestate != FRAME_OUTSIDE) {
		callout_reset(&sc->sc_timeout, hz/20, irt_timeout, sc);
	}
#endif

	return (0);
}


/*** irframe methods ***/

int
irframet_open(void *h, int flag, int mode, struct proc *p)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;

	DPRINTF(("%s: tp=%p\n", __FUNCTION__, tp));

	sc->sc_params.speed = 0;
	sc->sc_params.ebofs = IRDA_DEFAULT_EBOFS;
	sc->sc_params.maxsize = 0;
	sc->sc_framestate = FRAME_OUTSIDE;
	sc->sc_nframes = 0;
	sc->sc_framei = 0;
	sc->sc_frameo = 0;
	callout_init(&sc->sc_timeout);
	lockinit(&sc->sc_wr_lk, PZERO, "irfrtl", 0, 0);

	return (0);
}

int
irframet_close(void *h, int flag, int mode, struct proc *p)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int i, s;

	DPRINTF(("%s: tp=%p\n", __FUNCTION__, tp));

	callout_stop(&sc->sc_timeout);
	s = splir();
	if (sc->sc_inbuf != NULL) {
		free(sc->sc_inbuf, M_DEVBUF);
		sc->sc_inbuf = NULL;
	}
	for (i = 0; i < MAXFRAMES; i++) {
		if (sc->sc_frames[i].buf != NULL) {
			free(sc->sc_frames[i].buf, M_DEVBUF);
			sc->sc_frames[i].buf = NULL;
		}
	}
	splx(s);

	return (0);
}

int
irframet_read(void *h, struct uio *uio, int flag)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int error = 0;
	int s;

	DPRINTF(("%s: resid=%d, iovcnt=%d, offset=%ld\n", 
		 __FUNCTION__, uio->uio_resid, uio->uio_iovcnt, 
		 (long)uio->uio_offset));
	DPRINTF(("%s: nframe=%d framei=%d frameo=%d\n",
		 __FUNCTION__, sc->sc_nframes, sc->sc_framei, sc->sc_frameo));


	s = splir();	
	while (sc->sc_nframes == 0) {
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		sc->sc_state |= IRT_RSLP;
		DPRINTF(("%s: sleep\n", __FUNCTION__));
		error = tsleep(sc->sc_frames, PZERO | PCATCH, "irtrd", 0);
		DPRINTF(("%s: woke, error=%d\n", __FUNCTION__, error));
		if (error) {
			sc->sc_state &= ~IRT_RSLP;
			break;
		}
	}

	/* Do just one frame transfer per read */
	if (!error) {
		if (uio->uio_resid < sc->sc_frames[sc->sc_frameo].len) {
			DPRINTF(("%s: uio buffer smaller than frame size "
				 "(%d < %d)\n", __FUNCTION__, uio->uio_resid,
				 sc->sc_frames[sc->sc_frameo].len));
			error = EINVAL;
		} else {
			DPRINTF(("%s: moving %d bytes\n", __FUNCTION__,
				 sc->sc_frames[sc->sc_frameo].len));
			error = uiomove(sc->sc_frames[sc->sc_frameo].buf,
					sc->sc_frames[sc->sc_frameo].len, uio);
			DPRINTF(("%s: error=%d\n", __FUNCTION__, error));
		}
		sc->sc_frameo = (sc->sc_frameo+1) % MAXFRAMES;
		sc->sc_nframes--;
	}
	splx(s);

	return (error);
}

int
irt_putc(struct tty *tp, int c)
{
	int s;
	int error;

#if IRFRAMET_DEBUG
	if (irframetdebug > 3)
		DPRINTF(("%s: tp=%p c=0x%02x cc=%d\n", __FUNCTION__, tp, c,
			 tp->t_outq.c_cc));
#endif
	if (tp->t_outq.c_cc > tp->t_hiwat) {
		irframetstart(tp);
		s = spltty();
		/*
		 * This can only occur if FLUSHO is set in t_lflag,
		 * or if ttstart/oproc is synchronous (or very fast).
		 */
		if (tp->t_outq.c_cc <= tp->t_hiwat) {
			splx(s);
			goto go;
		}
		SET(tp->t_state, TS_ASLEEP);
		error = ttysleep(tp, &tp->t_outq, TTOPRI | PCATCH, ttyout, 0);
		splx(s);
		if (error)
			return (error);
	}
 go:
	if (putc(c, &tp->t_outq) < 0) {
		printf("irframe: putc failed\n");
		return (EIO);
	}
	return (0);
}

int
irframet_write(void *h, struct uio *uio, int flag)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	u_int8_t buf[MAX_IRDA_FRAME];
	int n;

	DPRINTF(("%s: resid=%d, iovcnt=%d, offset=%ld\n", 
		 __FUNCTION__, uio->uio_resid, uio->uio_iovcnt, 
		 (long)uio->uio_offset));

	n = irda_sir_frame(buf, MAX_IRDA_FRAME, uio, sc->sc_params.ebofs);
	if (n < 0) {
#ifdef IRFRAMET_DEBUG
		printf("%s: irda_sir_frame() error=%d\n", __FUNCTION__, -n);
#endif
		return (-n);
	}
	return (irt_write_frame(tp, buf, n));
}

int
irt_write_frame(struct tty *tp, u_int8_t *buf, size_t len)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int error, i;

	DPRINTF(("%s: tp=%p len=%d\n", __FUNCTION__, tp, len));

	lockmgr(&sc->sc_wr_lk, LK_EXCLUSIVE, NULL);
	error = 0;
	for (i = 0; !error && i < len; i++)
		error = irt_putc(tp, buf[i]);
	lockmgr(&sc->sc_wr_lk, LK_RELEASE, NULL);

	irframetstart(tp);

	DPRINTF(("%s: done, error=%d\n", __FUNCTION__, error));

	return (error);
}

int
irframet_poll(void *h, int events, struct proc *p)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int revents = 0;
	int s;

	DPRINTF(("%s: sc=%p\n", __FUNCTION__, sc));

	s = splir();
	/* XXX is this a good check? */
	if (events & (POLLOUT | POLLWRNORM))
		if (tp->t_outq.c_cc <= tp->t_lowat)
			revents |= events & (POLLOUT | POLLWRNORM);

	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_nframes > 0) {
			DPRINTF(("%s: have data\n", __FUNCTION__));
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			DPRINTF(("%s: recording select\n", __FUNCTION__));
			selrecord(p, &sc->sc_rsel);
		}
	}
	splx(s);

	return (revents);
}

int
irframet_set_params(void *h, struct irda_params *p)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;	
	int i;

	DPRINTF(("%s: tp=%p speed=%d ebofs=%d maxsize=%d\n",
		 __FUNCTION__, tp, p->speed, p->ebofs, p->maxsize));

	if (p->speed != sc->sc_params.speed) {
		/* Checked in irframe.c */
		lockmgr(&sc->sc_wr_lk, LK_EXCLUSIVE, NULL);
		irt_dongles[sc->sc_dongle].setspeed(tp, p->speed);
		lockmgr(&sc->sc_wr_lk, LK_RELEASE, NULL);
		sc->sc_params.speed = p->speed;
	}

	/* Max size checked in irframe.c */
	sc->sc_params.ebofs = p->ebofs;
	/* Max size checked in irframe.c */
	if (sc->sc_params.maxsize != p->maxsize) {
		sc->sc_params.maxsize = p->maxsize;
		if (sc->sc_inbuf != NULL)
			free(sc->sc_inbuf, M_DEVBUF);
		for (i = 0; i < MAXFRAMES; i++)
			if (sc->sc_frames[i].buf != NULL)
				free(sc->sc_frames[i].buf, M_DEVBUF);
		if (sc->sc_params.maxsize != 0) {
			sc->sc_inbuf = malloc(sc->sc_params.maxsize+2,
					      M_DEVBUF, M_WAITOK);
			for (i = 0; i < MAXFRAMES; i++)
				sc->sc_frames[i].buf =
					malloc(sc->sc_params.maxsize,
					       M_DEVBUF, M_WAITOK);
		} else {
			sc->sc_inbuf = NULL;
			for (i = 0; i < MAXFRAMES; i++)
				sc->sc_frames[i].buf = NULL;
		}
	}
	sc->sc_framestate = FRAME_OUTSIDE;

	return (0);
}

int
irframet_get_speeds(void *h, int *speeds)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;	

	DPRINTF(("%s: tp=%p\n", __FUNCTION__, tp));

	if (sc == NULL)		/* during attach */
		*speeds = IRDA_SPEEDS_SIR;
	else
		*speeds = irt_dongles[sc->sc_dongle].speedmask;
	return (0);
}

int
irframet_get_turnarounds(void *h, int *turnarounds)
{
	struct tty *tp = h;
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;	

	DPRINTF(("%s: tp=%p\n", __FUNCTION__, tp));

	*turnarounds = irt_dongles[sc->sc_dongle].turnmask;
	return (0);
}

void
irt_ioctl(struct tty *tp, u_long cmd, void *arg)
{
	int error;
	dev_t dev;

	dev = tp->t_dev;
	error = cdevsw[major(dev)].d_ioctl(dev, cmd, arg, 0, curproc);
#ifdef DIAGNOSTIC
	if (error)
		printf("irt_ioctl: cmd=0x%08lx error=%d\n", cmd, error);
#endif
}

void
irt_setspeed(struct tty *tp, u_int speed)
{
	struct termios tt;

	irt_ioctl(tp, TIOCGETA,  &tt);
	tt.c_ispeed = tt.c_ospeed = speed;
	tt.c_cflag &= ~HUPCL;
	tt.c_cflag |= CLOCAL;
	irt_ioctl(tp, TIOCSETAF, &tt);
}

void
irt_setline(struct tty *tp, u_int line)
{
	int mline;

	irt_ioctl(tp, TIOCMGET, &mline);
	mline &= ~(TIOCM_DTR | TIOCM_RTS);
	mline |= line;
	irt_ioctl(tp, TIOCMSET, (caddr_t)&mline);
}

void
irt_delay(struct tty *tp, u_int ms)
{
	if (cold)
		delay(ms * 1000);
	else
		tsleep(&irt_delay, PZERO, "irtdly", ms * hz / 1000 + 1);
		
}

/**********************************************************************
 * No dongle
 **********************************************************************/
void
irts_none(struct tty *tp, u_int speed)
{
	irt_setspeed(tp, speed);
}

/**********************************************************************
 * Tekram
 **********************************************************************/
#define TEKRAM_PW     0x10

#define TEKRAM_115200 (TEKRAM_PW|0x00)
#define TEKRAM_57600  (TEKRAM_PW|0x01)
#define TEKRAM_38400  (TEKRAM_PW|0x02)
#define TEKRAM_19200  (TEKRAM_PW|0x03)
#define TEKRAM_9600   (TEKRAM_PW|0x04)
#define TEKRAM_2400   (TEKRAM_PW|0x08)

#define TEKRAM_TV     (TEKRAM_PW|0x05)

void
irts_tekram(struct tty *tp, u_int speed)
{
	int s;

	irt_setspeed(tp, 9600);
	irt_setline(tp, 0);
	irt_delay(tp, 50);

	irt_setline(tp, TIOCM_RTS);
	irt_delay(tp, 1);

	irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
	irt_delay(tp, 1);	/* 50 us */

	irt_setline(tp, TIOCM_DTR);
	irt_delay(tp, 1);	/* 7 us */

	switch(speed) {
	case 115200: s = TEKRAM_115200; break;
	case 57600:  s = TEKRAM_57600; break;
	case 38400:  s = TEKRAM_38400; break;
	case 19200:  s = TEKRAM_19200; break;
	case 2400:   s = TEKRAM_2400; break;
	default:     s = TEKRAM_9600; break;
	}
	irt_putc(tp, s);
	irframetstart(tp);

	irt_delay(tp, 100);

	irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
	if (speed != 9600)
		irt_setspeed(tp, speed);
	irt_delay(tp, 1);	/* 50 us */
}

/**********************************************************************
 * Jeteye
 **********************************************************************/
void
irts_jeteye(struct tty *tp, u_int speed)
{
	switch (speed) {
	case 19200:
		irt_setline(tp, TIOCM_DTR);
		break;
	case 115200:
		irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
		break;
	default: /*9600*/
		irt_setline(tp, TIOCM_RTS);
		break;
	}
	irt_setspeed(tp, speed);
}

/**********************************************************************
 * Actisys
 **********************************************************************/
void
irts_actisys(struct tty *tp, u_int speed)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int pulses;

	irt_setspeed(tp, speed);

	switch(speed) {
	case 19200:  pulses=1; break;
	case 57600:  pulses=2; break;
	case 115200: pulses=3; break;
	case 38400:  pulses=4; break;
	default: /* 9600 */ pulses=0; break;
	}

	if (sc->sc_dongle_private == 0) {
		sc->sc_dongle_private = 1;
		irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
		/* 
		 * Must wait at least 50ms after initial
		 * power on to charge internal capacitor
		 */
		irt_delay(tp, 50);
	}
	irt_setline(tp, TIOCM_RTS);
	delay(2);
	for (;;) {
		irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
		delay(2);
		if (--pulses <= 0)
			break;
		irt_setline(tp, TIOCM_DTR);    
		delay(2);
	}
}

/**********************************************************************
 * Litelink
 **********************************************************************/
void
irts_litelink(struct tty *tp, u_int speed)
{
	struct irframet_softc *sc = (struct irframet_softc *)tp->t_sc;
	int pulses;

	irt_setspeed(tp, speed);

	switch(speed) {
	case 57600:  pulses=1; break;
	case 38400:  pulses=2; break;
	case 19200:  pulses=3; break;
	case 9600:   pulses=4; break;
	default: /* 115200 */ pulses=0; break;
	}

	if (sc->sc_dongle_private == 0) {
		sc->sc_dongle_private = 1;
		irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
	}
	irt_setline(tp, TIOCM_RTS);
	irt_delay(tp, 1); /* 15 us */;
	for (;;) {
		irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
		irt_delay(tp, 1); /* 15 us */;
		if (--pulses <= 0)
			break;
		irt_setline(tp, TIOCM_DTR);    
		irt_delay(tp, 1); /* 15 us */;
	}
}

/**********************************************************************
 * Girbil
 **********************************************************************/
/* Control register 1 */
#define GIRBIL_TXEN      0x01 /* Enable transmitter */
#define GIRBIL_RXEN      0x02 /* Enable receiver */
#define GIRBIL_ECAN      0x04 /* Cancel self emmited data */
#define GIRBIL_ECHO      0x08 /* Echo control characters */

/* LED Current Register */
#define GIRBIL_HIGH      0x20
#define GIRBIL_MEDIUM    0x21
#define GIRBIL_LOW       0x22

/* Baud register */
#define GIRBIL_2400      0x30
#define GIRBIL_4800      0x31	
#define GIRBIL_9600      0x32
#define GIRBIL_19200     0x33
#define GIRBIL_38400     0x34	
#define GIRBIL_57600     0x35	
#define GIRBIL_115200    0x36

/* Mode register */
#define GIRBIL_IRDA      0x40
#define GIRBIL_ASK       0x41

/* Control register 2 */
#define GIRBIL_LOAD      0x51 /* Load the new baud rate value */

void
irts_girbil(struct tty *tp, u_int speed)
{
	int s;

	irt_setspeed(tp, 9600);
	irt_setline(tp, TIOCM_DTR);
	irt_delay(tp, 5);
	irt_setline(tp, TIOCM_RTS);
	irt_delay(tp, 20);
	switch(speed) {
	case 115200: s = GIRBIL_115200; break;
	case 57600:  s = GIRBIL_57600; break;
	case 38400:  s = GIRBIL_38400; break;
	case 19200:  s = GIRBIL_19200; break;
	case 4800:   s = GIRBIL_4800; break;
	case 2400:   s = GIRBIL_2400; break;
	default:     s = GIRBIL_9600; break;
	}
	irt_putc(tp, GIRBIL_TXEN|GIRBIL_RXEN);
	irt_putc(tp, s);
	irt_putc(tp, GIRBIL_LOAD);
	irframetstart(tp);
	irt_delay(tp, 100);
	irt_setline(tp, TIOCM_DTR | TIOCM_RTS);
	if (speed != 9600)
		irt_setspeed(tp, speed);
}
