/*	$NetBSD: autoconf.c,v 1.39.2.1 1997/03/02 16:17:36 mrg Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *
 *	@(#)autoconf.c	8.4 (Berkeley) 10/1/93
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/disk.h>

#include <dev/cons.h>

#include <machine/autoconf.h>
#include <machine/adbsys.h>
#include <machine/viareg.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include "ether.h"

struct device	*booted_device;
int		booted_partition;

static void findbootdev __P((void));
static int target_to_unit __P((u_long, u_long, u_long));

struct devnametobdevmaj mac68k_nam2blk[] = {
	{ "sd",         4 },
	{ "cd",         6 },
	{ "md",		13 },
	{ NULL,		0 },
};

/*
 * configure:
 * called at boot time, configure all devices on the system
 */
void
configure()
{
	extern int	cold;

	VIA_initialize();	/* Init VIA hardware */
	mrg_init();		/* Init Mac ROM Glue */
	startrtclock();		/* start before adb_init() */
	adb_init();		/* ADB device subsystem & driver */

	if (config_rootfound("mainbus", "mainbus") == NULL)
		panic("No mainbus found!");

	findbootdev();

	printf("boot device: %s\n",
	    booted_device ? booted_device->dv_xname : "<unknown>");

	setroot(booted_device, booted_partition, mac68k_nam2blk);
	dumpconf();
	cold = 0;
}

/*
 * Yanked from i386/i386/autoconf.c (and tweaked a bit)
 */

u_long	bootdev;		/* XXX should be dev_t */

static void
findbootdev()
{
	struct device *dv;
	int major, unit, i;
	char buf[32];

	booted_device = NULL;
	booted_partition = 0;	/* Assume root is on partition a */

	major = B_TYPE(bootdev);
	for (i = 0; mac68k_nam2blk[i].d_name != NULL; i++)
		if (major == mac68k_nam2blk[i].d_maj)
			break;
	if (mac68k_nam2blk[i].d_name == NULL)
		return;

	unit = B_UNIT(bootdev);

	bootdev &= ~(B_UNITMASK << B_UNITSHIFT);
	unit = target_to_unit(-1, unit, 0);
	bootdev |= (unit << B_UNITSHIFT);

	sprintf(buf, "%s%d", mac68k_nam2blk[i].d_name, unit);
	for (dv = alldevs.tqh_first; dv != NULL;
	    dv = dv->dv_list.tqe_next) {
		if (strcmp(buf, dv->dv_xname) == 0) {
			booted_device = dv;
			return;
		}
	}
}

/*
 * Map a SCSI bus, target, lun to a device number.
 * This could be tape, disk, CD.  The calling routine, though,
 * assumes DISK.  It would be nice to allow CD, too...
 */
static int
target_to_unit(bus, target, lun)
	u_long bus, target, lun;
{
	struct scsibus_softc	*scsi;
	struct scsi_link	*sc_link;
	struct device		*sc_dev;
extern	struct cfdriver		scsibus_cd;

	if (target < 0 || target > 7 || lun < 0 || lun > 7) {
		printf("scsi target to unit, target (%ld) or lun (%ld)"
			" out of range.\n", target, lun);
		return -1;
	}

	if (bus == -1) {
		for (bus = 0 ; bus < scsibus_cd.cd_ndevs ; bus++) {
			if (scsibus_cd.cd_devs[bus]) {
				scsi = (struct scsibus_softc *)
						scsibus_cd.cd_devs[bus];
				if (scsi->sc_link[target][lun]) {
					sc_link = scsi->sc_link[target][lun];
					sc_dev = (struct device *)
							sc_link->device_softc;
					return sc_dev->dv_unit;
				}
			}
		}
		return -1;
	}
	if (bus < 0 || bus >= scsibus_cd.cd_ndevs) {
		printf("scsi target to unit, bus (%ld) out of range.\n", bus);
		return -1;
	}
	if (scsibus_cd.cd_devs[bus]) {
		scsi = (struct scsibus_softc *) scsibus_cd.cd_devs[bus];
		if (scsi->sc_link[target][lun]) {
			sc_link = scsi->sc_link[target][lun];
			sc_dev = (struct device *) sc_link->device_softc;
			return sc_dev->dv_unit;
		}
	}
	return -1;
}
