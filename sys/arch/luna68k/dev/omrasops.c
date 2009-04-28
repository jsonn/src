/* $NetBSD: omrasops.c,v 1.6.8.1 2009/04/28 07:34:17 skrll Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: omrasops.c,v 1.6.8.1 2009/04/28 07:34:17 skrll Exp $");

/*
 * Designed speficically for 'm68k bitorder';
 *	- most significant byte is stored at lower address,
 *	- most significant bit is displayed at left most on screen.
 * Implementation relys on;
 *	- every memory references is done in aligned 32bit chunk,
 *	- font glyphs are stored in 32bit padded.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wscons_rfont.h>
#include <dev/wscons/wsdisplayvar.h>

void rcons_init(struct rcons *, int, int);

/* wscons emulator operations */
static void	om_cursor(void *, int, int, int);
static int	om_mapchar(void *, int, unsigned int *);
static void	om_putchar(void *, int, int, u_int, long);
static void	om_copycols(void *, int, int, int, int);
static void	om_copyrows(void *, int, int, int num);
static void	om_erasecols(void *, int, int, int, long);
static void	om_eraserows(void *, int, int, long);
static int	om_allocattr(void *, int, int, int, long *);

struct wsdisplay_emulops omfb_emulops = {
	om_cursor,
	om_mapchar,
	om_putchar,
	om_copycols,
	om_erasecols,
	om_copyrows,
	om_eraserows,
	om_allocattr
};

#define	ALL1BITS	(~0U)
#define	ALL0BITS	(0U)
#define	BLITWIDTH	(32)
#define	ALIGNMASK	(0x1f)
#define	BYTESDONE	(4)

#define	W(p) (*(u_int32_t *)(p))
#define	R(p) (*(u_int32_t *)((uint8_t *)(p) + 0x40000))

/*
 * Blit a character at the specified co-ordinates.
 */
static void
om_putchar(void *cookie, int row, int startcol, u_int uc, long attr)
{
	struct rcons *rc = cookie;
	struct raster *rap = rc->rc_sp;
	uint8_t *p;
	int scanspan, startx, height, width, align, y;
	u_int32_t lmask, rmask, glyph, inverse;
	u_int32_t *g;

	scanspan = rap->linelongs * 4;
	y = rc->rc_yorigin + rc->rc_font->height * row;
	startx = rc->rc_xorigin + rc->rc_font->width * startcol;
	height = rc->rc_font->height;
	g = rc->rc_font->chars[uc].r->pixels;
	inverse = (attr != 0) ? ALL1BITS : ALL0BITS;

	p = (uint8_t *)rap->pixels + y * scanspan + ((startx / 32) * 4);
	align = startx & ALIGNMASK;
	width = rc->rc_font->width + align;
	lmask = ALL1BITS >> align;
	rmask = ALL1BITS << (-width & ALIGNMASK);
	if (width <= BLITWIDTH) {
		lmask &= rmask;
		while (height > 0) {
			glyph = *g;
			glyph = (glyph >> align) ^ inverse;
			W(p) = (R(p) & ~lmask) | (glyph & lmask);
			p += scanspan;
			g += 1;
			height--;
		}
	}
	else {
		uint8_t *q = p;
		u_int32_t lhalf, rhalf;

		while (height > 0) {
			glyph = *g;
			lhalf = (glyph >> align) ^ inverse;
			W(p) = (R(p) & ~lmask) | (lhalf & lmask);
			p += BYTESDONE;
			rhalf = (glyph << (BLITWIDTH - align)) ^ inverse;
			W(p) = (rhalf & rmask) | (R(p) & ~rmask);

			p = (q += scanspan);
			g += 1;
			height--;
		}
	}
}

static void
om_erasecols(void *cookie, int row, int startcol, int ncols, long attr)
{
        struct rcons *rc = cookie;
        struct raster *rap = rc->rc_sp;
        uint8_t *p;
        int scanspan, startx, height, width, align, w, y;
        u_int32_t lmask, rmask, fill;

        scanspan = rap->linelongs * 4;
        y = rc->rc_yorigin + rc->rc_font->height * row;
        startx = rc->rc_xorigin + rc->rc_font->width * startcol;
        height = rc->rc_font->height;
        w = rc->rc_font->width * ncols;
	fill = (attr != 0) ? ALL1BITS : ALL0BITS;

	p = (uint8_t *)rap->pixels + y * scanspan + ((startx / 32) * 4);
	align = startx & ALIGNMASK;
	width = w + align;
	lmask = ALL1BITS >> align;
	rmask = ALL1BITS << (-width & ALIGNMASK);
	if (width <= BLITWIDTH) {
		lmask &= rmask;
		fill &= lmask;
		while (height > 0) {
			W(p) = (R(p) & ~lmask) | fill;
			p += scanspan;
			height--;
		}
	}
	else {
		uint8_t *q = p;
		while (height > 0) {
			W(p) = (R(p) & ~lmask) | (fill & lmask);
			width -= 2 * BLITWIDTH;
			while (width > 0) {
				p += BYTESDONE;
				W(p) = fill;
				width -= BLITWIDTH;
			}
			p += BYTESDONE;
			W(p) = (fill & rmask) | (R(p) & ~rmask);

			p = (q += scanspan);
			width = w + align;
			height--;
		}
	}
}

static void
om_eraserows(void *cookie, int startrow, int nrows, long attr)
{
	struct rcons *rc = cookie;
	struct raster *rap = rc->rc_sp;
	uint8_t *p, *q;
	int scanspan, starty, height, width, w;
	u_int32_t rmask, fill;

	scanspan = rap->linelongs * 4;
	starty = rc->rc_yorigin + rc->rc_font->height * startrow;
	height = rc->rc_font->height * nrows;
	w = rc->rc_font->width * rc->rc_maxcol;
	fill = (attr == 1) ? ALL1BITS : ALL0BITS;

	p = (uint8_t *)rap->pixels + starty * scanspan;
	p += (rc->rc_xorigin / 32) * 4;
	width = w;
        rmask = ALL1BITS << (-width & ALIGNMASK);
	q = p;
	while (height > 0) {
		W(p) = fill;				/* always aligned */
		width -= 2 * BLITWIDTH;
		while (width > 0) {
			p += BYTESDONE;
			W(p) = fill;
			width -= BLITWIDTH;
		}
		p += BYTESDONE;
		W(p) = (fill & rmask) | (R(p) & ~rmask);
		p = (q += scanspan);
		width = w;
		height--;
	}
}

static void
om_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
        struct rcons *rc = cookie;
        struct raster *rap = rc->rc_sp;
        uint8_t *p, *q;
	int scanspan, offset, srcy, height, width, w;
        u_int32_t rmask;
        
	scanspan = rap->linelongs * 4;
	height = rc->rc_font->height * nrows;
	offset = (dstrow - srcrow) * scanspan * rc->rc_font->height;
	srcy = rc->rc_yorigin + rc->rc_font->height * srcrow;
	if (srcrow < dstrow && srcrow + nrows > dstrow) {
		scanspan = -scanspan;
		srcy += height;
	}

	p = (uint8_t *)rap->pixels + srcy * (rap->linelongs * 4);
	p += (rc->rc_xorigin / 32) * 4;
	w = rc->rc_font->width * rc->rc_maxcol;
	width = w;
	rmask = ALL1BITS << (-width & ALIGNMASK);
	q = p;
	while (height > 0) {
		W(p + offset) = R(p);			/* always aligned */
		width -= 2 * BLITWIDTH;
		while (width > 0) {
			p += BYTESDONE;
			W(p + offset) = R(p);
			width -= BLITWIDTH;
		}
		p += BYTESDONE;
		W(p + offset) = (R(p) & rmask) | (R(p + offset) & ~rmask);

		p = (q += scanspan);
		width = w;
		height--;
	}
}

static void
om_copycols(void *cookie, int startrow, int srccol, int dstcol, int ncols)
{
	struct rcons *rc = cookie;
	struct raster *rap = rc->rc_sp;
	uint8_t *sp, *dp, *basep;
	int scanspan, height, width, align, shift, w, y, srcx, dstx;
	u_int32_t lmask, rmask;

	scanspan = rap->linelongs * 4;
	y = rc->rc_yorigin + rc->rc_font->height * startrow;
	srcx = rc->rc_xorigin + rc->rc_font->width * srccol;
	dstx = rc->rc_xorigin + rc->rc_font->width * dstcol;
	height = rc->rc_font->height;
	w = rc->rc_font->width * ncols;
	basep = (uint8_t *)rap->pixels + y * scanspan;

	align = shift = srcx & ALIGNMASK;
	width = w + align;
	align = dstx & ALIGNMASK;
	lmask = ALL1BITS >> align;
	rmask = ALL1BITS << (-(w + align) & ALIGNMASK);
	shift = align - shift; 
	sp = basep + (srcx / 32) * 4;
	dp = basep + (dstx / 32) * 4;

	if (shift != 0)
		goto hardluckalignment;

	/* alignments comfortably match */
	if (width <= BLITWIDTH) {
		lmask &= rmask;
		while (height > 0) {
			W(dp) = (R(dp) & ~lmask) | (R(sp) & lmask);
			dp += scanspan;
			sp += scanspan;
			height--;
		}
	}
	/* copy forward (left-to-right) */
	else if (dstcol < srccol || srccol + ncols < dstcol) {
		uint8_t *sq = sp, *dq = dp;

		w = width;
		while (height > 0) {
			W(dp) = (R(dp) & ~lmask) | (R(sp) & lmask);
			width -= 2 * BLITWIDTH;
			while (width > 0) {
				sp += BYTESDONE;
				dp += BYTESDONE;
				W(dp) = R(sp);
				width -= BLITWIDTH;
			}
			sp += BYTESDONE;
			dp += BYTESDONE;
			W(dp) = (R(sp) & rmask) | (R(dp) & ~rmask);
			sp = (sq += scanspan);
			dp = (dq += scanspan);
			width = w;
			height--;
		}
	}
	/* copy backward (right-to-left) */
	else {
		uint8_t *sq, *dq;

		sq = (sp += width / 32 * 4);
		dq = (dp += width / 32 * 4);
		w = width;
		while (height > 0) {
			W(dp) = (R(sp) & rmask) | (R(dp) & ~rmask);
			width -= 2 * BLITWIDTH;
			while (width > 0) {
				sp -= BYTESDONE;
				dp -= BYTESDONE;
				W(dp) = R(sp);
				width -= BLITWIDTH;
			}
			sp -= BYTESDONE;
			dp -= BYTESDONE;
			W(dp) = (R(dp) & ~lmask) | (R(sp) & lmask);

			sp = (sq += scanspan);
			dp = (dq += scanspan);
			width = w;
			height--;
		}
	}
	return;

    hardluckalignment:
	/* alignments painfully disagree */
	return;
}

/*
 * Map a character.
 */
static int
om_mapchar(void *cookie, int c, u_int *cp)
{
	if (c < 128) {
		*cp = c;
		return (5);
	}
	*cp = ' ';
	return (0);
}

/*
 * Position|{enable|disable} the cursor at the specified location.
 */
static void
om_cursor(void *cookie, int on, int row, int col)
{
	struct rcons *rc = cookie;
	struct raster *rap = rc->rc_sp;
	uint8_t *p;
	int scanspan, startx, height, width, align, y;
	u_int32_t lmask, rmask, image;

	if (!on) {
		/* make sure it's on */
		if ((rc->rc_bits & RC_CURSOR) == 0)
			return;

		row = *rc->rc_crowp;
		col = *rc->rc_ccolp;
	} else {
		/* unpaint the old copy. */
		*rc->rc_crowp = row;
		*rc->rc_ccolp = col;
	}

	scanspan = rap->linelongs * 4;
	y = rc->rc_yorigin + rc->rc_font->height * row;
	startx = rc->rc_xorigin + rc->rc_font->width * col;
	height = rc->rc_font->height;

	p = (uint8_t *)rap->pixels + y * scanspan + ((startx / 32) * 4);
	align = startx & ALIGNMASK;
	width = rc->rc_font->width + align;
	lmask = ALL1BITS >> align;
	rmask = ALL1BITS << (-width & ALIGNMASK);
	if (width <= BLITWIDTH) {
		lmask &= rmask;
		while (height > 0) {
			image = R(p);
			W(p) = (image & ~lmask) | ((image ^ ALL1BITS) & lmask);
			p += scanspan;
			height--;
		}
	}
	else {
		uint8_t *q = p;

		while (height > 0) {
			image = R(p);
			W(p) = (image & ~lmask) | ((image ^ ALL1BITS) & lmask);
			p += BYTESDONE;
			image = R(p);
			W(p) = ((image ^ ALL1BITS) & rmask) | (image & ~rmask);

			p = (q += scanspan);
			height--;
		}
	}
	rc->rc_bits ^= RC_CURSOR;
}

/*
 * Allocate attribute. We just pack these into an integer.
 */
static int
om_allocattr(void *id, int fg, int bg, int flags, long *attrp)
{
	if (flags & (WSATTR_HILIT | WSATTR_BLINK |
		     WSATTR_UNDERLINE | WSATTR_WSCOLORS))
		return (EINVAL);
	if (flags & WSATTR_REVERSE)
		*attrp = 1;
	else
		*attrp = 0;
	return (0);
}

void
rcons_init(struct rcons *rc, int mrow, int mcol)
{
	struct raster *rp = rc->rc_sp;
	int i;

	rc->rc_font = &gallant19; /* 12x22 monospacing font */

	/* Get distance to top and bottom of font from font origin */
	rc->rc_font_ascent = -(rc->rc_font->chars)['a'].homey;

	i = rp->height / rc->rc_font->height;
	rc->rc_maxrow = min(i, mrow);

	i = rp->width / rc->rc_font->width;
	rc->rc_maxcol = min(i, mcol);

	/* Center emulator screen (but align x origin to 32 bits) */
	rc->rc_xorigin =
	    ((rp->width - rc->rc_maxcol * rc->rc_font->width) / 2) & ~ALIGNMASK;
	rc->rc_yorigin =
	    (rp->height - rc->rc_maxrow * rc->rc_font->height) / 2;
#if 0
	/* Raster width used for row copies */
	rc->rc_raswidth = rc->rc_maxcol * rc->rc_font->width;
	if (rc->rc_raswidth & ALIGNMASK) {
		/* Pad to 32 bits */
		i = (rc->rc_raswidth + ALIGNMASK) & ~ALIGNMASK;
		/* Make sure width isn't too wide */
		if (rc->rc_xorigin + i <= rp->width)
			rc->rc_raswidth = i;
	}
#endif
	rc->rc_bits = 0;
}
