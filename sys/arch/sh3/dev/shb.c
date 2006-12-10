/*	$NetBSD: shb.c,v 1.10.22.1 2006/12/10 07:16:34 yamt Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: shb.c,v 1.10.22.1 2006/12/10 07:16:34 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>

extern struct cfdriver shb_cd;

static int shb_match(struct device *, struct cfdata *, void *);
static void shb_attach(struct device *, struct device *, void *);
static int shb_print(void *, const char *);
static int shb_search(struct device *, struct cfdata *, const int *, void *);

CFATTACH_DECL(shb, sizeof(struct device),
    shb_match, shb_attach, NULL, NULL);


static int
shb_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, shb_cd.cd_name) != 0)
		return (0);

	return (1);
}

static void
shb_attach(struct device *parent, struct device *self, void *aux)
{

	printf("\n");

	config_search_ia(shb_search, self, "shb", NULL);
}

static int
shb_search(struct device *parent, struct cfdata *cf,
	   const int *ldesc, void *aux)
{

	if (config_match(parent, cf, NULL) > 0)
		config_attach(parent, cf, NULL, shb_print);

	return (0);
}

static int
shb_print(void *aux, const char *pnp)
{

	return (pnp ? QUIET : UNCONF);
}
