/*	$NetBSD: aster.c,v 1.4.8.1 2000/11/20 19:58:27 bouyer Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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

/*
 * zbus ISDN Blaster, ISDN Master driver.
 */

#include <sys/types.h>

#include <sys/conf.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/param.h>

#include <machine/bus.h>
#include <machine/conf.h>

#include <amiga/include/cpu.h>

#include <amiga/amiga/device.h>
#include <amiga/amiga/drcustom.h>

#include <amiga/dev/supio.h>
#include <amiga/dev/zbusvar.h>


struct aster_softc {
	struct device sc_dev;
	struct bus_space_tag sc_bst;
};

int astermatch __P((struct device *, struct cfdata *, void *));
void asterattach __P((struct device *, struct device *, void *));
int asterprint __P((void *auxp, const char *));

struct cfattach aster_ca = {
	sizeof(struct aster_softc), astermatch, asterattach
};

int
astermatch(parent, cfp, auxp)
	struct device *parent;
	struct cfdata *cfp;
	void *auxp;
{

	struct zbus_args *zap;

	zap = auxp;

	if (zap->manid == 5001 && zap->prodid == 1)	/* VMC ISDN Blaster */
		return (1);

	if (zap->manid == 2092 && zap->prodid == 64)	/* BSC ISDN Master */
		return (1);

	if (zap->manid == 5000 && zap->prodid == 1)	/* ITH ISDN Master II */
		return (1);

	return (0);
}

void
asterattach(parent, self, auxp)
	struct device *parent, *self;
	void *auxp;
{
	struct aster_softc *astrsc;
	struct zbus_args *zap;
	struct supio_attach_args supa;
	char *cardname;

	astrsc = (struct aster_softc *)self;
	zap = auxp;

	if (zap->manid == 5001 && zap->prodid == 1) {
		cardname = "Blaster";
		supa.supio_name = "isic";
	} else if (zap->manid == 2092 && zap->prodid == 64) {
		cardname = "Master";
		supa.supio_name = "isic";
	} else /* if (zap->manid == 5000 && zap->prodid == 1) */ {
		cardname = "Master II";
		supa.supio_name = "isicII";
	}
	if (parent)
		printf(": ISDN %s\n", cardname);

	astrsc->sc_bst.base = (u_long)zap->va + 0;
	astrsc->sc_bst.absm = &amiga_bus_stride_2;

	supa.supio_iot = &astrsc->sc_bst;

	supa.supio_iobase = 0;
	supa.supio_arg = 0;
	supa.supio_ipl = 2;	/* could be 6. isic_supio will decide. */
	config_found(self, &supa, asterprint); /* XXX */
#ifdef __notyet__
	hyper3i_attach_subr(self, &supa, asterprint);
#endif
}

int
asterprint(auxp, pnp)
	void *auxp;
	const char *pnp;
{
	struct supio_attach_args *supa;
	supa = auxp;

	if (pnp == NULL)
		return(QUIET);

	printf("%s at %s port 0x%04x",
	    supa->supio_name, pnp, supa->supio_iobase);

	return(UNCONF);
}
