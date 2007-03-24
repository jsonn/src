/*	$NetBSD: uipaq.c,v 1.5.2.1 2007/03/24 14:55:51 yamt Exp $	*/
/*	$OpenBSD: uipaq.c,v 1.1 2005/06/17 23:50:33 deraadt Exp $	*/

/*
 * Copyright (c) 2000-2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
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

/*
 * iPAQ driver
 * 
 * 19 July 2003:	Incorporated changes suggested by Sam Lawrance from
 * 			the uppc module
 *
 *
 * Contact isis@cs.umd.edu if you have any questions/comments about this driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/tty.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbcdc.h>	/*UCDC_* stuff */

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/ucomvar.h>

#ifdef UIPAQ_DEBUG
#define DPRINTF(x)	if (uipaqdebug) printf x
#define DPRINTFN(n,x)	if (uipaqdebug>(n)) printf x
int uipaqdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UIPAQ_CONFIG_NO		1
#define UIPAQ_IFACE_INDEX	0

#define UIPAQIBUFSIZE 1024
#define UIPAQOBUFSIZE 1024

struct uipaq_softc {
	USBBASEDEVICE		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* device */
	usbd_interface_handle	sc_iface;	/* interface */

	device_ptr_t		sc_subdev;	/* ucom uses that */
	u_int16_t		sc_lcr;		/* state for DTR/RTS */

	u_int16_t		sc_flags;

	u_char			sc_dying;
};

/* Callback routines */
Static void	uipaq_set(void *, int, int, int);


/* Support routines. */
/* based on uppc module by Sam Lawrance */
Static void	uipaq_dtr(struct uipaq_softc *sc, int onoff);
Static void	uipaq_rts(struct uipaq_softc *sc, int onoff);
Static void	uipaq_break(struct uipaq_softc* sc, int onoff);


struct ucom_methods uipaq_methods = {
	NULL,
	uipaq_set,
	NULL,
	NULL,
	NULL,	/*open*/
	NULL,	/*close*/
	NULL,
	NULL
};

struct uipaq_type {
	struct usb_devno	uv_dev;
	u_int16_t		uv_flags;
};

static const struct uipaq_type uipaq_devs[] = {
	{{ USB_VENDOR_HP, USB_PRODUCT_HP_2215 }, 0 },
	{{ USB_VENDOR_HP, USB_PRODUCT_HP_568J }, 0},
	{{ USB_VENDOR_COMPAQ, USB_PRODUCT_COMPAQ_IPAQPOCKETPC} , 0},
	{{ USB_VENDOR_CASIO, USB_PRODUCT_CASIO_BE300} , 0},
	{{ USB_VENDOR_SHARP, USB_PRODUCT_SHARP_WS007SH} , 0}
};

#define uipaq_lookup(v, p) ((const struct uipaq_type *)usb_lookup(uipaq_devs, v, p))

USB_DECLARE_DRIVER(uipaq);

USB_MATCH(uipaq)
{
	USB_MATCH_START(uipaq, uaa);

	DPRINTFN(20,("uipaq: vendor=0x%x, product=0x%x\n",
	    uaa->vendor, uaa->product));

	return (uipaq_lookup(uaa->vendor, uaa->product) != NULL ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

USB_ATTACH(uipaq)
{
	USB_ATTACH_START(uipaq, sc, uaa);
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char *devinfop;
	char *devname = USBDEVNAME(sc->sc_dev);
	int i;
	usbd_status err;
	struct ucom_attach_args uca;

	DPRINTFN(10,("\nuipaq_attach: sc=%p\n", sc));

	/* Move the device into the configured state. */
	err = usbd_set_config_no(dev, UIPAQ_CONFIG_NO, 1);
	if (err) {
		printf("\n%s: failed to set configuration, err=%s\n",
		    devname, usbd_errstr(err));
		goto bad;
	}

	err = usbd_device2interface_handle(dev, UIPAQ_IFACE_INDEX, &iface);
	if (err) {
		printf("\n%s: failed to get interface, err=%s\n",
		    devname, usbd_errstr(err));
		goto bad;
	}

	devinfop = usbd_devinfo_alloc(dev, 0);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", devname, devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_flags = uipaq_lookup(uaa->vendor, uaa->product)->uv_flags;

	id = usbd_get_interface_descriptor(iface);

	sc->sc_udev = dev;
	sc->sc_iface = iface;

	uca.ibufsize = UIPAQIBUFSIZE;
	uca.obufsize = UIPAQOBUFSIZE;
	uca.ibufsizepad = UIPAQIBUFSIZE;
	uca.opkthdrlen = 0;
	uca.device = dev;
	uca.iface = iface;
	uca.methods = &uipaq_methods;
	uca.arg = sc;
	uca.portno = UCOM_UNK_PORTNO;
	uca.info = "Generic";

/*	err = uipaq_init(sc);
	if (err) {
		printf("%s: init failed, %s\n", USBDEVNAME(sc->sc_dev),
		    usbd_errstr(err));
		goto bad;
	}*/

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

	uca.bulkin = uca.bulkout = -1;
	for (i=0; i<id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor for %d\n",
					devname,i);
			goto bad;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			uca.bulkin = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			uca.bulkout = ed->bEndpointAddress;
		}
	}
	if (uca.bulkin == -1 || uca.bulkout == -1) {
		printf("%s: no proper endpoints found (%d,%d) \n",
		    devname, uca.bulkin, uca.bulkout);
		USB_ATTACH_ERROR_RETURN;
	}

	sc->sc_subdev = config_found_sm_loc(self, "ucombus", NULL, &uca,
					    ucomprint, ucomsubmatch);

	USB_ATTACH_SUCCESS_RETURN;

bad:
	DPRINTF(("uipaq_attach: ATTACH ERROR\n"));
	sc->sc_dying = 1;
	USB_ATTACH_ERROR_RETURN;
}


void
uipaq_dtr(struct uipaq_softc* sc, int onoff)
{
	usb_device_request_t req;
	usbd_status err;
	int retries = 3;

	DPRINTF(("%s: uipaq_dtr: onoff=%x\n", USBDEVNAME(sc->sc_dev), onoff));

	/* Avoid sending unnecessary requests */
	if (onoff && (sc->sc_lcr & UCDC_LINE_DTR))
		return;
	if (!onoff && !(sc->sc_lcr & UCDC_LINE_DTR))
		return;

	/* Other parameters depend on reg */
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	sc->sc_lcr = onoff ? sc->sc_lcr | UCDC_LINE_DTR : sc->sc_lcr & ~UCDC_LINE_DTR;
	USETW(req.wValue, sc->sc_lcr);
	USETW(req.wIndex, 0x0);
	USETW(req.wLength, 0);

	/* Fire off the request a few times if necessary */
	while (retries) {
		err = usbd_do_request(sc->sc_udev, &req, NULL);
		if (!err)
			break;
		retries--;
	}
}


void
uipaq_rts(struct uipaq_softc* sc, int onoff)
{
	usb_device_request_t req;
	usbd_status err;
	int retries = 3;

	DPRINTF(("%s: uipaq_rts: onoff=%x\n", USBDEVNAME(sc->sc_dev), onoff));

	/* Avoid sending unnecessary requests */
	if (onoff && (sc->sc_lcr & UCDC_LINE_RTS)) return;
	if (!onoff && !(sc->sc_lcr & UCDC_LINE_RTS)) return;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	sc->sc_lcr = onoff ? sc->sc_lcr | UCDC_LINE_RTS : sc->sc_lcr & ~UCDC_LINE_RTS;
	USETW(req.wValue, sc->sc_lcr);
	USETW(req.wIndex, 0x0);
	USETW(req.wLength, 0);

	while (retries) {
		err = usbd_do_request(sc->sc_udev, &req, NULL);
		if (!err)
			break;
		retries--;
	}
}


void
uipaq_break(struct uipaq_softc* sc, int onoff)
{
	usb_device_request_t req;
	usbd_status err;
	int retries = 3;

	DPRINTF(("%s: uipaq_break: onoff=%x\n", USBDEVNAME(sc->sc_dev), onoff));

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_BREAK;

	USETW(req.wValue, onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);
	USETW(req.wIndex, 0x0);
	USETW(req.wLength, 0);

	while (retries) {
		err = usbd_do_request(sc->sc_udev, &req, NULL);
		if (!err)
			break;
		retries--;
	}
}


void
uipaq_set(void *addr, int portno, int reg, int onoff)
{
	struct uipaq_softc* sc = addr;

	switch (reg) {
	case UCOM_SET_DTR:
		uipaq_dtr(addr, onoff);
		break;
	case UCOM_SET_RTS:
		uipaq_rts(addr, onoff);
		break;
	case UCOM_SET_BREAK:
		uipaq_break(addr, onoff);
		break;
	default:
		printf("%s: unhandled set request: reg=%x onoff=%x\n",
		    USBDEVNAME(sc->sc_dev), reg, onoff);
		return;
	}
}


int
uipaq_activate(device_ptr_t self, enum devact act)
{
	struct uipaq_softc *sc = (struct uipaq_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_subdev != NULL)
			rv = config_deactivate(sc->sc_subdev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

int
uipaq_detach(device_ptr_t self, int flags)
{
	struct uipaq_softc *sc = (struct uipaq_softc *)self;
	int rv = 0;

	DPRINTF(("uipaq_detach: sc=%p flags=%d\n", sc, flags));
	sc->sc_dying = 1;
	if (sc->sc_subdev != NULL) {
		rv |= config_detach(sc->sc_subdev, flags);
		sc->sc_subdev = NULL;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

	return (rv);
}

