/*	$NetBSD: obio.c,v 1.10.34.1 2002/04/17 00:03:40 nathanw Exp $	*/

/*
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>

#include <mac68k/obio/obiovar.h>

static int	obio_match __P((struct device *, struct cfdata *, void *));
static void	obio_attach __P((struct device *, struct device *, void *));
static int	obio_print __P((void *, const char *));
static int	obio_search __P((struct device *, struct cfdata *, void *));

struct cfattach obio_ca = {
	sizeof(struct device), obio_match, obio_attach
};

static int
obio_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	static int obio_matched = 0;

	/* Allow only one instance. */
	if (obio_matched)
		return (0);

	obio_matched = 1;
	return (1);
}

static void
obio_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	printf("\n");

	/* Search for and attach children. */
	(void)config_search(obio_search, self, aux);
}

int
obio_print(args, name)
	void *args;
	const char *name;
{
	struct obio_attach_args *oa = (struct obio_attach_args *)args;

	if (oa->oa_addr != (-1))
		printf(" addr %x", oa->oa_addr);

	return (UNCONF);
}

int
obio_search(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct mainbus_attach_args *mba = (struct mainbus_attach_args *) aux;
	struct obio_attach_args oa;

	oa.oa_addr = cf->cf_loc[0];
	oa.oa_drq = cf->cf_loc[1];
	oa.oa_hsk = cf->cf_loc[2];
	oa.oa_tag = mba->mba_bst;
	oa.oa_dmat = mba->mba_dmat;

	if ((*cf->cf_attach->ca_match)(parent, cf, &oa) > 0)
		config_attach(parent, cf, &oa, obio_print);

	return (0);
}
