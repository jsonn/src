/*	$NetBSD: ite.c,v 1.39.4.2 1997/11/26 04:13:31 mellon Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
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
 * from: Utah $Hdr: ite.c 1.28 92/12/20$
 *
 *	@(#)ite.c	8.2 (Berkeley) 1/12/94
 */

/*
 * ite.c
 *
 * The ite module handles the system console; that is, stuff printed
 * by the kernel and by user programs while "desktop" and X aren't
 * running.  Some (very small) parts are based on hp300's 4.4 ite.c,
 * hence the above copyright.
 *
 *   -- Brad and Lawrence, June 26th, 1994
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <dev/cons.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/viareg.h>

#define KEYBOARD_ARRAY
#include <machine/keyboard.h>
#include <machine/adbsys.h>
#include <machine/iteioctl.h>
#include <machine/grfioctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <mac68k/mac68k/macrom.h>
#include <mac68k/dev/nubus.h>
#include <mac68k/dev/itevar.h>
#include <mac68k/dev/grfvar.h>

#include "6x10.h"
#define CHARWIDTH	6
#define CHARHEIGHT	10

/* Local function prototypes */
static inline void putpixel1 __P((int, int, int *, int));
static void	putpixel2 __P((int, int, int *, int));
static void	putpixel4 __P((int, int, int *, int));
static void	putpixel8 __P((int, int, int *, int));
static void	putpixel16 __P((int, int, int *, int));
static void	putpixel32 __P((int, int, int *, int));
static void	reversepixel1 __P((int, int, int));
static void	writechar __P((char, int, int, int));
static void	drawcursor __P((void));
static void	erasecursor __P((void));
static void	scrollup __P((void));
static void	scrolldown __P((void));
static void	clear_screen __P((int));
static void	clear_line __P((int));
static void	reset_tabs __P((void));
static void	clear_tabs __P((void));
static void	vt100_reset __P((void));
static void	putc_normal __P((char));
static void	putc_esc __P((char));
static void	putc_gotpars __P((char));
static void	putc_getpars __P((char));
static void	putc_square __P((char));
static void	ite_putchar __P((char));
static int	ite_pollforchar __P((void));
static int	itematch __P((struct device *, struct cfdata *, void *));
static void	iteattach __P((struct device *, struct device *, void *));

#define dprintf if (0) printf

#define ATTR_NONE	0
#define ATTR_BOLD	1
#define ATTR_UNDER	2
#define ATTR_REVERSE	4

enum vt100state_e {
	ESnormal,		/* Nothing yet                             */
	ESesc,			/* Got ESC                                 */
	ESsquare,		/* Got ESC [				   */
	ESgetpars,		/* About to get or getting the parameters  */
	ESgotpars,		/* Finished getting the parameters         */
	ESfunckey,		/* Function key                            */
	EShash,			/* DEC-specific stuff (screen align, etc.) */
	ESsetG0,		/* Specify the G0 character set            */
	ESsetG1,		/* Specify the G1 character set            */
	ESignore		/* Ignore this sequence                    */
} vt100state = ESnormal;

/* From Booter via locore */
long		videoaddr;
long		videorowbytes;
long		videobitdepth;
u_long		videosize;

/* Calculated by itecninit() */
static int	ite_initted = 0;
static int	width, height;		/* width, height in pixels */
static int	scrcols, scrrows;	/* width, height in characters */
static int	screenrowbytes;		/* number of visible bytes per row */

/* VT100 emulation */
#define MAXPARS	16			/* max number of VT100 op parameters */
static int	par[MAXPARS], numpars;	/* parameter array, # of parameters */
static int	x = 0, y = 0;		/* current VT100 cursor location */
static int	savex, savey;		/* saved cursor location */
static int	hanging_cursor;		/* cursor waiting for more output */
static int	attr;			/* current video attribute */
static char	tab_stops[255];		/* tab stops */
static int	scrreg_top;		/* scroll region */
static int	scrreg_bottom;

/* Console bell parameters */
static int	bell_freq = 1880;	/* frequency */
static int	bell_length = 10;	/* duration */
static int	bell_volume = 100;	/* volume */

/* For polled ADB mode */
static int	polledkey;
extern int	adb_polling;

extern u_int32_t mac68k_vidphys;

struct tty	*ite_tty;		/* Our tty */

static void	(*putpixel) __P((int x, int y, int *c, int num));
static void	(*reversepixel) __P((int x, int y, int num));

/* For capslock key functionality */
#define isealpha(ch)  (((ch)>='A'&&(ch)<='Z')||((ch)>='a'&&(ch)<='z')||((ch)>=0xC0&&(ch)<=0xFF))

/*
 * Bitmap handling functions
 */

static inline void 
putpixel1(xx, yy, c, num)
	int xx, yy;
	int *c;
	int num;
{
	u_int i, mask;
	u_char *sc;

	sc = (u_char *)videoaddr;

	i = 7 - (xx & 7);
	mask = ~(1 << i);
	sc += yy * videorowbytes + (xx >> 3);
	while (num--) {
		*sc &= mask;
		*sc |= (*c++ & 1) << i;
		sc += videorowbytes;
	}
}

static void 
putpixel2(xx, yy, c, num)
	int xx, yy;
	int *c;
	int num;
{
	u_int i, mask;
	u_char *sc;

	sc = (u_char *)videoaddr;

	i = 6 - ((xx & 3) << 1);
	mask = ~(3 << i);
	sc += yy * videorowbytes + (xx >> 2);
	while (num--) {
		*sc &= mask;
		*sc |= (*c++ & 3) << i;
		sc += videorowbytes;
	}
}

static void 
putpixel4(xx, yy, c, num)
	int xx, yy;
	int *c;
	int num;
{
	u_int i, mask;
	u_char *sc;

	sc = (u_char *)videoaddr;

	i = 4 - ((xx & 1) << 2);
	mask = ~(15 << i);
	sc += yy * videorowbytes + (xx >> 1);
	while (num--) {
		*sc &= mask;
		*sc |= (*c++ & 15) << i;
		sc += videorowbytes;
	}
}

static void 
putpixel8(xx, yy, c, num)
	int xx, yy;
	int *c;
	int num;
{
	u_char *sc;

	sc = (u_char *)videoaddr;

	sc += yy * videorowbytes + xx;
	while (num--) {
		*sc = *c++ & 0xff;
		sc += videorowbytes;
	}
}

static void
putpixel16(xx, yy, c, num)
	int xx, yy;
	int *c;
	int num;
{
	u_short	*sc;
	int videorowshorts;
	u_char uc;

	sc = (u_short *)videoaddr;

	videorowshorts = videorowbytes >> 1;
	sc += yy * videorowshorts + xx;
	while (num--) {
		uc = (*c++ & 0xff);
		*sc = (uc << 8) | uc;
		sc += videorowshorts;
	}
}

static void
putpixel32(xx, yy, c, num)
	int xx, yy;
	int *c;
	int num;
{
	u_long *sc;
	int videorowlongs;
	u_char uc;

	sc = (u_long *)videoaddr;

	videorowlongs = videorowbytes >> 2;
	sc += yy * videorowlongs + xx;
	while (num--) {
		uc = (*c++ & 0xff);
		*sc = (uc << 24) | (uc << 16) | (uc << 8) | uc;
		sc += videorowlongs;
	}
}

static void 
reversepixel1(xx, yy, num)
	int xx, yy, num;
{
	u_int mask;
	u_char *sc;
	u_long *sl;
	u_short *ss;
	int videorowshorts;
	int videorowlongs;

	sc = (u_char *)videoaddr;
	mask = 0;		/* Get rid of warning from compiler */

	switch (videobitdepth) {
	case 1:
		mask = 1 << (7 - (xx & 7));
		sc += yy * videorowbytes + (xx >> 3);
		break;
	case 2:
		mask = 3 << (6 - ((xx & 3) << 1));
		sc += yy * videorowbytes + (xx >> 2);
		break;
	case 4:
		mask = 15 << (4 - ((xx & 1) << 2));
		sc += yy * videorowbytes + (xx >> 1);
		break;
	case 8:
		mask = 255;
		sc += yy * videorowbytes + xx;
		break;
	case 16:
		videorowshorts = videorowbytes >> 1;
		ss = (u_short *)videoaddr;
		ss += yy * videorowshorts + xx;
		while (num--) {
			*ss ^= 0xffff;
			ss += videorowshorts;
		}
		return;
	case 32:
		videorowlongs = videorowbytes >> 2;
		sl = (u_long *)videoaddr;
		sl += yy * videorowlongs + xx;
		while (num--) {
			*sl ^= 0xffffffff;
			sl += videorowlongs;
		}
		return;
	default:
		panic("reversepixel(): unsupported bit depth");
	}

	while (num--) {
		*sc ^= mask;
		sc += videorowbytes;
	}
}

static void 
writechar(ch, x, y, attr)
	char ch;
	int x, y, attr;
{
	int i, j, mask, rev, col[CHARHEIGHT];
	u_char *c;

	ch &= 0x7F;
	x *= CHARWIDTH;
	y *= CHARHEIGHT;

	rev = (attr & ATTR_REVERSE) ? 255 : 0;

	c = &Font6x10[ch * CHARHEIGHT];

	switch (videobitdepth) {
	case 1:
		for (j = 0; j < CHARWIDTH; j++) {
			mask = 1 << (CHARWIDTH - 1 - j);
			for (i = 0; i < CHARHEIGHT; i++)
				col[i] = ((c[i] & mask) ? 255 : 0) ^ rev;
			putpixel1(x + j, y, col, CHARHEIGHT);
		}
		if (attr & ATTR_UNDER) {
			col[0] = 255;
			for (j = 0; j < CHARWIDTH; j++)
				putpixel1(x + j, y + CHARHEIGHT - 1, col, 1);
		}
		break;
	case 2:
	case 4:
	case 8:
	case 16:
	case 32:
		for (j = 0; j < CHARWIDTH; j++) {
			mask = 1 << (CHARWIDTH - 1 - j);
			for (i = 0; i < CHARHEIGHT; i++)
				col[i] = ((c[i] & mask) ? 255 : 0) ^ rev;
			putpixel(x + j, y, col, CHARHEIGHT);
		}
		if (attr & ATTR_UNDER) {
			col[0] = 255;
			for (j = 0; j < CHARWIDTH; j++)
				putpixel(x + j, y + CHARHEIGHT - 1, col, 1);
		}
		break;
	}
}

static void 
drawcursor()
{
	u_int j, X, Y;

	X = x * CHARWIDTH;
	Y = y * CHARHEIGHT;

	for (j = 0; j < CHARWIDTH; j++)
		reversepixel(X + j, Y, CHARHEIGHT);
}

static void 
erasecursor()
{
	u_int j, X, Y;

	X = x * CHARWIDTH;
	Y = y * CHARHEIGHT;

	for (j = 0; j < CHARWIDTH; j++)
		reversepixel(X + j, Y, CHARHEIGHT);
}

static void 
scrollup()
{
	u_char *from, *to;
	u_int linebytes;
	u_short i;

	linebytes = videorowbytes * CHARHEIGHT;
	to = (u_char *)videoaddr + (scrreg_top * linebytes);
	from = to + linebytes;

	for (i = (scrreg_bottom - scrreg_top) * CHARHEIGHT; i > 0; i--) {
		ovbcopy(from, to, screenrowbytes);
		from += videorowbytes;
		to += videorowbytes;
	}
	for (i = CHARHEIGHT; i > 0; i--) {
		bzero(to, screenrowbytes);
		to += videorowbytes;
	}
}

static void 
scrolldown()
{
	u_char *from, *to;
	u_int linebytes;
	u_short i;

	linebytes = videorowbytes * CHARHEIGHT;
	to = (u_char *)videoaddr + ((scrreg_bottom + 1) * linebytes);
	from = to - linebytes;

	for (i = (scrreg_bottom - scrreg_top) * CHARHEIGHT; i > 0; i--) {
		from -= videorowbytes;
		to -= videorowbytes;
		ovbcopy(from, to, screenrowbytes);
	}
	for (i = CHARHEIGHT; i > 0; i--) {
		to -= videorowbytes;
		bzero(to, screenrowbytes);
	}
}

static void 
clear_screen(which)
	int which;
{
	u_char *p;
	u_short len, i;

	p = (u_char *)videoaddr;

	switch (which) {
	case 0:		/* To end of screen	 */
		if (x > 0) {
			clear_line(0);
			if (y < scrrows)
				y++;
			x = 0;
		}
		p += y * videorowbytes * CHARHEIGHT;
		len = scrrows - y;
		break;
	case 1:		/* To start of screen	 */
		if (x > 0) {
			clear_line(1);
			if (y > 0)
				y--;
			x = 0;
		}
		len = y;
		break;
	case 2:		/* Whole screen		 */
	default:
		len = scrrows;
		break;
	}

	for (i = len * CHARHEIGHT; i > 0; i--) {
		bzero(p, screenrowbytes);
		p += videorowbytes;
	}
}

static void 
clear_line(which)
	int which;
{
	u_char *to;
	u_int linebytes;
	int start, end, i;


	/*
	 * This routine runs extremely slowly.  I don't think it's
	 * used all that often, except for To end of line.  I'll go
	 * back and speed this up when I speed up the whole ite
	 * module. --LK
	 */

	switch (which) {
	default:
	case 0:		/* To end of line	 */
		start = x;
		end = scrcols;
		break;
	case 1:		/* To start of line	 */
		start = 0;
		end = x;
		break;
	case 2:		/* Whole line		 */
		linebytes = videorowbytes * CHARHEIGHT;
		to = (u_char *)videoaddr + (y * linebytes);

		for (i = CHARHEIGHT; i > 0; i--) {
			bzero(to, screenrowbytes);
			to += videorowbytes;
		}
		return;
	}

	for (i = start; i < end; i++)
		writechar(' ', i, y, ATTR_NONE);
}

static void
reset_tabs()
{
	int i;

	for (i = 0; i < scrcols; i++)
		tab_stops[i] = ((i % 8) == 0);
}

static void
clear_tabs()
{
	int i;

	for (i = 0; i < scrcols; i++)
		tab_stops[i] = 0;
}

static void
vt100_reset()
{
	reset_tabs();
	scrreg_top    = 0;
	scrreg_bottom = scrrows - 1;
	attr = ATTR_NONE;
}

static void 
putc_normal(ch)
	char ch;
{
	switch (ch) {
	case '\a':		/* Beep			 */
		mac68k_ring_bell(bell_freq, bell_length, bell_volume);
		break;
	case 127:		/* Delete		 */
	case '\b':		/* Backspace		 */
		if (hanging_cursor)
			hanging_cursor = 0;
		else if (x > 0)
			x--;
		break;
	case '\t':		/* Tab			 */
		do
			x++;
		while ((tab_stops[x] == 0) && (x < scrcols));
		break;
	case '\n':		/* Line feed		 */
		if (y == scrreg_bottom)
			scrollup();
		else
			y++;
		break;
	case '\r':		/* Carriage return	 */
		x = 0;
		hanging_cursor = 0;
		break;
	case '\e':		/* Escape		 */
		vt100state = ESesc;
		hanging_cursor = 0;
		break;
	default:
		if (ch >= ' ') {
			if (hanging_cursor) {
				x = 0;
				if (y == scrreg_bottom)
					scrollup();
				else
					y++;
				hanging_cursor = 0;
			}

			writechar(ch, x, y, attr);

			if (x == scrcols - 1)
				hanging_cursor = 1;
			else
				x++;
			if (x >= scrcols) {	/* can we ever get here? */
				x = 0;
				y++;
			}
		}
		break;
	}
}

static void 
putc_esc(ch)
	char ch;
{
	vt100state = ESnormal;

	switch (ch) {
	case '[':
		vt100state = ESsquare;
		break;
	case '(':
		vt100state = ESsetG0;
		break;
	case ')':
		vt100state = ESsetG1;
		break;
	case 'E':		/* Next line		 */
		x = 0;
		/* FALLTHROUGH */
	case 'D':		/* Line feed		 */
		if (y == scrreg_bottom)
			scrollup();
		else
			y++;
		break;
	case 'H':		/* Set tab stop		 */
		tab_stops[x] = 1;
		break;
	case 'M':		/* Cursor up		 */
		if (y == scrreg_top)
			scrolldown();
		else
			y--;
		break;
	case '>':
		vt100_reset();
		break;
	case '7':		/* Save cursor		 */
		savex = x;
		savey = y;
		break;
	case '8':		/* Restore cursor	 */
		x = savex;
		y = savey;
		break;
	default:
		/* Rest not supported */
		break;
	}
}

static void 
putc_gotpars(ch)
	char ch;
{
	int i;

	vt100state = ESnormal;
	switch (ch) {
	case 'A':		/* Up			 */
		y -= par[0] ? par[0] : 1;
		if (y < scrreg_top)
			y = scrreg_top;
		break;
	case 'B':		/* Down			 */
		y += par[0] ? par[0] : 1;
		if (y > scrreg_bottom)
			y = scrreg_bottom;
		break;
	case 'C':		/* Right		 */
		x+= par[0] ? par[0] : 1;
		break;
	case 'D':		/* Left			 */
		x-= par[0] ? par[0] : 1;
		break;
	case 'H':		/* Set cursor position	 */
	case 'f':		/* Set cursor position   */
		x = par[1] - 1;
		y = par[0] - 1;
		hanging_cursor = 0;
		break;
	case 'J':		/* Clear part of screen	 */
		clear_screen(par[0]);
		break;
	case 'K':		/* Clear part of line	 */
		clear_line(par[0]);
		break;
	case 'L':		/* Add line		*/
		if (scrreg_top < scrreg_bottom) {
			i = scrreg_top;
			scrreg_top = y;
			scrolldown();
			scrreg_top = i;
		} else
			clear_line(0);
		break;
	case 'M':		/* Delete line		*/
		if (scrreg_top < scrreg_bottom) {
			i = scrreg_top;
			scrreg_top = y;
			scrollup();
			scrreg_top = i;
		} else
			clear_line(0);
		break;
	case 'g':		/* Clear tab stops	 */
		if (numpars >= 1) {
			if (par[0] == 3)
				clear_tabs();
			else if (par[0] == 0)
				tab_stops[x] = 0;
		}
		break;
	case 'm':		/* Set attribute	 */
		for (i = 0; i < numpars; i++) {
			switch (par[i]) {
			case 0:
				attr = ATTR_NONE;
				break;
			case 1:
				attr |= ATTR_BOLD;
				break;
			case 4:
				attr |= ATTR_UNDER;
				break;
			case 7:
				attr |= ATTR_REVERSE;
				break;
			case 21:
				attr &= ~ATTR_BOLD;
				break;
			case 24:
				attr &= ~ATTR_UNDER;
				break;
			case 27:
				attr &= ~ATTR_REVERSE;
				break;
			}
		}
		break;
	case 'r':		/* Set scroll region	 */
		/* ensure top < bottom, and both within limits */
		if ((numpars > 0) && (par[0] < scrrows))
			scrreg_top = par[0] - 1;
		else
			scrreg_top = 0;
		if ((numpars > 1) && (par[1] <= scrrows) && (par[1] > par[0]))
			scrreg_bottom = par[1] - 1;
		else
			scrreg_bottom = scrrows - 1;
		break;
	}
}

static void 
putc_getpars(ch)
	char ch;
{
	switch (ch) {
	case '?':
		/* Not supported */
		return;
	case '[':
		vt100state = ESnormal;
		/* Not supported */
		return;
	default:
		if (ch == ';' && numpars < MAXPARS - 1)
			numpars++;
		else if (ch >= '0' && ch <= '9') {
			par[numpars] *= 10;
			par[numpars] += ch - '0';
		} else {
			numpars++;
			vt100state = ESgotpars;
			putc_gotpars(ch);
		}
	}
}

static void 
putc_square(ch)
	char ch;
{
	u_short i;

	for (i = 0; i < MAXPARS; i++)
		par[i] = 0;

	numpars = 0;
	vt100state = ESgetpars;

	putc_getpars(ch);
}

static void 
ite_putchar(ch)
	char ch;
{
	switch (vt100state) {
	default:
		vt100state = ESnormal;	/* FALLTHROUGH */
	case ESnormal:
		putc_normal(ch);
		break;
	case ESesc:
		putc_esc(ch);
		break;
	case ESsquare:
		putc_square(ch);
		break;
	case ESgetpars:
		putc_getpars(ch);
		break;
	case ESgotpars:
		putc_gotpars(ch);
		break;
	}

	if (x >= scrcols)
		x = scrcols - 1;
	if (x < 0)
		x = 0;
	if (y >= scrrows)
		y = scrrows - 1;
	if (y < 0)
		y = 0;
}


/*
 * Keyboard support functions
 */

static int 
ite_pollforchar()
{
	int s;
	register int intbits;

	s = splhigh();

	polledkey = -1;
	adb_polling = 1;

	/* pretend we're VIA interrupt dispatcher */
	while (polledkey == -1) {
		intbits = via_reg(VIA1, vIFR);

		if (intbits & V1IF_ADBRDY) {
			mrg_adbintr();
			via_reg(VIA1, vIFR) = V1IF_ADBRDY;
		}
		if (intbits & 0x10) {
			mrg_pmintr();
			via_reg(VIA1, vIFR) = 0x10;
		}
	}

	adb_polling = 0;

	splx(s);

	return polledkey;
}


/*
 * Autoconfig attachment
 */

struct cfattach ite_ca = {
	sizeof(struct device), itematch, iteattach
};

struct cfdriver ite_cd = {
	NULL, "ite", DV_TTY
};

static int
itematch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct grfbus_attach_args *ga = aux;
	struct grfmode *gm = ga->ga_grfmode;
	vm_offset_t pa;

	if (strcmp(ga->ga_name, "ite"))
		return 0;
	pa = pmap_extract(pmap_kernel(), (vm_offset_t)(gm->fbbase + gm->fboff));

	if (pa != mac68k_vidphys) {
		if (pa < 0xf9000000 || pa > 0xfeffffff)
			return 0;
		/*
		 * OK, this is in the NuBus standard slot space range,
		 * so it might be of the form 0xFssxxxxx.  Mask off the
		 * slot number and duplicate it in bits 20-23, per IM:V
		 * pp 459, 463, and IM:VI ch 30 p 17.
		 * Note:  this is an ugly hack and I wish I knew what
		 * to do about it.  -- sr
		 */
		pa = (vm_offset_t)(((u_long)pa & 0xff0fffff) |
		    (((u_long)pa & 0x0f000000) >> 4));
	}
	return (pa == mac68k_vidphys);
}

static void 
iteattach(parent, self, aux)
	struct device *parent, *self;
	void	*aux;
{
	printf(" (minimal console)\n");
}


/*
 * Tty handling functions
 */

int
iteopen(dev, mode, devtype, p)
	dev_t dev;
	int mode;
	int devtype;
	struct proc *p;
{
	register struct tty *tp;
	register int error;

	dprintf("iteopen(): enter(0x%x)\n", (int)dev);

	if (!ite_initted)
		return (ENXIO);

	if (ite_tty == NULL) {
		tp = ite_tty = ttymalloc();
		tty_attach(tp);
	} else
		tp = ite_tty;
	if ((tp->t_state & (TS_ISOPEN | TS_XCLUDE)) == (TS_ISOPEN | TS_XCLUDE)
	    && p->p_ucred->cr_uid != 0)
		return (EBUSY);

	tp->t_oproc = itestart;
	tp->t_param = NULL;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = CS8 | CREAD;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		tp->t_state = TS_ISOPEN | TS_CARR_ON;
		ttsetwater(tp);
	}

	error = (*linesw[tp->t_line].l_open) (dev, tp);
	tp->t_winsize.ws_row = scrrows;
	tp->t_winsize.ws_col = scrcols;

	dprintf("iteopen(): exit(%d)\n", error);
	return (error);
}

int
iteclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	dprintf("iteclose: enter (%d)\n", (int)dev);

	(*linesw[ite_tty->t_line].l_close) (ite_tty, flag);
	ttyclose(ite_tty);
#if 0
	ttyfree(ite_tty);
	ite_tty = (struct tty *) 0;
#endif

	dprintf("iteclose: exit\n");
	return 0;
}

int
iteread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	dprintf("iteread: enter\n");
	return (*linesw[ite_tty->t_line].l_read) (ite_tty, uio, flag);
}

int
itewrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	dprintf("itewrite: enter\n");
	return (*linesw[ite_tty->t_line].l_write) (ite_tty, uio, flag);
}

struct tty *
itetty(dev)
	dev_t dev;
{
	return (ite_tty);
}

int
iteioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	int cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	register struct tty *tp = ite_tty;
	int error;

	dprintf("iteioctl: enter(%d, 0x%x)\n", cmd, cmd);

	error = (*linesw[tp->t_line].l_ioctl) (tp, cmd, addr, flag, p);
	if (error >= 0) {
		dprintf("iteioctl: exit(%d)\n", error);
		return (error);
	}

	error = ttioctl(tp, cmd, addr, flag, p);
	if (error >= 0) {
		dprintf("iteioctl: exit(%d)\n", error);
		return (error);
	}

	switch (cmd) {
	case ITEIOC_RINGBELL:
		return mac68k_ring_bell(bell_freq, bell_length, bell_volume);
	case ITEIOC_SETBELL:
		{
			struct bellparams *bp = (void *)addr;

			/* Do some sanity checks. */
			if (bp->freq < 10 || bp->freq > 40000)
				return (EINVAL);
			if (bp->len < 0 || bp->len > 3600)
				return (EINVAL);
			if (bp->vol < 0 || bp->vol > 100)
				return (EINVAL);

			bell_freq = bp->freq;
			bell_length = bp->len;
			bell_volume = bp->vol;
			return (0);
		}
	case ITEIOC_GETBELL:
		{
			struct bellparams *bp = (void *)addr;

			bell_freq = bp->freq;
			bell_length = bp->len;
			bell_volume = bp->vol;
			return (0);
		}
	}

	dprintf("iteioctl: exit(ENOTTY)\n");
	return (ENOTTY);
}

void 
itestart(register struct tty * tp)
{
	register int cc, s;

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		splx(s);
		return;
	}
	tp->t_state |= TS_BUSY;

	cc = tp->t_outq.c_cc;
	splx(s);
	erasecursor();
	while (cc-- > 0)
		ite_putchar(getc(&tp->t_outq));
	drawcursor();

	s = spltty();
	tp->t_state &= ~TS_BUSY;
	splx(s);
}

void 
itestop(struct tty * tp, int flag)
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
}

int
ite_intr(adb_event_t * event)
{
	static int shift = 0, control = 0, capslock = 0;
	int key, press, val, state;
	char str[10], *s;

	key = event->u.k.key;
	press = ADBK_PRESS(key);
	val = ADBK_KEYVAL(key);

	if (val == ADBK_SHIFT)
		shift = press;
	else if (val == ADBK_CAPSLOCK)
		capslock = !capslock;
	else if (val == ADBK_CONTROL)
		control = press;
	else if (press) {
		switch (val) {
		case ADBK_UP:
			str[0] = '\e';
			str[1] = 'O';
			str[2] = 'A';
			str[3] = '\0';
			break;
		case ADBK_DOWN:
			str[0] = '\e';
			str[1] = 'O';
			str[2] = 'B';
			str[3] = '\0';
			break;
		case ADBK_RIGHT:
			str[0] = '\e';
			str[1] = 'O';
			str[2] = 'C';
			str[3] = '\0';
			break;
		case ADBK_LEFT:
			str[0] = '\e';
			str[1] = 'O';
			str[2] = 'D';
			str[3] = '\0';
			break;
		default:
			state = 0;
			if (capslock && isealpha(keyboard[val][1]))
				state = 1;
			if (shift)
				state = 1;
			if (control)
				state = 2;
			str[0] = keyboard[val][state];
			str[1] = '\0';
			break;
		}
		if (adb_polling)
			polledkey = str[0];
		else
			for (s = str; *s; s++)
				(*linesw[ite_tty->t_line].l_rint)(*s, ite_tty);
	}
	return 0;
}
/*
 * Console functions
 */

int
itecnprobe(struct consdev * cp)
{
	int maj, unit;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == iteopen)
			break;

	if (maj == nchrdev)
		panic("itecnprobe(): did not find iteopen().");

	unit = 0;		/* hardcode first device as console. */

	/* initialize required fields */
	cp->cn_dev = makedev(maj, unit);
	cp->cn_pri = CN_INTERNAL;

	return 0;
}

void
itereset()
{
	width = videosize & 0xffff;
	height = (videosize >> 16) & 0xffff;
	scrrows = height / CHARHEIGHT;
	scrcols = width / CHARWIDTH;

	switch (videobitdepth) {
	default:
	case 1:
		putpixel = putpixel1;
		reversepixel = reversepixel1;
		screenrowbytes = (width + 7) >> 3;
		break;
	case 2:
		putpixel = putpixel2;
		reversepixel = reversepixel1;
		screenrowbytes = (width + 3) >> 2;
		break;
	case 4:
		putpixel = putpixel4;
		reversepixel = reversepixel1;
		screenrowbytes = (width + 1) >> 1;
		break;
	case 8:
		putpixel = putpixel8;
		reversepixel = reversepixel1;
		screenrowbytes = width;
		break;
	case 16:
		putpixel = putpixel16;
		reversepixel = reversepixel1;
		screenrowbytes = width*2;
		break;
	case 32:
		putpixel = putpixel32;
		reversepixel = reversepixel1;
		screenrowbytes = width*4;
		break;
  	}
  
  	vt100_reset();
}
  
int
itecninit(struct consdev * cp)
{
	if (ite_initted)
		return 0;

	ite_initted = 1;
	itereset();
	return iteon(cp->cn_dev, 0);
}

int
iteon(dev_t dev, int flags)
{
	if (!ite_initted)
		return (-1);

	erasecursor();
	clear_screen(2);
	drawcursor();
	return 0;
}

int
iteoff(dev_t dev, int flags)
{
	if (!ite_initted)
		return (-1);

	erasecursor();
	clear_screen(2);
	return 0;
}

int
itecngetc(dev_t dev)
{
	/* Oh, man... */

	return ite_pollforchar();
}

int
itecnputc(dev_t dev, int c)
{
	extern dev_t mac68k_zsdev;
	extern int zscnputc __P((dev_t dev, int c));

	erasecursor();
	ite_putchar(c);
	drawcursor();
	if (mac68k_machine.serial_boot_echo)
		zscnputc(mac68k_zsdev, c);

	return c;
}
