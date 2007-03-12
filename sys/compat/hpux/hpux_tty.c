/*	$NetBSD: hpux_tty.c,v 1.30.26.1 2007/03/12 05:52:00 rmind Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: hpux_tty.c 1.14 93/08/05$
 *
 *	@(#)hpux_tty.c	8.3 (Berkeley) 1/12/94
 */

/*
 * Copyright (c) 1988 University of Utah.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: hpux_tty.c 1.14 93/08/05$
 *
 *	@(#)hpux_tty.c	8.3 (Berkeley) 1/12/94
 */

/*
 * stty/gtty/termio emulation stuff
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hpux_tty.c,v 1.30.26.1 2007/03/12 05:52:00 rmind Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_43.h"
#endif

#ifndef COMPAT_43
#define COMPAT_43
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/kernel.h>

#include <compat/hpux/hpux.h>
#include <compat/hpux/hpux_termio.h>
#include <compat/hpux/hpux_syscallargs.h>

/*
 * Map BSD/POSIX style termios info to and from SYS5 style termio stuff.
 */
int
hpux_termio(fd, com, data, l)
	int fd, com;
	void *data;
	struct lwp *l;
{
	struct proc *p = l->l_proc;
	struct file *fp;
	struct filedesc *fdp = p->p_fd;
	struct termios tios;
	struct hpux_termios htios;
	int line, error;
	int newi = 0;
	int (*ioctlrout)(struct file *, u_long, void *, struct lwp *);

        if ((fp = fd_getfile(fdp, fd)) == NULL)
                return EBADF;
	ioctlrout = fp->f_ops->fo_ioctl;
	switch (com) {
	case HPUXTCGETATTR:
		newi = 1;
		/* fall into ... */
	case HPUXTCGETA:
		/*
		 * Get BSD terminal state
		 */
		if ((error = (*ioctlrout)(fp, TIOCGETA, (void *)&tios, l)))
			break;
		memset((char *)&htios, 0, sizeof htios);
		/*
		 * Set iflag.
		 * Same through ICRNL, no BSD equivs for IUCLC, IENQAK
		 */
		htios.c_iflag = tios.c_iflag & 0x1ff;
		if (tios.c_iflag & IXON)
			htios.c_iflag |= TIO_IXON;
		if (tios.c_iflag & IXOFF)
			htios.c_iflag |= TIO_IXOFF;
		if (tios.c_iflag & IXANY)
			htios.c_iflag |= TIO_IXANY;
		/*
		 * Set oflag.
		 * No BSD equivs for OLCUC/OCRNL/ONOCR/ONLRET/OFILL/OFDEL
		 * or any of the delays.
		 */
		if (tios.c_oflag & OPOST)
			htios.c_oflag |= TIO_OPOST;
		if (tios.c_oflag & ONLCR)
			htios.c_oflag |= TIO_ONLCR;
		if (tios.c_oflag & OXTABS)
			htios.c_oflag |= TIO_TAB3;
		/*
		 * Set cflag.
		 * Baud from ospeed, rest from cflag.
		 */
		htios.c_cflag = bsdtohpuxbaud(tios.c_ospeed);
		switch (tios.c_cflag & CSIZE) {
		case CS5:
			htios.c_cflag |= TIO_CS5; break;
		case CS6:
			htios.c_cflag |= TIO_CS6; break;
		case CS7:
			htios.c_cflag |= TIO_CS7; break;
		case CS8:
			htios.c_cflag |= TIO_CS8; break;
		}
		if (tios.c_cflag & CSTOPB)
			htios.c_cflag |= TIO_CSTOPB;
		if (tios.c_cflag & CREAD)
			htios.c_cflag |= TIO_CREAD;
		if (tios.c_cflag & PARENB)
			htios.c_cflag |= TIO_PARENB;
		if (tios.c_cflag & PARODD)
			htios.c_cflag |= TIO_PARODD;
		if (tios.c_cflag & HUPCL)
			htios.c_cflag |= TIO_HUPCL;
		if (tios.c_cflag & CLOCAL)
			htios.c_cflag |= TIO_CLOCAL;
		/*
		 * Set lflag.
		 * No BSD equiv for XCASE.
		 */
		if (tios.c_lflag & ECHOE)
			htios.c_lflag |= TIO_ECHOE;
		if (tios.c_lflag & ECHOK)
			htios.c_lflag |= TIO_ECHOK;
		if (tios.c_lflag & ECHO)
			htios.c_lflag |= TIO_ECHO;
		if (tios.c_lflag & ECHONL)
			htios.c_lflag |= TIO_ECHONL;
		if (tios.c_lflag & ISIG)
			htios.c_lflag |= TIO_ISIG;
		if (tios.c_lflag & ICANON)
			htios.c_lflag |= TIO_ICANON;
		if (tios.c_lflag & NOFLSH)
			htios.c_lflag |= TIO_NOFLSH;
		/*
		 * Line discipline
		 */
		if (!newi) {
			line = 0;
			(void) (*ioctlrout)(fp, TIOCGETD, (void *)&line, l);
			htios.c_reserved = line;
		}
		/*
		 * Set editing chars.
		 * No BSD equiv for VSWTCH.
		 */
		htios.c_cc[HPUXVINTR] = tios.c_cc[VINTR];
		htios.c_cc[HPUXVQUIT] = tios.c_cc[VQUIT];
		htios.c_cc[HPUXVERASE] = tios.c_cc[VERASE];
		htios.c_cc[HPUXVKILL] = tios.c_cc[VKILL];
		htios.c_cc[HPUXVEOF] = tios.c_cc[VEOF];
		htios.c_cc[HPUXVEOL] = tios.c_cc[VEOL];
		htios.c_cc[HPUXVEOL2] = tios.c_cc[VEOL2];
		htios.c_cc[HPUXVSWTCH] = 0;
#if 1
		/*
		 * XXX since VMIN and VTIME are not implemented,
		 * we need to return something reasonable.
		 * Otherwise a GETA/SETA combo would always put
		 * the tty in non-blocking mode (since VMIN == VTIME == 0).
		 */
		if (fp->f_flag & FNONBLOCK) {
			htios.c_cc[HPUXVMINS] = 0;
			htios.c_cc[HPUXVTIMES] = 0;
		} else {
			htios.c_cc[HPUXVMINS] = 6;
			htios.c_cc[HPUXVTIMES] = 1;
		}
#else
		htios.c_cc[HPUXVMINS] = tios.c_cc[VMIN];
		htios.c_cc[HPUXVTIMES] = tios.c_cc[VTIME];
#endif
		htios.c_cc[HPUXVSUSP] = tios.c_cc[VSUSP];
		htios.c_cc[HPUXVSTART] = tios.c_cc[VSTART];
		htios.c_cc[HPUXVSTOP] = tios.c_cc[VSTOP];
		if (newi)
			memcpy(data, (char *)&htios, sizeof htios);
		else
			termiostotermio(&htios, (struct hpux_termio *)data);
		break;

	case HPUXTCSETATTR:
	case HPUXTCSETATTRD:
	case HPUXTCSETATTRF:
		newi = 1;
		/* fall into ... */
	case HPUXTCSETA:
	case HPUXTCSETAW:
	case HPUXTCSETAF:
		/*
		 * Get old characteristics and determine if we are a tty.
		 */
		if ((error = (*ioctlrout)(fp, TIOCGETA, (void *)&tios, l)))
			break;
		if (newi)
			memcpy((char *)&htios, data, sizeof htios);
		else
			termiototermios((struct hpux_termio *)data,
			    &htios, &tios);
		/*
		 * Set iflag.
		 * Same through ICRNL, no HP-UX equiv for IMAXBEL
		 */
		tios.c_iflag &= ~(IXON|IXOFF|IXANY|0x1ff);
		tios.c_iflag |= htios.c_iflag & 0x1ff;
		if (htios.c_iflag & TIO_IXON)
			tios.c_iflag |= IXON;
		if (htios.c_iflag & TIO_IXOFF)
			tios.c_iflag |= IXOFF;
		if (htios.c_iflag & TIO_IXANY)
			tios.c_iflag |= IXANY;
		/*
		 * Set oflag.
		 * No HP-UX equiv for ONOEOT
		 */
		tios.c_oflag &= ~(OPOST|ONLCR|OXTABS);
		if (htios.c_oflag & TIO_OPOST)
			tios.c_oflag |= OPOST;
		if (htios.c_oflag & TIO_ONLCR)
			tios.c_oflag |= ONLCR;
		if (htios.c_oflag & TIO_TAB3)
			tios.c_oflag |= OXTABS;
		/*
		 * Set cflag.
		 * No HP-UX equiv for CCTS_OFLOW/CCTS_IFLOW/MDMBUF
		 */
		tios.c_cflag &=
			~(CSIZE|CSTOPB|CREAD|PARENB|PARODD|HUPCL|CLOCAL);
		switch (htios.c_cflag & TIO_CSIZE) {
		case TIO_CS5:
			tios.c_cflag |= CS5; break;
		case TIO_CS6:
			tios.c_cflag |= CS6; break;
		case TIO_CS7:
			tios.c_cflag |= CS7; break;
		case TIO_CS8:
			tios.c_cflag |= CS8; break;
		}
		if (htios.c_cflag & TIO_CSTOPB)
			tios.c_cflag |= CSTOPB;
		if (htios.c_cflag & TIO_CREAD)
			tios.c_cflag |= CREAD;
		if (htios.c_cflag & TIO_PARENB)
			tios.c_cflag |= PARENB;
		if (htios.c_cflag & TIO_PARODD)
			tios.c_cflag |= PARODD;
		if (htios.c_cflag & TIO_HUPCL)
			tios.c_cflag |= HUPCL;
		if (htios.c_cflag & TIO_CLOCAL)
			tios.c_cflag |= CLOCAL;
		/*
		 * Set lflag.
		 * No HP-UX equiv for ECHOKE/ECHOPRT/ECHOCTL
		 * IEXTEN treated as part of ICANON
		 */
		tios.c_lflag &= ~(ECHOE|ECHOK|ECHO|ISIG|ICANON|IEXTEN|NOFLSH);
		if (htios.c_lflag & TIO_ECHOE)
			tios.c_lflag |= ECHOE;
		if (htios.c_lflag & TIO_ECHOK)
			tios.c_lflag |= ECHOK;
		if (htios.c_lflag & TIO_ECHO)
			tios.c_lflag |= ECHO;
		if (htios.c_lflag & TIO_ECHONL)
			tios.c_lflag |= ECHONL;
		if (htios.c_lflag & TIO_ISIG)
			tios.c_lflag |= ISIG;
		if (htios.c_lflag & TIO_ICANON)
			tios.c_lflag |= (ICANON|IEXTEN);
		if (htios.c_lflag & TIO_NOFLSH)
			tios.c_lflag |= NOFLSH;
		/*
		 * Set editing chars.
		 * No HP-UX equivs of VWERASE/VREPRINT/VDSUSP/VLNEXT
		 * /VDISCARD/VSTATUS/VERASE2
		 */
		tios.c_cc[VINTR] = htios.c_cc[HPUXVINTR];
		tios.c_cc[VQUIT] = htios.c_cc[HPUXVQUIT];
		tios.c_cc[VERASE] = htios.c_cc[HPUXVERASE];
		tios.c_cc[VKILL] = htios.c_cc[HPUXVKILL];
		tios.c_cc[VEOF] = htios.c_cc[HPUXVEOF];
		tios.c_cc[VEOL] = htios.c_cc[HPUXVEOL];
		tios.c_cc[VEOL2] = htios.c_cc[HPUXVEOL2];
		tios.c_cc[VMIN] = htios.c_cc[HPUXVMINS];
		tios.c_cc[VTIME] = htios.c_cc[HPUXVTIMES];
		tios.c_cc[VSUSP] = htios.c_cc[HPUXVSUSP];
		tios.c_cc[VSTART] = htios.c_cc[HPUXVSTART];
		tios.c_cc[VSTOP] = htios.c_cc[HPUXVSTOP];

		/*
		 * Set the new stuff
		 */
		if (com == HPUXTCSETA || com == HPUXTCSETATTR)
			com = TIOCSETA;
		else if (com == HPUXTCSETAW || com == HPUXTCSETATTRD)
			com = TIOCSETAW;
		else
			com = TIOCSETAF;
		error = (*ioctlrout)(fp, com, (void *)&tios, l);
		if (error == 0) {
			/*
			 * Set line discipline
			 */
			if (!newi) {
				line = htios.c_reserved;
				(void) (*ioctlrout)(fp, TIOCSETD,
						    (void *)&line, l);
			}
			/*
			 * Set non-blocking IO if VMIN == VTIME == 0, clear
			 * if not.  Should handle the other cases as well.
			 * Note it isn't correct to just turn NBIO off like
			 * we do as it could be on as the result of a fcntl
			 * operation.
			 *
			 * XXX - wouldn't need to do this at all if VMIN/VTIME
			 * were implemented.
			 */
			{
				struct hpux_sys_fcntl_args {
					int fdes, cmd, arg;
				} args;
				int flags, nbio;

				nbio = (htios.c_cc[HPUXVMINS] == 0 &&
					htios.c_cc[HPUXVTIMES] == 0);
				if ((nbio && (fp->f_flag & FNONBLOCK) == 0) ||
				    (!nbio && (fp->f_flag & FNONBLOCK))) {
					args.fdes = fd;
					args.cmd = F_GETFL;
					args.arg = 0;
					(void) hpux_sys_fcntl(l, &args, &flags);
					if (nbio)
						flags |= HPUXNDELAY;
					else
						flags &= ~HPUXNDELAY;
					args.cmd = F_SETFL;
					args.arg = flags;
					(void) hpux_sys_fcntl(l, &args, &flags);
				}
			}
		}
		break;

	default:
		error = EINVAL;
		break;
	}
	return(error);
}

void
termiototermios(tio, tios, bsdtios)
	struct hpux_termio *tio;
	struct hpux_termios *tios;
	struct termios *bsdtios;
{
	int i;

	memset((char *)tios, 0, sizeof *tios);
	tios->c_iflag = tio->c_iflag;
	tios->c_oflag = tio->c_oflag;
	tios->c_cflag = tio->c_cflag;
	tios->c_lflag = tio->c_lflag;
	tios->c_reserved = tio->c_line;
	for (i = 0; i <= HPUXVSWTCH; i++)
		tios->c_cc[i] = tio->c_cc[i];
	if (tios->c_lflag & TIO_ICANON) {
		tios->c_cc[HPUXVEOF] = tio->c_cc[HPUXVEOF];
		tios->c_cc[HPUXVEOL] = tio->c_cc[HPUXVEOL];
		tios->c_cc[HPUXVMINS] = 0;
		tios->c_cc[HPUXVTIMES] = 0;
	} else {
		tios->c_cc[HPUXVEOF] = 0;
		tios->c_cc[HPUXVEOL] = 0;
		tios->c_cc[HPUXVMINS] = tio->c_cc[HPUXVMIN];
		tios->c_cc[HPUXVTIMES] = tio->c_cc[HPUXVTIME];
	}
	tios->c_cc[HPUXVSUSP] = bsdtios->c_cc[VSUSP];
	tios->c_cc[HPUXVSTART] = bsdtios->c_cc[VSTART];
	tios->c_cc[HPUXVSTOP] = bsdtios->c_cc[VSTOP];
}

void
termiostotermio(tios, tio)
	struct hpux_termios *tios;
	struct hpux_termio *tio;
{
	int i;

	tio->c_iflag = tios->c_iflag;
	tio->c_oflag = tios->c_oflag;
	tio->c_cflag = tios->c_cflag;
	tio->c_lflag = tios->c_lflag;
	tio->c_line = tios->c_reserved;
	for (i = 0; i <= HPUXVSWTCH; i++)
		tio->c_cc[i] = tios->c_cc[i];
	if (tios->c_lflag & TIO_ICANON) {
		tio->c_cc[HPUXVEOF] = tios->c_cc[HPUXVEOF];
		tio->c_cc[HPUXVEOL] = tios->c_cc[HPUXVEOL];
	} else {
		tio->c_cc[HPUXVMIN] = tios->c_cc[HPUXVMINS];
		tio->c_cc[HPUXVTIME] = tios->c_cc[HPUXVTIMES];
	}
}

int
bsdtohpuxbaud(bsdspeed)
	long bsdspeed;
{
	switch (bsdspeed) {
	case B0:     return(TIO_B0);
	case B50:    return(TIO_B50);
	case B75:    return(TIO_B75);
	case B110:   return(TIO_B110);
	case B134:   return(TIO_B134);
	case B150:   return(TIO_B150);
	case B200:   return(TIO_B200);
	case B300:   return(TIO_B300);
	case B600:   return(TIO_B600);
	case B1200:  return(TIO_B1200);
	case B1800:  return(TIO_B1800);
	case B2400:  return(TIO_B2400);
	case B4800:  return(TIO_B4800);
	case B9600:  return(TIO_B9600);
	case B19200: return(TIO_B19200);
	case B38400: return(TIO_B38400);
	default:     return(TIO_B0);
	}
}

int
hpuxtobsdbaud(hpux_speed)
	int hpux_speed;
{
	static const int hpuxtobsdbaudtab[32] = {
		B0,	B50,	B75,	B110,	B134,	B150,	B200,	B300,
		B600,	B0,	B1200,	B1800,	B2400,	B0,	B4800,	B0,
		B9600,	B19200,	B38400,	B0,	B0,	B0,	B0,	B0,
		B0,	B0,	B0,	B0,	B0,	B0,	EXTA,	EXTB
	};

	if (hpux_speed < 0 || hpux_speed > 31)
		return(B0);
	return(hpuxtobsdbaudtab[hpux_speed & TIO_CBAUD]);
}

int
hpux_sys_stty_6x(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct hpux_sys_stty_6x_args /* {
		syscallarg(int) fd;
		syscallarg(void *) arg;
	} */ *uap = v;

	return (getsettty(l, SCARG(uap, fd), HPUXTIOCGETP, SCARG(uap, arg)));
}

int
hpux_sys_gtty_6x(l, v, retval)
	struct lwp *l;
	void *v;
	register_t *retval;
{
	struct hpux_sys_gtty_6x_args /* {
		syscallarg(int) fd;
		syscallarg(void *) arg;
	} */ *uap = v;

	return (getsettty(l, SCARG(uap, fd), HPUXTIOCSETP, SCARG(uap, arg)));
}

/*
 * Simplified version of ioctl() for use by
 * gtty/stty and TIOCGETP/TIOCSETP.
 */
int
getsettty(l, fdes, com, cmarg)
	struct lwp *l;
	int fdes, com;
	void *cmarg;
{
	struct proc *p = l->l_proc;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct hpux_sgttyb hsb;
	struct sgttyb sb;
	int error;

	if ((fp = fd_getfile(fdp, fdes)) == NULL)
		return (EBADF);

	if ((fp->f_flag & (FREAD|FWRITE)) == 0)
		return (EBADF);

	if (com == HPUXTIOCSETP) {
		if ((error = copyin(cmarg, (void *)&hsb, sizeof hsb)))
			return (error);
		sb.sg_ispeed = hsb.sg_ispeed;
		sb.sg_ospeed = hsb.sg_ospeed;
		sb.sg_erase = hsb.sg_erase;
		sb.sg_kill = hsb.sg_kill;
		sb.sg_flags = hsb.sg_flags & ~(V7_HUPCL|V7_XTABS|V7_NOAL);
		if (hsb.sg_flags & V7_XTABS)
			sb.sg_flags |= XTABS;
		if (hsb.sg_flags & V7_HUPCL)
			(void)(*fp->f_ops->fo_ioctl)
				(fp, TIOCHPCL, (void *)0, l);
		com = TIOCSETP;
	} else {
		memset((void *)&hsb, 0, sizeof hsb);
		com = TIOCGETP;
	}
	error = (*fp->f_ops->fo_ioctl)(fp, com, (void *)&sb, l);
	if (error == 0 && com == TIOCGETP) {
		hsb.sg_ispeed = sb.sg_ispeed;
		hsb.sg_ospeed = sb.sg_ospeed;
		hsb.sg_erase = sb.sg_erase;
		hsb.sg_kill = sb.sg_kill;
		hsb.sg_flags = sb.sg_flags & ~(V7_HUPCL|V7_XTABS|V7_NOAL);
		if (sb.sg_flags & XTABS)
			hsb.sg_flags |= V7_XTABS;
		error = copyout((void *)&hsb, cmarg, sizeof hsb);
	}
	return (error);
}
