/*	$NetBSD: powerrom.c,v 1.1.6.3 2002/10/10 18:30:17 jdolecek Exp $	*/

/* Test driver to see if we can talk to PowerROMs */

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: powerrom.c,v 1.1.6.3 2002/10/10 18:30:17 jdolecek Exp $");

#include <sys/device.h>
#include <sys/systm.h>
#include <dev/podulebus/podulebus.h>
#include <dev/podulebus/podules.h>

int  powerrom_match(struct device *, struct cfdata *, void *);
void powerrom_attach(struct device *, struct device *, void *);

CFATTACH_DECL(powerrom, sizeof(struct device),
    powerrom_match, powerrom_attach, NULL, NULL);

int
powerrom_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct podulebus_attach_args *pa = aux;

	return (pa->pa_product == PODULE_ALSYSTEMS_SCSI);
}

void
powerrom_attach(struct device *parent, struct device *self, void *aux)
{
	struct podulebus_attach_args *pa = aux;

	if (podulebus_initloader(pa) == 0)
		printf(": card id = 0x%x", podloader_callloader(pa, 0, 0));
	printf("\n");
}
