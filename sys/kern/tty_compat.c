/*	$NetBSD: tty_compat.c,v 1.14.2.1 1994/08/03 03:51:06 cgd Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)tty_compat.c	8.1 (Berkeley) 6/10/93
 */

/* 
 * mapping routines for old line discipline (yuck)
 */
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/termios.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

int ttydebug = 0;

static struct speedtab compatspeeds[] = {
#define MAX_SPEED	17
	{ 115200, 17 },
	{ 57600, 16 },
	{ 38400, 15 },
	{ 19200, 14 },
	{ 9600,	13 },
	{ 4800,	12 },
	{ 2400,	11 },
	{ 1800,	10 },
	{ 1200,	9 },
	{ 600,	8 },
	{ 300,	7 },
	{ 200,	6 },
	{ 150,	5 },
	{ 134,	4 },
	{ 110,	3 },
	{ 75,	2 },
	{ 50,	1 },
	{ 0,	0 },
	{ -1,	-1 },
};
static int compatspcodes[] = { 
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200,
	1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200
};

/*ARGSUSED*/
ttcompat(tp, com, data, flag, p)
	register struct tty *tp;
	int com;
	caddr_t data;
	int flag;
	struct proc *p;
{

	switch (com) {
	case TIOCGETP: {
		register struct sgttyb *sg = (struct sgttyb *)data;
		register u_char *cc = tp->t_cc;
		register speed;

		speed = ttspeedtab(tp->t_ospeed, compatspeeds);
		sg->sg_ospeed = (speed == -1) ? MAX_SPEED : speed;
		if (tp->t_ispeed == 0)
			sg->sg_ispeed = sg->sg_ospeed;
		else {
			speed = ttspeedtab(tp->t_ispeed, compatspeeds);
			sg->sg_ispeed = (speed == -1) ? MAX_SPEED : speed;
		}
		sg->sg_erase = cc[VERASE];
		sg->sg_kill = cc[VKILL];
		sg->sg_flags = ttcompatgetflags(tp);
		break;
	}

	case TIOCSETP:
	case TIOCSETN: {
		register struct sgttyb *sg = (struct sgttyb *)data;
		struct termios term;
		int speed;

		term = tp->t_termios;
		if ((speed = sg->sg_ispeed) > MAX_SPEED || speed < 0)
			term.c_ispeed = speed;
		else
			term.c_ispeed = compatspcodes[speed];
		if ((speed = sg->sg_ospeed) > MAX_SPEED || speed < 0)
			term.c_ospeed = speed;
		else
			term.c_ospeed = compatspcodes[speed];
		term.c_cc[VERASE] = sg->sg_erase;
		term.c_cc[VKILL] = sg->sg_kill;
		tp->t_flags = (ttcompatgetflags(tp)&0xffff0000) | (sg->sg_flags&0xffff);
		ttcompatsetflags(tp, &term);
		return (ttioctl(tp, com == TIOCSETP ? TIOCSETAF : TIOCSETA, 
			&term, flag, p));
	}

	case TIOCGETC: {
		struct tchars *tc = (struct tchars *)data;
		register u_char *cc = tp->t_cc;

		tc->t_intrc = cc[VINTR];
		tc->t_quitc = cc[VQUIT];
		tc->t_startc = cc[VSTART];
		tc->t_stopc = cc[VSTOP];
		tc->t_eofc = cc[VEOF];
		tc->t_brkc = cc[VEOL];
		break;
	}
	case TIOCSETC: {
		struct tchars *tc = (struct tchars *)data;
		register u_char *cc = tp->t_cc;

		cc[VINTR] = tc->t_intrc;
		cc[VQUIT] = tc->t_quitc;
		cc[VSTART] = tc->t_startc;
		cc[VSTOP] = tc->t_stopc;
		cc[VEOF] = tc->t_eofc;
		cc[VEOL] = tc->t_brkc;
		if (tc->t_brkc == -1)
			cc[VEOL2] = _POSIX_VDISABLE;
		break;
	}
	case TIOCSLTC: {
		struct ltchars *ltc = (struct ltchars *)data;
		register u_char *cc = tp->t_cc;

		cc[VSUSP] = ltc->t_suspc;
		cc[VDSUSP] = ltc->t_dsuspc;
		cc[VREPRINT] = ltc->t_rprntc;
		cc[VDISCARD] = ltc->t_flushc;
		cc[VWERASE] = ltc->t_werasc;
		cc[VLNEXT] = ltc->t_lnextc;
		break;
	}
	case TIOCGLTC: {
		struct ltchars *ltc = (struct ltchars *)data;
		register u_char *cc = tp->t_cc;

		ltc->t_suspc = cc[VSUSP];
		ltc->t_dsuspc = cc[VDSUSP];
		ltc->t_rprntc = cc[VREPRINT];
		ltc->t_flushc = cc[VDISCARD];
		ltc->t_werasc = cc[VWERASE];
		ltc->t_lnextc = cc[VLNEXT];
		break;
	}
	case TIOCLBIS:
	case TIOCLBIC:
	case TIOCLSET: {
		struct termios term;
		long flags;

		term = tp->t_termios;
		flags = ttcompatgetflags(tp);
		switch (com) {
		case TIOCLSET:
			tp->t_flags = (flags&0xffff) | (*(int *)data<<16);
			break;
		case TIOCLBIS:
			tp->t_flags = flags | (*(int *)data<<16);
			break;
		case TIOCLBIC:
			tp->t_flags = flags & ~(*(int *)data<<16);
			break;
		}
		ttcompatsetlflags(tp, &term);
		return (ttioctl(tp, TIOCSETA, &term, flag, p));
	}
	case TIOCLGET:
		*(int *)data = ttcompatgetflags(tp)>>16;
		if (ttydebug)
			printf("CLGET: returning %x\n", *(int *)data);
		break;

	case OTIOCGETD:
		*(int *)data = tp->t_line ? tp->t_line : 2;
		break;

	case OTIOCSETD: {
		int ldisczero = 0;

		return (ttioctl(tp, TIOCSETD, 
			*(int *)data == 2 ? (caddr_t)&ldisczero : data, flag,
			p));
	    }

	case OTIOCCONS:
		*(int *)data = 1;
		return (ttioctl(tp, TIOCCONS, data, flag, p));

	case TIOCHPCL:
		tp->t_cflag |= HUPCL;
		break;

	default:
		return (-1);
	}
	return (0);
}

ttcompatgetflags(tp)
	register struct tty *tp;
{
	register long iflag = tp->t_iflag;
	register long lflag = tp->t_lflag;
	register long oflag = tp->t_oflag;
	register long cflag = tp->t_cflag;
	register flags = 0;

	if (iflag & IXOFF)
		flags |= TANDEM;
	if (iflag & ICRNL || oflag & ONLCR)
		flags |= CRMOD;
	if (cflag & PARENB) {
		if (iflag & INPCK) {
			if (cflag & PARODD)
				flags |= ODDP;
			else
				flags |= EVENP;
		} else
			flags |= EVENP | ODDP;
	} else {
		if ((tp->t_flags & LITOUT) && !(oflag & OPOST))
			flags |= LITOUT;
		if (tp->t_flags & PASS8)
			flags |= PASS8;
	}
	
	if ((lflag & ICANON) == 0) {	
		/* fudge */
		if (iflag&IXON || lflag&ISIG || lflag&IEXTEN || cflag&PARENB)
			flags |= CBREAK;
		else
			flags |= RAW;
	}
	if (cflag & MDMBUF)
		flags |= MDMBUF;
	if ((cflag & HUPCL) == 0)
		flags |= NOHANG;
	if (oflag & OXTABS)
		flags |= XTABS;
	if (lflag & ECHOE)
		flags |= CRTERA|CRTBS;
	if (lflag & ECHOKE)
		flags |= CRTKIL|CRTBS;
	if (lflag & ECHOPRT)
		flags |= PRTERA;
	if (lflag & ECHOCTL)
		flags |= CTLECH;
	if ((iflag & IXANY) == 0)
		flags |= DECCTQ;
	flags |= lflag & (ECHO|TOSTOP|FLUSHO|PENDIN|NOFLSH);
	if (ttydebug)
		printf("getflags: %x\n", flags);
	return (flags);
}

ttcompatsetflags(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register flags = tp->t_flags;
	register long iflag = t->c_iflag;
	register long oflag = t->c_oflag;
	register long lflag = t->c_lflag;
	register long cflag = t->c_cflag;

	if (flags & TANDEM)
		iflag |= IXOFF;
	else
		iflag &= ~IXOFF;
	if (flags & ECHO)
		lflag |= ECHO;
	else
		lflag &= ~ECHO;
	if (flags & CRMOD) {
		iflag |= ICRNL;
		oflag |= ONLCR;
	} else {
		iflag &= ~ICRNL;
		oflag &= ~ONLCR;
	}
	if (flags & XTABS)
		oflag |= OXTABS;
	else
		oflag &= ~OXTABS;


	if (flags & RAW) {
		iflag &= IXOFF;
		lflag &= ~(ISIG|ICANON|IEXTEN);
	} else {
		iflag |= BRKINT|IXON|IMAXBEL;
		lflag |= ISIG|IEXTEN;
		if (flags & CBREAK)
			lflag &= ~ICANON;
		else
			lflag |= ICANON;
	}
		
	switch (flags & ANYP) {
	case EVENP:
		iflag |= INPCK;
		cflag &= ~PARODD;
		break;
	case ODDP:
		iflag |= INPCK;
		cflag |= PARODD;
		break;
	default:
		iflag &= ~INPCK;
		break;
	}

	if (flags & (RAW|LITOUT|PASS8)) {
		cflag &= ~(CSIZE|PARENB);
		cflag |= CS8;
		if ((flags & (RAW|PASS8)) == 0)
			iflag |= ISTRIP;
		else
			iflag &= ~ISTRIP;
		if ((flags & (RAW|LITOUT)) == 0)
			oflag |= OPOST;
		else
			oflag &= ~OPOST;
	} else {
		cflag &= ~CSIZE;
		cflag |= CS7|PARENB;
		iflag |= ISTRIP;
		oflag |= OPOST;
	}

	t->c_iflag = iflag;
	t->c_oflag = oflag;
	t->c_lflag = lflag;
	t->c_cflag = cflag;
}

ttcompatsetlflags(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register flags = tp->t_flags;
	register long iflag = t->c_iflag;
	register long oflag = t->c_oflag;
	register long lflag = t->c_lflag;
	register long cflag = t->c_cflag;

	/* Nothing we can do with CRTBS. */
	if (flags & PRTERA)
		lflag |= ECHOPRT;
	else
		lflag &= ~ECHOPRT;
	if (flags & CRTERA)
		lflag |= ECHOE;
	else
		lflag &= ~ECHOE;
	/* Nothing we can do with TILDE. */
	if (flags & MDMBUF)
		cflag |= MDMBUF;
	else
		cflag &= ~MDMBUF;
	if (flags & NOHANG)
		cflag &= ~HUPCL;
	else
		cflag |= HUPCL;
	if (flags & CRTKIL)
		lflag |= ECHOKE;
	else
		lflag &= ~ECHOKE;
	if (flags & CTLECH)
		lflag |= ECHOCTL;
	else
		lflag &= ~ECHOCTL;
	if ((flags & DECCTQ) == 0)
		iflag |= IXANY;
	else
		iflag &= ~IXANY;
	lflag &= ~(TOSTOP|FLUSHO|PENDIN|NOFLSH);
	lflag |= flags&(TOSTOP|FLUSHO|PENDIN|NOFLSH);

	if (flags & (RAW|LITOUT|PASS8)) {
		cflag &= ~(CSIZE|PARENB);
		cflag |= CS8;
		if ((flags & (RAW|PASS8)) == 0)
			iflag |= ISTRIP;
		else
			iflag &= ~ISTRIP;
		if ((flags & (RAW|LITOUT)) == 0)
			oflag |= OPOST;
		else
			oflag &= ~OPOST;
	} else {
		cflag &= ~CSIZE;
		cflag |= CS7|PARENB;
		iflag |= ISTRIP;
		oflag |= OPOST;
	}

	t->c_iflag = iflag;
	t->c_oflag = oflag;
	t->c_lflag = lflag;
	t->c_cflag = cflag;
}
#endif	/* COMPAT_43 || COMPAT_SUNOS */
