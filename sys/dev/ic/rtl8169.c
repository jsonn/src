/*	$NetBSD: rtl8169.c,v 1.6.4.3 2005/02/15 21:33:12 skrll Exp $	*/

/*
 * Copyright (c) 1997, 1998-2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/re/if_re.c,v 1.20 2004/04/11 20:34:08 ru Exp $ */

/*
 * RealTek 8139C+/8169/8169S/8110S PCI NIC driver
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Networking Software Engineer
 * Wind River Systems
 */

/*
 * This driver is designed to support RealTek's next generation of
 * 10/100 and 10/100/1000 PCI ethernet controllers. There are currently
 * four devices in this family: the RTL8139C+, the RTL8169, the RTL8169S
 * and the RTL8110S.
 *
 * The 8139C+ is a 10/100 ethernet chip. It is backwards compatible
 * with the older 8139 family, however it also supports a special
 * C+ mode of operation that provides several new performance enhancing
 * features. These include:
 *
 *	o Descriptor based DMA mechanism. Each descriptor represents
 *	  a single packet fragment. Data buffers may be aligned on
 *	  any byte boundary.
 *
 *	o 64-bit DMA
 *
 *	o TCP/IP checksum offload for both RX and TX
 *
 *	o High and normal priority transmit DMA rings
 *
 *	o VLAN tag insertion and extraction
 *
 *	o TCP large send (segmentation offload)
 *
 * Like the 8139, the 8139C+ also has a built-in 10/100 PHY. The C+
 * programming API is fairly straightforward. The RX filtering, EEPROM
 * access and PHY access is the same as it is on the older 8139 series
 * chips.
 *
 * The 8169 is a 64-bit 10/100/1000 gigabit ethernet MAC. It has almost the
 * same programming API and feature set as the 8139C+ with the following
 * differences and additions:
 *
 *	o 1000Mbps mode
 *
 *	o Jumbo frames
 *
 * 	o GMII and TBI ports/registers for interfacing with copper
 *	  or fiber PHYs
 *
 *      o RX and TX DMA rings can have up to 1024 descriptors
 *        (the 8139C+ allows a maximum of 64)
 *
 *	o Slight differences in register layout from the 8139C+
 *
 * The TX start and timer interrupt registers are at different locations
 * on the 8169 than they are on the 8139C+. Also, the status word in the
 * RX descriptor has a slightly different bit layout. The 8169 does not
 * have a built-in PHY. Most reference boards use a Marvell 88E1000 'Alaska'
 * copper gigE PHY.
 *
 * The 8169S/8110S 10/100/1000 devices have built-in copper gigE PHYs
 * (the 'S' stands for 'single-chip'). These devices have the same
 * programming API as the older 8169, but also have some vendor-specific
 * registers for the on-board PHY. The 8110S is a LAN-on-motherboard
 * part designed to be pin-compatible with the RealTek 8100 10/100 chip.
 * 
 * This driver takes advantage of the RX and TX checksum offload and
 * VLAN tag insertion/extraction features. It also implements TX
 * interrupt moderation using the timer interrupt registers, which
 * significantly reduces TX interrupt load. There is also support
 * for jumbo frames, however the 8169/8169S/8110S can not transmit
 * jumbo frames larger than 7.5K, so the max MTU possible with this
 * driver is 7500 bytes.
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_vlanvar.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/*
 * Default to using PIO access for this driver.
 */
#define RE_USEIOSPACE

#include <dev/ic/rtl81x9reg.h>
#include <dev/ic/rtl81x9var.h>

#include <dev/ic/rtl8169var.h>


static int re_encap(struct rtk_softc *, struct mbuf *, int *);

static int re_newbuf(struct rtk_softc *, int, struct mbuf *);
static int re_rx_list_init(struct rtk_softc *);
static int re_tx_list_init(struct rtk_softc *);
static void re_rxeof(struct rtk_softc *);
static void re_txeof(struct rtk_softc *);
static void re_tick(void *);
static void re_start(struct ifnet *);
static int re_ioctl(struct ifnet *, u_long, caddr_t);
static int re_init(struct ifnet *);
static void re_stop(struct ifnet *, int);
static void re_watchdog(struct ifnet *);

static void re_shutdown(void *);
static int re_enable(struct rtk_softc *);
static void re_disable(struct rtk_softc *);
static void re_power(int, void *);

static int re_ifmedia_upd(struct ifnet *);
static void re_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static int re_gmii_readreg(struct device *, int, int);
static void re_gmii_writereg(struct device *, int, int, int);

static int re_miibus_readreg(struct device *, int, int);
static void re_miibus_writereg(struct device *, int, int, int);
static void re_miibus_statchg(struct device *);

static void re_reset(struct rtk_softc *);


#ifdef RE_USEIOSPACE
#define RTK_RES			SYS_RES_IOPORT
#define RTK_RID			RTK_PCI_LOIO
#else
#define RTK_RES			SYS_RES_MEMORY
#define RTK_RID			RTK_PCI_LOMEM
#endif

#define EE_SET(x)					\
	CSR_WRITE_1(sc, RTK_EECMD,			\
		CSR_READ_1(sc, RTK_EECMD) | x)

#define EE_CLR(x)					\
	CSR_WRITE_1(sc, RTK_EECMD,			\
		CSR_READ_1(sc, RTK_EECMD) & ~x)

static int
re_gmii_readreg(struct device *self, int phy, int reg)
{
	struct rtk_softc	*sc = (void *)self;
	u_int32_t		rval;
	int			i;

	if (phy != 7)
		return 0;

	/* Let the rgephy driver read the GMEDIASTAT register */

	if (reg == RTK_GMEDIASTAT) {
		rval = CSR_READ_1(sc, RTK_GMEDIASTAT);
		return rval;
	}

	CSR_WRITE_4(sc, RTK_PHYAR, reg << 16);
	DELAY(1000);

	for (i = 0; i < RTK_TIMEOUT; i++) {
		rval = CSR_READ_4(sc, RTK_PHYAR);
		if (rval & RTK_PHYAR_BUSY)
			break;
		DELAY(100);
	}

	if (i == RTK_TIMEOUT) {
		aprint_error("%s: PHY read failed\n", sc->sc_dev.dv_xname);
		return 0;
	}

	return rval & RTK_PHYAR_PHYDATA;
}

static void
re_gmii_writereg(struct device *dev, int phy, int reg, int data)
{
	struct rtk_softc	*sc = (void *)dev;
	u_int32_t		rval;
	int			i;

	CSR_WRITE_4(sc, RTK_PHYAR, (reg << 16) |
	    (data & RTK_PHYAR_PHYDATA) | RTK_PHYAR_BUSY);
	DELAY(1000);

	for (i = 0; i < RTK_TIMEOUT; i++) {
		rval = CSR_READ_4(sc, RTK_PHYAR);
		if (!(rval & RTK_PHYAR_BUSY))
			break;
		DELAY(100);
	}

	if (i == RTK_TIMEOUT) {
		aprint_error("%s: PHY write reg %x <- %x failed\n",
		    sc->sc_dev.dv_xname, reg, data);
		return;
	}

	return;
}

static int
re_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct rtk_softc	*sc = (void *)dev;
	u_int16_t		rval = 0;
	u_int16_t		re8139_reg = 0;
	int			s;

	s = splnet();

	if (sc->rtk_type == RTK_8169) {
		rval = re_gmii_readreg(dev, phy, reg);
		splx(s);
		return rval;
	}

	/* Pretend the internal PHY is only at address 0 */
	if (phy) {
		splx(s);
		return 0;
	}
	switch (reg) {
	case MII_BMCR:
		re8139_reg = RTK_BMCR;
		break;
	case MII_BMSR:
		re8139_reg = RTK_BMSR;
		break;
	case MII_ANAR:
		re8139_reg = RTK_ANAR;
		break;
	case MII_ANER:
		re8139_reg = RTK_ANER;
		break;
	case MII_ANLPAR:
		re8139_reg = RTK_LPAR;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		splx(s);
		return 0;
	/*
	 * Allow the rlphy driver to read the media status
	 * register. If we have a link partner which does not
	 * support NWAY, this is the register which will tell
	 * us the results of parallel detection.
	 */
	case RTK_MEDIASTAT:
		rval = CSR_READ_1(sc, RTK_MEDIASTAT);
		splx(s);
		return rval;
	default:
		aprint_error("%s: bad phy register\n", sc->sc_dev.dv_xname);
		splx(s);
		return 0;
	}
	rval = CSR_READ_2(sc, re8139_reg);
	splx(s);
	return rval;
}

static void
re_miibus_writereg(struct device *dev, int phy, int reg, int data)
{
	struct rtk_softc	*sc = (void *)dev;
	u_int16_t		re8139_reg = 0;
	int			s;

	s = splnet();

	if (sc->rtk_type == RTK_8169) {
		re_gmii_writereg(dev, phy, reg, data);
		splx(s);
		return;
	}

	/* Pretend the internal PHY is only at address 0 */
	if (phy) {
		splx(s);
		return;
	}
	switch (reg) {
	case MII_BMCR:
		re8139_reg = RTK_BMCR;
		break;
	case MII_BMSR:
		re8139_reg = RTK_BMSR;
		break;
	case MII_ANAR:
		re8139_reg = RTK_ANAR;
		break;
	case MII_ANER:
		re8139_reg = RTK_ANER;
		break;
	case MII_ANLPAR:
		re8139_reg = RTK_LPAR;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		splx(s);
		return;
		break;
	default:
		aprint_error("%s: bad phy register\n", sc->sc_dev.dv_xname);
		splx(s);
		return;
	}
	CSR_WRITE_2(sc, re8139_reg, data);
	splx(s);
	return;
}

static void
re_miibus_statchg(struct device *dev)
{

	return;
}

static void
re_reset(struct rtk_softc *sc)
{
	register int		i;

	CSR_WRITE_1(sc, RTK_COMMAND, RTK_CMD_RESET);

	for (i = 0; i < RTK_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_1(sc, RTK_COMMAND) & RTK_CMD_RESET))
			break;
	}
	if (i == RTK_TIMEOUT)
		aprint_error("%s: reset never completed!\n",
		    sc->sc_dev.dv_xname);

	/*
	 * NB: Realtek-supplied Linux driver does this only for
	 * MCFG_METHOD_2, which corresponds to sc->sc_rev == 2.
	 */
	if (1) /* XXX check softc flag for 8169s version */
		CSR_WRITE_1(sc, 0x82, 1);

	return;
}

/*
 * The following routine is designed to test for a defect on some
 * 32-bit 8169 cards. Some of these NICs have the REQ64# and ACK64#
 * lines connected to the bus, however for a 32-bit only card, they
 * should be pulled high. The result of this defect is that the
 * NIC will not work right if you plug it into a 64-bit slot: DMA
 * operations will be done with 64-bit transfers, which will fail
 * because the 64-bit data lines aren't connected.
 *
 * There's no way to work around this (short of talking a soldering
 * iron to the board), however we can detect it. The method we use
 * here is to put the NIC into digital loopback mode, set the receiver
 * to promiscuous mode, and then try to send a frame. We then compare
 * the frame data we sent to what was received. If the data matches,
 * then the NIC is working correctly, otherwise we know the user has
 * a defective NIC which has been mistakenly plugged into a 64-bit PCI
 * slot. In the latter case, there's no way the NIC can work correctly,
 * so we print out a message on the console and abort the device attach.
 */

int
re_diag(struct rtk_softc *sc)
{
	struct ifnet		*ifp = &sc->ethercom.ec_if;
	struct mbuf		*m0;
	struct ether_header	*eh;
	struct rtk_desc		*cur_rx;
	bus_dmamap_t		dmamap;
	u_int16_t		status;
	u_int32_t		rxstat;
	int			total_len, i, s, error = 0;
	u_int8_t		dst[] = { 0x00, 'h', 'e', 'l', 'l', 'o' };
	u_int8_t		src[] = { 0x00, 'w', 'o', 'r', 'l', 'd' };

	/* Allocate a single mbuf */

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL)
		return ENOBUFS;

	/*
	 * Initialize the NIC in test mode. This sets the chip up
	 * so that it can send and receive frames, but performs the
	 * following special functions:
	 * - Puts receiver in promiscuous mode
	 * - Enables digital loopback mode
	 * - Leaves interrupts turned off
	 */

	ifp->if_flags |= IFF_PROMISC;
	sc->rtk_testmode = 1;
	re_init(ifp);
	re_stop(ifp, 0);
	DELAY(100000);
	re_init(ifp);

	/* Put some data in the mbuf */

	eh = mtod(m0, struct ether_header *);
	bcopy((char *)&dst, eh->ether_dhost, ETHER_ADDR_LEN);
	bcopy((char *)&src, eh->ether_shost, ETHER_ADDR_LEN);
	eh->ether_type = htons(ETHERTYPE_IP);
	m0->m_pkthdr.len = m0->m_len = ETHER_MIN_LEN - ETHER_CRC_LEN;

	/*
	 * Queue the packet, start transmission.
	 */

	CSR_WRITE_2(sc, RTK_ISR, 0xFFFF);
	s = splnet();
	IF_ENQUEUE(&ifp->if_snd, m0);
	re_start(ifp);
	splx(s);
	m0 = NULL;

	/* Wait for it to propagate through the chip */

	DELAY(100000);
	for (i = 0; i < RTK_TIMEOUT; i++) {
		status = CSR_READ_2(sc, RTK_ISR);
		if ((status & (RTK_ISR_TIMEOUT_EXPIRED | RTK_ISR_RX_OK)) ==
		    (RTK_ISR_TIMEOUT_EXPIRED | RTK_ISR_RX_OK))
			break;
		DELAY(10);
	}
	if (i == RTK_TIMEOUT) {
		aprint_error("%s: diagnostic failed, failed to receive packet "
		    "in loopback mode\n", sc->sc_dev.dv_xname);
		error = EIO;
		goto done;
	}

	/*
	 * The packet should have been dumped into the first
	 * entry in the RX DMA ring. Grab it from there.
	 */

	dmamap = sc->rtk_ldata.rtk_rx_list_map;
	bus_dmamap_sync(sc->sc_dmat,
	    dmamap, 0, dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
	dmamap = sc->rtk_ldata.rtk_rx_dmamap[0];
	bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat,
	    sc->rtk_ldata.rtk_rx_dmamap[0]);

	m0 = sc->rtk_ldata.rtk_rx_mbuf[0];
	sc->rtk_ldata.rtk_rx_mbuf[0] = NULL;
	eh = mtod(m0, struct ether_header *);

	cur_rx = &sc->rtk_ldata.rtk_rx_list[0];
	total_len = RTK_RXBYTES(cur_rx);
	rxstat = le32toh(cur_rx->rtk_cmdstat);

	if (total_len != ETHER_MIN_LEN) {
		aprint_error("%s: diagnostic failed, received short packet\n",
		    sc->sc_dev.dv_xname);
		error = EIO;
		goto done;
	}

	/* Test that the received packet data matches what we sent. */

	if (bcmp((char *)&eh->ether_dhost, (char *)&dst, ETHER_ADDR_LEN) ||
	    bcmp((char *)&eh->ether_shost, (char *)&src, ETHER_ADDR_LEN) ||
	    ntohs(eh->ether_type) != ETHERTYPE_IP) {
		aprint_error("%s: WARNING, DMA FAILURE!\n",
		    sc->sc_dev.dv_xname);
		aprint_error("%s: expected TX data: %s",
		    sc->sc_dev.dv_xname, ether_sprintf(dst));
		aprint_error("/%s/0x%x\n", ether_sprintf(src), ETHERTYPE_IP);
		aprint_error("%s: received RX data: %s",
		    sc->sc_dev.dv_xname,
		    ether_sprintf(eh->ether_dhost));
		aprint_error("/%s/0x%x\n", ether_sprintf(eh->ether_shost),
		    ntohs(eh->ether_type));
		aprint_error("%s: You may have a defective 32-bit NIC plugged "
		    "into a 64-bit PCI slot.\n", sc->sc_dev.dv_xname);
		aprint_error("%s: Please re-install the NIC in a 32-bit slot "
		    "for proper operation.\n", sc->sc_dev.dv_xname);
		aprint_error("%s: Read the re(4) man page for more details.\n",
		    sc->sc_dev.dv_xname);
		error = EIO;
	}

done:
	/* Turn interface off, release resources */

	sc->rtk_testmode = 0;
	ifp->if_flags &= ~IFF_PROMISC;
	re_stop(ifp, 0);
	if (m0 != NULL)
		m_freem(m0);

	return error;
}


/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
re_attach(struct rtk_softc *sc)
{
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int16_t		val;
	struct ifnet		*ifp;
	int			error = 0, i, addr_len;


	/* XXX JRS: bus-attach-independent code begins approximately here */

	/* Reset the adapter. */
	re_reset(sc);

	if (sc->rtk_type == RTK_8169) {
		uint32_t hwrev;

		/* Revision of 8169/8169S/8110s in bits 30..26, 23 */
		hwrev = CSR_READ_4(sc, RTK_TXCFG) & 0x7c800000;
		if (hwrev == (0x1 << 28)) {
			sc->sc_rev = 4;
		} else if (hwrev == (0x1 << 26)) {
			sc->sc_rev = 3;
		} else if (hwrev == (0x1 << 23)) {
			sc->sc_rev = 2;
		} else
			sc->sc_rev = 1;

		/* Set RX length mask */

		sc->rtk_rxlenmask = RTK_RDESC_STAT_GFRAGLEN;

		/* Force station address autoload from the EEPROM */

		CSR_WRITE_1(sc, RTK_EECMD, RTK_EEMODE_AUTOLOAD);
		for (i = 0; i < RTK_TIMEOUT; i++) {
			if (!(CSR_READ_1(sc, RTK_EECMD) & RTK_EEMODE_AUTOLOAD))
				break;
			DELAY(100);
		}
		if (i == RTK_TIMEOUT)
			aprint_error("%s: eeprom autoload timed out\n",
			    sc->sc_dev.dv_xname);

		for (i = 0; i < ETHER_ADDR_LEN; i++)
			eaddr[i] = CSR_READ_1(sc, RTK_IDR0 + i);
	} else {

		/* Set RX length mask */

		sc->rtk_rxlenmask = RTK_RDESC_STAT_FRAGLEN;

		if (rtk_read_eeprom(sc, RTK_EE_ID, RTK_EEADDR_LEN1) == 0x8129)
			addr_len = RTK_EEADDR_LEN1;
		else
			addr_len = RTK_EEADDR_LEN0;

		/*
		 * Get station address from the EEPROM.
		 */
		for (i = 0; i < 3; i++) {
			val = rtk_read_eeprom(sc, RTK_EE_EADDR0 + i, addr_len);
			eaddr[(i * 2) + 0] = val & 0xff;
			eaddr[(i * 2) + 1] = val >> 8;
		}
	}

	aprint_normal("%s: Ethernet address %s\n",
	    sc->sc_dev.dv_xname, ether_sprintf(eaddr));


	/* Allocate DMA'able memory for the TX ring */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, RTK_TX_LIST_SZ, 
		    RTK_ETHER_ALIGN, 0, &sc->rtk_ldata.rtk_tx_listseg, 
		    1, &sc->rtk_ldata.rtk_tx_listnseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error("%s: can't allocate tx listseg, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_0;
	}

	/* Load the map for the TX ring. */
	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->rtk_ldata.rtk_tx_listseg,
		    sc->rtk_ldata.rtk_tx_listnseg, RTK_TX_LIST_SZ, 
		    (caddr_t *)&sc->rtk_ldata.rtk_tx_list,
		    BUS_DMA_NOWAIT)) != 0) {
		aprint_error("%s: can't map tx list, error = %d\n",
		    sc->sc_dev.dv_xname, error);
	  	goto fail_1;
	}
	memset(sc->rtk_ldata.rtk_tx_list, 0, RTK_TX_LIST_SZ);

	if ((error = bus_dmamap_create(sc->sc_dmat, RTK_TX_LIST_SZ, 1, 
		    RTK_TX_LIST_SZ, 0, BUS_DMA_ALLOCNOW, 
		    &sc->rtk_ldata.rtk_tx_list_map)) != 0) {
		aprint_error("%s: can't create tx list map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_2;
	}


	if ((error = bus_dmamap_load(sc->sc_dmat, 
		    sc->rtk_ldata.rtk_tx_list_map, sc->rtk_ldata.rtk_tx_list, 
		    RTK_TX_LIST_SZ, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error("%s: can't load tx list, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_3;
	}

	/* Create DMA maps for TX buffers */
	for (i = 0; i < RTK_TX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES * RTK_NTXSEGS,
		    RTK_NTXSEGS, MCLBYTES, 0, BUS_DMA_ALLOCNOW,
		    &sc->rtk_ldata.rtk_tx_dmamap[i]);
		if (error) {
			aprint_error("%s: can't create DMA map for TX\n",
			    sc->sc_dev.dv_xname);
			goto fail_4;
		}
	}

	/* Allocate DMA'able memory for the RX ring */
        if ((error = bus_dmamem_alloc(sc->sc_dmat, RTK_RX_LIST_SZ,
		    RTK_RING_ALIGN, 0, &sc->rtk_ldata.rtk_rx_listseg, 1,
		    &sc->rtk_ldata.rtk_rx_listnseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error("%s: can't allocate rx listseg, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_4;
	}

	/* Load the map for the RX ring. */
	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->rtk_ldata.rtk_rx_listseg,
		    sc->rtk_ldata.rtk_rx_listnseg, RTK_RX_LIST_SZ,
		    (caddr_t *)&sc->rtk_ldata.rtk_rx_list,
		    BUS_DMA_NOWAIT)) != 0) {
		aprint_error("%s: can't map rx list, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_5;
	}
	memset(sc->rtk_ldata.rtk_rx_list, 0, RTK_TX_LIST_SZ);

	if ((error = bus_dmamap_create(sc->sc_dmat, RTK_RX_LIST_SZ, 1,
		    RTK_RX_LIST_SZ, 0, BUS_DMA_ALLOCNOW,
		    &sc->rtk_ldata.rtk_rx_list_map)) != 0) {
		aprint_error("%s: can't create rx list map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_6;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat,
		    sc->rtk_ldata.rtk_rx_list_map, sc->rtk_ldata.rtk_rx_list,
		    RTK_RX_LIST_SZ, NULL, BUS_DMA_NOWAIT)) != 0) {
		aprint_error("%s: can't load rx list, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_7;
	}

	/* Create DMA maps for RX buffers */
	for (i = 0; i < RTK_RX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_ALLOCNOW, &sc->rtk_ldata.rtk_rx_dmamap[i]);
		if (error) {
			aprint_error("%s: can't create DMA map for RX\n",
			    sc->sc_dev.dv_xname);
			goto fail_8;
		}
	}

	/*
	 * Record interface as attached. From here, we should not fail.
	 */
	sc->sc_flags |= RTK_ATTACHED;

	ifp = &sc->ethercom.ec_if;
	ifp->if_softc = sc;
	strcpy(ifp->if_xname, sc->sc_dev.dv_xname);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = re_ioctl;
	sc->ethercom.ec_capabilities |=
	    ETHERCAP_VLAN_MTU | ETHERCAP_VLAN_HWTAGGING;
	ifp->if_start = re_start;
	ifp->if_stop = re_stop;
	ifp->if_capabilities |=
	    IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;
	ifp->if_watchdog = re_watchdog;
	ifp->if_init = re_init;
	if (sc->rtk_type == RTK_8169)
		ifp->if_baudrate = 1000000000;
	else
		ifp->if_baudrate = 100000000;
	ifp->if_snd.ifq_maxlen = RTK_IFQ_MAXLEN;
	ifp->if_capenable = ifp->if_capabilities;
	IFQ_SET_READY(&ifp->if_snd);

	callout_init(&sc->rtk_tick_ch);

	/* Do MII setup */
	sc->mii.mii_ifp = ifp;
	sc->mii.mii_readreg = re_miibus_readreg;
	sc->mii.mii_writereg = re_miibus_writereg;
	sc->mii.mii_statchg = re_miibus_statchg;
	ifmedia_init(&sc->mii.mii_media, IFM_IMASK, re_ifmedia_upd,
	    re_ifmedia_sts);
	mii_attach(&sc->sc_dev, &sc->mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	ifmedia_set(&sc->mii.mii_media, IFM_ETHER | IFM_AUTO);

	/*
	 * Call MI attach routine.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, eaddr);


	/*
	 * Make sure the interface is shutdown during reboot.
	 */
	sc->sc_sdhook = shutdownhook_establish(re_shutdown, sc);
	if (sc->sc_sdhook == NULL)
		aprint_error("%s: WARNING: unable to establish shutdown hook\n",
		    sc->sc_dev.dv_xname);
	/*
	 * Add a suspend hook to make sure we come back up after a
	 * resume.
	 */
	sc->sc_powerhook = powerhook_establish(re_power, sc);
	if (sc->sc_powerhook == NULL)
		aprint_error("%s: WARNING: unable to establish power hook\n",
		    sc->sc_dev.dv_xname);


	return;

fail_8:
	/* Destroy DMA maps for RX buffers. */
	for (i = 0; i < RTK_RX_DESC_CNT; i++)
		if (sc->rtk_ldata.rtk_rx_dmamap[i] != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->rtk_ldata.rtk_rx_dmamap[i]);

	/* Free DMA'able memory for the RX ring. */
	bus_dmamap_unload(sc->sc_dmat, sc->rtk_ldata.rtk_rx_list_map);
fail_7:
	bus_dmamap_destroy(sc->sc_dmat, sc->rtk_ldata.rtk_rx_list_map);
fail_6:
	bus_dmamem_unmap(sc->sc_dmat,
	    (caddr_t)sc->rtk_ldata.rtk_rx_list, RTK_RX_LIST_SZ);
fail_5:
	bus_dmamem_free(sc->sc_dmat,
	    &sc->rtk_ldata.rtk_rx_listseg, sc->rtk_ldata.rtk_rx_listnseg);

fail_4:
	/* Destroy DMA maps for TX buffers. */
	for (i = 0; i < RTK_TX_DESC_CNT; i++)
		if (sc->rtk_ldata.rtk_tx_dmamap[i] != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->rtk_ldata.rtk_tx_dmamap[i]);

	/* Free DMA'able memory for the TX ring. */
	bus_dmamap_unload(sc->sc_dmat, sc->rtk_ldata.rtk_tx_list_map);
fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->rtk_ldata.rtk_tx_list_map);
fail_2:
	bus_dmamem_unmap(sc->sc_dmat,
	    (caddr_t)sc->rtk_ldata.rtk_tx_list, RTK_TX_LIST_SZ);
fail_1:
	bus_dmamem_free(sc->sc_dmat,
	    &sc->rtk_ldata.rtk_tx_listseg, sc->rtk_ldata.rtk_tx_listnseg);
fail_0:
	return;
}


/*
 * re_activate:
 *     Handle device activation/deactivation requests.
 */
int
re_activate(struct device *self, enum devact act)
{
	struct rtk_softc *sc = (void *) self;
	int s, error = 0;

	s = splnet();
	switch (act) {
	case DVACT_ACTIVATE:
		error = EOPNOTSUPP;
		break;
	case DVACT_DEACTIVATE:
		mii_activate(&sc->mii, act, MII_PHY_ANY, MII_OFFSET_ANY);
		if_deactivate(&sc->ethercom.ec_if);
		break;
	}
	splx(s);

	return error;
}

/*
 * re_detach:
 *     Detach a rtk interface.
 */
int
re_detach(struct rtk_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;
	int i;

	/*
	 * Succeed now if there isn't any work to do.
	 */
	if ((sc->sc_flags & RTK_ATTACHED) == 0)
		return 0;

	/* Unhook our tick handler. */
	callout_stop(&sc->rtk_tick_ch);

	/* Detach all PHYs. */
	mii_detach(&sc->mii, MII_PHY_ANY, MII_OFFSET_ANY);

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->mii.mii_media, IFM_INST_ANY);

	ether_ifdetach(ifp);
	if_detach(ifp);

	/* XXX undo re_allocmem() */

	/* Destroy DMA maps for RX buffers. */
	for (i = 0; i < RTK_RX_DESC_CNT; i++)
		if (sc->rtk_ldata.rtk_rx_dmamap[i] != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->rtk_ldata.rtk_rx_dmamap[i]);

	/* Free DMA'able memory for the RX ring. */
	bus_dmamap_unload(sc->sc_dmat, sc->rtk_ldata.rtk_rx_list_map);
	bus_dmamap_destroy(sc->sc_dmat, sc->rtk_ldata.rtk_rx_list_map);
	bus_dmamem_unmap(sc->sc_dmat,
	    (caddr_t)sc->rtk_ldata.rtk_rx_list, RTK_RX_LIST_SZ);
	bus_dmamem_free(sc->sc_dmat,
	    &sc->rtk_ldata.rtk_rx_listseg, sc->rtk_ldata.rtk_rx_listnseg);

	/* Destroy DMA maps for TX buffers. */
	for (i = 0; i < RTK_TX_DESC_CNT; i++)
		if (sc->rtk_ldata.rtk_tx_dmamap[i] != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->rtk_ldata.rtk_tx_dmamap[i]);

	/* Free DMA'able memory for the TX ring. */
	bus_dmamap_unload(sc->sc_dmat, sc->rtk_ldata.rtk_tx_list_map);
	bus_dmamap_destroy(sc->sc_dmat, sc->rtk_ldata.rtk_tx_list_map);
	bus_dmamem_unmap(sc->sc_dmat,
	    (caddr_t)sc->rtk_ldata.rtk_tx_list, RTK_TX_LIST_SZ);
	bus_dmamem_free(sc->sc_dmat,
	    &sc->rtk_ldata.rtk_tx_listseg, sc->rtk_ldata.rtk_tx_listnseg);

	
	shutdownhook_disestablish(sc->sc_sdhook);
	powerhook_disestablish(sc->sc_powerhook);

	return 0;
}

/*
 * re_enable:
 *     Enable the RTL81X9 chip.
 */
static int 
re_enable(struct rtk_softc *sc)
{
	if (RTK_IS_ENABLED(sc) == 0 && sc->sc_enable != NULL) {
		if ((*sc->sc_enable)(sc) != 0) {
			aprint_error("%s: device enable failed\n",
			    sc->sc_dev.dv_xname);
			return EIO;
		}
		sc->sc_flags |= RTK_ENABLED;
	}
	return 0;
}

/*
 * re_disable:
 *     Disable the RTL81X9 chip.
 */
static void 
re_disable(struct rtk_softc *sc)
{

	if (RTK_IS_ENABLED(sc) && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_flags &= ~RTK_ENABLED;
	}
}

/*
 * re_power:
 *     Power management (suspend/resume) hook.
 */
void 
re_power(int why, void *arg)
{
	struct rtk_softc *sc = (void *) arg;
	struct ifnet *ifp = &sc->ethercom.ec_if;
	int s;

	s = splnet();
	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		re_stop(ifp, 0);
		if (sc->sc_power != NULL)
			(*sc->sc_power)(sc, why);
		break;
	case PWR_RESUME:
		if (ifp->if_flags & IFF_UP) {
			if (sc->sc_power != NULL)
				(*sc->sc_power)(sc, why);
			re_init(ifp);
		}
		break;
	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
	}
	splx(s);
}


static int
re_newbuf(struct rtk_softc *sc, int idx, struct mbuf *m)
{
	struct mbuf		*n = NULL;
	bus_dmamap_t		map;
	struct rtk_desc		*d;
	u_int32_t		cmdstat;
	int			error;

	if (m == NULL) {
		MGETHDR(n, M_DONTWAIT, MT_DATA);
		if (n == NULL)
			return ENOBUFS;
		m = n;

		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_freem(m);
			return ENOBUFS;
		}
	} else
		m->m_data = m->m_ext.ext_buf;

	/*
	 * Initialize mbuf length fields and fixup
	 * alignment so that the frame payload is
	 * longword aligned.
	 */
	m->m_len = m->m_pkthdr.len = MCLBYTES;
	m_adj(m, RTK_ETHER_ALIGN);

	map = sc->rtk_ldata.rtk_rx_dmamap[idx];
	error = bus_dmamap_load_mbuf(sc->sc_dmat, map, m, BUS_DMA_NOWAIT);

	if (error)
		goto out;

	d = &sc->rtk_ldata.rtk_rx_list[idx];
	if (le32toh(d->rtk_cmdstat) & RTK_RDESC_STAT_OWN)
		goto out;

	cmdstat = map->dm_segs[0].ds_len;
	d->rtk_bufaddr_lo = htole32(RTK_ADDR_LO(map->dm_segs[0].ds_addr));
	d->rtk_bufaddr_hi = htole32(RTK_ADDR_HI(map->dm_segs[0].ds_addr));
	cmdstat |= RTK_TDESC_CMD_SOF;
	if (idx == (RTK_RX_DESC_CNT - 1))
		cmdstat |= RTK_TDESC_CMD_EOR;
	d->rtk_cmdstat = htole32(cmdstat);

	d->rtk_cmdstat |= htole32(RTK_TDESC_CMD_EOF);


	sc->rtk_ldata.rtk_rx_list[idx].rtk_cmdstat |=
	    htole32(RTK_RDESC_CMD_OWN);
	sc->rtk_ldata.rtk_rx_mbuf[idx] = m;

	bus_dmamap_sync(sc->sc_dmat, sc->rtk_ldata.rtk_rx_dmamap[idx], 0,
	    sc->rtk_ldata.rtk_rx_dmamap[idx]->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	return 0;
out:
	if (n != NULL)
		m_freem(n);
	return ENOMEM;
}

static int
re_tx_list_init(struct rtk_softc *sc)
{
	memset((char *)sc->rtk_ldata.rtk_tx_list, 0, RTK_TX_LIST_SZ);
	memset((char *)&sc->rtk_ldata.rtk_tx_mbuf, 0,
	    (RTK_TX_DESC_CNT * sizeof(struct mbuf *)));

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rtk_ldata.rtk_tx_list_map, 0,
	    sc->rtk_ldata.rtk_tx_list_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	sc->rtk_ldata.rtk_tx_prodidx = 0;
	sc->rtk_ldata.rtk_tx_considx = 0;
	sc->rtk_ldata.rtk_tx_free = RTK_TX_DESC_CNT;

	return 0;
}

static int
re_rx_list_init(struct rtk_softc *sc)
{
	int			i;

	memset((char *)sc->rtk_ldata.rtk_rx_list, 0, RTK_RX_LIST_SZ);
	memset((char *)&sc->rtk_ldata.rtk_rx_mbuf, 0,
	    (RTK_RX_DESC_CNT * sizeof(struct mbuf *)));

	for (i = 0; i < RTK_RX_DESC_CNT; i++) {
		if (re_newbuf(sc, i, NULL) == ENOBUFS)
			return ENOBUFS;
	}

	/* Flush the RX descriptors */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rtk_ldata.rtk_rx_list_map,
	    0, sc->rtk_ldata.rtk_rx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	sc->rtk_ldata.rtk_rx_prodidx = 0;
	sc->rtk_head = sc->rtk_tail = NULL;

	return 0;
}

/*
 * RX handler for C+ and 8169. For the gigE chips, we support
 * the reception of jumbo frames that have been fragmented
 * across multiple 2K mbuf cluster buffers.
 */
static void
re_rxeof(struct rtk_softc *sc)
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	int			i, total_len;
	struct rtk_desc		*cur_rx;
	struct m_tag		*mtag;
	u_int32_t		rxstat, rxvlan;

	ifp = &sc->ethercom.ec_if;
	i = sc->rtk_ldata.rtk_rx_prodidx;

	/* Invalidate the descriptor memory */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rtk_ldata.rtk_rx_list_map,
	    0, sc->rtk_ldata.rtk_rx_list_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);

	while (!RTK_OWN(&sc->rtk_ldata.rtk_rx_list[i])) {

		cur_rx = &sc->rtk_ldata.rtk_rx_list[i];
		m = sc->rtk_ldata.rtk_rx_mbuf[i];
		total_len = RTK_RXBYTES(cur_rx);
		rxstat = le32toh(cur_rx->rtk_cmdstat);
		rxvlan = le32toh(cur_rx->rtk_vlanctl);

		/* Invalidate the RX mbuf and unload its map */

		bus_dmamap_sync(sc->sc_dmat,
		    sc->rtk_ldata.rtk_rx_dmamap[i],
		    0, sc->rtk_ldata.rtk_rx_dmamap[i]->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat,
		    sc->rtk_ldata.rtk_rx_dmamap[i]);

		if (!(rxstat & RTK_RDESC_STAT_EOF)) {
			m->m_len = MCLBYTES - RTK_ETHER_ALIGN;
			if (sc->rtk_head == NULL)
				sc->rtk_head = sc->rtk_tail = m;
			else {
				m->m_flags &= ~M_PKTHDR;
				sc->rtk_tail->m_next = m;
				sc->rtk_tail = m;
			}
			re_newbuf(sc, i, NULL);
			RTK_DESC_INC(i);
			continue;
		}

		/*
		 * NOTE: for the 8139C+, the frame length field
		 * is always 12 bits in size, but for the gigE chips,
		 * it is 13 bits (since the max RX frame length is 16K).
		 * Unfortunately, all 32 bits in the status word
		 * were already used, so to make room for the extra
		 * length bit, RealTek took out the 'frame alignment
		 * error' bit and shifted the other status bits
		 * over one slot. The OWN, EOR, FS and LS bits are
		 * still in the same places. We have already extracted
		 * the frame length and checked the OWN bit, so rather
		 * than using an alternate bit mapping, we shift the
		 * status bits one space to the right so we can evaluate
		 * them using the 8169 status as though it was in the
		 * same format as that of the 8139C+.
		 */
		if (sc->rtk_type == RTK_8169)
			rxstat >>= 1;

		if (rxstat & RTK_RDESC_STAT_RXERRSUM) {
			ifp->if_ierrors++;
			/*
			 * If this is part of a multi-fragment packet,
			 * discard all the pieces.
			 */
			if (sc->rtk_head != NULL) {
				m_freem(sc->rtk_head);
				sc->rtk_head = sc->rtk_tail = NULL;
			}
			re_newbuf(sc, i, m);
			RTK_DESC_INC(i);
			continue;
		}

		/*
		 * If allocating a replacement mbuf fails,
		 * reload the current one.
		 */

		if (re_newbuf(sc, i, NULL)) {
			ifp->if_ierrors++;
			if (sc->rtk_head != NULL) {
				m_freem(sc->rtk_head);
				sc->rtk_head = sc->rtk_tail = NULL;
			}
			re_newbuf(sc, i, m);
			RTK_DESC_INC(i);
			continue;
		}

		RTK_DESC_INC(i);

		if (sc->rtk_head != NULL) {
			m->m_len = total_len % (MCLBYTES - RTK_ETHER_ALIGN);
			/* 
			 * Special case: if there's 4 bytes or less
			 * in this buffer, the mbuf can be discarded:
			 * the last 4 bytes is the CRC, which we don't
			 * care about anyway.
			 */
			if (m->m_len <= ETHER_CRC_LEN) {
				sc->rtk_tail->m_len -=
				    (ETHER_CRC_LEN - m->m_len);
				m_freem(m);
			} else {
				m->m_len -= ETHER_CRC_LEN;
				m->m_flags &= ~M_PKTHDR;
				sc->rtk_tail->m_next = m;
			}
			m = sc->rtk_head;
			sc->rtk_head = sc->rtk_tail = NULL;
			m->m_pkthdr.len = total_len - ETHER_CRC_LEN;
		} else
			m->m_pkthdr.len = m->m_len =
			    (total_len - ETHER_CRC_LEN);

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

		/* Do RX checksumming if enabled */

		if (ifp->if_capenable & IFCAP_CSUM_IPv4) {

			/* Check IP header checksum */
			if (rxstat & RTK_RDESC_STAT_PROTOID)
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4;;
			if (rxstat & RTK_RDESC_STAT_IPSUMBAD)
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4_BAD;
		}

		/* Check TCP/UDP checksum */
		if (RTK_TCPPKT(rxstat) &&
		    (ifp->if_capenable & IFCAP_CSUM_TCPv4)) {
			m->m_pkthdr.csum_flags |= M_CSUM_TCPv4;
			if (rxstat & RTK_RDESC_STAT_TCPSUMBAD)
				m->m_pkthdr.csum_flags |= M_CSUM_TCP_UDP_BAD;
		}
		if (RTK_UDPPKT(rxstat) &&
		    (ifp->if_capenable & IFCAP_CSUM_UDPv4)) {
			m->m_pkthdr.csum_flags |= M_CSUM_UDPv4;
			if (rxstat & RTK_RDESC_STAT_UDPSUMBAD)
				m->m_pkthdr.csum_flags |= M_CSUM_TCP_UDP_BAD;
		}

		if (rxvlan & RTK_RDESC_VLANCTL_TAG) {
			mtag = m_tag_get(PACKET_TAG_VLAN, sizeof(u_int),
			    M_NOWAIT);
			if (mtag == NULL) {
				ifp->if_ierrors++;
				m_freem(m);
				continue;
			}
			*(u_int *)(mtag + 1) = 
			    be16toh(rxvlan & RTK_RDESC_VLANCTL_DATA);
			m_tag_prepend(m, mtag);
		}
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif
		(*ifp->if_input)(ifp, m);
	}

	/* Flush the RX DMA ring */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rtk_ldata.rtk_rx_list_map,
	    0, sc->rtk_ldata.rtk_rx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	sc->rtk_ldata.rtk_rx_prodidx = i;

	return;
}

static void
re_txeof(struct rtk_softc *sc)
{
	struct ifnet		*ifp;
	u_int32_t		txstat;
	int			idx;

	ifp = &sc->ethercom.ec_if;
	idx = sc->rtk_ldata.rtk_tx_considx;

	/* Invalidate the TX descriptor list */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rtk_ldata.rtk_tx_list_map,
	    0, sc->rtk_ldata.rtk_tx_list_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);

	while (idx != sc->rtk_ldata.rtk_tx_prodidx) {

		txstat = le32toh(sc->rtk_ldata.rtk_tx_list[idx].rtk_cmdstat);
		if (txstat & RTK_TDESC_CMD_OWN)
			break;

		/*
		 * We only stash mbufs in the last descriptor
		 * in a fragment chain, which also happens to
		 * be the only place where the TX status bits
		 * are valid.
		 */

		if (txstat & RTK_TDESC_CMD_EOF) {
			m_freem(sc->rtk_ldata.rtk_tx_mbuf[idx]);
			sc->rtk_ldata.rtk_tx_mbuf[idx] = NULL;
			bus_dmamap_unload(sc->sc_dmat,
			    sc->rtk_ldata.rtk_tx_dmamap[idx]);
			if (txstat & (RTK_TDESC_STAT_EXCESSCOL |
			    RTK_TDESC_STAT_COLCNT))
				ifp->if_collisions++;
			if (txstat & RTK_TDESC_STAT_TXERRSUM)
				ifp->if_oerrors++;
			else
				ifp->if_opackets++;
		}
		sc->rtk_ldata.rtk_tx_free++;
		RTK_DESC_INC(idx);
	}

	/* No changes made to the TX ring, so no flush needed */

	if (idx != sc->rtk_ldata.rtk_tx_considx) {
		sc->rtk_ldata.rtk_tx_considx = idx;
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;
	}

	/*
	 * If not all descriptors have been released reaped yet,
	 * reload the timer so that we will eventually get another
	 * interrupt that will cause us to re-enter this routine.
	 * This is done in case the transmitter has gone idle.
	 */
	if (sc->rtk_ldata.rtk_tx_free != RTK_TX_DESC_CNT)
		CSR_WRITE_4(sc, RTK_TIMERCNT, 1);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
re_shutdown(void *vsc)

{
	struct rtk_softc	*sc = (struct rtk_softc *)vsc;

	re_stop(&sc->ethercom.ec_if, 0);
}


static void
re_tick(void *xsc)
{
	struct rtk_softc	*sc = xsc;
	int s;

	/*XXX: just return for 8169S/8110S with rev 2 or newer phy */
	s = splnet();

	mii_tick(&sc->mii);
	splx(s);

	callout_reset(&sc->rtk_tick_ch, hz, re_tick, sc);
}

#ifdef DEVICE_POLLING
static void
re_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct rtk_softc *sc = ifp->if_softc;

	RTK_LOCK(sc);
	if (!(ifp->if_capenable & IFCAP_POLLING)) {
		ether_poll_deregister(ifp);
		cmd = POLL_DEREGISTER;
	}
	if (cmd == POLL_DEREGISTER) { /* final call, enable interrupts */
		CSR_WRITE_2(sc, RTK_IMR, RTK_INTRS_CPLUS);
		goto done;
	}

	sc->rxcycles = count;
	re_rxeof(sc);
	re_txeof(sc);

	if (ifp->if_snd.ifq_head != NULL)
		(*ifp->if_start)(ifp);

	if (cmd == POLL_AND_CHECK_STATUS) { /* also check status register */
		u_int16_t       status;

		status = CSR_READ_2(sc, RTK_ISR);
		if (status == 0xffff)
			goto done;
		if (status)
			CSR_WRITE_2(sc, RTK_ISR, status);

		/*
		 * XXX check behaviour on receiver stalls.
		 */

		if (status & RTK_ISR_SYSTEM_ERR) {
			re_reset(sc);
			re_init(sc);
		}
	}
done:
	RTK_UNLOCK(sc);
}
#endif /* DEVICE_POLLING */

int
re_intr(void *arg)
{
	struct rtk_softc	*sc = arg;
	struct ifnet		*ifp;
	u_int16_t		status;
	int			handled = 0;

	ifp = &sc->ethercom.ec_if;

	if (!(ifp->if_flags & IFF_UP))
		return 0;

#ifdef DEVICE_POLLING
	if (ifp->if_flags & IFF_POLLING)
		goto done;
	if ((ifp->if_capenable & IFCAP_POLLING) &&
	    ether_poll_register(re_poll, ifp)) { /* ok, disable interrupts */
		CSR_WRITE_2(sc, RTK_IMR, 0x0000);
		re_poll(ifp, 0, 1);
		goto done;
	}
#endif /* DEVICE_POLLING */

	for (;;) {

		status = CSR_READ_2(sc, RTK_ISR);
		/* If the card has gone away the read returns 0xffff. */
		if (status == 0xffff)
			break;
		if (status) {
			handled = 1;
			CSR_WRITE_2(sc, RTK_ISR, status);
		}

		if ((status & RTK_INTRS_CPLUS) == 0)
			break;

		if ((status & RTK_ISR_RX_OK) ||
		    (status & RTK_ISR_RX_ERR))
			re_rxeof(sc);

		if ((status & RTK_ISR_TIMEOUT_EXPIRED) ||
		    (status & RTK_ISR_TX_ERR) ||
		    (status & RTK_ISR_TX_DESC_UNAVAIL))
			re_txeof(sc);

		if (status & RTK_ISR_SYSTEM_ERR) {
			re_reset(sc);
			re_init(ifp);
		}

		if (status & RTK_ISR_LINKCHG) {
			callout_stop(&sc->rtk_tick_ch);
			re_tick(sc);
		}
	}

	if (ifp->if_flags & IFF_UP) /* kludge for interrupt during re_init() */
		if (ifp->if_snd.ifq_head != NULL)
			(*ifp->if_start)(ifp);

#ifdef DEVICE_POLLING
done:
#endif

	return handled;
}

static int
re_encap(struct rtk_softc *sc, struct mbuf *m_head, int *idx)
{
	bus_dmamap_t		map;
	int			error, i, curidx;
	struct m_tag		*mtag;
	struct rtk_desc		*d;
	u_int32_t		cmdstat, rtk_flags;

	if (sc->rtk_ldata.rtk_tx_free <= 4)
		return EFBIG;

	/*
	 * Set up checksum offload. Note: checksum offload bits must
	 * appear in all descriptors of a multi-descriptor transmit
	 * attempt. (This is according to testing done with an 8169
	 * chip. I'm not sure if this is a requirement or a bug.)
	 */

	rtk_flags = 0;

	if (m_head->m_pkthdr.csum_flags & M_CSUM_IPv4)
		rtk_flags |= RTK_TDESC_CMD_IPCSUM;
	if (m_head->m_pkthdr.csum_flags & M_CSUM_TCPv4)
		rtk_flags |= RTK_TDESC_CMD_TCPCSUM;
	if (m_head->m_pkthdr.csum_flags & M_CSUM_UDPv4)
		rtk_flags |= RTK_TDESC_CMD_UDPCSUM;

	map = sc->rtk_ldata.rtk_tx_dmamap[*idx];
	error = bus_dmamap_load_mbuf(sc->sc_dmat, map,
	    m_head, BUS_DMA_NOWAIT);

	if (error) {
		aprint_error("%s: can't map mbuf (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		return ENOBUFS;
	}

	if (map->dm_nsegs > sc->rtk_ldata.rtk_tx_free - 4)
		return ENOBUFS;
	/*
	 * Map the segment array into descriptors. Note that we set the
	 * start-of-frame and end-of-frame markers for either TX or RX, but
	 * they really only have meaning in the TX case. (In the RX case,
	 * it's the chip that tells us where packets begin and end.)
	 * We also keep track of the end of the ring and set the
	 * end-of-ring bits as needed, and we set the ownership bits
	 * in all except the very first descriptor. (The caller will
	 * set this descriptor later when it start transmission or
	 * reception.)
	 */
	i = 0;
	curidx = *idx;
	while (1) {
		d = &sc->rtk_ldata.rtk_tx_list[curidx];
		if (le32toh(d->rtk_cmdstat) & RTK_RDESC_STAT_OWN)
			return ENOBUFS;

		cmdstat = map->dm_segs[i].ds_len;
		d->rtk_bufaddr_lo =
		    htole32(RTK_ADDR_LO(map->dm_segs[i].ds_addr));
		d->rtk_bufaddr_hi =
		    htole32(RTK_ADDR_HI(map->dm_segs[i].ds_addr));
		if (i == 0)
			cmdstat |= RTK_TDESC_CMD_SOF;
		else
			cmdstat |= RTK_TDESC_CMD_OWN;
		if (curidx == (RTK_RX_DESC_CNT - 1))
			cmdstat |= RTK_TDESC_CMD_EOR;
		d->rtk_cmdstat = htole32(cmdstat | rtk_flags);
		i++;
		if (i == map->dm_nsegs)
			break;
		RTK_DESC_INC(curidx);
	}

	d->rtk_cmdstat |= htole32(RTK_TDESC_CMD_EOF);

	/*
	 * Insure that the map for this transmission
	 * is placed at the array index of the last descriptor
	 * in this chain.
	 */
	sc->rtk_ldata.rtk_tx_dmamap[*idx] =
	    sc->rtk_ldata.rtk_tx_dmamap[curidx];
	sc->rtk_ldata.rtk_tx_dmamap[curidx] = map;
	sc->rtk_ldata.rtk_tx_mbuf[curidx] = m_head;
	sc->rtk_ldata.rtk_tx_free -= map->dm_nsegs;

	/*
	 * Set up hardware VLAN tagging. Note: vlan tag info must
	 * appear in the first descriptor of a multi-descriptor
	 * transmission attempt.
	 */

	if (sc->ethercom.ec_nvlans &&
	    (mtag = m_tag_find(m_head, PACKET_TAG_VLAN, NULL)) != NULL)
		sc->rtk_ldata.rtk_tx_list[*idx].rtk_vlanctl =
		    htole32(htons(*(u_int *)(mtag + 1)) |
		    RTK_TDESC_VLANCTL_TAG);

	/* Transfer ownership of packet to the chip. */

	sc->rtk_ldata.rtk_tx_list[curidx].rtk_cmdstat |=
	    htole32(RTK_TDESC_CMD_OWN);
	if (*idx != curidx)
		sc->rtk_ldata.rtk_tx_list[*idx].rtk_cmdstat |=
		    htole32(RTK_TDESC_CMD_OWN);

	RTK_DESC_INC(curidx);
	*idx = curidx;

	return 0;
}

/*
 * Main transmit routine for C+ and gigE NICs.
 */

static void
re_start(struct ifnet *ifp)
{
	struct rtk_softc	*sc;
	struct mbuf		*m_head = NULL;
	int			idx;

	sc = ifp->if_softc;

	idx = sc->rtk_ldata.rtk_tx_prodidx;
	while (sc->rtk_ldata.rtk_tx_mbuf[idx] == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (re_encap(sc, m_head, &idx)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head);
#endif
	}

	/* Flush the TX descriptors */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rtk_ldata.rtk_tx_list_map,
	    0, sc->rtk_ldata.rtk_tx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	sc->rtk_ldata.rtk_tx_prodidx = idx;

	/*
	 * RealTek put the TX poll request register in a different
	 * location on the 8169 gigE chip. I don't know why.
	 */

	if (sc->rtk_type == RTK_8169)
		CSR_WRITE_2(sc, RTK_GTXSTART, RTK_TXSTART_START);
	else
		CSR_WRITE_2(sc, RTK_TXSTART, RTK_TXSTART_START);

	/*
	 * Use the countdown timer for interrupt moderation.
	 * 'TX done' interrupts are disabled. Instead, we reset the
	 * countdown timer, which will begin counting until it hits
	 * the value in the TIMERINT register, and then trigger an
	 * interrupt. Each time we write to the TIMERCNT register,
	 * the timer count is reset to 0.
	 */
	CSR_WRITE_4(sc, RTK_TIMERCNT, 1);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static int
re_init(struct ifnet *ifp)
{
	struct rtk_softc	*sc = ifp->if_softc;
	u_int32_t		rxcfg = 0;
	u_int32_t		reg;
	int error;
	
	if ((error = re_enable(sc)) != 0)
		goto out;

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	re_stop(ifp, 0);

	/*
	 * Enable C+ RX and TX mode, as well as VLAN stripping and
	 * RX checksum offload. We must configure the C+ register
	 * before all others.
	 */
	reg = 0;

	/*
	 * XXX: Realtek docs say bits 0 and 1 are reserved, for 8169S/8110S.
	 * FreeBSD  drivers set these bits anyway (for 8139C+?).
	 * So far, it works.
	 */

	/*
	 * XXX: For 8169 and 8196S revs below 2, set bit 14.
	 * For 8169S/8110S rev 2 and above, do not set bit 14.
	 */
	if (sc->rtk_type == RTK_8169 && sc->sc_rev == 1)
		reg |= (0x1 << 14) | RTK_CPLUSCMD_PCI_MRW;;

	if (1)  {/* not for 8169S ? */
		reg |= RTK_CPLUSCMD_VLANSTRIP |
		    (ifp->if_capenable &
		    (IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4) ?
		    RTK_CPLUSCMD_RXCSUM_ENB : 0);
	}
 
	CSR_WRITE_2(sc, RTK_CPLUS_CMD,
	    reg | RTK_CPLUSCMD_RXENB | RTK_CPLUSCMD_TXENB);

	/* XXX: from Realtek-supplied Linux driver. Wholly undocumented. */
	if (sc->rtk_type == RTK_8169)
		CSR_WRITE_2(sc, RTK_CPLUS_CMD+0x2, 0x0000);

	DELAY(10000);

	/*
	 * Init our MAC address.  Even though the chipset
	 * documentation doesn't mention it, we need to enter "Config
	 * register write enable" mode to modify the ID registers.
	 */
	CSR_WRITE_1(sc, RTK_EECMD, RTK_EEMODE_WRITECFG);
	memcpy(&reg, LLADDR(ifp->if_sadl), 4);
	CSR_WRITE_STREAM_4(sc, RTK_IDR0, reg);
	reg = 0;
	memcpy(&reg, LLADDR(ifp->if_sadl) + 4, 4);
	CSR_WRITE_STREAM_4(sc, RTK_IDR4, reg);
	CSR_WRITE_1(sc, RTK_EECMD, RTK_EEMODE_OFF);

	/*
	 * For C+ mode, initialize the RX descriptors and mbufs.
	 */
	re_rx_list_init(sc);
	re_tx_list_init(sc);

	/*
	 * Enable transmit and receive.
	 */
	CSR_WRITE_1(sc, RTK_COMMAND, RTK_CMD_TX_ENB | RTK_CMD_RX_ENB);

	/*
	 * Set the initial TX and RX configuration.
	 */
	if (sc->rtk_testmode) {
		if (sc->rtk_type == RTK_8169)
			CSR_WRITE_4(sc, RTK_TXCFG,
			    RTK_TXCFG_CONFIG | RTK_LOOPTEST_ON);
		else
			CSR_WRITE_4(sc, RTK_TXCFG,
			    RTK_TXCFG_CONFIG | RTK_LOOPTEST_ON_CPLUS);
	} else
		CSR_WRITE_4(sc, RTK_TXCFG, RTK_TXCFG_CONFIG);
	CSR_WRITE_4(sc, RTK_RXCFG, RTK_RXCFG_CONFIG);

	/* Set the individual bit to receive frames for this host only. */
	rxcfg = CSR_READ_4(sc, RTK_RXCFG);
	rxcfg |= RTK_RXCFG_RX_INDIV;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		rxcfg |= RTK_RXCFG_RX_ALLPHYS;
	else
		rxcfg &= ~RTK_RXCFG_RX_ALLPHYS;
	CSR_WRITE_4(sc, RTK_RXCFG, rxcfg);

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST)
		rxcfg |= RTK_RXCFG_RX_BROAD;
	else
		rxcfg &= ~RTK_RXCFG_RX_BROAD;
	CSR_WRITE_4(sc, RTK_RXCFG, rxcfg);

	/*
	 * Program the multicast filter, if necessary.
	 */
	rtk_setmulti(sc);

#ifdef DEVICE_POLLING
	/*
	 * Disable interrupts if we are polling.
	 */
	if (ifp->if_flags & IFF_POLLING)
		CSR_WRITE_2(sc, RTK_IMR, 0);
	else	/* otherwise ... */
#endif /* DEVICE_POLLING */
	/*
	 * Enable interrupts.
	 */
	if (sc->rtk_testmode)
		CSR_WRITE_2(sc, RTK_IMR, 0);
	else
		CSR_WRITE_2(sc, RTK_IMR, RTK_INTRS_CPLUS);

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RTK_MISSEDPKT, 0);
#ifdef notdef
	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RTK_COMMAND, RTK_CMD_TX_ENB | RTK_CMD_RX_ENB);
#endif
	/*
	 * Load the addresses of the RX and TX lists into the chip.
	 */

	CSR_WRITE_4(sc, RTK_RXLIST_ADDR_HI,
	    RTK_ADDR_HI(sc->rtk_ldata.rtk_rx_listseg.ds_addr));
	CSR_WRITE_4(sc, RTK_RXLIST_ADDR_LO,
	    RTK_ADDR_LO(sc->rtk_ldata.rtk_rx_listseg.ds_addr));

	CSR_WRITE_4(sc, RTK_TXLIST_ADDR_HI,
	    RTK_ADDR_HI(sc->rtk_ldata.rtk_tx_listseg.ds_addr));
	CSR_WRITE_4(sc, RTK_TXLIST_ADDR_LO,
	    RTK_ADDR_LO(sc->rtk_ldata.rtk_tx_listseg.ds_addr));

	CSR_WRITE_1(sc, RTK_EARLY_TX_THRESH, 16);

	/*
	 * Initialize the timer interrupt register so that
	 * a timer interrupt will be generated once the timer
	 * reaches a certain number of ticks. The timer is
	 * reloaded on each transmit. This gives us TX interrupt
	 * moderation, which dramatically improves TX frame rate.
	 */

	if (sc->rtk_type == RTK_8169)
		CSR_WRITE_4(sc, RTK_TIMERINT_8169, 0x800);
	else
		CSR_WRITE_4(sc, RTK_TIMERINT, 0x400);

	/*
	 * For 8169 gigE NICs, set the max allowed RX packet
	 * size so we can receive jumbo frames.
	 */
	if (sc->rtk_type == RTK_8169)
		CSR_WRITE_2(sc, RTK_MAXRXPKTLEN, 16383);

	if (sc->rtk_testmode)
		return 0;

	mii_mediachg(&sc->mii);

	CSR_WRITE_1(sc, RTK_CFG1, RTK_CFG1_DRVLOAD | RTK_CFG1_FULLDUPLEX);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	callout_reset(&sc->rtk_tick_ch, hz, re_tick, sc);

out:
	if (error) {
		ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
		ifp->if_timer = 0;
		aprint_error("%s: interface not running\n",
		    sc->sc_dev.dv_xname);
	}
	  
	return error;

}

/*
 * Set media options.
 */
static int
re_ifmedia_upd(struct ifnet *ifp)
{
	struct rtk_softc	*sc;

	sc = ifp->if_softc;

	return mii_mediachg(&sc->mii);
}

/*
 * Report current media status.
 */
static void
re_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct rtk_softc	*sc;

	sc = ifp->if_softc;

	mii_pollstat(&sc->mii);
	ifmr->ifm_active = sc->mii.mii_media_active;
	ifmr->ifm_status = sc->mii.mii_media_status;

	return;
}

static int
re_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct rtk_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	int			s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > RTK_JUMBO_MTU)
			error = EINVAL;
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->mii.mii_media, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				rtk_setmulti(sc);
			error = 0;
		}
		break;
	}

	splx(s);

	return error;
}

static void
re_watchdog(struct ifnet *ifp)
{
	struct rtk_softc	*sc;
	int			s;

	sc = ifp->if_softc;
	s = splnet();
	aprint_error("%s: watchdog timeout\n", sc->sc_dev.dv_xname);
	ifp->if_oerrors++;

	re_txeof(sc);
	re_rxeof(sc);

	re_init(ifp);

	splx(s);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
re_stop(struct ifnet *ifp, int disable)
{
	register int		i;
	struct rtk_softc *sc = ifp->if_softc;

	callout_stop(&sc->rtk_tick_ch);

#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif /* DEVICE_POLLING */

	mii_down(&sc->mii);

	CSR_WRITE_1(sc, RTK_COMMAND, 0x00);
	CSR_WRITE_2(sc, RTK_IMR, 0x0000);

	if (sc->rtk_head != NULL) {
		m_freem(sc->rtk_head);
		sc->rtk_head = sc->rtk_tail = NULL;
	}

	/* Free the TX list buffers. */
	for (i = 0; i < RTK_TX_DESC_CNT; i++) {
		if (sc->rtk_ldata.rtk_tx_mbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->rtk_ldata.rtk_tx_dmamap[i]);
			m_freem(sc->rtk_ldata.rtk_tx_mbuf[i]);
			sc->rtk_ldata.rtk_tx_mbuf[i] = NULL;
		}
	}

	/* Free the RX list buffers. */
	for (i = 0; i < RTK_RX_DESC_CNT; i++) {
		if (sc->rtk_ldata.rtk_rx_mbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->rtk_ldata.rtk_rx_dmamap[i]);
			m_freem(sc->rtk_ldata.rtk_rx_mbuf[i]);
			sc->rtk_ldata.rtk_rx_mbuf[i] = NULL;
		}
	}

	if (disable)
		re_disable(sc);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	return;
}
