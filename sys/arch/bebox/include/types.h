/*	$NetBSD: types.h,v 1.1.2.1 1997/11/05 04:39:01 thorpej Exp $	*/

/*-
 * Copyright (C) 1995 Wolfgang Solfrank.
 * Copyright (C) 1995 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACHTYPES_H_
#define	_MACHTYPES_H_

#include <sys/cdefs.h>

#define	__BIT_TYPES_DEFINED__
typedef	signed char		int8_t;
typedef	unsigned char		u_int8_t;
typedef	short			int16_t;
typedef	unsigned short		u_int16_t;
typedef	int			int32_t;
typedef	unsigned int		u_int32_t;
/* LONGLONG */
typedef	long long		int64_t;
/* LONGLONG */
typedef	unsigned long long	u_int64_t;

typedef	int32_t			register_t;

typedef	unsigned long		vm_size_t;
typedef	unsigned long		vm_offset_t;

#if 0
/* This is only to make some unneeded function declaration happy */
#define	label_t	void
#endif 0
typedef struct label_t {
        int val[40]; /* double check this XXX */
} label_t;

#define	__BROKEN_INDIRECT_CONFIG	/* i386 compat */

#endif	/* _MACHTYPES_H_ */
