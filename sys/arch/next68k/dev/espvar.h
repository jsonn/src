/*	$NetBSD: espvar.h,v 1.18.2.1 2008/05/18 12:32:32 yamt Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

struct esp_softc {
	struct ncr53c9x_softc sc_ncr53c9x;	/* glue to MI code */
	struct nextdma_softc *sc_dma;
	bus_space_tag_t	sc_bst;				
	bus_space_handle_t sc_bsh;	/* the device registers */

	uint8_t **sc_dmaaddr;		/* saved argument to esp_dma_setup */
	size_t *sc_dmalen;		/* saved argument to esp_dma_setup */
	size_t sc_dmasize;		/* saved argument to esp_dma_setup */
	int sc_datain;			/* saved argument to esp_dma_setup */

#define ESP_LOADED_MAIN   (0x01)
#define ESP_LOADED_TAIL   (0x02)
#define ESP_UNLOADED_MAIN (0x10)
#define ESP_UNLOADED_TAIL (0x20)

	int sc_loaded;			/* used by continue callback to remember
					 * which dmamaps are already loaded.
					 */

	/* To deal with begin alignment problems, we stuff the fifo
	 * with a begin buffer
	 */
	uint8_t *sc_begin;	/* pointer to start io buf, NULL if invalid */
	size_t sc_begin_size;		/*  */

	bus_dmamap_t sc_main_dmamap;	/* I/O DMA map */
	uint8_t *sc_main;	/* pointer to main io buf, NULL if invalid */
	size_t sc_main_size;	/* aligned length of main io buf we are using */

	/* To deal with end alignment problems, we copy the end of the DMA
	 * buffer into a "tail" buffer that we can control more carefully.
	 * We then chain this extra buffer onto the end.
	 */
#define ESP_DMA_OVERRUN 0x30 /* 3*DMA_ENDALIGNMENT */
#define ESP_DMA_MAXTAIL 128
#define ESP_DMA_TAILBUFSIZE (ESP_DMA_MAXTAIL+2*DMA_ENDALIGNMENT+ESP_DMA_OVERRUN)
	bus_dmamap_t  sc_tail_dmamap;
	uint8_t *sc_tail;	/* pointer into sc_tailbuf, NULL if invalid */
	size_t  sc_tail_size;	/* aligned length of tailbuf we are using */
	uint8_t sc_tailbuf[ESP_DMA_TAILBUFSIZE];
};

#ifndef ESP_MAX_DMASIZE
#define ESP_MAX_DMASIZE MAX_DMASIZE
#endif
