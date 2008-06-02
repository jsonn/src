/* $NetBSD: amiga_bus_simple_4.c,v 1.4.118.1 2008/06/02 13:21:50 mjf Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(1, "$NetBSD: amiga_bus_simple_4.c,v 1.4.118.1 2008/06/02 13:21:50 mjf Exp $");

#define AMIGA_SIMPLE_BUS_STRIDE 4		/* 1 byte per long */
#define AMIGA_SIMPLE_BUS_WORD_METHODS

#include "simple_busfuncs.c"

/*
 * Little-endian word methods.
 * Stream access does not swap, used for 16-bit wide transfers of byte streams.
 * Non-stream access swaps bytes.
 * XXX Only *_multi_2 transfers currently swap bytes XXX
 */

bsrm(oabs(bsrm2_swap_), u_int16_t);
bswm(oabs(bswm2_swap_), u_int16_t);

void
oabs(bsrm2_swap_)(bus_space_handle_t handle, bus_size_t offset,
	 	  u_int16_t *pointer, bus_size_t count)
{
	volatile u_int16_t *p;

	p = (volatile u_int16_t *)(handle + offset * AMIGA_SIMPLE_BUS_STRIDE);

	while (count > 0) {
		*pointer++ = bswap16(*p);
		--count;
	}
}

void
oabs(bswm2_swap_)(bus_space_handle_t handle, bus_size_t offset,
		  const u_int16_t *pointer, bus_size_t count)
{
	volatile u_int16_t *p;

	p = (volatile u_int16_t *)(handle + offset * AMIGA_SIMPLE_BUS_STRIDE);

	while (count > 0) {
		*p = bswap16(*pointer);
		++pointer;
		--count;
	}
}

const struct amiga_bus_space_methods amiga_bus_stride_4swap = {

        oabs(bsm_),
        oabs(bsms_),
        oabs(bsu_),
        0,
        0,

        oabs(bsr1_),
        oabs(bsw1_),
        oabs(bsrm1_),
        oabs(bswm1_),
        oabs(bsrr1_),
        oabs(bswr1_),
        oabs(bssr1_),
        oabs(bscr1_),

        oabs(bsr2_),		/* XXX swap? */
        oabs(bsw2_),		/* XXX swap? */
        oabs(bsr2_),
        oabs(bsw2_),
        oabs(bsrm2_swap_),
        oabs(bswm2_swap_),
        oabs(bsrm2_),
        oabs(bswm2_),
        oabs(bsrr2_),		/* XXX swap? */
        oabs(bswr2_),		/* XXX swap? */
        oabs(bsrr2_),
        oabs(bswr2_),
        oabs(bssr2_),		/* XXX swap? */
        oabs(bscr2_)		/* XXX swap? */
};
