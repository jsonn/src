/* $NetBSD: fwide.c,v 1.1.2.2 2002/01/28 20:51:00 nathanw Exp $ */

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
#include <stdio.h>
#include <wchar.h>
#include "local.h"
#include "reentrant.h"

int
fwide(FILE *fp, int mode)
{
	struct wchar_io_data *wcio;

	_DIAGASSERT(fp != NULL);

	/*
	 * this implementation use only -1, 0, 1
	 * for mode value.
	 * (we don't need to do this, but
	 *  this can make things simpler.)
	 */
	if (mode > 0)
		mode = 1;
	else if (mode < 0)
		mode = -1;

	FLOCKFILE(fp);
	wcio = WCIO_GET(fp);
	if (!wcio)
		return 0; /* XXX */

	if (wcio->wcio_mode == 0 && mode != 0)
		wcio->wcio_mode = mode;
	else
		mode = wcio->wcio_mode;
	FUNLOCKFILE(fp);

	return mode;
}
