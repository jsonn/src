/*	$NetBSD: rcp.c,v 1.25 1999/02/17 06:45:38 scottr Exp $	*/

/*
 * Copyright (c) 1983, 1990, 1992, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1983, 1990, 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)rcp.c	8.2 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: rcp.c,v 1.25 1999/02/17 06:45:38 scottr Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"
#include "extern.h"

#ifdef KERBEROS
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#include "krb.h"

char	dst_realm_buf[REALM_SZ];
char	*dest_realm = NULL;
int	use_kerberos = 1;
CREDENTIALS 	cred;
Key_schedule	schedule;
#ifdef CRYPT
int	doencrypt = 0;
#define	OPTIONS	"dfKk:prtx"
#else
#define	OPTIONS	"dfKk:prt"
#endif
#else
#define	OPTIONS "dfprt"
#endif

struct passwd *pwd;
char *pwname;
u_short	port;
uid_t	userid;
int errs, rem;
int pflag, iamremote, iamrecursive, targetshouldbedirectory;

#define	CMDNEEDS	64
char cmd[CMDNEEDS];		/* must hold "rcp -r -p -d\0" */

#ifdef KERBEROS
int	 kerberos __P((char **, char *, char *, char *));
void	 oldw __P((const char *, ...));
#endif
int	 response __P((void));
void	 rsource __P((char *, struct stat *));
void	 sink __P((int, char *[]));
void	 source __P((int, char *[]));
void	 tolocal __P((int, char *[]));
void	 toremote __P((char *, int, char *[]));
void	 usage __P((void));
int	 main __P((int, char *[]));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct servent *sp;
	int ch, fflag, tflag;
	char *targ, *shell;

	fflag = tflag = 0;
	while ((ch = getopt(argc, argv, OPTIONS)) != -1)
		switch(ch) {			/* User-visible flags. */
		case 'K':
#ifdef KERBEROS
			use_kerberos = 0;
#endif
			break;
#ifdef	KERBEROS
		case 'k':
			dest_realm = dst_realm_buf;
			(void)strncpy(dst_realm_buf, optarg, REALM_SZ);
			break;
#ifdef CRYPT
		case 'x':
			doencrypt = 1;
			/* des_set_key(cred.session, schedule); */
			break;
#endif
#endif
		case 'p':
			pflag = 1;
			break;
		case 'r':
			iamrecursive = 1;
			break;
						/* Server options. */
		case 'd':
			targetshouldbedirectory = 1;
			break;
		case 'f':			/* "from" */
			iamremote = 1;
			fflag = 1;
			break;
		case 't':			/* "to" */
			iamremote = 1;
			tflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#ifdef KERBEROS
	if (use_kerberos) {
#ifdef CRYPT
		shell = doencrypt ? "ekshell" : "kshell";
#else
		shell = "kshell";
#endif
		if ((sp = getservbyname(shell, "tcp")) == NULL) {
			use_kerberos = 0;
			oldw("can't get entry for %s/tcp service", shell);
			sp = getservbyname(shell = "shell", "tcp");
		}
	} else
		sp = getservbyname(shell = "shell", "tcp");
#else
	sp = getservbyname(shell = "shell", "tcp");
#endif
	if (sp == NULL)
		errx(1, "%s/tcp: unknown service", shell);
	port = sp->s_port;

	if ((pwd = getpwuid(userid = getuid())) == NULL)
		errx(1, "unknown user %d", (int)userid);

	if ((pwname = strdup(pwd->pw_name)) == NULL)
		err(1, "%s", "");

	rem = STDIN_FILENO;		/* XXX */

	if (fflag) {			/* Follow "protocol", send data. */
		(void)response();
		source(argc, argv);
		exit(errs);
	}

	if (tflag) {			/* Receive data. */
		sink(argc, argv);
		exit(errs);
	}

	if (argc < 2)
		usage();
	if (argc > 2)
		targetshouldbedirectory = 1;

	rem = -1;
	/* Command to be executed on remote system using "rsh". */
#ifdef	KERBEROS
	(void)snprintf(cmd, sizeof(cmd),
	    "rcp%s%s%s%s", iamrecursive ? " -r" : "",
#ifdef CRYPT
	    (doencrypt && use_kerberos ? " -x" : ""),
#else
	    "",
#endif
	    pflag ? " -p" : "", targetshouldbedirectory ? " -d" : "");
#else
	(void)snprintf(cmd, sizeof(cmd), "rcp%s%s%s",
	    iamrecursive ? " -r" : "", pflag ? " -p" : "",
	    targetshouldbedirectory ? " -d" : "");
#endif

	(void)signal(SIGPIPE, lostconn);

	if ((targ = colon(argv[argc - 1])) != NULL)/* Dest is remote host. */
		toremote(targ, argc, argv);
	else {
		tolocal(argc, argv);		/* Dest is local host. */
		if (targetshouldbedirectory)
			verifydir(argv[argc - 1]);
	}
	exit(errs);
	/* NOTREACHED */
}

void
toremote(targ, argc, argv)
	char *targ, *argv[];
	int argc;
{
	int i, len;
	char *bp, *host, *src, *suser, *thost, *tuser;

	*targ++ = 0;
	if (*targ == 0)
		targ = ".";

	if ((thost = strchr(argv[argc - 1], '@')) != NULL) {
		/* user@host */
		*thost++ = 0;
		tuser = argv[argc - 1];
		if (*tuser == '\0')
			tuser = NULL;
		else if (!okname(tuser))
			exit(1);
	} else {
		thost = argv[argc - 1];
		tuser = NULL;
	}

	for (i = 0; i < argc - 1; i++) {
		src = colon(argv[i]);
		if (src) {			/* remote to remote */
			*src++ = 0;
			if (*src == 0)
				src = ".";
			host = strchr(argv[i], '@');
			len = strlen(_PATH_RSH) + strlen(argv[i]) +
			    strlen(src) + (tuser ? strlen(tuser) : 0) +
			    strlen(thost) + strlen(targ) + CMDNEEDS + 20;
			if (!(bp = malloc(len)))
				err(1, "%s", "");
			if (host) {
				*host++ = 0;
				suser = argv[i];
				if (*suser == '\0')
					suser = pwname;
				else if (!okname(suser))
					continue;
				(void)snprintf(bp, len,
				    "%s %s -l %s -n %s %s '%s%s%s:%s'",
				    _PATH_RSH, host, suser, cmd, src,
				    tuser ? tuser : "", tuser ? "@" : "",
				    thost, targ);
			} else
				(void)snprintf(bp, len,
				    "exec %s %s -n %s %s '%s%s%s:%s'",
				    _PATH_RSH, argv[i], cmd, src,
				    tuser ? tuser : "", tuser ? "@" : "",
				    thost, targ);
			(void)susystem(bp);
			(void)free(bp);
		} else {			/* local to remote */
			if (rem == -1) {
				len = strlen(targ) + CMDNEEDS + 20;
				if (!(bp = malloc(len)))
					err(1, "%s", "");
				(void)snprintf(bp, len, "%s -t %s", cmd, targ);
				host = thost;
#ifdef KERBEROS
				if (use_kerberos)
					rem = kerberos(&host, bp, pwname,
					    tuser ? tuser : pwname);
				else
#endif
					rem = rcmd(&host, port, pwname,
					    tuser ? tuser : pwname,
					    bp, 0);
				if (rem < 0)
					exit(1);
				if (response() < 0)
					exit(1);
				(void)free(bp);
			}
			source(1, argv+i);
		}
	}
}

void
tolocal(argc, argv)
	int argc;
	char *argv[];
{
	int i, len;
	char *bp, *host, *src, *suser;

	for (i = 0; i < argc - 1; i++) {
		if (!(src = colon(argv[i]))) {		/* Local to local. */
			len = strlen(_PATH_CP) + strlen(argv[i]) +
			    strlen(argv[argc - 1]) + 20;
			if (!(bp = malloc(len)))
				err(1, "%s", "");
			(void)snprintf(bp, len, "exec %s%s%s %s %s", _PATH_CP,
			    iamrecursive ? " -r" : "", pflag ? " -p" : "",
			    argv[i], argv[argc - 1]);
			if (susystem(bp))
				++errs;
			(void)free(bp);
			continue;
		}
		*src++ = 0;
		if (*src == 0)
			src = ".";
		if ((host = strchr(argv[i], '@')) == NULL) {
			host = argv[i];
			suser = pwname;
		} else {
			*host++ = 0;
			suser = argv[i];
			if (*suser == '\0')
				suser = pwname;
			else if (!okname(suser))
				continue;
		}
		len = strlen(src) + CMDNEEDS + 20;
		if ((bp = malloc(len)) == NULL)
			err(1, "%s", "");
		(void)snprintf(bp, len, "%s -f %s", cmd, src);
		rem = 
#ifdef KERBEROS
		    use_kerberos ? 
			kerberos(&host, bp, pwname, suser) : 
#endif
			rcmd(&host, port, pwname, suser, bp, 0);
		(void)free(bp);
		if (rem < 0) {
			++errs;
			continue;
		}
		sink(1, argv + argc - 1);
		(void)close(rem);
		rem = -1;
	}
}

void
source(argc, argv)
	int argc;
	char *argv[];
{
	struct stat stb;
	static BUF buffer;
	BUF *bp;
	off_t i;
	int amt, fd, haderr, indx, result;
	char *last, *name, buf[BUFSIZ];

#ifdef __GNUC__
	/* This outrageous construct just to shut up a GCC warning. */
	(void) &i;
#endif

	for (indx = 0; indx < argc; ++indx) {
                name = argv[indx];
		if ((fd = open(name, O_RDONLY, 0)) < 0)
			goto syserr;
		if (fstat(fd, &stb)) {
syserr:			run_err("%s: %s", name, strerror(errno));
			goto next;
		}
		switch (stb.st_mode & S_IFMT) {
		case S_IFREG:
			break;
		case S_IFDIR:
			if (iamrecursive) {
				rsource(name, &stb);
				goto next;
			}
			/* FALLTHROUGH */
		default:
			run_err("%s: not a regular file", name);
			goto next;
		}
		if ((last = strrchr(name, '/')) == NULL)
			last = name;
		else
			++last;
		if (pflag) {
			/*
			 * Make it compatible with possible future
			 * versions expecting microseconds.
			 */
			(void)snprintf(buf, sizeof(buf), "T%ld %ld %ld %ld\n",
			    (long)stb.st_mtimespec.tv_sec,
			    (long)stb.st_mtimespec.tv_nsec / 1000,
			    (long)stb.st_atimespec.tv_sec,
			    (long)stb.st_atimespec.tv_nsec / 1000);
			(void)write(rem, buf, strlen(buf));
			if (response() < 0)
				goto next;
		}
#define	RCPMODEMASK	(S_ISUID|S_ISGID|S_ISTXT|S_IRWXU|S_IRWXG|S_IRWXO)
		(void)snprintf(buf, sizeof(buf), "C%04o %qd %s\n",
		    stb.st_mode & RCPMODEMASK, (long long)stb.st_size, last);
		(void)write(rem, buf, strlen(buf));
		if (response() < 0)
			goto next;
		if ((bp = allocbuf(&buffer, fd, BUFSIZ)) == NULL) {
next:			(void)close(fd);
			continue;
		}

		/* Keep writing after an error so that we stay sync'd up. */
		for (haderr = i = 0; i < stb.st_size; i += bp->cnt) {
			amt = bp->cnt;
			if (i + amt > stb.st_size)
				amt = stb.st_size - i;
			if (!haderr) {
				result = read(fd, bp->buf, amt);
				if (result != amt)
					haderr = result >= 0 ? EIO : errno;
			}
			if (haderr)
				(void)write(rem, bp->buf, amt);
			else {
				result = write(rem, bp->buf, amt);
				if (result != amt)
					haderr = result >= 0 ? EIO : errno;
			}
		}
		if (close(fd) && !haderr)
			haderr = errno;
		if (!haderr)
			(void)write(rem, "", 1);
		else
			run_err("%s: %s", name, strerror(haderr));
		(void)response();
	}
}

void
rsource(name, statp)
	char *name;
	struct stat *statp;
{
	DIR *dirp;
	struct dirent *dp;
	char *last, *vect[1], path[MAXPATHLEN];

	if (!(dirp = opendir(name))) {
		run_err("%s: %s", name, strerror(errno));
		return;
	}
	last = strrchr(name, '/');
	if (last == 0)
		last = name;
	else
		last++;
	if (pflag) {
		(void)snprintf(path, sizeof(path), "T%ld %ld %ld %ld\n",
		    (long)statp->st_mtimespec.tv_sec,
		    (long)statp->st_mtimespec.tv_nsec / 1000,
		    (long)statp->st_atimespec.tv_sec,
		    (long)statp->st_atimespec.tv_nsec / 1000);
		(void)write(rem, path, strlen(path));
		if (response() < 0) {
			closedir(dirp);
			return;
		}
	}
	(void)snprintf(path, sizeof(path),
	    "D%04o %d %s\n", statp->st_mode & RCPMODEMASK, 0, last);
	(void)write(rem, path, strlen(path));
	if (response() < 0) {
		closedir(dirp);
		return;
	}
	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_ino == 0)
			continue;
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(name) + 1 + strlen(dp->d_name) >= MAXPATHLEN - 1) {
			run_err("%s/%s: name too long", name, dp->d_name);
			continue;
		}
		(void)snprintf(path, sizeof(path), "%s/%s", name, dp->d_name);
		vect[0] = path;
		source(1, vect);
	}
	(void)closedir(dirp);
	(void)write(rem, "E\n", 2);
	(void)response();
}

void
sink(argc, argv)
	int argc;
	char *argv[];
{
	static BUF buffer;
	struct stat stb;
	struct timeval tv[2];
	enum { YES, NO, DISPLAYED } wrerr;
	BUF *bp;
	off_t i, j;
	int amt, count, exists, first, mask, mode, ofd, omode;
	int setimes, size, targisdir;
	int wrerrno = 0;	/* pacify gcc */
	char ch, *cp, *np, *targ, *why, *vect[1], buf[BUFSIZ];

#define	atime	tv[0]
#define	mtime	tv[1]
#define	SCREWUP(str)	{ why = str; goto screwup; }

#ifdef __GNUC__
	/* This outrageous construct just to shut up a GCC warning. */
	(void) &i;
#endif

	setimes = targisdir = 0;
	mask = umask(0);
	if (!pflag)
		(void)umask(mask);
	if (argc != 1) {
		run_err("ambiguous target");
		exit(1);
	}
	targ = *argv;
	if (targetshouldbedirectory)
		verifydir(targ);
	(void)write(rem, "", 1);
	if (stat(targ, &stb) == 0 && S_ISDIR(stb.st_mode))
		targisdir = 1;
	for (first = 1;; first = 0) {
		cp = buf;
		if (read(rem, cp, 1) <= 0)
			return;
		if (*cp++ == '\n')
			SCREWUP("unexpected <newline>");
		do {
			if (read(rem, &ch, sizeof(ch)) != sizeof(ch))
				SCREWUP("lost connection");
			*cp++ = ch;
		} while (cp < &buf[BUFSIZ - 1] && ch != '\n');
		*cp = 0;

		if (buf[0] == '\01' || buf[0] == '\02') {
			if (iamremote == 0)
				(void)write(STDERR_FILENO,
				    buf + 1, strlen(buf + 1));
			if (buf[0] == '\02')
				exit(1);
			++errs;
			continue;
		}
		if (buf[0] == 'E') {
			(void)write(rem, "", 1);
			return;
		}

		if (ch == '\n')
			*--cp = 0;

#define getnum(t) (t) = 0; while (isdigit((unsigned char)*cp)) (t) = (t) * 10 + (*cp++ - '0');
		cp = buf;
		if (*cp == 'T') {
			setimes++;
			cp++;
			getnum(mtime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("mtime.sec not delimited");
			getnum(mtime.tv_usec);
			if (*cp++ != ' ')
				SCREWUP("mtime.usec not delimited");
			getnum(atime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("atime.sec not delimited");
			getnum(atime.tv_usec);
			if (*cp++ != '\0')
				SCREWUP("atime.usec not delimited");
			(void)write(rem, "", 1);
			continue;
		}
		if (*cp != 'C' && *cp != 'D') {
			/*
			 * Check for the case "rcp remote:foo\* local:bar".
			 * In this case, the line "No match." can be returned
			 * by the shell before the rcp command on the remote is
			 * executed so the ^Aerror_message convention isn't
			 * followed.
			 */
			if (first) {
				run_err("%s", cp);
				exit(1);
			}
			SCREWUP("expected control record");
		}
		mode = 0;
		for (++cp; cp < buf + 5; cp++) {
			if (*cp < '0' || *cp > '7')
				SCREWUP("bad mode");
			mode = (mode << 3) | (*cp - '0');
		}
		if (*cp++ != ' ')
			SCREWUP("mode not delimited");

		for (size = 0; isdigit((unsigned char)*cp);)
			size = size * 10 + (*cp++ - '0');
		if (*cp++ != ' ')
			SCREWUP("size not delimited");
		if (targisdir) {
			static char *namebuf;
			static int cursize;
			size_t need;

			need = strlen(targ) + strlen(cp) + 250;
			if (need > cursize) {
				if (!(namebuf = malloc(need)))
					run_err("%s", strerror(errno));
			}
			(void)snprintf(namebuf, need, "%s%s%s", targ,
			    *targ ? "/" : "", cp);
			np = namebuf;
		} else
			np = targ;
		exists = stat(np, &stb) == 0;
		if (buf[0] == 'D') {
			int mod_flag = pflag;
			if (exists) {
				if (!S_ISDIR(stb.st_mode)) {
					errno = ENOTDIR;
					goto bad;
				}
				if (pflag)
					(void)chmod(np, mode);
			} else {
				/* Handle copying from a read-only directory */
				mod_flag = 1;
				if (mkdir(np, mode | S_IRWXU) < 0)
					goto bad;
			}
			vect[0] = np;
			sink(1, vect);
			if (setimes) {
				setimes = 0;
				if (utimes(np, tv) < 0)
				    run_err("%s: set times: %s",
					np, strerror(errno));
			}
			if (mod_flag)
				(void)chmod(np, mode);
			continue;
		}
		omode = mode;
		mode |= S_IWRITE;
		if ((ofd = open(np, O_WRONLY|O_CREAT, mode)) < 0) {
bad:			run_err("%s: %s", np, strerror(errno));
			continue;
		}
		(void)write(rem, "", 1);
		if ((bp = allocbuf(&buffer, ofd, BUFSIZ)) == NULL) {
			(void)close(ofd);
			continue;
		}
		cp = bp->buf;
		wrerr = NO;
		for (count = i = 0; i < size; i += BUFSIZ) {
			amt = BUFSIZ;
			if (i + amt > size)
				amt = size - i;
			count += amt;
			do {
				j = read(rem, cp, amt);
				if (j <= 0) {
					run_err("%s", j ? strerror(errno) :
					    "dropped connection");
					exit(1);
				}
				amt -= j;
				cp += j;
			} while (amt > 0);
			if (count == bp->cnt) {
				/* Keep reading so we stay sync'd up. */
				if (wrerr == NO) {
					j = write(ofd, bp->buf, count);
					if (j != count) {
						wrerr = YES;
						wrerrno = j >= 0 ? EIO : errno; 
					}
				}
				count = 0;
				cp = bp->buf;
			}
		}
		if (count != 0 && wrerr == NO &&
		    (j = write(ofd, bp->buf, count)) != count) {
			wrerr = YES;
			wrerrno = j >= 0 ? EIO : errno; 
		}
		if (ftruncate(ofd, size)) {
			run_err("%s: truncate: %s", np, strerror(errno));
			wrerr = DISPLAYED;
		}
		if (pflag) {
			if (exists || omode != mode)
				if (fchmod(ofd, omode))
					run_err("%s: set mode: %s",
					    np, strerror(errno));
		} else {
			if (!exists && omode != mode)
				if (fchmod(ofd, omode & ~mask))
					run_err("%s: set mode: %s",
					    np, strerror(errno));
		}
#ifndef __SVR4
		if (setimes && wrerr == NO) {
			setimes = 0;
			if (futimes(ofd, tv) < 0) {
				run_err("%s: set times: %s",
				    np, strerror(errno));
				wrerr = DISPLAYED;
			}
		}
#endif
		(void)close(ofd);
#ifdef __SVR4
		if (setimes && wrerr == NO) {
			setimes = 0;
			if (utimes(np, tv) < 0) {
				run_err("%s: set times: %s",
				    np, strerror(errno));
				wrerr = DISPLAYED;
			}
		}
#endif
		(void)response();
		switch(wrerr) {
		case YES:
			run_err("%s: write: %s", np, strerror(wrerrno));
			break;
		case NO:
			(void)write(rem, "", 1);
			break;
		case DISPLAYED:
			break;
		}
	}
screwup:
	run_err("protocol error: %s", why);
	exit(1);
	/* NOTREACHED */
}

#ifdef KERBEROS
int
kerberos(host, bp, locuser, user)
	char **host, *bp, *locuser, *user;
{
	struct servent *sp;

again:
	if (use_kerberos) {
		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
			dest_realm = krb_realmofhost(*host);
		rem = 
#ifdef CRYPT
		    doencrypt ? 
			krcmd_mutual(host,
			    port, user, bp, 0, dest_realm, &cred, schedule) :
#endif
			krcmd(host, port, user, bp, 0, dest_realm);

		if (rem < 0) {
			use_kerberos = 0;
			if ((sp = getservbyname("shell", "tcp")) == NULL)
				errx(1, "unknown service shell/tcp");
			if (errno == ECONNREFUSED)
			    oldw("remote host doesn't support Kerberos");
			else if (errno == ENOENT)
			    oldw("can't provide Kerberos authentication data");
			port = sp->s_port;
			goto again;
		}
	} else {
#ifdef CRYPT
		if (doencrypt)
			errx(1,
			   "the -x option requires Kerberos authentication");
#endif
		rem = rcmd(host, port, locuser, user, bp, 0);
	}
	return (rem);
}
#endif /* KERBEROS */

int
response()
{
	char ch, *cp, resp, rbuf[BUFSIZ];

	if (read(rem, &resp, sizeof(resp)) != sizeof(resp))
		lostconn(0);

	cp = rbuf;
	switch(resp) {
	case 0:				/* ok */
		return (0);
	default:
		*cp++ = resp;
		/* FALLTHROUGH */
	case 1:				/* error, followed by error msg */
	case 2:				/* fatal error, "" */
		do {
			if (read(rem, &ch, sizeof(ch)) != sizeof(ch))
				lostconn(0);
			*cp++ = ch;
		} while (cp < &rbuf[BUFSIZ] && ch != '\n');

		if (!iamremote)
			(void)write(STDERR_FILENO, rbuf, cp - rbuf);
		++errs;
		if (resp == 1)
			return (-1);
		exit(1);
	}
	/* NOTREACHED */
}

void
usage()
{
#ifdef KERBEROS
#ifdef CRYPT
	(void)fprintf(stderr, "%s\n\t%s\n",
	    "usage: rcp [-Kpx] [-k realm] f1 f2",
	    "or: rcp [-Kprx] [-k realm] f1 ... fn directory");
#else
	(void)fprintf(stderr, "%s\n\t%s\n",
	    "usage: rcp [-Kp] [-k realm] f1 f2",
	    "or: rcp [-Kpr] [-k realm] f1 ... fn directory");
#endif
#else
	(void)fprintf(stderr,
	    "usage: rcp [-p] f1 f2; or: rcp [-pr] f1 ... fn directory\n");
#endif
	exit(1);
	/* NOTREACHED */
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifdef KERBEROS
void
#if __STDC__
oldw(const char *fmt, ...)
#else
oldw(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "rcp: ");
	(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, ", using standard rcp\n");
	va_end(ap);
}
#endif

void
#if __STDC__
run_err(const char *fmt, ...)
#else
run_err(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	static FILE *fp;
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	++errs;
	if (fp == NULL && !(fp = fdopen(rem, "w")))
		return;
	(void)fprintf(fp, "%c", 0x01);
	(void)fprintf(fp, "rcp: ");
	(void)vfprintf(fp, fmt, ap);
	(void)fprintf(fp, "\n");
	(void)fflush(fp);

	if (!iamremote)
		vwarnx(fmt, ap);

	va_end(ap);
}
