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
 *	from: @(#)swapgeneric.c	5.5 (Berkeley) 5/9/91
 *	$Id: swapgeneric.c,v 1.4.6.1 1994/07/29 01:14:34 cgd Exp $
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/disklabel.h>

#include <machine/pte.h>

#include <i386/isa/isa_device.h>

#include "wd.h"
#include "fd.h"
#include "sd.h"
#include "cd.h"
#include "mcd.h"

/*
 * Generic configuration;  all in one
 */
dev_t	rootdev = NODEV;
dev_t	argdev = NODEV;
dev_t	dumpdev = NODEV;
int	nswap;
struct	swdevt swdevt[] = {
	{ NODEV,	1,	0 },
	{ 0,		0,	0 },
};
long	dumplo;
int	dmmin, dmmax, dmtext;

#if NWD > 0
extern	struct cfdriver wdcd;
#endif
#if NFD > 0
extern	struct cfdriver fdcd;
#endif
#if NSD > 0
extern	struct cfdriver sdcd;
#endif
#if NCD > 0
extern	struct cfdriver cdcd;
#endif
#if NMCD > 0
extern	struct cfdriver mcdcd;
#endif

struct	genericconf {
	struct cfdriver *gc_driver;
	char *gc_name;
	dev_t gc_major;
} genericconf[] = {
#if NWD > 0
	{ &wdcd,  "wd",  0 },
#endif
#if NSD > 0
	{ &sdcd,  "sd",  4 },
#endif
#if NCD > 0
	{ &cdcd,  "cd",  6 },
#endif
#if NMCD > 0
	{ &mcdcd, "mcd", 7 },
#endif
#if NFD > 0
	{ &fdcd,  "fd",  2 },
#endif
	{ 0 }
};

extern int ffs_mountroot();
int (*mountroot)() = ffs_mountroot;

setconf()
{
	register struct genericconf *gc;
	int unit, swaponroot = 0;
	struct isa_device *id;

	if (rootdev != NODEV)
		goto doswap;

	if (genericconf[0].gc_driver == 0)
		goto verybad;

	if (boothowto & RB_ASKNAME) {
		char name[128];
retry:
		printf("root device? ");
		gets(name);
		for (gc = genericconf; gc->gc_driver; gc++)
			if (gc->gc_name[0] == name[0] &&
			    gc->gc_name[1] == name[1])
				goto gotit;
		goto bad;
gotit:
		if (name[3] == '*') {
			name[3] = name[4];
			swaponroot++;
		}
		if (name[2] >= '0' && name[2] <= '7' && name[3] == 0) {
			unit = name[2] - '0';
			goto found;
		}
		printf("bad/missing unit number\n");
bad:
		printf("use:\n");	
		for (gc = genericconf; gc->gc_driver; gc++)
			printf("\t%s%%d\n", gc->gc_name);
		goto retry;
	}
	unit = 0;
	for (gc = genericconf; gc->gc_driver; gc++) {
		for (id = isa_devtab; id->id_driver != 0; id++) {
			if (id->id_state != FSTATE_FOUND)
				continue;
			if (id->id_unit == 0 &&
			    id->id_driver == gc->gc_driver &&
			    id->id_driver->cd_ndevs >= 1 &&
			    id->id_driver->cd_devs[0]) {
				printf("root on %s0\n", gc->gc_name);
				goto found;
			}
		}
	}
verybad:
	printf("no suitable root -- hit any key to reboot\n");
	printf("\n>");						/* XXX */						/* XXX */						/* XXX */
	cngetc();
	cpu_reset();
	for (;;) ;

found:
	rootdev = makedev(gc->gc_major, unit * MAXPARTITIONS);
doswap:
	swdevt[0].sw_dev = argdev = dumpdev =
	    makedev(major(rootdev), minor(rootdev) + 1);
	/* swap size and dumplo set during autoconfigure */
	if (swaponroot)
		rootdev = dumpdev;
}

gets(cp)
	char *cp;
{
	register char *lp;
	register c;

	lp = cp;
	for (;;) {
		printf("%c", c = cngetc()&0177);
		switch (c) {
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
		case '\177':
			if (lp > cp) {
				printf(" \b");
				lp--;
			}
			continue;
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;
		case '@':
		case 'u'&037:
			lp = cp;
			printf("%c", '\n');
			continue;
		default:
			*lp++ = c;
		}
	}
}
