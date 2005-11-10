/*	$NetBSD: cardbus_map.c,v 1.14.16.3 2005/11/10 14:03:54 skrll Exp $	*/

/*
 * Copyright (c) 1999 and 2000
 *      HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by HAYAKAWA Koichi.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cardbus_map.c,v 1.14.16.3 2005/11/10 14:03:54 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/cardbus/cardbusvar.h>

#include <dev/pci/pcireg.h>	/* XXX */

#if defined DEBUG && !defined CARDBUS_MAP_DEBUG
#define CARDBUS_MAP_DEBUG
#endif

#if defined CARDBUS_MAP_DEBUG
#define STATIC
#define DPRINTF(a) printf a
#else
#define STATIC static
#define DPRINTF(a)
#endif


static int cardbus_io_find(cardbus_chipset_tag_t, cardbus_function_tag_t,
				cardbustag_t, int, cardbusreg_t,
				bus_addr_t *, bus_size_t *, int *);
static int cardbus_mem_find(cardbus_chipset_tag_t, cardbus_function_tag_t,
				 cardbustag_t, int, cardbusreg_t,
				 bus_addr_t *, bus_size_t *, int *);

/*
 * static int cardbus_io_find(cardbus_chipset_tag_t cc,
 *			      cardbus_function_tag_t cf, cardbustag_t tag,
 *			      int reg, cardbusreg_t type, bus_addr_t *basep,
 *			      bus_size_t *sizep, int *flagsp)
 * This code is stolen from sys/dev/pci_map.c.
 */
static int
cardbus_io_find(cc, cf, tag, reg, type, basep, sizep, flagsp)
	cardbus_chipset_tag_t cc;
	cardbus_function_tag_t cf;
	cardbustag_t tag;
	int reg;
	cardbusreg_t type;
	bus_addr_t *basep;
	bus_size_t *sizep;
	int *flagsp;
{
	cardbusreg_t address, mask;
	int s;

	/* EXT ROM is able to map on memory space ONLY. */
	if (reg == CARDBUS_ROM_REG) {
		return 1;
	}

	if(reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3)) {
		panic("cardbus_io_find: bad request");
	}

	/*
	 * Section 6.2.5.1, `Address Maps', tells us that:
	 *
	 * 1) The builtin software should have already mapped the device in a
	 * reasonable way.
	 *
	 * 2) A device which wants 2^n bytes of memory will hardwire the bottom
	 * n bits of the address to 0.  As recommended, we write all 1s and see
	 * what we get back.
	 */
	s = splhigh();
	address = cardbus_conf_read(cc, cf, tag, reg);
	cardbus_conf_write(cc, cf, tag, reg, 0xffffffff);
	mask = cardbus_conf_read(cc, cf, tag, reg);
	cardbus_conf_write(cc, cf, tag, reg, address);
	splx(s);

	if (PCI_MAPREG_TYPE(address) != PCI_MAPREG_TYPE_IO) {
		printf("cardbus_io_find: expected type i/o, found mem\n");
		return 1;
	}

	if (PCI_MAPREG_IO_SIZE(mask) == 0) {
		printf("cardbus_io_find: void region\n");
		return 1;
	}

	if (basep != 0) {
		*basep = PCI_MAPREG_IO_ADDR(address);
	}
	if (sizep != 0) {
		*sizep = PCI_MAPREG_IO_SIZE(mask);
	}
	if (flagsp != 0) {
		*flagsp = 0;
	}

	return 0;
}



/*
 * static int cardbus_mem_find(cardbus_chipset_tag_t cc,
 *			       cardbus_function_tag_t cf, cardbustag_t tag,
 *			       int reg, cardbusreg_t type, bus_addr_t *basep,
 *			       bus_size_t *sizep, int *flagsp)
 * This code is stolen from sys/dev/pci_map.c.
 */
static int
cardbus_mem_find(cc, cf, tag, reg, type, basep, sizep, flagsp)
	cardbus_chipset_tag_t cc;
	cardbus_function_tag_t cf;
	cardbustag_t tag;
	int reg;
	cardbusreg_t type;
	bus_addr_t *basep;
	bus_size_t *sizep;
	int *flagsp;
{
	cardbusreg_t address, mask;
	int s;

	if (reg != CARDBUS_ROM_REG &&
	    (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3))) {
		panic("cardbus_mem_find: bad request");
	}

	/*
	 * Section 6.2.5.1, `Address Maps', tells us that:
	 *
	 * 1) The builtin software should have already mapped the device in a
	 * reasonable way.
	 *
	 * 2) A device which wants 2^n bytes of memory will hardwire the bottom
	 * n bits of the address to 0.  As recommended, we write all 1s and see
	 * what we get back.
	 */
	s = splhigh();
	address = cardbus_conf_read(cc, cf, tag, reg);
	cardbus_conf_write(cc, cf, tag, reg, 0xffffffff);
	mask = cardbus_conf_read(cc, cf, tag, reg);
	cardbus_conf_write(cc, cf, tag, reg, address);
	splx(s);

	if (reg != CARDBUS_ROM_REG) {
		/* memory space BAR */

		if (PCI_MAPREG_TYPE(address) != PCI_MAPREG_TYPE_MEM) {
			printf("cardbus_mem_find: expected type mem, found i/o\n");
			return 1;
		}
		if (PCI_MAPREG_MEM_TYPE(address) != PCI_MAPREG_MEM_TYPE(type)) {
			printf("cardbus_mem_find: expected mem type %08x, found %08x\n",
			    PCI_MAPREG_MEM_TYPE(type),
			    PCI_MAPREG_MEM_TYPE(address));
			return 1;
		}
	}

	if (PCI_MAPREG_MEM_SIZE(mask) == 0) {
		printf("cardbus_mem_find: void region\n");
		return 1;
	}

	switch (PCI_MAPREG_MEM_TYPE(address)) {
	case PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_MEM_TYPE_32BIT_1M:
		break;
	case PCI_MAPREG_MEM_TYPE_64BIT:
		printf("cardbus_mem_find: 64-bit memory mapping register\n");
		return 1;
	default:
		printf("cardbus_mem_find: reserved mapping register type\n");
		return 1;
	}

	if (basep != 0) {
		*basep = PCI_MAPREG_MEM_ADDR(address);
	}
	if (sizep != 0) {
		*sizep = PCI_MAPREG_MEM_SIZE(mask);
	}
	if (flagsp != 0) {
		*flagsp = PCI_MAPREG_MEM_PREFETCHABLE(address) ?
		    BUS_SPACE_MAP_PREFETCHABLE : 0;
	}

	return 0;
}




/*
 * int cardbus_mapreg_map(struct cardbus_softc *, int, int, cardbusreg_t,
 *			  int bus_space_tag_t *, bus_space_handle_t *,
 *			  bus_addr_t *, bus_size_t *)
 *    This function maps bus-space on the value of Base Address
 *   Register (BAR) indexed by the argument `reg' (the second argument).
 *   When the value of the BAR is not valid, such as 0x00000000, a new
 *   address should be allocated for the BAR and new address values is
 *   written on the BAR.
 */
int
cardbus_mapreg_map(sc, func, reg, type, busflags, tagp, handlep, basep, sizep)
	struct cardbus_softc *sc;
	int func, reg, busflags;
	cardbusreg_t type;
	bus_space_tag_t *tagp;
	bus_space_handle_t *handlep;
	bus_addr_t *basep;
	bus_size_t *sizep;
{
	cardbus_chipset_tag_t cc = sc->sc_cc;
	cardbus_function_tag_t cf = sc->sc_cf;
	bus_space_tag_t bustag;
#if rbus
	rbus_tag_t rbustag;
#endif
	bus_space_handle_t handle;
	bus_addr_t base;
	bus_size_t size;
	int flags;
	int status = 0;

	cardbustag_t tag = cardbus_make_tag(cc, cf, sc->sc_bus, func);

	DPRINTF(("cardbus_mapreg_map called: %s %x\n", sc->sc_dev.dv_xname,
	   type));

	if (PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_IO) {
		if (cardbus_io_find(cc, cf, tag, reg, type, &base, &size, &flags)) {
			status = 1;
		}
		bustag = sc->sc_iot;
#if rbus
		rbustag = sc->sc_rbus_iot;
#endif
	} else {
		if (cardbus_mem_find(cc, cf, tag, reg, type, &base, &size, &flags)){
			status = 1;
		}
		bustag = sc->sc_memt;
#if rbus
		rbustag = sc->sc_rbus_memt;
#endif
	}
	if (status == 0) {
#if rbus
		bus_addr_t mask = size - 1;
		if (base != 0) {
			mask = 0xffffffff;
		}
		if ((*cf->cardbus_space_alloc)(cc, rbustag, base, size, mask,
		    size, busflags | flags, &base, &handle)) {
			panic("io alloc");
		}
#else
		bus_addr_t start = 0x8300;
		bus_addr_t end = 0x8400;
		if (base != 0) {
			bus_addr_t start = base;
			bus_addr_t end = base + size;
		}
		if (bus_space_alloc(bustag, start, end, size, size, 0, 0, &base, &handle)) {
			panic("io alloc");
		}
#endif
	}
	cardbus_conf_write(cc, cf, tag, reg, base);

	DPRINTF(("cardbus_mapreg_map: physaddr %lx\n", (unsigned long)base));

	if (tagp != 0) {
		*tagp = bustag;
	}
	if (handlep != 0) {
		*handlep = handle;
	}
	if (basep != 0) {
		*basep = base;
	}
	if (sizep != 0) {
		*sizep = size;
	}
	cardbus_free_tag(cc, cf, tag);

	return 0;
}





/*
 * int cardbus_mapreg_unmap(struct cardbus_softc *sc, int func, int reg,
 *			    bus_space_tag_t tag, bus_space_handle_t handle,
 *			    bus_size_t size)
 *
 *   This function releases bus-space region and close memory or io
 *   window on the bridge.
 *
 *  Arguments:
 *   struct cardbus_softc *sc; the pointer to the device structure of cardbus.
 *   int func; the number of function on the device.
 *   int reg; the offset of BAR register.
 */
int
cardbus_mapreg_unmap(sc, func, reg, tag, handle, size)
	struct cardbus_softc *sc;
	int func, reg;
	bus_space_tag_t tag;
	bus_space_handle_t handle;
	bus_size_t size;
{
	cardbus_chipset_tag_t cc = sc->sc_cc;
	cardbus_function_tag_t cf = sc->sc_cf;
	int st = 1;
	cardbustag_t cardbustag;
#if rbus
	rbus_tag_t rbustag;

	if (sc->sc_iot == tag) {
		/* bus space is io space */
		DPRINTF(("%s: unmap i/o space\n", sc->sc_dev.dv_xname));
		rbustag = sc->sc_rbus_iot;
	} else if (sc->sc_memt == tag) {
		/* bus space is memory space */
		DPRINTF(("%s: unmap mem space\n", sc->sc_dev.dv_xname));
		rbustag = sc->sc_rbus_memt;
	} else {
		return 1;
	}
#endif

	cardbustag = cardbus_make_tag(cc, cf, sc->sc_bus, func);

	cardbus_conf_write(cc, cf, cardbustag, reg, 0);

#if rbus
	(*cf->cardbus_space_free)(cc, rbustag, handle, size);
#endif

	cardbus_free_tag(cc, cf, cardbustag);

	return st;
}





/*
 * int cardbus_save_bar(cardbus_devfunc_t);
 *
 *   This function saves the Base Address Registers at the CardBus
 *   function denoted by the argument.
 */
int cardbus_save_bar(ct)
	cardbus_devfunc_t ct;
{
	cardbustag_t tag = Cardbus_make_tag(ct);
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	ct->ct_bar[0] = cardbus_conf_read(cc, cf, tag, CARDBUS_BASE0_REG);
	ct->ct_bar[1] = cardbus_conf_read(cc, cf, tag, CARDBUS_BASE1_REG);
	ct->ct_bar[2] = cardbus_conf_read(cc, cf, tag, CARDBUS_BASE2_REG);
	ct->ct_bar[3] = cardbus_conf_read(cc, cf, tag, CARDBUS_BASE3_REG);
	ct->ct_bar[4] = cardbus_conf_read(cc, cf, tag, CARDBUS_BASE4_REG);
	ct->ct_bar[5] = cardbus_conf_read(cc, cf, tag, CARDBUS_BASE5_REG);

	DPRINTF(("cardbus_save_bar: %x %x\n", ct->ct_bar[0], ct->ct_bar[1]));

	Cardbus_free_tag(ct, tag);

	return 0;
}



/*
 * int cardbus_restore_bar(cardbus_devfunc_t);
 *
 *   This function saves the Base Address Registers at the CardBus
 *   function denoted by the argument.
 */
int cardbus_restore_bar(ct)
	cardbus_devfunc_t ct;
{
	cardbustag_t tag = Cardbus_make_tag(ct);
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;

	cardbus_conf_write(cc, cf, tag, CARDBUS_BASE0_REG, ct->ct_bar[0]);
	cardbus_conf_write(cc, cf, tag, CARDBUS_BASE1_REG, ct->ct_bar[1]);
	cardbus_conf_write(cc, cf, tag, CARDBUS_BASE2_REG, ct->ct_bar[2]);
	cardbus_conf_write(cc, cf, tag, CARDBUS_BASE3_REG, ct->ct_bar[3]);
	cardbus_conf_write(cc, cf, tag, CARDBUS_BASE4_REG, ct->ct_bar[4]);
	cardbus_conf_write(cc, cf, tag, CARDBUS_BASE5_REG, ct->ct_bar[5]);

	Cardbus_free_tag(ct, tag);

	return 0;
}
