/*	$NetBSD: usbdi.c,v 1.21.4.3 1999/08/02 22:09:00 thorpej Exp $	*/

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
#include <sys/kernel.h>
#if defined(__NetBSD__)
#include <sys/device.h>
#else
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#endif
#include <sys/malloc.h>
#include <sys/proc.h>

#include <dev/usb/usb.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#if defined(__FreeBSD__)
#include "usb_if.h"
#endif
 
#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) printf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) printf x
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

static usbd_status usbd_ar_pipe  __P((usbd_pipe_handle pipe));
static void usbd_transfer_cb __P((usbd_request_handle reqh));
static void usbd_sync_transfer_cb __P((usbd_request_handle reqh));
static usbd_status usbd_do_transfer __P((usbd_request_handle reqh));
void usbd_do_request_async_cb 
	__P((usbd_request_handle, usbd_private_handle, usbd_status));

static SIMPLEQ_HEAD(, usbd_request) usbd_free_requests;

#if defined(__FreeBSD__)
#define USB_CDEV_MAJOR	108

extern struct cdevsw usb_cdevsw;
#endif

usbd_status 
usbd_open_pipe(iface, address, flags, pipe)
	usbd_interface_handle iface;
	u_int8_t address;
	u_int8_t flags;
	usbd_pipe_handle *pipe;
{ 
	usbd_pipe_handle p;
	struct usbd_endpoint *ep;
	usbd_status r;
	int i;

	for (i = 0; i < iface->idesc->bNumEndpoints; i++) {
		ep = &iface->endpoints[i];
		if (ep->edesc->bEndpointAddress == address)
			goto found;
	}
	return (USBD_BAD_ADDRESS);
 found:
	if ((flags & USBD_EXCLUSIVE_USE) &&
	    ep->refcnt != 0)
		return (USBD_IN_USE);
	r = usbd_setup_pipe(iface->device, iface, ep, &p);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	LIST_INSERT_HEAD(&iface->pipes, p, next);
	*pipe = p;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_open_pipe_intr(iface, address, flags, pipe, priv, buffer, length, cb)
	usbd_interface_handle iface;
	u_int8_t address;
	u_int8_t flags;
	usbd_pipe_handle *pipe;
	usbd_private_handle priv;
	void *buffer;
	u_int32_t length;
	usbd_callback cb;
{
	usbd_status r;
	usbd_request_handle reqh;
	usbd_pipe_handle ipipe;

	reqh = usbd_alloc_request();
	if (reqh == 0)
		return (USBD_NOMEM);
	r = usbd_open_pipe(iface, address, USBD_EXCLUSIVE_USE, &ipipe);
	if (r != USBD_NORMAL_COMPLETION)
		goto bad1;
	r = usbd_setup_request(reqh, ipipe, priv, buffer, length, 
			       USBD_XFER_IN | flags, USBD_NO_TIMEOUT, cb);
	if (r != USBD_NORMAL_COMPLETION)
		goto bad2;
	ipipe->intrreqh = reqh;
	ipipe->repeat = 1;
	r = usbd_transfer(reqh);
	*pipe = ipipe;
	if (r != USBD_IN_PROGRESS)
		goto bad3;
	return (USBD_NORMAL_COMPLETION);

 bad3:
	ipipe->intrreqh = 0;
	ipipe->repeat = 0;
 bad2:
	usbd_close_pipe(ipipe);
 bad1:
	usbd_free_request(reqh);
	return r;
}

usbd_status 
usbd_open_pipe_iso(iface, address, flags, pipe, priv, bufsize, nbuf, cb)
	usbd_interface_handle iface;
	u_int8_t address;
	u_int8_t flags;
	usbd_pipe_handle *pipe;
	usbd_private_handle priv;
	u_int32_t bufsize;
	u_int32_t nbuf;
	usbd_callback cb;
{
	usbd_status r;
	usbd_pipe_handle p;

	r = usbd_open_pipe(iface, address, USBD_EXCLUSIVE_USE, &p);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	if (!p->methods->isobuf) {
		usbd_close_pipe(p);
		return (USBD_INVAL);
	}
	r = p->methods->isobuf(p, bufsize, nbuf);
	if (r != USBD_NORMAL_COMPLETION) {
		usbd_close_pipe(p);
		return (r);
	}	
	*pipe = p;
	return r;
}

usbd_status
usbd_close_pipe(pipe)
	usbd_pipe_handle pipe;
{
#ifdef DIAGNOSTIC
	if (pipe == 0) {
		printf("usbd_close_pipe: pipe==NULL\n");
		return (USBD_NORMAL_COMPLETION);
	}
#endif

	if (--pipe->refcnt != 0)
		return (USBD_NORMAL_COMPLETION);
	if (SIMPLEQ_FIRST(&pipe->queue) != 0)
		return (USBD_PENDING_REQUESTS);
	LIST_REMOVE(pipe, next);
	pipe->endpoint->refcnt--;
	pipe->methods->close(pipe);
	if (pipe->intrreqh)
		usbd_free_request(pipe->intrreqh);
	free(pipe, M_USB);
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_transfer(reqh)
	usbd_request_handle reqh;
{
	reqh->xfercb = usbd_transfer_cb;
	return (usbd_do_transfer(reqh));
}

static usbd_status
usbd_do_transfer(reqh)
	usbd_request_handle reqh;
{
	usbd_pipe_handle pipe = reqh->pipe;

	DPRINTFN(10,("usbd_do_transfer: reqh=%p\n", reqh));
	reqh->done = 0;
	return (pipe->methods->transfer(reqh));
}

usbd_request_handle 
usbd_alloc_request()
{
	usbd_request_handle reqh;

	reqh = SIMPLEQ_FIRST(&usbd_free_requests);
	if (reqh)
		SIMPLEQ_REMOVE_HEAD(&usbd_free_requests, reqh, next);
	else
		reqh = malloc(sizeof(*reqh), M_USB, M_NOWAIT);
	if (!reqh)
		return (0);
	memset(reqh, 0, sizeof *reqh);
	DPRINTFN(1,("usbd_alloc_request() = %p\n", reqh));
	return (reqh);
}

usbd_status 
usbd_free_request(reqh)
	usbd_request_handle reqh;
{
	DPRINTFN(1,("usbd_free_request: %p\n", reqh));
	SIMPLEQ_INSERT_HEAD(&usbd_free_requests, reqh, next);
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_setup_request(reqh, pipe, priv, buffer, length, flags, timeout, callback)
	usbd_request_handle reqh;
	usbd_pipe_handle pipe;
	usbd_private_handle priv;
	void *buffer;
	u_int32_t length;
	u_int16_t flags;
	u_int32_t timeout;
	void (*callback) __P((usbd_request_handle,
			      usbd_private_handle,
			      usbd_status));
{
	reqh->pipe = pipe;
	reqh->priv = priv;
	reqh->buffer = buffer;
	reqh->length = length;
	reqh->actlen = 0;
	reqh->flags = flags;
	reqh->timeout = timeout;
	reqh->status = USBD_NOT_STARTED;
	reqh->callback = callback;
	reqh->retries = 1;
	reqh->isreq = 0;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_setup_default_request(reqh, dev, priv, timeout, req, buffer, 
			   length, flags, callback)
	usbd_request_handle reqh;
	usbd_device_handle dev;
	usbd_private_handle priv;
	u_int32_t timeout;
	usb_device_request_t *req;
	void *buffer;
	u_int32_t length;
	u_int16_t flags;
	void (*callback) __P((usbd_request_handle,
			      usbd_private_handle,
			      usbd_status));
{
	reqh->pipe = dev->default_pipe;
	reqh->priv = priv;
	reqh->buffer = buffer;
	reqh->length = length;
	reqh->actlen = 0;
	reqh->flags = flags;
	reqh->timeout = timeout;
	reqh->status = USBD_NOT_STARTED;
	reqh->callback = callback;
	reqh->request = *req;
	reqh->retries = 1;
	reqh->isreq = 1;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_get_request_status(reqh, priv, buffer, count, status)
	usbd_request_handle reqh;
	usbd_private_handle *priv;
	void **buffer;
	u_int32_t *count;
	usbd_status *status;
{
	*priv = reqh->priv;
	*buffer = reqh->buffer;
	*count = reqh->actlen;
	*status = reqh->status;
	return (USBD_NORMAL_COMPLETION);
}

usb_config_descriptor_t *
usbd_get_config_descriptor(dev)
	usbd_device_handle dev;
{
	return (dev->cdesc);
}

usb_interface_descriptor_t *
usbd_get_interface_descriptor(iface)
	usbd_interface_handle iface;
{
	return (iface->idesc);
}

usb_device_descriptor_t *
usbd_get_device_descriptor(dev)
	usbd_device_handle dev;
{
	return (&dev->ddesc);
}

usb_endpoint_descriptor_t *
usbd_interface2endpoint_descriptor(iface, index)
	usbd_interface_handle iface;
	u_int8_t index;
{
	if (index >= iface->idesc->bNumEndpoints)
		return (0);
	return (iface->endpoints[index].edesc);
}

usbd_status 
usbd_abort_pipe(pipe)
	usbd_pipe_handle pipe;
{
	usbd_status r;
	int s;

#ifdef DIAGNOSTIC
	if (pipe == 0) {
		printf("usbd_close_pipe: pipe==NULL\n");
		return (USBD_NORMAL_COMPLETION);
	}
#endif
	s = splusb();
	r = usbd_ar_pipe(pipe);
	splx(s);
	return (r);
}
	
usbd_status 
usbd_clear_endpoint_stall(pipe)
	usbd_pipe_handle pipe;
{
	usbd_device_handle dev = pipe->device;
	usb_device_request_t req;
	usbd_status r;

	DPRINTFN(8, ("usbd_clear_endpoint_stall\n"));
	req.bmRequestType = UT_WRITE_ENDPOINT;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, UF_ENDPOINT_HALT);
	USETW(req.wIndex, pipe->endpoint->edesc->bEndpointAddress);
	USETW(req.wLength, 0);
	r = usbd_do_request(dev, &req, 0);
#if 0
XXX should we do this?
	if (r == USBD_NORMAL_COMPLETION) {
		pipe->state = USBD_PIPE_ACTIVE;
		/* XXX activate pipe */
	}
#endif
	return (r);
}

usbd_status 
usbd_clear_endpoint_stall_async(pipe)
	usbd_pipe_handle pipe;
{
	usbd_device_handle dev = pipe->device;
	usb_device_request_t req;
	usbd_status r;

	req.bmRequestType = UT_WRITE_ENDPOINT;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, UF_ENDPOINT_HALT);
	USETW(req.wIndex, pipe->endpoint->edesc->bEndpointAddress);
	USETW(req.wLength, 0);
	r = usbd_do_request_async(dev, &req, 0);
	return (r);
}

usbd_status 
usbd_endpoint_count(iface, count)
	usbd_interface_handle iface;
	u_int8_t *count;
{
	*count = iface->idesc->bNumEndpoints;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_interface_count(dev, count)
	usbd_device_handle dev;
	u_int8_t *count;
{
	if (!dev->cdesc)
		return (USBD_NOT_CONFIGURED);
	*count = dev->cdesc->bNumInterface;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_interface2device_handle(iface, dev)
	usbd_interface_handle iface;
	usbd_device_handle *dev;
{
	*dev = iface->device;
	return (USBD_NORMAL_COMPLETION);
}

usbd_status 
usbd_device2interface_handle(dev, ifaceno, iface)
	usbd_device_handle dev;
	u_int8_t ifaceno;
	usbd_interface_handle *iface;
{
	if (!dev->cdesc)
		return (USBD_NOT_CONFIGURED);
	if (ifaceno >= dev->cdesc->bNumInterface)
		return (USBD_INVAL);
	*iface = &dev->ifaces[ifaceno];
	return (USBD_NORMAL_COMPLETION);
}

/* XXXX use altno */
usbd_status
usbd_set_interface(iface, altidx)
	usbd_interface_handle iface;
	int altidx;
{
	usb_device_request_t req;
	usbd_status r;

	if (LIST_FIRST(&iface->pipes) != 0)
		return (USBD_IN_USE);

	if (iface->endpoints)
		free(iface->endpoints, M_USB);
	iface->endpoints = 0;
	iface->idesc = 0;

	r = usbd_fill_iface_data(iface->device, iface->index, altidx);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);

	req.bmRequestType = UT_WRITE_INTERFACE;
	req.bRequest = UR_SET_INTERFACE;
	USETW(req.wValue, iface->idesc->bAlternateSetting);
	USETW(req.wIndex, iface->idesc->bInterfaceNumber);
	USETW(req.wLength, 0);
	return usbd_do_request(iface->device, &req, 0);
}

int
usbd_get_no_alts(cdesc, ifaceno)
	usb_config_descriptor_t *cdesc;
	int ifaceno;
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
usbd_get_interface_altindex(iface)
	usbd_interface_handle iface;
{
	return (iface->altindex);
}

usbd_status
usbd_get_interface(iface, aiface)
	usbd_interface_handle iface;
	u_int8_t *aiface;
{
	usb_device_request_t req;

	req.bmRequestType = UT_READ_INTERFACE;
	req.bRequest = UR_GET_INTERFACE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, iface->idesc->bInterfaceNumber);
	USETW(req.wLength, 1);
	return usbd_do_request(iface->device, &req, aiface);
}

/*** Internal routines ***/

/* Dequeue all pipe operations, called at splusb(). */
static usbd_status
usbd_ar_pipe(pipe)
	usbd_pipe_handle pipe;
{
	usbd_request_handle reqh;

#if 0
	for (;;) {
		reqh = SIMPLEQ_FIRST(&pipe->queue);
		if (reqh == 0)
			break;
		SIMPLEQ_REMOVE_HEAD(&pipe->queue, reqh, next);
		reqh->status = USBD_CANCELLED;
		if (reqh->callback)
			reqh->callback(reqh, reqh->priv, reqh->status);
	}
#else
	DPRINTFN(2,("usbd_ar_pipe: pipe=%p\n", pipe));
	while ((reqh = SIMPLEQ_FIRST(&pipe->queue))) {
		DPRINTFN(2,("usbd_ar_pipe: reqh=%p (methods=%p)\n", 
			    pipe, pipe->methods));
		pipe->methods->abort(reqh);
		SIMPLEQ_REMOVE_HEAD(&pipe->queue, reqh, next);
	}
#endif
	return (USBD_NORMAL_COMPLETION);
}

static int usbd_global_init_done = 0;

void
usbd_init()
{
#if defined(__FreeBSD__)
	dev_t dev;
#endif
	
	if (!usbd_global_init_done) {
		usbd_global_init_done = 1;
		SIMPLEQ_INIT(&usbd_free_requests);

#if defined(__FreeBSD__)
		dev = makedev(USB_CDEV_MAJOR, 0);
		cdevsw_add(&dev, &usb_cdevsw, NULL);
#endif
	}
}

static void
usbd_transfer_cb(reqh)
	usbd_request_handle reqh;
{
	usbd_pipe_handle pipe = reqh->pipe;

	DPRINTFN(10, ("usbd_transfer_cb: reqh=%p\n", reqh));
	/* Count completed transfers. */
#ifdef DIAGNOSTIC
	if (!pipe)
		printf("usbd_transfer_cb: pipe==0, reqh=%p\n", reqh);
	else
#endif
	++pipe->device->bus->stats.requests
		[pipe->endpoint->edesc->bmAttributes & UE_XFERTYPE];

	/* XXX check retry count */
	reqh->done = 1;
	if (reqh->status == USBD_NORMAL_COMPLETION &&
	    reqh->actlen < reqh->length &&
	    !(reqh->flags & USBD_SHORT_XFER_OK)) {
		DPRINTFN(-1, ("usbd_transfer_cb: short xfer %d<%d (bytes)\n",
			      reqh->actlen, reqh->length));
		reqh->status = USBD_SHORT_XFER;
	}
	if (reqh->callback)
		reqh->callback(reqh, reqh->priv, reqh->status);
}

static void
usbd_sync_transfer_cb(reqh)
	usbd_request_handle reqh;
{
	DPRINTFN(10, ("usbd_sync_transfer_cb: reqh=%p\n", reqh));
	usbd_transfer_cb(reqh);
	if (!reqh->pipe->device->bus->use_polling)
		wakeup(reqh);
}

/* Like usbd_transfer(), but waits for completion. */
usbd_status
usbd_sync_transfer(reqh)
	usbd_request_handle reqh;
{
	usbd_status r;
	int s;

	reqh->xfercb = usbd_sync_transfer_cb;
	r = usbd_do_transfer(reqh);
	if (r != USBD_IN_PROGRESS)
		return (r);
	s = splusb();
	if (!reqh->done) {
		if (reqh->pipe->device->bus->use_polling)
			panic("usbd_sync_transfer: not done\n");
		tsleep(reqh, PRIBIO, "usbsyn", 0);
	}
	splx(s);
	return (reqh->status);
}

usbd_status
usbd_do_request(dev, req, data)
	usbd_device_handle dev;
	usb_device_request_t *req;
	void *data;
{
	return (usbd_do_request_flags(dev, req, data, 0, 0));
}

usbd_status
usbd_do_request_flags(dev, req, data, flags, actlen)
	usbd_device_handle dev;
	usb_device_request_t *req;
	void *data;
	u_int16_t flags;
	int *actlen;
{
	usbd_request_handle reqh;
	usbd_status r;

#ifdef DIAGNOSTIC
	if (!curproc) {
		printf("usbd_do_request: not in process context\n");
		return (USBD_XXX);
	}
#endif

	reqh = usbd_alloc_request();
	if (reqh == 0)
		return (USBD_NOMEM);
	r = usbd_setup_default_request(
		reqh, dev, 0, USBD_DEFAULT_TIMEOUT, req, data, 
		UGETW(req->wLength), flags, 0);
	if (r != USBD_NORMAL_COMPLETION)
		goto bad;
	r = usbd_sync_transfer(reqh);
#if defined(USB_DEBUG) || defined(DIAGNOSTIC)
	if (reqh->actlen > reqh->length)
		printf("usbd_do_request: overrun addr=%d type=0x%02x req=0x"
		       "%02x val=%d index=%d rlen=%d length=%d actlen=%d\n",
		       dev->address, reqh->request.bmRequestType,
		       reqh->request.bRequest, UGETW(reqh->request.wValue),
		       UGETW(reqh->request.wIndex), 
		       UGETW(reqh->request.wLength), 
		       reqh->length, reqh->actlen);
#endif
	if (actlen)
		*actlen = reqh->actlen;
	if (r == USBD_STALLED) {
		/* 
		 * The control endpoint has stalled.  Control endpoints
		 * should not halt, but some may do so anyway so clear
		 * any halt condition.
		 */
		usb_device_request_t treq;
		usb_status_t status;
		u_int16_t s;
		usbd_status nr;

		treq.bmRequestType = UT_READ_ENDPOINT;
		treq.bRequest = UR_GET_STATUS;
		USETW(treq.wValue, 0);
		USETW(treq.wIndex, 0);
		USETW(treq.wLength, sizeof(usb_status_t));
		nr = usbd_setup_default_request(
			reqh, dev, 0, USBD_DEFAULT_TIMEOUT, &treq, &status, 
			sizeof(usb_status_t), 0, 0);
		if (nr != USBD_NORMAL_COMPLETION)
			goto bad;
		nr = usbd_sync_transfer(reqh);
		if (nr != USBD_NORMAL_COMPLETION)
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
		nr = usbd_setup_default_request(
			reqh, dev, 0, USBD_DEFAULT_TIMEOUT, &treq, &status, 
			0, 0, 0);
		if (nr != USBD_NORMAL_COMPLETION)
			goto bad;
		nr = usbd_sync_transfer(reqh);
		if (nr != USBD_NORMAL_COMPLETION)
			goto bad;
	}

 bad:
	usbd_free_request(reqh);
	return (r);
}

void
usbd_do_request_async_cb(reqh, priv, status)
	usbd_request_handle reqh;
	usbd_private_handle priv;
	usbd_status status;
{
#if defined(USB_DEBUG) || defined(DIAGNOSTIC)
	if (reqh->actlen > reqh->length)
		printf("usbd_do_request: overrun addr=%d type=0x%02x req=0x"
		       "%02x val=%d index=%d rlen=%d length=%d actlen=%d\n",
		       reqh->pipe->device->address, 
		       reqh->request.bmRequestType,
		       reqh->request.bRequest, UGETW(reqh->request.wValue),
		       UGETW(reqh->request.wIndex), 
		       UGETW(reqh->request.wLength), 
		       reqh->length, reqh->actlen);
#endif
	usbd_free_request(reqh);
}

/*
 * Execute a request without waiting for completion.
 * Can be used from interrupt context.
 */
usbd_status
usbd_do_request_async(dev, req, data)
	usbd_device_handle dev;
	usb_device_request_t *req;
	void *data;
{
	usbd_request_handle reqh;
	usbd_status r;

	reqh = usbd_alloc_request();
	if (reqh == 0)
		return (USBD_NOMEM);
	r = usbd_setup_default_request(
		reqh, dev, 0, USBD_DEFAULT_TIMEOUT, req, data, 
		UGETW(req->wLength), 0, usbd_do_request_async_cb);
	if (r != USBD_NORMAL_COMPLETION) {
		usbd_free_request(reqh);
		return (r);
	}
	r = usbd_transfer(reqh);
	if (r != USBD_IN_PROGRESS)
		return (r);
	return (USBD_NORMAL_COMPLETION);
}

struct usbd_quirks *
usbd_get_quirks(dev)
	usbd_device_handle dev;
{
	return (dev->quirks);
}

/* XXX do periodic free() of free list */

/*
 * Called from keyboard driver when in polling mode.
 */
void
usbd_dopoll(iface)
	usbd_interface_handle iface;
{
	iface->device->bus->do_poll(iface->device->bus);
}

void
usbd_set_polling(iface, on)
	usbd_interface_handle iface;
	int on;
{
	iface->device->bus->use_polling = on;
}


usb_endpoint_descriptor_t *
usbd_get_endpoint_descriptor(iface, address)
	usbd_interface_handle iface;
	u_int8_t address;
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

#if defined(__FreeBSD__)
void
usbd_print_child(device_t parent, device_t child)
{
	/*
	struct usb_softc *sc = device_get_softc(child);
	*/

	printf(" at %s%d", device_get_name(parent), device_get_unit(parent));

	/* XXX How do we get to the usbd_device_handle???
	usbd_device_handle dev = invalidadosch;

	printf(" addr %d", dev->addr);

	if (bootverbose) {
		if (dev->lowspeed)
			printf(", lowspeed");
		if (dev->self_powered)
			printf(", self powered");
		else
			printf(", %dmA", dev->power);
		printf(", config %d", dev->config);
	}
	 */
}

/* Reconfigure all the USB busses in the system. */
int
usbd_driver_load(module_t mod, int what, void *arg)
{
	devclass_t usb_devclass = devclass_find("usb");
	devclass_t ugen_devclass = devclass_find("ugen");
	device_t *devlist;
	int devcount;
	int error;

	switch (what) { 
	case MOD_LOAD:
	case MOD_UNLOAD:
		if (!usb_devclass)
			return 0;	/* just ignore call */

		if (ugen_devclass) {
			/* detach devices from generic driver if possible */
			error = devclass_get_devices(ugen_devclass, &devlist,
						     &devcount);
			if (!error)
				for (devcount--; devcount >= 0; devcount--)
					(void)DEVICE_DETACH(devlist[devcount]);
		}

		error = devclass_get_devices(usb_devclass, &devlist, &devcount);
		if (error)
			return 0;	/* XXX maybe transient, or error? */

		for (devcount--; devcount >= 0; devcount--)
			USB_RECONFIGURE(devlist[devcount]);

		free(devlist, M_TEMP);
		return 0;
	}

	return 0;			/* nothing to do by us */
}

/* Set the description of the device including a malloc and copy. */
void
usbd_device_set_desc(device_t device, char *devinfo)
{
	size_t l;
	char *desc;

	if ( devinfo ) {
		l = strlen(devinfo);
		desc = malloc(l+1, M_USB, M_NOWAIT);
		if (desc)
			memcpy(desc, devinfo, l+1);
	} else
		desc = NULL;

	device_set_desc(device, desc);
}

char *
usbd_devname(bdevice *bdev)
{
	static char buf[20];
	/* 
	 * A static buffer is a loss if this routine is used from an interrupt,
	 * but it's not fatal.
	 */

	sprintf(buf, "%s%d", device_get_name(*bdev), device_get_unit(*bdev));
	return (buf);
}

#endif
