/*	$NetBSD: grf_nubus.c,v 1.62.8.3 2002/10/18 02:38:27 nathanw Exp $	*/

/*
 * Copyright (c) 1995 Allen Briggs.  All rights reserved.
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
 * Device-specific routines for handling Nubus-based video cards.
 */

#include <sys/param.h>

#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/grfioctl.h>
#include <machine/viareg.h>

#include <mac68k/nubus/nubus.h>
#include <mac68k/dev/grfvar.h>

static void	load_image_data __P((caddr_t data, struct image_data *image));

static void	grfmv_intr_generic_write1 __P((void *vsc));
static void	grfmv_intr_generic_write4 __P((void *vsc));
static void	grfmv_intr_generic_or4 __P((void *vsc));

static void	grfmv_intr_cb264 __P((void *vsc));
static void	grfmv_intr_cb364 __P((void *vsc));
static void	grfmv_intr_cmax __P((void *vsc));
static void	grfmv_intr_cti __P((void *vsc));
static void	grfmv_intr_radius __P((void *vsc));
static void	grfmv_intr_radius24 __P((void *vsc));
static void	grfmv_intr_supermacgfx __P((void *vsc));
static void	grfmv_intr_lapis __P((void *vsc));
static void	grfmv_intr_formac __P((void *vsc));
static void	grfmv_intr_vimage __P((void *vsc));
static void	grfmv_intr_gvimage __P((void *vsc));
static void	grfmv_intr_radius_gsc __P((void *vsc));
static void	grfmv_intr_radius_gx __P((void *vsc));

static int	grfmv_mode __P((struct grf_softc *gp, int cmd, void *arg));
static int	grfmv_match __P((struct device *, struct cfdata *, void *));
static void	grfmv_attach __P((struct device *, struct device *, void *));

CFATTACH_DECL(macvid, sizeof(struct grfbus_softc),
    grfmv_match, grfmv_attach, NULL, NULL);

static void
load_image_data(data, image)
	caddr_t	data;
	struct	image_data *image;
{
	bcopy(data     , &image->size,       4);
	bcopy(data +  4, &image->offset,     4);
	bcopy(data +  8, &image->rowbytes,   2);
	bcopy(data + 10, &image->top,        2);
	bcopy(data + 12, &image->left,       2);
	bcopy(data + 14, &image->bottom,     2);
	bcopy(data + 16, &image->right,      2);
	bcopy(data + 18, &image->version,    2);
	bcopy(data + 20, &image->packType,   2);
	bcopy(data + 22, &image->packSize,   4);
	bcopy(data + 26, &image->hRes,       4);
	bcopy(data + 30, &image->vRes,       4);
	bcopy(data + 34, &image->pixelType,  2);
	bcopy(data + 36, &image->pixelSize,  2);
	bcopy(data + 38, &image->cmpCount,   2);
	bcopy(data + 40, &image->cmpSize,    2);
	bcopy(data + 42, &image->planeBytes, 4);
}


static int
grfmv_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct nubus_attach_args *na = (struct nubus_attach_args *)aux;

	if (na->category != NUBUS_CATEGORY_DISPLAY)
		return 0;

	if (na->type != NUBUS_TYPE_VIDEO)
		return 0;

	if (na->drsw != NUBUS_DRSW_APPLE)
		return 0;

	/*
	 * If we've gotten this far, then we're dealing with a real-live
	 * Apple QuickDraw-compatible display card resource.  Now, how to
	 * determine that this is an active resource???  Dunno.  But we'll
	 * proceed like it is.
	 */

	return 1;
}

static void
grfmv_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)self;
	struct nubus_attach_args *na = (struct nubus_attach_args *)aux;
	struct image_data image_store, image;
	struct grfmode *gm;
	char cardname[CARD_NAME_LEN];
	nubus_dirent dirent;
	nubus_dir dir, mode_dir;
	int mode;

	bcopy(na->fmt, &sc->sc_slot, sizeof(nubus_slot));

	sc->sc_tag = na->na_tag;
	sc->card_id = na->drhw;
	sc->sc_basepa = (bus_addr_t)NUBUS_SLOT2PA(na->slot);
	sc->sc_fbofs = 0;

	if (bus_space_map(sc->sc_tag, sc->sc_basepa, NBMEMSIZE,
	    0, &sc->sc_handle)) {
		printf(": grfmv_attach: failed to map slot %d\n", na->slot);
		return;
	}

	nubus_get_main_dir(&sc->sc_slot, &dir);

	if (nubus_find_rsrc(sc->sc_tag, sc->sc_handle,
	    &sc->sc_slot, &dir, na->rsrcid, &dirent) <= 0) {
bad:
		bus_space_unmap(sc->sc_tag, sc->sc_handle, NBMEMSIZE);
		return;
	}

	nubus_get_dir_from_rsrc(&sc->sc_slot, &dirent, &sc->board_dir);

	if (nubus_find_rsrc(sc->sc_tag, sc->sc_handle,
	    &sc->sc_slot, &sc->board_dir, NUBUS_RSRC_TYPE, &dirent) <= 0)
		if ((na->rsrcid != 128) ||
		    (nubus_find_rsrc(sc->sc_tag, sc->sc_handle,
		    &sc->sc_slot, &dir, 129, &dirent) <= 0))
			goto bad;

	mode = NUBUS_RSRC_FIRSTMODE;
	if (nubus_find_rsrc(sc->sc_tag, sc->sc_handle,
	    &sc->sc_slot, &sc->board_dir, mode, &dirent) <= 0) {
		printf(": probe failed to get board rsrc.\n");
		goto bad;
	}

	nubus_get_dir_from_rsrc(&sc->sc_slot, &dirent, &mode_dir);

	if (nubus_find_rsrc(sc->sc_tag, sc->sc_handle,
	    &sc->sc_slot, &mode_dir, VID_PARAMS, &dirent) <= 0) {
		printf(": probe failed to get mode dir.\n");
		goto bad;
	}

	if (nubus_get_ind_data(sc->sc_tag, sc->sc_handle, &sc->sc_slot,
	    &dirent, (caddr_t)&image_store, sizeof(struct image_data)) <= 0) {
		printf(": probe failed to get indirect mode data.\n");
		goto bad;
	}

	/* Need to load display info (and driver?), etc... (?) */

	load_image_data((caddr_t)&image_store, &image);

	gm = &sc->curr_mode;
	gm->mode_id = mode;
	gm->ptype = image.pixelType;
	gm->psize = image.pixelSize;
	gm->width = image.right - image.left;
	gm->height = image.bottom - image.top;
	gm->rowbytes = image.rowbytes;
	gm->hres = image.hRes;
	gm->vres = image.vRes;
	gm->fbsize = gm->height * gm->rowbytes;
	gm->fbbase = (caddr_t)(sc->sc_handle.base);	/* XXX evil hack */
	gm->fboff = image.offset;

	strncpy(cardname, nubus_get_card_name(sc->sc_tag, sc->sc_handle,
	    &sc->sc_slot), CARD_NAME_LEN);
	cardname[CARD_NAME_LEN-1] = '\0';
	printf(": %s\n", cardname);

	if (sc->card_id == NUBUS_DRHW_TFB) {
		/*
		 * This is the Toby card, but apparently some manufacturers
		 * (like Cornerstone) didn't bother to get/use their own
		 * value here, even though the cards are different, so we
		 * so we try to differentiate here.
		 */
		if (strncmp(cardname, "Samsung 768", 11) == 0)
			sc->card_id = NUBUS_DRHW_SAM768;
		else if (strncmp(cardname, "Toby frame", 10) != 0)
			printf("%s: This display card pretends to be a TFB!\n",
			    sc->sc_dev.dv_xname);
	}

	switch (sc->card_id) {
	case NUBUS_DRHW_TFB:
	case NUBUS_DRHW_M2HRVC:
	case NUBUS_DRHW_PVC:
		sc->cli_offset = 0xa0000;
		sc->cli_value = 0;
		add_nubus_intr(na->slot, grfmv_intr_generic_write1, sc);
		break;
	case NUBUS_DRHW_WVC:
		sc->cli_offset = 0xa00000;
		sc->cli_value = 0;
		add_nubus_intr(na->slot, grfmv_intr_generic_write1, sc);
		break;
	case NUBUS_DRHW_COLORMAX:
		add_nubus_intr(na->slot, grfmv_intr_cmax, sc);
		break;
	case NUBUS_DRHW_SE30:
		/* Do nothing--SE/30 interrupts are disabled */
		break;
	case NUBUS_DRHW_MDC:
		sc->cli_offset = 0x200148;
		sc->cli_value = 1;
		add_nubus_intr(na->slot, grfmv_intr_generic_write4, sc);

		/* Enable interrupts; to disable, write 0x7 to this location */
		bus_space_write_4(sc->sc_tag, sc->sc_handle, 0x20013C, 5);
		break;
	case NUBUS_DRHW_CB264:
		add_nubus_intr(na->slot, grfmv_intr_cb264, sc);
		break;
	case NUBUS_DRHW_CB364:
		add_nubus_intr(na->slot, grfmv_intr_cb364, sc);
		break;
	case NUBUS_DRHW_RPC8:
		sc->cli_offset = 0xfdff8f;
		sc->cli_value = 0xff;
		add_nubus_intr(na->slot, grfmv_intr_generic_write1, sc);
		break;
	case NUBUS_DRHW_RPC8XJ:
		sc->cli_value = 0x66;
		add_nubus_intr(na->slot, grfmv_intr_radius, sc);
		break;
	case NUBUS_DRHW_RPC24X:
	case NUBUS_DRHW_BOOGIE:
		sc->cli_value = 0x64;
		add_nubus_intr(na->slot, grfmv_intr_radius, sc);
		break;
	case NUBUS_DRHW_RPC24XP:
		add_nubus_intr(na->slot, grfmv_intr_radius24, sc);
		break;
	case NUBUS_DRHW_RADGSC:
		add_nubus_intr(na->slot, grfmv_intr_radius_gsc, sc);
		break;
	case NUBUS_DRHW_RDCGX:
		add_nubus_intr(na->slot, grfmv_intr_radius_gx, sc);
		break;
	case NUBUS_DRHW_FIILX:
	case NUBUS_DRHW_FIISXDSP:
	case NUBUS_DRHW_FUTURASX:
		sc->cli_offset = 0xf05000;
		sc->cli_value = 0x80;
		add_nubus_intr(na->slot, grfmv_intr_generic_write1, sc);
		break;
	case NUBUS_DRHW_SAM768:
		add_nubus_intr(na->slot, grfmv_intr_cti, sc);
		break;
	case NUBUS_DRHW_SUPRGFX:
		add_nubus_intr(na->slot, grfmv_intr_supermacgfx, sc);
		break;
	case NUBUS_DRHW_SPECTRM8:
		sc->cli_offset = 0x0de178;
		sc->cli_value = 0x80;
		add_nubus_intr(na->slot, grfmv_intr_generic_or4, sc);
		break;
	case NUBUS_DRHW_LAPIS:
		add_nubus_intr(na->slot, grfmv_intr_lapis, sc);
		break;
	case NUBUS_DRHW_FORMAC:
		add_nubus_intr(na->slot, grfmv_intr_formac, sc);
		break;
	case NUBUS_DRHW_ROPS24LXI:
	case NUBUS_DRHW_ROPS24XLTV:
	case NUBUS_DRHW_ROPS24MXTV:
		sc->cli_offset = 0xfb0010;
		sc->cli_value = 0x00;
		add_nubus_intr(na->slot, grfmv_intr_generic_write4, sc);
		break;
	case NUBUS_DRHW_ROPSPPGT:
		sc->cli_offset = 0xf50010;
		sc->cli_value = 0x02;
		add_nubus_intr(na->slot, grfmv_intr_generic_write4, sc);
		break;
	case NUBUS_DRHW_VIMAGE:
		add_nubus_intr(na->slot, grfmv_intr_vimage, sc);
		break;
	case NUBUS_DRHW_GVIMAGE:
		add_nubus_intr(na->slot, grfmv_intr_gvimage, sc);
		break;
	case NUBUS_DRHW_MC2124NB:
		sc->cli_offset = 0xfd1000;
		sc->cli_value = 0x00;
		add_nubus_intr(na->slot, grfmv_intr_generic_write4, sc);
		break;
	case NUBUS_DRHW_MICRON:
		sc->cli_offset = 0xa00014;
		sc->cli_value = 0;
		add_nubus_intr(na->slot, grfmv_intr_generic_write4, sc);
		break;
	default:
		printf("%s: Unknown video card ID 0x%x --",
		    sc->sc_dev.dv_xname, sc->card_id);
		printf(" Not installing interrupt routine.\n");
		break;
	}

	/* Perform common video attachment. */
	grf_establish(sc, &sc->sc_slot, grfmv_mode);
}

static int
grfmv_mode(gp, cmd, arg)
	struct grf_softc *gp;
	int cmd;
	void *arg;
{
	switch (cmd) {
	case GM_GRFON:
	case GM_GRFOFF:
		return 0;
	case GM_CURRMODE:
		break;
	case GM_NEWMODE:
		break;
	case GM_LISTMODES:
		break;
	}
	return EINVAL;
}

/* Interrupt handlers... */
/*
 * Generic routine to clear interrupts for cards where it simply takes
 * a MOV.B to clear the interrupt.  The offset and value of this byte
 * varies between cards.
 */
/*ARGSUSED*/
static void
grfmv_intr_generic_write1(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;

	bus_space_write_1(sc->sc_tag, sc->sc_handle,
	    sc->cli_offset, (u_int8_t)sc->cli_value);
}

/*
 * Generic routine to clear interrupts for cards where it simply takes
 * a MOV.L to clear the interrupt.  The offset and value of this byte
 * varies between cards.
 */
/*ARGSUSED*/
static void
grfmv_intr_generic_write4(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;

	bus_space_write_4(sc->sc_tag, sc->sc_handle,
	    sc->cli_offset, sc->cli_value);
}

/*
 * Generic routine to clear interrupts for cards where it simply takes
 * an OR.L to clear the interrupt.  The offset and value of this byte
 * varies between cards.
 */
/*ARGSUSED*/
static void
grfmv_intr_generic_or4(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;
	unsigned long	scratch;

	scratch = bus_space_read_4(sc->sc_tag, sc->sc_handle, sc->cli_offset);
	scratch |= 0x80;
	bus_space_write_4(sc->sc_tag, sc->sc_handle, sc->cli_offset, scratch);
}

/*
 * Routine to clear interrupts for the Radius PrecisionColor 8xj card.
 */
/*ARGSUSED*/
static void
grfmv_intr_radius(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;
	u_int8_t c;

	c = sc->cli_value;

	c |= 0x80;
	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0xd00403, c);
	c &= 0x7f;
	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0xd00403, c);
}

/*
 * Routine to clear interrupts for the Radius PrecisionColor 24Xp card.
 * Is this what the 8xj routine is doing, too?
 */
/*ARGSUSED*/
static void
grfmv_intr_radius24(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;
	u_int8_t c;

	c = 0x80 | bus_space_read_1(sc->sc_tag, sc->sc_handle, 0xfffd8);
	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0xd00403, c);
	c &= 0x7f;
	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0xd00403, c);
}

/*
 * Routine to clear interrupts on Samsung 768x1006 video controller.
 * This controller was manufactured by Cornerstone Technology, Inc.,
 * now known as Cornerstone Imaging.
 *
 * To clear this interrupt, we apparently have to set, then clear,
 * bit 2 at byte offset 0x80000 from the card's base.
 *	Information for this provided by Brad Salai <bsalai@servtech.com>
 */
/*ARGSUSED*/
static void
grfmv_intr_cti(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;
	u_int8_t c;

	c = bus_space_read_1(sc->sc_tag, sc->sc_handle, 0x80000);
	c |= 0x02;
	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0x80000, c);
	c &= 0xfd;
	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0x80000, c);
}

/*ARGSUSED*/
static void
grfmv_intr_cb264(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc;
	volatile char *slotbase;

	sc = (struct grfbus_softc *)vsc;
	slotbase = (volatile char *)(sc->sc_handle.base); /* XXX evil hack */
	__asm __volatile(
		"	movl	%0,%%a0				\n"
		"	movl	%%a0@(0xff6028),%%d0		\n"
		"	andl	#0x2,%%d0			\n"
		"	beq	_mv_intr0			\n"
		"	movql	#0x3,%%d0			\n"
		"_mv_intr0:					\n"
		"	movl	%%a0@(0xff600c),%%d1		\n"
		"	andl	#0x3,%%d1			\n"
		"	cmpl	%%d1,%%d0			\n"
		"	beq	_mv_intr_fin			\n"
		"	movl	%%d0,%%a0@(0xff600c)		\n"
		"	nop					\n"
		"	tstb	%%d0				\n"
		"	beq	_mv_intr1			\n"
		"	movl	#0x0002,%%a0@(0xff6040)		\n"
		"	movl	#0x0102,%%a0@(0xff6044)		\n"
		"	movl	#0x0105,%%a0@(0xff6048)		\n"
		"	movl	#0x000e,%%a0@(0xff604c)		\n"
		"	movl	#0x001c,%%a0@(0xff6050)		\n"
		"	movl	#0x00bc,%%a0@(0xff6054)		\n"
		"	movl	#0x00c3,%%a0@(0xff6058)		\n"
		"	movl	#0x0061,%%a0@(0xff605c)		\n"
		"	movl	#0x0012,%%a0@(0xff6060)		\n"
		"	bra	_mv_intr_fin			\n"
		"_mv_intr1:					\n"
		"	movl	#0x0002,%%a0@(0xff6040)		\n"
		"	movl	#0x0209,%%a0@(0xff6044)		\n"
		"	movl	#0x020c,%%a0@(0xff6048)		\n"
		"	movl	#0x000f,%%a0@(0xff604c)		\n"
		"	movl	#0x0027,%%a0@(0xff6050)		\n"
		"	movl	#0x00c7,%%a0@(0xff6054)		\n"
		"	movl	#0x00d7,%%a0@(0xff6058)		\n"
		"	movl	#0x006b,%%a0@(0xff605c)		\n"
		"	movl	#0x0029,%%a0@(0xff6060)		\n"
		"_mv_intr_fin:					\n"
		"	movl	#0x1,%%a0@(0xff6014)"
		: : "g" (slotbase) : "a0","d0","d1");
}

/*
 * Support for the Colorboard 364 might be more complex than it needs to
 * be.  If we can find more information about this card, this might be
 * significantly simplified.  Contributions welcome...  :-)
 */
/*ARGSUSED*/
static void
grfmv_intr_cb364(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc;
	volatile char *slotbase;

	sc = (struct grfbus_softc *)vsc;
	slotbase = (volatile char *)(sc->sc_handle.base); /* XXX evil hack */
	__asm __volatile(
		"	movl	%0,%%a0				\n"
		"	movl	%%a0@(0xfe6028),%%d0		\n"
		"	andl	#0x2,%%d0			\n"
		"	beq	_cb364_intr4			\n"
		"	movql	#0x3,%%d0			\n"
		"	movl	%%a0@(0xfe6018),%%d1		\n"
		"	movl	#0x3,%%a0@(0xfe6018)		\n"
		"	movw	%%a0@(0xfe7010),%%d2		\n"
		"	movl	%%d1,%%a0@(0xfe6018)		\n"
		"	movl	%%a0@(0xfe6020),%%d1		\n"
		"	btst	#0x06,%%d2			\n"
		"	beq	_cb364_intr0			\n"
		"	btst	#0x00,%%d1			\n"
		"	beq	_cb364_intr5			\n"
		"	bsr	_cb364_intr1			\n"
		"	bra	_cb364_intr_out			\n"
		"_cb364_intr0:					\n"
		"	btst	#0x00,%%d1			\n"
		"	bne	_cb364_intr5			\n"
		"	bsr	_cb364_intr1			\n"
		"	bra	_cb364_intr_out			\n"
		"_cb364_intr1:					\n"
		"	movl	%%d0,%%a0@(0xfe600c)		\n"
		"	nop					\n"
		"	tstb	%%d0				\n"
		"	beq	_cb364_intr3			\n"
		"	movl	#0x0002,%%a0@(0xfe6040)		\n"
		"	movl	#0x0105,%%a0@(0xfe6048)		\n"
		"	movl	#0x000e,%%a0@(0xfe604c)		\n"
		"	movl	#0x00c3,%%a0@(0xfe6058)		\n"
		"	movl	#0x0061,%%a0@(0xfe605c)		\n"
		"	btst	#0x06,%%d2			\n"
		"	beq	_cb364_intr2			\n"
		"	movl	#0x001c,%%a0@(0xfe6050)		\n"
		"	movl	#0x00bc,%%a0@(0xfe6054)		\n"
		"	movl	#0x0012,%%a0@(0xfe6060)		\n"
		"	movl	#0x000e,%%a0@(0xfe6044)		\n"
		"	movl	#0x00c3,%%a0@(0xfe6064)		\n"
		"	movl	#0x0061,%%a0@(0xfe6020)		\n"
		"	rts					\n"
		"_cb364_intr2:					\n"
		"	movl	#0x0016,%%a0@(0xfe6050)		\n"
		"	movl	#0x00b6,%%a0@(0xfe6054)		\n"
		"	movl	#0x0011,%%a0@(0xfe6060)		\n"
		"	movl	#0x0101,%%a0@(0xfe6044)		\n"
		"	movl	#0x00bf,%%a0@(0xfe6064)		\n"
		"	movl	#0x0001,%%a0@(0xfe6020)		\n"
		"	rts					\n"
		"_cb364_intr3:					\n"
		"	movl	#0x0002,%%a0@(0xfe6040)		\n"
		"	movl	#0x0209,%%a0@(0xfe6044)		\n"
		"	movl	#0x020c,%%a0@(0xfe6048)		\n"
		"	movl	#0x000f,%%a0@(0xfe604c)		\n"
		"	movl	#0x0027,%%a0@(0xfe6050)		\n"
		"	movl	#0x00c7,%%a0@(0xfe6054)		\n"
		"	movl	#0x00d7,%%a0@(0xfe6058)		\n"
		"	movl	#0x006b,%%a0@(0xfe605c)		\n"
		"	movl	#0x0029,%%a0@(0xfe6060)		\n"
		"	oril	#0x0040,%%a0@(0xfe6064)		\n"
		"	movl	#0x0000,%%a0@(0xfe6020)		\n"
		"	rts					\n"
		"_cb364_intr4:					\n"
		"	movq	#0x00,%%d0			\n"
		"_cb364_intr5:					\n"
		"	movl	%%a0@(0xfe600c),%%d1		\n"
		"	andl	#0x3,%%d1			\n"
		"	cmpl	%%d1,%%d0			\n"
		"	beq	_cb364_intr_out			\n"
		"	bsr	_cb364_intr1			\n"
		"_cb364_intr_out:				\n"
		"	movl	#0x1,%%a0@(0xfe6014)		\n"
		"_cb364_intr_quit:"
		: : "g" (slotbase) : "a0","d0","d1","d2");
}

/*
 * Interrupt clearing routine for SuperMac GFX card.
 */
/*ARGSUSED*/
static void
grfmv_intr_supermacgfx(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;
	u_int8_t dummy;

	dummy = bus_space_read_1(sc->sc_tag, sc->sc_handle, 0xE70D3);
}

/*
 * Routine to clear interrupts for the Sigma Designs ColorMax card.
 */
/*ARGSUSED*/
static void
grfmv_intr_cmax(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;
	u_int32_t dummy;

	dummy = bus_space_read_4(sc->sc_tag, sc->sc_handle, 0xf501c);
	dummy = bus_space_read_4(sc->sc_tag, sc->sc_handle, 0xf5018);
}

/*
 * Routine to clear interrupts for the Lapis ProColorServer 8 PDS card
 * (for the SE/30).
 */
/*ARGSUSED*/
static void
grfmv_intr_lapis(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;

	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0xff7000, 0x08);
	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0xff7000, 0x0C);
}

/*
 * Routine to clear interrupts for the Formac Color Card II
 */
/*ARGSUSED*/
static void
grfmv_intr_formac(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;
	u_int8_t dummy;

	dummy = bus_space_read_1(sc->sc_tag, sc->sc_handle, 0xde80db);
	dummy = bus_space_read_1(sc->sc_tag, sc->sc_handle, 0xde80d3);
}

/*
 * Routine to clear interrupts for the Vimage by Interware Co., Ltd.
 */
/*ARGSUSED*/
static void
grfmv_intr_vimage(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;

	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0x800000, 0x67);
	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0x800000, 0xE7);
}

/*
 * Routine to clear interrupts for the Grand Vimage by Interware Co., Ltd.
 */
/*ARGSUSED*/
static void
grfmv_intr_gvimage(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;
	u_int8_t dummy;

	dummy = bus_space_read_1(sc->sc_tag, sc->sc_handle, 0xf00000);
}

/*
 * Routine to clear interrupts for the Radius GS/C
 */
/*ARGSUSED*/
static void
grfmv_intr_radius_gsc(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;
	u_int8_t dummy;

	dummy = bus_space_read_1(sc->sc_tag, sc->sc_handle, 0xfb802);
	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0xfb802, 0xff);
}

/*
 * Routine to clear interrupts for the Radius GS/C
 */
/*ARGSUSED*/
static void
grfmv_intr_radius_gx(vsc)
	void	*vsc;
{
	struct grfbus_softc *sc = (struct grfbus_softc *)vsc;

	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0x600000, 0x00);
	bus_space_write_1(sc->sc_tag, sc->sc_handle, 0x600000, 0x20);
}
