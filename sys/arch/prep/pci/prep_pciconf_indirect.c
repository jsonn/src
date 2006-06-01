/*	$NetBSD: prep_pciconf_indirect.c,v 1.5.6.1 2006/06/01 22:35:17 kardel Exp $	*/

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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: prep_pciconf_indirect.c,v 1.5.6.1 2006/06/01 22:35:17 kardel Exp $");

#include "opt_openpic.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#define _POWERPC_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/pio.h>
#include <machine/platform.h>

#if defined(OPENPIC)
#include <powerpc/openpic.h>
#endif /* OPENPIC */

#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#define	PCI_MODE1_ENABLE	0x80000000UL

extern volatile unsigned char *prep_pci_baseaddr;
extern volatile unsigned char *prep_pci_basedata;

#define	PCI_CBIO		0x10

void prep_pci_indirect_attach_hook(struct device *, struct device *,
    struct pcibus_attach_args *);
pcitag_t prep_pci_indirect_make_tag(void *, int, int, int);
pcireg_t prep_pci_indirect_conf_read(void *, pcitag_t, int);
void prep_pci_indirect_conf_write(void *, pcitag_t, int, pcireg_t);
void prep_pci_indirect_decompose_tag(void *, pcitag_t, int *, int *, int *);

void
prep_pci_get_chipset_tag_indirect(pci_chipset_tag_t pc)
{

	pc->pc_conf_v = NULL;

	pc->pc_attach_hook = prep_pci_indirect_attach_hook;
	pc->pc_bus_maxdevs = prep_pci_bus_maxdevs;
	pc->pc_make_tag = prep_pci_indirect_make_tag;
	pc->pc_conf_read = prep_pci_indirect_conf_read;
	pc->pc_conf_write = prep_pci_indirect_conf_write;

	pc->pc_intr_v = NULL;

	pc->pc_intr_map = prep_pci_intr_map;
	pc->pc_intr_string = prep_pci_intr_string;
	pc->pc_intr_evcnt = prep_pci_intr_evcnt;
	pc->pc_intr_establish = prep_pci_intr_establish;
	pc->pc_intr_disestablish = prep_pci_intr_disestablish;

	pc->pc_conf_interrupt = prep_pci_conf_interrupt;
	pc->pc_decompose_tag = prep_pci_indirect_decompose_tag;
	pc->pc_conf_hook = prep_pci_conf_hook;
}

void
prep_pci_indirect_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{

	if (pba->pba_bus != 0)
		return;

	printf(": indirect configuration space access");

#if defined(OPENPIC)
	if (openpic_base) {
		pci_chipset_tag_t pc;
		pcitag_t tag;
		pcireg_t id, address;

		pc = pba->pba_pc;
		tag = pci_make_tag(pc, 0, 13, 0);
		id = pci_conf_read(pc, tag, PCI_ID_REG);

		if (PCI_VENDOR(id) == PCI_VENDOR_IBM
		    && PCI_PRODUCT(id) == PCI_PRODUCT_IBM_MPIC) {
			address = pci_conf_read(pc, tag, PCI_CBIO);
			if ((address & PCI_MAPREG_TYPE_MASK) == PCI_MAPREG_TYPE_MEM) {
				address &= PCI_MAPREG_MEM_ADDR_MASK;
				openpic_base = (unsigned char *)(PREP_BUS_SPACE_MEM | address);
			}
		}
	}
#endif /* OPENPIC */
}

pcitag_t
prep_pci_indirect_make_tag(void *v, int bus, int device, int function)
{
	pcitag_t tag;

	if (bus >= 256 || device >= 32 || function >= 8)
		panic("pci_make_tag: bad request");

	tag = PCI_MODE1_ENABLE |
		    (bus << 16) | (device << 11) | (function << 8);
	return tag;
}

void
prep_pci_indirect_decompose_tag(void *v, pcitag_t tag, int *bp, int *dp,
    int *fp)
{

	if (bp != NULL)
		*bp = (tag >> 16) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 11) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 8) & 0x7;
	return;
}

pcireg_t
prep_pci_indirect_conf_read(void *v, pcitag_t tag, int reg)
{
	pcireg_t data;
	int s;

	s = splhigh();
	out32rb(prep_pci_baseaddr, tag | reg);
	data = in32rb(prep_pci_basedata);
	out32rb(prep_pci_baseaddr, 0);
	splx(s);

	return data;
}

void
prep_pci_indirect_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	int s;

	s = splhigh();
	out32rb(prep_pci_baseaddr, tag | reg);
	out32rb(prep_pci_basedata, data);
	out32rb(prep_pci_baseaddr, 0);
	splx(s);
}
