/*-
 * Copyright (c) 1993, 1994
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
 */

#ifndef lint
static char sccsid[] = "@(#)sex_term.c	8.40 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "../ex/script.h"

/*
 * sex_key_read --
 *	Read characters from the input.
 */
enum input
sex_key_read(sp, nrp, timeout)
	SCR *sp;
	int *nrp;
	struct timeval *timeout;
{
	struct timeval t, *tp;
	GS *gp;
	IBUF *tty;
	int maxfd, nr;

	*nrp = 0;
	gp = sp->gp;
	tty = gp->tty;

	/*
	 * We're about to block; check for signals.  If a signal received,
	 * clear it immediately, so that if it's reset while being serviced
	 * we won't miss it.
	 *
	 * These signal recipients set global flags.  None of this has
	 * anything to do with input keys, but it's something that can't
	 * be done asynchronously without adding locking to handle race
	 * conditions, and which needs to be done periodically.
	 */
sigchk:	while (F_ISSET(gp, G_SIGINT | G_SIGWINCH)) {
		if (F_ISSET(gp, G_SIGINT))
			return (INP_INTR);
		if (F_ISSET(gp, G_SIGWINCH)) {
			F_CLR(gp, G_SIGWINCH);
			if (!sp->s_window(sp, 1))
				(void)sp->s_refresh(sp, sp->ep);
		}
	}

	/*
	 * There are three cases here:
	 *
	 * 1: A read from a file or a pipe.  In this case, the reads
	 *    never timeout regardless.  This means that we can hang
	 *    when trying to complete a map, but we're going to hang
	 *    on the next read anyway.
	 */
	if (!F_ISSET(gp, G_STDIN_TTY)) {
		if ((nr = read(STDIN_FILENO,
		    tty->ch + tty->next + tty->cnt,
		    tty->nelem - (tty->next + tty->cnt))) > 0)
			goto success;
		return (INP_EOF);
	}

	/*
	 * 2: A read with an associated timeout.  In this case, we are trying
	 *    to complete a map sequence.  Ignore script windows and timeout
	 *    as specified.  If input arrives, we fall into #3, but because
	 *    timeout isn't NULL, don't read anything but command input.
	 *
	 * If interrupted, go back and check to see what it was.
	 */
	if (timeout != NULL) {
		if (F_ISSET(sp, S_SCRIPT))
			FD_CLR(sp->script->sh_master, &sp->rdfd);
		FD_SET(STDIN_FILENO, &sp->rdfd);
		for (;;) {
			switch (select(STDIN_FILENO + 1,
			    &sp->rdfd, NULL, NULL, timeout)) {
			case -1:		/* Error or interrupt. */
				if (errno == EINTR)
					goto sigchk;
				goto err;
			case  1:		/* Characters ready. */
				break;
			case  0:		/* Timeout. */
				return (INP_OK);
			}
			break;
		}
	}

	/*
	 * 3: At this point, we'll take anything that comes.  Select on the
	 *    command file descriptor and the file descriptor for the script
	 *    window if there is one.  Poll the fd's, increasing the timeout
	 *    each time each time we don't get anything until we're blocked
	 *    on I/O.
	 *
	 * If interrupted, go back and check to see what it was.
	 */
	for (t.tv_sec = t.tv_usec = 0;;) {
		/*
		 * Reset each time -- sscr_input() may call other
		 * routines which could reset bits.
		 */
		if (timeout == NULL && F_ISSET(sp, S_SCRIPT)) {
			tp = &t;

			FD_SET(STDIN_FILENO, &sp->rdfd);
			if (F_ISSET(sp, S_SCRIPT)) {
				FD_SET(sp->script->sh_master, &sp->rdfd);
				maxfd =
				    MAX(STDIN_FILENO, sp->script->sh_master);
			} else
				maxfd = STDIN_FILENO;
		} else {
			tp = NULL;

			FD_SET(STDIN_FILENO, &sp->rdfd);
			if (F_ISSET(sp, S_SCRIPT))
				FD_CLR(sp->script->sh_master, &sp->rdfd);
			maxfd = STDIN_FILENO;
		}

		switch (select(maxfd + 1, &sp->rdfd, NULL, NULL, tp)) {
		case -1:		/* Error or interrupt. */
			if (errno == EINTR)
				goto sigchk;
err:			msgq(sp, M_SYSERR, "select");
			return (INP_ERR);
		case 0:			/* Timeout. */
			if (t.tv_usec) {
				++t.tv_sec;
				t.tv_usec = 0;
			} else
				t.tv_usec += 500000;
			continue;
		}

		if (timeout == NULL && F_ISSET(sp, S_SCRIPT) &&
		    FD_ISSET(sp->script->sh_master, &sp->rdfd)) {
			sscr_input(sp);
			continue;
		}

		switch (nr = read(STDIN_FILENO,
		    tty->ch + tty->next + tty->cnt,
		    tty->nelem - (tty->next + tty->cnt))) {
		case  0:			/* EOF. */
			return (INP_EOF);
		case -1:			/* Error or interrupt. */
			if (errno == EINTR)
				goto sigchk;
			msgq(sp, M_SYSERR, "read");
			return (INP_ERR);
		default:
			goto success;
		}
		/* NOTREACHED */
	}

success:
	MEMSET(tty->chf + tty->next + tty->cnt, 0, nr);
	tty->cnt += *nrp = nr;
	return (INP_OK);
}
