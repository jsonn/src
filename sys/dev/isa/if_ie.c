/*	$NetBSD: if_ie.c,v 1.61.2.1 1997/11/22 03:51:40 mellon Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995 Charles Hannum.
 * Copyright (c) 1992, 1993, University of Vermont and State
 *  Agricultural College.
 * Copyright (c) 1992, 1993, Garrett A. Wollman.
 *
 * Portions:
 * Copyright (c) 1993, 1994, 1995, Rodney W. Grimes
 * Copyright (c) 1994, 1995, Rafal K. Boni
 * Copyright (c) 1990, 1991, William F. Jolitz
 * Copyright (c) 1990, The Regents of the University of California
 *
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles Hannum, by the
 *	University of Vermont and State Agricultural College and Garrett A.
 *	Wollman, by William F. Jolitz, and by the University of California,
 *	Berkeley, Lawrence Berkeley Laboratory, and its contributors.
 * 4. Neither the names of the Universities nor the names of the authors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR AUTHORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Intel 82586 Ethernet chip
 * Register, bit, and structure definitions.
 *
 * Original StarLAN driver written by Garrett Wollman with reference to the
 * Clarkson Packet Driver code for this chip written by Russ Nelson and others.
 *
 * BPF support code taken from hpdev/if_le.c, supplied with tcpdump.
 *
 * 3C507 support is loosely based on code donated to NetBSD by Rafal Boni.
 *
 * Intel EtherExpress 16 support taken from FreeBSD's if_ix.c, written
 * by Rodney W. Grimes.
 *
 * Majorly cleaned up and 3C507 code merged by Charles Hannum.
 */

/*
 * The i82586 is a very versatile chip, found in many implementations.
 * Programming this chip is mostly the same, but certain details differ
 * from card to card.  This driver is written so that different cards
 * can be automatically detected at run-time.
 */

/*
Mode of operation:

We run the 82586 in a standard Ethernet mode.  We keep NFRAMES received frame
descriptors around for the receiver to use, and NRXBUF associated receive
buffer descriptors, both in a circular list.  Whenever a frame is received, we
rotate both lists as necessary.  (The 586 treats both lists as a simple
queue.)  We also keep a transmit command around so that packets can be sent
off quickly.

We configure the adapter in AL-LOC = 1 mode, which means that the
Ethernet/802.3 MAC header is placed at the beginning of the receive buffer
rather than being split off into various fields in the RFD.  This also means
that we must include this header in the transmit buffer as well.

By convention, all transmit commands, and only transmit commands, shall have
the I (IE_CMD_INTR) bit set in the command.  This way, when an interrupt
arrives at ieintr(), it is immediately possible to tell what precisely caused
it.  ANY OTHER command-sending routines should run at splnet(), and should
post an acknowledgement to every interrupt they generate.

The 82586 has a 24-bit address space internally, and the adaptor's memory is
located at the top of this region.  However, the value we are given in
configuration is the CPU's idea of where the adaptor RAM is.  So, we must go
through a few gyrations to come up with a kernel virtual address which
represents the actual beginning of the 586 address space.  First, we autosize
the RAM by running through several possible sizes and trying to initialize the
adapter under the assumption that the selected size is correct.  Then, knowing
the correct RAM size, we set up our pointers in the softc.  `sc_maddr'
represents the computed base of the 586 address space.  `iomembot' represents
the actual configured base of adapter RAM.  Finally, `sc_msize' represents the
calculated size of 586 RAM.  Then, when laying out commands, we use the
interval [sc_maddr, sc_maddr + sc_msize); to make 24-pointers, we subtract
iomem, and to make 16-pointers, we subtract sc_maddr and and with 0xffff.
*/

#include "bpfilter.h"
#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/if_ether.h>


#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/pio.h>		/* XXX convert this driver! */
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <i386/isa/isa_machdep.h>	/* XXX USES ISA HOLE DIRECTLY */
#include <dev/ic/i82586reg.h>
#include <dev/isa/if_ieatt.h>
#include <dev/isa/if_ie507.h>
#include <dev/isa/if_iee16.h>
#include <dev/isa/elink.h>

#define	IED_RINT	0x01
#define	IED_TINT	0x02
#define	IED_RNR		0x04
#define	IED_CNA		0x08
#define	IED_READFRAME	0x10
#define	IED_ENQ		0x20
#define	IED_XMIT	0x40
#define	IED_ALL		0x7f

#define	ETHER_MIN_LEN	64
#define	ETHER_MAX_LEN	1518
#define	ETHER_ADDR_LEN	6

/*
sizeof(iscp) == 1+1+2+4 == 8
sizeof(scb) == 2+2+2+2+2+2+2+2 == 16
NFRAMES * sizeof(rfd) == NFRAMES*(2+2+2+2+6+6+2+2) == NFRAMES*24 == 384
sizeof(xmit_cmd) == 2+2+2+2+6+2 == 18
sizeof(transmit buffer) == ETHER_MAX_LEN == 1518
sizeof(transmit buffer desc) == 8
-----
1952

NRXBUF * sizeof(rbd) == NRXBUF*(2+2+4+2+2) == NRXBUF*12
NRXBUF * IE_RBUF_SIZE == NRXBUF*256

NRXBUF should be (16384 - 1952) / (256 + 12) == 14432 / 268 == 53

With NRXBUF == 48, this leaves us 1568 bytes for another command or
more buffers.  Another transmit command would be 18+8+1518 == 1544
---just barely fits!

Obviously all these would have to be reduced for smaller memory sizes.
With a larger memory, it would be possible to roughly double the number of
both transmit and receive buffers.
*/

#define	NFRAMES		16		/* number of receive frames */
#define	NRXBUF		48		/* number of buffers to allocate */
#define	IE_RBUF_SIZE	256		/* size of each receive buffer;
						MUST BE POWER OF TWO */
#define	NTXBUF		2		/* number of transmit commands */
#define	IE_TBUF_SIZE	ETHER_MAX_LEN	/* length of transmit buffer */


enum ie_hardware {
	IE_STARLAN10,
	IE_EN100,
	IE_SLFIBER,
	IE_3C507,
	IE_EE16,
	IE_UNKNOWN
};

const char *ie_hardware_names[] = {
	"StarLAN 10",
	"EN100",
	"StarLAN Fiber",
	"3C507",
	"EtherExpress 16",
	"Unknown"
};

/*
 * Ethernet status, per interface.
 */
struct ie_softc {
	struct device sc_dev;
	void *sc_ih;

	/* bus handles */
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	caddr_t sc_maddr;
	u_int sc_msize;

	struct ethercom sc_ethercom;

	void (*reset_586) __P((struct ie_softc *));
	void (*chan_attn) __P((struct ie_softc *));

	enum ie_hardware hard_type;
	int hard_vers;

	int want_mcsetup;
	int promisc;
	volatile struct ie_int_sys_conf_ptr *iscp;
	volatile struct ie_sys_ctl_block *scb;

	int rfhead, rftail, rbhead, rbtail;
	volatile struct ie_recv_frame_desc *rframes[NFRAMES];
	volatile struct ie_recv_buf_desc *rbuffs[NRXBUF];
	volatile char *cbuffs[NRXBUF];

	int xmit_busy;
	int xchead, xctail;
	volatile struct ie_xmit_cmd *xmit_cmds[NTXBUF];
	volatile struct ie_xmit_buf *xmit_buffs[NTXBUF];
	u_char *xmit_cbuffs[NTXBUF];

	struct ie_en_addr mcast_addrs[MAXMCAST + 1];
	int mcast_count;

	u_short	irq_encoded;		/* encoded interrupt on IEE16 */

#ifdef IEDEBUG
	int sc_debug;
#endif
	u_int8_t sc_enaddr[6];
	u_int8_t sc_pad[2];

#if NRND > 0
	rndsource_element_t rnd_source;
#endif
};

void iewatchdog __P((struct ifnet *));
int ieintr __P((void *));
void iestop __P((struct ie_softc *));
int ieinit __P((struct ie_softc *));
int ieioctl __P((struct ifnet *, u_long, caddr_t));
void iestart __P((struct ifnet *));
static void el_reset_586 __P((struct ie_softc *));
static void sl_reset_586 __P((struct ie_softc *));
static void el_chan_attn __P((struct ie_softc *));
static void sl_chan_attn __P((struct ie_softc *));
static void slel_get_address __P((struct ie_softc *));

static void ee16_reset_586 __P((struct ie_softc *));
static void ee16_chan_attn __P((struct ie_softc *));
static void ee16_interrupt_enable __P((struct ie_softc *));
void ee16_eeprom_outbits __P((struct ie_softc *, int, int));
void ee16_eeprom_clock __P((struct ie_softc *, int));
u_short ee16_read_eeprom __P((struct ie_softc *, int));
int ee16_eeprom_inbits __P((struct ie_softc *));

void iereset __P((struct ie_softc *));
void ie_readframe __P((struct ie_softc *, int));
void ie_drop_packet_buffer __P((struct ie_softc *));
void ie_find_mem_size __P((struct ie_softc *));
static int command_and_wait __P((struct ie_softc *, int,
    void volatile *, int));
void ierint __P((struct ie_softc *));
void ietint __P((struct ie_softc *));
void iexmit __P((struct ie_softc *));
struct mbuf *ieget __P((struct ie_softc *,
    struct ether_header *, int *));
void iememinit __P((void *, struct ie_softc *));
static int mc_setup __P((struct ie_softc *, void *));
static void mc_reset __P((struct ie_softc *));

#ifdef IEDEBUG
void print_rbd __P((volatile struct ie_recv_buf_desc *));

int in_ierint = 0;
int in_ietint = 0;
#endif

int	ieprobe __P((struct device *, void *, void *));
void	ieattach __P((struct device *, struct device *, void *));
int	sl_probe __P((struct ie_softc *, struct isa_attach_args *));
int	el_probe __P((struct ie_softc *, struct isa_attach_args *));
int	ee16_probe __P((struct ie_softc *, struct isa_attach_args *));
int	check_ie_present __P((struct ie_softc *, caddr_t, u_int));

static __inline void ie_setup_config __P((volatile struct ie_config_cmd *,
    int, int));
static __inline void ie_ack __P((struct ie_softc *, u_int));
static __inline int ether_equal __P((u_char *, u_char *));
static __inline int check_eh __P((struct ie_softc *, struct ether_header *,
    int *));
static __inline int ie_buflen __P((struct ie_softc *, int));
static __inline int ie_packet_len __P((struct ie_softc *));

static void chan_attn_timeout __P((void *));
static void run_tdr __P((struct ie_softc *, struct ie_tdr_cmd *));

struct cfattach ie_ca = {
	sizeof(struct ie_softc), ieprobe, ieattach
};

struct cfdriver ie_cd = {
	NULL, "ie", DV_IFNET
};

#define MK_24(base, ptr) ((caddr_t)((u_long)ptr - (u_long)base))
#define MK_16(base, ptr) ((u_short)(u_long)MK_24(base, ptr))

#define MEM 	sc->sc_maddr

#define IE_NPORTS 16 /*  the number of I/O ports */

/*
 * Here are a few useful functions.  We could have done these as macros, but
 * since we have the inline facility, it makes sense to use that instead.
 */
static __inline void
ie_setup_config(cmd, promiscuous, manchester)
	volatile struct ie_config_cmd *cmd;
	int promiscuous, manchester;
{

	cmd->ie_config_count = 0x0c;
	cmd->ie_fifo = 8;
	cmd->ie_save_bad = 0x40;
	cmd->ie_addr_len = 0x2e;
	cmd->ie_priority = 0;
	cmd->ie_ifs = 0x60;
	cmd->ie_slot_low = 0;
	cmd->ie_slot_high = 0xf2;
	cmd->ie_promisc = promiscuous | manchester << 2;
	cmd->ie_crs_cdt = 0;
	cmd->ie_min_len = 64;
	cmd->ie_junk = 0xff;
}

static __inline void
ie_ack(sc, mask)
	struct ie_softc *sc;
	u_int mask;
{
	volatile struct ie_sys_ctl_block *scb = sc->scb;

	scb->ie_command = scb->ie_status & mask;
	(sc->chan_attn)(sc);

	while (scb->ie_command)
		;		/* Spin Lock */
}

int
ieprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct ie_softc *sc = match;
	struct isa_attach_args *ia = aux;
	int found;

	sc->sc_iot = ia->ia_iot;

	/* Disallow wildcarded values. */
	if (ia->ia_irq == ISACF_IRQ_DEFAULT)
		return (0);
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
		return (0);

	/* Map i/o space. */
	if (bus_space_map(sc->sc_iot, ia->ia_iobase, IE_NPORTS, 0, &sc->sc_ioh))
		return 0;

	if (sl_probe(sc, ia)) {
		found = 1;
		goto out;
	}
	if (el_probe(sc, ia)) {
		found = 1;
		goto out;
	}
	if (ee16_probe(sc, ia)) {
		found = 1;
		goto out;
	}
	found = 0;
out:
	if (found)
		ia->ia_iosize = IE_NPORTS;
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, IE_NPORTS);
	return (found);
}

int
sl_probe(sc, ia)
	struct ie_softc *sc;
	struct isa_attach_args *ia;
{
	u_char c;

	/* Need this for part of the probe. */
	sc->reset_586 = sl_reset_586;
	sc->chan_attn = sl_chan_attn;

	c = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IEATT_REVISION);
	switch (SL_BOARD(c)) {
	case SL10_BOARD:
		sc->hard_type = IE_STARLAN10;
		break;
	case EN100_BOARD:
		sc->hard_type = IE_EN100;
		break;
	case SLFIBER_BOARD:
		sc->hard_type = IE_SLFIBER;
		break;

	default:
		/* Anything else is not recognized or cannot be used. */
#ifdef IEDEBUG
		printf("%s: unknown AT&T board type code %d\n",
		    sc->sc_dev.dv_xname, SL_BOARD(c));
#endif
		return 0;
	}

	sc->hard_vers = SL_REV(c);

	if (ia->ia_irq == IRQUNK || ia->ia_maddr == MADDRUNK) {
		printf("%s: %s does not have soft configuration\n",
		    sc->sc_dev.dv_xname, ie_hardware_names[sc->hard_type]);
		return 0;
	}

	/*
	 * Divine memory size on-board the card.  Ususally 16k.
	 */
	sc->sc_maddr = ISA_HOLE_VADDR(ia->ia_maddr);
	ie_find_mem_size(sc);

	if (!sc->sc_msize) {
		printf("%s: can't find shared memory\n", sc->sc_dev.dv_xname);
		return 0;
	}

	if (!ia->ia_msize)
		ia->ia_msize = sc->sc_msize;
	else if (ia->ia_msize != sc->sc_msize) {
		printf("%s: msize mismatch; kernel configured %d != board configured %d\n",
		    sc->sc_dev.dv_xname, ia->ia_msize, sc->sc_msize);
		return 0;
	}

	slel_get_address(sc);

	return 1;
}

int
el_probe(sc, ia)
	struct ie_softc *sc;
	struct isa_attach_args *ia;
{
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	u_char c;
	int i, rval = 0;
	u_char signature[] = "*3COM*";

	/* Need this for part of the probe. */
	sc->reset_586 = el_reset_586;
	sc->chan_attn = el_chan_attn;

	/*
	 * Map the Etherlink ID port for the probe sequence.
	 */
	if (bus_space_map(iot, ELINK_ID_PORT, 1, 0, &ioh)) {
		printf("3c507 probe: can't map Etherlink ID port\n");
		return 0;
	}

	/*
	 * Reset and put card in CONFIG state without changing address.
	 * XXX Indirect brokenness here!
	 */
	elink_reset(iot, ioh, sc->sc_dev.dv_parent->dv_unit);
	elink_idseq(iot, ioh, ELINK_507_POLY);
	elink_idseq(iot, ioh, ELINK_507_POLY);
	bus_space_write_1(iot, ioh, 0, 0xff);

	/* Check for 3COM signature before proceeding. */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IE507_CTRL,
		bus_space_read_1(sc->sc_iot, sc->sc_ioh, IE507_CTRL) & 0xfc); /* XXX */
	for (i = 0; i < 6; i++)
		if (bus_space_read_1(sc->sc_iot, sc->sc_ioh, i) != signature[i])
			goto out;

	c = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IE507_MADDR);
	if (c & 0x20) {
		printf("%s: can't map 3C507 RAM in high memory\n",
		    sc->sc_dev.dv_xname);
		goto out;
	}

	/* Go to RUN state. */
	bus_space_write_1(iot, ioh, 0, 0x0);
	elink_idseq(iot, ioh, ELINK_507_POLY);
	bus_space_write_1(iot, ioh, 0, 0x0);

	/* Set bank 2 for version info and read BCD version byte. */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IE507_CTRL,
		EL_CTRL_NRST | EL_CTRL_BNK2);
	i = bus_space_read_1(sc->sc_iot, sc->sc_ioh, 3);

	sc->hard_type = IE_3C507;
	sc->hard_vers = 10*(i / 16) + (i % 16) - 1;

	i = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IE507_IRQ) & 0x0f;

	if (ia->ia_irq != IRQUNK) {
		if (ia->ia_irq != i) {
			printf("%s: irq mismatch; kernel configured %d != board configured %d\n",
			    sc->sc_dev.dv_xname, ia->ia_irq, i);
			goto out;
		}
	} else
		ia->ia_irq = i;

	i = ((bus_space_read_1(sc->sc_iot, sc->sc_ioh, IE507_MADDR) & 0x1c) << 12)
		+ 0xc0000;

	if (ia->ia_maddr != MADDRUNK) {
		if (ia->ia_maddr != i) {
			printf("%s: maddr mismatch; kernel configured %x != board configured %x\n",
			    sc->sc_dev.dv_xname, ia->ia_maddr, i);
			goto out;
		}
	} else
		ia->ia_maddr = i;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IE507_CTRL, EL_CTRL_NORMAL);

	/*
	 * Divine memory size on-board the card.
	 */
	sc->sc_maddr = ISA_HOLE_VADDR(ia->ia_maddr);
	ie_find_mem_size(sc);

	if (!sc->sc_msize) {
		printf("%s: can't find shared memory\n", sc->sc_dev.dv_xname);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, IE507_CTRL, EL_CTRL_NRST);
		goto out;
	}

	if (!ia->ia_msize)
		ia->ia_msize = sc->sc_msize;
	else if (ia->ia_msize != sc->sc_msize) {
		printf("%s: msize mismatch; kernel configured %d != board configured %d\n",
		    sc->sc_dev.dv_xname, ia->ia_msize, sc->sc_msize);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, IE507_CTRL, EL_CTRL_NRST);
		goto out;
	}

	slel_get_address(sc);

	/* Clear the interrupt latch just in case. */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IE507_ICTRL, 1);

	rval = 1;

 out:
	bus_space_unmap(iot, ioh, 1);
	return rval;
}

/* Taken almost exactly from Rod's if_ix.c. */

int
ee16_probe(sc, ia)
	struct ie_softc *sc;
	struct isa_attach_args *ia;
{
	int i;
	u_short board_id, id_var1, id_var2, checksum = 0;
	u_short eaddrtemp, irq;
        u_short pg, adjust, decode, edecode;
	u_char	bart_config;

	short	irq_translate[] = {0, 0x09, 0x03, 0x04, 0x05, 0x0a, 0x0b, 0};

	/* Need this for part of the probe. */
	sc->reset_586 = ee16_reset_586;
	sc->chan_attn = ee16_chan_attn;

	/* reset any ee16 at the current iobase */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL, IEE16_RESET_ASIC);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL, 0);
	delay(240);

	/* now look for ee16. */
	board_id = id_var1 = id_var2 = 0;
	for (i=0; i<4 ; i++) {
		id_var1 = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IEE16_ID_PORT);
		id_var2 = ((id_var1 & 0x03) << 2);
		board_id |= (( id_var1 >> 4)  << id_var2);
	}

	if (board_id != IEE16_ID) {
#ifdef IEDEBUG
		printf("BART ID mismatch\n");
#endif
		return 0;		
	}

	/* need sc->sc_iobase for ee16_read_eeprom */
	sc->hard_type = IE_EE16;

	/*
	 * If ia->maddr == MADDRUNK, use value in eeprom location 6.
	 *
	 * The shared RAM location on the EE16 is encoded into bits
	 * 3-7 of EEPROM location 6.  We zero the upper byte, and 
	 * shift the 5 bits right 3.  The resulting number tells us
	 * the RAM location.  Because the EE16 supports either 16k or 32k
	 * of shared RAM, we only worry about the 32k locations. 
	 *
	 * NOTE: if a 64k EE16 exists, it should be added to this switch.
	 *       then the ia->ia_msize would need to be set per case statement.
	 *
	 *	value	msize	location
	 *	=====	=====	========
	 *	0x03	0x8000	0xCC000
	 *	0x06	0x8000	0xD0000
	 *	0x0C	0x8000	0xD4000
	 *	0x18	0x8000	0xD8000
	 *
	 */ 

	if ((ia->ia_maddr == MADDRUNK) || (ia->ia_msize == 0)) {
		i = ee16_read_eeprom(sc, 6) & 0xff;

#ifdef IEDEBUG
		printf("memory configuration %02x\n", i);
#endif

		switch (i & -i) {
		case 0x01:
			ia->ia_maddr = 0xc0000;
			i = ((i >> 0) | (i << 8)) & 0xff;
			break;
		case 0x02:
			ia->ia_maddr = 0xc4000;
			i = ((i >> 1) | (i << 7)) & 0xff;
			break;
		case 0x04:
			ia->ia_maddr = 0xc8000;
			i = ((i >> 2) | (i << 6)) & 0xff;
			break;
		case 0x08:
			ia->ia_maddr = 0xcc000;
			i = ((i >> 3) | (i << 5)) & 0xff;
			break;
		case 0x10:
			ia->ia_maddr = 0xd0000;
			i = ((i >> 4) | (i << 4)) & 0xff;
			break;
		case 0x20:
			ia->ia_maddr = 0xd4000;
			i = ((i >> 5) | (i << 3)) & 0xff;
			break;
		case 0x40:
			ia->ia_maddr = 0xd8000;
			i = ((i >> 6) | (i << 2)) & 0xff;
			break;
		case 0x80:
			ia->ia_maddr = 0xdc000;
			i = ((i >> 7) | (i << 1)) & 0xff;
			break;
		}

		switch (i) {
		case 0x01:
			ia->ia_msize = 16 * 1024;
			break;
		case 0x03:
			ia->ia_msize = 32 * 1024;
			break;
		case 0x07:
			ia->ia_msize = 48 * 1024;
			break;
		case 0x0f:
			ia->ia_msize = 64 * 1024;
			break;
		default:
#ifdef IEDEBUG
			printf("invalid memory size %02x\n", i);
#endif
			return 0;
		}
	}

	/* need to set these after checking for MADDRUNK */
	sc->sc_maddr = ISA_HOLE_VADDR(ia->ia_maddr);
	sc->sc_msize = ia->ia_msize; 

#ifdef IEDEBUG
	printf("found %d byte memory region at %x/%p\n", ia->ia_msize,
	    ia->ia_maddr, sc->sc_maddr);
#endif

	/* need to put the 586 in RESET, and leave it */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL, IEE16_RESET_586);

	/* read the eeprom and checksum it, should == IEE16_ID */
	for(i = 0; i < 0x40; i++)
		checksum += ee16_read_eeprom(sc, i);

	if (checksum != IEE16_ID) {
#ifdef IEDEBUG
		printf("checksum mismatch\n");
#endif
		return 0;	
	}

	/*
	 * Size and test the memory on the board.  The size of the memory
	 * can be one of 16k, 32k, 48k or 64k.  It can be located in the
	 * address range 0xC0000 to 0xEFFFF on 16k boundaries. 
	 *
	 * If the size does not match the passed in memory allocation size
	 * issue a warning, but continue with the minimum of the two sizes.
	 */
	pg = (ia->ia_maddr & 0x3C000) >> 14;
	adjust = IEE16_MCTRL_FMCS16 | (pg & 0x3) << 2;
	decode = ((1 << (ia->ia_msize / 16384)) - 1) << pg;
	edecode = ((~decode >> 4) & 0xF0) | (decode >> 8);

	/* ZZZ This should be checked against eeprom location 6, low byte */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_MEMDEC, decode & 0xFF);
	/* ZZZ This should be checked against eeprom location 1, low byte */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_MCTRL, adjust);
	/* ZZZ Now if I could find this one I would have it made */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_MPCTRL, (~decode & 0xFF));
	/* ZZZ I think this is location 6, high byte */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,+ IEE16_MECTRL,
		edecode); /*XXX disable Exxx */

	/*
	 * first prime the stupid bart DRAM controller so that it
	 * works, then zero out all of memory.
	 */
	bzero(sc->sc_maddr, 32);
	bzero(sc->sc_maddr, sc->sc_msize);

	/*
	 * Get the encoded interrupt number from the EEPROM, check it
	 * against the passed in IRQ.  Issue a warning if they do not
	 * match, and fail the probe.  If irq is 'IRQUNK' then we
	 * use the EEPROM irq, and continue.
	 */
	irq = ee16_read_eeprom(sc, IEE16_EEPROM_CONFIG1);
	irq = (irq & IEE16_EEPROM_IRQ) >> IEE16_EEPROM_IRQ_SHIFT;
	sc->irq_encoded = irq;
	irq = irq_translate[irq];
	if (ia->ia_irq != IRQUNK) {
		if (irq != ia->ia_irq) {
#ifdef IEDEBUG
			printf("\nie%d: fatal: board IRQ %d does not match kernel\n", sc->sc_dev.dv_unit, irq);
#endif
			return 0; 	/* _must_ match or probe fails */
		}
	} else
		ia->ia_irq = irq;

	/*
	 * Get the hardware ethernet address from the EEPROM and
	 * save it in the softc for use by the 586 setup code.
	 */
	eaddrtemp = ee16_read_eeprom(sc, IEE16_EEPROM_ENET_HIGH);
	sc->sc_enaddr[1] = eaddrtemp & 0xFF;
	sc->sc_enaddr[0] = eaddrtemp >> 8;
	eaddrtemp = ee16_read_eeprom(sc, IEE16_EEPROM_ENET_MID);
	sc->sc_enaddr[3] = eaddrtemp & 0xFF;
	sc->sc_enaddr[2] = eaddrtemp >> 8;
	eaddrtemp = ee16_read_eeprom(sc, IEE16_EEPROM_ENET_LOW);
	sc->sc_enaddr[5] = eaddrtemp & 0xFF;
	sc->sc_enaddr[4] = eaddrtemp >> 8;

	/* disable the board interrupts */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_IRQ, sc->irq_encoded);

	/* enable loopback to keep bad packets off the wire */
	if(sc->hard_type == IE_EE16) {
		bart_config = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IEE16_CONFIG);
		bart_config |= IEE16_BART_LOOPBACK;
		bart_config |= IEE16_BART_MCS16_TEST; /* inb doesn't get bit! */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_CONFIG, bart_config);
		bart_config = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IEE16_CONFIG);
	}

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL, 0);
	delay(100);
	if (!check_ie_present(sc, sc->sc_maddr, sc->sc_msize)) {
#ifdef IEDEBUG
		printf("check_ie_present failed\n");
#endif
		return 0;
	}

	return 1;		/* found */
}

/*
 * Taken almost exactly from Bill's if_is.c, then modified beyond recognition.
 */
void
ieattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ie_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	sc->sc_iot = ia->ia_iot;

	/* Map i/o space. */
	if (bus_space_map(sc->sc_iot, ia->ia_iobase, IE_NPORTS, 0, &sc->sc_ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = iestart;
	ifp->if_ioctl = ieioctl;
	ifp->if_watchdog = iewatchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

	printf(": address %s, type %s R%d\n",
	    ether_sprintf(sc->sc_enaddr),
	    ie_hardware_names[sc->hard_type], sc->hard_vers + 1);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB,
	    sizeof(struct ether_header));
#endif

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, ieintr, sc);

#if NRND > 0
	rnd_attach_source(&sc->rnd_source, sc->sc_dev.dv_xname, RND_TYPE_NET);
#endif
}

/*
 * Device timeout/watchdog routine.  Entered if the device neglects to generate
 * an interrupt after a transmit has been started on it.
 */
void
iewatchdog(ifp)
	struct ifnet *ifp;
{
	struct ie_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++ifp->if_oerrors;
	iereset(sc);
}

/*
 * What to do upon receipt of an interrupt.
 */
int
ieintr(arg)
	void *arg;
{
	struct ie_softc *sc = arg;
	struct ifnet *ifp = arg;
	register u_short status;

	/* Clear the interrupt latch on the 3C507. */
	if (sc->hard_type == IE_3C507)
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, IE507_ICTRL, 1);

	/* disable interrupts on the EE16. */
	if (sc->hard_type == IE_EE16)
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_IRQ, sc->irq_encoded);

	status = sc->scb->ie_status & IE_ST_WHENCE;
	if (status == 0)
		return 0;

loop:
	/* Ack interrupts FIRST in case we receive more during the ISR. */
	ie_ack(sc, status);

	if (status & (IE_ST_FR | IE_ST_RNR)) {
#ifdef IEDEBUG
		in_ierint++;
		if (sc->sc_debug & IED_RINT)
			printf("%s: rint\n", sc->sc_dev.dv_xname);
#endif
		ierint(sc);
#ifdef IEDEBUG
		in_ierint--;
#endif
	}

	if (status & IE_ST_CX) {
#ifdef IEDEBUG
		in_ietint++;
		if (sc->sc_debug & IED_TINT)
			printf("%s: tint\n", sc->sc_dev.dv_xname);
#endif
		ietint(sc);
#ifdef IEDEBUG
		in_ietint--;
#endif
	}

	if (status & IE_ST_RNR) {
		printf("%s: receiver not ready\n", sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		iereset(sc);
	}

#ifdef IEDEBUG
	if ((status & IE_ST_CNA) && (sc->sc_debug & IED_CNA))
		printf("%s: cna\n", sc->sc_dev.dv_xname);
#endif

#if NRND > 0
	if (status & (IE_ST_FR | IE_ST_CX))
		rnd_add_uint32(&sc->rnd_source, status);
#endif

	/* Clear the interrupt latch on the 3C507. */
	if (sc->hard_type == IE_3C507)
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, IE507_ICTRL, 1);

	status = sc->scb->ie_status & IE_ST_WHENCE;
	if (status == 0) {
		/* enable interrupts on the EE16. */
		if (sc->hard_type == IE_EE16)
		    bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_IRQ,
				sc->irq_encoded | IEE16_IRQ_ENABLE);
		return 1;
	}

	goto loop;
}

/*
 * Process a received-frame interrupt.
 */
void
ierint(sc)
	struct ie_softc *sc;
{
	volatile struct ie_sys_ctl_block *scb = sc->scb;
	struct ifnet *ifp;
	int i, status;
	static int timesthru = 1024;

	ifp = &sc->sc_ethercom.ec_if;

	i = sc->rfhead;
	for (;;) {
		status = sc->rframes[i]->ie_fd_status;

		if ((status & IE_FD_COMPLETE) && (status & IE_FD_OK)) {
			if (!--timesthru) {
				ifp->if_ierrors +=
				    scb->ie_err_crc + scb->ie_err_align +
				    scb->ie_err_resource + scb->ie_err_overrun;
				scb->ie_err_crc = scb->ie_err_align =
				    scb->ie_err_resource = scb->ie_err_overrun =
				    0;
				timesthru = 1024;
			}
			ie_readframe(sc, i);
		} else {
			if ((status & IE_FD_RNR) != 0 &&
			    (scb->ie_status & IE_RU_READY) == 0) {
				sc->rframes[0]->ie_fd_buf_desc =
						MK_16(MEM, sc->rbuffs[0]);
				scb->ie_recv_list = MK_16(MEM, sc->rframes[0]);
				command_and_wait(sc, IE_RU_START, 0, 0);
			}
			break;
		}
		i = (i + 1) % NFRAMES;
	}
}

/*
 * Process a command-complete interrupt.  These are only generated by the
 * transmission of frames.  This routine is deceptively simple, since most of
 * the real work is done by iestart().
 */
void
ietint(sc)
	struct ie_softc *sc;
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int status;

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	status = sc->xmit_cmds[sc->xctail]->ie_xmit_status;

	if (!(status & IE_STAT_COMPL) || (status & IE_STAT_BUSY))
		printf("ietint: command still busy!\n");

	if (status & IE_STAT_OK) {
		ifp->if_opackets++;
		ifp->if_collisions += status & IE_XS_MAXCOLL;
	} else {
		ifp->if_oerrors++;
		/*
		 * XXX
		 * Check SQE and DEFERRED?
		 * What if more than one bit is set?
		 */
		if (status & IE_STAT_ABORT)
			printf("%s: send aborted\n", sc->sc_dev.dv_xname);
		else if (status & IE_XS_LATECOLL)
			printf("%s: late collision\n", sc->sc_dev.dv_xname);
		else if (status & IE_XS_NOCARRIER)
			printf("%s: no carrier\n", sc->sc_dev.dv_xname);
		else if (status & IE_XS_LOSTCTS)
			printf("%s: lost CTS\n", sc->sc_dev.dv_xname);
		else if (status & IE_XS_UNDERRUN)
			printf("%s: DMA underrun\n", sc->sc_dev.dv_xname);
		else if (status & IE_XS_EXCMAX) {
			printf("%s: too many collisions\n", sc->sc_dev.dv_xname);
			ifp->if_collisions += 16;
		}
	}

	/*
	 * If multicast addresses were added or deleted while transmitting,
	 * mc_reset() set the want_mcsetup flag indicating that we should do
	 * it.
	 */
	if (sc->want_mcsetup) {
		mc_setup(sc, (caddr_t)sc->xmit_cbuffs[sc->xctail]);
		sc->want_mcsetup = 0;
	}

	/* Done with the buffer. */
	sc->xmit_busy--;
	sc->xctail = (sc->xctail + 1) % NTXBUF;

	/* Start the next packet, if any, transmitting. */
	if (sc->xmit_busy > 0)
		iexmit(sc);

	iestart(ifp);
}

/*
 * Compare two Ether/802 addresses for equality, inlined and unrolled for
 * speed.  I'd love to have an inline assembler version of this...
 */
static __inline int
ether_equal(one, two)
	u_char *one, *two;
{

	if (one[0] != two[0] || one[1] != two[1] || one[2] != two[2] ||
	    one[3] != two[3] || one[4] != two[4] || one[5] != two[5])
		return 0;
	return 1;
}

/*
 * Check for a valid address.  to_bpf is filled in with one of the following:
 *   0 -> BPF doesn't get this packet
 *   1 -> BPF does get this packet
 *   2 -> BPF does get this packet, but we don't
 * Return value is true if the packet is for us, and false otherwise.
 *
 * This routine is a mess, but it's also critical that it be as fast
 * as possible.  It could be made cleaner if we can assume that the
 * only client which will fiddle with IFF_PROMISC is BPF.  This is
 * probably a good assumption, but we do not make it here.  (Yet.)
 */
static __inline int
check_eh(sc, eh, to_bpf)
	struct ie_softc *sc;
	struct ether_header *eh;
	int *to_bpf;
{
	struct ifnet *ifp;
	int i;

	ifp = &sc->sc_ethercom.ec_if;

	switch (sc->promisc) {
	case IFF_ALLMULTI:
		/*
		 * Receiving all multicasts, but no unicasts except those
		 * destined for us.
		 */
#if NBPFILTER > 0
		*to_bpf = (ifp->if_bpf != 0); /* BPF gets this packet if anybody cares */
#endif
		if (eh->ether_dhost[0] & 1)
			return 1;
		if (ether_equal(eh->ether_dhost, LLADDR(ifp->if_sadl)))
			return 1;
		return 0;

	case IFF_PROMISC:
		/*
		 * Receiving all packets.  These need to be passed on to BPF.
		 */
#if NBPFILTER > 0
		*to_bpf = (ifp->if_bpf != 0);
#endif
		/* If for us, accept and hand up to BPF */
		if (ether_equal(eh->ether_dhost, LLADDR(ifp->if_sadl)))
			return 1;

#if NBPFILTER > 0
		if (*to_bpf)
			*to_bpf = 2; /* we don't need to see it */
#endif

		/*
		 * Not a multicast, so BPF wants to see it but we don't.
		 */
		if (!(eh->ether_dhost[0] & 1))
			return 1;

		/*
		 * If it's one of our multicast groups, accept it and pass it
		 * up.
		 */
		for (i = 0; i < sc->mcast_count; i++) {
			if (ether_equal(eh->ether_dhost, (u_char *)&sc->mcast_addrs[i])) {
#if NBPFILTER > 0
				if (*to_bpf)
					*to_bpf = 1;
#endif
				return 1;
			}
		}
		return 1;

	case IFF_ALLMULTI | IFF_PROMISC:
		/*
		 * Acting as a multicast router, and BPF running at the same
		 * time.  Whew!  (Hope this is a fast machine...)
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_ethercom.ec_if.if_bpf != 0);
#endif
		/* We want to see multicasts. */
		if (eh->ether_dhost[0] & 1)
			return 1;

		/* We want to see our own packets */
		if (ether_equal(eh->ether_dhost, LLADDR(ifp->if_sadl)))
			return 1;

		/* Anything else goes to BPF but nothing else. */
#if NBPFILTER > 0
		if (*to_bpf)
			*to_bpf = 2;
#endif
		return 1;

	case 0:
		/*
		 * Only accept unicast packets destined for us, or multicasts
		 * for groups that we belong to.  For now, we assume that the
		 * '586 will only return packets that we asked it for.  This
		 * isn't strictly true (it uses hashing for the multicast
		 * filter), but it will do in this case, and we want to get out
		 * of here as quickly as possible.
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_ethercom.ec_if.if_bpf != 0);
#endif
		return 1;
	}

#ifdef DIAGNOSTIC
	panic("check_eh: impossible");
#else
	return 0;			/* dump it */
#endif
}

/*
 * We want to isolate the bits that have meaning...  This assumes that
 * IE_RBUF_SIZE is an even power of two.  If somehow the act_len exceeds
 * the size of the buffer, then we are screwed anyway.
 */
static __inline int
ie_buflen(sc, head)
	struct ie_softc *sc;
	int head;
{

	return (sc->rbuffs[head]->ie_rbd_actual
	    & (IE_RBUF_SIZE | (IE_RBUF_SIZE - 1)));
}

static __inline int
ie_packet_len(sc)
	struct ie_softc *sc;
{
	int i;
	int head = sc->rbhead;
	int acc = 0;

	do {
		if (!(sc->rbuffs[sc->rbhead]->ie_rbd_actual & IE_RBD_USED))
			return -1;

		i = sc->rbuffs[head]->ie_rbd_actual & IE_RBD_LAST;

		acc += ie_buflen(sc, head);
		head = (head + 1) % NRXBUF;
	} while (!i);

	return acc;
}

/*
 * Setup all necessary artifacts for an XMIT command, and then pass the XMIT
 * command to the chip to be executed.  On the way, if we have a BPF listener
 * also give him a copy.
 */
void
iexmit(sc)
	struct ie_softc *sc;
{

#ifdef IEDEBUG
	if (sc->sc_debug & IED_XMIT)
		printf("%s: xmit buffer %d\n", sc->sc_dev.dv_xname,
		    sc->xctail);
#endif

#if NBPFILTER > 0
	/*
	 * If BPF is listening on this interface, let it see the packet before
	 * we push it on the wire.
	 */
	if (sc->sc_ethercom.ec_if.if_bpf)
		bpf_tap(sc->sc_ethercom.ec_if.if_bpf,
		    sc->xmit_cbuffs[sc->xctail],
		    sc->xmit_buffs[sc->xctail]->ie_xmit_flags);
#endif

	sc->xmit_buffs[sc->xctail]->ie_xmit_flags |= IE_XMIT_LAST;
	sc->xmit_buffs[sc->xctail]->ie_xmit_next = 0xffff;
	sc->xmit_buffs[sc->xctail]->ie_xmit_buf =
	    MK_24(MEM, sc->xmit_cbuffs[sc->xctail]);

	sc->xmit_cmds[sc->xctail]->com.ie_cmd_link = 0xffff;
	sc->xmit_cmds[sc->xctail]->com.ie_cmd_cmd =
	    IE_CMD_XMIT | IE_CMD_INTR | IE_CMD_LAST;

	sc->xmit_cmds[sc->xctail]->ie_xmit_status = 0;
	sc->xmit_cmds[sc->xctail]->ie_xmit_desc =
	    MK_16(MEM, sc->xmit_buffs[sc->xctail]);

	sc->scb->ie_command_list = MK_16(MEM, sc->xmit_cmds[sc->xctail]);
	command_and_wait(sc, IE_CU_START, 0, 0);

	sc->sc_ethercom.ec_if.if_timer = 5;
}

/*
 * Read data off the interface, and turn it into an mbuf chain.
 *
 * This code is DRAMATICALLY different from the previous version; this version
 * tries to allocate the entire mbuf chain up front, given the length of the
 * data available.  This enables us to allocate mbuf clusters in many
 * situations where before we would have had a long chain of partially-full
 * mbufs.  This should help to speed up the operation considerably.  (Provided
 * that it works, of course.)
 */
struct mbuf *
ieget(sc, ehp, to_bpf)
	struct ie_softc *sc;
	struct ether_header *ehp;
	int *to_bpf;
{
	struct mbuf *top, **mp, *m;
	int len, totlen, resid;
	int thisrboff, thismboff;
	int head;

	totlen = ie_packet_len(sc);
	if (totlen <= 0)
		return 0;

	head = sc->rbhead;

	/*
	 * Snarf the Ethernet header.
	 */
	bcopy((caddr_t)sc->cbuffs[head], (caddr_t)ehp, sizeof *ehp);

	/*
	 * As quickly as possible, check if this packet is for us.
	 * If not, don't waste a single cycle copying the rest of the
	 * packet in.
	 * This is only a consideration when FILTER is defined; i.e., when
	 * we are either running BPF or doing multicasting.
	 */
	if (!check_eh(sc, ehp, to_bpf)) {
		sc->sc_ethercom.ec_if.if_ierrors--; /* just this case, it's not an error */
		return 0;
	}

	resid = totlen -= (thisrboff = sizeof *ehp);

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return 0;
	m->m_pkthdr.rcvif = &sc->sc_ethercom.ec_if;
	m->m_pkthdr.len = totlen;
	len = MHLEN;
	top = 0;
	mp = &top;

	/*
	 * This loop goes through and allocates mbufs for all the data we will
	 * be copying in.  It does not actually do the copying yet.
	 */
	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return 0;
			}
			len = MLEN;
		}
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(top);
				return 0;
			}
			len = MCLBYTES;
		}
		m->m_len = len = min(totlen, len);
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	m = top;
	thismboff = 0;

	/*
	 * Now we take the mbuf chain (hopefully only one mbuf most of the
	 * time) and stuff the data into it.  There are no possible failures at
	 * or after this point.
	 */
	while (resid > 0) {
		int thisrblen = ie_buflen(sc, head) - thisrboff,
		    thismblen = m->m_len - thismboff;
		len = min(thisrblen, thismblen);

		bcopy((caddr_t)(sc->cbuffs[head] + thisrboff),
		    mtod(m, caddr_t) + thismboff, (u_int)len);
		resid -= len;

		if (len == thismblen) {
			m = m->m_next;
			thismboff = 0;
		} else
			thismboff += len;

		if (len == thisrblen) {
			head = (head + 1) % NRXBUF;
			thisrboff = 0;
		} else
			thisrboff += len;
	}

	/*
	 * Unless something changed strangely while we were doing the copy, we
	 * have now copied everything in from the shared memory.
	 * This means that we are done.
	 */
	return top;
}

/*
 * Read frame NUM from unit UNIT (pre-cached as IE).
 *
 * This routine reads the RFD at NUM, and copies in the buffers from the list
 * of RBD, then rotates the RBD and RFD lists so that the receiver doesn't
 * start complaining.  Trailers are DROPPED---there's no point in wasting time
 * on confusing code to deal with them.  Hopefully, this machine will never ARP
 * for trailers anyway.
 */
void
ie_readframe(sc, num)
	struct ie_softc *sc;
	int num;			/* frame number to read */
{
	int status;
	struct mbuf *m = 0;
	struct ether_header eh;
#if NBPFILTER > 0
	int bpf_gets_it = 0;
#endif

	status = sc->rframes[num]->ie_fd_status;

	/* Advance the RFD list, since we're done with this descriptor. */
	sc->rframes[num]->ie_fd_status = 0;
	sc->rframes[num]->ie_fd_last |= IE_FD_LAST;
	sc->rframes[sc->rftail]->ie_fd_last &= ~IE_FD_LAST;
	sc->rftail = (sc->rftail + 1) % NFRAMES;
	sc->rfhead = (sc->rfhead + 1) % NFRAMES;

	if (status & IE_FD_OK) {
#if NBPFILTER > 0
		m = ieget(sc, &eh, &bpf_gets_it);
#else
		m = ieget(sc, &eh, 0);
#endif
		ie_drop_packet_buffer(sc);
	}
	if (m == 0) {
		sc->sc_ethercom.ec_if.if_ierrors++;
		return;
	}

#ifdef IEDEBUG
	if (sc->sc_debug & IED_READFRAME)
		printf("%s: frame from ether %s type %x\n", sc->sc_dev.dv_xname,
		    ether_sprintf(eh.ether_shost), (u_int)eh.ether_type);
#endif

#if NBPFILTER > 0
	/*
	 * Check for a BPF filter; if so, hand it up.
	 * Note that we have to stick an extra mbuf up front, because bpf_mtap
	 * expects to have the ether header at the front.
	 * It doesn't matter that this results in an ill-formatted mbuf chain,
	 * since BPF just looks at the data.  (It doesn't try to free the mbuf,
	 * tho' it will make a copy for tcpdump.)
	 */
	if (bpf_gets_it) {
		struct mbuf m0;
		m0.m_len = sizeof eh;
		m0.m_data = (caddr_t)&eh;
		m0.m_next = m;

		/* Pass it up. */
		bpf_mtap(sc->sc_ethercom.ec_if.if_bpf, &m0);

		/*
		 * A signal passed up from the filtering code indicating that
		 * the packet is intended for BPF but not for the protocol
		 * machinery.  We can save a few cycles by not handing it off
		 * to them.
		 */
		if (bpf_gets_it == 2) {
			m_freem(m);
			return;
		}
	}
#endif /* NBPFILTER > 0 */

	/*
	 * In here there used to be code to check destination addresses upon
	 * receipt of a packet.  We have deleted that code, and replaced it
	 * with code to check the address much earlier in the cycle, before
	 * copying the data in; this saves us valuable cycles when operating
	 * as a multicast router or when using BPF.
	 */

	/*
	 * Finally pass this packet up to higher layers.
	 */
	ether_input(&sc->sc_ethercom.ec_if, &eh, m);
	sc->sc_ethercom.ec_if.if_ipackets++;
}

void
ie_drop_packet_buffer(sc)
	struct ie_softc *sc;
{
	int i;

	do {
		/*
		 * This means we are somehow out of sync.  So, we reset the
		 * adapter.
		 */
		if (!(sc->rbuffs[sc->rbhead]->ie_rbd_actual & IE_RBD_USED)) {
#ifdef IEDEBUG
			print_rbd(sc->rbuffs[sc->rbhead]);
#endif
			log(LOG_ERR, "%s: receive descriptors out of sync at %d\n",
			    sc->sc_dev.dv_xname, sc->rbhead);
			iereset(sc);
			return;
		}

		i = sc->rbuffs[sc->rbhead]->ie_rbd_actual & IE_RBD_LAST;

		sc->rbuffs[sc->rbhead]->ie_rbd_length |= IE_RBD_LAST;
		sc->rbuffs[sc->rbhead]->ie_rbd_actual = 0;
		sc->rbhead = (sc->rbhead + 1) % NRXBUF;
		sc->rbuffs[sc->rbtail]->ie_rbd_length &= ~IE_RBD_LAST;
		sc->rbtail = (sc->rbtail + 1) % NRXBUF;
	} while (!i);
}

/*
 * Start transmission on an interface.
 */
void
iestart(ifp)
	struct ifnet *ifp;
{
	struct ie_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	u_char *buffer;
	u_short len;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		if (sc->xmit_busy == NTXBUF) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == 0)
			break;

		/* We need to use m->m_pkthdr.len, so require the header */
		if ((m0->m_flags & M_PKTHDR) == 0)
			panic("iestart: no header mbuf");

#if NBPFILTER > 0
		/* Tap off here if there is a BPF listener. */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0);
#endif

#ifdef IEDEBUG
		if (sc->sc_debug & IED_ENQ)
			printf("%s: fill buffer %d\n", sc->sc_dev.dv_xname,
			    sc->xchead);
#endif

		buffer = sc->xmit_cbuffs[sc->xchead];
		for (m = m0; m != 0; m = m->m_next) {
			bcopy(mtod(m, caddr_t), buffer, m->m_len);
			buffer += m->m_len;
		}
		len = max(m0->m_pkthdr.len, ETHER_MIN_LEN);

		m_freem(m0);
		sc->xmit_buffs[sc->xchead]->ie_xmit_flags = len;

		/* Start the first packet transmitting. */
		if (sc->xmit_busy == 0)
			iexmit(sc);

		sc->xchead = (sc->xchead + 1) % NTXBUF;
		sc->xmit_busy++;
	}
}

/*
 * Check to see if there's an 82586 out there.
 */
int
check_ie_present(sc, where, size)
	struct ie_softc *sc;
	caddr_t where;
	u_int size;
{
	volatile struct ie_sys_conf_ptr *scp;
	volatile struct ie_int_sys_conf_ptr *iscp;
	volatile struct ie_sys_ctl_block *scb;
	u_long realbase;
	int s;

	s = splnet();

	realbase = (u_long)where + size - (1 << 24);

	scp = (volatile struct ie_sys_conf_ptr *)(realbase + IE_SCP_ADDR);
	bzero((char *)scp, sizeof *scp);

	/*
	 * First we put the ISCP at the bottom of memory; this tests to make
	 * sure that our idea of the size of memory is the same as the
	 * controller's.  This is NOT where the ISCP will be in normal
	 * operation.
	 */
	iscp = (volatile struct ie_int_sys_conf_ptr *)where;
	bzero((char *)iscp, sizeof *iscp);

	scb = (volatile struct ie_sys_ctl_block *)where;
	bzero((char *)scb, sizeof *scb);

	scp->ie_bus_use = 0;		/* 16-bit */
	scp->ie_iscp_ptr = (caddr_t)((volatile caddr_t)iscp -
	    (volatile caddr_t)realbase);

	iscp->ie_busy = 1;
	iscp->ie_scb_offset = MK_16(realbase, scb) + 256;

	(sc->reset_586)(sc);
	(sc->chan_attn)(sc);

	delay(100);			/* wait a while... */

	if (iscp->ie_busy) {
		splx(s);
		return 0;
	}

	/*
	 * Now relocate the ISCP to its real home, and reset the controller
	 * again.
	 */
	iscp = (void *)ALIGN(realbase + IE_SCP_ADDR - sizeof(*iscp));
	bzero((char *)iscp, sizeof *iscp);

	scp->ie_iscp_ptr = (caddr_t)((caddr_t)iscp - (caddr_t)realbase);

	iscp->ie_busy = 1;
	iscp->ie_scb_offset = MK_16(realbase, scb);

	(sc->reset_586)(sc);
	(sc->chan_attn)(sc);

	delay(100);

	if (iscp->ie_busy) {
		splx(s);
		return 0;
	}

	sc->sc_msize = size;
	sc->sc_maddr = (caddr_t)realbase;

	sc->iscp = iscp;
	sc->scb = scb;

	/*
	 * Acknowledge any interrupts we may have caused...
	 */
	ie_ack(sc, IE_ST_WHENCE);
	splx(s);

	return 1;
}

/*
 * Divine the memory size of ie board UNIT.
 * Better hope there's nothing important hiding just below the ie card...
 */
void
ie_find_mem_size(sc)
	struct ie_softc *sc;
{
	u_int size;

	sc->sc_msize = 0;

	for (size = 65536; size >= 16384; size -= 16384)
		if (check_ie_present(sc, sc->sc_maddr, size))
			return;

	return;
}

void
el_reset_586(sc)
	struct ie_softc *sc;
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IE507_CTRL, EL_CTRL_RESET);
	delay(100);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IE507_CTRL, EL_CTRL_NORMAL);
	delay(100);
}

void
sl_reset_586(sc)
	struct ie_softc *sc;
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEATT_RESET, 0);
}

void
ee16_reset_586(sc)
	struct ie_softc *sc;
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL, IEE16_RESET_586);
	delay(100);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL, 0);
	delay(100);
}

void
el_chan_attn(sc)
	struct ie_softc *sc;
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IE507_ATTN, 1);
}

void
sl_chan_attn(sc)
	struct ie_softc *sc;
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEATT_ATTN, 0);
}

void
ee16_chan_attn(sc)
	struct ie_softc *sc;
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ATTN, 0);
}

u_short
ee16_read_eeprom(sc, location)
	struct ie_softc *sc;
	int location;
{
	int ectrl, edata;

	ectrl = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL);
	ectrl &= IEE16_ECTRL_MASK;
	ectrl |= IEE16_ECTRL_EECS;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL, ectrl);

	ee16_eeprom_outbits(sc, IEE16_EEPROM_READ, IEE16_EEPROM_OPSIZE1);
	ee16_eeprom_outbits(sc, location, IEE16_EEPROM_ADDR_SIZE);
	edata = ee16_eeprom_inbits(sc);
	ectrl = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL);
	ectrl &= ~(IEE16_RESET_ASIC | IEE16_ECTRL_EEDI | IEE16_ECTRL_EECS);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL, ectrl);
	ee16_eeprom_clock(sc, 1);
	ee16_eeprom_clock(sc, 0);
	return edata;
}

void
ee16_eeprom_outbits(sc, edata, count)
	struct ie_softc *sc;
	int edata, count;
{
	int ectrl, i;

	ectrl = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL);
	ectrl &= ~IEE16_RESET_ASIC;
	for (i = count - 1; i >= 0; i--) {
		ectrl &= ~IEE16_ECTRL_EEDI;
		if (edata & (1 << i)) {
			ectrl |= IEE16_ECTRL_EEDI;
		}
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL, ectrl);
		delay(1);	/* eeprom data must be setup for 0.4 uSec */
		ee16_eeprom_clock(sc, 1);
		ee16_eeprom_clock(sc, 0);
	}
	ectrl &= ~IEE16_ECTRL_EEDI;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL, ectrl);
	delay(1);		/* eeprom data must be held for 0.4 uSec */
}

int
ee16_eeprom_inbits(sc)
	struct ie_softc *sc;
{
	int ectrl, edata, i;

	ectrl = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL);
	ectrl &= ~IEE16_RESET_ASIC;
	for (edata = 0, i = 0; i < 16; i++) {
		edata = edata << 1;
		ee16_eeprom_clock(sc, 1);
		ectrl = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL);
		if (ectrl & IEE16_ECTRL_EEDO) {
			edata |= 1;
		}
		ee16_eeprom_clock(sc, 0);
	}
	return (edata);
}

void
ee16_eeprom_clock(sc, state)
	struct ie_softc *sc;
	int state;
{
	int ectrl;

	ectrl = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL);
	ectrl &= ~(IEE16_RESET_ASIC | IEE16_ECTRL_EESK);
	if (state) {
		ectrl |= IEE16_ECTRL_EESK;
	}
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_ECTRL, ectrl);
	delay(9);		/* EESK must be stable for 8.38 uSec */
}

static inline void
ee16_interrupt_enable(sc)
	struct ie_softc *sc;
{
	delay(100);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_IRQ,
		sc->irq_encoded | IEE16_IRQ_ENABLE);
	delay(100);
}
void
slel_get_address(sc)
	struct ie_softc *sc;
{
	u_char *addr = sc->sc_enaddr;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		addr[i] = bus_space_read_1(sc->sc_iot, sc->sc_ioh, i);
}

void
iereset(sc)
	struct ie_softc *sc;
{
	int s = splnet();

	iestop(sc);

	/*
	 * Stop i82586 dead in its tracks.
	 */
	if (command_and_wait(sc, IE_RU_ABORT | IE_CU_ABORT, 0, 0))
		printf("%s: abort commands timed out\n", sc->sc_dev.dv_xname);

	if (command_and_wait(sc, IE_RU_DISABLE | IE_CU_STOP, 0, 0))
		printf("%s: disable commands timed out\n", sc->sc_dev.dv_xname);

	ieinit(sc);

	splx(s);
}

/*
 * This is called if we time out.
 */
static void
chan_attn_timeout(rock)
	void *rock;
{

	*(int *)rock = 1;
}

/*
 * Send a command to the controller and wait for it to either complete or be
 * accepted, depending on the command.  If the command pointer is null, then
 * pretend that the command is not an action command.  If the command pointer
 * is not null, and the command is an action command, wait for
 * ((volatile struct ie_cmd_common *)pcmd)->ie_cmd_status & MASK
 * to become true.
 */
static int
command_and_wait(sc, cmd, pcmd, mask)
	struct ie_softc *sc;
	int cmd;
	volatile void *pcmd;
	int mask;
{
	volatile struct ie_cmd_common *cc = pcmd;
	volatile struct ie_sys_ctl_block *scb = sc->scb;
	volatile int timedout = 0;
	extern int hz;

	scb->ie_command = (u_short)cmd;

	if (IE_ACTION_COMMAND(cmd) && pcmd) {
		(sc->chan_attn)(sc);

		/*
		 * According to the packet driver, the minimum timeout should
		 * be .369 seconds, which we round up to .4.
		 */
		timeout(chan_attn_timeout, (caddr_t)&timedout, 2 * hz / 5);

		/*
		 * Now spin-lock waiting for status.  This is not a very nice
		 * thing to do, but I haven't figured out how, or indeed if, we
		 * can put the process waiting for action to sleep.  (We may
		 * be getting called through some other timeout running in the
		 * kernel.)
		 */
		for (;;)
			if ((cc->ie_cmd_status & mask) || timedout)
				break;

		untimeout(chan_attn_timeout, (caddr_t)&timedout);

		return timedout;
	} else {
		/*
		 * Otherwise, just wait for the command to be accepted.
		 */
		(sc->chan_attn)(sc);

		while (scb->ie_command)
			;				/* spin lock */

		return 0;
	}
}

/*
 * Run the time-domain reflectometer.
 */
static void
run_tdr(sc, cmd)
	struct ie_softc *sc;
	struct ie_tdr_cmd *cmd;
{
	int result;

	cmd->com.ie_cmd_status = 0;
	cmd->com.ie_cmd_cmd = IE_CMD_TDR | IE_CMD_LAST;
	cmd->com.ie_cmd_link = 0xffff;

	sc->scb->ie_command_list = MK_16(MEM, cmd);
	cmd->ie_tdr_time = 0;

	if (command_and_wait(sc, IE_CU_START, cmd, IE_STAT_COMPL) ||
	    !(cmd->com.ie_cmd_status & IE_STAT_OK))
		result = 0x10000;
	else
		result = cmd->ie_tdr_time;

	ie_ack(sc, IE_ST_WHENCE);

	if (result & IE_TDR_SUCCESS)
		return;

	if (result & 0x10000)
		printf("%s: TDR command failed\n", sc->sc_dev.dv_xname);
	else if (result & IE_TDR_XCVR)
		printf("%s: transceiver problem\n", sc->sc_dev.dv_xname);
	else if (result & IE_TDR_OPEN)
		printf("%s: TDR detected an open %d clocks away\n",
		    sc->sc_dev.dv_xname, result & IE_TDR_TIME);
	else if (result & IE_TDR_SHORT)
		printf("%s: TDR detected a short %d clocks away\n",
		    sc->sc_dev.dv_xname, result & IE_TDR_TIME);
	else
		printf("%s: TDR returned unknown status %x\n",
		    sc->sc_dev.dv_xname, result);
}

#define	_ALLOC(p, n)	(bzero(p, n), p += n, p - n)
#define	ALLOC(p, n)	_ALLOC(p, ALIGN(n))

/*
 * Here is a helper routine for ieinit().  This sets up the buffers.
 */
void
iememinit(ptr, sc)
	void *ptr;
	struct ie_softc *sc;
{
	int i;

	/* First lay them out. */
	for (i = 0; i < NFRAMES; i++)
		sc->rframes[i] = ALLOC(ptr, sizeof(*sc->rframes[i]));

	/* Now link them together. */
	for (i = 0; i < NFRAMES; i++)
		sc->rframes[i]->ie_fd_next =
		    MK_16(MEM, sc->rframes[(i + 1) % NFRAMES]);

	/* Finally, set the EOL bit on the last one. */
	sc->rframes[NFRAMES - 1]->ie_fd_last |= IE_FD_LAST;

	/*
	 * Now lay out some buffers for the incoming frames.  Note that we set
	 * aside a bit of slop in each buffer, to make sure that we have enough
	 * space to hold a single frame in every buffer.
	 */
	for (i = 0; i < NRXBUF; i++) {
		sc->rbuffs[i] = ALLOC(ptr, sizeof(*sc->rbuffs[i]));
		sc->rbuffs[i]->ie_rbd_length = IE_RBUF_SIZE;
		sc->rbuffs[i]->ie_rbd_buffer = MK_24(MEM, ptr);
		sc->cbuffs[i] = ALLOC(ptr, IE_RBUF_SIZE);
	}

	/* Now link them together. */
	for (i = 0; i < NRXBUF; i++)
		sc->rbuffs[i]->ie_rbd_next =
		    MK_16(MEM, sc->rbuffs[(i + 1) % NRXBUF]);

	/* Tag EOF on the last one. */
	sc->rbuffs[NRXBUF - 1]->ie_rbd_length |= IE_RBD_LAST;

	/*
	 * We use the head and tail pointers on receive to keep track of the
	 * order in which RFDs and RBDs are used.
	 */
	sc->rfhead = 0;
	sc->rftail = NFRAMES - 1;
	sc->rbhead = 0;
	sc->rbtail = NRXBUF - 1;

	sc->scb->ie_recv_list = MK_16(MEM, sc->rframes[0]);
	sc->rframes[0]->ie_fd_buf_desc = MK_16(MEM, sc->rbuffs[0]);

	/*
	 * Finally, the transmit command and buffer are the last little bit of
	 * work.
	 */
	for (i = 0; i < NTXBUF; i++) {
		sc->xmit_cmds[i] = ALLOC(ptr, sizeof(*sc->xmit_cmds[i]));
		sc->xmit_buffs[i] = ALLOC(ptr, sizeof(*sc->xmit_buffs[i]));
	}

	for (i = 0; i < NTXBUF; i++)
		sc->xmit_cbuffs[i] = ALLOC(ptr, IE_TBUF_SIZE);

	/* Pointers to last packet sent and next available transmit buffer. */
	sc->xchead = sc->xctail = 0;

	/* Clear transmit-busy flag and set number of free transmit buffers. */
	sc->xmit_busy = 0;
}

/*
 * Run the multicast setup command.
 * Called at splnet().
 */
static int
mc_setup(sc, ptr)
	struct ie_softc *sc;
	void *ptr;
{
	volatile struct ie_mcast_cmd *cmd = ptr;

	cmd->com.ie_cmd_status = 0;
	cmd->com.ie_cmd_cmd = IE_CMD_MCAST | IE_CMD_LAST;
	cmd->com.ie_cmd_link = 0xffff;

	bcopy((caddr_t)sc->mcast_addrs, (caddr_t)cmd->ie_mcast_addrs,
	    sc->mcast_count * sizeof *sc->mcast_addrs);

	cmd->ie_mcast_bytes = sc->mcast_count * ETHER_ADDR_LEN; /* grrr... */

	sc->scb->ie_command_list = MK_16(MEM, cmd);
	if (command_and_wait(sc, IE_CU_START, cmd, IE_STAT_COMPL) ||
	    !(cmd->com.ie_cmd_status & IE_STAT_OK)) {
		printf("%s: multicast address setup command failed\n",
		    sc->sc_dev.dv_xname);
		return 0;
	}
	return 1;
}

/*
 * This routine takes the environment generated by check_ie_present() and adds
 * to it all the other structures we need to operate the adapter.  This
 * includes executing the CONFIGURE, IA-SETUP, and MC-SETUP commands, starting
 * the receiver unit, and clearing interrupts.
 *
 * THIS ROUTINE MUST BE CALLED AT splnet() OR HIGHER.
 */
int
ieinit(sc)
	struct ie_softc *sc;
{
	volatile struct ie_sys_ctl_block *scb = sc->scb;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	void *ptr;

	ptr = (void *)ALIGN(scb + 1);

	/*
	 * Send the configure command first.
	 */
	{
		volatile struct ie_config_cmd *cmd = ptr;

		scb->ie_command_list = MK_16(MEM, cmd);
		cmd->com.ie_cmd_status = 0;
		cmd->com.ie_cmd_cmd = IE_CMD_CONFIG | IE_CMD_LAST;
		cmd->com.ie_cmd_link = 0xffff;

		ie_setup_config(cmd, sc->promisc != 0,
		    sc->hard_type == IE_STARLAN10);

		if (command_and_wait(sc, IE_CU_START, cmd, IE_STAT_COMPL) ||
		    !(cmd->com.ie_cmd_status & IE_STAT_OK)) {
			printf("%s: configure command failed\n",
			    sc->sc_dev.dv_xname);
			return 0;
		}
	}

	/*
	 * Now send the Individual Address Setup command.
	 */
	{
		volatile struct ie_iasetup_cmd *cmd = ptr;

		scb->ie_command_list = MK_16(MEM, cmd);
		cmd->com.ie_cmd_status = 0;
		cmd->com.ie_cmd_cmd = IE_CMD_IASETUP | IE_CMD_LAST;
		cmd->com.ie_cmd_link = 0xffff;

		bcopy(LLADDR(ifp->if_sadl), (caddr_t)&cmd->ie_address,
		    sizeof cmd->ie_address);

		if (command_and_wait(sc, IE_CU_START, cmd, IE_STAT_COMPL) ||
		    !(cmd->com.ie_cmd_status & IE_STAT_OK)) {
			printf("%s: individual address setup command failed\n",
			    sc->sc_dev.dv_xname);
			return 0;
		}
	}

	/*
	 * Now run the time-domain reflectometer.
	 */
	run_tdr(sc, ptr);

	/*
	 * Acknowledge any interrupts we have generated thus far.
	 */
	ie_ack(sc, IE_ST_WHENCE);

	/*
	 * Set up the RFA.
	 */
	iememinit(ptr, sc);

	sc->sc_ethercom.ec_if.if_flags |= IFF_RUNNING;
	sc->sc_ethercom.ec_if.if_flags &= ~IFF_OACTIVE;

	sc->scb->ie_recv_list = MK_16(MEM, sc->rframes[0]);
	command_and_wait(sc, IE_RU_START, 0, 0);

	ie_ack(sc, IE_ST_WHENCE);

	/* take the ee16 out of loopback */
	{
	u_char	bart_config;

	if(sc->hard_type == IE_EE16) {
		bart_config = bus_space_read_1(sc->sc_iot, sc->sc_ioh, IEE16_CONFIG);
		bart_config &= ~IEE16_BART_LOOPBACK;
		bart_config |= IEE16_BART_MCS16_TEST; /* inb doesn't get bit! */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, IEE16_CONFIG, bart_config);
		ee16_interrupt_enable(sc); 
		ee16_chan_attn(sc);
		}
	}
	return 0;
}

void
iestop(sc)
	struct ie_softc *sc;
{

	command_and_wait(sc, IE_RU_DISABLE, 0, 0);
}

int
ieioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ie_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ieinit(sc);
			arp_ifinit(ifp, ifa);
			break;
#endif
#ifdef NS
		/* XXX - This code is probably wrong. */
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)LLADDR(ifp->if_sadl);
			else {
				bcopy(ina->x_host.c_host, LLADDR(ifp->if_sadl),
				    ETHER_ADDR_LEN);
			}
			/* Set new address. */
			ieinit(sc);
			break;
		    }
#endif /* NS */
		default:
			ieinit(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		sc->promisc = ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI);
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			iestop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			ieinit(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			iestop(sc);
			ieinit(sc);
		}
#ifdef IEDEBUG
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_debug = IED_ALL;
		else
			sc->sc_debug = 0;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ethercom):
		    ether_delmulti(ifr, &sc->sc_ethercom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			mc_reset(sc);
			error = 0;
		}
		break;

	default:
		error = EINVAL;
	}
	splx(s);
	return error;
}

static void
mc_reset(sc)
	struct ie_softc *sc;
{
	struct ether_multi *enm;
	struct ether_multistep step;
	struct ifnet *ifp;

	ifp = &sc->sc_ethercom.ec_if;

	/*
	 * Step through the list of addresses.
	 */
	sc->mcast_count = 0;
	ETHER_FIRST_MULTI(step, &sc->sc_ethercom, enm);
	while (enm) {
		if (sc->mcast_count >= MAXMCAST ||
		    bcmp(enm->enm_addrlo, enm->enm_addrhi, 6) != 0) {
			ifp->if_flags |= IFF_ALLMULTI;
			ieioctl(ifp, SIOCSIFFLAGS, (void *)0);
			goto setflag;
		}

		bcopy(enm->enm_addrlo, &sc->mcast_addrs[sc->mcast_count], 6);
		sc->mcast_count++;
		ETHER_NEXT_MULTI(step, enm);
	}
setflag:
	sc->want_mcsetup = 1;
}

#ifdef IEDEBUG
void
print_rbd(rbd)
	volatile struct ie_recv_buf_desc *rbd;
{

	printf("RBD at %08lx:\nactual %04x, next %04x, buffer %08x\n"
	    "length %04x, mbz %04x\n", (u_long)rbd, rbd->ie_rbd_actual,
	    rbd->ie_rbd_next, rbd->ie_rbd_buffer, rbd->ie_rbd_length,
	    rbd->mbz);
}
#endif

