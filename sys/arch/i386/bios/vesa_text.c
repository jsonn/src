/* $NetBSD: vesa_text.c,v 1.1.2.2 2002/07/16 08:29:05 gehenna Exp $ */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <machine/frame.h>
#include <machine/kvm86.h>
#include <machine/bus.h>

#include <arch/i386/bios/vesabios.h>
#include <arch/i386/bios/vesabiosreg.h>

static int vesatext_match(struct device *, struct cfdata *, void *);
static void vesatext_attach(struct device *, struct device *, void *);

struct vesatextsc {
	struct device sc_dev;
	int *sc_modes;
	int sc_nmodes;
};

struct cfattach vesatext_ca = {
	sizeof(struct vesatextsc), vesatext_match, vesatext_attach
};

static int
vesatext_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct vesabiosdev_attach_args *vaa = aux;

	if (strcmp(vaa->vbaa_type, "text"))
		return (0);

	return (1);
}

static void
vesatext_attach(parent, dev, aux)
	struct device *parent, *dev;
	void *aux;
{
	struct vesatextsc *sc = (struct vesatextsc *)dev;
	struct vesabiosdev_attach_args *vaa = aux;
	int i;

	sc->sc_modes = malloc(vaa->vbaa_nmodes * sizeof(int),
			      M_DEVBUF, M_NOWAIT);
	sc->sc_nmodes = vaa->vbaa_nmodes;
	for (i = 0; i < vaa->vbaa_nmodes; i++)
		sc->sc_modes[i] = vaa->vbaa_modes[i];

	printf("\n");
}
