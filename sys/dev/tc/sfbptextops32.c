
#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: sfbptextops32.c,v 1.1.8.2 2000/11/20 11:43:16 bouyer Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/tc/sfbreg.h>
#include <dev/pci/tgareg.h>

#define SFBBPP 32
#include "sfbtextops.i"
