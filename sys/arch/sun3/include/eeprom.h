/*	$NetBSD: eeprom.h,v 1.13.10.1 1998/01/27 02:06:19 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Structure/definitions for the Sun3 EEPROM.
 *
 * This information is published in the Sun document:
 * "PROM User's Manual", part number 800-1736010.
 */


/*
 * Note that most places where the PROM stores a "true/false" flag,
 * the true value is 0x12 and false is the usual zero.  Such flags
 * all take the values EE_TRUE or EE_FALSE so this file does not
 * need to define so many value macros.
 */
#define	EE_TRUE 0x12
#define	EE_FALSE   0

struct eeprom {

	/* 0x00 */
	u_char	eeTestArea[4];		/* Factory Defined */
	u_short	eeWriteCount[4];	/*    ||      ||   */
	u_char	eeChecksum[4];  	/*    ||      ||   */
	time_t	eeLastHwUpdate; 	/*    ||      ||   */

	/* 0x14 */
	u_char	eeInstalledMem; 	/* Megabytes */
	u_char	eeMemTestSize;		/*     ||    */

	/* 0x16 */
	u_char	eeScreenSize;
#define	EE_SCR_1152X900 	0x00
#define	EE_SCR_1024X1024	0x12
#define EE_SCR_1600X1280	0x13
#define EE_SCR_1440X1440	0x14

	u_char	eeWatchDogDoesReset;	/* Watchdog timeout action:
					 * true:  reset/reboot
					 * false: return to monitor
					 */
	/* 0x18 */
	u_char	eeBootDevStored;	/* Is the boot device stored:
					 * true:  use stored device spec.
					 * false: use default (try all)
					 */
	/* 0x19 */
	/* Stored boot device spec. i.e.: "sd(Ctlr,Unit,Part)" */
	u_char	eeBootDevName[2];	/* xy,xd,sd,ie,le,st,xt,mt,...	*/
	u_char	eeBootDevCtlr;
	u_char	eeBootDevUnit;
	u_char	eeBootDevPart;

	/* 0x1E */
	u_char	eeKeyboardType;		/* zero for sun keyboards */

	/* 0x1F */
	u_char	eeConsole;		/* What to use for the console	*/
#define	EE_CONS_BW		0x00	/* - On-board B&W / keyboard	*/
#define	EE_CONS_TTYA		0x10	/* - serial port A		*/
#define	EE_CONS_TTYB		0x11	/* - serial port B		*/
#define	EE_CONS_COLOR		0x12	/* - Color FB / keyboard	*/
#define	EE_CONS_P4OPT		0x20	/* - Option board on P4		*/

	/* 0x20 */
	u_char	eeCustomBanner;		/* Is there a custom banner:
					 * true:  use text at 0x68
					 * false: use Sun banner
					 */

	u_char	eeKeyClick;		/* true/false */

	/* 0x22 */
	/* Boot device with "Diag" switch in Diagnostic mode: */
	u_char	eeDiagDevName[2];
	u_char	eeDiagDevCtlr;
	u_char	eeDiagDevUnit;
	u_char	eeDiagDevPart;

	/* Video white-on-black (not implemented) */
	u_char	eeWhiteOnBlack;		/* true/false */

	/* 0x28 */
	char	eeDiagPath[40];		/* path name of diag program	*/

	/* 0x50 */
	u_char	eeTtyCols;		/* normally 80 (0x50) */
	u_char	eeTtyRows;		/* normally 34 (0x22) */
	u_char	ee_x52[6];		/* unused */

	/* 0x58 */
	/* Default parameters for tty A and tty B: */
	struct	eeTtyDef {
	    u_char	eetBaudSet;	/* Is the baud rate set?
					 * true:  use values here
					 * false: use default (9600)
					 */
	    u_char	eetBaudHi;	/* i.e. 96..  */
	    u_char	eetBaudLo;	/*      ..00  */
	    u_char	eetNoRtsDtr;	/* true: disable H/W flow
					 * false: enable H/W flow */
	    u_char	eet_pad[4];
	} eeTtyDefA, eeTtyDefB;

	/* 0x68 */
	char eeBannerString[80];	/* see eeCustomBanner above */

	/* 0xB8 */
	u_short	eeTestPattern;		/* must be 0xAA55 */
	u_short ee_xBA;			/* unused */

	/* 0xBC */
	/* Configuration data.  Hopefully we don't need it. */
	struct eeConf {
	    u_char	eecData[16];
	} eeConf[12+1];

	/* 0x18c */
	u_char	eeAltKeyTable;		/* What Key table to use:
					 * 0x58: EEPROM tables
					 * else: PROM key tables
					 */
	u_char	eeKeyboardLocale;	/* extended keyboard type */
	u_char	eeKeyboardID;		/* for EEPROM key tables  */
	u_char	eeCustomLogo;		/* true: use eeLogoBitmap */

	/* 0x190 */
	u_char	eeKeymapLC[0x80];
	u_char	eeKeymapUC[0x80];

	/* 0x290 */
	u_char	eeLogoBitmap[64][8];	/* 64x64 bit custom logo */

	/* 0x490 */
	u_char	ee_x490[2];		/* unused */

	/* 0x492 */
	u_char	ee_passwd_mode;		/* Only (ROM rev > 2.7.0)
					 * 0x5E = fully secure mode
					 * 0x01 = command secure mode
					 * Rest = non-secure mode
					 */
	u_char	ee_password[8];
	u_char	ee_x49b[0x500-0x49b];	/* unused */

	/* 0x500 */
	u_char	eeReserved[0x100];

	/* 0x600 */
	u_char	eeROM_Area[0x100];

	/* 0x700 */
	/* "Unix area" (hah!) */
	u_char ee_x700[0xb];		/* unused */
	/* 0x70b */
	u_char ee_diag_mode;		/* 3/80 diag switch:
					 * 0x06 = normal boot
					 * 0x12 = diagnostic mode
					 * Rest = full diagnostic boot
					 */
	/* 0x70c */
	u_char	ee_x70c[0x7d8-0x70c];	/* unused */

	/* 0x7d8 */
	u_char ee_80_IDPROM[32];	/* 3/80 IDPROM */
	/* 0x7f8 */
	u_char ee_80_CLOCK[8];		/* 3/80 clock */
};

#ifdef	_KERNEL
extern struct eeprom *eeprom_copy;
int	eeprom_uio __P((struct uio *));
#endif	/* _KERNEL */
