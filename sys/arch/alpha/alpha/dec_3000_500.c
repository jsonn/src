/* $NetBSD: dec_3000_500.c,v 1.15.4.3 1997/09/29 07:19:36 thorpej Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: dec_3000_500.c,v 1.15.4.3 1997/09/29 07:19:36 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/rpb.h>

#include <machine/autoconf.h>
#include <machine/conf.h>

#include <dev/tc/tcvar.h>

#include <alpha/tc/tcdsvar.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

void dec_3000_500_init __P((void));
static void dec_3000_500_device_register __P((struct device *, void *));

void
dec_3000_500_init()
{
	const char *sp = "DEC 3000/400 (\"Sandpiper\")";
	const char *sf = "DEC 3000/500 (\"Flamingo\")";

	platform.family = "DEC 3000/500 (\"Flamingo\")";
	switch (hwrpb->rpb_variation & SV_ST_MASK) {
	case SV_ST_SANDPIPER:
		platform.model = sp;
		break;

	case SV_ST_FLAMINGO:
		platform.model = sf;
		break;

	case SV_ST_HOTPINK:
		platform.model = "DEC 3000/500X (\"Hot Pink\")";
		break;

	case SV_ST_FLAMINGOPLUS:
	case SV_ST_ULTRA:
		platform.model = "DEC 3000/800 (\"Flamingo+\")";
		break;

	case SV_ST_SANDPLUS:
		platform.model = "DEC 3000/600 (\"Sandpiper+\")";
		break;

	case SV_ST_SANDPIPER45:
		platform.model = "DEC 3000/700 (\"Sandpiper45\")";
		break;

	case SV_ST_FLAMINGO45:
		platform.model = "DEC 3000/900 (\"Flamingo45\")";
		break;

	case SV_ST_RESERVED: /* this is how things used to be done */
		if (hwrpb->rpb_variation & SV_GRAPHICS)
			platform.model = sf;
		else
			platform.model = sp;
		break;
	default:
	{
		/* string is 24 bytes plus 64 bit hex number (16 byte) */
		static char s[42];
		sprintf(s, "unknown model variation %lx",
		    hwrpb->rpb_variation & SV_ST_MASK);
		platform.model = (const char *) s;
		break;
	}
	}
	platform.iobus = "tcasic";
	platform.device_register = dec_3000_500_device_register;
}

static void
dec_3000_500_device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	static int found, initted, scsiboot, netboot;
	static struct device *scsidev;
	struct bootdev_data *b = bootdev_data;
	struct device *parent = dev->dv_parent;
	struct cfdata *cf = dev->dv_cfdata;
	struct cfdriver *cd = cf->cf_driver;

	if (found)
		return;

	if (!initted) {
		scsiboot = (strcmp(b->protocol, "SCSI") == 0);
		netboot = (strcmp(b->protocol, "BOOTP") == 0);
#if 0
		printf("scsiboot = %d, netboot = %d\n", scsiboot, netboot);
#endif
		initted =1;
	}

	if (scsiboot && (strcmp(cd->cd_name, "asc") == 0)) {
		if (b->slot == 6 &&
		    strcmp(parent->dv_cfdata->cf_driver->cd_name, "tcds")
		      == 0) {
			struct tcdsdev_attach_args *tcdsdev = aux;

			if (tcdsdev->tcdsda_slot == b->channel) {
				scsidev = dev;
#if 0
				printf("\nscsidev = %s\n", dev->dv_xname);
#endif
			}
		}
	}

	if (scsiboot &&
	    (strcmp(cd->cd_name, "sd") == 0 ||
	     strcmp(cd->cd_name, "st") == 0 ||
	     strcmp(cd->cd_name, "cd") == 0)) {
		struct scsipibus_attach_args *sa = aux;

		if (scsidev == NULL)
			return;

		if (parent->dv_parent != scsidev)
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
#if 0
		printf("\nbooted_device = %s\n", booted_device->dv_xname);
#endif
		found = 1;
	}

	if (netboot) {
                if (b->slot == 7 && strcmp(cd->cd_name, "le") == 0 &&
		    strcmp(parent->dv_cfdata->cf_driver->cd_name, "ioasic")
		     == 0) {
			/*
			 * no need to check ioasic_attach_args, since only
			 * one le on ioasic.
			 */

			booted_device = dev;
#if 0
			printf("\nbooted_device = %s\n", booted_device->dv_xname);
#endif
			found = 1;
			return;
		}

		/*
		 * XXX GENERIC SUPPORT FOR TC NETWORK BOARDS
		 */
        }
}
