/*	$NetBSD: wcsdup.c,v 1.2.18.1 2008/06/04 02:04:34 yamt Exp $	*/

/*
 * Copyright (C) 2006 Aleksey Cheusov
 *
 * This material is provided "as is", with absolutely no warranty expressed
 * or implied. Any use is at your own risk.
 *
 * Permission to use or copy this software for any purpose is hereby granted 
 * without fee. Permission to modify the code and to distribute modified
 * code is also granted without any restrictions.
 */

#include <sys/cdefs.h>

#if defined(LIBC_SCCS) && !defined(lint) 
__RCSID("$NetBSD: wcsdup.c,v 1.2.18.1 2008/06/04 02:04:34 yamt Exp $"); 
#endif /* LIBC_SCCS and not lint */ 

#include "namespace.h"
#include <stdlib.h>
#include <assert.h>
#include <wchar.h>

__weak_alias(wcsdup,_wcsdup)

wchar_t *
wcsdup(const wchar_t *str)
{
	wchar_t *copy;
	size_t len;

	_DIAGASSERT(str != NULL);

	len = wcslen(str) + 1;
	copy = malloc(len * sizeof (wchar_t));

	if (!copy)
		return NULL;

	return wmemcpy(copy, str, len);
}
