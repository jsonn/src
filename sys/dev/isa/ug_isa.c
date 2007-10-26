/* $NetBSD: ug_isa.c,v 1.2.8.2 2007/10/26 15:45:31 joerg Exp $ */

/*
 * Copyright (c) 2007 Mihai Chelaru <kefren@netbsd.ro>
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

/*
 * Driver for Abit uGuru (interface is inspired from it.c and nslm7x.c)
 * Inspired by olle sandberg linux driver as Abit didn't care to release docs
 * Support for uGuru 2005 from Louis Kruger and Hans de Goede linux driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ug_isa.c,v 1.2.8.2 2007/10/26 15:45:31 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/envsys.h>
#include <sys/time.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/ic/ugreg.h>
#include <dev/ic/ugvar.h>

/* autoconf(9) functions */
static int  ug_isa_match(struct device *, struct cfdata *, void *);
static void ug_isa_attach(struct device *, struct device *, void *);
static int  ug_isa_detach(struct device *, int);

CFATTACH_DECL(ug_isa, sizeof(struct ug_softc),
    ug_isa_match, ug_isa_attach, ug_isa_detach, NULL);

extern uint8_t ug_ver;

static int
ug_isa_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct ug_softc wrap_sc;
	bus_space_handle_t bsh;
	uint8_t valc, vald;

	if (ia->ia_nio < 1)     /* need base addr */
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, 8, 0, &bsh))
		return 0;

	valc = bus_space_read_1(ia->ia_iot, bsh, UG_CMD);
	vald = bus_space_read_1(ia->ia_iot, bsh, UG_DATA);

	ug_ver = 0;

	/* Check for uGuru 2003 */

	if (((vald == 0) || (vald == 8)) && (valc == 0xAC))
		ug_ver = 1;

	/* Check for uGuru 2005 */

	wrap_sc.sc_iot = ia->ia_iot;
	wrap_sc.sc_ioh = bsh;

	if (ug2_sync(&wrap_sc) == 1)
		ug_ver = 2;

	/* unmap, prepare ia and bye */
	bus_space_unmap(ia->ia_iot, bsh, 8);

	if (ug_ver != 0) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = 8;
		ia->ia_niomem = 0;
		ia->ia_nirq = 0;
		ia->ia_ndrq = 0;
		return 1;
	}

	return 0;

}

static void
ug_isa_attach(struct device *parent, struct device *self, void *aux)
{
	struct ug_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	int i;

	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr,
	    8, 0, &sc->sc_ioh)) {
		aprint_error(": can't map i/o space\n");
		return;
	}

	ia->ia_iot = sc->sc_iot;
	sc->version = ug_ver;

	if (sc->version == 2) {
		ug2_attach(sc);
		return;
	}

	aprint_normal(": Abit uGuru system monitor\n");

	if (!ug_reset(sc))
		aprint_error("%s: reset failed.\n", sc->sc_dev.dv_xname);

	ug_setup_sensors(sc);

	for (i = 0; i < UG_NUM_SENSORS; i++) {
		sc->sc_data[i].sensor = i;
		sc->sc_data[i].state = ENVSYS_SVALID;
	}

	sc->sc_sysmon.sme_sensor_data = sc->sc_data;
	sc->sc_sysmon.sme_cookie = sc;
	sc->sc_sysmon.sme_gtredata = ug_gtredata;
	sc->sc_sysmon.sme_nsensors = UG_NUM_SENSORS;

	if (sysmon_envsys_register(&sc->sc_sysmon))
		aprint_error("%s: unable to register with sysmon\n",
		    sc->sc_dev.dv_xname);

}

static int
ug_isa_detach(struct device *self, int flags)
{
	struct ug_softc *sc = device_private(self);

	sysmon_envsys_unregister(&sc->sc_sysmon);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, 8);
	return 0;
}

