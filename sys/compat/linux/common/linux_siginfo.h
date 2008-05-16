/* 	$NetBSD: linux_siginfo.h,v 1.10.74.1 2008/05/16 02:23:43 yamt Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Eric Haszlakiewicz.
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

#ifndef _LINUX_SIGINFO_H
#define _LINUX_SIGINFO_H

#if defined(__i386__)
#include <compat/linux/arch/i386/linux_siginfo.h>
#elif defined(__m68k__)
#include <compat/linux/arch/m68k/linux_siginfo.h>
#elif defined(__alpha__)
#include <compat/linux/arch/alpha/linux_siginfo.h>
#elif defined(__powerpc__)
#include <compat/linux/arch/powerpc/linux_siginfo.h>
#elif defined(__mips__)
#include <compat/linux/arch/mips/linux_siginfo.h>
#elif defined(__arm__)
#include <compat/linux/arch/arm/linux_siginfo.h>
#elif defined(__amd64__)
#include <compat/linux/arch/amd64/linux_siginfo.h>
#endif

/* From linux/include/asm-generic/siginfo.h */
#define LINUX_CLD_EXITED	1
#define LINUX_CLD_KILLED	2
#define LINUX_CLD_DUMPED	3

#endif /* !_LINUX_SIGINFO_H */
