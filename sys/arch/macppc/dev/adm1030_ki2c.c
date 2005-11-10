/*	$NetBSD: adm1030_ki2c.c,v 1.1.6.2 2005/11/10 13:57:27 skrll Exp $	*/

/*-
 * Copyright (C) 2005 Michael Lorenz.
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
 * ki2c attachment for the ADM1030 environmental controller found in some 
 * iBook G3 and probably other Apple machines 
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: adm1030_ki2c.c,v 1.1.6.2 2005/11/10 13:57:27 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <dev/ofw/openfirm.h>

#include <machine/autoconf.h>
#include <macppc/dev/ki2cvar.h>

#include <dev/i2c/adm1030var.h>


static void adm1030_ki2c_attach(struct device *, struct device *, void *);
static int adm1030_ki2c_match(struct device *, struct cfdata *, void *);

CFATTACH_DECL(adm1030_ki2c, sizeof(struct adm1030c_softc),
    adm1030_ki2c_match, adm1030_ki2c_attach, NULL, NULL);

static int
adm1030_ki2c_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct ki2c_confargs *ka = aux;
	char compat[32];
	
	if (strcmp(ka->ka_name, "fan") != 0)
		return 0;

	memset(compat, 0, sizeof(compat));
	OF_getprop(ka->ka_node, "compatible", compat, sizeof(compat));
	if (strcmp(compat, "adm1030") != 0)
		return 0;
	
	return 1;
}

static void
adm1030_ki2c_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct adm1030c_softc *sc = (struct adm1030c_softc *)self;
	struct ki2c_confargs *ka = aux;
	int node;

	node = ka->ka_node;
	sc->sc_node = node;
	sc->parent = parent;
	sc->address = ka->ka_addr & 0xfe;
	printf(" ADM1030 thermal monitor and fan controller\n");
	sc->sc_i2c = ka->ka_tag;
	adm1030c_setup(sc);
}
