/*	$NetBSD: s3c24x0_intr.h,v 1.1.4.2 2004/09/18 14:32:38 skrll Exp $ */

/*
 * Copyright (c) 2002, 2003  Genetec corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corporation may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _S3C24X0_INTR_H_
#define	_S3C24X0_INTR_H_

#ifndef _LOCORE

#define	SI_TO_IRQBIT(si)  (1<<(si))

#define	get_pending_softint()	(softint_pending & soft_intr_mask)
#define	update_softintr_mask()	\
	(soft_intr_mask = s3c24x0_soft_imask[current_spl_level])
#define	s3c2xx0_update_hw_mask() \
	(*s3c2xx0_intr_mask_reg = ~(intr_mask & global_intr_mask))

/* no room for softinterrupts in intr_mask. */
extern int __volatile soft_intr_mask;
extern int s3c24x0_soft_imask[];


#include <arm/s3c2xx0/s3c2xx0_intr.h>

#endif /* ! _LOCORE */

#endif /* _S3C24X0_INTR_H_ */
