/*	$NetBSD: ntohl.c,v 1.10.20.1 2004/08/03 10:53:31 skrll Exp $	*/

/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: ntohl.c,v 1.10.20.1 2004/08/03 10:53:31 skrll Exp $");
#endif

#include <sys/types.h>

#undef ntohl

uint32_t
ntohl(x)
	uint32_t x;
{
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *)&x;
	return (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
#else
	return x;
#endif
}
