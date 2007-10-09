/* $NetBSD: bt3c.c,v 1.9.6.2 2007/10/09 13:41:58 ad Exp $ */

/*-
 * Copyright (c) 2005 Iain D. Hibbert,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * Driver for the 3Com Bluetooth PC Card 3CRWB6096, written with reference to
 *  FreeBSD and BlueZ drivers for same, with credit for those going to:
 *
 *		Maksim Yevmenkin <m_evmenkin@yahoo.com>		(FreeBSD)
 *		Marcel Holtmann <marcel@holtmann.org>		(BlueZ)
 *		Jose Orlando Pereira <jop@di.uminho.pt>		(BlueZ)
 *		David Hinds <dahinds@users.sourceforge.net>	(Original Code)
 */

/*
 * The CIS info from my card:
 *
 *	pcmcia1: CIS tuple chain:
 *	CISTPL_DEVICE type=null speed=null
 *	 01 03 00 00 ff
 *	CISTPL_VERS_1
 *	 15 24 05 00 33 43 4f 4d 00 33 43 52 57 42 36 30
 *	 2d 41 00 42 6c 75 65 74 6f 6f 74 68 20 50 43 20
 *	 43 61 72 64 00 ff
 *	CISTPL_MANFID
 *	 20 04 01 01 40 00
 *	CISTPL_FUNCID
 *	 21 02 02 01
 *	CISTPL_CONFIG
 *	 1a 06 05 30 20 03 17 00
 *	CISTPL_CFTABLE_ENTRY
 *	 1b 09 f0 41 18 a0 40 07 30 ff ff
 *	unhandled CISTPL 80
 *	 80 0a 02 01 40 00 2d 00 00 00 00 ff
 *	CISTPL_NO_LINK
 *	 14 00
 *	CISTPL_END
 *	 ff
 *	pcmcia1: CIS version PC Card Standard 5.0
 *	pcmcia1: CIS info: 3COM, 3CRWB60-A, Bluetooth PC Card
 *	pcmcia1: Manufacturer code 0x101, product 0x40
 *	pcmcia1: function 0: serial port, ccr addr 320 mask 17
 *	pcmcia1: function 0, config table entry 48: I/O card; irq mask ffff; iomask 0, iospace 0-7; rdybsy_active io8 irqlevel
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bt3c.c,v 1.9.6.2 2007/10/09 13:41:58 ad Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>

#include <dev/firmload.h>
#define BT3C_FIRMWARE_FILE	"BT3CPCC.bin"

/**************************************************************************
 *
 *	bt3c autoconfig glue
 */

struct bt3c_softc {
	struct device	sc_dev;			/* required */

	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o space info */
	int		sc_iow;			/* our i/o window */
	void		*sc_powerhook;		/* power hook descriptor */
	int		sc_flags;		/* flags */

	struct hci_unit sc_unit;		/* Bluetooth HCI Unit */

	/* hardware interrupt */
	void		*sc_intr;		/* cookie */
	int		sc_state;		/* receive state */
	int		sc_want;		/* how much we want */
	struct mbuf	*sc_rxp;		/* incoming packet */
	struct mbuf	*sc_txp;		/* outgoing packet */
};

/* sc_state */				/* receiving */
#define BT3C_RECV_PKT_TYPE	0		/* packet type */
#define BT3C_RECV_ACL_HDR	1		/* acl header */
#define BT3C_RECV_SCO_HDR	2		/* sco header */
#define BT3C_RECV_EVENT_HDR	3		/* event header */
#define BT3C_RECV_ACL_DATA	4		/* acl packet data */
#define BT3C_RECV_SCO_DATA	5		/* sco packet data */
#define BT3C_RECV_EVENT_DATA	6		/* event packet data */

/* sc_flags */
#define BT3C_SLEEPING		(1 << 0)	/* but not with the fishes */

static int bt3c_match(struct device *, struct cfdata *, void *);
static void bt3c_attach(struct device *, struct device *, void *);
static int bt3c_detach(struct device *, int);
static void bt3c_power(int, void *);

CFATTACH_DECL(bt3c, sizeof(struct bt3c_softc),
    bt3c_match, bt3c_attach, bt3c_detach, NULL);

static void bt3c_start(struct hci_unit *);
static int bt3c_enable(struct hci_unit *);
static void bt3c_disable(struct hci_unit *);

/**************************************************************************
 *
 *	Hardware Definitions & IO routines
 *
 *	I made up the names for most of these defs since we dont have
 *	manufacturers recommendations, but I dont like raw numbers..
 *
 *	all hardware routines are running at IPL_TTY
 *
 */
#define BT3C_ISR		0x7001		/* Interrupt Status Register */
#define BT3C_ISR_RXRDY			(1<<0)	/* Device has data */
#define BT3C_ISR_TXRDY			(1<<1)	/* Finished sending data */
#define BT3C_ISR_ANTENNA		(1<<5)	/* Antenna position changed */

#define BT3C_CSR		0x7002		/* Card Status Register */
#define BT3C_CSR_ANTENNA		(1<<4)	/* Antenna position */

#define BT3C_TX_COUNT		0x7005		/* Tx fifo contents */
#define BT3C_TX_FIFO		0x7080		/* Transmit Fifo */
#define BT3C_RX_COUNT		0x7006		/* Rx fifo contents */
#define BT3C_RX_FIFO		0x7480		/* Receive Fifo */
#define BT3C_FIFO_SIZE			256

/* IO Registers */
#define BT3C_IOR_DATA_L		0x00		/* data low byte */
#define BT3C_IOR_DATA_H		0x01		/* data high byte */
#define BT3C_IOR_ADDR_L		0x02		/* address low byte */
#define BT3C_IOR_ADDR_H		0x03		/* address high byte */
#define BT3C_IOR_CNTL		0x04		/* control byte */
#define BT3C_IOR_CNTL_BOOT		(1<<6)	/* Boot Card */
#define BT3C_IOR_CNTL_INTR		(1<<7)	/* Interrupt Requested */
#define BT3C_IOR_LEN		0x05

static inline uint16_t
bt3c_get(struct bt3c_softc *sc)
{
	uint16_t data;

	bus_space_barrier(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
				0, BT3C_IOR_LEN, BUS_SPACE_BARRIER_READ);
	data = bus_space_read_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
				BT3C_IOR_DATA_L);
	data |= bus_space_read_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
				BT3C_IOR_DATA_H) << 8;

	return data;
}

static inline void
bt3c_put(struct bt3c_softc *sc, uint16_t data)
{

	bus_space_barrier(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			0, BT3C_IOR_LEN, BUS_SPACE_BARRIER_WRITE);
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			BT3C_IOR_DATA_L, data & 0xff);
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			BT3C_IOR_DATA_H, (data >> 8) & 0xff);
}

static inline uint8_t
bt3c_read_control(struct bt3c_softc *sc)
{

	bus_space_barrier(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			0, BT3C_IOR_LEN, BUS_SPACE_BARRIER_READ);
	return bus_space_read_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			BT3C_IOR_CNTL);
}

static inline void
bt3c_write_control(struct bt3c_softc *sc, uint8_t data)
{

	bus_space_barrier(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			0, BT3C_IOR_LEN, BUS_SPACE_BARRIER_WRITE);
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			BT3C_IOR_CNTL, data);
}

static inline void
bt3c_set_address(struct bt3c_softc *sc, uint16_t addr)
{

	bus_space_barrier(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			0, BT3C_IOR_LEN, BUS_SPACE_BARRIER_WRITE);
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			BT3C_IOR_ADDR_L, addr & 0xff);
	bus_space_write_1(sc->sc_pcioh.iot, sc->sc_pcioh.ioh,
			BT3C_IOR_ADDR_H, (addr >> 8) & 0xff);
}

static inline uint16_t
bt3c_read(struct bt3c_softc *sc, uint16_t addr)
{

	bt3c_set_address(sc, addr);
	return bt3c_get(sc);
}

static inline void
bt3c_write(struct bt3c_softc *sc, uint16_t addr, uint16_t data)
{

	bt3c_set_address(sc, addr);
	bt3c_put(sc, data);
}

/*
 * receive incoming data from device, store in mbuf chain and
 * pass on complete packets to bt device
 */
static void
bt3c_receive(struct bt3c_softc *sc)
{
	struct mbuf *m = sc->sc_rxp;
	int space = 0;
	uint16_t count;
	uint8_t b;

	/*
	 * If we already started a packet, find the
	 * trailing end of it.
	 */
	if (m) {
		while (m->m_next)
			m = m->m_next;

		space = M_TRAILINGSPACE(m);
	}

	count = bt3c_read(sc, BT3C_RX_COUNT);
	bt3c_set_address(sc, BT3C_RX_FIFO);

	while (count > 0) {
		if (space == 0) {
			if (m == NULL) {
				/* new packet */
				MGETHDR(m, M_DONTWAIT, MT_DATA);
				if (m == NULL) {
					printf("%s: out of memory\n",
						sc->sc_dev.dv_xname);
					++sc->sc_unit.hci_stats.err_rx;
					goto out;	/* (lost sync) */
				}

				sc->sc_rxp = m;
				m->m_pkthdr.len = m->m_len = 0;
				space = MHLEN;

				sc->sc_state = BT3C_RECV_PKT_TYPE;
				sc->sc_want = 1;
			} else {
				/* extend mbuf */
				MGET(m->m_next, M_DONTWAIT, MT_DATA);
				if (m->m_next == NULL) {
					printf("%s: out of memory\n",
						sc->sc_dev.dv_xname);
					++sc->sc_unit.hci_stats.err_rx;
					goto out;	/* (lost sync) */
				}

				m = m->m_next;
				m->m_len = 0;
				space = MLEN;

				if (sc->sc_want > MINCLSIZE) {
					MCLGET(m, M_DONTWAIT);
					if (m->m_flags & M_EXT)
						space = MCLBYTES;
				}
			}
		}

		b = bt3c_get(sc);
		mtod(m, uint8_t *)[m->m_len++] = b;
		count--;
		space--;
		sc->sc_rxp->m_pkthdr.len++;
		sc->sc_unit.hci_stats.byte_rx++;

		sc->sc_want--;
		if (sc->sc_want > 0)
			continue; /* want more */

		switch (sc->sc_state) {
		case BT3C_RECV_PKT_TYPE:		/* Got packet type */

			switch (b) {
			case HCI_ACL_DATA_PKT:
				sc->sc_state = BT3C_RECV_ACL_HDR;
				sc->sc_want = sizeof(hci_acldata_hdr_t) - 1;
				break;

			case HCI_SCO_DATA_PKT:
				sc->sc_state = BT3C_RECV_SCO_HDR;
				sc->sc_want = sizeof(hci_scodata_hdr_t) - 1;
				break;

			case HCI_EVENT_PKT:
				sc->sc_state = BT3C_RECV_EVENT_HDR;
				sc->sc_want = sizeof(hci_event_hdr_t) - 1;
				break;

			default:
				printf("%s: Unknown packet type=%#x!\n",
					sc->sc_dev.dv_xname, b);
				++sc->sc_unit.hci_stats.err_rx;
				m_freem(sc->sc_rxp);
				sc->sc_rxp = NULL;
				goto out;	/* (lost sync) */
			}

			break;

		/*
		 * we assume (correctly of course :) that the packet headers
		 * all fit into a single pkthdr mbuf
		 */
		case BT3C_RECV_ACL_HDR:		/* Got ACL Header */
			sc->sc_state = BT3C_RECV_ACL_DATA;
			sc->sc_want = mtod(m, hci_acldata_hdr_t *)->length;
			sc->sc_want = le16toh(sc->sc_want);
			break;

		case BT3C_RECV_SCO_HDR:		/* Got SCO Header */
			sc->sc_state = BT3C_RECV_SCO_DATA;
			sc->sc_want =  mtod(m, hci_scodata_hdr_t *)->length;
			break;

		case BT3C_RECV_EVENT_HDR:	/* Got Event Header */
			sc->sc_state = BT3C_RECV_EVENT_DATA;
			sc->sc_want =  mtod(m, hci_event_hdr_t *)->length;
			break;

		case BT3C_RECV_ACL_DATA:	/* ACL Packet Complete */
			hci_input_acl(&sc->sc_unit, sc->sc_rxp);
			sc->sc_unit.hci_stats.acl_rx++;
			sc->sc_rxp = m = NULL;
			space = 0;
			break;

		case BT3C_RECV_SCO_DATA:	/* SCO Packet Complete */
			hci_input_sco(&sc->sc_unit, sc->sc_rxp);
			sc->sc_unit.hci_stats.sco_rx++;
			sc->sc_rxp = m = NULL;
			space = 0;
			break;

		case BT3C_RECV_EVENT_DATA:	/* Event Packet Complete */
			sc->sc_unit.hci_stats.evt_rx++;
			hci_input_event(&sc->sc_unit, sc->sc_rxp);
			sc->sc_rxp = m = NULL;
			space = 0;
			break;

		default:
			panic("%s: invalid state %d!\n",
				sc->sc_dev.dv_xname, sc->sc_state);
		}
	}

out:
	bt3c_write(sc, BT3C_RX_COUNT, 0x0000);
}

/*
 * write data from current packet to Transmit FIFO.
 * restart when done.
 */
static void
bt3c_transmit(struct bt3c_softc *sc)
{
	struct mbuf *m;
	int count, rlen;
	uint8_t *rptr;

	m = sc->sc_txp;
	if (m == NULL) {
		sc->sc_unit.hci_flags &= ~BTF_XMIT;
		bt3c_start(&sc->sc_unit);
		return;
	}

	count = 0;
	rlen = 0;
	rptr = mtod(m, uint8_t *);

	bt3c_set_address(sc, BT3C_TX_FIFO);

	for(;;) {
		if (rlen >= m->m_len) {
			m = m->m_next;
			if (m == NULL) {
				m = sc->sc_txp;
				sc->sc_txp = NULL;

				if (M_GETCTX(m, void *) == NULL)
					m_freem(m);
				else
					hci_complete_sco(&sc->sc_unit, m);

				break;
			}

			rlen = 0;
			rptr = mtod(m, uint8_t *);
			continue;
		}

		if (count >= BT3C_FIFO_SIZE) {
			m_adj(m, rlen);
			break;
		}

		bt3c_put(sc, *rptr++);
		rlen++;
		count++;
	}

	bt3c_write(sc, BT3C_TX_COUNT, count);
	sc->sc_unit.hci_stats.byte_tx += count;
}

/*
 * interrupt routine
 */
static int
bt3c_intr(void *arg)
{
	struct bt3c_softc *sc = arg;
	uint16_t control, isr;

	control = bt3c_read_control(sc);
	if (control & BT3C_IOR_CNTL_INTR) {
		isr = bt3c_read(sc, BT3C_ISR);
		if ((isr & 0xff) == 0x7f) {
			printf("%s: bt3c_intr got strange ISR=%04x\n",
				sc->sc_dev.dv_xname, isr);
		} else if ((isr & 0xff) != 0xff) {

			if (isr & BT3C_ISR_RXRDY)
				bt3c_receive(sc);

			if (isr & BT3C_ISR_TXRDY)
				bt3c_transmit(sc);

#ifdef DIAGNOSTIC
			if (isr & BT3C_ISR_ANTENNA) {
				if (bt3c_read(sc, BT3C_CSR) & BT3C_CSR_ANTENNA)
					printf("%s: Antenna Out\n",
						sc->sc_dev.dv_xname);
				else
					printf("%s: Antenna In\n",
						sc->sc_dev.dv_xname);
			}
#endif

			bt3c_write(sc, BT3C_ISR, 0x0000);
			bt3c_write_control(sc, control);

			return 1; /* handled */
		}
	}

	return 0; /* not handled */
}

/*
 * load firmware for the device
 *
 * The firmware file is a plain ASCII file in the Motorola S-Record format,
 * with lines in the format:
 *
 *	S<Digit><Len><Address><Data1><Data2>...<DataN><Checksum>
 *
 * <Digit>:	0	header
 *		3	data record (4 byte address)
 *		7	boot record (4 byte address)
 *
 * <Len>:	1 byte, and is the number of bytes in the rest of the line
 * <Address>:	4 byte address (only 2 bytes are valid for bt3c I think)
 * <Data>:	2 byte data word to be written to the address
 * <Checksum>:	checksum of all bytes in the line including <Len>
 *
 * all bytes are in hexadecimal
 */
static inline int32_t
hex(const uint8_t *p, int n)
{
	uint32_t val = 0;

	while (n > 0) {
		val <<= 4;

		if ('0' <= *p && *p <= '9')
			val += (*p - '0');
		else if ('a' <= *p && *p <= 'f')
			val += (*p - 'a' + 0xa);
		else if ('A' <= *p && *p <= 'F')
			val += (*p - 'A' + 0xa);
		else
			return -1;

		p++;
		n--;
	}

	return val;
}

static int
bt3c_load_firmware(struct bt3c_softc *sc)
{
	uint8_t *buf, *line, *next, *p;
	int32_t addr, data;
	int err, sum, len;
	firmware_handle_t fh;
	size_t size;

	err = firmware_open(sc->sc_dev.dv_cfdata->cf_name,
			    BT3C_FIRMWARE_FILE, &fh);
	if (err) {
		printf("%s: Cannot open firmware %s/%s\n", sc->sc_dev.dv_xname,
		    sc->sc_dev.dv_cfdata->cf_name, BT3C_FIRMWARE_FILE);
		return err;
	}

	size = (size_t)firmware_get_size(fh);
#ifdef DIAGNOSTIC
	if (size > 10 * 1024) {	/* sanity check */
		printf("%s: firmware file seems WAY too big!\n",
			sc->sc_dev.dv_xname);
		firmware_close(fh);
		return EFBIG;
	}
#endif

	buf = firmware_malloc(size);
	KASSERT(buf != NULL);

	err = firmware_read(fh, 0, buf, size);
	if (err) {
		printf("%s: Firmware read failed (%d)\n",
				sc->sc_dev.dv_xname, err);
		goto out;
	}

	/* Reset */
	bt3c_write(sc, 0x8040, 0x0404);
	bt3c_write(sc, 0x8040, 0x0400);
	DELAY(1);
	bt3c_write(sc, 0x8040, 0x0404);
	DELAY(17);

	next = buf;
	err = EFTYPE;

	while (next < buf + size) {
		line = next;

		while (*next != '\r' && *next != '\n') {
			if (next >= buf + size)
				goto out;

			next++;
		}

		/* 14 covers address and checksum minimum */
		if (next - line < 14)
			goto out;

		if (line[0] != 'S')
			goto out;

		/* verify line length */
		len = hex(line + 2, 2);
		if (len < 0 || next - line != len * 2 + 4)
			goto out;

		/* checksum the line */
		sum = 0;
		for (p = line + 2 ; p < next ; p += 2)
			sum += hex(p, 2);

		if ((sum & 0xff) != 0xff)
			goto out;

		/* extract relevant data */
		switch (line[1]) {
		case '0':
			/* we ignore the header */
			break;

		case '3':
			/* find number of data words */
			len = (len - 5) / 2;
			if (len > 15)
				goto out;

			addr = hex(line + 8, 4);
			if (addr < 0)
				goto out;

			bt3c_set_address(sc, addr);

			for (p = line + 12 ; p + 4 < next ; p += 4) {
				data = hex(p, 4);
				if (data < 0)
					goto out;

				bt3c_put(sc, data);
			}
			break;

		case '7':
			/*
			 * for some reason we ignore this record
			 * and boot from 0x3000 which happens to
			 * be the first record in the file.
			 */
			break;

		default:
			goto out;
		}

		/* skip to start of next line */
		while (next < buf + size && (*next == '\r' || *next == '\n'))
			next++;
	}

	err = 0;
	DELAY(17);

	/* Boot */
	bt3c_set_address(sc, 0x3000);
	bt3c_write_control(sc, (bt3c_read_control(sc) | BT3C_IOR_CNTL_BOOT));
	DELAY(17);

	/* Clear Registers */
	bt3c_write(sc, BT3C_RX_COUNT, 0x0000);
	bt3c_write(sc, BT3C_TX_COUNT, 0x0000);
	bt3c_write(sc, BT3C_ISR, 0x0000);
	DELAY(1000);

out:
	firmware_free(buf, size);
	firmware_close(fh);
	return err;
}

/**************************************************************************
 *
 *  bt device callbacks (all called at IPL_TTY)
 */

/*
 * start sending on bt3c
 * this should be called only when BTF_XMIT is not set, and
 * we only send cmd packets that are clear to send
 */
static void
bt3c_start(struct hci_unit *unit)
{
	struct bt3c_softc *sc = unit->hci_softc;
	struct mbuf *m;

	KASSERT((unit->hci_flags & BTF_XMIT) == 0);
	KASSERT(sc->sc_txp == NULL);

	if (MBUFQ_FIRST(&unit->hci_cmdq)) {
		MBUFQ_DEQUEUE(&unit->hci_cmdq, m);
		unit->hci_stats.cmd_tx++;
		M_SETCTX(m, NULL);
		goto start;
	}

	if (MBUFQ_FIRST(&unit->hci_scotxq)) {
		MBUFQ_DEQUEUE(&unit->hci_scotxq, m);
		unit->hci_stats.sco_tx++;
		goto start;
	}

	if (MBUFQ_FIRST(&unit->hci_acltxq)) {
		MBUFQ_DEQUEUE(&unit->hci_acltxq, m);
		unit->hci_stats.acl_tx++;
		M_SETCTX(m, NULL);
		goto start;
	}

	/* Nothing to send */
	return;

start:
	sc->sc_txp = m;
	unit->hci_flags |= BTF_XMIT;
	bt3c_transmit(sc);
}

/*
 * enable device
 *	turn on card
 *	load firmware
 *	establish interrupts
 */
static int
bt3c_enable(struct hci_unit *unit)
{
	struct bt3c_softc *sc = unit->hci_softc;
	int err;

	if (unit->hci_flags & BTF_RUNNING)
		return 0;

	sc->sc_intr = pcmcia_intr_establish(sc->sc_pf, IPL_TTY, bt3c_intr, sc);
	if (sc->sc_intr == NULL) {
		err = EIO;
		goto bad;
	}

	err = pcmcia_function_enable(sc->sc_pf);
	if (err)
		goto bad1;

	err = bt3c_load_firmware(sc);
	if (err)
		goto bad2;

	unit->hci_flags |= BTF_RUNNING;
	unit->hci_flags &= ~BTF_XMIT;

	/*
	 * 3Com card will send a Command_Status packet when its
	 * ready to receive commands
	 */
	unit->hci_num_cmd_pkts = 0;

	return 0;

bad2:
	pcmcia_function_disable(sc->sc_pf);
bad1:
	pcmcia_intr_disestablish(sc->sc_pf, sc->sc_intr);
	sc->sc_intr = NULL;
bad:
	return err;
}

/*
 * disable device
 *	shut down card
 *	disestablish interrupts
 *	free held packets
 */
static void
bt3c_disable(struct hci_unit *unit)
{
	struct bt3c_softc *sc = unit->hci_softc;

	if ((unit->hci_flags & BTF_RUNNING) == 0)
		return;

	pcmcia_function_disable(sc->sc_pf);

	if (sc->sc_intr) {
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_intr);
		sc->sc_intr = NULL;
	}

	if (sc->sc_rxp) {
		m_freem(sc->sc_rxp);
		sc->sc_rxp = NULL;
	}

	if (sc->sc_txp) {
		m_freem(sc->sc_txp);
		sc->sc_txp = NULL;
	}

	unit->hci_flags &= ~BTF_RUNNING;
}

/**************************************************************************
 *
 *	bt3c PCMCIA autoconfig glue
 */

static int
bt3c_match(struct device *parent, struct cfdata *match,
    void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->manufacturer == PCMCIA_VENDOR_3COM &&
	    pa->product == PCMCIA_PRODUCT_3COM_3CRWB6096)
	    return 10;		/* 'com' also claims this, so trump them */

	return 0;
}

static void
bt3c_attach(struct device *parent, struct device *self, void *aux)
{
	struct bt3c_softc *sc = (struct bt3c_softc *)self;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;

	sc->sc_pf = pa->pf;

	/* Find a PCMCIA config entry we can use */
	SIMPLEQ_FOREACH(cfe, &pa->pf->cfe_head, cfe_list) {
		if (cfe->num_memspace != 0)
			continue;

		if (cfe->num_iospace != 1)
			continue;

		if (pcmcia_io_alloc(pa->pf, cfe->iospace[0].start,
				cfe->iospace[0].length, 0, &sc->sc_pcioh) == 0)
			break;
	}

	if (cfe == 0) {
		aprint_error("bt3c_attach: cannot allocate io space\n");
		goto no_config_entry;
	}

	/* Initialise it */
	pcmcia_function_init(pa->pf, cfe);

	/* Map in the io space */
	if (pcmcia_io_map(pa->pf, PCMCIA_WIDTH_AUTO,
			&sc->sc_pcioh, &sc->sc_iow)) {
		aprint_error("bt3c_attach: cannot map io space\n");
		goto iomap_failed;
	}

	/* Attach Bluetooth unit */
	sc->sc_unit.hci_softc = sc;
	sc->sc_unit.hci_devname = sc->sc_dev.dv_xname;
	sc->sc_unit.hci_enable = bt3c_enable;
	sc->sc_unit.hci_disable = bt3c_disable;
	sc->sc_unit.hci_start_cmd = bt3c_start;
	sc->sc_unit.hci_start_acl = bt3c_start;
	sc->sc_unit.hci_start_sco = bt3c_start;
	sc->sc_unit.hci_ipl = makeiplcookie(IPL_TTY);
	hci_attach(&sc->sc_unit);

	/* establish a power change hook */
	sc->sc_powerhook = powerhook_establish(sc->sc_dev.dv_xname,
	    bt3c_power, sc);
	return;

iomap_failed:
	/* unmap io space */
	pcmcia_io_free(pa->pf, &sc->sc_pcioh);

no_config_entry:
	sc->sc_iow = -1;
}

static int
bt3c_detach(struct device *self, int flags)
{
	struct bt3c_softc *sc = (struct bt3c_softc *)self;
	int err = 0;

	bt3c_disable(&sc->sc_unit);

	if (sc->sc_powerhook) {
		powerhook_disestablish(sc->sc_powerhook);
		sc->sc_powerhook = NULL;
	}

	hci_detach(&sc->sc_unit);

	if (sc->sc_iow != -1) {
		pcmcia_io_unmap(sc->sc_pf, sc->sc_iow);
		pcmcia_io_free(sc->sc_pf, &sc->sc_pcioh);
		sc->sc_iow = -1;
	}

	return err;
}

static void
bt3c_power(int why, void *arg)
{
	struct bt3c_softc *sc = arg;

	switch(why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		if (sc->sc_unit.hci_flags & BTF_RUNNING) {
			hci_detach(&sc->sc_unit);

			sc->sc_flags |= BT3C_SLEEPING;
			printf_nolog("%s: sleeping\n", sc->sc_dev.dv_xname);
		}
		break;

	case PWR_RESUME:
		if (sc->sc_flags & BT3C_SLEEPING) {
			printf_nolog("%s: waking up\n", sc->sc_dev.dv_xname);
			sc->sc_flags &= ~BT3C_SLEEPING;

			memset(&sc->sc_unit, 0, sizeof(sc->sc_unit));
			sc->sc_unit.hci_softc = sc;
			sc->sc_unit.hci_devname = sc->sc_dev.dv_xname;
			sc->sc_unit.hci_enable = bt3c_enable;
			sc->sc_unit.hci_disable = bt3c_disable;
			sc->sc_unit.hci_start_cmd = bt3c_start;
			sc->sc_unit.hci_start_acl = bt3c_start;
			sc->sc_unit.hci_start_sco = bt3c_start;
			sc->sc_unit.hci_ipl = makeiplcookie(IPL_TTY);
			hci_attach(&sc->sc_unit);
		}
		break;

	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
	}
}
