/*	$NetBSD: pci_addr_fixup.c,v 1.2.2.1 2000/06/22 17:01:05 minoura Exp $	*/

/*-
 * Copyright (c) 2000 UCHIYAMA Yasushi.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "opt_pcibios.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/extent.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <i386/pci/pcibios.h>
#include <i386/pci/pci_addr_fixup.h>

#ifdef PCIBIOSVERBOSE
int	pcibiosverbose = 1;
#define	DPRINTF(arg) if (pcibiosverbose) printf arg;
#define	DPRINTFN(n, arg) if (pcibiosverbose > (n)) printf arg;
#else
#define	DPRINTF(arg)
#define DPRINTFN(n, arg)
#endif

struct pciaddr pciaddr;

typedef int (*pciaddr_resource_manage_func_t) 
	(pci_chipset_tag_t, pcitag_t, int, struct extent *, int,
	 bus_addr_t *, bus_size_t);
void	pciaddr_resource_manage __P((pci_chipset_tag_t, pcitag_t,
				     pciaddr_resource_manage_func_t));
void	pciaddr_resource_reserve __P((pci_chipset_tag_t, pcitag_t));
int	pciaddr_do_resource_reserve __P((pci_chipset_tag_t, pcitag_t, int,
					 struct extent *, int, bus_addr_t *,
					 bus_size_t));
void	pciaddr_resource_allocate __P((pci_chipset_tag_t, pcitag_t));
int	pciaddr_do_resource_allocate __P((pci_chipset_tag_t, pcitag_t, int,
					  struct extent *, int,	bus_addr_t *,
					  bus_size_t));
bus_addr_t pciaddr_ioaddr __P((u_int32_t));
void	pciaddr_print_devid __P((pci_chipset_tag_t, pcitag_t));

#define PCIADDR_MEM_START	0x0
#define PCIADDR_MEM_END		0xffffffff
#define PCIADDR_PORT_START	0x0
#define PCIADDR_PORT_END	0xffff

/* for ISA devices */
#define PCIADDR_ISAPORT_RESERVE	0x5800 /* empirical value */
#define PCIADDR_ISAMEM_RESERVE	(16 * 1024 * 1024)

void
pci_addr_fixup(pc, bus)
	pci_chipset_tag_t pc;
	int bus;
{
	extern paddr_t avail_end;
#ifdef PCIBIOSVERBOSE
	const char *verbose_header = 
		"[%s]-----------------------\n"
		"  device vendor product\n"
		"  register space address    size\n"
		"--------------------------------------------\n";
	const char *verbose_footer = 
		"--------------------------[%3d devices bogus]\n";
#endif
	const struct {
		bus_addr_t start;
		bus_size_t size;
		char *name;
	} system_reserve [] = {
		{ 0xfec00000, 0x100000, "I/O APIC" },
		{ 0xfee00000, 0x100000, "Local APIC" },
		{ 0xfffe0000, 0x20000, "BIOS PROM" },
		{ 0, 0, 0 }, /* terminator */
	}, *srp;
	paddr_t start;
	int error;
	
	pciaddr.extent_mem = extent_create("PCI I/O memory space",
					   PCIADDR_MEM_START, 
					   PCIADDR_MEM_END,
					   M_DEVBUF, 0, 0, EX_NOWAIT);
	KASSERT(pciaddr.extent_mem);
	pciaddr.extent_port = extent_create("PCI I/O port space",
					    PCIADDR_PORT_START,
					    PCIADDR_PORT_END,
					    M_DEVBUF, 0, 0, EX_NOWAIT);
	KASSERT(pciaddr.extent_port);

	/* 
	 * 1. check & reserve system BIOS setting.
	 */
	DPRINTF((verbose_header, "System BIOS Setting"));
	pci_device_foreach(pc, bus, pciaddr_resource_reserve);
	DPRINTF((verbose_footer, pciaddr.nbogus));

	/* 
	 * 2. reserve non-PCI area.
	 */
	for (srp = system_reserve; srp->size; srp++) {
		error = extent_alloc_region(pciaddr.extent_mem, srp->start,
					    srp->size, 
					    EX_NOWAIT| EX_MALLOCOK);	
		if (error != 0) {
			printf("WARNING: can't reserve area for %s.\n",
			       srp->name);
		}
	}

	/* 
	 * 3. determine allocation space 
	 */
	start = i386_round_page(avail_end + 1);
	if (start < PCIADDR_ISAMEM_RESERVE)
		start = PCIADDR_ISAMEM_RESERVE;
	pciaddr.mem_alloc_start = (start + 0x100000 + 1) & ~(0x100000 - 1);
	pciaddr.port_alloc_start = PCIADDR_ISAPORT_RESERVE;
	DPRINTF((" Physical memory end: 0x%08x\n PCI memory mapped I/O "
		 "space start: 0x%08x\n", (unsigned)avail_end, 
		 (unsigned)pciaddr.mem_alloc_start));

	if (pciaddr.nbogus == 0)
		return; /* no need to fixup */

	/* 
	 * 4. do fixup 
	 */
	DPRINTF((verbose_header, "PCIBIOS fixup stage"));
	pciaddr.nbogus = 0;
	pci_device_foreach(pc, bus, pciaddr_resource_allocate);
	DPRINTF((verbose_footer, pciaddr.nbogus));

}

void
pciaddr_resource_reserve(pc, tag)
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
#ifdef PCIBIOSVERBOSE
	pciaddr_print_devid(pc, tag);
#endif
	pciaddr_resource_manage(pc, tag, pciaddr_do_resource_reserve);	
}

void
pciaddr_resource_allocate(pc, tag)
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
#ifdef PCIBIOSVERBOSE
	pciaddr_print_devid(pc, tag);
#endif
	pciaddr_resource_manage(pc, tag, pciaddr_do_resource_allocate);
}

void
pciaddr_resource_manage(pc, tag, func)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	pciaddr_resource_manage_func_t func;
{
	struct extent *ex;
	pcireg_t val, mask;
	bus_addr_t addr;
	bus_size_t size;
	int error, useport, usemem, mapreg, type, reg_start, reg_end;

	val = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(val)) {
	default:
		printf("WARNING: unknown PCI device header.");
		pciaddr.nbogus++;
		return;
	case 0: 
		reg_start = PCI_MAPREG_START;
		reg_end   = PCI_MAPREG_END;
		break;
	case 1: /* PCI-PCI bridge */
		reg_start = PCI_MAPREG_START;
		reg_end   = PCI_MAPREG_PPB_END;
		break;
	case 2: /* PCI-CardBus bridge */
		reg_start = PCI_MAPREG_START;
		reg_end   = PCI_MAPREG_PCB_END;
		break;
	}
	error = useport = usemem = 0;
    
	for (mapreg = reg_start; mapreg < reg_end; mapreg += 4) {
		/* inquire PCI device bus space requirement */
		val = pci_conf_read(pc, tag, mapreg);
		pci_conf_write(pc, tag, mapreg, ~0);

		mask = pci_conf_read(pc, tag, mapreg);
		pci_conf_write(pc, tag, mapreg, val);
	
		type = PCI_MAPREG_TYPE(val);
		if (type == PCI_MAPREG_TYPE_MEM) {
			size = PCI_MAPREG_MEM_SIZE(mask);
			ex = pciaddr.extent_mem;
		} else {
			size = PCI_MAPREG_IO_SIZE(mask);
			ex = pciaddr.extent_port;
		}
		addr = pciaddr_ioaddr(val);
	
		if (!size) /* unused register */
			continue;

		if (type == PCI_MAPREG_TYPE_MEM)
			++usemem;
		else
			++useport;

		/* reservation/allocation phase */
		error += (*func) (pc, tag, mapreg, ex, type, &addr, size);

		DPRINTF(("\n\t%02xh %s 0x%08x 0x%08x", 
			 mapreg, type ? "port" : "mem ", 
			 (unsigned int)addr, (unsigned int)size));
	}
    
	/* enable/disable PCI device */
	val = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);	
	if (error == 0)
		val |= (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
			PCI_COMMAND_MASTER_ENABLE);
	else
		val &= ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
			 PCI_COMMAND_MASTER_ENABLE);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, val);
    
	if (error)
		pciaddr.nbogus++;

	DPRINTF(("\n\t\t[%s]\n", error ? "NG" : "OK"));
}

int
pciaddr_do_resource_allocate(pc, tag, mapreg, ex, type, addr, size)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	struct extent *ex;
	int mapreg, type;
	bus_addr_t *addr;
	bus_size_t size;
{
	bus_addr_t start;
	int error;
	
	if (*addr) /* no need to allocate */
		return (0);
	
	start = type == PCI_MAPREG_TYPE_MEM ? pciaddr.mem_alloc_start
		: pciaddr.port_alloc_start;
	if (start < ex->ex_start || start + size - 1 >= ex->ex_end) {
		DPRINTF(("No available resources. fixup failed\n"));
		return (1);
	}
	error = extent_alloc_subregion(ex, start, start + size - 1, size,
				       size, 0,
				       EX_FAST|EX_NOWAIT|EX_MALLOCOK, addr);
	if (error) {
		DPRINTF(("No available resources. fixup failed\n"));
		return (1);
	}

	/* write new address to PCI device configuration header */
	pci_conf_write(pc, tag, mapreg, *addr);
	/* check */
#ifndef PCIBIOSVERBOSE
	printf("pci_addr_fixup: ");
	pciaddr_print_devid(pc, tag);
#endif 
	if (pciaddr_ioaddr(pci_conf_read(pc, tag, mapreg)) != *addr) {
		pci_conf_write(pc, tag, mapreg, 0); /* clear */
		printf("fixup failed. (new address=%#x)\n", (unsigned)*addr);
		return (1);
	}
#ifndef PCIBIOSVERBOSE
	printf("new address 0x%08x\n", (unsigned)*addr);
#endif

	return (0);
}

int
pciaddr_do_resource_reserve(pc, tag, mapreg, ex, type, addr, size)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	struct extent *ex;
	int type, mapreg;
	bus_addr_t *addr;
	bus_size_t size;
{
	int error;

	if (*addr == 0)
		return (1);

	error = extent_alloc_region(ex, *addr, size, EX_NOWAIT| EX_MALLOCOK);
	if (error) {
		DPRINTF(("Resource conflict.\n"));
		pci_conf_write(pc, tag, mapreg, 0); /* clear */
		return (1);
	}

	return (0);
}

bus_addr_t
pciaddr_ioaddr(val)
	u_int32_t val;
{
	return ((PCI_MAPREG_TYPE(val) == PCI_MAPREG_TYPE_MEM)
		? PCI_MAPREG_MEM_ADDR(val)
		: PCI_MAPREG_IO_ADDR(val));
}

void
pciaddr_print_devid(pc, tag)
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	int bus, device, function;	
	pcireg_t id;
	
	id = pci_conf_read(pc, tag, PCI_ID_REG);
	pci_decompose_tag(pc, tag, &bus, &device, &function);
	printf("%03d:%02d:%d 0x%04x 0x%04x ", bus, device, function, 
	       PCI_VENDOR(id), PCI_PRODUCT(id));
}
