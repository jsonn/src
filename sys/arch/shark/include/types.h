/*	$NetBSD: types.h,v 1.2.36.3 2007/09/03 14:29:47 yamt Exp $	*/

#ifndef _SHARK_TYPES_H_
#define	_SHARK_TYPES_H_

#include <arm/arm32/types.h>

/* We need to invoke FIQs indirectly. */
#define	__ARM_FIQ_INDIRECT

#define	__HAVE_DEVICE_REGISTER
#define	__HAVE_TIMECOUNTER
#define	__HAVE_GENERIC_TODR

#endif /* _SHARK_TYPES_H_ */
