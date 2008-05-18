/*	$NetBSD: plumpowerreg.h,v 1.3.130.1 2008/05/18 12:32:03 yamt Exp $ */

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * POWER CONTROLLER
 */
#define	PLUM_POWER_REGBASE		0x7000
#define	PLUM_POWER_REGSIZE		0x1000

/* power control register */
#define PLUM_POWER_PWRCONT_REG		0x000

#define PLUM_POWER_PWRCONT_USBEN	0x00000400
#define PLUM_POWER_PWRCONT_IO5OE	0x00000200
#define PLUM_POWER_PWRCONT_LCDOE	0x00000100
/* EXTPW[0:2] Platform dependent control signal */
#define PLUM_POWER_PWRCONT_EXTPW2	0x00000040
#define PLUM_POWER_PWRCONT_EXTPW1	0x00000020
#define PLUM_POWER_PWRCONT_EXTPW0	0x00000010
#define PLUM_POWER_PWRCONT_IO5PWR	0x00000008
#define PLUM_POWER_PWRCONT_BKLIGHT	0x00000004
#define PLUM_POWER_PWRCONT_LCDPWR	0x00000002
#define PLUM_POWER_PWRCONT_LCDDSP	0x00000001

/* clock control register */
#define PLUM_POWER_CLKCONT_REG		0x004

#define	PLUM_POWER_CLKCONT_USBCLK2	0x00000020
#define	PLUM_POWER_CLKCONT_USBCLK1	0x00000010
#define	PLUM_POWER_CLKCONT_IO5CLK	0x00000008
#define	PLUM_POWER_CLKCONT_SMCLK	0x00000004
#define	PLUM_POWER_CLKCONT_PCCCLK2	0x00000002
#define	PLUM_POWER_CLKCONT_PCCCLK1	0x00000001

/* mask rom control register */
#define PLUM_POWER_MROMCNT_REG		0x008

#define PLUM_POWER_MROMCNT_MROMSL1	0x00000004
#define PLUM_POWER_MROMCNT_MROMSL0	0x00000002
#define PLUM_POWER_MROMCNT_MRMAEN	0x00000001
#define PLUM_POWER_MROMCNT_MROM_8MB	0x0
#define PLUM_POWER_MROMCNT_MROM_4MB	0x1
#define PLUM_POWER_MROMCNT_MROM_16MB	0x2

/* input signal enable register (MCS access) */
#define PLUM_POWER_INPENA_REG		0x00c
#define PLUM_POWER_INPENA		0x00000001

/* reset control register (I/O bus)*/
#define PLUM_POWER_RESETC_REG		0x010
/* Active High control */
#define PLUM_POWER_RESETC_IO5CL1	0x00000002
/* Active Low control */
#define PLUM_POWER_RESETC_IO5CL0	0x00000001

#define PLUM_POWER_TESTMD_REG		0x100
