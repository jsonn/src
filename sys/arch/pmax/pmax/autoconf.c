/*	$NetBSD: autoconf.c,v 1.25.8.1 1997/11/09 20:21:07 mellon Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: autoconf.c 1.31 91/01/21
 *
 *	@(#)autoconf.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: autoconf.c,v 1.25.8.1 1997/11/09 20:21:07 mellon Exp $");

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <pmax/dev/device.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/turbochannel.h>

void dumpconf __P((void)); 	/* XXX */

void xconsinit __P((void));	/* XXX console-init continuation */

#if 0
/*
 * XXX system-dependent, should call through a pointer.
 * (spl0 should _NOT_ enable TC interrupts on a 3MIN.)
 *
 */
int spl0 __P((void));
#endif


/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold = 1;	/* if 1, still working on cold-start */
int	cpuspeed = 30;	/* approx # instr per usec. */
extern	int pmax_boardtype;
extern	tc_option_t tc_slot_info[TC_MAX_LOGICAL_SLOTS];


extern int cputype;	/* glue for new-style config */
int cputype;

extern int initcpu __P((void));		/*XXX*/
void configure_scsi __P((void));

void	findroot __P((struct device **, int *));

struct devnametobdevmaj pmax_nam2blk[] = {
	{ "rz",		21 },
#ifdef notyet
	{ "md",		XXX },
#endif
	{ NULL,		0 },
};


/*
 * The following is used by the NFS code to select boot method.
 * 0 -> RARP/SunRPC bootparamd,  1 -> bootp/dhcp.
 *
 * The pmax proms need bootp anyway, so just use bootp.
 */
extern int nfs_boot_rfc951;
int nfs_boot_rfc951 = 1;


/*
 * Determine mass storage and memory configuration for a machine.
 * Print cpu type, and then iterate over an array of devices
 * found on the baseboard or in turbochannel option slots.
 * Once devices are configured, enable interrupts, and probe
 * for attached scsi devices.
 */
void
configure()
{
	int s;

	/*
	 * Set CPU type for new-style config. 
	 * Should support Decstations with CPUs on daughterboards,
	 * where system-type (board type) and CPU type aren't
	 * necessarily the same.
	 * (On hold until someone donates an r4400 daughterboard).
	 */
	cputype = pmax_boardtype;		/*XXX*/


	/*
	 * Kick off autoconfiguration
	 */
	s = splhigh();
	if (config_rootfound("mainbus", "mainbus") == NULL)
	    panic("no mainbus found");

#if 0
	printf("looking for non-PROM console driver\n");
#endif

	xconsinit();	/* do kludged-up console init */

#ifdef DEBUG
	if (cputype == DS_3MIN)
/*FIXME*/	printf("switched to non-PROM console\n");
#endif

	initcpu();

#ifdef DEBUG
	printf("autconfiguration done, spl back to 0x%x\n", s);
#endif
	/*
	 * Configuration is finished,  turn on interrupts.
	 * This is just spl0(), except on the 3MIN, where TURBOChannel
	 * option cards interrupt at IPLs 0-2, and some dumb drivers like
	 * the cfb want to just disable interrupts.
	 */
	if (cputype != DS_3MIN)
		spl0();

	/*
	 * Probe SCSI bus using old-style pmax configuration table.
	 * We do not yet have machine-independent SCSI support or polled
	 * SCSI.
	 */
	printf("Beginning old-style SCSI device autoconfiguration\n");
	configure_scsi();

	cold = 0;
}

void
cpu_rootconf()
{
	struct device *booted_device;
	int booted_partition;

	findroot(&booted_device, &booted_partition);

	printf("boot device: %s\n",
	    booted_device ? booted_device->dv_xname : "<unknown>");

	setroot(booted_device, booted_partition, pmax_nam2blk);
}

u_long	bootdev = 0;		/* should be dev_t, but not until 32 bits */

/*
 * Attempt to find the device from which we were booted.
 */
void
findroot(devpp, partp)
	struct device **devpp;
	int *partp;
{
	int i, majdev, unit, part, controller;
	struct pmax_scsi_device *dp;
	const char *bootdv_name;

	/*
	 * Default to "not found".
	 */
	*devpp = NULL;
	*partp = 0;
	bootdv_name = NULL;

	if ((bootdev & B_MAGICMASK) != B_DEVMAGIC)
		return;

	majdev = B_TYPE(bootdev);
	for (i = 0; pmax_nam2blk[i].d_name != NULL; i++) {
		if (majdev == pmax_nam2blk[i].d_maj) {
			bootdv_name = pmax_nam2blk[i].d_name;
			break;
		}
	}

	if (bootdv_name == NULL) {
#if defined(DEBUG)
		printf("findroot(): no name2blk for boot device %d\n", majdev);
#endif
		return;
	}

	controller = B_CONTROLLER(bootdev);
	part = B_PARTITION(bootdev);
	unit = B_UNIT(bootdev);

	for (dp = scsi_dinit; dp->sd_driver != NULL; dp++) {
		if (dp->sd_alive && dp->sd_drive == unit &&
		    dp->sd_ctlr == controller &&
		    dp->sd_driver->d_name[0] == bootdv_name[0] &&
		    dp->sd_driver->d_name[1] == bootdv_name[1]) {
			*devpp = dp->sd_devp;
			*partp = part;
			return;
		}
	}
#if defined(DEBUG)
	printf("findroot(): no driver for boot device %s\n", bootdv_name);
#endif
}

/*
 * Look at the string 'cp' and decode the boot device.
 * Boot names can be something like 'rz(0,0,0)vmunix' or '5/rz0/vmunix'.
 */
void
makebootdev(cp)
	register char *cp;
{
	int majdev, unit, part, ctrl;

	if (*cp >= '0' && *cp <= '9') {
		/* XXX should be able to specify controller */
		if (cp[1] != '/' || cp[4] < '0' || cp[4] > '9')
			goto defdev;
		unit = cp[4] - '0';
		if (cp[5] >= 'a' && cp[5] <= 'h')
			part = cp[5] - 'a';
		else
			part = 0;
		cp += 2;
		for (majdev = 0; pmax_nam2blk[majdev].d_name != NULL;
		    majdev++) {
			if (cp[0] == pmax_nam2blk[majdev].d_name[0] &&
			    cp[1] == pmax_nam2blk[majdev].d_name[1]) {
				bootdev = MAKEBOOTDEV(
				    pmax_nam2blk[majdev].d_maj, 0, 0,
				    unit, part);
				return;
			}
		}
		goto defdev;
	}
	for (majdev = 0; pmax_nam2blk[majdev].d_name != NULL; majdev++)
		if (cp[0] == pmax_nam2blk[majdev].d_name[0] &&
		    cp[1] == pmax_nam2blk[majdev].d_name[1] &&
		    cp[2] == '(')
			goto fndmaj;
defdev:
	bootdev = B_DEVMAGIC;
	return;

fndmaj:
	majdev = pmax_nam2blk[majdev].d_maj;
	for (ctrl = 0, cp += 3; *cp >= '0' && *cp <= '9'; )
		ctrl = ctrl * 10 + *cp++ - '0';
	if (*cp == ',')
		cp++;
	for (unit = 0; *cp >= '0' && *cp <= '9'; )
		unit = unit * 10 + *cp++ - '0';
	if (*cp == ',')
		cp++;
	for (part = 0; *cp >= '0' && *cp <= '9'; )
		part = part * 10 + *cp++ - '0';
	if (*cp != ')')
		goto defdev;
	bootdev = MAKEBOOTDEV(majdev, 0, ctrl, unit, part);
}
