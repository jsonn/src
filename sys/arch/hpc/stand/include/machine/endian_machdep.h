/*	$NetBSD: endian_machdep.h,v 1.1.2.2 2001/02/11 19:10:17 bouyer Exp $	*/

/* Windows CE architecture */

#ifndef _LITTLE_ENDIAN
#define	_LITTLE_ENDIAN	1234
#endif
#ifndef LITTLE_ENDIAN
#define	LITTLE_ENDIAN	1234
#endif

#define _BYTE_ORDER _LITTLE_ENDIAN
