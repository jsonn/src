/*	$NetBSD: win.c,v 1.13.42.1 2009/05/13 19:20:12 jym Exp $	*/

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
static char sccsid[] = "@(#)win.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: win.c,v 1.13.42.1 2009/05/13 19:20:12 jym Exp $");
#endif
#endif /* not lint */

#include <string.h>
#include "defs.h"
#include "char.h"
#include "window_string.h"

/*
 * Higher level routines for dealing with windows.
 *
 * There are two types of windows: user window, and information window.
 * User windows are the ones with a pty and shell.  Information windows
 * are for displaying error messages, and other information.
 *
 * The windows are doubly linked in overlapping order and divided into
 * two groups: foreground and normal.  Information
 * windows are always foreground.  User windows can be either.
 * Addwin() adds a window to the list at the top of one of the two groups.
 * Deletewin() deletes a window.  Front() moves a window to the front
 * of its group.  Wwopen(), wwadd(), and wwdelete() should never be called
 * directly.
 */

/*
 * Open a user window.
 */
struct ww *
openwin(int id, int row, int col, int nrow, int ncol, int nline, char *label, int type, int uflags, char *shf, char **sh)
{
	struct ww *w;

	if (id < 0 && (id = findid()) < 0)
		return 0;
	if (row + nrow <= 0 || row > wwnrow - 1
	    || col + ncol <= 0 || col > wwncol - 1) {
		error("Illegal window position.");
		return 0;
	}
	w = wwopen(type, 0, nrow, ncol, row, col, nline);
	if (w == 0) {
		error("Can't open window: %s.", wwerror());
		return 0;
	}
	w->ww_id = id;
	window[id] = w;
	CLR(w->ww_uflags, WWU_ALLFLAGS);
	SET(w->ww_uflags, uflags);
	w->ww_alt = w->ww_w;
	if (label != 0 && setlabel(w, label) < 0)
		error("No memory for label.");
	wwcursor(w, 1);
	/*
	 * We have to do this little maneuver to make sure
	 * addwin() puts w at the top, so we don't waste an
	 * insert and delete operation.
	 */
	setselwin((struct ww *)0);
	addwin(w, 0);
	setselwin(w);
	if (wwspawn(w, shf, sh) < 0) {
		error("Can't execute %s: %s.", shf, wwerror());
		closewin(w);
		return 0;
	}
	return w;
}

int
findid(void)
{
	int i;

	for (i = 0; i < NWINDOW && window[i] != 0; i++)
		;
	if (i >= NWINDOW) {
		error("Too many windows.");
		return -1;
	}
	return i;
}

struct ww *
findselwin(void)
{
	struct ww *w, *s = 0;
	int i;

	for (i = 0; i < NWINDOW; i++)
		if ((w = window[i]) != 0 && w != selwin &&
		    (s == 0 ||
		     (!isfg(w) && (w->ww_order < s->ww_order || isfg(s)))))
			s = w;
	return s;
}

/*
 * Close a user window.  Close all if w == 0.
 */
void
closewin(struct ww *w)
{
	char didit = 0;
	int i;

	if (w != 0) {
		closewin1(w);
		didit++;
	} else
		for (i = 0; i < NWINDOW; i++) {
			if ((w = window[i]) == 0)
				continue;
			closewin1(w);
			didit++;
		}
	if (didit) {
		if (selwin == 0) {
			if (lastselwin != 0) {
				setselwin(lastselwin);
				lastselwin = 0;
			} else if ((w = findselwin()))
				setselwin(w);
		}
		if (lastselwin == 0 && selwin)
			if ((w = findselwin()))
				lastselwin = w;
		reframe();
	}
}

/*
 * Open an information (display) window.
 */
struct ww *
openiwin(int nrow, const char *label)
{
	struct ww *w;

	if ((w = wwopen(WWT_INTERNAL, 0, nrow, wwncol, 2, 0, 0)) == 0)
		return 0;
	SET(w->ww_wflags, WWW_MAPNL | WWW_NOINTR | WWW_NOUPDATE | WWW_UNCTRL);
	SET(w->ww_uflags, WWU_HASFRAME | WWU_CENTER);
	w->ww_id = -1;
	(void) setlabel(w, label);
	addwin(w, 1);
	reframe();
	return w;
}

/*
 * Close an information window.
 */
void
closeiwin(struct ww *w)
{
	closewin1(w);
	reframe();
}

void
closewin1(struct ww *w)
{
	if (w == selwin)
		selwin = 0;
	if (w == lastselwin)
		lastselwin = 0;
	if (w->ww_id >= 0 && w->ww_id < NWINDOW)
		window[w->ww_id] = 0;
	if (w->ww_label)
		str_free(w->ww_label);
	deletewin(w);
	wwclose(w);
}

/*
 * Move the window to the top of its group.
 * Don't do it if already fully visible.
 * Wwvisible() doesn't work for tinted windows.
 * But anything to make it faster.
 * Always reframe() if doreframe is true.
 */
void
front(struct ww *w, char doreframe)
{
	if (w->ww_back != (isfg(w) ? framewin : fgwin) && !wwvisible(w)) {
		deletewin(w);
		addwin(w, isfg(w));
		doreframe = 1;
	}
	if (doreframe)
		reframe();
}

/*
 * Add a window at the top of normal windows or foreground windows.
 * For normal windows, we put it behind the current window.
 */
void
addwin(struct ww *w, char fg)
{
	if (fg) {
		wwadd(w, framewin);
		if (fgwin == framewin)
			fgwin = w;
	} else
		wwadd(w, selwin != 0 && selwin != w && !isfg(selwin)
				? selwin : fgwin);
}

/*
 * Delete a window.
 */
void
deletewin(struct ww *w)
{
	if (fgwin == w)
		fgwin = w->ww_back;
	wwdelete(w);
}

void
reframe(void)
{
	struct ww *w;

	wwunframe(framewin);
	for (w = wwhead.ww_back; w != &wwhead; w = w->ww_back)
		if (ISSET(w->ww_uflags, WWU_HASFRAME)) {
			wwframe(w, framewin);
			labelwin(w);
		}
}

void
labelwin(struct ww *w)
{
	int mode = w == selwin ? WWM_REV : 0;

	if (!ISSET(w->ww_uflags, WWU_HASFRAME))
		return;
	if (w->ww_id >= 0) {
		char buf[2];

		buf[0] = w->ww_id + '1';
		buf[1] = 0;
		wwlabel(w, framewin, 1, buf, mode);
	}
	if (w->ww_label) {
		int col;

		if (ISSET(w->ww_uflags, WWU_CENTER)) {
			col = (w->ww_w.nc - strlen(w->ww_label)) / 2;
			col = MAX(3, col);
		} else
			col = 3;
		wwlabel(w, framewin, col, w->ww_label, mode);
	}
}

void
stopwin(struct ww *w)
{
	if (w->ww_pty >= 0 && w->ww_type == WWT_PTY && wwstoptty(w->ww_pty) < 0)
		error("Can't stop output: %s.", wwerror());
	else
		SET(w->ww_pflags, WWP_STOPPED);
}

void
startwin(struct ww *w)
{
	if (w->ww_pty >= 0 && w->ww_type == WWT_PTY &&
	    wwstarttty(w->ww_pty) < 0)
		error("Can't start output: %s.", wwerror());
	else
		CLR(w->ww_pflags, WWP_STOPPED);
}

void
sizewin(struct ww *w, int nrow, int ncol)
{
	struct ww *back = w->ww_back;

	w->ww_alt.nr = w->ww_w.nr;
	w->ww_alt.nc = w->ww_w.nc;
	wwdelete(w);
	if (wwsize(w, nrow, ncol) < 0)
		error("Can't resize window: %s.", wwerror());
	wwadd(w, back);
	reframe();
}

void
waitnl(struct ww *w)
{
	(void) waitnl1(w, "[Type any key to continue]");
}

int
more(struct ww *w, char always)
{
	int c;
	int uc = ISSET(w->ww_wflags, WWW_UNCTRL);

	if (!always && w->ww_cur.r < w->ww_w.b - 2)
		return 0;
	c = waitnl1(w, "[Type escape to abort, any other key to continue]");
	CLR(w->ww_wflags, WWW_UNCTRL);
	wwputs("\033E", w);
	SET(w->ww_wflags, uc);
	return c == ctrl('[') ? 2 : 1;
}

int
waitnl1(struct ww *w, const char *prompt)
{
	int uc = ISSET(w->ww_wflags, WWW_UNCTRL);

	CLR(w->ww_wflags, WWW_UNCTRL);
	front(w, 0);
	wwprintf(w, "\033Y%c%c\033sA%s\033rA ",
		w->ww_w.nr - 1 + ' ', ' ', prompt);	/* print on last line */
	wwcurtowin(w);
	while (wwpeekc() < 0)
		wwiomux();
	SET(w->ww_wflags, uc);
	return wwgetc();
}
