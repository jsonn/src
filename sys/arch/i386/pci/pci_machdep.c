/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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
 *
 *	$Id: pci_machdep.c,v 1.3.2.2 1994/09/06 01:25:23 mycroft Exp $
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/pio.h>

#include <i386/isa/icu.h>
#include <i386/pci/pcivar.h>
#include <i386/pci/pcireg.h>

int pci_mode = -1;

#define	PCI_MODE1_ENABLE	0x80000000UL
#define	PCI_MODE1_ADDRESS_REG	0x0cf8
#define	PCI_MODE1_DATA_REG	0x0cfc

#define	PCI_MODE2_ENABLE_REG	0x0cf8
#define	PCI_MODE2_FORWARD_REG	0x0cfa

pcitag_t
pci_make_tag(bus, device, function)
	int bus, device, function;
{
	pcitag_t tag;

#ifndef PCI_CONF_MODE
	switch (pci_mode) {
	case 1:
		goto mode1;
	case 2:
		goto mode2;
	default:
		panic("pci_make_tag: mode not configured");
	}
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 1)
mode1:
	if (bus >= 256 || device >= 32 || function >= 8)
		panic("pci_make_tag: bad request");

	tag.mode1 = PCI_MODE1_ENABLE |
		    (bus << 16) | (device << 11) | (function << 8);
	return tag;
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
mode2:
	if (bus >= 256 || device >= 16 || function >= 8)
		panic("pci_make_tag: bad request");

	tag.mode2.port = 0xc000 | (device << 8);
	/*
	 * Allow special cycles in the same manner as mode 1, though with
	 * device == 15 rather than 31.
	 */
	tag.mode2.enable = 0xf1 | (function << 1);
	tag.mode2.forward = bus;
	return tag;
#endif
}

pcireg_t
pci_conf_read(tag, reg)
	pcitag_t tag;
	int reg;
{
	pcireg_t data;

#ifndef PCI_CONF_MODE
	switch (pci_mode) {
	case 1:
		goto mode1;
	case 2:
		goto mode2;
	default:
		panic("pci_conf_read: mode not configured");
	}
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 1)
mode1:
	outl(PCI_MODE1_ADDRESS_REG, tag.mode1 | reg);
	data = inl(PCI_MODE1_DATA_REG);
	outl(PCI_MODE1_ADDRESS_REG, 0);
	return data;
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
mode2:
	outb(PCI_MODE2_ENABLE_REG, tag.mode2.enable);
	outb(PCI_MODE2_FORWARD_REG, tag.mode2.forward);
	data = inl(tag.mode2.port | reg);
	outb(PCI_MODE2_ENABLE_REG, 0);
	return data;
#endif
}

void
pci_conf_write(tag, reg, data)
	pcitag_t tag;
	int reg;
	pcireg_t data;
{

#ifndef PCI_CONF_MODE
	switch (pci_mode) {
	case 1:
		goto mode1;
	case 2:
		goto mode2;
	default:
		panic("pci_conf_write: mode not configured");
	}
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 1)
mode1:
	outl(PCI_MODE1_ADDRESS_REG, tag.mode1 | reg);
	outl(PCI_MODE1_DATA_REG, data);
	outl(PCI_MODE1_ADDRESS_REG, 0);
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
mode2:
	outb(PCI_MODE2_ENABLE_REG, tag.mode2.enable);
	outb(PCI_MODE2_FORWARD_REG, tag.mode2.forward);
	outl(tag.mode2.port | reg, data);
	outb(PCI_MODE2_ENABLE_REG, 0);
#endif
}

int
pci_mode_detect()
{

#ifdef PCI_CONF_MODE
#if (PCI_CONF_MODE == 1) || (PCI_CONF_MODE == 2)
	return (pci_mode = PCI_CONF_MODE);
#else
#error Invalid PCI configuration mode.
#endif
#else
	if (pci_mode != -1)
		return pci_mode;

	/*
	 * We try to divine which configuration mode the host bridge wants.  We
	 * try mode 2 first, because our probe for mode 1 is likely to succeed
	 * for mode 2 also.
	 *
	 * XXX
	 * This should really be done using the PCI BIOS.
	 */

	outb(PCI_MODE2_ENABLE_REG, 0);
	outb(PCI_MODE2_FORWARD_REG, 0);
	if (inb(PCI_MODE2_ENABLE_REG) != 0 ||
	    inb(PCI_MODE2_FORWARD_REG) != 0)
		goto not2;
	return (pci_mode = 2);

not2:
	outl(PCI_MODE1_ADDRESS_REG, PCI_MODE1_ENABLE);
	if (inl(PCI_MODE1_ADDRESS_REG) != PCI_MODE1_ENABLE)
		goto not1;
	outl(PCI_MODE1_ADDRESS_REG, 0);
	if (inl(PCI_MODE1_ADDRESS_REG) != 0)
		goto not1;
	return (pci_mode = 1);

not1:
	return (pci_mode = 0);
#endif
}

int
pci_map_io(tag, reg, iobasep)
	pcitag_t tag;
	int reg;
	u_short *iobasep;
{

	panic("pci_map_io: not implemented");
}

#ifndef PCI_PMEM_START
#define PCI_PMEM_START	0xc0000000
#endif

vm_offset_t pci_paddr = PCI_PMEM_START;

int
pci_map_mem(tag, reg, vap, pap)
	pcitag_t tag;
	int reg;
	vm_offset_t *vap, *pap;
{
	pcireg_t data;
	int cachable;
	vm_size_t size;
	vm_offset_t va, pa;

	if (reg < PCI_MAP_REG_START || reg >= PCI_MAP_REG_END || (reg & 3))
		panic("pci_map_mem: bad request");

	/*
	 * Section 6.2.5.1, `Address Maps', says that a device which wants 2^n
	 * bytes of memory will hardwire the bottom n bits of the address to 0.
	 * As recommended, we write all 1s and see what we get back.
	 */
	pci_conf_write(tag, reg, 0xffffffff);
	data = pci_conf_read(tag, reg);

	if (data & PCI_MAP_IO)
		panic("pci_map_mem: attempt to memory map an I/O region");

	switch (data & PCI_MAP_MEMORY_TYPE_MASK) {
	case PCI_MAP_MEMORY_TYPE_32BIT:
		break;
	case PCI_MAP_MEMORY_TYPE_32BIT_1M:
		printf("pci_map_mem: attempt to map restricted 32-bit region\n");
		return EOPNOTSUPP;
	case PCI_MAP_MEMORY_TYPE_64BIT:
		printf("pci_map_mem: attempt to map 64-bit region\n");
		return EOPNOTSUPP;
	default:
		printf("pci_map_mem: reserved mapping type\n");
		return EINVAL;
	}

	size = round_page(-(data & PCI_MAP_MEMORY_ADDRESS_MASK));

	va = kmem_alloc_pageable(kernel_map, size);
	if (va == 0) {
		printf("pci_map_mem: not enough memory\n");
		return ENOMEM;
	}

	/*
	 * Since the bottom address bits are forced to 0, the region must
	 * be aligned by its size.
	 */
	pci_paddr = pa = (pci_paddr + size - 1) & -size;
	pci_paddr += size;

	/* Tell the driver where we mapped it. */
	*vap = va;
	*pap = pa;

	/* Tell the device where we mapped it. */
	pci_conf_write(tag, reg, pa);

	/* Map the space into the kernel page table. */
	cachable = !!(data & PCI_MAP_MEMORY_CACHABLE);
	while (size) {
		pmap_enter(kernel_pmap, va, pa, VM_PROT_READ | VM_PROT_WRITE,
		    TRUE);
		if (!cachable)
			pmap_changebit(pa, PG_N, ~0);
		else
			pmap_changebit(pa, 0, ~PG_N);
		va += NBPG;
		pa += NBPG;
		size -= NBPG;
	}

#if 1
	printf("pci_map_mem: memory mapped at %08x-%08x\n", *pap, pci_paddr - 1);
#endif

	return 0;
}

int
pci_map_int(tag, ih)
	pcitag_t tag;
	struct intrhand *ih;
{
	pcireg_t data;
	int pin, line;
	u_short irq;

	data = pci_conf_read(tag, PCI_INTERRUPT_REG);

	pin = PCI_INTERRUPT_PIN_EXTRACT(data);
	line = PCI_INTERRUPT_LINE_EXTRACT(data);

	if (pin == 0) {
		/* No IRQ used. */
		return 0;
	}

	if (pin > 4) {
		printf("pci_map_int: bad interrupt pin %d\n", pin);
		return EINVAL;
	}

	/*
	 * Section 6.2.4, `Miscellaneous Functions', says that 255 means
	 * `unknown' or `no connection' on a PC.  We assume that a device with
	 * `no connection' either doesn't have an interrupt (in which case the
	 * pin number should be 0, and would have been noticed above), or
	 * wasn't configured by the BIOS (in which case we punt, since there's
	 * no real way we can know how the chipset is configured).
	 *
	 * XXX
	 * Since IRQ 0 is only used by the clock, and we can't actually be sure
	 * that the BIOS did its job, we also recognize that as meaning that
	 * the BIOS has not configured the device.
	 */
	if (line == 0 || line == 255) {
		printf("pci_map_int: no mapping for pin %c\n", '@' + pin);
		return EINVAL;
	} else {
		if (line >= ICU_LEN) {
			printf("pci_map_int: bad interrupt line %d\n", line);
			return EINVAL;
		}
		if (line == 2) {
			printf("pci_map_int: changed line 2 to line 9\n");
			line = 9;
		}
		irq = 1 << line;
	}

#if 1
	printf("pci_map_int: pin %c mapped to irq %d\n", '@' + pin, ffs(irq) - 1);
#endif

	intr_establish(irq, ih);
	return 0;
}
