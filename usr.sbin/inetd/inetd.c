/*	$NetBSD: inetd.c,v 1.38.2.1 1997/12/15 04:55:14 mellon Exp $	*/

/*
 * Copyright (c) 1983, 1991, 1993, 1994
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1983, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n");
#if 0
static char sccsid[] = "@(#)inetd.c	8.4 (Berkeley) 4/13/94";
#else
__RCSID("$NetBSD: inetd.c,v 1.38.2.1 1997/12/15 04:55:14 mellon Exp $");
#endif
#endif /* not lint */

/*
 * Inetd - Internet super-server
 *
 * This program invokes all internet services as needed.  Connection-oriented
 * services are invoked each time a connection is made, by creating a process.
 * This process is passed the connection as file descriptor 0 and is expected
 * to do a getpeername to find out the source host and port.
 *
 * Datagram oriented services are invoked when a datagram
 * arrives; a process is created and passed a pending message
 * on file descriptor 0.  Datagram servers may either connect
 * to their peer, freeing up the original socket for inetd
 * to receive further messages on, or ``take over the socket'',
 * processing all arriving datagrams and, eventually, timing
 * out.	 The first type of server is said to be ``multi-threaded'';
 * the second type of server ``single-threaded''.
 *
 * Inetd uses a configuration file which is read at startup
 * and, possibly, at some later time in response to a hangup signal.
 * The configuration file is ``free format'' with fields given in the
 * order shown below.  Continuation lines for an entry must being with
 * a space or tab.  All fields must be present in each entry.
 *
 *	service name			must be in /etc/services or must
 *					name a tcpmux service
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols
 *	wait/nowait[.max]		single-threaded/multi-threaded, max #
 *	user[.group]			user/group to run daemon as
 *	server program			full path name
 *	server program arguments	maximum of MAXARGS (20)
 *
 * For RPC services
 *      service name/version            must be in /etc/rpc
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols
 *	wait/nowait[.max]		single-threaded/multi-threaded
 *	user[.group]			user to run daemon as
 *	server program			full path name
 *	server program arguments	maximum of MAXARGS (20)
 *
 * For non-RPC services, the "service name" can be of the form
 * hostaddress:servicename, in which case the hostaddress is used
 * as the host portion of the address to listen on.  If hostaddress
 * consists of a single `*' character, INADDR_ANY is used.
 *
 * A line can also consist of just
 *	hostaddress:
 * where hostaddress is as in the preceding paragraph.  Such a line must
 * have no further fields; the specified hostaddress is remembered and
 * used for all further lines that have no hostaddress specified,
 * until the next such line (or EOF).  (This is why * is provided to
 * allow explicit specification of INADDR_ANY.)  A line
 *	*:
 * is implicitly in effect at the beginning of the file.
 *
 * The hostaddress specifier may (and often will) contain dots;
 * the service name must not.
 *
 * For RPC services, host-address specifiers are accepted and will
 * work to some extent; however, because of limitations in the
 * portmapper interface, it will not work to try to give more than
 * one line for any given RPC service, even if the host-address
 * specifiers are different.
 *
 * TCP services without official port numbers are handled with the
 * RFC1078-based tcpmux internal service. Tcpmux listens on port 1 for
 * requests. When a connection is made from a foreign host, the service
 * requested is passed to tcpmux, which looks it up in the servtab list
 * and returns the proper entry for the service. Tcpmux returns a
 * negative reply if the service doesn't exist, otherwise the invoked
 * server is expected to return the positive reply if the service type in
 * inetd.conf file has the prefix "tcpmux/". If the service type has the
 * prefix "tcpmux/+", tcpmux will return the positive reply for the
 * process; this is for compatibility with older server code, and also
 * allows you to invoke programs that use stdin/stdout without putting any
 * special server code in them. Services that use tcpmux are "nowait"
 * because they do not have a well-known port and hence cannot listen
 * for new requests.
 *
 * Comment lines are indicated by a `#' in column 1.
 */

/*
 * Here's the scoop concerning the user.group feature:
 *
 * 1) set-group-option off.
 *
 * 	a) user = root:	NO setuid() or setgid() is done
 *
 * 	b) other:	setuid()
 * 			setgid(primary group as found in passwd)
 * 			initgroups(name, primary group)
 *
 * 2) set-group-option on.
 *
 * 	a) user = root:	NO setuid()
 * 			setgid(specified group)
 * 			NO initgroups()
 *
 * 	b) other:	setuid()
 * 			setgid(specified group)
 * 			initgroups(name, specified group)
 *
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifndef RLIMIT_NOFILE
#define RLIMIT_NOFILE	RLIMIT_OFILE
#endif

#define RPC

#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef RPC
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "pathnames.h"

#ifdef LIBWRAP
# include <tcpd.h>
#ifndef LIBWRAP_ALLOW_FACILITY
# define LIBWRAP_ALLOW_FACILITY LOG_AUTH
#endif
#ifndef LIBWRAP_ALLOW_SEVERITY
# define LIBWRAP_ALLOW_SEVERITY LOG_INFO
#endif
#ifndef LIBWRAP_DENY_FACILITY
# define LIBWRAP_DENY_FACILITY LOG_AUTH
#endif
#ifndef LIBWRAP_DENY_SEVERITY
# define LIBWRAP_DENY_SEVERITY LOG_WARNING
#endif
int allow_severity = LIBWRAP_ALLOW_FACILITY|LIBWRAP_ALLOW_SEVERITY;
int deny_severity = LIBWRAP_DENY_FACILITY|LIBWRAP_DENY_SEVERITY;
#endif

#define	TOOMANY		40		/* don't start more than TOOMANY */
#define	CNT_INTVL	60		/* servers in CNT_INTVL sec. */
#define	RETRYTIME	(60*10)		/* retry after bind or server fail */

#define	SIGBLOCK	(sigmask(SIGCHLD)|sigmask(SIGHUP)|sigmask(SIGALRM))

int	debug;
#ifdef LIBWRAP
int	lflag;
#endif
int	nsock, maxsock;
fd_set	allsock;
int	options;
int	timingout;
struct	servent *sp;
char	*curdom;

#ifndef OPEN_MAX
#define OPEN_MAX	64
#endif

/* Reserve some descriptors, 3 stdio + at least: 1 log, 1 conf. file */
#define FD_MARGIN	(8)
typeof(((struct rlimit *)0)->rlim_cur)	rlim_ofile_cur = OPEN_MAX;

#ifdef RLIMIT_NOFILE
struct rlimit	rlim_ofile;
#endif

struct	servtab {
	char	*se_hostaddr;		/* host address to listen on */
	char	*se_service;		/* name of service */
	int	se_socktype;		/* type of socket to use */
	int	se_family;		/* address family */
	char	*se_proto;		/* protocol used */
	int	se_rpcprog;		/* rpc program number */
	int	se_rpcversl;		/* rpc program lowest version */
	int	se_rpcversh;		/* rpc program highest version */
#define isrpcservice(sep)	((sep)->se_rpcversl != 0)
	short	se_wait;		/* single threaded server */
	short	se_checked;		/* looked at during merge */
	char	*se_user;		/* user name to run as */
	char	*se_group;		/* group name to run as */
	struct	biltin *se_bi;		/* if built-in, description */
	char	*se_server;		/* server program */
#define	MAXARGV 20
	char	*se_argv[MAXARGV+1];	/* program arguments */
	int	se_fd;			/* open descriptor */
	int	se_type;		/* type */
	union {
		struct	sockaddr se_un_ctrladdr;
		struct	sockaddr_in se_un_ctrladdr_in;
		struct	sockaddr_un se_un_ctrladdr_un;
	} se_un;			/* bound address */
#define se_ctrladdr	se_un.se_un_ctrladdr
#define se_ctrladdr_in	se_un.se_un_ctrladdr_in
#define se_ctrladdr_un	se_un.se_un_ctrladdr_un
	int	se_ctrladdr_size;
	int	se_max;			/* max # of instances of this service */
	int	se_count;		/* number started since se_time */
	struct	timeval se_time;	/* start of se_count */
#ifdef MULOG
	int	se_log;
#define MULOG_RFC931	0x40000000
#endif
	struct	servtab *se_next;
} *servtab;

#define NORM_TYPE	0
#define MUX_TYPE	1
#define MUXPLUS_TYPE	2
#define ISMUX(sep)	(((sep)->se_type == MUX_TYPE) || \
			 ((sep)->se_type == MUXPLUS_TYPE))
#define ISMUXPLUS(sep)	((sep)->se_type == MUXPLUS_TYPE)


void		chargen_dg __P((int, struct servtab *));
void		chargen_stream __P((int, struct servtab *));
void		close_sep __P((struct servtab *));
void		config __P((int));
void		daytime_dg __P((int, struct servtab *));
void		daytime_stream __P((int, struct servtab *));
void		discard_dg __P((int, struct servtab *));
void		discard_stream __P((int, struct servtab *));
void		echo_dg __P((int, struct servtab *));
void		echo_stream __P((int, struct servtab *));
void		endconfig __P((void));
struct servtab *enter __P((struct servtab *));
void		freeconfig __P((struct servtab *));
struct servtab *getconfigent __P((void));
void		goaway __P((int));
void		machtime_dg __P((int, struct servtab *));
void		machtime_stream __P((int, struct servtab *));
char	       *newstr __P((char *));
char	       *nextline __P((FILE *));
void		print_service __P((char *, struct servtab *));
void		reapchild __P((int));
void		retry __P((int));
void		run_service __P((int, struct servtab *));
int		setconfig __P((void));
void		setup __P((struct servtab *));
char	       *sskip __P((char **));
char	       *skip __P((char **));
void		tcpmux __P((int, struct servtab *));
void		usage __P((void));
void		logpid __P((void));
void		register_rpc __P((struct servtab *sep));
void		unregister_rpc __P((struct servtab *sep));
void		bump_nofile __P((void));
void		inetd_setproctitle __P((char *, int));
void		initring __P((void));
long		machtime __P((void));
static int	getline __P((int, char *, int));
int		main __P((int, char *[], char *[]));

struct biltin {
	char	*bi_service;		/* internally provided service name */
	int	bi_socktype;		/* type of socket supported */
	short	bi_fork;		/* 1 if should fork before call */
	short	bi_wait;		/* 1 if should wait for child */
	void	(*bi_fn) __P((int, struct servtab *));
					/* function which performs it */
} biltins[] = {
	/* Echo received data */
	{ "echo",	SOCK_STREAM,	1, 0,	echo_stream },
	{ "echo",	SOCK_DGRAM,	0, 0,	echo_dg },

	/* Internet /dev/null */
	{ "discard",	SOCK_STREAM,	1, 0,	discard_stream },
	{ "discard",	SOCK_DGRAM,	0, 0,	discard_dg },

	/* Return 32 bit time since 1970 */
	{ "time",	SOCK_STREAM,	0, 0,	machtime_stream },
	{ "time",	SOCK_DGRAM,	0, 0,	machtime_dg },

	/* Return human-readable time */
	{ "daytime",	SOCK_STREAM,	0, 0,	daytime_stream },
	{ "daytime",	SOCK_DGRAM,	0, 0,	daytime_dg },

	/* Familiar character generator */
	{ "chargen",	SOCK_STREAM,	1, 0,	chargen_stream },
	{ "chargen",	SOCK_DGRAM,	0, 0,	chargen_dg },

	{ "tcpmux",	SOCK_STREAM,	1, 0,	tcpmux },

	{ NULL }
};

#define NUMINT	(sizeof(intab) / sizeof(struct inent))
char	*CONFIG = _PATH_INETDCONF;
char	**Argv;
char 	*LastArg;
extern char	*__progname;

#ifdef sun
/*
 * Sun's RPC library caches the result of `dtablesize()'
 * This is incompatible with our "bumping" of file descriptors "on demand"
 */
int
_rpc_dtablesize()
{
	return rlim_ofile_cur;
}
#endif

int
main(argc, argv, envp)
	int argc;
	char *argv[], *envp[];
{
	struct servtab *sep, *nsep;
	struct sigvec sv;
	int ch, dofork;
	pid_t pid;

	Argv = argv;
	if (envp == 0 || *envp == 0)
		envp = argv;
	while (*envp)
		envp++;
	LastArg = envp[-1] + strlen(envp[-1]);

	while ((ch = getopt(argc, argv,
#ifdef LIBWRAP
					"dl"
#else
					"d"
#endif
					   )) != -1)
		switch(ch) {
		case 'd':
			debug = 1;
			options |= SO_DEBUG;
			break;
#ifdef LIBWRAP
		case 'l':
			lflag = 1;
			break;
#endif
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		CONFIG = argv[0];

	if (debug == 0)
		daemon(0, 0);
	openlog(__progname, LOG_PID | LOG_NOWAIT, LOG_DAEMON);
	logpid();

#ifdef RLIMIT_NOFILE
	if (getrlimit(RLIMIT_NOFILE, &rlim_ofile) < 0) {
		syslog(LOG_ERR, "getrlimit: %m");
	} else {
		rlim_ofile_cur = rlim_ofile.rlim_cur;
		if (rlim_ofile_cur == RLIM_INFINITY)	/* ! */
			rlim_ofile_cur = OPEN_MAX;
	}
#endif

	memset(&sv, 0, sizeof(sv));
	sv.sv_mask = SIGBLOCK;
	sv.sv_handler = retry;
	sigvec(SIGALRM, &sv, (struct sigvec *)0);
	config(SIGHUP);
	sv.sv_handler = config;
	sigvec(SIGHUP, &sv, (struct sigvec *)0);
	sv.sv_handler = reapchild;
	sigvec(SIGCHLD, &sv, (struct sigvec *)0);
	sv.sv_handler = goaway;
	sigvec(SIGTERM, &sv, (struct sigvec *)0);
	sv.sv_handler = goaway;
	sigvec(SIGINT, &sv, (struct sigvec *)0);
	sv.sv_handler = SIG_IGN;
	sigvec(SIGPIPE, &sv, (struct sigvec *)0);

	{
		/* space for daemons to overwrite environment for ps */
#define	DUMMYSIZE	100
		char dummy[DUMMYSIZE];

		(void)memset(dummy, 'x', DUMMYSIZE - 1);
		dummy[DUMMYSIZE - 1] = '\0';

		(void)setenv("inetd_dummy", dummy, 1);
	}

	for (;;) {
	    int n, ctrl;
	    fd_set readable;

	    if (nsock == 0) {
		(void) sigblock(SIGBLOCK);
		while (nsock == 0)
		    sigpause(0L);
		(void) sigsetmask(0L);
	    }
	    readable = allsock;
	    if ((n = select(maxsock + 1, &readable, (fd_set *)0,
		(fd_set *)0, (struct timeval *)0)) <= 0) {
		    if (n == -1 && errno != EINTR) {
			syslog(LOG_WARNING, "select: %m");
			sleep(1);
		    }
		    continue;
	    }
	    for (sep = servtab; n && sep; sep = nsep) {
	    nsep = sep->se_next;
	    if (sep->se_fd != -1 && FD_ISSET(sep->se_fd, &readable)) {
		n--;
		if (debug)
			fprintf(stderr, "someone wants %s\n", sep->se_service);
		if (!sep->se_wait && sep->se_socktype == SOCK_STREAM) {
			/* XXX here do the libwrap check-before-accept */
			ctrl = accept(sep->se_fd, (struct sockaddr *)0,
			    (int *)0);
			if (debug)
				fprintf(stderr, "accept, ctrl %d\n", ctrl);
			if (ctrl < 0) {
				if (errno != EINTR)
					syslog(LOG_WARNING,
					    "accept (for %s): %m",
					    sep->se_service);
				continue;
			}
		} else
			ctrl = sep->se_fd;
		(void) sigblock(SIGBLOCK);
		pid = 0;
#ifdef LIBWRAP_INTERNAL
		dofork = 1;
#else
		dofork = (sep->se_bi == 0 || sep->se_bi->bi_fork);
#endif
		if (dofork) {
			if (sep->se_count++ == 0)
			    (void)gettimeofday(&sep->se_time,
			        (struct timezone *)0);
			else if (sep->se_count >= sep->se_max) {
				struct timeval now;

				(void)gettimeofday(&now, (struct timezone *)0);
				if (now.tv_sec - sep->se_time.tv_sec >
				    CNT_INTVL) {
					sep->se_time = now;
					sep->se_count = 1;
				} else {
					syslog(LOG_ERR,
			"%s/%s server failing (looping), service terminated\n",
					    sep->se_service, sep->se_proto);
					close_sep(sep);
					sigsetmask(0L);
					if (!timingout) {
						timingout = 1;
						alarm(RETRYTIME);
					}
					continue;
				}
			}
			pid = fork();
			if (pid < 0) {
				syslog(LOG_ERR, "fork: %m");
				if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
					close(ctrl);
				sigsetmask(0L);
				sleep(1);
				continue;
			}
			if (pid != 0 && sep->se_wait) {
				sep->se_wait = pid;
				FD_CLR(sep->se_fd, &allsock);
				nsock--;
			}
			if (pid == 0 && debug)
				setsid();
		}
		sigsetmask(0L);
		if (pid == 0) {
			run_service(ctrl, sep);
			if (dofork)
				exit(0);
		}
		if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
			close(ctrl);
	    }
	    }
	}
}

void
run_service(ctrl, sep)
	int ctrl;
	struct servtab *sep;
{
	struct passwd *pwd;
	struct group *grp = NULL;	/* XXX gcc */
#ifdef LIBWRAP
	struct request_info req;
	int denied;
	char buf[7], *service = NULL;	/* XXX gcc */
#endif

#ifdef LIBWRAP
#ifndef LIBWRAP_INTERNAL
	if (sep->se_bi == 0)
#endif
	if (!sep->se_wait && sep->se_socktype == SOCK_STREAM) {
		request_init(&req, RQ_DAEMON, sep->se_argv[0] ?
		    sep->se_argv[0] : sep->se_service, RQ_FILE, ctrl, NULL);
		fromhost(&req);
		denied = !hosts_access(&req);
		if (denied || lflag) {
			sp = getservbyport(sep->se_ctrladdr_in.sin_port,
			    sep->se_proto);
			if (sp == NULL) {
				(void)snprintf(buf, sizeof buf, "%d",
				    ntohs(sep->se_ctrladdr_in.sin_port));
				service = buf;
			} else
				service = sp->s_name;
		}
		if (denied) {
			syslog(deny_severity,
			    "refused connection from %.500s, service %s (%s)",
			    eval_client(&req), service, sep->se_proto);
			goto reject;
		}
		if (lflag) {
			syslog(allow_severity,
			    "connection from %.500s, service %s (%s)",
			    eval_client(&req), service, sep->se_proto);
		}
	}
#endif /* LIBWRAP */

	if (sep->se_bi) {
		(*sep->se_bi->bi_fn)(ctrl, sep);
	} else {
		if ((pwd = getpwnam(sep->se_user)) == NULL) {
			syslog(LOG_ERR, "%s/%s: %s: No such user",
			    sep->se_service, sep->se_proto, sep->se_user);
			goto reject;
		}
		if (sep->se_group &&
		    (grp = getgrnam(sep->se_group)) == NULL) {
			syslog(LOG_ERR, "%s/%s: %s: No such group",
			    sep->se_service, sep->se_proto, sep->se_group);
			goto reject;
		}
		if (pwd->pw_uid) {
			if (sep->se_group)
				pwd->pw_gid = grp->gr_gid;
			if (setgid(pwd->pw_gid) < 0) {
				syslog(LOG_ERR,
				 "%s/%s: can't set gid %d: %m", sep->se_service,
				    sep->se_proto, pwd->pw_gid);
				goto reject;
			}
			(void) initgroups(pwd->pw_name,
			    pwd->pw_gid);
			if (setuid(pwd->pw_uid) < 0) {
				syslog(LOG_ERR,
				 "%s/%s: can't set uid %d: %m", sep->se_service,
				    sep->se_proto, pwd->pw_uid);
				goto reject;
			}
		} else if (sep->se_group) {
			(void) setgid((gid_t)grp->gr_gid);
		}
		if (debug)
			fprintf(stderr, "%d execl %s\n",
			    getpid(), sep->se_server);
#ifdef MULOG
		if (sep->se_log)
			dolog(sep, ctrl);
#endif
		/* Set our control descriptor to not close-on-exec... */
		if (fcntl(ctrl, F_SETFD, 0) < 0)
			syslog(LOG_ERR, "fcntl (F_SETFD, 0): %m");
		/* ...and dup it to stdin, stdout, and stderr. */
		if (ctrl != 0) {
			dup2(ctrl, 0);
			close(ctrl);
			ctrl = 0;
		}
		dup2(0, 1);
		dup2(0, 2);
#ifdef RLIMIT_NOFILE
		if (rlim_ofile.rlim_cur != rlim_ofile_cur &&
		    setrlimit(RLIMIT_NOFILE, &rlim_ofile) < 0)
			syslog(LOG_ERR, "setrlimit: %m");
#endif
		execv(sep->se_server, sep->se_argv);
		syslog(LOG_ERR, "cannot execute %s: %m", sep->se_server);
	reject:
		if (sep->se_socktype != SOCK_STREAM)
			recv(ctrl, buf, sizeof (buf), 0);
		_exit(1);
	}
}

void
reapchild(signo)
	int signo;
{
	int status;
	pid_t pid;
	struct servtab *sep;

	for (;;) {
		pid = wait3(&status, WNOHANG, (struct rusage *)0);
		if (pid <= 0)
			break;
		if (debug)
			fprintf(stderr, "%d reaped, status %#x\n", 
			    pid, status);
		for (sep = servtab; sep; sep = sep->se_next)
			if (sep->se_wait == pid) {
				if (WIFEXITED(status) && WEXITSTATUS(status))
					syslog(LOG_WARNING,
					    "%s: exit status 0x%x",
					    sep->se_server, WEXITSTATUS(status));
				else if (WIFSIGNALED(status))
					syslog(LOG_WARNING,
					    "%s: exit signal 0x%x",
					    sep->se_server, WTERMSIG(status));
				sep->se_wait = 1;
				FD_SET(sep->se_fd, &allsock);
				nsock++;
				if (debug)
					fprintf(stderr, "restored %s, fd %d\n",
					    sep->se_service, sep->se_fd);
			}
	}
}

void
config(signo)
	int signo;
{
	struct servtab *sep, *cp, **sepp;
	long omask;
	int n;

	if (!setconfig()) {
		syslog(LOG_ERR, "%s: %m", CONFIG);
		return;
	}
	for (sep = servtab; sep; sep = sep->se_next)
		sep->se_checked = 0;
	while ((cp = getconfigent())) {
		for (sep = servtab; sep; sep = sep->se_next)
			if (strcmp(sep->se_service, cp->se_service) == 0 &&
			    strcmp(sep->se_hostaddr, cp->se_hostaddr) == 0 &&
			    strcmp(sep->se_proto, cp->se_proto) == 0 &&
			    ISMUX(sep) == ISMUX(cp))
				break;
		if (sep != 0) {
			int i;

#define SWAP(type, a, b) {type c=(type)a; (type)a=(type)b; (type)b=(type)c;}

			omask = sigblock(SIGBLOCK);
			/*
			 * sep->se_wait may be holding the pid of a daemon
			 * that we're waiting for.  If so, don't overwrite
			 * it unless the config file explicitly says don't
			 * wait.
			 */
			if (cp->se_bi == 0 &&
			    (sep->se_wait == 1 || cp->se_wait == 0))
				sep->se_wait = cp->se_wait;
			SWAP(char *, sep->se_user, cp->se_user);
			SWAP(char *, sep->se_group, cp->se_group);
			SWAP(char *, sep->se_server, cp->se_server);
			for (i = 0; i < MAXARGV; i++)
				SWAP(char *, sep->se_argv[i], cp->se_argv[i]);
			SWAP(int, cp->se_type, sep->se_type);
			SWAP(int, cp->se_max, sep->se_max);
#undef SWAP
			if (isrpcservice(sep))
				unregister_rpc(sep);
			sep->se_rpcversl = cp->se_rpcversl;
			sep->se_rpcversh = cp->se_rpcversh;
			sigsetmask(omask);
			freeconfig(cp);
			if (debug)
				print_service("REDO", sep);
		} else {
			sep = enter(cp);
			if (debug)
				print_service("ADD ", sep);
		}
		sep->se_checked = 1;

		switch (sep->se_family) {
		case AF_UNIX:
			if (sep->se_fd != -1)
				break;
			n = strlen(sep->se_service);
			if (n > sizeof(sep->se_ctrladdr_un.sun_path)) {
				syslog(LOG_ERR, "%s: address too long",
				    sep->se_service);
				sep->se_checked = 0;
				continue;
			}
			(void)unlink(sep->se_service);
			strncpy(sep->se_ctrladdr_un.sun_path,
			    sep->se_service, n);
			sep->se_ctrladdr_un.sun_family = AF_UNIX;
			sep->se_ctrladdr_size = n +
			    sizeof(sep->se_ctrladdr_un) -
			    sizeof(sep->se_ctrladdr_un.sun_path);
			if (!ISMUX(sep))
				setup(sep);
			break;
		case AF_INET:
			sep->se_ctrladdr_in.sin_family = AF_INET;
			if (!strcmp(sep->se_hostaddr,"*"))
				sep->se_ctrladdr_in.sin_addr.s_addr =
				    INADDR_ANY;
			else if (!inet_aton(sep->se_hostaddr,
			    &sep->se_ctrladdr_in.sin_addr)) {
				/* Do we really want to support hostname lookups here? */
				struct hostent *hp;
				hp = gethostbyname(sep->se_hostaddr);
				if (hp == 0) {
					syslog(LOG_ERR, "%s: unknown host",
					    sep->se_hostaddr);
					sep->se_checked = 0;
					continue;
				} else if (hp->h_addrtype != AF_INET) {
					syslog(LOG_ERR,
				       "%s: address isn't an Internet address",
					    sep->se_hostaddr);
					sep->se_checked = 0;
					continue;
				} else if (hp->h_length != sizeof(struct in_addr)) {
					syslog(LOG_ERR,
		       "%s: address size wrong (under DNS corruption attack?)",
					    sep->se_hostaddr);
					sep->se_checked = 0;
					continue;
				} else {
					memcpy(&sep->se_ctrladdr_in.sin_addr,
					    hp->h_addr_list[0],
					    sizeof(struct in_addr));
				}
			}
			if (ISMUX(sep)) {
				sep->se_fd = -1;
				continue;
			}
			sep->se_ctrladdr_size = sizeof(sep->se_ctrladdr_in);
			if (isrpcservice(sep)) {
				struct rpcent *rp;

				sep->se_rpcprog = atoi(sep->se_service);
				if (sep->se_rpcprog == 0) {
					rp = getrpcbyname(sep->se_service);
					if (rp == 0) {
						syslog(LOG_ERR,
						    "%s/%s: unknown service",
						    sep->se_service,
						    sep->se_proto);
						sep->se_checked = 0;
						continue;
					}
					sep->se_rpcprog = rp->r_number;
				}
				if (sep->se_fd == -1 && !ISMUX(sep))
					setup(sep);
				if (sep->se_fd != -1)
					register_rpc(sep);
			} else {
				u_short port = htons(atoi(sep->se_service));

				if (!port) {
					sp = getservbyname(sep->se_service,
					    sep->se_proto);
					if (sp == 0) {
						syslog(LOG_ERR,
						    "%s/%s: unknown service",
						    sep->se_service,
						    sep->se_proto);
						sep->se_checked = 0;
						continue;
					}
					port = sp->s_port;
				}
				if (port != sep->se_ctrladdr_in.sin_port) {
					sep->se_ctrladdr_in.sin_port = port;
					if (sep->se_fd >= 0)
						close_sep(sep);
				}
				if (sep->se_fd == -1 && !ISMUX(sep))
					setup(sep);
			}
		}
	}
	endconfig();
	/*
	 * Purge anything not looked at above.
	 */
	omask = sigblock(SIGBLOCK);
	sepp = &servtab;
	while ((sep = *sepp)) {
		if (sep->se_checked) {
			sepp = &sep->se_next;
			continue;
		}
		*sepp = sep->se_next;
		if (sep->se_fd >= 0)
			close_sep(sep);
		if (isrpcservice(sep))
			unregister_rpc(sep);
		if (sep->se_family == AF_UNIX)
			(void)unlink(sep->se_service);
		if (debug)
			print_service("FREE", sep);
		freeconfig(sep);
		free((char *)sep);
	}
	(void) sigsetmask(omask);
}

void
retry(signo)
	int signo;
{
	struct servtab *sep;

	timingout = 0;
	for (sep = servtab; sep; sep = sep->se_next) {
		if (sep->se_fd == -1 && !ISMUX(sep)) {
			switch (sep->se_family) {
			case AF_UNIX:
			case AF_INET:
				setup(sep);
				if (sep->se_fd != -1 && isrpcservice(sep))
					register_rpc(sep);
				break;
			}
		}
	}
}

void
goaway(signo)
	int signo;
{
	struct servtab *sep;

	for (sep = servtab; sep; sep = sep->se_next) {
		if (sep->se_fd == -1)
			continue;

		switch (sep->se_family) {
		case AF_UNIX:
			(void)unlink(sep->se_service);
			break;
		case AF_INET:
			if (sep->se_wait == 1 && isrpcservice(sep))
				unregister_rpc(sep);
			break;
		}
		(void)close(sep->se_fd);
	}
	(void)unlink(_PATH_INETDPID);
	exit(0);
}

void
setup(sep)
	struct servtab *sep;
{
	int on = 1;

	if ((sep->se_fd = socket(sep->se_family, sep->se_socktype, 0)) < 0) {
		if (debug)
			fprintf(stderr, "socket failed on %s/%s: %s\n", 
			    sep->se_service, sep->se_proto, strerror(errno));
		syslog(LOG_ERR, "%s/%s: socket: %m",
		    sep->se_service, sep->se_proto);
		return;
	}
	/* Set all listening sockets to close-on-exec. */
	if (fcntl(sep->se_fd, F_SETFD, FD_CLOEXEC) < 0)
		syslog(LOG_ERR, "fcntl (F_SETFD, FD_CLOEXEC): %m");
#define	turnon(fd, opt) \
setsockopt(fd, SOL_SOCKET, opt, (char *)&on, sizeof (on))
	if (strcmp(sep->se_proto, "tcp") == 0 && (options & SO_DEBUG) &&
	    turnon(sep->se_fd, SO_DEBUG) < 0)
		syslog(LOG_ERR, "setsockopt (SO_DEBUG): %m");
	if (turnon(sep->se_fd, SO_REUSEADDR) < 0)
		syslog(LOG_ERR, "setsockopt (SO_REUSEADDR): %m");
#undef turnon
	if (bind(sep->se_fd, &sep->se_ctrladdr, sep->se_ctrladdr_size) < 0) {
		if (debug)
			fprintf(stderr, "bind failed on %s/%s: %s\n",
			    sep->se_service, sep->se_proto, strerror(errno));
		syslog(LOG_ERR, "%s/%s: bind: %m",
		    sep->se_service, sep->se_proto);
		(void) close(sep->se_fd);
		sep->se_fd = -1;
		if (!timingout) {
			timingout = 1;
			alarm(RETRYTIME);
		}
		return;
	}
	if (sep->se_socktype == SOCK_STREAM)
		listen(sep->se_fd, 10);

	FD_SET(sep->se_fd, &allsock);
	nsock++;
	if (sep->se_fd > maxsock) {
		maxsock = sep->se_fd;
		if (maxsock > rlim_ofile_cur - FD_MARGIN)
			bump_nofile();
	}
	if (debug)
		fprintf(stderr, "registered %s on %d\n",
		    sep->se_server, sep->se_fd);
}

/*
 * Finish with a service and its socket.
 */
void
close_sep(sep)
	struct servtab *sep;
{
	if (sep->se_fd >= 0) {
		nsock--;
		FD_CLR(sep->se_fd, &allsock);
		(void) close(sep->se_fd);
		sep->se_fd = -1;
	}
	sep->se_count = 0;
	/*
	 * Don't keep the pid of this running deamon: when reapchild()
	 * reaps this pid, it would erroneously increment nsock.
	 */
	if (sep->se_wait > 1)
		sep->se_wait = 1;
}

void
register_rpc(sep)
	struct servtab *sep;
{
#ifdef RPC
	int n;
	struct sockaddr_in sin;
	struct protoent *pp;

	if ((pp = getprotobyname(sep->se_proto+4)) == NULL) {
		syslog(LOG_ERR, "%s: getproto: %m",
		    sep->se_proto);
		return;
	}
	n = sizeof sin;
	if (getsockname(sep->se_fd, (struct sockaddr *)&sin, &n) < 0) {
		syslog(LOG_ERR, "%s/%s: getsockname: %m",
		    sep->se_service, sep->se_proto);
		return;
	}

	for (n = sep->se_rpcversl; n <= sep->se_rpcversh; n++) {
		if (debug)
			fprintf(stderr, "pmap_set: %u %u %u %u\n",
			    sep->se_rpcprog, n, pp->p_proto,
			    ntohs(sin.sin_port));
		(void)pmap_unset(sep->se_rpcprog, n);
		if (!pmap_set(sep->se_rpcprog, n, pp->p_proto, ntohs(sin.sin_port)))
			syslog(LOG_ERR, "pmap_set: %u %u %u %u: %m",
			    sep->se_rpcprog, n, pp->p_proto,
			    ntohs(sin.sin_port));
	}
#endif /* RPC */
}

void
unregister_rpc(sep)
	struct servtab *sep;
{
#ifdef RPC
	int n;

	for (n = sep->se_rpcversl; n <= sep->se_rpcversh; n++) {
		if (debug)
			fprintf(stderr, "pmap_unset(%u, %u)\n",
			    sep->se_rpcprog, n);
		if (!pmap_unset(sep->se_rpcprog, n))
			syslog(LOG_ERR, "pmap_unset(%u, %u)\n",
			    sep->se_rpcprog, n);
	}
#endif /* RPC */
}


struct servtab *
enter(cp)
	struct servtab *cp;
{
	struct servtab *sep;
	long omask;

	sep = (struct servtab *)malloc(sizeof (*sep));
	if (sep == (struct servtab *)0) {
		syslog(LOG_ERR, "Out of memory.");
		exit(-1);
	}
	*sep = *cp;
	sep->se_fd = -1;
	sep->se_rpcprog = -1;
	omask = sigblock(SIGBLOCK);
	sep->se_next = servtab;
	servtab = sep;
	sigsetmask(omask);
	return (sep);
}

FILE	*fconfig = NULL;
struct	servtab serv;
char	line[LINE_MAX];
char    *defhost;

int
setconfig()
{
	if (defhost) free(defhost);
	defhost = newstr("*");
	if (fconfig != NULL) {
		fseek(fconfig, 0L, SEEK_SET);
		return (1);
	}
	fconfig = fopen(CONFIG, "r");
	return (fconfig != NULL);
}

void
endconfig()
{
	if (fconfig) {
		(void) fclose(fconfig);
		fconfig = NULL;
	}
	if (defhost) {
		free(defhost);
		defhost = 0;
	}
}

struct servtab *
getconfigent()
{
	struct servtab *sep = &serv;
	int argc;
	char *cp, *arg;
	static char TCPMUX_TOKEN[] = "tcpmux/";
#define MUX_LEN		(sizeof(TCPMUX_TOKEN)-1)
	char *hostdelim;

more:
#ifdef MULOG
	while ((cp = nextline(fconfig)) && (*cp == '#' || *cp == '\0')) {
		/* Avoid use of `skip' if there is a danger of it looking
		 * at continuation lines.
		 */
		do {
			cp++;
		} while (*cp == ' ' || *cp == '\t');
		if (*cp == '\0')
			continue;
		if ((arg = skip(&cp)) == NULL)
			continue;
		if (strcmp(arg, "DOMAIN"))
			continue;
		if (curdom)
			free(curdom);
		curdom = NULL;
		while (*cp == ' ' || *cp == '\t')
			cp++;
		if (*cp == '\0')
			continue;
		arg = cp;
		while (*cp && *cp != ' ' && *cp != '\t')
			cp++;
		if (*cp != '\0')
			*cp++ = '\0';
		curdom = newstr(arg);
	}
#else
	while ((cp = nextline(fconfig)) && (*cp == '#' || *cp == '\0'))
		;
#endif
	if (cp == NULL)
		return ((struct servtab *)0);
	/*
	 * clear the static buffer, since some fields (se_ctrladdr,
	 * for example) don't get initialized here.
	 */
	memset((caddr_t)sep, 0, sizeof *sep);
	arg = skip(&cp);
	if (cp == NULL) {
		/* got an empty line containing just blanks/tabs. */
		goto more;
	}
	/* Check for a host name. */
	hostdelim = strrchr(arg, ':');
	if (hostdelim) {
		*hostdelim = '\0';
		sep->se_hostaddr = newstr(arg);
		arg = hostdelim + 1;
		/*
		 * If the line is of the form `host:', then just change the
		 * default host for the following lines.
		 */
		if (*arg == '\0') {
			arg = skip(&cp);
			if (cp == NULL) {
				free(defhost);
				defhost = sep->se_hostaddr;
				goto more;
			}
		}
	} else
		sep->se_hostaddr = newstr(defhost);
	if (strncmp(arg, TCPMUX_TOKEN, MUX_LEN) == 0) {
		char *c = arg + MUX_LEN;
		if (*c == '+') {
			sep->se_type = MUXPLUS_TYPE;
			c++;
		} else
			sep->se_type = MUX_TYPE;
		sep->se_service = newstr(c);
	} else {
		sep->se_service = newstr(arg);
		sep->se_type = NORM_TYPE;
	}

	arg = sskip(&cp);
	if (strcmp(arg, "stream") == 0)
		sep->se_socktype = SOCK_STREAM;
	else if (strcmp(arg, "dgram") == 0)
		sep->se_socktype = SOCK_DGRAM;
	else if (strcmp(arg, "rdm") == 0)
		sep->se_socktype = SOCK_RDM;
	else if (strcmp(arg, "seqpacket") == 0)
		sep->se_socktype = SOCK_SEQPACKET;
	else if (strcmp(arg, "raw") == 0)
		sep->se_socktype = SOCK_RAW;
	else
		sep->se_socktype = -1;

	sep->se_proto = newstr(sskip(&cp));
	if (strcmp(sep->se_proto, "unix") == 0) {
		sep->se_family = AF_UNIX;
	} else {
		sep->se_family = AF_INET;
		if (strncmp(sep->se_proto, "rpc/", 4) == 0) {
#ifdef RPC
			char *cp, *ccp;
			cp = strchr(sep->se_service, '/');
			if (cp == 0) {
				syslog(LOG_ERR, "%s: no rpc version",
				    sep->se_service);
				goto more;
			}
			*cp++ = '\0';
			sep->se_rpcversl = sep->se_rpcversh =
			    strtol(cp, &ccp, 0);
			if (ccp == cp) {
		badafterall:
				syslog(LOG_ERR, "%s/%s: bad rpc version",
				    sep->se_service, cp);
				goto more;
			}
			if (*ccp == '-') {
				cp = ccp + 1;
				sep->se_rpcversh = strtol(cp, &ccp, 0);
				if (ccp == cp)
					goto badafterall;
			}
#else
			syslog(LOG_ERR, "%s: rpc services not suported",
			    sep->se_service);
			goto more;
#endif /* RPC */
		}
	}
	arg = sskip(&cp);
	{
		char *cp;
		cp = strchr(arg, '.');
		if (cp) {
			*cp++ = '\0';
			sep->se_max = atoi(cp);
		} else
			sep->se_max = TOOMANY;
	}
	sep->se_wait = strcmp(arg, "wait") == 0;
	if (ISMUX(sep)) {
		/*
		 * Silently enforce "nowait" for TCPMUX services since
		 * they don't have an assigned port to listen on.
		 */
		sep->se_wait = 0;

		if (strcmp(sep->se_proto, "tcp")) {
			syslog(LOG_ERR, 
			    "%s: bad protocol for tcpmux service %s",
			    CONFIG, sep->se_service);
			goto more;
		}
		if (sep->se_socktype != SOCK_STREAM) {
			syslog(LOG_ERR, 
			    "%s: bad socket type for tcpmux service %s",
			    CONFIG, sep->se_service);
			goto more;
		}
	}
	sep->se_user = newstr(sskip(&cp));
	if ((sep->se_group = strchr(sep->se_user, '.')))
		*sep->se_group++ = '\0';
	sep->se_server = newstr(sskip(&cp));
	if (strcmp(sep->se_server, "internal") == 0) {
		struct biltin *bi;

		for (bi = biltins; bi->bi_service; bi++)
			if (bi->bi_socktype == sep->se_socktype &&
			    strcmp(bi->bi_service, sep->se_service) == 0)
				break;
		if (bi->bi_service == 0) {
			syslog(LOG_ERR, "internal service %s unknown",
			    sep->se_service);
			goto more;
		}
		sep->se_bi = bi;
		sep->se_wait = bi->bi_wait;
	} else
		sep->se_bi = NULL;
	argc = 0;
	for (arg = skip(&cp); cp; arg = skip(&cp)) {
#if MULOG
		char *colon;

		if (argc == 0 && (colon = strrchr(arg, ':'))) {
			while (arg < colon) {
				int	x;
				char	*ccp;

				switch (*arg++) {
				case 'l':
					x = 1;
					if (isdigit(*arg)) {
						x = strtol(arg, &ccp, 0);
						if (ccp == arg)
							break;
						arg = ccp;
					}
					sep->se_log &= ~MULOG_RFC931;
					sep->se_log |= x;
					break;
				case 'a':
					sep->se_log |= MULOG_RFC931;
					break;
				default:
					break;
				}
			}
			arg = colon + 1;
		}
#endif
		if (argc < MAXARGV)
			sep->se_argv[argc++] = newstr(arg);
	}
	while (argc <= MAXARGV)
		sep->se_argv[argc++] = NULL;
	return (sep);
}

void
freeconfig(cp)
	struct servtab *cp;
{
	int i;

	if (cp->se_hostaddr)
		free(cp->se_hostaddr);
	if (cp->se_service)
		free(cp->se_service);
	if (cp->se_proto)
		free(cp->se_proto);
	if (cp->se_user)
		free(cp->se_user);
	/* Note: se_group is part of the newstr'ed se_user */
	if (cp->se_server)
		free(cp->se_server);
	for (i = 0; i < MAXARGV; i++)
		if (cp->se_argv[i])
			free(cp->se_argv[i]);
}


/*
 * Safe skip - if skip returns null, log a syntax error in the
 * configuration file and exit.
 */
char *
sskip(cpp)
	char **cpp;
{
	char *cp;

	cp = skip(cpp);
	if (cp == NULL) {
		syslog(LOG_ERR, "%s: syntax error", CONFIG);
		exit(-1);
	}
	return (cp);
}

char *
skip(cpp)
	char **cpp;
{
	char *cp = *cpp;
	char *start;

	if (*cpp == NULL)
		return ((char *)0);

again:
	while (*cp == ' ' || *cp == '\t')
		cp++;
	if (*cp == '\0') {
		int c;

		c = getc(fconfig);
		(void) ungetc(c, fconfig);
		if (c == ' ' || c == '\t')
			if ((cp = nextline(fconfig)))
				goto again;
		*cpp = (char *)0;
		return ((char *)0);
	}
	start = cp;
	while (*cp && *cp != ' ' && *cp != '\t')
		cp++;
	if (*cp != '\0')
		*cp++ = '\0';
	*cpp = cp;
	return (start);
}

char *
nextline(fd)
	FILE *fd;
{
	char *cp;

	if (fgets(line, sizeof (line), fd) == NULL)
		return ((char *)0);
	cp = strchr(line, '\n');
	if (cp)
		*cp = '\0';
	return (line);
}

char *
newstr(cp)
	char *cp;
{
	if ((cp = strdup(cp ? cp : "")))
		return (cp);
	syslog(LOG_ERR, "strdup: %m");
	exit(-1);
}

void
inetd_setproctitle(a, s)
	char *a;
	int s;
{
	int size;
	char *cp;
	struct sockaddr_in sin;
	char buf[80];

	cp = Argv[0];
	size = sizeof(sin);
	if (getpeername(s, (struct sockaddr *)&sin, &size) == 0)
		(void)snprintf(buf, sizeof buf, "-%s [%s]", a,
		    inet_ntoa(sin.sin_addr));
	else
		(void)snprintf(buf, sizeof buf, "-%s", a);
	strncpy(cp, buf, LastArg - cp);
	cp += strlen(cp);
	while (cp < LastArg)
		*cp++ = ' ';
}

void
logpid()
{
	FILE *fp;

	if ((fp = fopen(_PATH_INETDPID, "w")) != NULL) {
		fprintf(fp, "%u\n", getpid());
		(void)fclose(fp);
	}
}

void
bump_nofile()
{
#ifdef RLIMIT_NOFILE

#define FD_CHUNK	32

	struct rlimit rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
		syslog(LOG_ERR, "getrlimit: %m");
		return;
	}
	rl.rlim_cur = MIN(rl.rlim_max, rl.rlim_cur + FD_CHUNK);
	if (rl.rlim_cur <= rlim_ofile_cur) {
		syslog(LOG_ERR,
		    "bump_nofile: cannot extend file limit, max = %d",
		    (int)rl.rlim_cur);
		return;
	}

	if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
		syslog(LOG_ERR, "setrlimit: %m");
		return;
	}

	rlim_ofile_cur = rl.rlim_cur;
	return;

#else
	syslog(LOG_ERR, "bump_nofile: cannot extend file limit");
	return;
#endif
}

/*
 * Internet services provided internally by inetd:
 */
#define	BUFSIZE	4096

/* ARGSUSED */
void
echo_stream(s, sep)		/* Echo service -- echo data back */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];
	int i;

	inetd_setproctitle(sep->se_service, s);
	while ((i = read(s, buffer, sizeof(buffer))) > 0 &&
	    write(s, buffer, i) > 0)
		;
}

/* ARGSUSED */
void
echo_dg(s, sep)			/* Echo service -- echo data back */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];
	int i, size;
	struct sockaddr sa;

	size = sizeof(sa);
	if ((i = recvfrom(s, buffer, sizeof(buffer), 0, &sa, &size)) < 0)
		return;
	(void) sendto(s, buffer, i, 0, &sa, sizeof(sa));
}

/* ARGSUSED */
void
discard_stream(s, sep)		/* Discard service -- ignore data */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];

	inetd_setproctitle(sep->se_service, s);
	while ((errno = 0, read(s, buffer, sizeof(buffer)) > 0) ||
			errno == EINTR)
		;
}

/* ARGSUSED */
void
discard_dg(s, sep)		/* Discard service -- ignore data */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];

	(void) read(s, buffer, sizeof(buffer));
}

#include <ctype.h>
#define LINESIZ 72
char ring[128];
char *endring;

void
initring()
{
	int i;

	endring = ring;

	for (i = 0; i <= 128; ++i)
		if (isprint(i))
			*endring++ = i;
}

/* ARGSUSED */
void
chargen_stream(s, sep)		/* Character generator */
	int s;
	struct servtab *sep;
{
	int len;
	char *rs, text[LINESIZ+2];

	inetd_setproctitle(sep->se_service, s);

	if (!endring) {
		initring();
		rs = ring;
	}

	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	for (rs = ring;;) {
		if ((len = endring - rs) >= LINESIZ)
			memmove(text, rs, LINESIZ);
		else {
			memmove(text, rs, len);
			memmove(text + len, ring, LINESIZ - len);
		}
		if (++rs == endring)
			rs = ring;
		if (write(s, text, sizeof(text)) != sizeof(text))
			break;
	}
}

/* ARGSUSED */
void
chargen_dg(s, sep)		/* Character generator */
	int s;
	struct servtab *sep;
{
	struct sockaddr sa;
	static char *rs;
	int len, size;
	char text[LINESIZ+2];

	if (endring == 0) {
		initring();
		rs = ring;
	}

	size = sizeof(sa);
	if (recvfrom(s, text, sizeof(text), 0, &sa, &size) < 0)
		return;

	if ((len = endring - rs) >= LINESIZ)
		memmove(text, rs, LINESIZ);
	else {
		memmove(text, rs, len);
		memmove(text + len, ring, LINESIZ - len);
	}
	if (++rs == endring)
		rs = ring;
	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	(void) sendto(s, text, sizeof(text), 0, &sa, sizeof(sa));
}

/*
 * Return a machine readable date and time, in the form of the
 * number of seconds since midnight, Jan 1, 1900.  Since gettimeofday
 * returns the number of seconds since midnight, Jan 1, 1970,
 * we must add 2208988800 seconds to this figure to make up for
 * some seventy years Bell Labs was asleep.
 */

long
machtime()
{
	struct timeval tv;

	if (gettimeofday(&tv, (struct timezone *)0) < 0) {
		if (debug)
			fprintf(stderr, "Unable to get time of day\n");
		return (0L);
	}
#define	OFFSET ((u_long)25567 * 24*60*60)
	return (htonl((long)(tv.tv_sec + OFFSET)));
#undef OFFSET
}

/* ARGSUSED */
void
machtime_stream(s, sep)
	int s;
	struct servtab *sep;
{
	long result;

	result = machtime();
	(void) write(s, (char *) &result, sizeof(result));
}

/* ARGSUSED */
void
machtime_dg(s, sep)
	int s;
	struct servtab *sep;
{
	long result;
	struct sockaddr sa;
	int size;

	size = sizeof(sa);
	if (recvfrom(s, (char *)&result, sizeof(result), 0, &sa, &size) < 0)
		return;
	result = machtime();
	(void) sendto(s, (char *) &result, sizeof(result), 0, &sa, sizeof(sa));
}

/* ARGSUSED */
void
daytime_stream(s, sep)		/* Return human-readable time of day */
	int s;
	struct servtab *sep;
{
	char buffer[256];
	time_t clock;
	int len;

	clock = time((time_t *) 0);

	len = snprintf(buffer, sizeof buffer, "%.24s\r\n", ctime(&clock));
	(void) write(s, buffer, len);
}

/* ARGSUSED */
void
daytime_dg(s, sep)		/* Return human-readable time of day */
	int s;
	struct servtab *sep;
{
	char buffer[256];
	time_t clock;
	struct sockaddr sa;
	int size, len;

	clock = time((time_t *) 0);

	size = sizeof(sa);
	if (recvfrom(s, buffer, sizeof(buffer), 0, &sa, &size) < 0)
		return;
	len = snprintf(buffer, sizeof buffer, "%.24s\r\n", ctime(&clock));
	(void) sendto(s, buffer, len, 0, &sa, sizeof(sa));
}

/*
 * print_service:
 *	Dump relevant information to stderr
 */
void
print_service(action, sep)
	char *action;
	struct servtab *sep;
{
	if (isrpcservice(sep))
		fprintf(stderr,
		    "%s: %s rpcprog=%d, rpcvers = %d/%d, proto=%s, wait.max=%d.%d, user.group=%s.%s builtin=%lx server=%s\n",
		    action, sep->se_service,
		    sep->se_rpcprog, sep->se_rpcversh, sep->se_rpcversl, sep->se_proto,
		    sep->se_wait, sep->se_max, sep->se_user, sep->se_group,
		    (long)sep->se_bi, sep->se_server);
	else
		fprintf(stderr,
		    "%s: %s proto=%s, wait.max=%d.%d, user.group=%s.%s builtin=%lx server=%s\n",
		    action, sep->se_service, sep->se_proto,
		    sep->se_wait, sep->se_max, sep->se_user, sep->se_group,
		    (long)sep->se_bi, sep->se_server);
}

void
usage()
{

#ifdef LIBWRAP
	(void)fprintf(stderr, "usage: %s [-dl] [conf]\n", __progname);
#else
	(void)fprintf(stderr, "usage: %s [-d] [conf]\n", __progname);
#endif
	exit(1);
}


/*
 *  Based on TCPMUX.C by Mark K. Lottor November 1988
 *  sri-nic::ps:<mkl>tcpmux.c
 */

static int		/* # of characters upto \r,\n or \0 */
getline(fd, buf, len)
	int fd;
	char *buf;
	int len;
{
	int count = 0, n;

	do {
		n = read(fd, buf, len-count);
		if (n == 0)
			return (count);
		if (n < 0)
			return (-1);
		while (--n >= 0) {
			if (*buf == '\r' || *buf == '\n' || *buf == '\0')
				return (count);
			count++;
			buf++;
		}
	} while (count < len);
	return (count);
}

#define MAX_SERV_LEN	(256+2)		/* 2 bytes for \r\n */

#define strwrite(fd, buf)	(void) write(fd, buf, sizeof(buf)-1)

void
tcpmux(ctrl, sep)
	int ctrl;
	struct servtab *sep;
{
	char service[MAX_SERV_LEN+1];
	int len;

	/* Get requested service name */
	if ((len = getline(ctrl, service, MAX_SERV_LEN)) < 0) {
		strwrite(ctrl, "-Error reading service name\r\n");
		goto reject;
	}
	service[len] = '\0';

	if (debug)
		fprintf(stderr, "tcpmux: someone wants %s\n", service);

	/*
	 * Help is a required command, and lists available services,
	 * one per line.
	 */
	if (!strcasecmp(service, "help")) {
		strwrite(ctrl, "+Available services:\r\n");
		strwrite(ctrl, "help\r\n");
		for (sep = servtab; sep; sep = sep->se_next) {
			if (!ISMUX(sep))
				continue;
			(void)write(ctrl, sep->se_service,
			    strlen(sep->se_service));
			strwrite(ctrl, "\r\n");
		}
		goto reject;
	}

	/* Try matching a service in inetd.conf with the request */
	for (sep = servtab; sep; sep = sep->se_next) {
		if (!ISMUX(sep))
			continue;
		if (!strcasecmp(service, sep->se_service)) {
			if (ISMUXPLUS(sep))
				strwrite(ctrl, "+Go\r\n");
			run_service(ctrl, sep);
			return;
		}
	}
	strwrite(ctrl, "-Service not available\r\n");
reject:
	_exit(1);
}


#ifdef MULOG
dolog(sep, ctrl)
	struct servtab *sep;
	int		ctrl;
{
	struct sockaddr		sa;
	struct sockaddr_in	*sin = (struct sockaddr_in *)&sa;
	int			len = sizeof(sa);
	struct hostent		*hp;
	char			*host, *dp, buf[BUFSIZ], *rfc931_name();
	int			connected = 1;

	if (sep->se_family != AF_INET)
		return;

	if (getpeername(ctrl, &sa, &len) < 0) {
		if (errno != ENOTCONN) {
			syslog(LOG_ERR, "getpeername: %m");
			return;
		}
		if (recvfrom(ctrl, buf, sizeof(buf), MSG_PEEK, &sa, &len) < 0) {
			syslog(LOG_ERR, "recvfrom: %m");
			return;
		}
		connected = 0;
	}
	if (sa.sa_family != AF_INET) {
		syslog(LOG_ERR, "unexpected address family %u", sa.sa_family);
		return;
	}

	hp = gethostbyaddr((char *) &sin->sin_addr.s_addr,
				sizeof (sin->sin_addr.s_addr), AF_INET);

	host = hp?hp->h_name:inet_ntoa(sin->sin_addr);

	switch (sep->se_log & ~MULOG_RFC931) {
	case 0:
		return;
	case 1:
		if (curdom == NULL || *curdom == '\0')
			break;
		dp = host + strlen(host) - strlen(curdom);
		if (dp < host)
			break;
		if (debug)
			fprintf(stderr, "check \"%s\" against curdom \"%s\"\n",
			    host, curdom);
		if (strcasecmp(dp, curdom) == 0)
			return;
		break;
	case 2:
	default:
		break;
	}

	openlog("", LOG_NOWAIT, MULOG);

	if (connected && (sep->se_log & MULOG_RFC931))
		syslog(LOG_INFO, "%s@%s wants %s",
		    rfc931_name(sin, ctrl), host, sep->se_service);
	else
		syslog(LOG_INFO, "%s wants %s",
		    host, sep->se_service);
}

/*
 * From tcp_log by
 *  Wietse Venema, Eindhoven University of Technology, The Netherlands.
 */
#if 0
static char sccsid[] = "@(#) rfc931.c 1.3 92/08/31 22:54:46";
#endif

#include <setjmp.h>

#define	RFC931_PORT	113		/* Semi-well-known port */
#define	TIMEOUT		4
#define	TIMEOUT2	10

static jmp_buf timebuf;

/* timeout - handle timeouts */

static void timeout(sig)
int     sig;
{
	longjmp(timebuf, sig);
}

/* rfc931_name - return remote user name */

char *
rfc931_name(there, ctrl)
struct sockaddr_in *there;		/* remote link information */
int	ctrl;
{
	struct sockaddr_in here;	/* local link information */
	struct sockaddr_in sin;		/* for talking to RFC931 daemon */
	int		length;
	int		s;
	unsigned	remote;
	unsigned	local;
	static char	user[256];		/* XXX */
	char		buf[256];
	char		*cp;
	char		*result = "USER_UNKNOWN";
	int		len;

	/* Find out local port number of our stdin. */

	length = sizeof(here);
	if (getsockname(ctrl, (struct sockaddr *) &here, &length) == -1) {
		syslog(LOG_ERR, "getsockname: %m");
		return (result);
	}
	/* Set up timer so we won't get stuck. */

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		syslog(LOG_ERR, "socket: %m");
		return (result);
	}

	sin = here;
	sin.sin_port = htons(0);
	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
		syslog(LOG_ERR, "bind: %m");
		return (result);
	}

	signal(SIGALRM, timeout);
	if (setjmp(timebuf)) {
		close(s);			/* not: fclose(fp) */
		return (result);
	}
	alarm(TIMEOUT);

	/* Connect to the RFC931 daemon. */

	sin = *there;
	sin.sin_port = htons(RFC931_PORT);
	if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
		close(s);
		alarm(0);
		return (result);
	}

	/* Query the RFC 931 server. Would 13-byte writes ever be broken up? */
	(void)snprintf(buf, sizeof buf, "%u,%u\r\n", ntohs(there->sin_port),
	    ntohs(here.sin_port));


	for (len = 0, cp = buf; len < strlen(buf); ) {
		int	n;

		if ((n = write(s, cp, strlen(buf) - len)) == -1) {
			close(s);
			alarm(0);
			return (result);
		}
		cp += n;
		len += n;
	}

	/* Read response */
	for (cp = buf; cp < buf + sizeof(buf) - 1; ) {
		char	c;
		if (read(s, &c, 1) != 1) {
			close(s);
			alarm(0);
			return (result);
		}
		if (c == '\n')
			break;
		*cp++ = c;
	}
	*cp = '\0';

	if (sscanf(buf, "%u , %u : USERID :%*[^:]:%255s", &remote, &local, user) == 3
		&& ntohs(there->sin_port) == remote
		&& ntohs(here.sin_port) == local) {

		/* Strip trailing carriage return. */
		if (cp = strchr(user, '\r'))
			*cp = 0;
		result = user;
	}

	alarm(0);
	close(s);
	return (result);
}
#endif
