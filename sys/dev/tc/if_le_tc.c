/*	$NetBSD: if_le_tc.c,v 1.3.6.1 1997/03/09 21:12:43 is Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * LANCE on TurboChannel.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <dev/tc/if_levar.h>
#include <dev/tc/tcvar.h>

#ifdef __BROKEN_INDIRECT_CONFIG
int	le_tc_match __P((struct device *, void *, void *));
#else
int	le_tc_match __P((struct device *, struct cfdata *, void *));
#endif
void	le_tc_attach __P((struct device *, struct device *, void *));

struct cfattach le_tc_ca = {
	sizeof(struct le_softc), le_tc_match, le_tc_attach
};

#define	LE_OFFSET_RAM		0x0
#define	LE_OFFSET_LANCE		0x100000
#define	LE_OFFSET_ROM		0x1c0000

int
le_tc_match(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct tc_attach_args *d = aux;

	if (strncmp("PMAD-AA ", d->ta_modname, TC_ROM_LLEN) &&
	    strncmp("PMAD-BA ", d->ta_modname, TC_ROM_LLEN))
		return (0);

	return (1);
}

void
le_tc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct le_softc *lesc = (void *)self;
	register struct am7990_softc *sc = &lesc->sc_am7990;
	struct tc_attach_args *d = aux;

	/*
	 * It's on the turbochannel proper, or a kn02
	 * baseboard implementation of a TC option card.
	 */
	lesc->sc_r1 = (struct lereg1 *)(d->ta_addr + LE_OFFSET_LANCE);
	sc->sc_mem = (void *)(d->ta_addr + LE_OFFSET_RAM);

	sc->sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_zerobuf = am7990_zerobuf_contig;

	/*
	 * TC lance boards have onboard SRAM buffers.  DMA
	 * between the onbard RAM and main memory is not possible,
	 * so  DMA setup is not required.
	 */

	dec_le_common_attach(sc, (u_char *)(d->ta_addr + LE_OFFSET_ROM + 2));

	tc_intr_establish(parent, d->ta_cookie, TC_IPL_NET, am7990_intr, sc);
}
