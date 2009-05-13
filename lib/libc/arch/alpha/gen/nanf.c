/*	$NetBSD: nanf.c,v 1.4.32.1 2009/05/13 19:18:20 jym Exp $	*/

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: nanf.c,v 1.4.32.1 2009/05/13 19:18:20 jym Exp $");
#endif /* LIBC_SCCS and not lint */

#include <math.h>
#include <machine/endian.h>

/* bytes for quiet NaN (IEEE single precision) */
const union __float_u __nanf =
		{ {    0,    0, 0xc0, 0x7f } };

__warn_references(__nanf, "warning: <math.h> defines NAN incorrectly for your compiler.")
