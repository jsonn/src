/*	$NetBSD: ulpt.c,v 1.11.4.2 1999/08/02 22:08:59 thorpej Exp $	*/

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

/*
 * Printer Class spec: http://www.usb.org/developers/data/usbprn10.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#if defined(__NetBSD__)
#include <sys/device.h>
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <sys/ioccom.h>
#include <sys/module.h>
#include <sys/bus.h>
#endif
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/syslog.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#define	TIMEOUT		hz*16	/* wait up to 16 seconds for a ready */
#define	STEP		hz/4

#define	LPTPRI		(PZERO+8)
#define	ULPT_BSIZE	1024

#ifdef USB_DEBUG
#define DPRINTF(x)	if (ulptdebug) printf x
#define DPRINTFN(n,x)	if (ulptdebug>(n)) printf x
int	ulptdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UR_GET_DEVICE_ID 0
#define UR_GET_PORT_STATUS 1
#define UR_SOFT_RESET 2

#define	LPS_NERR		0x08	/* printer no error */
#define	LPS_SELECT		0x10	/* printer selected */
#define	LPS_NOPAPER		0x20	/* printer out of paper */
#define LPS_INVERT      (LPS_SELECT|LPS_NERR)
#define LPS_MASK        (LPS_SELECT|LPS_NERR|LPS_NOPAPER)

struct ulpt_softc {
	bdevice sc_dev;
	usbd_device_handle sc_udev;	/* device */
	usbd_interface_handle sc_iface;	/* interface */
	int sc_ifaceno;
	usbd_pipe_handle sc_bulkpipe;	/* bulk pipe */
	int sc_bulk;

	u_char sc_state;
#define	ULPT_OPEN	0x01	/* device is open */
#define	ULPT_OBUSY	0x02	/* printer is busy doing output */
#define	ULPT_INIT	0x04	/* waiting to initialize for open */
	u_char sc_flags;
#define	ULPT_NOPRIME	0x40	/* don't prime on open */
	u_char sc_laststatus;

	int sc_refcnt;
	u_char sc_dying;
};

int ulptopen __P((dev_t, int, int, struct proc *));
int ulptclose __P((dev_t, int, int, struct proc *p));
int ulptwrite __P((dev_t, struct uio *uio, int));
int ulptioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
void ulpt_disco __P((void *));

int ulpt_do_write __P((struct ulpt_softc *, struct uio *uio, int));
int ulpt_status __P((struct ulpt_softc *));
void ulpt_reset __P((struct ulpt_softc *));
int ulpt_statusmsg __P((u_char, struct ulpt_softc *));

#define	ULPTUNIT(s)	(minor(s) & 0x1f)
#define	ULPTFLAGS(s)	(minor(s) & 0xe0)

USB_DECLARE_DRIVER(ulpt);

USB_MATCH(ulpt)
{
	USB_MATCH_START(ulpt, uaa);
	usb_interface_descriptor_t *id;
	
	DPRINTFN(10,("ulpt_match\n"));
	if (!uaa->iface)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id &&
	    id->bInterfaceClass == UCLASS_PRINTER &&
	    id->bInterfaceSubClass == USUBCLASS_PRINTER &&
	    (id->bInterfaceProtocol == UPROTO_PRINTER_UNI ||
	     id->bInterfaceProtocol == UPROTO_PRINTER_BI))
		return (UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);
	return (UMATCH_NONE);
}

USB_ATTACH(ulpt)
{
	USB_ATTACH_START(ulpt, sc, uaa);
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id = usbd_get_interface_descriptor(iface);
#if 0
	usb_config_descriptor_t *cd = usbd_get_config_descriptor(dev);
	usb_device_request_t req;
#endif
	char devinfo[1024];
	usb_endpoint_descriptor_t *ed;
	usbd_status r;
	
	DPRINTFN(10,("ulpt_attach: sc=%p\n", sc));
	usbd_devinfo(dev, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s, iclass %d/%d\n", USBDEVNAME(sc->sc_dev),
	       devinfo, id->bInterfaceClass, id->bInterfaceSubClass);

	/* Figure out which endpoint is the bulk out endpoint. */
	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (!ed)
		goto nobulk;
	if ((ed->bEndpointAddress & UE_IN) != UE_OUT ||
	    (ed->bmAttributes & UE_XFERTYPE) != UE_BULK) {
		/* In case we are using a bidir protocol... */
		ed = usbd_interface2endpoint_descriptor(iface, 1);
		if (!ed)
			goto nobulk;
		if ((ed->bEndpointAddress & UE_IN) != UE_OUT ||
		    (ed->bmAttributes & UE_XFERTYPE) != UE_BULK)
			goto nobulk;
	}
	sc->sc_bulk = ed->bEndpointAddress;
	DPRINTFN(10, ("ulpt_attach: bulk=%d\n", sc->sc_bulk));

	sc->sc_iface = iface;
	r = usbd_interface2device_handle(iface, &sc->sc_udev);
	if (r != USBD_NORMAL_COMPLETION) {
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}
	sc->sc_ifaceno = id->bInterfaceNumber;

#if 0
XXX needs a different way to read the id string since the length
is unknown.  usbd_do_request() returns error on a short transfer.
	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_DEVICE_ID;
	USETW(req.wValue, cd->bConfigurationValue);
	USETW2(req.wIndex, id->bInterfaceNumber, id->bAlternateSetting);
	USETW(req.wLength, sizeof devinfo - 1);
	r = usbd_do_request(dev, &req, devinfo);
	if (r == USBD_NORMAL_COMPLETION) {
		int len;
		char *idstr;
		len = (devinfo[0] << 8) | (devinfo[1] & 0xff);
		/* devinfo now contains an IEEE-1284 device ID */
		idstr = devinfo+2;
		idstr[len] = 0;
		printf("%s: device id <%s>\n", USBDEVNAME(sc->sc_dev), idstr);
	} else {
		printf("%s: cannot get device id\n", USBDEVNAME(sc->sc_dev));
	}
#endif

	USB_ATTACH_SUCCESS_RETURN;

 nobulk:
	printf("%s: could not find bulk endpoint\n", USBDEVNAME(sc->sc_dev));
	sc->sc_dying = 1;
	USB_ATTACH_ERROR_RETURN;
}

int
ulpt_activate(self, act)
	struct device *self;
	enum devact act;
{
	return (0);
}

int
ulpt_detach(self, flags)
	struct device  *self;
	int flags;
{
	struct ulpt_softc *sc = (struct ulpt_softc *)self;
	int maj, mn;
	int s;

	DPRINTF(("ulpt_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;
	if (sc->sc_bulkpipe)
		usbd_abort_pipe(sc->sc_bulkpipe);

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* There is noone to wake, aborting the pipe is enough */
		/* Wait for processes to go away. */
		usb_detach_wait(&sc->sc_dev);
	}
	splx(s);

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == ulptopen)
			break;

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

	return (0);
}

int
ulpt_status(sc)
	struct ulpt_softc *sc;
{
	usb_device_request_t req;
	usbd_status r;
	u_char status;

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_PORT_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, 1);
	r = usbd_do_request(sc->sc_udev, &req, &status);
	DPRINTFN(1, ("ulpt_status: status=0x%02x r=%d\n", status, r));
	if (r == USBD_NORMAL_COMPLETION)
		return (status);
	else
		return (0);
}

void
ulpt_reset(sc)
	struct ulpt_softc *sc;
{
	usb_device_request_t req;

	DPRINTFN(1, ("ulpt_reset\n"));
	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	req.bRequest = UR_SOFT_RESET;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, 0);
	(void)usbd_do_request(sc->sc_udev, &req, 0);
}

/*
 * Reset the printer, then wait until it's selected and not busy.
 */
int
ulptopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	u_char flags = ULPTFLAGS(dev);
	usbd_status r;
	int spin, error;
	USB_GET_SC_OPEN(ulpt, ULPTUNIT(dev), sc);

	if (!sc || !sc->sc_iface || sc->sc_dying)
		return (ENXIO);

	if (sc->sc_state)
		return (EBUSY);

	sc->sc_state = ULPT_INIT;
	sc->sc_flags = flags;
	DPRINTF(("ulptopen: flags=0x%x\n", (unsigned)flags));

	if ((flags & ULPT_NOPRIME) == 0)
		ulpt_reset(sc);

	for (spin = 0; (ulpt_status(sc) & LPS_SELECT) == 0; spin += STEP) {
		if (spin >= TIMEOUT) {
			sc->sc_state = 0;
			return (EBUSY);
		}

		/* wait 1/4 second, give up if we get a signal */
		error = tsleep((caddr_t)sc, LPTPRI | PCATCH, "ulptop", STEP);
		if (error != EWOULDBLOCK) {
			sc->sc_state = 0;
			return (error);
		}
	}

	r = usbd_open_pipe(sc->sc_iface, sc->sc_bulk, 0, &sc->sc_bulkpipe);
	if (r != USBD_NORMAL_COMPLETION) {
		sc->sc_state = 0;
		return (EIO);
	}

	sc->sc_state = ULPT_OPEN;

	DPRINTF(("ulptopen: done\n"));
	return (0);
}

int
ulpt_statusmsg(status, sc)
	u_char status;
	struct ulpt_softc *sc;
{
	u_char new;

	status = (status ^ LPS_INVERT) & LPS_MASK;
	new = status & ~sc->sc_laststatus;
	sc->sc_laststatus = status;

	if (new & LPS_SELECT)
		log(LOG_NOTICE, "%s: offline\n", USBDEVNAME(sc->sc_dev));
	else if (new & LPS_NOPAPER)
		log(LOG_NOTICE, "%s: out of paper\n", USBDEVNAME(sc->sc_dev));
	else if (new & LPS_NERR)
		log(LOG_NOTICE, "%s: output error\n", USBDEVNAME(sc->sc_dev));

	return (status);
}

int
ulptclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	USB_GET_SC(ulpt, ULPTUNIT(dev), sc);

	if (sc->sc_state != ULPT_OPEN)
		/* We are being forced to close before the open completed. */
		return (0);

	usbd_close_pipe(sc->sc_bulkpipe);
	sc->sc_bulkpipe = 0;

	sc->sc_state = 0;

	DPRINTF(("ulptclose: closed\n"));
	return (0);
}

int
ulpt_do_write(sc, uio, flags)
	struct ulpt_softc *sc;
	struct uio *uio;
	int flags;
{
	size_t n;
	int error = 0;
	char buf[ULPT_BSIZE];
	usbd_request_handle reqh;
	usbd_status r;

	DPRINTF(("ulptwrite\n"));
	reqh = usbd_alloc_request();
	if (reqh == 0)
		return (ENOMEM);
	while ((n = min(ULPT_BSIZE, uio->uio_resid)) != 0) {
		ulpt_statusmsg(ulpt_status(sc), sc);
		error = uiomove(buf, n, uio);
		if (error)
			break;
		/* XXX use callback to enable interrupt? */
		r = usbd_setup_request(reqh, sc->sc_bulkpipe, 0, buf, n,
				       0, USBD_NO_TIMEOUT, 0);
		if (r != USBD_NORMAL_COMPLETION) {
			error = EIO;
			break;
		}
		DPRINTFN(1, ("ulptwrite: transfer %d bytes\n", n));
		r = usbd_sync_transfer(reqh);
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTF(("ulptwrite: error=%d\n", r));
			usbd_clear_endpoint_stall(sc->sc_bulkpipe);
			error = EIO;
			break;
		}
	}
	usbd_free_request(reqh);

	return (error);
}

int
ulptwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	USB_GET_SC(ulpt, ULPTUNIT(dev), sc);
	int error;

	sc->sc_refcnt++;
	error = ulpt_do_write(sc, uio, flags);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(&sc->sc_dev);
	return (error);
}

int
ulptioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error = 0;

	switch (cmd) {
	default:
		error = ENODEV;
	}

	return error;
}

#if defined(__FreeBSD__)
DRIVER_MODULE(ulpt, usb, ulpt_driver, ulpt_devclass, usbd_driver_load, 0);
#endif
