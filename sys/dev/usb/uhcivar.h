/*	$NetBSD: uhcivar.h,v 1.16.4.1 1999/11/15 00:41:34 fvdl Exp $	*/

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
 * To avoid having 1024 TDs for each isochronous transfer we introduce
 * a virtual frame list.  Every UHCI_VFRAMELIST_COUNT entries in the real
 * frame list points to a non-active TD.  These, in turn, which form the 
 * starts of the virtual frame list.  This also has the advantage that it 
 * simplifies linking in/out TD/QH in the schedule.
 * Furthermore, initially each of the inactive TDs point to an inactive
 * QH that forms the start of the interrupt traffic for that slot.
 * Each of these QHs point to the same QH that is the start of control
 * traffic.
 *
 * UHCI_VFRAMELIST_COUNT should be a power of 2 and <= UHCI_FRAMELIST_COUNT.
 */
#define UHCI_VFRAMELIST_COUNT 128

typedef struct uhci_soft_qh uhci_soft_qh_t;
typedef struct uhci_soft_td uhci_soft_td_t;

typedef union {
	struct uhci_soft_qh *sqh;
	struct uhci_soft_td *std;
} uhci_soft_td_qh_t;

/*
 * An interrupt info struct contains the information needed to
 * execute a requested routine when the controller generates an
 * interrupt.  Since we cannot know which transfer generated
 * the interrupt all structs are linked together so they can be
 * searched at interrupt time.
 */
typedef struct uhci_intr_info {
	struct uhci_softc *sc;
	usbd_xfer_handle xfer;
	uhci_soft_td_t *stdstart;
	uhci_soft_td_t *stdend;
	LIST_ENTRY(uhci_intr_info) list;
#if defined(__FreeBSD__)
	struct callout_handle timeout_handle;
#endif /* defined(__FreeBSD__) */
#ifdef DIAGNOSTIC
	int isdone;
#endif
} uhci_intr_info_t;

/*
 * Extra information that we need for a TD.
 */
struct uhci_soft_td {
	uhci_td_t td;			/* The real TD, must be first */
	uhci_soft_td_qh_t link; 	/* soft version of the td_link field */
	uhci_physaddr_t physaddr;	/* TD's physical address. */
};
/* 
 * Make the size such that it is a multiple of UHCI_TD_ALIGN.  This way
 * we can pack a number of soft TD together and have the real TS well
 * aligned.
 * NOTE: Minimum size is 32 bytes.
 */
#define UHCI_STD_SIZE ((sizeof (struct uhci_soft_td) + UHCI_TD_ALIGN - 1) / UHCI_TD_ALIGN * UHCI_TD_ALIGN)
#define UHCI_STD_CHUNK 128 /*(PAGE_SIZE / UHCI_TD_SIZE)*/

/*
 * Extra information that we need for a QH.
 */
struct uhci_soft_qh {
	uhci_qh_t qh;			/* The real QH, must be first */
	uhci_soft_qh_t *hlink;		/* soft version of qh_hlink */
	uhci_soft_td_t *elink;		/* soft version of qh_elink */
	uhci_physaddr_t physaddr;	/* QH's physical address. */
	int pos;			/* Timeslot position */
	uhci_intr_info_t *intr_info;	/* Who to call on completion. */
/* XXX should try to shrink with 4 bytes to fit into 32 bytes */
};
/* See comment about UHCI_STD_SIZE. */
#define UHCI_SQH_SIZE ((sizeof (struct uhci_soft_qh) + UHCI_QH_ALIGN - 1) / UHCI_QH_ALIGN * UHCI_QH_ALIGN)
#define UHCI_SQH_CHUNK 128 /*(PAGE_SIZE / UHCI_QH_SIZE)*/

/*
 * Information about an entry in the virtial frame list.
 */
struct uhci_vframe {
	uhci_soft_td_t *htd;		/* pointer to dummy TD */
	uhci_soft_td_t *etd;		/* pointer to last TD */
	uhci_soft_qh_t *hqh;		/* pointer to dummy QH */
	uhci_soft_qh_t *eqh;		/* pointer to last QH */
	u_int bandwidth;		/* max bandwidth used by this frame */
};

typedef struct uhci_softc {
	struct usbd_bus sc_bus;		/* base device */
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	uhci_physaddr_t *sc_pframes;
	usb_dma_t sc_dma;
	struct uhci_vframe sc_vframes[UHCI_VFRAMELIST_COUNT];

	uhci_soft_qh_t *sc_ctl_start;	/* dummy QH for control */
	uhci_soft_qh_t *sc_ctl_end;	/* last control QH */
	uhci_soft_qh_t *sc_bulk_start;	/* dummy QH for bulk */
	uhci_soft_qh_t *sc_bulk_end;	/* last bulk transfer */

	uhci_soft_td_t *sc_freetds;
	uhci_soft_qh_t *sc_freeqhs;

	u_int8_t sc_addr;		/* device address */
	u_int8_t sc_conf;		/* device configuration */

	char sc_isreset;

	char sc_suspend;
	usbd_xfer_handle sc_has_timo;

	LIST_HEAD(, uhci_intr_info) sc_intrhead;

	/* Info for the root hub interrupt channel. */
	int sc_ival;

	char sc_vflock;
#define UHCI_HAS_LOCK 1
#define UHCI_WANT_LOCK 2

	char sc_vendor[16];
	int sc_id_vendor;

	void *sc_powerhook;
	device_ptr_t sc_child;
} uhci_softc_t;

usbd_status	uhci_init __P((uhci_softc_t *));
int		uhci_intr __P((void *));
int		uhci_detach __P((uhci_softc_t *, int));
int		uhci_activate __P((device_ptr_t, enum devact));

