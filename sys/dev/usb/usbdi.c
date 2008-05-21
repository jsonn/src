/*	$NetBSD: usbdi.c,v 1.119.12.6 2008/05/21 05:04:03 itohy Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/usbdi.c,v 1.99 2006/11/27 18:39:02 marius Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: usbdi.c,v 1.119.12.6 2008/05/21 05:04:03 itohy Exp $");
/* __FBSDID("$FreeBSD: src/sys/dev/usb/usbdi.c,v 1.99 2006/11/27 18:39:02 marius Exp $"); */

#ifdef __NetBSD__
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/kernel.h>
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include "usb_if.h"
#if defined(DIAGNOSTIC) && defined(__i386__)
#include <machine/cpu.h>
#endif
#endif
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
/* #include <dev/usb/usb_mem.h> */
#include <dev/usb/usb_quirks.h>

#if defined(__FreeBSD__)
#include "usb_if.h"
#define delay(d)	DELAY(d)
#endif

/* UTF-8 encoding stuff */
#include <fs/unicode.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

Static usbd_xfer_handle usbd_alloc_xfer_flag(usbd_device_handle,
	usbd_pipe_handle pipe, enum usbd_waitflg);
Static void usbd_free_buffer_flag(usbd_xfer_handle, enum usbd_waitflg);
Static usbd_status usbd_free_xfer_flag(usbd_xfer_handle, enum usbd_waitflg);
Static usbd_status usbd_ar_pipe(usbd_pipe_handle pipe);
Static void usbd_callback_task(void *);
Static void usbd_do_request_async_cb
	(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void usbd_do_request_async_task(void *);
Static void usbd_start_next(usbd_pipe_handle pipe);
Static usbd_status usbd_open_pipe_ival
	(usbd_interface_handle, u_int8_t, u_int8_t, usbd_pipe_handle *, int);

int
usbd_xfer_isread(usbd_xfer_handle xfer)
{
	if (xfer->rqflags & URQ_REQUEST)
		return (xfer->request.bmRequestType & UT_READ);
	else
		return (xfer->pipe->endpoint->edesc->bEndpointAddress &
			UE_DIR_IN);
}

#ifdef USB_DEBUG
void
usbd_dump_iface(struct usbd_interface *iface)
{
	printf("usbd_dump_iface: iface=%p\n", iface);
	if (iface == NULL)
		return;
	printf(" device=%p idesc=%p index=%d altindex=%d priv=%p\n",
	       iface->device, iface->idesc, iface->index, iface->altindex,
	       iface->priv);
}

void
usbd_dump_device(struct usbd_device *dev)
{
	printf("usbd_dump_device: dev=%p\n", dev);
	if (dev == NULL)
		return;
	printf(" bus=%p default_pipe=%p\n", dev->bus, dev->default_pipe);
	printf(" address=%d config=%d depth=%d speed=%d self_powered=%d "
	       "power=%d langid=%d\n",
	       dev->address, dev->config, dev->depth, dev->speed,
	       dev->self_powered, dev->power, dev->langid);
}

void
usbd_dump_endpoint(struct usbd_endpoint *endp)
{
	printf("usbd_dump_endpoint: endp=%p\n", endp);
	if (endp == NULL)
		return;
	printf(" edesc=%p refcnt=%d\n", endp->edesc, endp->refcnt);
	if (endp->edesc)
		printf(" bEndpointAddress=0x%02x\n",
		       endp->edesc->bEndpointAddress);
}

void
usbd_dump_queue(usbd_pipe_handle pipe)
{
	usbd_xfer_handle xfer;

	printf("usbd_dump_queue: pipe=%p\n", pipe);
	SIMPLEQ_FOREACH(xfer, &pipe->queue, next) {
		printf("  xfer=%p\n", xfer);
	}
}

void
usbd_dump_pipe(usbd_pipe_handle pipe)
{
	printf("usbd_dump_pipe: pipe=%p\n", pipe);
	if (pipe == NULL)
		return;
	usbd_dump_iface(pipe->iface);
	usbd_dump_device(pipe->device);
	usbd_dump_endpoint(pipe->endpoint);
	printf(" (usbd_dump_pipe:)\n refcnt=%d running=%d aborting=%d\n",
	       pipe->refcnt, pipe->running, pipe->aborting);
	printf(" intrxfer=%p, repeat=%d, interval=%d\n",
	       pipe->intrxfer, pipe->repeat, pipe->interval);
}
#endif

usbd_status
usbd_open_pipe(usbd_interface_handle iface, u_int8_t address,
	       u_int8_t flags, usbd_pipe_handle *pipe)
{
	return (usbd_open_pipe_ival(iface, address, flags, pipe,
				    USBD_DEFAULT_INTERVAL));
}

usbd_status
usbd_open_pipe_ival(usbd_interface_handle iface, u_int8_t address,
		    u_int8_t flags, usbd_pipe_handle *pipe, int ival)
{
	usbd_pipe_handle p;
	struct usbd_endpoint *ep;
	usbd_status err;
	int i;

	DPRINTFN(3,("usbd_open_pipe: iface=%p address=0x%x flags=0x%x\n",
		    iface, address, flags));

	for (i = 0; i < iface->idesc->bNumEndpoints; i++) {
		ep = &iface->endpoints[i];
		if (ep->edesc == NULL)
			return (USBD_IOERROR);
		if (ep->edesc->bEndpointAddress == address)
			goto found;
	}
	return (USBD_BAD_ADDRESS);
 found:
	if ((flags & USBD_EXCLUSIVE_USE) && ep->refcnt != 0)
		return (USBD_IN_USE);
	err = usbd_setup_pipe(iface->device, iface, ep, ival, &p);
	if (err)
		return (err);
	LIST_INSERT_HEAD(&iface->pipes, p, next);
	*pipe = p;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_open_pipe_intr(usbd_interface_handle iface, u_int8_t address,
		    u_int8_t flags, usbd_pipe_handle *pipe,
		    usbd_private_handle priv, void *buffer, u_int32_t len,
		    usbd_callback cb, int ival)
{
	usbd_status err;
	usbd_xfer_handle xfer;
	usbd_pipe_handle ipipe;

	DPRINTFN(3,("usbd_open_pipe_intr: address=0x%x flags=0x%x len=%d\n",
		    address, flags, len));

	err = usbd_open_pipe_ival(iface, address, USBD_EXCLUSIVE_USE,
				  &ipipe, ival);
	if (err)
		return (err);
	xfer = usbd_alloc_xfer(iface->device, ipipe);
	if (xfer == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	usbd_setup_xfer(xfer, ipipe, priv, buffer, len, flags,
	    USBD_NO_TIMEOUT, cb);
	ipipe->intrxfer = xfer;
	ipipe->repeat = 1;
	err = usbd_transfer(xfer);
	*pipe = ipipe;
	if (err != USBD_IN_PROGRESS && err)
		goto bad2;
	return (USBD_NORMAL_COMPLETION);

 bad2:
	ipipe->intrxfer = NULL;
	ipipe->repeat = 0;
	usbd_free_xfer(xfer);
 bad1:
	usbd_close_pipe(ipipe);
	return (err);
}

usbd_status
usbd_close_pipe(usbd_pipe_handle pipe)
{
#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		printf("usbd_close_pipe: pipe==NULL\n");
		return (USBD_NORMAL_COMPLETION);
	}
#endif

	if (--pipe->refcnt != 0)
		return (USBD_NORMAL_COMPLETION);
	if (! SIMPLEQ_EMPTY(&pipe->queue))
		return (USBD_PENDING_REQUESTS);
	LIST_REMOVE(pipe, next);
	pipe->endpoint->refcnt--;
	pipe->methods->close(pipe);
	if (pipe->intrxfer != NULL)
		usbd_free_xfer(pipe->intrxfer);
	free(pipe, M_USB);
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_transfer(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	usbd_status err;
	u_int size, flags;
	int s;

	DPRINTFN(5,("usbd_transfer: xfer=%p, flags=%x, pipe=%p, running=%d\n",
		    xfer, xfer->flags, pipe, pipe->running));
#ifdef USB_DEBUG
	if (usbdebug > 5)
		usbd_dump_queue(pipe);
#endif
	xfer->done = 0;

	if (pipe->aborting)
		return (USBD_CANCELLED);

	size = xfer->length;
	flags = xfer->flags;
	/* If there is no buffer, allocate one. */
	if (!(xfer->rqflags & URQ_MASK_DRV_REQUESTED_BUF) && size != 0) {
		struct usbd_bus *bus = pipe->device->bus;

#ifdef DIAGNOSTIC
		if (xfer->rqflags & URQ_AUTO_BUF)
			printf("usbd_transfer: has old buffer!\n");
		if (xfer->device->bus->intr_context)
			panic("usbd_transfer: allocm not in process context");
#endif
		err = bus->methods->allocm(bus, xfer,
		    (flags & USBD_NO_COPY) ? xfer->buffer : NULL,
		    size);
		if (err)
			return (err);
		xfer->rqflags |= URQ_AUTO_BUF;
	}
#ifdef DIAGNOSTIC
	if ((xfer->rqflags & URQ_MASK_DRV_REQUESTED_BUF) != 0 &&
	    size > xfer->bufsize) {
		panic("usbd_transfer: buffer size overrun size=%u bufsize=%lu rqflags=%x",
		    size, (unsigned long)xfer->bufsize, xfer->rqflags);
	}
#endif

#ifdef DIAGNOSTIC
	if ((flags & USBD_NO_COPY) == 0 && size != 0) {
		if (xfer->rqflags & URQ_DEV_MAP_BUFFER)
			printf("usbd_transfer: USBD_NO_COPY recommended with mapped buffer");
		if (xfer->rqflags & URQ_DEV_MAP_MBUF)
			panic("usbd_transfer: USBD_NO_COPY required with mapped mbuf");
	}
#endif
	/* Copy data if going out. */
	if (!(flags & USBD_NO_COPY) && size != 0 &&
	    !usbd_xfer_isread(xfer) && xfer->buffer != xfer->hcbuffer) {
		DPRINTFN(5, ("usbd_transfer: copy %p (alloc) <- %p (buffer), %u bytes\n",
			     xfer->hcbuffer, xfer->buffer, (unsigned)size));
		memcpy(xfer->hcbuffer, xfer->buffer, size);
	}

	err = pipe->methods->transfer(xfer);
	/*
	 * Note xfer may not be valid after non-error completion
	 * of the transfer method unless synchronous.
	 */

	if (err != USBD_IN_PROGRESS && err) {
		/* The transfer has not been queued, so free buffer. */
		if (xfer->rqflags & URQ_AUTO_BUF) {
			struct usbd_bus *bus = pipe->device->bus;

#ifdef DIAGNOSTIC
			if (bus->intr_context)
				panic("usbd_transfer: freem not in process context");
#endif
			bus->methods->freem(bus, xfer, U_WAITOK);
			xfer->rqflags &= ~URQ_AUTO_BUF;
		}
	}

	if (!(flags & USBD_SYNCHRONOUS))
		return (xfer->done ? 0 : USBD_IN_PROGRESS);

	/* Sync transfer, wait for completion. */
	s = splusb();
	while (!xfer->done) {
		if (pipe->device->bus->use_polling)
			panic("usbd_transfer: not done");
		tsleep(xfer, PRIBIO, "usbsyn", 0);
	}
	splx(s);
	return (xfer->status);
}

/* Like usbd_transfer(), but waits for completion. */
usbd_status
usbd_sync_transfer(usbd_xfer_handle xfer)
{
	xfer->flags |= USBD_SYNCHRONOUS;
	return (usbd_transfer(xfer));
}

void *
usbd_alloc_buffer(usbd_xfer_handle xfer, u_int32_t size)
{
	struct usbd_bus *bus = xfer->device->bus;
	usbd_status err;

	USB_KASSERT2((xfer->rqflags & URQ_MASK_BUF) == 0,
	    ("usbd_alloc_buffer: xfer already has a buffer"));

#ifdef DIAGNOSTIC
	if (xfer->device->bus->intr_context)
		panic("usbd_alloc_buffer: allocm not in process context");
#endif
	err = bus->methods->allocm(bus, xfer, NULL, size);
	if (err)
		return (NULL);
	xfer->bufsize = size;
	xfer->rqflags |= URQ_DEV_BUF;
	return xfer->hcbuffer;
}

Static void
usbd_free_buffer_flag(usbd_xfer_handle xfer, enum usbd_waitflg waitflg)
{
	struct usbd_bus *bus;

	USB_KASSERT2((xfer->rqflags & URQ_MASK_BUF) == URQ_DEV_BUF,
	    ("usbd_free_buffer_flag: no/auto buffer"));

	xfer->rqflags &= ~(URQ_DEV_BUF | URQ_AUTO_BUF);
	xfer->bufsize = 0;
	bus = xfer->device->bus;
#ifdef DIAGNOSTIC
	if (waitflg == U_WAITOK && bus->intr_context)
		panic("usbd_free_buffer: freem not in process context");
#endif
	bus->methods->freem(bus, xfer, waitflg);

	xfer->hcbuffer = NULL;
}

void
usbd_free_buffer(usbd_xfer_handle xfer)
{
	usbd_free_buffer_flag(xfer, U_WAITOK);
}

void *
usbd_get_buffer(usbd_xfer_handle xfer)
{
	if (!(xfer->rqflags & URQ_DEV_BUF))
		return (NULL);
	return (xfer->hcbuffer);
}

/*
 * map buffer handlings
 */

/* Allocate mapping resources to prepare buffer mapping. */
usbd_status
usbd_map_alloc(usbd_xfer_handle xfer)
{
	struct usbd_bus *bus = xfer->device->bus;
	usbd_status err;

#ifdef DIAGNOSTIC
	if (bus->intr_context)
		panic("usbd_map_alloc: map_alloc not in process context");
#endif
	USB_KASSERT2((xfer->rqflags & URQ_DEV_MAP_PREPARED) == 0,
	    ("usbd_map_alloc: map already allocated"));

	err = bus->methods->map_alloc(xfer);
	if (err != USBD_NORMAL_COMPLETION)
		return (err);

	xfer->rqflags |= URQ_DEV_MAP_PREPARED;

	return (USBD_NORMAL_COMPLETION);
}

/* Free mapping resources. */
void
usbd_map_free(usbd_xfer_handle xfer)
{
	struct usbd_bus *bus = xfer->device->bus;

#ifdef DIAGNOSTIC
	if (bus->intr_context)
		panic("usbd_map_free: map_free not in process context");
#endif
	USB_KASSERT2((xfer->rqflags & URQ_DEV_MAP_PREPARED),
	    ("usbd_map_free: map not allocated"));

	bus->methods->map_free(xfer);

	xfer->rqflags &= ~URQ_DEV_MAP_PREPARED;
}

/* Map plain buffer. */
void
usbd_map_buffer(usbd_xfer_handle xfer, void *buf, u_int32_t size)
{
	struct usbd_bus *bus = xfer->device->bus;

#ifdef DIAGNOSTIC
	if ((xfer->rqflags & URQ_MASK_BUF))
		panic("usbd_map_buffer: already has buffer (rqflags=%x)",
		    xfer->rqflags);
	if ((xfer->rqflags & URQ_DEV_MAP_PREPARED) == 0 && bus->intr_context)
		panic("usbd_map_buffer: mapm (without map_alloc) not in process context");
#endif

	bus->methods->mapm(xfer, buf, size);

	xfer->bufsize = size;
	xfer->rqflags |= URQ_DEV_MAP_BUFFER;
}

/* Map mbuf(9) chain. */
usbd_status
usbd_map_buffer_mbuf(usbd_xfer_handle xfer, struct mbuf *chain)
{
	struct usbd_bus *bus = xfer->device->bus;
	usbd_status err;

#ifdef DIAGNOSTIC
	if ((xfer->rqflags & URQ_MASK_BUF))
		panic("usbd_map_buffer_mbuf: already has buffer (rqflags=%x)",
		    xfer->rqflags);
	if ((xfer->rqflags & URQ_DEV_MAP_PREPARED) == 0 && bus->intr_context)
		panic("usbd_map_buffer_mbuf: mapm_mbuf (without map_alloc) not in process context");
#endif

	err = bus->methods->mapm_mbuf(xfer, chain);
	if (err != USBD_NORMAL_COMPLETION)
		return (err);

	xfer->bufsize = chain->m_pkthdr.len;
	xfer->mbuf_chain = chain;
	xfer->rqflags |= URQ_DEV_MAP_MBUF;

	return (err);
}

/* Unmap plain buffer or mbuf(9) chain. */
void
usbd_unmap_buffer(usbd_xfer_handle xfer)
{
	struct usbd_bus *bus = xfer->device->bus;

	USB_KASSERT2((xfer->rqflags & (URQ_DEV_MAP_BUFFER | URQ_DEV_MAP_MBUF)),
	    ("usbd_unmap_buffer: no map"));
#ifdef DIAGNOSTIC
	if ((xfer->rqflags & URQ_DEV_MAP_PREPARED) == 0 && bus->intr_context)
		panic("usbd_unmap_buffer: unmapm (without map_alloc) not in process context");
#endif

	bus->methods->unmapm(xfer);

	xfer->rqflags &= ~(URQ_DEV_MAP_BUFFER | URQ_DEV_MAP_MBUF);
	xfer->bufsize = 0;
	xfer->mbuf_chain = NULL;
}

Static usbd_xfer_handle
usbd_alloc_xfer_flag(usbd_device_handle dev, usbd_pipe_handle pipe,
	enum usbd_waitflg waitflg)
{
	usbd_xfer_handle xfer;

#ifdef DIAGNOSTIC
	if (waitflg == U_WAITOK && dev->bus->intr_context)
		panic("usbd_alloc_xfer: allocx not in process context");
	if (pipe == NULL)
		panic("usbd_alloc_xfer: pipe == NULL");
#endif
	xfer = dev->bus->methods->allocx(dev->bus, pipe, waitflg);
	if (xfer == NULL)
		return (NULL);
	xfer->pipe = pipe;
	xfer->device = dev;
	usb_callout_init(xfer->timeout_handle);
	DPRINTFN(5,("usbd_alloc_xfer_flag() = %p\n", xfer));
	return (xfer);
}

usbd_xfer_handle
usbd_alloc_xfer(usbd_device_handle dev, usbd_pipe_handle pipe)
{
	return (usbd_alloc_xfer_flag(dev, pipe, U_WAITOK));
}

usbd_xfer_handle
usbd_alloc_default_xfer(usbd_device_handle dev)
{
	return (usbd_alloc_xfer_flag(dev, dev->default_pipe, U_WAITOK));
}

Static usbd_status
usbd_free_xfer_flag(usbd_xfer_handle xfer, enum usbd_waitflg waitflg)
{
	DPRINTFN(5,("usbd_free_xfer_flag: %p %d\n", xfer, waitflg));
	usb_rem_task(xfer->device, &xfer->task);
	if (xfer->rqflags & (URQ_DEV_BUF | URQ_AUTO_BUF))
		usbd_free_buffer_flag(xfer, waitflg);
	if (xfer->rqflags & (URQ_DEV_MAP_BUFFER | URQ_DEV_MAP_MBUF))
		usbd_unmap_buffer(xfer);
	if (waitflg == U_WAITOK && (xfer->rqflags & URQ_DEV_MAP_PREPARED))
		usbd_map_free(xfer);
#if defined(__NetBSD__) && defined(DIAGNOSTIC)
	if (callout_pending(&xfer->timeout_handle)) {
		callout_stop(&xfer->timeout_handle);
		printf("usbd_free_xfer_flag: timout_handle pending");
	}
#endif
	xfer->device->bus->methods->freex(xfer->device->bus, xfer);
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_free_xfer(usbd_xfer_handle xfer)
{

	return (usbd_free_xfer_flag(xfer, U_WAITOK));
}

void
usbd_setup_xfer(usbd_xfer_handle xfer, usbd_pipe_handle pipe,
		usbd_private_handle priv, void *buffer, u_int32_t length,
		u_int16_t flags, u_int32_t timeout,
		usbd_callback callback)
{
	USB_KASSERT(xfer->pipe == pipe);	/* XXX pipe arg should go */
	xfer->priv = priv;
	xfer->buffer = buffer;
	xfer->length = length;
	xfer->actlen = 0;
	xfer->flags = flags;
	xfer->timeout = timeout;
	xfer->status = USBD_NOT_STARTED;
	xfer->callback = callback;
	xfer->rqflags &= ~URQ_REQUEST;
	xfer->nframes = 0;
}

void
usbd_setup_default_xfer(usbd_xfer_handle xfer, usbd_device_handle dev,
			usbd_private_handle priv, u_int32_t timeout,
			usb_device_request_t *req, void *buffer,
			u_int32_t length, u_int16_t flags,
			usbd_callback callback)
{
	USB_KASSERT(xfer->pipe == dev->default_pipe);	/* XXX */
	xfer->priv = priv;
	xfer->buffer = buffer;
	xfer->length = length;
	xfer->actlen = 0;
	xfer->flags = flags;
	xfer->timeout = timeout;
	xfer->status = USBD_NOT_STARTED;
	xfer->callback = callback;
	xfer->request = *req;
	xfer->rqflags |= URQ_REQUEST;
	xfer->nframes = 0;
}

void
usbd_setup_isoc_xfer(usbd_xfer_handle xfer, usbd_pipe_handle pipe,
		     usbd_private_handle priv, u_int16_t *frlengths,
		     u_int32_t nframes, u_int16_t flags, usbd_callback callback)
{
	int i;

	USB_KASSERT(xfer->pipe == pipe);	/* XXX pipe arg should go */
	xfer->priv = priv;
	xfer->buffer = 0;
	xfer->length = 0;
	for (i = 0; i < nframes; i++)
		xfer->length += frlengths[i];
	xfer->actlen = 0;
	xfer->flags = flags;
	xfer->timeout = USBD_NO_TIMEOUT;
	xfer->status = USBD_NOT_STARTED;
	xfer->callback = callback;
	xfer->rqflags &= ~URQ_REQUEST;
	xfer->frlengths = frlengths;
	xfer->nframes = nframes;
}

void
usbd_get_xfer_status(usbd_xfer_handle xfer, usbd_private_handle *priv,
		     void **buffer, u_int32_t *count, usbd_status *status)
{
	if (priv != NULL)
		*priv = xfer->priv;
	if (buffer != NULL)
		*buffer = xfer->buffer;
	if (count != NULL)
		*count = xfer->actlen;
	if (status != NULL)
		*status = xfer->status;
}

usb_config_descriptor_t *
usbd_get_config_descriptor(usbd_device_handle dev)
{
#ifdef DIAGNOSTIC
	if (dev == NULL) {
		printf("usbd_get_config_descriptor: dev == NULL\n");
		return (NULL);
	}
#endif
	return (dev->cdesc);
}

int
usbd_get_speed(usbd_device_handle dev)
{
	return (dev->speed);
}

usb_interface_descriptor_t *
usbd_get_interface_descriptor(usbd_interface_handle iface)
{
#ifdef DIAGNOSTIC
	if (iface == NULL) {
		printf("usbd_get_interface_descriptor: dev == NULL\n");
		return (NULL);
	}
#endif
	return (iface->idesc);
}

usb_device_descriptor_t *
usbd_get_device_descriptor(usbd_device_handle dev)
{
	return (&dev->ddesc);
}

usb_endpoint_descriptor_t *
usbd_interface2endpoint_descriptor(usbd_interface_handle iface, u_int8_t index)
{
	if (index >= iface->idesc->bNumEndpoints)
		return (0);
	return (iface->endpoints[index].edesc);
}

usbd_status
usbd_abort_pipe(usbd_pipe_handle pipe)
{
	usbd_status err;
	int s;

#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		printf("usbd_abort_pipe: pipe==NULL\n");
		return (USBD_NORMAL_COMPLETION);
	}
#endif
	s = splusb();
	err = usbd_ar_pipe(pipe);
	splx(s);
	return (err);
}

usbd_status
usbd_abort_default_pipe(usbd_device_handle dev)
{
	return (usbd_abort_pipe(dev->default_pipe));
}

usbd_status
usbd_clear_endpoint_stall(usbd_pipe_handle pipe)
{
	usbd_device_handle dev = pipe->device;
	usb_device_request_t req;
	usbd_status err;

	DPRINTFN(8, ("usbd_clear_endpoint_stall\n"));

	/*
	 * Clearing en endpoint stall resets the endpoint toggle, so
	 * do the same to the HC toggle.
	 */
	pipe->methods->cleartoggle(pipe);

	req.bmRequestType = UT_WRITE_ENDPOINT;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, UF_ENDPOINT_HALT);
	USETW(req.wIndex, pipe->endpoint->edesc->bEndpointAddress);
	USETW(req.wLength, 0);
	err = usbd_do_request(dev, &req, 0);
#if 0
XXX should we do this?
	if (!err) {
		pipe->state = USBD_PIPE_ACTIVE;
		/* XXX activate pipe */
	}
#endif
	return (err);
}

usbd_status
usbd_clear_endpoint_stall_async(usbd_pipe_handle pipe)
{
	usbd_device_handle dev = pipe->device;
	usb_device_request_t req;
	usbd_status err;

	pipe->methods->cleartoggle(pipe);

	req.bmRequestType = UT_WRITE_ENDPOINT;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, UF_ENDPOINT_HALT);
	USETW(req.wIndex, pipe->endpoint->edesc->bEndpointAddress);
	USETW(req.wLength, 0);
	err = usbd_do_request_async(dev, &req, 0);
	return (err);
}

void
usbd_clear_endpoint_toggle(usbd_pipe_handle pipe)
{
	pipe->methods->cleartoggle(pipe);
}

usbd_status
usbd_endpoint_count(usbd_interface_handle iface, u_int8_t *count)
{
#ifdef DIAGNOSTIC
	if (iface == NULL || iface->idesc == NULL) {
		printf("usbd_endpoint_count: NULL pointer\n");
		return (USBD_INVAL);
	}
#endif
	*count = iface->idesc->bNumEndpoints;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_interface_count(usbd_device_handle dev, u_int8_t *count)
{
	if (dev->cdesc == NULL)
		return (USBD_NOT_CONFIGURED);
	*count = dev->cdesc->bNumInterface;
	return (USBD_NORMAL_COMPLETION);
}

void
usbd_interface2device_handle(usbd_interface_handle iface,
			     usbd_device_handle *dev)
{
	*dev = iface->device;
}

usbd_status
usbd_device2interface_handle(usbd_device_handle dev,
			     u_int8_t ifaceno, usbd_interface_handle *iface)
{
	if (dev->cdesc == NULL)
		return (USBD_NOT_CONFIGURED);
	if (ifaceno >= dev->cdesc->bNumInterface)
		return (USBD_INVAL);
	*iface = &dev->ifaces[ifaceno];
	return (USBD_NORMAL_COMPLETION);
}

usbd_device_handle
usbd_pipe2device_handle(usbd_pipe_handle pipe)
{
	return (pipe->device);
}

/* XXXX use altno */
usbd_status
usbd_set_interface(usbd_interface_handle iface, int altidx)
{
	usb_device_request_t req;
	usbd_status err;
	void *endpoints;

	if (LIST_FIRST(&iface->pipes) != 0)
		return (USBD_IN_USE);

	endpoints = iface->endpoints;
	err = usbd_fill_iface_data(iface->device, iface->index, altidx);
	if (err)
		return (err);

	/* new setting works, we can free old endpoints */
	if (endpoints != NULL)
		free(endpoints, M_USB);

#ifdef DIAGNOSTIC
	if (iface->idesc == NULL) {
		printf("usbd_set_interface: NULL pointer\n");
		return (USBD_INVAL);
	}
#endif

	req.bmRequestType = UT_WRITE_INTERFACE;
	req.bRequest = UR_SET_INTERFACE;
	USETW(req.wValue, iface->idesc->bAlternateSetting);
	USETW(req.wIndex, iface->idesc->bInterfaceNumber);
	USETW(req.wLength, 0);
	return (usbd_do_request(iface->device, &req, 0));
}

int
usbd_get_no_alts(usb_config_descriptor_t *cdesc, int ifaceno)
{
	char *p = (char *)cdesc;
	char *end = p + UGETW(cdesc->wTotalLength);
	usb_interface_descriptor_t *d;
	int n;

	for (n = 0; p < end; p += d->bLength) {
		d = (usb_interface_descriptor_t *)p;
		if (p + d->bLength <= end &&
		    d->bDescriptorType == UDESC_INTERFACE &&
		    d->bInterfaceNumber == ifaceno)
			n++;
	}
	return (n);
}

int
usbd_get_interface_altindex(usbd_interface_handle iface)
{
	return (iface->altindex);
}

usbd_status
usbd_get_interface(usbd_interface_handle iface, u_int8_t *aiface)
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_INTERFACE;
	req.bRequest = UR_GET_INTERFACE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, iface->idesc->bInterfaceNumber);
	USETW(req.wLength, 1);
	return (usbd_do_request(iface->device, &req, aiface));
}

/*** Internal routines ***/

/* Dequeue all pipe operations, called at splusb(). */
Static usbd_status
usbd_ar_pipe(usbd_pipe_handle pipe)
{
	usbd_xfer_handle xfer;

	SPLUSBCHECK;

	DPRINTFN(2,("usbd_ar_pipe: pipe=%p\n", pipe));
#ifdef USB_DEBUG
	if (usbdebug > 5)
		usbd_dump_queue(pipe);
#endif
	pipe->repeat = 0;
	pipe->aborting = 1;
	while ((xfer = SIMPLEQ_FIRST(&pipe->queue)) != NULL) {
		DPRINTFN(2,("usbd_ar_pipe: pipe=%p xfer=%p (methods=%p)\n",
			    pipe, xfer, pipe->methods));
		/* Make the HC abort it (and invoke the callback). */
		pipe->methods->abort(xfer);
		USB_KASSERT2(SIMPLEQ_FIRST(&pipe->queue) != xfer,
		    ("usbd_ar_pipe"));
		/* XXX only for non-0 usbd_clear_endpoint_stall(pipe); */
	}
	pipe->aborting = 0;
	return (USBD_NORMAL_COMPLETION);
}

/* Called at splusb() */
void
usb_transfer_complete(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	int sync = xfer->flags & USBD_SYNCHRONOUS;
	int erred = xfer->status == USBD_CANCELLED ||
	    xfer->status == USBD_TIMEOUT;
	int repeat, polling;

	SPLUSBCHECK;

	DPRINTFN(5, ("usb_transfer_complete: pipe=%p xfer=%p status=%d "
		     "actlen=%d\n", pipe, xfer, xfer->status, xfer->actlen));
#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_ONQU) {
		printf("usb_transfer_complete: xfer=%p not busy 0x%08x\n",
		       xfer, xfer->busy_free);
	}
#endif

#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		printf("usb_transfer_complete: pipe==0, xfer=%p\n", xfer);
		return;
	}
#endif
	repeat = pipe->repeat;
	polling = pipe->device->bus->use_polling;
	/* XXXX */
	if (polling)
		pipe->running = 0;

	if (!(xfer->flags & USBD_NO_COPY) && xfer->actlen != 0 &&
	    usbd_xfer_isread(xfer)) {
#ifdef DIAGNOSTIC
		if (xfer->actlen > xfer->length) {
			printf("usb_transfer_complete: actlen > len %d > %d\n",
			       xfer->actlen, xfer->length);
			xfer->actlen = xfer->length;
		}
#endif
		/* Copy data if it is not already in the correct buffer. */
		if (!(xfer->flags & USBD_NO_COPY) && xfer->hcbuffer != NULL) {
			DPRINTFN(5, ("usb_transfer_complete: copy %p (buffer) <- %p (alloc), %u bytes\n",
				     xfer->buffer, xfer->hcbuffer, (unsigned)xfer->actlen));
#ifdef DIAGNOSTIC
			if ((xfer->rqflags & (URQ_AUTO_BUF | URQ_DEV_BUF)) == 0)
				panic("usb_transfer_complete: USBD_NO_COPY required with mapped buffer");
#endif
			memcpy(xfer->buffer, xfer->hcbuffer, xfer->actlen);
		}
	}

	/*
	 * if we allocated or mapped the buffer in usbd_transfer()
	 * we unmap it here.
	 */
	if (xfer->rqflags & URQ_AUTO_BUF) {
		if (!repeat) {
			struct usbd_bus *bus = pipe->device->bus;
			bus->methods->freem(bus, xfer, U_NOWAIT);
			xfer->rqflags &= ~URQ_AUTO_BUF;
		}
	}

	if (!repeat) {
		/* Remove request from queue. */
#ifdef DIAGNOSTIC
		xfer->busy_free = XFER_BUSY;
#endif
		USB_KASSERT2(SIMPLEQ_FIRST(&pipe->queue) == xfer,
		    ("usb_transfer_complete: bad dequeue"));
		SIMPLEQ_REMOVE_HEAD(&pipe->queue, next);
	}
	DPRINTFN(5,("usb_transfer_complete: repeat=%d new head=%p\n",
		    repeat, SIMPLEQ_FIRST(&pipe->queue)));

	/* Count completed transfers. */
	++pipe->device->bus->stats.uds_requests
		[pipe->endpoint->edesc->bmAttributes & UE_XFERTYPE];

	xfer->done = 1;
	if (!xfer->status && xfer->actlen < xfer->length &&
	    !(xfer->flags & USBD_SHORT_XFER_OK)) {
		DPRINTFN(-1,("usbd_transfer_cb: short transfer %d<%d\n",
			     xfer->actlen, xfer->length));
		xfer->status = USBD_SHORT_XFER;
	}

	/*
	 * For repeat operations, call the callback first, as the xfer
	 * will not go away and the "done" method may modify it. Otherwise
	 * reverse the order in case the callback wants to free or reuse
	 * the xfer.
	 */
	if (repeat) {
		if (xfer->callback)
			xfer->callback(xfer, xfer->priv, xfer->status);
		pipe->methods->done(xfer);
	} else {
		pipe->methods->done(xfer);
		if (xfer->callback) {
			if (xfer->flags & USBD_CALLBACK_AS_TASK) {
				/*
				 * Callback as a task (in thread context).
				 */
				usb_init_task(&xfer->task, usbd_callback_task,
				    xfer);
				usb_add_task(xfer->device, &xfer->task,
				    USB_TASKQ_DRIVER);
			} else
				xfer->callback(xfer, xfer->priv, xfer->status);
		}
	}

	if (sync && !polling)
		wakeup(xfer);

	if (!repeat) {
		/* XXX should we stop the queue on all errors? */
		if (erred && pipe->iface != NULL)	/* not control pipe */
			pipe->running = 0;
		else
			usbd_start_next(pipe);
	}
}

Static void
usbd_callback_task(void *arg)
{
	usbd_xfer_handle xfer = arg;

	xfer->callback(xfer, xfer->priv, xfer->status);
}

usbd_status
usb_insert_transfer(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	usbd_status err;
	int s;

	DPRINTFN(5,("usb_insert_transfer: pipe=%p running=%d timeout=%d\n",
		    pipe, pipe->running, xfer->timeout));
#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_BUSY) {
		printf("usb_insert_transfer: xfer=%p not busy 0x%08x\n",
		       xfer, xfer->busy_free);
		return (USBD_INVAL);
	}
	xfer->busy_free = XFER_ONQU;
#endif
	s = splusb();
	USB_KASSERT2(SIMPLEQ_FIRST(&pipe->queue) != xfer,
	    ("usb_insert_transfer"));
	SIMPLEQ_INSERT_TAIL(&pipe->queue, xfer, next);
	if (pipe->running)
		err = USBD_IN_PROGRESS;
	else {
		pipe->running = 1;
		err = USBD_NORMAL_COMPLETION;
	}
	splx(s);
	return (err);
}

/* Called at splusb() */
void
usbd_start_next(usbd_pipe_handle pipe)
{
	usbd_xfer_handle xfer;
	usbd_status err;

	SPLUSBCHECK;

#ifdef DIAGNOSTIC
	if (pipe == NULL) {
		printf("usbd_start_next: pipe == NULL\n");
		return;
	}
	if (pipe->methods == NULL || pipe->methods->start == NULL) {
		printf("usbd_start_next: pipe=%p no start method\n", pipe);
		return;
	}
#endif

	/* Get next request in queue. */
	xfer = SIMPLEQ_FIRST(&pipe->queue);
	DPRINTFN(5, ("usbd_start_next: pipe=%p, xfer=%p\n", pipe, xfer));
	if (xfer == NULL) {
		pipe->running = 0;
	} else {
		err = pipe->methods->start(xfer);
		if (err != USBD_IN_PROGRESS) {
			printf("usbd_start_next: error=%d\n", err);
			pipe->running = 0;
			/* XXX do what? */
		}
	}
}

usbd_status
usbd_do_request(usbd_device_handle dev, usb_device_request_t *req, void *data)
{
	return (usbd_do_request_flags(dev, req, data, 0, 0,
				      USBD_DEFAULT_TIMEOUT));
}

usbd_status
usbd_do_request_flags(usbd_device_handle dev, usb_device_request_t *req,
		      void *data, u_int16_t flags, int *actlen, u_int32_t timo)
{
	return (usbd_do_request_flags_pipe(dev, dev->default_pipe, req,
					   data, flags, actlen, timo));
}

usbd_status
usbd_do_request_flags_pipe(usbd_device_handle dev, usbd_pipe_handle pipe,
	usb_device_request_t *req, void *data, u_int16_t flags, int *actlen,
	u_int32_t timeout)
{
	usbd_xfer_handle xfer;
	usbd_status err;

#ifdef DIAGNOSTIC
#if defined(__i386__) && defined(__FreeBSD__)
	KASSERT(curthread->td_intr_nesting_level == 0,
	       	("usbd_do_request: in interrupt context"));
#endif
	if (dev->bus->intr_context) {
		panic("usbd_do_request: not in process context");
	}
#endif

	xfer = usbd_alloc_xfer(dev, pipe);
	if (xfer == NULL)
		return (USBD_NOMEM);
	xfer->pipe = dev->default_pipe;	/* XXX */
	/* XXX */
	usbd_setup_default_xfer(xfer, dev, 0, timeout, req,
				data, UGETW(req->wLength), flags, 0);
	/* XXX */
	xfer->pipe = pipe;
	err = usbd_sync_transfer(xfer);
#if defined(USB_DEBUG) || defined(DIAGNOSTIC)
	if (xfer->actlen > xfer->length) {
		DPRINTF(("usbd_do_request: overrun addr=%d type=0x%02x req=0x"
			 "%02x val=%d index=%d rlen=%d length=%d actlen=%d\n",
			 dev->address, xfer->request.bmRequestType,
			 xfer->request.bRequest, UGETW(xfer->request.wValue),
			 UGETW(xfer->request.wIndex),
			 UGETW(xfer->request.wLength),
			 xfer->length, xfer->actlen));
	}
#endif
	if (actlen != NULL)
		*actlen = xfer->actlen;
	if (err == USBD_STALLED) {
		/*
		 * The control endpoint has stalled.  Control endpoints
		 * should not halt, but some may do so anyway so clear
		 * any halt condition.
		 */
		usb_device_request_t treq;
		usb_status_t status;
		u_int16_t s;
		usbd_status nerr;

		treq.bmRequestType = UT_READ_ENDPOINT;
		treq.bRequest = UR_GET_STATUS;
		USETW(treq.wValue, 0);
		USETW(treq.wIndex, 0);
		USETW(treq.wLength, sizeof(usb_status_t));
		usbd_setup_default_xfer(xfer, dev, 0, USBD_DEFAULT_TIMEOUT,
					   &treq, &status,sizeof(usb_status_t),
					   0, 0);
		nerr = usbd_sync_transfer(xfer);
		if (nerr)
			goto bad;
		s = UGETW(status.wStatus);
		DPRINTF(("usbd_do_request: status = 0x%04x\n", s));
		if (!(s & UES_HALT))
			goto bad;
		treq.bmRequestType = UT_WRITE_ENDPOINT;
		treq.bRequest = UR_CLEAR_FEATURE;
		USETW(treq.wValue, UF_ENDPOINT_HALT);
		USETW(treq.wIndex, 0);
		USETW(treq.wLength, 0);
		usbd_setup_default_xfer(xfer, dev, 0, USBD_DEFAULT_TIMEOUT,
					   &treq, &status, 0, 0, 0);
		nerr = usbd_sync_transfer(xfer);
		if (nerr)
			goto bad;
	}

 bad:
	usbd_free_xfer(xfer);
	return (err);
}

void
usbd_do_request_async_cb(usbd_xfer_handle xfer,
    usbd_private_handle priv, usbd_status status)
{
#if defined(USB_DEBUG) || defined(DIAGNOSTIC)
	if (xfer->actlen > xfer->length) {
		DPRINTF(("usbd_do_request: overrun addr=%d type=0x%02x req=0x"
			 "%02x val=%d index=%d rlen=%d length=%d actlen=%d\n",
			 xfer->pipe->device->address,
			 xfer->request.bmRequestType,
			 xfer->request.bRequest, UGETW(xfer->request.wValue),
			 UGETW(xfer->request.wIndex),
			 UGETW(xfer->request.wLength),
			 xfer->length, xfer->actlen));
	}
#endif
	usbd_free_xfer_flag(xfer, U_NOWAIT);
}

/*
 * Execute a request without waiting for completion.
 * Can be used from interrupt context.
 */
usbd_status
usbd_do_request_async(usbd_device_handle dev, usb_device_request_t *req,
		      void *data)
{
	usbd_xfer_handle xfer;

	xfer = usbd_alloc_xfer_flag(dev, dev->default_pipe, U_NOWAIT);
	if (xfer == NULL)
		return (USBD_NOMEM);
	usbd_setup_default_xfer(xfer, dev, 0, USBD_DEFAULT_TIMEOUT, req,
	    data, UGETW(req->wLength), 0, usbd_do_request_async_cb);

	/*
	 * Request will be processed later as a task (in thread context).
	 */
	usb_init_task(&xfer->task, usbd_do_request_async_task, xfer);
	usb_add_task(dev, &xfer->task, USB_TASKQ_HC);

	return (USBD_IN_PROGRESS);
}

Static void
usbd_do_request_async_task(void *arg)
{
	usbd_xfer_handle xfer = arg;
	usbd_status err;

	/* XXX impossible to notify caller the error */
	err = usbd_transfer(xfer);
	if (err != USBD_IN_PROGRESS && err) {
		usbd_free_xfer(xfer);
	}
}

const struct usbd_quirks *
usbd_get_quirks(usbd_device_handle dev)
{
#ifdef DIAGNOSTIC
	if (dev == NULL) {
		printf("usbd_get_quirks: dev == NULL\n");
		return 0;
	}
#endif
	return (dev->quirks);
}

/* XXX do periodic free() of free list */

/*
 * Called from keyboard driver when in polling mode.
 */
void
usbd_dopoll(usbd_interface_handle iface)
{
	iface->device->bus->methods->do_poll(iface->device->bus);
}

void
usbd_set_polling(usbd_device_handle dev, int on)
{
	if (on)
		dev->bus->use_polling++;
	else
		dev->bus->use_polling--;
	/* When polling we need to make sure there is nothing pending to do. */
	if (dev->bus->use_polling)
		dev->bus->methods->soft_intr(dev->bus);
}


usb_endpoint_descriptor_t *
usbd_get_endpoint_descriptor(usbd_interface_handle iface, u_int8_t address)
{
	struct usbd_endpoint *ep;
	int i;

	for (i = 0; i < iface->idesc->bNumEndpoints; i++) {
		ep = &iface->endpoints[i];
		if (ep->edesc->bEndpointAddress == address)
			return (iface->endpoints[i].edesc);
	}
	return (0);
}

/*
 * usbd_ratecheck() can limit the number of error messages that occurs.
 * When a device is unplugged it may take up to 0.25s for the hub driver
 * to notice it.  If the driver continuosly tries to do I/O operations
 * this can generate a large number of messages.
 */
int
usbd_ratecheck(struct timeval *last)
{
#if defined(__NetBSD__)
	static struct timeval errinterval = { 0, 250000 }; /* 0.25 s*/

	return (ratecheck(last, &errinterval));
#elif defined(__FreeBSD__)
	if (last->tv_sec == time_second)
		return (0);
	last->tv_sec = time_second;
	return (1);
#endif
}

/*
 * Search for a vendor/product pair in an array.  The item size is
 * given as an argument.
 */
const struct usb_devno *
usb_match_device(const struct usb_devno *tbl, u_int nentries, u_int sz,
		 u_int16_t vendor, u_int16_t product)
{
	while (nentries-- > 0) {
		u_int16_t tproduct = tbl->ud_product;
		if (tbl->ud_vendor == vendor &&
		    (tproduct == product || tproduct == USB_PRODUCT_ANY))
			return (tbl);
		tbl = (const struct usb_devno *)((const char *)tbl + sz);
	}
	return (NULL);
}


void
usb_desc_iter_init(usbd_device_handle dev, usbd_desc_iter_t *iter)
{
	const usb_config_descriptor_t *cd = usbd_get_config_descriptor(dev);

        iter->cur = (const uByte *)cd;
        iter->end = (const uByte *)cd + UGETW(cd->wTotalLength);
}

const usb_descriptor_t *
usb_desc_iter_next(usbd_desc_iter_t *iter)
{
	const usb_descriptor_t *desc;

	if (iter->cur + sizeof(usb_descriptor_t) >= iter->end) {
		if (iter->cur != iter->end)
			printf("usb_desc_iter_next: bad descriptor\n");
		return NULL;
	}
	desc = (const usb_descriptor_t *)iter->cur;
	if (desc->bLength == 0) {
		printf("usb_desc_iter_next: descriptor length = 0\n");
		return NULL;
	}
	iter->cur += desc->bLength;
	if (iter->cur > iter->end) {
		printf("usb_desc_iter_next: descriptor length too large\n");
		return NULL;
	}
	return desc;
}

usbd_status
usbd_get_string(usbd_device_handle dev, int si, char *buf)
{
	return usbd_get_string0(dev, si, buf, 1);
}

usbd_status
usbd_get_string0(usbd_device_handle dev, int si, char *buf, int unicode)
{
	int swap = dev->quirks->uq_flags & UQ_SWAP_UNICODE;
	usb_string_descriptor_t us;
	char *s;
	int i, n;
	u_int16_t c;
	usbd_status err;
	int size;

	buf[0] = '\0';
	if (si == 0)
		return (USBD_INVAL);
	if (dev->quirks->uq_flags & UQ_NO_STRINGS)
		return (USBD_STALLED);
	if (dev->langid == USBD_NOLANG) {
		/* Set up default language */
		err = usbd_get_string_desc(dev, USB_LANGUAGE_TABLE, 0, &us,
		    &size);
		if (err || size < 4) {
			DPRINTFN(-1,("usbd_get_string: getting lang failed, using 0\n"));
			dev->langid = 0; /* Well, just pick something then */
		} else {
			/* Pick the first language as the default. */
			dev->langid = UGETW(us.bString[0]);
		}
	}
	err = usbd_get_string_desc(dev, si, dev->langid, &us, &size);
	if (err)
		return (err);
	s = buf;
	n = size / 2 - 1;
	if (unicode) {
		for (i = 0; i < n; i++) {
			c = UGETW(us.bString[i]);
			if (swap)
				c = (c >> 8) | (c << 8);
			s += wput_utf8(s, 3, c);
		}
		*s++ = 0;
	}
#if defined(__NetBSD__) && defined(COMPAT_30)
	else {
		for (i = 0; i < n; i++) {
			c = UGETW(us.bString[i]);
			if (swap)
				c = (c >> 8) | (c << 8);
			*s++ = (c < 0x80) ? c : '?';
		}
		*s++ = 0;
	}
#endif
	return (USBD_NORMAL_COMPLETION);
}

#if defined(__FreeBSD__)
int
usbd_driver_load(module_t mod, int what, void *arg)
{
	/* XXX should implement something like a function that removes all generic devices */

 	return (0);
}

#endif
