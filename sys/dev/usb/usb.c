/*	$NetBSD: usb.c,v 1.25.2.1 1999/12/27 18:35:44 wrstuden Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/usb.c,v 1.20 1999/11/17 22:33:46 n_hibma Exp $	*/

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
 * USB specifications and other documentation can be found at
 * http://www.usb.org/developers/data/ and
 * http://www.usb.org/developers/index.html .
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/filio.h>
#include <sys/uio.h>
#endif
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#define USB_DEV_MINOR 255

#if defined(__FreeBSD__)
MALLOC_DEFINE(M_USB, "USB", "USB");
MALLOC_DEFINE(M_USBDEV, "USBdev", "USB device");
MALLOC_DEFINE(M_USBHC, "USBHC", "USB host controller");

#include "usb_if.h"
#endif /* defined(__FreeBSD__) */

#include <machine/bus.h>

#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
int	usbdebug = 0;
#ifdef UHCI_DEBUG
int	uhcidebug;
#endif
#ifdef OHCI_DEBUG
int	ohcidebug;
#endif
/* 
 * 0  - do usual exploration
 * 1  - do not use timeout exploration
 * >1 - do no exploration
 */
int	usb_noexplore = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct usb_softc {
	USBBASEDEVICE	sc_dev;		/* base device */
	usbd_bus_handle sc_bus;		/* USB controller */
	struct usbd_port sc_port;	/* dummy port for root hub */

#if defined(__FreeBSD__)
	/* This part should be deleted when kthreads is available */
	struct selinfo	sc_consel;	/* waiting for connect change */
#else
	struct proc    *sc_event_thread;
#endif

	char		sc_dying;
};

#if defined(__NetBSD__) || defined(__OpenBSD__)
cdev_decl(usb);
#elif defined(__FreeBSD__)
d_open_t  usbopen; 
d_close_t usbclose;
d_read_t usbread;
d_ioctl_t usbioctl;
int usbpoll __P((dev_t, int, struct proc *));

struct cdevsw usb_cdevsw = {
	/* open */      usbopen,
	/* close */     usbclose,
	/* read */      noread,
	/* write */     nowrite,
	/* ioctl */     usbioctl,
	/* poll */      usbpoll,
	/* mmap */      nommap,
	/* strategy */  nostrategy,
	/* name */      "usb",
	/* maj */       USB_CDEV_MAJOR,
	/* dump */      nodump,
	/* psize */     nopsize,
	/* flags */     0,
	/* bmaj */      -1
};
#endif

static usbd_status usb_discover __P((struct usb_softc *));
static void	usb_create_event_thread __P((void *));
static void	usb_event_thread __P((void *));

#define USB_MAX_EVENTS 50
struct usb_event_q {
	struct usb_event ue;
	SIMPLEQ_ENTRY(usb_event_q) next;
};
static SIMPLEQ_HEAD(, usb_event_q) usb_events = 
	SIMPLEQ_HEAD_INITIALIZER(usb_events);
static int usb_nevents = 0;
static struct selinfo usb_selevent;
static struct proc *usb_async_proc;  /* process who wants USB SIGIO */
static int usb_dev_open = 0;

static int usb_get_next_event __P((struct usb_event *));

#if defined(__NetBSD__) || defined(__OpenBSD__)
/* Flag to see if we are in the cold boot process. */
extern int cold;
#endif

static const char *usbrev_str[] = USBREV_STR;

USB_DECLARE_DRIVER(usb);

USB_MATCH(usb)
{
	DPRINTF(("usbd_match\n"));
	return (UMATCH_GENERIC);
}

USB_ATTACH(usb)
{
#if defined(__NetBSD__) || defined(__OpenBSD__)
	struct usb_softc *sc = (struct usb_softc *)self;
#elif defined(__FreeBSD__)
	struct usb_softc *sc = device_get_softc(self);
	void *aux = device_get_ivars(self);
#endif
	usbd_device_handle dev;
	usbd_status err;
	int usbrev;
	
#if defined(__FreeBSD__)
	printf("%s", USBDEVNAME(sc->sc_dev));
	sc->sc_dev = self;
#endif

	DPRINTF(("usbd_attach\n"));

	usbd_init();
	sc->sc_bus = aux;
	sc->sc_bus->usbctl = sc;
	sc->sc_port.power = USB_MAX_POWER;

	usbrev = sc->sc_bus->usbrev;
	printf(": USB revision %s", usbrev_str[usbrev]);
	if (usbrev != USBREV_1_0 && usbrev != USBREV_1_1) {
		printf(", not supported\n");
		USB_ATTACH_ERROR_RETURN;
	}
	printf("\n");

	/* Make sure not to use tsleep() if we are cold booting. */
	if (cold)
		sc->sc_bus->use_polling++;

	err = usbd_new_device(USBDEV(sc->sc_dev), sc->sc_bus, 0, 0, 0,
		  &sc->sc_port);
	if (!err) {
		dev = sc->sc_port.device;
		if (dev->hub == NULL) {
			sc->sc_dying = 1;
			printf("%s: root device is not a hub\n", 
			       USBDEVNAME(sc->sc_dev));
			USB_ATTACH_ERROR_RETURN;
		}
		sc->sc_bus->root_hub = dev;
#if 1
		/* 
		 * Turning this code off will delay attachment of USB devices
		 * until the USB event thread is running, which means that
		 * the keyboard will not work until after cold boot.
		 */
		if (cold)
			dev->hub->explore(sc->sc_bus->root_hub);
#endif
	} else {
		printf("%s: root hub problem, error=%d\n", 
		       USBDEVNAME(sc->sc_dev), err); 
		sc->sc_dying = 1;
	}
	if (cold)
		sc->sc_bus->use_polling--;

	kthread_create(usb_create_event_thread, sc);

#if defined(__FreeBSD__)
	make_dev(&usb_cdevsw, device_get_unit(self), UID_ROOT, GID_OPERATOR,
		 0644, "usb%d", device_get_unit(self));
#endif

	USB_ATTACH_SUCCESS_RETURN;
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
void
usb_create_event_thread(arg)
	void *arg;
{
	struct usb_softc *sc = arg;

	if (kthread_create1(usb_event_thread, sc, &sc->sc_event_thread,
			   "%s", sc->sc_dev.dv_xname)) {
		printf("%s: unable to create event thread for\n",
		       sc->sc_dev.dv_xname);
		panic("usb_create_event_thread");
	}
}

void
usb_event_thread(arg)
	void *arg;
{
	struct usb_softc *sc = arg;

	DPRINTF(("usb_event_thread: start\n"));

	while (!sc->sc_dying) {
#ifdef USB_DEBUG
		if (usb_noexplore < 2)
#endif
		usb_discover(sc);
		(void)tsleep(&sc->sc_bus->needs_explore, PWAIT, "usbevt",
#ifdef USB_DEBUG
			     usb_noexplore ? 0 :
#endif
			     hz*60
			);
		DPRINTFN(2,("usb_event_thread: woke up\n"));
	}
	sc->sc_event_thread = 0;

	/* In case parent is waiting for us to exit. */
	wakeup(sc);

	DPRINTF(("usb_event_thread: exit\n"));
	kthread_exit(0);
}

int
usbctlprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	/* only "usb"es can attach to host controllers */
	if (pnp)
		printf("usb at %s", pnp);

	return (UNCONF);
}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

int
usbopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = minor(dev);
	struct usb_softc *sc;

	if (unit == USB_DEV_MINOR) {
		if (usb_dev_open)
			return (EBUSY);
		usb_dev_open = 1;
		usb_async_proc = 0;
		return (0);
	}

	USB_GET_SC_OPEN(usb, unit, sc);

	if (sc->sc_dying)
		return (EIO);

	return (0);
}

int
usbread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct usb_event ue;
	int s, error, n;

	if (minor(dev) != USB_DEV_MINOR)
		return (ENXIO);

	if (uio->uio_resid != sizeof(struct usb_event))
		return (EINVAL);

	error = 0;
	s = splusb();
	for (;;) {
		n = usb_get_next_event(&ue);
		if (n != 0)
			break;
		if (flag & IO_NDELAY) {
			error = EWOULDBLOCK;
			break;
		}
		error = tsleep(&usb_events, PZERO | PCATCH, "usbrea", 0);
		if (error)
			break;
	}
	splx(s);
	if (!error)
		error = uiomove((void *)&ue, uio->uio_resid, uio);

	return (error);
}

int
usbclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = minor(dev);

	if (unit == USB_DEV_MINOR) {
		usb_async_proc = 0;
		usb_dev_open = 0;
	}

	return (0);
}

int
usbioctl(devt, cmd, data, flag, p)
	dev_t devt;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct usb_softc *sc;
	int unit = minor(devt);

	if (unit == USB_DEV_MINOR) {
		switch (cmd) {
		case FIONBIO:
			/* All handled in the upper FS layer. */
			return (0);
			
		case FIOASYNC:
			if (*(int *)data)
				usb_async_proc = p;
			else
				usb_async_proc = 0;
			return (0);

		default:
			return (EINVAL);
		}
	}

	USB_GET_SC(usb, unit, sc);

	if (sc->sc_dying)
		return (EIO);

	switch (cmd) {
#if defined(__FreeBSD__) 
	/* This part should be deleted when kthreads is available */
  	case USB_DISCOVER:
  		usb_discover(sc);
  		break;
#endif
#ifdef USB_DEBUG
	case USB_SETDEBUG:
		usbdebug  = ((*(int *)data) & 0x000000ff);
#ifdef UHCI_DEBUG
		uhcidebug = ((*(int *)data) & 0x0000ff00) >> 8;
#endif
#ifdef OHCI_DEBUG
		ohcidebug = ((*(int *)data) & 0x00ff0000) >> 16;
#endif
		break;
#endif
	case USB_REQUEST:
	{
		struct usb_ctl_request *ur = (void *)data;
		int len = UGETW(ur->request.wLength);
		struct iovec iov;
		struct uio uio;
		void *ptr = 0;
		int addr = ur->addr;
		usbd_status err;
		int error = 0;

		DPRINTF(("usbioctl: USB_REQUEST addr=%d len=%d\n", addr, len));
		if (len < 0 || len > 32768)
			return (EINVAL);
		if (addr < 0 || addr >= USB_MAX_DEVICES || 
		    sc->sc_bus->devices[addr] == 0)
			return (EINVAL);
		if (len != 0) {
			iov.iov_base = (caddr_t)ur->data;
			iov.iov_len = len;
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_resid = len;
			uio.uio_offset = 0;
			uio.uio_segflg = UIO_USERSPACE;
			uio.uio_rw =
				ur->request.bmRequestType & UT_READ ? 
				UIO_READ : UIO_WRITE;
			uio.uio_procp = p;
			ptr = malloc(len, M_TEMP, M_WAITOK);
			if (uio.uio_rw == UIO_WRITE) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
		err = usbd_do_request_flags(sc->sc_bus->devices[addr],
			  &ur->request, ptr, ur->flags, &ur->actlen);
		if (err) {
			error = EIO;
			goto ret;
		}
		if (len != 0) {
			if (uio.uio_rw == UIO_READ) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
	ret:
		if (ptr)
			free(ptr, M_TEMP);
		return (error);
	}

	case USB_DEVICEINFO:
	{
		struct usb_device_info *di = (void *)data;
		int addr = di->addr;
		usbd_device_handle dev;

		if (addr < 1 || addr >= USB_MAX_DEVICES)
			return (EINVAL);
		dev = sc->sc_bus->devices[addr];
		if (dev == NULL)
			return (ENXIO);
		usbd_fill_deviceinfo(dev, di);
		break;
	}

	case USB_DEVICESTATS:
		*(struct usb_device_stats *)data = sc->sc_bus->stats;
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

int
usbpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	int revents, mask, s;

	if (minor(dev) == USB_DEV_MINOR) {
		revents = 0;
		mask = POLLIN | POLLRDNORM;
		
		s = splusb();
		if (events & mask && usb_nevents > 0)
			revents |= events & mask;
		if (revents == 0 && events & mask)
			selrecord(p, &usb_selevent);
		splx(s);
		
		return (revents);
	} else {
#if defined(__FreeBSD__)
		/* This part should be deleted when kthreads is available */
		struct usb_softc *sc;
		int unit = minor(dev);

		USB_GET_SC(usb, unit, sc);

		revents = 0;
		mask = POLLOUT | POLLRDNORM;

		s = splusb();
		if (events & mask && sc->sc_bus->needs_explore)
			revents |= events & mask;
		if (revents == 0 && events & mask)
			selrecord(p, &sc->sc_consel);
		splx(s);

		return (revents);
#else
		return (ENXIO);
#endif
	}
}

/* Explore device tree from the root. */
usbd_status
usb_discover(sc)
	struct usb_softc *sc;
{
#if defined(__FreeBSD__)
	/* The splxxx parts should be deleted when kthreads is available */
	int s;
#endif

	/* 
	 * We need mutual exclusion while traversing the device tree,
	 * but this is guaranteed since this function is only called
	 * from the event thread for the controller.
	 */
#if defined(__FreeBSD__)
	s = splusb();
#endif
	while (sc->sc_bus->needs_explore && !sc->sc_dying) {
		sc->sc_bus->needs_explore = 0;
#if defined(__FreeBSD__)
		splx(s);
#endif
		sc->sc_bus->root_hub->hub->explore(sc->sc_bus->root_hub);
#if defined(__FreeBSD__)
		s = splusb();
#endif
	}
#if defined(__FreeBSD__)
	splx(s);
#endif

	return (USBD_NORMAL_COMPLETION);
}

void
usb_needs_explore(bus)
	usbd_bus_handle bus;
{
	bus->needs_explore = 1;
#if defined(__FreeBSD__)
	/* This part should be deleted when kthreads is available */
	selwakeup(&bus->usbctl->sc_consel);
#endif
	wakeup(&bus->needs_explore);
}

/* Called at splusb() */
int
usb_get_next_event(ue)
	struct usb_event *ue;
{
	struct usb_event_q *ueq;

	if (usb_nevents <= 0)
		return (0);
	ueq = SIMPLEQ_FIRST(&usb_events);
	*ue = ueq->ue;
	SIMPLEQ_REMOVE_HEAD(&usb_events, ueq, next);
	free(ueq, M_USBDEV);
	usb_nevents--;
	return (1);
}

void
usbd_add_event(type, dev)
	int type;
	usbd_device_handle dev;
{
	struct usb_event_q *ueq;
	struct usb_event ue;
	struct timeval thetime;
	int s;

	s = splusb();
	if (++usb_nevents >= USB_MAX_EVENTS) {
		/* Too many queued events, drop an old one. */
		DPRINTFN(-1,("usb: event dropped\n"));
		(void)usb_get_next_event(&ue);
	}
	/* Don't want to wait here inside splusb() */
	ueq = malloc(sizeof *ueq, M_USBDEV, M_NOWAIT);
	if (ueq == NULL) {
		printf("usb: no memory, event dropped\n");
		splx(s);
		return;
	}
	ueq->ue.ue_type = type;
	ueq->ue.ue_cookie = dev->cookie;
	usbd_fill_deviceinfo(dev, &ueq->ue.ue_device);
	microtime(&thetime);
	TIMEVAL_TO_TIMESPEC(&thetime, &ueq->ue.ue_time);
	SIMPLEQ_INSERT_TAIL(&usb_events, ueq, next);
	wakeup(&usb_events);
	selwakeup(&usb_selevent);
	if (usb_async_proc != NULL)
		psignal(usb_async_proc, SIGIO);
	splx(s);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
usb_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	struct usb_softc *sc = (struct usb_softc *)self;
	usbd_device_handle dev = sc->sc_port.device;
	int i, rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		if (dev && dev->cdesc && dev->subdevs) {
			for (i = 0; dev->subdevs[i]; i++)
				rv |= config_deactivate(dev->subdevs[i]);
		}
		break;
	}
	return (rv);
}

int
usb_detach(self, flags)
	device_ptr_t self;
	int flags;
{
	struct usb_softc *sc = (struct usb_softc *)self;

	DPRINTF(("usb_detach: start\n"));

	sc->sc_dying = 1;

	/* Make all devices disconnect. */
	if (sc->sc_port.device)
		usb_disconnect_port(&sc->sc_port, self);

	/* Kill off event thread. */
	if (sc->sc_event_thread) {
		wakeup(&sc->sc_bus->needs_explore);
		if (tsleep(sc, PWAIT, "usbdet", hz * 60))
			printf("%s: event thread didn't die\n",
			       USBDEVNAME(sc->sc_dev));
		DPRINTF(("usb_detach: event thread dead\n"));
	}

	usbd_finish();
	return (0);
}
#elif defined(__FreeBSD__)
int
usb_detach(device_t self)
{
	DPRINTF(("%s: unload, prevented\n", USBDEVNAME(self)));

	return (EINVAL);
}
#endif


#if defined(__FreeBSD__)
DRIVER_MODULE(usb, ohci, usb_driver, usb_devclass, 0, 0);
DRIVER_MODULE(usb, uhci, usb_driver, usb_devclass, 0, 0);
#endif
