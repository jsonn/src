/* $NetBSD: dec_6600.c,v 1.1 1999/06/29 07:08:53 ross Exp $ */

/*
 * Copyright (c) 1995, 1996, 1997 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: dec_6600.c,v 1.1 1999/06/29 07:08:53 ross Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>
#include <dev/cons.h>

#include <machine/rpb.h>
#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <dev/isa/isavar.h>
#include <dev/isa/pckbcvar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/pci/tsreg.h>
#include <alpha/pci/tsvar.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include "pckbd.h"

#ifndef CONSPEED
#define CONSPEED TTYDEF_SPEED
#endif

#define	DR_VERBOSE(f) while (0)

static int comcnrate __attribute__((unused)) = CONSPEED;

void dec_6600_init __P((void));
static void dec_6600_cons_init __P((void));
static void dec_6600_device_register __P((struct device *, void *));

void
dec_6600_init()
{

	platform.family = "6600";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		/* XXX Don't know the system variations, yet. */
		platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "tsc";
	platform.cons_init = dec_6600_cons_init;
	platform.device_register = dec_6600_device_register;
	STQP(TS_C_DIM0) = 0UL;
	STQP(TS_C_DIM1) = 0UL;
}

static void
dec_6600_cons_init()
{
	struct ctb *ctb;
	u_int64_t ctbslot;
	struct tsp_config *tsp;

	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);
	ctbslot = ctb->ctb_turboslot;

	tsp = tsp_init(0, 0);

	switch (ctb->ctb_term_type) {
	case 2: 
		/* serial console ... */
		assert(CTB_TURBOSLOT_HOSE(ctbslot) == 0);
		/* XXX */
		{
			/*
			 * Delay to allow PROM putchars to complete.
			 * FIFO depth * character time,
			 * character time = (1000000 / (defaultrate / 10))
			 */
			DELAY(160000000 / comcnrate);

			if(comcnattach(&tsp->pc_iot, 0x3f8, comcnrate,
			    COM_FREQ,
			    (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8))
				panic("can't init serial console");

			break;
		}

	case 3:
#if NPCKBD > 0
		/* display console ... */
		/* XXX */
		(void) pckbc_cnattach(&tsp->pc_iot, PCKBC_KBD_SLOT);

		if (CTB_TURBOSLOT_TYPE(ctbslot) ==
		    CTB_TURBOSLOT_TYPE_ISA)
			isa_display_console(&tsp->pc_iot, &tsp->pc_memt);
		else {
			/* The display PCI might be different */
			tsp = tsp_init(0, CTB_TURBOSLOT_HOSE(ctbslot));
			pci_display_console(&tsp->pc_iot, &tsp->pc_memt,
			    &tsp->pc_pc, CTB_TURBOSLOT_BUS(ctbslot),
			    CTB_TURBOSLOT_SLOT(ctbslot), 0);
		}
#else
		panic("not configured to use display && keyboard console");
#endif
		break;

	default:
		printf("ctb_term_type = 0x%lx ctb_turboslot = 0x%lx"
		    " hose = %ld\n", ctb->ctb_term_type, ctbslot,
		    CTB_TURBOSLOT_HOSE(ctbslot));

		panic("consinit: unknown console type %ld\n",
		    ctb->ctb_term_type);
	}
}

static void
dec_6600_device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	static int found, initted, scsiboot, ideboot, netboot;
	static struct device *primarydev, *pcidev, *scsipidev;
	struct bootdev_data *b = bootdev_data;
	struct device *parent = dev->dv_parent;
	struct cfdata *cf = dev->dv_cfdata;
	struct cfdriver *cd = cf->cf_driver;

	if (found)
		return;

	if (!initted) {
		scsiboot = (strcmp(b->protocol, "SCSI") == 0);
		netboot = (strcmp(b->protocol, "BOOTP") == 0);
		/*
		 * Add an extra check to boot from ide drives:
		 * Newer SRM firmware use the protocol identifier IDE,
		 * older SRM firmware use the protocol identifier SCSI.
		 */
		ideboot = (strcmp(b->protocol, "IDE") == 0);
		DR_VERBOSE(printf("scsiboot = %d, ideboot = %d, netboot = %d\n",
		    scsiboot, ideboot, netboot));
		initted = 1;
	}
	if (primarydev == NULL) {
		if (strcmp(cd->cd_name, "tsp"))
			return;
		else {
			struct tsp_attach_args *tsp = aux;

			if (b->bus != tsp->tsp_slot)
				return;
			primarydev = dev;
			DR_VERBOSE(printf("\nprimarydev = %s\n",
			    primarydev->dv_xname));
			return;
		}
	}
	if (pcidev == NULL) {
		if (parent != primarydev)
			return;
		if (strcmp(cd->cd_name, "pci"))
			return;
		else {
			struct pcibus_attach_args *pba = aux;

			if ((b->slot / 1000) != pba->pba_bus)
				return;
	
			pcidev = dev;
			DR_VERBOSE(printf("\npcidev = %s\n", pcidev->dv_xname));
			return;
		}
	}
	if ((ideboot || scsiboot) && scsipidev == NULL) {
		if (parent != pcidev)
			return;
		else {
			struct pci_attach_args *pa = aux;

			if (b->slot % 1000 / 100 != pa->pa_function)
				return;
			if (b->slot % 100 != pa->pa_device)
				return;
	
			scsipidev = dev;
			DR_VERBOSE(printf("\nscsipidev = %s\n",
			    scsipidev->dv_xname));
			return;
		}
	}
	if ((scsiboot || ideboot) &&
	    (!strcmp(cd->cd_name, "sd") ||
	     !strcmp(cd->cd_name, "st") ||
	     !strcmp(cd->cd_name, "cd"))) {
		struct scsipibus_attach_args *sa = aux;

		if (parent->dv_parent != scsipidev)
			return;

		if (b->unit / 100 != sa->sa_sc_link->scsipi_scsi.target)
			return;

		/* XXX LUN! */

		switch (b->boot_dev_type) {
		case 0:
			if (strcmp(cd->cd_name, "sd") &&
			    strcmp(cd->cd_name, "cd"))
				return;
			break;
		case 1:
			if (strcmp(cd->cd_name, "st"))
				return;
			break;
		default:
			return;
		}

		/* we've found it! */
		booted_device = dev;
		DR_VERBOSE(printf("\nbooted_device = %s\n",
		    booted_device->dv_xname));
		found = 1;
	}

	/*
	 * Support to boot from IDE drives.
	 * Should work with all SRM firmware versions, but is at the
	 * moment limited to hard disks. Support for IDE CD ROM is
	 * included above, under the theory that it will always be
	 * via the pci ide controller.
	 */
	if ((ideboot || scsiboot) && !strcmp(cd->cd_name, "wd")) {
		if ((strncmp("pciide", parent->dv_xname, 6) != 0)) {
			return;
		} else {
			if (parent != scsipidev)
				return;
		}

		/* we've found it! */
		booted_device = dev;
		DR_VERBOSE(printf("\nbooted_device = %s\n",
		    booted_device->dv_xname));
		found = 1;
	}
	if (netboot) {
		if (parent != pcidev)
			return;
		else {
			struct pci_attach_args *pa = aux;

			if (b->slot % 1000 / 100 != pa->pa_function)
				return;
			if ((b->slot % 100) != pa->pa_device)
				return;
	
			booted_device = dev;
			DR_VERBOSE(printf("\nbooted_device = %s\n",
			    booted_device->dv_xname));
			found = 1;
			return;
		}
	}
}
