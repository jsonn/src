/*	$NetBSD: pciidevar.h,v 1.33.8.1 2006/07/13 17:49:29 gdamore Exp $	*/

/*
 * Copyright (c) 1998 Christopher G. Demetriou.  All rights reserved.
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

#ifndef _DEV_PCI_PCIIDEVAR_H_
#define	_DEV_PCI_PCIIDEVAR_H_

/*
 * PCI IDE driver exported software structures.
 *
 * Author: Christopher G. Demetriou, March 2, 1998.
 */

#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>
#include "opt_pciide.h"

/* options passed via the 'flags' config keyword */
#define	PCIIDE_OPTIONS_DMA	0x01
#define	PCIIDE_OPTIONS_NODMA	0x02

#ifndef ATADEBUG
#define ATADEBUG
#endif

#define DEBUG_DMA   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#ifdef ATADEBUG
extern int atadebug_pciide_mask;
#define ATADEBUG_PRINT(args, level) \
	if (atadebug_pciide_mask & (level)) printf args
#else
#define ATADEBUG_PRINT(args, level)
#endif

struct device;

/*
 * While standard PCI IDE controllers only have 2 channels, it is
 * common for PCI SATA controllers to have more.  Here we define
 * the maximum number of channels that any one PCI IDE device can
 * have.
 */
#define	PCIIDE_MAX_CHANNELS	4

struct pciide_softc {
	struct wdc_softc	sc_wdcdev;	/* common wdc definitions */
	pci_chipset_tag_t	sc_pc;		/* PCI registers info */
	pcitag_t		sc_tag;
	void			*sc_pci_ih;	/* PCI interrupt handle */
	int			sc_dma_ok;	/* bus-master DMA info */
	/*
	 * sc_dma_ioh may only be used to allocate the dma_iohs
	 * array in the channels (see below), or by chip-dependent
	 * code that knows what it's doing, as the registers may
	 * be laid out differently. All code in pciide_common.c
	 * must use the channel->dma_iohs array.
	 */
	bus_space_tag_t		sc_dma_iot;
	bus_space_handle_t	sc_dma_ioh;
	bus_dma_tag_t		sc_dmat;

	/*
	 * Some controllers might have DMA restrictions other than
	 * the norm.
	 */
	bus_size_t		sc_dma_maxsegsz;
	bus_size_t		sc_dma_boundary;

	/* For VIA/AMD/nVidia */
	bus_addr_t sc_apo_regbase;

	/* For Cypress */
	const struct cy82c693_handle *sc_cy_handle;
	int sc_cy_compatchan;

	/* for SiS */
	u_int8_t sis_type;

	/*
	 * For Silicon Image SATALink, Serverworks SATA, Artisea SATA
	 * and Promise SATA
	 */
	bus_space_tag_t sc_ba5_st;
	bus_space_handle_t sc_ba5_sh;
	int sc_ba5_en;

	/* Vendor info (for interpreting Chip description) */
	pcireg_t sc_pci_id;
	/* Chip description */
	const struct pciide_product_desc *sc_pp;
	/* common definitions */
	struct ata_channel *wdc_chanarray[PCIIDE_MAX_CHANNELS];
	/* internal bookkeeping */
	struct pciide_channel {			/* per-channel data */
		struct ata_channel ata_channel; /* generic part */
		const char	*name;
		int		compat;	/* is it compat? */
		void		*ih;	/* compat or pci handle */
		bus_space_handle_t ctl_baseioh; /* ctrl regs blk, native mode */
		/* DMA tables and DMA map for xfer, for each drive */
		struct pciide_dma_maps {
			bus_dmamap_t    dmamap_table;
			struct idedma_table *dma_table;
			bus_dmamap_t    dmamap_xfer;
			int dma_flags;
		} dma_maps[2];
		bus_space_handle_t	dma_iohs[IDEDMA_NREGS];
		/*
		 * Some controllers require certain bits to
		 * always be set for proper operation of the
		 * controller.  Set those bits here, if they're
		 * required.
		 */
		uint8_t		idedma_cmd;
	} pciide_channels[PCIIDE_MAX_CHANNELS];

	/* Power management */
	void			*sc_powerhook;
	struct pci_conf_state	sc_pciconf; /* Restore buffer */
	/* Intel power management */
	pcireg_t		sc_idetim;
	pcireg_t		sc_udmatim;
};

/* Given an ata_channel, get the pciide_softc. */
#define	CHAN_TO_PCIIDE(chp)	((struct pciide_softc *) (chp)->ch_atac)

/* Given an ata_channel, get the pciide_channel. */
#define	CHAN_TO_PCHAN(chp)	((struct pciide_channel *) (chp))

struct pciide_product_desc {
	u_int32_t ide_product;
	int ide_flags;
	const char *ide_name;
	/* map and setup chip, probe drives */
	void (*chip_map)(struct pciide_softc*, struct pci_attach_args*);
};

/* Flags for ide_flags */
#define	IDE_16BIT_IOSPACE	0x0002 /* I/O space BARS ignore upper word */


/* inlines for reading/writing 8-bit PCI registers */
static inline u_int8_t pciide_pci_read(pci_chipset_tag_t, pcitag_t, int);
static inline void pciide_pci_write(pci_chipset_tag_t, pcitag_t,
					   int, u_int8_t);

static inline u_int8_t
pciide_pci_read(pc, pa, reg)
	pci_chipset_tag_t pc;
	pcitag_t pa;
	int reg;
{

	return (pci_conf_read(pc, pa, (reg & ~0x03)) >>
	    ((reg & 0x03) * 8) & 0xff);
}

static inline void
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

void default_chip_map(struct pciide_softc*, struct pci_attach_args*);
void sata_setup_channel(struct ata_channel*);

void pciide_channel_dma_setup(struct pciide_channel *);
int  pciide_dma_table_setup(struct pciide_softc*, int, int);
int  pciide_dma_dmamap_setup(struct pciide_softc *, int, int,
				void *, size_t, int);
int  pciide_dma_init(void*, int, int, void *, size_t, int);
void pciide_dma_start(void*, int, int);
int  pciide_dma_finish(void*, int, int, int);
void pciide_irqack(struct ata_channel *);

/*
 * Functions defined by machine-dependent code.
 */

/* Attach compat interrupt handler, returning handle or NULL if failed. */
#ifdef __HAVE_PCIIDE_MACHDEP_COMPAT_INTR_ESTABLISH
void	*pciide_machdep_compat_intr_establish(struct device *,
	    struct pci_attach_args *, int, int (*)(void *), void *);
#endif

const struct pciide_product_desc* pciide_lookup_product
	(u_int32_t, const struct pciide_product_desc *);
void	pciide_common_attach(struct pciide_softc *, struct pci_attach_args *,
		const struct pciide_product_desc *);

int	pciide_chipen(struct pciide_softc *, struct pci_attach_args *);
void	pciide_mapregs_compat(struct pci_attach_args *,
	    struct pciide_channel *, int, bus_size_t *, bus_size_t*);
void	pciide_mapregs_native(struct pci_attach_args *,
	    struct pciide_channel *, bus_size_t *, bus_size_t *,
	    int (*pci_intr)(void *));
void	pciide_mapreg_dma(struct pciide_softc *,
	    struct pci_attach_args *);
int	pciide_chansetup(struct pciide_softc *, int, pcireg_t);
void	pciide_mapchan(struct pci_attach_args *,
	    struct pciide_channel *, pcireg_t, bus_size_t *, bus_size_t *,
	    int (*pci_intr)(void *));
void	pciide_map_compat_intr(struct pci_attach_args *,
	    struct pciide_channel *, int);
int	pciide_compat_intr(void *);
int	pciide_pci_intr(void *);

#endif /* _DEV_PCI_PCIIDEVAR_H_ */
