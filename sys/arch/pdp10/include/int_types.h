/*	$NetBSD: int_types.h,v 1.1.8.1 2005/05/29 23:38:16 riz Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	from: @(#)types.h	7.5 (Berkeley) 3/9/91
 */

#ifndef	_PDP10_INT_TYPES_H_
#define	_PDP10_INT_TYPES_H_

#include <sys/cdefs.h>

/*
 * 7.18.1 Integer types
 */

/* 7.18.1.1 Exact-width integer types */

#ifdef __GNUC__
typedef	__signed char		 __int8_t __attribute__ ((size (8)));
typedef	unsigned char		__uint8_t __attribute__ ((size (8)));
typedef	short int		__int16_t __attribute__ ((size (16)));
typedef	unsigned short int     __uint16_t __attribute__ ((size (16)));
typedef	int			__int32_t __attribute__ ((size (32)));
typedef	unsigned int	       __uint32_t __attribute__ ((size (32)));
#elif defined(__PCC__)
typedef	signed char		 __int8_t /* _Pragma ((size (8))) */;
typedef	unsigned char		__uint8_t /* _Pragma ((size (8))) */;
typedef	short int		__int16_t /* _Pragma ((size (16))) */;
typedef	unsigned short int     __uint16_t /* _Pragma ((size (16))) */;
typedef	int			__int32_t /* _Pragma ((size (32))) */;
typedef	unsigned int	       __uint32_t /* _Pragma ((size (32))) */;
#else
#error Need special types for compiler
#endif
#ifdef __COMPILER_INT64__
typedef	__COMPILER_INT64__	__int64_t;
typedef	__COMPILER_UINT64__    __uint64_t;
#else
/* LONGLONG */
typedef	long long int		__int64_t;
/* LONGLONG */
typedef	unsigned long long int __uint64_t;
#endif

#define	__BIT_TYPES_DEFINED__

/* 7.18.1.4 Integer types capable of holding object pointers */

#ifdef __ELF__
typedef	long int	       __intptr_t;
typedef	unsigned long int      __uintptr_t;
#else
typedef	int		       __intptr_t;
typedef	unsigned int	      __uintptr_t;
#endif

#endif	/* !_VAX_INT_TYPES_H_ */
