/*	NetBSD: util.h,v 1.10 1997/12/01 02:25:46 lukem Exp	*/

/*-
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <sys/cdefs.h>
#include <sys/ttycom.h>
#include <sys/types.h>
#include <stdio.h>
#include <pwd.h>
#include <termios.h>
#include <utmp.h>

#define	PIDLOCK_NONBLOCK	1
#define PIDLOCK_USEHOSTNAME	2

#define	FPARSELN_UNESCESC	0x01
#define	FPARSELN_UNESCCONT	0x02
#define	FPARSELN_UNESCCOMM	0x04
#define	FPARSELN_UNESCREST	0x08
#define	FPARSELN_UNESCALL	0x0f

__BEGIN_DECLS
void	login __P((struct utmp *));
int	login_tty __P((int));
int	logout __P((const char *));
void	logwtmp __P((const char *, const char *, const char *));
int	pw_lock __P((int retries));
int	pw_mkdb __P((void));
int	pw_abort __P((void));
void	pw_init __P((void));
void	pw_edit __P((int notsetuid, const char *filename));
void	pw_prompt __P((void));
void	pw_copy __P((int ffd, int tfd, struct passwd *pw,
		     struct passwd *old_pw));
int	pw_scan __P((char *bp, struct passwd *pw, int *flags));
void	pw_error __P((const char *name, int err, int eval));
int	openpty __P((int *, int *, char *, struct termios *,
		     struct winsize *));
char   *fparseln __P((FILE *, size_t *, size_t *, const char[3], int));
pid_t	forkpty __P((int *, char *, struct termios *, struct winsize *));
int	getmaxpartitions __P((void));
int	getrawpartition __P((void));
int	opendisk __P((const char *, int, char *, size_t, int));
int	pidlock __P((const char *, int, pid_t *, const char *));
int	ttylock __P((const char *, int, pid_t *));
int	ttyunlock __P((const char *));
int	ttyaction __P((char *tty, char *act, char *user));
struct iovec;
char   *ttymsg __P((struct iovec *, int, const char *, int));
__END_DECLS

#endif /* !_UTIL_H_ */
