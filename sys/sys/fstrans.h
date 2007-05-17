/*	$NetBSD: fstrans.h,v 1.3.4.1 2007/05/17 13:41:55 yamt Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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

/*
 * File system transaction operations.
 */

#ifndef _SYS_FSTRANS_H_
#define	_SYS_FSTRANS_H_

#include <sys/mount.h>

#define SUSPEND_SUSPEND	0x0001		/* VFS_SUSPENDCTL: suspend */
#define SUSPEND_RESUME	0x0002		/* VFS_SUSPENDCTL: resume */

enum fstrans_lock_type {
	FSTRANS_LAZY = 1,		/* Granted while not suspended */
	FSTRANS_SHARED = 2		/* Granted while not suspending */
#ifdef _FSTRANS_API_PRIVATE
	,
	FSTRANS_EXCL = 3		/* Internal: exclusive lock */
#endif /* _FSTRANS_API_PRIVATE */
};

enum fstrans_state {
	FSTRANS_NORMAL,
	FSTRANS_SUSPENDING,
	FSTRANS_SUSPENDED
};

void	fstrans_init(void);
#define fstrans_start(mp, t)						\
do {									\
	_fstrans_start((mp), (t), 1);					\
} while (/* CONSTCOND */ 0)
#define fstrans_start_nowait(mp, t)	_fstrans_start((mp), (t), 0)
int	_fstrans_start(struct mount *, enum fstrans_lock_type, int);
void	fstrans_done(struct mount *);
int	fstrans_is_owner(struct mount *);

int	fstrans_setstate(struct mount *, enum fstrans_state);
enum fstrans_state fstrans_getstate(struct mount *);

int	vfs_suspend(struct mount *, int);
void	vfs_resume(struct mount *);

#endif /* _SYS_FSTRANS_H_ */
