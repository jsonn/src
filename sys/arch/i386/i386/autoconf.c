/*	$NetBSD: autoconf.c,v 1.22.2.1 1997/01/14 21:25:22 thorpej Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)autoconf.c	7.1 (Berkeley) 5/9/91
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time and initializes the vba 
 * device tables and the memory controller monitoring.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <machine/pte.h>
#include <machine/cpu.h>

void findroot __P((struct device **, int *));

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
extern int	cold;		/* cold start flag initialized in locore.s */

struct devnametobdevmaj i386_nam2blk[] = {
	{ "wd",		0 },
	{ "sd",		4 },
	{ "cd",		6 },
	{ "mcd",	7 },
	{ "fd",		2 },
	{ "md",		17 },
	{ NULL,		0 },
};

/*
 * Determine i/o configuration for a machine.
 */
void
configure()
{
	struct device *booted_device;
	int booted_partition;

	startrtclock();

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("configure: mainbus not configured");

	printf("biomask %x netmask %x ttymask %x\n",
	    (u_short)imask[IPL_BIO], (u_short)imask[IPL_NET],
	    (u_short)imask[IPL_TTY]);

	spl0();

	findroot(&booted_device, &booted_partition);

	printf("boot device: %s\n",
	    booted_device ? booted_device->dv_xname : "<unknown>");

	setroot(booted_device, booted_partition, i386_nam2blk);

	/*
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */
	swapconf();
	dumpconf();
	cold = 0;
}

u_long	bootdev = 0;		/* should be dev_t, but not until 32 bits */

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
void
findroot(devpp, partp)
	struct device **devpp;
	int *partp;
{
	int i, majdev, unit, part;
	struct device *dv;
	char buf[32];

	/*
	 * Default to "not found."
	 */
	*devpp = NULL;
	*partp = 0;

#if 0
	printf("howto %x bootdev %x ", boothowto, bootdev);
#endif

	if ((bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC)
		return;

	majdev = (bootdev >> B_TYPESHIFT) & B_TYPEMASK;
	for (i = 0; i386_nam2blk[i].d_name != NULL; i++)
		if (majdev == i386_nam2blk[i].d_maj)
			break;
	if (i386_nam2blk[i].d_name == NULL)
		return;

	part = (bootdev >> B_PARTITIONSHIFT) & B_PARTITIONMASK;
	unit = (bootdev >> B_UNITSHIFT) & B_UNITMASK;

	sprintf(buf, "%s%d", i386_nam2blk[i].d_name, unit);
	for (dv = alldevs.tqh_first; dv != NULL;
	    dv = dv->dv_list.tqe_next) {
		if (strcmp(buf, dv->dv_xname) == 0) {
			*devpp = dv;
			*partp = part;
			return;
		}
	}
}
