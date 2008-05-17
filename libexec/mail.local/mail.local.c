/*	$NetBSD: mail.local.c,v 1.23.2.1 2008/05/17 16:30:33 bouyer Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
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
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n");
#if 0
static char sccsid[] = "@(#)mail.local.c	8.22 (Berkeley) 6/21/95";
#else
__RCSID("$NetBSD: mail.local.c,v 1.23.2.1 2008/05/17 16:30:33 bouyer Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <sysexits.h>


#include "pathnames.h"

int	deliver __P((int, char *, int));
void	logerr __P((int, const char *, ...))
     __attribute__((__format__(__printf__, 2, 3)));
void	logwarn __P((const char *, ...))
     __attribute__((__format__(__printf__, 1, 2)));
void	notifybiff __P((char *));
int	store __P((const char *));
void	usage __P((void));
int	main __P((int, char **));

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct passwd *pw;
	int ch, fd, eval, lockfile = 0;
	uid_t uid;
	const char *from;

	/* use a reasonable umask */
	(void) umask(0077);

	openlog("mail.local", LOG_PERROR, LOG_MAIL);

	from = NULL;
	while ((ch = getopt(argc, argv, "ldf:r:")) != -1)
		switch (ch) {
		case 'd':		/* backward compatible */
			break;
		case 'f':
		case 'r':		/* backward compatible */
			if (from)
				logerr(EX_USAGE, "multiple -f options");
			from = optarg;
			break;
		case 'l':
			lockfile++;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!*argv)
		usage();

	/*
	 * If from not specified, use the name from getlogin() if the
	 * uid matches, otherwise, use the name from the password file
	 * corresponding to the uid.
	 */
	uid = getuid();
	if (!from && (!(from = getlogin()) ||
	    !(pw = getpwnam(from)) || pw->pw_uid != uid))
		from = (pw = getpwuid(uid)) ? pw->pw_name : "???";

	fd = store(from);
	for (eval = EX_OK; *argv; ++argv) {
		int rval;

		rval = deliver(fd, *argv, lockfile);
		if (eval == EX_OK && rval != EX_OK)
			eval = rval;
	}
	exit (eval);
}

int
store(from)
	const char *from;
{
	FILE *fp = NULL;	/* XXX gcc */
	time_t tval;
	int fd, eline;
	char *tn, line[2048];

	tn = strdup(_PATH_LOCTMP);
	if (!tn)
		logerr(EX_OSERR, "not enough core");
	if ((fd = mkstemp(tn)) == -1 || !(fp = fdopen(fd, "w+")))
		logerr(EX_OSERR, "unable to open temporary file");
	(void)unlink(tn);
	free(tn);

	(void)time(&tval);
	(void)fprintf(fp, "From %s %s", from, ctime(&tval));

	line[0] = '\0';
	for (eline = 1; fgets(line, sizeof(line), stdin);) {
		if (line[0] == '\n')
			eline = 1;
		else {
			if (eline && line[0] == 'F' && !memcmp(line, "From ", 5))
				(void)putc('>', fp);
			eline = 0;
		}
		(void)fprintf(fp, "%s", line);
		if (ferror(fp))
			break;
	}

	/* If message not newline terminated, need an extra. */
	if (!index(line, '\n'))
		(void)putc('\n', fp);
	/* Output a newline; note, empty messages are allowed. */
	(void)putc('\n', fp);

	(void)fflush(fp);
	if (ferror(fp))
		logerr(EX_OSERR, "temporary file write error");
	fd = dup(fd);
	(void)fclose(fp);
	return(fd);
}

int
deliver(fd, name, lockfile)
	int fd;
	char *name;
	int lockfile;
{
	struct stat sb;
	struct passwd pwres, *pw;
	char pwbuf[1024];
	int created, mbfd, nr, nw, off, rval=EX_OK, lfd=-1;
	char biffmsg[100], buf[8*1024], path[MAXPATHLEN], lpath[MAXPATHLEN];
	off_t curoff;

	/*
	 * Disallow delivery to unknown names -- special mailboxes can be
	 * handled in the sendmail aliases file.
	 */
	if ((getpwnam_r(name, &pwres, pwbuf, sizeof(pwbuf), &pw)) != 0) {
		logwarn("unable to find user %s: %s", name, strerror(errno));
		return(EX_TEMPFAIL);
	}
	if (pw == NULL) {
		logwarn("unknown name: %s", name);
		return(EX_NOUSER);
	}

	(void)snprintf(path, sizeof path, "%s/%s", _PATH_MAILDIR, name);

	if (lockfile) {
		(void)snprintf(lpath, sizeof lpath, "%s/%s.lock",
		    _PATH_MAILDIR, name);

		if((lfd = open(lpath, O_CREAT|O_WRONLY|O_EXCL,
		    S_IRUSR|S_IWUSR)) < 0) {
			logwarn("%s: %s", lpath, strerror(errno));
			return(EX_OSERR);
		}
	}

	if (!(created = lstat(path, &sb)) &&
	    (sb.st_nlink != 1 || S_ISLNK(sb.st_mode))) {
		logwarn("%s: linked file", path);
		return(EX_OSERR);
	}
	
	if ((mbfd = open(path, O_APPEND|O_WRONLY|O_EXLOCK,
	    S_IRUSR|S_IWUSR)) < 0) {
		if ((mbfd = open(path, O_APPEND|O_CREAT|O_WRONLY|O_EXLOCK,
		    S_IRUSR|S_IWUSR)) < 0) {
			logwarn("%s: %s", path, strerror(errno));
			return(EX_OSERR);
		}
	}

	curoff = lseek(mbfd, 0, SEEK_END);
	(void)snprintf(biffmsg, sizeof biffmsg, "%s@%lld\n", name,
	    (long long)curoff);
	if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
		logwarn("temporary file: %s", strerror(errno));
		rval = EX_OSERR;
		goto bad;
	}

	while ((nr = read(fd, buf, sizeof(buf))) > 0)
		for (off = 0; off < nr;  off += nw)
			if ((nw = write(mbfd, buf + off, nr - off)) < 0) {
				logwarn("%s: %s", path, strerror(errno));
				goto trunc;
			}
	if (nr < 0) {
		logwarn("temporary file: %s", strerror(errno));
trunc:		(void)ftruncate(mbfd, curoff);
		rval = EX_OSERR;
	}

	/*
	 * Set the owner and group.  Historically, binmail repeated this at
	 * each mail delivery.  We no longer do this, assuming that if the
	 * ownership or permissions were changed there was a reason for doing
	 * so.
	 */
bad:
	if (lockfile) {
		if (lfd >= 0) {
			unlink(lpath);
			close(lfd);
		}
	}
	if (created) 
		(void)fchown(mbfd, pw->pw_uid, pw->pw_gid);

	(void)fsync(mbfd);		/* Don't wait for update. */
	(void)close(mbfd);		/* Implicit unlock. */

	if (rval == EX_OK)
		notifybiff(biffmsg);

	return rval;
}

void
notifybiff(msg)
	char *msg;
{
	static struct sockaddr_in addr;
	static int f = -1;
	struct hostent *hp;
	struct servent *sp;
	int len;

	if (!addr.sin_family) {
		/* Be silent if biff service not available. */
		if (!(sp = getservbyname("biff", "udp")))
			return;
		if (!(hp = gethostbyname("localhost"))) {
			logwarn("localhost: %s", strerror(errno));
			return;
		}
		addr.sin_len = sizeof(struct sockaddr_in);
		addr.sin_family = hp->h_addrtype;
		addr.sin_port = sp->s_port;
		memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);
	}
	if (f < 0 && (f = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		logwarn("socket: %s", strerror(errno));
		return;
	}
	len = strlen(msg) + 1;
	if (sendto(f, msg, len, 0, (struct sockaddr *)&addr, sizeof(addr))
	    != len)
		logwarn("sendto biff: %s", strerror(errno));
}

void
usage()
{
	logerr(EX_USAGE, "usage: mail.local [-l] [-f from] user ...");
}

void
logerr(int status, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);

	exit(status);
	/* NOTREACHED */
	return;
}

void
logwarn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);
	return;
}
