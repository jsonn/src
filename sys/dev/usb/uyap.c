/*	$NetBSD: uyap.c,v 1.9.22.1 2006/10/22 06:06:53 yamt Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by  Lennart Augustsson <lennart@augustsson.net>.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
__KERNEL_RCSID(0, "$NetBSD: uyap.c,v 1.9.22.1 2006/10/22 06:06:53 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/tty.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/ezload.h>

const struct ezdata uyap_firmware[] = {
#include "dev/usb/uyap_firmware.h"
};
const struct ezdata *uyap_firmwares[] = { uyap_firmware, NULL };

struct uyap_softc {
	USBBASEDEVICE		sc_dev;		/* base device */
};

USB_DECLARE_DRIVER(uyap);

USB_MATCH(uyap)
{
	USB_MATCH_START(uyap, uaa);

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	/* Match the boot device. */
	if (uaa->vendor == USB_VENDOR_SILICONPORTALS &&
	    uaa->product == USB_PRODUCT_SILICONPORTALS_YAPPH_NF)
		return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

USB_ATTACH(uyap)
{
	USB_ATTACH_START(uyap, sc, uaa);
	usbd_device_handle dev = uaa->device;
	usbd_status err;
	char *devinfop;

	devinfop = usbd_devinfo_alloc(dev, 0);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfop);
	usbd_devinfo_free(devinfop);

	printf("%s: downloading firmware\n", USBDEVNAME(sc->sc_dev));

	err = ezload_downloads_and_reset(dev, uyap_firmwares);
	if (err) {
		printf("%s: download ezdata error: %s\n",
		       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		USB_ATTACH_ERROR_RETURN;
	}

	printf("%s: firmware download complete, disconnecting.\n",
	       USBDEVNAME(sc->sc_dev));
	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(uyap)
{
	/*USB_DETACH_START(uyap, sc);*/

	return (0);
}

int
uyap_activate(device_ptr_t self __unused, enum devact act __unused)
{
	return 0;
}
