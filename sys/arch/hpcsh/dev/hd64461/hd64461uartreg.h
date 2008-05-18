/*	$NetBSD: hd64461uartreg.h,v 1.4.76.1 2008/05/18 12:32:06 yamt Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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

#ifndef _HPCSH_DEV_HD64461UARTREG_H_
#define _HPCSH_DEV_HD64461UARTREG_H_

/*
 * UART module Register
 */

/* Receiver Buffer Register (r) */
#define HD64461_URBR_REG8			0xb0008000
/* Transmitter Buffer Register (w) */
#define HD64461_UTBR_REG8			0xb0008000
/* Interrupt Enable Register */
#define HD64461_UIER_REG8			0xb0008002
/* Interrupt Identification Register (r) */
#define HD64461_UIIR_REG8			0xb0008004
/* FIFO Control Register (w) */
#define HD64461_UFCR_REG8			0xb0008004
/* Line Control Register */
#define HD64461_ULCR_REG8			0xb0008006
/* Modem Control Register */
#define HD64461_UMCR_REG8			0xb0008008
/* Divisor Latch LSB */
#define HD64461_UDLL_REG8			0xb0008000
/* Divisor Latch MSB */
#define HD64461_UDLM_REG8			0xb0008002
/* Line Status Register */
#define HD64461_ULSR_REG8			0xb000800a
/* Modem Status Register */
#define HD64461_UMSR_REG8			0xb000800c
/* Scratch Pad Register */
#define HD64461_USCR_REG8			0xb000800e

#define	LSR_TXRDY	0x20	/* Transmitter buffer empty */

#endif /* !_HPCSH_DEV_HD64461UARTREG_H_ */
