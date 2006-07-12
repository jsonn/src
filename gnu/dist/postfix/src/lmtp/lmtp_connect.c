/*	$NetBSD: lmtp_connect.c,v 1.1.1.5.2.1 2006/07/12 15:06:39 tron Exp $	*/

/*++
/* NAME
/*	lmtp_connect 3
/* SUMMARY
/*	connect to LMTP server
/* SYNOPSIS
/*	#include "lmtp.h"
/*
/*	LMTP_SESSION *lmtp_connect(destination, why)
/*	char	*destination;
/*	VSTRING	*why;
/* DESCRIPTION
/*	This module implements LMTP connection management.
/*
/*	lmtp_connect() attempts to establish an LMTP session.
/*
/*	The destination is one of the following:
/* .IP unix:address
/*	Connect to a UNIX-domain service. The destination is a pathname.
/*	Beware: UNIX-domain sockets are flakey on Solaris, at last up to
/*	and including Solaris 7.0.
/* .IP inet:address
/*	Connect to an IPV4 service.
/*	The destination is either a host name or a numeric address.
/*	Symbolic or numeric service port information may be appended,
/*	separated by a colon (":").
/*
/*	Numerical address information should always be quoted with `[]'.
/* .PP
/*	When no transport is specified, "inet" is assumed.
/* DIAGNOSTICS
/*	This routine either returns an LMTP_SESSION pointer, or
/*	returns a null pointer and set the \fIlmtp_errno\fR
/*	global variable accordingly:
/* .IP LMTP_RETRY
/*	The connection attempt failed, but should be retried later.
/* .IP LMTP_FAIL
/*	The connection attempt failed.
/* .PP
/*	In addition, a textual description of the error is made available
/*	via the \fIwhy\fR argument.
/* SEE ALSO
/*	lmtp_proto(3) LMTP client protocol
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*
/*	Alterations for LMTP by:
/*	Philip A. Prindeville
/*	Mirapoint, Inc.
/*	USA.
/*
/*	Additional work on LMTP by:
/*	Amos Gouaux
/*	University of Texas at Dallas
/*	P.O. Box 830688, MC34
/*	Richardson, TX 75083, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

/* Utility library. */

#include <msg.h>
#include <vstream.h>
#include <vstring.h>
#include <split_at.h>
#include <mymalloc.h>
#include <iostuff.h>
#include <timed_connect.h>
#include <stringops.h>
#include <host_port.h>
#include <sane_connect.h>
#include <inet_addr_list.h>
#include <myaddrinfo.h>
#include <sock_addr.h>

/* Global library. */

#include <mail_params.h>
#include <mail_proto.h>
#include <own_inet_addr.h>

/* DNS library. */

#include <dns.h>
	
/* Application-specific. */

#include "lmtp.h"
#include "lmtp_addr.h"

 /*
  * Forward declaration.
  */
static LMTP_SESSION *lmtp_connect_sock(int, struct sockaddr *, int,
				               const char *, const char *,
				               const char *, VSTRING *);

/* lmtp_connect_unix - connect to UNIX-domain address */

static LMTP_SESSION *lmtp_connect_unix(const char *addr,
			              const char *destination, VSTRING *why)
{
#undef sun
    char   *myname = "lmtp_connect_unix";
    struct sockaddr_un sun;
    int     len = strlen(addr);
    int     sock;

    /*
     * Sanity checks.
     */
    if (len >= (int) sizeof(sun.sun_path)) {
	msg_warn("unix-domain name too long: %s", addr);
	lmtp_errno = LMTP_RETRY;
	return (0);
    }

    /*
     * Initialize.
     */
    memset((char *) &sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
#ifdef HAS_SUN_LEN
    sun.sun_len = len + 1;
#endif
    memcpy(sun.sun_path, addr, len + 1);

    /*
     * Create a client socket.
     */
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	msg_fatal("%s: socket: %m", myname);

    /*
     * Connect to the LMTP server.
     */
    if (msg_verbose)
	msg_info("%s: trying: %s...", myname, addr);

    return (lmtp_connect_sock(sock, (struct sockaddr *) & sun, sizeof(sun),
			      addr, addr, destination, why));
}

/* lmtp_connect_addr - connect to explicit address */

static LMTP_SESSION *lmtp_connect_addr(DNS_RR *addr, unsigned port,
			              const char *destination, VSTRING *why)
{
    char   *myname = "lmtp_connect_addr";
    struct sockaddr_storage ss;
    struct sockaddr *sa = SOCK_ADDR_PTR(&ss);
    SOCKADDR_SIZE salen = sizeof(ss);
    MAI_HOSTADDR_STR hostaddr;
    int     sock;

    /*
     * Sanity checks.
     */
    if (dns_rr_to_sa(addr, port, sa, &salen) != 0) {
	msg_warn("%s: skip address type %s: %m",
		 myname, dns_strtype(addr->type));
	lmtp_errno = LMTP_RETRY;
	return (0);
    }

    /*
     * Initialize.
     */
    if ((sock = socket(sa->sa_family, SOCK_STREAM, 0)) < 0)
	msg_fatal("%s: socket: %m", myname);

    /*
     * Connect to the LMTP server.
     */
    SOCKADDR_TO_HOSTADDR(sa, salen, &hostaddr, (MAI_SERVPORT_STR *) 0, 0);
    if (msg_verbose)
	msg_info("%s: trying: %s[%s] port %d...",
		 myname, addr->rname, hostaddr.buf, ntohs(port));

    return (lmtp_connect_sock(sock, sa, salen,
			      addr->rname, hostaddr.buf, destination, why));
}

/* lmtp_connect_sock - connect a socket over some transport */

static LMTP_SESSION *lmtp_connect_sock(int sock, struct sockaddr * sa, int len,
				         const char *name, const char *addr,
			              const char *destination, VSTRING *why)
{
    int     conn_stat;
    int     saved_errno;
    VSTREAM *stream;
    int     ch;

    if (var_lmtp_conn_tmout > 0) {
	non_blocking(sock, NON_BLOCKING);
	conn_stat = timed_connect(sock, sa, len, var_lmtp_conn_tmout);
	saved_errno = errno;
	non_blocking(sock, BLOCKING);
	errno = saved_errno;
    } else {
	conn_stat = sane_connect(sock, sa, len);
    }
    if (conn_stat < 0) {
	vstring_sprintf(why, "connect to %s[%s]: %m",
			name, addr);
	lmtp_errno = LMTP_RETRY;
	close(sock);
	return (0);
    }

    /*
     * Skip this host if it takes no action within some time limit.
     */
    if (read_wait(sock, var_lmtp_lhlo_tmout) < 0) {
	vstring_sprintf(why, "connect to %s[%s]: read timeout",
			name, addr);
	lmtp_errno = LMTP_RETRY;
	close(sock);
	return (0);
    }

    /*
     * Skip this host if it disconnects without talking to us.
     */
    stream = vstream_fdopen(sock, O_RDWR);
    if ((ch = VSTREAM_GETC(stream)) == VSTREAM_EOF) {
	vstring_sprintf(why, "connect to %s[%s]: server dropped connection without sending the initial greeting",
			name, addr);
	lmtp_errno = LMTP_RETRY;
	vstream_fclose(stream);
	return (0);
    }
    vstream_ungetc(stream, ch);

    /*
     * Skip this host if it sends a 4xx or 5xx greeting.
     */
    if (ch == '4' || ch == '5') {
	vstring_sprintf(why, "connect to %s[%s]: server refused mail service",
			name, addr);
	lmtp_errno = LMTP_RETRY;
	vstream_fclose(stream);
	return (0);
    }
    return (lmtp_session_alloc(stream, name, addr, destination));
}

/* lmtp_connect_host - direct connection to host */

static LMTP_SESSION *lmtp_connect_host(char *host, unsigned port,
			              const char *destination, VSTRING *why)
{
    LMTP_SESSION *session = 0;
    DNS_RR *addr_list;
    DNS_RR *addr;

    /*
     * Try each address in the specified order until we find one that works.
     * The addresses belong to the same A record, so we have no information
     * on what address is "best".
     */
    addr_list = lmtp_host_addr(host, why);
    for (addr = addr_list; addr; addr = addr->next) {
	if ((session = lmtp_connect_addr(addr, port, destination, why)) != 0) {
	    break;
	}
    }
    dns_rr_free(addr_list);
    return (session);
}

/* lmtp_parse_destination - parse destination */

static char *lmtp_parse_destination(const char *destination, char *def_service,
				            char **hostp, unsigned *portp)
{
    char   *myname = "lmtp_parse_destination";
    char   *buf = mystrdup(destination);
    char   *service;
    struct servent *sp;
    char   *protocol = "tcp";		/* XXX configurable? */
    unsigned port;
    const char *err;

    if (msg_verbose)
	msg_info("%s: %s %s", myname, destination, def_service);

    /*
     * Parse the host/port information. We're working with a copy of the
     * destination argument so the parsing can be destructive.
     */
    if ((err = host_port(buf, hostp, (char *) 0, &service, def_service)) != 0)
	msg_fatal("%s in LMTP server description: %s", err, destination);

    /*
     * Convert service to port number, network byte order. Since most folks
     * aren't going to have lmtp defined as a service, use a default value
     * instead of just blowing up.
     */
    if (alldig(service)) {
	if ((port = atoi(service)) >= 65536)
	    msg_fatal("bad numeric port in destination: %s", destination);
	*portp = htons(port);
    } else if ((sp = getservbyname(service, protocol)) != 0)
	*portp = sp->s_port;
    else
	*portp = htons(var_lmtp_tcp_port);
    return (buf);
}

/* lmtp_connect - establish LMTP connection */

LMTP_SESSION *lmtp_connect(const char *destination, VSTRING *why)
{
    char   *myname = "lmtp_connect";
    LMTP_SESSION *session;
    char   *dest_buf;
    char   *host;
    unsigned port;
    char   *def_service = "lmtp";	/* XXX configurable? */

    /*
     * Connect to the LMTP server.
     * 
     * XXX Ad-hoc transport parsing and connection management. Some or all
     * should be moved away to a reusable library routine so that every
     * program benefits from it.
     * 
     * XXX Should transform destination into canonical form (unix:/path or
     * inet:host:port before entering it into the connection cache. See also
     * the connection cache lookup code in lmtp.c.
     */
    if (strncmp(destination, "unix:", 5) == 0)
	return (lmtp_connect_unix(destination + 5, destination, why));
    if (strncmp(destination, "inet:", 5) == 0)
	dest_buf = lmtp_parse_destination(destination + 5, def_service,
					  &host, &port);
    else
	dest_buf = lmtp_parse_destination(destination, def_service,
					  &host, &port);
    if (msg_verbose)
	msg_info("%s: connecting to %s port %d", myname, host, ntohs(port));
    session = lmtp_connect_host(host, port, destination, why);
    myfree(dest_buf);
    return (session);
}
