/*	$NetBSD: ftpd.c,v 1.95.2.1 2000/06/22 08:45:10 lukem Exp $	*/

/*
 * Copyright (c) 1997-2000 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1985, 1988, 1990, 1992, 1993, 1994
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

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT(
"@(#) Copyright (c) 1985, 1988, 1990, 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ftpd.c	8.5 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: ftpd.c,v 1.95.2.1 2000/06/22 08:45:10 lukem Exp $");
#endif
#endif /* not lint */

/*
 * FTP server.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#define	FTP_NAMES
#include <arpa/ftp.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <grp.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <util.h>
#include <utmp.h>
#ifdef SKEY
#include <skey.h>
#endif
#ifdef KERBEROS5
#include <com_err.h>
#include <krb5/krb5.h>
#endif

#define	GLOBAL
#include "extern.h"
#include "pathnames.h"
#include "version.h"

int	data;
jmp_buf	urgcatch;
struct	passwd *pw;
int	sflag;
int	stru;			/* avoid C keyword */
int	mode;
int	doutmp = 0;		/* update utmp file */
int	mapped = 0;		/* IPv4 connection on AF_INET6 socket */
off_t	file_size;
off_t	byte_count;
static char ttyline[20];
static struct utmp utmp;	/* for utmp */

static char *anondir = NULL;
static char confdir[MAXPATHLEN];

#if defined(KERBEROS) || defined(KERBEROS5)
int	has_ccache = 0;
int	notickets = 1;
char	*krbtkfile_env = NULL;
char	*tty = ttyline;
int	login_krb5_forwardable_tgt = 0;
#endif

int epsvall = 0;

/*
 * Timeout intervals for retrying connections
 * to hosts that don't accept PORT cmds.  This
 * is a kludge, but given the problems with TCP...
 */
#define	SWAITMAX	90	/* wait at most 90 seconds */
#define	SWAITINT	5	/* interval between retries */

int	swaitmax = SWAITMAX;
int	swaitint = SWAITINT;

static int	 bind_pasv_addr(void);
static int	 checkuser(const char *, const char *, int, int, char **);
static int	 checkaccess(const char *);
static void	 dolog(struct sockaddr *);
static void	 end_login(void);
static FILE	*getdatasock(const char *);
static char	*gunique(const char *);
static void	 lostconn(int);
static void	 myoob(int);
static int	 receive_data(FILE *, FILE *);
static int	 send_data(FILE *, FILE *, off_t, int);
static struct passwd *sgetpwnam(const char *);

int	main(int, char *[]);

#if defined(KERBEROS)
int	klogin(struct passwd *, char *, char *, char *);
void	kdestroy(void);
#endif
#if defined(KERBEROS5)
int	k5login(struct passwd *, char *, char *, char *);
void	k5destroy(void);
#endif

int
main(int argc, char *argv[])
{
	int		addrlen, ch, on = 1, tos, keepalive;
#ifdef KERBEROS5
	krb5_error_code	kerror;
#endif

	connections = 1;
	debug = 0;
	logging = 0;
	pdata = -1;
	sflag = 0;
	usedefault = 1;
	(void)strcpy(confdir, _DEFAULT_CONFDIR);
	hostname[0] = '\0';
	gidcount = 0;

	while ((ch = getopt(argc, argv, "a:c:C:dh:lst:T:u:Uv")) != -1) {
		switch (ch) {
		case 'a':
			anondir = optarg;
			break;

		case 'c':
			(void)strlcpy(confdir, optarg, sizeof(confdir));
			break;

		case 'C':
			pw = sgetpwnam(optarg);
			exit(checkaccess(optarg) ? 0 : 1);
			/* NOTREACHED */

		case 'd':
		case 'v':		/* deprecated */
			debug = 1;
			break;

		case 'h':
			strlcpy(hostname, optarg, sizeof(hostname));
			break;

		case 'l':
			logging++;	/* > 1 == extra logging */
			break;

		case 's':
			sflag = 1;
			break;

		case 't':
		case 'T':
		case 'u':
			warnx("-%c has been deprecated in favour of ftpd.conf",
			    ch);
			break;

		case 'U':
			doutmp = 1;
			break;

		default:
			if (optopt == 'a' || optopt == 'C')
				exit(1);
			warnx("unknown flag -%c ignored", optopt);
			break;
		}
	}

	/*
	 * LOG_NDELAY sets up the logging connection immediately,
	 * necessary for anonymous ftp's that chroot and can't do it later.
	 */
	openlog("ftpd", LOG_PID | LOG_NDELAY, LOG_FTP);
	addrlen = sizeof(his_addr); /* xxx */
	if (getpeername(0, (struct sockaddr *)&his_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getpeername (%s): %m",argv[0]);
		exit(1);
	}
	addrlen = sizeof(ctrl_addr);
	if (getsockname(0, (struct sockaddr *)&ctrl_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getsockname (%s): %m",argv[0]);
		exit(1);
	}
	if (his_addr.su_family == AF_INET6
	 && IN6_IS_ADDR_V4MAPPED(&his_addr.su_sin6.sin6_addr)) {
#if 1
		/*
		 * IPv4 control connection arrived to AF_INET6 socket.
		 * I hate to do this, but this is the easiest solution.
		 *
		 * The assumption is untrue on SIIT environment.
		 */
		union sockunion tmp_addr;
		const int off = sizeof(struct in6_addr) - sizeof(struct in_addr);

		tmp_addr = his_addr;
		memset(&his_addr, 0, sizeof(his_addr));
		his_addr.su_sin.sin_family = AF_INET;
		his_addr.su_sin.sin_len = sizeof(his_addr.su_sin);
		memcpy(&his_addr.su_sin.sin_addr,
			&tmp_addr.su_sin6.sin6_addr.s6_addr[off],
			sizeof(his_addr.su_sin.sin_addr));
		his_addr.su_sin.sin_port = tmp_addr.su_sin6.sin6_port;

		tmp_addr = ctrl_addr;
		memset(&ctrl_addr, 0, sizeof(ctrl_addr));
		ctrl_addr.su_sin.sin_family = AF_INET;
		ctrl_addr.su_sin.sin_len = sizeof(ctrl_addr.su_sin);
		memcpy(&ctrl_addr.su_sin.sin_addr,
			&tmp_addr.su_sin6.sin6_addr.s6_addr[off],
			sizeof(ctrl_addr.su_sin.sin_addr));
		ctrl_addr.su_sin.sin_port = tmp_addr.su_sin6.sin6_port;
#else
		while (fgets(line, sizeof(line), fd) != NULL) {
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			reply(-530, "%s", line);
		}
		(void) fflush(stdout);
		(void) fclose(fd);
		reply(530,
			"Connection from IPv4 mapped address is not supported.");
		exit(0);
#endif

		mapped = 1;
	} else
		mapped = 0;
#ifdef IP_TOS
	if (!mapped && his_addr.su_family == AF_INET) {
		tos = IPTOS_LOWDELAY;
		if (setsockopt(0, IPPROTO_IP, IP_TOS, (char *)&tos,
			       sizeof(int)) < 0)
			syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
	}
#endif
	data_source.su_port = htons(ntohs(ctrl_addr.su_port) - 1);

	/* if the hostname hasn't been given, attempt to determine it */ 
	if (hostname[0] == '\0') {
		if (getnameinfo((struct sockaddr *)&ctrl_addr, ctrl_addr.su_len,
		    hostname, sizeof(hostname), NULL, 0, 0) != 0)
			(void)gethostname(hostname, sizeof(hostname));
		hostname[sizeof(hostname) - 1] = '\0';
	}

	/* set this here so klogin can use it... */
	(void)snprintf(ttyline, sizeof(ttyline), "ftp%d", getpid());

	(void) freopen(_PATH_DEVNULL, "w", stderr);
	(void) signal(SIGPIPE, lostconn);
	(void) signal(SIGCHLD, SIG_IGN);
	if (signal(SIGURG, myoob) == SIG_ERR)
		syslog(LOG_ERR, "signal: %m");

	/* Try to handle urgent data inline */
#ifdef SO_OOBINLINE
	if (setsockopt(0, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on)) < 0)
		syslog(LOG_ERR, "setsockopt: %m");
#endif
	/* Set keepalives on the socket to detect dropped connections.  */
#ifdef SO_KEEPALIVE
	keepalive = 1;
	if (setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (char *)&keepalive,
	    sizeof(int)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
#endif

#ifdef	F_SETOWN
	if (fcntl(fileno(stdin), F_SETOWN, getpid()) == -1)
		syslog(LOG_ERR, "fcntl F_SETOWN: %m");
#endif
	dolog((struct sockaddr *)&his_addr);
	/*
	 * Set up default state
	 */
	data = -1;
	type = TYPE_A;
	form = FORM_N;
	stru = STRU_F;
	mode = MODE_S;
	tmpline[0] = '\0';
	hasyyerrored = 0;

#ifdef KERBEROS5
	kerror = krb5_init_context(&kcontext);
	if (kerror) {
		syslog(LOG_NOTICE, "%s when initializing Kerberos context",
		    error_message(kerror));
		exit(0);
	}
#endif /* KERBEROS5 */

	init_curclass();
	curclass.timeout = 300;		/* 5 minutes, as per login(1) */
	curclass.type = CLASS_REAL;

	/* If logins are disabled, print out the message. */
	if (format_file(_PATH_NOLOGIN, 530)) {
		reply(530, "System not available.");
		exit(0);
	}
	(void)format_file(conffilename(_PATH_FTPWELCOME), 220);
		/* reply(220,) must follow */
	reply(220, "%s FTP server (%s) ready.", hostname, FTPD_VERSION);

	(void) setjmp(errcatch);
	for (;;)
		(void) yyparse();
	/* NOTREACHED */
}

static void
lostconn(int signo)
{

	if (debug)
		syslog(LOG_DEBUG, "lost connection");
	dologout(1);
}

/*
 * Save the result of a getpwnam.  Used for USER command, since
 * the data returned must not be clobbered by any other command
 * (e.g., globbing).
 */
static struct passwd *
sgetpwnam(const char *name)
{
	static struct passwd save;
	struct passwd *p;

	if ((p = getpwnam(name)) == NULL)
		return (p);
	if (save.pw_name) {
		free((char *)save.pw_name);
		memset(save.pw_passwd, 0, strlen(save.pw_passwd));
		free((char *)save.pw_passwd);
		free((char *)save.pw_gecos);
		free((char *)save.pw_dir);
		free((char *)save.pw_shell);
	}
	save = *p;
	save.pw_name = xstrdup(p->pw_name);
	save.pw_passwd = xstrdup(p->pw_passwd);
	save.pw_gecos = xstrdup(p->pw_gecos);
	save.pw_dir = xstrdup(p->pw_dir);
	save.pw_shell = xstrdup(p->pw_shell);
	return (&save);
}

static int	login_attempts;	/* number of failed login attempts */
static int	askpasswd;	/* had user command, ask for passwd */
static char	curname[10];	/* current USER name */

/*
 * USER command.
 * Sets global passwd pointer pw if named account exists and is acceptable;
 * sets askpasswd if a PASS command is expected.  If logged in previously,
 * need to reset state.  If name is "ftp" or "anonymous", the name is not in
 * _PATH_FTPUSERS, and ftp account exists, set guest and pw, then just return.
 * If account doesn't exist, ask for passwd anyway.  Otherwise, check user
 * requesting login privileges.  Disallow anyone who does not have a standard
 * shell as returned by getusershell().  Disallow anyone mentioned in the file
 * _PATH_FTPUSERS to allow people such as root and uucp to be avoided.
 */
void
user(const char *name)
{
	if (logged_in) {
		switch (curclass.type) {
		case CLASS_GUEST:
			reply(530, "Can't change user from guest login.");
			return;
		case CLASS_CHROOT:
			reply(530, "Can't change user from chroot user.");
			return;
		case CLASS_REAL:
			end_login();
			break;
		default:
			abort();
		}
	}

#if defined(KERBEROS)
	kdestroy();
#endif
#if defined(KERBEROS5)
	k5destroy();
#endif

	curclass.type = CLASS_REAL;
	if (strcmp(name, "ftp") == 0 || strcmp(name, "anonymous") == 0) {
			/* need `pw' setup for checkaccess() and checkuser () */
		if ((pw = sgetpwnam("ftp")) == NULL)
			reply(530, "User %s unknown.", name);
		else if (! checkaccess("ftp") || ! checkaccess("anonymous"))
			reply(530, "User %s access denied.", name);
		else {
			curclass.type = CLASS_GUEST;
			askpasswd = 1;
			reply(331,
			    "Guest login ok, type your name as password.");
		}
		if (!askpasswd && logging)
			syslog(LOG_NOTICE,
			    "ANONYMOUS FTP LOGIN REFUSED FROM %s", remotehost);
		return;
	}

	pw = sgetpwnam(name);
	if (logging)
		strlcpy(curname, name, sizeof(curname));

#ifdef SKEY
	if (skey_haskey(name) == 0) {
		char *myskey;

		myskey = skey_keyinfo(name);
		reply(331, "Password [%s] required for %s.",
		    myskey ? myskey : "error getting challenge", name);
	} else
#endif
		reply(331, "Password required for %s.", name);

	askpasswd = 1;
	/*
	 * Delay before reading passwd after first failed
	 * attempt to slow down passwd-guessing programs.
	 */
	if (login_attempts)
		sleep((unsigned) login_attempts);
}

/*
 * Determine whether something is to happen (allow access, chroot)
 * for a user. Each line is a shell-style glob followed by
 * `yes' or `no'.
 *
 * For backward compatability, `allow' and `deny' are synonymns
 * for `yes' and `no', respectively.
 *
 * Each glob is matched against the username in turn, and the first
 * match found is used. If no match is found, the result is the
 * argument `def'. If a match is found but without and explicit
 * `yes'/`no', the result is the opposite of def.
 *
 * If the file doesn't exist at all, the result is the argument
 * `nofile'
 *
 * Any line starting with `#' is considered a comment and ignored.
 *
 * Returns 0 if the user is denied, or 1 if they are allowed.
 *
 * NOTE: needs struct passwd *pw setup before use.
 */
int
checkuser(const char *fname, const char *name, int def, int nofile,
	    char **retclass)
{
	FILE	*fd;
	int	 retval;
	char	*glob, *perm, *class, *buf, *p;
	size_t	 len, line;

	retval = def;
	if (retclass != NULL)
		*retclass = NULL;
	if ((fd = fopen(conffilename(fname), "r")) == NULL)
		return nofile;

	line = 0;
	for (;
	    (buf = fparseln(fd, &len, &line, NULL, FPARSELN_UNESCCOMM |
	    		FPARSELN_UNESCCONT | FPARSELN_UNESCESC)) != NULL;
	    free(buf), buf = NULL) {
		glob = perm = class = NULL;
		p = buf;
		if (len < 1)
			continue;
		if (p[len - 1] == '\n')
			p[--len] = '\0';
		if (EMPTYSTR(p))
			continue;

		NEXTWORD(p, glob);
		NEXTWORD(p, perm);
		NEXTWORD(p, class);
		if (EMPTYSTR(glob))
			continue;
		if (!EMPTYSTR(class)) {
			if (strcasecmp(class, "all") == 0 ||
			    strcasecmp(class, "none") == 0) {
				syslog(LOG_WARNING,
		"%s line %d: illegal user-defined class `%s' - skipping entry",
					    fname, (int)line, class);
				continue;
			}
		}

					/* have a host specifier */
		if ((p = strchr(glob, '@')) != NULL) {
			u_int32_t	net, mask, addr;
			int		bits;

			*p++ = '\0';
					/* check against network or CIDR */
			if (isdigit(*p) &&
			    (bits = inet_net_pton(AF_INET, p,
			    &net, sizeof(net))) != -1) {
				net = ntohl(net);
				mask = 0xffffffffU << (32 - bits);
				addr = ntohl(his_addr.su_sin.sin_addr.s_addr);
				if ((addr & mask) != net)
					continue;

					/* check against hostname glob */
			} else if (fnmatch(p, remotehost, 0) != 0)
				continue;
		}

					/* have a group specifier */
		if ((p = strchr(glob, ':')) != NULL) {
			gid_t	*groups, *ng;
			int	 gsize, i, found;

			*p++ = '\0';
			groups = NULL;
			gsize = 16;
			do {
				ng = realloc(groups, gsize * sizeof(gid_t));
				if (ng == NULL)
					fatal(
					    "Local resource failure: realloc");
				groups = ng;
			} while (getgrouplist(pw->pw_name, pw->pw_gid,
						groups, &gsize) == -1);
			found = 0;
			for (i = 0; i < gsize; i++) {
				struct group *g;

				if ((g = getgrgid(groups[i])) == NULL)
					continue;
				if (fnmatch(p, g->gr_name, 0) == 0) {
					found = 1;
					break;
				}
			}
			free(groups);
			if (!found)
				continue;
		}

					/* check against username glob */
		if (fnmatch(glob, name, 0) != 0)
			continue;

		if (perm != NULL &&
		    ((strcasecmp(perm, "allow") == 0) ||
		     (strcasecmp(perm, "yes") == 0)))
			retval = 1;
		else if (perm != NULL &&
		    ((strcasecmp(perm, "deny") == 0) ||
		     (strcasecmp(perm, "no") == 0)))
			retval = 0;
		else
			retval = !def;
		if (!EMPTYSTR(class) && retclass != NULL)
			*retclass = xstrdup(class);
		free(buf);
		break;
	}
	(void) fclose(fd);
	return (retval);
}

/*
 * Check if user is allowed by /etc/ftpusers
 * returns 1 for yes, 0 for no
 *
 * NOTE: needs struct passwd *pw setup (for checkuser())
 */
int
checkaccess(const char *name)
{

	return (checkuser(_PATH_FTPUSERS, name, 1, 0, NULL));
}

/*
 * Terminate login as previous user, if any, resetting state;
 * used when USER command is given or login fails.
 */
static void
end_login(void)
{

	(void) seteuid((uid_t)0);
	if (logged_in) {
		logwtmp(ttyline, "", "");
		if (doutmp)
			logout(utmp.ut_line);
	}
	pw = NULL;
	logged_in = 0;
	quietmessages = 0;
	gidcount = 0;
	curclass.type = CLASS_REAL;
}

void
pass(const char *passwd)
{
	int		 rval;
	const char	*cp, *shell, *home;
	char		*class;

	class = NULL;
	if (logged_in || askpasswd == 0) {
		reply(503, "Login with USER first.");
		return;
	}
	askpasswd = 0;
	if (curclass.type != CLASS_GUEST) {
				/* "ftp" is only account allowed no password */
		if (pw == NULL) {
			rval = 1;	/* failure below */
			goto skip;
		}
#if defined(KERBEROS)
		if (klogin(pw, "", hostname, (char *)passwd) == 0) {
			rval = 0;
			goto skip;
		}
#endif
#if defined(KERBEROS5)
		if (k5login(pw, "", hostname, (char *)passwd) == 0) {
			rval = 0;
			goto skip;
		}
#endif
#ifdef SKEY
		if (skey_haskey(pw->pw_name) == 0) {
			char *p;
			int r;

			p = xstrdup(passwd);
			r = skey_passcheck(pw->pw_name, p);
			free(p);
			if (r != -1) {
				rval = 0;
				goto skip;
			}
		}
#endif
		if (!sflag && *pw->pw_passwd != '\0' &&
		    !strcmp(crypt(passwd, pw->pw_passwd), pw->pw_passwd)) {
			rval = 0;
			goto skip;
		}
		rval = 1;

 skip:
		if (pw != NULL && pw->pw_expire && time(NULL) >= pw->pw_expire)
			rval = 2;
		/*
		 * If rval > 0, the user failed the authentication check
		 * above.  If rval == 0, either Kerberos or local authentication
		 * succeeded.
		 */
		if (rval) {
			reply(530, rval == 2 ? "Password expired." :
			    "Login incorrect.");
			if (logging) {
				syslog(LOG_NOTICE,
				    "FTP LOGIN FAILED FROM %s", remotehost);
				syslog(LOG_AUTHPRIV | LOG_NOTICE,
				    "FTP LOGIN FAILED FROM %s, %s",
				    remotehost, curname);
			}
			pw = NULL;
			if (login_attempts++ >= 5) {
				syslog(LOG_NOTICE,
				    "repeated login failures from %s",
				    remotehost);
				exit(0);
			}
			return;
		}
	}

	/* password was ok; see if anything else prevents login */
	if (! checkuser(_PATH_FTPUSERS, pw->pw_name, 1, 0, &class)) {
		reply(530, "User %s may not use FTP.", pw->pw_name);
		if (logging)
			syslog(LOG_NOTICE, "FTP LOGIN REFUSED FROM %s, %s",
			    remotehost, pw->pw_name);
		goto bad;
	}
	/* check for valid shell, if not guest user */
	if ((shell = pw->pw_shell) == NULL || *shell == 0)
		shell = _PATH_BSHELL;
	while ((cp = getusershell()) != NULL)
		if (strcmp(cp, shell) == 0)
			break;
	endusershell();
	if (cp == NULL && curclass.type != CLASS_GUEST) {
		reply(530, "User %s may not use FTP.", pw->pw_name);
		if (logging)
			syslog(LOG_NOTICE, "FTP LOGIN REFUSED FROM %s, %s",
			    remotehost, pw->pw_name);
		goto bad;
	}

	login_attempts = 0;		/* this time successful */
	if (setegid((gid_t)pw->pw_gid) < 0) {
		reply(550, "Can't set gid.");
		goto bad;
	}
	(void) initgroups(pw->pw_name, pw->pw_gid);
	gidcount = getgroups(sizeof(gidlist), gidlist);

	/* open wtmp before chroot */
	logwtmp(ttyline, pw->pw_name, remotehost);

	/* open utmp before chroot */
	if (doutmp) {
		memset((void *)&utmp, 0, sizeof(utmp));
		(void)time(&utmp.ut_time);
		(void)strncpy(utmp.ut_name, pw->pw_name, sizeof(utmp.ut_name));
		(void)strncpy(utmp.ut_host, remotehost, sizeof(utmp.ut_host));
		(void)strncpy(utmp.ut_line, ttyline, sizeof(utmp.ut_line));
		login(&utmp);
	}

	logged_in = 1;

			/* check user in /etc/ftpchroot */
	if (checkuser(_PATH_FTPCHROOT, pw->pw_name, 0, 0, NULL)) {
		if (curclass.type == CLASS_GUEST) {
			syslog(LOG_NOTICE,
	    "Can't change guest user to chroot class; remove entry in %s",
			    _PATH_FTPCHROOT);
			exit(1);
		}
		curclass.type = CLASS_CHROOT;
	}
	if (class == NULL) {
		switch (curclass.type) {
		case CLASS_GUEST:
			class = xstrdup("guest");
			break;
		case CLASS_CHROOT:
			class = xstrdup("chroot");
			break;
		case CLASS_REAL:
			class = xstrdup("real");
			break;
		default:
			abort();
		}
	}

	/* parse ftpd.conf, setting up various parameters */
	parse_conf(class);
	count_users();
	if (curclass.limit != -1 && connections > curclass.limit) {
		if (! EMPTYSTR(curclass.limitfile))
			(void)format_file(conffilename(curclass.limitfile),530);
		reply(530,
		    "User %s access denied, connection limit of %d reached.",
		    pw->pw_name, curclass.limit);
		syslog(LOG_NOTICE,
	"Maximum connection limit of %d for class %s reached, login refused",
		    curclass.limit, curclass.classname);
		goto bad;
	}

	home = "/";
	switch (curclass.type) {
	case CLASS_GUEST:
		/*
		 * We MUST do a chdir() after the chroot. Otherwise
		 * the old current directory will be accessible as "."
		 * outside the new root!
		 */
		if (chroot(anondir ? anondir : pw->pw_dir) < 0 ||
		    chdir("/") < 0) {
			reply(550, "Can't set guest privileges.");
			goto bad;
		}
		break;
	case CLASS_CHROOT:
		if (chroot(pw->pw_dir) < 0 || chdir("/") < 0) {
			reply(550, "Can't change root.");
			goto bad;
		}
		break;
	case CLASS_REAL:
		if (chdir(pw->pw_dir) < 0) {
			if (chdir("/") < 0) {
				reply(530,
				    "User %s: can't change directory to %s.",
				    pw->pw_name, pw->pw_dir);
				goto bad;
			} else
				reply(-230,
				    "No directory! Logging in with home=/");
		} else
			home = pw->pw_dir;
		break;
	}
	if (seteuid((uid_t)pw->pw_uid) < 0) {
		reply(550, "Can't set uid.");
		goto bad;
	}
	setenv("HOME", home, 1);

	if (curclass.type == CLASS_GUEST && passwd[0] == '-')
		quietmessages = 1;

	/*
	 * Display a login message, if it exists.
	 * N.B. reply(230,) must follow the message.
	 */
	(void)format_file(conffilename(curclass.motd), 230);
	show_chdir_messages(230);
	if (curclass.type == CLASS_GUEST) {
		reply(230, "Guest login ok, access restrictions apply.");
#ifdef HASSETPROCTITLE
		snprintf(proctitle, sizeof(proctitle),
		    "%s: anonymous/%.*s", remotehost,
		    (int) (sizeof(proctitle) - sizeof(remotehost) -
		    sizeof(": anonymous/")), passwd);
		setproctitle(proctitle);
#endif /* HASSETPROCTITLE */
		if (logging)
			syslog(LOG_INFO,
			"ANONYMOUS FTP LOGIN FROM %s, %s (class: %s, type: %s)",
			    remotehost, passwd,
			    curclass.classname, CURCLASSTYPE);
	} else {
		reply(230, "User %s logged in.", pw->pw_name);
#ifdef HASSETPROCTITLE
		snprintf(proctitle, sizeof(proctitle),
		    "%s: %s", remotehost, pw->pw_name);
		setproctitle(proctitle);
#endif /* HASSETPROCTITLE */
		if (logging)
			syslog(LOG_INFO,
			    "FTP LOGIN FROM %s as %s (class: %s, type: %s)",
			    remotehost, pw->pw_name,
			    curclass.classname, CURCLASSTYPE);
	}
	(void) umask(curclass.umask);
	goto cleanuppass;
 bad:
	/* Forget all about it... */
	end_login();
 cleanuppass:
	if (class)
		free(class);
}

void
retrieve(char *argv[], const char *name)
{
	FILE *fin, *dout;
	struct stat st;
	int (*closefunc)(FILE *) = NULL;
	int log, sendrv, closerv, stderrfd, isconversion, isdata, isls;
	struct timeval start, finish, td, *tdp;
	const char *dispname;

	sendrv = closerv = stderrfd = -1;
	isconversion = isdata = isls = log = 0;
	tdp = NULL;
	dispname = name;
	fin = dout = NULL;
	if (argv == NULL) {
		log = 1;
		isdata = 1;
		fin = fopen(name, "r");
		closefunc = fclose;
		if (fin == NULL)
			argv = do_conversion(name);
		if (argv != NULL) {
			isconversion++;
			syslog(LOG_INFO, "get command: '%s' on '%s'",
			    argv[0], name);
		}
	}
	if (argv != NULL) {
		char temp[MAXPATHLEN];

		if (strcmp(argv[0], INTERNAL_LS) == 0) {
			isls = 1;
			stderrfd = -1;
		} else {
			(void)snprintf(temp, sizeof(temp), "%s", TMPFILE);
			stderrfd = mkstemp(temp);
			if (stderrfd != -1)
				(void)unlink(temp);
		}
		dispname = argv[0];
		fin = ftpd_popen(argv, "r", stderrfd);
		closefunc = ftpd_pclose;
		st.st_size = -1;
		st.st_blksize = BUFSIZ;
	}
	if (fin == NULL) {
		if (errno != 0) {
			perror_reply(550, dispname);
			if (log)
				logcmd("get", -1, name, NULL, NULL,
				    strerror(errno));
		}
		goto cleanupretrieve;
	}
	byte_count = -1;
	if (argv == NULL
	    && (fstat(fileno(fin), &st) < 0 || !S_ISREG(st.st_mode))) {
		reply(550, "%s: not a plain file.", dispname);
		goto done;
	}
	if (restart_point) {
		if (type == TYPE_A) {
			off_t i;
			int c;

			for (i = 0; i < restart_point; i++) {
				if ((c=getc(fin)) == EOF) {
					perror_reply(550, dispname);
					goto done;
				}
				if (c == '\n')
					i++;
			}
		} else if (lseek(fileno(fin), restart_point, SEEK_SET) < 0) {
			perror_reply(550, dispname);
			goto done;
		}
	}
	dout = dataconn(dispname, st.st_size, "w");
	if (dout == NULL)
		goto done;

	(void)gettimeofday(&start, NULL);
	sendrv = send_data(fin, dout, st.st_blksize, isdata);
	(void)gettimeofday(&finish, NULL);
	(void) fclose(dout);		/* close now to affect timing stats */
	dout = NULL;
	timersub(&finish, &start, &td);
	tdp = &td;
 done:
	if (log)
		logcmd("get", byte_count, name, NULL, tdp, NULL);
	closerv = (*closefunc)(fin);
	if (sendrv == 0) {
		FILE *err;
		struct stat sb;

		if (!isls && argv != NULL && closerv != 0) {
			reply(-226,
			    "Command returned an exit status of %d",
			    closerv);
			if (isconversion)
				syslog(LOG_INFO,
				    "retrieve command: '%s' returned %d",
				    argv[0], closerv);
		}
		if (!isls && argv != NULL && stderrfd != -1 &&
		    (fstat(stderrfd, &sb) == 0) && sb.st_size > 0 &&
		    ((err = fdopen(stderrfd, "r")) != NULL)) {
			char *cp, line[LINE_MAX];

			reply(-226, "Command error messages:");
			rewind(err);
			while (fgets(line, sizeof(line), err) != NULL) {
				if ((cp = strchr(line, '\n')) != NULL)
					*cp = '\0';
				reply(0, "  %s", line);
			}
			(void) fflush(stdout);
			(void) fclose(err);
				/* a reply(226,) must follow */
		}
		reply(226, "Transfer complete.");
	}
 cleanupretrieve:
	closedataconn(dout);
	if (stderrfd != -1)
		(void)close(stderrfd);
	if (isconversion)
		free(argv);
}

void
store(const char *name, const char *mode, int unique)
{
	FILE *fout, *din;
	struct stat st;
	int (*closefunc)(FILE *);
	struct timeval start, finish, td, *tdp;
	char *desc;

	din = NULL;
	desc = (*mode == 'w') ? "put" : "append";
	if (unique && stat(name, &st) == 0 &&
	    (name = gunique(name)) == NULL) {
		logcmd(desc, -1, name, NULL, NULL, "cannot create unique file");
		goto cleanupstore;
	}

	if (restart_point)
		mode = "r+";
	fout = fopen(name, mode);
	closefunc = fclose;
	tdp = NULL;
	if (fout == NULL) {
		perror_reply(553, name);
		logcmd(desc, -1, name, NULL, NULL, strerror(errno));
		goto cleanupstore;
	}
	byte_count = -1;
	if (restart_point) {
		if (type == TYPE_A) {
			off_t i;
			int c;

			for (i = 0; i < restart_point; i++) {
				if ((c=getc(fout)) == EOF) {
					perror_reply(550, name);
					goto done;
				}
				if (c == '\n')
					i++;
			}
			/*
			 * We must do this seek to "current" position
			 * because we are changing from reading to
			 * writing.
			 */
			if (fseek(fout, 0L, SEEK_CUR) < 0) {
				perror_reply(550, name);
				goto done;
			}
		} else if (lseek(fileno(fout), restart_point, SEEK_SET) < 0) {
			perror_reply(550, name);
			goto done;
		}
	}
	din = dataconn(name, (off_t)-1, "r");
	if (din == NULL)
		goto done;
	(void)gettimeofday(&start, NULL);
	if (receive_data(din, fout) == 0) {
		if (unique)
			reply(226, "Transfer complete (unique file name:%s).",
			    name);
		else
			reply(226, "Transfer complete.");
	}
	(void)gettimeofday(&finish, NULL);
	(void) fclose(din);		/* close now to affect timing stats */
	din = NULL;
	timersub(&finish, &start, &td);
	tdp = &td;
 done:
	logcmd(desc, byte_count, name, NULL, tdp, NULL);
	(*closefunc)(fout);
 cleanupstore:
	closedataconn(din);
}

static FILE *
getdatasock(const char *mode)
{
	int on = 1, s, t, tries;

	if (data >= 0)
		return (fdopen(data, mode));
	(void) seteuid((uid_t)0);
	s = socket(ctrl_addr.su_family, SOCK_STREAM, 0);
	if (s < 0)
		goto bad;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
	    (char *) &on, sizeof(on)) < 0)
		goto bad;
	if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE,
	    (char *) &on, sizeof(on)) < 0)
		goto bad;
	/* anchor socket to avoid multi-homing problems */
	data_source = ctrl_addr;
	data_source.su_port = htons(20); /* ftp-data port */
	for (tries = 1; ; tries++) {
		if (bind(s, (struct sockaddr *)&data_source,
		    data_source.su_len) >= 0)
			break;
		if (errno != EADDRINUSE || tries > 10)
			goto bad;
		sleep(tries);
	}
	(void) seteuid((uid_t)pw->pw_uid);
#ifdef IP_TOS
	if (!mapped && ctrl_addr.su_family == AF_INET) {
		on = IPTOS_THROUGHPUT;
		if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&on,
			       sizeof(int)) < 0)
			syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
	}
#endif
	return (fdopen(s, mode));
 bad:
	/* Return the real value of errno (close may change it) */
	t = errno;
	(void) seteuid((uid_t)pw->pw_uid);
	(void) close(s);
	errno = t;
	return (NULL);
}

FILE *
dataconn(const char *name, off_t size, const char *mode)
{
	char sizebuf[32];
	FILE *file;
	int retry = 0, tos, keepalive;

	file_size = size;
	byte_count = 0;
	if (size != (off_t) -1)
		(void)snprintf(sizebuf, sizeof(sizebuf), " (%qd byte%s)",
		    (qdfmt_t)size, PLURAL(size));
	else
		sizebuf[0] = '\0';
	if (pdata >= 0) {
		union sockunion from;
		int s, fromlen = sizeof(from);

		(void) alarm(curclass.timeout);
		s = accept(pdata, (struct sockaddr *)&from, &fromlen);
		(void) alarm(0);
		if (s < 0) {
			reply(425, "Can't open data connection.");
			(void) close(pdata);
			pdata = -1;
			return (NULL);
		}
		(void) close(pdata);
		pdata = s;
		switch (from.su_family) {
		case AF_INET:
#ifdef IP_TOS
			if (!mapped) {
				tos = IPTOS_THROUGHPUT;
				(void) setsockopt(s, IPPROTO_IP, IP_TOS,
				    (char *)&tos, sizeof(int));
			}
			break;
#endif
		}
		/* Set keepalives on the socket to detect dropped conns. */
#ifdef SO_KEEPALIVE
		keepalive = 1;
		(void) setsockopt(s, SOL_SOCKET, SO_KEEPALIVE,
		    (char *)&keepalive, sizeof(int));
#endif
		reply(150, "Opening %s mode data connection for '%s'%s.",
		     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
		return (fdopen(pdata, mode));
	}
	if (data >= 0) {
		reply(125, "Using existing data connection for '%s'%s.",
		    name, sizebuf);
		usedefault = 1;
		return (fdopen(data, mode));
	}
	if (usedefault)
		data_dest = his_addr;
	usedefault = 1;
	file = getdatasock(mode);
	if (file == NULL) {
		char hbuf[INET6_ADDRSTRLEN];
		char pbuf[10];
		getnameinfo((struct sockaddr *)&data_source, data_source.su_len,
			hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
			NI_NUMERICHOST | NI_NUMERICSERV);
		reply(425, "Can't create data socket (%s,%s): %s.",
		      hbuf, pbuf, strerror(errno));
		return (NULL);
	}
	data = fileno(file);
	while (connect(data, (struct sockaddr *)&data_dest,
	    data_dest.su_len) < 0) {
		if (errno == EADDRINUSE && retry < swaitmax) {
			sleep((unsigned) swaitint);
			retry += swaitint;
			continue;
		}
		perror_reply(425, "Can't build data connection");
		(void) fclose(file);
		data = -1;
		return (NULL);
	}
	reply(150, "Opening %s mode data connection for '%s'%s.",
	     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
	return (file);
}

void
closedataconn(FILE *fd)
{

	if (fd != NULL)
		(void)fclose(fd);
	data = -1;
	if (pdata >= 0)
		(void)close(pdata);
	pdata = -1;
}

/*
 * Tranfer the contents of "instr" to "outstr" peer using the appropriate
 * encapsulation of the data subject * to Mode, Structure, and Type.
 *
 * NB: Form isn't handled.
 */
static int
send_data(FILE *instr, FILE *outstr, off_t blksize, int isdata)
{
	int	 c, filefd, netfd, rval;
	char	*buf;

	transflag = 1;
	rval = -1;
	buf = NULL;
	if (setjmp(urgcatch))
		goto cleanup_send_data;

	switch (type) {

	case TYPE_A:
 /* XXXX: rate limit ascii send (get) */
		(void) alarm(curclass.timeout);
		while ((c = getc(instr)) != EOF) {
			byte_count++;
			if (c == '\n') {
				if (ferror(outstr))
					goto data_err;
				(void) putc('\r', outstr);
				if (isdata) {
					total_data_out++;
					total_data++;
				}
				total_bytes_out++;
				total_bytes++;
			}
			(void) putc(c, outstr);
			if (isdata) {
				total_data_out++;
				total_data++;
			}
			total_bytes_out++;
			total_bytes++;
			if ((byte_count % 4096) == 0)
				(void) alarm(curclass.timeout);
		}
		(void) alarm(0);
		fflush(outstr);
		if (ferror(instr))
			goto file_err;
		if (ferror(outstr))
			goto data_err;
		rval = 0;
		goto cleanup_send_data;

	case TYPE_I:
	case TYPE_L:
		if ((buf = malloc((size_t)blksize)) == NULL) {
			perror_reply(451, "Local resource failure: malloc");
			goto cleanup_send_data;
		}
		filefd = fileno(instr);
		netfd = fileno(outstr);
		(void) alarm(curclass.timeout);
		if (curclass.rateget) {
			while (1) {
				int d;
				struct timeval then, now, td;
				off_t bufrem;
				char *bufp;

				(void)gettimeofday(&then, NULL);
				errno = c = d = 0;
				bufrem = curclass.rateget;
				while (bufrem > 0) {
					if ((c = read(filefd, buf,
					    MIN(blksize, bufrem))) <= 0)
						goto senddone;
					(void) alarm(curclass.timeout);
					bufrem -= c;
					byte_count += c;
					if (isdata) {
						total_data_out += c;
						total_data += c;
					}
					total_bytes_out += c;
					total_bytes += c;
					for (bufp = buf; c > 0;
					    c -= d, bufp += d)
						if ((d =
						    write(netfd, bufp, c)) <= 0)
							break;
					if (d < 0)
						goto data_err;
				}
				(void)gettimeofday(&now, NULL);
				timersub(&now, &then, &td);
				if (td.tv_sec == 0)
					usleep(1000000 - td.tv_usec);
			}
		} else {
			while ((c = read(filefd, buf, (size_t)blksize)) > 0) {
				if (write(netfd, buf, c) != c)
					goto data_err;
				(void) alarm(curclass.timeout);
				byte_count += c;
				if (isdata) {
					total_data_out += c;
					total_data += c;
				}
				total_bytes_out += c;
				total_bytes += c;
			}
		}
 senddone:
		if (c < 0)
			goto file_err;
		rval = 0;
		goto cleanup_send_data;

	default:
		reply(550, "Unimplemented TYPE %d in send_data", type);
		goto cleanup_send_data;
	}

 data_err:
	(void) alarm(0);
	perror_reply(426, "Data connection");
	goto cleanup_send_data;

 file_err:
	(void) alarm(0);
	perror_reply(551, "Error on input file");
		/* FALLTHROUGH */

 cleanup_send_data:
	(void) alarm(0);
	transflag = 0;
	if (buf)
		free(buf);
	if (isdata) {
		total_files_out++;
		total_files++;
	}
	total_xfers_out++;
	total_xfers++;
	return (rval);
}

/*
 * Transfer data from peer to "outstr" using the appropriate encapulation of
 * the data subject to Mode, Structure, and Type.
 *
 * N.B.: Form isn't handled.
 */
static int
receive_data(FILE *instr, FILE *outstr)
{
	int	c, bare_lfs, netfd, filefd, rval;
	char	buf[BUFSIZ];
#ifdef __GNUC__
	(void) &bare_lfs;
#endif

	bare_lfs = 0;
	transflag = 1;
	rval = -1;
	if (setjmp(urgcatch))
		goto cleanup_recv_data;

	switch (type) {

	case TYPE_I:
	case TYPE_L:
		netfd = fileno(instr);
		filefd = fileno(outstr);
		(void) alarm(curclass.timeout);
		if (curclass.rateput) {
			while (1) {
				int d;
				struct timeval then, now, td;
				off_t bufrem;

				(void)gettimeofday(&then, NULL);
				errno = c = d = 0;
				for (bufrem = curclass.rateput; bufrem > 0; ) {
					if ((c = read(netfd, buf,
					    MIN(sizeof(buf), bufrem))) <= 0)
						goto recvdone;
					if ((d = write(filefd, buf, c)) != c)
						goto recvdone;
					(void) alarm(curclass.timeout);
					bufrem -= c;
					byte_count += c;
					total_data_in += c;
					total_data += c;
					total_bytes_in += c;
					total_bytes += c;
				}
				(void)gettimeofday(&now, NULL);
				timersub(&now, &then, &td);
				if (td.tv_sec == 0)
					usleep(1000000 - td.tv_usec);
			}
		} else {
			while ((c = read(netfd, buf, sizeof(buf))) > 0) {
				if (write(filefd, buf, c) != c)
					goto file_err;
				(void) alarm(curclass.timeout);
				byte_count += c;
				total_data_in += c;
				total_data += c;
				total_bytes_in += c;
				total_bytes += c;
			}
		}
 recvdone:
		if (c < 0)
			goto data_err;
		rval = 0;
		goto cleanup_recv_data;

	case TYPE_E:
		reply(553, "TYPE E not implemented.");
		goto cleanup_recv_data;

	case TYPE_A:
		(void) alarm(curclass.timeout);
 /* XXXX: rate limit ascii receive (put) */
		while ((c = getc(instr)) != EOF) {
			byte_count++;
			total_data_in++;
			total_data++;
			total_bytes_in++;
			total_bytes++;
			if ((byte_count % 4096) == 0)
				(void) alarm(curclass.timeout);
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				if (ferror(outstr))
					goto data_err;
				if ((c = getc(instr)) != '\n') {
					byte_count++;
					total_data_in++;
					total_data++;
					total_bytes_in++;
					total_bytes++;
					if ((byte_count % 4096) == 0)
						(void) alarm(curclass.timeout);
					(void) putc ('\r', outstr);
					if (c == '\0' || c == EOF)
						goto contin2;
				}
			}
			(void) putc(c, outstr);
	contin2:	;
		}
		(void) alarm(0);
		fflush(outstr);
		if (ferror(instr))
			goto data_err;
		if (ferror(outstr))
			goto file_err;
		if (bare_lfs) {
			reply(-226,
			    "WARNING! %d bare linefeeds received in ASCII mode",
			    bare_lfs);
			reply(0, "File may not have transferred correctly.");
		}
		rval = 0;
		goto cleanup_recv_data;

	default:
		reply(550, "Unimplemented TYPE %d in receive_data", type);
		goto cleanup_recv_data;
	}

 data_err:
	(void) alarm(0);
	perror_reply(426, "Data Connection");
	goto cleanup_recv_data;

 file_err:
	(void) alarm(0);
	perror_reply(452, "Error writing file");
	goto cleanup_recv_data;

 cleanup_recv_data:
	(void) alarm(0);
	transflag = 0;
	total_files_in++;
	total_files++;
	total_xfers_in++;
	total_xfers++;
	return (rval);
}

void
statcmd(void)
{
	union sockunion *su = NULL;
	static char ntop_buf[INET6_ADDRSTRLEN];
  	u_char *a, *p;
	int ispassive, af;
	off_t otbi, otbo, otb;

	a = p = (u_char *)NULL;

	reply(-211, "%s FTP server status:", hostname);
	reply(0, "Version: %s", FTPD_VERSION);
	ntop_buf[0] = '\0';
	if (!getnameinfo((struct sockaddr *)&his_addr, his_addr.su_len,
			ntop_buf, sizeof(ntop_buf), NULL, 0, NI_NUMERICHOST)
	    && strcmp(remotehost, ntop_buf) != 0) {
		reply(0, "Connected to %s (%s)", remotehost, ntop_buf);
	} else
		reply(0, "Connected to %s", remotehost);
	if (logged_in) {
		if (curclass.type == CLASS_GUEST)
			reply(0, "Logged in anonymously");
		else
			reply(0, "Logged in as %s%s", pw->pw_name,
			    curclass.type == CLASS_CHROOT ? " (chroot)" : "");
	} else if (askpasswd)
		reply(0, "Waiting for password");
	else
		reply(0, "Waiting for user name");
	cprintf(stdout, "    TYPE: %s", typenames[type]);
	if (type == TYPE_A || type == TYPE_E)
		cprintf(stdout, ", FORM: %s", formnames[form]);
	if (type == TYPE_L) {
#if NBBY == 8
		cprintf(stdout, " %d", NBBY);
#else
			/* XXX: `bytesize' needs to be defined in this case */
		cprintf(stdout, " %d", bytesize);
#endif
	}
	cprintf(stdout, "; STRUcture: %s; transfer MODE: %s\r\n",
	    strunames[stru], modenames[mode]);
	ispassive = 0;
	if (data != -1) {
  		reply(0, "Data connection open");
		su = NULL;
	} else if (pdata != -1) {
		reply(0, "in Passive mode");
		su = (union sockunion *)&pasv_addr;
		ispassive = 1;
		goto printaddr;
	} else if (usedefault == 0) {
		if (epsvall) {
			reply(0, "EPSV only mode (EPSV ALL)");
			goto epsvonly;
		}
		su = (union sockunion *)&data_dest;
 printaddr:
							/* PASV/PORT */
		if (su->su_family == AF_INET) {
			a = (u_char *) &su->su_sin.sin_addr;
			p = (u_char *) &su->su_sin.sin_port;
#define UC(b) (((int) b) & 0xff)
			reply(0, "%s (%d,%d,%d,%d,%d,%d)",
				ispassive ? "PASV" : "PORT" ,
				UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				UC(p[0]), UC(p[1]));
		}

							/* LPSV/LPRT */
	    {
		int alen, af, i;

		alen = 0;
		switch (su->su_family) {
		case AF_INET:
			a = (u_char *) &su->su_sin.sin_addr;
			p = (u_char *) &su->su_sin.sin_port;
			alen = sizeof(su->su_sin.sin_addr);
			af = 4;
			break;
		case AF_INET6:
			a = (u_char *) &su->su_sin6.sin6_addr;
			p = (u_char *) &su->su_sin6.sin6_port;
			alen = sizeof(su->su_sin6.sin6_addr);
			af = 6;
			break;
		default:
			af = 0;
			break;
		}
		if (af) {
			cprintf(stdout, "    %s (%d,%d",
			    ispassive ? "LPSV" : "LPRT", af, alen);
			for (i = 0; i < alen; i++)
				cprintf(stdout, ",%d", UC(a[i]));
			cprintf(stdout, ",%d,%d,%d)", 2, UC(p[0]), UC(p[1]));
#undef UC
		}
	    }

		/* EPRT/EPSV */
 epsvonly:
		switch (su->su_family) {
		case AF_INET:
			af = 1;
			break;
		case AF_INET6:
			af = 2;
			break;
		default:
			af = 0;
			break;
		}
		if (af) {
			if (getnameinfo((struct sockaddr *)su, su->su_len,
				    ntop_buf, sizeof(ntop_buf), NULL, 0,
					NI_NUMERICHOST) == 0) {
				reply(0, "%s (|%d|%s|%d|)",
				    ispassive ? "EPSV" : "EPRT",
				    af, ntop_buf, ntohs(su->su_port));
			}
		}
	} else
		reply(0, "No data connection");

	if (logged_in) {
		reply(0, "Data sent:        %qd byte%s in %qd file%s",
		    (qdfmt_t)total_data_out, PLURAL(total_data_out),
		    (qdfmt_t)total_files_out, PLURAL(total_files_out));
		reply(0, "Data received:    %qd byte%s in %qd file%s",
		    (qdfmt_t)total_data_in, PLURAL(total_data_in),
		    (qdfmt_t)total_files_in, PLURAL(total_files_in));
		reply(0, "Total data:       %qd byte%s in %qd file%s",
		    (qdfmt_t)total_data, PLURAL(total_data),
		    (qdfmt_t)total_files, PLURAL(total_files));
	}
	otbi = total_bytes_in;
	otbo = total_bytes_out;
	otb = total_bytes;
	reply(0, "Traffic sent:     %qd byte%s in %qd transfer%s",
	    (qdfmt_t)otbo, PLURAL(otbo),
	    (qdfmt_t)total_xfers_out, PLURAL(total_xfers_out));
	reply(0, "Traffic received: %qd byte%s in %qd transfer%s",
	    (qdfmt_t)otbi, PLURAL(otbi),
	    (qdfmt_t)total_xfers_in, PLURAL(total_xfers_in));
	reply(0, "Total traffic:    %qd byte%s in %qd transfer%s",
	    (qdfmt_t)otb, PLURAL(otb),
	    (qdfmt_t)total_xfers, PLURAL(total_xfers));

	if (logged_in) {
		struct ftpconv *cp;

		reply(0, "");
		reply(0, "Class: %s, type: %s",
		    curclass.classname, CURCLASSTYPE);
		reply(0, "Check PORT/LPRT commands: %sabled",
		    curclass.checkportcmd ? "en" : "dis");
		if (curclass.display != NULL)
			reply(0, "Display file: %s", curclass.display);
		if (curclass.notify != NULL)
			reply(0, "Notify fileglob: %s", curclass.notify);
		reply(0, "Idle timeout: %d, maximum timeout: %d",
		    curclass.timeout, curclass.maxtimeout);
		reply(0, "Current connections: %d", connections);
		if (curclass.limit == -1)
			reply(0, "Maximum connections: unlimited");
		else
			reply(0, "Maximum connections: %d", curclass.limit);
		if (curclass.limitfile)
			reply(0, "Connection limit exceeded file: %s",
			    curclass.limitfile);
		if (curclass.motd != NULL)
			reply(0, "MotD file: %s", curclass.motd);
		reply(0,
	    "Modify commands (CHMOD, DELE, MKD, RMD, RNFR, UMASK): %sabled",
		    curclass.modify ? "en" : "dis");
		reply(0, "Upload commands (APPE, STOR, STOU): %sabled",
		    curclass.upload ? "en" : "dis");
		if (curclass.portmin && curclass.portmax)
			reply(0, "PASV port range: %d - %d",
			    curclass.portmin, curclass.portmax);
		if (curclass.rateget)
			reply(0, "Rate get limit: %d bytes/sec",
			    curclass.rateget);
		else
			reply(0, "Rate get limit: disabled");
		if (curclass.rateput)
			reply(0, "Rate put limit: %d bytes/sec",
			    curclass.rateput);
		else
			reply(0, "Rate put limit: disabled");
		reply(0, "Umask: %.04o", curclass.umask);
		for (cp = curclass.conversions; cp != NULL; cp=cp->next) {
			if (cp->suffix == NULL || cp->types == NULL ||
			    cp->command == NULL)
				continue;
			reply(0, "Conversion: %s [%s] disable: %s, command: %s",
			    cp->suffix, cp->types, cp->disable, cp->command);
		}
	}

	reply(211, "End of status");
}

void
fatal(const char *s)
{

	reply(451, "Error in server: %s\n", s);
	reply(221, "Closing connection due to server error.");
	dologout(0);
	/* NOTREACHED */
}

/*
 * reply() --
 *	depending on the value of n, display fmt with a trailing CRLF and
 *	prefix of:
 *	n < -1		prefix the message with abs(n) + "-"	(initial line)
 *	n == 0		prefix the message with 4 spaces	(middle lines)
 *	n >  0		prefix the message with n + " "		(final line)
 */
void
reply(int n, const char *fmt, ...)
{
	off_t b;
	va_list ap;

	va_start(ap, fmt);
	b = 0;
	if (n == 0)
		cprintf(stdout, "    ");
	else if (n < 0)
		cprintf(stdout, "%d-", -n);
	else
		cprintf(stdout, "%d ", n);
	b = vprintf(fmt, ap);
	total_bytes += b;
	total_bytes_out += b;
	cprintf(stdout, "\r\n");
	(void)fflush(stdout);
	if (debug) {
		syslog(LOG_DEBUG, "<--- %d%c", abs(n), (n < 0) ? '-' : ' ');
		vsyslog(LOG_DEBUG, fmt, ap);
	}
}

static void
dolog(struct sockaddr *who)
{
	getnameinfo(who, who->sa_len, remotehost, sizeof(remotehost), NULL,0,0);
#ifdef HASSETPROCTITLE
	snprintf(proctitle, sizeof(proctitle), "%s: connected", remotehost);
	setproctitle(proctitle);
#endif /* HASSETPROCTITLE */
	if (logging)
		syslog(LOG_INFO, "connection from %s to %s",
		    remotehost, hostname);
}

/*
 * Record logout in wtmp file
 * and exit with supplied status.
 */
void
dologout(int status)
{
	/*
	* Prevent reception of SIGURG from resulting in a resumption
	* back to the main program loop.
	*/
	transflag = 0;

	if (logged_in) {
		(void) seteuid((uid_t)0);
		logwtmp(ttyline, "", "");
		if (doutmp)
			logout(utmp.ut_line);
#ifdef KERBEROS
		if (!notickets && krbtkfile_env)
			unlink(krbtkfile_env);
#endif
	}
	/* beware of flushing buffers after a SIGPIPE */
	_exit(status);
}

static void
myoob(int signo)
{
	char *cp;

	/* only process if transfer occurring */
	if (!transflag)
		return;
	cp = tmpline;
	if (getline(cp, 7, stdin) == NULL) {
		reply(221, "You could at least say goodbye.");
		dologout(0);
	}
	if (strcasecmp(cp, "ABOR\r\n") == 0) {
		tmpline[0] = '\0';
		reply(426, "Transfer aborted. Data connection closed.");
		reply(226, "Abort successful");
		longjmp(urgcatch, 1);
	}
	if (strcasecmp(cp, "STAT\r\n") == 0) {
		tmpline[0] = '\0';
		if (file_size != (off_t) -1)
			reply(213, "Status: %qd of %qd byte%s transferred",
			    (qdfmt_t)byte_count, (qdfmt_t)file_size,
			    PLURAL(byte_count));
		else
			reply(213, "Status: %qd byte%s transferred",
			    (qdfmt_t)byte_count, PLURAL(byte_count));
	}
}

static int
bind_pasv_addr(void)
{
	static int passiveport;
	int port, len;

	len = pasv_addr.su_len;
	if (curclass.portmin == 0 && curclass.portmax == 0) {
		pasv_addr.su_port = 0;
		return (bind(pdata, (struct sockaddr *)&pasv_addr, len));
	}

	if (passiveport == 0) {
		srand(getpid());
		passiveport = rand() % (curclass.portmax - curclass.portmin)
		    + curclass.portmin;
	}

	port = passiveport;
	while (1) {
		port++;
		if (port > curclass.portmax)
			port = curclass.portmin;
		else if (port == passiveport) {
			errno = EAGAIN;
			return (-1);
		}
		pasv_addr.su_port = htons(port);
		if (bind(pdata, (struct sockaddr *)&pasv_addr, len) == 0)
			break;
		if (errno != EADDRINUSE)
			return (-1);
	}
	passiveport = port;
	return (0);
}

/*
 * Note: a response of 425 is not mentioned as a possible response to
 *	the PASV command in RFC959. However, it has been blessed as
 *	a legitimate response by Jon Postel in a telephone conversation
 *	with Rick Adams on 25 Jan 89.
 */
void
passive(void)
{
	int len;
	char *p, *a;

	if (pdata >= 0)
		close(pdata);
	pdata = socket(AF_INET, SOCK_STREAM, 0);
	if (pdata < 0 || !logged_in) {
		perror_reply(425, "Can't open passive connection");
		return;
	}
	pasv_addr = ctrl_addr;

	if (bind_pasv_addr() < 0)
		goto pasv_error;
	len = pasv_addr.su_len;
	if (getsockname(pdata, (struct sockaddr *) &pasv_addr, &len) < 0)
		goto pasv_error;
	if (listen(pdata, 1) < 0)
		goto pasv_error;
	a = (char *) &pasv_addr.su_sin.sin_addr;
	p = (char *) &pasv_addr.su_port;

#define UC(b) (((int) b) & 0xff)

	reply(227, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)", UC(a[0]),
		UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
	return;

 pasv_error:
	(void) close(pdata);
	pdata = -1;
	perror_reply(425, "Can't open passive connection");
	return;
}

/*
 * 228 Entering Long Passive Mode (af, hal, h1, h2, h3,..., pal, p1, p2...)
 * 229 Entering Extended Passive Mode (|||port|)
 */
void
long_passive(char *cmd, int pf)
{
	int len;
	char *p, *a;

	if (!logged_in) {
		syslog(LOG_NOTICE, "long passive but not logged in");
		reply(503, "Login with USER first.");
		return;
	}

	if (pf != PF_UNSPEC) {
		if (ctrl_addr.su_family != pf) {
			switch (ctrl_addr.su_family) {
			case AF_INET:
				pf = 1;
				break;
			case AF_INET6:
				pf = 2;
				break;
			default:
				pf = 0;
				break;
			}
			/*
			 * XXX
			 * only EPRT/EPSV ready clients will understand this
			 */
			if (strcmp(cmd, "EPSV") == 0 && pf) {
				reply(522, "Network protocol mismatch, "
					    "use (%d)", pf);
			} else
				reply(501, "Network protocol mismatch"); /*XXX*/

			return;
		}
	}
 
	if (pdata >= 0)
		close(pdata);
	pdata = socket(ctrl_addr.su_family, SOCK_STREAM, 0);
	if (pdata < 0) {
		perror_reply(425, "Can't open passive connection");
		return;
	}
	pasv_addr = ctrl_addr;
	if (bind_pasv_addr() < 0)
		goto pasv_error;
	len = pasv_addr.su_len;
	if (getsockname(pdata, (struct sockaddr *) &pasv_addr, &len) < 0)
		goto pasv_error;
	if (listen(pdata, 1) < 0)
		goto pasv_error;
	p = (char *) &pasv_addr.su_port;

#define UC(b) (((int) b) & 0xff)

	if (strcmp(cmd, "LPSV") == 0) {
		switch (pasv_addr.su_family) {
		case AF_INET:
			a = (char *) &pasv_addr.su_sin.sin_addr;
			reply(228, "Entering Long Passive Mode (%d,%d,%d,%d,%d,%d,%d,%d,%d)",
				4, 4, UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				2, UC(p[0]), UC(p[1]));
			return;
		case AF_INET6:
			a = (char *) &pasv_addr.su_sin6.sin6_addr;
			reply(228, "Entering Long Passive Mode (%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)",
				6, 16,
				UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
				UC(a[4]), UC(a[5]), UC(a[6]), UC(a[7]),
				UC(a[8]), UC(a[9]), UC(a[10]), UC(a[11]),
				UC(a[12]), UC(a[13]), UC(a[14]), UC(a[15]),
				2, UC(p[0]), UC(p[1]));
			return;
		}
#undef UC
	} else if (strcmp(cmd, "EPSV") == 0) {
		switch (pasv_addr.su_family) {
		case AF_INET:
		case AF_INET6:
			reply(229, "Entering Extended Passive Mode (|||%d|)",
			ntohs(pasv_addr.su_port));
			return;
		}
	} else {
		/* more proper error code? */
	}

 pasv_error:
	(void) close(pdata);
	pdata = -1;
	perror_reply(425, "Can't open passive connection");
	return;
}

/*
 * Generate unique name for file with basename "local".
 * The file named "local" is already known to exist.
 * Generates failure reply on error.
 *
 * XXX this function should under go changes similar to
 * the mktemp(3)/mkstemp(3) changes.
 */
static char *
gunique(const char *local)
{
	static char new[MAXPATHLEN];
	struct stat st;
	char *cp;
	int count;

	cp = strrchr(local, '/');
	if (cp)
		*cp = '\0';
	if (stat(cp ? local : ".", &st) < 0) {
		perror_reply(553, cp ? local : ".");
		return (NULL);
	}
	if (cp)
		*cp = '/';
	for (count = 1; count < 100; count++) {
		(void)snprintf(new, sizeof(new) - 1, "%s.%d", local, count);
		if (stat(new, &st) < 0)
			return (new);
	}
	reply(452, "Unique file name cannot be created.");
	return (NULL);
}

/*
 * Format and send reply containing system error number.
 */
void
perror_reply(int code, const char *string)
{
	int save_errno;

	save_errno = errno;
	reply(code, "%s: %s.", string, strerror(errno));
	errno = save_errno;
}

static char *onefile[] = {
	"",
	0
};

void
send_file_list(const char *whichf)
{
	struct stat st;
	DIR *dirp = NULL;
	struct dirent *dir;
	FILE *dout = NULL;
	char **dirlist, *dirname, *p;
	int simple = 0;
	int freeglob = 0;
	glob_t gl;
	off_t b;

#ifdef __GNUC__
	(void) &dout;
	(void) &dirlist;
	(void) &simple;
	(void) &freeglob;
#endif

	p = NULL;
	if (strpbrk(whichf, "~{[*?") != NULL) {
		int flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_TILDE;

		memset(&gl, 0, sizeof(gl));
		freeglob = 1;
		if (glob(whichf, flags, 0, &gl)) {
			reply(550, "not found");
			goto out;
		} else if (gl.gl_pathc == 0) {
			errno = ENOENT;
			perror_reply(550, whichf);
			goto out;
		}
		dirlist = gl.gl_pathv;
	} else {
		p = xstrdup(whichf);
		onefile[0] = p;
		dirlist = onefile;
		simple = 1;
	}
					/* XXX: } for vi sm */

	if (setjmp(urgcatch)) {
		transflag = 0;
		goto out;
	}
	while ((dirname = *dirlist++) != NULL) {
		int trailingslash = 0;

		if (stat(dirname, &st) < 0) {
			/*
			 * If user typed "ls -l", etc, and the client
			 * used NLST, do what the user meant.
			 */
			/* XXX: nuke this support? */
			if (dirname[0] == '-' && *dirlist == NULL &&
			    transflag == 0) {
				char *argv[] = { INTERNAL_LS, "", NULL };

				argv[1] = dirname;
				retrieve(argv, dirname);
				goto out;
			}
			perror_reply(550, whichf);
			goto cleanup_send_file_list;
		}

		if (S_ISREG(st.st_mode)) {
			if (dout == NULL) {
				dout = dataconn("file list", (off_t)-1, "w");
				if (dout == NULL)
					goto out;
				transflag++;
			}
			b = fprintf(dout, "%s%s\n", dirname,
			    type == TYPE_A ? "\r" : "");
			total_bytes += b;
			total_bytes_out += b;
			byte_count += strlen(dirname) + 1;
			continue;
		} else if (!S_ISDIR(st.st_mode))
			continue;

		if (dirname[strlen(dirname) - 1] == '/')
			trailingslash++;

		if ((dirp = opendir(dirname)) == NULL)
			continue;

		while ((dir = readdir(dirp)) != NULL) {
			char nbuf[MAXPATHLEN];

			if (ISDOTDIR(dir->d_name) || ISDOTDOTDIR(dir->d_name))
				continue;

			(void)snprintf(nbuf, sizeof(nbuf), "%s%s%s", dirname,
			    trailingslash ? "" : "/", dir->d_name);

			/*
			 * We have to do a stat to ensure it's
			 * not a directory or special file.
			 */
			/* XXX: follow RFC959 and filter out non files ? */
			if (simple || (stat(nbuf, &st) == 0 &&
			    S_ISREG(st.st_mode))) {
				char *p;

				if (dout == NULL) {
					dout = dataconn("file list", (off_t)-1,
						"w");
					if (dout == NULL)
						goto out;
					transflag++;
				}
				p = nbuf;
				if (nbuf[0] == '.' && nbuf[1] == '/')
					p = &nbuf[2];
				b = fprintf(dout, "%s%s\n", p,
				    type == TYPE_A ? "\r" : "");
				total_bytes += b;
				total_bytes_out += b;
				byte_count += strlen(nbuf) + 1;
			}
		}
		(void) closedir(dirp);
	}

	if (dout == NULL)
		reply(550, "No files found.");
	else if (ferror(dout) != 0)
		perror_reply(550, "Data connection");
	else
		reply(226, "Transfer complete.");

 cleanup_send_file_list:
	transflag = 0;
	closedataconn(dout);
 out:
	total_xfers++;
	total_xfers_out++;
	if (p)
		free(p);
	if (freeglob)
		globfree(&gl);
}

char *
conffilename(const char *s)
{
	static char filename[MAXPATHLEN];

	if (*s == '/')
		strlcpy(filename, s, sizeof(filename));
	else
		(void)snprintf(filename, sizeof(filename), "%s/%s", confdir ,s);
	return (filename);
}

/*
 * logcmd --
 *	based on the arguments, syslog a message:
 *	 if bytes != -1		"<command> <file1> = <bytes> bytes"
 *	 else if file2 != NULL	"<command> <file1> <file2>"
 *	 else			"<command> <file1>"
 *	if elapsed != NULL, append "in xxx.yyy seconds"
 *	if error != NULL, append ": " + error
 */
void
logcmd(const char *command, off_t bytes, const char *file1, const char *file2,
	const struct timeval *elapsed, const char *error)
{
	char	buf[MAXPATHLEN * 2 + 100], realfile[MAXPATHLEN];
	const char *p;
	size_t	len;

	if (logging <=1)
		return;

	if ((p = realpath(file1, realfile)) == NULL) {
#if 0	/* XXX: too noisy */
		syslog(LOG_WARNING, "realpath `%s' failed: %s",
		    realfile, strerror(errno));
#endif
		p = file1;
	}
	len = snprintf(buf, sizeof(buf), "%s %s", command, p);

	if (bytes != (off_t)-1) {
		len += snprintf(buf + len, sizeof(buf) - len,
		    " = %qd byte%s", (qdfmt_t) bytes, PLURAL(bytes));
	} else if (file2 != NULL) {
		if ((p = realpath(file2, realfile)) == NULL) {
#if 0	/* XXX: too noisy */
			syslog(LOG_WARNING, "realpath `%s' failed: %s",
			    realfile, strerror(errno));
#endif
			p = file2;
		}
		len += snprintf(buf + len, sizeof(buf) - len, " %s", p);
	}

	if (elapsed != NULL) {
		len += snprintf(buf + len, sizeof(buf) - len,
		    " in %ld.%.03d seconds", elapsed->tv_sec,
		    (int)(elapsed->tv_usec / 1000));
	}

	if (error != NULL)
		len += snprintf(buf + len, sizeof(buf) - len, ": %s", error);

	syslog(LOG_INFO, "%s", buf);
}

char *
xstrdup(const char *s)
{
	char *new = strdup(s);

	if (new == NULL)
		fatal("Local resource failure: malloc");
		/* NOTREACHED */
	return (new);
}

/*
 * As per fprintf(), but increment total_bytes and total_bytes_out,
 * by the appropriate amount.
 */
void
cprintf(FILE *fd, const char *fmt, ...)
{
	off_t b;
	va_list ap;

	va_start(ap, fmt);
	b = vfprintf(fd, fmt, ap);
	total_bytes += b;
	total_bytes_out += b;
}
