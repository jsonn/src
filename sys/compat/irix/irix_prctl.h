/*	$NetBSD: irix_prctl.h,v 1.1.4.2 2002/01/10 19:51:19 thorpej Exp $ */

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

#ifndef _IRIX_PRCTL_H_
#define _IRIX_PRCTL_H_

/* From IRIX's <sys/prctl.h> */

#define IRIX_PR_MAXPROCS	1
#define IRIX_PR_ISBLOCKED	2
#define IRIX_PR_SETSTACKSIZE	3
#define IRIX_PR_GETSTACKSIZE	4
#define IRIX_PR_MAXPPROCS	5
#define IRIX_PR_UNBLKONEXEC	6
#define IRIX_PR_SETEXITSIG	8
#define IRIX_PR_RESIDENT	9
#define IRIX_PR_ATTACHADDR	10
#define IRIX_PR_DETACHADDR	11
#define IRIX_PR_TERMCHILD	12
#define IRIX_PR_GETSHMASK	13
#define IRIX_PR_GETNSHARE	14
#define IRIX_PR_COREPID		15
#define IRIX_PR_ATTACHADDRPERM	16
#define IRIX_PR_PTHREADEXIT	17
#define IRIX_PR_SETABORTSIG	18
#define IRIX_PR_INIT_THREADS	20
#define IRIX_PR_THREAD_CTL	21
#define IRIX_PR_LASTSHEXIT	22

#endif /* _IRIX_IRIX_PRCTL_H_ */
