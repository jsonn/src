/*	$NetBSD: amr.c,v 1.1.4.3 2002/06/20 03:45:19 nathanw Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

/*-
 * Copyright (c) 1999,2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from FreeBSD: amr_pci.c,v 1.5 2000/08/30 07:52:40 msmith Exp
 * from FreeBSD: amr.c,v 1.16 2000/08/30 07:52:40 msmith Exp 
 */

/*
 * Driver for AMI RAID controllers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: amr.c,v 1.1.4.3 2002/06/20 03:45:19 nathanw Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/endian.h>
#include <machine/bus.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/amrreg.h>
#include <dev/pci/amrvar.h>

#if AMR_MAX_SEGS > 32
#error AMR_MAX_SEGS too high
#endif

#define	AMR_ENQUIRY_BUFSIZE	2048
#define	AMR_SGL_SIZE		(sizeof(struct amr_sgentry) * 32)

void	amr_attach(struct device *, struct device *, void *);
void	*amr_enquire(struct amr_softc *, u_int8_t, u_int8_t, u_int8_t);
int	amr_init(struct amr_softc *, const char *,
			 struct pci_attach_args *pa);
int	amr_intr(void *);
int	amr_match(struct device *, struct cfdata *, void *);
int	amr_print(void *, const char *);
void	amr_shutdown(void *);
int	amr_submatch(struct device *, struct cfdata *, void *);

int	amr_mbox_wait(struct amr_softc *);
int	amr_quartz_get_work(struct amr_softc *, struct amr_mailbox *);
int	amr_quartz_submit(struct amr_softc *, struct amr_ccb *);
int	amr_std_get_work(struct amr_softc *, struct amr_mailbox *);
int	amr_std_submit(struct amr_softc *, struct amr_ccb *);

static inline u_int8_t	amr_inb(struct amr_softc *, int);
static inline u_int32_t	amr_inl(struct amr_softc *, int);
static inline void	amr_outb(struct amr_softc *, int, u_int8_t);
static inline void	amr_outl(struct amr_softc *, int, u_int32_t);

struct cfattach amr_ca = {
	sizeof(struct amr_softc), amr_match, amr_attach
};

#define AT_QUARTZ	0x01	/* `Quartz' chipset */
#define	AT_SIG		0x02	/* Check for signature */

struct amr_pci_type {
	u_short	apt_vendor;
	u_short	apt_product;
	u_short	apt_flags;
} static const amr_pci_type[] = {
	{ PCI_VENDOR_AMI,   PCI_PRODUCT_AMI_MEGARAID,  0 },
	{ PCI_VENDOR_AMI,   PCI_PRODUCT_AMI_MEGARAID2, 0 },
	{ PCI_VENDOR_AMI,   PCI_PRODUCT_AMI_MEGARAID3, AT_QUARTZ },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_AMI_MEGARAID3, AT_QUARTZ | AT_SIG }
};

struct amr_typestr {
	const char	*at_str;
	int		at_sig;
} static const amr_typestr[] = {
	{ "Series 431",			AMR_SIG_431 },
	{ "Series 438",			AMR_SIG_438 },
	{ "Series 466",			AMR_SIG_466 },
	{ "Series 467",			AMR_SIG_467 },
	{ "Series 490",			AMR_SIG_490 },
	{ "Series 762",			AMR_SIG_762 },
	{ "HP NetRAID (T5)",		AMR_SIG_T5 },
	{ "HP NetRAID (T7)",		AMR_SIG_T7 },
};

static void	*amr_sdh;

static inline u_int8_t
amr_inb(struct amr_softc *amr, int off)
{

	bus_space_barrier(amr->amr_iot, amr->amr_ioh, off, 1,
	    BUS_SPACE_BARRIER_WRITE | BUS_SPACE_BARRIER_READ);
	return (bus_space_read_1(amr->amr_iot, amr->amr_ioh, off));
}

static inline u_int32_t
amr_inl(struct amr_softc *amr, int off)
{

	bus_space_barrier(amr->amr_iot, amr->amr_ioh, off, 4,
	    BUS_SPACE_BARRIER_WRITE | BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(amr->amr_iot, amr->amr_ioh, off));
}

static inline void
amr_outb(struct amr_softc *amr, int off, u_int8_t val)
{

	bus_space_write_1(amr->amr_iot, amr->amr_ioh, off, val);
	bus_space_barrier(amr->amr_iot, amr->amr_ioh, off, 1,
	    BUS_SPACE_BARRIER_WRITE);
}

static inline void
amr_outl(struct amr_softc *amr, int off, u_int32_t val)
{

	bus_space_write_4(amr->amr_iot, amr->amr_ioh, off, val);
	bus_space_barrier(amr->amr_iot, amr->amr_ioh, off, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

/*
 * Match a supported device.
 */
int
amr_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct pci_attach_args *pa;
	pcireg_t s;
	int i;

	pa = (struct pci_attach_args *)aux;

	/*
	 * Don't match the device if it's operating in I2O mode.  In this
	 * case it should be handled by the `iop' driver.
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_I2O)
		return (0);

	for (i = 0; i < sizeof(amr_pci_type) / sizeof(amr_pci_type[0]); i++)
		if (PCI_VENDOR(pa->pa_id) == amr_pci_type[i].apt_vendor && 
		    PCI_PRODUCT(pa->pa_id) == amr_pci_type[i].apt_product)
		    	break;

	if (i == sizeof(amr_pci_type) / sizeof(amr_pci_type[0]))
		return (0);

	if ((amr_pci_type[i].apt_flags & AT_SIG) == 0)
		return (1);

	s = pci_conf_read(pa->pa_pc, pa->pa_tag, AMR_QUARTZ_SIG_REG) & 0xffff;
	return (s == AMR_QUARTZ_SIG0 || s == AMR_QUARTZ_SIG1);
}

/*
 * Attach a supported device.  XXX This doesn't fail gracefully, and may
 * over-allocate resources.
 */
void
amr_attach(struct device *parent, struct device *self, void *aux)
{
	bus_space_tag_t memt, iot;
	bus_space_handle_t memh, ioh;
	struct pci_attach_args *pa;
	struct amr_attach_args amra;
	const struct amr_pci_type *apt;
	struct amr_softc *amr;
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	const char *intrstr;
	pcireg_t reg;
	int rseg, i, size, rv, memreg, ioreg;
        bus_dma_segment_t seg;
        struct amr_ccb *ac;

	amr = (struct amr_softc *)self;
	pa = (struct pci_attach_args *)aux;
	pc = pa->pa_pc;

	for (i = 0; i < sizeof(amr_pci_type) / sizeof(amr_pci_type[0]); i++)
		if (PCI_VENDOR(pa->pa_id) == amr_pci_type[i].apt_vendor &&
		    PCI_PRODUCT(pa->pa_id) == amr_pci_type[i].apt_product)
			break;
	apt = amr_pci_type + i;

	memreg = ioreg = 0;
	for (i = 0x10; i <= 0x14; i += 4) {
		reg = pci_conf_read(pc, pa->pa_tag, i);
		switch (PCI_MAPREG_TYPE(reg)) {
		case PCI_MAPREG_TYPE_MEM:
			if (PCI_MAPREG_MEM_SIZE(reg) != 0)
				memreg = i;
			break;
		case PCI_MAPREG_TYPE_IO:
			if (PCI_MAPREG_IO_SIZE(reg) != 0)
				ioreg = i;
			break;
		}
	}

	if (memreg != 0)
		if (pci_mapreg_map(pa, memreg, PCI_MAPREG_TYPE_MEM, 0,
		    &memt, &memh, NULL, NULL))
			memreg = 0;
	if (ioreg != 0)
		if (pci_mapreg_map(pa, ioreg, PCI_MAPREG_TYPE_IO, 0,
		    &iot, &ioh, NULL, NULL))
			ioreg = 0;

	if (memreg) {
		amr->amr_iot = memt;
		amr->amr_ioh = memh;
	} else if (ioreg) {
		amr->amr_iot = iot;
		amr->amr_ioh = ioh;
	} else {
		printf("can't map control registers\n");
		return;
	}

	amr->amr_dmat = pa->pa_dmat;

	/* Enable the device. */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    reg | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		printf("can't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	amr->amr_ih = pci_intr_establish(pc, ih, IPL_BIO, amr_intr, amr);
	if (amr->amr_ih == NULL) {
		printf("can't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	/*
	 * Allocate space for the mailbox and S/G lists.  Some controllers
	 * don't like S/G lists to be located below 0x2000, so we allocate
	 * enough slop to enable us to compensate.
	 *
	 * The standard mailbox structure needs to be aligned on a 16-byte
	 * boundary.  The 64-bit mailbox has one extra field, 4 bytes in
	 * size, which preceeds the standard mailbox.
	 */
	size = AMR_SGL_SIZE * AMR_MAX_CMDS + 0x2000;

	if ((rv = bus_dmamem_alloc(amr->amr_dmat, size, PAGE_SIZE, NULL, &seg,
	    1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate buffer, rv = %d\n",
		    amr->amr_dv.dv_xname, rv);
		return;
	}

	if ((rv = bus_dmamem_map(amr->amr_dmat, &seg, rseg, size, 
	    (caddr_t *)&amr->amr_mbox,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map buffer, rv = %d\n",
		    amr->amr_dv.dv_xname, rv);
		return;
	}

	if ((rv = bus_dmamap_create(amr->amr_dmat, size, 1, size, 0, 
	    BUS_DMA_NOWAIT, &amr->amr_dmamap)) != 0) {
		printf("%s: unable to create buffer DMA map, rv = %d\n",
		    amr->amr_dv.dv_xname, rv);
		return;
	}

	if ((rv = bus_dmamap_load(amr->amr_dmat, amr->amr_dmamap,
	    amr->amr_mbox, size, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load buffer DMA map, rv = %d\n",
		    amr->amr_dv.dv_xname, rv);
		return;
	}

	memset(amr->amr_mbox, 0, size);

	amr->amr_mbox_paddr = amr->amr_dmamap->dm_segs[0].ds_addr + 16;
	amr->amr_sgls_paddr = (amr->amr_mbox_paddr + 0x1fff) & ~0x1fff;
	amr->amr_sgls = (struct amr_sgentry *)((caddr_t)amr->amr_mbox +
	    amr->amr_sgls_paddr - amr->amr_dmamap->dm_segs[0].ds_addr);
	amr->amr_mbox = (struct amr_mailbox *)((caddr_t)amr->amr_mbox + 16);

	/*
	 * Allocate and initalise the command control blocks.
	 */
	ac = malloc(sizeof(*ac) * AMR_MAX_CMDS, M_DEVBUF, M_NOWAIT | M_ZERO);
	amr->amr_ccbs = ac;
	SLIST_INIT(&amr->amr_ccb_freelist);

	for (i = 0; i < AMR_MAX_CMDS; i++, ac++) {
		rv = bus_dmamap_create(amr->amr_dmat, AMR_MAX_XFER,
		    AMR_MAX_SEGS, AMR_MAX_XFER, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ac->ac_xfer_map);
		if (rv != 0)
			break;

		ac->ac_ident = i;
		SLIST_INSERT_HEAD(&amr->amr_ccb_freelist, ac, ac_chain.slist);
	}
	if (i != AMR_MAX_CMDS)
		printf("%s: %d/%d CCBs created\n", amr->amr_dv.dv_xname,
		    i, AMR_MAX_CMDS);

	/*
	 * Take care of model-specific tasks.
	 */
	if ((apt->apt_flags & AT_QUARTZ) != 0) {
		amr->amr_submit = amr_quartz_submit;
		amr->amr_get_work = amr_quartz_get_work;
	} else {
		amr->amr_submit = amr_std_submit;
		amr->amr_get_work = amr_std_get_work;

		/* Notify the controller of the mailbox location. */
		amr_outl(amr, AMR_SREG_MBOX, amr->amr_mbox_paddr);
		amr_outb(amr, AMR_SREG_MBOX_ENABLE, AMR_SMBOX_ENABLE_ADDR);

		/* Clear outstanding interrupts and enable interrupts. */
		amr_outb(amr, AMR_SREG_CMD, AMR_SCMD_ACKINTR);
		amr_outb(amr, AMR_SREG_TOGL,
		    amr_inb(amr, AMR_SREG_TOGL) | AMR_STOGL_ENABLE);
	}

	/*
	 * Retrieve parameters, and tell the world about us.
	 */
	amr->amr_maxqueuecnt = i;
	printf(": AMI RAID ");
	if (amr_init(amr, intrstr, pa) != 0)
		return;

	/* 
	 * Cap the maximum number of outstanding commands.  AMI's Linux
	 * driver doesn't trust the controller's reported value, and lockups
	 * have been seen when we do.
	 */
	amr->amr_maxqueuecnt = min(amr->amr_maxqueuecnt, AMR_MAX_CMDS);
	if (amr->amr_maxqueuecnt > i)
		amr->amr_maxqueuecnt = i;

	/* Set our `shutdownhook' before we start any device activity. */
	if (amr_sdh == NULL)
		amr_sdh = shutdownhook_establish(amr_shutdown, NULL);

	/* Attach sub-devices. */
	for (i = 0; i < amr->amr_numdrives; i++) {
		if (amr->amr_drive[i].al_size == 0)
			continue;
		amra.amra_unit = i;
		config_found_sm(&amr->amr_dv, &amra, amr_print, amr_submatch);
	}

	SIMPLEQ_INIT(&amr->amr_ccb_queue);
}

/*
 * Print autoconfiguration message for a sub-device.
 */
int
amr_print(void *aux, const char *pnp)
{
	struct amr_attach_args *amra;

	amra = (struct amr_attach_args *)aux;

	if (pnp != NULL)
		printf("block device at %s", pnp);
	printf(" unit %d", amra->amra_unit);
	return (UNCONF);
}

/*
 * Match a sub-device.
 */
int
amr_submatch(struct device *parent, struct cfdata *cf, void *aux)
{
	struct amr_attach_args *amra;

	amra = (struct amr_attach_args *)aux;

	if (cf->amracf_unit != AMRCF_UNIT_DEFAULT &&
	    cf->amracf_unit != amra->amra_unit)
		return (0);

	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

/*
 * Retrieve operational parameters and describe the controller.
 */
int
amr_init(struct amr_softc *amr, const char *intrstr,
	 struct pci_attach_args *pa)
{
	struct amr_prodinfo *ap;
	struct amr_enquiry *ae;
	struct amr_enquiry3 *aex;
	const char *prodstr;
	u_int i, sig;
	char buf[64];

	/*
	 * Try to get 40LD product info, which tells us what the card is
	 * labelled as.
	 */
	ap = amr_enquire(amr, AMR_CMD_CONFIG, AMR_CONFIG_PRODUCT_INFO, 0);
	if (ap != NULL) {
		printf("<%.80s>\n", ap->ap_product);
		if (intrstr != NULL)
			printf("%s: interrupting at %s\n",
			    amr->amr_dv.dv_xname, intrstr);
		printf("%s: firmware %.16s, BIOS %.16s, %dMB RAM\n",
		    amr->amr_dv.dv_xname, ap->ap_firmware, ap->ap_bios,
		    le16toh(ap->ap_memsize));

		amr->amr_maxqueuecnt = ap->ap_maxio;
		free(ap, M_DEVBUF);

		/*
		 * Fetch and record state of logical drives.
		 */
		aex = amr_enquire(amr, AMR_CMD_CONFIG, AMR_CONFIG_ENQ3,
		    AMR_CONFIG_ENQ3_SOLICITED_FULL);
		if (aex == NULL) {
			printf("%s ENQUIRY3 failed\n", amr->amr_dv.dv_xname);
			return (-1);
		}

		if (aex->ae_numldrives > AMR_MAX_UNITS) {
			printf("%s: adjust AMR_MAX_UNITS to %d (currently %d)"
			    "\n", amr->amr_dv.dv_xname,
			    ae->ae_ldrv.al_numdrives, AMR_MAX_UNITS);
			amr->amr_numdrives = AMR_MAX_UNITS;
		} else
			amr->amr_numdrives = aex->ae_numldrives;

		for (i = 0; i < amr->amr_numdrives; i++) {
			amr->amr_drive[i].al_size =
			    le32toh(aex->ae_drivesize[i]);
			amr->amr_drive[i].al_state = aex->ae_drivestate[i];
			amr->amr_drive[i].al_properties = aex->ae_driveprop[i];
		}

		free(aex, M_DEVBUF);
		return (0);
	}

	/*
	 * Try 8LD extended ENQUIRY to get the controller signature.  Once
	 * found, search for a product description.
	 */
	if ((ae = amr_enquire(amr, AMR_CMD_EXT_ENQUIRY2, 0, 0)) != NULL) {
		i = 0;
		sig = le32toh(ae->ae_signature);

		while (i < sizeof(amr_typestr) / sizeof(amr_typestr[0])) {
			if (amr_typestr[i].at_sig == sig)
				break;
			i++;
		}
		if (i == sizeof(amr_typestr) / sizeof(amr_typestr[0])) {
			sprintf(buf, "unknown ENQUIRY2 sig (0x%08x)", sig);
			prodstr = buf;
		} else
			prodstr = amr_typestr[i].at_str;
	} else {
		if ((ae = amr_enquire(amr, AMR_CMD_ENQUIRY, 0, 0)) == NULL) {
			printf("%s: unsupported controller\n",
			    amr->amr_dv.dv_xname);
			return (-1);
		}

		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_AMI_MEGARAID:
			prodstr = "Series 428";
			break;
		case PCI_PRODUCT_AMI_MEGARAID2:
			prodstr = "Series 434";
			break;
		default:
			sprintf(buf, "unknown PCI dev (0x%04x)",
			    PCI_PRODUCT(pa->pa_id));
			prodstr = buf;
			break;
		}
	}

	printf("<%s>\n", prodstr);
	if (intrstr != NULL)
		printf("%s: interrupting at %s\n", amr->amr_dv.dv_xname,
		    intrstr);
	printf("%s: firmware <%.4s>, BIOS <%.4s>, %dMB RAM\n",
	    amr->amr_dv.dv_xname, ae->ae_adapter.aa_firmware,
	    ae->ae_adapter.aa_bios, ae->ae_adapter.aa_memorysize);

	amr->amr_maxqueuecnt = ae->ae_adapter.aa_maxio;

	/*
	 * Record state of logical drives.
	 */
	if (ae->ae_ldrv.al_numdrives > AMR_MAX_UNITS) {
		printf("%s: adjust AMR_MAX_UNITS to %d (currently %d)\n",
		    amr->amr_dv.dv_xname, ae->ae_ldrv.al_numdrives,
		    AMR_MAX_UNITS);
		amr->amr_numdrives = AMR_MAX_UNITS;
	} else
		amr->amr_numdrives = ae->ae_ldrv.al_numdrives;

	for (i = 0; i < AMR_MAX_UNITS; i++) {
		amr->amr_drive[i].al_size = le32toh(ae->ae_ldrv.al_size[i]);
		amr->amr_drive[i].al_state = ae->ae_ldrv.al_state[i];
		amr->amr_drive[i].al_properties = ae->ae_ldrv.al_properties[i];
	}

	free(ae, M_DEVBUF);
	return (0);
}

/*
 * Flush the internal cache on each configured controller.  Called at
 * shutdown time.
 */
void
amr_shutdown(void *cookie)
{
        extern struct cfdriver amr_cd;
	struct amr_softc *amr;
	struct amr_ccb *ac;
	int i, rv;

	for (i = 0; i < amr_cd.cd_ndevs; i++) {
		if ((amr = device_lookup(&amr_cd, i)) == NULL)
			continue;

		if ((rv = amr_ccb_alloc(amr, &ac)) == 0) {
			ac->ac_mbox.mb_command = AMR_CMD_FLUSH;
			rv = amr_ccb_poll(amr, ac, 30000);
			amr_ccb_free(amr, ac);
		}
		if (rv != 0)
			printf("%s: unable to flush cache (%d)\n",
			    amr->amr_dv.dv_xname, rv);
	}
}

/*
 * Interrupt service routine.
 */
int
amr_intr(void *cookie)
{
	struct amr_softc *amr;
	struct amr_ccb *ac;
	struct amr_mailbox mbox;
	u_int i, forus, idx;

	amr = cookie;
	forus = 0;

	while ((*amr->amr_get_work)(amr, &mbox) == 0) {
		/* Iterate over completed commands in this result. */
		for (i = 0; i < mbox.mb_nstatus; i++) {
			idx = mbox.mb_completed[i] - 1;
			ac = amr->amr_ccbs + idx;

			if (idx >= amr->amr_maxqueuecnt) {
				printf("%s: bad status (bogus ID: %u=%u)\n",
				    amr->amr_dv.dv_xname, i, idx);
				continue;
			}

			if ((ac->ac_flags & AC_ACTIVE) == 0) {
				printf("%s: bad status (not active; 0x04%x)\n",
				    amr->amr_dv.dv_xname, ac->ac_flags);
				continue;
			}

			ac->ac_status = mbox.mb_status;
			ac->ac_flags = (ac->ac_flags & ~AC_ACTIVE) |
			    AC_COMPLETE;

			/* Pass notification to upper layers. */
			if (ac->ac_handler != NULL)
				(*ac->ac_handler)(ac);
		}
		forus = 1;
	}

	if (forus)
		amr_ccb_enqueue(amr, NULL);
	return (forus);
}

/*
 * Run a generic enquiry-style command.
 */
void *
amr_enquire(struct amr_softc *amr, u_int8_t cmd, u_int8_t cmdsub,
	    u_int8_t cmdqual)
{
	struct amr_ccb *ac;
	u_int8_t *mb;
	void *buf;
	int rv;

	if (amr_ccb_alloc(amr, &ac) != 0)
		return (NULL);
	buf = malloc(AMR_ENQUIRY_BUFSIZE, M_DEVBUF, M_NOWAIT);

	/* Build the command proper. */
	mb = (u_int8_t *)&ac->ac_mbox;
	mb[0] = cmd;
	mb[2] = cmdsub;
	mb[3] = cmdqual;

	if ((rv = amr_ccb_map(amr, ac, buf, AMR_ENQUIRY_BUFSIZE, 0)) == 0) {
		rv = amr_ccb_poll(amr, ac, 2000);
		amr_ccb_unmap(amr, ac);
	}

	amr_ccb_free(amr, ac);

	if (rv != 0) {
		free(buf, M_DEVBUF);
		buf = NULL;
	}

	return (buf);
}

/*
 * Allocate and initialise a CCB.
 */
int
amr_ccb_alloc(struct amr_softc *amr, struct amr_ccb **acp)
{
	struct amr_ccb *ac;
	struct amr_mailbox *mb;
	int s;

	s = splbio();
	if ((ac = SLIST_FIRST(&amr->amr_ccb_freelist)) == NULL) {
		splx(s);
		return (EAGAIN);
	}
	SLIST_REMOVE_HEAD(&amr->amr_ccb_freelist, ac_chain.slist);
	splx(s);

	ac->ac_handler = NULL;
	mb = &ac->ac_mbox;
	*acp = ac;

	memset(mb, 0, sizeof(*mb));

	mb->mb_ident = ac->ac_ident + 1;
	mb->mb_busy = 1;
	mb->mb_poll = 0;
	mb->mb_ack = 0;

	return (0);
}

/*
 * Free a CCB.
 */
void
amr_ccb_free(struct amr_softc *amr, struct amr_ccb *ac)
{
	int s;

	ac->ac_flags = 0;

	s = splbio();
	SLIST_INSERT_HEAD(&amr->amr_ccb_freelist, ac, ac_chain.slist);
	splx(s);
}

/*
 * If a CCB is specified, enqueue it.  Pull CCBs off the software queue in
 * the order that they were enqueued and try to submit their command blocks
 * to the controller for execution.
 */
void
amr_ccb_enqueue(struct amr_softc *amr, struct amr_ccb *ac)
{
	int s;

	s = splbio();

	if (ac != NULL)
		SIMPLEQ_INSERT_TAIL(&amr->amr_ccb_queue, ac, ac_chain.simpleq);

	while ((ac = SIMPLEQ_FIRST(&amr->amr_ccb_queue)) != NULL) {
		if ((*amr->amr_submit)(amr, ac) != 0)
			break;
		SIMPLEQ_REMOVE_HEAD(&amr->amr_ccb_queue, ac_chain.simpleq);
	}

	splx(s);
}

/*
 * Map the specified CCB's data buffer onto the bus, and fill the
 * scatter-gather list.
 */
int
amr_ccb_map(struct amr_softc *amr, struct amr_ccb *ac, void *data, int size,
	    int out)
{
	struct amr_sgentry *sge;
	struct amr_mailbox *mb;
	int nsegs, i, rv, sgloff;
	bus_dmamap_t xfer;

	xfer = ac->ac_xfer_map;

	rv = bus_dmamap_load(amr->amr_dmat, xfer, data, size, NULL,
	    BUS_DMA_NOWAIT);
	if (rv != 0)
		return (rv);

	mb = &ac->ac_mbox;
	ac->ac_xfer_size = size;
	ac->ac_flags |= (out ? AC_XFER_OUT : AC_XFER_IN);
	sgloff = AMR_SGL_SIZE * ac->ac_ident;

	/* We don't need to use a scatter/gather list for just 1 segment. */
	nsegs = xfer->dm_nsegs;
	if (nsegs == 1) {
		mb->mb_nsgelem = 0;
		mb->mb_physaddr = htole32(xfer->dm_segs[0].ds_addr);
		ac->ac_flags |= AC_NOSGL;
	} else {
		mb->mb_nsgelem = nsegs;
		mb->mb_physaddr = htole32(amr->amr_sgls_paddr + sgloff);

		sge = (struct amr_sgentry *)((caddr_t)amr->amr_sgls + sgloff);
		for (i = 0; i < nsegs; i++, sge++) {
			sge->sge_addr = htole32(xfer->dm_segs[i].ds_addr);
			sge->sge_count = htole32(xfer->dm_segs[i].ds_len);
		}
	}

	bus_dmamap_sync(amr->amr_dmat, xfer, 0, ac->ac_xfer_size,
	    out ? BUS_DMASYNC_PREWRITE : BUS_DMASYNC_PREREAD);

	if ((ac->ac_flags & AC_NOSGL) == 0)
		bus_dmamap_sync(amr->amr_dmat, amr->amr_dmamap, sgloff,
		    AMR_SGL_SIZE, BUS_DMASYNC_PREWRITE);

	return (0);
}

/*
 * Unmap the specified CCB's data buffer.
 */
void
amr_ccb_unmap(struct amr_softc *amr, struct amr_ccb *ac)
{

	if ((ac->ac_flags & AC_NOSGL) == 0)
		bus_dmamap_sync(amr->amr_dmat, amr->amr_dmamap,
		    AMR_SGL_SIZE * ac->ac_ident, AMR_SGL_SIZE,
		    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(amr->amr_dmat, ac->ac_xfer_map, 0, ac->ac_xfer_size,
	    (ac->ac_flags & AC_XFER_IN) != 0 ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(amr->amr_dmat, ac->ac_xfer_map);
}

/*
 * Submit a command to the controller and poll on completion.  Return
 * non-zero on timeout or error.  Must be called with interrupts blocked.
 */
int
amr_ccb_poll(struct amr_softc *amr, struct amr_ccb *ac, int timo)
{
	int rv;

	if ((rv = (*amr->amr_submit)(amr, ac)) != 0)
		return (rv);

	for (timo *= 10; timo != 0; timo--) {
		amr_intr(amr);
		if ((ac->ac_flags & AC_COMPLETE) != 0)
			break;
		DELAY(100);
	}

	return (timo == 0 || ac->ac_status != 0 ? EIO : 0);
}

/*
 * Wait for the mailbox to become available.
 */
int
amr_mbox_wait(struct amr_softc *amr)
{
	int timo;

	for (timo = 10000; timo != 0; timo--) {
		if (amr->amr_mbox->mb_busy == 0)
			break;
		DELAY(100);
	}

#if 0
	if (timo != 0)
		printf("%s: controller wedged\n", amr->amr_dv.dv_xname);
#endif

	return (timo != 0 ? 0 : EIO);
}

/*
 * Tell the controller that the mailbox contains a valid command.  Must be
 * called with interrupts blocked.
 */
int
amr_quartz_submit(struct amr_softc *amr, struct amr_ccb *ac)
{
	u_int32_t v;

	v = amr_inl(amr, AMR_QREG_IDB);
	if ((v & (AMR_QIDB_SUBMIT | AMR_QIDB_ACK)) != 0)
		return (EBUSY);

	memcpy(amr->amr_mbox, &ac->ac_mbox, sizeof(ac->ac_mbox));

	ac->ac_flags |= AC_ACTIVE;
	amr_outl(amr, AMR_QREG_IDB, amr->amr_mbox_paddr | AMR_QIDB_SUBMIT);
	DELAY(10);
	return (0);
}

int
amr_std_submit(struct amr_softc *amr, struct amr_ccb *ac)
{

	if ((amr_inb(amr, AMR_SREG_MBOX_BUSY) & AMR_SMBOX_BUSY_FLAG) != 0)
		return (EBUSY);

	memcpy(amr->amr_mbox, &ac->ac_mbox, sizeof(ac->ac_mbox));

	ac->ac_flags |= AC_ACTIVE;
	amr_outb(amr, AMR_SREG_CMD, AMR_SCMD_POST);
	return (0);
}

/*
 * Claim any work that the controller has completed; acknowledge completion,
 * save details of the completion in (mbsave).  Must be called with
 * interrupts blocked.
 */
int
amr_quartz_get_work(struct amr_softc *amr, struct amr_mailbox *mbsave)
{
	u_int32_t v;

	if (amr_mbox_wait(amr))
		return (EBUSY);

	v = amr_inl(amr, AMR_QREG_IDB);
	if ((v & (AMR_QIDB_SUBMIT | AMR_QIDB_ACK)) != 0)
		return (EBUSY);

	/* Work waiting for us? */
	if (amr_inl(amr, AMR_QREG_ODB) != AMR_QODB_READY)
		return (-1);

	/* Save the mailbox, which contains a list of completed commands. */
	memcpy(mbsave, amr->amr_mbox, sizeof(*mbsave));

	/* Ack the interrupt and mailbox transfer. */
	amr_outl(amr, AMR_QREG_ODB, AMR_QODB_READY);
	amr_outl(amr, AMR_QREG_IDB, amr->amr_mbox_paddr | AMR_QIDB_ACK);
	DELAY(10);

#if 0
	/*
	 * This waits for the controller to notice that we've taken the
	 * command from it.  It's very inefficient, and we shouldn't do it,
	 * but if we remove this code, we stop completing commands under
	 * load.
	 *
	 * Peter J says we shouldn't do this.  The documentation says we
	 * should.  Who is right?
	 */
	while ((amr_inl(amr, AMR_QREG_IDB) & AMR_QIDB_ACK) != 0)
		;
#endif

	return (0);
}

int
amr_std_get_work(struct amr_softc *amr, struct amr_mailbox *mbsave)
{
	u_int8_t istat;

	if (amr_mbox_wait(amr))
		return (EBUSY);

	/* Puke if the mailbox is busy. */
	if ((amr_inb(amr, AMR_SREG_MBOX_BUSY) & AMR_SMBOX_BUSY_FLAG) != 0)
		return (-1);

	/* Check for valid interrupt status. */
	if (((istat = amr_inb(amr, AMR_SREG_INTR)) & AMR_SINTR_VALID) == 0)
		return (-1);

	/* Ack the interrupt. */
	amr_outb(amr, AMR_SREG_INTR, istat);

	/* Save mailbox, which contains a list of completed commands. */
	memcpy(mbsave, amr->amr_mbox, sizeof(*mbsave));

	/* Ack mailbox transfer. */
	amr_outb(amr, AMR_SREG_CMD, AMR_SCMD_ACKINTR);

	return (0);
}
