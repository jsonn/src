/*	$NetBSD: lpq.c,v 1.16.10.1 2008/01/09 02:02:06 matt Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
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
__COPYRIGHT("@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#if 0
static char sccsid[] = "@(#)lpq.c	8.3 (Berkeley) 5/10/95";
#else
__RCSID("$NetBSD: lpq.c,v 1.16.10.1 2008/01/09 02:02:06 matt Exp $");
#endif
#endif /* not lint */

/*
 * Spool Queue examination program
 *
 * lpq [-a] [-l] [-Pprinter] [user...] [job...]
 *
 * -a show all non-null queues on the local machine
 * -l long output
 * -P used to identify printer as per lpr/lprm
 */

#include <sys/param.h>

#include <syslog.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <err.h>

#include "lp.h"
#include "lp.local.h"
#include "pathnames.h"

int	 requ[MAXREQUESTS];	/* job number of spool entries */
int	 requests;		/* # of spool requests */
char	*user[MAXUSERS];	/* users to process */
int	 users;			/* # of users in user array */
uid_t	uid, euid;

static void usage(void) __dead;

int
main(int argc, char *argv[])
{
	int	ch, aflag, lflag;
	char	*buf, *cp;

	setprogname(*argv);
	euid = geteuid();
	uid = getuid();
	seteuid(uid);
	if (gethostname(host, sizeof(host)))
		err(1, "lpq: gethostname");
	host[sizeof(host) - 1] = '\0';
	openlog("lpd", 0, LOG_LPR);

	aflag = lflag = 0;
	while ((ch = getopt(argc, argv, "alP:w:")) != -1)
		switch((char)ch) {
		case 'a':
			++aflag;
			break;
		case 'l':			/* long output */
			++lflag;
			break;
		case 'P':		/* printer name */
			printer = optarg;
			break;
		case 'w':
			wait_time = atoi(optarg);
			if (wait_time < 0)
				errx(1, "wait time must be postive: %s",
				    optarg);
			if (wait_time < 30)
			    warnx("warning: wait time less than 30 seconds");
			break;
		case '?':
		default:
			usage();
		}

	if (!aflag && printer == NULL && (printer = getenv("PRINTER")) == NULL)
		printer = DEFLP;

	for (argc -= optind, argv += optind; argc; --argc, ++argv)
		if (isdigit((unsigned char)argv[0][0])) {
			if (requests >= MAXREQUESTS)
				fatal("too many requests");
			requ[requests++] = atoi(*argv);
		}
		else {
			if (users >= MAXUSERS)
				fatal("too many users");
			user[users++] = *argv;
		}

	if (aflag) {
		while (cgetnext(&buf, printcapdb) > 0) {
			if (ckqueue(buf) <= 0) {
				free(buf);
				continue;	/* no jobs */
			}
			for (cp = buf; *cp; cp++)
				if (*cp == '|' || *cp == ':') {
					*cp = '\0';
					break;
				}
			printer = buf;
			printf("%s:\n", printer);
			displayq(lflag);
			free(buf);
			printf("\n");
		}
	} else
		displayq(lflag);
	return 0;
}

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: %s [-a] [-l] [-Pprinter] [-w maxwait] [user ...] [job ...]", getprogname());
	exit(1);
}
