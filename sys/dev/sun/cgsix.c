/*	$NetBSD: cgsix.c,v 1.5.2.1 2001/09/26 19:55:01 nathanw Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)cgsix.c	8.4 (Berkeley) 1/21/94
 */

/*
 * color display (cgsix) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#ifdef DEBUG
#include <sys/proc.h>
#include <sys/syslog.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>

#include <dev/sun/btreg.h>
#include <dev/sun/btvar.h>
#include <dev/sun/cgsixreg.h>
#include <dev/sun/cgsixvar.h>
#include <dev/sun/pfourreg.h>

#ifdef RASTERCONSOLE
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsconsio.h>
#endif

#include <machine/conf.h>

static void	cg6_unblank(struct device *);

/* cdevsw prototypes */
cdev_decl(cgsix);

extern struct cfdriver cgsix_cd;

/* frame buffer generic driver */
static struct fbdriver cg6_fbdriver = {
	cg6_unblank, cgsixopen, cgsixclose, cgsixioctl, cgsixpoll, cgsixmmap
};

static void cg6_reset (struct cgsix_softc *);
static void cg6_loadcmap (struct cgsix_softc *, int, int);
static void cg6_loadomap (struct cgsix_softc *);
static void cg6_setcursor (struct cgsix_softc *);/* set position */
static void cg6_loadcursor (struct cgsix_softc *);/* set shape */

#ifdef RASTERCONSOLE
int cgsix_use_rasterconsole = 1;

/*
 * cg6 accelerated console routines.
 *
 * Note that buried in this code in several places is the assumption
 * that pixels are exactly one byte wide.  Since this is cg6-specific
 * code, this seems safe.  This assumption resides in things like the
 * use of ri_emuwidth without messing around with ri_pelbytes, or the
 * assumption that ri_font->fontwidth is the right thing to multiply
 * character-cell counts by to get byte counts.
 */

/*
 * Magic values for blitter
 */

/* Values for the mode register */
#define CG6_MODE	(						\
	  0x00200000 /* GX_BLIT_SRC */					\
	| 0x00020000 /* GX_MODE_COLOR8 */				\
	| 0x00008000 /* GX_DRAW_RENDER */				\
	| 0x00002000 /* GX_BWRITE0_ENABLE */				\
	| 0x00001000 /* GX_BWRITE1_DISABLE */				\
	| 0x00000200 /* GX_BREAD_0 */					\
	| 0x00000080 /* GX_BDISP_0 */					\
)
#define CG6_MODE_MASK	(						\
	  0x00300000 /* GX_BLIT_ALL */					\
	| 0x00060000 /* GX_MODE_ALL */					\
	| 0x00018000 /* GX_DRAW_ALL */					\
	| 0x00006000 /* GX_BWRITE0_ALL */				\
	| 0x00001800 /* GX_BWRITE1_ALL */				\
	| 0x00000600 /* GX_BREAD_ALL */					\
	| 0x00000180 /* GX_BDISP_ALL */					\
)

/* Value for the alu register for screen-to-screen copies */
#define CG6_ALU_COPY	(						\
	  0x80000000 /* GX_PLANE_ONES (ignore planemask register) */	\
	| 0x20000000 /* GX_PIXEL_ONES (ignore pixelmask register) */	\
	| 0x00800000 /* GX_ATTR_SUPP (function unknown) */		\
	| 0x00000000 /* GX_RAST_BOOL (function unknown) */		\
	| 0x00000000 /* GX_PLOT_PLOT (function unknown) */		\
	| 0x08000000 /* GX_PATTERN_ONES (ignore pattern) */		\
	| 0x01000000 /* GX_POLYG_OVERLAP (unsure - handle overlap?) */	\
	| 0x0000cccc /* ALU = src */					\
)

/* Value for the alu register for region fills */
#define CG6_ALU_FILL	(						\
	  0x80000000 /* GX_PLANE_ONES (ignore planemask register) */	\
	| 0x20000000 /* GX_PIXEL_ONES (ignore pixelmask register) */	\
	| 0x00800000 /* GX_ATTR_SUPP (function unknown) */		\
	| 0x00000000 /* GX_RAST_BOOL (function unknown) */		\
	| 0x00000000 /* GX_PLOT_PLOT (function unknown) */		\
	| 0x08000000 /* GX_PATTERN_ONES (ignore pattern) */		\
	| 0x01000000 /* GX_POLYG_OVERLAP (unsure - handle overlap?) */	\
	| 0x0000ff00 /* ALU = fg color */				\
)

/* Value for the alu register for toggling an area */
#define CG6_ALU_FLIP	(						\
	  0x80000000 /* GX_PLANE_ONES (ignore planemask register) */	\
	| 0x20000000 /* GX_PIXEL_ONES (ignore pixelmask register) */	\
	| 0x00800000 /* GX_ATTR_SUPP (function unknown) */		\
	| 0x00000000 /* GX_RAST_BOOL (function unknown) */		\
	| 0x00000000 /* GX_PLOT_PLOT (function unknown) */		\
	| 0x08000000 /* GX_PATTERN_ONES (ignore pattern) */		\
	| 0x01000000 /* GX_POLYG_OVERLAP (unsure - handle overlap?) */	\
	| 0x00005555 /* ALU = ~dst */					\
)

/*
 * Wait for a blit to finish.
 * 0x8000000 bit: function unknown; 0x20000000 bit: GX_BLT_INPROGRESS
 */
#define CG6_BLIT_WAIT(fbc) do {						\
	while (((fbc)->fbc_blit & 0xa0000000) == 0xa0000000)		\
		/*EMPTY*/;						\
} while (0)

/*
 * Wait for a drawing operation to finish, or at least get queued.
 * 0x8000000 bit: function unknown; 0x20000000 bit: GX_FULL
 */
#define CG6_DRAW_WAIT(fbc) do {						\
       	while (((fbc)->fbc_draw & 0xa0000000) == 0xa0000000)		\
		/*EMPTY*/;						\
} while (0)

/*
 * Wait for the whole engine to go idle.  This may not matter in our case;
 * I'm not sure whether blits are actually queued or not.  It more likely
 * is intended for lines and such that do get queued.
 * 0x10000000 bit: GX_INPROGRESS
 */
#define CG6_DRAIN(fbc) do {						\
	while ((fbc)->fbc_s & 0x10000000)				\
		/*EMPTY*/;						\
} while (0)

static void cg6_ras_init(struct cgsix_softc *);
static void cg6_ras_copyrows(void *, int, int, int);
static void cg6_ras_copycols(void *, int, int, int, int);
static void cg6_ras_erasecols(void *, int, int, int, long int);
static void cg6_ras_eraserows(void *, int, int, long int);
static void cg6_ras_do_cursor(struct rasops_info *);

static void
cg6_ras_init(struct cgsix_softc *sc)
{
	volatile struct cg6_fbc *fbc = sc->sc_fbc;

	CG6_DRAIN(fbc);
	fbc->fbc_mode &= ~CG6_MODE_MASK;
	fbc->fbc_mode |= CG6_MODE;
}

static void
cg6_ras_copyrows(void *cookie, int src, int dst, int n)
{
	struct rasops_info *ri;
	volatile struct cg6_fbc *fbc;

	ri = cookie;
	if (dst == src)
		return;
	if (src < 0) {
		n += src;
		src = 0;
	}
	if (src+n > ri->ri_rows)
		n = ri->ri_rows - src;
	if (dst < 0) {
		n += dst;
		dst = 0;
	}
	if (dst+n > ri->ri_rows)
		n = ri->ri_rows - dst;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontheight;
	src *= ri->ri_font->fontheight;
	dst *= ri->ri_font->fontheight;
	fbc = ((struct cgsix_softc *)ri->ri_hw)->sc_fbc;
	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = CG6_ALU_COPY;
	fbc->fbc_x0 = ri->ri_xorigin;
	fbc->fbc_y0 = ri->ri_yorigin + src;
	fbc->fbc_x1 = ri->ri_xorigin + ri->ri_emuwidth - 1;
	fbc->fbc_y1 = ri->ri_yorigin + src + n - 1;
	fbc->fbc_x2 = ri->ri_xorigin;
	fbc->fbc_y2 = ri->ri_yorigin + dst;
	fbc->fbc_x3 = ri->ri_xorigin + ri->ri_emuwidth - 1;
	fbc->fbc_y3 = ri->ri_yorigin + dst + n - 1;
	CG6_BLIT_WAIT(fbc);
	CG6_DRAIN(fbc);
}

static void
cg6_ras_copycols(void *cookie, int row, int src, int dst, int n)
{
	struct rasops_info *ri;
	volatile struct cg6_fbc *fbc;

	ri = cookie;
	if (dst == src)
		return;
	if ((row < 0) || (row >= ri->ri_rows))
		return;
	if (src < 0) {
		n += src;
		src = 0;
	}
	if (src+n > ri->ri_cols)
		n = ri->ri_cols - src;
	if (dst < 0) {
		n += dst;
		dst = 0;
	}
	if (dst+n > ri->ri_cols)
		n = ri->ri_cols - dst;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontwidth;
	src *= ri->ri_font->fontwidth;
	dst *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;
	fbc = ((struct cgsix_softc *)ri->ri_hw)->sc_fbc;
	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = CG6_ALU_COPY;
	fbc->fbc_x0 = ri->ri_xorigin + src;
	fbc->fbc_y0 = ri->ri_yorigin + row;
	fbc->fbc_x1 = ri->ri_xorigin + src + n - 1;
	fbc->fbc_y1 = ri->ri_yorigin + row + ri->ri_font->fontheight - 1;
	fbc->fbc_x2 = ri->ri_xorigin + dst;
	fbc->fbc_y2 = ri->ri_yorigin + row;
	fbc->fbc_x3 = ri->ri_xorigin + dst + n - 1;
	fbc->fbc_y3 = ri->ri_yorigin + row + ri->ri_font->fontheight - 1;
	CG6_BLIT_WAIT(fbc);
	CG6_DRAIN(fbc);
}

static void
cg6_ras_erasecols(void *cookie, int row, int col, int n, long int attr)
{
	struct rasops_info *ri;
	volatile struct cg6_fbc *fbc;

	ri = cookie;
	if ((row < 0) || (row >= ri->ri_rows))
		return;
	if (col < 0) {
		n += col;
		col = 0;
	}
	if (col+n > ri->ri_cols)
		n = ri->ri_cols - col;
	if (n <= 0)
		return;
	n *= ri->ri_font->fontwidth;
	col *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;
	fbc = ((struct cgsix_softc *)ri->ri_hw)->sc_fbc;
	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = CG6_ALU_FILL;
	fbc->fbc_fg = ri->ri_devcmap[(attr >> 16) & 0xf];
	fbc->fbc_arecty = ri->ri_yorigin + row;
	fbc->fbc_arectx = ri->ri_xorigin + col;
	fbc->fbc_arecty = ri->ri_yorigin + row + ri->ri_font->fontheight - 1;
	fbc->fbc_arectx = ri->ri_xorigin + col + n - 1;
	CG6_DRAW_WAIT(fbc);
	CG6_DRAIN(fbc);
}

static void
cg6_ras_eraserows(void *cookie, int row, int n, long int attr)
{
	struct rasops_info *ri;
	volatile struct cg6_fbc *fbc;

	ri = cookie;
	if (row < 0) {
		n += row;
		row = 0;
	}
	if (row+n > ri->ri_rows)
		n = ri->ri_rows - row;
	if (n <= 0)
		return;
	fbc = ((struct cgsix_softc *)ri->ri_hw)->sc_fbc;
	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = CG6_ALU_FILL;
	fbc->fbc_fg = ri->ri_devcmap[(attr >> 16) & 0xf];
	if ((n == ri->ri_rows) && (ri->ri_flg & RI_FULLCLEAR)) {
		fbc->fbc_arecty = 0;
		fbc->fbc_arectx = 0;
		fbc->fbc_arecty = ri->ri_height - 1;
		fbc->fbc_arectx = ri->ri_width - 1;
	} else {
		row *= ri->ri_font->fontheight;
		fbc->fbc_arecty = ri->ri_yorigin + row;
		fbc->fbc_arectx = ri->ri_xorigin;
		fbc->fbc_arecty = ri->ri_yorigin + row + (n * ri->ri_font->fontheight) - 1;
		fbc->fbc_arectx = ri->ri_xorigin + ri->ri_emuwidth - 1;
	}
	CG6_DRAW_WAIT(fbc);
	CG6_DRAIN(fbc);
}

/*
 * Really want something more like fg^bg here, but that would be more
 * or less impossible to migrate to colors.  So we hope there's
 * something not too inappropriate in the colormap...besides, it's what
 * the non-accelerated code did. :-)
 */
static void
cg6_ras_do_cursor(struct rasops_info *ri)
{
	volatile struct cg6_fbc *fbc;
	int row;
	int col;

	row = ri->ri_crow * ri->ri_font->fontheight;
	col = ri->ri_ccol * ri->ri_font->fontwidth;
	fbc = ((struct cgsix_softc *)ri->ri_hw)->sc_fbc;
	fbc->fbc_clip = 0;
	fbc->fbc_s = 0;
	fbc->fbc_offx = 0;
	fbc->fbc_offy = 0;
	fbc->fbc_clipminx = 0;
	fbc->fbc_clipminy = 0;
	fbc->fbc_clipmaxx = ri->ri_width - 1;
	fbc->fbc_clipmaxy = ri->ri_height - 1;
	fbc->fbc_alu = CG6_ALU_FLIP;
	fbc->fbc_arecty = ri->ri_yorigin + row;
	fbc->fbc_arectx = ri->ri_xorigin + col;
	fbc->fbc_arecty = ri->ri_yorigin + row + ri->ri_font->fontheight - 1;
	fbc->fbc_arectx = ri->ri_xorigin + col + ri->ri_font->fontwidth - 1;
	CG6_DRAW_WAIT(fbc);
	CG6_DRAIN(fbc);
}
#endif /* RASTERCONSOLE */

void
cg6attach(sc, name, isconsole)
	struct cgsix_softc *sc;
	char *name;
	int isconsole;
{
	struct fbdevice *fb = &sc->sc_fb;

	fb->fb_driver = &cg6_fbdriver;

	/* Don't have to map the pfour register on the cgsix. */
	fb->fb_pfour = NULL;

	fb->fb_type.fb_cmsize = 256;
	fb->fb_type.fb_size = fb->fb_type.fb_height * fb->fb_linebytes;
	printf(": %s, %d x %d", name,
	       fb->fb_type.fb_width, fb->fb_type.fb_height);

	sc->sc_fhcrev = (*sc->sc_fhc >> FHC_REV_SHIFT) &
			(FHC_REV_MASK >> FHC_REV_SHIFT);

	printf(", rev %d", sc->sc_fhcrev);

	/* reset cursor & frame buffer controls */
	cg6_reset(sc);

	/* enable video */
	sc->sc_thc->thc_misc |= THC_MISC_VIDEN;

	if (isconsole) {
		printf(" (console)");
#ifdef RASTERCONSOLE
		if (cgsix_use_rasterconsole) {
			fbrcons_init(&sc->sc_fb);
			sc->sc_fb.fb_rinfo.ri_hw = sc;
			sc->sc_fb.fb_rinfo.ri_ops.copyrows = cg6_ras_copyrows;
			sc->sc_fb.fb_rinfo.ri_ops.copycols = cg6_ras_copycols;
			sc->sc_fb.fb_rinfo.ri_ops.erasecols = cg6_ras_erasecols;
			sc->sc_fb.fb_rinfo.ri_ops.eraserows = cg6_ras_eraserows;
			sc->sc_fb.fb_rinfo.ri_do_cursor = cg6_ras_do_cursor;
			cg6_ras_init(sc);
		}
#endif
	}

	printf("\n");
	fb_attach(&sc->sc_fb, isconsole);
}


int
cgsixopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = minor(dev);

	if (unit >= cgsix_cd.cd_ndevs || cgsix_cd.cd_devs[unit] == NULL)
		return (ENXIO);
	return (0);
}

int
cgsixclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	struct cgsix_softc *sc = cgsix_cd.cd_devs[minor(dev)];

	cg6_reset(sc);

	/* (re-)initialize the default color map */
	bt_initcmap(&sc->sc_cmap, 256);
	cg6_loadcmap(sc, 0, 256);

	return (0);
}

int
cgsixioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct cgsix_softc *sc = cgsix_cd.cd_devs[minor(dev)];
	u_int count;
	int v, error;
	union cursor_cmap tcm;

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGATTR:
#define fba ((struct fbgattr *)data)
		fba->real_type = sc->sc_fb.fb_type.fb_type;
		fba->owner = 0;		/* XXX ??? */
		fba->fbtype = sc->sc_fb.fb_type;
		fba->sattr.flags = 0;
		fba->sattr.emu_type = sc->sc_fb.fb_type.fb_type;
		fba->sattr.dev_specific[0] = -1;
		fba->emu_types[0] = sc->sc_fb.fb_type.fb_type;
		fba->emu_types[1] = -1;
#undef fba
		break;

	case FBIOGETCMAP:
#define	p ((struct fbcmap *)data)
		return (bt_getcmap(p, &sc->sc_cmap, 256, 1));

	case FBIOPUTCMAP:
		/* copy to software map */
		error = bt_putcmap(p, &sc->sc_cmap, 256, 1);
		if (error)
			return (error);
		/* now blast them into the chip */
		/* XXX should use retrace interrupt */
		cg6_loadcmap(sc, p->index, p->count);
#undef p
		break;

	case FBIOGVIDEO:
		*(int *)data = sc->sc_blanked;
		break;

	case FBIOSVIDEO:
		if (*(int *)data)
			cg6_unblank(&sc->sc_dev);
		else if (!sc->sc_blanked) {
			sc->sc_blanked = 1;
			sc->sc_thc->thc_misc &= ~THC_MISC_VIDEN;
		}
		break;

/* these are for both FBIOSCURSOR and FBIOGCURSOR */
#define p ((struct fbcursor *)data)
#define cc (&sc->sc_cursor)

	case FBIOGCURSOR:
		/* do not quite want everything here... */
		p->set = FB_CUR_SETALL;	/* close enough, anyway */
		p->enable = cc->cc_enable;
		p->pos = cc->cc_pos;
		p->hot = cc->cc_hot;
		p->size = cc->cc_size;

		/* begin ugh ... can we lose some of this crap?? */
		if (p->image != NULL) {
			count = cc->cc_size.y * 32 / NBBY;
			error = copyout((caddr_t)cc->cc_bits[1],
			    (caddr_t)p->image, count);
			if (error)
				return (error);
			error = copyout((caddr_t)cc->cc_bits[0],
			    (caddr_t)p->mask, count);
			if (error)
				return (error);
		}
		if (p->cmap.red != NULL) {
			error = bt_getcmap(&p->cmap,
			    (union bt_cmap *)&cc->cc_color, 2, 1);
			if (error)
				return (error);
		} else {
			p->cmap.index = 0;
			p->cmap.count = 2;
		}
		/* end ugh */
		break;

	case FBIOSCURSOR:
		/*
		 * For setcmap and setshape, verify parameters, so that
		 * we do not get halfway through an update and then crap
		 * out with the software state screwed up.
		 */
		v = p->set;
		if (v & FB_CUR_SETCMAP) {
			/*
			 * This use of a temporary copy of the cursor
			 * colormap is not terribly efficient, but these
			 * copies are small (8 bytes)...
			 */
			tcm = cc->cc_color;
			error = bt_putcmap(&p->cmap, (union bt_cmap *)&tcm, 2, 1);
			if (error)
				return (error);
		}
		if (v & FB_CUR_SETSHAPE) {
			if ((u_int)p->size.x > 32 || (u_int)p->size.y > 32)
				return (EINVAL);
			count = p->size.y * 32 / NBBY;
			if (!uvm_useracc(p->image, count, B_READ) ||
			    !uvm_useracc(p->mask, count, B_READ))
				return (EFAULT);
		}

		/* parameters are OK; do it */
		if (v & (FB_CUR_SETCUR | FB_CUR_SETPOS | FB_CUR_SETHOT)) {
			if (v & FB_CUR_SETCUR)
				cc->cc_enable = p->enable;
			if (v & FB_CUR_SETPOS)
				cc->cc_pos = p->pos;
			if (v & FB_CUR_SETHOT)
				cc->cc_hot = p->hot;
			cg6_setcursor(sc);
		}
		if (v & FB_CUR_SETCMAP) {
			cc->cc_color = tcm;
			cg6_loadomap(sc); /* XXX defer to vertical retrace */
		}
		if (v & FB_CUR_SETSHAPE) {
			cc->cc_size = p->size;
			count = p->size.y * 32 / NBBY;
			bzero((caddr_t)cc->cc_bits, sizeof cc->cc_bits);
			copyin(p->mask, (caddr_t)cc->cc_bits[0], count);
			copyin(p->image, (caddr_t)cc->cc_bits[1], count);
			cg6_loadcursor(sc);
		}
		break;

#undef p
#undef cc

	case FBIOGCURPOS:
		*(struct fbcurpos *)data = sc->sc_cursor.cc_pos;
		break;

	case FBIOSCURPOS:
		sc->sc_cursor.cc_pos = *(struct fbcurpos *)data;
		cg6_setcursor(sc);
		break;

	case FBIOGCURMAX:
		/* max cursor size is 32x32 */
		((struct fbcurpos *)data)->x = 32;
		((struct fbcurpos *)data)->y = 32;
		break;

	default:
#ifdef DEBUG
		log(LOG_NOTICE, "cgsixioctl(0x%lx) (%s[%d])\n", cmd,
		    p->p_comm, p->p_pid);
#endif
		return (ENOTTY);
	}
	return (0);
}

int
cgsixpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{

	return (seltrue(dev, events, p));
}

/*
 * Clean up hardware state (e.g., after bootup or after X crashes).
 */
static void
cg6_reset(sc)
	struct cgsix_softc *sc;
{
	volatile struct cg6_tec_xxx *tec;
	int fhc;
	volatile struct bt_regs *bt;

	/* hide the cursor, just in case */
	sc->sc_thc->thc_cursxy = (THC_CURSOFF << 16) | THC_CURSOFF;

	/* turn off frobs in transform engine (makes X11 work) */
	tec = sc->sc_tec;
	tec->tec_mv = 0;
	tec->tec_clip = 0;
	tec->tec_vdc = 0;

	/* take care of hardware bugs in old revisions */
	if (sc->sc_fhcrev < 5) {
		/*
		 * Keep current resolution; set cpu to 68020, set test
		 * window (size 1Kx1K), and for rev 1, disable dest cache.
		 */
		fhc = (*sc->sc_fhc & FHC_RES_MASK) | FHC_CPU_68020 |
		    FHC_TEST |
		    (11 << FHC_TESTX_SHIFT) | (11 << FHC_TESTY_SHIFT);
		if (sc->sc_fhcrev < 2)
			fhc |= FHC_DST_DISABLE;
		*sc->sc_fhc = fhc;
	}

	/* Enable cursor in Brooktree DAC. */
	bt = sc->sc_bt;
	bt->bt_addr = 0x06 << 24;
	bt->bt_ctrl |= 0x03 << 24;
}

static void
cg6_setcursor(sc)
	struct cgsix_softc *sc;
{

	/* we need to subtract the hot-spot value here */
#define COORD(f) (sc->sc_cursor.cc_pos.f - sc->sc_cursor.cc_hot.f)
	sc->sc_thc->thc_cursxy = sc->sc_cursor.cc_enable ?
	    ((COORD(x) << 16) | (COORD(y) & 0xffff)) :
	    (THC_CURSOFF << 16) | THC_CURSOFF;
#undef COORD
}

static void
cg6_loadcursor(sc)
	struct cgsix_softc *sc;
{
	volatile struct cg6_thc *thc;
	u_int edgemask, m;
	int i;

	/*
	 * Keep the top size.x bits.  Here we *throw out* the top
	 * size.x bits from an all-one-bits word, introducing zeros in
	 * the top size.x bits, then invert all the bits to get what
	 * we really wanted as our mask.  But this fails if size.x is
	 * 32---a sparc uses only the low 5 bits of the shift count---
	 * so we have to special case that.
	 */
	edgemask = ~0;
	if (sc->sc_cursor.cc_size.x < 32)
		edgemask = ~(edgemask >> sc->sc_cursor.cc_size.x);
	thc = sc->sc_thc;
	for (i = 0; i < 32; i++) {
		m = sc->sc_cursor.cc_bits[0][i] & edgemask;
		thc->thc_cursmask[i] = m;
		thc->thc_cursbits[i] = m & sc->sc_cursor.cc_bits[1][i];
	}
}

/*
 * Load a subset of the current (new) colormap into the color DAC.
 */
static void
cg6_loadcmap(sc, start, ncolors)
	struct cgsix_softc *sc;
	int start, ncolors;
{
	volatile struct bt_regs *bt;
	u_int *ip, i;
	int count;

	ip = &sc->sc_cmap.cm_chip[BT_D4M3(start)];	/* start/4 * 3 */
	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	bt = sc->sc_bt;
	bt->bt_addr = BT_D4M4(start) << 24;
	while (--count >= 0) {
		i = *ip++;
		/* hardware that makes one want to pound boards with hammers */
		bt->bt_cmap = i;
		bt->bt_cmap = i << 8;
		bt->bt_cmap = i << 16;
		bt->bt_cmap = i << 24;
	}
}

/*
 * Load the cursor (overlay `foreground' and `background') colors.
 */
static void
cg6_loadomap(sc)
	struct cgsix_softc *sc;
{
	volatile struct bt_regs *bt;
	u_int i;

	bt = sc->sc_bt;
	bt->bt_addr = 0x01 << 24;	/* set background color */
	i = sc->sc_cursor.cc_color.cm_chip[0];
	bt->bt_omap = i;		/* R */
	bt->bt_omap = i << 8;		/* G */
	bt->bt_omap = i << 16;		/* B */

	bt->bt_addr = 0x03 << 24;	/* set foreground color */
	bt->bt_omap = i << 24;		/* R */
	i = sc->sc_cursor.cc_color.cm_chip[1];
	bt->bt_omap = i;		/* G */
	bt->bt_omap = i << 8;		/* B */
}

static void
cg6_unblank(dev)
	struct device *dev;
{
	struct cgsix_softc *sc = (struct cgsix_softc *)dev;

	if (sc->sc_blanked) {
		sc->sc_blanked = 0;
		sc->sc_thc->thc_misc |= THC_MISC_VIDEN;
	}
}

/* XXX the following should be moved to a "user interface" header */
/*
 * Base addresses at which users can mmap() the various pieces of a cg6.
 * Note that although the Brooktree color registers do not occupy 8K,
 * the X server dies if we do not allow it to map 8K there (it just maps
 * from 0x70000000 forwards, as a contiguous chunk).
 */
#define	CG6_USER_FBC	0x70000000
#define	CG6_USER_TEC	0x70001000
#define	CG6_USER_BTREGS	0x70002000
#define	CG6_USER_FHC	0x70004000
#define	CG6_USER_THC	0x70005000
#define	CG6_USER_ROM	0x70006000
#define	CG6_USER_RAM	0x70016000
#define	CG6_USER_DHC	0x80000000

struct mmo {
	u_long	mo_uaddr;	/* user (virtual) address */
	u_long	mo_size;	/* size, or 0 for video ram size */
	u_long	mo_physoff;	/* offset from sc_physadr */
};

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 *
 * XXX	needs testing against `demanding' applications (e.g., aviator)
 */
paddr_t
cgsixmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	struct cgsix_softc *sc = cgsix_cd.cd_devs[minor(dev)];
	struct mmo *mo;
	u_int u, sz;
	static struct mmo mmo[] = {
		{ CG6_USER_RAM, 0, CGSIX_RAM_OFFSET },

		/* do not actually know how big most of these are! */
		{ CG6_USER_FBC, 1, CGSIX_FBC_OFFSET },
		{ CG6_USER_TEC, 1, CGSIX_TEC_OFFSET },
		{ CG6_USER_BTREGS, 8192 /* XXX */, CGSIX_BT_OFFSET },
		{ CG6_USER_FHC, 1, CGSIX_FHC_OFFSET },
		{ CG6_USER_THC, sizeof(struct cg6_thc), CGSIX_THC_OFFSET },
		{ CG6_USER_ROM, 65536, CGSIX_ROM_OFFSET },
		{ CG6_USER_DHC, 1, CGSIX_DHC_OFFSET },
	};
#define NMMO (sizeof mmo / sizeof *mmo)

	if (off & PGOFSET)
		panic("cgsixmmap");

	/*
	 * Entries with size 0 map video RAM (i.e., the size in fb data).
	 *
	 * Since we work in pages, the fact that the map offset table's
	 * sizes are sometimes bizarre (e.g., 1) is effectively ignored:
	 * one byte is as good as one page.
	 */
	for (mo = mmo; mo < &mmo[NMMO]; mo++) {
		if ((u_long)off < mo->mo_uaddr)
			continue;
		u = off - mo->mo_uaddr;
		sz = mo->mo_size ? mo->mo_size : sc->sc_fb.fb_type.fb_size;
		if (u < sz) {
			return (bus_space_mmap(sc->sc_bustag,
				sc->sc_paddr, u+mo->mo_physoff,
				prot, BUS_SPACE_MAP_LINEAR));
		}
	}

#ifdef DEBUG
	{
	  struct proc *p = curproc;	/* XXX */
	  log(LOG_NOTICE, "cgsixmmap(0x%llx) (%s[%d])\n",
		(long long)off, p->p_comm, p->p_pid);
	}
#endif
	return (-1);	/* not a user-map offset */
}
