/*	$NetBSD: usb_port.h,v 1.79.6.1 2008/04/03 12:42:57 mjf Exp $	*/

/*
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

#ifndef _USB_PORT_H
#define _USB_PORT_H

/*
 * Macro's to cope with the differences between operating systems.
 */

#include "opt_usbverbose.h"

#if defined(_KERNEL)
#include <sys/mallocvar.h>

MALLOC_DECLARE(M_USB);
MALLOC_DECLARE(M_USBDEV);
MALLOC_DECLARE(M_USBHC);

#endif

#define USB_USE_SOFTINTR

#ifdef USB_DEBUG
#define UKBD_DEBUG 1
#define UHIDEV_DEBUG 1
#define UHID_DEBUG 1
#define OHCI_DEBUG 1
#define UGEN_DEBUG 1
#define UHCI_DEBUG 1
#define UHUB_DEBUG 1
#define ULPT_DEBUG 1
#define UCOM_DEBUG 1
#define UPLCOM_DEBUG 1
#define UMCT_DEBUG 1
#define UMODEM_DEBUG 1
#define UAUDIO_DEBUG 1
#define AUE_DEBUG 1
#define CUE_DEBUG 1
#define KUE_DEBUG 1
#define URL_DEBUG 1
#define UMASS_DEBUG 1
#define UVISOR_DEBUG 1
#define UPL_DEBUG 1
#define UZCOM_DEBUG 1
#define URIO_DEBUG 1
#define UFTDI_DEBUG 1
#define USCANNER_DEBUG 1
#define USSCANNER_DEBUG 1
#define EHCI_DEBUG 1
#define UIRDA_DEBUG 1
#define USTIR_DEBUG 1
#define UISDATA_DEBUG 1
#define UDSBR_DEBUG 1
#define UBT_DEBUG 1
#define AXE_DEBUG 1
#define UIPAQ_DEBUG 1
#define UCYCOM_DEBUG 1
#define Static
#else
#define Static static
#endif

typedef struct proc *usb_proc_ptr;

typedef struct device *device_ptr_t;
#define USBBASEDEVICE struct device
#define USBDEV(bdev) (&(bdev))
#define USBDEVNAME(bdev) ((bdev).dv_xname)
#define USBDEVUNIT(bdev) device_unit(&(bdev))
#define USBDEVPTRNAME(bdevptr) ((bdevptr)->dv_xname)
#define USBGETSOFTC(d) ((void *)(d))

#define DECLARE_USB_DMA_T \
	struct usb_dma_block; \
	typedef struct { \
		struct usb_dma_block *block; \
		u_int offs; \
	} usb_dma_t

typedef struct callout usb_callout_t;
#define usb_callout_init(h)	callout_init(&(h), 0)
#define usb_callout_destroy(h)	callout_destroy((&h))
#define	usb_callout(h, t, f, d)	callout_reset(&(h), (t), (f), (d))
#define	usb_uncallout(h, f, d)	callout_stop(&(h))

#define usb_kthread_create1		kthread_create
#define usb_kthread_create(f, a)	((f)(a))

typedef struct malloc_type *usb_malloc_type;

#define Ether_ifattach ether_ifattach
#define IF_INPUT(ifp, m) (*(ifp)->if_input)((ifp), (m))

#define logprintf printf

#define	USB_DNAME(dname)	dname
#define USB_DECLARE_DRIVER(dname)  \
int __CONCAT(dname,_match)(device_t, struct cfdata *, void *); \
void __CONCAT(dname,_attach)(device_t, device_t, void *); \
int __CONCAT(dname,_detach)(device_t, int); \
int __CONCAT(dname,_activate)(device_t, enum devact); \
\
extern struct cfdriver __CONCAT(dname,_cd); \
\
CFATTACH_DECL(USB_DNAME(dname), \
    sizeof(struct ___CONCAT(dname,_softc)), \
    ___CONCAT(dname,_match), \
    ___CONCAT(dname,_attach), \
    ___CONCAT(dname,_detach), \
    ___CONCAT(dname,_activate))

#define USB_MATCH(dname) \
int __CONCAT(dname,_match)(struct device *parent, \
    struct cfdata *match, void *aux)

#define USB_MATCH_START(dname, uaa) \
	struct usb_attach_arg *uaa = aux

#define USB_IFMATCH_START(dname, uaa) \
	struct usbif_attach_arg *uaa = aux

#define USB_ATTACH(dname) \
void __CONCAT(dname,_attach)(struct device *parent, \
    struct device *self, void *aux)

#define USB_ATTACH_START(dname, sc, uaa) \
	struct __CONCAT(dname,_softc) *sc = \
		(struct __CONCAT(dname,_softc) *)self; \
	struct usb_attach_arg *uaa = aux

#define USB_IFATTACH_START(dname, sc, uaa) \
	struct __CONCAT(dname,_softc) *sc = \
		(struct __CONCAT(dname,_softc) *)self; \
	struct usbif_attach_arg *uaa = aux

/* Returns from attach */
#define USB_ATTACH_ERROR_RETURN	return
#define USB_ATTACH_SUCCESS_RETURN	return

#define USB_ATTACH_SETUP \
	do { \
		aprint_naive("\n"); \
		aprint_normal("\n"); \
	} while (0)

#define USB_DETACH(dname) \
int __CONCAT(dname,_detach)(struct device *self, int flags)

#define USB_DETACH_START(dname, sc) \
	struct __CONCAT(dname,_softc) *sc = \
		(struct __CONCAT(dname,_softc) *)self

#define USB_GET_SC_OPEN(dname, unit, sc) \
	if (unit >= __CONCAT(dname,_cd).cd_ndevs) \
		return (ENXIO); \
	sc = __CONCAT(dname,_cd).cd_devs[unit]; \
	if (sc == NULL) \
		return (ENXIO)

#define USB_GET_SC(dname, unit, sc) \
	sc = __CONCAT(dname,_cd).cd_devs[unit]

#define USB_DO_ATTACH(dev, bdev, parent, args, print, sub) \
	(config_found_sm_loc(parent, "usbdevif", \
			     NULL, args, print, sub))

#define USB_DO_IFATTACH(dev, bdev, parent, args, print, sub) \
	(config_found_sm_loc(parent, "usbifif", \
			     NULL, args, print, sub))

#endif /* _USB_PORT_H */
