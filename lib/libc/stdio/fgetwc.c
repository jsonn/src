/* $NetBSD: fgetwc.c,v 1.1.2.2 2002/01/28 20:50:53 nathanw Exp $ */

/*-
 * Copyright (c)2001 Citrus Project,
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
 * $Citrus$
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <wchar.h>
#include "local.h"
#include "reentrant.h"

wint_t
fgetwc(FILE *fp)
{
	struct wchar_io_data *wcio;
	mbstate_t *st;
	wchar_t wc;
	size_t size;

	_DIAGASSERT(fp != NULL);

	FLOCKFILE(fp);
	_SET_ORIENTATION(fp, 1);
	wcio = WCIO_GET(fp);
	if (wcio == 0) {
		FUNLOCKFILE(fp);
		errno = ENOMEM;
		return WEOF;
	}

	/* if there're ungetwc'ed wchars, use them */
	if (wcio->wcio_ungetwc_inbuf) {
		wc = wcio->wcio_ungetwc_buf[--wcio->wcio_ungetwc_inbuf];
		FUNLOCKFILE(fp);

		return wc;
	}

	st = &wcio->wcio_mbstate_in;

	do {
		char c;
		int ch = __sgetc(fp);

		if (ch == EOF) {
			FUNLOCKFILE(fp);
			return WEOF;
		}

		c = ch;
		size = mbrtowc(&wc, &c, 1, st);
		if (size == (size_t)-1) {
			FUNLOCKFILE(fp);
			errno = EILSEQ;
			return WEOF;
		}
	} while (size == (size_t)-2);
	FUNLOCKFILE(fp);

	_DIAGASSERT(size == 1);

	return wc;
}
