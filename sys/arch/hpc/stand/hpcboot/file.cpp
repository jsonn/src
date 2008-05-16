/* -*-C++-*-	$NetBSD: file.cpp,v 1.4.78.1 2008/05/16 02:22:24 yamt Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <console.h>
#include <file.h>
#include <file_fat.h>
#include <file_http.h>
#include <file_ufs.h>

FileManager::FileManager(Console *&cons, enum FileOps ops)
	: File(cons)
{
	// File System
	switch(ops) {
	default:
	case FILE_UFS:
		_file = new UfsFile(_cons);
		break;
	case FILE_FAT:
		_file = new FatFile(_cons);
		break;
	case FILE_HTTP:
		_file = new HttpFile(_cons);
	}

	// GZIP set magic header
	_gz_magic[0] = 0x1f; _gz_magic[1] = 0x8b;
	memset(_inbuf, 0, Z_BUFSIZE);
	_reset();
}

//
// UNICODE util.
//
BOOL
File::_to_ascii(char *m, const TCHAR *w, size_t mlen)
{
	size_t len = WideCharToMultiByte(CP_ACP, 0, w, wcslen(w), 0, 0, 0, 0);
	if (len + 1 > mlen) {
		DPRINTF((TEXT("buffer insufficeint. %d > %d\n"),
		    len + 1, mlen));
		return FALSE;
	}
	int ret = WideCharToMultiByte(CP_ACP, 0, w, len, m, len, 0, 0);
	if (ret == 0) {
		DPRINTF((TEXT("can't convert to ASCII.\n")));
		return FALSE;
	}
	m[len] = '\0';

	return TRUE;
}
