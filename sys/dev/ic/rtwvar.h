/* $NetBSD: rtwvar.h,v 1.1.2.2 2004/10/19 15:56:56 skrll Exp $ */
/*-
 * Copyright (c) 2004, 2005 David Young.  All rights reserved.
 *
 * Driver for the Realtek RTL8180 802.11 MAC/BBP by David Young.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY David Young ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL David
 * Young BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifndef _DEV_IC_RTWVAR_H_
#define	_DEV_IC_RTWVAR_H_

#include <sys/queue.h>
#include <sys/callout.h>

#ifdef RTW_DEBUG
extern int rtw_debug;
#define RTW_DPRINTF(x)	if (rtw_debug > 0) printf x
#define RTW_DPRINTF2(x)	if (rtw_debug > 1) printf x
#define RTW_DPRINTF3(x)	if (rtw_debug > 2) printf x
#define	DPRINTF(sc, x)	if ((sc)->sc_ic.ic_if.if_flags & IFF_DEBUG) printf x
#define	DPRINTF2(sc, x)	if ((sc)->sc_ic.ic_if.if_flags & IFF_DEBUG) RTW_DPRINTF2(x)
#define	DPRINTF3(sc, x)	if ((sc)->sc_ic.ic_if.if_flags & IFF_DEBUG) RTW_DPRINTF3(x)
#else /* RTW_DEBUG */
#define RTW_DPRINTF(x)
#define RTW_DPRINTF2(x)
#define RTW_DPRINTF3(x)
#define	DPRINTF(sc, x)
#define	DPRINTF2(sc, x)
#define	DPRINTF3(sc, x)
#endif /* RTW_DEBUG */

#if 0
enum rtw_rftype {
	RTW_RFTYPE_INTERSIL = 0,
	RTW_RFTYPE_RFMD,
	RTW_RFTYPE_PHILIPS,
	RTW_RFTYPE_MAXIM
};
#endif

enum rtw_locale {
	RTW_LOCALE_USA = 0,
	RTW_LOCALE_EUROPE,
	RTW_LOCALE_JAPAN,
	RTW_LOCALE_UNKNOWN
};

enum rtw_rfchipid {
	RTW_RFCHIPID_RESERVED = 0,
	RTW_RFCHIPID_INTERSIL = 1,
	RTW_RFCHIPID_RFMD = 2,
	RTW_RFCHIPID_PHILIPS = 3,
	RTW_RFCHIPID_MAXIM = 4,
	RTW_RFCHIPID_GCT = 5
}; 

/* sc_flags */
#define RTW_F_ENABLED		0x00000001	/* chip is enabled */
#define RTW_F_DIGPHY		0x00000002	/* digital PHY */
#define RTW_F_DFLANTB		0x00000004	/* B antenna is default */
#define RTW_F_ANTDIV		0x00000010	/* h/w antenna diversity */
#define RTW_F_9356SROM		0x00000020	/* 93c56 SROM */
#define RTW_F_SLEEP		0x00000040	/* chip is enabled */
	/* all PHY flags */
#define RTW_F_ALLPHY		(RTW_F_DIGPHY|RTW_F_DFLANTB|RTW_F_ANTDIV)

struct rtw_regs {
	bus_space_tag_t		r_bt;
	bus_space_handle_t	r_bh;
};

#define RTW_SR_GET(sr, ofs) \
    (((sr)->sr_content[(ofs)/2] >> (((ofs) % 2 == 0) ? 0 : 8)) & 0xff)

#define RTW_SR_GET16(sr, ofs) \
    (RTW_SR_GET((sr), (ofs)) | (RTW_SR_GET((sr), (ofs) + 1) << 8))

struct rtw_srom {
	u_int16_t		*sr_content;
	u_int16_t		sr_size;
};

struct rtw_rxctl {
	struct mbuf			*srx_mbuf;
	bus_dmamap_t			srx_dmamap;
};

struct rtw_txctl {
	SIMPLEQ_ENTRY(rtw_txctl)	stx_q;
	struct mbuf			*stx_mbuf;
	bus_dmamap_t			stx_dmamap;
	struct ieee80211_node		*stx_ni;	/* destination node */
	u_int				stx_first;	/* 1st hw descriptor */
	u_int				stx_last;	/* last hw descriptor */
};

#define RTW_NTXPRI	4	/* number of Tx priorities */
#define RTW_TXPRILO	0
#define RTW_TXPRIMD	1
#define RTW_TXPRIHI	2
#define RTW_TXPRIBCN	3	/* beacon priority */

#define RTW_MAXPKTSEGS		32	/* max 32 segments per Tx packet */

#define CASSERT(cond, complaint) complaint[(cond) ? 0 : -1] = complaint[(cond) ? 0 : -1]

#define RTW_NTXDESC_ROUNDUP(n) \
    roundup(n, RTW_DESC_ALIGNMENT / sizeof(struct rtw_txdesc))

#define RTW_TXQLEN_ROUNDUP(n) \
    (RTW_NTXDESC_ROUNDUP(n * RTW_MAXPKTSEGS) / RTW_MAXPKTSEGS)

#define RTW_RXQLEN_ROUNDUP(n) \
    roundup(n, RTW_DESC_ALIGNMENT / sizeof(struct rtw_rxctl))

/* The descriptor rings must begin on RTW_DESC_ALIGNMENT boundaries.
 * I allocate them consecutively from one buffer, so just round up.
 */
#define RTW_TXQLENLO	RTW_TXQLEN_ROUNDUP(64)	/* high-priority queue length */
#define RTW_TXQLENMD	RTW_TXQLEN_ROUNDUP(32)	/* medium-priority */
#define RTW_TXQLENHI	RTW_TXQLEN_ROUNDUP(16)	/* high-priority */
#define RTW_TXQLENBCN	1			/* beacon */

#define RTW_NTXDESCLO	(RTW_TXQLENLO * RTW_MAXPKTSEGS)
#define RTW_NTXDESCMD	(RTW_TXQLENMD * RTW_MAXPKTSEGS)
#define RTW_NTXDESCHI	(RTW_TXQLENHI * RTW_MAXPKTSEGS)
#define RTW_NTXDESCBCN	(RTW_TXQLENBCN * RTW_MAXPKTSEGS)

#define RTW_NTXDESCTOTAL	(RTW_NTXDESCLO + RTW_NTXDESCMD + \
				 RTW_NTXDESCHI + RTW_NTXDESCBCN)

#define RTW_RXQLEN	RTW_RXQLEN_ROUNDUP(32)
#define RTW_NRXDESC	RTW_RXQLEN

struct rtw_txdesc_blk {
	u_int			htc_ndesc;
	u_int			htc_next;
	u_int			htc_nfree;
	bus_addr_t		htc_physbase;
	bus_addr_t		htc_ofs;
	struct rtw_txdesc	*htc_desc;
};

#define RTW_NEXT_DESC(htc, idx) \
    (htc->htc_physbase + \
     sizeof(struct rtw_txdesc) * ((idx + 1) % htc->htc_ndesc))

SIMPLEQ_HEAD(rtw_txq, rtw_txctl);

struct rtw_txctl_blk {
	/* dirty/free s/w descriptors */
	struct rtw_txq		stc_dirtyq;
	struct rtw_txq		stc_freeq;
	u_int			stc_ndesc;
	struct rtw_txctl	*stc_desc;
};

struct rtw_descs {
	struct rtw_txdesc	hd_txlo[RTW_NTXDESCLO];
	struct rtw_txdesc	hd_txmd[RTW_NTXDESCMD];
	struct rtw_txdesc	hd_txhi[RTW_NTXDESCMD];
	struct rtw_rxdesc	hd_rx[RTW_NRXDESC];
	struct rtw_txdesc	hd_bcn[RTW_NTXDESCBCN];
};
#define RTW_DESC_OFFSET(ring, i)	offsetof(struct rtw_descs, ring[i])
#define RTW_RING_OFFSET(ring)		RTW_DESC_OFFSET(ring, 0)
#define RTW_RING_BASE(sc, ring)		((sc)->sc_desc_physaddr + \
					 RTW_RING_OFFSET(ring))

/* Radio capture format for RTL8180. */

#define RTW_RX_RADIOTAP_PRESENT	\
	((1 << IEEE80211_RADIOTAP_FLAGS) | (1 << IEEE80211_RADIOTAP_RATE) | \
	 (1 << IEEE80211_RADIOTAP_CHANNEL) | \
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL))

struct rtw_rx_radiotap_header {
	struct ieee80211_radiotap_header	rr_ihdr;
	u_int8_t				rr_flags;
	u_int8_t				rr_rate;
	u_int16_t				rr_chan_freq;
	u_int16_t				rr_chan_flags;
	u_int8_t				rr_antsignal;
} __attribute__((__packed__));

#define RTW_TX_RADIOTAP_PRESENT	((1 << IEEE80211_RADIOTAP_FLAGS) | \
				 (1 << IEEE80211_RADIOTAP_RATE) | \
				 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct rtw_tx_radiotap_header {
	struct ieee80211_radiotap_header	rt_ihdr;
	u_int8_t				rt_flags;
	u_int8_t				rt_rate;
	u_int16_t				rt_chan_freq;
	u_int16_t				rt_chan_flags;
} __attribute__((__packed__));

enum rtw_attach_state {FINISHED, FINISH_DESCMAP_LOAD, FINISH_DESCMAP_CREATE,
	FINISH_DESC_MAP, FINISH_DESC_ALLOC, FINISH_RXMAPS_CREATE,
	FINISH_TXMAPS_CREATE, FINISH_RESET, FINISH_READ_SROM, FINISH_PARSE_SROM,
	FINISH_RF_ATTACH, FINISH_ID_STA, FINISH_TXDESCBLK_SETUP,
	FINISH_TXCTLBLK_SETUP, DETACHED};

struct rtw_hooks {
	void			*rh_shutdown;	/* shutdown hook */
	void			*rh_power;	/* power management hook */
};

struct rtw_mtbl {
	int			(*mt_newstate)(struct ieee80211com *,
					enum ieee80211_state, int);
	void			(*mt_recv_mgmt)(struct ieee80211com *,
				    struct mbuf *, struct ieee80211_node *,
				    int, int, u_int32_t);
	struct ieee80211_node	*(*mt_node_alloc)(struct ieee80211com *);
	void			(*mt_node_free)(struct ieee80211com *,
					struct ieee80211_node *);
};

enum rtw_pwrstate { RTW_OFF = 0, RTW_SLEEP, RTW_ON };

typedef void (*rtw_continuous_tx_cb_t)(void *arg, int);

struct rtw_phy {
	struct rtw_rf	*p_rf;
	struct rtw_regs	*p_regs;
};

struct rtw_bbpset {
	u_int	bb_antatten;
	u_int	bb_chestlim;
	u_int	bb_chsqlim;
	u_int	bb_ifagcdet;
	u_int	bb_ifagcini;
	u_int	bb_ifagclimit;
	u_int	bb_lnadet;
	u_int	bb_sys1;
	u_int	bb_sys2;
	u_int	bb_sys3;
	u_int	bb_trl;
	u_int	bb_txagc;
};

struct rtw_rf {
	void	(*rf_destroy)(struct rtw_rf *);
	/* args: frequency, txpower, power state */
	int	(*rf_init)(struct rtw_rf *, u_int, u_int8_t,
	                  enum rtw_pwrstate);
	/* arg: power state */
	int	(*rf_pwrstate)(struct rtw_rf *, enum rtw_pwrstate);
	/* arg: frequency */
	int	(*rf_tune)(struct rtw_rf *, u_int);
	/* arg: txpower */
	int	(*rf_txpower)(struct rtw_rf *, u_int8_t);
	rtw_continuous_tx_cb_t	rf_continuous_tx_cb;
	void			*rf_continuous_tx_arg;
	struct rtw_bbpset	rf_bbpset;
};

static __inline void
rtw_rf_destroy(struct rtw_rf *rf)
{
	(*rf->rf_destroy)(rf);
}

static __inline int
rtw_rf_init(struct rtw_rf *rf, u_int freq, u_int8_t opaque_txpower,
    enum rtw_pwrstate power)
{
	return (*rf->rf_init)(rf, freq, opaque_txpower, power);
}

static __inline int
rtw_rf_pwrstate(struct rtw_rf *rf, enum rtw_pwrstate power)
{
	return (*rf->rf_pwrstate)(rf, power);
}

static __inline int
rtw_rf_tune(struct rtw_rf *rf, u_int freq)
{
	return (*rf->rf_tune)(rf, freq);
}

static __inline int
rtw_rf_txpower(struct rtw_rf *rf, u_int8_t opaque_txpower)
{
	return (*rf->rf_txpower)(rf, opaque_txpower);
}

typedef int (*rtw_rf_write_t)(struct rtw_regs *, enum rtw_rfchipid, u_int,
    u_int32_t);

struct rtw_rfbus {
	struct rtw_regs		*b_regs;
	rtw_rf_write_t		b_write;
};

static __inline int
rtw_rfbus_write(struct rtw_rfbus *bus, enum rtw_rfchipid rfchipid, u_int addr,
    u_int32_t val)
{
	return (*bus->b_write)(bus->b_regs, rfchipid, addr, val);
}

struct rtw_max2820 {
	struct rtw_rf		mx_rf;
	struct rtw_rfbus	mx_bus;
	int			mx_is_a;	/* 1: MAX2820A/MAX2821A */
};

struct rtw_sa2400 {
	struct rtw_rf		sa_rf;
	struct rtw_rfbus	sa_bus;
	int			sa_digphy;	/* 1: digital PHY */
};

typedef void (*rtw_pwrstate_t)(struct rtw_regs *, enum rtw_pwrstate, int);

struct rtw_softc {
	struct device		sc_dev;
	struct ieee80211com	sc_ic;
	struct rtw_regs		sc_regs;
	bus_dma_tag_t		sc_dmat;
	u_int32_t		sc_flags;

#if 0
	enum rtw_rftype		sc_rftype;
#endif
	enum rtw_attach_state	sc_attach_state;
	enum rtw_rfchipid	sc_rfchipid;
	enum rtw_locale		sc_locale;
	u_int8_t		sc_phydelay;

	/* s/w Tx/Rx descriptors */
	struct rtw_txctl_blk	sc_txctl_blk[RTW_NTXPRI];
	struct rtw_rxctl	sc_rxctl[RTW_RXQLEN];
	u_int			sc_txq;
	u_int			sc_txnext;

	struct rtw_txdesc_blk	sc_txdesc_blk[RTW_NTXPRI];
	struct rtw_rxdesc	*sc_rxdesc;
	u_int			sc_rxnext;

	struct rtw_descs	*sc_descs;

	bus_dma_segment_t	sc_desc_segs;
	int			sc_desc_nsegs;
	bus_dmamap_t		sc_desc_dmamap;
#define	sc_desc_physaddr sc_desc_dmamap->dm_segs[0].ds_addr

	struct rtw_srom		sc_srom;

	enum rtw_pwrstate	sc_pwrstate;

	rtw_pwrstate_t		sc_pwrstate_cb;

	struct rtw_rf		*sc_rf;

	u_int16_t		sc_inten;

	/* interrupt acknowledge hook */
	void (*sc_intr_ack) __P((struct rtw_regs *));

	int			(*sc_enable)(struct rtw_softc *);
	void			(*sc_disable)(struct rtw_softc *);
	void			(*sc_power)(struct rtw_softc *, int);
	struct rtw_mtbl		sc_mtbl;
	struct rtw_hooks	sc_hooks;

	caddr_t			sc_radiobpf;

	struct callout		sc_scan_ch;
	u_int			sc_cur_chan;

	u_int32_t		sc_tsfth;	/* most significant TSFT bits */
	u_int32_t		sc_rcr;		/* RTW_RCR */
	u_int8_t		sc_csthr;	/* carrier-sense threshold */

	int			sc_do_tick;	/* indicate 1s ticks */
	struct timeval		sc_tick0;	/* first tick */

	uint8_t		sc_rev;			/* PCI/Cardbus revision */

	union {
		struct rtw_rx_radiotap_header	tap;
		u_int8_t			pad[64];
	} sc_rxtapu;
	union {
		struct rtw_tx_radiotap_header	tap;
		u_int8_t			pad[64];
	} sc_txtapu;
};

#define	sc_if		sc_ic.ic_if
#define sc_rxtap	sc_rxtapu.tap
#define sc_txtap	sc_txtapu.tap

void rtw_txdac_enable(struct rtw_regs *, int);
void rtw_anaparm_enable(struct rtw_regs *, int);
void rtw_config0123_enable(struct rtw_regs *, int);
void rtw_continuous_tx_enable(struct rtw_regs *, int);

void rtw_attach(struct rtw_softc *);
int rtw_detach(struct rtw_softc *);
int rtw_intr(void *);

void rtw_disable(struct rtw_softc *);
int rtw_enable(struct rtw_softc *);

int rtw_activate(struct device *, enum devact);
void rtw_power(int, void *);
void rtw_shutdown(void *);

const char *rtw_pwrstate_string(enum rtw_pwrstate);

#endif /* _DEV_IC_RTWVAR_H_ */
