/*	$NetBSD: uhid.c,v 1.15.4.1 1999/06/21 01:19:27 thorpej Exp $	*/

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
 * HID spec: http://www.usb.org/developers/data/usbhid10.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__)
#include <sys/device.h>
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <sys/ioccom.h>
#include <sys/filio.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#endif
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/hid.h>
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (uhiddebug) printf x
#define DPRINTFN(n,x)	if (uhiddebug>(n)) printf x
int	uhiddebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct uhid_softc {
	bdevice sc_dev;		/* base device */
	usbd_interface_handle sc_iface;	/* interface */
	usbd_pipe_handle sc_intrpipe;	/* interrupt pipe */
	int sc_ep_addr;

	int sc_isize;
	int sc_osize;
	int sc_fsize;
	u_int8_t sc_iid;
	u_int8_t sc_oid;
	u_int8_t sc_fid;

	char *sc_ibuf;
	char *sc_obuf;

	void *sc_repdesc;
	int sc_repdesc_size;

	struct clist sc_q;
	struct selinfo sc_rsel;
	u_char sc_state;	/* driver state */
#define	UHID_OPEN	0x01	/* device is open */
#define	UHID_ASLP	0x02	/* waiting for device data */
#define UHID_NEEDCLEAR	0x04	/* needs clearing endpoint stall */
#define UHID_IMMED	0x08	/* return read data immediately */
	int sc_disconnected;	/* device is gone */
};

#define	UHIDUNIT(dev)	(minor(dev))
#define	UHID_CHUNK	128	/* chunk size for read */
#define	UHID_BSIZE	1020	/* buffer size */

int uhidopen __P((dev_t, int, int, struct proc *));
int uhidclose __P((dev_t, int, int, struct proc *p));
int uhidread __P((dev_t, struct uio *uio, int));
int uhidwrite __P((dev_t, struct uio *uio, int));
int uhidioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int uhidpoll __P((dev_t, int, struct proc *));
void uhid_intr __P((usbd_request_handle, usbd_private_handle, usbd_status));
void uhid_disco __P((void *));

USB_DECLARE_DRIVER(uhid);

USB_MATCH(uhid)
{
	USB_MATCH_START(uhid, uaa);
	usb_interface_descriptor_t *id;
	
	if (!uaa->iface)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (!id || id->bInterfaceClass != UCLASS_HID)
		return (UMATCH_NONE);
	return (UMATCH_IFACECLASS_GENERIC);
}

USB_ATTACH(uhid)
{
	USB_ATTACH_START(uhid, sc, uaa);
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int size;
	void *desc;
	usbd_status r;
	char devinfo[1024];
	
	sc->sc_disconnected = 1;
	sc->sc_iface = iface;
	id = usbd_get_interface_descriptor(iface);
	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s, iclass %d/%d\n", USBDEVNAME(sc->sc_dev),
	       devinfo, id->bInterfaceClass, id->bInterfaceSubClass);

	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (!ed) {
		printf("%s: could not read endpoint descriptor\n",
		       USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	DPRINTFN(10,("uhid_attach: bLength=%d bDescriptorType=%d "
		     "bEndpointAddress=%d-%s bmAttributes=%d wMaxPacketSize=%d"
		     " bInterval=%d\n",
		     ed->bLength, ed->bDescriptorType, 
		     ed->bEndpointAddress & UE_ADDR,
		     ed->bEndpointAddress & UE_IN ? "in" : "out",
		     ed->bmAttributes & UE_XFERTYPE,
		     UGETW(ed->wMaxPacketSize), ed->bInterval));

	if ((ed->bEndpointAddress & UE_IN) != UE_IN ||
	    (ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		printf("%s: unexpected endpoint\n", USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->sc_ep_addr = ed->bEndpointAddress;
	sc->sc_disconnected = 0;

	r = usbd_alloc_report_desc(uaa->iface, &desc, &size, M_USB);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: no report descriptor\n", USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}
	
	(void)usbd_set_idle(iface, 0, 0);

	sc->sc_isize = hid_report_size(desc, size, hid_input,   &sc->sc_iid);
	sc->sc_osize = hid_report_size(desc, size, hid_output,  &sc->sc_oid);
	sc->sc_fsize = hid_report_size(desc, size, hid_feature, &sc->sc_fid);

	sc->sc_repdesc = desc;
	sc->sc_repdesc_size = size;

	USB_ATTACH_SUCCESS_RETURN;
}

#if defined(__FreeBSD__)
static int
uhid_detach(device_t self)
{       
        struct uhid_softc *sc = device_get_softc(self);
	char *devinfo = (char *) device_get_desc(self);

	if (devinfo) {
		device_set_desc(self, NULL);
		free(devinfo, M_USB);
	}
	return 0;
}
#endif

void
uhid_disco(p)
	void *p;
{
	struct uhid_softc *sc = p;

	DPRINTF(("ums_hid: sc=%p\n", sc));
	usbd_abort_pipe(sc->sc_intrpipe);
	sc->sc_disconnected = 1;
}

void
uhid_intr(reqh, addr, status)
	usbd_request_handle reqh;
	usbd_private_handle addr;
	usbd_status status;
{
	struct uhid_softc *sc = addr;

	DPRINTFN(5, ("uhid_intr: status=%d\n", status));
	DPRINTFN(5, ("uhid_intr: data = %02x %02x %02x\n",
		     sc->sc_ibuf[0], sc->sc_ibuf[1], sc->sc_ibuf[2]));

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uhid_intr: status=%d\n", status));
		sc->sc_state |= UHID_NEEDCLEAR;
		return;
	}

	(void) b_to_q(sc->sc_ibuf, sc->sc_isize, &sc->sc_q);
		
	if (sc->sc_state & UHID_ASLP) {
		sc->sc_state &= ~UHID_ASLP;
		DPRINTFN(5, ("uhid_intr: waking %p\n", sc));
		wakeup((caddr_t)sc);
	}
	selwakeup(&sc->sc_rsel);
}

int
uhidopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	usbd_status r;
	USB_GET_SC_OPEN(uhid, UHIDUNIT(dev), sc);

	DPRINTF(("uhidopen: sc=%p, disco=%d\n", sc, sc->sc_disconnected));

	if (sc->sc_disconnected)
		return (EIO);

	if (sc->sc_state & UHID_OPEN)
		return (EBUSY);
	sc->sc_state |= UHID_OPEN;

#if defined(__NetBSD__)
	if (clalloc(&sc->sc_q, UHID_BSIZE, 0) == -1) {
		sc->sc_state &= ~UHID_OPEN;
		return (ENOMEM);
	}
#elif defined(__FreeBSD__)
	clist_alloc_cblocks(&sc->sc_q, UHID_BSIZE, 0);
#endif

	sc->sc_ibuf = malloc(sc->sc_isize, M_USB, M_WAITOK);
	sc->sc_obuf = malloc(sc->sc_osize, M_USB, M_WAITOK);

	/* Set up interrupt pipe. */
	r = usbd_open_pipe_intr(sc->sc_iface, sc->sc_ep_addr, 
				USBD_SHORT_XFER_OK,
				&sc->sc_intrpipe, sc, sc->sc_ibuf, 
				sc->sc_isize, uhid_intr);
	if (r != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uhidopen: usbd_open_pipe_intr failed, "
			 "error=%d\n",r));
		free(sc->sc_ibuf, M_USB);
		free(sc->sc_obuf, M_USB);
		sc->sc_state &= ~UHID_OPEN;
		return (EIO);
	}

	sc->sc_state &= ~UHID_IMMED;

	usbd_set_disco(sc->sc_intrpipe, uhid_disco, sc);

	return (0);
}

int
uhidclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	if (sc->sc_disconnected)
		return (EIO);

	DPRINTF(("uhidclose: sc=%p\n", sc));

	/* Disable interrupts. */
	usbd_abort_pipe(sc->sc_intrpipe);
	usbd_close_pipe(sc->sc_intrpipe);

#if defined(__NetBSD__)
	clfree(&sc->sc_q);
#elif defined(__FreeBSD__)
	clist_free_cblocks(&sc->sc_q);
#endif

	free(sc->sc_ibuf, M_USB);
	free(sc->sc_obuf, M_USB);

	sc->sc_state &= ~UHID_OPEN;

	return (0);
}

int
uhidread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int s;
	int error = 0;
	size_t length;
	u_char buffer[UHID_CHUNK];
	usbd_status r;
	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	if (sc->sc_disconnected)
		return (EIO);

	DPRINTFN(1, ("uhidread\n"));
	if (sc->sc_state & UHID_IMMED) {
		DPRINTFN(1, ("uhidread immed\n"));
		
		r = usbd_get_report(sc->sc_iface, UHID_INPUT_REPORT,
				    sc->sc_iid, buffer, sc->sc_isize);
		if (r != USBD_NORMAL_COMPLETION)
			return (EIO);
		return (uiomove(buffer, sc->sc_isize, uio));
	}

	s = splusb();
	while (sc->sc_q.c_cc == 0) {
		if (flag & IO_NDELAY) {
			splx(s);
			return EWOULDBLOCK;
		}
		sc->sc_state |= UHID_ASLP;
		DPRINTFN(5, ("uhidread: sleep on %p\n", sc));
		error = tsleep((caddr_t)sc, PZERO | PCATCH, "uhidrea", 0);
		DPRINTFN(5, ("uhidread: woke, error=%d\n", error));
		if (error) {
			sc->sc_state &= ~UHID_ASLP;
			splx(s);
			return (error);
		}
		if (sc->sc_state & UHID_NEEDCLEAR) {
			DPRINTFN(-1,("uhidread: clearing stall\n"));
			sc->sc_state &= ~UHID_NEEDCLEAR;
			usbd_clear_endpoint_stall(sc->sc_intrpipe);
		}
	}
	splx(s);

	/* Transfer as many chunks as possible. */
	while (sc->sc_q.c_cc > 0 && uio->uio_resid > 0) {
		length = min(sc->sc_q.c_cc, uio->uio_resid);
		if (length > sizeof(buffer))
			length = sizeof(buffer);

		/* Remove a small chunk from the input queue. */
		(void) q_to_b(&sc->sc_q, buffer, length);
		DPRINTFN(5, ("uhidread: got %d chars\n", length));

		/* Copy the data to the user process. */
		if ((error = uiomove(buffer, length, uio)) != 0)
			break;
	}

	return (error);
}

int
uhidwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int error;
	int size;
	usbd_status r;
	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	if (sc->sc_disconnected)
		return (EIO);

	DPRINTFN(1, ("uhidwrite\n"));
	
	size = sc->sc_osize;
	error = 0;
	while (uio->uio_resid > 0) {
		if (uio->uio_resid != size)
			return (0);
		if ((error = uiomove(sc->sc_obuf, size, uio)) != 0)
			break;
		if (sc->sc_oid)
			r = usbd_set_report(sc->sc_iface, UHID_OUTPUT_REPORT,
					    sc->sc_obuf[0], 
					    sc->sc_obuf+1, size-1);
		else
			r = usbd_set_report(sc->sc_iface, UHID_OUTPUT_REPORT,
					    0, sc->sc_obuf, size);
		if (r != USBD_NORMAL_COMPLETION) {
			error = EIO;
			break;
		}
	}
	return (error);
}

int
uhidioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct usb_ctl_report_desc *rd;
	struct usb_ctl_report *re;
	int size, id;
	usbd_status r;
	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	if (sc->sc_disconnected)
		return (EIO);

	DPRINTFN(2, ("uhidioctl: cmd=%lx\n", cmd));
	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		break;

	case USB_GET_REPORT_DESC:
		rd = (struct usb_ctl_report_desc *)addr;
		size = min(sc->sc_repdesc_size, sizeof rd->data);
		rd->size = size;
		memcpy(rd->data, sc->sc_repdesc, size);
		break;

	case USB_SET_IMMED:
		if (*(int *)addr) {
			/* XXX should read into ibuf, but does it matter */
			r = usbd_get_report(sc->sc_iface, UHID_INPUT_REPORT,
					    sc->sc_iid, sc->sc_ibuf, 
					    sc->sc_isize);
			if (r != USBD_NORMAL_COMPLETION)
				return (EOPNOTSUPP);

			sc->sc_state |=  UHID_IMMED;
		} else
			sc->sc_state &= ~UHID_IMMED;
		break;

	case USB_GET_REPORT:
		re = (struct usb_ctl_report *)addr;
		switch (re->report) {
		case UHID_INPUT_REPORT:
			size = sc->sc_isize;
			id = sc->sc_iid;
			break;
		case UHID_OUTPUT_REPORT:
			size = sc->sc_osize;
			id = sc->sc_oid;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->sc_fsize;
			id = sc->sc_fid;
			break;
		default:
			return (EINVAL);
		}
		r = usbd_get_report(sc->sc_iface, re->report, id, 
				    re->data, size);
		if (r != USBD_NORMAL_COMPLETION)
			return (EIO);
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

int
uhidpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	int revents = 0;
	int s;
	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	if (sc->sc_disconnected)
		return (EIO);

	s = splusb();
	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_q.c_cc > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &sc->sc_rsel);
	}

	splx(s);
	return (revents);
}

#if defined(__FreeBSD__)
DRIVER_MODULE(uhid, usb, uhid_driver, uhid_devclass, usbd_driver_load, 0);
#endif
