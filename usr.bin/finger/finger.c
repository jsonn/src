/*	$NetBSD: finger.c,v 1.13 1998/12/19 15:59:50 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
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
 * Luke Mewburn <lukem@netbsd.org> added the following on 961121:
 *    - mail status ("No Mail", "Mail read:...", or "New Mail ...,
 *	Unread since ...".)
 *    - 4 digit phone extensions (3210 is printed as x3210.)
 *    - host/office toggling in short format with -h & -o.
 *    - short day names (`Tue' printed instead of `Jun 21' if the
 *	login time is < 6 days.
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)finger.c	8.5 (Berkeley) 5/4/95";
#else
__RCSID("$NetBSD: finger.c,v 1.13 1998/12/19 15:59:50 christos Exp $");
#endif
#endif /* not lint */

/*
 * Finger prints out information about users.  It is not portable since
 * certain fields (e.g. the full user name, office, and phone numbers) are
 * extracted from the gecos field of the passwd file which other UNIXes
 * may not have or may use for other things.
 *
 * There are currently two output formats; the short format is one line
 * per user and displays login name, tty, login time, real name, idle time,
 * and either remote host information (default) or office location/phone
 * number, depending on if -h or -o is used respectively.
 * The long format gives the same information (in a more legible format) as
 * well as home directory, shell, mail info, and .plan/.project files.
 */

#include <sys/param.h>

#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include "finger.h"
#include "extern.h"

DB *db;
time_t now;
int entries, gflag, lflag, mflag, oflag, sflag, pplan;
char tbuf[1024];

static void loginlist __P((void));
static void userlist __P((int, char **));
int main __P((int, char **));

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;

	oflag = 1;		/* default to old "office" behavior */

	while ((ch = getopt(argc, argv, "lmpshog")) != -1)
		switch(ch) {
		case 'l':
			lflag = 1;		/* long format */
			break;
		case 'm':
			mflag = 1;		/* force exact match of names */
			break;
		case 'p':
			pplan = 1;		/* don't show .plan/.project */
			break;
		case 's':
			sflag = 1;		/* short format */
			break;
		case 'h':
			oflag = 0;		/* remote host info */
			break;
		case 'o':
			oflag = 1;		/* office info */
			break;
		case 'g':
			gflag = 1;		/* no gecos info, besides name */
			break;
		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: finger [-lmpsho] [login ...]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	(void)time(&now);
	setpassent(1);
	if (!*argv) {
		/*
		 * Assign explicit "small" format if no names given and -l
		 * not selected.  Force the -s BEFORE we get names so proper
		 * screening will be done.
		 */
		if (!lflag)
			sflag = 1;	/* if -l not explicit, force -s */
		loginlist();
		if (entries == 0)
			(void)printf("No one logged on.\n");
	} else {
		userlist(argc, argv);
		/*
		 * Assign explicit "large" format if names given and -s not
		 * explicitly stated.  Force the -l AFTER we get names so any
		 * remote finger attempts specified won't be mishandled.
		 */
		if (!sflag)
			lflag = 1;	/* if -s not explicit, force -l */
	}
	if (entries) {
		if (lflag)
			lflag_print();
		else
			sflag_print();
	}
	return (0);
}

static void
loginlist()
{
	PERSON *pn;
	DBT data, key;
	struct passwd *pw;
	struct utmp user;
	int r, sflag;
	char name[UT_NAMESIZE + 1];

	if (!freopen(_PATH_UTMP, "r", stdin))
		err(1, "cant read %s", _PATH_UTMP);
	name[UT_NAMESIZE] = '\0';
	while (fread((char *)&user, sizeof(user), 1, stdin) == 1) {
		if (!user.ut_name[0])
			continue;
		if ((pn = find_person(user.ut_name)) == NULL) {
			memcpy(name, user.ut_name, UT_NAMESIZE);
			if ((pw = getpwnam(name)) == NULL)
				continue;
			pn = enter_person(pw);
		}
		enter_where(&user, pn);
	}
	if (db && lflag)
		for (sflag = R_FIRST;; sflag = R_NEXT) {
			PERSON *tmp;

			r = (*db->seq)(db, &key, &data, sflag);
			if (r == -1)
				err(1, "db seq");
			if (r == 1)
				break;
			memmove(&tmp, data.data, sizeof tmp);
			enter_lastlog(tmp);
		}
}

static void
userlist(argc, argv)
	int argc;
	char **argv;
{
	register PERSON *pn;
	DBT data, key;
	struct utmp user;
	struct passwd *pw;
	int r, sflag, *used, *ip;
	char **ap, **nargv, **np, **p;

	if ((nargv = malloc((argc+1) * sizeof(char *))) == NULL ||
	    (used = calloc(argc, sizeof(int))) == NULL)
#ifdef __GNUC__
		err(1, "%s", "");	/* XXX gcc */
#else
		err(1, NULL);
#endif

	/* Pull out all network requests. */
	for (ap = p = argv, np = nargv; *p; ++p)
		if (strchr(*p, '@'))
			*np++ = *p;
		else
			*ap++ = *p;

	*np++ = NULL;
	*ap++ = NULL;

	if (!*argv)
		goto net;

	/*
	 * Traverse the list of possible login names and check the login name
	 * and real name against the name specified by the user.
	 */
	if (mflag) {
		for (p = argv; *p; ++p)
			if ((pw = getpwnam(*p)) != NULL)
				enter_person(pw);
			else
				(void)fprintf(stderr,
				    "finger: %s: no such user\n", *p);
	} else {
		while ((pw = getpwent()) != NULL)
			for (p = argv, ip = used; *p; ++p, ++ip)
				if (match(pw, *p)) {
					enter_person(pw);
					*ip = 1;
				}
		for (p = argv, ip = used; *p; ++p, ++ip)
			if (!*ip)
				(void)fprintf(stderr,
				    "finger: %s: no such user\n", *p);
	}

	/* Handle network requests. */
net:
	for (p = nargv; *p;)
		netfinger(*p++);

	if (entries == 0)
		return;

	/*
	 * Scan thru the list of users currently logged in, saving
	 * appropriate data whenever a match occurs.
	 */
	if (!freopen(_PATH_UTMP, "r", stdin))
		err(1, "%s", _PATH_UTMP);
	while (fread((char *)&user, sizeof(user), 1, stdin) == 1) {
		if (!user.ut_name[0])
			continue;
		if ((pn = find_person(user.ut_name)) == NULL)
			continue;
		enter_where(&user, pn);
	}
	if (db)
		for (sflag = R_FIRST;; sflag = R_NEXT) {
			PERSON *tmp;

			r = (*db->seq)(db, &key, &data, sflag);
			if (r == -1)
				err(1, "db seq");
			if (r == 1)
				break;
			memmove(&tmp, data.data, sizeof tmp);
			enter_lastlog(tmp);
		}
}
