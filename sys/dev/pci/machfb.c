/*	$NetBSD: machfb.c,v 1.23.2.2 2005/06/08 11:45:53 tron Exp $	*/

/*
 * Copyright (c) 2002 Bang Jun-Young
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Some code is derived from ATI Rage Pro and Derivatives Programmer's Guide.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, 
	"$NetBSD: machfb.c,v 1.23.2.2 2005/06/08 11:45:53 tron Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/callout.h>

#ifdef __sparc__
#include <machine/promlib.h>
#endif

#ifdef __powerpc__
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>
#endif

#include <dev/ic/videomode.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>
#include <dev/pci/machfbreg.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#define MACH64_REG_SIZE		1024
#define MACH64_REG_OFF		0x7ffc00

#define	NBARS		3	/* number of Mach64 PCI BARs */

struct vga_bar {
	bus_addr_t vb_base;
	pcireg_t vb_busaddr;
	bus_size_t vb_size;
	pcireg_t vb_type;
	int vb_flags;
};

struct mach64_softc {
	struct device sc_dev;
	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;

	struct vga_bar sc_bars[NBARS];
	struct vga_bar sc_rom;

#define sc_aperbase 	sc_bars[0].vb_base
#define sc_apersize	sc_bars[0].vb_size
#define sc_aperphys 	sc_bars[0].vb_busaddr

#define sc_iobase	sc_bars[1].vb_base
#define sc_iosize	sc_bars[1].vb_size

#define sc_regbase	sc_bars[2].vb_base
#define sc_regsize	sc_bars[2].vb_size
#define sc_regphys	sc_bars[2].vb_busaddr

	bus_space_tag_t sc_regt;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_regh;
	bus_space_handle_t sc_memh;

	size_t memsize;
	int memtype;

	int sc_mode;
	int sc_bg;

	int has_dsp;
	int bits_per_pixel;
	int max_x;
	int max_y;
	int virt_x;
	int virt_y;
	int color_depth;

	int mem_freq;
	int ramdac_freq;
	int ref_freq;

	int ref_div;
	int log2_vclk_post_div;
	int vclk_post_div;
	int vclk_fb_div;
	int mclk_post_div;
	int mclk_fb_div;

	struct mach64screen *wanted;
	struct mach64screen *active;
	void (*switchcb)(void *, int, int);
	void *switchcbarg;
	struct callout switch_callout;
	LIST_HEAD(, mach64screen) screens;
	const struct wsscreen_descr *currenttype;
	u_char sc_cmap_red[256];
	u_char sc_cmap_green[256];
	u_char sc_cmap_blue[256];
	int sc_dacw;
};

struct mach64screen {
	struct rasops_info ri;
	LIST_ENTRY(mach64screen) next;
	struct mach64_softc *sc;
	const struct wsscreen_descr *type;
	int active;
	u_int *chars;
	long *attrs;
	int dispoffset;
	int mindispoffset;
	int maxdispoffset;

	int cursoron;
	int cursorcol;
	int cursorrow;
	int cursordrawn;
};

struct mach64_crtcregs {
	u_int32_t h_total_disp;
	u_int32_t h_sync_strt_wid;
	u_int32_t v_total_disp;
	u_int32_t v_sync_strt_wid;
	u_int32_t gen_cntl;
	u_int32_t clock_cntl;
	u_int32_t color_depth;
	u_int32_t dot_clock;
};

struct {
	u_int16_t chip_id;
	u_int32_t ramdac_freq;
} mach64_info[] = {
	{ PCI_PRODUCT_ATI_MACH64_CT, 135000 },
	{ PCI_PRODUCT_ATI_RAGE_PRO_AGP, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_PRO_AGP1X, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_PRO_PCI_B, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_XL_AGP, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_PRO_PCI_P, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_PRO_PCI_L, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_XL_PCI, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_II, 135000 },
	{ PCI_PRODUCT_ATI_RAGE_IIP, 200000 },
	{ PCI_PRODUCT_ATI_RAGE_IIC_PCI, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_IIC_AGP_B, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_IIC_AGP_P, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_LT_PRO_AGP, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_MOB_M3_PCI, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_MOB_M3_AGP, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_LT, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_LT_PRO_PCI, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_MOBILITY, 230000 },
	{ PCI_PRODUCT_ATI_RAGE_LT_PRO, 230000 },
	{ PCI_PRODUCT_ATI_MACH64_VT, 170000 },
	{ PCI_PRODUCT_ATI_MACH64_VTB, 200000 },
	{ PCI_PRODUCT_ATI_MACH64_VT4, 230000 }
};

static int mach64_chip_id, mach64_chip_rev;
static struct videomode default_mode;
static struct mach64screen mach64_console_screen;

static char *mach64_memtype_names[] = {
	"(N/A)", "DRAM", "EDO DRAM", "EDO DRAM", "SDRAM", "SGRAM", "WRAM",
	"(unknown type)"
};

struct videomode mach64_modes[] = {
	/* 640x400 @ 70 Hz, 31.5 kHz */
	{ 25175, 640, 664, 760, 800, 400, 409, 411, 450, 0 },
	/* 640x480 @ 72 Hz, 36.5 kHz */
	{ 25175, 640, 664, 760, 800, 480, 491, 493, 525, 0 },
	/* 800x600 @ 72 Hz, 48.0 kHz */
	{ 50000, 800, 856, 976, 1040, 600, 637, 643, 666,
	  VID_PHSYNC | VID_PVSYNC },
	/* 1024x768 @ 70 Hz, 56.5 kHz */
	{ 75000, 1024, 1048, 1184, 1328, 768, 771, 777, 806,
	  VID_NHSYNC | VID_NVSYNC },
	/* 1152x864 @ 70 Hz, 62.4 kHz */
	{ 92000, 1152, 1208, 1368, 1474, 864, 865, 875, 895, 0 },
	/* 1280x1024 @ 70 Hz, 74.59 kHz */
	{ 126500, 1280, 1312, 1472, 1696, 1024, 1032, 1040, 1068,
	  VID_NHSYNC | VID_NVSYNC }
};

extern const u_char rasops_cmap[768];

int	mach64_match(struct device *, struct cfdata *, void *);
void	mach64_attach(struct device *, struct device *, void *);

CFATTACH_DECL(machfb, sizeof(struct mach64_softc), mach64_match, mach64_attach,
    NULL, NULL);

void	mach64_init(struct mach64_softc *);
int	mach64_get_memsize(struct mach64_softc *);
int	mach64_get_max_ramdac(struct mach64_softc *);
void	mach64_get_mode(struct mach64_softc *, struct videomode *);
int	mach64_calc_crtcregs(struct mach64_softc *, struct mach64_crtcregs *,
	    struct videomode *);
void	mach64_set_crtcregs(struct mach64_softc *, struct mach64_crtcregs *);
int	mach64_modeswitch(struct mach64_softc *, struct videomode *);
void	mach64_set_dsp(struct mach64_softc *);
void	mach64_set_pll(struct mach64_softc *, int);
void	mach64_reset_engine(struct mach64_softc *);
void	mach64_init_engine(struct mach64_softc *);
void	mach64_adjust_frame(struct mach64_softc *, int, int);
void	mach64_init_lut(struct mach64_softc *);
void	mach64_switch_screen(struct mach64_softc *);
void	mach64_init_screen(struct mach64_softc *, struct mach64screen *,
	    const struct wsscreen_descr *, int, long *, int);
void	mach64_restore_screen(struct mach64screen *,
	    const struct wsscreen_descr *, u_int *);
int 	mach64_set_screentype(struct mach64_softc *,
	    const struct wsscreen_descr *);
int	mach64_is_console(struct pci_attach_args *);

void	mach64_cursor(void *, int, int, int);
int	mach64_mapchar(void *, int, u_int *);
void	mach64_putchar(void *, int, int, u_int, long);
void	mach64_copycols(void *, int, int, int, int);
void	mach64_erasecols(void *, int, int, int, long);
void	mach64_copyrows(void *, int, int, int);
void	mach64_eraserows(void *, int, int, long);
int	mach64_allocattr(void *, int, int, int, long *);
void 	mach64_clearscreen(struct mach64_softc *);

void	mach64_scroll(void *, void *, int);

int	mach64_putcmap(struct mach64_softc *, struct wsdisplay_cmap *);
int	mach64_getcmap(struct mach64_softc *, struct wsdisplay_cmap *);
int	mach64_putpalreg(struct mach64_softc *, uint8_t, uint8_t, uint8_t,
			 uint8_t);
void	mach64_bitblt(struct mach64_softc *, int, int, int, int, int, int, int,
		      int) ;
void	mach64_rectfill(struct mach64_softc *, int, int, int, int, int);
void	mach64_setup_mono(struct mach64_softc *, int, int, int, int, uint32_t,
			  uint32_t);
void	mach64_feed_bytes(struct mach64_softc *, int, uint8_t *);
void	mach64_showpal(struct mach64_softc *);
int	mach64_getwschar(void *, struct wsdisplay_char *);
int	mach64_putwschar(void *, struct wsdisplay_char *);

void	set_address(struct rasops_info *, bus_addr_t);

#if 0
const struct wsdisplay_emulops mach64_emulops = {
	mach64_cursor,
	mach64_mapchar,
	mach64_putchar,
	mach64_copycols,
	mach64_erasecols,
	mach64_copyrows,
	mach64_eraserows,
	mach64_allocattr,
};
#endif

struct wsscreen_descr mach64_defaultscreen = {
	"default",
	0, 0,
	&mach64_console_screen.ri.ri_ops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
	&default_mode
}, mach64_80x25_screen = {
	"80x25", 80, 25,
	&mach64_console_screen.ri.ri_ops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
	&mach64_modes[0]
}, mach64_80x30_screen = {
	"80x30", 80, 30,
	&mach64_console_screen.ri.ri_ops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
	&mach64_modes[1]
}, mach64_80x40_screen = {
	"80x40", 80, 40,
	&mach64_console_screen.ri.ri_ops,
	8, 10,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
	&mach64_modes[0]
}, mach64_80x50_screen = {
	"80x50", 80, 50,
	&mach64_console_screen.ri.ri_ops,
	8, 8,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
	&mach64_modes[0]
}, mach64_100x37_screen = {
	"100x37", 100, 37,
	&mach64_console_screen.ri.ri_ops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
	&mach64_modes[2]
}, mach64_128x48_screen = {
	"128x48", 128, 48,
	&mach64_console_screen.ri.ri_ops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
	&mach64_modes[3]
}, mach64_144x54_screen = {
	"144x54", 144, 54,
	&mach64_console_screen.ri.ri_ops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
	&mach64_modes[4]
}, mach64_160x64_screen = {
	"160x54", 160, 64,
	&mach64_console_screen.ri.ri_ops,
	8, 16,
	WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
	&mach64_modes[5]
};

const struct wsscreen_descr *_mach64_scrlist[] = {
	&mach64_defaultscreen,
	&mach64_80x25_screen,
	&mach64_80x30_screen,
	&mach64_80x40_screen,
	&mach64_80x50_screen,
	&mach64_100x37_screen,
	&mach64_128x48_screen,
	&mach64_144x54_screen,
	&mach64_160x64_screen
};

struct wsscreen_list mach64_screenlist = {
	sizeof(_mach64_scrlist) / sizeof(struct wsscreen_descr *),
	_mach64_scrlist
};

int	mach64_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	mach64_mmap(void *, off_t, int);
int	mach64_alloc_screen(void *, const struct wsscreen_descr *, void **,
		    int *, int *, long *);
void	mach64_free_screen(void *, void *);
int	mach64_show_screen(void *, void *, int, void (*)(void *, int, int),
		    void *);
int	mach64_load_font(void *, void *, struct wsdisplay_font *);

struct wsdisplay_accessops mach64_accessops = {
	mach64_ioctl,
	mach64_mmap,
	mach64_alloc_screen,
	mach64_free_screen,
	mach64_show_screen,
	NULL,			/* load_font */
	NULL,			/* polls */
	mach64_getwschar,	/* getwschar */
	mach64_putwschar,	/* putwschar */
	NULL,			/* scroll */
	NULL,			/* getborder */
	NULL			/* setborder */
};

/*
 * Inline functions for getting access to register aperture.
 */
static inline u_int32_t regr(struct mach64_softc *, u_int32_t);
static inline u_int8_t regrb(struct mach64_softc *, u_int32_t);
static inline void regw(struct mach64_softc *, u_int32_t, u_int32_t);
static inline void regwb(struct mach64_softc *, u_int32_t, u_int8_t);
static inline void regwb_pll(struct mach64_softc *, u_int32_t, u_int8_t);

static inline u_int32_t
regr(struct mach64_softc *sc, u_int32_t index)
{
	return bus_space_read_4(sc->sc_regt, sc->sc_regh, index);
}

static inline u_int8_t
regrb(struct mach64_softc *sc, u_int32_t index)
{
	return bus_space_read_1(sc->sc_regt, sc->sc_regh, index);
}

static inline void
regw(struct mach64_softc *sc, u_int32_t index, u_int32_t data)
{
	bus_space_write_4(sc->sc_regt, sc->sc_regh, index, data);
	bus_space_barrier(sc->sc_regt, sc->sc_regh, index, 4, 
	    BUS_SPACE_BARRIER_WRITE);
}

static inline void
regwb(struct mach64_softc *sc, u_int32_t index, u_int8_t data)
{
	bus_space_write_1(sc->sc_regt, sc->sc_regh, index, data);
	bus_space_barrier(sc->sc_regt, sc->sc_regh, index, 1, 
	    BUS_SPACE_BARRIER_WRITE);
}

static inline void
regwb_pll(struct mach64_softc *sc, u_int32_t index, u_int8_t data)
{
	regwb(sc, CLOCK_CNTL + 1, (index << 2) | PLL_WR_EN);
	regwb(sc, CLOCK_CNTL + 2, data);
	regwb(sc, CLOCK_CNTL + 1, (index << 2) & ~PLL_WR_EN);
}

static inline void
wait_for_fifo(struct mach64_softc *sc, u_int8_t v)
{
	while ((regr(sc, FIFO_STAT) & 0xffff) > (0x8000 >> v))
		;
}

static inline void
wait_for_idle(struct mach64_softc *sc)
{
	wait_for_fifo(sc, 16);
	while ((regr(sc, GUI_STAT) & 1) != 0)
		;
}

int
mach64_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	int i;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY ||
	    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_DISPLAY_VGA)
		return 0;

	for (i = 0; i < sizeof(mach64_info) / sizeof(mach64_info[0]); i++)
		if (PCI_PRODUCT(pa->pa_id) == mach64_info[i].chip_id) {
			mach64_chip_id = PCI_PRODUCT(pa->pa_id);
			mach64_chip_rev = PCI_REVISION(pa->pa_class);
			return 100;
		}

	return 0;
}

void
mach64_attach(struct device *parent, struct device *self, void *aux)
{
	struct mach64_softc *sc = (void *)self;
	struct pci_attach_args *pa = aux;
	char devinfo[256];
	int bar, reg, id;
	struct wsemuldisplaydev_attach_args aa;
	long defattr;
	int setmode, console;
	pcireg_t screg;

	sc->sc_pc = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_dacw=-1;
	sc->sc_mode=WSDISPLAYIO_MODE_EMUL;

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo, sizeof(devinfo));
	printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(pa->pa_class));
	
	/* enable memory and IO access */
	screg=pci_conf_read(sc->sc_pc, sc->sc_pcitag, PCI_COMMAND_STATUS_REG);
	screg|=PCI_FLAGS_IO_ENABLED|PCI_FLAGS_MEM_ENABLED;
	pci_conf_write(sc->sc_pc, sc->sc_pcitag,PCI_COMMAND_STATUS_REG,screg);

	/* enable memory and IO access */
	screg=pci_conf_read(sc->sc_pc, sc->sc_pcitag, PCI_COMMAND_STATUS_REG);
	screg|=PCI_FLAGS_IO_ENABLED|PCI_FLAGS_MEM_ENABLED;
	pci_conf_write(sc->sc_pc, sc->sc_pcitag,PCI_COMMAND_STATUS_REG,screg);

	for (bar = 0; bar < NBARS; bar++) {
		reg = PCI_MAPREG_START + (bar * 4);
		sc->sc_bars[bar].vb_type = pci_mapreg_type(sc->sc_pc,
		    sc->sc_pcitag, reg);
		(void)pci_mapreg_info(sc->sc_pc, sc->sc_pcitag, reg,
		    sc->sc_bars[bar].vb_type, &sc->sc_bars[bar].vb_base,
		    &sc->sc_bars[bar].vb_size, &sc->sc_bars[bar].vb_flags);
		sc->sc_bars[bar].vb_busaddr=pci_conf_read(sc->sc_pc,
		    sc->sc_pcitag, reg)&0xfffffff0;
	}
	sc->sc_memt = pa->pa_memt;

	mach64_init(sc);

	printf("%s: %d MB aperture at 0x%08x, %d KB registers at 0x%08x\n",
	    sc->sc_dev.dv_xname, (u_int)(sc->sc_apersize / (1024 * 1024)),
	    (u_int)sc->sc_aperphys, (u_int)(sc->sc_regsize / 1024),
	    (u_int)sc->sc_regphys);

	if (mach64_chip_id == PCI_PRODUCT_ATI_MACH64_CT ||
	    ((mach64_chip_id == PCI_PRODUCT_ATI_MACH64_VT ||
	    mach64_chip_id == PCI_PRODUCT_ATI_RAGE_II) &&
	    (mach64_chip_rev & 0x07) == 0))
		sc->has_dsp = 0;
	else
		sc->has_dsp = 1;

	sc->memsize = mach64_get_memsize(sc);
	if (sc->memsize == 8192)
		/* The last page is used as register aperture. */
		sc->memsize -= 4;
	sc->memtype = regr(sc, CONFIG_STAT0) & 0x07;

	/* XXX is there any way to calculate reference frequency from
	   known values? */
	if ((mach64_chip_id == PCI_PRODUCT_ATI_RAGE_XL_PCI) ||
	    ((mach64_chip_id>=PCI_PRODUCT_ATI_RAGE_LT_PRO_PCI) &&
	    (mach64_chip_id<=PCI_PRODUCT_ATI_RAGE_LT_PRO))) {
		printf("ref_freq=29.498MHz\n");
		sc->ref_freq = 29498;
	} else
		sc->ref_freq = 14318;

	regwb(sc, CLOCK_CNTL + 1, PLL_REF_DIV << 2);
	sc->ref_div = regrb(sc, CLOCK_CNTL + 2);
	regwb(sc, CLOCK_CNTL + 1, MCLK_FB_DIV << 2);
	sc->mclk_fb_div = regrb(sc, CLOCK_CNTL + 2);
	sc->mem_freq = (2 * sc->ref_freq * sc->mclk_fb_div) /
	    (sc->ref_div * 2);
	sc->mclk_post_div = (sc->mclk_fb_div * 2 * sc->ref_freq) /
	    (sc->mem_freq * sc->ref_div);
	sc->ramdac_freq = mach64_get_max_ramdac(sc);
	printf("%s: %ld KB %s %d.%d MHz, maximum RAMDAC clock %d MHz\n",
	    sc->sc_dev.dv_xname, (u_long)sc->memsize,
	    mach64_memtype_names[sc->memtype],
	    sc->mem_freq / 1000, sc->mem_freq % 1000,
	    sc->ramdac_freq / 1000);

	id = regr(sc, CONFIG_CHIP_ID) & 0xffff;
	if (id != mach64_chip_id) {
		printf("%s: chip ID mismatch, 0x%x != 0x%x\n",
		    sc->sc_dev.dv_xname, id, mach64_chip_id);
		return;
	}

	console = mach64_is_console(pa);

#if defined(__sparc__) || defined(__powerpc__)
	if (console) {
		mach64_get_mode(sc, &default_mode);
		setmode = 0;
	} else {
		memcpy(&default_mode, &mach64_modes[4], sizeof(struct 
		    videomode));
		setmode = 1;
	}
#else
	memcpy(&default_mode, &mach64_modes[0], sizeof(struct videomode));
	setmode = 1;
#endif

	sc->bits_per_pixel = 8;
	sc->virt_x = default_mode.hdisplay;
	sc->virt_y = default_mode.vdisplay;
	sc->max_x = sc->virt_x - 1;
	sc->max_y = (sc->memsize * 1024) /
	    (sc->virt_x * (sc->bits_per_pixel / 8)) - 1;

	sc->color_depth = CRTC_PIX_WIDTH_8BPP;

	mach64_init_engine(sc);
#if 0
	mach64_adjust_frame(0, 0);
	if (sc->bits_per_pixel == 8)
		mach64_init_lut(sc);
#endif

	printf("%s: initial resolution %dx%d at %d bpp\n", sc->sc_dev.dv_xname,
	    default_mode.hdisplay, default_mode.vdisplay,
	    sc->bits_per_pixel);

	mach64_console_screen.ri.ri_hw = &mach64_console_screen;
	mach64_console_screen.ri.ri_depth = sc->bits_per_pixel;
	mach64_console_screen.ri.ri_width = default_mode.hdisplay;
	mach64_console_screen.ri.ri_height = default_mode.vdisplay;
	mach64_console_screen.ri.ri_stride = mach64_console_screen.ri.ri_width;

	mach64_console_screen.ri.ri_bits=(void *)(uintptr_t)sc->sc_aperbase;

	mach64_console_screen.ri.ri_flg = RI_CENTER;
	mach64_console_screen.active=1;
	sc->active=&mach64_console_screen;

	/* XXX width/height are nonsense, we only want to pick a font */
	rasops_init(&mach64_console_screen.ri,
	    mach64_console_screen.ri.ri_height / 16,
	    mach64_console_screen.ri.ri_width / 8);

	/* now get the real values */
	rasops_reconfig(&mach64_console_screen.ri,
	    mach64_console_screen.ri.ri_height /
	    mach64_console_screen.ri.ri_font->fontheight,
	    mach64_console_screen.ri.ri_width /
	    mach64_console_screen.ri.ri_font->fontwidth);

	set_address(&mach64_console_screen.ri,sc->sc_aperbase);

	/* enable acceleration */
	mach64_console_screen.ri.ri_ops.copyrows=mach64_copyrows;
	mach64_console_screen.ri.ri_ops.eraserows=mach64_eraserows;
	mach64_console_screen.ri.ri_ops.copycols=mach64_copycols;
	mach64_console_screen.ri.ri_ops.erasecols=mach64_erasecols;
	mach64_console_screen.ri.ri_ops.putchar=mach64_putchar;
	mach64_console_screen.ri.ri_ops.cursor=mach64_cursor;

	mach64_defaultscreen.nrows = mach64_console_screen.ri.ri_rows;
	mach64_defaultscreen.ncols = mach64_console_screen.ri.ri_cols;

	mach64_allocattr(&mach64_console_screen.ri, WS_DEFAULT_FG, 
	    WS_DEFAULT_BG, 0, &defattr);

	sc->sc_bg=WS_DEFAULT_BG;

	/* really necessary? */
	mach64_defaultscreen.capabilities=mach64_console_screen.ri.ri_caps;
	mach64_defaultscreen.textops=&mach64_console_screen.ri.ri_ops;

	/* Initialize fonts */
	/* XXX shouldn't that happen /before/ we call rasops_init()? */
	wsfont_init();

	if (console) {
		mach64_init_screen(sc, &mach64_console_screen,
		    &mach64_defaultscreen, 1, &defattr, setmode);
		wsdisplay_cnattach(&mach64_defaultscreen, 
		    &mach64_console_screen.ri, 0, 0, defattr);
	}

	mach64_init_lut(sc);
	mach64_clearscreen(sc);

	aa.console = console;
	aa.scrdata = &mach64_screenlist;
	aa.accessops = &mach64_accessops;
	aa.accesscookie = sc;

	config_found(self, &aa, wsemuldisplaydevprint);
}

void
mach64_init_screen(struct mach64_softc *sc, struct mach64screen *scr,
    const struct wsscreen_descr *type, int existing, long *attrp, int setmode)
{
	struct rasops_info *ri=&scr->ri;
	int cnt;
	scr->sc = sc;
	scr->type = type;
	scr->mindispoffset = 0;
	scr->maxdispoffset = sc->memsize * 1024;
	scr->dispoffset = 0;
	scr->cursorcol = 0;
	scr->cursorrow = 0;

	cnt=type->nrows * type->ncols;
	scr->attrs=(long *)malloc(cnt * (sizeof(long) + sizeof(u_int)),
	    M_DEVBUF, M_WAITOK);
	scr->chars=(u_int *)&scr->attrs[cnt];
	
	/* 
	 * we allocate both chars and attributes in one chunk, attributes first 
	 * because they have the (potentially) bigger alignment 
	 */
	ri->ri_depth = sc->bits_per_pixel;
	ri->ri_width = default_mode.hdisplay;
	ri->ri_height = default_mode.vdisplay;
	ri->ri_stride = ri->ri_width;
	ri->ri_flg = RI_CENTER;

	if (existing) {
		scr->active = 1;
		ri->ri_flg|=RI_CLEAR;
		if (setmode && mach64_set_screentype(sc, type)) {
			panic("%s: failed to switch video mode",
			    sc->sc_dev.dv_xname);
		}
	} else {
		scr->active = 0;
	}

	LIST_INSERT_HEAD(&sc->screens, scr, next);
}

void
mach64_init(struct mach64_softc *sc)
{
	u_int32_t *p32, saved_value;
	u_int8_t *p;
	int need_swap;

	if (bus_space_map(sc->sc_memt, sc->sc_aperbase, sc->sc_apersize,
		BUS_SPACE_MAP_LINEAR, &sc->sc_memh)) {
		panic("%s: failed to map aperture", sc->sc_dev.dv_xname);
	}
	sc->sc_aperbase = (vaddr_t)bus_space_vaddr(sc->sc_memt, sc->sc_memh);

	sc->sc_regt = sc->sc_memt;
	bus_space_subregion(sc->sc_regt, sc->sc_memh, MACH64_REG_OFF,
	    sc->sc_regsize, &sc->sc_regh);
	sc->sc_regbase = sc->sc_aperbase + 0x7ffc00;

	/*
	 * Test wether the aperture is byte swapped or not
	 */
	p32 = (u_int32_t*)(u_long)sc->sc_aperbase;
	saved_value = *p32;
	p = (u_int8_t*)(u_long)sc->sc_aperbase;
	*p32 = 0x12345678;
	if (p[0] == 0x12 && p[1] == 0x34 && p[2] == 0x56 && p[3] == 0x78)
		need_swap = 0;
	else
		need_swap = 1;
	if (need_swap) {
		sc->sc_aperbase += 0x800000;
		sc->sc_apersize -= 0x800000;
	}
	*p32 = saved_value;

	LIST_INIT(&sc->screens);
	sc->active = NULL;
	sc->currenttype = &mach64_defaultscreen;
	callout_init(&sc->switch_callout);
}

int
mach64_get_memsize(struct mach64_softc *sc)
{
	int tmp, memsize;
	int mem_tab[] = {
		512, 1024, 2048, 4096, 6144, 8192, 12288, 16384
	};

	tmp = regr(sc, MEM_CNTL);
	printf("memctl: %08x\n",tmp);
	if (sc->has_dsp) {
		tmp &= 0x0000000f;
		if (tmp < 8)
			memsize = (tmp + 1) * 512;
		else if (tmp < 12)
			memsize = (tmp - 3) * 1024;
		else
			memsize = (tmp - 7) * 2048;
	} else {
		memsize = mem_tab[tmp & 0x07];
	}

	return memsize;
}

int
mach64_get_max_ramdac(struct mach64_softc *sc)
{
	int i;

	if ((mach64_chip_id == PCI_PRODUCT_ATI_MACH64_VT ||
	     mach64_chip_id == PCI_PRODUCT_ATI_RAGE_II) &&
	     (mach64_chip_rev & 0x07))
		return 170000;

	for (i = 0; i < sizeof(mach64_info) / sizeof(mach64_info[0]); i++)
		if (mach64_chip_id == mach64_info[i].chip_id)
			return mach64_info[i].ramdac_freq;

	if (sc->bits_per_pixel == 8)
		return 135000;
	else
		return 80000;
}

void
mach64_get_mode(struct mach64_softc *sc, struct videomode *mode)
{
	struct mach64_crtcregs crtc;

	crtc.h_total_disp = regr(sc, CRTC_H_TOTAL_DISP);
	crtc.h_sync_strt_wid = regr(sc, CRTC_H_SYNC_STRT_WID);
	crtc.v_total_disp = regr(sc, CRTC_V_TOTAL_DISP);
	crtc.v_sync_strt_wid = regr(sc, CRTC_V_SYNC_STRT_WID);

	mode->htotal = ((crtc.h_total_disp & 0xffff) + 1) << 3;
	mode->hdisplay = ((crtc.h_total_disp >> 16) + 1) << 3;
	mode->hsync_start = ((crtc.h_sync_strt_wid & 0xffff) + 1) << 3;
	mode->hsync_end = ((crtc.h_sync_strt_wid >> 16) << 3) +
	    mode->hsync_start;
	mode->vtotal = (crtc.v_total_disp & 0xffff) + 1;
	mode->vdisplay = (crtc.v_total_disp >> 16) + 1;
	mode->vsync_start = (crtc.v_sync_strt_wid & 0xffff) + 1;
	mode->vsync_end = (crtc.v_sync_strt_wid >> 16) + mode->vsync_start;

#ifdef DEBUG_MACHFB
	printf("mach64_get_mode: %d %d %d %d %d %d %d %d\n",
	    mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
	    mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal);
#endif
}

int
mach64_calc_crtcregs(struct mach64_softc *sc, struct mach64_crtcregs *crtc,
    struct videomode *mode)
{

	if (mode->dot_clock > sc->ramdac_freq)
		/* Clock too high. */
		return 1;

	crtc->h_total_disp = (((mode->hdisplay >> 3) - 1) << 16) |
	    ((mode->htotal >> 3) - 1);
	crtc->h_sync_strt_wid =
	    (((mode->hsync_end - mode->hsync_start) >> 3) << 16) |
	    ((mode->hsync_start >> 3) - 1);

	crtc->v_total_disp = ((mode->vdisplay - 1) << 16) |
	    (mode->vtotal - 1);
	crtc->v_sync_strt_wid =
	    ((mode->vsync_end - mode->vsync_start) << 16) |
	    (mode->vsync_start - 1);

	if (mode->flags & VID_NVSYNC)
		crtc->v_sync_strt_wid |= CRTC_VSYNC_NEG;

	switch (sc->bits_per_pixel) {
	case 8:
		crtc->color_depth = CRTC_PIX_WIDTH_8BPP;
		break;
	case 16:
		crtc->color_depth = CRTC_PIX_WIDTH_16BPP;
		break;
	case 32:
		crtc->color_depth = CRTC_PIX_WIDTH_32BPP;
		break;
	}

	crtc->gen_cntl = 0;
	if (mode->flags & VID_INTERLACE)
		crtc->gen_cntl |= CRTC_INTERLACE_EN;
	if (mode->flags & VID_CSYNC)
		crtc->gen_cntl |= CRTC_CSYNC_EN;

	crtc->dot_clock = mode->dot_clock;

	return 0;
}

void
mach64_set_crtcregs(struct mach64_softc *sc, struct mach64_crtcregs *crtc)
{

	mach64_set_pll(sc, crtc->dot_clock);

	if (sc->has_dsp)
		mach64_set_dsp(sc);

	regw(sc, CRTC_H_TOTAL_DISP, crtc->h_total_disp);
	regw(sc, CRTC_H_SYNC_STRT_WID, crtc->h_sync_strt_wid);
	regw(sc, CRTC_V_TOTAL_DISP, crtc->v_total_disp);
	regw(sc, CRTC_V_SYNC_STRT_WID, crtc->v_sync_strt_wid);

	regw(sc, CRTC_VLINE_CRNT_VLINE, 0);

	regw(sc, CRTC_OFF_PITCH, (sc->virt_x >> 3) << 22);

	regw(sc, CRTC_GEN_CNTL, crtc->gen_cntl | crtc->color_depth |
	    CRTC_EXT_DISP_EN | CRTC_EXT_EN);
}

int
mach64_modeswitch(struct mach64_softc *sc, struct videomode *mode)
{
	struct mach64_crtcregs crtc;

	if (mach64_calc_crtcregs(sc, &crtc, mode))
		return 1;

	mach64_set_crtcregs(sc, &crtc);
	return 0;
}

void
mach64_reset_engine(struct mach64_softc *sc)
{

	/* Reset engine.*/
	regw(sc, GEN_TEST_CNTL, regr(sc, GEN_TEST_CNTL) & ~GUI_ENGINE_ENABLE);

	/* Enable engine. */
	regw(sc, GEN_TEST_CNTL, regr(sc, GEN_TEST_CNTL) | GUI_ENGINE_ENABLE);

	/* Ensure engine is not locked up by clearing any FIFO or
	   host errors. */
	regw(sc, BUS_CNTL, regr(sc, BUS_CNTL) | BUS_HOST_ERR_ACK |
	    BUS_FIFO_ERR_ACK);
}

void
mach64_init_engine(struct mach64_softc *sc)
{
	u_int32_t pitch_value;

	pitch_value = sc->virt_x;

	if (sc->bits_per_pixel == 24)
		pitch_value *= 3;

	mach64_reset_engine(sc);

	wait_for_fifo(sc, 14);

	regw(sc, CONTEXT_MASK, 0xffffffff);

	regw(sc, DST_OFF_PITCH, (pitch_value / 8) << 22);

	regw(sc, DST_Y_X, 0);
	regw(sc, DST_HEIGHT, 0);
	regw(sc, DST_BRES_ERR, 0);
	regw(sc, DST_BRES_INC, 0);
	regw(sc, DST_BRES_DEC, 0);

	regw(sc, DST_CNTL, DST_LAST_PEL | DST_X_LEFT_TO_RIGHT |
	    DST_Y_TOP_TO_BOTTOM);

	regw(sc, SRC_OFF_PITCH, (pitch_value / 8) << 22);

	regw(sc, SRC_Y_X, 0);
	regw(sc, SRC_HEIGHT1_WIDTH1, 1);
	regw(sc, SRC_Y_X_START, 0);
	regw(sc, SRC_HEIGHT2_WIDTH2, 1);

	regw(sc, SRC_CNTL, SRC_LINE_X_LEFT_TO_RIGHT);

	wait_for_fifo(sc, 13);
	regw(sc, HOST_CNTL, 0);

	regw(sc, PAT_REG0, 0);
	regw(sc, PAT_REG1, 0);
	regw(sc, PAT_CNTL, 0);

	regw(sc, SC_LEFT, 0);
	regw(sc, SC_TOP, 0);
	regw(sc, SC_BOTTOM, default_mode.vdisplay - 1);
	regw(sc, SC_RIGHT, pitch_value - 1);

	regw(sc, DP_BKGD_CLR, 0);
	regw(sc, DP_FRGD_CLR, 0xffffffff);
	regw(sc, DP_WRITE_MASK, 0xffffffff);
	regw(sc, DP_MIX, (MIX_SRC << 16) | MIX_DST);

	regw(sc, DP_SRC, FRGD_SRC_FRGD_CLR);

	wait_for_fifo(sc, 3);
	regw(sc, CLR_CMP_CLR, 0);
	regw(sc, CLR_CMP_MASK, 0xffffffff);
	regw(sc, CLR_CMP_CNTL, 0);

	wait_for_fifo(sc, 2);
	switch (sc->bits_per_pixel) {
	case 8:
		regw(sc, DP_PIX_WIDTH, HOST_8BPP | SRC_8BPP | DST_8BPP);
		regw(sc, DP_CHAIN_MASK, DP_CHAIN_8BPP);
		/* We want 8 bit per channel */
		regw(sc, DAC_CNTL, regr(sc, DAC_CNTL) | DAC_8BIT_EN);
		break;
#if 0
	case 32:
		regw(sc, DP_PIX_WIDTH, HOST_32BPP | SRC_32BPP | DST_32BPP);
		regw(sc, DP_CHAIN_MASK, DP_CHAIN_32BPP);
		regw(sc, DAC_CNTL, regr(sc, DAC_CNTL) | DAC_8BIT_EN);
		break;
#endif
	}

	wait_for_fifo(sc, 5);
	regw(sc, CRTC_INT_CNTL, regr(sc, CRTC_INT_CNTL) & ~0x20);
	regw(sc, GUI_TRAJ_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM);

	wait_for_idle(sc);
}

void
mach64_adjust_frame(struct mach64_softc *sc, int x, int y)
{
	int offset;

	offset = ((x + y * sc->virt_x) * (sc->bits_per_pixel >> 3)) >> 3;

	regw(sc, CRTC_OFF_PITCH, (regr(sc, CRTC_OFF_PITCH) & 0xfff00000) |
	     offset);
}

void
mach64_set_dsp(struct mach64_softc *sc)
{
	u_int32_t fifo_depth, page_size, dsp_precision, dsp_loop_latency;
	u_int32_t dsp_off, dsp_on, dsp_xclks_per_qw;
	u_int32_t xclks_per_qw, y;
	u_int32_t fifo_off, fifo_on;

	printf("initializing the DSP\n");
	if (mach64_chip_id == PCI_PRODUCT_ATI_MACH64_VT ||
	    mach64_chip_id == PCI_PRODUCT_ATI_RAGE_II ||
	    mach64_chip_id == PCI_PRODUCT_ATI_RAGE_IIP ||
	    mach64_chip_id == PCI_PRODUCT_ATI_RAGE_IIC_PCI ||
	    mach64_chip_id == PCI_PRODUCT_ATI_RAGE_IIC_AGP_B ||
	    mach64_chip_id == PCI_PRODUCT_ATI_RAGE_IIC_AGP_P) {
		dsp_loop_latency = 0;
		fifo_depth = 24;
	} else {
		dsp_loop_latency = 2;
		fifo_depth = 32;
	}

	dsp_precision = 0;
	xclks_per_qw = (sc->mclk_fb_div * sc->vclk_post_div * 64 << 11) /
	    (sc->vclk_fb_div * sc->mclk_post_div * sc->bits_per_pixel);
	y = (xclks_per_qw * fifo_depth) >> 11;
	while (y) {
		y >>= 1;
		dsp_precision++;
	}
	dsp_precision -= 5;
	fifo_off = ((xclks_per_qw * (fifo_depth - 1)) >> 5) + (3 << 6);

	switch (sc->memtype) {
	case DRAM:
	case EDO_DRAM:
	case PSEUDO_EDO:
		if (sc->memsize > 1024) {
			page_size = 9;
			dsp_loop_latency += 6;
		} else {
			page_size = 10;
			if (sc->memtype == DRAM)
				dsp_loop_latency += 8;
			else
				dsp_loop_latency += 7;
		}
		break;
	case SDRAM:
	case SGRAM:
		if (sc->memsize > 1024) {
			page_size = 8;
			dsp_loop_latency += 8;
		} else {
			page_size = 10;
			dsp_loop_latency += 9;
		}
		break;
	default:
		page_size = 10;
		dsp_loop_latency += 9;
		break;
	}

	if (xclks_per_qw >= (page_size << 11))
		fifo_on = ((2 * page_size + 1) << 6) + (xclks_per_qw >> 5);
	else
		fifo_on = (3 * page_size + 2) << 6;

	dsp_xclks_per_qw = xclks_per_qw >> dsp_precision;
	dsp_on = fifo_on >> dsp_precision;
	dsp_off = fifo_off >> dsp_precision;

#ifdef DEBUG_MACHFB
	printf("dsp_xclks_per_qw = %d, dsp_on = %d, dsp_off = %d,\n"
	    "dsp_precision = %d, dsp_loop_latency = %d,\n"
	    "mclk_fb_div = %d, vclk_fb_div = %d,\n"
	    "mclk_post_div = %d, vclk_post_div = %d\n",
	    dsp_xclks_per_qw, dsp_on, dsp_off, dsp_precision, dsp_loop_latency,
	    sc->mclk_fb_div, sc->vclk_fb_div,
	    sc->mclk_post_div, sc->vclk_post_div);
#endif

	regw(sc, DSP_ON_OFF, ((dsp_on << 16) & DSP_ON) | (dsp_off & DSP_OFF));
	regw(sc, DSP_CONFIG, ((dsp_precision << 20) & DSP_PRECISION) |
	    ((dsp_loop_latency << 16) & DSP_LOOP_LATENCY) |
	    (dsp_xclks_per_qw & DSP_XCLKS_PER_QW));
}

void
mach64_set_pll(struct mach64_softc *sc, int clock)
{
	int q;

	q = (clock * sc->ref_div * 100) / (2 * sc->ref_freq);
#ifdef DEBUG_MACHFB
	printf("q = %d\n", q);
#endif
	if (q > 25500) {
		printf("Warning: q > 25500\n");
		q = 25500;
		sc->vclk_post_div = 1;
		sc->log2_vclk_post_div = 0;
	} else if (q > 12750) {
		sc->vclk_post_div = 1;
		sc->log2_vclk_post_div = 0;
	} else if (q > 6350) {
		sc->vclk_post_div = 2;
		sc->log2_vclk_post_div = 1;
	} else if (q > 3150) {
		sc->vclk_post_div = 4;
		sc->log2_vclk_post_div = 2;
	} else if (q >= 1600) {
		sc->vclk_post_div = 8;
		sc->log2_vclk_post_div = 3;
	} else {
		printf("Warning: q < 1600\n");
		sc->vclk_post_div = 8;
		sc->log2_vclk_post_div = 3;
	}
	sc->vclk_fb_div = q * sc->vclk_post_div / 100;

	regwb_pll(sc, MCLK_FB_DIV, sc->mclk_fb_div);
	regwb_pll(sc, VCLK_POST_DIV, sc->log2_vclk_post_div);
	regwb_pll(sc, VCLK0_FB_DIV, sc->vclk_fb_div);
}

void
mach64_init_lut(struct mach64_softc *sc)
{
	int i,idx;
	idx=0;
	for(i=0;i<256;i++) {
		mach64_putpalreg(sc, i, rasops_cmap[idx], rasops_cmap[idx+1], 
		    rasops_cmap[idx+2]);
		idx+=3;
	}
}

int
mach64_putpalreg(struct mach64_softc *sc, uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
	sc->sc_cmap_red[index]=r;
	sc->sc_cmap_green[index]=g;
	sc->sc_cmap_blue[index]=b;
	/* 
	 * writing the dac index takes a while, in theory we can poll some
	 * register to see when it's ready - but we better avoid writing it
	 * unnecessarily 
	 */
	if(index!=sc->sc_dacw)
	{
		regwb(sc, DAC_MASK, 0xff);
		regwb(sc, DAC_WINDEX, index);
	}
	sc->sc_dacw=index+1;
	regwb(sc, DAC_DATA, r);
	regwb(sc, DAC_DATA, g);
	regwb(sc, DAC_DATA, b);
	return 0;
}

int
mach64_putcmap(struct mach64_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int i, error;
	u_char rbuf[256], gbuf[256], bbuf[256];
	u_char *r, *g, *b;

	printf("putcmap: %d %d\n",index, count);
	if (cm->index >= 256 || cm->count > 256 ||
	    (cm->index + cm->count) > 256)
		return EINVAL;
	error = copyin(cm->red, &rbuf[index], count);
	if (error)
		return error;
	error = copyin(cm->green, &gbuf[index], count);
	if (error)
		return error;
	error = copyin(cm->blue, &bbuf[index], count);
	if (error)
		return error;

	memcpy(&sc->sc_cmap_red[index], &rbuf[index], count);
	memcpy(&sc->sc_cmap_green[index], &gbuf[index], count);
	memcpy(&sc->sc_cmap_blue[index], &bbuf[index], count);

	r = &sc->sc_cmap_red[index];
	g = &sc->sc_cmap_green[index];
	b = &sc->sc_cmap_blue[index];

	for (i = 0; i < count; i++) {
		mach64_putpalreg(sc,index,*r, *g, *b);
		index++;
		r++, g++, b++;
	}
	return 0;
}

int
mach64_getcmap(struct mach64_softc *sc, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error;

	if (index >= 255 || count > 256 || index + count > 256)
		return EINVAL;

	error = copyout(&sc->sc_cmap_red[index],   cm->red,   count);
	if (error)
		return error;
	error = copyout(&sc->sc_cmap_green[index], cm->green, count);
	if (error)
		return error;
	error = copyout(&sc->sc_cmap_blue[index],  cm->blue,  count);
	if (error)
		return error;

	return 0;
}

int
mach64_set_screentype(struct mach64_softc *sc, const struct wsscreen_descr *des)
{
	struct mach64_crtcregs regs;

	if (mach64_calc_crtcregs(sc, &regs,
	    (struct videomode *)des->modecookie))
		return 1;

	mach64_set_crtcregs(sc, &regs);
	return 0;
}

int
mach64_is_console(struct pci_attach_args *pa)
{
#ifdef __sparc__
	int node;

	node = PCITAG_NODE(pa->pa_tag);
	if (node == -1)
		return 0;

	return (node == prom_instance_to_package(prom_stdout()));
#elif defined(__powerpc__)
	/* check if we're the /chosen console device */
	int chosen, stdout, node, us;
	us=pcidev_to_ofdev(pa->pa_pc, pa->pa_tag);
	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "stdout", &stdout, 4);
	node = OF_instance_to_package(stdout);
	return(us == node);
#else
	return 1;
#endif
}

/*
 * wsdisplay_emulops
 */

void
mach64_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri=cookie;
	struct mach64screen *scr=ri->ri_hw;
	struct mach64_softc *sc=scr->sc;
	int x,y,wi=ri->ri_font->fontwidth,he=ri->ri_font->fontheight;
	if(scr->active) {
		x=scr->cursorcol*wi+ri->ri_xorigin;
		y=scr->cursorrow*he+ri->ri_yorigin;
		if(scr->cursordrawn) {
			mach64_bitblt(sc,x,y,x,y,wi,he,MIX_NOT_SRC,0xff);
			scr->cursordrawn=0;
		}
		scr->cursorrow=row;
		scr->cursorcol=col;
		if((scr->cursoron=on)!=0)
		{
			x=scr->cursorcol*wi+ri->ri_xorigin;
			y=scr->cursorrow*he+ri->ri_yorigin;
			mach64_bitblt(sc,x,y,x,y,wi,he,MIX_NOT_SRC,0xff);
			scr->cursordrawn=1;
		}
	} else {
		scr->cursoron=on;
		scr->cursorrow=row;
		scr->cursorcol=col;
		scr->cursordrawn=0;
	}
}

#if 0
int
mach64_mapchar(void *cookie, int uni, u_int *index)
{

	return 0;
}
#endif

void
mach64_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri=cookie;
	struct mach64screen *scr=ri->ri_hw;
	struct mach64_softc *sc=scr->sc;
	int offset=ri->ri_cols*row+col;
	scr->attrs[offset]=attr;
	scr->chars[offset]=c;
	if((scr->active) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		int fg,bg,uc;
		uint8_t *data;
		int x,y,wi=ri->ri_font->fontwidth,he=ri->ri_font->fontheight;

		/*scr->putchar(cookie,row,col,c,attr);*/
		if (!CHAR_IN_FONT(c, ri->ri_font))
			return;
		bg = (u_char)ri->ri_devcmap[(attr >> 16) & 0xf];
		fg = (u_char)ri->ri_devcmap[(attr >> 24) & 0xf];
		x=ri->ri_xorigin+col*wi;
		y=ri->ri_yorigin+row*he;
		if(c == 0x20) {
			mach64_rectfill(sc,x,y,wi,he,bg);
		} else {
			uc = c-ri->ri_font->firstchar;
			data = (uint8_t *)ri->ri_font->data + uc * 
			    ri->ri_fontscale;

			mach64_setup_mono(sc,x,y,wi,he,fg,bg);
			mach64_feed_bytes(sc,ri->ri_fontscale,data);
		}
	}
}


void
mach64_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri=cookie;
	struct mach64screen *scr=ri->ri_hw;
	struct mach64_softc *sc=scr->sc;
	int32_t xs,xd,y,width,height;

	int from=srccol+row*ri->ri_cols;
	int to=dstcol+row*ri->ri_cols;
	memmove(&scr->attrs[to],&scr->attrs[from],ncols*sizeof(long));
	memmove(&scr->chars[to],&scr->chars[from],ncols*sizeof(u_int));

	if((scr->active) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		xs=ri->ri_xorigin+ri->ri_font->fontwidth*srccol;
		xd=ri->ri_xorigin+ri->ri_font->fontwidth*dstcol;
		y=ri->ri_yorigin+ri->ri_font->fontheight*row;
		width=ri->ri_font->fontwidth*ncols;
		height=ri->ri_font->fontheight;
		mach64_bitblt(sc,xs,y,xd,y,width,height,MIX_SRC,0xff);
	}
}

void
mach64_erasecols(void *cookie, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri=cookie;
	struct mach64screen *scr=ri->ri_hw;
	struct mach64_softc *sc=scr->sc;
	int32_t x,y,width,height,fg,bg,ul;;

	int start=startcol+row*ri->ri_cols;
	int end=start+ncols, i;
	for(i=start;i<end;i++) {
		scr->attrs[i]=fillattr;
		scr->chars[i]=0x20;
	}
	if((scr->active) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x=ri->ri_xorigin+ri->ri_font->fontwidth*startcol;
		y=ri->ri_yorigin+ri->ri_font->fontheight*row;
		width=ri->ri_font->fontwidth*ncols;
		height=ri->ri_font->fontheight;
		rasops_unpack_attr(fillattr,&fg,&bg,&ul);

		mach64_rectfill(sc,x,y,width,height,bg);
	}
}

void
mach64_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri=cookie;
	struct mach64screen *scr=ri->ri_hw;
	struct mach64_softc *sc=scr->sc;
	int32_t x, ys, yd, width, height;

	int from, to, len;
	from=ri->ri_cols*srcrow;
	to=ri->ri_cols*dstrow;
	len=ri->ri_cols*nrows;
	
	memmove(&scr->attrs[to],&scr->attrs[from],len*sizeof(long));
	memmove(&scr->chars[to],&scr->chars[from],len*sizeof(u_int));
	
	if((scr->active) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x=ri->ri_xorigin;
		ys=ri->ri_yorigin+ri->ri_font->fontheight*srcrow;
		yd=ri->ri_yorigin+ri->ri_font->fontheight*dstrow;
		width=ri->ri_emuwidth;
		height=ri->ri_font->fontheight*nrows;
		mach64_bitblt(sc,x,ys,x,yd,width,height,MIX_SRC,0xff);
	}
}

void
mach64_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri=cookie;
	struct mach64screen *scr=ri->ri_hw;
	struct mach64_softc *sc=scr->sc;
	int32_t x,y,width,height,fg,bg,ul;

	int start=ri->ri_cols*row, end=ri->ri_cols*(row+nrows),i;
	for(i=start;i<end;i++) {
		scr->attrs[i]=fillattr;
		scr->chars[i]=0x20;
	}

	if((scr->active) && (sc->sc_mode == WSDISPLAYIO_MODE_EMUL)) {
		x=ri->ri_xorigin;
		y=ri->ri_yorigin+ri->ri_font->fontheight*row;
		width=ri->ri_emuwidth;
		height=ri->ri_font->fontheight*nrows;
		rasops_unpack_attr(fillattr,&fg,&bg,&ul);

		mach64_rectfill(sc,x,y,width,height,bg);
	}
}

void
mach64_bitblt(struct mach64_softc *sc, int xs, int ys, int xd, int yd, int width, int height, int rop, int mask)
{
	uint32_t dest_ctl=0;
	wait_for_idle(sc);
	regw(sc,DP_WRITE_MASK,mask);	/* XXX only good for 8 bit */
	regw(sc,DP_PIX_WIDTH,DST_8BPP|SRC_8BPP|HOST_8BPP);
	regw(sc,DP_SRC,FRGD_SRC_BLIT);
	regw(sc,DP_MIX,(rop&0xffff)<<16);
	regw(sc,CLR_CMP_CNTL,0);	/* no transparency */
	if(yd<ys) {
		dest_ctl=DST_Y_TOP_TO_BOTTOM;
	} else {
		ys+=height-1;
		yd+=height-1;
		dest_ctl=DST_Y_BOTTOM_TO_TOP;
	}
	if(xd<xs) {
		dest_ctl|=DST_X_LEFT_TO_RIGHT;
		regw(sc,SRC_CNTL,SRC_LINE_X_LEFT_TO_RIGHT);
	} else {
		dest_ctl|=DST_X_RIGHT_TO_LEFT;
		xs+=width-1;
		xd+=width-1;
		regw(sc,SRC_CNTL,SRC_LINE_X_RIGHT_TO_LEFT);
	}
	regw(sc,DST_CNTL,dest_ctl);

	regw(sc,SRC_Y_X,(xs<<16)|ys);
	regw(sc,SRC_WIDTH1,width);
	regw(sc,DST_Y_X,(xd<<16)|yd);
	regw(sc,DST_HEIGHT_WIDTH,(width<<16)|height);
}

void
mach64_setup_mono(struct mach64_softc *sc, int xd, int yd, int width, 
     int height, uint32_t fg, uint32_t bg)
{
	wait_for_idle(sc);
	regw(sc,DP_WRITE_MASK,0xff);	/* XXX only good for 8 bit */
	regw(sc,DP_PIX_WIDTH,DST_8BPP|SRC_1BPP|HOST_1BPP);
	regw(sc,DP_SRC,MONO_SRC_HOST|BKGD_SRC_BKGD_CLR|FRGD_SRC_FRGD_CLR);
	regw(sc,DP_MIX,((MIX_SRC&0xffff)<<16)|MIX_SRC);
	regw(sc,CLR_CMP_CNTL,0);	/* no transparency */
	regw(sc,SRC_CNTL,SRC_LINE_X_LEFT_TO_RIGHT);
	regw(sc,DST_CNTL,DST_Y_TOP_TO_BOTTOM|DST_X_LEFT_TO_RIGHT);
	regw(sc,HOST_CNTL,HOST_BYTE_ALIGN);
	regw(sc,DP_BKGD_CLR,bg);
	regw(sc,DP_FRGD_CLR,fg);
	regw(sc,SRC_Y_X,0);
	regw(sc,SRC_WIDTH1,width);
	regw(sc,DST_Y_X,(xd<<16)|yd);
	regw(sc,DST_HEIGHT_WIDTH,(width<<16)|height);
	/* now feed the data into the chip */
}

void
mach64_feed_bytes(struct mach64_softc *sc, int count, uint8_t *data)
{
	int i;
	uint32_t latch=0, bork;
	int shift=0;
	int reg=0;
	for(i=0;i<count;i++) {
		bork=data[i];
		latch|=(bork<<shift);
		if(shift == 24) {
			regw(sc,HOST_DATA0+reg,latch);
			latch=0;
			shift=0;
			reg=(reg+4)&0x3c;
		} else
			shift+=8;
	}
	if(shift!=0)	/* 24 */
		regw(sc,HOST_DATA0+reg,latch);
}


void
mach64_rectfill(struct mach64_softc *sc, int x, int y, int width, int height, 
    int colour)
{
	wait_for_idle(sc);
	regw(sc,DP_WRITE_MASK,0xff);
	regw(sc,DP_FRGD_CLR,colour);
	regw(sc,DP_PIX_WIDTH,DST_8BPP|SRC_8BPP|HOST_8BPP);
	regw(sc,DP_SRC,FRGD_SRC_FRGD_CLR);
	regw(sc,DP_MIX,(MIX_SRC)<<16);
	regw(sc,CLR_CMP_CNTL,0);	/* no transparency */
	regw(sc,SRC_CNTL,SRC_LINE_X_LEFT_TO_RIGHT);
	regw(sc,DST_CNTL,DST_X_LEFT_TO_RIGHT|DST_Y_TOP_TO_BOTTOM);

	regw(sc,SRC_Y_X,(x<<16)|y);
	regw(sc,SRC_WIDTH1,width);
	regw(sc,DST_Y_X,(x<<16)|y);
	regw(sc,DST_HEIGHT_WIDTH,(width<<16)|height);
}

void
mach64_clearscreen(struct mach64_softc *sc)
{
	mach64_rectfill(sc,0,0,sc->virt_x,sc->virt_y,sc->sc_bg);
}


void
mach64_showpal(struct mach64_softc *sc)
{
	int i,x=0;
	for (i=0;i<16;i++) {
		mach64_rectfill(sc,x,0,64,64,i);
		x+=64;
	}
}

int
mach64_allocattr(void *cookie, int fg, int bg, int flags, long *attrp)
{
	if((fg == 0)&&(bg == 0))
	{
		fg=WS_DEFAULT_FG;
		bg=WS_DEFAULT_BG;
	}
	*attrp=(fg&0xf)<<24|(bg&0xf)<<16|(flags&0xff)<<8;
	return 0;
}

/*
 * wsdisplay_accessops
 */

int
mach64_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct mach64_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;
	struct mach64screen *ms=sc->active;
	switch (cmd) {
		case WSDISPLAYIO_GTYPE:
			/* XXX is this the right type to return? */
			*(u_int *)data = WSDISPLAY_TYPE_PCIMISC;	
			return 0;

		case WSDISPLAYIO_GINFO:
			wdf = (void *)data;
			wdf->height = ms->ri.ri_height;
			wdf->width = ms->ri.ri_width;
			wdf->depth = ms->ri.ri_depth;
			wdf->cmsize = 256;
			return 0;
		case WSDISPLAYIO_GETCMAP:
			return mach64_getcmap(sc, 
			    (struct wsdisplay_cmap *)data);

		case WSDISPLAYIO_PUTCMAP:
			return mach64_putcmap(sc, 
			    (struct wsdisplay_cmap *)data);
		/* PCI config read/write passthrough. */
		case PCI_IOC_CFGREAD:
		case PCI_IOC_CFGWRITE:
			return (pci_devioctl(sc->sc_pc, sc->sc_pcitag,
			    cmd, data, flag, p));
		case WSDISPLAYIO_SMODE:
			{
				int new_mode=*(int*)data;
				if(new_mode!=sc->sc_mode)
				{
					sc->sc_mode=new_mode;
					if(new_mode == WSDISPLAYIO_MODE_EMUL)
					{
						/*
						 * reset a few things the 
						 * Xserver might have screwed up
						 */
						mach64_restore_screen(ms,
						    ms->type, ms->chars);
						mach64_cursor(ms, ms->cursoron, 
						    ms->cursorrow,
						    ms->cursorcol);
					}
				}
			}
			return 0;
		case WSDISPLAYIO_GETWSCHAR:
			return mach64_getwschar(sc,
			    (struct wsdisplay_char *)data);
		case WSDISPLAYIO_PUTWSCHAR:
			return mach64_putwschar(sc,
			    (struct wsdisplay_char *)data);
	}
	return EPASSTHROUGH;
}

paddr_t
mach64_mmap(void *v, off_t offset, int prot)
{
	struct mach64_softc *sc = v;
	paddr_t pa;
	/* 'regular' framebuffer mmap()ing */
	if(offset<sc->sc_apersize) {
		pa = bus_space_mmap(sc->sc_memt, sc->sc_aperbase+offset, 0, 
		    prot, BUS_SPACE_MAP_LINEAR);
		return pa;
	}
#if 0
	/* evil hack to allow mmap()ing other devices as well */
	if((offset>0x80000000) && (offset<=0xffffffff)) {
		pa = bus_space_mmap(sc->sc_memt, offset, 0, prot, 
		    BUS_SPACE_MAP_LINEAR);
		return pa;
	}
#endif

	if((offset>=sc->sc_aperphys) && 
	    (offset<(sc->sc_aperphys+sc->sc_apersize))) {
		pa = bus_space_mmap(sc->sc_memt, offset, 0, prot, 
		    BUS_SPACE_MAP_LINEAR);
		return pa;
	}

	if((offset>=sc->sc_regphys) && 
	    (offset<(sc->sc_regphys+sc->sc_regsize))) {
		pa = bus_space_mmap(sc->sc_memt, offset, 0, prot, 
		    BUS_SPACE_MAP_LINEAR);
		return pa;
	}

	return -1;
}

int
mach64_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	struct mach64_softc *sc = v;
	struct mach64screen *scr;
	struct rasops_info *ri;
	int cnt=type->nrows * type->ncols;

	scr = malloc(sizeof(struct mach64screen), M_DEVBUF, M_WAITOK|M_ZERO);
	mach64_init_screen(sc, scr, type, 0, defattrp, sc->active == NULL);
	ri=&scr->ri;

	ri->ri_hw=scr;
	/*ri->ri_bits=(void *)sc->sc_aperbase;*/
	rasops_init(ri, mach64_console_screen.ri.ri_height / 8,
	    mach64_console_screen.ri.ri_width / 8);

	rasops_reconfig(ri, ri->ri_height / ri->ri_font->fontheight,
	    ri->ri_width / ri->ri_font->fontwidth);
	set_address(ri,sc->sc_aperbase);
	mach64_allocattr(ri,WS_DEFAULT_FG,WS_DEFAULT_BG,0,defattrp);

	scr->ri.ri_ops.copyrows=mach64_copyrows;
	scr->ri.ri_ops.eraserows=mach64_eraserows;
	scr->ri.ri_ops.copycols=mach64_copycols;
	scr->ri.ri_ops.erasecols=mach64_erasecols;
	scr->ri.ri_ops.putchar=mach64_putchar;
	scr->ri.ri_ops.cursor=mach64_cursor;

	scr->attrs=(long *)malloc((cnt)*(sizeof(long)+sizeof(u_int)),
	    M_DEVBUF, M_WAITOK);
	scr->chars=(u_int *)&scr->attrs[cnt];
	mach64_eraserows(ri, 0, ri->ri_rows, *defattrp);

	if (sc->active == NULL) {
		scr->active = 1;
		sc->active = scr;
		sc->currenttype = type;
	}

	*cookiep = scr;
	*curxp = scr->cursorcol;
	*curyp = scr->cursorrow;

	return 0;
}

void
mach64_free_screen(void *v, void *cookie)
{
	struct mach64_softc *sc = v;
	struct mach64screen *scr = cookie;

	LIST_REMOVE(scr, next);
	if (scr != &mach64_console_screen) {
		free(scr->attrs,M_DEVBUF);
		free(scr, M_DEVBUF);
	} else
		panic("mach64_free_screen: console");

	if (sc->active == scr)
		sc->active = 0;
}

int
mach64_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct mach64_softc *sc = v;
	struct mach64screen *scr, *oldscr;

	scr = cookie;
	oldscr = sc->active;
	if (scr == oldscr)
		return 0;

	sc->wanted = scr;
	sc->switchcb = cb;
	sc->switchcbarg = cbarg;
	if (cb) {
		callout_reset(&sc->switch_callout, 0,
		    (void(*)(void *))mach64_switch_screen, sc);
		return EAGAIN;
	}

	mach64_switch_screen(sc);

	return 0;
}

void
mach64_switch_screen(struct mach64_softc *sc)
{
	struct mach64screen *scr, *oldscr;
	const struct wsscreen_descr *type;

	scr = sc->wanted;
	if (!scr) {
		printf("mach64_switch_screen: disappeared\n");
		(*sc->switchcb)(sc->switchcbarg, EIO, 0);
		return;
	}
	type = scr->type;
	oldscr = sc->active; /* can be NULL! */
#ifdef DIAGNOSTIC
	if (oldscr) {
		if (!oldscr->active)
			panic("mach64_switch_screen: not active");
		if (oldscr->type != sc->currenttype)
			panic("mach64_switch_screen: bad type");
	}
#endif
	if (scr == oldscr)
		return;

#ifdef DIAGNOSTIC
/* XXX: this one bites us at reboot */
/*	if (scr->active)
		panic("mach64_switch_screen: active");*/
#endif

	if (oldscr)
		oldscr->active = 0;

	if (sc->currenttype != type) {
		mach64_set_screentype(sc, type);
		sc->currenttype = type;
	}

	scr->dispoffset = scr->mindispoffset;

	if (!oldscr || (scr->dispoffset != oldscr->dispoffset)) {

	}

	/* Clear the entire screen. */

	scr->active = 1;
	mach64_restore_screen(scr, type, scr->chars);

	sc->active = scr;

	scr->ri.ri_ops.cursor(scr, scr->cursoron, scr->cursorrow, 
	    scr->cursorcol);

	sc->wanted = 0;
	if (sc->switchcb)
		(*sc->switchcb)(sc->switchcbarg, 0, 0);
}

void
mach64_restore_screen(struct mach64screen *scr,
    const struct wsscreen_descr *type, u_int *mem)
{
	int i, j, offset=0;
	/*struct rasops_info *ri=&scr->ri;*/
	u_int *charptr=scr->chars;
	long *attrptr=scr->attrs;
	mach64_clearscreen(scr->sc);
	for (i = 0; i < scr->ri.ri_rows; i++) {
		for (j = 0; j < scr->ri.ri_cols; j++) {
			mach64_putchar(scr, i, j, charptr[offset], 
			    attrptr[offset]);
			offset++;
		}
	}
	scr->cursordrawn=0;
}

/* set ri->ri_bits according to fb, ri_xorigin and ri_yorigin */
void
set_address(struct rasops_info *ri, bus_addr_t fb)
{
	/*printf(" %d %d %d\n",ri->ri_xorigin,ri->ri_yorigin,ri->ri_stride);*/
	ri->ri_bits = (void *)((u_long)fb + ri->ri_stride * ri->ri_yorigin + 
	    ri->ri_xorigin);
}

int
mach64_getwschar(void *cookie, struct wsdisplay_char *wsc)
{
	struct mach64_softc *sc=cookie;
	struct mach64screen *scr=sc->active;
	int fg,bg,fl;
	if(scr){
		if((wsc->col>=0) && (wsc->col<scr->ri.ri_cols) && 
		    (wsc->row>=0) && (wsc->row<scr->ri.ri_rows)) {
			int pos = scr->ri.ri_cols * wsc->row + wsc->col;
			wsc->letter=scr->chars[pos];
			rasops_unpack_attr(scr->attrs[pos], &fg, &bg, &fl);
			wsc->foreground=fg;
			wsc->background=bg;
			wsc->flags=fl;
			return 0;
		}
	}
	return EINVAL;
}

int
mach64_putwschar(void *cookie, struct wsdisplay_char *wsc)
{
	struct mach64_softc *sc=cookie;
	struct mach64screen *scr=sc->active;
	long attr;
	if(scr){
		if((wsc->col>=0) && (wsc->col<scr->ri.ri_cols) &&
		    (wsc->row>=0) && (wsc->row<scr->ri.ri_rows)) {
			mach64_allocattr(&scr->ri,wsc->foreground, 
			    wsc->background, wsc->flags,&attr);
			mach64_putchar(&scr->ri,wsc->row, wsc->col, 
			    wsc->letter,attr);
			return 0;
		}
	}
	return EINVAL;
}

#if 0
int
mach64_load_font(void *v, void *cookie, struct wsdisplay_font *data)
{

	return 0;
}
#endif

