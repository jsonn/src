/*	$NetBSD: fpsetmask.c,v 1.2.16.1 2002/01/28 20:49:57 nathanw Exp $	*/

/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>

#include "namespace.h"

#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpsetmask,_fpsetmask)
#endif

fp_except
fpsetmask(mask)
	fp_except mask;
{
	fp_except old;
	fp_except new;

	__asm__("cfc1 %0,$31" : "=r" (old));

	new = old;
	new &= ~(0x1f << 7); 
	new |= ((mask & 0x1f) << 7);

	__asm__("ctc1 %0,$31" : : "r" (new));

	return (old >> 7) & 0x1f;
}
