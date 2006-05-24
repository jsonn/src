/*	$NetBSD: pci_machdep.c,v 1.19.2.2 2006/05/24 10:56:39 yamt Exp $	*/

/*
 * Copyright (c) 2000 Soren S. Jorvang.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pci_machdep.c,v 1.19.2.2 2006/05/24 10:56:39 yamt Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/extent.h>

#define _COBALT_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciconf.h>

#include <cobalt/dev/gtreg.h>

/*
 * PCI doesn't have any special needs; just use
 * the generic versions of these functions.
 */
struct cobalt_bus_dma_tag pci_bus_dma_tag = {
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	_bus_dmamap_sync,
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

void
pci_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
	/* XXX */

	return;
}

int
pci_bus_maxdevs(pci_chipset_tag_t pc, int busno)
{

	return 32;
}

pcitag_t
pci_make_tag(pci_chipset_tag_t pc, int bus, int device, int function)
{

	return (bus << 16) | (device << 11) | (function << 8);
}

void
pci_decompose_tag(pci_chipset_tag_t pc, pcitag_t tag, int *bp, int *dp, int *fp)
{

	if (bp != NULL)
		*bp = (tag >> 16) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 11) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 8) & 0x07;
}

pcireg_t
pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	pcireg_t data;
	int bus, dev, func;

	pci_decompose_tag(pc, tag, &bus, &dev, &func);

	/*
	 * 2700 hardware wedges on accesses to device 6.
	 */
	if (bus == 0 && dev == 6)
		return 0;
	/*
	 * 2800 hardware wedges on accesses to device 31.
	 */
	if (bus == 0 && dev == 31)
		return 0;

	bus_space_write_4(pc->pc_bst, pc->pc_bsh, GT_PCICFG_ADDR,
	    PCICFG_ENABLE | tag | reg);
	data = bus_space_read_4(pc->pc_bst, pc->pc_bsh, GT_PCICFG_DATA);
	bus_space_write_4(pc->pc_bst, pc->pc_bsh, GT_PCICFG_ADDR, 0);

	return data;
}

void
pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t data)
{

	bus_space_write_4(pc->pc_bst, pc->pc_bsh, GT_PCICFG_ADDR,
	    PCICFG_ENABLE | tag | reg);
	bus_space_write_4(pc->pc_bst, pc->pc_bsh, GT_PCICFG_DATA, data);
	bus_space_write_4(pc->pc_bst, pc->pc_bsh, GT_PCICFG_ADDR, 0);
}

int
pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t intrtag = pa->pa_intrtag;
	int pin = pa->pa_intrpin;
	int line = pa->pa_intrline;
	int bus, dev, func;

	pci_decompose_tag(pc, intrtag, &bus, &dev, &func);

	/*
	 * The interrupt lines of the two Tulips are connected
	 * directly to the CPU.
	 */

	if (bus == 0 && dev == 7 && pin == PCI_INTERRUPT_PIN_A)
		*ihp = 16 + 1;
	else if (bus == 0 && dev == 12 && pin == PCI_INTERRUPT_PIN_A)
		*ihp = 16 + 2;
	else
		*ihp = line;

	return 0;
}

const char *
pci_intr_string(pci_chipset_tag_t pc, pci_intr_handle_t ih)
{
	static char irqstr[8];

	if (ih >= 16)
		sprintf(irqstr, "level %d", ih - 16);
	else
		sprintf(irqstr, "irq %d", ih);

	return irqstr;
}

const struct evcnt *
pci_intr_evcnt(pci_chipset_tag_t pc, pci_intr_handle_t ih)
{

	/* XXX for now, no evcnt parent reported */
	return NULL;
}

void *
pci_intr_establish(pci_chipset_tag_t pc, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg)
{

	if (ih >= 16)
		return cpu_intr_establish(ih - 16, level, func, arg);
	else
		return icu_intr_establish(ih, IST_LEVEL, level, func, arg);
}

void
pci_intr_disestablish(pci_chipset_tag_t pc, void *cookie)
{

	/* Try both, only the valid one will disestablish. */
	cpu_intr_disestablish(cookie);
	icu_intr_disestablish(cookie);
}

void
pci_conf_interrupt(pci_chipset_tag_t pc, int bus, int dev, int pin, int swiz,
    int *iline)
{

	/* not yet... */
}

int
pci_conf_hook(pci_chipset_tag_t pc, int bus, int dev, int func, pcireg_t id)
{

	/* ignore bogus IDs */
	if (PCI_VENDOR(id) == 0)
		return 0;

	/* 2700 hardware wedges on accesses to device 6. */
	if (bus == 0 && dev == 6)
		return 0;

	/* 2800 hardware wedges on accesses to device 31. */
	if (bus == 0 && dev == 31)
		return 0;

	/* Don't configure the bridge and PCI probe. */ 
	if (PCI_VENDOR(id) == PCI_VENDOR_GALILEO &&
	    PCI_PRODUCT(id) == PCI_PRODUCT_GALILEO_GT64011)
	        return 0;

	/* Don't configure on-board VIA VT82C586 (pcib, viaide, uhci) */
	if (bus == 0 && dev == 9)
		return 0;

	return PCI_CONF_DEFAULT & ~(PCI_COMMAND_SERR_ENABLE |
	    PCI_COMMAND_PARITY_ENABLE);
}
