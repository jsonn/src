/*	$NetBSD: vidc20config.c,v 1.2.4.7 2002/10/18 02:35:33 nathanw Exp $	*/

/*
 * Copyright (c) 2001 Reinoud Zandijk
 * Copyright (c) 1996 Mark Brinicombe
 * Copyright (c) 1996 Robert Black
 * Copyright (c) 1994-1995 Melvyn Tang-Richardson
 * Copyright (c) 1994-1995 RiscBSD kernel team
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the RiscBSD kernel team
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE RISCBSD TEAM ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NetBSD kernel project
 *
 * vidcvideo.c
 *
 * This file is the lower basis of the wscons driver for VIDC based ARM machines.
 * It features the initialisation and all VIDC writing and keeps in internal state
 * copy.
 * Its currenly set up as a library file and not as a device; it could be named
 * vidcvideo0 eventually.
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: vidc20config.c,v 1.2.4.7 2002/10/18 02:35:33 nathanw Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <arm/iomd/vidc.h>
#include <arm/arm32/katelib.h>
#include <machine/bootconfig.h>
#include <machine/intr.h>

#include <sys/systm.h>
#include <sys/device.h>
#include <uvm/uvm_extern.h>

#include <arm/iomd/iomdreg.h>
#include <arm/iomd/iomdvar.h>
#include <arm/iomd/vidc20config.h>


/*
 * A structure containing ALL the information required to restore
 * the VIDC20 to any given state.  ALL vidc transactions should
 * go through these procedures, which record the vidc's state.
 * it may be an idea to set the permissions of the vidc base address
 * so we get a fault, so the fault routine can record the state but
 * I guess that's not really necessary for the time being, since we
 * can make the kernel more secure later on.  Also, it is possible
 * to write a routine to allow 'reading' of the vidc registers.
 */

static struct vidc_state vidc_lookup = {	
	{ 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
	},

	VIDC_PALREG,
	VIDC_BCOL,
	VIDC_CP1 ,
	VIDC_CP2,
	VIDC_CP3,
	VIDC_HCR,
	VIDC_HSWR,
	VIDC_HBSR,
	VIDC_HDSR,
	VIDC_HDER,
	VIDC_HBER,
	VIDC_HCSR,
	VIDC_HIR,
	VIDC_VCR,
	VIDC_VSWR,
	VIDC_VBSR,
	VIDC_VDSR,
	VIDC_VDER,
	VIDC_VBER,
	VIDC_VCSR,
	VIDC_VCER,
	VIDC_EREG,
	VIDC_FSYNREG,
	VIDC_CONREG,
	VIDC_DCTL
};

struct vidc_state vidc_current[1];


/*
 * XXX global display variables XXX ... should be a structure
 */
static int cold_init = 0;		/* flags initialisation */
extern videomemory_t videomemory;

static struct vidc_mode  vidc_initialmode;
static struct vidc_mode *vidc_currentmode;

unsigned int dispstart;
unsigned int dispsize;
unsigned int dispbase;
unsigned int dispend;
unsigned int ptov;
unsigned int vmem_base;
unsigned int phys_base;
unsigned int transfersize;


/* cursor stuff */
char *cursor_normal;
char *cursor_transparent;
int   p_cursor_normal;
int   p_cursor_transparent;
int   cursor_width;
int   cursor_height;


/*
 * VIDC mode definitions
 * generated from RISC OS mode definition file by an `awk' script
 */
extern struct vidc_mode vidcmodes[];


/*
 * configuration printing
 *
 */

void
vidcvideo_printdetails(void)
{
        printf(": refclk=%dMHz %dKB %s ", (vidc_fref / 1000000),
            videomemory.vidm_size / 1024,
            (videomemory.vidm_type == VIDEOMEM_TYPE_VRAM) ? "VRAM" : "DRAM");
}


/*
 * Common functions to directly access VIDC registers
 */
int
vidcvideo_write(reg, value)
	u_int reg;
	int value;
{
	int counter;

	int *current;
	int *tab;

	tab 	= (int *)&vidc_lookup;
	current = (int *)vidc_current; 


	/*
	 * OK, the VIDC_PALETTE register is handled differently
	 * to the others on the VIDC, so take that into account here
	 */
	if (reg==VIDC_PALREG) {
		vidc_current->palreg = 0;
		WriteWord(vidc_base, reg | value);
		return 0;
	}

	if (reg==VIDC_PALETTE) {
		WriteWord(vidc_base, reg | value);
		vidc_current->palette[vidc_current->palreg] = value;
		vidc_current->palreg++;
		vidc_current->palreg = vidc_current->palreg & 0xff;
		return 0;
	}

	/*
	 * Undefine SAFER if you wish to speed things up (a little)
	 * although this means the function will assume things abou
	 * the structure of vidc_state. i.e. the first 256 words are
	 * the palette array
	 */

#define SAFER

#ifdef 	SAFER
#define INITVALUE 0
#else
#define INITVALUE 256
#endif

	for ( counter=INITVALUE; counter<= sizeof(struct vidc_state); counter++ ) {
		if ( reg==tab[counter] ) {
			WriteWord ( vidc_base, reg | value );
			current[counter] = value;
			return 0;
		}
	}
	return -1;
}


void
vidcvideo_setpalette(vidc)
	struct vidc_state *vidc;
{
	int counter = 0;

	vidcvideo_write(VIDC_PALREG, 0x00000000);
	for (counter = 0; counter <= 255; counter++)
		vidcvideo_write(VIDC_PALETTE, vidc->palette[counter]);
}


void
vidcvideo_setstate(vidc)
	struct vidc_state *vidc;
{
	vidcvideo_write ( VIDC_PALREG,		vidc->palreg 	);
	vidcvideo_write ( VIDC_BCOL,		vidc->bcol	);
	vidcvideo_write ( VIDC_CP1,		vidc->cp1	);
	vidcvideo_write ( VIDC_CP2,		vidc->cp2	);
	vidcvideo_write ( VIDC_CP3,		vidc->cp3	);
	vidcvideo_write ( VIDC_HCR,		vidc->hcr	);
	vidcvideo_write ( VIDC_HSWR,		vidc->hswr	);
	vidcvideo_write ( VIDC_HBSR,		vidc->hbsr	);
	vidcvideo_write ( VIDC_HDSR,		vidc->hdsr	);
	vidcvideo_write ( VIDC_HDER,		vidc->hder	);
	vidcvideo_write ( VIDC_HBER,		vidc->hber	);
	vidcvideo_write ( VIDC_HCSR,		vidc->hcsr	);
	vidcvideo_write ( VIDC_HIR,		vidc->hir	);
	vidcvideo_write ( VIDC_VCR,		vidc->vcr	);
	vidcvideo_write ( VIDC_VSWR,		vidc->vswr	);
	vidcvideo_write ( VIDC_VBSR,		vidc->vbsr	);
	vidcvideo_write ( VIDC_VDSR,		vidc->vdsr	);
	vidcvideo_write ( VIDC_VDER,		vidc->vder	);
	vidcvideo_write ( VIDC_VBER,		vidc->vber	);
	vidcvideo_write ( VIDC_VCSR,		vidc->vcsr	);
	vidcvideo_write ( VIDC_VCER,		vidc->vcer	);
/*
 * Right, dunno what to set these to yet, but let's keep RiscOS's
 * ones for now, until the time is right to finish this code
 */	

/*	vidcvideo_write ( VIDC_EREG,		vidc->ereg	);	*/
/*	vidcvideo_write ( VIDC_FSYNREG,	vidc->fsynreg	);	*/
/*	vidcvideo_write ( VIDC_CONREG,	vidc->conreg	);	*/
/*	vidcvideo_write ( VIDC_DCTL,		vidc->dctl	);	*/

	vidcvideo_setpalette(vidc);
}


void
vidcvideo_getstate(vidc)
	struct vidc_state *vidc;
{
	*vidc = *vidc_current;
}


void
vidcvideo_getmode(mode)
	struct vidc_mode *mode;
{
	*mode = *vidc_currentmode;
}


static int
vidcvideo_coldinit(void)
{
	int found;
	int loop;

	/* Blank out the cursor */

	vidcvideo_write(VIDC_CP1, 0x0);
	vidcvideo_write(VIDC_CP2, 0x0);
	vidcvideo_write(VIDC_CP3, 0x0);

	/* Try to determine the current mode */
	vidc_initialmode.hder     = bootconfig.width+1;
	vidc_initialmode.vder     = bootconfig.height+1;
	vidc_initialmode.log2_bpp = bootconfig.log2_bpp;

	dispbase = vmem_base = dispstart = videomemory.vidm_vbase;
	phys_base = videomemory.vidm_pbase;

	/* Nut - should be using videomemory.vidm_size - mark */
	if (videomemory.vidm_type == VIDEOMEM_TYPE_DRAM) {
		dispsize = videomemory.vidm_size;
		transfersize = 16;
	} else {
		dispsize = bootconfig.vram[0].pages * NBPG;
		transfersize = dispsize >> 10;
	};
    
	ptov = dispbase - phys_base;

	dispend = dispstart+dispsize;

	/* try to find the current mode from the bootloader in my table */ 
	vidc_currentmode = &vidcmodes[0];
	loop = 0;
	found = 0;
	while (vidcmodes[loop].pixel_rate != 0) {
  		if (vidcmodes[loop].hder == (bootconfig.width + 1)
  		    && vidcmodes[loop].vder == (bootconfig.height + 1)
		    && vidcmodes[loop].frame_rate == bootconfig.framerate) {
			vidc_currentmode = &vidcmodes[loop];
			found = 1;
		}
		++loop;
	}

	/* if not found choose first mode but dont be picky on the framerate */
	if (!found) {
		vidc_currentmode = &vidcmodes[0];
		loop = 0;
		found = 0;

		while (vidcmodes[loop].pixel_rate != 0) {
			if (vidcmodes[loop].hder == (bootconfig.width + 1)
 			    && vidcmodes[loop].vder == (bootconfig.height + 1)) {
 				vidc_currentmode = &vidcmodes[loop];
 				found = 1;
 			}
 			++loop;
 		}
	}

	vidc_currentmode->log2_bpp = bootconfig.log2_bpp;

	dispstart = dispbase;
	dispend = dispstart+dispsize;

	IOMD_WRITE_WORD(IOMD_VIDINIT, dispstart-ptov);
	IOMD_WRITE_WORD(IOMD_VIDSTART, dispstart-ptov);
	IOMD_WRITE_WORD(IOMD_VIDEND, (dispend-transfersize)-ptov);
	return 0;
}


/* simple function to abstract vidc variables ; returns virt start address of screen */
/* XXX asumption that video memory is mapped in twice */
void *vidcvideo_hwscroll(int bytes) {
	dispstart += bytes;
	if (dispstart >= dispbase + dispsize) dispstart -= dispsize;
	if (dispstart <  dispbase)            dispstart += dispsize;
	dispend = dispstart+dispsize;

	/* return the start of the bit map of the screen (left top) */
	return (void *) dispstart;
}


/* reset the HW scroll to be at the start for the benefit of f.e. X */
void *vidcvideo_hwscroll_reset(void) {
	void *cookie = (void *) dispstart;

	dispstart = dispbase;
	dispend = dispstart + dispsize;
	return cookie;
}


/* put HW scroll back to where it was */
void *vidcvideo_hwscroll_back(void *cookie) {
	dispstart = (int) cookie;
	dispend = dispstart + dispsize;
	return cookie;
}


/* this function is to be called perferably at vsync */
void vidcvideo_progr_scroll(void) {
	IOMD_WRITE_WORD(IOMD_VIDINIT, dispstart-ptov);
	IOMD_WRITE_WORD(IOMD_VIDSTART, dispstart-ptov);
	IOMD_WRITE_WORD(IOMD_VIDEND, (dispend-transfersize)-ptov);
}


/*
 * Select a new mode by reprogramming the VIDC chip
 * XXX this part is known not to work for 32bpp
 */

struct vidc_mode newmode;

static const int bpp_mask_table[] = {
	0,  /* 1bpp */
	1,  /* 2bpp */
	2,  /* 4bpp */
	3,  /* 8bpp */
	4,  /* 16bpp */
	6   /* 32bpp */
};


void
vidcvideo_setmode(struct vidc_mode *mode)
{    
	register int acc;
	int bpp_mask;
        int ereg;
	int best_r, best_v;
	int least_error;
	int r, v, f;

	/*
	 * Find out what bit mask we need to or with the vidc20 control register
	 * in order to generate the desired number of bits per pixel.
	 * log_bpp is log base 2 of the number of bits per pixel.
	 */

	bpp_mask = bpp_mask_table[mode->log2_bpp];

	newmode = *mode;
	vidc_currentmode = &newmode;

	least_error = INT_MAX;
	best_r = 0; best_v = 0;

	for (v = 63; v > 0; v--) {
		for (r = 63; r > 0; r--) {
			f = ((v * vidc_fref) /1000) / r;
			if (least_error >=
			    abs(f - vidc_currentmode->pixel_rate)) {
				least_error = 
				    abs(f - vidc_currentmode->pixel_rate);
				best_r = r;
				best_v = v;
			}
		}
	}

	if (best_r > 63) best_r=63;
	if (best_v > 63) best_v=63;
	if (best_r < 1)  best_r= 1;
	if (best_v < 1)  best_v= 1;

	vidcvideo_write(VIDC_FSYNREG, (best_v-1)<<8 | (best_r-1)<<0);

	acc=0;
	acc+=vidc_currentmode->hswr;	vidcvideo_write(VIDC_HSWR, (acc - 8 ) & (~1));
	acc+=vidc_currentmode->hbsr;	vidcvideo_write(VIDC_HBSR, (acc - 12) & (~1));
	acc+=vidc_currentmode->hdsr;	vidcvideo_write(VIDC_HDSR, (acc - 18) & (~1));
	acc+=vidc_currentmode->hder;	vidcvideo_write(VIDC_HDER, (acc - 18) & (~1));
	acc+=vidc_currentmode->hber;	vidcvideo_write(VIDC_HBER, (acc - 12) & (~1));
	acc+=vidc_currentmode->hcr;	vidcvideo_write(VIDC_HCR,  (acc - 8 ) & (~3));

	acc=0;
	acc+=vidc_currentmode->vswr;	vidcvideo_write(VIDC_VSWR, (acc - 1));
	acc+=vidc_currentmode->vbsr;	vidcvideo_write(VIDC_VBSR, (acc - 1));
	acc+=vidc_currentmode->vdsr;	vidcvideo_write(VIDC_VDSR, (acc - 1));
	acc+=vidc_currentmode->vder;	vidcvideo_write(VIDC_VDER, (acc - 1));
	acc+=vidc_currentmode->vber;	vidcvideo_write(VIDC_VBER, (acc - 1));
	acc+=vidc_currentmode->vcr;	vidcvideo_write(VIDC_VCR,  (acc - 1));

	IOMD_WRITE_WORD(IOMD_FSIZE, vidc_currentmode->vcr
	    + vidc_currentmode->vswr
	    + vidc_currentmode->vber
	    + vidc_currentmode->vbsr - 1);

	if (dispsize <= 1024*1024)
		vidcvideo_write(VIDC_DCTL, vidc_currentmode->hder>>2 | 1<<16 | 1<<12);
	else
		vidcvideo_write(VIDC_DCTL, vidc_currentmode->hder>>2 | 3<<16 | 1<<12);

	ereg = 1<<12;
	if (vidc_currentmode->sync_pol & 0x01)
		ereg |= 1<<16;
	if (vidc_currentmode->sync_pol & 0x02)
		ereg |= 1<<18;
	vidcvideo_write(VIDC_EREG, ereg);
	if (dispsize > 1024*1024) {
		if (vidc_currentmode->hder >= 800)
 			vidcvideo_write(VIDC_CONREG, 7<<8 | bpp_mask<<5);
		else
			vidcvideo_write(VIDC_CONREG, 6<<8 | bpp_mask<<5);
	} else {
		vidcvideo_write(VIDC_CONREG, 7<<8 | bpp_mask<<5);
	}
}


#if 0
/* not used for now */
void
vidcvideo_set_display_base(base)
	u_int base;
{
	dispstart = dispstart-dispbase + base;
	dispbase = vmem_base = base;
	dispend = base + dispsize;
	ptov = dispbase - phys_base;
}
#endif


/*
 * Main initialisation routine for now
 */

static int cursor_init = 0;

int
vidcvideo_init(void)
{
	vidcvideo_coldinit();
	if (cold_init && (cursor_init == 0)) 
		/*	vidcvideo_flash_go() */;

	/* setting a mode goes wrong in 32 bpp ... 8 and 16 seem OK */
	vidcvideo_setmode(vidc_currentmode);
	vidcvideo_blank(0);			/* display on */

	vidcvideo_stdpalette();

	if (cold_init == 0) {
		vidcvideo_write(VIDC_CP1, 0x0);
		vidcvideo_write(VIDC_CP2, 0x0);
		vidcvideo_write(VIDC_CP3, 0x0);
	} else {
		vidcvideo_cursor_init(CURSOR_MAX_WIDTH, CURSOR_MAX_HEIGHT);
	};

	cold_init=1;
	return 0;
}


/* reinitialise the vidcvideo */
void
vidcvideo_reinit()
{
	vidcvideo_coldinit();
	vidcvideo_setmode(vidc_currentmode);
}


int
vidcvideo_cursor_init(int width, int height)
{
	static char *cursor_data = NULL;
	int counter;
	int line;
	paddr_t pa;

	cursor_width  = width;
	cursor_height = height;

	if (!cursor_data) {
		/* Allocate cursor memory first time round */
		cursor_data = (char *)uvm_km_zalloc(kernel_map, NBPG);
		if (!cursor_data)
			panic("Cannot allocate memory for hardware cursor");
		(void) pmap_extract(pmap_kernel(), (vaddr_t)cursor_data, &pa);
		IOMD_WRITE_WORD(IOMD_CURSINIT, pa);
	}

	/* Blank the cursor while initialising it's sprite */

	vidcvideo_write ( VIDC_CP1, 0x0 );
	vidcvideo_write ( VIDC_CP2, 0x0 );
	vidcvideo_write ( VIDC_CP3, 0x0 );

 	cursor_normal       = cursor_data;
	cursor_transparent  = cursor_data + (height * width);

 	cursor_transparent += 32;					/* ALIGN */
	cursor_transparent = (char *)((int)cursor_transparent & (~31) );

	for ( line = 0; line<height; ++ line )
	{
	    for ( counter=0; counter<width/4;counter++ )
		cursor_normal[line * width + counter]=0x55;		/* why 0x55 ? */
	    for ( ; counter<8; counter++ )
		cursor_normal[line * width + counter]=0;
	}

	for ( line = 0; line<height; ++ line )
	{
	    for ( counter=0; counter<width/4;counter++ )
		cursor_transparent[line * width + counter]=0x00;
	    for ( ; counter<8; counter++ )
		cursor_transparent[line * width + counter]=0;
	}


	(void) pmap_extract(pmap_kernel(), (vaddr_t)cursor_normal,
	    (paddr_t *)&p_cursor_normal);
	(void) pmap_extract(pmap_kernel(), (vaddr_t)cursor_transparent,
	    (paddr_t *)&p_cursor_transparent);

	memset ( cursor_normal, 0x55, width*height );			/* white? */
	memset ( cursor_transparent, 0x00, width*height );		/* to see the diffence */

	/* Ok, now program the cursor; should be blank */
	vidcvideo_enablecursor(0);

        return 0;
}


void
vidcvideo_updatecursor(xcur, ycur)
	int xcur, ycur;
{
	int frontporch = vidc_currentmode->hswr + vidc_currentmode->hbsr + vidc_currentmode->hdsr;
	int topporch   = vidc_currentmode->vswr + vidc_currentmode->vbsr + vidc_currentmode->vdsr;

	vidcvideo_write(VIDC_HCSR, frontporch -17 + xcur);
	vidcvideo_write(VIDC_VCSR, topporch   -2  + (ycur+1)-2 + 3 - cursor_height);
	vidcvideo_write(VIDC_VCER, topporch   -2  + (ycur+3)+2 + 3 );
	return;
}


void
vidcvideo_enablecursor(on)
	int on;
{
	if (on) {
		IOMD_WRITE_WORD(IOMD_CURSINIT,p_cursor_normal);
	} else {
		IOMD_WRITE_WORD(IOMD_CURSINIT,p_cursor_transparent);
	};
	vidcvideo_write ( VIDC_CP1, 0xffffff );		/* enable */

	return;
}


void
vidcvideo_stdpalette()
{
	int i;

	switch (vidc_currentmode->log2_bpp) {
	case 0: /* 1 bpp */
	case 1: /* 2 bpp */
	case 2: /* 4 bpp */
	case 3: /* 8 bpp */
		vidcvideo_write(VIDC_PALREG, 0x00000000);
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));  
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(255,   0,   0));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(  0, 255,   0));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(255, 255,   0));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(  0,   0, 255));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(255,   0, 255));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(  0, 255, 255));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(255, 255, 255));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(128, 128, 128));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(255, 128, 128));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(128, 255, 128));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(255, 255, 128));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(128, 128, 255));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(255, 128, 255));
		vidcvideo_write(VIDC_PALETTE, VIDC_COL(255, 255, 255));
		break;
	case 4: /* 16 bpp */
		/*
		 * The use of the palette in 16-bit modes is quite
		 * fun.  Comments in linux/drivers/video/acornfb.c
		 * imply that it goes something like this:
		 *
		 * red   = LUT[pixel[7:0]].red
		 * green = LUT[pixel[11:4]].green
		 * blue  = LUT[pixel[15:8]].blue
		 *
		 * We use 6:5:5 R:G:B cos that's what Xarm32VIDC wants.
		 */
#define RBITS 6
#define GBITS 5
#define BBITS 5
		vidcvideo_write(VIDC_PALREG, 0x00000000);
		for (i = 0; i < 256; i++) {
			int r, g, b;

			r = i & ((1 << RBITS) - 1);
			g = (i >> (RBITS - 4)) & ((1 << GBITS) - 1);
			b = (i >> (RBITS + GBITS - 8)) & ((1 << BBITS) - 1);
			vidcvideo_write(VIDC_PALETTE,
			    VIDC_COL(r << (8 - RBITS) | r >> (2 * RBITS - 8),
				g << (8 - GBITS) | g >> (2 * GBITS - 8),
				b << (8 - BBITS) | b >> (2 * BBITS - 8)));
		}
		break;			
	case 5: /* 32 bpp */
		vidcvideo_write(VIDC_PALREG, 0x00000000);
		for (i = 0; i < 256; i++)
			vidcvideo_write(VIDC_PALETTE, VIDC_COL(i, i, i));  
		break;
	}
			
}

int
vidcvideo_blank(video_off)
	int video_off;
{
        int ereg;

	ereg = 1<<12;
	if (vidc_currentmode->sync_pol & 0x01)
		ereg |= 1<<16;
	if (vidc_currentmode->sync_pol & 0x02)
		ereg |= 1<<18;

	if (!video_off) {
    		vidcvideo_write(VIDC_EREG, ereg);
	} else {
		vidcvideo_write(VIDC_EREG, 0);
	};
	return 0;
}

/* end of vidc20config.c */
