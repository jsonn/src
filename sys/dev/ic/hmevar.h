/*	$NetBSD: hmevar.h,v 1.1.4.1 2000/11/20 11:40:34 bouyer Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

#include "rnd.h"

#include <sys/callout.h>
#if NRND > 0
#include <sys/rnd.h>
#endif


struct hme_ring {
	/* Ring Descriptors */
	caddr_t		rb_membase;	/* Packet buffer: CPU address */
	bus_addr_t	rb_dmabase;	/* Packet buffer: DMA address */
	caddr_t		rb_txd;		/* Transmit descriptors */
	bus_addr_t	rb_txddma;	/* DMA address of same */
	caddr_t		rb_rxd;		/* Receive descriptors */
	bus_addr_t	rb_rxddma;	/* DMA address of same */
	caddr_t		rb_txbuf;	/* Transmit buffers */
	caddr_t		rb_rxbuf;	/* Receive buffers */
	int		rb_ntbuf;	/* # of transmit buffers */
	int		rb_nrbuf;	/* # of receive buffers */

	/* Ring Descriptor state */
	int	rb_tdhead, rb_tdtail;
	int	rb_rdtail;
	int	rb_td_nbusy;
};

struct hme_softc {
	struct device	sc_dev;		/* boilerplate device view */
	struct ethercom	sc_ethercom;	/* Ethernet common part */
	struct mii_data	sc_mii;		/* MII media control */
#define sc_media	sc_mii.mii_media/* shorthand */
	struct callout	sc_tick_ch;	/* tick callout */

	/* The following bus handles are to be provided by the bus front-end */
	bus_space_tag_t	sc_bustag;	/* bus tag */
	bus_dma_tag_t	sc_dmatag;	/* bus dma tag */
	bus_dmamap_t	sc_dmamap;	/* bus dma handle */
	bus_space_handle_t sc_seb;	/* HME Global registers */
	bus_space_handle_t sc_erx;	/* HME ERX registers */
	bus_space_handle_t sc_etx;	/* HME ETX registers */
	bus_space_handle_t sc_mac;	/* HME MAC registers */
	bus_space_handle_t sc_mif;	/* HME MIF registers */
	int		sc_burst;	/* DVMA burst size in effect */
	int		sc_phys[2];	/* MII instance -> PHY map */

	int		sc_pci;		/* XXXXX -- PCI buses are LE. */

	/* Ring descriptor */
	struct hme_ring		sc_rb;
#if notused
	void		(*sc_copytobuf) __P((struct hme_softc *,
					     void *, void *, size_t));
	void		(*sc_copyfrombuf) __P((struct hme_softc *,
					      void *, void *, size_t));
#endif

	int			sc_debug;
	void			*sc_sh;		/* shutdownhook cookie */
	u_int8_t		sc_enaddr[ETHER_ADDR_LEN]; /* MAC address */

	/* Special hardware hooks */
	void	(*sc_hwreset) __P((struct hme_softc *));
	void	(*sc_hwinit) __P((struct hme_softc *));

#if NRND > 0
	rndsource_element_t	rnd_source;
#endif
};


void	hme_config __P((struct hme_softc *));
void	hme_reset __P((struct hme_softc *));
int	hme_intr __P((void *));
