/* $NetBSD: autoconf.c,v 1.5.2.1 2007/09/03 14:27:05 yamt Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: autoconf.c,v 1.5.2.1 2007/09/03 14:27:05 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <machine/disklabel.h>
#include <machine/cpu.h>

#include <luna68k/luna68k/isr.h>

static struct device *find_dev_byname __P((const char *));

/*
 * Determine mass storage and memory configuration for a machine.
 */
void
cpu_configure()
{
	booted_device = NULL;	/* set by device drivers (if found) */

	(void)splhigh();
	isrinit();
	softintr_init();
	if (config_rootfound("mainbus", NULL) == NULL)
		panic("autoconfig failed, no root");

	spl0();
}

void
cpu_rootconf()
{
#if 1 /* XXX to be reworked with helps of 2nd stage loaders XXX */
	int i;
	const char *devname;
	char *cp;
	extern char bootarg[64];

	cp = bootarg;
	devname = "sd0";
	for (i = 0; i < sizeof(bootarg); i++) {
		if (*cp == '\0')
			break;
		if (*cp == 'E' && memcmp("ENADDR=", cp, 7) == 0) {
			devname = "le0";
			break;
		}
		cp++;
	}
	booted_device = find_dev_byname(devname);

#endif
	printf("boot device: %s\n",
		(booted_device) ? booted_device->dv_xname : "<unknown>");

	setroot(booted_device, 0); /* XXX partition 'a' XXX */
}

static struct device *
find_dev_byname(name)
	const char *name;
{
	struct device *dv;

	for (dv = alldevs.tqh_first; dv != NULL; dv = dv->dv_list.tqe_next) {
		if (!strcmp(dv->dv_xname, name)) {
			return dv;
		}
	}
	return NULL;
}
