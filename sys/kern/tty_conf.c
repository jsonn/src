/*	$NetBSD: tty_conf.c,v 1.24.8.1 2000/11/20 18:09:12 bouyer Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)tty_conf.c	8.5 (Berkeley) 1/9/95
 */

#include "opt_compat_freebsd.h"
#include "opt_compat_43.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/conf.h>

#define	ttynodisc ((int (*) __P((dev_t, struct tty *)))enodev)
#define	ttyerrclose ((int (*) __P((struct tty *, int flags)))enodev)
#define	ttyerrio ((int (*) __P((struct tty *, struct uio *, int)))enodev)
#define	ttyerrinput ((int (*) __P((int c, struct tty *)))enodev)
#define	ttyerrstart ((int (*) __P((struct tty *)))enodev)

int	nullioctl __P((struct tty *, u_long, caddr_t, int, struct proc *));

#include "tb.h"
#if NTB > 0
int	tbopen __P((dev_t dev, struct tty *tp));
int	tbclose __P((struct tty *tp, int flags));
int	tbread __P((struct tty *tp, struct uio *uio, int flags));
int	tbtioctl __P((struct tty *tp, u_long cmd, caddr_t data,
			int flag, struct proc *p));
int	tbinput __P((int c, struct tty *tp));
#endif

#include "sl.h"
#if NSL > 0
int	slopen __P((dev_t dev, struct tty *tp));
int	slclose __P((struct tty *tp, int flags));
int	sltioctl __P((struct tty *tp, u_long cmd, caddr_t data,
			int flag, struct proc *p));
int	slinput __P((int c, struct tty *tp));
int	slstart __P((struct tty *tp));
#endif

#include "ppp.h"
#if NPPP > 0
int	pppopen __P((dev_t dev, struct tty *tp));
int	pppclose __P((struct tty *tp, int flags));
int	ppptioctl __P((struct tty *tp, u_long cmd, caddr_t data,
			int flag, struct proc *p));
int	pppinput __P((int c, struct tty *tp));
int	pppstart __P((struct tty *tp));
int	pppread __P((struct tty *tp, struct uio *uio, int flag));
int	pppwrite __P((struct tty *tp, struct uio *uio, int flag));
#endif

#include "strip.h"
#if NSTRIP > 0
int	stripopen __P((dev_t dev, struct tty *tp));
int	stripclose __P((struct tty *tp, int flags));
int	striptioctl __P((struct tty *tp, u_long cmd, caddr_t data,
			int flag, struct proc *p));
int	stripinput __P((int c, struct tty *tp));
int	stripstart __P((struct tty *tp));
#endif

/*
 * XXXXXX
 *
 * The implementation for the following is currently in
 * sys/dev/sun.  I expect it will be moved out of there
 * eventually, but until then add yourself to the list if
 * you want to use the Sun Keyboard or Mouse line disciplines.
 */
#if defined(__sparc__) || defined(__sparc_v9__) || defined(sun3) || defined(sun3x)
#include "kbd.h"
#if NKBD > 0
int	sunkbdinput __P((int c, struct tty *tp));
int	sunkbdstart __P((struct tty *tp));
int	sunkbdstart __P((struct tty *tp));
#endif

#include "ms.h"
#if NMS > 0
int	sunmsinput __P((int c, struct tty *tp));
#endif
#endif

struct	linesw linesw[] =
{
	{ ttylopen, ttylclose, ttread, ttwrite, nullioctl,
	  ttyinput, ttstart, ttymodem },		/* 0- termios */

	{ ttynodisc, ttyerrclose, ttyerrio, ttyerrio, nullioctl,
	  ttyerrinput, ttyerrstart, nullmodem },	/* 1- defunct */

#if defined(COMPAT_43) || defined(COMPAT_FREEBSD)
	{ ttylopen, ttylclose, ttread, ttwrite, nullioctl,
	  ttyinput, ttstart, ttymodem },		/* 2- old NTTYDISC */
#else
	{ ttynodisc, ttyerrclose, ttyerrio, ttyerrio, nullioctl,
	  ttyerrinput, ttyerrstart, nullmodem },	/* 2- defunct */
#endif

#if NTB > 0
	{ tbopen, tbclose, tbread, ttyerrio, tbtioctl,
	  tbinput, ttstart, nullmodem },		/* 3- TABLDISC */
#else
	{ ttynodisc, ttyerrclose, ttyerrio, ttyerrio, nullioctl,
	  ttyerrinput, ttyerrstart, nullmodem },
#endif

#if NSL > 0
	{ slopen, slclose, ttyerrio, ttyerrio, sltioctl,
	  slinput, slstart, nullmodem },		/* 4- SLIPDISC */
#else
	{ ttynodisc, ttyerrclose, ttyerrio, ttyerrio, nullioctl,
	  ttyerrinput, ttyerrstart, nullmodem },
#endif

#if NPPP > 0
	{ pppopen, pppclose, pppread, pppwrite, ppptioctl,
	  pppinput, pppstart, ttymodem },		/* 5- PPPDISC */
#else
	{ ttynodisc, ttyerrclose, ttyerrio, ttyerrio, nullioctl,
	  ttyerrinput, ttyerrstart, nullmodem },
#endif

#if NSTRIP > 0
	{ stripopen, stripclose, ttyerrio, ttyerrio, striptioctl,
	  stripinput, stripstart, nullmodem },		/* 6- STRIPDISC */
#else
	{ ttynodisc, ttyerrclose, ttyerrio, ttyerrio, nullioctl,
	  ttyerrinput, ttyerrstart, nullmodem },
#endif

/*
 * The following are special line disciplines for Sun style Keybaords and Mice.
 * Since they are used to handle special hardware they are enabled if/when the
 * hardware is detected and you cannot switch in or out of them by normal means.
 *
 * All I/O currently goes through the keyboard and mouse device nodes so the
 * TTY does no I/O itself.
 */
#if NKBD > 0
	{ ttylopen, ttylclose, ttyerrio, ttyerrio, nullioctl,
	  sunkbdinput, sunkbdstart, nullmodem },	/* 7- SUNKBDDISC */
#else
	{ ttynodisc, ttyerrclose, ttyerrio, ttyerrio, nullioctl,
	  ttyerrinput, ttyerrstart, nullmodem },
#endif

#if NMS > 0
	{ ttylopen, ttylclose, ttyerrio, ttyerrio, nullioctl,
	  sunmsinput, ttstart, nullmodem },		/* 8- SUNMOUSEDISC */
#else
	{ ttynodisc, ttyerrclose, ttyerrio, ttyerrio, nullioctl,
	  ttyerrinput, ttyerrstart, nullmodem },
#endif
};

int	nlinesw = sizeof(linesw) / sizeof(linesw[0]);

/*
 * Do nothing specific version of line
 * discipline specific ioctl command.
 */
/*ARGSUSED*/
int
nullioctl(tp, cmd, data, flags, p)
	struct tty *tp;
	u_long cmd;
	char *data;
	int flags;
	struct proc *p;
{

#ifdef lint
	tp = tp; data = data; flags = flags; p = p;
#endif
	return (-1);
}
