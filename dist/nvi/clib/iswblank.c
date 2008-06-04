/*	$NetBSD: iswblank.c,v 1.1.1.1.2.3 2008/06/04 02:03:06 yamt Exp $ */

#include "config.h"

#if defined(LIBC_SCCS) && !defined(lint)
static const char sccsid[] = "Id: iswblank.c,v 1.1 2001/10/11 19:22:29 skimo Exp";
#endif /* LIBC_SCCS and not lint */

#include <wchar.h>
#include <wctype.h>

int
iswblank (wint_t wc)
{
    return iswctype(wc, wctype("blank"));
}
