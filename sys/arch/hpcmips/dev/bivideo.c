/*	$NetBSD: bivideo.c,v 1.12.2.4 2001/01/05 17:34:17 bouyer Exp $	*/

/*-
 * Copyright (c) 1999
 *         Shin Takemura and PocketBSD Project. All rights reserved.
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
#define FBDEBUG
static const char _copyright[] __attribute__ ((unused)) =
    "Copyright (c) 1999 Shin Takemura.  All rights reserved.";
static const char _rcsid[] __attribute__ ((unused)) =
    "$Id: bivideo.c,v 1.12.2.4 2001/01/05 17:34:17 bouyer Exp $";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/bootinfo.h>
#include <machine/platid.h>
#include <machine/platid_mask.h>
#include <machine/config_hook.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/rasops/rasops.h>

#include <arch/hpcmips/dev/hpcfbvar.h>
#include <arch/hpcmips/dev/hpcfbio.h>
#include <arch/hpcmips/dev/bivideovar.h>
#include <arch/hpcmips/dev/hpccmapvar.h>

#define VPRINTF(arg)	do { if (bootverbose) printf arg; } while(0);

/*
 *  global variables
 */
int bivideo_dont_attach = 0;

/*
 *  function prototypes
 */
int	bivideomatch __P((struct device *, struct cfdata *, void *));
void	bivideoattach __P((struct device *, struct device *, void *));
int	bivideo_ioctl __P((void *, u_long, caddr_t, int, struct proc *));
paddr_t	bivideo_mmap __P((void *, off_t, int));

struct bivideo_softc {
	struct device		sc_dev;
	struct hpcfb_fbconf	sc_fbconf;
	struct hpcfb_dspconf	sc_dspconf;
	void			*sc_powerhook;	/* power management hook */
	int			sc_powerstate;
#define PWRSTAT_SUSPEND		(1<<0)
#define PWRSTAT_LCD		(1<<1)
#define PWRSTAT_BACKLIGHT	(1<<2)
#define PWRSTAT_ALL		(0xffffffff)
};
static int bivideo_init __P((struct hpcfb_fbconf *fb));
static void bivideo_power __P((int, void *));
static void bivideo_update_powerstate __P((struct bivideo_softc *, int));


/*
 *  static variables
 */
struct cfattach bivideo_ca = {
	sizeof(struct bivideo_softc), bivideomatch, bivideoattach,
};
struct hpcfb_accessops bivideo_ha = {
	bivideo_ioctl, bivideo_mmap
};

static int console_flag = 0;
static int attach_flag = 0;

/*
 *  function bodies
 */
int
bivideomatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;
    
	if (bivideo_dont_attach ||
	    strcmp(ma->ma_name, match->cf_driver->cd_name))
		return 0;

	return (1);
}

void
bivideoattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct bivideo_softc *sc = (struct bivideo_softc *)self;
	struct hpcfb_attach_args ha;

	if (attach_flag) {
		panic("%s(%d): bivideo attached twice", __FILE__, __LINE__);
	}
	attach_flag = 1;

	bivideo_init(&sc->sc_fbconf);

	printf(": pseudo video controller");
	if (console_flag) {
		printf(", console");
	}
	printf("\n");

	/* Add a suspend hook to power saving */
	sc->sc_powerstate = 0;
	sc->sc_powerhook = powerhook_establish(bivideo_power, sc);
	if (sc->sc_powerhook == NULL)
		printf("%s: WARNING: unable to establish power hook\n",
			sc->sc_dev.dv_xname);

	ha.ha_console = console_flag;
	ha.ha_accessops = &bivideo_ha;
	ha.ha_accessctx = sc;
	ha.ha_curfbconf = 0;
	ha.ha_nfbconf = 1;
	ha.ha_fbconflist = &sc->sc_fbconf;
	ha.ha_curdspconf = 0;
	ha.ha_ndspconf = 1;
	ha.ha_dspconflist = &sc->sc_dspconf;

	config_found(self, &ha, hpcfbprint);
}

int
bivideo_getcnfb(fb)
	struct hpcfb_fbconf *fb;
{
	console_flag = 1;

	return bivideo_init(fb);
}

static int
bivideo_init(fb)
	struct hpcfb_fbconf *fb;
{
	/*
	 * get fb settings from bootinfo
	 */
	if (bootinfo == NULL ||
	    bootinfo->fb_addr == 0 ||
	    bootinfo->fb_line_bytes == 0 ||
	    bootinfo->fb_width == 0 ||
	    bootinfo->fb_height == 0) {
		printf("no frame buffer infomation.\n");
		return (-1);
	}

	/* zero fill */
	bzero(fb, sizeof(*fb));

	fb->hf_conf_index	= 0;	/* configuration index		*/
	fb->hf_nconfs		= 1;   	/* how many configurations	*/
	strcpy(fb->hf_name, "built-in video");
					/* frame buffer name		*/
	strcpy(fb->hf_conf_name, "default");
					/* configuration name		*/
	fb->hf_height		= bootinfo->fb_height;
	fb->hf_width		= bootinfo->fb_width;
	fb->hf_baseaddr		= (u_long)bootinfo->fb_addr;
	fb->hf_offset		= (u_long)bootinfo->fb_addr -
				      mips_ptob(mips_btop(bootinfo->fb_addr));
					/* frame buffer start offset   	*/
	fb->hf_bytes_per_line	= bootinfo->fb_line_bytes;
	fb->hf_nplanes		= 1;
	fb->hf_bytes_per_plane	= bootinfo->fb_height *
					bootinfo->fb_line_bytes;

	fb->hf_access_flags |= HPCFB_ACCESS_BYTE;
	fb->hf_access_flags |= HPCFB_ACCESS_WORD;
	fb->hf_access_flags |= HPCFB_ACCESS_DWORD;

	switch (bootinfo->fb_type) {
		/*
		 * gray scale
		 */
	case BIFB_D2_M2L_3:
	case BIFB_D2_M2L_3x2:
		fb->hf_access_flags |= HPCFB_ACCESS_REVERSE;
		/* fall through */
	case BIFB_D2_M2L_0:
	case BIFB_D2_M2L_0x2:
		fb->hf_class = HPCFB_CLASS_GRAYSCALE;
		fb->hf_access_flags |= HPCFB_ACCESS_STATIC;
		fb->hf_pack_width = 8;
		fb->hf_pixels_per_pack = 4;
		fb->hf_pixel_width = 2;
		fb->hf_class_data_length = sizeof(struct hf_gray_tag);
		fb->hf_u.hf_gray.hf_flags = 0;	/* reserved for future use */
		break;

		/*
		 * indexed color
		 */
	case BIFB_D8_FF:
		fb->hf_access_flags |= HPCFB_ACCESS_REVERSE;
		/* fall through */
	case BIFB_D8_00:
		fb->hf_class = HPCFB_CLASS_INDEXCOLOR;
		fb->hf_access_flags |= HPCFB_ACCESS_STATIC;
		fb->hf_pack_width = 8;
		fb->hf_pixels_per_pack = 1;
		fb->hf_pixel_width = 8;
		fb->hf_class_data_length = sizeof(struct hf_indexed_tag);
		fb->hf_u.hf_indexed.hf_flags = 0; /* reserved for future use */
		break;

		/*
		 * RGB color
		 */
	case BIFB_D16_FFFF:
		fb->hf_access_flags |= HPCFB_ACCESS_REVERSE;
		/* fall through */
	case BIFB_D16_0000:
		fb->hf_class = HPCFB_CLASS_RGBCOLOR;
		fb->hf_access_flags |= HPCFB_ACCESS_STATIC;
#if BYTE_ORDER == LITTLE_ENDIAN
		fb->hf_swap_flags = HPCFB_SWAP_BYTE;
#endif
		fb->hf_pack_width = 16;
		fb->hf_pixels_per_pack = 1;
		fb->hf_pixel_width = 16;

		fb->hf_class_data_length = sizeof(struct hf_rgb_tag);
		fb->hf_u.hf_rgb.hf_flags = 0;	/* reserved for future use */

		fb->hf_u.hf_rgb.hf_red_width = 5;
		fb->hf_u.hf_rgb.hf_red_shift = 11;
		fb->hf_u.hf_rgb.hf_green_width = 6;
		fb->hf_u.hf_rgb.hf_green_shift = 5;
		fb->hf_u.hf_rgb.hf_blue_width = 5;
		fb->hf_u.hf_rgb.hf_blue_shift = 0;
		fb->hf_u.hf_rgb.hf_alpha_width = 0;
		fb->hf_u.hf_rgb.hf_alpha_shift = 0;
		break;

	default:
		printf("unknown type (=%d).\n", bootinfo->fb_type);
		return (-1);
		break;
	}

	return (0); /* no error */
}

static void 
bivideo_power(why, arg)
	int why;
	void *arg;
{
	struct bivideo_softc *sc = arg;

	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		sc->sc_powerstate |= PWRSTAT_SUSPEND;
		bivideo_update_powerstate(sc, PWRSTAT_ALL);
		break;
	case PWR_RESUME:
		sc->sc_powerstate &= ~PWRSTAT_SUSPEND;
		bivideo_update_powerstate(sc, PWRSTAT_ALL);
		break;
	}
}

static void
bivideo_update_powerstate(sc, updates)
	struct bivideo_softc *sc;
	int updates;
{
	if (updates & PWRSTAT_LCD)
		config_hook_call(CONFIG_HOOK_POWERCONTROL,
		    CONFIG_HOOK_POWERCONTROL_LCD,
		    (void*)!(sc->sc_powerstate & PWRSTAT_SUSPEND));

	if (updates & PWRSTAT_BACKLIGHT)
		config_hook_call(CONFIG_HOOK_POWERCONTROL,
		    CONFIG_HOOK_POWERCONTROL_LCDLIGHT,
		    (void*)(!(sc->sc_powerstate & PWRSTAT_SUSPEND) &&
			     (sc->sc_powerstate & PWRSTAT_BACKLIGHT)));
}

int
bivideo_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct bivideo_softc *sc = (struct bivideo_softc *)v;
	struct hpcfb_fbconf *fbconf;
	struct hpcfb_dspconf *dspconf;
	struct wsdisplay_cmap *cmap;
	struct wsdisplay_param *dispparam;

	switch (cmd) {
	case WSDISPLAYIO_GETCMAP:
		cmap = (struct wsdisplay_cmap*)data;

		if (sc->sc_fbconf.hf_class != HPCFB_CLASS_INDEXCOLOR ||
		    sc->sc_fbconf.hf_pack_width != 8 ||
		    256 <= cmap->index ||
		    256 < (cmap->index + cmap->count))
			return (EINVAL);

		if (!uvm_useracc(cmap->red, cmap->count, B_WRITE) ||
		    !uvm_useracc(cmap->green, cmap->count, B_WRITE) ||
		    !uvm_useracc(cmap->blue, cmap->count, B_WRITE))
			return (EFAULT);

		copyout(&bivideo_cmap_r[cmap->index], cmap->red, cmap->count);
		copyout(&bivideo_cmap_g[cmap->index], cmap->green,cmap->count);
		copyout(&bivideo_cmap_b[cmap->index], cmap->blue, cmap->count);

		return (0);

	case WSDISPLAYIO_PUTCMAP:
		/*
		 * This driver can't set color map.
		 */
		return (EINVAL);

	case WSDISPLAYIO_GETPARAM:
		dispparam = (struct wsdisplay_param*)data;
		switch (dispparam->param) {
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			dispparam->min = 0;
			dispparam->max = 1;
			dispparam->curval =
			    (sc->sc_powerstate & PWRSTAT_BACKLIGHT) ? 1 : 0;
			VPRINTF(("bivideo_ioctl: GETPARAM:BACKLIGHT:%d\n",
				dispparam->curval));
			break;
		case WSDISPLAYIO_PARAM_CONTRAST:
			VPRINTF(("bivideo_ioctl: GETPARAM:CONTRAST\n"));
			return (EINVAL);
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			VPRINTF(("bivideo_ioctl: GETPARAM:BRIGHTNESS\n"));
			return (EINVAL);
		default:
			return (EINVAL);
		}
		return (0);

	case WSDISPLAYIO_SETPARAM:
		dispparam = (struct wsdisplay_param*)data;
		switch (dispparam->param) {
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			if (dispparam->curval < 0 ||
			    1 < dispparam->curval)
				return (EINVAL);
			if (dispparam->curval == 0)
				sc->sc_powerstate &= ~PWRSTAT_BACKLIGHT;
			else
				sc->sc_powerstate |= PWRSTAT_BACKLIGHT;
			bivideo_update_powerstate(sc, PWRSTAT_BACKLIGHT);
			VPRINTF(("bivideo_ioctl: SETPARAM:BACKLIGHT:%d\n",
				(sc->sc_powerstate & PWRSTAT_BACKLIGHT)?1:0));
			break;
		case WSDISPLAYIO_PARAM_CONTRAST:
			VPRINTF(("bivideo_ioctl: SETPARAM:CONTRAST\n"));
			return (EINVAL);
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			VPRINTF(("mq200_ioctl: SETPARAM:BRIGHTNESS\n"));
			return (EINVAL);
		default:
			return (EINVAL);
		}
		return (0);

	case HPCFBIO_GCONF:
		fbconf = (struct hpcfb_fbconf *)data;
		if (fbconf->hf_conf_index != 0 &&
		    fbconf->hf_conf_index != HPCFB_CURRENT_CONFIG) {
			return (EINVAL);
		}
		*fbconf = sc->sc_fbconf;	/* structure assignment */
		return (0);
	case HPCFBIO_SCONF:
		fbconf = (struct hpcfb_fbconf *)data;
		if (fbconf->hf_conf_index != 0 &&
		    fbconf->hf_conf_index != HPCFB_CURRENT_CONFIG) {
			return (EINVAL);
		}
		/*
		 * nothing to do because we have only one configration
		 */
		return (0);
	case HPCFBIO_GDSPCONF:
		dspconf = (struct hpcfb_dspconf *)data;
		if ((dspconf->hd_unit_index != 0 &&
		     dspconf->hd_unit_index != HPCFB_CURRENT_UNIT) ||
		    (dspconf->hd_conf_index != 0 &&
		     dspconf->hd_conf_index != HPCFB_CURRENT_CONFIG)) {
			return (EINVAL);
		}
		*dspconf = sc->sc_dspconf;	/* structure assignment */
		return (0);
	case HPCFBIO_SDSPCONF:
		dspconf = (struct hpcfb_dspconf *)data;
		if ((dspconf->hd_unit_index != 0 &&
		     dspconf->hd_unit_index != HPCFB_CURRENT_UNIT) ||
		    (dspconf->hd_conf_index != 0 &&
		     dspconf->hd_conf_index != HPCFB_CURRENT_CONFIG)) {
			return (EINVAL);
		}
		/*
		 * nothing to do
		 * because we have only one unit and one configration
		 */
		return (0);
	case HPCFBIO_GOP:
	case HPCFBIO_SOP:
		/*
		 * curently not implemented...
		 */
		return (EINVAL);
	}

	return (ENOTTY);
}

paddr_t
bivideo_mmap(ctx, offset, prot)
	void *ctx;
	off_t offset;
	int prot;
{
	struct bivideo_softc *sc = (struct bivideo_softc *)ctx;

	if (offset < 0 ||
	    (sc->sc_fbconf.hf_bytes_per_plane +
		sc->sc_fbconf.hf_offset) <  offset)
		return -1;

	return mips_btop((u_long)bootinfo->fb_addr + offset);
}
