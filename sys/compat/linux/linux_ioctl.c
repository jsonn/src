/*	$NetBSD: linux_ioctl.c,v 1.8.2.1 1995/11/16 18:45:18 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
 * All rights reserved.
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/tty.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <net/if.h>
#include <sys/sockio.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_sockio.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>

static speed_t linux_speeds[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
	9600, 19200, 38400, 57600, 115200
};

static int linux_spmasks[] = {
	LINUX_B0, LINUX_B50, LINUX_B75, LINUX_B110, LINUX_B134, LINUX_B150,
	LINUX_B200, LINUX_B300, LINUX_B600, LINUX_B1200, LINUX_B1800,
	LINUX_B2400, LINUX_B4800, LINUX_B9600, LINUX_B19200, LINUX_B38400,
	LINUX_B57600, LINUX_B115200, LINUX_B230400
};

/*
 * Deal with termio ioctl cruft. This doesn't look very good..
 * XXX too much code duplication, obviously..
 *
 * The conversion routines between Linux and BSD structures assume
 * that the fields are already filled with the current values,
 * so that fields present in BSD but not in Linux keep their current
 * values.
 */

static int
linux_termio_to_bsd_termios(lt, bts)
	register struct linux_termio *lt;
	register struct termios *bts;
{
	int index;

	bts->c_iflag = 0;
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_IGNBRK, IGNBRK);
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_BRKINT, BRKINT);
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_IGNPAR, IGNPAR);
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_INPCK, INPCK);
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_ISTRIP, ISTRIP);
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_INLCR, INLCR);
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_IGNCR, IGNCR);
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_ICRNL, ICRNL);
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_IXON, IXON);
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_IXANY, IXANY);
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_IXOFF, IXOFF);
	bts->c_iflag |= cvtto_bsd_mask(lt->c_iflag, LINUX_IMAXBEL, IMAXBEL);

	bts->c_oflag = 0;
	bts->c_oflag |= cvtto_bsd_mask(lt->c_oflag, LINUX_OPOST, OPOST);
	bts->c_oflag |= cvtto_bsd_mask(lt->c_oflag, LINUX_ONLCR, ONLCR);
	bts->c_oflag |= cvtto_bsd_mask(lt->c_oflag, LINUX_XTABS, OXTABS);

	/*
	 * This could have been:
	 * bts->c_cflag = (lt->c_flag & LINUX_CSIZE) << 4
	 * But who knows, those values might perhaps change one day.
	 */
	switch (lt->c_cflag & LINUX_CSIZE) {
	case LINUX_CS5:
		bts->c_cflag = CS5;
		break;
	case LINUX_CS6:
		bts->c_cflag = CS6;
		break;
	case LINUX_CS7:
		bts->c_cflag = CS7;
		break;
	case LINUX_CS8:
		bts->c_cflag = CS8;
		break;
	}
	bts->c_cflag |= cvtto_bsd_mask(lt->c_cflag, LINUX_CSTOPB, CSTOPB);
	bts->c_cflag |= cvtto_bsd_mask(lt->c_cflag, LINUX_CREAD, CREAD);
	bts->c_cflag |= cvtto_bsd_mask(lt->c_cflag, LINUX_PARENB, PARENB);
	bts->c_cflag |= cvtto_bsd_mask(lt->c_cflag, LINUX_PARODD, PARODD);
	bts->c_cflag |= cvtto_bsd_mask(lt->c_cflag, LINUX_HUPCL, HUPCL);
	bts->c_cflag |= cvtto_bsd_mask(lt->c_cflag, LINUX_CLOCAL, CLOCAL);
	bts->c_cflag |= cvtto_bsd_mask(lt->c_cflag, LINUX_CRTSCTS, CRTSCTS);

	bts->c_lflag = 0;
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_ISIG, ISIG);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_ICANON, ICANON);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_ECHO, ECHO);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_ECHOE, ECHOE);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_ECHOK, ECHOK);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_ECHONL, ECHONL);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_NOFLSH, NOFLSH);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_TOSTOP, TOSTOP);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_ECHOCTL, ECHOCTL);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_ECHOPRT, ECHOPRT);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_ECHOKE, ECHOKE);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_FLUSHO, FLUSHO);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_PENDIN, PENDIN);
	bts->c_lflag |= cvtto_bsd_mask(lt->c_lflag, LINUX_IEXTEN, IEXTEN);

	index = lt->c_cflag & LINUX_CBAUD;
	if (index & LINUX_CBAUDEX)
		index = (index & ~LINUX_CBAUDEX) + LINUX_NSPEEDS - 1;
	bts->c_ispeed = bts->c_ospeed = linux_speeds[index];

	bts->c_cc[VINTR] = lt->c_cc[LINUX_VINTR];
	bts->c_cc[VQUIT] = lt->c_cc[LINUX_VQUIT];
	bts->c_cc[VERASE] = lt->c_cc[LINUX_VERASE];
	bts->c_cc[VKILL] = lt->c_cc[LINUX_VKILL];
	bts->c_cc[VEOF] = lt->c_cc[LINUX_VEOF];
	bts->c_cc[VTIME] = lt->c_cc[LINUX_VTIME];
	bts->c_cc[VMIN] = lt->c_cc[LINUX_VMIN];
}

static int
bsd_termios_to_linux_termio(bts, lt)
	register struct termios *bts;
	register struct linux_termio *lt;
{
	int i, mask;

	lt->c_iflag = 0;
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, IGNBRK, LINUX_IGNBRK);
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, BRKINT, LINUX_BRKINT);
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, IGNPAR, LINUX_IGNPAR);
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, INPCK, LINUX_INPCK);
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, ISTRIP, LINUX_ISTRIP);
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, INLCR, LINUX_INLCR);
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, IGNCR, LINUX_IGNCR);
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, ICRNL, LINUX_ICRNL);
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, IXON, LINUX_IXON);
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, IXANY, LINUX_IXANY);
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, IXOFF, LINUX_IXOFF);
	lt->c_iflag |= cvtto_linux_mask(bts->c_iflag, IMAXBEL, LINUX_IMAXBEL);

	lt->c_oflag = 0;
	lt->c_oflag |= cvtto_linux_mask(bts->c_oflag, OPOST, LINUX_OPOST);
	lt->c_oflag |= cvtto_linux_mask(bts->c_oflag, ONLCR, LINUX_ONLCR);
	lt->c_oflag |= cvtto_linux_mask(bts->c_oflag, OXTABS, LINUX_XTABS);

	switch (bts->c_cflag & CSIZE) {
	case CS5:
		lt->c_cflag = LINUX_CS5;
		break;
	case CS6:
		lt->c_cflag = LINUX_CS6;
		break;
	case CS7:
		lt->c_cflag = LINUX_CS7;
		break;
	case CS8:
		lt->c_cflag = LINUX_CS8;
		break;
	}
	lt->c_cflag |= cvtto_linux_mask(bts->c_cflag, CSTOPB, LINUX_CSTOPB);
	lt->c_cflag |= cvtto_linux_mask(bts->c_cflag, CREAD, LINUX_CREAD);
	lt->c_cflag |= cvtto_linux_mask(bts->c_cflag, PARENB, LINUX_PARENB);
	lt->c_cflag |= cvtto_linux_mask(bts->c_cflag, PARODD, LINUX_PARODD);
	lt->c_cflag |= cvtto_linux_mask(bts->c_cflag, HUPCL, LINUX_HUPCL);
	lt->c_cflag |= cvtto_linux_mask(bts->c_cflag, CLOCAL, LINUX_CLOCAL);
	lt->c_cflag |= cvtto_linux_mask(bts->c_cflag, CRTSCTS, LINUX_CRTSCTS);

	lt->c_lflag = 0;
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, ISIG, LINUX_ISIG);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, ICANON, LINUX_ICANON);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHO, LINUX_ECHO);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHOE, LINUX_ECHOE);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHOK, LINUX_ECHOK);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHONL, LINUX_ECHONL);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, NOFLSH, LINUX_NOFLSH);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, TOSTOP, LINUX_TOSTOP);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHOCTL, LINUX_ECHOCTL);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHOPRT, LINUX_ECHOPRT);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHOKE, LINUX_ECHOKE);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, FLUSHO, LINUX_FLUSHO);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, PENDIN, LINUX_PENDIN);
	lt->c_lflag |= cvtto_linux_mask(bts->c_lflag, IEXTEN, LINUX_IEXTEN);

	mask = LINUX_B9600;	/* XXX default value should this be 0? */
	for (i = 0; i < sizeof (linux_speeds) / sizeof (speed_t); i++) {
		if (bts->c_ospeed == linux_speeds[i]) {
			mask = linux_spmasks[i];
			break;
		}
	}
	lt->c_cflag |= mask;

	lt->c_cc[LINUX_VINTR] = bts->c_cc[VINTR];
	lt->c_cc[LINUX_VQUIT] = bts->c_cc[VQUIT];
	lt->c_cc[LINUX_VERASE] = bts->c_cc[VERASE];
	lt->c_cc[LINUX_VKILL] = bts->c_cc[VKILL];
	lt->c_cc[LINUX_VEOF] = bts->c_cc[VEOF];
	lt->c_cc[LINUX_VTIME] = bts->c_cc[VTIME];
	lt->c_cc[LINUX_VMIN] = bts->c_cc[VMIN];
	lt->c_cc[LINUX_VSWTC] = 0;

	/* XXX should be fixed someday */
	lt->c_line = 0;
}

static int
linux_termios_to_bsd_termios(lts, bts)
	register struct linux_termios *lts;
	register struct termios *bts;
{
	int index;

	bts->c_iflag = 0;
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_IGNBRK, IGNBRK);
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_BRKINT, BRKINT);
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_IGNPAR, IGNPAR);
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_INPCK, INPCK);
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_ISTRIP, ISTRIP);
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_INLCR, INLCR);
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_IGNCR, IGNCR);
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_ICRNL, ICRNL);
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_IXON, IXON);
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_IXANY, IXANY);
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_IXOFF, IXOFF);
	bts->c_iflag |= cvtto_bsd_mask(lts->c_iflag, LINUX_IMAXBEL, IMAXBEL);

	bts->c_oflag = 0;
	bts->c_oflag |= cvtto_bsd_mask(lts->c_oflag, LINUX_OPOST, OPOST);
	bts->c_oflag |= cvtto_bsd_mask(lts->c_oflag, LINUX_ONLCR, ONLCR);
	bts->c_oflag |= cvtto_bsd_mask(lts->c_oflag, LINUX_XTABS, OXTABS);

	bts->c_cflag = 0;
	switch (lts->c_cflag & LINUX_CSIZE) {
	case LINUX_CS5:
		bts->c_cflag = CS5;
		break;
	case LINUX_CS6:
		bts->c_cflag = CS6;
		break;
	case LINUX_CS7:
		bts->c_cflag = CS7;
		break;
	case LINUX_CS8:
		bts->c_cflag = CS8;
		break;
	}
	bts->c_cflag |= cvtto_bsd_mask(lts->c_cflag, LINUX_CSTOPB, CSTOPB);
	bts->c_cflag |= cvtto_bsd_mask(lts->c_cflag, LINUX_CREAD, CREAD);
	bts->c_cflag |= cvtto_bsd_mask(lts->c_cflag, LINUX_PARENB, PARENB);
	bts->c_cflag |= cvtto_bsd_mask(lts->c_cflag, LINUX_PARODD, PARODD);
	bts->c_cflag |= cvtto_bsd_mask(lts->c_cflag, LINUX_HUPCL, HUPCL);
	bts->c_cflag |= cvtto_bsd_mask(lts->c_cflag, LINUX_CLOCAL, CLOCAL);
	bts->c_cflag |= cvtto_bsd_mask(lts->c_cflag, LINUX_CRTSCTS, CRTSCTS);

	bts->c_lflag = 0;
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_ISIG, ISIG);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_ICANON, ICANON);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_ECHO, ECHO);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_ECHOE, ECHOE);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_ECHOK, ECHOK);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_ECHONL, ECHONL);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_NOFLSH, NOFLSH);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_TOSTOP, TOSTOP);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_ECHOCTL, ECHOCTL);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_ECHOPRT, ECHOPRT);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_ECHOKE, ECHOKE);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_FLUSHO, FLUSHO);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_PENDIN, PENDIN);
	bts->c_lflag |= cvtto_bsd_mask(lts->c_lflag, LINUX_IEXTEN, IEXTEN);

	index = lts->c_cflag & LINUX_CBAUD;
	if (index & LINUX_CBAUDEX)
		index = (index & ~LINUX_CBAUDEX) + LINUX_NSPEEDS - 1;
	bts->c_ispeed = bts->c_ospeed = linux_speeds[index];

	bts->c_cc[VINTR] = lts->c_cc[LINUX_VINTR];
	bts->c_cc[VQUIT] = lts->c_cc[LINUX_VQUIT];
	bts->c_cc[VERASE] = lts->c_cc[LINUX_VERASE];
	bts->c_cc[VKILL] = lts->c_cc[LINUX_VKILL];
	bts->c_cc[VEOF] = lts->c_cc[LINUX_VEOF];
	bts->c_cc[VTIME] = lts->c_cc[LINUX_VTIME];
	bts->c_cc[VMIN] = lts->c_cc[LINUX_VMIN];
	bts->c_cc[VEOL] = lts->c_cc[LINUX_VEOL];
	bts->c_cc[VEOL2] = lts->c_cc[LINUX_VEOL2];
	bts->c_cc[VWERASE] = lts->c_cc[LINUX_VWERASE];
	bts->c_cc[VSUSP] = lts->c_cc[LINUX_VSUSP];
	bts->c_cc[VSTART] = lts->c_cc[LINUX_VSTART];
	bts->c_cc[VSTOP] = lts->c_cc[LINUX_VSTOP];
	bts->c_cc[VLNEXT] = lts->c_cc[LINUX_VLNEXT];
	bts->c_cc[VDISCARD] = lts->c_cc[LINUX_VDISCARD];
	bts->c_cc[VREPRINT] = lts->c_cc[LINUX_VREPRINT];
}

static int
bsd_termios_to_linux_termios(bts, lts)
	register struct termios *bts;
	register struct linux_termios *lts;
{
	int i, mask;

	lts->c_iflag = 0;
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, IGNBRK, LINUX_IGNBRK);
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, BRKINT, LINUX_BRKINT);
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, IGNPAR, LINUX_IGNPAR);
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, INPCK, LINUX_INPCK);
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, ISTRIP, LINUX_ISTRIP);
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, INLCR, LINUX_INLCR);
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, IGNCR, LINUX_IGNCR);
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, ICRNL, LINUX_ICRNL);
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, IXON, LINUX_IXON);
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, IXANY, LINUX_IXANY);
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, IXOFF, LINUX_IXOFF);
	lts->c_iflag |= cvtto_linux_mask(bts->c_iflag, IMAXBEL, LINUX_IMAXBEL);

	lts->c_oflag = 0;
	lts->c_oflag |= cvtto_linux_mask(bts->c_oflag, OPOST, LINUX_OPOST);
	lts->c_oflag |= cvtto_linux_mask(bts->c_oflag, ONLCR, LINUX_ONLCR);
	lts->c_oflag |= cvtto_linux_mask(bts->c_oflag, OXTABS, LINUX_XTABS);

	switch (bts->c_cflag & CSIZE) {
	case CS5:
		lts->c_cflag = LINUX_CS5;
		break;
	case CS6:
		lts->c_cflag = LINUX_CS6;
		break;
	case CS7:
		lts->c_cflag = LINUX_CS7;
		break;
	case CS8:
		lts->c_cflag = LINUX_CS8;
		break;
	}
	lts->c_cflag |= cvtto_linux_mask(bts->c_cflag, CS5, LINUX_CS5);
	lts->c_cflag |= cvtto_linux_mask(bts->c_cflag, CS6, LINUX_CS6);
	lts->c_cflag |= cvtto_linux_mask(bts->c_cflag, CS7, LINUX_CS7);
	lts->c_cflag |= cvtto_linux_mask(bts->c_cflag, CS8, LINUX_CS8);
	lts->c_cflag |= cvtto_linux_mask(bts->c_cflag, CSTOPB, LINUX_CSTOPB);
	lts->c_cflag |= cvtto_linux_mask(bts->c_cflag, CREAD, LINUX_CREAD);
	lts->c_cflag |= cvtto_linux_mask(bts->c_cflag, PARENB, LINUX_PARENB);
	lts->c_cflag |= cvtto_linux_mask(bts->c_cflag, PARODD, LINUX_PARODD);
	lts->c_cflag |= cvtto_linux_mask(bts->c_cflag, HUPCL, LINUX_HUPCL);
	lts->c_cflag |= cvtto_linux_mask(bts->c_cflag, CLOCAL, LINUX_CLOCAL);
	lts->c_cflag |= cvtto_linux_mask(bts->c_cflag, CRTSCTS, LINUX_CRTSCTS);

	lts->c_lflag = 0;
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, ISIG, LINUX_ISIG);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, ICANON, LINUX_ICANON);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHO, LINUX_ECHO);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHOE, LINUX_ECHOE);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHOK, LINUX_ECHOK);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHONL, LINUX_ECHONL);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, NOFLSH, LINUX_NOFLSH);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, TOSTOP, LINUX_TOSTOP);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHOCTL, LINUX_ECHOCTL);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHOPRT, LINUX_ECHOPRT);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, ECHOKE, LINUX_ECHOKE);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, FLUSHO, LINUX_FLUSHO);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, PENDIN, LINUX_PENDIN);
	lts->c_lflag |= cvtto_linux_mask(bts->c_lflag, IEXTEN, LINUX_IEXTEN);

	mask = LINUX_B9600;	/* XXX default value */
	for (i = 0; i < sizeof (linux_speeds) / sizeof (speed_t); i++) {
		if (bts->c_ospeed == linux_speeds[i]) {
			mask = linux_spmasks[i];
			break;
		}
	}
	lts->c_cflag |= mask;

	lts->c_cc[LINUX_VINTR] = bts->c_cc[VINTR];
	lts->c_cc[LINUX_VQUIT] = bts->c_cc[VQUIT];
	lts->c_cc[LINUX_VERASE] = bts->c_cc[VERASE];
	lts->c_cc[LINUX_VKILL] = bts->c_cc[VKILL];
	lts->c_cc[LINUX_VEOF] = bts->c_cc[VEOF];
	lts->c_cc[LINUX_VTIME] = bts->c_cc[VTIME];
	lts->c_cc[LINUX_VMIN] = bts->c_cc[VMIN];
	lts->c_cc[LINUX_VEOL] = bts->c_cc[VEOL];
	lts->c_cc[LINUX_VEOL2] = bts->c_cc[VEOL2];
	lts->c_cc[LINUX_VWERASE] = bts->c_cc[VWERASE];
	lts->c_cc[LINUX_VSUSP] = bts->c_cc[VSUSP];
	lts->c_cc[LINUX_VSTART] = bts->c_cc[VSTART];
	lts->c_cc[LINUX_VSTOP] = bts->c_cc[VSTOP];
	lts->c_cc[LINUX_VLNEXT] = bts->c_cc[VLNEXT];
	lts->c_cc[LINUX_VDISCARD] = bts->c_cc[VDISCARD];
	lts->c_cc[LINUX_VREPRINT] = bts->c_cc[VREPRINT];
	lts->c_cc[LINUX_VSWTC] = 0;

	/* XXX should be fixed someday */
	lts->c_line = 0;
}

/*
 * Most ioctl command are just converted to their NetBSD values,
 * and passed on. The ones that take structure pointers and (flag)
 * values need some massaging. This is done the usual way by
 * allocating stackgap memory, letting the actual ioctl call do its
 * work their and converting back the data afterwards.
 */
int
linux_sys_ioctl(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct linux_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	int fd;
	unsigned long com;
	caddr_t data, sg;
	struct file *fp;
	struct filedesc *fdp;
	struct linux_termio tmplt, *alt;
	struct linux_termios tmplts, *alts;
	struct termios tmpbts, *abts;
	struct sys_ioctl_args ia;
	int error, idat, *idatp;

	fd = SCARG(&ia, fd) = SCARG(uap, fd);
	com = SCARG(uap, com);
	data = SCARG(&ia, data) = SCARG(uap, data);
	retval[0] = 0;

	fdp = p->p_fd;
	if ((u_int)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);

	if ((fp->f_flag & (FREAD | FWRITE)) == 0)
		return (EBADF);

	sg = stackgap_init(p->p_emul);

	switch (com) {
	case LINUX_TCGETS:
		SCARG(&ia, com) = TIOCGETA;
		abts = stackgap_alloc(&sg, sizeof (*abts));
		SCARG(&ia, data) = (caddr_t) abts;
		if ((error = sys_ioctl(p, &ia, retval)) != 0)
			return error;
		if ((error = copyin(abts, &tmpbts, sizeof tmpbts)))
			return error;
		bsd_termios_to_linux_termios(&tmpbts, &tmplts);
		return copyout(&tmplts, data, sizeof tmplts);
	case LINUX_TCSETS:
	case LINUX_TCSETSW:
	case LINUX_TCSETSF:
		switch (com) {
		case LINUX_TCSETS:
			SCARG(&ia, com) = TIOCSETA;
			break;
		case LINUX_TCSETSW:
			SCARG(&ia, com) = TIOCSETAW;
			break;
		case LINUX_TCSETSF:
			SCARG(&ia, com) = TIOCSETAF;
			break;
		}
		if ((error = copyin(data, &tmplts, sizeof tmplts)))
			return error;
		abts = stackgap_alloc(&sg, sizeof tmpbts);
		/*
		 * First fill in all fields, so that we keep the current
		 * values for fields that Linux doesn't know about.
		 */
		if ((error = (*fp->f_ops->fo_ioctl)(fp, TIOCGETA,
		    (caddr_t) &tmpbts, p)))
			return error;
		linux_termios_to_bsd_termios(&tmplts, &tmpbts);
		if ((error = copyout(&tmpbts, abts, sizeof tmpbts)))
			return error;
		SCARG(&ia, data) = (caddr_t) abts;
		return sys_ioctl(p, &ia, retval);
	case LINUX_TCGETA:
		SCARG(&ia, com) = TIOCGETA;
		abts = stackgap_alloc(&sg, sizeof (*abts));
		SCARG(&ia, data) = (caddr_t) abts;
		if ((error = sys_ioctl(p, &ia, retval)) != 0)
			return error;
		if ((error = copyin(abts, &tmpbts, sizeof tmpbts)))
			return error;
		bsd_termios_to_linux_termio(&tmpbts, &tmplt);
		return copyout(&tmplt, data, sizeof tmplt);
	case LINUX_TCSETA:
	case LINUX_TCSETAW:
	case LINUX_TCSETAF:
		switch (com) {
		case LINUX_TCSETA:
			SCARG(&ia, com) = TIOCSETA;
			break;
		case LINUX_TCSETAW:
			SCARG(&ia, com) = TIOCSETAW;
			break;
		case LINUX_TCSETAF:
			SCARG(&ia, com) = TIOCSETAF;
			break;
		}
		if ((error = copyin(data, &tmplt, sizeof tmplt)))
			return error;
		abts = stackgap_alloc(&sg, sizeof tmpbts);
		/*
		 * First fill in all fields, so that we keep the current
		 * values for fields that Linux doesn't know about.
		 */
		if ((error = (*fp->f_ops->fo_ioctl)(fp, TIOCGETA,
		    (caddr_t) &tmpbts, p)))
			return error;
		linux_termio_to_bsd_termios(&tmplt, &tmpbts);
		if ((error = copyout(&tmpbts, abts, sizeof tmpbts)))
			return error;
		SCARG(&ia, data) = (caddr_t) abts;
		return sys_ioctl(p, &ia, retval);
	case LINUX_TIOCSETD:
		if ((error = copyin(data, (caddr_t) &idat, sizeof idat)))
			return error;
		switch (idat) {
		case LINUX_N_TTY:
			idat = TTYDISC;
			break;
		case LINUX_N_SLIP:
			idat = SLIPDISC;
			break;
		case LINUX_N_PPP:
			idat = PPPDISC;
			break;
		/*
		 * We can't handle the mouse line discipline Linux has.
		 */
		case LINUX_N_MOUSE:
		default:
			return EINVAL;
		}

		idatp = (int *) stackgap_alloc(&sg, sizeof (int));
		if ((error = copyout(&idat, idatp, sizeof (int))))
			return error;
		SCARG(&ia, com) = TIOCSETD;
		SCARG(&ia, data) = (caddr_t) idatp;
		break;
	case LINUX_TIOCGETD:
		idatp = (int *) stackgap_alloc(&sg, sizeof (int));
		SCARG(&ia, com) = TIOCGETD;
		SCARG(&ia, data) = (caddr_t) idatp;
		if ((error = sys_ioctl(p, &ia, retval)))
			return error;
		if ((error = copyin(idatp, (caddr_t) &idat, sizeof (int))))
			return error;
		switch (idat) {
		case TTYDISC:
			idat = LINUX_N_TTY;
			break;
		case SLIPDISC:
			idat = LINUX_N_SLIP;
			break;
		case PPPDISC:
			idat = LINUX_N_PPP;
			break;
		/*
		 * Linux does not have the tablet line discipline.
		 */
		case TABLDISC:
		default:
			idat = -1;	/* XXX What should this be? */
			break;
		}
		return copyout(&idat, data, sizeof (int));
	case LINUX_TIOCGWINSZ:
		SCARG(&ia, com) = TIOCGWINSZ;
		break;
	case LINUX_TIOCSWINSZ:
		SCARG(&ia, com) = TIOCSWINSZ;
		break;
	case LINUX_TIOCGPGRP:
		SCARG(&ia, com) = TIOCGPGRP;
		break;
	case LINUX_TIOCSPGRP:
		SCARG(&ia, com) = TIOCSPGRP;
		break;
	case LINUX_FIONREAD:
		SCARG(&ia, com) = FIONREAD;
		break;
	case LINUX_FIONBIO:
		SCARG(&ia, com) = FIONBIO;
		break;
	case LINUX_FIOASYNC:
		SCARG(&ia, com) = FIOASYNC;
		break;
	case LINUX_TIOCEXCL:
		SCARG(&ia, com) = TIOCEXCL;
		break;
	case LINUX_TIOCNXCL:
		SCARG(&ia, com) = TIOCNXCL;
		break;
	case LINUX_TIOCCONS:
		SCARG(&ia, com) = TIOCCONS;
		break;
	case LINUX_TIOCNOTTY:
		SCARG(&ia, com) = TIOCNOTTY;
		break;
	case LINUX_SIOCADDMULTI:
		SCARG(&ia, com) = SIOCADDMULTI;
		break;
	case LINUX_SIOCDELMULTI:
		SCARG(&ia, com) = SIOCDELMULTI;
		break;
	default:
		return linux_machdepioctl(p, uap, retval);
	}

	return sys_ioctl(p, &ia, retval);
}
