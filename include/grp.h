/*	$NetBSD: grp.h,v 1.14.12.1 2002/01/28 21:27:19 nathanw Exp $	*/

/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)grp.h	8.2 (Berkeley) 1/21/94
 */

#ifndef _GRP_H_
#define	_GRP_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>
#include <sys/types.h>

#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
#define	_PATH_GROUP		"/etc/group"
#endif

struct group {
	__aconst char *gr_name;			/* group name */
	__aconst char *gr_passwd;		/* group password */
	gid_t	gr_gid;				/* group id */
	__aconst char *__aconst *gr_mem;	/* group members */
};

__BEGIN_DECLS
struct group	*getgrgid __P((gid_t));
struct group	*getgrnam __P((const char *));
#if !defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE)
struct group	*getgrent __P((void));
void		 setgrent __P((void));
void		 endgrent __P((void));
#endif
#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
void		 setgrfile __P((const char *));
int		 setgroupent __P((int));
const char	*group_from_gid __P((gid_t, int));
int		 gid_from_group __P((const char *, gid_t *));
int		 pwcache_groupdb __P((int (*)(int), void (*)(void),
				    struct group * (*)(const char *),
				    struct group * (*)(gid_t)));
#endif
__END_DECLS

#endif /* !_GRP_H_ */
