/*	$NetBSD: ansi.h,v 1.17.20.1 2006/11/18 21:29:32 ad Exp $ */

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
 * 	@(#)ansi.h	8.2 (Berkeley) 1/4/94
 */

#ifndef	_ANSI_H_
#define	_ANSI_H_

#include <sys/cdefs.h>
#include <machine/int_types.h>

/*
 * Types which are fundamental to the implementation and may appear in
 * more than one standard header are defined here.  Standard headers
 * then use:
 *	#ifdef	_BSD_SIZE_T_
 *	typedef	_BSD_SIZE_T_ size_t;
 *	#undef	_BSD_SIZE_T_
 *	#endif
 */
#define	_BSD_CLOCK_T_		unsigned long	/* clock() */
#define	_BSD_PTRDIFF_T_		long		/* ptr1 - ptr2 */
#define	_BSD_SIZE_T_		unsigned long	/* sizeof() */
#define	_BSD_SSIZE_T_		long		/* byte count or error */
#define	_BSD_TIME_T_		long		/* time() */
#if __GNUC_PREREQ__(3, 0)
#define	_BSD_VA_LIST_		__builtin_va_list /* va_list */
#else
#define	_BSD_VA_LIST_		char *		/* va_list */
#endif
#define	_BSD_CLOCKID_T_		int		/* clockid_t */
#define	_BSD_TIMER_T_		int		/* timer_t */
#define	_BSD_SUSECONDS_T_	int		/* suseconds_t */
#define	_BSD_USECONDS_T_	unsigned int	/* useconds_t */

/*
 * NOTE: rune_t is not covered by ANSI nor other standards, and should not
 * be instantiated outside of lib/libc/locale.  use wchar_t.
 *
 * Runes (wchar_t) is declared to be an ``int'' instead of the more natural
 * ``unsigned long'' or ``long''.  Two things are happening here.  It is not
 * unsigned so that EOF (-1) can be naturally assigned to it and used.  Also,
 * it looks like 10646 will be a 31 bit standard.  This means that if your
 * ints cannot hold 32 bits, you will be in trouble.  The reason an int was
 * chosen over a long is that the is*() and to*() routines take ints (says
 * ANSI C), but they use _RUNE_T_ instead of int.  By changing it here, you
 * lose a bit of ANSI conformance, but your programs will still work.
 *
 * Note that _WCHAR_T_ and _RUNE_T_ must be of the same type.  When wchar_t
 * and rune_t are typedef'd, _WCHAR_T_ will be undef'd, but _RUNE_T remains
 * defined for ctype.h.
 */
#define	_BSD_WCHAR_T_		int		/* wchar_t */
#define	_BSD_WINT_T_		int		/* wint_t */
#define	_BSD_RUNE_T_		int		/* rune_t */
#define _BSD_WCTRANS_T_		void *		/* wctrans_t */
#define _BSD_WCTYPE_T_		void *		/* wctype_t */

/*
 * mbstate_t is an opaque object to keep conversion state, during multibyte
 * stream conversions.  The content must not be referenced by user programs.
 */
typedef union {
	__int64_t __mbstateL;	/* for alignment */
	char __mbstate8[128];
} __mbstate_t;
#define	_BSD_MBSTATE_T_		__mbstate_t	/* mbstate_t */

#endif	/* _ANSI_H_ */
