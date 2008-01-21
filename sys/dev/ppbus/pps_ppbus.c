/* $NetBSD: pps_ppbus.c,v 1.2.4.4 2008/01/21 09:44:29 yamt Exp $ */

/*
 * ported to timecounters by Frank Kardel 2006
 *
 * Copyright (c) 2004
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pps_ppbus.c,v 1.2.4.4 2008/01/21 09:44:29 yamt Exp $");

#include "opt_ntp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/timepps.h>

#include <dev/ppbus/ppbus_base.h>
#include <dev/ppbus/ppbus_device.h>
#include <dev/ppbus/ppbus_io.h>
#include <dev/ppbus/ppbus_var.h>

struct pps_softc {
	struct ppbus_device_softc pps_dev;
	struct device *ppbus;
	int busy;
	struct pps_state pps_state;	/* pps state */
};

static int pps_probe(struct device *, struct cfdata *, void *);
static void pps_attach(struct device *, struct device *, void *);
CFATTACH_DECL(pps, sizeof(struct pps_softc), pps_probe, pps_attach,
	NULL, NULL);
extern struct cfdriver pps_cd;

static dev_type_open(ppsopen);
static dev_type_close(ppsclose);
static dev_type_ioctl(ppsioctl);
const struct cdevsw pps_cdevsw = {
	ppsopen, ppsclose, noread, nowrite, ppsioctl,
	nostop, notty, nopoll, nommap, nokqfilter, D_OTHER
};

static void ppsintr(void *arg);

static int
pps_probe(struct device *parent, struct cfdata *match, void *aux)
{
	struct ppbus_attach_args *args = aux;

	/* we need an interrupt */
	if (!(args->capabilities & PPBUS_HAS_INTR))
		return 0;

	return 1;
}

static void
pps_attach(struct device *parent, struct device *self, void *aux)
{
	struct pps_softc *sc = device_private(self);

	sc->ppbus = parent;

	printf("\n");
}

static int
ppsopen(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct pps_softc *sc;
	int res, weg = 0;

	sc = device_lookup(&pps_cd, minor(dev));
	if (!sc)
		return (ENXIO);

	if (sc->busy)
		return (0);

	if (ppbus_request_bus(sc->ppbus, &sc->pps_dev.sc_dev,
			      PPBUS_WAIT|PPBUS_INTR, 0))
		return (EINTR);

	ppbus_write_ivar(sc->ppbus, PPBUS_IVAR_IEEE, &weg);

	/* attach the interrupt handler */
	/* XXX priority should be set here */
	res = ppbus_add_handler(sc->ppbus, ppsintr, sc);
	if (res) {
		ppbus_release_bus(sc->ppbus, &sc->pps_dev.sc_dev,
				  PPBUS_WAIT, 0);
		return (res);
	}

	ppbus_set_mode(sc->ppbus, PPBUS_PS2, 0);
	ppbus_wctr(sc->ppbus, IRQENABLE | PCD | nINIT | SELECTIN);

	memset((void *)&sc->pps_state, 0, sizeof(sc->pps_state));
	sc->pps_state.ppscap = PPS_CAPTUREASSERT;
	pps_init(&sc->pps_state);

	sc->busy = 1;
	return (0);
}

static int
ppsclose(dev_t dev, int flags, int fmt, struct lwp *l)
{
	struct pps_softc *sc = device_lookup(&pps_cd, minor(dev));
	struct device *ppbus = sc->ppbus;

	sc->busy = 0;
	sc->pps_state.ppsparam.mode = 0;

	ppbus_wdtr(ppbus, 0);
	ppbus_wctr(ppbus, 0);

	ppbus_remove_handler(ppbus, ppsintr);
	ppbus_set_mode(ppbus, PPBUS_COMPATIBLE, 0);
	ppbus_release_bus(ppbus, &sc->pps_dev.sc_dev, PPBUS_WAIT, 0);
	return (0);
}

static void
ppsintr(void *arg)
{
	struct pps_softc *sc = arg;
	struct device *ppbus = sc->ppbus;

	pps_capture(&sc->pps_state);

	if (!(ppbus_rstr(ppbus) & nACK))
		return;

	if (sc->pps_state.ppsparam.mode & PPS_ECHOASSERT) 
		ppbus_wctr(ppbus, IRQENABLE | AUTOFEED);

	pps_event(&sc->pps_state, PPS_CAPTUREASSERT);

	if (sc->pps_state.ppsparam.mode & PPS_ECHOASSERT) 
		ppbus_wctr(ppbus, IRQENABLE);
}

static int
ppsioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	struct pps_softc *sc = device_lookup(&pps_cd, minor(dev));
	int error = 0;

	switch (cmd) {
	case PPS_IOC_CREATE:
	case PPS_IOC_DESTROY:
	case PPS_IOC_GETPARAMS:
	case PPS_IOC_SETPARAMS:
	case PPS_IOC_GETCAP:
	case PPS_IOC_FETCH:
#ifdef PPS_SYNC
	case PPS_IOC_KCBIND:
#endif
		error = pps_ioctl(cmd, data, &sc->pps_state);
		break;

	default:
		error = EPASSTHROUGH;
		break;
	}
	return (error);
}
