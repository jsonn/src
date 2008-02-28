/*	$NetBSD: types.h,v 1.7.6.1 2008/02/28 21:47:56 rjs Exp $	*/

#ifndef _SHARK_TYPES_H_
#define	_SHARK_TYPES_H_

#include <arm/arm32/types.h>

/* We need to invoke FIQs indirectly. */
#define	__ARM_FIQ_INDIRECT

#define	__HAVE_DEVICE_REGISTER

#endif /* _SHARK_TYPES_H_ */
