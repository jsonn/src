/* $NetBSD: Lint_div.c,v 1.1.10.1 2000/06/23 16:59:09 minoura Exp $ */

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <stdlib.h>

/*ARGSUSED*/
div_t
div(num, denom)
	int num, denom;
{
	div_t rv = { 0 };
	return (rv);
}
