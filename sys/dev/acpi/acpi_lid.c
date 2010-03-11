/*	$NetBSD: acpi_lid.c,v 1.25.4.4 2010/03/11 15:03:22 yamt Exp $	*/

/*
 * Copyright 2001, 2003 Wasabi Systems, Inc.
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
 * ACPI Lid Switch driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_lid.c,v 1.25.4.4 2010/03/11 15:03:22 yamt Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#define _COMPONENT		 ACPI_LID_COMPONENT
ACPI_MODULE_NAME		 ("acpi_lid")

struct acpilid_softc {
	struct acpi_devnode	*sc_node;
	struct sysmon_pswitch	 sc_smpsw;
	uint64_t		 sc_status;
};

static const char * const lid_hid[] = {
	"PNP0C0D",
	NULL
};

static int	acpilid_match(device_t, cfdata_t, void *);
static void	acpilid_attach(device_t, device_t, void *);
static int	acpilid_detach(device_t, int);
static void	acpilid_status_changed(void *);
static void	acpilid_notify_handler(ACPI_HANDLE, UINT32, void *);

CFATTACH_DECL_NEW(acpilid, sizeof(struct acpilid_softc),
    acpilid_match, acpilid_attach, acpilid_detach, NULL);

/*
 * acpilid_match:
 *
 *	Autoconfiguration `match' routine.
 */
static int
acpilid_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, lid_hid);
}

/*
 * acpilid_attach:
 *
 *	Autoconfiguration `attach' routine.
 */
static void
acpilid_attach(device_t parent, device_t self, void *aux)
{
	struct acpilid_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	ACPI_STATUS rv;

	aprint_naive(": ACPI Lid Switch\n");
	aprint_normal(": ACPI Lid Switch\n");

	sc->sc_node = aa->aa_node;

	sc->sc_smpsw.smpsw_name = device_xname(self);
	sc->sc_smpsw.smpsw_type = PSWITCH_TYPE_LID;

	(void)pmf_device_register(self, NULL, NULL);
	(void)sysmon_pswitch_register(&sc->sc_smpsw);

	rv = AcpiInstallNotifyHandler(sc->sc_node->ad_handle,
	    ACPI_DEVICE_NOTIFY, acpilid_notify_handler, self);

	if (ACPI_FAILURE(rv))
		aprint_error_dev(self, "failed to register notify handler\n");
}

static int
acpilid_detach(device_t self, int flags)
{
	struct acpilid_softc *sc = device_private(self);
	ACPI_STATUS rv;

	rv = AcpiRemoveNotifyHandler(sc->sc_node->ad_handle,
	    ACPI_DEVICE_NOTIFY, acpilid_notify_handler);

	if (ACPI_FAILURE(rv))
		return EBUSY;

	pmf_device_deregister(self);
	sysmon_pswitch_unregister(&sc->sc_smpsw);

	return 0;
}

/*
 * acpilid_status_changed:
 *
 *	Get, and possibly display, the lid status, and take action.
 */
static void
acpilid_status_changed(void *arg)
{
	device_t dv = arg;
	struct acpilid_softc *sc = device_private(dv);
	ACPI_STATUS rv;

	rv = acpi_eval_integer(sc->sc_node->ad_handle, "_LID", &sc->sc_status);

	if (ACPI_FAILURE(rv))
		return;

	sysmon_pswitch_event(&sc->sc_smpsw, (sc->sc_status == 0) ?
	    PSWITCH_EVENT_PRESSED : PSWITCH_EVENT_RELEASED);
}

/*
 * acpilid_notify_handler:
 *
 *	Callback from ACPI interrupt handler to notify us of an event.
 */
static void
acpilid_notify_handler(ACPI_HANDLE handle, uint32_t notify, void *context)
{
	static const int handler = OSL_NOTIFY_HANDLER;
	device_t dv = context;

	switch (notify) {

	case ACPI_NOTIFY_LidStatusChanged:
		(void)AcpiOsExecute(handler, acpilid_status_changed, dv);
		break;

	default:
		aprint_error_dev(dv, "unknown notify 0x%02X\n", notify);
	}
}

#ifdef _MODULE

MODULE(MODULE_CLASS_DRIVER, acpilid, NULL);
CFDRIVER_DECL(acpilid, DV_DULL, NULL);

static int acpilidloc[] = { -1 };
extern struct cfattach acpilid_ca;

static struct cfparent acpiparent = {
	"acpinodebus", NULL, DVUNIT_ANY
};

static struct cfdata acpilid_cfdata[] = {
	{
		.cf_name = "acpilid",
		.cf_atname = "acpilid",
		.cf_unit = 0,
		.cf_fstate = FSTATE_STAR,
		.cf_loc = acpilidloc,
		.cf_flags = 0,
		.cf_pspec = &acpiparent,
	},

	{ NULL }
};

static int
acpilid_modcmd(modcmd_t cmd, void *context)
{
	int err;

	switch (cmd) {

	case MODULE_CMD_INIT:

		err = config_cfdriver_attach(&acpilid_cd);

		if (err != 0)
			return err;

		err = config_cfattach_attach("acpilid", &acpilid_ca);

		if (err != 0) {
			config_cfdriver_detach(&acpilid_cd);
			return err;
		}

		err = config_cfdata_attach(acpilid_cfdata, 1);

		if (err != 0) {
			config_cfattach_detach("acpilid", &acpilid_ca);
			config_cfdriver_detach(&acpilid_cd);
			return err;
		}

		return 0;

	case MODULE_CMD_FINI:

		err = config_cfdata_detach(acpilid_cfdata);

		if (err != 0)
			return err;

		config_cfattach_detach("acpilid", &acpilid_ca);
		config_cfdriver_detach(&acpilid_cd);

		return 0;

	default:
		return ENOTTY;
	}
}

#endif	/* _MODULE */
