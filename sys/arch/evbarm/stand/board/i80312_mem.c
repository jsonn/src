/*	$NetBSD: i80312_mem.c,v 1.3.2.1 2006/02/01 14:51:26 yamt Exp $	*/

/*
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file provides the mem_init() function for boards using the
 * Intel i80312 companion I/O chip.
 */

#include <sys/types.h>
#include <lib/libsa/stand.h>

#include <arm/xscale/i80312reg.h>

#include "board.h"

#define	INL(x)		*((volatile uint32_t *) \
			  (I80312_PMMR_BASE + I80312_MEM_BASE + (x)))

void
mem_init(void)
{
	uint32_t sdbr, sbr0, sbr1;
	uint32_t bank0, bank1;
	uint32_t start, size, heap;

	sdbr = INL(I80312_MEM_SB);
	sbr0 = INL(I80312_MEM_SB0);
	sbr1 = INL(I80312_MEM_SB1);

	start = sdbr;

	sdbr = (sdbr >> 25) & 0xf;

	sbr0 = ((sbr0 >> 3) & 0x1f) - sdbr;
	sbr1 = ((sbr1 >> 3) & 0x1f) - sbr0;

	bank0 = sbr0 << 25;
	bank1 = sbr1 << 25;

	size = bank0 + bank1;

	heap = (start + size) - BOARD_HEAP_SIZE;

	printf(">> RAM 0x%x - 0x%x, heap at 0x%x\n",
	    start, (start + size) - 1, heap);
	setheap((void *)heap, (void *)(start + size - 1));
}
