/*	$NetBSD: am7990.c,v 1.35.4.1 1997/10/14 10:22:24 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include "bpfilter.h"
#include "rnd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h> 
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if defined(CCITT) && defined(LLC)
#include <sys/socketvar.h>
#include <netccitt/x25.h>
#include <netccitt/pk.h>
#include <netccitt/pk_var.h>
#include <netccitt/pk_extern.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#ifdef LEDEBUG
void am7990_recv_print __P((struct am7990_softc *, int));
void am7990_xmit_print __P((struct am7990_softc *, int));
#endif

integrate void am7990_rint __P((struct am7990_softc *));
integrate void am7990_tint __P((struct am7990_softc *));

integrate int am7990_put __P((struct am7990_softc *, int, struct mbuf *));
integrate struct mbuf *am7990_get __P((struct am7990_softc *, int, int));
integrate void am7990_read __P((struct am7990_softc *, int, int)); 

hide void am7990_shutdown __P((void *));

int am7990_mediachange __P((struct ifnet *));
void am7990_mediastatus __P((struct ifnet *, struct ifmediareq *));

#define	ifp	(&sc->sc_ethercom.ec_if)

static inline int ether_cmp __P((char *, char *));

/*
 * Compare two Ether/802 addresses for equality, inlined and
 * unrolled for speed.
 * Note: test last byte of ethernet address first, as it will
 * likely have the most random distribution.
 */
static inline int
ether_cmp(a, b)
	char *a, *b;
{

	return ((a[5] != b[5]) || (a[4] != b[4]) || (a[3] != b[3]) ||
		(a[2] != b[2]) || (a[1] != b[1]) || (a[0] != b[0]));
}

#define ETHER_CMP	ether_cmp

/*
 * am7990 configuration driver.  Attachments are provided by
 * machine-dependent driver front-ends.
 */
struct cfdriver le_cd = {
	NULL, "le", DV_IFNET
};

void
am7990_config(sc)
	struct am7990_softc *sc;
{
	int mem, i;

	/* Make sure the chip is stopped. */
	am7990_stop(sc);

	/* Initialize ifnet structure. */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = am7990_start;
	ifp->if_ioctl = am7990_ioctl;
	ifp->if_watchdog = am7990_watchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
#ifdef LANCE_REVC_BUG
	ifp->if_flags &= ~IFF_MULTICAST;
#endif

	/* Initialize ifmedia structures. */
	ifmedia_init(&sc->sc_media, 0, am7990_mediachange, am7990_mediastatus);
	if (sc->sc_supmedia != NULL) {
		for (i = 0; i < sc->sc_nsupmedia; i++)
			ifmedia_add(&sc->sc_media, sc->sc_supmedia[i],
			   0, NULL);
		ifmedia_set(&sc->sc_media, sc->sc_defaultmedia);
	} else {
		ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_MANUAL);
	}

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	switch (sc->sc_memsize) {
	case 8192:
		sc->sc_nrbuf = 4;
		sc->sc_ntbuf = 1;
		break;
	case 16384:
		sc->sc_nrbuf = 8;
		sc->sc_ntbuf = 2;
		break;
	case 32768:
		sc->sc_nrbuf = 16;
		sc->sc_ntbuf = 4;
		break;
	case 65536:
		sc->sc_nrbuf = 32;
		sc->sc_ntbuf = 8;
		break;
	case 131072:
		sc->sc_nrbuf = 64;
		sc->sc_ntbuf = 16;
		break;
	default:
		panic("am7990_config: weird memory size");
	}

	printf(": address %s\n", ether_sprintf(sc->sc_enaddr));
	printf("%s: %d receive buffers, %d transmit buffers\n",
	    sc->sc_dev.dv_xname, sc->sc_nrbuf, sc->sc_ntbuf);

	sc->sc_sh = shutdownhook_establish(am7990_shutdown, sc);
	if (sc->sc_sh == NULL)
		panic("am7990_config: can't establish shutdownhook");
	sc->sc_rbufaddr = malloc(sc->sc_nrbuf * sizeof(int), M_DEVBUF,
					M_WAITOK);
	sc->sc_tbufaddr = malloc(sc->sc_ntbuf * sizeof(int), M_DEVBUF,
					M_WAITOK);

	mem = 0;
	sc->sc_initaddr = mem;
	mem += sizeof(struct leinit);
	sc->sc_rmdaddr = mem;
	mem += sizeof(struct lermd) * sc->sc_nrbuf;
	sc->sc_tmdaddr = mem;
	mem += sizeof(struct letmd) * sc->sc_ntbuf;
	for (i = 0; i < sc->sc_nrbuf; i++, mem += LEBLEN)
		sc->sc_rbufaddr[i] = mem;
	for (i = 0; i < sc->sc_ntbuf; i++, mem += LEBLEN)
		sc->sc_tbufaddr[i] = mem;
#ifdef notyet
	if (mem > ...)
		panic(...);
#endif

#if NRND > 0
	rnd_attach_source(&sc->rnd_source, sc->sc_dev.dv_xname,
			  RND_TYPE_NET);
#endif
}

void
am7990_reset(sc)
	struct am7990_softc *sc;
{
	int s;

	s = splimp();
	am7990_init(sc);
	splx(s);
}

/*
 * Set up the initialization block and the descriptor rings.
 */
void
am7990_meminit(sc)
	register struct am7990_softc *sc;
{
	u_long a;
	int bix;
	struct leinit init;
	struct lermd rmd;
	struct letmd tmd;
	u_int8_t *myaddr;

#if NBPFILTER > 0
	if (ifp->if_flags & IFF_PROMISC)
		init.init_mode = LE_MODE_NORMAL | LE_MODE_PROM;
	else
#endif
		init.init_mode = LE_MODE_NORMAL;
	if (sc->sc_initmodemedia == 1)
		init.init_mode |= LE_MODE_PSEL0;

	myaddr = LLADDR(ifp->if_sadl);
	init.init_padr[0] = (myaddr[1] << 8) | myaddr[0];
	init.init_padr[1] = (myaddr[3] << 8) | myaddr[2];
	init.init_padr[2] = (myaddr[5] << 8) | myaddr[4];
	am7990_setladrf(&sc->sc_ethercom, init.init_ladrf);

	sc->sc_last_rd = 0;
	sc->sc_first_td = sc->sc_last_td = sc->sc_no_td = 0;

	a = sc->sc_addr + LE_RMDADDR(sc, 0);
	init.init_rdra = a;
	init.init_rlen = (a >> 16) | ((ffs(sc->sc_nrbuf) - 1) << 13);

	a = sc->sc_addr + LE_TMDADDR(sc, 0);
	init.init_tdra = a;
	init.init_tlen = (a >> 16) | ((ffs(sc->sc_ntbuf) - 1) << 13);

	(*sc->sc_copytodesc)(sc, &init, LE_INITADDR(sc), sizeof(init));

	/*
	 * Set up receive ring descriptors.
	 */
	for (bix = 0; bix < sc->sc_nrbuf; bix++) {
		a = sc->sc_addr + LE_RBUFADDR(sc, bix);
		rmd.rmd0 = a;
		rmd.rmd1_hadr = a >> 16;
		rmd.rmd1_bits = LE_R1_OWN;
		rmd.rmd2 = -LEBLEN | LE_XMD2_ONES;
		rmd.rmd3 = 0;
		(*sc->sc_copytodesc)(sc, &rmd, LE_RMDADDR(sc, bix),
		    sizeof(rmd));
	}

	/*
	 * Set up transmit ring descriptors.
	 */
	for (bix = 0; bix < sc->sc_ntbuf; bix++) {
		a = sc->sc_addr + LE_TBUFADDR(sc, bix);
		tmd.tmd0 = a;
		tmd.tmd1_hadr = a >> 16;
		tmd.tmd1_bits = 0;
		tmd.tmd2 = 0 | LE_XMD2_ONES;
		tmd.tmd3 = 0;
		(*sc->sc_copytodesc)(sc, &tmd, LE_TMDADDR(sc, bix),
		    sizeof(tmd));
	}
}

void
am7990_stop(sc)
	struct am7990_softc *sc;
{

	(*sc->sc_wrcsr)(sc, LE_CSR0, LE_C0_STOP);
}

/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
void
am7990_init(sc)
	register struct am7990_softc *sc;
{
	register int timo;
	u_long a;

	(*sc->sc_wrcsr)(sc, LE_CSR0, LE_C0_STOP);
	DELAY(100);

	/* Newer LANCE chips have a reset register */
	if (sc->sc_hwreset)
		(*sc->sc_hwreset)(sc);

	/* Set the correct byte swapping mode, etc. */
	(*sc->sc_wrcsr)(sc, LE_CSR3, sc->sc_conf3);

	/* Set up LANCE init block. */
	am7990_meminit(sc);

	/* Give LANCE the physical address of its init block. */
	a = sc->sc_addr + LE_INITADDR(sc);
	(*sc->sc_wrcsr)(sc, LE_CSR1, a);
	(*sc->sc_wrcsr)(sc, LE_CSR2, a >> 16);

	/* Try to initialize the LANCE. */
	DELAY(100);
	(*sc->sc_wrcsr)(sc, LE_CSR0, LE_C0_INIT);

	/* Wait for initialization to finish. */
	for (timo = 100000; timo; timo--)
		if ((*sc->sc_rdcsr)(sc, LE_CSR0) & LE_C0_IDON)
			break;

	if ((*sc->sc_rdcsr)(sc, LE_CSR0) & LE_C0_IDON) {
		/* Start the LANCE. */
		(*sc->sc_wrcsr)(sc, LE_CSR0, LE_C0_INEA | LE_C0_STRT |
		    LE_C0_IDON);
		ifp->if_flags |= IFF_RUNNING;
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;
		am7990_start(ifp);
	} else
		printf("%s: card failed to initialize\n", sc->sc_dev.dv_xname);
	if (sc->sc_hwinit)
		(*sc->sc_hwinit)(sc);
}

/*
 * Routine to copy from mbuf chain to transmit buffer in
 * network buffer memory.
 */
integrate int
am7990_put(sc, boff, m)
	struct am7990_softc *sc;
	int boff;
	register struct mbuf *m;
{
	register struct mbuf *n;
	register int len, tlen = 0;

	for (; m; m = n) {
		len = m->m_len;
		if (len == 0) {
			MFREE(m, n);
			continue;
		}
		(*sc->sc_copytobuf)(sc, mtod(m, caddr_t), boff, len);
		boff += len;
		tlen += len;
		MFREE(m, n);
	}
	if (tlen < LEMINSIZE) {
		(*sc->sc_zerobuf)(sc, boff, LEMINSIZE - tlen);
		tlen = LEMINSIZE;
	}
	return (tlen);
}

/*
 * Pull data off an interface.
 * Len is length of data, with local net header stripped.
 * We copy the data into mbufs.  When full cluster sized units are present
 * we copy into clusters.
 */
integrate struct mbuf *
am7990_get(sc, boff, totlen)
	struct am7990_softc *sc;
	int boff, totlen;
{
	register struct mbuf *m;
	struct mbuf *top, **mp;
	int len;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	len = MHLEN;
	top = 0;
	mp = &top;

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
				m_free(m);
				m_freem(top);
				return 0;
			}
			len = MCLBYTES;
		}
		if (!top) {
			register int pad =
			    ALIGN(sizeof(struct ether_header)) -
			        sizeof(struct ether_header);
			m->m_data += pad;
			len -= pad;
		}
		m->m_len = len = min(totlen, len);
		(*sc->sc_copyfrombuf)(sc, mtod(m, caddr_t), boff, len);
		boff += len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}

/*
 * Pass a packet to the higher levels.
 */
integrate void
am7990_read(sc, boff, len)
	register struct am7990_softc *sc;
	int boff, len;
{
	struct mbuf *m;
	struct ether_header *eh;

	if (len <= sizeof(struct ether_header) ||
	    len > ETHERMTU + sizeof(struct ether_header)) {
#ifdef LEDEBUG
		printf("%s: invalid packet size %d; dropping\n",
		    sc->sc_dev.dv_xname, len);
#endif
		ifp->if_ierrors++;
		return;
	}

	/* Pull packet off interface. */
	m = am7990_get(sc, boff, len);
	if (m == 0) {
		ifp->if_ierrors++;
		return;
	}

	ifp->if_ipackets++;

	/* We assume that the header fit entirely in one mbuf. */
	eh = mtod(m, struct ether_header *);

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	if (ifp->if_bpf) {
		bpf_mtap(ifp->if_bpf, m);

#ifndef LANCE_REVC_BUG
		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no BPF listeners.  And if we are in promiscuous
		 * mode, we have to check if this packet is really ours.
		 */
		if ((ifp->if_flags & IFF_PROMISC) != 0 &&
		    (eh->ether_dhost[0] & 1) == 0 && /* !mcast and !bcast */
		    ETHER_CMP(eh->ether_dhost, LLADDR(ifp->if_sadl))) {
			m_freem(m);
			return;
		}
#endif
	}
#endif

#ifdef LANCE_REVC_BUG
	/*
	 * The old LANCE (Rev. C) chips have a bug which causes
	 * garbage to be inserted in front of the received packet.
	 * The work-around is to ignore packets with an invalid
	 * destination address (garbage will usually not match).
	 * Of course, this precludes multicast support...
	 */
	if (ETHER_CMP(eh->ether_dhost, LLADDR(ifp->if_sadl)) &&
	    ETHER_CMP(eh->ether_dhost, etherbroadcastaddr)) {
		m_freem(m);
		return;
	}
#endif

	/* Pass the packet up, with the ether header sort-of removed. */
	m_adj(m, sizeof(struct ether_header));
	ether_input(ifp, eh, m);
}

integrate void
am7990_rint(sc)
	struct am7990_softc *sc;
{
	register int bix;
	int rp;
	struct lermd rmd;

	bix = sc->sc_last_rd;

	/* Process all buffers with valid data. */
	for (;;) {
		rp = LE_RMDADDR(sc, bix);
		(*sc->sc_copyfromdesc)(sc, &rmd, rp, sizeof(rmd));

		if (rmd.rmd1_bits & LE_R1_OWN)
			break;

		if (rmd.rmd1_bits & LE_R1_ERR) {
			if (rmd.rmd1_bits & LE_R1_ENP) {
#ifdef LEDEBUG
				if ((rmd.rmd1_bits & LE_R1_OFLO) == 0) {
					if (rmd.rmd1_bits & LE_R1_FRAM)
						printf("%s: framing error\n",
						    sc->sc_dev.dv_xname);
					if (rmd.rmd1_bits & LE_R1_CRC)
						printf("%s: crc mismatch\n",
						    sc->sc_dev.dv_xname);
				}
#endif
			} else {
				if (rmd.rmd1_bits & LE_R1_OFLO)
					printf("%s: overflow\n",
					    sc->sc_dev.dv_xname);
			}
			if (rmd.rmd1_bits & LE_R1_BUFF)
				printf("%s: receive buffer error\n",
				    sc->sc_dev.dv_xname);
			ifp->if_ierrors++;
		} else if ((rmd.rmd1_bits & (LE_R1_STP | LE_R1_ENP)) !=
		    (LE_R1_STP | LE_R1_ENP)) {
			printf("%s: dropping chained buffer\n",
			    sc->sc_dev.dv_xname);
			ifp->if_ierrors++;
		} else {
#ifdef LEDEBUG
			if (sc->sc_debug)
				am7990_recv_print(sc, sc->sc_last_rd);
#endif
			am7990_read(sc, LE_RBUFADDR(sc, bix),
			    (int)rmd.rmd3 - 4);
		}

		rmd.rmd1_bits = LE_R1_OWN;
		rmd.rmd2 = -LEBLEN | LE_XMD2_ONES;
		rmd.rmd3 = 0;
		(*sc->sc_copytodesc)(sc, &rmd, rp, sizeof(rmd));

#ifdef LEDEBUG
		if (sc->sc_debug)
			printf("sc->sc_last_rd = %x, rmd: "
			       "ladr %04x, hadr %02x, flags %02x, "
			       "bcnt %04x, mcnt %04x\n",
				sc->sc_last_rd,
				rmd.rmd0, rmd.rmd1_hadr, rmd.rmd1_bits,
				rmd.rmd2, rmd.rmd3);
#endif

		if (++bix == sc->sc_nrbuf)
			bix = 0;
	}

	sc->sc_last_rd = bix;
}

integrate void
am7990_tint(sc)
	register struct am7990_softc *sc;
{
	register int bix;
	struct letmd tmd;

	bix = sc->sc_first_td;

	for (;;) {
		if (sc->sc_no_td <= 0)
			break;

#ifdef LEDEBUG
		if (sc->sc_debug)
			printf("trans tmd: "
			    "ladr %04x, hadr %02x, flags %02x, "
			    "bcnt %04x, mcnt %04x\n",
			    tmd.tmd0, tmd.tmd1_hadr, tmd.tmd1_bits,
			    tmd.tmd2, tmd.tmd3);
#endif

		(*sc->sc_copyfromdesc)(sc, &tmd, LE_TMDADDR(sc, bix),
		    sizeof(tmd));

		if (tmd.tmd1_bits & LE_T1_OWN)
			break;

		ifp->if_flags &= ~IFF_OACTIVE;

		if (tmd.tmd1_bits & LE_T1_ERR) {
			if (tmd.tmd3 & LE_T3_BUFF)
				printf("%s: transmit buffer error\n",
				    sc->sc_dev.dv_xname);
			else if (tmd.tmd3 & LE_T3_UFLO)
				printf("%s: underflow\n", sc->sc_dev.dv_xname);
			if (tmd.tmd3 & (LE_T3_BUFF | LE_T3_UFLO)) {
				am7990_reset(sc);
				return;
			}
			if (tmd.tmd3 & LE_T3_LCAR) {
				sc->sc_havecarrier = 0;
				if (sc->sc_nocarrier)
					(*sc->sc_nocarrier)(sc);
				else
					printf("%s: lost carrier\n",
					    sc->sc_dev.dv_xname);
			}
			if (tmd.tmd3 & LE_T3_LCOL)
				ifp->if_collisions++;
			if (tmd.tmd3 & LE_T3_RTRY) {
				printf("%s: excessive collisions, tdr %d\n",
				    sc->sc_dev.dv_xname,
				    tmd.tmd3 & LE_T3_TDR_MASK);
				ifp->if_collisions += 16;
			}
			ifp->if_oerrors++;
		} else {
			if (tmd.tmd1_bits & LE_T1_ONE)
				ifp->if_collisions++;
			else if (tmd.tmd1_bits & LE_T1_MORE)
				/* Real number is unknown. */
				ifp->if_collisions += 2;
			ifp->if_opackets++;
		}

		if (++bix == sc->sc_ntbuf)
			bix = 0;

		--sc->sc_no_td;
	}

	sc->sc_first_td = bix;

	am7990_start(ifp);

	if (sc->sc_no_td == 0)
		ifp->if_timer = 0;
}

/*
 * Controller interrupt.
 */
int
am7990_intr(arg)
	register void *arg;
{
	register struct am7990_softc *sc = arg;
	register u_int16_t isr;

	isr = (*sc->sc_rdcsr)(sc, LE_CSR0) | sc->sc_saved_csr0;
	sc->sc_saved_csr0 = 0;
#ifdef LEDEBUG
	if (sc->sc_debug)
		printf("%s: am7990_intr entering with isr=%04x\n",
		    sc->sc_dev.dv_xname, isr);
#endif
	if ((isr & LE_C0_INTR) == 0)
		return (0);

	(*sc->sc_wrcsr)(sc, LE_CSR0,
	    isr & (LE_C0_INEA | LE_C0_BABL | LE_C0_MISS | LE_C0_MERR |
		   LE_C0_RINT | LE_C0_TINT | LE_C0_IDON));
	if (isr & LE_C0_ERR) {
		if (isr & LE_C0_BABL) {
#ifdef LEDEBUG
			printf("%s: babble\n", sc->sc_dev.dv_xname);
#endif
			ifp->if_oerrors++;
		}
#if 0
		if (isr & LE_C0_CERR) {
			printf("%s: collision error\n", sc->sc_dev.dv_xname);
			ifp->if_collisions++;
		}
#endif
		if (isr & LE_C0_MISS) {
#ifdef LEDEBUG
			printf("%s: missed packet\n", sc->sc_dev.dv_xname);
#endif
			ifp->if_ierrors++;
		}
		if (isr & LE_C0_MERR) {
			printf("%s: memory error\n", sc->sc_dev.dv_xname);
			am7990_reset(sc);
			return (1);
		}
	}

	if ((isr & LE_C0_RXON) == 0) {
		printf("%s: receiver disabled\n", sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		am7990_reset(sc);
		return (1);
	}
	if ((isr & LE_C0_TXON) == 0) {
		printf("%s: transmitter disabled\n", sc->sc_dev.dv_xname);
		ifp->if_oerrors++;
		am7990_reset(sc);
		return (1);
	}

	/*
	 * Pretend we have carrier; if we don't this will be cleared
	 * shortly.
	 */
	sc->sc_havecarrier = 1;

	if (isr & LE_C0_RINT)
		am7990_rint(sc);
	if (isr & LE_C0_TINT)
		am7990_tint(sc);

#if NRND > 0
	rnd_add_uint32(&sc->rnd_source, isr);
#endif

	return (1);
}

#undef	ifp

void
am7990_watchdog(ifp)
	struct ifnet *ifp;
{
	struct am7990_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++ifp->if_oerrors;

	am7990_reset(sc);
}

int
am7990_mediachange(ifp)
	struct ifnet *ifp;
{
	struct am7990_softc *sc = ifp->if_softc;

	if (sc->sc_mediachange)
		return ((*sc->sc_mediachange)(sc));
	return (EINVAL);
}

void
am7990_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct am7990_softc *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_UP) == 0)
		return;

	ifmr->ifm_status = IFM_AVALID;
	if (sc->sc_havecarrier)
		ifmr->ifm_status |= IFM_ACTIVE;

	if (sc->sc_mediastatus)
		(*sc->sc_mediastatus)(sc, ifmr);
}

/*
 * Setup output on interface.
 * Get another datagram to send off of the interface queue, and map it to the
 * interface before starting the output.
 * Called only at splimp or interrupt level.
 */
void
am7990_start(ifp)
	register struct ifnet *ifp;
{
	register struct am7990_softc *sc = ifp->if_softc;
	register int bix;
	register struct mbuf *m;
	struct letmd tmd;
	int rp;
	int len;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	bix = sc->sc_last_td;

	for (;;) {
		rp = LE_TMDADDR(sc, bix);
		(*sc->sc_copyfromdesc)(sc, &tmd, rp, sizeof(tmd));

		if (tmd.tmd1_bits & LE_T1_OWN) {
			ifp->if_flags |= IFF_OACTIVE;
			printf("missing buffer, no_td = %d, last_td = %d\n",
			    sc->sc_no_td, sc->sc_last_td);
		}

		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;

#if NBPFILTER > 0
		/*
		 * If BPF is listening on this interface, let it see the packet
		 * before we commit it to the wire.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/*
		 * Copy the mbuf chain into the transmit buffer.
		 */
		len = am7990_put(sc, LE_TBUFADDR(sc, bix), m);

#ifdef LEDEBUG
		if (len > ETHERMTU + sizeof(struct ether_header))
			printf("packet length %d\n", len);
#endif

		ifp->if_timer = 5;

		/*
		 * Init transmit registers, and set transmit start flag.
		 */
		tmd.tmd1_bits = LE_T1_OWN | LE_T1_STP | LE_T1_ENP;
		tmd.tmd2 = -len | LE_XMD2_ONES;
		tmd.tmd3 = 0;

		(*sc->sc_copytodesc)(sc, &tmd, rp, sizeof(tmd));

#ifdef LEDEBUG
		if (sc->sc_debug)
			am7990_xmit_print(sc, sc->sc_last_td);
#endif

		(*sc->sc_wrcsr)(sc, LE_CSR0, LE_C0_INEA | LE_C0_TDMD);

		if (++bix == sc->sc_ntbuf)
			bix = 0;

		if (++sc->sc_no_td == sc->sc_ntbuf) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

	}

	sc->sc_last_td = bix;
}

/*
 * Process an ioctl request.
 */
int
am7990_ioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	register struct am7990_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splimp();

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			am7990_init(sc);
			arp_ifinit(ifp, ifa);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			register struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)LLADDR(ifp->if_sadl);
			else {
				bcopy(ina->x_host.c_host,
				    LLADDR(ifp->if_sadl),
				    sizeof(sc->sc_enaddr));
			}	
			/* Set new address. */
			am7990_init(sc);
			break;
		    }
#endif
		default:
			am7990_init(sc);
			break;
		}
		break;

#if defined(CCITT) && defined(LLC)
	case SIOCSIFCONF_X25:
		ifp->if_flags |= IFF_UP;
		ifa->ifa_rtrequest = cons_rtrequest; /* XXX */
		error = x25_llcglue(PRC_IFUP, ifa->ifa_addr);
		if (error == 0)
			am7990_init(sc);
		break;
#endif /* CCITT && LLC */

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			am7990_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    	   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			am7990_init(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			/*am7990_stop(sc);*/
			am7990_init(sc);
		}
#ifdef LEDEBUG
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_debug = 1;
		else
			sc->sc_debug = 0;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ethercom) :
		    ether_delmulti(ifr, &sc->sc_ethercom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			am7990_reset(sc);
			error = 0;
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}

hide void
am7990_shutdown(arg)
	void *arg;
{

	am7990_stop((struct am7990_softc *)arg);
}

#ifdef LEDEBUG
void
am7990_recv_print(sc, no)
	struct am7990_softc *sc;
	int no;
{
	struct lermd rmd;
	u_int16_t len;
	struct ether_header eh;

	(*sc->sc_copyfromdesc)(sc, &rmd, LE_RMDADDR(sc, no), sizeof(rmd));
	len = rmd.rmd3;
	printf("%s: receive buffer %d, len = %d\n", sc->sc_dev.dv_xname, no,
	    len);
	printf("%s: status %04x\n", sc->sc_dev.dv_xname,
	    (*sc->sc_rdcsr)(sc, LE_CSR0));
	printf("%s: ladr %04x, hadr %02x, flags %02x, bcnt %04x, mcnt %04x\n",
	    sc->sc_dev.dv_xname,
	    rmd.rmd0, rmd.rmd1_hadr, rmd.rmd1_bits, rmd.rmd2, rmd.rmd3);
	if (len >= sizeof(eh)) {
		(*sc->sc_copyfrombuf)(sc, &eh, LE_RBUFADDR(sc, no), sizeof(eh));
		printf("%s: dst %s", sc->sc_dev.dv_xname,
			ether_sprintf(eh.ether_dhost));
		printf(" src %s type %04x\n", ether_sprintf(eh.ether_shost),
			ntohs(eh.ether_type));
	}
}

void
am7990_xmit_print(sc, no)
	struct am7990_softc *sc;
	int no;
{
	struct letmd tmd;
	u_int16_t len;
	struct ether_header eh;

	(*sc->sc_copyfromdesc)(sc, &tmd, LE_TMDADDR(sc, no), sizeof(tmd));
	len = -tmd.tmd2;
	printf("%s: transmit buffer %d, len = %d\n", sc->sc_dev.dv_xname, no,
	    len);
	printf("%s: status %04x\n", sc->sc_dev.dv_xname,
	    (*sc->sc_rdcsr)(sc, LE_CSR0));
	printf("%s: ladr %04x, hadr %02x, flags %02x, bcnt %04x, mcnt %04x\n",
	    sc->sc_dev.dv_xname,
	    tmd.tmd0, tmd.tmd1_hadr, tmd.tmd1_bits, tmd.tmd2, tmd.tmd3);
	if (len >= sizeof(eh)) {
		(*sc->sc_copyfrombuf)(sc, &eh, LE_TBUFADDR(sc, no), sizeof(eh));
		printf("%s: dst %s", sc->sc_dev.dv_xname,
			ether_sprintf(eh.ether_dhost));
		printf(" src %s type %04x\n", ether_sprintf(eh.ether_shost),
		    ntohs(eh.ether_type));
	}
}
#endif /* LEDEBUG */

/*
 * Set up the logical address filter.
 */
void
am7990_setladrf(ac, af)
	struct ethercom *ac;
	u_int16_t *af;
{
	struct ifnet *ifp = &ac->ec_if;
	struct ether_multi *enm;
	register u_char *cp, c;
	register u_int32_t crc;
	register int i, len;
	struct ether_multistep step;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

	if (ifp->if_flags & IFF_PROMISC)
		goto allmulti;

	af[0] = af[1] = af[2] = af[3] = 0x0000;
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (ETHER_CMP(enm->enm_addrlo, enm->enm_addrhi)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		cp = enm->enm_addrlo;
		crc = 0xffffffff;
		for (len = sizeof(enm->enm_addrlo); --len >= 0;) {
			c = *cp++;
			for (i = 8; --i >= 0;) {
				if ((crc & 0x01) ^ (c & 0x01)) {
					crc >>= 1;
					crc ^= 0xedb88320;
				} else
					crc >>= 1;
				c >>= 1;
			}
		}
		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Set the corresponding bit in the filter. */
		af[crc >> 4] |= 1 << (crc & 0xf);

		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	return;

allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	af[0] = af[1] = af[2] = af[3] = 0xffff;
}


/*
 * Routines for accessing the transmit and receive buffers.
 * The various CPU and adapter configurations supported by this
 * driver require three different access methods for buffers
 * and descriptors:
 *	(1) contig (contiguous data; no padding),
 *	(2) gap2 (two bytes of data followed by two bytes of padding),
 *	(3) gap16 (16 bytes of data followed by 16 bytes of padding).
 */

/*
 * contig: contiguous data with no padding.
 *
 * Buffers may have any alignment.
 */

void
am7990_copytobuf_contig(sc, from, boff, len)
	struct am7990_softc *sc;
	void *from;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;

	/*
	 * Just call bcopy() to do the work.
	 */
	bcopy(from, buf + boff, len);
}

void
am7990_copyfrombuf_contig(sc, to, boff, len)
	struct am7990_softc *sc;
	void *to;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;

	/*
	 * Just call bcopy() to do the work.
	 */
	bcopy(buf + boff, to, len);
}

void
am7990_zerobuf_contig(sc, boff, len)
	struct am7990_softc *sc;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;

	/*
	 * Just let bzero() do the work
	 */
	bzero(buf + boff, len);
}

#if 0
/*
 * Examples only; duplicate these and tweak (if necessary) in
 * machine-specific front-ends.
 */

/*
 * gap2: two bytes of data followed by two bytes of pad.
 *
 * Buffers must be 4-byte aligned.  The code doesn't worry about
 * doing an extra byte.
 */

void
am7990_copytobuf_gap2(sc, fromv, boff, len)
	struct am7990_softc *sc;
	void *fromv;
	int boff;
	register int len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t from = fromv;
	register volatile u_int16_t *bptr;

	if (boff & 0x1) {
		/* handle unaligned first byte */
		bptr = ((volatile u_int16_t *)buf) + (boff - 1);
		*bptr = (*from++ << 8) | (*bptr & 0xff);
		bptr += 2;
		len--;
	} else
		bptr = ((volatile u_int16_t *)buf) + boff;
	while (len > 1) {
		*bptr = (from[1] << 8) | (from[0] & 0xff);
		bptr += 2;
		from += 2;
		len -= 2;
	}
	if (len == 1)
		*bptr = (u_int16_t)*from;
}

void
am7990_copyfrombuf_gap2(sc, tov, boff, len)
	struct am7990_softc *sc;
	void *tov;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t to = tov;
	register volatile u_int16_t *bptr;
	register u_int16_t tmp;

	if (boff & 0x1) {
		/* handle unaligned first byte */
		bptr = ((volatile u_int16_t *)buf) + (boff - 1);
		*to++ = (*bptr >> 8) & 0xff;
		bptr += 2;
		len--;
	} else
		bptr = ((volatile u_int16_t *)buf) + boff;
	while (len > 1) {
		tmp = *bptr;
		*to++ = tmp & 0xff;
		*to++ = (tmp >> 8) & 0xff;
		bptr += 2;
		len -= 2;
	}
	if (len == 1)
		*to = *bptr & 0xff;
}

void
am7990_zerobuf_gap2(sc, boff, len)
	struct am7990_softc *sc;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register volatile u_int16_t *bptr;

	if ((unsigned)boff & 0x1) {
		bptr = ((volatile u_int16_t *)buf) + (boff - 1);
		*bptr &= 0xff;
		bptr += 2;
		len--;
	} else
		bptr = ((volatile u_int16_t *)buf) + boff;
	while (len > 0) {
		*bptr = 0;
		bptr += 2;
		len -= 2;
	}
}

/*
 * gap16: 16 bytes of data followed by 16 bytes of pad.
 *
 * Buffers must be 32-byte aligned.
 */

void
am7990_copytobuf_gap16(sc, fromv, boff, len)
	struct am7990_softc *sc;
	void *fromv;
	int boff;
	register int len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t from = fromv;
	register caddr_t bptr;
	register int xfer;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;
	xfer = min(len, 16 - boff);
	while (len > 0) {
		bcopy(from, bptr + boff, xfer);
		from += xfer;
		bptr += 32;
		boff = 0;
		len -= xfer;
		xfer = min(len, 16);
	}
}

void
am7990_copyfrombuf_gap16(sc, tov, boff, len)
	struct am7990_softc *sc;
	void *tov;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t to = tov;
	register caddr_t bptr;
	register int xfer;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;
	xfer = min(len, 16 - boff);
	while (len > 0) {
		bcopy(bptr + boff, to, xfer);
		to += xfer;
		bptr += 32;
		boff = 0;
		len -= xfer;
		xfer = min(len, 16);
	}
}

void
am7990_zerobuf_gap16(sc, boff, len)
	struct am7990_softc *sc;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	register caddr_t bptr;
	register int xfer;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;
	xfer = min(len, 16 - boff);
	while (len > 0) {
		bzero(bptr + boff, xfer);
		bptr += 32;
		boff = 0;
		len -= xfer;
		xfer = min(len, 16);
	}
}
#endif /* Example only */
