/*	$NetBSD: hpcfb.c,v 1.15.2.2 2000/11/20 20:45:53 bouyer Exp $	*/

/*-
 * Copyright (c) 1999
 *         Shin Takemura and PocketBSD Project. All rights reserved.
 * Copyright (c) 2000
 *         SATO Kazumi. All rights reserved.
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
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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
 */

/*
 * multiscreen, virtual text vram and hpcfb_emulops functions
 * written by SATO Kazumi.
 */

#define FBDEBUG
static const char _copyright[] __attribute__ ((unused)) =
    "Copyright (c) 1999 Shin Takemura.  All rights reserved.";
static const char _rcsid[] __attribute__ ((unused)) =
    "$Id: hpcfb.c,v 1.15.2.2 2000/11/20 20:45:53 bouyer Exp $";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/autoconf.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_callbacks.h>

#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#include <arch/hpcmips/dev/hpcfbvar.h>
#include <arch/hpcmips/dev/hpcfbio.h>

#include "bivideo.h"
#if NBIVIDEO > 0
#include <arch/hpcmips/dev/bivideovar.h>
#endif

#ifdef FBDEBUG
int	hpcfb_debug = 0;
#define	DPRINTF(arg) if (hpcfb_debug) printf arg;
#else
#define	DPRINTF(arg)
#endif

/*
 * currently experimental
#define HPCFB_JUMP
#define HPCFB_MULTI
#define HPCFB_BSTORE
*/

struct hpcfb_vchar {
	u_int c;
	long attr;
};

struct hpcfb_devconfig {
	struct rasops_info	dc_rinfo;	/* rasops infomation */

	int		dc_blanked;	/* currently had video disabled */
	struct hpcfb_softc *dc_sc;
	int dc_rows;
	int dc_cols;
	struct hpcfb_vchar *dc_tvram;
	int dc_curx;
	int dc_cury;
#ifdef HPCFB_JUMP
	int dc_min_row;
	int dc_max_row;
	int dc_scroll;
	struct callout *dc_scroll_ch;
	int dc_scroll_src;
	int dc_scroll_dst;
	int dc_scroll_num;
#endif /* HPCFB_JUMP */
	int dc_state;
#define HPCFB_DC_CURRENT		0x80000000
#define HPCFB_DC_DRAWING		0x01
#define HPCFB_DC_SCROLLPENDING		0x02
#define HPCFB_DC_UPDATE			0x04
#ifdef HPCFB_BSTORE
	u_char *dc_bstore;
#endif /* HPCFB_BSTORE */
#ifdef HPCFB_MULTI
	int dc_scrno;
#endif /* HPCFB_MULTI */
#if defined(HPCFB_BSTORE) || defined(HPCFB_MULTI)
	int	dc_memsize;
#endif /* defined(HPCFB_BSTORE) || defined(HPCFB_MULTI) */
};

#define HPCFB_MAX_SCREEN 5
#define HPCFB_MAX_JUMP 5

struct hpcfb_softc {
	struct	device sc_dev;
	struct	hpcfb_devconfig *sc_dc;	/* device configuration */
#ifdef HPCFB_MULTI
	struct	hpcfb_devconfig *screens[HPCFB_MAX_SCREEN];
#endif /* HPCFB_MULTI */
	const struct hpcfb_accessops	*sc_accessops;
	void *sc_accessctx;
	int nscreens;
	void			*sc_powerhook;	/* power management hook */
	struct device *sc_wsdisplay;
	int sc_screen_resumed;
};
/*
 *  function prototypes
 */
int	hpcfbmatch __P((struct device *, struct cfdata *, void *));
void	hpcfbattach __P((struct device *, struct device *, void *));
int	hpcfbprint __P((void *aux, const char *pnp));

int	hpcfb_ioctl __P((void *, u_long, caddr_t, int, struct proc *));
paddr_t	hpcfb_mmap __P((void *, off_t, int));
void	hpcfb_refresh_screen __P((struct hpcfb_softc *sc));
static int	hpcfb_init __P((struct hpcfb_fbconf *fbconf,
				struct hpcfb_devconfig *dc));
static int	hpcfb_alloc_screen __P((void *, const struct wsscreen_descr *,
				     void **, int *, int *, long *));
static void	hpcfb_free_screen __P((void *, void *));
static int	hpcfb_show_screen __P((void *, void *, int,
				    void (*) (void *, int, int), void *));
static void	hpcfb_power __P((int, void *));
static void	hpcfb_cmap_reorder __P((struct hpcfb_fbconf *,
					struct hpcfb_devconfig *));

static int	pow __P((int, int));

void    hpcfb_cursor __P((void *c, int on, int row, int col));
int     hpcfb_mapchar __P((void *, int, unsigned int *));
void    hpcfb_putchar __P((void *c, int row, int col, u_int uc, long attr));
void    hpcfb_copycols __P((void *c, int row, int srccol, 
			int dstcol, int ncols));
void    hpcfb_erasecols __P((void *c, int row, int startcol, 
			int ncols, long attr));
void    hpcfb_redraw __P((void *c, int row, int nrows));
void    hpcfb_copyrows __P((void *c, int srcrow, int dstrow, int nrows));
void    hpcfb_eraserows __P((void *c, int row, int nrows, long attr));
int     hpcfb_alloc_attr __P((void *c, int fg, int bg, int flags, long *attr));

struct wsdisplay_emulops hpcfb_emulops = {
	hpcfb_cursor,
	hpcfb_mapchar,
	hpcfb_putchar,
	hpcfb_copycols,
	hpcfb_erasecols,
	hpcfb_copyrows,
	hpcfb_eraserows,
	hpcfb_alloc_attr
};

/*
 *  static variables
 */
struct cfattach hpcfb_ca = {
	sizeof(struct hpcfb_softc), hpcfbmatch, hpcfbattach,
};

struct wsscreen_descr hpcfb_stdscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global */
	&hpcfb_emulops,	/* XXX */
	0, 0,
	WSSCREEN_REVERSE
};

const struct wsscreen_descr *_hpcfb_scrlist[] = {
	&hpcfb_stdscreen,
	/* XXX other formats, graphics screen? */
};

struct wsscreen_list hpcfb_screenlist = {
	sizeof(_hpcfb_scrlist) / sizeof(struct wsscreen_descr *),
	_hpcfb_scrlist
};

struct wsdisplay_accessops hpcfb_accessops = {
	hpcfb_ioctl,
	hpcfb_mmap,
	hpcfb_alloc_screen,
	hpcfb_free_screen,
	hpcfb_show_screen,
	0 /* load_font */
};

void    hpcfb_tv_putchar __P((struct hpcfb_devconfig *, int, int, u_int, long));
void    hpcfb_tv_copycols __P((struct hpcfb_devconfig *, int, int, int, int));
void    hpcfb_tv_erasecols __P((struct hpcfb_devconfig *, int, int, int, long));
void    hpcfb_tv_copyrows __P((struct hpcfb_devconfig *, int, int, int));
void    hpcfb_tv_eraserows __P((struct hpcfb_devconfig *, int, int, long));

struct wsdisplay_emulops rasops_emul;

static int hpcfbconsole;
struct hpcfb_devconfig hpcfb_console_dc;
struct wsscreen_descr hpcfb_console_wsscreen;
struct hpcfb_vchar hpcfb_console_tvram[200*200];

/*
 *  function bodies
 */
static int
pow(int x, int n)
{
	int res = 1;
	while (0 < n--) {
		res *= x;
	}
	return (res);
}

int
hpcfbmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
#if 0
	struct hpcfb_attach_args *ha = aux;
#endif

	return (1);
}

void
hpcfbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct hpcfb_softc *sc = (struct hpcfb_softc *)self;
	struct hpcfb_attach_args *ha = aux;
	struct wsemuldisplaydev_attach_args wa;

	sc->sc_accessops = ha->ha_accessops;
	sc->sc_accessctx = ha->ha_accessctx;

	if (hpcfbconsole) {
#ifdef HPCFB_MULTI
		sc->screens[0] = 
#endif /* HPCFB_MULTI */
		sc->sc_dc = &hpcfb_console_dc;
		sc->nscreens = 1;
		sc->sc_dc->dc_state |= HPCFB_DC_CURRENT;
		hpcfb_console_dc.dc_sc = sc;
	} else {
#ifdef HPCFB_MULTI
		sc->screens[0] = 
#endif /* HPCFB_MULTI */
		sc->sc_dc = (struct hpcfb_devconfig *)
		    malloc(sizeof(struct hpcfb_devconfig), M_DEVBUF, M_WAITOK);
		sc->nscreens = 1;
		bzero(sc->sc_dc, sizeof(struct hpcfb_devconfig));
		if (hpcfb_init(&ha->ha_fbconflist[0], sc->sc_dc) != 0) {
			return;
		}
		sc->sc_dc->dc_state |= HPCFB_DC_CURRENT;
		sc->sc_dc->dc_sc = sc;
	}
	hpcfb_stdscreen.nrows = sc->sc_dc->dc_rows;
        hpcfb_stdscreen.ncols = sc->sc_dc->dc_cols;
	hpcfb_stdscreen.capabilities = sc->sc_dc->dc_rinfo.ri_caps;
	printf(": hpcrasops %dx%d pixels, %d colors, %dx%d chars: ",
	       sc->sc_dc->dc_rinfo.ri_width,
	       sc->sc_dc->dc_rinfo.ri_height,
	       pow(2, sc->sc_dc->dc_rinfo.ri_depth),
	       sc->sc_dc->dc_rinfo.ri_cols,
	       sc->sc_dc->dc_rinfo.ri_rows);
#ifdef HPCFB_MULTI
	printf(" multi");
#endif /* HPCFB_MULTI */
#ifdef HPCFB_BSTORE
	printf(" bstore");
#endif /* HPCFB_BSTORE */
	printf("\n");

	/* Set video chip dependent CLUT if any. */
	if (hpcfbconsole && sc->sc_accessops->setclut) {
		sc->sc_accessops->setclut(sc->sc_accessctx, 
					  &hpcfb_console_dc.dc_rinfo);
	}

	/* Add a power hook to power management */
	sc->sc_powerhook = powerhook_establish(hpcfb_power, sc);
	if (sc->sc_powerhook == NULL)
		printf("%s: WARNING: unable to establish power hook\n",
			sc->sc_dev.dv_xname);

	wa.console = hpcfbconsole;
	wa.scrdata = &hpcfb_screenlist;
	wa.accessops = &hpcfb_accessops;
	wa.accesscookie = sc;

	sc->sc_wsdisplay = config_found(self, &wa, wsemuldisplaydevprint);
}

/* Print function (for parent devices). */
int
hpcfbprint(aux, pnp)
	void *aux;
	const char *pnp;
{
#if 0
	struct hpcfb_attach_args *ha = aux;
#endif

	if (pnp)
		printf("hpcfb at %s", pnp);

	return (UNCONF);
}

int
hpcfb_cnattach(fbconf)
	struct hpcfb_fbconf *fbconf;
{
	struct hpcfb_fbconf __fbconf __attribute__((__unused__));
	long defattr;

	bzero(&hpcfb_console_dc, sizeof(struct hpcfb_devconfig));
#if NBIVIDEO > 0
	if (fbconf == 0) {
		memset(&__fbconf, 0, sizeof(struct hpcfb_fbconf));
		if (bivideo_getcnfb(&__fbconf) != 0)
			return (ENXIO);
		fbconf = &__fbconf;
	}
#endif /* NBIVIDEO > 0 */
	if (hpcfb_init(fbconf, &hpcfb_console_dc) != 0)
		return (ENXIO);

	hpcfb_console_wsscreen = hpcfb_stdscreen;
	hpcfb_console_wsscreen.nrows = hpcfb_console_dc.dc_rows;
	hpcfb_console_wsscreen.ncols = hpcfb_console_dc.dc_cols;
	hpcfb_console_wsscreen.capabilities = hpcfb_console_dc.dc_rinfo.ri_caps;
	hpcfb_alloc_attr(&hpcfb_console_dc, 7, 0, 0, &defattr);
	wsdisplay_cnattach(&hpcfb_console_wsscreen, &hpcfb_console_dc,
			   0, 0, defattr);
	hpcfbconsole = 1;

	return (0);
}

int
hpcfb_init(fbconf, dc)
	struct hpcfb_fbconf *fbconf;
	struct hpcfb_devconfig *dc;
{
	struct rasops_info *ri;
	vaddr_t fbaddr;

	fbaddr = (vaddr_t)fbconf->hf_baseaddr + fbconf->hf_offset;

	/* init rasops */
	ri = &dc->dc_rinfo;
	bzero(ri, sizeof(struct rasops_info));
	ri->ri_depth = fbconf->hf_pixel_width;
	ri->ri_bits = (caddr_t)fbaddr;
	ri->ri_width = fbconf->hf_width;
	ri->ri_height = fbconf->hf_height;
	ri->ri_stride = fbconf->hf_bytes_per_line;
#if 0
	ri->ri_flg = RI_FORCEMONO | RI_CURSOR;
#else
	ri->ri_flg = RI_CURSOR;
#endif
	if (rasops_init(ri, 200, 200)) {
		panic("%s(%d): rasops_init() failed!", __FILE__, __LINE__);
	}

	/* over write color map of rasops */
	hpcfb_cmap_reorder (fbconf, dc);

	dc->dc_curx = -1;
	dc->dc_cury = -1;
	dc->dc_rows = dc->dc_rinfo.ri_rows;
	dc->dc_cols = dc->dc_rinfo.ri_cols;
#ifdef HPCFB_JUMP
	dc->dc_max_row = 0;
	dc->dc_min_row = dc->dc_rows;
	dc->dc_scroll = 0;
	callout_init(dc->dc_scroll_ch);
#endif /* HPCFB_JUMP */
	dc->dc_tvram = hpcfb_console_tvram;
	bzero(hpcfb_console_tvram, sizeof(hpcfb_console_tvram));
#if defined(HPCFB_BSTORE) || defined(HPCFB_MULTI)
	dc->dc_memsize = ri->ri_stride * ri->ri_height;
#endif /* defined(HPCFB_BSTORE) || defined(HPCFB_MULTI) */
#ifdef HPCFB_BSTORE
	if (dc->dc_bstore == NULL) {
		dc->dc_bstore =
			malloc(dc->dc_memsize, M_DEVBUF, M_WAITOK);
		bzero(dc->dc_bsrore, dc->dc_memsize);
	}
#endif /* HPCFB_BSTORE */
#ifdef HPCFB_MULTI
	dc->dc_scrno = 0;
#endif /* HPCFB_MULTI */
	/* hook rasops in hpcfb_ops */
	rasops_emul = ri->ri_ops; /* struct copy */
	ri->ri_ops = hpcfb_emulops; /* struct copy */

	return (0);
}

static void
hpcfb_cmap_reorder(fbconf, dc)
	struct hpcfb_fbconf *fbconf;
	struct hpcfb_devconfig *dc;
{
	struct rasops_info *ri = &dc->dc_rinfo;
	int reverse = fbconf->hf_access_flags & HPCFB_ACCESS_REVERSE;
	int *cmap = ri->ri_devcmap;
	vaddr_t fbaddr = (vaddr_t)fbconf->hf_baseaddr + fbconf->hf_offset;
	int i, j, bg, fg, tmp;

	/*
	 * Set forground and background so that the screen 
	 * looks black on white.
	 * Normally, black = 00 and white = ff.
	 * HPCFB_ACCESS_REVERSE means black = ff and white = 00.
	 */
	switch (fbconf->hf_pixel_width) {
	case 1:
		/* FALLTHROUGH */
	case 2:
		/* FALLTHROUGH */
	case 4:
		if (reverse) {
			bg = 0;
			fg = ~0;
		} else {
			bg = ~0;
			fg = 0;
		}
		/* for gray-scale LCD, hi-contrast color map */
		cmap[0] = bg;
		for (i = 1; i < 16; i++)
			cmap[i] = fg;
		break;
	case 8:
		/* FALLTHROUGH */
	case 16:
		if (reverse) {
			for (i = 0, j = 15; i < 8; i++, j--) {
				tmp = cmap[i];
				cmap[i] = cmap[j];
				cmap[j] = tmp;
			}
		}
		break;
	}

	/* clear the screen */
	bg = cmap[0];
	for (i = 0;
	     i < fbconf->hf_height * fbconf->hf_bytes_per_line;
	     i += sizeof(u_int32_t)) {
		*(u_int32_t *)(fbaddr + i) = bg;
	}
}

int
hpcfb_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct hpcfb_softc *sc = v;
	struct hpcfb_devconfig *dc = sc->sc_dc;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSKBDIO_BELL:
		return (0);
		break;

	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_HPCFB;
		return 0;

	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->height = dc->dc_rinfo.ri_height;
		wdf->width = dc->dc_rinfo.ri_width;
		wdf->depth = dc->dc_rinfo.ri_depth;
		wdf->cmsize = 256;	/* XXXX */
		return 0;		
		
	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
	case WSDISPLAYIO_GETPARAM:
	case WSDISPLAYIO_SETPARAM:
	case HPCFBIO_GCONF:
	case HPCFBIO_SCONF:
	case HPCFBIO_GDSPCONF:
	case HPCFBIO_SDSPCONF:
	case HPCFBIO_GOP:
	case HPCFBIO_SOP:
		return (*sc->sc_accessops->ioctl)(sc->sc_accessctx,
						  cmd, data, flag, p);

	default:
		if (IOCGROUP(cmd) != 't')
			DPRINTF(("%s(%d): hpcfb_ioctl(%lx, %lx) grp=%c num=%ld\n",
			 __FILE__, __LINE__,
			 cmd, (u_long)data, (char)IOCGROUP(cmd), cmd&0xff));
		break;
	}

	return (ENOTTY); /* Inappropriate ioctl for device */
}

paddr_t
hpcfb_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct hpcfb_softc *sc = v;

	return (*sc->sc_accessops->mmap)(sc->sc_accessctx, offset, prot);
}

static void 
hpcfb_power(why, arg)
	int why;
	void *arg;
{
	struct hpcfb_softc *sc = arg;

	switch (why) {
	case PWR_STANDBY:
		break;
	case PWR_SUSPEND:
		/* XXX, casting to 'struct wsdisplay_softc *' means
		   that you should not call the method here... */
		sc->sc_screen_resumed = wsdisplay_getactivescreen(
			(struct wsdisplay_softc *)sc->sc_wsdisplay);
		if (wsdisplay_switch(sc->sc_wsdisplay,
		    WSDISPLAY_NULLSCREEN,
		    1 /* waitok */) == 0) {
			wsscreen_switchwait(
				(struct wsdisplay_softc *)sc->sc_wsdisplay,
				WSDISPLAY_NULLSCREEN);
		} else {
			sc->sc_screen_resumed = WSDISPLAY_NULLSCREEN;
		}
#ifdef HPCFB_BSTORE
		if (sc->sc_dc->dc_bstore)
			bcopy(sc->sc_dc->dc_rinfo.ri_bits, sc->sc_dc->dc_bstore,
				sc->sc_dc->dc_memsize);
#endif /* HPCFB_BSTORE */
		break;
	case PWR_RESUME:
		if (sc->sc_screen_resumed != WSDISPLAY_NULLSCREEN)
			wsdisplay_switch(sc->sc_wsdisplay,
			    sc->sc_screen_resumed,
			    1 /* waitok */);
#ifdef HPCFB_BSTORE
		if (sc->sc_dc->dc_bstore)
			bcopy(sc->sc_dc->dc_bstore, 
				sc->sc_dc->dc_rinfo.ri_bits,
				sc->sc_dc->dc_memsize);
#endif /* HPCFB_BSTORE */
		break;
	}
}

void
hpcfb_refresh_screen(sc)
	struct hpcfb_softc *sc;
{
	struct hpcfb_devconfig *dc = sc->sc_dc;
	int x, y;

	/*
	 * refresh screen
	 */
	x = dc->dc_curx;
	y = dc->dc_cury;
	if (0 <= x && 0 <= y)
		hpcfb_cursor(dc, 0,  y, x); /* disable cursor */
	/* redraw all text */
	hpcfb_redraw(dc, 0, dc->dc_rows);
	if (0 <= x && 0 <= y)
		hpcfb_cursor(dc, 1,  y, x); /* enable cursor */
}

static int
hpcfb_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct hpcfb_softc *sc = v;
#ifdef HPCFB_MULTI
	struct hpcfb_devconfig *dc;
#endif /* HPCFB_MULTI */

	DPRINTF(("%s(%d): hpcfb_alloc_screen()\n", __FILE__, __LINE__));

#ifdef HPCFB_MULTI
	if (sc->nscreens > HPCFB_MAX_SCREEN)
		return (ENOMEM);


	if (sc->screens[sc->nscreens] == NULL){
		sc->screens[sc->nscreens] =
			malloc(sizeof(struct hpcfb_devconfig), M_DEVBUF, M_WAITOK);
		bzero(sc->screens[sc->nscreens], sizeof(struct hpcfb_devconfig));
	}
	dc = sc->screens[sc->nscreens];
	dc->dc_sc = sc;
	dc->dc_rinfo = sc->sc_dc->dc_rinfo;
	dc->dc_scrno = sc->nscreens;
	dc->dc_curx = -1;
	dc->dc_cury = -1;
	dc->dc_rows = sc->sc_dc->dc_rinfo.ri_rows;
	dc->dc_cols = sc->sc_dc->dc_rinfo.ri_cols;
	dc->dc_memsize = ri->ri_stride * ri->ri_height;
#ifdef HPCFB_BSTORE
	if (dc->dc_bstore == NULL) {
		dc->dc_bstore = 
			malloc(dc->dc_memsize, M_DEVBUF, M_WAITOK);
		bzero(dc->dc_bstore, dc->dc_memsize);
	}
#endif /* HPCFB_BSTORE */
	dc->dc_rinfo.ri_bits  = dc->dc_bstore;
	if (dc->dc_tvram == NULL){
		dc->dc_tvram = 
			malloc(sizeof(struct hpcfb_vchar)
				* dc->dc_rows
				* dc->dc_cols , M_DEVBUF, M_WAITOK);	
		bzero(dc->dc_tvram, 
				sizeof(struct hpcfb_vchar)
				* dc->dc_rows
				* dc->dc_cols);
	}
				
	*curxp = 0;
	*curyp = 0;
	sc->nscreens++;
	*cookiep = dc; 
	hpcfb_alloc_attr(*cookiep, 7, 0, 0, attrp);
	hpcfb_eraserows(*cookiep, 0, dc->dc_rows, *attrp);
#else /* HPCFB_MULTI */
	if (sc->nscreens > 0)
		return (ENOMEM);
	*curxp = 0;
	*curyp = 0;
	sc->nscreens++;
	*cookiep = &sc->sc_dc->dc_rinfo;
	sc->sc_dc->dc_rinfo.ri_ops.alloc_attr(*cookiep, 
					      7, 0, 0, attrp);
#endif /* HPCFB_MULTI */
	return (0);
}

static void
hpcfb_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct hpcfb_softc *sc = v;

#ifdef HPCFB_MULTI
	if (sc->nscreens == 1 && sc->sc_dc == &hpcfb_console_dc)
		panic("hpcfb_free_screen: console");
	sc->nscreens--;
#else /* HPCFB_MULTI */
	if (sc->sc_dc == &hpcfb_console_dc)
		panic("hpcfb_free_screen: console");

	sc->nscreens--;
#endif /* HPCFB_MULTI */
}

static int
hpcfb_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb) __P((void *, int, int));
	void *cbarg;
{
	struct hpcfb_softc *sc = v;
#ifdef HPCFB_MULTI
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
#endif /* HPCFB_MULTI */

	DPRINTF(("%s(%d): hpcfb_show_screen()\n", __FILE__, __LINE__));

#ifdef HPCFB_MULTI
	/* save current screen image */
	dc->dc_rinfo.ri_bits = sc->sc_dc->dc_rinfo.ri_bits;
	sc->sc_dc->dc_state &= ~HPCFB_DC_CURRENT;
#ifdef HPCFB_BSTORE
	if (sc->sc_dc->dc_bstore)
		bcopy(sc->sc_dc->dc_rinfo.ri_bits, sc->sc_dc->dc_bstore,
			sc->sc_dc->dc_memsize);
#endif /* HPCFB_BSTORE */
	/* switch screen image */
	sc->sc_dc->dc_rinfo.ri_bits = sc->sc_dc->dc_bstore;
	dc->dc_state |= HPCFB_DC_CURRENT;
	dc->dc_state |= HPCFB_DC_DRAWING;
	sc->sc_dc = dc;
	if (dc->dc_bstore) {
		bcopy(dc->dc_bstore, dc->dc_rinfo.ri_bits, dc->dc_memsize);
		dc->dc_state &= ~HPCFB_DC_DRAWING;
	} else {
		hpcfb_redraw(dc, 0, dc->dc_rows);
		dc->dc_state &= ~HPCFB_DC_DRAWING;
		if (dc->dc_curx > 0 && dc->dc_cury > 0)
			hpcfb_cursor(dc, 1,  dc->dc_cury, dc->dc_curx); 
	}
#else /* HPCFB_MULTI */
	hpcfb_refresh_screen(sc);
#endif /* !HPCFB_MULTI */

	return (0);
}

/*
 * cursor
 */
void
hpcfb_cursor(cookie, on, row, col)
	void *cookie;
	int on, row, col;
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct hpcfb_softc *sc = dc->dc_sc;
	struct rasops_info *ri = &dc->dc_rinfo;
	int curwidth, curheight;
	int xoff, yoff;

	if (on) {
		dc->dc_curx = col;
		dc->dc_cury = row;
	} else {
		dc->dc_curx = -1;
		dc->dc_cury = -1;
	}

#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */

	if (ri->ri_bits == NULL)
		return;

	dc->dc_state |= HPCFB_DC_DRAWING;
	if (sc && sc->sc_accessops->cursor) {
		xoff = col * ri->ri_xscale;
		yoff = row * ri->ri_yscale;
		curheight = ri->ri_font->fontheight;
		curwidth = ri->ri_xscale;
		(*sc->sc_accessops->cursor)(sc->sc_accessctx,
				on, xoff, yoff, curwidth, curheight);
	} else 
		rasops_emul.cursor(ri, on, row, col);
	dc->dc_state &= ~HPCFB_DC_DRAWING;
}

/*
 * mapchar
 */
int
hpcfb_mapchar(cookie, c, cp)
	void *cookie;
	int c;
	unsigned int *cp;
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct rasops_info *ri = &dc->dc_rinfo;

	return rasops_emul.mapchar(ri, c, cp);
}

/*
 * putchar
 */
void
hpcfb_tv_putchar(dc, row, col, uc, attr)
	struct hpcfb_devconfig *dc;
	int row, col;
	u_int uc;
	long attr;
{
	struct hpcfb_vchar *vscn = dc->dc_tvram;
	struct hpcfb_vchar *vc = 
		(vscn + row * dc->dc_cols + col);

#ifdef HPCFB_JUMP
	if (row < dc->dc_min_row)
		dc->dc_min_row = row;
	if (row > dc->dc_max_row)
		dc->dc_max_row = row;

#endif /* HPCFB_JUMP */
	if (vscn == 0)
		return;
	vc->c = uc;
	vc->attr = attr;
}

void
hpcfb_putchar(cookie, row, col, uc, attr)
	void *cookie;
	int row, col;
	u_int uc;
	long attr;
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct hpcfb_softc *sc = dc->dc_sc;
	struct rasops_info *ri = &dc->dc_rinfo;
	int xoff;
	int yoff;
	int fclr, uclr;
	struct wsdisplay_font *font;

	hpcfb_tv_putchar(dc, row, col, uc, attr);
#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */

	if (ri->ri_bits == NULL)
		return;

	dc->dc_state |= HPCFB_DC_DRAWING;
	dc->dc_state &= ~HPCFB_DC_DRAWING;
	if (sc && sc->sc_accessops->putchar 
	       && (dc->dc_state&HPCFB_DC_CURRENT)) {
		yoff = row * ri->ri_yscale;
		xoff =  col * ri->ri_xscale;
		fclr = ri->ri_devcmap[((u_int)attr >> 24) & 15];
		uclr = ri->ri_devcmap[((u_int)attr >> 16) & 15];

		(*sc->sc_accessops->putchar)(sc->sc_accessctx,
				xoff, yoff, font, fclr, uclr, uc, attr);
	} else
		rasops_emul.putchar(ri, row, col, uc, attr);
}

/*
 * copycols
 */
void
hpcfb_tv_copycols(dc, row, srccol, dstcol, ncols)
	struct hpcfb_devconfig *dc;
	int row, srccol, dstcol, ncols;
{
	struct hpcfb_vchar *vscn = dc->dc_tvram;
	struct hpcfb_vchar *svc = 
		(vscn + row * dc->dc_cols + srccol);
	struct hpcfb_vchar *dvc = 
		(vscn + row * dc->dc_cols + dstcol);

#ifdef HPCFB_JUMP
	if (row < dc->dc_min_row)
		dc->dc_min_row = row;
	if (row > dc->dc_max_row)
		dc->dc_max_row = row;
#endif /* HPCFB_JUMP */

	if (vscn == 0)
		return;

	bcopy(svc, dvc, ncols*sizeof(struct hpcfb_vchar));
}

void
hpcfb_copycols(cookie, row, srccol, dstcol, ncols)
	void *cookie;
	int row, srccol, dstcol, ncols;
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct hpcfb_softc *sc = dc->dc_sc;
	struct rasops_info *ri = &dc->dc_rinfo;
	int srcxoff,dstxoff;
	int srcyoff,dstyoff;
	int height, width;

	hpcfb_tv_copycols(dc, row, srccol, dstcol, ncols);
#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */
	if (ri->ri_bits == NULL)
		return;

	dc->dc_state |= HPCFB_DC_DRAWING;
	dc->dc_state &= ~HPCFB_DC_DRAWING;
	if (sc && sc->sc_accessops->bitblit
	       && (dc->dc_state&HPCFB_DC_CURRENT)) {
		srcxoff = srccol * ri->ri_xscale;
		srcyoff = row * ri->ri_yscale;
		dstxoff = dstcol * ri->ri_xscale;
		dstyoff = row * ri->ri_yscale;
		width = ncols * ri->ri_xscale;
		height = ri->ri_yscale;
		(*sc->sc_accessops->bitblit)(sc->sc_accessctx,
			srcxoff, srcyoff, dstxoff, dstyoff, height, width);
	} else
		rasops_emul.copycols(ri, row, srccol, dstcol, ncols);
}


/*
 * erasecols
 */
void
hpcfb_tv_erasecols(dc, row, startcol, ncols, attr)
	struct hpcfb_devconfig *dc;
	int row, startcol, ncols;
	long attr;
{
	int cols = dc->dc_cols;
	struct hpcfb_vchar *vscn = dc->dc_tvram;
	struct hpcfb_vchar *svc = vscn + row * cols + startcol;
	int i;

#ifdef HPCFB_JUMP
	if (row < dc->dc_min_row)
		dc->dc_min_row = row;
	if (row > dc->dc_max_row)
		dc->dc_max_row = row;
#endif /* HPCFB_JUMP */

	if (vscn == 0)
		return;

	for (i = 0; i < ncols; i++) {
		svc->c = ' ';
		svc->attr = attr;
		svc++;
	}
}

void
hpcfb_erasecols(cookie, row, startcol, ncols, attr)
	void *cookie;
	int row, startcol, ncols;
	long attr;
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct hpcfb_softc *sc = dc->dc_sc;
	struct rasops_info *ri = &dc->dc_rinfo;
	int xoff, yoff;
	int width, height;

	hpcfb_tv_erasecols(dc, row, startcol, ncols, attr);
#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */
	if (ri->ri_bits == NULL)
		return;

	dc->dc_state |= HPCFB_DC_DRAWING;
	dc->dc_state &= ~HPCFB_DC_DRAWING;
	if (sc && sc->sc_accessops->erase
	       && (dc->dc_state&HPCFB_DC_CURRENT)) {
		xoff = startcol * ri->ri_xscale;
		yoff = row * ri->ri_yscale;
		width = ncols * ri->ri_xscale;
		height = ri->ri_yscale;
		(*sc->sc_accessops->erase)(sc->sc_accessctx,
			xoff, yoff, height, width, attr);
	} else 
		rasops_emul.erasecols(ri, row, startcol, ncols, attr);
}

/*
 * Copy rows.
 */
void
hpcfb_tv_copyrows(dc, src, dst, num)
	struct hpcfb_devconfig *dc;
	int src, dst, num;
{
	struct hpcfb_vchar *vscn = dc->dc_tvram;
	int cols = dc->dc_cols;
	struct hpcfb_vchar *svc = vscn + src * cols;
	struct hpcfb_vchar *dvc = vscn + dst * cols;

#ifdef HPCFB_JUMP
	if (dst < dc->dc_min_row)
		dc->dc_min_row = dst;
	if (dst + num > dc->dc_max_row)
		dc->dc_max_row = dst + num;
#endif /* HPCFB_JUMP */

	if (vscn == 0)
		return;

	bcopy(svc, dvc, num*cols*sizeof(struct hpcfb_vchar));
}

void
hpcfb_redraw(cookie, row, num)
	void *cookie;
	int row, num;
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct rasops_info *ri = &dc->dc_rinfo;
	int cols = dc->dc_cols;
	struct hpcfb_vchar *vscn = dc->dc_tvram;
	struct hpcfb_vchar *svc;
	int i, j;

#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */
	if (vscn == 0)
		return;

	if (ri->ri_bits == NULL)
		return;

	for (i = 0; i < num; i++) {
		for (j = 0; j < cols; j++) {
			svc = vscn + (row+i) * cols + j;
			rasops_emul.putchar(ri, row + i, j, svc->c, svc->attr);
		}
	}
}

#ifdef HPCFB_JUMP
void
hpcfb_realscroll(dc)
	struct hpcfb_devconfig *dc;
{
	struct rasops_info *ri = &dc->dc_rinfo;
	struct hpcfb_softc *sc = dc->dc_sc;
	int srcyoff, dstyoff;
	int width, height;
	int src, dst, num;


	if (ri->ri_bits == NULL)
		return;

	src = dc->dc_scroll_src;
	dst = dc->dc_scroll_dst;
	num = dc->dc_scroll_num;

	if (sc && sc->sc_accessops->bitblit
	       && (dc->dc_state&HPCFB_DC_CURRENT)) {
		srcyoff = src * ri->ri_yscale;
		dstyoff = dst * ri->ri_yscale;
		width = ri->ri_stride;
		height = num * ri->ri_yscale;
		(*sc->sc_accessops->bitblit)(sc->sc_accessctx,
			0, srcyoff, 0, dstyoff, height, width);
	} else
		hpcfb_redraw(dc, dst, num);
}

void
hpcfb_do_scroll(dc)
	struct hpcfb_devconfig *dc;
{
	struct rasops_info *ri = &dc->dc_rinfo;
	struct hpcfb_softc *sc = dc->dc_sc;
	int srcyoff, dstyoff;
	int width, height;
	int src, dst, num;

	/* callout_stop(dc->dc_scroll_ch); */
	dc->dc_state &= ~HPCFB_DC_SCROLLPENDING;
	dc->dc_state |= HPCFB_DC_DRAWING;
	if ((dc->dc_state&HPCFB_DC_UPDATE)) {
		hpcfb_redraw(cookie, dc->dc_min_row, 
				dc->dc_max_row - dc->dc_min_row);
	} else {
		hpcfb_realscroll(dc);
	}
	dc->dc_state &= ~HPCFB_DC_DRAWING;
}

#endif /* HPCFB_JUMP */

void
hpcfb_copyrows(cookie, src, dst, num)
	void *cookie;
	int src, dst, num;
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct rasops_info *ri = &dc->dc_rinfo;
	struct hpcfb_softc *sc = dc->dc_sc;
	int srcyoff, dstyoff;
	int width, height;

	hpcfb_tv_copyrows(cookie, src, dst, num);

#ifdef HPCFB_JUMP
	if ((dc->dc_state&HPCFB_DC_SCROLLPENDING) == 0) {
		dc->dc_state |= HPCFB_DC_SCROLLPENDING;
		dc->dc_scroll = 1;
		dc->dc_scroll_src = src;
		dc->dc_scroll_dst = dst;
		dc->dc_scroll_num = num;
		callout_reset(dc->sc_scroll_ch, HZ/100, &hpcfb_realscroll, dc);
		return;
	} else if (dc->dc_scroll++ < dc-> dc_rows / HPCFB_MAX_JUMP) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	} else {
		dc->dc_state &= ~HPCFB_DC_SCROLLPENDING;
		callout_stop(dc->sc_scroll_ch);
	}
	if (dc->dc_state&HPCFB_DC_UPDATE) {
		dc->dc_state &= ~HPCFB_DC_UPDATE;
		dc->dc_state |= HPCFB_DC_DRAWING;
		hpcfb_redraw(cookie, dc->dc_min_row, 
				dc->dc_max_row - dc->dc_min_row);
		dc->dc_max_row = 0;
		dc->dc_min_row = dc->dc_rows;
		dc->dc_state &= ~HPCFB_DC_DRAWING;
		if (dc->dc_curx > 0 && dc->dc_cury > 0)
			hpcfb_cursor(dc, 1,  dc->dc_cury, dc->dc_curx); 
		return;
	}
#endif /* HPCFB_JUMP */

	if (ri->ri_bits == NULL)
		return;


	dc->dc_state |= HPCFB_DC_DRAWING;
	if (sc && sc->sc_accessops->bitblit
	       && (dc->dc_state&HPCFB_DC_CURRENT)) {
		srcyoff = src * ri->ri_yscale;
		dstyoff = dst * ri->ri_yscale;
		width = ri->ri_stride;
		height = num * ri->ri_yscale;
		(*sc->sc_accessops->bitblit)(sc->sc_accessctx,
			0, srcyoff, 0, dstyoff, height, width);
	} else
		hpcfb_redraw(cookie, dst, num);
	dc->dc_state &= ~HPCFB_DC_DRAWING;
}

/*
 * eraserows
 */
void
hpcfb_tv_eraserows(dc, row, nrow, attr)
	struct hpcfb_devconfig *dc;
	int row, nrow;
	long attr;
{
	struct hpcfb_vchar *vscn = dc->dc_tvram;
	struct hpcfb_vchar *svc = vscn + row * dc->dc_cols;
	int i;
	int j;

#ifdef HPCFB_JUMP
	if (row < dc->dc_min_row)
		dc->dc_min_row = row;
	if (row + nrow > dc->dc_max_row)
		dc->dc_max_row = row + nrow;
#endif /* HPCFB_JUMP */

	if (vscn == 0)
		return;

	for (i = 0; i < nrow; i++) {
		for (j = 0; j < dc->dc_cols; j++) {
			svc->c = ' ';
			svc->attr = attr;
			svc++;
		}
	}
}

void
hpcfb_eraserows(cookie, row, nrow, attr)
	void *cookie;
	int row, nrow;
	long attr;
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct hpcfb_softc *sc = dc->dc_sc;
	struct rasops_info *ri = &dc->dc_rinfo;
	int yoff;
	int width;
	int height;

	hpcfb_tv_eraserows(dc, row, nrow, attr);
#ifdef HPCFB_JUMP
	if (dc->dc_state&HPCFB_DC_SCROLLPENDING) {
		dc->dc_state |= HPCFB_DC_UPDATE;
		return;
	}
#endif /* HPCFB_JUMP */
	if (ri->ri_bits == NULL)
		return;

	dc->dc_state |= HPCFB_DC_DRAWING;
	if (sc && sc->sc_accessops->erase
	       && (dc->dc_state&HPCFB_DC_CURRENT)) {
		yoff = row * ri->ri_yscale;
		width = ri->ri_stride;
		height = nrow * ri->ri_yscale;
		(*sc->sc_accessops->erase)(sc->sc_accessctx,
			0, yoff, height, width, attr);
	} else 
		rasops_emul.eraserows(ri, row, nrow, attr);
	dc->dc_state &= ~HPCFB_DC_DRAWING;
}

/*
 * alloc_attr
 */
int
hpcfb_alloc_attr(cookie, fg, bg, flags, attrp)
	void *cookie;
	int fg, bg, flags;
	long *attrp;
{
	struct hpcfb_devconfig *dc = (struct hpcfb_devconfig *)cookie;
	struct rasops_info *ri = &dc->dc_rinfo;

	return rasops_emul.alloc_attr(ri, fg, bg, flags, attrp);
}
