/*-
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1992, 1993, University of Vermont and State
 *  Agricultural College.
 * Copyright (c) 1992, 1993, Garrett A. Wollman.
 *
 * Portions:
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
 *
 *	$Id: if_ie.c,v 1.11.2.1 1994/08/14 09:26:40 mycroft Exp $
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
it.  ANY OTHER command-sending routines should run at splimp(), and should
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

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>
#include <net/route.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/pio.h>

#include <i386/isa/isavar.h>
#include <i386/isa/ic/i82586.h>
#include <i386/isa/if_ieatt.h>
#include <i386/isa/if_ie507.h>
#include <i386/isa/elink.h>

static struct mbuf *last_not_for_us;

#define	IED_RINT	0x01
#define	IED_TINT	0x02
#define	IED_RNR		0x04
#define	IED_CNA		0x08
#define	IED_READFRAME	0x10
#define	IED_ALL		0x1f

#define	ETHER_MIN_LEN	64
#define	ETHER_MAX_LEN	1518
#define	ETHER_ADDR_LEN	6

/* 
sizeof(iscp) == 1+1+2+4 == 8
sizeof(scb) == 2+2+2+2+2+2+2+2 == 16
NFRAMES * sizeof(rfd) == NFRAMES*(2+2+2+2+6+6+2+2) == NFRAMES*24 == 384
sizeof(xmit_cmd) == 2+2+2+2+6+2 == 18
sizeof(transmit buffer) == 1512
sizeof(transmit buffer desc) == 8
-----
1946

NRXBUF * sizeof(rbd) == NRXBUF*(2+2+4+2+2) == NRXBUF*12
NRXBUF * IE_RBUF_SIZE == NRXBUF*256

NRXBUF should be (16384 - 1946) / (256 + 12) == 14438 / 268 == 53

With NRXBUF == 48, this leaves us 1574 bytes for another command or
more buffers.  Another transmit command would be 18+8+1512 == 1538
---just barely fits!

Obviously all these would have to be reduced for smaller memory sizes.
With a larger memory, it would be possible to roughly double the number of
both transmit and receive buffers.
*/

#define	NFRAMES		16	/* number of frames to allow for receive */
#define	NRXBUF		48	/* number of buffers to allocate */
#define	IE_RBUF_SIZE	256	/* size of each buffer, MUST BE POWER OF TWO */
#define	NTXBUF		2	/* number of transmit buffer/command pairs */
#define	IE_TBUF_SIZE	1512	/* length of transmit buffer */


enum ie_hardware {
	IE_STARLAN10,
	IE_EN100,
	IE_SLFIBER,
	IE_3C507,
	IE_UNKNOWN
};

const char *ie_hardware_names[] = {
	"StarLAN 10",
	"EN100",
	"StarLAN Fiber",
	"3C507",
	"Unknown"
};

/*
 * Ethernet status, per interface.
 */
struct ie_softc {
	struct device sc_dev;
	struct intrhand sc_ih;

	u_short sc_iobase;
	caddr_t sc_maddr;
	u_int sc_msize;

	struct arpcom sc_arpcom;

	void (*reset_586)();
	void (*chan_attn)();

	enum ie_hardware hard_type;
	int hard_vers;

	int want_mcsetup;
	int promisc;
	volatile struct ie_int_sys_conf_ptr *iscp;
	volatile struct ie_sys_ctl_block *scb;
	volatile struct ie_recv_frame_desc *rframes[NFRAMES];
	volatile struct ie_recv_buf_desc *rbuffs[NRXBUF];
	volatile char *cbuffs[NRXBUF];
	int rfhead, rftail, rbhead, rbtail;

	volatile struct ie_xmit_cmd *xmit_cmds[NTXBUF];
	volatile struct ie_xmit_buf *xmit_buffs[NTXBUF];
	int xmit_count;
	u_char *xmit_cbuffs[NTXBUF];

	struct ie_en_addr mcast_addrs[MAXMCAST + 1];
	int mcast_count;

#ifdef IEDEBUG
	int sc_debug;
#endif
};

int iewatchdog __P((/* short */));
int ieintr __P((struct ie_softc *));
int ieinit __P((struct ie_softc *sc));
int ieioctl __P((struct ifnet *ifp, int command, caddr_t data));
int iestart __P((struct ifnet *ifp));
static void el_reset_586 __P((struct ie_softc *));
static void sl_reset_586 __P((struct ie_softc *));
static void el_chan_attn __P((struct ie_softc *));
static void sl_chan_attn __P((struct ie_softc *));
void iereset __P((struct ie_softc *));
static void ie_readframe __P((struct ie_softc *sc, int bufno));
static void ie_drop_packet_buffer __P((struct ie_softc *sc));
static void slel_get_address __P((struct ie_softc *));
static void find_ie_mem_size __P((struct ie_softc *));
static int command_and_wait __P((struct ie_softc *sc, int command,
    void volatile *pcmd, int));
/*static*/ void ierint __P((struct ie_softc *sc));
/*static*/ void ietint __P((struct ie_softc *sc));
/*static*/ void iernr __P((struct ie_softc *sc));
static void start_receiver __P((struct ie_softc *sc));
static int ieget __P((struct ie_softc *, struct mbuf **,
		      struct ether_header *, int *));
static caddr_t setup_rfa __P((caddr_t ptr, struct ie_softc *sc));
static int mc_setup __P((struct ie_softc *, caddr_t));
static void mc_reset __P((struct ie_softc *sc));
#ifdef IEDEBUG
void print_rbd __P((volatile struct ie_recv_buf_desc *rbd));
int in_ierint = 0;
int in_ietint = 0;
#endif

int ieprobe();
void ieattach();

struct cfdriver iecd = {
	NULL, "ie", ieprobe, ieattach, DV_IFNET, sizeof(struct ie_softc)
};

#define MK_24(base, ptr) ((caddr_t)((u_long)ptr - (u_long)base))
#define MK_16(base, ptr) ((u_short)(u_long)MK_24(base, ptr))

#define PORT sc->sc_iobase
#define MEM sc->sc_maddr

#define bis(c, b)	do { const register u_short com_ad = (c); \
			     outb(com_ad, inb(com_ad) | (b)); } while(0)
#define bic(c, b)	do { const register u_short com_ad = (c); \
			     outb(com_ad, inb(com_ad) &~ (b)); } while(0)

/*
 * Here are a few useful functions.  We could have done these as macros,
 * but since we have the inline facility, it makes sense to use that
 * instead.
 */
static inline void
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
	cmd->ie_promisc = !!promiscuous | manchester << 2;
	cmd->ie_crs_cdt = 0;
	cmd->ie_min_len = 64;
	cmd->ie_junk = 0xff;
}

static inline caddr_t
Align(ptr)
      caddr_t ptr;
{
	u_long l = (u_long)ptr;

	l = (l + 3) & ~3L;
	return (caddr_t)l;
}

static inline void
ie_ack(sc, mask)
	struct ie_softc *sc;
	u_int mask;
{
	volatile struct ie_sys_ctl_block *scb = sc->scb;

	scb->ie_command = scb->ie_status & mask;
	(sc->chan_attn)(sc);
}

int
ieprobe(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ie_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;

	if (sl_probe(sc, ia))
		return 1;
	if (el_probe(sc, ia))
		return 1;
	return 0;
}

int
sl_probe(sc, ia)
	struct ie_softc *sc;
	struct isa_attach_args *ia;
{
	u_char c;

	sc->sc_iobase = ia->ia_iobase;
	sc->sc_maddr = ia->ia_maddr;

	/* Need this for part of the probe. */
	sc->reset_586 = sl_reset_586;
	sc->chan_attn = sl_chan_attn;

	c = inb(PORT + IEATT_REVISION);
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
		printf("%s: unknown AT&T board type code %d\n",
		    sc->sc_dev.dv_xname, SL_BOARD(c));
		return 0;
	}

	sc->hard_vers = SL_REV(c);

	/*
	 * Divine memory size on-board the card.  Ususally 16k.
	 */
	find_ie_mem_size(sc);

	if (!sc->sc_msize) {
		printf("%s: can't find shared memory\n", sc->sc_dev.dv_xname);
		return 0;
	}

	if (!ia->ia_msize)
		ia->ia_msize = sc->sc_msize;
	else if (ia->ia_msize != sc->sc_msize) {
		printf("%s: kernel configured msize %d doesn't match board configured msize %d\n",
		    sc->sc_dev.dv_xname, ia->ia_msize, sc->sc_msize);
		return 0;
	}

	slel_get_address(sc);

	ia->ia_iosize = 16;
	return 1;
}

int
el_probe(sc, ia)
	struct ie_softc *sc;
	struct isa_attach_args *ia;
{
	u_char c;
	int i;
	u_char signature[] = "*3COM*";

	sc->sc_iobase = ia->ia_iobase;
	sc->sc_maddr = ia->ia_maddr;

	/* Need this for part of the probe. */
	sc->reset_586 = el_reset_586;
	sc->chan_attn = el_chan_attn;

	/* Reset and put card in CONFIG state without changing address. */
	elink_reset();
	outb(ELINK_ID_PORT, 0x00);
	elink_idseq(ELINK_507_POLY);
	elink_idseq(ELINK_507_POLY);
	outb(ELINK_ID_PORT, 0xff);

	/* Check for 3COM signature before proceeding. */
	outb(PORT + IE507_CTRL, inb(PORT + IE507_CTRL) & 0xfc);	/* XXX */
	for (i = 0; i < 6; i++)
		if (inb(PORT + i) != signature[i])
			return 0;
	
	c = inb(PORT + IE507_MADDR);
	if (c & 0x20) {
		printf("%s: can't map 3C507 RAM in high memory\n",
		    sc->sc_dev.dv_xname);
		return 0;
	}

	/* go to RUN state */
	outb(ELINK_ID_PORT, 0x00);
	elink_idseq(ELINK_507_POLY);
	outb(ELINK_ID_PORT, 0x00);

	outb(PORT + IE507_CTRL, EL_CTRL_NRST);

	c = inb(PORT + IE507_IRQ) & 0x0f;

	if (ia->ia_irq != (1 << c)) {
		printf("%s: kernel configured irq %d doesn't match board configured irq %d\n",
		    sc->sc_dev.dv_xname, ffs(ia->ia_irq) - 1, c);
		return 0;
	}

	c = (inb(PORT + IE507_MADDR) & 0x1c) + 0xc0;

	if (kvtop(ia->ia_maddr) != ((int)c << 12)) {
		printf("%s: kernel configured maddr %x doesn't match board configured maddr %x\n",
		    sc->sc_dev.dv_xname, kvtop(ia->ia_maddr),
		    (int)c << 12);
		return 0;
	}

	outb(PORT + IE507_CTRL, EL_CTRL_NORMAL);

	sc->hard_type = IE_3C507;
	sc->hard_vers = 0;	/* 3C507 has no version number. */

	/*
	 * Divine memory size on-board the card.
	 */
	find_ie_mem_size(sc);

	if (!sc->sc_msize) {
		printf("%s: can't find shared memory\n", sc->sc_dev.dv_xname);
		outb(PORT + IE507_CTRL, EL_CTRL_NRST);
		return 0;
	}

	if (!ia->ia_msize)
		ia->ia_msize = sc->sc_msize;
	else if (ia->ia_msize != sc->sc_msize) {
		printf("%s: kernel configured msize %d doesn't match board configured msize %d\n",
		    sc->sc_dev.dv_xname, ia->ia_msize, sc->sc_msize);
		outb(PORT + IE507_CTRL, EL_CTRL_NRST);
		return 0;
	}

	slel_get_address(sc);

	/* Clear the interrupt latch just in case. */
	outb(PORT + IE507_ICTRL, 1);

	ia->ia_iosize = 16;
	return 1;
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
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	ifp->if_unit = sc->sc_dev.dv_unit;
	ifp->if_name = iecd.cd_name;
	ifp->if_output = ether_output;
	ifp->if_start = iestart;
	ifp->if_ioctl = ieioctl;
	ifp->if_watchdog = iewatchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

	printf(": address %s, type %s R%d\n",
	    ether_sprintf(sc->sc_arpcom.ac_enaddr),
	    ie_hardware_names[sc->hard_type], sc->hard_vers + 1);

#if NBPFILTER > 0
	bpfattach(&sc->sc_arpcom.ac_if.if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	sc->sc_ih.ih_fun = ieintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_level = IPL_NET;
	intr_establish(ia->ia_irq, &sc->sc_ih);
}

/*
 * Device timeout/watchdog routine.  Entered if the device neglects to generate
 * an interrupt after a transmit has been started on it.
 */
int
iewatchdog(unit)
	short unit;
{
	struct ie_softc *sc = iecd.cd_devs[unit];

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	iereset(sc);
}

/*
 * What to do upon receipt of an interrupt.
 */
int
ieintr(sc)
	struct ie_softc *sc;
{
	register u_short status;

	status = sc->scb->ie_status;

	if ((status & IE_ST_WHENCE) == 0) {
		/* Clear the interrupt latch on the 3C507. */
		if (sc->hard_type == IE_3C507 &&
		    (inb(PORT + IE507_CTRL) & EL_CTRL_INTL))
			outb(PORT + IE507_ICTRL, 1);
		return 0;
	}

loop:
	if (status & (IE_ST_RECV | IE_ST_RNR)) {
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

	if (status & IE_ST_DONE) {
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
#ifdef IEDEBUG
		if (sc->sc_debug & IED_RNR)
			printf("%s: rnr\n", sc->sc_dev.dv_xname);
#endif
		iernr(sc);
	}

#ifdef IEDEBUG
	if ((status & IE_ST_ALLDONE) && (sc->sc_debug & IED_CNA))
		printf("%s: cna\n", sc->sc_dev.dv_xname);
#endif

	/* Don't ack interrupts which we didn't receive */
	ie_ack(sc, IE_ST_WHENCE & status);

	if ((status = sc->scb->ie_status) & IE_ST_WHENCE)
		goto loop;

	/* Clear the interrupt latch on the 3C507. */
	if (sc->hard_type == IE_3C507)
		outb(PORT + IE507_ICTRL, 1);

	return 1;
}

/*
 * Process a received-frame interrupt.
 */
void
ierint(sc)
	struct ie_softc *sc;
{
	volatile struct ie_sys_ctl_block *scb = sc->scb;
	int i, status;
	static int timesthru = 1024;

	i = sc->rfhead;
	for (;;) {
		status = sc->rframes[i]->ie_fd_status;
	
		if ((status & IE_FD_COMPLETE) && (status & IE_FD_OK)) {
			sc->sc_arpcom.ac_if.if_ipackets++;
			if (!--timesthru) {
				sc->sc_arpcom.ac_if.if_ierrors +=
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
				sc->rframes[0]->ie_fd_next = MK_16(MEM,
				    sc->rbuffs[0]);
				scb->ie_recv_list = MK_16(MEM, sc->rframes[0]);
				command_and_wait(sc, IE_RU_START, 0, 0);
			}
			break;
		}
		i = (i + 1) % NFRAMES;
	}
}

/*
 * Process a command-complete interrupt.  These are only generated by
 * the transmission of frames.  This routine is deceptively simple, since
 * most of the real work is done by iestart().
 */
void
ietint(sc)
	struct ie_softc *sc;
{
	int status;
	int i;

	sc->sc_arpcom.ac_if.if_timer = 0;
	sc->sc_arpcom.ac_if.if_flags &= ~IFF_OACTIVE;

	for (i = 0; i < sc->xmit_count; i++) {
		status = sc->xmit_cmds[i]->ie_xmit_status;
	
		if (status & IE_XS_LATECOLL) {
			printf("%s: late collision\n", sc->sc_dev.dv_xname);
			sc->sc_arpcom.ac_if.if_collisions++;
			sc->sc_arpcom.ac_if.if_oerrors++;
		} else if (status & IE_XS_NOCARRIER) {
			printf("%s: no carrier\n", sc->sc_dev.dv_xname);
			sc->sc_arpcom.ac_if.if_oerrors++;
		} else if (status & IE_XS_LOSTCTS) {
			printf("%s: lost CTS\n", sc->sc_dev.dv_xname);
			sc->sc_arpcom.ac_if.if_oerrors++;
		} else if (status & IE_XS_UNDERRUN) {
			printf("%s: DMA underrun\n", sc->sc_dev.dv_xname);
			sc->sc_arpcom.ac_if.if_oerrors++;
		} else if (status & IE_XS_EXCMAX) {
			printf("%s: too many collisions\n", sc->sc_dev.dv_xname);
			sc->sc_arpcom.ac_if.if_collisions += 16;
			sc->sc_arpcom.ac_if.if_oerrors++;
		} else {
			sc->sc_arpcom.ac_if.if_opackets++;
			sc->sc_arpcom.ac_if.if_collisions += status & IE_XS_MAXCOLL;
		}
	}
	sc->xmit_count = 0;

	/*
	 * If multicast addresses were added or deleted while we were transmitting,
	 * mc_reset() set the want_mcsetup flag indicating that we should do it.
	 */
	if (sc->want_mcsetup) {
		mc_setup(sc, (caddr_t)sc->xmit_cbuffs[0]);
		sc->want_mcsetup = 0;
	}

	/* Wish I knew why this seems to be necessary... */
	sc->xmit_cmds[0]->ie_xmit_status |= IE_STAT_COMPL;

	iestart(&sc->sc_arpcom.ac_if);
}

/*
 * Process a receiver-not-ready interrupt.  I believe that we get these
 * when there aren't enough buffers to go around.  For now (FIXME), we
 * just restart the receiver, and hope everything's ok.
 */
void
iernr(sc)
	struct ie_softc *sc;
{

	command_and_wait(sc, IE_RU_DISABLE, 0, 0); /* just in case */
	setup_rfa((caddr_t)sc->rframes[0], sc);

	sc->scb->ie_recv_list = MK_16(MEM, sc->rframes[0]);
	command_and_wait(sc, IE_RU_START, 0, 0); /* was ENABLE */

	ie_ack(sc, IE_ST_WHENCE);

	sc->sc_arpcom.ac_if.if_ierrors++;
}

/*
 * Compare two Ether/802 addresses for equality, inlined and
 * unrolled for speed.  I'd love to have an inline assembler
 * version of this...
 */
static inline int
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
static inline int
check_eh(sc, eh, to_bpf)
	struct ie_softc *sc; 
	struct ether_header *eh;
	int *to_bpf;
{
	int i;

	switch(sc->promisc) {
	case IFF_ALLMULTI:
		/*
		 * Receiving all multicasts, but no unicasts except those destined for us.
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.if_bpf != 0); /* BPF gets this packet if anybody cares */
#endif
		if (eh->ether_dhost[0] & 1)
			return 1;
		if (ether_equal(eh->ether_dhost, sc->sc_arpcom.ac_enaddr)) return 1;
		return 0;
	
	case IFF_PROMISC:
		/*
		 * Receiving all packets.  These need to be passed on to BPF.
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.if_bpf != 0);
#endif
		/* If for us, accept and hand up to BPF */
		if (ether_equal(eh->ether_dhost, sc->sc_arpcom.ac_enaddr)) return 1;
	
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
		 * Acting as a multicast router, and BPF running at the same time.
		 * Whew!  (Hope this is a fast machine...)
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.if_bpf != 0);
#endif
		/* We want to see multicasts. */
		if (eh->ether_dhost[0] & 1)
			return 1;
	
		/* We want to see our own packets */
		if (ether_equal(eh->ether_dhost, sc->sc_arpcom.ac_enaddr)) return 1;
	
		/* Anything else goes to BPF but nothing else. */
#if NBPFILTER > 0
		if (*to_bpf)
			*to_bpf = 2;
#endif
		return 1;
	
	default:
		/*
		 * Only accept unicast packets destined for us, or multicasts
		 * for groups that we belong to.  For now, we assume that the
		 * '586 will only return packets that we asked it for.  This
		 * isn't strictly true (it uses hashing for the multicast filter),
		 * but it will do in this case, and we want to get out of here
		 * as quickly as possible.
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.if_bpf != 0);
#endif
		return 1;
	}
	return 0;
}

/*
 * We want to isolate the bits that have meaning...  This assumes that
 * IE_RBUF_SIZE is an even power of two.  If somehow the act_len exceeds
 * the size of the buffer, then we are screwed anyway.
 */
static inline int
ie_buflen(sc, head)
	struct ie_softc *sc;
	int head;
{

	return (sc->rbuffs[head]->ie_rbd_actual
	    & (IE_RBUF_SIZE | (IE_RBUF_SIZE - 1)));
}

static inline int
ie_packet_len(sc)
	struct ie_softc *sc;
{
	int i;
	int head = sc->rbhead;
	int acc = 0;

	do {
		if (!(sc->rbuffs[sc->rbhead]->ie_rbd_actual & IE_RBD_USED)) {
#ifdef IEDEBUG
			print_rbd(sc->rbuffs[sc->rbhead]);
#endif
			log(LOG_ERR, "%s: receive descriptors out of sync at %d\n",
			    sc->sc_dev.dv_xname, sc->rbhead);
			iereset(sc);
			return -1;
		}
	
		i = sc->rbuffs[head]->ie_rbd_actual & IE_RBD_LAST;
	
		acc += ie_buflen(sc, head);
		head = (head + 1) % NRXBUF;
	} while (!i);

	return acc;
}

/*
 * Read data off the interface, and turn it into an mbuf chain.
 *
 * This code is DRAMATICALLY different from the previous version; this
 * version tries to allocate the entire mbuf chain up front, given the
 * length of the data available.  This enables us to allocate mbuf
 * clusters in many situations where before we would have had a long
 * chain of partially-full mbufs.  This should help to speed up the
 * operation considerably.  (Provided that it works, of course.)
 */
static inline int
ieget(sc, mp, ehp, to_bpf)
	struct ie_softc *sc;
	struct mbuf **mp;
	struct ether_header *ehp;
	int *to_bpf;
{
	struct mbuf *m, *top, **mymp;
	int i;
	int offset;
	int totlen, resid;
	int thismboff;
	int head;

	totlen = ie_packet_len(sc);
	if (totlen <= 0)
		return -1;

	i = sc->rbhead;

	/*
	 * Snarf the Ethernet header.
	 */
	bcopy((caddr_t)sc->cbuffs[i], (caddr_t)ehp, sizeof *ehp);

	/*
	 * As quickly as possible, check if this packet is for us.
	 * If not, don't waste a single cycle copying the rest of the
	 * packet in.
	 * This is only a consideration when FILTER is defined; i.e., when
	 * we are either running BPF or doing multicasting.
	 */
	if (!check_eh(sc, ehp, to_bpf)) {
		ie_drop_packet_buffer(sc);
		sc->sc_arpcom.ac_if.if_ierrors--; /* just this case, it's not an error */
		return -1;
	}
	totlen -= (offset = sizeof *ehp);

	MGETHDR(*mp, M_DONTWAIT, MT_DATA);
	if (!*mp) {
		ie_drop_packet_buffer(sc);
		return -1;
	}

	m = *mp;
	m->m_pkthdr.rcvif = &sc->sc_arpcom.ac_if;
	m->m_len = MHLEN;
	resid = m->m_pkthdr.len = totlen;
	top = 0;
	mymp = &top;

	/*
	 * This loop goes through and allocates mbufs for all the data we will
	 * be copying in.  It does not actually do the copying yet.
	 */
	do {				/* while (resid > 0) */
		/*
		 * Try to allocate an mbuf to hold the data that we have.  If we 
		 * already allocated one, just get another one and stick it on the
		 * end (eventually).  If we don't already have one, try to allocate
		 * an mbuf cluster big enough to hold the whole packet, if we think it's
		 * reasonable, or a single mbuf which may or may not be big enough.
		 * Got that?
		 */
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (!m) {
				m_freem(top);
				ie_drop_packet_buffer(sc);
				return -1;
			}
			m->m_len = MLEN;
		}
	
		if (resid >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				m->m_len = min(resid, MCLBYTES);
		} else {
			if (resid < m->m_len) {
				if (!top && resid + max_linkhdr <= m->m_len)
					m->m_data += max_linkhdr;
				m->m_len = resid;
			}
		}
		resid -= m->m_len;
		*mymp = m;
		mymp = &m->m_next;
	} while (resid > 0);

	resid = totlen;
	m = top;
	thismboff = 0;
	head = sc->rbhead;

	/*
	 * Now we take the mbuf chain (hopefully only one mbuf most of the
	 * time) and stuff the data into it.  There are no possible failures
	 * at or after this point.
	 */
	while (resid > 0) {		/* while there's stuff left */
		int thislen = ie_buflen(sc, head) - offset;
	
		/*
		 * If too much data for the current mbuf, then fill the current one
		 * up, go to the next one, and try again.
		 */
		if (thislen > m->m_len - thismboff) {
			int newlen = m->m_len - thismboff;
			bcopy((caddr_t)(sc->cbuffs[head] + offset),
			    mtod(m, caddr_t) + thismboff, (u_int)newlen);
			m = m->m_next;
			thismboff = 0;		/* new mbuf, so no offset */
			offset += newlen;	/* we are now this far into the packet */
			resid -= newlen;	/* so there is this much left to get */
			continue;
		}
	
		/*
		 * If there is more than enough space in the mbuf to hold the
		 * contents of this buffer, copy everything in, advance pointers,
		 * and so on.
		 */
		if (thislen < m->m_len - thismboff) {
			bcopy((caddr_t)(sc->cbuffs[head] + offset),
			    mtod(m, caddr_t) + thismboff, (u_int)thislen);
			thismboff += thislen;	/* we are this far into the mbuf */
			resid -= thislen;	/* and this much is left */
			goto nextbuf;
		}
	
		/*
		 * Otherwise, there is exactly enough space to put this buffer's
		 * contents into the current mbuf.  Do the combination of the above
		 * actions.
		 */
		bcopy((caddr_t)(sc->cbuffs[head] + offset),
		    mtod(m, caddr_t) + thismboff, (u_int)thislen);
		m = m->m_next;
		thismboff = 0;		/* new mbuf, start at the beginning */
		resid -= thislen;		/* and we are this far through */
	
		/*
		 * Advance all the pointers.  We can get here from either of the
		 * last two cases, but never the first.
		 */
	nextbuf:
		offset = 0;
		sc->rbuffs[head]->ie_rbd_actual = 0;
		sc->rbuffs[head]->ie_rbd_length |= IE_RBD_LAST;
		sc->rbhead = head = (head + 1) % NRXBUF;
		sc->rbuffs[sc->rbtail]->ie_rbd_length &= ~IE_RBD_LAST;
		sc->rbtail = (sc->rbtail + 1) % NRXBUF;
	}

	/*
	 * Unless something changed strangely while we were doing the copy,
	 * we have now copied everything in from the shared memory.
	 * This means that we are done.
	 */
	return 0;
}

/*
 * Read frame NUM from unit UNIT (pre-cached as IE).
 *
 * This routine reads the RFD at NUM, and copies in the buffers from
 * the list of RBD, then rotates the RBD and RFD lists so that the receiver
 * doesn't start complaining.  Trailers are DROPPED---there's no point
 * in wasting time on confusing code to deal with them.  Hopefully,
 * this machine will never ARP for trailers anyway.
 */
static void
ie_readframe(sc, num)
	struct ie_softc *sc;
	int num;			/* frame number to read */
{
	struct ie_recv_frame_desc rfd;
	struct mbuf *m = 0;
	struct ether_header eh;
#if NBPFILTER > 0
	int bpf_gets_it = 0;
#endif

	bcopy((caddr_t)(sc->rframes[num]), &rfd,
	    sizeof(struct ie_recv_frame_desc));

	/* Immediately advance the RFD list, since we have copied ours now. */
	sc->rframes[num]->ie_fd_status = 0;
	sc->rframes[num]->ie_fd_last |= IE_FD_LAST;
	sc->rframes[sc->rftail]->ie_fd_last &= ~IE_FD_LAST;
	sc->rftail = (sc->rftail + 1) % NFRAMES;
	sc->rfhead = (sc->rfhead + 1) % NFRAMES;

	if (rfd.ie_fd_status & IE_FD_OK) {
#if NBPFILTER > 0
		if (ieget(sc, &m, &eh, &bpf_gets_it)) {
#else
		if (ieget(sc, &m, &eh, 0)) {
#endif
			sc->sc_arpcom.ac_if.if_ierrors++;
			return;
		}
	}

#ifdef IEDEBUG
	if (sc->sc_debug & IED_READFRAME)
		printf("%s: frame from ether %s type %x\n", sc->sc_dev.dv_xname,
		    ether_sprintf(eh.ether_shost), (u_int)eh.ether_type);
#endif

	if (!m)
		return;

	if (last_not_for_us) {
		m_freem(last_not_for_us);
		last_not_for_us = 0;
	}

#if NBPFILTER > 0
	/*
	 * Check for a BPF filter; if so, hand it up.
	 * Note that we have to stick an extra mbuf up front, because
	 * bpf_mtap expects to have the ether header at the front.
	 * It doesn't matter that this results in an ill-formatted mbuf chain,
	 * since BPF just looks at the data.  (It doesn't try to free the mbuf,
	 * tho' it will make a copy for tcpdump.)
	 */
	if (bpf_gets_it) {
		struct mbuf m0;
		m0.m_len = sizeof eh;
		m0.m_data = (caddr_t)&eh;
		m0.m_next = m;
	
		/* Pass it up */
		bpf_mtap(sc->sc_arpcom.ac_if.if_bpf, &m0);
	}
	/*
	 * A signal passed up from the filtering code indicating that the
	 * packet is intended for BPF but not for the protocol machinery.
	 * We can save a few cycles by not handing it off to them.
	 */
	if (bpf_gets_it == 2) {
		last_not_for_us = m;
		return;
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
	ether_input(&sc->sc_arpcom.ac_if, &eh, m);
}

static void
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
int
iestart(ifp)
	struct ifnet *ifp;
{
	struct ie_softc *sc = iecd.cd_devs[ifp->if_unit];
	struct mbuf *m0, *m;
	u_char *buffer;
	u_short len;
	/* This is not really volatile, in this routine, but it makes gcc happy. */
	volatile u_short *bptr = &sc->scb->ie_command_list;
	
	if ((ifp->if_flags ^ IFF_RUNNING) & (IFF_RUNNING | IFF_OACTIVE))
		return 0;
	
	do {
		IF_DEQUEUE(&sc->sc_arpcom.ac_if.if_snd, m);
		if (!m)
			break;
		
		buffer = sc->xmit_cbuffs[sc->xmit_count];
		len = 0;
		
		for (m0 = m; m && len < IE_TBUF_SIZE; m = m->m_next) {
			bcopy(mtod(m, caddr_t), buffer, m->m_len);
			buffer += m->m_len;
			len += m->m_len;
		}
		
		m_freem(m0);
		len = max(len, ETHER_MIN_LEN);
		
#if NBPFILTER > 0
		/*
		 * See if bpf is listening on this interface, let it see the packet
		 * before we commit it to the wire.
		 */
		if (sc->sc_arpcom.ac_if.if_bpf)
			bpf_tap(sc->sc_arpcom.ac_if.if_bpf, sc->xmit_cbuffs[sc->xmit_count],
			    len);
#endif
		
		sc->xmit_buffs[sc->xmit_count]->ie_xmit_flags = IE_XMIT_LAST | len;
		sc->xmit_buffs[sc->xmit_count]->ie_xmit_next = 0xffff;
		sc->xmit_buffs[sc->xmit_count]->ie_xmit_buf = 
			MK_24(MEM, sc->xmit_cbuffs[sc->xmit_count]);
		
		sc->xmit_cmds[sc->xmit_count]->com.ie_cmd_cmd = IE_CMD_XMIT;
		sc->xmit_cmds[sc->xmit_count]->ie_xmit_status = 0;
		sc->xmit_cmds[sc->xmit_count]->ie_xmit_desc = 
			MK_16(MEM, sc->xmit_buffs[sc->xmit_count]);
		
		*bptr = MK_16(MEM, sc->xmit_cmds[sc->xmit_count]);
		bptr = &sc->xmit_cmds[sc->xmit_count]->com.ie_cmd_link;
	} while (++sc->xmit_count < NTXBUF);
	
	/*
	 * If we queued up anything for transmission, send it.
	 */
	if (sc->xmit_count) {
		sc->xmit_cmds[sc->xmit_count - 1]->com.ie_cmd_cmd |= 
		    IE_CMD_LAST | IE_CMD_INTR;
		
		/*
		 * By passing the command pointer as a null, we tell
		 * command_and_wait() to pretend that this isn't an action
		 * command.  I wish I understood what was happening here.
		 */
		command_and_wait(sc, IE_CU_START, 0, 0);
		ifp->if_flags |= IFF_OACTIVE;
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
	
	s = splimp();
	
	realbase = (u_long)where + size - (1 << 24);
	
	scp = (volatile struct ie_sys_conf_ptr *)(realbase + IE_SCP_ADDR);
	bzero((char *)scp, sizeof *scp);
	
	/*
	 * First we put the ISCP at the bottom of memory; this tests to make
	 * sure that our idea of the size of memory is the same as the controller's.
	 * This is NOT where the ISCP will be in normal operation.
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
	iscp = (void *)Align((caddr_t)(realbase + IE_SCP_ADDR - 
	    sizeof(struct ie_int_sys_conf_ptr)));
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
static void
find_ie_mem_size(sc)
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

	outb(PORT + IE507_CTRL, EL_CTRL_RESET);
	delay(100);
	outb(PORT + IE507_CTRL, EL_CTRL_NORMAL);
	delay(100);
}

void
sl_reset_586(sc)
	struct ie_softc *sc;
{

	outb(PORT + IEATT_RESET, 0);
}

void
el_chan_attn(sc)
	struct ie_softc *sc;
{

	outb(PORT + IE507_ATTN, 1);
}

void
sl_chan_attn(sc)
	struct ie_softc *sc;
{

	outb(PORT + IEATT_ATTN, 0);
}

void
slel_get_address(sc)
	struct ie_softc *sc;
{
	u_char *addr = sc->sc_arpcom.ac_enaddr;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		addr[i] = inb(PORT + i);
}

void
iereset(sc)
	struct ie_softc *sc;
{
	int s = splimp();
	
	printf("%s: reset\n", sc->sc_dev.dv_xname);
	sc->sc_arpcom.ac_if.if_flags &= ~IFF_UP;
	ieioctl(&sc->sc_arpcom.ac_if, SIOCSIFFLAGS, 0);
	
	/*
	 * Stop i82586 dead in its tracks.
	 */
	if (command_and_wait(sc, IE_RU_ABORT | IE_CU_ABORT, 0, 0))
		printf("%s: abort commands timed out\n", sc->sc_dev.dv_xname);
	
	if (command_and_wait(sc, IE_RU_DISABLE | IE_CU_STOP, 0, 0))
		printf("%s: disable commands timed out\n", sc->sc_dev.dv_xname);
	
#ifdef notdef
	if (!check_ie_present(sc, sc->sc_maddr, sc->sc_msize))
		panic("ie disappeared!\n");
#endif
	
	sc->sc_arpcom.ac_if.if_flags |= IFF_UP;
	ieioctl(&sc->sc_arpcom.ac_if, SIOCSIFFLAGS, 0);
	
	splx(s);
}

/*
 * This is called if we time out.
 */
static void
chan_attn_timeout(rock)
	caddr_t rock;
{

	*(int *)rock = 1;
}

/*
 * Send a command to the controller and wait for it to either
 * complete or be accepted, depending on the command.  If the
 * command pointer is null, then pretend that the command is
 * not an action command.  If the command pointer is not null,
 * and the command is an action command, wait for
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
		 * According to the packet driver, the minimum timeout should be
		 * .369 seconds, which we round up to .4.
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
 * Run the time-domain reflectometer...
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
	cmd->ie_tdr_time = 0;
	
	sc->scb->ie_command_list = MK_16(MEM, cmd);
	cmd->ie_tdr_time = 0;
	
	if (command_and_wait(sc, IE_CU_START, cmd, IE_STAT_COMPL))
		result = 0x2000;
	else
		result = cmd->ie_tdr_time;
	
	ie_ack(sc, IE_ST_WHENCE);
	
	if (result & IE_TDR_SUCCESS)
		return;
	
	if (result & IE_TDR_XCVR) {
		printf("%s: transceiver problem\n", sc->sc_dev.dv_xname);
	} else if (result & IE_TDR_OPEN) {
		printf("%s: TDR detected an open %d clocks away\n",
		       sc->sc_dev.dv_xname, result & IE_TDR_TIME);
	} else if (result & IE_TDR_SHORT) {
		printf("%s: TDR detected a short %d clocks away\n",
		       sc->sc_dev.dv_xname, result & IE_TDR_TIME);
	} else {
		printf("%s: TDR returned unknown status %x\n",
		       sc->sc_dev.dv_xname, result);
	}
}

static void
start_receiver(sc)
	struct ie_softc *sc;
{
	int s = splimp();
	
	sc->scb->ie_recv_list = MK_16(MEM, sc->rframes[0]);
	command_and_wait(sc, IE_RU_START, 0, 0);
	
	ie_ack(sc, IE_ST_WHENCE);
	
	splx(s);
}

/*
 * Here is a helper routine for iernr() and ieinit().  This sets up
 * the RFA.
 */
static caddr_t
setup_rfa(ptr, sc)
	caddr_t ptr;
	struct ie_softc *sc;
{
	volatile struct ie_recv_frame_desc *rfd = (void *)ptr;
	volatile struct ie_recv_buf_desc *rbd;
	int i;
	
	/* First lay them out */
	for (i = 0; i < NFRAMES; i++) {
		sc->rframes[i] = rfd;
		bzero((char *)rfd, sizeof *rfd);
		rfd++;
	}
	
	ptr = (caddr_t)Align((caddr_t)rfd);
	
	/* Now link them together */
	for (i = 0; i < NFRAMES; i++)
		sc->rframes[i]->ie_fd_next =
		    MK_16(MEM, sc->rframes[(i + 1) % NFRAMES]);
	
	/* Finally, set the EOL bit on the last one. */
	sc->rframes[NFRAMES - 1]->ie_fd_last |= IE_FD_LAST;
	
	/*
	 * Now lay out some buffers for the incoming frames.  Note that
	 * we set aside a bit of slop in each buffer, to make sure that
	 * we have enough space to hold a single frame in every buffer.
	 */
	rbd = (void *)ptr;
	
	for (i = 0; i < NRXBUF; i++) {
		sc->rbuffs[i] = rbd;
		bzero((char *)rbd, sizeof *rbd);
		ptr = (caddr_t)Align(ptr + sizeof *rbd);
		rbd->ie_rbd_length = IE_RBUF_SIZE;
		rbd->ie_rbd_buffer = MK_24(MEM, ptr);
		sc->cbuffs[i] =  (void *)ptr;
		ptr += IE_RBUF_SIZE;
		rbd = (void *)ptr;
	}
	
	/* Now link them together */
	for (i = 0; i < NRXBUF; i++)
		sc->rbuffs[i]->ie_rbd_next =
		    MK_16(MEM, sc->rbuffs[(i + 1) % NRXBUF]);
	
	/* Tag EOF on the last one */
	sc->rbuffs[NRXBUF - 1]->ie_rbd_length |= IE_RBD_LAST;
	
	/* We use the head and tail pointers on receive to keep track of
	 * the order in which RFDs and RBDs are used. */
	sc->rfhead = 0;
	sc->rftail = NFRAMES - 1;
	sc->rbhead = 0;
	sc->rbtail = NRXBUF - 1;
	
	sc->scb->ie_recv_list = MK_16(MEM, sc->rframes[0]);
	sc->rframes[0]->ie_fd_buf_desc = MK_16(MEM, sc->rbuffs[0]);
	
	ptr = Align(ptr);
	return ptr;
}

/*
 * Run the multicast setup command.
 * Called at splimp().
 */
static int
mc_setup(sc, ptr)
	struct ie_softc *sc;
	caddr_t ptr;
{
	volatile struct ie_mcast_cmd *cmd = (void *)ptr;
	
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
 * This routine takes the environment generated by check_ie_present()
 * and adds to it all the other structures we need to operate the adapter.
 * This includes executing the CONFIGURE, IA-SETUP, and MC-SETUP commands,
 * starting the receiver unit, and clearing interrupts.
 *
 * THIS ROUTINE MUST BE CALLED AT splimp() OR HIGHER.
 */
int
ieinit(sc)
	struct ie_softc *sc;
{
	volatile struct ie_sys_ctl_block *scb = sc->scb;
	caddr_t ptr;
	int n;
	
	ptr = (caddr_t)Align((caddr_t)scb + sizeof *scb);
	
	/*
	 * Send the configure command first.
	 */
	{
		volatile struct ie_config_cmd *cmd = (void *)ptr;
		
		ie_setup_config(cmd, sc->promisc, sc->hard_type == IE_STARLAN10);
		cmd->com.ie_cmd_status = 0;
		cmd->com.ie_cmd_cmd = IE_CMD_CONFIG | IE_CMD_LAST;
		cmd->com.ie_cmd_link = 0xffff;
		
		scb->ie_command_list = MK_16(MEM, cmd);
		
		if (command_and_wait(sc, IE_CU_START, cmd, IE_STAT_COMPL) ||
		    !(cmd->com.ie_cmd_status & IE_STAT_OK)) {
			printf("%s: configure command failed\n", sc->sc_dev.dv_xname);
			return 0;
		}
	}
	/*
	 * Now send the Individual Address Setup command.
	 */
	{
		volatile struct ie_iasetup_cmd *cmd = (void *)ptr;
		
		cmd->com.ie_cmd_status = 0;
		cmd->com.ie_cmd_cmd = IE_CMD_IASETUP | IE_CMD_LAST;
		cmd->com.ie_cmd_link = 0xffff;
		
		bcopy(sc->sc_arpcom.ac_enaddr, (caddr_t)&cmd->ie_address,
		    sizeof cmd->ie_address);
		
		scb->ie_command_list = MK_16(MEM, cmd);
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
	run_tdr(sc, (void *)ptr);
	
	/*
	 * Acknowledge any interrupts we have generated thus far.
	 */
	ie_ack(sc, IE_ST_WHENCE);
	
	/*
	 * Set up the RFA.
	 */
	ptr = setup_rfa(ptr, sc);
	
	/*
	 * Finally, the transmit command and buffer are the last little bit of work.
	 */
	for (n = 0; n < NTXBUF; n++) {
		sc->xmit_cmds[n] = (void *)ptr;
		bzero(ptr, sizeof *sc->xmit_cmds[n]);
		ptr += sizeof *sc->xmit_cmds[n];
		ptr = Align(ptr);
		sc->xmit_buffs[n] = (void *)ptr;
		bzero(ptr, sizeof *sc->xmit_buffs[n]);
		ptr += sizeof *sc->xmit_buffs[n];
		ptr = Align(ptr);
	}
	
	for (n = 0; n < NTXBUF; n++) {
		sc->xmit_cbuffs[n] = (void *)ptr;
		ptr += IE_TBUF_SIZE;
		ptr = Align(ptr);
	}
	
	/*
	 * This must be coordinated with iestart() and ietint().
	 */
	sc->xmit_cmds[0]->ie_xmit_status = IE_STAT_COMPL;
	
	sc->sc_arpcom.ac_if.if_flags |= IFF_RUNNING; /* tell higher levels that we are here */
	start_receiver(sc);
	return 0;
}

static void
iestop(sc)
	struct ie_softc *sc;
{

	command_and_wait(sc, IE_RU_DISABLE, 0, 0);
}

int
ieioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int cmd;
	caddr_t data;
{
	struct ie_softc *sc = iecd.cd_devs[ifp->if_unit];
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;
	
	s = splimp();
	
	switch(cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		
		switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ieinit(sc);
			/*
			 * See if another station has *our* IP address.
			 * i.e.: There is an address conflict! If a
			 * conflict exists, a message is sent to the
			 * console.
			 */
			sc->sc_arpcom.ac_ipaddr = IA_SIN(ifa)->sin_addr;
			arpwhohas(&sc->sc_arpcom, &IA_SIN(ifa)->sin_addr);
			break;
#endif
#ifdef NS
		/* XXX - This code is probably wrong. */
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;
				
			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)(sc->sc_arpcom.ac_enaddr);
			else
				bcopy(ina->x_host.c_host,
				    sc->sc_arpcom.ac_enaddr,
				    sizeof(sc->sc_arpcom.ac_enaddr));
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
		sc->promisc = ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI);
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
		    ether_addmulti(ifr, &sc->sc_arpcom):
		    ether_delmulti(ifr, &sc->sc_arpcom);
		
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
	
	/*
	 * Step through the list of addresses.
	 */
	sc->mcast_count = 0;
	ETHER_FIRST_MULTI(step, &sc->sc_arpcom, enm);
	while (enm) {
		if (sc->mcast_count >= MAXMCAST ||
		    bcmp(enm->enm_addrlo, enm->enm_addrhi, 6) != 0) {
			sc->sc_arpcom.ac_if.if_flags |= IFF_ALLMULTI;
			ieioctl(&sc->sc_arpcom.ac_if, SIOCSIFFLAGS, (void *)0);
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
