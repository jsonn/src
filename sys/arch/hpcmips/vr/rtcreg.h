/*	$NetBSD: rtcreg.h,v 1.1.1.1 1999/09/16 12:23:32 takemura Exp $	*/

/*-
 * Copyright (c) 1999 Shin Takemura. All rights reserved.
 * Copyright (c) 1999 SATO Kazumi. All rights reserved.
 * Copyright (c) 1999 PocketBSD Project. All rights reserved.
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
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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
 */

#define	SECMIN	((unsigned)60)			/* seconds per minute */
#define	SECHOUR	((unsigned)(60*SECMIN))		/* seconds per hour */
#define	SECDAY	((unsigned)(24*SECHOUR))	/* seconds per day */
#define	SECYR	((unsigned)(365*SECDAY))	/* seconds per common year */

#define	YRREF		1998
#define	MREF		1
#define	DREF		1
#define	LEAPYEAR4(year)	((((year) % 4) == 0 && ((year) % 100) != 0) || ((year%400)) == 0)
#define	LEAPYEAR2(year)	(((year) % 4) == 0)

/*
 *	RTC (Real Time Clock Unit) Registers definitions.
 *		start 0x0B0000C0
 */
#define ETIME_L_REG_W		0x000	/* Elapsed Time L */
#define ETIME_M_REG_W		0x002	/* Elapsed Time M */
#define ETIME_H_REG_W		0x004	/* Elapsed Time H */

#define		ETIME_L_HZ		0x8000	/* 1 HZ */


#define ECMP_L_REG_W		0x008	/* Elapsed Compare L */
#define ECMP_M_REG_W		0x00a	/* Elapsed Compare M */
#define ECMP_H_REG_W		0x00c	/* Elapsed Compare H */


#define RTCL1_L_REG_W		0x010	/* RTC Long 1 L */
#define RTCL1_H_REG_W		0x012	/* RTC Long 1 H */

#define		RTCL1_L_HZ		0x8000	/* 1 HZ */


#define RTCL1_CNT_L_REG_W	0x014	/* RTC Long 1 Count L */
#define RTCL1_CNT_H_REG_W	0x016	/* RTC Long 1 Count H */


#define RTCL2_L_REG_W		0x018	/* RTC Long 2 L */
#define RTCL2_H_REG_W		0x01a	/* RTC Long 2 H */

#define		RTCL2_L_HZ		0x8000	/* 1 HZ */


#define RTCL2_CNT_L_REG_W	0x01c	/* RTC Long 2 Count L */
#define RTCL2_CNT_H_REG_W	0x01e	/* RTC Long 2 Count H */


#define TCLK_L_REG_W		0x100	/* TCLK L */
#define TCLK_H_REG_W		0x102	/* TCLK H */


#define TCLK_CNT_L_REG_W	0x104	/* TCLK Count L */
#define TCLK_CNT_H_REG_W	0x106	/* TCLK Count H */


#define RTCINT_REG_W		0x11e	/* RTC intr reg. */

#define		RTCINT_TCLOCK		(1<<3)	/* TClock */
#define		RTCINT_RTCLONG2		(1<<2)	/* RTC Long 2 */
#define		RTCINT_RTCLONG1		(1<<1)	/* RTC Long 1 */
#define		RTCINT_ELAPSED		(1)	/* Elapsed time */
#define		RTCINT_ALL		(RTCINT_TCLOCK|RTCINT_RTCLONG2|RTCINT_RTCLONG1|RTCINT_ELAPSED)

/* END rtcreg.h */
