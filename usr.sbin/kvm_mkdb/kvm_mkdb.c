/*	$NetBSD: kvm_mkdb.c,v 1.14 1997/10/18 08:49:30 lukem Exp $	*/

/*-
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1990, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)kvm_mkdb.c	8.3 (Berkeley) 5/4/95";
#else
__RCSID("$NetBSD: kvm_mkdb.c,v 1.14 1997/10/18 08:49:30 lukem Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

	int	main __P((int, char **));
static	void	usage __P((void));

HASHINFO openinfo = {
	4096,		/* bsize */
	128,		/* ffactor */
	1024,		/* nelem */
	2048 * 1024,	/* cachesize */
	NULL,		/* hash() */
	0		/* lorder */
};

static DB *db;
static char dbtemp[MAXPATHLEN];

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	char *p, *nlistpath, *nlistname, dbname[MAXPATHLEN];

	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	/* If the existing db file matches the currently running kernel, exit */
	if (testdb())
		exit(0);

#define	basename(cp)	((p = strrchr((cp), '/')) != NULL ? p + 1 : (cp))
	nlistpath = argc > 0 ? argv[0] : _PATH_UNIX;
	nlistname = basename(nlistpath);

	(void)snprintf(dbname, sizeof(dbname), "%s", _PATH_KVMDB);
	(void)snprintf(dbtemp, sizeof(dbtemp), "%s.tmp", _PATH_KVMDB);
	(void)umask(0);
	db = dbopen(dbtemp, O_CREAT | O_EXLOCK | O_TRUNC | O_RDWR,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, DB_HASH, &openinfo);
	if (db == NULL)
		err(1, "%s", dbtemp);
	create_knlist(nlistpath, db);
	if (db->close(db)) {
		warn("%s", dbtemp);
		db = NULL;
		punt();
	}
	db = NULL;
	if (rename(dbtemp, dbname)) {
		warn("rename %s to %s", dbtemp, dbname);
		punt();
	}
	exit(0);
}

void
usage()
{
	(void)fprintf(stderr, "usage: kvm_mkdb [file]\n");
	exit(1);
}

void
punt()
{

	if (db != NULL)
		db->close(db);
	unlink(dbtemp);
	exit(1);
}
