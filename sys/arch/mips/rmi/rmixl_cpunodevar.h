/*	$NetBSD: rmixl_cpunodevar.h,v 1.1.2.3 2011/12/24 01:57:54 matt Exp $	*/
/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Cliff Neighbors.
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

#ifndef _ARCH_MIPS_RMI_RMIXL_CPUNODEVAR_H_
#define _ARCH_MIPS_RMI_RMIXL_CPUNODEVAR_H_

struct cpunode_softc {
	device_t	sc_dev;
	u_int		sc_node;
	bus_dma_tag_t	sc_29bit_dmat;
	bus_dma_tag_t	sc_32bit_dmat;
	bus_dma_tag_t	sc_64bit_dmat;
};

struct cpunode_attach_args {
	const char     *na_name;
	int		na_node;
	int		na_core;
	bus_dma_tag_t	na_dmat29;
	bus_dma_tag_t	na_dmat32;
	bus_dma_tag_t	na_dmat64;
};

#endif	/* _ARCH_MIPS_RMI_RMIXL_CPUNODEVAR_H_ */
