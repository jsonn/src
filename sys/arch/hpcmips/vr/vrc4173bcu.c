/*	$NetBSD: vrc4173bcu.c,v 1.1.4.1 2002/01/10 19:44:13 thorpej Exp $	*/

/*-
 * Copyright (c) 2001,2002 Enami Tsugutomo.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
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

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <machine/platid.h>
#include <machine/platid_mask.h>

#include <hpcmips/vr/vrc4173bcuvar.h>
#include <hpcmips/vr/vrc4173icureg.h>
#include <hpcmips/vr/vrc4173cmureg.h>

#define	VRC4173BCU_BADR		0x10
#ifdef DEBUG
#define	DPRINTF(args)	printf args
#else
#define	DPRINTF(args)
#endif

#define USE_WINCE_CLKMASK	(~0)

static int	vrc4173bcu_match(struct device *, struct cfdata *, void *);
static void	vrc4173bcu_attach(struct device *, struct device *, void *);
static int	vrc4173bcu_print(void *, const char *);

/*
 * machine dependent info
 */
static struct vrc4173bcu_platdep {
	platid_mask_t *platidmask;
	u_int32_t clkmask;
	int intr_port;	/* GPIO port to which VRCINT is connected to. XXX */
} platdep_table[] = {
	{
		&platid_mask_MACH_VICTOR_INTERLINK_MPC303,
		USE_WINCE_CLKMASK,	/* clock mask */
		1,			/* intrrupt port# */
	},
	{
		&platid_mask_MACH_VICTOR_INTERLINK_MPC304,
		USE_WINCE_CLKMASK,	/* clock mask */
		1,			/* intrrupt port# */
	},
	{
		&platid_mask_MACH_NEC_MCR_SIGMARION2,
		USE_WINCE_CLKMASK,	/* clock mask */
		0,			/* intrrupt port# */
	},
	{
		&platid_wild,
		USE_WINCE_CLKMASK,	/* XXX */
		-1,
	},
};

struct vrc4173bcu_softc {
	struct device sc_dev;

	pci_chipset_tag_t sc_pc;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_size_t sc_size;

	bus_space_handle_t sc_icuh;	/* I/O handle for ICU. */
	bus_space_handle_t sc_cmuh;	/* I/O handle for CMU. */
	void *sc_ih;

#define	VRC4173BCU_NINTRHAND	(16)	/* XXX */
	struct intrhand {
		int (*ih_func)(void *);
		void *ih_arg;
	} sc_intrhand[VRC4173BCU_NINTRHAND];
	int sc_intrmask;

	struct vrc4173bcu_platdep *sc_platdep;
};

struct cfattach vrc4173bcu_ca = {
	sizeof(struct vrc4173bcu_softc), vrc4173bcu_match, vrc4173bcu_attach,
};

int
vrc4173bcu_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NEC &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NEC_VRC4173_BCU)
		return (1);
 
	return (0);
}

void
vrc4173bcu_attach(struct device *parent, struct device *self, void *aux)
{
	struct vrc4173bcu_softc *sc = (struct vrc4173bcu_softc *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	pcireg_t csr;
	char devinfo[256];
	int i;
	u_int16_t reg;
#ifdef DEBUG
	char buf[80];
#endif

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(pa->pa_class));

#if 0
	printf("%s: ", sc->sc_dev.dv_xname);
	pci_conf_print(pa->pa_pc, pa->pa_tag, NULL);
#endif

	csr = pci_conf_read(pc, tag, VRC4173BCU_BADR);
	DPRINTF(("%s: base addr = 0x%08x\n", sc->sc_dev.dv_xname, csr));

	sc->sc_platdep = platid_search(&platid, platdep_table,
	    sizeof(platdep_table)/sizeof(*platdep_table),
	    sizeof(*platdep_table));

	/* Map I/O registers */
	if (pci_mapreg_map(pa, VRC4173BCU_BADR, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, &sc->sc_size)) {
		printf("%s: can't map mem space\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_pc = pc;

	/* Enable the device. */
	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	DPRINTF(("%s: csr = 0x%08x", sc->sc_dev.dv_xname, csr));
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_IO_ENABLE);
	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	DPRINTF((" -> 0x%08x\n", csr));

	csr = pci_conf_read(pc, tag, VRC4173BCU_BADR);
	DPRINTF(("%s: base addr = %x@0x%08x\n", sc->sc_dev.dv_xname,
	    (int)sc->sc_size, csr));
	DPRINTF(("%s: iot = 0x%08x, ioh = 0x%08x\n", sc->sc_dev.dv_xname,
	    (int)sc->sc_iot, (int)sc->sc_ioh));

	/*
	 * Map I/O space for ICU.
	 */
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh,
	    VRC4173ICU_IOBASE, VRC4173ICU_IOSIZE, &sc->sc_icuh)) {
		printf(": can't map ICU i/o space\n");
		return;
	}

	/*
	 * Map I/O space for CMU.
	 */
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh,
	    VRC4173CMU_IOBASE, VRC4173CMU_IOSIZE, &sc->sc_cmuh)) {
		printf(": can't map CMU i/o space\n");
		return;
	}

	/* machine dependent setup */
	if (sc->sc_platdep->clkmask == USE_WINCE_CLKMASK) {
		/* XXX, You can nothing! */
		reg = bus_space_read_2(sc->sc_iot, sc->sc_cmuh,
		    VRC4173CMU_CLKMSK);
		printf("%s: default clock mask is %04x\n",
		    sc->sc_dev.dv_xname, reg);
	} else {
		/* assert all reset bits */
		bus_space_write_2(sc->sc_iot, sc->sc_cmuh, VRC4173CMU_SRST,
		    VRC4173CMU_SRST_AC97 | VRC4173CMU_SRST_USB |
		    VRC4173CMU_SRST_CARD2 | VRC4173CMU_SRST_CARD1);
		/* set clock mask */
		bus_space_write_2(sc->sc_iot, sc->sc_cmuh,
		    VRC4173CMU_CLKMSK, sc->sc_platdep->clkmask);
		/* clear reset bit */
		bus_space_write_2(sc->sc_iot, sc->sc_cmuh, VRC4173CMU_SRST, 0);
	}

#ifdef DEBUG
	reg = bus_space_read_2(sc->sc_iot, sc->sc_icuh, VRC4173ICU_SYSINT1);
	bitmask_snprintf(reg,
	    "\20\1USB\2PCMCIA2\3PCMCIA1\4PS2CH2\5PS2CH1\6PIU\7AIU\10KIU"
	    "\11GIU\12AC97\13AC97-1\14B11\15B12\16DOZEPIU\17B14\20B15",
	    buf, sizeof(buf));
	printf("%s: SYSINT1 = 0x%s\n", sc->sc_dev.dv_xname, buf);

	reg = bus_space_read_2(sc->sc_iot, sc->sc_icuh, VRC4173ICU_MKIUINT);
	bitmask_snprintf(reg,
	    "\20\1SCANINT\2KDATRDY\3KDATLOST\4B3\5B4\6B5\7B6\10B7"
	    "\11B8\12B9\13B10\14B11\15B12\16B13\17B14\20B15",
	    buf, sizeof(buf));
	printf("%s: MKIUINT = 0x%s\n", sc->sc_dev.dv_xname, buf);

	reg = bus_space_read_2(sc->sc_iot, sc->sc_icuh, VRC4173ICU_MSYSINT1);
	bitmask_snprintf(reg,
	    "\20\1USB\2PCMCIA2\3PCMCIA1\4PS2CH2\5PS2CH1\6PIU\7AIU\10KIU"
	    "\11GIU\12AC97\13AC97-1\14B11\15B12\16DOZEPIU\17B14\20B15",
	    buf, sizeof(buf));
	printf("%s: MSYSINT1 = 0x%s\n", sc->sc_dev.dv_xname, buf);

#if 1
	reg = VRC4173ICU_USBINTR | VRC4173ICU_PIUINTR | VRC4173ICU_KIUINTR |
	    VRC4173ICU_DOZEPIUINTR;
	bus_space_write_2(sc->sc_iot, sc->sc_icuh, VRC4173ICU_MSYSINT1, reg);

	reg = bus_space_read_2(sc->sc_iot, sc->sc_icuh, VRC4173ICU_MSYSINT1);
	bitmask_snprintf(reg,
	    "\20\1USB\2PCMCIA2\3PCMCIA1\4PS2CH2\5PS2CH1\6PIU\7AIU\10KIU"
	    "\11GIU\12AC97\13AC97-1\14B11\15B12\16DOZEPIU\17B14\20B15",
	    buf, sizeof(buf));
	printf("%s: MSYSINT1 = 0x%s\n", sc->sc_dev.dv_xname, buf);
#endif
#endif

	for (i = 0; i < VRC4173BCU_NINTRHAND; i++)
		sc->sc_intrhand[i].ih_func = NULL;
	sc->sc_intrmask = 0;
	bus_space_write_2(sc->sc_iot, sc->sc_icuh, VRC4173ICU_MSYSINT1,
	    sc->sc_intrmask);

	/*
	 * Attach sub units found in vrc4173.  XXX.
	 */
	config_found(self, "vrc4173cmu", vrc4173bcu_print);
	config_found(self, "vrc4173giu", vrc4173bcu_print);
	config_found(self, "vrc4173piu", vrc4173bcu_print);
	config_found(self, "vrc4173kiu", vrc4173bcu_print);
	config_found(self, "vrc4173aiu", vrc4173bcu_print);
	config_found(self, "vrc4173ps2u", vrc4173bcu_print);

	/*
	 * Establish VRCINT interrupt.  Normally connected to one of
	 * GPIO pin in VR41xx.  XXX.
	 */
	if (0 <= sc->sc_platdep->intr_port) {
		sc->sc_ih = pci_vrcintr_establish(pc,
		    sc->sc_platdep->intr_port, vrc4173bcu_intr, sc);
		if (sc->sc_ih != NULL)
			printf("%s: interrupting at %p\n", sc->sc_dev.dv_xname,
			    sc->sc_ih);
	} else {
		printf("%s: interrupt port isn't specified\n",
		    sc->sc_dev.dv_xname);
	}
}

int
vrc4173bcu_print(void *aux, const char *pnp)
{
	const char *name = aux;

	if (pnp)
		printf("%s at %s", name, pnp);

	return (UNCONF);
}

int
vrc4173bcu_intr(void *arg)
{
	struct vrc4173bcu_softc *sc = (struct vrc4173bcu_softc *)arg;
	struct intrhand *ih;
	u_int16_t reg;
	int i, handled;

	reg = bus_space_read_2(sc->sc_iot, sc->sc_icuh, VRC4173ICU_SYSINT1);
	if (reg == 0)
		return (0);

#if 0
    {
	char buf[80];
	bitmask_snprintf(reg,
	    "\20\1USB\2PCMCIA2\3PCMCIA1\4PS2CH2\5PS2CH1\6PIU\7AIU\10KIU"
	    "\11GIU\12AC97\13AC97-1\14B11\15B12\16DOZEPIU\17B14\20B15",
	    buf, sizeof(buf));
	printf("%s: %s\n", sc->sc_dev.dv_xname, buf);
    }
#endif
	for (handled = i = 0; i < VRC4173BCU_NINTRHAND; i++) {
		ih = &sc->sc_intrhand[i];
		if (ih->ih_func != NULL && (reg & (1 << i)) != 0) {
			handled = 1;
			(*ih->ih_func)(ih->ih_arg);
		}
	}

	return (handled);
}

void *
vrc4173bcu_intr_establish(struct vrc4173bcu_softc *sc, int kind,
    int (*func)(void *), void *arg)
{
	struct intrhand *ih;

	DPRINTF(("vrc4173bcu_intr_establish: %d, %p, %p\n", kind, func, arg));
	if (kind < 0 || kind >= VRC4173BCU_NINTRHAND)
		return (NULL);

	ih = &sc->sc_intrhand[kind];
	if (ih->ih_func != NULL)
		return (NULL);

	ih->ih_func = func;
	ih->ih_arg = arg;
	sc->sc_intrmask |= (1 << kind);
	bus_space_write_2(sc->sc_iot, sc->sc_icuh, VRC4173ICU_MSYSINT1,
	    sc->sc_intrmask);

	return (ih);
}

void
vrc4173bcu_intr_disestablish(struct vrc4173bcu_softc *sc, void *ihp)
{
	struct intrhand *ih = ihp;

	if (ih < &sc->sc_intrhand[0] ||
	    ih >= &sc->sc_intrhand[VRC4173BCU_NINTRHAND])
		return;

	ih->ih_func = NULL;
	sc->sc_intrmask &= ~(1 << (ih - &sc->sc_intrhand[0]));
	bus_space_write_2(sc->sc_iot, sc->sc_icuh, VRC4173ICU_MSYSINT1,
	    sc->sc_intrmask);
}

int
vrc4173bcu_pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t intrtag = pa->pa_intrtag;
	int bus, dev, func;
#ifdef DEBUG
	int line = pa->pa_intrline;
	int pin = pa->pa_intrpin;
#endif

	pci_decompose_tag(pc, intrtag, &bus, &dev, &func);
	DPRINTF(("%s(%d, %d, %d): line = %d, pin = %d\n", pc->pc_dev->dv_xname,
	    bus, dev, func, line, pin));

	*ihp = -1;
	switch (dev) {
	case 1:				/* CARDU0 */
		*ihp = VRC4173ICU_PCMCIA1INTR;
		break;
	case 2:				/* CARDU1 */
		*ihp = VRC4173ICU_PCMCIA2INTR;
		break;
	case 12:			/* VRC4173 (SigmarionII) */
	case 19:			/* VRC4173 (MP-C303) */
		switch (func) {
		case 2:
			*ihp = VRC4173ICU_USBINTR;
			break;
		}
		break;
	}

	return (*ihp == -1);
}

const char *
vrc4173bcu_pci_intr_string(pci_chipset_tag_t pc, pci_intr_handle_t ih)
{
	static char irqstr[8 + sizeof("vrc4173 intr")];

	snprintf(irqstr, sizeof(irqstr), "vrc4173 intr %d", (int)ih);
	return (irqstr);
}

const struct evcnt *
vrc4173bcu_pci_intr_evcnt(pci_chipset_tag_t pc, pci_intr_handle_t ih)
{

	/* XXX for now, no evcnt parent reported */
	return (NULL);
}

