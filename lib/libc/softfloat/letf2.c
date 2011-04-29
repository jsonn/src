/* $NetBSD: letf2.c,v 1.1.4.2 2011/04/29 07:48:36 matt Exp $ */

/*
 * Written by Matt Thomas, 2011.  This file is in the Public Domain.
 */

#include "softfloat-for-gcc.h"
#include "milieu.h"
#include "softfloat.h"

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: letf2.c,v 1.1.4.2 2011/04/29 07:48:36 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#ifdef FLOAT128

flag __letf2(float128, float128);

flag
__letf2(float128 a, float128 b)
{

	/* libgcc1.c says 1 - (a <= b) */
	return 1 - float128_le(a, b);
}

#endif /* FLOAT128 */
