/*	$NetBSD: if_ade.c,v 1.28.18.1 2007/09/03 16:46:57 jmcneill Exp $	*/

/*
 * NOTE: this version of if_de was modified for bounce buffers prior
 * to the introduction of bus_dma to netbsd. When a busified if_de
 * is available and the a12 platform bus_dma is complete, then this
 * module will be taken for a long walk on a short pier.
 */

/* [Notice revision 2.0]
 * Copyright (c) 1997 Avalon Computer Systems, Inc.
 * All rights reserved.
 *
 * Author: Ross Harvey
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright and
 *    author notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Avalon Computer Systems, Inc. nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. This copyright will be assigned to The NetBSD Foundation on
 *    1/1/2000 unless these terms (including possibly the assignment
 *    date) are updated in writing by Avalon prior to the latest specified
 *    assignment date.
 *
 * THIS SOFTWARE IS PROVIDED BY AVALON COMPUTER SYSTEMS, INC. AND CONTRIBUTORS
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
/*-
 * Copyright (c) 1994-1997 Matt Thomas (matt@3am-software.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Id: if_de.c,v 1.83 1997/03/25 21:12:17 thomas Exp
 *
 */

/*
 * DEC 21040 PCI Ethernet Controller
 *
 * Written by Matt Thomas
 * BPF support code stolen directly from if_ec.c
 *
 *   This driver supports the DEC DE435 or any other PCI
 *   board which support 21040, 21041, or 21140 (mostly).
 */
#define	TULIP_HDR_DATA
#define	LCLDMA 1

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ade.c,v 1.28.18.1 2007/09/03 16:46:57 jmcneill Exp $");

#include "opt_inet.h"
#include "opt_ns.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>	/* only for declaration of wakeup() used by vm.h */
#if defined(__FreeBSD__)
#include <machine/clock.h>
#elif defined(__bsdi__) || defined(__NetBSD__)
#include <sys/device.h>
#endif

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>

#if defined(__bsdi__) && _BSDI_VERSION >= 199701
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <uvm/uvm_extern.h>

#if defined(__FreeBSD__)
#include <vm/pmap.h>
#include <pci.h>
#include <netinet/if_ether.h>
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/dc21040.h>
#define	DEVAR_INCLUDE	"pci/if_devar.h"
#endif
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
#include <netinet/if_ether.h>
#include <i386/pci/ic/dc21040.h>
#include <i386/isa/isa.h>
#include <i386/isa/icu.h>
#include <i386/isa/dma.h>
#include <i386/isa/isavar.h>
#include <i386/pci/pci.h>
#if _BSDI_VERSION < 199510
#include <eisa.h>
#else
#define	NEISA 0
#endif
#if NEISA > 0 && _BSDI_VERSION >= 199401
#include <i386/eisa/eisa.h>
#define	TULIP_EISA
#endif
#define	DEVAR_INCLUDE	"i386/pci/if_devar.h"
#endif /* __bsdi__ */

#if defined(__NetBSD__)
#include <net/if_ether.h>
#if defined(INET)
#include <netinet/if_inarp.h>
#endif
#include <machine/bus.h>
#if defined(__alpha__)
#include <machine/intr.h>
#endif
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ic/dc21040reg.h>
#endif /* __NetBSD__ */

/*
 * Intel CPUs should use I/O mapped access.
 */
#if defined(__i386__) || defined(TULIP_EISA)
#define	TULIP_IOMAPPED
#endif

#if 0
/*
 * This turns on all sort of debugging stuff and make the
 * driver much larger.
 */
#define TULIP_DEBUG
#endif

#if 0
#define	TULIP_USE_SOFTINTR
#endif

#define	TULIP_HZ	10

#include <alpha/a12/if_adevar.h>
tulip_softc_t * debug_sc;	/* XXX */
/*
 * This module supports
 *	the DEC 21040 PCI Ethernet Controller.
 *	the DEC 21041 PCI Ethernet Controller.
 *	the DEC 21140 PCI Fast Ethernet Controller.
 */

static void tulip_mii_autonegotiate(tulip_softc_t * const sc, const unsigned phyaddr);
static tulip_intrfunc_t tulip_intr_shared(void *arg);
static tulip_intrfunc_t tulip_intr_normal(void *arg);
static void tulip_init(tulip_softc_t * const sc);
static void tulip_reset(tulip_softc_t * const sc);
static ifnet_ret_t tulip_ifstart(struct ifnet *ifp);
static void tulip_rx_intr(tulip_softc_t * const sc);
static void tulip_addr_filter(tulip_softc_t * const sc);
static unsigned tulip_mii_readreg(tulip_softc_t * const sc, unsigned devaddr, unsigned regno);
static void tulip_mii_writereg(tulip_softc_t * const sc, unsigned devaddr, unsigned regno, unsigned data);
static int tulip_mii_map_abilities(tulip_softc_t * const sc, unsigned abilities);
static tulip_media_t tulip_mii_phy_readspecific(tulip_softc_t * const sc);
static int tulip_srom_decode(tulip_softc_t * const sc);
static int tulip_ifmedia_change(struct ifnet * const ifp);
static void tulip_ifmedia_status(struct ifnet * const ifp, struct ifmediareq *req);
/* static void tulip_21140_map_media(tulip_softc_t *sc); */
#ifdef LCLDMA
static void reset_lcl_dma(tulip_softc_t * const sc);
static void a12_m_copydata(struct mbuf *m, int off, int len, void *cp);
static void dumpring(void **);
/* 
 * Note for LCLDMA mods. These are for systems such as the Avalon
 * A12 that can't DMA to main memory, but instead have an essentially
 * dual-ported local buffer memory.  Other systems with this kind
 * of bus architecture include many of the VAXstation models. Of
 * course, they won't be needing de(4) drivers.
 */
static void
donothing(struct mbuf *m, void *buf, size_t size, void *arg)
{
	int s;

	if (__predict_true(m != NULL)) {
		s = splvm();
		pool_cache_put(&mbpool_cache, m);
		splx(s);
	}

}
static void a12r2pb(void *vsrc, void *vdst, int len) {
	long	bounce[9];
	int	i,
		offset,	/* destination misalignment */
		t,	/* segment count */
		llw;	/* last longword index */

	char	*src = vsrc;
	long	*dst = vdst;

	offset = (long)dst & 7;
	dst = (long *)((long)dst & ~7L);
	alpha_wmb();
	while(len) {
		t = 64;
		if(t > len)
			t = len;
		llw = (t+offset) >> 3;
		if(((t|offset) & 7) == 0 && llw)
			--llw;
		else	bounce[llw] = dst[llw];
		/*
		 * The whole idea is that llw indexes the first long
		 * that contains at least 1 byte NOT to be overwritten.
		 * If we are an even, aligned transfer, then bump down
		 * llw to avoid an extra load/store to the slow buffer
		 * mem.
		 */
		if(offset)
			bounce[0] = dst[0];
		memcpy((int8_t *)bounce+offset,src,t);
		for(i=llw; i>=0; --i) {	/* reverse so d_status is last */
			alpha_mb();
			dst[i] = bounce[i];
			alpha_mb();
		}
#if 0
{ static int printcount;
		if(++printcount<=4) {
			for(i=llw; i>=0; --i) {
				alpha_mb();
				printf("dst[%d]=%16lx@%16lx bounce[%d]=%16lx\n",
					i, dst[i], &dst[i], i, bounce[i]);
				alpha_mb();
			}
		}
}
#endif
		dst += 8;
		src += t;
		len -= t;
	}
	alpha_wmb();
}

#define BEGIN(p) do { tulip_desc_t t; memcpy(&t,(p),sizeof(t))
#define	END(p)			   a12r2pb(&t,(p),sizeof(t)); } while(0)

static void setstatus(tulip_desc_t *t, u_int32_t val) {
	a12r2pb(&val, &t->d_status, sizeof(t->d_status));
}

#ifdef LCLDMA
#include <machine/rpb.h>
static u_int32_t a12map(u_int32_t p) {
	if(cputype==ST_AVALON_A12)
		p = (p & 0xfffffff) | 0x80000000;
	return p;
}
#endif
#endif

static void
tulip_timeout_callback(
    void *arg)
{
    tulip_softc_t * const sc = arg;
    tulip_spl_t s = TULIP_RAISESPL();

    sc->tulip_flags &= ~TULIP_TIMEOUTPENDING;
    sc->tulip_probe_timeout -= 1000 / TULIP_HZ;
    (sc->tulip_boardsw->bd_media_poll)(sc, TULIP_MEDIAPOLL_TIMER);
    TULIP_RESTORESPL(s);
}

static void
tulip_timeout(
    tulip_softc_t * const sc)
{
    if (sc->tulip_flags & TULIP_TIMEOUTPENDING)
	return;
    sc->tulip_flags |= TULIP_TIMEOUTPENDING;
    timeout(tulip_timeout_callback, sc, (hz + TULIP_HZ / 2) / TULIP_HZ);
}

#if defined(TULIP_NEED_FASTTIMEOUT)
static void
tulip_fasttimeout_callback(
    void *arg)
{
    tulip_softc_t * const sc = arg;
    tulip_spl_t s = TULIP_RAISESPL();

    sc->tulip_flags &= ~TULIP_FASTTIMEOUTPENDING;
    (sc->tulip_boardsw->bd_media_poll)(sc, TULIP_MEDIAPOLL_FASTTIMER);
    TULIP_RESTORESPL(s);
}

static void
tulip_fasttimeout(
    tulip_softc_t * const sc)
{
    if (sc->tulip_flags & TULIP_FASTTIMEOUTPENDING)
	return;
    sc->tulip_flags |= TULIP_FASTTIMEOUTPENDING;
    timeout(tulip_fasttimeout_callback, sc, 1);
}
#endif

static int
tulip_txprobe(
    tulip_softc_t * const sc)
{
    struct mbuf *m;
    /*
     * Before we are sure this is the right media we need
     * to send a small packet to make sure there's carrier.
     * Strangely, BNC and AUI will 'see" receive data if
     * either is connected so the transmit is the only way
     * to verify the connectivity.
     */
    m = m_gethdr(M_DONTWAIT, MT_DATA);
    if (m == NULL)
	return 0;
    /*
     * Construct a LLC TEST message which will point to ourselves.
     */
    memcpy(mtod(m, struct ether_header *)->ether_dhost, sc->tulip_enaddr, 6);
    memcpy(mtod(m, struct ether_header *)->ether_shost, sc->tulip_enaddr, 6);
    mtod(m, struct ether_header *)->ether_type = htons(3);
    mtod(m, unsigned char *)[14] = 0;
    mtod(m, unsigned char *)[15] = 0;
    mtod(m, unsigned char *)[16] = 0xE3;	/* LLC Class1 TEST (no poll) */
    m->m_len = m->m_pkthdr.len = sizeof(struct ether_header) + 3;
    /*
     * send it!
     */
    sc->tulip_cmdmode |= TULIP_CMD_TXRUN;
    sc->tulip_flags |= TULIP_TXPROBE_ACTIVE;
    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
    IF_PREPEND(&sc->tulip_if.if_snd, m);
    tulip_ifstart(&sc->tulip_if);
    sc->tulip_probe.probe_txprobes++;
    return 1;
}

#ifdef BIG_PACKET
#define TULIP_SIAGEN_WATCHDOG	(sc->tulip_if.if_mtu > ETHERMTU ? TULIP_WATCHDOG_RXDISABLE|TULIP_WATCHDOG_TXDISABLE : 0)
#else
#define	TULIP_SIAGEN_WATCHDOG	0
#endif

static void
tulip_media_set(
    tulip_softc_t * const sc,
    tulip_media_t media)
{
    const tulip_media_info_t *mi = sc->tulip_mediums[media];

    if (mi == NULL)
	return;

    /*
     * If we are switching media, make sure we don't think there's
     * any stale RX activity
     */
    sc->tulip_flags &= ~TULIP_RXACT;
    if (mi->mi_type == TULIP_MEDIAINFO_SIA) {
	TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
	TULIP_CSR_WRITE(sc, csr_sia_tx_rx,        mi->mi_sia_tx_rx);
	if (sc->tulip_features & TULIP_HAVE_SIAGP) {
	    TULIP_CSR_WRITE(sc, csr_sia_general,  mi->mi_sia_gp_control|mi->mi_sia_general|TULIP_SIAGEN_WATCHDOG);
	    TULIP_CSR_WRITE(sc, csr_sia_general,  mi->mi_sia_gp_data|mi->mi_sia_general|TULIP_SIAGEN_WATCHDOG);
	} else {
	    TULIP_CSR_WRITE(sc, csr_sia_general,  mi->mi_sia_general|TULIP_SIAGEN_WATCHDOG);
	}
	TULIP_CSR_WRITE(sc, csr_sia_connectivity, mi->mi_sia_connectivity);
    } else if (mi->mi_type == TULIP_MEDIAINFO_GPR) {
#define	TULIP_GPR_CMDBITS	(TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER|TULIP_CMD_TXTHRSHLDCTL)
	/*
	 * If the cmdmode bits don't match the currently operating mode,
	 * set the cmdmode appropriately and reset the chip.
	 */
	if (((mi->mi_cmdmode ^ TULIP_CSR_READ(sc, csr_command)) & TULIP_GPR_CMDBITS) != 0) {
	    sc->tulip_cmdmode &= ~TULIP_GPR_CMDBITS;
	    sc->tulip_cmdmode |= mi->mi_cmdmode;
	    tulip_reset(sc);
	}
	TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_PINSET|sc->tulip_gpinit);
	DELAY(10);
	TULIP_CSR_WRITE(sc, csr_gp, (u_int8_t) mi->mi_gpdata);
    } else if (mi->mi_type == TULIP_MEDIAINFO_SYM) {
	/*
	 * If the cmdmode bits don't match the currently operating mode,
	 * set the cmdmode appropriately and reset the chip.
	 */
	if (((mi->mi_cmdmode ^ TULIP_CSR_READ(sc, csr_command)) & TULIP_GPR_CMDBITS) != 0) {
	    sc->tulip_cmdmode &= ~TULIP_GPR_CMDBITS;
	    sc->tulip_cmdmode |= mi->mi_cmdmode;
	    tulip_reset(sc);
	}
	TULIP_CSR_WRITE(sc, csr_sia_general, mi->mi_gpcontrol);
	TULIP_CSR_WRITE(sc, csr_sia_general, mi->mi_gpdata);
    } else if (mi->mi_type == TULIP_MEDIAINFO_MII
	       && sc->tulip_probe_state != TULIP_PROBE_INACTIVE) {
	int idx;
	if (sc->tulip_features & TULIP_HAVE_SIAGP) {
	    const u_int8_t *dp;
	    dp = &sc->tulip_rombuf[mi->mi_reset_offset];
	    for (idx = 0; idx < mi->mi_reset_length; idx++, dp += 2) {
		DELAY(10);
		TULIP_CSR_WRITE(sc, csr_sia_general, (dp[0] + 256 * dp[1]) << 16);
	    }
	    sc->tulip_phyaddr = mi->mi_phyaddr;
	    dp = &sc->tulip_rombuf[mi->mi_gpr_offset];
	    for (idx = 0; idx < mi->mi_gpr_length; idx++, dp += 2) {
		DELAY(10);
		TULIP_CSR_WRITE(sc, csr_sia_general, (dp[0] + 256 * dp[1]) << 16);
	    }
	} else {
	    for (idx = 0; idx < mi->mi_reset_length; idx++) {
		DELAY(10);
		TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_rombuf[mi->mi_reset_offset + idx]);
	    }
	    sc->tulip_phyaddr = mi->mi_phyaddr;
	    for (idx = 0; idx < mi->mi_gpr_length; idx++) {
		DELAY(10);
		TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_rombuf[mi->mi_gpr_offset + idx]);
	    }
	}
	if (sc->tulip_flags & TULIP_TRYNWAY) {
	    tulip_mii_autonegotiate(sc, sc->tulip_phyaddr);
	} else if ((sc->tulip_flags & TULIP_DIDNWAY) == 0) {
	    u_int32_t data = tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_CONTROL);
	    data &= ~(PHYCTL_SELECT_100MB|PHYCTL_FULL_DUPLEX|PHYCTL_AUTONEG_ENABLE);
	    sc->tulip_flags &= ~TULIP_DIDNWAY;
	    if (TULIP_IS_MEDIA_FD(media))
		data |= PHYCTL_FULL_DUPLEX;
	    if (TULIP_IS_MEDIA_100MB(media))
		data |= PHYCTL_SELECT_100MB;
	    tulip_mii_writereg(sc, sc->tulip_phyaddr, PHYREG_CONTROL, data);
	}
    }
}

static void
tulip_linkup(
    tulip_softc_t * const sc,
    tulip_media_t media)
{
    if ((sc->tulip_flags & TULIP_LINKUP) == 0)
	sc->tulip_flags |= TULIP_PRINTLINKUP;
    sc->tulip_flags |= TULIP_LINKUP;
    sc->tulip_if.if_flags &= ~IFF_OACTIVE;
#if 0 /* XXX how does with work with ifmedia? */
    if ((sc->tulip_flags & TULIP_DIDNWAY) == 0) {
	if (sc->tulip_if.if_flags & IFF_FULLDUPLEX) {
	    if (TULIP_CAN_MEDIA_FD(media)
		    && sc->tulip_mediums[TULIP_FD_MEDIA_OF(media)] != NULL)
		media = TULIP_FD_MEDIA_OF(media);
	} else {
	    if (TULIP_IS_MEDIA_FD(media)
		    && sc->tulip_mediums[TULIP_HD_MEDIA_OF(media)] != NULL)
		media = TULIP_HD_MEDIA_OF(media);
	}
    }
#endif
    if (sc->tulip_media != media) {
#ifdef TULIP_DEBUG
	sc->tulip_dbg.dbg_last_media = sc->tulip_media;
#endif
	sc->tulip_media = media;
	sc->tulip_flags |= TULIP_PRINTMEDIA;
	if (TULIP_IS_MEDIA_FD(sc->tulip_media)) {
	    sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
	} else if (sc->tulip_chipid != TULIP_21041 || (sc->tulip_flags & TULIP_DIDNWAY) == 0) {
	    sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	}
    }
    /*
     * We could set probe_timeout to 0 but setting to 3000 puts this
     * in one central place and the only matters is tulip_link is
     * followed by a tulip_timeout.  Therefore setting it should not
     * result in aberrant behavour.
     */
    sc->tulip_probe_timeout = 3000;
    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
    sc->tulip_flags &= ~(TULIP_TXPROBE_ACTIVE|TULIP_TRYNWAY);
    if (sc->tulip_flags & TULIP_INRESET) {
	tulip_media_set(sc, sc->tulip_media);
    } else {
	tulip_reset(sc);
	tulip_init(sc);
    }
}

static void
tulip_media_print(
    tulip_softc_t * const sc)
{
    if ((sc->tulip_flags & TULIP_LINKUP) == 0)
	return;
    if (sc->tulip_flags & TULIP_PRINTMEDIA) {
	printf(TULIP_PRINTF_FMT ": enabling %s port\n",
	       TULIP_PRINTF_ARGS,
	       tulip_mediums[sc->tulip_media]);
	sc->tulip_flags &= ~(TULIP_PRINTMEDIA|TULIP_PRINTLINKUP);
    } else if (sc->tulip_flags & TULIP_PRINTLINKUP) {
	printf(TULIP_PRINTF_FMT ": link up\n", TULIP_PRINTF_ARGS);
	sc->tulip_flags &= ~TULIP_PRINTLINKUP;
    }
}

#if defined(TULIP_DO_GPR_SENSE)
static tulip_media_t
tulip_21140_gpr_media_sense(
    tulip_softc_t * const sc)
{
    tulip_media_t maybe_media = TULIP_MEDIA_UNKNOWN;
    tulip_media_t last_media = TULIP_MEDIA_UNKNOWN;
    tulip_media_t media;

    /*
     * If one of the media blocks contained a default media flag,
     * use that.
     */
    for (media = TULIP_MEDIA_UNKNOWN; media < TULIP_MEDIA_MAX; media++) {
	const tulip_media_info_t *mi;
	/*
	 * Media is not supported (or is full-duplex).
	 */
	if ((mi = sc->tulip_mediums[media]) == NULL || TULIP_IS_MEDIA_FD(media))
	    continue;
	if (mi->mi_type != TULIP_MEDIAINFO_GPR)
	    continue;

	/*
	 * Remember the media is this is the "default" media.
	 */
	if (mi->mi_default && maybe_media == TULIP_MEDIA_UNKNOWN)
	    maybe_media = media;

	/*
	 * No activity mask?  Can't see if it is active if there's no mask.
	 */
	if (mi->mi_actmask == 0)
	    continue;

	/*
	 * Does the activity data match?
	 */
	if ((TULIP_CSR_READ(sc, csr_gp) & mi->mi_actmask) != mi->mi_actdata)
	    continue;

#if defined(TULIP_DEBUG)
	printf(TULIP_PRINTF_FMT ": gpr_media_sense: %s: 0x%02x & 0x%02x == 0x%02x\n",
	       TULIP_PRINTF_ARGS, tulip_mediums[media],
	       TULIP_CSR_READ(sc, csr_gp) & 0xFF,
	       mi->mi_actmask, mi->mi_actdata);
#endif
	/*
	 * It does!  If this is the first media we detected, then 
	 * remember this media.  If isn't the first, then there were
	 * multiple matches which we equate to no match (since we don't
	 * which to select (if any).
	 */
	if (last_media == TULIP_MEDIA_UNKNOWN) {
	    last_media = media;
	} else if (last_media != media) {
	    last_media = TULIP_MEDIA_UNKNOWN;
	}
    }
    return (last_media != TULIP_MEDIA_UNKNOWN) ? last_media : maybe_media;
}
#endif /* TULIP_DO_GPR_SENSE */

static tulip_link_status_t
tulip_media_link_monitor(
    tulip_softc_t * const sc)
{
    const tulip_media_info_t * const mi = sc->tulip_mediums[sc->tulip_media];
    tulip_link_status_t linkup = TULIP_LINK_DOWN;

    if (mi == NULL) {
#if defined(DIAGNOSTIC) || defined(TULIP_DEBUG)
	panic("tulip_media_link_monitor: %s: botch at line %d",
	      tulip_mediums[sc->tulip_media],__LINE__);
#endif
	return TULIP_LINK_UNKNOWN;
    }


    /*
     * Have we seen some packets?  If so, the link must be good.
     */
    if ((sc->tulip_flags & (TULIP_RXACT|TULIP_LINKUP)) == (TULIP_RXACT|TULIP_LINKUP)) {
	sc->tulip_flags &= ~TULIP_RXACT;
	sc->tulip_probe_timeout = 3000;
	return TULIP_LINK_UP;
    }

    sc->tulip_flags &= ~TULIP_RXACT;
    if (mi->mi_type == TULIP_MEDIAINFO_MII) {
	u_int32_t status;
	/*
	 * Read the PHY status register.
	 */
	status = tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_STATUS);
	if (status & PHYSTS_AUTONEG_DONE) {
	    /*
	     * If the PHY has completed autonegotiation, see the if the
	     * remote systems abilities have changed.  If so, upgrade or
	     * downgrade as appropriate.
	     */
	    u_int32_t abilities = tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_AUTONEG_ABILITIES);
	    abilities = (abilities << 6) & status;
	    if (abilities != sc->tulip_abilities) {
#if defined(TULIP_DEBUG)
		loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation changed: 0x%04x -> 0x%04x\n",
			   TULIP_PRINTF_ARGS, sc->tulip_phyaddr,
			   sc->tulip_abilities, abilities);
#endif
		if (tulip_mii_map_abilities(sc, abilities)) {
		    tulip_linkup(sc, sc->tulip_probe_media);
		    return TULIP_LINK_UP;
		}
		/*
		 * if we had selected media because of autonegotiation,
		 * we need to probe for the new media.
		 */
		sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
		if (sc->tulip_flags & TULIP_DIDNWAY)
		    return TULIP_LINK_DOWN;
	    }
	}
	/*
	 * The link is now up.  If was down, say its back up.
	 */
	if ((status & (PHYSTS_LINK_UP|PHYSTS_REMOTE_FAULT)) == PHYSTS_LINK_UP)
	    linkup = TULIP_LINK_UP;
    } else if (mi->mi_type == TULIP_MEDIAINFO_GPR) {
	/*
	 * No activity sensor?  Assume all's well.
	 */
	if (mi->mi_actmask == 0)
	    return TULIP_LINK_UNKNOWN;
	/*
	 * Does the activity data match?
	 */
	if ((TULIP_CSR_READ(sc, csr_gp) & mi->mi_actmask) == mi->mi_actdata)
	    linkup = TULIP_LINK_UP;
    } else if (mi->mi_type == TULIP_MEDIAINFO_SIA) {
	/*
	 * Assume non TP ok for now.
	 */
	if (!TULIP_IS_MEDIA_TP(sc->tulip_media))
	    return TULIP_LINK_UNKNOWN;
	if ((TULIP_CSR_READ(sc, csr_sia_status) & TULIP_SIASTS_LINKFAIL) == 0)
	    linkup = TULIP_LINK_UP;
    } else if (mi->mi_type == TULIP_MEDIAINFO_SYM) {
	return TULIP_LINK_UNKNOWN;
    }
    /*
     * We will wait for 3 seconds until the link goes into suspect mode.
     */
    if (sc->tulip_flags & TULIP_LINKUP) {
	if (linkup == TULIP_LINK_UP)
	    sc->tulip_probe_timeout = 3000;
	if (sc->tulip_probe_timeout > 0)
	    return TULIP_LINK_UP;

	sc->tulip_flags &= ~TULIP_LINKUP;
	printf(TULIP_PRINTF_FMT ": link down: cable problem?\n", TULIP_PRINTF_ARGS);
    }
#if defined(TULIP_DEBUG)
    sc->tulip_dbg.dbg_link_downed++;
#endif
    return TULIP_LINK_DOWN;
}

static void
tulip_media_poll(
    tulip_softc_t * const sc,
    tulip_mediapoll_event_t event)
{
#if defined(TULIP_DEBUG)
    sc->tulip_dbg.dbg_events[event]++;
#endif
    if (sc->tulip_probe_state == TULIP_PROBE_INACTIVE
	    && event == TULIP_MEDIAPOLL_TIMER) {
	switch (tulip_media_link_monitor(sc)) {
	    case TULIP_LINK_DOWN: {
		/*
		 * Link Monitor failed.  Probe for new media.
		 */
		event = TULIP_MEDIAPOLL_LINKFAIL;
		break;
	    }
	    case TULIP_LINK_UP: {
		/*
		 * Check again soon.
		 */
		tulip_timeout(sc);
		return;
	    }
	    case TULIP_LINK_UNKNOWN: {
		/*
		 * We can't tell so don't bother.
		 */
		return;
	    }
	}
    }

    if (event == TULIP_MEDIAPOLL_LINKFAIL) {
	if (sc->tulip_probe_state == TULIP_PROBE_INACTIVE) {
	    if (TULIP_DO_AUTOSENSE(sc)) {
#if defined(TULIP_DEBUG)
		sc->tulip_dbg.dbg_link_failures++;
#endif
		sc->tulip_media = TULIP_MEDIA_UNKNOWN;
		tulip_reset(sc);	/* restart probe */
	    }
	    return;
	}
#if defined(TULIP_DEBUG)
	sc->tulip_dbg.dbg_link_pollintrs++;
#endif
    }

    if (event == TULIP_MEDIAPOLL_START) {
	sc->tulip_if.if_flags |= IFF_OACTIVE;
	if (sc->tulip_probe_state != TULIP_PROBE_INACTIVE)
	    return;
	sc->tulip_probe_mediamask = 0;
	sc->tulip_probe_passes = 0;
#if defined(TULIP_DEBUG)
	sc->tulip_dbg.dbg_media_probes++;
#endif
	/*
	 * If the SROM contained an explicit media to use, use it.
	 */
	sc->tulip_cmdmode &= ~(TULIP_CMD_RXRUN|TULIP_CMD_FULLDUPLEX);
	sc->tulip_flags |= TULIP_TRYNWAY|TULIP_PROBE1STPASS;
	sc->tulip_flags &= ~(TULIP_DIDNWAY|TULIP_PRINTMEDIA|TULIP_PRINTLINKUP);
	/*
	 * connidx is defaulted to a media_unknown type.
	 */
	sc->tulip_probe_media = tulip_srom_conninfo[sc->tulip_connidx].sc_media;
	if (sc->tulip_probe_media != TULIP_MEDIA_UNKNOWN) {
	    tulip_linkup(sc, sc->tulip_probe_media);
	    tulip_timeout(sc);
	    return;
	}

	if (sc->tulip_features & TULIP_HAVE_GPR) {
	    sc->tulip_probe_state = TULIP_PROBE_GPRTEST;
	    sc->tulip_probe_timeout = 2000;
	} else {
	    sc->tulip_probe_media = TULIP_MEDIA_MAX;
	    sc->tulip_probe_timeout = 0;
	    sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	}
    }

    /*
     * Ignore txprobe failures or spurious callbacks.
     */
    if (event == TULIP_MEDIAPOLL_TXPROBE_FAILED
	    && sc->tulip_probe_state != TULIP_PROBE_MEDIATEST) {
	sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
	return;
    }

    /*
     * If we really transmitted a packet, then that's the media we'll use.
     */
    if (event == TULIP_MEDIAPOLL_TXPROBE_OK || event == TULIP_MEDIAPOLL_LINKPASS) {
	if (event == TULIP_MEDIAPOLL_LINKPASS)
	    sc->tulip_probe_media = TULIP_MEDIA_10BASET;
#if defined(TULIP_DEBUG)
	else
	    sc->tulip_dbg.dbg_txprobes_ok[sc->tulip_probe_media]++;
#endif
	tulip_linkup(sc, sc->tulip_probe_media);
	tulip_timeout(sc);
	return;
    }

    if (sc->tulip_probe_state == TULIP_PROBE_GPRTEST) {
#if defined(TULIP_DO_GPR_SENSE)
	/*
	 * Check for media via the general purpose register.
	 *
	 * Try to sense the media via the GPR.  If the same value
	 * occurs 3 times in a row then just use that.
	 */
	if (sc->tulip_probe_timeout > 0) {
	    tulip_media_t new_probe_media = tulip_21140_gpr_media_sense(sc);
#if defined(TULIP_DEBUG)
	    printf(TULIP_PRINTF_FMT ": media_poll: gpr sensing = %s\n",
		   TULIP_PRINTF_ARGS, tulip_mediums[new_probe_media]);
#endif
	    if (new_probe_media != TULIP_MEDIA_UNKNOWN) {
		if (new_probe_media == sc->tulip_probe_media) {
		    if (--sc->tulip_probe_count == 0)
			tulip_linkup(sc, sc->tulip_probe_media);
		} else {
		    sc->tulip_probe_count = 10;
		}
	    }
	    sc->tulip_probe_media = new_probe_media;
	    tulip_timeout(sc);
	    return;
	}
#endif /* TULIP_DO_GPR_SENSE */
	/*
	 * Brute force.  We cycle through each of the media types
	 * and try to transmit a packet.
	 */
	sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	sc->tulip_probe_media = TULIP_MEDIA_MAX;
	sc->tulip_probe_timeout = 0;
	tulip_timeout(sc);
	return;
    }

    if (sc->tulip_probe_state != TULIP_PROBE_MEDIATEST
	   && (sc->tulip_features & TULIP_HAVE_MII)) {
	tulip_media_t old_media = sc->tulip_probe_media;
	tulip_mii_autonegotiate(sc, sc->tulip_phyaddr);
	switch (sc->tulip_probe_state) {
	    case TULIP_PROBE_FAILED:
	    case TULIP_PROBE_MEDIATEST: {
		/*
		 * Try the next media.
		 */
		sc->tulip_probe_mediamask |= sc->tulip_mediums[sc->tulip_probe_media]->mi_mediamask;
		sc->tulip_probe_timeout = 0;
#ifdef notyet
		if (sc->tulip_probe_state == TULIP_PROBE_FAILED)
		    break;
		if (sc->tulip_probe_media != tulip_mii_phy_readspecific(sc))
		    break;
		sc->tulip_probe_timeout = TULIP_IS_MEDIA_TP(sc->tulip_probe_media) ? 2500 : 300;
#endif
		break;
	    }
	    case TULIP_PROBE_PHYAUTONEG: {
		return;
	    }
	    case TULIP_PROBE_INACTIVE: {
		/*
		 * Only probe if we autonegotiated a media that hasn't failed.
		 */
		sc->tulip_probe_timeout = 0;
		if (sc->tulip_probe_mediamask & TULIP_BIT(sc->tulip_probe_media)) {
		    sc->tulip_probe_media = old_media;
		    break;
		}
		tulip_linkup(sc, sc->tulip_probe_media);
		tulip_timeout(sc);
		return;
	    }
	    default: {
#if defined(DIAGNOSTIC) || defined(TULIP_DEBUG)
		panic("tulip_media_poll: botch at line %d", __LINE__);
#endif
		break;
	    }
	}
    }

    if (event == TULIP_MEDIAPOLL_TXPROBE_FAILED) {
#if defined(TULIP_DEBUG)
	sc->tulip_dbg.dbg_txprobes_failed[sc->tulip_probe_media]++;
#endif
	sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
	return;
    }

    /*
     * switch to another media if we tried this one enough.
     */
    if (/* event == TULIP_MEDIAPOLL_TXPROBE_FAILED || */ sc->tulip_probe_timeout <= 0) {
#if defined(TULIP_DEBUG)
	if (sc->tulip_probe_media == TULIP_MEDIA_UNKNOWN) {
	    printf(TULIP_PRINTF_FMT ": poll media unknown!\n",
		   TULIP_PRINTF_ARGS);
	    sc->tulip_probe_media = TULIP_MEDIA_MAX;
	}
#endif
	/*
	 * Find the next media type to check for.  Full Duplex
	 * types are not allowed.
	 */
	do {
	    sc->tulip_probe_media -= 1;
	    if (sc->tulip_probe_media == TULIP_MEDIA_UNKNOWN) {
		if (++sc->tulip_probe_passes == 3) {
		    printf(TULIP_PRINTF_FMT ": autosense failed: cable problem?\n",
			   TULIP_PRINTF_ARGS);
		    if ((sc->tulip_if.if_flags & IFF_UP) == 0) {
			sc->tulip_if.if_flags &= ~IFF_RUNNING;
			sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
			return;
		    }
		}
		sc->tulip_flags ^= TULIP_TRYNWAY;	/* XXX */
		sc->tulip_probe_mediamask = 0;
		sc->tulip_probe_media = TULIP_MEDIA_MAX - 1;
	    }
	} while (sc->tulip_mediums[sc->tulip_probe_media] == NULL
		 || (sc->tulip_probe_mediamask & TULIP_BIT(sc->tulip_probe_media))
		 || TULIP_IS_MEDIA_FD(sc->tulip_probe_media));

#if defined(TULIP_DEBUG)
	printf(TULIP_PRINTF_FMT ": %s: probing %s\n", TULIP_PRINTF_ARGS,
	       event == TULIP_MEDIAPOLL_TXPROBE_FAILED ? "txprobe failed" : "timeout",
	       tulip_mediums[sc->tulip_probe_media]);
#endif
	sc->tulip_probe_timeout = TULIP_IS_MEDIA_TP(sc->tulip_probe_media) ? 2500 : 1000;
	sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	sc->tulip_probe.probe_txprobes = 0;
	tulip_reset(sc);
	tulip_media_set(sc, sc->tulip_probe_media);
	sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
    }
    tulip_timeout(sc);

    /*
     * If this is hanging off a phy, we know are doing NWAY and we have
     * forced the phy to a specific speed.  Wait for link up before
     * before sending a packet.
     */
    switch (sc->tulip_mediums[sc->tulip_probe_media]->mi_type) {
	case TULIP_MEDIAINFO_MII: {
	    if (sc->tulip_probe_media != tulip_mii_phy_readspecific(sc))
		return;
	    break;
	}
	case TULIP_MEDIAINFO_SIA: {
	    if (TULIP_IS_MEDIA_TP(sc->tulip_probe_media)) {
		if (TULIP_CSR_READ(sc, csr_sia_status) & TULIP_SIASTS_LINKFAIL)
		    return;
		tulip_linkup(sc, sc->tulip_probe_media);
#ifdef notyet
		if (sc->tulip_features & TULIP_HAVE_MII)
		    tulip_timeout(sc);
#endif
		return;
	    }
	    break;
	}
	case TULIP_MEDIAINFO_RESET:
	case TULIP_MEDIAINFO_SYM:
	case TULIP_MEDIAINFO_GPR: {
	    break;
	}
    }
    /*
     * Try to send a packet.
     */
    tulip_txprobe(sc);
}

static void
tulip_media_select(
    tulip_softc_t * const sc)
{
    if (sc->tulip_features & TULIP_HAVE_GPR) {
	TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_PINSET|sc->tulip_gpinit);
	DELAY(10);
	TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_gpdata);
    }
    /*
     * If this board has no media, just return
     */
    if (IFM_SUBTYPE(sc->tulip_ifmedia.ifm_media) == IFM_NONE)
	return;
    if (sc->tulip_media == TULIP_MEDIA_UNKNOWN) {
	TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	(*sc->tulip_boardsw->bd_media_poll)(sc, TULIP_MEDIAPOLL_START);
    } else {
	tulip_media_set(sc, sc->tulip_media);
    }
}

static void
tulip_21040_mediainfo_init(
    tulip_softc_t * const sc,
    tulip_media_t media)
{
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT|TULIP_CMD_THRSHLD160
	|TULIP_CMD_BACKOFFCTR;
    sc->tulip_if.if_baudrate = 10000000;

    if (media == TULIP_MEDIA_10BASET || media == TULIP_MEDIA_UNKNOWN) {
	TULIP_MEDIAINFO_SIA_INIT(sc, &sc->tulip_mediainfo[0], 21040, 10BASET);
	TULIP_MEDIAINFO_SIA_INIT(sc, &sc->tulip_mediainfo[1], 21040, 10BASET_FD);
    }

    if (media == TULIP_MEDIA_AUIBNC || media == TULIP_MEDIA_UNKNOWN) {
	TULIP_MEDIAINFO_SIA_INIT(sc, &sc->tulip_mediainfo[2], 21040, AUIBNC);
    }

    if (media == TULIP_MEDIA_UNKNOWN) {
	TULIP_MEDIAINFO_SIA_INIT(sc, &sc->tulip_mediainfo[3], 21040, EXTSIA);
    }
}

static void
tulip_21040_media_probe(
    tulip_softc_t * const sc)
{
    tulip_21040_mediainfo_init(sc, TULIP_MEDIA_UNKNOWN);
    return;
}

static void
tulip_21040_10baset_only_media_probe(
    tulip_softc_t * const sc)
{
    tulip_21040_mediainfo_init(sc, TULIP_MEDIA_10BASET);
    tulip_media_set(sc, TULIP_MEDIA_10BASET);
    sc->tulip_media = TULIP_MEDIA_10BASET;
}

static void
tulip_21040_10baset_only_media_select(
    tulip_softc_t * const sc)
{
    sc->tulip_flags |= TULIP_LINKUP;
    if (sc->tulip_media == TULIP_MEDIA_10BASET_FD) {
	sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
	sc->tulip_flags &= ~TULIP_SQETEST;
    } else {
	sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	sc->tulip_flags |= TULIP_SQETEST;
    }
    tulip_media_set(sc, sc->tulip_media);
}

static void
tulip_21040_auibnc_only_media_probe(
    tulip_softc_t * const sc)
{
    tulip_21040_mediainfo_init(sc, TULIP_MEDIA_AUIBNC);
    sc->tulip_flags |= TULIP_SQETEST|TULIP_LINKUP;
    tulip_media_set(sc, TULIP_MEDIA_AUIBNC);
    sc->tulip_media = TULIP_MEDIA_AUIBNC;
}

static void
tulip_21040_auibnc_only_media_select(
    tulip_softc_t * const sc)
{
    tulip_media_set(sc, TULIP_MEDIA_AUIBNC);
    sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
}

static const tulip_boardsw_t tulip_21040_boardsw = {
    TULIP_21040_GENERIC,
    tulip_21040_media_probe,
    tulip_media_select,
    tulip_media_poll,
};

static const tulip_boardsw_t tulip_21040_10baset_only_boardsw = {
    TULIP_21040_GENERIC,
    tulip_21040_10baset_only_media_probe,
    tulip_21040_10baset_only_media_select,
    NULL,
};

static const tulip_boardsw_t tulip_21040_auibnc_only_boardsw = {
    TULIP_21040_GENERIC,
    tulip_21040_auibnc_only_media_probe,
    tulip_21040_auibnc_only_media_select,
    NULL,
};

static void
tulip_21041_mediainfo_init(
    tulip_softc_t * const sc)
{
    tulip_media_info_t * const mi = sc->tulip_mediainfo;

#ifdef notyet
    if (sc->tulip_revinfo >= 0x20) {
	TULIP_MEDIAINFO_SIA_INIT(sc, &mi[0], 21041P2, 10BASET);
	TULIP_MEDIAINFO_SIA_INIT(sc, &mi[1], 21041P2, 10BASET_FD);
	TULIP_MEDIAINFO_SIA_INIT(sc, &mi[0], 21041P2, AUI);
	TULIP_MEDIAINFO_SIA_INIT(sc, &mi[1], 21041P2, BNC);
	return;
    }
#endif
    TULIP_MEDIAINFO_SIA_INIT(sc, &mi[0], 21041, 10BASET);
    TULIP_MEDIAINFO_SIA_INIT(sc, &mi[1], 21041, 10BASET_FD);
    TULIP_MEDIAINFO_SIA_INIT(sc, &mi[2], 21041, AUI);
    TULIP_MEDIAINFO_SIA_INIT(sc, &mi[3], 21041, BNC);
}

static void
tulip_21041_media_probe(
    tulip_softc_t * const sc)
{
    sc->tulip_if.if_baudrate = 10000000;
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT|TULIP_CMD_ENHCAPTEFFCT
	|TULIP_CMD_THRSHLD160|TULIP_CMD_BACKOFFCTR;
    sc->tulip_intrmask |= TULIP_STS_LINKPASS;
    tulip_21041_mediainfo_init(sc);
}

static void
tulip_21041_media_poll(
    tulip_softc_t * const sc,
    const tulip_mediapoll_event_t event)
{
    u_int32_t sia_status;

#if defined(TULIP_DEBUG)
    sc->tulip_dbg.dbg_events[event]++;
#endif

    if (event == TULIP_MEDIAPOLL_LINKFAIL) {
	if (sc->tulip_probe_state != TULIP_PROBE_INACTIVE
		|| !TULIP_DO_AUTOSENSE(sc))
	    return;
	sc->tulip_media = TULIP_MEDIA_UNKNOWN;
	tulip_reset(sc);	/* start probe */
	return;
    }

    /*
     * If we've been been asked to start a poll or link change interrupt
     * restart the probe (and reset the tulip to a known state).
     */
    if (event == TULIP_MEDIAPOLL_START) {
	sc->tulip_if.if_flags |= IFF_OACTIVE;
	sc->tulip_cmdmode &= ~(TULIP_CMD_FULLDUPLEX|TULIP_CMD_RXRUN);
#ifdef notyet
	if (sc->tulip_revinfo >= 0x20) {
	    sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
	    sc->tulip_flags |= TULIP_DIDNWAY;
	}
#endif
	TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
	sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	sc->tulip_probe_media = TULIP_MEDIA_10BASET;
	sc->tulip_probe_timeout = TULIP_21041_PROBE_10BASET_TIMEOUT;
	tulip_media_set(sc, TULIP_MEDIA_10BASET);
	tulip_timeout(sc);
	return;
    }

    if (sc->tulip_probe_state == TULIP_PROBE_INACTIVE)
	return;

    if (event == TULIP_MEDIAPOLL_TXPROBE_OK) {
#if defined(TULIP_DEBUG)
	sc->tulip_dbg.dbg_txprobes_ok[sc->tulip_probe_media]++;
#endif
	tulip_linkup(sc, sc->tulip_probe_media);
	return;
    }

    sia_status = TULIP_CSR_READ(sc, csr_sia_status);
    TULIP_CSR_WRITE(sc, csr_sia_status, sia_status);
    if ((sia_status & TULIP_SIASTS_LINKFAIL) == 0) {
	if (sc->tulip_revinfo >= 0x20) {
	    if (sia_status & (PHYSTS_10BASET_FD << (16 - 6)))
		sc->tulip_probe_media = TULIP_MEDIA_10BASET_FD;
	}
	/*
	 * If the link has passed LinkPass, 10baseT is the
	 * proper media to use.
	 */
	tulip_linkup(sc, sc->tulip_probe_media);
	return;
    }

    /*
     * wait for up to 2.4 seconds for the link to reach pass state.
     * Only then start scanning the other media for activity.
     * choose media with receive activity over those without.
     */
    if (sc->tulip_probe_media == TULIP_MEDIA_10BASET) {
	if (event != TULIP_MEDIAPOLL_TIMER)
	    return;
	if (sc->tulip_probe_timeout > 0
		&& (sia_status & TULIP_SIASTS_OTHERRXACTIVITY) == 0) {
	    tulip_timeout(sc);
	    return;
	}
	sc->tulip_probe_timeout = TULIP_21041_PROBE_AUIBNC_TIMEOUT;
	sc->tulip_flags |= TULIP_WANTRXACT;
	if (sia_status & TULIP_SIASTS_OTHERRXACTIVITY) {
	    sc->tulip_probe_media = TULIP_MEDIA_BNC;
	} else {
	    sc->tulip_probe_media = TULIP_MEDIA_AUI;
	}
	tulip_media_set(sc, sc->tulip_probe_media);
	tulip_timeout(sc);
	return;
    }

    /*
     * If we failed, clear the txprobe active flag.
     */
    if (event == TULIP_MEDIAPOLL_TXPROBE_FAILED)
	sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;


    if (event == TULIP_MEDIAPOLL_TIMER) {
	/*
	 * If we've received something, then that's our link!
	 */
	if (sc->tulip_flags & TULIP_RXACT) {
	    tulip_linkup(sc, sc->tulip_probe_media);
	    return;
	}
	/*
	 * if no txprobe active  
	 */
	if ((sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0
		&& ((sc->tulip_flags & TULIP_WANTRXACT) == 0
		    || (sia_status & TULIP_SIASTS_RXACTIVITY))) {
	    sc->tulip_probe_timeout = TULIP_21041_PROBE_AUIBNC_TIMEOUT;
	    tulip_txprobe(sc);
	    tulip_timeout(sc);
	    return;
	}
	/*
	 * Take 2 passes through before deciding to not
	 * wait for receive activity.  Then take another
	 * two passes before spitting out a warning.
	 */
	if (sc->tulip_probe_timeout <= 0) {
	    if (sc->tulip_flags & TULIP_WANTRXACT) {
		sc->tulip_flags &= ~TULIP_WANTRXACT;
		sc->tulip_probe_timeout = TULIP_21041_PROBE_AUIBNC_TIMEOUT;
	    } else {
		printf(TULIP_PRINTF_FMT ": autosense failed: cable problem?\n",
		       TULIP_PRINTF_ARGS);
		if ((sc->tulip_if.if_flags & IFF_UP) == 0) {
		    sc->tulip_if.if_flags &= ~IFF_RUNNING;
		    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
		    return;
		}
	    }
	}
    }
    
    /*
     * Since this media failed to probe, try the other one.
     */
    sc->tulip_probe_timeout = TULIP_21041_PROBE_AUIBNC_TIMEOUT;
    if (sc->tulip_probe_media == TULIP_MEDIA_AUI) {
	sc->tulip_probe_media = TULIP_MEDIA_BNC;
    } else {
	sc->tulip_probe_media = TULIP_MEDIA_AUI;
    }
    tulip_media_set(sc, sc->tulip_probe_media);
    sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
    tulip_timeout(sc);
}

static const tulip_boardsw_t tulip_21041_boardsw = {
    TULIP_21041_GENERIC,
    tulip_21041_media_probe,
    tulip_media_select,
    tulip_21041_media_poll
};

static const tulip_phy_attr_t tulip_mii_phy_attrlist[] = {
    { 0x20005c00, 0,		/* 08-00-17 */
      {
	{ 0x19, 0x0040, 0x0040 },	/* 10TX */
	{ 0x19, 0x0040, 0x0000 },	/* 100TX */
      },
#if defined(TULIP_DEBUG)
      "NS DP83840",
#endif
    },
    { 0x0281F400, 0,		/* 00-A0-7D */
      {
	{ 0x12, 0x0010, 0x0000 },	/* 10T */
	{ },				/* 100TX */
	{ 0x12, 0x0010, 0x0010 },	/* 100T4 */
	{ 0x12, 0x0008, 0x0008 },	/* FULL_DUPLEX */
      },
#if defined(TULIP_DEBUG)
      "Seeq 80C240"
#endif
    },
#if 0
    { 0x0015F420, 0,	/* 00-A0-7D */
      {
	{ 0x12, 0x0010, 0x0000 },	/* 10T */
	{ },				/* 100TX */
	{ 0x12, 0x0010, 0x0010 },	/* 100T4 */
	{ 0x12, 0x0008, 0x0008 },	/* FULL_DUPLEX */
      },
#if defined(TULIP_DEBUG)
      "Broadcom BCM5000"
#endif
    },
#endif
    { 0x0281F400, 0,		/* 00-A0-BE */
      {
	{ 0x11, 0x8000, 0x0000 },	/* 10T */
	{ 0x11, 0x8000, 0x8000 },	/* 100TX */
	{ },				/* 100T4 */
	{ 0x11, 0x4000, 0x4000 },	/* FULL_DUPLEX */
      },
#if defined(TULIP_DEBUG)
      "ICS 1890"
#endif 
    },
    { 0 }
};

static tulip_media_t
tulip_mii_phy_readspecific(
    tulip_softc_t * const sc)
{
    const tulip_phy_attr_t *attr;
    u_int16_t data;
    u_int32_t id;
    unsigned idx = 0;
    static const tulip_media_t table[] = {
	TULIP_MEDIA_UNKNOWN,
	TULIP_MEDIA_10BASET,
	TULIP_MEDIA_100BASETX,
	TULIP_MEDIA_100BASET4,
	TULIP_MEDIA_UNKNOWN,
	TULIP_MEDIA_10BASET_FD,
	TULIP_MEDIA_100BASETX_FD,
	TULIP_MEDIA_UNKNOWN
    };

    /*
     * Don't read phy specific registers if link is not up.
     */
    data = tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_STATUS);
    if ((data & (PHYSTS_LINK_UP|PHYSTS_EXTENDED_REGS)) != (PHYSTS_LINK_UP|PHYSTS_EXTENDED_REGS))
	return TULIP_MEDIA_UNKNOWN;

    id = (tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_IDLOW) << 16) |
	tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_IDHIGH);
    for (attr = tulip_mii_phy_attrlist;; attr++) {
	if (attr->attr_id == 0)
	    return TULIP_MEDIA_UNKNOWN;
	if ((id & ~0x0F) == attr->attr_id)
	    break;
    }

    if (attr->attr_modes[PHY_MODE_100TX].pm_regno) {
	const tulip_phy_modedata_t * const pm = &attr->attr_modes[PHY_MODE_100TX];
	data = tulip_mii_readreg(sc, sc->tulip_phyaddr, pm->pm_regno);
	if ((data & pm->pm_mask) == pm->pm_value)
	    idx = 2;
    }
    if (idx == 0 && attr->attr_modes[PHY_MODE_100T4].pm_regno) {
	const tulip_phy_modedata_t * const pm = &attr->attr_modes[PHY_MODE_100T4];
	data = tulip_mii_readreg(sc, sc->tulip_phyaddr, pm->pm_regno);
	if ((data & pm->pm_mask) == pm->pm_value)
	    idx = 3;
    }
    if (idx == 0 && attr->attr_modes[PHY_MODE_10T].pm_regno) {
	const tulip_phy_modedata_t * const pm = &attr->attr_modes[PHY_MODE_10T];
	data = tulip_mii_readreg(sc, sc->tulip_phyaddr, pm->pm_regno);
	if ((data & pm->pm_mask) == pm->pm_value)
	    idx = 1;
    } 
    if (idx != 0 && attr->attr_modes[PHY_MODE_FULLDUPLEX].pm_regno) {
	const tulip_phy_modedata_t * const pm = &attr->attr_modes[PHY_MODE_FULLDUPLEX];
	data = tulip_mii_readreg(sc, sc->tulip_phyaddr, pm->pm_regno);
	idx += ((data & pm->pm_mask) == pm->pm_value ? 4 : 0);
    }
    return table[idx];
}

static unsigned
tulip_mii_get_phyaddr(
    tulip_softc_t * const sc,
    unsigned offset)
{
    unsigned phyaddr;

    for (phyaddr = 1; phyaddr < 32; phyaddr++) {
	unsigned status = tulip_mii_readreg(sc, phyaddr, PHYREG_STATUS);
	if (status == 0 || status == 0xFFFF || status < PHYSTS_10BASET)
	    continue;
	if (offset == 0)
	    return phyaddr;
	offset--;
    }
    if (offset == 0) {
	unsigned status = tulip_mii_readreg(sc, 0, PHYREG_STATUS);
	if (status == 0 || status == 0xFFFF || status < PHYSTS_10BASET)
	    return TULIP_MII_NOPHY;
	return 0;
    }
    return TULIP_MII_NOPHY;
}

static int
tulip_mii_map_abilities(
    tulip_softc_t * const sc,
    unsigned abilities)
{
    sc->tulip_abilities = abilities;
    if (abilities & PHYSTS_100BASETX_FD) {
	sc->tulip_probe_media = TULIP_MEDIA_100BASETX_FD;
    } else if (abilities & PHYSTS_100BASET4) {
	sc->tulip_probe_media = TULIP_MEDIA_100BASET4;
    } else if (abilities & PHYSTS_100BASETX) {
	sc->tulip_probe_media = TULIP_MEDIA_100BASETX;
    } else if (abilities & PHYSTS_10BASET_FD) {
	sc->tulip_probe_media = TULIP_MEDIA_10BASET_FD;
    } else if (abilities & PHYSTS_10BASET) {
	sc->tulip_probe_media = TULIP_MEDIA_10BASET;
    } else {
	sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	return 0;
    }
    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
    return 1;
}

static void
tulip_mii_autonegotiate(
    tulip_softc_t * const sc,
    const unsigned phyaddr)
{
    switch (sc->tulip_probe_state) {
        case TULIP_PROBE_MEDIATEST:
        case TULIP_PROBE_INACTIVE: {
	    sc->tulip_flags |= TULIP_DIDNWAY;
	    tulip_mii_writereg(sc, phyaddr, PHYREG_CONTROL, PHYCTL_RESET);
	    sc->tulip_probe_timeout = 3000;
	    sc->tulip_intrmask |= TULIP_STS_ABNRMLINTR|TULIP_STS_NORMALINTR;
	    sc->tulip_probe_state = TULIP_PROBE_PHYRESET;
	    /* FALL THROUGH */
	}
        case TULIP_PROBE_PHYRESET: {
	    u_int32_t status;
	    u_int32_t data = tulip_mii_readreg(sc, phyaddr, PHYREG_CONTROL);
	    if (data & PHYCTL_RESET) {
		if (sc->tulip_probe_timeout > 0) {
		    tulip_timeout(sc);
		    return;
		}
		printf(TULIP_PRINTF_FMT "(phy%d): error: reset of PHY never completed!\n",
			   TULIP_PRINTF_ARGS, phyaddr);
		sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
		sc->tulip_probe_state = TULIP_PROBE_FAILED;
		sc->tulip_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
		return;
	    }
	    status = tulip_mii_readreg(sc, phyaddr, PHYREG_STATUS);
	    if ((status & PHYSTS_CAN_AUTONEG) == 0) {
#if defined(TULIP_DEBUG)
		loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation disabled\n",
			   TULIP_PRINTF_ARGS, phyaddr);
#endif
		sc->tulip_flags &= ~TULIP_DIDNWAY;
		sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
		return;
	    }
	    if (tulip_mii_readreg(sc, phyaddr, PHYREG_AUTONEG_ADVERTISEMENT) != ((status >> 6) | 0x01))
		tulip_mii_writereg(sc, phyaddr, PHYREG_AUTONEG_ADVERTISEMENT, (status >> 6) | 0x01);
	    tulip_mii_writereg(sc, phyaddr, PHYREG_CONTROL, data|PHYCTL_AUTONEG_RESTART|PHYCTL_AUTONEG_ENABLE);
	    data = tulip_mii_readreg(sc, phyaddr, PHYREG_CONTROL);
#if defined(TULIP_DEBUG)
	    if ((data & PHYCTL_AUTONEG_ENABLE) == 0)
		loudprintf(TULIP_PRINTF_FMT "(phy%d): oops: enable autonegotiation failed: 0x%04x\n",
			   TULIP_PRINTF_ARGS, phyaddr, data);
	    else
		loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation restarted: 0x%04x\n",
			   TULIP_PRINTF_ARGS, phyaddr, data);
	    sc->tulip_dbg.dbg_nway_starts++;
#endif
	    sc->tulip_probe_state = TULIP_PROBE_PHYAUTONEG;
	    sc->tulip_probe_timeout = 3000;
	    /* FALL THROUGH */
	}
        case TULIP_PROBE_PHYAUTONEG: {
	    u_int32_t status = tulip_mii_readreg(sc, phyaddr, PHYREG_STATUS);
	    u_int32_t data;
	    if ((status & PHYSTS_AUTONEG_DONE) == 0) {
		if (sc->tulip_probe_timeout > 0) {
		    tulip_timeout(sc);
		    return;
		}
#if defined(TULIP_DEBUG)
		loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation timeout: sts=0x%04x, ctl=0x%04x\n",
			   TULIP_PRINTF_ARGS, phyaddr, status,
			   tulip_mii_readreg(sc, phyaddr, PHYREG_CONTROL));
#endif
		sc->tulip_flags &= ~TULIP_DIDNWAY;
		sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
		return;
	    }
	    data = tulip_mii_readreg(sc, phyaddr, PHYREG_AUTONEG_ABILITIES);
#if defined(TULIP_DEBUG)
	    loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation complete: 0x%04x\n",
		       TULIP_PRINTF_ARGS, phyaddr, data);
#endif
	    data = (data << 6) & status;
	    if (!tulip_mii_map_abilities(sc, data))
		sc->tulip_flags &= ~TULIP_DIDNWAY;
	    return;
	}
	default: {
#if defined(DIAGNOSTIC)
	    panic("tulip_media_poll: botch at line %d", __LINE__);
#endif
	    break;
	}
    }
#if defined(TULIP_DEBUG)
    loudprintf(TULIP_PRINTF_FMT "(phy%d): autonegotiation failure: state = %d\n",
	       TULIP_PRINTF_ARGS, phyaddr, sc->tulip_probe_state);
	    sc->tulip_dbg.dbg_nway_failures++;
#endif
}

static void
tulip_2114x_media_preset(
    tulip_softc_t * const sc)
{
    const tulip_media_info_t *mi = NULL;
    tulip_media_t media = sc->tulip_media;

    if (sc->tulip_probe_state == TULIP_PROBE_INACTIVE)
	media = sc->tulip_media;
    else
	media = sc->tulip_probe_media;
    
    sc->tulip_cmdmode &= ~TULIP_CMD_PORTSELECT;
    sc->tulip_flags &= ~TULIP_SQETEST;
    /* here is the srom media problem */
    if (media != TULIP_MEDIA_UNKNOWN) {
#if 1 || defined(TULIP_DEBUG)
	if (media < TULIP_MEDIA_MAX && sc->tulip_mediums[media] != NULL) {
#endif
	    mi = sc->tulip_mediums[media];
	    if (mi->mi_type == TULIP_MEDIAINFO_MII) {
		sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT;
	    } else if (mi->mi_type == TULIP_MEDIAINFO_GPR
		       || mi->mi_type == TULIP_MEDIAINFO_SYM) {
		sc->tulip_cmdmode &= ~TULIP_GPR_CMDBITS;
		sc->tulip_cmdmode |= mi->mi_cmdmode;
	    } else if (mi->mi_type == TULIP_MEDIAINFO_SIA) {
		TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
	    }
#if 1 || defined(TULIP_DEBUG)
	} else {
	    printf(TULIP_PRINTF_FMT ": preset: bad media %d!\n",
		   TULIP_PRINTF_ARGS, media);
	}
#endif
    }
    switch (media) {
	case TULIP_MEDIA_BNC:
	case TULIP_MEDIA_AUI:
	case TULIP_MEDIA_10BASET: {
	    sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	    sc->tulip_cmdmode |= TULIP_CMD_TXTHRSHLDCTL;
	    sc->tulip_if.if_baudrate = 10000000;
	    sc->tulip_flags |= TULIP_SQETEST;
	    break;
	}
	case TULIP_MEDIA_10BASET_FD: {
	    sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX|TULIP_CMD_TXTHRSHLDCTL;
	    sc->tulip_if.if_baudrate = 10000000;
	    break;
	}
	case TULIP_MEDIA_100BASEFX:
	case TULIP_MEDIA_100BASET4:
	case TULIP_MEDIA_100BASETX: {
	    sc->tulip_cmdmode &= ~(TULIP_CMD_FULLDUPLEX|TULIP_CMD_TXTHRSHLDCTL);
	    sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT;
	    sc->tulip_if.if_baudrate = 100000000;
	    break;
	}
	case TULIP_MEDIA_100BASEFX_FD:
	case TULIP_MEDIA_100BASETX_FD: {
	    sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX|TULIP_CMD_PORTSELECT;
	    sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
	    sc->tulip_if.if_baudrate = 100000000;
	    break;
	}
	default: {
	    break;
	}
    }
    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
}

/*
 ********************************************************************
 *  Start of 21140/21140A support which does not use the MII interface 
 */

static void
tulip_null_media_poll(
    tulip_softc_t * const sc,
    tulip_mediapoll_event_t event)
{
#if defined(TULIP_DEBUG)
    sc->tulip_dbg.dbg_events[event]++;
#endif
#if defined(DIAGNOSTIC)
    printf(TULIP_PRINTF_FMT ": botch(media_poll) at line %d\n",
	   TULIP_PRINTF_ARGS, __LINE__);
#endif
}

inline static void
tulip_21140_mediainit(
    tulip_softc_t * const sc,
    tulip_media_info_t * const mip,
    tulip_media_t const media,
    unsigned gpdata,
    unsigned cmdmode)
{
    sc->tulip_mediums[media] = mip;
    mip->mi_type = TULIP_MEDIAINFO_GPR;
    mip->mi_cmdmode = cmdmode;
    mip->mi_gpdata = gpdata;
}

static void
tulip_21140_evalboard_media_probe(
    tulip_softc_t * const sc)
{
    tulip_media_info_t *mip = sc->tulip_mediainfo;

    sc->tulip_gpinit = TULIP_GP_EB_PINS;
    sc->tulip_gpdata = TULIP_GP_EB_INIT;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EB_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EB_INIT);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    DELAY(1000000);
    if ((TULIP_CSR_READ(sc, csr_gp) & TULIP_GP_EB_OK100) != 0) {
	sc->tulip_media = TULIP_MEDIA_10BASET;
    } else {
	sc->tulip_media = TULIP_MEDIA_100BASETX;
    }
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET,
			  TULIP_GP_EB_INIT,
			  TULIP_CMD_TXTHRSHLDCTL);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET_FD,
			  TULIP_GP_EB_INIT,
			  TULIP_CMD_TXTHRSHLDCTL|TULIP_CMD_FULLDUPLEX);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX,
			  TULIP_GP_EB_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX_FD,
			  TULIP_GP_EB_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER|TULIP_CMD_FULLDUPLEX);
}

static const tulip_boardsw_t tulip_21140_eb_boardsw = {
    TULIP_21140_DEC_EB,
    tulip_21140_evalboard_media_probe,
    tulip_media_select,
    tulip_null_media_poll,
    tulip_2114x_media_preset,
};

static void
tulip_21140_smc9332_media_probe(
    tulip_softc_t * const sc)
{
    tulip_media_info_t *mip = sc->tulip_mediainfo;
    int idx, cnt = 0;

    TULIP_CSR_WRITE(sc, csr_command, TULIP_CMD_PORTSELECT|TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
		   33MHz that comes to two microseconds but wait a
		   bit longer anyways) */
    TULIP_CSR_WRITE(sc, csr_command, TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    sc->tulip_gpinit = TULIP_GP_SMC_9332_PINS;
    sc->tulip_gpdata = TULIP_GP_SMC_9332_INIT;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_SMC_9332_PINS|TULIP_GP_PINSET);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_SMC_9332_INIT);
    DELAY(200000);
    for (idx = 1000; idx > 0; idx--) {
	u_int32_t csr = TULIP_CSR_READ(sc, csr_gp);
	if ((csr & (TULIP_GP_SMC_9332_OK10|TULIP_GP_SMC_9332_OK100)) == (TULIP_GP_SMC_9332_OK10|TULIP_GP_SMC_9332_OK100)) {
	    if (++cnt > 100)
		break;
	} else if ((csr & TULIP_GP_SMC_9332_OK10) == 0) {
	    break;
	} else {
	    cnt = 0;
	}
	DELAY(1000);
    }
    sc->tulip_media = cnt > 100 ? TULIP_MEDIA_100BASETX : TULIP_MEDIA_10BASET;
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX,
			  TULIP_GP_SMC_9332_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX_FD,
			  TULIP_GP_SMC_9332_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER|TULIP_CMD_FULLDUPLEX);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET,
			  TULIP_GP_SMC_9332_INIT,
			  TULIP_CMD_TXTHRSHLDCTL);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET_FD,
			  TULIP_GP_SMC_9332_INIT,
			  TULIP_CMD_TXTHRSHLDCTL|TULIP_CMD_FULLDUPLEX);
}
 
static const tulip_boardsw_t tulip_21140_smc9332_boardsw = {
    TULIP_21140_SMC_9332,
    tulip_21140_smc9332_media_probe,
    tulip_media_select,
    tulip_null_media_poll,
    tulip_2114x_media_preset,
};

static void
tulip_21140_cogent_em100_media_probe(
    tulip_softc_t * const sc)
{
    tulip_media_info_t *mip = sc->tulip_mediainfo;

    sc->tulip_gpinit = TULIP_GP_EM100_PINS;
    sc->tulip_gpdata = TULIP_GP_EM100_INIT;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EM100_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EM100_INIT);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    sc->tulip_media = TULIP_MEDIA_100BASETX;

    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX,
			  TULIP_GP_EM100_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX_FD,
			  TULIP_GP_EM100_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER|TULIP_CMD_FULLDUPLEX);
}

static const tulip_boardsw_t tulip_21140_cogent_em100_boardsw = {
    TULIP_21140_COGENT_EM100,
    tulip_21140_cogent_em100_media_probe,
    tulip_media_select,
    tulip_null_media_poll,
    tulip_2114x_media_preset
};

static void
tulip_21140_znyx_zx34x_media_probe(
    tulip_softc_t * const sc)
{
    tulip_media_info_t *mip = sc->tulip_mediainfo;
    int cnt10 = 0, cnt100 = 0, idx;

    sc->tulip_gpinit = TULIP_GP_ZX34X_PINS;
    sc->tulip_gpdata = TULIP_GP_ZX34X_INIT;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ZX34X_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ZX34X_INIT);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);

    DELAY(200000);
    for (idx = 1000; idx > 0; idx--) {
	u_int32_t csr = TULIP_CSR_READ(sc, csr_gp);
	if ((csr & (TULIP_GP_ZX34X_LNKFAIL|TULIP_GP_ZX34X_SYMDET|TULIP_GP_ZX34X_SIGDET)) == (TULIP_GP_ZX34X_LNKFAIL|TULIP_GP_ZX34X_SYMDET|TULIP_GP_ZX34X_SIGDET)) {
	    if (++cnt100 > 100)
		break;
	} else if ((csr & TULIP_GP_ZX34X_LNKFAIL) == 0) {
	    if (++cnt10 > 100)
		break;
	} else {
	    cnt10 = 0;
	    cnt100 = 0;
	}
	DELAY(1000);
    }
    sc->tulip_media = cnt100 > 100 ? TULIP_MEDIA_100BASETX : TULIP_MEDIA_10BASET;
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET,
			  TULIP_GP_ZX34X_INIT,
			  TULIP_CMD_TXTHRSHLDCTL);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET_FD,
			  TULIP_GP_ZX34X_INIT,
			  TULIP_CMD_TXTHRSHLDCTL|TULIP_CMD_FULLDUPLEX);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX,
			  TULIP_GP_ZX34X_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX_FD,
			  TULIP_GP_ZX34X_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER|TULIP_CMD_FULLDUPLEX);
}

static const tulip_boardsw_t tulip_21140_znyx_zx34x_boardsw = {
    TULIP_21140_ZNYX_ZX34X,
    tulip_21140_znyx_zx34x_media_probe,
    tulip_media_select,
    tulip_null_media_poll,
    tulip_2114x_media_preset,
};

static void
tulip_2114x_media_probe(
    tulip_softc_t * const sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD|TULIP_CMD_MUSTBEONE
	|TULIP_CMD_BACKOFFCTR;
}

static const tulip_boardsw_t tulip_2114x_isv_boardsw = {
    TULIP_21140_ISV,
    tulip_2114x_media_probe,
    tulip_media_select,
    tulip_media_poll,
    tulip_2114x_media_preset,
};
/* 
 * At least in some versions of the driver, all 2114x are mapped to
 * tulip_21140_eb_boardsw, so this isn't necessarily going to be
 * used. But for possible later use, provide this as a clone of
 * the dec evalboard configuration. The Avalon card should look the
 * same to the software as the eval card, with the exception of the
 * srom format in the early production units.
 */
static const tulip_boardsw_t tulip_2114x_avalon_boardsw = {
    TULIP_21140_AVALON,
    tulip_21140_evalboard_media_probe,
    tulip_media_select,
    tulip_null_media_poll,
    tulip_2114x_media_preset,
};


/*
 * ******** END of chip-specific handlers. ***********
 */

/*
 * Code the read the SROM and MII bit streams (I2C)
 */
static void
tulip_delay_300ns(
    tulip_softc_t * const sc)
{
    int idx;
    for (idx = (300 / 33) + 1; idx > 0; idx--)
	(void) TULIP_CSR_READ(sc, csr_busmode);
}

#define EMIT    do { TULIP_CSR_WRITE(sc, csr_srom_mii, csr); tulip_delay_300ns(sc); } while (0)

static void
tulip_srom_idle(
    tulip_softc_t * const sc)
{
    unsigned bit, csr;
    
    csr  = SROMSEL ; EMIT;
    csr  = SROMSEL | SROMRD; EMIT;  
    csr ^= SROMCS; EMIT;
    csr ^= SROMCLKON; EMIT;

    /*
     * Write 25 cycles of 0 which will force the SROM to be idle.
     */
    for (bit = 3 + SROM_BITWIDTH + 16; bit > 0; bit--) {
        csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
        csr ^= SROMCLKON; EMIT;     /* clock high; data valid */
    }
    csr ^= SROMCLKOFF; EMIT;
    csr ^= SROMCS; EMIT;
    csr  = 0; EMIT;
}

     
static void
tulip_srom_read(
    tulip_softc_t * const sc)
{   
    int idx; 
    const unsigned bitwidth = SROM_BITWIDTH;
    const unsigned cmdmask = (SROMCMD_RD << bitwidth);
    const unsigned msb = 1 << (bitwidth + 3 - 1);
    unsigned lastidx = (1 << bitwidth) - 1;

    tulip_srom_idle(sc);

    for (idx = 0; idx <= lastidx; idx++) {
        unsigned lastbit, data, bits, bit, csr;
	csr  = SROMSEL ;	        EMIT;
        csr  = SROMSEL | SROMRD;        EMIT;
        csr ^= SROMCSON;                EMIT;
        csr ^=            SROMCLKON;    EMIT;
    
        lastbit = 0;
        for (bits = idx|cmdmask, bit = bitwidth + 3; bit > 0; bit--, bits <<= 1) {
            const unsigned thisbit = bits & msb;
            csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
            if (thisbit != lastbit) {
                csr ^= SROMDOUT; EMIT;  /* clock low; invert data */
            } else {
		EMIT;
	    }
            csr ^= SROMCLKON; EMIT;     /* clock high; data valid */
            lastbit = thisbit;
        }
        csr ^= SROMCLKOFF; EMIT;

        for (data = 0, bits = 0; bits < 16; bits++) {
            data <<= 1;
            csr ^= SROMCLKON; EMIT;     /* clock high; data valid */ 
            data |= TULIP_CSR_READ(sc, csr_srom_mii) & SROMDIN ? 1 : 0;
            csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
        }
	sc->tulip_rombuf[idx*2] = data & 0xFF;
	sc->tulip_rombuf[idx*2+1] = data >> 8;
	csr  = SROMSEL | SROMRD; EMIT;
	csr  = 0; EMIT;
    }
    tulip_srom_idle(sc);
}

#define MII_EMIT    do { TULIP_CSR_WRITE(sc, csr_srom_mii, csr); tulip_delay_300ns(sc); } while (0)

static void
tulip_mii_writebits(
    tulip_softc_t * const sc,
    unsigned data,
    unsigned bits)
{
    unsigned msb = 1 << (bits - 1);
    unsigned csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    unsigned lastbit = (csr & MII_DOUT) ? msb : 0;

    csr |= MII_WR; MII_EMIT;  		/* clock low; assert write */

    for (; bits > 0; bits--, data <<= 1) {
	const unsigned thisbit = data & msb;
	if (thisbit != lastbit) {
	    csr ^= MII_DOUT; MII_EMIT;  /* clock low; invert data */
	}
	csr ^= MII_CLKON; MII_EMIT;     /* clock high; data valid */
	lastbit = thisbit;
	csr ^= MII_CLKOFF; MII_EMIT;    /* clock low; data not valid */
    }
}

static void
tulip_mii_turnaround(
    tulip_softc_t * const sc,
    unsigned cmd)
{
    unsigned csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);

    if (cmd == MII_WRCMD) {
	csr |= MII_DOUT; MII_EMIT;	/* clock low; change data */
	csr ^= MII_CLKON; MII_EMIT;	/* clock high; data valid */
	csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
	csr ^= MII_DOUT; MII_EMIT;	/* clock low; change data */
    } else {
	csr |= MII_RD; MII_EMIT;	/* clock low; switch to read */
    }
    csr ^= MII_CLKON; MII_EMIT;		/* clock high; data valid */
    csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
}

static unsigned
tulip_mii_readbits(
    tulip_softc_t * const sc)
{
    unsigned data;
    unsigned csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    int idx;

    for (idx = 0, data = 0; idx < 16; idx++) {
	data <<= 1;	/* this is NOOP on the first pass through */
	csr ^= MII_CLKON; MII_EMIT;	/* clock high; data valid */
	if (TULIP_CSR_READ(sc, csr_srom_mii) & MII_DIN)
	    data |= 1;
	csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
    }
    csr ^= MII_RD; MII_EMIT;		/* clock low; turn off read */

    return data;
}

static unsigned
tulip_mii_readreg(
    tulip_softc_t * const sc,
    unsigned devaddr,
    unsigned regno)
{
    unsigned csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    unsigned data;

    csr &= ~(MII_RD|MII_CLK); MII_EMIT;
    tulip_mii_writebits(sc, MII_PREAMBLE, 32);
    tulip_mii_writebits(sc, MII_RDCMD, 8);
    tulip_mii_writebits(sc, devaddr, 5);
    tulip_mii_writebits(sc, regno, 5);
    tulip_mii_turnaround(sc, MII_RDCMD);

    data = tulip_mii_readbits(sc);
#if defined(TULIP_DEBUG)
    sc->tulip_dbg.dbg_phyregs[regno][0] = data;
    sc->tulip_dbg.dbg_phyregs[regno][1]++;
#endif
    return data;
}

static void
tulip_mii_writereg(
    tulip_softc_t * const sc,
    unsigned devaddr,
    unsigned regno,
    unsigned data)
{
    unsigned csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    csr &= ~(MII_RD|MII_CLK); MII_EMIT;
    tulip_mii_writebits(sc, MII_PREAMBLE, 32);
    tulip_mii_writebits(sc, MII_WRCMD, 8);
    tulip_mii_writebits(sc, devaddr, 5);
    tulip_mii_writebits(sc, regno, 5);
    tulip_mii_turnaround(sc, MII_WRCMD);
    tulip_mii_writebits(sc, data, 16);
#if defined(TULIP_DEBUG)
    sc->tulip_dbg.dbg_phyregs[regno][2] = data;
    sc->tulip_dbg.dbg_phyregs[regno][3]++;
#endif
}

#define	tulip_mchash(mca)	(tulip_crc32(mca, 6) & 0x1FF)
#define	tulip_srom_crcok(databuf)	( \
    ((tulip_crc32(databuf, 126) & 0xFFFF) ^ 0xFFFF)== \
     ((databuf)[126] | ((databuf)[127] << 8)))

static unsigned
tulip_crc32(
    const unsigned char *databuf,
    size_t datalen)
{
    u_int idx, bit, data, crc = 0xFFFFFFFFUL;

    for (idx = 0; idx < datalen; idx++)
        for (data = *databuf++, bit = 0; bit < 8; bit++, data >>= 1)
            crc = (crc >> 1) ^ (((crc ^ data) & 1) ? TULIP_CRC32_POLY : 0);
    return crc;
}

static void
tulip_identify_dec_nic(
    tulip_softc_t * const sc)
{
    strcpy(sc->tulip_boardid, "DEC ");
#define D0	4
    if (sc->tulip_chipid <= TULIP_DE425)
	return;
    if (memcmp(sc->tulip_rombuf + 29, "DE500", 5) == 0
	|| memcmp(sc->tulip_rombuf + 29, "DE450", 5) == 0) {
	memcpy(&sc->tulip_boardid[D0], sc->tulip_rombuf + 29, 8);
	sc->tulip_boardid[D0+8] = ' ';
    }
#undef D0
}

static void
tulip_identify_znyx_nic(
    tulip_softc_t * const sc)
{
    unsigned id = 0;
    strcpy(sc->tulip_boardid, "ZNYX ZX3XX ");
    if (sc->tulip_chipid == TULIP_21140 || sc->tulip_chipid == TULIP_21140A) {
	unsigned znyx_ptr;
	sc->tulip_boardid[8] = '4';
	znyx_ptr = sc->tulip_rombuf[124] + 256 * sc->tulip_rombuf[125];
	if (znyx_ptr < 26 || znyx_ptr > 116) {
	    sc->tulip_boardsw = &tulip_21140_znyx_zx34x_boardsw;
	    return;
	}
	/* ZX344 = 0010 .. 0013FF
	 */
	if (sc->tulip_rombuf[znyx_ptr] == 0x4A
		&& sc->tulip_rombuf[znyx_ptr + 1] == 0x52
		&& sc->tulip_rombuf[znyx_ptr + 2] == 0x01) {
	    id = sc->tulip_rombuf[znyx_ptr + 5] + 256 * sc->tulip_rombuf[znyx_ptr + 4];
	    if ((id >> 8) == (TULIP_ZNYX_ID_ZX342 >> 8)) {
		sc->tulip_boardid[9] = '2';
		if (id == TULIP_ZNYX_ID_ZX342B) {
		    sc->tulip_boardid[10] = 'B';
		    sc->tulip_boardid[11] = ' ';
		}
		sc->tulip_boardsw = &tulip_21140_znyx_zx34x_boardsw;
	    } else if (id == TULIP_ZNYX_ID_ZX344) {
		sc->tulip_boardid[10] = '4';
		sc->tulip_boardsw = &tulip_21140_znyx_zx34x_boardsw;
	    } else if (id == TULIP_ZNYX_ID_ZX345) {
		sc->tulip_boardid[9] = (sc->tulip_rombuf[19] > 1) ? '8' : '5';
	    } else if (id == TULIP_ZNYX_ID_ZX346) {
		sc->tulip_boardid[9] = '6';
	    } else if (id == TULIP_ZNYX_ID_ZX351) {
		sc->tulip_boardid[8] = '5';
		sc->tulip_boardid[9] = '1';
	    }
	}
	if (id == 0) {
	    /*
	     * Assume it's a ZX342...
	     */
	    sc->tulip_boardsw = &tulip_21140_znyx_zx34x_boardsw;
	}
	return;
    }
    sc->tulip_boardid[8] = '1';
    if (sc->tulip_chipid == TULIP_21041) {
	sc->tulip_boardid[10] = '1';
	return;
    }
    if (sc->tulip_rombuf[32] == 0x4A && sc->tulip_rombuf[33] == 0x52) {
	id = sc->tulip_rombuf[37] + 256 * sc->tulip_rombuf[36];
	if (id == TULIP_ZNYX_ID_ZX312T) {
	    sc->tulip_boardid[9] = '2';
	    sc->tulip_boardid[10] = 'T';
	    sc->tulip_boardid[11] = ' ';
	    sc->tulip_boardsw = &tulip_21040_10baset_only_boardsw;
	} else if (id == TULIP_ZNYX_ID_ZX314_INTA) {
	    sc->tulip_boardid[9] = '4';
	    sc->tulip_boardsw = &tulip_21040_10baset_only_boardsw;
	    sc->tulip_flags |= TULIP_SHAREDINTR|TULIP_BASEROM;
	} else if (id == TULIP_ZNYX_ID_ZX314) {
	    sc->tulip_boardid[9] = '4';
	    sc->tulip_boardsw = &tulip_21040_10baset_only_boardsw;
	    sc->tulip_flags |= TULIP_BASEROM;
	} else if (id == TULIP_ZNYX_ID_ZX315_INTA) {
	    sc->tulip_boardid[9] = '5';
	    sc->tulip_flags |= TULIP_SHAREDINTR|TULIP_BASEROM;
	} else if (id == TULIP_ZNYX_ID_ZX315) {
	    sc->tulip_boardid[9] = '5';
	    sc->tulip_flags |= TULIP_BASEROM;
	} else {
	    id = 0;
	}
    }		    
    if (id == 0) {
	if ((sc->tulip_enaddr[3] & ~3) == 0xF0 && (sc->tulip_enaddr[5] & 3) == 0) {
	    sc->tulip_boardid[9] = '4';
	    sc->tulip_boardsw = &tulip_21040_10baset_only_boardsw;
	    sc->tulip_flags |= TULIP_SHAREDINTR|TULIP_BASEROM;
	} else if ((sc->tulip_enaddr[3] & ~3) == 0xF4 && (sc->tulip_enaddr[5] & 1) == 0) {
	    sc->tulip_boardid[9] = '5';
	    sc->tulip_boardsw = &tulip_21040_boardsw;
	    sc->tulip_flags |= TULIP_SHAREDINTR|TULIP_BASEROM;
	} else if ((sc->tulip_enaddr[3] & ~3) == 0xEC) {
	    sc->tulip_boardid[9] = '2';
	    sc->tulip_boardsw = &tulip_21040_boardsw;
	}
    }
}

static void
tulip_identify_smc_nic(
    tulip_softc_t * const sc)
{
    u_int32_t id1, id2, ei;
    int auibnc = 0, utp = 0;
    char *cp;

    strcpy(sc->tulip_boardid, "SMC ");
    if (sc->tulip_chipid == TULIP_21041)
	return;
    if (sc->tulip_chipid != TULIP_21040) {
	if (sc->tulip_boardsw != &tulip_2114x_isv_boardsw) {
	    strcpy(&sc->tulip_boardid[4], "9332DST ");
	    sc->tulip_boardsw = &tulip_21140_smc9332_boardsw;
	} else if (sc->tulip_flags & (TULIP_BASEROM|TULIP_SLAVEDROM)) {
	    strcpy(&sc->tulip_boardid[4], "9332BDT ");
	} else {
	    strcpy(&sc->tulip_boardid[4], "9334BDT ");
	}
	return;
    }
    id1 = sc->tulip_rombuf[0x60] | (sc->tulip_rombuf[0x61] << 8);
    id2 = sc->tulip_rombuf[0x62] | (sc->tulip_rombuf[0x63] << 8);
    ei  = sc->tulip_rombuf[0x66] | (sc->tulip_rombuf[0x67] << 8);

    strcpy(&sc->tulip_boardid[4], "8432");
    cp = &sc->tulip_boardid[8];
    if ((id1 & 1) == 0)
	*cp++ = 'B', auibnc = 1;
    if ((id1 & 0xFF) > 0x32)
	*cp++ = 'T', utp = 1;
    if ((id1 & 0x4000) == 0)
	*cp++ = 'A', auibnc = 1;
    if (id2 == 0x15) {
	sc->tulip_boardid[7] = '4';
	*cp++ = '-';
	*cp++ = 'C';
	*cp++ = 'H';
	*cp++ = (ei ? '2' : '1');
    }
    *cp++ = ' ';
    *cp = '\0';
    if (utp && !auibnc)
	sc->tulip_boardsw = &tulip_21040_10baset_only_boardsw;
    else if (!utp && auibnc)
	sc->tulip_boardsw = &tulip_21040_auibnc_only_boardsw;
}

static void
tulip_identify_cogent_nic(
    tulip_softc_t * const sc)
{
#ifdef HRH_RIP
    strcpy(sc->tulip_boardid, "Cogent ");
    if (sc->tulip_chipid == TULIP_21140 || sc->tulip_chipid == TULIP_21140A) {
	if (sc->tulip_rombuf[32] == TULIP_COGENT_EM100_ID)
	    sc->tulip_boardsw = &tulip_21140_cogent_em100_boardsw;
    } else if (sc->tulip_chipid == TULIP_21040) {
	sc->tulip_flags |= TULIP_SHAREDINTR|TULIP_BASEROM;
    }
#endif
}
#if 0

static void
tulip_identify_avalon_nic(
    tulip_softc_t * const sc)
{
    strcpy(sc->tulip_boardid, "Avalon PMC2TTX");
}
#endif

static void
tulip_identify_asante_nic(
    tulip_softc_t * const sc)
{
    strcpy(sc->tulip_boardid, "Asante ");
    if ((sc->tulip_chipid == TULIP_21140 || sc->tulip_chipid == TULIP_21140A)
	    && sc->tulip_boardsw != &tulip_2114x_isv_boardsw) {
	tulip_media_info_t *mi = sc->tulip_mediainfo;

	/*
	 * The Asante Fast Ethernet doesn't always ship with a valid
	 * new format SROM.  So if isn't in the new format, we cheat
	 * set it up as if we had.
	 */

	sc->tulip_gpinit = TULIP_GP_ASANTE_PINS;
	sc->tulip_gpdata = 0;

	TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ASANTE_PINS|TULIP_GP_PINSET);
	TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ASANTE_PHYRESET);
	DELAY(100);
	TULIP_CSR_WRITE(sc, csr_gp, 0);
	DELAY(200000);

	mi->mi_type = TULIP_MEDIAINFO_MII;
	mi->mi_gpr_length = 0;
	mi->mi_gpr_offset = 0;
	mi->mi_reset_length = 0;
	mi->mi_reset_offset = 0;

	mi->mi_phyaddr = tulip_mii_get_phyaddr(sc, 0);
	if (mi->mi_phyaddr == TULIP_MII_NOPHY)
	    return;
	sc->tulip_features |= TULIP_HAVE_MII;
	mi->mi_capabilities  = PHYSTS_10BASET|PHYSTS_10BASET_FD|PHYSTS_100BASETX|PHYSTS_100BASETX_FD;
	mi->mi_advertisement = PHYSTS_10BASET|PHYSTS_10BASET_FD|PHYSTS_100BASETX|PHYSTS_100BASETX_FD;
	mi->mi_full_duplex   = PHYSTS_10BASET_FD|PHYSTS_100BASETX_FD;
	mi->mi_tx_threshold  = PHYSTS_10BASET|PHYSTS_10BASET_FD;
	TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX_FD);
	TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX);
	TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASET4);
	TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET_FD);
	TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET);

	sc->tulip_boardsw = &tulip_2114x_isv_boardsw;
    }
}

static int
tulip_srom_decode(
    tulip_softc_t * const sc)
{
    int idx1, idx2, idx3;

    const tulip_srom_header_t *shp = (tulip_srom_header_t *) &sc->tulip_rombuf[0];
    const tulip_srom_adapter_info_t *saip = (tulip_srom_adapter_info_t *) (shp + 1);
    tulip_srom_media_t srom_media;
    tulip_media_info_t *mi = sc->tulip_mediainfo;
    const u_int8_t *dp;
    u_int32_t leaf_offset, blocks, data;

    for (idx1 = 0; idx1 < shp->sh_adapter_count; idx1++, saip++) {
	if (shp->sh_adapter_count == 1)
	    break;
	if (saip->sai_device == sc->tulip_pci_devno)
	    break;
    }
    /*
     * Didn't find the right media block for this card.
     */
    if (idx1 == shp->sh_adapter_count)
	return 0;

    /*
     * Save the hardware address.
     */
    memcpy((void *) sc->tulip_enaddr, (void *) shp->sh_ieee802_address, 6);
    /*
     * If this is a multiple port card, add the adapter index to the last
     * byte of the hardware address.  (if it isn't multiport, adding 0
     * won't hurt.
     */
    sc->tulip_enaddr[5] += idx1;

    leaf_offset = saip->sai_leaf_offset_lowbyte
	+ saip->sai_leaf_offset_highbyte * 256;
    dp = sc->tulip_rombuf + leaf_offset;
	
    sc->tulip_conntype = (tulip_srom_connection_t) (dp[0] + dp[1] * 256); dp += 2;

    for (idx2 = 0;; idx2++) {
	if (tulip_srom_conninfo[idx2].sc_type == sc->tulip_conntype
	        || tulip_srom_conninfo[idx2].sc_type == TULIP_SROM_CONNTYPE_NOT_USED)
	    break;
    }
    sc->tulip_connidx = idx2;

    if (sc->tulip_chipid == TULIP_21041) {
	blocks = *dp++;
	for (idx2 = 0; idx2 < blocks; idx2++) {
	    tulip_media_t media;
	    data = *dp++;
	    srom_media = (tulip_srom_media_t) (data & 0x3F);
	    for (idx3 = 0; tulip_srom_mediums[idx3].sm_type != TULIP_MEDIA_UNKNOWN; idx3++) {
		if (tulip_srom_mediums[idx3].sm_srom_type == srom_media)
		    break;
	    }
	    media = tulip_srom_mediums[idx3].sm_type;
	    if (media != TULIP_MEDIA_UNKNOWN) {
		if (data & TULIP_SROM_21041_EXTENDED) {
		    mi->mi_type = TULIP_MEDIAINFO_SIA;
		    sc->tulip_mediums[media] = mi;
		    mi->mi_sia_connectivity = dp[0] + dp[1] * 256;
		    mi->mi_sia_tx_rx        = dp[2] + dp[3] * 256;
		    mi->mi_sia_general      = dp[4] + dp[5] * 256;
		    mi++;
		} else {
		    switch (media) {
			case TULIP_MEDIA_BNC: {
			    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, BNC);
			    mi++;
			    break;
			}
			case TULIP_MEDIA_AUI: {
			    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, AUI);
			    mi++;
			    break;
			}
			case TULIP_MEDIA_10BASET: {
			    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, 10BASET);
			    mi++;
			    break;
			}
			case TULIP_MEDIA_10BASET_FD: {
			    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, 10BASET_FD);
			    mi++;
			    break;
			}
			default: {
			    break;
			}
		    }
		}
	    }
	    if (data & TULIP_SROM_21041_EXTENDED)	
		dp += 6;
	}
#ifdef notdef
	if (blocks == 0) {
	    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, BNC); mi++;
	    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, AUI); mi++;
	    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, 10BASET); mi++;
	    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, 10BASET_FD); mi++;
	}
#endif
    } else {
	unsigned length, type;
	tulip_media_t gp_media = TULIP_MEDIA_UNKNOWN;
	if (sc->tulip_features & TULIP_HAVE_GPR)
	    sc->tulip_gpinit = *dp++;
	blocks = *dp++;
	for (idx2 = 0; idx2 < blocks; idx2++) {
	    const u_int8_t *ep;
	    if ((*dp & 0x80) == 0) {
		length = 4;
		type = 0;
	    } else {
		length = (*dp++ & 0x7f) - 1;
		type = *dp++ & 0x3f;
	    }
	    ep = dp + length;
	    switch (type & 0x3f) {
		case 0: {	/* 21140[A] GPR block */
		    tulip_media_t media;
		    srom_media = (tulip_srom_media_t) dp[0];
		    for (idx3 = 0; tulip_srom_mediums[idx3].sm_type != TULIP_MEDIA_UNKNOWN; idx3++) {
			if (tulip_srom_mediums[idx3].sm_srom_type == srom_media)
			    break;
		    }
		    media = tulip_srom_mediums[idx3].sm_type;
		    if (media == TULIP_MEDIA_UNKNOWN)
			break;
		    mi->mi_type = TULIP_MEDIAINFO_GPR;
		    sc->tulip_mediums[media] = mi;
		    mi->mi_gpdata = dp[1];
		    if (media > gp_media && !TULIP_IS_MEDIA_FD(media)) {
			sc->tulip_gpdata = mi->mi_gpdata;
			gp_media = media;
		    }
		    data = dp[2] + dp[3] * 256;
		    mi->mi_cmdmode = TULIP_SROM_2114X_CMDBITS(data);
		    if (data & TULIP_SROM_2114X_NOINDICATOR) {
			mi->mi_actmask = 0;
		    } else {
#if 0
			mi->mi_default = (data & TULIP_SROM_2114X_DEFAULT) != 0;
#endif
			mi->mi_actmask = TULIP_SROM_2114X_BITPOS(data);
			mi->mi_actdata = (data & TULIP_SROM_2114X_POLARITY) ? 0 : mi->mi_actmask;
		    }
		    mi++;
		    break;
		}
		case 1: {	/* 21140[A] MII block */
		    const unsigned phyno = *dp++;
		    mi->mi_type = TULIP_MEDIAINFO_MII;
		    mi->mi_gpr_length = *dp++;
		    mi->mi_gpr_offset = dp - sc->tulip_rombuf;
		    dp += mi->mi_gpr_length;
		    mi->mi_reset_length = *dp++;
		    mi->mi_reset_offset = dp - sc->tulip_rombuf;
		    dp += mi->mi_reset_length;

		    /*
		     * Before we probe for a PHY, use the GPR information
		     * to select it.  If we don't, it may be inaccessible.
		     */
		    TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_gpinit|TULIP_GP_PINSET);
		    for (idx3 = 0; idx3 < mi->mi_reset_length; idx3++) {
			DELAY(10);
			TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_rombuf[mi->mi_reset_offset + idx3]);
		    }
		    sc->tulip_phyaddr = mi->mi_phyaddr;
		    for (idx3 = 0; idx3 < mi->mi_gpr_length; idx3++) {
			DELAY(10);
			TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_rombuf[mi->mi_gpr_offset + idx3]);
		    }

		    mi->mi_phyaddr = tulip_mii_get_phyaddr(sc, phyno);
		    if (mi->mi_phyaddr == TULIP_MII_NOPHY)
			break;
		    sc->tulip_features |= TULIP_HAVE_MII;
		    mi->mi_capabilities  = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_advertisement = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_full_duplex   = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_tx_threshold  = dp[0] + dp[1] * 256; dp += 2;
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX_FD);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASET4);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET_FD);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET);
		    mi++;
		    break;
		}
		case 2: {	/* 2114[23] SIA block */
		    tulip_media_t media;
		    srom_media = (tulip_srom_media_t) dp[0];
		    for (idx3 = 0; tulip_srom_mediums[idx3].sm_type != TULIP_MEDIA_UNKNOWN; idx3++) {
			if (tulip_srom_mediums[idx3].sm_srom_type == srom_media)
			    break;
		    }
		    media = tulip_srom_mediums[idx3].sm_type;
		    if (media == TULIP_MEDIA_UNKNOWN)
			break;
		    mi->mi_type = TULIP_MEDIAINFO_SIA;
		    sc->tulip_mediums[media] = mi;
		    if (type & 0x40) {
			mi->mi_sia_connectivity = dp[0] + dp[1] * 256;
			mi->mi_sia_tx_rx        = dp[2] + dp[3] * 256;
			mi->mi_sia_general      = dp[4] + dp[5] * 256;
			dp += 6;
		    } else {
			switch (media) {
			    case TULIP_MEDIA_BNC: {
				TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21142, BNC);
				break;
			    }
			    case TULIP_MEDIA_AUI: {
				TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21142, AUI);
				break;
			    }
			    case TULIP_MEDIA_10BASET: {
				TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21142, 10BASET);
				break;
			    }
			    case TULIP_MEDIA_10BASET_FD: {
				TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21142, 10BASET_FD);
				break;
			    }
			    default: {
				goto bad_media;
			    }
			}
		    }
		    mi->mi_sia_gp_control = (dp[0] + dp[1] * 256) << 16;
		    mi->mi_sia_gp_data    = (dp[2] + dp[3] * 256) << 16;
		    mi++;
		  bad_media:
		    break;
		}
		case 3: {	/* 2114[23] MII PHY block */
		    const unsigned phyno = *dp++;
		    const u_int8_t *dp0;
		    mi->mi_type = TULIP_MEDIAINFO_MII;
		    mi->mi_gpr_length = *dp++;
		    mi->mi_gpr_offset = dp - sc->tulip_rombuf;
		    dp += 2 * mi->mi_gpr_length;
		    mi->mi_reset_length = *dp++;
		    mi->mi_reset_offset = dp - sc->tulip_rombuf;
		    dp += 2 * mi->mi_reset_length;

		    dp0 = &sc->tulip_rombuf[mi->mi_reset_offset];
		    for (idx3 = 0; idx3 < mi->mi_reset_length; idx3++, dp0 += 2) {
			DELAY(10);
			TULIP_CSR_WRITE(sc, csr_sia_general, (dp0[0] + 256 * dp0[1]) << 16);
		    }
		    sc->tulip_phyaddr = mi->mi_phyaddr;
		    dp0 = &sc->tulip_rombuf[mi->mi_gpr_offset];
		    for (idx3 = 0; idx3 < mi->mi_gpr_length; idx3++, dp0 += 2) {
			DELAY(10);
			TULIP_CSR_WRITE(sc, csr_sia_general, (dp0[0] + 256 * dp0[1]) << 16);
		    }
		    mi->mi_phyaddr = tulip_mii_get_phyaddr(sc, phyno);
		    if (mi->mi_phyaddr == TULIP_MII_NOPHY)
			break;
		    sc->tulip_features |= TULIP_HAVE_MII;
		    mi->mi_capabilities  = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_advertisement = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_full_duplex   = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_tx_threshold  = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_mii_interrupt = dp[0] + dp[1] * 256; dp += 2;
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX_FD);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASET4);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET_FD);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET);
		    mi++;
		    break;
		}
		case 4: {	/* 21143 SYM block */
		    tulip_media_t media;
		    srom_media = (tulip_srom_media_t) dp[0];
		    for (idx3 = 0; tulip_srom_mediums[idx3].sm_type != TULIP_MEDIA_UNKNOWN; idx3++) {
			if (tulip_srom_mediums[idx3].sm_srom_type == srom_media)
			    break;
		    }
		    media = tulip_srom_mediums[idx3].sm_type;
		    if (media == TULIP_MEDIA_UNKNOWN)
			break;
		    mi->mi_type = TULIP_MEDIAINFO_SYM;
		    sc->tulip_mediums[media] = mi;
		    mi->mi_gpcontrol = (dp[1] + dp[2] * 256) << 16;
		    mi->mi_gpdata    = (dp[3] + dp[4] * 256) << 16;
		    data = dp[5] + dp[6] * 256;
		    mi->mi_cmdmode = TULIP_SROM_2114X_CMDBITS(data);
		    if (data & TULIP_SROM_2114X_NOINDICATOR) {
			mi->mi_actmask = 0;
		    } else {
			mi->mi_default = (data & TULIP_SROM_2114X_DEFAULT) != 0;
			mi->mi_actmask = TULIP_SROM_2114X_BITPOS(data);
			mi->mi_actdata = (data & TULIP_SROM_2114X_POLARITY) ? 0 : mi->mi_actmask;
		    }
		    mi++;
		    break;
		}
#if 0
		case 5: {	/* 21143 Reset block */
		    mi->mi_type = TULIP_MEDIAINFO_RESET;
		    mi->mi_reset_length = *dp++;
		    mi->mi_reset_offset = dp - sc->tulip_rombuf;
		    dp += 2 * mi->mi_reset_length;
		    mi++;
		    break;
		}
#endif
		default: {
		}
	    }
	    dp = ep;
	}
    }
    return mi - sc->tulip_mediainfo;
}

static const struct {
    void (*vendor_identify_nic)(tulip_softc_t * const sc);
    unsigned char vendor_oui[3];
} tulip_vendors[] = {
    { tulip_identify_dec_nic,		{ 0x08, 0x00, 0x2B } },
    { tulip_identify_dec_nic,		{ 0x00, 0x00, 0xF8 } },
    { tulip_identify_smc_nic,		{ 0x00, 0x00, 0xC0 } },
    { tulip_identify_smc_nic,		{ 0x00, 0xE0, 0x29 } },
    { tulip_identify_znyx_nic,		{ 0x00, 0xC0, 0x95 } },
    { tulip_identify_cogent_nic,	{ 0x00, 0x00, 0x92 } },
    { tulip_identify_asante_nic,	{ 0x00, 0x00, 0x94 } },
#if 0
    { tulip_identify_avalon_nic,	{ 0x00, 0x80, 0xe0 } },
    { tulip_identify_avalon_nic,	{ 0x02, 0xbb, 0x01 } },
#else
    { tulip_identify_dec_nic,		{ 0x00, 0x80, 0xe0 } },
    { tulip_identify_dec_nic,		{ 0x02, 0xbb, 0x01 } },
#endif
    { NULL }
};

/*
 * This deals with the vagaries of the address roms and the
 * brain-deadness that various vendors commit in using them.
 */
static int
tulip_read_macaddr(
    tulip_softc_t * const sc)
{
    int cksum, rom_cksum, idx;
    u_int32_t csr;
    unsigned char tmpbuf[8];
    static const u_char testpat[] = { 0xFF, 0, 0x55, 0xAA, 0xFF, 0, 0x55, 0xAA };

    sc->tulip_connidx = TULIP_SROM_LASTCONNIDX;

    if (sc->tulip_chipid == TULIP_21040) {
	TULIP_CSR_WRITE(sc, csr_enetrom, 1);
	for (idx = 0; idx < sizeof(sc->tulip_rombuf); idx++) {
	    int cnt = 0;
	    while (((csr = TULIP_CSR_READ(sc, csr_enetrom)) & 0x80000000L) && cnt < 10000)
		cnt++;
	    sc->tulip_rombuf[idx] = csr & 0xFF;
	}
	sc->tulip_boardsw = &tulip_21040_boardsw;
#if defined(TULIP_EISA)
    } else if (sc->tulip_chipid == TULIP_DE425) {
	int cnt;
	for (idx = 0, cnt = 0; idx < sizeof(testpat) && cnt < 32; cnt++) {
	    tmpbuf[idx] = TULIP_CSR_READBYTE(sc, csr_enetrom);
	    if (tmpbuf[idx] == testpat[idx])
		++idx;
	    else
		idx = 0;
	}
	for (idx = 0; idx < 32; idx++)
	    sc->tulip_rombuf[idx] = TULIP_CSR_READBYTE(sc, csr_enetrom);
	sc->tulip_boardsw = &tulip_21040_boardsw;
#endif /* TULIP_EISA */
    } else {
	if (sc->tulip_chipid == TULIP_21041) {
	    /*
	     * Thankfully all 21041's act the same.
	     */
	    sc->tulip_boardsw = &tulip_21041_boardsw;
	} else {
	    /*
	     * Assume all 21140 board are compatible with the
	     * DEC 10/100 evaluation board.  Not really valid but
	     * it's the best we can do until every one switches to
	     * the new SROM format.
	     */
	     
	    sc->tulip_boardsw = &tulip_21140_eb_boardsw;
	}
	tulip_srom_read(sc);
	if (tulip_srom_crcok(sc->tulip_rombuf)) {
	    /*
	     * SROM CRC is valid therefore it must be in the
	     * new format.
	     */
	    sc->tulip_features |= TULIP_HAVE_ISVSROM;
	} else if (sc->tulip_rombuf[126] == 0xff && sc->tulip_rombuf[127] == 0xFF) {
	    /*
	     * No checksum is present.  See if the SROM id checks out;
	     * the first 18 bytes should be 0 followed by a 1 followed
	     * by the number of adapters (which we don't deal with yet).
	     */
	    for (idx = 0; idx < 18; idx++) {
		if (sc->tulip_rombuf[idx] != 0)
		    break;
	    }
	    if (idx == 18 && sc->tulip_rombuf[18] == 1 && sc->tulip_rombuf[19] != 0)
		sc->tulip_features |= TULIP_HAVE_ISVSROM;
	}
	if ((sc->tulip_features & TULIP_HAVE_ISVSROM) && tulip_srom_decode(sc)) {
	    if (sc->tulip_chipid != TULIP_21041)
		sc->tulip_boardsw = &tulip_2114x_isv_boardsw;

	    /*
	     * If the SROM specifies more than one adapter, tag this as a
	     * BASE rom.
	     */
	    if (sc->tulip_rombuf[19] > 1)
		sc->tulip_flags |= TULIP_BASEROM;
	    if (sc->tulip_boardsw == NULL)
		return -6;
	    goto check_oui;
	}
    }


    if (memcmp(&sc->tulip_rombuf[0], &sc->tulip_rombuf[16], 8) != 0) {
	/*
	 * Detect early Avalon 100-TX PMC cards with magic number followed
	 * by hw addr. These should all have been upgraded by now. It's an
	 * electronic eeprom write-in-place, so there's no excuse not to...
	 */
	if (memcmp("AC5E", sc->tulip_rombuf, 4) == 0) {
		panic("PMC2TTX: old format Avalon srom, reprogram hw addr");
#if 0
		printf("ade%d: Warning: reinit srom! Old Avalon format!",
			cf->cf_unit);
		memcpy(sc->tulip_enaddr, sc->tulip_rombuf + 4, 6);
		sc->tulip_flags |= TULIP_ROMOK;
		goto check_oui;
#endif
	}
	/*
	 * Some folks don't use the standard ethernet rom format
	 * but instead just put the address in the first 6 bytes
	 * of the rom and let the rest be all 0xffs.  (Can we say
	 * ZNYX???) (well sometimes they put in a checksum so we'll
	 * start at 8).
	 */
	for (idx = 8; idx < 32; idx++) {
	    if (sc->tulip_rombuf[idx] != 0xFF)
		return -4;
	}
	/*
	 * Make sure the address is not multicast or locally assigned
	 * that the OUI is not 00-00-00.
	 */
	if ((sc->tulip_rombuf[0] & 3) != 0)
	    return -4;
	if (sc->tulip_rombuf[0] == 0 && sc->tulip_rombuf[1] == 0
		&& sc->tulip_rombuf[2] == 0)
	    return -4;
	memcpy(sc->tulip_enaddr, sc->tulip_rombuf, 6);
	sc->tulip_flags |= TULIP_ROMOK;
	goto check_oui;
    } else {
	/*
	 * A number of makers of multiport boards (ZNYX and Cogent)
	 * only put on one address ROM on their 21040 boards.  So
	 * if the ROM is all zeros (or all 0xFFs), look at the
	 * previous configured boards (as long as they are on the same
	 * PCI bus and the bus number is non-zero) until we find the
	 * master board with address ROM.  We then use its address ROM
	 * as the base for this board.  (we add our relative board
	 * to the last byte of its address).
	 */
	for (idx = 0; idx < sizeof(sc->tulip_rombuf); idx++) {
	    if (sc->tulip_rombuf[idx] != 0 && sc->tulip_rombuf[idx] != 0xFF)
		break;
	}
	if (idx == sizeof(sc->tulip_rombuf)) {
	    int root_unit;
	    tulip_softc_t *root_sc = NULL;
	    for (root_unit = sc->tulip_unit - 1; root_unit >= 0; root_unit--) {
		root_sc = TULIP_UNIT_TO_SOFTC(root_unit);
		if (root_sc == NULL || (root_sc->tulip_flags & (TULIP_ROMOK|TULIP_SLAVEDROM)) == TULIP_ROMOK)
		    break;
		root_sc = NULL;
	    }
	    if (root_sc != NULL && (root_sc->tulip_flags & TULIP_BASEROM)
		    && root_sc->tulip_chipid == sc->tulip_chipid
		    && root_sc->tulip_pci_busno == sc->tulip_pci_busno) {
		sc->tulip_flags |= TULIP_SLAVEDROM;
		sc->tulip_boardsw = root_sc->tulip_boardsw;
		strcpy(sc->tulip_boardid, root_sc->tulip_boardid);
		if (sc->tulip_boardsw->bd_type == TULIP_21140_ISV) {
		    memcpy(sc->tulip_rombuf, root_sc->tulip_rombuf,
			  sizeof(sc->tulip_rombuf));
		    if (!tulip_srom_decode(sc))
			return -5;
		} else {
		    memcpy(sc->tulip_enaddr, root_sc->tulip_enaddr, 6);
		    sc->tulip_enaddr[5] += sc->tulip_unit - root_sc->tulip_unit;
		}
		/*
		 * Now for a truly disgusting kludge: all 4 21040s on
		 * the ZX314 share the same INTA line so the mapping
		 * setup by the BIOS on the PCI bridge is worthless.
		 * Rather than reprogramming the value in the config
		 * register, we will handle this internally.
		 */
		if (root_sc->tulip_flags & TULIP_SHAREDINTR) {
		    sc->tulip_slaves = root_sc->tulip_slaves;
		    root_sc->tulip_slaves = sc;
		    sc->tulip_flags |= TULIP_SLAVEDINTR;
		}
		return 0;
	    }
	}
    }

    /*
     * This is the standard DEC address ROM test.
     */

    if (memcmp(&sc->tulip_rombuf[24], testpat, 8) != 0)
	return -3;

    tmpbuf[0] = sc->tulip_rombuf[15]; tmpbuf[1] = sc->tulip_rombuf[14];
    tmpbuf[2] = sc->tulip_rombuf[13]; tmpbuf[3] = sc->tulip_rombuf[12];
    tmpbuf[4] = sc->tulip_rombuf[11]; tmpbuf[5] = sc->tulip_rombuf[10];
    tmpbuf[6] = sc->tulip_rombuf[9];  tmpbuf[7] = sc->tulip_rombuf[8];
    if (memcmp(&sc->tulip_rombuf[0], tmpbuf, 8) != 0)
	return -2;

    memcpy(sc->tulip_enaddr, sc->tulip_rombuf, 6);

    cksum = *(u_int16_t *) &sc->tulip_enaddr[0];
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_int16_t *) &sc->tulip_enaddr[2];
    if (cksum > 65535) cksum -= 65535;
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_int16_t *) &sc->tulip_enaddr[4];
    if (cksum >= 65535) cksum -= 65535;

    rom_cksum = *(u_int16_t *) &sc->tulip_rombuf[6];
	
    if (cksum != rom_cksum)
	return -1;

  check_oui:
    /*
     * Check for various boards based on OUI.  Did I say braindead?
     */
    for (idx = 0; tulip_vendors[idx].vendor_identify_nic != NULL; idx++) {
	if (memcmp((void *) sc->tulip_enaddr,
		 (void *) tulip_vendors[idx].vendor_oui, 3) == 0) {
	    (*tulip_vendors[idx].vendor_identify_nic)(sc);
	    break;
	}
    }

    sc->tulip_flags |= TULIP_ROMOK;
    return 0;
}

static void
tulip_ifmedia_add(
    tulip_softc_t * const sc)
{
    tulip_media_t media;
    int medias = 0;

    for (media = TULIP_MEDIA_UNKNOWN; media < TULIP_MEDIA_MAX; media++) {
	if (sc->tulip_mediums[media] != NULL) {
	    ifmedia_add(&sc->tulip_ifmedia, tulip_media_to_ifmedia[media],
			0, 0);
	    medias++;
	}
    }
    if (medias == 0) {
	ifmedia_add(&sc->tulip_ifmedia, IFM_ETHER | IFM_NONE, 0, 0);
	ifmedia_set(&sc->tulip_ifmedia, IFM_ETHER | IFM_NONE);
    } else if (sc->tulip_media == TULIP_MEDIA_UNKNOWN) {
	ifmedia_add(&sc->tulip_ifmedia, IFM_ETHER | IFM_AUTO, 0, 0);
	ifmedia_set(&sc->tulip_ifmedia, IFM_ETHER | IFM_AUTO);
    } else {
	ifmedia_set(&sc->tulip_ifmedia, tulip_media_to_ifmedia[sc->tulip_media]);
	sc->tulip_flags |= TULIP_PRINTMEDIA;
	tulip_linkup(sc, sc->tulip_media);
    }
}

static int
tulip_ifmedia_change(
    struct ifnet * const ifp)
{
    tulip_softc_t * const sc = TULIP_IFP_TO_SOFTC(ifp);

    sc->tulip_flags |= TULIP_NEEDRESET;
    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
    sc->tulip_media = TULIP_MEDIA_UNKNOWN;
    if (IFM_SUBTYPE(sc->tulip_ifmedia.ifm_media) != IFM_AUTO) {
	tulip_media_t media;
	for (media = TULIP_MEDIA_UNKNOWN; media < TULIP_MEDIA_MAX; media++) {
	    if (sc->tulip_mediums[media] != NULL
		&& sc->tulip_ifmedia.ifm_media == tulip_media_to_ifmedia[media]) {
		sc->tulip_flags |= TULIP_PRINTMEDIA;
		sc->tulip_flags &= ~TULIP_DIDNWAY;
		tulip_linkup(sc, media);
		return 0;
	    }
	}
    }
    sc->tulip_flags &= ~(TULIP_TXPROBE_ACTIVE|TULIP_WANTRXACT);
    tulip_reset(sc);
    tulip_init(sc);
    return 0;
}

/*
 * Media status callback
 */
static void
tulip_ifmedia_status(
    struct ifnet * const ifp,
    struct ifmediareq *req)
{
    tulip_softc_t *sc = TULIP_IFP_TO_SOFTC(ifp);

#if defined(__bsdi__)
    if (sc->tulip_mii.mii_instance != 0) {
	mii_pollstat(&sc->tulip_mii);
	req->ifm_active = sc->tulip_mii.mii_media_active;
	req->ifm_status = sc->tulip_mii.mii_media_status;
	return;
    }
#endif
    if (sc->tulip_media == TULIP_MEDIA_UNKNOWN)
	return;

    req->ifm_status = IFM_AVALID;
    if (sc->tulip_flags & TULIP_LINKUP)
	req->ifm_status |= IFM_ACTIVE;

    req->ifm_active = tulip_media_to_ifmedia[sc->tulip_media];
}

static void
tulip_addr_filter(
    tulip_softc_t * const sc)
{
    struct ether_multistep step;
    struct ether_multi *enm;

    sc->tulip_flags &= ~(TULIP_WANTHASH|TULIP_ALLMULTI);
    sc->tulip_flags |= TULIP_WANTSETUP;
    sc->tulip_cmdmode &= ~TULIP_CMD_RXRUN;
    sc->tulip_intrmask &= ~TULIP_STS_RXSTOPPED;
#if defined(IFF_ALLMULTI)    
    sc->tulip_if.if_flags &= ~IFF_ALLMULTI;
#endif
    if (sc->tulip_multicnt > 14) {
	/*
	 * Some early passes of the 21140 have broken multicast hashes.
	 * When we get too many multicasts with these chips, we have to
	 * switch into all-multicast mode.
	 */
	if (sc->tulip_features & TULIP_HAVE_BROKEN_HASH) {
	    sc->tulip_flags |= TULIP_ALLMULTI;
	} else {
	    u_int32_t *sp = sc->tulip_setupdata;
	    unsigned hash;
	    /*
	     * If we have more than 14 multicasts, we have
	     * go into hash perfect mode (512 bit multicast
	     * hash and one perfect hardware).
	     */
	    memset(sc->tulip_setupdata, 0, sizeof(sc->tulip_setupdata));
	    ETHER_FIRST_MULTI(step, TULIP_ETHERCOM(sc), enm);
	    while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, 6) == 0) {
		    hash = tulip_mchash(enm->enm_addrlo);
		    sp[hash >> 4] |= 1 << (hash & 0xF);
		} else {
		    sc->tulip_flags |= TULIP_ALLMULTI;
		    break;
		}
		ETHER_NEXT_MULTI(step, enm);
	    }
	    /*
	     * No reason to use a hash if we are going to be
	     * receiving every multicast.
	     */
	    if ((sc->tulip_flags & TULIP_ALLMULTI) == 0) {
		hash = tulip_mchash(etherbroadcastaddr);
		sp[hash >> 4] |= 1 << (hash & 0xF);
		sc->tulip_flags |= TULIP_WANTHASH;
		sp[39] = ((u_int16_t *) sc->tulip_enaddr)[0]; 
		sp[40] = ((u_int16_t *) sc->tulip_enaddr)[1]; 
		sp[41] = ((u_int16_t *) sc->tulip_enaddr)[2];
	    }
	}
    }
    if ((sc->tulip_flags & TULIP_WANTHASH) == 0) {
	u_int32_t *sp = sc->tulip_setupdata;
	int idx = 0;
	if ((sc->tulip_flags & TULIP_ALLMULTI) == 0) {
	    /*
	     * Else can get perfect filtering for 16 addresses.
	     */
	    ETHER_FIRST_MULTI(step, TULIP_ETHERCOM(sc), enm);
	    for (; enm != NULL; idx++) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, 6) == 0) {
		    *sp++ = ((u_int16_t *) enm->enm_addrlo)[0]; 
		    *sp++ = ((u_int16_t *) enm->enm_addrlo)[1]; 
		    *sp++ = ((u_int16_t *) enm->enm_addrlo)[2];
		} else {
		    sc->tulip_flags |= TULIP_ALLMULTI;
		    break;
		}
		ETHER_NEXT_MULTI(step, enm);
	    }
	    /*
	     * Add the broadcast address.
	     */
	    idx++;
	    *sp++ = 0xFFFF;
	    *sp++ = 0xFFFF;
	    *sp++ = 0xFFFF;
	}
	/*
	 * Pad the rest with our hardware address
	 */
	for (; idx < 16; idx++) {
	    *sp++ = ((u_int16_t *) sc->tulip_enaddr)[0]; 
	    *sp++ = ((u_int16_t *) sc->tulip_enaddr)[1]; 
	    *sp++ = ((u_int16_t *) sc->tulip_enaddr)[2];
	}
    }
#if defined(IFF_ALLMULTI)
    if (sc->tulip_flags & TULIP_ALLMULTI)
	sc->tulip_if.if_flags |= IFF_ALLMULTI;
#endif
}

static void
tulip_reset(
    tulip_softc_t * const sc)
{
    tulip_ringinfo_t *ri;
    tulip_desc_t *di;
    u_int32_t inreset = (sc->tulip_flags & TULIP_INRESET);

debug_sc = sc;
    /*
     * Brilliant.  Simply brilliant.  When switching modes/speeds
     * on a 2114*, you need to set the appriopriate MII/PCS/SCL/PS
     * bits in CSR6 and then do a software reset to get the 21140
     * to properly reset its internal pathways to the right places.
     *   Grrrr.
     */
    if (sc->tulip_boardsw->bd_media_preset != NULL)
	(*sc->tulip_boardsw->bd_media_preset)(sc);

    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
		   33MHz that comes to two microseconds but wait a
		   bit longer anyways) */

    if (!inreset) {
	sc->tulip_flags |= TULIP_INRESET;
	sc->tulip_flags &= ~(TULIP_NEEDRESET|TULIP_RXBUFSLOW);
	sc->tulip_if.if_flags &= ~IFF_OACTIVE;
    }

    TULIP_CSR_WRITE(sc, csr_txlist, a12map(TULIP_KVATOPHYS(sc, &sc->tulip_txinfo.ri_first[0])));
    TULIP_CSR_WRITE(sc, csr_rxlist, a12map(TULIP_KVATOPHYS(sc, &sc->tulip_rxinfo.ri_first[0])));
    if(cputype==ST_AVALON_A12)
	TULIP_CSR_WRITE(sc, csr_busmode, 0x00008100);
    else {
	TULIP_CSR_WRITE(sc, csr_busmode,
	    (1 << (TULIP_BURSTSIZE(sc->tulip_unit) + 8))
	    |TULIP_BUSMODE_CACHE_ALIGN8
	    |TULIP_BUSMODE_READMULTIPLE
	    |(BYTE_ORDER != LITTLE_ENDIAN ? TULIP_BUSMODE_BIGENDIAN : 0));
    }

    sc->tulip_txtimer = 0;
#ifdef LCLDMA
    reset_lcl_dma(sc);
#endif
    sc->tulip_txq.ifq_maxlen = TULIP_TXDESCS;
    /*
     * Free all the mbufs that were on the transmit ring.
     */
    for (;;) {
	struct mbuf *m;
	IF_DEQUEUE(&sc->tulip_txq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }

    ri = &sc->tulip_txinfo;
    ri->ri_nextin = ri->ri_nextout = ri->ri_first;
    ri->ri_free = ri->ri_max;
    for (di = ri->ri_first; di < ri->ri_last; di++)
	setstatus(di,0);
    /*
     * We need to collect all the mbufs on the 
     * receive ring before we reinit it either to put
     * them back on or to know if we have to allocate
     * more.
     */
    ri = &sc->tulip_rxinfo;
    ri->ri_nextin = ri->ri_nextout = ri->ri_first;
    ri->ri_free = ri->ri_max;
    for (di = ri->ri_first; di < ri->ri_last; di++) {
	BEGIN(di);
	    t.d_status = 0;
	    t.d_length1= 0;
	    t.d_length2= 0;
#ifndef LCLDMA
	    t.d_addr1 = 0;
	    t.d_addr2 = 0;
#endif
	END(di);
    }
    for (;;) {
	struct mbuf *m;
	IF_DEQUEUE(&sc->tulip_rxq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }

    /*
     * If tulip_reset is being called recurisvely, exit quickly knowing
     * that when the outer tulip_reset returns all the right stuff will
     * have happened.
     */
    if (inreset)
	return;

    sc->tulip_intrmask |= TULIP_STS_NORMALINTR|TULIP_STS_RXINTR|TULIP_STS_TXINTR
	|TULIP_STS_ABNRMLINTR|TULIP_STS_SYSERROR|TULIP_STS_TXSTOPPED
	    |TULIP_STS_TXBABBLE|TULIP_STS_LINKFAIL|TULIP_STS_RXSTOPPED;

    if ((sc->tulip_flags & TULIP_DEVICEPROBE) == 0)
	(*sc->tulip_boardsw->bd_media_select)(sc);
#if defined(TULIP_DEBUG)
    if ((sc->tulip_flags & TULIP_NEEDRESET) == TULIP_NEEDRESET)
	printf(TULIP_PRINTF_FMT ": tulip_reset: additional reset needed?!?\n",
	       TULIP_PRINTF_ARGS);
#endif
    tulip_media_print(sc);
    if (sc->tulip_features & TULIP_HAVE_DUALSENSE)
	TULIP_CSR_WRITE(sc, csr_sia_status, TULIP_CSR_READ(sc, csr_sia_status));

    sc->tulip_flags &= ~(TULIP_DOINGSETUP|TULIP_WANTSETUP|TULIP_INRESET
			 |TULIP_RXACT);
    tulip_addr_filter(sc);
}

static void
tulip_init(
    tulip_softc_t * const sc)
{
    if (sc->tulip_if.if_flags & IFF_UP) {
	if ((sc->tulip_if.if_flags & IFF_RUNNING) == 0) {
	    /* initialize the media */
	    tulip_reset(sc);
	}
	sc->tulip_if.if_flags |= IFF_RUNNING;
	if (sc->tulip_if.if_flags & IFF_PROMISC) {
	    sc->tulip_cmdmode |= TULIP_CMD_PROMISCUOUS;
	} else {
	    sc->tulip_cmdmode &= ~TULIP_CMD_PROMISCUOUS;
	    if (sc->tulip_flags & TULIP_ALLMULTI) {
		sc->tulip_cmdmode |= TULIP_CMD_ALLMULTI;
	    } else {
		sc->tulip_cmdmode &= ~TULIP_CMD_ALLMULTI;
	    }
	}
	sc->tulip_cmdmode |= TULIP_CMD_TXRUN;
	if ((sc->tulip_flags & (TULIP_TXPROBE_ACTIVE|TULIP_WANTSETUP)) == 0) {
	    tulip_rx_intr(sc);
	    sc->tulip_cmdmode |= TULIP_CMD_RXRUN;
	    sc->tulip_intrmask |= TULIP_STS_RXSTOPPED;
	} else {
	    sc->tulip_if.if_flags |= IFF_OACTIVE;
	    sc->tulip_cmdmode &= ~TULIP_CMD_RXRUN;
	    sc->tulip_intrmask &= ~TULIP_STS_RXSTOPPED;
	}
	TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
	tulip_ifstart(&sc->tulip_if);
    } else {
	sc->tulip_if.if_flags &= ~IFF_RUNNING;
	tulip_reset(sc);
    }
}

static void
tulip_rx_intr(
    tulip_softc_t * const sc)
{
    tulip_ringinfo_t * const ri = &sc->tulip_rxinfo;
    struct ifnet * const ifp = &sc->tulip_if;
    int fillok = 1;
#if defined(TULIP_DEBUG)
    int cnt = 0;
#endif

    for (;;) {
	struct ether_header eh;
	tulip_desc_t *eop = ri->ri_nextin;
	int total_len = 0, last_offset = 0;
	struct mbuf *ms = NULL, *me = NULL;
	int accept = 0;

	if (fillok && sc->tulip_rxq.ifq_len < TULIP_RXQ_TARGET)
	    goto queue_mbuf;

#if defined(TULIP_DEBUG)
	if (cnt == ri->ri_max)
	    break;
#endif
	/*
	 * If the TULIP has no descriptors, there can't be any receive
	 * descriptors to process.
 	 */
	if (eop == ri->ri_nextout)
	    break;
	    
	/*
	 * 90% of the packets will fit in one descriptor.  So we optimize
	 * for that case.
	 */
	if ((((volatile tulip_desc_t *) eop)->d_status & (TULIP_DSTS_OWNER|TULIP_DSTS_RxFIRSTDESC|TULIP_DSTS_RxLASTDESC)) == (TULIP_DSTS_RxFIRSTDESC|TULIP_DSTS_RxLASTDESC)) {
	    IF_DEQUEUE(&sc->tulip_rxq, ms);
	    me = ms;
	} else {
	    /*
	     * If still owned by the TULIP, don't touch it.
	     */
	    if (((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_OWNER)
		break;

	    /*
	     * It is possible (though improbable unless the BIG_PACKET support
	     * is enabled or MCLBYTES < 1518) for a received packet to cross
	     * more than one receive descriptor.  
	     */
	    while ((((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_RxLASTDESC) == 0) {
#if defined(LCLDMA) && defined(DIAGNOSTIC)
		panic("tulip descriptor count > 1");
#endif
		if (++eop == ri->ri_last)
		    eop = ri->ri_first;
		if (eop == ri->ri_nextout || ((((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_OWNER))) {
#if defined(TULIP_DEBUG)
		    sc->tulip_dbg.dbg_rxintrs++;
		    sc->tulip_dbg.dbg_rxpktsperintr[cnt]++;
#endif
		    return;
		}
		total_len++;
	    }
	    /*
	     * Dequeue the first buffer for the start of the packet.  Hopefully
	     * this will be the only one we need to dequeue.  However, if the
	     * packet consumed multiple descriptors, then we need to dequeue
	     * those buffers and chain to the starting mbuf.  All buffers but
	     * the last buffer have the same length so we can set that now.
	     * (we add to last_offset instead of multiplying since we normally
	     * won't go into the loop and thereby saving a ourselves from
	     * doing a multiplication by 0 in the normal case).
	     */
	    IF_DEQUEUE(&sc->tulip_rxq, ms);
	    for (me = ms; total_len > 0; total_len--) {
		me->m_len = TULIP_RX_BUFLEN;
		last_offset += TULIP_RX_BUFLEN;
		IF_DEQUEUE(&sc->tulip_rxq, me->m_next);
		me = me->m_next;
	    }
	}

	/*
	 *  Now get the size of received packet (minus the CRC).
	 */
	total_len = ((eop->d_status >> 16) & 0x7FFF) - 4;
	/* good packet ? */
	if ((eop->d_status & TULIP_DSTS_ERRSUM) == 0
		|| (sc->tulip_flags & TULIP_RXBAD)
#ifdef BIG_PACKET
	     || (total_len <= sc->tulip_if.if_mtu + sizeof(struct ether_header) && 
		 (eop->d_status & (TULIP_DSTS_RxBADLENGTH|TULIP_DSTS_RxRUNT|
				  TULIP_DSTS_RxCOLLSEEN|TULIP_DSTS_RxBADCRC|
				  TULIP_DSTS_RxOVERFLOW)) == 0)
#endif
		) {
	    me->m_len = total_len - last_offset;
	    eh = *mtod(ms, struct ether_header *);
#if NBPFILTER > 0
	    if (sc->tulip_bpf != NULL) {
		if (me == ms)
		    TULIP_BPF_TAP(sc, mtod(ms, void *), total_len);
		else
		    TULIP_BPF_MTAP(sc, ms);
	    }
#endif
	    if ((sc->tulip_if.if_flags & IFF_PROMISC)
		    && (eh.ether_dhost[0] & 1) == 0
		    && !TULIP_ADDREQUAL(eh.ether_dhost, sc->tulip_enaddr))
		    goto next;
	    accept = 1;
	    sc->tulip_flags |= TULIP_RXACT;
	} else {
	    ifp->if_ierrors++;
	    if (eop->d_status & (TULIP_DSTS_RxBADLENGTH|TULIP_DSTS_RxOVERFLOW|TULIP_DSTS_RxWATCHDOG)) {
		sc->tulip_dot3stats.dot3StatsInternalMacReceiveErrors++;
	    } else {
		const char *error = NULL;
		if (eop->d_status & TULIP_DSTS_RxTOOLONG) {
		    sc->tulip_dot3stats.dot3StatsFrameTooLongs++;
		    error = "frame too long";
		}
		if (eop->d_status & TULIP_DSTS_RxBADCRC) {
		    if (eop->d_status & TULIP_DSTS_RxDRBBLBIT) {
			sc->tulip_dot3stats.dot3StatsAlignmentErrors++;
			error = "alignment error";
		    } else {
			sc->tulip_dot3stats.dot3StatsFCSErrors++;
			error = "bad crc";
		    }
		}
		if (error != NULL && (sc->tulip_flags & TULIP_NOMESSAGES) == 0) {
		    printf(TULIP_PRINTF_FMT ": receive: " TULIP_EADDR_FMT ": %s\n",
			   TULIP_PRINTF_ARGS,
			   TULIP_EADDR_ARGS(mtod(ms, u_char *) + 6),
			   error);
		    sc->tulip_flags |= TULIP_NOMESSAGES;
		}
	    }
	}
      next:
#if defined(TULIP_DEBUG)
	cnt++;
#endif
	ifp->if_ipackets++;
	if (++eop == ri->ri_last)
	    eop = ri->ri_first;
	ri->ri_nextin = eop;
      queue_mbuf:
	/*
	 * Either we are priming the TULIP with mbufs (m == NULL)
	 * or we are about to accept an mbuf for the upper layers
	 * so we need to allocate an mbuf to replace it.  If we
	 * can't replace it, send up it anyways.  This may cause
	 * us to drop packets in the future but that's better than
	 * being caught in livelock.
	 *
	 * Note that if this packet crossed multiple descriptors
	 * we don't even try to reallocate all the mbufs here.
	 * Instead we rely on the test of the beginning of
	 * the loop to refill for the extra consumed mbufs.
	 */
	if (accept || ms == NULL) {
	    struct mbuf *m0;
	    MGETHDR(m0, M_DONTWAIT, MT_DATA);
	    if (m0 != NULL) {
#if defined(TULIP_COPY_RXDATA)
		if (!accept || total_len >= MHLEN - 2) {
#endif
		    MCLGET(m0, M_DONTWAIT);
		    if ((m0->m_flags & M_EXT) == 0) {
			m_freem(m0);
			m0 = NULL;
		    }
#if defined(TULIP_COPY_RXDATA)
		}
#endif
	    }
	    if (accept
#if defined(TULIP_COPY_RXDATA)
		&& m0 != NULL
#endif
		) {
#if defined(__bsdi__)
		eh.ether_type = ntohs(eh.ether_type);
#endif
#if !defined(TULIP_COPY_RXDATA)
		ms->m_pkthdr.len = total_len;
		ms->m_pkthdr.rcvif = ifp;
#if defined(__NetBSD__)
		(*ifp->if_input)(ifp, ms);
#else
		m_adj(ms, sizeof(struct ether_header);
		ether_input(ifp, &eh, ms);
#endif /* __NetBSD__ */
#ifdef LCLDMA
#error LCLDMA requires TULIP_COPY_RXDATA
#endif
#else
#ifdef BIG_PACKET
#error BIG_PACKET is incompatible with TULIP_COPY_RXDATA
#endif
		m0->m_data += 2;	/* align data after header */
		m_copydata(ms, 0, total_len, mtod(m0, void *));
		m0->m_len = m0->m_pkthdr.len = total_len;
		m0->m_pkthdr.rcvif = ifp;
#if defined(__NetBSD__)
		(*ifp->if_input)(ifp, m0);
#else
		m_adj(m0, sizeof(struct ether_header);
		ether_input(ifp, &eh, m0);
#endif /* __NetBSD__ */
		m0 = ms;
#endif /* ! TULIP_COPY_RXDATA */
	    }
	    ms = m0;
	}
	if (ms == NULL) {
	    /*
	     * Couldn't allocate a new buffer.  Don't bother 
	     * trying to replenish the receive queue.
	     */
	    fillok = 0;
	    sc->tulip_flags |= TULIP_RXBUFSLOW;
#if defined(TULIP_DEBUG)
	    sc->tulip_dbg.dbg_rxlowbufs++;
#endif
	    continue;
	}
	/*
	 * Now give the buffer(s) to the TULIP and save in our
	 * receive queue.
	 */
#ifdef LCLDMA
	/*
	 * Made it almost to the end without any separate LCLDMA
	 * code. (Other than debugging.) That took some planning.
	 * If we are priming the rx ring, we have a main-memory
	 * mbuf cluster that we really can't use. Otherwise, we
	 * recycle them efficiently.
	 */
	if (ms->m_flags & M_CLUSTER) {	/* not one of ours */
		struct mbuf *ms2 = ms;
		int ring_entry_number = ri->ri_nextout - ri->ri_first;
		MEXTREMOVE(ms2);	/* uses "ms" internally! */
		MEXTADD(ms2, sc->tulip_rx_kva[ring_entry_number], 
			TULIP_RX_BUFLEN, MT_DATA, donothing, 0);
	}
#ifdef TULIP_DEBUG
	if (ms->m_next != NULL)
		panic("tulip lcldma rx unexpected chain");
	if (ri->ri_nextout->d_addr1
	!= a12map(TULIP_KVATOPHYS(sc, mtod(ms, void *))))
		panic("tulip LCLDMA rx d_addr1");
#endif
#endif
	do {
	    BEGIN(ri->ri_nextout);
		t.d_length1 = TULIP_RX_BUFLEN;
		t.d_addr1   = a12map(TULIP_KVATOPHYS(sc, mtod(ms, void *)));
	    END(ri->ri_nextout);
	    setstatus(ri->ri_nextout,TULIP_DSTS_OWNER);
	    if (++ri->ri_nextout == ri->ri_last)
		ri->ri_nextout = ri->ri_first;
	    me = ms->m_next;
	    ms->m_next = NULL;
	    IF_ENQUEUE(&sc->tulip_rxq, ms);
	} while ((ms = me) != NULL);

	if (sc->tulip_rxq.ifq_len >= TULIP_RXQ_TARGET)
	    sc->tulip_flags &= ~TULIP_RXBUFSLOW;
    }

#if defined(TULIP_DEBUG)
    sc->tulip_dbg.dbg_rxintrs++;
    sc->tulip_dbg.dbg_rxpktsperintr[cnt]++;
#endif
}

static int
tulip_tx_intr(
    tulip_softc_t * const sc)
{
    tulip_ringinfo_t * const ri = &sc->tulip_txinfo;
    struct mbuf *m;
    int xmits = 0;
    int	f;

    while (ri->ri_free < ri->ri_max) {
	if (((volatile tulip_desc_t *) ri->ri_nextin)->d_status & TULIP_DSTS_OWNER)
	    break;

	f = ri->ri_nextin->d_flag;	/* potentially non-cached RAM */
	if (f & TULIP_DFLAG_TxLASTSEG) {
	    if (f & TULIP_DFLAG_TxSETUPPKT) {
		/*
		 * We've just finished processing a setup packet.
		 * Mark that we finished it.  If there's not
		 * another pending, startup the TULIP receiver.
		 * Make sure we ack the RXSTOPPED so we won't get
		 * an abormal interrupt indication.
		 */
		sc->tulip_flags &= ~TULIP_DOINGSETUP;
		if ((sc->tulip_flags & (TULIP_WANTSETUP|TULIP_TXPROBE_ACTIVE)) == 0) {
		    tulip_rx_intr(sc);
		    sc->tulip_cmdmode |= TULIP_CMD_RXRUN;
		    sc->tulip_intrmask |= TULIP_STS_RXSTOPPED;
		    TULIP_CSR_WRITE(sc, csr_status, TULIP_STS_RXSTOPPED);
		    TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
		    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
		}
	    } else {
		tulip_desc_t * const nextin = ri->ri_nextin;
		IF_DEQUEUE(&sc->tulip_txq, m);
		m_freem(m);
		if (sc->tulip_flags & TULIP_TXPROBE_ACTIVE) {
		    tulip_mediapoll_event_t event = TULIP_MEDIAPOLL_TXPROBE_OK;
		    if (nextin->d_status & (TULIP_DSTS_TxNOCARR|TULIP_DSTS_TxEXCCOLL)) {
#if defined(TULIP_DEBUG)
			if (nextin->d_status & TULIP_DSTS_TxNOCARR)
			    sc->tulip_dbg.dbg_txprobe_nocarr++;
			if (nextin->d_status & TULIP_DSTS_TxEXCCOLL)
			    sc->tulip_dbg.dbg_txprobe_exccoll++;
#endif
			event = TULIP_MEDIAPOLL_TXPROBE_FAILED;
		    }
		    (*sc->tulip_boardsw->bd_media_poll)(sc, event);
		    /*
		     * Escape from the loop before media poll has reset the TULIP!
		     */
		    break;
		} else {
		    xmits++;
		    if (nextin->d_status & TULIP_DSTS_ERRSUM) {
			sc->tulip_if.if_oerrors++;
			if (nextin->d_status & TULIP_DSTS_TxEXCCOLL)
			    sc->tulip_dot3stats.dot3StatsExcessiveCollisions++;
			if (nextin->d_status & TULIP_DSTS_TxLATECOLL)
			    sc->tulip_dot3stats.dot3StatsLateCollisions++;
			if (nextin->d_status & (TULIP_DSTS_TxNOCARR|TULIP_DSTS_TxCARRLOSS))
			    sc->tulip_dot3stats.dot3StatsCarrierSenseErrors++;
			if (nextin->d_status & (TULIP_DSTS_TxUNDERFLOW|TULIP_DSTS_TxBABBLE))
			    sc->tulip_dot3stats.dot3StatsInternalMacTransmitErrors++;
		    } else {
			u_int32_t collisions = 
			    (nextin->d_status & TULIP_DSTS_TxCOLLMASK)
				>> TULIP_DSTS_V_TxCOLLCNT;
			sc->tulip_if.if_collisions += collisions;
			if (collisions == 1)
			    sc->tulip_dot3stats.dot3StatsSingleCollisionFrames++;
			else if (collisions > 1)
			    sc->tulip_dot3stats.dot3StatsMultipleCollisionFrames++;
			else if (nextin->d_status & TULIP_DSTS_TxDEFERRED)
			    sc->tulip_dot3stats.dot3StatsDeferredTransmissions++;
			/*
			 * SQE is only valid for 10baseT/BNC/AUI when not
			 * running in full-duplex.  In order to speed up the
			 * test, the corresponding bit in tulip_flags needs to
			 * set as well to get us to count SQE Test Errors.
			 */
			if (nextin->d_status & TULIP_DSTS_TxNOHRTBT & sc->tulip_flags)
			    sc->tulip_dot3stats.dot3StatsSQETestErrors++;
		    }
		}
	    }
	}

	if (++ri->ri_nextin == ri->ri_last)
	    ri->ri_nextin = ri->ri_first;

	ri->ri_free++;
	if ((sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0)
	    sc->tulip_if.if_flags &= ~IFF_OACTIVE;
    }
    /*
     * If nothing left to transmit, disable the timer.
     * Else if progress, reset the timer back to 2 ticks.
     */
    if (ri->ri_free == ri->ri_max || (sc->tulip_flags & TULIP_TXPROBE_ACTIVE))
	sc->tulip_txtimer = 0;
    else if (xmits > 0)
	sc->tulip_txtimer = TULIP_TXTIMER;
    sc->tulip_if.if_opackets += xmits;
    return xmits;
}

static void
tulip_print_abnormal_interrupt(
    tulip_softc_t * const sc,
    u_int32_t csr)
{
    const char * const *msgp = tulip_status_bits;
    const char *sep;

    csr &= (1 << (sizeof(tulip_status_bits)/sizeof(tulip_status_bits[0]))) - 1;
    printf(TULIP_PRINTF_FMT ": abnormal interrupt:", TULIP_PRINTF_ARGS);
    for (sep = " "; csr != 0; csr >>= 1, msgp++) {
	if ((csr & 1) && *msgp != NULL) {
	    printf("%s%s", sep, *msgp);
	    sep = ", ";
	}
    }
    printf("\n");
}

static void
tulip_intr_handler(
    tulip_softc_t * const sc,
    int *progress_p)
{
    u_int32_t csr;

    while ((csr = TULIP_CSR_READ(sc, csr_status)) & sc->tulip_intrmask) {
	*progress_p = 1;
	TULIP_CSR_WRITE(sc, csr_status, csr);

	if (csr & TULIP_STS_SYSERROR) {
	    sc->tulip_last_system_error = (csr & TULIP_STS_ERRORMASK) >> TULIP_STS_ERR_SHIFT;
	    if (sc->tulip_flags & TULIP_NOMESSAGES) {
		sc->tulip_flags |= TULIP_SYSTEMERROR;
	    } else {
		printf(TULIP_PRINTF_FMT ": system error: %s\n",
		       TULIP_PRINTF_ARGS,
		       tulip_system_errors[sc->tulip_last_system_error]);
	    }
	    sc->tulip_flags |= TULIP_NEEDRESET;
	    sc->tulip_system_errors++;
	    break;
	}
	if (csr & (TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL)) {
#if defined(TULIP_DEBUG)
	    sc->tulip_dbg.dbg_link_intrs++;
#endif
	    if (sc->tulip_boardsw->bd_media_poll != NULL) {
		(*sc->tulip_boardsw->bd_media_poll)(sc, csr & TULIP_STS_LINKFAIL
						    ? TULIP_MEDIAPOLL_LINKFAIL
						    : TULIP_MEDIAPOLL_LINKPASS);
		csr &= ~TULIP_STS_ABNRMLINTR;
	    }
	    tulip_media_print(sc);
	}
	if (csr & (TULIP_STS_RXINTR|TULIP_STS_RXNOBUF)) {
	    u_int32_t misses = TULIP_CSR_READ(sc, csr_missed_frames);
	    if (csr & TULIP_STS_RXNOBUF)
		sc->tulip_dot3stats.dot3StatsMissedFrames += misses & 0xFFFF;
	    /*
	     * Pass 2.[012] of the 21140A-A[CDE] may hang and/or corrupt data
	     * on receive overflows.
	     */
	   if ((misses & 0x0FFE0000) && (sc->tulip_features & TULIP_HAVE_RXBUGGY)) {
		/*
		 * Stop the receiver process and spin until it's stopped.
		 * Tell rx_intr to drop the packets it dequeues.
		 */
		TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode & ~TULIP_CMD_RXRUN);
		while ((TULIP_CSR_READ(sc, csr_status) & TULIP_STS_RXSTOPPED) == 0)
		    ;
		TULIP_CSR_WRITE(sc, csr_status, TULIP_STS_RXSTOPPED);
		sc->tulip_flags |= TULIP_RXBAD;
	    }
	    tulip_rx_intr(sc);
	    if (sc->tulip_flags & TULIP_RXBAD) {
		/*
		 * Restart the receiver.
		 */
		sc->tulip_flags &= ~TULIP_RXBAD;
		TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
	    }
	}
	if (csr & TULIP_STS_ABNRMLINTR) {
	    u_int32_t tmp = csr & sc->tulip_intrmask
		& ~(TULIP_STS_NORMALINTR|TULIP_STS_ABNRMLINTR);
	    if (sc->tulip_flags & TULIP_NOMESSAGES) {
		sc->tulip_statusbits |= tmp;
	    } else {
		tulip_print_abnormal_interrupt(sc, tmp);
		sc->tulip_flags |= TULIP_NOMESSAGES;
	    }
	    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
	}
	if (sc->tulip_txinfo.ri_free < sc->tulip_txinfo.ri_max) {
	    tulip_tx_intr(sc);
	    if ((sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0)
		tulip_ifstart(&sc->tulip_if);
	}
    }
    if (sc->tulip_flags & TULIP_NEEDRESET) {
	tulip_reset(sc);
	tulip_init(sc);
    }
}

#if defined(TULIP_USE_SOFTINTR)
/*
 * This is a experimental idea to alleviate problems due to interrupt
 * livelock.  What is interrupt livelock?  It's when you spend all your
 * time servicing device interrupts and never drop below device ipl
 * to do "useful" work.
 *
 * So what we do here is see if the device needs service and if so,
 * disable interrupts (dismiss the interrupt), place it in a list of devices
 * needing service, and issue a network software interrupt.
 *
 * When our network software interrupt routine gets called, we simply
 * walk done the list of devices that we have created and deal with them
 * at splnet/splsoftnet.
 *
 */
static void
tulip_hardintr_handler(
    tulip_softc_t * const sc,
    int *progress_p)
{
    if (TULIP_CSR_READ(sc, csr_status) & (TULIP_STS_NORMALINTR|TULIP_STS_ABNRMLINTR) == 0)
	return;
    *progress_p = 1;
    /*
     * disable interrupts
     */
    TULIP_CSR_WRITE(sc, csr_intr, 0);
    /*
     * mark it as needing a software interrupt
     */
    tulip_softintr_mask |= (1U << sc->tulip_unit);
}

static void
tulip_softintr(
    void)
{
    u_int32_t softintr_mask, mask;
    int progress = 0;
    int unit;
    tulip_spl_t s;

    /*
     * Copy mask to local copy and reset global one to 0.
     */
    s = TULIP_RAISESPL();
    softintr_mask = tulip_softintr_mask;
    tulip_softintr_mask = 0;
    TULIP_RESTORESPL(s);

    /*
     * Optimize for the single unit case.
     */
    if (tulip_softintr_max_unit == 0) {
	if (softintr_mask & 1) {
	    tulip_softc_t * const sc = TULIP_UNIT_TO_SOFTC(0);
	    /*
	     * Handle the "interrupt" and then reenable interrupts
	     */
	    softintr_mask = 0;
	    tulip_intr_handler(sc, &progress);
	    TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	}
	return;
    }

    /*
     * Handle all "queued" interrupts in a round robin fashion.
     * This is done so as not to favor a particular interface.
     */
    unit = tulip_softintr_last_unit;
    mask = (1U << unit);
    while (softintr_mask != 0) {
	if (tulip_softintr_max_unit == unit) {
	    unit  = 0; mask   = 1;
	} else {
	    unit += 1; mask <<= 1;
	}
	if (softintr_mask & mask) {
	    tulip_softc_t * const sc = TULIP_UNIT_TO_SOFTC(unit);
	    /*
	     * Handle the "interrupt" and then reenable interrupts
	     */
	    softintr_mask ^= mask;
	    tulip_intr_handler(sc, &progress);
	    TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	}
    }

    /*
     * Save where we ending up.
     */
    tulip_softintr_last_unit = unit;
}
#endif	/* TULIP_USE_SOFTINTR */

static tulip_intrfunc_t
tulip_intr_shared(
    void *arg)
{
    tulip_softc_t * sc;
    int progress = 0;

    for (sc = (tulip_softc_t *) arg; sc != NULL; sc = sc->tulip_slaves) {
#if defined(TULIP_DEBUG)
	sc->tulip_dbg.dbg_intrs++;
#endif
#if defined(TULIP_USE_SOFTINTR)
	tulip_hardintr_handler(sc, &progress);
#else
	tulip_intr_handler(sc, &progress);
#endif
    }
#if defined(TULIP_USE_SOFTINTR)
    if (progress)
	schednetisr(NETISR_DE);
#endif
#if !defined(TULIP_VOID_INTRFUNC)
    return progress;
#endif
}

static tulip_intrfunc_t
tulip_intr_normal(
    void *arg)
{
    tulip_softc_t * sc = (tulip_softc_t *) arg;
    int progress = 0;

#if defined(TULIP_DEBUG)
    sc->tulip_dbg.dbg_intrs++;
#endif
#if defined(TULIP_USE_SOFTINTR)
    tulip_hardintr_handler(sc, &progress);
    if (progress)
	schednetisr(NETISR_DE);
#else
    tulip_intr_handler(sc, &progress);
#endif
#if !defined(TULIP_VOID_INTRFUNC)
    return progress;
#endif
}

/*
 * This routine is entered at splnet() (splsoftnet() on NetBSD)
 * and thereby imposes no problems when TULIP_USE_SOFTINTR is 
 * defined or not.
 */
static int
tulip_ifioctl(
    struct ifnet * const ifp,
    ioctl_cmd_t cmd,
    void *data)
{
    tulip_softc_t * const sc = TULIP_IFP_TO_SOFTC(ifp);
    struct ifaddr *ifa = (struct ifaddr *)data;
    struct ifreq *ifr = (struct ifreq *) data;
    tulip_spl_t s;
    int error = 0;

#if defined(TULIP_USE_SOFTINTR)
    s = TULIP_RAISESOFTSPL();
#else
    s = TULIP_RAISESPL();
#endif
    switch (cmd) {
	case SIOCSIFADDR: {
	    ifp->if_flags |= IFF_UP;
	    switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET: {
		    tulip_init(sc);
		    TULIP_ARP_IFINIT(sc, ifa);
		    break;
		}
#endif /* INET */

#ifdef NS
		/*
		 * This magic copied from if_is.c; I don't use XNS,
		 * so I have no way of telling if this actually
		 * works or not.
		 */
		case AF_NS: {
		    struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);
		    if (ns_nullhost(*ina)) {
			ina->x_host = *(union ns_host *)(sc->tulip_enaddr);
		    } else {
			ifp->if_flags &= ~IFF_RUNNING;
			memcpy((void *)sc->tulip_enaddr,
			    (void *)ina->x_host.c_host,
			    sizeof(sc->tulip_enaddr));
		    }
		    tulip_init(sc);
		    break;
		}
#endif /* NS */

		default: {
		    tulip_init(sc);
		    break;
		}
	    }
	    break;
	}
	case SIOCGIFADDR: {
	    memcpy((void *) ((struct sockaddr *)&ifr->ifr_data)->sa_data,
		(void *) sc->tulip_enaddr, 6);
	    break;
	}

	case SIOCSIFFLAGS: {
	    tulip_init(sc);
	    break;
	}

#if defined(SIOCSIFMEDIA)
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA: {
	    error = ifmedia_ioctl(ifp, ifr, &sc->tulip_ifmedia, cmd);
	    break;
	}
#endif

	case SIOCADDMULTI:
	case SIOCDELMULTI: {
	    /*
	     * Update multicast listeners
	     */
	    if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING) {
		    tulip_addr_filter(sc);	/* reset multicast filtering */
		    tulip_init(sc);
		}
		error = 0;
	    }
	    break;
	}
#if defined(SIOCSIFMTU)
#if !defined(ifr_mtu)
#define ifr_mtu ifr_metric
#endif
	case SIOCSIFMTU:
	    /*
	     * Set the interface MTU.
	     */
	    if (ifr->ifr_mtu > ETHERMTU
#ifdef BIG_PACKET
		    && sc->tulip_chipid != TULIP_21140
		    && sc->tulip_chipid != TULIP_21140A
		    && sc->tulip_chipid != TULIP_21041
#endif
		) {
		error = EINVAL;
		break;
	    }
	    ifp->if_mtu = ifr->ifr_mtu;
#ifdef BIG_PACKET
	    tulip_reset(sc);
	    tulip_init(sc);
#endif
	    break;
#endif /* SIOCSIFMTU */

#ifdef SIOCGADDRROM
	case SIOCGADDRROM: {
	    error = copyout(sc->tulip_rombuf, ifr->ifr_data, sizeof(sc->tulip_rombuf));
	    break;
	}
#endif
#ifdef SIOCGCHIPID
	case SIOCGCHIPID: {
	    ifr->ifr_metric = (int) sc->tulip_chipid;
	    break;
	}
#endif
	default: {
	    error = EINVAL;
	    break;
	}
    }

    TULIP_RESTORESPL(s);
    return error;
}
#if 0
static void htnote(char * );
static void htnote(char *s) {
	if(hrh_trace)
		hrh_check_status(s);
}
#endif

/*
 * This routine gets called at device spl (from ether_output).  This might
 * pose a problem for TULIP_USE_SOFTINTR if ether_output is called at
 * device spl from another driver.
 */
static ifnet_ret_t
tulip_ifstart(
    struct ifnet * const ifp)
{
    tulip_softc_t * const sc = TULIP_IFP_TO_SOFTC(ifp);
    struct ifqueue * const ifq = &ifp->if_snd;
    tulip_ringinfo_t * const ri = &sc->tulip_txinfo;
    struct mbuf *m, *m0, *next_m0;

    if ((ifp->if_flags & IFF_RUNNING) == 0
	    && (sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0)
	return;

    for (;;) {
	tulip_desc_t *eop, *nextout;
	int segcnt, free, recopy;
	u_int32_t d_status;

	if ((sc->tulip_flags & (TULIP_WANTSETUP|TULIP_TXPROBE_ACTIVE)) == TULIP_WANTSETUP) {
#if defined(TULIP_DEBUG)
	    if ((sc->tulip_cmdmode & TULIP_CMD_TXRUN) == 0) {
		printf(TULIP_PRINTF_FMT ": ifstart: tx not running\n",
		       TULIP_PRINTF_ARGS);
		ifp->if_flags |= IFF_OACTIVE;
		return;
	    }
#endif
	    if ((sc->tulip_flags & TULIP_DOINGSETUP) || ri->ri_free == 1) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	    }
	    memcpy(sc->tulip_setupbuf, sc->tulip_setupdata,
		   sizeof(sc->tulip_setupdata));
	    sc->tulip_flags &= ~TULIP_WANTSETUP;
	    sc->tulip_flags |= TULIP_DOINGSETUP;
	    ri->ri_free--;
	    nextout = ri->ri_nextout;
	    BEGIN(nextout);
	    	t.d_flag &= TULIP_DFLAG_ENDRING|TULIP_DFLAG_CHAIN;
	    	t.d_flag |= TULIP_DFLAG_TxFIRSTSEG|TULIP_DFLAG_TxLASTSEG
		    |TULIP_DFLAG_TxSETUPPKT|TULIP_DFLAG_TxWANTINTR;
	    	if (sc->tulip_flags & TULIP_WANTHASH)
			t.d_flag |= TULIP_DFLAG_TxHASHFILT;
	    	t.d_length1 = sizeof(sc->tulip_setupdata);
#ifdef LCLDMA
	    	a12r2pb(sc->tulip_setupbuf,
			sc->tulip_tx_kva[nextout-ri->ri_first],
			sizeof(sc->tulip_setupbuf));
	    	if (a12map(TULIP_KVATOPHYS(
			sc, sc->tulip_tx_kva[nextout-ri->ri_first]))
		    != nextout->d_addr1)
			panic("tulip tx 1");
#else
	    	t.d_addr1 = TULIP_KVATOPHYS(sc, sc->tulip_setupbuf);
#endif
	    	t.d_length2 = 0;
	    	t.d_addr2 = 0;
	    END(nextout);
	    /*
	     * Advance the ring for the next transmit packet.
	     */
	    if (++ri->ri_nextout == ri->ri_last)
		ri->ri_nextout = ri->ri_first;
	    /*
	     * Make sure the next descriptor is owned by us since it
	     * may have been set up above if we ran out of room in the
	     * ring.
	     */
	    setstatus(ri->ri_nextout,0);
	    setstatus(nextout, TULIP_DSTS_OWNER);
	    TULIP_CSR_WRITE(sc, csr_txpoll, 1);
	}

	IF_DEQUEUE(ifq, m);
	if (m == NULL)
	    break;

#if defined(TULIP_DEBUG)
	if ((sc->tulip_cmdmode & TULIP_CMD_TXRUN) == 0) {
	    printf(TULIP_PRINTF_FMT ": ifstart%s: tx not running\n",
		   TULIP_PRINTF_ARGS,
		   (sc->tulip_flags & TULIP_TXPROBE_ACTIVE) ? "(probe)" : "");
	    ifp->if_flags |= IFF_OACTIVE;
	    IF_PREPEND(ifq, m);
	    return;
	}
#endif

	/*
	 * Now we try to fill in our transmit descriptors.  This is
	 * a bit reminiscent of going on the Ark two by two
	 * since each descriptor for the TULIP can describe
	 * two buffers.  So we advance through packet filling
	 * each of the two entries at a time to to fill each
	 * descriptor.  Clear the first and last segment bits
	 * in each descriptor (actually just clear everything
	 * but the end-of-ring or chain bits) to make sure
	 * we don't get messed up by previously sent packets.
	 *
	 * We may fail to put the entire packet on the ring if
	 * there is either not enough ring entries free or if the
	 * packet has more than MAX_TXSEG segments.  In the former
	 * case we will just wait for the ring to empty.  In the
	 * latter case we have to recopy.
	 */
	d_status = 0;
	recopy = 0;
	eop = nextout = ri->ri_nextout;
	m0 = m;
	segcnt = 0;
	free = ri->ri_free;
	/* Create a chain of descriptors tracking the chain of mbufs */
	do {
	    int len = m0->m_len;
	    void *addr = mtod(m0, void *);
	    unsigned clsize = PAGE_SIZE - (((u_long) addr) & PGOFSET);

	    next_m0 = m0->m_next;
	    while (len > 0) {
		unsigned slen = min(len, clsize);
#ifdef BIG_PACKET
		int partial = 0;
		if (slen >= 2048)
		    slen = 2040, partial = 1;
#endif
		segcnt++;
		if (segcnt > TULIP_MAX_TXSEG) {
		    recopy = 1;
		    next_m0 = NULL; /* to break out of outside loop */
		    break;
		}
		if (segcnt & 1) {
		    if (--free <= 0) {
			/*
			 * There's no more room but since nothing
			 * has been committed at this point, just
			 * show output is active, put back the
			 * mbuf and return.
			 */
			ifp->if_flags |= IFF_OACTIVE;
			IF_PREPEND(ifq, m);
			return;
		    }
		    eop = nextout;
		    if (++nextout == ri->ri_last)
			nextout = ri->ri_first;
		    BEGIN(eop);
		    	t.d_flag &= TULIP_DFLAG_ENDRING|TULIP_DFLAG_CHAIN;
		    	t.d_status = d_status;
#ifdef LCLDMA
		    	slen = m->m_pkthdr.len;
		    	if (slen > TULIP_TX_BUFLEN)
				panic("tulip lcldma tx 2");
		    	a12_m_copydata(m,0,slen,
					sc->tulip_tx_kva[eop - ri->ri_first]);
#if defined(TULIP_DEBUG)
		    	if (sc->tulip_tx_kva[eop - ri->ri_first]
		    	!= a12map(TULIP_KVATOPHYS(sc,eop->d_addr1)))
				panic("tulip lcldma tx 3");
#endif
		    	t.d_length1 = slen;
		    	next_m0 = NULL;
#else
		    	t.d_addr1 = TULIP_KVATOPHYS(sc, addr);
		    	t.d_length1 = slen;
#endif
		    END(eop);
		} else {
		    /*
		     *  Fill in second half of descriptor
		     */
		    if(cputype==ST_AVALON_A12)
			panic("tulip lcldma tx 3.5");
		    eop->d_addr2 = TULIP_KVATOPHYS(sc, addr);
		    eop->d_length2 = slen;
		}
		d_status = TULIP_DSTS_OWNER;
		len -= slen;
		addr += slen;
#ifdef BIG_PACKET
		if (partial)
		    continue;
#endif
		clsize = PAGE_SIZE;
	    }
	} while ((m0 = next_m0) != NULL);

	/*
	 * The packet exceeds the number of transmit buffer
	 * entries that we can use for one packet, so we have
	 * recopy it into one mbuf and then try again.
	 */
	if (recopy) {
	    MGETHDR(m0, M_DONTWAIT, MT_DATA);
	    if (m0 != NULL) {
		if (m->m_pkthdr.len > MHLEN) {
		    MCLGET(m0, M_DONTWAIT);
		    if ((m0->m_flags & M_EXT) == 0) {
			m_freem(m);
			m_freem(m0);
			continue;
		    }
		}
		m_copydata(m, 0, m->m_pkthdr.len, mtod(m0, void *));
		m0->m_pkthdr.len = m0->m_len = m->m_pkthdr.len;
		IF_PREPEND(ifq, m0);
	    }
	    m_freem(m);
	    continue;
	}

	/*
	 * The descriptors have been filled in.  Now get ready
	 * to transmit.
	 */
#if NBPFILTER > 0
	if (sc->tulip_bpf != NULL)
	    TULIP_BPF_MTAP(sc, m);
#endif
	IF_ENQUEUE(&sc->tulip_txq, m);

	/*
	 * Make sure the next descriptor after this packet is owned
	 * by us since it may have been set up above if we ran out
	 * of room in the ring.
	 */

	setstatus(nextout,0);

	/*
	 * If we only used the first segment of the last descriptor,
	 * make sure the second segment will not be used.
	 */
	BEGIN(eop);
	    if (segcnt & 1) {
		t.d_addr2 = 0;
		t.d_length2 = 0;
	    }

	    /*
	     * Mark the last and first segments, indicate we want a transmit
	     * complete interrupt, give the descriptors to the TULIP, and tell
	     * it to transmit!
	     */
	    t.d_flag |= TULIP_DFLAG_TxLASTSEG|TULIP_DFLAG_TxWANTINTR;
	END(eop);

	/*
	 * Note that ri->ri_nextout is still the start of the packet
	 * and until we set the OWNER bit, we can still back out of
	 * everything we have done.
	 */
	BEGIN(ri->ri_nextout);
		t.d_flag  |= TULIP_DFLAG_TxFIRSTSEG;
		t.d_status = TULIP_DSTS_OWNER;
	END(ri->ri_nextout);

	/*
	 * This advances the ring for us.
	 */
	ri->ri_nextout = nextout;
	ri->ri_free = free;

	TULIP_CSR_WRITE(sc, csr_txpoll, 1);

	if (sc->tulip_flags & TULIP_TXPROBE_ACTIVE) {
	    ifp->if_flags |= IFF_OACTIVE;
	    return;
	}
	if (sc->tulip_txtimer == 0)
	    sc->tulip_txtimer = TULIP_TXTIMER;
    }
    if (m != NULL) {
	ifp->if_flags |= IFF_OACTIVE;
	IF_PREPEND(ifq, m);
    }
}

/*
 * Even though this routine runs at device spl, it does not break
 * our use of splnet (splsoftnet under NetBSD) for the majority
 * of this driver (if TULIP_USE_SOFTINTR defined) since 
 * if_watcbog is called from if_watchdog which is called from
 * splsoftclock which is below spl[soft]net.
 */
static void
tulip_ifwatchdog(
    struct ifnet *ifp)
{
    tulip_softc_t * const sc = TULIP_IFP_TO_SOFTC(ifp);

#if defined(TULIP_DEBUG)
    u_int32_t rxintrs = sc->tulip_dbg.dbg_rxintrs - sc->tulip_dbg.dbg_last_rxintrs;
    if (rxintrs > sc->tulip_dbg.dbg_high_rxintrs_hz)
	sc->tulip_dbg.dbg_high_rxintrs_hz = rxintrs;
    sc->tulip_dbg.dbg_last_rxintrs = sc->tulip_dbg.dbg_rxintrs;
#endif /* TULIP_DEBUG */

    sc->tulip_if.if_timer = 1;
    /*
     * These should be rare so do a bulk test up front so we can just skip
     * them if needed.
     */
    if (sc->tulip_flags & (TULIP_SYSTEMERROR|TULIP_RXBUFSLOW|TULIP_NOMESSAGES)) {
	/*
	 * If the number of receive buffer is low, try to refill
	 */
	if (sc->tulip_flags & TULIP_RXBUFSLOW)
	    tulip_rx_intr(sc);

	if (sc->tulip_flags & TULIP_SYSTEMERROR) {
	    printf(TULIP_PRINTF_FMT ": %d system errors: last was %s\n",
		   TULIP_PRINTF_ARGS, sc->tulip_system_errors,
		   tulip_system_errors[sc->tulip_last_system_error]);
	}
	if (sc->tulip_statusbits) {
	    tulip_print_abnormal_interrupt(sc, sc->tulip_statusbits);
	    sc->tulip_statusbits = 0;
	}

	sc->tulip_flags &= ~(TULIP_NOMESSAGES|TULIP_SYSTEMERROR);
    }

    if (sc->tulip_txtimer && --sc->tulip_txtimer == 0) {
	printf(TULIP_PRINTF_FMT ": transmission timeout\n", TULIP_PRINTF_ARGS);
	if (TULIP_DO_AUTOSENSE(sc)) {
	    sc->tulip_media = TULIP_MEDIA_UNKNOWN;
	    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
	    sc->tulip_flags &= ~(TULIP_WANTRXACT|TULIP_LINKUP);
	}
	tulip_reset(sc);
	tulip_init(sc);
    }
}
#if defined(__bsdi__) || (defined(__FreeBSD__) && BSD < 199506)
static ifnet_ret_t
tulip_ifwatchdog_wrapper(
    int unit)
{
    tulip_ifwatchdog(&TULIP_UNIT_TO_SOFTC(unit)->tulip_if);
}
#define	tulip_ifwatchdog	tulip_ifwatchdog_wrapper
#endif

/*
 * All printf's are real as of now!
 */
#ifdef printf
#undef printf
#endif
#if !defined(IFF_NOTRAILERS)
#define IFF_NOTRAILERS		0
#endif

static void
tulip_attach(
    tulip_softc_t * const sc)
{
    struct ifnet * const ifp = &sc->tulip_if;

    ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_NOTRAILERS|IFF_MULTICAST;
    ifp->if_ioctl = tulip_ifioctl;
    ifp->if_start = tulip_ifstart;
    ifp->if_watchdog = tulip_ifwatchdog;
    ifp->if_timer = 1;
#if (!defined(__bsdi__) || _BSDI_VERSION < 199401) && !defined(__NetBSD__)
    ifp->if_output = ether_output;
#endif
#if defined(__bsdi__) && _BSDI_VERSION < 199401
    ifp->if_mtu = ETHERMTU;
#endif
  
#if defined(__bsdi__) && _BSDI_VERSION >= 199510
    aprint_naive(": DEC Ethernet");
    aprint_normal(": %s%s", sc->tulip_boardid,
        tulip_chipdescs[sc->tulip_chipid]);
    aprint_verbose(" pass %d.%d", (sc->tulip_revinfo & 0xF0) >> 4,
        sc->tulip_revinfo & 0x0F);
    printf("\n");
    sc->tulip_pf = aprint_normal;
    aprint_normal(TULIP_PRINTF_FMT ": address " TULIP_EADDR_FMT "\n",
		  TULIP_PRINTF_ARGS,
		  TULIP_EADDR_ARGS(sc->tulip_enaddr));
#else
    printf(
#if defined(__bsdi__)
	   "\n"
#endif
	   TULIP_PRINTF_FMT ": %s%s pass %d.%d\n",
	   TULIP_PRINTF_ARGS,
	   sc->tulip_boardid,
	   tulip_chipdescs[sc->tulip_chipid],
	   (sc->tulip_revinfo & 0xF0) >> 4,
	   sc->tulip_revinfo & 0x0F);
    printf(TULIP_PRINTF_FMT ": address " TULIP_EADDR_FMT "\n",
	   TULIP_PRINTF_ARGS,
	   TULIP_EADDR_ARGS(sc->tulip_enaddr));
#endif

#if defined(__alpha__)
    /*
     * In case the SRM console told us about a bogus media,
     * we need to check to be safe.
     */
    if (sc->tulip_mediums[sc->tulip_media] == NULL) {
	sc->tulip_media = TULIP_MEDIA_UNKNOWN;
    }
#endif

    ifmedia_init(&sc->tulip_ifmedia, 0,
		 tulip_ifmedia_change,
		 tulip_ifmedia_status);
    (*sc->tulip_boardsw->bd_media_probe)(sc);
    sc->tulip_flags &= ~TULIP_DEVICEPROBE;
    tulip_ifmedia_add(sc);

    tulip_reset(sc);

#if defined(__bsdi__) && _BSDI_VERSION >= 199510
    sc->tulip_pf = printf;
    TULIP_ETHER_IFATTACH(sc);
#else
    if_attach(ifp);
#if defined(__NetBSD__) || (defined(__FreeBSD__) && BSD >= 199506)
    TULIP_ETHER_IFATTACH(sc);
#endif
#endif /* __bsdi__ */
}

static void
tulip_initcsrs(
    tulip_softc_t * const sc,
    tulip_csrptr_t csr_base,
    size_t csr_size)
{
    sc->tulip_csrs.csr_busmode		= csr_base +  0 * csr_size;
    sc->tulip_csrs.csr_txpoll		= csr_base +  1 * csr_size;
    sc->tulip_csrs.csr_rxpoll		= csr_base +  2 * csr_size;
    sc->tulip_csrs.csr_rxlist		= csr_base +  3 * csr_size;
    sc->tulip_csrs.csr_txlist		= csr_base +  4 * csr_size;
    sc->tulip_csrs.csr_status		= csr_base +  5 * csr_size;
    sc->tulip_csrs.csr_command		= csr_base +  6 * csr_size;
    sc->tulip_csrs.csr_intr		= csr_base +  7 * csr_size;
    sc->tulip_csrs.csr_missed_frames	= csr_base +  8 * csr_size;
    sc->tulip_csrs.csr_9		= csr_base +  9 * csr_size;
    sc->tulip_csrs.csr_10		= csr_base + 10 * csr_size;
    sc->tulip_csrs.csr_11		= csr_base + 11 * csr_size;
    sc->tulip_csrs.csr_12		= csr_base + 12 * csr_size;
    sc->tulip_csrs.csr_13		= csr_base + 13 * csr_size;
    sc->tulip_csrs.csr_14		= csr_base + 14 * csr_size;
    sc->tulip_csrs.csr_15		= csr_base + 15 * csr_size;
#if defined(TULIP_EISA)
    sc->tulip_csrs.csr_enetrom		= csr_base + DE425_ENETROM_OFFSET;
#endif
}

/* XXX. In the long run, these don't belong here in a driver */

void *lcl_dma_ram_next(int);
void  lcl_dma_restart(void);

void lcl_dma_restart(void) {
	/* lcl_dma_ram_next(-1); */
}

#include <alpha/pci/a12creg.h>

static void *	LCLDMAstart = (void *)ALPHA_PHYS_TO_K0SEG(A12_PCIBuffer);
static int	lclpcisize  = 128*1024;

void *
lcl_dma_ram_next(int size)
{
	static	void *first;
	void	*t;

	if (first==0L /* || size==-1 */) {
		first = LCLDMAstart;
	}
	t      = first;
	first += size;
	if (first > LCLDMAstart + lclpcisize) {
#ifdef TULIP_DEBUG
		printf("first=%p LCLDMAstart=%p lclpcisize=%p\n",
			first, LCLDMAstart, lclpcisize);
#endif
		panic("tulip lcldma next");
	}
	return t;
}

static char *lcl_dma_retrace;

void lcl_dma_start_check(void);
void
lcl_dma_start_check(void)
{
	lcl_dma_retrace = LCLDMAstart;
}

void *lcl_dma_ram_next_check(int);
void *
lcl_dma_ram_next_check(int size)
{
	void	*t = lcl_dma_retrace;
	lcl_dma_retrace += size;
	return   t;
}

/*
 * For LCLDMA, we need to use local memory for both the descriptors
 * and for the buffers, and we need to easily be able to recall the
 * buffer addresses.
 */
#ifdef LCLDMA
tulip_softc_t *debug_sc;
#if 0
static void a12pbs(tulip_softc_t * const sc, u_int32_t *base, void *kva) {
	u_int32_t p;

	p = a12map(TULIP_KVATOPHYS(sc,kva));
	a12r2pb(&p, base, sizeof(p));
}
#endif

static void
tulip_initring(
    tulip_softc_t * const sc,
    tulip_ringinfo_t * const ri,
    tulip_desc_t *descs,
    void **kvalist,
    int ndescs)
{
    int i, dsize  = sizeof descs[0] * ndescs;

    debug_sc = sc;
    ri->ri_max = ndescs;
    (void)lcl_dma_ram_next(16);	/* superstitiously avoid the origin */
    ri->ri_first = (tulip_desc_t *)lcl_dma_ram_next(dsize);
    memset(ri->ri_first, 0, dsize);
    for(i=0; i<ndescs; ++i) {
	    kvalist[i] = lcl_dma_ram_next(TULIP_XX_BUFLEN);
	    BEGIN(ri->ri_first+i);
		memset(&t, 0, sizeof(t));
		t.d_addr1 = a12map(TULIP_KVATOPHYS(sc,kvalist[i]));
	    END(ri->ri_first+i);
    }
    ri->ri_last = ri->ri_first + ri->ri_max;
    BEGIN(ri->ri_last-1);
	t.d_flag = TULIP_DFLAG_ENDRING;
    END(ri->ri_last-1);
    dumpring(kvalist);
}

static void dumpring(void **kvalist) {
#if 0
	long *d = (long *)0xfffffc8020000000L;
	int k;

	for(k=0; k<10; ++k)
		printf("kvalist[%d] = %16lx\n", k, kvalist[k]);
	for(k=0; k<20; ++k)
		printf("%03x: %16lx %16lx %16lx %16lx\n",
			k*4*8,
			d[k*4],
			d[k*4+1],
			d[k*4+2],
			d[k*4+3]);
#endif
}


/*
 * This doesn't do anything other than check that the ring hasn't been
 * changed since tulip_pci_attach()
 */
static void
tulip_ring_reset(
	tulip_softc_t * const sc,
	tulip_ringinfo_t * const ri,
	tulip_desc_t *descs,
	void **kvalist,
	int ndescs)
{
	int i;
	int q;
	tulip_desc_t *t;

	t = 0;	/* -Wall */
	for(i=q=0; i<ndescs; ++i) {
		if (q) {
			printf("d_flag %d = %x\n", i-1, t->d_flag);
			goto die;
		}
		t = ri->ri_first + i;
		if (t->d_addr1 != a12map(TULIP_KVATOPHYS(sc,kvalist[i]))) {
			printf("&a1=%lx t->d_addr1=%x a12map(...)=%x\n",
				&t->d_addr1,
				 t->d_addr1,
				a12map(TULIP_KVATOPHYS(sc,kvalist[i])));
			dumpring(kvalist);
			goto die;
		}
		if (t->d_addr2) {
			printf("t->d_addr2=%x\n",t->d_addr2);
			goto die;
		}
		q = t->d_flag & TULIP_DFLAG_ENDRING;
	}
	if (q)
		return;
	printf("final d_flag = %x\n", t->d_flag);
die:	panic("tulip ring corrupted");
}

static void
reset_lcl_dma(tulip_softc_t * const sc)
{
    tulip_ring_reset(sc, &sc->tulip_rxinfo, sc->tulip_rxdescs, sc->tulip_rx_kva,
	TULIP_RXDESCS);
    tulip_ring_reset(sc, &sc->tulip_txinfo, sc->tulip_txdescs, sc->tulip_tx_kva,
	TULIP_TXDESCS);
}
#else
static void
tulip_initring(		/* the usual case */
    tulip_softc_t * const sc,
    tulip_ringinfo_t * const ri,
    tulip_desc_t *descs,
    void **kvalist;
    int ndescs)
{
    ri->ri_max = ndescs;
    ri->ri_first = descs;
    ri->ri_last = ri->ri_first + ri->ri_max;
    memset((void *) ri->ri_first, 0, sizeof(ri->ri_first[0]) * ri->ri_max);
    ri->ri_last[-1].d_flag = TULIP_DFLAG_ENDRING;
}
#endif

/*
 * This is the PCI configuration support.  Since the 21040 is available
 * on both EISA and PCI boards, one must be careful in how defines the
 * 21040 in the config file.
 */

#define	PCI_CFID	0x00	/* Configuration ID */
#define	PCI_CFCS	0x04	/* Configurtion Command/Status */
#define	PCI_CFRV	0x08	/* Configuration Revision */
#define	PCI_CFLT	0x0c	/* Configuration Latency Timer */
#define	PCI_CBIO	0x10	/* Configuration Base IO Address */
#define	PCI_CBMA	0x14	/* Configuration Base Memory Address */
#define	PCI_CFIT	0x3c	/* Configuration Interrupt */
#define	PCI_CFDA	0x40	/* Configuration Driver Area */

#if defined(TULIP_EISA)
static const int tulip_eisa_irqs[4] = { IRQ5, IRQ9, IRQ10, IRQ11 };
#endif

#if defined(__FreeBSD__)

#define	TULIP_PCI_ATTACH_ARGS	pcici_t config_id, int unit
#define	TULIP_SHUTDOWN_ARGS	int howto, void *arg

#if defined(TULIP_DEVCONF)
static void tulip_shutdown(TULIP_SHUTDOWN_ARGS);

static int
tulip_pci_shutdown(
    struct kern_devconf * const kdc,
    int force)
{
    if (kdc->kdc_unit < TULIP_MAX_DEVICES) {
	tulip_softc_t * const sc = TULIP_UNIT_TO_SOFTC(kdc->kdc_unit);
	if (sc != NULL)
	    tulip_shutdown(0, sc);
    }
    (void) dev_detach(kdc);
    return 0;
}
#endif

static char*
tulip_pci_probe(
    pcici_t config_id,
    pcidi_t device_id)
{
    if (cputype != ST_AVALON_A12)	/* XXX */
	return NULL;
    if (PCI_VENDORID(device_id) != DEC_VENDORID)
	return NULL;
    if (PCI_CHIPID(device_id) == CHIPID_21040)
	return "Digital 21040 Ethernet";
    if (PCI_CHIPID(device_id) == CHIPID_21041)
	return "Digital 21041 Ethernet";
    if (PCI_CHIPID(device_id) == CHIPID_21140) {
	u_int32_t revinfo = pci_conf_read(config_id, PCI_CFRV) & 0xFF;
	if (revinfo >= 0x20)
	    return "Digital 21140A Fast Ethernet";
	else
	    return "Digital 21140 Fast Ethernet";
    }
    if (PCI_CHIPID(device_id) == CHIPID_21142) {
	u_int32_t revinfo = pci_conf_read(config_id, PCI_CFRV) & 0xFF;
	if (revinfo >= 0x20)
	    return "Digital 21143 Fast Ethernet";
	else
	    return "Digital 21142 Fast Ethernet";
    }
    return NULL;
}

static void  tulip_pci_attach(TULIP_PCI_ATTACH_ARGS);
static u_long tulip_pci_count;

struct pci_device adedevice = {
    "ade",
    tulip_pci_probe,
    tulip_pci_attach,
   &tulip_pci_count,
#if defined(TULIP_DEVCONF)
    tulip_pci_shutdown,
#endif
};

DATA_SET (pcidevice_set, adedevice);
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
#define	TULIP_PCI_ATTACH_ARGS	struct device * const parent, struct device * const self, void *const aux
#define	TULIP_SHUTDOWN_ARGS	void *arg

static int
tulip_pci_match(
    pci_devaddr_t *pa)
{
    int irq;
    unsigned id;

    id = pci_inl(pa, PCI_VENDOR_ID);
    if (PCI_VENDORID(id) != DEC_VENDORID)
	return 0;
    id = PCI_CHIPID(id);
    if (id != CHIPID_21040 && id != CHIPID_21041
	    && id != CHIPID_21140 && id != CHIPID_21142)
	return 0;
    irq = pci_inl(pa, PCI_I_LINE) & 0xFF;
    if (irq == 0 || irq >= 16) {
	printf("ade?: invalid IRQ %d; skipping\n", irq);
	return 0;
    }
    return 1;
}

static int
tulip_probe(
    struct device *parent,
    struct cfdata *cf,
    void *aux)
{
    struct isa_attach_args * const ia = (struct isa_attach_args *) aux;
    unsigned irq, slot;
    pci_devaddr_t *pa;

#if _BSDI_VERSION >= 199401
    switch (ia->ia_bustype) {
    case BUS_PCI:
#endif
	pa = pci_scan(tulip_pci_match);
	if (pa == NULL)
	    return 0;

	irq = (1 << (pci_inl(pa, PCI_I_LINE) & 0xFF));

	/* Get the base address; assume the BIOS set it up correctly */
#if defined(TULIP_IOMAPPED)
	ia->ia_maddr = NULL;
	ia->ia_msize = 0;
	ia->ia_iobase = pci_inl(pa, PCI_CBIO) & ~7;
	pci_outl(pa, PCI_CBIO, 0xFFFFFFFF);
	ia->ia_iosize = ((~pci_inl(pa, PCI_CBIO)) | 7) + 1;
	pci_outl(pa, PCI_CBIO, (int) ia->ia_iobase);

	/* Disable memory space access */
	pci_outl(pa, PCI_COMMAND, pci_inl(pa, PCI_COMMAND) & ~2);
#else
	ia->ia_maddr = (void *) (pci_inl(pa, PCI_CBMA) & ~7);
	pci_outl(pa, PCI_CBMA, 0xFFFFFFFF);
	ia->ia_msize = ((~pci_inl(pa, PCI_CBMA)) | 7) + 1;
	pci_outl(pa, PCI_CBMA, (int) ia->ia_maddr);
	ia->ia_iobase = 0;
	ia->ia_iosize = 0;

	/* Disable I/O space access */
	pci_outl(pa, PCI_COMMAND, pci_inl(pa, PCI_COMMAND) & ~1);
#endif /* TULIP_IOMAPPED */

	ia->ia_aux = (void *) pa;
#if _BSDI_VERSION >= 199401
	break;

#if defined(TULIP_EISA)
    case BUS_EISA: {
	unsigned tmp;

	if ((slot = eisa_match(cf, ia)) == 0)
	    return 0;
	ia->ia_iobase = slot << 12;
	ia->ia_iosize = EISA_NPORT;
	eisa_slotalloc(slot);
	tmp = inb(ia->ia_iobase + DE425_CFG0);
	irq = tulip_eisa_irqs[(tmp >> 1) & 0x03];
	/*
	 * Until BSD/OS likes level interrupts, force
	 * the DE425 into edge-triggered mode.
	 */
	if ((tmp & 1) == 0)
	    outb(ia->ia_iobase + DE425_CFG0, tmp | 1);
	/*
	 * CBIO needs to map to the EISA slot
	 * enable I/O access and Master
	 */
	outl(ia->ia_iobase + DE425_CBIO, ia->ia_iobase);
	outl(ia->ia_iobase + DE425_CFCS, 5 | inl(ia->ia_iobase + DE425_CFCS));
	ia->ia_aux = NULL;
	break;
    }
#endif /* TULIP_EISA */
    default:
	return 0;
    }
#endif

    /* PCI bus masters don't use host DMA channels */
    ia->ia_drq = DRQNONE;

    if (ia->ia_irq != IRQUNK && irq != ia->ia_irq) {
	printf("ade%d: error: desired IRQ of %d does not match device's "
	    "actual IRQ of %d,\n",
	       cf->cf_unit,
	       ffs(ia->ia_irq) - 1, ffs(irq) - 1);
	return 0;
    }
    if (ia->ia_irq == IRQUNK)
	ia->ia_irq = irq;
#ifdef IRQSHARE
    ia->ia_irq |= IRQSHARE;
#endif
    return 1;
}

static void tulip_pci_attach(TULIP_PCI_ATTACH_ARGS);

#if defined(TULIP_EISA)
static char *tulip_eisa_ids[] = {
    "DEC4250",
    NULL
};
#endif

struct cfdriver decd = {
    0, "ade", tulip_probe, tulip_pci_attach,
#if _BSDI_VERSION >= 199401
    DV_IFNET,
#endif
    sizeof(tulip_softc_t),
#if defined(TULIP_EISA)
    tulip_eisa_ids
#endif
};

#endif /* __bsdi__ */

#if defined(__NetBSD__)
#define	TULIP_PCI_ATTACH_ARGS	struct device * const parent, struct device * const self, void *const aux
#define	TULIP_SHUTDOWN_ARGS	void *arg
static int
tulip_pci_probe(
    struct device *parent,
    struct cfdata *match,
    void *aux)
{
    struct pci_attach_args *pa = (struct pci_attach_args *) aux;

    if (PCI_VENDORID(pa->pa_id) != DEC_VENDORID)
	return 0;
    if (PCI_CHIPID(pa->pa_id) == CHIPID_21040
	    || PCI_CHIPID(pa->pa_id) == CHIPID_21041
	    || PCI_CHIPID(pa->pa_id) == CHIPID_21140
	    || PCI_CHIPID(pa->pa_id) == CHIPID_21142)
	return 1;

    return 0;
}

static void tulip_pci_attach(TULIP_PCI_ATTACH_ARGS);

CFATTACH_DECL(ade, sizeof(tulip_softc_t),
    tulip_pci_probe, tulip_pci_attach, NULL, NULL);

#endif /* __NetBSD__ */

static void
tulip_shutdown(
    TULIP_SHUTDOWN_ARGS)
{
    tulip_softc_t * const sc = arg;
    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
		   33MHz that comes to two microseconds but wait a
		   bit longer anyways) */
}

static void
tulip_pci_attach(
    TULIP_PCI_ATTACH_ARGS)
{
#if defined(__FreeBSD__)
    tulip_softc_t *sc;
#define	PCI_CONF_WRITE(r, v)	pci_conf_write(config_id, (r), (v))
#define	PCI_CONF_READ(r)	pci_conf_read(config_id, (r))
#define	PCI_GETBUSDEVINFO(sc)	((void)((sc)->tulip_pci_busno = ((config_id.cfg1 >> 16) & 0xFF), /* XXX */ \
					(sc)->tulip_pci_devno = ((config_id.cfg1 >> 11) & 0x1F))) /* XXX */
#endif
#if defined(__bsdi__)
    tulip_softc_t * const sc = (tulip_softc_t *) self;
    struct isa_attach_args * const ia = (struct isa_attach_args *) aux;
    pci_devaddr_t *pa = (pci_devaddr_t *) ia->ia_aux;
    const int unit = sc->tulip_dev.dv_unit;
#define	PCI_CONF_WRITE(r, v)	pci_outl(pa, (r), (v))
#define	PCI_CONF_READ(r)	pci_inl(pa, (r))
#define	PCI_GETBUSDEVINFO(sc)	((void)((sc)->tulip_pci_busno = pa->d_bus, \
					(sc)->tulip_pci_devno = pa->d_agent))
#endif
#if defined(__NetBSD__)
    tulip_softc_t * const sc = (tulip_softc_t *) self;
    struct pci_attach_args * const pa = (struct pci_attach_args *) aux;
    const int unit = device_unit(&sc->tulip_dev);
#ifdef THIS_IS_THE_ORIGINAL_HRH_DRIVER_USING_THE_NOW_DEFUNCT_PCI_MEM_FIND
    bus_addr_t regbase;
    bus_size_t regsize;
    int cacheable;
#else
#ifdef PCI_PREFER_IOSPACE
	error error error error
#endif
#endif
#define	PCI_CONF_WRITE(r, v)	pci_conf_write(pa->pa_pc, pa->pa_tag, (r), (v))
#define	PCI_CONF_READ(r)	pci_conf_read(pa->pa_pc, pa->pa_tag, (r))
#define	PCI_GETBUSDEVINFO(sc)	do { \
	int busno, devno, funcno; \
	pci_decompose_tag(pa->pa_pc, pa->pa_tag, &busno, &devno, &funcno); \
	(sc)->tulip_pci_busno = busno; \
	(sc)->tulip_pci_devno = devno; \
    } while (0)
#endif /* __NetBSD__ */
    int retval, idx;
    u_int32_t revinfo, cfdainfo, id;
#if !defined(TULIP_IOMAPPED) && defined(__FreeBSD__)
    paddr_t pa_csrs;
#endif
    unsigned csroffset = TULIP_PCI_CSROFFSET;
    unsigned csrsize = TULIP_PCI_CSRSIZE;
    tulip_csrptr_t csr_base;
    tulip_chipid_t chipid = TULIP_CHIPID_UNKNOWN;

    if (unit >= TULIP_MAX_DEVICES) {
#ifdef __FreeBSD__
	printf("ade%d", unit);
#endif
	printf(": not configured; limit of %d reached or exceeded\n",
	       TULIP_MAX_DEVICES);
	return;
    }

#if defined(__bsdi__)
    if (pa != NULL) {
	revinfo = pci_inl(pa, PCI_CFRV) & 0xFF;
	id = pci_inl(pa, PCI_CFID);
	cfdainfo = pci_inl(pa, PCI_CFDA);
#if defined(TULIP_EISA)
    } else {
	revinfo = inl(ia->ia_iobase + DE425_CFRV) & 0xFF;
	csroffset = TULIP_EISA_CSROFFSET;
	csrsize = TULIP_EISA_CSRSIZE;
	chipid = TULIP_DE425;
	cfdainfo = 0;
#endif /* TULIP_EISA */
    }
#else /* __bsdi__ */
    revinfo  = PCI_CONF_READ(PCI_CFRV) & 0xFF;
    id       = PCI_CONF_READ(PCI_CFID);
    cfdainfo = PCI_CONF_READ(PCI_CFDA);
#endif /* __bsdi__ */

    if (PCI_VENDORID(id) == DEC_VENDORID) {
	if (PCI_CHIPID(id) == CHIPID_21040) chipid = TULIP_21040;
	else if (PCI_CHIPID(id) == CHIPID_21140) {
	    chipid = (revinfo >= 0x20) ? TULIP_21140A : TULIP_21140;
	} else if (PCI_CHIPID(id) == CHIPID_21142) {
	    chipid = (revinfo >= 0x20) ? TULIP_21143 : TULIP_21142;
	}
	else if (PCI_CHIPID(id) == CHIPID_21041) chipid = TULIP_21041;
	else if (PCI_CHIPID(id) == CHIPID_21142) chipid = TULIP_21142;
    }
    if (chipid == TULIP_CHIPID_UNKNOWN)
	return;

    if ((chipid == TULIP_21040 || chipid == TULIP_DE425) && revinfo < 0x20) {
#ifdef __FreeBSD__
	printf("ade%d", unit);
#endif
	printf(": not configured; 21040 pass 2.0 required (%d.%d found)\n",
	       revinfo >> 4, revinfo & 0x0f);
	return;
    } else if (chipid == TULIP_21140 && revinfo < 0x11) {
#ifndef __FreeBSD__
	printf("\n");
#endif
	printf("ade%d: not configured; 21140 pass 1.1 required (%d.%d found)\n",
	       unit, revinfo >> 4, revinfo & 0x0f);
	return;
    }

#if defined(__FreeBSD__)
    sc = (tulip_softc_t *) malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT);
    if (sc == NULL)
	return;
    bzero(sc, sizeof(*sc));				/* Zero out the softc*/
#endif

    PCI_GETBUSDEVINFO(sc);
    sc->tulip_chipid = chipid;
    sc->tulip_flags |= TULIP_DEVICEPROBE;
    if (chipid == TULIP_21140 || chipid == TULIP_21140A)
	sc->tulip_features |= TULIP_HAVE_GPR;
    if (chipid == TULIP_21140A && revinfo <= 0x22)
	sc->tulip_features |= TULIP_HAVE_RXBUGGY;
    if (chipid == TULIP_21140)
	sc->tulip_features |= TULIP_HAVE_BROKEN_HASH;
    if (chipid != TULIP_21040 && chipid != TULIP_DE425 && chipid != TULIP_21140)
	sc->tulip_features |= TULIP_HAVE_POWERMGMT;
    if (chipid == TULIP_21041 || chipid == TULIP_21142 || chipid == TULIP_21143) {
	sc->tulip_features |= TULIP_HAVE_DUALSENSE;
	if (chipid != TULIP_21041 || sc->tulip_revinfo >= 0x20)
	    sc->tulip_features |= TULIP_HAVE_SIANWAY;
	if (chipid != TULIP_21041)
	    sc->tulip_features |= TULIP_HAVE_SIAGP;
    }

    if (sc->tulip_features & TULIP_HAVE_POWERMGMT
	    && (cfdainfo & (TULIP_CFDA_SLEEP|TULIP_CFDA_SNOOZE))) {
	cfdainfo &= ~(TULIP_CFDA_SLEEP|TULIP_CFDA_SNOOZE);
	PCI_CONF_WRITE(PCI_CFDA, cfdainfo);
	DELAY(11*1000);
    }
#if defined(__alpha__)
    /*
     * The Alpha SRM console encodes a console set media in the driver
     * part of the CFDA register.  Note that the Multia presents a
     * problem in that its BNC mode is really EXTSIA.  So in that case
     * force a probe.
     */
    switch ((cfdainfo >> 8) & 0xff) {
    case 1: sc->tulip_media = chipid > TULIP_DE425 ? TULIP_MEDIA_AUI : TULIP_MEDIA_AUIBNC;
	break;
    case 2: sc->tulip_media = chipid > TULIP_DE425 ? TULIP_MEDIA_BNC : TULIP_MEDIA_UNKNOWN;
	break;
    case 3: sc->tulip_media = TULIP_MEDIA_10BASET;	break;
    case 4: sc->tulip_media = TULIP_MEDIA_10BASET_FD;	break;
    case 5: sc->tulip_media = TULIP_MEDIA_100BASETX;	break;
    case 6: sc->tulip_media = TULIP_MEDIA_100BASETX_FD;	break;
    }
#endif


#if defined(__NetBSD__)
    strcpy(sc->tulip_if.if_xname, self->dv_xname);
    sc->tulip_if.if_softc = sc;
    sc->tulip_pc = pa->pa_pc;
#else
    sc->tulip_unit = unit;
    sc->tulip_name = "ade";
#endif
    sc->tulip_revinfo = revinfo;
#if defined(__FreeBSD__)
#if BSD >= 199506
    sc->tulip_if.if_softc = sc;
#endif
#if defined(TULIP_IOMAPPED)
    retval = pci_map_port(config_id, PCI_CBIO, &csr_base);
#else
    retval = pci_map_mem(config_id, PCI_CBMA, (vaddr_t *) &csr_base, &pa_csrs);
#endif
    if (!retval) {
	free((void *) sc, M_DEVBUF);
	return;
    }
    tulips[unit] = sc;
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
    sc->tulip_pf = printf;
#if defined(TULIP_IOMAPPED)
    csr_base = ia->ia_iobase;
#else
    csr_base = (vaddr_t) mapphys((vaddr_t) ia->ia_maddr, ia->ia_msize);
#endif
#endif /* __bsdi__ */

#if defined(__NetBSD__)
    csr_base = 0;
#ifdef THIS_IS_THE_ORIGINAL_HRH_DRIVER_USING_THE_NOW_DEFUNCT_PCI_MEM_FIND
#if defined(TULIP_IOMAPPED)
    sc->tulip_bustag = pa->pa_iot;
    cacheable = 0;
    if (pci_io_find(pa->pa_pc, pa->pa_tag, PCI_CBIO, &regbase, &regsize))
	return;
#else
    sc->tulip_bustag = pa->pa_memt;
    if (pci_mem_find(pa->pa_pc, pa->pa_tag, PCI_CBMA, &regbase, &regsize, &cacheable))
        return;
#endif
    if (bus_space_map(sc->tulip_bustag, regbase, regsize, cacheable, &sc->tulip_bushandle))
	return;
#else
    {
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	u_int32_t cfcs = PCI_CONF_READ(PCI_CFCS);

	cfcs &= ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE);
	if (!pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
			    &iot, &ioh, NULL, NULL)) {
	    cfcs |= PCI_COMMAND_IO_ENABLE;
	}
	if (!pci_mapreg_map(pa, PCI_CBMA,
			    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT,
			    0, &memt, &memh, NULL, NULL)) {
	    cfcs |= PCI_COMMAND_MEM_ENABLE;
	}
	if ((cfcs & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE)) == 0) {
	    printf(": unable to map device registers\n");
	    return;
	}
	cfcs |= PCI_COMMAND_MASTER_ENABLE;
	PCI_CONF_WRITE(PCI_CFCS, cfcs);
#if defined(PCI_PREFER_IOSPACE)
	if (cfcs & PCI_COMMAND_IO_ENABLE) {
	    sc->tulip_bustag = iot, sc->tulip_bushandle = ioh;
	} else {
	    sc->tulip_bustag = memt, sc->tulip_bushandle = memh;
	}
#else
	if (cfcs & PCI_COMMAND_MEM_ENABLE) {
	    sc->tulip_bustag = memt, sc->tulip_bushandle = memh;
	} else {
	    sc->tulip_bustag = iot, sc->tulip_bushandle = ioh;
	}
#endif /* PCI_PREFER_IOSPACE */
    }
#endif
#endif /* __NetBSD__ */

    tulip_initcsrs(sc, csr_base + csroffset, csrsize);

    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);	/* hrh */
    DELAY(100);							/* hrh */

    lcl_dma_restart();
    tulip_initring(sc, &sc->tulip_rxinfo, sc->tulip_rxdescs, sc->tulip_rx_kva,
	TULIP_RXDESCS);
    tulip_initring(sc, &sc->tulip_txinfo, sc->tulip_txdescs, sc->tulip_tx_kva,
	TULIP_TXDESCS);

    /*
     * Make sure there won't be any interrupts or such...
     */
    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(100);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
		   33MHz that comes to two microseconds but wait a
		   bit longer anyways) */

    if ((retval = tulip_read_macaddr(sc)) < 0) {
#if defined(__FreeBSD__)
	printf(TULIP_PRINTF_FMT, TULIP_PRINTF_ARGS);
#endif
	printf(": can't read ENET ROM (why=%d) (", retval);
	for (idx = 0; idx < 32; idx++)
	    printf("%02x", sc->tulip_rombuf[idx]);
	printf("\n");
	printf(TULIP_PRINTF_FMT ": %s%s pass %d.%d\n",
	       TULIP_PRINTF_ARGS,
	       sc->tulip_boardid, tulip_chipdescs[sc->tulip_chipid],
	       (sc->tulip_revinfo & 0xF0) >> 4, sc->tulip_revinfo & 0x0F);
	printf(TULIP_PRINTF_FMT ": address unknown\n", TULIP_PRINTF_ARGS);
    } else {
	tulip_spl_t s;
	tulip_intrfunc_t (*intr_rtn)(void *) = tulip_intr_normal;

	if (sc->tulip_flags & TULIP_SHAREDINTR)
	    intr_rtn = tulip_intr_shared;

#if defined(__NetBSD__)
	if ((sc->tulip_flags & TULIP_SLAVEDINTR) == 0) {
	    pci_intr_handle_t intrhandle;
	    const char *intrstr;

	    if (pci_intr_map(pa, &intrhandle)) {
		printf(": couldn't map interrupt\n");
		return;
	    }
	    intrstr = pci_intr_string(pa->pa_pc, intrhandle);
	    sc->tulip_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_NET,
					      intr_rtn, sc);
	    if (sc->tulip_ih == NULL)
		printf(": couldn't establish interrupt");
	    if (intrstr != NULL)
		printf(" at %s", intrstr);
	    printf("\n");
	    if (sc->tulip_ih == NULL)
		return;
	}
	sc->tulip_ats = shutdownhook_establish(tulip_shutdown, sc);
	if (sc->tulip_ats == NULL)
	    printf("\n%s: warning: couldn't establish shutdown hook\n",
		   sc->tulip_xname);
#endif
#if defined(__FreeBSD__)
	if ((sc->tulip_flags & TULIP_SLAVEDINTR) == 0) {
	    if (!pci_map_int (config_id, intr_rtn, (void*) sc, &net_imask)) {
		printf(TULIP_PRINTF_FMT ": couldn't map interrupt\n",
		       TULIP_PRINTF_ARGS);
		return;
	    }
	}
#if !defined(TULIP_DEVCONF)
	at_shutdown(tulip_shutdown, sc, SHUTDOWN_POST_SYNC);
#endif
#endif
#if defined(__bsdi__)
	if ((sc->tulip_flags & TULIP_SLAVEDINTR) == 0) {
	    isa_establish(&sc->tulip_id, &sc->tulip_dev);

	    sc->tulip_ih.ih_fun = intr_rtn;
	    sc->tulip_ih.ih_arg = (void *) sc;
	    intr_establish(ia->ia_irq, &sc->tulip_ih, DV_NET);
	}

	sc->tulip_ats.func = tulip_shutdown;
	sc->tulip_ats.arg = (void *) sc;
	atshutdown(&sc->tulip_ats, ATSH_ADD);
#endif
#if defined(TULIP_USE_SOFTINTR)
	if (sc->tulip_unit > tulip_softintr_max_unit)
	    tulip_softintr_max_unit = sc->tulip_unit;
#endif

	s = TULIP_RAISESPL();
	tulip_reset(sc);
	tulip_attach(sc);
	TULIP_RESTORESPL(s);
    }
}

static void
a12_m_copydata(m, off, len, cp)
	register struct mbuf *m;
	register int off;
	register int len;
	void *cp;
{
static	long packet[200];	/* may seem unwise, but typ a12 ram is 512 MB */

	if(len > sizeof(packet))
		panic("a12_m_copydata");
	m_copydata(m, off, len, (void *)packet);
	a12r2pb(packet,cp,len);
}
