/*	$NetBSD: miidevs.h,v 1.8 1999/09/07 19:29:52 soren Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: miidevs,v 1.7 1999/09/04 23:58:55 soren Exp 
 */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * List of known MII OUIs.
 * For a complete list see http://standards.ieee.org/regauth/oui/
 *
 * XXX Vendors do obviously not agree how OUIs (18 bit) are mapped
 * to the 16 bits available in the id registers. The MII_OUI() macro
 * in "mii.h" reflects the most obvious way. If a vendor uses a
 * different mapping, an "xx" prefixed OUI is defined here which is
 * mangled accordingly to compensate.
 */

#define	MII_OUI_AMD	0x00001a	/* Advanced Micro Devices */
#define	MII_OUI_DAVICOM	0x00606e	/* Davicom Semiconductor */
#define	MII_OUI_ICS	0x00a0be	/* Integrated Circuit Systems */
#define	MII_OUI_INTEL	0x00aa00	/* Intel */
#define	MII_OUI_LEVEL1	0x00207b	/* Level 1 */
#define	MII_OUI_NATSEMI	0x080017	/* National Semiconductor */
#define	MII_OUI_QUALSEMI	0x006051	/* Quality Semiconductor */
#define	MII_OUI_SEEQ	0x00a07d	/* Seeq */
#define	MII_OUI_SIS	0x00e006	/* Silicon Integrated Systems */
#define	MII_OUI_TI	0x080028	/* Texas Instruments */
#define	MII_OUI_TSC	0x00c039	/* TDK Semiconductor */

/* in the 79c873, AMD uses another OUI (which matches Davicom!) */
#define	MII_OUI_xxAMD	0x00606e	/* Advanced Micro Devices */

/* Some Intel 82553's use an alternative OUI. */
#define	MII_OUI_xxINTEL	0x00f800	/* Intel */

/* some vendors have the bits swapped within bytes
	(ie, ordered as on the wire) */
#define	MII_OUI_xxICS	0x00057d	/* Integrated Circuit Systems */
#define	MII_OUI_xxSEEQ	0x0005be	/* Seeq */
#define	MII_OUI_xxSIS	0x000760	/* Silicon Integrated Systems */
#define	MII_OUI_xxTI	0x100014	/* Texas Instruments */

/* Level 1 is completely different - from right to left.
	(Two bits get lost in the third OUI byte.) */
#define	MII_OUI_xxLEVEL1	0x1e0400	/* Level 1 */

/* Don't know what's going on here. */
#define	MII_OUI_xxDAVICOM	0x006040	/* Davicom Semiconductor */


/*
 * List of known models.  Grouped by oui.
 */

/* Advanced Micro Devices PHYs */
#define	MII_MODEL_xxAMD_79C873	0x0000
#define	MII_STR_xxAMD_79C873	"Am79C873 10/100 media interface"
#define	MII_MODEL_AMD_79c973phy	0x0036
#define	MII_STR_AMD_79c973phy	"Am79c973 internal PHY"

/* Davicom Semiconductor PHYs */
#define	MII_MODEL_xxDAVICOM_DM9101	0x0000
#define	MII_STR_xxDAVICOM_DM9101	"DM9101 10/100 media interface"

/* Integrated Circuit Systems PHYs */
#define	MII_MODEL_xxICS_1890	0x0002
#define	MII_STR_xxICS_1890	"ICS1890 10/100 media interface"

/* Intel PHYs */
#define	MII_MODEL_xxINTEL_I82553	0x0000
#define	MII_STR_xxINTEL_I82553	"i82553 10/100 media interface"
#define	MII_MODEL_INTEL_I82555	0x0015
#define	MII_STR_INTEL_I82555	"i82555 10/100 media interface"
#define	MII_MODEL_INTEL_I82553	0x0035
#define	MII_STR_INTEL_I82553	"i82553 10/100 media interface"

/* Level 1 PHYs */
#define	MII_MODEL_xxLEVEL1_LXT970	0x0000
#define	MII_STR_xxLEVEL1_LXT970	"LXT970 10/100 media interface"

/* National Semiconductor PHYs */
#define	MII_MODEL_NATSEMI_DP83840	0x0000
#define	MII_STR_NATSEMI_DP83840	"DP83840 10/100 media interface"
#define	MII_MODEL_NATSEMI_DP83843	0x0001
#define	MII_STR_NATSEMI_DP83843	"DP83843 10/100 media interface"

/* Quality Semiconductor PHYs */
#define	MII_MODEL_QUALSEMI_QS6612	0x0000
#define	MII_STR_QUALSEMI_QS6612	"QS6612 10/100 media interface"

/* Seeq PHYs */
#define	MII_MODEL_xxSEEQ_80220	0x0003
#define	MII_STR_xxSEEQ_80220	"Seeq 80220 10/100 media interface"
#define	MII_MODEL_xxSEEQ_84220	0x0004
#define	MII_STR_xxSEEQ_84220	"Seeq 84220 10/100 media interface"

/* Silicon Integrated Systems PHYs */
#define	MII_MODEL_xxSIS_900	0x0000
#define	MII_STR_xxSIS_900	"SiS 900 10/100 media interface"

/* Texas Instruments PHYs */
#define	MII_MODEL_xxTI_TLAN10T	0x0001
#define	MII_STR_xxTI_TLAN10T	"ThunderLAN 10baseT media interface"
#define	MII_MODEL_xxTI_100VGPMI	0x0002
#define	MII_STR_xxTI_100VGPMI	"ThunderLAN 100VG-AnyLan media interface"

/* TDK Semiconductor PHYs */
#define	MII_MODEL_TSC_78Q2120	0x0014
#define	MII_STR_TSC_78Q2120	"78Q2120 10/100 media interface"
#define	MII_MODEL_TSC_78Q2121	0x0015
#define	MII_STR_TSC_78Q2121	"78Q2121 100baseTX media interface"
