/*	$NetBSD: if_wm.c,v 1.68.2.3.2.7 2005/02/23 15:34:11 he Exp $	*/

/*
 * Copyright (c) 2001, 2002, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Device driver for the Intel i8254x family of Gigabit Ethernet chips.
 *
 * TODO (in order of importance):
 *
 *	- Rework how parameters are loaded from the EEPROM.
 *	- Figure out what to do with the i82545GM and i82546GB
 *	  SERDES controllers.
 *	- Fix hw VLAN assist.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_wm.c,v 1.68.2.3.2.7 2005/02/23 15:34:11 he Exp $");

#include "bpfilter.h"
#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h> 
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <uvm/uvm_extern.h>		/* for PAGE_SIZE */

#if NRND > 0
#include <sys/rnd.h>
#endif

#include <net/if.h>
#include <net/if_dl.h> 
#include <net/if_media.h>
#include <net/if_ether.h>

#if NBPFILTER > 0 
#include <net/bpf.h>
#endif

#include <netinet/in.h>			/* XXX for struct ip */
#include <netinet/in_systm.h>		/* XXX for struct ip */
#include <netinet/ip.h>			/* XXX for struct ip */
#include <netinet/tcp.h>		/* XXX for struct tcphdr */

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/endian.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_wmreg.h>

#ifdef WM_DEBUG
#define	WM_DEBUG_LINK		0x01
#define	WM_DEBUG_TX		0x02
#define	WM_DEBUG_RX		0x04
#define	WM_DEBUG_GMII		0x08
int	wm_debug = WM_DEBUG_TX|WM_DEBUG_RX|WM_DEBUG_LINK;

#define	DPRINTF(x, y)	if (wm_debug & (x)) printf y
#else
#define	DPRINTF(x, y)	/* nothing */
#endif /* WM_DEBUG */

/*
 * Transmit descriptor list size.  Due to errata, we can only have
 * 256 hardware descriptors in the ring.  We tell the upper layers
 * that they can queue a lot of packets, and we go ahead and manage
 * up to 64 (16 for the i82547) of them at a time.  We allow up to
 * 40 DMA segments per packet (there have been reports of jumbo frame
 * packets with as many as 30 DMA segments!).
 */
#define	WM_NTXSEGS		40
#define	WM_IFQUEUELEN		256
#define	WM_TXQUEUELEN_MAX	64
#define	WM_TXQUEUELEN_MAX_82547	16
#define	WM_TXQUEUELEN(sc)	((sc)->sc_txnum)
#define	WM_TXQUEUELEN_MASK(sc)	(WM_TXQUEUELEN(sc) - 1)
#define	WM_TXQUEUE_GC(sc)	(WM_TXQUEUELEN(sc) / 8)
#define	WM_NTXDESC		256
#define	WM_NTXDESC_MASK		(WM_NTXDESC - 1)
#define	WM_NEXTTX(x)		(((x) + 1) & WM_NTXDESC_MASK)
#define	WM_NEXTTXS(sc, x)	(((x) + 1) & WM_TXQUEUELEN_MASK(sc))

/*
 * Receive descriptor list size.  We have one Rx buffer for normal
 * sized packets.  Jumbo packets consume 5 Rx buffers for a full-sized
 * packet.  We allocate 256 receive descriptors, each with a 2k
 * buffer (MCLBYTES), which gives us room for 50 jumbo packets.
 */
#define	WM_NRXDESC		256
#define	WM_NRXDESC_MASK		(WM_NRXDESC - 1)
#define	WM_NEXTRX(x)		(((x) + 1) & WM_NRXDESC_MASK)
#define	WM_PREVRX(x)		(((x) - 1) & WM_NRXDESC_MASK)

/*
 * Control structures are DMA'd to the i82542 chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make serveral things
 * easier.
 */
struct wm_control_data {
	/*
	 * The transmit descriptors.
	 */
	wiseman_txdesc_t wcd_txdescs[WM_NTXDESC];

	/*
	 * The receive descriptors.
	 */
	wiseman_rxdesc_t wcd_rxdescs[WM_NRXDESC];
};

#define	WM_CDOFF(x)	offsetof(struct wm_control_data, x)
#define	WM_CDTXOFF(x)	WM_CDOFF(wcd_txdescs[(x)])
#define	WM_CDRXOFF(x)	WM_CDOFF(wcd_rxdescs[(x)])

/*
 * Software state for transmit jobs.
 */
struct wm_txsoft {
	struct mbuf *txs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t txs_dmamap;	/* our DMA map */
	int txs_firstdesc;		/* first descriptor in packet */
	int txs_lastdesc;		/* last descriptor in packet */
	int txs_ndesc;			/* # of descriptors used */
};

/*
 * Software state for receive buffers.  Each descriptor gets a
 * 2k (MCLBYTES) buffer and a DMA map.  For packets which fill
 * more than one buffer, we chain them together.
 */
struct wm_rxsoft {
	struct mbuf *rxs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t rxs_dmamap;	/* our DMA map */
};

typedef enum {
	WM_T_unknown		= 0,
	WM_T_82542_2_0,			/* i82542 2.0 (really old) */
	WM_T_82542_2_1,			/* i82542 2.1+ (old) */
	WM_T_82543,			/* i82543 */
	WM_T_82544,			/* i82544 */
	WM_T_82540,			/* i82540 */
	WM_T_82545,			/* i82545 */
	WM_T_82545_3,			/* i82545 3.0+ */
	WM_T_82546,			/* i82546 */
	WM_T_82546_3,			/* i82546 3.0+ */
	WM_T_82541,			/* i82541 */
	WM_T_82541_2,			/* i82541 2.0+ */
	WM_T_82547,			/* i82547 */
	WM_T_82547_2,			/* i82547 2.0+ */
} wm_chip_type;

/*
 * Software state per device.
 */
struct wm_softc {
	struct device sc_dev;		/* generic device information */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_space_tag_t sc_iot;		/* I/O space tag */
	bus_space_handle_t sc_ioh;	/* I/O space handle */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	struct ethercom sc_ethercom;	/* ethernet common data */
	void *sc_sdhook;		/* shutdown hook */

	wm_chip_type sc_type;		/* chip type */
	int sc_flags;			/* flags; see below */
	int sc_bus_speed;		/* PCI/PCIX bus speed */
	int sc_pcix_offset;		/* PCIX capability register offset */

	void *sc_ih;			/* interrupt cookie */

	int sc_ee_addrbits;		/* EEPROM address bits */

	struct mii_data sc_mii;		/* MII/media information */

	struct callout sc_tick_ch;	/* tick callout */

	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	int		sc_align_tweak;

	/*
	 * Software state for the transmit and receive descriptors.
	 */
	int			sc_txnum;	/* must be a power of two */
	struct wm_txsoft	sc_txsoft[WM_TXQUEUELEN_MAX];
	struct wm_rxsoft	sc_rxsoft[WM_NRXDESC];

	/*
	 * Control data structures.
	 */
	struct wm_control_data *sc_control_data;
#define	sc_txdescs	sc_control_data->wcd_txdescs
#define	sc_rxdescs	sc_control_data->wcd_rxdescs

#ifdef WM_EVENT_COUNTERS
	/* Event counters. */
	struct evcnt sc_ev_txsstall;	/* Tx stalled due to no txs */
	struct evcnt sc_ev_txdstall;	/* Tx stalled due to no txd */
 	struct evcnt sc_ev_txfifo_stall;/* Tx FIFO stalls (82547) */
	struct evcnt sc_ev_txforceintr;	/* Tx interrupts forced */
	struct evcnt sc_ev_txdw;	/* Tx descriptor interrupts */
	struct evcnt sc_ev_txqe;	/* Tx queue empty interrupts */
	struct evcnt sc_ev_rxintr;	/* Rx interrupts */
	struct evcnt sc_ev_linkintr;	/* Link interrupts */

	struct evcnt sc_ev_rxipsum;	/* IP checksums checked in-bound */
	struct evcnt sc_ev_rxtusum;	/* TCP/UDP cksums checked in-bound */
	struct evcnt sc_ev_txipsum;	/* IP checksums comp. out-bound */
	struct evcnt sc_ev_txtusum;	/* TCP/UDP cksums comp. out-bound */

	struct evcnt sc_ev_txctx_init;	/* Tx cksum context cache initialized */
	struct evcnt sc_ev_txctx_hit;	/* Tx cksum context cache hit */
	struct evcnt sc_ev_txctx_miss;	/* Tx cksum context cache miss */

	struct evcnt sc_ev_txseg[WM_NTXSEGS]; /* Tx packets w/ N segments */
	struct evcnt sc_ev_txdrop;	/* Tx packets dropped (too many segs) */

	struct evcnt sc_ev_tu;		/* Tx underrun */
#endif /* WM_EVENT_COUNTERS */

	bus_addr_t sc_tdt_reg;		/* offset of TDT register */

	int	sc_txfree;		/* number of free Tx descriptors */
	int	sc_txnext;		/* next ready Tx descriptor */

	int	sc_txsfree;		/* number of free Tx jobs */
	int	sc_txsnext;		/* next free Tx job */
	int	sc_txsdirty;		/* dirty Tx jobs */

	/* These 5 variables are used only on the 82547. */
	int	sc_txfifo_size;		/* Tx FIFO size */
	int	sc_txfifo_head;		/* current head of FIFO */
	uint32_t sc_txfifo_addr;	/* internal address of start of FIFO */
	int	sc_txfifo_stall;	/* Tx FIFO is stalled */
	struct callout sc_txfifo_ch;	/* Tx FIFO stall work-around timer */

	uint32_t sc_txctx_ipcs;		/* cached Tx IP cksum ctx */
	uint32_t sc_txctx_tucs;		/* cached Tx TCP/UDP cksum ctx */

	bus_addr_t sc_rdt_reg;		/* offset of RDT register */

	int	sc_rxptr;		/* next ready Rx descriptor/queue ent */
	int	sc_rxdiscard;
	int	sc_rxlen;
	struct mbuf *sc_rxhead;
	struct mbuf *sc_rxtail;
	struct mbuf **sc_rxtailp;

	uint32_t sc_ctrl;		/* prototype CTRL register */
#if 0
	uint32_t sc_ctrl_ext;		/* prototype CTRL_EXT register */
#endif
	uint32_t sc_icr;		/* prototype interrupt bits */
	uint32_t sc_tctl;		/* prototype TCTL register */
	uint32_t sc_rctl;		/* prototype RCTL register */
	uint32_t sc_txcw;		/* prototype TXCW register */
	uint32_t sc_tipg;		/* prototype TIPG register */
 	uint32_t sc_pba;		/* prototype PBA register */

	int sc_tbi_linkup;		/* TBI link status */
	int sc_tbi_anstate;		/* autonegotiation state */

	int sc_mchash_type;		/* multicast filter offset */

#if NRND > 0
	rndsource_element_t rnd_source;	/* random source */
#endif
};

#define	WM_RXCHAIN_RESET(sc)						\
do {									\
	(sc)->sc_rxtailp = &(sc)->sc_rxhead;				\
	*(sc)->sc_rxtailp = NULL;					\
	(sc)->sc_rxlen = 0;						\
} while (/*CONSTCOND*/0)

#define	WM_RXCHAIN_LINK(sc, m)						\
do {									\
	*(sc)->sc_rxtailp = (sc)->sc_rxtail = (m);			\
	(sc)->sc_rxtailp = &(m)->m_next;				\
} while (/*CONSTCOND*/0)

/* sc_flags */
#define	WM_F_HAS_MII		0x01	/* has MII */
#define	WM_F_EEPROM_HANDSHAKE	0x02	/* requires EEPROM handshake */
#define	WM_F_EEPROM_SPI		0x04	/* EEPROM is SPI */
#define	WM_F_IOH_VALID		0x10	/* I/O handle is valid */
#define	WM_F_BUS64		0x20	/* bus is 64-bit */
#define	WM_F_PCIX		0x40	/* bus is PCI-X */
#define	WM_F_CSA		0x80	/* bus is CSA */

#ifdef WM_EVENT_COUNTERS
#define	WM_EVCNT_INCR(ev)	(ev)->ev_count++
#else
#define	WM_EVCNT_INCR(ev)	/* nothing */
#endif

#define	CSR_READ(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_WRITE(sc, reg, val)						\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))
#define	CSR_WRITE_FLUSH(sc)						\
	(void) CSR_READ((sc), WMREG_STATUS)

#define	WM_CDTXADDR(sc, x)	((sc)->sc_cddma + WM_CDTXOFF((x)))
#define	WM_CDRXADDR(sc, x)	((sc)->sc_cddma + WM_CDRXOFF((x)))

#define	WM_CDTXSYNC(sc, x, n, ops)					\
do {									\
	int __x, __n;							\
									\
	__x = (x);							\
	__n = (n);							\
									\
	/* If it will wrap around, sync to the end of the ring. */	\
	if ((__x + __n) > WM_NTXDESC) {					\
		bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,	\
		    WM_CDTXOFF(__x), sizeof(wiseman_txdesc_t) *		\
		    (WM_NTXDESC - __x), (ops));				\
		__n -= (WM_NTXDESC - __x);				\
		__x = 0;						\
	}								\
									\
	/* Now sync whatever is left. */				\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    WM_CDTXOFF(__x), sizeof(wiseman_txdesc_t) * __n, (ops));	\
} while (/*CONSTCOND*/0)

#define	WM_CDRXSYNC(sc, x, ops)						\
do {									\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	   WM_CDRXOFF((x)), sizeof(wiseman_rxdesc_t), (ops));		\
} while (/*CONSTCOND*/0)

#define	WM_INIT_RXDESC(sc, x)						\
do {									\
	struct wm_rxsoft *__rxs = &(sc)->sc_rxsoft[(x)];		\
	wiseman_rxdesc_t *__rxd = &(sc)->sc_rxdescs[(x)];		\
	struct mbuf *__m = __rxs->rxs_mbuf;				\
									\
	/*								\
	 * Note: We scoot the packet forward 2 bytes in the buffer	\
	 * so that the payload after the Ethernet header is aligned	\
	 * to a 4-byte boundary.					\
	 *								\
	 * XXX BRAINDAMAGE ALERT!					\
	 * The stupid chip uses the same size for every buffer, which	\
	 * is set in the Receive Control register.  We are using the 2K	\
	 * size option, but what we REALLY want is (2K - 2)!  For this	\
	 * reason, we can't "scoot" packets longer than the standard	\
	 * Ethernet MTU.  On strict-alignment platforms, if the total	\
	 * size exceeds (2K - 2) we set align_tweak to 0 and let	\
	 * the upper layer copy the headers.				\
	 */								\
	__m->m_data = __m->m_ext.ext_buf + (sc)->sc_align_tweak;	\
									\
	__rxd->wrx_addr.wa_low =					\
	    htole32(__rxs->rxs_dmamap->dm_segs[0].ds_addr + 		\
		(sc)->sc_align_tweak);					\
	__rxd->wrx_addr.wa_high = 0;					\
	__rxd->wrx_len = 0;						\
	__rxd->wrx_cksum = 0;						\
	__rxd->wrx_status = 0;						\
	__rxd->wrx_errors = 0;						\
	__rxd->wrx_special = 0;						\
	WM_CDRXSYNC((sc), (x), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE); \
									\
	CSR_WRITE((sc), (sc)->sc_rdt_reg, (x));				\
} while (/*CONSTCOND*/0)

static void	wm_start(struct ifnet *);
static void	wm_watchdog(struct ifnet *);
static int	wm_ioctl(struct ifnet *, u_long, caddr_t);
static int	wm_init(struct ifnet *);
static void	wm_stop(struct ifnet *, int);

static void	wm_shutdown(void *);

static void	wm_reset(struct wm_softc *);
static void	wm_rxdrain(struct wm_softc *);
static int	wm_add_rxbuf(struct wm_softc *, int);
static int	wm_read_eeprom(struct wm_softc *, int, int, u_int16_t *);
static void	wm_tick(void *);

static void	wm_set_filter(struct wm_softc *);

static int	wm_intr(void *);
static void	wm_txintr(struct wm_softc *);
static void	wm_rxintr(struct wm_softc *);
static void	wm_linkintr(struct wm_softc *, uint32_t);

static void	wm_tbi_mediainit(struct wm_softc *);
static int	wm_tbi_mediachange(struct ifnet *);
static void	wm_tbi_mediastatus(struct ifnet *, struct ifmediareq *);

static void	wm_tbi_set_linkled(struct wm_softc *);
static void	wm_tbi_check_link(struct wm_softc *);

static void	wm_gmii_reset(struct wm_softc *);

static int	wm_gmii_i82543_readreg(struct device *, int, int);
static void	wm_gmii_i82543_writereg(struct device *, int, int, int);

static int	wm_gmii_i82544_readreg(struct device *, int, int);
static void	wm_gmii_i82544_writereg(struct device *, int, int, int);

static void	wm_gmii_statchg(struct device *);

static void	wm_gmii_mediainit(struct wm_softc *);
static int	wm_gmii_mediachange(struct ifnet *);
static void	wm_gmii_mediastatus(struct ifnet *, struct ifmediareq *);

static int	wm_match(struct device *, struct cfdata *, void *);
static void	wm_attach(struct device *, struct device *, void *);

CFATTACH_DECL(wm, sizeof(struct wm_softc),
    wm_match, wm_attach, NULL, NULL);

static void	wm_82547_txfifo_stall(void *);

/*
 * Devices supported by this driver.
 */
const struct wm_product {
	pci_vendor_id_t		wmp_vendor;
	pci_product_id_t	wmp_product;
	const char		*wmp_name;
	wm_chip_type		wmp_type;
	int			wmp_flags;
#define	WMP_F_1000X		0x01
#define	WMP_F_1000T		0x02
} wm_products[] = {
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82542,
	  "Intel i82542 1000BASE-X Ethernet",
	  WM_T_82542_2_1,	WMP_F_1000X },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82543GC_FIBER,
	  "Intel i82543GC 1000BASE-X Ethernet",
	  WM_T_82543,		WMP_F_1000X },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82543GC_COPPER,
	  "Intel i82543GC 1000BASE-T Ethernet",
	  WM_T_82543,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82544EI_COPPER,
	  "Intel i82544EI 1000BASE-T Ethernet",
	  WM_T_82544,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82544EI_FIBER,
	  "Intel i82544EI 1000BASE-X Ethernet",
	  WM_T_82544,		WMP_F_1000X },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82544GC_COPPER,
	  "Intel i82544GC 1000BASE-T Ethernet",
	  WM_T_82544,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82544GC_LOM,
	  "Intel i82544GC (LOM) 1000BASE-T Ethernet",
	  WM_T_82544,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82540EM,
	  "Intel i82540EM 1000BASE-T Ethernet",
	  WM_T_82540,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82540EM_LOM,
	  "Intel i82540EM (LOM) 1000BASE-T Ethernet",
	  WM_T_82540,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82540EP_LOM,
	  "Intel i82540EP 1000BASE-T Ethernet",
	  WM_T_82540,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82540EP,
	  "Intel i82540EP 1000BASE-T Ethernet",
	  WM_T_82540,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82540EP_LP,
	  "Intel i82540EP 1000BASE-T Ethernet",
	  WM_T_82540,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82545EM_COPPER,
	  "Intel i82545EM 1000BASE-T Ethernet",
	  WM_T_82545,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82545GM_COPPER,
	  "Intel i82545GM 1000BASE-T Ethernet",
	  WM_T_82545_3,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82545GM_FIBER,
	  "Intel i82545GM 1000BASE-X Ethernet",
	  WM_T_82545_3,		WMP_F_1000X },
#if 0
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82545GM_SERDES,
	  "Intel i82545GM Gigabit Ethernet (SERDES)",
	  WM_T_82545_3,		WMP_F_SERDES },
#endif
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546EB_COPPER,
	  "Intel i82546EB 1000BASE-T Ethernet",
	  WM_T_82546,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,     PCI_PRODUCT_INTEL_82546EB_QUAD,
	  "Intel i82546EB 1000BASE-T Ethernet",
	  WM_T_82546,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82545EM_FIBER,
	  "Intel i82545EM 1000BASE-X Ethernet",
	  WM_T_82545,		WMP_F_1000X },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546EB_FIBER,
	  "Intel i82546EB 1000BASE-X Ethernet",
	  WM_T_82546,		WMP_F_1000X },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546GB_COPPER,
	  "Intel i82546GB 1000BASE-T Ethernet",
	  WM_T_82546_3,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546GB_FIBER,
	  "Intel i82546GB 1000BASE-X Ethernet",
	  WM_T_82546_3,		WMP_F_1000X },
#if 0
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546GB_SERDES,
	  "Intel i82546GB Gigabit Ethernet (SERDES)",
	  WM_T_82546_3,		WMP_F_SERDES },
#endif
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541EI,
	  "Intel i82541EI 1000BASE-T Ethernet",
	  WM_T_82541,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541EI_MOBILE,
	  "Intel i82541EI Mobile 1000BASE-T Ethernet",
	  WM_T_82541,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541ER,
	  "Intel i82541ER 1000BASE-T Ethernet",
	  WM_T_82541_2,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541GI,
	  "Intel i82541GI 1000BASE-T Ethernet",
	  WM_T_82541_2,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541GI_MOBILE,
	  "Intel i82541GI Mobile 1000BASE-T Ethernet",
	  WM_T_82541_2,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82547EI,
	  "Intel i82547EI 1000BASE-T Ethernet",
	  WM_T_82547,		WMP_F_1000T },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82547GI,
	  "Intel i82547GI 1000BASE-T Ethernet",
	  WM_T_82547_2,		WMP_F_1000T },
	{ 0,			0,
	  NULL,
	  0,			0 },
};

#ifdef WM_EVENT_COUNTERS
#if WM_NTXSEGS != 40
#error Update wm_txseg_evcnt_names
#endif
static const char *wm_txseg_evcnt_names[WM_NTXSEGS] = {
	"txseg1",
	"txseg2",
	"txseg3",
	"txseg4",
	"txseg5",
	"txseg6",
	"txseg7",
	"txseg8",
	"txseg9",
	"txseg10",
	"txseg11",
	"txseg12",
	"txseg13",
	"txseg14",
	"txseg15",
	"txseg16",
	"txseg17",
	"txseg18",
	"txseg19",
	"txseg20",
	"txseg21",
	"txseg22",
	"txseg23",
	"txseg24",
	"txseg25",
	"txseg26",
	"txseg27",
	"txseg28",
	"txseg29",
	"txseg30",
	"txseg31",
	"txseg32",
	"txseg33",
	"txseg34",
	"txseg35",
	"txseg36",
	"txseg37",
	"txseg38",
	"txseg39",
	"txseg40",
};
#endif /* WM_EVENT_COUNTERS */

#if 0 /* Not currently used */
static __inline uint32_t
wm_io_read(struct wm_softc *sc, int reg)
{

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, 0, reg);
	return (bus_space_read_4(sc->sc_iot, sc->sc_ioh, 4));
}
#endif

static __inline void
wm_io_write(struct wm_softc *sc, int reg, uint32_t val)
{

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, 0, reg);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, 4, val);
}

static const struct wm_product *
wm_lookup(const struct pci_attach_args *pa)
{
	const struct wm_product *wmp;

	for (wmp = wm_products; wmp->wmp_name != NULL; wmp++) {
		if (PCI_VENDOR(pa->pa_id) == wmp->wmp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == wmp->wmp_product)
			return (wmp);
	}
	return (NULL);
}

static int
wm_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (wm_lookup(pa) != NULL)
		return (1);

	return (0);
}

static void
wm_attach(struct device *parent, struct device *self, void *aux)
{
	struct wm_softc *sc = (void *) self;
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	const char *eetype;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_dma_segment_t seg;
	int memh_valid;
	int i, rseg, error;
	const struct wm_product *wmp;
	uint8_t enaddr[ETHER_ADDR_LEN];
	uint16_t myea[ETHER_ADDR_LEN / 2], cfg1, cfg2, swdpin;
	pcireg_t preg, memtype;
	uint32_t reg;
	int pmreg;

	callout_init(&sc->sc_tick_ch);

	wmp = wm_lookup(pa);
	if (wmp == NULL) {
		printf("\n");
		panic("wm_attach: impossible");
	}

	sc->sc_dmat = pa->pa_dmat;

	preg = PCI_REVISION(pci_conf_read(pc, pa->pa_tag, PCI_CLASS_REG));
	aprint_naive(": Ethernet controller\n");
	aprint_normal(": %s, rev. %d\n", wmp->wmp_name, preg);

	sc->sc_type = wmp->wmp_type;
	if (sc->sc_type < WM_T_82543) {
		if (preg < 2) {
			aprint_error("%s: i82542 must be at least rev. 2\n",
			    sc->sc_dev.dv_xname);
			return;
		}
		if (preg < 3)
			sc->sc_type = WM_T_82542_2_0;
	}

	/*
	 * Map the device.  All devices support memory-mapped acccess,
	 * and it is really required for normal operation.
	 */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, WM_PCI_MMBA);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		memh_valid = (pci_mapreg_map(pa, WM_PCI_MMBA,
		    memtype, 0, &memt, &memh, NULL, NULL) == 0);
		break;
	default:
		memh_valid = 0;
	}

	if (memh_valid) {
		sc->sc_st = memt;
		sc->sc_sh = memh;
	} else {
		aprint_error("%s: unable to map device registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * In addition, i82544 and later support I/O mapped indirect
	 * register access.  It is not desirable (nor supported in
	 * this driver) to use it for normal operation, though it is
	 * required to work around bugs in some chip versions.
	 */
	if (sc->sc_type >= WM_T_82544) {
		/* First we have to find the I/O BAR. */
		for (i = PCI_MAPREG_START; i < PCI_MAPREG_END; i += 4) {
			if (pci_mapreg_type(pa->pa_pc, pa->pa_tag, i) ==
			    PCI_MAPREG_TYPE_IO)
				break;
		}
		if (i == PCI_MAPREG_END)
			aprint_error("%s: WARNING: unable to find I/O BAR\n",
			    sc->sc_dev.dv_xname);
		else if (pci_mapreg_map(pa, i, PCI_MAPREG_TYPE_IO,
					0, &sc->sc_iot, &sc->sc_ioh,
					NULL, NULL) == 0)
			sc->sc_flags |= WM_F_IOH_VALID;
		else
			aprint_error("%s: WARNING: unable to map I/O space\n",
			    sc->sc_dev.dv_xname);
	}

	/* Enable bus mastering.  Disable MWI on the i82542 2.0. */
	preg = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	preg |= PCI_COMMAND_MASTER_ENABLE;
	if (sc->sc_type < WM_T_82542_2_1)
		preg &= ~PCI_COMMAND_INVALIDATE_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, preg);

	/* Get it out of power save mode, if needed. */
	if (pci_get_capability(pc, pa->pa_tag, PCI_CAP_PWRMGMT, &pmreg, 0)) {
		preg = pci_conf_read(pc, pa->pa_tag, pmreg + PCI_PMCSR) &
		    PCI_PMCSR_STATE_MASK;
		if (preg == PCI_PMCSR_STATE_D3) {
			/*
			 * The card has lost all configuration data in
			 * this state, so punt.
			 */
			aprint_error("%s: unable to wake from power state D3\n",
			    sc->sc_dev.dv_xname);
			return;
		}
		if (preg != PCI_PMCSR_STATE_D0) {
			aprint_normal("%s: waking up from power state D%d\n",
			    sc->sc_dev.dv_xname, preg);
			pci_conf_write(pc, pa->pa_tag, pmreg + PCI_PMCSR,
			    PCI_PMCSR_STATE_D0);
		}
	}

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error("%s: unable to map interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, wm_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error("%s: unable to establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			aprint_normal(" at %s", intrstr);
		aprint_normal("\n");
		return;
	}
	aprint_normal("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	/*
	 * Determine a few things about the bus we're connected to.
	 */
	if (sc->sc_type < WM_T_82543) {
		/* We don't really know the bus characteristics here. */
		sc->sc_bus_speed = 33;
	} else if (sc->sc_type == WM_T_82547 || sc->sc_type == WM_T_82547_2) {
		/*
		 * CSA (Communication Streaming Architecture) is about as fast
		 * a 32-bit 66MHz PCI Bus.
		 */
		sc->sc_flags |= WM_F_CSA;
		sc->sc_bus_speed = 66;
		aprint_verbose("%s: Communication Streaming Architecture\n",
		    sc->sc_dev.dv_xname);
		if (sc->sc_type == WM_T_82547) {
			callout_init(&sc->sc_txfifo_ch);
			callout_setfunc(&sc->sc_txfifo_ch,
					wm_82547_txfifo_stall, sc);
			aprint_verbose("%s: using 82547 Tx FIFO stall "
				       "work-around\n", sc->sc_dev.dv_xname);
		}
	} else {
		reg = CSR_READ(sc, WMREG_STATUS);
		if (reg & STATUS_BUS64)
			sc->sc_flags |= WM_F_BUS64;
		if (sc->sc_type >= WM_T_82544 &&
		    (reg & STATUS_PCIX_MODE) != 0) {
			pcireg_t pcix_cmd, pcix_sts, bytecnt, maxb;

			sc->sc_flags |= WM_F_PCIX;
			if (pci_get_capability(pa->pa_pc, pa->pa_tag,
					       PCI_CAP_PCIX,
					       &sc->sc_pcix_offset, NULL) == 0)
				aprint_error("%s: unable to find PCIX "
				    "capability\n", sc->sc_dev.dv_xname);
			else if (sc->sc_type != WM_T_82545_3 &&
				 sc->sc_type != WM_T_82546_3) {
				/*
				 * Work around a problem caused by the BIOS
				 * setting the max memory read byte count
				 * incorrectly.
				 */
				pcix_cmd = pci_conf_read(pa->pa_pc, pa->pa_tag,
				    sc->sc_pcix_offset + PCI_PCIX_CMD);
				pcix_sts = pci_conf_read(pa->pa_pc, pa->pa_tag,
				    sc->sc_pcix_offset + PCI_PCIX_STATUS);

				bytecnt =
				    (pcix_cmd & PCI_PCIX_CMD_BYTECNT_MASK) >>
				    PCI_PCIX_CMD_BYTECNT_SHIFT;
				maxb =
				    (pcix_sts & PCI_PCIX_STATUS_MAXB_MASK) >>
				    PCI_PCIX_STATUS_MAXB_SHIFT;
				if (bytecnt > maxb) {
					aprint_verbose("%s: resetting PCI-X "
					    "MMRBC: %d -> %d\n",
					    sc->sc_dev.dv_xname,
					    512 << bytecnt, 512 << maxb);
					pcix_cmd = (pcix_cmd &
					    ~PCI_PCIX_CMD_BYTECNT_MASK) |
					   (maxb << PCI_PCIX_CMD_BYTECNT_SHIFT);
					pci_conf_write(pa->pa_pc, pa->pa_tag,
					    sc->sc_pcix_offset + PCI_PCIX_CMD,
					    pcix_cmd);
				}
			}
		}
		/*
		 * The quad port adapter is special; it has a PCIX-PCIX
		 * bridge on the board, and can run the secondary bus at
		 * a higher speed.
		 */
		if (wmp->wmp_product == PCI_PRODUCT_INTEL_82546EB_QUAD) {
			sc->sc_bus_speed = (sc->sc_flags & WM_F_PCIX) ? 120
								      : 66;
		} else if (sc->sc_flags & WM_F_PCIX) {
			switch (reg & STATUS_PCIXSPD_MASK) {
			case STATUS_PCIXSPD_50_66:
				sc->sc_bus_speed = 66;
				break;
			case STATUS_PCIXSPD_66_100:
				sc->sc_bus_speed = 100;
				break;
			case STATUS_PCIXSPD_100_133:
				sc->sc_bus_speed = 133;
				break;
			default:
				aprint_error(
				    "%s: unknown PCIXSPD %d; assuming 66MHz\n",
				    sc->sc_dev.dv_xname,
				    reg & STATUS_PCIXSPD_MASK);
				sc->sc_bus_speed = 66;
			}
		} else
			sc->sc_bus_speed = (reg & STATUS_PCI66) ? 66 : 33;
		aprint_verbose("%s: %d-bit %dMHz %s bus\n", sc->sc_dev.dv_xname,
		    (sc->sc_flags & WM_F_BUS64) ? 64 : 32, sc->sc_bus_speed,
		    (sc->sc_flags & WM_F_PCIX) ? "PCIX" : "PCI");
	}

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct wm_control_data), PAGE_SIZE, 0, &seg, 1, &rseg,
	    0)) != 0) {
		aprint_error(
		    "%s: unable to allocate control data, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    sizeof(struct wm_control_data), (caddr_t *)&sc->sc_control_data,
	    0)) != 0) {
		aprint_error("%s: unable to map control data, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct wm_control_data), 1,
	    sizeof(struct wm_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		aprint_error("%s: unable to create control data DMA map, "
		    "error = %d\n", sc->sc_dev.dv_xname, error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct wm_control_data), NULL,
	    0)) != 0) {
		aprint_error(
		    "%s: unable to load control data DMA map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_3;
	}


	/*
	 * Create the transmit buffer DMA maps.
	 */
	WM_TXQUEUELEN(sc) =
	    (sc->sc_type == WM_T_82547 || sc->sc_type == WM_T_82547_2) ?
	    WM_TXQUEUELEN_MAX_82547 : WM_TXQUEUELEN_MAX;
	for (i = 0; i < WM_TXQUEUELEN(sc); i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, ETHER_MAX_LEN_JUMBO,
		    WM_NTXSEGS, WTX_MAX_LEN, 0, 0,
		    &sc->sc_txsoft[i].txs_dmamap)) != 0) {
			aprint_error("%s: unable to create Tx DMA map %d, "
			    "error = %d\n", sc->sc_dev.dv_xname, i, error);
			goto fail_4;
		}
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < WM_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
			aprint_error("%s: unable to create Rx DMA map %d, "
			    "error = %d\n", sc->sc_dev.dv_xname, i, error);
			goto fail_5;
		}
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	/*
	 * Reset the chip to a known state.
	 */
	wm_reset(sc);

	/*
	 * Get some information about the EEPROM.
	 */
	if (sc->sc_type >= WM_T_82540)
		sc->sc_flags |= WM_F_EEPROM_HANDSHAKE;
	if (sc->sc_type <= WM_T_82544)
		sc->sc_ee_addrbits = 6;
	else if (sc->sc_type <= WM_T_82546_3) {
		reg = CSR_READ(sc, WMREG_EECD);
		if (reg & EECD_EE_SIZE)
			sc->sc_ee_addrbits = 8;
		else
			sc->sc_ee_addrbits = 6;
	} else if (sc->sc_type <= WM_T_82547_2) {
		reg = CSR_READ(sc, WMREG_EECD);
		if (reg & EECD_EE_TYPE) {
			sc->sc_flags |= WM_F_EEPROM_SPI;
			sc->sc_ee_addrbits = (reg & EECD_EE_ABITS) ? 16 : 8;
		} else
			sc->sc_ee_addrbits = (reg & EECD_EE_ABITS) ? 8 : 6;
	} else {
		/* Assume everything else is SPI. */
		reg = CSR_READ(sc, WMREG_EECD);
		sc->sc_flags |= WM_F_EEPROM_SPI;
		sc->sc_ee_addrbits = (reg & EECD_EE_ABITS) ? 16 : 8;
	}
	if (sc->sc_flags & WM_F_EEPROM_SPI)
		eetype = "SPI";
	else
		eetype = "MicroWire";
	aprint_verbose("%s: %u word (%d address bits) %s EEPROM\n",
	    sc->sc_dev.dv_xname, 1U << sc->sc_ee_addrbits,
	    sc->sc_ee_addrbits, eetype);

	/*
	 * Read the Ethernet address from the EEPROM.
	 */
	if (wm_read_eeprom(sc, EEPROM_OFF_MACADDR,
	    sizeof(myea) / sizeof(myea[0]), myea)) {
		aprint_error("%s: unable to read Ethernet address\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	enaddr[0] = myea[0] & 0xff;
	enaddr[1] = myea[0] >> 8;
	enaddr[2] = myea[1] & 0xff;
	enaddr[3] = myea[1] >> 8;
	enaddr[4] = myea[2] & 0xff;
	enaddr[5] = myea[2] >> 8;

	/*
	 * Toggle the LSB of the MAC address on the second port
	 * of the i82546.
	 */
	if (sc->sc_type == WM_T_82546 || sc->sc_type == WM_T_82546_3) {
		if ((CSR_READ(sc, WMREG_STATUS) >> STATUS_FUNCID_SHIFT) & 1)
			enaddr[5] ^= 1;
	}

	aprint_normal("%s: Ethernet address %s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(enaddr));

	/*
	 * Read the config info from the EEPROM, and set up various
	 * bits in the control registers based on their contents.
	 */
	if (wm_read_eeprom(sc, EEPROM_OFF_CFG1, 1, &cfg1)) {
		aprint_error("%s: unable to read CFG1 from EEPROM\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	if (wm_read_eeprom(sc, EEPROM_OFF_CFG2, 1, &cfg2)) {
		aprint_error("%s: unable to read CFG2 from EEPROM\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	if (sc->sc_type >= WM_T_82544) {
		if (wm_read_eeprom(sc, EEPROM_OFF_SWDPIN, 1, &swdpin)) {
			aprint_error("%s: unable to read SWDPIN from EEPROM\n",
			    sc->sc_dev.dv_xname);
			return;
		}
	}

	if (cfg1 & EEPROM_CFG1_ILOS)
		sc->sc_ctrl |= CTRL_ILOS;
	if (sc->sc_type >= WM_T_82544) {
		sc->sc_ctrl |=
		    ((swdpin >> EEPROM_SWDPIN_SWDPIO_SHIFT) & 0xf) <<
		    CTRL_SWDPIO_SHIFT;
		sc->sc_ctrl |=
		    ((swdpin >> EEPROM_SWDPIN_SWDPIN_SHIFT) & 0xf) <<
		    CTRL_SWDPINS_SHIFT;
	} else {
		sc->sc_ctrl |=
		    ((cfg1 >> EEPROM_CFG1_SWDPIO_SHIFT) & 0xf) <<
		    CTRL_SWDPIO_SHIFT;
	}

#if 0
	if (sc->sc_type >= WM_T_82544) {
		if (cfg1 & EEPROM_CFG1_IPS0)
			sc->sc_ctrl_ext |= CTRL_EXT_IPS;
		if (cfg1 & EEPROM_CFG1_IPS1)
			sc->sc_ctrl_ext |= CTRL_EXT_IPS1;
		sc->sc_ctrl_ext |=
		    ((swdpin >> (EEPROM_SWDPIN_SWDPIO_SHIFT + 4)) & 0xd) <<
		    CTRL_EXT_SWDPIO_SHIFT;
		sc->sc_ctrl_ext |=
		    ((swdpin >> (EEPROM_SWDPIN_SWDPIN_SHIFT + 4)) & 0xd) <<
		    CTRL_EXT_SWDPINS_SHIFT;
	} else {
		sc->sc_ctrl_ext |=
		    ((cfg2 >> EEPROM_CFG2_SWDPIO_SHIFT) & 0xf) <<
		    CTRL_EXT_SWDPIO_SHIFT;
	}
#endif

	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
#if 0
	CSR_WRITE(sc, WMREG_CTRL_EXT, sc->sc_ctrl_ext);
#endif

	/*
	 * Set up some register offsets that are different between
	 * the i82542 and the i82543 and later chips.
	 */
	if (sc->sc_type < WM_T_82543) {
		sc->sc_rdt_reg = WMREG_OLD_RDT0;
		sc->sc_tdt_reg = WMREG_OLD_TDT;
	} else {
		sc->sc_rdt_reg = WMREG_RDT;
		sc->sc_tdt_reg = WMREG_TDT;
	}

	/*
	 * Determine if we should use flow control.  We should
	 * always use it, unless we're on a i82542 < 2.1.
	 */
	if (sc->sc_type >= WM_T_82542_2_1)
		sc->sc_ctrl |= CTRL_TFCE | CTRL_RFCE;

	/*
	 * Determine if we're TBI or GMII mode, and initialize the
	 * media structures accordingly.
	 */
	if (sc->sc_type < WM_T_82543 ||
	    (CSR_READ(sc, WMREG_STATUS) & STATUS_TBIMODE) != 0) {
		if (wmp->wmp_flags & WMP_F_1000T)
			aprint_error("%s: WARNING: TBIMODE set on 1000BASE-T "
			    "product!\n", sc->sc_dev.dv_xname);
		wm_tbi_mediainit(sc);
	} else {
		if (wmp->wmp_flags & WMP_F_1000X)
			aprint_error("%s: WARNING: TBIMODE clear on 1000BASE-X "
			    "product!\n", sc->sc_dev.dv_xname);
		wm_gmii_mediainit(sc);
	}

	ifp = &sc->sc_ethercom.ec_if;
	strcpy(ifp->if_xname, sc->sc_dev.dv_xname);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = wm_ioctl;
	ifp->if_start = wm_start;
	ifp->if_watchdog = wm_watchdog;
	ifp->if_init = wm_init;
	ifp->if_stop = wm_stop;
	IFQ_SET_MAXLEN(&ifp->if_snd, max(WM_IFQUEUELEN, IFQ_MAXLEN));
	IFQ_SET_READY(&ifp->if_snd);

	sc->sc_ethercom.ec_capabilities |= ETHERCAP_JUMBO_MTU;

	/*
	 * If we're a i82543 or greater, we can support VLANs.
	 */
	if (sc->sc_type >= WM_T_82543)
		sc->sc_ethercom.ec_capabilities |=
		    ETHERCAP_VLAN_MTU /* XXXJRT | ETHERCAP_VLAN_HWTAGGING */;

	/*
	 * We can perform TCPv4 and UDPv4 checkums in-bound.  Only
	 * on i82543 and later.
	 */
	if (sc->sc_type >= WM_T_82543)
		ifp->if_capabilities |=
		    IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);
#if NRND > 0
	rnd_attach_source(&sc->rnd_source, sc->sc_dev.dv_xname,
	    RND_TYPE_NET, 0);
#endif

#ifdef WM_EVENT_COUNTERS
	/* Attach event counters. */
	evcnt_attach_dynamic(&sc->sc_ev_txsstall, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txsstall");
	evcnt_attach_dynamic(&sc->sc_ev_txdstall, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txdstall");
 	evcnt_attach_dynamic(&sc->sc_ev_txfifo_stall, EVCNT_TYPE_MISC,
 	    NULL, sc->sc_dev.dv_xname, "txfifo_stall");
	evcnt_attach_dynamic(&sc->sc_ev_txforceintr, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txforceintr");
	evcnt_attach_dynamic(&sc->sc_ev_txdw, EVCNT_TYPE_INTR,
	    NULL, sc->sc_dev.dv_xname, "txdw");
	evcnt_attach_dynamic(&sc->sc_ev_txqe, EVCNT_TYPE_INTR,
	    NULL, sc->sc_dev.dv_xname, "txqe");
	evcnt_attach_dynamic(&sc->sc_ev_rxintr, EVCNT_TYPE_INTR,
	    NULL, sc->sc_dev.dv_xname, "rxintr");
	evcnt_attach_dynamic(&sc->sc_ev_linkintr, EVCNT_TYPE_INTR,
	    NULL, sc->sc_dev.dv_xname, "linkintr");

	evcnt_attach_dynamic(&sc->sc_ev_rxipsum, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "rxipsum");
	evcnt_attach_dynamic(&sc->sc_ev_rxtusum, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "rxtusum");
	evcnt_attach_dynamic(&sc->sc_ev_txipsum, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txipsum");
	evcnt_attach_dynamic(&sc->sc_ev_txtusum, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txtusum");

	evcnt_attach_dynamic(&sc->sc_ev_txctx_init, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txctx init");
	evcnt_attach_dynamic(&sc->sc_ev_txctx_hit, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txctx hit");
	evcnt_attach_dynamic(&sc->sc_ev_txctx_miss, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txctx miss");

	for (i = 0; i < WM_NTXSEGS; i++)
		evcnt_attach_dynamic(&sc->sc_ev_txseg[i], EVCNT_TYPE_MISC,
		    NULL, sc->sc_dev.dv_xname, wm_txseg_evcnt_names[i]);

	evcnt_attach_dynamic(&sc->sc_ev_txdrop, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "txdrop");

	evcnt_attach_dynamic(&sc->sc_ev_tu, EVCNT_TYPE_MISC,
	    NULL, sc->sc_dev.dv_xname, "tu");
#endif /* WM_EVENT_COUNTERS */

	/*
	 * Make sure the interface is shutdown during reboot.
	 */
	sc->sc_sdhook = shutdownhook_establish(wm_shutdown, sc);
	if (sc->sc_sdhook == NULL)
		aprint_error("%s: WARNING: unable to establish shutdown hook\n",
		    sc->sc_dev.dv_xname);
	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_5:
	for (i = 0; i < WM_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_rxsoft[i].rxs_dmamap);
	}
 fail_4:
	for (i = 0; i < WM_TXQUEUELEN(sc); i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_control_data,
	    sizeof(struct wm_control_data));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
 fail_0:
	return;
}

/*
 * wm_shutdown:
 *
 *	Make sure the interface is stopped at reboot time.
 */
static void
wm_shutdown(void *arg)
{
	struct wm_softc *sc = arg;

	wm_stop(&sc->sc_ethercom.ec_if, 1);
}

/*
 * wm_tx_cksum:
 *
 *	Set up TCP/IP checksumming parameters for the
 *	specified packet.
 */
static int
wm_tx_cksum(struct wm_softc *sc, struct wm_txsoft *txs, uint32_t *cmdp,
    uint8_t *fieldsp)
{
	struct mbuf *m0 = txs->txs_mbuf;
	struct livengood_tcpip_ctxdesc *t;
	uint32_t ipcs, tucs;
	struct ip *ip;
	struct ether_header *eh;
	int offset, iphl;
	uint8_t fields = 0;

	/*
	 * XXX It would be nice if the mbuf pkthdr had offset
	 * fields for the protocol headers.
	 */

	eh = mtod(m0, struct ether_header *);
	switch (htons(eh->ether_type)) {
	case ETHERTYPE_IP:
		iphl = sizeof(struct ip);
		offset = ETHER_HDR_LEN;
		break;

	case ETHERTYPE_VLAN:
		iphl = sizeof(struct ip);
		offset = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		break;

	default:
		/*
		 * Don't support this protocol or encapsulation.
		 */
		*fieldsp = 0;
		*cmdp = 0;
		return (0);
	}

	if (m0->m_len < (offset + iphl)) {
		if ((txs->txs_mbuf = m_pullup(m0, offset + iphl)) == NULL) {
			printf("%s: wm_tx_cksum: mbuf allocation failed, "
			    "packet dropped\n", sc->sc_dev.dv_xname);
			return (ENOMEM);
		}
		m0 = txs->txs_mbuf;
	}

	ip = (struct ip *) (mtod(m0, caddr_t) + offset);
	iphl = ip->ip_hl << 2;

	/*
	 * NOTE: Even if we're not using the IP or TCP/UDP checksum
	 * offload feature, if we load the context descriptor, we
	 * MUST provide valid values for IPCSS and TUCSS fields.
	 */

	if (m0->m_pkthdr.csum_flags & M_CSUM_IPv4) {
		WM_EVCNT_INCR(&sc->sc_ev_txipsum);
		fields |= WTX_IXSM;
		ipcs = WTX_TCPIP_IPCSS(offset) |
		    WTX_TCPIP_IPCSO(offset + offsetof(struct ip, ip_sum)) |
		    WTX_TCPIP_IPCSE(offset + iphl - 1);
	} else if (__predict_true(sc->sc_txctx_ipcs != 0xffffffff)) {
		/* Use the cached value. */
		ipcs = sc->sc_txctx_ipcs;
	} else {
		/* Just initialize it to the likely value anyway. */
		ipcs = WTX_TCPIP_IPCSS(offset) |
		    WTX_TCPIP_IPCSO(offset + offsetof(struct ip, ip_sum)) |
		    WTX_TCPIP_IPCSE(offset + iphl - 1);
	}

	offset += iphl;

	if (m0->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_UDPv4)) {
		WM_EVCNT_INCR(&sc->sc_ev_txtusum);
		fields |= WTX_TXSM;
		tucs = WTX_TCPIP_TUCSS(offset) |
		    WTX_TCPIP_TUCSO(offset + m0->m_pkthdr.csum_data) |
		    WTX_TCPIP_TUCSE(0) /* rest of packet */;
	} else if (__predict_true(sc->sc_txctx_tucs != 0xffffffff)) {
		/* Use the cached value. */
		tucs = sc->sc_txctx_tucs;
	} else {
		/* Just initialize it to a valid TCP context. */
		tucs = WTX_TCPIP_TUCSS(offset) |
		    WTX_TCPIP_TUCSO(offset + offsetof(struct tcphdr, th_sum)) |
		    WTX_TCPIP_TUCSE(0) /* rest of packet */;
	}

	if (sc->sc_txctx_ipcs == ipcs &&
	    sc->sc_txctx_tucs == tucs) {
		/* Cached context is fine. */
		WM_EVCNT_INCR(&sc->sc_ev_txctx_hit);
	} else {
		/* Fill in the context descriptor. */
#ifdef WM_EVENT_COUNTERS
		if (sc->sc_txctx_ipcs == 0xffffffff &&
		    sc->sc_txctx_tucs == 0xffffffff)
			WM_EVCNT_INCR(&sc->sc_ev_txctx_init);
		else
			WM_EVCNT_INCR(&sc->sc_ev_txctx_miss);
#endif
		t = (struct livengood_tcpip_ctxdesc *)
		    &sc->sc_txdescs[sc->sc_txnext];
		t->tcpip_ipcs = htole32(ipcs);
		t->tcpip_tucs = htole32(tucs);
		t->tcpip_cmdlen = htole32(WTX_CMD_DEXT | WTX_DTYP_C);
		t->tcpip_seg = 0;
		WM_CDTXSYNC(sc, sc->sc_txnext, 1, BUS_DMASYNC_PREWRITE);

		sc->sc_txctx_ipcs = ipcs;
		sc->sc_txctx_tucs = tucs;

		sc->sc_txnext = WM_NEXTTX(sc->sc_txnext);
		txs->txs_ndesc++;
	}

	*cmdp = WTX_CMD_DEXT | WTX_DTYP_D;
	*fieldsp = fields;

	return (0);
}

/*
 * wm_82547_txfifo_stall:
 *
 *	Callout used to wait for the 82547 Tx FIFO to drain,
 *	reset the FIFO pointers, and restart packet transmission.
 */
static void
wm_82547_txfifo_stall(void *arg)
{
	struct wm_softc *sc = arg;
	int s;

	s = splnet();

	if (sc->sc_txfifo_stall) {
		if (CSR_READ(sc, WMREG_TDT) == CSR_READ(sc, WMREG_TDH) &&
		    CSR_READ(sc, WMREG_TDFT) == CSR_READ(sc, WMREG_TDFH) &&
		    CSR_READ(sc, WMREG_TDFTS) == CSR_READ(sc, WMREG_TDFHS)) {
			/*
			 * Packets have drained.  Stop transmitter, reset
			 * FIFO pointers, restart transmitter, and kick
			 * the packet queue.
			 */
			uint32_t tctl = CSR_READ(sc, WMREG_TCTL);
			CSR_WRITE(sc, WMREG_TCTL, tctl & ~TCTL_EN);
			CSR_WRITE(sc, WMREG_TDFT, sc->sc_txfifo_addr);
			CSR_WRITE(sc, WMREG_TDFH, sc->sc_txfifo_addr);
			CSR_WRITE(sc, WMREG_TDFTS, sc->sc_txfifo_addr);
			CSR_WRITE(sc, WMREG_TDFHS, sc->sc_txfifo_addr);
			CSR_WRITE(sc, WMREG_TCTL, tctl);
			CSR_WRITE_FLUSH(sc);

			sc->sc_txfifo_head = 0;
			sc->sc_txfifo_stall = 0;
			wm_start(&sc->sc_ethercom.ec_if);
		} else {
			/*
			 * Still waiting for packets to drain; try again in
			 * another tick.
			 */
			callout_schedule(&sc->sc_txfifo_ch, 1);
		}
	}

	splx(s);
}

/*
 * wm_82547_txfifo_bugchk:
 *
 *	Check for bug condition in the 82547 Tx FIFO.  We need to
 *	prevent enqueueing a packet that would wrap around the end
 *	if the Tx FIFO ring buffer, otherwise the chip will croak.
 *
 *	We do this by checking the amount of space before the end
 *	of the Tx FIFO buffer.  If the packet will not fit, we "stall"
 *	the Tx FIFO, wait for all remaining packets to drain, reset
 *	the internal FIFO pointers to the beginning, and restart
 *	transmission on the interface.
 */
#define	WM_FIFO_HDR		0x10
#define	WM_82547_PAD_LEN	0x3e0
static int
wm_82547_txfifo_bugchk(struct wm_softc *sc, struct mbuf *m0)
{
	int space = sc->sc_txfifo_size - sc->sc_txfifo_head;
	int len = roundup(m0->m_pkthdr.len + WM_FIFO_HDR, WM_FIFO_HDR);

	/* Just return if already stalled. */
	if (sc->sc_txfifo_stall)
		return (1);

	if (sc->sc_mii.mii_media_active & IFM_FDX) {
		/* Stall only occurs in half-duplex mode. */
		goto send_packet;
	}

	if (len >= WM_82547_PAD_LEN + space) {
		sc->sc_txfifo_stall = 1;
		callout_schedule(&sc->sc_txfifo_ch, 1);
		return (1);
	}

 send_packet:
	sc->sc_txfifo_head += len;
	if (sc->sc_txfifo_head >= sc->sc_txfifo_size)
		sc->sc_txfifo_head -= sc->sc_txfifo_size;

	return (0);
}

/*
 * wm_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
static void
wm_start(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;
	struct mbuf *m0;
#if 0 /* XXXJRT */
	struct m_tag *mtag;
#endif
	struct wm_txsoft *txs;
	bus_dmamap_t dmamap;
	int error, nexttx, lasttx = -1, ofree, seg;
	uint32_t cksumcmd;
	uint8_t cksumfields;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous number of free descriptors.
	 */
	ofree = sc->sc_txfree;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	for (;;) {
		/* Grab a packet off the queue. */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: have packet to transmit: %p\n",
		    sc->sc_dev.dv_xname, m0));

		/* Get a work queue entry. */
		if (sc->sc_txsfree < WM_TXQUEUE_GC(sc)) {
			wm_txintr(sc);
			if (sc->sc_txsfree == 0) {
				DPRINTF(WM_DEBUG_TX,
				    ("%s: TX: no free job descriptors\n",
					sc->sc_dev.dv_xname));
				WM_EVCNT_INCR(&sc->sc_ev_txsstall);
				break;
			}
		}

		txs = &sc->sc_txsoft[sc->sc_txsnext];
		dmamap = txs->txs_dmamap;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the allotted number of segments, or we
		 * were short on resources.  For the too-many-segments
		 * case, we simply report an error and drop the packet,
		 * since we can't sanely copy a jumbo packet to a single
		 * buffer.
		 */
		error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
		if (error) {
			if (error == EFBIG) {
				WM_EVCNT_INCR(&sc->sc_ev_txdrop);
				printf("%s: Tx packet consumes too many "
				    "DMA segments, dropping...\n",
				    sc->sc_dev.dv_xname);
				IFQ_DEQUEUE(&ifp->if_snd, m0);
				m_freem(m0);
				continue;
			}
			/*
			 * Short on resources, just stop for now.
			 */
			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: dmamap load failed: %d\n",
			    sc->sc_dev.dv_xname, error));
			break;
		}

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.  Note, we always reserve one descriptor
		 * at the end of the ring due to the semantics of the
		 * TDT register, plus one more in the event we need
		 * to re-load checksum offload context.
		 */
		if (dmamap->dm_nsegs > (sc->sc_txfree - 2)) {
			/*
			 * Not enough free descriptors to transmit this
			 * packet.  We haven't committed anything yet,
			 * so just unload the DMA map, put the packet
			 * pack on the queue, and punt.  Notify the upper
			 * layer that there are no more slots left.
			 */
			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: need %d descriptors, have %d\n",
			    sc->sc_dev.dv_xname, dmamap->dm_nsegs,
			    sc->sc_txfree - 1));
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			WM_EVCNT_INCR(&sc->sc_ev_txdstall);
			break;
		}

		/*
		 * Check for 82547 Tx FIFO bug.  We need to do this
		 * once we know we can transmit the packet, since we
		 * do some internal FIFO space accounting here.
		 */
		if (sc->sc_type == WM_T_82547 &&
		    wm_82547_txfifo_bugchk(sc, m0)) {
			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: 82547 Tx FIFO bug detected\n",
			    sc->sc_dev.dv_xname));
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			WM_EVCNT_INCR(&sc->sc_ev_txfifo_stall);
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: packet has %d DMA segments\n",
		    sc->sc_dev.dv_xname, dmamap->dm_nsegs));

		WM_EVCNT_INCR(&sc->sc_ev_txseg[dmamap->dm_nsegs - 1]);

		/*
		 * Store a pointer to the packet so that we can free it
		 * later.
		 *
		 * Initially, we consider the number of descriptors the
		 * packet uses the number of DMA segments.  This may be
		 * incremented by 1 if we do checksum offload (a descriptor
		 * is used to set the checksum context).
		 */
		txs->txs_mbuf = m0;
		txs->txs_firstdesc = sc->sc_txnext;
		txs->txs_ndesc = dmamap->dm_nsegs;

		/*
		 * Set up checksum offload parameters for
		 * this packet.
		 */
		if (m0->m_pkthdr.csum_flags &
		    (M_CSUM_IPv4|M_CSUM_TCPv4|M_CSUM_UDPv4)) {
			if (wm_tx_cksum(sc, txs, &cksumcmd,
					&cksumfields) != 0) {
				/* Error message already displayed. */
				bus_dmamap_unload(sc->sc_dmat, dmamap);
				continue;
			}
		} else {
			cksumcmd = 0;
			cksumfields = 0;
		}

		cksumcmd |= WTX_CMD_IDE;

		/*
		 * Initialize the transmit descriptor.
		 */
		for (nexttx = sc->sc_txnext, seg = 0;
		     seg < dmamap->dm_nsegs;
		     seg++, nexttx = WM_NEXTTX(nexttx)) {
			/*
			 * Note: we currently only use 32-bit DMA
			 * addresses.
			 */
			sc->sc_txdescs[nexttx].wtx_addr.wa_high = 0;
			sc->sc_txdescs[nexttx].wtx_addr.wa_low =
			    htole32(dmamap->dm_segs[seg].ds_addr);
			sc->sc_txdescs[nexttx].wtx_cmdlen =
			    htole32(cksumcmd | dmamap->dm_segs[seg].ds_len);
			sc->sc_txdescs[nexttx].wtx_fields.wtxu_status = 0;
			sc->sc_txdescs[nexttx].wtx_fields.wtxu_options =
			    cksumfields;
			sc->sc_txdescs[nexttx].wtx_fields.wtxu_vlan = 0;
			lasttx = nexttx;

			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: desc %d: low 0x%08x, len 0x%04x\n",
			    sc->sc_dev.dv_xname, nexttx,
			    (u_int)le32toh(dmamap->dm_segs[seg].ds_addr),
			    (u_int)le32toh(dmamap->dm_segs[seg].ds_len)));
		}

		KASSERT(lasttx != -1);

		/*
		 * Set up the command byte on the last descriptor of
		 * the packet.  If we're in the interrupt delay window,
		 * delay the interrupt.
		 */
		sc->sc_txdescs[lasttx].wtx_cmdlen |=
		    htole32(WTX_CMD_EOP | WTX_CMD_IFCS | WTX_CMD_RS);

#if 0 /* XXXJRT */
		/*
		 * If VLANs are enabled and the packet has a VLAN tag, set
		 * up the descriptor to encapsulate the packet for us.
		 *
		 * This is only valid on the last descriptor of the packet.
		 */
		if (sc->sc_ethercom.ec_nvlans != 0 &&
		    (mtag = m_tag_find(m0, PACKET_TAG_VLAN, NULL)) != NULL) {
			sc->sc_txdescs[lasttx].wtx_cmdlen |=
			    htole32(WTX_CMD_VLE);
			sc->sc_txdescs[lasttx].wtx_fields.wtxu_vlan
			    = htole16(*(u_int *)(mtag + 1) & 0xffff);
		}
#endif /* XXXJRT */

		txs->txs_lastdesc = lasttx;

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: desc %d: cmdlen 0x%08x\n", sc->sc_dev.dv_xname,
		    lasttx, le32toh(sc->sc_txdescs[lasttx].wtx_cmdlen)));

		/* Sync the descriptors we're using. */
		WM_CDTXSYNC(sc, sc->sc_txnext, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Give the packet to the chip. */
		CSR_WRITE(sc, sc->sc_tdt_reg, nexttx);

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: TDT -> %d\n", sc->sc_dev.dv_xname, nexttx));

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: finished transmitting packet, job %d\n",
		    sc->sc_dev.dv_xname, sc->sc_txsnext));

		/* Advance the tx pointer. */
		sc->sc_txfree -= txs->txs_ndesc;
		sc->sc_txnext = nexttx;

		sc->sc_txsfree--;
		sc->sc_txsnext = WM_NEXTTXS(sc, sc->sc_txsnext);

#if NBPFILTER > 0
		/* Pass the packet to any BPF listeners. */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0);
#endif /* NBPFILTER > 0 */
	}

	if (sc->sc_txsfree == 0 || sc->sc_txfree <= 2) {
		/* No more slots; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txfree != ofree) {
		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/*
 * wm_watchdog:		[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
static void
wm_watchdog(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;

	/*
	 * Since we're using delayed interrupts, sweep up
	 * before we report an error.
	 */
	wm_txintr(sc);

	if (sc->sc_txfree != WM_NTXDESC) {
		printf("%s: device timeout (txfree %d txsfree %d txnext %d)\n",
		    sc->sc_dev.dv_xname, sc->sc_txfree, sc->sc_txsfree,
		    sc->sc_txnext);
		ifp->if_oerrors++;

		/* Reset the interface. */
		(void) wm_init(ifp);
	}

	/* Try to get more packets going. */
	wm_start(ifp);
}

/*
 * wm_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
static int
wm_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct wm_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	int s, error;

	s = splnet();

	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				wm_set_filter(sc);
			error = 0;
		}
		break;
	}

	/* Try to get more packets going. */
	wm_start(ifp);

	splx(s);
	return (error);
}

/*
 * wm_intr:
 *
 *	Interrupt service routine.
 */
static int
wm_intr(void *arg)
{
	struct wm_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t icr;
	int wantinit, handled = 0;

	for (wantinit = 0; wantinit == 0;) {
		icr = CSR_READ(sc, WMREG_ICR);
		if ((icr & sc->sc_icr) == 0)
			break;

#if 0 /*NRND > 0*/
		if (RND_ENABLED(&sc->rnd_source))
			rnd_add_uint32(&sc->rnd_source, icr);
#endif

		handled = 1;

#if defined(WM_DEBUG) || defined(WM_EVENT_COUNTERS)
		if (icr & (ICR_RXDMT0|ICR_RXT0)) {
			DPRINTF(WM_DEBUG_RX,
			    ("%s: RX: got Rx intr 0x%08x\n",
			    sc->sc_dev.dv_xname,
			    icr & (ICR_RXDMT0|ICR_RXT0)));
			WM_EVCNT_INCR(&sc->sc_ev_rxintr);
		}
#endif
		wm_rxintr(sc);

#if defined(WM_DEBUG) || defined(WM_EVENT_COUNTERS)
		if (icr & ICR_TXDW) {
			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: got TXDW interrupt\n",
			    sc->sc_dev.dv_xname));
			WM_EVCNT_INCR(&sc->sc_ev_txdw);
		}
#endif
		wm_txintr(sc);

		if (icr & (ICR_LSC|ICR_RXSEQ|ICR_RXCFG)) {
			WM_EVCNT_INCR(&sc->sc_ev_linkintr);
			wm_linkintr(sc, icr);
		}

		if (icr & ICR_RXO) {
			printf("%s: Receive overrun\n", sc->sc_dev.dv_xname);
			wantinit = 1;
		}
	}

	if (handled) {
		if (wantinit)
			wm_init(ifp);

		/* Try to get more packets going. */
		wm_start(ifp);
	}

	return (handled);
}

/*
 * wm_txintr:
 *
 *	Helper; handle transmit interrupts.
 */
static void
wm_txintr(struct wm_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct wm_txsoft *txs;
	uint8_t status;
	int i;

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through the Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	for (i = sc->sc_txsdirty; sc->sc_txsfree != WM_TXQUEUELEN(sc);
	     i = WM_NEXTTXS(sc, i), sc->sc_txsfree++) {
		txs = &sc->sc_txsoft[i];

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: checking job %d\n", sc->sc_dev.dv_xname, i));

		WM_CDTXSYNC(sc, txs->txs_firstdesc, txs->txs_dmamap->dm_nsegs,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		status =
		    sc->sc_txdescs[txs->txs_lastdesc].wtx_fields.wtxu_status;
		if ((status & WTX_ST_DD) == 0) {
			WM_CDTXSYNC(sc, txs->txs_lastdesc, 1,
			    BUS_DMASYNC_PREREAD);
			break;
		}

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: job %d done: descs %d..%d\n",
		    sc->sc_dev.dv_xname, i, txs->txs_firstdesc,
		    txs->txs_lastdesc));

		/*
		 * XXX We should probably be using the statistics
		 * XXX registers, but I don't know if they exist
		 * XXX on chips before the i82544.
		 */

#ifdef WM_EVENT_COUNTERS
		if (status & WTX_ST_TU)
			WM_EVCNT_INCR(&sc->sc_ev_tu);
#endif /* WM_EVENT_COUNTERS */

		if (status & (WTX_ST_EC|WTX_ST_LC)) {
			ifp->if_oerrors++;
			if (status & WTX_ST_LC)
				printf("%s: late collision\n",
				    sc->sc_dev.dv_xname);
			else if (status & WTX_ST_EC) {
				ifp->if_collisions += 16;
				printf("%s: excessive collisions\n",
				    sc->sc_dev.dv_xname);
			}
		} else
			ifp->if_opackets++;

		sc->sc_txfree += txs->txs_ndesc;
		bus_dmamap_sync(sc->sc_dmat, txs->txs_dmamap,
		    0, txs->txs_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
		m_freem(txs->txs_mbuf);
		txs->txs_mbuf = NULL;
	}

	/* Update the dirty transmit buffer pointer. */
	sc->sc_txsdirty = i;
	DPRINTF(WM_DEBUG_TX,
	    ("%s: TX: txsdirty -> %d\n", sc->sc_dev.dv_xname, i));

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer.
	 */
	if (sc->sc_txsfree == WM_TXQUEUELEN(sc))
		ifp->if_timer = 0;
}

/*
 * wm_rxintr:
 *
 *	Helper; handle receive interrupts.
 */
static void
wm_rxintr(struct wm_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct wm_rxsoft *rxs;
	struct mbuf *m;
	int i, len;
	uint8_t status, errors;

	for (i = sc->sc_rxptr;; i = WM_NEXTRX(i)) {
		rxs = &sc->sc_rxsoft[i];

		DPRINTF(WM_DEBUG_RX,
		    ("%s: RX: checking descriptor %d\n",
		    sc->sc_dev.dv_xname, i));

		WM_CDRXSYNC(sc, i, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		status = sc->sc_rxdescs[i].wrx_status;
		errors = sc->sc_rxdescs[i].wrx_errors;
		len = le16toh(sc->sc_rxdescs[i].wrx_len);

		if ((status & WRX_ST_DD) == 0) {
			/*
			 * We have processed all of the receive descriptors.
			 */
			WM_CDRXSYNC(sc, i, BUS_DMASYNC_PREREAD);
			break;
		}

		if (__predict_false(sc->sc_rxdiscard)) {
			DPRINTF(WM_DEBUG_RX,
			    ("%s: RX: discarding contents of descriptor %d\n",
			    sc->sc_dev.dv_xname, i));
			WM_INIT_RXDESC(sc, i);
			if (status & WRX_ST_EOP) {
				/* Reset our state. */
				DPRINTF(WM_DEBUG_RX,
				    ("%s: RX: resetting rxdiscard -> 0\n",
				    sc->sc_dev.dv_xname));
				sc->sc_rxdiscard = 0;
			}
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		m = rxs->rxs_mbuf;

		/*
		 * Add a new receive buffer to the ring.
		 */
		if (wm_add_rxbuf(sc, i) != 0) {
			/*
			 * Failed, throw away what we've done so
			 * far, and discard the rest of the packet.
			 */
			ifp->if_ierrors++;
			bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
			WM_INIT_RXDESC(sc, i);
			if ((status & WRX_ST_EOP) == 0)
				sc->sc_rxdiscard = 1;
			if (sc->sc_rxhead != NULL)
				m_freem(sc->sc_rxhead);
			WM_RXCHAIN_RESET(sc);
			DPRINTF(WM_DEBUG_RX,
			    ("%s: RX: Rx buffer allocation failed, "
			    "dropping packet%s\n", sc->sc_dev.dv_xname,
			    sc->sc_rxdiscard ? " (discard)" : ""));
			continue;
		}

		WM_RXCHAIN_LINK(sc, m);

		m->m_len = len;

		DPRINTF(WM_DEBUG_RX,
		    ("%s: RX: buffer at %p len %d\n",
		    sc->sc_dev.dv_xname, m->m_data, len));

		/*
		 * If this is not the end of the packet, keep
		 * looking.
		 */
		if ((status & WRX_ST_EOP) == 0) {
			sc->sc_rxlen += len;
			DPRINTF(WM_DEBUG_RX,
			    ("%s: RX: not yet EOP, rxlen -> %d\n",
			    sc->sc_dev.dv_xname, sc->sc_rxlen));
			continue;
		}

		/*
		 * Okay, we have the entire packet now...
		 */
		*sc->sc_rxtailp = NULL;
		m = sc->sc_rxhead;
		len += sc->sc_rxlen;

		WM_RXCHAIN_RESET(sc);

		DPRINTF(WM_DEBUG_RX,
		    ("%s: RX: have entire packet, len -> %d\n",
		    sc->sc_dev.dv_xname, len));

		/*
		 * If an error occurred, update stats and drop the packet.
		 */
		if (errors &
		     (WRX_ER_CE|WRX_ER_SE|WRX_ER_SEQ|WRX_ER_CXE|WRX_ER_RXE)) {
			ifp->if_ierrors++;
			if (errors & WRX_ER_SE)
				printf("%s: symbol error\n",
				    sc->sc_dev.dv_xname);
			else if (errors & WRX_ER_SEQ)
				printf("%s: receive sequence error\n",
				    sc->sc_dev.dv_xname);
			else if (errors & WRX_ER_CE)
				printf("%s: CRC error\n",
				    sc->sc_dev.dv_xname);
			m_freem(m);
			continue;
		}

		/*
		 * No errors.  Receive the packet.
		 *
		 * Note, we have configured the chip to include the
		 * CRC with every packet.
		 */
		m->m_flags |= M_HASFCS;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = len;

#if 0 /* XXXJRT */
		/*
		 * If VLANs are enabled, VLAN packets have been unwrapped
		 * for us.  Associate the tag with the packet.
		 */
		if (sc->sc_ethercom.ec_nvlans != 0 &&
		    (status & WRX_ST_VP) != 0) {
			struct m_tag *vtag;

			vtag = m_tag_get(PACKET_TAG_VLAN, sizeof(u_int),
			    M_NOWAIT);
			if (vtag == NULL) {
				ifp->if_ierrors++;
				printf("%s: unable to allocate VLAN tag\n",
				    sc->sc_dev.dv_xname);
				m_freem(m);
				continue;
			}

			*(u_int *)(vtag + 1) =
			    le16toh(sc->sc_rxdescs[i].wrx_special);
		}
#endif /* XXXJRT */

		/*
		 * Set up checksum info for this packet.
		 */
		if (status & WRX_ST_IPCS) {
			WM_EVCNT_INCR(&sc->sc_ev_rxipsum);
			m->m_pkthdr.csum_flags |= M_CSUM_IPv4;
			if (errors & WRX_ER_IPE)
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4_BAD;
		}
		if (status & WRX_ST_TCPCS) {
			/*
			 * Note: we don't know if this was TCP or UDP,
			 * so we just set both bits, and expect the
			 * upper layers to deal.
			 */
			WM_EVCNT_INCR(&sc->sc_ev_rxtusum);
			m->m_pkthdr.csum_flags |= M_CSUM_TCPv4|M_CSUM_UDPv4;
			if (errors & WRX_ER_TCPE)
				m->m_pkthdr.csum_flags |= M_CSUM_TCP_UDP_BAD;
		}

		ifp->if_ipackets++;

#if NBPFILTER > 0
		/* Pass this up to any BPF listeners. */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif /* NBPFILTER > 0 */

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;

	DPRINTF(WM_DEBUG_RX,
	    ("%s: RX: rxptr -> %d\n", sc->sc_dev.dv_xname, i));
}

/*
 * wm_linkintr:
 *
 *	Helper; handle link interrupts.
 */
static void
wm_linkintr(struct wm_softc *sc, uint32_t icr)
{
	uint32_t status;

	/*
	 * If we get a link status interrupt on a 1000BASE-T
	 * device, just fall into the normal MII tick path.
	 */
	if (sc->sc_flags & WM_F_HAS_MII) {
		if (icr & ICR_LSC) {
			DPRINTF(WM_DEBUG_LINK,
			    ("%s: LINK: LSC -> mii_tick\n",
			    sc->sc_dev.dv_xname));
			mii_tick(&sc->sc_mii);
		} else if (icr & ICR_RXSEQ) {
			DPRINTF(WM_DEBUG_LINK,
			    ("%s: LINK Receive sequence error\n",
			    sc->sc_dev.dv_xname));
		}
		return;
	}

	/*
	 * If we are now receiving /C/, check for link again in
	 * a couple of link clock ticks.
	 */
	if (icr & ICR_RXCFG) {
		DPRINTF(WM_DEBUG_LINK, ("%s: LINK: receiving /C/\n",
		    sc->sc_dev.dv_xname));
		sc->sc_tbi_anstate = 2;
	}

	if (icr & ICR_LSC) {
		status = CSR_READ(sc, WMREG_STATUS);
		if (status & STATUS_LU) {
			DPRINTF(WM_DEBUG_LINK, ("%s: LINK: LSC -> up %s\n",
			    sc->sc_dev.dv_xname,
			    (status & STATUS_FD) ? "FDX" : "HDX"));
			sc->sc_tctl &= ~TCTL_COLD(0x3ff);
			if (status & STATUS_FD)
				sc->sc_tctl |=
				    TCTL_COLD(TX_COLLISION_DISTANCE_FDX);
			else
				sc->sc_tctl |=
				    TCTL_COLD(TX_COLLISION_DISTANCE_HDX);
			CSR_WRITE(sc, WMREG_TCTL, sc->sc_tctl);
			sc->sc_tbi_linkup = 1;
		} else {
			DPRINTF(WM_DEBUG_LINK, ("%s: LINK: LSC -> down\n",
			    sc->sc_dev.dv_xname));
			sc->sc_tbi_linkup = 0;
		}
		sc->sc_tbi_anstate = 2;
		wm_tbi_set_linkled(sc);
	} else if (icr & ICR_RXSEQ) {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: Receive sequence error\n",
		    sc->sc_dev.dv_xname));
	}
}

/*
 * wm_tick:
 *
 *	One second timer, used to check link status, sweep up
 *	completed transmit jobs, etc.
 */
static void
wm_tick(void *arg)
{
	struct wm_softc *sc = arg;
	int s;

	s = splnet();

	if (sc->sc_flags & WM_F_HAS_MII)
		mii_tick(&sc->sc_mii);
	else
		wm_tbi_check_link(sc);

	splx(s);

	callout_reset(&sc->sc_tick_ch, hz, wm_tick, sc);
}

/*
 * wm_reset:
 *
 *	Reset the i82542 chip.
 */
static void
wm_reset(struct wm_softc *sc)
{
	int i;

	/*
	 * Allocate on-chip memory according to the MTU size.
	 * The Packet Buffer Allocation register must be written
	 * before the chip is reset.
	 */
	if (sc->sc_type < WM_T_82547) {
		sc->sc_pba = sc->sc_ethercom.ec_if.if_mtu > 8192 ?
		    PBA_40K : PBA_48K;
	} else {
		sc->sc_pba = sc->sc_ethercom.ec_if.if_mtu > 8192 ?
		    PBA_22K : PBA_30K;
		sc->sc_txfifo_head = 0;
		sc->sc_txfifo_addr = sc->sc_pba << PBA_ADDR_SHIFT;
		sc->sc_txfifo_size =
		    (PBA_40K - sc->sc_pba) << PBA_BYTE_SHIFT;
		sc->sc_txfifo_stall = 0;
	}
	CSR_WRITE(sc, WMREG_PBA, sc->sc_pba);

	switch (sc->sc_type) {
	case WM_T_82544:
	case WM_T_82540:
	case WM_T_82545:
	case WM_T_82546:
	case WM_T_82541:
	case WM_T_82541_2:
		/*
		 * These chips have a problem with the memory-mapped
		 * write cycle when issuing the reset, so use I/O-mapped
		 * access, if possible.
		 */
		if (sc->sc_flags & WM_F_IOH_VALID)
			wm_io_write(sc, WMREG_CTRL, CTRL_RST);
		else
			CSR_WRITE(sc, WMREG_CTRL, CTRL_RST);
		break;

	case WM_T_82545_3:
	case WM_T_82546_3:
		/* Use the shadow control register on these chips. */
		CSR_WRITE(sc, WMREG_CTRL_SHADOW, CTRL_RST);
		break;

	default:
		/* Everything else can safely use the documented method. */
		CSR_WRITE(sc, WMREG_CTRL, CTRL_RST);
		break;
	}
	delay(10000);

	for (i = 0; i < 1000; i++) {
		if ((CSR_READ(sc, WMREG_CTRL) & CTRL_RST) == 0)
			return;
		delay(20);
	}

	if (CSR_READ(sc, WMREG_CTRL) & CTRL_RST)
		printf("%s: WARNING: reset failed to complete\n",
		    sc->sc_dev.dv_xname);
}

/*
 * wm_init:		[ifnet interface function]
 *
 *	Initialize the interface.  Must be called at splnet().
 */
static int
wm_init(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;
	struct wm_rxsoft *rxs;
	int i, error = 0;
	uint32_t reg;

	/*
	 * *_HDR_ALIGNED_P is constant 1 if __NO_STRICT_ALIGMENT is set.
	 * There is a small but measurable benefit to avoiding the adjusment
	 * of the descriptor so that the headers are aligned, for normal mtu,
	 * on such platforms.  One possibility is that the DMA itself is
	 * slightly more efficient if the front of the entire packet (instead
	 * of the front of the headers) is aligned.
	 *
	 * Note we must always set align_tweak to 0 if we are using
	 * jumbo frames.
	 */
#ifdef __NO_STRICT_ALIGNMENT
	sc->sc_align_tweak = 0;
#else
	if ((ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN) > (MCLBYTES - 2))
		sc->sc_align_tweak = 0;
	else
		sc->sc_align_tweak = 2;
#endif /* __NO_STRICT_ALIGNMENT */

	/* Cancel any pending I/O. */
	wm_stop(ifp, 0);

	/* Reset the chip to a known state. */
	wm_reset(sc);

	/* Initialize the transmit descriptor ring. */
	memset(sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	WM_CDTXSYNC(sc, 0, WM_NTXDESC,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->sc_txfree = WM_NTXDESC;
	sc->sc_txnext = 0;

	sc->sc_txctx_ipcs = 0xffffffff;
	sc->sc_txctx_tucs = 0xffffffff;

	if (sc->sc_type < WM_T_82543) {
		CSR_WRITE(sc, WMREG_OLD_TBDAH, 0);
		CSR_WRITE(sc, WMREG_OLD_TBDAL, WM_CDTXADDR(sc, 0));
		CSR_WRITE(sc, WMREG_OLD_TDLEN, sizeof(sc->sc_txdescs));
		CSR_WRITE(sc, WMREG_OLD_TDH, 0);
		CSR_WRITE(sc, WMREG_OLD_TDT, 0);
		CSR_WRITE(sc, WMREG_OLD_TIDV, 128);
	} else {
		CSR_WRITE(sc, WMREG_TBDAH, 0);
		CSR_WRITE(sc, WMREG_TBDAL, WM_CDTXADDR(sc, 0));
		CSR_WRITE(sc, WMREG_TDLEN, sizeof(sc->sc_txdescs));
		CSR_WRITE(sc, WMREG_TDH, 0);
		CSR_WRITE(sc, WMREG_TDT, 0);
		CSR_WRITE(sc, WMREG_TIDV, 128);

		CSR_WRITE(sc, WMREG_TXDCTL, TXDCTL_PTHRESH(0) |
		    TXDCTL_HTHRESH(0) | TXDCTL_WTHRESH(0));
		CSR_WRITE(sc, WMREG_RXDCTL, RXDCTL_PTHRESH(0) |
		    RXDCTL_HTHRESH(0) | RXDCTL_WTHRESH(1));
	}
	CSR_WRITE(sc, WMREG_TQSA_LO, 0);
	CSR_WRITE(sc, WMREG_TQSA_HI, 0);

	/* Initialize the transmit job descriptors. */
	for (i = 0; i < WM_TXQUEUELEN(sc); i++)
		sc->sc_txsoft[i].txs_mbuf = NULL;
	sc->sc_txsfree = WM_TXQUEUELEN(sc);
	sc->sc_txsnext = 0;
	sc->sc_txsdirty = 0;

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	if (sc->sc_type < WM_T_82543) {
		CSR_WRITE(sc, WMREG_OLD_RDBAH0, 0);
		CSR_WRITE(sc, WMREG_OLD_RDBAL0, WM_CDRXADDR(sc, 0));
		CSR_WRITE(sc, WMREG_OLD_RDLEN0, sizeof(sc->sc_rxdescs));
		CSR_WRITE(sc, WMREG_OLD_RDH0, 0);
		CSR_WRITE(sc, WMREG_OLD_RDT0, 0);
		CSR_WRITE(sc, WMREG_OLD_RDTR0, 28 | RDTR_FPD);

		CSR_WRITE(sc, WMREG_OLD_RDBA1_HI, 0);
		CSR_WRITE(sc, WMREG_OLD_RDBA1_LO, 0);
		CSR_WRITE(sc, WMREG_OLD_RDLEN1, 0);
		CSR_WRITE(sc, WMREG_OLD_RDH1, 0);
		CSR_WRITE(sc, WMREG_OLD_RDT1, 0);
		CSR_WRITE(sc, WMREG_OLD_RDTR1, 0);
	} else {
		CSR_WRITE(sc, WMREG_RDBAH, 0);
		CSR_WRITE(sc, WMREG_RDBAL, WM_CDRXADDR(sc, 0));
		CSR_WRITE(sc, WMREG_RDLEN, sizeof(sc->sc_rxdescs));
		CSR_WRITE(sc, WMREG_RDH, 0);
		CSR_WRITE(sc, WMREG_RDT, 0);
		CSR_WRITE(sc, WMREG_RDTR, 28 | RDTR_FPD);
	}
	for (i = 0; i < WM_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL) {
			if ((error = wm_add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				    sc->sc_dev.dv_xname, i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				wm_rxdrain(sc);
				goto out;
			}
		} else
			WM_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;
	sc->sc_rxdiscard = 0;
	WM_RXCHAIN_RESET(sc);

	/*
	 * Clear out the VLAN table -- we don't use it (yet).
	 */
	CSR_WRITE(sc, WMREG_VET, 0);
	for (i = 0; i < WM_VLAN_TABSIZE; i++)
		CSR_WRITE(sc, WMREG_VFTA + (i << 2), 0);

	/*
	 * Set up flow-control parameters.
	 *
	 * XXX Values could probably stand some tuning.
	 */
	if (sc->sc_ctrl & (CTRL_RFCE|CTRL_TFCE)) {
		CSR_WRITE(sc, WMREG_FCAL, FCAL_CONST);
		CSR_WRITE(sc, WMREG_FCAH, FCAH_CONST);
		CSR_WRITE(sc, WMREG_FCT, ETHERTYPE_FLOWCONTROL);

		if (sc->sc_type < WM_T_82543) {
			CSR_WRITE(sc, WMREG_OLD_FCRTH, FCRTH_DFLT);
			CSR_WRITE(sc, WMREG_OLD_FCRTL, FCRTL_DFLT);
		} else {
			CSR_WRITE(sc, WMREG_FCRTH, FCRTH_DFLT);
			CSR_WRITE(sc, WMREG_FCRTL, FCRTL_DFLT);
		}
		CSR_WRITE(sc, WMREG_FCTTV, FCTTV_DFLT);
	}

#if 0 /* XXXJRT */
	/* Deal with VLAN enables. */
	if (sc->sc_ethercom.ec_nvlans != 0)
		sc->sc_ctrl |= CTRL_VME;
	else
#endif /* XXXJRT */
		sc->sc_ctrl &= ~CTRL_VME;

	/* Write the control registers. */
	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
#if 0
	CSR_WRITE(sc, WMREG_CTRL_EXT, sc->sc_ctrl_ext);
#endif

	/*
	 * Set up checksum offload parameters.
	 */
	reg = CSR_READ(sc, WMREG_RXCSUM);
	if (ifp->if_capenable & IFCAP_CSUM_IPv4)
		reg |= RXCSUM_IPOFL;
	else
		reg &= ~RXCSUM_IPOFL;
	if (ifp->if_capenable & (IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4))
		reg |= RXCSUM_IPOFL | RXCSUM_TUOFL;
	else {
		reg &= ~RXCSUM_TUOFL;
		if ((ifp->if_capenable & IFCAP_CSUM_IPv4) == 0)
			reg &= ~RXCSUM_IPOFL;
	}
	CSR_WRITE(sc, WMREG_RXCSUM, reg);

	/*
	 * Set up the interrupt registers.
	 */
	CSR_WRITE(sc, WMREG_IMC, 0xffffffffU);
	sc->sc_icr = ICR_TXDW | ICR_LSC | ICR_RXSEQ | ICR_RXDMT0 |
	    ICR_RXO | ICR_RXT0;
	if ((sc->sc_flags & WM_F_HAS_MII) == 0)
		sc->sc_icr |= ICR_RXCFG;
	CSR_WRITE(sc, WMREG_IMS, sc->sc_icr);

	/* Set up the inter-packet gap. */
	CSR_WRITE(sc, WMREG_TIPG, sc->sc_tipg);

#if 0 /* XXXJRT */
	/* Set the VLAN ethernetype. */
	CSR_WRITE(sc, WMREG_VET, ETHERTYPE_VLAN);
#endif

	/*
	 * Set up the transmit control register; we start out with
	 * a collision distance suitable for FDX, but update it whe
	 * we resolve the media type.
	 */
	sc->sc_tctl = TCTL_EN | TCTL_PSP | TCTL_CT(TX_COLLISION_THRESHOLD) |
	    TCTL_COLD(TX_COLLISION_DISTANCE_FDX);
	CSR_WRITE(sc, WMREG_TCTL, sc->sc_tctl);

	/* Set the media. */
	(void) (*sc->sc_mii.mii_media.ifm_change)(ifp);

	/*
	 * Set up the receive control register; we actually program
	 * the register when we set the receive filter.  Use multicast
	 * address offset type 0.
	 *
	 * Only the i82544 has the ability to strip the incoming
	 * CRC, so we don't enable that feature.
	 */
	sc->sc_mchash_type = 0;
	sc->sc_rctl = RCTL_EN | RCTL_LBM_NONE | RCTL_RDMTS_1_2 | RCTL_LPE |
	    RCTL_DPF | RCTL_MO(sc->sc_mchash_type);

	if(MCLBYTES == 2048) {
		sc->sc_rctl |= RCTL_2k;
	} else {
	/*
	 * XXX MCLBYTES > 2048 causes "Tx packet consumes too many DMA"
	 * XXX segments, dropping" -- why?
	 */
#if 0
		if(sc->sc_type >= WM_T_82543) {
			switch(MCLBYTES) {
			case 4096:
				sc->sc_rctl |= RCTL_BSEX | RCTL_BSEX_4k;
				break;
			case 8192:
				sc->sc_rctl |= RCTL_BSEX | RCTL_BSEX_8k;
				break;
			case 16384:
				sc->sc_rctl |= RCTL_BSEX | RCTL_BSEX_16k;
				break;
			default:
				panic("wm_init: MCLBYTES %d unsupported",
				    MCLBYTES);
				break;
			}
		} else panic("wm_init: i82542 requires MCLBYTES = 2048");
#else
		panic("wm_init: MCLBYTES > 2048 not supported.");
#endif
	}

	/* Set the receive filter. */
	wm_set_filter(sc);

	/* Start the one second link check clock. */
	callout_reset(&sc->sc_tick_ch, hz, wm_tick, sc);

	/* ...all done! */
	ifp->if_flags |= IFF_RUNNING; 
	ifp->if_flags &= ~IFF_OACTIVE;

 out:
	if (error)
		printf("%s: interface not running\n", sc->sc_dev.dv_xname);
	return (error);
}

/*
 * wm_rxdrain:
 *
 *	Drain the receive queue.
 */
static void
wm_rxdrain(struct wm_softc *sc)
{
	struct wm_rxsoft *rxs;
	int i;

	for (i = 0; i < WM_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
	}
}

/*
 * wm_stop:		[ifnet interface function]
 *
 *	Stop transmission on the interface.
 */
static void
wm_stop(struct ifnet *ifp, int disable)
{
	struct wm_softc *sc = ifp->if_softc;
	struct wm_txsoft *txs;
	int i;

	/* Stop the one second clock. */
	callout_stop(&sc->sc_tick_ch);

	/* Stop the 82547 Tx FIFO stall check timer. */
	if (sc->sc_type == WM_T_82547)
		callout_stop(&sc->sc_txfifo_ch);

	if (sc->sc_flags & WM_F_HAS_MII) {
		/* Down the MII. */
		mii_down(&sc->sc_mii);
	}

	/* Stop the transmit and receive processes. */
	CSR_WRITE(sc, WMREG_TCTL, 0);
	CSR_WRITE(sc, WMREG_RCTL, 0);

	/* Release any queued transmit buffers. */
	for (i = 0; i < WM_TXQUEUELEN(sc); i++) {
		txs = &sc->sc_txsoft[i];
		if (txs->txs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
	}

	if (disable)
		wm_rxdrain(sc);

	/* Mark the interface as down and cancel the watchdog timer. */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}

/*
 * wm_acquire_eeprom:
 *
 *	Perform the EEPROM handshake required on some chips.
 */
static int
wm_acquire_eeprom(struct wm_softc *sc)
{
	uint32_t reg;
	int x;

	if (sc->sc_flags & WM_F_EEPROM_HANDSHAKE)  {
		reg = CSR_READ(sc, WMREG_EECD);

		/* Request EEPROM access. */
		reg |= EECD_EE_REQ;
		CSR_WRITE(sc, WMREG_EECD, reg);

		/* ..and wait for it to be granted. */
		for (x = 0; x < 100; x++) {
			reg = CSR_READ(sc, WMREG_EECD);
			if (reg & EECD_EE_GNT)
				break;
			delay(5);
		}
		if ((reg & EECD_EE_GNT) == 0) {
			aprint_error("%s: could not acquire EEPROM GNT\n",
			    sc->sc_dev.dv_xname);
			reg &= ~EECD_EE_REQ;
			CSR_WRITE(sc, WMREG_EECD, reg);
			return (1);
		}
	}

	return (0);
}

/*
 * wm_release_eeprom:
 *
 *	Release the EEPROM mutex.
 */
static void
wm_release_eeprom(struct wm_softc *sc)
{
	uint32_t reg;

	if (sc->sc_flags & WM_F_EEPROM_HANDSHAKE) {
		reg = CSR_READ(sc, WMREG_EECD);
		reg &= ~EECD_EE_REQ;
		CSR_WRITE(sc, WMREG_EECD, reg);
	}
}

/*
 * wm_eeprom_sendbits:
 *
 *	Send a series of bits to the EEPROM.
 */
static void
wm_eeprom_sendbits(struct wm_softc *sc, uint32_t bits, int nbits)
{
	uint32_t reg;
	int x;

	reg = CSR_READ(sc, WMREG_EECD);

	for (x = nbits; x > 0; x--) {
		if (bits & (1U << (x - 1)))
			reg |= EECD_DI;
		else
			reg &= ~EECD_DI;
		CSR_WRITE(sc, WMREG_EECD, reg);
		delay(2);
		CSR_WRITE(sc, WMREG_EECD, reg | EECD_SK);
		delay(2);
		CSR_WRITE(sc, WMREG_EECD, reg);
		delay(2);
	}
}

/*
 * wm_eeprom_recvbits:
 *
 *	Receive a series of bits from the EEPROM.
 */
static void
wm_eeprom_recvbits(struct wm_softc *sc, uint32_t *valp, int nbits)
{
	uint32_t reg, val;
	int x;

	reg = CSR_READ(sc, WMREG_EECD) & ~EECD_DI;

	val = 0;
	for (x = nbits; x > 0; x--) {
		CSR_WRITE(sc, WMREG_EECD, reg | EECD_SK);
		delay(2);
		if (CSR_READ(sc, WMREG_EECD) & EECD_DO)
			val |= (1U << (x - 1));
		CSR_WRITE(sc, WMREG_EECD, reg);
		delay(2);
	}
	*valp = val;
}

/*
 * wm_read_eeprom_uwire:
 *
 *	Read a word from the EEPROM using the MicroWire protocol.
 */
static int
wm_read_eeprom_uwire(struct wm_softc *sc, int word, int wordcnt, uint16_t *data)
{
	uint32_t reg, val;
	int i;

	for (i = 0; i < wordcnt; i++) {
		/* Clear SK and DI. */
		reg = CSR_READ(sc, WMREG_EECD) & ~(EECD_SK | EECD_DI);
		CSR_WRITE(sc, WMREG_EECD, reg);

		/* Set CHIP SELECT. */
		reg |= EECD_CS;
		CSR_WRITE(sc, WMREG_EECD, reg);
		delay(2);

		/* Shift in the READ command. */
		wm_eeprom_sendbits(sc, UWIRE_OPC_READ, 3);

		/* Shift in address. */
		wm_eeprom_sendbits(sc, word + i, sc->sc_ee_addrbits);

		/* Shift out the data. */
		wm_eeprom_recvbits(sc, &val, 16);
		data[i] = val & 0xffff;

		/* Clear CHIP SELECT. */
		reg = CSR_READ(sc, WMREG_EECD) & ~EECD_CS;
		CSR_WRITE(sc, WMREG_EECD, reg);
		delay(2);
	}

	return (0);
}

/*
 * wm_spi_eeprom_ready:
 *
 *	Wait for a SPI EEPROM to be ready for commands.
 */
static int
wm_spi_eeprom_ready(struct wm_softc *sc)
{
	uint32_t val;
	int usec;

	for (usec = 0; usec < SPI_MAX_RETRIES; delay(5), usec += 5) {
		wm_eeprom_sendbits(sc, SPI_OPC_RDSR, 8);
		wm_eeprom_recvbits(sc, &val, 8);
		if ((val & SPI_SR_RDY) == 0)
			break;
	}
	if (usec >= SPI_MAX_RETRIES) {
		aprint_error("%s: EEPROM failed to become ready\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}
	return (0);
}

/*
 * wm_read_eeprom_spi:
 *
 *	Read a work from the EEPROM using the SPI protocol.
 */
static int
wm_read_eeprom_spi(struct wm_softc *sc, int word, int wordcnt, uint16_t *data)
{
	uint32_t reg, val;
	int i;
	uint8_t opc;

	/* Clear SK and CS. */
	reg = CSR_READ(sc, WMREG_EECD) & ~(EECD_SK | EECD_CS);
	CSR_WRITE(sc, WMREG_EECD, reg);
	delay(2);

	if (wm_spi_eeprom_ready(sc))
		return (1);

	/* Toggle CS to flush commands. */
	CSR_WRITE(sc, WMREG_EECD, reg | EECD_CS);
	delay(2);
	CSR_WRITE(sc, WMREG_EECD, reg);
	delay(2);

	opc = SPI_OPC_READ;
	if (sc->sc_ee_addrbits == 8 && word >= 128)
		opc |= SPI_OPC_A8;

	wm_eeprom_sendbits(sc, opc, 8);
	wm_eeprom_sendbits(sc, word << 1, sc->sc_ee_addrbits);

	for (i = 0; i < wordcnt; i++) {
		wm_eeprom_recvbits(sc, &val, 16);
		data[i] = ((val >> 8) & 0xff) | ((val & 0xff) << 8);
	}

	/* Raise CS and clear SK. */
	reg = (CSR_READ(sc, WMREG_EECD) & ~EECD_SK) | EECD_CS;
	CSR_WRITE(sc, WMREG_EECD, reg);
	delay(2);

	return (0);
}

/*
 * wm_read_eeprom:
 *
 *	Read data from the serial EEPROM.
 */
static int
wm_read_eeprom(struct wm_softc *sc, int word, int wordcnt, uint16_t *data)
{
	int rv;

	if (wm_acquire_eeprom(sc))
		return (1);

	if (sc->sc_flags & WM_F_EEPROM_SPI)
		rv = wm_read_eeprom_spi(sc, word, wordcnt, data);
	else
		rv = wm_read_eeprom_uwire(sc, word, wordcnt, data);

	wm_release_eeprom(sc);
	return (rv);
}

/*
 * wm_add_rxbuf:
 *
 *	Add a receive buffer to the indiciated descriptor.
 */
static int
wm_add_rxbuf(struct wm_softc *sc, int idx)
{
	struct wm_rxsoft *rxs = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	if (rxs->rxs_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);

	rxs->rxs_mbuf = m;

	m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;
	error = bus_dmamap_load_mbuf(sc->sc_dmat, rxs->rxs_dmamap, m,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: unable to load rx DMA map %d, error = %d\n",
		    sc->sc_dev.dv_xname, idx, error);
		panic("wm_add_rxbuf");	/* XXX XXX XXX */
	}

	bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
	    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	WM_INIT_RXDESC(sc, idx);

	return (0);
}

/*
 * wm_set_ral:
 *
 *	Set an entery in the receive address list.
 */
static void
wm_set_ral(struct wm_softc *sc, const uint8_t *enaddr, int idx)
{
	uint32_t ral_lo, ral_hi;

	if (enaddr != NULL) {
		ral_lo = enaddr[0] | (enaddr[1] << 8) | (enaddr[2] << 16) |
		    (enaddr[3] << 24);
		ral_hi = enaddr[4] | (enaddr[5] << 8);
		ral_hi |= RAL_AV;
	} else {
		ral_lo = 0;
		ral_hi = 0;
	}

	if (sc->sc_type >= WM_T_82544) {
		CSR_WRITE(sc, WMREG_RAL_LO(WMREG_CORDOVA_RAL_BASE, idx),
		    ral_lo);
		CSR_WRITE(sc, WMREG_RAL_HI(WMREG_CORDOVA_RAL_BASE, idx),
		    ral_hi);
	} else {
		CSR_WRITE(sc, WMREG_RAL_LO(WMREG_RAL_BASE, idx), ral_lo);
		CSR_WRITE(sc, WMREG_RAL_HI(WMREG_RAL_BASE, idx), ral_hi);
	}
}

/*
 * wm_mchash:
 *
 *	Compute the hash of the multicast address for the 4096-bit
 *	multicast filter.
 */
static uint32_t
wm_mchash(struct wm_softc *sc, const uint8_t *enaddr)
{
	static const int lo_shift[4] = { 4, 3, 2, 0 };
	static const int hi_shift[4] = { 4, 5, 6, 8 };
	uint32_t hash;

	hash = (enaddr[4] >> lo_shift[sc->sc_mchash_type]) |
	    (((uint16_t) enaddr[5]) << hi_shift[sc->sc_mchash_type]);

	return (hash & 0xfff);
}

/*
 * wm_set_filter:
 *
 *	Set up the receive filter.
 */
static void
wm_set_filter(struct wm_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	bus_addr_t mta_reg;
	uint32_t hash, reg, bit;
	int i;

	if (sc->sc_type >= WM_T_82544)
		mta_reg = WMREG_CORDOVA_MTA;
	else
		mta_reg = WMREG_MTA;

	sc->sc_rctl &= ~(RCTL_BAM | RCTL_UPE | RCTL_MPE);

	if (ifp->if_flags & IFF_BROADCAST)
		sc->sc_rctl |= RCTL_BAM;
	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_rctl |= RCTL_UPE;
		goto allmulti;
	}

	/*
	 * Set the station address in the first RAL slot, and
	 * clear the remaining slots.
	 */
	wm_set_ral(sc, LLADDR(ifp->if_sadl), 0);
	for (i = 1; i < WM_RAL_TABSIZE; i++)
		wm_set_ral(sc, NULL, i);

	/* Clear out the multicast table. */
	for (i = 0; i < WM_MC_TABSIZE; i++)
		CSR_WRITE(sc, mta_reg + (i << 2), 0);

	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		hash = wm_mchash(sc, enm->enm_addrlo);

		reg = (hash >> 5) & 0x7f;
		bit = hash & 0x1f;

		hash = CSR_READ(sc, mta_reg + (reg << 2));
		hash |= 1U << bit;

		/* XXX Hardware bug?? */
		if (sc->sc_type == WM_T_82544 && (reg & 0xe) == 1) {
			bit = CSR_READ(sc, mta_reg + ((reg - 1) << 2));
			CSR_WRITE(sc, mta_reg + (reg << 2), hash);
			CSR_WRITE(sc, mta_reg + ((reg - 1) << 2), bit);
		} else
			CSR_WRITE(sc, mta_reg + (reg << 2), hash);

		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	sc->sc_rctl |= RCTL_MPE;

 setit:
	CSR_WRITE(sc, WMREG_RCTL, sc->sc_rctl);
}

/*
 * wm_tbi_mediainit:
 *
 *	Initialize media for use on 1000BASE-X devices.
 */
static void
wm_tbi_mediainit(struct wm_softc *sc)
{
	const char *sep = "";

	if (sc->sc_type < WM_T_82543)
		sc->sc_tipg = TIPG_WM_DFLT;
	else
		sc->sc_tipg = TIPG_LG_DFLT;

	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, wm_tbi_mediachange,
	    wm_tbi_mediastatus);

	/*
	 * SWD Pins:
	 *
	 *	0 = Link LED (output)
	 *	1 = Loss Of Signal (input)
	 */
	sc->sc_ctrl |= CTRL_SWDPIO(0);
	sc->sc_ctrl &= ~CTRL_SWDPIO(1);

	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);

#define	ADD(ss, mm, dd)							\
do {									\
	printf("%s%s", sep, ss);					\
	ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|(mm), (dd), NULL);	\
	sep = ", ";							\
} while (/*CONSTCOND*/0)

	printf("%s: ", sc->sc_dev.dv_xname);
	ADD("1000baseSX", IFM_1000_SX, ANAR_X_HD);
	ADD("1000baseSX-FDX", IFM_1000_SX|IFM_FDX, ANAR_X_FD);
	ADD("auto", IFM_AUTO, ANAR_X_FD|ANAR_X_HD);
	printf("\n");

#undef ADD

	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
}

/*
 * wm_tbi_mediastatus:	[ifmedia interface function]
 *
 *	Get the current interface media status on a 1000BASE-X device.
 */
static void
wm_tbi_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct wm_softc *sc = ifp->if_softc;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (sc->sc_tbi_linkup == 0) {
		ifmr->ifm_active |= IFM_NONE;
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= IFM_1000_SX;
	if (CSR_READ(sc, WMREG_STATUS) & STATUS_FD)
		ifmr->ifm_active |= IFM_FDX;
}

/*
 * wm_tbi_mediachange:	[ifmedia interface function]
 *
 *	Set hardware to newly-selected media on a 1000BASE-X device.
 */
static int
wm_tbi_mediachange(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;
	struct ifmedia_entry *ife = sc->sc_mii.mii_media.ifm_cur;
	uint32_t status;
	int i;

	sc->sc_txcw = ife->ifm_data;
	if (sc->sc_ctrl & CTRL_RFCE)
		sc->sc_txcw |= ANAR_X_PAUSE_TOWARDS;
	if (sc->sc_ctrl & CTRL_TFCE)
		sc->sc_txcw |= ANAR_X_PAUSE_ASYM;
	sc->sc_txcw |= TXCW_ANE;

	CSR_WRITE(sc, WMREG_TXCW, sc->sc_txcw);
	delay(10000);

	sc->sc_tbi_anstate = 0;

	if ((CSR_READ(sc, WMREG_CTRL) & CTRL_SWDPIN(1)) == 0) {
		/* Have signal; wait for the link to come up. */
		for (i = 0; i < 50; i++) {
			delay(10000);
			if (CSR_READ(sc, WMREG_STATUS) & STATUS_LU)
				break;
		}

		status = CSR_READ(sc, WMREG_STATUS);
		if (status & STATUS_LU) {
			/* Link is up. */
			DPRINTF(WM_DEBUG_LINK,
			    ("%s: LINK: set media -> link up %s\n",
			    sc->sc_dev.dv_xname,
			    (status & STATUS_FD) ? "FDX" : "HDX"));
			sc->sc_tctl &= ~TCTL_COLD(0x3ff);
			if (status & STATUS_FD)
				sc->sc_tctl |=
				    TCTL_COLD(TX_COLLISION_DISTANCE_FDX);
			else
				sc->sc_tctl |=
				    TCTL_COLD(TX_COLLISION_DISTANCE_HDX);
			CSR_WRITE(sc, WMREG_TCTL, sc->sc_tctl);
			sc->sc_tbi_linkup = 1;
		} else {
			/* Link is down. */
			DPRINTF(WM_DEBUG_LINK,
			    ("%s: LINK: set media -> link down\n",
			    sc->sc_dev.dv_xname));
			sc->sc_tbi_linkup = 0;
		}
	} else {
		DPRINTF(WM_DEBUG_LINK, ("%s: LINK: set media -> no signal\n",
		    sc->sc_dev.dv_xname));
		sc->sc_tbi_linkup = 0;
	}

	wm_tbi_set_linkled(sc);

	return (0);
}

/*
 * wm_tbi_set_linkled:
 *
 *	Update the link LED on 1000BASE-X devices.
 */
static void
wm_tbi_set_linkled(struct wm_softc *sc)
{

	if (sc->sc_tbi_linkup)
		sc->sc_ctrl |= CTRL_SWDPIN(0);
	else
		sc->sc_ctrl &= ~CTRL_SWDPIN(0);

	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
}

/*
 * wm_tbi_check_link:
 *
 *	Check the link on 1000BASE-X devices.
 */
static void
wm_tbi_check_link(struct wm_softc *sc)
{
	uint32_t rxcw, ctrl, status;

	if (sc->sc_tbi_anstate == 0)
		return;
	else if (sc->sc_tbi_anstate > 1) {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: anstate %d\n", sc->sc_dev.dv_xname,
		    sc->sc_tbi_anstate));
		sc->sc_tbi_anstate--;
		return;
	}

	sc->sc_tbi_anstate = 0;

	rxcw = CSR_READ(sc, WMREG_RXCW);
	ctrl = CSR_READ(sc, WMREG_CTRL);
	status = CSR_READ(sc, WMREG_STATUS);

	if ((status & STATUS_LU) == 0) {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: checklink -> down\n", sc->sc_dev.dv_xname));
		sc->sc_tbi_linkup = 0;
	} else {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: checklink -> up %s\n", sc->sc_dev.dv_xname,
		    (status & STATUS_FD) ? "FDX" : "HDX"));
		sc->sc_tctl &= ~TCTL_COLD(0x3ff);
		if (status & STATUS_FD)
			sc->sc_tctl |=
			    TCTL_COLD(TX_COLLISION_DISTANCE_FDX);
		else
			sc->sc_tctl |=
			    TCTL_COLD(TX_COLLISION_DISTANCE_HDX);
		CSR_WRITE(sc, WMREG_TCTL, sc->sc_tctl);
		sc->sc_tbi_linkup = 1;
	}

	wm_tbi_set_linkled(sc);
}

/*
 * wm_gmii_reset:
 *
 *	Reset the PHY.
 */
static void
wm_gmii_reset(struct wm_softc *sc)
{
	uint32_t reg;

	if (sc->sc_type >= WM_T_82544) {
		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl | CTRL_PHY_RESET);
		delay(20000);

		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
		delay(20000);
	} else {
		/* The PHY reset pin is active-low. */
		reg = CSR_READ(sc, WMREG_CTRL_EXT);
		reg &= ~((CTRL_EXT_SWDPIO_MASK << CTRL_EXT_SWDPIO_SHIFT) |
		    CTRL_EXT_SWDPIN(4));
		reg |= CTRL_EXT_SWDPIO(4);

		CSR_WRITE(sc, WMREG_CTRL_EXT, reg | CTRL_EXT_SWDPIN(4));
		delay(10);

		CSR_WRITE(sc, WMREG_CTRL_EXT, reg);
		delay(10);

		CSR_WRITE(sc, WMREG_CTRL_EXT, reg | CTRL_EXT_SWDPIN(4));
		delay(10);
#if 0
		sc->sc_ctrl_ext = reg | CTRL_EXT_SWDPIN(4);
#endif
	}
}

/*
 * wm_gmii_mediainit:
 *
 *	Initialize media for use on 1000BASE-T devices.
 */
static void
wm_gmii_mediainit(struct wm_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	/* We have MII. */
	sc->sc_flags |= WM_F_HAS_MII;

	sc->sc_tipg = TIPG_1000T_DFLT;

	/*
	 * Let the chip set speed/duplex on its own based on
	 * signals from the PHY.
	 */
	sc->sc_ctrl |= CTRL_SLU | CTRL_ASDE;
	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);

	/* Initialize our media structures and probe the GMII. */
	sc->sc_mii.mii_ifp = ifp;

	if (sc->sc_type >= WM_T_82544) {
		sc->sc_mii.mii_readreg = wm_gmii_i82544_readreg;
		sc->sc_mii.mii_writereg = wm_gmii_i82544_writereg;
	} else {
		sc->sc_mii.mii_readreg = wm_gmii_i82543_readreg;
		sc->sc_mii.mii_writereg = wm_gmii_i82543_writereg;
	}
	sc->sc_mii.mii_statchg = wm_gmii_statchg;

	wm_gmii_reset(sc);

	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, wm_gmii_mediachange,
	    wm_gmii_mediastatus);

	mii_attach(&sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
}

/*
 * wm_gmii_mediastatus:	[ifmedia interface function]
 *
 *	Get the current interface media status on a 1000BASE-T device.
 */
static void
wm_gmii_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct wm_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
}

/*
 * wm_gmii_mediachange:	[ifmedia interface function]
 *
 *	Set hardware to newly-selected media on a 1000BASE-T device.
 */
static int
wm_gmii_mediachange(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;

	if (ifp->if_flags & IFF_UP)
		mii_mediachg(&sc->sc_mii);
	return (0);
}

#define	MDI_IO		CTRL_SWDPIN(2)
#define	MDI_DIR		CTRL_SWDPIO(2)	/* host -> PHY */
#define	MDI_CLK		CTRL_SWDPIN(3)

static void
i82543_mii_sendbits(struct wm_softc *sc, uint32_t data, int nbits)
{
	uint32_t i, v;

	v = CSR_READ(sc, WMREG_CTRL);
	v &= ~(MDI_IO|MDI_CLK|(CTRL_SWDPIO_MASK << CTRL_SWDPIO_SHIFT));
	v |= MDI_DIR | CTRL_SWDPIO(3);

	for (i = 1 << (nbits - 1); i != 0; i >>= 1) {
		if (data & i)
			v |= MDI_IO;
		else
			v &= ~MDI_IO;
		CSR_WRITE(sc, WMREG_CTRL, v);
		delay(10);
		CSR_WRITE(sc, WMREG_CTRL, v | MDI_CLK);
		delay(10);
		CSR_WRITE(sc, WMREG_CTRL, v);
		delay(10);
	}
}

static uint32_t
i82543_mii_recvbits(struct wm_softc *sc)
{
	uint32_t v, i, data = 0;

	v = CSR_READ(sc, WMREG_CTRL);
	v &= ~(MDI_IO|MDI_CLK|(CTRL_SWDPIO_MASK << CTRL_SWDPIO_SHIFT));
	v |= CTRL_SWDPIO(3);

	CSR_WRITE(sc, WMREG_CTRL, v);
	delay(10);
	CSR_WRITE(sc, WMREG_CTRL, v | MDI_CLK);
	delay(10);
	CSR_WRITE(sc, WMREG_CTRL, v);
	delay(10);

	for (i = 0; i < 16; i++) {
		data <<= 1;
		CSR_WRITE(sc, WMREG_CTRL, v | MDI_CLK);
		delay(10);
		if (CSR_READ(sc, WMREG_CTRL) & MDI_IO)
			data |= 1;
		CSR_WRITE(sc, WMREG_CTRL, v);
		delay(10);
	}

	CSR_WRITE(sc, WMREG_CTRL, v | MDI_CLK);
	delay(10);
	CSR_WRITE(sc, WMREG_CTRL, v);
	delay(10);

	return (data);
}

#undef MDI_IO
#undef MDI_DIR
#undef MDI_CLK

/*
 * wm_gmii_i82543_readreg:	[mii interface function]
 *
 *	Read a PHY register on the GMII (i82543 version).
 */
static int
wm_gmii_i82543_readreg(struct device *self, int phy, int reg)
{
	struct wm_softc *sc = (void *) self;
	int rv;

	i82543_mii_sendbits(sc, 0xffffffffU, 32);
	i82543_mii_sendbits(sc, reg | (phy << 5) |
	    (MII_COMMAND_READ << 10) | (MII_COMMAND_START << 12), 14);
	rv = i82543_mii_recvbits(sc) & 0xffff;

	DPRINTF(WM_DEBUG_GMII,
	    ("%s: GMII: read phy %d reg %d -> 0x%04x\n",
	    sc->sc_dev.dv_xname, phy, reg, rv));

	return (rv);
}

/*
 * wm_gmii_i82543_writereg:	[mii interface function]
 *
 *	Write a PHY register on the GMII (i82543 version).
 */
static void
wm_gmii_i82543_writereg(struct device *self, int phy, int reg, int val)
{
	struct wm_softc *sc = (void *) self;

	i82543_mii_sendbits(sc, 0xffffffffU, 32);
	i82543_mii_sendbits(sc, val | (MII_COMMAND_ACK << 16) |
	    (reg << 18) | (phy << 23) | (MII_COMMAND_WRITE << 28) |
	    (MII_COMMAND_START << 30), 32);
}

/*
 * wm_gmii_i82544_readreg:	[mii interface function]
 *
 *	Read a PHY register on the GMII.
 */
static int
wm_gmii_i82544_readreg(struct device *self, int phy, int reg)
{
	struct wm_softc *sc = (void *) self;
	uint32_t mdic = 0;
	int i, rv;

	CSR_WRITE(sc, WMREG_MDIC, MDIC_OP_READ | MDIC_PHYADD(phy) |
	    MDIC_REGADD(reg));

	for (i = 0; i < 100; i++) {
		mdic = CSR_READ(sc, WMREG_MDIC);
		if (mdic & MDIC_READY)
			break;
		delay(10);
	}

	if ((mdic & MDIC_READY) == 0) {
		printf("%s: MDIC read timed out: phy %d reg %d\n",
		    sc->sc_dev.dv_xname, phy, reg);
		rv = 0;
	} else if (mdic & MDIC_E) {
#if 0 /* This is normal if no PHY is present. */
		printf("%s: MDIC read error: phy %d reg %d\n",
		    sc->sc_dev.dv_xname, phy, reg);
#endif
		rv = 0;
	} else {
		rv = MDIC_DATA(mdic);
		if (rv == 0xffff)
			rv = 0;
	}

	return (rv);
}

/*
 * wm_gmii_i82544_writereg:	[mii interface function]
 *
 *	Write a PHY register on the GMII.
 */
static void
wm_gmii_i82544_writereg(struct device *self, int phy, int reg, int val)
{
	struct wm_softc *sc = (void *) self;
	uint32_t mdic = 0;
	int i;

	CSR_WRITE(sc, WMREG_MDIC, MDIC_OP_WRITE | MDIC_PHYADD(phy) |
	    MDIC_REGADD(reg) | MDIC_DATA(val));

	for (i = 0; i < 100; i++) {
		mdic = CSR_READ(sc, WMREG_MDIC);
		if (mdic & MDIC_READY)
			break;
		delay(10);
	}

	if ((mdic & MDIC_READY) == 0)
		printf("%s: MDIC write timed out: phy %d reg %d\n",
		    sc->sc_dev.dv_xname, phy, reg);
	else if (mdic & MDIC_E)
		printf("%s: MDIC write error: phy %d reg %d\n",
		    sc->sc_dev.dv_xname, phy, reg);
}

/*
 * wm_gmii_statchg:	[mii interface function]
 *
 *	Callback from MII layer when media changes.
 */
static void
wm_gmii_statchg(struct device *self)
{
	struct wm_softc *sc = (void *) self;

	sc->sc_tctl &= ~TCTL_COLD(0x3ff);

	if (sc->sc_mii.mii_media_active & IFM_FDX) {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: statchg: FDX\n", sc->sc_dev.dv_xname));
		sc->sc_tctl |= TCTL_COLD(TX_COLLISION_DISTANCE_FDX);
	} else  {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: statchg: HDX\n", sc->sc_dev.dv_xname));
		sc->sc_tctl |= TCTL_COLD(TX_COLLISION_DISTANCE_HDX);
	}

	CSR_WRITE(sc, WMREG_TCTL, sc->sc_tctl);
}
