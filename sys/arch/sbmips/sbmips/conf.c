/* $NetBSD: conf.c,v 1.3.4.5 2002/08/31 13:45:52 gehenna Exp $ */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: conf.c,v 1.3.4.5 2002/08/31 13:45:52 gehenna Exp $");

#undef	CONF_HAVE_ISDN
#undef	CONF_HAVE_PCI
#undef	CONF_HAVE_SCSIPI
#undef	CONF_HAVE_USB
#undef	CONF_HAVE_WSCONS
#undef	CONF_HAVE_SPKR
#undef	CONF_HAVE_ISA		/* XXX needs a better name */
#define	CONF_HAVE_KTTCP
#define	CONF_HAVE_SYSMON
#undef	CONF_HAVE_ALCHEMY
#define	CONF_HAVE_SBMIPS

#include "arch/evbmips/evbmips/conf_common.c"
