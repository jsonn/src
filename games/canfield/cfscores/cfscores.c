/*	$NetBSD: cfscores.c,v 1.14.6.1 2008/09/18 04:39:56 wrstuden Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1983, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)cfscores.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: cfscores.c,v 1.14.6.1 2008/09/18 04:39:56 wrstuden Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <err.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"

struct betinfo {
	long	hand;		/* cost of dealing hand */
	long	inspection;	/* cost of inspecting hand */
	long	game;		/* cost of buying game */
	long	runs;		/* cost of running through hands */
	long	information;	/* cost of information */
	long	thinktime;	/* cost of thinking time */
	long	wins;		/* total winnings */
	long	worth;		/* net worth after costs */
};

int dbfd;

void	printuser(const struct passwd *, int);

int
main(int argc, char *argv[])
{
	struct passwd *pw;
	uid_t uid;

	/* Revoke setgid privileges */
	setgid(getgid());

	if (argc > 2) {
		printf("Usage: cfscores [user]\n");
		exit(1);
	}
	dbfd = open(_PATH_SCORE, O_RDONLY);
	if (dbfd < 0)
		err(2, "open %s", _PATH_SCORE);
	setpwent();
	if (argc == 1) {
		uid = getuid();
		pw = getpwuid(uid);
		if (pw == 0) {
			printf("You are not listed in the password file?!?\n");
			exit(2);
		}
		printuser(pw, 1);
		exit(0);
	}
	if (strcmp(argv[1], "-a") == 0) {
		while ((pw = getpwent()) != 0)
			printuser(pw, 0);
		exit(0);
	}
	pw = getpwnam(argv[1]);
	if (pw == 0) {
		printf("User %s unknown\n", argv[1]);
		exit(3);
	}
	printuser(pw, 1);
	exit(0);
}

/*
 * print out info for specified password entry
 */
void
printuser(const struct passwd *pw, int printfail)
{
	struct betinfo total;
	off_t pos;
	int i;

	pos = pw->pw_uid * (off_t)sizeof(struct betinfo);
	/* test pos, not pw_uid; uid_t can be unsigned, which makes gcc warn */
	if (pos < 0) {
		printf("Bad uid %d\n", (int)pw->pw_uid);
		return;
	}
	i = lseek(dbfd, pos, SEEK_SET);
	if (i < 0)
		warn("lseek %s", _PATH_SCORE);
	i = read(dbfd, (char *)&total, sizeof(total));
	if (i < 0)
		warn("read %s", _PATH_SCORE);
	if (i == 0 || total.hand == 0) {
		if (printfail)
			printf("%s has never played canfield.\n", pw->pw_name);
		return;
	}
	printf("*----------------------*\n");
	if (total.worth >= 0)
		printf("* Winnings for %-8s*\n", pw->pw_name);
	else
		printf("* Losses for %-10s*\n", pw->pw_name);
	printf("*======================*\n");
	printf("|Costs           Total |\n");
	printf("| Hands       %8ld |\n", total.hand);
	printf("| Inspections %8ld |\n", total.inspection);
	printf("| Games       %8ld |\n", total.game);
	printf("| Runs        %8ld |\n", total.runs);
	printf("| Information %8ld |\n", total.information);
	printf("| Think time  %8ld |\n", total.thinktime);
	printf("|Total Costs  %8ld |\n", total.wins - total.worth);
	printf("|Winnings     %8ld |\n", total.wins);
	printf("|Net Worth    %8ld |\n", total.worth);
	printf("*----------------------*\n\n");
}
