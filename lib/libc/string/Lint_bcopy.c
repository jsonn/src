/* $NetBSD: Lint_bcopy.c,v 1.1.10.1 2000/06/23 16:59:11 minoura Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <string.h>

/*ARGSUSED*/
void
bcopy(src, dst, len)
	const void *src;
	void *dst;
	size_t len;
{
}
