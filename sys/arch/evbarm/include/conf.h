/*	$NetBSD: conf.h,v 1.2.2.2 2002/01/08 00:24:15 nathanw Exp $	*/

#ifndef _EVBARM_CONF_H
#define	_EVBARM_CONF_H

/*
 * EVBARM specifc device includes go in here
 */
#include "plcom.h"

#define CONF_HAVE_PCI
#define CONF_HAVE_SCSIPI
#define	CONF_HAVE_USB
#define	CONF_HAVE_WSCONS

#endif	/* _EVBARM_CONF_H */
