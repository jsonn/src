/*	$NetBSD: rmixl_obiovar.h,v 1.1.2.8 2010/03/21 21:26:45 cliff Exp $	*/

#ifndef _MIPS_RMI_RMIXL_OBIOVAR_H_
#define _MIPS_RMI_RMIXL_OBIOVAR_H_

#include <dev/pci/pcivar.h>
#include <mips/bus_dma.h>
#include <mips/pci_machdep.h>

struct obio_attach_args {
	bus_space_tag_t	obio_eb_bst;
	bus_space_tag_t	obio_el_bst;
	bus_addr_t	obio_addr;
	bus_size_t	obio_size;
	int		obio_intr;
	int		obio_tmsk;
	unsigned int	obio_mult;
	bus_dma_tag_t	obio_29bit_dmat;
	bus_dma_tag_t	obio_32bit_dmat;
	bus_dma_tag_t	obio_64bit_dmat;
};

typedef struct obio_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_eb_bst;
	bus_space_tag_t		sc_el_bst;
	bus_dma_tag_t		sc_29bit_dmat;
	bus_dma_tag_t		sc_32bit_dmat;
	bus_dma_tag_t		sc_64bit_dmat;
	bus_addr_t		sc_base;
	bus_size_t		sc_size;
} obio_softc_t;

#endif /* _MIPS_RMI_RMIXL_OBIOVAR_H_ */
