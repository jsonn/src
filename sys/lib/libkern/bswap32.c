/*	$NetBSD: bswap32.c,v 1.1.2.2 1997/10/14 10:26:38 thorpej Exp $	*/

/*
 * Written by Manuel Bouyer <bouyer@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$NetBSD: bswap32.c,v 1.1.2.2 1997/10/14 10:26:38 thorpej Exp $";
#endif

#include <sys/types.h>

#undef bswap32

u_int32_t
bswap32(x)
    u_int32_t x;
{
	return  ((x << 24) & 0xff000000 ) |
			((x <<  8) & 0x00ff0000 ) |
			((x >>  8) & 0x0000ff00 ) |
			((x >> 24) & 0x000000ff );
}

