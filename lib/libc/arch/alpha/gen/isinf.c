/*	$NetBSD: isinf.c,v 1.1.2.1 1995/04/18 04:18:05 jtc Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "namespace.h"
#include <sys/types.h>
#include <machine/ieee.h>
#include <math.h>

int
isnan(d)
	double d;
{
	register struct ieee_double *p = (struct ieee_double *)&d;

	return (p->dbl_exp == DBL_EXP_INFNAN &&
	    (p->dbl_frach || p->dbl_fracl));
}

int
isinf(d)
	double d;
{
	register struct ieee_double *p = (struct ieee_double *)&d;

	return (p->dbl_exp == DBL_EXP_INFNAN &&
	    !p->dbl_frach && !p->dbl_fracl);
}
