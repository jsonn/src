/* 	$NetBSD: binary.c,v 1.2.2.1 2005/07/03 21:17:21 tron Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Co�dan Sm�rgrav
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
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: binary.c,v 1.2.2.1 2005/07/03 21:17:21 tron Exp $");
#endif /* not lint */

#include <ctype.h>
#include <stdio.h>
#include <zlib.h>

#include "grep.h"

#define BUFFER_SIZE 128

static inline int
okchar(unsigned char c)
{
	return isprint(c) || isspace(c) || c == line_endchar;
}

int
bin_file(FILE *f)
{
	unsigned char buf[BUFFER_SIZE];
	size_t i, m;

	if (fseek(f, 0L, SEEK_SET) == -1)
		return 0;

	if ((m = fread(buf, 1, BUFFER_SIZE, f)) == 0)
		return 0;

	for (i = 0; i < m; i++)
		if (!okchar(buf[i]))
			return 1; 
	
	rewind(f);
	return 0;
}

int
gzbin_file(gzFile *f)
{
	unsigned char buf[BUFFER_SIZE];
	int i, m;

	if (gzseek(f, 0L, SEEK_SET) == -1)
		return 0;

	if ((m = gzread(f, buf, BUFFER_SIZE)) <= 0)
		return 0;

	for (i = 0; i < m; i++)
		if (!okchar(buf[i]))
			return 1;

	gzrewind(f);
	return 0;
}

int
mmbin_file(mmf_t *f)
{
	size_t i;
	/* XXX knows too much about mmf internals */
	for (i = 0; i < BUFFER_SIZE && i < f->len; i++)
		if (!okchar(f->base[i]))
			return 1;
	mmrewind(f);
	return 0;
}
