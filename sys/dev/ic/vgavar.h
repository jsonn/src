/* $NetBSD: vgavar.h,v 1.21.10.1 2005/04/29 11:28:53 kent Exp $ */

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

#include <sys/callout.h>

#include "opt_vga.h"

struct vga_handle {
	struct pcdisplay_handle vh_ph;
	bus_space_handle_t vh_ioh_vga, vh_allmemh;
	int vh_mono;
};
#define vh_iot 		vh_ph.ph_iot
#define vh_memt 	vh_ph.ph_memt
#define vh_ioh_6845 	vh_ph.ph_ioh_6845
#define vh_memh 	vh_ph.ph_memh

struct vga_funcs {
	int (*vf_ioctl)(void *, u_long, caddr_t, int, struct proc *);
	paddr_t (*vf_mmap)(void *, off_t, int);
};

struct vga_config {
	struct vga_handle hdl;
	struct vga_softc *softc;

	int nscreens;
	LIST_HEAD(, vgascreen) screens;
	struct vgascreen *active; /* current display */
	const struct wsscreen_descr *currenttype;

	int vc_biosmapped;
	bus_space_tag_t vc_biostag;
	bus_space_handle_t vc_bioshdl;

	struct vgascreen *wantedscreen;
	void (*switchcb)(void *, int, int);
	void *switchcbarg;

	struct callout vc_switch_callout;
	int vc_quirks;
	int vc_type;
	const struct vga_funcs *vc_funcs;

#ifndef VGA_RASTERCONSOLE
	int currentfontset1, currentfontset2;
	int vc_nfontslots;
	struct egavga_font *vc_fonts[8]; /* currently loaded */
	TAILQ_HEAD(, egavga_font) vc_fontlist; /* LRU queue */
#else
	int nfonts;
	LIST_HEAD(, vga_raster_font) vc_fontlist;
#endif /* !VGA_RASTERCONSOLE */
};

struct vga_softc {
	struct device sc_dev;
	struct vga_config *sc_vc;
};

static inline u_int8_t 	_vga_attr_read(struct vga_handle *, int);
static inline void 	_vga_attr_write(struct vga_handle *, int, u_int8_t);
static inline u_int8_t 	_vga_ts_read(struct vga_handle *, int);
static inline void 	_vga_ts_write(struct vga_handle *, int, u_int8_t);
static inline u_int8_t 	_vga_gdc_read(struct vga_handle *, int);
static inline void 	_vga_gdc_write(struct vga_handle *, int, u_int8_t);

static inline u_int8_t
_vga_attr_read(struct vga_handle *vh, int reg)
{
	u_int8_t res;

	/* reset state */
	(void) bus_space_read_1(vh->vh_iot, vh->vh_ioh_6845, 10);

	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, VGA_ATC_INDEX, reg);
	res = bus_space_read_1(vh->vh_iot, vh->vh_ioh_vga, VGA_ATC_DATAR);

	/* reset state XXX unneeded? */
	(void) bus_space_read_1(vh->vh_iot, vh->vh_ioh_6845, 10);

	/* enable */
	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, 0, 0x20);

	return (res);
}

static inline void
_vga_attr_write(struct vga_handle *vh, int reg, u_int8_t val)
{

	/* reset state */
	(void) bus_space_read_1(vh->vh_iot, vh->vh_ioh_6845, 10);

	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, VGA_ATC_INDEX, reg);
	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, VGA_ATC_DATAW, val);

	/* reset state XXX unneeded? */
	(void) bus_space_read_1(vh->vh_iot, vh->vh_ioh_6845, 10);

	/* enable */
	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, 0, 0x20);
}

static inline u_int8_t
_vga_ts_read(struct vga_handle *vh, int reg)
{

	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, VGA_TS_INDEX, reg);
	return (bus_space_read_1(vh->vh_iot, vh->vh_ioh_vga, VGA_TS_DATA));
}

static inline void
_vga_ts_write(struct vga_handle *vh, int reg, u_int8_t val)
{

	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, VGA_TS_INDEX, reg);
	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, VGA_TS_DATA, val);
}

static inline u_int8_t
_vga_gdc_read(struct vga_handle *vh, int reg)
{

	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, VGA_GDC_INDEX, reg);
	return (bus_space_read_1(vh->vh_iot, vh->vh_ioh_vga, VGA_GDC_DATA));
}

static inline void
_vga_gdc_write(struct vga_handle *vh, int reg, u_int8_t val)
{

	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, VGA_GDC_INDEX, reg);
	bus_space_write_1(vh->vh_iot, vh->vh_ioh_vga, VGA_GDC_DATA, val);
}

#define vga_attr_read(vh, reg) \
	_vga_attr_read(vh, offsetof(struct reg_vgaattr, reg))
#define vga_attr_write(vh, reg, val) \
	_vga_attr_write(vh, offsetof(struct reg_vgaattr, reg), val)
#define vga_ts_read(vh, reg) \
	_vga_ts_read(vh, offsetof(struct reg_vgats, reg))
#define vga_ts_write(vh, reg, val) \
	_vga_ts_write(vh, offsetof(struct reg_vgats, reg), val)
#define vga_gdc_read(vh, reg) \
	_vga_gdc_read(vh, offsetof(struct reg_vgagdc, reg))
#define vga_gdc_write(vh, reg, val) \
	_vga_gdc_write(vh, offsetof(struct reg_vgagdc, reg), val)

#define vga_6845_read(vh, reg) \
	pcdisplay_6845_read(&(vh)->vh_ph, reg)
#define vga_6845_write(vh, reg, val) \
	pcdisplay_6845_write(&(vh)->vh_ph, reg, val)
#define _vga_6845_read(vh, reg) \
	_pcdisplay_6845_read(&(vh)->vh_ph, reg)
#define _vga_6845_write(vh, reg, val) \
	_pcdisplay_6845_write(&(vh)->vh_ph, reg, val)

int	vga_common_probe(bus_space_tag_t, bus_space_tag_t);
void	vga_common_attach(struct vga_softc *, bus_space_tag_t,
			  bus_space_tag_t, int, int, const struct vga_funcs *);
#define VGA_QUIRK_ONEFONT	0x01
#define VGA_QUIRK_NOFASTSCROLL	0x02
int	vga_is_console(bus_space_tag_t, int);

int	vga_cnattach(bus_space_tag_t, bus_space_tag_t, int, int);

#ifndef VGA_RASTERCONSOLE
struct wsscreen_descr;
void 	vga_loadchars(struct vga_handle *, int, int, int, int, const char *);
void 	vga_readoutchars(struct vga_handle *, int, int, int, int, char *);
#ifdef VGA_CONSOLE_ATI_BROKEN_FONTSEL
void 	vga_copyfont01(struct vga_handle *);
#endif
void 	vga_setfontset(struct vga_handle *, int, int);
void 	vga_setscreentype(struct vga_handle *, const struct wsscreen_descr *);
#else /* !VGA_RASTERCONSOLE */
void 	vga_load_builtinfont(struct vga_handle *, u_int8_t *, int, int);
#endif /* !VGA_RASTERCONSOLE */
#ifdef VGA_RESET
void	vga_reset(struct vga_handle *, void (*)(struct vga_handle *));
#endif

extern int vga_no_builtinfont;
