/*	$NetBSD: linux_types.h,v 1.15.76.1 2008/05/16 02:23:39 yamt Exp $	*/

/*-
 * Copyright (c) 1995, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden and Eric Haszlakiewicz.
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

#ifndef _I386_LINUX_TYPES_H
#define _I386_LINUX_TYPES_H

typedef unsigned short linux_uid_t;
typedef unsigned short linux_gid_t;
typedef unsigned short linux_dev_t;
typedef unsigned long linux_ino_t;
typedef unsigned short linux_mode_t;
typedef unsigned short linux_nlink_t;
typedef long linux_time_t;
typedef long linux_clock_t;
typedef long linux_off_t;
typedef int linux_pid_t;

/* From linux_termios.h */
typedef unsigned char linux_cc_t;
typedef unsigned long linux_speed_t;
typedef unsigned long linux_tcflag_t;

#define LINUX_STAT_HAS_NSEC	1
struct linux_stat {
	linux_dev_t		lst_dev;
	unsigned short		pad1;
	linux_ino_t		lst_ino;
	linux_mode_t		lst_mode;
	linux_nlink_t		lst_nlink;
	linux_uid_t		lst_uid;
	linux_gid_t		lst_gid;
	linux_dev_t		lst_rdev;
	unsigned short		pad2;
	linux_off_t		lst_size;
	unsigned long		lst_blksize;
	unsigned long		lst_blocks;
	linux_time_t		lst_atime;
	unsigned long		lst_atime_nsec;
	linux_time_t		lst_mtime;
	unsigned long		lst_mtime_nsec;
	linux_time_t		lst_ctime;
	unsigned long		lst_ctime_nsec;
	unsigned long		unused4;
	unsigned long		unused5;
};

/* This matches struct stat64 in glibc2.1, hence the absolutely
 * insane amounts of padding around dev_t's.
 */
#define LINUX_STAT64_HAS_NSEC	1
struct linux_stat64 {
	unsigned long long lst_dev;
	unsigned int	__pad1;

#define LINUX_STAT64_HAS_BROKEN_ST_INO	1
	unsigned int	__lst_ino;
	unsigned int	lst_mode;
	unsigned int	lst_nlink;

	unsigned int	lst_uid;
	unsigned int	lst_gid;

	unsigned long long	lst_rdev;
	unsigned int	__pad2;

	long long	lst_size;
	unsigned int	lst_blksize;

	unsigned long long lst_blocks;	/* Number 512-byte blocks allocated. */

	unsigned int	lst_atime;
	unsigned int	lst_atime_nsec;

	unsigned int	lst_mtime;
	unsigned int	lst_mtime_nsec;

	unsigned int	lst_ctime;
	unsigned int	lst_ctime_nsec;

	unsigned long long lst_ino;
};

#endif /* !_I386_LINUX_TYPES_H */
