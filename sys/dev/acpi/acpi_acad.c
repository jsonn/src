/*	$NetBSD: acpi_acad.c,v 1.1.4.5 2003/01/03 17:01:11 thorpej Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ACPI AC Adapter driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_acad.c,v 1.1.4.5 2003/01/03 17:01:11 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/acpi/acpica.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/sysmon/sysmonvar.h>

#define ACPIACAD_NSENSORS	2

/* sensor indexes */
#define ACPIACAD_CONNECTED	0
#define ACPIACAD_DISCONNECTED	1

struct acpiacad_softc {
	struct device sc_dev;		/* base device glue */
	struct acpi_devnode *sc_node;	/* our ACPI devnode */
	int sc_flags;			/* see below */
	int sc_status;			/* power status */
	struct sysmon_envsys sc_sysmon;
	struct envsys_basic_info sc_info[ACPIACAD_NSENSORS];
	struct envsys_tre_data sc_data[ACPIACAD_NSENSORS];
};

const struct envsys_range acpiacad_range[] = {
	{ 0, 2,		ENVSYS_INDICATOR },
	{ 1, 0, 	-1},
};

#define	AACAD_F_VERBOSE		0x01	/* verbose events */

int	acpiacad_match(struct device *, struct cfdata *, void *);
void	acpiacad_attach(struct device *, struct device *, void *);

CFATTACH_DECL(acpiacad, sizeof(struct acpiacad_softc),
    acpiacad_match, acpiacad_attach, NULL, NULL);

void	acpiacad_get_status(void *);
void	acpiacad_notify_handler(ACPI_HANDLE, UINT32, void *context);
static void acpiacad_init_envsys(struct acpiacad_softc *sc);
static int acpiacad_gtredata(struct sysmon_envsys *, struct envsys_tre_data *);
static int acpiacad_streinfo(struct sysmon_envsys *, struct envsys_basic_info *);

/*
 * acpiacad_match:
 *
 *	Autoconfiguration `match' routine.
 */
int
acpiacad_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return (0);

	if (strcmp(aa->aa_node->ad_devinfo.HardwareId, "ACPI0003") == 0)
		return (1);

	return (0);
}

/*
 * acpiacad_attach:
 *
 *	Autoconfiguration `attach' routine.
 */
void
acpiacad_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpiacad_softc *sc = (void *) self;
	struct acpi_attach_args *aa = aux;
	ACPI_STATUS rv;

	printf(": ACPI AC Adapter\n");

	sc->sc_node = aa->aa_node;

	rv = AcpiInstallNotifyHandler(sc->sc_node->ad_handle,
	    ACPI_DEVICE_NOTIFY, acpiacad_notify_handler, sc);
	if (rv != AE_OK) {
		printf("%s: unable to register DEVICE NOTIFY handler: %d\n",
		    sc->sc_dev.dv_xname, rv);
		return;
	}

	/* XXX See acpiacad_notify_handler() */
	rv = AcpiInstallNotifyHandler(sc->sc_node->ad_handle,
	    ACPI_SYSTEM_NOTIFY, acpiacad_notify_handler, sc);
	if (rv != AE_OK) {
		printf("%s: unable to register SYSTEM NOTIFY handler: %d\n",
		    sc->sc_dev.dv_xname, rv);
		return;
	}

	/* Display the current state. */
	sc->sc_flags = AACAD_F_VERBOSE;
	acpiacad_get_status(sc);
	acpiacad_init_envsys(sc);
}

/*
 * acpiacad_get_status:
 *
 *	Get, and possibly display, the current AC line status.
 */
void
acpiacad_get_status(void *arg)
{
	struct acpiacad_softc *sc = arg;

	if (acpi_eval_integer(sc->sc_node->ad_handle, "_PSR",
	    &sc->sc_status) != AE_OK)
		return;

	sc->sc_data[ACPIACAD_CONNECTED].cur.data_s = !!(sc->sc_status);
	sc->sc_data[ACPIACAD_DISCONNECTED].cur.data_s = !(sc->sc_status);

	if (sc->sc_flags & AACAD_F_VERBOSE)
		printf("%s: AC adapter %sconnected\n",
		    sc->sc_dev.dv_xname, sc->sc_status == 0 ? "not " : "");
}

/*
 * acpiacad_notify_handler:
 *
 *	Callback from ACPI interrupt handler to notify us of an event.
 */
void
acpiacad_notify_handler(ACPI_HANDLE handle, UINT32 notify, void *context)
{
	struct acpiacad_softc *sc = context;
	int rv;

	switch (notify) {
	/*
	 * XXX So, BusCheck is not exactly what I would expect,
	 * but at least my IBM T21 sends it on AC adapter status
	 * change.  --thorpej@wasabisystems.com
	 */
	case ACPI_NOTIFY_BusCheck:
	case ACPI_NOTIFY_PowerSourceStatusChanged:
#ifdef ACPI_ACAD_DEBUG
		printf("%s: received notify message: 0x%x\n",
		    sc->sc_dev.dv_xname, notify);
#endif
		rv = AcpiOsQueueForExecution(OSD_PRIORITY_LO,
		    acpiacad_get_status, sc);
		if (rv != AE_OK)
			printf("%s: unable to queue status check: %d\n",
			    sc->sc_dev.dv_xname, rv);
		break;

	default:
		printf("%s: received unknown notify message: 0x%x\n",
		    sc->sc_dev.dv_xname, notify);
	}
}

static void
acpiacad_init_envsys(struct acpiacad_softc *sc)
{
	int i;

	sc->sc_sysmon.sme_ranges = acpiacad_range;

	for (i=0; i<ACPIACAD_NSENSORS; i++) {
		sc->sc_data[i].sensor = sc->sc_info[i].sensor = i;
		sc->sc_data[i].validflags |= (ENVSYS_FVALID | ENVSYS_FCURVALID);
		sc->sc_info[i].validflags = ENVSYS_FVALID;
		sc->sc_data[i].warnflags = 0;
	}

#define INITDATA(index, unit, string) \
	sc->sc_data[index].units = unit;     				\
	sc->sc_info[index].units = unit;     				\
	snprintf(sc->sc_info[index].desc, sizeof(sc->sc_info->desc),	\
	    "%s %s", sc->sc_dev.dv_xname, string);			\

	INITDATA(ACPIACAD_CONNECTED, ENVSYS_INDICATOR, "connected");
	INITDATA(ACPIACAD_DISCONNECTED, ENVSYS_INDICATOR, "disconnected");

	sc->sc_sysmon.sme_sensor_info = sc->sc_info;
	sc->sc_sysmon.sme_sensor_data = sc->sc_data;
	sc->sc_sysmon.sme_cookie = sc;
	sc->sc_sysmon.sme_gtredata = acpiacad_gtredata;
	sc->sc_sysmon.sme_streinfo = acpiacad_streinfo;
	sc->sc_sysmon.sme_nsensors = ACPIACAD_NSENSORS;
	sc->sc_sysmon.sme_envsys_version = 1000;

	if (sysmon_envsys_register(&sc->sc_sysmon))
		printf("%s: unable to register with sysmon\n",
		    sc->sc_dev.dv_xname);
}


int
acpiacad_gtredata(struct sysmon_envsys *sme, struct envsys_tre_data *tred)
{
	struct acpiacad_softc *sc = sme->sme_cookie;

	/* XXX locking */
	*tred = sc->sc_data[tred->sensor];
	/* XXX locking */

	return (0);
}


int
acpiacad_streinfo(struct sysmon_envsys *sme, struct envsys_basic_info *binfo)
{

	/* XXX Not implemented */
	binfo->validflags = 0;

	return (0);
}
