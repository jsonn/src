/*	$NetBSD: twinkle2.c,v 1.5.4.1 2005/07/24 00:50:34 snj Exp $	*/

/*
 * 
 *  Copyright (c) 1980, 1993
 * 	 The Regents of the University of California.  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the University nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 * 
 * 	@(#)twinkle2.c	8.1 (Berkeley) 6/8/93
 */

#include <stdlib.h>
#include <unistd.h>
#include <curses.h>
#include <signal.h>

#define	NCOLS	80
#define	NLINES	24
#define	MAXPATTERNS	4

typedef struct {
	int	y, x;
} LOCS;

static LOCS	Layout[NCOLS * NLINES];	/* current board layout */

static int	Pattern,		/* current pattern number */
		Numstars;		/* number of stars in pattern */

static void puton(char);
static void die(int);
static void makeboard(void);
static int ison(int, int);

static int AM;
static char *VS;
static char *TI;
static char *CL;

int
main(void)
{
	char	*sp;
	char	buf[1024];
	char	*ptr = buf;

	srand(getpid());		/* initialize random sequence */

	if (isatty(0)) {
		initscr();
		gettmode();
		if ((sp = getenv("TERM")) != NULL)
			setterm(sp);
		signal(SIGINT, die);
	}
	else {
		printf("Need a terminal on fd=%d\n", 0);
		exit(1);
	}

	tgetent(buf, sp);

	AM = tgetflag("am");
	TI = tgetstr("ti", &ptr);
	if (TI == NULL) {
		printf("terminal does not have the ti capability\n");
		exit(1);
	}
	VS = tgetstr("vs", &ptr);
	if (VS == NULL) {
		printf("terminal does not have the vs capability\n");
		exit(1);
	}
	CL = tgetstr("cl", &ptr);
	if (CL == NULL) {
		printf("terminal does not have the cl capability\n");
		exit(1);
	}
	puts(TI);
	puts(VS);

	noecho();
	nonl();
	tputs(CL, NLINES, putchar);
	for (;;) {
		makeboard();		/* make the board setup */
		puton('*');		/* put on '*'s */
		puton(' ');		/* cover up with ' 's */
	}
}

/*
 * On program exit, move the cursor to the lower left corner by
 * direct addressing, since current location is not guaranteed.
 * We lie and say we used to be at the upper right corner to guarantee
 * absolute addressing.
 */
static void
die(int n)
{
	signal(SIGINT, SIG_IGN);
	mvcur(0, COLS - 1, LINES - 1, 0);
	endwin();
	exit(n);
}

static void
puton(char ch)
{
	LOCS	*lp;
	int		r;
	LOCS	*end;
	LOCS		temp;
	static int	lasty, lastx;

	end = &Layout[Numstars];
	for (lp = Layout; lp < end; lp++) {
		r = rand() % Numstars;
		temp = *lp;
		*lp = Layout[r];
		Layout[r] = temp;
	}

	for (lp = Layout; lp < end; lp++)
			/* prevent scrolling */
		if (!AM || (lp->y < NLINES - 1 || lp->x < NCOLS - 1)) {
			mvcur(lasty, lastx, lp->y, lp->x);
			putchar(ch);
			lasty = lp->y;
			if ((lastx = lp->x + 1) >= NCOLS)
				if (AM) {
					lastx = 0;
					lasty++;
				}
				else
					lastx = NCOLS - 1;
		}
}

/*
 * Make the current board setup.  It picks a random pattern and
 * calls ison() to determine if the character is on that pattern
 * or not.
 */
static void
makeboard(void)
{
	int		y, x;
	LOCS	*lp;

	Pattern = rand() % MAXPATTERNS;
	lp = Layout;
	for (y = 0; y < NLINES; y++)
		for (x = 0; x < NCOLS; x++)
			if (ison(y, x)) {
				lp->y = y;
				lp->x = x;
				lp++;
			}
	Numstars = lp - Layout;
}

/*
 * Return TRUE if (y, x) is on the current pattern.
 */
static int
ison(int y, int x)
{
	switch (Pattern) {
	  case 0:	/* alternating lines */
		return !(y & 01);
	  case 1:	/* box */
		if (x >= LINES && y >= NCOLS)
			return FALSE;
		if (y < 3 || y >= NLINES - 3)
			return TRUE;
		return (x < 3 || x >= NCOLS - 3);
	  case 2:	/* holy pattern! */
		return ((x + y) & 01);
	  case 3:	/* bar across center */
		return (y >= 9 && y <= 15);
	}
	/* NOTREACHED */
}
