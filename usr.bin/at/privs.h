/*	$NetBSD: privs.h,v 1.3.4.1 1998/10/24 00:23:12 cgd Exp $	*/

/* 
 *  privs.h - header for privileged operations 
 *  Copyright (C) 1993  Thomas Koenig
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * From: OpenBSD: privs.h,v 1.4 1997/03/01 23:40:12 millert Exp
 */

#ifndef _PRIVS_H
#define _PRIVS_H

#include <unistd.h>

/* Relinquish privileges temporarily for a setuid or setgid program
 * with the option of getting them back later.  This is done by
 * utilizing POSIX saved user and groups ids.  Call RELINQUISH_PRIVS once
 * at the beginning of the main program.  This will cause all operatons
 * to be executed with the real userid.  When you need the privileges
 * of the setuid/setgid invocation, call PRIV_START; when you no longer
 * need it, call PRIV_END.  Note that it is an error to call PRIV_START
 * and not PRIV_END within the same function.
 *
 * Use RELINQUISH_PRIVS_ROOT(a,b) if your program started out running
 * as root, and you want to drop back the effective userid to a
 * and the effective group id to b, with the option to get them back
 * later.
 *
 * Problems: Do not use return between PRIV_START and PRIV_END; this
 * will cause the program to continue running in an unprivileged
 * state.
 *
 * It is NOT safe to call exec(), system() or popen() with a user-
 * supplied program (i.e. without carefully checking PATH and any
 * library load paths) with relinquished privileges; the called program
 * can aquire them just as easily.  Set both effective and real userid
 * to the real userid before calling any of them.
 */

#ifndef MAIN
extern
#endif
uid_t real_uid, effective_uid;

#ifndef MAIN 
extern
#endif
gid_t real_gid, effective_gid;

#define RELINQUISH_PRIVS { \
      real_uid = getuid(); \
      effective_uid = geteuid(); \
      real_gid = getgid(); \
      effective_gid = getegid(); \
      PRIV_END \
}

#define RELINQUISH_PRIVS_ROOT(a, b) { \
	real_uid = (a); \
	effective_uid = geteuid(); \
	real_gid = (b); \
	effective_gid = getegid(); \
	PRIV_END \
}

#define PRIV_START { \
	if (seteuid(effective_uid) == -1) \
		perr("Cannot get user privs"); \
	if (setegid(effective_gid) == -1) \
		perr("Cannot get group privs"); \
}

#define PRIV_END { \
	if (setegid(real_gid) == -1) \
		perr("Cannot relinguish group privs"); \
	if (seteuid(real_uid) == -1) \
		perr("Cannot relinguish user privs"); \
}

#endif
