/*	$NetBSD: if_ex_pci.c,v 1.20.8.1 2002/07/15 10:35:37 gehenna Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden; Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ex_pci.c,v 1.20.8.1 2002/07/15 10:35:37 gehenna Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h> 
#include <sys/socket.h> 
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/mii/miivar.h>
#include <dev/mii/mii.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>
#include <dev/ic/elinkxlreg.h>
#include <dev/ic/elinkxlvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

struct ex_pci_softc {
	struct ex_softc sc_ex;

	/* PCI function status space. 556,556B requests it. */
	bus_space_tag_t sc_funct;
	bus_space_handle_t sc_funch;

	pci_chipset_tag_t psc_pc;	/* pci chipset tag */
	pcireg_t psc_regs[0x20>>2];	/* saved PCI config regs (sparse) */
	pcitag_t psc_tag;		/* pci device tag */

	int psc_pwrmgmt_csr_reg;	/* ACPI power management register */
	pcireg_t psc_pwrmgmt_csr;	/* ...and the contents at D0 */
};

/*
 * PCI constants.
 * XXX These should be in a common file!
 */
#define PCI_CONN		0x48    /* Connector type */
#define PCI_CBIO		0x10    /* Configuration Base IO Address */
#define PCI_POWERCTL		0xe0
#define PCI_FUNCMEM		0x18

#define PCI_INTR		4
#define PCI_INTRACK		0x00008000

int ex_pci_match __P((struct device *, struct cfdata *, void *));
void ex_pci_attach __P((struct device *, struct device *, void *));
void ex_pci_intr_ack __P((struct ex_softc *));

int ex_pci_enable __P((struct ex_softc *));
void ex_pci_disable __P((struct ex_softc *));

void ex_pci_confreg_restore __P((struct ex_pci_softc *));

struct cfattach ex_pci_ca = {
	sizeof(struct ex_pci_softc), ex_pci_match, ex_pci_attach
};

const struct ex_pci_product {
	u_int32_t	epp_prodid;	/* PCI product ID */
	int		epp_flags;	/* initial softc flags */
	const char	*epp_name;	/* device name */
} ex_pci_products[] = {
	{ PCI_PRODUCT_3COM_3C900TPO,	0,
	  "3c900-TPO Ethernet" },
	{ PCI_PRODUCT_3COM_3C900COMBO,	0,
	  "3c900-COMBO Ethernet" },

	{ PCI_PRODUCT_3COM_3C905TX,	EX_CONF_MII,
	  "3c905-TX 10/100 Ethernet" },
	{ PCI_PRODUCT_3COM_3C905T4,	EX_CONF_MII,
	  "3c905-T4 10/100 Ethernet" },

	{ PCI_PRODUCT_3COM_3C900BTPO,	EX_CONF_90XB,
	  "3c900B-TPO Ethernet" },
	{ PCI_PRODUCT_3COM_3C900BCOMBO,	EX_CONF_90XB,
	  "3c900B-COMBO Ethernet" },
	{ PCI_PRODUCT_3COM_3C900BTPC,   EX_CONF_90XB,
	  "3c900B-TPC Ethernet" },

	{ PCI_PRODUCT_3COM_3C905BTX,	EX_CONF_90XB|EX_CONF_MII|EX_CONF_INTPHY,
	  "3c905B-TX 10/100 Ethernet" },
	{ PCI_PRODUCT_3COM_3C905BT4,	EX_CONF_90XB|EX_CONF_MII,
	  "3c905B-T4 10/100 Ethernet" },
	{ PCI_PRODUCT_3COM_3C905BCOMBO,	EX_CONF_90XB/*|EX_CONF_MII|EX_CONF_INTPHY*/,
	  "3c905B-COMBO 10/100 Ethernet" },
	{ PCI_PRODUCT_3COM_3C905BFX,	EX_CONF_90XB,
	  "3c905B-FX 10/100 Ethernet" },

	/* XXX Internal PHY? */
	{ PCI_PRODUCT_3COM_3C980SRV,	EX_CONF_90XB,
	  "3c980 Server Adapter 10/100 Ethernet" },
	{ PCI_PRODUCT_3COM_3C980CTXM,	EX_CONF_90XB|EX_CONF_MII,
	  "3c980C-TXM 10/100 Ethernet" },

	{ PCI_PRODUCT_3COM_3C905CTX,	EX_CONF_90XB|EX_CONF_MII,
	  "3c905C-TX 10/100 Ethernet with mngmt" },

	{ PCI_PRODUCT_3COM_3C450TX,		EX_CONF_90XB,
	  "3c450-TX 10/100 Ethernet" },

	{ PCI_PRODUCT_3COM_3CSOHO100TX,	EX_CONF_90XB,
	  "3cSOHO100-TX 10/100 Ethernet" },

	{ PCI_PRODUCT_3COM_3C555,
	   EX_CONF_90XB | EX_CONF_MII | EX_CONF_EEPROM_OFF |
	   EX_CONF_EEPROM_8BIT,
	  "3c555 MiniPCI 10/100 Ethernet" },

	{ PCI_PRODUCT_3COM_3C556,
	   EX_CONF_90XB | EX_CONF_MII | EX_CONF_EEPROM_OFF |
	   EX_CONF_PCI_FUNCREG | EX_CONF_RESETHACK | EX_CONF_INV_LED_POLARITY |
	   EX_CONF_PHY_POWER | EX_CONF_EEPROM_8BIT,
	  "3c556 MiniPCI 10/100 Ethernet" },

	{ PCI_PRODUCT_3COM_3C556B,
	   EX_CONF_90XB | EX_CONF_MII | EX_CONF_EEPROM_OFF |
	   EX_CONF_PCI_FUNCREG | EX_CONF_RESETHACK | EX_CONF_INV_LED_POLARITY |
	   EX_CONF_PHY_POWER,
	  "3c556B MiniPCI 10/100 Ethernet" },

	{ 0,				0,
	  NULL },
};

const struct ex_pci_product *ex_pci_lookup
    __P((const struct pci_attach_args *));

const struct ex_pci_product *
ex_pci_lookup(pa)
	const struct pci_attach_args *pa;
{
	const struct ex_pci_product *epp;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_3COM)
		return (NULL);

	for (epp = ex_pci_products; epp->epp_name != NULL; epp++)
		if (PCI_PRODUCT(pa->pa_id) == epp->epp_prodid)
			return (epp);
	return (NULL);
}

int
ex_pci_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (ex_pci_lookup(pa) != NULL)
		return (2);	/* beat ep_pci */

	return (0);
}

void
ex_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ex_softc *sc = (void *)self;
	struct ex_pci_softc *psc = (void *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const struct ex_pci_product *epp;
	const char *intrstr = NULL;
	int rev, pmreg;
	pcireg_t reg;

	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, NULL)) {
		printf(": can't map i/o space\n");
		return;
	}

	epp = ex_pci_lookup(pa);
	if (epp == NULL) {
		printf("\n");
		panic("ex_pci_attach: impossible");
	}

	rev = PCI_REVISION(pci_conf_read(pc, pa->pa_tag, PCI_CLASS_REG));
	printf(": 3Com %s (rev. 0x%x)\n", epp->epp_name, rev);

	sc->sc_dmat = pa->pa_dmat;

	sc->ex_bustype = EX_BUS_PCI;
	sc->ex_conf = epp->epp_flags;

	/* Enable the card. */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
	    PCI_COMMAND_MASTER_ENABLE);

	psc->psc_pc = pc;
	psc->psc_tag = pa->pa_tag;
	psc->psc_regs[PCI_COMMAND_STATUS_REG>>2] = 
	    pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	psc->psc_regs[PCI_BHLC_REG>>2] =
	    pci_conf_read(pc, pa->pa_tag, PCI_BHLC_REG); 
	psc->psc_regs[PCI_CBIO>>2] =
	    pci_conf_read(pc, pa->pa_tag, PCI_CBIO);

	if (sc->ex_conf & EX_CONF_PCI_FUNCREG) {
		/* Map PCI function status window. */
		if (pci_mapreg_map(pa, PCI_FUNCMEM, PCI_MAPREG_TYPE_MEM, 0,
		    &psc->sc_funct, &psc->sc_funch, NULL, NULL)) {
			printf("%s: unable to map function status window\n",
			    sc->sc_dev.dv_xname);
			return;
		}
		sc->intr_ack = ex_pci_intr_ack;

		psc->psc_regs[PCI_FUNCMEM>>2] =
		    pci_conf_read(pc, pa->pa_tag, PCI_FUNCMEM);
	}

	psc->psc_regs[PCI_INTERRUPT_REG>>2] =
	    pci_conf_read(pc, pa->pa_tag, PCI_INTERRUPT_REG);

	/* Get it out of power save mode if needed (BIOS bugs) */
	if (pci_get_capability(pc, pa->pa_tag, PCI_CAP_PWRMGMT, &pmreg, 0)) {
		sc->enable = ex_pci_enable;
		sc->disable = ex_pci_disable;

		psc->psc_pwrmgmt_csr_reg = pmreg + 4;
		reg = pci_conf_read(pc, pa->pa_tag,
		    psc->psc_pwrmgmt_csr_reg) & 0x3;

		psc->psc_pwrmgmt_csr = (reg & ~PCI_PMCSR_STATE_MASK) |
		    PCI_PMCSR_STATE_D0;

		if (reg == PCI_PMCSR_STATE_D3) {
			/*
			 * The card has lost all configuration data in
			 * this state, so punt.
			 */
			printf("%s: unable to wake up from power state D3\n",
			    sc->sc_dev.dv_xname);
			return;
		}
		if (reg != PCI_PMCSR_STATE_D0) {
			printf("%s: waking up from power state D%d\n",
			    sc->sc_dev.dv_xname, reg);
			pci_conf_write(pc, pa->pa_tag, pmreg + 4, 0);
		}
	}

	sc->enabled = 1;

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}

	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, ex_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	ex_config(sc);

	if (sc->ex_conf & EX_CONF_PCI_FUNCREG)
		bus_space_write_4(psc->sc_funct, psc->sc_funch, PCI_INTR, 
		    PCI_INTRACK);

	if (sc->disable != NULL)
		ex_disable(sc);
}

void            
ex_pci_intr_ack(sc)
	struct ex_softc *sc;
{
	struct ex_pci_softc *psc = (struct ex_pci_softc *)sc;
        
	bus_space_write_4(psc->sc_funct, psc->sc_funch, PCI_INTR,
	    PCI_INTRACK);
}

void
ex_pci_confreg_restore(psc)
	struct ex_pci_softc *psc;
{
	struct ex_softc *sc = (void *) psc;
	pcireg_t reg;

	reg = pci_conf_read(psc->psc_pc, psc->psc_tag, PCI_COMMAND_STATUS_REG);

	pci_conf_write(psc->psc_pc, psc->psc_tag,
	    PCI_COMMAND_STATUS_REG,
	    (reg & 0xffff0000) |
	    (psc->psc_regs[PCI_COMMAND_STATUS_REG>>2] & 0xffff));
	pci_conf_write(psc->psc_pc, psc->psc_tag, PCI_BHLC_REG,
	    psc->psc_regs[PCI_BHLC_REG>>2]);
	pci_conf_write(psc->psc_pc, psc->psc_tag, PCI_CBIO,
	    psc->psc_regs[PCI_CBIO>>2]);
	if (sc->ex_conf & EX_CONF_PCI_FUNCREG)
		pci_conf_write(psc->psc_pc, psc->psc_tag, PCI_FUNCMEM,
		    psc->psc_regs[PCI_FUNCMEM>>2]);
	pci_conf_write(psc->psc_pc, psc->psc_tag, PCI_INTERRUPT_REG,
	    psc->psc_regs[PCI_INTERRUPT_REG>>2]);
}

int
ex_pci_enable(sc)
	struct ex_softc *sc;
{
	struct ex_pci_softc *psc = (void *) sc;

#if 0
	printf("%s: going to power state D0\n", sc->sc_dev.dv_xname);
#endif

	/* Bring the device into D0 power state. */
	pci_conf_write(psc->psc_pc, psc->psc_tag,
	    psc->psc_pwrmgmt_csr_reg, psc->psc_pwrmgmt_csr);

	/* Now restore the configuration registers. */
	ex_pci_confreg_restore(psc);

	return (0);
}

void
ex_pci_disable(sc)
	struct ex_softc *sc;
{
	struct ex_pci_softc *psc = (void *) sc;

#if 0
	printf("%s: going to power state D3\n", sc->sc_dev.dv_xname);
#endif

	/* Put the device into D3 state. */
	pci_conf_write(psc->psc_pc, psc->psc_tag,
	    psc->psc_pwrmgmt_csr_reg, (psc->psc_pwrmgmt_csr &
	    ~PCI_PMCSR_STATE_MASK) | PCI_PMCSR_STATE_D3);
}
