/*	$NetBSD: grf_dv.c,v 1.16.6.2 2002/01/08 00:24:33 nathanw Exp $	*/

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
 * from: Utah $Hdr: grf_dv.c 1.12 93/08/13$
 *
 *	@(#)grf_dv.c	8.4 (Berkeley) 1/12/94
 */

/*
 * Graphics routines for the DaVinci, HP98730/98731 Graphics system.
 */

#include "opt_compat_hpux.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <dev/cons.h>

#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>
#include <hp300/dev/intiovar.h>

#include <hp300/dev/grfioctl.h>
#include <hp300/dev/grfvar.h>
#include <hp300/dev/grfreg.h>
#include <hp300/dev/grf_dvreg.h>

#include <hp300/dev/itevar.h>
#include <hp300/dev/itereg.h>

#include "ite.h"

int	dv_init __P((struct grf_data *, int, caddr_t));
int	dv_mode __P((struct grf_data *, int, caddr_t));
void	dv_reset __P((struct dvboxfb *));

int	dvbox_intio_match __P((struct device *, struct cfdata *, void *));
void	dvbox_intio_attach __P((struct device *, struct device *, void *));

int	dvbox_dio_match __P((struct device *, struct cfdata *, void *));
void	dvbox_dio_attach __P((struct device *, struct device *, void *));

int	dvboxcnattach __P((bus_space_tag_t, bus_addr_t, int));

struct cfattach dvbox_intio_ca = {
	sizeof(struct grfdev_softc), dvbox_intio_match, dvbox_intio_attach
};

struct cfattach dvbox_dio_ca = {
	sizeof(struct grfdev_softc), dvbox_dio_match, dvbox_dio_attach
};

/* DaVinci grf switch */
struct grfsw dvbox_grfsw = {
	GID_DAVINCI, GRFDAVINCI, "dvbox", dv_init, dv_mode
};

static int dvconscode;
static caddr_t dvconaddr;

#if NITE > 0
void	dvbox_init __P((struct ite_data *));
void	dvbox_deinit __P((struct ite_data *));
void	dvbox_putc __P((struct ite_data *, int, int, int, int));
void	dvbox_cursor __P((struct ite_data *, int));
void	dvbox_clear __P((struct ite_data *, int, int, int, int));
void	dvbox_scroll __P((struct ite_data *, int, int, int, int));
void	dvbox_windowmove __P((struct ite_data *, int, int, int, int,
		int, int, int));

/* DaVinci ite switch */
struct itesw dvbox_itesw = {
	dvbox_init, dvbox_deinit, dvbox_clear, dvbox_putc,
	dvbox_cursor, dvbox_scroll, ite_readbyte, ite_writeglyph
};
#endif /* NITE > 0 */

int
dvbox_intio_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct intio_attach_args *ia = aux;
	struct grfreg *grf;

	if (strcmp("fb      ",ia->ia_modname) != 0)
		return (0);

	if (badaddr((caddr_t)ia->ia_addr))
		return (0);

	grf = (struct grfreg *)ia->ia_addr;

	if (grf->gr_id == DIO_DEVICE_ID_FRAMEBUFFER &&
	    grf->gr_id2 == DIO_DEVICE_SECID_DAVINCI) {
		return (1);
	}

	return (0);
}

void
dvbox_intio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct grfdev_softc *sc = (struct grfdev_softc *)self;
	struct intio_attach_args *ia = aux;
	caddr_t grf;

	grf = (caddr_t)ia->ia_addr;
	sc->sc_scode = -1;	/* XXX internal i/o */

	sc->sc_isconsole = (sc->sc_scode == dvconscode);
	grfdev_attach(sc, dv_init, grf, &dvbox_grfsw);
}

int
dvbox_dio_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct dio_attach_args *da = aux;

	if (da->da_id == DIO_DEVICE_ID_FRAMEBUFFER &&
	    da->da_secid == DIO_DEVICE_SECID_DAVINCI)
		return (1);

	return (0);
}

void
dvbox_dio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct grfdev_softc *sc = (struct grfdev_softc *)self;
	struct dio_attach_args *da = aux;
	caddr_t grf;

	sc->sc_scode = da->da_scode;
	if (sc->sc_scode == dvconscode)
		grf = dvconaddr;
	else {
		grf = iomap(dio_scodetopa(sc->sc_scode), da->da_size);
		if (grf == 0) {
			printf("%s: can't map framebuffer\n",
			    sc->sc_dev.dv_xname);
			return;
		}
	}

	sc->sc_isconsole = (sc->sc_scode == dvconscode);
	grfdev_attach(sc, dv_init, grf, &dvbox_grfsw);
}

/*
 * Initialize hardware.
 * Must point g_display at a grfinfo structure describing the hardware.
 * Returns 0 if hardware not present, non-zero ow.
 */
int
dv_init(gp, scode, addr)
	struct grf_data *gp;
	int scode;
	caddr_t addr;
{
	struct dvboxfb *dbp;
	struct grfinfo *gi = &gp->g_display;
	int fboff;

	/*
	 * If the console has been initialized, and it was us, there's
	 * no need to repeat this.
	 */
	if (scode != dvconscode) {
		dbp = (struct dvboxfb *) addr;
		if (ISIIOVA(addr))
			gi->gd_regaddr = (caddr_t) IIOP(addr);
		else
			gi->gd_regaddr = dio_scodetopa(scode);
		gi->gd_regsize = 0x20000;
		gi->gd_fbwidth = (dbp->fbwmsb << 8) | dbp->fbwlsb;
		gi->gd_fbheight = (dbp->fbhmsb << 8) | dbp->fbhlsb;
		gi->gd_fbsize = gi->gd_fbwidth * gi->gd_fbheight;
			fboff = (dbp->fbomsb << 8) | dbp->fbolsb;
		gi->gd_fbaddr = (caddr_t) (*((u_char *)addr + fboff) << 16);
		if (gi->gd_regaddr >= (caddr_t)DIOIIBASE) {
			/*
			 * For DIO II space the fbaddr just computed is
			 * the offset from the select code base (regaddr)
			 * of the framebuffer.  Hence it is also implicitly
			 * the size of the set.
			 */
			gi->gd_regsize = (int) gi->gd_fbaddr;
			gi->gd_fbaddr += (int) gi->gd_regaddr;
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
		gi->gd_dwidth = (dbp->dwmsb << 8) | dbp->dwlsb;
		gi->gd_dheight = (dbp->dwmsb << 8) | dbp->dwlsb;
		gi->gd_planes = 0;	/* ?? */
		gi->gd_colors = 256;

		dv_reset(dbp);
	}
	return(1);
}

/*
 *  Magic code herein.
 */
void
dv_reset(dbp)
	struct dvboxfb *dbp;
{
  	dbp->reset = 0x80;
	DELAY(100);

	dbp->interrupt = 0x04;
	dbp->en_scan   = 0x01;
	dbp->fbwen     = ~0;
	dbp->opwen     = ~0;
	dbp->fold      = 0x01;
	dbp->drive     = 0x01;
	dbp->rep_rule  = 0x33;
	dbp->alt_rr    = 0x33;
	dbp->zrr       = 0x33;

	dbp->fbvenp    = 0xFF;
	dbp->dispen    = 0x01;
	dbp->fbvens    = 0x0;
	dbp->fv_trig   = 0x01;
	DELAY(100);
	dbp->vdrive    = 0x0;
	dbp->zconfig   = 0x0;

	while (dbp->wbusy & 0x01)
	  DELAY(100);

	dbp->cmapbank = 0;

	dbp->red0   = 0;
	dbp->red1   = 0;
	dbp->green0 = 0;
	dbp->green1 = 0;
	dbp->blue0  = 0;
	dbp->blue1  = 0;

	dbp->panxh   = 0;
	dbp->panxl   = 0;
	dbp->panyh   = 0;
	dbp->panyl   = 0;
	dbp->zoom    = 0;
	dbp->cdwidth = 0x50;
	dbp->chstart = 0x52;
	dbp->cvwidth = 0x22;
	dbp->pz_trig = 1;
}

/*
 * Change the mode of the display.
 * Right now all we can do is grfon/grfoff.
 * Return a UNIX error number or 0 for success.
 */
int
dv_mode(gp, cmd, data)
	struct grf_data *gp;
	int cmd;
	caddr_t data;
{
	struct dvboxfb *dbp;
	int error = 0;

	dbp = (struct dvboxfb *) gp->g_regkva;
	switch (cmd) {
	case GM_GRFON:
	  	dbp->dispen = 0x01;
	  	break;

	case GM_GRFOFF:
		break;

	case GM_GRFOVON:
		dbp->opwen = 0xF;
		dbp->drive = 0x10;
		break;

	case GM_GRFOVOFF:
		dbp->opwen = 0;
		dbp->drive = 0x01;
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
		memcpy(fi->name, "HP98730", 8);
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
 * DaVinci ite routines
 */

#define REGBASE		((struct dvboxfb *)(ip->regbase))
#define WINDOWMOVER	dvbox_windowmove

void
dvbox_init(ip)
	struct ite_data *ip;
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

	dv_reset((struct dvboxfb *)ip->regbase);

	/*
	 * Turn on frame buffer, turn on overlay planes, set replacement
	 * rule, enable top overlay plane writes for ite, disable all frame
	 * buffer planes, set byte per pixel, and display frame buffer 0.
	 * Lastly, turn on the box.
	 */
	REGBASE->interrupt = 0x04;
	REGBASE->drive     = 0x10;		
 	REGBASE->rep_rule  = RR_COPY << 4 | RR_COPY;
	REGBASE->opwen     = 0x01;
	REGBASE->fbwen     = 0x0;
	REGBASE->fold      = 0x01;
	REGBASE->vdrive    = 0x0;
	REGBASE->dispen    = 0x01;

	/*
	 * Video enable top overlay plane.
	 */
	REGBASE->opvenp = 0x01;
	REGBASE->opvens = 0x01;

	/*
	 * Make sure that overlay planes override frame buffer planes.
	 */
	REGBASE->ovly0p  = 0x0;
	REGBASE->ovly0s  = 0x0;
	REGBASE->ovly1p  = 0x0;
	REGBASE->ovly1s  = 0x0;
	REGBASE->fv_trig = 0x1;
	DELAY(100);

	/*
	 * Setup the overlay colormaps. Need to set the 0,1 (black/white)
	 * color for both banks.
	 */

	for (i = 0; i <= 1; i++) {
		REGBASE->cmapbank = i;
		REGBASE->rgb[0].red   = 0x00;
		REGBASE->rgb[0].green = 0x00;
		REGBASE->rgb[0].blue  = 0x00;
		REGBASE->rgb[1].red   = 0xFF;
		REGBASE->rgb[1].green = 0xFF;
		REGBASE->rgb[1].blue  = 0xFF;
	}
	REGBASE->cmapbank = 0;
	
	db_waitbusy(ip->regbase);

	ite_fontinfo(ip);
	ite_fontinit(ip);

	/*
	 * Clear the (visible) framebuffer.
	 */
	dvbox_windowmove(ip, 0, 0, 0, 0, ip->dheight, ip->dwidth, RR_CLEAR);
	db_waitbusy(ip->regbase);

	/*
	 * Stash the inverted cursor.
	 */
	dvbox_windowmove(ip, charY(ip, ' '), charX(ip, ' '),
			 ip->cblanky, ip->cblankx, ip->ftheight,
			 ip->ftwidth, RR_COPYINVERTED);
}

void
dvbox_deinit(ip)
	struct ite_data *ip;
{
	dvbox_windowmove(ip, 0, 0, 0, 0, ip->fbheight, ip->fbwidth, RR_CLEAR);
	db_waitbusy(ip->regbase);

   	ip->flags &= ~ITE_INITED;
}

void
dvbox_putc(ip, c, dy, dx, mode)
	struct ite_data *ip;
        int dy, dx, c, mode;
{
        int wrr = ((mode == ATTR_INV) ? RR_COPYINVERTED : RR_COPY);
	
	dvbox_windowmove(ip, charY(ip, c), charX(ip, c),
			 dy * ip->ftheight, dx * ip->ftwidth,
			 ip->ftheight, ip->ftwidth, wrr);
}

void
dvbox_cursor(ip, flag)
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
dvbox_clear(ip, sy, sx, h, w)
	struct ite_data *ip;
	int sy, sx, h, w;
{
	dvbox_windowmove(ip, sy * ip->ftheight, sx * ip->ftwidth,
			 sy * ip->ftheight, sx * ip->ftwidth, 
			 h  * ip->ftheight, w  * ip->ftwidth,
			 RR_CLEAR);
}

void
dvbox_scroll(ip, sy, sx, count, dir)
        struct ite_data *ip;
        int sy, count, dir, sx;
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

	dvbox_windowmove(ip, sy * ip->ftheight, sx * ip->ftwidth,
			 dy * ip->ftheight, dx * ip->ftwidth,
			 height * ip->ftheight,
			 width  * ip->ftwidth, RR_COPY);
}

void
dvbox_windowmove(ip, sy, sx, dy, dx, h, w, func)
	struct ite_data *ip;
	int sy, sx, dy, dx, h, w, func;
{
	struct dvboxfb *dp = REGBASE;
	if (h == 0 || w == 0)
		return;
	
	db_waitbusy(ip->regbase);
	dp->rep_rule = func << 4 | func;
	dp->source_y = sy;
	dp->source_x = sx;
	dp->dest_y   = dy;
	dp->dest_x   = dx;
	dp->wheight  = h;
	dp->wwidth   = w;
	dp->wmove    = 1;
}

/*
 * DaVinci console support
 */

int
dvboxcnattach(bus_space_tag_t bst, bus_addr_t addr, int scode)
{
	bus_space_handle_t bsh;
	caddr_t va;
	struct grfreg *grf;
	struct grf_data *gp = &grf_cn;
	u_int8_t *dioiidev;
	int size;

	if (bus_space_map(bst, addr, NBPG, 0, &bsh))
		return (1);
	va = bus_space_vaddr(bst, bsh);
	grf = (struct grfreg *)va;

	if ((grf->gr_id != GRFHWID) || (grf->gr_id2 != GID_DAVINCI)) {
		bus_space_unmap(bst, bsh, NBPG); 
		return (1);
	}

	if (scode > 132) {
		dioiidev = (u_int8_t *)va;
		size =  ((dioiidev[0x101] + 1) * 0x100000);
	} else
		size = DIOCSIZE;

	bus_space_unmap(bst, bsh, NBPG);
	if (bus_space_map(bst, addr, size, 0, &bsh))
		return (1);
	va = bus_space_vaddr(bst, bsh);

	/*
	 * Initialize the framebuffer hardware.
	 */
	(void)dv_init(gp, scode, va);
	dvconscode = scode;
	dvconaddr = va;

	/*
	 * Set up required grf data.
	*/
	gp->g_sw = &dvbox_grfsw;
	gp->g_display.gd_id = gp->g_sw->gd_swid;
	gp->g_flags = GF_ALIVE;

	/*
	 * Initialize the terminal emulator.
	*/
	itedisplaycnattach(gp, &dvbox_itesw);
	return (0);
}

#endif /* NITE > 0 */
