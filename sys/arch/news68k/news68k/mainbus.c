/*	$NetBSD: mainbus.c,v 1.8.2.1 2004/08/03 10:38:23 skrll Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * DECstation port: Jonathan Stone
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mainbus.c,v 1.8.2.1 2004/08/03 10:38:23 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <news68k/news68k/machid.h>

struct mainbus_softc {
	struct device sc_dev;
};

/* Definition of the mainbus driver. */
static int  mainbus_match(struct device *, struct cfdata *, void *);
static void mainbus_attach(struct device *, struct device *, void *);
static int  mainbus_search(struct device *, struct cfdata *, void *);
static int  mainbus_print(void *, const char *);

CFATTACH_DECL(mainbus, sizeof(struct mainbus_softc),
    mainbus_match, mainbus_attach, NULL, NULL);

static int mainbus_found;

static int
mainbus_match(parent, cfdata, aux)
	struct device *parent;
	struct cfdata *cfdata;
	void *aux;
{

	if (mainbus_found)
		return 0;

	return 1;
}

static void
mainbus_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct mainbus_attach_args ma;

	mainbus_found = 1;
	printf("\n");

	config_search(mainbus_search, self, &ma);
}

static int
mainbus_search(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;

	ma->ma_name = cf->cf_name;
	ma->ma_systype = cf->cf_systype;

	if (config_match(parent, cf, ma) > 0)
		config_attach(parent, cf, ma, mainbus_print);

	return 0;
}

static int
mainbus_print(aux, cp)
	void *aux;
	const char *cp;
{
	struct mainbus_attach_args *ma = aux;

	if (cp)
		aprint_normal("%s at %s", ma->ma_name, cp);

	return (UNCONF);
}
