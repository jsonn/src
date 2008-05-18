/*	$NetBSD: mainbus.c,v 1.6.76.1 2008/05/18 12:32:27 yamt Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: mainbus.c,v 1.6.76.1 2008/05/18 12:32:27 yamt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/autoconf.h>

int mainbus_match(struct device *, struct cfdata *, void *);
void mainbus_attach(struct device *, struct device *, void *);
int mainbus_print(void *, const char *);

struct mainbus_attach_args mainbusdevs[] = {
	{ "cpu"		,         -1,         -1, -1, -1 },
	{ "shb"		,         -1,         -1, -1, -1 },
	{ "com"		, 0xa4000000,         -1, 11, -1 },
	{ "com"		, 0xa4000008,         -1, 12, -1 },
	{ "mmeyepcmcia"	, 0xb000000a, 0xb8000000,  3, 10 },
	{ "mmeyepcmaia"	, 0xb000000c, 0xb9000000,  4,  9 },
	{ NULL }	/* terminator */
};

CFATTACH_DECL(mainbus, sizeof(struct device),
    mainbus_match, mainbus_attach, NULL, NULL);

int
mainbus_match(struct device *parent, struct cfdata *cf, void *aux)
{

	return (1);
}

void
mainbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct mainbus_attach_args *ma;

	printf("\n");

	for (ma = mainbusdevs; ma->ma_name != NULL; ma++)
		config_found(self, ma, mainbus_print);
}

int
mainbus_print(void *aux, const char *pnp)
{

	return (pnp ? QUIET : UNCONF);
}
