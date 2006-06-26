/*	$NetBSD: timer_jazzio.c,v 1.8.8.1 2006/06/26 12:44:23 yamt Exp $	*/
/*	$OpenBSD: clock.c,v 1.6 1998/10/15 21:30:15 imp Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	from: @(#)clock.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Copyright (c) 1997 Per Fogelstrom.
 * Copyright (c) 1988 University of Utah.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: clock.c 1.18 91/01/21
 *
 *	from: @(#)clock.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: timer_jazzio.c,v 1.8.8.1 2006/06/26 12:44:23 yamt Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/platform.h>

#include <dev/isa/isavar.h>

#include <arc/arc/timervar.h>
#include <arc/jazz/jazziovar.h>
#include <arc/jazz/timer_jazziovar.h>

struct timer_jazzio_softc {
	struct device	sc_dev;
};

/* Definition of the driver for autoconfig. */
int timer_jazzio_match(struct device *, struct cfdata *, void *);
void timer_jazzio_attach(struct device *, struct device *, void *);

CFATTACH_DECL(timer_jazzio, sizeof(struct timer_jazzio_softc),
    timer_jazzio_match, timer_jazzio_attach, NULL, NULL);

/* Jazz timer access code */
void timer_jazzio_init(struct device *sc);

struct timerfns timerfns_jazzio = {
	timer_jazzio_init,
};

struct timer_jazzio_config *timer_jazzio_conf = NULL;
int timer_jazzio_found = 0;
struct evcnt timer_jazzio_ev =
    EVCNT_INITIALIZER(EVCNT_TYPE_INTR, NULL, "jazzio", "timer");

int
timer_jazzio_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct jazzio_attach_args *ja = aux;

	/* make sure that we're looking for this type of device. */
	if (strcmp(ja->ja_name, "timer") != 0)
		return 0;

	if (timer_jazzio_found)
		return 0;

	return 1;
}

void
timer_jazzio_attach(struct device *parent, struct device *self, void *aux)
{
	struct timer_jazzio_softc *sc = (struct timer_jazzio_softc *)self;

	if (timer_jazzio_conf == NULL)
		panic("timer_jazzio_conf isn't initialized");

	printf("\n");

	evcnt_attach_static(&timer_jazzio_ev);
	(*platform->set_intr)(timer_jazzio_conf->tjc_intr_mask,
	    timer_jazzio_conf->tjc_intr, ARC_INTPRI_TIMER_INT);

	timerattach(&sc->sc_dev, &timerfns_jazzio);

	timer_jazzio_found = 1;
}

void
timer_jazzio_init(struct device *sc)
{

	(*timer_jazzio_conf->tjc_init)(1000 / hz);
}
