/*	$NetBSD: mainbus.c,v 1.4.6.1 2001/11/12 21:17:42 thorpej Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Eduardo Horvath and Simon Burge for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include "pckbc.h"
#include "pci.h"
#include "opt_pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#define _GALAXY_BUS_DMA_PRIVATE
#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/walnut.h>

#include <powerpc/ibm4xx/ibm405gp.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciconf.h>

/*
 * The devices built in to the 405GP cpu.
 */
const struct ppc405gp_dev {
	const char *name;
	bus_addr_t addr;
	int irq;
} ppc405gp_devs [] = {
	{ "com",	UART0_BASE,	 0 },
	{ "com",	UART1_BASE,	 1 },
	{ "dsrtc",	NVRAM_BASE,	-1 },
	{ "emac",	EMAC0_BASE,	15 }, /* XXX: really irq 9..15 */
	{ "gpio",	GPIO0_BASE,	-1 },
	{ "i2c",	IIC0_BASE,	-1 },
	{ "wdog",	-1,        	-1 },
	{ "pckbc",	KEY_MOUSE_BASE,	10 }, /* XXX: really irq x..x+1 */
	{ NULL }
};

static int	mainbus_match(struct device *, struct cfdata *, void *);
static void	mainbus_attach(struct device *, struct device *, void *);
static int	mainbus_submatch(struct device *, struct cfdata *, void *);
static int	mainbus_print(void *, const char *);

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};


/*
 * Probe for the mainbus; always succeeds.
 */
static int
mainbus_match(struct device *parent, struct cfdata *match, void *aux)
{

	return 1;
}

static int
mainbus_submatch(struct device *parent, struct cfdata *cf, void *aux)
{
	union mainbus_attach_args *maa = aux;

	/* Skip locator song-and-dance if we're attaching the PCI layer */
	if (strcmp(maa->mba_pba.pba_busname, "pci") == 0)
		return ((*cf->cf_attach->ca_match)(parent, cf, aux));

	if (cf->cf_loc[MAINBUSCF_ADDR] != MAINBUSCF_ADDR_DEFAULT &&
	    cf->cf_loc[MAINBUSCF_ADDR] != maa->mba_rmb.rmb_addr)
		return (0);

	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

/*
 * Attach the mainbus.
 */
static void
mainbus_attach(struct device *parent, struct device *self, void *aux)
{
	union mainbus_attach_args mba;
	int i;
#if NPCKBC > 0
	bus_space_handle_t ioh_fpga;
	bus_space_tag_t iot_fpga = galaxy_make_bus_space_tag(0, 0);
	uint8_t fpga_reg;
#endif
#ifdef PCI_NETBSD_CONFIGURE
	struct extent *ioext, *memext;
#ifdef PCI_CONFIGURE_VERBOSE
	extern int pci_conf_debug;

	pci_conf_debug = 1;
#endif
#endif

	printf("\n");

	/* Attach the CPU first */
	mba.mba_rmb.rmb_name = "cpu";
	mba.mba_rmb.rmb_addr = MAINBUSCF_ADDR_DEFAULT;
	mba.mba_rmb.rmb_irq = MAINBUSCF_IRQ_DEFAULT;
	config_found(self, &mba, mainbus_print);
	
	for (i = 0; ppc405gp_devs[i].name != NULL; i++) {
		mba.mba_rmb.rmb_name = ppc405gp_devs[i].name;
		mba.mba_rmb.rmb_addr = ppc405gp_devs[i].addr;
		mba.mba_rmb.rmb_irq = ppc405gp_devs[i].irq;
		mba.mba_rmb.rmb_dmat = &galaxy_default_bus_dma_tag;

		(void) config_found_sm(self, &mba, mainbus_print,
		    mainbus_submatch);
	}

#if NPCKBC > 0
	/* Configure FPGA */
	if (bus_space_map(iot_fpga, FPGA_BASE, FPGA_SIZE, 0, &ioh_fpga)) {
		printf("mainbus_attach: can't map FPGA\n");
		/* XXX - disable keyboard probe? */
	} else {
		/* Use separate interrupts for keyboard and mouse */
		fpga_reg = bus_space_read_1(iot_fpga, ioh_fpga, FPGA_BRDC);
		fpga_reg |= FPGA_BRDC_INT;
		bus_space_write_1(iot_fpga, ioh_fpga, FPGA_BRDC, fpga_reg);

		/* Set interrupts to active high */
		fpga_reg = bus_space_read_1(iot_fpga, ioh_fpga, FPGA_INT_POL);
		fpga_reg |= (FPGA_IRQ_KYBD | FPGA_IRQ_MOUSE);
		bus_space_write_1(iot_fpga, ioh_fpga, FPGA_INT_POL, fpga_reg);

		/* Set interrupts to level triggered */
		fpga_reg = bus_space_read_1(iot_fpga, ioh_fpga, FPGA_INT_TRIG);
		fpga_reg |= (FPGA_IRQ_KYBD | FPGA_IRQ_MOUSE);
		bus_space_write_1(iot_fpga, ioh_fpga, FPGA_INT_TRIG, fpga_reg);

		/* Enable interrupts */
		fpga_reg = bus_space_read_1(iot_fpga, ioh_fpga, FPGA_INT_ENABLE);
		fpga_reg |= (FPGA_IRQ_KYBD | FPGA_IRQ_MOUSE);
		bus_space_write_1(iot_fpga, ioh_fpga, FPGA_INT_ENABLE, fpga_reg);

		bus_space_unmap(&iot_fpga, ioh_fpga, 2);
	}
#endif

#if NPCI > 0
	pci_machdep_init();
	galaxy_setup_pci();
#ifdef PCI_CONFIGURE_VERBOSE
	galaxy_show_pci_map();
#endif
	// scan_pci_bus();

#ifdef PCI_NETBSD_CONFIGURE
	// galaxy_show_pci_map();
	// scan_pci_bus();
	memext = extent_create("pcimem", MIN_PCI_MEMADDR_NOPREFETCH,
	    MIN_PCI_MEMADDR_NOPREFETCH + 0x1fffffff, M_DEVBUF, NULL, 0,
	    EX_NOWAIT);
	ioext = extent_create("pciio", MIN_PCI_PCI_IOADDR,
	    MIN_PCI_PCI_IOADDR + 0xffff, M_DEVBUF, NULL, 0, EX_NOWAIT);
	pci_configure_bus(0, ioext, memext, NULL, 0);
	extent_destroy(memext);
#endif /* PCI_NETBSD_CONFIGURE */

#ifdef PCI_CONFIGURE_VERBOSE
	printf("running config_found PCI\n");
#endif
	mba.mba_pba.pba_busname = "pci";
	/* IO window located @ e8000000 and maps to 0-0xffff */
	mba.mba_pba.pba_iot = galaxy_make_bus_space_tag(MIN_PLB_PCI_IOADDR, 0);
	/* PCI memory window is directly mapped */
	mba.mba_pba.pba_memt = galaxy_make_bus_space_tag(0, 0);
	mba.mba_pba.pba_dmat = &galaxy_default_bus_dma_tag;
	mba.mba_pba.pba_bus = 0;
	mba.mba_pba.pba_flags = PCI_FLAGS_MEM_ENABLED | PCI_FLAGS_IO_ENABLED;
	config_found(self, &mba.mba_pba, mainbus_print);
#endif /* NPCI > 0 */
}

static int
mainbus_print(void *aux, const char *pnp)
{
	union mainbus_attach_args *mba = aux;

	if (pnp)
		printf("%s at %s", mba->mba_busname, pnp);

	if (strcmp(mba->mba_busname, "pci") == 0)
		printf(" bus %d", mba->mba_pba.pba_bus);
	else {
		if (mba->mba_rmb.rmb_addr != MAINBUSCF_ADDR_DEFAULT)
			printf(" addr 0x%08lx", mba->mba_rmb.rmb_addr);
		if (mba->mba_rmb.rmb_irq != MAINBUSCF_IRQ_DEFAULT)
			printf(" irq %d", mba->mba_rmb.rmb_irq);
	}
	return (UNCONF);
}

#if 0
static void
scan_pci_bus(void)
{
	pcitag_t tag;
	int i, x;

	for (i=0;i<32;i++){
		tag = pci_make_tag(0, 0, i, 0);
		x = pci_conf_read(0, tag, 0);
		printf("%d tag=%08x : %08x\n", i, tag, x);
#if 0
		if (PCI_VENDOR(x) == PCI_VENDOR_INTEL
		    && PCI_PRODUCT(x) == PCI_PRODUCT_INTEL_80960_RP) {
			/* Do not configure PCI bus analyzer */
			continue;
		}
		x = pci_conf_read(0, tag, PCI_COMMAND_STATUS_REG);
		x |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
		pci_conf_write(0, tag, PCI_COMMAND_STATUS_REG, x);
#endif
	}
}
#endif
