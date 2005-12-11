/*	$NetBSD: if_axe.c,v 1.6.2.5 2005/12/11 10:29:05 christos Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999, 2000-2003
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

/*
 * ASIX Electronics AX88172 USB 2.0 ethernet driver. Used in the
 * LinkSys USB200M and various other adapters.
 *
 * Manuals available from:
 * http://www.asix.com.tw/datasheet/mac/Ax88172.PDF
 * (also http://people.freebsd.org/~wpaul/ASIX/Ax88172.PDF)
 * Note: you need the manual for the AX88170 chip (USB 1.x ethernet
 * controller) to find the definitions for the RX control register.
 * http://www.asix.com.tw/datasheet/mac/Ax88170.PDF
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Engineer
 * Wind River Systems
 */

/*
 * The AX88172 provides USB ethernet supports at 10 and 100Mbps.
 * It uses an external PHY (reference designs use a RealTek chip),
 * and has a 64-bit multicast hash filter. There is some information
 * missing from the manual which one needs to know in order to make
 * the chip function:
 *
 * - You must set bit 7 in the RX control register, otherwise the
 *   chip won't receive any packets.
 * - You must initialize all 3 IPG registers, or you won't be able
 *   to send any packets.
 *
 * Note that this device appears to only support loading the station
 * address via autload from the EEPROM (i.e. there's no way to manaully
 * set it).
 *
 * (Adam Weinberger wanted me to name this driver if_gir.c.)
 */

/*
 * Ported to OpenBSD 3/28/2004 by Greg Taleck <taleck@oz.net>
 * with bits and pieces from the aue and url drivers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_axe.c,v 1.6.2.5 2005/12/11 10:29:05 christos Exp $");

#if defined(__NetBSD__)
#include "opt_inet.h"
#include "opt_ns.h"
#include "rnd.h"
#endif

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#if defined(__OpenBSD__)
#include <sys/proc.h>
#endif
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

#include <dev/usb/if_axereg.h>

#ifdef AXE_DEBUG
#define DPRINTF(x)	do { if (axedebug) logprintf x; } while (0)
#define DPRINTFN(n,x)	do { if (axedebug >= (n)) logprintf x; } while (0)
int	axedebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * Various supported device vendors/products.
 */
Static const struct axe_type axe_devs[] = {
	{ { USB_VENDOR_ASIX,		USB_PRODUCT_ASIX_AX88172}, 0 },
	{ { USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_FETHER_USB2_TX }, 0},
	{ { USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DUBE100}, 0 },
	{ { USB_VENDOR_LINKSYS2,	USB_PRODUCT_LINKSYS2_USB200M}, 0 },
	{ { USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_LUAU2KTX}, 0 },
	{ { USB_VENDOR_NETGEAR,		USB_PRODUCT_NETGEAR_FA120}, 0 },
	{ { USB_VENDOR_SITECOM,		USB_PRODUCT_SITECOM_LN029}, 0 },
	{ { USB_VENDOR_SYSTEMTALKS,	USB_PRODUCT_SYSTEMTALKS_SGCX2UL}, 0 },
};
#define axe_lookup(v, p) ((const struct axe_type *)usb_lookup(axe_devs, v, p))

USB_DECLARE_DRIVER(axe);

Static int axe_tx_list_init(struct axe_softc *);
Static int axe_rx_list_init(struct axe_softc *);
Static int axe_newbuf(struct axe_softc *, struct axe_chain *, struct mbuf *);
Static int axe_encap(struct axe_softc *, struct mbuf *, int);
Static void axe_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void axe_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void axe_tick(void *);
Static void axe_tick_task(void *);
#if 0
Static void axe_rxstart(struct ifnet *);
#endif
Static void axe_start(struct ifnet *);
Static int axe_ioctl(struct ifnet *, u_long, caddr_t);
Static void axe_init(void *);
Static void axe_stop(struct axe_softc *);
Static void axe_watchdog(struct ifnet *);
Static int axe_miibus_readreg(device_ptr_t, int, int);
Static void axe_miibus_writereg(device_ptr_t, int, int, int);
Static void axe_miibus_statchg(device_ptr_t);
Static int axe_cmd(struct axe_softc *, int, int, int, void *);
Static int axe_ifmedia_upd(struct ifnet *);
Static void axe_ifmedia_sts(struct ifnet *, struct ifmediareq *);
Static void axe_reset(struct axe_softc *sc);

Static void axe_setmulti(struct axe_softc *);
Static void axe_lock_mii(struct axe_softc *sc);
Static void axe_unlock_mii(struct axe_softc *sc);

/* Get exclusive access to the MII registers */
Static void
axe_lock_mii(struct axe_softc *sc)
{
	sc->axe_refcnt++;
	usb_lockmgr(&sc->axe_mii_lock, LK_EXCLUSIVE, NULL);
}

Static void
axe_unlock_mii(struct axe_softc *sc)
{
	usb_lockmgr(&sc->axe_mii_lock, LK_RELEASE, NULL);
	if (--sc->axe_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->axe_dev));
}

Static int
axe_cmd(struct axe_softc *sc, int cmd, int index, int val, void *buf)
{
	usb_device_request_t	req;
	usbd_status		err;

	if (sc->axe_dying)
		return(0);

	axe_lock_mii(sc);
	if (AXE_CMD_DIR(cmd))
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	else
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AXE_CMD_CMD(cmd);
	USETW(req.wValue, val);
	USETW(req.wIndex, index);
	USETW(req.wLength, AXE_CMD_LEN(cmd));

	err = usbd_do_request(sc->axe_udev, &req, buf);
	axe_unlock_mii(sc);

	if (err)
		return(-1);

	return(0);
}

Static int
axe_miibus_readreg(device_ptr_t dev, int phy, int reg)
{
	struct axe_softc	*sc = USBGETSOFTC(dev);
	usbd_status		err;
	u_int16_t		val;

	if (sc->axe_dying) {
		DPRINTF(("axe: dying\n"));
		return(0);
	}

#ifdef notdef
	/*
	 * The chip tells us the MII address of any supported
	 * PHYs attached to the chip, so only read from those.
	 */

	if (sc->axe_phyaddrs[0] != AXE_NOPHY && phy != sc->axe_phyaddrs[0])
		return (0);

	if (sc->axe_phyaddrs[1] != AXE_NOPHY && phy != sc->axe_phyaddrs[1])
		return (0);
#endif
	if (sc->axe_phyaddrs[0] != 0xFF && sc->axe_phyaddrs[0] != phy)
		return (0);

	val = 0;

	axe_lock_mii(sc);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	err = axe_cmd(sc, AXE_CMD_MII_READ_REG, reg, phy, (void *)&val);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);
	axe_unlock_mii(sc);

	if (err) {
		printf("%s: read PHY failed\n", USBDEVNAME(sc->axe_dev));
		return(-1);
	}

	if (val)
		sc->axe_phyaddrs[0] = phy;

	return (le16toh(val));
}

Static void
axe_miibus_writereg(device_ptr_t dev, int phy, int reg, int aval)
{
	struct axe_softc	*sc = USBGETSOFTC(dev);
	usbd_status		err;
	u_int16_t		val;

	if (sc->axe_dying)
		return;

	val = htole16(aval);
	axe_lock_mii(sc);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_SW, 0, 0, NULL);
	err = axe_cmd(sc, AXE_CMD_MII_WRITE_REG, reg, phy, (void *)&val);
	axe_cmd(sc, AXE_CMD_MII_OPMODE_HW, 0, 0, NULL);
	axe_unlock_mii(sc);

	if (err) {
		printf("%s: write PHY failed\n", USBDEVNAME(sc->axe_dev));
		return;
	}
}

Static void
axe_miibus_statchg(device_ptr_t dev)
{
	struct axe_softc	*sc = USBGETSOFTC(dev);
	struct mii_data		*mii = GET_MII(sc);
	int val, err;

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		val = AXE_MEDIA_FULL_DUPLEX;
	else
		val = 0;
	DPRINTF(("axe_miibus_statchg: val=0x%x\n", val));
	err = axe_cmd(sc, AXE_CMD_WRITE_MEDIA, 0, val, NULL);
	if (err) {
		printf("%s: media change failed\n", USBDEVNAME(sc->axe_dev));
		return;
	}
}

/*
 * Set media options.
 */
Static int
axe_ifmedia_upd(struct ifnet *ifp)
{
        struct axe_softc        *sc = ifp->if_softc;
        struct mii_data         *mii = GET_MII(sc);

        sc->axe_link = 0;
        if (mii->mii_instance) {
                struct mii_softc        *miisc;
                LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
                         mii_phy_reset(miisc);
        }
        mii_mediachg(mii);

        return (0);
}

/*
 * Report current media status.
 */
Static void
axe_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
        struct axe_softc        *sc = ifp->if_softc;
        struct mii_data         *mii = GET_MII(sc);

        mii_pollstat(mii);
        ifmr->ifm_active = mii->mii_media_active;
        ifmr->ifm_status = mii->mii_media_status;
}

Static void
axe_setmulti(struct axe_softc *sc)
{
	struct ifnet		*ifp;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int32_t		h = 0;
	u_int16_t		rxmode;
	u_int8_t		hashtbl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	if (sc->axe_dying)
		return;

	ifp = GET_IFP(sc);

	axe_cmd(sc, AXE_CMD_RXCTL_READ, 0, 0, (void *)&rxmode);
	rxmode = le16toh(rxmode);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
	allmulti:
		rxmode |= AXE_RXCMD_ALLMULTI;
		axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
		return;
	} else
		rxmode &= ~AXE_RXCMD_ALLMULTI;

	/* now program new ones */
#if defined(__NetBSD__)
	ETHER_FIRST_MULTI(step, &sc->axe_ec, enm);
#else
	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
#endif
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
			   ETHER_ADDR_LEN) != 0)
			goto allmulti;

		h = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN) >> 26;
		hashtbl[h / 8] |= 1 << (h % 8);
		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;
	axe_cmd(sc, AXE_CMD_WRITE_MCAST, 0, 0, (void *)&hashtbl);
	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);
	return;
}

Static void
axe_reset(struct axe_softc *sc)
{
	if (sc->axe_dying)
		return;
	/* XXX What to reset? */

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
	return;
}

/*
 * Probe for a AX88172 chip.
 */
USB_MATCH(axe)
{
	USB_MATCH_START(axe, uaa);

	if (!uaa->iface) {
		return(UMATCH_NONE);
	}

	return (axe_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
USB_ATTACH(axe)
{
	USB_ATTACH_START(axe, sc, uaa);
	usbd_device_handle dev = uaa->device;
	usbd_status err;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	struct mii_data	*mii;
	u_char eaddr[ETHER_ADDR_LEN];
	char *devinfop;
	char *devname = USBDEVNAME(sc->axe_dev);
	struct ifnet *ifp;
	int i, s;

	devinfop = usbd_devinfo_alloc(dev, 0);
	USB_ATTACH_SETUP;

	err = usbd_set_config_no(dev, AXE_CONFIG_NO, 1);
	if (err) {
		printf("%s: getting interface handle failed\n",
		    USBDEVNAME(sc->axe_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	usb_init_task(&sc->axe_tick_task, axe_tick_task, sc);
	lockinit(&sc->axe_mii_lock, PZERO, "axemii", 0, LK_CANRECURSE);
	usb_init_task(&sc->axe_stop_task, (void (*)(void *))axe_stop, sc);

	err = usbd_device2interface_handle(dev, AXE_IFACE_IDX, &sc->axe_iface);
	if (err) {
		printf("%s: getting interface handle failed\n",
		    USBDEVNAME(sc->axe_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->axe_udev = dev;
	sc->axe_product = uaa->product;
	sc->axe_vendor = uaa->vendor;

	id = usbd_get_interface_descriptor(sc->axe_iface);

	printf("%s: %s\n", USBDEVNAME(sc->axe_dev), devinfop);
	usbd_devinfo_free(devinfop);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->axe_iface, i);
		if (!ed) {
			printf("%s: couldn't get ep %d\n",
			    USBDEVNAME(sc->axe_dev), i);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->axe_ed[AXE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->axe_ed[AXE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->axe_ed[AXE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	s = splnet();

	/*
	 * Get station address.
	 */
	axe_cmd(sc, AXE_CMD_READ_NODEID, 0, 0, &eaddr);

	/*
	 * Load IPG values and PHY indexes.
	 */
	axe_cmd(sc, AXE_CMD_READ_IPG012, 0, 0, (void *)&sc->axe_ipgs);
	axe_cmd(sc, AXE_CMD_READ_PHYID, 0, 0, (void *)&sc->axe_phyaddrs);

	/*
	 * Work around broken adapters that appear to lie about
	 * their PHY addresses.
	 */
	sc->axe_phyaddrs[0] = sc->axe_phyaddrs[1] = 0xFF;

	/*
	 * An ASIX chip was detected. Inform the world.
	 */
	printf("%s: Ethernet address %s\n", USBDEVNAME(sc->axe_dev),
	    ether_sprintf(eaddr));

	/* Initialize interface info.*/
	ifp = GET_IFP(sc);
	ifp->if_softc = sc;
	strncpy(ifp->if_xname, devname, IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = axe_ioctl;
	ifp->if_start = axe_start;

	ifp->if_watchdog = axe_watchdog;

/*	ifp->if_baudrate = 10000000; */
/*	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;*/

	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize MII/media info. */
	mii = &sc->axe_mii;
	mii->mii_ifp = ifp;
	mii->mii_readreg = axe_miibus_readreg;
	mii->mii_writereg = axe_miibus_writereg;
	mii->mii_statchg = axe_miibus_statchg;
	mii->mii_flags = MIIF_AUTOTSLEEP;

	ifmedia_init(&mii->mii_media, 0, axe_ifmedia_upd, axe_ifmedia_sts);
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
	rnd_attach_source(&sc->rnd_source, USBDEVNAME(sc->axe_dev),
	    RND_TYPE_NET, 0);
#endif

	usb_callout_init(sc->axe_stat_ch);

	sc->axe_attached = 1;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->axe_udev,
			   USBDEV(sc->axe_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(axe)
{
	USB_DETACH_START(axe, sc);
	int			s;
	struct ifnet		*ifp = GET_IFP(sc);

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->axe_dev), __func__));

	/* Detached before attached finished, so just bail out. */
	if (!sc->axe_attached)
		return (0);

	usb_uncallout(sc->axe_stat_ch, axe_tick, sc);

	sc->axe_dying = 1;

	ether_ifdetach(ifp);

	if (sc->axe_ep[AXE_ENDPT_TX] != NULL)
		usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_TX]);
	if (sc->axe_ep[AXE_ENDPT_RX] != NULL)
		usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_RX]);
	if (sc->axe_ep[AXE_ENDPT_INTR] != NULL)
		usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_INTR]);

	/*
	 * Remove any pending tasks.  They cannot be executing because they run
	 * in the same thread as detach.
	 */
	usb_rem_task(sc->axe_udev, &sc->axe_tick_task);
	usb_rem_task(sc->axe_udev, &sc->axe_stop_task);

	s = splusb();

	if (--sc->axe_refcnt >= 0) {
		/* Wait for processes to go away */
		usb_detach_wait(USBDEV(sc->axe_dev));
	}

	if (ifp->if_flags & IFF_RUNNING)
		axe_stop(sc);

#if defined(__NetBSD__)
#if NRND > 0
	rnd_detach_source(&sc->rnd_source);
#endif
#endif /* __NetBSD__ */
	mii_detach(&sc->axe_mii, MII_PHY_ANY, MII_OFFSET_ANY);
	ifmedia_delete_instance(&sc->axe_mii.mii_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);

#ifdef DIAGNOSTIC
	if (sc->axe_ep[AXE_ENDPT_TX] != NULL ||
	    sc->axe_ep[AXE_ENDPT_RX] != NULL ||
	    sc->axe_ep[AXE_ENDPT_INTR] != NULL)
		printf("%s: detach has active endpoints\n",
		       USBDEVNAME(sc->axe_dev));
#endif

	sc->axe_attached = 0;

	if (--sc->axe_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->axe_dev));
	}
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->axe_udev,
			   USBDEV(sc->axe_dev));

	return (0);
}

int
axe_activate(device_ptr_t self, enum devact act)
{
	struct axe_softc *sc = (struct axe_softc *)self;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->axe_dev), __func__));

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		if_deactivate(&sc->axe_ec.ec_if);
		sc->axe_dying = 1;
		break;
	}
	return (0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
Static int
axe_newbuf(struct axe_softc *sc, struct axe_chain *c, struct mbuf *m)
{
	struct mbuf		*m_new = NULL;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->axe_dev),__func__));

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", USBDEVNAME(sc->axe_dev));
			return (ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", USBDEVNAME(sc->axe_dev));
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
	c->axe_mbuf = m_new;

	return (0);
}

Static int
axe_rx_list_init(struct axe_softc *sc)
{
	struct axe_cdata *cd;
	struct axe_chain *c;
	int i;

	DPRINTF(("%s: %s: enter\n", USBDEVNAME(sc->axe_dev), __func__));

	cd = &sc->axe_cdata;
	for (i = 0; i < AXE_RX_LIST_CNT; i++) {
		c = &cd->axe_rx_chain[i];
		c->axe_sc = sc;
		c->axe_idx = i;
		if (axe_newbuf(sc, c, NULL) == ENOBUFS)
			return (ENOBUFS);
		if (c->axe_xfer == NULL) {
			c->axe_xfer = usbd_alloc_xfer(sc->axe_udev);
			if (c->axe_xfer == NULL)
				return (ENOBUFS);
			c->axe_buf = usbd_alloc_buffer(c->axe_xfer, AXE_BUFSZ);
			if (c->axe_buf == NULL) {
				usbd_free_xfer(c->axe_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

Static int
axe_tx_list_init(struct axe_softc *sc)
{
	struct axe_cdata *cd;
	struct axe_chain *c;
	int i;

	DPRINTF(("%s: %s: enter\n", USBDEVNAME(sc->axe_dev), __func__));

	cd = &sc->axe_cdata;
	for (i = 0; i < AXE_TX_LIST_CNT; i++) {
		c = &cd->axe_tx_chain[i];
		c->axe_sc = sc;
		c->axe_idx = i;
		c->axe_mbuf = NULL;
		if (c->axe_xfer == NULL) {
			c->axe_xfer = usbd_alloc_xfer(sc->axe_udev);
			if (c->axe_xfer == NULL)
				return (ENOBUFS);
			c->axe_buf = usbd_alloc_buffer(c->axe_xfer, AXE_BUFSZ);
			if (c->axe_buf == NULL) {
				usbd_free_xfer(c->axe_xfer);
				return (ENOBUFS);
			}
		}
	}

	return (0);
}

#if 0
Static void
axe_rxstart(struct ifnet *ifp)
{
	struct axe_softc	*sc;
	struct axe_chain	*c;

	sc = ifp->if_softc;
	axe_lock_mii(sc);
	c = &sc->axe_cdata.axe_rx_chain[sc->axe_cdata.axe_rx_prod];

	if (axe_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		axe_unlock_mii(sc);
		return;
	}

	/* Setup new transfer. */
	usbd_setup_xfer(c->axe_xfer, sc->axe_ep[AXE_ENDPT_RX],
	    c, mtod(c->axe_mbuf, char *), AXE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, axe_rxeof);
	usbd_transfer(c->axe_xfer);
	axe_unlock_mii(sc);

	return;
}
#endif

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
Static void
axe_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct axe_softc	*sc;
	struct axe_chain	*c;
	struct ifnet		*ifp;
	struct mbuf		*m;
	u_int32_t		total_len;
	int			s;

	c = priv;
	sc = c->axe_sc;
	ifp = GET_IFP(sc);

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->axe_dev),__func__));

	if (sc->axe_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		if (usbd_ratecheck(&sc->axe_rx_notice)) {
			printf("%s: usb errors on rx: %s\n",
			    USBDEVNAME(sc->axe_dev), usbd_errstr(status));
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->axe_ep[AXE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	m = c->axe_mbuf;

	if (total_len <= sizeof(struct ether_header)) {
		ifp->if_ierrors++;
		goto done;
	}

	ifp->if_ipackets++;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = total_len;


	memcpy(mtod(c->axe_mbuf, char *), c->axe_buf, total_len);

	/* No errors; receive the packet. */
	total_len -= ETHER_CRC_LEN + 4;

	s = splnet();

	/* XXX ugly */
	if (axe_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		goto done1;
	}

#if NBPFILTER > 0
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m);
#endif

	DPRINTFN(10,("%s: %s: deliver %d\n", USBDEVNAME(sc->axe_dev),
		    __func__, m->m_len));
	IF_INPUT(ifp, m);
 done1:
	splx(s);

 done:

	/* Setup new transfer. */
	usbd_setup_xfer(xfer, sc->axe_ep[AXE_ENDPT_RX],
	    c, c->axe_buf, AXE_BUFSZ,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, axe_rxeof);
	usbd_transfer(xfer);

	DPRINTFN(10,("%s: %s: start rx\n", USBDEVNAME(sc->axe_dev),
		    __func__));
	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

Static void
axe_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct axe_softc	*sc;
	struct axe_chain	*c;
	struct ifnet		*ifp;
	int			s;

	c = priv;
	sc = c->axe_sc;
	ifp = GET_IFP(sc);

	if (sc->axe_dying)
		return;

	s = splnet();

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}
		ifp->if_oerrors++;
		printf("%s: usb error on tx: %s\n", USBDEVNAME(sc->axe_dev),
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->axe_ep[AXE_ENDPT_TX]);
		splx(s);
		return;
	}

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	m_freem(c->axe_mbuf);
	c->axe_mbuf = NULL;

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		axe_start(ifp);

	ifp->if_opackets++;
	splx(s);
	return;
}

Static void
axe_tick(void *xsc)
{
	struct axe_softc *sc = xsc;

	if (sc == NULL)
		return;

	DPRINTFN(0xff, ("%s: %s: enter\n", USBDEVNAME(sc->axe_dev),
			__func__));

	if (sc->axe_dying)
		return;

	/* Perform periodic stuff in process context */
	usb_add_task(sc->axe_udev, &sc->axe_tick_task);

}

Static void
axe_tick_task(void *xsc)
{
	int			s;
	struct axe_softc	*sc;
	struct ifnet		*ifp;
	struct mii_data		*mii;

	sc = xsc;

	if (sc == NULL)
		return;

	if (sc->axe_dying)
		return;

	ifp = GET_IFP(sc);
	mii = GET_MII(sc);
	if (mii == NULL)
		return;

	s = splnet();

	mii_tick(mii);
	if (!sc->axe_link) {
		mii_pollstat(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			DPRINTF(("%s: %s: got link\n",
				 USBDEVNAME(sc->axe_dev), __func__));
			sc->axe_link++;
			if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
				   axe_start(ifp);
		}
	}

	usb_callout(sc->axe_stat_ch, hz, axe_tick, sc);

	splx(s);
}

Static int
axe_encap(struct axe_softc *sc, struct mbuf *m, int idx)
{
	struct axe_chain	*c;
	usbd_status		err;

	c = &sc->axe_cdata.axe_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	m_copydata(m, 0, m->m_pkthdr.len, c->axe_buf);
	c->axe_mbuf = m;

	usbd_setup_xfer(c->axe_xfer, sc->axe_ep[AXE_ENDPT_TX],
	    c, c->axe_buf, m->m_pkthdr.len, USBD_FORCE_SHORT_XFER, 10000,
	    axe_txeof);

	/* Transmit */
	err = usbd_transfer(c->axe_xfer);
	if (err != USBD_IN_PROGRESS) {
		axe_stop(sc);
		return(EIO);
	}

	sc->axe_cdata.axe_tx_cnt++;

	return(0);
}

Static void
axe_start(struct ifnet *ifp)
{
	struct axe_softc	*sc;
	struct mbuf		*m_head = NULL;

	sc = ifp->if_softc;

	if (!sc->axe_link) {
		return;
	}

	if (ifp->if_flags & IFF_OACTIVE) {
		return;
	}

	IF_DEQUEUE(&ifp->if_snd, m_head);
	if (m_head == NULL) {
		return;
	}

	if (axe_encap(sc, m_head, 0)) {
		IF_PREPEND(&ifp->if_snd, m_head);
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	/*
	 * If there's a BPF listener, bounce a copy of this frame
	 * to him.
	 */
#if NBPFILTER > 0
	 if (ifp->if_bpf)
	 	BPF_MTAP(ifp, m_head);
#endif

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

Static void
axe_init(void *xsc)
{
	struct axe_softc	*sc = xsc;
	struct ifnet		*ifp = GET_IFP(sc);
	struct axe_chain	*c;
	usbd_status		err;
	int			rxmode;
	int			i, s;

	if (ifp->if_flags & IFF_RUNNING)
		return;

	s = splnet();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	axe_reset(sc);

	/* Enable RX logic. */

	/* Init RX ring. */
	if (axe_rx_list_init(sc) == ENOBUFS) {
		printf("%s: rx list init failed\n", USBDEVNAME(sc->axe_dev));
		splx(s);
		return;
	}

	/* Init TX ring. */
	if (axe_tx_list_init(sc) == ENOBUFS) {
		printf("%s: tx list init failed\n", USBDEVNAME(sc->axe_dev));
		splx(s);
		return;
	}

	/* Set transmitter IPG values */
	axe_cmd(sc, AXE_CMD_WRITE_IPG0, 0, sc->axe_ipgs[0], NULL);
	axe_cmd(sc, AXE_CMD_WRITE_IPG1, 0, sc->axe_ipgs[1], NULL);
	axe_cmd(sc, AXE_CMD_WRITE_IPG2, 0, sc->axe_ipgs[2], NULL);

	/* Enable receiver, set RX mode */
	rxmode = AXE_RXCMD_UNICAST|AXE_RXCMD_MULTICAST|AXE_RXCMD_ENABLE;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		rxmode |= AXE_RXCMD_PROMISC;

	if (ifp->if_flags & IFF_BROADCAST)
		rxmode |= AXE_RXCMD_BROADCAST;

	axe_cmd(sc, AXE_CMD_RXCTL_WRITE, 0, rxmode, NULL);

	/* Load the multicast filter. */
	axe_setmulti(sc);

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->axe_iface, sc->axe_ed[AXE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->axe_ep[AXE_ENDPT_RX]);
	if (err) {
		printf("%s: open rx pipe failed: %s\n",
		    USBDEVNAME(sc->axe_dev), usbd_errstr(err));
		splx(s);
		return;
	}

	err = usbd_open_pipe(sc->axe_iface, sc->axe_ed[AXE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->axe_ep[AXE_ENDPT_TX]);
	if (err) {
		printf("%s: open tx pipe failed: %s\n",
		    USBDEVNAME(sc->axe_dev), usbd_errstr(err));
		splx(s);
		return;
	}

	/* Start up the receive pipe. */
	for (i = 0; i < AXE_RX_LIST_CNT; i++) {
		c = &sc->axe_cdata.axe_rx_chain[i];
		usbd_setup_xfer(c->axe_xfer, sc->axe_ep[AXE_ENDPT_RX],
		    c, mtod(c->axe_mbuf, char *), AXE_BUFSZ,
		    USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, axe_rxeof);
		usbd_transfer(c->axe_xfer);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	usb_callout_init(sc->axe_stat_ch);
	usb_callout(sc->axe_stat_ch, hz, axe_tick, sc);
	return;
}

Static int
axe_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct axe_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	struct mii_data		*mii;
	u_int16_t		rxmode;
	int			error = 0;

	switch(cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		axe_init(sc);

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
			    !(sc->axe_if_flags & IFF_PROMISC)) {

				axe_cmd(sc, AXE_CMD_RXCTL_READ,
					0, 0, (void *)&rxmode);
				rxmode = le16toh(rxmode) | AXE_RXCMD_PROMISC;
				axe_cmd(sc, AXE_CMD_RXCTL_WRITE,
					0, rxmode, NULL);

				axe_setmulti(sc);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->axe_if_flags & IFF_PROMISC) {
				axe_cmd(sc, AXE_CMD_RXCTL_READ,
					0, 0, (void *)&rxmode);
				rxmode = le16toh(rxmode) & ~AXE_RXCMD_PROMISC;
				axe_cmd(sc, AXE_CMD_RXCTL_WRITE,
					0, rxmode, NULL);
				axe_setmulti(sc);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				axe_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				axe_stop(sc);
		}
		sc->axe_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
#ifdef __NetBSD__
		error = (cmd == SIOCADDMULTI) ?
			ether_addmulti(ifr, &sc->axe_ec) :
			ether_delmulti(ifr, &sc->axe_ec);
#else
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->arpcom) :
		    ether_delmulti(ifr, &sc->arpcom);
#endif /* __NetBSD__ */
		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				axe_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = GET_MII(sc);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;

	default:
		error = EINVAL;
		break;
	}

	return(error);
}

/*
 * XXX
 * You can't call axe_txeof since the USB transfer has not
 * completed yet.
 */
Static void
axe_watchdog(struct ifnet *ifp)
{
	struct axe_softc	*sc;
	struct axe_chain	*c;
	usbd_status		stat;
	int			s;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", USBDEVNAME(sc->axe_dev));

	s = splusb();
	c = &sc->axe_cdata.axe_tx_chain[0];
	usbd_get_xfer_status(c->axe_xfer, NULL, NULL, NULL, &stat);
	axe_txeof(c->axe_xfer, c, stat);

	if (ifp->if_snd.ifq_head != NULL)
		axe_start(ifp);
	splx(s);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
Static void
axe_stop(struct axe_softc *sc)
{
	usbd_status		err;
	struct ifnet		*ifp;
	int			i;

	axe_reset(sc);

	ifp = GET_IFP(sc);
	ifp->if_timer = 0;

	usb_uncallout(sc->axe_stat_ch, axe_tick, sc);

	/* Stop transfers. */
	if (sc->axe_ep[AXE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_RX]);
		if (err) {
			printf("%s: abort rx pipe failed: %s\n",
		    	    USBDEVNAME(sc->axe_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_RX]);
		if (err) {
			printf("%s: close rx pipe failed: %s\n",
		    	    USBDEVNAME(sc->axe_dev), usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_RX] = NULL;
	}

	if (sc->axe_ep[AXE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_TX]);
		if (err) {
			printf("%s: abort tx pipe failed: %s\n",
		    	    USBDEVNAME(sc->axe_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_TX]);
		if (err) {
			printf("%s: close tx pipe failed: %s\n",
			    USBDEVNAME(sc->axe_dev), usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_TX] = NULL;
	}

	if (sc->axe_ep[AXE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->axe_ep[AXE_ENDPT_INTR]);
		if (err) {
			printf("%s: abort intr pipe failed: %s\n",
		    	    USBDEVNAME(sc->axe_dev), usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->axe_ep[AXE_ENDPT_INTR]);
		if (err) {
			printf("%s: close intr pipe failed: %s\n",
			    USBDEVNAME(sc->axe_dev), usbd_errstr(err));
		}
		sc->axe_ep[AXE_ENDPT_INTR] = NULL;
	}

	/* Free RX resources. */
	for (i = 0; i < AXE_RX_LIST_CNT; i++) {
		if (sc->axe_cdata.axe_rx_chain[i].axe_mbuf != NULL) {
			m_freem(sc->axe_cdata.axe_rx_chain[i].axe_mbuf);
			sc->axe_cdata.axe_rx_chain[i].axe_mbuf = NULL;
		}
		if (sc->axe_cdata.axe_rx_chain[i].axe_xfer != NULL) {
			usbd_free_xfer(sc->axe_cdata.axe_rx_chain[i].axe_xfer);
			sc->axe_cdata.axe_rx_chain[i].axe_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < AXE_TX_LIST_CNT; i++) {
		if (sc->axe_cdata.axe_tx_chain[i].axe_mbuf != NULL) {
			m_freem(sc->axe_cdata.axe_tx_chain[i].axe_mbuf);
			sc->axe_cdata.axe_tx_chain[i].axe_mbuf = NULL;
		}
		if (sc->axe_cdata.axe_tx_chain[i].axe_xfer != NULL) {
			usbd_free_xfer(sc->axe_cdata.axe_tx_chain[i].axe_xfer);
			sc->axe_cdata.axe_tx_chain[i].axe_xfer = NULL;
		}
	}

	sc->axe_link = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

