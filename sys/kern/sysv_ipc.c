/*	$NetBSD: sysv_ipc.c,v 1.14.30.1 2001/11/12 21:18:56 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysv_ipc.c,v 1.14.30.1 2001/11/12 21:18:56 thorpej Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/ipc.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/stat.h>

/*
 * Check for ipc permission
 */

int
ipcperm(cred, perm, mode)
	struct ucred *cred;
	struct ipc_perm *perm;
	int mode;
{
	mode_t mask;

	if (cred->cr_uid == 0)
		return (0);

	if (mode == IPC_M) {
		if (cred->cr_uid == perm->uid ||
		    cred->cr_uid == perm->cuid)
			return (0);
		return (EPERM);
	}

	mask = 0;

	if (cred->cr_uid == perm->uid ||
	    cred->cr_uid == perm->cuid) {
		if (mode & IPC_R)
			mask |= S_IRUSR;
		if (mode & IPC_W)
			mask |= S_IWUSR;
		return ((perm->mode & mask) == mask ? 0 : EACCES);
	}

	if (cred->cr_gid == perm->gid || groupmember(perm->gid, cred) ||
	    cred->cr_gid == perm->cgid || groupmember(perm->cgid, cred)) {
		if (mode & IPC_R)
			mask |= S_IRGRP;
		if (mode & IPC_W)
			mask |= S_IWGRP;
		return ((perm->mode & mask) == mask ? 0 : EACCES);
	}

	if (mode & IPC_R)
		mask |= S_IROTH;
	if (mode & IPC_W)
		mask |= S_IWOTH;
	return ((perm->mode & mask) == mask ? 0 : EACCES);
}
