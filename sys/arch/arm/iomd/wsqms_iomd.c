/*	$NetBSD: wsqms_iomd.c,v 1.2.10.1 2002/07/16 00:55:31 gehenna Exp $	*/

/*-
 * Copyright (c) 2001 Reinoud Zandijk
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Reinoud Zandijk
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
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
 *
 *
 * Quadrature mouse driver for the wscons as used in the IOMD; config glue...
 */


#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: wsqms_iomd.c,v 1.2.10.1 2002/07/16 00:55:31 gehenna Exp $");

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/select.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/iomd/wsqmsvar.h>
#include <arm/iomd/iomdvar.h>

static int  wsqms_iomd_probe(struct device *, struct cfdata *, void *);
static void wsqms_iomd_attach(struct device *, struct device *, void *);


struct cfattach wsqms_iomd_ca = {
	sizeof(struct wsqms_softc), wsqms_iomd_probe, wsqms_iomd_attach
};


static int
wsqms_iomd_probe(struct device *parent, struct cfdata *cf, void *aux)
{
	struct qms_attach_args *qa = aux;

	if (strcmp(qa->qa_name, "qms") == 0)
		return(1);

	return(0);
}


static void
wsqms_iomd_attach(struct device *parent, struct device *self, void *aux)
{
	struct wsqms_softc *sc = (void *)self;
	struct qms_attach_args *qa = aux;

	sc->sc_iot = qa->qa_iot;
	sc->sc_ioh = qa->qa_ioh;
	sc->sc_butioh = qa->qa_ioh_but;

	wsqms_attach(sc);
}

/* End of wsqms_iomd.c */
