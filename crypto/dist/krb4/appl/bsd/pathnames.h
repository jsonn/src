/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	from: @(#)pathnames.h	5.2 (Berkeley) 4/9/90
 *	$Id: pathnames.h,v 1.1.1.1.4.2 2000/06/16 18:46:17 thorpej Exp $
 */

/******* First fix default path, we stick to _PATH_DEFPATH everywhere */

#if !defined(_PATH_DEFPATH) && defined(_PATH_USERPATH)
#define _PATH_DEFPATH _PATH_USERPATH
#endif

#if defined(_PATH_DEFPATH) && !defined(_DEF_PATH)
#define _DEF_PATH _PATH_DEFPATH
#endif

#if !defined(_PATH_DEFPATH) && defined(_DEF_PATH)
#define _PATH_DEFPATH _DEF_PATH
#endif

#ifndef _PATH_DEFPATH
#define _PATH_DEFPATH "/usr/ucb:/usr/bin:/bin"
#define _DEF_PATH _PATH_DEFPATH
#endif /* !_PATH_DEFPATH */

#ifndef _PATH_DEFSUPATH
#define _PATH_DEFSUPATH "/usr/sbin:"  _DEF_PATH
#endif /* _PATH_DEFSUPATH */

/******* Default PATH fixed! */

#undef  _PATH_RLOGIN		/* Redifine rlogin */
#define	_PATH_RLOGIN	BINDIR  "/rlogin"

#undef _PATH_RSH		/* Redifine rsh */
#define _PATH_RSH	BINDIR  "/rsh"

#undef _PATH_RCP		/* Redifine rcp */
#define _PATH_RCP	BINDIR  "/rcp"

#undef _PATH_LOGIN
#define _PATH_LOGIN	BINDIR "/login"

/******* The rest is fallback defaults */

#ifndef _PATH_DEV
#define _PATH_DEV "/dev/"
#endif

#ifndef _PATH_CP
#define _PATH_CP "/bin/cp"
#endif /* _PATH_CP */

#ifndef _PATH_SHELLS
#define _PATH_SHELLS "/etc/shells"
#endif /* _PATH_SHELLS */

#ifndef _PATH_BSHELL
#define _PATH_BSHELL "/bin/sh"
#endif /* _PATH_BSHELL */

#ifndef _PATH_CSHELL
#define _PATH_CSHELL "/bin/csh"
#endif /* _PATH_CSHELL */

#ifndef _PATH_NOLOGIN
#define _PATH_NOLOGIN "/etc/nologin"
#endif /* _PATH_NOLOGIN */

#ifndef _PATH_TTY
#define _PATH_TTY "/dev/tty"
#endif /* _PATH_TTY */

#ifndef _PATH_HUSHLOGIN
#define _PATH_HUSHLOGIN ".hushlogin"
#endif /* _PATH_HUSHLOGIN */

#ifndef _PATH_NOMAILCHECK
#define _PATH_NOMAILCHECK ".nomailcheck"
#endif /* _PATH_NOMAILCHECK */

#ifndef _PATH_MOTDFILE
#define _PATH_MOTDFILE "/etc/motd"
#endif /* _PATH_MOTDFILE */

#ifndef _PATH_LOGACCESS
#define _PATH_LOGACCESS "/etc/login.access"
#endif /* _PATH_LOGACCESS */

#ifndef _PATH_HEQUIV
#define _PATH_HEQUIV "/etc/hosts.equiv"
#endif

#ifndef _PATH_FBTAB
#define _PATH_FBTAB "/etc/fbtab"
#endif /* _PATH_FBTAB */

#ifndef _PATH_LOGINDEVPERM
#define _PATH_LOGINDEVPERM "/etc/logindevperm"
#endif /* _PATH_LOGINDEVPERM */

#ifndef _PATH_CHPASS
#define _PATH_CHPASS "/usr/bin/passwd"
#endif /* _PATH_CHPASS */

#if defined(__hpux)
#define __FALLBACK_MAILDIR__ "/usr/mail"
#else
#define __FALLBACK_MAILDIR__ "/usr/spool/mail"
#endif

#ifndef KRB4_MAILDIR
#ifndef _PATH_MAILDIR
#ifdef MAILDIR
#define _PATH_MAILDIR MAILDIR
#else
#define _PATH_MAILDIR __FALLBACK_MAILDIR__
#endif
#endif /* _PATH_MAILDIR */
#define KRB4_MAILDIR _PATH_MAILDIR
#endif

#ifndef _PATH_LASTLOG
#define _PATH_LASTLOG		"/var/adm/lastlog"
#endif

#if defined(UTMP_FILE) && !defined(_PATH_UTMP)
#define _PATH_UTMP UTMP_FILE
#endif

#ifndef _PATH_UTMP
#define _PATH_UTMP   "/etc/utmp"
#endif

#if defined(WTMP_FILE) && !defined(_PATH_WTMP)
#define _PATH_WTMP WTMP_FILE
#endif

#ifndef _PATH_WTMP
#define _PATH_WTMP   "/usr/adm/wtmp"
#endif

#ifndef _PATH_ETC_DEFAULT_LOGIN
#define _PATH_ETC_DEFAULT_LOGIN	"/etc/default/login"
#endif

#ifndef _PATH_ETC_ENVIRONMENT
#define _PATH_ETC_ENVIRONMENT "/etc/environment"
#endif

#ifndef _PATH_ETC_SECURETTY
#define _PATH_ETC_SECURETTY "/etc/securetty"
#endif

/*
 * NeXT KLUDGE ALERT!!!!!!!!!!!!!!!!!!
 * Some sort of bug in the NEXTSTEP cpp.
 */
#ifdef NeXT
#undef  _PATH_DEFSUPATH
#define _PATH_DEFSUPATH "/usr/sbin:/usr/ucb:/usr/bin:/bin"
#undef  _PATH_RLOGIN
#define	_PATH_RLOGIN	"/usr/athena/bin/rlogin"
#undef  _PATH_RSH
#define _PATH_RSH	"/usr/athena/bin/rsh"
#undef  _PATH_RCP
#define _PATH_RCP	"/usr/athena/bin/rcp"
#undef  _PATH_LOGIN
#define _PATH_LOGIN	"/usr/athena/bin/login"
#endif
