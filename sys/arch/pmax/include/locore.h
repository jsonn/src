/*	$NetBSD: locore.h,v 1.2.32.1 2000/11/20 20:20:31 bouyer Exp $	*/

/*
 * Copyright 1996 The Board of Trustees of The Leland Stanford
 * Junior University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Stanford University
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * This file contributed by Jonathan Stone
 */

#ifndef _PMAX_LOCORE_H_
#define _PMAX_LOCORE_H_

#include <mips/locore.h>

void	CopyToBuffer __P((u_short *src, 	/* NB: must be short aligned */
	     volatile u_short *dst, int length));
void	CopyFromBuffer __P((volatile u_short *src, char *dst, int length));

void	kn230_wbflush __P((void));

#endif	/* !_PMAX_LOCORE_H_ */
