/*	$NetBSD: frexp_ieee754.c,v 1.4.6.1 2006/03/24 22:41:07 riz Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Header: frexp.c,v 1.1 91/07/07 04:45:01 torek Exp
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)frexp.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: frexp_ieee754.c,v 1.4.6.1 2006/03/24 22:41:07 riz Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <machine/ieee.h>
#include <math.h>

/*
 * Split the given value into a fraction in the range [0.5, 1.0) and
 * an exponent, such that frac * (2^exp) == value.  If value is 0,
 * return 0.
 */
double
frexp(double value, int *eptr)
{
	union ieee_double_u u;

	if (value) {
		/*
		 * Fractions in [0.5..1.0) have an exponent of 2^-1.
		 * Leave Inf and NaN alone, however.
		 * WHAT ABOUT DENORMS?
		 */
		u.dblu_d = value;
		if (u.dblu_dbl.dbl_exp != DBL_EXP_INFNAN) {
			*eptr = 0;
			if (u.dblu_dbl.dbl_exp == 0) {
				/* denormal, scale out of mantissa */
				*eptr = -DBL_FRACBITS;
				u.dblu_d *= 4.50359962737049600000e+15;
			}
			*eptr += u.dblu_dbl.dbl_exp - (DBL_EXP_BIAS - 1);
			u.dblu_dbl.dbl_exp = DBL_EXP_BIAS - 1;
		}
		return (u.dblu_d);
	} else {
		*eptr = 0;
		return (0.0);
	}
}
