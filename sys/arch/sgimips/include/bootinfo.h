/*	$NetBSD: bootinfo.h,v 1.2.20.2 2004/09/18 14:39:48 skrll Exp $	*/

/*
 * Copyright (c) 1997
 *	Matthias Drochner.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _SGIMIPS_BOOTINFO_H_
#define _SGIMIPS_BOOTINFO_H_

#define	BOOTINFO_MAGIC		0x20011121

struct btinfo_common {
	struct btinfo_common *next;
	int type;
};

#define BTINFO_MAGIC	1
#define BTINFO_BOOTPATH 2
#define BTINFO_SYMTAB	3

struct btinfo_magic {
	struct btinfo_common common;
	int magic;
};

#define BTINFO_BOOTPATH_LEN	80
struct btinfo_bootpath {
	struct btinfo_common common;
	char bootpath[BTINFO_BOOTPATH_LEN];
};

struct btinfo_symtab {
	struct btinfo_common common;
	unsigned long nsym;
	unsigned long ssym;
	unsigned long esym;
};

#ifdef _KERNEL
void	*lookup_bootinfo(int);
#endif

#endif	/* !_SGIMIPS_BOOTINFO_H_ */
