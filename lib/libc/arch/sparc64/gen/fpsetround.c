/*	$NetBSD: fpsetround.c,v 1.1.12.1 2002/01/28 20:50:19 nathanw Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>

#include "namespace.h"

#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpsetround,_fpsetround)
#endif

fp_rnd
fpsetround(rnd_dir)
	fp_rnd rnd_dir;
{
	fp_rnd old;
	fp_rnd new;

	__asm__("st %%fsr,%0" : "=m" (*&old));

	new = old;
	new &= ~(0x03 << 30); 
	new |= ((rnd_dir & 0x03) << 30);

	__asm__("ld %0,%%fsr" : : "m" (*&new));

	return (old >> 30) & 0x03;
}
