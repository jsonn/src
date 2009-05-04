/*	$NetBSD: linux32_mman.c,v 1.7.4.1 2009/05/04 08:12:23 yamt Exp $ */

/*-
 * Copyright (c) 2006 Emmanuel Dreyfus, all rights reserved.
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
 *	This product includes software developed by Emmanuel Dreyfus
 * 4. The name of the author may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE THE AUTHOR AND CONTRIBUTORS ``AS IS'' 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: linux32_mman.c,v 1.7.4.1 2009/05/04 08:12:23 yamt Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/fstypes.h>
#include <sys/signal.h>
#include <sys/dirent.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/swap.h>

#include <machine/types.h>

#include <sys/syscallargs.h>

#include <compat/netbsd32/netbsd32.h>
#include <compat/netbsd32/netbsd32_conv.h>
#include <compat/netbsd32/netbsd32_syscallargs.h>

#include <compat/linux/common/linux_types.h>
#include <compat/linux/common/linux_signal.h>
#include <compat/linux/common/linux_machdep.h>
#include <compat/linux/common/linux_misc.h>
#include <compat/linux/common/linux_oldolduname.h>
#include <compat/linux/common/linux_ipc.h>
#include <compat/linux/common/linux_sem.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/common/linux_mmap.h>

#include <compat/linux32/common/linux32_types.h>
#include <compat/linux32/common/linux32_signal.h>
#include <compat/linux32/common/linux32_machdep.h>
#include <compat/linux32/common/linux32_sysctl.h>
#include <compat/linux32/common/linux32_socketcall.h>
#include <compat/linux32/linux32_syscallargs.h>

int
linux32_sys_old_mmap(struct lwp *l, const struct linux32_sys_old_mmap_args *uap, register_t *retval)
{
	/* {
		syscallarg(linux32_oldmmapp) lmp;
	} */
	struct linux_sys_old_mmap_args ua;

	NETBSD32TOP_UAP(lmp, struct linux_oldmmap);
	return linux_sys_old_mmap(l, &ua, retval);
}

int
linux32_sys_mprotect(struct lwp *l, const struct linux32_sys_mprotect_args *uap, register_t *retval)
{
	/* {
		syscallarg(netbsd32_voidp) start;
		syscallarg(netbsd32_long) len;
		syscallarg(int) prot;
	} */
	struct linux_sys_mprotect_args ua;

	NETBSD32TOP_UAP(start, void);
	NETBSD32TOX_UAP(len, long);
	NETBSD32TO64_UAP(prot);
	return (linux_sys_mprotect(l, &ua, retval));
}

int
linux32_sys_mremap(struct lwp *l, const struct linux32_sys_mremap_args *uap, register_t *retval)
{
	/* {
		syscallarg(netbsd32_voidp) old_address;
		syscallarg(netbsd32_size_t) old_size;
		syscallarg(netbsd32_size_t) new_size;
		syscallarg(netbsd32_u_long) flags;
	} */
	struct linux_sys_mremap_args ua;

	NETBSD32TOP_UAP(old_address, void);
	NETBSD32TOX_UAP(old_size, size_t);
	NETBSD32TOX_UAP(new_size, size_t);
	NETBSD32TOX_UAP(flags, u_long);

	return linux_sys_mremap(l, &ua, retval);
}

int
linux32_sys_mmap2(struct lwp *l, const struct linux32_sys_mmap2_args *uap, register_t *retval)
{
	/* {
		syscallarg(netbsd32_u_long) addr;
		syscallarg(netbsd32_size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(linux32_off_t) offset;
	} */
	struct linux_sys_mmap_args ua;

	NETBSD32TOX64_UAP(addr, u_long);
	NETBSD32TOX64_UAP(len, size_t);
	NETBSD32TO64_UAP(prot);
	NETBSD32TO64_UAP(flags);
	NETBSD32TO64_UAP(fd);
	NETBSD32TOX64_UAP(offset, linux_off_t);

	return linux_sys_mmap2(l, &ua, retval);
}
