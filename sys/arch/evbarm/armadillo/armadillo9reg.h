/*	$NetBSD: armadillo9reg.h,v 1.1.22.2 2006/06/21 14:50:33 yamt Exp $	*/

/*
 * Copyright (c) 2005 HAMAJIMA Katsuomi. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ARMADILLO9REG_H_
#define	_ARMADILLO9REG_H_

/*
 * Memory map and register definitions for the Armadillo-9 single board computer
 */
#define	ARMADILLO9_IO_VBASE	0xf0300000UL
#define	ARMADILLO9_IO8_VBASE	ARMADILLO9_IO_VBASE
#define	ARMADILLO9_IO8_HWBASE	0x10000000UL
#define	ARMADILLO9_IO8_SIZE	0x04000000UL
#define	ARMADILLO9_IO16_VBASE	(ARMADILLO9_IO8_VBASE + ARMADILLO9_IO8_SIZE)
#define	ARMADILLO9_IO16_HWBASE	0x20000000UL
#define	ARMADILLO9_IO16_SIZE	0x04000000UL
#define	ARMADILLO9_ISAIO	0x02000000UL
#define	ARMADILLO9_ISAMEM	0x03000000UL

#endif /* _ARMADILLO9REG_H_ */
