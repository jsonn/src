/*	$NetBSD: usb.c,v 1.95.4.1 2007/05/22 14:57:46 itohy Exp $	*/

/*-
 * Copyright (c) 1998, 2002 The NetBSD Foundation, Inc.
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
 * USB specifications and other documentation can be found at
 * http://www.usb.org/developers/docs/ and
 * http://www.usb.org/developers/devclass_docs/
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: usb.c,v 1.95.4.1 2007/05/22 14:57:46 itohy Exp $");
/* __FBSDID("$FreeBSD: src/sys/dev/usb/usb.c,v 1.111 2006/10/19 01:15:58 iedowse Exp $"); */

#if defined(__NetBSD__)
#include "opt_compat_netbsd.h"
#include "ohci.h"
#include "uhci.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#include <sys/vnode.h>
#elif defined(__FreeBSD__)
#if __FreeBSD_version >= 500000
#include <sys/mutex.h>
#endif
#include <sys/unistd.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/filio.h>
#include <sys/uio.h>
#endif
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/poll.h>
#if defined(__FreeBSD__) && __FreeBSD_version >= 500014
#include <sys/selinfo.h>
#else
#include <sys/select.h>
#endif
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#define USBUNIT(d)	(minor(d))	/* usb_discover device nodes, kthread */
#define USB_DEV_MINOR	255		/* event queue device */

MALLOC_DEFINE(M_USB, "USB", "USB");
MALLOC_DEFINE(M_USBDEV, "USBdev", "USB device");
MALLOC_DEFINE(M_USBHC, "USBHC", "USB host controller");

#if defined(__FreeBSD__)
#include "usb_if.h"
#endif /* defined(__FreeBSD__) */

#include <machine/bus.h>

#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
int	usbdebug = 0;
#if defined(UHCI_DEBUG) && NUHCI > 0
extern int	uhcidebug;
#endif
#if defined(OHCI_DEBUG) && NOHCI > 0
extern int	ohcidebug;
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

#ifdef __FreeBSD__
/* Define this unconditionally in case a kernel module is loaded that
 * has been compiled with debugging options.
 */
SYSCTL_NODE(_hw, OID_AUTO, usb, CTLFLAG_RW, 0, "USB debugging");
SYSCTL_INT(_hw_usb, OID_AUTO, debug, CTLFLAG_RW,
	   &usbdebug, 0, "usb debug level");
#endif

struct usb_softc {
	USBBASEDEVICE	sc_dev;		/* base device */
#ifdef __FreeBSD__
	struct cdev	*sc_usbdev;	/* /dev/usbN device */
	TAILQ_ENTRY(usb_softc) sc_coldexplist; /* cold needs-explore list */
#endif
	usbd_bus_handle sc_bus;		/* USB controller */
	struct usbd_port sc_port;	/* dummy port for root hub */

	struct proc	*sc_event_thread;

	char		sc_dying;
};

struct usb_taskq {
	TAILQ_HEAD(, usb_task) tasks;
	struct proc *task_thread_proc;
	const char *name;
	int taskcreated;	/* task thread exists. */
};

static struct usb_taskq usb_taskq[USB_NUM_TASKQS] = {
	{ TAILQ_HEAD_INITIALIZER(usb_taskq[0].tasks), NULL, NULL, 0 },
	{ TAILQ_HEAD_INITIALIZER(usb_taskq[1].tasks), NULL, NULL, 0 }
};

#if defined(__NetBSD__) || defined(__OpenBSD__)
dev_type_open(usbopen);
dev_type_close(usbclose);
dev_type_read(usbread);
dev_type_ioctl(usbioctl);
dev_type_poll(usbpoll);
dev_type_kqfilter(usbkqfilter);

const struct cdevsw usb_cdevsw = {
	usbopen, usbclose, usbread, nowrite, usbioctl,
	nostop, notty, usbpoll, nommap, usbkqfilter, D_OTHER,
};
#elif defined(__FreeBSD__)
d_open_t  usbopen;
d_close_t usbclose;
d_read_t usbread;
d_ioctl_t usbioctl;
d_poll_t usbpoll;

struct cdevsw usb_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	usbopen,
	.d_close =	usbclose,
	.d_read =	usbread,
	.d_ioctl =	usbioctl,
	.d_poll =	usbpoll,
	.d_name =	"usb",
#if __FreeBSD_version < 500014
	.d_bmaj =	-1
#endif
};
#endif

Static void	usb_discover(void *);
#ifdef __FreeBSD__
Static bus_child_detached_t usb_child_detached;
#endif
Static void	usb_create_event_thread(void *);
Static void	usb_event_thread(void *);
Static void	usb_task_thread(void *);

Static int usb_nbusses;			/* Number of /dev/usbN busses. */
#ifdef __FreeBSD__
Static struct cdev *usb_dev;		/* The /dev/usb device. */
/* Busses to explore at the end of boot-time device configuration. */
Static TAILQ_HEAD(, usb_softc) usb_coldexplist =
    TAILQ_HEAD_INITIALIZER(usb_coldexplist);
#endif

#define USB_MAX_EVENTS 100
struct usb_event_q {
	struct usb_event ue;
	TAILQ_ENTRY(usb_event_q) next;
};
Static TAILQ_HEAD(, usb_event_q) usb_events =
	TAILQ_HEAD_INITIALIZER(usb_events);
Static int usb_nevents = 0;
Static struct selinfo usb_selevent;
Static usb_sigproc_ptr usb_async_proc;  /* process that wants USB SIGIO */
Static int usb_dev_open = 0;
Static struct usb_event *usb_alloc_event(void);
Static void usb_free_event(struct usb_event *);
Static void usb_add_event(int, struct usb_event *);

Static int usb_get_next_event(struct usb_event *);

#if defined(__NetBSD__) && defined(COMPAT_30)
Static void usb_copy_old_devinfo(struct usb_device_info_old *, const struct usb_device_info *);
#endif

Static const char *usbrev_str[] = USBREV_STR;

#if defined(__NetBSD__) || defined(__OpenBSD__)
USB_DECLARE_DRIVER(usb);
#elif defined(__FreeBSD__)
USB_DECLARE_DRIVER_INIT(usb,
			DEVMETHOD(bus_child_detached, usb_child_detached),
			DEVMETHOD(device_suspend, bus_generic_suspend),
			DEVMETHOD(device_resume, bus_generic_resume),
			DEVMETHOD(device_shutdown, bus_generic_shutdown)
			);
MODULE_VERSION(usb, 1);
#endif

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
	int speed;
	struct usb_event *ue;

#ifdef __FreeBSD__
	sc->sc_dev = self;
#endif

	DPRINTF(("usbd_attach\n"));

	sc->sc_bus = aux;
	sc->sc_bus->usbctl = sc;
	sc->sc_port.power = USB_MAX_POWER;

#if defined(__FreeBSD__)
	printf("%s", USBDEVNAME(sc->sc_dev));
#endif
	usbrev = sc->sc_bus->usbrev;
	printf(": USB revision %s", usbrev_str[usbrev]);
	switch (usbrev) {
	case USBREV_1_0:
	case USBREV_1_1:
		speed = USB_SPEED_FULL;
		break;
	case USBREV_2_0:
		speed = USB_SPEED_HIGH;
		break;
	default:
		printf(", not supported\n");
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}
	printf("\n");

	/* Make sure not to use tsleep() if we are cold booting. */
	if (cold)
		sc->sc_bus->use_polling++;

	ue = usb_alloc_event();
	ue->u.ue_ctrlr.ue_bus = USBDEVUNIT(sc->sc_dev);
	usb_add_event(USB_EVENT_CTRLR_ATTACH, ue);

#ifdef USB_USE_SOFTINTR
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	/* XXX we should have our own level */
	sc->sc_bus->soft = softintr_establish(IPL_SOFTNET,
	    sc->sc_bus->methods->soft_intr, sc->sc_bus);
	if (sc->sc_bus->soft == NULL) {
		printf("%s: can't register softintr\n", USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}
#else
	usb_callout_init(sc->sc_bus->softi);
#endif
#endif

	err = usbd_new_device(USBDEV(sc->sc_dev), sc->sc_bus, 0, speed, 0,
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
#if defined(__FreeBSD__)
		if (cold) {
			/* Explore high-speed busses before others. */
			if (speed == USB_SPEED_HIGH)
				dev->hub->explore(sc->sc_bus->root_hub);
			else
				TAILQ_INSERT_TAIL(&usb_coldexplist, sc,
				    sc_coldexplist);
		}
#else
		if (cold && (device_cfdata(&sc->sc_dev)->cf_flags & 1))
			dev->hub->explore(sc->sc_bus->root_hub);
#endif
#endif
	} else {
		printf("%s: root hub problem, error=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		sc->sc_dying = 1;
	}
	if (cold)
		sc->sc_bus->use_polling--;

	config_pending_incr();
#if defined(__NetBSD__) || defined(__OpenBSD__)
	usb_kthread_create(usb_create_event_thread, sc);
#endif

	usb_nbusses++;
#if defined(__FreeBSD__)
	usb_create_event_thread(sc);
	/* The per controller devices (used for usb_discover) */
	/* XXX This is redundant now, but old usbd's will want it */
	sc->sc_usbdev = make_dev(&usb_cdevsw, device_get_unit(self), UID_ROOT,
	    GID_OPERATOR, 0660, "usb%d", device_get_unit(self));
	if (usb_nbusses == 1) {
		/* The device spitting out events */
		usb_dev = make_dev(&usb_cdevsw, USB_DEV_MINOR, UID_ROOT,
		    GID_OPERATOR, 0660, "usb");
	}
#endif

	USB_ATTACH_SUCCESS_RETURN;
}

static const char *taskq_names[] = USB_TASKQ_NAMES;

void
usb_create_event_thread(void *arg)
{
	struct usb_softc *sc = arg;
	struct usb_taskq *taskq;
	int i;

	if (usb_kthread_create1(usb_event_thread, sc, &sc->sc_event_thread,
			   "%s", USBDEVNAME(sc->sc_dev))) {
		printf("%s: unable to create event thread for\n",
		       USBDEVNAME(sc->sc_dev));
		panic("usb_create_event_thread");
	}
	for (i = 0; i < USB_NUM_TASKQS; i++) {
		taskq = &usb_taskq[i];

		if (taskq->taskcreated == 0) {
			taskq->taskcreated = 1;
			taskq->name = taskq_names[i];
			if (usb_kthread_create2(usb_task_thread, taskq,
			    &taskq->task_thread_proc, taskq->name)) {
				printf("unable to create task thread: %s\n",
				    taskq->name);
				panic("usb_create_event_thread task");
			}
		}
	}
}

/*
 * Add a task to be performed by the task thread.  This function can be
 * called from any context and the task will be executed in a process
 * context ASAP.
 */
void
usb_add_task(usbd_device_handle dev, struct usb_task *task, int queue)
{
	struct usb_taskq *taskq;
	int s;

	s = splusb();
	taskq = &usb_taskq[queue];
	if (task->queue == -1) {
		DPRINTFN(2,("usb_add_task: task=%p\n", task));
		TAILQ_INSERT_TAIL(&taskq->tasks, task, next);
		task->queue = queue;
	} else {
		DPRINTFN(3,("usb_add_task: task=%p on q\n", task));
	}
	wakeup(&taskq->tasks);
	splx(s);
}

void
usb_rem_task(usbd_device_handle dev, struct usb_task *task)
{
	struct usb_taskq *taskq;
	int s;

	s = splusb();
	if (task->queue != -1) {
		taskq = &usb_taskq[task->queue];
		TAILQ_REMOVE(&taskq->tasks, task, next);
		task->queue = -1;
	}
	splx(s);
}

void
usb_event_thread(void *arg)
{
	static int newthread_wchan;
	struct usb_softc *sc = arg;

#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	mtx_lock(&Giant);
#endif

	DPRINTF(("usb_event_thread: start\n"));

	/*
	 * In case this controller is a companion controller to an
	 * EHCI controller we need to wait until the EHCI controller
	 * has grabbed the port.  What we do here is wait until no new
	 * USB threads have been created in a while. XXX we actually
	 * just want to wait for the PCI slot to be fully scanned.
	 *
	 * Note that when you `kldload usb' it actually attaches the
	 * devices in order that the drivers appear in the kld, not the
	 * normal PCI order, since the addition of each driver within
	 * usb.ko (ohci, ehci etc.) causes a separate PCI bus re-scan.
	 */
	wakeup(&newthread_wchan);
	for (;;) {
		if (tsleep(&newthread_wchan , PWAIT, "usbets", hz * 4) != 0)
			break;
	}

	/* Make sure first discover does something. */
	sc->sc_bus->needs_explore = 1;
	usb_discover(sc);
	config_pending_decr();

	while (!sc->sc_dying) {
#ifdef USB_DEBUG
		if (usb_noexplore < 2)
#endif
		usb_discover(sc);
#ifdef USB_DEBUG
		(void)tsleep(&sc->sc_bus->needs_explore, PWAIT, "usbevt",
		    usb_noexplore ? 0 : hz * 60);
#else
		(void)tsleep(&sc->sc_bus->needs_explore, PWAIT, "usbevt",
		    hz * 60);
#endif
		DPRINTFN(2,("usb_event_thread: woke up\n"));
	}
	sc->sc_event_thread = NULL;

	/* In case parent is waiting for us to exit. */
	wakeup(sc);

	DPRINTF(("usb_event_thread: exit\n"));
	kthread_exit(0);
}

void
usb_task_thread(void *arg)
{
	struct usb_task *task;
	struct usb_taskq *taskq;
	int s;

#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
	mtx_lock(&Giant);
#endif

	taskq = arg;
	DPRINTF(("usb_task_thread: start taskq %s\n", taskq->name));

	s = splusb();

	while (usb_nbusses > 0) {
		task = TAILQ_FIRST(&taskq->tasks);
		if (task == NULL) {
			tsleep(&taskq->tasks, PWAIT, "usbtsk", 0);
			task = TAILQ_FIRST(&taskq->tasks);
		}
		DPRINTFN(2,("usb_task_thread: woke up task=%p\n", task));
		if (task != NULL) {
			TAILQ_REMOVE(&taskq->tasks, task, next);
			task->queue = -1;
			splx(s);
			task->fun(task->arg);
			s = splusb();
		}
	}

	splx(s);

	taskq->taskcreated = 0;
	wakeup(&taskq->taskcreated);

	DPRINTF(("usb_event_thread: exit\n"));
	kthread_exit(0);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
usbctlprint(void *aux, const char *pnp)
{
	/* only "usb"es can attach to host controllers */
	if (pnp)
		aprint_normal("usb at %s", pnp);

	return (UNCONF);
}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

int
usbopen(usb_cdev_t dev, int flag, int mode, usb_proc_ptr p)
{
	int unit = USBUNIT(dev);
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
usbread(usb_cdev_t dev, struct uio *uio, int flag)
{
	struct usb_event *ue;
#if defined(__NetBSD__) && defined(COMPAT_30)
	struct usb_event_old *ueo = NULL;	/* XXXGCC */
	int useold = 0;
#endif
	int s, error, n;

	if (USBUNIT(dev) != USB_DEV_MINOR)
		return (ENXIO);

	switch (uio->uio_resid) {
#if defined(__NetBSD__) && defined(COMPAT_30)
	case sizeof(struct usb_event_old):
		ueo = malloc(sizeof(struct usb_event_old), M_USBDEV,
			     M_WAITOK|M_ZERO);
		useold = 1;
		/* FALLTHRU */
#endif
	case sizeof(struct usb_event):
		ue = usb_alloc_event();
		break;
	default:
		return (EINVAL);
	}

	error = 0;
	s = splusb();
	for (;;) {
		n = usb_get_next_event(ue);
		if (n != 0)
			break;
		if (flag &
#ifdef __FreeBSD__
		    O_NONBLOCK
#else
		    IO_NDELAY
#endif
		    ) {
			error = EWOULDBLOCK;
			break;
		}
		error = tsleep(&usb_events, PZERO | PCATCH, "usbrea", 0);
		if (error)
			break;
	}
	splx(s);
	if (!error) {
#if defined(__NetBSD__) && defined(COMPAT_30)
		if (useold) { /* copy fields to old struct */
			ueo->ue_type = ue->ue_type;
			memcpy(&ueo->ue_time, &ue->ue_time,
			      sizeof(struct timespec));
			switch (ue->ue_type) {
				case USB_EVENT_DEVICE_ATTACH:
				case USB_EVENT_DEVICE_DETACH:
					usb_copy_old_devinfo(&ueo->u.ue_device, &ue->u.ue_device);
					break;

				case USB_EVENT_CTRLR_ATTACH:
				case USB_EVENT_CTRLR_DETACH:
					ueo->u.ue_ctrlr.ue_bus=ue->u.ue_ctrlr.ue_bus;
					break;

				case USB_EVENT_DRIVER_ATTACH:
				case USB_EVENT_DRIVER_DETACH:
					ueo->u.ue_driver.ue_cookie=ue->u.ue_driver.ue_cookie;
					memcpy(ueo->u.ue_driver.ue_devname,
					       ue->u.ue_driver.ue_devname,  
					       sizeof(ue->u.ue_driver.ue_devname));
					break;
				default:
					;
			}

			error = uiomove((void *)ueo, uio->uio_resid, uio);
		} else
#endif
			error = uiomove((void *)ue, uio->uio_resid, uio);
	}
	usb_free_event(ue);
#if defined(__NetBSD__) && defined(COMPAT_30)
	if (useold)
		free(ueo, M_USBDEV);
#endif

	return (error);
}

int
usbclose(usb_cdev_t dev, int flag, int mode, usb_proc_ptr p)
{
	int unit = USBUNIT(dev);

	if (unit == USB_DEV_MINOR) {
		usb_async_proc = 0;
		usb_dev_open = 0;
	}

	return (0);
}

int
usbioctl(usb_cdev_t devt, u_long cmd, caddr_t data, int flag, usb_proc_ptr p)
{
	struct usb_softc *sc;
	int unit = USBUNIT(devt);

	if (unit == USB_DEV_MINOR) {
		switch (cmd) {
		case FIONBIO:
			/* All handled in the upper FS layer. */
			return (0);

		case FIOASYNC:
			if (*(int *)data)
#if defined(__NetBSD__) && __NetBSD_Version__ >= 399001400
				usb_async_proc = p->l_proc;
#elif defined(__FreeBSD__) && __FreeBSD_version >= 500000
				usb_async_proc = p->td_proc;
#else
				usb_async_proc = p;
#endif
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
#if defined(USB_DEBUG)
	case USB_SETDEBUG:
		if (!(flag & FWRITE))
			return (EBADF);
		usbdebug  = ((*(int *)data) & 0x000000ff);
#if defined(UHCI_DEBUG) && NUHCI > 0
		uhcidebug = ((*(int *)data) & 0x0000ff00) >> 8;
#endif
#if defined(OHCI_DEBUG) && NOHCI > 0
		ohcidebug = ((*(int *)data) & 0x00ff0000) >> 16;
#endif
		break;
#endif /* USB_DEBUG */
#if defined(__FreeBSD__)
	/* This part should be deleted */
  	case USB_DISCOVER:
  		break;
#endif
	case USB_REQUEST:
	{
		struct usb_ctl_request *ur = (void *)data;
		int len = UGETW(ur->ucr_request.wLength);
		struct iovec iov;
		struct uio uio;
		void *ptr = 0;
		int addr = ur->ucr_addr;
		usbd_status err;
		int error = 0;

		if (!(flag & FWRITE))
			return (EBADF);

		DPRINTF(("usbioctl: USB_REQUEST addr=%d len=%d\n", addr, len));
		if (len < 0 || len > 32768)
			return (EINVAL);
		if (addr < 0 || addr >= USB_MAX_DEVICES ||
		    sc->sc_bus->devices[addr] == 0)
			return (EINVAL);
		if (len != 0) {
			iov.iov_base = (caddr_t)ur->ucr_data;
			iov.iov_len = len;
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_resid = len;
			uio.uio_offset = 0;
			uio.uio_rw =
				ur->ucr_request.bmRequestType & UT_READ ?
				UIO_READ : UIO_WRITE;
#ifdef __NetBSD__
			uio.uio_vmspace = p->l_proc->p_vmspace;
#endif
#ifdef __FreeBSD__
			uio.uio_segflg = UIO_USERSPACE;
			uio.uio_td = p;
#endif
			ptr = malloc(len, M_TEMP, M_WAITOK);
			if (uio.uio_rw == UIO_WRITE) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
		err = usbd_do_request_flags(sc->sc_bus->devices[addr],
			  &ur->ucr_request, ptr, ur->ucr_flags, &ur->ucr_actlen,
			  USBD_DEFAULT_TIMEOUT);
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
		usbd_device_handle dev;
		struct usb_device_info *di = (void *)data;
		int addr = di->udi_addr;

		if (addr < 1 || addr >= USB_MAX_DEVICES)
			return EINVAL;
		if ((dev = sc->sc_bus->devices[addr]) == NULL)
			return ENXIO;
		usbd_fill_deviceinfo(dev, di, 1);
		break;
	}

#if defined(__NetBSD__) && defined(COMPAT_30)
	case USB_DEVICEINFO_OLD:
	{
		usbd_device_handle dev;
		struct usb_device_info_old *di = (void *)data;
		int addr = di->udi_addr;

		if (addr < 1 || addr >= USB_MAX_DEVICES)
			return EINVAL;
		if ((dev = sc->sc_bus->devices[addr]) == NULL)
			return ENXIO;
		usbd_fill_deviceinfo_old(dev, di, 1);
		break;
	}
#endif

	case USB_DEVICESTATS:
		*(struct usb_device_stats *)data = sc->sc_bus->stats;
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

int
usbpoll(usb_cdev_t dev, int events, usb_proc_ptr p)
{
	int revents, mask, s;
	int unit = USBUNIT(dev);

	if (unit == USB_DEV_MINOR) {
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
		return (0);
	}
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
static void
filt_usbrdetach(struct knote *kn)
{
	int s;

	s = splusb();
	SLIST_REMOVE(&usb_selevent.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

static int
filt_usbread(struct knote *kn, long hint)
{

	if (usb_nevents == 0)
		return (0);

	kn->kn_data = sizeof(struct usb_event);
	return (1);
}

static const struct filterops usbread_filtops =
	{ 1, NULL, filt_usbrdetach, filt_usbread };

int
usbkqfilter(dev_t dev, struct knote *kn)
{
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		if (minor(dev) != USB_DEV_MINOR)
			return (1);
		klist = &usb_selevent.sel_klist;
		kn->kn_fop = &usbread_filtops;
		break;

	default:
		return (1);
	}

	kn->kn_hook = NULL;

	s = splusb();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return (0);
}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

/* Explore device tree from the root. */
Static void
usb_discover(void *v)
{
	struct usb_softc *sc = v;
	/* splxxx should be changed to mutexes for preemption safety some day */
	int s;

	DPRINTFN(2,("usb_discover\n"));
#ifdef USB_DEBUG
	if (usb_noexplore > 1)
		return;
#endif

	/*
	 * We need mutual exclusion while traversing the device tree,
	 * but this is guaranteed since this function is only called
	 * from the event thread for the controller.
	 */
	s = splusb();
	while (sc->sc_bus->needs_explore && !sc->sc_dying) {
		sc->sc_bus->needs_explore = 0;
		splx(s);
		sc->sc_bus->root_hub->hub->explore(sc->sc_bus->root_hub);
		s = splusb();
	}
	splx(s);
}

void
usb_needs_explore(usbd_device_handle dev)
{
	DPRINTFN(2,("usb_needs_explore\n"));
	dev->bus->needs_explore = 1;
	wakeup(&dev->bus->needs_explore);
}

void
usb_needs_reattach(usbd_device_handle dev)
{
	DPRINTFN(2,("usb_needs_reattach\n"));
	dev->powersrc->reattach = 1;
	dev->bus->needs_explore = 1;
	wakeup(&dev->bus->needs_explore);
}

/* Called at splusb() */
int
usb_get_next_event(struct usb_event *ue)
{
	struct usb_event_q *ueq;

	if (usb_nevents <= 0)
		return (0);
	ueq = TAILQ_FIRST(&usb_events);
#ifdef DIAGNOSTIC
	if (ueq == NULL) {
		printf("usb: usb_nevents got out of sync! %d\n", usb_nevents);
		usb_nevents = 0;
		return (0);
	}
#endif
	if (ue)
		*ue = ueq->ue;
	TAILQ_REMOVE(&usb_events, ueq, next);
	usb_free_event((struct usb_event *)(void *)ueq);
	usb_nevents--;
	return (1);
}

void
usbd_add_dev_event(int type, usbd_device_handle udev)
{
	struct usb_event *ue = usb_alloc_event();

	usbd_fill_deviceinfo(udev, &ue->u.ue_device, USB_EVENT_IS_ATTACH(type));
	usb_add_event(type, ue);
}

void
usbd_add_drv_event(int type, usbd_device_handle udev, device_ptr_t dev)
{
	struct usb_event *ue = usb_alloc_event();

	ue->u.ue_driver.ue_cookie = udev->cookie;
	strncpy(ue->u.ue_driver.ue_devname, USBDEVPTRNAME(dev),
	    sizeof ue->u.ue_driver.ue_devname);
	usb_add_event(type, ue);
}

Static struct usb_event *
usb_alloc_event(void)
{
	/* Yes, this is right; we allocate enough so that we can use it later */
	return malloc(sizeof(struct usb_event_q), M_USBDEV, M_WAITOK|M_ZERO);
}

Static void
usb_free_event(struct usb_event *uep)
{
	free(uep, M_USBDEV);
}

Static void
usb_add_event(int type, struct usb_event *uep)
{
	struct usb_event_q *ueq;
	struct timeval thetime;
	int s;

	microtime(&thetime);
	/* Don't want to wait here inside splusb() */
	ueq = (struct usb_event_q *)(void *)uep;
	ueq->ue = *uep;
	ueq->ue.ue_type = type;
	TIMEVAL_TO_TIMESPEC(&thetime, &ueq->ue.ue_time);

	s = splusb();
	if (USB_EVENT_IS_DETACH(type)) {
		struct usb_event_q *ueqi, *ueqi_next;

		for (ueqi = TAILQ_FIRST(&usb_events); ueqi; ueqi = ueqi_next) {
			ueqi_next = TAILQ_NEXT(ueqi, next);
			if (ueqi->ue.u.ue_driver.ue_cookie.cookie ==
			    uep->u.ue_device.udi_cookie.cookie) {
				TAILQ_REMOVE(&usb_events, ueqi, next);
				free(ueqi, M_USBDEV);
				usb_nevents--;
				ueqi_next = TAILQ_FIRST(&usb_events);
			}
		}
	}
	if (usb_nevents >= USB_MAX_EVENTS) {
		/* Too many queued events, drop an old one. */
		DPRINTFN(-1,("usb: event dropped\n"));
		(void)usb_get_next_event(0);
	}
	TAILQ_INSERT_TAIL(&usb_events, ueq, next);
	usb_nevents++;
	wakeup(&usb_events);
#if defined(__NetBSD__) || defined(__OpenBSD__)
	selnotify(&usb_selevent, 0);
#elif defined(__FreeBSD__)
	selwakeuppri(&usb_selevent, PZERO);
#endif
	if (usb_async_proc != NULL) {
		USB_PROC_LOCK(usb_async_proc);
		psignal(usb_async_proc, SIGIO);
		USB_PROC_UNLOCK(usb_async_proc);
	}
	splx(s);
}

void
usb_schedsoftintr(usbd_bus_handle bus)
{
	DPRINTFN(10,("usb_schedsoftintr: polling=%d\n", bus->use_polling));
#ifdef USB_USE_SOFTINTR
	if (bus->use_polling) {
		bus->methods->soft_intr(bus);
	} else {
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
		softintr_schedule(bus->soft);
#else
		if (!callout_pending(&bus->softi))
			callout_reset(&bus->softi, 0, bus->methods->soft_intr,
			    bus);
#endif /* __HAVE_GENERIC_SOFT_INTERRUPTS */
	}
#else
	bus->methods->soft_intr(bus);
#endif /* USB_USE_SOFTINTR */
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
usb_activate(device_ptr_t self, enum devact act)
{
	struct usb_softc *sc = (struct usb_softc *)self;
	usbd_device_handle dev = sc->sc_port.device;
	int i, rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		if (dev != NULL && dev->cdesc != NULL && dev->subdevs != NULL) {
			for (i = 0; dev->subdevs[i]; i++)
				rv |= config_deactivate(dev->subdevs[i]);
		}
		break;
	}
	return (rv);
}
#endif

USB_DETACH(usb)
{
	USB_DETACH_START(usb, sc);
	struct usb_event *ue;
	struct usb_taskq *taskq;
	int i;

	DPRINTF(("usb_detach: start\n"));

	sc->sc_dying = 1;

	/* Make all devices disconnect. */
	if (sc->sc_port.device != NULL)
		usb_disconnect_port(&sc->sc_port, self);

	/* Kill off event thread. */
	if (sc->sc_event_thread != NULL) {
		wakeup(&sc->sc_bus->needs_explore);
		if (tsleep(sc, PWAIT, "usbdet", hz * 60))
			printf("%s: event thread didn't die\n",
			       USBDEVNAME(sc->sc_dev));
		DPRINTF(("usb_detach: event thread dead\n"));
	}

#ifdef __FreeBSD__
	destroy_dev(sc->sc_usbdev);
#endif
	if (--usb_nbusses == 0) {
#ifdef __FreeBSD__
		destroy_dev(usb_dev);
		usb_dev = NULL;
#endif
		for (i = 0; i < USB_NUM_TASKQS; i++) {
			taskq = &usb_taskq[i];
			wakeup(&taskq->tasks);
			if (tsleep(&taskq->taskcreated, PWAIT, "usbtdt",
			    hz * 60)) {
				printf("usb task thread %s didn't die\n",
				    taskq->name);
			}
		}
	}

#ifdef USB_USE_SOFTINTR
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	if (sc->sc_bus->soft != NULL) {
		softintr_disestablish(sc->sc_bus->soft);
		sc->sc_bus->soft = NULL;
	}
#else
	callout_stop(&sc->sc_bus->softi);
#endif
#endif

	ue = usb_alloc_event();
	ue->u.ue_ctrlr.ue_bus = USBDEVUNIT(sc->sc_dev);
	usb_add_event(USB_EVENT_CTRLR_DETACH, ue);

	return (0);
}

#if defined(__NetBSD__) && defined(COMPAT_30)
Static void
usb_copy_old_devinfo(struct usb_device_info_old *uo,
		     const struct usb_device_info *ue)
{
	const unsigned char *p;
	unsigned char *q;
	int i, n;

	uo->udi_bus = ue->udi_bus;
	uo->udi_addr = ue->udi_addr;       
	uo->udi_cookie = ue->udi_cookie;
	for (i = 0, p = (const unsigned char *)ue->udi_product,
	     q = (unsigned char *)uo->udi_product;
	     *p && i < USB_MAX_STRING_LEN - 1; p++) {
		if (*p < 0x80)
			q[i++] = *p;
		else {
			q[i++] = '?';
			if ((*p & 0xe0) == 0xe0)
				p++;
			p++;
		}
	}
	q[i] = 0;

	for (i = 0, p = ue->udi_vendor, q = uo->udi_vendor;
	     *p && i < USB_MAX_STRING_LEN - 1; p++) {
		if (* p < 0x80)
			q[i++] = *p;
		else {
			q[i++] = '?';
			p++;
			if ((*p & 0xe0) == 0xe0)
				p++;
		}
	}
	q[i] = 0;

	memcpy(uo->udi_release, ue->udi_release, sizeof(uo->udi_release));

	uo->udi_productNo = ue->udi_productNo;
	uo->udi_vendorNo = ue->udi_vendorNo;
	uo->udi_releaseNo = ue->udi_releaseNo;
	uo->udi_class = ue->udi_class;
	uo->udi_subclass = ue->udi_subclass;
	uo->udi_protocol = ue->udi_protocol;
	uo->udi_config = ue->udi_config;
	uo->udi_speed = ue->udi_speed;
	uo->udi_power = ue->udi_power;    
	uo->udi_nports = ue->udi_nports;

	for (n=0; n<USB_MAX_DEVNAMES; n++)
		memcpy(uo->udi_devnames[n],
		       ue->udi_devnames[n], USB_MAX_DEVNAMELEN);
	memcpy(uo->udi_ports, ue->udi_ports, sizeof(uo->udi_ports));
}
#endif

#if defined(__FreeBSD__)
Static void
usb_child_detached(device_t self, device_t child)
{
	struct usb_softc *sc = device_get_softc(self);

	/* XXX, should check it is the right device. */
	sc->sc_port.device = NULL;
}

/* Explore USB busses at the end of device configuration. */
Static void
usb_cold_explore(void *arg)
{
	struct usb_softc *sc;

	USB_KASSERT2(cold || TAILQ_EMPTY(&usb_coldexplist),
	    ("usb_cold_explore: busses to explore when !cold"));
	while (!TAILQ_EMPTY(&usb_coldexplist)) {
		sc = TAILQ_FIRST(&usb_coldexplist);
		TAILQ_REMOVE(&usb_coldexplist, sc, sc_coldexplist);

		sc->sc_bus->use_polling++;
		sc->sc_port.device->hub->explore(sc->sc_bus->root_hub);
		sc->sc_bus->use_polling--;
	}
}

DRIVER_MODULE(usb, ohci, usb_driver, usb_devclass, 0, 0);
DRIVER_MODULE(usb, uhci, usb_driver, usb_devclass, 0, 0);
DRIVER_MODULE(usb, ehci, usb_driver, usb_devclass, 0, 0);
DRIVER_MODULE(usb, slhci, usb_driver, usb_devclass, 0, 0);
SYSINIT(usb_cold_explore, SI_SUB_CONFIGURE, SI_ORDER_MIDDLE,
    usb_cold_explore, NULL);
#endif
