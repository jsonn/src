/* $NetBSD: linux_mmap.h,v 1.1.12.1 2001/02/11 19:13:52 bouyer Exp $   */

/*-
 * Copyright (c) 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Eric Haszlakiewicz and Emmanuel Dreyfus.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef _POWERPC_LINUX_MMAP_H
#define _POWERPC_LINUX_MMAP_H

/* 
 * LINUX_PROT_* are defined in common/linux_mmap.h 
 * LINUX_MAP_SHARED/PRIVATE are defined in common/linux_mmap.h 
 */

/* 
 * From Linux's include/asm-ppc/mman.h 
 */
#define LINUX_MAP_FIXED		0x0010
#define LINUX_MAP_ANON		0x0020

/* Ignored */
#define LINUX_MAP_NORESERVE	0x0040
#define LINUX_MAP_GROWSDOWN	0x0100
#define LINUX_MAP_DENYWRITE	0x0800
#define	LINUX_MAP_EXECUTABLE	0x1000

/* 
 * On the PowerPC, we have a problem with the offset argument. It's 32 bit
 * long on Linux and 64 bit long on NetBSD. Therefore we use a wrapper
 * function linux_sys_powerpc_mmap() to linux_sys_mmap()
 * 
 * Linux's off_t is __kernel_off_t (include/linux/types.h) which in turn
 * is a long (include/asm-ppc/posix_types.h)
 */
#define linux_off_t long

#ifdef _KERNEL
__BEGIN_DECLS 
int linux_sys_powerpc_mmap(struct proc *, void *, register_t *);
__END_DECLS
#endif /* !_KERNEL */ 

#endif /* !_POWERPC_LINUX_MMAP_H */
