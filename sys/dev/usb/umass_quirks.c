/*	$NetBSD: umass_quirks.c,v 1.43.2.4 2005/03/04 16:50:55 skrll Exp $	*/

/*
 * Copyright (c) 2001, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by MAEKAWA Masahide (gehenna@NetBSD.org).
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 *	  This product includes software developed by the NetBSD
 *	  Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: umass_quirks.c,v 1.43.2.4 2005/03/04 16:50:55 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <dev/scsipi/scsipi_all.h> /* for scsiconf.h below */
#include <dev/scsipi/scsiconf.h> /* for quirks defines */

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/umassvar.h>
#include <dev/usb/umass_quirks.h>

Static usbd_status umass_init_insystem(struct umass_softc *);
Static usbd_status umass_init_shuttle(struct umass_softc *);

Static void umass_fixup_sony(struct umass_softc *);

/*
 * XXX
 * PLEASE NOTE that if you want quirk entries added to this table, you MUST
 * compile a kernel with USB_DEBUG, and submit a full log of the output from
 * whatever operation is "failing" with ?hcidebug=20 or higher and
 * umassdebug=0xffffff.  (It's usually helpful to also set MSGBUFSIZE to
 * something "large" unless you're using a serial console.)  Without this
 * information, the source of the problem cannot be properly analyzed, and
 * the quirk entry WILL NOT be accepted.
 * Also, when an entry is committed to this table, a concise but clear
 * description of the problem MUST accompany it.
 * - mycroft
 */
Static const struct umass_quirk umass_quirks[] = {
	/*
	 * The following 3 In-System Design adapters use a non-standard ATA
	 * over BBB protocol.  Force this protocol by quirk entries.
	 */
	{ { USB_VENDOR_INSYSTEM, USB_PRODUCT_INSYSTEM_ADAPTERV2 },
	  UMASS_WPROTO_BBB, UMASS_CPROTO_ISD_ATA,
	  0,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},
	{ { USB_VENDOR_INSYSTEM, USB_PRODUCT_INSYSTEM_ATAPI },
	  UMASS_WPROTO_BBB, UMASS_CPROTO_ISD_ATA,
	  0,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},
	{ { USB_VENDOR_INSYSTEM, USB_PRODUCT_INSYSTEM_DRIVEV2_5 },
	  UMASS_WPROTO_BBB, UMASS_CPROTO_ISD_ATA,
	  0,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	{ { USB_VENDOR_INSYSTEM, USB_PRODUCT_INSYSTEM_USBCABLE },
	  UMASS_WPROTO_CBI, UMASS_CPROTO_ATAPI,
	  0,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  umass_init_insystem, NULL
	},

	{ { USB_VENDOR_SHUTTLE, USB_PRODUCT_SHUTTLE_EUSB },
	  UMASS_WPROTO_CBI_I, UMASS_CPROTO_ATAPI,
	  0,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  umass_init_shuttle, NULL
	},

	/*
	 * These work around genuine device bugs -- returning the wrong info in
	 * the CSW block.
	 */
	{ { USB_VENDOR_OLYMPUS, USB_PRODUCT_OLYMPUS_C1 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  UMASS_QUIRK_WRONG_CSWSIG,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},
	{ { USB_VENDOR_SCANLOGIC, USB_PRODUCT_SCANLOGIC_SL11R },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  UMASS_QUIRK_WRONG_CSWTAG,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},
	{ { USB_VENDOR_SHUTTLE, USB_PRODUCT_SHUTTLE_ORCA },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  UMASS_QUIRK_WRONG_CSWTAG,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},

	/*
	 * Some Sony cameras advertise a subclass code of 0xff, so we force it
	 * to the correct value iff necessary.
	 */
	{ { USB_VENDOR_SONY, USB_PRODUCT_SONY_DSC },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, umass_fixup_sony
	},

	/*
	 * Stupid device reports itself as SFF-8070, but actually returns a UFI
	 * interrupt descriptor.  - mycroft, 2004/06/28
	 */
	{ { USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_40_MS },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UFI,
	  0,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},

	/*
	 * The DiskOnKey does not reject commands it doesn't recognize in a
	 * sane way -- rather than STALLing the bulk pipe, it continually NAKs
	 * until we time out.  To prevent being screwed by this, for now we
	 * disable 10-byte MODE SENSE the klugy way.  - mycroft, 2003/10/16
	 */
	{ { USB_VENDOR_MSYSTEMS, USB_PRODUCT_MSYSTEMS_DISKONKEY },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NOBIGMODESENSE,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},
	{ { USB_VENDOR_MSYSTEMS, USB_PRODUCT_MSYSTEMS_DISKONKEY2 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NOBIGMODESENSE,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},
};

const struct umass_quirk *
umass_lookup(u_int16_t vendor, u_int16_t product)
{
	return ((const struct umass_quirk *)
		usb_lookup(umass_quirks, vendor, product));
}

Static usbd_status
umass_init_insystem(struct umass_softc *sc)
{
	usbd_status err;

	err = usbd_set_interface(sc->sc_iface, 1);
	if (err) {
		DPRINTF(UDMASS_USB,
			("%s: could not switch to Alt Interface 1\n",
			USBDEVNAME(sc->sc_dev)));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static usbd_status
umass_init_shuttle(struct umass_softc *sc)
{
	usb_device_request_t req;
	u_int8_t status[2];

	/* The Linux driver does this */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = 1;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, sizeof(status));

	return (usbd_do_request(sc->sc_udev, &req, &status));
}

Static void
umass_fixup_sony(struct umass_softc *sc)
{
	usb_interface_descriptor_t *id;

	id = usbd_get_interface_descriptor(sc->sc_iface);
	if (id->bInterfaceSubClass == 0xff)
		sc->sc_cmd = UMASS_CPROTO_SCSI;
}
