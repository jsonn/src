
#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: sfbptextops.c,v 1.1.2.1 1999/12/27 18:35:39 wrstuden Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/tc/sfbreg.h>
#include <dev/pci/tgareg.h>

#define SFBBPP 8
#include "sfbtextops.i"
