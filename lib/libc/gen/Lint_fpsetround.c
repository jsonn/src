/*	$NetBSD: Lint_fpsetround.c,v 1.1.2.2 1997/11/08 21:58:14 veego Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <ieeefp.h>

/*ARGSUSED*/
fp_rnd
fpsetround(r)
	fp_rnd r;
{
	fp_rnd rv = { 0 };

	return (rv);
}
