/* $NetBSD: mainbus.c,v 1.27.28.1 2002/10/10 18:30:53 jdolecek Exp $ */

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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: mainbus.c,v 1.27.28.1 2002/10/10 18:30:53 jdolecek Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/conf.h>

#include <machine/autoconf.h>
#include <machine/rpb.h>
#include <machine/cpuconf.h>

/* Definition of the mainbus driver. */
static int	mbmatch __P((struct device *, struct cfdata *, void *));
static void	mbattach __P((struct device *, struct device *, void *));
static int	mbprint __P((void *, const char *));

CFATTACH_DECL(mainbus, sizeof(struct device),
    mbmatch, mbattach, NULL, NULL);

/* There can be only one. */
int	mainbus_found;

static int
mbmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{

	if (mainbus_found)
		return (0);

	return (1);
}

static void
mbattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct mainbus_attach_args ma;
	struct pcs *pcsp;
	int i, cpuattachcnt;
	extern int ncpus;

	mainbus_found = 1;

	printf("\n");

	/*
	 * Try to find and attach all of the CPUs in the machine.
	 */
	cpuattachcnt = 0;
	for (i = 0; i < hwrpb->rpb_pcs_cnt; i++) {
		pcsp = LOCATE_PCS(hwrpb, i);
		if ((pcsp->pcs_flags & PCS_PP) == 0)
			continue;

		ma.ma_name = "cpu";
		ma.ma_slot = i;
		if (config_found(self, &ma, mbprint) != NULL)
			cpuattachcnt++;
	}
	if (ncpus != cpuattachcnt)
		printf("WARNING: %d cpus in machine, %d attached\n",
			ncpus, cpuattachcnt);

	if (platform.iobus != NULL) {
		ma.ma_name = platform.iobus;
		ma.ma_slot = 0;			/* meaningless */
		config_found(self, &ma, mbprint);
	}
}

static int
mbprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct mainbus_attach_args *ma = aux;

	if (pnp)
		printf("%s at %s", ma->ma_name, pnp);

	return (UNCONF);
}
