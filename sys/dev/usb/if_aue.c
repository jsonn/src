/*	$NetBSD: if_aue.c,v 1.75.4.2 2002/08/29 05:22:58 gehenna Exp $	*/
/*
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
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
 *
 * $FreeBSD: src/sys/dev/usb/if_aue.c,v 1.11 2000/01/14 01:36:14 wpaul Exp $
 */

/*
 * ADMtek AN986 Pegasus and AN8511 Pegasus II USB to ethernet driver.
 * Datasheet is available from http://www.admtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Pegasus chip uses four USB "endpoints" to provide 10/100 ethernet
 * support: the control endpoint for reading/writing registers, burst
 * read endpoint for packet reception, burst write for packet transmission
 * and one for "interrupts." The chip uses the same RX filter scheme
 * as the other ADMtek ethernet parts: one perfect filter entry for the
 * the station address and a 64-bit multicast hash table. The chip supports
 * both MII and HomePNA attachments.
 *
 * Since the maximum data transfer speed of USB is supposed to be 12Mbps,
 * you're never really going to get 100Mbps speeds from this device. I
 * think the idea is to allow the device to connect to 10 or 100Mbps
 * networks, not necessarily to provide 100Mbps performance. Also, since
 * the controller uses an external PHY chip, it's possible that board
 * designers might simply choose a 10Mbps PHY.
 *
 * Registers are accessed using usbd_do_request(). Packet transfers are
 * done using usbd_transfer() and friends.
 */

/*
 * Ported to NetBSD and somewhat rewritten by Lennart Augustsson.
 */

/*
 * TODO:
 * better error messages from rxstat
 * split out if_auevar.h
 * add thread to avoid register reads from interrupt context
 * more error checks
 * investigate short rx problem
 * proper cleanup on errors
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_aue.c,v 1.75.4.2 2002/08/29 05:22:58 gehenna Exp $");

#if defined(__NetBSD__)
#include "opt_inet.h"
#include "opt_ns.h"
#include "bpfilter.h"
#include "rnd.h"
#elif defined(__OpenBSD__)
#include "bpfilter.h"
#endif /* defined(__OpenBSD__) */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/device.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <net/if.h>
#if defined(__NetBSD__)
#include <net/if_arp.h>
#endif
#include <net/if_dl.h>
#include <net/if_media.h>

#define BPF_MTAP(ifp, m) bpf_mtap((ifp)->if_bpf, (m))

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if defined(__NetBSD__)
#include <net/if_ether.h>
#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif
#endif /* defined(__NetBSD__) */

#if defined(__OpenBSD__)
#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif
#endif /* defined(__OpenBSD__) */

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_auereg.h>

#ifdef AUE_DEBUG
#define DPRINTF(x)	if (auedebug) logprintf x
#define DPRINTFN(n,x)	if (auedebug >= (n)) logprintf x
int	auedebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * Various supported device vendors/products.
 */
struct aue_type {
	struct usb_devno	aue_dev;
	u_int16_t		aue_flags;
#define LSYS	0x0001		/* use Linksys reset */
#define PNA	0x0002		/* has Home PNA */
#define PII	0x0004		/* Pegasus II chip */
};

Static const struct aue_type aue_devs[] = {
 {{ USB_VENDOR_3COM,		USB_PRODUCT_3COM_3C460B},         PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX1},	  PNA|PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX2},	  PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_UFE1000},	  LSYS },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX4},	  PNA },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX5},	  PNA },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX6},	  PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX7},	  PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX8},	  PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX9},	  PNA },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX10},	  0 },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_DSB650TX_PNA}, 0 },
 {{ USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_USB320_EC},	  0 },
 {{ USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_SS1001},	  PII },
 {{ USB_VENDOR_ADMTEK,		USB_PRODUCT_ADMTEK_PEGASUS},	  PNA },
 {{ USB_VENDOR_ADMTEK,		USB_PRODUCT_ADMTEK_PEGASUSII},	  PII },
 {{ USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_USB2LAN},	  PII },
 {{ USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USB100},	  0 },
 {{ USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USBLP100}, PNA },
 {{ USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USBEL100}, 0 },
 {{ USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USBE100},  PII },
 {{ USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_FETHER_USB_TX}, 0 },
 {{ USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_FETHER_USB_TXS},PII },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX4},	  LSYS|PII },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX1},	  LSYS },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX},	  LSYS },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX_PNA},  PNA },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX3},	  LSYS|PII },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX2},	  LSYS|PII },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650},	  0 },
 {{ USB_VENDOR_ELECOM,		USB_PRODUCT_ELECOM_LDUSBTX0},	  0 },
 {{ USB_VENDOR_ELECOM,		USB_PRODUCT_ELECOM_LDUSBTX1},	  0 },
 {{ USB_VENDOR_ELECOM,		USB_PRODUCT_ELECOM_LDUSBTX2},	  0 },
 {{ USB_VENDOR_ELECOM,		USB_PRODUCT_ELECOM_LDUSBTX3},	  PII },
 {{ USB_VENDOR_ELECOM,		USB_PRODUCT_ELECOM_LDUSBLTX},	  PII },
 {{ USB_VENDOR_ELSA,		USB_PRODUCT_ELSA_USB2ETHERNET},	  0 },
 {{ USB_VENDOR_IODATA,		USB_PRODUCT_IODATA_USBETTX},	  0 },
 {{ USB_VENDOR_IODATA,		USB_PRODUCT_IODATA_USBETTXS},	  PII },
 {{ USB_VENDOR_KINGSTON,	USB_PRODUCT_KINGSTON_KNU101TX},   0 },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB10TX1},	  LSYS|PII },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB10T},	  LSYS },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB100TX},	  LSYS },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB100H1},	  LSYS|PNA },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB10TA},	  LSYS },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB10TX2},	  LSYS|PII },
 {{ USB_VENDOR_MELCO, 		USB_PRODUCT_MELCO_LUATX1}, 	  0 },
 {{ USB_VENDOR_MELCO, 		USB_PRODUCT_MELCO_LUATX5}, 	  0 },
 {{ USB_VENDOR_MELCO, 		USB_PRODUCT_MELCO_LUA2TX5}, 	  PII },
 {{ USB_VENDOR_SIEMENS,		USB_PRODUCT_SIEMENS_SPEEDSTREAM}, PII },
 {{ USB_VENDOR_SMARTBRIDGES,	USB_PRODUCT_SMARTBRIDGES_SMARTNIC},PII },
 {{ USB_VENDOR_SMC,		USB_PRODUCT_SMC_2202USB},	  0 },
 {{ USB_VENDOR_SMC,		USB_PRODUCT_SMC_2206USB},	  PII },
 {{ USB_VENDOR_SOHOWARE,	USB_PRODUCT_SOHOWARE_NUB100},	  0 },
};
#define aue_lookup(v, p) ((struct aue_type *)usb_lookup(aue_devs, v, p))

USB_DECLARE_DRIVER(aue);

Static void aue_reset_pegasus_II(struct aue_softc *sc);
Static int aue_tx_list_init(struct aue_softc *);
Static int aue_rx_list_init(struct aue_softc *);
Static int aue_newbuf(struct aue_softc *, struct aue_chain *, struct mbuf *);
Static int aue_send(struct aue_softc *, struct mbuf *, int);
Static void aue_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void aue_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void aue_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void aue_tick(void *);
Static void aue_tick_task(void *);
Static void aue_start(struct ifnet *);
Static int aue_ioctl(struct ifnet *, u_long, caddr_t);
Static void aue_init(void *);
Static void aue_stop(struct aue_softc *);
Static void aue_watchdog(struct ifnet *);
Static int aue_openpipes(struct aue_softc *);
Static int aue_ifmedia_upd(struct ifnet *);
Static void aue_ifmedia_sts(struct ifnet *, struct ifmediareq *);

Static int aue_eeprom_getword(struct aue_softc *, int);
Static void aue_read_mac(struct aue_softc *, u_char *);
Static int aue_miibus_readreg(device_ptr_t, int, int);
Static void aue_miibus_writereg(device_ptr_t, int, int, int);
Static void aue_miibus_statchg(device_ptr_t);

Static void aue_lock_mii(struct aue_softc *);
Static void aue_unlock_mii(struct aue_softc *);

Static void aue_setmulti(struct aue_softc *);
Static u_int32_t aue_crc(caddr_t);
Static void aue_reset(struct aue_softc *);

Static int aue_csr_read_1(struct aue_softc *, int);
Static int aue_csr_write_1(struct aue_softc *, int, int);
Static int aue_csr_read_2(struct aue_softc *, int);
Static int aue_csr_write_2(struct aue_softc *, int, int);

#define AUE_SETBIT(sc, reg, x)				\
	aue_csr_write_1(sc, reg, aue_csr_read_1(sc, reg) | (x))

#define AUE_CLRBIT(sc, reg, x)				\
	aue_csr_write_1(sc, reg, aue_csr_read_1(sc, reg) & ~(x))

Static int
aue_csr_read_1(struct aue_softc *sc, int reg)
{
	usb_device_request_t	req;
	usbd_status		err;
	uByte			val = 0;

	if (sc->aue_dying)
		return (0);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AUE_UR_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	err = usbd_do_request(sc->aue_udev, &req, &val);

	if (err) {
		DPRINTF(("%s: aue_csr_read_1: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->aue_dev), reg, usbd_errstr(err)));
		return (0);
	}

	return (val);
}

Static int
aue_csr_read_2(struct aue_softc *sc, int reg)
{
	usb_device_request_t	req;
	usbd_status		err;
	uWord			val;

	if (sc->aue_dying)
		return (0);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AUE_UR_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	err = usbd_do_request(sc->aue_udev, &req, &val);

	if (err) {
		DPRINTF(("%s: aue_csr_read_2: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->aue_dev), reg, usbd_errstr(err)));
		return (0);
	}

	return (UGETW(val));
}

Static int
aue_csr_write_1(struct aue_softc *sc, int reg, int aval)
{
	usb_device_request_t	req;
	usbd_status		err;
	uByte			val;

	if (sc->aue_dying)
		return (0);

	val = aval;
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AUE_UR_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	err = usbd_do_request(sc->aue_udev, &req, &val);

	if (err) {
		DPRINTF(("%s: aue_csr_write_1: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->aue_dev), reg, usbd_errstr(err)));
		return (-1);
	}

	return (0);
}

Static int
aue_csr_write_2(struct aue_softc *sc, int reg, int aval)
{
	usb_device_request_t	req;
	usbd_status		err;
	uWord			val;

	if (sc->aue_dying)
		return (0);

	USETW(val, aval);
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AUE_UR_WRITEREG;
	USETW(req.wValue, aval);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	err = usbd_do_request(sc->aue_udev, &req, &val);

	if (err) {
		DPRINTF(("%s: aue_csr_write_2: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->aue_dev), reg, usbd_errstr(err)));
		return (-1);
	}

	return (0);
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
Static int
aue_eeprom_getword(struct aue_softc *sc, int addr)
{
	int		i;

	aue_csr_write_1(sc, AUE_EE_REG, addr);
	aue_csr_write_1(sc, AUE_EE_CTL, AUE_EECTL_READ);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_EE_CTL) & AUE_EECTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("%s: EEPROM read timed out\n",
		    USBDEVNAME(sc->aue_dev));
	}

	return (aue_csr_read_2(sc, AUE_EE_DATA));
}

/*
 * Read the MAC from the EEPROM.  It's at offset 0.
 */
Static void
aue_read_mac(struct aue_softc *sc, u_char *dest)
{
	int			i;
	int			off = 0;
	int			word;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	for (i = 0; i < 3; i++) {
		word = aue_eeprom_getword(sc, off + i);
		dest[2 * i] = (u_char)word;
		dest[2 * i + 1] = (u_char)(word >> 8);
	}
}

/* Get exclusive access to the MII registers */
Static void
aue_lock_mii(struct aue_softc *sc)
{
	sc->aue_refcnt++;
	lockmgr(&sc->aue_mii_lock, LK_EXCLUSIVE, NULL);
}

Static void
aue_unlock_mii(struct aue_softc *sc)
{
	lockmgr(&sc->aue_mii_lock, LK_RELEASE, NULL);
	if (--sc->aue_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->aue_dev));
}

Static int
aue_miibus_readreg(device_ptr_t dev, int phy, int reg)
{
	struct aue_softc	*sc = USBGETSOFTC(dev);
	int			i;
	u_int16_t		val;

	if (sc->aue_dying) {
#ifdef DIAGNOSTIC
		printf("%s: dying\n", USBDEVNAME(sc->aue_dev));
#endif
		return 0;
	}

#if 0
	/*
	 * The Am79C901 HomePNA PHY actually contains
	 * two transceivers: a 1Mbps HomePNA PHY and a
	 * 10Mbps full/half duplex ethernet PHY with
	 * NWAY autoneg. However in the ADMtek adapter,
	 * only the 1Mbps PHY is actually connected to
	 * anything, so we ignore the 10Mbps one. It
	 * happens to be configured for MII address 3,
	 * so we filter that out.
	 */
	if (sc->aue_vendor == USB_VENDOR_ADMTEK &&
	    sc->aue_product == USB_PRODUCT_ADMTEK_PEGASUS) {
		if (phy == 3)
			return (0);
	}
#endif

	aue_lock_mii(sc);
	aue_csr_write_1(sc, AUE_PHY_ADDR, phy);
	aue_csr_write_1(sc, AUE_PHY_CTL, reg | AUE_PHYCTL_READ);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_PHY_CTL) & AUE_PHYCTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("%s: MII read timed out\n", USBDEVNAME(sc->aue_dev));
	}

	val = aue_csr_read_2(sc, AUE_PHY_DATA);

	DPRINTFN(11,("%s: %s: phy=%d reg=%d => 0x%04x\n",
		     USBDEVNAME(sc->aue_dev), __func__, phy, reg, val));

	aue_unlock_mii(sc);
	return (val);
}

Static void
aue_miibus_writereg(device_ptr_t dev, int phy, int reg, int data)
{
	struct aue_softc	*sc = USBGETSOFTC(dev);
	int			i;

#if 0
	if (sc->aue_vendor == USB_VENDOR_ADMTEK &&
	    sc->aue_product == USB_PRODUCT_ADMTEK_PEGASUS) {
		if (phy == 3)
			return;
	}
#endif

	DPRINTFN(11,("%s: %s: phy=%d reg=%d data=0x%04x\n",
		     USBDEVNAME(sc->aue_dev), __func__, phy, reg, data));

	aue_lock_mii(sc);
	aue_csr_write_2(sc, AUE_PHY_DATA, data);
	aue_csr_write_1(sc, AUE_PHY_ADDR, phy);
	aue_csr_write_1(sc, AUE_PHY_CTL, reg | AUE_PHYCTL_WRITE);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_PHY_CTL) & AUE_PHYCTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("%s: MII read timed out\n",
		    USBDEVNAME(sc->aue_dev));
	}
	aue_unlock_mii(sc);
}

Static void
aue_miibus_statchg(device_ptr_t dev)
{
	struct aue_softc	*sc = USBGETSOFTC(dev);
	struct mii_data		*mii = GET_MII(sc);

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	aue_lock_mii(sc);
	AUE_CLRBIT(sc, AUE_CTL0, AUE_CTL0_RX_ENB | AUE_CTL0_TX_ENB);

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX) {
		AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_SPEEDSEL);
	} else {
		AUE_CLRBIT(sc, AUE_CTL1, AUE_CTL1_SPEEDSEL);
	}

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_DUPLEX);
	else
		AUE_CLRBIT(sc, AUE_CTL1, AUE_CTL1_DUPLEX);

	AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_RX_ENB | AUE_CTL0_TX_ENB);
	aue_unlock_mii(sc);

	/*
	 * Set the LED modes on the LinkSys adapter.
	 * This turns on the 'dual link LED' bin in the auxmode
	 * register of the Broadcom PHY.
	 */
	if (!sc->aue_dying && (sc->aue_flags & LSYS)) {
		u_int16_t auxmode;
		auxmode = aue_miibus_readreg(dev, 0, 0x1b);
		aue_miibus_writereg(dev, 0, 0x1b, auxmode | 0x04);
	}
	DPRINTFN(5,("%s: %s: exit\n", USBDEVNAME(sc->aue_dev), __func__));
}

#define AUE_POLY	0xEDB88320
#define AUE_BITS	6

Static u_int32_t
aue_crc(caddr_t addr)
{
	u_int32_t		idx, bit, data, crc;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1)
			crc = (crc >> 1) ^ (((crc ^ data) & 1) ? AUE_POLY : 0);
	}

	return (crc & ((1 << AUE_BITS) - 1));
}

Static void
aue_setmulti(struct aue_softc *sc)
{
	struct ifnet		*ifp;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	u_int32_t		h = 0, i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	ifp = GET_IFP(sc);

	if (ifp->if_flags & IFF_PROMISC) {
allmulti:
		ifp->if_flags |= IFF_ALLMULTI;
		AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_ALLMULTI);
		return;
	}

	AUE_CLRBIT(sc, AUE_CTL0, AUE_CTL0_ALLMULTI);

	/* first, zot all the existing hash bits */
	for (i = 0; i < 8; i++)
		aue_csr_write_1(sc, AUE_MAR0 + i, 0);

	/* now program new ones */
#if defined(__NetBSD__)
	ETHER_FIRST_MULTI(step, &sc->aue_ec, enm);
#else
	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
#endif
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo,
		    enm->enm_addrhi, ETHER_ADDR_LEN) != 0)
			goto allmulti;

		h = aue_crc(enm->enm_addrlo);
		AUE_SETBIT(sc, AUE_MAR + (h >> 3), 1 << (h & 0x7));
		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;
}

Static void
aue_reset_pegasus_II(struct aue_softc *sc)
{
	/* Magic constants taken from Linux driver. */
	aue_csr_write_1(sc, AUE_REG_1D, 0);
	aue_csr_write_1(sc, AUE_REG_7B, 2);
#if 0
	if ((sc->aue_flags & HAS_HOME_PNA) && mii_mode)
		aue_csr_write_1(sc, AUE_REG_81, 6);
	else
#endif
		aue_csr_write_1(sc, AUE_REG_81, 2);
}

Static void
aue_reset(struct aue_softc *sc)
{
	int		i;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_RESETMAC);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (!(aue_csr_read_1(sc, AUE_CTL1) & AUE_CTL1_RESETMAC))
			break;
	}

	if (i == AUE_TIMEOUT)
		printf("%s: reset failed\n", USBDEVNAME(sc->aue_dev));

#if 0
	/* XXX what is mii_mode supposed to be */
	if (sc->aue_mii_mode && (sc->aue_flags & PNA))
		aue_csr_write_1(sc, AUE_GPIO1, 0x34);
	else
		aue_csr_write_1(sc, AUE_GPIO1, 0x26);
#endif

	/*
	 * The PHY(s) attached to the Pegasus chip may be held
	 * in reset until we flip on the GPIO outputs. Make sure
	 * to set the GPIO pins high so that the PHY(s) will
	 * be enabled.
	 *
	 * Note: We force all of the GPIO pins low first, *then*
	 * enable the ones we want.
  	 */
	if (sc->aue_flags & LSYS) {
		/* Grrr. LinkSys has to be different from everyone else. */
		aue_csr_write_1(sc, AUE_GPIO0,
		    AUE_GPIO_SEL0 | AUE_GPIO_SEL1);
	} else {
		aue_csr_write_1(sc, AUE_GPIO0,
		    AUE_GPIO_OUT0 | AUE_GPIO_SEL0);
	}
  	aue_csr_write_1(sc, AUE_GPIO0,
	    AUE_GPIO_OUT0 | AUE_GPIO_SEL0 | AUE_GPIO_SEL1);

	if (sc->aue_flags & PII)
		aue_reset_pegasus_II(sc);

	/* Wait a little while for the chip to get its brains in order. */
	delay(10000);		/* XXX */
}

/*
 * Probe for a Pegasus chip.
 */
USB_MATCH(aue)
{
	USB_MATCH_START(aue, uaa);

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	return (aue_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
USB_ATTACH(aue)
{
	USB_ATTACH_START(aue, sc, uaa);
	char			devinfo[1024];
	int			s;
	u_char			eaddr[ETHER_ADDR_LEN];
	struct ifnet		*ifp;
	struct mii_data		*mii;
	usbd_device_handle	dev = uaa->device;
	usbd_interface_handle	iface;
	usbd_status		err;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int			i;

	DPRINTFN(5,(" : aue_attach: sc=%p", sc));

	usbd_devinfo(dev, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->aue_dev), devinfo);

	err = usbd_set_config_no(dev, AUE_CONFIG_NO, 1);
	if (err) {
		printf("%s: setting config no failed\n",
		    USBDEVNAME(sc->aue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	usb_init_task(&sc->aue_tick_task, aue_tick_task, sc);
	usb_init_task(&sc->aue_stop_task, (void (*)(void *))aue_stop, sc);
	lockinit(&sc->aue_mii_lock, PZERO, "auemii", 0, 0);

	err = usbd_device2interface_handle(dev, AUE_IFACE_IDX, &iface);
	if (err) {
		printf("%s: getting interface handle failed\n",
		    USBDEVNAME(sc->aue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->aue_flags = aue_lookup(uaa->vendor, uaa->product)->aue_flags;

	sc->aue_udev = dev;
	sc->aue_iface = iface;
	sc->aue_product = uaa->product;
	sc->aue_vendor = uaa->vendor;

	id = usbd_get_interface_descriptor(iface);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: couldn't get endpoint descriptor %d\n",
			    USBDEVNAME(sc->aue_dev), i);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->aue_ed[AUE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->aue_ed[AUE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->aue_ed[AUE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	if (sc->aue_ed[AUE_ENDPT_RX] == 0 || sc->aue_ed[AUE_ENDPT_TX] == 0 ||
	    sc->aue_ed[AUE_ENDPT_INTR] == 0) {
		printf("%s: missing endpoint\n", USBDEVNAME(sc->aue_dev));
		USB_ATTACH_ERROR_RETURN;
	}


	s = splnet();

	/* Reset the adapter. */
	aue_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	aue_read_mac(sc, eaddr);

	/*
	 * A Pegasus chip was detected. Inform the world.
	 */
	ifp = GET_IFP(sc);
	printf("%s: Ethernet address %s\n", USBDEVNAME(sc->aue_dev),
	    ether_sprintf(eaddr));

	/* Initialize interface info.*/
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = aue_ioctl;
	ifp->if_start = aue_start;
	ifp->if_watchdog = aue_watchdog;
#if defined(__OpenBSD__)
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
#endif
	strncpy(ifp->if_xname, USBDEVNAME(sc->aue_dev), IFNAMSIZ);

	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize MII/media info. */
	mii = &sc->aue_mii;
	mii->mii_ifp = ifp;
	mii->mii_readreg = aue_miibus_readreg;
	mii->mii_writereg = aue_miibus_writereg;
	mii->mii_statchg = aue_miibus_statchg;
	mii->mii_flags = MIIF_AUTOTSLEEP;
	ifmedia_init(&mii->mii_media, 0, aue_ifmedia_upd, aue_ifmedia_sts);
	mii_attach(self, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		ifmedia_add(&mii->mii_media, IFM_ETHER | IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_NONE);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_AUTO);

	/* Attach the interface. */
	if_attach(ifp);
	Ether_ifattach(ifp, eaddr);
#if NRND > 0
	rnd_attach_source(&sc->rnd_source, USBDEVNAME(sc->aue_dev),
	    RND_TYPE_NET, 0);
#endif

	usb_callout_init(sc->aue_stat_ch);

	sc->aue_attached = 1;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->aue_udev,
			   USBDEV(sc->aue_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(aue)
{
	USB_DETACH_START(aue, sc);
	struct ifnet		*ifp = GET_IFP(sc);
	int			s;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	if (!sc->aue_attached) {
		/* Detached before attached finished, so just bail out. */
		return (0);
	}

	usb_uncallout(sc->aue_stat_ch, aue_tick, sc);
	/*
	 * Remove any pending tasks.  They cannot be executing because they run
	 * in the same thread as detach.
	 */
	usb_rem_task(sc->aue_udev, &sc->aue_tick_task);
	usb_rem_task(sc->aue_udev, &sc->aue_stop_task);

	s = splusb();

	if (ifp->if_flags & IFF_RUNNING)
		aue_stop(sc);

#if defined(__NetBSD__)
#if NRND > 0
	rnd_detach_source(&sc->rnd_source);
#endif
	mii_detach(&sc->aue_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	ifmedia_delete_instance(&sc->aue_mii.mii_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
#endif /* __NetBSD__ */

	if_detach(ifp);

#ifdef DIAGNOSTIC
	if (sc->aue_ep[AUE_ENDPT_TX] != NULL ||
	    sc->aue_ep[AUE_ENDPT_RX] != NULL ||
	    sc->aue_ep[AUE_ENDPT_INTR] != NULL)
		printf("%s: detach has active endpoints\n",
		       USBDEVNAME(sc->aue_dev));
#endif

	sc->aue_attached = 0;

	if (--sc->aue_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->aue_dev));
	}
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->aue_udev,
			   USBDEV(sc->aue_dev));

	return (0);
}

int
aue_activate(device_ptr_t self, enum devact act)
{
	struct aue_softc *sc = (struct aue_softc *)self;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		if_deactivate(&sc->aue_ec.ec_if);
		sc->aue_dying = 1;
		break;
	}
	return (0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
Static int
aue_newbuf(struct aue_softc *sc, struct aue_chain *c, struct mbuf *m)
{
	struct mbuf		*m_new = NULL;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev),__func__));

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", USBDEVNAME(sc->aue_dev));
			return (ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", USBDEVNAME(sc->aue_dev));
			m_freem(m_new);
			return (ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, ETHER_ALIGN);
	c->aue_mbuf = m_new;

	return (0);
}

Static int
aue_rx_list_init(struct aue_softc *sc)
{
	struct aue_cdata	*cd;
	struct aue_chain	*c;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	cd = &sc->aue_cdata;
	for (i = 0; i < AUE_RX_LIST_CNT; i++) {
		c = &cd->aue_rx_chain[i];
		c->aue_sc = sc;
		c->aue_idx = i;
		if (aue_newbuf(sc, c, NULL) == ENOBUFS)
			return (ENOBUFS);
		if (c->aue_xfer == NULL) {
			c->aue_xfer = usbd_alloc_xfer(sc->aue_udev);
			if (c->aue_xfer == NULL)
				return (ENOBUFS);
			c->aue_buf = usbd_alloc_buffer(c->aue_xfer, AUE_BUFSZ);
			if (c->aue_buf == NULL)
				return (ENOBUFS); /* XXX free xfer */
		}
	}

	return (0);
}

Static int
aue_tx_list_init(struct aue_softc *sc)
{
	struct aue_cdata	*cd;
	struct aue_chain	*c;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	cd = &sc->aue_cdata;
	for (i = 0; i < AUE_TX_LIST_CNT; i++) {
		c = &cd->aue_tx_chain[i];
		c->aue_sc = sc;
		c->aue_idx = i;
		c->aue_mbuf = NULL;
		if (c->aue_xfer == NULL) {
			c->aue_xfer = usbd_alloc_xfer(sc->aue_udev);
			if (c->aue_xfer == NULL)
				return (ENOBUFS);
			c->aue_buf = usbd_alloc_buffer(c->aue_xfer, AUE_BUFSZ);
			if (c->aue_buf == NULL)
				return (ENOBUFS);
		}
	}

	return (0);
}

Static void
aue_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct aue_softc	*sc = priv;
	struct ifnet		*ifp = GET_IFP(sc);
	struct aue_intrpkt	*p = &sc->aue_cdata.aue_ibuf;

	DPRINTFN(15,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev),__func__));

	if (sc->aue_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			return;
		}
		sc->aue_intr_errs++;
		if (usbd_ratecheck(&sc->aue_rx_notice)) {
			printf("%s: %u usb errors on intr: %s\n",
			    USBDEVNAME(sc->aue_dev), sc->aue_intr_errs,
			    usbd_errstr(status));
			sc->aue_intr_errs = 0;
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->aue_ep[AUE_ENDPT_RX]);
		return;
	}

	if (p->aue_txstat0)
		ifp->if_oerrors++;

	if (p->aue_txstat0 & (AUE_TXSTAT0_LATECOLL | AUE_TXSTAT0_EXCESSCOLL))
		ifp->if_collisions++;
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
Static void
aue_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct aue_chain	*c = priv;
	struct aue_softc	*sc = c->aue_sc;
	struct ifnet		*ifp = GET_IFP(sc);
	struct mbuf		*m;
	u_int32_t		total_len;
	struct aue_rxpkt	r;
	int			s;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev),__func__));

	if (sc->aue_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		sc->aue_rx_errs++;
		if (usbd_ratecheck(&sc->aue_rx_notice)) {
			printf("%s: %u usb errors on rx: %s\n",
			    USBDEVNAME(sc->aue_dev), sc->aue_rx_errs,
			    usbd_errstr(status));
			sc->aue_rx_errs = 0;
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->aue_ep[AUE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	memcpy(mtod(c->aue_mbuf, char *), c->aue_buf, total_len);

	if (total_len <= 4 + ETHER_CRC_LEN) {
		ifp->if_ierrors++;
		goto done;
	}

	memcpy(&r, c->aue_buf + total_len - 4, sizeof(r));

	/* Turn off all the non-error bits in the rx status word. */
	r.aue_rxstat &= AUE_RXSTAT_MASK;
	if (r.aue_rxstat) {
		ifp->if_ierrors++;
		goto done;
	}

	/* No errors; receive the packet. */
	m = c->aue_mbuf;
	total_len -= ETHER_CRC_LEN + 4;
	m->m_pkthdr.len = m->m_len = total_len;
	ifp->if_ipackets++;

	m->m_pkthdr.rcvif = ifp;

	s = splnet();

	/* XXX ugly */
	if (aue_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		goto done1;
	}

#if NBPFILTER > 0
	/*
	 * Handle BPF listeners. Let the BPF user see the packet, but
	 * don't pass it up to the ether_input() layer unless it's
	 * a broadcast packet, multicast packet, matches our ethernet
	 * address or the interface is in promiscuous mode.
	 */
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m);
#endif

	DPRINTFN(10,("%s: %s: deliver %d\n", USBDEVNAME(sc->aue_dev),
		    __func__, m->m_len));
	IF_INPUT(ifp, m);
 done1:
	splx(s);

 done:

	/* Setup new transfer. */
	usbd_setup_xfer(xfer, sc->aue_ep[AUE_ENDPT_RX],
	    c, c->aue_buf, AUE_BUFSZ,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, aue_rxeof);
	usbd_transfer(xfer);

	DPRINTFN(10,("%s: %s: start rx\n", USBDEVNAME(sc->aue_dev),
		    __func__));
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

Static void
aue_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct aue_chain	*c = priv;
	struct aue_softc	*sc = c->aue_sc;
	struct ifnet		*ifp = GET_IFP(sc);
	int			s;

	if (sc->aue_dying)
		return;

	s = splnet();

	DPRINTFN(10,("%s: %s: enter status=%d\n", USBDEVNAME(sc->aue_dev),
		    __func__, status));

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}
		ifp->if_oerrors++;
		printf("%s: usb error on tx: %s\n", USBDEVNAME(sc->aue_dev),
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->aue_ep[AUE_ENDPT_TX]);
		splx(s);
		return;
	}

	ifp->if_opackets++;

	m_freem(c->aue_mbuf);
	c->aue_mbuf = NULL;

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		aue_start(ifp);

	splx(s);
}

Static void
aue_tick(void *xsc)
{
	struct aue_softc	*sc = xsc;

	DPRINTFN(15,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev),__func__));

	if (sc == NULL)
		return;

	if (sc->aue_dying)
		return;

	/* Perform periodic stuff in process context. */
	usb_add_task(sc->aue_udev, &sc->aue_tick_task);
}

Static void
aue_tick_task(void *xsc)
{
	struct aue_softc	*sc = xsc;
	struct ifnet		*ifp;
	struct mii_data		*mii;
	int			s;

	DPRINTFN(15,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev),__func__));

	if (sc->aue_dying)
		return;

	ifp = GET_IFP(sc);
	mii = GET_MII(sc);
	if (mii == NULL)
		return;

	s = splnet();

	mii_tick(mii);
	if (!sc->aue_link) {
		mii_pollstat(mii); /* XXX FreeBSD has removed this call */
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			DPRINTFN(2,("%s: %s: got link\n",
				    USBDEVNAME(sc->aue_dev),__func__));
			sc->aue_link++;
			if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
				aue_start(ifp);
		}
	}

	usb_callout(sc->aue_stat_ch, hz, aue_tick, sc);

	splx(s);
}

Static int
aue_send(struct aue_softc *sc, struct mbuf *m, int idx)
{
	int			total_len;
	struct aue_chain	*c;
	usbd_status		err;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev),__func__));

	c = &sc->aue_cdata.aue_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	m_copydata(m, 0, m->m_pkthdr.len, c->aue_buf + 2);
	c->aue_mbuf = m;

	/*
	 * The ADMtek documentation says that the packet length is
	 * supposed to be specified in the first two bytes of the
	 * transfer, however it actually seems to ignore this info
	 * and base the frame size on the bulk transfer length.
	 */
	c->aue_buf[0] = (u_int8_t)m->m_pkthdr.len;
	c->aue_buf[1] = (u_int8_t)(m->m_pkthdr.len >> 8);
	total_len = m->m_pkthdr.len + 2;

	usbd_setup_xfer(c->aue_xfer, sc->aue_ep[AUE_ENDPT_TX],
	    c, c->aue_buf, total_len, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    AUE_TX_TIMEOUT, aue_txeof);

	/* Transmit */
	err = usbd_transfer(c->aue_xfer);
	if (err != USBD_IN_PROGRESS) {
		printf("%s: aue_send error=%s\n", USBDEVNAME(sc->aue_dev),
		       usbd_errstr(err));
		/* Stop the interface from process context. */
		usb_add_task(sc->aue_udev, &sc->aue_stop_task);
		return (EIO);
	}
	DPRINTFN(5,("%s: %s: send %d bytes\n", USBDEVNAME(sc->aue_dev),
		    __func__, total_len));

	sc->aue_cdata.aue_tx_cnt++;

	return (0);
}

Static void
aue_start(struct ifnet *ifp)
{
	struct aue_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;

	DPRINTFN(5,("%s: %s: enter, link=%d\n", USBDEVNAME(sc->aue_dev),
		    __func__, sc->aue_link));

	if (sc->aue_dying)
		return;

	if (!sc->aue_link)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	IFQ_POLL(&ifp->if_snd, m_head);
	if (m_head == NULL)
		return;

	if (aue_send(sc, m_head, 0)) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	IFQ_DEQUEUE(&ifp->if_snd, m_head);

#if NBPFILTER > 0
	/*
	 * If there's a BPF listener, bounce a copy of this frame
	 * to him.
	 */
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m_head);
#endif

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

Static void
aue_init(void *xsc)
{
	struct aue_softc	*sc = xsc;
	struct ifnet		*ifp = GET_IFP(sc);
	struct mii_data		*mii = GET_MII(sc);
	int			i, s;
	u_char			*eaddr;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	if (sc->aue_dying)
		return;

	if (ifp->if_flags & IFF_RUNNING)
		return;

	s = splnet();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	aue_reset(sc);

#if defined(__OpenBSD__)
	eaddr = sc->arpcom.ac_enaddr;
#elif defined(__NetBSD__)
	eaddr = LLADDR(ifp->if_sadl);
#endif /* defined(__NetBSD__) */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		aue_csr_write_1(sc, AUE_PAR0 + i, eaddr[i]);

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
	else
		AUE_CLRBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);

	/* Init TX ring. */
	if (aue_tx_list_init(sc) == ENOBUFS) {
		printf("%s: tx list init failed\n", USBDEVNAME(sc->aue_dev));
		splx(s);
		return;
	}

	/* Init RX ring. */
	if (aue_rx_list_init(sc) == ENOBUFS) {
		printf("%s: rx list init failed\n", USBDEVNAME(sc->aue_dev));
		splx(s);
		return;
	}

	/* Load the multicast filter. */
	aue_setmulti(sc);

	/* Enable RX and TX */
	aue_csr_write_1(sc, AUE_CTL0, AUE_CTL0_RXSTAT_APPEND | AUE_CTL0_RX_ENB);
	AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_TX_ENB);
	AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_EP3_CLR);

	mii_mediachg(mii);

	if (sc->aue_ep[AUE_ENDPT_RX] == NULL) {
		if (aue_openpipes(sc)) {
			splx(s);
			return;
		}
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	usb_callout(sc->aue_stat_ch, hz, aue_tick, sc);
}

Static int
aue_openpipes(struct aue_softc *sc)
{
	struct aue_chain	*c;
	usbd_status		err;
	int i;

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->aue_iface, sc->aue_ed[AUE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->aue_ep[AUE_ENDPT_RX]);
	if (err) {
		printf("%s: open rx pipe failed: %s\n",
		    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		return (EIO);
	}
	err = usbd_open_pipe(sc->aue_iface, sc->aue_ed[AUE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->aue_ep[AUE_ENDPT_TX]);
	if (err) {
		printf("%s: open tx pipe failed: %s\n",
		    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		return (EIO);
	}
	err = usbd_open_pipe_intr(sc->aue_iface, sc->aue_ed[AUE_ENDPT_INTR],
	    USBD_EXCLUSIVE_USE, &sc->aue_ep[AUE_ENDPT_INTR], sc,
	    &sc->aue_cdata.aue_ibuf, AUE_INTR_PKTLEN, aue_intr,
	    AUE_INTR_INTERVAL);
	if (err) {
		printf("%s: open intr pipe failed: %s\n",
		    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		return (EIO);
	}

	/* Start up the receive pipe. */
	for (i = 0; i < AUE_RX_LIST_CNT; i++) {
		c = &sc->aue_cdata.aue_rx_chain[i];
		usbd_setup_xfer(c->aue_xfer, sc->aue_ep[AUE_ENDPT_RX],
		    c, c->aue_buf, AUE_BUFSZ,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY, USBD_NO_TIMEOUT,
		    aue_rxeof);
		(void)usbd_transfer(c->aue_xfer); /* XXX */
		DPRINTFN(5,("%s: %s: start read\n", USBDEVNAME(sc->aue_dev),
			    __func__));

	}
	return (0);
}

/*
 * Set media options.
 */
Static int
aue_ifmedia_upd(struct ifnet *ifp)
{
	struct aue_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = GET_MII(sc);

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	if (sc->aue_dying)
		return (0);

	sc->aue_link = 0;
	if (mii->mii_instance) {
		struct mii_softc	*miisc;
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
		    miisc = LIST_NEXT(miisc, mii_list))
			 mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return (0);
}

/*
 * Report current media status.
 */
Static void
aue_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct aue_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = GET_MII(sc);

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

Static int
aue_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct aue_softc	*sc = ifp->if_softc;
	struct ifaddr 		*ifa = (struct ifaddr *)data;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct mii_data		*mii;
	int			s, error = 0;

	if (sc->aue_dying)
		return (EIO);

	s = splnet();

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		aue_init(sc);

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
#if defined(__NetBSD__)
			arp_ifinit(ifp, ifa);
#else
			arp_ifinit(&sc->arpcom, ifa);
#endif
			break;
#endif /* INET */
#ifdef NS
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host = *(union ns_host *)
					LLADDR(ifp->if_sadl);
			else
				memcpy(LLADDR(ifp->if_sadl),
				       ina->x_host.c_host,
				       ifp->if_addrlen);
			break;
		    }
#endif /* NS */
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ETHERMTU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->aue_if_flags & IFF_PROMISC)) {
				AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->aue_if_flags & IFF_PROMISC) {
				AUE_CLRBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				aue_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				aue_stop(sc);
		}
		sc->aue_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (command == SIOCADDMULTI) ?
			ether_addmulti(ifr, &sc->aue_ec) :
			ether_delmulti(ifr, &sc->aue_ec);
		if (error == ENETRESET) {
			aue_init(sc);
		}
		aue_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = GET_MII(sc);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);

	return (error);
}

Static void
aue_watchdog(struct ifnet *ifp)
{
	struct aue_softc	*sc = ifp->if_softc;
	struct aue_chain	*c;
	usbd_status		stat;
	int			s;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", USBDEVNAME(sc->aue_dev));

	s = splusb();
	c = &sc->aue_cdata.aue_tx_chain[0];
	usbd_get_xfer_status(c->aue_xfer, NULL, NULL, NULL, &stat);
	aue_txeof(c->aue_xfer, c, stat);

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		aue_start(ifp);
	splx(s);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
Static void
aue_stop(struct aue_softc *sc)
{
	usbd_status		err;
	struct ifnet		*ifp;
	int			i;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->aue_dev), __func__));

	ifp = GET_IFP(sc);
	ifp->if_timer = 0;

	aue_csr_write_1(sc, AUE_CTL0, 0);
	aue_csr_write_1(sc, AUE_CTL1, 0);
	aue_reset(sc);
	usb_uncallout(sc->aue_stat_ch, aue_tick, sc);

	/* Stop transfers. */
	if (sc->aue_ep[AUE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_RX]);
		if (err) {
			printf("%s: abort rx pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_RX]);
		if (err) {
			printf("%s: close rx pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_RX] = NULL;
	}

	if (sc->aue_ep[AUE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_TX]);
		if (err) {
			printf("%s: abort tx pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_TX]);
		if (err) {
			printf("%s: close tx pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_TX] = NULL;
	}

	if (sc->aue_ep[AUE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_INTR]);
		if (err) {
			printf("%s: abort intr pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_INTR]);
		if (err) {
			printf("%s: close intr pipe failed: %s\n",
			    USBDEVNAME(sc->aue_dev), usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_INTR] = NULL;
	}

	/* Free RX resources. */
	for (i = 0; i < AUE_RX_LIST_CNT; i++) {
		if (sc->aue_cdata.aue_rx_chain[i].aue_mbuf != NULL) {
			m_freem(sc->aue_cdata.aue_rx_chain[i].aue_mbuf);
			sc->aue_cdata.aue_rx_chain[i].aue_mbuf = NULL;
		}
		if (sc->aue_cdata.aue_rx_chain[i].aue_xfer != NULL) {
			usbd_free_xfer(sc->aue_cdata.aue_rx_chain[i].aue_xfer);
			sc->aue_cdata.aue_rx_chain[i].aue_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < AUE_TX_LIST_CNT; i++) {
		if (sc->aue_cdata.aue_tx_chain[i].aue_mbuf != NULL) {
			m_freem(sc->aue_cdata.aue_tx_chain[i].aue_mbuf);
			sc->aue_cdata.aue_tx_chain[i].aue_mbuf = NULL;
		}
		if (sc->aue_cdata.aue_tx_chain[i].aue_xfer != NULL) {
			usbd_free_xfer(sc->aue_cdata.aue_tx_chain[i].aue_xfer);
			sc->aue_cdata.aue_tx_chain[i].aue_xfer = NULL;
		}
	}

	sc->aue_link = 0;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}
