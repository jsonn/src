/* $NetBSD: vgareg.h,v 1.2.28.1 2002/01/10 19:55:08 thorpej Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
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
 *
 */

struct reg_vgaattr { /* indexed via port 0x3c0 */
	u_int8_t palette[16];
	u_int8_t mode, overscan, colplen, horpixpan;
	u_int8_t colreset, misc;
};
#define VGA_ATC_NREGS	21
#define VGA_ATC_INDEX	0
#define VGA_ATC_DATAW	0
#define VGA_ATC_DATAR	1

struct reg_vgats { /* indexed via port 0x3c4 */
	u_int8_t syncreset, mode, wrplmask, fontsel, memmode;
};
#define VGA_TS_NREGS	5 
#define VGA_TS_INDEX 	4
#define VGA_TS_DATA	5

struct reg_vgagdc { /* indexed via port 0x3ce */
	u_int8_t setres, ensetres, colorcomp, rotfunc;
	u_int8_t rdplanesel, mode, misc, colorcare;
	u_int8_t bitmask;
};
#define VGA_GDC_NREGS	9
#define VGA_GDC_INDEX	0xe
#define VGA_GDC_DATA	0xf

struct reg_vgacrtc { /* indexed via port 0x3d4 */
	u_int8_t index[25];
};
#define VGA_CRTC_NREGS	25
#define VGA_CRTC_INDEX	0x14
#define VGA_CRTC_DATA	0x15

/* misc output register */
#define VGA_MISC_INDEX	2
#define VGA_MISC_DATAR	0xc
#define VGA_MISC_DATAW	2
