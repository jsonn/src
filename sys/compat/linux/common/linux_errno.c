/*	$NetBSD: linux_errno.c,v 1.9.12.1 2001/02/11 19:13:59 bouyer Exp $	*/

/*-
 * Copyright (c) 1995 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden.
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

#include <compat/linux/common/linux_errno.h>

/*
 * This list is used to translate NetBSD errnos to linux errnos
 * when returning from a system call. (NetBSD system call->linux program)
 */
const int native_to_linux_errno[] = {
	0,
	-LINUX_EPERM,
	-LINUX_ENOENT,
	-LINUX_ESRCH,
	-LINUX_EINTR,
	-LINUX_EIO,
	-LINUX_ENXIO,
	-LINUX_E2BIG,
	-LINUX_ENOEXEC,
	-LINUX_EBADF,
	-LINUX_ECHILD,
	-LINUX_EDEADLK,
	-LINUX_ENOMEM,
	-LINUX_EACCES,
	-LINUX_EFAULT,
	-LINUX_ENOTBLK,
	-LINUX_EBUSY,
	-LINUX_EEXIST,
	-LINUX_EXDEV,
	-LINUX_ENODEV,
	-LINUX_ENOTDIR,
	-LINUX_EISDIR,
	-LINUX_EINVAL,
	-LINUX_ENFILE,
	-LINUX_EMFILE,
	-LINUX_ENOTTY,
	-LINUX_ETXTBSY,
	-LINUX_EFBIG,
	-LINUX_ENOSPC,
	-LINUX_ESPIPE,
	-LINUX_EROFS,
	-LINUX_EMLINK,
	-LINUX_EPIPE,
	-LINUX_EDOM,
	-LINUX_ERANGE,
	-LINUX_EAGAIN,
	-LINUX_EINPROGRESS,
	-LINUX_EALREADY,
	-LINUX_ENOTSOCK,
	-LINUX_EDESTADDRREQ,
	-LINUX_EMSGSIZE,
	-LINUX_EPROTOTYPE,
	-LINUX_ENOPROTOOPT,
	-LINUX_EPROTONOSUPPORT,
	-LINUX_ESOCKTNOSUPPORT,
	-LINUX_EOPNOTSUPP,
	-LINUX_EPFNOSUPPORT,
	-LINUX_EAFNOSUPPORT,
	-LINUX_EADDRINUSE,
	-LINUX_EADDRNOTAVAIL,
	-LINUX_ENETDOWN,
	-LINUX_ENETUNREACH,
	-LINUX_ENETRESET,
	-LINUX_ECONNABORTED,
	-LINUX_ECONNRESET,
	-LINUX_ENOBUFS,
	-LINUX_EISCONN,
	-LINUX_ENOTCONN,
	-LINUX_ESHUTDOWN,
	-LINUX_ETOOMANYREFS,
	-LINUX_ETIMEDOUT,
	-LINUX_ECONNREFUSED,
	-LINUX_ELOOP,
	-LINUX_ENAMETOOLONG,
	-LINUX_EHOSTDOWN,
	-LINUX_EHOSTUNREACH,
	-LINUX_ENOTEMPTY,
	-LINUX_ENOSYS,		/* not mapped (EPROCLIM) */
	-LINUX_EUSERS,
	-LINUX_EDQUOT,
	-LINUX_ESTALE,
	-LINUX_EREMOTE,
	-LINUX_ENOSYS,		/* not mapped (EBADRPC) */
	-LINUX_ENOSYS,		/* not mapped (ERPCMISMATCH) */
	-LINUX_ENOSYS,		/* not mapped (EPROGUNAVAIL) */
	-LINUX_ENOSYS,		/* not mapped (EPROGMISMATCH) */
	-LINUX_ENOSYS,		/* not mapped (EPROCUNAVAIL) */
	-LINUX_ENOLCK,
	-LINUX_ENOSYS,
	-LINUX_ENOSYS,		/* not mapped (EFTYPE) */
	-LINUX_ENOSYS,		/* not mapped (EAUTH) */
	-LINUX_ENOSYS,		/* not mapped (ENEEDAUTH) */
	-LINUX_EIDRM,
	-LINUX_ENOMSG,		/* 83 */

	/*
	 * The rest of the list consists of errors that only
	 * Linux has. They can be used to map them on to
	 * themselves, so Linux emulating syscalls can return
	 * these values.
	 */

	-LINUX_ECHRNG,
	-LINUX_EL2NSYNC,
	-LINUX_EL3HLT,
	-LINUX_EL3RST,
	-LINUX_ELNRNG,
	-LINUX_EUNATCH,
	-LINUX_ENOCSI,
	-LINUX_EL2HLT,
	-LINUX_EBADE,
	-LINUX_EBADR,
	-LINUX_EXFULL,
	-LINUX_ENOANO,
	-LINUX_EBADRQC,
	-LINUX_EBADSLT,
	-LINUX_EDEADLOCK,
	-LINUX_EBFONT,
	-LINUX_ENOSTR,
	-LINUX_ENODATA,
	-LINUX_ETIME,
	-LINUX_ENOSR,
	-LINUX_ENONET,
	-LINUX_ENOPKG,
	-LINUX_ENOLINK,
	-LINUX_EADV,
	-LINUX_ESRMNT,
	-LINUX_ECOMM,
	-LINUX_EPROTO,
	-LINUX_EMULTIHOP,
	-LINUX_EDOTDOT,
	-LINUX_EBADMSG,
	-LINUX_EOVERFLOW,
	-LINUX_ENOTUNIQ,
	-LINUX_EBADFD,
	-LINUX_EREMCHG,
	-LINUX_ELIBACC,
	-LINUX_ELIBBAD,
	-LINUX_ELIBSCN,
	-LINUX_ELIBMAX,
	-LINUX_ELIBEXEC,
	-LINUX_EILSEQ,
	-LINUX_ERESTART,
	-LINUX_ESTRPIPE,
	-LINUX_EUCLEAN,
	-LINUX_ENOTNAM,
	-LINUX_ENAVAIL,
	-LINUX_EISNAM,
	-LINUX_EREMOTEIO,
};
