/*	$NetBSD: types.h,v 1.2.2.2 2002/07/14 17:47:33 gehenna Exp $	*/

/*	$OpenBSD: types.h,v 1.6 2001/08/11 01:58:34 art Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)types.h	8.3 (Berkeley) 1/5/94
 */

#ifndef	_HPPA_TYPES_H_
#define	_HPPA_TYPES_H_

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
typedef struct label_t {
	int	lbl_rp;
	int	lbl_sp;
	int	lbl_s[17];
	int	lbl_ss[1];
	double	lbl_sf[10];	/* hp800:fr12-fr15, hp700:fr12-fr21 */
} label_t;

typedef	unsigned long		hppa_hpa_t;
typedef	unsigned long		hppa_spa_t;
typedef	unsigned int		pa_space_t;
typedef	unsigned long		vaddr_t;
typedef	unsigned long		vsize_t;
typedef	unsigned long		paddr_t;
typedef	unsigned long		psize_t;
/* XXX DIE DIE DIE */
typedef	unsigned long vm_offset_t;
typedef unsigned long vm_size_t;

#endif

typedef int			register_t;

#define	__MACHINE_STACK_GROWS_UP	/* stack grows to higher addresses */

#endif	/* _HPPA_TYPES_H_ */
