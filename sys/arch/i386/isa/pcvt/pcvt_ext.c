/*
 * Copyright (c) 1992, 1995 Hellmuth Michaelis and Joerg Wunsch.
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
 * @(#)pcvt_ext.c, 3.32, Last Edit-Date: [Tue Oct  3 11:19:48 1995]
 *
 */

/*---------------------------------------------------------------------------*
 *
 *	pcvt_ext.c	VT220 Driver Extended Support Routines
 *	------------------------------------------------------
 *
 *	-hm	------------ Release 3.00 --------------
 *	-hm	integrating NetBSD-current patches
 *	-hm	applied Onno van der Linden's patch for Cirrus BIOS upgrade
 *	-hm	pcvt_x_hook has to care about fkey labels now
 *	-hm	changed some bcopyb's to bcopy's
 *	-hm	TS_INDEX -> TS_DATA for cirrus (mail from Onno/Charles)
 *	-jw	removed kbc_8042(), and replaced by kbd_emulate_pc()
 *	-hm	X server patch from John Kohl <jtk@kolvir.blrc.ma.us>
 *	-hm	applying Joerg's patch for FreeBSD 2.0
 *	-hm	enable 132 col support for Trident TVGA8900CL
 *	-hm	applying patch from Joerg fixing Crtat bug
 *	-hm	removed PCVT_FAKE_SYSCONS10
 *	-hm	fastscroll/Crtat bugfix from Lon Willett
 *	-hm	bell patch from Thomas Eberhardt for NetBSD
 *	-hm	multiple X server bugfixes from Lon Willett
 *	-hm	patch from John Kohl fixing tsleep bug in usl_vt_ioctl()
 *	-hm	bugfix: clear 25th line when switching to a force 24 lines vt
 *	-jw	add some forward declarations
 *	-hm	fixing MDA re-init when leaving X
 *	-hm	patch from John Kohl fixing potential divide by 0 problem
 *	-hm	patch from Joerg: console unavailable flag handling
 *	-hm	bugfix: unknown cirrus board enables 132 cols
 *	-hm	fixing NetBSD PR1123, minor typo (reported by J.T. Conklin)
 *	-hm	adding support for Cirrus 5430 chipset
 *	-hm	adding NetBSD-current patches from John Kohl
 *	-hm	---------------- Release 3.30 -----------------------
 *	-hm	patch to support Cirrus CL-GD62x5 from Martin
 *	-hm	patch to support 132 cols for Cirrus CL-GD62x5 from Martin
 *	-hm	patch from Frank van der Linden for keyboard state per VT
 *	-hm	patch from Charles Hannum, bugfix of keyboard state switch
 *	-hm	implemented KDGKBMODE keyboard ioctl
 *	-hm	patch from John Kohl, missing kbd_setmode() in switch_screen()
 *	-hm	---------------- Release 3.32 -----------------------
 *
 *---------------------------------------------------------------------------*/

#include "vt.h"
#if NVT > 0

#include "pcvt_hdr.h"		/* global include */

static int  s3testwritable( void );
static int  et4000_col( int );
static int  wd90c11_col( int );
static int  tri9000_col( int );
static int  v7_1024i_col( int );
static int  s3_928_col( int );
static int  cl_gd542x_col( int );

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
				can_do_132col = 1;
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

	if (newbyte == 0x12)
	{
		vga_family = VGA_F_CIR;

		switch ((byte & 0xfc) >> 2)
		{
			case 0x06:
				can_do_132col = 1;
				return VGA_CL_GD6225;

			case 0x22:
				switch (byte & 3)
				{
					case 0:
						can_do_132col = 1;
						return VGA_CL_GD5402;

					case 1:
						can_do_132col = 1;
						return VGA_CL_GD5402r1;

					case 2:
						can_do_132col = 1;
						return VGA_CL_GD5420;

					case 3:
						can_do_132col = 1;
						return VGA_CL_GD5420r1;
				}
				break;
			case 0x23:
				can_do_132col = 1;
				return VGA_CL_GD5422;

			case 0x24:
				can_do_132col = 1;
				return VGA_CL_GD5426;

			case 0x25:
				can_do_132col = 1;
				return VGA_CL_GD5424;

			case 0x26:
				can_do_132col = 1;
				return VGA_CL_GD5428;

			case 0x28:
				can_do_132col = 1;
				return VGA_CL_GD5430;

		}
		return(VGA_CL_UNKNOWN);
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
		"unknown s3",
		"cl-gd5402",
		"cl-gd5402r1",
		"cl-gd5420",
		"cl-gd5420r1",
		"cl-gd5422",
		"cl-gd5424",
		"cl-gd5426",
		"cl-gd5428",
		"cl-gd5430",
		"cl-gd62x5",
		"unknown cirrus",
						/* VGA_MAX_CHIPSET */
		"vga_string: chipset name table ptr overflow!"
	};

	if(number > VGA_MAX_CHIPSET)		/* failsafe */
		number = VGA_MAX_CHIPSET;

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
		case VGA_TR8900CL:
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
		case VGA_CL_GD5430:
		case VGA_CL_GD6225:
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
		outb(TS_DATA, 0x30);   /* Post-Scalar Value Register */

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

		sp = savearea.cirrus;

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

		outb(GN_MISCOUTW, *sp++);	/* Misc output register */
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
void
switch_screen(int n, int oldgrafx, int newgrafx)
{

#if PCVT_SCREENSAVER
	static unsigned saved_scrnsv_tmo = 0;
#endif	/* PCVT_SCREENSAVER */

#if !PCVT_KBD_FIFO
	int x;
#endif	/* !PCVT_KBD_FIFO */

	int cols = vsp->maxcol;		/* get current col val */

	if(n < 0 || n >= totalscreens)
		return;

#if !PCVT_KBD_FIFO
	x = spltty();			/* protect us */
#endif	/* !PCVT_KBD_FIFO */

	if(!oldgrafx && newgrafx)
	{
		/* switch from text to graphics */

#if PCVT_SCREENSAVER
		if((saved_scrnsv_tmo = scrnsv_timeout))
			pcvt_set_scrnsv_tmo(0);	/* screensaver off */
#endif /* PCVT_SCREENSAVER */

		async_update(UPDATE_STOP);	/* status display off */
	}

	if(!oldgrafx)
	{
		/* switch from text mode */

		/* video board memory -> kernel memory */
		bcopy(vsp->Crtat, vsp->Memory,
		      vsp->screen_rows * vsp->maxcol * CHR);

		vsp->Crtat = vsp->Memory;	/* operate in memory now */
	}

	/* update global screen pointers/variables */
	current_video_screen = n;	/* current screen no */

#if !PCVT_NETBSD && !(PCVT_FREEBSD > 110 && PCVT_FREEBSD < 200)
	pcconsp = &pccons[n];		/* current tty */
#elif PCVT_FREEBSD > 110 && PCVT_FREEBSD < 200
	pcconsp = pccons[n];		/* current tty */
#elif PCVT_NETBSD > 100
	pcconsp = vs[n].vs_tty;		/* current tty */
#else
	pcconsp = pc_tty[n];		/* current tty */
#endif

	vsp = &vs[n];			/* current video state ptr */

	if(oldgrafx && !newgrafx)
	{
		/* switch from graphics to text mode */
		unsigned i;

		/* restore fonts */
		for(i = 0; i < totalfonts; i++)
			if(saved_charsets[i])
				vga_move_charset(i, 0, 0);

#if PCVT_SCREENSAVER
		/* activate screen saver */
		if(saved_scrnsv_tmo)
			pcvt_set_scrnsv_tmo(saved_scrnsv_tmo);
#endif /* PCVT_SCREENSAVER */

		/* re-initialize lost MDA information */
		if(adaptor_type == MDA_ADAPTOR)
		{
		    /*
		     * Due to the fact that HGC registers are write-only,
		     * the Xserver can only make guesses about the state
		     * the HGC adaptor has been before turning on X mode.
		     * Thus, the display must be re-enabled now, and the
		     * cursor shape and location restored.
		     */
		    outb(GN_DMCNTLM, 0x28); /* enable display, text mode */
		    outb(addr_6845, CRTC_CURSORH); /* select high register */
		    outb(addr_6845+1,
			 ((vsp->Crtat + vsp->cur_offset) - Crtat) >> 8);
		    outb(addr_6845, CRTC_CURSORL); /* select low register */
		    outb(addr_6845+1,
			 ((vsp->Crtat + vsp->cur_offset) - Crtat));

		    outb(addr_6845, CRTC_CURSTART); /* select high register */
		    outb(addr_6845+1, vsp->cursor_start);
		    outb(addr_6845, CRTC_CUREND); /* select low register */
		    outb(addr_6845+1, vsp->cursor_end);
		}

		/* make status display happy */
		async_update(UPDATE_START);
	}

	if(!newgrafx)
	{
		/* to text mode */

		/* kernel memory -> video board memory */
		bcopy(vsp->Crtat, Crtat,
		      vsp->screen_rows * vsp->maxcol * CHR);

		vsp->Crtat = Crtat;		/* operate on screen now */

		outb(addr_6845, CRTC_STARTADRH);
		outb(addr_6845+1, 0);
		outb(addr_6845, CRTC_STARTADRL);
		outb(addr_6845+1, 0);
	}

#if !PCVT_KBD_FIFO
	splx(x);
#endif	/* !PCVT_KBD_FIFO */

	select_vga_charset(vsp->vga_charset);

	if(vsp->maxcol != cols)
		vga_col(vsp, vsp->maxcol);	/* select 80/132 columns */

 	outb(addr_6845, CRTC_CURSORH);	/* select high register */
	outb(addr_6845+1, vsp->cur_offset >> 8);
	outb(addr_6845, CRTC_CURSORL);	/* select low register */
	outb(addr_6845+1, vsp->cur_offset);

	if(vsp->cursor_on)
	{
		outb(addr_6845, CRTC_CURSTART);	/* select high register */
		outb(addr_6845+1, vsp->cursor_start);
		outb(addr_6845, CRTC_CUREND);	/* select low register */
		outb(addr_6845+1, vsp->cursor_end);
	}
	else
	{
		sw_cursor(0);
	}

	if(adaptor_type == VGA_ADAPTOR)
	{
		unsigned i;

		/* switch VGA DAC palette entries */
		for(i = 0; i < NVGAPEL; i++)
			vgapaletteio(i, &vsp->palette[i], 1);
	}

	if(!newgrafx)
	{
		update_led();	/* update led's */
		update_hp(vsp);	/* update fkey labels, if present */

		/* if we switch to a vt with force 24 lines mode and	*/
		/* pure VT emulation and 25 rows charset, then we have	*/
		/* to clear the last line on display ...		*/

		if(vsp->force24 && (vsp->vt_pure_mode == M_PUREVT) &&
			(vgacs[vsp->vga_charset].screen_size == SIZ_25ROWS))
		{
			fillw(' ', vsp->Crtat + vsp->screen_rows * vsp->maxcol,
				vsp->maxcol);
		}
	}
	kbd_setmode(vsp->kbd_state);
}

/*---------------------------------------------------------------------------*
 *	Change specified vt to VT_AUTO mode
 *	xxx Maybe this should also reset VT_GRAFX mode; since switching and
 *	graphics modes are not going to work without VT_PROCESS mode.
 *---------------------------------------------------------------------------*/
static void
set_auto_mode (struct video_state *vsx)
{
	unsigned ostatus = vsx->vt_status;
	vsx->smode.mode = VT_AUTO;
	vsx->proc = NULL;
	vsx->pid = 0;
	vsx->vt_status &= ~(VT_WAIT_REL|VT_WAIT_ACK);
	if (ostatus & VT_WAIT_ACK) {
#if 0
		assert (!(ostatus&VT_WAIT_REL));
		assert (vsp == vsx &&
			vt_switch_pending == current_video_screen + 1);
		vt_switch_pending = 0;
#else
		if (vsp == vsx &&
		    vt_switch_pending == current_video_screen + 1)
			vt_switch_pending = 0;
#endif
	}
	if (ostatus&VT_WAIT_REL) {
		int new_screen = vt_switch_pending - 1;
#if 0
		assert(vsp == vsx && vt_switch_pending);
		vt_switch_pending = 0;
		vgapage (new_screen);
#else
		if (vsp == vsx && vt_switch_pending) {
			vt_switch_pending = 0;
			vgapage (new_screen);
		}
#endif
	}
}

/*---------------------------------------------------------------------------*
 *	Exported function; to be called when a vt is closed down.
 *
 *	Ideally, we would like to be able to recover from an X server crash;
 *	but in reality, if the server crashes hard while in control of the
 *	vga board, then you're not likely to be able to use pcvt ttys
 *	without rebooting.
 *---------------------------------------------------------------------------*/
void
reset_usl_modes (struct video_state *vsx)
{
	/* Clear graphics mode */
	if (vsx->vt_status & VT_GRAFX)
	{
		vsx->vt_status &= ~VT_GRAFX;
		if (vsp == vsx)
			switch_screen(current_video_screen, 1, 0);
	}

	/* Take kbd out of raw mode */
	if(vsx->kbd_state == K_RAW)
	{
		if(vsx == vsp)
			kbd_setmode(K_XLATE);
		vsx->kbd_state = K_XLATE;
	}

	/* Clear process controlled mode */
	set_auto_mode (vsx);
}

/*---------------------------------------------------------------------------*
 *	switch to virtual screen n (0 ... PCVT_NSCREENS-1)
 *	(the name vgapage() stands for historical reasons)
 *---------------------------------------------------------------------------*/
int
vgapage(int new_screen)
{
	int x;

	if(new_screen < 0 || new_screen >= totalscreens)
		return EINVAL;

	/* fallback to VT_AUTO if controlling processes died */
	if(vsp->proc && vsp->proc != pfind(vsp->pid))
		set_auto_mode(vsp);
	if(vs[new_screen].proc
	   && vs[new_screen].proc != pfind(vs[new_screen].pid))
		set_auto_mode(&vs[new_screen]);

	if (!vt_switch_pending && new_screen == current_video_screen)
		return 0;

	if(vt_switch_pending && vt_switch_pending != new_screen + 1) {
		/* Try resignaling uncooperative X-window servers */
		if (vsp->smode.mode == VT_PROCESS) {
			if (vsp->vt_status & VT_WAIT_REL) {
				if(vsp->smode.relsig)
					psignal(vsp->proc, vsp->smode.relsig);
			} else if (vsp->vt_status & VT_WAIT_ACK) {
				if(vsp->smode.acqsig)
					psignal(vsp->proc, vsp->smode.acqsig);
			}
		}
		return EAGAIN;
	}

	vt_switch_pending = new_screen + 1;

	if(vsp->smode.mode == VT_PROCESS)
	{
		/* we cannot switch immediately here */
		vsp->vt_status |= VT_WAIT_REL;
		if(vsp->smode.relsig)
			psignal(vsp->proc, vsp->smode.relsig);
	}
	else
	{
		struct video_state *old_vsp = vsp;

		switch_screen(new_screen,
			      vsp->vt_status & VT_GRAFX,
			      vs[new_screen].vt_status & VT_GRAFX);

		x = spltty();
		if(old_vsp->vt_status & VT_WAIT_ACT)
		{
			old_vsp->vt_status &= ~VT_WAIT_ACT;
			wakeup((caddr_t)&old_vsp->smode);
		}
		if(vsp->vt_status & VT_WAIT_ACT)
		{
			vsp->vt_status &= ~VT_WAIT_ACT;
			wakeup((caddr_t)&vsp->smode);
		}
		splx(x);

		if(vsp->smode.mode == VT_PROCESS)
		{
			/* if _new_ vt is under process control... */
			vsp->vt_status |= VT_WAIT_ACK;
			if(vsp->smode.acqsig)
				psignal(vsp->proc, vsp->smode.acqsig);
		}
		else
		{
			/* we are committed */
			vt_switch_pending = 0;

#if PCVT_FREEBSD > 206
			/*
			 * XXX: If pcvt is acting as the systems console,
			 * avoid panics going to the debugger while we are in
			 * process mode.
			 */
			if(pcvt_is_console)
				cons_unavail = 0;
#endif
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------*
 *	VT_USL ioctl handling 
 *---------------------------------------------------------------------------*/
int
usl_vt_ioctl(Dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	int i, j, error, opri, mode;
	struct vt_mode newmode;
	struct video_state *vsx = &vs[minor(dev)];
	
	switch(cmd)
	{

	case VT_SETMODE:
		newmode = *(struct vt_mode *)data;

		opri = spltty();

		if (newmode.mode != VT_PROCESS)
		{
			if (vsx->smode.mode == VT_PROCESS)
			{
				if (vsx->proc != p)
				{
					splx(opri);
					return EPERM;
				}
				set_auto_mode(vsx);
			}
			splx(opri);
			return 0;
		}

		/*
		 * NB: XFree86-3.1.1 does the following:
		 *		VT_ACTIVATE (vtnum)
		 *		VT_WAITACTIVE (vtnum)
		 *		VT_SETMODE (VT_PROCESS)
		 * So it is possible that the screen was switched
		 * between the WAITACTIVE and the SETMODE (here).  This
		 * can actually happen quite frequently, and it was
		 * leading to dire consequences. Now it is detected by
		 * requiring that minor(dev) match current_video_screen.
		 * An alternative would be to operate on vs[minor(dev)]
		 * instead of *vsp, but that would leave the server
		 * confused, because it would believe that its vt was
		 * currently activated.
		 */
		if (minor(dev) != current_video_screen)
		{
			splx(opri);
			return EPERM;
		}

		/* Check for server died */

		if(vsp->proc && vsp->proc != pfind(vsp->pid))
			set_auto_mode(vsp);

		/* Check for server already running */

		if (vsp->smode.mode == VT_PROCESS && vsp->proc != p)
		{
			splx(opri);
			return EBUSY; /* already in use on this VT */
		}

		vsp->smode = newmode;
		vsp->proc = p;
		vsp->pid = p->p_pid;

#if PCVT_FREEBSD > 206
		/*
		 * XXX: If pcvt is acting as the systems console,
		 * avoid panics going to the debugger while we are in
		 * process mode.
		 */
		if(pcvt_is_console)
			cons_unavail = (newmode.mode == VT_PROCESS);
#endif

		splx(opri);
		return 0;

	case VT_GETMODE:
		*(struct vt_mode *)data = vsp->smode;
		return 0;

	case VT_RELDISP:
		if (minor(dev) != current_video_screen)
			return EPERM;
		if (vsp->smode.mode != VT_PROCESS)
			return EINVAL;
		if (vsp->proc != p)
			return EPERM;
		switch(*(int *)data)
		{
		case VT_FALSE:
			/* process refuses to release screen; abort */
			if(vt_switch_pending
			   && (vsp->vt_status & VT_WAIT_REL))
			{
				vsp->vt_status &= ~VT_WAIT_REL;
				vt_switch_pending = 0;
				return 0;
			}
			break;

		case VT_TRUE:
			/* process releases its VT */
			if(vt_switch_pending
			   && (vsp->vt_status & VT_WAIT_REL))
			{
				int new_screen = vt_switch_pending - 1;
				struct video_state *old_vsp = vsp;

				vsp->vt_status &= ~VT_WAIT_REL;

				switch_screen(new_screen,
					      vsp->vt_status & VT_GRAFX,
					      vs[new_screen].vt_status
					      & VT_GRAFX);

				opri = spltty();
				if(old_vsp->vt_status & VT_WAIT_ACT)
				{
					old_vsp->vt_status &= ~VT_WAIT_ACT;
					wakeup((caddr_t)&old_vsp->smode);
				}
				if(vsp->vt_status & VT_WAIT_ACT)
				{
					vsp->vt_status &= ~VT_WAIT_ACT;
					wakeup((caddr_t)&vsp->smode);
				}
				splx(opri);

				if(vsp->smode.mode == VT_PROCESS)
				{
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
				{
					/* we are committed */
					vt_switch_pending = 0;

#if PCVT_FREEBSD > 206
					/* XXX */
					if(pcvt_is_console)
						cons_unavail = 0;
#endif
				}
				return 0;
			}
			break;

		case VT_ACKACQ:
			/* new vts controlling process acknowledged */
			if(vsp->vt_status & VT_WAIT_ACK)
			{
				vt_switch_pending = 0;
				vsp->vt_status &= ~VT_WAIT_ACK;

#if PCVT_FREEBSD > 206
				/* XXX */
				if(pcvt_is_console)
					cons_unavail = 1;
#endif
				return 0;
			}
			break;
		}
		return EINVAL;	/* end case VT_RELDISP */


	case VT_OPENQRY:
		/* return free vt */
		for(i = 0; i < PCVT_NSCREENS; i++)
		{
			if(!vs[i].openf)
			{
				*(int *)data = i + 1;
				return 0;
			}
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

		if(i != -1 && (i < 0 || i >= PCVT_NSCREENS))
			return EINVAL;

		if(i != -1 && current_video_screen == i)
			return 0;

		if(i == -1)
		{
			/* xxx Is this what it is supposed to do? */
			int x = spltty();
			i = current_video_screen;
			error = 0;
			while (current_video_screen == i && error == 0)
			{
				vs[i].vt_status |= VT_WAIT_ACT;
				error = tsleep((caddr_t)&vs[i].smode,
					       PZERO | PCATCH, "waitvt", 0);
			}
			splx(x);
		}
		else
		{
			int x = spltty();
			error = 0;
			while (current_video_screen != i && error == 0)
			{
				vs[i].vt_status |= VT_WAIT_ACT;
				error = tsleep((caddr_t)&vs[i].smode,
					       PZERO | PCATCH, "waitvt", 0);
			}
			splx(x);
		}
		return (error == ERESTART) ? PCVT_ERESTART : error;

	case KDENABIO:
		/* grant the process IO access; only allowed if euid == 0 */
	{

#if PCVT_NETBSD > 9 || PCVT_FREEBSD >= 200
		struct trapframe *fp = (struct trapframe *)p->p_md.md_regs;
#elif PCVT_NETBSD || (PCVT_FREEBSD && PCVT_FREEBSD > 102)
		struct trapframe *fp = (struct trapframe *)p->p_regs;
#else
		struct syscframe *fp = (struct syscframe *)p->p_regs;
#endif

		if(suser(p->p_ucred, &p->p_acflag) != 0)
			return (EPERM);

#if PCVT_NETBSD || (PCVT_FREEBSD && PCVT_FREEBSD > 102)
		fp->tf_eflags |= PSL_IOPL;
#else
		fp->sf_eflags |= PSL_IOPL;
#endif

		return 0;
	}

	case KDDISABIO:
		/* abandon IO access permission */
	{

#if PCVT_NETBSD > 9 || PCVT_FREEBSD >= 200
		struct trapframe *fp = (struct trapframe *)p->p_md.md_regs;
		fp->tf_eflags &= ~PSL_IOPL;
#elif PCVT_NETBSD || (PCVT_FREEBSD && PCVT_FREEBSD > 102)
		struct trapframe *fp = (struct trapframe *)p->p_regs;
		fp->tf_eflags &= ~PSL_IOPL;
#else
		struct syscframe *fp = (struct syscframe *)p->p_regs;
		fp->sf_eflags &= ~PSL_IOPL;
#endif

		return 0;
	}

	case KDSETMODE:
	{
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
				switch_screen(current_video_screen, 1, 0);
			return 0;

		case KD_GRAPHICS:
			/* xxx It might be a good idea to require that
			   the vt be in process controlled mode here,
			   and that the calling process is the owner */
			haschanged = (vsx->vt_status & VT_GRAFX) == 0;
			vsx->vt_status |= VT_GRAFX;
			if(haschanged && vsx == vsp)
				switch_screen(current_video_screen, 0, 1);
			return 0;

		}
		return EINVAL;	/* end case KDSETMODE */
	}

	case KDSETRAD:
		/* set keyboard repeat and delay */
		return kbdioctl(dev, KBDSTPMAT, data, flag);

	case KDGKBMODE:
		*(int *)data = vsx->kbd_state;
		return 0;
		
	case KDSKBMODE:
		mode = *(int *)data;
		switch(mode)
		{
	  		case K_RAW:
  			case K_XLATE:
				if(vsx->kbd_state != mode)
				{
					if(vsx == vsp)
						kbd_setmode(mode);
					vsx->kbd_state = mode;
				}
  				return 0;
		}
		return EINVAL;	/* end KDSKBMODE */

	case KDMKTONE:
		/* ring the speaker */
		if(data)
		{
			int duration = *(int *)data >> 16;
			int pitch = *(int *)data & 0xffff;

#if PCVT_NETBSD
			if(pitch != 0)
			{
			    sysbeep(PCVT_SYSBEEPF / pitch,
				    duration * hz / 1000);
			}
#else /* PCVT_NETBSD */
			sysbeep(pitch, duration * hz / 3000);
#endif /* PCVT_NETBSD */

		}
		else
		{
 			sysbeep(PCVT_SYSBEEPF / 1493, hz / 4);
 		}
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

#endif	/* NVT > 0 */

/* ------------------------- E O F ------------------------------------------*/

