/*-
 * Copyright (c) 1991, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)id.c	8.3 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: id.c,v 1.15.4.1 1999/12/27 18:37:01 wrstuden Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int	main __P((int, char **));

static void	current __P((void));
static void	pretty __P((struct passwd *));
static void	group __P((struct passwd *, int));
static void	usage __P((void));
static void	user __P((struct passwd *));
static struct passwd *who __P((char *));

static int maxgroups;
static gid_t *groups;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct group *gr;
	struct passwd *pw;
	int ch, id;
	int Gflag, gflag, nflag, pflag, rflag, uflag;
	extern const char *__progname;

	Gflag = gflag = nflag = pflag = rflag = uflag = 0;

	if (!strcmp(__progname, "groups")) {
		Gflag = 1;
		nflag = 1;
	} else if (!strcmp(__progname, "whoami")) {
		uflag = 1;
		nflag = 1;
	}

	while ((ch = getopt(argc, argv, "Ggnpru")) != -1)
		switch(ch) {
		case 'G':
			Gflag = 1;
			break;
		case 'g':
			gflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	switch(Gflag + gflag + pflag + uflag) {
	case 1:
		break;
	case 0:
		if (!nflag && !rflag)
			break;
		/* FALLTHROUGH */
	default:
		usage();
	}

	pw = *argv ? who(*argv) : NULL;

	maxgroups = sysconf(_SC_NGROUPS_MAX);
	if ((groups = malloc((maxgroups + 1) * sizeof(gid_t))) == NULL)
		err(1, NULL);

	if (gflag) {
		id = pw ? pw->pw_gid : rflag ? getgid() : getegid();
		if (nflag && (gr = getgrgid(id)))
			(void)printf("%s\n", gr->gr_name);
		else
			(void)printf("%u\n", id);
		goto done;
	}

	if (uflag) {
		id = pw ? pw->pw_uid : rflag ? getuid() : geteuid();
		if (nflag && (pw = getpwuid(id)))
			(void)printf("%s\n", pw->pw_name);
		else
			(void)printf("%u\n", id);
		goto done;
	}

	if (Gflag) {
		group(pw, nflag);
		goto done;
	}

	if (pflag) {
		pretty(pw);
		goto done;
	}

	if (pw)
		user(pw);
	else
		current();
done:
	free(groups);
	return(0);
}

static void
pretty(pw)
	struct passwd *pw;
{
	struct group *gr;
	u_int eid, rid;
	char *login;

	if (pw) {
		(void)printf("uid\t%s\n", pw->pw_name);
		(void)printf("groups\t");
		group(pw, 1);
	} else {
		if ((login = getlogin()) == NULL)
			err(1, "getlogin");

		pw = getpwuid(rid = getuid());
		if (pw == NULL || strcmp(login, pw->pw_name))
			(void)printf("login\t%s\n", login);
		if (pw)
			(void)printf("uid\t%s\n", pw->pw_name);
		else
			(void)printf("uid\t%u\n", rid);
		
		if ((eid = geteuid()) != rid) {
			if ((pw = getpwuid(eid)) != NULL)
				(void)printf("euid\t%s", pw->pw_name);
			else
				(void)printf("euid\t%u", eid);
		}
		if ((rid = getgid()) != (eid = getegid())) {
			if ((gr = getgrgid(rid)) != NULL)
				(void)printf("rgid\t%s\n", gr->gr_name);
			else
				(void)printf("rgid\t%u\n", rid);
		}
		(void)printf("groups\t");
		group(NULL, 1);
	}
}

static void
current()
{
	struct group *gr;
	struct passwd *pw;
	int cnt, id, eid, lastid, ngroups;
	char *fmt;

	id = getuid();
	(void)printf("uid=%u", id);
	if ((pw = getpwuid(id)) != NULL)
		(void)printf("(%s)", pw->pw_name);
	if ((eid = geteuid()) != id) {
		(void)printf(" euid=%u", eid);
		if ((pw = getpwuid(eid)) != NULL)
			(void)printf("(%s)", pw->pw_name);
	}
	id = getgid();
	(void)printf(" gid=%u", id);
	if ((gr = getgrgid(id)) != NULL)
		(void)printf("(%s)", gr->gr_name);
	if ((eid = getegid()) != id) {
		(void)printf(" egid=%u", eid);
		if ((gr = getgrgid(eid)) != NULL)
			(void)printf("(%s)", gr->gr_name);
	}
	if ((ngroups = getgroups(maxgroups, groups)) != NULL) {
		for (fmt = " groups=%u", lastid = -1, cnt = 0; cnt < ngroups;
		    fmt = ",%u", lastid = id, cnt++) {
			id = groups[cnt];
			if (lastid == id)
				continue;
			(void)printf(fmt, id);
			if ((gr = getgrgid(id)) != NULL)
				(void)printf("(%s)", gr->gr_name);
		}
	}
	(void)printf("\n");
}

static void
user(pw)
	struct passwd *pw;
{
	struct group *gr;
	char *fmt;
	int cnt, id, lastid, ngroups;

	id = pw->pw_uid;
	(void)printf("uid=%u(%s)", id, pw->pw_name);
	(void)printf(" gid=%lu", (u_long)pw->pw_gid);
	if ((gr = getgrgid(pw->pw_gid)) != NULL)
		(void)printf("(%s)", gr->gr_name);
	ngroups = maxgroups + 1;
	(void) getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups);
	for (fmt = " groups=%u", lastid = -1, cnt = 0; cnt < ngroups;
	    fmt=",%u", lastid = id, cnt++) {
		id = groups[cnt];
		if (lastid == id)
			continue;
		(void)printf(fmt, id);
		if ((gr = getgrgid(id)) != NULL)
			(void)printf("(%s)", gr->gr_name);
	}
	(void)printf("\n");
}

static void
group(pw, nflag)
	struct passwd *pw;
	int nflag;
{
	struct group *gr;
	int cnt, id, lastid, ngroups;
	char *fmt;

	if (pw) {
		ngroups = maxgroups;
		(void) getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups);
	} else {
		groups[0] = getgid();
		ngroups = getgroups(maxgroups, groups + 1) + 1;
	}
	fmt = nflag ? "%s" : "%u";
	for (lastid = -1, cnt = 0; cnt < ngroups; ++cnt) {
		if (lastid == (id = groups[cnt]))
			continue;
		if (nflag) {
			if ((gr = getgrgid(id)) != NULL)
				(void)printf(fmt, gr->gr_name);
			else
				(void)printf(*fmt == ' ' ? " %u" : "%u",
				    id);
			fmt = " %s";
		} else {
			(void)printf(fmt, id);
			fmt = " %u";
		}
		lastid = id;
	}
	(void)printf("\n");
}

struct passwd *
who(u)
	char *u;
{
	struct passwd *pw;
	long id;
	char *ep;

	/*
	 * Translate user argument into a pw pointer.  First, try to
	 * get it as specified.  If that fails, try it as a number.
	 */
	if ((pw = getpwnam(u)) != NULL)
		return(pw);
	id = strtol(u, &ep, 10);
	if (*u && !*ep && (pw = getpwuid(id)))
		return(pw);
	errx(1, "%s: No such user", u);
	/* NOTREACHED */
	return (NULL);
}

void
usage()
{
	(void)fprintf(stderr, "usage: id [user]\n");
	(void)fprintf(stderr, "       id -G [-n] [user]\n");
	(void)fprintf(stderr, "       id -g [-nr] [user]\n");
	(void)fprintf(stderr, "       id -p\n");
	(void)fprintf(stderr, "       id -u [-nr] [user]\n");
	exit(1);
}
