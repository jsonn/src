/*	$NetBSD: midi_pcppi.c,v 1.19.8.1 2009/04/28 07:35:46 skrll Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: midi_pcppi.c,v 1.19.8.1 2009/04/28 07:35:46 skrll Exp $");

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

#include <sys/bus.h>

#include <dev/isa/pcppivar.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/midivar.h>
#include <dev/midisynvar.h>

#define MAX_DURATION 30		/* turn off sound automagically after 30 s */

struct midi_pcppi_softc {
	struct midi_softc sc_mididev;
	midisyn sc_midisyn;
};

static int	midi_pcppi_match(device_t, cfdata_t , void *);
static void	midi_pcppi_attach(device_t, device_t, void *);
static int	midi_pcppi_detach(device_t, int);

void	midi_pcppi_on   (midisyn *, uint_fast16_t, midipitch_t, int16_t);
void	midi_pcppi_off  (midisyn *, uint_fast16_t, uint_fast8_t);
void	midi_pcppi_close(midisyn *);
static void midi_pcppi_repitchv(midisyn *, uint_fast16_t, midipitch_t);

CFATTACH_DECL3_NEW(midi_pcppi, sizeof(struct midi_pcppi_softc),
    midi_pcppi_match, midi_pcppi_attach, midi_pcppi_detach, NULL, NULL, NULL,
    DVF_DETACH_SHUTDOWN);

struct midisyn_methods midi_pcppi_hw = {
	.close    = midi_pcppi_close,
	.attackv  = midi_pcppi_on,
	.releasev = midi_pcppi_off,
	.repitchv = midi_pcppi_repitchv,
};

int midi_pcppi_attached = 0;	/* Not very nice */

static int
midi_pcppi_match(device_t parent, cfdata_t match, void *aux)
{
	return (!midi_pcppi_attached);
}

static void
midi_pcppi_attach(device_t parent, device_t self, void *aux)
{
	struct midi_pcppi_softc *sc = device_private(self);
	struct pcppi_attach_args *pa = (struct pcppi_attach_args *)aux;
	midisyn *ms;

	sc->sc_mididev.dev = self;
	ms = &sc->sc_midisyn;
	ms->mets = &midi_pcppi_hw;
	strcpy(ms->name, "PC speaker");
	ms->nvoice = 1;
	ms->data = pa->pa_cookie;

	midi_pcppi_attached++;

	midisyn_attach(&sc->sc_mididev, ms);
	midi_attach(&sc->sc_mididev, parent);
        if (!device_pmf_is_registered(self))
		if (!pmf_device_register(self, NULL, NULL))
			aprint_error_dev(self,
			    "couldn't establish power handler\n"); 
}

static int
midi_pcppi_detach(device_t self, int flags)
{
	KASSERT(midi_pcppi_attached > 0);

	midi_pcppi_attached--;
	return mididetach(self, flags);
} 

void
midi_pcppi_on(midisyn *ms,
    uint_fast16_t voice, midipitch_t mp, int16_t level)
{
	pcppi_tag_t t = ms->data;

	pcppi_bell(t,
	           MIDIHZ18_TO_HZ(MIDIPITCH_TO_HZ18(mp)),
	           MAX_DURATION * hz, 0);
}

void
midi_pcppi_off(midisyn *ms, uint_fast16_t voice, uint_fast8_t vel)
{
	pcppi_tag_t t = ms->data;

	/*printf("OFF %p %d\n", t, note >> 16);*/
	pcppi_bell(t, 0, 0, 0);
}

void
midi_pcppi_close(midisyn *ms)
{
	pcppi_tag_t t = ms->data;

	/* Make sure we are quiet. */
	pcppi_bell(t, 0, 0, 0);
}

static void
midi_pcppi_repitchv(midisyn *ms, uint_fast16_t voice, midipitch_t newpitch)
{
	midi_pcppi_on(ms, voice, newpitch, 64);
}
