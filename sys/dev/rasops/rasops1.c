/* 	$NetBSD: rasops1.c,v 1.12.8.1 2001/10/01 12:46:14 fvdl Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include "opt_rasops.h"
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rasops1.c,v 1.12.8.1 2001/10/01 12:46:14 fvdl Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <machine/endian.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/rasops/rasops.h>
#include <dev/rasops/rasops_masks.h>

static void	rasops1_copycols __P((void *, int, int, int, int));
static void	rasops1_erasecols __P((void *, int, int, int, long));
static void	rasops1_do_cursor __P((struct rasops_info *));
static void	rasops1_putchar __P((void *, int, int col, u_int, long));
#ifndef RASOPS_SMALL
static void	rasops1_putchar8 __P((void *, int, int col, u_int, long));
static void	rasops1_putchar16 __P((void *, int, int col, u_int, long));
#endif

/*
 * Initialize rasops_info struct for this colordepth.
 */
void
rasops1_init(ri)
	struct rasops_info *ri;
{

	switch (ri->ri_font->fontwidth) {
#ifndef RASOPS_SMALL
	case 8:
		ri->ri_ops.putchar = rasops1_putchar8;
		break;
	case 16:
		ri->ri_ops.putchar = rasops1_putchar16;
		break;
#endif
	default:
		ri->ri_ops.putchar = rasops1_putchar;
		break;
	}

	if ((ri->ri_font->fontwidth & 7) != 0) {
		ri->ri_ops.erasecols = rasops1_erasecols;
		ri->ri_ops.copycols = rasops1_copycols;
		ri->ri_do_cursor = rasops1_do_cursor;
	}
}

/*
 * Paint a single character. This is the generic version, this is ugly.
 */
static void
rasops1_putchar(cookie, row, col, uc, attr)
	void *cookie;
	int row, col;
	u_int uc;
	long attr;
{
	u_int fs, rs, fb, bg, fg, lmask, rmask;
	u_int32_t height, width;
	struct rasops_info *ri;
	int32_t *rp;
	u_char *fr;

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	col *= ri->ri_font->fontwidth;
	rp = (int32_t *)(ri->ri_bits + row * ri->ri_yscale + ((col >> 3) & ~3));
	height = ri->ri_font->fontheight;
	width = ri->ri_font->fontwidth;
	col = col & 31;
	rs = ri->ri_stride;

	bg = (attr & 0x000f0000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];
	fg = (attr & 0x0f000000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];

	/* If fg and bg match this becomes a space character */
	if (fg == bg || uc == ' ') {
		uc = (u_int)-1;
		fr = 0;		/* shutup gcc */
		fs = 0;		/* shutup gcc */
	} else {
		uc -= ri->ri_font->firstchar;
		fr = (u_char *)ri->ri_font->data + uc * ri->ri_fontscale;
		fs = ri->ri_font->stride;
	}

	/* Single word, one mask */
	if ((col + width) <= 32) {
		rmask = rasops_pmask[col][width];
		lmask = ~rmask;

		if (uc == (u_int)-1) {
			bg &= rmask;

			while (height--) {
				*rp = (*rp & lmask) | bg;
				DELTA(rp, rs, int32_t *);
			}
		} else {
			/* NOT fontbits if bg is white */
			if (bg) {
				while (height--) {
					fb = ~(fr[3] | (fr[2] << 8) |
					    (fr[1] << 16) | (fr[0] << 24));
					*rp = (*rp & lmask)
					    | (MBE(fb >> col) & rmask);

					fr += fs;
					DELTA(rp, rs, int32_t *);
				}
			} else {
				while (height--) {
					fb = (fr[3] | (fr[2] << 8) |
					    (fr[1] << 16) | (fr[0] << 24));
					*rp = (*rp & lmask)
					    | (MBE(fb >> col) & rmask);

					fr += fs;
					DELTA(rp, rs, int32_t *);
				}
			}
		}

		/* Do underline */
		if ((attr & 1) != 0) {
			DELTA(rp, -(ri->ri_stride << 1), int32_t *);
			*rp = (*rp & lmask) | (fg & rmask);
		}
	} else {
		lmask = ~rasops_lmask[col];
		rmask = ~rasops_rmask[(col + width) & 31];

		if (uc == (u_int)-1) {
			width = bg & ~rmask;
			bg = bg & ~lmask;

			while (height--) {
				rp[0] = (rp[0] & lmask) | bg;
				rp[1] = (rp[1] & rmask) | width;
				DELTA(rp, rs, int32_t *);
			}
		} else {
			width = 32 - col;

			/* NOT fontbits if bg is white */
			if (bg) {
				while (height--) {
					fb = ~(fr[3] | (fr[2] << 8) |
					    (fr[1] << 16) | (fr[0] << 24));

					rp[0] = (rp[0] & lmask)
					    | MBE((u_int)fb >> col);

					rp[1] = (rp[1] & rmask)
					    | (MBE((u_int)fb << width) & ~rmask);

					fr += fs;
					DELTA(rp, rs, int32_t *);
				}
			} else {
				while (height--) {
					fb = (fr[3] | (fr[2] << 8) |
					    (fr[1] << 16) | (fr[0] << 24));

					rp[0] = (rp[0] & lmask)
					    | MBE(fb >> col);

					rp[1] = (rp[1] & rmask)
					    | (MBE(fb << width) & ~rmask);

					fr += fs;
					DELTA(rp, rs, int32_t *);
				}
			}
		}

		/* Do underline */
		if ((attr & 1) != 0) {
			DELTA(rp, -(ri->ri_stride << 1), int32_t *);
			rp[0] = (rp[0] & lmask) | (fg & ~lmask);
			rp[1] = (rp[1] & rmask) | (fg & ~rmask);
		}
	}
}

#ifndef RASOPS_SMALL
/*
 * Paint a single character. This is for 8-pixel wide fonts.
 */
static void
rasops1_putchar8(cookie, row, col, uc, attr)
	void *cookie;
	int row, col;
	u_int uc;
	long attr;
{
	int height, fs, rs, bg, fg;
	struct rasops_info *ri;
	u_char *fr, *rp;

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	height = ri->ri_font->fontheight;
	rs = ri->ri_stride;

	bg = (attr & 0x000f0000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];
	fg = (attr & 0x0f000000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];

	/* If fg and bg match this becomes a space character */
	if (fg == bg || uc == ' ') {
		while (height--) {
			*rp = bg;
			rp += rs;
		}
	} else {
		uc -= ri->ri_font->firstchar;
		fr = (u_char *)ri->ri_font->data + uc * ri->ri_fontscale;
		fs = ri->ri_font->stride;

		/* NOT fontbits if bg is white */
		if (bg) {
			while (height--) {
				*rp = ~*fr;
				fr += fs;
				rp += rs;
			}
		} else {
			while (height--) {
				*rp = *fr;
				fr += fs;
				rp += rs;
			}
		}

	}

	/* Do underline */
	if ((attr & 1) != 0)
		rp[-(ri->ri_stride << 1)] = fg;
}

/*
 * Paint a single character. This is for 16-pixel wide fonts.
 */
static void
rasops1_putchar16(cookie, row, col, uc, attr)
	void *cookie;
	int row, col;
	u_int uc;
	long attr;
{
	int height, fs, rs, bg, fg;
	struct rasops_info *ri;
	u_char *fr, *rp;

	ri = (struct rasops_info *)cookie;

#ifdef RASOPS_CLIPPING
	/* Catches 'row < 0' case too */
	if ((unsigned)row >= (unsigned)ri->ri_rows)
		return;

	if ((unsigned)col >= (unsigned)ri->ri_cols)
		return;
#endif

	rp = ri->ri_bits + row * ri->ri_yscale + col * ri->ri_xscale;
	height = ri->ri_font->fontheight;
	rs = ri->ri_stride;

	bg = (attr & 0x000f0000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];
	fg = (attr & 0x0f000000) ? ri->ri_devcmap[1] : ri->ri_devcmap[0];

	/* If fg and bg match this becomes a space character */
	if (fg == bg || uc == ' ') {
		while (height--) {
			*(int16_t *)rp = bg;
			rp += rs;
		}
	} else {
		uc -= ri->ri_font->firstchar;
		fr = (u_char *)ri->ri_font->data + uc * ri->ri_fontscale;
		fs = ri->ri_font->stride;

		/* NOT fontbits if bg is white */
		if (bg) {
			while (height--) {
				rp[0] = ~fr[0];
				rp[1] = ~fr[1];
				fr += fs;
				rp += rs;
			}
		} else {
			while (height--) {
				rp[0] = fr[0];
				rp[1] = fr[1];
				fr += fs;
				rp += rs;
			}
		}
	}

	/* Do underline */
	if ((attr & 1) != 0)
		*(int16_t *)(rp - (ri->ri_stride << 1)) = fg;
}
#endif	/* !RASOPS_SMALL */

/*
 * Grab routines common to depths where (bpp < 8)
 */
#define NAME(ident)	rasops1_##ident
#define PIXEL_SHIFT	0

#include <dev/rasops/rasops_bitops.h>
