/* $NetBSD: api_up1000.c,v 1.8.2.2 2002/10/10 18:30:41 jdolecek Exp $ */

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
/*
 * Additional Copyright (c) 1997 by Matthew Jacob for NASA/Ames Research Center
 */

#include "opt_kgdb.h"

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: api_up1000.c,v 1.8.2.2 2002/10/10 18:30:41 jdolecek Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>
#include <sys/conf.h>
#include <dev/cons.h>

#include <machine/alpha.h>
#include <machine/rpb.h>
#include <machine/autoconf.h>
#include <machine/cpuconf.h>
#include <machine/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/pci/irongatereg.h>
#include <alpha/pci/irongatevar.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/ata/atavar.h>
#include <dev/ata/wdvar.h>

#include "pckbd.h"

#ifndef CONSPEED
#define CONSPEED TTYDEF_SPEED
#endif
static int comcnrate = CONSPEED;

#define	DPRINTF(x)	if (bootdev_debug) printf x

void api_up1000_init __P((void));
static void api_up1000_cons_init __P((void));
static void api_up1000_device_register __P((struct device *, void *));

#ifdef KGDB
#include <machine/db_machdep.h>

static const char *kgdb_devlist[] = {
	"com",
	NULL,
};
#endif /* KGDB */

void
api_up1000_init()
{

	platform.family = "Alpha Processor, Inc. UP1000";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		/* XXX Don't know the system variations, yet. */
		platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "irongate";
	platform.cons_init = api_up1000_cons_init;
	platform.device_register = api_up1000_device_register;
}

static void
api_up1000_cons_init()
{
	struct ctb *ctb;
	struct irongate_config *icp;
	extern struct irongate_config irongate_configuration;

	icp = &irongate_configuration;
	irongate_init(icp, 0);

	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);

	switch (ctb->ctb_term_type) {
	case CTB_PRINTERPORT: 
		/* serial console ... */
		/* XXX */
		{
			/*
			 * Delay to allow PROM putchars to complete.
			 * FIFO depth * character time,
			 * character time = (1000000 / (defaultrate / 10))
			 */
			DELAY(160000000 / comcnrate);

			if(comcnattach(&icp->ic_iot, 0x3f8, comcnrate,
			    COM_FREQ,
			    (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8))
				panic("can't init serial console");

			break;
		}

	case CTB_GRAPHICS:
#if NPCKBD > 0
		/* display console ... */
		/* XXX */
		(void) pckbc_cnattach(&icp->ic_iot, IO_KBD, KBCMDP,
		    PCKBC_KBD_SLOT);

		if (CTB_TURBOSLOT_TYPE(ctb->ctb_turboslot) ==
		    CTB_TURBOSLOT_TYPE_ISA)
			isa_display_console(&icp->ic_iot, &icp->ic_memt);
		else
			pci_display_console(&icp->ic_iot, &icp->ic_memt,
			    &icp->ic_pc, CTB_TURBOSLOT_BUS(ctb->ctb_turboslot),
			    CTB_TURBOSLOT_SLOT(ctb->ctb_turboslot), 0);
#else
		panic("not configured to use display && keyboard console");
#endif
		break;

	default:
		printf("ctb->ctb_term_type = 0x%lx\n", ctb->ctb_term_type);
		printf("ctb->ctb_turboslot = 0x%lx\n", ctb->ctb_turboslot);

		panic("consinit: unknown console type %ld",
		    ctb->ctb_term_type);
	}
#ifdef KGDB
	/* Attach the KGDB device. */
	alpha_kgdb_init(kgdb_devlist, &icp->ic_iot);
#endif /* KGDB */
}

static void
api_up1000_device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	static int found, initted, scsiboot, ideboot, netboot;
	static struct device *pcidev, *scsipidev;
	struct bootdev_data *b = bootdev_data;
	struct device *parent = dev->dv_parent;
	struct cfdata *cf = dev->dv_cfdata;
	const char *name = cf->cf_name;

	if (found)
		return;

	if (!initted) {
		scsiboot = (strcmp(b->protocol, "SCSI") == 0);
		netboot = (strcmp(b->protocol, "BOOTP") == 0) ||
		    (strcmp(b->protocol, "MOP") == 0);
		/*
		 * Add an extra check to boot from ide drives:
		 * Newer SRM firmware use the protocol identifier IDE,
		 * older SRM firmware use the protocol identifier SCSI.
		 */
		ideboot = (strcmp(b->protocol, "IDE") == 0);
		DPRINTF(("\nscsiboot = %d, ideboot = %d, netboot = %d\n",
		    scsiboot, ideboot, netboot));
		initted = 1;
	}

	if (pcidev == NULL) {
		if (strcmp(name, "pci"))
			return;
		else {
			struct pcibus_attach_args *pba = aux;

			if ((b->slot / 1000) != pba->pba_bus)
				return;
	
			pcidev = dev;
			DPRINTF(("\npcidev = %s\n", pcidev->dv_xname));
			return;
		}
	}

	if ((ideboot || scsiboot) && (scsipidev == NULL)) {
		if (parent != pcidev)
			return;
		else {
			struct pci_attach_args *pa = aux;

			if (b->slot % 1000 / 100 != pa->pa_function)
				return;
			if (b->slot % 100 != pa->pa_device)
				return;
	
			scsipidev = dev;
			DPRINTF(("\nscsipidev = %s\n", scsipidev->dv_xname));
			return;
		}
	}

	if ((ideboot || scsiboot) &&
	    (!strcmp(name, "sd") ||
	     !strcmp(name, "st") ||
	     !strcmp(name, "cd"))) {
		struct scsipibus_attach_args *sa = aux;

		if (parent->dv_parent != scsipidev)
			return;

		if ((sa->sa_periph->periph_channel->chan_bustype->bustype_type
		     == SCSIPI_BUSTYPE_SCSI ||
		     sa->sa_periph->periph_channel->chan_bustype->bustype_type
		     == SCSIPI_BUSTYPE_ATAPI)
		    && b->unit / 100 != sa->sa_periph->periph_target)
			return;

		/* XXX LUN! */

		switch (b->boot_dev_type) {
		case 0:
			if (strcmp(name, "sd") &&
			    strcmp(name, "cd"))
				return;
			break;
		case 1:
			if (strcmp(name, "st"))
				return;
			break;
		default:
			return;
		}

		/* we've found it! */
		booted_device = dev;
		DPRINTF(("\nbooted_device = %s\n", booted_device->dv_xname));
		found = 1;
	}

	/*
	 * Support to boot from IDE drives.
	 */
	if ((ideboot || scsiboot) && !strcmp(name, "wd")) {
		struct ata_device *adev = aux;
		if ((strncmp("pciide", parent->dv_xname, 6) != 0)) {
			return;
		} else {
			if (parent != scsipidev)
				return;
		}
		DPRINTF(("\natapi info: drive %d, channel %d\n",
		    adev->adev_drv_data->drive, adev->adev_channel));
		DPRINTF(("bootdev info: unit: %d, channel: %d\n",
		    b->unit, b->channel));
		if (b->unit != adev->adev_drv_data->drive ||
		    b->channel != adev->adev_channel)
			return;

		/* we've found it! */
		booted_device = dev;
		DPRINTF(("booted_device = %s\n", booted_device->dv_xname));
		found = 1;
	}

	if (netboot) {
		if (parent != pcidev)
			return;
		else {
			struct pci_attach_args *pa = aux;

			if ((b->slot % 1000) != pa->pa_device)
				return;

			/* XXX function? */
	
			booted_device = dev;
			DPRINTF(("\nbooted_device = %s\n",
			    booted_device->dv_xname));
			found = 1;
			return;
		}
	}
}
