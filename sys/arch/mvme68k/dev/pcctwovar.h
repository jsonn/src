/*	$NetBSD: pcctwovar.h,v 1.1.2.1 2000/03/11 20:51:50 scw Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford
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
 *	      This product includes software developed by the NetBSD
 *	      Foundation, Inc. and its contributors.
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

#ifndef	_MVME68K_PCCTWOVAR_H
#define	_MVME68K_PCCTWOVAR_H

/*
 * Structure used to attach PCC devices.
 */
struct pcctwo_attach_args {
	const char	*pa_name;	/* name of device */
	int		pa_ipl;		/* interrupt level */
	bus_dma_tag_t	pa_dmat;	/* DMA tag */
	bus_space_tag_t	pa_bust;	/* Bus tag */
	bus_addr_t	pa_offset;	/* Offset with 'Bus tag' bus space */
};

/* Shorthand for locators. */
#include "locators.h"
#define pcctwocf_ipl	cf_loc[PCCTWOCF_IPL]

/*
 * PCCChip2 driver's soft state structure
 */
struct pcctwo_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_bust;	/* PCCChip2's register tag */
	bus_space_handle_t	sc_bush;	/* PCCChip2's register handle */
};

extern struct pcctwo_softc *sys_pcctwo;

extern void pcctwointr_establish __P((int, int (*)(void *), int, void *));
extern void pcctwointr_disestablish __P((int));

#endif	/* _MVME68K_PCCTWOVAR_H */
