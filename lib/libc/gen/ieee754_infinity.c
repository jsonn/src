/*	$NetBSD: ieee754_infinity.c,v 1.1.2.2 2002/03/08 21:35:09 nathanw Exp $	*/

/*
 * IEEE-compatible infinity.c -- public domain.
 */

#include <math.h>
#include <machine/endian.h>

const union __double_u __infinity =
#if BYTE_ORDER == BIG_ENDIAN
	{ { 0x7f, 0xf0, 0, 0, 0, 0,    0,    0} };
#else
	{ {    0,    0, 0, 0, 0, 0, 0xf0, 0x7f} };
#endif
