/*	$NetBSD: main.c,v 1.23.2.2 2002/07/21 04:41:13 lukem Exp $	*/

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char *rcsid = "from FreeBSD Id: main.c,v 1.16 1997/10/08 07:45:43 charnier Exp";
#else
__RCSID("$NetBSD: main.c,v 1.23.2.2 2002/07/21 04:41:13 lukem Exp $");
#endif
#endif

/*
 *
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the add module.
 *
 */

#include <err.h>
#include <sys/param.h>
#include "lib.h"
#include "add.h"
#include "verify.h"

static char Options[] = "hVvIRfnp:SMs:t:u";

char   *Prefix = NULL;
Boolean NoInstall = FALSE;
Boolean NoRecord = FALSE;

char   *Mode = NULL;
char   *Owner = NULL;
char   *Group = NULL;
char   *PkgName = NULL;
char   *Directory = NULL;
char    FirstPen[FILENAME_MAX];
add_mode_t AddMode = NORMAL;
int	upgrade = 0;

static void
usage(void)
{
	(void) fprintf(stderr, "%s\n%s\n",
	    "usage: pkg_add [-hVvInfRMSu] [-t template] [-p prefix]",
	    "               [-s verification-type] pkg-name [pkg-name ...]");
	exit(1);
}

int
main(int argc, char **argv)
{
	int     ch, error=0;
	lpkg_head_t pkgs;

	while ((ch = getopt(argc, argv, Options)) != -1) {
		switch (ch) {
		case 'v':
			Verbose = TRUE;
			break;

		case 'p':
			Prefix = optarg;
			break;

		case 'I':
			NoInstall = TRUE;
			break;

		case 'R':
			NoRecord = TRUE;
			break;

		case 'f':
			Force = TRUE;
			break;

		case 'n':
			Fake = TRUE;
			Verbose = TRUE;
			break;

		case 's':
			set_verification(optarg);
			break;

		case 't':
			strcpy(FirstPen, optarg);
			break;

		case 'S':
			AddMode = SLAVE;
			break;

		case 'M':
			AddMode = MASTER;
			break;

		case 'V':
			show_version();
			/* NOTREACHED */

		case 'u':
			upgrade = 1;
			break;
		case 'h':
		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	path_create(getenv("PKG_PATH"));
	TAILQ_INIT(&pkgs);

	if (AddMode != SLAVE) {
		/* Get all the remaining package names, if any */
		for (ch = 0; *argv; ch++, argv++) {
			lpkg_t *lpp;

			if (IS_STDIN(*argv))
				lpp = alloc_lpkg("-");
			else
				lpp = alloc_lpkg(*argv);

			TAILQ_INSERT_TAIL(&pkgs, lpp, lp_link);
		}
	}
	/* If no packages, yelp */
	else if (!ch)
		warnx("missing package name(s)"), usage();
	else if (ch > 1 && AddMode == MASTER)
		warnx("only one package name may be specified with master mode"),
		    usage();
	error += pkg_perform(&pkgs);
	if (error  != 0) {
		if (Verbose)
			warnx("%d package addition(s) failed", error);
		exit(1);
	}
	exit(0);
}
