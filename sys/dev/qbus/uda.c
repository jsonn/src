/*	$NetBSD: uda.c,v 1.54.6.1 2006/03/28 09:42:14 tron Exp $	*/
/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)uda.c	7.32 (Berkeley) 2/13/91
 */

/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	@(#)uda.c	7.32 (Berkeley) 2/13/91
 */

/*
 * UDA50 disk device driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uda.c,v 1.54.6.1 2006/03/28 09:42:14 tron Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/sid.h>

#include <dev/qbus/ubavar.h>

#include <dev/mscp/mscp.h>
#include <dev/mscp/mscpreg.h>
#include <dev/mscp/mscpvar.h>

#include "ioconf.h"

/*
 * Software status, per controller.
 */
struct	uda_softc {
	struct	device sc_dev;	/* Autoconfig info */
	struct	evcnt sc_intrcnt; /* Interrupt counting */
	struct	uba_unit sc_unit; /* Struct common for UBA to communicate */
	struct	ubinfo sc_ui;
	bus_dma_tag_t		sc_dmat;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_iph;
	bus_space_handle_t	sc_sah;
	struct	mscp_softc *sc_softc;	/* MSCP info (per mscpvar.h) */
	int	sc_inq;
};

static	int udamatch(struct device *, struct cfdata *, void *);
static	void udaattach(struct device *, struct device *, void *);
static	void udareset(struct device *);
static	void udaintr(void *);
static	int udaready(struct uba_unit *);
static	void udactlrdone(struct device *);
static	int udaprint(void *, const char *);
static	void udasaerror(struct device *, int);
static	void udago(struct device *, struct mscp_xi *);

CFATTACH_DECL(mtc, sizeof(struct uda_softc),
    udamatch, udaattach, NULL, NULL);

CFATTACH_DECL(uda, sizeof(struct uda_softc),
    udamatch, udaattach, NULL, NULL);

/*
 * More driver definitions, for generic MSCP code.
 */
struct	mscp_ctlr uda_mscp_ctlr = {
	udactlrdone,
	udago,
	udasaerror,
};

int
udaprint(void *aux, const char *name)
{
	if (name)
		aprint_normal("%s: mscpbus", name);
	return UNCONF;
}

/*
 * Poke at a supposed UDA50 to see if it is there.
 */
int
udamatch(struct device *parent, struct cfdata *cf, void *aux)
{
	struct	uba_attach_args *ua = aux;
	struct	mscp_softc mi;	/* Nice hack */
	struct	uba_softc *ubasc;
	int	tries;

	/* Get an interrupt vector. */
	ubasc = (void *)parent;

	mi.mi_iot = ua->ua_iot;
	mi.mi_iph = ua->ua_ioh;
	mi.mi_sah = ua->ua_ioh + 2;
	mi.mi_swh = ua->ua_ioh + 2;

	/*
	 * Initialise the controller (partially).  The UDA50 programmer's
	 * manual states that if initialisation fails, it should be retried
	 * at least once, but after a second failure the port should be
	 * considered `down'; it also mentions that the controller should
	 * initialise within ten seconds.  Or so I hear; I have not seen
	 * this manual myself.
	 */
	tries = 0;
again:

	bus_space_write_2(mi.mi_iot, mi.mi_iph, 0, 0); /* Start init */
	if (mscp_waitstep(&mi, MP_STEP1, MP_STEP1) == 0)
		return 0; /* Nothing here... */

	bus_space_write_2(mi.mi_iot, mi.mi_sah, 0,
	    MP_ERR | (NCMDL2 << 11) | (NRSPL2 << 8) | MP_IE |
	    ((ubasc->uh_lastiv - 4) >> 2));

	if (mscp_waitstep(&mi, MP_STEP2, MP_STEP2) == 0) {
		printf("udaprobe: init step2 no change. sa=%x\n",
		    bus_space_read_2(mi.mi_iot, mi.mi_sah, 0));
		goto bad;
	}

	/* should have interrupted by now */
	return 1;
bad:
	if (++tries < 2)
		goto again;
	return 0;
}

void
udaattach(struct device *parent, struct device *self, void *aux)
{
	struct	uda_softc *sc = (void *)self;
	struct	uba_attach_args *ua = aux;
	struct	uba_softc *uh = (void *)parent;
	struct	mscp_attach_args ma;
	int	error;

	printf("\n");

	uh->uh_lastiv -= 4;	/* remove dynamic interrupt vector */

	uba_intr_establish(ua->ua_icookie, ua->ua_cvec,
		udaintr, sc, &sc->sc_intrcnt);
	uba_reset_establish(udareset, &sc->sc_dev);
	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, ua->ua_evcnt,
		sc->sc_dev.dv_xname, "intr");

	sc->sc_iot = ua->ua_iot;
	sc->sc_iph = ua->ua_ioh;
	sc->sc_sah = ua->ua_ioh + 2;
	sc->sc_dmat = ua->ua_dmat;

	/*
	 * Fill in the uba_unit struct, so we can communicate with the uba.
	 */
	sc->sc_unit.uu_softc = sc;	/* Backpointer to softc */
	sc->sc_unit.uu_ready = udaready;/* go routine called from adapter */
	sc->sc_unit.uu_keepbdp = vax_cputype == VAX_750 ? 1 : 0;

	/*
	 * Map the communication area and command and
	 * response packets into Unibus space.
	 */
	sc->sc_ui.ui_size = sizeof(struct mscp_pack);
	if ((error = ubmemalloc((void *)parent, &sc->sc_ui, UBA_CANTWAIT)))
		return printf("ubmemalloc failed: %d\n", error);

	bzero(sc->sc_ui.ui_vaddr, sizeof (struct mscp_pack));

	/*
	 * The only thing that differ UDA's and Tape ctlr's is
	 * their vcid. Beacuse there are no way to determine which
	 * ctlr type it is, we check what is generated and later
	 * set the correct vcid.
	 */
	ma.ma_type = (device_is_a(self, "mtc") ? MSCPBUS_TAPE : MSCPBUS_DISK);

	ma.ma_mc = &uda_mscp_ctlr;
	ma.ma_type |= MSCPBUS_UDA;
	ma.ma_uda = (struct mscp_pack *)sc->sc_ui.ui_vaddr;
	ma.ma_softc = &sc->sc_softc;
	ma.ma_iot = sc->sc_iot;
	ma.ma_iph = sc->sc_iph;
	ma.ma_sah = sc->sc_sah;
	ma.ma_swh = sc->sc_sah;
	ma.ma_dmat = sc->sc_dmat;
	ma.ma_dmam = sc->sc_ui.ui_dmam;
	ma.ma_ivec = uh->uh_lastiv;
	ma.ma_ctlrnr = (ua->ua_iaddr == 0172150 ? 0 : 1);	/* XXX */
	ma.ma_adapnr = uh->uh_nr;
	config_found(&sc->sc_dev, &ma, udaprint);
}

/*
 * Start a transfer if there are free resources available, otherwise
 * let it go in udaready, forget it for now.
 * Called from mscp routines.
 */
void
udago(struct device *usc, struct mscp_xi *mxi)
{
	struct uda_softc *sc = (void *)usc;
	struct uba_unit *uu;
	struct buf *bp = mxi->mxi_bp;
	int err;

	/*
	 * If we already have transfers queued, don't try to load
	 * the map again.
	 */
	if (sc->sc_inq == 0) {
		err = bus_dmamap_load(sc->sc_dmat, mxi->mxi_dmam,
		    bp->b_data, bp->b_bcount,
		    (bp->b_flags & B_PHYS ? bp->b_proc : 0), BUS_DMA_NOWAIT);
		if (err == 0) {
			mscp_dgo(sc->sc_softc, mxi);
			return;
		}
	}
	uu = malloc(sizeof(struct uba_unit), M_DEVBUF, M_NOWAIT);
	if (uu == 0)
		panic("udago: no mem");
	uu->uu_ready = udaready;
	uu->uu_softc = sc;
	uu->uu_ref = mxi;
	uba_enqueue(uu);
	sc->sc_inq++;
}

/*
 * Called if we have been blocked for resources, and resources
 * have been freed again. Return 1 if we could start all
 * transfers again, 0 if we still are waiting.
 * Called from uba resource free routines.
 */
int
udaready(struct uba_unit *uu)
{
	struct uda_softc *sc = uu->uu_softc;
	struct mscp_xi *mxi = uu->uu_ref;
	struct buf *bp = mxi->mxi_bp;
	int err;

	err = bus_dmamap_load(sc->sc_dmat, mxi->mxi_dmam, bp->b_data,
	    bp->b_bcount, (bp->b_flags & B_PHYS ? bp->b_proc : 0),
	    BUS_DMA_NOWAIT);

	if (err)
		return 0;
	mscp_dgo(sc->sc_softc, mxi);
	sc->sc_inq--;
	free(uu, M_DEVBUF);
	return 1;
}

static struct saerr {
	int	code;		/* error code (including UDA_ERR) */
	const char	*desc;		/* what it means: Efoo => foo error */
} saerr[] = {
	{ 0100001, "Eunibus packet read" },
	{ 0100002, "Eunibus packet write" },
	{ 0100003, "EUDA ROM and RAM parity" },
	{ 0100004, "EUDA RAM parity" },
	{ 0100005, "EUDA ROM parity" },
	{ 0100006, "Eunibus ring read" },
	{ 0100007, "Eunibus ring write" },
	{ 0100010, " unibus interrupt master failure" },
	{ 0100011, "Ehost access timeout" },
	{ 0100012, " host exceeded command limit" },
	{ 0100013, " unibus bus master failure" },
	{ 0100014, " DM XFC fatal error" },
	{ 0100015, " hardware timeout of instruction loop" },
	{ 0100016, " invalid virtual circuit id" },
	{ 0100017, "Eunibus interrupt write" },
	{ 0104000, "Efatal sequence" },
	{ 0104040, " D proc ALU" },
	{ 0104041, "ED proc control ROM parity" },
	{ 0105102, "ED proc w/no BD#2 or RAM parity" },
	{ 0105105, "ED proc RAM buffer" },
	{ 0105152, "ED proc SDI" },
	{ 0105153, "ED proc write mode wrap serdes" },
	{ 0105154, "ED proc read mode serdes, RSGEN & ECC" },
	{ 0106040, "EU proc ALU" },
	{ 0106041, "EU proc control reg" },
	{ 0106042, " U proc DFAIL/cntl ROM parity/BD #1 test CNT" },
	{ 0106047, " U proc const PROM err w/D proc running SDI test" },
	{ 0106055, " unexpected trap" },
	{ 0106071, "EU proc const PROM" },
	{ 0106072, "EU proc control ROM parity" },
	{ 0106200, "Estep 1 data" },
	{ 0107103, "EU proc RAM parity" },
	{ 0107107, "EU proc RAM buffer" },
	{ 0107115, " test count wrong (BD 12)" },
	{ 0112300, "Estep 2" },
	{ 0122240, "ENPR" },
	{ 0122300, "Estep 3" },
	{ 0142300, "Estep 4" },
	{ 0, " unknown error code" }
};

/*
 * If the error bit was set in the controller status register, gripe,
 * then (optionally) reset the controller and requeue pending transfers.
 */
void
udasaerror(struct device *usc, int doreset)
{
	struct	uda_softc *sc = (void *)usc;
	int code = bus_space_read_2(sc->sc_iot, sc->sc_sah, 0);
	struct saerr *e;

	if ((code & MP_ERR) == 0)
		return;
	for (e = saerr; e->code; e++)
		if (e->code == code)
			break;
	printf("%s: controller error, sa=0%o (%s%s)\n",
		sc->sc_dev.dv_xname, code, e->desc + 1,
		*e->desc == 'E' ? " error" : "");
#if 0 /* XXX we just avoid panic when autoconfig non-existent KFQSA devices */
	if (doreset) {
		mscp_requeue(sc->sc_softc);
/*		(void) udainit(sc);	XXX */
	}
#endif
}

/*
 * Interrupt routine.  Depending on the state of the controller,
 * continue initialisation, or acknowledge command and response
 * interrupts, and process responses.
 */
static void
udaintr(void *arg)
{
	struct uda_softc *sc = arg;
	struct uba_softc *uh;

	/* ctlr fatal error */
	if (bus_space_read_2(sc->sc_iot, sc->sc_sah, 0) & MP_ERR) {
		udasaerror(&sc->sc_dev, 1);
		return;
	}
	/*
	 * Handle buffer purge requests.
	 * XXX - should be done in bus_dma_sync().
	 */
	uh = (void *)device_parent(&sc->sc_dev);
#ifdef notyet
	if (ud->mp_ca.ca_bdp) {
		if (uh->uh_ubapurge)
			(*uh->uh_ubapurge)(uh, ud->mp_ca.ca_bdp);
		/* signal purge complete */
		bus_space_write_2(sc->sc_iot, sc->sc_sah, 0, 0);
	}
#endif

	mscp_intr(sc->sc_softc);
}

/*
 * A Unibus reset has occurred on UBA uban.  Reinitialise the controller(s)
 * on that Unibus, and requeue outstanding I/O.
 */
static void
udareset(struct device *dev)
{
	struct uda_softc *sc = (void *)dev;
	/*
	 * Our BDP (if any) is gone; our command (if any) is
	 * flushed; the device is no longer mapped; and the
	 * UDA50 is not yet initialised.
	 */
	if (sc->sc_unit.uu_bdp) {
		/* printf("<%d>", UBAI_BDP(sc->sc_unit.uu_bdp)); */
		sc->sc_unit.uu_bdp = 0;
	}

	/* reset queues and requeue pending transfers */
	mscp_requeue(sc->sc_softc);

	/*
	 * If it fails to initialise we will notice later and
	 * try again (and again...).  Do not call udastart()
	 * here; it will be done after the controller finishes
	 * initialisation.
	 */
/* XXX	if (udainit(sc)) */
		printf(" (hung)");
}

void
udactlrdone(struct device *usc)
{
	struct uda_softc *sc = (void *)usc;
	int s;

	s = spluba();
	uba_done((struct uba_softc *)device_parent(&sc->sc_dev));
	splx(s);
}
