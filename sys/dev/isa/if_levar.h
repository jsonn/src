/*	$NetBSD: if_levar.h,v 1.9.4.1 1997/09/16 03:50:18 thorpej Exp $	*/

/*
 * LANCE Ethernet driver header file
 *
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * Copyright (C) 1993, Paul Richards. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 */

/* Board types */
#define	BICC		1
#define	BICC_RDP	0xc
#define	BICC_RAP	0xe

#define	NE2100		2
#define	PCnet_ISA	4
#define	NE2100_RDP	0x10
#define	NE2100_RAP	0x12

#define	DEPCA		3
#define	DEPCA_CSR	0x0
#define	DEPCA_CSR_SHE		0x80	/* Shared memory enabled */
#define	DEPCA_CSR_LOW32K	0x40	/* Map lower 32K chunk */
#define	DEPCA_CSR_DUM		0x08	/* rev E compatibility */
#define	DEPCA_CSR_IM		0x04	/* Interrupt masked */
#define	DEPCA_CSR_IEN		0x02	/* Interrupt enabled */
#define	DEPCA_RDP	0x4
#define	DEPCA_RAP	0x6
#define	DEPCA_ADP	0xc

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * ethercom.ec_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct le_softc {
	struct	am7990_softc sc_am7990;	/* glue to MI code */

	void	*sc_ih;
	bus_space_tag_t sc_iot;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_ioh;
	bus_space_handle_t sc_memh;
	bus_dma_tag_t	sc_dmat;	/* DMA glue for non-DEPCA */
	bus_dmamap_t	sc_dmam;
	int	sc_card;
	int	sc_rap, sc_rdp;		/* offsets to LANCE registers */
};
