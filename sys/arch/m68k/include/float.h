/*	$NetBSD: float.h,v 1.16.2.1 2004/08/03 10:36:40 skrll Exp $	*/

#ifndef _M68K_FLOAT_H_
#define _M68K_FLOAT_H_

#ifndef __mc68010__
#define LDBL_MANT_DIG	64
#define LDBL_EPSILON	1.0842021724855044340E-19L
#define LDBL_DIG	18
#define LDBL_MIN_EXP	(-16381)
#define LDBL_MIN	1.6810515715560467531E-4932L
#define LDBL_MIN_10_EXP	(-4931)
#define LDBL_MAX_EXP	16384
#define LDBL_MAX	1.1897314953572317650E+4932L
#define LDBL_MAX_10_EXP	4932
#endif

#include <sys/float_ieee754.h>

#ifndef __mc68010__
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_C_SOURCE) && \
    !defined(_XOPEN_SOURCE) || \
    ((__STDC_VERSION__ - 0) >= 199901L) || \
    ((_POSIX_C_SOURCE - 0) >= 200112L) || \
    ((_XOPEN_SOURCE  - 0) >= 600) || \
    defined(_ISOC99_SOURCE) || defined(_NETBSD_SOURCE)
#define	DECIMAL_DIG	21
#endif /* !defined(_ANSI_SOURCE) && ... */
#endif /* !__mc68010__ */

#endif	/* !_M68K_FLOAT_H_ */
