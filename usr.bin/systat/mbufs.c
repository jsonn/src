/*	$NetBSD: mbufs.c,v 1.14.26.1 2009/05/13 19:20:07 jym Exp $	*/

/*-
 * Copyright (c) 1980, 1992, 1993
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)mbufs.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: mbufs.c,v 1.14.26.1 2009/05/13 19:20:07 jym Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/mbuf.h>

#include <stdlib.h>

#include "systat.h"
#include "extern.h"

static struct mbstat *mb;

const char *mtnames[] = {
	"free",
	"data",
	"headers",
	"sockets",
	"pcbs",
	"routes",
	"hosts",
	"arps",
	"socknames",
	"zombies",
	"sockopts",
	"frags",
	"rights",
	"ifaddrs",
};

#define	NNAMES	(sizeof (mtnames) / sizeof (mtnames[0]))

WINDOW *
openmbufs(void)
{

	return (subwin(stdscr, -1, 0, 5, 0));
}

void
closembufs(WINDOW *w)
{

	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

void
labelmbufs(void)
{

	wmove(wnd, 0, 0); wclrtoeol(wnd);
	mvwaddstr(wnd, 0, 10,
	    "/0   /5   /10  /15  /20  /25  /30  /35  /40  /45  /50  /55  /60");
}

void
showmbufs(void)
{
	int i, j, max, idx;
	char buf[10];

	if (mb == 0)
		return;
	for (j = 0; j < getmaxy(wnd); j++) {
		max = 0, idx = -1; 
		for (i = 0; i < getmaxy(wnd); i++)
			if (mb->m_mtypes[i] > max) {
				max = mb->m_mtypes[i];
				idx = i;
			}
		if (max == 0)
			break;
		if (j > (int)NNAMES)
			mvwprintw(wnd, 1+j, 0, "%10d", idx);
		else
			mvwprintw(wnd, 1+j, 0, "%-10.10s", mtnames[idx]);
		wmove(wnd, 1 + j, 10);
		if (max > 60) {
			snprintf(buf, sizeof buf, " %5d", max);
			max = 60;
			while (max--)
				waddch(wnd, 'X');
			waddstr(wnd, buf);
		} else {
			wclrtoeol(wnd);
			whline(wnd, 'X', max);
		}
		mb->m_mtypes[idx] = 0;
	}
	wmove(wnd, 1+j, 0); wclrtobot(wnd);
}

static struct nlist namelist[] = {
#define	X_MBSTAT	0
	{ .n_name = "_mbstat" },
	{ .n_name = NULL }
};

int
initmbufs(void)
{

	if (namelist[X_MBSTAT].n_type == 0) {
		if (kvm_nlist(kd, namelist)) {
			nlisterr(namelist);
			return(0);
		}
		if (namelist[X_MBSTAT].n_type == 0) {
			error("No namelist");
			return(0);
		}
	}
	if (mb == 0)
		mb = (struct mbstat *)calloc(1, sizeof (*mb));
	return(1);
}

void
fetchmbufs(void)
{

	if (namelist[X_MBSTAT].n_type == 0)
		return;
	NREAD(X_MBSTAT, mb, sizeof (*mb));
}
