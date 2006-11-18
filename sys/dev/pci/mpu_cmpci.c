/*	$NetBSD: mpu_cmpci.c,v 1.10.20.1 2006/11/18 21:34:31 ad Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@NetBSD.org).
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
__KERNEL_RCSID(0, "$NetBSD: mpu_cmpci.c,v 1.10.20.1 2006/11/18 21:34:31 ad Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/audioio.h>
#include <sys/midiio.h>

#include <machine/bus.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>

#include <dev/pci/pcivar.h>

#include <dev/ic/mpuvar.h>
#include <dev/pci/cmpcivar.h>

static int
mpu_cmpci_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct audio_attach_args *aa = (struct audio_attach_args *)aux;
	struct cmpci_softc *ysc = (struct cmpci_softc *)parent;
	struct mpu_softc sc;

	if (aa->type != AUDIODEV_TYPE_MPU)
		return (0);
	memset(&sc, 0, sizeof sc);
	sc.iot = ysc->sc_iot;
	sc.ioh = ysc->sc_mpu_ioh;
	return (mpu_find(&sc));
}

static void
mpu_cmpci_attach(struct device *parent, struct device *self, void *aux)
{
	struct cmpci_softc *ysc = (struct cmpci_softc *)parent;
	struct mpu_softc *sc = (struct mpu_softc *)self;

	printf("\n");

	sc->iot = ysc->sc_iot;
	sc->ioh = ysc->sc_mpu_ioh;
	sc->model = "CMPCI MPU-401 MIDI UART";

	mpu_attach(sc);
}

CFATTACH_DECL(mpu_cmpci, sizeof (struct mpu_softc),
    mpu_cmpci_match, mpu_cmpci_attach, NULL, NULL);
