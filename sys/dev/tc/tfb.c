/* $NetBSD: tfb.c,v 1.11.4.3 1999/08/02 22:08:15 thorpej Exp $ */

/*
 * Copyright (c) 1998, 1999 Tohru Nishimura.  All rights reserved.
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
 *      This product includes software developed by Tohru Nishimura
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: tfb.c,v 1.11.4.3 1999/08/02 22:08:15 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/tc/tcvar.h>
#include <dev/ic/bt463reg.h>
#include <dev/ic/bt431reg.h>	

#include <uvm/uvm_extern.h>

#if defined(pmax)
#define	machine_btop(x) mips_btop(x)
#define	MACHINE_KSEG0_TO_PHYS(x) MIPS_KSEG0_TO_PHYS(x)

/*
 * struct bt463reg {
 * 	u_int8_t	bt_lo;
 * 	unsigned : 24;
 * 	u_int8_t	bt_hi;
 * 	unsigned : 24;
 * 	u_int8_t	bt_reg;
 * 	unsigned : 24;
 * 	u_int8_t	bt_cmap;
 * };
 *
 * N.B. a pair of Bt431s are located adjascently.
 * 	struct bt431twin {
 *		struct {
 *			u_int8_t u0;	for sprite image
 *			u_int8_t u1;	for sprite mask
 *			unsigned :16;
 *		} bt_lo;
 *		...
 *
 * struct bt431reg {
 * 	u_int16_t	bt_lo;
 * 	unsigned : 16;
 * 	u_int16_t	bt_hi;
 * 	unsigned : 16;
 * 	u_int16_t	bt_ram;
 * 	unsigned : 16;
 * 	u_int16_t	bt_ctl;
 * };
 */

#define	BYTE(base, index)	*((u_int8_t *)(base) + ((index)<<2))
#define	HALF(base, index)	*((u_int16_t *)(base) + ((index)<<1))

#endif

#if defined(__alpha__) || defined(alpha)
#define machine_btop(x) alpha_btop(x)
#define MACHINE_KSEG0_TO_PHYS(x) ALPHA_K0SEG_TO_PHYS(x)

/*
 * struct bt463reg {
 * 	u_int32_t	bt_lo;
 * 	u_int32_t	bt_hi;
 * 	u_int32_t	bt_reg;
 * 	u_int32_t	bt_cmap;
 * };
 * 
 * struct bt431reg {
 * 	u_int32_t	bt_lo;
 * 	u_int32_t	bt_hi;
 * 	u_int32_t	bt_ram;
 * 	u_int32_t	bt_ctl;
 * };
 */

#define	BYTE(base, index)	*((u_int32_t *)(base) + (index))
#define	HALF(base, index)	*((u_int32_t *)(base) + (index))

#endif

/* Bt463 hardware registers */
#define	bt_lo	0
#define	bt_hi	1
#define	bt_reg	2
#define	bt_cmap	3

/* Bt431 hardware registers */
#define	bt_ram	2
#define	bt_ctl	3

#define	SELECT463(vdac, regno) do {			\
	BYTE(vdac, bt_lo) = (regno) & 0x00ff;		\
	BYTE(vdac, bt_hi) = ((regno)& 0xff00) >> 8;	\
	tc_wmb();					\
   } while (0)

#define	TWIN(x)	   ((x) | ((x) << 8))
#define	TWIN_LO(x) (twin = (x) & 0x00ff, (twin << 8) | twin)
#define	TWIN_HI(x) (twin = (x) & 0xff00, twin | (twin >> 8))

#define	SELECT431(curs, regno) do {	\
	HALF(curs, bt_lo) = TWIN(regno);\
	HALF(curs, bt_hi) = 0;		\
	tc_wmb();			\
   } while (0)

struct fb_devconfig {
	vaddr_t dc_vaddr;		/* memory space virtual base address */
	paddr_t dc_paddr;		/* memory space physical base address */
	vsize_t dc_size;		/* size of slot memory */
	int	dc_wid;			/* width of frame buffer */
	int	dc_ht;			/* height of frame buffer */
	int	dc_depth;		/* depth, bits per pixel */
	int	dc_rowbytes;		/* bytes in a FB scan line */
	vaddr_t dc_videobase;		/* base of flat frame buffer */
	struct raster	dc_raster;	/* raster description */
	struct rcons	dc_rcons;	/* raster blitter control info */
	int	    dc_blanked;		/* currently has video disabled */
};

struct hwcmap256 {
#define	CMAP_SIZE	256	/* R/G/B entries */
	u_int8_t r[CMAP_SIZE];
	u_int8_t g[CMAP_SIZE];
	u_int8_t b[CMAP_SIZE];
};

struct hwcursor64 {
	struct wsdisplay_curpos cc_pos;
	struct wsdisplay_curpos cc_hot;
	struct wsdisplay_curpos cc_size;
	struct wsdisplay_curpos cc_magic;
#define	CURSOR_MAX_SIZE	64
	u_int8_t cc_color[6];
	u_int64_t cc_image[64 + 64];
};

struct tfb_softc {
	struct device sc_dev;
	struct fb_devconfig *sc_dc;	/* device configuration */
	struct hwcmap256 sc_cmap;	/* software copy of colormap */
	struct hwcursor64 sc_cursor;	/* software copy of cursor */
	int sc_curenb;			/* cursor sprite enabled */
	int sc_changed;			/* need update of colormap */
#define	DATA_ENB_CHANGED	0x01	/* cursor enable changed */
#define	DATA_CURCMAP_CHANGED	0x02	/* cursor colormap changed */
#define	DATA_CURSHAPE_CHANGED	0x04	/* cursor size, image, mask changed */
#define	DATA_CMAP_CHANGED	0x08	/* colormap changed */
#define	DATA_ALL_CHANGED	0x0f
	int nscreens;
};

#define	TX_MAGIC_X	360
#define	TX_MAGIC_Y	36

#define	TX_BT463_OFFSET	0x040000
#define	TX_BT431_OFFSET	0x040010
#define	TX_CONTROL	0x040030
#define	TX_MAP_REGISTER	0x040030
#define	TX_PIP_OFFSET	0x0800c0
#define	TX_SELECTION	0x100000
#define	TX_8FB_OFFSET	0x200000
#define	TX_8FB_SIZE	0x200000
#define	TX_24FB_OFFSET	0x400000
#define	TX_24FB_SIZE	0x400000
#define	TX_VIDEO_ENABLE	0xa00000

#define	TX_CTL_VIDEO_ON	0x80
#define	TX_CTL_INT_ENA	0x40
#define	TX_CTL_INT_PEND	0x20
#define	TX_CTL_SEG_ENA	0x10
#define	TX_CTL_SEG	0x0f

int  tfbmatch __P((struct device *, struct cfdata *, void *));
void tfbattach __P((struct device *, struct device *, void *));

struct cfattach tfb_ca = {
	sizeof(struct tfb_softc), tfbmatch, tfbattach,
};

void tfb_getdevconfig __P((tc_addr_t, struct fb_devconfig *));
struct fb_devconfig tfb_console_dc;
tc_addr_t tfb_consaddr;

struct wsdisplay_emulops tfb_emulops = {
	rcons_cursor,			/* could use hardware cursor; punt */
	rcons_mapchar,
	rcons_putchar,
	rcons_copycols,
	rcons_erasecols,
	rcons_copyrows,
	rcons_eraserows,
	rcons_alloc_attr
};

struct wsscreen_descr tfb_stdscreen = {
	"std", 0, 0,
	&tfb_emulops,
	0, 0,
	0
};

const struct wsscreen_descr *_tfb_scrlist[] = {
	&tfb_stdscreen,
};

struct wsscreen_list tfb_screenlist = {
	sizeof(_tfb_scrlist) / sizeof(struct wsscreen_descr *), _tfb_scrlist
};

int	tfbioctl __P((void *, u_long, caddr_t, int, struct proc *));
int	tfbmmap __P((void *, off_t, int));

int	tfb_alloc_screen __P((void *, const struct wsscreen_descr *,
				      void **, int *, int *, long *));
void	tfb_free_screen __P((void *, void *));
void	tfb_show_screen __P((void *, void *));

struct wsdisplay_accessops tfb_accessops = {
	tfbioctl,
	tfbmmap,
	tfb_alloc_screen,
	tfb_free_screen,
	tfb_show_screen,
	0 /* load_font */
};

int  tfb_cnattach __P((tc_addr_t));
int  tfbintr __P((void *));
void tfbinit __P((struct fb_devconfig *));

static int  get_cmap __P((struct tfb_softc *, struct wsdisplay_cmap *));
static int  set_cmap __P((struct tfb_softc *, struct wsdisplay_cmap *));
static int  set_cursor __P((struct tfb_softc *, struct wsdisplay_cursor *));
static int  get_cursor __P((struct tfb_softc *, struct wsdisplay_cursor *));
static void set_curpos __P((struct tfb_softc *, struct wsdisplay_curpos *));
static void bt431_set_curpos __P((struct tfb_softc *));

/* bit order reverse */
const static u_int8_t flip[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

int
tfbmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct tc_attach_args *ta = aux;

	if (strncmp("PMAG-RO ", ta->ta_modname, TC_ROM_LLEN) != 0
	    && strncmp("PMAG-JA ", ta->ta_modname, TC_ROM_LLEN) != 0)
		return (0);

	return (1);
}

void
tfb_getdevconfig(dense_addr, dc)
	tc_addr_t dense_addr;
	struct fb_devconfig *dc;
{
	struct raster *rap;
	struct rcons *rcp;
	int i;

	dc->dc_vaddr = dense_addr;
	dc->dc_paddr = MACHINE_KSEG0_TO_PHYS(dc->dc_vaddr);

	dc->dc_wid = 1280;
	dc->dc_ht = 1024;
	dc->dc_depth = 8;
	dc->dc_rowbytes = 1280;
	dc->dc_videobase = dc->dc_vaddr + TX_8FB_OFFSET;
	dc->dc_blanked = 0;

	/* initialize colormap and cursor resource */
	tfbinit(dc);

	/* clear the screen */
	for (i = 0; i < dc->dc_ht * dc->dc_rowbytes; i += sizeof(u_int32_t))
		*(u_int32_t *)(dc->dc_videobase + i) = 0x0;

	/* initialize the raster */
	rap = &dc->dc_raster;
	rap->width = dc->dc_wid;
	rap->height = dc->dc_ht;
	rap->depth = dc->dc_depth;
	rap->linelongs = dc->dc_rowbytes / sizeof(u_int32_t);
	rap->pixels = (u_int32_t *)dc->dc_videobase;

	/* initialize the raster console blitter */
	rcp = &dc->dc_rcons;
	rcp->rc_sp = rap;
	rcp->rc_crow = rcp->rc_ccol = -1;
	rcp->rc_crowp = &rcp->rc_crow;
	rcp->rc_ccolp = &rcp->rc_ccol;
	rcons_init(rcp, 34, 80);

	tfb_stdscreen.nrows = dc->dc_rcons.rc_maxrow;
	tfb_stdscreen.ncols = dc->dc_rcons.rc_maxcol;
}

void
tfbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct tfb_softc *sc = (struct tfb_softc *)self;
	struct tc_attach_args *ta = aux;
	struct wsemuldisplaydev_attach_args waa;
	struct hwcmap256 *cm;
	int console;

	console = (ta->ta_addr == tfb_consaddr);
	if (console) {
		sc->sc_dc = &tfb_console_dc;
		sc->nscreens = 1;
	}
	else {
		sc->sc_dc = (struct fb_devconfig *)
		    malloc(sizeof(struct fb_devconfig), M_DEVBUF, M_WAITOK);
		tfb_getdevconfig(ta->ta_addr, sc->sc_dc);
	}
	printf(": %d x %d, 8,24bpp\n", sc->sc_dc->dc_wid, sc->sc_dc->dc_ht);

	cm = &sc->sc_cmap;
	memset(cm, 255, sizeof(struct hwcmap256));	/* XXX */
	cm->r[0] = cm->g[0] = cm->b[0] = 0;		/* XXX */

	sc->sc_cursor.cc_magic.x = TX_MAGIC_X;
	sc->sc_cursor.cc_magic.y = TX_MAGIC_Y;

	tc_intr_establish(parent, ta->ta_cookie, TC_IPL_TTY, tfbintr, sc);

	*(u_int8_t *)(sc->sc_dc->dc_vaddr + TX_CONTROL) &= ~0x40;
	*(u_int8_t *)(sc->sc_dc->dc_vaddr + TX_CONTROL) |= 0x40;

	waa.console = console;
	waa.scrdata = &tfb_screenlist;
	waa.accessops = &tfb_accessops;
	waa.accesscookie = sc;

	config_found(self, &waa, wsemuldisplaydevprint);
}

int
tfbioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct tfb_softc *sc = v;
	struct fb_devconfig *dc = sc->sc_dc;
	int turnoff;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = /* WSDISPLAY_TYPE_TX */ 0x19980910;
		return (0);

	case WSDISPLAYIO_GINFO:
#define	wsd_fbip ((struct wsdisplay_fbinfo *)data)
		wsd_fbip->height = sc->sc_dc->dc_ht;
		wsd_fbip->width = sc->sc_dc->dc_wid;
		wsd_fbip->depth = sc->sc_dc->dc_depth;
		wsd_fbip->cmsize = CMAP_SIZE;
#undef fbt
		return (0);

	case WSDISPLAYIO_GETCMAP:
		return get_cmap(sc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return set_cmap(sc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_SVIDEO:
		turnoff = *(int *)data == WSDISPLAYIO_VIDEO_OFF;
		if ((dc->dc_blanked == 0) ^ turnoff) {
			dc->dc_blanked = turnoff;
#if 0	/* XXX later XXX */		
	To turn off;
	- clear the MSB of TX control register; &= ~0x80,
	- assign Bt431 register #0 with value 0x4 to hide sprite cursor.
#endif	/* XXX XXX XXX */
		}
		return (0);

	case WSDISPLAYIO_GVIDEO:
		*(u_int *)data = dc->dc_blanked ?
		    WSDISPLAYIO_VIDEO_OFF : WSDISPLAYIO_VIDEO_ON;
		return (0);

	case WSDISPLAYIO_GCURPOS:
		*(struct wsdisplay_curpos *)data = sc->sc_cursor.cc_pos;
		return (0);

	case WSDISPLAYIO_SCURPOS:
		set_curpos(sc, (struct wsdisplay_curpos *)data);
		bt431_set_curpos(sc);
		return (0);

	case WSDISPLAYIO_GCURMAX:
		((struct wsdisplay_curpos *)data)->x =
		((struct wsdisplay_curpos *)data)->y = CURSOR_MAX_SIZE;
		return (0);

	case WSDISPLAYIO_GCURSOR:
		return get_cursor(sc, (struct wsdisplay_cursor *)data);

	case WSDISPLAYIO_SCURSOR:
		return set_cursor(sc, (struct wsdisplay_cursor *)data);
	}
	return (ENOTTY);
}

int
tfbmmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct tfb_softc *sc = v;

	if (offset >= TX_8FB_SIZE || offset < 0)
		return (-1);
	return machine_btop(sc->sc_dc->dc_paddr + TX_8FB_OFFSET + offset);
}

int
tfb_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct tfb_softc *sc = v;
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
tfb_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct tfb_softc *sc = v;

	if (sc->sc_dc == &tfb_console_dc)
		panic("tfb_free_screen: console");

	sc->nscreens--;
}

void
tfb_show_screen(v, cookie)
	void *v;
	void *cookie;
{
}

int
tfb_cnattach(addr)
        tc_addr_t addr;
{
        struct fb_devconfig *dcp = &tfb_console_dc;
        long defattr;

        tfb_getdevconfig(addr, dcp);
 
        rcons_alloc_attr(&dcp->dc_rcons, 0, 0, 0, &defattr);

        wsdisplay_cnattach(&tfb_stdscreen, &dcp->dc_rcons,
                           0, 0, defattr);
        tfb_consaddr = addr;
        return(0);
}

int
tfbintr(arg)
	void *arg;
{
	struct tfb_softc *sc = arg;
	caddr_t tfbbase = (caddr_t)sc->sc_dc->dc_vaddr;
	void *vdac, *curs;
	int v;
	
	*(u_int8_t *)(tfbbase + TX_CONTROL) &= ~0x40;
	if (sc->sc_changed == 0)
		goto done;

	vdac = (void *)(tfbbase + TX_BT463_OFFSET);
	curs = (void *)(tfbbase + TX_BT431_OFFSET);
	v = sc->sc_changed;
	sc->sc_changed = 0;
	if (v & DATA_ENB_CHANGED) {
		SELECT431(curs, BT431_REG_COMMAND);
		HALF(curs, bt_ctl) = (sc->sc_curenb) ? 0x4444 : 0x0404;
	}
	if (v & DATA_CURCMAP_CHANGED) {
		u_int8_t *cp = sc->sc_cursor.cc_color;

		SELECT463(vdac, BT463_IREG_CURSOR_COLOR_0);
#if 0
		BYTE(vdac, bt_reg) = cp[1]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[3]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[5]; tc_wmb();

		BYTE(vdac, bt_reg) = cp[0]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[2]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[4]; tc_wmb();

		BYTE(vdac, bt_reg) = cp[1]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[3]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[5]; tc_wmb();

		BYTE(vdac, bt_reg) = cp[1]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[3]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[5]; tc_wmb();
#else
		BYTE(vdac, bt_reg) = cp[0]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[2]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[4]; tc_wmb();

		BYTE(vdac, bt_reg) = cp[1]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[3]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[5]; tc_wmb();

		BYTE(vdac, bt_reg) = cp[0]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[2]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[4]; tc_wmb();

		BYTE(vdac, bt_reg) = cp[0]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[2]; tc_wmb();
		BYTE(vdac, bt_reg) = cp[4]; tc_wmb();
#endif
	}
	if (v & DATA_CURSHAPE_CHANGED) {
		u_int8_t *ip, *mp, img, msk;
		int bcnt;

		ip = (u_int8_t *)sc->sc_cursor.cc_image;
		mp = (u_int8_t *)(sc->sc_cursor.cc_image + CURSOR_MAX_SIZE);
		bcnt = 0;
		SELECT431(curs, BT431_REG_CRAM_BASE);

		/* 64 pixel scan line is consisted with 16 byte cursor ram */
		while (bcnt < sc->sc_cursor.cc_size.y * 16) {
			/* pad right half 32 pixel when smaller than 33 */
			if ((bcnt & 0x8) && sc->sc_cursor.cc_size.x < 33) {
				HALF(curs, bt_ram) = 0;
				tc_wmb();
			}
			else {
				img = *ip++;
				msk = *mp++;
				img &= msk;	/* cookie off image */
				HALF(curs, bt_ram)
				    = (flip[msk] << 8) | flip[img];
				tc_wmb();
			}
			bcnt += 2;
		}
		/* pad unoccupied scan lines */
		while (bcnt < CURSOR_MAX_SIZE * 16) {
			HALF(curs, bt_ram) = 0;
			tc_wmb();
			bcnt += 2;
		}
	}
	if (v & DATA_CMAP_CHANGED) {
		struct hwcmap256 *cm = &sc->sc_cmap;
		int index;

		SELECT463(vdac, BT463_IREG_CPALETTE_RAM);
		for (index = 0; index < CMAP_SIZE; index++) {
			BYTE(vdac, bt_cmap) = cm->r[index];
			BYTE(vdac, bt_cmap) = cm->g[index];
			BYTE(vdac, bt_cmap) = cm->b[index];
		}
	}
done:
	*(u_int8_t *)(tfbbase + TX_CONTROL) &= ~0x40;	/* !? Eeeh !? */
	*(u_int8_t *)(tfbbase + TX_CONTROL) |= 0x40;
	return (1);
}

void
tfbinit(dc)
	struct fb_devconfig *dc;
{
	caddr_t tfbbase = (caddr_t)dc->dc_vaddr;
	void *vdac = (void *)(tfbbase + TX_BT463_OFFSET);
	void *curs = (void *)(tfbbase + TX_BT431_OFFSET);
	int i;

	SELECT463(vdac, BT463_IREG_COMMAND_0);
	BYTE(vdac, bt_reg) = 0x40;	tc_wmb();	/* CMD 0 */
	BYTE(vdac, bt_reg) = 0x46;	tc_wmb();	/* CMD 1 */
	BYTE(vdac, bt_reg) = 0xc0;	tc_wmb();	/* CMD 2 */
	BYTE(vdac, bt_reg) = 0;		tc_wmb();	/* !? 204 !? */
	BYTE(vdac, bt_reg) = 0xff;	tc_wmb();	/* plane  0:7  */
	BYTE(vdac, bt_reg) = 0xff;	tc_wmb();	/* plane  8:15 */
	BYTE(vdac, bt_reg) = 0xff;	tc_wmb();	/* plane 16:23 */
	BYTE(vdac, bt_reg) = 0xff;	tc_wmb();	/* plane 24:27 */
	BYTE(vdac, bt_reg) = 0x00;	tc_wmb();	/* blink  0:7  */
	BYTE(vdac, bt_reg) = 0x00;	tc_wmb();	/* blink  8:15 */
	BYTE(vdac, bt_reg) = 0x00;	tc_wmb();	/* blink 16:23 */
	BYTE(vdac, bt_reg) = 0x00;	tc_wmb();	/* blink 24:27 */
	BYTE(vdac, bt_reg) = 0x00;	tc_wmb();

#if 0 /* XXX ULTRIX does initialize 16 entry window type here XXX */
  {
	static u_int32_t windowtype[BT463_IREG_WINDOW_TYPE_TABLE] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};

	SELECT463(vdac, BT463_IREG_WINDOW_TYPE_TABLE);
	for (i = 0; i < BT463_NWTYPE_ENTRIES; i++) {
		BYTE(vdac, bt_reg) = windowtype[i];	  /*   0:7  */
		BYTE(vdac, bt_reg) = windowtype[i] >> 8;  /*   8:15 */
		BYTE(vdac, bt_reg) = windowtype[i] >> 16; /*  16:23 */
	}
  }
#endif

	SELECT463(vdac, BT463_IREG_CPALETTE_RAM);
	BYTE(vdac, bt_cmap) = 0;		tc_wmb();
	BYTE(vdac, bt_cmap) = 0;		tc_wmb();
	BYTE(vdac, bt_cmap) = 0;		tc_wmb();
	for (i = 1; i < 256; i++) {
		BYTE(vdac, bt_cmap) = 0xff;	tc_wmb();
		BYTE(vdac, bt_cmap) = 0xff;	tc_wmb();
		BYTE(vdac, bt_cmap) = 0xff;	tc_wmb();
	}

	/* !? Eeeh !? */
	SELECT463(vdac, 0x0100 /* BT463_IREG_CURSOR_COLOR_0 */);
	for (i = 0; i < 256; i++) {
		BYTE(vdac, bt_cmap) = i;	tc_wmb();
		BYTE(vdac, bt_cmap) = i;	tc_wmb();
		BYTE(vdac, bt_cmap) = i;	tc_wmb();
	}

	SELECT431(curs, BT431_REG_COMMAND);
	HALF(curs, bt_ctl) = 0x0404;		tc_wmb();
	HALF(curs, bt_ctl) = 0; /* XLO */	tc_wmb();
	HALF(curs, bt_ctl) = 0; /* XHI */	tc_wmb();
	HALF(curs, bt_ctl) = 0; /* YLO */	tc_wmb();
	HALF(curs, bt_ctl) = 0; /* YHI */	tc_wmb();
	HALF(curs, bt_ctl) = 0; /* XWLO */	tc_wmb();
	HALF(curs, bt_ctl) = 0; /* XWHI */	tc_wmb();
	HALF(curs, bt_ctl) = 0; /* WYLO */	tc_wmb();
	HALF(curs, bt_ctl) = 0; /* WYLO */	tc_wmb();
	HALF(curs, bt_ctl) = 0; /* WWLO */	tc_wmb();
	HALF(curs, bt_ctl) = 0; /* WWHI */	tc_wmb();
	HALF(curs, bt_ctl) = 0; /* WHLO */	tc_wmb();
	HALF(curs, bt_ctl) = 0; /* WHHI */	tc_wmb();

	SELECT431(curs, BT431_REG_CRAM_BASE);
	for (i = 0; i < 512; i++) {
		HALF(curs, bt_ram) = 0;	tc_wmb();
	}
}

static int
get_cmap(sc, p)
	struct tfb_softc *sc;
	struct wsdisplay_cmap *p;
{
	u_int index = p->index, count = p->count;

	if (index >= CMAP_SIZE || (index + count) > CMAP_SIZE)
		return (EINVAL);

	if (!uvm_useracc(p->red, count, B_WRITE) ||
	    !uvm_useracc(p->green, count, B_WRITE) ||
	    !uvm_useracc(p->blue, count, B_WRITE))
		return (EFAULT);

	copyout(&sc->sc_cmap.r[index], p->red, count);
	copyout(&sc->sc_cmap.g[index], p->green, count);
	copyout(&sc->sc_cmap.b[index], p->blue, count);

	return (0);
}

static int
set_cmap(sc, p)
	struct tfb_softc *sc;
	struct wsdisplay_cmap *p;
{
	u_int index = p->index, count = p->count;

	if (index >= CMAP_SIZE || (index + count) > CMAP_SIZE)
		return (EINVAL);

	if (!uvm_useracc(p->red, count, B_READ) ||
	    !uvm_useracc(p->green, count, B_READ) ||
	    !uvm_useracc(p->blue, count, B_READ))
		return (EFAULT);

	copyin(p->red, &sc->sc_cmap.r[index], count);
	copyin(p->green, &sc->sc_cmap.g[index], count);
	copyin(p->blue, &sc->sc_cmap.b[index], count);

	sc->sc_changed |= DATA_CMAP_CHANGED;

	return (0);
}

static int
set_cursor(sc, p)
	struct tfb_softc *sc;
	struct wsdisplay_cursor *p;
{
#define	cc (&sc->sc_cursor)
	int v, index, count, icount;

	v = p->which;
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		index = p->cmap.index;
		count = p->cmap.count;
		if (index >= 2 || (index + count) > 2)
			return (EINVAL);
		if (!uvm_useracc(p->cmap.red, count, B_READ) ||
		    !uvm_useracc(p->cmap.green, count, B_READ) ||
		    !uvm_useracc(p->cmap.blue, count, B_READ))
			return (EFAULT);
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		if (p->size.x > CURSOR_MAX_SIZE || p->size.y > CURSOR_MAX_SIZE)
			return (EINVAL);
		icount = ((p->size.x < 33) ? 4 : 8) * p->size.y;
		if (!uvm_useracc(p->image, icount, B_READ) ||
		    !uvm_useracc(p->mask, icount, B_READ))
			return (EFAULT);
	}
	if (v & (WSDISPLAY_CURSOR_DOPOS | WSDISPLAY_CURSOR_DOCUR)) {
		if (v & WSDISPLAY_CURSOR_DOCUR)
			cc->cc_hot = p->hot;
		if (v & WSDISPLAY_CURSOR_DOPOS)
			set_curpos(sc, &p->pos);
		bt431_set_curpos(sc);
	}

	sc->sc_changed = 0;
	if (v & WSDISPLAY_CURSOR_DOCUR) {
		sc->sc_curenb = p->enable;
		sc->sc_changed |= DATA_ENB_CHANGED;
	}
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		copyin(p->cmap.red, &cc->cc_color[index], count);
		copyin(p->cmap.green, &cc->cc_color[index + 2], count);
		copyin(p->cmap.blue, &cc->cc_color[index + 4], count);
		sc->sc_changed |= DATA_CURCMAP_CHANGED;
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		cc->cc_size = p->size;
		memset(cc->cc_image, 0, sizeof cc->cc_image);
		copyin(p->image, cc->cc_image, icount);
		copyin(p->mask, cc->cc_image+CURSOR_MAX_SIZE, icount);
		sc->sc_changed |= DATA_CURSHAPE_CHANGED;
	}

	return (0);
#undef cc
}

static int
get_cursor(sc, p)
	struct tfb_softc *sc;
	struct wsdisplay_cursor *p;
{
	return (ENOTTY); /* XXX */
}

static void
set_curpos(sc, curpos)
	struct tfb_softc *sc;
	struct wsdisplay_curpos *curpos;
{
	struct fb_devconfig *dc = sc->sc_dc;
	int x = curpos->x, y = curpos->y;

	if (y < 0)
		y = 0;
	else if (y > dc->dc_ht)
		y = dc->dc_ht;
	if (x < 0)
		x = 0;
	else if (x > dc->dc_wid)
		x = dc->dc_wid;
	sc->sc_cursor.cc_pos.x = x;
	sc->sc_cursor.cc_pos.y = y;
}

void
bt431_set_curpos(sc)
	struct tfb_softc *sc;
{
	caddr_t tfbbase = (caddr_t)sc->sc_dc->dc_vaddr;
	void *curs = (void *)(tfbbase + TX_BT431_OFFSET);
	u_int16_t twin;
	int x, y, s;

	x = sc->sc_cursor.cc_pos.x - sc->sc_cursor.cc_hot.x;
	y = sc->sc_cursor.cc_pos.y - sc->sc_cursor.cc_hot.y;

	x += sc->sc_cursor.cc_magic.x;
	y += sc->sc_cursor.cc_magic.y;

	s = spltty();

	SELECT431(curs, BT431_REG_CURSOR_X_LOW);
	HALF(curs, bt_ctl) = TWIN_LO(x);	tc_wmb();
	HALF(curs, bt_ctl) = TWIN_HI(x);	tc_wmb();
	HALF(curs, bt_ctl) = TWIN_LO(y);	tc_wmb();
	HALF(curs, bt_ctl) = TWIN_HI(y);	tc_wmb();

	splx(s);
}
