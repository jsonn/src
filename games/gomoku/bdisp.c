/*	$NetBSD: bdisp.c,v 1.8.32.1 2008/09/18 04:39:57 wrstuden Exp $	*/

/*
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
static char sccsid[] = "@(#)bdisp.c	8.2 (Berkeley) 5/3/95";
#else
__RCSID("$NetBSD: bdisp.c,v 1.8.32.1 2008/09/18 04:39:57 wrstuden Exp $");
#endif
#endif /* not lint */

#include <curses.h>
#include <string.h>
#include <stdlib.h>
#include "gomoku.h"

#define	SCRNH		24		/* assume 24 lines for the moment */
#define	SCRNW		80		/* assume 80 chars for the moment */

static	int	lastline;
static	char	pcolor[] = "*O.?";

extern int interactive;
extern char *plyr[];

/*
 * Initialize screen display.
 */
void
cursinit()
{

	if (!initscr()) {
		fprintf(stderr, "couldn't initialize screen\n");
		exit (0);
	}
	noecho();
	cbreak();
	leaveok(stdscr, TRUE);
}

/*
 * Restore screen display.
 */
void
cursfini()
{

	leaveok(stdscr, FALSE);
	move(23, 0);
	clrtoeol();
	refresh();
	endwin();
}

/*
 * Initialize board display.
 */
void
bdisp_init()
{
	int i, j;

	/* top border */
	for (i = 1; i < BSZ1; i++) {
		move(0, 2 * i + 1);
		addch(letters[i]);
	}
	/* left and right edges */
	for (j = BSZ1; --j > 0; ) {
		move(20 - j, 0);
		printw("%2d ", j);
		move(20 - j, 2 * BSZ1 + 1);
		printw("%d ", j);
	}
	/* bottom border */
	for (i = 1; i < BSZ1; i++) {
		move(20, 2 * i + 1);
		addch(letters[i]);
	}
	bdwho(0);
	move(0, 47);
	addstr("#  black  white");
	lastline = 0;
	bdisp();
}

/*
 * Update who is playing whom.
 */
void
bdwho(update)
	int update;
{
	int i;

	move(21, 0);
	clrtoeol();
	i = 6 - strlen(plyr[BLACK]) / 2;
	move(21, i > 0 ? i : 0);
	printw("BLACK/%s", plyr[BLACK]);
	i = 30 - strlen(plyr[WHITE]) / 2;
	move(21, i);
	printw("WHITE/%s", plyr[WHITE]);
	move(21, 19);
	addstr(" vs. ");
	if (update)
		refresh();
}

/*
 * Update the board display after a move.
 */
void
bdisp()
{
	int i, j, c;
	struct spotstr *sp;

	for (j = BSZ1; --j > 0; ) {
		for (i = 1; i < BSZ1; i++) {
			move(BSZ1 - j, 2 * i + 1);
			sp = &board[i + j * BSZ1];
			if (debug > 1 && sp->s_occ == EMPTY) {
				if (sp->s_flg & IFLAGALL)
					c = '+';
				else if (sp->s_flg & CFLAGALL)
					c = '-';
				else
					c = '.';
			} else
				c = pcolor[sp->s_occ];
			addch(c);
		}
	}
	refresh();
}

#ifdef DEBUG
/*
 * Dump board display to a file.
 */
void
bdump(fp)
	FILE *fp;
{
	int i, j, c;
	struct spotstr *sp;

	/* top border */
	fprintf(fp, "   A B C D E F G H J K L M N O P Q R S T\n");

	for (j = BSZ1; --j > 0; ) {
		/* left edge */
		fprintf(fp, "%2d ", j);
		for (i = 1; i < BSZ1; i++) {
			sp = &board[i + j * BSZ1];
			if (debug > 1 && sp->s_occ == EMPTY) {
				if (sp->s_flg & IFLAGALL)
					c = '+';
				else if (sp->s_flg & CFLAGALL)
					c = '-';
				else
					c = '.';
			} else
				c = pcolor[sp->s_occ];
			putc(c, fp);
			putc(' ', fp);
		}
		/* right edge */
		fprintf(fp, "%d\n", j);
	}

	/* bottom border */
	fprintf(fp, "   A B C D E F G H J K L M N O P Q R S T\n");
}
#endif /* DEBUG */

/*
 * Display a transcript entry
 */
void
dislog(str)
	const char *str;
{

	if (++lastline >= SCRNH - 1) {
		/* move 'em up */
		lastline = 1;
	}
	move(lastline, 46);
	addnstr(str, SCRNW - 46 - 1);
	clrtoeol();
	move(lastline + 1, 46);
	clrtoeol();
}

/*
 * Display a question.
 */

void
ask(str)
	const char *str;
{
	int len = strlen(str);

	move(23, 0);
	addstr(str);
	clrtoeol();
	move(23, len);
	refresh();
}

int
getline(buf, size)
	char *buf;
	int size;
{
	char *cp, *end;
	int c;

	c = 0;
	cp = buf;
	end = buf + size - 1;	/* save room for the '\0' */
	while (cp < end && (c = getchar()) != EOF && c != '\n' && c != '\r') {
		*cp++ = c;
		if (interactive) {
			switch (c) {
			case 0x0c: /* ^L */
				wrefresh(curscr);
				cp--;
				continue;
			case 0x15: /* ^U */
			case 0x18: /* ^X */
				while (cp > buf) {
					cp--;
					addch('\b');
				}
				clrtoeol();
				break;
			case '\b':
			case 0x7f: /* DEL */
				if (cp == buf + 1) {
					cp--;
					continue;
				}
				cp -= 2;
				addch('\b');
				c = ' ';
				/* FALLTHROUGH */
			default:
				addch(c);
			}
			refresh();
		}
	}
	*cp = '\0';
	return(c != EOF);
}
