/* $NetBSD: acpi_pci.c,v 1.4.2.3 2010/08/11 22:53:16 yamt Exp $ */

/*
 * Copyright (c) 2009, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christoph Egger and Gregoire Sutre.
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
__KERNEL_RCSID(0, "$NetBSD: acpi_pci.c,v 1.4.2.3 2010/08/11 22:53:16 yamt Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/systm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_pci.h>

#define _COMPONENT	  ACPI_BUS_COMPONENT
ACPI_MODULE_NAME	  ("acpi_pci")

#define ACPI_HILODWORD(x) ACPI_HIWORD(ACPI_LODWORD((x)))
#define ACPI_LOLODWORD(x) ACPI_LOWORD(ACPI_LODWORD((x)))

static ACPI_STATUS	  acpi_pcidev_pciroot_bus(ACPI_HANDLE, uint16_t *);
static ACPI_STATUS	  acpi_pcidev_pciroot_bus_callback(ACPI_RESOURCE *,
							   void *);

/*
 * Regarding PCI Segment Groups (ACPI 4.0, p. 277):
 *
 * "The optional _SEG object is located under a PCI host bridge and
 *  evaluates to an integer that describes the PCI Segment Group (see PCI
 *  Firmware Specification v3.0)."
 *
 * "PCI Segment Group is purely a software concept managed by system
 *  firmware and used by OSPM. It is a logical collection of PCI buses
 *  (or bus segments). It is a way to logically group the PCI bus segments
 *  and PCI Express Hierarchies. _SEG is a level higher than _BBN."
 *
 * "PCI Segment Group supports more than 256 buses in a system by allowing
 *  the reuse of the PCI bus numbers.  Within each PCI Segment Group, the bus
 *  numbers for the PCI buses must be unique. PCI buses in different PCI
 *  Segment Group are permitted to have the same bus number."
 */

/*
 * Regarding PCI Base Bus Numbers (ACPI 4.0, p. 277):
 *
 * "For multi-root PCI platforms, the _BBN object evaluates to the PCI bus
 *  number that the BIOS assigns.  This is needed to access a PCI_Config
 *  operation region for the specified bus.  The _BBN object is located under
 *  a PCI host bridge and must be unique for every host bridge within a
 *  segment since it is the PCI bus number."
 *
 * Moreover, the ACPI FAQ (http://www.acpi.info/acpi_faq.htm) says:
 *
 * "For a multiple root bus machine, _BBN is required for each bus.  _BBN
 *  should provide the bus number assigned to this bus by the BIOS at boot
 *  time."
 */

/*
 * acpi_pcidev_pciroot_bus:
 *
 *	Derive the PCI bus number of a PCI root bridge from its resources.
 *	If successful, return AE_OK and fill *busp.  Otherwise, return an
 *	exception code and leave *busp unchanged.
 */
static ACPI_STATUS
acpi_pcidev_pciroot_bus(ACPI_HANDLE handle, uint16_t *busp)
{
	ACPI_STATUS rv;
	int32_t bus;

	bus = -1;

	/*
	 * XXX:	Use the ACPI resource parsing functions (acpi_resource.c)
	 *	once bus number ranges have been implemented there.
	 */
	rv = AcpiWalkResources(handle, "_CRS",
	    acpi_pcidev_pciroot_bus_callback, &bus);

	if (ACPI_FAILURE(rv))
		return rv;

	if (bus < 0 || bus > 0xFFFF)
		return AE_NOT_EXIST;

	*busp = (uint16_t)bus;

	return rv;
}

static ACPI_STATUS
acpi_pcidev_pciroot_bus_callback(ACPI_RESOURCE *res, void *context)
{
	ACPI_RESOURCE_ADDRESS64 addr64;
	int32_t *bus = context;

	/* Always continue the walk by returning AE_OK. */
	if ((res->Type != ACPI_RESOURCE_TYPE_ADDRESS16) &&
	    (res->Type != ACPI_RESOURCE_TYPE_ADDRESS32) &&
	    (res->Type != ACPI_RESOURCE_TYPE_ADDRESS64))
		return AE_OK;

	if (ACPI_FAILURE(AcpiResourceToAddress64(res, &addr64)))
		return AE_OK;

	if (addr64.ResourceType != ACPI_BUS_NUMBER_RANGE)
		return AE_OK;

	if (*bus != -1)
		return AE_ALREADY_EXISTS;

	*bus = addr64.Minimum;

	return AE_OK;
}

/*
 * acpi_pcidev_scan:
 *
 *	Scan the ACPI device tree for PCI devices.  A node is detected as a
 *	PCI device if it has an ancestor that is a PCI root bridge and such
 *	that all intermediate nodes are PCI-to-PCI bridges.  Depth-first
 *	recursive implementation.
 */
ACPI_STATUS
acpi_pcidev_scan(struct acpi_devnode *ad)
{
	struct acpi_devnode *child;
	struct acpi_pci_info *ap;
	ACPI_INTEGER val;
	ACPI_STATUS rv;

	ad->ad_pciinfo = NULL;

	if (ad->ad_devinfo->Type != ACPI_TYPE_DEVICE ||
	    !(ad->ad_devinfo->Valid & ACPI_VALID_ADR))
		goto rec;

	/*
	 * We attach PCI information only to devices that are present,
	 * enabled, and functioning properly.
	 * Note: there is a possible race condition, because _STA may
	 * have changed since ad->ad_devinfo->CurrentStatus was set.
	 */
	if ((ad->ad_devinfo->Valid & ACPI_VALID_STA) != 0 &&
	    (ad->ad_devinfo->CurrentStatus & ACPI_STA_OK) != ACPI_STA_OK)
		goto rec;

	if (ad->ad_devinfo->Flags & ACPI_PCI_ROOT_BRIDGE) {

		ap = kmem_zalloc(sizeof(*ap), KM_SLEEP);

		if (ap == NULL)
			return AE_NO_MEMORY;

		/*
		 * If no _SEG exist, all PCI bus segments are assumed
		 * to be in the PCI segment group 0 (ACPI 4.0, p. 277).
		 * The segment group number is conveyed in the lower
		 * 16 bits of _SEG (the other bits are all reserved).
		 */
		rv = acpi_eval_integer(ad->ad_handle, "_SEG", &val);

		if (ACPI_SUCCESS(rv))
			ap->ap_segment = ACPI_LOWORD(val);

		/* Try to get bus number using _CRS first. */
		rv = acpi_pcidev_pciroot_bus(ad->ad_handle, &ap->ap_bus);

		if (ACPI_FAILURE(rv)) {
			rv = acpi_eval_integer(ad->ad_handle, "_BBN", &val);

			if (ACPI_SUCCESS(rv))
				ap->ap_bus = ACPI_LOWORD(val);
		}

		ap->ap_device = ACPI_HILODWORD(ad->ad_devinfo->Address);
		ap->ap_function = ACPI_LOLODWORD(ad->ad_devinfo->Address);

		if (ap->ap_bus > 255 || ap->ap_device > 31 ||
		    ap->ap_function > 7) {
			aprint_error_dev(ad->ad_root,
			    "invalid PCI address for %s\n", ad->ad_name);
			kmem_free(ap, sizeof(*ap));
			goto rec;
		}

		ap->ap_bridge = true;
		ap->ap_downbus = ap->ap_bus;

		ad->ad_pciinfo = ap;

		goto rec;
	}

	if ((ad->ad_parent != NULL) &&
	    (ad->ad_parent->ad_pciinfo != NULL) &&
	    (ad->ad_parent->ad_pciinfo->ap_bridge)) {

		/*
		 * Our parent is a PCI root bridge or a PCI-to-PCI
		 * bridge. We have the same PCI segment number, and
		 * our bus number is its downstream bus number.
		 */
		ap = kmem_zalloc(sizeof(*ap), KM_SLEEP);

		if (ap == NULL)
			return AE_NO_MEMORY;

		ap->ap_segment = ad->ad_parent->ad_pciinfo->ap_segment;
		ap->ap_bus = ad->ad_parent->ad_pciinfo->ap_downbus;

		ap->ap_device = ACPI_HILODWORD(ad->ad_devinfo->Address);
		ap->ap_function = ACPI_LOLODWORD(ad->ad_devinfo->Address);

		if (ap->ap_device > 31 || ap->ap_function > 7) {
			aprint_error_dev(ad->ad_root,
			    "invalid PCI address for %s\n", ad->ad_name);
			kmem_free(ap, sizeof(*ap));
			goto rec;
		}

		/*
		 * Check whether this device is a PCI-to-PCI
		 * bridge and get its secondary bus number.
		 */
		rv = acpi_pcidev_ppb_downbus(ap->ap_segment, ap->ap_bus,
		    ap->ap_device, ap->ap_function, &ap->ap_downbus);

		ap->ap_bridge = (rv != AE_OK) ? false : true;
		ad->ad_pciinfo = ap;

		goto rec;
	}

rec:
	SIMPLEQ_FOREACH(child, &ad->ad_child_head, ad_child_list) {
		rv = acpi_pcidev_scan(child);

		if (ACPI_FAILURE(rv))
			return rv;
	}

	return AE_OK;
}

/*
 * acpi_pcidev_ppb_downbus:
 *
 *	Retrieve the secondary bus number of the PCI-to-PCI bridge having the
 *	given PCI id.  If successful, return AE_OK and fill *downbus.
 *	Otherwise, return an exception code and leave *downbus unchanged.
 *
 * XXX	Need to deal with PCI segment groups (see also acpica/OsdHardware.c).
 */
ACPI_STATUS
acpi_pcidev_ppb_downbus(uint16_t segment, uint16_t bus, uint16_t device,
    uint16_t function, uint16_t *downbus)
{
	struct acpi_softc *sc = acpi_softc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
	pcireg_t val;

	if (bus > 255 || device > 31 || function > 7)
		return AE_BAD_PARAMETER;

	pc = sc->sc_pc;

	tag = pci_make_tag(pc, bus, device, function);

	/* Check that this device exists. */
	val = pci_conf_read(pc, tag, PCI_ID_REG);

	if (PCI_VENDOR(val) == PCI_VENDOR_INVALID ||
	    PCI_VENDOR(val) == 0)
		return AE_NOT_EXIST;

	/* Check that this device is a PCI-to-PCI bridge. */
	val = pci_conf_read(pc, tag, PCI_BHLC_REG);

	if (PCI_HDRTYPE_TYPE(val) != PCI_HDRTYPE_PPB)
		return AE_TYPE;

	/* This is a PCI-to-PCI bridge.  Get its secondary bus#. */
	val = pci_conf_read(pc, tag, PPB_REG_BUSINFO);
	*downbus = PPB_BUSINFO_SECONDARY(val);

	return AE_OK;
}

/*
 * acpi_pcidev_find:
 *
 *      Finds a PCI device in the ACPI name space.
 *
 *      Returns an ACPI device node on success and NULL on failure.
 */
struct acpi_devnode *
acpi_pcidev_find(uint16_t segment, uint16_t bus,
    uint16_t device, uint16_t function)
{
	struct acpi_softc *sc = acpi_softc;
	struct acpi_devnode *ad;

	if (sc == NULL)
		return NULL;

	SIMPLEQ_FOREACH(ad, &sc->ad_head, ad_list) {

		if (ad->ad_pciinfo != NULL &&
		    ad->ad_pciinfo->ap_segment == segment &&
		    ad->ad_pciinfo->ap_bus == bus &&
		    ad->ad_pciinfo->ap_device == device &&
		    ad->ad_pciinfo->ap_function == function)
			return ad;
	}

	return NULL;
}
