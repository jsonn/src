/*	$NetBSD: linux_exec.h,v 1.2.4.2 2001/09/13 01:15:18 thorpej Exp $ */

/*-
 * Copyright (c) 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus.
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
 *	     This product includes software developed by the NetBSD
 *	     Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *	   contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
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

#ifndef _MIPS_LINUX_EXEC_H
#define _MIPS_LINUX_EXEC_H

#include <sys/exec_aout.h>
#include <sys/exec_elf.h>
#include <sys/types.h>
#include <sys/systm.h>

/*
 * From Linux's include/asm-mips/elf.h
 */
#define LINUX_ELF_HWCAP (0)

/*
 * Linux a.out format parameters
 */
#define LINUX_M_MIPS		MID_MIPS 	/* XXX This is a guess */
#define LINUX_MID_MACHINE	LINUX_M_MIPS	

/*
 * Linux Elf32 format parameters
 */

#define LINUX_GCC_SIGNATURE 1			/* XXX to be tested */

#define LINUX_COPYARGS_FUNCTION ELFNAME2(linux,copyargs)

typedef struct {
	Elf32_Sword a_type;
	Elf32_Word  a_v;
} LinuxAux32Info;
typedef struct {
	Elf64_Sword a_type;
	Elf64_Word  a_v;
} LinuxAux64Info;
#if defined(ELFSIZE) && (ELFSIZE == 64)
#define LinuxAuxInfo LinuxAux64Info
#else
#define LinuxAuxInfo LinuxAux32Info
#endif

#ifdef _KERNEL
__BEGIN_DECLS
void * ELFNAME2(linux,copyargs) __P((struct exec_package *,
    struct ps_strings *, void *, void *)); 
__END_DECLS
#endif /* _KERNEL */

#endif /* !_MIPS_LINUX_EXEC_H */
