/*	$NetBSD: runeglue.c,v 1.11.10.1 2007/11/06 23:11:15 matt Exp $	*/

/*-
 * Copyright (c)1999 Citrus Project,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	Id: runeglue.c,v 1.7 2000/12/22 22:52:29 itojun Exp
 */

/*
 * Glue code to hide "rune" facility from user programs.
 * This is important to keep backward/future compatibility.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: runeglue.c,v 1.11.10.1 2007/11/06 23:11:15 matt Exp $");
#endif /* LIBC_SCCS and not lint */

#define _CTYPE_PRIVATE
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "citrus/citrus_module.h"
#include "citrus/citrus_ctype.h"
#include "rune.h"
#include "rune_local.h"

#if EOF != -1
#error "EOF != -1"
#endif
#if _CACHED_RUNES != 256
#error "_CACHED_RUNES != 256"
#endif

int
__runetable_to_netbsd_ctype(rl)
	_RuneLocale *rl;
{
	int i, ch;
	unsigned char *new_ctype;
	short *new_toupper, *new_tolower;

	_DIAGASSERT(locale != NULL);

	new_ctype = malloc(sizeof(*new_ctype) * (1 + _CTYPE_NUM_CHARS));
	if (!new_ctype)
		return -1;
	new_toupper = malloc(sizeof(*new_toupper) * (1 + 256));
	if (!new_toupper) {
		free(new_ctype);
		return -1;
	}
	new_tolower = malloc(sizeof(*new_tolower) * (1 + 256));
	if (!new_tolower) {
		free(new_ctype);
		free(new_toupper);
		return -1;
	}

	memset(new_ctype, 0, sizeof(*new_ctype) * (1 + _CTYPE_NUM_CHARS));
	memset(new_toupper, 0, sizeof(*new_toupper) * (1 + 256));
	memset(new_tolower, 0, sizeof(*new_tolower) * (1 + 256));

	new_ctype[0] = 0;
	new_toupper[0] = EOF;
	new_tolower[0] = EOF;
	for (i = 0; i < _CTYPE_NUM_CHARS; i++) {
		new_ctype[i + 1] = 0;
		new_toupper[i + 1] = i;
		new_tolower[i + 1] = i;

		/* XXX: FIXME
		 * expected 'x' == L'x', see defect report #279
		 * http://www.open-std.org/JTC1/SC22/WG14/www/docs/dr_279.htm
		 */
		if (_citrus_ctype_wctob(rl->rl_citrus_ctype, (wint_t)i, &ch)) {
			free(new_ctype);
			free(new_toupper);
			free(new_tolower);
			return -1;
		}
		if (ch == EOF)
			continue;

		if (rl->rl_runetype[i] & _CTYPE_U)
			new_ctype[i + 1] |= _U;
		if (rl->rl_runetype[i] & _CTYPE_L)
			new_ctype[i + 1] |= _L;
		if (rl->rl_runetype[i] & _CTYPE_D)
			new_ctype[i + 1] |= _N;
		if (rl->rl_runetype[i] & _CTYPE_S)
			new_ctype[i + 1] |= _S;
		if (rl->rl_runetype[i] & _CTYPE_P)
			new_ctype[i + 1] |= _P;
		if (rl->rl_runetype[i] & _CTYPE_C)
			new_ctype[i + 1] |= _C;
		if (rl->rl_runetype[i] & _CTYPE_X)
			new_ctype[i + 1] |= _X;
		/*
		 * TWEAK!  _B has been used incorrectly (or with older
		 * declaration) in ctype.h isprint() macro.
		 * _B does not mean isblank, it means "isprint && !isgraph".
		 * the following is okay since isblank() was hardcoded in
		 * function (i.e. isblank() is inherently locale unfriendly).
		 */
#if 1
		if ((rl->rl_runetype[i] & (_CTYPE_R | _CTYPE_G)) == _CTYPE_R)
			new_ctype[i + 1] |= _B;
#else
		if (rl->rl_runetype[i] & _CTYPE_B)
			new_ctype[i + 1] |= _B;
#endif
		new_toupper[i + 1] = (short)rl->rl_mapupper[i];
		new_tolower[i + 1] = (short)rl->rl_maplower[i];
	}

	/* LINTED const cast */
	rl->rl_ctype_tab = (const unsigned char *)new_ctype;
	/* LINTED const cast */
	rl->rl_toupper_tab = (const short *)new_toupper;
	/* LINTED const cast */
	rl->rl_tolower_tab = (const short *)new_tolower;

	return 0;
}
