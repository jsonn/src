/*	$NetBSD: cgfourteenreg.h,v 1.1.78.2 2007/09/03 14:29:55 yamt Exp $ */

/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
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
 *	This product includes software developed by Harvard University and
 *	its contributors.
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
 */

/*
 * Register/dac/clut/cursor definitions for cgfourteen frame buffer
 */

/* Locations of control registers in cg14 register set */
#define	CG14_OFFSET_CURS	0x1000
#define CG14_OFFSET_DAC		0x2000
#define CG14_OFFSET_XLUT	0x3000
#define CG14_OFFSET_CLUT1	0x4000
#define CG14_OFFSET_CLUT2	0x5000
#define CG14_OFFSET_CLUT3	0x6000
#define CG14_OFFSET_CLUTINCR	0xf000

/* cursor registers */
#define CG14_CURSOR_PLANE0	0x1000
#define CG14_CURSOR_PLANE1	0x1080
#define CG14_CURSOR_CONTROL	0x1100
	#define CG14_CRSR_ENABLE	0x04
	#define CG14_CRSR_DBLBUFFER	0x02
#define CG14_CURSOR_X		0x1104
#define CG14_CURSOR_Y		0x1106
#define CG14_CURSOR_COLOR1	0x1108
#define CG14_CURSOR_COLOR2	0x110c
#define CG14_CURSOR_COLOR3	0x1110

/* ranges in framebuffer space */
#define CG14_FB_VRAM		0x00000000
#define CG14_FB_CBGR		0x01000000
#define CG14_FB_PX32		0x03000000
#define CG14_FB_PG32		0x03800000

/* Main control register set */
struct cg14ctl {
	volatile uint8_t	ctl_mctl;	/* main control register */
#define CG14_MCTL	0x00000000
#define CG14_MCTL_ENABLEINTR	0x80		/* interrupts */
#define CG14_MCTL_ENABLEVID	0x40		/* enable video */
#define CG14_MCTL_PIXMODE_MASK	0x30
#define		CG14_MCTL_PIXMODE_8	0x00	/* data is 16 8-bit pixels */
#define		CG14_MCTL_PIXMODE_16	0x20	/* data is 8 16-bit pixels */
#define		CG14_MCTL_PIXMODE_32	0x30	/* data is 4 32-bit pixels */
#define CG14_MCTL_PIXMODE_SHIFT	4
#define	CG14_MCTL_TMR		0x0c
#define CG14_MCTL_ENABLETMR	0x02
#define CG14_MCTL_rev0RESET	0x01
#define CG14_MCTL_POWERCTL	0x01

	volatile uint8_t	ctl_ppr;	/* packed pixel register */
	volatile uint8_t	ctl_tmsr0; 	/* test status reg. 0 */
	volatile uint8_t	ctl_tmsr1;	/* test status reg. 1 */
	volatile uint8_t	ctl_msr;	/* master status register */
	volatile uint8_t	ctl_fsr;	/* fault status register */
	volatile uint8_t	ctl_rsr;	/* revision status register */
#define CG14_RSR_REVMASK	0xf0 		/*  mask to get revision */
#define CG14_RSR_IMPLMASK	0x0f		/*  mask to get impl. code */
	volatile uint8_t	ctl_ccr;	/* clock control register */
	/* XXX etc. */
};

/* Hardware cursor map */
#define CG14_CURS_SIZE		32
struct cg14curs {
	volatile uint32_t	curs_plane0[CG14_CURS_SIZE];	/* plane 0 */
	volatile uint32_t	curs_plane1[CG14_CURS_SIZE];
	volatile uint8_t	curs_ctl;	/* control register */
#define CG14_CURS_ENABLE	0x4
#define CG14_CURS_DOUBLEBUFFER	0x2 		/* use X-channel for curs */
	volatile uint8_t	pad0[3];
	volatile uint16_t	curs_x;		/* x position */
	volatile uint16_t	curs_y;		/* y position */
	volatile uint32_t	curs_color1;	/* color register 1 */
	volatile uint32_t	curs_color2;	/* color register 2 */
	volatile uint32_t	pad[444];	/* pad to 2KB boundary */
	volatile uint32_t	curs_plane0incr[CG14_CURS_SIZE]; /* autoincr */
	volatile uint32_t	curs_plane1incr[CG14_CURS_SIZE]; /* autoincr */
};

/* DAC */
struct cg14dac {
	volatile uint8_t	dac_addr;	/* address register */
	volatile uint8_t	pad0[255];
	volatile uint8_t	dac_gammalut;	/* gamma LUT */
	volatile uint8_t	pad1[255];
	volatile uint8_t	dac_regsel;	/* register select */
	volatile uint8_t	pad2[255];
	volatile uint8_t	dac_mode;	/* mode register */
};

#define CG14_CLUT_SIZE	256

/* XLUT registers */
struct cg14xlut {
	volatile uint8_t	xlut_lut[CG14_CLUT_SIZE];	/* the LUT */
	volatile uint8_t	xlut_lutd[CG14_CLUT_SIZE];	/* ??? */
	volatile uint8_t	pad0[0x600];
	volatile uint8_t	xlut_lutinc[CG14_CLUT_SIZE];	/* autoincrLUT*/
	volatile uint8_t	xlut_lutincd[CG14_CLUT_SIZE];
};

/* Color Look-Up Table (CLUT) */
struct cg14clut {
	volatile uint32_t	clut_lut[CG14_CLUT_SIZE];	/* the LUT */
	volatile uint32_t	clut_lutd[CG14_CLUT_SIZE];	/* ??? */
	volatile uint32_t	clut_lutinc[CG14_CLUT_SIZE];	/* autoincr */
	volatile uint32_t	clut_lutincd[CG14_CLUT_SIZE];
};
