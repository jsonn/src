/*	$NetBSD: ieeefp.h,v 1.1.104.1 2008/09/28 10:40:11 mjf Exp $	*/

/* 
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#ifndef _X86_IEEEFP_H_
#define _X86_IEEEFP_H_

#include <sys/featuretest.h>

#if defined(_NETBSD_SOURCE) || defined(_ISOC99_SOURCE)

typedef int fenv_t;
typedef int fexcept_t;

#define	FE_INVALID	0x01	/* invalid operation exception */
#define	FE_DENORMAL	0x02	/* denormalization exception */
#define	FE_DIVBYZERO	0x04	/* divide-by-zero exception */
#define	FE_OVERFLOW	0x08	/* underflow exception */
#define	FE_UNDERFLOW	0x10	/* overflow exception */
#define	FE_INEXACT	0x20	/* imprecise (loss of precision) */

#define	FE_ALL_EXCEPT	0x3f

#define	FE_TONEAREST	0	/* round to nearest representable number */
#define	FE_DOWNWARD	1	/* round toward negative infinity */
#define	FE_UPWARD	2	/* round toward positive infinity */
#define	FE_TOWARDZERO	3	/* round to zero (truncate) */

#if !defined(_ISOC99_SOURCE)

typedef int fp_except;
#define FP_X_INV	FE_INVALID	/* invalid operation exception */
#define FP_X_DNML	FE_DENORMAL	/* denormalization exception */
#define FP_X_DZ		FE_DIVBYZERO	/* divide-by-zero exception */
#define FP_X_OFL	FE_OVERFLOW	/* overflow exception */
#define FP_X_UFL	FE_UNDERFLOW	/* underflow exception */
#define FP_X_IMP	FE_INEXACT	/* imprecise (loss of precision) */

typedef enum {
    FP_RN=FE_TONEAREST,		/* round to nearest representable number */
    FP_RM=FE_DOWNWARD,		/* round toward negative infinity */
    FP_RP=FE_UPWARD,		/* round toward positive infinity */
    FP_RZ=FE_TOWARDZERO		/* round to zero (truncate) */
} fp_rnd;

#endif /* !_ISOC99_SOURCE */

#endif /* _NETBSD_SOURCE || _ISOC99_SOURCE */

#endif /* _X86_IEEEFP_H_ */
