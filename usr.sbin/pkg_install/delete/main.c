/*	$NetBSD: main.c,v 1.5.2.1 1998/11/06 20:40:55 cgd Exp $	*/

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char *rcsid = "from FreeBSD Id: main.c,v 1.11 1997/10/08 07:46:48 charnier Exp";
#else
__RCSID("$NetBSD: main.c,v 1.5.2.1 1998/11/06 20:40:55 cgd Exp $");
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
 * This is the delete module.
 *
 */

#include <err.h>
#include "lib.h"
#include "delete.h"

static char Options[] = "hvDdnfp:";

char	*Prefix		= NULL;
Boolean	NoDeInstall	= FALSE;
Boolean	CleanDirs	= FALSE;

static void
usage(void)
{
    fprintf(stderr, "usage: pkg_delete [-vDdnf] [-p prefix] pkg-name ...\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    int ch, error;
    char **pkgs, **start;

    pkgs = start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1)
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'f':
	    Force = TRUE;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 'D':
	    NoDeInstall = TRUE;
	    break;

	case 'd':
	    CleanDirs = TRUE;
	    break;

	case 'n':
	    Fake = TRUE;
	    Verbose = TRUE;
	    break;

	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}

    argc -= optind;
    argv += optind;

    /* Get all the remaining package names, if any */
    /* Get all the remaining package names, if any */
    while (*argv)
	*pkgs++ = *argv++;

    /* If no packages, yelp */
    if (pkgs == start)
	warnx("missing package name(s)"), usage();
    *pkgs = NULL;
    if (!Fake && getuid() != 0)
	errx(1, "you must be root to delete packages");
    if ((error = pkg_perform(start)) != 0) {
	if (Verbose)
	    warnx("%d package deletion(s) failed", error);
	return error;
    }
    else
	return 0;
}
