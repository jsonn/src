/*	$NetBSD: port_after.h,v 1.1.1.1.8.1 2001/01/28 15:53:03 he Exp $	*/

#ifndef	PORT_AFTER_H
#define	PORT_AFTER_H
#define USE_POSIX
#define POSIX_SIGNALS
#define USE_UTIME
#define USE_WAITPID
#define HAVE_GETRUSAGE
#define HAVE_FCHMOD
#define NEED_PSELECT
#define SETGRENT_VOID
#define SETPWENT_VOID
#define HAVE_SA_LEN
#define HAVE_MINIMUM_IFREQ
#undef USE_LOG_CONS
#define RLIMIT_TYPE rlim_t
#define RLIMIT_LONGLONG
#define RLIMIT_FILE_INFINITY
#define HAVE_CHROOT
#define CAN_CHANGE_ID
#define __BIND_RES_TEXT
#define MISSING_IN6ADDR_ANY

#define _TIMEZONE timezone

#define PORT_NONBLOCK	O_NONBLOCK
#define PORT_WOULDBLK	EWOULDBLOCK
#define WAIT_T		int
#define KSYMS		"/netbsd"
#define KMEM		"/dev/kmem"
#define UDPSUM		"udpcksum"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#if (!defined(BSD)) || (BSD < 199306)
#include <sys/bitypes.h>
#endif

/*
 * We need to know the IPv6 address family number even on IPv4-only systems.
 * Note that this is NOT a protocol constant, and that if the system has its
 * own AF_INET6, different from ours below, all of BIND's libraries and
 * executables will need to be recompiled after the system <sys/socket.h>
 * has had this type added.  The type number below is correct on most BSD-
 * derived systems for which AF_INET6 is defined.
 */
#ifndef AF_INET6
#define AF_INET6	24
#endif

#ifndef	PF_INET6
#define PF_INET6	AF_INET6
#endif

#include <port_ipv6.h>

#ifndef HAS_INET6_STRUCTS
/* Replace with structure from later rev of O/S if known. */
struct in6_addr {
	u_int8_t	s6_addr[16];
};

/* Replace with structure from later rev of O/S if known. */
struct sockaddr_in6 {
#ifdef	HAVE_SA_LEN
	u_int8_t	sin6_len;	/* length of this struct */
	u_int8_t	sin6_family;	/* AF_INET6 */
#else
	u_int16_t	sin6_family;	/* AF_INET6 */
#endif
	u_int16_t	sin6_port;	/* transport layer port # */
	u_int32_t	sin6_flowinfo;	/* IPv6 flow information */
	struct in6_addr	sin6_addr;	/* IPv6 address */
	u_int32_t	sin6_scope_id;	/* set of interfaces for a scope */
};
#endif	/* HAS_INET6_STRUCTS */
#ifndef ISC_FACILITY
#define ISC_FACILITY LOG_DAEMON
#endif

#endif	/* ! PORT_AFTER_H */
