/*
 * Copyright (c) 1993 Markus Wild.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * loosely from: Header: sun_ioctl.c,v 1.7 93/05/28 04:40:43 torek Exp 
 * $Id: sun_ioctl.c,v 1.5.2.2 1993/11/28 02:31:11 deraadt Exp $
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/tty.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

/*
 * SunOS ioctl calls.
 * This file is something of a hodge-podge.
 * Support gets added as things turn up....
 */

struct sun_ttysize {
	int	ts_row;
	int	ts_col;
};

struct sun_termio {
	u_short	c_iflag;
	u_short	c_oflag;
	u_short	c_cflag;
	u_short	c_lflag;
	char	c_line;
	unsigned char c_cc[8];
};
#define SUN_TCGETA	_IOR('T', 1, struct sun_termio)
#define SUN_TCSETA	_IOW('T', 2, struct sun_termio)
#define SUN_TCSETAW	_IOW('T', 3, struct sun_termio)
#define SUN_TCSETAF	_IOW('T', 4, struct sun_termio)
#define SUN_TCSBRK	_IO('T', 5)

struct sun_termios {
	u_long	c_iflag;
	u_long	c_oflag;
	u_long	c_cflag;
	u_long	c_lflag;
	char	c_line;
	u_char	c_cc[17];
};
#define SUN_TCXONC	_IO('T', 6)
#define SUN_TCFLSH	_IO('T', 7)
#define SUN_TCGETS	_IOR('T', 8, struct sun_termios)
#define SUN_TCSETS	_IOW('T', 9, struct sun_termios)
#define SUN_TCSETSW	_IOW('T', 10, struct sun_termios)
#define SUN_TCSETSF	_IOW('T', 11, struct sun_termios)
#define SUN_TCSNDBRK	_IO('T', 12)
#define SUN_TCDRAIN	_IO('T', 13)

static struct speedtab sptab[] = {
	{ 0, 0 },
	{ 50, 1 },
	{ 75, 2 },
	{ 110, 3 },
	{ 134, 4 },
	{ 135, 4 },
	{ 150, 5 },
	{ 200, 6 },
	{ 300, 7 },
	{ 600, 8 },
	{ 1200, 9 },
	{ 1800, 10 },
	{ 2400, 11 },
	{ 4800, 12 },
	{ 9600, 13 },
	{ 19200, 14 },
	{ 38400, 15 },
	{ -1, -1 }
};

static u_long s2btab[] = { 
	0,
	50,
	75,
	110,
	134,
	150,
	200,
	300,
	600,
	1200,
	1800,
	2400,
	4800,
	9600,
	19200,
	38400,
};

/*
 * these two conversion functions have mostly been done
 * with some perl cut&paste, then handedited to comment
 * out what doesn't exist under NetBSD.
 * A note from Markus's code:
 *	(l & BITMASK1) / BITMASK1 * BITMASK2  is translated
 *	optimally by gcc m68k, much better than any ?: stuff.
 *	Code may vary with different architectures of course.
 *
 * I don't know what optimizer you used, but seeing divu's and
 * bfextu's in the m68k assembly output did not encourage me...
 * as well, gcc on the sparc definately generates much better
 * code with ?:.
 */

static void
stios2btios(st, bt)
	struct sun_termios *st;
	struct termios *bt;
{
	register u_long l, r;

	l = st->c_iflag;
	r = 	((l & 0x00000001) ? IGNBRK	: 0);
	r |=	((l & 0x00000002) ? BRKINT	: 0);
	r |=	((l & 0x00000004) ? IGNPAR	: 0);
	r |=	((l & 0x00000008) ? PARMRK	: 0);
	r |=	((l & 0x00000010) ? INPCK	: 0);
	r |=	((l & 0x00000020) ? ISTRIP	: 0);
	r |= 	((l & 0x00000040) ? INLCR	: 0);
	r |=	((l & 0x00000080) ? IGNCR	: 0);
	r |=	((l & 0x00000100) ? ICRNL	: 0);
	/*	((l & 0x00000200) ? IUCLC	: 0) */
	r |=	((l & 0x00000400) ? IXON	: 0);
	r |=	((l & 0x00000800) ? IXANY	: 0);
	r |=	((l & 0x00001000) ? IXOFF	: 0);
	r |=	((l & 0x00002000) ? IMAXBEL	: 0);
	bt->c_iflag = r;

	l = st->c_oflag;
	r = 	((l & 0x00000001) ? OPOST	: 0);
	/*	((l & 0x00000002) ? OLCUC	: 0) */
	r |=	((l & 0x00000004) ? ONLCR	: 0);
	/*	((l & 0x00000008) ? OCRNL	: 0) */
	/*	((l & 0x00000010) ? ONOCR	: 0) */
	/*	((l & 0x00000020) ? ONLRET	: 0) */
	/*	((l & 0x00000040) ? OFILL	: 0) */
	/*	((l & 0x00000080) ? OFDEL	: 0) */
	/*	((l & 0x00000100) ? NLDLY	: 0) */
	/*	((l & 0x00000100) ? NL1		: 0) */
	/*	((l & 0x00000600) ? CRDLY	: 0) */
	/*	((l & 0x00000200) ? CR1		: 0) */
	/*	((l & 0x00000400) ? CR2		: 0) */
	/*	((l & 0x00000600) ? CR3		: 0) */
	/*	((l & 0x00001800) ? TABDLY	: 0) */
	/*	((l & 0x00000800) ? TAB1	: 0) */
	/*	((l & 0x00001000) ? TAB2	: 0) */
	r |=	((l & 0x00001800) ? OXTABS	: 0);
	/*	((l & 0x00002000) ? BSDLY	: 0) */
	/*	((l & 0x00002000) ? BS1		: 0) */
	/*	((l & 0x00004000) ? VTDLY	: 0) */
	/*	((l & 0x00004000) ? VT1		: 0) */
	/*	((l & 0x00008000) ? FFDLY	: 0) */
	/*	((l & 0x00008000) ? FF1		: 0) */
	/*	((l & 0x00010000) ? PAGEOUT	: 0) */
	/*	((l & 0x00020000) ? WRAP	: 0) */
	bt->c_oflag = r;

	l = st->c_cflag;
	r = 	((l & 0x00000010) ? CS6		: 0);
	r |=	((l & 0x00000020) ? CS7		: 0);
	r |=	((l & 0x00000030) ? CS8		: 0);
	r |=	((l & 0x00000040) ? CSTOPB	: 0);
	r |=	((l & 0x00000080) ? CREAD	: 0);
	r |= 	((l & 0x00000100) ? PARENB	: 0);
	r |=	((l & 0x00000200) ? PARODD	: 0);
	r |=	((l & 0x00000400) ? HUPCL	: 0);
	r |=	((l & 0x00000800) ? CLOCAL	: 0);
	/*	((l & 0x00001000) ? LOBLK	: 0) */
	r |=	((l & 0x80000000) ? (CRTS_IFLOW|CCTS_OFLOW) : 0);
	bt->c_cflag = r;

	bt->c_ispeed = bt->c_ospeed = s2btab[l & 0x0000000f];

	l = st->c_lflag;
	r = 	((l & 0x00000001) ? ISIG	: 0);
	r |=	((l & 0x00000002) ? ICANON	: 0);
	/*	((l & 0x00000004) ? XCASE	: 0) */
	r |=	((l & 0x00000008) ? ECHO	: 0);
	r |=	((l & 0x00000010) ? ECHOE	: 0);
	r |=	((l & 0x00000020) ? ECHOK	: 0);
	r |=	((l & 0x00000040) ? ECHONL	: 0);
	r |= 	((l & 0x00000080) ? NOFLSH	: 0);
	r |=	((l & 0x00000100) ? TOSTOP	: 0);
	r |=	((l & 0x00000200) ? ECHOCTL	: 0);
	r |=	((l & 0x00000400) ? ECHOPRT	: 0);
	r |=	((l & 0x00000800) ? ECHOKE	: 0);
	/*	((l & 0x00001000) ? DEFECHO	: 0) */
	r |=	((l & 0x00002000) ? FLUSHO	: 0);
	r |=	((l & 0x00004000) ? PENDIN	: 0);
	bt->c_lflag = r;

	bt->c_cc[VINTR]    = st->c_cc[0]  ? st->c_cc[0]  : _POSIX_VDISABLE;
	bt->c_cc[VQUIT]    = st->c_cc[1]  ? st->c_cc[1]  : _POSIX_VDISABLE;
	bt->c_cc[VERASE]   = st->c_cc[2]  ? st->c_cc[2]  : _POSIX_VDISABLE;
	bt->c_cc[VKILL]    = st->c_cc[3]  ? st->c_cc[3]  : _POSIX_VDISABLE;
	bt->c_cc[VEOF]     = st->c_cc[4]  ? st->c_cc[4]  : _POSIX_VDISABLE;
	bt->c_cc[VEOL]     = st->c_cc[5]  ? st->c_cc[5]  : _POSIX_VDISABLE;
	bt->c_cc[VEOL2]    = st->c_cc[6]  ? st->c_cc[6]  : _POSIX_VDISABLE;
    /*	bt->c_cc[VSWTCH]   = st->c_cc[7]  ? st->c_cc[7]  : _POSIX_VDISABLE; */
	bt->c_cc[VSTART]   = st->c_cc[8]  ? st->c_cc[8]  : _POSIX_VDISABLE;
	bt->c_cc[VSTOP]    = st->c_cc[9]  ? st->c_cc[9]  : _POSIX_VDISABLE;
	bt->c_cc[VSUSP]    = st->c_cc[10] ? st->c_cc[10] : _POSIX_VDISABLE;
	bt->c_cc[VDSUSP]   = st->c_cc[11] ? st->c_cc[11] : _POSIX_VDISABLE;
	bt->c_cc[VREPRINT] = st->c_cc[12] ? st->c_cc[12] : _POSIX_VDISABLE;
	bt->c_cc[VDISCARD] = st->c_cc[13] ? st->c_cc[13] : _POSIX_VDISABLE;
	bt->c_cc[VWERASE]  = st->c_cc[14] ? st->c_cc[14] : _POSIX_VDISABLE;
	bt->c_cc[VLNEXT]   = st->c_cc[15] ? st->c_cc[15] : _POSIX_VDISABLE;
	bt->c_cc[VSTATUS]  = st->c_cc[16] ? st->c_cc[16] : _POSIX_VDISABLE;
}


static void
btios2stios(bt, st)
	struct termios *bt;
	struct sun_termios *st;
{
	register u_long l, r;

	l = bt->c_iflag;
	r = 	((l &  IGNBRK) ? 0x00000001	: 0);
	r |=	((l &  BRKINT) ? 0x00000002	: 0);
	r |=	((l &  IGNPAR) ? 0x00000004	: 0);
	r |=	((l &  PARMRK) ? 0x00000008	: 0);
	r |=	((l &   INPCK) ? 0x00000010	: 0);
	r |=	((l &  ISTRIP) ? 0x00000020	: 0);
	r |=	((l &   INLCR) ? 0x00000040	: 0);
	r |=	((l &   IGNCR) ? 0x00000080	: 0);
	r |=	((l &   ICRNL) ? 0x00000100	: 0);
	/*	((l &   IUCLC) ? 0x00000200	: 0) */
	r |=	((l &    IXON) ? 0x00000400	: 0);
	r |=	((l &   IXANY) ? 0x00000800	: 0);
	r |=	((l &   IXOFF) ? 0x00001000	: 0);
	r |=	((l & IMAXBEL) ? 0x00002000	: 0);
	st->c_iflag = r;

	l = bt->c_oflag;
	r =	((l &   OPOST) ? 0x00000001	: 0);
	/*	((l &   OLCUC) ? 0x00000002	: 0) */
	r |=	((l &   ONLCR) ? 0x00000004	: 0);
	/*	((l &   OCRNL) ? 0x00000008	: 0) */
	/*	((l &   ONOCR) ? 0x00000010	: 0) */
	/*	((l &  ONLRET) ? 0x00000020	: 0) */
	/*	((l &   OFILL) ? 0x00000040	: 0) */
	/*	((l &   OFDEL) ? 0x00000080	: 0) */
	/*	((l &   NLDLY) ? 0x00000100	: 0) */
	/*	((l &     NL1) ? 0x00000100	: 0) */
	/*	((l &   CRDLY) ? 0x00000600	: 0) */
	/*	((l &     CR1) ? 0x00000200	: 0) */
	/*	((l &     CR2) ? 0x00000400	: 0) */
	/*	((l &     CR3) ? 0x00000600	: 0) */
	/*	((l &  TABDLY) ? 0x00001800	: 0) */
	/*	((l &    TAB1) ? 0x00000800	: 0) */
	/*	((l &    TAB2) ? 0x00001000	: 0) */
	r |=	((l &  OXTABS) ? 0x00001800	: 0);
	/*	((l &   BSDLY) ? 0x00002000	: 0) */
	/*	((l &     BS1) ? 0x00002000	: 0) */
	/*	((l &   VTDLY) ? 0x00004000	: 0) */
	/*	((l &     VT1) ? 0x00004000	: 0) */
	/*	((l &   FFDLY) ? 0x00008000	: 0) */
	/*	((l &     FF1) ? 0x00008000	: 0) */
	/*	((l & PAGEOUT) ? 0x00010000	: 0) */
	/*	((l &    WRAP) ? 0x00020000	: 0) */
	st->c_oflag = r;

	l = bt->c_cflag;
	r = 	((l &     CS6) ? 0x00000010	: 0);
	r |=	((l &     CS7) ? 0x00000020	: 0);
	r |=	((l &     CS8) ? 0x00000030	: 0);
	r |=	((l &  CSTOPB) ? 0x00000040	: 0);
	r |=	((l &   CREAD) ? 0x00000080	: 0);
	r |=	((l &  PARENB) ? 0x00000100	: 0);
	r |=	((l &  PARODD) ? 0x00000200	: 0);
	r |=	((l &   HUPCL) ? 0x00000400	: 0);
	r |=	((l &  CLOCAL) ? 0x00000800	: 0);
	/*	((l &   LOBLK) ? 0x00001000	: 0) */
	r |=	((l & (CRTS_IFLOW|CCTS_OFLOW)) ? 0x80000000 : 0);
	st->c_cflag = r;

	l = bt->c_lflag;
	r =	((l &    ISIG) ? 0x00000001	: 0);
	r |=	((l &  ICANON) ? 0x00000002	: 0);
	/*	((l &   XCASE) ? 0x00000004	: 0) */
	r |=	((l &    ECHO) ? 0x00000008	: 0);
	r |=	((l &   ECHOE) ? 0x00000010	: 0);
	r |=	((l &   ECHOK) ? 0x00000020	: 0);
	r |=	((l &  ECHONL) ? 0x00000040	: 0);
	r |=	((l &  NOFLSH) ? 0x00000080	: 0);
	r |=	((l &  TOSTOP) ? 0x00000100	: 0);
	r |=	((l & ECHOCTL) ? 0x00000200	: 0);
	r |=	((l & ECHOPRT) ? 0x00000400	: 0);
	r |=	((l &  ECHOKE) ? 0x00000800	: 0);
	/*	((l & DEFECHO) ? 0x00001000	: 0) */
	r |=	((l &  FLUSHO) ? 0x00002000	: 0);
	r |=	((l &  PENDIN) ? 0x00004000	: 0);
	st->c_lflag = r;

	l = ttspeedtab(bt->c_ospeed, sptab);
	if (l >= 0)
		st->c_cflag |= l;

	st->c_cc[0] = bt->c_cc[VINTR]   != _POSIX_VDISABLE? bt->c_cc[VINTR]:0;
	st->c_cc[1] = bt->c_cc[VQUIT]   != _POSIX_VDISABLE? bt->c_cc[VQUIT]:0;
	st->c_cc[2] = bt->c_cc[VERASE]  != _POSIX_VDISABLE? bt->c_cc[VERASE]:0;
	st->c_cc[3] = bt->c_cc[VKILL]   != _POSIX_VDISABLE? bt->c_cc[VKILL]:0;
	st->c_cc[4] = bt->c_cc[VEOF]    != _POSIX_VDISABLE? bt->c_cc[VEOF]:0;
	st->c_cc[5] = bt->c_cc[VEOL]    != _POSIX_VDISABLE? bt->c_cc[VEOL]:0;
	st->c_cc[6] = bt->c_cc[VEOL2]   != _POSIX_VDISABLE? bt->c_cc[VEOL2]:0;
	st->c_cc[7] = 0;
		/*    bt->c_cc[VSWTCH]  != _POSIX_VDISABLE? bt->c_cc[VSWTCH]: */
	st->c_cc[8] = bt->c_cc[VSTART]  != _POSIX_VDISABLE? bt->c_cc[VSTART]:0;
	st->c_cc[9] = bt->c_cc[VSTOP]   != _POSIX_VDISABLE? bt->c_cc[VSTOP]:0;
	st->c_cc[10]= bt->c_cc[VSUSP]   != _POSIX_VDISABLE? bt->c_cc[VSUSP]:0;
	st->c_cc[11]= bt->c_cc[VDSUSP]  != _POSIX_VDISABLE? bt->c_cc[VDSUSP]:0;
	st->c_cc[12]= bt->c_cc[VREPRINT]!= _POSIX_VDISABLE? bt->c_cc[VREPRINT]:0;
	st->c_cc[13]= bt->c_cc[VDISCARD]!= _POSIX_VDISABLE? bt->c_cc[VDISCARD]:0;
	st->c_cc[14]= bt->c_cc[VWERASE] != _POSIX_VDISABLE? bt->c_cc[VWERASE]:0;
	st->c_cc[15]= bt->c_cc[VLNEXT]  != _POSIX_VDISABLE? bt->c_cc[VLNEXT]:0;
	st->c_cc[16]= bt->c_cc[VSTATUS] != _POSIX_VDISABLE? bt->c_cc[VSTATUS]:0;

	st->c_line = 0;
}

static void
stios2stio(ts, t)
	struct sun_termios *ts;
	struct sun_termio *t;
{
	t->c_iflag = ts->c_iflag;
	t->c_oflag = ts->c_oflag;
	t->c_cflag = ts->c_cflag;
	t->c_lflag = ts->c_lflag;
	t->c_line  = ts->c_line;
	bcopy(ts->c_cc, t->c_cc, 8);
}

static void
stio2stios(t, ts)
	struct sun_termio *t;
	struct sun_termios *ts;
{
	ts->c_iflag = t->c_iflag;
	ts->c_oflag = t->c_oflag;
	ts->c_cflag = t->c_cflag;
	ts->c_lflag = t->c_lflag;
	ts->c_line  = t->c_line;
	bcopy(t->c_cc, ts->c_cc, 8); /* don't touch the upper fields! */
}

struct sun_ioctl_args {
	int	fd;
	int	cmd;
	caddr_t	data;
};

int
sun_ioctl(p, uap, retval)
	register struct proc *p;
	register struct sun_ioctl_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	register int (*ctl)();
	int error;

	if ( (unsigned)uap->fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
		return EBADF;

	if ((fp->f_flag & (FREAD|FWRITE)) == 0)
		return EBADF;

	ctl = fp->f_ops->fo_ioctl;

	switch (uap->cmd) {
	case _IOR('t', 0, int):
		uap->cmd = TIOCGETD;
		break;
	case _IOW('t', 1, int):
	    {
		int disc;

		if ((error = copyin(uap->data, (caddr_t)&disc,
		    sizeof disc)) != 0)
			return error;

		/* map SunOS NTTYDISC into our termios discipline */
		if (disc == 2)
			disc = 0;
		/* all other disciplines are not supported by NetBSD */
		if (disc)
			return ENXIO;

		return (*ctl)(fp, TIOCSETD, (caddr_t)&disc, p);
	    }
	case _IOW('t', 101, int):	/* sun SUN_TIOCSSOFTCAR */
	    {
		int x;	/* unused */

		return copyin((caddr_t)&x, uap->data, sizeof x);
	    }
	case _IOR('t', 100, int):	/* sun SUN_TIOCSSOFTCAR */
	    {
		int x = 0;

		return copyout((caddr_t)&x, uap->data, sizeof x);
	    }
	case _IO('t', 36): 		/* sun TIOCCONS, no parameters */
	    {
		int on = 1;
		return (*ctl)(fp, TIOCCONS, (caddr_t)&on, p);
	    }
	case _IOW('t', 37, struct sun_ttysize): 
	    {
		struct winsize ws;
		struct sun_ttysize ss;

		if ((error = (*ctl)(fp, TIOCGWINSZ, (caddr_t)&ws, p)) != 0)
			return (error);

		if ((error = copyin (uap->data, &ss, sizeof (ss))) != 0)
			return error;

		ws.ws_row = ss.ts_row;
		ws.ws_col = ss.ts_col;

		return ((*ctl)(fp, TIOCSWINSZ, (caddr_t)&ws, p));
	    }
	case _IOW('t', 38, struct sun_ttysize): 
	    {
		struct winsize ws;
		struct sun_ttysize ss;

		if ((error = (*ctl)(fp, TIOCGWINSZ, (caddr_t)&ws, p)) != 0)
			return (error);

		ss.ts_row = ws.ws_row;
		ss.ts_col = ws.ws_col;

		return copyout ((caddr_t)&ss, uap->data, sizeof (ss));
	    }
	case _IOR('t', 130, int):
		uap->cmd = TIOCSPGRP;
		break;
	case _IOR('t', 131, int):
		uap->cmd = TIOCGPGRP;
		break;
	case _IO('t', 132):
		uap->cmd = TIOCSCTTY;
		break;
	case SUN_TCGETA:
	case SUN_TCGETS: 
	    {
		struct termios bts;
		struct sun_termios sts;
		struct sun_termio st;
	
		if ((error = (*ctl)(fp, TIOCGETA, (caddr_t)&bts, p)) != 0)
			return error;
	
		btios2stios (&bts, &sts);
		if (uap->cmd == SUN_TCGETA) {
			stios2stio (&sts, &st);
			return copyout((caddr_t)&st, uap->data, sizeof (st));
		} else
			return copyout((caddr_t)&sts, uap->data, sizeof (sts));
		/*NOTREACHED*/
	    }
	case SUN_TCSETA:
	case SUN_TCSETAW:
	case SUN_TCSETAF:
	    {
		struct termios bts;
		struct sun_termios sts;
		struct sun_termio st;

		if ((error = copyin(uap->data, (caddr_t)&st,
		    sizeof (st))) != 0)
			return error;

		/* get full BSD termios so we don't lose information */
		if ((error = (*ctl)(fp, TIOCGETA, (caddr_t)&bts, p)) != 0)
			return error;

		/*
		 * convert to sun termios, copy in information from
		 * termio, and convert back, then set new values.
		 */
		btios2stios(&bts, &sts);
		stio2stios(&st, &sts);
		stios2btios(&sts, &bts);

		return (*ctl)(fp, uap->cmd - SUN_TCSETA + TIOCSETA,
		    (caddr_t)&bts, p);
	    }
	case SUN_TCSETS:
	case SUN_TCSETSW:
	case SUN_TCSETSF:
	    {
		struct termios bts;
		struct sun_termios sts;

		if ((error = copyin (uap->data, (caddr_t)&sts,
		    sizeof (sts))) != 0)
			return error;
		stios2btios (&sts, &bts);
		return (*ctl)(fp, uap->cmd - SUN_TCSETS + TIOCSETA,
		    (caddr_t)&bts, p);
	    }

/*
 * Socket ioctl translations.
 */
#define IFREQ_IN(a) { \
	struct ifreq ifreq; \
	if (error = copyin (uap->data, (caddr_t)&ifreq, sizeof (ifreq))) \
		return error; \
	return (*ctl)(fp, a, (caddr_t)&ifreq, p); \
}
#define IFREQ_INOUT(a) { \
	struct ifreq ifreq; \
	if (error = copyin (uap->data, (caddr_t)&ifreq, sizeof (ifreq))) \
		return error; \
	if (error = (*ctl)(fp, a, (caddr_t)&ifreq, p)) \
		return error; \
	return copyout ((caddr_t)&ifreq, uap->data, sizeof (ifreq)); \
}

	case _IOW('i', 12, struct ifreq):
		/* SIOCSIFADDR */
		break;

	case _IOWR('i', 13, struct ifreq):
		IFREQ_INOUT(OSIOCGIFADDR);

	case _IOW('i', 14, struct ifreq):
		/* SIOCSIFDSTADDR */
		break;

	case _IOWR('i', 15, struct ifreq):
		IFREQ_INOUT(OSIOCGIFDSTADDR);

	case _IOW('i', 16, struct ifreq):
		/* SIOCSIFFLAGS */
		break;

	case _IOWR('i', 17, struct ifreq):
		/* SIOCGIFFLAGS */
		break;

	case _IOW('i', 21, struct ifreq):
		IFREQ_IN(SIOCSIFMTU);

	case _IOWR('i', 22, struct ifreq):
		IFREQ_INOUT(SIOCGIFMTU);

	case _IOWR('i', 23, struct ifreq):
		IFREQ_INOUT(SIOCGIFBRDADDR);

	case _IOW('i', 24, struct ifreq):
		IFREQ_IN(SIOCSIFBRDADDR);

	case _IOWR('i', 25, struct ifreq):
		IFREQ_INOUT(OSIOCGIFNETMASK);

	case _IOW('i', 26, struct ifreq):
		IFREQ_IN(SIOCSIFNETMASK);

	case _IOWR('i', 27, struct ifreq):
		IFREQ_INOUT(SIOCGIFMETRIC);

	case _IOWR('i', 28, struct ifreq):
		IFREQ_IN(SIOCSIFMETRIC);

	case _IOW('i', 30, struct arpreq):
		/* SIOCSARP */
		break;

	case _IOWR('i', 31, struct arpreq):
	    {
		struct arpreq arpreq;

		if (error = copyin (uap->data, (caddr_t)&arpreq, sizeof (arpreq)))
			return error;
		if (error = (*ctl)(fp, OSIOCGARP, (caddr_t)&arpreq, p))
			return error;
		return copyout ((caddr_t)&arpreq, uap->data, sizeof (arpreq));
	    }

	case _IOW('i', 32, struct arpreq):
		/* SIOCDARP */
		break;

	case _IOW('i', 18, struct ifreq):	/* SIOCSIFMEM */
	case _IOWR('i', 19, struct ifreq):	/* SIOCGIFMEM */
	case _IOW('i', 40, struct ifreq):	/* SIOCUPPER */
	case _IOW('i', 41, struct ifreq):	/* SIOCLOWER */
	case _IOW('i', 44, struct ifreq):	/* SIOCSETSYNC */
	case _IOWR('i', 45, struct ifreq):	/* SIOCGETSYNC */
	case _IOWR('i', 46, struct ifreq):	/* SIOCSDSTATS */
	case _IOWR('i', 47, struct ifreq):	/* SIOCSESTATS */
	case _IOW('i', 48, int):		/* SIOCSPROMISC */
	case _IOW('i', 49, struct ifreq):	/* SIOCADDMULTI */
	case _IOW('i', 50, struct ifreq):	/* SIOCDELMULTI */
		return EOPNOTSUPP;

	case _IOWR('i', 20, struct ifconf):	/* SIOCGIFCONF */
	    {
		struct ifconf ifconf;

		/*
		 * XXX: two more problems
		 * 1. our sockaddr's are variable length, not always sizeof(sockaddr)
		 * 2. this returns a name per protocol, ie. it returns two "lo0"'s
		 */
		if (error = copyin (uap->data, (caddr_t)&ifconf, sizeof (ifconf)))
			return error;
		if (error = (*ctl)(fp, OSIOCGIFCONF, (caddr_t)&ifconf, p))
			return error;
		return copyout ((caddr_t)&ifconf, uap->data, sizeof (ifconf));
	    }
	}
	return (ioctl(p, uap, retval));
}
