/*	$NetBSD: atw.c,v 1.24.2.2.2.1 2005/01/24 21:38:46 he Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2000, 2002, 2003, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Young, by Jason R. Thorpe, and by Charles M. Hannum.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Device driver for the ADMtek ADM8211 802.11 MAC/BBP.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: atw.c,v 1.24.2.2.2.1 2005/01/24 21:38:46 he Exp $");

#include "bpfilter.h"

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
#include <sys/time.h>

#include <machine/endian.h>

#include <uvm/uvm_extern.h>
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_compat.h>
#include <net80211/ieee80211_radiotap.h>

#if NBPFILTER > 0 
#include <net/bpf.h>
#endif 

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/atwreg.h>
#include <dev/ic/rf3000reg.h>
#include <dev/ic/si4136reg.h>
#include <dev/ic/atwvar.h>
#include <dev/ic/smc93cx6var.h>

/* XXX TBD open questions
 *
 *
 * When should I set DSSS PAD in reg 0x15 of RF3000? In 1-2Mbps
 * modes only, or all modes (5.5-11 Mbps CCK modes, too?) Does the MAC
 * handle this for me?
 *
 */
/* device attachment
 *
 *    print TOFS[012]
 *
 * device initialization
 *
 *    clear ATW_FRCTL_MAXPSP to disable max power saving
 *    set ATW_TXBR_ALCUPDATE to enable ALC
 *    set TOFS[012]? (hope not)
 *    disable rx/tx
 *    set ATW_PAR_SWR (software reset)
 *    wait for ATW_PAR_SWR clear
 *    disable interrupts
 *    ack status register 
 *    enable interrupts 
 *
 * rx/tx initialization
 *
 *    disable rx/tx w/ ATW_NAR_SR, ATW_NAR_ST
 *    allocate and init descriptor rings
 *    write ATW_PAR_DSL (descriptor skip length)
 *    write descriptor base addrs: ATW_TDBD, ATW_TDBP, write ATW_RDB		
 *    write ATW_NAR_SQ for one/both transmit descriptor rings
 *    write ATW_NAR_SQ for one/both transmit descriptor rings
 *    enable rx/tx w/ ATW_NAR_SR, ATW_NAR_ST
 *
 * rx/tx end
 *
 *    stop DMA
 *    disable rx/tx w/ ATW_NAR_SR, ATW_NAR_ST
 *    flush tx w/ ATW_NAR_HF
 *
 * scan
 *
 *    initialize rx/tx
 *
 * IBSS join/create
 *
 *    set ATW_NAR_EA (is set by ASIC?)
 *
 * BSS join: (re)association response
 *
 *    set ATW_FRCTL_AID
 *
 * optimizations ???
 *
 */

#define	VOODOO_DUR_11_ROUNDING		0x01 /* necessary */
#define	VOODOO_DUR_2_4_SPECIALCASE	0x02 /* NOT necessary */
int atw_voodoo = VOODOO_DUR_11_ROUNDING;

int atw_rfio_enable_delay = 20 * 1000;
int atw_rfio_disable_delay = 2 * 1000;
int atw_writewep_delay = 5;
int atw_beacon_len_adjust = 4;
int atw_dwelltime = 200;

#ifdef ATW_DEBUG
int atw_xhdrctl = 0;
int atw_xrtylmt = ~0;
int atw_xservice = IEEE80211_PLCP_SERVICE;
int atw_xpaylen = 0;

int atw_debug = 0;

#define ATW_DPRINTF(x)	if (atw_debug > 0) printf x
#define ATW_DPRINTF2(x)	if (atw_debug > 1) printf x
#define ATW_DPRINTF3(x)	if (atw_debug > 2) printf x
#define	DPRINTF(sc, x)	if ((sc)->sc_ic.ic_if.if_flags & IFF_DEBUG) printf x
#define	DPRINTF2(sc, x)	if ((sc)->sc_ic.ic_if.if_flags & IFF_DEBUG) ATW_DPRINTF2(x)
#define	DPRINTF3(sc, x)	if ((sc)->sc_ic.ic_if.if_flags & IFF_DEBUG) ATW_DPRINTF3(x)
static void atw_print_regs(struct atw_softc *, const char *);
static void atw_rf3000_print(struct atw_softc *);
static void atw_si4126_print(struct atw_softc *);
static void atw_dump_pkt(struct ifnet *, struct mbuf *);
#else
#define ATW_DPRINTF(x)
#define ATW_DPRINTF2(x)
#define ATW_DPRINTF3(x)
#define	DPRINTF(sc, x)	/* nothing */
#define	DPRINTF2(sc, x)	/* nothing */
#define	DPRINTF3(sc, x)	/* nothing */
#endif

#ifdef ATW_STATS
void	atw_print_stats(struct atw_softc *);
#endif

void	atw_start(struct ifnet *);
void	atw_watchdog(struct ifnet *);
int	atw_ioctl(struct ifnet *, u_long, caddr_t);
int	atw_init(struct ifnet *);
void	atw_txdrain(struct atw_softc *);
void	atw_stop(struct ifnet *, int);

void	atw_reset(struct atw_softc *);
int	atw_read_srom(struct atw_softc *);

void	atw_shutdown(void *);

void	atw_rxdrain(struct atw_softc *);
int	atw_add_rxbuf(struct atw_softc *, int);
void	atw_idle(struct atw_softc *, u_int32_t);

int	atw_enable(struct atw_softc *);
void	atw_disable(struct atw_softc *);
void	atw_power(int, void *);

void	atw_rxintr(struct atw_softc *);
void	atw_txintr(struct atw_softc *);
void	atw_linkintr(struct atw_softc *, u_int32_t);

static int atw_newstate(struct ieee80211com *, enum ieee80211_state, int);
static void atw_tsf(struct atw_softc *);
static void atw_start_beacon(struct atw_softc *, int);
static void atw_write_wep(struct atw_softc *);
static void atw_write_bssid(struct atw_softc *);
static void atw_write_bcn_thresh(struct atw_softc *);
static void atw_write_ssid(struct atw_softc *);
static void atw_write_sup_rates(struct atw_softc *);
static void atw_clear_sram(struct atw_softc *);
static void atw_write_sram(struct atw_softc *, u_int, u_int8_t *, u_int);
static int atw_media_change(struct ifnet *);
static void atw_media_status(struct ifnet *, struct ifmediareq *);
static void atw_filter_setup(struct atw_softc *);
static void atw_frame_setdurs(struct atw_softc *, struct atw_frame *, int, int);
static __inline u_int64_t atw_predict_beacon(u_int64_t, u_int32_t);
static void atw_recv_beacon(struct ieee80211com *, struct mbuf *,
    struct ieee80211_node *, int, int, u_int32_t);
static void atw_recv_mgmt(struct ieee80211com *, struct mbuf *,
    struct ieee80211_node *, int, int, u_int32_t);
static void atw_node_free(struct ieee80211com *, struct ieee80211_node *);
static struct ieee80211_node *atw_node_alloc(struct ieee80211com *);

static int atw_tune(struct atw_softc *);

static void atw_rfio_enable(struct atw_softc *, int);

/* RFMD RF3000 Baseband Processor */
static int atw_rf3000_init(struct atw_softc *);
static int atw_rf3000_tune(struct atw_softc *, u_int8_t);
static int atw_rf3000_write(struct atw_softc *, u_int, u_int);
#ifdef ATW_DEBUG
static int atw_rf3000_read(struct atw_softc *sc, u_int, u_int *);
#endif /* ATW_DEBUG */

/* Silicon Laboratories Si4126 RF/IF Synthesizer */
static int atw_si4126_tune(struct atw_softc *, u_int8_t);
static int atw_si4126_write(struct atw_softc *, u_int, u_int);
#ifdef ATW_DEBUG
static int atw_si4126_read(struct atw_softc *, u_int, u_int *);
#endif /* ATW_DEBUG */

const struct atw_txthresh_tab atw_txthresh_tab_lo[] = ATW_TXTHRESH_TAB_LO_RATE;
const struct atw_txthresh_tab atw_txthresh_tab_hi[] = ATW_TXTHRESH_TAB_HI_RATE;

const char *atw_tx_state[] = {
	"STOPPED",
	"RUNNING - FETCH",
	"RUNNING - WAIT",
	"RUNNING - READING",
	"-- RESERVED1 --",
	"-- RESERVED2 --",
	"SUSPENDED",
	"RUNNING - CLOSE"
};

const char *atw_rx_state[] = {
	"STOPPED",
	"RUNNING - FETCH",
	"RUNNING - CHECK",
	"RUNNING - WAIT",
	"SUSPENDED",
	"RUNNING - CLOSE",
	"RUNNING - FLUSH",
	"RUNNING - QUEUE"
};

int
atw_activate(struct device *self, enum devact act)
{
	struct atw_softc *sc = (struct atw_softc *)self;
	int rv = 0, s;

	s = splnet();
	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ic.ic_if);
		break;
	}
	splx(s);
	return rv;
}

/*
 * atw_enable:
 *
 *	Enable the ADM8211 chip.
 */
int
atw_enable(struct atw_softc *sc)
{

	if (ATW_IS_ENABLED(sc) == 0) {
		if (sc->sc_enable != NULL && (*sc->sc_enable)(sc) != 0) {
			printf("%s: device enable failed\n",
			    sc->sc_dev.dv_xname);
			return (EIO);
		}
		sc->sc_flags |= ATWF_ENABLED;
	}
	return (0);
}

/*
 * atw_disable:
 *
 *	Disable the ADM8211 chip.
 */
void
atw_disable(struct atw_softc *sc)
{
	if (!ATW_IS_ENABLED(sc))
		return;
	if (sc->sc_disable != NULL)
		(*sc->sc_disable)(sc);
	sc->sc_flags &= ~ATWF_ENABLED;
}

/* Returns -1 on failure. */
int
atw_read_srom(struct atw_softc *sc)
{
	struct seeprom_descriptor sd;
	u_int32_t reg;

	(void)memset(&sd, 0, sizeof(sd));

	reg = ATW_READ(sc, ATW_TEST0);

	if ((reg & (ATW_TEST0_EPNE|ATW_TEST0_EPSNM)) != 0) {
		printf("%s: bad or missing/bad SROM\n", sc->sc_dev.dv_xname);
		return -1;
	}

	switch (reg & ATW_TEST0_EPTYP_MASK) {
	case ATW_TEST0_EPTYP_93c66:
		ATW_DPRINTF(("%s: 93c66 SROM\n", sc->sc_dev.dv_xname));
		sc->sc_sromsz = 512;
		sd.sd_chip = C56_66;
		break;
	case ATW_TEST0_EPTYP_93c46:
		ATW_DPRINTF(("%s: 93c46 SROM\n", sc->sc_dev.dv_xname));
		sc->sc_sromsz = 128;
		sd.sd_chip = C46;
		break;
	default:
		printf("%s: unknown SROM type %d\n", sc->sc_dev.dv_xname,
		    MASK_AND_RSHIFT(reg, ATW_TEST0_EPTYP_MASK));
		return -1;
	}

	sc->sc_srom = malloc(sc->sc_sromsz, M_DEVBUF, M_NOWAIT);

	if (sc->sc_srom == NULL) {
		printf("%s: unable to allocate SROM buffer\n",
		    sc->sc_dev.dv_xname);
		return -1;
	}

	(void)memset(sc->sc_srom, 0, sc->sc_sromsz);

	/* ADM8211 has a single 32-bit register for controlling the
	 * 93cx6 SROM.  Bit SRS enables the serial port. There is no
	 * "ready" bit. The ADM8211 input/output sense is the reverse
	 * of read_seeprom's.
	 */
	sd.sd_tag = sc->sc_st;
	sd.sd_bsh = sc->sc_sh;
	sd.sd_regsize = 4;
	sd.sd_control_offset = ATW_SPR;
	sd.sd_status_offset = ATW_SPR;
	sd.sd_dataout_offset = ATW_SPR;
	sd.sd_CK = ATW_SPR_SCLK;
	sd.sd_CS = ATW_SPR_SCS;
	sd.sd_DI = ATW_SPR_SDO;
	sd.sd_DO = ATW_SPR_SDI;
	sd.sd_MS = ATW_SPR_SRS;
	sd.sd_RDY = 0;

	if (!read_seeprom(&sd, sc->sc_srom, 0, sc->sc_sromsz/2)) {
		printf("%s: could not read SROM\n", sc->sc_dev.dv_xname);
		free(sc->sc_srom, M_DEVBUF);
		return -1;
	}
#ifdef ATW_DEBUG
	{
		int i;
		ATW_DPRINTF(("\nSerial EEPROM:\n\t"));
		for (i = 0; i < sc->sc_sromsz/2; i = i + 1) {
			if (((i % 8) == 0) && (i != 0)) {
				ATW_DPRINTF(("\n\t"));
			}
			ATW_DPRINTF((" 0x%x", sc->sc_srom[i]));
		}
		ATW_DPRINTF(("\n"));
	}
#endif /* ATW_DEBUG */
	return 0;
}

#ifdef ATW_DEBUG
static void
atw_print_regs(struct atw_softc *sc, const char *where)
{
#define PRINTREG(sc, reg) \
	ATW_DPRINTF2(("%s: reg[ " #reg " / %03x ] = %08x\n", \
	    sc->sc_dev.dv_xname, reg, ATW_READ(sc, reg)))

	ATW_DPRINTF2(("%s: %s\n", sc->sc_dev.dv_xname, where));

	PRINTREG(sc, ATW_PAR);
	PRINTREG(sc, ATW_FRCTL);
	PRINTREG(sc, ATW_TDR);
	PRINTREG(sc, ATW_WTDP);
	PRINTREG(sc, ATW_RDR);
	PRINTREG(sc, ATW_WRDP);
	PRINTREG(sc, ATW_RDB);
	PRINTREG(sc, ATW_CSR3A);
	PRINTREG(sc, ATW_TDBD);
	PRINTREG(sc, ATW_TDBP);
	PRINTREG(sc, ATW_STSR);
	PRINTREG(sc, ATW_CSR5A);
	PRINTREG(sc, ATW_NAR);
	PRINTREG(sc, ATW_CSR6A);
	PRINTREG(sc, ATW_IER);
	PRINTREG(sc, ATW_CSR7A);
	PRINTREG(sc, ATW_LPC);
	PRINTREG(sc, ATW_TEST1);
	PRINTREG(sc, ATW_SPR);
	PRINTREG(sc, ATW_TEST0);
	PRINTREG(sc, ATW_WCSR);
	PRINTREG(sc, ATW_WPDR);
	PRINTREG(sc, ATW_GPTMR);
	PRINTREG(sc, ATW_GPIO);
	PRINTREG(sc, ATW_BBPCTL);
	PRINTREG(sc, ATW_SYNCTL);
	PRINTREG(sc, ATW_PLCPHD);
	PRINTREG(sc, ATW_MMIWADDR);
	PRINTREG(sc, ATW_MMIRADDR1);
	PRINTREG(sc, ATW_MMIRADDR2);
	PRINTREG(sc, ATW_TXBR);
	PRINTREG(sc, ATW_CSR15A);
	PRINTREG(sc, ATW_ALCSTAT);
	PRINTREG(sc, ATW_TOFS2);
	PRINTREG(sc, ATW_CMDR);
	PRINTREG(sc, ATW_PCIC);
	PRINTREG(sc, ATW_PMCSR);
	PRINTREG(sc, ATW_PAR0);
	PRINTREG(sc, ATW_PAR1);
	PRINTREG(sc, ATW_MAR0);
	PRINTREG(sc, ATW_MAR1);
	PRINTREG(sc, ATW_ATIMDA0);
	PRINTREG(sc, ATW_ABDA1);
	PRINTREG(sc, ATW_BSSID0);
	PRINTREG(sc, ATW_TXLMT);
	PRINTREG(sc, ATW_MIBCNT);
	PRINTREG(sc, ATW_BCNT);
	PRINTREG(sc, ATW_TSFTH);
	PRINTREG(sc, ATW_TSC);
	PRINTREG(sc, ATW_SYNRF);
	PRINTREG(sc, ATW_BPLI);
	PRINTREG(sc, ATW_CAP0);
	PRINTREG(sc, ATW_CAP1);
	PRINTREG(sc, ATW_RMD);
	PRINTREG(sc, ATW_CFPP);
	PRINTREG(sc, ATW_TOFS0);
	PRINTREG(sc, ATW_TOFS1);
	PRINTREG(sc, ATW_IFST);
	PRINTREG(sc, ATW_RSPT);
	PRINTREG(sc, ATW_TSFTL);
	PRINTREG(sc, ATW_WEPCTL);
	PRINTREG(sc, ATW_WESK);
	PRINTREG(sc, ATW_WEPCNT);
	PRINTREG(sc, ATW_MACTEST);
	PRINTREG(sc, ATW_FER);
	PRINTREG(sc, ATW_FEMR);
	PRINTREG(sc, ATW_FPSR);
	PRINTREG(sc, ATW_FFER);
#undef PRINTREG
}
#endif /* ATW_DEBUG */

/*
 * Finish attaching an ADMtek ADM8211 MAC.  Called by bus-specific front-end.
 */
void
atw_attach(struct atw_softc *sc)
{
	static const u_int8_t empty_macaddr[IEEE80211_ADDR_LEN] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int country_code, error, i, nrate;
	u_int32_t reg;
	static const char *type_strings[] = {"Intersil (not supported)",
	    "RFMD", "Marvel (not supported)"};

	sc->sc_txth = atw_txthresh_tab_lo;

	SIMPLEQ_INIT(&sc->sc_txfreeq);
	SIMPLEQ_INIT(&sc->sc_txdirtyq);

#ifdef ATW_DEBUG
	atw_print_regs(sc, "atw_attach");
#endif /* ATW_DEBUG */

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct atw_control_data), PAGE_SIZE, 0, &sc->sc_cdseg,
	    1, &sc->sc_cdnseg, 0)) != 0) {
		printf("%s: unable to allocate control data, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_cdseg, sc->sc_cdnseg,
	    sizeof(struct atw_control_data), (caddr_t *)&sc->sc_control_data,
	    BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map control data, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct atw_control_data), 1,
	    sizeof(struct atw_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		printf("%s: unable to create control data DMA map, "
		    "error = %d\n", sc->sc_dev.dv_xname, error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct atw_control_data), NULL,
	    0)) != 0) {
		printf("%s: unable to load control data DMA map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_3;
	}

	/*
	 * Create the transmit buffer DMA maps.
	 */
	sc->sc_ntxsegs = ATW_NTXSEGS;
	for (i = 0; i < ATW_TXQUEUELEN; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    sc->sc_ntxsegs, MCLBYTES, 0, 0,
		    &sc->sc_txsoft[i].txs_dmamap)) != 0) {
			printf("%s: unable to create tx DMA map %d, "
			    "error = %d\n", sc->sc_dev.dv_xname, i, error);
			goto fail_4;
		}
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < ATW_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
			printf("%s: unable to create rx DMA map %d, "
			    "error = %d\n", sc->sc_dev.dv_xname, i, error);
			goto fail_5;
		}
	}
	for (i = 0; i < ATW_NRXDESC; i++) {
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	/* Reset the chip to a known state. */
	atw_reset(sc);

	if (atw_read_srom(sc) == -1)
		return;

	sc->sc_rftype = MASK_AND_RSHIFT(sc->sc_srom[ATW_SR_CSR20],
	    ATW_SR_RFTYPE_MASK);

	sc->sc_bbptype = MASK_AND_RSHIFT(sc->sc_srom[ATW_SR_CSR20],
	    ATW_SR_BBPTYPE_MASK);

	if (sc->sc_rftype > sizeof(type_strings)/sizeof(type_strings[0])) {
		printf("%s: unknown RF\n", sc->sc_dev.dv_xname);
		return;
	}
	if (sc->sc_bbptype > sizeof(type_strings)/sizeof(type_strings[0])) {
		printf("%s: unknown BBP\n", sc->sc_dev.dv_xname);
		return;
	}

	printf("%s: %s RF, %s BBP", sc->sc_dev.dv_xname,
	    type_strings[sc->sc_rftype], type_strings[sc->sc_bbptype]);

	/* XXX There exists a Linux driver which seems to use RFType = 0 for
	 * MARVEL. My bug, or theirs?
	 */

	reg = LSHIFT(sc->sc_rftype, ATW_SYNCTL_RFTYPE_MASK);

	switch (sc->sc_rftype) {
	case ATW_RFTYPE_INTERSIL:
		reg |= ATW_SYNCTL_CS1;
		break;
	case ATW_RFTYPE_RFMD:
		reg |= ATW_SYNCTL_CS0;
		break;
	case ATW_RFTYPE_MARVEL:
		break;
	}

	sc->sc_synctl_rd = reg | ATW_SYNCTL_RD;
	sc->sc_synctl_wr = reg | ATW_SYNCTL_WR;

	reg = LSHIFT(sc->sc_bbptype, ATW_BBPCTL_TYPE_MASK);

	switch (sc->sc_bbptype) {
	case ATW_RFTYPE_INTERSIL:
		reg |= ATW_BBPCTL_TWI;
		break;
	case ATW_RFTYPE_RFMD:
		reg |= ATW_BBPCTL_RF3KADDR_ADDR | ATW_BBPCTL_NEGEDGE_DO |
		    ATW_BBPCTL_CCA_ACTLO;
		break;
	case ATW_RFTYPE_MARVEL:
		break;
	}

	sc->sc_bbpctl_wr = reg | ATW_BBPCTL_WR;
	sc->sc_bbpctl_rd = reg | ATW_BBPCTL_RD;

	/*
	 * From this point forward, the attachment cannot fail.  A failure
	 * before this point releases all resources that may have been
	 * allocated.
	 */
	sc->sc_flags |= ATWF_ATTACHED /* | ATWF_RTSCTS */;

	ATW_DPRINTF((" SROM MAC %04x%04x%04x",
	    htole16(sc->sc_srom[ATW_SR_MAC00]),
	    htole16(sc->sc_srom[ATW_SR_MAC01]),
	    htole16(sc->sc_srom[ATW_SR_MAC10])));

	country_code = MASK_AND_RSHIFT(sc->sc_srom[ATW_SR_CTRY_CR29],
	    ATW_SR_CTRY_MASK);

#define ADD_CHANNEL(_ic, _chan) do {					\
	_ic->ic_channels[_chan].ic_flags = IEEE80211_CHAN_B;		\
	_ic->ic_channels[_chan].ic_freq =				\
	    ieee80211_ieee2mhz(_chan, _ic->ic_channels[_chan].ic_flags);\
} while (0)

	/* Find available channels */
	switch (country_code) {
	case COUNTRY_MMK2:	/* 1-14 */
		ADD_CHANNEL(ic, 14);
		/*FALLTHROUGH*/
	case COUNTRY_ETSI:	/* 1-13 */
		for (i = 1; i <= 13; i++)
			ADD_CHANNEL(ic, i);
		break;
	case COUNTRY_FCC:	/* 1-11 */
	case COUNTRY_IC:	/* 1-11 */
		for (i = 1; i <= 11; i++)
			ADD_CHANNEL(ic, i);
		break;
	case COUNTRY_MMK:	/* 14 */
		ADD_CHANNEL(ic, 14);
		break;
	case COUNTRY_FRANCE:	/* 10-13 */
		for (i = 10; i <= 13; i++)
			ADD_CHANNEL(ic, i);
		break;
	default:	/* assume channels 10-11 */
	case COUNTRY_SPAIN:	/* 10-11 */
		for (i = 10; i <= 11; i++)
			ADD_CHANNEL(ic, i);
		break;
	}

	/* Read the MAC address. */
	reg = ATW_READ(sc, ATW_PAR0);
	ic->ic_myaddr[0] = MASK_AND_RSHIFT(reg, ATW_PAR0_PAB0_MASK);
	ic->ic_myaddr[1] = MASK_AND_RSHIFT(reg, ATW_PAR0_PAB1_MASK);
	ic->ic_myaddr[2] = MASK_AND_RSHIFT(reg, ATW_PAR0_PAB2_MASK);
	ic->ic_myaddr[3] = MASK_AND_RSHIFT(reg, ATW_PAR0_PAB3_MASK);
	reg = ATW_READ(sc, ATW_PAR1);
	ic->ic_myaddr[4] = MASK_AND_RSHIFT(reg, ATW_PAR1_PAB4_MASK);
	ic->ic_myaddr[5] = MASK_AND_RSHIFT(reg, ATW_PAR1_PAB5_MASK);

	if (IEEE80211_ADDR_EQ(ic->ic_myaddr, empty_macaddr)) {
		printf(" could not get mac address, attach failed\n");
		return;
	}

	printf(" 802.11 address %s\n", ether_sprintf(ic->ic_myaddr));

	memcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST |
	    IFF_NOTRAILERS;
	ifp->if_ioctl = atw_ioctl;
	ifp->if_start = atw_start;
	ifp->if_watchdog = atw_watchdog;
	ifp->if_init = atw_init;
	ifp->if_stop = atw_stop;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_phytype = IEEE80211_T_DS;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_caps = IEEE80211_C_PMGT | IEEE80211_C_IBSS |
	    IEEE80211_C_HOSTAP | IEEE80211_C_MONITOR | IEEE80211_C_WEP;

	nrate = 0;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 2;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 4;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 11;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 22;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_nrates = nrate;

	/*
	 * Call MI attach routines.
	 */

	if_attach(ifp);
	ieee80211_ifattach(ifp);

	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = atw_newstate;

	sc->sc_recv_mgmt = ic->ic_recv_mgmt;
	ic->ic_recv_mgmt = atw_recv_mgmt;

	sc->sc_node_free = ic->ic_node_free;
	ic->ic_node_free = atw_node_free;

	sc->sc_node_alloc = ic->ic_node_alloc;
	ic->ic_node_alloc = atw_node_alloc;

	/* possibly we should fill in our own sc_send_prresp, since
	 * the ADM8211 is probably sending probe responses in ad hoc
	 * mode.
	 */

	/* complete initialization */
	ieee80211_media_init(ifp, atw_media_change, atw_media_status);
	callout_init(&sc->sc_scan_ch);

#if NBPFILTER > 0
	bpfattach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + 64, &sc->sc_radiobpf);
#endif

	/*
	 * Make sure the interface is shutdown during reboot.
	 */
	sc->sc_sdhook = shutdownhook_establish(atw_shutdown, sc);
	if (sc->sc_sdhook == NULL)
		printf("%s: WARNING: unable to establish shutdown hook\n",
		    sc->sc_dev.dv_xname);

	/*
	 * Add a suspend hook to make sure we come back up after a
	 * resume.
	 */
	sc->sc_powerhook = powerhook_establish(atw_power, sc);
	if (sc->sc_powerhook == NULL)
		printf("%s: WARNING: unable to establish power hook\n",
		    sc->sc_dev.dv_xname);

	memset(&sc->sc_rxtapu, 0, sizeof(sc->sc_rxtapu));
	sc->sc_rxtap.ar_ihdr.it_len = sizeof(sc->sc_rxtapu);
	sc->sc_rxtap.ar_ihdr.it_present = ATW_RX_RADIOTAP_PRESENT;

	memset(&sc->sc_txtapu, 0, sizeof(sc->sc_txtapu));
	sc->sc_txtap.at_ihdr.it_len = sizeof(sc->sc_txtapu);
	sc->sc_txtap.at_ihdr.it_present = ATW_TX_RADIOTAP_PRESENT;

	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_5:
	for (i = 0; i < ATW_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap == NULL)
			continue;
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_rxsoft[i].rxs_dmamap);
	}
 fail_4:
	for (i = 0; i < ATW_TXQUEUELEN; i++) {
		if (sc->sc_txsoft[i].txs_dmamap == NULL)
			continue;
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_control_data,
	    sizeof(struct atw_control_data));
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_cdseg, sc->sc_cdnseg);
 fail_0:
	return;
}

static struct ieee80211_node *
atw_node_alloc(struct ieee80211com *ic)
{
	struct atw_softc *sc = (struct atw_softc *)ic->ic_if.if_softc;
	struct ieee80211_node *ni = (*sc->sc_node_alloc)(ic);

	DPRINTF(sc, ("%s: alloc node %p\n", sc->sc_dev.dv_xname, ni));
	return ni;
}

static void
atw_node_free(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct atw_softc *sc = (struct atw_softc *)ic->ic_if.if_softc;

	DPRINTF(sc, ("%s: freeing node %p %s\n", sc->sc_dev.dv_xname, ni,
	    ether_sprintf(ni->ni_bssid)));
	(*sc->sc_node_free)(ic, ni);
}

/*
 * atw_reset:
 *
 *	Perform a soft reset on the ADM8211.
 */
void
atw_reset(struct atw_softc *sc)
{
	int i;

	ATW_WRITE(sc, ATW_PAR, ATW_PAR_SWR);

	for (i = 0; i < 10000; i++) {
		if (ATW_ISSET(sc, ATW_PAR, ATW_PAR_SWR) == 0)
			break;
		DELAY(1);
	}

	DPRINTF2(sc, ("%s: atw_reset %d iterations\n", sc->sc_dev.dv_xname, i));

	if (ATW_ISSET(sc, ATW_PAR, ATW_PAR_SWR))
		printf("%s: reset failed to complete\n", sc->sc_dev.dv_xname);

	/* Turn off maximum power saving. */
	ATW_CLR(sc, ATW_FRCTL, ATW_FRCTL_MAXPSP);

	/* Recall EEPROM. */
	ATW_SET(sc, ATW_TEST0, ATW_TEST0_EPRLD);

	DELAY(10 * 1000);

	/* A reset seems to affect the SRAM contents, so put them into
	 * a known state.
	 */
	atw_clear_sram(sc);

	memset(sc->sc_bssid, 0, sizeof(sc->sc_bssid));

	sc->sc_lost_bcn_thresh = 0;
}

static void
atw_clear_sram(struct atw_softc *sc)
{
#if 0
	for (addr = 0; addr < 448; addr++) {
		ATW_WRITE(sc, ATW_WEPCTL,
		    ATW_WEPCTL_WR | ATW_WEPCTL_UNKNOWN0	| addr);
		DELAY(1000);
		ATW_WRITE(sc, ATW_WESK, 0);
		DELAY(1000); /* paranoia */
	}
	return;
#endif
	memset(sc->sc_sram, 0, sizeof(sc->sc_sram));
	/* XXX not for revision 0x20. */
	atw_write_sram(sc, 0, sc->sc_sram, sizeof(sc->sc_sram));
}

/* TBD atw_init
 *
 * set MAC based on ic->ic_bss->myaddr
 * write WEP keys
 * set TX rate
 */

/*
 * atw_init:		[ ifnet interface function ]
 *
 *	Initialize the interface.  Must be called at splnet().
 */
int
atw_init(struct ifnet *ifp)
{
	struct atw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct atw_txsoft *txs;
	struct atw_rxsoft *rxs;
	u_int32_t reg;
	int i, error = 0;

	if ((error = atw_enable(sc)) != 0)
		goto out;

	/*
	 * Cancel any pending I/O. This also resets.
	 */
	atw_stop(ifp, 0);

	ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	DPRINTF(sc, ("%s: channel %d freq %d flags 0x%04x\n",
	    __func__, ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan),
	    ic->ic_bss->ni_chan->ic_freq, ic->ic_bss->ni_chan->ic_flags));

	/* Turn off APM??? (A binary-only driver does this.)
	 *
	 * Set Rx store-and-forward mode.
	 */
	reg = ATW_READ(sc, ATW_CMDR);
	reg &= ~ATW_CMDR_APM;
	reg &= ~ATW_CMDR_DRT_MASK;
	reg |= ATW_CMDR_RTE | LSHIFT(0x2, ATW_CMDR_DRT_MASK);

	ATW_WRITE(sc, ATW_CMDR, reg);

	/* Set data rate for PLCP Signal field, 1Mbps = 10 x 100Kb/s.
	 *
	 * XXX a binary-only driver sets a different service field than
	 * 0. why?
	 */
	reg = ATW_READ(sc, ATW_PLCPHD);
	reg &= ~(ATW_PLCPHD_SERVICE_MASK|ATW_PLCPHD_SIGNAL_MASK);
	reg |= LSHIFT(10, ATW_PLCPHD_SIGNAL_MASK) |
	    LSHIFT(0xb0, ATW_PLCPHD_SERVICE_MASK);
	ATW_WRITE(sc, ATW_PLCPHD, reg);

	/* XXX this magic can probably be figured out from the RFMD docs */
	reg = LSHIFT(4, ATW_TOFS2_PWR1UP_MASK)    | /* 8 ms = 4 * 2 ms */
	      LSHIFT(13, ATW_TOFS2_PWR0PAPE_MASK) | /* 13 us */
	      LSHIFT(8, ATW_TOFS2_PWR1PAPE_MASK)  | /* 8 us */
	      LSHIFT(5, ATW_TOFS2_PWR0TRSW_MASK)  | /* 5 us */
	      LSHIFT(12, ATW_TOFS2_PWR1TRSW_MASK) | /* 12 us */
	      LSHIFT(13, ATW_TOFS2_PWR0PE2_MASK)  | /* 13 us */
	      LSHIFT(4, ATW_TOFS2_PWR1PE2_MASK)   | /* 4 us */
	      LSHIFT(5, ATW_TOFS2_PWR0TXPE_MASK);  /* 5 us */
	ATW_WRITE(sc, ATW_TOFS2, reg);

	ATW_WRITE(sc, ATW_TXLMT, LSHIFT(512, ATW_TXLMT_MTMLT_MASK) |
	                         LSHIFT(224, ATW_TXLMT_SRTYLIM_MASK));

	/* XXX this resets an Intersil RF front-end? */
	/* TBD condition on Intersil RFType? */
	ATW_WRITE(sc, ATW_SYNRF, ATW_SYNRF_INTERSIL_EN);
	DELAY(10 * 1000);
	ATW_WRITE(sc, ATW_SYNRF, 0);
	DELAY(5 * 1000);

	/* 16 TU max duration for contention-free period */
	reg = ATW_READ(sc, ATW_CFPP) & ~ATW_CFPP_CFPMD;
	ATW_WRITE(sc, ATW_CFPP, reg | LSHIFT(16, ATW_CFPP_CFPMD));

	/* XXX I guess that the Cardbus clock is 22MHz?
	 * I am assuming that the role of ATW_TOFS0_USCNT is
	 * to divide the bus clock to get a 1MHz clock---the datasheet is not
	 * very clear on this point. It says in the datasheet that it is
	 * possible for the ADM8211 to accomodate bus speeds between 22MHz
	 * and 33MHz; maybe this is the way? I see a binary-only driver write
	 * these values. These values are also the power-on default.
	 */
	ATW_WRITE(sc, ATW_TOFS0,
	    LSHIFT(22, ATW_TOFS0_USCNT_MASK) |
	    ATW_TOFS0_TUCNT_MASK /* set all bits in TUCNT */);

	/* Initialize interframe spacing.  EIFS=0x64 is used by a binary-only
	 * driver. Go figure.
	 */
	reg = LSHIFT(IEEE80211_DUR_DS_SLOT, ATW_IFST_SLOT_MASK) |
	      LSHIFT(22 * IEEE80211_DUR_DS_SIFS /* # of 22MHz cycles */,
	             ATW_IFST_SIFS_MASK) |
	      LSHIFT(IEEE80211_DUR_DS_DIFS, ATW_IFST_DIFS_MASK) |
	      LSHIFT(0x64 /* IEEE80211_DUR_DS_EIFS */, ATW_IFST_EIFS_MASK);

	ATW_WRITE(sc, ATW_IFST, reg);

	/* XXX More magic. Might relate to ACK timing. */
	ATW_WRITE(sc, ATW_RSPT, LSHIFT(0xffff, ATW_RSPT_MART_MASK) |
	    LSHIFT(0xff, ATW_RSPT_MIRT_MASK));

	/* Set up the MMI read/write addresses for the BBP.
	 *
	 * TBD find out the Marvel settings.
	 */
	switch (sc->sc_bbptype) {
	case ATW_BBPTYPE_INTERSIL:
		ATW_WRITE(sc, ATW_MMIWADDR, ATW_MMIWADDR_INTERSIL);
		ATW_WRITE(sc, ATW_MMIRADDR1, ATW_MMIRADDR1_INTERSIL);
		ATW_WRITE(sc, ATW_MMIRADDR2, ATW_MMIRADDR2_INTERSIL);
		break;
	case ATW_BBPTYPE_MARVEL:
		break;
	case ATW_BBPTYPE_RFMD:
		ATW_WRITE(sc, ATW_MMIWADDR, ATW_MMIWADDR_RFMD);
		ATW_WRITE(sc, ATW_MMIRADDR1, ATW_MMIRADDR1_RFMD);
		ATW_WRITE(sc, ATW_MMIRADDR2, ATW_MMIRADDR2_RFMD);
	default:
		break;
	}

	sc->sc_wepctl = 0;
	ATW_WRITE(sc, ATW_MACTEST, ATW_MACTEST_MMI_USETXCLK);

	if ((error = atw_rf3000_init(sc)) != 0)
		goto out;

	/*
	 * Initialize the PCI Access Register.
	 */
	sc->sc_busmode = ATW_PAR_BAR;	/* XXX what is this? */

	/*
	 * If we're allowed to do so, use Memory Read Line
	 * and Memory Read Multiple.
	 *
	 * XXX Should we use Memory Write and Invalidate?
	 */
	if (sc->sc_flags & ATWF_MRL)
		sc->sc_busmode |= ATW_PAR_MRLE;
	if (sc->sc_flags & ATWF_MRM)
		sc->sc_busmode |= ATW_PAR_MRME;
	if (sc->sc_flags & ATWF_MWI)
		sc->sc_busmode |= ATW_PAR_MWIE;
	if (sc->sc_maxburst == 0)
		sc->sc_maxburst = 8;	/* ADM8211 default */

	switch (sc->sc_cacheline) {
	default:
		/* Use burst length. */
		break;
	case 8:
		sc->sc_busmode |= ATW_PAR_CAL_8DW;
		break;
	case 16:
		sc->sc_busmode |= ATW_PAR_CAL_16DW;
		break;
	case 32:
		sc->sc_busmode |= ATW_PAR_CAL_32DW;
		break;
	}
	switch (sc->sc_maxburst) {
	case 1:
		sc->sc_busmode |= ATW_PAR_PBL_1DW;
		break;
	case 2:
		sc->sc_busmode |= ATW_PAR_PBL_2DW;
		break;
	case 4:
		sc->sc_busmode |= ATW_PAR_PBL_4DW;
		break;
	case 8:
		sc->sc_busmode |= ATW_PAR_PBL_8DW;
		break;
	case 16:
		sc->sc_busmode |= ATW_PAR_PBL_16DW;
		break;
	case 32:
		sc->sc_busmode |= ATW_PAR_PBL_32DW;
		break;
	default:
		sc->sc_busmode |= ATW_PAR_PBL_8DW;
		break;
	}

	ATW_WRITE(sc, ATW_PAR, sc->sc_busmode);
	DPRINTF(sc, ("%s: ATW_PAR %08x busmode %08x\n", sc->sc_dev.dv_xname,
	    ATW_READ(sc, ATW_PAR), sc->sc_busmode));

	/*
	 * Initialize the OPMODE register.  We don't write it until
	 * we're ready to begin the transmit and receive processes.
	 */
	sc->sc_opmode = ATW_NAR_SR | ATW_NAR_ST |
	    sc->sc_txth[sc->sc_txthresh].txth_opmode;

	/*
	 * Initialize the transmit descriptor ring.
	 */
	memset(sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	for (i = 0; i < ATW_NTXDESC; i++) {
		/* no transmit chaining */
		sc->sc_txdescs[i].at_ctl = 0 /* ATW_TXFLAG_TCH */;
		sc->sc_txdescs[i].at_buf2 =
		    htole32(ATW_CDTXADDR(sc, ATW_NEXTTX(i)));
	}
	/* use ring mode */
	sc->sc_txdescs[ATW_NTXDESC - 1].at_ctl |= ATW_TXFLAG_TER;
	ATW_CDTXSYNC(sc, 0, ATW_NTXDESC,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->sc_txfree = ATW_NTXDESC;
	sc->sc_txnext = 0;

	/*
	 * Initialize the transmit job descriptors.
	 */
	SIMPLEQ_INIT(&sc->sc_txfreeq);
	SIMPLEQ_INIT(&sc->sc_txdirtyq);
	for (i = 0; i < ATW_TXQUEUELEN; i++) {
		txs = &sc->sc_txsoft[i];
		txs->txs_mbuf = NULL;
		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < ATW_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL) {
			if ((error = atw_add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				    sc->sc_dev.dv_xname, i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				atw_rxdrain(sc);
				goto out;
			}
		} else
			ATW_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;

	/* disable all wake-up events */
	ATW_CLR(sc, ATW_WCSR, ATW_WCSR_WP1E|ATW_WCSR_WP2E|ATW_WCSR_WP3E|
	                      ATW_WCSR_WP4E|ATW_WCSR_WP5E|ATW_WCSR_TSFTWE|
			      ATW_WCSR_TIMWE|ATW_WCSR_ATIMWE|ATW_WCSR_KEYWE|
			      ATW_WCSR_WFRE|ATW_WCSR_MPRE|ATW_WCSR_LSOE);

	/* ack all wake-up events */
	ATW_SET(sc, ATW_WCSR, 0);

	/*
	 * Initialize the interrupt mask and enable interrupts.
	 */
	/* normal interrupts */
	sc->sc_inten =  ATW_INTR_TCI | ATW_INTR_TDU | ATW_INTR_RCI |
	    ATW_INTR_NISS | ATW_INTR_LINKON | ATW_INTR_BCNTC;

	/* abnormal interrupts */
	sc->sc_inten |= ATW_INTR_TPS | ATW_INTR_TLT | ATW_INTR_TRT |
	    ATW_INTR_TUF | ATW_INTR_RDU | ATW_INTR_RPS | ATW_INTR_AISS |
	    ATW_INTR_FBE | ATW_INTR_LINKOFF | ATW_INTR_TSFTF | ATW_INTR_TSCZ;

	sc->sc_linkint_mask = ATW_INTR_LINKON | ATW_INTR_LINKOFF |
	    ATW_INTR_BCNTC | ATW_INTR_TSFTF | ATW_INTR_TSCZ;
	sc->sc_rxint_mask = ATW_INTR_RCI | ATW_INTR_RDU;
	sc->sc_txint_mask = ATW_INTR_TCI | ATW_INTR_TUF | ATW_INTR_TLT |
	    ATW_INTR_TRT;

	sc->sc_linkint_mask &= sc->sc_inten;
	sc->sc_rxint_mask &= sc->sc_inten;
	sc->sc_txint_mask &= sc->sc_inten;

	ATW_WRITE(sc, ATW_IER, sc->sc_inten);
	ATW_WRITE(sc, ATW_STSR, 0xffffffff);
	if (sc->sc_intr_ack != NULL)
		(*sc->sc_intr_ack)(sc);

	DPRINTF(sc, ("%s: ATW_IER %08x, inten %08x\n",
	    sc->sc_dev.dv_xname, ATW_READ(sc, ATW_IER), sc->sc_inten));

	/*
	 * Give the transmit and receive rings to the ADM8211.
	 */
	ATW_WRITE(sc, ATW_TDBD, ATW_CDTXADDR(sc, sc->sc_txnext));
	ATW_WRITE(sc, ATW_RDB, ATW_CDRXADDR(sc, sc->sc_rxptr));

	/* common 802.11 configuration */
	ic->ic_flags &= ~IEEE80211_F_IBSSON;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		sc->sc_opmode &= ~ATW_NAR_EA;
		break;
	case IEEE80211_M_AHDEMO: /* XXX */
	case IEEE80211_M_IBSS:
		ic->ic_flags |= IEEE80211_F_IBSSON;
		/*FALLTHROUGH*/
	case IEEE80211_M_HOSTAP: /* XXX */
		/* EA bit seems important for ad hoc reception. */
		sc->sc_opmode |= ATW_NAR_EA;
		break;
	case IEEE80211_M_MONITOR: /* XXX */
		break;
	}

	atw_start_beacon(sc, 0);

	switch (ic->ic_opmode) {
	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_HOSTAP:
		ic->ic_bss->ni_intval = ic->ic_lintval;
		ic->ic_bss->ni_rssi = 0;
		ic->ic_bss->ni_rstamp = 0;
		break;
	default:					/* XXX */
		break;
	}

	atw_write_ssid(sc);
	atw_write_sup_rates(sc);
	if (ic->ic_caps & IEEE80211_C_WEP)
		atw_write_wep(sc);

	/*
	 * Set the receive filter.  This will start the transmit and
	 * receive processes.
	 */
	atw_filter_setup(sc);

	/*
	 * Start the receive process.
	 */
	ATW_WRITE(sc, ATW_RDR, 0x1);

	/*
	 * Note that the interface is now running.
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ic->ic_state = IEEE80211_S_INIT;

	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		error = ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	else
		error = ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
 out:
	if (error) {
		ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
		ifp->if_timer = 0;
		printf("%s: interface not running\n", sc->sc_dev.dv_xname);
	}
#ifdef ATW_DEBUG
	atw_print_regs(sc, "end of init");
#endif /* ATW_DEBUG */

	return (error);
}

/* enable == 1: host control of RF3000/Si4126 through ATW_SYNCTL.
 *           0: MAC control of RF3000/Si4126.
 *
 * Applies power, or selects RF front-end? Sets reset condition.
 *
 * TBD support non-RFMD BBP, non-SiLabs synth.
 */
static void
atw_rfio_enable(struct atw_softc *sc, int enable)
{
	if (enable) {
		ATW_WRITE(sc, ATW_SYNRF,
		    ATW_SYNRF_SELRF|ATW_SYNRF_PE1|ATW_SYNRF_PHYRST);
		DELAY(atw_rfio_enable_delay);
	} else {
		ATW_WRITE(sc, ATW_SYNRF, 0);
		DELAY(atw_rfio_disable_delay); /* shorter for some reason */
	}
}

static int
atw_tune(struct atw_softc *sc)
{
	int rc;
	u_int32_t reg;
	int chan;
	struct ieee80211com *ic = &sc->sc_ic;

	chan = ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan);
	if (chan == IEEE80211_CHAN_ANY)
		panic("%s: chan == IEEE80211_CHAN_ANY\n", __func__);

	if (chan == sc->sc_cur_chan)
		return 0;

	DPRINTF(sc, ("%s: chan %d -> %d\n", sc->sc_dev.dv_xname,
	    sc->sc_cur_chan, chan));

	atw_idle(sc, ATW_NAR_SR|ATW_NAR_ST);

	if ((rc = atw_si4126_tune(sc, chan)) != 0 ||
	    (rc = atw_rf3000_tune(sc, chan)) != 0)
		printf("%s: failed to tune channel %d\n", sc->sc_dev.dv_xname,
		    chan);

	reg = ATW_READ(sc, ATW_CAP0) & ~ATW_CAP0_CHN_MASK;
	ATW_WRITE(sc, ATW_CAP0,
	    reg | LSHIFT(chan, ATW_CAP0_CHN_MASK));

	ATW_WRITE(sc, ATW_NAR, sc->sc_opmode);

	if (rc == 0)
		sc->sc_cur_chan = chan;

	return rc;
}

#ifdef ATW_DEBUG
static void
atw_si4126_print(struct atw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	u_int addr, val;

	if (atw_debug < 3 || (ifp->if_flags & IFF_DEBUG) == 0)
		return;

	for (addr = 0; addr <= 8; addr++) {
		printf("%s: synth[%d] = ", sc->sc_dev.dv_xname, addr);
		if (atw_si4126_read(sc, addr, &val) == 0) {
			printf("<unknown> (quitting print-out)\n");
			break;
		}
		printf("%05x\n", val);
	}
}
#endif /* ATW_DEBUG */

/* Tune to channel chan by adjusting the Si4126 RF/IF synthesizer.
 *
 * The RF/IF synthesizer produces two reference frequencies for
 * the RF2948B transceiver.  The first frequency the RF2948B requires
 * is two times the so-called "intermediate frequency" (IF). Since
 * a SAW filter on the radio fixes the IF at 374MHz, I program the
 * Si4126 to generate IF LO = 374MHz x 2 = 748MHz.  The second
 * frequency required by the transceiver is the radio frequency
 * (RF). This is a superheterodyne transceiver; for f(chan) the
 * center frequency of the channel we are tuning, RF = f(chan) -
 * IF.
 *
 * XXX I am told by SiLabs that the Si4126 will accept a broader range
 * of XIN than the 2-25MHz mentioned by the datasheet, even *without*
 * XINDIV2 = 1.  I've tried this (it is necessary to double R) and it
 * works, but I have still programmed for XINDIV2 = 1 to be safe.
 */
static int
atw_si4126_tune(struct atw_softc *sc, u_int8_t chan)
{
	int rc = 0;
	u_int mhz;
	u_int R;
	u_int32_t reg;
	u_int16_t gain;

#ifdef ATW_DEBUG
	atw_si4126_print(sc);
#endif /* ATW_DEBUG */

	if (chan == 14)
		mhz = 2484;
	else 
		mhz = 2412 + 5 * (chan - 1);

	/* Tune IF to 748MHz to suit the IF LO input of the
	 * RF2494B, which is 2 x IF. No need to set an IF divider
         * because an IF in 526MHz - 952MHz is allowed.
	 *
	 * XIN is 44.000MHz, so divide it by two to get allowable
	 * range of 2-25MHz. SiLabs tells me that this is not
	 * strictly necessary.
	 */

	R = 44;

	atw_rfio_enable(sc, 1);

	/* Power-up RF, IF synthesizers. */
	if ((rc = atw_si4126_write(sc, SI4126_POWER,
	    SI4126_POWER_PDIB|SI4126_POWER_PDRB)) != 0)
		goto out;

	/* If RF2 N > 2047, then set KP2 to 1. */
	gain = LSHIFT(((mhz - 374) > 2047) ? 1 : 0, SI4126_GAIN_KP2_MASK);

	if ((rc = atw_si4126_write(sc, SI4126_GAIN, gain)) != 0)
		goto out;

	/* set LPWR, too? */
	if ((rc = atw_si4126_write(sc, SI4126_MAIN,
	    SI4126_MAIN_XINDIV2)) != 0)
		goto out;

	/* We set XINDIV2 = 1, so IF = N/(2 * R) * XIN.  XIN = 44MHz.
	 * I choose N = 1496, R = 44 so that 1496/(2 * 44) * 44MHz = 748MHz.
	 */
	if ((rc = atw_si4126_write(sc, SI4126_IFN, 1496)) != 0)
		goto out;

	if ((rc = atw_si4126_write(sc, SI4126_IFR, R)) != 0)
		goto out;

	/* Set RF1 arbitrarily. DO NOT configure RF1 after RF2, because
	 * then RF1 becomes the active RF synthesizer, even on the Si4126,
	 * which has no RF1!
	 */
	if ((rc = atw_si4126_write(sc, SI4126_RF1R, R)) != 0)
		goto out;

	if ((rc = atw_si4126_write(sc, SI4126_RF1N, mhz - 374)) != 0)
		goto out;

	/* N/R * XIN = RF. XIN = 44MHz. We desire RF = mhz - IF,
	 * where IF = 374MHz.  Let's divide XIN to 1MHz. So R = 44.
	 * Now let's multiply it to mhz. So mhz - IF = N.
	 */
	if ((rc = atw_si4126_write(sc, SI4126_RF2R, R)) != 0)
		goto out;

	if ((rc = atw_si4126_write(sc, SI4126_RF2N, mhz - 374)) != 0)
		goto out;

	/* wait 100us from power-up for RF, IF to settle */
	DELAY(100);

	if ((sc->sc_if.if_flags & IFF_LINK1) == 0 || chan == 14) {
		/* XXX there is a binary driver which sends
		 * ATW_GPIO_EN_MASK = 1, ATW_GPIO_O_MASK = 1. I had speculated
		 * that this enables the Si4126 by raising its PWDN#, but I
		 * think that it actually sets the Prism RF front-end
		 * to a special mode for channel 14.  
		 */
		reg = ATW_READ(sc, ATW_GPIO);
		reg &= ~(ATW_GPIO_EN_MASK|ATW_GPIO_O_MASK|ATW_GPIO_I_MASK);
		reg |= LSHIFT(1, ATW_GPIO_EN_MASK) | LSHIFT(1, ATW_GPIO_O_MASK);
		ATW_WRITE(sc, ATW_GPIO, reg);
	}

#ifdef ATW_DEBUG
	atw_si4126_print(sc);
#endif /* ATW_DEBUG */

out:
	atw_rfio_enable(sc, 0);

	return rc;
}

/* Baseline initialization of RF3000 BBP: set CCA mode and enable antenna
 * diversity.
 *
 * Call this w/ Tx/Rx suspended.
 */
static int
atw_rf3000_init(struct atw_softc *sc)
{
	int rc = 0;

	atw_idle(sc, ATW_NAR_SR|ATW_NAR_ST);

	atw_rfio_enable(sc, 1);

	/* enable diversity */
	rc = atw_rf3000_write(sc, RF3000_DIVCTL, RF3000_DIVCTL_ENABLE);

	if (rc != 0)
		goto out;

	/* sensible setting from a binary-only driver */
	rc = atw_rf3000_write(sc, RF3000_GAINCTL,
	    LSHIFT(0x1d, RF3000_GAINCTL_TXVGC_MASK));

	if (rc != 0)
		goto out;

	/* magic from a binary-only driver */
	rc = atw_rf3000_write(sc, RF3000_LOGAINCAL,
	    LSHIFT(0x38, RF3000_LOGAINCAL_CAL_MASK));

	if (rc != 0)
		goto out;

	rc = atw_rf3000_write(sc, RF3000_HIGAINCAL, RF3000_HIGAINCAL_DSSSPAD);

	if (rc != 0)
		goto out;

	rc = atw_rf3000_write(sc, RF3000_OPTIONS1, 0x0);

	if (rc != 0)
		goto out;

	rc = atw_rf3000_write(sc, RF3000_OPTIONS2, RF3000_OPTIONS2_LNAGS_DELAY);

	if (rc != 0)
		goto out;

	/* CCA is acquisition sensitive */ 
	rc = atw_rf3000_write(sc, RF3000_CCACTL,
	    LSHIFT(RF3000_CCACTL_MODE_ACQ, RF3000_CCACTL_MODE_MASK));

	if (rc != 0)
		goto out;

out:
	atw_rfio_enable(sc, 0);
	ATW_WRITE(sc, ATW_NAR, sc->sc_opmode);
	return rc;
}

#ifdef ATW_DEBUG
static void
atw_rf3000_print(struct atw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	u_int addr, val;

	if (atw_debug < 3 || (ifp->if_flags & IFF_DEBUG) == 0)
		return;

	for (addr = 0x01; addr <= 0x15; addr++) {
		printf("%s: bbp[%d] = \n", sc->sc_dev.dv_xname, addr);
		if (atw_rf3000_read(sc, addr, &val) != 0) {
			printf("<unknown> (quitting print-out)\n");
			break;
		}
		printf("%08x\n", val);
	}
}
#endif /* ATW_DEBUG */

/* Set the power settings on the BBP for channel `chan'. */
static int
atw_rf3000_tune(struct atw_softc *sc, u_int8_t chan)
{
	int rc = 0;
	u_int32_t reg;
	u_int16_t txpower, lpf_cutoff, lna_gs_thresh;

	txpower = sc->sc_srom[ATW_SR_TXPOWER(chan)];
	lpf_cutoff = sc->sc_srom[ATW_SR_LPF_CUTOFF(chan)];
	lna_gs_thresh = sc->sc_srom[ATW_SR_LNA_GS_THRESH(chan)];

	/* odd channels: LSB, even channels: MSB */
	if (chan % 2 == 1) {
		txpower &= 0xFF;
		lpf_cutoff &= 0xFF;
		lna_gs_thresh &= 0xFF;
	} else {
		txpower >>= 8;
		lpf_cutoff >>= 8;
		lna_gs_thresh >>= 8;
	}

#ifdef ATW_DEBUG
	atw_rf3000_print(sc);
#endif /* ATW_DEBUG */

	DPRINTF(sc, ("%s: chan %d txpower %02x, lpf_cutoff %02x, "
	    "lna_gs_thresh %02x\n",
	    sc->sc_dev.dv_xname, chan, txpower, lpf_cutoff, lna_gs_thresh));

	atw_rfio_enable(sc, 1);

	if ((rc = atw_rf3000_write(sc, RF3000_GAINCTL,
	    LSHIFT(txpower, RF3000_GAINCTL_TXVGC_MASK))) != 0)
		goto out;

	if ((rc = atw_rf3000_write(sc, RF3000_LOGAINCAL, lpf_cutoff)) != 0)
		goto out;

	if ((rc = atw_rf3000_write(sc, RF3000_HIGAINCAL, lna_gs_thresh)) != 0)
		goto out;

	/* from a binary-only driver. */
	reg = ATW_READ(sc, ATW_PLCPHD);
	reg &= ~ATW_PLCPHD_SERVICE_MASK;
	reg |= LSHIFT(txpower << 2, ATW_PLCPHD_SERVICE_MASK);
	ATW_WRITE(sc, ATW_PLCPHD, reg);

#ifdef ATW_DEBUG
	atw_rf3000_print(sc);
#endif /* ATW_DEBUG */

out:
	atw_rfio_enable(sc, 0);

	return rc;
}

/* Write a register on the RF3000 baseband processor using the
 * registers provided by the ADM8211 for this purpose.
 *
 * Return 0 on success.
 */
static int
atw_rf3000_write(struct atw_softc *sc, u_int addr, u_int val)
{
	u_int32_t reg;
	int i;

	for (i = 1000; --i >= 0; ) {
		if (ATW_ISSET(sc, ATW_BBPCTL, ATW_BBPCTL_RD|ATW_BBPCTL_WR) == 0)
			break;
		DELAY(100);
	}

	if (i < 0) {
		printf("%s: BBPCTL busy (pre-write)\n", sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}

	reg = sc->sc_bbpctl_wr |
	     LSHIFT(val & 0xff, ATW_BBPCTL_DATA_MASK) |
	     LSHIFT(addr & 0x7f, ATW_BBPCTL_ADDR_MASK);

	ATW_WRITE(sc, ATW_BBPCTL, reg);

	for (i = 1000; --i >= 0; ) {
		DELAY(100);
		if (ATW_ISSET(sc, ATW_BBPCTL, ATW_BBPCTL_WR) == 0)
			break;
	}

	ATW_CLR(sc, ATW_BBPCTL, ATW_BBPCTL_WR);

	if (i < 0) {
		printf("%s: BBPCTL busy (post-write)\n", sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}
	return 0;
}

/* Read a register on the RF3000 baseband processor using the registers
 * the ADM8211 provides for this purpose.
 *
 * The 7-bit register address is addr.  Record the 8-bit data in the register
 * in *val.
 *
 * Return 0 on success.
 *
 * XXX This does not seem to work. The ADM8211 must require more or
 * different magic to read the chip than to write it. Possibly some
 * of the magic I have derived from a binary-only driver concerns
 * the "chip address" (see the RF3000 manual).
 */
#ifdef ATW_DEBUG
static int
atw_rf3000_read(struct atw_softc *sc, u_int addr, u_int *val)
{
	u_int32_t reg;
	int i;

	for (i = 1000; --i >= 0; ) {
		if (ATW_ISSET(sc, ATW_BBPCTL, ATW_BBPCTL_RD|ATW_BBPCTL_WR) == 0)
			break;
		DELAY(100);
	}

	if (i < 0) {
		printf("%s: start atw_rf3000_read, BBPCTL busy\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}

	reg = sc->sc_bbpctl_rd | LSHIFT(addr & 0x7f, ATW_BBPCTL_ADDR_MASK);

	ATW_WRITE(sc, ATW_BBPCTL, reg);

	for (i = 1000; --i >= 0; ) {
		DELAY(100);
		if (ATW_ISSET(sc, ATW_BBPCTL, ATW_BBPCTL_RD) == 0)
			break;
	}

	ATW_CLR(sc, ATW_BBPCTL, ATW_BBPCTL_RD);

	if (i < 0) {
		printf("%s: atw_rf3000_read wrote %08x; BBPCTL still busy\n",
		    sc->sc_dev.dv_xname, reg);
		return ETIMEDOUT;
	}
	if (val != NULL)
		*val = MASK_AND_RSHIFT(reg, ATW_BBPCTL_DATA_MASK);
	return 0;
}
#endif /* ATW_DEBUG */

/* Write a register on the Si4126 RF/IF synthesizer using the registers
 * provided by the ADM8211 for that purpose.
 *
 * val is 18 bits of data, and val is the 4-bit address of the register.
 *
 * Return 0 on success.
 */
static int
atw_si4126_write(struct atw_softc *sc, u_int addr, u_int val)
{
	u_int32_t bits, reg;
	int i;

	KASSERT((addr & ~PRESHIFT(SI4126_TWI_ADDR_MASK)) == 0);
	KASSERT((val & ~PRESHIFT(SI4126_TWI_DATA_MASK)) == 0);

	for (i = 1000; --i >= 0; ) {
		if (ATW_ISSET(sc, ATW_SYNCTL, ATW_SYNCTL_RD|ATW_SYNCTL_WR) == 0)
			break;
		DELAY(100);
	}

	if (i < 0) {
		printf("%s: start atw_si4126_write, SYNCTL busy\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}

	bits = LSHIFT(val, SI4126_TWI_DATA_MASK) |
	       LSHIFT(addr, SI4126_TWI_ADDR_MASK);

	reg = sc->sc_synctl_wr | LSHIFT(bits, ATW_SYNCTL_DATA_MASK);

	ATW_WRITE(sc, ATW_SYNCTL, reg);

	for (i = 1000; --i >= 0; ) {
		DELAY(100);
		if (ATW_ISSET(sc, ATW_SYNCTL, ATW_SYNCTL_WR) == 0)
			break;
	}

	/* restore to acceptable starting condition */
	ATW_CLR(sc, ATW_SYNCTL, ATW_SYNCTL_WR);

	if (i < 0) {
		printf("%s: atw_si4126_write wrote %08x, SYNCTL still busy\n",
		    sc->sc_dev.dv_xname, reg);
		return ETIMEDOUT;
	}
	return 0;
}

/* Read 18-bit data from the 4-bit address addr in Si4126
 * RF synthesizer and write the data to *val. Return 0 on success.
 *
 * XXX This does not seem to work. The ADM8211 must require more or
 * different magic to read the chip than to write it.
 */
#ifdef ATW_DEBUG
static int
atw_si4126_read(struct atw_softc *sc, u_int addr, u_int *val)
{
	u_int32_t reg;
	int i;

	KASSERT((addr & ~PRESHIFT(SI4126_TWI_ADDR_MASK)) == 0);

	for (i = 1000; --i >= 0; ) {
		if (ATW_ISSET(sc, ATW_SYNCTL, ATW_SYNCTL_RD|ATW_SYNCTL_WR) == 0)
			break;
		DELAY(100);
	}

	if (i < 0) {
		printf("%s: start atw_si4126_read, SYNCTL busy\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}

	reg = sc->sc_synctl_rd | LSHIFT(addr, ATW_SYNCTL_DATA_MASK);

	ATW_WRITE(sc, ATW_SYNCTL, reg);

	for (i = 1000; --i >= 0; ) {
		DELAY(100);
		if (ATW_ISSET(sc, ATW_SYNCTL, ATW_SYNCTL_RD) == 0)
			break;
	}

	ATW_CLR(sc, ATW_SYNCTL, ATW_SYNCTL_RD);

	if (i < 0) {
		printf("%s: atw_si4126_read wrote %08x, SYNCTL still busy\n",
		    sc->sc_dev.dv_xname, reg);
		return ETIMEDOUT;
	}
	if (val != NULL)
		*val = MASK_AND_RSHIFT(ATW_READ(sc, ATW_SYNCTL),
		                       ATW_SYNCTL_DATA_MASK);
	return 0;
}
#endif /* ATW_DEBUG */

/* XXX is the endianness correct? test. */
#define	atw_calchash(addr) \
	(ether_crc32_le((addr), IEEE80211_ADDR_LEN) & BITS(5, 0))

/*
 * atw_filter_setup:
 *
 *	Set the ADM8211's receive filter.
 */
static void
atw_filter_setup(struct atw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ethercom *ec = &ic->ic_ec;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int hash;
	u_int32_t hashes[2] = { 0, 0 };
	struct ether_multi *enm;
	struct ether_multistep step;

	DPRINTF(sc, ("%s: atw_filter_setup: sc_flags 0x%08x\n",
	    sc->sc_dev.dv_xname, sc->sc_flags));

	/*
	 * If we're running, idle the receive engine.  If we're NOT running,
	 * we're being called from atw_init(), and our writing ATW_NAR will
	 * start the transmit and receive processes in motion.
	 */
	if (ifp->if_flags & IFF_RUNNING)
		atw_idle(sc, ATW_NAR_SR);

	sc->sc_opmode &= ~(ATW_NAR_PR|ATW_NAR_MM);

	ifp->if_flags &= ~IFF_ALLMULTI;

	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_opmode |= ATW_NAR_PR;
allmulti:
		ifp->if_flags |= IFF_ALLMULTI;
		goto setit;
	}

	/*
	 * Program the 64-bit multicast hash filter.
	 */
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		/* XXX */
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    ETHER_ADDR_LEN) != 0)
			goto allmulti;

		hash = atw_calchash(enm->enm_addrlo);
		hashes[hash >> 5] |= 1 << (hash & 0x1f);
		ETHER_NEXT_MULTI(step, enm);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		hash = atw_calchash(etherbroadcastaddr);
		hashes[hash >> 5] |= 1 << (hash & 0x1f);
	}

	/* all bits set => hash is useless */
	if (~(hashes[0] & hashes[1]) == 0)
		goto allmulti;

 setit:
	if (ifp->if_flags & IFF_ALLMULTI)
		sc->sc_opmode |= ATW_NAR_MM;

	/* XXX in scan mode, do not filter packets. maybe this is
	 * unnecessary.
	 */
	if (ic->ic_state == IEEE80211_S_SCAN)
		sc->sc_opmode |= ATW_NAR_PR;

	ATW_WRITE(sc, ATW_MAR0, hashes[0]);
	ATW_WRITE(sc, ATW_MAR1, hashes[1]);
	ATW_WRITE(sc, ATW_NAR, sc->sc_opmode);
	DPRINTF(sc, ("%s: ATW_NAR %08x opmode %08x\n", sc->sc_dev.dv_xname,
	    ATW_READ(sc, ATW_NAR), sc->sc_opmode));

	DPRINTF(sc, ("%s: atw_filter_setup: returning\n", sc->sc_dev.dv_xname));
}

/* Tell the ADM8211 our preferred BSSID. The ADM8211 must match
 * a beacon's BSSID and SSID against the preferred BSSID and SSID
 * before it will raise ATW_INTR_LINKON. When the ADM8211 receives
 * no beacon with the preferred BSSID and SSID in the number of
 * beacon intervals given in ATW_BPLI, then it raises ATW_INTR_LINKOFF.
 */
static void
atw_write_bssid(struct atw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int8_t *bssid;

	bssid = ic->ic_bss->ni_bssid;

	ATW_WRITE(sc, ATW_ABDA1,
	    (ATW_READ(sc, ATW_ABDA1) &
	    ~(ATW_ABDA1_BSSIDB4_MASK|ATW_ABDA1_BSSIDB5_MASK)) |
	    LSHIFT(bssid[4], ATW_ABDA1_BSSIDB4_MASK) |
	    LSHIFT(bssid[5], ATW_ABDA1_BSSIDB5_MASK));

	ATW_WRITE(sc, ATW_BSSID0,
	    LSHIFT(bssid[0], ATW_BSSID0_BSSIDB0_MASK) |
	    LSHIFT(bssid[1], ATW_BSSID0_BSSIDB1_MASK) |
	    LSHIFT(bssid[2], ATW_BSSID0_BSSIDB2_MASK) |
	    LSHIFT(bssid[3], ATW_BSSID0_BSSIDB3_MASK));

	DPRINTF(sc, ("%s: BSSID %s -> ", sc->sc_dev.dv_xname,
	    ether_sprintf(sc->sc_bssid)));
	DPRINTF(sc, ("%s\n", ether_sprintf(bssid)));

	memcpy(sc->sc_bssid, bssid, sizeof(sc->sc_bssid));
}

/* Tell the ADM8211 how many beacon intervals must pass without
 * receiving a beacon with the preferred BSSID & SSID set by
 * atw_write_bssid and atw_write_ssid before ATW_INTR_LINKOFF
 * raised.
 */
static void
atw_write_bcn_thresh(struct atw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int lost_bcn_thresh;

	/* Lose link after one second or 7 beacons, whichever comes
	 * first, but do not lose link before 2 beacons are lost.
	 *
	 * In host AP mode, set the lost-beacon threshold to 0.
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		lost_bcn_thresh = 0;
	else {
		int beacons_per_second =
		    1000000 / (IEEE80211_DUR_TU * MAX(1,ic->ic_bss->ni_intval));
		lost_bcn_thresh = MAX(2, MIN(7, beacons_per_second));
	}

	/* XXX resets wake-up status bits */
	ATW_WRITE(sc, ATW_WCSR,
	    (ATW_READ(sc, ATW_WCSR) & ~ATW_WCSR_BLN_MASK) |
	    (LSHIFT(lost_bcn_thresh, ATW_WCSR_BLN_MASK) & ATW_WCSR_BLN_MASK));

	DPRINTF(sc, ("%s: lost-beacon threshold %d -> %d\n",
	    sc->sc_dev.dv_xname, sc->sc_lost_bcn_thresh, lost_bcn_thresh));

	sc->sc_lost_bcn_thresh = lost_bcn_thresh;

	DPRINTF(sc, ("%s: atw_write_bcn_thresh reg[WCSR] = %08x\n",
	    sc->sc_dev.dv_xname, ATW_READ(sc, ATW_WCSR)));
}

/* Write buflen bytes from buf to SRAM starting at the SRAM's ofs'th
 * 16-bit word.
 */
static void
atw_write_sram(struct atw_softc *sc, u_int ofs, u_int8_t *buf, u_int buflen)
{
	u_int i;
	u_int8_t *ptr;

	memcpy(&sc->sc_sram[ofs], buf, buflen);

	if (ofs % 2 != 0) {
		ofs--;
		buflen++;
	}

	if (buflen % 2 != 0)
		buflen++;

	assert(buflen + ofs <= ATW_SRAM_SIZE);

	ptr = &sc->sc_sram[ofs];

	for (i = 0; i < buflen; i += 2) {
		ATW_WRITE(sc, ATW_WEPCTL, ATW_WEPCTL_WR |
		    LSHIFT((ofs + i) / 2, ATW_WEPCTL_TBLADD_MASK));
		DELAY(atw_writewep_delay);

		ATW_WRITE(sc, ATW_WESK,
		    LSHIFT((ptr[i + 1] << 8) | ptr[i], ATW_WESK_DATA_MASK));
		DELAY(atw_writewep_delay);
	}
	ATW_WRITE(sc, ATW_WEPCTL, sc->sc_wepctl); /* restore WEP condition */

	if (sc->sc_if.if_flags & IFF_DEBUG) {
		int n_octets = 0;
		printf("%s: wrote %d bytes at 0x%x wepctl 0x%08x\n",
		    sc->sc_dev.dv_xname, buflen, ofs, sc->sc_wepctl);
		for (i = 0; i < buflen; i++) {
			printf(" %02x", ptr[i]);
			if (++n_octets % 24 == 0)
				printf("\n");
		}
		if (n_octets % 24 != 0)
			printf("\n");
	}
}

/* Write WEP keys from the ieee80211com to the ADM8211's SRAM. */
static void
atw_write_wep(struct atw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	/* SRAM shared-key record format: key0 flags key1 ... key12 */
	u_int8_t buf[IEEE80211_WEP_NKID]
	            [1 /* key[0] */ + 1 /* flags */ + 12 /* key[1 .. 12] */];
	u_int32_t reg;
	int i;

	sc->sc_wepctl = 0;
	ATW_WRITE(sc, ATW_WEPCTL, sc->sc_wepctl);

	if ((ic->ic_flags & IEEE80211_F_WEPON) == 0)
		return;

	memset(&buf[0][0], 0, sizeof(buf));

	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (ic->ic_nw_keys[i].wk_len > 5) {
			buf[i][1] = ATW_WEP_ENABLED | ATW_WEP_104BIT;
		} else if (ic->ic_nw_keys[i].wk_len != 0) {
			buf[i][1] = ATW_WEP_ENABLED;
		} else {
			buf[i][1] = 0;
			continue;
		}
		buf[i][0] = ic->ic_nw_keys[i].wk_key[0];
		memcpy(&buf[i][2], &ic->ic_nw_keys[i].wk_key[1],
		    ic->ic_nw_keys[i].wk_len - 1);
	}

	reg = ATW_READ(sc, ATW_MACTEST);
	reg |= ATW_MACTEST_MMI_USETXCLK | ATW_MACTEST_FORCE_KEYID;
	reg &= ~ATW_MACTEST_KEYID_MASK;
	reg |= LSHIFT(ic->ic_wep_txkey, ATW_MACTEST_KEYID_MASK);
	ATW_WRITE(sc, ATW_MACTEST, reg);

	/* RX bypass WEP if revision != 0x20. (I assume revision != 0x20
	 * throughout.)
	 */
	sc->sc_wepctl = ATW_WEPCTL_WEPENABLE | ATW_WEPCTL_WEPRXBYP;
	if (sc->sc_if.if_flags & IFF_LINK2)
		sc->sc_wepctl &= ~ATW_WEPCTL_WEPRXBYP;

	atw_write_sram(sc, ATW_SRAM_ADDR_SHARED_KEY, (u_int8_t*)&buf[0][0],
	    sizeof(buf));
}

const struct timeval atw_beacon_mininterval = {1, 0}; /* 1s */

static void
atw_recv_mgmt(struct ieee80211com *ic, struct mbuf *m,
    struct ieee80211_node *ni, int subtype, int rssi, u_int32_t rstamp)
{
	struct atw_softc *sc = (struct atw_softc*)ic->ic_softc;

	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
		/* do nothing: hardware answers probe request */
		break;
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_BEACON:
		atw_recv_beacon(ic, m, ni, subtype, rssi, rstamp);
		break;
	default:
		(*sc->sc_recv_mgmt)(ic, m, ni, subtype, rssi, rstamp);
		break;
	}
	return;
}

/* In ad hoc mode, atw_recv_beacon is responsible for the coalescence
 * of IBSSs with like SSID/channel but different BSSID. It joins the
 * oldest IBSS (i.e., with greatest TSF time), since that is the WECA
 * convention. Possibly the ADMtek chip does this for us; I will have
 * to test to find out.
 *
 * XXX we should add the duration field of the received beacon to
 * the TSF time it contains before comparing it with the ADM8211's
 * TSF.
 */
static void
atw_recv_beacon(struct ieee80211com *ic, struct mbuf *m0,
    struct ieee80211_node *ni, int subtype, int rssi, u_int32_t rstamp)
{
	struct atw_softc *sc;
	struct ieee80211_frame *wh;
	u_int64_t tsft, bcn_tsft;
	u_int32_t tsftl, tsfth;
	int do_print = 0;

	sc = (struct atw_softc*)ic->ic_if.if_softc;

	if (ic->ic_if.if_flags & IFF_DEBUG)
		do_print = (ic->ic_if.if_flags & IFF_LINK0)
		    ? 1 : ratecheck(&sc->sc_last_beacon, &atw_beacon_mininterval);

	wh = mtod(m0, struct ieee80211_frame *);

	(*sc->sc_recv_mgmt)(ic, m0, ni, subtype, rssi, rstamp);

	if (ic->ic_state != IEEE80211_S_RUN) {
		if (do_print)
			printf("%s: atw_recv_beacon: not running\n",
			    sc->sc_dev.dv_xname);
		return;
	}

	if ((ni = ieee80211_lookup_node(ic, wh->i_addr2,
	    ic->ic_bss->ni_chan)) == NULL) {
		if (do_print)
			printf("%s: atw_recv_beacon: no node %s\n",
			    sc->sc_dev.dv_xname, ether_sprintf(wh->i_addr2));
		return;
	}

	if (ieee80211_match_bss(ic, ni) != 0) {
		if (do_print)
			printf("%s: atw_recv_beacon: ssid mismatch %s\n",
			    sc->sc_dev.dv_xname, ether_sprintf(wh->i_addr2));
		return;
	}

	if (memcmp(ni->ni_bssid, ic->ic_bss->ni_bssid, IEEE80211_ADDR_LEN) == 0)
		return;

	if (do_print)
		printf("%s: atw_recv_beacon: bssid mismatch %s\n",
		    sc->sc_dev.dv_xname, ether_sprintf(ni->ni_bssid));

	if (ic->ic_opmode != IEEE80211_M_IBSS)
		return;

	/* If we read TSFTL right before rollover, we read a TSF timer
	 * that is too high rather than too low. This prevents a spurious
	 * synchronization down the line, however, our IBSS could suffer
	 * from a creeping TSF....
	 */
	tsftl = ATW_READ(sc, ATW_TSFTL);
	tsfth = ATW_READ(sc, ATW_TSFTH);

	tsft = (u_int64_t)tsfth << 32 | tsftl;
	bcn_tsft = le64toh(*(u_int64_t*)ni->ni_tstamp);

	if (do_print)
		printf("%s: my tsft %" PRIu64 " beacon tsft %" PRIu64 "\n",
		    sc->sc_dev.dv_xname, tsft, bcn_tsft);

	/* we are faster, let the other guy catch up */
	if (bcn_tsft < tsft)
		return;

	if (do_print)
		printf("%s: sync TSF with %s\n", sc->sc_dev.dv_xname,
		    ether_sprintf(wh->i_addr2));

	ic->ic_flags &= ~IEEE80211_F_SIBSS;

#if 0
	atw_tsf(sc);
#endif

	/* negotiate rates with new IBSS */
	ieee80211_fix_rate(ic, ni, IEEE80211_F_DOFRATE |
	    IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
	if (ni->ni_rates.rs_nrates == 0) {
		printf("%s: rates mismatch, BSSID %s\n", sc->sc_dev.dv_xname,
			ether_sprintf(ni->ni_bssid));
		return;
	}

	if (do_print) {
		printf("%s: sync BSSID %s -> ", sc->sc_dev.dv_xname,
		    ether_sprintf(ic->ic_bss->ni_bssid));
		printf("%s ", ether_sprintf(ni->ni_bssid));
		printf("(from %s)\n", ether_sprintf(wh->i_addr2));
	}

	(*ic->ic_node_copy)(ic, ic->ic_bss, ni);

	atw_write_bssid(sc);
	atw_write_bcn_thresh(sc);
	atw_start_beacon(sc, 1);
}

/* Write the SSID in the ieee80211com to the SRAM on the ADM8211.
 * In ad hoc mode, the SSID is written to the beacons sent by the
 * ADM8211. In both ad hoc and infrastructure mode, beacons received
 * with matching SSID affect ATW_INTR_LINKON/ATW_INTR_LINKOFF
 * indications.
 */
static void
atw_write_ssid(struct atw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	/* 34 bytes are reserved in ADM8211 SRAM for the SSID */
	u_int8_t buf[roundup(1 /* length */ + IEEE80211_NWID_LEN, 2)];

	memset(buf, 0, sizeof(buf));
	buf[0] = ic->ic_bss->ni_esslen;
	memcpy(&buf[1], ic->ic_bss->ni_essid, ic->ic_bss->ni_esslen);

	atw_write_sram(sc, ATW_SRAM_ADDR_SSID, buf, sizeof(buf));
}

/* Write the supported rates in the ieee80211com to the SRAM of the ADM8211.
 * In ad hoc mode, the supported rates are written to beacons sent by the
 * ADM8211.
 */
static void
atw_write_sup_rates(struct atw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	/* 14 bytes are probably (XXX) reserved in the ADM8211 SRAM for
	 * supported rates
	 */
	u_int8_t buf[roundup(1 /* length */ + IEEE80211_RATE_SIZE, 2)];

	memset(buf, 0, sizeof(buf));

	buf[0] = ic->ic_bss->ni_rates.rs_nrates;

	memcpy(&buf[1], ic->ic_bss->ni_rates.rs_rates,
	    ic->ic_bss->ni_rates.rs_nrates);

	atw_write_sram(sc, ATW_SRAM_ADDR_SUPRATES, buf, sizeof(buf));
}

/* Start/stop sending beacons. */
void
atw_start_beacon(struct atw_softc *sc, int start)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int32_t len, capinfo, reg_bcnt, reg_cap1;

	if (ATW_IS_ENABLED(sc) == 0)
		return;

	len = capinfo = 0;

	/* start beacons */
	len = sizeof(struct ieee80211_frame) +
	    8 /* timestamp */ + 2 /* beacon interval */ +
	    2 /* capability info */ +
	    2 + ic->ic_bss->ni_esslen /* SSID element */ +
	    2 + ic->ic_bss->ni_rates.rs_nrates /* rates element */ +
	    3 /* DS parameters */ +
	    IEEE80211_CRC_LEN;

	reg_bcnt = ATW_READ(sc, ATW_BCNT) & ~ATW_BCNT_BCNT_MASK;

	reg_cap1 = ATW_READ(sc, ATW_CAP1) & ~ATW_CAP1_CAPI_MASK;

	ATW_WRITE(sc, ATW_BCNT, reg_bcnt);
	ATW_WRITE(sc, ATW_CAP1, reg_cap1);

	if (!start)
		return;

	/* TBD use ni_capinfo */

	if (sc->sc_flags & ATWF_SHORT_PREAMBLE)
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;

	switch (ic->ic_opmode) {
	case IEEE80211_M_IBSS:
		len += 4; /* IBSS parameters */
		capinfo |= IEEE80211_CAPINFO_IBSS;
		break;
	case IEEE80211_M_HOSTAP:
		/* XXX 6-byte minimum TIM */
		len += atw_beacon_len_adjust;
		capinfo |= IEEE80211_CAPINFO_ESS;
		break;
	default:
		return;
	}

	reg_bcnt |= LSHIFT(len, ATW_BCNT_BCNT_MASK);
	reg_cap1 |= LSHIFT(capinfo, ATW_CAP1_CAPI_MASK);

	ATW_WRITE(sc, ATW_BCNT, reg_bcnt);
	ATW_WRITE(sc, ATW_CAP1, reg_cap1);

	DPRINTF(sc, ("%s: atw_start_beacon reg[ATW_BCNT] = %08x\n",
	    sc->sc_dev.dv_xname, reg_bcnt));

	DPRINTF(sc, ("%s: atw_start_beacon reg[ATW_CAP1] = %08x\n",
	    sc->sc_dev.dv_xname, reg_cap1));
}

/* First beacon was sent at time 0 microseconds, current time is
 * tsfth << 32 | tsftl microseconds, and beacon interval is tbtt
 * microseconds.  Return the expected time in microseconds for the
 * beacon after next.
 */
static __inline u_int64_t
atw_predict_beacon(u_int64_t tsft, u_int32_t tbtt)
{
	return tsft + (tbtt - tsft % tbtt);
}

/* If we've created an IBSS, write the TSF time in the ADM8211 to
 * the ieee80211com.
 *
 * Predict the next target beacon transmission time (TBTT) and
 * write it to the ADM8211.
 */
static void
atw_tsf(struct atw_softc *sc)
{
#define TBTTOFS 20 /* TU */

	struct ieee80211com *ic = &sc->sc_ic;
	u_int64_t tsft, tbtt;

	if ((ic->ic_opmode == IEEE80211_M_HOSTAP) ||
	    ((ic->ic_opmode == IEEE80211_M_IBSS) &&
	     (ic->ic_flags & IEEE80211_F_SIBSS))) {
		tsft = ATW_READ(sc, ATW_TSFTH);
		tsft <<= 32;
		tsft |= ATW_READ(sc, ATW_TSFTL);
		*(u_int64_t*)&ic->ic_bss->ni_tstamp[0] = htole64(tsft);
	} else
		tsft = le64toh(*(u_int64_t*)&ic->ic_bss->ni_tstamp[0]);

	tbtt = atw_predict_beacon(tsft,
	    ic->ic_bss->ni_intval * IEEE80211_DUR_TU);

	/* skip one more beacon so that the TBTT cannot pass before
	 * we've programmed it, and also so that we can subtract a
	 * few TU so that we wake a little before TBTT. 
	 */
	tbtt += ic->ic_bss->ni_intval * IEEE80211_DUR_TU;

	/* wake up a little early */
	tbtt -= TBTTOFS * IEEE80211_DUR_TU;

	DPRINTF(sc, ("%s: tsft %" PRIu64 " tbtt %" PRIu64 "\n",
	    sc->sc_dev.dv_xname, tsft, tbtt));

	ATW_WRITE(sc, ATW_TOFS1,
	    LSHIFT(1, ATW_TOFS1_TSFTOFSR_MASK) |
	    LSHIFT(TBTTOFS, ATW_TOFS1_TBTTOFS_MASK) |
	    LSHIFT(
		MASK_AND_RSHIFT((u_int32_t)tbtt, BITS(25, 10)),
		ATW_TOFS1_TBTTPRE_MASK));
#undef TBTTOFS
}

static void
atw_next_scan(void *arg)
{
	struct atw_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int s;

	/* don't call atw_start w/o network interrupts blocked */
	s = splnet();
	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ifp);
	splx(s);
}

/* Synchronize the hardware state with the software state. */
static int
atw_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = &ic->ic_if;
	struct atw_softc *sc = ifp->if_softc;
	enum ieee80211_state ostate;
	int error;

	ostate = ic->ic_state;

	if (nstate == IEEE80211_S_INIT) {
		callout_stop(&sc->sc_scan_ch);
		sc->sc_cur_chan = IEEE80211_CHAN_ANY;
		atw_start_beacon(sc, 0);
		return (*sc->sc_newstate)(ic, nstate, arg);
	}

	if ((error = atw_tune(sc)) != 0)
		return error;

	switch (nstate) {
	case IEEE80211_S_ASSOC:
		break;
	case IEEE80211_S_INIT:
		panic("%s: unexpected state IEEE80211_S_INIT\n", __func__);
		break;
	case IEEE80211_S_SCAN:
		memset(sc->sc_bssid, 0, IEEE80211_ADDR_LEN);
		atw_write_bssid(sc);

		callout_reset(&sc->sc_scan_ch, atw_dwelltime * hz / 1000,
		    atw_next_scan, sc);

		break;
	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_STA)
			break;
		/*FALLTHROUGH*/
	case IEEE80211_S_AUTH:
		atw_write_bssid(sc);
		atw_write_bcn_thresh(sc);
		atw_write_ssid(sc);
		atw_write_sup_rates(sc);

		if (ic->ic_opmode == IEEE80211_M_AHDEMO ||
		    ic->ic_opmode == IEEE80211_M_MONITOR)
			break;

		/* set listen interval
		 * XXX do software units agree w/ hardware?
		 */
		ATW_WRITE(sc, ATW_BPLI,
		    LSHIFT(ic->ic_bss->ni_intval, ATW_BPLI_BP_MASK) |
		    LSHIFT(ic->ic_lintval / ic->ic_bss->ni_intval,
			   ATW_BPLI_LI_MASK));

		DPRINTF(sc, ("%s: reg[ATW_BPLI] = %08x\n",
		    sc->sc_dev.dv_xname, ATW_READ(sc, ATW_BPLI)));

		atw_tsf(sc);
		break;
	}

	if (nstate != IEEE80211_S_SCAN)
		callout_stop(&sc->sc_scan_ch);

	if (nstate == IEEE80211_S_RUN &&
	    (ic->ic_opmode == IEEE80211_M_HOSTAP ||
	     ic->ic_opmode == IEEE80211_M_IBSS))
		atw_start_beacon(sc, 1);
	else
		atw_start_beacon(sc, 0);

	return (*sc->sc_newstate)(ic, nstate, arg);
}

/*
 * atw_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
int
atw_add_rxbuf(struct atw_softc *sc, int idx)
{
	struct atw_rxsoft *rxs = &sc->sc_rxsoft[idx];
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

	error = bus_dmamap_load(sc->sc_dmat, rxs->rxs_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't load rx DMA map %d, error = %d\n",
		    sc->sc_dev.dv_xname, idx, error);
		panic("atw_add_rxbuf");	/* XXX */
	}

	bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
	    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	ATW_INIT_RXDESC(sc, idx);

	return (0);
}

/*
 * Release any queued transmit buffers.
 */
void
atw_txdrain(struct atw_softc *sc)
{
	struct atw_txsoft *txs;

	while ((txs = SIMPLEQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);
		if (txs->txs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}
	sc->sc_tx_timer = 0;
}

/*
 * atw_stop:		[ ifnet interface function ]
 *
 *	Stop transmission on the interface.
 */
void
atw_stop(struct ifnet *ifp, int disable)
{
	struct atw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	/* Disable interrupts. */
	ATW_WRITE(sc, ATW_IER, 0);

	/* Stop the transmit and receive processes. */
	sc->sc_opmode = 0;
	ATW_WRITE(sc, ATW_NAR, 0);
	ATW_WRITE(sc, ATW_TDBD, 0);
	ATW_WRITE(sc, ATW_TDBP, 0);
	ATW_WRITE(sc, ATW_RDB, 0);

	atw_txdrain(sc);

	if (disable) {
		atw_rxdrain(sc);
		atw_disable(sc);
	}

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	if (!disable)
		atw_reset(sc);
}

/*
 * atw_rxdrain:
 *
 *	Drain the receive queue.
 */
void
atw_rxdrain(struct atw_softc *sc)
{
	struct atw_rxsoft *rxs;
	int i;

	for (i = 0; i < ATW_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL)
			continue;
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
		m_freem(rxs->rxs_mbuf);
		rxs->rxs_mbuf = NULL;
	}
}

/*
 * atw_detach:
 *
 *	Detach an ADM8211 interface.
 */
int
atw_detach(struct atw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	struct atw_rxsoft *rxs;
	struct atw_txsoft *txs;
	int i;

	/*
	 * Succeed now if there isn't any work to do.
	 */
	if ((sc->sc_flags & ATWF_ATTACHED) == 0)
		return (0);

	ieee80211_ifdetach(ifp);
	if_detach(ifp);

	for (i = 0; i < ATW_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, rxs->rxs_dmamap);
	}
	for (i = 0; i < ATW_TXQUEUELEN; i++) {
		txs = &sc->sc_txsoft[i];
		if (txs->txs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, txs->txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_control_data,
	    sizeof(struct atw_control_data));
	bus_dmamem_free(sc->sc_dmat, &sc->sc_cdseg, sc->sc_cdnseg);

	shutdownhook_disestablish(sc->sc_sdhook);
	powerhook_disestablish(sc->sc_powerhook);

	if (sc->sc_srom)
		free(sc->sc_srom, M_DEVBUF);

	return (0);
}

/* atw_shutdown: make sure the interface is stopped at reboot time. */
void
atw_shutdown(void *arg)
{
	struct atw_softc *sc = arg;

	atw_stop(&sc->sc_ic.ic_if, 1);
}

int
atw_intr(void *arg)
{
	struct atw_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	u_int32_t status, rxstatus, txstatus, linkstatus;
	int handled = 0, txthresh;

#ifdef DEBUG
	if (ATW_IS_ENABLED(sc) == 0)
		panic("%s: atw_intr: not enabled", sc->sc_dev.dv_xname);
#endif

	/*
	 * If the interface isn't running, the interrupt couldn't
	 * possibly have come from us.
	 */
	if ((ifp->if_flags & IFF_RUNNING) == 0 ||
	    (sc->sc_dev.dv_flags & DVF_ACTIVE) == 0)
		return (0);

	for (;;) {
		status = ATW_READ(sc, ATW_STSR);

		if (status)
			ATW_WRITE(sc, ATW_STSR, status);

		if (sc->sc_intr_ack != NULL)
			(*sc->sc_intr_ack)(sc);

#ifdef ATW_DEBUG
#define PRINTINTR(flag) do { \
	if ((status & flag) != 0) { \
		printf("%s" #flag, delim); \
		delim = ","; \
	} \
} while (0)

		if (atw_debug > 1 && status) {
			const char *delim = "<";

			printf("%s: reg[STSR] = %x",
			    sc->sc_dev.dv_xname, status);

			PRINTINTR(ATW_INTR_FBE);
			PRINTINTR(ATW_INTR_LINKOFF);
			PRINTINTR(ATW_INTR_LINKON);
			PRINTINTR(ATW_INTR_RCI);
			PRINTINTR(ATW_INTR_RDU);
			PRINTINTR(ATW_INTR_REIS);
			PRINTINTR(ATW_INTR_RPS);
			PRINTINTR(ATW_INTR_TCI);
			PRINTINTR(ATW_INTR_TDU);
			PRINTINTR(ATW_INTR_TLT);
			PRINTINTR(ATW_INTR_TPS);
			PRINTINTR(ATW_INTR_TRT);
			PRINTINTR(ATW_INTR_TUF);
			PRINTINTR(ATW_INTR_BCNTC);
			PRINTINTR(ATW_INTR_ATIME);
			PRINTINTR(ATW_INTR_TBTT);
			PRINTINTR(ATW_INTR_TSCZ);
			PRINTINTR(ATW_INTR_TSFTF);
			printf(">\n");
		}
#undef PRINTINTR
#endif /* ATW_DEBUG */

		if ((status & sc->sc_inten) == 0)
			break;

		handled = 1;

		rxstatus = status & sc->sc_rxint_mask;
		txstatus = status & sc->sc_txint_mask;
		linkstatus = status & sc->sc_linkint_mask;

		if (linkstatus) {
			atw_linkintr(sc, linkstatus);
		}

		if (rxstatus) {
			/* Grab any new packets. */
			atw_rxintr(sc);

			if (rxstatus & ATW_INTR_RDU) {
				printf("%s: receive ring overrun\n",
				    sc->sc_dev.dv_xname);
				/* Get the receive process going again. */
				ATW_WRITE(sc, ATW_RDR, 0x1);
				break;
			}
		}

		if (txstatus) {
			/* Sweep up transmit descriptors. */
			atw_txintr(sc);

			if (txstatus & ATW_INTR_TLT)
				DPRINTF(sc, ("%s: tx lifetime exceeded\n",
				    sc->sc_dev.dv_xname));

			if (txstatus & ATW_INTR_TRT)
				DPRINTF(sc, ("%s: tx retry limit exceeded\n",
				    sc->sc_dev.dv_xname));

			/* If Tx under-run, increase our transmit threshold
			 * if another is available.
			 */
			txthresh = sc->sc_txthresh + 1;
			if ((txstatus & ATW_INTR_TUF) &&
			    sc->sc_txth[txthresh].txth_name != NULL) {
				/* Idle the transmit process. */
				atw_idle(sc, ATW_NAR_ST);

				sc->sc_txthresh = txthresh;
				sc->sc_opmode &= ~(ATW_NAR_TR_MASK|ATW_NAR_SF);
				sc->sc_opmode |=
				    sc->sc_txth[txthresh].txth_opmode;
				printf("%s: transmit underrun; new "
				    "threshold: %s\n", sc->sc_dev.dv_xname,
				    sc->sc_txth[txthresh].txth_name);

				/* Set the new threshold and restart
				 * the transmit process.
				 */
				ATW_WRITE(sc, ATW_NAR, sc->sc_opmode);
				/* XXX Log every Nth underrun from
				 * XXX now on?
				 */
			}
		}

		if (status & (ATW_INTR_TPS|ATW_INTR_RPS)) {
			if (status & ATW_INTR_TPS)
				printf("%s: transmit process stopped\n",
				    sc->sc_dev.dv_xname);
			if (status & ATW_INTR_RPS)
				printf("%s: receive process stopped\n",
				    sc->sc_dev.dv_xname);
			(void)atw_init(ifp);
			break;
		}

		if (status & ATW_INTR_FBE) {
			printf("%s: fatal bus error\n", sc->sc_dev.dv_xname);
			(void)atw_init(ifp);
			break;
		}

		/*
		 * Not handled:
		 *
		 *	Transmit buffer unavailable -- normal
		 *	condition, nothing to do, really.
		 *
		 *	Early receive interrupt -- not available on
		 *	all chips, we just use RI.  We also only
		 *	use single-segment receive DMA, so this
		 *	is mostly useless.
		 *
		 *      TBD others
		 */
	}

	/* Try to get more packets going. */
	atw_start(ifp);

	return (handled);
}

/*
 * atw_idle:
 *
 *	Cause the transmit and/or receive processes to go idle.
 *
 *      XXX It seems that the ADM8211 will not signal the end of the Rx/Tx
 *	process in STSR if I clear SR or ST after the process has already
 *	ceased. Fair enough. But the Rx process status bits in ATW_TEST0
 *      do not seem to be too reliable. Perhaps I have the sense of the
 *	Rx bits switched with the Tx bits?
 */
void
atw_idle(struct atw_softc *sc, u_int32_t bits)
{
	u_int32_t ackmask = 0, opmode, stsr, test0;
	int i, s;

	/* without this, somehow we run concurrently w/ interrupt handler */
	s = splnet(); 

	opmode = sc->sc_opmode & ~bits;

	if (bits & ATW_NAR_SR)
		ackmask |= ATW_INTR_RPS;

	if (bits & ATW_NAR_ST) {
		ackmask |= ATW_INTR_TPS;
		/* set ATW_NAR_HF to flush TX FIFO. */
		opmode |= ATW_NAR_HF;
	}

	ATW_WRITE(sc, ATW_NAR, opmode);

	for (i = 0; i < 1000; i++) {
		stsr = ATW_READ(sc, ATW_STSR);
		if ((stsr & ackmask) == ackmask)
			break;
		DELAY(10);
	}

	ATW_WRITE(sc, ATW_STSR, stsr & ackmask);

	if ((stsr & ackmask) == ackmask)
		goto out;

	test0 = ATW_READ(sc, ATW_TEST0);

	if ((bits & ATW_NAR_ST) != 0 && (stsr & ATW_INTR_TPS) == 0 &&
	    (test0 & ATW_TEST0_TS_MASK) != ATW_TEST0_TS_STOPPED) {
		printf("%s: transmit process not idle [%s]\n",
		    sc->sc_dev.dv_xname,
		    atw_tx_state[MASK_AND_RSHIFT(test0, ATW_TEST0_TS_MASK)]);
		printf("%s: bits %08x test0 %08x stsr %08x\n",
		    sc->sc_dev.dv_xname, bits, test0, stsr);
	}

	if ((bits & ATW_NAR_SR) != 0 && (stsr & ATW_INTR_RPS) == 0 &&
	    (test0 & ATW_TEST0_RS_MASK) != ATW_TEST0_RS_STOPPED) {
		DPRINTF2(sc, ("%s: receive process not idle [%s]\n",
		    sc->sc_dev.dv_xname,
		    atw_rx_state[MASK_AND_RSHIFT(test0, ATW_TEST0_RS_MASK)]));
		DPRINTF2(sc, ("%s: bits %08x test0 %08x stsr %08x\n",
		    sc->sc_dev.dv_xname, bits, test0, stsr));
	}
out:
	if ((bits & ATW_NAR_ST) != 0)
		atw_txdrain(sc);
	splx(s);
	return;
}

/*
 * atw_linkintr:
 *
 *	Helper; handle link-status interrupts.
 */
void
atw_linkintr(struct atw_softc *sc, u_int32_t linkstatus)
{
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_state != IEEE80211_S_RUN)
		return;

	if (linkstatus & ATW_INTR_LINKON) {
		DPRINTF(sc, ("%s: link on\n", sc->sc_dev.dv_xname));
		sc->sc_rescan_timer = 0;
	} else if (linkstatus & ATW_INTR_LINKOFF) {
		DPRINTF(sc, ("%s: link off\n", sc->sc_dev.dv_xname));
		switch (ic->ic_opmode) {
		case IEEE80211_M_HOSTAP:
			return;
		case IEEE80211_M_IBSS:
			if (ic->ic_flags & IEEE80211_F_SIBSS)
				return;
			/*FALLTHROUGH*/
		case IEEE80211_M_STA:
			sc->sc_rescan_timer = 3;
			ic->ic_if.if_timer = 1;
			break;
		default:
			break;
		}
	}
}

/*
 * atw_rxintr:
 *
 *	Helper; handle receive interrupts.
 */
void
atw_rxintr(struct atw_softc *sc)
{
	static int rate_tbl[] = {2, 4, 11, 22, 44};
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ieee80211_frame *wh;
	struct ifnet *ifp = &ic->ic_if;
	struct atw_rxsoft *rxs;
	struct mbuf *m;
	u_int32_t rxstat;
	int i, len, rate, rate0;
	u_int32_t rssi;

	for (i = sc->sc_rxptr;; i = ATW_NEXTRX(i)) {
		rxs = &sc->sc_rxsoft[i];

		ATW_CDRXSYNC(sc, i, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		rxstat = le32toh(sc->sc_rxdescs[i].ar_stat);
		rssi = le32toh(sc->sc_rxdescs[i].ar_rssi);
		rate0 = MASK_AND_RSHIFT(rxstat, ATW_RXSTAT_RXDR_MASK);

		if (rxstat & ATW_RXSTAT_OWN)
			break; /* We have processed all receive buffers. */

		DPRINTF3(sc,
		    ("%s: rx stat %08x rssi %08x buf1 %08x buf2 %08x\n",
		    sc->sc_dev.dv_xname,
		    sc->sc_rxdescs[i].ar_stat,
		    sc->sc_rxdescs[i].ar_rssi,
		    sc->sc_rxdescs[i].ar_buf1,
		    sc->sc_rxdescs[i].ar_buf2));

		/*
		 * Make sure the packet fit in one buffer.  This should
		 * always be the case.
		 */
		if ((rxstat & (ATW_RXSTAT_FS|ATW_RXSTAT_LS)) !=
		    (ATW_RXSTAT_FS|ATW_RXSTAT_LS)) {
			printf("%s: incoming packet spilled, resetting\n",
			    sc->sc_dev.dv_xname);
			(void)atw_init(ifp);
			return;
		}

		/*
		 * If an error occurred, update stats, clear the status
		 * word, and leave the packet buffer in place.  It will
		 * simply be reused the next time the ring comes around.
	 	 * If 802.1Q VLAN MTU is enabled, ignore the Frame Too Long
		 * error.
		 */

		if ((rxstat & ATW_RXSTAT_ES) != 0 &&
		    ((sc->sc_ic.ic_ec.ec_capenable & ETHERCAP_VLAN_MTU) == 0 ||
		     (rxstat & (ATW_RXSTAT_DE | ATW_RXSTAT_SFDE |
		                ATW_RXSTAT_SIGE | ATW_RXSTAT_CRC16E |
				ATW_RXSTAT_RXTOE | ATW_RXSTAT_CRC32E |
				ATW_RXSTAT_ICVE)) != 0)) {
#define	PRINTERR(bit, str)						\
			if (rxstat & (bit))				\
				printf("%s: receive error: %s\n",	\
				    sc->sc_dev.dv_xname, str)
			ifp->if_ierrors++;
			PRINTERR(ATW_RXSTAT_DE, "descriptor error");
			PRINTERR(ATW_RXSTAT_SFDE, "PLCP SFD error");
			PRINTERR(ATW_RXSTAT_SIGE, "PLCP signal error");
			PRINTERR(ATW_RXSTAT_CRC16E, "PLCP CRC16 error");
			PRINTERR(ATW_RXSTAT_RXTOE, "time-out");
			PRINTERR(ATW_RXSTAT_CRC32E, "FCS error");
			PRINTERR(ATW_RXSTAT_ICVE, "WEP ICV error");
#undef PRINTERR
			ATW_INIT_RXDESC(sc, i);
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		/*
		 * No errors; receive the packet.  Note the ADM8211
		 * includes the CRC in promiscuous mode.
		 */
		len = MASK_AND_RSHIFT(rxstat, ATW_RXSTAT_FL_MASK);

		/*
		 * Allocate a new mbuf cluster.  If that fails, we are
		 * out of memory, and must drop the packet and recycle
		 * the buffer that's already attached to this descriptor.
		 */
		m = rxs->rxs_mbuf;
		if (atw_add_rxbuf(sc, i) != 0) {
			ifp->if_ierrors++;
			ATW_INIT_RXDESC(sc, i);
			bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
			continue;
		}

		ifp->if_ipackets++;
		if (sc->sc_opmode & ATW_NAR_PR)
			m->m_flags |= M_HASFCS;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		if (rate0 >= sizeof(rate_tbl) / sizeof(rate_tbl[0]))
			rate = 0;
		else
			rate = rate_tbl[rate0];

 #if NBPFILTER > 0
		/* Pass this up to any BPF listeners. */
		if (sc->sc_radiobpf != NULL) {
			struct mbuf mb;

			struct atw_rx_radiotap_header *tap = &sc->sc_rxtap;

			tap->ar_rate = rate;
			tap->ar_chan_freq = ic->ic_bss->ni_chan->ic_freq;
			tap->ar_chan_flags = ic->ic_bss->ni_chan->ic_flags;

			/* TBD verify units are dB */
			tap->ar_antsignal = (int)rssi;
			/* TBD tap->ar_flags */

			M_COPY_PKTHDR(&mb, m);
			mb.m_data = (caddr_t)tap;
			mb.m_len = tap->ar_ihdr.it_len;
			mb.m_next = m;
			mb.m_pkthdr.len += mb.m_len;
			bpf_mtap(sc->sc_radiobpf, &mb);
 		}
 #endif /* NPBFILTER > 0 */

		wh = mtod(m, struct ieee80211_frame *);
		ni = ieee80211_find_rxnode(ic, wh);
		if (m->m_pkthdr.len >= sizeof(struct ieee80211_frame_min) ||
		    ic->ic_opmode == IEEE80211_M_MONITOR)
			ieee80211_input(ifp, m, ni, (int)rssi, 0);
		/*
		 * The frame may have caused the node to be marked for
		 * reclamation (e.g. in response to a DEAUTH message)
		 * so use free_node here instead of unref_node.
		 */
		if (ni == ic->ic_bss)
			ieee80211_unref_node(&ni);
		else
			ieee80211_free_node(ic, ni);
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;
}

/*
 * atw_txintr:
 *
 *	Helper; handle transmit interrupts.
 */
void
atw_txintr(struct atw_softc *sc)
{
#define TXSTAT_ERRMASK (ATW_TXSTAT_TUF | ATW_TXSTAT_TLT | ATW_TXSTAT_TRT | \
    ATW_TXSTAT_TRO | ATW_TXSTAT_SOFBR)
#define TXSTAT_FMT "\20\31ATW_TXSTAT_SOFBR\32ATW_TXSTAT_TRO\33ATW_TXSTAT_TUF" \
    "\34ATW_TXSTAT_TRT\35ATW_TXSTAT_TLT"

	static char txstat_buf[sizeof("ffffffff<>" TXSTAT_FMT)];
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	struct atw_txsoft *txs;
	u_int32_t txstat;

	DPRINTF3(sc, ("%s: atw_txintr: sc_flags 0x%08x\n",
	    sc->sc_dev.dv_xname, sc->sc_flags));

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	while ((txs = SIMPLEQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		ATW_CDTXSYNC(sc, txs->txs_lastdesc,
		    txs->txs_ndescs,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

#ifdef ATW_DEBUG
		if ((ifp->if_flags & IFF_DEBUG) != 0 && atw_debug > 2) {
			int i;
			printf("    txsoft %p transmit chain:\n", txs);
			for (i = txs->txs_firstdesc;; i = ATW_NEXTTX(i)) {
				printf("     descriptor %d:\n", i);
				printf("       at_status:   0x%08x\n",
				    le32toh(sc->sc_txdescs[i].at_stat));
				printf("       at_flags:      0x%08x\n",
				    le32toh(sc->sc_txdescs[i].at_flags));
				printf("       at_buf1: 0x%08x\n",
				    le32toh(sc->sc_txdescs[i].at_buf1));
				printf("       at_buf2: 0x%08x\n",
				    le32toh(sc->sc_txdescs[i].at_buf2));
				if (i == txs->txs_lastdesc)
					break;
			}
		}
#endif

		txstat = le32toh(sc->sc_txdescs[txs->txs_lastdesc].at_stat);
		if (txstat & ATW_TXSTAT_OWN)
			break;

		SIMPLEQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);

		sc->sc_txfree += txs->txs_ndescs;

		bus_dmamap_sync(sc->sc_dmat, txs->txs_dmamap,
		    0, txs->txs_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
		m_freem(txs->txs_mbuf);
		txs->txs_mbuf = NULL;

		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);

		if ((ifp->if_flags & IFF_DEBUG) != 0 &&
		    (txstat & TXSTAT_ERRMASK) != 0) {
			bitmask_snprintf(txstat & TXSTAT_ERRMASK, TXSTAT_FMT,
			    txstat_buf, sizeof(txstat_buf));
			printf("%s: txstat %s %d\n", sc->sc_dev.dv_xname,
			    txstat_buf,
			    MASK_AND_RSHIFT(txstat, ATW_TXSTAT_ARC_MASK));
		}

		/*
		 * Check for errors and collisions.
		 */
		if (txstat & ATW_TXSTAT_TUF)
			sc->sc_stats.ts_tx_tuf++;
		if (txstat & ATW_TXSTAT_TLT)
			sc->sc_stats.ts_tx_tlt++;
		if (txstat & ATW_TXSTAT_TRT)
			sc->sc_stats.ts_tx_trt++;
		if (txstat & ATW_TXSTAT_TRO)
			sc->sc_stats.ts_tx_tro++;
		if (txstat & ATW_TXSTAT_SOFBR) {
			sc->sc_stats.ts_tx_sofbr++;
		}

		if ((txstat & ATW_TXSTAT_ES) == 0)
			ifp->if_collisions +=
			    MASK_AND_RSHIFT(txstat, ATW_TXSTAT_ARC_MASK);
		else
			ifp->if_oerrors++;

		ifp->if_opackets++;
	}

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer.
	 */
	if (txs == NULL)
		sc->sc_tx_timer = 0;
#undef TXSTAT_ERRMASK
#undef TXSTAT_FMT
}

/*
 * atw_watchdog:	[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
void
atw_watchdog(struct ifnet *ifp)
{
	struct atw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ifp->if_timer = 0;
	if (ATW_IS_ENABLED(sc) == 0)
		return;

	if (sc->sc_rescan_timer) {
		if (--sc->sc_rescan_timer == 0)
			(void)ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	}
	if (sc->sc_tx_timer) {
		if (--sc->sc_tx_timer == 0 &&
		    !SIMPLEQ_EMPTY(&sc->sc_txdirtyq)) {
			printf("%s: transmit timeout\n", ifp->if_xname);
			ifp->if_oerrors++;
			(void)atw_init(ifp);
			atw_start(ifp);
		}
	}
	if (sc->sc_tx_timer != 0 || sc->sc_rescan_timer != 0)
		ifp->if_timer = 1;
	ieee80211_watchdog(ifp);
}

/* Compute the 802.11 Duration field and the PLCP Length fields for
 * a len-byte frame (HEADER + PAYLOAD + FCS) sent at rate * 500Kbps.
 * Write the fields to the ADM8211 Tx header, frm.
 *
 * TBD use the fragmentation threshold to find the right duration for
 * the first & last fragments.
 *
 * TBD make certain of the duration fields applied by the ADM8211 to each
 * fragment. I think that the ADM8211 knows how to subtract the CTS
 * duration when ATW_HDRCTL_RTSCTS is clear; that is why I add it regardless.
 * I also think that the ADM8211 does *some* arithmetic for us, because
 * otherwise I think we would have to set a first duration for CTS/first
 * fragment, a second duration for fragments between the first and the
 * last, and a third duration for the last fragment.
 *
 * TBD make certain that duration fields reflect addition of FCS/WEP
 * and correct duration arithmetic as necessary.
 */
static void
atw_frame_setdurs(struct atw_softc *sc, struct atw_frame *frm, int rate,
    int len)
{
	int remainder;

	/* deal also with encrypted fragments */
	if (frm->atw_hdrctl & htole16(ATW_HDRCTL_WEP)) {
		DPRINTF2(sc, ("%s: atw_frame_setdurs len += 8\n",
		    sc->sc_dev.dv_xname));
		len += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN +
		       IEEE80211_WEP_CRCLEN;
	}

	/* 802.11 Duration Field for CTS/Data/ACK sequence minus FCS & WEP
	 * duration (XXX added by MAC?).
	 */
	frm->atw_head_dur = (16 * (len - IEEE80211_CRC_LEN)) / rate;
	remainder = (16 * (len - IEEE80211_CRC_LEN)) % rate;

	if (rate <= 4)
		/* 1-2Mbps WLAN: send ACK/CTS at 1Mbps */
		frm->atw_head_dur += 3 * (IEEE80211_DUR_DS_SIFS +
		    IEEE80211_DUR_DS_SHORT_PREAMBLE +
		    IEEE80211_DUR_DS_FAST_PLCPHDR) +
		    IEEE80211_DUR_DS_SLOW_CTS + IEEE80211_DUR_DS_SLOW_ACK;
	else
		/* 5-11Mbps WLAN: send ACK/CTS at 2Mbps */
		frm->atw_head_dur += 3 * (IEEE80211_DUR_DS_SIFS +
		    IEEE80211_DUR_DS_SHORT_PREAMBLE +
		    IEEE80211_DUR_DS_FAST_PLCPHDR) +
		    IEEE80211_DUR_DS_FAST_CTS + IEEE80211_DUR_DS_FAST_ACK;

	/* lengthen duration if long preamble */
	if ((sc->sc_flags & ATWF_SHORT_PREAMBLE) == 0)
		frm->atw_head_dur +=
		    3 * (IEEE80211_DUR_DS_LONG_PREAMBLE -
		         IEEE80211_DUR_DS_SHORT_PREAMBLE) +
		    3 * (IEEE80211_DUR_DS_SLOW_PLCPHDR -
		         IEEE80211_DUR_DS_FAST_PLCPHDR);

	if (remainder != 0)
		frm->atw_head_dur++;

	if ((atw_voodoo & VOODOO_DUR_2_4_SPECIALCASE) &&
	    (rate == 2 || rate == 4)) {
		/* derived from Linux: how could this be right? */
		frm->atw_head_plcplen = frm->atw_head_dur;
	} else {
		frm->atw_head_plcplen = (16 * len) / rate;
		remainder = (80 * len) % (rate * 5);

		if (remainder != 0) {
			frm->atw_head_plcplen++;

			/* XXX magic */
			if ((atw_voodoo & VOODOO_DUR_11_ROUNDING) &&
			    rate == 22 && remainder <= 30)
				frm->atw_head_plcplen |= 0x8000;
		}
	}
	frm->atw_tail_plcplen = frm->atw_head_plcplen =
	    htole16(frm->atw_head_plcplen);
	frm->atw_tail_dur = frm->atw_head_dur = htole16(frm->atw_head_dur);
}

#ifdef ATW_DEBUG
static void
atw_dump_pkt(struct ifnet *ifp, struct mbuf *m0)
{
	struct atw_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int i, noctets = 0;

	printf("%s: %d-byte packet\n", sc->sc_dev.dv_xname,
	    m0->m_pkthdr.len);

	for (m = m0; m; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		for (i = 0; i < m->m_len; i++) {
			printf(" %02x", ((u_int8_t*)m->m_data)[i]);
			if (++noctets % 24 == 0)
				printf("\n");
		}
	}
	printf("%s%s: %d bytes emitted\n",
	    (noctets % 24 != 0) ? "\n" : "", sc->sc_dev.dv_xname, noctets);
}
#endif /* ATW_DEBUG */

/*
 * atw_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
void
atw_start(struct ifnet *ifp)
{
	struct atw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ieee80211_frame *wh;
	struct atw_frame *hh;
	struct mbuf *m0, *m;
	struct atw_txsoft *txs, *last_txs;
	struct atw_txdesc *txd;
	int do_encrypt, rate;
	bus_dmamap_t dmamap;
	int ctl, error, firsttx, nexttx, lasttx = -1, first, ofree, seg;

	DPRINTF2(sc, ("%s: atw_start: sc_flags 0x%08x, if_flags 0x%08x\n",
	    sc->sc_dev.dv_xname, sc->sc_flags, ifp->if_flags));

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

#if 0 /* TBD ??? */
	if ((sc->sc_flags & ATWF_LINK_UP) == 0 && ifp->if_snd.ifq_len < 10)
		return;
#endif

	/*
	 * Remember the previous number of free descriptors and
	 * the first descriptor we'll use.
	 */
	ofree = sc->sc_txfree;
	firsttx = sc->sc_txnext;

	DPRINTF2(sc, ("%s: atw_start: txfree %d, txnext %d\n",
	    sc->sc_dev.dv_xname, ofree, firsttx));

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	while ((txs = SIMPLEQ_FIRST(&sc->sc_txfreeq)) != NULL &&
	       sc->sc_txfree != 0) {

		do_encrypt = 0;
		/*
		 * Grab a packet off the management queue, if it
		 * is not empty. Otherwise, from the data queue.
		 */
		IF_DEQUEUE(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;
		} else {
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
#if NBPFILTER > 0
			if (ifp->if_bpf != NULL)
				bpf_mtap(ifp->if_bpf, m0);
#endif /* NBPFILTER > 0 */
			if ((m0 = ieee80211_encap(ifp, m0, &ni)) == NULL) {
				ifp->if_oerrors++;
				break;
			}
		}

		rate = MAX(ieee80211_get_rate(ic), 2);

#if NBPFILTER > 0
		/*
		 * Pass the packet to any BPF listeners.
		 */
		if (ic->ic_rawbpf != NULL)
			bpf_mtap((caddr_t)ic->ic_rawbpf, m0);

		if (sc->sc_radiobpf != NULL) {
			struct mbuf mb;
			struct atw_tx_radiotap_header *tap = &sc->sc_txtap;

			tap->at_rate = rate;
			tap->at_chan_freq = ic->ic_bss->ni_chan->ic_freq;
			tap->at_chan_flags = ic->ic_bss->ni_chan->ic_flags;

			/* TBD tap->at_flags */

			M_COPY_PKTHDR(&mb, m0);
			mb.m_data = (caddr_t)tap;
			mb.m_len = tap->at_ihdr.it_len;
			mb.m_next = m0;
			mb.m_pkthdr.len += mb.m_len;
			bpf_mtap(sc->sc_radiobpf, &mb);
		}
#endif /* NBPFILTER > 0 */

		M_PREPEND(m0, offsetof(struct atw_frame, atw_ihdr), M_DONTWAIT);

		if (ni != NULL && ni != ic->ic_bss)
			ieee80211_free_node(ic, ni);

		if (m0 == NULL) {
			ifp->if_oerrors++;
			break;
		}

		/* just to make sure. */
		m0 = m_pullup(m0, sizeof(struct atw_frame));

		if (m0 == NULL) {
			ifp->if_oerrors++;
			break;
		}

		hh = mtod(m0, struct atw_frame *);
		wh = &hh->atw_ihdr;

		do_encrypt = (wh->i_fc[1] & IEEE80211_FC1_WEP) ? 1 : 0;

		/* Copy everything we need from the 802.11 header:
		 * Frame Control; address 1, address 3, or addresses
		 * 3 and 4. NIC fills in BSSID, SA.
		 */
		if (wh->i_fc[1] & IEEE80211_FC1_DIR_TODS) {
			if (wh->i_fc[1] & IEEE80211_FC1_DIR_FROMDS)
				panic("%s: illegal WDS frame",
				    sc->sc_dev.dv_xname);
			memcpy(hh->atw_dst, wh->i_addr3, IEEE80211_ADDR_LEN);
		} else
			memcpy(hh->atw_dst, wh->i_addr1, IEEE80211_ADDR_LEN);

		*(u_int16_t*)hh->atw_fc = *(u_int16_t*)wh->i_fc;

		/* initialize remaining Tx parameters */
		memset(&hh->u, 0, sizeof(hh->u));

		hh->atw_rate = rate * 5;
		/* XXX this could be incorrect if M_FCS. _encap should
		 * probably strip FCS just in case it sticks around in
		 * bridged packets.
		 */
		hh->atw_service = IEEE80211_PLCP_SERVICE; /* XXX guess */
		hh->atw_paylen = htole16(m0->m_pkthdr.len -
		    sizeof(struct atw_frame));

#if 0
		/* this virtually guaranteed that WEP-encrypted frames
		 * are fragmented. oops.
		 */
		hh->atw_fragthr = htole16(m0->m_pkthdr.len -
		    sizeof(struct atw_frame) + sizeof(struct ieee80211_frame));
		hh->atw_fragthr &= htole16(ATW_FRAGTHR_FRAGTHR_MASK);
#else
		hh->atw_fragthr = htole16(ATW_FRAGTHR_FRAGTHR_MASK);
#endif

		hh->atw_rtylmt = 3;
		hh->atw_hdrctl = htole16(ATW_HDRCTL_UNKNOWN1);
		if (do_encrypt) {
			hh->atw_hdrctl |= htole16(ATW_HDRCTL_WEP);
			hh->atw_keyid = ic->ic_wep_txkey;
		}

		/* TBD 4-addr frames */
		atw_frame_setdurs(sc, hh, rate,
		    m0->m_pkthdr.len - sizeof(struct atw_frame) +
		    sizeof(struct ieee80211_frame) + IEEE80211_CRC_LEN);

		/* never fragment multicast frames */
		if (IEEE80211_IS_MULTICAST(hh->atw_dst)) {
			hh->atw_fragthr = htole16(ATW_FRAGTHR_FRAGTHR_MASK);
		} else if (sc->sc_flags & ATWF_RTSCTS) {
			hh->atw_hdrctl |= htole16(ATW_HDRCTL_RTSCTS);
		}

#ifdef ATW_DEBUG
		/* experimental stuff */
		if (atw_xrtylmt != ~0)
			hh->atw_rtylmt = atw_xrtylmt;
		if (atw_xhdrctl != 0)
			hh->atw_hdrctl |= htole16(atw_xhdrctl);
		if (atw_xservice != IEEE80211_PLCP_SERVICE)
			hh->atw_service = atw_xservice;
		if (atw_xpaylen != 0)
			hh->atw_paylen = htole16(atw_xpaylen);
		hh->atw_fragnum = 0;

		if ((ifp->if_flags & IFF_DEBUG) != 0 && atw_debug > 2) {
			printf("%s: dst = %s, rate = 0x%02x, "
			    "service = 0x%02x, paylen = 0x%04x\n",
			    sc->sc_dev.dv_xname, ether_sprintf(hh->atw_dst),
			    hh->atw_rate, hh->atw_service, hh->atw_paylen);

			printf("%s: fc[0] = 0x%02x, fc[1] = 0x%02x, "
			    "dur1 = 0x%04x, dur2 = 0x%04x, "
			    "dur3 = 0x%04x, rts_dur = 0x%04x\n",
			    sc->sc_dev.dv_xname, hh->atw_fc[0], hh->atw_fc[1],
			    hh->atw_tail_plcplen, hh->atw_head_plcplen,
			    hh->atw_tail_dur, hh->atw_head_dur);

			printf("%s: hdrctl = 0x%04x, fragthr = 0x%04x, "
			    "fragnum = 0x%02x, rtylmt = 0x%04x\n",
			    sc->sc_dev.dv_xname, hh->atw_hdrctl,
			    hh->atw_fragthr, hh->atw_fragnum, hh->atw_rtylmt);

			printf("%s: keyid = %d\n",
			    sc->sc_dev.dv_xname, hh->atw_keyid);

			atw_dump_pkt(ifp, m0);
		}
#endif /* ATW_DEBUG */

		dmamap = txs->txs_dmamap;

		/*
		 * Load the DMA map.  Copy and try (once) again if the packet
		 * didn't fit in the alloted number of segments.
		 */
		for (first = 1;
		     (error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		                  BUS_DMA_WRITE|BUS_DMA_NOWAIT)) != 0 && first;
		     first = 0) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				printf("%s: unable to allocate Tx mbuf\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			if (m0->m_pkthdr.len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					printf("%s: unable to allocate Tx "
					    "cluster\n", sc->sc_dev.dv_xname);
					m_freem(m);
					break;
				}
			}
			m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, caddr_t));
			m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			m_freem(m0);
			m0 = m;
			m = NULL;
		}
		if (error != 0) {
			printf("%s: unable to load Tx buffer, "
			    "error = %d\n", sc->sc_dev.dv_xname, error);
			m_freem(m0);
			break;
		}

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.
		 */
		if (dmamap->dm_nsegs > sc->sc_txfree) {
			/*
			 * Not enough free descriptors to transmit
			 * this packet.  Unload the DMA map and
			 * drop the packet.  Notify the upper layer
			 * that there are no more slots left.
			 *
			 * XXX We could allocate an mbuf and copy, but
			 * XXX it is worth it?
			 */
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			m_freem(m0);
			break;
		}

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/* XXX arbitrary retry limit; 8 because I have seen it in
		 * use already and maybe 0 means "no tries" !
		 */
		ctl = htole32(LSHIFT(8, ATW_TXCTL_TL_MASK));

		DPRINTF2(sc, ("%s: TXDR <- max(10, %d)\n",
		    sc->sc_dev.dv_xname, rate * 5));
		ctl |= htole32(LSHIFT(MAX(10, rate * 5), ATW_TXCTL_TXDR_MASK));

		/*
		 * Initialize the transmit descriptors.
		 */
		for (nexttx = sc->sc_txnext, seg = 0;
		     seg < dmamap->dm_nsegs;
		     seg++, nexttx = ATW_NEXTTX(nexttx)) {
			/*
			 * If this is the first descriptor we're
			 * enqueueing, don't set the OWN bit just
			 * yet.  That could cause a race condition.
			 * We'll do it below.
			 */
			txd = &sc->sc_txdescs[nexttx];
			txd->at_ctl = ctl |
			    ((nexttx == firsttx) ? 0 : htole32(ATW_TXCTL_OWN));
 
			txd->at_buf1 = htole32(dmamap->dm_segs[seg].ds_addr);
			txd->at_flags =
			    htole32(LSHIFT(dmamap->dm_segs[seg].ds_len,
			                   ATW_TXFLAG_TBS1_MASK)) |
			    ((nexttx == (ATW_NTXDESC - 1))
			        ? htole32(ATW_TXFLAG_TER) : 0);
			lasttx = nexttx;
		}

		IASSERT(lasttx != -1, ("bad lastx"));
		/* Set `first segment' and `last segment' appropriately. */
		sc->sc_txdescs[sc->sc_txnext].at_flags |=
		    htole32(ATW_TXFLAG_FS);
		sc->sc_txdescs[lasttx].at_flags |= htole32(ATW_TXFLAG_LS);

#ifdef ATW_DEBUG
		if ((ifp->if_flags & IFF_DEBUG) != 0 && atw_debug > 2) {
			printf("     txsoft %p transmit chain:\n", txs);
			for (seg = sc->sc_txnext;; seg = ATW_NEXTTX(seg)) {
				printf("     descriptor %d:\n", seg);
				printf("       at_ctl:   0x%08x\n",
				    le32toh(sc->sc_txdescs[seg].at_ctl));
				printf("       at_flags:      0x%08x\n",
				    le32toh(sc->sc_txdescs[seg].at_flags));
				printf("       at_buf1: 0x%08x\n",
				    le32toh(sc->sc_txdescs[seg].at_buf1));
				printf("       at_buf2: 0x%08x\n",
				    le32toh(sc->sc_txdescs[seg].at_buf2));
				if (seg == lasttx)
					break;
			}
		}
#endif

		/* Sync the descriptors we're using. */
		ATW_CDTXSYNC(sc, sc->sc_txnext, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * Store a pointer to the packet so we can free it later,
		 * and remember what txdirty will be once the packet is
		 * done.
		 */
		txs->txs_mbuf = m0;
		txs->txs_firstdesc = sc->sc_txnext;
		txs->txs_lastdesc = lasttx;
		txs->txs_ndescs = dmamap->dm_nsegs;

		/* Advance the tx pointer. */
		sc->sc_txfree -= dmamap->dm_nsegs;
		sc->sc_txnext = nexttx;

		SIMPLEQ_REMOVE_HEAD(&sc->sc_txfreeq, txs_q);
		SIMPLEQ_INSERT_TAIL(&sc->sc_txdirtyq, txs, txs_q);

		last_txs = txs;
	}

	if (txs == NULL || sc->sc_txfree == 0) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txfree != ofree) {
		DPRINTF2(sc, ("%s: packets enqueued, IC on %d, OWN on %d\n",
		    sc->sc_dev.dv_xname, lasttx, firsttx));
		/*
		 * Cause a transmit interrupt to happen on the
		 * last packet we enqueued.
		 */
		sc->sc_txdescs[lasttx].at_flags |= htole32(ATW_TXFLAG_IC);
		ATW_CDTXSYNC(sc, lasttx, 1,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * The entire packet chain is set up.  Give the
		 * first descriptor to the chip now.
		 */
		sc->sc_txdescs[firsttx].at_ctl |= htole32(ATW_TXCTL_OWN);
		ATW_CDTXSYNC(sc, firsttx, 1,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Wake up the transmitter. */
		/* XXX USE AUTOPOLLING? */
		ATW_WRITE(sc, ATW_TDR, 0x1);

		/* Set a watchdog timer in case the chip flakes out. */
		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

/*
 * atw_power:
 *
 *	Power management (suspend/resume) hook.
 */
void
atw_power(int why, void *arg)
{
	struct atw_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	DPRINTF(sc, ("%s: atw_power(%d,)\n", sc->sc_dev.dv_xname, why));

	s = splnet();
	switch (why) {
	case PWR_STANDBY:
		/* XXX do nothing. */
		break;
	case PWR_SUSPEND:
		atw_stop(ifp, 0);
		if (sc->sc_power != NULL)
			(*sc->sc_power)(sc, why);
		break;
	case PWR_RESUME:
		if (ifp->if_flags & IFF_UP) {
			if (sc->sc_power != NULL)
				(*sc->sc_power)(sc, why);
			atw_init(ifp);
		}
		break;
	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
	}
	splx(s);
}

/*
 * atw_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
int
atw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct atw_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	/* XXX monkey see, monkey do. comes from wi_ioctl. */
	if ((sc->sc_dev.dv_flags & DVF_ACTIVE) == 0)
		return ENXIO;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ATW_IS_ENABLED(sc)) {
				/*
				 * To avoid rescanning another access point,
				 * do not call atw_init() here.  Instead,
				 * only reflect media settings.
				 */
				atw_filter_setup(sc);
			} else
				error = atw_init(ifp);
		} else if (ATW_IS_ENABLED(sc))
			atw_stop(ifp, 1);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ic.ic_ec) :
		    ether_delmulti(ifr, &sc->sc_ic.ic_ec);
		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				atw_filter_setup(sc); /* do not rescan */
			error = 0;
		}
		break;
	default:
		error = ieee80211_ioctl(ifp, cmd, data);
		if (error == ENETRESET) {
			if (ATW_IS_ENABLED(sc))
				error = atw_init(ifp);
			else
				error = 0;
		}
		break;
	}

	/* Try to get more packets going. */
	if (ATW_IS_ENABLED(sc))
		atw_start(ifp);

	splx(s);
	return (error);
}

static int
atw_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) ==
		    (IFF_RUNNING|IFF_UP))
			atw_init(ifp);		/* XXX lose error */
		error = 0;
	}
	return error;
}

static void
atw_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct atw_softc *sc = ifp->if_softc;

	if (ATW_IS_ENABLED(sc) == 0) {
		imr->ifm_active = IFM_IEEE80211 | IFM_NONE;
		imr->ifm_status = 0;
		return;
	}
	ieee80211_media_status(ifp, imr);
}
