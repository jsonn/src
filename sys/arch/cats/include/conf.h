/*	$NetBSD: conf.h,v 1.1.2.2 2002/01/10 19:41:01 thorpej Exp $	*/

#ifndef _CATS_CONF_H
#define	_CATS_CONF_H

/*
 * CATS specific device includes go in here
 */
#include "fcom.h"

#define	CONF_HAVE_PCI
#define	CONF_HAVE_USB
#define	CONF_HAVE_SCSIPI
#define	CONF_HAVE_WSCONS

#endif	/* _CATS_CONF_H */
