/*	$NetBSD: wireg.h,v 1.7.2.9 2002/01/11 23:39:02 nathanw Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 * FreeBSD driver ported to NetBSD by Bill Sommerfeld in the back of the
 * Oslo IETF plenary meeting.
 */

#define WI_TIMEOUT	65536

#define WI_PORT0	0
#define WI_PORT1	1
#define WI_PORT2	2
#define WI_PORT3	3
#define WI_PORT4	4
#define WI_PORT5	5

/* Default port: 0 (only 0 exists on stations) */
#define WI_DEFAULT_PORT	(WI_PORT0 << 8)

/* Default TX rate: 2Mbps, auto fallback */
#define WI_DEFAULT_TX_RATE	3

/* Default network name: ANY */
/*
 * [sommerfeld 1999/07/15] Changed from "ANY" to ""; according to Bill Fenner,
 * ANY is used in MS driver user interfaces, while "" is used over the
 * wire..
 */
#define WI_DEFAULT_NETNAME	""

#define WI_DEFAULT_AP_DENSITY	1

#define WI_DEFAULT_RTS_THRESH	2347

#define WI_DEFAULT_DATALEN	2304

#define WI_DEFAULT_CREATE_IBSS	0

#define WI_DEFAULT_PM_ENABLED	0

#define WI_DEFAULT_MAX_SLEEP	100

#define WI_DEFAULT_ROAMING	1

#define WI_DEFAULT_AUTHTYPE	1

#ifdef __NetBSD__
#define OS_STRING_NAME	"NetBSD"
#endif
#ifdef __FreeBSD__
#define OS_STRING_NAME	"FreeBSD"
#endif
#ifdef __OpenBSD__
#define OS_STRING_NAME	"OpenBSD"
#endif

#define WI_DEFAULT_NODENAME	OS_STRING_NAME " WaveLAN/IEEE node"

#define WI_DEFAULT_IBSS		OS_STRING_NAME " IBSS"

#define WI_DEFAULT_CHAN		3

/*
 * register space access macros
 */
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg) , val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg))
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg))
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg))

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define bus_space_write_stream_2	bus_space_write_2
#define bus_space_write_multi_stream_2	bus_space_write_multi_2
#define bus_space_read_stream_2		bus_space_read_2
#define bus_space_read_multi_stream_2		bus_space_read_multi_2
#endif

#define CSR_WRITE_STREAM_2(sc, reg, val)	\
	bus_space_write_stream_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), val)
#define CSR_WRITE_MULTI_STREAM_2(sc, reg, val, count)	\
	bus_space_write_multi_stream_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), val, count)
#define CSR_READ_STREAM_2(sc, reg)		\
	bus_space_read_stream_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg))
#define CSR_READ_MULTI_STREAM_2(sc, reg, buf, count)		\
	bus_space_read_multi_stream_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), buf, count)

/*
 * The WaveLAN/IEEE cards contain an 802.11 MAC controller which Lucent
 * calls 'Hermes.' In typical fashion, getting documentation about this
 * controller is about as easy as squeezing blood from a stone. Here
 * is more or less what I know:
 *
 * - The Hermes controller is firmware driven, and the host interacts
 *   with the Hermes via a firmware interface, which can change.
 *
 * - The Hermes is described in a document called: "Hermes Firmware
 *   WaveLAN/IEEE Station Functions," document #010245, which of course
 *   Lucent will not release without an NDA.
 *
 * - Lucent has created a library called HCF (Hardware Control Functions)
 *   though which it wants developers to interact with the card. The HCF
 *   is needlessly complex, ill conceived and badly documented. Actually,
 *   the comments in the HCP code itself aren't bad, but the publically
 *   available manual that comes with it is awful, probably due largely to
 *   the fact that it has been emasculated in order to hide information
 *   that Lucent wants to keep proprietary. The purpose of the HCF seems
 *   to be to insulate the driver programmer from the Hermes itself so that
 *   Lucent has an excuse not to release programming in for it.
 *
 * - Lucent only makes available documentation and code for 'HCF Light'
 *   which is a stripped down version of HCF with certain features not
 *   implemented, most notably support for 802.11 frames.
 *
 * - The HCF code which I have seen blows goats. Whoever decided to
 *   use a 132 column format should be shot.
 *
 * Rather than actually use the Lucent HCF library, I have stripped all
 * the useful information from it and used it to create a driver in the
 * usual BSD form. Note: I don't want to hear anybody whining about the
 * fact that the Lucent code is GPLed and mine isn't. I did not actually
 * put any of Lucent's code in this driver: I only used it as a reference
 * to obtain information about the underlying hardware. The Hermes
 * programming interface is not GPLed, so bite me.
 */

/*
 * Size of Hermes & Prism2 I/O space.
 */
#define WI_IOSIZE		0x40
#define WI_PCI_CBMA		0x10	/* Configuration Base Memory Address */

/*
 * Hermes & Prism2 register definitions 
 */

/* Hermes command/status registers. */
#define WI_COMMAND		0x00
#define WI_PARAM0		0x02
#define WI_PARAM1		0x04
#define WI_PARAM2		0x06
#define WI_STATUS		0x08
#define WI_RESP0		0x0A
#define WI_RESP1		0x0C
#define WI_RESP2		0x0E

/* Command register values. */
#define WI_CMD_BUSY		0x8000 /* busy bit */
#define WI_CMD_INI		0x0000 /* initialize */
#define WI_CMD_ENABLE		0x0001 /* enable */
#define WI_CMD_DISABLE		0x0002 /* disable */
#define WI_CMD_DIAG		0x0003
#define WI_CMD_ALLOC_MEM	0x000A /* allocate NIC memory */
#define WI_CMD_TX		0x000B /* transmit */
#define WI_CMD_NOTIFY		0x0010
#define WI_CMD_INQUIRE		0x0011
#define WI_CMD_ACCESS		0x0021
#define WI_CMD_PROGRAM		0x0022

#define WI_CMD_CODE_MASK	0x003F

/*
 * Reclaim qualifier bit, applicable to the
 * TX and INQUIRE commands.
 */
#define WI_RECLAIM		0x0100 /* reclaim NIC memory */

/*
 * ACCESS command qualifier bits.
 */
#define WI_ACCESS_READ		0x0000
#define WI_ACCESS_WRITE		0x0100

/*
 * PROGRAM command qualifier bits.
 */
#define WI_PROGRAM_DISABLE	0x0000
#define WI_PROGRAM_ENABLE_RAM	0x0100
#define WI_PROGRAM_ENABLE_NVRAM	0x0200
#define WI_PROGRAM_NVRAM	0x0300

/* Status register values */
#define WI_STAT_CMD_CODE	0x003F
#define WI_STAT_DIAG_ERR	0x0100
#define WI_STAT_INQ_ERR		0x0500
#define WI_STAT_CMD_RESULT	0x7F00

/* memory handle management registers */
#define WI_INFO_FID		0x10
#define WI_RX_FID		0x20
#define WI_ALLOC_FID		0x22
#define WI_TX_CMP_FID		0x24

/*
 * Buffer Access Path (BAP) registers.
 * These are I/O channels. I believe you can use each one for
 * any desired purpose independently of the other. In general
 * though, we use BAP1 for reading and writing LTV records and
 * reading received data frames, and BAP0 for writing transmit
 * frames. This is a convention though, not a rule.
 */
#define WI_SEL0			0x18
#define WI_SEL1			0x1A
#define WI_OFF0			0x1C
#define WI_OFF1			0x1E
#define WI_DATA0		0x36
#define WI_DATA1		0x38
#define WI_BAP0			WI_DATA0
#define WI_BAP1			WI_DATA1

#define WI_OFF_BUSY		0x8000
#define WI_OFF_ERR		0x4000
#define WI_OFF_DATAOFF		0x0FFF

/* Event registers */
#define WI_EVENT_STAT		0x30	/* Event status */
#define WI_INT_EN		0x32	/* Interrupt enable/disable */
#define WI_EVENT_ACK		0x34	/* Ack event */

/* Events */
#define WI_EV_TICK		0x8000	/* aux timer tick */
#define WI_EV_RES		0x4000	/* controller h/w error (time out) */
#define WI_EV_INFO_DROP		0x2000	/* no RAM to build unsolicited frame */
#define WI_EV_NO_CARD		0x0800	/* card removed (hunh?) */
#define WI_EV_DUIF_RX		0x0400	/* wavelan management packet received */
#define WI_EV_INFO		0x0080	/* async info frame */
#define WI_EV_CMD		0x0010	/* command completed */
#define WI_EV_ALLOC		0x0008	/* async alloc/reclaim completed */
#define WI_EV_TX_EXC		0x0004	/* async xmit completed with failure */
#define WI_EV_TX		0x0002	/* async xmit completed succesfully */
#define WI_EV_RX		0x0001	/* async rx completed */

#define WI_INTRS	\
	(WI_EV_RX|WI_EV_TX|WI_EV_TX_EXC|WI_EV_ALLOC|WI_EV_INFO|WI_EV_INFO_DROP)

/* Host software registers */
#define WI_SW0			0x28
#define WI_SW1			0x2A
#define WI_SW2			0x2C
#define WI_SW3			0x2E 	/* does not appear in Prism2 */

#define WI_CNTL			0x14

#define WI_CNTL_AUX_ENA		0xC000
#define WI_CNTL_AUX_ENA_STAT	0xC000
#define WI_CNTL_AUX_DIS_STAT	0x0000
#define WI_CNTL_AUX_ENA_CNTL	0x8000
#define WI_CNTL_AUX_DIS_CNTL	0x4000

#define WI_AUX_PAGE		0x3A
#define WI_AUX_OFFSET		0x3C
#define WI_AUX_DATA		0x3E

/*
 * PCI Host Interface Registers (HFA3842 Specific)
 * The value of all Register's Offset, such as WI_INFO_FID and WI_PARAM0,
 * has doubled.
 * About WI_PCI_COR: In this Register, only soft-reset bit implement; Bit(7).
 */
#define WI_PCI_COR		0x4C
#define WI_PCI_HCR		0x5C
#define WI_PCI_MASTER0_ADDRH	0x80
#define WI_PCI_MASTER0_ADDRL	0x84
#define WI_PCI_MASTER0_LEN	0x88
#define WI_PCI_MASTER0_CON	0x8C

#define WI_PCI_STATUS		0x98

#define WI_PCI_MASTER1_ADDRH	0xA0
#define WI_PCI_MASTER1_ADDRL	0xA4
#define WI_PCI_MASTER1_LEN	0xA8
#define WI_PCI_MASTER1_CON	0xAC

#define WI_PCI_SOFT_RESET	(1 << 7)

/*
 * One form of communication with the Hermes is with what Lucent calls
 * LTV records, where LTV stands for Length, Type and Value. The length
 * and type are 16 bits and are in native byte order. The value is in
 * multiples of 16 bits and is in little endian byte order.
 */
struct wi_ltv_gen {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_val;
};

struct wi_ltv_str {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_str[17];
};

#define WI_SETVAL(recno, val)			\
	do {					\
		struct wi_ltv_gen	g;	\
						\
		g.wi_len = 2;			\
		g.wi_type = recno;		\
		g.wi_val = htole16(val);	\
		wi_write_record(sc, &g);	\
	} while (0)

#define WI_SETSTR(recno, str)					\
	do {							\
		struct wi_ltv_str	s;			\
		int			l;			\
								\
		l = (strlen(str) + 1) & ~0x1;			\
		memset((char *)&s, 0, sizeof(s));		\
		s.wi_len = (l / 2) + 2;				\
		s.wi_type = recno;				\
		s.wi_str[0] = htole16(strlen(str));		\
		memcpy((char *)&s.wi_str[1], str, strlen(str));	\
		wi_write_record(sc, (struct wi_ltv_gen *)&s);	\
	} while (0)

/*
 * Download buffer location and length (0xFD01).
 */
struct wi_ltv_dnld_buf {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_buf_pg; /* page addr of intermediate dl buf*/
	u_int16_t		wi_buf_off; /* offset of idb */
	u_int16_t		wi_buf_len; /* len of idb */
};

/*
 * Mem sizes (0xFD02).
 */
struct wi_ltv_memsz {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_mem_ram;
	u_int16_t		wi_mem_nvram;
};

/*
 * NIC Identification (0xFD0B, 0xFD20)
 */
struct wi_ltv_ver {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_ver[4];
#define WI_NIC_EVB2	0x8000
#define WI_NIC_HWB3763	0x8001
#define WI_NIC_HWB3163	0x8002
#define WI_NIC_HWB3163B	0x8003
#define WI_NIC_EVB3	0x8004
#define WI_NIC_HWB1153	0x8007
#define WI_NIC_P2_SST	0x8008	/* Prism2 with SST flush */
#define WI_NIC_PRISM2_5	0x800C
#define WI_NIC_3874A	0x8013	/* Prism2.5 Mini-PCI */
};

/*
 * List of intended regulatory domains (0xFD11).
 */
struct wi_ltv_domains {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_domains[6];
};

/*
 * CIS struct (0xFD13).
 */
struct wi_ltv_cis {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_cis[240];
};

/*
 * Communications quality (0xFD43).
 */
struct wi_ltv_commqual {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_coms_qual;
	u_int16_t		wi_sig_lvl;
	u_int16_t		wi_noise_lvl;
};

/*
 * Actual system scale thresholds (0xFC06, 0xFD46).
 */
struct wi_ltv_scalethresh {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_energy_detect;
	u_int16_t		wi_carrier_detect;
	u_int16_t		wi_defer;
	u_int16_t		wi_cell_search;
	u_int16_t		wi_out_of_range;
	u_int16_t		wi_delta_snr;
};

/*
 * PCF info struct (0xFD87).
 */
struct wi_ltv_pcf {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_medium_occupancy_limit;
	u_int16_t		wi_cfp_period;
	u_int16_t		wi_cfp_max_duration;
};

/*
 * Connection control characteristics. (0xFC00)
 * 1 == Basic Service Set (BSS)
 * 2 == Wireless Distribudion System (WDS)
 * 3 == Pseudo IBSS
 */
#define WI_PORTTYPE_BSS		0x1
#define WI_PORTTYPE_WDS		0x2
#define WI_PORTTYPE_ADHOC	0x3

/*
 * Mac addresses. (0xFC01, 0xFC08)
 */
struct wi_ltv_macaddr {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int8_t		wi_mac_addr[6];
};

/*
 * Station set identification (SSID). (0xFC02, 0xFC04)
 */
struct wi_ltv_ssid {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_id[17];
};

/*
 * Set our station name. (0xFC0E)
 */
struct wi_ltv_nodename {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	u_int16_t		wi_nodename[17];
};

/*
 * Multicast addresses to be put in filter. We're
 * allowed up to 16 addresses in the filter. (0xFC80)
 */
struct wi_ltv_mcast {
	u_int16_t		wi_len;
	u_int16_t		wi_type;
	struct ether_addr	wi_mcast[16];
};

/*
 * Information frame types.
 */
#define WI_INFO_NOTIFY		0xF000	/* Handover address */
#define WI_INFO_COUNTERS	0xF100	/* Statistics counters */
#define WI_INFO_SCAN_RESULTS	0xF101	/* Scan results */
#define WI_INFO_LINK_STAT	0xF200	/* Link status */
#define WI_INFO_ASSOC_STAT	0xF201	/* Association status */

/*
 * Hermes transmit/receive frame structure
 */
struct wi_frame {
	u_int16_t		wi_status;	/* 0x00 */
	u_int16_t		wi_rsvd0;	/* 0x02 */
	u_int16_t		wi_rsvd1;	/* 0x04 */
	u_int16_t		wi_q_info;	/* 0x06 */
	u_int16_t		wi_rsvd2;	/* 0x08 */
	u_int16_t		wi_rsvd3;	/* 0x0A */
	u_int16_t		wi_tx_ctl;	/* 0x0C */
	u_int16_t		wi_frame_ctl;	/* 0x0E */
	u_int16_t		wi_id;		/* 0x10 */
	u_int8_t		wi_addr1[6];	/* 0x12 */
	u_int8_t		wi_addr2[6];	/* 0x18 */
	u_int8_t		wi_addr3[6];	/* 0x1E */
	u_int16_t		wi_seq_ctl;	/* 0x24 */
	u_int8_t		wi_addr4[6];	/* 0x26 */
	u_int16_t		wi_dat_len;	/* 0x2C */
	u_int8_t		wi_dst_addr[6];	/* 0x2E */
	u_int8_t		wi_src_addr[6];	/* 0x34 */
	u_int16_t		wi_len;		/* 0x3A */
	u_int16_t		wi_dat[3];	/* 0x3C */ /* SNAP header */
	u_int16_t		wi_type;	/* 0x42 */
};

#define WI_802_3_OFFSET		0x2E
#define WI_802_11_OFFSET	0x44
#define WI_802_11_OFFSET_RAW	0x3C

#define WI_STAT_BADCRC		0x0001
#define WI_STAT_UNDECRYPTABLE	0x0002
#define WI_STAT_ERRSTAT		0x0003
#define WI_STAT_MAC_PORT	0x0700
#define WI_STAT_1042		0x2000	/* RFC1042 encoded */
#define WI_STAT_TUNNEL		0x4000	/* Bridge-tunnel encoded */
#define WI_STAT_WMP_MSG		0x6000	/* WaveLAN-II management protocol */
#define WI_RXSTAT_MSG_TYPE	0xE000

#define WI_ENC_TX_802_3		0x00
#define WI_ENC_TX_802_11	0x11
#define WI_ENC_TX_E_II		0x0E

#define WI_ENC_TX_1042		0x00
#define WI_ENC_TX_TUNNEL	0xF8

#define WI_TXCNTL_MACPORT	0x00FF
#define WI_TXCNTL_STRUCTTYPE	0xFF00

/*
 * SNAP (sub-network access protocol) constants for transmission
 * of IP datagrams over IEEE 802 networks, taken from RFC1042.
 * We need these for the LLC/SNAP header fields in the TX/RX frame
 * structure.
 */
#define WI_SNAP_K1		0xaa	/* assigned global SAP for SNAP */
#define WI_SNAP_K2		0x00
#define WI_SNAP_CONTROL		0x03	/* unnumbered information format */
#define WI_SNAP_WORD0		(WI_SNAP_K1 | (WI_SNAP_K1 << 8))
#define WI_SNAP_WORD1		(WI_SNAP_K2 | (WI_SNAP_CONTROL << 8))
#define WI_SNAPHDR_LEN		0x6
