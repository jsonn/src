/* $NetBSD: negdf2.c,v 1.1.4.2 2000/06/23 16:18:02 minoura Exp $ */

/*
 * Written by Ben Harris, 2000.  This file is in the Public Domain.
 */

#include "softfloat-for-gcc.h"
#include "milieu.h"
#include "softfloat.h"

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: negdf2.c,v 1.1.4.2 2000/06/23 16:18:02 minoura Exp $");
#endif /* LIBC_SCCS and not lint */

float64 __negdf2(float64);

float64
__negdf2(float64 a)
{

	/* libgcc1.c says -a */
	return a ^ FLOAT64_MANGLE(0x8000000000000000ULL);
}
