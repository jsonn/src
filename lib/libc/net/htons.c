/*	$NetBSD: htons.c,v 1.5.6.1 1996/05/29 23:28:19 cgd Exp $	*/

/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$NetBSD: htons.c,v 1.5.6.1 1996/05/29 23:28:19 cgd Exp $";
#endif

#include <sys/types.h>
#include <machine/endian.h>

#undef htons

unsigned short
htons(x)
	unsigned short x;
{
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *) &x;
	return (u_int16_t)(s[0] << 8 | s[1]);
#else
	return x;
#endif
}
