/*	$NetBSD: opl_wss.c,v 1.1.16.1 2002/01/10 19:55:38 thorpej Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson
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
__KERNEL_RCSID(0, "$NetBSD: opl_wss.c,v 1.1.16.1 2002/01/10 19:55:38 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/select.h>
#include <sys/audioio.h>
#include <sys/midiio.h>

#include <machine/bus.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/ic/oplreg.h>
#include <dev/ic/oplvar.h>

#include <dev/isa/isavar.h>
#include <dev/isa/ad1848var.h>
#include <dev/isa/wssvar.h>

int	opl_wss_match __P((struct device *, struct cfdata *, void *));
void	opl_wss_attach __P((struct device *, struct device *, void *));

struct cfattach opl_wss_ca = {
	sizeof (struct opl_softc), opl_wss_match, opl_wss_attach
};

int
opl_wss_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct audio_attach_args *aa = (struct audio_attach_args *)aux;
	struct wss_softc *ssc = (struct wss_softc *)parent;
	struct opl_softc sc;

	if (aa->type != AUDIODEV_TYPE_OPL || ssc->sc_opl_ioh == 0)
		return (0);
	sc.ioh = ssc->sc_opl_ioh;
	sc.iot = ssc->sc_iot;
	sc.offs = 0;
	return (opl_find(&sc));
}

void
opl_wss_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct wss_softc *ssc = (struct wss_softc *)parent;
	struct opl_softc *sc = (struct opl_softc *)self;

	sc->ioh = ssc->sc_opl_ioh;
	sc->iot = ssc->sc_iot;
	sc->offs = 0;
	strcpy(sc->syn.name, "WSS ");

	opl_attach(sc);
}
