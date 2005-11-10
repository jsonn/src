/*	$NetBSD: autoconf.c,v 1.77.2.6 2005/11/10 13:59:59 skrll Exp $	*/

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
 * All rights reserved.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: autoconf.c,v 1.77.2.6 2005/11/10 13:59:59 skrll Exp $");

#include "opt_compat_netbsd.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/disk.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/conf.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <machine/nexus.h>
#include <machine/ioa.h>
#include <machine/ka820.h>
#include <machine/ka750.h>
#include <machine/ka650.h>
#include <machine/clock.h>
#include <machine/rpb.h>
#include <machine/bus.h>

#include <vax/vax/gencons.h>

#include <dev/bi/bireg.h>

#include "locators.h"
#include "opt_cputype.h"

void	gencnslask __P((void));

struct cpu_dep *dep_call;

struct evcnt softnet_intrcnt =
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "soft", "net");
struct evcnt softserial_intrcnt =
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "soft", "serial");
struct evcnt softclock_intrcnt =
	EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "soft", "clock");

#define MAINBUS	0

void
cpu_configure(void)
{

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("mainbus not configured");

	evcnt_attach_static(&softserial_intrcnt);
	evcnt_attach_static(&softnet_intrcnt);
	evcnt_attach_static(&softclock_intrcnt);

	/*
	 * We're ready to start up. Clear CPU cold start flag.
	 * Soft cold-start flag will be cleared in configure().
	 */
	if (dep_call->cpu_clrf) 
		(*dep_call->cpu_clrf)();
}

void
cpu_rootconf(void)
{
	/*
	 * The device we booted from are looked for during autoconfig.
	 * If there has been a match, it's already been done.
	 */

#ifdef DEBUG
	printf("booted from type %d unit %d csr 0x%lx adapter %lx slave %d\n",
	    rpb.devtyp, rpb.unit, rpb.csrphy, rpb.adpphy, rpb.slave);
#endif
	printf("boot device: %s\n",
	    booted_device ? booted_device->dv_xname : "<unknown>");

	setroot(booted_device, booted_partition);
}

int	mainbus_print __P((void *, const char *));
int	mainbus_match __P((struct device *, struct cfdata *, void *));
void	mainbus_attach __P((struct device *, struct device *, void *));

int
mainbus_print(aux, hej)
	void *aux;
	const char *hej;
{
	if (hej)
		aprint_normal("nothing at %s", hej);
	return (UNCONF);
}

int
mainbus_match(parent, cf, aux)
	struct	device	*parent;
	struct cfdata *cf;
	void	*aux;
{
	return 1; /* First (and only) mainbus */
}

void
mainbus_attach(parent, self, hej)
	struct	device	*parent, *self;
	void	*hej;
{

	printf("\n");

	/*
	 * Hopefully there a master bus?
	 * Maybe should have this as master instead of mainbus.
	 */
	config_found(self, NULL, mainbus_print);

#if VAX53 || VAXANY
	/* Kludge: To have two master buses */
	if (vax_boardtype == VAX_BTYP_53)
		config_found(self, (void *)1, mainbus_print);
#endif

	if (dep_call->cpu_subconf)
		(*dep_call->cpu_subconf)(self);

#if defined(COMPAT_14)
	if (rpb.rpb_base == (void *)-1)
		printf("\nWARNING: you must update your boot blocks.\n\n");
#endif

}

CFATTACH_DECL(mainbus, sizeof(struct device),
    mainbus_match, mainbus_attach, NULL, NULL);

#include "sd.h"
#include "cd.h"
#include "rl.h"
#include "ra.h"
#include "hp.h"
#include "ry.h"

static int ubtest(void *);
static int jmfr(const char *, struct device *, int);
static int booted_qe(struct device *, void *);
static int booted_qt(struct device *, void *);
static int booted_le(struct device *, void *);
static int booted_ze(struct device *, void *);
static int booted_de(struct device *, void *);
static int booted_ni(struct device *, void *);
#if NSD > 0 || NCD > 0
static int booted_sd(struct device *, void *);
#endif
#if NRL > 0
static int booted_rl(struct device *, void *);
#endif
#if NRA
static int booted_ra(struct device *, void *);
#endif
#if NHP
static int booted_hp(struct device *, void *);
#endif
#if NRD
static int booted_rd(struct device *, void *);
#endif

int (*devreg[])(struct device *, void *) = {
	booted_qe,
	booted_qt,
	booted_le,
	booted_ze,
	booted_de,
	booted_ni,
#if NSD > 0 || NCD > 0
	booted_sd,
#endif
#if NRL > 0
	booted_rl,
#endif
#if NRA
	booted_ra,
#endif
#if NHP
	booted_hp,
#endif
#if NRD
	booted_rd,
#endif
	0,
};

#define	ubreg(x) ((x) & 017777)

void
device_register(struct device *dev, void *aux)
{
	int (**dp)(struct device *, void *) = devreg;

	/* If there's a synthetic RPB, we can't trust it */
	if (rpb.rpb_base == (void *)-1)
		return;

	while (*dp) {
		if ((*dp)(dev, aux)) {
			booted_device = dev;
			break;
		}
		dp++;
	}
}

/*
 * Simple checks. Return 1 on fail.
 */
int
jmfr(const char *n, struct device *dev, int nr)
{
	if (rpb.devtyp != nr)
		return 1;
	return strcmp(n, dev->dv_cfdata->cf_name);
}

#include <dev/qbus/ubavar.h>
int
ubtest(void *aux)
{
	paddr_t p;

	p = kvtophys(((struct uba_attach_args *)aux)->ua_ioh);
	if (rpb.csrphy != p)
		return 1;
	return 0;
}

#if 1 /* NNI */
#include <dev/bi/bivar.h>
int
booted_ni(struct device *dev, void *aux)
{
	struct bi_attach_args *ba = aux;

	if (jmfr("ni", dev, BDEV_NI) || (kvtophys(ba->ba_ioh) != rpb.csrphy))
		return 0;

	return 1;
}
#endif /* NNI */

#if 1 /* NDE */
int
booted_de(struct device *dev, void *aux)
{

	if (jmfr("de", dev, BDEV_DE) || ubtest(aux))
		return 0;

	return 1;
}
#endif /* NDE */

int
booted_le(struct device *dev, void *aux)
{
	if (jmfr("le", dev, BDEV_LE))
		return 0;
	return 1;
}

int
booted_ze(struct device *dev, void *aux)
{
	if (jmfr("ze", dev, BDEV_ZE))
		return 0;
	return 1;
}

int
booted_qt(struct device *dev, void *aux)
{
	if (jmfr("qt", dev, BDEV_QE) || ubtest(aux))
		return 0;

	return 1;
}

#if 1 /* NQE */
int
booted_qe(struct device *dev, void *aux)
{
	if (jmfr("qe", dev, BDEV_QE) || ubtest(aux))
		return 0;

	return 1;
}
#endif /* NQE */

#if NSD > 0 || NCD > 0
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipiconf.h>
int
booted_sd(struct device *dev, void *aux)
{
	struct scsipibus_attach_args *sa = aux;
	struct device *ppdev;

	/* Is this a SCSI device? */
	if (jmfr("sd", dev, BDEV_SD) && jmfr("sd", dev, BDEV_SDN) &&
	    jmfr("cd", dev, BDEV_SD) && jmfr("cd", dev, BDEV_SDN))
		return 0;

	if (sa->sa_periph->periph_channel->chan_bustype->bustype_type !=
	    SCSIPI_BUSTYPE_SCSI)
		return 0; /* ``Cannot happen'' */

	if (sa->sa_periph->periph_target != (rpb.unit/100) ||
	    sa->sa_periph->periph_lun != (rpb.unit % 100))
		return 0; /* Wrong unit */

	ppdev = dev->dv_parent->dv_parent;

	/* VS3100 NCR 53C80 (si) & VS4000 NCR 53C94 (asc) */
	if ((jmfr("si",  ppdev, BDEV_SD) == 0 ||	/* new name */
	     jmfr("asc", ppdev, BDEV_SD) == 0 ||
	     jmfr("asc", ppdev, BDEV_SDN) == 0) &&
	    (ppdev->dv_cfdata->cf_loc[VSBUSCF_CSR] == rpb.csrphy))
			return 1;

	return 0; /* Where did we come from??? */
}
#endif
#if NRL > 0
#include <dev/qbus/rlvar.h>
int
booted_rl(struct device *dev, void *aux)
{
	struct rlc_attach_args *raa = aux;
	static int ub;

	if (jmfr("rlc", dev, BDEV_RL) == 0)
		ub = ubtest(aux);
	if (ub)
		return 0;
	if (jmfr("rl", dev, BDEV_RL))
		return 0;
	if (raa->hwid != rpb.unit)
		return 0; /* Wrong unit number */
	return 1;
}
#endif

#if NRA
#include <dev/mscp/mscp.h>
#include <dev/mscp/mscpreg.h>
#include <dev/mscp/mscpvar.h>
int
booted_ra(struct device *dev, void *aux)
{
	struct drive_attach_args *da = aux;
	struct mscp_softc *pdev = (void *)dev->dv_parent;
	paddr_t ioaddr;

	if (jmfr("ra", dev, BDEV_UDA))
		return 0;

	if (da->da_mp->mscp_unit != rpb.unit)
		return 0; /* Wrong unit number */

	ioaddr = kvtophys(pdev->mi_iph); /* Get phys addr of CSR */
	if (rpb.devtyp == BDEV_UDA && rpb.csrphy == ioaddr)
		return 1; /* Did match CSR */

	return 0;
}
#endif
#if NHP
#include <vax/mba/mbavar.h>
int
booted_hp(struct device *dev, void *aux)
{
	static int mbaaddr;

	/* Save last adapter address */
	if (jmfr("mba", dev, BDEV_HP) == 0) {
		struct sbi_attach_args *sa = aux;

		mbaaddr = kvtophys(sa->sa_ioh);
		return 0;
	}

	if (jmfr("hp", dev, BDEV_HP))
		return 0;

	if (((struct mba_attach_args *)aux)->ma_unit != rpb.unit)
		return 0;

	if (mbaaddr != rpb.adpphy)
		return 0;

	return 1;
}
#endif
#if NRD
int     
booted_rd(struct device *dev, void *aux)
{
	int *nr = aux; /* XXX - use the correct attach struct */

	if (jmfr("rd", dev, BDEV_RD))
		return 0;

	if (*nr != rpb.unit)
		return 0;

	return 1;
}
#endif
