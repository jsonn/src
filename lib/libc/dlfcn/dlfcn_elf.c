/*	$NetBSD: dlfcn_elf.c,v 1.2.2.1 2002/08/01 03:28:08 nathanw Exp $	*/

/*
 * Copyright (c) 2000 Takuya SHIOZAKI
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: dlfcn_elf.c,v 1.2.2.1 2002/08/01 03:28:08 nathanw Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#define ELFSIZE ARCH_ELFSIZE
#include "rtld.h"

#ifdef __weak_extern
__weak_extern(__mainprog_obj)
#endif
extern const Obj_Entry *__mainprog_obj;

#ifdef __weak_alias
__weak_alias(dlopen,__dlopen)
__weak_alias(dlclose,__dlclose)
__weak_alias(dlsym,__dlsym)
__weak_alias(dlerror,__dlerror)
__weak_alias(dladdr,__dladdr)
#endif

#include <dlfcn_stubs.c>
