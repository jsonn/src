/*	$NetBSD: hd64465reg.h,v 1.1.8.2 2002/03/16 15:58:07 jdolecek Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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
 * HD64465 power managerment and system configuration register.
 */
/* System Module Standby Register */
#define HD64465_SMSCR		0xb0000000
#define   SMSCR_PS2ST		  0x4000
#define   SMSCR_ADCST		  0x1000
#define   SMSCR_UARTST		  0x0800
#define   SMSCR_SCDIST		  0x0200
#define   SMSCR_PPST		  0x0100
#define   SMSCR_PC0ST		  0x0040
#define   SMSCR_PC1ST		  0x0020
#define   SMSCR_AFEST		  0x0010
#define   SMSCR_TM0ST		  0x0008
#define   SMSCR_TM1ST		  0x0004
#define   SMSCR_IRDAST		  0x0002
#define   SMSCR_KBCST		  0x0001

/* System Configuration Register */
#define HD64465_SCONFR		0xb0000002
/* System Bus Control Register */
#define HD64465_SBCR		0xb0000004
/* System Peripheral Clock Control Register */
#define HD64465_SPCCR		0xb0000006
#define   SPCCR_ADCCLK		  0x8000
#define   SPCCR_UARTCLK		  0x2000
#define   SPCCR_PPCLK		  0x1000
#define   SPCCR_FIRCLK		  0x0800
#define   SPCCR_SIRCLK		  0x0400
#define   SPCCR_SCDICLK		  0x0200
#define   SPCCR_KBCCLK		  0x0100
#define   SPCCR_USBCLK		  0x0080
#define   SPCCR_AFECLK		  0x0040
#define   SPCCR_UCKOSC		  0x0002
#define   SPCCR_AFEOSC		  0x0001

/* System Peripheral S/W Reset Control Register */
#define HD64465_SPSRCR		0xb0000008
/* System PLL Control Register */
#define HD64465_SPLLCR		0xb000000a
/* System Revision Register */
#define HD64465_SRR		0xb000000c
/* System Test Mode Control Register */
#define HD64465_STMCR		0xb000000e
/* System Device ID Register */
#define HD64465_SDIDR		0xb0000010	/* ro 0x8122 */
/* System Debug Port Control Register */
#define HD64465_SDPCR		0xb0000ff0
