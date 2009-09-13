/*	$NetBSD: rmixl_mainbus.c,v 1.1.2.1 2009/09/13 03:27:38 cliff Exp $	*/

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * mainbus.c
 *
 * mainbus configuration
 *
 * Created      : 15/12/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rmixl_mainbus.c,v 1.1.2.1 2009/09/13 03:27:38 cliff Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/bus.h>
#include "locators.h"

static int  mainbusmatch  __P((struct device *, struct cfdata *, void *));
static void mainbusattach __P((struct device *, struct device *, void *));
static int  mainbussearch __P((struct device *, struct cfdata *,
				const int *, void *));

CFATTACH_DECL(mainbus, sizeof(struct device),
    mainbusmatch, mainbusattach, NULL, NULL);

static int mainbus_found;

static int
mainbusmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	if (mainbus_found)
		return 0;
	return 1;
}

static int
mainbussearch(parent, cf, ldesc, aux)
	struct device *parent;
	struct cfdata *cf;
	const int *ldesc;
	void *aux;
{
	if (config_match(parent, cf, NULL) > 0)
		config_attach(parent, cf, aux, NULL);

	return 0;
}

static void
mainbusattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	aprint_naive("\n");
	aprint_normal("\n");

	config_search_ia(mainbussearch, self, "mainbus", NULL);
}
