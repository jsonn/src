/*	$NetBSD: autoconf.c,v 1.12.2.1 1997/01/14 21:25:37 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 * 	The Regents of the University of California. All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: autoconf.c 1.36 92/12/20$
 * 
 *	@(#)autoconf.c  8.2 (Berkeley) 1/12/94
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
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <machine/vmparam.h>
#include <machine/autoconf.h>
#include <machine/disklabel.h>
#include <machine/cpu.h>
#include <machine/pte.h>

#include <mvme68k/mvme68k/isr.h>

struct device *booted_device;	/* boot device */

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold;			/* if 1, still working on cold-start */

void mainbus_attach __P((struct device *, struct device *, void *));
int  mainbus_match __P((struct device *, void *, void *));
int  mainbus_print __P((void *, const char *));

struct mainbus_softc {
	struct device sc_dev;
};

struct cfattach mainbus_ca = {
	sizeof(struct mainbus_softc), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL, 0
};

#ifdef MVME147
static	char *mainbusdevs_147[] = {
	"pcc", NULL
};
#endif

#ifdef MVME162
static	char *mainbusdevs_162[] = {
	"mc", "flash", "sram", NULL
};
#endif

#if defined(MVME167) || defined(MVME177)
static	char *mainbusdevs_1x7[] = {	/* includes 166, 177 */
	"pcctwo", "sram", NULL
};
#endif

int
mainbus_match(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	static int mainbus_matched;

	if (mainbus_matched)
		return (0);

	return ((mainbus_matched = 1));
}

void
mainbus_attach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	char **devices;
	int i;

	printf("\n");

	/*
	 * Attach children appropriate for this CPU.
	 */
	switch (machineid) {
#ifdef MVME147
	case MVME_147:
		devices = mainbusdevs_147;
		break;
#endif

#ifdef MVME162
	case MVME_162:
		devices = mainbusdevs_162;
		break;
#endif

#if defined(MVME167) || defined(MVME177)
	case MVME_166:
	case MVME_167:
	case MVME_177:
		devices = mainbusdevs_1x7;
		break;
#endif

	default:
		panic("mainbus_attach: impossible CPU type");
	}

	for (i = 0; devices[i] != NULL; ++i)
		(void)config_found(self, devices[i], mainbus_print);
}

int
mainbus_print(aux, cp)
	void *aux;
	const char *cp;
{
	char *devname = aux;

	if (cp)
		printf("%s at %s", devname, cp);

	return (UNCONF);
}

struct devnametobdevmaj mvme68k_nam2blk[] = {
	{ "sd",		4 },
	{ "st",		7 },
	{ "cd",		8 },
	{ "md",		9 },
	{ NULL,		0 },
};

/*
 * Determine mass storage and memory configuration for a machine.
 */
configure()
{

	booted_device = NULL;	/* set by device drivers (if found) */

	init_sir();
	isrinit();

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("autoconfig failed, no root");

	printf("boot device: %s\n",
		(booted_device) ? booted_device->dv_xname : "<unknown>");

	setroot(booted_device, 0, mvme68k_nam2blk);

	swapconf();
	dumpconf();
	cold = 0;
}

/*
 * find a device matching "name" and unit number
 */
struct device *
getdevunit(name, unit)
	char *name;
	int unit;
{
	struct device *dev = alldevs.tqh_first;
	char num[10], fullname[16];
	int lunit;

	/* compute length of name and decimal expansion of unit number */
	sprintf(num, "%d", unit);
	lunit = strlen(num);
	if (strlen(name) + lunit >= sizeof(fullname) - 1)
		panic("config_attach: device name too long");

	strcpy(fullname, name);
	strcat(fullname, num);

	while (strcmp(dev->dv_xname, fullname) != 0) {
		if ((dev = dev->dv_list.tqe_next) == NULL)
			return NULL;
	}
	return dev;
}
