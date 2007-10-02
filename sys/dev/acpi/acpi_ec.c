/*	$NetBSD: acpi_ec.c,v 1.41.6.5 2007/10/02 21:44:11 joerg Exp $	*/

/*-
 * Copyright (c) 2007 Joerg Sonnenberger <joerg@NetBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_ec.c,v 1.41.6.5 2007/10/02 21:44:11 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/mutex.h>

#include <machine/bus.h>

#include <dev/acpi/acpivar.h>

#define	EC_LOCK_TIMEOUT		5

/* From ACPI 3.0b, chapter 12.3 */
#define EC_COMMAND_READ		0x80
#define	EC_COMMAND_WRITE	0x81
#define	EC_COMMAND_BURST_EN	0x82
#define	EC_COMMAND_BURST_DIS	0x83
#define	EC_COMMAND_QUERY	0x84

/* From ACPI 3.0b, chapter 12.2.1 */
#define	EC_STATUS_OBF		0x01
#define	EC_STATUS_IBF		0x02
#define	EC_STATUS_CMD		0x08
#define	EC_STATUS_BURST		0x10
#define	EC_STATUS_SCI		0x20
#define	EC_STATUS_SMI		0x40

static const char *ec_hid[] = {
	"PNP0C09",
	NULL,
};

enum ec_state_t {
	EC_STATE_QUERY,
	EC_STATE_READ,
	EC_STATE_READ_VAL,
	EC_STATE_READ_WAIT,
	EC_STATE_WRITE,
	EC_STATE_WRITE_VAL,
	EC_STATE_WRITE_WAIT,
	EC_STATE_FREE
};

struct acpiec_softc {
	ACPI_HANDLE sc_ech;

	ACPI_HANDLE sc_gpeh;
	UINT8 sc_gpebit;

	bus_space_tag_t sc_data_st;
	bus_space_handle_t sc_data_sh;

	bus_space_tag_t sc_csr_st;
	bus_space_handle_t sc_csr_sh;

	bool sc_need_global_lock;
	UINT32 sc_global_lock;

	kmutex_t sc_mtx, sc_access_mtx;
	kcondvar_t sc_cv, sc_cv_sci;
	enum ec_state_t sc_state;
	bool sc_got_sci;

	uint8_t sc_cur_addr, sc_cur_val;
};

static int acpiecdt_match(device_t, struct cfdata *, void *);
static void acpiecdt_attach(device_t, device_t, void *);

static int acpiec_match(device_t, struct cfdata *, void *);
static void acpiec_attach(device_t, device_t, void *);

static void acpiec_common_attach(device_t, device_t, ACPI_HANDLE,
    bus_addr_t, bus_addr_t, ACPI_HANDLE, uint8_t);

static pnp_status_t acpiec_power(device_t, pnp_request_t, void *);

static bool acpiec_parse_gpe_package(device_t, ACPI_HANDLE,
    ACPI_HANDLE *, uint8_t *);

static void acpiec_gpe_query(void *);
static UINT32 acpiec_gpe_handler(void *);
static ACPI_STATUS acpiec_space_setup(ACPI_HANDLE, UINT32, void *, void **);
static ACPI_STATUS acpiec_space_handler(UINT32, ACPI_PHYSICAL_ADDRESS,
    UINT32, ACPI_INTEGER *, void *, void *);

static void acpiec_gpe_state_maschine(device_t);

CFATTACH_DECL_NEW(acpiec, sizeof(struct acpiec_softc),
    acpiec_match, acpiec_attach, NULL, NULL);

CFATTACH_DECL_NEW(acpiecdt, sizeof(struct acpiec_softc),
    acpiecdt_match, acpiecdt_attach, NULL, NULL);

static device_t ec_singleton = NULL;
static bool acpiec_cold = false;

static bool
acpiecdt_find(device_t parent, ACPI_HANDLE *ec_handle,
    bus_addr_t *cmd_reg, bus_addr_t *data_reg, uint8_t *gpebit)
{
	EC_BOOT_RESOURCES *ec_boot;
	ACPI_STATUS rv;

	rv = AcpiGetFirmwareTable("ECDT", 1, ACPI_LOGICAL_ADDRESSING,
	    (void *)&ec_boot);
	if (rv != AE_OK)
		return false;

	if (ec_boot->EcControl.RegisterBitWidth != 8 ||
	    ec_boot->EcData.RegisterBitWidth != 8) {
		aprint_error_dev(parent,
		    "ECDT register width invalid (%d/%d)\n",
		    ec_boot->EcControl.RegisterBitWidth,
		    ec_boot->EcData.RegisterBitWidth);
		return false;
	}

	rv = AcpiGetHandle(ACPI_ROOT_OBJECT, ec_boot->EcId, ec_handle);
	if (rv != AE_OK) {
		aprint_error_dev(parent,
		    "failed to look up EC object %s: %s\n",
		    ec_boot->EcId, AcpiFormatException(rv));
		return false;
	}

	*cmd_reg = ec_boot->EcControl.Address;
	*data_reg = ec_boot->EcData.Address;
	*gpebit = ec_boot->GpeBit;

	return true;
}

static int
acpiecdt_match(device_t parent, struct cfdata *match, void *aux)
{
	ACPI_HANDLE ec_handle;
	bus_addr_t cmd_reg, data_reg;
	uint8_t gpebit;

	if (acpiecdt_find(parent, &ec_handle, &cmd_reg, &data_reg, &gpebit))
		return 1;
	else
		return 0;
}

static void
acpiecdt_attach(device_t parent, device_t self, void *aux)
{
	ACPI_HANDLE ec_handle;
	bus_addr_t cmd_reg, data_reg;
	uint8_t gpebit;

	if (!acpiecdt_find(parent, &ec_handle, &cmd_reg, &data_reg, &gpebit))
		panic("ECDT disappeared");

	aprint_naive(": ACPI Embedded Controller via ECDT\n");
	aprint_normal(": ACPI Embedded Controller via ECDT\n");

	acpiec_common_attach(parent, self, ec_handle, cmd_reg, data_reg,
	    NULL, gpebit);
}

static int
acpiec_match(device_t parent, struct cfdata *match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, ec_hid);
}

static void
acpiec_attach(device_t parent, device_t self, void *aux)
{
	struct acpi_attach_args *aa = aux;
	struct acpi_resources ec_res;
	struct acpi_io *io0, *io1;
	ACPI_HANDLE gpe_handle;
	uint8_t gpebit;
	ACPI_STATUS rv;

	if (ec_singleton != NULL) {
		aprint_naive(": ACPI Embedded Controller (disabled)\n");
		aprint_normal(": ACPI Embedded Controller (disabled)\n");
		pnp_register(self, pnp_generic_power);
		return;
	}

	aprint_naive(": ACPI Embedded Controller\n");
	aprint_normal(": ACPI Embedded Controller\n");

	if (!acpiec_parse_gpe_package(self, aa->aa_node->ad_handle,
				      &gpe_handle, &gpebit))
		return;

	rv = acpi_resource_parse(self, aa->aa_node->ad_handle, "_CRS",
	    &ec_res, &acpi_resource_parse_ops_default);
	if (rv != AE_OK) {
		aprint_error_dev(self, "resource parsing failed: %s\n",
		    AcpiFormatException(rv));
		return;
	}

	if ((io0 = acpi_res_io(&ec_res, 0)) == NULL) {
		aprint_error_dev(self, "no data register resource\n");
		goto free_res;
	}
	if ((io1 = acpi_res_io(&ec_res, 1)) == NULL) {
		aprint_error_dev(self, "no CSR register resource\n");
		goto free_res;
	}

	acpiec_common_attach(parent, self, aa->aa_node->ad_handle,
	    io1->ar_base, io0->ar_base, gpe_handle, gpebit);

free_res:
	acpi_resource_cleanup(&ec_res);
}

static void
acpiec_common_attach(device_t parent, device_t self,
    ACPI_HANDLE ec_handle, bus_addr_t cmd_reg, bus_addr_t data_reg,
    ACPI_HANDLE gpe_handle, uint8_t gpebit)
{
	struct acpiec_softc *sc = device_private(self);
	ACPI_STATUS rv;
	ACPI_INTEGER val;

	sc->sc_ech = ec_handle;
	sc->sc_gpeh = gpe_handle;
	sc->sc_gpebit = gpebit;

	sc->sc_state = EC_STATE_FREE;
	mutex_init(&sc->sc_mtx, MUTEX_DRIVER, IPL_TTY);
	mutex_init(&sc->sc_access_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sc->sc_cv, "eccv");
	cv_init(&sc->sc_cv_sci, "ecsci");

	if (bus_space_map(sc->sc_data_st, data_reg, 1, 0,
	    &sc->sc_data_sh) != 0) {
		aprint_error_dev(self, "unable to map data register\n");
		return;
	}

	if (bus_space_map(sc->sc_csr_st, cmd_reg, 1, 0, &sc->sc_csr_sh) != 0) {
		aprint_error_dev(self, "unable to map CSR register\n");
		goto post_data_map;
	}

	rv = acpi_eval_integer(sc->sc_ech, "_GLK", &val);
	if (rv == AE_OK) {
		sc->sc_need_global_lock = val != 0;
	} else if (rv != AE_NOT_FOUND) {
		aprint_error_dev(self, "unable to evaluate _GLK: %s\n",
		    AcpiFormatException(rv));
		goto post_csr_map;
	} else {
		sc->sc_need_global_lock = false;
	}
	if (sc->sc_need_global_lock)
		aprint_normal_dev(self, "using global ACPI lock\n");

	rv = AcpiInstallAddressSpaceHandler(sc->sc_ech, ACPI_ADR_SPACE_EC,
	    acpiec_space_handler, acpiec_space_setup, self);
	if (rv != AE_OK) {
		aprint_error_dev(self,
		    "unable to install address space handler: %s\n",
		    AcpiFormatException(rv));
		goto post_csr_map;
	}

	rv = AcpiInstallGpeHandler(sc->sc_gpeh, sc->sc_gpebit,
	    ACPI_GPE_EDGE_TRIGGERED, acpiec_gpe_handler, self);
	if (rv != AE_OK) {
		aprint_error_dev(self, "unable to install GPE handler: %s\n",
		    AcpiFormatException(rv));
		goto post_csr_map;
	}

	rv = AcpiSetGpeType(sc->sc_gpeh, sc->sc_gpebit, ACPI_GPE_TYPE_RUNTIME);
	if (rv != AE_OK) {
		aprint_error_dev(self, "unable to set GPE type: %s\n",
		    AcpiFormatException(rv));
		goto post_csr_map;
	}

	rv = AcpiEnableGpe(sc->sc_gpeh, sc->sc_gpebit, ACPI_ISR);
	if (rv != AE_OK) {
		aprint_error_dev(self, "unable to enable GPE: %s\n",
		    AcpiFormatException(rv));
		goto post_csr_map;
	}

	if (kthread_create(PRI_NONE, KTHREAD_MPSAFE, NULL, acpiec_gpe_query,
		           self, NULL, "acpiec sci thread")) {
		aprint_error_dev(self, "unable to create query kthread\n");
		goto post_csr_map;
	}

	ec_singleton = self;

	pnp_register(self, acpiec_power);

	return;

post_csr_map:
	(void)AcpiRemoveGpeHandler(sc->sc_gpeh, sc->sc_gpebit,
	    acpiec_gpe_handler);
	(void)AcpiRemoveAddressSpaceHandler(sc->sc_ech,
	    ACPI_ADR_SPACE_EC, acpiec_space_handler);
	bus_space_unmap(sc->sc_csr_st, sc->sc_csr_sh, 1);
post_data_map:
	bus_space_unmap(sc->sc_data_st, sc->sc_data_sh, 1);
}

static pnp_status_t
acpiec_power(device_t dv, pnp_request_t req, void *opaque)
{
	pnp_capabilities_t *pcaps;
	pnp_state_t *pstate;

	switch (req) {
	case PNP_REQUEST_GET_CAPABILITIES:
		pcaps = opaque;
		pcaps->state = PNP_STATE_D0 | PNP_STATE_D3;
		break;
	case PNP_REQUEST_GET_STATE:
		pstate = opaque;
		if (acpiec_cold)
			*pstate = PNP_STATE_D0;
		else
			*pstate = PNP_STATE_D3;
		break;
	case PNP_REQUEST_SET_STATE:
		pstate = opaque;
		switch (*pstate) {
		case PNP_STATE_D0:
			acpiec_cold = false;
			break;
		case PNP_STATE_D3:
			acpiec_cold = true;
			break;
		default:
			return PNP_STATUS_UNSUPPORTED;
		}
		break;
	default:
		return PNP_STATUS_UNSUPPORTED;
	}

	return PNP_STATUS_SUCCESS;
}

static bool
acpiec_parse_gpe_package(device_t self, ACPI_HANDLE ec_handle,
    ACPI_HANDLE *gpe_handle, uint8_t *gpebit)
{
	ACPI_BUFFER buf;
	ACPI_OBJECT *p, *c;
	ACPI_STATUS rv;

	rv = acpi_eval_struct(ec_handle, "_GPE", &buf);
	if (rv != AE_OK) {
		aprint_error_dev(self, "unable to evaluate _GPE: %s\n",
		    AcpiFormatException(rv));
		return false;
	}

	p = buf.Pointer;

	if (p->Type == ACPI_TYPE_INTEGER) {
		*gpe_handle = NULL;
		*gpebit = p->Integer.Value;
		AcpiOsFree(p);
		return true;
	}

	if (p->Type != ACPI_TYPE_PACKAGE) {
		aprint_error_dev(self, "_GPE is neither integer nor package\n");
		AcpiOsFree(p);
		return false;
	}
	
	if (p->Package.Count != 2) {
		aprint_error_dev(self, "_GPE package does not contain 2 elements\n");
		AcpiOsFree(p);
		return false;
	}

	c = &p->Package.Elements[0];
	switch (c->Type) {
	case ACPI_TYPE_LOCAL_REFERENCE:
	case ACPI_TYPE_ANY:
		*gpe_handle = c->Reference.Handle;
		break;
	case ACPI_TYPE_STRING:
		/* XXX should be using real scope here */
		rv = AcpiGetHandle(NULL, p->String.Pointer, gpe_handle);
		if (rv != AE_OK) {
			aprint_error_dev(self,
			    "_GPE device reference unresolvable\n");
			AcpiOsFree(p);
			return false;
		}
		break;
	default:
		aprint_error_dev(self, "_GPE device reference incorrect\n");
		AcpiOsFree(p);
		return false;
	}
	c = &p->Package.Elements[1];
	if (c->Type != ACPI_TYPE_INTEGER) {
		aprint_error_dev(self,
		    "_GPE package needs integer as 2nd field\n");
		AcpiOsFree(p);
		return false;
	}
	*gpebit = c->Integer.Value;
	AcpiOsFree(p);
	return true;
}

static uint8_t
acpiec_read_data(struct acpiec_softc *sc)
{
	return bus_space_read_1(sc->sc_data_st, sc->sc_data_sh, 0);
}

static void
acpiec_write_data(struct acpiec_softc *sc, uint8_t val)
{
	bus_space_write_1(sc->sc_data_st, sc->sc_data_sh, 0, val);
}

static uint8_t
acpiec_read_status(struct acpiec_softc *sc)
{
	return bus_space_read_1(sc->sc_csr_st, sc->sc_csr_sh, 0);
}

static void
acpiec_write_command(struct acpiec_softc *sc, uint8_t cmd)
{
	bus_space_write_1(sc->sc_csr_st, sc->sc_csr_sh, 0, cmd);
}

static ACPI_STATUS
acpiec_space_setup(ACPI_HANDLE region, UINT32 func, void *arg,
    void **region_arg)
{
	if (func == ACPI_REGION_DEACTIVATE)
		*region_arg = NULL;
	else
		*region_arg = arg;

	return AE_OK;
}

static void
acpiec_lock(device_t dv)
{
	struct acpiec_softc *sc = device_private(dv);
	ACPI_STATUS rv;

	mutex_enter(&sc->sc_access_mtx);

	if (sc->sc_need_global_lock) {
		rv = AcpiAcquireGlobalLock(EC_LOCK_TIMEOUT, &sc->sc_global_lock);
		if (rv != AE_OK) {
			aprint_error("%s: failed to acquire global lock (continuing)\n",
			    device_xname(dv));
			return;
		}
	}
}

static void
acpiec_unlock(device_t dv)
{
	struct acpiec_softc *sc = device_private(dv);
	ACPI_STATUS rv;

	if (sc->sc_need_global_lock) {
		rv = AcpiReleaseGlobalLock(sc->sc_global_lock);
		if (rv != AE_OK) {
			aprint_error("%s: failed to release global lock (continuing)\n",
			    device_xname(dv));
		}
	}
	mutex_exit(&sc->sc_access_mtx);
}

static ACPI_STATUS
acpiec_read(device_t dv, uint8_t addr, uint8_t *val)
{
	struct acpiec_softc *sc = device_private(dv);
	int timeouts = 0;

	acpiec_lock(dv);
	mutex_enter(&sc->sc_mtx);

retry:
	sc->sc_cur_addr = addr;
	sc->sc_state = EC_STATE_READ;

	acpiec_write_command(sc, EC_COMMAND_READ);
	if (cold || acpiec_cold) {
		for (delay(1); sc->sc_state != EC_STATE_READ_WAIT; delay(1))
			acpiec_gpe_state_maschine(dv);
	} else while (cv_timedwait(&sc->sc_cv, &sc->sc_mtx, hz)) {
		mutex_exit(&sc->sc_mtx);
		AcpiClearGpe(sc->sc_gpeh, sc->sc_gpebit, ACPI_NOT_ISR);
		mutex_enter(&sc->sc_mtx);
		if (++timeouts < 5)
			goto retry;
		mutex_exit(&sc->sc_mtx);
		acpiec_unlock(dv);
		aprint_error_dev(dv, "command takes over 5sec...\n");
		return AE_ERROR;
	}
	*val = acpiec_read_data(sc);

	sc->sc_state = EC_STATE_FREE;

	if (sc->sc_got_sci)
		cv_signal(&sc->sc_cv_sci);


	mutex_exit(&sc->sc_mtx);
	acpiec_unlock(dv);
	return AE_OK;
}

static ACPI_STATUS
acpiec_write(device_t dv, uint8_t addr, uint8_t val)
{
	struct acpiec_softc *sc = device_private(dv);
	int timeouts = 0;

	acpiec_lock(dv);
	mutex_enter(&sc->sc_mtx);

retry:
	sc->sc_cur_addr = addr;
	sc->sc_cur_val = val;
	sc->sc_state = EC_STATE_WRITE;

	acpiec_write_command(sc, EC_COMMAND_WRITE);
	if (cold || acpiec_cold) {
		for (delay(1); sc->sc_state != EC_STATE_WRITE_WAIT; delay(1))
			acpiec_gpe_state_maschine(dv);
	} else while (cv_timedwait(&sc->sc_cv, &sc->sc_mtx, hz)) {
		mutex_exit(&sc->sc_mtx);
		AcpiClearGpe(sc->sc_gpeh, sc->sc_gpebit, ACPI_NOT_ISR);
		mutex_enter(&sc->sc_mtx);
		if (++timeouts < 5)
			goto retry;
		mutex_exit(&sc->sc_mtx);
		acpiec_unlock(dv);
		aprint_error_dev(dv, "command takes over 5sec...\n");
		return AE_ERROR;
	}

	sc->sc_state = EC_STATE_FREE;

	if (sc->sc_got_sci)
		cv_signal(&sc->sc_cv_sci);

	mutex_exit(&sc->sc_mtx);
	acpiec_unlock(dv);
	return AE_OK;
}

static ACPI_STATUS
acpiec_space_handler(UINT32 func, ACPI_PHYSICAL_ADDRESS paddr,
    UINT32 width, ACPI_INTEGER *value, void *arg, void *region_arg)
{
	device_t dv;
	struct acpiec_softc *sc;
	ACPI_STATUS rv;
	uint8_t addr, reg;
	unsigned int i;

	if (paddr > 0xff || width % 8 != 0 || value == NULL || arg == NULL ||
	    paddr + width / 8 > 0xff)
		return AE_BAD_PARAMETER;

	addr = paddr;
	dv = arg;
	sc = device_private(dv);

	rv = AE_OK;

	switch (func) {
	case ACPI_READ:
		for (i = 0; i < width; i += 8, ++addr) {
			rv = acpiec_read(dv, addr, &reg);
			if (rv != AE_OK)
				break;
			*value |= (ACPI_INTEGER)reg << i;
		}
		break;
	case ACPI_WRITE:
		for (i = 0; i < width; i += 8, ++addr) {
			reg = (*value >>i) & 0xff;
			rv = acpiec_write(dv, addr, reg);
			if (rv != AE_OK)
				break;
		}
		break;
	default:
		aprint_error("%s: invalid Address Space function called: %x\n",
		    device_xname(dv), (unsigned int)func);
		return AE_BAD_PARAMETER;
	}

	return rv;
}

static void
acpiec_gpe_query(void *arg)
{
	device_t dv = arg;
	struct acpiec_softc *sc = device_private(dv);
	uint8_t reg;
	char qxx[5];
	ACPI_STATUS rv;

loop:
	mutex_enter(&sc->sc_mtx);

	if (sc->sc_got_sci == false)
		cv_wait(&sc->sc_cv_sci, &sc->sc_mtx);
	mutex_exit(&sc->sc_mtx);

	acpiec_lock(dv);
	mutex_enter(&sc->sc_mtx);

	acpiec_write_command(sc, EC_COMMAND_QUERY);
	sc->sc_state = EC_STATE_QUERY;

	cv_wait(&sc->sc_cv, &sc->sc_mtx);

	reg = acpiec_read_data(sc);
	sc->sc_state = EC_STATE_FREE;
	sc->sc_got_sci = false;

	mutex_exit(&sc->sc_mtx);
	acpiec_unlock(dv);

	if (reg == 0)
		goto loop; /* Spurious query result */
	/*
	 * Evaluate _Qxx to respond to the controller.
	 */
	snprintf(qxx, sizeof(qxx), "_Q%02X", (unsigned int)reg);
	rv = AcpiEvaluateObject(sc->sc_ech, qxx, NULL, NULL);
	if (rv != AE_OK && rv != AE_NOT_FOUND) {
		aprint_error("%s: GPE query method %s failed: %s",
		    device_xname(dv), qxx, AcpiFormatException(rv));
	}

	goto loop;
}

static void
acpiec_gpe_state_maschine(device_t dv)
{
	struct acpiec_softc *sc = device_private(dv);
	uint8_t reg;

	reg = acpiec_read_status(sc);

	if (reg & EC_STATUS_SCI)
		sc->sc_got_sci = true;

	switch (sc->sc_state) {
	case EC_STATE_QUERY:
		if ((reg & EC_STATUS_OBF) == 0)
			break; /* Nothing of interest here. */

		cv_signal(&sc->sc_cv);
		break;

	case EC_STATE_READ:
		if ((reg & EC_STATUS_IBF) != 0)
			break; /* Nothing of interest here. */

		acpiec_write_data(sc, sc->sc_cur_addr);
		sc->sc_state = EC_STATE_READ_VAL;
		break;

	case EC_STATE_READ_VAL:
		if ((reg & EC_STATUS_OBF) == 0)
			break; /* Nothing of interest here. */
		cv_signal(&sc->sc_cv);
		sc->sc_state = EC_STATE_READ_WAIT;
		break;

	case EC_STATE_READ_WAIT:
		break; /* Nothing of interest here. */

	case EC_STATE_WRITE:
		if ((reg & EC_STATUS_IBF) != 0)
			break; /* Nothing of interest here. */
		acpiec_write_data(sc, sc->sc_cur_addr);
		sc->sc_state = EC_STATE_WRITE_VAL;
		break;

	case EC_STATE_WRITE_VAL:
		if ((reg & EC_STATUS_IBF) != 0)
			break; /* Nothing of interest here. */
		cv_signal(&sc->sc_cv);
		acpiec_write_data(sc, sc->sc_cur_val);
		sc->sc_state = EC_STATE_WRITE_WAIT;
		break;

	case EC_STATE_WRITE_WAIT:
		break;

	case EC_STATE_FREE:
		if (sc->sc_got_sci)
			cv_signal(&sc->sc_cv_sci);
		break;
	default:
		panic("invalid state");
	}
}

static UINT32
acpiec_gpe_handler(void *arg)
{
	device_t dv = arg;
	struct acpiec_softc *sc = device_private(dv);

	AcpiClearGpe(sc->sc_gpeh, sc->sc_gpebit, ACPI_ISR);

	mutex_enter(&sc->sc_mtx);
	acpiec_gpe_state_maschine(dv);
	mutex_exit(&sc->sc_mtx);

	return 0;
}
