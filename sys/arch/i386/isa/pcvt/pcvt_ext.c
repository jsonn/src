/*
 * Copyright (c) 1993, 1994 Hellmuth Michaelis and Joerg Wunsch
 *
 * Copyright (C) 1992, 1993 Soeren Schmidt.
 *
 * All rights reserved.
 *
 * For the sake of compatibility, portions of this code regarding the
 * X server interface are taken from Soeren Schmidt's syscons driver.
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
 *	This product includes software developed by
 *	Hellmuth Michaelis, Joerg Wunsch and Soeren Schmidt.
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * @(#)pcvt_ext.c, 3.00, Last Edit-Date: [Sun Feb 27 17:04:52 1994]
 *
 */

/*---------------------------------------------------------------------------*
 *
 *	pcvt_ext.c	VT220 Driver Extended Support Routines
 *	------------------------------------------------------
 *
 *	written by Hellmuth Michaelis, hm@hcshh.hcs.de       and
 *	           Joerg Wunsch, joerg_wunsch@uriah.sax.de
 *
 *	-hm	splitting pcvt_sup.c
 *	-hm	adding 132 column support for S3 80c928 chipset
 *	-hm	support for keyboard scancode sets 1 and 2
 *	-hm	132 col support for Cirrus 542x from Onno van der Linden
 *	-jw/hm	all ifdef's converted to if's
 *	-hm	applied patch from Szabolcs Szigeti for TVGA 8900B and
 *		TVGA8900C to make the operating with 132 columns
 *	-hm	------------ Release 3.00 --------------
 *
 *---------------------------------------------------------------------------*/

#include "vt.h"
#if NVT > 0

#include "pcvt_hdr.h"		/* global include */

static int  s3testwritable( void );

/* storage to save video timing values of 80 columns text mode */
static union {
	u_char generic[11];
	u_char et4000[11];
	u_char wd90c11[12];
	u_char tri9000[13];
	u_char v7_1024i[17];
	u_char s3_928[32];	
	u_char cirrus[13];
}
savearea;

static int regsaved = 0;	/* registers are saved to savearea */

/*---------------------------------------------------------------------------*
 *
 *	Find out which video board we are running on, taken from:
 *	Richard Ferraro: Programmers Guide to the EGA and VGA Cards
 *	and from David E. Wexelblat's SuperProbe Version 1.0.
 *	When a board is found, for which 132 column switching is
 *	provided, the global variable "can_do_132col" is set to 1,
 *	also the global variable vga_family is set to what we found.
 *
 *	###############################################################
 *	## THIS IS GETTING MORE AND MORE A LARGE SPAGHETTI HACK !!!! ##
 *	###############################################################
 *
 *---------------------------------------------------------------------------*/
u_char
vga_chipset(void)
{
	u_char *ptr;
	u_char byte, oldbyte, old1byte, newbyte;

#if PCVT_132GENERIC
	can_do_132col = 1;	/* assumes everyone can do 132 col */
#else
	can_do_132col = 0;	/* assumes noone can do 132 col */
#endif /* PCVT_132GENERIC */

	vga_family = VGA_F_NONE;

/*---------------------------------------------------------------------------*
 * 	check for Western Digital / Paradise chipsets
 *---------------------------------------------------------------------------*/
	
	ptr = (u_char *)Crtat;

	if(color)
		ptr += (0xc007d - 0xb8000);
	else
		ptr += (0xc007d - 0xb0000);	

	if((*ptr++ == 'V') && (*ptr++ == 'G') &&
	   (*ptr++ == 'A') && (*ptr++ == '='))
	{
		int wd90c10;
		
		vga_family = VGA_F_WD;

		outb(addr_6845, 0x2b);
		oldbyte = inb(addr_6845+1);
		outb(addr_6845+1, 0xaa);
		newbyte = inb(addr_6845+1);
		outb(addr_6845+1, oldbyte);
		if(newbyte != 0xaa)
			return(VGA_PVGA);	/* PVGA1A chip */

		outb(TS_INDEX, 0x12);
		oldbyte = inb(TS_DATA);
		outb(TS_DATA, oldbyte & 0xbf);
		newbyte = inb(TS_DATA) & 0x40;
		if(newbyte != 0)
			return(VGA_WD90C00);	/* WD90C00 chip */
		
		outb(TS_DATA, oldbyte | 0x40);
		newbyte = inb(TS_DATA) & 0x40;
		if(newbyte == 0)
			return(VGA_WD90C00);	/* WD90C00 chip */

		outb(TS_DATA, oldbyte);

		wd90c10 = 0;
		outb(TS_INDEX, 0x10);
		oldbyte = inb(TS_DATA);

		outb(TS_DATA, oldbyte & 0xfb);
		newbyte = inb(TS_DATA) & 0x04;
		if(newbyte != 0)
			wd90c10 = 1;

		outb(TS_DATA, oldbyte | 0x04);
		newbyte = inb(TS_DATA) & 0x04;
		if(newbyte == 0)
			wd90c10 = 1;

		outb(TS_DATA, oldbyte);

		if(wd90c10)
			return(VGA_WD90C10);
		else
		{
			can_do_132col = 1;
			return(VGA_WD90C11);
		}
	}

/*---------------------------------------------------------------------------*
 *	check for Trident chipsets
 *---------------------------------------------------------------------------*/

	outb(TS_INDEX, 0x0b);
	oldbyte = inb(TS_DATA);


	outb(TS_INDEX, 0x0b);
	outb(TS_DATA, 0x00);

	byte = inb(TS_DATA);	/* chipset type */

	
	outb(TS_INDEX, 0x0e);
	old1byte = inb(TS_DATA);

	outb(TS_DATA, 0);
	newbyte = inb(TS_DATA);

	outb(TS_DATA, (old1byte ^ 0x02));

	outb(TS_INDEX, 0x0b);
	outb(TS_DATA, oldbyte);

	if((newbyte & 0x0f) == 0x02)
	{
		/* is a trident chip */

		vga_family = VGA_F_TRI;

		switch(byte)
		{
			case 0x01:
				return(VGA_TR8800BR);

			case 0x02:
				return(VGA_TR8800CS);

			case 0x03:
				can_do_132col = 1;
				return(VGA_TR8900B);

			case 0x04:
			case 0x13:
	/* Haven't tried, but should work */
				can_do_132col = 1;
				return(VGA_TR8900C);

			case 0x23:
				can_do_132col = 1;
				return(VGA_TR9000);

			case 0x33:
				return(VGA_TR8900CL);

			case 0x83:
				return(VGA_TR9200);

			case 0x93:
				return(VGA_TR9100);

			default:
				return(VGA_TRUNKNOWN);
		}
	}
	
/*---------------------------------------------------------------------------*
 *	check for Tseng Labs ET3000/4000 chipsets
 *---------------------------------------------------------------------------*/

	outb(GN_HERCOMPAT, 0x06);
	if(color)
		outb(GN_DMCNTLC, 0xa0);
	else
		outb(GN_DMCNTLM, 0xa0);

	/* read old value */	

	if(color)
		inb(GN_INPSTAT1C);
	else
		inb(GN_INPSTAT1M);
	outb(ATC_INDEX, ATC_MISC);
	oldbyte = inb(ATC_DATAR);

	/* write new value */

	if(color)
		inb(GN_INPSTAT1C);
	else
		inb(GN_INPSTAT1M);
	outb(ATC_INDEX, ATC_MISC);
	newbyte = oldbyte ^ 0x10;	
	outb(ATC_DATAW, newbyte);

	/* read back new value */
	if(color)
		inb(GN_INPSTAT1C);
	else
		inb(GN_INPSTAT1M);
	outb(ATC_INDEX, ATC_MISC);
	byte = inb(ATC_DATAR);

	/* write back old value */
	if(color)
		inb(GN_INPSTAT1C);
	else
		inb(GN_INPSTAT1M);
	outb(ATC_INDEX, ATC_MISC);
	outb(ATC_DATAW, oldbyte);	

	if(byte == newbyte)	/* ET3000 or ET4000 */
	{
		vga_family = VGA_F_TSENG;
		
		outb(addr_6845, CRTC_EXTSTART);
		oldbyte = inb(addr_6845+1);
		newbyte = oldbyte ^ 0x0f;
		outb(addr_6845+1, newbyte);
		byte = inb(addr_6845+1);
		outb(addr_6845+1, oldbyte);			

		if(byte == newbyte)
		{
			can_do_132col = 1;		
			return(VGA_ET4000);
		}
		else
		{
			return(VGA_ET3000);
		}
	}

/*---------------------------------------------------------------------------*
 *	check for Video7 VGA chipsets
 *---------------------------------------------------------------------------*/

	outb(TS_INDEX, TS_EXTCNTL);	/* enable extensions */
	outb(TS_DATA, 0xea);	

	outb(addr_6845, CRTC_STARTADRH);
	oldbyte = inb(addr_6845+1);

	outb(addr_6845+1, 0x55);
	newbyte = inb(addr_6845+1);	

	outb(addr_6845, CRTC_V7ID);	/* id register */
	byte = inb(addr_6845+1);	/* read id */
	
	outb(addr_6845, CRTC_STARTADRH);
	outb(addr_6845+1, oldbyte);
	
	outb(TS_INDEX, TS_EXTCNTL);	/* disable extensions */
	outb(TS_DATA, 0xae);	

	if(byte == (0x55 ^ 0xea))
	{					/* is Video 7 */

		vga_family = VGA_F_V7;

		outb(TS_INDEX, TS_EXTCNTL);	/* enable extensions */
		outb(TS_DATA, 0xea);	

		outb(TS_INDEX, TS_V7CHIPREV);
		byte = inb(TS_DATA);

		outb(TS_INDEX, TS_EXTCNTL);	/* disable extensions */
		outb(TS_DATA, 0xae);

		if(byte < 0xff && byte >= 0x80)
			return(VGA_V7VEGA);
		if(byte < 0x7f && byte >= 0x70)
			return(VGA_V7FWVR);
		if(byte < 0x5a && byte >= 0x50)
			return(VGA_V7V5);
		if(byte < 0x4a && byte > 0x40)
		{
			can_do_132col = 1;
			return(VGA_V71024I);
		}
		return(VGA_V7UNKNOWN);
	}

/*---------------------------------------------------------------------------*
 *	check for S3 chipsets
 *---------------------------------------------------------------------------*/

	outb(addr_6845, 0x38);		/* reg 1 lock register */
	old1byte = inb(addr_6845+1);	/* get old value */

	outb(addr_6845, 0x38);
	outb(addr_6845+1, 0x00);	/* lock registers */

	if(s3testwritable() == 0)	/* check if locked */
	{
		outb(addr_6845, 0x38);
		outb(addr_6845+1, 0x48);	/* unlock registers */

		if(s3testwritable() == 1 )	/* check if unlocked */
		{
			vga_family = VGA_F_S3;	/* FAMILY S3  */

			outb(addr_6845, 0x30);	/* chip id/rev reg */
			byte = inb(addr_6845+1);

			switch(byte & 0xf0)
			{
				case 0x80:
					switch(byte & 0x0f)
					{
						case 0x01:
							outb(addr_6845, 0x38);
							outb(addr_6845+1, old1byte);
							return VGA_S3_911;

						case 0x02:
							outb(addr_6845, 0x38);
							outb(addr_6845+1, old1byte);
							return VGA_S3_924;

						default:
							outb(addr_6845, 0x38);
							outb(addr_6845+1, old1byte);
							return VGA_S3_UNKNOWN;
					}
					break;

				case 0xA0:
					outb(addr_6845, 0x38);
					outb(addr_6845+1, old1byte);
					return VGA_S3_80x;

				case 0x90:
				case 0xb0:
					outb(addr_6845, 0x38);
					outb(addr_6845+1, old1byte);
					can_do_132col = 1;
					return VGA_S3_928;

				default:
					outb(addr_6845, 0x38);
					outb(addr_6845+1, old1byte);
					return VGA_S3_UNKNOWN;
			}
		}
	}			

/*---------------------------------------------------------------------------*
 *	check for Cirrus chipsets
 *---------------------------------------------------------------------------*/

	outb(TS_INDEX, 6);
	oldbyte = inb(TS_DATA);
	outb(TS_INDEX, 6);
	outb(TS_DATA, 0x12);
	outb(TS_INDEX, 6);
	newbyte = inb(TS_DATA);
	outb(addr_6845, 0x27);
	byte = inb(addr_6845 + 1);
	outb(TS_INDEX, 6);
	outb(TS_DATA, oldbyte);
	if (newbyte == 0x12) {
		vga_family = VGA_F_CIR;
		can_do_132col = 1;
		switch ((byte & 0xfc) >> 2) {
		case 0x22:
			switch (byte & 3) {
			case 0:
				return VGA_CL_GD5402;
			case 1:
				return VGA_CL_GD5402r1;
			case 2:
				return VGA_CL_GD5420;
			case 3:
				return VGA_CL_GD5420r1;
			}
			break;
		case 0x23:
			return VGA_CL_GD5422;
		case 0x25:
			return VGA_CL_GD5424;
		case 0x24:
			return VGA_CL_GD5426;
		case 0x26:
			return VGA_CL_GD5428;
		}
	}

	return(VGA_UNKNOWN);
}

/*---------------------------------------------------------------------------
 * test if index 35 lower nibble is writable (taken from SuperProbe 1.0)
 *---------------------------------------------------------------------------*/
static int
s3testwritable(void)
{
	u_char old, new1, new2;

	outb(addr_6845, 0x35);
	old = inb(addr_6845+1);			/* save */

	outb(addr_6845, 0x35);
	outb(addr_6845+1, (old & 0xf0));	/* write 0 */

	outb(addr_6845, 0x35);
	new1 = (inb(addr_6845+1)) & 0x0f;	/* must read 0 */

	outb(addr_6845, 0x35);
	outb(addr_6845+1, (old | 0x0f));	/* write 1 */

	outb(addr_6845, 0x35);
	new2 = (inb(addr_6845+1)) & 0x0f;	/* must read 1 */

	outb(addr_6845, 0x35);
	outb(addr_6845+1, old);			/* restore */

	return((new1==0) && (new2==0x0f));
}

/*---------------------------------------------------------------------------*
 *	return ptr to string describing vga type
 *---------------------------------------------------------------------------*/
char *
vga_string(int number)
{
	static char *vga_tab[] = {
		"generic",
		"et4000",
		"et3000",
		"pvga1a",
		"wd90c00",
		"wd90c10",
		"wd90c11",
		"v7 vega",
		"v7 fast",
		"v7 ver5",
		"v7 1024i",
		"unknown v7",
		"tvga 8800br",
		"tvga 8800cs",
		"tvga 8900b",
		"tvga 8900c",
		"tvga 8900cl",
		"tvga 9000",
		"tvga 9100",
		"tvga 9200",
		"unknown trident",
		"s3 911",
		"s3 924",
		"s3 801/805",
		"s3 928",
		"unkown s3",
		"cl-gd5402",
		"cl-gd5402r1",
		"cl-gd5420",
		"cl-gd5420r1",
		"cl-gd5422",
		"cl-gd5424",
		"cl-gd5426",
		"cl-gd5428"
	};
	return(vga_tab[number]);
}

/*---------------------------------------------------------------------------*
 *	toggle vga 80/132 column operation
 *---------------------------------------------------------------------------*/
int
vga_col(struct video_state *svsp, int cols)
{
	int ret = 0;
	
	if(adaptor_type != VGA_ADAPTOR)
		return(0);
		
	switch(vga_type)
	{
		case VGA_ET4000:
			ret = et4000_col(cols);
			break;

		case VGA_WD90C11:
			ret = wd90c11_col(cols);
			break;
		case VGA_TR8900B:
		case VGA_TR8900C:
		case VGA_TR9000:
			ret = tri9000_col(cols);
			break;
		
		case VGA_V71024I:
			ret = v7_1024i_col(cols);
			break;
			
		case VGA_S3_928:
			ret = s3_928_col(cols);
			break;
			
		case VGA_CL_GD5402:
		case VGA_CL_GD5402r1:
		case VGA_CL_GD5420:
		case VGA_CL_GD5420r1:
		case VGA_CL_GD5422:
		case VGA_CL_GD5424:
		case VGA_CL_GD5426:
		case VGA_CL_GD5428:
			ret = cl_gd542x_col(cols);
			break;

		default:

#if PCVT_132GENERIC
			ret = generic_col(cols);
#endif /* PCVT_132GENERIC */
	
			break;
	}

	if(ret == 0)
		return(0);	/* failed */

	svsp->maxcol = cols;

	return(1);
}

#if PCVT_132GENERIC
/*---------------------------------------------------------------------------*
 *	toggle 80/132 column operation for "generic" SVGAs
 *	NB: this is supposed to work on any (S)VGA as long as the monitor
 *	is able to sync down to 21.5 kHz horizontally. The resulting
 *	vertical frequency is only 50 Hz, so if there is some better board
 *	specific algorithm, we avoid using this generic one.
 *	REPORT ANY FAILURES SO WE CAN IMPROVE THIS
 *---------------------------------------------------------------------------*/

#if PCVT_EXP_132COL
/*
 *	Some improved (i.e. higher scan rates) figures for the horizontal
 *	timing. USE AT YOUR OWN RISK, THIS MIGHT DAMAGE YOUR MONITOR DUE
 *	TO A LOSS OF HORIZONTAL SYNC!
 *	The figures have been tested with an ET3000 board along with a
 *	NEC MultiSync 3D monitor. If you are playing here, consider
 *	testing with several screen pictures (dark background vs. light
 *	background, even enlightening the border color may impact the
 *	result - you can do this e.g. by "scon -p black,42,42,42")
 *	Remember that all horizontal timing values must be dividable
 *	by 8! (The scheme below is taken so that nifty kernel hackers
 *	are able to patch the figures at run-time.)
 *
 *	The actual numbers result in 23 kHz line scan and 54 Hz vertical
 *	scan.
 */
#endif /* PCVT_EXP_132COL */

int
generic_col(int cols)
{
	u_char *sp;
	u_char byte;

#if !PCVT_EXP_132COL

	/* stable figures for any multisync monitor that syncs down to 22 kHz*/
	static volatile u_short htotal = 1312;
	static volatile u_short displayend = 1056;
	static volatile u_short blankstart = 1072;
	static volatile u_short syncstart = 1112;
	static volatile u_short syncend = 1280;
	
#else /* PCVT_EXP_132COL */

	/* reduced sync-pulse width and sync delays */
	static volatile u_short htotal = 1232;
	static volatile u_short displayend = 1056;
	static volatile u_short blankstart = 1056;
	static volatile u_short syncstart = 1104;
	static volatile u_short syncend = 1168;
	
#endif /* PCVT_EXP_132COL */

	vga_screen_off();
	
	/* enable access to first 7 CRTC registers */

	outb(addr_6845, CRTC_VSYNCE);			
	byte = inb(addr_6845+1);
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte & 0x7f);
	
	if(cols == SCR_COL132)		/* switch 80 -> 132 */
	{
		/* save state of board for 80 columns */
		
		if(!regsaved)
		{
			regsaved = 1;

			sp = savearea.generic;
		
			outb(addr_6845, 0x00);	/* Horizontal Total */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x01);	/* Horizontal Display End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x02);	/* Horizontal Blank Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x03);	/* Horizontal Blank End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x05);	/* Horizontal Retrace End */
			*sp++ = inb(addr_6845+1);
		
			outb(addr_6845, 0x13);	/* Row Offset Register */
			*sp++ = inb(addr_6845+1);
		
			outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
			*sp++ = inb(TS_DATA);
		
			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Mode control */
			outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);
		
			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Horizontal Pixel Panning */
			outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);

			*sp++ = inb(GN_MISCOUTR); /* Misc output register */
		}

		/* setup chipset for 132 column operation */


		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, (htotal / 8) - 5);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, (displayend / 8) - 1);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, blankstart / 8);
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, ((syncend / 8) & 0x1f) | 0x80);
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, syncstart / 8);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1,
		     (((syncend / 8) & 0x20) * 4)
		     | ((syncend / 8) & 0x1f));
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, 0x42);
	
		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, 0x01);	/* 8 dot char clock */
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); /* ATC Mode control */
		outb(ATC_DATAW, 0x08);	/* Line graphics disable */
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); /* ATC Horizontal Pixel Panning */
		outb(ATC_DATAW, 0x00);

		/* Misc output register */
		/* use the 28.322 MHz clock */
		outb(GN_MISCOUTW, (inb(GN_MISCOUTR) & ~0x0c) | 4);
	}
	else	/* switch 132 -> 80 */
	{
		if(!regsaved)			/* failsafe */
		{
			/* disable access to first 7 CRTC registers */
			outb(addr_6845, CRTC_VSYNCE);
			outb(addr_6845+1, byte);
			vga_screen_on();
			return(0);
		}
	
		sp = savearea.generic;
		
		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, *sp++);		
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, *sp++);		
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, *sp++);
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, *sp++);
	
		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, *sp++);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Mode control */
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Horizontal Pixel Panning */
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);

		outb(GN_MISCOUTW, *sp++);	/* Misc output register */
	}			
	
	/* disable access to first 7 CRTC registers */
	
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte);

	vga_screen_on();
	
	return(1);
}
#endif /* PCVT_132GENERIC */

/*---------------------------------------------------------------------------*
 *	toggle 80/132 column operation for ET4000 based boards
 *---------------------------------------------------------------------------*/
int
et4000_col(int cols)
{
	u_char *sp;
	u_char byte;

	vga_screen_off();
	
	/* enable access to first 7 CRTC registers */

	outb(addr_6845, CRTC_VSYNCE);			
	byte = inb(addr_6845+1);
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte & 0x7f);
	
	if(cols == SCR_COL132)		/* switch 80 -> 132 */
	{
		/* save state of board for 80 columns */
		
		if(!regsaved)
		{
			regsaved = 1;

			sp = savearea.et4000;
		
			outb(addr_6845, 0x00);	/* Horizontal Total */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x01);	/* Horizontal Display End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x02);	/* Horizontal Blank Start */
			*sp++ = inb(addr_6845+1);
		
			outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x05);	/* Horizontal Retrace End */
			*sp++ = inb(addr_6845+1);
		
			outb(addr_6845, 0x13);	/* Row Offset Register */
			*sp++ = inb(addr_6845+1);
		
			outb(addr_6845, 0x34);	/* 6845 Compatibility */
			*sp++ = inb(addr_6845+1);
		
			outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
			*sp++ = inb(TS_DATA);
		
			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Mode control */
			outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);
		
			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Horizontal Pixel Panning */
			outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);

			*sp++ = inb(GN_MISCOUTR);	/* Misc output register */
		}

		/* setup chipset for 132 column operation */
		
		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, 0x9f);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, 0x83);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, 0x84);
	
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, 0x8b);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, 0x80);
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, 0x42);
	
		outb(addr_6845, 0x34);	/* 6845 Compatibility */
		outb(addr_6845+1, 0x0a);
	
		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, 0x01);	/* 8 dot char clock */
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); /* ATC Mode control */
		outb(ATC_DATAW, 0x08);	/* Line graphics disable */
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); /* ATC Horizontal Pixel Panning */
		outb(ATC_DATAW, 0x00);

		/* Misc output register */

		outb(GN_MISCOUTW, (inb(GN_MISCOUTR) & ~0x0c));
	}
	else	/* switch 132 -> 80 */
	{
		if(!regsaved)			/* failsafe */
		{
			/* disable access to first 7 CRTC registers */
			outb(addr_6845, CRTC_VSYNCE);
			outb(addr_6845+1, byte);
			vga_screen_on();
			return(0);
		}
	
		sp = savearea.et4000;
		
		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, *sp++);

		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, *sp++);		

	
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, *sp++);
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, *sp++);
	
		outb(addr_6845, 0x34);	/* 6845 Compatibility */
		outb(addr_6845+1, *sp++);
	
		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, *sp++);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Mode control */
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Horizontal Pixel Panning */
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);

		outb(GN_MISCOUTW, *sp++);	/* Misc output register */
	}			
	
	/* disable access to first 7 CRTC registers */
	
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte);

	vga_screen_on();
	
	return(1);
}

/*---------------------------------------------------------------------------*
 *	toggle 80/132 column operation for WD/Paradise based boards
 *
 *	when this card does 132 cols, the char map select register (TS_INDEX,
 *	TS_FONTSEL) function bits get REDEFINED. whoever did design this,
 *	please don't cross my way ever .......
 *
 *---------------------------------------------------------------------------*/
int
wd90c11_col(int cols)
{

#if !PCVT_BACKUP_FONTS
	static unsigned char *sv_fontwd[NVGAFONTS];
#endif /*  !PCVT_BACKUP_FONTS */

	u_char *sp;
	u_char byte;
	int i;

	vga_screen_off();
	
	/* enable access to first 7 CRTC registers */

	outb(addr_6845, CRTC_VSYNCE);			
	byte = inb(addr_6845+1);
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte & 0x7f);

	/* enable access to WD/Paradise "control extensions" */

	outb(GDC_INDEX, GDC_PR5GPLOCK);
	outb(GDC_INDEX, 0x05);	
	outb(addr_6845, CRTC_PR10);
	outb(addr_6845, 0x85);	
	outb(TS_INDEX, TS_UNLOCKSEQ);
	outb(TS_DATA, 0x48);

	if(cols == SCR_COL132)		/* switch 80 -> 132 */
	{
		/* save state of board for 80 columns */
		
		if(!regsaved)
		{
			regsaved = 1;

			/* save current fonts */
			
#if !PCVT_BACKUP_FONTS
			for(i = 0; i < totalfonts; i++)
			{
				if(vgacs[i].loaded)
				{
					if((sv_fontwd[i] =
					    (u_char *)malloc(32 * 256,
							     M_DEVBUF,
							     M_WAITOK))
					   == NULL)
						printf("pcvt: no font buffer\n");
					else
						vga_move_charset(i,
								 sv_fontwd[i],
								 1);
				}
				else
				{
					sv_fontwd[i] = 0;
				}
			}

#endif /* !PCVT_BACKUP_FONTS */

			sp = savearea.wd90c11;
		
			outb(addr_6845, 0x00);	/* Horizontal Total */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x01);	/* Horizontal Display End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x02);	/* Horizontal Blank Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x03);	/* Horizontal Blank End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x05);	/* Horizontal Retrace End */
			*sp++ = inb(addr_6845+1);
		
			outb(addr_6845, 0x13);	/* Row Offset Register */
			*sp++ = inb(addr_6845+1);
		
			outb(addr_6845, 0x2e);	/* misc 1 */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x2f);	/* misc 2 */
			*sp++ = inb(addr_6845+1);
		
			outb(TS_INDEX, 0x10);/* Timing Sequencer */
			*sp++ = inb(TS_DATA);
			outb(TS_INDEX, 0x12);/* Timing Sequencer */
			*sp++ = inb(TS_DATA);

			*sp++ = inb(GN_MISCOUTR);	/* Misc output register */
		}

		/* setup chipset for 132 column operation */

		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, 0x9c);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, 0x83);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, 0x84);
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, 0x9f);
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, 0x8a);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, 0x1c);
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, 0x42);

		outb(addr_6845, 0x2e);	/* misc 1 */
		outb(addr_6845+1, 0x04);
		outb(addr_6845, 0x2f);	/* misc 2 */
		outb(addr_6845+1, 0x00);

		outb(TS_INDEX, 0x10);/* Timing Sequencer */
		outb(TS_DATA, 0x21);
		outb(TS_INDEX, 0x12);/* Timing Sequencer */
		outb(TS_DATA, 0x14);
		
		outb(GN_MISCOUTW, (inb(GN_MISCOUTR) | 0x08));	/* Misc output register */

		vsp->wd132col = 1;
	}
	else	/* switch 132 -> 80 */
	{
		if(!regsaved)			/* failsafe */
		{
			/* disable access to first 7 CRTC registers */

			outb(addr_6845, CRTC_VSYNCE);
			outb(addr_6845+1, byte);

			/* disable access to WD/Paradise "control extensions" */
		
			outb(GDC_INDEX, GDC_PR5GPLOCK);
			outb(GDC_INDEX, 0x00);	
			outb(addr_6845, CRTC_PR10);
			outb(addr_6845, 0x00);	
			outb(TS_INDEX, TS_UNLOCKSEQ);
			outb(TS_DATA, 0x00);

			vga_screen_on();

			return(0);
		}
	
		sp = savearea.wd90c11;
		
		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, *sp++);
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, *sp++);
	
		outb(addr_6845, 0x2e);	/* misc 1 */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x2f);	/* misc 2 */
		outb(addr_6845+1, *sp++);
	
		outb(TS_INDEX, 0x10);/* Timing Sequencer */
		outb(addr_6845+1, *sp++);
		outb(TS_INDEX, 0x12);/* Timing Sequencer */
		outb(addr_6845+1, *sp++);

		outb(GN_MISCOUTW, *sp++);	/* Misc output register */

		vsp->wd132col = 0;
	}			

	/* restore fonts */

#if !PCVT_BACKUP_FONTS
	for(i = 0; i < totalfonts; i++)
	{
		if(sv_fontwd[i])
			vga_move_charset(i, sv_fontwd[i], 0);
	}
#else
	for(i = 0; i < totalfonts; i++)
		if(saved_charsets[i])
			vga_move_charset(i, 0, 0);
#endif /* !PCVT_BACKUP_FONTS */
	
	select_vga_charset(vsp->vga_charset);

	/* disable access to first 7 CRTC registers */
	
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte);

	/* disable access to WD/Paradise "control extensions" */

	outb(GDC_INDEX, GDC_PR5GPLOCK);
	outb(GDC_INDEX, 0x00);	
	outb(addr_6845, CRTC_PR10);
	outb(addr_6845, 0x00);	
	outb(TS_INDEX, TS_UNLOCKSEQ);
	outb(TS_DATA, 0x00);
	
	vga_screen_on();
	
	return(1);
}

/*---------------------------------------------------------------------------*
 *	toggle 80/132 column operation for TRIDENT 9000 based boards
 *---------------------------------------------------------------------------*/
int
tri9000_col(int cols)
{
	u_char *sp;
	u_char byte;
	
	vga_screen_off();

	/* sync reset is necessary to preserve memory contents ... */
	
	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x01);	/* synchronous reset */

	/* disable protection of misc out and other regs */

	outb(addr_6845, CRTC_MTEST);			
	byte = inb(addr_6845+1);
	outb(addr_6845, CRTC_MTEST);
	outb(addr_6845+1, byte & ~0x50);

	/* enable access to first 7 CRTC registers */

	outb(addr_6845, CRTC_VSYNCE);			
	byte = inb(addr_6845+1);
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte & 0x7f);
	
	if(cols == SCR_COL132)		/* switch 80 -> 132 */
	{
		/* save state of board for 80 columns */
		
		if(!regsaved)
		{
			regsaved = 1;

			sp = savearea.tri9000;

			outb(addr_6845, 0x00);	/* Horizontal Total */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x01);	/* Horizontal Display End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x02);	/* Horizontal Blank Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x03);	/* Horizontal Blank End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x05);	/* Horizontal Retrace End */
			*sp++ = inb(addr_6845+1);

			outb(addr_6845, 0x13);
			*sp++ = inb(addr_6845+1);
		
			outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
			*sp++ = inb(TS_DATA);

			outb(TS_INDEX, TS_HWVERS);/* Hardware Version register */
			outb(TS_DATA, 0x00);	  /* write ANYTHING switches to OLD */
			outb(TS_INDEX, TS_MODEC2);
			*sp++ = inb(TS_DATA);

			outb(TS_INDEX, TS_HWVERS);/* Hardware Version register */
			inb(TS_DATA);		  /* read switches to NEW */
			outb(TS_INDEX, TS_MODEC2);
			*sp++ = inb(TS_DATA);
		
			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Mode control */
			outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);
		
			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Horizontal Pixel Panning */
			outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);

			*sp++ = inb(GN_MISCOUTR);	/* Misc output register */
		}

		/* setup chipset for 132 column operation */
		
		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, 0x9b);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, 0x83);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, 0x84);
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, 0x1e);
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, 0x87);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, 0x1a);

		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, 0x42);
	
		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, 0x01);	/* 8 dot char clock */
	
		outb(TS_INDEX, TS_HWVERS);/* Hardware Version register */
		outb(TS_DATA, 0x00);	  /* write ANYTHING switches to OLD */
		outb(TS_INDEX, TS_MODEC2);
		outb(TS_DATA, 0x00);

		outb(TS_INDEX, TS_HWVERS);/* Hardware Version register */
		inb(TS_DATA);		  /* read switches to NEW */
		outb(TS_INDEX, TS_MODEC2);
		outb(TS_DATA, 0x01);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); /* ATC Mode control */
		outb(ATC_DATAW, 0x08);	/* Line graphics disable */
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); /* ATC Horizontal Pixel Panning */
		outb(ATC_DATAW, 0x00);
	
		outb(GN_MISCOUTW, (inb(GN_MISCOUTR) | 0x0c));	/* Misc output register */
	}
	else	/* switch 132 -> 80 */
	{
		if(!regsaved)			/* failsafe */
		{
			/* disable access to first 7 CRTC registers */
			outb(addr_6845, CRTC_VSYNCE);
			outb(addr_6845+1, byte);

			outb(TS_INDEX, TS_SYNCRESET);
			outb(TS_DATA, 0x03);	/* clear synchronous reset */

			vga_screen_on();

			return(0);
		}
	
		sp = savearea.tri9000;

		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, *sp++);		
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, *sp++);		
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, *sp++);

		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, *sp++);
	
		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, *sp++);

		outb(TS_INDEX, TS_HWVERS);/* Hardware Version register */
		outb(TS_DATA, 0x00);	  /* write ANYTHING switches to OLD */
		outb(TS_INDEX, TS_MODEC2);/* Timing Sequencer */
		outb(TS_DATA, *sp++);

		outb(TS_INDEX, TS_HWVERS);/* Hardware Version register */
		inb(TS_DATA);		  /* read switches to NEW */
		outb(TS_INDEX, TS_MODEC2);/* Timing Sequencer */
		outb(TS_DATA, *sp++);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Mode control */
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Horizontal Pixel Panning */
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);

		outb(GN_MISCOUTW, *sp++);	/* Misc output register */
	}			
	
	/* disable access to first 7 CRTC registers */
	
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte);

	outb(TS_INDEX, TS_SYNCRESET);
	outb(TS_DATA, 0x03);	/* clear synchronous reset */

	vga_screen_on();

	return(1);
}

/*---------------------------------------------------------------------------*
 *	toggle 80/132 column operation for Video7 VGA 1024i
 *---------------------------------------------------------------------------*/
int
v7_1024i_col(int cols)
{
	u_char *sp;
	u_char byte;
	u_char save__byte;
	
	vga_screen_off();

	/* enable access to first 7 CRTC registers */

	/* first, enable read access to vertical retrace start/end */
	outb(addr_6845, CRTC_HBLANKE);
	byte = inb(addr_6845+1);
	outb(addr_6845, CRTC_HBLANKE);
	outb(addr_6845+1, (byte | 0x80));

	/* second, enable access to protected registers */
	outb(addr_6845, CRTC_VSYNCE);			
	save__byte = byte = inb(addr_6845+1);
	byte |= 0x20;	/* no irq 2 */
	byte &= 0x6f;	/* wr enable, clr irq flag */
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte);
	
	outb(TS_INDEX, TS_EXTCNTL);	/* enable extensions */
	outb(TS_DATA, 0xea);	


	if(cols == SCR_COL132)		/* switch 80 -> 132 */
	{
		/* save state of board for 80 columns */
		
		if(!regsaved)
		{
			regsaved = 1;

			sp = savearea.v7_1024i;
		
			outb(addr_6845, 0x00);	/* Horizontal Total */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x01);	/* Horizontal Display End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x02);	/* Horizontal Blank Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x03);	/* Horizontal Blank End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x05);	/* Horizontal Retrace End */
			*sp++ = inb(addr_6845+1);
		
			outb(addr_6845, 0x13);	/* Row Offset Register */
			*sp++ = inb(addr_6845+1);
		
			outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
			*sp++ = inb(TS_DATA);
		
			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Mode control */
			outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);
		
			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Horizontal Pixel Panning */
			outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);

			outb(TS_INDEX, 0x83);
			*sp++ = inb(TS_DATA);
	
			outb(TS_INDEX, 0xa4);
			*sp++ = inb(TS_DATA);
	
			outb(TS_INDEX, 0xe0);
			*sp++ = inb(TS_DATA);
	
			outb(TS_INDEX, 0xe4);
			*sp++ = inb(TS_DATA);
	
			outb(TS_INDEX, 0xf8);
			*sp++ = inb(TS_DATA);
	
			outb(TS_INDEX, 0xfd);
			*sp++ = inb(TS_DATA);
	
			*sp++ = inb(GN_MISCOUTR);	/* Misc output register */
		}

		/* setup chipset for 132 column operation */
		
		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, 0x9c);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, 0x83);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, 0x86);
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, 0x9e);
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, 0x89);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, 0x1c);
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, 0x42);
	
		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, 0x01);	/* 8 dot char clock */

		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); /* ATC Mode control */
		outb(ATC_DATAW, 0x08);	/* Line graphics disable */
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); /* ATC Horizontal Pixel Panning */
		outb(ATC_DATAW, 0x00);

		outb(TS_INDEX, TS_SYNCRESET);
		outb(TS_DATA, 0x01);	/* synchronous reset */

		outb(TS_INDEX, 0x83);
		outb(TS_DATA, 0xa0);

		outb(TS_INDEX, 0xa4);
		outb(TS_DATA, 0x1c);

		outb(TS_INDEX, 0xe0);
		outb(TS_DATA, 0x00);

		outb(TS_INDEX, 0xe4);
		outb(TS_DATA, 0xfe);

		outb(TS_INDEX, 0xf8);
		outb(TS_DATA, 0x1b);

		outb(TS_INDEX, 0xfd);
		outb(TS_DATA, 0x33);

		byte = inb(GN_MISCOUTR);
		byte |= 0x0c;
		outb(GN_MISCOUTW, byte);	/* Misc output register */

		outb(TS_INDEX, TS_SYNCRESET);
		outb(TS_DATA, 0x03);	/* clear synchronous reset */
	}
	else	/* switch 132 -> 80 */
	{
		if(!regsaved)			/* failsafe */
		{
			outb(TS_INDEX, TS_EXTCNTL);	/* disable extensions */
			outb(TS_DATA, 0xae);	

			/* disable access to first 7 CRTC registers */
			outb(addr_6845, CRTC_VSYNCE);
			outb(addr_6845+1, byte);
			vga_screen_on();
			return(0);
		}
	
		sp = savearea.v7_1024i;
		
		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, *sp++);		
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, *sp++);		
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, *sp++);
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, *sp++);
	
		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, *sp++);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Mode control */
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Horizontal Pixel Panning */
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);

		outb(TS_INDEX, TS_SYNCRESET);
		outb(TS_DATA, 0x01);	/* synchronous reset */

		outb(TS_INDEX, 0x83);
		outb(TS_DATA, *sp++);

		outb(TS_INDEX, 0xa4);
		outb(TS_DATA, *sp++);

		outb(TS_INDEX, 0xe0);
		outb(TS_DATA, *sp++);

		outb(TS_INDEX, 0xe4);
		outb(TS_DATA, *sp++);

		outb(TS_INDEX, 0xf8);
		outb(TS_DATA, *sp++);

		outb(TS_INDEX, 0xfd);
		outb(TS_DATA, *sp++);

		outb(GN_MISCOUTW, *sp++);	/* Misc output register */

		outb(TS_INDEX, TS_SYNCRESET);
		outb(TS_DATA, 0x03);	/* clear synchronous reset */
	}			
	
	outb(TS_INDEX, TS_EXTCNTL);	/* disable extensions */
	outb(TS_DATA, 0xae);	

	/* disable access to first 7 CRTC registers */
	
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, save__byte);

	vga_screen_on();
	
	return(1);
}

/*---------------------------------------------------------------------------*
 *	toggle 80/132 column operation for S3 86C928 based boards
 *---------------------------------------------------------------------------*/
int
s3_928_col(int cols)
{
	u_char *sp;
	u_char byte;

	vga_screen_off();
	
	outb(addr_6845, 0x38);
	outb(addr_6845+1, 0x48);	/* unlock registers */
	outb(addr_6845, 0x39);
	outb(addr_6845+1, 0xa0);	/* unlock registers */

	/* enable access to first 7 CRTC registers */

	outb(addr_6845, CRTC_VSYNCE);			
	byte = inb(addr_6845+1);
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte & 0x7f);
	
	if(cols == SCR_COL132)		/* switch 80 -> 132 */
	{
		/* save state of board for 80 columns */
		
		if(!regsaved)
		{
			regsaved = 1;

			sp = savearea.s3_928;
		
			outb(addr_6845, 0x00);	/* Horizontal Total */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x01);	/* Horizontal Display End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x02);	/* Horizontal Blank Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x03);	/* Horizontal Blank End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x05);	/* Horizontal Retrace End */
			*sp++ = inb(addr_6845+1);
		
			outb(addr_6845, 0x13);	/* Row Offset Register */
			*sp++ = inb(addr_6845+1);

			outb(addr_6845, 0x34);	/* Backward Compat 3 Reg */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x3b);	/* Data Xfer Exec Position */
			*sp++ = inb(addr_6845+1);

			outb(addr_6845, 0x42);	/* (Clock) Mode Control */
			*sp++ = inb(addr_6845+1);
		
			outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
			*sp++ = inb(TS_DATA);
		
			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Mode control */
			outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);
		
			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Horizontal Pixel Panning */
			outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);

			*sp++ = inb(GN_MISCOUTR);	/* Misc output register */
		}

		/* setup chipset for 132 column operation */
		
		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, 0x9a);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, 0x83);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, 0x86);
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, 0x9d);
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, 0x87);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, 0x1b);
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, 0x42);

		outb(addr_6845, 0x34);	
		outb(addr_6845+1, 0x10);/* enable data xfer pos control */
		outb(addr_6845, 0x3b);
		outb(addr_6845+1, 0x90);/* set data xfer pos value */
			
		outb(addr_6845, 0x42);	/* (Clock) Mode Control */
		outb(addr_6845+1, 0x02);/* Select 40MHz Clock */
		
		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, 0x01);	/* 8 dot char clock */
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); /* ATC Mode control */
		outb(ATC_DATAW, 0x08);	/* Line graphics disable */
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); /* ATC Horizontal Pixel Panning */
		outb(ATC_DATAW, 0x00);

		/* Misc output register */

		outb(GN_MISCOUTW, (inb(GN_MISCOUTR) | 0x0c));
	}
	else	/* switch 132 -> 80 */
	{
		if(!regsaved)			/* failsafe */
		{
			/* disable access to first 7 CRTC registers */
			outb(addr_6845, CRTC_VSYNCE);
			outb(addr_6845+1, byte);

			outb(addr_6845, 0x38);
			outb(addr_6845+1, 0x00);	/* lock registers */
			outb(addr_6845, 0x39);
			outb(addr_6845+1, 0x00);	/* lock registers */

			vga_screen_on();
			return(0);
		}
	
		sp = savearea.s3_928;
		
		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, *sp++);		
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, *sp++);		
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, *sp++);
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, *sp++);

		outb(addr_6845, 0x34);
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x3b);
		outb(addr_6845+1, *sp++);

		outb(addr_6845, 0x42);	/* Mode control */
		outb(addr_6845+1, *sp++);
		
		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, *sp++);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Mode control */
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Horizontal Pixel Panning */
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);

		outb(GN_MISCOUTW, *sp++);	/* Misc output register */
	}			
	
	/* disable access to first 7 CRTC registers */
	
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte);

	outb(addr_6845, 0x38);
	outb(addr_6845+1, 0x00);	/* lock registers */
	outb(addr_6845, 0x39);
	outb(addr_6845+1, 0x00);	/* lock registers */
	
	vga_screen_on();
	
	return(1);
}

/*---------------------------------------------------------------------------*
 *	toggle 80/132 column operation for Cirrus Logic 542x based boards
 *---------------------------------------------------------------------------*/
int
cl_gd542x_col(int cols)
{
	u_char *sp;
	u_char byte;

	vga_screen_off();
	
	/* enable access to first 7 CRTC registers */

	outb(addr_6845, CRTC_VSYNCE);			
	byte = inb(addr_6845+1);
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte & 0x7f);

	/* enable access to cirrus extension registers */

	outb(TS_INDEX, 6);
	outb(TS_DATA, 0x12);
	
	if(cols == SCR_COL132)		/* switch 80 -> 132 */
	{
		/* save state of board for 80 columns */
		
		if(!regsaved)
		{
			regsaved = 1;

			sp = savearea.cirrus;
		
			outb(addr_6845, 0x00);	/* Horizontal Total */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x01);	/* Horizontal Display End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x02);	/* Horizontal Blank Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x03);	/* Horizontal Blank End */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
			*sp++ = inb(addr_6845+1);
			outb(addr_6845, 0x05);	/* Horizontal Retrace End */
			*sp++ = inb(addr_6845+1);
		
			outb(addr_6845, 0x13);	/* Row Offset Register */
			*sp++ = inb(addr_6845+1);
		
			outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
			*sp++ = inb(TS_DATA);
		

			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Mode control */
			outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);
		
			if(color)
				inb(GN_INPSTAT1C);
			else
				inb(GN_INPSTAT1M);
			/* ATC Horizontal Pixel Panning */
			outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
			*sp++ = inb(ATC_DATAR);

			/* VCLK2 Numerator Register */
			outb(TS_INDEX, 0xd);
			*sp++ = inb(TS_DATA);

			/* VCLK2 Denominator and Post-Scalar Value Register */
			outb(TS_INDEX, 0x1d);
			*sp++ = inb(TS_DATA);

			/* Misc output register */
			*sp++ = inb(GN_MISCOUTR);
		}

		/* setup chipset for 132 column operation */

		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, 0x9f);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, 0x83);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, 0x84);
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, 0x82);
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, 0x8a);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, 0x9e);
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, 0x42);

		/* set VCLK2 to 41.164 MHz ..... */
		outb(TS_INDEX, 0xd);	/* VCLK2 Numerator Register */
		outb(TS_DATA, 0x45);

		outb(TS_INDEX, 0x1d);	/* VCLK2 Denominator and */
		outb(TS_DATA, 0x30);	/* Post-Scalar Value Register */

		/* and use it. */
		outb(GN_MISCOUTW, (inb(GN_MISCOUTR) & ~0x0c) | (2 << 2));

		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, 0x01);	/* 8 dot char clock */
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); /* ATC Mode control */
		outb(ATC_DATAW, 0x08);	/* Line graphics disable */
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); /* ATC Horizontal Pixel Panning */
		outb(ATC_DATAW, 0x00);
	}
	else	/* switch 132 -> 80 */
	{
		if(!regsaved)			/* failsafe */
		{
			/* disable access to first 7 CRTC registers */
			outb(addr_6845, CRTC_VSYNCE);
			outb(addr_6845+1, byte);
			/* disable access to cirrus extension registers */
			outb(TS_INDEX, 6);
			outb(TS_DATA, 0);
			vga_screen_on();
			return(0);
		}
	
		sp = savearea.generic;
		
		outb(addr_6845, 0x00);	/* Horizontal Total */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x01);	/* Horizontal Display End */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x02);	/* Horizontal Blank Start */
		outb(addr_6845+1, *sp++);		
		outb(addr_6845, 0x03);	/* Horizontal Blank End */
		outb(addr_6845+1, *sp++);		
		outb(addr_6845, 0x04);	/* Horizontal Retrace Start */
		outb(addr_6845+1, *sp++);
		outb(addr_6845, 0x05);	/* Horizontal Retrace End */
		outb(addr_6845+1, *sp++);
	
		outb(addr_6845, 0x13);	/* Row Offset Register */
		outb(addr_6845+1, *sp++);
	
		outb(TS_INDEX, TS_MODE);/* Timing Sequencer */
		outb(TS_DATA, *sp++);

		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Mode control */
		outb(ATC_INDEX, ATC_MODE | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);
	
		if(color)
			inb(GN_INPSTAT1C);
		else
			inb(GN_INPSTAT1M);
		/* ATC Horizontal Pixel Panning */
		outb(ATC_INDEX, ATC_HORPIXPAN | ATC_ACCESS); 
		outb(ATC_DATAW, *sp++);

		/* VCLK2 Numerator Register */
		outb(TS_INDEX, 0xd);
		outb(TS_DATA, *sp++);

		/* VCLK2 Denominator and Post-Scalar Value Register */
		outb(TS_INDEX, 0x1d);
		outb(TS_DATA, *sp++);

		/* Misc output register */
		outb(GN_MISCOUTW, *sp++);
	}			

	/* disable access to cirrus extension registers */

	outb(TS_INDEX, 6);
	outb(TS_DATA, 0);
	
	/* disable access to first 7 CRTC registers */
	
	outb(addr_6845, CRTC_VSYNCE);
	outb(addr_6845+1, byte);

	vga_screen_on();

	return(1);
}

/*---------------------------------------------------------------------------*
 *	switch screen from text mode to X-mode and vice versa
 *---------------------------------------------------------------------------*/
#if PCVT_USL_VT_COMPAT
static void
pcvt_x_hook(int tografx)
{
	int i;

#if PCVT_SCREENSAVER
	static unsigned saved_scrnsv_tmo = 0;
#endif /* PCVT_SCREENSAVER*/

#if PCVT_NETBSD
	extern u_short *Crtat;
#endif /* PCVT_NETBSD */

	/* save area for grahpics mode switch */

	if(!tografx)
	{	
		/* into standard text mode */
		/* step 1: restore fonts */
		for(i = 0; i < totalfonts; i++)
			if(saved_charsets[i])
				vga_move_charset(i, 0, 0);

		
#if PCVT_SCREENSAVER
		/* step 2: activate screen saver */
		if(saved_scrnsv_tmo)
			pcvt_set_scrnsv_tmo(saved_scrnsv_tmo);
#endif /* PCVT_SCREENSAVER */

		/* step 3: re-initialize lost MDA information */
		if(adaptor_type == MDA_ADAPTOR)
		{
			/*
			 * Due to the fact that HGC registers are
			 * write-only, the Xserver can only make
			 * guesses about the state the HGC adaptor
			 * has been before turning on X mode. Thus,
			 * the display must be re-enabled now, and
			 * the cursor shape and location restored.
			 */
			/* enable display, text mode */
			outb(GN_DMCNTLM, 0x28);

			/* restore cursor mode and shape */
			outb(addr_6845, CRTC_CURSORH);
			outb(addr_6845+1,
			     ((vsp->Crtat + vsp->cur_offset) - Crtat) >> 8);
			outb(addr_6845, CRTC_CURSORL);
			outb(addr_6845+1,
			     ((vsp->Crtat + vsp->cur_offset) - Crtat));
			
			outb(addr_6845, CRTC_CURSTART);
			outb(addr_6845+1, vsp->cursor_start);
			outb(addr_6845, CRTC_CUREND);
			outb(addr_6845+1, vsp->cursor_end);
		}

		/* step 4: restore screen and re-enable text output */

		/* kernel memory -> video board memory */
		bcopyb(vsp->Memory, Crtat,
		       vsp->screen_rowsize * vsp->maxcol * CHR);

		vsp->Crtat = Crtat;	/* operate on-screen now */

		outb(addr_6845, CRTC_STARTADRH);
		outb(addr_6845+1, 0);
		outb(addr_6845, CRTC_STARTADRL);
		outb(addr_6845+1, 0);
	
		/* step 5: make status display happy */
		async_update(0);
	}
	else /* tografx */
	{	
		/* switch to graphics mode: save everything we need */

		/* step 1: deactivate screensaver */

#if PCVT_SCREENSAVER
		if(saved_scrnsv_tmo = scrnsv_timeout)
			pcvt_set_scrnsv_tmo(0);	/* turn it off */
#endif /* PCVT_SCREENSAVER */

		/* step 2: handle status display */
		async_update((void *)1);	/* turn off */

		/* step 3: disable text output and save screen contents */

		/* video board memory -> kernel memory */
		bcopyb(vsp->Crtat, vsp->Memory,
		       vsp->screen_rowsize * vsp->maxcol * CHR);

		vsp->Crtat = vsp->Memory;	/* operate in memory now */
	}
}

/*---------------------------------------------------------------------------*
 *	switch to virtual screen n (0 ... PCVT_NSCREENS-1), VT_USL version
 *	(the name vgapage() stands for historical reasons)
 *---------------------------------------------------------------------------*/
int
vgapage(int new_screen)
{
	int x;
	
	if(new_screen < 0 || new_screen >= totalscreens)
		return EINVAL;
	
	if(vsp->proc != pfind(vsp->pid))
		/* XXX what is this for? */
		vt_switch_pending = 0;
		
	if(vt_switch_pending)
		return EAGAIN;
		
	vt_switch_pending = new_screen + 1;
	
	x = spltty();
	if(vs[new_screen].vt_status & VT_WAIT_ACT)
	{
		wakeup((caddr_t)&vs[new_screen].smode);
		vs[new_screen].vt_status &= ~VT_WAIT_ACT;
	}
	splx(x);

	if(new_screen == current_video_screen)
	{
		vt_switch_pending = 0;
		return 0;
	}

	/* fallback to VT_AUTO if controlling processes died */
	if(vsp->proc
	   && vsp->proc != pfind(vsp->pid))
		vsp->smode.mode = VT_AUTO;
	if(vs[new_screen].proc
	   && vs[new_screen].proc != pfind(vs[new_screen].pid))
		vs[new_screen].smode.mode = VT_AUTO;

	if(vsp->smode.mode == VT_PROCESS)
	{
		/* we cannot switch immediately here */
		vsp->vt_status |= VT_WAIT_REL;
		if(vsp->smode.relsig)
			psignal(vsp->proc, vsp->smode.relsig);
	}
	else
	{
		int modechange = 0;
		
		if((vs[new_screen].vt_status & VT_GRAFX) !=
		   (vsp->vt_status & VT_GRAFX))
		{
			if(vsp->vt_status & VT_GRAFX)
				modechange = 1;	/* to text */
			else
				modechange = 2;	/* to grfx */
		}
		
		if(modechange == 2)
			pcvt_x_hook(1);
				
		switch_screen(new_screen, vsp->vt_status & VT_GRAFX);

		if(modechange == 1)
			pcvt_x_hook(0);
				
		if(vsp->smode.mode == VT_PROCESS)
		{
			/* if _new_ vt is under process control... */
			vsp->vt_status |= VT_WAIT_ACK;
			if(vsp->smode.acqsig)
				psignal(vsp->proc, vsp->smode.acqsig);
		}
		else
			/* we are comitted */
			vt_switch_pending = 0;
	}
	return 0;
}

/*---------------------------------------------------------------------------*
 *	ioctl handling for VT_USL mode
 *---------------------------------------------------------------------------*/
int
usl_vt_ioctl(Dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	int i, j, error;

	switch(cmd)
	{

#if PCVT_FAKE_SYSCONS10
	case CONS_GETVERS:
		*(int *)data = 0x100; /* fake syscons 1.0 */
		return 0;
#endif /* PCVT_FAKE_SYSCONS10 */

	case VT_SETMODE:
		vsp->smode = *(struct vt_mode *)data;
		if(vsp->smode.mode == VT_PROCESS) {
			vsp->proc = p;
			vsp->pid = p->p_pid;
		}
		return 0;

	case VT_GETMODE:
		*(struct vt_mode *)data = vsp->smode;
		return 0;

	case VT_RELDISP:
		switch(*(int *)data) {
		case VT_FALSE:
			/* process refuses to release screen; abort */
			if(vt_switch_pending
			   && vt_switch_pending - 1 == current_video_screen
			   && (vsp->vt_status & VT_WAIT_REL)) {
				vsp->vt_status &= ~VT_WAIT_REL;
				vt_switch_pending = 0;
				return 0;
			}
			break;
			
		case VT_TRUE:
			/* process releases its VT */
			if(vt_switch_pending
			   && minor(dev) == current_video_screen
			   && (vsp->vt_status & VT_WAIT_REL)) {
				int new_screen = vt_switch_pending - 1;
				int modechange = 0;
				
				vsp->vt_status &= ~VT_WAIT_REL;

				if((vs[new_screen].vt_status & VT_GRAFX) !=
				   (vsp->vt_status & VT_GRAFX))
				{
					if(vsp->vt_status & VT_GRAFX)
						modechange = 1;	/* to text */
					else
						modechange = 2;	/* to grfx */
				}

				if(modechange == 2)
					pcvt_x_hook(1);
				
				switch_screen(new_screen,
					      vsp->vt_status & VT_GRAFX);

				if(modechange == 1)
					pcvt_x_hook(0);
				
				if(vsp->smode.mode == VT_PROCESS) {
					/*
					 * if the new vt is also in process
					 * mode, we have to wait until its
					 * controlling process acknowledged
					 * the switch
					 */
					vsp->vt_status
						|= VT_WAIT_ACK;
					if(vsp->smode.acqsig)
						psignal(vsp->proc,
							vsp->smode.acqsig);
				}
				else
					/* we are comitted */
					vt_switch_pending = 0;
				return 0;
			}
			break;
			
		case VT_ACKACQ:
			/* new vts controlling process acknowledged */
			if(vsp->vt_status & VT_WAIT_ACK) {
				vt_switch_pending = 0;
				vsp->vt_status &= ~VT_WAIT_ACK;
				return 0;
			}
			break;
		}
		return EINVAL;	/* end case VT_RELDISP */


	case VT_OPENQRY:
		/* return free vt */
		for(i = 0; i < PCVT_NSCREENS; i++)
			if(!vs[i].openf) {
				*(int *)data = i + 1;
				return 0;
			}
		return EAGAIN;

	case VT_GETACTIVE:
		*(int *)data = current_video_screen + 1;
		return 0;
		
	case VT_ACTIVATE:
		return vgapage(*(int *)data - 1);

	case VT_WAITACTIVE:
		/* sleep until vt switch happened */
		i = *(int *)data - 1;
		
		if(i != -1
		   && (i < 0 || i >= PCVT_NSCREENS))
			return EINVAL;
		
		if(i != -1
		   && minor(dev) == i)
			return 0;

		if(i == -1)
		{
			int x = spltty();
			vsp->vt_status |= VT_WAIT_ACT;
			while((error = tsleep((caddr_t)&vsp->smode,
					      PZERO | PCATCH, "waitvt", 0))
			      == ERESTART)
				;
			vsp->vt_status &= ~VT_WAIT_ACT;
			splx(x);
		}
		else
		{
			int x = spltty();
			vs[i].vt_status |= VT_WAIT_ACT;
			while((error = tsleep((caddr_t)&vs[i].smode,
					      PZERO | PCATCH, "waitvt", 0))
			      == ERESTART)
				;
			vs[i].vt_status &= ~VT_WAIT_ACT;
			splx(x);
		}
		return error;
		
	case KDENABIO:
		/* grant the process IO access; only allowed if euid == 0 */
	{
		struct trapframe *fp = (struct trapframe *)p->p_md.md_regs;
		
		if(suser(p->p_ucred, &p->p_acflag) != 0)
			return (EPERM);

		fp->tf_eflags |= PSL_IOPL;
	
		return 0;
	}
		
	
	case KDDISABIO:
		/* abandon IO access permission */
	{
		struct trapframe *fp = (struct trapframe *)p->p_md.md_regs;
		fp->tf_eflags &= ~PSL_IOPL;

		return 0;
	}

	case KDSETMODE:
	{
		struct video_state *vsx = &vs[minor(dev)];
		int haschanged = 0;
			
		if(adaptor_type != VGA_ADAPTOR
		   && adaptor_type != MDA_ADAPTOR)
			/* X will only run on those adaptors */
			return (EINVAL);

		/* set text/graphics mode of current vt */
		switch(*(int *)data)
		{
		case KD_TEXT:
			haschanged = (vsx->vt_status & VT_GRAFX) != 0;
			vsx->vt_status &= ~VT_GRAFX;
			if(haschanged && vsx == vsp)
				pcvt_x_hook(0);
			return 0;
			
		case KD_GRAPHICS:
			haschanged = (vsx->vt_status & VT_GRAFX) == 0;
			vsx->vt_status |= VT_GRAFX;
			if(haschanged && vsx == vsp)
				pcvt_x_hook(1);
			return 0;
			
		}
		return EINVAL;	/* end case KDSETMODE */
	}
		
	case KDSETRAD:
		/* set keyboard repeat and delay */
		return kbdioctl(dev, KBDSTPMAT, data, flag);

	case KDSKBMODE:
		switch(*(int *)data)
		{
		case K_RAW:

#if PCVT_SCANSET == 2
			/* put keyboard to return ancient PC scan codes */
			kbc_8042cmd(CONTR_WRITE); 
			outb(CONTROLLER_DATA,
#if PCVT_USEKBDSEC		/* security enabled */
		(COMMAND_SYSFLG|COMMAND_IRQEN|COMMAND_PCSCAN));
#else				/* no security */
		(COMMAND_INHOVR|COMMAND_SYSFLG|COMMAND_IRQEN|COMMAND_PCSCAN));
#endif /* PCVT_USEKBDSEC */
#endif /* PCVT_SCANSET == 2 */

			pcvt_kbd_raw = 1;
			shift_down = meta_down = altgr_down = ctrl_down = 0;
			return 0;

		case K_XLATE:
#if PCVT_SCANSET == 2
			kbc_8042cmd(CONTR_WRITE); 
			outb(CONTROLLER_DATA,
#if PCVT_USEKBDSEC		/* security enabled */
			     (COMMAND_SYSFLG|COMMAND_IRQEN));
#else				/* no security */
			     (COMMAND_INHOVR|COMMAND_SYSFLG|COMMAND_IRQEN));
#endif /* PCVT_USEKBDSEC */
#endif /* PCVT_SCANSET == 2 */

			pcvt_kbd_raw = 0;
			return 0;
		}
		return EINVAL;	/* end KDSKBMODE */
		
	case KDMKTONE:
		/* ring the speaker */
		if(data) {
			int duration = *(int *)data >> 16;
			int pitch = *(int *)data & 0xffff;
			
			sysbeep(pitch, duration * hz / 3000);
		}
		else
 			sysbeep(PCVT_SYSBEEPF / 1493, hz / 4);
		return 0;
		
	case KDSETLED:
		/* set kbd LED status */
		/* unfortunately, the LED definitions between pcvt and */
		/* USL differ some way :-( */
		i = *(int *)data;
		j = (i & LED_CAP? KBD_CAPSLOCK: 0)
			+ (i & LED_NUM? KBD_NUMLOCK: 0)
			+ (i & LED_SCR? KBD_SCROLLLOCK: 0);
		return kbdioctl(dev, KBDSLOCK, (caddr_t)&j, flag);
		
	case KDGETLED:
		/* get kbd LED status */
		if((error = kbdioctl(dev, KBDGLOCK, (caddr_t)&j, flag)))
			return error;
		i = (j & KBD_CAPSLOCK? LED_CAP: 0)
			+ (j & KBD_NUMLOCK? LED_NUM: 0)
			+ (j & KBD_SCROLLLOCK? LED_SCR: 0);
		*(int *)data = i;
		return 0;

	case GIO_KEYMAP:
		get_usl_keymap((keymap_t *)data);
		return 0;
	}			/* end case cmd */

	return -1;		/* inappropriate usl_vt_compat ioctl */
}
#endif /* PCVT_USL_VT_COMPAT */

#endif	/* NVT > 0 */

/* ------------------------- E O F ------------------------------------------*/

