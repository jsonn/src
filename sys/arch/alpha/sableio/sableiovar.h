/* $NetBSD: sableiovar.h,v 1.1.128.1 2008/06/02 13:21:47 mjf Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#ifndef _ALPHA_SABLEIO_SABLEIOVAR_H_
#define _ALPHA_SABLEIO_SABLEIOVAR_H_

/*
 * Arguments used to attach devices to the Sable STDIO module.
 */
struct sableio_attach_args {
	const char *sa_name;		/* device name */
	bus_addr_t sa_ioaddr;		/* I/O space address */
	int sa_sableirq[2];		/* Sable IRQs */
	int sa_drq;			/* DRQ */

	bus_space_tag_t sa_iot;		/* I/O space tag */
	bus_dma_tag_t sa_dmat;		/* ISA DMA tag */

	isa_chipset_tag_t sa_ic;	/* ISA chipset tag */
	pci_chipset_tag_t sa_pc;	/* PCI chipset tag */
};

isa_chipset_tag_t sableio_pickisa(void);

#endif /* _ALPHA_SABLEIO_SABLEIOVAR_H_ */
