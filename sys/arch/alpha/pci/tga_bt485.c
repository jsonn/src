/*	$NetBSD: tga_bt485.c,v 1.4.2.1 1997/01/25 01:33:53 cgd Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <vm/vm.h>

#include <dev/pci/pcivar.h>
#include <machine/tgareg.h>
#include <alpha/pci/tgavar.h>
#include <alpha/pci/bt485reg.h>

#include <machine/wsconsio.h>

/*
 * Functions exported via the RAMDAC configuration table.
 */
void	tga_bt485_init __P((struct tga_devconfig *, int));
int	tga_bt485_intr __P((void *));
int	tga_bt485_set_cmap __P((struct tga_devconfig *,
	    struct wsdisplay_cmap *));
int	tga_bt485_get_cmap __P((struct tga_devconfig *,
	    struct wsdisplay_cmap *));
int	tga_bt485_set_cursor __P((struct tga_devconfig *,
	    struct wsdisplay_cursor *));
int	tga_bt485_get_cursor __P((struct tga_devconfig *,
	    struct wsdisplay_cursor *));
int	tga_bt485_set_curpos __P((struct tga_devconfig *,
	    struct wsdisplay_curpos *));
int	tga_bt485_get_curpos __P((struct tga_devconfig *,
	    struct wsdisplay_curpos *));
int	tga_bt485_get_curmax __P((struct tga_devconfig *,
	    struct wsdisplay_curpos *));

const struct tga_ramdac_conf tga_ramdac_bt485 = {
	"Bt485",
	tga_bt485_init,
	tga_bt485_intr,
	tga_bt485_set_cmap,
	tga_bt485_get_cmap,
	tga_bt485_set_cursor,
	tga_bt485_get_cursor,
	tga_bt485_set_curpos,
	tga_bt485_get_curpos,
	tga_bt485_get_curmax,
};

/*
 * Private data.
 */
struct bt485data {
	int	changed;			/* what changed; see below */
	int	curenb;				/* cursor enabled */
	struct wsdisplay_curpos curpos;		/* current cursor position */
	struct wsdisplay_curpos curhot;		/* cursor hotspot */
	char curcmap_r[2];			/* cursor colormap */
	char curcmap_g[2];
	char curcmap_b[2];
	struct wsdisplay_curpos cursize;	/* current cursor size */
	char curimage[512];			/* cursor image data */
	char curmask[512];			/* cursor mask data */
	char cmap_r[256];				/* colormap */
	char cmap_g[256];
	char cmap_b[256];
};

#define	DATA_ENB_CHANGED	0x01	/* cursor enable changed */
#define	DATA_CURCMAP_CHANGED	0x02	/* cursor colormap changed */
#define	DATA_CURSHAPE_CHANGED	0x04	/* cursor size, image, mask changed */
#define	DATA_CMAP_CHANGED	0x08	/* colormap changed */
#define	DATA_ALL_CHANGED	0x0f

#define	CURSOR_MAX_SIZE		64

/*
 * Internal functions.
 */
inline void	tga_bt485_wr_d __P((volatile tga_reg_t *, u_int, u_int8_t));
inline u_int8_t tga_bt485_rd_d __P((volatile tga_reg_t *, u_int));
inline void	tga_bt485_wr_i  __P((volatile tga_reg_t *, u_int8_t, u_int8_t));
inline u_int8_t tga_bt485_rd_i __P((volatile tga_reg_t *, u_int8_t));
void	tga_bt485_update __P((struct tga_devconfig *, struct bt485data *));
void	tga_bt485_update_curpos __P((struct tga_devconfig *, struct bt485data *));

#define	tga_bt485_sched_update(dc)					\
    ((dc)->dc_regs[TGA_REG_SISR] = 0x00010000)			/* XXX */

/*****************************************************************************/

/*
 * Functions exported via the RAMDAC configuration table.
 */

void
tga_bt485_init(dc, alloc)
	struct tga_devconfig *dc;
	int alloc;
{
	u_int8_t regval;
	struct bt485data tmp, *data;
	int i;

	/*
	 * Init the BT485 for normal operation.
	 */

	/*
	 * Allow indirect register access.  (Actually, this is
	 * already enabled.  In fact, if it is _disabled_, for
	 * some reason the monitor appears to lose sync!!! (?!?!)
	 */
	regval = tga_bt485_rd_d(dc->dc_regs, BT485_REG_COMMAND_0);
	regval |= 0x80;
	tga_bt485_wr_d(dc->dc_regs, BT485_REG_COMMAND_0, regval);

	/* Set the RAMDAC to 8BPP (no interestion options). */
	tga_bt485_wr_d(dc->dc_regs, BT485_REG_COMMAND_1, 0x40);

	/* Disable the cursor (for now) */
	regval = tga_bt485_rd_d(dc->dc_regs, BT485_REG_COMMAND_2);
	regval &= ~0x03;
	tga_bt485_wr_d(dc->dc_regs, BT485_REG_COMMAND_2, regval);

	/* Use a 64x64x2 cursor */
	regval = tga_bt485_rd_d(dc->dc_regs, BT485_IREG_COMMAND_3);
	regval |= 0x04;
	tga_bt485_wr_d(dc->dc_regs, BT485_IREG_COMMAND_3, regval);

	/*
	 * If we should allocate a new private info struct, do so.
	 * Otherwise, use the one we have (if it's there), or
	 * use the temporary one on the stack.
	 */
	if (alloc) {
		if (dc->dc_ramdac_private != NULL)
			panic("tga_bt485_init: already have private struct");
		dc->dc_ramdac_private = malloc(sizeof *data, M_DEVBUF,
		    M_WAITOK);
	}
	if (dc->dc_ramdac_private != NULL)
		data = dc->dc_ramdac_private;
	else
		data = &tmp;

	/*
	 * Initalize the RAMDAC info struct to hold all of our
	 * data, and fill it in.
	 */
	data->changed = DATA_ALL_CHANGED;

	data->curenb = 0;				/* cursor disabled */
	data->curpos.x = data->curpos.y = 0;		/* right now at 0,0 */
	data->curhot.x = data->curhot.y = 0;		/* hot spot at 0,0 */

	/* initial cursor colormap: 0 is black, 1 is white */
	data->curcmap_r[0] = data->curcmap_g[0] = data->curcmap_b[0] = 0;
	data->curcmap_r[1] = data->curcmap_g[1] = data->curcmap_b[1] = 0xff;

	/* initial cursor data: 64x64 block of white. */
	data->cursize.x = data->cursize.y = 64;
	for (i = 0; i < 512; i++)
		data->curimage[i] = data->curmask[i] = 0xff;

	/* Initial colormap: 0 is black, everything else is white */
	data->cmap_r[0] = data->cmap_g[0] = data->cmap_b[0] = 0;
	for (i = 1; i < 256; i++)
		data->cmap_r[i] = data->cmap_g[i] = data->cmap_b[i] = 255;

	tga_bt485_update(dc, data);

	dc->dc_regs[TGA_REG_SISR] = 0x00000001;			/* XXX */
}

int
tga_bt485_set_cmap(dc, cmapp)
	struct tga_devconfig *dc;
	struct wsdisplay_cmap *cmapp;
{
	struct bt485data *data = dc->dc_ramdac_private;
	int count, index, s;

	if ((u_int)cmapp->index >= 256 ||
	    ((u_int)cmapp->index + (u_int)cmapp->count) > 256)
		return (EINVAL);
	if (!useracc(cmapp->red, cmapp->count, B_READ) ||
	    !useracc(cmapp->green, cmapp->count, B_READ) ||
	    !useracc(cmapp->blue, cmapp->count, B_READ))
		return (EFAULT);

	s = spltty();

	index = cmapp->index;
	count = cmapp->count;
	copyin(cmapp->red, &data->cmap_r[index], count);
	copyin(cmapp->green, &data->cmap_g[index], count);
	copyin(cmapp->blue, &data->cmap_b[index], count);

	data->changed |= DATA_CMAP_CHANGED;

	tga_bt485_sched_update(dc);
	splx(s);

	return (0);
}

int
tga_bt485_get_cmap(dc, cmapp)
	struct tga_devconfig *dc;
	struct wsdisplay_cmap *cmapp;
{
	struct bt485data *data = dc->dc_ramdac_private;
	int error, count, index;

	if ((u_int)cmapp->index >= 256 ||
	    ((u_int)cmapp->index + (u_int)cmapp->count) > 256)
		return (EINVAL);

	count = cmapp->count;
	index = cmapp->index;

	error = copyout(&data->cmap_r[index], cmapp->red, count);
	if (error)
		return (error);
	error = copyout(&data->cmap_g[index], cmapp->green, count);
	if (error)
		return (error);
	error = copyout(&data->cmap_b[index], cmapp->blue, count);
	return (error);
}

int
tga_bt485_set_cursor(dc, cursorp)
	struct tga_devconfig *dc;
	struct wsdisplay_cursor *cursorp;
{
	struct bt485data *data = dc->dc_ramdac_private;
	int count, index, v, s;

	v = cursorp->which;

	/*
	 * For DOCMAP and DOSHAPE, verify that parameters are OK
	 * before we do anything that we can't recover from.
	 */
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		if ((u_int)cursorp->cmap.index > 2 ||
		    ((u_int)cursorp->cmap.index +
		     (u_int)cursorp->cmap.count) > 2)
			return (EINVAL);
		count = cursorp->cmap.count;
		if (!useracc(cursorp->cmap.red, count, B_READ) ||
		    !useracc(cursorp->cmap.green, count, B_READ) ||
		    !useracc(cursorp->cmap.blue, count, B_READ))
			return (EFAULT);
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		if ((u_int)cursorp->size.x > CURSOR_MAX_SIZE ||
		    (u_int)cursorp->size.y > CURSOR_MAX_SIZE)
			return (EINVAL);
		count = (CURSOR_MAX_SIZE / NBBY) * data->cursize.y;
		if (!useracc(cursorp->image, count, B_READ) ||
		    !useracc(cursorp->mask, count, B_READ))
			return (EFAULT);
	}

	if (v & (WSDISPLAY_CURSOR_DOPOS | WSDISPLAY_CURSOR_DOCUR)) {
		if (v & WSDISPLAY_CURSOR_DOPOS)
			data->curpos = cursorp->pos;
		if (v & WSDISPLAY_CURSOR_DOCUR)
			data->curhot = cursorp->hot;
		tga_bt485_update_curpos(dc, data);
	}

	s = spltty();

	/* Parameters are OK; perform the requested operations. */
	if (v & WSDISPLAY_CURSOR_DOCUR) {
		data->curenb = cursorp->enable;
		data->changed |= DATA_ENB_CHANGED;
	}
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		count = cursorp->cmap.count;
		index = cursorp->cmap.index;
		copyin(cursorp->cmap.red, &data->cmap_r[index], count);
		copyin(cursorp->cmap.green, &data->cmap_g[index], count);
		copyin(cursorp->cmap.blue, &data->cmap_b[index], count);
		data->changed |= DATA_CURCMAP_CHANGED;
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		data->cursize = cursorp->size;
		count = (CURSOR_MAX_SIZE / NBBY) * data->cursize.y;
		bzero(data->curimage, sizeof data->curimage);
		bzero(data->curmask, sizeof data->curmask);
		copyin(cursorp->image, data->curimage, count);	/* can't fail */
		copyin(cursorp->mask, data->curmask, count);	/* can't fail */
		data->changed |= DATA_CURSHAPE_CHANGED;
	}

	if (data->changed)
		tga_bt485_sched_update(dc);
	splx(s);

	return (0);
}

int
tga_bt485_get_cursor(dc, cursorp)
	struct tga_devconfig *dc;
	struct wsdisplay_cursor *cursorp;
{
	struct bt485data *data = dc->dc_ramdac_private;
	int error, count;

	/* we return everything they want */
	cursorp->which = WSDISPLAY_CURSOR_DOALL;

	cursorp->enable = data->curenb;	/* DOCUR */
	cursorp->pos = data->curpos;	/* DOPOS */
	cursorp->hot = data->curhot;	/* DOHOT */

	cursorp->cmap.index = 0;	/* DOCMAP */
	cursorp->cmap.count = 2;
	if (cursorp->cmap.red != NULL) {
		error = copyout(data->curcmap_r, cursorp->cmap.red, 2);
		if (error)
			return (error);
	}
	if (cursorp->cmap.green != NULL) {
		error = copyout(data->curcmap_g, cursorp->cmap.green, 2);
		if (error)
			return (error);
	}
	if (cursorp->cmap.blue != NULL) {
		error = copyout(data->curcmap_b, cursorp->cmap.blue, 2);
		if (error)
			return (error);
	}

	cursorp->size = data->cursize;	/* DOSHAPE */
	if (cursorp->image != NULL) {
		count = (CURSOR_MAX_SIZE / NBBY) * data->cursize.y;
		error = copyout(data->curimage, cursorp->image, count);
		if (error)
			return (error);
		error = copyout(data->curmask, cursorp->mask, count);
		if (error)
			return (error);
	}

	return (0);
}

int
tga_bt485_set_curpos(dc, curposp)
	struct tga_devconfig *dc;
	struct wsdisplay_curpos *curposp;
{
	struct bt485data *data = dc->dc_ramdac_private;

	data->curpos = *curposp;
	tga_bt485_update_curpos(dc, data);

	return (0);
}

int
tga_bt485_get_curpos(dc, curposp)
	struct tga_devconfig *dc;
	struct wsdisplay_curpos *curposp;
{
	struct bt485data *data = dc->dc_ramdac_private;

	*curposp = data->curpos;
	return (0);
}

int
tga_bt485_get_curmax(dc, curposp)
	struct tga_devconfig *dc;
	struct wsdisplay_curpos *curposp;
{

	curposp->x = curposp->y = CURSOR_MAX_SIZE;
	return (0);
}

int
tga_bt485_intr(v)
	void *v;
{
	struct tga_devconfig *dc = v;

	if ((dc->dc_regs[TGA_REG_SISR] & 0x00010001) != 0x00010001)
		return 0;
	tga_bt485_update(dc, dc->dc_ramdac_private);
	dc->dc_regs[TGA_REG_SISR] = 0x00000001;
	return (1);
}

/*****************************************************************************/

/*
 * Internal functions.
 */

inline void
tga_bt485_wr_d(tgaregs, btreg, val)
	volatile tga_reg_t *tgaregs;
	u_int btreg;
	u_int8_t val;
{

	if (btreg > BT485_REG_MAX)
		panic("tga_bt485_wr_d: reg %d out of range\n", btreg);

	tgaregs[TGA_REG_EPDR] = (btreg << 9) | (0 << 8 ) | val; /* XXX */
	alpha_mb();
}

inline u_int8_t
tga_bt485_rd_d(tgaregs, btreg)
	volatile tga_reg_t *tgaregs;
	u_int btreg;
{
	tga_reg_t rdval;

	if (btreg > BT485_REG_MAX)
		panic("tga_bt485_rd_d: reg %d out of range\n", btreg);

	tgaregs[TGA_REG_EPSR] = (btreg << 1) | 0x1;		/* XXX */
	alpha_mb();

	rdval = tgaregs[TGA_REG_EPDR];
	return (rdval >> 16) & 0xff;				/* XXX */
}

inline void
tga_bt485_wr_i(tgaregs, ireg, val)
	volatile tga_reg_t *tgaregs;
	u_int8_t ireg;
	u_int8_t val;
{
	tga_bt485_wr_d(tgaregs, BT485_REG_PCRAM_WRADDR, ireg);
	tga_bt485_wr_d(tgaregs, BT485_REG_EXTENDED, val);
}

inline u_int8_t
tga_bt485_rd_i(tgaregs, ireg)
	volatile tga_reg_t *tgaregs;
	u_int8_t ireg;
{
	tga_bt485_wr_d(tgaregs, BT485_REG_PCRAM_WRADDR, ireg);
	return (tga_bt485_rd_d(tgaregs, BT485_REG_EXTENDED));
}

void
tga_bt485_update(dc, data)
	struct tga_devconfig *dc;
	struct bt485data *data;
{
	u_int8_t regval;
	int count, i, v;

	v = data->changed;
	data->changed = 0;

	if (v & DATA_ENB_CHANGED) {
		regval = tga_bt485_rd_d(dc->dc_regs, BT485_REG_COMMAND_2);
		if (data->curenb)
			regval |= 0x01;
		else
			regval &= ~0x03;
                tga_bt485_wr_d(dc->dc_regs, BT485_REG_COMMAND_2, regval);
	}

	if (v & DATA_CURCMAP_CHANGED) {
		/* addr[9:0] assumed to be 0 */
		/* set addr[7:0] to 1 */
                tga_bt485_wr_d(dc->dc_regs, BT485_REG_COC_WRADDR, 0x01);

		/* spit out the cursor data */
		for (i = 0; i < 2; i++) {
                	tga_bt485_wr_d(dc->dc_regs, BT485_REG_COCDATA,
			    data->curcmap_r[i]);
                	tga_bt485_wr_d(dc->dc_regs, BT485_REG_COCDATA,
			    data->curcmap_g[i]);
                	tga_bt485_wr_d(dc->dc_regs, BT485_REG_COCDATA,
			    data->curcmap_b[i]);
		}
	}

	if (v & DATA_CURSHAPE_CHANGED) {
		count = (CURSOR_MAX_SIZE / NBBY) * data->cursize.y;

		/*
		 * Write the cursor image data:
		 *	set addr[9:8] to 0,
		 *	set addr[7:0] to 0,
		 *	spit it all out.
		 */
		regval = tga_bt485_rd_i(dc->dc_regs,
		    BT485_IREG_COMMAND_3);
		regval &= ~0x03;
		tga_bt485_wr_i(dc->dc_regs, BT485_IREG_COMMAND_3,
		    regval);
                tga_bt485_wr_d(dc->dc_regs, BT485_REG_PCRAM_WRADDR, 0);
		for (i = 0; i < count; i++)
			tga_bt485_wr_d(dc->dc_regs, BT485_REG_CURSOR_RAM,
			    data->curimage[i]);
		
		/*
		 * Write the cursor mask data:
		 *	set addr[9:8] to 2,
		 *	set addr[7:0] to 0,
		 *	spit it all out.
		 */
		regval = tga_bt485_rd_i(dc->dc_regs,
		    BT485_IREG_COMMAND_3);
		regval &= ~0x03; regval |= 0x02;
		tga_bt485_wr_i(dc->dc_regs, BT485_IREG_COMMAND_3,
		    regval);
                tga_bt485_wr_d(dc->dc_regs, BT485_REG_PCRAM_WRADDR, 0);
		for (i = 0; i < count; i++)
			tga_bt485_wr_d(dc->dc_regs, BT485_REG_CURSOR_RAM,
			    data->curmask[i]);

		/* set addr[9:0] back to 0 */
		regval = tga_bt485_rd_i(dc->dc_regs, BT485_IREG_COMMAND_3);
		regval &= ~0x03;
		tga_bt485_wr_i(dc->dc_regs, BT485_IREG_COMMAND_3, regval);
	}

	if (v & DATA_CMAP_CHANGED) {
		/* addr[9:0] assumed to be 0 */
		/* set addr[7:0] to 0 */
                tga_bt485_wr_d(dc->dc_regs, BT485_REG_PCRAM_WRADDR, 0x00);

		/* spit out the cursor data */
		for (i = 0; i < 256; i++) {
                	tga_bt485_wr_d(dc->dc_regs, BT485_REG_PALETTE,
			    data->cmap_r[i]);
                	tga_bt485_wr_d(dc->dc_regs, BT485_REG_PALETTE,
			    data->cmap_g[i]);
                	tga_bt485_wr_d(dc->dc_regs, BT485_REG_PALETTE,
			    data->cmap_b[i]);
		}
	}
}

void
tga_bt485_update_curpos(dc, data)
	struct tga_devconfig *dc;
	struct bt485data *data;
{
	int s, x, y;

	s = spltty();

	x = data->curpos.x + CURSOR_MAX_SIZE - data->curhot.x;
	y = data->curpos.y + CURSOR_MAX_SIZE - data->curhot.y;
	tga_bt485_wr_d(dc->dc_regs, BT485_REG_CURSOR_X_LOW, x & 0xff);
	tga_bt485_wr_d(dc->dc_regs, BT485_REG_CURSOR_X_HIGH, (x >> 8) & 0x0f);
	tga_bt485_wr_d(dc->dc_regs, BT485_REG_CURSOR_Y_LOW, y & 0xff);
	tga_bt485_wr_d(dc->dc_regs, BT485_REG_CURSOR_Y_HIGH, (y >> 8) & 0x0f);

	splx(s);
}
