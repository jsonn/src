/* $NetBSD: eqdf2.c,v 1.1.4.2 2000/06/23 16:17:55 minoura Exp $ */

/*
 * Written by Ben Harris, 2000.  This file is in the Public Domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: eqdf2.c,v 1.1.4.2 2000/06/23 16:17:55 minoura Exp $");
#endif /* LIBC_SCCS and not lint */

#include "softfloat-for-gcc.h"
#include "milieu.h"
#include "softfloat.h"

flag __eqdf2(float64, float64);

flag
__eqdf2(float64 a, float64 b)
{

	/* libgcc1.c says !(a == b) */
	return !float64_eq(a, b);
}
