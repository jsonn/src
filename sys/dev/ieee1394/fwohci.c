/*	$NetBSD: fwohci.c,v 1.16.2.3 2001/08/24 00:09:41 nathanw Exp $	*/

#define DOUBLEBUF 1
#define NO_THREAD 1
/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
 * IEEE1394 Open Host Controller Interface
 *	based on OHCI Specification 1.1 (January 6, 2000)
 * The first version to support network interface part is wrtten by
 * Atsushi Onoe <onoe@netbsd.org>.
 */

/*
 * The first version to support isochronous acquisition part is wrtten
 * by HAYAKAWA Koichi <haya@netbsd.org>.
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>

#if __NetBSD_Version__ >= 105010000
#include <uvm/uvm_extern.h>
#else
#include <vm/vm.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ieee1394/ieee1394reg.h>
#include <dev/ieee1394/fwohcireg.h>

#include <dev/ieee1394/ieee1394var.h>
#include <dev/ieee1394/fwohcivar.h>

static const char * const ieee1394_speeds[] = { IEEE1394_SPD_STRINGS };

#if 0
static int fwohci_dnamem_alloc(struct fwohci_softc *sc, int size,
    int alignment, bus_dmamap_t *mapp, caddr_t *kvap, int flags);
#endif
static void fwohci_create_event_thread(void *);
static void fwohci_thread_init(void *);

static void fwohci_event_thread(struct fwohci_softc *);
static void fwohci_hw_init(struct fwohci_softc *);
static void fwohci_power(int, void *);
static void fwohci_shutdown(void *);

static int  fwohci_desc_alloc(struct fwohci_softc *);
static struct fwohci_desc *fwohci_desc_get(struct fwohci_softc *, int);
static void fwohci_desc_put(struct fwohci_softc *, struct fwohci_desc *, int);

static int  fwohci_ctx_alloc(struct fwohci_softc *, struct fwohci_ctx **,
    int, int, int);
static void fwohci_ctx_free(struct fwohci_softc *, struct fwohci_ctx *);
static void fwohci_ctx_init(struct fwohci_softc *, struct fwohci_ctx *);

static int  fwohci_buf_alloc(struct fwohci_softc *, struct fwohci_buf *);
static void fwohci_buf_free(struct fwohci_softc *, struct fwohci_buf *);
static void fwohci_buf_init_rx(struct fwohci_softc *);
static void fwohci_buf_start_rx(struct fwohci_softc *);
static void fwohci_buf_stop_tx(struct fwohci_softc *);
static void fwohci_buf_stop_rx(struct fwohci_softc *);
static void fwohci_buf_next(struct fwohci_softc *, struct fwohci_ctx *);
static int  fwohci_buf_pktget(struct fwohci_softc *, struct fwohci_buf **,
    caddr_t *, int);
static int  fwohci_buf_input(struct fwohci_softc *, struct fwohci_ctx *,
    struct fwohci_pkt *);
static int  fwohci_buf_input_ppb(struct fwohci_softc *, struct fwohci_ctx *,
    struct fwohci_pkt *);

static u_int8_t fwohci_phy_read(struct fwohci_softc *, u_int8_t);
static void fwohci_phy_write(struct fwohci_softc *, u_int8_t, u_int8_t);
static void fwohci_phy_busreset(struct fwohci_softc *);
static void fwohci_phy_input(struct fwohci_softc *, struct fwohci_pkt *);

static int  fwohci_handler_set(struct fwohci_softc *, int, u_int32_t, u_int32_t,
    int (*)(struct fwohci_softc *, void *, struct fwohci_pkt *), void *);

static void fwohci_arrq_input(struct fwohci_softc *, struct fwohci_ctx *);
static void fwohci_arrs_input(struct fwohci_softc *, struct fwohci_ctx *);
static void fwohci_ir_input(struct fwohci_softc *, struct fwohci_ctx *);

static int  fwohci_at_output(struct fwohci_softc *, struct fwohci_ctx *,
    struct fwohci_pkt *);
static void fwohci_at_done(struct fwohci_softc *, struct fwohci_ctx *, int);
static void fwohci_atrs_output(struct fwohci_softc *, int, struct fwohci_pkt *,
    struct fwohci_pkt *);

static int  fwohci_guidrom_init(struct fwohci_softc *);
static void fwohci_configrom_init(struct fwohci_softc *);
static int  fwohci_configrom_input(struct fwohci_softc *, void *,
    struct fwohci_pkt *);
static void fwohci_selfid_init(struct fwohci_softc *);
static int  fwohci_selfid_input(struct fwohci_softc *);

static void fwohci_csr_init(struct fwohci_softc *);
static int  fwohci_csr_input(struct fwohci_softc *, void *,
    struct fwohci_pkt *);

static void fwohci_uid_collect(struct fwohci_softc *);
static void fwohci_uid_req(struct fwohci_softc *, int);
static int  fwohci_uid_input(struct fwohci_softc *, void *,
    struct fwohci_pkt *);
static int  fwohci_uid_lookup(struct fwohci_softc *, const u_int8_t *);
static void fwohci_check_nodes(struct fwohci_softc *);

static int  fwohci_if_inreg(struct device *, u_int32_t, u_int32_t,
    void (*)(struct device *, struct mbuf *));
static int  fwohci_if_input(struct fwohci_softc *, void *, struct fwohci_pkt *);
static int  fwohci_if_input_iso(struct fwohci_softc *, void *, struct fwohci_pkt *);
static int  fwohci_if_output(struct device *, struct mbuf *,
    void (*)(struct device *, struct mbuf *));
static int fwohci_if_setiso(struct device *, u_int32_t, u_int32_t, u_int32_t,
    void (*)(struct device *, struct mbuf *));
static int  fwohci_read(struct ieee1394_abuf *);
static int  fwohci_write(struct ieee1394_abuf *);
static int  fwohci_read_resp(struct fwohci_softc *, void *, struct fwohci_pkt *);
static int  fwohci_write_ack(struct fwohci_softc *, void *, struct fwohci_pkt *);
static int  fwohci_read_multi_resp(struct fwohci_softc *, void *,
    struct fwohci_pkt *);
static int  fwohci_inreg(struct ieee1394_abuf *, int);
static int  fwohci_parse_input(struct fwohci_softc *, void *,
    struct fwohci_pkt *);
static int  fwohci_submatch(struct device *, struct cfdata *, void *);

#ifdef FW_DEBUG
static void fwohci_show_intr(struct fwohci_softc *, u_int32_t);
static void fwohci_show_phypkt(struct fwohci_softc *, u_int32_t);

/* 1 is normal debug, 2 is verbose debug, 3 is complete (packet dumps). */

#define DPRINTF(x)      if (fwdebug) printf x
#define DPRINTFN(n,x)   if (fwdebug>(n)) printf x
int     fwdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

int
fwohci_init(struct fwohci_softc *sc, const struct evcnt *ev)
{
	int i;
	u_int32_t val;
#if 0
	int error;
#endif

	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, ev,
	    sc->sc_sc1394.sc1394_dev.dv_xname, "intr");

	evcnt_attach_dynamic(&sc->sc_isocnt, EVCNT_TYPE_MISC, ev,
	    sc->sc_sc1394.sc1394_dev.dv_xname, "iso");
	evcnt_attach_dynamic(&sc->sc_isopktcnt, EVCNT_TYPE_MISC, ev,
	    sc->sc_sc1394.sc1394_dev.dv_xname, "isopackets");

	/*
	 * Wait for reset completion
	 */
	for (i = 0; i < OHCI_LOOP; i++) {
		val = OHCI_CSR_READ(sc, OHCI_REG_HCControlClear);
		if ((val & OHCI_HCControl_SoftReset) == 0)
			break;
		DELAY(10);
	}

	/* What dialect of OHCI is this device?
	 */
	val = OHCI_CSR_READ(sc, OHCI_REG_Version);
	printf("%s: OHCI %u.%u", sc->sc_sc1394.sc1394_dev.dv_xname,
	    OHCI_Version_GET_Version(val), OHCI_Version_GET_Revision(val));

	LIST_INIT(&sc->sc_nodelist);

	if (fwohci_guidrom_init(sc) != 0) {
		printf("\n%s: fatal: no global UID ROM\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
		return -1;
	}

	printf(", %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
	    sc->sc_sc1394.sc1394_guid[0], sc->sc_sc1394.sc1394_guid[1],
	    sc->sc_sc1394.sc1394_guid[2], sc->sc_sc1394.sc1394_guid[3],
	    sc->sc_sc1394.sc1394_guid[4], sc->sc_sc1394.sc1394_guid[5],
	    sc->sc_sc1394.sc1394_guid[6], sc->sc_sc1394.sc1394_guid[7]);

	/* Get the maximum link speed and receive size
	 */
	val = OHCI_CSR_READ(sc, OHCI_REG_BusOptions);
	sc->sc_sc1394.sc1394_link_speed =
	    OHCI_BITVAL(val, OHCI_BusOptions_LinkSpd);
	if (sc->sc_sc1394.sc1394_link_speed < IEEE1394_SPD_MAX) {
		printf(", %s",
		    ieee1394_speeds[sc->sc_sc1394.sc1394_link_speed]);
	} else {
		printf(", unknown speed %u", sc->sc_sc1394.sc1394_link_speed);
	}
	
	/* MaxRec is encoded as log2(max_rec_octets)-1
	 */
	sc->sc_sc1394.sc1394_max_receive =
	    1 << (OHCI_BITVAL(val, OHCI_BusOptions_MaxRec) + 1);
	printf(", %u max_rec", sc->sc_sc1394.sc1394_max_receive);

	/*
	 * Count how many isochronous ctx we have.
	 */
	OHCI_CSR_WRITE(sc, OHCI_REG_IsoRecvIntMaskSet, ~0);
	val = OHCI_CSR_READ(sc, OHCI_REG_IsoRecvIntMaskClear);
	OHCI_CSR_WRITE(sc, OHCI_REG_IsoRecvIntMaskClear, ~0);
	for (i = 0; val != 0; val >>= 1) {
		if (val & 0x1)
			i++;
	}
	sc->sc_isoctx = i;
	printf(", %d iso_ctx", sc->sc_isoctx);
	
	printf("\n");

#if 0
	error = fwohci_dnamem_alloc(sc, OHCI_CONFIG_SIZE,
	    OHCI_CONFIG_ALIGNMENT, &sc->sc_configrom_map,
	    (caddr_t *) &sc->sc_configrom, BUS_DMA_WAITOK|BUS_DMA_COHERENT);
	return error;
#endif

	sc->sc_dying = 0;
	sc->sc_nodeid = 0xffff;		/* invalid */

	kthread_create(fwohci_create_event_thread, sc);
	return 0;
}

static int
fwohci_if_setiso(struct device *self, u_int32_t channel, u_int32_t tag,
    u_int32_t direction, void (*handler)(struct device *, struct mbuf *))
{
	struct fwohci_softc *sc = (struct fwohci_softc *)self;
	int retval;
	int s;

	if (direction == 1) {
		return EIO;
	}

	s = splnet();
	retval = fwohci_handler_set(sc, IEEE1394_TCODE_STREAM_DATA,
	    channel, tag, fwohci_if_input_iso, handler);
	splx(s);

	if (!retval) {
		printf("%s: dummy iso handler set\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
	} else {
		printf("%s: dummy iso handler cannot set\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
	}

	return retval;
}

int
fwohci_intr(void *arg)
{
	struct fwohci_softc * const sc = arg;
	int progress = 0;
	u_int32_t intmask, iso;

	for (;;) {
		intmask = OHCI_CSR_READ(sc, OHCI_REG_IntEventClear);

		/*
		 * On a bus reset, everything except bus reset gets
		 * cleared.  That can't get cleared until the selfid
		 * phase completes (which happens outside the
		 * interrupt routines). So if just a bus reset is left
		 * in the mask and it's already in the sc_intmask,
		 * just return.
		 */

		if ((intmask == 0) ||
		    (progress && (intmask == OHCI_Int_BusReset) &&
			(sc->sc_intmask & OHCI_Int_BusReset))) {
			if (progress)
				wakeup(fwohci_event_thread);
			return progress;
		}
		OHCI_CSR_WRITE(sc, OHCI_REG_IntEventClear,
		    intmask & ~OHCI_Int_BusReset);
#ifdef FW_DEBUG
		if (fwdebug > 1)
			fwohci_show_intr(sc, intmask);
#endif

		if (intmask & OHCI_Int_BusReset) {
			/*
			 * According to OHCI spec 6.1.1 "busReset",
			 * All asynchronous transmit must be stopped before
			 * clearing BusReset.  Moreover, the BusReset
			 * interrupt bit should not be cleared during the
			 * SelfID phase.  Thus we turned off interrupt mask
			 * bit of BusReset instead until SelfID completion
			 * or SelfID timeout.
			 */
			intmask &= OHCI_Int_SelfIDComplete;
			OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskClear,
			    OHCI_Int_BusReset);
			sc->sc_intmask = OHCI_Int_BusReset;
		}
		sc->sc_intmask |= intmask;

		if (intmask & OHCI_Int_IsochTx) {
			iso = OHCI_CSR_READ(sc, OHCI_REG_IsoXmitIntEventClear);
			OHCI_CSR_WRITE(sc, OHCI_REG_IsoXmitIntEventClear, iso);
		}
		if (intmask & OHCI_Int_IsochRx) {
#if NO_THREAD
			int i;
			int asyncstream = 0;
#endif

			iso = OHCI_CSR_READ(sc, OHCI_REG_IsoRecvIntEventClear);
			OHCI_CSR_WRITE(sc, OHCI_REG_IsoRecvIntEventClear, iso);
#if NO_THREAD
			for (i = 0; i < sc->sc_isoctx; i++) {
				if ((iso & (1<<i)) && sc->sc_ctx_ir[i] != NULL) {
					if (sc->sc_ctx_ir[i]->fc_type == FWOHCI_CTX_ISO_SINGLE) {
						asyncstream |= (1 << i);
						continue;
					}
					bus_dmamap_sync(sc->sc_dmat,
					    sc->sc_ddmamap,
					    0, sizeof(struct fwohci_desc) * sc->sc_descsize,
					    BUS_DMASYNC_PREREAD);
					sc->sc_isocnt.ev_count++;

					fwohci_ir_input(sc, sc->sc_ctx_ir[i]);
				}
			}
			if (asyncstream != 0) {
				sc->sc_iso |= asyncstream;
			} else {
				/* all iso intr is pure isochronous */
				sc->sc_intmask &= ~OHCI_Int_IsochRx;
			}
#else
			sc->sc_iso |= iso;
#endif /* NO_THREAD */
		}

		if (!progress) {
			sc->sc_intrcnt.ev_count++;
			progress = 1;
		}
	}
}

static void
fwohci_create_event_thread(void *arg)
{
	struct fwohci_softc  *sc = arg;

	if (kthread_create1(fwohci_thread_init, sc, &sc->sc_event_thread, "%s",
	    sc->sc_sc1394.sc1394_dev.dv_xname)) {
		printf("%s: unable to create event thread\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
		panic("fwohci_create_event_thread");
	}
}

static void
fwohci_thread_init(void *arg)
{
	struct fwohci_softc *sc = arg;
	int i;

	/*
	 * Allocate descriptors
	 */
	if (fwohci_desc_alloc(sc)) {
		printf("%s: not enabling interrupts\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
		kthread_exit(1);
	}

	/*
	 * Enable Link Power
	 */

	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet, OHCI_HCControl_LPS);

	/*
	 * Allocate DMA Context
	 */
	fwohci_ctx_alloc(sc, &sc->sc_ctx_arrq, OHCI_BUF_ARRQ_CNT,
	    OHCI_CTX_ASYNC_RX_REQUEST, FWOHCI_CTX_ASYNC);
	fwohci_ctx_alloc(sc, &sc->sc_ctx_arrs, OHCI_BUF_ARRS_CNT,
	    OHCI_CTX_ASYNC_RX_RESPONSE, FWOHCI_CTX_ASYNC);
	fwohci_ctx_alloc(sc, &sc->sc_ctx_atrq, 0, OHCI_CTX_ASYNC_TX_REQUEST,
	    FWOHCI_CTX_ASYNC);
	fwohci_ctx_alloc(sc, &sc->sc_ctx_atrs, 0, OHCI_CTX_ASYNC_TX_RESPONSE,
	    FWOHCI_CTX_ASYNC);
	sc->sc_ctx_ir = malloc(sizeof(sc->sc_ctx_ir[0]) * sc->sc_isoctx,
	    M_DEVBUF, M_WAITOK);
	for (i = 0; i < sc->sc_isoctx; i++)
		sc->sc_ctx_ir[i] = NULL;

	/*
	 * Allocate buffer for configuration ROM and SelfID buffer
	 */
	fwohci_buf_alloc(sc, &sc->sc_buf_cnfrom);
	fwohci_buf_alloc(sc, &sc->sc_buf_selfid);

	callout_init(&sc->sc_selfid_callout);

	sc->sc_sc1394.sc1394_ifinreg = fwohci_if_inreg;
	sc->sc_sc1394.sc1394_ifoutput = fwohci_if_output;
	sc->sc_sc1394.sc1394_ifsetiso = fwohci_if_setiso;

	/*
	 * establish hooks for shutdown and suspend/resume 
	 */
	sc->sc_shutdownhook = shutdownhook_establish(fwohci_shutdown, sc);
	sc->sc_powerhook = powerhook_establish(fwohci_power, sc);

	sc->sc_sc1394.sc1394_if = config_found(&sc->sc_sc1394.sc1394_dev, "fw",
	    fwohci_print);

	/* Main loop. It's not coming back normally. */

	fwohci_event_thread(sc);

	kthread_exit(0);
}

static void
fwohci_event_thread(struct fwohci_softc *sc)
{
	int i, s;
	u_int32_t intmask, iso;

	s = splbio();

	/*
	 * Initialize hardware registers.
	 */

	fwohci_hw_init(sc);

	/* Initial Bus Reset */
	fwohci_phy_busreset(sc);
	splx(s);

	while (!sc->sc_dying) {
		s = splbio();
		intmask = sc->sc_intmask;
		if (intmask == 0) {
			tsleep(fwohci_event_thread, PZERO, "fwohciev", 0);
			splx(s);
			continue;
		}
		sc->sc_intmask = 0;
		splx(s);

		if (intmask & OHCI_Int_BusReset) {
			fwohci_buf_stop_tx(sc);
			if (sc->sc_uidtbl != NULL) {
				free(sc->sc_uidtbl, M_DEVBUF);
				sc->sc_uidtbl = NULL;
			}

			callout_reset(&sc->sc_selfid_callout,
			    OHCI_SELFID_TIMEOUT,
			    (void (*)(void *))fwohci_phy_busreset, sc);
			sc->sc_nodeid = 0xffff;	/* indicate invalid */
			sc->sc_rootid = 0;
			sc->sc_irmid = IEEE1394_BCAST_PHY_ID;
		}
		if (intmask & OHCI_Int_SelfIDComplete) {
			s = splbio();
			OHCI_CSR_WRITE(sc, OHCI_REG_IntEventClear,
			    OHCI_Int_BusReset);
			OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet,
			    OHCI_Int_BusReset);
			splx(s);
			callout_stop(&sc->sc_selfid_callout);
			if (fwohci_selfid_input(sc) == 0) {
				fwohci_buf_start_rx(sc);
				fwohci_uid_collect(sc);
			}
		}
		if (intmask & OHCI_Int_ReqTxComplete)
			fwohci_at_done(sc, sc->sc_ctx_atrq, 0);
		if (intmask & OHCI_Int_RespTxComplete)
			fwohci_at_done(sc, sc->sc_ctx_atrs, 0);
		if (intmask & OHCI_Int_RQPkt)
			fwohci_arrq_input(sc, sc->sc_ctx_arrq);
		if (intmask & OHCI_Int_RSPkt)
			fwohci_arrs_input(sc, sc->sc_ctx_arrs);
		if (intmask & OHCI_Int_IsochRx) {
			s = splbio();
			iso = sc->sc_iso;
			sc->sc_iso = 0;
			splx(s);
			for (i = 0; i < sc->sc_isoctx; i++) {
				if ((iso & (1 << i)) &&
				    sc->sc_ctx_ir[i] != NULL) {
					fwohci_ir_input(sc, sc->sc_ctx_ir[i]);
					sc->sc_isocnt.ev_count++;
				}
			}
		}
	}
}

#if 0
static int
fwohci_dnamem_alloc(struct fwohci_softc *sc, int size, int alignment,
    bus_dmamap_t *mapp, caddr_t *kvap, int flags)
{
	bus_dma_segment_t segs[1];
	int error, nsegs, steps;

	steps = 0;
	error = bus_dmamem_alloc(sc->sc_dmat, size, alignment, alignment,
	    segs, 1, &nsegs, flags);
	if (error)
		goto cleanup;

	steps = 1;
	error = bus_dmamem_map(sc->sc_dmat, segs, nsegs, segs[0].ds_len,
	    kvap, flags);
	if (error)
		goto cleanup;

	if (error == 0)
		error = bus_dmamap_create(sc->sc_dmat, size, 1, alignment,
		    size, flags, mapp);
	if (error)
		goto cleanup;
	if (error == 0)
		error = bus_dmamap_load(sc->sc_dmat, *mapp, *kvap, size, NULL,
		    flags);
	if (error)
		goto cleanup;

 cleanup:
	switch (steps) {
	case 1:
		bus_dmamem_free(sc->sc_dmat, segs, nsegs);
	}

	return error;
}
#endif

int
fwohci_print(void *aux, const char *pnp)
{
	char *name = aux;

	if (pnp)
		printf("%s at %s", name, pnp);

	return QUIET;
}

static void
fwohci_hw_init(struct fwohci_softc *sc)
{
	int i;
	u_int32_t val;

	/*
	 * Software Reset.
	 */
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet, OHCI_HCControl_SoftReset);
	for (i = 0; i < OHCI_LOOP; i++) {
		val = OHCI_CSR_READ(sc, OHCI_REG_HCControlClear);
		if ((val & OHCI_HCControl_SoftReset) == 0)
			break;
		DELAY(10);
	}

	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet, OHCI_HCControl_LPS);

	/*
	 * First, initilize CSRs with undefined value to default settings.
	 */
	val = OHCI_CSR_READ(sc, OHCI_REG_BusOptions);
	val |= OHCI_BusOptions_ISC | OHCI_BusOptions_CMC;
#if 0
	val |= OHCI_BusOptions_BMC | OHCI_BusOptions_IRMC;
#else
	val &= ~(OHCI_BusOptions_BMC | OHCI_BusOptions_IRMC);
#endif
	OHCI_CSR_WRITE(sc, OHCI_REG_BusOptions, val);
	for (i = 0; i < sc->sc_isoctx; i++) {
		OHCI_SYNC_RX_DMA_WRITE(sc, i, OHCI_SUBREG_ContextControlClear,
		    ~0);
	}
	OHCI_CSR_WRITE(sc, OHCI_REG_LinkControlClear, ~0);

	fwohci_configrom_init(sc);
	fwohci_selfid_init(sc);
	fwohci_buf_init_rx(sc);
	fwohci_csr_init(sc);

	/*
	 * Final CSR settings.
	 */
	OHCI_CSR_WRITE(sc, OHCI_REG_LinkControlSet,
	    OHCI_LinkControl_CycleTimerEnable |
	    OHCI_LinkControl_RcvSelfID | OHCI_LinkControl_RcvPhyPkt);

	OHCI_CSR_WRITE(sc, OHCI_REG_ATRetries, 0x00000888);	/*XXX*/

	/* clear receive filter */
	OHCI_CSR_WRITE(sc, OHCI_REG_IRMultiChanMaskHiClear, ~0);
	OHCI_CSR_WRITE(sc, OHCI_REG_IRMultiChanMaskLoClear, ~0);
	OHCI_CSR_WRITE(sc, OHCI_REG_AsynchronousRequestFilterHiSet, 0x80000000);

	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlClear,
	    OHCI_HCControl_NoByteSwapData | OHCI_HCControl_APhyEnhanceEnable);
#if BYTE_ORDER == BIG_ENDIAN
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet,
	    OHCI_HCControl_NoByteSwapData);
#endif

	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskClear, ~0);
	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet, OHCI_Int_BusReset |
	    OHCI_Int_SelfIDComplete | OHCI_Int_IsochRx | OHCI_Int_IsochTx |
	    OHCI_Int_RSPkt | OHCI_Int_RQPkt | OHCI_Int_ARRS | OHCI_Int_ARRQ |
	    OHCI_Int_RespTxComplete | OHCI_Int_ReqTxComplete);
	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet, OHCI_Int_CycleTooLong |
	    OHCI_Int_UnrecoverableError | OHCI_Int_CycleInconsistent |
	    OHCI_Int_LockRespErr | OHCI_Int_PostedWriteErr);
	OHCI_CSR_WRITE(sc, OHCI_REG_IsoXmitIntMaskSet, ~0);
	OHCI_CSR_WRITE(sc, OHCI_REG_IsoRecvIntMaskSet, ~0);
	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet, OHCI_Int_MasterEnable);

	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet, OHCI_HCControl_LinkEnable);

	/*
	 * Start the receivers
	 */
	fwohci_buf_start_rx(sc);
}

static void
fwohci_power(int why, void *arg)
{
	struct fwohci_softc *sc = arg;
	int s;

	s = splbio();
	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		fwohci_shutdown(sc);
		break;
	case PWR_RESUME:
		fwohci_hw_init(sc);
		fwohci_phy_busreset(sc);
		break;
	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
	}
	splx(s);
}

static void
fwohci_shutdown(void *arg)
{
	struct fwohci_softc *sc = arg;
	u_int32_t val;

	callout_stop(&sc->sc_selfid_callout);
	/* disable all interrupt */
	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskClear, OHCI_Int_MasterEnable);
	fwohci_buf_stop_tx(sc);
	fwohci_buf_stop_rx(sc);
	val = OHCI_CSR_READ(sc, OHCI_REG_BusOptions);
	val &= ~(OHCI_BusOptions_BMC | OHCI_BusOptions_ISC |
		OHCI_BusOptions_CMC | OHCI_BusOptions_IRMC);
	OHCI_CSR_WRITE(sc, OHCI_REG_BusOptions, val);
	fwohci_phy_busreset(sc);
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlClear, OHCI_HCControl_LinkEnable);
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlClear, OHCI_HCControl_LPS);
	OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet, OHCI_HCControl_SoftReset);
}

/*
 * COMMON FUNCTIONS
 */

/*
 * read the PHY Register.
 */
static u_int8_t
fwohci_phy_read(struct fwohci_softc *sc, u_int8_t reg)
{
	int i;
	u_int32_t val;

	OHCI_CSR_WRITE(sc, OHCI_REG_PhyControl,
	    OHCI_PhyControl_RdReg | (reg << OHCI_PhyControl_RegAddr_BITPOS));
	for (i = 0; i < OHCI_LOOP; i++) {
		if (OHCI_CSR_READ(sc, OHCI_REG_PhyControl) &
		    OHCI_PhyControl_RdDone)
			break;
		DELAY(10);
	}
	val = OHCI_CSR_READ(sc, OHCI_REG_PhyControl);
	return (val & OHCI_PhyControl_RdData) >> OHCI_PhyControl_RdData_BITPOS;
}

/*
 * write the PHY Register.
 */
static void
fwohci_phy_write(struct fwohci_softc *sc, u_int8_t reg, u_int8_t val)
{
	int i;

	OHCI_CSR_WRITE(sc, OHCI_REG_PhyControl, OHCI_PhyControl_WrReg |
	    (reg << OHCI_PhyControl_RegAddr_BITPOS) |
	    (val << OHCI_PhyControl_WrData_BITPOS));
	for (i = 0; i < OHCI_LOOP; i++) {
		if (!(OHCI_CSR_READ(sc, OHCI_REG_PhyControl) &
		    OHCI_PhyControl_WrReg))
			break;
		DELAY(10);
	}
}

/*
 * Initiate Bus Reset
 */
static void
fwohci_phy_busreset(struct fwohci_softc *sc)
{
	int s;
	u_int8_t val;

	s = splbio();
	OHCI_CSR_WRITE(sc, OHCI_REG_IntEventClear,
	    OHCI_Int_BusReset | OHCI_Int_SelfIDComplete);
	OHCI_CSR_WRITE(sc, OHCI_REG_IntMaskSet, OHCI_Int_BusReset);
	callout_stop(&sc->sc_selfid_callout);
	val = fwohci_phy_read(sc, 1);
	val = (val & 0x80) |			/* preserve RHB (force root) */
	    0x40 |				/* Initiate Bus Reset */
	    0x3f;				/* default GAP count */
	fwohci_phy_write(sc, 1, val);
	splx(s);
}

/*
 * PHY Packet
 */
static void
fwohci_phy_input(struct fwohci_softc *sc, struct fwohci_pkt *pkt)
{
	u_int32_t val;

	val = pkt->fp_hdr[1];
	if (val != ~pkt->fp_hdr[2]) {
		if (val == 0 && ((*pkt->fp_trail & 0x001f0000) >> 16) ==
		    OHCI_CTXCTL_EVENT_BUS_RESET) {
			DPRINTFN(1, ("fwohci_phy_input: BusReset: 0x%08x\n",
			    pkt->fp_hdr[2]));
		} else {
			printf("%s: phy packet corrupted (0x%08x, 0x%08x)\n",
			    sc->sc_sc1394.sc1394_dev.dv_xname, val,
			    pkt->fp_hdr[2]);
		}
		return;
	}
#ifdef FW_DEBUG
	if (fwdebug > 1)
		fwohci_show_phypkt(sc, val);
#endif
}

/*
 * Descriptor for context DMA.
 */
static int
fwohci_desc_alloc(struct fwohci_softc *sc)
{
	int error, mapsize, dsize;

	/*
	 * allocate descriptor buffer
	 */

	sc->sc_descsize = OHCI_BUF_ARRQ_CNT + OHCI_BUF_ARRS_CNT +
	    OHCI_BUF_ATRQ_CNT + OHCI_BUF_ATRS_CNT +
	    OHCI_BUF_IR_CNT * sc->sc_isoctx + 2;
	dsize = sizeof(struct fwohci_desc) * sc->sc_descsize;
	mapsize = howmany(sc->sc_descsize, NBBY);
	sc->sc_descmap = malloc(mapsize, M_DEVBUF, M_WAITOK);
	memset(sc->sc_descmap, 0, mapsize);

	if ((error = bus_dmamem_alloc(sc->sc_dmat, dsize, PAGE_SIZE, 0,
	    &sc->sc_dseg, 1, &sc->sc_dnseg, 0)) != 0) {
		printf("%s: unable to allocate descriptor buffer, error = %d\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_dseg, sc->sc_dnseg,
	    dsize, (caddr_t *)&sc->sc_desc, BUS_DMA_COHERENT | BUS_DMA_WAITOK))
	    != 0) {
		printf("%s: unable to map descriptor buffer, error = %d\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, dsize, sc->sc_dnseg,
	    dsize, 0, BUS_DMA_WAITOK, &sc->sc_ddmamap)) != 0) {
		printf("%s: unable to create descriptor buffer DMA map, "
		    "error = %d\n", sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_ddmamap, sc->sc_desc,
	    dsize, NULL, BUS_DMA_WAITOK)) != 0) {
		printf("%s: unable to load descriptor buffer DMA map, "
		    "error = %d\n", sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_3;
	}

	return 0;

  fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_ddmamap);
  fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_desc, dsize);
  fail_1:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_dseg, sc->sc_dnseg);
  fail_0:
	return error;
}

static struct fwohci_desc *
fwohci_desc_get(struct fwohci_softc *sc, int ndesc)
{
	int i, n;

	for (n = 0; n <= sc->sc_descsize - ndesc; n++) {
		for (i = 0; ; i++) {
			if (i == ndesc) {
				for (i = 0; i < ndesc; i++)
					setbit(sc->sc_descmap, n + i);
				return sc->sc_desc + n;
			}
			if (isset(sc->sc_descmap, n + i))
				break;
		}
	}
	return NULL;
}

static void
fwohci_desc_put(struct fwohci_softc *sc, struct fwohci_desc *fd, int ndesc)
{
	int i, n;

	n = fd - sc->sc_desc;
	for (i = 0; i < ndesc; i++, n++) {
#ifdef DIAGNOSTIC
		if (isclr(sc->sc_descmap, n))
			panic("fwohci_desc_put: duplicated free");
#endif
		clrbit(sc->sc_descmap, n);
	}
}

/*
 * Asyncronous/Isochronous Transmit/Receive Context
 */
static int
fwohci_ctx_alloc(struct fwohci_softc *sc, struct fwohci_ctx **fcp,
    int bufcnt, int ctx, int ctxtype)
{
	int i, error;
	struct fwohci_ctx *fc;
	struct fwohci_buf *fb;
	struct fwohci_desc *fd;
	int buf2cnt;

	fc = malloc(sizeof(*fc) + sizeof(*fb) * bufcnt, M_DEVBUF, M_WAITOK);
	memset(fc, 0, sizeof(*fc) + sizeof(*fb) * bufcnt);
	LIST_INIT(&fc->fc_handler);
	TAILQ_INIT(&fc->fc_buf);
	fc->fc_ctx = ctx;
	fc->fc_bufcnt = bufcnt;
	fb = (struct fwohci_buf *)&fc[1];
#if DOUBLEBUF
	TAILQ_INIT(&fc->fc_buf2); /* for isochronous */
	if (ctxtype == FWOHCI_CTX_ISO_MULTI) {
		buf2cnt = bufcnt/2;
		bufcnt -= buf2cnt;
		if (buf2cnt == 0) {
			panic("cannot allocate iso buffer");
		}
	}
#endif
	for (i = 0; i < bufcnt; i++, fb++) {
		if ((error = fwohci_buf_alloc(sc, fb)) != 0)
			goto fail;
		if ((fd = fwohci_desc_get(sc, 1)) == NULL) {
			error = ENOBUFS;
			goto fail;
		}
		fb->fb_desc = fd;
		fb->fb_daddr = sc->sc_ddmamap->dm_segs[0].ds_addr +
		    ((caddr_t)fd - (caddr_t)sc->sc_desc);
		fd->fd_flags = OHCI_DESC_INPUT | OHCI_DESC_STATUS |
		    OHCI_DESC_INTR_ALWAYS | OHCI_DESC_BRANCH;
		fd->fd_reqcount = fb->fb_dmamap->dm_segs[0].ds_len;
		fd->fd_data = fb->fb_dmamap->dm_segs[0].ds_addr;
		TAILQ_INSERT_TAIL(&fc->fc_buf, fb, fb_list);
	}
#if DOUBLEBUF
	if (ctxtype == FWOHCI_CTX_ISO_MULTI) {
		for (i = bufcnt; i < bufcnt + buf2cnt; i++, fb++) {
			if ((error = fwohci_buf_alloc(sc, fb)) != 0)
				goto fail;
			if ((fd = fwohci_desc_get(sc, 1)) == NULL) {
				error = ENOBUFS;
				goto fail;
			}
			fb->fb_desc = fd;
			fb->fb_daddr = sc->sc_ddmamap->dm_segs[0].ds_addr +
			    ((caddr_t)fd - (caddr_t)sc->sc_desc);
			bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
			    (caddr_t)fd - (caddr_t)sc->sc_desc, sizeof(struct fwohci_desc),
			    BUS_DMASYNC_PREWRITE);
			fd->fd_flags = OHCI_DESC_INPUT | OHCI_DESC_STATUS |
			    OHCI_DESC_INTR_ALWAYS | OHCI_DESC_BRANCH;
			fd->fd_reqcount = fb->fb_dmamap->dm_segs[0].ds_len;
			fd->fd_data = fb->fb_dmamap->dm_segs[0].ds_addr;
			TAILQ_INSERT_TAIL(&fc->fc_buf2, fb, fb_list);
			bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
			    (caddr_t)fd - (caddr_t)sc->sc_desc, sizeof(struct fwohci_desc),
			    BUS_DMASYNC_POSTWRITE);
		}
	}
#endif /* DOUBLEBUF */
	fc->fc_type = ctxtype;
	*fcp = fc;
	return 0;

  fail:
	while (i-- > 0) {
		fb--;
		if (fb->fb_desc)
			fwohci_desc_put(sc, fb->fb_desc, 1);
		fwohci_buf_free(sc, fb);
	}
	free(fc, M_DEVBUF);
	return error;
}

static void
fwohci_ctx_free(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	struct fwohci_buf *fb;
	struct fwohci_handler *fh;

	while ((fh = LIST_FIRST(&fc->fc_handler)) != NULL)
		fwohci_handler_set(sc, fh->fh_tcode, fh->fh_key1, fh->fh_key2,
		    NULL, NULL);
	while ((fb = TAILQ_FIRST(&fc->fc_buf)) != NULL) {
		TAILQ_REMOVE(&fc->fc_buf, fb, fb_list);
		if (fb->fb_desc)
			fwohci_desc_put(sc, fb->fb_desc, 1);
		fwohci_buf_free(sc, fb);
	}
#if DOUBLEBUF
	while ((fb = TAILQ_FIRST(&fc->fc_buf2)) != NULL) {
		TAILQ_REMOVE(&fc->fc_buf2, fb, fb_list);
		if (fb->fb_desc)
			fwohci_desc_put(sc, fb->fb_desc, 1);
		fwohci_buf_free(sc, fb);
	}
#endif /* DOUBLEBUF */
	free(fc, M_DEVBUF);
}

static void
fwohci_ctx_init(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	struct fwohci_buf *fb, *nfb;
	struct fwohci_desc *fd;
	struct fwohci_handler *fh;
	int n;

	for (fb = TAILQ_FIRST(&fc->fc_buf); fb != NULL; fb = nfb) {
		nfb = TAILQ_NEXT(fb, fb_list);
		fb->fb_off = 0;
		fd = fb->fb_desc;
		fd->fd_branch = (nfb != NULL) ? (nfb->fb_daddr | 1) : 0;
		fd->fd_rescount = fd->fd_reqcount;
	}

#if DOUBLEBUF
	for (fb = TAILQ_FIRST(&fc->fc_buf2); fb != NULL; fb = nfb) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
		    (caddr_t)fd - (caddr_t)sc->sc_desc, sizeof(struct fwohci_desc),
		    BUS_DMASYNC_PREWRITE);
		nfb = TAILQ_NEXT(fb, fb_list);
		fb->fb_off = 0;
		fd = fb->fb_desc;
		fd->fd_branch = (nfb != NULL) ? (nfb->fb_daddr | 1) : 0;
		fd->fd_rescount = fd->fd_reqcount;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_ddmamap,
		    (caddr_t)fd - (caddr_t)sc->sc_desc, sizeof(struct fwohci_desc),
		    BUS_DMASYNC_POSTWRITE);
	}
#endif /* DOUBLEBUF */

	n = fc->fc_ctx;
	fb = TAILQ_FIRST(&fc->fc_buf);
	if (fc->fc_type != FWOHCI_CTX_ASYNC) {
		OHCI_SYNC_RX_DMA_WRITE(sc, n, OHCI_SUBREG_CommandPtr,
		    fb->fb_daddr | 1);
		OHCI_SYNC_RX_DMA_WRITE(sc, n, OHCI_SUBREG_ContextControlClear,
		    OHCI_CTXCTL_RX_BUFFER_FILL |
		    OHCI_CTXCTL_RX_CYCLE_MATCH_ENABLE |
		    OHCI_CTXCTL_RX_MULTI_CHAN_MODE |
		    OHCI_CTXCTL_RX_DUAL_BUFFER_MODE);
		OHCI_SYNC_RX_DMA_WRITE(sc, n, OHCI_SUBREG_ContextControlSet,
		    OHCI_CTXCTL_RX_ISOCH_HEADER);
		if (fc->fc_type == FWOHCI_CTX_ISO_MULTI) {
			OHCI_SYNC_RX_DMA_WRITE(sc, n,
			    OHCI_SUBREG_ContextControlSet,
			    OHCI_CTXCTL_RX_BUFFER_FILL);
		}
		fh = LIST_FIRST(&fc->fc_handler);
		OHCI_SYNC_RX_DMA_WRITE(sc, n, OHCI_SUBREG_ContextMatch,
		    (OHCI_CTXMATCH_TAG0 << fh->fh_key2) | fh->fh_key1);
	} else {
		OHCI_ASYNC_DMA_WRITE(sc, n, OHCI_SUBREG_CommandPtr,
		    fb->fb_daddr | 1);
	}
}

/*
 * DMA data buffer
 */
static int
fwohci_buf_alloc(struct fwohci_softc *sc, struct fwohci_buf *fb)
{
	int error;

	if ((error = bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE,
	    PAGE_SIZE, &fb->fb_seg, 1, &fb->fb_nseg, BUS_DMA_WAITOK)) != 0) {
		printf("%s: unable to allocate buffer, error = %d\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &fb->fb_seg,
	    fb->fb_nseg, PAGE_SIZE, &fb->fb_buf, BUS_DMA_WAITOK)) != 0) {
		printf("%s: unable to map buffer, error = %d\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, fb->fb_nseg,
	    PAGE_SIZE, 0, BUS_DMA_WAITOK, &fb->fb_dmamap)) != 0) {
		printf("%s: unable to create buffer DMA map, "
		    "error = %d\n", sc->sc_sc1394.sc1394_dev.dv_xname,
		    error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, fb->fb_dmamap,
	    fb->fb_buf, PAGE_SIZE, NULL, BUS_DMA_WAITOK)) != 0) {
		printf("%s: unable to load buffer DMA map, "
		    "error = %d\n", sc->sc_sc1394.sc1394_dev.dv_xname,
		    error);
		goto fail_3;
	}

	return 0;

	bus_dmamap_unload(sc->sc_dmat, fb->fb_dmamap);
  fail_3:
	bus_dmamap_destroy(sc->sc_dmat, fb->fb_dmamap);
  fail_2:
	bus_dmamem_unmap(sc->sc_dmat, fb->fb_buf, PAGE_SIZE);
  fail_1:
	bus_dmamem_free(sc->sc_dmat, &fb->fb_seg, fb->fb_nseg);
  fail_0:
	return error;
}

static void
fwohci_buf_free(struct fwohci_softc *sc, struct fwohci_buf *fb)
{

	bus_dmamap_unload(sc->sc_dmat, fb->fb_dmamap);
	bus_dmamap_destroy(sc->sc_dmat, fb->fb_dmamap);
	bus_dmamem_unmap(sc->sc_dmat, fb->fb_buf, PAGE_SIZE);
	bus_dmamem_free(sc->sc_dmat, &fb->fb_seg, fb->fb_nseg);
}

static void
fwohci_buf_init_rx(struct fwohci_softc *sc)
{
	int i;

	/*
	 * Initialize for Asynchronous Receive Queue.
	 */
	fwohci_ctx_init(sc, sc->sc_ctx_arrq);
	fwohci_ctx_init(sc, sc->sc_ctx_arrs);

	/*
	 * Initialize for Isochronous Receive Queue.
	 */
	for (i = 0; i < sc->sc_isoctx; i++) {
		if (sc->sc_ctx_ir[i] != NULL)
			fwohci_ctx_init(sc, sc->sc_ctx_ir[i]);
	}
}

static void
fwohci_buf_start_rx(struct fwohci_softc *sc)
{
	int i;

	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_RX_REQUEST,
	    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_RX_RESPONSE,
	    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
	for (i = 0; i < sc->sc_isoctx; i++) {
		if (sc->sc_ctx_ir[i] != NULL)
			OHCI_SYNC_RX_DMA_WRITE(sc, i,
			    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
	}
}

static void
fwohci_buf_stop_tx(struct fwohci_softc *sc)
{
	int i;

	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_TX_REQUEST,
	    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_TX_RESPONSE,
	    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);

	/*
	 * Make sure the transmitter is stopped.
	 */
	for (i = 0; i < OHCI_LOOP; i++) {
		DELAY(10);
		if (OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_TX_REQUEST,
		    OHCI_SUBREG_ContextControlClear) & OHCI_CTXCTL_ACTIVE)
			continue;
		if (OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_TX_RESPONSE,
		    OHCI_SUBREG_ContextControlClear) & OHCI_CTXCTL_ACTIVE)
			continue;
		break;
	}

	/*
	 * Initialize for Asynchronous Transmit Queue.
	 */
	fwohci_at_done(sc, sc->sc_ctx_atrq, 1);
	fwohci_at_done(sc, sc->sc_ctx_atrs, 1);
}

static void
fwohci_buf_stop_rx(struct fwohci_softc *sc)
{
	int i;

	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_RX_REQUEST,
	    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
	OHCI_ASYNC_DMA_WRITE(sc, OHCI_CTX_ASYNC_RX_RESPONSE,
	    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
	for (i = 0; i < sc->sc_isoctx; i++) {
		OHCI_SYNC_RX_DMA_WRITE(sc, i,
		    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
	}
}

static void
fwohci_buf_next(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	struct fwohci_buf *fb, *tfb;

#if DOUBLEBUF
	if (fc->fc_type != FWOHCI_CTX_ISO_MULTI) {
#endif
		while ((fb = TAILQ_FIRST(&fc->fc_buf)) != NULL) {
			if (fc->fc_type) {
				if (fb->fb_off == 0)
					break;
			} else {
				if (fb->fb_off != fb->fb_desc->fd_reqcount ||
				    fb->fb_desc->fd_rescount != 0)
					break;
			}
			TAILQ_REMOVE(&fc->fc_buf, fb, fb_list);
			fb->fb_desc->fd_rescount = fb->fb_desc->fd_reqcount;
			fb->fb_off = 0;
			fb->fb_desc->fd_branch = 0;
			tfb = TAILQ_LAST(&fc->fc_buf, fwohci_buf_s);
			tfb->fb_desc->fd_branch = fb->fb_daddr | 1;
			TAILQ_INSERT_TAIL(&fc->fc_buf, fb, fb_list);
		}
#if DOUBLEBUF
	} else {
		struct fwohci_buf_s fctmp;

		/* cleaning buffer */
		for (fb = TAILQ_FIRST(&fc->fc_buf); fb != NULL;
		     fb = TAILQ_NEXT(fb, fb_list)) {
			fb->fb_off = 0;
			fb->fb_desc->fd_rescount = fb->fb_desc->fd_reqcount;
		}
	
		/* rotating buffer */
		fctmp = fc->fc_buf;
		fc->fc_buf = fc->fc_buf2;
		fc->fc_buf2 = fctmp;
	}
#endif
}

static int
fwohci_buf_pktget(struct fwohci_softc *sc, struct fwohci_buf **fbp, caddr_t *pp,
    int len)
{
	struct fwohci_buf *fb;
	struct fwohci_desc *fd;
	int bufend;

	fb = *fbp;
  again:
	fd = fb->fb_desc;
	DPRINTFN(1, ("fwohci_buf_pktget: desc %ld, off %d, req %d, res %d,"
	    " len %d, avail %d\n", (long)(fd - sc->sc_desc), fb->fb_off,
	    fd->fd_reqcount, fd->fd_rescount, len,
	    fd->fd_reqcount - fd->fd_rescount - fb->fb_off));
	bufend = fd->fd_reqcount - fd->fd_rescount;
	if (fb->fb_off >= bufend) {
		DPRINTFN(5, ("buf %x finish req %d res %d off %d ",
		    fb->fb_desc->fd_data, fd->fd_reqcount, fd->fd_rescount,
		    fb->fb_off));
		if (fd->fd_rescount == 0) {
			*fbp = fb = TAILQ_NEXT(fb, fb_list);
			if (fb != NULL)
				goto again;
		}
		return 0;
	}
	if (fb->fb_off + len > bufend)
		len = bufend - fb->fb_off;
	bus_dmamap_sync(sc->sc_dmat, fb->fb_dmamap, fb->fb_off, len,
	    BUS_DMASYNC_POSTREAD);
	*pp = fb->fb_buf + fb->fb_off;
	fb->fb_off += roundup(len, 4);
	return len;
}

static int
fwohci_buf_input(struct fwohci_softc *sc, struct fwohci_ctx *fc,
    struct fwohci_pkt *pkt)
{
	caddr_t p;
	struct fwohci_buf *fb;
	int len, count, i;

	memset(pkt, 0, sizeof(*pkt));
	pkt->fp_uio.uio_iov = pkt->fp_iov;
	pkt->fp_uio.uio_rw = UIO_WRITE;
	pkt->fp_uio.uio_segflg = UIO_SYSSPACE;

	/* get first quadlet */
	fb = TAILQ_FIRST(&fc->fc_buf);
	count = 4;
	len = fwohci_buf_pktget(sc, &fb, &p, count);
	if (len <= 0) {
		DPRINTFN(1, ("fwohci_buf_input: no input for %d\n",
		    fc->fc_ctx));
		return 0;
	}
	pkt->fp_hdr[0] = *(u_int32_t *)p;
	pkt->fp_tcode = (pkt->fp_hdr[0] & 0x000000f0) >> 4;
	switch (pkt->fp_tcode) {
	case IEEE1394_TCODE_WRITE_REQ_QUAD:
	case IEEE1394_TCODE_READ_RESP_QUAD:
		pkt->fp_hlen = 12;
		pkt->fp_dlen = 4;
		break;
	case IEEE1394_TCODE_READ_REQ_BLOCK:
		pkt->fp_hlen = 16;
		pkt->fp_dlen = 0;
		break;
	case IEEE1394_TCODE_WRITE_REQ_BLOCK:
	case IEEE1394_TCODE_READ_RESP_BLOCK:
	case IEEE1394_TCODE_LOCK_REQ:
	case IEEE1394_TCODE_LOCK_RESP:
		pkt->fp_hlen = 16;
		break;
	case IEEE1394_TCODE_STREAM_DATA:
#ifdef DIAGNOSTIC
		if (fc->fc_type == FWOHCI_CTX_ISO_MULTI)
#endif
		{
			pkt->fp_hlen = 4;
			pkt->fp_dlen = pkt->fp_hdr[0] >> 16;
			DPRINTFN(5, ("[%d]", pkt->fp_dlen));
			break;
		}
#ifdef DIAGNOSTIC
		else {
			printf("fwohci_buf_input: bad tcode: STREAM_DATA\n");
			return 0;
		}
#endif
	default:
		pkt->fp_hlen = 12;
		pkt->fp_dlen = 0;
		break;
	}

	/* get header */
	while (count < pkt->fp_hlen) {
		len = fwohci_buf_pktget(sc, &fb, &p, pkt->fp_hlen - count);
		if (len == 0) {
			printf("fwohci_buf_input: malformed input 1: %d\n",
			    pkt->fp_hlen - count);
			return 0;
		}
		memcpy((caddr_t)pkt->fp_hdr + count, p, len);
		count += len;
	}
	if (pkt->fp_hlen == 16 &&
	    pkt->fp_tcode != IEEE1394_TCODE_READ_REQ_BLOCK)
		pkt->fp_dlen = pkt->fp_hdr[3] >> 16;
	DPRINTFN(1, ("fwohci_buf_input: tcode=0x%x, hlen=%d, dlen=%d\n",
	    pkt->fp_tcode, pkt->fp_hlen, pkt->fp_dlen));

	/* get data */
	count = 0;
	i = 0;
	while (count < pkt->fp_dlen) {
		len = fwohci_buf_pktget(sc, &fb,
		    (caddr_t *)&pkt->fp_iov[i].iov_base,
		    pkt->fp_dlen - count);
		if (len == 0) {
			printf("fwohci_buf_input: malformed input 2: %d\n",
			    pkt->fp_dlen - count);
			return 0;
		}
		pkt->fp_iov[i++].iov_len = len;
		count += len;
	}
	pkt->fp_uio.uio_iovcnt = i;
	pkt->fp_uio.uio_resid = count;

	/* get trailer */
	len = fwohci_buf_pktget(sc, &fb, (caddr_t *)&pkt->fp_trail,
	    sizeof(*pkt->fp_trail));
	if (len <= 0) {
		printf("fwohci_buf_input: malformed input 3: %d\n",
		    pkt->fp_hlen - count);
		return 0;
	}
	return 1;
}

static int
fwohci_buf_input_ppb(struct fwohci_softc *sc, struct fwohci_ctx *fc,
    struct fwohci_pkt *pkt)
{
	caddr_t p;
	int len;
	struct fwohci_buf *fb;
	struct fwohci_desc *fd;

	if (fc->fc_type ==  FWOHCI_CTX_ISO_MULTI) {
		return fwohci_buf_input(sc, fc, pkt);
	}

	memset(pkt, 0, sizeof(*pkt));
	pkt->fp_uio.uio_iov = pkt->fp_iov;
	pkt->fp_uio.uio_rw = UIO_WRITE;
	pkt->fp_uio.uio_segflg = UIO_SYSSPACE;

	for (fb = TAILQ_FIRST(&fc->fc_buf); ; fb = TAILQ_NEXT(fb, fb_list)) {
		if (fb == NULL)
			return 0;
		if (fb->fb_off == 0)
			break;
	}
	fd = fb->fb_desc;
	len = fd->fd_reqcount - fd->fd_rescount;
	if (len == 0)
		return 0;
	bus_dmamap_sync(sc->sc_dmat, fb->fb_dmamap, fb->fb_off, len,
	    BUS_DMASYNC_POSTREAD);

	p = fb->fb_buf;
	fb->fb_off += roundup(len, 4);
	if (len < 8) {
		printf("fwohci_buf_input_ppb: malformed input 1: %d\n", len);
		return 0;
	}

	/*
	 * get trailer first, may be bogus data unless status update
	 * in descriptor is set.
	 */
	pkt->fp_trail = (u_int32_t *)p;
	*pkt->fp_trail = (*pkt->fp_trail & 0xffff) | (fd->fd_status << 16);
	pkt->fp_hdr[0] = ((u_int32_t *)p)[1];
	pkt->fp_tcode = (pkt->fp_hdr[0] & 0x000000f0) >> 4;
#ifdef DIAGNOSTIC
	if (pkt->fp_tcode != IEEE1394_TCODE_STREAM_DATA) {
		printf("fwohci_buf_input_ppb: bad tcode: 0x%x\n",
		    pkt->fp_tcode);
		return 0;
	}
#endif
	pkt->fp_hlen = 4;
	pkt->fp_dlen = pkt->fp_hdr[0] >> 16;
	p += 8;
	len -= 8;
	if (pkt->fp_dlen != len) {
		printf("fwohci_buf_input_ppb: malformed input 2: %d != %d\n",
		    pkt->fp_dlen, len);
		return 0;
	}
	DPRINTFN(1, ("fwohci_buf_input_ppb: tcode=0x%x, hlen=%d, dlen=%d\n",
	    pkt->fp_tcode, pkt->fp_hlen, pkt->fp_dlen));
	pkt->fp_iov[0].iov_base = p;
	pkt->fp_iov[0].iov_len = len;
	pkt->fp_uio.uio_iovcnt = 0;
	pkt->fp_uio.uio_resid = len;
	return 1;
}

static int
fwohci_handler_set(struct fwohci_softc *sc,
    int tcode, u_int32_t key1, u_int32_t key2,
    int (*handler)(struct fwohci_softc *, void *, struct fwohci_pkt *),
    void *arg)
{
	struct fwohci_ctx *fc;
	struct fwohci_handler *fh;
	int i, j;

	if (tcode == IEEE1394_TCODE_STREAM_DATA) {
		int isasync = key1 & OHCI_ASYNC_STREAM;

		key1 &= IEEE1394_ISOCH_MASK;
		j = sc->sc_isoctx;
		fh = NULL;
		for (i = 0; i < sc->sc_isoctx; i++) {
			if ((fc = sc->sc_ctx_ir[i]) == NULL) {
				if (j == sc->sc_isoctx)
					j = i;
				continue;
			}
			fh = LIST_FIRST(&fc->fc_handler);
			if (fh->fh_tcode == tcode &&
			    fh->fh_key1 == key1 && fh->fh_key2 == key2)
				break;
			fh = NULL;
		}
		if (fh == NULL) {
			if (handler == NULL)
				return 0;
			if (j == sc->sc_isoctx) {
				DPRINTF(("fwohci_handler_set: no more free "
				    "context\n"));
				return ENOMEM;
			}
			if ((fc = sc->sc_ctx_ir[j]) == NULL) {
				fwohci_ctx_alloc(sc, &fc, OHCI_BUF_IR_CNT, j,
				    isasync ? FWOHCI_CTX_ISO_SINGLE :
				    FWOHCI_CTX_ISO_MULTI);
				sc->sc_ctx_ir[j] = fc;
			}
		}
	} else {
		switch (tcode) {
		case IEEE1394_TCODE_WRITE_REQ_QUAD:
		case IEEE1394_TCODE_WRITE_REQ_BLOCK:
		case IEEE1394_TCODE_READ_REQ_QUAD:
		case IEEE1394_TCODE_READ_REQ_BLOCK:
		case IEEE1394_TCODE_LOCK_REQ:
			fc = sc->sc_ctx_arrq;
			break;
		case IEEE1394_TCODE_WRITE_RESP:
		case IEEE1394_TCODE_READ_RESP_QUAD:
		case IEEE1394_TCODE_READ_RESP_BLOCK:
		case IEEE1394_TCODE_LOCK_RESP:
			fc = sc->sc_ctx_arrs;
			break;
		default:
			return EIO;
		}
		for (fh = LIST_FIRST(&fc->fc_handler); fh != NULL;
		    fh = LIST_NEXT(fh, fh_list)) {
			if (fh->fh_tcode == tcode &&
			    fh->fh_key1 == key1 && fh->fh_key2 == key2)
				break;
		}
	}
	if (handler == NULL) {
		if (fh != NULL) {
			LIST_REMOVE(fh, fh_list);
			free(fh, M_DEVBUF);
		}
		if (tcode == IEEE1394_TCODE_STREAM_DATA) {
			OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx,
			    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
			sc->sc_ctx_ir[fc->fc_ctx] = NULL;
			fwohci_ctx_free(sc, fc);
		}
		return 0;
	}
	if (fh == NULL) {
		fh = malloc(sizeof(*fh), M_DEVBUF, M_WAITOK);
		LIST_INSERT_HEAD(&fc->fc_handler, fh, fh_list);
	}
	fh->fh_tcode = tcode;
	fh->fh_key1 = key1;
	fh->fh_key2 = key2;
	fh->fh_handler = handler;
	fh->fh_handarg = arg;
	DPRINTFN(1, ("fwohci_handler_set: ctx %d, tcode %x, key 0x%x, 0x%x\n",
	    fc->fc_ctx, tcode, key1, key2));

	if (tcode == IEEE1394_TCODE_STREAM_DATA) {
		fwohci_ctx_init(sc, fc);
		DPRINTFN(1, ("fwohci_handler_set: SYNC desc %ld\n",
		    (long)(TAILQ_FIRST(&fc->fc_buf)->fb_desc - sc->sc_desc)));
		OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
	}
	return 0;
}

/*
 * Asyncronous Receive Requests input frontend.
 */
static void
fwohci_arrq_input(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	int rcode;
	u_int32_t key1, key2;
	struct fwohci_handler *fh;
	struct fwohci_pkt pkt, res;

	/*
	 * Do not return if next packet is in the buffer, or the next
	 * packet cannot be received until the next receive interrupt.
	 */
	while (fwohci_buf_input(sc, fc, &pkt)) {
		if (pkt.fp_tcode == OHCI_TCODE_PHY) {
			fwohci_phy_input(sc, &pkt);
			continue;
		}
		key1 = pkt.fp_hdr[1] & 0xffff;
		key2 = pkt.fp_hdr[2];
		memset(&res, 0, sizeof(res));
		res.fp_uio.uio_rw = UIO_WRITE;
		res.fp_uio.uio_segflg = UIO_SYSSPACE;
		for (fh = LIST_FIRST(&fc->fc_handler); fh != NULL;
		    fh = LIST_NEXT(fh, fh_list)) {
			if (pkt.fp_tcode == fh->fh_tcode &&
			    key1 == fh->fh_key1 &&
			    key2 == fh->fh_key2) {
				rcode = (*fh->fh_handler)(sc, fh->fh_handarg,
				    &pkt);
				break;
			}
		}
		if (fh == NULL) {
			rcode = IEEE1394_RCODE_ADDRESS_ERROR;
			DPRINTFN(1, ("fwohci_arrq_input: no listener: tcode "
			    "0x%x, addr=0x%04x %08x\n", pkt.fp_tcode, key1,
			    key2));
		}
		if (((*pkt.fp_trail & 0x001f0000) >> 16) !=
		    OHCI_CTXCTL_EVENT_ACK_PENDING)
			continue;
		if (rcode != -1) 
			fwohci_atrs_output(sc, rcode, &pkt, &res);
	}
	fwohci_buf_next(sc, fc);
	OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
	    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_WAKE);
}


/*
 * Asynchronous Receive Response input frontend.
 */
static void
fwohci_arrs_input(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	struct fwohci_pkt pkt;
	struct fwohci_handler *fh;
	u_int16_t srcid;
	int rcode, tlabel;

	while (fwohci_buf_input(sc, fc, &pkt)) {
		srcid = pkt.fp_hdr[1] >> 16;
		rcode = (pkt.fp_hdr[1] & 0x0000f000) >> 12;
		tlabel = (pkt.fp_hdr[0] & 0x0000fc00) >> 10;
		DPRINTFN(1, ("fwohci_arrs_input: tcode 0x%x, from 0x%04x,"
		    " tlabel 0x%x, rcode 0x%x, hlen %d, dlen %d\n",
		    pkt.fp_tcode, srcid, tlabel, rcode, pkt.fp_hlen,
		    pkt.fp_dlen));
		for (fh = LIST_FIRST(&fc->fc_handler); fh != NULL;
		    fh = LIST_NEXT(fh, fh_list)) {
			if (pkt.fp_tcode == fh->fh_tcode &&
			    (srcid & OHCI_NodeId_NodeNumber) == fh->fh_key1 &&
			    tlabel == fh->fh_key2) {
				(*fh->fh_handler)(sc, fh->fh_handarg, &pkt);
				LIST_REMOVE(fh, fh_list);
				free(fh, M_DEVBUF);
				break;
			}
		}
		if (fh == NULL) 
			DPRINTFN(1, ("fwohci_arrs_input: no listner\n"));
	}
	fwohci_buf_next(sc, fc);
	OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
	    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_WAKE);
}

/*
 * Isochronous Receive input frontend.
 */
static void
fwohci_ir_input(struct fwohci_softc *sc, struct fwohci_ctx *fc)
{
	int rcode, chan, tag;
	struct iovec *iov;
	struct fwohci_handler *fh;
	struct fwohci_pkt pkt;

#if DOUBLEBUF
	if (fc->fc_type == FWOHCI_CTX_ISO_MULTI) {
		struct fwohci_buf *fb;
		int i;
		u_int32_t reg;

		/* stop dma engine before read buffer */
		reg = OHCI_SYNC_RX_DMA_READ(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlClear);
		DPRINTFN(5, ("ir_input %08x =>", reg));
		if (reg & OHCI_CTXCTL_RUN) {
			OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx,
			    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
		}
		DPRINTFN(5, (" %08x\n", OHCI_SYNC_RX_DMA_READ(sc, fc->fc_ctx, OHCI_SUBREG_ContextControlClear)));

		i = 0;
		while ((reg = OHCI_SYNC_RX_DMA_READ(sc, fc->fc_ctx, OHCI_SUBREG_ContextControlSet)) & OHCI_CTXCTL_ACTIVE) {
			delay(10);
			if (++i > 10000) {
				printf("cannot stop dma engine 0x08x\n", reg);
				return;
			}
		}

		/* rotate dma buffer */
		fb = TAILQ_FIRST(&fc->fc_buf2);
		OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx, OHCI_SUBREG_CommandPtr,
		    fb->fb_daddr | 1);
		/* start dma engine */
		OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
		OHCI_CSR_WRITE(sc, OHCI_REG_IsoRecvIntEventClear,
		    (1 << fc->fc_ctx));
	}
#endif

	while (fwohci_buf_input_ppb(sc, fc, &pkt)) {
		chan = (pkt.fp_hdr[0] & 0x00003f00) >> 8;
		tag  = (pkt.fp_hdr[0] & 0x0000c000) >> 14;
		DPRINTFN(1, ("fwohci_ir_input: hdr 0x%08x, tcode %d, hlen %d, "
		    "dlen %d\n", pkt.fp_hdr[0], pkt.fp_tcode, pkt.fp_hlen,
		    pkt.fp_dlen));
		if (tag == IEEE1394_TAG_GASP) {
			/*
			 * The pkt with tag=3 is GASP format.
			 * Move GASP header to header part.
			 */
			if (pkt.fp_dlen < 8)
				continue;
			iov = pkt.fp_iov;
			/* assuming pkt per buffer mode */
			pkt.fp_hdr[1] = ntohl(((u_int32_t *)iov->iov_base)[0]);
			pkt.fp_hdr[2] = ntohl(((u_int32_t *)iov->iov_base)[1]);
			iov->iov_base = (caddr_t)iov->iov_base + 8;
			iov->iov_len -= 8;
			pkt.fp_hlen += 8;
			pkt.fp_dlen -= 8;
		}
		sc->sc_isopktcnt.ev_count++;
		for (fh = LIST_FIRST(&fc->fc_handler); fh != NULL;
		    fh = LIST_NEXT(fh, fh_list)) {
			if (pkt.fp_tcode == fh->fh_tcode &&
			    chan == fh->fh_key1 && tag == fh->fh_key2) {
				rcode = (*fh->fh_handler)(sc, fh->fh_handarg,
				    &pkt);
				break;
			}
		}
#ifdef FW_DEBUG
		if (fh == NULL) {
			DPRINTFN(1, ("fwohci_ir_input: no handler\n"));
		} else {
			DPRINTFN(1, ("fwohci_ir_input: rcode %d\n", rcode));
		}
#endif
	}
	fwohci_buf_next(sc, fc);

	if (fc->fc_type == FWOHCI_CTX_ISO_SINGLE) {
		OHCI_SYNC_RX_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlSet,
		    OHCI_CTXCTL_WAKE);
	}
}

/*
 * Asynchronous Transmit common routine.
 */
static int
fwohci_at_output(struct fwohci_softc *sc, struct fwohci_ctx *fc,
    struct fwohci_pkt *pkt)
{
	struct fwohci_buf *fb;
	struct fwohci_desc *fd;
	struct mbuf *m, *m0;
	int i, ndesc, error, off, len;
	u_int32_t val;
#ifdef FW_DEBUG
	struct iovec *iov;
#endif
	
	if ((sc->sc_nodeid & OHCI_NodeId_NodeNumber) == IEEE1394_BCAST_PHY_ID)
		/* We can't send anything during selfid duration */
		return EAGAIN;

#ifdef FW_DEBUG
	DPRINTFN(1, ("fwohci_at_output: tcode 0x%x, hlen %d, dlen %d",
	    pkt->fp_tcode, pkt->fp_hlen, pkt->fp_dlen));
	for (i = 0; i < pkt->fp_hlen/4; i++) 
		DPRINTFN(2, ("%s%08x", i?" ":"\n    ", pkt->fp_hdr[i]));
	DPRINTFN(2, ("$"));
	for (ndesc = 0, iov = pkt->fp_iov;
	     ndesc < pkt->fp_uio.uio_iovcnt; ndesc++, iov++) {
		for (i = 0; i < iov->iov_len; i++)
			DPRINTFN(2, ("%s%02x", (i%32)?((i%4)?"":" "):"\n    ",
			    ((u_int8_t *)iov->iov_base)[i]));
		DPRINTFN(2, ("$"));
	}
	DPRINTFN(1, ("\n"));
#endif

	if ((m = pkt->fp_m) != NULL) {
		for (ndesc = 2; m != NULL; m = m->m_next)
			ndesc++;
		if (ndesc > OHCI_DESC_MAX) {
			m0 = NULL;
			ndesc = 2;
			for (off = 0; off < pkt->fp_dlen; off += len) {
				if (m0 == NULL) {
					MGETHDR(m0, M_DONTWAIT, MT_DATA);
					if (m0 != NULL)
						M_COPY_PKTHDR(m0, pkt->fp_m);
					m = m0;
				} else {
					MGET(m->m_next, M_DONTWAIT, MT_DATA);
					m = m->m_next;
				}
				if (m != NULL)
					MCLGET(m, M_DONTWAIT);
				if (m == NULL || (m->m_flags & M_EXT) == 0) {
					m_freem(m0);
					return ENOMEM;
				}
				len = pkt->fp_dlen - off;
				if (len > m->m_ext.ext_size)
					len = m->m_ext.ext_size;
				m_copydata(pkt->fp_m, off, len,
				    mtod(m, caddr_t));
				m->m_len = len;
				ndesc++;
			}
			m_freem(pkt->fp_m);
			pkt->fp_m = m0;
		}
	} else
		ndesc = 2 + pkt->fp_uio.uio_iovcnt;

	if (ndesc > OHCI_DESC_MAX)
		return ENOBUFS;

	if (fc->fc_bufcnt > 50)			/*XXX*/
		return ENOBUFS;
	fb = malloc(sizeof(*fb), M_DEVBUF, M_WAITOK);
	fb->fb_nseg = ndesc;
	fb->fb_desc = fwohci_desc_get(sc, ndesc);
	if (fb->fb_desc == NULL) {
		free(fb, M_DEVBUF);
		return ENOBUFS;
	}
	fb->fb_daddr = sc->sc_ddmamap->dm_segs[0].ds_addr +
	    ((caddr_t)fb->fb_desc - (caddr_t)sc->sc_desc);
	fb->fb_m = pkt->fp_m;
	fb->fb_callback = pkt->fp_callback;
	fb->fb_statuscb = pkt->fp_statuscb;
	fb->fb_statusarg = pkt->fp_statusarg;
	
	if (ndesc > 2) {
		if ((error = bus_dmamap_create(sc->sc_dmat, pkt->fp_dlen, ndesc,
		    PAGE_SIZE, 0, BUS_DMA_WAITOK, &fb->fb_dmamap)) != 0) {
			fwohci_desc_put(sc, fb->fb_desc, ndesc);
			free(fb, M_DEVBUF);
			return error;
		}

		if (pkt->fp_m != NULL)
			error = bus_dmamap_load_mbuf(sc->sc_dmat, fb->fb_dmamap,
			    pkt->fp_m, BUS_DMA_WAITOK);
		else
			error = bus_dmamap_load_uio(sc->sc_dmat, fb->fb_dmamap,
			    &pkt->fp_uio, BUS_DMA_WAITOK);
		if (error != 0) {
			bus_dmamap_destroy(sc->sc_dmat, fb->fb_dmamap);
			fwohci_desc_put(sc, fb->fb_desc, ndesc);
			free(fb, M_DEVBUF);
			return error;
		}
		bus_dmamap_sync(sc->sc_dmat, fb->fb_dmamap, 0, pkt->fp_dlen,
		    BUS_DMASYNC_PREWRITE);
	}

	fd = fb->fb_desc;
	fd->fd_flags = OHCI_DESC_IMMED;
	fd->fd_reqcount = pkt->fp_hlen;
	fd->fd_data = 0;
	fd->fd_branch = 0;
	fd->fd_status = 0;
	if (fc->fc_ctx == OHCI_CTX_ASYNC_TX_RESPONSE) {
		i = 3;				/* XXX: 3 sec */
		val = OHCI_CSR_READ(sc, OHCI_REG_IsochronousCycleTimer);
		fd->fd_timestamp = ((val >> 12) & 0x1fff) |
		    ((((val >> 25) + i) & 0x7) << 13);
	} else
		fd->fd_timestamp = 0;
	memcpy(fd + 1, pkt->fp_hdr, pkt->fp_hlen);
	for (i = 0; i < ndesc - 2; i++) {
		fd = fb->fb_desc + 2 + i;
		fd->fd_flags = 0;
		fd->fd_reqcount = fb->fb_dmamap->dm_segs[i].ds_len;
		fd->fd_data = fb->fb_dmamap->dm_segs[i].ds_addr;
		fd->fd_branch = 0;
		fd->fd_status = 0;
		fd->fd_timestamp = 0;
	}
	fd->fd_flags |= OHCI_DESC_LAST | OHCI_DESC_BRANCH;
	fd->fd_flags |= OHCI_DESC_INTR_ALWAYS;

#ifdef FW_DEBUG
	DPRINTFN(1, ("fwohci_at_output: desc %ld",
	    (long)(fb->fb_desc - sc->sc_desc)));
	for (i = 0; i < ndesc * 4; i++)
		DPRINTFN(2, ("%s%08x", i&7?" ":"\n    ",
		    ((u_int32_t *)fb->fb_desc)[i]));
	DPRINTFN(1, ("\n"));
#endif

	val = OHCI_ASYNC_DMA_READ(sc, fc->fc_ctx,
	    OHCI_SUBREG_ContextControlClear);

	if (val & OHCI_CTXCTL_RUN) {
		if (fc->fc_branch == NULL) {
			OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
			    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
			goto run;
		}
		*fc->fc_branch = fb->fb_daddr | ndesc;
		OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_WAKE);
	} else {
  run:
		OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_CommandPtr, fb->fb_daddr | ndesc);
		OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
		    OHCI_SUBREG_ContextControlSet, OHCI_CTXCTL_RUN);
	}
	fc->fc_branch = &fd->fd_branch;

	fc->fc_bufcnt++;
	TAILQ_INSERT_TAIL(&fc->fc_buf, fb, fb_list);
	pkt->fp_m = NULL;
	return 0;
}

static void
fwohci_at_done(struct fwohci_softc *sc, struct fwohci_ctx *fc, int force)
{
	struct fwohci_buf *fb;
	struct fwohci_desc *fd;
	struct fwohci_pkt pkt;
	int i;

	while ((fb = TAILQ_FIRST(&fc->fc_buf)) != NULL) {
		fd = fb->fb_desc;
#ifdef FW_DEBUG
		DPRINTFN(1, ("fwohci_at_done: %sdesc %ld (%d)",
		    force ? "force " : "", (long)(fd - sc->sc_desc),
		    fb->fb_nseg));
		for (i = 0; i < fb->fb_nseg * 4; i++)
			DPRINTFN(2, ("%s%08x", i&7?" ":"\n    ",
			    ((u_int32_t *)fd)[i]));
		DPRINTFN(1, ("\n"));
#endif
		if (fb->fb_nseg > 2)
			fd += fb->fb_nseg - 1;
		if (!force && !(fd->fd_status & OHCI_CTXCTL_ACTIVE))
			break;
		TAILQ_REMOVE(&fc->fc_buf, fb, fb_list);
		if (fc->fc_branch == &fd->fd_branch) {
			OHCI_ASYNC_DMA_WRITE(sc, fc->fc_ctx,
			    OHCI_SUBREG_ContextControlClear, OHCI_CTXCTL_RUN);
			fc->fc_branch = NULL;
			for (i = 0; i < OHCI_LOOP; i++) {
				if (!(OHCI_ASYNC_DMA_READ(sc, fc->fc_ctx,
				    OHCI_SUBREG_ContextControlClear) &
				    OHCI_CTXCTL_ACTIVE))
					break;
				DELAY(10);
			}
		}

		if (fb->fb_statuscb) {
			memset(&pkt, 0, sizeof(pkt));
			pkt.fp_status = fd->fd_status; 
			memcpy(pkt.fp_hdr, fd + 1, sizeof(pkt.fp_hdr[0]));
			
			/* Indicate this is just returning the status bits. */
			pkt.fp_tcode = -1;
			(*fb->fb_statuscb)(sc, fb->fb_statusarg, &pkt);
			fb->fb_statuscb = NULL;
			fb->fb_statusarg = NULL;
		}
		fwohci_desc_put(sc, fb->fb_desc, fb->fb_nseg);
		if (fb->fb_nseg > 2)
			bus_dmamap_destroy(sc->sc_dmat, fb->fb_dmamap);
		fc->fc_bufcnt--;
		if (fb->fb_callback) {
			(*fb->fb_callback)(sc->sc_sc1394.sc1394_if, fb->fb_m);
			fb->fb_callback = NULL;
		} else if (fb->fb_m != NULL)
			m_freem(fb->fb_m);
		free(fb, M_DEVBUF);
	}
}

/*
 * Asynchronous Transmit Reponse -- in response of request packet.
 */
static void
fwohci_atrs_output(struct fwohci_softc *sc, int rcode, struct fwohci_pkt *req,
    struct fwohci_pkt *res)
{

	if (((*req->fp_trail & 0x001f0000) >> 16) !=
	    OHCI_CTXCTL_EVENT_ACK_PENDING) 
		return;

	res->fp_hdr[0] = (req->fp_hdr[0] & 0x0000fc00) | 0x00000100;
	res->fp_hdr[1] = (req->fp_hdr[1] & 0xffff0000) | (rcode << 12);
	switch (req->fp_tcode) {
	case IEEE1394_TCODE_WRITE_REQ_QUAD:
	case IEEE1394_TCODE_WRITE_REQ_BLOCK:
		res->fp_tcode = IEEE1394_TCODE_WRITE_RESP;
		res->fp_hlen = 12;
		break;
	case IEEE1394_TCODE_READ_REQ_QUAD:
		res->fp_tcode = IEEE1394_TCODE_READ_RESP_QUAD;
		res->fp_hlen = 16;
		res->fp_dlen = 0;
		if (res->fp_uio.uio_iovcnt == 1 && res->fp_iov[0].iov_len == 4)
			res->fp_hdr[3] =
			    *(u_int32_t *)res->fp_iov[0].iov_base;
		res->fp_uio.uio_iovcnt = 0;
		break;
	case IEEE1394_TCODE_READ_REQ_BLOCK:
	case IEEE1394_TCODE_LOCK_REQ:
		if (req->fp_tcode == IEEE1394_TCODE_LOCK_REQ)
			res->fp_tcode = IEEE1394_TCODE_LOCK_RESP;
		else
			res->fp_tcode = IEEE1394_TCODE_READ_RESP_BLOCK;
		res->fp_hlen = 16;
		res->fp_dlen = res->fp_uio.uio_resid;
		res->fp_hdr[3] = res->fp_dlen << 16;
		break;
	}
	res->fp_hdr[0] |= (res->fp_tcode << 4);
	fwohci_at_output(sc, sc->sc_ctx_atrs, res);
}

/*
 * APPLICATION LAYER SERVICES
 */

/*
 * Retrieve Global UID from GUID ROM
 */
static int
fwohci_guidrom_init(struct fwohci_softc *sc)
{
	int i, n, off;
	u_int32_t val1, val2;

	/* Extract the Global UID
	 */
	val1 = OHCI_CSR_READ(sc, OHCI_REG_GUIDHi);
	val2 = OHCI_CSR_READ(sc, OHCI_REG_GUIDLo);

	if (val1 != 0 || val2 != 0) {
		sc->sc_sc1394.sc1394_guid[0] = (val1 >> 24) & 0xff;
		sc->sc_sc1394.sc1394_guid[1] = (val1 >> 16) & 0xff;
		sc->sc_sc1394.sc1394_guid[2] = (val1 >>  8) & 0xff;
		sc->sc_sc1394.sc1394_guid[3] = (val1 >>  0) & 0xff;
		sc->sc_sc1394.sc1394_guid[4] = (val2 >> 24) & 0xff;
		sc->sc_sc1394.sc1394_guid[5] = (val2 >> 16) & 0xff;
		sc->sc_sc1394.sc1394_guid[6] = (val2 >>  8) & 0xff;
		sc->sc_sc1394.sc1394_guid[7] = (val2 >>  0) & 0xff;
	} else {
		val1 = OHCI_CSR_READ(sc, OHCI_REG_Version);
		if ((val1 & OHCI_Version_GUID_ROM) == 0)
			return -1;
		OHCI_CSR_WRITE(sc, OHCI_REG_Guid_Rom, OHCI_Guid_AddrReset);
		for (i = 0; i < OHCI_LOOP; i++) {
			val1 = OHCI_CSR_READ(sc, OHCI_REG_Guid_Rom);
			if (!(val1 & OHCI_Guid_AddrReset))
				break;
			DELAY(10);
		}
		off = OHCI_BITVAL(val1, OHCI_Guid_MiniROM) + 4;
		val2 = 0;
		for (n = 0; n < off + sizeof(sc->sc_sc1394.sc1394_guid); n++) {
			OHCI_CSR_WRITE(sc, OHCI_REG_Guid_Rom,
			    OHCI_Guid_RdStart);
			for (i = 0; i < OHCI_LOOP; i++) {
				val1 = OHCI_CSR_READ(sc, OHCI_REG_Guid_Rom);
				if (!(val1 & OHCI_Guid_RdStart))
					break;
				DELAY(10);
			}
			if (n < off)
				continue;
			val1 = OHCI_BITVAL(val1, OHCI_Guid_RdData);
			sc->sc_sc1394.sc1394_guid[n - off] = val1;
			val2 |= val1;
		}
		if (val2 == 0)
			return -1;
	}
	return 0;
}

/*
 * Initialization for Configuration ROM (no DMA context)
 */

#define	CFR_MAXUNIT		20

struct configromctx {
	u_int32_t	*ptr;
	int		curunit;
	struct {
		u_int32_t	*start;
		int		length;
		u_int32_t	*refer;
		int		refunit;
	} unit[CFR_MAXUNIT];
};

#define	CFR_PUT_DATA4(cfr, d1, d2, d3, d4)				\
	(*(cfr)->ptr++ = (((d1)<<24) | ((d2)<<16) | ((d3)<<8) | (d4)))

#define	CFR_PUT_DATA1(cfr, d)	(*(cfr)->ptr++ = (d))

#define	CFR_PUT_VALUE(cfr, key, d)	(*(cfr)->ptr++ = ((key)<<24) | (d))

#define	CFR_PUT_CRC(cfr, n)						\
	(*(cfr)->unit[n].start = ((cfr)->unit[n].length << 16) |	\
	    fwohci_crc16((cfr)->unit[n].start + 1, (cfr)->unit[n].length))

#define	CFR_START_UNIT(cfr, n)						\
do {									\
	if ((cfr)->unit[n].refer != NULL) {				\
		*(cfr)->unit[n].refer |=				\
		    (cfr)->ptr - (cfr)->unit[n].refer;			\
		CFR_PUT_CRC(cfr, (cfr)->unit[n].refunit);		\
	}								\
	(cfr)->curunit = (n);						\
	(cfr)->unit[n].start = (cfr)->ptr++;				\
} while (0 /* CONSTCOND */)

#define	CFR_PUT_REFER(cfr, key, n)					\
do {									\
	(cfr)->unit[n].refer = (cfr)->ptr;				\
	(cfr)->unit[n].refunit = (cfr)->curunit;			\
	*(cfr)->ptr++ = (key) << 24;					\
} while (0 /* CONSTCOND */)

#define	CFR_END_UNIT(cfr)						\
do {									\
	(cfr)->unit[(cfr)->curunit].length = (cfr)->ptr -		\
	    ((cfr)->unit[(cfr)->curunit].start + 1);			\
	CFR_PUT_CRC(cfr, (cfr)->curunit);				\
} while (0 /* CONSTCOND */)

static u_int16_t
fwohci_crc16(u_int32_t *ptr, int len)
{
	int shift;
	u_int32_t crc, sum, data;

	crc = 0;
	while (len-- > 0) {
		data = *ptr++;
		for (shift = 28; shift >= 0; shift -= 4) {
			sum = ((crc >> 12) ^ (data >> shift)) & 0x000f;
			crc = (crc << 4) ^ (sum << 12) ^ (sum << 5) ^ sum;
		}
		crc &= 0xffff;
	}
	return crc;
}

static void
fwohci_configrom_init(struct fwohci_softc *sc)
{
	int i, val;
	struct fwohci_buf *fb;
	u_int32_t *hdr;
	struct configromctx cfr;

	fb = &sc->sc_buf_cnfrom;
	memset(&cfr, 0, sizeof(cfr));
	cfr.ptr = hdr = (u_int32_t *)fb->fb_buf;

	/* headers */
	CFR_START_UNIT(&cfr, 0);
	CFR_PUT_DATA1(&cfr, OHCI_CSR_READ(sc, OHCI_REG_BusId));
	CFR_PUT_DATA1(&cfr, OHCI_CSR_READ(sc, OHCI_REG_BusOptions));
	CFR_PUT_DATA1(&cfr, OHCI_CSR_READ(sc, OHCI_REG_GUIDHi));
	CFR_PUT_DATA1(&cfr, OHCI_CSR_READ(sc, OHCI_REG_GUIDLo));
	CFR_END_UNIT(&cfr);
	/* copy info_length from crc_length */
	*hdr |= (*hdr & 0x00ff0000) << 8;
	OHCI_CSR_WRITE(sc, OHCI_REG_ConfigROMhdr, *hdr);

	/* root directory */
	CFR_START_UNIT(&cfr, 1);
	CFR_PUT_VALUE(&cfr, 0x03, 0x00005e);	/* vendor id */
	CFR_PUT_REFER(&cfr, 0x81, 2);		/* textual descriptor offset */
	CFR_PUT_VALUE(&cfr, 0x0c, 0x0083c0);	/* node capability */
						/* spt,64,fix,lst,drq */
#ifdef INET
	CFR_PUT_REFER(&cfr, 0xd1, 3);		/* IPv4 unit directory */
#endif /* INET */
#ifdef INET6
	CFR_PUT_REFER(&cfr, 0xd1, 4);		/* IPv6 unit directory */
#endif /* INET6 */
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 2);
	CFR_PUT_VALUE(&cfr, 0, 0);		/* textual descriptor */
	CFR_PUT_DATA1(&cfr, 0);			/* minimal ASCII */
	CFR_PUT_DATA4(&cfr, 'N', 'e', 't', 'B');
	CFR_PUT_DATA4(&cfr, 'S', 'D', 0x00, 0x00);
	CFR_END_UNIT(&cfr);

#ifdef INET
	/* IPv4 unit directory */
	CFR_START_UNIT(&cfr, 3);
	CFR_PUT_VALUE(&cfr, 0x12, 0x00005e);	/* unit spec id */
	CFR_PUT_REFER(&cfr, 0x81, 6);		/* textual descriptor offset */
	CFR_PUT_VALUE(&cfr, 0x13, 0x000001);	/* unit sw version */
	CFR_PUT_REFER(&cfr, 0x81, 7);		/* textual descriptor offset */
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 6);
	CFR_PUT_VALUE(&cfr, 0, 0);		/* textual descriptor */
	CFR_PUT_DATA1(&cfr, 0);			/* minimal ASCII */
	CFR_PUT_DATA4(&cfr, 'I', 'A', 'N', 'A');
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 7);
	CFR_PUT_VALUE(&cfr, 0, 0);		/* textual descriptor */
	CFR_PUT_DATA1(&cfr, 0);			/* minimal ASCII */
	CFR_PUT_DATA4(&cfr, 'I', 'P', 'v', '4');
	CFR_END_UNIT(&cfr);
#endif /* INET */

#ifdef INET6
	/* IPv6 unit directory */
	CFR_START_UNIT(&cfr, 4);
	CFR_PUT_VALUE(&cfr, 0x12, 0x00005e);	/* unit spec id */
	CFR_PUT_REFER(&cfr, 0x81, 8);		/* textual descriptor offset */
	CFR_PUT_VALUE(&cfr, 0x13, 0x000002);	/* unit sw version */
						/* XXX: TBA by IANA */
	CFR_PUT_REFER(&cfr, 0x81, 9);		/* textual descriptor offset */
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 8);
	CFR_PUT_VALUE(&cfr, 0, 0);		/* textual descriptor */
	CFR_PUT_DATA1(&cfr, 0);			/* minimal ASCII */
	CFR_PUT_DATA4(&cfr, 'I', 'A', 'N', 'A');
	CFR_END_UNIT(&cfr);

	CFR_START_UNIT(&cfr, 9);
	CFR_PUT_VALUE(&cfr, 0, 0);		/* textual descriptor */
	CFR_PUT_DATA1(&cfr, 0);
	CFR_PUT_DATA4(&cfr, 'I', 'P', 'v', '6');
	CFR_END_UNIT(&cfr);
#endif /* INET6 */

	fb->fb_off = cfr.ptr - hdr;
#ifdef FW_DEBUG
	DPRINTFN(2, ("%s: Config ROM:", sc->sc_sc1394.sc1394_dev.dv_xname));
	for (i = 0; i < fb->fb_off; i++)
		DPRINTFN(2, ("%s%08x", i&7?" ":"\n    ", hdr[i]));
	DPRINTFN(2, ("\n"));
#endif /* FW_DEBUG */

	/*
	 * Make network byte order for DMA
	 */
	for (i = 0; i < fb->fb_off; i++)
		HTONL(hdr[i]);
	bus_dmamap_sync(sc->sc_dmat, fb->fb_dmamap, 0,
	    (caddr_t)cfr.ptr - fb->fb_buf, BUS_DMASYNC_PREWRITE);

	OHCI_CSR_WRITE(sc, OHCI_REG_ConfigROMmap,
	    fb->fb_dmamap->dm_segs[0].ds_addr);

	/* This register is only valid on OHCI 1.1. */
	val = OHCI_CSR_READ(sc, OHCI_REG_Version);
	if ((OHCI_Version_GET_Version(val) == 1) &&
	    (OHCI_Version_GET_Revision(val) == 1))
		OHCI_CSR_WRITE(sc, OHCI_REG_HCControlSet,
		    OHCI_HCControl_BIBImageValid);
	
	/* Just allow quad reads of the rom. */
	for (i = 0; i < fb->fb_off; i++) 
		fwohci_handler_set(sc, IEEE1394_TCODE_READ_REQ_QUAD,
		    CSR_BASE_HI, CSR_BASE_LO + CSR_CONFIG_ROM + (i * 4),
		    fwohci_configrom_input, NULL);
}

static int
fwohci_configrom_input(struct fwohci_softc *sc, void *arg,
    struct fwohci_pkt *pkt)
{
	struct fwohci_pkt res;
	u_int32_t loc, *rom;

	/* This will be used as an array index so size accordingly. */
	loc = pkt->fp_hdr[2] - (CSR_BASE_LO + CSR_CONFIG_ROM);
	if ((loc & 0x03) != 0) {
		/* alignment error */
		return IEEE1394_RCODE_ADDRESS_ERROR;
	}
	else
		loc /= 4;
	rom = (u_int32_t *)sc->sc_buf_cnfrom.fb_buf;

	DPRINTFN(1, ("fwohci_configrom_input: ConfigRom[0x%04x]: 0x%08x\n", loc,
	    ntohl(rom[loc])));

	memset(&res, 0, sizeof(res));
	res.fp_hdr[3] = rom[loc];
	fwohci_atrs_output(sc, IEEE1394_RCODE_COMPLETE, pkt, &res);
	return -1;
}

/*
 * SelfID buffer (no DMA context)
 */
static void
fwohci_selfid_init(struct fwohci_softc *sc)
{
	struct fwohci_buf *fb;

	fb = &sc->sc_buf_selfid;
#ifdef DIAGNOSTIC
	if ((fb->fb_dmamap->dm_segs[0].ds_addr & 0x7ff) != 0)
		panic("fwohci_selfid_init: not aligned: %p (%ld) %p",
		    (caddr_t)fb->fb_dmamap->dm_segs[0].ds_addr,
		    (unsigned long)fb->fb_dmamap->dm_segs[0].ds_len, fb->fb_buf);
#endif
	memset(fb->fb_buf, 0, fb->fb_dmamap->dm_segs[0].ds_len);
	bus_dmamap_sync(sc->sc_dmat, fb->fb_dmamap, 0,
	    fb->fb_dmamap->dm_segs[0].ds_len, BUS_DMASYNC_PREREAD);

	OHCI_CSR_WRITE(sc, OHCI_REG_SelfIDBuffer,
	    fb->fb_dmamap->dm_segs[0].ds_addr);
}

static int
fwohci_selfid_input(struct fwohci_softc *sc)
{
	int i;
	u_int32_t count, val, gen;
	u_int32_t *buf;

	buf = (u_int32_t *)sc->sc_buf_selfid.fb_buf;
	val = OHCI_CSR_READ(sc, OHCI_REG_SelfIDCount);
  again:
	if (val & OHCI_SelfID_Error) {
		printf("%s: SelfID Error\n", sc->sc_sc1394.sc1394_dev.dv_xname);
		return -1;
	}
	count = OHCI_BITVAL(val, OHCI_SelfID_Size);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_buf_selfid.fb_dmamap,
	    0, count << 2, BUS_DMASYNC_POSTREAD);
	gen = OHCI_BITVAL(buf[0], OHCI_SelfID_Gen);

#ifdef FW_DEBUG
	DPRINTFN(1, ("%s: SelfID: 0x%08x", sc->sc_sc1394.sc1394_dev.dv_xname,
	    val));
	for (i = 0; i < count; i++)
		DPRINTFN(2, ("%s%08x", i&7?" ":"\n    ", buf[i]));
	DPRINTFN(1, ("\n"));
#endif /* FW_DEBUG */

	for (i = 1; i < count; i += 2) {
		if (buf[i] != ~buf[i + 1])
			break;
		if (buf[i] & 0x00000001)
			continue;	/* more pkt */
		if (buf[i] & 0x00800000)
			continue;	/* external id */
		sc->sc_rootid = (buf[i] & 0x3f000000) >> 24;
		if ((buf[i] & 0x00400800) == 0x00400800)
			sc->sc_irmid = sc->sc_rootid;
	}

	val = OHCI_CSR_READ(sc, OHCI_REG_SelfIDCount);
	if (OHCI_BITVAL(val, OHCI_SelfID_Gen) != gen) {
		if (OHCI_BITVAL(val, OHCI_SelfID_Gen) !=
		    OHCI_BITVAL(buf[0], OHCI_SelfID_Gen))
			goto again;
		DPRINTF(("%s: SelfID Gen mismatch (%d, %d)\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, gen,
		    OHCI_BITVAL(val, OHCI_SelfID_Gen)));
		return -1;
	}
	if (i != count) {
		printf("%s: SelfID corrupted (%d, 0x%08x, 0x%08x)\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, i, buf[i], buf[i + 1]);
#if 1
		if (i == 1 && buf[i] == 0 && buf[i + 1] == 0) {
			/*
			 * XXX: CXD3222 sometimes fails to DMA
			 * selfid packet??
			 */
			sc->sc_rootid = (count - 1) / 2 - 1;
			sc->sc_irmid = sc->sc_rootid;
		} else
#endif
		return -1;
	}

	val = OHCI_CSR_READ(sc, OHCI_REG_NodeId);
	if ((val & OHCI_NodeId_IDValid) == 0) {
		sc->sc_nodeid = 0xffff;		/* invalid */
		printf("%s: nodeid is invalid\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname);
		return -1;
	}
	sc->sc_nodeid = val & 0xffff;
	
	DPRINTF(("%s: nodeid=0x%04x(%d), rootid=%d, irmid=%d\n",
	    sc->sc_sc1394.sc1394_dev.dv_xname, sc->sc_nodeid,
	    sc->sc_nodeid & OHCI_NodeId_NodeNumber, sc->sc_rootid,
	    sc->sc_irmid));

	if ((sc->sc_nodeid & OHCI_NodeId_NodeNumber) > sc->sc_rootid)
		return -1;

	if ((sc->sc_nodeid & OHCI_NodeId_NodeNumber) == sc->sc_rootid)
		OHCI_CSR_WRITE(sc, OHCI_REG_LinkControlSet,
		    OHCI_LinkControl_CycleMaster);
	else
		OHCI_CSR_WRITE(sc, OHCI_REG_LinkControlClear,
		    OHCI_LinkControl_CycleMaster);
	return 0;
}

/*
 * some CSRs are handled by driver.
 */
static void
fwohci_csr_init(struct fwohci_softc *sc)
{
	int i;
	static u_int32_t csr[] = { 
	    CSR_STATE_CLEAR, CSR_STATE_SET, CSR_SB_CYCLE_TIME,
	    CSR_SB_BUS_TIME, CSR_SB_BUSY_TIMEOUT, CSR_SB_BUS_MANAGER_ID,
	    CSR_SB_CHANNEL_AVAILABLE_HI, CSR_SB_CHANNEL_AVAILABLE_LO,
	    CSR_SB_BROADCAST_CHANNEL
	};

	for (i = 0; i < sizeof(csr) / sizeof(csr[0]); i++) {
		fwohci_handler_set(sc, IEEE1394_TCODE_WRITE_REQ_QUAD,
		    CSR_BASE_HI, CSR_BASE_LO + csr[i], fwohci_csr_input, NULL);
		fwohci_handler_set(sc, IEEE1394_TCODE_READ_REQ_QUAD,
		    CSR_BASE_HI, CSR_BASE_LO + csr[i], fwohci_csr_input, NULL);
	}
	sc->sc_csr[CSR_SB_BROADCAST_CHANNEL] = 31;	/*XXX*/
}

static int
fwohci_csr_input(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	struct fwohci_pkt res;
	u_int32_t reg;

	/*
	 * XXX need to do special functionality other than just r/w...
	 */
	reg = pkt->fp_hdr[2] - CSR_BASE_LO;

	if ((reg & 0x03) != 0) {
		/* alignment error */
		return IEEE1394_RCODE_ADDRESS_ERROR;
	}
	DPRINTFN(1, ("fwohci_csr_input: CSR[0x%04x]: 0x%08x", reg,
	    *(u_int32_t *)(&sc->sc_csr[reg])));
	if (pkt->fp_tcode == IEEE1394_TCODE_WRITE_REQ_QUAD) {
		DPRINTFN(1, (" -> 0x%08x\n",
		    ntohl(*(u_int32_t *)pkt->fp_iov[0].iov_base)));
		*(u_int32_t *)&sc->sc_csr[reg] =
		    ntohl(*(u_int32_t *)pkt->fp_iov[0].iov_base);
	} else {
		DPRINTFN(1, ("\n"));
		res.fp_hdr[3] = htonl(*(u_int32_t *)&sc->sc_csr[reg]);
		res.fp_iov[0].iov_base = &res.fp_hdr[3];
		res.fp_iov[0].iov_len = 4;
		res.fp_uio.uio_resid = 4;
		res.fp_uio.uio_iovcnt = 1;
		fwohci_atrs_output(sc, IEEE1394_RCODE_COMPLETE, pkt, &res);
		return -1;
	}
	return IEEE1394_RCODE_COMPLETE;
}

/*
 * Mapping between nodeid and unique ID (EUI-64).
 *
 * Track old mappings and simply update their devices with the new id's when
 * they match an existing EUI. This allows proper renumeration of the bus.
 */
static void
fwohci_uid_collect(struct fwohci_softc *sc)
{
	int i;
	struct fwohci_uidtbl *fu;
	struct ieee1394_softc *iea;

	LIST_FOREACH(iea, &sc->sc_nodelist, sc1394_node)
		iea->sc1394_node_id = 0xffff;

	if (sc->sc_uidtbl != NULL)
		free(sc->sc_uidtbl, M_DEVBUF);
	sc->sc_uidtbl = malloc(sizeof(*fu) * (sc->sc_rootid + 1), M_DEVBUF,
	    M_NOWAIT);		/* XXX M_WAITOK requires locks */
	if (sc->sc_uidtbl == NULL)
		return;
	memset(sc->sc_uidtbl, 0, sizeof(*fu) * (sc->sc_rootid + 1));

	for (i = 0, fu = sc->sc_uidtbl; i <= sc->sc_rootid; i++, fu++) {
		if (i == (sc->sc_nodeid & OHCI_NodeId_NodeNumber)) {
			memcpy(fu->fu_uid, sc->sc_sc1394.sc1394_guid, 8);
			fu->fu_valid = 3;

			iea = (struct ieee1394_softc *)sc->sc_sc1394.sc1394_if;
			if (iea) {
				iea->sc1394_node_id = i;
				DPRINTF(("%s: Updating nodeid to %d\n", 
				    iea->sc1394_dev.dv_xname,
				    iea->sc1394_node_id));
			}
		} else {
			fu->fu_valid = 0;
			fwohci_uid_req(sc, i);
		}
	}
	if (sc->sc_rootid == 0)
		fwohci_check_nodes(sc);
}

static void
fwohci_uid_req(struct fwohci_softc *sc, int phyid)
{
	struct fwohci_pkt pkt;

	memset(&pkt, 0, sizeof(pkt));
	pkt.fp_tcode = IEEE1394_TCODE_READ_REQ_QUAD;
	pkt.fp_hlen = 12;
	pkt.fp_dlen = 0;
	pkt.fp_hdr[0] = 0x00000100 | (sc->sc_tlabel << 10) | 
	    (pkt.fp_tcode << 4);
	pkt.fp_hdr[1] = ((0xffc0 | phyid) << 16) | CSR_BASE_HI;
	pkt.fp_hdr[2] = CSR_BASE_LO + CSR_CONFIG_ROM + 12;
	fwohci_handler_set(sc, IEEE1394_TCODE_READ_RESP_QUAD, phyid,
	    sc->sc_tlabel, fwohci_uid_input, (void *)0);
	sc->sc_tlabel = (sc->sc_tlabel + 1) & 0x3f;
	fwohci_at_output(sc, sc->sc_ctx_atrq, &pkt);

	pkt.fp_hdr[0] = 0x00000100 | (sc->sc_tlabel << 10) | 
	    (pkt.fp_tcode << 4);
	pkt.fp_hdr[2] = CSR_BASE_LO + CSR_CONFIG_ROM + 16;
	fwohci_handler_set(sc, IEEE1394_TCODE_READ_RESP_QUAD, phyid,
	    sc->sc_tlabel, fwohci_uid_input, (void *)1);
	sc->sc_tlabel = (sc->sc_tlabel + 1) & 0x3f;
	fwohci_at_output(sc, sc->sc_ctx_atrq, &pkt);
}

static int
fwohci_uid_input(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *res)
{
	struct fwohci_uidtbl *fu;
	struct ieee1394_softc *iea;
	struct ieee1394_attach_args fwa;
	int i, n, done, rcode, found;

	found = 0;

	n = (res->fp_hdr[1] >> 16) & OHCI_NodeId_NodeNumber;
	rcode = (res->fp_hdr[1] & 0x0000f000) >> 12;
	if (rcode != IEEE1394_RCODE_COMPLETE ||
	    sc->sc_uidtbl == NULL ||
	    n > sc->sc_rootid)
		return 0;
	fu = &sc->sc_uidtbl[n];
	if (arg == 0) {
		memcpy(fu->fu_uid, res->fp_iov[0].iov_base, 4);
		fu->fu_valid |= 0x1;
	} else {
		memcpy(fu->fu_uid + 4, res->fp_iov[0].iov_base, 4);
		fu->fu_valid |= 0x2;
	}
#ifdef FW_DEBUG
	if (fu->fu_valid == 0x3)
		DPRINTFN(1, ("fwohci_uid_input: "
		    "Node %d, UID %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", n,
		    fu->fu_uid[0], fu->fu_uid[1], fu->fu_uid[2], fu->fu_uid[3],
		    fu->fu_uid[4], fu->fu_uid[5], fu->fu_uid[6], fu->fu_uid[7]));
#endif
	if (fu->fu_valid == 0x3) {
		LIST_FOREACH(iea, &sc->sc_nodelist, sc1394_node)
			if (memcmp(iea->sc1394_guid, fu->fu_uid, 8) == 0) {
				found = 1;
				iea->sc1394_node_id = n;
				DPRINTF(("%s: Updating nodeid to %d\n", 
				    iea->sc1394_dev.dv_xname,
				    iea->sc1394_node_id));
				break;
			}
		if (!found) {
			strcpy(fwa.name, "fwnode");
			memcpy(fwa.uid, fu->fu_uid, 8);
			fwa.nodeid = n;
			fwa.read = fwohci_read;
			fwa.write = fwohci_write;
			fwa.inreg = fwohci_inreg;
			iea = (struct ieee1394_softc *)
			    config_found_sm(&sc->sc_sc1394.sc1394_dev, &fwa, 
			    fwohci_print, fwohci_submatch);
			if (iea != NULL)
				LIST_INSERT_HEAD(&sc->sc_nodelist, iea,
				    sc1394_node);
		}
	}
	done = 1;

	for (i = 0; i < sc->sc_rootid + 1; i++) {
		fu = &sc->sc_uidtbl[i];
		if (fu->fu_valid != 0x3) {
			done = 0;
			break;
		}
	}
	if (done)
		fwohci_check_nodes(sc);

	return 0;
}

static void
fwohci_check_nodes(struct fwohci_softc *sc)
{
	struct device *detach = NULL;
	struct ieee1394_softc *iea;

	LIST_FOREACH(iea, &sc->sc_nodelist, sc1394_node) {

		/*
		 * Have to defer detachment until the next
		 * loop iteration since config_detach
		 * free's the softc and the loop iterator
		 * needs data from the softc to move
		 * forward.
		 */

		if (detach) {
			config_detach(detach, 0);
			detach = NULL;
		}
		if (iea->sc1394_node_id == 0xffff) {
			detach = (struct device *)iea;
			LIST_REMOVE(iea, sc1394_node);
		}
	}
	if (detach) 
		config_detach(detach, 0);
}

static int
fwohci_uid_lookup(struct fwohci_softc *sc, const u_int8_t *uid)
{
	struct fwohci_uidtbl *fu;
	int n;
	static const u_int8_t bcast[] =
	    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	fu = sc->sc_uidtbl;
	if (fu == NULL) {
		if (memcmp(uid, bcast, sizeof(bcast)) == 0)
			return IEEE1394_BCAST_PHY_ID;
		fwohci_uid_collect(sc); /* try to get */
		return -1;
	}
	for (n = 0; n <= sc->sc_rootid; n++, fu++) {
		if (fu->fu_valid == 0x3 && memcmp(fu->fu_uid, uid, 8) == 0)
			return n;
	}
	if (memcmp(uid, bcast, sizeof(bcast)) == 0)
		return IEEE1394_BCAST_PHY_ID;
	for (n = 0, fu = sc->sc_uidtbl; n <= sc->sc_rootid; n++, fu++) {
		if (fu->fu_valid != 0x3) {
			/*
			 * XXX: need timer before retransmission
			 */
			fwohci_uid_req(sc, n);
		}
	}
	return -1;
}

/*
 * functions to support network interface
 */
static int
fwohci_if_inreg(struct device *self, u_int32_t offhi, u_int32_t offlo,
    void (*handler)(struct device *, struct mbuf *))
{
	struct fwohci_softc *sc = (struct fwohci_softc *)self;

	fwohci_handler_set(sc, IEEE1394_TCODE_WRITE_REQ_BLOCK, offhi, offlo, 
	    handler ? fwohci_if_input : NULL, handler);
	fwohci_handler_set(sc, IEEE1394_TCODE_STREAM_DATA,
	    (sc->sc_csr[CSR_SB_BROADCAST_CHANNEL] & IEEE1394_ISOCH_MASK) | OHCI_ASYNC_STREAM,
	    IEEE1394_TAG_GASP, handler ? fwohci_if_input : NULL, handler);
	return 0;
}

static int
fwohci_if_input(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	int n, len;
	struct mbuf *m;
	struct iovec *iov;
	void (*handler)(struct device *, struct mbuf *) = arg;

#ifdef FW_DEBUG
	int i;
	DPRINTFN(1, ("fwohci_if_input: tcode=0x%x, dlen=%d", pkt->fp_tcode,
	    pkt->fp_dlen));
	for (i = 0; i < pkt->fp_hlen/4; i++)
		DPRINTFN(2, ("%s%08x", i?" ":"\n    ", pkt->fp_hdr[i]));
	DPRINTFN(2, ("$"));
	for (n = 0, len = pkt->fp_dlen; len > 0; len -= i, n++){
		iov = &pkt->fp_iov[n];
		for (i = 0; i < iov->iov_len; i++)
			DPRINTFN(2, ("%s%02x", (i%32)?((i%4)?"":" "):"\n    ",
			    ((u_int8_t *)iov->iov_base)[i]));
		DPRINTFN(2, ("$"));
	}
	DPRINTFN(1, ("\n"));
#endif /* FW_DEBUG */
	len = pkt->fp_dlen;
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return IEEE1394_RCODE_COMPLETE;
	m->m_len = 16;
	if (len + m->m_len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return IEEE1394_RCODE_COMPLETE;
		}
	}
	n = (pkt->fp_hdr[1] >> 16) & OHCI_NodeId_NodeNumber;
	if (sc->sc_uidtbl == NULL || n > sc->sc_rootid ||
	    sc->sc_uidtbl[n].fu_valid != 0x3) {
		printf("%s: packet from unknown node: phy id %d\n",
		    sc->sc_sc1394.sc1394_dev.dv_xname, n);
		m_freem(m);
		fwohci_uid_req(sc, n);
		return IEEE1394_RCODE_COMPLETE;
	}
	memcpy(mtod(m, caddr_t), sc->sc_uidtbl[n].fu_uid, 8);
	if (pkt->fp_tcode == IEEE1394_TCODE_STREAM_DATA) {
		m->m_flags |= M_BCAST;
		mtod(m, u_int32_t *)[2] = mtod(m, u_int32_t *)[3] = 0;
	} else {
		mtod(m, u_int32_t *)[2] = htonl(pkt->fp_hdr[1]);
		mtod(m, u_int32_t *)[3] = htonl(pkt->fp_hdr[2]);
	}
	mtod(m, u_int8_t *)[8] = n;	/*XXX: node id for debug */
	mtod(m, u_int8_t *)[9] =
	    (*pkt->fp_trail >> (16 + OHCI_CTXCTL_SPD_BITPOS)) &
	    ((1 << OHCI_CTXCTL_SPD_BITLEN) - 1);

	m->m_pkthdr.rcvif = NULL;	/* set in child */
	m->m_pkthdr.len = len + m->m_len;
	/*
	 * We may use receive buffer by external mbuf instead of copy here.
	 * But asynchronous receive buffer must be operate in buffer fill
	 * mode, so that each receive buffer will shared by multiple mbufs.
	 * If upper layer doesn't free mbuf soon, e.g. application program
	 * is suspended, buffer must be reallocated.
	 * Isochronous buffer must be operate in packet buffer mode, and
	 * it is easy to map receive buffer to external mbuf.  But it is
	 * used for broadcast/multicast only, and is expected not so
	 * performance sensitive for now.
	 * XXX: The performance may be important for multicast case,
	 * so we should revisit here later.
	 *						-- onoe
	 */
	n = 0;
	iov = pkt->fp_uio.uio_iov;
	while (len > 0) {
		memcpy(mtod(m, caddr_t) + m->m_len, iov->iov_base,
		    iov->iov_len);
		m->m_len += iov->iov_len;
		len -= iov->iov_len;
		iov++;
	}
	(*handler)(sc->sc_sc1394.sc1394_if, m);
	return IEEE1394_RCODE_COMPLETE;
}

static int
fwohci_if_input_iso(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	int n, len;
	int chan, tag;
	struct mbuf *m;
	struct iovec *iov;
	void (*handler)(struct device *, struct mbuf *) = arg;
#ifdef FW_DEBUG
	int i;
#endif

	chan = (pkt->fp_hdr[0] & 0x00003f00) >> 8;
	tag  = (pkt->fp_hdr[0] & 0x0000c000) >> 14;
#ifdef FW_DEBUG
	DPRINTFN(1, ("fwohci_if_input_iso: "
	    "tcode=0x%x, chan=%d, tag=%x, dlen=%d",
	    pkt->fp_tcode, chan, tag, pkt->fp_dlen));
	for (i = 0; i < pkt->fp_hlen/4; i++)
		DPRINTFN(2, ("%s%08x", i?" ":"\n\t", pkt->fp_hdr[i]));
	DPRINTFN(2, ("$"));
	for (n = 0, len = pkt->fp_dlen; len > 0; len -= i, n++){
		iov = &pkt->fp_iov[n];
		for (i = 0; i < iov->iov_len; i++)
			DPRINTFN(2, ("%s%02x",
			    (i%32)?((i%4)?"":" "):"\n\t",
			    ((u_int8_t *)iov->iov_base)[i]));
		DPRINTFN(2, ("$"));
	}
	DPRINTFN(2, ("\n"));
#endif /* FW_DEBUG */
	len = pkt->fp_dlen;
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return IEEE1394_RCODE_COMPLETE;
	m->m_len = 16;
	if (m->m_len + len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return IEEE1394_RCODE_COMPLETE;
		}
	}

	m->m_flags |= M_BCAST;

	if (tag == IEEE1394_TAG_GASP) {
		n = (pkt->fp_hdr[1] >> 16) & OHCI_NodeId_NodeNumber;
		if (sc->sc_uidtbl == NULL || n > sc->sc_rootid ||
		    sc->sc_uidtbl[n].fu_valid != 0x3) {
			printf("%s: packet from unknown node: phy id %d\n",
			    sc->sc_sc1394.sc1394_dev.dv_xname, n);
			m_freem(m);
			return IEEE1394_RCODE_COMPLETE;
		}
		memcpy(mtod(m, caddr_t), sc->sc_uidtbl[n].fu_uid, 8);
		mtod(m, u_int32_t *)[2] = htonl(pkt->fp_hdr[1]);
		mtod(m, u_int32_t *)[3] = htonl(pkt->fp_hdr[2]);
		mtod(m, u_int8_t *)[8] = n;	/*XXX: node id for debug */
		mtod(m, u_int8_t *)[9] =
		    (*pkt->fp_trail >> (16 + OHCI_CTXCTL_SPD_BITPOS)) &
		    ((1 << OHCI_CTXCTL_SPD_BITLEN) - 1);
	} else {
		m->m_flags |= M_LINK0;
	}
	mtod(m, u_int8_t *)[14] = chan;
	mtod(m, u_int8_t *)[15] = tag;


	m->m_pkthdr.rcvif = NULL;	/* set in child */
	m->m_pkthdr.len = len + m->m_len;
	/*
	 * We may use receive buffer by external mbuf instead of copy here.
	 * But asynchronous receive buffer must be operate in buffer fill
	 * mode, so that each receive buffer will shared by multiple mbufs.
	 * If upper layer doesn't free mbuf soon, e.g. application program
	 * is suspended, buffer must be reallocated.
	 * Isochronous buffer must be operate in packet buffer mode, and
	 * it is easy to map receive buffer to external mbuf.  But it is
	 * used for broadcast/multicast only, and is expected not so
	 * performance sensitive for now.
	 * XXX: The performance may be important for multicast case,
	 * so we should revisit here later.
	 *						-- onoe
	 */
	n = 0;
	iov = pkt->fp_uio.uio_iov;
	while (len > 0) {
		memcpy(mtod(m, caddr_t) + m->m_len, iov->iov_base,
		    iov->iov_len);
	        m->m_len += iov->iov_len;
	        len -= iov->iov_len;
		iov++;
	}
	(*handler)(sc->sc_sc1394.sc1394_if, m);
	return IEEE1394_RCODE_COMPLETE;
}



static int
fwohci_if_output(struct device *self, struct mbuf *m0,
    void (*callback)(struct device *, struct mbuf *))
{
	struct fwohci_softc *sc = (struct fwohci_softc *)self;
	struct fwohci_pkt pkt;
	u_int8_t *p;
	int n, error, spd, hdrlen, maxrec;
#ifdef FW_DEBUG
	struct mbuf *m;
#endif

	p = mtod(m0, u_int8_t *);
	if (m0->m_flags & (M_BCAST | M_MCAST)) {
		spd = IEEE1394_SPD_S100;	/*XXX*/
		maxrec = 512;			/*XXX*/
		hdrlen = 8;
	} else {
		n = fwohci_uid_lookup(sc, p);
		if (n < 0) {
			printf("%s: nodeid unknown:"
			    " %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
			    sc->sc_sc1394.sc1394_dev.dv_xname,
			    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
			error = EHOSTUNREACH;
			goto end;
		}
		if (n == IEEE1394_BCAST_PHY_ID) {
			printf("%s: broadcast with !M_MCAST\n",
			    sc->sc_sc1394.sc1394_dev.dv_xname);
#ifdef FW_DEBUG
			DPRINTFN(2, ("packet:"));
			for (m = m0; m != NULL; m = m->m_next) {
				for (n = 0; n < m->m_len; n++)
					DPRINTFN(2, ("%s%02x", (n%32)?
					    ((n%4)?"":" "):"\n    ",
					    mtod(m, u_int8_t *)[n]));
				DPRINTFN(2, ("$"));
			}
			DPRINTFN(2, ("\n"));
#endif
			error = EHOSTUNREACH;
			goto end;
		}
		maxrec = 2 << p[8];
		spd = p[9];
		hdrlen = 0;
	}
	if (spd > sc->sc_sc1394.sc1394_link_speed) {
		DPRINTF(("fwohci_if_output: spd (%d) is faster than %d\n",
		    spd, sc->sc_sc1394.sc1394_link_speed));
		spd = sc->sc_sc1394.sc1394_link_speed;
	}
	if (maxrec > (512 << spd)) {
		DPRINTF(("fwohci_if_output: maxrec (%d) is larger for spd (%d)"
		    "\n", maxrec, spd));
		maxrec = 512 << spd;
	}
	while (maxrec > sc->sc_sc1394.sc1394_max_receive) {
		DPRINTF(("fwohci_if_output: maxrec (%d) is larger than"
		    " %d\n", maxrec, sc->sc_sc1394.sc1394_max_receive));
		maxrec >>= 1;
	}
	if (maxrec < 512) {
		DPRINTF(("fwohci_if_output: maxrec (%d) is smaller than "
		    "minimum\n", maxrec));
		maxrec = 512;
	}

	m_adj(m0, 16 - hdrlen);
	if (m0->m_pkthdr.len > maxrec) {
		DPRINTF(("fwohci_if_output: packet too big: hdr %d, pktlen "
		    "%d, maxrec %d\n", hdrlen, m0->m_pkthdr.len, maxrec));
		error = E2BIG;	/*XXX*/
		goto end;
	}

	memset(&pkt, 0, sizeof(pkt));
	pkt.fp_uio.uio_iov = pkt.fp_iov;
	pkt.fp_uio.uio_segflg = UIO_SYSSPACE;
	pkt.fp_uio.uio_rw = UIO_WRITE;
	if (m0->m_flags & (M_BCAST | M_MCAST)) {
		/* construct GASP header */
		p = mtod(m0, u_int8_t *);
		p[0] = sc->sc_nodeid >> 8;
		p[1] = sc->sc_nodeid & 0xff;
		p[2] = 0x00; p[3] = 0x00; p[4] = 0x5e;
		p[5] = 0x00; p[6] = 0x00; p[7] = 0x01;
		pkt.fp_tcode = IEEE1394_TCODE_STREAM_DATA;
		pkt.fp_hlen = 8;
		pkt.fp_hdr[0] = (spd << 16) | (IEEE1394_TAG_GASP << 14) |
		    ((sc->sc_csr[CSR_SB_BROADCAST_CHANNEL] &
		    OHCI_NodeId_NodeNumber) << 8);
		pkt.fp_hdr[1] = m0->m_pkthdr.len << 16;
	} else {
		pkt.fp_tcode = IEEE1394_TCODE_WRITE_REQ_BLOCK;
		pkt.fp_hlen = 16;
		pkt.fp_hdr[0] = 0x00800100 | (sc->sc_tlabel << 10) |
		    (spd << 16);
		pkt.fp_hdr[1] =
		    (((sc->sc_nodeid & OHCI_NodeId_BusNumber) | n) << 16) |
		    (p[10] << 8) | p[11];
		pkt.fp_hdr[2] = (p[12]<<24) | (p[13]<<16) | (p[14]<<8) | p[15];
		pkt.fp_hdr[3] = m0->m_pkthdr.len << 16;
		sc->sc_tlabel = (sc->sc_tlabel + 1) & 0x3f;
	}
	pkt.fp_hdr[0] |= (pkt.fp_tcode << 4);
	pkt.fp_dlen = m0->m_pkthdr.len;
	pkt.fp_m = m0;
	pkt.fp_callback = callback;
	error = fwohci_at_output(sc, sc->sc_ctx_atrq, &pkt);
	m0 = pkt.fp_m;
  end:
	if (m0 != NULL) {
		if (callback)
			(*callback)(sc->sc_sc1394.sc1394_if, m0);
		else
			m_freem(m0);
	}
	return error;
}

/*
 * High level routines to provide abstraction to attaching layers to
 * send/receive data.
 */

/*
 * These break down into 4 routines as follows:
 *
 * int fwohci_read(struct ieee1394_abuf *)
 *
 * This routine will attempt to read a region from the requested node.
 * A callback must be provided which will be called when either the completed
 * read is done or an unrecoverable error occurs. This is mainly a convenience
 * routine since it will encapsulate retrying a region as quadlet vs. block reads
 * and recombining all the returned data. This could also be done with a series
 * of write/inreg's for each packet sent.
 *
 * int fwohci_write(struct ieee1394_abuf *)
 *
 * The work horse main entry point for putting packets on the bus. This is the
 * generalized interface for fwnode/etc code to put packets out onto the bus.
 * It accepts all standard ieee1394 tcodes (XXX: only a few today) and optionally
 * will callback via a func pointer to the calling code with the resulting ACK
 * code from the packet. If the ACK code is to be ignored (i.e. no cb) then the
 * write routine will take care of free'ing the abuf since the fwnode/etc code
 * won't have any knowledge of when to do this. This allows for simple one-off
 * packets to be sent from the upper-level code without worrying about a callback
 * for cleanup.
 *
 * int fwohci_inreg(struct ieee1394_abuf *, int)
 *
 * This is very simple. It evals the abuf passed in and registers an internal
 * handler as the callback for packets received for that operation.
 * The integer argument specifies whether on a block read/write operation to
 * allow sub-regions to be read/written (in block form) as well.
 *
 * XXX: This whole structure needs to be redone as a list of regions and
 * operations allowed on those regions.
 *
 * int fwohci_unreg(struct ieee1394_abuf *, int)
 *
 * XXX: TBD. For now passing in a NULL ab_cb to inreg will unregister. This
 * routine will simply verify ab_cb is NULL and call inreg.
 *
 * This simply unregisters the respective callback done via inreg for items
 * which only need to register an area for a one-time operation (like a status
 * buffer a remote node will write to when the current operation is done). The
 * int argument specifies the same behavior as inreg, except in reverse (i.e.
 * it unregisters).
 */
 
static int
fwohci_read(struct ieee1394_abuf *ab)
{
	struct fwohci_pkt pkt;
	struct ieee1394_softc *sc = ab->ab_req;
	struct fwohci_softc *psc =
	    (struct fwohci_softc *)sc->sc1394_dev.dv_parent;
	struct fwohci_cb *fcb;
	u_int32_t high, lo;
	int rv, tcode;

	/* Have to have a callback when reading. */
	if (ab->ab_cb == NULL)
		return -1;

	fcb = malloc(sizeof(struct fwohci_cb), M_DEVBUF, M_WAITOK);
	fcb->ab = ab;
	fcb->count = 0;
	fcb->abuf_valid = 1;
	
	high = ((ab->ab_csr & 0x0000ffff00000000) >> 32);
	lo = (ab->ab_csr & 0x00000000ffffffff);

	memset(&pkt, 0, sizeof(pkt));
	pkt.fp_hdr[1] = ((0xffc0 | ab->ab_req->sc1394_node_id) << 16) | high;
	pkt.fp_hdr[2] = lo;
	pkt.fp_dlen = 0;

	if (ab->ab_length == 4) {
		pkt.fp_tcode = IEEE1394_TCODE_READ_REQ_QUAD;
		tcode = IEEE1394_TCODE_READ_RESP_QUAD;
		pkt.fp_hlen = 12;
	} else {
		pkt.fp_tcode = IEEE1394_TCODE_READ_REQ_BLOCK;
		pkt.fp_hlen = 16;
		tcode = IEEE1394_TCODE_READ_RESP_BLOCK;
		pkt.fp_hdr[3] = (ab->ab_length << 16);
	}
	pkt.fp_hdr[0] = 0x00000100 | (sc->sc1394_link_speed << 16) |
	    (psc->sc_tlabel << 10) | (pkt.fp_tcode << 4);

	pkt.fp_statusarg = fcb;
	pkt.fp_statuscb = fwohci_read_resp;

	rv = fwohci_handler_set(psc, tcode, ab->ab_req->sc1394_node_id,
	    psc->sc_tlabel, fwohci_read_resp, fcb);
	if (rv) 
		return rv;
	rv = fwohci_at_output(psc, psc->sc_ctx_atrq, &pkt);
	if (rv)
		fwohci_handler_set(psc, tcode, ab->ab_req->sc1394_node_id,
		    psc->sc_tlabel, NULL, NULL);
	psc->sc_tlabel = (psc->sc_tlabel + 1) & 0x3f;
	fcb->count = 1;
	return rv;
}

static int
fwohci_write(struct ieee1394_abuf *ab)
{
	struct fwohci_pkt pkt;
	struct ieee1394_softc *sc = ab->ab_req;
	struct fwohci_softc *psc =
	    (struct fwohci_softc *)sc->sc1394_dev.dv_parent; 
	u_int32_t high, lo;
	int rv;

	if (ab->ab_length > sc->sc1394_max_receive) {
		DPRINTF(("Packet too large: %d\n", ab->ab_length));
		return E2BIG;
	}

	memset(&pkt, 0, sizeof(pkt));

	pkt.fp_tcode = ab->ab_tcode;
	pkt.fp_uio.uio_iov = pkt.fp_iov;
	pkt.fp_uio.uio_segflg = UIO_SYSSPACE;
	pkt.fp_uio.uio_rw = UIO_WRITE;

	pkt.fp_statusarg = ab;
	pkt.fp_statuscb = fwohci_write_ack;

	switch (ab->ab_tcode) {
	case IEEE1394_TCODE_WRITE_RESP:
		pkt.fp_hlen = 12;
	case IEEE1394_TCODE_READ_RESP_QUAD:
	case IEEE1394_TCODE_READ_RESP_BLOCK:
		if (!pkt.fp_hlen)
			pkt.fp_hlen = 16;
		high = ab->ab_retlen;
		ab->ab_retlen = 0;
		lo = 0;
		pkt.fp_hdr[0] = 0x00000100 | (sc->sc1394_link_speed << 16) |
		    (ab->ab_tlabel << 10) | (pkt.fp_tcode << 4);
		break;
	default:
		pkt.fp_hlen = 16;
		high = ((ab->ab_csr & 0x0000ffff00000000) >> 32);
		lo = (ab->ab_csr & 0x00000000ffffffff);
		pkt.fp_hdr[0] = 0x00000100 | (sc->sc1394_link_speed << 16) |
		    (psc->sc_tlabel << 10) | (pkt.fp_tcode << 4);
		break;
	}

	pkt.fp_hdr[1] = ((0xffc0 | ab->ab_req->sc1394_node_id) << 16) | high;
	pkt.fp_hdr[2] = lo;
	if (pkt.fp_hlen == 16) {
		if (ab->ab_length == 4) {
			pkt.fp_hdr[3] = ab->ab_data[0];
			pkt.fp_dlen = 0;
		}  else {
			pkt.fp_hdr[3] = (ab->ab_length << 16);
			pkt.fp_dlen = ab->ab_length;
			pkt.fp_uio.uio_iovcnt = 1;
			pkt.fp_uio.uio_resid = ab->ab_length;
			pkt.fp_iov[0].iov_base = ab->ab_data;
			pkt.fp_iov[0].iov_len = ab->ab_length;
		}
	}
	switch (ab->ab_tcode) {
	case IEEE1394_TCODE_WRITE_RESP:
	case IEEE1394_TCODE_READ_RESP_QUAD:
	case IEEE1394_TCODE_READ_RESP_BLOCK:
		rv = fwohci_at_output(psc, psc->sc_ctx_atrs, &pkt);
		break;
	default:
		rv = fwohci_at_output(psc, psc->sc_ctx_atrq, &pkt);
		break;
	}
	return rv;
}

static int
fwohci_read_resp(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	struct fwohci_cb *fcb = arg;
	struct ieee1394_abuf *ab = fcb->ab;
	struct fwohci_pkt newpkt;
	u_int32_t *cur, high, lo;
	int i, tcode, rcode, status, rv;

	/*
	 * Both the ACK handling and normal response callbacks are handled here.
	 * The main reason for this is the various error conditions that can
	 * occur trying to block read some areas and the ways that gets reported
	 * back to calling station. This is a variety of ACK codes, responses,
	 * etc which makes it much more difficult to process if both aren't
	 * handled here.
	 */
	
	/* Check for status packet. */

	if (pkt->fp_tcode == -1) {
		status = pkt->fp_status & OHCI_DESC_STATUS_ACK_MASK;
		rcode = -1;
		tcode = (pkt->fp_hdr[0] >> 4) & 0xf;
		if ((status != OHCI_CTXCTL_EVENT_ACK_COMPLETE) &&
		    (status != OHCI_CTXCTL_EVENT_ACK_PENDING))
			DPRINTF(("Got status packet: 0x%02x\n",
			    (unsigned int)status));
		fcb->count--;

		/*
		 * Got all the ack's back and the buffer is invalid (i.e. the
		 * callback has been called. Clean up.
		 */
		
		if (fcb->abuf_valid == 0) {
			if (fcb->count == 0)
				free(fcb, M_DEVBUF);
			return IEEE1394_RCODE_COMPLETE;
		}
	} else {
		status = -1;
		tcode = pkt->fp_tcode;
		rcode = (pkt->fp_hdr[1] & 0x0000f000) >> 12;
	}

	/*
	 * Some area's (like the config rom want to be read as quadlets only.
	 *
	 * The current ideas to try are:
	 *
	 * Got an ACK_TYPE_ERROR on a block read.
	 *
	 * Got either RCODE_TYPE or RCODE_ADDRESS errors in a block read
	 * response.
	 *
	 * In all cases construct a new packet for a quadlet read and let
	 * mutli_resp handle the iteration over the space.
	 */

	if (((status == OHCI_CTXCTL_EVENT_ACK_TYPE_ERROR) &&
	     (tcode == IEEE1394_TCODE_READ_REQ_BLOCK)) ||
	    (((rcode == IEEE1394_RCODE_TYPE_ERROR) ||
	     (rcode == IEEE1394_RCODE_ADDRESS_ERROR)) &&
	      (tcode == IEEE1394_TCODE_READ_RESP_BLOCK))) {

		/* Read the area in quadlet chunks (internally track this). */

		memset(&newpkt, 0, sizeof(newpkt));

		high = ((ab->ab_csr & 0x0000ffff00000000) >> 32);
		lo = (ab->ab_csr & 0x00000000ffffffff);

		newpkt.fp_tcode = IEEE1394_TCODE_READ_REQ_QUAD;
		newpkt.fp_hlen = 12;
		newpkt.fp_dlen = 0;
		newpkt.fp_hdr[1] =
		    ((0xffc0 | ab->ab_req->sc1394_node_id) << 16) | high;
		newpkt.fp_hdr[2] = lo;
		newpkt.fp_hdr[0] = 0x00000100 | (sc->sc_tlabel << 10) |
		    (newpkt.fp_tcode << 4);

		rv = fwohci_handler_set(sc, IEEE1394_TCODE_READ_RESP_QUAD,
		    ab->ab_req->sc1394_node_id, sc->sc_tlabel,
		    fwohci_read_multi_resp, fcb);
		if (rv) {
			(*ab->ab_cb)(ab, -1);
			goto cleanup;
		}
		newpkt.fp_statusarg = fcb;
		newpkt.fp_statuscb = fwohci_read_resp;
		rv = fwohci_at_output(sc, sc->sc_ctx_atrq, &newpkt);
		if (rv) {
			fwohci_handler_set(sc, IEEE1394_TCODE_READ_RESP_QUAD,
			    ab->ab_req->sc1394_node_id, sc->sc_tlabel, NULL,
			    NULL);
			(*ab->ab_cb)(ab, -1);
			goto cleanup;
		}
		fcb->count++;
		sc->sc_tlabel = (sc->sc_tlabel + 1) & 0x3f;
		return IEEE1394_RCODE_COMPLETE;
	} else if ((rcode != -1) || ((status != -1) &&
	    (status != OHCI_CTXCTL_EVENT_ACK_COMPLETE) &&
	    (status != OHCI_CTXCTL_EVENT_ACK_PENDING))) {

		/*
		 * Recombine all the iov data into 1 chunk for higher
		 * level code.
		 */

		if (rcode != -1) {
			cur = ab->ab_data;
			for (i = 0; i < pkt->fp_uio.uio_iovcnt; i++) {
				/*
				 * Make sure and don't exceed the buffer
				 * allocated for return.
				 */
				if ((ab->ab_retlen + pkt->fp_iov[i].iov_len) >
				    ab->ab_length) {
					memcpy(cur, pkt->fp_iov[i].iov_base,
					    (ab->ab_length - ab->ab_retlen));
					ab->ab_retlen = ab->ab_length;
					break;
				}
				memcpy(cur, pkt->fp_iov[i].iov_base,
				    pkt->fp_iov[i].iov_len);
				cur += pkt->fp_iov[i].iov_len;
				ab->ab_retlen += pkt->fp_iov[i].iov_len;
			}
		}
		if (status != -1) 
			/* XXX: Need a complete tlabel interface. */
			for (i = 0; i < 64; i++)
				fwohci_handler_set(sc,
				    IEEE1394_TCODE_READ_RESP_QUAD,
				    ab->ab_req->sc1394_node_id, i, NULL, NULL);
		(*ab->ab_cb)(ab, rcode);
		goto cleanup;
	} else
		/* Good ack packet. */
		return IEEE1394_RCODE_COMPLETE;

	/* Can't get here unless ab->ab_cb has been called. */
	
 cleanup:
	fcb->abuf_valid = 0;
	if (fcb->count == 0)
		free(fcb, M_DEVBUF);
	return IEEE1394_RCODE_COMPLETE;
}

static int
fwohci_read_multi_resp(struct fwohci_softc *sc, void *arg,
    struct fwohci_pkt *pkt)
{
	struct fwohci_cb *fcb = arg;
	struct ieee1394_abuf *ab = fcb->ab;
	struct fwohci_pkt newpkt;
	u_int32_t high, lo;
	int rcode, rv;

	/*
	 * Bad return codes from the wire, just return what's already in the
	 * buf.
	 */

	/* Make sure a response packet didn't arrive after a bad ACK. */
	if (fcb->abuf_valid == 0)
		return IEEE1394_RCODE_COMPLETE;

	rcode = (pkt->fp_hdr[1] & 0x0000f000) >> 12;

	if (rcode) {
		(*ab->ab_cb)(ab, rcode);
		goto cleanup;
	}

	if ((ab->ab_retlen + pkt->fp_iov[0].iov_len) > ab->ab_length) {
		memcpy(((char *)ab->ab_data + ab->ab_retlen),
		    pkt->fp_iov[0].iov_base, (ab->ab_length - ab->ab_retlen));
		ab->ab_retlen = ab->ab_length;
	} else {
		memcpy(((char *)ab->ab_data + ab->ab_retlen),
		    pkt->fp_iov[0].iov_base, 4);
		ab->ab_retlen += 4;
	}
	/* Still more, loop and read 4 more bytes. */
	if (ab->ab_retlen < ab->ab_length) {
		memset(&newpkt, 0, sizeof(newpkt));

		high = ((ab->ab_csr & 0x0000ffff00000000) >> 32);
		lo = (ab->ab_csr & 0x00000000ffffffff) + ab->ab_retlen;

		newpkt.fp_tcode = IEEE1394_TCODE_READ_REQ_QUAD;
		newpkt.fp_hlen = 12;
		newpkt.fp_dlen = 0;
		newpkt.fp_hdr[1] =
		    ((0xffc0 | ab->ab_req->sc1394_node_id) << 16) | high;
		newpkt.fp_hdr[2] = lo;
		newpkt.fp_hdr[0] = 0x00000100 | (sc->sc_tlabel << 10) |
		    (newpkt.fp_tcode << 4);

		newpkt.fp_statusarg = fcb;
		newpkt.fp_statuscb = fwohci_read_resp;

		/*
		 * Bad return code.  Just give up and return what's
		 * come in now.
		 */
		rv = fwohci_handler_set(sc, IEEE1394_TCODE_READ_RESP_QUAD,
		    ab->ab_req->sc1394_node_id, sc->sc_tlabel,
		    fwohci_read_multi_resp, fcb);
		if (rv) 
			(*ab->ab_cb)(ab, -1);
		else {
			rv = fwohci_at_output(sc, sc->sc_ctx_atrq, &newpkt);
			if (rv) {
				fwohci_handler_set(sc,
				    IEEE1394_TCODE_READ_RESP_QUAD,
				    ab->ab_req->sc1394_node_id, sc->sc_tlabel,
				    NULL, NULL);
				(*ab->ab_cb)(ab, -1);
			} else {
				sc->sc_tlabel = (sc->sc_tlabel + 1) & 0x3f;
				fcb->count++;
				return IEEE1394_RCODE_COMPLETE;
			}
		}
	} else
		(*ab->ab_cb)(ab, IEEE1394_RCODE_COMPLETE);

 cleanup:
	/* Can't get here unless ab_cb has been called. */
	fcb->abuf_valid = 0;
	if (fcb->count == 0)
		free(fcb, M_DEVBUF);
	return IEEE1394_RCODE_COMPLETE;
}

static int
fwohci_write_ack(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	struct ieee1394_abuf *ab = arg;
	u_int16_t status;


	status = pkt->fp_status & OHCI_DESC_STATUS_ACK_MASK;
	if ((status != OHCI_CTXCTL_EVENT_ACK_COMPLETE) &&
	    (status != OHCI_CTXCTL_EVENT_ACK_PENDING))
		DPRINTF(("Got status packet: 0x%02x\n",
		    (unsigned int)status));

	/* No callback means this level should free the buffers. */
	if (ab->ab_cb)
		(*ab->ab_cb)(ab, status);
	else {
		if (ab->ab_data)
			free(ab->ab_data, M_1394DATA);
		free(ab, M_1394DATA);
	}
	return IEEE1394_RCODE_COMPLETE;
}

static int
fwohci_inreg(struct ieee1394_abuf *ab, int allow)
{
	struct ieee1394_softc *sc = ab->ab_req;
	struct fwohci_softc *psc =
	    (struct fwohci_softc *)sc->sc1394_dev.dv_parent; 
	u_int32_t high, lo;
	int i, j, rv;

	high = ((ab->ab_csr & 0x0000ffff00000000) >> 32);
	lo = (ab->ab_csr & 0x00000000ffffffff);

	rv = 0;
	switch (ab->ab_tcode) {
	case IEEE1394_TCODE_READ_REQ_QUAD:
	case IEEE1394_TCODE_WRITE_REQ_QUAD:
		if (ab->ab_cb)
			rv = fwohci_handler_set(psc, ab->ab_tcode, high, lo,
			    fwohci_parse_input, ab);
		else
			fwohci_handler_set(psc, ab->ab_tcode, high, lo, NULL,
			    NULL);
		break;
	case IEEE1394_TCODE_READ_REQ_BLOCK:
	case IEEE1394_TCODE_WRITE_REQ_BLOCK:
		if (allow) {
			for (i = 0; i < (ab->ab_length / 4); i++) {
				if (ab->ab_cb) {
					rv = fwohci_handler_set(psc,
					    ab->ab_tcode, high, lo + (i * 4),
					    fwohci_parse_input, ab);
					if (rv)
						break;
				} else
					fwohci_handler_set(psc, ab->ab_tcode,
					    high, lo + (i * 4), NULL, NULL);
			}
			if (i != (ab->ab_length / 4)) {
				j = i + 1;
				for (i = 0; i < j; i++) 
					fwohci_handler_set(psc, ab->ab_tcode,
					    high, lo + (i * 4), NULL, NULL);
			} else
				ab->ab_data = (void *)1;
		} else {
			if (ab->ab_cb) 
				rv = fwohci_handler_set(psc, ab->ab_tcode, high,
				    lo, fwohci_parse_input, ab);
			else
				fwohci_handler_set(psc, ab->ab_tcode, high, lo,
				    NULL, NULL);
		}
		break;
	default:
		DPRINTF(("Invalid registration tcode: %d\n", ab->ab_tcode));
		return -1;
		break;
	}
	return rv;
}

static int
fwohci_parse_input(struct fwohci_softc *sc, void *arg, struct fwohci_pkt *pkt)
{
	struct ieee1394_abuf *ab = (struct ieee1394_abuf *)arg;
	u_int64_t csr;
	u_int32_t *cur;
	int i, count;

	ab->ab_tcode = (pkt->fp_hdr[0] >> 4) & 0xf;
	ab->ab_tlabel = (pkt->fp_hdr[0] >> 10) & 0x3f;
	csr = (((u_int64_t)(pkt->fp_hdr[1] & 0xffff) << 32) | pkt->fp_hdr[2]);

	switch (ab->ab_tcode) {
	case IEEE1394_TCODE_READ_REQ_QUAD:
		ab->ab_retlen = 4;
		break;
	case IEEE1394_TCODE_READ_REQ_BLOCK:
		ab->ab_retlen = (pkt->fp_hdr[3] >> 16) & 0xffff;
		if (ab->ab_data) {
			if ((csr + ab->ab_retlen) >
			    (ab->ab_csr + ab->ab_length))
				return IEEE1394_RCODE_ADDRESS_ERROR;
			ab->ab_data = NULL;
		} else
			if (ab->ab_retlen != ab->ab_length)
				return IEEE1394_RCODE_ADDRESS_ERROR;
		break;
	case IEEE1394_TCODE_WRITE_REQ_QUAD:
		ab->ab_retlen = 4;
	case IEEE1394_TCODE_WRITE_REQ_BLOCK:
		if (!ab->ab_retlen) 
			ab->ab_retlen = (pkt->fp_hdr[3] >> 16) & 0xffff;
		if (ab->ab_data) {
			if ((csr + ab->ab_retlen) >
			    (ab->ab_csr + ab->ab_length))
				return IEEE1394_RCODE_ADDRESS_ERROR;
			ab->ab_data = NULL;
		} else
			if (ab->ab_retlen != ab->ab_length)
				return IEEE1394_RCODE_ADDRESS_ERROR;

		ab->ab_data = malloc(ab->ab_retlen, M_1394DATA, M_WAITOK);
		if (ab->ab_tcode == IEEE1394_TCODE_WRITE_REQ_QUAD)
			ab->ab_data[0] = pkt->fp_hdr[3];
		else {
			count = 0;
			cur = ab->ab_data;
			for (i = 0; i < pkt->fp_uio.uio_iovcnt; i++) {
				memcpy(cur, pkt->fp_iov[i].iov_base,
				    pkt->fp_iov[i].iov_len);
				cur += pkt->fp_iov[i].iov_len;
				count += pkt->fp_iov[i].iov_len;
			}
			if (ab->ab_retlen != count)
				panic("Packet claims %d length "
				    "but only %d bytes returned\n",
				    ab->ab_retlen, count);
		}
		break;
	default:
		panic("Got a callback for a tcode that wasn't requested: %d\n",
		    ab->ab_tcode);
		break;
	}
	ab->ab_csr = csr;
	ab->ab_cb(ab, IEEE1394_RCODE_COMPLETE);
	return -1;
}

static int
fwohci_submatch(struct device *parent, struct cfdata *cf, void *aux)
{
	struct ieee1394_attach_args *fwa = aux;

	/* Both halves must be filled in for a match. */
	if ((cf->fwbuscf_idhi == FWBUS_UNK_IDHI &&
	    cf->fwbuscf_idlo == FWBUS_UNK_IDLO) ||
	    (cf->fwbuscf_idhi == ntohl(*((u_int32_t *)&fwa->uid[0])) &&
	    cf->fwbuscf_idlo == ntohl(*((u_int32_t *)&fwa->uid[4]))))
		return ((*cf->cf_attach->ca_match)(parent, cf, aux));
	return 0;
}

#ifdef FW_DEBUG
static void
fwohci_show_intr(struct fwohci_softc *sc, u_int32_t intmask)
{

	printf("%s: intmask=0x%08x:", sc->sc_sc1394.sc1394_dev.dv_xname,
	    intmask);
	if (intmask & OHCI_Int_CycleTooLong)
		printf(" CycleTooLong");
	if (intmask & OHCI_Int_UnrecoverableError)
		printf(" UnrecoverableError");
	if (intmask & OHCI_Int_CycleInconsistent)
		printf(" CycleInconsistent");
	if (intmask & OHCI_Int_BusReset)
		printf(" BusReset");
	if (intmask & OHCI_Int_SelfIDComplete)
		printf(" SelfIDComplete");
	if (intmask & OHCI_Int_LockRespErr)
		printf(" LockRespErr");
	if (intmask & OHCI_Int_PostedWriteErr)
		printf(" PostedWriteErr");
	if (intmask & OHCI_Int_ReqTxComplete)
		printf(" ReqTxComplete(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_TX_REQUEST,
		    OHCI_SUBREG_ContextControlClear));
	if (intmask & OHCI_Int_RespTxComplete)
		printf(" RespTxComplete(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_TX_RESPONSE,
		    OHCI_SUBREG_ContextControlClear));
	if (intmask & OHCI_Int_ARRS)
		printf(" ARRS(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_RX_RESPONSE,
		    OHCI_SUBREG_ContextControlClear));
	if (intmask & OHCI_Int_ARRQ)
		printf(" ARRQ(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_RX_REQUEST,
		    OHCI_SUBREG_ContextControlClear));
	if (intmask & OHCI_Int_IsochRx)
		printf(" IsochRx(0x%08x)",
		    OHCI_CSR_READ(sc, OHCI_REG_IsoRecvIntEventClear));
	if (intmask & OHCI_Int_IsochTx)
		printf(" IsochTx(0x%08x)",
		    OHCI_CSR_READ(sc, OHCI_REG_IsoXmitIntEventClear));
	if (intmask & OHCI_Int_RQPkt)
		printf(" RQPkt(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_RX_REQUEST,
		    OHCI_SUBREG_ContextControlClear));
	if (intmask & OHCI_Int_RSPkt)
		printf(" RSPkt(0x%04x)",
		    OHCI_ASYNC_DMA_READ(sc, OHCI_CTX_ASYNC_RX_RESPONSE,
		    OHCI_SUBREG_ContextControlClear));
	printf("\n");
}

static void
fwohci_show_phypkt(struct fwohci_softc *sc, u_int32_t val)
{
	u_int8_t key, phyid;

	key = (val & 0xc0000000) >> 30;
	phyid = (val & 0x3f000000) >> 24;
	printf("%s: PHY packet from %d: ",
	    sc->sc_sc1394.sc1394_dev.dv_xname, phyid);
	switch (key) {
	case 0:
		printf("PHY Config:");
		if (val & 0x00800000)
			printf(" ForceRoot");
		if (val & 0x00400000)
			printf(" Gap=%x", (val & 0x003f0000) >> 16);
		printf("\n");
		break;
	case 1:
		printf("Link-on\n");
		break;
	case 2:
		printf("SelfID:");
		if (val & 0x00800000) {
			printf(" #%d", (val & 0x00700000) >> 20);
		} else {
			if (val & 0x00400000)
				printf(" LinkActive");
			printf(" Gap=%x", (val & 0x003f0000) >> 16);
			printf(" Spd=S%d", 100 << ((val & 0x0000c000) >> 14));
			if (val & 0x00000800)
				printf(" Cont");
			if (val & 0x00000002)
				printf(" InitiateBusReset");
		}
		if (val & 0x00000001)
			printf(" +");
		printf("\n");
		break;
	default:
		printf("unknown: 0x%08x\n", val);
		break;
	}
}
#endif /* FW_DEBUG */
