/*	$NetBSD: if_axereg.h,v 1.3.58.1 2007/12/08 17:57:33 ad Exp $	*/

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
 *
 * $FreeBSD: src/sys/dev/usb/if_axereg.h,v 1.2 2003/06/15 21:45:43 wpaul Exp $
 */

/*
 * Definitions for the ASIX Electronics AX88172 to ethernet controller.
 */


/*
 * Vendor specific commands
 * ASIX conveniently doesn't document the 'set NODEID' command in their
 * datasheet (thanks a lot guys).
 * To make handling these commands easier, I added some extra data
 * which is decided by the axe_cmd() routine. Commands are encoded
 * in 16 bites, with the format: LDCC. L and D are both nibbles in
 * the high byte. L represents the data length (0 to 15) and D
 * represents the direction (0 for vendor read, 1 for vendor write).
 * CC is the command byte, as specified in the manual.
 */

#define AXE_CMD_DIR(x)	(((x) & 0x0F00) >> 8)
#define AXE_CMD_LEN(x)	(((x) & 0xF000) >> 12)
#define AXE_CMD_CMD(x)	((x) & 0x00FF)

#define AXE_CMD_READ_RXTX_SRAM			0x2002
#define AXE_CMD_WRITE_RX_SRAM			0x0103
#define AXE_CMD_WRITE_TX_SRAM			0x0104
#define AXE_CMD_MII_OPMODE_SW			0x0106
#define AXE_CMD_MII_READ_REG			0x2007
#define AXE_CMD_MII_WRITE_REG			0x2108
#define AXE_CMD_MII_READ_OPMODE			0x1009
#define AXE_CMD_MII_OPMODE_HW			0x010A
#define AXE_CMD_SROM_READ			0x200B
#define AXE_CMD_SROM_WRITE			0x010C
#define AXE_CMD_SROM_WR_ENABLE			0x010D
#define AXE_CMD_SROM_WR_DISABLE			0x010E
#define AXE_CMD_RXCTL_READ			0x200F
#define AXE_CMD_RXCTL_WRITE			0x0110
#define AXE_CMD_READ_IPG012			0x3011
#define AXE_CMD_WRITE_IPG0			0x0112
#define AXE_CMD_WRITE_IPG1			0x0113
#define AXE_CMD_WRITE_IPG2			0x0114
#define AXE_CMD_READ_MCAST			0x8015
#define AXE_CMD_WRITE_MCAST			0x8116
#define AXE_CMD_READ_NODEID			0x6017
#define AXE_CMD_WRITE_NODEID			0x6118
#define AXE_CMD_READ_PHYID			0x2019
#define AXE_CMD_READ_MEDIA			0x101A
#define AXE_CMD_WRITE_MEDIA			0x011B
#define AXE_CMD_READ_MONITOR_MODE		0x101C
#define AXE_CMD_WRITE_MONITOR_MODE		0x011D
#define AXE_CMD_READ_GPIO			0x101E
#define AXE_CMD_WRITE_GPIO			0x011F

#define AXE_MEDIA_FULL_DUPLEX			0x02
#define AXE_MEDIA_TX_ABORT_ALLOW		0x04
#define AXE_MEDIA_FLOW_CONTROL_EN		0x10

#define AXE_RXCMD_PROMISC			0x0001
#define AXE_RXCMD_ALLMULTI			0x0002
#define AXE_RXCMD_UNICAST			0x0004
#define AXE_RXCMD_BROADCAST			0x0008
#define AXE_RXCMD_MULTICAST			0x0010
#define AXE_RXCMD_ENABLE			0x0080

#define AXE_NOPHY				0xE0

#define AXE_TIMEOUT		1000
#define AXE_BUFSZ		1536
#define AXE_MIN_FRAMELEN	60
#define AXE_RX_FRAMES		1
#define AXE_TX_FRAMES		1

#define AXE_RX_LIST_CNT		1
#define AXE_TX_LIST_CNT		1

#define AXE_CTL_READ		0x01
#define AXE_CTL_WRITE		0x02

#define AXE_CONFIG_NO		1
#define AXE_IFACE_IDX		0

/*
 * The interrupt endpoint is currently unused
 * by the ASIX part.
 */
#define AXE_ENDPT_RX		0x0
#define AXE_ENDPT_TX		0x1
#define AXE_ENDPT_INTR		0x2
#define AXE_ENDPT_MAX		0x3

struct axe_type {
	struct usb_devno	axe_dev;
	u_int16_t		axe_flags;
/* XXX No flags so far */
};

struct axe_softc;

struct axe_chain {
	struct axe_softc	*axe_sc;
	usbd_xfer_handle	axe_xfer;
	char			*axe_buf;
	struct mbuf		*axe_mbuf;
	int			axe_accum;
	int			axe_idx;
};

struct axe_cdata {
	struct axe_chain	axe_tx_chain[AXE_TX_LIST_CNT];
	struct axe_chain	axe_rx_chain[AXE_RX_LIST_CNT];
	int			axe_tx_prod;
	int			axe_tx_cons;
	int			axe_tx_cnt;
	int			axe_rx_prod;
};

#define AXE_INC(x, y)		(x) = (x + 1) % y

struct axe_softc {
	USBBASEDEVICE		axe_dev;
#if defined(__FreeBSD__)
	struct arpcom		arpcom;
	device_t		axe_miibus;
#define GET_IFP(sc) (&(sc)->arpcom.ac_if)
#define GET_MII(sc) (device_get_softc((sc)->axe_miibus))
#elif defined(__NetBSD__)
	struct ethercom		axe_ec;
	struct mii_data		axe_mii;
#if NRND > 0
	rndsource_element_t	rnd_source;
#endif
#define GET_IFP(sc) (&(sc)->axe_ec.ec_if)
#define GET_MII(sc) (&(sc)->axe_mii)
#elif defined(__OpenBSD__)
	struct arpcom		arpcom;
	struct mii_data		axe_mii;
#if NRND > 0
	rndsource_element_t	rnd_source;
#endif
#define GET_IFP(sc) (&(sc)->arpcom.ac_if)
#define GET_MII(sc) (&(sc)->axe_mii)
#endif
	usbd_device_handle	axe_udev;
	usbd_interface_handle	axe_iface;

	u_int16_t		axe_vendor;
	u_int16_t		axe_product;

	int			axe_ed[AXE_ENDPT_MAX];
	usbd_pipe_handle	axe_ep[AXE_ENDPT_MAX];
	int			axe_if_flags;
	struct axe_cdata	axe_cdata;
	usb_callout_t		axe_stat_ch;

	int			axe_refcnt;
	char			axe_dying;
	char			axe_attached;

	struct usb_task		axe_tick_task;
	struct usb_task		axe_stop_task;

	kmutex_t		axe_mii_lock;

	int			axe_link;
	unsigned char		axe_ipgs[3];
	unsigned char 		axe_phyaddrs[2];
	struct timeval		axe_rx_notice;
};

#if 0
#define	AXE_LOCK(_sc)		mtx_lock(&(_sc)->axe_mtx)
#define	AXE_UNLOCK(_sc)		mtx_unlock(&(_sc)->axe_mtx)
#else
#define	AXE_LOCK(_sc)
#define	AXE_UNLOCK(_sc)
#endif

#define ETHER_ALIGN		2
