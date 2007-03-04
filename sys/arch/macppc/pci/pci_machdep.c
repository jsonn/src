/*	$NetBSD: pci_machdep.c,v 1.32.2.1 2007/03/04 12:21:28 bouyer Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Machine-specific functions for PCI autoconfiguration.
 *
 * On PCs, there are two methods of generating PCI configuration cycles.
 * We try to detect the appropriate mechanism for this machine and set
 * up a few function pointers to access the correct method directly.
 *
 * The configuration method can be hard-coded in the config file by
 * using `options PCI_CONF_MODE=N', where `N' is the configuration mode
 * as defined section 3.6.4.1, `Generating Configuration Cycles'.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pci_machdep.c,v 1.32.2.1 2007/03/04 12:21:28 bouyer Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#define _MACPPC_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <machine/autoconf.h>
#include <machine/pio.h>
#include <machine/intr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/ppbreg.h>
#include <dev/pci/pcidevs.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>

static void fixpci __P((int, pci_chipset_tag_t));
static int find_node_intr __P((int, u_int32_t *, u_int32_t *));
static void fix_cardbus_bridge(int, pci_chipset_tag_t, pcitag_t);

/*
 * PCI doesn't have any special needs; just use the generic versions
 * of these functions.
 */
struct macppc_bus_dma_tag pci_bus_dma_tag = {
	0,			/* _bounce_thresh */
	_bus_dmamap_create, 
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	NULL,			/* _dmamap_sync */
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

void
pci_attach_hook(parent, self, pba)
	struct device *parent, *self;
	struct pcibus_attach_args *pba;
{
	pci_chipset_tag_t pc = pba->pba_pc;
	int bus = pba->pba_bus;
	int node, nn, sz;
	int32_t busrange[2];

	for (node = pc->node; node; node = nn) {
		sz = OF_getprop(node, "bus-range", busrange, 8);
		if (sz == 8 && busrange[0] == bus) {
			fixpci(node, pc);
			return;
		}
		if ((nn = OF_child(node)) != 0)
			continue;
		while ((nn = OF_peer(node)) == 0) {
			node = OF_parent(node);
			if (node == pc->node)
				return;		/* not found */
		}
	}
}

int
pci_bus_maxdevs(pc, busno)
	pci_chipset_tag_t pc;
	int busno;
{

	/*
	 * Bus number is irrelevant.  Configuration Mechanism 1 is in
	 * use, can have devices 0-32 (i.e. the `normal' range).
	 */
	return 32;
}

pcitag_t
pci_make_tag(pc, bus, device, function)
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	pcitag_t tag;

	if (bus >= 256 || device >= 32 || function >= 8)
		panic("pci_make_tag: bad request");

	/* XXX magic number */
	tag = 0x80000000 | (bus << 16) | (device << 11) | (function << 8);

	return tag;
}

void
pci_decompose_tag(pc, tag, bp, dp, fp)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int *bp, *dp, *fp;
{

	if (bp != NULL)
		*bp = (tag >> 16) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 11) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 8) & 0x07;
}

pcireg_t
pci_conf_read(pc, tag, reg)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
{

	return (*pc->conf_read)(pc, tag, reg);
}

void
pci_conf_write(pc, tag, reg, data)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
	pcireg_t data;
{

	(*pc->conf_write)(pc, tag, reg, data);
}

int
pci_intr_map(pa, ihp)
	struct pci_attach_args *pa;
	pci_intr_handle_t *ihp;
{
	int pin = pa->pa_intrpin;
	int line = pa->pa_intrline;
	
#if DEBUG
	printf("%s: pin: %d, line: %d\n", __FUNCTION__, pin, line);
#endif

	if (pin == 0) {
		/* No IRQ used. */
		printf("pci_intr_map: interrupt pin %d\n", pin);
		goto bad;
	}

	if (pin > 4) {
		printf("pci_intr_map: bad interrupt pin %d\n", pin);
		goto bad;
	}

	/*
	 * Section 6.2.4, `Miscellaneous Functions', says that 255 means
	 * `unknown' or `no connection' on a PC.  We assume that a device with
	 * `no connection' either doesn't have an interrupt (in which case the
	 * pin number should be 0, and would have been noticed above), or
	 * wasn't configured by the BIOS (in which case we punt, since there's
	 * no real way we can know how the interrupt lines are mapped in the
	 * hardware).
	 *
	 * XXX
	 * Since IRQ 0 is only used by the clock, and we can't actually be sure
	 * that the BIOS did its job, we also recognize that as meaning that
	 * the BIOS has not configured the device.
	 */
	if (line == 0 || line == 255) {
		printf("pci_intr_map: no mapping for pin %c\n", '@' + pin);
		goto bad;
	} else {
		if (line >= ICU_LEN) {
			printf("pci_intr_map: bad interrupt line %d\n", line);
			goto bad;
		}
	}

	*ihp = line;
	return 0;

bad:
	*ihp = -1;
	return 1;
}

const char *
pci_intr_string(pc, ih)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
{
	static char irqstr[8];		/* 4 + 2 + NULL + sanity */

	if (ih == 0 || ih >= ICU_LEN)
		panic("pci_intr_string: bogus handle 0x%x", ih);

	sprintf(irqstr, "irq %d", ih);
	return (irqstr);
	
}

const struct evcnt *
pci_intr_evcnt(pc, ih)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
{

	/* XXX for now, no evcnt parent reported */
	return NULL;
}

void *
pci_intr_establish(pc, ih, level, func, arg)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	int level, (*func) __P((void *));
	void *arg;
{

	if (ih == 0 || ih >= ICU_LEN)
		panic("pci_intr_establish: bogus handle 0x%x", ih);

	return intr_establish(ih, IST_LEVEL, level, func, arg);
}

void
pci_intr_disestablish(pc, cookie)
	pci_chipset_tag_t pc;
	void *cookie;
{

	intr_disestablish(cookie);
}

#define pcibus(x) \
	(((x) & OFW_PCI_PHYS_HI_BUSMASK) >> OFW_PCI_PHYS_HI_BUSSHIFT)
#define pcidev(x) \
	(((x) & OFW_PCI_PHYS_HI_DEVICEMASK) >> OFW_PCI_PHYS_HI_DEVICESHIFT)
#define pcifunc(x) \
	(((x) & OFW_PCI_PHYS_HI_FUNCTIONMASK) >> OFW_PCI_PHYS_HI_FUNCTIONSHIFT)

void
fixpci(parent, pc)
	int parent;
	pci_chipset_tag_t pc;
{
	int node;
	pcitag_t tag;
	pcireg_t csr, intr, id, cr;
	int len, i, ilen;
	int32_t irqs[4];
	struct {
		u_int32_t phys_hi, phys_mid, phys_lo;
		u_int32_t size_hi, size_lo;
	} addr[8];
	struct {
		u_int32_t phys_hi, phys_mid, phys_lo;
		u_int32_t icells[5];
	} iaddr;

	len = OF_getprop(parent, "#interrupt-cells", &ilen, sizeof(ilen));
	if (len < 0)            
		ilen = 0;       
	for (node = OF_child(parent); node; node = OF_peer(node)) {
		len = OF_getprop(node, "assigned-addresses", addr,
				 sizeof(addr));
		if (len < (int)sizeof(addr[0]))
			continue;

		tag = pci_make_tag(pc, pcibus(addr[0].phys_hi),
				   pcidev(addr[0].phys_hi),
				   pcifunc(addr[0].phys_hi));

		/*
		 * Make sure the IO and MEM enable bits are set in the CSR.
		 */
		csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
		csr &= ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE);

		for (i = 0; i < len / sizeof(addr[0]); i++) {
			switch (addr[i].phys_hi & OFW_PCI_PHYS_HI_SPACEMASK) {
			case OFW_PCI_PHYS_HI_SPACE_IO:
				csr |= PCI_COMMAND_IO_ENABLE;
				break;

			case OFW_PCI_PHYS_HI_SPACE_MEM32:
			case OFW_PCI_PHYS_HI_SPACE_MEM64:
				csr |= PCI_COMMAND_MEM_ENABLE;
				break;
			}
		}

		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);

		/*
		 * Make sure the line register is programmed with the
		 * interrupt mapping.
		 */
		if (ilen == 0) {
			/*
			 * Early Apple OFW implementation don't handle
			 * interrupts as defined by the OFW PCI bindings.
			 */
			len = OF_getprop(node, "AAPL,interrupts", irqs, 4);
		} else {
			iaddr.phys_hi = addr[0].phys_hi;
			iaddr.phys_mid = addr[0].phys_mid;
			iaddr.phys_lo = addr[0].phys_lo;
			/*
			 * Thankfully, PCI can only have one entry in its
			 * "interrupts" property.
			 */
			len = OF_getprop(node, "interrupts", &iaddr.icells[0],
			    4*ilen);
			if (len != 4*ilen)
				continue;
			len = find_node_intr(node, &iaddr.phys_hi, irqs);
		}
		if (len <= 0) {
			/*
			 * If we still don't have an interrupt, try one
			 * more time.  This case covers devices behind the
			 * PCI-PCI bridge in a UMAX S900 or similar (9500?)
			 * system.  These slots all share the bridge's
			 * interrupt.
			 */
			len = find_node_intr(node, &addr[0].phys_hi, irqs);
			if (len <= 0)
				continue;
		}

		/* 
		 * For PowerBook 2400, 3400 and original G3:
		 * check if we have a 2nd ohare PIC - if so frob the built-in 
		 * tlp's IRQ to 60
		 * first see if we have something on bus 0 device 13 and if 
		 * it's a DEC 21041
		 */
		id = pci_conf_read(pc, tag, PCI_ID_REG);
		if ((tag == pci_make_tag(pc, 0, 13, 0)) &&
		    (PCI_VENDOR(id) == PCI_VENDOR_DEC) && 
		    (PCI_PRODUCT(id) == PCI_PRODUCT_DEC_21041)) {

			/* now look for the 2nd ohare */
			if (OF_finddevice("/bandit/pci106b,7") != -1) {

				irqs[0] = 60;
				printf("\nohare: frobbing tlp IRQ to 60");
			}
		}

		intr = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);
		intr &= ~PCI_INTERRUPT_LINE_MASK;
		intr |= irqs[0] & PCI_INTERRUPT_LINE_MASK;
		pci_conf_write(pc, tag, PCI_INTERRUPT_REG, intr);

		/* fix secondary bus numbers on CardBus bridges */
		cr = pci_conf_read(pc, tag, PCI_CLASS_REG);
		if ((PCI_CLASS(cr) == PCI_CLASS_BRIDGE) &&
		    (PCI_SUBCLASS(cr) == PCI_SUBCLASS_BRIDGE_CARDBUS)) {
			uint32_t bi, busid;

			/*
			 * we found a CardBus bridge. Check if the bus number
			 * is sane
			 */
			bi = pci_conf_read(pc, tag, PPB_REG_BUSINFO);
			busid = bi & 0xff;
			if (busid == 0) {
				fix_cardbus_bridge(node, pc, tag);
			}
		}
	}
}

static void
fix_cardbus_bridge(int node, pci_chipset_tag_t pc, pcitag_t tag)
{
	uint32_t bus_number;
	pcireg_t bi;
	int bus, dev, fn, ih, len;
	char path[256];

	len = OF_package_to_path(node, path, sizeof(path));
	path[len] = 0;

	ih = OF_open(path);
	OF_call_method("load-ata", ih, 0, 0);
	OF_close(ih);
	
	if (OF_getprop(node, "AAPL,bus-id", &bus_number, sizeof(bus_number))
	    > 0) {

		printf("\n%s: fixing bus number to %d", path, bus_number);
		pci_decompose_tag(pc, tag, &bus, &dev, &fn);
		bi = pci_conf_read(pc, tag, PPB_REG_BUSINFO);
		bi &= 0xff000000;
		/* XXX subordinate is always 32 here */
		bi |= (bus & 0xff) | (bus_number << 8) | 0x200000;
		pci_conf_write(pc, tag, PPB_REG_BUSINFO, bi);
	}
}

/*
 * Find PCI IRQ of the node from OF tree.
 */
int
find_node_intr(node, addr, intr)
	int node;
	u_int32_t *addr, *intr;
{
	int parent, len, mlen, iparent;
	int match, i;
	u_int32_t map[160];
	const u_int32_t *mp;
	u_int32_t imapmask[8], maskedaddr[8];
	u_int32_t acells, icells;
	char name[32];

	/* XXXSL: 1st check for a  interrupt-parent property */
        if (OF_getprop(node, "interrupt-parent", &iparent, sizeof(iparent)) == sizeof(iparent))
	{
		/* How many cells to specify an interrupt ?? */
		if (OF_getprop(iparent, "#interrupt-cells", &icells, 4) != 4)
			return -1;

		if (OF_getprop(node, "interrupts", &map, sizeof(map)) != (icells * 4))
			return -1;

		memcpy(intr, map, icells * 4);
		return (icells * 4);
	}

	parent = OF_parent(node);
	len = OF_getprop(parent, "interrupt-map", map, sizeof(map));
	mlen = OF_getprop(parent, "interrupt-map-mask", imapmask,
	    sizeof(imapmask));

	if (mlen != -1)
		memcpy(maskedaddr, addr, mlen);
again:
	if (len == -1 || mlen == -1)
		goto nomap;

#ifdef DIAGNOSTIC
	if (mlen == sizeof(imapmask)) {
		printf("interrupt-map too long\n");
		return -1;
	}
#endif

	/* mask addr by "interrupt-map-mask" */
	for (i = 0; i < mlen / 4; i++)
		maskedaddr[i] &= imapmask[i];

	mp = map;
	i = 0;
	while (len > mlen) {
		match = memcmp(maskedaddr, mp, mlen);
		mp += mlen / 4;
		len -= mlen;

		/*
		 * We must read "#address-cells" and "#interrupt-cells" each
		 * time because each interrupt-parent may be different.
		 */
		iparent = *mp++;
		len -= 4;
		i = OF_getprop(iparent, "#address-cells", &acells, 4);
		if (i <= 0)
			acells = 0;
		else if (i != 4)
			return -1;
		if (OF_getprop(iparent, "#interrupt-cells", &icells, 4) != 4)
			return -1;

		/* Found. */
		if (match == 0) {
			/*
			 * We matched on address/interrupt, but are we done?
			 */
			if (acells == 0) { /* XXX */
				/*
				 * If we are at the interrupt controller,
				 * we are finally done.  Save the result and
				 * return.
				 */
				memcpy(intr, mp, icells * 4);
				return icells * 4;
			}
			/*
			 * We are now at an intermedia interrupt node.  We
			 * need to use its interrupt mask and map the 
			 * supplied address/interrupt via its map.
			 */
			mlen = OF_getprop(iparent, "interrupt-map-mask",
			    imapmask, sizeof(imapmask));
#ifdef DIAGNOSTIC
			if (mlen != (acells + icells)*4) {
				printf("interrupt-map inconsistent (%d, %d)\n",
				    mlen, (acells + icells)*4);
				return -1;
			}
#endif
			memcpy(maskedaddr, mp, mlen);
			len = OF_getprop(iparent, "interrupt-map", map,
			    sizeof(map));
			goto again;
		}

		mp += (acells + icells);
		len -= (acells + icells) * 4;
	}

nomap:
	/*
	 * If the node has no interrupt property and the parent is a
	 * pci-bridge, use parent's interrupt.  This occurs on a PCI
	 * slot.  (e.g. AHA-3940)
	 */
	memset(name, 0, sizeof(name));
	OF_getprop(parent, "name", name, sizeof(name));
	if (strcmp(name, "pci-bridge") == 0) {
		len = OF_getprop(parent, "AAPL,interrupts", intr, 4) ;
		if (len == 4)
			return len;
#if 0
		/*
		 * XXX I don't know what is the correct local address.
		 * XXX Use the first entry for now.
		 */
		len = OF_getprop(parent, "interrupt-map", map, sizeof(map));
		if (len >= 36) {
			addr = &map[5];
			return find_node_intr(parent, addr, intr);
		}
#endif
	}

	/*
	 * If all else fails, attempt to get AAPL, interrupts property.
	 * Grackle, at least, uses this instead of above in some cases.
	 */
	len = OF_getprop(node, "AAPL,interrupts", intr, 4) ;
	if (len == 4)
		return len;

	return -1;
}
