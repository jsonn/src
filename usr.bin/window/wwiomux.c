/*	$NetBSD: wwiomux.c,v 1.13.20.1 2009/05/13 19:20:12 jym Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)wwiomux.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: wwiomux.c,v 1.13.20.1 2009/05/13 19:20:12 jym Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#if !defined(OLD_TTY) && !defined(TIOCPKT_DATA)
#include <sys/ioctl.h>
#endif
#include <sys/time.h>
#include <poll.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include "ww.h"

/*
 * Multiple window output handler.
 * The idea is to copy window outputs to the terminal, via the
 * display package.  We try to give wwcurwin highest priority.
 * The only return conditions are when there is keyboard input
 * and when a child process dies.
 * When there's nothing to do, we sleep in a select().
 * The history of this routine is interesting.
 */
void
wwiomux(void)
{
	struct ww *w;
	nfds_t nfd;
	int i;
	int volatile dostdin;	/* avoid longjmp clobbering */
	char volatile c;	/* avoid longjmp clobbering */
	char *p;
	int millis;
	char noblock = 0;
	static struct pollfd *pfd = NULL;
	static nfds_t maxfds = 0;

	c = 0; 	/* XXXGCC -Wuninitialized */

	for (;;) {
		if (wwinterrupt()) {
			wwclrintr();
			return;
		}

		nfd = 0;
		for (w = wwhead.ww_forw; w != &wwhead; w = w->ww_forw) {
			if (w->ww_pty < 0 || w->ww_obq >= w->ww_obe)
				continue;
			nfd++;
		}

		if (maxfds <= ++nfd) {	/* One more for the fd=0 case below */
			struct pollfd *npfd = pfd == NULL ?
			    malloc(sizeof(*pfd) * nfd) :
			   realloc(pfd, sizeof(*pfd) * nfd);
			if (npfd == NULL) {
				warn("will retry");
				if (pfd)
					free(pfd);
				pfd = NULL;
				maxfds = 0;
				return;
			}
			pfd = npfd;
			maxfds = nfd;
		}

		nfd = 0;
		for (w = wwhead.ww_forw; w != &wwhead; w = w->ww_forw) {
			if (w->ww_pty < 0)
				continue;
			if (w->ww_obq < w->ww_obe) {
				pfd[nfd].fd = w->ww_pty;
				pfd[nfd++].events = POLLIN;
			}
			if (w->ww_obq > w->ww_obp &&
			    !ISSET(w->ww_pflags, WWP_STOPPED))
				noblock = 1;
		}
		if (wwibq < wwibe) {
			dostdin = nfd;
			pfd[nfd].fd = 0;
			pfd[nfd++].events = POLLIN;
		} else {
			dostdin = -1;
		}

		if (!noblock) {
			if (wwcurwin != 0)
				wwcurtowin(wwcurwin);
			wwupdate();
			wwflush();
			(void) setjmp(wwjmpbuf);
			wwsetjmp = 1;
			if (wwinterrupt()) {
				wwsetjmp = 0;
				wwclrintr();
				return;
			}
			/* XXXX */
			millis = 30000;
		} else {
			millis = 10;
		}
		wwnselect++;
		i = poll(pfd, nfd, millis);
		wwsetjmp = 0;
		noblock = 0;

		if (i < 0)
			wwnselecte++;
		else if (i == 0)
			wwnselectz++;
		else {
			if (dostdin != -1 && (pfd[dostdin].revents & POLLIN) != 0)
				wwrint();

			nfd = 0;
			for (w = wwhead.ww_forw; w != &wwhead; w = w->ww_forw) {
				int n;

				if (w->ww_pty < 0)
					continue;
				if (w->ww_pty != pfd[nfd].fd)
					continue;
				if ((pfd[nfd++].revents & POLLIN) == 0)
					continue;
				wwnwread++;
				p = w->ww_obq;
				if (w->ww_type == WWT_PTY) {
					if (p == w->ww_ob) {
						w->ww_obp++;
						w->ww_obq++;
					} else
						p--;
					c = *p;
				}
				n = read(w->ww_pty, p, w->ww_obe - p);
				if (n < 0) {
					wwnwreade++;
					(void) close(w->ww_pty);
					w->ww_pty = -1;
				} else if (n == 0) {
					wwnwreadz++;
					(void) close(w->ww_pty);
					w->ww_pty = -1;
				} else if (w->ww_type != WWT_PTY) {
					wwnwreadd++;
					wwnwreadc += n;
					w->ww_obq += n;
				} else if (*p == TIOCPKT_DATA) {
					n--;
					wwnwreadd++;
					wwnwreadc += n;
					w->ww_obq += n;
				} else {
					wwnwreadp++;
					if (*p & TIOCPKT_STOP)
						SET(w->ww_pflags, WWP_STOPPED);
					if (*p & TIOCPKT_START)
						CLR(w->ww_pflags, WWP_STOPPED);
					if (*p & TIOCPKT_FLUSHWRITE) {
						CLR(w->ww_pflags, WWP_STOPPED);
						w->ww_obq = w->ww_obp =
							w->ww_ob;
					}
				}
				if (w->ww_type == WWT_PTY)
					*p = c;
			}
		}
		/*
		 * Try the current window first, if there is output
		 * then process it and go back to the top to try again.
		 * This can lead to starvation of the other windows,
		 * but presumably that what we want.
		 * Update will eventually happen when output from wwcurwin
		 * dies down.
		 */
		if ((w = wwcurwin) != 0 && w->ww_pty >= 0 &&
		    w->ww_obq > w->ww_obp &&
		    !ISSET(w->ww_pflags, WWP_STOPPED)) {
			int n = wwwrite(w, w->ww_obp, w->ww_obq - w->ww_obp);
			if ((w->ww_obp += n) == w->ww_obq)
				w->ww_obq = w->ww_obp = w->ww_ob;
			noblock = 1;
			continue;
		}
		for (w = wwhead.ww_forw; w != &wwhead; w = w->ww_forw)
			if (w->ww_pty >= 0 && w->ww_obq > w->ww_obp &&
			    !ISSET(w->ww_pflags, WWP_STOPPED)) {
				int n = wwwrite(w, w->ww_obp,
					w->ww_obq - w->ww_obp);
				if ((w->ww_obp += n) == w->ww_obq)
					w->ww_obq = w->ww_obp = w->ww_ob;
				if (wwinterrupt())
					break;
			}
	}
}
