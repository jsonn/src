/*	$NetBSD: netdb.h,v 1.48.2.1 2005/03/21 23:07:15 tron Exp $	*/

/*
 * Copyright (c) 1980, 1983, 1988, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * Portions Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 *    This product includes software developed by WIDE Project and
 *    its contributors.
 * 4. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * --Copyright--
 */

/*
 *      @(#)netdb.h	8.1 (Berkeley) 6/2/93
 *	Id: netdb.h,v 1.12.2.1.4.4 2004/03/16 02:19:19 marka Exp
 */

#ifndef _NETDB_H_
#define	_NETDB_H_

#include <machine/ansi.h>
#include <machine/endian_machdep.h>
#include <sys/ansi.h>
#include <sys/cdefs.h>
#include <sys/featuretest.h>
#include <inttypes.h>
/*
 * Data types
 */
#ifndef socklen_t
typedef __socklen_t	socklen_t;
#define	socklen_t	__socklen_t
#endif

#ifdef  _BSD_SIZE_T_
typedef _BSD_SIZE_T_	size_t;
#undef  _BSD_SIZE_T_
#endif

#if defined(_NETBSD_SOURCE)
#ifndef _PATH_HEQUIV
#define	_PATH_HEQUIV	"/etc/hosts.equiv"
#endif
#ifndef _PATH_HOSTS
#define	_PATH_HOSTS	"/etc/hosts"
#endif
#ifndef _PATH_NETWORKS
#define	_PATH_NETWORKS	"/etc/networks"
#endif
#ifndef _PATH_PROTOCOLS
#define	_PATH_PROTOCOLS	"/etc/protocols"
#endif
#ifndef _PATH_SERVICES
#define	_PATH_SERVICES	"/etc/services"
#endif
#endif

__BEGIN_DECLS
extern int h_errno;
__END_DECLS

/*
 * Structures returned by network data base library.  All addresses are
 * supplied in host order, and returned in network order (suitable for
 * use in system calls).
 */
struct	hostent {
	char	*h_name;	/* official name of host */
	char	**h_aliases;	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	char	**h_addr_list;	/* list of addresses from name server */
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

/*
 * Assumption here is that a network number
 * fits in an unsigned long -- probably a poor one.
 */
struct	netent {
	char		*n_name;	/* official name of net */
	char		**n_aliases;	/* alias list */
	int		n_addrtype;	/* net address type */
#if (defined(__sparc__) && defined(_LP64)) || \
    (defined(__sh__) && defined(_LP64) && (_BYTE_ORDER == _BIG_ENDIAN))
	int		__n_pad0;	/* ABI compatibility */
#endif
	uint32_t	n_net;		/* network # */
#if defined(__alpha__) || (defined(__i386__) && defined(_LP64)) || \
    (defined(__sh__) && defined(_LP64) && (_BYTE_ORDER == _LITTLE_ENDIAN))
	int		__n_pad0;	/* ABI compatibility */
#endif
};

struct	servent {
	char	*s_name;	/* official service name */
	char	**s_aliases;	/* alias list */
	int	s_port;		/* port # */
	char	*s_proto;	/* protocol to use */
};

struct	protoent {
	char	*p_name;	/* official protocol name */
	char	**p_aliases;	/* alias list */
	int	p_proto;	/* protocol # */
};

/*
 * Note: ai_addrlen used to be a size_t, per RFC 2553.
 * In XNS5.2, and subsequently in POSIX-2001 and
 * draft-ietf-ipngwg-rfc2553bis-02.txt it was changed to a socklen_t.
 * To accomodate for this while preserving binary compatibility with the
 * old interface, we prepend or append 32 bits of padding, depending on
 * the (LP64) architecture's endianness.
 *
 * This should be deleted the next time the libc major number is
 * incremented.
 */
#if (_POSIX_C_SOURCE - 0) >= 200112L || (_XOPEN_SOURCE - 0) >= 520 || \
    defined(_NETBSD_SOURCE)
struct addrinfo {
	int	ai_flags;	/* AI_xxx */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
#if defined(__sparc__) && defined(_LP64)
	int	__ai_pad0;	/* ABI compatibility */
#endif
	socklen_t ai_addrlen;	/* length of ai_addr */
#if defined(__alpha__) || (defined(__i386__) && defined(_LP64))
	int	__ai_pad0;	/* ABI compatbility */
#endif
	char	*ai_canonname;	/* canonical name for hostname */
	struct sockaddr *ai_addr;	/* binary address */
	struct addrinfo *ai_next;	/* next structure in linked list */
};
#endif

/*
 * Error return codes from gethostbyname() and gethostbyaddr()
 * (left in extern int h_errno).
 */

#if defined(_NETBSD_SOURCE)
#define	NETDB_INTERNAL	-1	/* see errno */
#define	NETDB_SUCCESS	0	/* no problem */
#endif
#define	HOST_NOT_FOUND	1 /* Authoritative Answer Host not found */
#define	TRY_AGAIN	2 /* Non-Authoritive Host not found, or SERVERFAIL */
#define	NO_RECOVERY	3 /* Non recoverable errors, FORMERR, REFUSED, NOTIMP */
#define	NO_DATA		4 /* Valid name, no data record of requested type */
#if defined(_NETBSD_SOURCE)
#define	NO_ADDRESS	NO_DATA		/* no address, look for MX record */
#endif

/*
 * Error return codes from getaddrinfo()
 */
#if (_POSIX_C_SOURCE - 0) >= 200112L || (_XOPEN_SOURCE - 0) >= 520 || \
    defined(_NETBSD_SOURCE)
#define	EAI_ADDRFAMILY	 1	/* address family for hostname not supported */
#define	EAI_AGAIN	 2	/* temporary failure in name resolution */
#define	EAI_BADFLAGS	 3	/* invalid value for ai_flags */
#define	EAI_FAIL	 4	/* non-recoverable failure in name resolution */
#define	EAI_FAMILY	 5	/* ai_family not supported */
#define	EAI_MEMORY	 6	/* memory allocation failure */
#define	EAI_NODATA	 7	/* no address associated with hostname */
#define	EAI_NONAME	 8	/* hostname nor servname provided, or not known */
#define	EAI_SERVICE	 9	/* servname not supported for ai_socktype */
#define	EAI_SOCKTYPE	10	/* ai_socktype not supported */
#define	EAI_SYSTEM	11	/* system error returned in errno */
#define	EAI_BADHINTS	12
#define	EAI_PROTOCOL	13
#define	EAI_MAX		14
#endif /* _POSIX_C_SOURCE >= 200112 || _XOPEN_SOURCE >= 520 || _NETBSD_SOURCE */

/*
 * Flag values for getaddrinfo()
 */
#if (_POSIX_C_SOURCE - 0) >= 200112L || (_XOPEN_SOURCE - 0) >= 520 || \
    defined(_NETBSD_SOURCE)
#define	AI_PASSIVE	0x00000001 /* get address to use bind() */
#define	AI_CANONNAME	0x00000002 /* fill ai_canonname */
#define	AI_NUMERICHOST	0x00000004 /* prevent host name resolution */
#define	AI_NUMERICSERV	0x00000008 /* prevent service name resolution */
/* valid flags for addrinfo (not a standard def, apps should not use it) */
#define	AI_MASK	\
    (AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST | AI_NUMERICSERV)

#if 0
/*
 * Flag values for getipnodebyname()
 */
#define	AI_V4MAPPED	0x00000008
#define	AI_ALL		0x00000010
#define	AI_ADDRCONFIG	0x00000020
#define	AI_DEFAULT	(AI_V4MAPPED|AI_ADDRCONFIG)
#endif
#endif

#if (_POSIX_C_SOURCE - 0) >= 200112L || (_XOPEN_SOURCE - 0) >= 520 || \
    defined(_NETBSD_SOURCE)
/*
 * Constants for getnameinfo()
 */
#if defined(_NETBSD_SOURCE)
#define	NI_MAXHOST	1025
#define	NI_MAXSERV	32
#endif

/*
 * Flag values for getnameinfo()
 */
#define	NI_NOFQDN	0x00000001
#define	NI_NUMERICHOST	0x00000002
#define	NI_NAMEREQD	0x00000004
#define	NI_NUMERICSERV	0x00000008
#define	NI_DGRAM	0x00000010
#define	NI_NUMERICSCOPE	0x00000040

/*
 * Scope delimit character
 */
#if defined(_NETBSD_SOURCE)
#define	SCOPE_DELIMITER	'%'
#endif
#endif /* (_POSIX_C_SOURCE - 0) >= 200112L || ... */

__BEGIN_DECLS
void		endhostent(void);
void		endnetent(void);
void		endprotoent(void);
void		endservent(void);
#if (_XOPEN_SOURCE - 0) >= 520 && (_XOPEN_SOURCE - 0) < 600 || \
    defined(_NETBSD_SOURCE)
#if 0 /* we do not ship this */
void		freehostent(struct hostent *);
#endif
#endif
struct hostent	*gethostbyaddr(const char *, socklen_t, int);
struct hostent	*gethostbyname(const char *);
#if defined(_NETBSD_SOURCE)
struct hostent	*gethostbyname2(const char *, int);
#endif
struct hostent	*gethostent(void);
#if (_XOPEN_SOURCE - 0) >= 520 && (_XOPEN_SOURCE - 0) < 600 || \
    defined(_NETBSD_SOURCE)
#if 0 /* we do not ship these */
struct hostent	*getipnodebyaddr(const void *, size_t, int, int *);
struct hostent	*getipnodebyname(const char *, int, int, int *);
#endif
#endif
struct netent	*getnetbyaddr(uint32_t, int);
struct netent	*getnetbyname(const char *);
struct netent	*getnetent(void);
struct protoent	*getprotobyname(const char *);
struct protoent	*getprotobynumber(int);
struct protoent	*getprotoent(void);
struct servent	*getservbyname(const char *, const char *);
struct servent	*getservbyport(int, const char *);
struct servent	*getservent(void);
#if defined(_NETBSD_SOURCE)
void		herror(const char *);
const char	*hstrerror(int);
#endif
void		sethostent(int);
#if defined(_NETBSD_SOURCE)
/* void		sethostfile(const char *); */
#endif
void		setnetent(int);
void		setprotoent(int);
#if (_POSIX_C_SOURCE - 0) >= 200112L || (_XOPEN_SOURCE - 0) >= 520 || \
    defined(_NETBSD_SOURCE)
void		setservent(int);
int		getaddrinfo(const char * __restrict, const char * __restrict,
				 const struct addrinfo * __restrict,
				 struct addrinfo ** __restrict);
int		getnameinfo(const struct sockaddr * __restrict, socklen_t,
				 char * __restrict, socklen_t,
				 char * __restrict, socklen_t, int);
void		freeaddrinfo(struct addrinfo *);
const char	*gai_strerror(int);
#endif
void		setservent(int);

#if defined(_NETBSD_SOURCE) && defined(_LIBC)

struct protoent_data {
        FILE *fp;
	struct protoent proto;
	char **aliases;
	size_t maxaliases;
	int stayopen;
	char *line;
	void *dummy;
};

struct protoent	*getprotoent_r(struct protoent *, struct protoent_data *);
struct protoent	*getprotobyname_r(const char *,
    struct protoent *, struct protoent_data *);
struct protoent	*getprotobynumber_r(int,
    struct protoent *, struct protoent_data *);
void setprotoent_r(int, struct protoent_data *);
void endprotoent_r(struct protoent_data *);

struct servent_data {
        FILE *fp;
	struct servent serv;
	char **aliases;
	size_t maxaliases;
	int stayopen;
	char *line;
	void *dummy;
};

struct servent	*getservent_r(struct servent *, struct servent_data *);
struct servent	*getservbyname_r(const char *, const char *,
    struct servent *, struct servent_data *);
struct servent	*getservbyport_r(int, const char *,
    struct servent *, struct servent_data *);
void setservent_r(int, struct servent_data *);
void endservent_r(struct servent_data *);

#endif /* _NETBSD_SOURCE && _LIBC */
__END_DECLS

#endif /* !_NETDB_H_ */
