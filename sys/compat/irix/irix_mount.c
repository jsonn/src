/*	$NetBSD: irix_mount.c,v 1.17.6.2 2007/12/09 19:36:51 jmcneill Exp $ */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: irix_mount.c,v 1.17.6.2 2007/12/09 19:36:51 jmcneill Exp $");

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>
#include <sys/kauth.h>

#include <compat/common/compat_util.h>

#include <compat/irix/irix_types.h>
#include <compat/irix/irix_signal.h>
#include <compat/irix/irix_syscall.h>
#include <compat/irix/irix_syscallargs.h>

int
irix_sys_getmountid(struct lwp *l, void *v, register_t *retval)
{
	struct irix_sys_getmountid_args /* {
		syscallarg(const char *) path;
		syscallarg(irix_mountid_t *) buf;
	} */ *uap = v;
	kauth_cred_t cred;
	struct vnode *vp;
	int error = 0;
	struct nameidata nd;
	irix_mountid_t mountid;
	void *addr;

	cred = kauth_cred_dup(l->l_cred);
	kauth_cred_seteuid(cred, kauth_cred_getuid(l->l_cred));
	kauth_cred_setegid(cred, kauth_cred_getgid(l->l_cred));

	/* Get the vnode for the requested path */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | TRYEMULROOT, UIO_USERSPACE,
	    SCARG(uap, path));
	nd.ni_cnd.cn_cred = cred;
	if ((error = namei(&nd)) != 0)
		goto out;
	vp = nd.ni_vp;

	/* Check for accessibility */
	if ((error = VOP_ACCESS(vp, VREAD | VEXEC, cred)) != 0)
		goto bad;

	/*
	 * Return the address of the mount structure
	 * as the unique ID for the filesystem
	 */
	addr = (void *)&vp->v_mount;
	bzero((void *)&mountid, sizeof(mountid));
	(void)memcpy((void *)&mountid, &addr, sizeof(addr));
	error = copyout(&mountid, SCARG(uap, buf), sizeof(mountid));

bad:
	vput(vp);
out:
	kauth_cred_free(cred);
	return (error);
}
