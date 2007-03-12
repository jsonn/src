/*	$NetBSD: grf_rb.c,v 1.35.10.1 2007/03/12 05:47:43 rmind Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*
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
 *
 * from: Utah $Hdr: grf_rb.c 1.15 93/08/13$
 *
 *	@(#)grf_rb.c	8.4 (Berkeley) 1/12/94
 */
/*
 * Copyright (c) 1988 University of Utah.
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
 * from: Utah $Hdr: grf_rb.c 1.15 93/08/13$
 *
 *	@(#)grf_rb.c	8.4 (Berkeley) 1/12/94
 */

/*
 * Graphics routines for the Renaissance, HP98720 Graphics system.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: grf_rb.c,v 1.35.10.1 2007/03/12 05:47:43 rmind Exp $");

#include "opt_compat_hpux.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <dev/cons.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>
#include <hp300/dev/intiovar.h>

#include <hp300/dev/grfioctl.h>
#include <hp300/dev/grfvar.h>
#include <hp300/dev/grfreg.h>
#include <hp300/dev/grf_rbreg.h>

#include <hp300/dev/itevar.h>
#include <hp300/dev/itereg.h>

#include "ite.h"

static int	rb_init(struct grf_data *gp, int, uint8_t *);
static int	rb_mode(struct grf_data *gp, int, void *);

static int	rbox_intio_match(struct device *, struct cfdata *, void *);
static void	rbox_intio_attach(struct device *, struct device *, void *);

static int	rbox_dio_match(struct device *, struct cfdata *, void *);
static void	rbox_dio_attach(struct device *, struct device *, void *);

int	rboxcnattach(bus_space_tag_t, bus_addr_t, int);

CFATTACH_DECL(rbox_intio, sizeof(struct grfdev_softc),
    rbox_intio_match, rbox_intio_attach, NULL, NULL);

CFATTACH_DECL(rbox_dio, sizeof(struct grfdev_softc),
    rbox_dio_match, rbox_dio_attach, NULL, NULL);

/* Renaissance grf switch */
static struct grfsw rbox_grfsw = {
	GID_RENAISSANCE, GRFRBOX, "renaissance", rb_init, rb_mode
};

static int rbconscode;
static void *rbconaddr;

#if NITE > 0
static void	rbox_init(struct ite_data *);
static void	rbox_deinit(struct ite_data *);
static void	rbox_putc(struct ite_data *, int, int, int, int);
static void	rbox_cursor(struct ite_data *, int);
static void	rbox_clear(struct ite_data *, int, int, int, int);
static void	rbox_scroll(struct ite_data *, int, int, int, int);
static void	rbox_windowmove(struct ite_data *, int, int, int, int,
			int, int, int);

/* Renaissance ite switch */
static struct itesw rbox_itesw = {
	rbox_init, rbox_deinit, rbox_clear, rbox_putc,
	rbox_cursor, rbox_scroll, ite_readbyte, ite_writeglyph
};
#endif /* NITE > 0 */

static int
rbox_intio_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct intio_attach_args *ia = aux;
	struct grfreg *grf;

	if (strcmp("fb",ia->ia_modname) != 0)
		return 0;

	if (badaddr((void *)ia->ia_addr))
		return 0;

	grf = (struct grfreg *)ia->ia_addr;

	if (grf->gr_id == DIO_DEVICE_ID_FRAMEBUFFER &&
	    grf->gr_id2 == DIO_DEVICE_SECID_RENASSIANCE) {
		return 1;
	}

	return 0;
}

static void
rbox_intio_attach(struct device *parent, struct device *self, void *aux)
{
	struct grfdev_softc *sc = (struct grfdev_softc *)self;
	struct intio_attach_args *ia = aux;
	void *grf;

	grf = (void *)ia->ia_addr;
	sc->sc_scode = -1;	/* XXX internal i/o */

	sc->sc_isconsole = (sc->sc_scode == rbconscode);
	grfdev_attach(sc, rb_init, grf, &rbox_grfsw);
}

static int
rbox_dio_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct dio_attach_args *da = aux;

	if (da->da_id == DIO_DEVICE_ID_FRAMEBUFFER &&
	    da->da_secid == DIO_DEVICE_SECID_RENASSIANCE)
		return 1;

	return 0;
}

static void
rbox_dio_attach(struct device *parent, struct device *self, void *aux)
{
	struct grfdev_softc *sc = (struct grfdev_softc *)self;
	struct dio_attach_args *da = aux;
	bus_space_handle_t bsh;
	void *grf;

	sc->sc_scode = da->da_scode;
	if (sc->sc_scode == rbconscode)
		grf = rbconaddr;
	else {
		if (bus_space_map(da->da_bst, da->da_addr, da->da_size,
		    0, &bsh)) {
			printf("%s: can't map framebuffer\n",
			    sc->sc_dev.dv_xname);
			return;
		}
		grf = bus_space_vaddr(da->da_bst, bsh);
	}

	sc->sc_isconsole = (sc->sc_scode == rbconscode);
	grfdev_attach(sc, rb_init, grf, &rbox_grfsw);
}

/*
 * Initialize hardware.
 * Must point g_display at a grfinfo structure describing the hardware.
 * Returns 0 if hardware not present, non-zero ow.
 */
static int
rb_init(struct grf_data *gp, int scode, uint8_t *addr)
{
	struct rboxfb *rbp;
	struct grfinfo *gi = &gp->g_display;
	int fboff;

	/*
	 * If the console has been initialized, and it was us, there's
	 * no need to repeat this.
	 */
	if (scode != rbconscode) {
		rbp = (struct rboxfb *)addr;
		if (ISIIOVA(addr))
			gi->gd_regaddr = (void *)IIOP(addr);
		else
			gi->gd_regaddr = dio_scodetopa(scode);
		gi->gd_regsize = 0x20000;
		gi->gd_fbwidth = (rbp->fbwmsb << 8) | rbp->fbwlsb;
		gi->gd_fbheight = (rbp->fbhmsb << 8) | rbp->fbhlsb;
		gi->gd_fbsize = gi->gd_fbwidth * gi->gd_fbheight;
		fboff = (rbp->fbomsb << 8) | rbp->fbolsb;
		gi->gd_fbaddr = (void *)(*(addr + fboff) << 16);
		if ((vaddr_t)gi->gd_regaddr >= DIOIIBASE) {
			/*
			 * For DIO II space the fbaddr just computed is
			 * the offset from the select code base (regaddr)
			 * of the framebuffer.  Hence it is also implicitly
			 * the size of the set.
			 */
			gi->gd_regsize = (int)gi->gd_fbaddr;
			gi->gd_fbaddr += (int)gi->gd_regaddr;
			gp->g_regkva = addr;
			gp->g_fbkva = addr + gi->gd_regsize;
		} else {
			/*
			 * For DIO space we need to map the separate
			 * framebuffer.
			 */
			gp->g_regkva = addr;
			gp->g_fbkva = iomap(gi->gd_fbaddr, gi->gd_fbsize);
		}
		gi->gd_dwidth = (rbp->dwmsb << 8) | rbp->dwlsb;
		gi->gd_dheight = (rbp->dwmsb << 8) | rbp->dwlsb;
		gi->gd_planes = 0;	/* ?? */
		gi->gd_colors = 256;
	}
	return 1;
}

/*
 * Change the mode of the display.
 * Right now all we can do is grfon/grfoff.
 * Return a UNIX error number or 0 for success.
 */
static int
rb_mode(struct grf_data *gp, int cmd, void *data)
{
	struct rboxfb *rbp;
	int error = 0;

	rbp = (struct rboxfb *) gp->g_regkva;
	switch (cmd) {
	/*
	 * The minimal info here is from the Renaissance X driver.
	 */
	case GM_GRFON:
	case GM_GRFOFF:
		break;

	case GM_GRFOVON:
		rbp->write_enable = 0;
		rbp->opwen = 0xF;
		rbp->drive = 0x10;
		break;

	case GM_GRFOVOFF:
		rbp->opwen = 0;
		rbp->write_enable = 0xffffffff;
		rbp->drive = 0x01;
		break;

	/*
	 * Remember UVA of mapping for GCDESCRIBE.
	 * XXX this should be per-process.
	 */
	case GM_MAP:
		gp->g_data = data;
		break;

	case GM_UNMAP:
		gp->g_data = 0;
		break;

#ifdef COMPAT_HPUX
	case GM_DESCRIBE:
	{
		struct grf_fbinfo *fi = (struct grf_fbinfo *)data;
		struct grfinfo *gi = &gp->g_display;
		int i;

		/* feed it what HP-UX expects */
		fi->id = gi->gd_id;
		fi->mapsize = gi->gd_fbsize;
		fi->dwidth = gi->gd_dwidth;
		fi->dlength = gi->gd_dheight;
		fi->width = gi->gd_fbwidth;
		fi->length = gi->gd_fbheight;
		fi->bpp = NBBY;
		fi->xlen = (fi->width * fi->bpp) / NBBY;
		fi->npl = gi->gd_planes;
		fi->bppu = fi->npl;
		fi->nplbytes = fi->xlen * ((fi->length * fi->bpp) / NBBY);
		memcpy(fi->name, "HP98720", 8);
		fi->attr = 2;	/* HW block mover */
		/*
		 * If mapped, return the UVA where mapped.
		 */
		if (gp->g_data) {
			fi->regbase = gp->g_data;
			fi->fbbase = fi->regbase + gp->g_display.gd_regsize;
		} else {
			fi->fbbase = 0;
			fi->regbase = 0;
		}
		for (i = 0; i < 6; i++)
			fi->regions[i] = 0;
		break;
	}
#endif

	default:
		error = EINVAL;
		break;
	}
	return error;
}

#if NITE > 0

/*
 * Renaissance ite routines
 */

#define REGBASE		((struct rboxfb *)(ip->regbase))
#define WINDOWMOVER	rbox_windowmove

static void
rbox_init(struct ite_data *ip)
{
	int i;

	/* XXX */
	if (ip->regbase == 0) {
		struct grf_data *gp = ip->grf;

		ip->regbase = gp->g_regkva;
		ip->fbbase = gp->g_fbkva;
		ip->fbwidth = gp->g_display.gd_fbwidth;
		ip->fbheight = gp->g_display.gd_fbheight;
		ip->dwidth = gp->g_display.gd_dwidth;
		ip->dheight = gp->g_display.gd_dheight;
		/*
		 * XXX some displays (e.g. the davinci) appear
		 * to return a display height greater than the
		 * returned FB height.  Guess we should go back
		 * to getting the display dimensions from the
		 * fontrom...
		 */
		if (ip->dwidth > ip->fbwidth)
			ip->dwidth = ip->fbwidth;
		if (ip->dheight > ip->fbheight)
			ip->dheight = ip->fbheight;
	}

	rb_waitbusy(ip->regbase);

	REGBASE->reset = 0x39;
	DELAY(1000);

	REGBASE->interrupt = 0x04;
	REGBASE->display_enable = 0x01;
	REGBASE->video_enable = 0x01;
	REGBASE->drive = 0x01;
	REGBASE->vdrive = 0x0;

	ite_fontinfo(ip);

	REGBASE->opwen = 0xFF;

	/*
	 * Clear the framebuffer.
	 */
	rbox_windowmove(ip, 0, 0, 0, 0, ip->fbheight, ip->fbwidth, RR_CLEAR);
	rb_waitbusy(ip->regbase);

	for(i = 0; i < 16; i++) {
		*(ip->regbase + 0x63c3 + i*4) = 0x0;
		*(ip->regbase + 0x6403 + i*4) = 0x0;
		*(ip->regbase + 0x6803 + i*4) = 0x0;
		*(ip->regbase + 0x6c03 + i*4) = 0x0;
		*(ip->regbase + 0x73c3 + i*4) = 0x0;
		*(ip->regbase + 0x7403 + i*4) = 0x0;
		*(ip->regbase + 0x7803 + i*4) = 0x0;
		*(ip->regbase + 0x7c03 + i*4) = 0x0;
	}

	REGBASE->rep_rule = 0x33;

	/*
	 * I cannot figure out how to make the blink planes stop. So, we
	 * must set both colormaps so that when the planes blink, and
	 * the secondary colormap is active, we still get text.
	 */
	CM1RED[0x00].value = 0x00;
	CM1GRN[0x00].value = 0x00;
	CM1BLU[0x00].value = 0x00;
	CM1RED[0x01].value = 0xFF;
	CM1GRN[0x01].value = 0xFF;
	CM1BLU[0x01].value = 0xFF;

	CM2RED[0x00].value = 0x00;
	CM2GRN[0x00].value = 0x00;
	CM2BLU[0x00].value = 0x00;
	CM2RED[0x01].value = 0xFF;
	CM2GRN[0x01].value = 0xFF;
	CM2BLU[0x01].value = 0xFF;

	REGBASE->blink = 0x00;
	REGBASE->write_enable = 0x01;
	REGBASE->opwen = 0x00;

	ite_fontinit(ip);

	/*
	 * Stash the inverted cursor.
	 */
	rbox_windowmove(ip, charY(ip, ' '), charX(ip, ' '),
			    ip->cblanky, ip->cblankx, ip->ftheight,
			    ip->ftwidth, RR_COPYINVERTED);
}

static void
rbox_deinit(struct ite_data *ip)
{
	rbox_windowmove(ip, 0, 0, 0, 0, ip->fbheight, ip->fbwidth, RR_CLEAR);
	rb_waitbusy(ip->regbase);

	ip->flags &= ~ITE_INITED;
}

static void
rbox_putc(struct ite_data *ip, int c, int dy, int dx, int mode)
{
	int wrr = ((mode == ATTR_INV) ? RR_COPYINVERTED : RR_COPY);

	rbox_windowmove(ip, charY(ip, c), charX(ip, c),
			dy * ip->ftheight, dx * ip->ftwidth,
			ip->ftheight, ip->ftwidth, wrr);
}

static void
rbox_cursor(struct ite_data *ip, int flag)
{
	if (flag == DRAW_CURSOR)
		draw_cursor(ip)
	else if (flag == MOVE_CURSOR) {
		erase_cursor(ip)
		draw_cursor(ip)
	}
	else
		erase_cursor(ip)
}

static void
rbox_clear(struct ite_data *ip, int sy, int sx, int h, int w)
{
	rbox_windowmove(ip, sy * ip->ftheight, sx * ip->ftwidth,
			sy * ip->ftheight, sx * ip->ftwidth,
			h  * ip->ftheight, w  * ip->ftwidth,
			RR_CLEAR);
}

static void
rbox_scroll(struct ite_data *ip, int sy, int sx, int count, int dir)
{
	int dy;
	int dx = sx;
	int height = 1;
	int width = ip->cols;

	if (dir == SCROLL_UP) {
		dy = sy - count;
		height = ip->rows - sy;
	}
	else if (dir == SCROLL_DOWN) {
		dy = sy + count;
		height = ip->rows - dy - 1;
	}
	else if (dir == SCROLL_RIGHT) {
		dy = sy;
		dx = sx + count;
		width = ip->cols - dx;
	}
	else {
		dy = sy;
		dx = sx - count;
		width = ip->cols - sx;
	}

	rbox_windowmove(ip, sy * ip->ftheight, sx * ip->ftwidth,
			dy * ip->ftheight, dx * ip->ftwidth,
			height * ip->ftheight,
			width  * ip->ftwidth, RR_COPY);
}

static void
rbox_windowmove(struct ite_data *ip, int sy, int sx, int dy, int dx, int h,
    int w, int func)
{
	struct rboxfb *rp = REGBASE;
	if (h == 0 || w == 0)
		return;

	rb_waitbusy(ip->regbase);
	rp->rep_rule = func << 4 | func;
	rp->source_y = sy;
	rp->source_x = sx;
	rp->dest_y = dy;
	rp->dest_x = dx;
	rp->wheight = h;
	rp->wwidth  = w;
	rp->wmove = 1;
}

/*
 * Renaissance console support
 */
int
rboxcnattach(bus_space_tag_t bst, bus_addr_t addr, int scode)
{
	bus_space_handle_t bsh;
	void *va;
	struct grfreg *grf;
	struct grf_data *gp = &grf_cn;
	int size;

	if (bus_space_map(bst, addr, PAGE_SIZE, 0, &bsh))
		return 1;
	va = bus_space_vaddr(bst, bsh);
	grf = (struct grfreg *)va;

	if (badaddr(va) ||
	    (grf->gr_id != GRFHWID) || (grf->gr_id2 != GID_RENAISSANCE)) {
		bus_space_unmap(bst, bsh, PAGE_SIZE);
		return 1;
	}

	size = DIO_SIZE(scode, va);

	bus_space_unmap(bst, bsh, PAGE_SIZE);
	if (bus_space_map(bst, addr, size, 0, &bsh))
		return 1;
	va = bus_space_vaddr(bst, bsh);

	/*
	 * Initialize the framebuffer hardware.
	 */
	(void)rb_init(gp, scode, va);
	rbconscode = scode;
	rbconaddr = va;

	/*
	 * Set up required grf data.
	*/
	gp->g_sw = &rbox_grfsw;
	gp->g_display.gd_id = gp->g_sw->gd_swid;
	gp->g_flags = GF_ALIVE;

	/*
	 * Initialize the terminal emulator.
	*/
	itedisplaycnattach(gp, &rbox_itesw);
	return 0;
}

#endif /* NITE > 0 */
