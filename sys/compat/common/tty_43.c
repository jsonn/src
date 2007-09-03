/*	$NetBSD: tty_43.c,v 1.18.12.2 2007/09/03 14:31:51 yamt Exp $	*/

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
 *	@(#)tty_compat.c	8.2 (Berkeley) 1/9/95
 */

/*
 * mapping routines for old line discipline (yuck)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tty_43.c,v 1.18.12.2 2007/09/03 14:31:51 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/termios.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/ioctl_compat.h>

/*
 * XXX libcompat files should be included with config attributes
 */
#ifdef COMPAT_OLDTTY

int ttydebug = 0;

static const struct speedtab compatspeeds[] = {
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
static const int compatspcodes[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200,
	1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200
};

int ttcompatgetflags __P((struct tty *));
void ttcompatsetflags __P((struct tty *, struct termios *));
void ttcompatsetlflags __P((struct tty *, struct termios *));

/*ARGSUSED*/
int
ttcompat(tp, com, data, flag, l)
	struct tty *tp;
	u_long com;
	void *data;
	int flag;
	struct lwp *l;
{

	switch (com) {
	case TIOCGETP: {
		struct sgttyb *sg = (struct sgttyb *)data;
		u_char *cc = tp->t_cc;
		int speed;

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
		struct sgttyb *sg = (struct sgttyb *)data;
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
			(void *)&term, flag, l));
	}

	case TIOCGETC: {
		struct tchars *tc = (struct tchars *)data;
		u_char *cc = tp->t_cc;

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
		u_char *cc = tp->t_cc;

		cc[VINTR] = tc->t_intrc;
		cc[VQUIT] = tc->t_quitc;
		cc[VSTART] = tc->t_startc;
		cc[VSTOP] = tc->t_stopc;
		cc[VEOF] = tc->t_eofc;
		cc[VEOL] = tc->t_brkc;
		if (tc->t_brkc == (char)-1)
			cc[VEOL2] = _POSIX_VDISABLE;
		break;
	}
	case TIOCSLTC: {
		struct ltchars *ltc = (struct ltchars *)data;
		u_char *cc = tp->t_cc;

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
		u_char *cc = tp->t_cc;

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
		int flags;

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
		return (ttioctl(tp, TIOCSETA, (void *)&term, flag, l));
	}
	case TIOCLGET:
		*(int *)data = ttcompatgetflags(tp)>>16;
		if (ttydebug)
			printf("CLGET: returning %x\n", *(int *)data);
		break;

	case OTIOCGETD:
		*(int *)data = (tp->t_linesw == NULL) ?
		    2 /* XXX old NTTYDISC */ : tp->t_linesw->l_no;
		break;

	case OTIOCSETD: {
		int ldisczero = 0;

		return (ttioctl(tp, TIOCSETD,
			*(int *)data == 2 ? (void *)&ldisczero : data, flag,
			l));
	    }

	case OTIOCCONS:
		*(int *)data = 1;
		return (ttioctl(tp, TIOCCONS, data, flag, l));

	case TIOCHPCL:
		SET(tp->t_cflag, HUPCL);
		break;

	case TIOCGSID:
		if (tp->t_session == NULL)
			return ENOTTY;

		if (tp->t_session->s_leader == NULL)
			return ENOTTY;

		*(int *) data =  tp->t_session->s_leader->p_pid;
		break;

	default:
		return (EPASSTHROUGH);
	}
	return (0);
}

int
ttcompatgetflags(tp)
	struct tty *tp;
{
	tcflag_t iflag = tp->t_iflag;
	tcflag_t lflag = tp->t_lflag;
	tcflag_t oflag = tp->t_oflag;
	tcflag_t cflag = tp->t_cflag;
	int flags = 0;

	if (ISSET(iflag, IXOFF))
		SET(flags, TANDEM);
	if (ISSET(iflag, ICRNL) || ISSET(oflag, ONLCR))
		SET(flags, CRMOD);
	if (ISSET(cflag, PARENB)) {
		if (ISSET(iflag, INPCK)) {
			if (ISSET(cflag, PARODD))
				SET(flags, ODDP);
			else
				SET(flags, EVENP);
		} else
			SET(flags, ANYP);
	}

	if (!ISSET(lflag, ICANON)) {
		/* fudge */
		if (ISSET(iflag, IXON) || ISSET(lflag, ISIG|IEXTEN) ||
		    ISSET(cflag, PARENB))
			SET(flags, CBREAK);
		else
			SET(flags, RAW);
	}

	if (ISSET(flags, RAW))
		SET(flags, ISSET(tp->t_flags, LITOUT|PASS8));
	else if (ISSET(cflag, CSIZE) == CS8) {
		if (!ISSET(oflag, OPOST))
			SET(flags, LITOUT);
		if (!ISSET(iflag, ISTRIP))
			SET(flags, PASS8);
	}

	if (ISSET(cflag, MDMBUF))
		SET(flags, MDMBUF);
	if (!ISSET(cflag, HUPCL))
		SET(flags, NOHANG);
	if (ISSET(oflag, OXTABS))
		SET(flags, XTABS);
	if (ISSET(lflag, ECHOE))
		SET(flags, CRTERA|CRTBS);
	if (ISSET(lflag, ECHOKE))
		SET(flags, CRTKIL|CRTBS);
	if (ISSET(lflag, ECHOPRT))
		SET(flags, PRTERA);
	if (ISSET(lflag, ECHOCTL))
		SET(flags, CTLECH);
	if (!ISSET(iflag, IXANY))
		SET(flags, DECCTQ);
	SET(flags, ISSET(lflag, ECHO|TOSTOP|FLUSHO|PENDIN|NOFLSH));
	if (ttydebug)
		printf("getflags: %x\n", flags);
	return (flags);
}

void
ttcompatsetflags(tp, t)
	struct tty *tp;
	struct termios *t;
{
	int flags = tp->t_flags;
	tcflag_t iflag = t->c_iflag;
	tcflag_t oflag = t->c_oflag;
	tcflag_t lflag = t->c_lflag;
	tcflag_t cflag = t->c_cflag;

	if (ISSET(flags, TANDEM))
		SET(iflag, IXOFF);
	else
		CLR(iflag, IXOFF);
	if (ISSET(flags, ECHO))
		SET(lflag, ECHO);
	else
		CLR(lflag, ECHO);
	if (ISSET(flags, CRMOD)) {
		SET(iflag, ICRNL);
		SET(oflag, ONLCR);
	} else {
		CLR(iflag, ICRNL);
		CLR(oflag, ONLCR);
	}
	if (ISSET(flags, XTABS))
		SET(oflag, OXTABS);
	else
		CLR(oflag, OXTABS);


	if (ISSET(flags, RAW)) {
		iflag &= IXOFF;
		CLR(lflag, ISIG|ICANON|IEXTEN);
		CLR(cflag, PARENB);
	} else {
		SET(iflag, BRKINT|IXON|IMAXBEL);
		SET(lflag, ISIG|IEXTEN);
		if (ISSET(flags, CBREAK))
			CLR(lflag, ICANON);
		else
			SET(lflag, ICANON);
		switch (ISSET(flags, ANYP)) {
		case 0:
			CLR(cflag, PARENB);
			break;
		case ANYP:
			SET(cflag, PARENB);
			CLR(iflag, INPCK);
			break;
		case EVENP:
			SET(cflag, PARENB);
			SET(iflag, INPCK);
			CLR(cflag, PARODD);
			break;
		case ODDP:
			SET(cflag, PARENB);
			SET(iflag, INPCK);
			SET(cflag, PARODD);
			break;
		}
	}

	if (ISSET(flags, RAW|LITOUT|PASS8)) {
		CLR(cflag, CSIZE);
		SET(cflag, CS8);
		if (!ISSET(flags, RAW|PASS8))
			SET(iflag, ISTRIP);
		else
			CLR(iflag, ISTRIP);
		if (!ISSET(flags, RAW|LITOUT))
			SET(oflag, OPOST);
		else
			CLR(oflag, OPOST);
	} else {
		CLR(cflag, CSIZE);
		SET(cflag, CS7);
		SET(iflag, ISTRIP);
		SET(oflag, OPOST);
	}

	t->c_iflag = iflag;
	t->c_oflag = oflag;
	t->c_lflag = lflag;
	t->c_cflag = cflag;
}

void
ttcompatsetlflags(tp, t)
	struct tty *tp;
	struct termios *t;
{
	int flags = tp->t_flags;
	tcflag_t iflag = t->c_iflag;
	tcflag_t oflag = t->c_oflag;
	tcflag_t lflag = t->c_lflag;
	tcflag_t cflag = t->c_cflag;

	/* Nothing we can do with CRTBS. */
	if (ISSET(flags, PRTERA))
		SET(lflag, ECHOPRT);
	else
		CLR(lflag, ECHOPRT);
	if (ISSET(flags, CRTERA))
		SET(lflag, ECHOE);
	else
		CLR(lflag, ECHOE);
	/* Nothing we can do with TILDE. */
	if (ISSET(flags, MDMBUF))
		SET(cflag, MDMBUF);
	else
		CLR(cflag, MDMBUF);
	if (ISSET(flags, NOHANG))
		CLR(cflag, HUPCL);
	else
		SET(cflag, HUPCL);
	if (ISSET(flags, CRTKIL))
		SET(lflag, ECHOKE);
	else
		CLR(lflag, ECHOKE);
	if (ISSET(flags, CTLECH))
		SET(lflag, ECHOCTL);
	else
		CLR(lflag, ECHOCTL);
	if (!ISSET(flags, DECCTQ))
		SET(iflag, IXANY);
	else
		CLR(iflag, IXANY);
	CLR(lflag, TOSTOP|FLUSHO|PENDIN|NOFLSH);
	SET(lflag, ISSET(flags, TOSTOP|FLUSHO|PENDIN|NOFLSH));

	if (ISSET(flags, RAW|LITOUT|PASS8)) {
		CLR(cflag, CSIZE);
		SET(cflag, CS8);
		if (!ISSET(flags, RAW|PASS8))
			SET(iflag, ISTRIP);
		else
			CLR(iflag, ISTRIP);
		if (!ISSET(flags, RAW|LITOUT))
			SET(oflag, OPOST);
		else
			CLR(oflag, OPOST);
	} else {
		CLR(cflag, CSIZE);
		SET(cflag, CS7);
		SET(iflag, ISTRIP);
		SET(oflag, OPOST);
	}

	t->c_iflag = iflag;
	t->c_oflag = oflag;
	t->c_lflag = lflag;
	t->c_cflag = cflag;
}

#endif /* COMPAT_OLDTTY */
