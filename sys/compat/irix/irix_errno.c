/*	$NetBSD: irix_errno.c,v 1.1.4.2 2002/01/10 19:51:17 thorpej Exp $ */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
__KERNEL_RCSID(0, "$NetBSD: irix_errno.c,v 1.1.4.2 2002/01/10 19:51:17 thorpej Exp $");

#include <compat/irix/irix_errno.h>



const int native_to_irix_errno[] = {
	0,		/* 0 */
	IRIX_EPERM,
	IRIX_ENOENT,
	IRIX_ESRCH,
	IRIX_EINTR,
	IRIX_EIO,	/* 5 */
	IRIX_ENXIO,
	IRIX_E2BIG,
	IRIX_ENOEXEC,
	IRIX_EBADF,
	IRIX_ECHILD,	/* 10 */
	IRIX_EAGAIN,
	IRIX_ENOMEM,
	IRIX_EACCES,
	IRIX_EFAULT,
	IRIX_ENOTBLK,	/* 15 */
	IRIX_EBUSY,
	IRIX_EEXIST,
	IRIX_EXDEV,
	IRIX_ENODEV,
	IRIX_ENOTDIR,	/* 20 */
	IRIX_EISDIR,
	IRIX_EINVAL,
	IRIX_ENFILE,
	IRIX_EMFILE,
	IRIX_ENOTTY,	/* 25 */
	IRIX_ETXTBSY,
	IRIX_EFBIG,
	IRIX_ENOSPC,
	IRIX_ESPIPE,
	IRIX_EROFS,	/* 30 */
	IRIX_EMLINK,
	IRIX_EPIPE,
	IRIX_EDOM,
	IRIX_ERANGE,
	IRIX_EAGAIN,	/* 35 */
	IRIX_EWOULDBLOCK,
	IRIX_EINPROGRESS,
	IRIX_EALREADY,
	IRIX_ENOTSOCK,
	IRIX_EDESTADDRREQ,	/* 40 */
	IRIX_EMSGSIZE,
	IRIX_EPROTOTYPE,
	IRIX_ENOPROTOOPT,
	IRIX_EPROTONOSUPPORT,
	IRIX_ESOCKTNOSUPPORT,	/* 45 */
	IRIX_EOPNOTSUPP,
	IRIX_EPFNOSUPPORT,
	IRIX_EAFNOSUPPORT,
	IRIX_EADDRINUSE,
	IRIX_EADDRNOTAVAIL,	/* 50 */
	IRIX_ENETDOWN,
	IRIX_ENETUNREACH,
	IRIX_ENETRESET,
	IRIX_ECONNABORTED,
	IRIX_ECONNRESET,	/* 55 */
	IRIX_ENOBUFS,
	IRIX_EISCONN,
	IRIX_ENOTCONN,
	IRIX_ESHUTDOWN,
	IRIX_ETOOMANYREFS,	/* 60 */
	IRIX_ETIMEDOUT,
	IRIX_ECONNREFUSED,
	IRIX_ELOOP,
	IRIX_ENAMETOOLONG,
	IRIX_EHOSTDOWN,		/* 65 */
	IRIX_EHOSTUNREACH,
	IRIX_ENOTEMPTY,
	IRIX_EPROCLIM,
	IRIX_EUSERS,
	IRIX_EDQUOT,		/* 70 */
	IRIX_ESTALE,
	IRIX_EREMOTE,
	0,			/* EBADRPC */
	0,			/* ERPCMISMATCH	*/
	0,			/* EPROGUNAVAIL */ /* 75 */
	0,			/* EPROGMISMATCH */
	0,			/* EPROCUNAVAIL */
	IRIX_ENOLCK,
	IRIX_ENOSYS,
	0,			/* EFTYPE */		/* 80 */
	0,			/* EAUTH */
	0,			/* ENEEDAUTH */
	IRIX_EIDRM,
	IRIX_ENOMSG,
	IRIX_EOVERFLOW,		/* 85 */
	0,			/* EILSEG */
	0,			/* ELAST */
	IRIX_ERESTART,
	0,			/* EJUSTRETURN */
};
