/* $NetBSD: Lint_memset.c,v 1.1.10.1 2000/06/23 16:59:14 minoura Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <string.h>

/*ARGSUSED*/
void *
memset(b, c, len)
	void *b;
	int c;
	size_t len;
{
	return (0);
}
