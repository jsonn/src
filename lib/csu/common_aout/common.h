/*	$NetBSD: common.h,v 1.14.18.1 2008/05/18 12:30:10 yamt Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

#include <string.h>

#ifdef DYNAMIC

#include <sys/syscall.h>
#include <a.out.h>
#ifndef N_GETMAGIC
#define N_GETMAGIC(x)	((x).a_magic)
#endif
#ifndef N_BSSADDR
#define N_BSSADDR(x)	(N_DATADDR(x)+(x).a_data)
#endif

#include <sys/mman.h>
#ifdef sun
#define MAP_ANON	0
#endif

#include <dlfcn.h>
#include <link.h>

extern struct _dynamic	_DYNAMIC;
static void		__load_rtld __P((struct _dynamic *));
extern int		__syscall __P((int, ...));
int			_callmain __P((void));
static char		*_strrchr __P((char *, char));
#ifdef DEBUG
static char		*_getenv __P((char *));
static int		_strncmp __P((char *, char *, int));
#endif

#ifdef sun
#define LDSO	"/usr/lib/ld.so"
#endif
#ifdef __NetBSD__
#ifndef LDSO
#define LDSO	"/usr/libexec/ld.so"
#endif
#endif

/*
 * We need these system calls, but can't use library stubs
 */
#define _exit(v)		__syscall(SYS_exit, (v))
#define open(name, f, m)	__syscall(SYS_open, (name), (f), (m))
#define close(fd)		__syscall(SYS_close, (fd))
#define read(fd, s, n)		__syscall(SYS_read, (fd), (s), (n))
#define write(fd, s, n)		__syscall(SYS_write, (fd), (s), (n))
#define dup(fd)			__syscall(SYS_dup, (fd))
#define dup2(fd, fdnew)		__syscall(SYS_dup2, (fd), (fdnew))
#ifdef sun
#define mmap(addr, len, prot, flags, fd, off)	\
    __syscall(SYS_mmap, (addr), (len), (prot), _MAP_NEW|(flags), (fd), (off))
#else
#define mmap(addr, len, prot, flags, fd, off)	\
    __syscall(SYS___syscall, (quad_t)SYS_mmap, (addr), (len), (prot), (flags), \
	(fd), 0, (off_t)(off))
#endif

#define _FATAL(str) \
	write(2, str, sizeof(str) - 1), \
	_exit(1);

#endif /* DYNAMIC */

extern int		main __P((int, char **, char **));
#ifdef MCRT0
extern void		monstartup __P((u_long, u_long));
extern void		_mcleanup __P((void));
#endif

char			**environ;
int			errno;
static char		empty[1];
char			*__progname = empty;
struct ps_strings;
struct ps_strings	*__ps_strings = 0;
#ifndef DYNAMIC
#define _strrchr	strrchr
#endif

extern unsigned char	etext;
extern unsigned char	eprol __asm ("eprol");

