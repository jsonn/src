/*	$NetBSD: sb_isapnp.c,v 1.34.2.1 1999/12/27 18:34:57 wrstuden Exp $	*/

/*
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/mulaw.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/isa/sbreg.h>
#include <dev/isa/sbvar.h>
#include <dev/isa/sbdspvar.h>

int	sb_isapnp_match __P((struct device *, struct cfdata *, void *));
void	sb_isapnp_attach __P((struct device *, struct device *, void *));

struct cfattach sb_isapnp_ca = {
	sizeof(struct sbdsp_softc), sb_isapnp_match, sb_isapnp_attach
};


/*
 * Probe / attach routines.
 */

/*
 * Probe for the soundblaster hardware.
 */
int
sb_isapnp_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_sb_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return (pri);
}


/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver.
 */
void
sb_isapnp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbdsp_softc *sc = (struct sbdsp_softc *)self;
	struct isapnp_attach_args *ipa = aux;

	printf("\n");

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		printf("%s: error in region allocation\n", 
		       sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_ic = ipa->ipa_ic;

	sc->sc_iot = ipa->ipa_iot;
	sc->sc_ioh = ipa->ipa_io[0].h;

	/* XXX These are only for setting chip configuration registers. */
	sc->sc_iobase = ipa->ipa_io[0].base;
	sc->sc_irq = ipa->ipa_irq[0].num;

	sc->sc_drq8 = ipa->ipa_drq[0].num;
        if (ipa->ipa_ndrq > 1 && ipa->ipa_drq[0].num != ipa->ipa_drq[1].num) {
        	/* Some cards have the 16 bit drq first */
        	if (sc->sc_drq8 >= 4) {
                	sc->sc_drq16 = sc->sc_drq8;
                        sc->sc_drq8 = ipa->ipa_drq[1].num;
                } else
                	sc->sc_drq16 = ipa->ipa_drq[1].num;
        } else
        	sc->sc_drq16 = DRQUNK;

#if NMPU > 0
	if (ipa->ipa_nio > 1) {
		sc->sc_hasmpu = 1;
		sc->sc_mpu_iot = ipa->ipa_iot;
		sc->sc_mpu_ioh = ipa->ipa_io[1].h;
	}
#endif

	if (!sbmatch(sc)) {
		printf("%s: sbmatch failed\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_ih = isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num,
	    ipa->ipa_irq[0].type, IPL_AUDIO, sbdsp_intr, sc);

	printf("%s: %s %s", sc->sc_dev.dv_xname, ipa->ipa_devident,
	       ipa->ipa_devclass);

	sbattach(sc);
}
