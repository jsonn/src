/*	$NetBSD: uhidev.h,v 1.7.36.1 2008/05/16 02:25:10 yamt Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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

#include "rnd.h"
#if NRND > 0
#include <sys/rnd.h>
#endif

struct uhidev_softc {
	USBBASEDEVICE sc_dev;		/* base device */
	usbd_device_handle sc_udev;
	usbd_interface_handle sc_iface;	/* interface */
	usbd_pipe_handle sc_ipipe;	/* input interrupt pipe */
	int sc_iep_addr;

	u_char *sc_ibuf;
	u_int sc_isize;

	usbd_pipe_handle sc_opipe;	/* output interrupt pipe */
	usbd_xfer_handle sc_oxfer;	/* write request */
	int sc_oep_addr;

	void *sc_repdesc;
	int sc_repdesc_size;

	u_int sc_nrepid;
	struct uhidev **sc_subdevs;

	int sc_refcnt;
	u_char sc_dying;
};

struct uhidev {
	USBBASEDEVICE sc_dev;		/* base device */
	struct uhidev_softc *sc_parent;
	uByte sc_report_id;
	u_int8_t sc_state;
	int sc_in_rep_size;
#define	UHIDEV_OPEN	0x01	/* device is open */
	void (*sc_intr)(struct uhidev *, void *, u_int);
#if NRND > 0
        rndsource_element_t     rnd_source;
#endif
};

struct uhidev_attach_arg {
	struct usbif_attach_arg *uaa;
	struct uhidev_softc *parent;
	int reportid;
	int reportsize;
};

void uhidev_get_report_desc(struct uhidev_softc *, void **, int *);
int uhidev_open(struct uhidev *);
void uhidev_close(struct uhidev *);
usbd_status uhidev_set_report(struct uhidev *scd, int type, void *data,int len);
void uhidev_set_report_async(struct uhidev *scd, int type, void *data, int len);
usbd_status uhidev_get_report(struct uhidev *scd, int type, void *data,int len);
usbd_status uhidev_write(struct uhidev_softc *, void *, int);
