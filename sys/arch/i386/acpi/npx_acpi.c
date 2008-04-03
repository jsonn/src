/* $NetBSD: npx_acpi.c,v 1.16.52.1 2008/04/03 12:42:17 mjf Exp $ */

/*
 * Copyright (c) 2002 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npx_acpi.c,v 1.16.52.1 2008/04/03 12:42:17 mjf Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/intr.h>
#include <machine/specialreg.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/acpi/acpica.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <i386/isa/npxvar.h>

static int	npx_acpi_match(device_t, cfdata_t, void *);
static void	npx_acpi_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(npx_acpi, sizeof(struct npx_softc), npx_acpi_match,
    npx_acpi_attach, NULL, NULL);

/*
 * Supported device IDs
 */

static const char * const npx_acpi_ids[] = {
	"PNP0C04",	/* Math Coprocessor */
	NULL
};

/*
 * npx_acpi_match: autoconf(9) match routine
 */
static int
npx_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, npx_acpi_ids);
}

/*
 * npx_acpi_attach: autoconf(9) attach routine
 */
static void
npx_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct npx_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_io *io;
	struct acpi_irq *irq;
	ACPI_STATUS rv;

	aprint_naive("\n");
	aprint_normal("\n");

	sc->sc_dev = self;

	/* parse resources */
	rv = acpi_resource_parse(sc->sc_dev, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	/* find our i/o registers */
	io = acpi_res_io(&res, 0);
	if (io == NULL) {
		aprint_error_dev(self,
		    "unable to find i/o register resource\n");
		goto out;
	}

	/* find our IRQ */
	irq = acpi_res_irq(&res, 0);
	if (irq == NULL) {
		aprint_error_dev(self, "unable to find irq resource\n");
		goto out;
	}

	sc->sc_iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, io->ar_base, io->ar_length,
		    0, &sc->sc_ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		goto out;
	}

	sc->sc_type = npxprobe1(sc->sc_iot, sc->sc_ioh, irq->ar_irq);

	switch (sc->sc_type) {
	case NPX_INTERRUPT:
		lcr0(rcr0() & ~CR0_NE);
		sc->sc_ih = isa_intr_establish(aa->aa_ic, irq->ar_irq,
		    (irq->ar_type == ACPI_EDGE_SENSITIVE) ? IST_EDGE:IST_LEVEL,
		    IPL_NONE, (int (*)(void *))npxintr, NULL);
		break;
	case NPX_EXCEPTION:
		/*FALLTHROUGH*/
	case NPX_CPUID:
		aprint_verbose_dev(self, "%susing exception 16\n",
		    sc->sc_type == NPX_CPUID ? "reported by CPUID; " : "");
		sc->sc_type = NPX_EXCEPTION;
		break;
	case NPX_BROKEN:
		aprint_error_dev(self, "error reporting broken; not using\n");
		sc->sc_type = NPX_NONE;
		goto out;
	case NPX_NONE:
		panic("npx_acpi_attach");
	}

	npxattach(sc);

 out:
	acpi_resource_cleanup(&res);
}
