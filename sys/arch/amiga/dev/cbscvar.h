/*	$NetBSD: cbscvar.h,v 1.3.4.1 2010/03/11 15:02:00 yamt Exp $	*/

/*
 * Copyright (c) 1997 Michael L. Hitch.
 * All rights reserved.
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
 */

struct cbsc_softc {
	struct ncr53c9x_softc	sc_ncr53c9x;	/* glue to MI code */

	struct isr	sc_isr;			/* Interrupt chain struct */

	volatile uint8_t *sc_reg;		/* the registers */
	volatile uint8_t *sc_dmabase;

	int		sc_active;		/* Pseudo-DMA state vars */
	int		sc_datain;
	int		sc_tc;
	size_t		sc_dmasize;
	size_t		sc_dmatrans;
	uint8_t		**sc_dmaaddr;
	size_t		*sc_pdmalen;
	paddr_t		sc_pa;

	uint8_t		sc_pad1[18];		/* XXX */
	uint8_t		sc_alignbuf[256];
	uint8_t		sc_pad2[16];
	uint8_t		sc_hardbits;
	uint8_t		sc_portbits;
	uint8_t		sc_xfr_align;
};

#define CBSC_HB_CREQ		0x80

#define CBSC_PB_LONG		0x20
#define CBSC_PB_WRITE		0x40
#define CBSC_PB_LED		0x80
