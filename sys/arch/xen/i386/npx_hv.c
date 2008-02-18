/*	$NetBSD: npx_hv.c,v 1.3.56.1 2008/02/18 21:05:19 mjf Exp $	*/

/*
 *
 * Copyright (c) 2004 Christian Limpach.
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
 *      This product includes software developed by Christian Limpach.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npx_hv.c,v 1.3.56.1 2008/02/18 21:05:19 mjf Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/stdarg.h>

#include <xen/xen.h>
#include <xen/hypervisor.h>

#include <i386/isa/npxvar.h>

int npx_hv_probe(struct device *, struct cfdata *, void *);
void npx_hv_attach(struct device *, struct device *, void *);

CFATTACH_DECL(npx_hv, sizeof(struct npx_softc),
    npx_hv_probe, npx_hv_attach, NULL, NULL);

int
npx_hv_probe(struct device *parent, struct cfdata *match, void *aux)
{
	struct xen_npx_attach_args *xa = (struct xen_npx_attach_args *)aux;

	if (strcmp(xa->xa_device, "npx") == 0)
		return 1;
	return 0;
}

void
npx_hv_attach(struct device *parent, struct device *self, void *aux)
{
	struct npx_softc *sc = (void *)self;

	sc->sc_type = NPX_EXCEPTION;

	printf(": using exception 16\n");

	npxattach(sc);
}
