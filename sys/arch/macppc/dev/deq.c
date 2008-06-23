/*	$NetBSD: deq.c,v 1.3.50.1 2008/06/23 04:30:31 wrstuden Exp $	*/

/*-
 * Copyright (C) 2005 Michael Lorenz
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * a dummy device to attach to OF's deq node marking the TAS3004 audio mixer /
 * equalizer chip, needed by snapper
 */
 
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: deq.c,v 1.3.50.1 2008/06/23 04:30:31 wrstuden Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <dev/ofw/openfirm.h>
#include <dev/i2c/i2cvar.h>

#include <machine/autoconf.h>
#include <macppc/dev/ki2cvar.h>
#include <macppc/dev/deqvar.h>

static void deq_attach(device_t, device_t, void *);
static int deq_match(device_t, struct cfdata *, void *);

CFATTACH_DECL_NEW(deq, sizeof(struct deq_softc),
    deq_match, deq_attach, NULL, NULL);

int
deq_match(parent, cf, aux)
	device_t parent;
	struct cfdata *cf;
	void *aux;
{
	struct ki2c_confargs *ka = aux;
	char compat[32];
	
	if (strcmp(ka->ka_name, "deq") != 0)
		return 0;

	memset(compat, 0, sizeof(compat));
	if(OF_getprop(ka->ka_node, "i2c-address", compat, sizeof(compat)))
		return 1;
	return 0;
}

void
deq_attach(parent, self, aux)
	device_t parent, self;
	void *aux;
{
	struct deq_softc *sc = device_private(self);
	struct ki2c_confargs *ka = aux;
	int node;

	sc->sc_dev = self;
	node = ka->ka_node;
	sc->sc_node = node;
	sc->sc_parent = parent;
	sc->sc_address = ka->ka_addr & 0xfe;
	sc->sc_i2c = ka->ka_tag;
	printf(" Apple Digital Equalizer, addr 0x%x\n", sc->sc_address);
}
