/*	$NetBSD: grf_gb.c,v 1.25.2.1.2.1 2006/06/04 13:58:45 tron Exp $	*/

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
 * from: Utah $Hdr: grf_gb.c 1.18 93/08/13$
 *
 *	@(#)grf_gb.c	8.4 (Berkeley) 1/12/94
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
 * from: Utah $Hdr: grf_gb.c 1.18 93/08/13$
 *
 *	@(#)grf_gb.c	8.4 (Berkeley) 1/12/94
 */

/*
 * Graphics routines for the Gatorbox.
 *
 * Note: In the context of this system, "gator" and "gatorbox" both refer to
 *       HP 987x0 graphics systems.  "Gator" is not used for high res mono.
 *       (as in 9837 Gator systems)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: grf_gb.c,v 1.25.2.1.2.1 2006/06/04 13:58:45 tron Exp $");

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
#include <hp300/dev/grf_gbreg.h>

#include <hp300/dev/itevar.h>
#include <hp300/dev/itereg.h>

#include "ite.h"

#define CRTC_DATA_LENGTH  0x0e
u_char crtc_init_data[CRTC_DATA_LENGTH] = {
    0x29, 0x20, 0x23, 0x04, 0x30, 0x0b, 0x30,
    0x30, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00
};

int	gb_init __P((struct grf_data *gp, int, caddr_t));
int	gb_mode __P((struct grf_data *gp, int, caddr_t));
void	gb_microcode __P((struct gboxfb *));

int	gbox_intio_match __P((struct device *, struct cfdata *, void *));
void	gbox_intio_attach __P((struct device *, struct device *, void *));

int	gbox_dio_match __P((struct device *, struct cfdata *, void *));
void	gbox_dio_attach __P((struct device *, struct device *, void *));

int	gboxcnattach __P((bus_space_tag_t, bus_addr_t, int));

CFATTACH_DECL(gbox_intio, sizeof(struct grfdev_softc),
    gbox_intio_match, gbox_intio_attach, NULL, NULL);

CFATTACH_DECL(gbox_dio, sizeof(struct grfdev_softc),
    gbox_dio_match, gbox_dio_attach, NULL, NULL);

/* Gatorbox grf switch */
struct grfsw gbox_grfsw = {
	GID_GATORBOX, GRFGATOR, "gatorbox", gb_init, gb_mode
};

static int gbconscode;
static caddr_t gbconaddr;

#if NITE > 0
void	gbox_init __P((struct ite_data *));
void	gbox_deinit __P((struct ite_data *));
void	gbox_putc __P((struct ite_data *, int, int, int, int));
void	gbox_cursor __P((struct ite_data *, int));
void	gbox_clear __P((struct ite_data *, int, int, int, int));
void	gbox_scroll __P((struct ite_data *, int, int, int, int));
void	gbox_windowmove __P((struct ite_data *, int, int, int, int,
		int, int, int));

/* Gatorbox ite switch */
struct itesw gbox_itesw = {
	gbox_init, gbox_deinit, gbox_clear, gbox_putc,
	gbox_cursor, gbox_scroll, ite_readbyte, ite_writeglyph
};
#endif /* NITE > 0 */

int
gbox_intio_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct intio_attach_args *ia = aux;
	struct grfreg *grf;

	if (strcmp("fb",ia->ia_modname) != 0)
		return (0);

	if (badaddr((caddr_t)ia->ia_addr))
		return (0);

	grf = (struct grfreg *)ia->ia_addr;

	if (grf->gr_id == DIO_DEVICE_ID_FRAMEBUFFER &&
	    grf->gr_id2 == DIO_DEVICE_SECID_GATORBOX) {
		return (1);
	}

	return (0);
}

void
gbox_intio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct grfdev_softc *sc = (struct grfdev_softc *)self;
	struct intio_attach_args *ia = aux;
	caddr_t grf;

	grf = (caddr_t)ia->ia_addr;
	sc->sc_scode = -1;	/* XXX internal i/o */

	sc->sc_isconsole = (sc->sc_scode == gbconscode);
	grfdev_attach(sc, gb_init, grf, &gbox_grfsw);
}

int
gbox_dio_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct dio_attach_args *da = aux;

	if (da->da_id == DIO_DEVICE_ID_FRAMEBUFFER &&
	    da->da_secid == DIO_DEVICE_SECID_GATORBOX)
		return (1);

	return (0);
}

void
gbox_dio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct grfdev_softc *sc = (struct grfdev_softc *)self;
	struct dio_attach_args *da = aux;
	caddr_t grf;

	sc->sc_scode = da->da_scode;
	if (sc->sc_scode == gbconscode)
		grf = gbconaddr;
	else {
		grf = iomap(dio_scodetopa(sc->sc_scode), da->da_size);
		if (grf == 0) {
			printf("%s: can't map framebuffer\n",
			    sc->sc_dev.dv_xname);
			return;
		}
	}

	sc->sc_isconsole = (sc->sc_scode == gbconscode);
	grfdev_attach(sc, gb_init, grf, &gbox_grfsw);
}

/*
 * Initialize hardware.
 * Must point g_display at a grfinfo structure describing the hardware.
 * Returns 0 if hardware not present, non-zero ow.
 */
int
gb_init(gp, scode, addr)
	struct grf_data *gp;
	int scode;
	caddr_t addr;
{
	struct gboxfb *gbp;
	struct grfinfo *gi = &gp->g_display;
	u_char *fbp, save;
	int fboff;

	/*
	 * If the console has been initialized, and it was us, there's
	 * no need to repeat this.
	 */
	if (scode != gbconscode) {
		gbp = (struct gboxfb *) addr;
		if (ISIIOVA(addr))
			gi->gd_regaddr = (caddr_t) IIOP(addr);
		else
			gi->gd_regaddr = dio_scodetopa(scode);
		gi->gd_regsize = 0x10000;
		gi->gd_fbwidth = 1024;		/* XXX */
		gi->gd_fbheight = 1024;		/* XXX */
		gi->gd_fbsize = gi->gd_fbwidth * gi->gd_fbheight;
		fboff = (gbp->fbomsb << 8) | gbp->fbolsb;
		gi->gd_fbaddr = (caddr_t) (*((u_char *)addr + fboff) << 16);
		gp->g_regkva = addr;
		gp->g_fbkva = iomap(gi->gd_fbaddr, gi->gd_fbsize);
		gi->gd_dwidth = 1024;		/* XXX */
		gi->gd_dheight = 768;		/* XXX */
		gi->gd_planes = 0;		/* how do we do this? */
		/*
		 * The minimal info here is from the Gatorbox X driver.
		 */
		fbp = (u_char *) gp->g_fbkva;
		gbp->write_protect = 0;
		gbp->interrupt = 4;		/** fb_enable ? **/
		gbp->rep_rule = 3;		/* GXcopy */
		gbp->blink1 = 0xff;
		gbp->blink2 = 0xff;

		gb_microcode(gbp);

		/*
		 * Find out how many colors are available by determining
		 * which planes are installed.  That is, write all ones to
		 * a frame buffer location, see how many ones are read back.
		 */
		save = *fbp;
		*fbp = 0xFF;
		gi->gd_colors = *fbp + 1;
		*fbp = save;
	}
	return(1);
}

/*
 * Program the 6845.
 */
void
gb_microcode(gbp)
	struct gboxfb *gbp;
{
	int i;

	for (i = 0; i < CRTC_DATA_LENGTH; i++) {
		gbp->crtc_address = i;
		gbp->crtc_data = crtc_init_data[i];
	}
}

/*
 * Change the mode of the display.
 * Right now all we can do is grfon/grfoff.
 * Return a UNIX error number or 0 for success.
 */
int
gb_mode(gp, cmd, data)
	struct grf_data *gp;
	int cmd;
	caddr_t data;
{
	struct gboxfb *gbp;
	int error = 0;

	gbp = (struct gboxfb *)gp->g_regkva;
	switch (cmd) {
	case GM_GRFON:
		gbp->sec_interrupt = 1;
		break;

	case GM_GRFOFF:
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
		memcpy(fi->name,  "HP98700", 8);
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
	return(error);
}

#if NITE > 0

/*
 * Gatorbox ite routines
 */

#define REGBASE     	((struct gboxfb *)(ip->regbase))
#define WINDOWMOVER 	gbox_windowmove

void
gbox_init(ip)
	struct ite_data *ip;
{
	/* XXX */
	if (ip->regbase == 0) {
		struct grf_data *gp = ip->grf;

		ip->regbase = gp->g_regkva;
		ip->fbbase = gp->g_fbkva;
		ip->fbwidth = gp->g_display.gd_fbwidth;
		ip->fbheight = gp->g_display.gd_fbheight;
		ip->dwidth = gp->g_display.gd_dwidth;
		ip->dheight = gp->g_display.gd_dheight;
	}

	REGBASE->write_protect = 0x0;
	REGBASE->interrupt = 0x4;
	REGBASE->rep_rule = RR_COPY;
	REGBASE->blink1 = 0xff;
	REGBASE->blink2 = 0xff;
	gb_microcode((struct gboxfb *)ip->regbase);
	REGBASE->sec_interrupt = 0x01;

	/*
	 * Set up the color map entries. We use three entries in the
	 * color map. The first, is for black, the second is for
	 * white, and the very last entry is for the inverted cursor.
	 */
	REGBASE->creg_select = 0x00;
	REGBASE->cmap_red    = 0x00;
	REGBASE->cmap_grn    = 0x00;
	REGBASE->cmap_blu    = 0x00;
	REGBASE->cmap_write  = 0x00;
	gbcm_waitbusy(ip->regbase);

	REGBASE->creg_select = 0x01;
	REGBASE->cmap_red    = 0xFF;
	REGBASE->cmap_grn    = 0xFF;
	REGBASE->cmap_blu    = 0xFF;
	REGBASE->cmap_write  = 0x01;
	gbcm_waitbusy(ip->regbase);

	REGBASE->creg_select = 0xFF;
	REGBASE->cmap_red    = 0xFF;
	REGBASE->cmap_grn    = 0xFF;
	REGBASE->cmap_blu    = 0xFF;
	REGBASE->cmap_write  = 0x01;
	gbcm_waitbusy(ip->regbase);

	ite_fontinfo(ip);
	ite_fontinit(ip);

	/*
	 * Clear the display. This used to be before the font unpacking
	 * but it crashes. Figure it out later.
	 */
	gbox_windowmove(ip, 0, 0, 0, 0, ip->dheight, ip->dwidth, RR_CLEAR);
	tile_mover_waitbusy(ip->regbase);

	/*
	 * Stash the inverted cursor.
	 */
	gbox_windowmove(ip, charY(ip, ' '), charX(ip, ' '),
			ip->cblanky, ip->cblankx, ip->ftheight,
			ip->ftwidth, RR_COPYINVERTED);
}

void
gbox_deinit(ip)
	struct ite_data *ip;
{
	gbox_windowmove(ip, 0, 0, 0, 0, ip->dheight, ip->dwidth, RR_CLEAR);
	tile_mover_waitbusy(ip->regbase);

	ip->flags &= ~ITE_INITED;
}

void
gbox_putc(ip, c, dy, dx, mode)
	struct ite_data *ip;
	int dy, dx;
	int c, mode;
{
	int wrr = ((mode == ATTR_INV) ? RR_COPYINVERTED : RR_COPY);

	gbox_windowmove(ip, charY(ip, c), charX(ip, c),
			    dy * ip->ftheight, dx * ip->ftwidth,
			    ip->ftheight, ip->ftwidth, wrr);
}

void
gbox_cursor(ip, flag)
	struct ite_data *ip;
	int flag;
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

void
gbox_clear(ip, sy, sx, h, w)
	struct ite_data *ip;
	int sy, sx, h, w;
{
	gbox_windowmove(ip, sy * ip->ftheight, sx * ip->ftwidth,
			sy * ip->ftheight, sx * ip->ftwidth,
			h  * ip->ftheight, w  * ip->ftwidth,
			RR_CLEAR);
}
#define	gbox_blockmove(ip, sy, sx, dy, dx, h, w) \
	gbox_windowmove((ip), \
			(sy) * ip->ftheight, \
			(sx) * ip->ftwidth, \
			(dy) * ip->ftheight, \
			(dx) * ip->ftwidth, \
			(h)  * ip->ftheight, \
			(w)  * ip->ftwidth, \
			RR_COPY)

void
gbox_scroll(ip, sy, sx, count, dir)
	struct ite_data *ip;
	int sy, dir, sx, count;
{
	int height, dy, i;

	tile_mover_waitbusy(ip->regbase);
	REGBASE->write_protect = 0x0;

	if (dir == SCROLL_UP) {
		dy = sy - count;
		height = ip->rows - sy;
		for (i = 0; i < height; i++)
			gbox_blockmove(ip, sy + i, sx, dy + i, 0, 1, ip->cols);
	}
	else if (dir == SCROLL_DOWN) {
		dy = sy + count;
		height = ip->rows - dy;
		for (i = (height - 1); i >= 0; i--)
			gbox_blockmove(ip, sy + i, sx, dy + i, 0, 1, ip->cols);
	}
	else if (dir == SCROLL_RIGHT) {
		gbox_blockmove(ip, sy, sx, sy, sx + count,
			       1, ip->cols - (sx + count));
	}
	else {
		gbox_blockmove(ip, sy, sx, sy, sx - count,
			       1, ip->cols - sx);
	}
}

void
gbox_windowmove(ip, sy, sx, dy, dx, h, w, mask)
	struct ite_data *ip;
	int sy, sx, dy, dx, mask, h, w;
{
	int src, dest;

	src  = (sy * 1024) + sx;	/* upper left corner in pixels */
	dest = (dy * 1024) + dx;

	tile_mover_waitbusy(ip->regbase);
	REGBASE->width = -(w / 4);
	REGBASE->height = -(h / 4);
	if (src < dest)
		REGBASE->rep_rule = MOVE_DOWN_RIGHT|mask;
	else {
		REGBASE->rep_rule = MOVE_UP_LEFT|mask;
		/*
		 * Adjust to top of lower right tile of the block.
		 */
		src = src + ((h - 4) * 1024) + (w - 4);
		dest= dest + ((h - 4) * 1024) + (w - 4);
	}
	FBBASE[dest] = FBBASE[src];
}

/*
 * Gatorbox console support
 */
int
gboxcnattach(bus_space_tag_t bst, bus_addr_t addr, int scode)
{
	bus_space_handle_t bsh;
	caddr_t va;
	struct grfreg *grf;
	struct grf_data *gp = &grf_cn;
	u_int8_t *dioiidev;
	int size;

	if (bus_space_map(bst, addr, PAGE_SIZE, 0, &bsh))
		return (1);
	va = bus_space_vaddr(bst, bsh);
	grf = (struct grfreg *)va;

	if (badaddr(va) ||
	    (grf->gr_id != GRFHWID) || (grf->gr_id2 != GID_GATORBOX)) {
		bus_space_unmap(bst, bsh, PAGE_SIZE);
		return (1);
	}

	if (DIO_ISDIOII(scode)) {
		dioiidev = (u_int8_t *)va;
		size =  ((dioiidev[0x101] + 1) * 0x100000);
	} else
		size = DIOCSIZE;

	bus_space_unmap(bst, bsh, PAGE_SIZE);
	if (bus_space_map(bst, addr, size, 0, &bsh))
		return (1);
	va = bus_space_vaddr(bst, bsh);

	/*
	 * Initialize the framebuffer hardware.
	 */
	(void)gb_init(gp, scode, va);
	gbconscode = scode;
	gbconaddr = va;

	/*
	 * Set up required grf data.
	*/
	gp->g_sw = &gbox_grfsw;
	gp->g_display.gd_id = gp->g_sw->gd_swid;
	gp->g_flags = GF_ALIVE;

	/*
	 * Initialize the terminal emulator.
	*/
	itedisplaycnattach(gp, &gbox_itesw);
	return (0);
}

#endif /* NITE > 0 */
