/*	$NetBSD: usb_quirks.c,v 1.14.4.1 1999/11/15 00:41:39 fvdl Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
 * Carlstedt Research & Technology.
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

#include <sys/param.h>
#include <sys/systm.h>
#if defined(__FreeBSD__)
#include <sys/bus.h>
#endif
 
#include <dev/usb/usb.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
extern int usbdebug;
#endif

static struct usbd_quirk_entry {
	u_int16_t idVendor;
	u_int16_t idProduct;
	u_int16_t bcdDevice;
	struct usbd_quirks quirks;
} usb_quirks[] = {
 { USB_VENDOR_KYE, USB_PRODUCT_KYE_NICHE,	    0x100, { UQ_NO_SET_PROTO}},
 { USB_VENDOR_INSIDEOUT,USB_PRODUCT_INSIDEOUT_EDGEPORT4, 
   						    0x094, { UQ_SWAP_UNICODE}},
 { USB_VENDOR_BTC, USB_PRODUCT_BTC_BTC7932,	    0x100, { UQ_NO_STRINGS }},
 { USB_VENDOR_ADS, USB_PRODUCT_ADS_ENET,	    0x002, { UQ_NO_STRINGS }},
 { USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_SERIAL1, 0x101, { UQ_NO_STRINGS }},
 { USB_VENDOR_DALLAS, USB_PRODUCT_DALLAS_J6502,	    0x0a2, { UQ_BAD_ADC }},
 { USB_VENDOR_LOGITECH, USB_PRODUCT_LOGITECH_N48,   0x110, { UQ_MS_REVZ }},
 { 0, 0, 0, { 0 } }
};

struct usbd_quirks usbd_no_quirk = { 0 };

struct usbd_quirks *
usbd_find_quirk(d)
	usb_device_descriptor_t *d;
{
	struct usbd_quirk_entry *t;

	for (t = usb_quirks; t->idVendor != 0; t++) {
		if (t->idVendor  == UGETW(d->idVendor) &&
		    t->idProduct == UGETW(d->idProduct) &&
		    t->bcdDevice == UGETW(d->bcdDevice))
			break;
	}
#ifdef USB_DEBUG
	if (usbdebug && t->quirks.uq_flags)
		logprintf("usbd_find_quirk 0x%04x/0x%04x/%x: %d\n", 
			  UGETW(d->idVendor), UGETW(d->idProduct),
			  UGETW(d->bcdDevice), t->quirks.uq_flags);
#endif
	return (&t->quirks);
}
