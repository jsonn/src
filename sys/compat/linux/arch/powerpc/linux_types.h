/*	$NetBSD: linux_types.h,v 1.4.4.1 2005/04/29 11:28:33 kent Exp $ */

/*-
 * Copyright (c) 1995, 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden, Eric Haszlakiewicz and Emmanuel Dreyfus.
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

#ifndef _POWERPC_LINUX_TYPES_H
#define _POWERPC_LINUX_TYPES_H

/*
 * from Linux's include/asm-ppc/posix-types.h
 */
typedef unsigned int linux_uid_t;
typedef unsigned int linux_gid_t;
typedef unsigned int linux_dev_t;
typedef unsigned int linux_ino_t;
typedef unsigned int linux_mode_t;
typedef unsigned short linux_nlink_t;
typedef long linux_time_t;
typedef long linux_clock_t;
typedef long linux_off_t;
typedef int linux_pid_t;

/*
 * From Linux's include/asm-ppc/termbits.h
 */
typedef unsigned char linux_cc_t;
typedef unsigned int linux_speed_t;
typedef unsigned int linux_tcflag_t;

/*
 * From Linux's include/asm-ppc/stat.h
 */
struct linux_stat {  /* warning: there is also a old_kernel_stat in Linux*/
	linux_dev_t		lst_dev;
	linux_ino_t		lst_ino;
	linux_mode_t	lst_mode;
	linux_nlink_t	lst_nlink;
	linux_uid_t		lst_uid;
	linux_gid_t		lst_gid;
	linux_dev_t		lst_rdev;
	linux_off_t		lst_size;
	unsigned long	lst_blksize;
	unsigned long	lst_blocks;
	unsigned long	lst_atime;
	unsigned long	unused1;
	unsigned long	lst_mtime;
	unsigned long	unused2;
	unsigned long	lst_ctime;
	unsigned long	unused3;
	unsigned long	unused4;
	unsigned long	unused5;
};

/*
 * This matches struct stat64 in glibc2.1, hence the absolutely
 * insane amounts of padding around dev_t's.
 *
 * Still from Linux'sinclude/asm-ppc/stat.h
 */
struct linux_stat64 {
	unsigned long long lst_dev;
	unsigned long long lst_ino;
	unsigned int lst_mode;
	unsigned int lst_nlink;
	unsigned int lst_uid;
	unsigned int lst_gid;
	unsigned long long lst_rdev;
	unsigned short	int __pad2;
	long long lst_size;
	long lst_blksize;
	long long lst_blocks;	/* Number 512-byte blocks allocated. */
	long lst_atime;
	unsigned long int	__unused1;
	long lst_mtime;
	unsigned long int	__unused2;
	long lst_ctime;
	unsigned long int	__unused3;
	unsigned long int	__unused4;
	unsigned long int	__unused5;
};

#endif /* !_POWERPC_LINUX_TYPES_H */
