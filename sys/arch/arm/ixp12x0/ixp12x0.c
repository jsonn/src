/*	$NetBSD: ixp12x0.c,v 1.1.2.3 2002/08/30 00:19:14 gehenna Exp $ */
/*
 * Copyright (c) 2002
 *	Ichiro FUKUHARA <ichiro@ichiro.org>.
 * All rights reserved.
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
 *	This product includes software developed by Ichiro FUKUHARA.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ICHIRO FUKUHARA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ICHIRO FUKUHARA OR THE VOICES IN HIS HEAD BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <uvm/uvm.h>

#include <machine/bus.h>

#include <arm/ixp12x0/ixp12x0reg.h>
#include <arm/ixp12x0/ixp12x0var.h>
#include <arm/ixp12x0/ixp12x0_pcireg.h>

int ixp12x0_pcibus_print(void *, const char *);

static struct ixp12x0_softc *ixp12x0_softc;

void
ixp12x0_attach(sc)
	struct ixp12x0_softc *sc;
{
	struct pcibus_attach_args pba;
	pcireg_t reg;

	ixp12x0_softc = sc;

	printf("\n");
	/*
	 * Subregion for PCI Configuration Spase Registers
	 */
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, 0,
	    IXP12X0_PCI_SIZE, &sc->sc_pci_ioh))
		panic("%s: unable to subregion PCI registers\n",
		      sc->sc_dev.dv_xname); 
	/*
	 * PCI bus reset
	 */
	/* XXX assert PCI reset Mode */
	reg = bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh,
		SA_CONTROL) &~ SA_CONTROL_PNR;
	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		SA_CONTROL, reg);
	DELAY(10);

	/* XXX Disable door bell and outbound interrupt */
	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		PCI_CAP_PTR, 0xc);
	/* Disable door bell int to PCI */
	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		DBELL_PCI_MASK, 0x0);
	/* Disable door bell int to SA-core */
	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		DBELL_SA_MASK, 0x0);

	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		PCI_ADDR_EXT, 0);

	/* XXX Negate PCI reset */
	reg = bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh,
		SA_CONTROL) | SA_CONTROL_PNR;
	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		SA_CONTROL, reg);
	DELAY(10);
	/*
	 * specify window size of memory access and SDRAM.
	 */
	reg = bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh,
		IXP_PCI_MEM_BAR) | IXP1200_PCI_MEM_BAR;
	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		IXP_PCI_MEM_BAR, reg);

	reg = bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh,
		IXP_PCI_IO_BAR) | IXP1200_PCI_IO_BAR;
	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		IXP_PCI_IO_BAR, reg);

	reg = bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh,
		IXP_PCI_DRAM_BAR) | IXP1200_PCI_DRAM_BAR;
	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		IXP_PCI_DRAM_BAR, reg);

	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		CSR_BASE_ADDR_MASK, CSR_BASE_ADDR_MASK_1M);
	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		DRAM_BASE_ADDR_MASK, DRAM_BASE_ADDR_MASK_256MB);

#if DEBUG
	printf("IXP_PCI_MEM_BAR = 0x%08x\nIXP_PCI_IO_BAR = 0x%08x\nIXP_PCI_DRAM_BAR = 0x%08x\nCSR_BASE_ADDR_MASK = 0x%08x\n",
	bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh, IXP_PCI_MEM_BAR),
	bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh, IXP_PCI_IO_BAR),
	bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh, IXP_PCI_DRAM_BAR),
	bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh, DRAM_BASE_ADDR_MASK));
#endif 
	/* Initialize complete */
	reg = bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh,
		SA_CONTROL) | 0x1;
	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		SA_CONTROL, reg);
#if DEBUG
	printf("SA_CONTROL = 0x%08x\n",
		bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh, SA_CONTROL));
#endif 
	/*
	 * Enable bus mastering and I/O,memory access
	 */
	/* host only */
	reg = bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh,
		PCI_COMMAND_STATUS_REG) |
		PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
		PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_INVALIDATE_ENABLE;
	bus_space_write_4(sc->sc_iot, sc->sc_pci_ioh,
		PCI_COMMAND_STATUS_REG, reg);
#if DEBUG
	printf("PCI_COMMAND_STATUS_REG = 0x%08x\n",
		bus_space_read_4(sc->sc_iot, sc->sc_pci_ioh, PCI_COMMAND_STATUS_REG));
#endif
	/*
	 * Initialize the bus space and DMA tags and the PCI chipset tag.
	 */
	ixp12x0_io_bs_init(&sc->ia_pci_iot, sc);
	ixp12x0_mem_bs_init(&sc->ia_pci_memt, sc);
	ixp12x0_pci_init(&sc->ia_pci_chipset, sc);
	ixp12x0_pci_dma_init(&sc->ia_pci_dmat, sc);

	/*
	 * Attach the PCI bus.
	 */
	pba.pba_busname = "pci";
	pba.pba_pc = &sc->ia_pci_chipset;
	pba.pba_iot = &sc->ia_pci_iot;
	pba.pba_memt = &sc->ia_pci_memt;
	pba.pba_dmat = &sc->ia_pci_dmat;
	pba.pba_bus = 0;	/* bus number = 0 */
	pba.pba_intrswiz = 0;	/* XXX */
	pba.pba_intrtag = 0;
	pba.pba_flags = PCI_FLAGS_IO_ENABLED | PCI_FLAGS_MEM_ENABLED |
		PCI_FLAGS_MRL_OKAY | PCI_FLAGS_MRM_OKAY | PCI_FLAGS_MWI_OKAY;
	(void) config_found(&sc->sc_dev, &pba, ixp12x0_pcibus_print);
}

int
ixp12x0_pcibus_print(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);

	printf(" bus %d", pba->pba_bus);

	return (UNCONF);
}

/*
 * IXP12x0 specific I/O registers mapping table
 */
static struct pmap_ent	map_tbl_ixp12x0[] = {
	{ "StrongARM System and Peripheral Registers",
	  IXP12X0_SYS_VBASE, IXP12X0_SYS_HWBASE,
	  IXP12X0_SYS_SIZE,
	  VM_PROT_READ|VM_PROT_WRITE,
	  PTE_NOCACHE, },

	{ "PCI Registers Accessible Through StrongARM Core",
	  IXP12X0_PCI_VBASE, IXP12X0_PCI_HWBASE,
	  IXP12X0_PCI_SIZE,
	  VM_PROT_READ|VM_PROT_WRITE,
	  PTE_NOCACHE, },

	{ "PCI Registers Accessible Through I/O Cycle Access",
	  IXP12X0_PCI_IO_VBASE, IXP12X0_PCI_IO_HWBASE,
	  IXP12X0_PCI_IO_SIZE,
	  VM_PROT_READ|VM_PROT_WRITE,
	  PTE_NOCACHE, },

	{ "PCI Registers Accessible Through Memory Cycle Access",
	  IXP12X0_PCI_MEM_VBASE, IXP12X0_PCI_MEM_VBASE,
	  IXP12X0_PCI_MEM_SIZE,
	  VM_PROT_READ|VM_PROT_WRITE,
	  PTE_NOCACHE, },

	{ "PCI Type0 Configuration Space",
	  IXP12X0_PCI_TYPE0_VBASE, IXP12X0_PCI_TYPE0_VBASE,
	  IXP12X0_PCI_TYPEX_SIZE,
	  VM_PROT_READ|VM_PROT_WRITE,
	  PTE_NOCACHE, },

	{ "PCI Type1 Configuration Space",
	  IXP12X0_PCI_TYPE1_VBASE, IXP12X0_PCI_TYPE1_VBASE,
	  IXP12X0_PCI_TYPEX_SIZE,
	  VM_PROT_READ|VM_PROT_WRITE,
	  PTE_NOCACHE, },

	{ NULL, 0, 0, 0, 0, 0 },
};

/*
 * mapping virtual memories
 */
void
ixp12x0_pmap_chunk_table(vaddr_t l1pt, struct pmap_ent* m)
{
	int loop;

	loop = 0;
	while (m[loop].msg) {
		printf("mapping %s...\n", m[loop].msg);
		pmap_map_chunk(l1pt, m[loop].va, m[loop].pa,
			       m[loop].sz, m[loop].prot, m[loop].cache);
		++loop;
	}
}

/*
 * mapping I/O registers
 */
void
ixp12x0_pmap_io_reg(vaddr_t l1pt)
{
	ixp12x0_pmap_chunk_table(l1pt, map_tbl_ixp12x0);
}

void
ixp12x0_reset(void)
{
	bus_space_write_4(ixp12x0_softc->sc_iot, ixp12x0_softc->sc_pci_ioh,
		IXPPCI_IXP1200_RESET, RESET_FULL);
}
