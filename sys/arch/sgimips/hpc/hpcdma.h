/*	$NetBSD: hpcdma.h,v 1.1.6.1 2001/11/12 21:17:29 thorpej Exp $	*/

/*
 * Copyright (c) 2001 Wayne Knowles
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wayne Knowles
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

#ifndef _SGIMIPS_HPC_DMA_H
#define _SGIMIPS_HPC_DMA_H

#include <machine/bus.h>

struct hpc_dma_softc {
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	bus_dma_tag_t		sc_dmat;

	u_int32_t		sc_flags;
#define	HPCDMA_READ	0x20		/* direction of transfer */
#define	HPCDMA_LOADED	0x40		/* bus_dmamap loaded */
#define	HPCDMA_ACTIVE	0x80		/* DMA engine is busy */
	u_int32_t		sc_dmacmd;
	int			sc_ndesc;
	bus_dmamap_t		sc_dmamap;
	struct hpc_dma_desc    *sc_desc_kva; /* Virtual address */
	struct hpc_dma_desc    *sc_desc_pa; /* Physical address */
	ssize_t			sc_dlen;    /* number of bytes transfered */
};


void hpcdma_init(struct hpc_attach_args *, struct hpc_dma_softc *, int);
void hpcdma_sglist_create(struct hpc_dma_softc *, bus_dmamap_t);
void hpcdma_cntl(struct hpc_dma_softc *, u_int32_t);
void hpcdma_flush(struct hpc_dma_softc *);

#endif /* _SGIMIPS_HPC_DMA_H */
