/*	$NetBSD: obio.c,v 1.18.2.2 2004/09/18 14:36:56 skrll Exp $	*/

/*-
 * Copyright (C) 1998	Internet Research Institute, Inc.
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
 *	This product includes software developed by
 *	Internet Research Institute, Inc.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: obio.c,v 1.18.2.2 2004/09/18 14:36:56 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ofw/openfirm.h>

#include <machine/autoconf.h>

static void obio_attach __P((struct device *, struct device *, void *));
static int obio_match __P((struct device *, struct cfdata *, void *));
static int obio_print __P((void *, const char *));

struct obio_softc {
	struct device sc_dev;
	int sc_node;
};


CFATTACH_DECL(obio, sizeof(struct obio_softc),
    obio_match, obio_attach, NULL, NULL);

int
obio_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_APPLE)
		switch (PCI_PRODUCT(pa->pa_id)) {

		case PCI_PRODUCT_APPLE_GC:
		case PCI_PRODUCT_APPLE_OHARE:
		case PCI_PRODUCT_APPLE_HEATHROW:
		case PCI_PRODUCT_APPLE_PADDINGTON:
		case PCI_PRODUCT_APPLE_KEYLARGO:
		case PCI_PRODUCT_APPLE_PANGEA_MACIO:
		case PCI_PRODUCT_APPLE_INTREPID:
			return 1;
		}

	return 0;
}

/*
 * Attach all the sub-devices we can find
 */
void
obio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct obio_softc *sc = (struct obio_softc *)self;
	struct pci_attach_args *pa = aux;
	struct confargs ca;
	int node, child, namelen;
	u_int reg[20];
	int intr[6];
	char name[32];

	switch (PCI_PRODUCT(pa->pa_id)) {

	/* XXX should not use name */
	case PCI_PRODUCT_APPLE_GC:
		node = OF_finddevice("/bandit/gc");
		break;

	case PCI_PRODUCT_APPLE_OHARE:
		node = OF_finddevice("/bandit/ohare");
		break;

	case PCI_PRODUCT_APPLE_HEATHROW:
	case PCI_PRODUCT_APPLE_PADDINGTON:
	case PCI_PRODUCT_APPLE_KEYLARGO:
	case PCI_PRODUCT_APPLE_PANGEA_MACIO:
	case PCI_PRODUCT_APPLE_INTREPID:
		node = OF_finddevice("mac-io");
		if (node == -1)
			node = OF_finddevice("/pci/mac-io");
		break;

	default:
		printf("obio_attach: unknown obio controller\n");
		return;
	}

	sc->sc_node = node;

	if (OF_getprop(node, "assigned-addresses", reg, sizeof(reg)) < 12)
		return;
	ca.ca_baseaddr = reg[2];

	printf(": addr 0x%x\n", ca.ca_baseaddr);

	/* Enable CD and microphone sound input. */
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_APPLE_PADDINGTON)
		out8(ca.ca_baseaddr + 0x37, 0x03);

	for (child = OF_child(node); child; child = OF_peer(child)) {
		namelen = OF_getprop(child, "name", name, sizeof(name));
		if (namelen < 0)
			continue;
		if (namelen >= sizeof(name))
			continue;

		name[namelen] = 0;
		ca.ca_name = name;
		ca.ca_node = child;

		ca.ca_nreg  = OF_getprop(child, "reg", reg, sizeof(reg));
		ca.ca_nintr = OF_getprop(child, "AAPL,interrupts", intr,
				sizeof(intr));
		if (ca.ca_nintr == -1)
			ca.ca_nintr = OF_getprop(child, "interrupts", intr,
					sizeof(intr));

		ca.ca_reg = reg;
		ca.ca_intr = intr;

		config_found(self, &ca, obio_print);
	}
}

static char *skiplist[] = {
	"interrupt-controller",
	"gpio",
	"escc-legacy",
	"timer",
	"i2c",
	"power-mgt"
};

#define N_LIST (sizeof(skiplist) / sizeof(skiplist[0]))

int
obio_print(aux, obio)
	void *aux;
	const char *obio;
{
	struct confargs *ca = aux;
	int i;

	for (i = 0; i < N_LIST; i++)
		if (strcmp(ca->ca_name, skiplist[i]) == 0)
			return QUIET;

	if (obio)
		aprint_normal("%s at %s", ca->ca_name, obio);

	if (ca->ca_nreg > 0)
		aprint_normal(" offset 0x%x", ca->ca_reg[0]);

	return UNCONF;
}
