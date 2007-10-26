/*	$NetBSD: ehci.c,v 1.123.18.5 2007/10/26 15:47:47 joerg Exp $ */

/*
 * Copyright (c) 2004,2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) and by Charles M. Hannum.
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
 * USB Enhanced Host Controller Driver, a.k.a. USB 2.0 controller.
 *
 * The EHCI 1.0 spec can be found at
 * http://developer.intel.com/technology/usb/download/ehci-r10.pdf
 * and the USB 2.0 spec at
 * http://www.usb.org/developers/docs/usb_20.zip
 *
 */

/*
 * TODO:
 * 1) hold off explorations by companion controllers until ehci has started.
 *
 * 2) The EHCI driver lacks support for isochronous transfers, so
 *    devices using them don't work.
 *
 * 3) The hub driver needs to handle and schedule the transaction translator,
 *    to assign place in frame where different devices get to go. See chapter
 *    on hubs in USB 2.0 for details.
 *
 * 4) command failures are not recovered correctly
*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ehci.c,v 1.123.18.5 2007/10/26 15:47:47 joerg Exp $");

#include "ohci.h"
#include "uhci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <sys/bus.h>
#include <machine/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#ifdef EHCI_DEBUG
#define DPRINTF(x)	do { if (ehcidebug) printf x; } while(0)
#define DPRINTFN(n,x)	do { if (ehcidebug>(n)) printf x; } while (0)
int ehcidebug = 0;
#ifndef __NetBSD__
#define bitmask_snprintf(q,f,b,l) snprintf((b), (l), "%b", (q), (f))
#endif
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct ehci_pipe {
	struct usbd_pipe pipe;
	int nexttoggle;

	ehci_soft_qh_t *sqh;
	union {
		ehci_soft_qtd_t *qtd;
		/* ehci_soft_itd_t *itd; */
	} tail;
	union {
		/* Control pipe */
		struct {
			usb_dma_t reqdma;
			u_int length;
		} ctl;
		/* Interrupt pipe */
		struct {
			u_int length;
		} intr;
		/* Bulk pipe */
		struct {
			u_int length;
		} bulk;
		/* Iso pipe */
		/* XXX */
	} u;
};

Static void		ehci_shutdown(void *);

Static usbd_status	ehci_open(usbd_pipe_handle);
Static void		ehci_poll(struct usbd_bus *);
Static void		ehci_softintr(void *);
Static int		ehci_intr1(ehci_softc_t *);
Static void		ehci_waitintr(ehci_softc_t *, usbd_xfer_handle);
Static void		ehci_check_intr(ehci_softc_t *, struct ehci_xfer *);
Static void		ehci_idone(struct ehci_xfer *);
Static void		ehci_timeout(void *);
Static void		ehci_timeout_task(void *);
Static void		ehci_intrlist_timeout(void *);

Static usbd_status	ehci_allocm(struct usbd_bus *, usb_dma_t *, u_int32_t);
Static void		ehci_freem(struct usbd_bus *, usb_dma_t *);

Static usbd_xfer_handle	ehci_allocx(struct usbd_bus *);
Static void		ehci_freex(struct usbd_bus *, usbd_xfer_handle);

Static usbd_status	ehci_root_ctrl_transfer(usbd_xfer_handle);
Static usbd_status	ehci_root_ctrl_start(usbd_xfer_handle);
Static void		ehci_root_ctrl_abort(usbd_xfer_handle);
Static void		ehci_root_ctrl_close(usbd_pipe_handle);
Static void		ehci_root_ctrl_done(usbd_xfer_handle);

Static usbd_status	ehci_root_intr_transfer(usbd_xfer_handle);
Static usbd_status	ehci_root_intr_start(usbd_xfer_handle);
Static void		ehci_root_intr_abort(usbd_xfer_handle);
Static void		ehci_root_intr_close(usbd_pipe_handle);
Static void		ehci_root_intr_done(usbd_xfer_handle);

Static usbd_status	ehci_device_ctrl_transfer(usbd_xfer_handle);
Static usbd_status	ehci_device_ctrl_start(usbd_xfer_handle);
Static void		ehci_device_ctrl_abort(usbd_xfer_handle);
Static void		ehci_device_ctrl_close(usbd_pipe_handle);
Static void		ehci_device_ctrl_done(usbd_xfer_handle);

Static usbd_status	ehci_device_bulk_transfer(usbd_xfer_handle);
Static usbd_status	ehci_device_bulk_start(usbd_xfer_handle);
Static void		ehci_device_bulk_abort(usbd_xfer_handle);
Static void		ehci_device_bulk_close(usbd_pipe_handle);
Static void		ehci_device_bulk_done(usbd_xfer_handle);

Static usbd_status	ehci_device_intr_transfer(usbd_xfer_handle);
Static usbd_status	ehci_device_intr_start(usbd_xfer_handle);
Static void		ehci_device_intr_abort(usbd_xfer_handle);
Static void		ehci_device_intr_close(usbd_pipe_handle);
Static void		ehci_device_intr_done(usbd_xfer_handle);

Static usbd_status	ehci_device_isoc_transfer(usbd_xfer_handle);
Static usbd_status	ehci_device_isoc_start(usbd_xfer_handle);
Static void		ehci_device_isoc_abort(usbd_xfer_handle);
Static void		ehci_device_isoc_close(usbd_pipe_handle);
Static void		ehci_device_isoc_done(usbd_xfer_handle);

Static void		ehci_device_clear_toggle(usbd_pipe_handle pipe);
Static void		ehci_noop(usbd_pipe_handle pipe);

Static int		ehci_str(usb_string_descriptor_t *, int, const char *);
Static void		ehci_pcd(ehci_softc_t *, usbd_xfer_handle);
Static void		ehci_disown(ehci_softc_t *, int, int);

Static ehci_soft_qh_t  *ehci_alloc_sqh(ehci_softc_t *);
Static void		ehci_free_sqh(ehci_softc_t *, ehci_soft_qh_t *);

Static ehci_soft_qtd_t  *ehci_alloc_sqtd(ehci_softc_t *);
Static void		ehci_free_sqtd(ehci_softc_t *, ehci_soft_qtd_t *);
Static usbd_status	ehci_alloc_sqtd_chain(struct ehci_pipe *,
			    ehci_softc_t *, int, int, usbd_xfer_handle,
			    ehci_soft_qtd_t **, ehci_soft_qtd_t **);
Static void		ehci_free_sqtd_chain(ehci_softc_t *, ehci_soft_qtd_t *,
					    ehci_soft_qtd_t *);

Static usbd_status	ehci_device_request(usbd_xfer_handle xfer);

Static usbd_status	ehci_device_setintr(ehci_softc_t *, ehci_soft_qh_t *,
			    int ival);

Static void		ehci_add_qh(ehci_soft_qh_t *, ehci_soft_qh_t *);
Static void		ehci_rem_qh(ehci_softc_t *, ehci_soft_qh_t *,
				    ehci_soft_qh_t *);
Static void		ehci_set_qh_qtd(ehci_soft_qh_t *, ehci_soft_qtd_t *);
Static void		ehci_sync_hc(ehci_softc_t *);

Static void		ehci_close_pipe(usbd_pipe_handle, ehci_soft_qh_t *);
Static void		ehci_abort_xfer(usbd_xfer_handle, usbd_status);

#ifdef EHCI_DEBUG
Static void		ehci_dump_regs(ehci_softc_t *);
void			ehci_dump(void);
Static ehci_softc_t 	*theehci;
Static void		ehci_dump_link(ehci_link_t, int);
Static void		ehci_dump_sqtds(ehci_soft_qtd_t *);
Static void		ehci_dump_sqtd(ehci_soft_qtd_t *);
Static void		ehci_dump_qtd(ehci_qtd_t *);
Static void		ehci_dump_sqh(ehci_soft_qh_t *);
#ifdef DIAGNOSTIC
Static void		ehci_dump_exfer(struct ehci_xfer *);
#endif
#endif

#define EHCI_NULL htole32(EHCI_LINK_TERMINATE)

#define EHCI_INTR_ENDPT 1

#define ehci_add_intr_list(sc, ex) \
	LIST_INSERT_HEAD(&(sc)->sc_intrhead, (ex), inext);
#define ehci_del_intr_list(ex) \
	do { \
		LIST_REMOVE((ex), inext); \
		(ex)->inext.le_prev = NULL; \
	} while (0)
#define ehci_active_intr_list(ex) ((ex)->inext.le_prev != NULL)

Static const struct usbd_bus_methods ehci_bus_methods = {
	ehci_open,
	ehci_softintr,
	ehci_poll,
	ehci_allocm,
	ehci_freem,
	ehci_allocx,
	ehci_freex,
};

Static const struct usbd_pipe_methods ehci_root_ctrl_methods = {
	ehci_root_ctrl_transfer,
	ehci_root_ctrl_start,
	ehci_root_ctrl_abort,
	ehci_root_ctrl_close,
	ehci_noop,
	ehci_root_ctrl_done,
};

Static const struct usbd_pipe_methods ehci_root_intr_methods = {
	ehci_root_intr_transfer,
	ehci_root_intr_start,
	ehci_root_intr_abort,
	ehci_root_intr_close,
	ehci_noop,
	ehci_root_intr_done,
};

Static const struct usbd_pipe_methods ehci_device_ctrl_methods = {
	ehci_device_ctrl_transfer,
	ehci_device_ctrl_start,
	ehci_device_ctrl_abort,
	ehci_device_ctrl_close,
	ehci_noop,
	ehci_device_ctrl_done,
};

Static const struct usbd_pipe_methods ehci_device_intr_methods = {
	ehci_device_intr_transfer,
	ehci_device_intr_start,
	ehci_device_intr_abort,
	ehci_device_intr_close,
	ehci_device_clear_toggle,
	ehci_device_intr_done,
};

Static const struct usbd_pipe_methods ehci_device_bulk_methods = {
	ehci_device_bulk_transfer,
	ehci_device_bulk_start,
	ehci_device_bulk_abort,
	ehci_device_bulk_close,
	ehci_device_clear_toggle,
	ehci_device_bulk_done,
};

Static const struct usbd_pipe_methods ehci_device_isoc_methods = {
	ehci_device_isoc_transfer,
	ehci_device_isoc_start,
	ehci_device_isoc_abort,
	ehci_device_isoc_close,
	ehci_noop,
	ehci_device_isoc_done,
};

static const uint8_t revbits[EHCI_MAX_POLLRATE] = {
0x00,0x40,0x20,0x60,0x10,0x50,0x30,0x70,0x08,0x48,0x28,0x68,0x18,0x58,0x38,0x78,
0x04,0x44,0x24,0x64,0x14,0x54,0x34,0x74,0x0c,0x4c,0x2c,0x6c,0x1c,0x5c,0x3c,0x7c,
0x02,0x42,0x22,0x62,0x12,0x52,0x32,0x72,0x0a,0x4a,0x2a,0x6a,0x1a,0x5a,0x3a,0x7a,
0x06,0x46,0x26,0x66,0x16,0x56,0x36,0x76,0x0e,0x4e,0x2e,0x6e,0x1e,0x5e,0x3e,0x7e,
0x01,0x41,0x21,0x61,0x11,0x51,0x31,0x71,0x09,0x49,0x29,0x69,0x19,0x59,0x39,0x79,
0x05,0x45,0x25,0x65,0x15,0x55,0x35,0x75,0x0d,0x4d,0x2d,0x6d,0x1d,0x5d,0x3d,0x7d,
0x03,0x43,0x23,0x63,0x13,0x53,0x33,0x73,0x0b,0x4b,0x2b,0x6b,0x1b,0x5b,0x3b,0x7b,
0x07,0x47,0x27,0x67,0x17,0x57,0x37,0x77,0x0f,0x4f,0x2f,0x6f,0x1f,0x5f,0x3f,0x7f,
};

usbd_status
ehci_init(ehci_softc_t *sc)
{
	u_int32_t vers, sparams, cparams, hcr;
	u_int i;
	usbd_status err;
	ehci_soft_qh_t *sqh;
	u_int ncomp;

	DPRINTF(("ehci_init: start\n"));
#ifdef EHCI_DEBUG
	theehci = sc;
#endif

	sc->sc_offs = EREAD1(sc, EHCI_CAPLENGTH);

	vers = EREAD2(sc, EHCI_HCIVERSION);
	aprint_verbose("%s: EHCI version %x.%x\n", USBDEVNAME(sc->sc_bus.bdev),
	       vers >> 8, vers & 0xff);

	sparams = EREAD4(sc, EHCI_HCSPARAMS);
	DPRINTF(("ehci_init: sparams=0x%x\n", sparams));
	sc->sc_npcomp = EHCI_HCS_N_PCC(sparams);
	ncomp = EHCI_HCS_N_CC(sparams);
	if (ncomp != sc->sc_ncomp) {
		aprint_verbose("%s: wrong number of companions (%d != %d)\n",
		       USBDEVNAME(sc->sc_bus.bdev),
		       ncomp, sc->sc_ncomp);
#if NOHCI == 0 || NUHCI == 0
		aprint_error("%s: ohci or uhci probably not configured\n",
			     USBDEVNAME(sc->sc_bus.bdev));
#endif
		if (ncomp < sc->sc_ncomp)
			sc->sc_ncomp = ncomp;
	}
	if (sc->sc_ncomp > 0) {
		aprint_normal("%s: companion controller%s, %d port%s each:",
		    USBDEVNAME(sc->sc_bus.bdev), sc->sc_ncomp!=1 ? "s" : "",
		    EHCI_HCS_N_PCC(sparams),
		    EHCI_HCS_N_PCC(sparams)!=1 ? "s" : "");
		for (i = 0; i < sc->sc_ncomp; i++)
			aprint_normal(" %s", USBDEVNAME(sc->sc_comps[i]->bdev));
		aprint_normal("\n");
	}
	sc->sc_noport = EHCI_HCS_N_PORTS(sparams);
	cparams = EREAD4(sc, EHCI_HCCPARAMS);
	DPRINTF(("ehci_init: cparams=0x%x\n", cparams));
	sc->sc_hasppc = EHCI_HCS_PPC(sparams);

	if (EHCI_HCC_64BIT(cparams)) {
		/* MUST clear segment register if 64 bit capable. */
		EWRITE4(sc, EHCI_CTRLDSSEGMENT, 0);
	}

	sc->sc_bus.usbrev = USBREV_2_0;

	usb_setup_reserve(sc, &sc->sc_dma_reserve, sc->sc_bus.dmatag,
	    USB_MEM_RESERVE);

	/* Reset the controller */
	DPRINTF(("%s: resetting\n", USBDEVNAME(sc->sc_bus.bdev)));
	EOWRITE4(sc, EHCI_USBCMD, 0);	/* Halt controller */
	usb_delay_ms(&sc->sc_bus, 1);
	EOWRITE4(sc, EHCI_USBCMD, EHCI_CMD_HCRESET);
	for (i = 0; i < 100; i++) {
		usb_delay_ms(&sc->sc_bus, 1);
		hcr = EOREAD4(sc, EHCI_USBCMD) & EHCI_CMD_HCRESET;
		if (!hcr)
			break;
	}
	if (hcr) {
		aprint_error("%s: reset timeout\n",
		    USBDEVNAME(sc->sc_bus.bdev));
		return (USBD_IOERROR);
	}

	/* XXX need proper intr scheduling */
	sc->sc_rand = 96;

	/* frame list size at default, read back what we got and use that */
	switch (EHCI_CMD_FLS(EOREAD4(sc, EHCI_USBCMD))) {
	case 0: sc->sc_flsize = 1024; break;
	case 1: sc->sc_flsize = 512; break;
	case 2: sc->sc_flsize = 256; break;
	case 3: return (USBD_IOERROR);
	}
	err = usb_allocmem(&sc->sc_bus, sc->sc_flsize * sizeof(ehci_link_t),
	    EHCI_FLALIGN_ALIGN, &sc->sc_fldma);
	if (err)
		return (err);
	DPRINTF(("%s: flsize=%d\n", USBDEVNAME(sc->sc_bus.bdev),sc->sc_flsize));
	sc->sc_flist = KERNADDR(&sc->sc_fldma, 0);
	EOWRITE4(sc, EHCI_PERIODICLISTBASE, DMAADDR(&sc->sc_fldma, 0));

	/* Set up the bus struct. */
	sc->sc_bus.methods = &ehci_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct ehci_pipe);

	sc->sc_shutdownhook = shutdownhook_establish(ehci_shutdown, sc);

	sc->sc_eintrs = EHCI_NORMAL_INTRS;

	/*
	 * Allocate the interrupt dummy QHs. These are arranged to give poll
	 * intervals that are powers of 2 times 1ms.
	 */
	for (i = 0; i < EHCI_INTRQHS; i++) {
		sqh = ehci_alloc_sqh(sc);
		if (sqh == NULL) {
			err = USBD_NOMEM;
			goto bad1;
		}
		sc->sc_islots[i].sqh = sqh;
	}
	for (i = 0; i < EHCI_INTRQHS; i++) {
		sqh = sc->sc_islots[i].sqh;
		if (i == 0) {
			/* The last (1ms) QH terminates. */
			sqh->qh.qh_link = EHCI_NULL;
			sqh->next = NULL;
		} else {
			/* Otherwise the next QH has half the poll interval */
			sqh->next = sc->sc_islots[(i + 1) / 2 - 1].sqh;
			sqh->qh.qh_link = htole32(sqh->next->physaddr |
			    EHCI_LINK_QH);
		}
		sqh->qh.qh_endp = htole32(EHCI_QH_SET_EPS(EHCI_QH_SPEED_HIGH));
		sqh->qh.qh_curqtd = EHCI_NULL;
		sqh->next = NULL;
		sqh->qh.qh_qtd.qtd_next = EHCI_NULL;
		sqh->qh.qh_qtd.qtd_altnext = EHCI_NULL;
		sqh->qh.qh_qtd.qtd_status = htole32(EHCI_QTD_HALTED);
		sqh->sqtd = NULL;
	}
	/* Point the frame list at the last level (128ms). */
	for (i = 0; i < sc->sc_flsize; i++) {
		int j;

		j = (i & ~(EHCI_MAX_POLLRATE-1)) |
		    revbits[i & (EHCI_MAX_POLLRATE-1)];
		sc->sc_flist[j] = htole32(EHCI_LINK_QH |
		    sc->sc_islots[EHCI_IQHIDX(EHCI_IPOLLRATES - 1,
		    i)].sqh->physaddr);
	}

	/* Allocate dummy QH that starts the async list. */
	sqh = ehci_alloc_sqh(sc);
	if (sqh == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	/* Fill the QH */
	sqh->qh.qh_endp =
	    htole32(EHCI_QH_SET_EPS(EHCI_QH_SPEED_HIGH) | EHCI_QH_HRECL);
	sqh->qh.qh_link =
	    htole32(sqh->physaddr | EHCI_LINK_QH);
	sqh->qh.qh_curqtd = EHCI_NULL;
	sqh->next = NULL;
	/* Fill the overlay qTD */
	sqh->qh.qh_qtd.qtd_next = EHCI_NULL;
	sqh->qh.qh_qtd.qtd_altnext = EHCI_NULL;
	sqh->qh.qh_qtd.qtd_status = htole32(EHCI_QTD_HALTED);
	sqh->sqtd = NULL;
#ifdef EHCI_DEBUG
	if (ehcidebug) {
		ehci_dump_sqh(sqh);
	}
#endif

	/* Point to async list */
	sc->sc_async_head = sqh;
	EOWRITE4(sc, EHCI_ASYNCLISTADDR, sqh->physaddr | EHCI_LINK_QH);

	usb_callout_init(sc->sc_tmo_intrlist);

	lockinit(&sc->sc_doorbell_lock, PZERO, "ehcidb", 0, 0);

	/* Turn on controller */
	EOWRITE4(sc, EHCI_USBCMD,
		 EHCI_CMD_ITC_2 | /* 2 microframes interrupt delay */
		 (EOREAD4(sc, EHCI_USBCMD) & EHCI_CMD_FLS_M) |
		 EHCI_CMD_ASE |
		 EHCI_CMD_PSE |
		 EHCI_CMD_RS);

	/* Take over port ownership */
	EOWRITE4(sc, EHCI_CONFIGFLAG, EHCI_CONF_CF);

	for (i = 0; i < 100; i++) {
		usb_delay_ms(&sc->sc_bus, 1);
		hcr = EOREAD4(sc, EHCI_USBSTS) & EHCI_STS_HCH;
		if (!hcr)
			break;
	}
	if (hcr) {
		aprint_error("%s: run timeout\n", USBDEVNAME(sc->sc_bus.bdev));
		return (USBD_IOERROR);
	}

	/* Enable interrupts */
	DPRINTFN(1,("ehci_init: enabling\n"));
	EOWRITE4(sc, EHCI_USBINTR, sc->sc_eintrs);

	return (USBD_NORMAL_COMPLETION);

#if 0
 bad2:
	ehci_free_sqh(sc, sc->sc_async_head);
#endif
 bad1:
	usb_freemem(&sc->sc_bus, &sc->sc_fldma);
	return (err);
}

int
ehci_intr(void *v)
{
	ehci_softc_t *sc = v;

	if (sc == NULL || sc->sc_dying)
		return (0);

	/* If we get an interrupt while polling, then just ignore it. */
	if (sc->sc_bus.use_polling) {
		u_int32_t intrs = EHCI_STS_INTRS(EOREAD4(sc, EHCI_USBSTS));

		if (intrs)
			EOWRITE4(sc, EHCI_USBSTS, intrs); /* Acknowledge */
#ifdef DIAGNOSTIC
		DPRINTFN(16, ("ehci_intr: ignored interrupt while polling\n"));
#endif
		return (0);
	}

	return (ehci_intr1(sc));
}

Static int
ehci_intr1(ehci_softc_t *sc)
{
	u_int32_t intrs, eintrs;

	DPRINTFN(20,("ehci_intr1: enter\n"));

	/* In case the interrupt occurs before initialization has completed. */
	if (sc == NULL) {
#ifdef DIAGNOSTIC
		printf("ehci_intr1: sc == NULL\n");
#endif
		return (0);
	}

	intrs = EHCI_STS_INTRS(EOREAD4(sc, EHCI_USBSTS));
	if (!intrs)
		return (0);

	eintrs = intrs & sc->sc_eintrs;
	DPRINTFN(7, ("ehci_intr1: sc=%p intrs=0x%x(0x%x) eintrs=0x%x\n",
		     sc, (u_int)intrs, EOREAD4(sc, EHCI_USBSTS),
		     (u_int)eintrs));
	if (!eintrs)
		return (0);

	EOWRITE4(sc, EHCI_USBSTS, intrs); /* Acknowledge */
	sc->sc_bus.intr_context++;
	sc->sc_bus.no_intrs++;
	if (eintrs & EHCI_STS_IAA) {
		DPRINTF(("ehci_intr1: door bell\n"));
		wakeup(&sc->sc_async_head);
		eintrs &= ~EHCI_STS_IAA;
	}
	if (eintrs & (EHCI_STS_INT | EHCI_STS_ERRINT)) {
		DPRINTFN(5,("ehci_intr1: %s %s\n",
			    eintrs & EHCI_STS_INT ? "INT" : "",
			    eintrs & EHCI_STS_ERRINT ? "ERRINT" : ""));
		usb_schedsoftintr(&sc->sc_bus);
		eintrs &= ~(EHCI_STS_INT | EHCI_STS_ERRINT);
	}
	if (eintrs & EHCI_STS_HSE) {
		printf("%s: unrecoverable error, controller halted\n",
		       USBDEVNAME(sc->sc_bus.bdev));
		/* XXX what else */
	}
	if (eintrs & EHCI_STS_PCD) {
		ehci_pcd(sc, sc->sc_intrxfer);
		eintrs &= ~EHCI_STS_PCD;
	}

	sc->sc_bus.intr_context--;

	if (eintrs != 0) {
		/* Block unprocessed interrupts. */
		sc->sc_eintrs &= ~eintrs;
		EOWRITE4(sc, EHCI_USBINTR, sc->sc_eintrs);
		printf("%s: blocking intrs 0x%x\n",
		       USBDEVNAME(sc->sc_bus.bdev), eintrs);
	}

	return (1);
}


void
ehci_pcd(ehci_softc_t *sc, usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe;
	u_char *p;
	int i, m;

	if (xfer == NULL) {
		/* Just ignore the change. */
		return;
	}

	pipe = xfer->pipe;

	p = KERNADDR(&xfer->dmabuf, 0);
	m = min(sc->sc_noport, xfer->length * 8 - 1);
	memset(p, 0, xfer->length);
	for (i = 1; i <= m; i++) {
		/* Pick out CHANGE bits from the status reg. */
		if (EOREAD4(sc, EHCI_PORTSC(i)) & EHCI_PS_CLEAR)
			p[i/8] |= 1 << (i%8);
	}
	DPRINTF(("ehci_pcd: change=0x%02x\n", *p));
	xfer->actlen = xfer->length;
	xfer->status = USBD_NORMAL_COMPLETION;

	usb_transfer_complete(xfer);
}

void
ehci_softintr(void *v)
{
	ehci_softc_t *sc = v;
	struct ehci_xfer *ex, *nextex;

	DPRINTFN(10,("%s: ehci_softintr (%d)\n", USBDEVNAME(sc->sc_bus.bdev),
		     sc->sc_bus.intr_context));

	sc->sc_bus.intr_context++;

	/*
	 * The only explanation I can think of for why EHCI is as brain dead
	 * as UHCI interrupt-wise is that Intel was involved in both.
	 * An interrupt just tells us that something is done, we have no
	 * clue what, so we need to scan through all active transfers. :-(
	 */
	for (ex = LIST_FIRST(&sc->sc_intrhead); ex; ex = nextex) {
		nextex = LIST_NEXT(ex, inext);
		ehci_check_intr(sc, ex);
	}

	/* Schedule a callout to catch any dropped transactions. */
	if ((sc->sc_flags & EHCIF_DROPPED_INTR_WORKAROUND) &&
	    !LIST_EMPTY(&sc->sc_intrhead))
		usb_callout(sc->sc_tmo_intrlist, hz,
		    ehci_intrlist_timeout, sc);

#ifdef USB_USE_SOFTINTR
	if (sc->sc_softwake) {
		sc->sc_softwake = 0;
		wakeup(&sc->sc_softwake);
	}
#endif /* USB_USE_SOFTINTR */

	sc->sc_bus.intr_context--;
}

/* Check for an interrupt. */
void
ehci_check_intr(ehci_softc_t *sc, struct ehci_xfer *ex)
{
	ehci_soft_qtd_t *sqtd, *lsqtd;
	u_int32_t status;

	DPRINTFN(/*15*/2, ("ehci_check_intr: ex=%p\n", ex));

	if (ex->sqtdstart == NULL) {
		printf("ehci_check_intr: sqtdstart=NULL\n");
		return;
	}
	lsqtd = ex->sqtdend;
#ifdef DIAGNOSTIC
	if (lsqtd == NULL) {
		printf("ehci_check_intr: lsqtd==0\n");
		return;
	}
#endif
	/*
	 * If the last TD is still active we need to check whether there
	 * is a an error somewhere in the middle, or whether there was a
	 * short packet (SPD and not ACTIVE).
	 */
	if (le32toh(lsqtd->qtd.qtd_status) & EHCI_QTD_ACTIVE) {
		DPRINTFN(12, ("ehci_check_intr: active ex=%p\n", ex));
		for (sqtd = ex->sqtdstart; sqtd != lsqtd; sqtd=sqtd->nextqtd) {
			status = le32toh(sqtd->qtd.qtd_status);
			/* If there's an active QTD the xfer isn't done. */
			if (status & EHCI_QTD_ACTIVE)
				break;
			/* Any kind of error makes the xfer done. */
			if (status & EHCI_QTD_HALTED)
				goto done;
			/* We want short packets, and it is short: it's done */
			if (EHCI_QTD_GET_BYTES(status) != 0)
				goto done;
		}
		DPRINTFN(12, ("ehci_check_intr: ex=%p std=%p still active\n",
			      ex, ex->sqtdstart));
		return;
	}
 done:
	DPRINTFN(12, ("ehci_check_intr: ex=%p done\n", ex));
	usb_uncallout(ex->xfer.timeout_handle, ehci_timeout, ex);
	ehci_idone(ex);
}

void
ehci_idone(struct ehci_xfer *ex)
{
	usbd_xfer_handle xfer = &ex->xfer;
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	ehci_soft_qtd_t *sqtd, *lsqtd;
	u_int32_t status = 0, nstatus = 0;
	int actlen;

	DPRINTFN(/*12*/2, ("ehci_idone: ex=%p\n", ex));
#ifdef DIAGNOSTIC
	{
		int s = splhigh();
		if (ex->isdone) {
			splx(s);
#ifdef EHCI_DEBUG
			printf("ehci_idone: ex is done!\n   ");
			ehci_dump_exfer(ex);
#else
			printf("ehci_idone: ex=%p is done!\n", ex);
#endif
			return;
		}
		ex->isdone = 1;
		splx(s);
	}
#endif

	if (xfer->status == USBD_CANCELLED ||
	    xfer->status == USBD_TIMEOUT) {
		DPRINTF(("ehci_idone: aborted xfer=%p\n", xfer));
		return;
	}

#ifdef EHCI_DEBUG
	DPRINTFN(/*10*/2, ("ehci_idone: xfer=%p, pipe=%p ready\n", xfer, epipe));
	if (ehcidebug > 10)
		ehci_dump_sqtds(ex->sqtdstart);
#endif

	/* The transfer is done, compute actual length and status. */
	lsqtd = ex->sqtdend;
	actlen = 0;
	for (sqtd = ex->sqtdstart; sqtd != lsqtd->nextqtd; sqtd=sqtd->nextqtd) {
		nstatus = le32toh(sqtd->qtd.qtd_status);
		if (nstatus & EHCI_QTD_ACTIVE)
			break;

		status = nstatus;
		if (EHCI_QTD_GET_PID(status) !=	EHCI_QTD_PID_SETUP)
			actlen += sqtd->len - EHCI_QTD_GET_BYTES(status);
	}

	/*
	 * If there are left over TDs we need to update the toggle.
	 * The default pipe doesn't need it since control transfers
	 * start the toggle at 0 every time.
	 * For a short transfer we need to update the toggle for the missing
	 * packets within the qTD.
	 */
	if ((sqtd != lsqtd->nextqtd || EHCI_QTD_GET_BYTES(status)) &&
	    xfer->pipe->device->default_pipe != xfer->pipe) {
		DPRINTFN(2, ("ehci_idone: need toggle update "
			     "status=%08x nstatus=%08x\n", status, nstatus));
#if 0
		ehci_dump_sqh(epipe->sqh);
		ehci_dump_sqtds(ex->sqtdstart);
#endif
		epipe->nexttoggle = EHCI_QTD_GET_TOGGLE(nstatus);
	}

	DPRINTFN(/*10*/2, ("ehci_idone: len=%d, actlen=%d, status=0x%x\n",
			   xfer->length, actlen, status));
	xfer->actlen = actlen;
	if (status & EHCI_QTD_HALTED) {
#ifdef EHCI_DEBUG
		char sbuf[128];

		bitmask_snprintf((u_int32_t)status,
				 "\20\7HALTED\6BUFERR\5BABBLE\4XACTERR"
				 "\3MISSED\1PINGSTATE", sbuf, sizeof(sbuf));

		DPRINTFN(2, ("ehci_idone: error, addr=%d, endpt=0x%02x, "
			  "status 0x%s\n",
			  xfer->pipe->device->address,
			  xfer->pipe->endpoint->edesc->bEndpointAddress,
			  sbuf));
		if (ehcidebug > 2) {
			ehci_dump_sqh(epipe->sqh);
			ehci_dump_sqtds(ex->sqtdstart);
		}
#endif
		/* low&full speed has an extra error flag */
		if (EHCI_QH_GET_EPS(epipe->sqh->qh.qh_endp) !=
		    EHCI_QH_SPEED_HIGH)
			status &= EHCI_QTD_STATERRS | EHCI_QTD_PINGSTATE;
		else
			status &= EHCI_QTD_STATERRS;
		if (status == 0) /* no other errors means a stall */
			xfer->status = USBD_STALLED;
		else
			xfer->status = USBD_IOERROR; /* more info XXX */
		/* XXX need to reset TT on missed microframe */
		if (status & EHCI_QTD_MISSEDMICRO) {
			ehci_softc_t *sc = (ehci_softc_t *)
			    xfer->pipe->device->bus;

			printf("%s: missed microframe, TT reset not "
			    "implemented, hub might be inoperational\n",
			    USBDEVNAME(sc->sc_bus.bdev));
		}
	} else {
		xfer->status = USBD_NORMAL_COMPLETION;
	}

	usb_transfer_complete(xfer);
	DPRINTFN(/*12*/2, ("ehci_idone: ex=%p done\n", ex));
}

/*
 * Wait here until controller claims to have an interrupt.
 * Then call ehci_intr and return.  Use timeout to avoid waiting
 * too long.
 */
void
ehci_waitintr(ehci_softc_t *sc, usbd_xfer_handle xfer)
{
	int timo;
	u_int32_t intrs;

	xfer->status = USBD_IN_PROGRESS;
	for (timo = xfer->timeout; timo >= 0; timo--) {
		usb_delay_ms(&sc->sc_bus, 1);
		if (sc->sc_dying)
			break;
		intrs = EHCI_STS_INTRS(EOREAD4(sc, EHCI_USBSTS)) &
			sc->sc_eintrs;
		DPRINTFN(15,("ehci_waitintr: 0x%04x\n", intrs));
#ifdef EHCI_DEBUG
		if (ehcidebug > 15)
			ehci_dump_regs(sc);
#endif
		if (intrs) {
			ehci_intr1(sc);
			if (xfer->status != USBD_IN_PROGRESS)
				return;
		}
	}

	/* Timeout */
	DPRINTF(("ehci_waitintr: timeout\n"));
	xfer->status = USBD_TIMEOUT;
	usb_transfer_complete(xfer);
	/* XXX should free TD */
}

void
ehci_poll(struct usbd_bus *bus)
{
	ehci_softc_t *sc = (ehci_softc_t *)bus;
#ifdef EHCI_DEBUG
	static int last;
	int new;
	new = EHCI_STS_INTRS(EOREAD4(sc, EHCI_USBSTS));
	if (new != last) {
		DPRINTFN(10,("ehci_poll: intrs=0x%04x\n", new));
		last = new;
	}
#endif

	if (EOREAD4(sc, EHCI_USBSTS) & sc->sc_eintrs)
		ehci_intr1(sc);
}

int
ehci_detach(struct ehci_softc *sc, int flags)
{
	int rv = 0;

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);

	if (rv != 0)
		return (rv);

	usb_uncallout(sc->sc_tmo_intrlist, ehci_intrlist_timeout, sc);

	if (sc->sc_shutdownhook != NULL)
		shutdownhook_disestablish(sc->sc_shutdownhook);

	usb_delay_ms(&sc->sc_bus, 300); /* XXX let stray task complete */

	/* XXX free other data structures XXX */

	return (rv);
}


int
ehci_activate(device_ptr_t self, enum devact act)
{
	struct ehci_softc *sc = (struct ehci_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		if (sc->sc_child != NULL)
			rv = config_deactivate(sc->sc_child);
		break;
	}
	return (rv);
}

/*
 * Handle suspend/resume.
 *
 * We need to switch to polling mode here, because this routine is
 * called from an interrupt context.  This is all right since we
 * are almost suspended anyway.
 *
 * Note that this power handler isn't to be registered directly; the
 * bus glue needs to call out to it.
 */
void
ehci_suspend(device_t dv)
{
	ehci_softc_t *sc = (ehci_softc_t *)dv;
	int i, s;
	uint32_t cmd, hcr;

	s = splhardusb();

	sc->sc_bus.use_polling++;

	for (i = 1; i <= sc->sc_noport; i++) {
		cmd = EOREAD4(sc, EHCI_PORTSC(i));
		if ((cmd & EHCI_PS_PO) == 0 && (cmd & EHCI_PS_PE) == EHCI_PS_PE)
			EOWRITE4(sc, EHCI_PORTSC(i), cmd | EHCI_PS_SUSP);
	}

	sc->sc_cmd = EOREAD4(sc, EHCI_USBCMD);

	cmd = sc->sc_cmd & ~(EHCI_CMD_ASE | EHCI_CMD_PSE);
	EOWRITE4(sc, EHCI_USBCMD, cmd);

	for (i = 0; i < 100; i++) {
		hcr = EOREAD4(sc, EHCI_USBSTS) & (EHCI_STS_ASS | EHCI_STS_PSS);
		if (hcr == 0)
			break;

		usb_delay_ms(&sc->sc_bus, 1);
	}
	if (hcr != 0)
		printf("%s: reset timeout\n", USBDEVNAME(sc->sc_bus.bdev));

	cmd &= ~EHCI_CMD_RS;
	EOWRITE4(sc, EHCI_USBCMD, cmd);

	for (i = 0; i < 100; i++) {
		hcr = EOREAD4(sc, EHCI_USBSTS) & EHCI_STS_HCH;
		if (hcr == EHCI_STS_HCH)
			break;

		usb_delay_ms(&sc->sc_bus, 1);
	}
	if (hcr != EHCI_STS_HCH)
		printf("%s: config timeout\n", USBDEVNAME(sc->sc_bus.bdev));

	sc->sc_bus.use_polling--;
	splx(s);
}

void
ehci_resume(device_t dv)
{
	ehci_softc_t *sc = (ehci_softc_t *)dv;
	int i, s;
	uint32_t cmd, hcr;

	s = splhardusb();

	sc->sc_bus.use_polling++;

	/* restore things in case the bios sucks */
	EOWRITE4(sc, EHCI_CTRLDSSEGMENT, 0);
	EOWRITE4(sc, EHCI_PERIODICLISTBASE, DMAADDR(&sc->sc_fldma, 0));
	EOWRITE4(sc, EHCI_ASYNCLISTADDR,
	    sc->sc_async_head->physaddr | EHCI_LINK_QH);
	EOWRITE4(sc, EHCI_USBINTR, sc->sc_eintrs);

	EOWRITE4(sc, EHCI_USBCMD, sc->sc_cmd);

	hcr = 0;
	for (i = 1; i <= sc->sc_noport; i++) {
		cmd = EOREAD4(sc, EHCI_PORTSC(i));
		if ((cmd & EHCI_PS_PO) == 0 &&
		    (cmd & EHCI_PS_SUSP) == EHCI_PS_SUSP) {
			EOWRITE4(sc, EHCI_PORTSC(i), cmd | EHCI_PS_FPR);
			hcr = 1;
		}
	}

	if (hcr) {
		usb_delay_ms(&sc->sc_bus, USB_RESUME_WAIT);

		for (i = 1; i <= sc->sc_noport; i++) {
			cmd = EOREAD4(sc, EHCI_PORTSC(i));
			if ((cmd & EHCI_PS_PO) == 0 &&
			    (cmd & EHCI_PS_SUSP) == EHCI_PS_SUSP)
				EOWRITE4(sc, EHCI_PORTSC(i),
				    cmd & ~EHCI_PS_FPR);
		}
	}

	EOWRITE4(sc, EHCI_USBCMD, sc->sc_cmd);

	for (i = 0; i < 100; i++) {
		hcr = EOREAD4(sc, EHCI_USBSTS) & EHCI_STS_HCH;
		if (hcr != EHCI_STS_HCH)
			break;

		usb_delay_ms(&sc->sc_bus, 1);
	}
	if (hcr == EHCI_STS_HCH)
		printf("%s: config timeout\n", USBDEVNAME(sc->sc_bus.bdev));

	usb_delay_ms(&sc->sc_bus, USB_RESUME_WAIT);

	sc->sc_bus.use_polling--;

	splx(s);
}

/*
 * Shut down the controller when the system is going down.
 */
void
ehci_shutdown(void *v)
{
	ehci_softc_t *sc = v;

	DPRINTF(("ehci_shutdown: stopping the HC\n"));
	EOWRITE4(sc, EHCI_USBCMD, 0);	/* Halt controller */
	EOWRITE4(sc, EHCI_USBCMD, EHCI_CMD_HCRESET);
}

usbd_status
ehci_allocm(struct usbd_bus *bus, usb_dma_t *dma, u_int32_t size)
{
	struct ehci_softc *sc = (struct ehci_softc *)bus;
	usbd_status err;

	err = usb_allocmem(&sc->sc_bus, size, 0, dma);
	if (err == USBD_NOMEM)
		err = usb_reserve_allocm(&sc->sc_dma_reserve, dma, size);
#ifdef EHCI_DEBUG
	if (err)
		printf("ehci_allocm: usb_allocmem()=%d\n", err);
#endif
	return (err);
}

void
ehci_freem(struct usbd_bus *bus, usb_dma_t *dma)
{
	struct ehci_softc *sc = (struct ehci_softc *)bus;

	if (dma->block->flags & USB_DMA_RESERVE) {
		usb_reserve_freem(&((struct ehci_softc *)bus)->sc_dma_reserve,
		    dma);
		return;
	}
	usb_freemem(&sc->sc_bus, dma);
}

usbd_xfer_handle
ehci_allocx(struct usbd_bus *bus)
{
	struct ehci_softc *sc = (struct ehci_softc *)bus;
	usbd_xfer_handle xfer;

	xfer = SIMPLEQ_FIRST(&sc->sc_free_xfers);
	if (xfer != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_free_xfers, next);
#ifdef DIAGNOSTIC
		if (xfer->busy_free != XFER_FREE) {
			printf("ehci_allocx: xfer=%p not free, 0x%08x\n", xfer,
			       xfer->busy_free);
		}
#endif
	} else {
		xfer = malloc(sizeof(struct ehci_xfer), M_USB, M_NOWAIT);
	}
	if (xfer != NULL) {
		memset(xfer, 0, sizeof(struct ehci_xfer));
#ifdef DIAGNOSTIC
		EXFER(xfer)->isdone = 1;
		xfer->busy_free = XFER_BUSY;
#endif
	}
	return (xfer);
}

void
ehci_freex(struct usbd_bus *bus, usbd_xfer_handle xfer)
{
	struct ehci_softc *sc = (struct ehci_softc *)bus;

#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_BUSY) {
		printf("ehci_freex: xfer=%p not busy, 0x%08x\n", xfer,
		       xfer->busy_free);
	}
	xfer->busy_free = XFER_FREE;
	if (!EXFER(xfer)->isdone) {
		printf("ehci_freex: !isdone\n");
	}
#endif
	SIMPLEQ_INSERT_HEAD(&sc->sc_free_xfers, xfer, next);
}

Static void
ehci_device_clear_toggle(usbd_pipe_handle pipe)
{
	struct ehci_pipe *epipe = (struct ehci_pipe *)pipe;

	DPRINTF(("ehci_device_clear_toggle: epipe=%p status=0x%x\n",
		 epipe, epipe->sqh->qh.qh_qtd.qtd_status));
#ifdef USB_DEBUG
	if (ehcidebug)
		usbd_dump_pipe(pipe);
#endif
	epipe->nexttoggle = 0;
}

Static void
ehci_noop(usbd_pipe_handle pipe)
{
}

#ifdef EHCI_DEBUG
void
ehci_dump_regs(ehci_softc_t *sc)
{
	int i;
	printf("cmd=0x%08x, sts=0x%08x, ien=0x%08x\n",
	       EOREAD4(sc, EHCI_USBCMD),
	       EOREAD4(sc, EHCI_USBSTS),
	       EOREAD4(sc, EHCI_USBINTR));
	printf("frindex=0x%08x ctrdsegm=0x%08x periodic=0x%08x async=0x%08x\n",
	       EOREAD4(sc, EHCI_FRINDEX),
	       EOREAD4(sc, EHCI_CTRLDSSEGMENT),
	       EOREAD4(sc, EHCI_PERIODICLISTBASE),
	       EOREAD4(sc, EHCI_ASYNCLISTADDR));
	for (i = 1; i <= sc->sc_noport; i++)
		printf("port %d status=0x%08x\n", i,
		       EOREAD4(sc, EHCI_PORTSC(i)));
}

/*
 * Unused function - this is meant to be called from a kernel
 * debugger.
 */
void
ehci_dump()
{
	ehci_dump_regs(theehci);
}

void
ehci_dump_link(ehci_link_t link, int type)
{
	link = le32toh(link);
	printf("0x%08x", link);
	if (link & EHCI_LINK_TERMINATE)
		printf("<T>");
	else {
		printf("<");
		if (type) {
			switch (EHCI_LINK_TYPE(link)) {
			case EHCI_LINK_ITD: printf("ITD"); break;
			case EHCI_LINK_QH: printf("QH"); break;
			case EHCI_LINK_SITD: printf("SITD"); break;
			case EHCI_LINK_FSTN: printf("FSTN"); break;
			}
		}
		printf(">");
	}
}

void
ehci_dump_sqtds(ehci_soft_qtd_t *sqtd)
{
	int i;
	u_int32_t stop;

	stop = 0;
	for (i = 0; sqtd && i < 20 && !stop; sqtd = sqtd->nextqtd, i++) {
		ehci_dump_sqtd(sqtd);
		stop = sqtd->qtd.qtd_next & htole32(EHCI_LINK_TERMINATE);
	}
	if (sqtd)
		printf("dump aborted, too many TDs\n");
}

void
ehci_dump_sqtd(ehci_soft_qtd_t *sqtd)
{
	printf("QTD(%p) at 0x%08x:\n", sqtd, sqtd->physaddr);
	ehci_dump_qtd(&sqtd->qtd);
}

void
ehci_dump_qtd(ehci_qtd_t *qtd)
{
	u_int32_t s;
	char sbuf[128];

	printf("  next="); ehci_dump_link(qtd->qtd_next, 0);
	printf(" altnext="); ehci_dump_link(qtd->qtd_altnext, 0);
	printf("\n");
	s = le32toh(qtd->qtd_status);
	bitmask_snprintf(EHCI_QTD_GET_STATUS(s),
			 "\20\10ACTIVE\7HALTED\6BUFERR\5BABBLE\4XACTERR"
			 "\3MISSED\2SPLIT\1PING", sbuf, sizeof(sbuf));
	printf("  status=0x%08x: toggle=%d bytes=0x%x ioc=%d c_page=0x%x\n",
	       s, EHCI_QTD_GET_TOGGLE(s), EHCI_QTD_GET_BYTES(s),
	       EHCI_QTD_GET_IOC(s), EHCI_QTD_GET_C_PAGE(s));
	printf("    cerr=%d pid=%d stat=0x%s\n", EHCI_QTD_GET_CERR(s),
	       EHCI_QTD_GET_PID(s), sbuf);
	for (s = 0; s < 5; s++)
		printf("  buffer[%d]=0x%08x\n", s, le32toh(qtd->qtd_buffer[s]));
}

void
ehci_dump_sqh(ehci_soft_qh_t *sqh)
{
	ehci_qh_t *qh = &sqh->qh;
	u_int32_t endp, endphub;

	printf("QH(%p) at 0x%08x:\n", sqh, sqh->physaddr);
	printf("  link="); ehci_dump_link(qh->qh_link, 1); printf("\n");
	endp = le32toh(qh->qh_endp);
	printf("  endp=0x%08x\n", endp);
	printf("    addr=0x%02x inact=%d endpt=%d eps=%d dtc=%d hrecl=%d\n",
	       EHCI_QH_GET_ADDR(endp), EHCI_QH_GET_INACT(endp),
	       EHCI_QH_GET_ENDPT(endp),  EHCI_QH_GET_EPS(endp),
	       EHCI_QH_GET_DTC(endp), EHCI_QH_GET_HRECL(endp));
	printf("    mpl=0x%x ctl=%d nrl=%d\n",
	       EHCI_QH_GET_MPL(endp), EHCI_QH_GET_CTL(endp),
	       EHCI_QH_GET_NRL(endp));
	endphub = le32toh(qh->qh_endphub);
	printf("  endphub=0x%08x\n", endphub);
	printf("    smask=0x%02x cmask=0x%02x huba=0x%02x port=%d mult=%d\n",
	       EHCI_QH_GET_SMASK(endphub), EHCI_QH_GET_CMASK(endphub),
	       EHCI_QH_GET_HUBA(endphub), EHCI_QH_GET_PORT(endphub),
	       EHCI_QH_GET_MULT(endphub));
	printf("  curqtd="); ehci_dump_link(qh->qh_curqtd, 0); printf("\n");
	printf("Overlay qTD:\n");
	ehci_dump_qtd(&qh->qh_qtd);
}

#ifdef DIAGNOSTIC
Static void
ehci_dump_exfer(struct ehci_xfer *ex)
{
	printf("ehci_dump_exfer: ex=%p\n", ex);
}
#endif
#endif

usbd_status
ehci_open(usbd_pipe_handle pipe)
{
	usbd_device_handle dev = pipe->device;
	ehci_softc_t *sc = (ehci_softc_t *)dev->bus;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	u_int8_t addr = dev->address;
	u_int8_t xfertype = ed->bmAttributes & UE_XFERTYPE;
	struct ehci_pipe *epipe = (struct ehci_pipe *)pipe;
	ehci_soft_qh_t *sqh;
	usbd_status err;
	int s;
	int ival, speed, naks;
	int hshubaddr, hshubport;

	DPRINTFN(1, ("ehci_open: pipe=%p, addr=%d, endpt=%d (%d)\n",
		     pipe, addr, ed->bEndpointAddress, sc->sc_addr));

	if (dev->myhsport) {
		hshubaddr = dev->myhsport->parent->address;
		hshubport = dev->myhsport->portno;
	} else {
		hshubaddr = 0;
		hshubport = 0;
	}

	if (sc->sc_dying)
		return (USBD_IOERROR);

	epipe->nexttoggle = 0;

	if (addr == sc->sc_addr) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &ehci_root_ctrl_methods;
			break;
		case UE_DIR_IN | EHCI_INTR_ENDPT:
			pipe->methods = &ehci_root_intr_methods;
			break;
		default:
			return (USBD_INVAL);
		}
		return (USBD_NORMAL_COMPLETION);
	}

	/* XXX All this stuff is only valid for async. */
	switch (dev->speed) {
	case USB_SPEED_LOW:  speed = EHCI_QH_SPEED_LOW;  break;
	case USB_SPEED_FULL: speed = EHCI_QH_SPEED_FULL; break;
	case USB_SPEED_HIGH: speed = EHCI_QH_SPEED_HIGH; break;
	default: panic("ehci_open: bad device speed %d", dev->speed);
	}
	if (speed != EHCI_QH_SPEED_HIGH && xfertype == UE_ISOCHRONOUS) {
		printf("%s: *** WARNING: opening low/full speed isoc device, "
		       "this does not work yet.\n",
		       USBDEVNAME(sc->sc_bus.bdev));
		DPRINTFN(1,("ehci_open: hshubaddr=%d hshubport=%d\n",
			    hshubaddr, hshubport));
		return USBD_INVAL;
	}

	naks = 8;		/* XXX */
	sqh = ehci_alloc_sqh(sc);
	if (sqh == NULL)
		return (USBD_NOMEM);
	/* qh_link filled when the QH is added */
	sqh->qh.qh_endp = htole32(
		EHCI_QH_SET_ADDR(addr) |
		EHCI_QH_SET_ENDPT(UE_GET_ADDR(ed->bEndpointAddress)) |
		EHCI_QH_SET_EPS(speed) |
		EHCI_QH_DTC |
		EHCI_QH_SET_MPL(UGETW(ed->wMaxPacketSize)) |
		(speed != EHCI_QH_SPEED_HIGH && xfertype == UE_CONTROL ?
		 EHCI_QH_CTL : 0) |
		EHCI_QH_SET_NRL(naks)
		);
	sqh->qh.qh_endphub = htole32(
		EHCI_QH_SET_MULT(1) |
		EHCI_QH_SET_HUBA(hshubaddr) |
		EHCI_QH_SET_PORT(hshubport) |
		EHCI_QH_SET_CMASK(0x08) | /* XXX */
		EHCI_QH_SET_SMASK(xfertype == UE_INTERRUPT ? 0x02 : 0)
		);
	sqh->qh.qh_curqtd = EHCI_NULL;
	/* Fill the overlay qTD */
	sqh->qh.qh_qtd.qtd_next = EHCI_NULL;
	sqh->qh.qh_qtd.qtd_altnext = EHCI_NULL;
	sqh->qh.qh_qtd.qtd_status = htole32(0);

	epipe->sqh = sqh;

	switch (xfertype) {
	case UE_CONTROL:
		err = usb_allocmem(&sc->sc_bus, sizeof(usb_device_request_t),
				   0, &epipe->u.ctl.reqdma);
#ifdef EHCI_DEBUG
		if (err)
			printf("ehci_open: usb_allocmem()=%d\n", err);
#endif
		if (err)
			goto bad;
		pipe->methods = &ehci_device_ctrl_methods;
		s = splusb();
		ehci_add_qh(sqh, sc->sc_async_head);
		splx(s);
		break;
	case UE_BULK:
		pipe->methods = &ehci_device_bulk_methods;
		s = splusb();
		ehci_add_qh(sqh, sc->sc_async_head);
		splx(s);
		break;
	case UE_INTERRUPT:
		pipe->methods = &ehci_device_intr_methods;
		ival = pipe->interval;
		if (ival == USBD_DEFAULT_INTERVAL) {
			if (speed == EHCI_QH_SPEED_HIGH) {
				if (ed->bInterval > 16) {
					/*
					 * illegal with high-speed, but there
					 * were documentation bugs in the spec,
					 * so be generous
					 */
					ival = 256;
				} else
					ival = (1 << (ed->bInterval - 1)) / 8;
			} else
				ival = ed->bInterval;
		}
		err = ehci_device_setintr(sc, sqh, ival);
		if (err)
			goto bad;
		break;
	case UE_ISOCHRONOUS:
		pipe->methods = &ehci_device_isoc_methods;
		/* FALLTHROUGH */
	default:
		err = USBD_INVAL;
		goto bad;
	}
	return (USBD_NORMAL_COMPLETION);

 bad:
	ehci_free_sqh(sc, sqh);
	return (err);
}

/*
 * Add an ED to the schedule.  Called at splusb().
 */
void
ehci_add_qh(ehci_soft_qh_t *sqh, ehci_soft_qh_t *head)
{
	SPLUSBCHECK;

	sqh->next = head->next;
	sqh->qh.qh_link = head->qh.qh_link;
	head->next = sqh;
	head->qh.qh_link = htole32(sqh->physaddr | EHCI_LINK_QH);

#ifdef EHCI_DEBUG
	if (ehcidebug > 5) {
		printf("ehci_add_qh:\n");
		ehci_dump_sqh(sqh);
	}
#endif
}

/*
 * Remove an ED from the schedule.  Called at splusb().
 */
void
ehci_rem_qh(ehci_softc_t *sc, ehci_soft_qh_t *sqh, ehci_soft_qh_t *head)
{
	ehci_soft_qh_t *p;

	SPLUSBCHECK;
	/* XXX */
	for (p = head; p != NULL && p->next != sqh; p = p->next)
		;
	if (p == NULL)
		panic("ehci_rem_qh: ED not found");
	p->next = sqh->next;
	p->qh.qh_link = sqh->qh.qh_link;

	ehci_sync_hc(sc);
}

void
ehci_set_qh_qtd(ehci_soft_qh_t *sqh, ehci_soft_qtd_t *sqtd)
{
	int i;
	u_int32_t status;

	/* Save toggle bit and ping status. */
	status = sqh->qh.qh_qtd.qtd_status &
	    htole32(EHCI_QTD_TOGGLE_MASK |
		    EHCI_QTD_SET_STATUS(EHCI_QTD_PINGSTATE));
	/* Set HALTED to make hw leave it alone. */
	sqh->qh.qh_qtd.qtd_status =
	    htole32(EHCI_QTD_SET_STATUS(EHCI_QTD_HALTED));
	sqh->qh.qh_curqtd = 0;
	sqh->qh.qh_qtd.qtd_next = htole32(sqtd->physaddr);
	sqh->qh.qh_qtd.qtd_altnext = 0;
	for (i = 0; i < EHCI_QTD_NBUFFERS; i++)
		sqh->qh.qh_qtd.qtd_buffer[i] = 0;
	sqh->sqtd = sqtd;
	/* Set !HALTED && !ACTIVE to start execution, preserve some fields */
	sqh->qh.qh_qtd.qtd_status = status;
}

/*
 * Ensure that the HC has released all references to the QH.  We do this
 * by asking for a Async Advance Doorbell interrupt and then we wait for
 * the interrupt.
 * To make this easier we first obtain exclusive use of the doorbell.
 */
void
ehci_sync_hc(ehci_softc_t *sc)
{
	int s, error;

	if (sc->sc_dying) {
		DPRINTFN(2,("ehci_sync_hc: dying\n"));
		return;
	}
	DPRINTFN(2,("ehci_sync_hc: enter\n"));
	usb_lockmgr(&sc->sc_doorbell_lock, LK_EXCLUSIVE, NULL); /* get doorbell */
	s = splhardusb();
	/* ask for doorbell */
	EOWRITE4(sc, EHCI_USBCMD, EOREAD4(sc, EHCI_USBCMD) | EHCI_CMD_IAAD);
	DPRINTFN(1,("ehci_sync_hc: cmd=0x%08x sts=0x%08x\n",
		    EOREAD4(sc, EHCI_USBCMD), EOREAD4(sc, EHCI_USBSTS)));
	error = tsleep(&sc->sc_async_head, PZERO, "ehcidi", hz); /* bell wait */
	DPRINTFN(1,("ehci_sync_hc: cmd=0x%08x sts=0x%08x\n",
		    EOREAD4(sc, EHCI_USBCMD), EOREAD4(sc, EHCI_USBSTS)));
	splx(s);
	usb_lockmgr(&sc->sc_doorbell_lock, LK_RELEASE, NULL); /* release doorbell */
#ifdef DIAGNOSTIC
	if (error)
		printf("ehci_sync_hc: tsleep() = %d\n", error);
#endif
	DPRINTFN(2,("ehci_sync_hc: exit\n"));
}

/***********/

/*
 * Data structures and routines to emulate the root hub.
 */
Static usb_device_descriptor_t ehci_devd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE,		/* type */
	{0x00, 0x02},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	UDPROTO_HSHUBSTT,	/* protocol */
	64,			/* max packet */
	{0},{0},{0x00,0x01},	/* device id */
	1,2,0,			/* string indicies */
	1			/* # of configurations */
};

Static const usb_device_qualifier_t ehci_odevd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE_QUALIFIER,	/* type */
	{0x00, 0x02},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	UDPROTO_FSHUB,		/* protocol */
	64,			/* max packet */
	1,			/* # of configurations */
	0
};

Static const usb_config_descriptor_t ehci_confd = {
	USB_CONFIG_DESCRIPTOR_SIZE,
	UDESC_CONFIG,
	{USB_CONFIG_DESCRIPTOR_SIZE +
	 USB_INTERFACE_DESCRIPTOR_SIZE +
	 USB_ENDPOINT_DESCRIPTOR_SIZE},
	1,
	1,
	0,
	UC_ATTR_MBO | UC_SELF_POWERED,
	0			/* max power */
};

Static const usb_interface_descriptor_t ehci_ifcd = {
	USB_INTERFACE_DESCRIPTOR_SIZE,
	UDESC_INTERFACE,
	0,
	0,
	1,
	UICLASS_HUB,
	UISUBCLASS_HUB,
	UIPROTO_HSHUBSTT,
	0
};

Static const usb_endpoint_descriptor_t ehci_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_DIR_IN | EHCI_INTR_ENDPT,
	UE_INTERRUPT,
	{8, 0},			/* max packet */
	12
};

Static const usb_hub_descriptor_t ehci_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_HUB,
	0,
	{0,0},
	0,
	0,
	{""},
	{""},
};

Static int
ehci_str(usb_string_descriptor_t *p, int l, const char *s)
{
	int i;

	if (l == 0)
		return (0);
	p->bLength = 2 * strlen(s) + 2;
	if (l == 1)
		return (1);
	p->bDescriptorType = UDESC_STRING;
	l -= 2;
	for (i = 0; s[i] && l > 1; i++, l -= 2)
		USETW2(p->bString[i], 0, s[i]);
	return (2*i+2);
}

/*
 * Simulate a hardware hub by handling all the necessary requests.
 */
Static usbd_status
ehci_root_ctrl_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ehci_root_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

Static usbd_status
ehci_root_ctrl_start(usbd_xfer_handle xfer)
{
	ehci_softc_t *sc = (ehci_softc_t *)xfer->pipe->device->bus;
	usb_device_request_t *req;
	void *buf = NULL;
	int port, i;
	int s, len, value, index, l, totlen = 0;
	usb_port_status_t ps;
	usb_hub_descriptor_t hubd;
	usbd_status err;
	u_int32_t v;

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST))
		/* XXX panic */
		return (USBD_INVAL);
#endif
	req = &xfer->request;

	DPRINTFN(4,("ehci_root_ctrl_start: type=0x%02x request=%02x\n",
		    req->bmRequestType, req->bRequest));

	len = UGETW(req->wLength);
	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	if (len != 0)
		buf = KERNADDR(&xfer->dmabuf, 0);

#define C(x,y) ((x) | ((y) << 8))
	switch(C(req->bRequest, req->bmRequestType)) {
	case C(UR_CLEAR_FEATURE, UT_WRITE_DEVICE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
		/*
		 * DEVICE_REMOTE_WAKEUP and ENDPOINT_HALT are no-ops
		 * for the integrated root hub.
		 */
		break;
	case C(UR_GET_CONFIG, UT_READ_DEVICE):
		if (len > 0) {
			*(u_int8_t *)buf = sc->sc_conf;
			totlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		DPRINTFN(8,("ehci_root_ctrl_start: wValue=0x%04x\n", value));
		if (len == 0)
			break;
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			USETW(ehci_devd.idVendor, sc->sc_id_vendor);
			memcpy(buf, &ehci_devd, l);
			break;
		/*
		 * We can't really operate at another speed, but the spec says
		 * we need this descriptor.
		 */
		case UDESC_DEVICE_QUALIFIER:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			memcpy(buf, &ehci_odevd, l);
			break;
		/*
		 * We can't really operate at another speed, but the spec says
		 * we need this descriptor.
		 */
		case UDESC_OTHER_SPEED_CONFIGURATION:
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &ehci_confd, l);
			((usb_config_descriptor_t *)buf)->bDescriptorType =
				value >> 8;
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &ehci_ifcd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &ehci_endpd, l);
			break;
		case UDESC_STRING:
			*(u_int8_t *)buf = 0;
			totlen = 1;
			switch (value & 0xff) {
			case 0: /* Language table */
				if (len > 0)
					*(u_int8_t *)buf = 4;
				if (len >=  4) {
		USETW(((usb_string_descriptor_t *)buf)->bString[0], 0x0409);
					totlen = 4;
				}
				break;
			case 1: /* Vendor */
				totlen = ehci_str(buf, len, sc->sc_vendor);
				break;
			case 2: /* Product */
				totlen = ehci_str(buf, len, "EHCI root hub");
				break;
			}
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_GET_INTERFACE, UT_READ_INTERFACE):
		if (len > 0) {
			*(u_int8_t *)buf = 0;
			totlen = 1;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_DEVICE):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus,UDS_SELF_POWERED);
			totlen = 2;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_INTERFACE):
	case C(UR_GET_STATUS, UT_READ_ENDPOINT):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus, 0);
			totlen = 2;
		}
		break;
	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		if (value >= USB_MAX_DEVICES) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_addr = value;
		break;
	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (value != 0 && value != 1) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_conf = value;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_DEVICE):
	case C(UR_SET_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		break;
	case C(UR_SYNCH_FRAME, UT_WRITE_ENDPOINT):
		break;
	/* Hub requests */
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_OTHER):
		DPRINTFN(4, ("ehci_root_ctrl_start: UR_CLEAR_PORT_FEATURE "
			     "port=%d feature=%d\n",
			     index, value));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = EHCI_PORTSC(index);
		v = EOREAD4(sc, port);
		DPRINTFN(4, ("ehci_root_ctrl_start: portsc=0x%08x\n", v));
		v &= ~EHCI_PS_CLEAR;
		switch(value) {
		case UHF_PORT_ENABLE:
			EOWRITE4(sc, port, v &~ EHCI_PS_PE);
			break;
		case UHF_PORT_SUSPEND:
			EOWRITE4(sc, port, v &~ EHCI_PS_SUSP);
			break;
		case UHF_PORT_POWER:
			if (sc->sc_hasppc)
				EOWRITE4(sc, port, v &~ EHCI_PS_PP);
			break;
		case UHF_PORT_TEST:
			DPRINTFN(2,("ehci_root_ctrl_start: clear port test "
				    "%d\n", index));
			break;
		case UHF_PORT_INDICATOR:
			DPRINTFN(2,("ehci_root_ctrl_start: clear port ind "
				    "%d\n", index));
			EOWRITE4(sc, port, v &~ EHCI_PS_PIC);
			break;
		case UHF_C_PORT_CONNECTION:
			EOWRITE4(sc, port, v | EHCI_PS_CSC);
			break;
		case UHF_C_PORT_ENABLE:
			EOWRITE4(sc, port, v | EHCI_PS_PEC);
			break;
		case UHF_C_PORT_SUSPEND:
			/* how? */
			break;
		case UHF_C_PORT_OVER_CURRENT:
			EOWRITE4(sc, port, v | EHCI_PS_OCC);
			break;
		case UHF_C_PORT_RESET:
			sc->sc_isreset[index] = 0;
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
#if 0
		switch(value) {
		case UHF_C_PORT_CONNECTION:
		case UHF_C_PORT_ENABLE:
		case UHF_C_PORT_SUSPEND:
		case UHF_C_PORT_OVER_CURRENT:
		case UHF_C_PORT_RESET:
		default:
			break;
		}
#endif
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		if (len == 0)
			break;
		if ((value & 0xff) != 0) {
			err = USBD_IOERROR;
			goto ret;
		}
		hubd = ehci_hubd;
		hubd.bNbrPorts = sc->sc_noport;
		v = EOREAD4(sc, EHCI_HCSPARAMS);
		USETW(hubd.wHubCharacteristics,
		    EHCI_HCS_PPC(v) ? UHD_PWR_INDIVIDUAL : UHD_PWR_NO_SWITCH |
		    EHCI_HCS_P_INDICATOR(EREAD4(sc, EHCI_HCSPARAMS))
		        ? UHD_PORT_IND : 0);
		hubd.bPwrOn2PwrGood = 200; /* XXX can't find out? */
		for (i = 0, l = sc->sc_noport; l > 0; i++, l -= 8, v >>= 8)
			hubd.DeviceRemovable[i++] = 0; /* XXX can't find out? */
		hubd.bDescLength = USB_HUB_DESCRIPTOR_SIZE + i;
		l = min(len, hubd.bDescLength);
		totlen = l;
		memcpy(buf, &hubd, l);
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		memset(buf, 0, len); /* ? XXX */
		totlen = len;
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
		DPRINTFN(8,("ehci_root_ctrl_start: get port status i=%d\n",
			    index));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = EOREAD4(sc, EHCI_PORTSC(index));
		DPRINTFN(8,("ehci_root_ctrl_start: port status=0x%04x\n",
			    v));
		i = UPS_HIGH_SPEED;
		if (v & EHCI_PS_CS)	i |= UPS_CURRENT_CONNECT_STATUS;
		if (v & EHCI_PS_PE)	i |= UPS_PORT_ENABLED;
		if (v & EHCI_PS_SUSP)	i |= UPS_SUSPEND;
		if (v & EHCI_PS_OCA)	i |= UPS_OVERCURRENT_INDICATOR;
		if (v & EHCI_PS_PR)	i |= UPS_RESET;
		if (v & EHCI_PS_PP)	i |= UPS_PORT_POWER;
		USETW(ps.wPortStatus, i);
		i = 0;
		if (v & EHCI_PS_CSC)	i |= UPS_C_CONNECT_STATUS;
		if (v & EHCI_PS_PEC)	i |= UPS_C_PORT_ENABLED;
		if (v & EHCI_PS_OCC)	i |= UPS_C_OVERCURRENT_INDICATOR;
		if (sc->sc_isreset[index]) i |= UPS_C_PORT_RESET;
		USETW(ps.wPortChange, i);
		l = min(len, sizeof ps);
		memcpy(buf, &ps, l);
		totlen = l;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_CLASS_DEVICE):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = EHCI_PORTSC(index);
		v = EOREAD4(sc, port);
		DPRINTFN(4, ("ehci_root_ctrl_start: portsc=0x%08x\n", v));
		v &= ~EHCI_PS_CLEAR;
		switch(value) {
		case UHF_PORT_ENABLE:
			EOWRITE4(sc, port, v | EHCI_PS_PE);
			break;
		case UHF_PORT_SUSPEND:
			EOWRITE4(sc, port, v | EHCI_PS_SUSP);
			break;
		case UHF_PORT_RESET:
			DPRINTFN(5,("ehci_root_ctrl_start: reset port %d\n",
				    index));
			if (EHCI_PS_IS_LOWSPEED(v)) {
				/* Low speed device, give up ownership. */
				ehci_disown(sc, index, 1);
				break;
			}
			/* Start reset sequence. */
			v &= ~ (EHCI_PS_PE | EHCI_PS_PR);
			EOWRITE4(sc, port, v | EHCI_PS_PR);
			/* Wait for reset to complete. */
			usb_delay_ms(&sc->sc_bus, USB_PORT_ROOT_RESET_DELAY);
			if (sc->sc_dying) {
				err = USBD_IOERROR;
				goto ret;
			}
			/* Terminate reset sequence. */
			EOWRITE4(sc, port, v);
			/* Wait for HC to complete reset. */
			usb_delay_ms(&sc->sc_bus, EHCI_PORT_RESET_COMPLETE);
			if (sc->sc_dying) {
				err = USBD_IOERROR;
				goto ret;
			}
			v = EOREAD4(sc, port);
			DPRINTF(("ehci after reset, status=0x%08x\n", v));
			if (v & EHCI_PS_PR) {
				printf("%s: port reset timeout\n",
				       USBDEVNAME(sc->sc_bus.bdev));
				return (USBD_TIMEOUT);
			}
			if (!(v & EHCI_PS_PE)) {
				/* Not a high speed device, give up ownership.*/
				ehci_disown(sc, index, 0);
				break;
			}
			sc->sc_isreset[index] = 1;
			DPRINTF(("ehci port %d reset, status = 0x%08x\n",
				 index, v));
			break;
		case UHF_PORT_POWER:
			DPRINTFN(2,("ehci_root_ctrl_start: set port power "
				    "%d (has PPC = %d)\n", index,
				    sc->sc_hasppc));
			if (sc->sc_hasppc)
				EOWRITE4(sc, port, v | EHCI_PS_PP);
			break;
		case UHF_PORT_TEST:
			DPRINTFN(2,("ehci_root_ctrl_start: set port test "
				    "%d\n", index));
			break;
		case UHF_PORT_INDICATOR:
			DPRINTFN(2,("ehci_root_ctrl_start: set port ind "
				    "%d\n", index));
			EOWRITE4(sc, port, v | EHCI_PS_PIC);
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_CLEAR_TT_BUFFER, UT_WRITE_CLASS_OTHER):
	case C(UR_RESET_TT, UT_WRITE_CLASS_OTHER):
	case C(UR_GET_TT_STATE, UT_READ_CLASS_OTHER):
	case C(UR_STOP_TT, UT_WRITE_CLASS_OTHER):
		break;
	default:
		err = USBD_IOERROR;
		goto ret;
	}
	xfer->actlen = totlen;
	err = USBD_NORMAL_COMPLETION;
 ret:
	xfer->status = err;
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
	return (USBD_IN_PROGRESS);
}

void
ehci_disown(ehci_softc_t *sc, int index, int lowspeed)
{
	int port;
	u_int32_t v;

	DPRINTF(("ehci_disown: index=%d lowspeed=%d\n", index, lowspeed));
#ifdef DIAGNOSTIC
	if (sc->sc_npcomp != 0) {
		int i = (index-1) / sc->sc_npcomp;
		if (i >= sc->sc_ncomp)
			printf("%s: strange port\n",
			       USBDEVNAME(sc->sc_bus.bdev));
		else
			printf("%s: handing over %s speed device on "
			       "port %d to %s\n",
			       USBDEVNAME(sc->sc_bus.bdev),
			       lowspeed ? "low" : "full",
			       index, USBDEVNAME(sc->sc_comps[i]->bdev));
	} else {
		printf("%s: npcomp == 0\n", USBDEVNAME(sc->sc_bus.bdev));
	}
#endif
	port = EHCI_PORTSC(index);
	v = EOREAD4(sc, port) &~ EHCI_PS_CLEAR;
	EOWRITE4(sc, port, v | EHCI_PS_PO);
}

/* Abort a root control request. */
Static void
ehci_root_ctrl_abort(usbd_xfer_handle xfer)
{
	/* Nothing to do, all transfers are synchronous. */
}

/* Close the root pipe. */
Static void
ehci_root_ctrl_close(usbd_pipe_handle pipe)
{
	DPRINTF(("ehci_root_ctrl_close\n"));
	/* Nothing to do. */
}

void
ehci_root_intr_done(usbd_xfer_handle xfer)
{
	xfer->hcpriv = NULL;
}

Static usbd_status
ehci_root_intr_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ehci_root_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

Static usbd_status
ehci_root_intr_start(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;

	if (sc->sc_dying)
		return (USBD_IOERROR);

	sc->sc_intrxfer = xfer;

	return (USBD_IN_PROGRESS);
}

/* Abort a root interrupt request. */
Static void
ehci_root_intr_abort(usbd_xfer_handle xfer)
{
	int s;

	if (xfer->pipe->intrxfer == xfer) {
		DPRINTF(("ehci_root_intr_abort: remove\n"));
		xfer->pipe->intrxfer = NULL;
	}
	xfer->status = USBD_CANCELLED;
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
}

/* Close the root pipe. */
Static void
ehci_root_intr_close(usbd_pipe_handle pipe)
{
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;

	DPRINTF(("ehci_root_intr_close\n"));

	sc->sc_intrxfer = NULL;
}

void
ehci_root_ctrl_done(usbd_xfer_handle xfer)
{
	xfer->hcpriv = NULL;
}

/************************/

ehci_soft_qh_t *
ehci_alloc_sqh(ehci_softc_t *sc)
{
	ehci_soft_qh_t *sqh;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;

	if (sc->sc_freeqhs == NULL) {
		DPRINTFN(2, ("ehci_alloc_sqh: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, EHCI_SQH_SIZE * EHCI_SQH_CHUNK,
			  EHCI_PAGE_SIZE, &dma);
#ifdef EHCI_DEBUG
		if (err)
			printf("ehci_alloc_sqh: usb_allocmem()=%d\n", err);
#endif
		if (err)
			return (NULL);
		for(i = 0; i < EHCI_SQH_CHUNK; i++) {
			offs = i * EHCI_SQH_SIZE;
			sqh = KERNADDR(&dma, offs);
			sqh->physaddr = DMAADDR(&dma, offs);
			sqh->next = sc->sc_freeqhs;
			sc->sc_freeqhs = sqh;
		}
	}
	sqh = sc->sc_freeqhs;
	sc->sc_freeqhs = sqh->next;
	memset(&sqh->qh, 0, sizeof(ehci_qh_t));
	sqh->next = NULL;
	return (sqh);
}

void
ehci_free_sqh(ehci_softc_t *sc, ehci_soft_qh_t *sqh)
{
	sqh->next = sc->sc_freeqhs;
	sc->sc_freeqhs = sqh;
}

ehci_soft_qtd_t *
ehci_alloc_sqtd(ehci_softc_t *sc)
{
	ehci_soft_qtd_t *sqtd;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;
	int s;

	if (sc->sc_freeqtds == NULL) {
		DPRINTFN(2, ("ehci_alloc_sqtd: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, EHCI_SQTD_SIZE*EHCI_SQTD_CHUNK,
			  EHCI_PAGE_SIZE, &dma);
#ifdef EHCI_DEBUG
		if (err)
			printf("ehci_alloc_sqtd: usb_allocmem()=%d\n", err);
#endif
		if (err)
			return (NULL);
		s = splusb();
		for(i = 0; i < EHCI_SQTD_CHUNK; i++) {
			offs = i * EHCI_SQTD_SIZE;
			sqtd = KERNADDR(&dma, offs);
			sqtd->physaddr = DMAADDR(&dma, offs);
			sqtd->nextqtd = sc->sc_freeqtds;
			sc->sc_freeqtds = sqtd;
		}
		splx(s);
	}

	s = splusb();
	sqtd = sc->sc_freeqtds;
	sc->sc_freeqtds = sqtd->nextqtd;
	memset(&sqtd->qtd, 0, sizeof(ehci_qtd_t));
	sqtd->nextqtd = NULL;
	sqtd->xfer = NULL;
	splx(s);

	return (sqtd);
}

void
ehci_free_sqtd(ehci_softc_t *sc, ehci_soft_qtd_t *sqtd)
{
	int s;

	s = splusb();
	sqtd->nextqtd = sc->sc_freeqtds;
	sc->sc_freeqtds = sqtd;
	splx(s);
}

usbd_status
ehci_alloc_sqtd_chain(struct ehci_pipe *epipe, ehci_softc_t *sc,
		     int alen, int rd, usbd_xfer_handle xfer,
		     ehci_soft_qtd_t **sp, ehci_soft_qtd_t **ep)
{
	ehci_soft_qtd_t *next, *cur;
	ehci_physaddr_t dataphys, dataphyspage, dataphyslastpage, nextphys;
	u_int32_t qtdstatus;
	int len, curlen, mps;
	int i, tog;
	usb_dma_t *dma = &xfer->dmabuf;
	u_int16_t flags = xfer->flags;

	DPRINTFN(alen<4*4096,("ehci_alloc_sqtd_chain: start len=%d\n", alen));

	len = alen;
	dataphys = DMAADDR(dma, 0);
	dataphyslastpage = EHCI_PAGE(dataphys + len - 1);
	qtdstatus = EHCI_QTD_ACTIVE |
	    EHCI_QTD_SET_PID(rd ? EHCI_QTD_PID_IN : EHCI_QTD_PID_OUT) |
	    EHCI_QTD_SET_CERR(3)
	    /* IOC set below */
	    /* BYTES set below */
	    ;
	mps = UGETW(epipe->pipe.endpoint->edesc->wMaxPacketSize);
	tog = epipe->nexttoggle;
	qtdstatus |= EHCI_QTD_SET_TOGGLE(tog);

	cur = ehci_alloc_sqtd(sc);
	*sp = cur;
	if (cur == NULL)
		goto nomem;
	for (;;) {
		dataphyspage = EHCI_PAGE(dataphys);
		/* The EHCI hardware can handle at most 5 pages. */
		if (dataphyslastpage - dataphyspage <
		    EHCI_QTD_NBUFFERS * EHCI_PAGE_SIZE) {
			/* we can handle it in this QTD */
			curlen = len;
		} else {
			/* must use multiple TDs, fill as much as possible. */
			curlen = EHCI_QTD_NBUFFERS * EHCI_PAGE_SIZE -
				 EHCI_PAGE_OFFSET(dataphys);
#ifdef DIAGNOSTIC
			if (curlen > len) {
				printf("ehci_alloc_sqtd_chain: curlen=0x%x "
				       "len=0x%x offs=0x%x\n", curlen, len,
				       EHCI_PAGE_OFFSET(dataphys));
				printf("lastpage=0x%x page=0x%x phys=0x%x\n",
				       dataphyslastpage, dataphyspage,
				       dataphys);
				curlen = len;
			}
#endif
			/* the length must be a multiple of the max size */
			curlen -= curlen % mps;
			DPRINTFN(1,("ehci_alloc_sqtd_chain: multiple QTDs, "
				    "curlen=%d\n", curlen));
#ifdef DIAGNOSTIC
			if (curlen == 0)
				panic("ehci_alloc_sqtd_chain: curlen == 0");
#endif
		}
		DPRINTFN(4,("ehci_alloc_sqtd_chain: dataphys=0x%08x "
			    "dataphyslastpage=0x%08x len=%d curlen=%d\n",
			    dataphys, dataphyslastpage,
			    len, curlen));
		len -= curlen;

		/*
		 * Allocate another transfer if there's more data left,
		 * or if force last short transfer flag is set and we're
		 * allocating a multiple of the max packet size.
		 */
		if (len != 0 ||
		    ((curlen % mps) == 0 && !rd && curlen != 0 &&
		     (flags & USBD_FORCE_SHORT_XFER))) {
			next = ehci_alloc_sqtd(sc);
			if (next == NULL)
				goto nomem;
			nextphys = htole32(next->physaddr);
		} else {
			next = NULL;
			nextphys = EHCI_NULL;
		}

		for (i = 0; i * EHCI_PAGE_SIZE <
		            curlen + EHCI_PAGE_OFFSET(dataphys); i++) {
			ehci_physaddr_t a = dataphys + i * EHCI_PAGE_SIZE;
			if (i != 0) /* use offset only in first buffer */
				a = EHCI_PAGE(a);
			cur->qtd.qtd_buffer[i] = htole32(a);
			cur->qtd.qtd_buffer_hi[i] = 0;
#ifdef DIAGNOSTIC
			if (i >= EHCI_QTD_NBUFFERS) {
				printf("ehci_alloc_sqtd_chain: i=%d\n", i);
				goto nomem;
			}
#endif
		}
		cur->nextqtd = next;
		cur->qtd.qtd_next = cur->qtd.qtd_altnext = nextphys;
		cur->qtd.qtd_status =
		    htole32(qtdstatus | EHCI_QTD_SET_BYTES(curlen));
		cur->xfer = xfer;
		cur->len = curlen;
		DPRINTFN(10,("ehci_alloc_sqtd_chain: cbp=0x%08x end=0x%08x\n",
			    dataphys, dataphys + curlen));
		/* adjust the toggle based on the number of packets in this
		   qtd */
		if (((curlen + mps - 1) / mps) & 1) {
			tog ^= 1;
			qtdstatus ^= EHCI_QTD_TOGGLE_MASK;
		}
		if (next == NULL)
			break;
		DPRINTFN(10,("ehci_alloc_sqtd_chain: extend chain\n"));
		dataphys += curlen;
		cur = next;
	}
	cur->qtd.qtd_status |= htole32(EHCI_QTD_IOC);
	*ep = cur;
	epipe->nexttoggle = tog;

	DPRINTFN(10,("ehci_alloc_sqtd_chain: return sqtd=%p sqtdend=%p\n",
		     *sp, *ep));

	return (USBD_NORMAL_COMPLETION);

 nomem:
	/* XXX free chain */
	DPRINTFN(-1,("ehci_alloc_sqtd_chain: no memory\n"));
	return (USBD_NOMEM);
}

Static void
ehci_free_sqtd_chain(ehci_softc_t *sc, ehci_soft_qtd_t *sqtd,
		    ehci_soft_qtd_t *sqtdend)
{
	ehci_soft_qtd_t *p;
	int i;

	DPRINTFN(10,("ehci_free_sqtd_chain: sqtd=%p sqtdend=%p\n",
		     sqtd, sqtdend));

	for (i = 0; sqtd != sqtdend; sqtd = p, i++) {
		p = sqtd->nextqtd;
		ehci_free_sqtd(sc, sqtd);
	}
}

/****************/

/*
 * Close a reqular pipe.
 * Assumes that there are no pending transactions.
 */
void
ehci_close_pipe(usbd_pipe_handle pipe, ehci_soft_qh_t *head)
{
	struct ehci_pipe *epipe = (struct ehci_pipe *)pipe;
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;
	ehci_soft_qh_t *sqh = epipe->sqh;
	int s;

	s = splusb();
	ehci_rem_qh(sc, sqh, head);
	splx(s);
	ehci_free_sqh(sc, epipe->sqh);
}

/*
 * Abort a device request.
 * If this routine is called at splusb() it guarantees that the request
 * will be removed from the hardware scheduling and that the callback
 * for it will be called with USBD_CANCELLED status.
 * It's impossible to guarantee that the requested transfer will not
 * have happened since the hardware runs concurrently.
 * If the transaction has already happened we rely on the ordinary
 * interrupt processing to process it.
 * XXX This is most probably wrong.
 */
void
ehci_abort_xfer(usbd_xfer_handle xfer, usbd_status status)
{
#define exfer EXFER(xfer)
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	ehci_softc_t *sc = (ehci_softc_t *)epipe->pipe.device->bus;
	ehci_soft_qh_t *sqh = epipe->sqh;
	ehci_soft_qtd_t *sqtd;
	ehci_physaddr_t cur;
	u_int32_t qhstatus;
	int s;
	int hit;
	int wake;

	DPRINTF(("ehci_abort_xfer: xfer=%p pipe=%p\n", xfer, epipe));

	if (sc->sc_dying) {
		/* If we're dying, just do the software part. */
		s = splusb();
		xfer->status = status;	/* make software ignore it */
		usb_uncallout(xfer->timeout_handle, ehci_timeout, xfer);
		usb_transfer_complete(xfer);
		splx(s);
		return;
	}

	if (xfer->device->bus->intr_context || !curproc)
		panic("ehci_abort_xfer: not in process context");

	/*
	 * If an abort is already in progress then just wait for it to
	 * complete and return.
	 */
	if (xfer->hcflags & UXFER_ABORTING) {
		DPRINTFN(2, ("ehci_abort_xfer: already aborting\n"));
#ifdef DIAGNOSTIC
		if (status == USBD_TIMEOUT)
			printf("ehci_abort_xfer: TIMEOUT while aborting\n");
#endif
		/* Override the status which might be USBD_TIMEOUT. */
		xfer->status = status;
		DPRINTFN(2, ("ehci_abort_xfer: waiting for abort to finish\n"));
		xfer->hcflags |= UXFER_ABORTWAIT;
		while (xfer->hcflags & UXFER_ABORTING)
			tsleep(&xfer->hcflags, PZERO, "ehciaw", 0);
		return;
	}
	xfer->hcflags |= UXFER_ABORTING;

	/*
	 * Step 1: Make interrupt routine and hardware ignore xfer.
	 */
	s = splusb();
	xfer->status = status;	/* make software ignore it */
	usb_uncallout(xfer->timeout_handle, ehci_timeout, xfer);
	qhstatus = sqh->qh.qh_qtd.qtd_status;
	sqh->qh.qh_qtd.qtd_status = qhstatus | htole32(EHCI_QTD_HALTED);
	for (sqtd = exfer->sqtdstart; ; sqtd = sqtd->nextqtd) {
		sqtd->qtd.qtd_status |= htole32(EHCI_QTD_HALTED);
		if (sqtd == exfer->sqtdend)
			break;
	}
	splx(s);

	/*
	 * Step 2: Wait until we know hardware has finished any possible
	 * use of the xfer.  Also make sure the soft interrupt routine
	 * has run.
	 */
	ehci_sync_hc(sc);
	s = splusb();
#ifdef USB_USE_SOFTINTR
	sc->sc_softwake = 1;
#endif /* USB_USE_SOFTINTR */
	usb_schedsoftintr(&sc->sc_bus);
#ifdef USB_USE_SOFTINTR
	tsleep(&sc->sc_softwake, PZERO, "ehciab", 0);
#endif /* USB_USE_SOFTINTR */
	splx(s);

	/*
	 * Step 3: Remove any vestiges of the xfer from the hardware.
	 * The complication here is that the hardware may have executed
	 * beyond the xfer we're trying to abort.  So as we're scanning
	 * the TDs of this xfer we check if the hardware points to
	 * any of them.
	 */
	s = splusb();		/* XXX why? */
	cur = EHCI_LINK_ADDR(le32toh(sqh->qh.qh_curqtd));
	hit = 0;
	for (sqtd = exfer->sqtdstart; ; sqtd = sqtd->nextqtd) {
		hit |= cur == sqtd->physaddr;
		if (sqtd == exfer->sqtdend)
			break;
	}
	sqtd = sqtd->nextqtd;
	/* Zap curqtd register if hardware pointed inside the xfer. */
	if (hit && sqtd != NULL) {
		DPRINTFN(1,("ehci_abort_xfer: cur=0x%08x\n", sqtd->physaddr));
		sqh->qh.qh_curqtd = htole32(sqtd->physaddr); /* unlink qTDs */
		sqh->qh.qh_qtd.qtd_status = qhstatus;
	} else {
		DPRINTFN(1,("ehci_abort_xfer: no hit\n"));
	}

	/*
	 * Step 4: Execute callback.
	 */
#ifdef DIAGNOSTIC
	exfer->isdone = 1;
#endif
	wake = xfer->hcflags & UXFER_ABORTWAIT;
	xfer->hcflags &= ~(UXFER_ABORTING | UXFER_ABORTWAIT);
	usb_transfer_complete(xfer);
	if (wake)
		wakeup(&xfer->hcflags);

	splx(s);
#undef exfer
}

void
ehci_timeout(void *addr)
{
	struct ehci_xfer *exfer = addr;
	struct ehci_pipe *epipe = (struct ehci_pipe *)exfer->xfer.pipe;
	ehci_softc_t *sc = (ehci_softc_t *)epipe->pipe.device->bus;

	DPRINTF(("ehci_timeout: exfer=%p\n", exfer));
#ifdef USB_DEBUG
	if (ehcidebug > 1)
		usbd_dump_pipe(exfer->xfer.pipe);
#endif

	if (sc->sc_dying) {
		ehci_abort_xfer(&exfer->xfer, USBD_TIMEOUT);
		return;
	}

	/* Execute the abort in a process context. */
	usb_init_task(&exfer->abort_task, ehci_timeout_task, addr);
	usb_add_task(exfer->xfer.pipe->device, &exfer->abort_task,
	    USB_TASKQ_HC);
}

void
ehci_timeout_task(void *addr)
{
	usbd_xfer_handle xfer = addr;
	int s;

	DPRINTF(("ehci_timeout_task: xfer=%p\n", xfer));

	s = splusb();
	ehci_abort_xfer(xfer, USBD_TIMEOUT);
	splx(s);
}

/************************/

Static usbd_status
ehci_device_ctrl_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ehci_device_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

Static usbd_status
ehci_device_ctrl_start(usbd_xfer_handle xfer)
{
	ehci_softc_t *sc = (ehci_softc_t *)xfer->pipe->device->bus;
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST)) {
		/* XXX panic */
		printf("ehci_device_ctrl_transfer: not a request\n");
		return (USBD_INVAL);
	}
#endif

	err = ehci_device_request(xfer);
	if (err)
		return (err);

	if (sc->sc_bus.use_polling)
		ehci_waitintr(sc, xfer);
	return (USBD_IN_PROGRESS);
}

void
ehci_device_ctrl_done(usbd_xfer_handle xfer)
{
	struct ehci_xfer *ex = EXFER(xfer);
	ehci_softc_t *sc = (ehci_softc_t *)xfer->pipe->device->bus;
	/*struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;*/

	DPRINTFN(10,("ehci_ctrl_done: xfer=%p\n", xfer));

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST)) {
		panic("ehci_ctrl_done: not a request");
	}
#endif

	if (xfer->status != USBD_NOMEM && ehci_active_intr_list(ex)) {
		ehci_del_intr_list(ex);	/* remove from active list */
		ehci_free_sqtd_chain(sc, ex->sqtdstart, NULL);
	}

	DPRINTFN(5, ("ehci_ctrl_done: length=%d\n", xfer->actlen));
}

/* Abort a device control request. */
Static void
ehci_device_ctrl_abort(usbd_xfer_handle xfer)
{
	DPRINTF(("ehci_device_ctrl_abort: xfer=%p\n", xfer));
	ehci_abort_xfer(xfer, USBD_CANCELLED);
}

/* Close a device control pipe. */
Static void
ehci_device_ctrl_close(usbd_pipe_handle pipe)
{
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;
	/*struct ehci_pipe *epipe = (struct ehci_pipe *)pipe;*/

	DPRINTF(("ehci_device_ctrl_close: pipe=%p\n", pipe));
	ehci_close_pipe(pipe, sc->sc_async_head);
}

usbd_status
ehci_device_request(usbd_xfer_handle xfer)
{
#define exfer EXFER(xfer)
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	usb_device_request_t *req = &xfer->request;
	usbd_device_handle dev = epipe->pipe.device;
	ehci_softc_t *sc = (ehci_softc_t *)dev->bus;
	int addr = dev->address;
	ehci_soft_qtd_t *setup, *stat, *next;
	ehci_soft_qh_t *sqh;
	int isread;
	int len;
	usbd_status err;
	int s;

	isread = req->bmRequestType & UT_READ;
	len = UGETW(req->wLength);

	DPRINTFN(3,("ehci_device_request: type=0x%02x, request=0x%02x, "
		    "wValue=0x%04x, wIndex=0x%04x len=%d, addr=%d, endpt=%d\n",
		    req->bmRequestType, req->bRequest, UGETW(req->wValue),
		    UGETW(req->wIndex), len, addr,
		    epipe->pipe.endpoint->edesc->bEndpointAddress));

	setup = ehci_alloc_sqtd(sc);
	if (setup == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	stat = ehci_alloc_sqtd(sc);
	if (stat == NULL) {
		err = USBD_NOMEM;
		goto bad2;
	}

	sqh = epipe->sqh;
	epipe->u.ctl.length = len;

	/* Update device address and length since they may have changed
	   during the setup of the control pipe in usbd_new_device(). */
	/* XXX This only needs to be done once, but it's too early in open. */
	/* XXXX Should not touch ED here! */
	sqh->qh.qh_endp =
	    (sqh->qh.qh_endp & htole32(~(EHCI_QH_ADDRMASK | EHCI_QH_MPLMASK))) |
	    htole32(
	     EHCI_QH_SET_ADDR(addr) |
	     EHCI_QH_SET_MPL(UGETW(epipe->pipe.endpoint->edesc->wMaxPacketSize))
	    );

	/* Set up data transaction */
	if (len != 0) {
		ehci_soft_qtd_t *end;

		/* Start toggle at 1. */
		epipe->nexttoggle = 1;
		err = ehci_alloc_sqtd_chain(epipe, sc, len, isread, xfer,
			  &next, &end);
		if (err)
			goto bad3;
		end->qtd.qtd_status &= htole32(~EHCI_QTD_IOC);
		end->nextqtd = stat;
		end->qtd.qtd_next =
		end->qtd.qtd_altnext = htole32(stat->physaddr);
	} else {
		next = stat;
	}

	memcpy(KERNADDR(&epipe->u.ctl.reqdma, 0), req, sizeof *req);

	/* Clear toggle */
	setup->qtd.qtd_status = htole32(
	    EHCI_QTD_ACTIVE |
	    EHCI_QTD_SET_PID(EHCI_QTD_PID_SETUP) |
	    EHCI_QTD_SET_CERR(3) |
	    EHCI_QTD_SET_TOGGLE(0) |
	    EHCI_QTD_SET_BYTES(sizeof *req)
	    );
	setup->qtd.qtd_buffer[0] = htole32(DMAADDR(&epipe->u.ctl.reqdma, 0));
	setup->qtd.qtd_buffer_hi[0] = 0;
	setup->nextqtd = next;
	setup->qtd.qtd_next = setup->qtd.qtd_altnext = htole32(next->physaddr);
	setup->xfer = xfer;
	setup->len = sizeof *req;

	stat->qtd.qtd_status = htole32(
	    EHCI_QTD_ACTIVE |
	    EHCI_QTD_SET_PID(isread ? EHCI_QTD_PID_OUT : EHCI_QTD_PID_IN) |
	    EHCI_QTD_SET_CERR(3) |
	    EHCI_QTD_SET_TOGGLE(1) |
	    EHCI_QTD_IOC
	    );
	stat->qtd.qtd_buffer[0] = 0; /* XXX not needed? */
	stat->qtd.qtd_buffer_hi[0] = 0; /* XXX not needed? */
	stat->nextqtd = NULL;
	stat->qtd.qtd_next = stat->qtd.qtd_altnext = EHCI_NULL;
	stat->xfer = xfer;
	stat->len = 0;

#ifdef EHCI_DEBUG
	if (ehcidebug > 5) {
		DPRINTF(("ehci_device_request:\n"));
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(setup);
	}
#endif

	exfer->sqtdstart = setup;
	exfer->sqtdend = stat;
#ifdef DIAGNOSTIC
	if (!exfer->isdone) {
		printf("ehci_device_request: not done, exfer=%p\n", exfer);
	}
	exfer->isdone = 0;
#endif

	/* Insert qTD in QH list. */
	s = splusb();
	ehci_set_qh_qtd(sqh, setup);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
                usb_callout(xfer->timeout_handle, mstohz(xfer->timeout),
			    ehci_timeout, xfer);
	}
	ehci_add_intr_list(sc, exfer);
	xfer->status = USBD_IN_PROGRESS;
	splx(s);

#ifdef EHCI_DEBUG
	if (ehcidebug > 10) {
		DPRINTF(("ehci_device_request: status=%x\n",
			 EOREAD4(sc, EHCI_USBSTS)));
		delay(10000);
		ehci_dump_regs(sc);
		ehci_dump_sqh(sc->sc_async_head);
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(setup);
	}
#endif

	return (USBD_NORMAL_COMPLETION);

 bad3:
	ehci_free_sqtd(sc, stat);
 bad2:
	ehci_free_sqtd(sc, setup);
 bad1:
	DPRINTFN(-1,("ehci_device_request: no memory\n"));
	xfer->status = err;
	usb_transfer_complete(xfer);
	return (err);
#undef exfer
}

/*
 * Some EHCI chips from VIA seem to trigger interrupts before writing back the
 * qTD status, or miss signalling occasionally under heavy load.  If the host
 * machine is too fast, we we can miss transaction completion - when we scan
 * the active list the transaction still seems to be active.  This generally
 * exhibits itself as a umass stall that never recovers.
 *
 * We work around this behaviour by setting up this callback after any softintr
 * that completes with transactions still pending, giving us another chance to
 * check for completion after the writeback has taken place.
 */
void
ehci_intrlist_timeout(void *arg)
{
	ehci_softc_t *sc = arg;
	int s = splusb();

	DPRINTF(("ehci_intrlist_timeout\n"));
	usb_schedsoftintr(&sc->sc_bus);

	splx(s);
}

/************************/

Static usbd_status
ehci_device_bulk_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ehci_device_bulk_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
ehci_device_bulk_start(usbd_xfer_handle xfer)
{
#define exfer EXFER(xfer)
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	usbd_device_handle dev = epipe->pipe.device;
	ehci_softc_t *sc = (ehci_softc_t *)dev->bus;
	ehci_soft_qtd_t *data, *dataend;
	ehci_soft_qh_t *sqh;
	usbd_status err;
	int len, isread, endpt;
	int s;

	DPRINTFN(2, ("ehci_device_bulk_start: xfer=%p len=%d flags=%d\n",
		     xfer, xfer->length, xfer->flags));

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST)
		panic("ehci_device_bulk_start: a request");
#endif

	len = xfer->length;
	endpt = epipe->pipe.endpoint->edesc->bEndpointAddress;
	isread = UE_GET_DIR(endpt) == UE_DIR_IN;
	sqh = epipe->sqh;

	epipe->u.bulk.length = len;

	err = ehci_alloc_sqtd_chain(epipe, sc, len, isread, xfer, &data,
				   &dataend);
	if (err) {
		DPRINTFN(-1,("ehci_device_bulk_transfer: no memory\n"));
		xfer->status = err;
		usb_transfer_complete(xfer);
		return (err);
	}

#ifdef EHCI_DEBUG
	if (ehcidebug > 5) {
		DPRINTF(("ehci_device_bulk_start: data(1)\n"));
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(data);
	}
#endif

	/* Set up interrupt info. */
	exfer->sqtdstart = data;
	exfer->sqtdend = dataend;
#ifdef DIAGNOSTIC
	if (!exfer->isdone) {
		printf("ehci_device_bulk_start: not done, ex=%p\n", exfer);
	}
	exfer->isdone = 0;
#endif

	s = splusb();
	ehci_set_qh_qtd(sqh, data);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
		usb_callout(xfer->timeout_handle, mstohz(xfer->timeout),
			    ehci_timeout, xfer);
	}
	ehci_add_intr_list(sc, exfer);
	xfer->status = USBD_IN_PROGRESS;
	splx(s);

#ifdef EHCI_DEBUG
	if (ehcidebug > 10) {
		DPRINTF(("ehci_device_bulk_start: data(2)\n"));
		delay(10000);
		DPRINTF(("ehci_device_bulk_start: data(3)\n"));
		ehci_dump_regs(sc);
#if 0
		printf("async_head:\n");
		ehci_dump_sqh(sc->sc_async_head);
#endif
		printf("sqh:\n");
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(data);
	}
#endif

	if (sc->sc_bus.use_polling)
		ehci_waitintr(sc, xfer);

	return (USBD_IN_PROGRESS);
#undef exfer
}

Static void
ehci_device_bulk_abort(usbd_xfer_handle xfer)
{
	DPRINTF(("ehci_device_bulk_abort: xfer=%p\n", xfer));
	ehci_abort_xfer(xfer, USBD_CANCELLED);
}

/*
 * Close a device bulk pipe.
 */
Static void
ehci_device_bulk_close(usbd_pipe_handle pipe)
{
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;

	DPRINTF(("ehci_device_bulk_close: pipe=%p\n", pipe));
	ehci_close_pipe(pipe, sc->sc_async_head);
}

void
ehci_device_bulk_done(usbd_xfer_handle xfer)
{
	struct ehci_xfer *ex = EXFER(xfer);
	ehci_softc_t *sc = (ehci_softc_t *)xfer->pipe->device->bus;
	/*struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;*/

	DPRINTFN(10,("ehci_bulk_done: xfer=%p, actlen=%d\n",
		     xfer, xfer->actlen));

	if (xfer->status != USBD_NOMEM && ehci_active_intr_list(ex)) {
		ehci_del_intr_list(ex);	/* remove from active list */
		ehci_free_sqtd_chain(sc, ex->sqtdstart, NULL);
	}

	DPRINTFN(5, ("ehci_bulk_done: length=%d\n", xfer->actlen));
}

/************************/

Static usbd_status
ehci_device_setintr(ehci_softc_t *sc, ehci_soft_qh_t *sqh, int ival)
{
	struct ehci_soft_islot *isp;
	int islot, lev;

	/* Find a poll rate that is large enough. */
	for (lev = EHCI_IPOLLRATES - 1; lev > 0; lev--)
		if (EHCI_ILEV_IVAL(lev) <= ival)
			break;

	/* Pick an interrupt slot at the right level. */
	/* XXX could do better than picking at random */
	sc->sc_rand = (sc->sc_rand + 191) % sc->sc_flsize;
	islot = EHCI_IQHIDX(lev, sc->sc_rand);

	sqh->islot = islot;
	isp = &sc->sc_islots[islot];
	ehci_add_qh(sqh, isp->sqh);

	return (USBD_NORMAL_COMPLETION);
}

Static usbd_status
ehci_device_intr_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/*
	 * Pipe isn't running (otherwise err would be USBD_INPROG),
	 * so start it first.
	 */
	return (ehci_device_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

Static usbd_status
ehci_device_intr_start(usbd_xfer_handle xfer)
{
#define exfer EXFER(xfer)
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	usbd_device_handle dev = xfer->pipe->device;
	ehci_softc_t *sc = (ehci_softc_t *)dev->bus;
	ehci_soft_qtd_t *data, *dataend;
	ehci_soft_qh_t *sqh;
	usbd_status err;
	int len, isread, endpt;
	int s;

	DPRINTFN(2, ("ehci_device_intr_start: xfer=%p len=%d flags=%d\n",
	    xfer, xfer->length, xfer->flags));

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST)
		panic("ehci_device_intr_start: a request");
#endif

	len = xfer->length;
	endpt = epipe->pipe.endpoint->edesc->bEndpointAddress;
	isread = UE_GET_DIR(endpt) == UE_DIR_IN;
	sqh = epipe->sqh;

	epipe->u.intr.length = len;

	err = ehci_alloc_sqtd_chain(epipe, sc, len, isread, xfer, &data,
	    &dataend);
	if (err) {
		DPRINTFN(-1, ("ehci_device_intr_start: no memory\n"));
		xfer->status = err;
		usb_transfer_complete(xfer);
		return (err);
	}

#ifdef EHCI_DEBUG
	if (ehcidebug > 5) {
		DPRINTF(("ehci_device_intr_start: data(1)\n"));
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(data);
	}
#endif

	/* Set up interrupt info. */
	exfer->sqtdstart = data;
	exfer->sqtdend = dataend;
#ifdef DIAGNOSTIC
	if (!exfer->isdone) {
		printf("ehci_device_intr_start: not done, ex=%p\n", exfer);
	}
	exfer->isdone = 0;
#endif

	s = splusb();
	ehci_set_qh_qtd(sqh, data);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
		usb_callout(xfer->timeout_handle, mstohz(xfer->timeout),
		    ehci_timeout, xfer);
	}
	ehci_add_intr_list(sc, exfer);
	xfer->status = USBD_IN_PROGRESS;
	splx(s);

#ifdef EHCI_DEBUG
	if (ehcidebug > 10) {
		DPRINTF(("ehci_device_intr_start: data(2)\n"));
		delay(10000);
		DPRINTF(("ehci_device_intr_start: data(3)\n"));
		ehci_dump_regs(sc);
		printf("sqh:\n");
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(data);
	}
#endif

	if (sc->sc_bus.use_polling)
		ehci_waitintr(sc, xfer);

	return (USBD_IN_PROGRESS);
#undef exfer
}

Static void
ehci_device_intr_abort(usbd_xfer_handle xfer)
{
	DPRINTFN(1, ("ehci_device_intr_abort: xfer=%p\n", xfer));
	if (xfer->pipe->intrxfer == xfer) {
		DPRINTFN(1, ("echi_device_intr_abort: remove\n"));
		xfer->pipe->intrxfer = NULL;
	}
	ehci_abort_xfer(xfer, USBD_CANCELLED);
}

Static void
ehci_device_intr_close(usbd_pipe_handle pipe)
{
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;
	struct ehci_pipe *epipe = (struct ehci_pipe *)pipe;
	struct ehci_soft_islot *isp;

	isp = &sc->sc_islots[epipe->sqh->islot];
	ehci_close_pipe(pipe, isp->sqh);
}

Static void
ehci_device_intr_done(usbd_xfer_handle xfer)
{
#define exfer EXFER(xfer)
	struct ehci_xfer *ex = EXFER(xfer);
	ehci_softc_t *sc = (ehci_softc_t *)xfer->pipe->device->bus;
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	ehci_soft_qtd_t *data, *dataend;
	ehci_soft_qh_t *sqh;
	usbd_status err;
	int len, isread, endpt, s;

	DPRINTFN(10, ("ehci_device_intr_done: xfer=%p, actlen=%d\n",
	    xfer, xfer->actlen));

	if (xfer->pipe->repeat) {
		ehci_free_sqtd_chain(sc, ex->sqtdstart, NULL);

		len = epipe->u.intr.length;
		xfer->length = len;
		endpt = epipe->pipe.endpoint->edesc->bEndpointAddress;
		isread = UE_GET_DIR(endpt) == UE_DIR_IN;
		sqh = epipe->sqh;

		err = ehci_alloc_sqtd_chain(epipe, sc, len, isread, xfer,
		    &data, &dataend);
		if (err) {
			DPRINTFN(-1, ("ehci_device_intr_done: no memory\n"));
			xfer->status = err;
			return;
		}

		/* Set up interrupt info. */
		exfer->sqtdstart = data;
		exfer->sqtdend = dataend;
#ifdef DIAGNOSTIC
		if (!exfer->isdone) {
			printf("ehci_device_intr_done: not done, ex=%p\n",
			    exfer);
		}
		exfer->isdone = 0;
#endif

		s = splusb();
		ehci_set_qh_qtd(sqh, data);
		if (xfer->timeout && !sc->sc_bus.use_polling) {
			usb_callout(xfer->timeout_handle,
			    mstohz(xfer->timeout), ehci_timeout, xfer);
		}
		splx(s);

		xfer->status = USBD_IN_PROGRESS;
	} else if (xfer->status != USBD_NOMEM && ehci_active_intr_list(ex)) {
		ehci_del_intr_list(ex); /* remove from active list */
		ehci_free_sqtd_chain(sc, ex->sqtdstart, NULL);
	}
#undef exfer
}

/************************/

Static usbd_status
ehci_device_isoc_transfer(usbd_xfer_handle xfer)
{
	return USBD_IOERROR;
}
Static usbd_status
ehci_device_isoc_start(usbd_xfer_handle xfer)
{
	return USBD_IOERROR;
}
Static void
ehci_device_isoc_abort(usbd_xfer_handle xfer)
{
}
Static void
ehci_device_isoc_close(usbd_pipe_handle pipe)
{
}
Static void
ehci_device_isoc_done(usbd_xfer_handle xfer)
{
}
