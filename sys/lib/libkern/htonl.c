/*	$NetBSD: htonl.c,v 1.11.10.1 2005/03/19 08:36:21 yamt Exp $	*/

/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: htonl.c,v 1.11.10.1 2005/03/19 08:36:21 yamt Exp $");
#endif

#include <sys/types.h>

#undef htonl

uint32_t
htonl(x)
	uint32_t x;
{
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *)&x;
	return (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
#else
	return x;
#endif
}
