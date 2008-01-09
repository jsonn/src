/*	$NetBSD: hpcout.c,v 1.10.34.2 2008/01/09 01:52:39 matt Exp $	*/

/*-
 * Copyright (c) 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hpcout.c,v 1.10.34.2 2008/01/09 01:52:39 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/config_hook.h>
#include <sys/bus.h>

#include <dev/hpc/hpciovar.h>
#include <dev/hpc/hpciomanvar.h>

#include "locators.h"

int	hpcout_match(struct device *, struct cfdata *, void *);
void	hpcout_attach(struct device *, struct device *, void *);
int	hpcout_hook(void *, int, long, void *);

struct hpcout_softc {
	struct device sc_dev;
	struct hpcioman_attach_args sc_hma;
};

#define sc_hc		sc_hma.hma_hc
#define sc_intr_mode	sc_hma.hma_intr_mode
#define sc_type		sc_hma.hma_type
#define sc_id		sc_hma.hma_id
#define sc_port		sc_hma.hma_port
#define sc_initvalue	sc_hma.hma_initvalue
#define sc_on		sc_hma.hma_on
#define sc_off		sc_hma.hma_off

CFATTACH_DECL(hpcout, sizeof(struct hpcout_softc),
    hpcout_match, hpcout_attach, NULL, NULL);

int
hpcout_match(struct device *parent, struct cfdata *cf, void *aux)
{
	return (1);
}

void
hpcout_attach(struct device *parent, struct device *self, void *aux)
{
	struct hpcioman_attach_args *hma = aux;
	struct hpcout_softc *sc = device_private(self);

	sc->sc_hma = *hma;	/* structure assignment */

	if (hma->hma_hc == NULL ||
	    hma->hma_type == HPCIOMANCF_EVTYPE_DEFAULT ||
	    hma->hma_id == HPCIOMANCF_ID_DEFAULT ||
	    hma->hma_port == HPCIOMANCF_PORT_DEFAULT) {
		printf(": ignored\n");
		return;
	}
	printf("\n");

	if (hma->hma_initvalue != -1)
		hpcio_portwrite(sc->sc_hc, sc->sc_port, hma->hma_initvalue);

	config_hook(sc->sc_type, sc->sc_id, CONFIG_HOOK_SHARE,
	    hpcout_hook, sc);
}

int
hpcout_hook(void *arg, int type, long id, void *msg)
{
	struct hpcout_softc *sc = arg;

	printf("%s: port %d: %s\n", __func__, sc->sc_port,
	    (int)msg ? "on" : "off");
	hpcio_portwrite(sc->sc_hc, sc->sc_port,
	    (int)msg ? sc->sc_on : sc->sc_off);

	return (0);
}
