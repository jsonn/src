/* $NetBSD: Lint_strcpy.c,v 1.1.10.1 2000/06/23 16:59:16 minoura Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <string.h>

/*ARGSUSED*/
char *
strcpy(dst, src)
	char *dst;
	const char *src;
{
	return (0);
}
