/* $NetBSD: sfb.c,v 1.22.14.1 1999/12/27 18:31:30 wrstuden Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: sfb.c,v 1.22.14.1 1999/12/27 18:31:30 wrstuden Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/conf.h>

#include <dev/tc/tcvar.h>
#include <machine/sfbreg.h>
#include <alpha/tc/sfbvar.h>
#include <alpha/tc/bt459reg.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsdisplayvar.h>
#include <machine/fbio.h>

#include <machine/autoconf.h>
#include <machine/pte.h>

int	sfbmatch __P((struct device *, struct cfdata *, void *));
void	sfbattach __P((struct device *, struct device *, void *));

struct cfattach sfb_ca = {
	sizeof(struct sfb_softc), sfbmatch, sfbattach,
};

void	sfb_getdevconfig __P((tc_addr_t dense_addr, struct sfb_devconfig *dc));
struct sfb_devconfig sfb_console_dc;
tc_addr_t sfb_consaddr;

struct wsdisplay_emulops sfb_emulfuncs = {
	rcons_cursor,			/* could use hardware cursor; punt */
	rcons_mapchar,
	rcons_putchar,
	rcons_copycols,
	rcons_erasecols,
	rcons_copyrows,
	rcons_eraserows,
	rcons_alloc_attr
};

struct wsscreen_descr sfb_stdscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global */
	&sfb_emulfuncs,
	0, 0
};

const struct wsscreen_descr *_sfb_scrlist[] = {
	&sfb_stdscreen,
	/* XXX other formats, graphics screen? */
};

struct wsscreen_list sfb_screenlist = {
	sizeof(_sfb_scrlist) / sizeof(struct wsscreen_descr *), _sfb_scrlist
};

int	sfbioctl __P((void *, u_long, caddr_t, int, struct proc *));
int	sfbmmap __P((void *, off_t, int));

static int	sfb_alloc_screen __P((void *, const struct wsscreen_descr *,
				      void **, int *, int *, long *));
static void	sfb_free_screen __P((void *, void *));
static int	sfb_show_screen __P((void *, void *, int,
				     void (*) (void *, int, int), void *));

void	sfb_blank __P((struct sfb_devconfig *));
void	sfb_unblank __P((struct sfb_devconfig *));

void	sfb_put_cmap __P((struct sfb_devconfig *, struct fbcmap *));
void	sfb_get_cmap __P((struct sfb_devconfig *, struct fbcmap *));
int     sfb_set_curpos __P((struct sfb_devconfig *, struct fbcurpos *));
int     sfb_get_curpos __P((struct sfb_devconfig *, struct fbcurpos *));
int     sfb_get_curmax __P((struct sfb_devconfig *, struct fbcurpos *));
int     sfb_set_cursor __P((struct sfb_devconfig *, struct fbcursor *));
int     sfb_get_cursor __P((struct sfb_devconfig *, struct fbcursor *));

struct wsdisplay_accessops sfb_accessops = {
	sfbioctl,
	sfbmmap,
	sfb_alloc_screen,
	sfb_free_screen,
	sfb_show_screen,
	0 /* load_font */
};

int
sfbmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct tc_attach_args *ta = aux;

	if (strncmp("PMAGB-BA", ta->ta_modname, TC_ROM_LLEN) != 0)
		return (0);

	return (10);
}

void
sfb_getdevconfig(dense_addr, dc)
	tc_addr_t dense_addr;
	struct sfb_devconfig *dc;
{
	struct raster *rap;
	struct rcons *rcp;
	char *regp, *ramdacregp;
	int i;

	dc->dc_vaddr = dense_addr;
	dc->dc_paddr = ALPHA_K0SEG_TO_PHYS(dc->dc_vaddr);	/* XXX */
	dc->dc_size = SFB_SIZE;

	regp = (char *)dc->dc_vaddr + SFB_ASIC_OFFSET;
	ramdacregp = (char *)dc->dc_vaddr + SFB_RAMDAC_OFFSET;

	dc->dc_wid =
	    (*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_HSETUP) & 0x1ff) * 4;
	dc->dc_ht =
	    (*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_VSETUP) & 0x7ff);

	switch (*(volatile u_int32_t *)(regp + SFB_ASIC_DEEP)) {
	case 0:
	case 1:					/* XXX by the book; wrong? */
		dc->dc_depth = 8;		/* 8 plane */
		break;
	case 2:
		dc->dc_depth = 16;		/* 16 plane */
		break;
	case 4:
		dc->dc_depth = 32;		/* 32 plane */
		break;
	default:
		dc->dc_depth = 8;		/* XXX can't happen? */
		break;
	}

	dc->dc_rowbytes = dc->dc_wid * (dc->dc_depth / 8);

	dc->dc_videobase = dc->dc_vaddr + SFB_FB_OFFSET +
	    ((*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_BASE)) *
	     4096 * (dc->dc_depth / 8));
	
	(*(volatile u_int32_t *)(regp + SFB_ASIC_MODE)) = 0;
	tc_wmb();
	(*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_VALID)) = 1;
	tc_wmb();

	/*
	 * Set all bits in the pixel mask, to enable writes to all pixels.
	 * It seems that the console firmware clears some of them
	 * under some circumstances, which causes cute vertical stripes.
	 */
	(*(volatile u_int32_t *)(regp + SFB_ASIC_PIXELMASK)) = 0xffffffff;
	tc_wmb();
	(*(volatile u_int32_t *)(regp + SFB_ASIC_PLANEMASK)) = 0xffffffff;
	tc_wmb();

	/* Initialize the RAMDAC/colormap */
	/* start XXX XXX XXX */
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRLOW)) = 0;
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRHIGH)) = 0;
	tc_wmb();
	for (i = 0; i < 256; i++) {
		(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_CMAPDATA)) =
		  dc->dc_cmap_red[i] = i ? 0xff : 0;
		tc_wmb();
		(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_CMAPDATA)) =
		  dc->dc_cmap_green[i] = i ? 0xff : 0;
		tc_wmb();
		(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_CMAPDATA)) =
		  dc->dc_cmap_blue[i] = i ? 0xff : 0;
		tc_wmb();
	}
	/* end XXX XXX XXX */
	
	/* clear the screen */
	for (i = 0; i < dc->dc_ht * dc->dc_rowbytes; i += sizeof(u_int32_t))
		*(u_int32_t *)(dc->dc_videobase + i) = 0x00000000;

	/* disable hardware cursor */
	dc->dc_cursor_enable = 0;
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRLOW)) =
	    BT459_REG_CCR;
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRHIGH)) =
	    BT459_REG_CCR >> 8;
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) = 0x00;
	tc_wmb();

	/* initialize the cursor position  */
	dc->dc_curpos_x = 368;
	dc->dc_curpos_y = 34;
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRLOW)) = 
	    BT459_REG_CXLO;
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRHIGH)) =
	    BT459_REG_CXLO >> 8;
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_curpos_x;
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_curpos_x >> 8;
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_curpos_y;
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_curpos_y >> 8;
	tc_wmb();

	/* initialize the cursor color  */
	dc->dc_cursor_red[0] = 0xff;
	dc->dc_cursor_green[0] = 0xff;
	dc->dc_cursor_blue[0] = 0xff;
	dc->dc_cursor_red[1] = 0x00;
	dc->dc_cursor_green[1] = 0x00;
	dc->dc_cursor_blue[1] = 0x00;
	dc->dc_cursor_red[2] = 0xff;
	dc->dc_cursor_green[2] = 0xff;
	dc->dc_cursor_blue[2] = 0xff;
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRLOW)) =
	    BT459_REG_CCOLOR_1;
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRHIGH)) =
	    BT459_REG_CCOLOR_1 >> 8;
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_cursor_red[0];
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_cursor_green[0];
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_cursor_blue[0];
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRLOW)) =
	    BT459_REG_CCOLOR_2;
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRHIGH)) =
	    BT459_REG_CCOLOR_2 >> 8;
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_cursor_red[1];
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_cursor_green[1];
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_cursor_blue[1];
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRLOW)) =
	    BT459_REG_CCOLOR_3;
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRHIGH)) =
	    BT459_REG_CCOLOR_3 >> 8;
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_cursor_red[2];
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_cursor_green[2];
	tc_wmb();
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_cursor_blue[2];
	tc_wmb();

	/* initialize the raster */
	rap = &dc->dc_raster;
	rap->width = dc->dc_wid;
	rap->height = dc->dc_ht;
	rap->depth = 8;
	rap->linelongs = dc->dc_rowbytes / sizeof(u_int32_t);
	rap->pixels = (u_int32_t *)dc->dc_videobase;

	/* initialize the raster console blitter */
	rcp = &dc->dc_rcons;
	rcp->rc_sp = rap;
	rcp->rc_crow = rcp->rc_ccol = -1;
	rcp->rc_crowp = &rcp->rc_crow;
	rcp->rc_ccolp = &rcp->rc_ccol;
	rcons_init(rcp, 34, 80);

	sfb_stdscreen.nrows = dc->dc_rcons.rc_maxrow;
	sfb_stdscreen.ncols = dc->dc_rcons.rc_maxcol;
}

void
sfbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sfb_softc *sc = (struct sfb_softc *)self;
	struct tc_attach_args *ta = aux;
	struct wsemuldisplaydev_attach_args waa;
	int console;

	console = (ta->ta_addr == sfb_consaddr);
	if (console) {
		sc->sc_dc = &sfb_console_dc;
		sc->nscreens = 1;
	} else {
		sc->sc_dc = (struct sfb_devconfig *)
		    malloc(sizeof(struct sfb_devconfig), M_DEVBUF, M_WAITOK);
		sfb_getdevconfig(ta->ta_addr, sc->sc_dc);
	}
	if (sc->sc_dc->dc_vaddr == NULL) {
		printf(": couldn't map memory space; punt!\n");
		return;
	}
	printf(": %d x %d, %dbpp\n", sc->sc_dc->dc_wid, sc->sc_dc->dc_ht,
	    sc->sc_dc->dc_depth);

#if 0
	x = (char *)ta->ta_addr + SFB_ASIC_OFFSET;
	printf("%s: Video Base Address = 0x%x\n", self->dv_xname,
	    *(u_int32_t *)(x + SFB_ASIC_VIDEO_BASE));
	printf("%s: Horizontal Setup = 0x%x\n", self->dv_xname,
	    *(u_int32_t *)(x + SFB_ASIC_VIDEO_HSETUP));
	printf("%s: Vertical Setup = 0x%x\n", self->dv_xname,
	    *(u_int32_t *)(x + SFB_ASIC_VIDEO_VSETUP));
#endif

	waa.console = console;
	waa.scrdata = &sfb_screenlist;
	waa.accessops = &sfb_accessops;
	waa.accesscookie = sc;

	config_found(self, &waa, wsemuldisplaydevprint);
}

int
sfbioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct sfb_softc *sc = v;
	struct sfb_devconfig *dc = sc->sc_dc;

	switch (cmd) {
	case FBIOGTYPE:
#define fbt ((struct fbtype *)data)
		fbt->fb_type = FBTYPE_SFB;
		fbt->fb_height = sc->sc_dc->dc_ht;
		fbt->fb_width = sc->sc_dc->dc_wid;
		fbt->fb_depth = sc->sc_dc->dc_depth;
		fbt->fb_cmsize = 256;		/* XXX ??? */
		fbt->fb_size = sc->sc_dc->dc_size;
#undef fbt
		return (0);

	case FBIOPUTCMAP:
		sfb_put_cmap(dc, (struct fbcmap *)data);
		return (0);

	case FBIOGETCMAP:
		sfb_get_cmap(dc, (struct fbcmap *)data);
		return (0);

	case FBIOGATTR:
		return (ENOTTY);			/* XXX ? */

	case FBIOSVIDEO:
		if (*(int *)data == FBVIDEO_OFF)
			sfb_blank(sc->sc_dc);
		else
			sfb_unblank(sc->sc_dc);
		return (0);

	case FBIOGVIDEO:
		*(int *)data = dc->dc_blanked ? FBVIDEO_OFF : FBVIDEO_ON;
		return (0);

	case FBIOSCURSOR:
		return sfb_set_cursor(dc, (struct fbcursor *)data);

	case FBIOGCURSOR:
		return sfb_get_cursor(dc, (struct fbcursor *)data);

	case FBIOSCURPOS:
		return sfb_set_curpos(dc, (struct fbcurpos *)data);

	case FBIOGCURPOS:
		return sfb_get_curpos(dc, (struct fbcurpos *)data);

	case FBIOGCURMAX:
		return sfb_get_curmax(dc, (struct fbcurpos *)data);
	}
	return (-1);
}

int
sfbmmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct sfb_softc *sc = v;

	if (offset >= SFB_SIZE || offset < 0)
		return (-1);
	return alpha_btop(sc->sc_dc->dc_paddr + offset);
}

int
sfb_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct sfb_softc *sc = v;
	long defattr;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_dc->dc_rcons; /* one and only for now */
	*curxp = 0;
	*curyp = 0;
	rcons_alloc_attr(&sc->sc_dc->dc_rcons, 0, 0, 0, &defattr);
	*attrp = defattr;
	sc->nscreens++;
	return (0);
}

void
sfb_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct sfb_softc *sc = v;

	if (sc->sc_dc == &sfb_console_dc)
		panic("sfb_free_screen: console");

	sc->nscreens--;
}

int
sfb_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb) __P((void *, int, int));
	void *cbarg;
{

	return (0);
}

int
sfb_cnattach(addr)
	tc_addr_t addr;
{
	struct sfb_devconfig *dcp = &sfb_console_dc;
	long defattr;

	sfb_getdevconfig(addr, dcp);

	rcons_alloc_attr(&dcp->dc_rcons, 0, 0, 0, &defattr);

	wsdisplay_cnattach(&sfb_stdscreen, &dcp->dc_rcons,
			   0, 0, defattr);
	sfb_consaddr = addr;
	return(0);
}

/*
 * Functions to blank and unblank the display.
 */
void
sfb_blank(dc)
	struct sfb_devconfig *dc;
{
	char *regp = (char *)dc->dc_vaddr + SFB_ASIC_OFFSET;

	if (!dc->dc_blanked) {
		dc->dc_blanked = 1;
	    	*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_VALID) = 0;
		tc_wmb();
	}
}

void
sfb_unblank(dc)
	struct sfb_devconfig *dc;
{
	char *regp = (char *)dc->dc_vaddr + SFB_ASIC_OFFSET;
	
	if (dc->dc_blanked) {
		dc->dc_blanked = 0;
	    	*(volatile u_int32_t *)(regp + SFB_ASIC_VIDEO_VALID) = 1;
		tc_wmb();
	}
}

void
sfb_put_cmap(dc, cmap)
     struct sfb_devconfig *dc;
     struct fbcmap *cmap;
{
	char *ramdacregp;
	int i;
	int max_i;
  
	ramdacregp = (char *)dc->dc_vaddr + SFB_RAMDAC_OFFSET;

	if (cmap->index > 255) {
		return;
	}
	max_i = cmap->index + cmap->count;
	if (max_i > 256) {
		max_i = 256;
	}

	for (i=cmap->index ; i<max_i ; i++) {
		dc->dc_cmap_red[i] = cmap->red[i];
		dc->dc_cmap_green[i] = cmap->green[i];
		dc->dc_cmap_blue[i] = cmap->blue[i];
	}
	/* Initialize the RAMDAC/colormap */
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRLOW)) =
	    cmap->index;
	(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_ADDRHIGH)) = 0;
	tc_wmb();
	for (i=cmap->index ; i<max_i ; i++) {
		(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_CMAPDATA)) =
		    dc->dc_cmap_red[i];
		tc_wmb();
		(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_CMAPDATA)) =
		    dc->dc_cmap_green[i];
		tc_wmb();
		(*(volatile u_int32_t *)(ramdacregp + SFB_RAMDAC_CMAPDATA)) =
		    dc->dc_cmap_blue[i];
		tc_wmb();
	}
}

void 
sfb_get_cmap(dc, cmap)
	struct sfb_devconfig *dc;
	struct fbcmap  *cmap;
{
	int i;
	int max_i;

	if (cmap->index > 255) {
		return;
	}
	max_i = cmap->index + cmap->count;
	if (max_i > 256) {
		max_i = 256;
	}

	for (i = cmap->index; i < max_i; i++) {
		cmap->red[i] = dc->dc_cmap_red[i];
		cmap->green[i] = dc->dc_cmap_green[i];
		cmap->blue[i] = dc->dc_cmap_blue[i];
	}
}

int 
sfb_set_curpos(dc, curpos)
	struct sfb_devconfig *dc;
	struct fbcurpos *curpos;
{
	char *ramdacregp;

	dc->dc_curpos_x = curpos->x + 368;
	dc->dc_curpos_y = curpos->y + 34;

	ramdacregp = (char *) dc->dc_vaddr + SFB_RAMDAC_OFFSET;

	(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRLOW)) =
	    BT459_REG_CXLO;
	(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRHIGH)) =
	    BT459_REG_CXLO >> 8;
	tc_wmb();

	(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_curpos_x;
	tc_wmb();
	(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_curpos_x >> 8;
	tc_wmb();
	(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_curpos_y;
	tc_wmb();
	(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
	    dc->dc_curpos_y >> 8;
	tc_wmb();

	return 0;
}

int 
sfb_get_curpos(dc, curpos)
	struct sfb_devconfig *dc;
	struct fbcurpos *curpos;
{
	curpos->x = dc->dc_curpos_x - 368;
	curpos->y = dc->dc_curpos_y - 34;

	return 0;
}

int 
sfb_get_curmax(dc, curpos)
	struct sfb_devconfig *dc;
	struct fbcurpos *curpos;
{
	curpos->x = 1280;
	curpos->y = 1024;

	return 0;
}

int 
sfb_set_cursor(dc, cursor)
	struct sfb_devconfig *dc;
	struct fbcursor *cursor;
{
	unsigned char buf;
	char *ramdacregp;
	int result, i;

	result = 0;

	ramdacregp = (char *) dc->dc_vaddr + SFB_RAMDAC_OFFSET;

	if (cursor->set & FB_CUR_SETCUR) {
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRLOW)) =
		    BT459_REG_CCR;
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRHIGH)) =
		    BT459_REG_CCR >> 8;
		tc_wmb();

		dc->dc_cursor_enable = cursor->enable;

		if (cursor->enable == 0) {
			(*(volatile u_int32_t *)
			    (ramdacregp + SFB_RAMDAC_REGDATA)) = 0x00;
		} else {
			(*(volatile u_int32_t *)
			    (ramdacregp + SFB_RAMDAC_REGDATA)) = 0xc0;
		}
		tc_wmb();

		result |= 0;
	}
	if (cursor->set & FB_CUR_SETPOS) {
		result |= sfb_set_curpos(dc, &cursor->pos);
	}
	if (cursor->set & FB_CUR_SETHOT) {
		result |= sfb_set_curpos(dc, &cursor->pos);
	}
	if (cursor->set & FB_CUR_SETCMAP) {
		for (i = 0; i < 3; i++) {
			dc->dc_cursor_red[i] = cursor->cmap.red[i];
			dc->dc_cursor_green[i] = cursor->cmap.green[i];
			dc->dc_cursor_blue[i] = cursor->cmap.blue[i];
		}

		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRLOW)) =
		    BT459_REG_CCOLOR_1;
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRHIGH)) =
		    BT459_REG_CCOLOR_1 >> 8;
		tc_wmb();

		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
		    cursor->cmap.red[0];
		tc_wmb();
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
		    cursor->cmap.green[0];
		tc_wmb();
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
		    cursor->cmap.blue[0];
		tc_wmb();

		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRLOW)) =
		    BT459_REG_CCOLOR_2;
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRHIGH)) =
		    BT459_REG_CCOLOR_2 >> 8;
		tc_wmb();

		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
		    cursor->cmap.red[1];
		tc_wmb();
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
		    cursor->cmap.green[1];
		tc_wmb();
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
		    cursor->cmap.blue[1];
		tc_wmb();

		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRLOW)) =
		    BT459_REG_CCOLOR_3;
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRHIGH)) =
		    BT459_REG_CCOLOR_3 >> 8;
		tc_wmb();

		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
		    cursor->cmap.red[2];
		tc_wmb();
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
		    cursor->cmap.green[2];
		tc_wmb();
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_REGDATA)) =
		    cursor->cmap.blue[2];
		tc_wmb();

		result |= 0;
	}
	if (cursor->set & FB_CUR_SETSHAPE) {
		for (i = 0; i < 512; i++) {
			buf = (cursor->image[i] & 0x01) |
			    ((cursor->image[i] & 0x02) << 1) |
			    ((cursor->image[i] & 0x04) << 2) |
			    ((cursor->image[i] & 0x08) << 3);
			dc->dc_cursor_bitmap[i + i] = buf;
			buf = ((cursor->image[i] & 0x10) >> 4) |
			    ((cursor->image[i] & 0x20) >> 3) |
			    ((cursor->image[i] & 0x40) >> 2) |
			    ((cursor->image[i] & 0x80) >> 1);
			dc->dc_cursor_bitmap[i + i + 1] = buf;
		}

		for (i = 0; i < 512; i++) {
			buf = ((cursor->mask[i] & 0x01) << 1) |
			    ((cursor->mask[i] & 0x02) << 2) |
			    ((cursor->mask[i] & 0x04) << 3) |
			    ((cursor->mask[i] & 0x08) << 4);
			dc->dc_cursor_bitmap[i + i] |= buf;
			buf = ((cursor->mask[i] & 0x10) >> 3) |
			    ((cursor->mask[i] & 0x20) >> 2) |
			    ((cursor->mask[i] & 0x40) >> 1) |
			    (cursor->mask[i] & 0x80);
			dc->dc_cursor_bitmap[i + i + 1] |= buf;
		}

		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRLOW)) =
		    BT459_REG_CRAM_BASE;
		(*(volatile u_int32_t *) (ramdacregp + SFB_RAMDAC_ADDRHIGH)) =
		    BT459_REG_CRAM_BASE >> 8;
		tc_wmb();

		for (i = 0; i < 1024; i++) {
			(*(volatile u_int32_t *)
			    (ramdacregp + SFB_RAMDAC_REGDATA)) =
			    dc->dc_cursor_bitmap[i];
			tc_wmb();
		}
		result |= 0;
	}
	return result;
}

int 
sfb_get_cursor(dc, cursor)
	struct sfb_devconfig *dc;
	struct fbcursor *cursor;
{
	int result, i, j, k;

	result = 0;

	if (cursor->set & FB_CUR_SETCUR) {
		cursor->enable = dc->dc_cursor_enable;
	}
	if (cursor->set & FB_CUR_SETPOS) {
		result |= sfb_get_curpos(dc, &cursor->pos);
	}
	if (cursor->set & FB_CUR_SETHOT) {
		result |= sfb_get_curpos(dc, &cursor->pos);
	}
	if (cursor->set & FB_CUR_SETCMAP) {
		for (i = 0; i < 3; i++) {
			cursor->cmap.red[i] = dc->dc_cursor_red[i];
			cursor->cmap.green[i] = dc->dc_cursor_green[i];
			cursor->cmap.blue[i] = dc->dc_cursor_blue[i];
		}
	}
	if (cursor->set & FB_CUR_SETSHAPE) {
		cursor->size.x = 64;
		cursor->size.y = 64;
		for (i = 0; i < 512; i++) {
			j = i + i;
			k = j + 1;
			cursor->image[i] =
			    (dc->dc_cursor_bitmap[j] & 0x01) |
			    ((dc->dc_cursor_bitmap[j] & 0x04) >> 1) |
			    ((dc->dc_cursor_bitmap[j] & 0x10) >> 2) |
			    ((dc->dc_cursor_bitmap[j] & 0x40) >> 3) |
			    ((dc->dc_cursor_bitmap[k] & 0x01) << 4) |
			    ((dc->dc_cursor_bitmap[k] & 0x04) << 3) |
			    ((dc->dc_cursor_bitmap[k] & 0x10) << 2) |
			    ((dc->dc_cursor_bitmap[k] & 0x40) << 1);
			cursor->mask[i] =
			    ((dc->dc_cursor_bitmap[j] & 0x02) >> 1) |
			    ((dc->dc_cursor_bitmap[j] & 0x08) >> 2) |
			    ((dc->dc_cursor_bitmap[j] & 0x20) >> 3) |
			    ((dc->dc_cursor_bitmap[j] & 0x80) >> 4) |
			    ((dc->dc_cursor_bitmap[k] & 0x02) << 3) |
			    ((dc->dc_cursor_bitmap[k] & 0x08) << 2) |
			    ((dc->dc_cursor_bitmap[k] & 0x20) << 1) |
			    (dc->dc_cursor_bitmap[k] & 0x80);
		}
	}
	return result;
}
