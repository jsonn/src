/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
#ifndef __NetBSD__
static char sccsid[] = "@(#)iswctype.c	8.3 (Berkeley) 2/24/94";
#endif
#endif /* LIBC_SCCS and not lint */

#include <wchar.h>
#include <wctype.h>
#if !defined(__FreeBSD__)
#define _BSD_RUNE_T_    int
#define _BSD_CT_RUNE_T_ _rune_t
#include "runetype.h"
#endif

#undef iswalnum
int
iswalnum(c)
	wint_t c;
{
	return (__istype_w((c), _CTYPE_A)|__isctype_w((c), _CTYPE_D));
}

#undef iswalpha
int
iswalpha(c)
	wint_t c;
{
	return (__istype_w((c), _CTYPE_A));
}

#undef iswcntrl
int
iswcntrl(c)
	wint_t c;
{
	return (__istype_w((c), _CTYPE_C));
}

#undef iswdigit
int
iswdigit(c)
	wint_t c;
{
	return (__isctype_w((c), _CTYPE_D));
}

#undef iswgraph
int
iswgraph(c)
	wint_t c;
{
	return (__istype_w((c), _CTYPE_G));
}

#undef iswlower
int
iswlower(c)
	wint_t c;
{
	return (__istype_w((c), _CTYPE_L));
}

#undef iswprint
int
iswprint(c)
	wint_t c;
{
	return (__istype_w((c), _CTYPE_R));
}

#undef iswpunct
int
iswpunct(c)
	wint_t c;
{
	return (__istype_w((c), _CTYPE_P));
}

#undef iswspace
int
iswspace(c)
	wint_t c;
{
	return (__istype_w((c), _CTYPE_S));
}

#undef iswupper
int
iswupper(c)
	wint_t c;
{
	return (__istype_w((c), _CTYPE_U));
}

#undef iswxdigit
int
iswxdigit(c)
	wint_t c;
{
	return (__isctype_w((c), _CTYPE_X));
}

#undef towlower
wint_t
towlower(c)
	wint_t c;
{
        return (__tolower_w(c));
}

#undef towupper
wint_t
towupper(c)
	wint_t c;
{
        return (__toupper_w(c));
}

#undef wcwidth
int
wcwidth(c)
	wint_t c;
{
        return ((unsigned)__maskrune_w((c), _CTYPE_SWM)>>_CTYPE_SWS);
}

