/*	$NetBSD: if_cue.c,v 1.48.10.2 2007/06/13 03:59:16 itohy Exp $	*/
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
 * $FreeBSD: src/sys/dev/usb/if_cue.c,v 1.4 2000/01/16 22:45:06 wpaul Exp $
 */

/*
 * CATC USB-EL1210A USB to ethernet driver. Used in the CATC Netmate
 * adapters and others.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The CATC USB-EL1210A provides USB ethernet support at 10Mbps. The
 * RX filter uses a 512-bit multicast hash table, single perfect entry
 * for the station address, and promiscuous mode. Unlike the ADMtek
 * and KLSI chips, the CATC ASIC supports read and write combining
 * mode where multiple packets can be transfered using a single bulk
 * transaction, which helps performance a great deal.
 */

/*
 * Ported to NetBSD and somewhat rewritten by Lennart Augustsson.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_cue.c,v 1.48.10.2 2007/06/13 03:59:16 itohy Exp $");

#if defined(__NetBSD__)
#include "opt_inet.h"
#include "bpfilter.h"
#include "rnd.h"
#elif defined(__OpenBSD__)
#include "bpfilter.h"
#endif /* defined(__OpenBSD__) */

#include <sys/param.h>
#include <sys/systm.h>
#if !defined(__OpenBSD__)
#include <sys/callout.h>
#endif
#include <sys/sockio.h>
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


#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_ethersubr.h>

#include <dev/usb/if_cuereg.h>

#ifdef CUE_DEBUG
#define DPRINTF(x)	if (cuedebug) logprintf x
#define DPRINTFN(n,x)	if (cuedebug >= (n)) logprintf x
int	cuedebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * Various supported device vendors/products.
 */
Static struct usb_devno cue_devs[] = {
	{ USB_VENDOR_CATC, USB_PRODUCT_CATC_NETMATE },
	{ USB_VENDOR_CATC, USB_PRODUCT_CATC_NETMATE2 },
	{ USB_VENDOR_SMARTBRIDGES, USB_PRODUCT_SMARTBRIDGES_SMARTLINK },
	/* Belkin F5U111 adapter covered by NETMATE entry */
};
#define cue_lookup(v, p) (usb_lookup(cue_devs, v, p))

USB_DECLARE_DRIVER(cue);

Static int cue_open_pipes(struct cue_softc *);
Static int cue_send(struct cue_softc *, struct mbuf *, int);
Static void cue_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void cue_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void cue_tick(void *);
Static void cue_tick_task(void *);
Static void cue_start(struct ifnet *);
Static int cue_ioctl(struct ifnet *, u_long, caddr_t);
Static int cue_init(struct ifnet *);
Static void cue_stop(struct ifnet *, int);
Static void cue_watchdog(struct ifnet *);

Static void cue_setmulti(struct cue_softc *);
Static u_int32_t cue_crc(const char *);
Static void cue_reset(struct cue_softc *);

Static int cue_csr_read_1(struct cue_softc *, int);
Static int cue_csr_write_1(struct cue_softc *, int, int);
Static int cue_csr_read_2(struct cue_softc *, int);
#if 0
Static int cue_csr_write_2(struct cue_softc *, int, int);
#endif
Static int cue_mem(struct cue_softc *, int, int, void *, int);
Static int cue_getmac(struct cue_softc *, void *);

#define CUE_SETBIT(sc, reg, x)				\
	cue_csr_write_1(sc, reg, cue_csr_read_1(sc, reg) | (x))

#define CUE_CLRBIT(sc, reg, x)				\
	cue_csr_write_1(sc, reg, cue_csr_read_1(sc, reg) & ~(x))

Static int
cue_csr_read_1(struct cue_softc	*sc, int reg)
{
	usb_device_request_t	req;
	usbd_status		err;
	u_int8_t		val = 0;

	if (sc->cue_dying)
		return (0);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	err = usbd_do_request(sc->cue_udev, &req, &val);

	if (err) {
		DPRINTF(("%s: cue_csr_read_1: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->cue_dev), reg, usbd_errstr(err)));
		return (0);
	}

	DPRINTFN(10,("%s: cue_csr_read_1 reg=0x%x val=0x%x\n",
		     USBDEVNAME(sc->cue_dev), reg, val));

	return (val);
}

Static int
cue_csr_read_2(struct cue_softc	*sc, int reg)
{
	usb_device_request_t	req;
	usbd_status		err;
	uWord			val;

	if (sc->cue_dying)
		return (0);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	err = usbd_do_request(sc->cue_udev, &req, &val);

	DPRINTFN(10,("%s: cue_csr_read_2 reg=0x%x val=0x%x\n",
		     USBDEVNAME(sc->cue_dev), reg, UGETW(val)));

	if (err) {
		DPRINTF(("%s: cue_csr_read_2: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->cue_dev), reg, usbd_errstr(err)));
		return (0);
	}

	return (UGETW(val));
}

Static int
cue_csr_write_1(struct cue_softc *sc, int reg, int val)
{
	usb_device_request_t	req;
	usbd_status		err;

	if (sc->cue_dying)
		return (0);

	DPRINTFN(10,("%s: cue_csr_write_1 reg=0x%x val=0x%x\n",
		     USBDEVNAME(sc->cue_dev), reg, val));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->cue_udev, &req, NULL);

	if (err) {
		DPRINTF(("%s: cue_csr_write_1: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->cue_dev), reg, usbd_errstr(err)));
		return (-1);
	}

	DPRINTFN(20,("%s: cue_csr_write_1, after reg=0x%x val=0x%x\n",
		     USBDEVNAME(sc->cue_dev), reg, cue_csr_read_1(sc, reg)));

	return (0);
}

#if 0
Static int
cue_csr_write_2(struct cue_softc *sc, int reg, int aval)
{
	usb_device_request_t	req;
	usbd_status		err;
	uWord			val;
	int			s;

	if (sc->cue_dying)
		return (0);

	DPRINTFN(10,("%s: cue_csr_write_2 reg=0x%x val=0x%x\n",
		     USBDEVNAME(sc->cue_dev), reg, aval));

	USETW(val, aval);
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->cue_udev, &req, NULL);

	if (err) {
		DPRINTF(("%s: cue_csr_write_2: reg=0x%x err=%s\n",
			 USBDEVNAME(sc->cue_dev), reg, usbd_errstr(err)));
		return (-1);
	}

	return (0);
}
#endif

Static int
cue_mem(struct cue_softc *sc, int cmd, int addr, void *buf, int len)
{
	usb_device_request_t	req;
	usbd_status		err;

	DPRINTFN(10,("%s: cue_mem cmd=0x%x addr=0x%x len=%d\n",
		     USBDEVNAME(sc->cue_dev), cmd, addr, len));

	if (cmd == CUE_CMD_READSRAM)
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
	else
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = cmd;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, len);

	err = usbd_do_request(sc->cue_udev, &req, buf);

	if (err) {
		DPRINTF(("%s: cue_csr_mem: addr=0x%x err=%s\n",
			 USBDEVNAME(sc->cue_dev), addr, usbd_errstr(err)));
		return (-1);
	}

	return (0);
}

Static int
cue_getmac(struct cue_softc *sc, void *buf)
{
	usb_device_request_t	req;
	usbd_status		err;

	DPRINTFN(10,("%s: cue_getmac\n", USBDEVNAME(sc->cue_dev)));

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_GET_MACADDR;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, ETHER_ADDR_LEN);

	err = usbd_do_request(sc->cue_udev, &req, buf);

	if (err) {
		printf("%s: read MAC address failed\n",USBDEVNAME(sc->cue_dev));
		return (-1);
	}

	return (0);
}

#define CUE_POLY	0xEDB88320
#define CUE_BITS	9

Static u_int32_t
cue_crc(const char *addr)
{
	u_int32_t		idx, bit, data, crc;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1)
			crc = (crc >> 1) ^ (((crc ^ data) & 1) ? CUE_POLY : 0);
	}

	return (crc & ((1 << CUE_BITS) - 1));
}

Static void
cue_setmulti(struct cue_softc *sc)
{
	struct ifnet		*ifp;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	u_int32_t		h, i;

	ifp = GET_IFP(sc);

	DPRINTFN(2,("%s: cue_setmulti if_flags=0x%x\n",
		    USBDEVNAME(sc->cue_dev), ifp->if_flags));

	if (ifp->if_flags & IFF_PROMISC) {
allmulti:
		ifp->if_flags |= IFF_ALLMULTI;
		for (i = 0; i < CUE_MCAST_TABLE_LEN; i++)
			sc->cue_mctab[i] = 0xFF;
		cue_mem(sc, CUE_CMD_WRITESRAM, CUE_MCAST_TABLE_ADDR,
		    &sc->cue_mctab, CUE_MCAST_TABLE_LEN);
		return;
	}

	/* first, zot all the existing hash bits */
	for (i = 0; i < CUE_MCAST_TABLE_LEN; i++)
		sc->cue_mctab[i] = 0;

	/* now program new ones */
#if defined(__NetBSD__)
	ETHER_FIRST_MULTI(step, &sc->cue_ec, enm);
#else
	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
#endif
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo,
		    enm->enm_addrhi, ETHER_ADDR_LEN) != 0)
			goto allmulti;

		h = cue_crc(enm->enm_addrlo);
		sc->cue_mctab[h >> 3] |= 1 << (h & 0x7);
		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;

	/*
	 * Also include the broadcast address in the filter
	 * so we can receive broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		h = cue_crc(etherbroadcastaddr);
		sc->cue_mctab[h >> 3] |= 1 << (h & 0x7);
	}

	cue_mem(sc, CUE_CMD_WRITESRAM, CUE_MCAST_TABLE_ADDR,
	    &sc->cue_mctab, CUE_MCAST_TABLE_LEN);
}

Static void
cue_reset(struct cue_softc *sc)
{
	usb_device_request_t	req;
	usbd_status		err;

	DPRINTFN(2,("%s: cue_reset\n", USBDEVNAME(sc->cue_dev)));

	if (sc->cue_dying)
		return;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_RESET;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->cue_udev, &req, NULL);

	if (err)
		printf("%s: reset failed\n", USBDEVNAME(sc->cue_dev));

	/* Wait a little while for the chip to get its brains in order. */
	usbd_delay_ms(sc->cue_udev, 1);
}

/*
 * Probe for a CATC chip.
 */
USB_MATCH(cue)
{
	USB_MATCH_START(cue, uaa);

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	return (cue_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
USB_ATTACH(cue)
{
	USB_ATTACH_START(cue, sc, uaa);
	char			*devinfop;
	int			s;
	u_char			eaddr[ETHER_ADDR_LEN];
	usbd_device_handle	dev = uaa->device;
	usbd_interface_handle	iface;
	usbd_status		err;
	struct ifnet		*ifp;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int			i;

	DPRINTFN(5,(" : cue_attach: sc=%p, dev=%p", sc, dev));

	devinfop = usbd_devinfo_alloc(dev, 0);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->cue_dev), devinfop);
	usbd_devinfo_free(devinfop);

	err = usbd_set_config_no(dev, CUE_CONFIG_NO, 1);
	if (err) {
		printf("%s: setting config no failed\n",
		    USBDEVNAME(sc->cue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->cue_udev = dev;
	sc->cue_product = uaa->product;
	sc->cue_vendor = uaa->vendor;

	usb_init_task(&sc->cue_tick_task, cue_tick_task, sc);
	usb_init_task(&sc->cue_stop_task, (void (*)(void *))cue_stop, sc);

	err = usbd_device2interface_handle(dev, CUE_IFACE_IDX, &iface);
	if (err) {
		printf("%s: getting interface handle failed\n",
		    USBDEVNAME(sc->cue_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->cue_iface = iface;
	id = usbd_get_interface_descriptor(iface);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: couldn't get ep %d\n",
			    USBDEVNAME(sc->cue_dev), i);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->cue_ed[CUE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->cue_ed[CUE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->cue_ed[CUE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

#if 0
	/* Reset the adapter. */
	cue_reset(sc);
#endif
	/*
	 * Get station address.
	 */
	cue_getmac(sc, &eaddr);

	s = splnet();

	/*
	 * A CATC chip was detected. Inform the world.
	 */
	printf("%s: Ethernet address %s\n", USBDEVNAME(sc->cue_dev),
	    ether_sprintf(eaddr));

	/* Initialize interface info.*/
	ifp = GET_IFP(sc);
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = cue_ioctl;
	ifp->if_start = cue_start;
	ifp->if_init = cue_init;
	ifp->if_stop = cue_stop;
	ifp->if_watchdog = cue_watchdog;
#if defined(__OpenBSD__)
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
#endif
	strncpy(ifp->if_xname, USBDEVNAME(sc->cue_dev), IFNAMSIZ);

	IFQ_SET_READY(&ifp->if_snd);

	/* Attach the interface. */
	if_attach(ifp);
	Ether_ifattach(ifp, eaddr);
#if NRND > 0
	rnd_attach_source(&sc->rnd_source, USBDEVNAME(sc->cue_dev),
	    RND_TYPE_NET, 0);
#endif

	usb_callout_init(sc->cue_stat_ch);

	sc->cue_attached = 1;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->cue_udev,
	    USBDEV(sc->cue_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(cue)
{
	USB_DETACH_START(cue, sc);
	struct ifnet		*ifp = GET_IFP(sc);
	int			s;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev), __func__));

	usb_uncallout(sc->cue_stat_ch, cue_tick, sc);
	/*
	 * Remove any pending task.  It cannot be executing because it run
	 * in the same thread as detach.
	 */
	usb_rem_task(sc->cue_udev, &sc->cue_tick_task);
	usb_rem_task(sc->cue_udev, &sc->cue_stop_task);

	if (!sc->cue_attached) {
		/* Detached before attached finished, so just bail out. */
		return (0);
	}

	s = splusb();

	if (ifp->if_flags & IFF_RUNNING)
		cue_stop(ifp, 1);

#if defined(__NetBSD__)
#if NRND > 0
	rnd_detach_source(&sc->rnd_source);
#endif
	ether_ifdetach(ifp);
#endif /* __NetBSD__ */

	if_detach(ifp);

#ifdef DIAGNOSTIC
	if (sc->cue_ep[CUE_ENDPT_TX] != NULL ||
	    sc->cue_ep[CUE_ENDPT_RX] != NULL ||
	    sc->cue_ep[CUE_ENDPT_INTR] != NULL)
		printf("%s: detach has active endpoints\n",
		       USBDEVNAME(sc->cue_dev));
#endif

	sc->cue_attached = 0;
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->cue_udev,
	    USBDEV(sc->cue_dev));

	return (0);
}

int
cue_activate(device_ptr_t self, enum devact act)
{
	struct cue_softc *sc = (struct cue_softc *)self;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev), __func__));

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		/* Deactivate the interface. */
		if_deactivate(&sc->cue_ec.ec_if);
		sc->cue_dying = 1;
		break;
	}
	return (0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
Static void
cue_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ue_chain		*c = priv;
	struct cue_softc	*sc = (void *)c->ue_dev;
	struct ifnet		*ifp = GET_IFP(sc);
	struct mbuf		*m;
	int			total_len = 0;
	u_int16_t		len;
	int			s;

	DPRINTFN(10,("%s: %s: enter status=%d\n", USBDEVNAME(sc->cue_dev),
		     __func__, status));

	if (sc->cue_dying)
		return;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	usbd_unmap_buffer(xfer);

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		sc->cue_rx_errs++;
		if (usbd_ratecheck(&sc->cue_rx_notice)) {
			printf("%s: %u usb errors on rx: %s\n",
			    USBDEVNAME(sc->cue_dev), sc->cue_rx_errs,
			    usbd_errstr(status));
			sc->cue_rx_errs = 0;
		}
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->cue_ep[CUE_ENDPT_RX]);
		goto done;
	}

#if 0
	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);
	/* XXX should check total_len ? */
#endif

	m = c->ue_mbuf;
	len = UGETW(mtod(m, u_int8_t *));

	/* No errors; receive the packet. */
	total_len = len;

	if (len < sizeof(struct ether_header)) {
		ifp->if_ierrors++;
		goto done;
	}

	/*
	 * Allocate new mbuf cluster for the next transfer.
	 * If that failed, discard current packet and recycle the mbuf.
	 */
	if ((c->ue_mbuf = usb_ether_newbuf(NULL)) == NULL) {
		printf("%s: no memory for rx list -- packet dropped!\n",
		    USBDEVNAME(sc->cue_dev));
		ifp->if_ierrors++;
		c->ue_mbuf = usb_ether_newbuf(m);
		goto done;
	}

	ifp->if_ipackets++;
	m_adj(m, sizeof(u_int16_t));
	m->m_pkthdr.len = m->m_len = total_len;

	m->m_pkthdr.rcvif = ifp;

	s = splnet();

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

	DPRINTFN(10,("%s: %s: deliver %d\n", USBDEVNAME(sc->cue_dev),
		    __func__, m->m_len));
	IF_INPUT(ifp, m);
	splx(s);

done:
	/* Setup new transfer. */
	(void)usbd_map_buffer_mbuf(c->ue_xfer, c->ue_mbuf);
	usbd_setup_xfer(c->ue_xfer, sc->cue_ep[CUE_ENDPT_RX],
	    c, NULL /* XXX buf */, CUE_BUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, cue_rxeof);
	usbd_transfer(c->ue_xfer);

	DPRINTFN(10,("%s: %s: start rx\n", USBDEVNAME(sc->cue_dev),
		    __func__));
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
Static void
cue_txeof(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct ue_chain		*c = priv;
	struct cue_softc	*sc = (void *)c->ue_dev;
	struct ifnet		*ifp = GET_IFP(sc);
	int			s;

	if (sc->cue_dying)
		return;

	usbd_unmap_buffer(xfer);

	s = splnet();

	DPRINTFN(10,("%s: %s: enter status=%d\n", USBDEVNAME(sc->cue_dev),
		    __func__, status));

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}
		ifp->if_oerrors++;
		printf("%s: usb error on tx: %s\n", USBDEVNAME(sc->cue_dev),
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->cue_ep[CUE_ENDPT_TX]);
		splx(s);
		return;
	}

	ifp->if_opackets++;

	m_freem(c->ue_mbuf);
	c->ue_mbuf = NULL;

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		cue_start(ifp);

	splx(s);
}

Static void
cue_tick(void *xsc)
{
	struct cue_softc	*sc = xsc;

	if (sc == NULL)
		return;

	if (sc->cue_dying)
		return;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev), __func__));

	/* Perform statistics update in process context. */
	usb_add_task(sc->cue_udev, &sc->cue_tick_task, USB_TASKQ_DRIVER);
}

Static void
cue_tick_task(void *xsc)
{
	struct cue_softc	*sc = xsc;
	struct ifnet		*ifp;

	if (sc->cue_dying)
		return;

	DPRINTFN(2,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev), __func__));

	ifp = GET_IFP(sc);

	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_SINGLECOLL);
	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_MULTICOLL);
	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_EXCESSCOLL);

	if (cue_csr_read_2(sc, CUE_RX_FRAMEERR))
		ifp->if_ierrors++;
}

Static int
cue_send(struct cue_softc *sc, struct mbuf *m, int idx)
{
	int			total_len;
	struct ue_chain		*c;
	usbd_status		err;
	int			ret;

	c = &sc->cue_cdata.cue_tx_chain[idx];

	/* Prepend two bytes at the beginning to hold the frame length. */
	M_PREPEND(m, sizeof(u_int16_t), M_DONTWAIT);
	if (m != NULL)
		m = m_pullup(m, sizeof(u_int16_t));	/* just in case */
	if (m == NULL) {
		GET_IFP(sc)->if_oerrors++;
		return (ENOBUFS);
	}

	total_len = m->m_pkthdr.len;

	DPRINTFN(10,("%s: %s: total_len=%d\n",
		     USBDEVNAME(sc->cue_dev), __func__, total_len));

	/* The first two bytes are the frame length */
	USETW(mtod(m, char *), m->m_pkthdr.len - sizeof(u_int16_t));

	ret = usb_ether_map_tx_buffer_mbuf(c, m);
	if (ret) {
		m_freem(m);
		return (ret);
	}

	/* XXX 10000 */
	usbd_setup_xfer(c->ue_xfer, sc->cue_ep[CUE_ENDPT_TX],
	    c, NULL /* XXX buf */, total_len, USBD_NO_COPY, 10000, cue_txeof);

	/* Transmit */
	err = usbd_transfer(c->ue_xfer);
	if (err != USBD_IN_PROGRESS) {
		c->ue_mbuf = NULL;
		m_freem(m);
		printf("%s: cue_send error=%s\n", USBDEVNAME(sc->cue_dev),
		       usbd_errstr(err));
		/* Stop the interface from process context. */
		usb_add_task(sc->cue_udev, &sc->cue_stop_task,
		    USB_TASKQ_DRIVER);
		return (EIO);
	}

	sc->cue_cdata.cue_tx_cnt++;

	return (0);
}

Static void
cue_start(struct ifnet *ifp)
{
	struct cue_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;

	if (sc->cue_dying)
		return;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev),__func__));

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	IFQ_POLL(&ifp->if_snd, m_head);
	if (m_head == NULL)
		return;

	IFQ_DEQUEUE(&ifp->if_snd, m_head);

#if NBPFILTER > 0
	/*
	 * If there's a BPF listener, bounce a copy of this frame
	 * to him.
	 */
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m_head);
#endif

	if (cue_send(sc, m_head, 0)) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

Static int
cue_init(struct ifnet *ifp)
{
	struct cue_softc	*sc = ifp->if_softc;
	int			i, s, ctl;
	u_char			*eaddr;
	struct ue_chain		*c;

	if (sc->cue_dying)
		return (EIO);

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev),__func__));

	if (ifp->if_flags & IFF_RUNNING)
		return (EIO);

	s = splnet();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
#if 1
	cue_reset(sc);
#endif

	/* Set advanced operation modes. */
	cue_csr_write_1(sc, CUE_ADVANCED_OPMODES,
	    CUE_AOP_EMBED_RXLEN | 0x03); /* 1 wait state */

#if defined(__OpenBSD__)
	eaddr = sc->arpcom.ac_enaddr;
#elif defined(__NetBSD__)
	eaddr = LLADDR(ifp->if_sadl);
#endif
	/* Set MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		cue_csr_write_1(sc, CUE_PAR0 - i, eaddr[i]);

	/* Enable RX logic. */
	ctl = CUE_ETHCTL_RX_ON | CUE_ETHCTL_MCAST_ON;
	if (ifp->if_flags & IFF_PROMISC)
		ctl |= CUE_ETHCTL_PROMISC;
	cue_csr_write_1(sc, CUE_ETHCTL, ctl);

	/* Load the multicast filter. */
	cue_setmulti(sc);

	/*
	 * Set the number of RX and TX buffers that we want
	 * to reserve inside the ASIC.
	 */
	cue_csr_write_1(sc, CUE_RX_BUFPKTS, CUE_RX_FRAMES);
	cue_csr_write_1(sc, CUE_TX_BUFPKTS, CUE_TX_FRAMES);

	/* Set advanced operation modes. */
	cue_csr_write_1(sc, CUE_ADVANCED_OPMODES,
	    CUE_AOP_EMBED_RXLEN | 0x01); /* 1 wait state */

	/* Program the LED operation. */
	cue_csr_write_1(sc, CUE_LEDCTL, CUE_LEDCTL_FOLLOW_LINK);

	if (sc->cue_ep[CUE_ENDPT_RX] == NULL) {
		if (cue_open_pipes(sc)) {
			splx(s);
			return (EIO);
		}
	}

	/* Init TX ring. */
	if ((i = usb_ether_tx_list_init(USBDEV(sc->cue_dev),
	    sc->cue_cdata.cue_tx_chain, CUE_TX_LIST_CNT,
	    sc->cue_udev, sc->cue_ep[CUE_ENDPT_TX], NULL))) {
		printf("%s: tx list init failed\n", USBDEVNAME(sc->cue_dev));
		splx(s);
		return (i);
	}

	/* Init RX ring. */
	if ((i = usb_ether_rx_list_init(USBDEV(sc->cue_dev),
	    sc->cue_cdata.cue_rx_chain, CUE_RX_LIST_CNT,
	    sc->cue_udev, sc->cue_ep[CUE_ENDPT_RX]))) {
		printf("%s: rx list init failed\n", USBDEVNAME(sc->cue_dev));
		splx(s);
		return (i);
	}

	/* Start up the receive pipe. */
	for (i = 0; i < CUE_RX_LIST_CNT; i++) {
		c = &sc->cue_cdata.cue_rx_chain[i];
		(void)usbd_map_buffer_mbuf(c->ue_xfer, c->ue_mbuf);
		usbd_setup_xfer(c->ue_xfer, sc->cue_ep[CUE_ENDPT_RX],
		    c, NULL /* XXX buf */, CUE_BUFSZ,
		    USBD_SHORT_XFER_OK | USBD_NO_COPY, USBD_NO_TIMEOUT,
		    cue_rxeof);
		usbd_transfer(c->ue_xfer);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	usb_callout(sc->cue_stat_ch, hz, cue_tick, sc);

	return (0);
}

Static int
cue_open_pipes(struct cue_softc	*sc)
{
	usbd_status		err;

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->cue_iface, sc->cue_ed[CUE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->cue_ep[CUE_ENDPT_RX]);
	if (err) {
		printf("%s: open rx pipe failed: %s\n",
		    USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		return (EIO);
	}
	err = usbd_open_pipe(sc->cue_iface, sc->cue_ed[CUE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->cue_ep[CUE_ENDPT_TX]);
	if (err) {
		printf("%s: open tx pipe failed: %s\n",
		    USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		return (EIO);
	}

	return (0);
}

Static int
cue_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct cue_softc	*sc = ifp->if_softc;
#if 0
	struct ifaddr 		*ifa = (struct ifaddr *)data;
	struct ifreq		*ifr = (struct ifreq *)data;
#endif
	int			s, error = 0;

	if (sc->cue_dying)
		return (EIO);

	s = splnet();

	switch(command) {
#if 0
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		cue_init(ifp);

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
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ETHERMTU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
#endif

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->cue_if_flags & IFF_PROMISC)) {
				CUE_SETBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);
				cue_setmulti(sc);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->cue_if_flags & IFF_PROMISC) {
				CUE_CLRBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);
				cue_setmulti(sc);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				cue_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				cue_stop(ifp, 0);
		}
		sc->cue_if_flags = ifp->if_flags;
		error = 0;
		break;
#if 0
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		cue_setmulti(sc);
		error = 0;
		break;
#endif
	default:
		error = ether_ioctl(ifp, command, data);
		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				cue_setmulti(sc);
			error = 0;
		}
		break;
	}

	splx(s);

	return (error);
}

Static void
cue_watchdog(struct ifnet *ifp)
{
	struct cue_softc	*sc = ifp->if_softc;
	struct ue_chain		*c;
	usbd_status		stat;
	int			s;

	DPRINTFN(5,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev),__func__));

	if (sc->cue_dying)
		return;

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", USBDEVNAME(sc->cue_dev));

	s = splusb();
	c = &sc->cue_cdata.cue_tx_chain[0];
	usbd_get_xfer_status(c->ue_xfer, NULL, NULL, NULL, &stat);
	cue_txeof(c->ue_xfer, c, stat);

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		cue_start(ifp);
	splx(s);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
Static void
cue_stop(struct ifnet *ifp, int disable)
{
	struct cue_softc	*sc = ifp->if_softc;
	usbd_status		err;

	DPRINTFN(10,("%s: %s: enter\n", USBDEVNAME(sc->cue_dev),__func__));

	ifp = GET_IFP(sc);
	ifp->if_timer = 0;

	cue_csr_write_1(sc, CUE_ETHCTL, 0);
	cue_reset(sc);
	usb_uncallout(sc->cue_stat_ch, cue_tick, sc);

	/* Stop transfers. */
	if (sc->cue_ep[CUE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_RX]);
		if (err) {
			printf("%s: abort rx pipe failed: %s\n",
			USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
	}

	if (sc->cue_ep[CUE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_TX]);
		if (err) {
			printf("%s: abort tx pipe failed: %s\n",
			USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
	}

	if (sc->cue_ep[CUE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_INTR]);
		if (err) {
			printf("%s: abort intr pipe failed: %s\n",
			USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
	}

	/* Free RX/TX resources. */
	usb_ether_rx_list_free(sc->cue_cdata.cue_rx_chain, CUE_RX_LIST_CNT);
	usb_ether_tx_list_free(sc->cue_cdata.cue_tx_chain, CUE_TX_LIST_CNT);

	/* Close pipes. */
	if (sc->cue_ep[CUE_ENDPT_RX] != NULL) {
		err = usbd_close_pipe(sc->cue_ep[CUE_ENDPT_RX]);
		if (err) {
			printf("%s: close rx pipe failed: %s\n",
			USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
		sc->cue_ep[CUE_ENDPT_RX] = NULL;
	}
	if (sc->cue_ep[CUE_ENDPT_TX] != NULL) {
		err = usbd_close_pipe(sc->cue_ep[CUE_ENDPT_TX]);
		if (err) {
			printf("%s: close tx pipe failed: %s\n",
			    USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
		sc->cue_ep[CUE_ENDPT_TX] = NULL;
	}
	if (sc->cue_ep[CUE_ENDPT_INTR] != NULL) {
		err = usbd_close_pipe(sc->cue_ep[CUE_ENDPT_INTR]);
		if (err) {
			printf("%s: close intr pipe failed: %s\n",
			    USBDEVNAME(sc->cue_dev), usbd_errstr(err));
		}
		sc->cue_ep[CUE_ENDPT_INTR] = NULL;
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}
