/* $NetBSD: boot_flag.h,v 1.1.4.1 2001/06/21 20:09:43 nathanw Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
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

#ifndef _SYS_BOOT_FLAG_H_
#define _SYS_BOOT_FLAG_H_

#include <sys/reboot.h>

/*
 * Recognize standard boot arguments. If the flag is known, appropriate
 * value is or'ed to retval, otherwise retval is left intact.
 * Note that not all ports use all flags recognized here. This list is mere
 * concatenation of all non-conflicting standard boot flags. Individual ports
 * might use also other flags (see e.g. alpha).
 */
#define	BOOT_FLAG(arg, retval) do {				\
	switch (arg) {						\
	case 'a': /* ask for file name to boot from */		\
		(retval) |= RB_ASKNAME;				\
		break;						\
	case 'b': /* always halt, never reboot */		\
		(retval) |= RB_HALT;				\
		break;						\
	case 'd': /* break into the kernel debugger ASAP (if compiled in) */ \
		(retval) |= RB_KDB;				\
		break;						\
	case 'm': /* mini root present in memory */		\
		(retval) |= RB_MINIROOT;			\
		break;						\
	case 'q': /* boot quietly */				\
		(retval) |= AB_QUIET;				\
		break;						\
	case 's': /* boot to single user */			\
		(retval) |= RB_SINGLE;				\
		break;						\
	case 'v': /* boot verbosely */				\
		(retval) |= AB_VERBOSE;				\
		break;						\
	default:  /* something else, do nothing */		\
		break;						\
	} /* switch */						\
								\
	} while (/* CONSTCOND */ 0)

#endif /* _SYS_BOOT_FLAG_H_ */
