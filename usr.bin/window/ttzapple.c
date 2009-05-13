/*	$NetBSD: ttzapple.c,v 1.8.20.1 2009/05/13 19:20:12 jym Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
static char sccsid[] = "@(#)ttzapple.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: ttzapple.c,v 1.8.20.1 2009/05/13 19:20:12 jym Exp $");
#endif
#endif /* not lint */

#include <stdio.h>
#include "ww.h"
#include "tt.h"
#include "char.h"

/*
zz|zapple|perfect apple:\
	:am:pt:co#80:li#24:le=^H:nd=^F:up=^K:do=^J:\
	:ho=\E0:ll=\E1:cm=\E=%+ %+ :ch=\E<%+ :cv=\E>%+ :\
	:cl=\E4:ce=\E2:cd=\E3:rp=\E@%.%+ :\
	:so=\E+:se=\E-:\
	:dc=\Ec:DC=\EC%+ :ic=\Ei:IC=\EI%+ :\
	:al=\Ea:AL=\EA%+ :dl=\Ed:DL=\ED%+ :\
	:sf=\Ef:SF=\EF%+ :sr=\Er:SR=\ER%+ :cs=\E?%+ %+ :\
	:is=\E-\ET :
*/

#define NCOL		80
#define NROW		24
#define TOKEN_MAX	32

extern short gen_frame[];

	/* for error correction */
int zz_ecc;
int zz_lastc;

	/* for checkpointing */
int zz_sum;

void	zz_checkpoint(void);
void	zz_checksum(char *, int);
void	zz_clear(void);
void	zz_clreol(void);
void	zz_clreos(void);
void	zz_compress(int);
void	zz_delchar(int);
void	zz_delline(int);
void	zz_end(void);
void	zz_insline(int);
void	zz_insspace(int);
void	zz_move(int, int);
void	zz_put_token(int, const char *, int);
void	zz_putc(char);
void	zz_reset(void);
int	zz_rint(char *, int);
void	zz_scroll_down(int);
void	zz_scroll_up(int);
void	zz_setmodes(int);
void	zz_setscroll(int, int);
void	zz_set_token(int, char *, int);
void	zz_start(void);
void	zz_write(const char *, int);

void
zz_setmodes(int new)
{
	if (new & WWM_REV) {
		if ((tt.tt_modes & WWM_REV) == 0)
			ttesc('+');
	} else
		if (tt.tt_modes & WWM_REV)
			ttesc('-');
	tt.tt_modes = new;
}

void
zz_insline(int n)
{
	if (n == 1)
		ttesc('a');
	else {
		ttesc('A');
		ttputc(n + ' ');
	}
}

void
zz_delline(int n)
{
	if (n == 1)
		ttesc('d');
	else {
		ttesc('D');
		ttputc(n + ' ');
	}
}

void
zz_putc(char c)
{
	if (tt.tt_nmodes != tt.tt_modes)
		zz_setmodes(tt.tt_nmodes);
	ttputc(c);
	if (++tt.tt_col == NCOL)
		tt.tt_col = 0, tt.tt_row++;
}

void
zz_write(const char *p, int n)
{
	if (tt.tt_nmodes != tt.tt_modes)
		zz_setmodes(tt.tt_nmodes);
	ttwrite(p, n);
	tt.tt_col += n;
	if (tt.tt_col == NCOL)
		tt.tt_col = 0, tt.tt_row++;
}

void
zz_move(int row, int col)
{
	int x;

	if (tt.tt_row == row) {
same_row:
		if ((x = col - tt.tt_col) == 0)
			return;
		if (col == 0) {
			ttctrl('m');
			goto out;
		}
		switch (x) {
		case 2:
			ttctrl('f');
		case 1:
			ttctrl('f');
			goto out;
		case -2:
			ttctrl('h');
		case -1:
			ttctrl('h');
			goto out;
		}
		if ((col & 7) == 0 && x > 0 && x <= 16) {
			ttctrl('i');
			if (x > 8)
				ttctrl('i');
			goto out;
		}
		ttesc('<');
		ttputc(col + ' ');
		goto out;
	}
	if (tt.tt_col == col) {
		switch (row - tt.tt_row) {
		case 2:
			ttctrl('j');
		case 1:
			ttctrl('j');
			goto out;
		case -2:
			ttctrl('k');
		case -1:
			ttctrl('k');
			goto out;
		}
		if (col == 0) {
			if (row == 0)
				goto home;
			if (row == NROW - 1)
				goto ll;
		}
		ttesc('>');
		ttputc(row + ' ');
		goto out;
	}
	if (col == 0) {
		if (row == 0) {
home:
			ttesc('0');
			goto out;
		}
		if (row == tt.tt_row + 1) {
			/*
			 * Do newline first to match the sequence
			 * for scroll down and return
			 */
			ttctrl('j');
			ttctrl('m');
			goto out;
		}
		if (row == NROW - 1) {
ll:
			ttesc('1');
			goto out;
		}
	}
	/* favor local motion for better compression */
	if (row == tt.tt_row + 1) {
		ttctrl('j');
		goto same_row;
	}
	if (row == tt.tt_row - 1) {
		ttctrl('k');
		goto same_row;
	}
	ttesc('=');
	ttputc(' ' + row);
	ttputc(' ' + col);
out:
	tt.tt_col = col;
	tt.tt_row = row;
}

void
zz_start(void)
{
	ttesc('T');
	ttputc(TOKEN_MAX + ' ');
	ttesc('U');
	ttputc('!');
	zz_ecc = 1;
	zz_lastc = -1;
	ttesc('v');
	ttflush();
	zz_sum = 0;
	zz_setscroll(0, NROW - 1);
	zz_clear();
	zz_setmodes(0);
}

void
zz_reset(void)
{
	zz_setscroll(0, NROW - 1);
	tt.tt_modes = WWM_REV;
	zz_setmodes(0);
	tt.tt_col = tt.tt_row = -10;
}

void
zz_end(void)
{
	ttesc('T');
	ttputc(' ');
	ttesc('U');
	ttputc(' ');
	zz_ecc = 0;
}

void
zz_clreol(void)
{
	ttesc('2');
}

void
zz_clreos(void)
{
	ttesc('3');
}

void
zz_clear(void)
{
	ttesc('4');
	tt.tt_col = tt.tt_row = 0;
}

void
zz_insspace(int n)
{
	if (n == 1)
		ttesc('i');
	else {
		ttesc('I');
		ttputc(n + ' ');
	}
}

void
zz_delchar(int n)
{
	if (n == 1)
		ttesc('c');
	else {
		ttesc('C');
		ttputc(n + ' ');
	}
}

void
zz_scroll_down(int n)
{
	if (n == 1) {
		if (tt.tt_row == NROW - 1)
			ttctrl('j');
		else
			ttesc('f');
	} else {
		ttesc('F');
		ttputc(n + ' ');
	}
}

void
zz_scroll_up(int n)
{
	if (n == 1)
		ttesc('r');
	else {
		ttesc('R');
		ttputc(n + ' ');
	}
}

void
zz_setscroll(int top, int bot)
{
	ttesc('?');
	ttputc(top + ' ');
	ttputc(bot + ' ');
	tt.tt_scroll_top = top;
	tt.tt_scroll_bot = bot;
}

int zz_debug = 0;

void
zz_set_token(int t, char *s, int n)
{
	if (tt.tt_nmodes != tt.tt_modes)
		zz_setmodes(tt.tt_nmodes);
	if (zz_debug) {
		char buf[100];
		zz_setmodes(WWM_REV);
		(void) sprintf(buf, "%02x=", t);
		ttputs(buf);
		tt.tt_col += 3;
	}
	ttputc(0x80);
	ttputc(t + 1);
	s[n - 1] |= 0x80;
	ttwrite(s, n);
	s[n - 1] &= ~0x80;
}

void
zz_put_token(int t, const char *s __unused, int n __unused)
{
	if (tt.tt_nmodes != tt.tt_modes)
		zz_setmodes(tt.tt_nmodes);
	if (zz_debug) {
		char buf[100];
		zz_setmodes(WWM_REV);
		(void) sprintf(buf, "%02x>", t);
		ttputs(buf);
		tt.tt_col += 3;
	}
	ttputc(t + 0x81);
}

int
zz_rint(char *p, int n)
{
	int i;
	char *q;

	if (!zz_ecc)
		return n;
	for (i = n, q = p; --i >= 0;) {
		int c = (unsigned char) *p++;

		if (zz_lastc == 0) {
			switch (c) {
			case 0:
				*q++ = 0;
				zz_lastc = -1;
				break;
			case 1:		/* start input ecc */
				zz_ecc = 2;
				zz_lastc = -1;
				wwnreadstat++;
				break;
			case 2:		/* ack checkpoint */
				tt.tt_ack = 1;
				zz_lastc = -1;
				wwnreadack++;
				break;
			case 3:		/* nack checkpoint */
				tt.tt_ack = -1;
				zz_lastc = -1;
				wwnreadnack++;
				break;
			default:
				zz_lastc = c;
				wwnreadec++;
			}
		} else if (zz_ecc == 1) {
			if (c)
				*q++ = c;
			else
				zz_lastc = 0;
		} else {
			if (zz_lastc < 0) {
				zz_lastc = c;
			} else if (zz_lastc == c) {
				*q++ = zz_lastc;
				zz_lastc = -1;
			} else {
				wwnreadec++;
				zz_lastc = c;
			}
		}
	}
	return q - (p - n);
}

void
zz_checksum(char *p, int n)
{
	while (--n >= 0) {
		int c = *p++ & 0x7f;
		c ^= zz_sum;
		zz_sum = c << 1 | (c >> 11 & 1);
	}
}

void
zz_compress(int flag)
{
	if (flag)
		tt.tt_checksum = 0;
	else
		tt.tt_checksum = zz_checksum;
}

void
zz_checkpoint(void)
{
	static char x[] = { ctrl('['), 'V', 0, 0 };

	zz_checksum(x, sizeof x);
	ttesc('V');
	ttputc(' ' + (zz_sum & 0x3f));
	ttputc(' ' + (zz_sum >> 6 & 0x3f));
	ttflush();
	zz_sum = 0;
}

int
tt_zapple(void)
{
	tt.tt_insspace = zz_insspace;
	tt.tt_delchar = zz_delchar;
	tt.tt_insline = zz_insline;
	tt.tt_delline = zz_delline;
	tt.tt_clreol = zz_clreol;
	tt.tt_clreos = zz_clreos;
	tt.tt_scroll_down = zz_scroll_down;
	tt.tt_scroll_up = zz_scroll_up;
	tt.tt_setscroll = zz_setscroll;
	tt.tt_availmodes = WWM_REV;
	tt.tt_wrap = 1;
	tt.tt_retain = 0;
	tt.tt_ncol = NCOL;
	tt.tt_nrow = NROW;
	tt.tt_start = zz_start;
	tt.tt_reset = zz_reset;
	tt.tt_end = zz_end;
	tt.tt_write = zz_write;
	tt.tt_putc = zz_putc;
	tt.tt_move = zz_move;
	tt.tt_clear = zz_clear;
	tt.tt_setmodes = zz_setmodes;
	tt.tt_frame = gen_frame;
	tt.tt_padc = TT_PADC_NONE;
	tt.tt_ntoken = 127;
	tt.tt_set_token = zz_set_token;
	tt.tt_put_token = zz_put_token;
	tt.tt_token_min = 1;
	tt.tt_token_max = TOKEN_MAX;
	tt.tt_set_token_cost = 2;
	tt.tt_put_token_cost = 1;
	tt.tt_compress = zz_compress;
	tt.tt_checksum = zz_checksum;
	tt.tt_checkpoint = zz_checkpoint;
	tt.tt_reset = zz_reset;
	tt.tt_rint = zz_rint;
	return 0;
}
