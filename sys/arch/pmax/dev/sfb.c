/*	$NetBSD: sfb.c,v 1.11.4.1 1996/05/30 04:03:55 mhitch Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	from: @(#)sfb.c	8.1 (Berkeley) 6/10/93
 */

/*
 *  devGraphics.c --
 *
 *     	This file contains machine-dependent routines for the graphics device.
 *
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.  
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/dev/ds3100.md/RCS/devGraphics.c,
 *	v 9.2 90/02/13 22:16:24 shirriff Exp  SPRITE (DECWRL)";
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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

#include "fb.h"
#include "sfb.h"

#include <sys/param.h>
#include <sys/systm.h>					/* printf() */
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <dev/tc/tcvar.h>
#include <machine/fbio.h>
#include <machine/fbvar.h>


#include <pmax/dev/bt459.h>
#include <pmax/dev/sfbreg.h>

#include <machine/machConst.h>
#include <pmax/pmax/pmaxtype.h>
#include <machine/pmioctl.h>
#include <pmax/dev/fbreg.h>

/*
 * These need to be mapped into user space.
 */
struct fbuaccess sfbu;
struct pmax_fbtty sfbfb;
struct fbinfo	sfbfi;	/*XXX*/ /* should be softc */


extern int pmax_boardtype;

/*
 * Forward references.
 */

int sfbinit __P((struct fbinfo *fi, caddr_t cfbaddr, int unit, int silent));

#define CMAP_BITS	(3 * 256)		/* 256 entries, 3 bytes per. */
static u_char cmap_bits [CMAP_BITS];		/* colormap for console... */

int sfbmatch __P((struct device *, void *, void *));
void sfbattach __P((struct device *, struct device *, void *));

struct cfattach sfb_ca = {
	sizeof(struct device), sfbmatch, sfbattach
};

struct cfdriver sfb_cd = {
	NULL, "sfb", DV_DULL
};

struct fbdriver sfb_driver = {
	bt459_video_on,
	bt459_video_off,
	bt459InitColorMap,
	bt459GetColorMap,
	bt459LoadColorMap,
	bt459PosCursor,
	bt459LoadCursor,
	bt459CursorColor
};


/* match and attach routines cut-and-pasted from cfb */

int
sfbmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	/*struct cfdata *cf = match;*/
	struct tc_attach_args *ta = aux;

	/* make sure that we're looking for this type of device. */
	if (!TC_BUS_MATCHNAME(ta, "PMAGB-BA"))
		return (0);

	/*
	 * if the TC rom ident matches, assume the VRAM is present too.
	 */
#if 0
	if (badaddr( ((caddr_t)ta->ta_addr) + SFB_OFFSET_VRAM, 4))
		return (0);
#endif

	return (1);
}

/*
 * Attach a device.  Hand off all the work to sfbinit(),
 * so console-config code can attach sfbs early in boot.
 */
void
sfbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct tc_attach_args *ta = aux;
	caddr_t sfbaddr = (caddr_t)ta->ta_addr;
	int unit = self->dv_unit;
	struct fbinfo *fi = (struct fbinfo *) self;

#ifdef notyet
	/* if this is the console, it's already configured. */
	if (ta->ta_cookie == cons_slot)
		return;	/* XXX patch up f softc pointer */
#endif

	if (!sfbinit(fi, sfbaddr, unit, 0))
		return;

#if 0 /*XXX*/
	*(sfbaddr + SFB_INTERRUPT_ENABLE) = 0;
#endif
}


/*
 * Initialization
 */
int
sfbinit(fi, base, unit, silent)
	struct fbinfo *fi;
	char *base;
	int unit;
	int silent;
{

	/*
	 * If this device is being intialized as the console, malloc()
	 * is not yet up and we must use statically-allocated space.
	 */
	if (fi == NULL) {
		fi = &sfbfi;	/* XXX */
  		fi->fi_cmap_bits = (caddr_t)cmap_bits;
	}
	else {
    		fi->fi_cmap_bits = malloc(CMAP_BITS, M_DEVBUF, M_NOWAIT);
		if (fi->fi_cmap_bits == NULL) {
			printf("cfb%d: no memory for cmap\n", unit);
			return (0);
		}
	}

	/* check for no frame buffer */
	if (badaddr(base + SFB_OFFSET_VRAM, 4))
		return (0);

	/* Fill in main frame buffer info struct. */
	fi->fi_unit = unit;
	fi->fi_pixels = (caddr_t)(base + SFB_OFFSET_VRAM);
	fi->fi_pixelsize = 1280 * 1024;
	fi->fi_base = (caddr_t)(base + SFB_ASIC_OFFSET);
	fi->fi_vdac = (caddr_t)(base + SFB_OFFSET_BT459);
	fi->fi_size = (fi->fi_pixels + SFB_FB_SIZE) - fi->fi_base;
	fi->fi_linebytes = 1280;
	fi->fi_driver = &sfb_driver;
	fi->fi_blanked = 0;

	/* Fill in Frame Buffer Type struct. */
	fi->fi_type.fb_boardtype = PMAX_FBTYPE_SFB;
	fi->fi_type.fb_height = 1024;
	fi->fi_type.fb_width = 1280;
	fi->fi_type.fb_depth = 8;
	fi->fi_type.fb_cmsize = 256;
	fi->fi_type.fb_size = SFB_FB_SIZE;

	/* Initialize the RAMDAC. */
	bt459init (fi);

	/* Initialize the SFB ASIC... */
	*((int *)(base + SFB_MODE)) = 0;
	*((int *)(base + SFB_PLANEMASK)) = 0xFFFFFFFF;

/* XXX below, up to fbconnect(), cut-and-pasted from cfb */
	/*
	 * qvss/pm-style mmap()ed event queue compatibility glue
	 */

	/*
	 * Must be in Uncached space since the fbuaccess structure is
	 * mapped into the user's address space uncached.
	 */
	fi->fi_fbu = (struct fbuaccess *)
		MACH_PHYS_TO_UNCACHED(MACH_CACHED_TO_PHYS(&sfbu));

	/* This is glass-tty state but it's in the shared structure. Ick. */
	fi->fi_fbu->scrInfo.max_row = 67;
	fi->fi_fbu->scrInfo.max_col = 80;

	init_pmaxfbu(fi);

	/*
	 * Initialize old-style pmax glass-tty screen info.
	 */
	fi->fi_glasstty = &sfbfb;


	/*
	 * Initialize the color map, the screen, and the mouse.
	 */
	if (tb_kbdmouseconfig(fi)) {
		printf(" (mouse/keyboard config failed)");
		return (0);
	}


	/*sfbInitColorMap();*/  /* done by bt459init() */

	/*
	 * Connect to the raster-console pseudo-driver
	 */

	fbconnect ("PMAGB-BA", fi, silent);
	return (1);
}

/* old bt459 code used to be here */
