/*	$NetBSD: stdarg.h,v 1.1.10.1 2004/08/03 10:35:37 skrll Exp $	*/

/*	$OpenBSD: stdarg.h,v 1.2 1998/11/23 03:28:23 mickey Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)stdarg.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _HPPA_STDARG_H_
#define	_HPPA_STDARG_H_

typedef double *va_list;

#ifdef __GNUC__
#define	va_start(ap,lastarg)	((ap) = (va_list)__builtin_saveregs())
#else
#define	va_start(ap,lastarg)	__builtin_va_start(ap, &lastarg)
#endif

#define va_arg(ap,type)							\
	(sizeof(type) > 8 ?						\
	    ((ap = (va_list) ((char *)ap - sizeof (int))),		\
	     (*((type *) (void *) (*((int *) (ap)))))):			\
	    ((ap = (va_list) ((long)((char *)ap - sizeof (type)) &	\
	                             (sizeof(type) > 4 ? ~0x7 : ~0x3))),\
	     (*((type *) (void *) ((char *)ap + ((8 - sizeof(type)) % 4))))))

#define	va_end(ap)

#endif /* !_HPPA_STDARG_H */
