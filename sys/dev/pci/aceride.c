/*	$NetBSD: aceride.c,v 1.7.2.2 2004/08/03 10:49:06 skrll Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_acer_reg.h>

static void acer_chip_map(struct pciide_softc*, struct pci_attach_args*);
static void acer_setup_channel(struct wdc_channel*);
static int  acer_pci_intr(void *);

static int  aceride_match(struct device *, struct cfdata *, void *);
static void aceride_attach(struct device *, struct device *, void *);

CFATTACH_DECL(aceride, sizeof(struct pciide_softc),
    aceride_match, aceride_attach, NULL, NULL);

static const struct pciide_product_desc pciide_acer_products[] =  {
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

static int
aceride_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ALI &&
	    PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		if (pciide_lookup_product(pa->pa_id, pciide_acer_products))
			return (2);
	}
	return (0);
}

static void
aceride_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct pciide_softc *sc = (struct pciide_softc *)self;

	pciide_common_attach(sc, pa,
	    pciide_lookup_product(pa->pa_id, pciide_acer_products));

}

static void
acer_chip_map(struct pciide_softc *sc, struct pci_attach_args *pa)
{
	struct pciide_channel *cp;
	int channel;
	pcireg_t cr, interface;
	bus_size_t cmdsize, ctlsize;
	pcireg_t rev = PCI_REVISION(pa->pa_class);

	if (pciide_chipen(sc, pa) == 0)
		return;

	aprint_normal("%s: bus-master DMA support present",
	    sc->sc_wdcdev.sc_dev.dv_xname);
	pciide_mapreg_dma(sc, pa);
	aprint_normal("\n");
	sc->sc_wdcdev.cap = WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DATA32 |
	    WDC_CAPABILITY_MODE;
	if (sc->sc_dma_ok) {
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA;
		if (rev >= 0x20) {
			sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
			if (rev >= 0xC4)
				sc->sc_wdcdev.UDMA_cap = 5;
			else if (rev >= 0xC2)
				sc->sc_wdcdev.UDMA_cap = 4;
			else
				sc->sc_wdcdev.UDMA_cap = 2;
		}
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_IRQACK;
		sc->sc_wdcdev.irqack = pciide_irqack;
	}
	    
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.DMA_cap = 2;
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

	/* From linux: enable "Cable Detection" */
	if (rev >= 0xC2) {
		pciide_pci_write(sc->sc_pc, sc->sc_tag, ACER_0x4B,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_0x4B)
		    | ACER_0x4B_CDETECT);
	}

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		cp = &sc->pciide_channels[channel];
		if (pciide_chansetup(sc, channel, interface) == 0)
			continue;
		if ((interface & PCIIDE_CHAN_EN(channel)) == 0) {
			aprint_normal("%s: %s channel ignored (disabled)\n",
			    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			cp->wdc_channel.ch_flags |= WDCF_DISABLED;
			continue;
		}
		/* newer controllers seems to lack the ACER_CHIDS. Sigh */
		pciide_mapchan(pa, cp, interface, &cmdsize, &ctlsize,
		     (rev >= 0xC2) ? pciide_pci_intr : acer_pci_intr);
	}
}

static void
acer_setup_channel(struct wdc_channel *chp)
{
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t acer_fifo_udma;
	u_int32_t idedma_ctl;
	struct pciide_channel *cp = (struct pciide_channel*)chp;
	struct pciide_softc *sc = (struct pciide_softc *)cp->wdc_channel.ch_wdc;

	idedma_ctl = 0;
	acer_fifo_udma = pci_conf_read(sc->sc_pc, sc->sc_tag, ACER_FTH_UDMA);
	WDCDEBUG_PRINT(("acer_setup_channel: old fifo/udma reg 0x%x\n", 
	    acer_fifo_udma), DEBUG_PROBE);
	/* setup DMA if needed */
	pciide_channel_dma_setup(cp);

	if ((chp->ch_drive[0].drive_flags | chp->ch_drive[1].drive_flags) &
	    DRIVE_UDMA) { /* check 80 pins cable */
		if (pciide_pci_read(sc->sc_pc, sc->sc_tag, ACER_0x4A) &
		    ACER_0x4A_80PIN(chp->ch_channel)) {
			if (chp->ch_drive[0].UDMA_mode > 2)
				chp->ch_drive[0].UDMA_mode = 2;
			if (chp->ch_drive[1].UDMA_mode > 2)
				chp->ch_drive[1].UDMA_mode = 2;
		}
	}

	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		WDCDEBUG_PRINT(("acer_setup_channel: old timings reg for "
		    "channel %d drive %d 0x%x\n", chp->ch_channel, drive,
		    pciide_pci_read(sc->sc_pc, sc->sc_tag,
		    ACER_IDETIM(chp->ch_channel, drive))), DEBUG_PROBE);
		/* clear FIFO/DMA mode */
		acer_fifo_udma &= ~(ACER_FTH_OPL(chp->ch_channel, drive, 0x3) |
		    ACER_UDMA_EN(chp->ch_channel, drive) |
		    ACER_UDMA_TIM(chp->ch_channel, drive, 0x7));

		/* add timing values, setup DMA if needed */
		if ((drvp->drive_flags & DRIVE_DMA) == 0 &&
		    (drvp->drive_flags & DRIVE_UDMA) == 0) {
			acer_fifo_udma |=
			    ACER_FTH_OPL(chp->ch_channel, drive, 0x1);
			goto pio;
		}

		acer_fifo_udma |= ACER_FTH_OPL(chp->ch_channel, drive, 0x2);
		if (drvp->drive_flags & DRIVE_UDMA) {
			/* use Ultra/DMA */
			drvp->drive_flags &= ~DRIVE_DMA;
			acer_fifo_udma |= ACER_UDMA_EN(chp->ch_channel, drive);
			acer_fifo_udma |= 
			    ACER_UDMA_TIM(chp->ch_channel, drive,
				acer_udma[drvp->UDMA_mode]);
			/* XXX disable if one drive < UDMA3 ? */
			if (drvp->UDMA_mode >= 3) {
				pciide_pci_write(sc->sc_pc, sc->sc_tag,
				    ACER_0x4B,
				    pciide_pci_read(sc->sc_pc, sc->sc_tag,
					ACER_0x4B) | ACER_0x4B_UDMA66);
			}
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
		    ACER_IDETIM(chp->ch_channel, drive),
		    acer_pio[drvp->PIO_mode]);
	}
	WDCDEBUG_PRINT(("acer_setup_channel: new fifo/udma reg 0x%x\n",
	    acer_fifo_udma), DEBUG_PROBE);
	pci_conf_write(sc->sc_pc, sc->sc_tag, ACER_FTH_UDMA, acer_fifo_udma);
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, cp->dma_iohs[IDEDMA_CTL], 0,
		    idedma_ctl);
	}
}

static int
acer_pci_intr(void *arg)
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct wdc_channel *wdc_cp;
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
			if (crv == 0) {
				printf("%s:%d: bogus intr\n",
				    sc->sc_wdcdev.sc_dev.dv_xname, i);
				pciide_irqack(wdc_cp);
			} else
				rv = 1;
		}
	}
	return rv;
}
