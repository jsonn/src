/*	$NetBSD: if_le.c,v 1.22.6.2 1997/03/10 16:00:44 is Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <machine/cpu.h>
#include <machine/mtpr.h>

#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <amiga/dev/zbusvar.h>
#include <amiga/dev/if_levar.h>

/* offsets for:	   ID,   REGS,    MEM */
int	lestd[] = { 0, 0x4000, 0x8000 };

int le_zbus_match __P((struct device *, struct cfdata *, void *));
void le_zbus_attach __P((struct device *, struct device *, void *));

struct cfattach le_zbus_ca = {
	sizeof(struct le_softc), le_zbus_match, le_zbus_attach
};

hide void lewrcsr __P((struct am7990_softc *, u_int16_t, u_int16_t));
hide u_int16_t lerdcsr __P((struct am7990_softc *, u_int16_t));

hide void
lewrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

hide u_int16_t
lerdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	struct lereg1 *ler1 = ((struct le_softc *)sc)->sc_r1;
	u_int16_t val;

	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
	return (val);
}

int
le_zbus_match(parent, cfp, aux)
	struct device *parent;
	struct cfdata *cfp;
	void *aux;
{
	struct zbus_args *zap = aux;

	/* Commodore ethernet card */
	if (zap->manid == 514 && zap->prodid == 112)
		return (1);

	/* Ameristar ethernet card */
	if (zap->manid == 1053 && zap->prodid == 1)
		return (1);

	return (0);
}

void
le_zbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct le_softc *lesc = (struct le_softc *)self;
	struct am7990_softc *sc = &lesc->sc_am7990;
	struct zbus_args *zap = aux;
	u_long ser;
	u_int8_t myaddr[ETHER_ADDR_LEN];

	lesc->sc_r1 = (struct lereg1 *)(lestd[1] + (int)zap->va);
	sc->sc_mem = (void *)(lestd[2] + (int)zap->va);

	sc->sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_zerobuf = am7990_zerobuf_contig;

	sc->sc_rdcsr = lerdcsr;
	sc->sc_wrcsr = lewrcsr;
	sc->sc_hwinit = NULL;

	sc->sc_conf3 = LE_C3_BSWP;
	sc->sc_addr = 0x8000;

	/*
	 * Manufacturer decides the 3 first bytes, i.e. ethernet vendor ID.
	 */
	switch (zap->manid) {
	case 514:
		/* Commodore */
		sc->sc_memsize = 32768;
		sc->sc_enaddr[0] = 0x00;
		sc->sc_enaddr[1] = 0x80;
		sc->sc_enaddr[2] = 0x10;
		break;

	case 1053:
		/* Ameristar */
		sc->sc_memsize = 32768;
		sc->sc_enaddr[0] = 0x00;
		sc->sc_enaddr[1] = 0x00;
		sc->sc_enaddr[2] = 0x9f;
		break;

	default:
		panic("le_zbus_attach: bad manid");
	}

	/*
	 * Serial number for board is used as host ID.
	 */
	ser = (u_long)zap->serno;
	sc->sc_enaddr[3] = (ser >> 16) & 0xff;
	sc->sc_enaddr[4] = (ser >>  8) & 0xff;
	sc->sc_enaddr[5] = (ser      ) & 0xff;

	am7990_config(sc);

	lesc->sc_isr.isr_intr = am7990_intr;
	lesc->sc_isr.isr_arg = sc;
	lesc->sc_isr.isr_ipl = 2;
	add_isr(&lesc->sc_isr);
}
