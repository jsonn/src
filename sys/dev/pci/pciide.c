/*	$NetBSD: pciide.c,v 1.68.2.25 2001/07/02 13:47:25 jhawk Exp $	*/


/*
 * Copyright (c) 1999, 2000, 2001 Manuel Bouyer.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */


/*
 * Copyright (c) 1996, 1998 Christopher G. Demetriou.  All rights reserved.
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

/*
 * PCI IDE controller driver.
 *
 * Author: Christopher G. Demetriou, March 2, 1998 (derived from NetBSD
 * sys/dev/pci/ppb.c, revision 1.16).
 *
 * See "PCI IDE Controller Specification, Revision 1.0 3/4/94" and
 * "Programming Interface for Bus Master IDE Controller, Revision 1.0
 * 5/16/94" from the PCI SIG.
 *
 */

#ifndef WDCDEBUG
#define WDCDEBUG
#endif

#define DEBUG_DMA   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#ifdef WDCDEBUG
int wdcdebug_pciide_mask = 0;
#define WDCDEBUG_PRINT(args, level) \
	if (wdcdebug_pciide_mask & (level)) printf args
#else
#define WDCDEBUG_PRINT(args, level)
#endif
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/endian.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_piix_reg.h>
#include <dev/pci/pciide_amd_reg.h>
#include <dev/pci/pciide_apollo_reg.h>
#include <dev/pci/pciide_cmd_reg.h>
#include <dev/pci/pciide_cy693_reg.h>
#include <dev/pci/pciide_sis_reg.h>
#include <dev/pci/pciide_acer_reg.h>
#include <dev/pci/pciide_pdc202xx_reg.h>
#include <dev/pci/pciide_opti_reg.h>
#include <dev/pci/pciide_hpt_reg.h>
#include <dev/pci/cy82c693var.h>

#include "opt_pciide.h"

/* inlines for reading/writing 8-bit PCI registers */
static __inline u_int8_t pciide_pci_read __P((pci_chipset_tag_t, pcitag_t,
					      int));
static __inline void pciide_pci_write __P((pci_chipset_tag_t, pcitag_t,
					   int, u_int8_t));

static __inline u_int8_t
pciide_pci_read(pc, pa, reg)
	pci_chipset_tag_t pc;
	pcitag_t pa;
	int reg;
{

	return (pci_conf_read(pc, pa, (reg & ~0x03)) >>
	    ((reg & 0x03) * 8) & 0xff);
}

static __inline void
pciide_pci_write(pc, pa, reg, val)
	pci_chipset_tag_t pc;
	pcitag_t pa;
	int reg;
	u_int8_t val;
{
	pcireg_t pcival;

	pcival = pci_conf_read(pc, pa, (reg & ~0x03));
	pcival &= ~(0xff << ((reg & 0x03) * 8));
	pcival |= (val << ((reg & 0x03) * 8));
	pci_conf_write(pc, pa, (reg & ~0x03), pcival);
}

void default_chip_map __P((struct pciide_softc*, struct pci_attach_args*));

void piix_chip_map __P((struct pciide_softc*, struct pci_attach_args*));
void piix_setup_channel __P((struct channel_softc*));
void piix3_4_setup_channel __P((struct channel_softc*));
static u_int32_t piix_setup_idetim_timings __P((u_int8_t, u_int8_t, u_int8_t));
static u_int32_t piix_setup_idetim_drvs __P((struct ata_drive_datas*));
static u_int32_t piix_setup_sidetim_timings __P((u_int8_t, u_int8_t, u_int8_t));

void amd7x6_chip_map __P((struct pciide_softc*, struct pci_attach_args*));
void amd7x6_setup_channel __P((struct channel_softc*));

void apollo_chip_map __P((struct pciide_softc*, struct pci_attach_args*));
void apollo_setup_channel __P((struct channel_softc*));

void cmd_chip_map __P((struct pciide_softc*, struct pci_attach_args*));
void cmd0643_9_chip_map __P((struct pciide_softc*, struct pci_attach_args*));
void cmd0643_9_setup_channel __P((struct channel_softc*));
void cmd_channel_map __P((struct pci_attach_args *,
			struct pciide_softc *, int));
int  cmd_pci_intr __P((void *));
void cmd646_9_irqack __P((struct channel_softc *));

void cy693_chip_map __P((struct pciide_softc*, struct pci_attach_args*));
void cy693_setup_channel __P((struct channel_softc*));

void sis_chip_map __P((struct pciide_softc*, struct pci_attach_args*));
void sis_setup_channel __P((struct channel_softc*));

void acer_chip_map __P((struct pciide_softc*, struct pci_attach_args*));
void acer_setup_channel __P((struct channel_softc*));
int  acer_pci_intr __P((void *));

void pdc202xx_chip_map __P((struct pciide_softc*, struct pci_attach_args*));
void pdc202xx_setup_channel __P((struct channel_softc*));
int  pdc202xx_pci_intr __P((void *));
int  pdc20265_pci_intr __P((void *));

void opti_chip_map __P((struct pciide_softc*, struct pci_attach_args*));
void opti_setup_channel __P((struct channel_softc*));

void hpt_chip_map __P((struct pciide_softc*, struct pci_attach_args*));
void hpt_setup_channel __P((struct channel_softc*));
int  hpt_pci_intr __P((void *));

void pciide_channel_dma_setup __P((struct pciide_channel *));
int  pciide_dma_table_setup __P((struct pciide_softc*, int, int));
int  pciide_dma_init __P((void*, int, int, void *, size_t, int));
void pciide_dma_start __P((void*, int, int));
int  pciide_dma_finish __P((void*, int, int, int));
void pciide_irqack __P((struct channel_softc *));
void pciide_print_modes __P((struct pciide_channel *));

struct pciide_product_desc {
	u_int32_t ide_product;
	int ide_flags;
	const char *ide_name;
	/* map and setup chip, probe drives */
	void (*chip_map) __P((struct pciide_softc*, struct pci_attach_args*));
};

/* Flags for ide_flags */
#define IDE_PCI_CLASS_OVERRIDE 0x0001 /* accept even if class != pciide */

/* Default product description for devices not known from this controller */
const struct pciide_product_desc default_product_desc = {
	0,
	0,
	"Generic PCI IDE controller",
	default_chip_map,
};

const struct pciide_product_desc pciide_intel_products[] =  {
	{ PCI_PRODUCT_INTEL_82092AA,
	  0,
	  "Intel 82092AA IDE controller",
	  default_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82371FB_IDE,
	  0,
	  "Intel 82371FB IDE controller (PIIX)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82371SB_IDE,
	  0,
	  "Intel 82371SB IDE Interface (PIIX3)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82371AB_IDE,
	  0,
	  "Intel 82371AB IDE controller (PIIX4)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801AA_IDE,
	  0,
	  "Intel 82801AA IDE Controller (ICH)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801AB_IDE,
	  0,
	  "Intel 82801AB IDE Controller (ICH0)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801BA_IDE,
	  0,
	  "Intel 82801BA IDE Controller (ICH2)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801BAM_IDE,
	  0,
	  "Intel 82801BAM IDE Controller (ICH2)",
	  piix_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

const struct pciide_product_desc pciide_amd_products[] =  {
	{ PCI_PRODUCT_AMD_PBC756_IDE,
	  0,
	  "Advanced Micro Devices AMD756 IDE Controller",
	  amd7x6_chip_map
	},
	{ PCI_PRODUCT_AMD_PBC766_IDE,
	  0,
	  "Advanced Micro Devices AMD766 IDE Controller",
	  amd7x6_chip_map
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

const struct pciide_product_desc pciide_cmd_products[] =  {
	{ PCI_PRODUCT_CMDTECH_640,
	  0,
	  "CMD Technology PCI0640",
	  cmd_chip_map
	},
	{ PCI_PRODUCT_CMDTECH_643,
	  0,
	  "CMD Technology PCI0643",
	  cmd0643_9_chip_map,
	},
	{ PCI_PRODUCT_CMDTECH_646,
	  0,
	  "CMD Technology PCI0646",
	  cmd0643_9_chip_map,
	},
	{ PCI_PRODUCT_CMDTECH_648,
	  IDE_PCI_CLASS_OVERRIDE,
	  "CMD Technology PCI0648",
	  cmd0643_9_chip_map,
	},
	{ PCI_PRODUCT_CMDTECH_649,
	  IDE_PCI_CLASS_OVERRIDE,
	  "CMD Technology PCI0649",
	  cmd0643_9_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

const struct pciide_product_desc pciide_via_products[] =  {
	{ PCI_PRODUCT_VIATECH_VT82C586_IDE,
	  0,
	  NULL,
	  apollo_chip_map,
	 },
	{ PCI_PRODUCT_VIATECH_VT82C586A_IDE,
	  0,
	  NULL,
	  apollo_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

const struct pciide_product_desc pciide_cypress_products[] =  {
	{ PCI_PRODUCT_CONTAQ_82C693,
	  0,
	  "Cypress 82C693 IDE Controller",
	  cy693_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

const struct pciide_product_desc pciide_sis_products[] =  {
	{ PCI_PRODUCT_SIS_5597_IDE,
	  0,
	  "Silicon Integrated System 5597/5598 IDE controller",
	  sis_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

const struct pciide_product_desc pciide_acer_products[] =  {
	{ PCI_PRODUCT_ALI_M5229,
	  0,
	  "Acer Labs M5229 UDMA IDE Controller",
	  acer_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

const struct pciide_product_desc pciide_promise_products[] =  {
	{ PCI_PRODUCT_PROMISE_ULTRA33,
	  IDE_PCI_CLASS_OVERRIDE,
	  "Promise Ultra33/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_ULTRA66,
	  IDE_PCI_CLASS_OVERRIDE,
	  "Promise Ultra66/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_ULTRA100,
	  IDE_PCI_CLASS_OVERRIDE,
	  "Promise Ultra100/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ PCI_PRODUCT_PROMISE_ULTRA100X,
	  IDE_PCI_CLASS_OVERRIDE,
	  "Promise Ultra100/ATA Bus Master IDE Accelerator",
	  pdc202xx_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

const struct pciide_product_desc pciide_opti_products[] =  {
	{ PCI_PRODUCT_OPTI_82C621,
	  0,
	  "OPTi 82c621 PCI IDE controller",
	  opti_chip_map,
	},
	{ PCI_PRODUCT_OPTI_82C568,
	  0,
	  "OPTi 82c568 (82c621 compatible) PCI IDE controller",
	  opti_chip_map,
	},
	{ PCI_PRODUCT_OPTI_82D568,
	  0,
	  "OPTi 82d568 (82c621 compatible) PCI IDE controller",
	  opti_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

const struct pciide_product_desc pciide_triones_products[] =  {
	{ PCI_PRODUCT_TRIONES_HPT366,
	  IDE_PCI_CLASS_OVERRIDE,
	  NULL,
	  hpt_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

struct pciide_vendor_desc {
	u_int32_t ide_vendor;
	const struct pciide_product_desc *ide_products;
};

const struct pciide_vendor_desc pciide_vendors[] = {
	{ PCI_VENDOR_INTEL, pciide_intel_products },
	{ PCI_VENDOR_CMDTECH, pciide_cmd_products },
	{ PCI_VENDOR_VIATECH, pciide_via_products },
	{ PCI_VENDOR_CONTAQ, pciide_cypress_products },
	{ PCI_VENDOR_SIS, pciide_sis_products },
	{ PCI_VENDOR_ALI, pciide_acer_products },
	{ PCI_VENDOR_PROMISE, pciide_promise_products },
	{ PCI_VENDOR_AMD, pciide_amd_products },
	{ PCI_VENDOR_OPTI, pciide_opti_products },
	{ PCI_VENDOR_TRIONES, pciide_triones_products },
	{ 0, NULL }
};

/* options passed via the 'flags' config keyword */
#define PCIIDE_OPTIONS_DMA	0x01

int	pciide_match __P((struct device *, struct cfdata *, void *));
void	pciide_attach __P((struct device *, struct device *, void *));

struct cfattach pciide_ca = {
	sizeof(struct pciide_softc), pciide_match, pciide_attach
};
int	pciide_chipen __P((struct pciide_softc *, struct pci_attach_args *));
int	pciide_mapregs_compat __P(( struct pci_attach_args *,
	    struct pciide_channel *, int, bus_size_t *, bus_size_t*));
int	pciide_mapregs_native __P((struct pci_attach_args *, 
	    struct pciide_channel *, bus_size_t *, bus_size_t *,
	    int (*pci_intr) __P((void *))));
void	pciide_mapreg_dma __P((struct pciide_softc *,
	    struct pci_attach_args *));
int	pciide_chansetup __P((struct pciide_softc *, int, pcireg_t));
void	pciide_mapchan __P((struct pci_attach_args *,
	    struct pciide_channel *, pcireg_t, bus_size_t *, bus_size_t *,
	    int (*pci_intr) __P((void *))));
int	pciide_chan_candisable __P((struct pciide_channel *));
void	pciide_map_compat_intr __P(( struct pci_attach_args *,
	    struct pciide_channel *, int, int));
int	pciide_print __P((void *, const char *pnp));
int	pciide_compat_intr __P((void *));
int	pciide_pci_intr __P((void *));
const struct pciide_product_desc* pciide_lookup_product __P((u_int32_t));

const struct pciide_product_desc *
pciide_lookup_product(id)
	u_int32_t id;
{
	const struct pciide_product_desc *pp;
	const struct pciide_vendor_desc *vp;

	for (vp = pciide_vendors; vp->ide_products != NULL; vp++)
		if (PCI_VENDOR(id) == vp->ide_vendor)
			break;

	if ((pp = vp->ide_products) == NULL)
		return NULL;

	for (; pp->chip_map != NULL; pp++)
		if (PCI_PRODUCT(id) == pp->ide_product)
			break;
    
	if (pp->chip_map == NULL)
		return NULL;
	return pp;
}

int
pciide_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	const struct pciide_product_desc *pp;

	/*
	 * Check the ID register to see that it's a PCI IDE controller.
	 * If it is, we assume that we can deal with it; it _should_
	 * work in a standardized way...
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		return (1);
	}

	/*
	 * Some controllers (e.g. promise Utra-33) don't claim to be PCI IDE
	 * controllers. Let see if we can deal with it anyway.
	 */
	pp = pciide_lookup_product(pa->pa_id);
	if (pp  && (pp->ide_flags & IDE_PCI_CLASS_OVERRIDE)) {
		return (1);
	}

	return (0);
}

void
pciide_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	struct pciide_softc *sc = (struct pciide_softc *)self;
	pcireg_t csr;
	char devinfo[256];
	const char *displaydev;

	sc->sc_pp = pciide_lookup_product(pa->pa_id);
	if (sc->sc_pp == NULL) {
		sc->sc_pp = &default_product_desc;
		pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
		displaydev = devinfo;
	} else
		displaydev = sc->sc_pp->ide_name;

	/* if displaydev == NULL, printf is done in chip-specific map */
	if (displaydev)
		printf(": %s (rev. 0x%02x)\n", displaydev,
		    PCI_REVISION(pa->pa_class));

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
#ifdef WDCDEBUG
	if (wdcdebug_pciide_mask & DEBUG_PROBE)
		pci_conf_print(sc->sc_pc, sc->sc_tag, NULL);
#endif
	sc->sc_pp->chip_map(sc, pa);

	if (sc->sc_dma_ok) {
		csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
		csr |= PCI_COMMAND_MASTER_ENABLE;
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);
	}
	WDCDEBUG_PRINT(("pciide: command/status register=%x\n",
	    pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG)), DEBUG_PROBE);
}

/* tell wether the chip is enabled or not */
int
pciide_chipen(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	pcireg_t csr;
	if ((pa->pa_flags & PCI_FLAGS_IO_ENABLED) == 0) {
		csr = pci_conf_read(sc->sc_pc, sc->sc_tag,
		    PCI_COMMAND_STATUS_REG);
		printf("%s: device disabled (at %s)\n",
	 	   sc->sc_wdcdev.sc_dev.dv_xname,
	  	  (csr & PCI_COMMAND_IO_ENABLE) == 0 ?
		  "device" : "bridge");
		return 0;
	}
	return 1;
}

int
pciide_mapregs_compat(pa, cp, compatchan, cmdsizep, ctlsizep)
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
	int compatchan;
	bus_size_t *cmdsizep, *ctlsizep;
{
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	cp->compat = 1;
	*cmdsizep = PCIIDE_COMPAT_CMD_SIZE;
	*ctlsizep = PCIIDE_COMPAT_CTL_SIZE;

	wdc_cp->cmd_iot = pa->pa_iot;
	if (bus_space_map(wdc_cp->cmd_iot, PCIIDE_COMPAT_CMD_BASE(compatchan),
	    PCIIDE_COMPAT_CMD_SIZE, 0, &wdc_cp->cmd_ioh) != 0) {
		printf("%s: couldn't map %s channel cmd regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return (0);
	}

	wdc_cp->ctl_iot = pa->pa_iot;
	if (bus_space_map(wdc_cp->ctl_iot, PCIIDE_COMPAT_CTL_BASE(compatchan),
	    PCIIDE_COMPAT_CTL_SIZE, 0, &wdc_cp->ctl_ioh) != 0) {
		printf("%s: couldn't map %s channel ctl regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		bus_space_unmap(wdc_cp->cmd_iot, wdc_cp->cmd_ioh,
		    PCIIDE_COMPAT_CMD_SIZE);
		return (0);
	}

	return (1);
}

int
pciide_mapregs_native(pa, cp, cmdsizep, ctlsizep, pci_intr)
	struct pci_attach_args * pa;
	struct pciide_channel *cp;
	bus_size_t *cmdsizep, *ctlsizep;
	int (*pci_intr) __P((void *));
{
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct channel_softc *wdc_cp = &cp->wdc_channel;
	const char *intrstr;
	pci_intr_handle_t intrhandle;

	cp->compat = 0;

	if (sc->sc_pci_ih == NULL) {
		if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
		    pa->pa_intrline, &intrhandle) != 0) {
			printf("%s: couldn't map native-PCI interrupt\n",
			    sc->sc_wdcdev.sc_dev.dv_xname);
			return 0;
		}	
		intrstr = pci_intr_string(pa->pa_pc, intrhandle);
		sc->sc_pci_ih = pci_intr_establish(pa->pa_pc,
		    intrhandle, IPL_BIO, pci_intr, sc);
		if (sc->sc_pci_ih != NULL) {
			printf("%s: using %s for native-PCI interrupt\n",
			    sc->sc_wdcdev.sc_dev.dv_xname,
			    intrstr ? intrstr : "unknown interrupt");
		} else {
			printf("%s: couldn't establish native-PCI interrupt",
			    sc->sc_wdcdev.sc_dev.dv_xname);
			if (intrstr != NULL)
				printf(" at %s", intrstr);
			printf("\n");
			return 0;
		}
	}
	cp->ih = sc->sc_pci_ih;
	if (pci_mapreg_map(pa, PCIIDE_REG_CMD_BASE(wdc_cp->channel),
	    PCI_MAPREG_TYPE_IO, 0,
	    &wdc_cp->cmd_iot, &wdc_cp->cmd_ioh, NULL, cmdsizep) != 0) {
		printf("%s: couldn't map %s channel cmd regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return 0;
	}

	if (pci_mapreg_map(pa, PCIIDE_REG_CTL_BASE(wdc_cp->channel),
	    PCI_MAPREG_TYPE_IO, 0,
	    &wdc_cp->ctl_iot, &cp->ctl_baseioh, NULL, ctlsizep) != 0) {
		printf("%s: couldn't map %s channel ctl regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		bus_space_unmap(wdc_cp->cmd_iot, wdc_cp->cmd_ioh, *cmdsizep);
		return 0;
	}
	/*
	 * In native mode, 4 bytes of I/O space are mapped for the control
	 * register, the control register is at offset 2. Pass the generic
	 * code a handle for only one byte at the rigth offset.
	 */
	if (bus_space_subregion(wdc_cp->ctl_iot, cp->ctl_baseioh, 2, 1,
	    &wdc_cp->ctl_ioh) != 0) {
		printf("%s: unable to subregion %s channel ctl regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		bus_space_unmap(wdc_cp->cmd_iot, wdc_cp->cmd_ioh, *cmdsizep);
		bus_space_unmap(wdc_cp->cmd_iot, cp->ctl_baseioh, *ctlsizep);
		return 0;
	}
	return (1);
}

void
pciide_mapreg_dma(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	pcireg_t maptype;

	/*
	 * Map DMA registers
	 *
	 * Note that sc_dma_ok is the right variable to test to see if
	 * DMA can be done.  If the interface doesn't support DMA,
	 * sc_dma_ok will never be non-zero.  If the DMA regs couldn't
	 * be mapped, it'll be zero.  I.e., sc_dma_ok will only be
	 * non-zero if the interface supports DMA and the registers
	 * could be mapped.
	 *
	 * XXX Note that despite the fact that the Bus Master IDE specs
	 * XXX say that "The bus master IDE function uses 16 bytes of IO
	 * XXX space," some controllers (at least the United
	 * XXX Microelectronics UM8886BF) place it in memory space.
	 */
	maptype = pci_mapreg_type(pa->pa_pc, pa->pa_tag,
	    PCIIDE_REG_BUS_MASTER_DMA);

	switch (maptype) {
	case PCI_MAPREG_TYPE_IO:
	case PCI_MAPREG_MEM_TYPE_32BIT:
		sc->sc_dma_ok = (pci_mapreg_map(pa,
		    PCIIDE_REG_BUS_MASTER_DMA, maptype, 0,
		    &sc->sc_dma_iot, &sc->sc_dma_ioh, NULL, NULL) == 0);
		sc->sc_dmat = pa->pa_dmat;
		if (sc->sc_dma_ok == 0) {
			printf(", but unused (couldn't map registers)");
		} else {
			sc->sc_wdcdev.dma_arg = sc;
			sc->sc_wdcdev.dma_init = pciide_dma_init;
			sc->sc_wdcdev.dma_start = pciide_dma_start;
			sc->sc_wdcdev.dma_finish = pciide_dma_finish;
		}
		break;

	default:
		sc->sc_dma_ok = 0;
		printf(", but unsupported register maptype (0x%x)", maptype);
	}
}

int
pciide_compat_intr(arg)
	void *arg;
{
	struct pciide_channel *cp = arg;

#ifdef DIAGNOSTIC
	/* should only be called for a compat channel */
	if (cp->compat == 0)
		panic("pciide compat intr called for non-compat chan %p\n", cp);
#endif
	return (wdcintr(&cp->wdc_channel));
}

int
pciide_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv;

	rv = 0;
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;

		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		/* if this channel not waiting for intr, skip */
		if ((wdc_cp->ch_flags & WDCF_IRQ_WAIT) == 0)
			continue;

		crv = wdcintr(wdc_cp);
		if (crv == 0)
			;		/* leave rv alone */
		else if (crv == 1)
			rv = 1;		/* claim the intr */
		else if (rv == 0)	/* crv should be -1 in this case */
			rv = crv;	/* if we've done no better, take it */
	}
	return (rv);
}

void
pciide_channel_dma_setup(cp)
	struct pciide_channel *cp;
{
	int drive;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct ata_drive_datas *drvp;

	for (drive = 0; drive < 2; drive++) {
		drvp = &cp->wdc_channel.ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* setup DMA if needed */
		if (((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0) ||
		    sc->sc_dma_ok == 0) {
			drvp->drive_flags &= ~(DRIVE_DMA | DRIVE_UDMA);
			continue;
		}
		if (pciide_dma_table_setup(sc, cp->wdc_channel.channel, drive)
		    != 0) {
			/* Abort DMA setup */
			drvp->drive_flags &= ~(DRIVE_DMA | DRIVE_UDMA);
			continue;
		}
	}
}

int
pciide_dma_table_setup(sc, channel, drive)
	struct pciide_softc *sc;
	int channel, drive;
{
	bus_dma_segment_t seg;
	int error, rseg;
	const bus_size_t dma_table_size =
	    sizeof(struct idedma_table) * NIDEDMA_TABLES;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];

	/* If table was already allocated, just return */
	if (dma_maps->dma_table)
		return 0;

	/* Allocate memory for the DMA tables and map it */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, dma_table_size,
	    IDEDMA_TBL_ALIGN, IDEDMA_TBL_ALIGN, &seg, 1, &rseg,
	    BUS_DMA_NOWAIT)) != 0) {
		printf("%s:%d: unable to allocate table DMA for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    dma_table_size,
	    (caddr_t *)&dma_maps->dma_table,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s:%d: unable to map table DMA for"
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	WDCDEBUG_PRINT(("pciide_dma_table_setup: table at %p len %ld, "
	    "phy 0x%lx\n", dma_maps->dma_table, dma_table_size,
	    seg.ds_addr), DEBUG_PROBE);

	/* Create and load table DMA map for this disk */
	if ((error = bus_dmamap_create(sc->sc_dmat, dma_table_size,
	    1, dma_table_size, IDEDMA_TBL_ALIGN, BUS_DMA_NOWAIT,
	    &dma_maps->dmamap_table)) != 0) {
		printf("%s:%d: unable to create table DMA map for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	if ((error = bus_dmamap_load(sc->sc_dmat,
	    dma_maps->dmamap_table,
	    dma_maps->dma_table,
	    dma_table_size, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s:%d: unable to load table DMA map for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	WDCDEBUG_PRINT(("pciide_dma_table_setup: phy addr of table 0x%lx\n",
	    dma_maps->dmamap_table->dm_segs[0].ds_addr), DEBUG_PROBE);
	/* Create a xfer DMA map for this drive */
	if ((error = bus_dmamap_create(sc->sc_dmat, IDEDMA_BYTE_COUNT_MAX,
	    NIDEDMA_TABLES, IDEDMA_BYTE_COUNT_MAX, IDEDMA_BYTE_COUNT_ALIGN,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &dma_maps->dmamap_xfer)) != 0) {
		printf("%s:%d: unable to create xfer DMA map for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	return 0;
}

int
pciide_dma_init(v, channel, drive, databuf, datalen, flags)
	void *v;
	int channel, drive;
	void *databuf;
	size_t datalen;
	int flags;
{
	struct pciide_softc *sc = v;
	int error, seg;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];

	error = bus_dmamap_load(sc->sc_dmat,
	    dma_maps->dmamap_xfer,
	    databuf, datalen, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s:%d: unable to load xfer DMA map for"
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}

	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_xfer, 0,
	    dma_maps->dmamap_xfer->dm_mapsize,
	    (flags & WDC_DMA_READ) ?
	    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	for (seg = 0; seg < dma_maps->dmamap_xfer->dm_nsegs; seg++) {
#ifdef DIAGNOSTIC
		/* A segment must not cross a 64k boundary */
		{
		u_long phys = dma_maps->dmamap_xfer->dm_segs[seg].ds_addr;
		u_long len = dma_maps->dmamap_xfer->dm_segs[seg].ds_len;
		if ((phys & ~IDEDMA_BYTE_COUNT_MASK) !=
		    ((phys + len - 1) & ~IDEDMA_BYTE_COUNT_MASK)) {
			printf("pciide_dma: segment %d physical addr 0x%lx"
			    " len 0x%lx not properly aligned\n",
			    seg, phys, len);
			panic("pciide_dma: buf align");
		}
		}
#endif
		dma_maps->dma_table[seg].base_addr =
		    htole32(dma_maps->dmamap_xfer->dm_segs[seg].ds_addr);
		dma_maps->dma_table[seg].byte_count =
		    htole32(dma_maps->dmamap_xfer->dm_segs[seg].ds_len &
		    IDEDMA_BYTE_COUNT_MASK);
		WDCDEBUG_PRINT(("\t seg %d len %d addr 0x%x\n",
		   seg, le32toh(dma_maps->dma_table[seg].byte_count),
		   le32toh(dma_maps->dma_table[seg].base_addr)), DEBUG_DMA);

	}
	dma_maps->dma_table[dma_maps->dmamap_xfer->dm_nsegs -1].byte_count |=
	    htole32(IDEDMA_BYTE_COUNT_EOT);

	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_table, 0,
	    dma_maps->dmamap_table->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* Maps are ready. Start DMA function */
#ifdef DIAGNOSTIC
	if (dma_maps->dmamap_table->dm_segs[0].ds_addr & ~IDEDMA_TBL_MASK) {
		printf("pciide_dma_init: addr 0x%lx not properly aligned\n",
		    dma_maps->dmamap_table->dm_segs[0].ds_addr);
		panic("pciide_dma_init: table align");
	}
#endif

	/* Clear status bits */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL + IDEDMA_SCH_OFFSET * channel,
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		IDEDMA_CTL + IDEDMA_SCH_OFFSET * channel));
	/* Write table addr */
	bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_TBL + IDEDMA_SCH_OFFSET * channel,
	    dma_maps->dmamap_table->dm_segs[0].ds_addr);
	/* set read/write */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD + IDEDMA_SCH_OFFSET * channel,
	    (flags & WDC_DMA_READ) ? IDEDMA_CMD_WRITE: 0);
	/* remember flags */
	dma_maps->dma_flags = flags;
	return 0;
}

void
pciide_dma_start(v, channel, drive)
	void *v;
	int channel, drive;
{
	struct pciide_softc *sc = v;

	WDCDEBUG_PRINT(("pciide_dma_start\n"),DEBUG_XFERS);
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD + IDEDMA_SCH_OFFSET * channel,
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		IDEDMA_CMD + IDEDMA_SCH_OFFSET * channel) | IDEDMA_CMD_START);
}

int
pciide_dma_finish(v, channel, drive, force)
	void *v;
	int channel, drive;
	int force;
{
	struct pciide_softc *sc = v;
	u_int8_t status;
	int error = 0;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];

	status = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL + IDEDMA_SCH_OFFSET * channel);
	WDCDEBUG_PRINT(("pciide_dma_finish: status 0x%x\n", status),
	    DEBUG_XFERS);

	if (force == 0 && (status & IDEDMA_CTL_INTR) == 0)
		return WDC_DMAST_NOIRQ;

	/* stop DMA channel */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD + IDEDMA_SCH_OFFSET * channel,
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		IDEDMA_CMD + IDEDMA_SCH_OFFSET * channel) & ~IDEDMA_CMD_START);

	/* Unload the map of the data buffer */
	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_xfer, 0,
	    dma_maps->dmamap_xfer->dm_mapsize,
	    (dma_maps->dma_flags & WDC_DMA_READ) ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, dma_maps->dmamap_xfer);

	if ((status & IDEDMA_CTL_ERR) != 0) {
		printf("%s:%d:%d: bus-master DMA error: status=0x%x\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, channel, drive, status);
		error |= WDC_DMAST_ERR;
	}

	if ((status & IDEDMA_CTL_INTR) == 0) {
		printf("%s:%d:%d: bus-master DMA error: missing interrupt, "
		    "status=0x%x\n", sc->sc_wdcdev.sc_dev.dv_xname, channel,
		    drive, status);
		error |= WDC_DMAST_NOIRQ;
	}

	if ((status & IDEDMA_CTL_ACT) != 0) {
		/* data underrun, may be a valid condition for ATAPI */
		error |= WDC_DMAST_UNDER;
	}
	return error;
}

void
pciide_irqack(chp)
	struct channel_softc *chp;
{
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	/* clear status bits in IDE DMA registers */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL + IDEDMA_SCH_OFFSET * chp->channel,
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		IDEDMA_CTL + IDEDMA_SCH_OFFSET * chp->channel));
}

/* some common code used by several chip_map */
int
pciide_chansetup(sc, channel, interface)
	struct pciide_softc *sc;
	int channel;
	pcireg_t interface;
{
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	sc->wdc_chanarray[channel] = &cp->wdc_channel;
	cp->name = PCIIDE_CHANNEL_NAME(channel);
	cp->wdc_channel.channel = channel;
	cp->wdc_channel.wdc = &sc->sc_wdcdev;
	cp->wdc_channel.ch_queue =
	    malloc(sizeof(struct channel_queue), M_DEVBUF, M_NOWAIT);
	if (cp->wdc_channel.ch_queue == NULL) {
		printf("%s %s channel: "
		    "can't allocate memory for command queue",
		sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return 0;
	}
	printf("%s: %s channel %s to %s mode\n",
	    sc->sc_wdcdev.sc_dev.dv_xname, cp->name,
	    (interface & PCIIDE_INTERFACE_SETTABLE(channel)) ?
	    "configured" : "wired",
	    (interface & PCIIDE_INTERFACE_PCI(channel)) ?
	    "native-PCI" : "compatibility");
	return 1;
}

/* some common code used by several chip channel_map */
void
pciide_mapchan(pa, cp, interface, cmdsizep, ctlsizep, pci_intr)
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
	pcireg_t interface;
	bus_size_t *cmdsizep, *ctlsizep;
	int (*pci_intr) __P((void *));
{
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	if (interface & PCIIDE_INTERFACE_PCI(wdc_cp->channel))
		cp->hw_ok = pciide_mapregs_native(pa, cp, cmdsizep, ctlsizep,
		    pci_intr);
	else 
		cp->hw_ok = pciide_mapregs_compat(pa, cp,
		    wdc_cp->channel, cmdsizep, ctlsizep);

	if (cp->hw_ok == 0)
		return;
	wdc_cp->data32iot = wdc_cp->cmd_iot;
	wdc_cp->data32ioh = wdc_cp->cmd_ioh;
	wdcattach(wdc_cp);
}

/*
 * Generic code to call to know if a channel can be disabled. Return 1
 * if channel can be disabled, 0 if not
 */
int
pciide_chan_candisable(cp)
	struct pciide_channel *cp;
{
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	if ((wdc_cp->ch_drive[0].drive_flags & DRIVE) == 0 &&
	    (wdc_cp->ch_drive[1].drive_flags & DRIVE) == 0) {
		printf("%s: disabling %s channel (no drives)\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		cp->hw_ok = 0;
		return 1;
	}
	return 0;
}

/*
 * generic code to map the compat intr if hw_ok=1 and it is a compat channel.
 * Set hw_ok=0 on failure
 */
void
pciide_map_compat_intr(pa, cp, compatchan, interface)
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
	int compatchan, interface;
{
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	if (cp->hw_ok == 0)
		return;
	if ((interface & PCIIDE_INTERFACE_PCI(wdc_cp->channel)) != 0)
		return;

	cp->ih = pciide_machdep_compat_intr_establish(&sc->sc_wdcdev.sc_dev,
	    pa, compatchan, pciide_compat_intr, cp);
	if (cp->ih == NULL) {
		printf("%s: no compatibility interrupt for use by %s "
		    "channel\n", sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		cp->hw_ok = 0;
	}
}

void
pciide_print_modes(cp)
	struct pciide_channel *cp;
{
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int drive;
	struct channel_softc *chp;
	struct ata_drive_datas *drvp;

	chp = &cp->wdc_channel;
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		printf("%s(%s:%d:%d): using PIO mode %d",
		    drvp->drv_softc->dv_xname,
		    sc->sc_wdcdev.sc_dev.dv_xname,
		    chp->channel, drive, drvp->PIO_mode);
		if (drvp->drive_flags & DRIVE_DMA)
			printf(", DMA mode %d", drvp->DMA_mode);
		if (drvp->drive_flags & DRIVE_UDMA)
			printf(", Ultra-DMA mode %d", drvp->UDMA_mode);
		if (drvp->drive_flags & (DRIVE_DMA | DRIVE_UDMA))
			printf(" (using DMA data transfers)");
		printf("\n");
	}
}

void
default_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	pcireg_t csr;
	int channel, drive;
	struct ata_drive_datas *drvp;
	u_int8_t idedma_ctl;
	bus_size_t cmdsize, ctlsize;
	char *failreason;

	if (pciide_chipen(sc, pa) == 0)
		return;

	if (interface & PCIIDE_INTERFACE_BUS_MASTER_DMA) {
		printf("%s: bus-master DMA support present",
		    sc->sc_wdcdev.sc_dev.dv_xname);
		if (sc->sc_pp == &default_product_desc &&
		    (sc->sc_wdcdev.sc_dev.dv_cfdata->cf_flags &
		    PCIIDE_OPTIONS_DMA) == 0) {
			printf(", but unused (no driver support)");
			sc->sc_dma_ok = 0;
		} else {
			pciide_mapreg_dma(sc, pa);
		if (sc->sc_dma_ok != 0)
			printf(", used without full driver "
			    "support");
		}
	} else {
		printf("%s: hardware does not support DMA",
		    sc->sc_wdcdev.sc_dev.dv_xname);
		sc->sc_dma_ok = 0;
	}
	printf("\n");
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 0;
	sc->sc_wdcdev.DMA_cap = 0;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16;

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if (interface & PCIIDE_INTERFACE_PCI(channel)) {
			cp->hw_ok = pciide_mapregs_native(pa, cp, &cmdsize,
			    &ctlsize, pciide_pci_intr);
		} else {
			cp->hw_ok = pciide_mapregs_compat(pa, cp,
			    channel, &cmdsize, &ctlsize);
		}
		if (cp->hw_ok == 0)
			continue;
		/*
		 * Check to see if something appears to be there.
		 */
		failreason = NULL;
		if (!wdcprobe(&cp->wdc_channel)) {
			failreason = "not responding; disabled or no drives?";
			goto next;
		}
		/*
		 * Now, make sure it's actually attributable to this PCI IDE
		 * channel by trying to access the channel again while the
		 * PCI IDE controller's I/O space is disabled.  (If the
		 * channel no longer appears to be there, it belongs to
		 * this controller.)  YUCK!
		 */
		csr = pci_conf_read(sc->sc_pc, sc->sc_tag,
		    PCI_COMMAND_STATUS_REG);
		pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG,
		    csr & ~PCI_COMMAND_IO_ENABLE);
		if (wdcprobe(&cp->wdc_channel))
			failreason = "other hardware responding at addresses";
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    PCI_COMMAND_STATUS_REG, csr);
next:
		if (failreason) {
			printf("%s: %s channel ignored (%s)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name,
			    failreason);
			cp->hw_ok = 0;
			bus_space_unmap(cp->wdc_channel.cmd_iot,
			    cp->wdc_channel.cmd_ioh, cmdsize);
			bus_space_unmap(cp->wdc_channel.ctl_iot,
			    cp->wdc_channel.ctl_ioh, ctlsize);
		} else {
			pciide_map_compat_intr(pa, cp, channel, interface);
		}
		if (cp->hw_ok) {
			cp->wdc_channel.data32iot = cp->wdc_channel.cmd_iot;
			cp->wdc_channel.data32ioh = cp->wdc_channel.cmd_ioh;
			wdcattach(&cp->wdc_channel);
		}
	}

	if (sc->sc_dma_ok == 0)
		return;

	/* Allocate DMA maps */
	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		idedma_ctl = 0;
		cp = &sc->pciide_channels[channel];
		for (drive = 0; drive < 2; drive++) {
			drvp = &cp->wdc_channel.ch_drive[drive];
			/* If no drive, skip */
			if ((drvp->drive_flags & DRIVE) == 0)
				continue;
			if ((drvp->drive_flags & DRIVE_DMA) == 0)
				continue;
			if (pciide_dma_table_setup(sc, channel, drive) != 0) {
				/* Abort DMA setup */
				printf("%s:%d:%d: can't allocate DMA maps, "
				    "using PIO transfers\n",
				    sc->sc_wdcdev.sc_dev.dv_xname,
				    channel, drive);
				drvp->drive_flags &= ~DRIVE_DMA;
			}
			printf("%s:%d:%d: using DMA data transfers\n",
			    sc->sc_wdcdev.sc_dev.dv_xname,
			    channel, drive);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
		if (idedma_ctl != 0) {
			/* Add software bits in status register */
			bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_CTL + (IDEDMA_SCH_OFFSET * channel),
			    idedma_ctl);
		}
	}
}

void
piix_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int channel;
	u_int32_t idetim;
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;

	printf("%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
		switch(sc->sc_pp->ide_product) {
		case PCI_PRODUCT_INTEL_82371AB_IDE:
		case PCI_PRODUCT_INTEL_82801AA_IDE:
		case PCI_PRODUCT_INTEL_82801AB_IDE:
		case PCI_PRODUCT_INTEL_82801BA_IDE:
		case PCI_PRODUCT_INTEL_82801BAM_IDE:
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
		}
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	switch(sc->sc_pp->ide_product) {
	case PCI_PRODUCT_INTEL_82801AA_IDE:
		sc->sc_wdcdev.UDMA_cap = 4;
		break;
	case PCI_PRODUCT_INTEL_82801BA_IDE:
	case PCI_PRODUCT_INTEL_82801BAM_IDE:
		sc->sc_wdcdev.UDMA_cap = 5;
		break;
	default:
		sc->sc_wdcdev.UDMA_cap = 2;
	}
	if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82371FB_IDE)
		sc->sc_wdcdev.set_modes = piix_setup_channel;
	else
		sc->sc_wdcdev.set_modes = piix3_4_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	WDCDEBUG_PRINT(("piix_setup_chip: old idetim=0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM)),
	    DEBUG_PROBE);
	if (sc->sc_pp->ide_product != PCI_PRODUCT_INTEL_82371FB_IDE) {
		WDCDEBUG_PRINT((", sidetim=0x%x",
		    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM)),
		    DEBUG_PROBE);
		if (sc->sc_wdcdev.cap & WDC_CAPABILITY_UDMA) {
			WDCDEBUG_PRINT((", udamreg 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG)),
			    DEBUG_PROBE);
		}
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE) {
			WDCDEBUG_PRINT((", IDE_CONTROL 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_CONFIG)),
			    DEBUG_PROBE);
		}

	}
	WDCDEBUG_PRINT(("\n"), DEBUG_PROBE);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		/* PIIX is compat-only */
		if (pciide_chansetup(sc, channel, 0) == 0)
			continue;
		idetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM);
		if ((PIIX_IDETIM_READ(idetim, channel) &
		    PIIX_IDETIM_IDE) == 0) {
			printf("%s: %s channel ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		/* PIIX are compat-only pciide devices */
		pciide_mapchan(pa, cp, 0, &cmdsize, &ctlsize, pciide_pci_intr);
		if (cp->hw_ok == 0)
			continue;
		if (pciide_chan_candisable(cp)) {
			idetim = PIIX_IDETIM_CLEAR(idetim, PIIX_IDETIM_IDE,
			    channel);
			pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_IDETIM,
			    idetim);
		}
		pciide_map_compat_intr(pa, cp, channel, 0);
		if (cp->hw_ok == 0)
			continue;
		sc->sc_wdcdev.set_modes(&cp->wdc_channel);
	}

	WDCDEBUG_PRINT(("piix_setup_chip: idetim=0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM)),
	    DEBUG_PROBE);
	if (sc->sc_pp->ide_product != PCI_PRODUCT_INTEL_82371FB_IDE) {
		WDCDEBUG_PRINT((", sidetim=0x%x",
		    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM)),
		    DEBUG_PROBE);
		if (sc->sc_wdcdev.cap & WDC_CAPABILITY_UDMA) {
			WDCDEBUG_PRINT((", udamreg 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG)),
			    DEBUG_PROBE);
		}
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE) {
			WDCDEBUG_PRINT((", IDE_CONTROL 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_CONFIG)),
			    DEBUG_PROBE);
		}
	}
	WDCDEBUG_PRINT(("\n"), DEBUG_PROBE);
}

void
piix_setup_channel(chp)
	struct channel_softc *chp;
{
	u_int8_t mode[2], drive;
	u_int32_t oidetim, idetim, idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	struct ata_drive_datas *drvp = cp->wdc_channel.ch_drive;

	oidetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM);
	idetim = PIIX_IDETIM_CLEAR(oidetim, 0xffff, chp->channel);
	idedma_ctl = 0;

	/* set up new idetim: Enable IDE registers decode */
	idetim = PIIX_IDETIM_SET(idetim, PIIX_IDETIM_IDE,
	    chp->channel);

	/* setup DMA */
	pciide_channel_dma_setup(cp);

	/*
	 * Here we have to mess up with drives mode: PIIX can't have
	 * different timings for master and slave drives.
	 * We need to find the best combination.
	 */

	/* If both drives supports DMA, take the lower mode */
	if ((drvp[0].drive_flags & DRIVE_DMA) &&
	    (drvp[1].drive_flags & DRIVE_DMA)) {
		mode[0] = mode[1] =
		    min(drvp[0].DMA_mode, drvp[1].DMA_mode);
		    drvp[0].DMA_mode = mode[0];
		    drvp[1].DMA_mode = mode[1];
		goto ok;
	}
	/*
	 * If only one drive supports DMA, use its mode, and
	 * put the other one in PIO mode 0 if mode not compatible
	 */
	if (drvp[0].drive_flags & DRIVE_DMA) {
		mode[0] = drvp[0].DMA_mode;
		mode[1] = drvp[1].PIO_mode;
		if (piix_isp_pio[mode[1]] != piix_isp_dma[mode[0]] ||
		    piix_rtc_pio[mode[1]] != piix_rtc_dma[mode[0]])
			mode[1] = drvp[1].PIO_mode = 0;
		goto ok;
	}
	if (drvp[1].drive_flags & DRIVE_DMA) {
		mode[1] = drvp[1].DMA_mode;
		mode[0] = drvp[0].PIO_mode;
		if (piix_isp_pio[mode[0]] != piix_isp_dma[mode[1]] ||
		    piix_rtc_pio[mode[0]] != piix_rtc_dma[mode[1]])
			mode[0] = drvp[0].PIO_mode = 0;
		goto ok;
	}
	/*
	 * If both drives are not DMA, takes the lower mode, unless
	 * one of them is PIO mode < 2
	 */
	if (drvp[0].PIO_mode < 2) {
		mode[0] = drvp[0].PIO_mode = 0;
		mode[1] = drvp[1].PIO_mode;
	} else if (drvp[1].PIO_mode < 2) {
		mode[1] = drvp[1].PIO_mode = 0;
		mode[0] = drvp[0].PIO_mode;
	} else {
		mode[0] = mode[1] =
		    min(drvp[1].PIO_mode, drvp[0].PIO_mode);
		drvp[0].PIO_mode = mode[0];
		drvp[1].PIO_mode = mode[1];
	}
ok:	/* The modes are setup */
	for (drive = 0; drive < 2; drive++) {
		if (drvp[drive].drive_flags & DRIVE_DMA) {
			idetim |= piix_setup_idetim_timings(
			    mode[drive], 1, chp->channel);
			goto end;
		}
	}
	/* If we are there, none of the drives are DMA */
	if (mode[0] >= 2)
		idetim |= piix_setup_idetim_timings(
		    mode[0], 0, chp->channel);
	else 
		idetim |= piix_setup_idetim_timings(
		    mode[1], 0, chp->channel);
end:	/*
	 * timing mode is now set up in the controller. Enable
	 * it per-drive
	 */
	for (drive = 0; drive < 2; drive++) {
		/* If no drive, skip */
		if ((drvp[drive].drive_flags & DRIVE) == 0)
			continue;
		idetim |= piix_setup_idetim_drvs(&drvp[drive]);
		if (drvp[drive].drive_flags & DRIVE_DMA)
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL + (IDEDMA_SCH_OFFSET * chp->channel),
		    idedma_ctl);
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_IDETIM, idetim);
	pciide_print_modes(cp);
}

void
piix3_4_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	u_int32_t oidetim, idetim, sidetim, udmareg, ideconf, idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int drive;
	int channel = chp->channel;

	oidetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM);
	sidetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM);
	udmareg = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG);
	ideconf = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_CONFIG);
	idetim = PIIX_IDETIM_CLEAR(oidetim, 0xffff, channel);
	sidetim &= ~(PIIX_SIDETIM_ISP_MASK(channel) |
	    PIIX_SIDETIM_RTC_MASK(channel));

	idedma_ctl = 0;
	/* If channel disabled, no need to go further */
	if ((PIIX_IDETIM_READ(oidetim, channel) & PIIX_IDETIM_IDE) == 0)
		return;
	/* set up new idetim: Enable IDE registers decode */
	idetim = PIIX_IDETIM_SET(idetim, PIIX_IDETIM_IDE, channel);

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		udmareg &= ~(PIIX_UDMACTL_DRV_EN(channel, drive) |
		    PIIX_UDMATIM_SET(0x3, channel, drive));
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		if (((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0))
			goto pio;

		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE) {
			ideconf |= PIIX_CONFIG_PINGPONG;
		}
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE) {
			/* setup Ultra/100 */
			if (drvp->UDMA_mode > 2 &&
			    (ideconf & PIIX_CONFIG_CR(channel, drive)) == 0)
				drvp->UDMA_mode = 2;
			if (drvp->UDMA_mode > 4) {
				ideconf |= PIIX_CONFIG_UDMA100(channel, drive);
			} else {
				ideconf &= ~PIIX_CONFIG_UDMA100(channel, drive);
				if (drvp->UDMA_mode > 2) {
					ideconf |= PIIX_CONFIG_UDMA66(channel,
					    drive);
				} else {
					ideconf &= ~PIIX_CONFIG_UDMA66(channel,
					    drive);
				}
			}
		}
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AA_IDE) {
			/* setup Ultra/66 */
			if (drvp->UDMA_mode > 2 &&
			    (ideconf & PIIX_CONFIG_CR(channel, drive)) == 0)
				drvp->UDMA_mode = 2;
			if (drvp->UDMA_mode > 2)
				ideconf |= PIIX_CONFIG_UDMA66(channel, drive);
			else
				ideconf &= ~PIIX_CONFIG_UDMA66(channel, drive);
		}
		if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) &&
		    (drvp->drive_flags & DRIVE_UDMA)) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			udmareg |= PIIX_UDMACTL_DRV_EN( channel, drive);
			udmareg |= PIIX_UDMATIM_SET(
			    piix4_sct_udma[drvp->UDMA_mode], channel, drive);
		} else {
			/* use Multiword DMA */
			drvp->drive_flags &= ~DRIVE_UDMA;
			if (drive == 0) {
				idetim |= piix_setup_idetim_timings(
				    drvp->DMA_mode, 1, channel);
			} else {
				sidetim |= piix_setup_sidetim_timings(
					drvp->DMA_mode, 1, channel);
				idetim =PIIX_IDETIM_SET(idetim,
				    PIIX_IDETIM_SITRE, channel);
			}
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
	
pio:		/* use PIO mode */
		idetim |= piix_setup_idetim_drvs(drvp);
		if (drive == 0) {
			idetim |= piix_setup_idetim_timings(
			    drvp->PIO_mode, 0, channel);
		} else {
			sidetim |= piix_setup_sidetim_timings(
				drvp->PIO_mode, 0, channel);
			idetim =PIIX_IDETIM_SET(idetim,
			    PIIX_IDETIM_SITRE, channel);
		}
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL + (IDEDMA_SCH_OFFSET * channel),
		    idedma_ctl);
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_IDETIM, idetim);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM, sidetim);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG, udmareg);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_CONFIG, ideconf);
	pciide_print_modes(cp);
}


/* setup ISP and RTC fields, based on mode */
static u_int32_t
piix_setup_idetim_timings(mode, dma, channel)
	u_int8_t mode;
	u_int8_t dma;
	u_int8_t channel;
{
	
	if (dma)
		return PIIX_IDETIM_SET(0,
		    PIIX_IDETIM_ISP_SET(piix_isp_dma[mode]) | 
		    PIIX_IDETIM_RTC_SET(piix_rtc_dma[mode]),
		    channel);
	else 
		return PIIX_IDETIM_SET(0,
		    PIIX_IDETIM_ISP_SET(piix_isp_pio[mode]) | 
		    PIIX_IDETIM_RTC_SET(piix_rtc_pio[mode]),
		    channel);
}

/* setup DTE, PPE, IE and TIME field based on PIO mode */
static u_int32_t
piix_setup_idetim_drvs(drvp)
	struct ata_drive_datas *drvp;
{
	u_int32_t ret = 0;
	struct channel_softc *chp = drvp->chnl_softc;
	u_int8_t channel = chp->channel;
	u_int8_t drive = drvp->drive;

	/*
	 * If drive is using UDMA, timings setups are independant
	 * So just check DMA and PIO here.
	 */
	if (drvp->drive_flags & DRIVE_DMA) {
		/* if mode = DMA mode 0, use compatible timings */
		if ((drvp->drive_flags & DRIVE_DMA) &&
		    drvp->DMA_mode == 0) {
			drvp->PIO_mode = 0;
			return ret;
		}
		ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_TIME(drive), channel);
		/*
		 * PIO and DMA timings are the same, use fast timings for PIO
		 * too, else use compat timings.
		 */
		if ((piix_isp_pio[drvp->PIO_mode] !=
		    piix_isp_dma[drvp->DMA_mode]) ||
		    (piix_rtc_pio[drvp->PIO_mode] !=
		    piix_rtc_dma[drvp->DMA_mode]))
			drvp->PIO_mode = 0;
		/* if PIO mode <= 2, use compat timings for PIO */
		if (drvp->PIO_mode <= 2) {
			ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_DTE(drive),
			    channel);
			return ret;
		}
	}

	/*
	 * Now setup PIO modes. If mode < 2, use compat timings.
	 * Else enable fast timings. Enable IORDY and prefetch/post
	 * if PIO mode >= 3.
	 */

	if (drvp->PIO_mode < 2)
		return ret;

	ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_TIME(drive), channel);
	if (drvp->PIO_mode >= 3) {
		ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_IE(drive), channel);
		ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_PPE(drive), channel);
	}
	return ret;
}

/* setup values in SIDETIM registers, based on mode */
static u_int32_t
piix_setup_sidetim_timings(mode, dma, channel)
	u_int8_t mode;
	u_int8_t dma;
	u_int8_t channel;
{
	if (dma)
		return PIIX_SIDETIM_ISP_SET(piix_isp_dma[mode], channel) |
		    PIIX_SIDETIM_RTC_SET(piix_rtc_dma[mode], channel);
	else 
		return PIIX_SIDETIM_ISP_SET(piix_isp_pio[mode], channel) |
		    PIIX_SIDETIM_RTC_SET(piix_rtc_pio[mode], channel);
}

void
amd7x6_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	int channel;
	pcireg_t chanenable;
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;
	printf("%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;

	if (sc->sc_pp->ide_product == PCI_PRODUCT_AMD_PBC766_IDE)
		sc->sc_wdcdev.UDMA_cap = 5;
	else
		sc->sc_wdcdev.UDMA_cap = 4;
	sc->sc_wdcdev.set_modes = amd7x6_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;
	chanenable = pci_conf_read(sc->sc_pc, sc->sc_tag, AMD7X6_CHANSTATUS_EN);

	WDCDEBUG_PRINT(("amd7x6_chip_map: Channel enable=0x%x\n", chanenable),
	    DEBUG_PROBE);
	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;

		if ((chanenable & AMD7X6_CHAN_EN(channel)) == 0) {
			printf("%s: %s channel ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);

		if (pciide_chan_candisable(cp))
			chanenable &= ~AMD7X6_CHAN_EN(channel);
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;

		amd7x6_setup_channel(&cp->wdc_channel);
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, AMD7X6_CHANSTATUS_EN,
	    chanenable);
	return;
}

void
amd7x6_setup_channel(chp)
	struct channel_softc *chp;
{
	u_int32_t udmatim_reg, datatim_reg;
	u_int8_t idedma_ctl;
	int mode, drive;
	struct ata_drive_datas *drvp;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
#ifndef PCIIDE_AMD756_ENABLEDMA
	int rev = PCI_REVISION(
	    pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_CLASS_REG));
#endif

	idedma_ctl = 0;
	datatim_reg = pci_conf_read(sc->sc_pc, sc->sc_tag, AMD7X6_DATATIM);
	udmatim_reg = pci_conf_read(sc->sc_pc, sc->sc_tag, AMD7X6_UDMA);
	datatim_reg &= ~AMD7X6_DATATIM_MASK(chp->channel);
	udmatim_reg &= ~AMD7X6_UDMA_MASK(chp->channel);

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		if (((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0)) {
			mode = drvp->PIO_mode;
			goto pio;
		}
		if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) &&
		    (drvp->drive_flags & DRIVE_UDMA)) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			udmatim_reg |= AMD7X6_UDMA_EN(chp->channel, drive) |
			    AMD7X6_UDMA_EN_MTH(chp->channel, drive) |
			    AMD7X6_UDMA_TIME(chp->channel, drive,
				amd7x6_udma_tim[drvp->UDMA_mode]);
			/* can use PIO timings, MW DMA unused */
			mode = drvp->PIO_mode;
		} else {
			/* use Multiword DMA, but only if revision is OK */
			drvp->drive_flags &= ~DRIVE_UDMA;
#ifndef PCIIDE_AMD756_ENABLEDMA
			/*
			 * The workaround doesn't seem to be necessary
			 * with all drives, so it can be disabled by
			 * PCIIDE_AMD756_ENABLEDMA. It causes a hard hang if
			 * triggered. 
			 */
			if (sc->sc_pp->ide_product ==
			      PCI_PRODUCT_AMD_PBC756_IDE &&
			    AMD756_CHIPREV_DISABLEDMA(rev)) {
				printf("%s:%d:%d: multi-word DMA disabled due "
				    "to chip revision\n",
				    sc->sc_wdcdev.sc_dev.dv_xname,
				    chp->channel, drive);
				mode = drvp->PIO_mode;
				drvp->drive_flags &= ~DRIVE_DMA;
				goto pio;
			}
#endif
			/* mode = min(pio, dma+2) */
			if (drvp->PIO_mode <= (drvp->DMA_mode +2))
				mode = drvp->PIO_mode;
			else
				mode = drvp->DMA_mode + 2;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

pio:		/* setup PIO mode */
		if (mode <= 2) {
			drvp->DMA_mode = 0;
			drvp->PIO_mode = 0;
			mode = 0;
		} else {
			drvp->PIO_mode = mode;
			drvp->DMA_mode = mode - 2;
		}
		datatim_reg |=
		    AMD7X6_DATATIM_PULSE(chp->channel, drive,
			amd7x6_pio_set[mode]) |
		    AMD7X6_DATATIM_RECOV(chp->channel, drive,
			amd7x6_pio_rec[mode]);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL + (IDEDMA_SCH_OFFSET * chp->channel),
		    idedma_ctl);
	}
	pciide_print_modes(cp);
	pci_conf_write(sc->sc_pc, sc->sc_tag, AMD7X6_DATATIM, datatim_reg);
	pci_conf_write(sc->sc_pc, sc->sc_tag, AMD7X6_UDMA, udmatim_reg);
}

void
apollo_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	int channel;
	u_int32_t ideconf;
	bus_size_t cmdsize, ctlsize;
	pcitag_t pcib_tag;
	pcireg_t pcib_id, pcib_class;

	if (pciide_chipen(sc, pa) == 0)
		return;
	/* get a PCI tag for the ISA bridge (function 0 of the same device) */
	pcib_tag = pci_make_tag(pa->pa_pc, pa->pa_bus, pa->pa_device, 0);
	/* and read ID and rev of the ISA bridge */
	pcib_id = pci_conf_read(sc->sc_pc, pcib_tag, PCI_ID_REG);
	pcib_class = pci_conf_read(sc->sc_pc, pcib_tag, PCI_CLASS_REG);
	printf(": VIA Technologies ");
	switch (PCI_PRODUCT(pcib_id)) {
	case PCI_PRODUCT_VIATECH_VT82C586_ISA:
		printf("VT82C586 (Apollo VP) ");
		if(PCI_REVISION(pcib_class) >= 0x02) {
			printf("ATA33 controller\n");
			sc->sc_wdcdev.UDMA_cap = 2;
		} else {
			printf("controller\n");
			sc->sc_wdcdev.UDMA_cap = 0;
		}
		break;
	case PCI_PRODUCT_VIATECH_VT82C596A:
		printf("VT82C596A (Apollo Pro) ");
		if (PCI_REVISION(pcib_class) >= 0x12) {
			printf("ATA66 controller\n");
			sc->sc_wdcdev.UDMA_cap = 4;
		} else {
			printf("ATA33 controller\n");
			sc->sc_wdcdev.UDMA_cap = 2;
		}
		break;
	case PCI_PRODUCT_VIATECH_VT82C686A_ISA:
		printf("VT82C686A (Apollo KX133) ");
		if (PCI_REVISION(pcib_class) >= 0x40) {
			printf("ATA100 controller\n");
			sc->sc_wdcdev.UDMA_cap = 5;
		} else {
			printf("ATA66 controller\n");
			sc->sc_wdcdev.UDMA_cap = 4;
		}
		break;
	default:
		printf("unknown ATA controller\n");
		sc->sc_wdcdev.UDMA_cap = 0;
	}
			
	printf("%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
		if (sc->sc_wdcdev.UDMA_cap > 0)
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.set_modes = apollo_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	WDCDEBUG_PRINT(("apollo_chip_map: old APO_IDECONF=0x%x, "
	    "APO_CTLMISC=0x%x, APO_DATATIM=0x%x, APO_UDMA=0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_IDECONF),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_CTLMISC),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_DATATIM),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_UDMA)),
	    DEBUG_PROBE);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;

		ideconf = pci_conf_read(sc->sc_pc, sc->sc_tag, APO_IDECONF);
		if ((ideconf & APO_IDECONF_EN(channel)) == 0) {
			printf("%s: %s channel ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);
		if (cp->hw_ok == 0)
			continue;
		if (pciide_chan_candisable(cp)) {
			ideconf &= ~APO_IDECONF_EN(channel);
			pci_conf_write(sc->sc_pc, sc->sc_tag, APO_IDECONF,
			    ideconf);
		}
		pciide_map_compat_intr(pa, cp, channel, interface);

		if (cp->hw_ok == 0)
			continue;
		apollo_setup_channel(&sc->pciide_channels[channel].wdc_channel);
	}
	WDCDEBUG_PRINT(("apollo_chip_map: APO_DATATIM=0x%x, APO_UDMA=0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_DATATIM),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, APO_UDMA)), DEBUG_PROBE);
}

void
apollo_setup_channel(chp)
	struct channel_softc *chp;
{
	u_int32_t udmatim_reg, datatim_reg;
	u_int8_t idedma_ctl;
	int mode, drive;
	struct ata_drive_datas *drvp;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	idedma_ctl = 0;
	datatim_reg = pci_conf_read(sc->sc_pc, sc->sc_tag, APO_DATATIM);
	udmatim_reg = pci_conf_read(sc->sc_pc, sc->sc_tag, APO_UDMA);
	datatim_reg &= ~APO_DATATIM_MASK(chp->channel);
	udmatim_reg &= ~APO_UDMA_MASK(chp->channel);

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		if (((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0)) {
			mode = drvp->PIO_mode;
			goto pio;
		}
		if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) &&
		    (drvp->drive_flags & DRIVE_UDMA)) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			udmatim_reg |= APO_UDMA_EN(chp->channel, drive) |
			    APO_UDMA_EN_MTH(chp->channel, drive);
			if (sc->sc_wdcdev.UDMA_cap == 5) {
				/* 686b */
				udmatim_reg |= APO_UDMA_CLK66(chp->channel);
				udmatim_reg |= APO_UDMA_TIME(chp->channel,
				    drive, apollo_udma100_tim[drvp->UDMA_mode]);
			} else if (sc->sc_wdcdev.UDMA_cap == 4) {
				/* 596b or 686a */
				udmatim_reg |= APO_UDMA_CLK66(chp->channel);
				udmatim_reg |= APO_UDMA_TIME(chp->channel,
				    drive, apollo_udma66_tim[drvp->UDMA_mode]);
			} else {
				/* 596a or 586b */
				udmatim_reg |= APO_UDMA_TIME(chp->channel,
				    drive, apollo_udma33_tim[drvp->UDMA_mode]);
			}
			/* can use PIO timings, MW DMA unused */
			mode = drvp->PIO_mode;
		} else {
			/* use Multiword DMA */
			drvp->drive_flags &= ~DRIVE_UDMA;
			/* mode = min(pio, dma+2) */
			if (drvp->PIO_mode <= (drvp->DMA_mode +2))
				mode = drvp->PIO_mode;
			else
				mode = drvp->DMA_mode + 2;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

pio:		/* setup PIO mode */
		if (mode <= 2) {
			drvp->DMA_mode = 0;
			drvp->PIO_mode = 0;
			mode = 0;
		} else {
			drvp->PIO_mode = mode;
			drvp->DMA_mode = mode - 2;
		}
		datatim_reg |=
		    APO_DATATIM_PULSE(chp->channel, drive,
			apollo_pio_set[mode]) |
		    APO_DATATIM_RECOV(chp->channel, drive,
			apollo_pio_rec[mode]);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL + (IDEDMA_SCH_OFFSET * chp->channel),
		    idedma_ctl);
	}
	pciide_print_modes(cp);
	pci_conf_write(sc->sc_pc, sc->sc_tag, APO_DATATIM, datatim_reg);
	pci_conf_write(sc->sc_pc, sc->sc_tag, APO_UDMA, udmatim_reg);
}

void
cmd_channel_map(pa, sc, channel)
	struct pci_attach_args *pa;
	struct pciide_softc *sc;
	int channel;
{
	struct pciide_channel *cp = &sc->pciide_channels[channel];
	bus_size_t cmdsize, ctlsize;
	u_int8_t ctrl = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_CTRL);
	int interface;

	/* 
	 * The 0648/0649 can be told to identify as a RAID controller.
	 * In this case, we have to fake interface
	 */
	if (PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCIIDE_INTERFACE_SETTABLE(0) |
		    PCIIDE_INTERFACE_SETTABLE(1);
		if (pciide_pci_read(pa->pa_pc, pa->pa_tag, CMD_CONF) &
		    CMD_CONF_DSA1)
			interface |= PCIIDE_INTERFACE_PCI(0) |
			    PCIIDE_INTERFACE_PCI(1);
	} else {
		interface = PCI_INTERFACE(pa->pa_class);
	}

	sc->wdc_chanarray[channel] = &cp->wdc_channel;
	cp->name = PCIIDE_CHANNEL_NAME(channel);
	cp->wdc_channel.channel = channel;
	cp->wdc_channel.wdc = &sc->sc_wdcdev;

	if (channel > 0) {
		cp->wdc_channel.ch_queue =
		    sc->pciide_channels[0].wdc_channel.ch_queue;
	} else {
		cp->wdc_channel.ch_queue =
		    malloc(sizeof(struct channel_queue), M_DEVBUF, M_NOWAIT);
	}
	if (cp->wdc_channel.ch_queue == NULL) {
		printf("%s %s channel: "
		    "can't allocate memory for command queue",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		    return;
	}

	printf("%s: %s channel %s to %s mode\n",
	    sc->sc_wdcdev.sc_dev.dv_xname, cp->name,
	    (interface & PCIIDE_INTERFACE_SETTABLE(channel)) ?
	    "configured" : "wired",
	    (interface & PCIIDE_INTERFACE_PCI(channel)) ?
	    "native-PCI" : "compatibility");

	/*
	 * with a CMD PCI64x, if we get here, the first channel is enabled:
	 * there's no way to disable the first channel without disabling
	 * the whole device
	 */
	if (channel != 0 && (ctrl & CMD_CTRL_2PORT) == 0) {
		printf("%s: %s channel ignored (disabled)\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return;
	}

	pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize, cmd_pci_intr);
	if (cp->hw_ok == 0)
		return;
	if (channel == 1) {
		if (pciide_chan_candisable(cp)) {
			ctrl &= ~CMD_CTRL_2PORT;
			pciide_pci_write(pa->pa_pc, pa->pa_tag,
			    CMD_CTRL, ctrl);
		}
	}
	pciide_map_compat_intr(pa, cp, channel, interface);
}

int
cmd_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv; 
	u_int32_t priirq, secirq;

	rv = 0;
	priirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_CONF);
	secirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_ARTTIM23);
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		if ((i == 0 && (priirq & CMD_CONF_DRV0_INTR)) ||
		    (i == 1 && (secirq & CMD_ARTTIM23_IRQ))) {
			crv = wdcintr(wdc_cp);
			if (crv == 0)
				printf("%s:%d: bogus intr\n",
				    sc->sc_wdcdev.sc_dev.dv_xname, i);
			else
				rv = 1;
		}
	}
	return rv;
}

void
cmd_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	int channel;

	/*
	 * For a CMD PCI064x, the use of PCI_COMMAND_IO_ENABLE
	 * and base adresses registers can be disabled at
	 * hardware level. In this case, the device is wired
	 * in compat mode and its first channel is always enabled,
	 * but we can't rely on PCI_COMMAND_IO_ENABLE.
	 * In fact, it seems that the first channel of the CMD PCI0640
	 * can't be disabled.
	 */

#ifdef PCIIDE_CMD064x_DISABLE
	if (pciide_chipen(sc, pa) == 0)
		return;
#endif

	printf("%s: hardware does not support DMA\n",
	    sc->sc_wdcdev.sc_dev.dv_xname);
	sc->sc_dma_ok = 0;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16;

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cmd_channel_map(pa, sc, channel);
	}
}

void
cmd0643_9_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{	 
	struct pciide_channel *cp;
	int channel;
	int rev = PCI_REVISION(
	    pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_CLASS_REG));

	/*
	 * For a CMD PCI064x, the use of PCI_COMMAND_IO_ENABLE
	 * and base adresses registers can be disabled at
	 * hardware level. In this case, the device is wired
	 * in compat mode and its first channel is always enabled,
	 * but we can't rely on PCI_COMMAND_IO_ENABLE.
	 * In fact, it seems that the first channel of the CMD PCI0640
	 * can't be disabled.
	 */

#ifdef PCIIDE_CMD064x_DISABLE
	if (pciide_chipen(sc, pa) == 0)
		return;
#endif
	printf("%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		switch (sc->sc_pp->ide_product) {
		case PCI_PRODUCT_CMDTECH_649:
		case PCI_PRODUCT_CMDTECH_648:
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
			sc->sc_wdcdev.UDMA_cap = 4;
			sc->sc_wdcdev.irqack = cmd646_9_irqack;
			break;
		case PCI_PRODUCT_CMDTECH_646:
			if (rev >= CMD0646U2_REV) {
				sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
				sc->sc_wdcdev.UDMA_cap = 2;
			} else if (rev >= CMD0646U_REV) {
			/*
			 * Linux's driver claims that the 646U is broken
			 * with UDMA. Only enable it if we know what we're
			 * doing
			 */
#ifdef PCIIDE_CMD0646U_ENABLEUDMA
				sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
				sc->sc_wdcdev.UDMA_cap = 2;
#endif
				/* explicitely disable UDMA */
				pciide_pci_write(sc->sc_pc, sc->sc_tag,
				    CMD_UDMATIM(0), 0);
				pciide_pci_write(sc->sc_pc, sc->sc_tag,
				    CMD_UDMATIM(1), 0);
			}
			sc->sc_wdcdev.irqack = cmd646_9_irqack;
			break;
		default:
			sc->sc_wdcdev.irqack = pciide_irqack;
		}
	}

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.set_modes = cmd0643_9_setup_channel;

	WDCDEBUG_PRINT(("cmd0643_9_chip_map: old timings reg 0x%x 0x%x\n",
		pci_conf_read(sc->sc_pc, sc->sc_tag, 0x54),
		pci_conf_read(sc->sc_pc, sc->sc_tag, 0x58)),
		DEBUG_PROBE);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		cmd_channel_map(pa, sc, channel);
		if (cp->hw_ok == 0)
			continue;
		cmd0643_9_setup_channel(&cp->wdc_channel);
	}
	/*
	 * note - this also makes sure we clear the irq disable and reset
	 * bits
	 */
	pciide_pci_write(sc->sc_pc, sc->sc_tag, CMD_DMA_MODE, CMD_DMA_MULTIPLE);
	WDCDEBUG_PRINT(("cmd0643_9_chip_map: timings reg now 0x%x 0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, 0x54),
	    pci_conf_read(sc->sc_pc, sc->sc_tag, 0x58)),
	    DEBUG_PROBE);
}

void
cmd0643_9_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	u_int8_t tim;
	u_int32_t idedma_ctl, udma_reg;
	int drive;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	idedma_ctl = 0;
	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		tim = cmd0643_9_data_tim_pio[drvp->PIO_mode];
		if (drvp->drive_flags & (DRIVE_DMA | DRIVE_UDMA)) {
			if (drvp->drive_flags & DRIVE_UDMA) {
				/* UltraDMA on a 646U2, 0648 or 0649 */
				drvp->drive_flags &= ~DRIVE_DMA;
				udma_reg = pciide_pci_read(sc->sc_pc,
				    sc->sc_tag, CMD_UDMATIM(chp->channel));
				if (drvp->UDMA_mode > 2 &&
				    (pciide_pci_read(sc->sc_pc, sc->sc_tag,
				    CMD_BICSR) &
				    CMD_BICSR_80(chp->channel)) == 0)
					drvp->UDMA_mode = 2;
				if (drvp->UDMA_mode > 2)
					udma_reg &= ~CMD_UDMATIM_UDMA33(drive);
				else if (sc->sc_wdcdev.UDMA_cap > 2) 
					udma_reg |= CMD_UDMATIM_UDMA33(drive);
				udma_reg |= CMD_UDMATIM_UDMA(drive);
				udma_reg &= ~(CMD_UDMATIM_TIM_MASK <<
				    CMD_UDMATIM_TIM_OFF(drive));
				udma_reg |=
				    (cmd0646_9_tim_udma[drvp->UDMA_mode] <<
				    CMD_UDMATIM_TIM_OFF(drive));
				pciide_pci_write(sc->sc_pc, sc->sc_tag,
				    CMD_UDMATIM(chp->channel), udma_reg);
			} else {
				/*
				 * use Multiword DMA.
				 * Timings will be used for both PIO and DMA,
				 * so adjust DMA mode if needed
				 * if we have a 0646U2/8/9, turn off UDMA
				 */
				if (sc->sc_wdcdev.cap & WDC_CAPABILITY_UDMA) {
					udma_reg = pciide_pci_read(sc->sc_pc,
					    sc->sc_tag,
					    CMD_UDMATIM(chp->channel));
					udma_reg &= ~CMD_UDMATIM_UDMA(drive);
					pciide_pci_write(sc->sc_pc, sc->sc_tag,
					    CMD_UDMATIM(chp->channel),
					    udma_reg);
				}
				if (drvp->PIO_mode >= 3 &&
				    (drvp->DMA_mode + 2) > drvp->PIO_mode) {
					drvp->DMA_mode = drvp->PIO_mode - 2;
				}
				tim = cmd0643_9_data_tim_dma[drvp->DMA_mode];
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
		pciide_pci_write(sc->sc_pc, sc->sc_tag,
		    CMD_DATA_TIM(chp->channel, drive), tim);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL + (IDEDMA_SCH_OFFSET * chp->channel),
		    idedma_ctl);
	}
	pciide_print_modes(cp);
}

void
cmd646_9_irqack(chp)
	struct channel_softc *chp;
{
	u_int32_t priirq, secirq;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	if (chp->channel == 0) {
		priirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_CONF);
		pciide_pci_write(sc->sc_pc, sc->sc_tag, CMD_CONF, priirq);
	} else {
		secirq = pciide_pci_read(sc->sc_pc, sc->sc_tag, CMD_ARTTIM23);
		pciide_pci_write(sc->sc_pc, sc->sc_tag, CMD_ARTTIM23, secirq);
	}
	pciide_irqack(chp);
}

void
cy693_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{	 
	struct pciide_channel *cp;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;
	/*
	 * this chip has 2 PCI IDE functions, one for primary and one for
	 * secondary. So we need to call pciide_mapregs_compat() with
	 * the real channel
	 */
	if (pa->pa_function == 1) {
		sc->sc_cy_compatchan = 0;
	} else if (pa->pa_function == 2) {
		sc->sc_cy_compatchan = 1;
	} else {
		printf("%s: unexpected PCI function %d\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, pa->pa_function);
		return;
	}
	if (interface & PCIIDE_INTERFACE_BUS_MASTER_DMA) {
		printf("%s: bus-master DMA support present",
		    sc->sc_wdcdev.sc_dev.dv_xname);
		pciide_mapreg_dma(sc, pa);
	} else {
		printf("%s: hardware does not support DMA",
		    sc->sc_wdcdev.sc_dev.dv_xname);
		sc->sc_dma_ok = 0;
	}
	printf("\n");

	sc->sc_cy_handle = cy82c693_init(pa->pa_iot);
	if (sc->sc_cy_handle == NULL) {
		printf("%s: unable to map hyperCache control registers\n",
		    sc->sc_wdcdev.sc_dev.dv_xname);
		sc->sc_dma_ok = 0;
	}

	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.set_modes = cy693_setup_channel;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = 1;

	/* Only one channel for this chip; if we are here it's enabled */
	cp = &sc->pciide_channels[0];
	sc->wdc_chanarray[0] = &cp->wdc_channel;
	cp->name = PCIIDE_CHANNEL_NAME(0);
	cp->wdc_channel.channel = 0;
	cp->wdc_channel.wdc = &sc->sc_wdcdev;
	cp->wdc_channel.ch_queue =
	    malloc(sizeof(struct channel_queue), M_DEVBUF, M_NOWAIT);
	if (cp->wdc_channel.ch_queue == NULL) {
		printf("%s primary channel: "
		    "can't allocate memory for command queue",
		sc->sc_wdcdev.sc_dev.dv_xname);
		return;
	}
	printf("%s: primary channel %s to ",
	    sc->sc_wdcdev.sc_dev.dv_xname, 
	    (interface & PCIIDE_INTERFACE_SETTABLE(0)) ?
	    "configured" : "wired");
	if (interface & PCIIDE_INTERFACE_PCI(0)) {
		printf("native-PCI");
		cp->hw_ok = pciide_mapregs_native(pa, cp, &cmdsize, &ctlsize,
		    pciide_pci_intr);
	} else {
		printf("compatibility");
		cp->hw_ok = pciide_mapregs_compat(pa, cp, sc->sc_cy_compatchan,
		    &cmdsize, &ctlsize);
	}
	printf(" mode\n");
	cp->wdc_channel.data32iot = cp->wdc_channel.cmd_iot;
	cp->wdc_channel.data32ioh = cp->wdc_channel.cmd_ioh;
	wdcattach(&cp->wdc_channel);
	if (pciide_chan_candisable(cp)) {
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    PCI_COMMAND_STATUS_REG, 0);
	}
	pciide_map_compat_intr(pa, cp, sc->sc_cy_compatchan, interface);
	if (cp->hw_ok == 0)
		return;
	WDCDEBUG_PRINT(("cy693_chip_map: old timings reg 0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, CY_CMD_CTRL)),DEBUG_PROBE);
	cy693_setup_channel(&cp->wdc_channel);
	WDCDEBUG_PRINT(("cy693_chip_map: new timings reg 0x%x\n",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, CY_CMD_CTRL)), DEBUG_PROBE);
}

void
cy693_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t cy_cmd_ctrl;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int dma_mode = -1;

	cy_cmd_ctrl = idedma_ctl = 0;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		if (drvp->drive_flags & DRIVE_DMA) {
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
			/* use Multiword DMA */
			if (dma_mode == -1 || dma_mode > drvp->DMA_mode)
				dma_mode = drvp->DMA_mode;
		}
		cy_cmd_ctrl |= (cy_pio_pulse[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOW_PULSE_OFF(drive));
		cy_cmd_ctrl |= (cy_pio_rec[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOW_REC_OFF(drive));
		cy_cmd_ctrl |= (cy_pio_pulse[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOR_PULSE_OFF(drive));
		cy_cmd_ctrl |= (cy_pio_rec[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOR_REC_OFF(drive));
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, CY_CMD_CTRL, cy_cmd_ctrl);
	chp->ch_drive[0].DMA_mode = dma_mode;
	chp->ch_drive[1].DMA_mode = dma_mode;

	if (dma_mode == -1)
		dma_mode = 0;

	if (sc->sc_cy_handle != NULL) {
		/* Note: `multiple' is implied. */
		cy82c693_write(sc->sc_cy_handle,
		    (sc->sc_cy_compatchan == 0) ?
		    CY_DMA_IDX_PRIMARY : CY_DMA_IDX_SECONDARY, dma_mode);
	}

	pciide_print_modes(cp);

	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL, idedma_ctl);
	}
}

void
sis_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int channel;
	u_int8_t sis_ctr0 = pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_CTRL0);
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);
	pcireg_t rev = PCI_REVISION(pa->pa_class);
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;
	printf("%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
		if (rev > 0xd0)
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
	}

	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	if (sc->sc_wdcdev.cap & WDC_CAPABILITY_UDMA)
		sc->sc_wdcdev.UDMA_cap = 2;
	sc->sc_wdcdev.set_modes = sis_setup_channel;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_MISC,
	    pciide_pci_read(sc->sc_pc, sc->sc_tag, SIS_MISC) |
	    SIS_MISC_TIM_SEL | SIS_MISC_FIFO_SIZE);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if ((channel == 0 && (sis_ctr0 & SIS_CTRL0_CHAN0_EN) == 0) ||
		    (channel == 1 && (sis_ctr0 & SIS_CTRL0_CHAN1_EN) == 0)) {
			printf("%s: %s channel ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);
		if (cp->hw_ok == 0)
			continue;
		if (pciide_chan_candisable(cp)) {
			if (channel == 0)
				sis_ctr0 &= ~SIS_CTRL0_CHAN0_EN;
			else
				sis_ctr0 &= ~SIS_CTRL0_CHAN1_EN;
			pciide_pci_write(sc->sc_pc, sc->sc_tag, SIS_CTRL0,
			    sis_ctr0);
		}
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;
		sis_setup_channel(&cp->wdc_channel);
	}
}

void
sis_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t sis_tim;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	WDCDEBUG_PRINT(("sis_setup_channel: old timings reg for "
	    "channel %d 0x%x\n", chp->channel, 
	    pci_conf_read(sc->sc_pc, sc->sc_tag, SIS_TIM(chp->channel))),
	    DEBUG_PROBE);
	sis_tim = 0;
	idedma_ctl = 0;
	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		if ((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0)
			goto pio;

		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			sis_tim |= sis_udma_tim[drvp->UDMA_mode] << 
			    SIS_TIM_UDMA_TIME_OFF(drive);
			sis_tim |= SIS_TIM_UDMA_EN(drive);
		} else {
			/*
			 * use Multiword DMA
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed
			 */
			if (drvp->PIO_mode > (drvp->DMA_mode + 2))
				drvp->PIO_mode = drvp->DMA_mode + 2;
			if (drvp->DMA_mode + 2 > (drvp->PIO_mode))
				drvp->DMA_mode = (drvp->PIO_mode > 2) ?
				    drvp->PIO_mode - 2 : 0;
			if (drvp->DMA_mode == 0)
				drvp->PIO_mode = 0;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
pio:		sis_tim |= sis_pio_act[drvp->PIO_mode] <<
		    SIS_TIM_ACT_OFF(drive);
		sis_tim |= sis_pio_rec[drvp->PIO_mode] <<
		    SIS_TIM_REC_OFF(drive);
	}
	WDCDEBUG_PRINT(("sis_setup_channel: new timings reg for "
	    "channel %d 0x%x\n", chp->channel, sis_tim), DEBUG_PROBE);
	pci_conf_write(sc->sc_pc, sc->sc_tag, SIS_TIM(chp->channel), sis_tim);
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL, idedma_ctl);
	}
	pciide_print_modes(cp);
}

void
acer_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t cr, interface;
	bus_size_t cmdsize, ctlsize;
	pcireg_t rev = PCI_REVISION(pa->pa_class);

	if (pciide_chipen(sc, pa) == 0)
		return;
	printf("%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA;
		if (rev >= 0x20)
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	    
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	sc->sc_wdcdev.UDMA_cap = 2;
	sc->sc_wdcdev.set_modes = acer_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	pciide_pci_write(sc->sc_pc, sc->sc_tag, ACER_CDRC,
	    (pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_CDRC) |
		ACER_CDRC_DMA_EN) & ~ACER_CDRC_FIFO_DISABLE);

	/* Enable "microsoft register bits" R/W. */
	pciide_pci_write(sc->sc_pc, sc->sc_tag, ACER_CCAR3,
	    pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_CCAR3) | ACER_CCAR3_PI);
	pciide_pci_write(sc->sc_pc, sc->sc_tag, ACER_CCAR1,
	    pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_CCAR1) &
	    ~(ACER_CHANSTATUS_RO|PCIIDE_CHAN_RO(0)|PCIIDE_CHAN_RO(1)));
	pciide_pci_write(sc->sc_pc, sc->sc_tag, ACER_CCAR2,
	    pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_CCAR2) &
	    ~ACER_CHANSTATUSREGS_RO);
	cr = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_CLASS_REG);
	cr |= (PCIIDE_CHANSTATUS_EN << PCI_INTERFACE_SHIFT);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_CLASS_REG, cr);
	/* Don't use cr, re-read the real register content instead */
	interface = PCI_INTERFACE(pci_conf_read(sc->sc_pc, sc->sc_tag,
	    PCI_CLASS_REG));

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if ((interface & PCIIDE_CHAN_EN(channel)) == 0) {
			printf("%s: %s channel ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    acer_pci_intr);
		if (cp->hw_ok == 0)
			continue;
		if (pciide_chan_candisable(cp)) {
			cr &= ~(PCIIDE_CHAN_EN(channel) << PCI_INTERFACE_SHIFT);
			pci_conf_write(sc->sc_pc, sc->sc_tag,
			    PCI_CLASS_REG, cr);
		}
		pciide_map_compat_intr(pa, cp, channel, interface);
		acer_setup_channel(&cp->wdc_channel);
	}
}

void
acer_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t acer_fifo_udma;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	idedma_ctl = 0;
	acer_fifo_udma = pci_conf_read(sc->sc_pc, sc->sc_tag, ACER_FTH_UDMA);
	WDCDEBUG_PRINT(("acer_setup_channel: old fifo/udma reg 0x%x\n", 
	    acer_fifo_udma), DEBUG_PROBE);
	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		WDCDEBUG_PRINT(("acer_setup_channel: old timings reg for "
		    "channel %d drive %d 0x%x\n", chp->channel, drive,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag,
		    ACER_IDETIM(chp->channel, drive))), DEBUG_PROBE);
		/* clear FIFO/DMA mode */
		acer_fifo_udma &= ~(ACER_FTH_OPL(chp->channel, drive, 0x3) |
		    ACER_UDMA_EN(chp->channel, drive) |
		    ACER_UDMA_TIM(chp->channel, drive, 0x7));

		/* add timing values, setup DMA if needed */
		if ((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0) {
			acer_fifo_udma |=
			    ACER_FTH_OPL(chp->channel, drive, 0x1);
			goto pio;
		}

		acer_fifo_udma |= ACER_FTH_OPL(chp->channel, drive, 0x2);
		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			acer_fifo_udma |= ACER_UDMA_EN(chp->channel, drive);
			acer_fifo_udma |= 
			    ACER_UDMA_TIM(chp->channel, drive,
				acer_udma[drvp->UDMA_mode]);
		} else {
			/*
			 * use Multiword DMA
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed
			 */
			if (drvp->PIO_mode > (drvp->DMA_mode + 2))
				drvp->PIO_mode = drvp->DMA_mode + 2;
			if (drvp->DMA_mode + 2 > (drvp->PIO_mode))
				drvp->DMA_mode = (drvp->PIO_mode > 2) ?
				    drvp->PIO_mode - 2 : 0;
			if (drvp->DMA_mode == 0)
				drvp->PIO_mode = 0;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
pio:		pciide_pci_write(sc->sc_pc, sc->sc_tag,
		    ACER_IDETIM(chp->channel, drive),
		    acer_pio[drvp->PIO_mode]);
	}
	WDCDEBUG_PRINT(("acer_setup_channel: new fifo/udma reg 0x%x\n",
	    acer_fifo_udma), DEBUG_PROBE);
	pci_conf_write(sc->sc_pc, sc->sc_tag, ACER_FTH_UDMA, acer_fifo_udma);
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL, idedma_ctl);
	}
	pciide_print_modes(cp);
}

int
acer_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv; 
	u_int32_t chids;

	rv = 0;
	chids = pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_CHIDS);
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		if (chids & ACER_CHIDS_INT(i)) {
			crv = wdcintr(wdc_cp);
			if (crv == 0)
				printf("%s:%d: bogus intr\n",
				    sc->sc_wdcdev.sc_dev.dv_xname, i);
			else
				rv = 1;
		}
	}
	return rv;
}

void
hpt_chip_map(sc, pa)
        struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int i, compatchan, revision;
	pcireg_t interface;
	bus_size_t cmdsize, ctlsize;

	if (pciide_chipen(sc, pa) == 0)
		return;
	revision = PCI_REVISION(pa->pa_class);
	printf(": Triones/Highpoint ");
	if (revision == HPT370_REV)
		printf("HPT370 IDE Controller\n");
	else
		printf("HPT366 IDE Controller\n");

	/* 
	 * when the chip is in native mode it identifies itself as a
	 * 'misc mass storage'. Fake interface in this case.
	 */
	if (PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		interface = PCI_INTERFACE(pa->pa_class);
	} else {
		interface = PCIIDE_INTERFACE_BUS_MASTER_DMA |
		    PCIIDE_INTERFACE_PCI(0);
		if (revision == HPT370_REV)
			interface |= PCIIDE_INTERFACE_PCI(1);
	}

	printf("%s: bus-master DMA support present",
		sc->sc_wdcdev.sc_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;

	sc->sc_wdcdev.set_modes = hpt_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	if (revision == HPT366_REV) {
		sc->sc_wdcdev.UDMA_cap = 4;
		/*
		 * The 366 has 2 PCI IDE functions, one for primary and one
		 * for secondary. So we need to call pciide_mapregs_compat()
		 * with the real channel
		 */
		if (pa->pa_function == 0) {
			compatchan = 0;
		} else if (pa->pa_function == 1) {
			compatchan = 1;
		} else {
			printf("%s: unexpected PCI function %d\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, pa->pa_function);
			return;
		}
		sc->sc_wdcdev.nchannels = 1;
	} else {
		sc->sc_wdcdev.nchannels = 2;
		sc->sc_wdcdev.UDMA_cap = 5;
	}
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		if (sc->sc_wdcdev.nchannels > 1) {
			compatchan = i;
			if((pciide_pci_read(sc->sc_pc, sc->sc_tag,
			   HPT370_CTRL1(i)) & HPT370_CTRL1_EN) == 0) {
				printf("%s: %s channel ignored (disabled)\n",
				    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
				continue;
			}
		}
		if (pciide_chansetup(sc, i, interface) == 0)
			continue;
		if (interface & PCIIDE_INTERFACE_PCI(i)) {
			cp->hw_ok = pciide_mapregs_native(pa, cp, &cmdsize,
			    &ctlsize, hpt_pci_intr);
		} else {
			cp->hw_ok = pciide_mapregs_compat(pa, cp, compatchan,
			    &cmdsize, &ctlsize);
		}
		if (cp->hw_ok == 0)
			return;
		cp->wdc_channel.data32iot = cp->wdc_channel.cmd_iot;
		cp->wdc_channel.data32ioh = cp->wdc_channel.cmd_ioh;
		wdcattach(&cp->wdc_channel);
		hpt_setup_channel(&cp->wdc_channel);
	}
	if (revision == HPT370_REV) {
		/*
		 * HPT370_REV has a bit to disable interrupts, make sure
		 * to clear it
		 */
		pciide_pci_write(sc->sc_pc, sc->sc_tag, HPT_CSEL,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, HPT_CSEL) &
		    ~HPT_CSEL_IRQDIS);
	}
	return;
}

void
hpt_setup_channel(chp)
	struct channel_softc *chp;
{
        struct ata_drive_datas *drvp;
	int drive;
	int cable;
	u_int32_t before, after;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;

	cable = pciide_pci_read(sc->sc_pc, sc->sc_tag, HPT_CSEL);

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;

	/* Per drive settings */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		before = pci_conf_read(sc->sc_pc, sc->sc_tag,
					HPT_IDETIM(chp->channel, drive));

                /* add timing values, setup DMA if needed */
                if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			if ((cable & HPT_CSEL_CBLID(chp->channel)) != 0 &&
			    drvp->UDMA_mode > 2)
				drvp->UDMA_mode = 2;
                        after = (sc->sc_wdcdev.nchannels == 2) ?
			    hpt370_udma[drvp->UDMA_mode] :
			    hpt366_udma[drvp->UDMA_mode];
                        idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
                } else if (drvp->drive_flags & DRIVE_DMA) {
                        /*
                         * use Multiword DMA.
                         * Timings will be used for both PIO and DMA, so adjust
                         * DMA mode if needed
                         */
                        if (drvp->PIO_mode >= 3 &&
                            (drvp->DMA_mode + 2) > drvp->PIO_mode) {
                                drvp->DMA_mode = drvp->PIO_mode - 2;
                        }
                        after = (sc->sc_wdcdev.nchannels == 2) ?
			    hpt370_dma[drvp->DMA_mode] :
			    hpt366_dma[drvp->DMA_mode];
                        idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
                } else {
			/* PIO only */
                	after = (sc->sc_wdcdev.nchannels == 2) ?
			    hpt370_pio[drvp->PIO_mode] :
			    hpt366_pio[drvp->PIO_mode];
		}
		pci_conf_write(sc->sc_pc, sc->sc_tag,
                    HPT_IDETIM(chp->channel, drive), after);
		WDCDEBUG_PRINT(("%s: bus speed register set to 0x%08x "
		    "(BIOS 0x%08x)\n", drvp->drv_softc->dv_xname,
		    after, before), DEBUG_PROBE);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL, idedma_ctl);
	}
	pciide_print_modes(cp);
}

int
hpt_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int rv = 0;
	int dmastat, i, crv;

	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		dmastat = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL + IDEDMA_SCH_OFFSET * i);
		if((dmastat & IDEDMA_CTL_INTR) == 0)
			continue;
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		crv = wdcintr(wdc_cp);
		if (crv == 0) {
			printf("%s:%d: bogus intr\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, i);
			bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_CTL + IDEDMA_SCH_OFFSET * i, dmastat);
		} else
			rv = 1;
	}
	return rv;
}


/* Macros to test product */
#define PDC_IS_262(sc)							\
	((sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_ULTRA66 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_ULTRA100 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_ULTRA100X)
#define PDC_IS_265(sc)							\
	((sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_ULTRA100 ||	\
	(sc)->sc_pp->ide_product == PCI_PRODUCT_PROMISE_ULTRA100X)

void
pdc202xx_chip_map(sc, pa)
        struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t interface, st, mode;
	bus_size_t cmdsize, ctlsize;

	st = pci_conf_read(sc->sc_pc, sc->sc_tag, PDC2xx_STATE);
	WDCDEBUG_PRINT(("pdc202xx_setup_chip: controller state 0x%x\n", st),
	    DEBUG_PROBE);
	if (pciide_chipen(sc, pa) == 0)
		return;

	/* turn off  RAID mode */
	st &= ~PDC2xx_STATE_IDERAID;

	/*
	 * can't rely on the PCI_CLASS_REG content if the chip was in raid
	 * mode. We have to fake interface
	 */
	interface = PCIIDE_INTERFACE_SETTABLE(0) | PCIIDE_INTERFACE_SETTABLE(1);
	if (st & PDC2xx_STATE_NATIVE)
		interface |= PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1);

	printf("%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	printf("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
	if (PDC_IS_265(sc))
		sc->sc_wdcdev.UDMA_cap = 5;
	else if (PDC_IS_262(sc))
		sc->sc_wdcdev.UDMA_cap = 4;
	else
		sc->sc_wdcdev.UDMA_cap = 2;
	sc->sc_wdcdev.set_modes = pdc202xx_setup_channel;
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	/* setup failsafe defaults */
	mode = 0;
	mode = PDC2xx_TIM_SET_PA(mode, pdc2xx_pa[0]);
	mode = PDC2xx_TIM_SET_PB(mode, pdc2xx_pb[0]);
	mode = PDC2xx_TIM_SET_MB(mode, pdc2xx_dma_mb[0]);
	mode = PDC2xx_TIM_SET_MC(mode, pdc2xx_dma_mc[0]);
	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		WDCDEBUG_PRINT(("pdc202xx_setup_chip: channel %d drive 0 "
		    "initial timings  0x%x, now 0x%x\n", channel,
		    pci_conf_read(sc->sc_pc, sc->sc_tag,
		    PDC2xx_TIM(channel, 0)), mode | PDC2xx_TIM_IORDYp),
		    DEBUG_PROBE);
		pci_conf_write(sc->sc_pc, sc->sc_tag, PDC2xx_TIM(channel, 0),
		    mode | PDC2xx_TIM_IORDYp);
		WDCDEBUG_PRINT(("pdc202xx_setup_chip: channel %d drive 1 "
		    "initial timings  0x%x, now 0x%x\n", channel,
		    pci_conf_read(sc->sc_pc, sc->sc_tag,
		    PDC2xx_TIM(channel, 1)), mode), DEBUG_PROBE);
		pci_conf_write(sc->sc_pc, sc->sc_tag, PDC2xx_TIM(channel, 1),
		    mode);
	}

	mode = PDC2xx_SCR_DMA;
	if (PDC_IS_262(sc)) {
		mode = PDC2xx_SCR_SET_GEN(mode, PDC262_SCR_GEN_LAT);
	} else {
		/* the BIOS set it up this way */
		mode = PDC2xx_SCR_SET_GEN(mode, 0x1);
	}
	mode = PDC2xx_SCR_SET_I2C(mode, 0x3); /* ditto */
	mode = PDC2xx_SCR_SET_POLL(mode, 0x1); /* ditto */
	WDCDEBUG_PRINT(("pdc202xx_setup_chip: initial SCR  0x%x, now 0x%x\n",
	    bus_space_read_4(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_SCR), mode),
	    DEBUG_PROBE);
	bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_SCR, mode);

	/* controller initial state register is OK even without BIOS */
	/* Set DMA mode to IDE DMA compatibility */
	mode = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_PM);
	WDCDEBUG_PRINT(("pdc202xx_setup_chip: primary mode 0x%x", mode ),
	    DEBUG_PROBE);
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_PM,
	    mode | 0x1);
	mode = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_SM);
	WDCDEBUG_PRINT((", secondary mode 0x%x\n", mode ), DEBUG_PROBE);
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_SM,
	    mode | 0x1);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if ((st & (PDC_IS_262(sc) ?
		    PDC262_STATE_EN(channel):PDC246_STATE_EN(channel))) == 0) {
			printf("%s: %s channel ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		if (PDC_IS_265(sc))
			pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
			    pdc20265_pci_intr);
		else
			pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
			    pdc202xx_pci_intr);
		if (cp->hw_ok == 0)
			continue;
		if (pciide_chan_candisable(cp))
			st &= ~(PDC_IS_262(sc) ?
			    PDC262_STATE_EN(channel):PDC246_STATE_EN(channel));
		pciide_map_compat_intr(pa, cp, channel, interface);
		pdc202xx_setup_channel(&cp->wdc_channel);
	}
	WDCDEBUG_PRINT(("pdc202xx_setup_chip: new controller state 0x%x\n", st),
	    DEBUG_PROBE);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PDC2xx_STATE, st);
	return;
}

void
pdc202xx_setup_channel(chp)
	struct channel_softc *chp;
{
        struct ata_drive_datas *drvp;
	int drive;
	pcireg_t mode, st;
	u_int32_t idedma_ctl, scr, atapi;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int channel = chp->channel;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	idedma_ctl = 0;
	WDCDEBUG_PRINT(("pdc202xx_setup_channel %s: scr 0x%x\n",
	    sc->sc_wdcdev.sc_dev.dv_xname,
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh, PDC262_U66)),
	    DEBUG_PROBE);

	/* Per channel settings */
	if (PDC_IS_262(sc)) {
		scr = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_U66);
		st = pci_conf_read(sc->sc_pc, sc->sc_tag, PDC2xx_STATE);
		/* Trimm UDMA mode */
		if ((st & PDC262_STATE_80P(channel)) != 0 ||
		    (chp->ch_drive[0].drive_flags & DRIVE_UDMA &&
		    chp->ch_drive[0].UDMA_mode <= 2) ||
		    (chp->ch_drive[1].drive_flags & DRIVE_UDMA &&
		    chp->ch_drive[1].UDMA_mode <= 2)) {
			if (chp->ch_drive[0].UDMA_mode > 2)
				chp->ch_drive[0].UDMA_mode = 2;
			if (chp->ch_drive[1].UDMA_mode > 2)
				chp->ch_drive[1].UDMA_mode = 2;
		}
		/* Set U66 if needed */
		if ((chp->ch_drive[0].drive_flags & DRIVE_UDMA &&
		    chp->ch_drive[0].UDMA_mode > 2) ||
		    (chp->ch_drive[1].drive_flags & DRIVE_UDMA &&
		    chp->ch_drive[1].UDMA_mode > 2))
			scr |= PDC262_U66_EN(channel);
		else
			scr &= ~PDC262_U66_EN(channel);
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_U66, scr);
		WDCDEBUG_PRINT(("pdc202xx_setup_channel %s:%d: ATAPI 0x%x\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, channel,
		    bus_space_read_4(sc->sc_dma_iot, sc->sc_dma_ioh,
		    PDC262_ATAPI(channel))), DEBUG_PROBE);
		if (chp->ch_drive[0].drive_flags & DRIVE_ATAPI ||
			chp->ch_drive[1].drive_flags & DRIVE_ATAPI) {
			if (((chp->ch_drive[0].drive_flags & DRIVE_UDMA) &&
			    !(chp->ch_drive[1].drive_flags & DRIVE_UDMA) &&
			    (chp->ch_drive[1].drive_flags & DRIVE_DMA)) ||
			    ((chp->ch_drive[1].drive_flags & DRIVE_UDMA) &&
			    !(chp->ch_drive[0].drive_flags & DRIVE_UDMA) &&
			    (chp->ch_drive[0].drive_flags & DRIVE_DMA)))
				atapi = 0;
			else
				atapi = PDC262_ATAPI_UDMA;
			bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
			    PDC262_ATAPI(channel), atapi);
		}
	}
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		mode = 0;
		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			mode = PDC2xx_TIM_SET_MB(mode,
			    pdc2xx_udma_mb[drvp->UDMA_mode]);
			mode = PDC2xx_TIM_SET_MC(mode,
			    pdc2xx_udma_mc[drvp->UDMA_mode]);
			drvp->drive_flags &= ~DRIVE_DMA;
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else if (drvp->drive_flags & DRIVE_DMA) {
			mode = PDC2xx_TIM_SET_MB(mode,
			    pdc2xx_dma_mb[drvp->DMA_mode]);
			mode = PDC2xx_TIM_SET_MC(mode,
			    pdc2xx_dma_mc[drvp->DMA_mode]);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		} else {
			mode = PDC2xx_TIM_SET_MB(mode,
			    pdc2xx_dma_mb[0]);
			mode = PDC2xx_TIM_SET_MC(mode,
			    pdc2xx_dma_mc[0]);
		}
		mode = PDC2xx_TIM_SET_PA(mode, pdc2xx_pa[drvp->PIO_mode]);
		mode = PDC2xx_TIM_SET_PB(mode, pdc2xx_pb[drvp->PIO_mode]);
		if (drvp->drive_flags & DRIVE_ATA)
			mode |= PDC2xx_TIM_PRE;
		mode |= PDC2xx_TIM_SYNC | PDC2xx_TIM_ERRDY;
		if (drvp->PIO_mode >= 3) {
			mode |= PDC2xx_TIM_IORDY;
			if (drive == 0)
				mode |= PDC2xx_TIM_IORDYp;
		}
		WDCDEBUG_PRINT(("pdc202xx_setup_channel: %s:%d:%d "
		    "timings 0x%x\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, 
		    chp->channel, drive, mode), DEBUG_PROBE);
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    PDC2xx_TIM(chp->channel, drive), mode);
	}
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL, idedma_ctl);
	}
	pciide_print_modes(cp);
}

int
pdc202xx_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv; 
	u_int32_t scr;

	rv = 0;
	scr = bus_space_read_4(sc->sc_dma_iot, sc->sc_dma_ioh, PDC2xx_SCR);
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		if (scr & PDC2xx_SCR_INT(i)) {
			crv = wdcintr(wdc_cp);
			if (crv == 0)
				printf("%s:%d: bogus intr (reg 0x%x)\n",
				    sc->sc_wdcdev.sc_dev.dv_xname, i, scr);
			else
				rv = 1;
		}
	}
	return rv;
}

int
pdc20265_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv; 
	u_int32_t dmastat;

	rv = 0;
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;
		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		/*
		 * The Ultra/100 seems to assert PDC2xx_SCR_INT * spuriously,
		 * however it asserts INT in IDEDMA_CTL even for non-DMA ops.
		 * So use it instead (requires 2 reg reads instead of 1,
		 * but we can't do it another way).
		 */
		dmastat = bus_space_read_1(sc->sc_dma_iot,
		    sc->sc_dma_ioh, IDEDMA_CTL + IDEDMA_SCH_OFFSET * i);
		if((dmastat & IDEDMA_CTL_INTR) == 0)
			continue;
		crv = wdcintr(wdc_cp);
		if (crv == 0)
			printf("%s:%d: bogus intr\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, i);
		else
			rv = 1;
	}
	return rv;
}

void
opti_chip_map(sc, pa)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	struct pciide_channel *cp;
	bus_size_t cmdsize, ctlsize;
	pcireg_t interface;
	u_int8_t init_ctrl;
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;
	printf("%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	printf("\n");

	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	sc->sc_wdcdev.PIO_cap = 4;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA | WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.DMA_cap = 2;
	}
	sc->sc_wdcdev.set_modes = opti_setup_channel;

	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = PCIIDE_NUM_CHANNELS;

	init_ctrl = pciide_pci_read(sc->sc_pc, sc->sc_tag,
	    OPTI_REG_INIT_CONTROL);

	interface = PCI_INTERFACE(pa->pa_class);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if (channel == 1 &&
		    (init_ctrl & OPTI_INIT_CONTROL_CH2_DISABLE) != 0) {
			printf("%s: %s channel ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);
		if (cp->hw_ok == 0)
			continue;
		pciide_map_compat_intr(pa, cp, channel, interface);
		if (cp->hw_ok == 0)
			continue;
		opti_setup_channel(&cp->wdc_channel);
	}
}

void
opti_setup_channel(chp)
	struct channel_softc *chp;
{
	struct ata_drive_datas *drvp;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.wdc;
	int drive, spd;
	int mode[2];
	u_int8_t rv, mr;

	/*
	 * The `Delay' and `Address Setup Time' fields of the
	 * Miscellaneous Register are always zero initially.
	 */
	mr = opti_read_config(chp, OPTI_REG_MISC) & ~OPTI_MISC_INDEX_MASK;
	mr &= ~(OPTI_MISC_DELAY_MASK |
		OPTI_MISC_ADDR_SETUP_MASK |
		OPTI_MISC_INDEX_MASK);

	/* Prime the control register before setting timing values */
	opti_write_config(chp, OPTI_REG_CONTROL, OPTI_CONTROL_DISABLE);

	/* Determine the clockrate of the PCIbus the chip is attached to */
	spd = (int) opti_read_config(chp, OPTI_REG_STRAP);
	spd &= OPTI_STRAP_PCI_SPEED_MASK;

	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0) {
			mode[drive] = -1;
			continue;
		}

		if ((drvp->drive_flags & DRIVE_DMA)) {
			/*
			 * Timings will be used for both PIO and DMA,
			 * so adjust DMA mode if needed
			 */
			if (drvp->PIO_mode > (drvp->DMA_mode + 2))
				drvp->PIO_mode = drvp->DMA_mode + 2;
			if (drvp->DMA_mode + 2 > (drvp->PIO_mode))
				drvp->DMA_mode = (drvp->PIO_mode > 2) ?
				    drvp->PIO_mode - 2 : 0;
			if (drvp->DMA_mode == 0)
				drvp->PIO_mode = 0;

			mode[drive] = drvp->DMA_mode + 5;
		} else
			mode[drive] = drvp->PIO_mode;

		if (drive && mode[0] >= 0 &&
		    (opti_tim_as[spd][mode[0]] != opti_tim_as[spd][mode[1]])) {
			/*
			 * Can't have two drives using different values
			 * for `Address Setup Time'.
			 * Slow down the faster drive to compensate.
			 */
			int d = (opti_tim_as[spd][mode[0]] >
				 opti_tim_as[spd][mode[1]]) ?  0 : 1;

			mode[d] = mode[1-d];
			chp->ch_drive[d].PIO_mode = chp->ch_drive[1-d].PIO_mode;
			chp->ch_drive[d].DMA_mode = 0;
			chp->ch_drive[d].drive_flags &= DRIVE_DMA;
		}
	}

	for (drive = 0; drive < 2; drive++) {
		int m;
		if ((m = mode[drive]) < 0)
			continue;

		/* Set the Address Setup Time and select appropriate index */
		rv = opti_tim_as[spd][m] << OPTI_MISC_ADDR_SETUP_SHIFT;
		rv |= OPTI_MISC_INDEX(drive);
		opti_write_config(chp, OPTI_REG_MISC, mr | rv);

		/* Set the pulse width and recovery timing parameters */
		rv  = opti_tim_cp[spd][m] << OPTI_PULSE_WIDTH_SHIFT;
		rv |= opti_tim_rt[spd][m] << OPTI_RECOVERY_TIME_SHIFT;
		opti_write_config(chp, OPTI_REG_READ_CYCLE_TIMING, rv);
		opti_write_config(chp, OPTI_REG_WRITE_CYCLE_TIMING, rv);

		/* Set the Enhanced Mode register appropriately */
	    	rv = pciide_pci_read(sc->sc_pc, sc->sc_tag, OPTI_REG_ENH_MODE);
		rv &= ~OPTI_ENH_MODE_MASK(chp->channel, drive);
		rv |= OPTI_ENH_MODE(chp->channel, drive, opti_tim_em[m]);
		pciide_pci_write(sc->sc_pc, sc->sc_tag, OPTI_REG_ENH_MODE, rv);
	}

	/* Finally, enable the timings */
	opti_write_config(chp, OPTI_REG_CONTROL, OPTI_CONTROL_ENABLE);

	pciide_print_modes(cp);
}
