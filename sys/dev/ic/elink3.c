/*	$NetBSD: elink3.c,v 1.32.4.6 1997/09/29 20:34:00 thorpej Exp $	*/

/*
 * Copyright (c) 1996, 1997 Jonathan Stone <jonathan@NetBSD.org>
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@beer.org>
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
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

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

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#define ETHER_MIN_LEN	64
#define ETHER_MAX_LEN   1518
#define ETHER_ADDR_LEN  6

/* XXX workaround for MGET weirdshit */
#undef MGET
#define MGET(m, how, type) (m) = m_get((how), (type))

/*
 * Structure to map  media-present bits in boards to 
 * ifmedia codes and printable media names. Used for table-driven
 * ifmedia initialization.
 */
struct ep_media {
	int	epm_eeprom_data;	/* bitmask for eeprom config */
	int	epm_conn;		/* sc->ep_connectors code for medium */
	char*	epm_name;		/* name of medium */
	int	epm_ifmedia;		/* ifmedia word for medium */
	int	epm_ifdata;
};

/*
 * ep_media table for Vortex/Demon/Boomerang:
 * map from media-present bits in register RESET_OPTIONS+2 
 * to  ifmedia "media words" and printable names.
 *
 * XXX indexed directly by INTERNAL_CONFIG default_media field,
 * (i.e., EPMEDIA_ constants)  forcing order of entries. 
 *  Note that 3 is reserved.
 */
struct ep_media ep_vortex_media[8] = {
  { EP_PCI_UTP,        EPC_UTP, "utp",	    IFM_ETHER|IFM_10_T,
       EPMEDIA_10BASE_T },
  { EP_PCI_AUI,        EPC_AUI, "aui",	    IFM_ETHER|IFM_10_5,
       EPMEDIA_AUI },
  { 0,                 0,  	"reserved", IFM_NONE,  EPMEDIA_RESV1 },
  { EP_PCI_BNC,        EPC_BNC, "bnc",	    IFM_ETHER|IFM_10_2,
       EPMEDIA_10BASE_2 },
  { EP_PCI_100BASE_TX, EPC_100TX, "100-TX", IFM_ETHER|IFM_100_TX,
       EPMEDIA_100BASE_TX },
  { EP_PCI_100BASE_FX, EPC_100FX, "100-FX", IFM_ETHER|IFM_100_FX,
       EPMEDIA_100BASE_FX },
  { EP_PCI_100BASE_MII,EPC_MII,   "mii",    IFM_ETHER|IFM_100_TX,
       EPMEDIA_MII },
  { EP_PCI_100BASE_T4, EPC_100T4, "100-T4", IFM_ETHER|IFM_100_T4,
       EPMEDIA_100BASE_T4 }
};

/*
 * ep_media table for 3c509/3c509b/3c579/3c589:
 * map from media-present bits in register CNFG_CNTRL
 * (window 0, offset ?) to  ifmedia "media words" and printable names.
 */
struct ep_media ep_isa_media[3] = {
  { EP_W0_CC_UTP,  EPC_UTP, "utp",   IFM_ETHER|IFM_10_T, EPMEDIA_10BASE_T },
  { EP_W0_CC_AUI,  EPC_AUI, "aui",   IFM_ETHER|IFM_10_5, EPMEDIA_AUI },
  { EP_W0_CC_BNC,  EPC_BNC, "bnc",   IFM_ETHER|IFM_10_2, EPMEDIA_10BASE_2 },
};

/* Map vortex reset_options bits to if_media codes. */
const u_int ep_default_to_media[8] = {
	IFM_ETHER | IFM_10_T,
	IFM_ETHER | IFM_10_5,
	0, 			/* reserved by 3Com */
	IFM_ETHER | IFM_10_2,
	IFM_ETHER | IFM_100_TX,
	IFM_ETHER | IFM_100_FX,
	IFM_ETHER | IFM_100_TX,	/* XXX really MII: need to talk to PHY */
	IFM_ETHER | IFM_100_T4,
};

/* Autoconfig defintion of driver back-end */
struct cfdriver ep_cd = {
	NULL, "ep", DV_IFNET
};


void	ep_internalconfig __P((struct ep_softc *sc));
void	ep_vortex_probemedia __P((struct ep_softc *sc));
void	ep_isa_probemedia __P((struct ep_softc *sc));

static void eptxstat __P((struct ep_softc *));
static int epstatus __P((struct ep_softc *));
void epinit __P((struct ep_softc *));
int epioctl __P((struct ifnet *, u_long, caddr_t));
void epstart __P((struct ifnet *));
void epwatchdog __P((struct ifnet *));
void epreset __P((struct ep_softc *));
static void epshutdown __P((void *));
void	epread __P((struct ep_softc *));
struct mbuf *epget __P((struct ep_softc *, int));
void	epmbuffill __P((void *));
void	epmbufempty __P((struct ep_softc *));
void	epsetfilter __P((struct ep_softc *));
int	epsetmedia __P((struct ep_softc *, int epmedium));

int	epenable __P((struct ep_softc *));
void	epdisable __P((struct ep_softc *));

/* ifmedia callbacks */
int	ep_media_change __P((struct ifnet *ifp));
void	ep_media_status __P((struct ifnet *ifp, struct ifmediareq *req));

static int epbusyeeprom __P((struct ep_softc *));
static inline void ep_complete_cmd __P((struct ep_softc *sc, 
					u_int cmd, u_int arg));


/*
 * Issue a (reset) command, and be sure it has completed.
 * Used for commands that reset part or all of the  board.
 * On newer hardware we could poll SC_COMMAND_IN_PROGRESS,
 * but older hardware doesn't implement it and we must delay.
 * It's easiest to just delay always.
 */
static inline void
ep_complete_cmd(sc, cmd, arg)
	struct ep_softc *sc;
	u_int cmd, arg;
{
	register bus_space_tag_t iot = sc->sc_iot;
	register bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_2(iot, ioh, cmd, arg);

#ifdef notyet
	/* if this adapter family has S_COMMAND_IN_PROGRESS, use it */
	while (bus_space_read_2(iot, ioh, EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;
	else
#else
	DELAY(100000);	/* need at least 1 ms, but be generous. */
#endif
}

/*
 * Back-end attach and configure.
 */
void
epconfig(sc, chipset, enaddr)
	struct ep_softc *sc;
	u_short chipset;
	u_int8_t *enaddr;
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int16_t i;
	u_int8_t myla[6];

	sc->ep_chipset = chipset;

	/*
	 * We could have been groveling around in other register
	 * windows in the front-end; make sure we're in window 0
	 * to read the EEPROM.
	 */
	GO_WINDOW(0);

	if (enaddr == NULL) {
		/*
		 * Read the station address from the eeprom
		 */
		for (i = 0; i < 3; i++) {
			u_int16_t x;
			if (epbusyeeprom(sc))
				return;		/* XXX why is eeprom busy? */
			bus_space_write_2(iot, ioh, EP_W0_EEPROM_COMMAND,
					  READ_EEPROM | i);
			if (epbusyeeprom(sc))
				return;		/* XXX why is eeprom busy? */
			x = bus_space_read_2(iot, ioh, EP_W0_EEPROM_DATA);
			myla[(i << 1)] = x >> 8;
			myla[(i << 1) + 1] = x;
		}
		enaddr = myla;
	}

	printf("%s: MAC address %s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(enaddr));

	/*
	 * Vortex-based (3c59x pci,eisa) and Boomerang (3c900,3c515?) cards
	 * allow FDDI-sized (4500) byte packets.  Commands only take an
	 * 11-bit parameter, and  11 bits isn't enough to hold a full-size
	 * packet length.
	 * Commands to these cards implicitly upshift a packet size
	 * or threshold by 2 bits. 
	 * To detect  cards with large-packet support, we probe by setting
	 * the transmit threshold register, then change windows and
	 * read back the threshold register directly, and see if the
	 * threshold value was shifted or not.
	 */
	bus_space_write_2(iot, ioh, EP_COMMAND,
			  SET_TX_AVAIL_THRESH | EP_LARGEWIN_PROBE ); 
	GO_WINDOW(5);
	i = bus_space_read_2(iot, ioh, EP_W5_TX_AVAIL_THRESH);
	GO_WINDOW(1);
	switch (i)  {
	case EP_LARGEWIN_PROBE:
	case (EP_LARGEWIN_PROBE & EP_LARGEWIN_MASK):
		sc->ep_pktlenshift = 0;
		break;

	case (EP_LARGEWIN_PROBE << 2):
		sc->ep_pktlenshift = 2;
		/* XXX does the 3c515 support Vortex-style RESET_OPTIONS? */
		break;

	default:
		printf("%s: wrote 0x%x to TX_AVAIL_THRESH, read back 0x%x. "
		    "Interface disabled\n",
		    sc->sc_dev.dv_xname, EP_LARGEWIN_PROBE, (int) i);
		return;
	}

	/*
	 * Ensure Tx-available interrupts are enabled for 
	 * start the interface.
	 * XXX should be in epinit()?
	 */
	bus_space_write_2(iot, ioh, EP_COMMAND,
	    SET_TX_AVAIL_THRESH | (1600 >> sc->ep_pktlenshift));

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = epstart;
	ifp->if_ioctl = epioctl;
	ifp->if_watchdog = epwatchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;

	if_attach(ifp);
	ether_ifattach(ifp, enaddr);

	/*
	 * Finish configuration: 
	 * determine chipset if the front-end couldn't do so,
	 * show board details, set media.
	 */

	/* print RAM size */
	ep_internalconfig(sc);
	GO_WINDOW(0);

	ifmedia_init(&sc->sc_media, 0, ep_media_change, ep_media_status);

	/* 
	 * If we've got an indirect (ISA) board, the chipset is
	 * unknown.  If the board has large-packet support, it's a
	 * Vortex/Boomerang, otherwise it's a 3c509.  XXX use eeprom
	 * capability word instead?
	 */

	if (sc->ep_chipset == EP_CHIPSET_UNKNOWN && sc->ep_pktlenshift)  {
		printf("warning: unknown chipset, possibly 3c515?\n");
#ifdef notyet
		sc->sc_chipset = EP_CHIPSET_VORTEX;
#endif	/* notyet */
	}

	/*
	 * Ascertain which media types are present and inform ifmedia.
	 */
	switch (sc->ep_chipset) {
	/* on a direct bus, the attach routine can tell, but check anyway. */
	case EP_CHIPSET_VORTEX:
	case EP_CHIPSET_BOOMERANG2:
		ep_vortex_probemedia(sc);
		break;

	/* on ISA we can't yet tell 3c509 from 3c515. Assume the former. */
	case EP_CHIPSET_3C509:
	default:
		ep_isa_probemedia(sc);
		break;
	}

	GO_WINDOW(1);		/* Window 1 is operating window */

#if NBPFILTER > 0
	bpfattach(&sc->sc_ethercom.ec_if.if_bpf, ifp, DLT_EN10MB,
		  sizeof(struct ether_header));
#endif

	sc->tx_start_thresh = 20;	/* probably a good starting point. */

	/*  Establish callback to reset card when we reboot. */
	shutdownhook_establish(epshutdown, sc);

	ep_complete_cmd(sc, EP_COMMAND, RX_RESET);
	ep_complete_cmd(sc, EP_COMMAND, TX_RESET);
}


/*
 * Show interface-model-independent info from window 3
 * internal-configuration register.
 */
void
ep_internalconfig(sc)
	struct ep_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	u_int config0;
	u_int config1;

	int  ram_size, ram_width, ram_speed, rom_size, ram_split;
	/*
	 * NVRAM buffer Rx:Tx config names for busmastering cards
	 * (Demon, Vortex, and later).
	 */
	const char *onboard_ram_config[] = {
		"5:3", "3:1", "1:1", "(undefined)" };

	GO_WINDOW(3);
	config0 = (u_int)bus_space_read_2(iot, ioh, EP_W3_INTERNAL_CONFIG);
	config1 = (u_int)bus_space_read_2(iot, ioh, EP_W3_INTERNAL_CONFIG + 2);
	GO_WINDOW(0);

	ram_size  = (config0 & CONFIG_RAMSIZE) >> CONFIG_RAMSIZE_SHIFT;
	ram_width = (config0 & CONFIG_RAMWIDTH) >> CONFIG_RAMWIDTH_SHIFT;
	ram_speed = (config0 & CONFIG_RAMSPEED) >> CONFIG_RAMSPEED_SHIFT;
	rom_size  = (config0 & CONFIG_ROMSIZE) >> CONFIG_ROMSIZE_SHIFT;

	ram_split  = (config1 & CONFIG_RAMSPLIT) >> CONFIG_RAMSPLIT_SHIFT;

	printf("%s: %dKB %s-wide FIFO, %s Rx:Tx split, ",
	       sc->sc_dev.dv_xname,
	       8 << ram_size,
	       (ram_width) ? "word" : "byte",
	       onboard_ram_config[ram_split]);
}


/*
 * Find supported media on 3c509-generation hardware that doesn't have
 * a "reset_options" register in window 3.
 * Use the config_cntrl register  in window 0 instead.
 * Used on original, 10Mbit ISA (3c509), 3c509B, and pre-Demon EISA cards
 * that implement  CONFIG_CTRL.  We don't have a good way to set the
 * default active mediuim; punt to ifconfig  instead.
 *
 * XXX what about 3c515, pcmcia 10/100?
 */
void
ep_isa_probemedia(sc)
	struct ep_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifmedia *ifm = &sc->sc_media;
	int	conn, i;
	u_int16_t ep_w0_config, port;

	conn = 0;
	GO_WINDOW(0);
	ep_w0_config = bus_space_read_2(iot, ioh, EP_W0_CONFIG_CTRL);
	for (i = 0; i < 3; i++) {
		struct ep_media * epm = ep_isa_media + i;

		if ((ep_w0_config & epm->epm_eeprom_data) != 0) {

			ifmedia_add(ifm, epm->epm_ifmedia, epm->epm_ifdata, 0);
			if (conn)
				printf("/");
			printf(epm->epm_name);
			conn |= epm->epm_conn;
		}
	}
	sc->ep_connectors = conn;

	/* get default medium from EEPROM */
	if (epbusyeeprom(sc))
		return;		/* XXX why is eeprom busy? */
	bus_space_write_2(iot, ioh, EP_W0_EEPROM_COMMAND,
	    READ_EEPROM | EEPROM_ADDR_CFG);
	if (epbusyeeprom(sc))
		return;		/* XXX why is  eeprom busy? */
	port = bus_space_read_2(iot, ioh, EP_W0_EEPROM_DATA);
	port = port >> 14;

	printf(" (default %s)\n", ep_vortex_media[port].epm_name);
	/* tell ifconfig what currently-active media is. */
	ifmedia_set(ifm, ep_default_to_media[port]);

	/* XXX autoselect not yet implemented */
}


/*
 * Find media present on large-packet-capable elink3 devices.
 * Show onboard configuration of large-packet-capable elink3 devices
 * (Demon, Vortex, Boomerang), which do not implement CONFIG_CTRL in window 0.
 * Use media and card-version info in window 3 instead.
 *
 * XXX how much of this works with 3c515, pcmcia 10/100?
 */
void
ep_vortex_probemedia(sc)
	struct ep_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifmedia *ifm = &sc->sc_media;
	u_int config1, conn;
	int reset_options;
	int default_media;	/* 3-bit encoding of default (EEPROM) media */
	int autoselect;		/* boolean: should default to autoselect */
	const char *medium_name;
	register int i;

	GO_WINDOW(3);
	config1 = (u_int)bus_space_read_2(iot, ioh, EP_W3_INTERNAL_CONFIG + 2);
	reset_options  = (int)bus_space_read_1(iot, ioh, EP_W3_RESET_OPTIONS);
	GO_WINDOW(0);

	default_media = (config1 & CONFIG_MEDIAMASK) >> CONFIG_MEDIAMASK_SHIFT;
        autoselect = (config1 & CONFIG_AUTOSELECT) >> CONFIG_AUTOSELECT_SHIFT;

	/* set available media options */
	conn = 0;
	for (i = 0; i < 8; i++) {
		struct ep_media * epm = ep_vortex_media + i;

		if ((reset_options & epm->epm_eeprom_data) != 0) {
			if (conn) printf("/");
			printf(epm->epm_name);
			conn |= epm->epm_conn;
			ifmedia_add(ifm, epm->epm_ifmedia, epm->epm_ifdata, 0);
		}
	}

	sc->ep_connectors = conn;

	/* Show  eeprom's idea of default media.  */
	medium_name = (default_media > 8)
		? "(unknown/impossible media)"
		: ep_vortex_media[default_media].epm_name;
	printf(" default %s%s\n",
	       medium_name,  (autoselect)? ", autoselect" : "" );

#ifdef notyet	
	/*
	 * Set default: either the active interface the card
	 * reads  from the EEPROM, or if autoselect is true,
	 * whatever we find is actually connected. 
	 *
	 * XXX autoselect not yet implemented.
	 */
#endif	/* notyet */

	/* tell ifconfig what currently-active media is. */
	ifmedia_set(ifm, ep_default_to_media[default_media]);
}


/*
 * Bring device up.
 *
 * The order in here seems important. Otherwise we may not receive
 * interrupts. ?!
 */
void
epinit(sc)
	register struct ep_softc *sc;
{
	register struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;

	while (bus_space_read_2(iot, ioh, EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;

	if (sc->bustype != EP_BUS_PCI) {
		GO_WINDOW(0);
		bus_space_write_2(iot, ioh, EP_W0_CONFIG_CTRL, 0);
		bus_space_write_2(iot, ioh, EP_W0_CONFIG_CTRL, ENABLE_DRQ_IRQ);
	}

	if (sc->bustype == EP_BUS_PCMCIA) {
		bus_space_write_2(iot, ioh, EP_W0_RESOURCE_CFG, 0x3f00);
	}

	GO_WINDOW(2);
	for (i = 0; i < 6; i++)	/* Reload the ether_addr. */
		bus_space_write_1(iot, ioh, EP_W2_ADDR_0 + i,
		    LLADDR(ifp->if_sadl)[i]);

	/*
	 * Reset the station-address receive filter.
	 * A bug workaround for busmastering  (Vortex, Demon) cards.
	 */
	for (i = 0; i < 6; i++)
		bus_space_write_1(iot, ioh, EP_W2_RECVMASK_0 + i, 0);

	ep_complete_cmd(sc, EP_COMMAND, RX_RESET);
	ep_complete_cmd(sc, EP_COMMAND, TX_RESET);

	GO_WINDOW(1);		/* Window 1 is operating window */
	for (i = 0; i < 31; i++)
		bus_space_read_1(iot, ioh, EP_W1_TX_STATUS);

	/* Set threshhold for for Tx-space avaiable interrupt. */
	bus_space_write_2(iot, ioh, EP_COMMAND,
	    SET_TX_AVAIL_THRESH | (1600 >> sc->ep_pktlenshift));

	/* Enable interrupts. */
	bus_space_write_2(iot, ioh, EP_COMMAND, SET_RD_0_MASK | S_CARD_FAILURE |
				S_RX_COMPLETE | S_TX_COMPLETE | S_TX_AVAIL);
	bus_space_write_2(iot, ioh, EP_COMMAND, SET_INTR_MASK | S_CARD_FAILURE |
				S_RX_COMPLETE | S_TX_COMPLETE | S_TX_AVAIL);

	/*
	 * Attempt to get rid of any stray interrupts that occured during
	 * configuration.  On the i386 this isn't possible because one may
	 * already be queued.  However, a single stray interrupt is
	 * unimportant.
	 */
	bus_space_write_2(iot, ioh, EP_COMMAND, ACK_INTR | 0xff);

	epsetfilter(sc);
	epsetmedia(sc, sc->sc_media.ifm_cur->ifm_data);

	bus_space_write_2(iot, ioh, EP_COMMAND, RX_ENABLE);
	bus_space_write_2(iot, ioh, EP_COMMAND, TX_ENABLE);

	epmbuffill(sc);

	/* Interface is now `running', with no output active. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Attempt to start output, if any. */
	epstart(ifp);
}


/*
 * Set multicast receive filter. 
 * elink3 hardware has no selective multicast filter in hardware.
 * Enable reception of all multicasts and filter in software.
 */
void
epsetfilter(sc)
	register struct ep_softc *sc;
{
	register struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	GO_WINDOW(1);		/* Window 1 is operating window */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, EP_COMMAND, SET_RX_FILTER |
	    FIL_INDIVIDUAL | FIL_BRDCST |
	    ((ifp->if_flags & IFF_MULTICAST) ? FIL_MULTICAST : 0 ) |
	    ((ifp->if_flags & IFF_PROMISC) ? FIL_PROMISC : 0 ));
}


int
ep_media_change(ifp)
	struct ifnet *ifp;
{
	register struct ep_softc *sc = ifp->if_softc;

	return	epsetmedia(sc, sc->sc_media.ifm_cur->ifm_data);
}

/*
 * Set active media to a specific given EPMEDIA_<> value.
 * For vortex/demon/boomerang cards, update media field in w3_internal_config,
 *       and power on selected transceiver.
 * For 3c509-generation cards (3c509/3c579/3c589/3c509B),
 *	update media field in w0_address_config, and power on selected xcvr.
 */
int
epsetmedia(sc, medium)
	register struct ep_softc *sc;
	int medium;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int w4_media;

	/*
	 * First, change the media-control bits in EP_W4_MEDIA_TYPE.
	 */

	 /* Turn everything off.  First turn off linkbeat and UTP. */
	GO_WINDOW(4);
	w4_media = bus_space_read_2(iot, ioh, EP_W4_MEDIA_TYPE);
	w4_media =  w4_media & ~(ENABLE_UTP|SQE_ENABLE);
	bus_space_write_2(iot, ioh, EP_W4_MEDIA_TYPE, w4_media);

	/* Turn off coax */
	bus_space_write_2(iot, ioh, EP_COMMAND, STOP_TRANSCEIVER);
	delay(1000);

	/*
	 * Now turn on the selected media/transceiver.
	 */
	GO_WINDOW(4);
	switch  (medium) {
	case EPMEDIA_10BASE_T:
		bus_space_write_2(iot, ioh, EP_W4_MEDIA_TYPE,
		    w4_media | ENABLE_UTP);
		break;

	case EPMEDIA_10BASE_2:
		bus_space_write_2(iot, ioh, EP_COMMAND, START_TRANSCEIVER);
		DELAY(1000);	/* 50ms not enmough? */
		break;

	/* XXX following only for new-generation cards */
	case EPMEDIA_100BASE_TX:
	case EPMEDIA_100BASE_FX:
	case EPMEDIA_100BASE_T4:	/* XXX check documentation */
		bus_space_write_2(iot, ioh, EP_W4_MEDIA_TYPE,
		    w4_media | LINKBEAT_ENABLE);
		DELAY(1000);	/* not strictly necessary? */
		break;

	case EPMEDIA_AUI:
		bus_space_write_2(iot, ioh, EP_W4_MEDIA_TYPE,
		    w4_media | SQE_ENABLE);
		DELAY(1000);	/*  not strictly necessary? */
		break;
	case EPMEDIA_MII:
		break;
	default:
#if defined(DEBUG)
		printf("%s unknown media 0x%x\n", sc->sc_dev.dv_xname, medium);
#endif
		break;
		
	}

	/*
	 * Tell the chip which PHY [sic] to use.
	 */
	if  (sc->ep_chipset==EP_CHIPSET_VORTEX	||
	     sc->ep_chipset==EP_CHIPSET_BOOMERANG2) {
		int config0, config1;

		GO_WINDOW(3);
		config0 = (u_int)bus_space_read_2(iot, ioh,
		    EP_W3_INTERNAL_CONFIG);
		config1 = (u_int)bus_space_read_2(iot, ioh,
		    EP_W3_INTERNAL_CONFIG + 2);

#if defined(DEBUG)
		printf("%s:  read 0x%x, 0x%x from EP_W3_CONFIG register\n",
		       sc->sc_dev.dv_xname, config0, config1);
#endif
		config1 = config1 & ~CONFIG_MEDIAMASK;
		config1 |= (medium << CONFIG_MEDIAMASK_SHIFT);
		
#if defined(DEBUG)
		printf("epsetmedia: %s: medium 0x%x, 0x%x to EP_W3_CONFIG\n",
		    sc->sc_dev.dv_xname, medium, config1);
#endif
		bus_space_write_2(iot, ioh, EP_W3_INTERNAL_CONFIG, config0);
		bus_space_write_2(iot, ioh, EP_W3_INTERNAL_CONFIG + 2, config1);
	}
	else if (sc->ep_chipset == EP_CHIPSET_3C509) {
		register int w0_addr_cfg;

		GO_WINDOW(0);
		w0_addr_cfg = bus_space_read_2(iot, ioh, EP_W0_ADDRESS_CFG);
		w0_addr_cfg &= 0x3fff;
		bus_space_write_2(iot, ioh, EP_W0_ADDRESS_CFG,
		    w0_addr_cfg | (medium << 14));
		DELAY(1000);
	}

	GO_WINDOW(1);		/* Window 1 is operating window */
	return (0);
}

/*
 * Get currently-selected media from card.
 * (if_media callback, may be called before interface is brought up).
 */
void
ep_media_status(ifp, req)
	struct ifnet *ifp;
	struct ifmediareq *req;
{
	register struct ep_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int config1;
	u_int ep_mediastatus;

	/* XXX read from softc when we start autosensing media */
	req->ifm_active = sc->sc_media.ifm_cur->ifm_media;
	
	switch (sc->ep_chipset) {
	case EP_CHIPSET_VORTEX:
	case EP_CHIPSET_BOOMERANG:
		GO_WINDOW(3);
		delay(5000);

		config1 = bus_space_read_2(iot, ioh, EP_W3_INTERNAL_CONFIG + 2);
		GO_WINDOW(1);

		config1 = 
		    (config1 & CONFIG_MEDIAMASK) >> CONFIG_MEDIAMASK_SHIFT;
		req->ifm_active = ep_default_to_media[config1];

		/* XXX check full-duplex bits? */

		GO_WINDOW(4);
		req->ifm_status = IFM_AVALID;	/* XXX */
		ep_mediastatus = bus_space_read_2(iot, ioh, EP_W4_MEDIA_TYPE);
		if (ep_mediastatus & LINKBEAT_DETECT)
			req->ifm_status |= IFM_ACTIVE; 	/* XXX  automedia */

		break;

	case EP_CHIPSET_UNKNOWN:
	case EP_CHIPSET_3C509:
		req->ifm_status = 0;	/* XXX */
		break;

	default:
		printf("%s: media_status on unknown chipset 0x%x\n",
		       ifp->if_xname, sc->ep_chipset);
		break;
	}

	/* XXX look for softc heartbeat for other chips or media */

	GO_WINDOW(1);
	return;
}



/*
 * Start outputting on the interface.
 * Always called as splnet().
 */
void
epstart(ifp)
	struct ifnet *ifp;
{
	register struct ep_softc *sc = ifp->if_softc;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct mbuf *m, *m0;
	int sh, len, pad;

	/* Don't transmit if interface is busy or not running */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

startagain:
	/* Sneak a peek at the next packet */
	m0 = ifp->if_snd.ifq_head;
	if (m0 == 0)
		return;

	/* We need to use m->m_pkthdr.len, so require the header */
	if ((m0->m_flags & M_PKTHDR) == 0)
		panic("epstart: no header mbuf");
	len = m0->m_pkthdr.len;

	pad = (4 - len) & 3;

	/*
	 * The 3c509 automatically pads short packets to minimum ethernet
	 * length, but we drop packets that are too large. Perhaps we should
	 * truncate them instead?
	 */
	if (len + pad > ETHER_MAX_LEN) {
		/* packet is obviously too large: toss it */
		++ifp->if_oerrors;
		IF_DEQUEUE(&ifp->if_snd, m0);
		m_freem(m0);
		goto readcheck;
	}

	if (bus_space_read_2(iot, ioh, EP_W1_FREE_TX) < len + pad + 4) {
		bus_space_write_2(iot, ioh, EP_COMMAND,
		    SET_TX_AVAIL_THRESH |
		    ((len + pad + 4) >> sc->ep_pktlenshift));
		/* not enough room in FIFO */
		ifp->if_flags |= IFF_OACTIVE;
		return;
	} else {
		bus_space_write_2(iot, ioh, EP_COMMAND,
		    SET_TX_AVAIL_THRESH | EP_THRESH_DISABLE );
	}

	IF_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == 0)		/* not really needed */
		return;

	bus_space_write_2(iot, ioh, EP_COMMAND, SET_TX_START_THRESH |
	    ((len / 4 + sc->tx_start_thresh) /* >> sc->ep_pktlenshift*/) );

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m0);
#endif

	/*
	 * Do the output at splhigh() so that an interrupt from another device
	 * won't cause a FIFO underrun.
	 */
	sh = splhigh();

	bus_space_write_2(iot, ioh, EP_W1_TX_PIO_WR_1, len);
	bus_space_write_2(iot, ioh, EP_W1_TX_PIO_WR_1,
	    0xffff);	/* Second dword meaningless */
	if (EP_IS_BUS_32(sc->bustype)) {
		for (m = m0; m; ) {
			if (m->m_len > 3)  {
				/* align our reads from core */
				if (mtod(m, u_long) & 3)  {
					u_long count =
					    4 - (mtod(m, u_long) & 3);
					bus_space_write_multi_1(iot, ioh,
					    EP_W1_TX_PIO_WR_1,
					    mtod(m, u_int8_t *), count);
					m->m_data =
					    (void *)(mtod(m, u_long) + count);
					m->m_len -= count;
				}
				bus_space_write_multi_4(iot, ioh,
				    EP_W1_TX_PIO_WR_1,
				    mtod(m, u_int32_t *), m->m_len >> 2);
				m->m_data = (void *)(mtod(m, u_long) +
					(u_long)(m->m_len & ~3));
				m->m_len -= m->m_len & ~3;
			}
			if (m->m_len)  {
				bus_space_write_multi_1(iot, ioh,
				    EP_W1_TX_PIO_WR_1,
				    mtod(m, u_int8_t *), m->m_len);
			}
			MFREE(m, m0);
			m = m0;
		}
	} else {
		for (m = m0; m; ) {
			if (m->m_len > 1)  {
				if (mtod(m, u_long) & 1)  {
					bus_space_write_1(iot, ioh,
					    EP_W1_TX_PIO_WR_1,
					    *(mtod(m, u_int8_t *)));
					m->m_data =
					    (void *)(mtod(m, u_long) + 1);
					m->m_len -= 1;
				}
				bus_space_write_multi_2(iot, ioh,
				    EP_W1_TX_PIO_WR_1, mtod(m, u_int16_t *),
				    m->m_len >> 1);
			}
			if (m->m_len & 1)  {
				bus_space_write_1(iot, ioh, EP_W1_TX_PIO_WR_1,
				     *(mtod(m, u_int8_t *) + m->m_len - 1));
			}
			MFREE(m, m0);
			m = m0;
		}
	}
	while (pad--)
		bus_space_write_1(iot, ioh, EP_W1_TX_PIO_WR_1, 0);

	splx(sh);

	++ifp->if_opackets;

readcheck:
	if ((bus_space_read_2(iot, ioh, EP_W1_RX_STATUS) & ERR_INCOMPLETE) == 0) {
		/* We received a complete packet. */
		u_int16_t status = bus_space_read_2(iot, ioh, EP_STATUS);

		if ((status & S_INTR_LATCH) == 0) {
			/*
			 * No interrupt, read the packet and continue
			 * Is  this supposed to happen? Is my motherboard 
			 * completely busted?
			 */
			epread(sc);
		} else {
			/* Got an interrupt, return so that it gets serviced. */
#if 0
			printf("%s: S_INTR_LATCH %04x mask=%04x ipending=%04x (%04x)\n",
			       sc->sc_dev.dv_xname, status,
			       cpl, ipending, imask[IPL_NET]);
#endif

			return;
		}
	} else {
		/* Check if we are stuck and reset [see XXX comment] */
		if (epstatus(sc)) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: adapter reset\n",
				    sc->sc_dev.dv_xname);
			epreset(sc);
		}
	}

	goto startagain;
}


/*
 * XXX: The 3c509 card can get in a mode where both the fifo status bit
 *	FIFOS_RX_OVERRUN and the status bit ERR_INCOMPLETE are set
 *	We detect this situation and we reset the adapter.
 *	It happens at times when there is a lot of broadcast traffic
 *	on the cable (once in a blue moon).
 */
static int
epstatus(sc)
	register struct ep_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int16_t fifost;

	/*
	 * Check the FIFO status and act accordingly
	 */
	GO_WINDOW(4);
	fifost = bus_space_read_2(iot, ioh, EP_W4_FIFO_DIAG);
	GO_WINDOW(1);

	if (fifost & FIFOS_RX_UNDERRUN) {
		if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
			printf("%s: RX underrun\n", sc->sc_dev.dv_xname);
		epreset(sc);
		return 0;
	}

	if (fifost & FIFOS_RX_STATUS_OVERRUN) {
		if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
			printf("%s: RX Status overrun\n", sc->sc_dev.dv_xname);
		return 1;
	}

	if (fifost & FIFOS_RX_OVERRUN) {
		if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
			printf("%s: RX overrun\n", sc->sc_dev.dv_xname);
		return 1;
	}

	if (fifost & FIFOS_TX_OVERRUN) {
		if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
			printf("%s: TX overrun\n", sc->sc_dev.dv_xname);
		epreset(sc);
		return 0;
	}

	return 0;
}


static void
eptxstat(sc)
	register struct ep_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;

	/*
	 * We need to read+write TX_STATUS until we get a 0 status
	 * in order to turn off the interrupt flag.
	 */
	while ((i = bus_space_read_1(iot, ioh, EP_W1_TX_STATUS)) & TXS_COMPLETE) {
		bus_space_write_1(iot, ioh, EP_W1_TX_STATUS, 0x0);

		if (i & TXS_JABBER) {
			++sc->sc_ethercom.ec_if.if_oerrors;
			if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
				printf("%s: jabber (%x)\n",
				       sc->sc_dev.dv_xname, i);
			epreset(sc);
		} else if (i & TXS_UNDERRUN) {
			++sc->sc_ethercom.ec_if.if_oerrors;
			if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
				printf("%s: fifo underrun (%x) @%d\n",
				       sc->sc_dev.dv_xname, i,
				       sc->tx_start_thresh);
			if (sc->tx_succ_ok < 100)
				    sc->tx_start_thresh = min(ETHER_MAX_LEN,
					    sc->tx_start_thresh + 20);
			sc->tx_succ_ok = 0;
			epreset(sc);
		} else if (i & TXS_MAX_COLLISION) {
			++sc->sc_ethercom.ec_if.if_collisions;
			bus_space_write_2(iot, ioh, EP_COMMAND, TX_ENABLE);
			sc->sc_ethercom.ec_if.if_flags &= ~IFF_OACTIVE;
		} else
			sc->tx_succ_ok = (sc->tx_succ_ok+1) & 127;
	}
}

int
epintr(arg)
	void *arg;
{
	register struct ep_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	u_int16_t status;
	int ret = 0;

	if (sc->enabled == 0)
		return (0);

	for (;;) {
		bus_space_write_2(iot, ioh, EP_COMMAND, C_INTR_LATCH);

		status = bus_space_read_2(iot, ioh, EP_STATUS);

		if ((status & (S_TX_COMPLETE | S_TX_AVAIL |
			       S_RX_COMPLETE | S_CARD_FAILURE)) == 0) {
			if ((status & S_INTR_LATCH) == 0) {
#if 0
				printf("%s: intr latch cleared\n",
				       sc->sc_dev.dv_xname);
#endif
				break;
			}
		}

		ret = 1;

		/*
		 * Acknowledge any interrupts.  It's important that we do this
		 * first, since there would otherwise be a race condition.
		 * Due to the i386 interrupt queueing, we may get spurious
		 * interrupts occasionally.
		 */
		bus_space_write_2(iot, ioh, EP_COMMAND, ACK_INTR |
				  (status & (C_INTR_LATCH |
					     C_CARD_FAILURE |
					     C_TX_COMPLETE |
					     C_TX_AVAIL |
					     C_RX_COMPLETE |
					     C_RX_EARLY |
					     C_INT_RQD |
					     C_UPD_STATS)));

#if 0
		status = bus_space_read_2(iot, ioh, EP_STATUS);

		printf("%s: intr%s%s%s%s\n", sc->sc_dev.dv_xname,
		       (status & S_RX_COMPLETE)?" RX_COMPLETE":"",
		       (status & S_TX_COMPLETE)?" TX_COMPLETE":"",
		       (status & S_TX_AVAIL)?" TX_AVAIL":"",
		       (status & S_CARD_FAILURE)?" CARD_FAILURE":"");
#endif

		if (status & S_RX_COMPLETE)
			epread(sc);
		if (status & S_TX_AVAIL) {
			sc->sc_ethercom.ec_if.if_flags &= ~IFF_OACTIVE;
			epstart(&sc->sc_ethercom.ec_if);
		}
		if (status & S_CARD_FAILURE) {
			printf("%s: adapter failure (%x)\n",
			    sc->sc_dev.dv_xname, status);
			epreset(sc);
			return (1);
		}
		if (status & S_TX_COMPLETE) {
			eptxstat(sc);
			epstart(ifp);
		}
	}	

	/* no more interrupts */
	return (ret);
}

void
epread(sc)
	register struct ep_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mbuf *m;
	struct ether_header *eh;
	int len;

	len = bus_space_read_2(iot, ioh, EP_W1_RX_STATUS);

again:
	if (ifp->if_flags & IFF_DEBUG) {
		int err = len & ERR_MASK;
		char *s = NULL;

		if (len & ERR_INCOMPLETE)
			s = "incomplete packet";
		else if (err == ERR_OVERRUN)
			s = "packet overrun";
		else if (err == ERR_RUNT)
			s = "runt packet";
		else if (err == ERR_ALIGNMENT)
			s = "bad alignment";
		else if (err == ERR_CRC)
			s = "bad crc";
		else if (err == ERR_OVERSIZE)
			s = "oversized packet";
		else if (err == ERR_DRIBBLE)
			s = "dribble bits";

		if (s)
			printf("%s: %s\n", sc->sc_dev.dv_xname, s);
	}

	if (len & ERR_INCOMPLETE)
		return;

	if (len & ERR_RX) {
		++ifp->if_ierrors;
		goto abort;
	}

	len &= RX_BYTES_MASK;	/* Lower 11 bits = RX bytes. */

	/* Pull packet off interface. */
	m = epget(sc, len);
	if (m == 0) {
		ifp->if_ierrors++;
		goto abort;
	}

	++ifp->if_ipackets;

	/* We assume the header fit entirely in one mbuf. */
	eh = mtod(m, struct ether_header *);

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	if (ifp->if_bpf) {
		bpf_mtap(ifp->if_bpf, m);

		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no BPF listeners.  And if we are in promiscuous
		 * mode, we have to check if this packet is really ours.
		 */
		if ((ifp->if_flags & IFF_PROMISC) &&
		    (eh->ether_dhost[0] & 1) == 0 && /* !mcast and !bcast */
		    bcmp(eh->ether_dhost, LLADDR(sc->sc_ethercom.ec_if.if_sadl),
			    sizeof(eh->ether_dhost)) != 0) {
			m_freem(m);
			return;
		}
	}
#endif

	/* We assume the header fit entirely in one mbuf. */
	m_adj(m, sizeof(struct ether_header));
	ether_input(ifp, eh, m);

	/*
	 * In periods of high traffic we can actually receive enough
	 * packets so that the fifo overrun bit will be set at this point,
	 * even though we just read a packet. In this case we
	 * are not going to receive any more interrupts. We check for
	 * this condition and read again until the fifo is not full.
	 * We could simplify this test by not using epstatus(), but
	 * rechecking the RX_STATUS register directly. This test could
	 * result in unnecessary looping in cases where there is a new
	 * packet but the fifo is not full, but it will not fix the
	 * stuck behavior.
	 *
	 * Even with this improvement, we still get packet overrun errors
	 * which are hurting performance. Maybe when I get some more time
	 * I'll modify epread() so that it can handle RX_EARLY interrupts.
	 */
	if (epstatus(sc)) {
		len = bus_space_read_2(iot, ioh, EP_W1_RX_STATUS);
		/* Check if we are stuck and reset [see XXX comment] */
		if (len & ERR_INCOMPLETE) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: adapter reset\n",
				    sc->sc_dev.dv_xname);
			epreset(sc);
			return;
		}
		goto again;
	}

	return;

abort:
	bus_space_write_2(iot, ioh, EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (bus_space_read_2(iot, ioh, EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;
}

struct mbuf *
epget(sc, totlen)
	struct ep_softc *sc;
	int totlen;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mbuf *top, **mp, *m;
	int len, remaining;
	int sh;

	m = sc->mb[sc->next_mb];
	sc->mb[sc->next_mb] = 0;
	if (m == 0) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == 0)
			return 0;
	} else {
		/* If the queue is no longer full, refill. */
		if (sc->last_mb == sc->next_mb)
			timeout(epmbuffill, sc, 1);
		/* Convert one of our saved mbuf's. */
		sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
		m->m_data = m->m_pktdat;
		m->m_flags = M_PKTHDR;
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	len = MHLEN;
	top = 0;
	mp = &top;

	/*
	 * We read the packet at splhigh() so that an interrupt from another
	 * device doesn't cause the card's buffer to overflow while we're
	 * reading it.  We may still lose packets at other times.
	 */
	sh = splhigh();

	while (totlen > 0) {
		if (top) {
			m = sc->mb[sc->next_mb];
			sc->mb[sc->next_mb] = 0;
			if (m == 0) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == 0) {
					splx(sh);
					m_freem(top);
					return 0;
				}
			} else {
				sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
			}
			len = MLEN;
		}
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				splx(sh);
				m_free(m);
				m_freem(top);
				return 0;
			}
			len = MCLBYTES;
		}
		if (top == 0)  {
			/* align the struct ip header */
			caddr_t newdata = (caddr_t)
			    ALIGN(m->m_data + sizeof(struct ether_header))
			    - sizeof(struct ether_header);
			len -= newdata - m->m_data;
			m->m_data = newdata;
		}
		remaining = len = min(totlen, len);
		if (EP_IS_BUS_32(sc->bustype)) {
			u_long offset = mtod(m, u_long);
			/*
			 * Read bytes up to the point where we are aligned.
			 * (We can align to 4 bytes, rather than ALIGNBYTES,
			 * here because we're later reading 4-byte chunks.)
			 */
			if ((remaining > 3) && (offset & 3))  {
				int count = (4 - (offset & 3));
				bus_space_read_multi_1(iot, ioh,
				    EP_W1_RX_PIO_RD_1,
				    (u_int8_t *) offset, count);
				offset += count;
				remaining -= count;
			}
			if (remaining > 3) {
				bus_space_read_multi_4(iot, ioh,
				    EP_W1_RX_PIO_RD_1,
				    (u_int32_t *) offset, remaining >> 2);
				offset += remaining & ~3;
				remaining &= 3;
			}
			if (remaining)  {
				bus_space_read_multi_1(iot, ioh,
				    EP_W1_RX_PIO_RD_1,
				    (u_int8_t *) offset, remaining);
			}
		} else {
			u_long offset = mtod(m, u_long);
			if ((remaining > 1) && (offset & 1))  {
				bus_space_read_multi_1(iot, ioh,
				    EP_W1_RX_PIO_RD_1,
				    (u_int8_t *) offset, 1);
				remaining -= 1;
				offset += 1;
			}
			if (remaining > 1) {
				bus_space_read_multi_2(iot, ioh,
				    EP_W1_RX_PIO_RD_1,
				    (u_int16_t *) offset, remaining >> 1);
				offset += remaining & ~1;
			}
			if (remaining & 1)  {
				bus_space_read_multi_1(iot, ioh,
				    EP_W1_RX_PIO_RD_1,
				    (u_int8_t *) offset, remaining & 1);
			}
		}
		m->m_len = len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	bus_space_write_2(iot, ioh, EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (bus_space_read_2(iot, ioh, EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;

	splx(sh);

	return top;
}

int
epioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ep_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {

	case SIOCSIFADDR:
		if ((error = epenable(sc)) != 0)
			break;
		/* epinit is called just below */
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			epinit(sc);
			arp_ifinit(&sc->sc_ethercom.ec_if, ifa);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			register struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host = *(union ns_host *)
				    LLADDR(ifp->if_sadl);
			else
				bcopy(ina->x_host.c_host,
				    LLADDR(ifp->if_sadl),
				    ifp->if_addrlen);
			/* Set new address. */
			epinit(sc);
			break;
		    }
#endif
		default:
			epinit(sc);
			break;
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (sc->enabled)
			error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		else
			error = EIO;
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			epstop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			epdisable(sc);
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			if ((error = epenable(sc)) != 0)
				break;
			epinit(sc);
		} else if (sc->enabled) {
			/*
			 * deal with flags changes:
			 * IFF_MULTICAST, IFF_PROMISC.
			 */
			epsetfilter(sc);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (sc->enabled == 0) {
			error = EIO;
			break;
		}

		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ethercom) :
		    ether_delmulti(ifr, &sc->sc_ethercom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			epreset(sc);
			error = 0;
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}

void
epreset(sc)
	struct ep_softc *sc;
{
	int s;

	s = splnet();
	epstop(sc);
	epinit(sc);
	splx(s);
}

void
epwatchdog(ifp)
	struct ifnet *ifp;
{
	struct ep_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_ethercom.ec_if.if_oerrors;

	epreset(sc);
}

void
epstop(sc)
	register struct ep_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_2(iot, ioh, EP_COMMAND, RX_DISABLE);
	bus_space_write_2(iot, ioh, EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (bus_space_read_2(iot, ioh, EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;
	bus_space_write_2(iot, ioh, EP_COMMAND, TX_DISABLE);
	bus_space_write_2(iot, ioh, EP_COMMAND, STOP_TRANSCEIVER);

	ep_complete_cmd(sc, EP_COMMAND, RX_RESET);
	ep_complete_cmd(sc, EP_COMMAND, TX_RESET);

	bus_space_write_2(iot, ioh, EP_COMMAND, C_INTR_LATCH);
	bus_space_write_2(iot, ioh, EP_COMMAND, SET_RD_0_MASK);
	bus_space_write_2(iot, ioh, EP_COMMAND, SET_INTR_MASK);
	bus_space_write_2(iot, ioh, EP_COMMAND, SET_RX_FILTER);

	epmbufempty(sc);
}


/*
 * Before reboots, reset card completely.
 */
static void
epshutdown(arg)
	void *arg;
{
	register struct ep_softc *sc = arg;

	if (sc->enabled) {
		epstop(sc);
		ep_complete_cmd(sc, EP_COMMAND, GLOBAL_RESET);
	}
}

/*
 * We get eeprom data from the id_port given an offset into the
 * eeprom.  Basically; after the ID_sequence is sent to all of
 * the cards; they enter the ID_CMD state where they will accept
 * command requests. 0x80-0xbf loads the eeprom data.  We then
 * read the port 16 times and with every read; the cards check
 * for contention (ie: if one card writes a 0 bit and another
 * writes a 1 bit then the host sees a 0. At the end of the cycle;
 * each card compares the data on the bus; if there is a difference
 * then that card goes into ID_WAIT state again). In the meantime;
 * one bit of data is returned in the AX register which is conveniently
 * returned to us by bus_space_read_1().  Hence; we read 16 times getting one
 * bit of data with each read.
 *
 * NOTE: the caller must provide an i/o handle for ELINK_ID_PORT!
 */
u_int16_t
epreadeeprom(iot, ioh, offset)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int offset;
{
	u_int16_t data = 0;
	int i;

	bus_space_write_1(iot, ioh, 0, 0x80 + offset);
	delay(1000);
	for (i = 0; i < 16; i++)
		data = (data << 1) | (bus_space_read_2(iot, ioh, 0) & 1);
	return (data);
}

static int
epbusyeeprom(sc)
	struct ep_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i = 100, j;

	if (sc->bustype == EP_BUS_PCMCIA) {
		delay(1000);
		return 0;
	}

	j = 0;		/* bad GCC flow analysis */
	while (i--) {
		j = bus_space_read_2(iot, ioh, EP_W0_EEPROM_COMMAND);
		if (j & EEPROM_BUSY)
			delay(100);
		else
			break;
	}
	if (!i) {
		printf("\n%s: eeprom failed to come ready\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}
	if (j & EEPROM_TST_MODE) {
		/* XXX PnP mode? */
		printf("\n%s: erase pencil mark!\n", sc->sc_dev.dv_xname);
		return (1);
	}
	return (0);
}

void
epmbuffill(v)
	void *v;
{
	struct ep_softc *sc = v;
	int s, i;

	s = splnet();
	i = sc->last_mb;
	do {
		if (sc->mb[i] == NULL)
			MGET(sc->mb[i], M_DONTWAIT, MT_DATA);
		if (sc->mb[i] == NULL)
			break;
		i = (i + 1) % MAX_MBS;
	} while (i != sc->next_mb);
	sc->last_mb = i;
	/* If the queue was not filled, try again. */
	if (sc->last_mb != sc->next_mb)
		timeout(epmbuffill, sc, 1);
	splx(s);
}

void
epmbufempty(sc)
	struct ep_softc *sc;
{
	int s, i;

	s = splnet();
	for (i = 0; i<MAX_MBS; i++) {
		if (sc->mb[i]) {
			m_freem(sc->mb[i]);
			sc->mb[i] = NULL;
		}
	}
	sc->last_mb = sc->next_mb = 0;
	untimeout(epmbuffill, sc);
	splx(s);
}

int
epenable(sc)
	struct ep_softc *sc;
{

	if (sc->enabled == 0 && sc->enable != NULL) {
		if ((*sc->enable)(sc) != 0) {
			printf("%s: device enable failed\n",
			    sc->sc_dev.dv_xname);
			return (EIO);
		}
	}

	sc->enabled = 1;
	return (0);
}

void
epdisable(sc)
	struct ep_softc *sc;
{

	if (sc->enabled != 0 && sc->disable != NULL)
		(*sc->disable)(sc);

	sc->enabled = 0;
}
