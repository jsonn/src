/*	$NetBSD: phantomas.c,v 1.1.4.4 2004/09/21 13:15:40 skrll Exp $	*/
/*	$OpenBSD: phantomas.c,v 1.1 2002/12/18 23:52:45 mickey Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>

#include <hp700/dev/cpudevs.h>

struct phantomas_softc {
	struct device sc_dev;
};

int	phantomasmatch(struct device *, struct cfdata *, void *);
void	phantomasattach(struct device *, struct device *, void *);
static void phantomas_callback(struct device *self, struct confargs *ca);

CFATTACH_DECL(phantomas, sizeof(struct phantomas_softc),
    phantomasmatch, phantomasattach, NULL, NULL);

int
phantomasmatch(struct device *parent, struct cfdata *cfdata, void *aux)
{
	struct confargs *ca = aux;

	if (ca->ca_type.iodc_type != HPPA_TYPE_BCPORT ||
	    ca->ca_type.iodc_sv_model != HPPA_BCPORT_PHANTOM) {
		return (0);
	}
	return (1);
}

void
phantomasattach(struct device *parent, struct device *self, void *aux)
{
	struct confargs *ca = aux;

	printf("\n");
	(*pdc_scanbus)(self, ca, phantomas_callback);
}

static void
phantomas_callback(struct device *self, struct confargs *ca)
{

	config_found_sm(self, ca, mbprint, mbsubmatch);
}
