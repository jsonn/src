/*	$NetBSD: nsswitch.h,v 1.1.4.6 1999/01/15 06:31:27 lukem Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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

#ifndef _NSSWITCH_H
#define _NSSWITCH_H	1

#include <sys/types.h>

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifndef _PATH_NS_CONF
#define _PATH_NS_CONF	"/etc/nsswitch.conf"
#endif

#define	NS_CONTINUE	0
#define	NS_RETURN	1

#define	NS_SUCCESS	(1<<0)		/* entry was found */
#define	NS_UNAVAIL	(1<<1)		/* source not responding, or corrupt */
#define	NS_NOTFOUND	(1<<2)		/* source responded 'no such entry' */
#define	NS_TRYAGAIN	(1<<3)		/* source busy, may respond to retrys */
#define	NS_STATUSMASK	0x000000ff	/* bitmask to get the status flags */

/*
 * currently implemented sources
 */
#define NSSRC_FILES	"files"		/* local files */
#define	NSSRC_DNS	"dns"		/* DNS; IN for hosts, HS for others */
#define	NSSRC_NIS	"nis"		/* yp/nis */
#define	NSSRC_NISPLUS	"nisplus"	/* nis+ */
#define	NSSRC_COMPAT	"compat"	/* passwd,group in yp compat mode */

/*
 * currently implemented databases
 */
#define NSDB_HOSTS		"hosts"
#define NSDB_GROUP		"group"
#define NSDB_GROUP_COMPAT	"group_compat"
#define NSDB_NETGROUP		"netgroup"
#define NSDB_PASSWD		"passwd"
#define NSDB_PASSWD_COMPAT	"passwd_compat"
#define NSDB_SHELLS		"shells"

/*
 * suggested databases to implement
 */
#define NSDB_ALIASES		"aliases"
#define NSDB_AUTH		"auth"
#define NSDB_AUTOMOUNT		"automount"
#define NSDB_BOOTPARAMS		"bootparams"
#define NSDB_ETHERS		"ethers"
#define NSDB_EXPORTS		"exports"
#define NSDB_NETMASKS		"netmasks"
#define NSDB_NETWORKS		"networks"
#define NSDB_PHONES		"phones"
#define NSDB_PRINTCAP		"printcap"
#define NSDB_PROTOCOLS		"protocols"
#define NSDB_REMOTE		"remote"
#define NSDB_RPC		"rpc"
#define NSDB_SENDMAILVARS	"sendmailvars"
#define NSDB_SERVICES		"services"
#define NSDB_TERMCAP		"termcap"
#define NSDB_TTYS		"ttys"

/*
 * ns_dtab - `nsswitch dispatch table'
 * contains an entry for each source and the appropriate function to call
 */
typedef struct ns_dtab {
	const char	 *src;
	int		(*callback)(void *retval, void *cb_data, va_list ap);
	void		 *cb_data;
} ns_dtab;

/*
 * macros to help build an ns_dtab[]
 */
#define NS_FILES_CB(F,C)	{ NSSRC_FILES,	F,	NULL }
 
#ifdef HESIOD
#   define NS_DNS_CB(F,C)	{ NSSRC_DNS,	F,	NULL }
#else
#   define NS_DNS_CB(F,C)	{ NSSRC_DNS,	NULL,	NULL }
#endif

#ifdef YP
#   define NS_NIS_CB(F,C)	{ NSSRC_NIS,	F,	NULL }
#else
#   define NS_NIS_CB(F,C)	{ NSSRC_NIS,	NULL,	NULL }
#endif

#if defined(HESIOD) || defined(YP)
#   define NS_COMPAT_CB(F,C)	{ NSSRC_COMPAT,	F,	NULL }
#else
#   define NS_COMPAT_CB(F,C)	{ NSSRC_COMPAT,	NULL,	NULL }
#endif

/*
 * maximum length of a string (database or source name).
 * a sanity check more than anything else (the strings
 * are dynamically allocated; there may be bugs in the
 * lexer or yaccer with large strings, so we limit the
 * string length.
 */
#define NS_MAXSTR	32


#ifdef _NS_PRIVATE

/*
 * private data structures for back-end nsswitch implementation
 */

/*
 * ns_src - `nsswitch source'
 * used by the nsparser routines to store a mapping between a source
 * and its dispatch control flags for a given database.
 */
typedef struct ns_src {
	const char	*name;
	u_int32_t	 flags;
} ns_src;

/*
 * ns_dbt - `nsswitch database thang'
 * for each database in /etc/nsswitch.conf there is a ns_dbt, with its
 * name and a list of ns_src's containing the source information.
 */
typedef struct ns_dbt {
	const char	*name;		/* name of database */
	ns_src		*srclist;	/* list of sources */
	int		 srclistsize;	/* size of srclist */
} ns_dbt;

#endif /* _NS_PRIVATE */


#include <sys/cdefs.h>

__BEGIN_DECLS
extern	int	nsdispatch	__P((void *, const ns_dtab [], const char *,
				    ...));

#ifdef _NS_PRIVATE
extern	void		 _nsdbtaddsrc __P((ns_dbt *, const ns_src *));
extern	void		 _nsdbtdump __P((const ns_dbt *));
extern	const ns_dbt	*_nsdbtget __P((const char *));
extern	void		 _nsdbtput __P((const ns_dbt *));
extern	void		 _nsyyerror __P((const char *));
extern	int		 _nsyylex __P((void));
extern	int		 _nsyylineno;
#endif /* _NS_PRIVATE */

__END_DECLS

#endif /* !_NSSWITCH_H */
