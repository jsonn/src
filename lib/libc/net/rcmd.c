/*	$NetBSD: rcmd.c,v 1.19.4.2 1997/02/08 04:38:31 mycroft Exp $	*/

/*
 * Copyright (c) 1983, 1993, 1994
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

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)rcmd.c	8.3 (Berkeley) 3/26/94";
#else
static char *rcsid = "$NetBSD: rcmd.c,v 1.19.4.2 1997/02/08 04:38:31 mycroft Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/poll.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netgroup.h>

#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>

int	__ivaliduser __P((FILE *, u_int32_t, const char *, const char *));
static int __icheckhost __P((u_int32_t, const char *));
static char *__gethostloop __P((u_int32_t));

int
rcmd(ahost, rport, locuser, remuser, cmd, fd2p)
	char **ahost;
	u_short rport;
	const char *locuser, *remuser, *cmd;
	int *fd2p;
{
	struct hostent *hp;
	struct sockaddr_in sin, from;
	struct pollfd reads[2];
	int oldmask;
	pid_t pid;
	int s, lport, timo;
	char c;

	pid = getpid();
	hp = gethostbyname(*ahost);
	if (hp == NULL) {
		herror(*ahost);
		return (-1);
	}
	*ahost = hp->h_name;
	oldmask = sigblock(sigmask(SIGURG));
	for (timo = 1, lport = IPPORT_RESERVED - 1;;) {
		s = rresvport(&lport);
		if (s < 0) {
			if (errno == EAGAIN)
				(void)fprintf(stderr,
				    "rcmd: socket: All ports in use\n");
			else
				(void)fprintf(stderr, "rcmd: socket: %s\n",
				    strerror(errno));
			sigsetmask(oldmask);
			return (-1);
		}
		fcntl(s, F_SETOWN, pid);
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_family = hp->h_addrtype;
		sin.sin_port = rport;
		bcopy(hp->h_addr_list[0], &sin.sin_addr, hp->h_length);
		if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
			break;
		(void)close(s);
		if (errno == EADDRINUSE) {
			lport--;
			continue;
		}
		if (errno == ECONNREFUSED && timo <= 16) {
			(void)sleep(timo);
			timo *= 2;
			continue;
		}
		if (hp->h_addr_list[1] != NULL) {
			int oerrno = errno;

			(void)fprintf(stderr, "connect to address %s: ",
			    inet_ntoa(sin.sin_addr));
			errno = oerrno;
			perror(0);
			hp->h_addr_list++;
			bcopy(hp->h_addr_list[0], &sin.sin_addr, hp->h_length);
			(void)fprintf(stderr, "Trying %s...\n",
			    inet_ntoa(sin.sin_addr));
			continue;
		}
		(void)fprintf(stderr, "%s: %s\n", hp->h_name, strerror(errno));
		sigsetmask(oldmask);
		return (-1);
	}
	lport--;
	if (fd2p == 0) {
		write(s, "", 1);
		lport = 0;
	} else {
		char num[8];
		int s2 = rresvport(&lport), s3;
		int len = sizeof(from);

		if (s2 < 0)
			goto bad;
		listen(s2, 1);
		(void)snprintf(num, sizeof(num), "%d", lport);
		if (write(s, num, strlen(num)+1) != strlen(num)+1) {
			(void)fprintf(stderr,
			    "rcmd: write (setting up stderr): %s\n",
			    strerror(errno));
			(void)close(s2);
			goto bad;
		}
		reads[0].fd = s;
		reads[0].events = POLLIN;
		reads[1].fd = s2;
		reads[1].events = POLLIN;
		errno = 0;
		if (poll(reads, 2, INFTIM) < 1 ||
		    (reads[1].revents & POLLIN) == 0) {
			if (errno != 0)
				(void)fprintf(stderr,
				    "rcmd: poll (setting up stderr): %s\n",
				    strerror(errno));
			else
				(void)fprintf(stderr,
				"poll: protocol failure in circuit setup\n");
			(void)close(s2);
			goto bad;
		}
		s3 = accept(s2, (struct sockaddr *)&from, &len);
		(void)close(s2);
		if (s3 < 0) {
			(void)fprintf(stderr,
			    "rcmd: accept: %s\n", strerror(errno));
			lport = 0;
			goto bad;
		}
		*fd2p = s3;
		from.sin_port = ntohs(from.sin_port);
		if (from.sin_family != AF_INET ||
		    from.sin_port >= IPPORT_RESERVED ||
		    from.sin_port < IPPORT_RESERVED / 2) {
			(void)fprintf(stderr,
			    "socket: protocol failure in circuit setup.\n");
			goto bad2;
		}
	}
	(void)write(s, locuser, strlen(locuser)+1);
	(void)write(s, remuser, strlen(remuser)+1);
	(void)write(s, cmd, strlen(cmd)+1);
	if (read(s, &c, 1) != 1) {
		(void)fprintf(stderr,
		    "rcmd: %s: %s\n", *ahost, strerror(errno));
		goto bad2;
	}
	if (c != 0) {
		while (read(s, &c, 1) == 1) {
			(void)write(STDERR_FILENO, &c, 1);
			if (c == '\n')
				break;
		}
		goto bad2;
	}
	sigsetmask(oldmask);
	return (s);
bad2:
	if (lport)
		(void)close(*fd2p);
bad:
	(void)close(s);
	sigsetmask(oldmask);
	return (-1);
}

int
rresvport(alport)
	int *alport;
{
	struct sockaddr_in sin;
	int s;

	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return (-1);
	for (;;) {
		sin.sin_port = htons((u_short)*alport);
		if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
			return (s);
		if (errno != EADDRINUSE) {
			(void)close(s);
			return (-1);
		}
		(*alport)--;
		if (*alport == IPPORT_RESERVED/2) {
			(void)close(s);
			errno = EAGAIN;		/* close */
			return (-1);
		}
	}
}

int	__check_rhosts_file = 1;
char	*__rcmd_errstr;

int
ruserok(rhost, superuser, ruser, luser)
	const char *rhost, *ruser, *luser;
	int superuser;
{
	struct hostent *hp;
	char **ap;
	int i;
#define MAXADDRS	35
	u_int32_t addrs[MAXADDRS + 1];

	if ((hp = gethostbyname(rhost)) == NULL)
		return (-1);
	for (i = 0, ap = hp->h_addr_list; *ap && i < MAXADDRS; ++ap, ++i)
		bcopy(*ap, &addrs[i], sizeof(addrs[i]));
	addrs[i] = 0;

	for (i = 0; i < MAXADDRS && addrs[i]; i++)
		if (iruserok(addrs[i], superuser, ruser, luser) == 0)
			return (0);
	return (-1);
}

/*
 * New .rhosts strategy: We are passed an ip address. We spin through
 * hosts.equiv and .rhosts looking for a match. When the .rhosts only
 * has ip addresses, we don't have to trust a nameserver.  When it
 * contains hostnames, we spin through the list of addresses the nameserver
 * gives us and look for a match.
 *
 * Returns 0 if ok, -1 if not ok.
 */
int
iruserok(raddr, superuser, ruser, luser)
	u_int32_t raddr;
	int superuser;
	const char *ruser, *luser;
{
	register char *cp;
	struct stat sbuf;
	struct passwd *pwd;
	FILE *hostf;
	uid_t uid;
	gid_t gid;
	int first;
	char pbuf[MAXPATHLEN];

	first = 1;
	hostf = superuser ? NULL : fopen(_PATH_HEQUIV, "r");
again:
	if (hostf) {
		if (__ivaliduser(hostf, raddr, luser, ruser) == 0) {
			(void)fclose(hostf);
			return (0);
		}
		(void)fclose(hostf);
	}
	if (first == 1 && (__check_rhosts_file || superuser)) {
		first = 0;
		if ((pwd = getpwnam(luser)) == NULL)
			return (-1);
		(void)strncpy(pbuf, pwd->pw_dir, sizeof(pbuf) - 1);
		(void)strncat(pbuf, "/.rhosts", sizeof(pbuf) - strlen(pbuf) - 1);

		/*
		 * Change effective uid while opening .rhosts.  If root and
		 * reading an NFS mounted file system, can't read files that
		 * are protected read/write owner only.
		 */
		uid = geteuid();
		gid = getegid();
		(void)setegid(pwd->pw_gid);
		initgroups(pwd->pw_name, pwd->pw_gid);
		(void)seteuid(pwd->pw_uid);
		hostf = fopen(pbuf, "r");
		(void)seteuid(uid);
		(void)setegid(gid);

		if (hostf == NULL)
			return (-1);
		/*
		 * If not a regular file, or is owned by someone other than
		 * user or root or if writeable by anyone but the owner, quit.
		 */
		cp = NULL;
		if (lstat(pbuf, &sbuf) < 0)
			cp = ".rhosts lstat failed";
		else if (!S_ISREG(sbuf.st_mode))
			cp = ".rhosts not regular file";
		else if (fstat(fileno(hostf), &sbuf) < 0)
			cp = ".rhosts fstat failed";
		else if (sbuf.st_uid && sbuf.st_uid != pwd->pw_uid)
			cp = "bad .rhosts owner";
		else if (sbuf.st_mode & (S_IWGRP|S_IWOTH))
			cp = ".rhosts writeable by other than owner";
		/* If there were any problems, quit. */
		if (cp) {
			__rcmd_errstr = cp;
			(void)fclose(hostf);
			return (-1);
		}
		goto again;
	}
	return (-1);
}

/*
 * XXX
 * Don't make static, used by lpd(8).
 *
 * Returns 0 if ok, -1 if not ok.
 */
int
__ivaliduser(hostf, raddr, luser, ruser)
	FILE *hostf;
	u_int32_t raddr;
	const char *luser, *ruser;
{
	register char *user, *p;
	int ch;
	char buf[MAXHOSTNAMELEN + 128];		/* host + login */
	const char *auser, *ahost;
	int hostok, userok;
	char *rhost = NULL;
	int firsttime = 1;
	char domain[MAXHOSTNAMELEN];

	getdomainname(domain, sizeof(domain));

	while (fgets(buf, sizeof(buf), hostf)) {
		p = buf;
		/* Skip lines that are too long. */
		if (strchr(p, '\n') == NULL) {
			while ((ch = getc(hostf)) != '\n' && ch != EOF)
				;
			continue;
		}
		while (*p != '\n' && *p != ' ' && *p != '\t' && *p != '\0') {
			*p = isupper(*p) ? tolower(*p) : *p;
			p++;
		}
		if (*p == ' ' || *p == '\t') {
			*p++ = '\0';
			while (*p == ' ' || *p == '\t')
				p++;
			user = p;
			while (*p != '\n' && *p != ' ' &&
			    *p != '\t' && *p != '\0')
				p++;
		} else
			user = p;
		*p = '\0';

		if (p == buf)
			continue;

		auser = *user ? user : luser;
		ahost = buf;

		if (ahost[0] == '+')
			switch (ahost[1]) {
			case '\0':
				hostok = 1;
				break;

			case '@':
				if (firsttime) {
					rhost = __gethostloop(raddr);
					firsttime = 0;
				}
				if (rhost)
					hostok = innetgr(&ahost[2], rhost,
					    NULL, domain);
				else
					hostok = 0;
				break;

			default:
				hostok = __icheckhost(raddr, &ahost[1]);
				break;
			}
		else if (ahost[0] == '-')
			switch (ahost[1]) {
			case '\0':
				hostok = -1;
				break;

			case '@':
				if (firsttime) {
					rhost = __gethostloop(raddr);
					firsttime = 0;
				}
				if (rhost)
					hostok = -innetgr(&ahost[2], rhost,
					    NULL, domain);
				else
					hostok = 0;
				break;

			default:
				hostok = -__icheckhost(raddr, &ahost[1]);
				break;
			}
		else
			hostok = __icheckhost(raddr, ahost);


		if (auser[0] == '+')
			switch (auser[1]) {
			case '\0':
				userok = 1;
				break;

			case '@':
				userok = innetgr(&auser[2], NULL, ruser,
				    domain);
				break;

			default:
				userok = strcmp(ruser, &auser[1]) == 0;
				break;
			}
		else if (auser[0] == '-')
			switch (auser[1]) {
			case '\0':
				userok = -1;
				break;

			case '@':
				userok = -innetgr(&auser[2], NULL, ruser,
				    domain);
				break;

			default:
				userok = -(strcmp(ruser, &auser[1]) == 0);
				break;
			}
		else
			userok = strcmp(ruser, auser) == 0;

		/* Check if one component did not match */
		if (hostok == 0 || userok == 0)
			continue;

		/* Check if we got a forbidden pair */
		if (userok == -1 || hostok == -1)
			return -1;

		/* Check if we got a valid pair */
		if (hostok == 1 && userok == 1)
			return 0;
	}
	return -1;
}

/*
 * Returns "true" if match, 0 if no match.
 */
static int
__icheckhost(raddr, lhost)
	u_int32_t raddr;
	const char *lhost;
{
	register struct hostent *hp;
	register u_int32_t laddr;
	register char **pp;

	/* Try for raw ip address first. */
	if (isdigit(*lhost) && (int32_t)(laddr = inet_addr(lhost)) != -1)
		return (raddr == laddr);

	/* Better be a hostname. */
	if ((hp = gethostbyname(lhost)) == NULL)
		return (0);

	/* Spin through ip addresses. */
	for (pp = hp->h_addr_list; *pp; ++pp)
		if (!bcmp(&raddr, *pp, sizeof(u_int32_t)))
			return (1);

	/* No match. */
	return (0);
}

/*
 * Return the hostname associated with the supplied address.
 * Do a reverse lookup as well for security. If a loop cannot
 * be found, pack the result of inet_ntoa() into the string.
 */
static char *
__gethostloop(raddr)
	u_int32_t raddr;
{
	static char remotehost[MAXHOSTNAMELEN];
	struct hostent *hp;
	struct in_addr in;

	hp = gethostbyaddr((char *) &raddr, sizeof(raddr), AF_INET);
	if (hp == NULL)
		return (NULL);

	/*
	 * Look up the name and check that the supplied
	 * address is in the list
	 */
	strncpy(remotehost, hp->h_name, sizeof(remotehost) - 1);
	remotehost[sizeof(remotehost) - 1] = '\0';
	hp = gethostbyname(remotehost);
	if (hp == NULL)
		return (NULL);

	for (; hp->h_addr_list[0] != NULL; hp->h_addr_list++)
		if (!bcmp(hp->h_addr_list[0], (caddr_t)&raddr, sizeof(raddr)))
			return (remotehost);

	/*
	 * either the DNS adminstrator has made a configuration
	 * mistake, or someone has attempted to spoof us
	 */
	in.s_addr = raddr;
	syslog(LOG_NOTICE, "rcmd: address %s not listed for host %s",
	    inet_ntoa(in), hp->h_name);
	return (NULL);
}
