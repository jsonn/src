/*	$NetBSD: piixide.c,v 1.9.2.10 2005/11/10 14:06:03 skrll Exp $	*/

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
 *	This product includes software developed by Manuel Bouyer.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: piixide.c,v 1.9.2.10 2005/11/10 14:06:03 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_piix_reg.h>

static void piix_chip_map(struct pciide_softc*, struct pci_attach_args *);
static void piix_setup_channel(struct ata_channel *);
static void piix3_4_setup_channel(struct ata_channel *);
static u_int32_t piix_setup_idetim_timings(u_int8_t, u_int8_t, u_int8_t);
static u_int32_t piix_setup_idetim_drvs(struct ata_drive_datas *);
static u_int32_t piix_setup_sidetim_timings(u_int8_t, u_int8_t, u_int8_t);
static void piixsata_chip_map(struct pciide_softc*, struct pci_attach_args *);

static void piixide_powerhook(int, void *);
static int  piixide_match(struct device *, struct cfdata *, void *);
static void piixide_attach(struct device *, struct device *, void *);

static const struct pciide_product_desc pciide_intel_products[] =  {
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
	{ PCI_PRODUCT_INTEL_82440MX_IDE,
	  0,
	  "Intel 82440MX IDE controller",
	  piix_chip_map
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
	  "Intel 82801BAM IDE Controller (ICH2-M)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801CA_IDE_1,
	  0,
	  "Intel 82801CA IDE Controller (ICH3)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801CA_IDE_2,
	  0,
	  "Intel 82801CA IDE Controller (ICH3)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801DB_IDE,
	  0,
	  "Intel 82801DB IDE Controller (ICH4)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801DBM_IDE,
	  0,
	  "Intel 82801DBM IDE Controller (ICH4-M)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801EB_IDE,
	  0,
	  "Intel 82801EB IDE Controller (ICH5)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801EB_SATA,
	  0,
	  "Intel 82801EB Serial ATA Controller",
	  piixsata_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801ER_SATA,
	  0,
	  "Intel 82801ER Serial ATA/Raid Controller",
	  piixsata_chip_map,
	},
	{ PCI_PRODUCT_INTEL_6300ESB_IDE,
	  0,
	  "Intel 6300ESB IDE Controller (ICH5)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_6300ESB_SATA,
	  0,
	  "Intel 6300ESB Serial ATA Controller",
	  piixsata_chip_map,
	},
	{ PCI_PRODUCT_INTEL_6300ESB_RAID,
	  0,
	  "Intel 6300ESB Serial ATA/RAID Controller",
	  piixsata_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801FB_IDE,
	  0,
	  "Intel 82801FB IDE Controller (ICH6)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801FB_SATA,
	  0,
	  "Intel 82801FB Serial ATA/Raid Controller",
	  piixsata_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801FR_SATA,
	  0,
	  "Intel 82801FR Serial ATA/Raid Controller",
	  piixsata_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801FBM_SATA,
	  0,
	  "Intel 82801FBM Serial ATA Controller (ICH6)",
	  piixsata_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801G_IDE,
	  0,
	  "Intel 82801GB/GR IDE Controller (ICH7)",
	  piix_chip_map,
	},
	{ PCI_PRODUCT_INTEL_82801G_SATA,
	  0,
	  "Intel 82801GB/GR Serial ATA/Raid Controller (ICH7)",
	  piixsata_chip_map,
	},
	{ 0,
	  0,
	  NULL,
	  NULL
	}
};

CFATTACH_DECL(piixide, sizeof(struct pciide_softc),
    piixide_match, piixide_attach, NULL, NULL);

static int
piixide_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL) {
		if (pciide_lookup_product(pa->pa_id, pciide_intel_products))
			return (2);
	}
	return (0);
}

static void
piixide_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = (struct pciide_softc *)self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_intel_products));

	/* Setup our powerhook */
	sc->sc_powerhook = powerhook_establish(piixide_powerhook, sc);
	if (sc->sc_powerhook == NULL)
		printf("%s: WARNING: unable to establish PCI power hook\n",
		    sc->sc_wdcdev.sc_atac.atac_dev.dv_xname);
}

static void
piixide_powerhook(int why, void *hdl)
{
	struct pciide_softc *sc = (struct pciide_softc *)hdl;

	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		pci_conf_capture(sc->sc_pc, sc->sc_tag, &sc->sc_pciconf);
		break;
	case PWR_RESUME:
		pci_conf_restore(sc->sc_pc, sc->sc_tag, &sc->sc_pciconf);
		break;
	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
	}

	return;
}

static void
piix_chip_map(struct pciide_softc *sc, struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	u_int32_t idetim;
	bus_size_t cmdsize, ctlsize;
	pcireg_t interface = PCI_INTERFACE(pa->pa_class);

	if (pciide_chipen(sc, pa) == 0)
		return;

	aprint_normal("%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_atac.atac_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	aprint_normal("\n");
	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
		switch(sc->sc_pp->ide_product) {
		case PCI_PRODUCT_INTEL_82371AB_IDE:
		case PCI_PRODUCT_INTEL_82440MX_IDE:
		case PCI_PRODUCT_INTEL_82801AA_IDE:
		case PCI_PRODUCT_INTEL_82801AB_IDE:
		case PCI_PRODUCT_INTEL_82801BA_IDE:
		case PCI_PRODUCT_INTEL_82801BAM_IDE:
		case PCI_PRODUCT_INTEL_82801CA_IDE_1:
		case PCI_PRODUCT_INTEL_82801CA_IDE_2:
		case PCI_PRODUCT_INTEL_82801DB_IDE:
		case PCI_PRODUCT_INTEL_82801DBM_IDE:
		case PCI_PRODUCT_INTEL_82801EB_IDE:
		case PCI_PRODUCT_INTEL_6300ESB_IDE:
		case PCI_PRODUCT_INTEL_82801FB_IDE:
		case PCI_PRODUCT_INTEL_82801G_IDE:
			sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_UDMA;
		}
	}
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
	switch(sc->sc_pp->ide_product) {
	case PCI_PRODUCT_INTEL_82801AA_IDE:
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 4;
		break;
	case PCI_PRODUCT_INTEL_82801BA_IDE:
	case PCI_PRODUCT_INTEL_82801BAM_IDE:
	case PCI_PRODUCT_INTEL_82801CA_IDE_1:
	case PCI_PRODUCT_INTEL_82801CA_IDE_2:
	case PCI_PRODUCT_INTEL_82801DB_IDE:
	case PCI_PRODUCT_INTEL_82801DBM_IDE:
	case PCI_PRODUCT_INTEL_82801EB_IDE:
	case PCI_PRODUCT_INTEL_6300ESB_IDE:
	case PCI_PRODUCT_INTEL_82801FB_IDE:
	case PCI_PRODUCT_INTEL_82801G_IDE:
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 5;
		break;
	default:
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 2;
	}
	if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82371FB_IDE)
		sc->sc_wdcdev.sc_atac.atac_set_modes = piix_setup_channel;
	else
		sc->sc_wdcdev.sc_atac.atac_set_modes = piix3_4_setup_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;

	ATADEBUG_PRINT(("piix_setup_chip: old idetim=0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM)),
	    DEBUG_PROBE);
	if (sc->sc_pp->ide_product != PCI_PRODUCT_INTEL_82371FB_IDE) {
		ATADEBUG_PRINT((", sidetim=0x%x",
		    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM)),
		    DEBUG_PROBE);
		if (sc->sc_wdcdev.sc_atac.atac_cap & ATAC_CAP_UDMA) {
			ATADEBUG_PRINT((", udamreg 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG)),
			    DEBUG_PROBE);
		}
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE_1 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE_2 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DBM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801FB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_6300ESB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801G_IDE) {
			ATADEBUG_PRINT((", IDE_CONTROL 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_CONFIG)),
			    DEBUG_PROBE);
		}

	}
	ATADEBUG_PRINT(("\n"), DEBUG_PROBE);

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		idetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM);
		if ((PIIX_IDETIM_READ(idetim, channel) &
		    PIIX_IDETIM_IDE) == 0) {
#if 1
			aprint_normal("%s: %s channel ignored (disabled)\n",
			    sc->sc_wdcdev.sc_atac.atac_dev.dv_xname, cp->name);
			cp->ata_channel.ch_flags |= ATACH_DISABLED;
			continue;
#else
			pcireg_t interface;

			idetim = PIIX_IDETIM_SET(idetim, PIIX_IDETIM_IDE,
			    channel);
			pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_IDETIM,
			    idetim);
			interface = PCI_INTERFACE(pci_conf_read(sc->sc_pc,
			    sc->sc_tag, PCI_CLASS_REG));
			aprint_normal("channel %d idetim=%08x interface=%02x\n",
			    channel, idetim, interface);
#endif
		}
		pciide_mapchan(pa, cp, interface,
		    &cmdsize, &ctlsize, pciide_pci_intr);
	}

	ATADEBUG_PRINT(("piix_setup_chip: idetim=0x%x",
	    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM)),
	    DEBUG_PROBE);
	if (sc->sc_pp->ide_product != PCI_PRODUCT_INTEL_82371FB_IDE) {
		ATADEBUG_PRINT((", sidetim=0x%x",
		    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM)),
		    DEBUG_PROBE);
		if (sc->sc_wdcdev.sc_atac.atac_cap & ATAC_CAP_UDMA) {
			ATADEBUG_PRINT((", udamreg 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG)),
			    DEBUG_PROBE);
		}
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801AB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE_1 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE_2 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DBM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801FB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_6300ESB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801G_IDE) {
			ATADEBUG_PRINT((", IDE_CONTROL 0x%x",
			    pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_CONFIG)),
			    DEBUG_PROBE);
		}
	}
	ATADEBUG_PRINT(("\n"), DEBUG_PROBE);
}

static void
piix_setup_channel(struct ata_channel *chp)
{
	u_int8_t mode[2], drive;
	u_int32_t oidetim, idetim, idedma_ctl;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	struct ata_drive_datas *drvp = cp->ata_channel.ch_drive;

	oidetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM);
	idetim = PIIX_IDETIM_CLEAR(oidetim, 0xffff, chp->ch_channel);
	idedma_ctl = 0;

	/* set up new idetim: Enable IDE registers decode */
	idetim = PIIX_IDETIM_SET(idetim, PIIX_IDETIM_IDE,
	    chp->ch_channel);

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
			    mode[drive], 1, chp->ch_channel);
			goto end;
		}
	}
	/* If we are there, none of the drives are DMA */
	if (mode[0] >= 2)
		idetim |= piix_setup_idetim_timings(
		    mode[0], 0, chp->ch_channel);
	else
		idetim |= piix_setup_idetim_timings(
		    mode[1], 0, chp->ch_channel);
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
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_IDETIM, idetim);
}

static void
piix3_4_setup_channel(struct ata_channel *chp)
{
	struct ata_drive_datas *drvp;
	u_int32_t oidetim, idetim, sidetim, udmareg, ideconf, idedma_ctl;
	struct pciide_channel *cp = CHAN_TO_PCHAN(chp);
	struct pciide_softc *sc = CHAN_TO_PCIIDE(chp);
	struct wdc_softc *wdc = &sc->sc_wdcdev;
	int drive, s;
	int channel = chp->ch_channel;

	oidetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_IDETIM);
	sidetim = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM);
	udmareg = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG);
	ideconf = pci_conf_read(sc->sc_pc, sc->sc_tag, PIIX_CONFIG);
	idetim = PIIX_IDETIM_CLEAR(oidetim, 0xffff, channel);
	sidetim &= ~(PIIX_SIDETIM_ISP_MASK(channel) |
	    PIIX_SIDETIM_RTC_MASK(channel));
	idedma_ctl = 0;

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
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE_1 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE_2 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DBM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801FB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_6300ESB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801G_IDE) {
			ideconf |= PIIX_CONFIG_PINGPONG;
		}
		if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BA_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801BAM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE_1 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801CA_IDE_2 ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801DBM_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801EB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801FB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_6300ESB_IDE ||
		    sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82801G_IDE) {
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
		if ((wdc->sc_atac.atac_cap & ATAC_CAP_UDMA) &&
		    (drvp->drive_flags & DRIVE_UDMA)) {
			/* use Ultra/DMA */
			s = splbio();
			drvp->drive_flags &= ~DRIVE_DMA;
			splx(s);
			udmareg |= PIIX_UDMACTL_DRV_EN( channel, drive);
			udmareg |= PIIX_UDMATIM_SET(
			    piix4_sct_udma[drvp->UDMA_mode], channel, drive);
		} else {
			/* use Multiword DMA */
			s = splbio();
			drvp->drive_flags &= ~DRIVE_UDMA;
			splx(s);
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
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_IDETIM, idetim);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_SIDETIM, sidetim);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_UDMAREG, udmareg);
	pci_conf_write(sc->sc_pc, sc->sc_tag, PIIX_CONFIG, ideconf);
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
	struct ata_channel *chp = drvp->chnl_softc;
	u_int8_t channel = chp->ch_channel;
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

static void
piixsata_chip_map(struct pciide_softc *sc, struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	bus_size_t cmdsize, ctlsize;
	pcireg_t interface, cmdsts;
	int channel;

	if (pciide_chipen(sc, pa) == 0)
		return;

	aprint_normal("%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_atac.atac_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	aprint_normal("\n");

	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16 | ATAC_CAP_DATA32;
	sc->sc_wdcdev.sc_atac.atac_pio_cap = 4;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DMA | ATAC_CAP_UDMA;
		sc->sc_wdcdev.irqack = pciide_irqack;
		sc->sc_wdcdev.sc_atac.atac_dma_cap = 2;
		sc->sc_wdcdev.sc_atac.atac_udma_cap = 6;
	}
	sc->sc_wdcdev.sc_atac.atac_set_modes = sata_setup_channel;

	sc->sc_wdcdev.sc_atac.atac_channels = sc->wdc_chanarray;
	sc->sc_wdcdev.sc_atac.atac_nchannels = PCIIDE_NUM_CHANNELS;

	cmdsts = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG);
	cmdsts &= ~0x0400;
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG, cmdsts);

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_RAID)
		sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_RAID;

	interface = PCI_INTERFACE(pa->pa_class);

	wdc_allocate_regs(&sc->sc_wdcdev);

	for (channel = 0; channel < sc->sc_wdcdev.sc_atac.atac_nchannels;
	     channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		    pciide_pci_intr);
	}
}
