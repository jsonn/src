/* $NetBSD: nesf2.c,v 1.1.4.2 2000/06/23 16:18:02 minoura Exp $ */

/*
 * Written by Ben Harris, 2000.  This file is in the Public Domain.
 */

#include "softfloat-for-gcc.h"
#include "milieu.h"
#include "softfloat.h"

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: nesf2.c,v 1.1.4.2 2000/06/23 16:18:02 minoura Exp $");
#endif /* LIBC_SCCS and not lint */

flag __nesf2(float32, float32);

flag
__nesf2(float32 a, float32 b)
{

	/* libgcc1.c says a != b */
	return !float32_eq(a, b);
}
