/*	$NetBSD: main.c,v 1.4.2.2 1997/11/18 01:25:41 mellon Exp $	*/

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char *rcsid = "from FreeBSD Id: main.c,v 1.14 1997/10/08 07:47:26 charnier Exp";
#else
__RCSID("$NetBSD: main.c,v 1.4.2.2 1997/11/18 01:25:41 mellon Exp $");
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
#include "lib.h"
#include "info.h"

static char Options[] = "acdDe:fikrRpLqImvhl:";

int	Flags		= 0;
Boolean AllInstalled	= FALSE;
Boolean Quiet		= FALSE;
char *InfoPrefix	= "";
char PlayPen[FILENAME_MAX];
char *CheckPkg		= NULL;

static void usage __P((void));

int
main(int argc, char **argv)
{
    int ch;
    char **pkgs, **start;

    pkgs = start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1)
	switch(ch) {
	case 'a':
	    AllInstalled = TRUE;
	    break;

	case 'v':
	    Verbose = TRUE;
	    /* Reasonable definition of 'everything' */
	    Flags = SHOW_COMMENT | SHOW_DESC | SHOW_PLIST | SHOW_INSTALL |
		SHOW_DEINSTALL | SHOW_REQUIRE | SHOW_DISPLAY | SHOW_MTREE;
	    break;

	case 'I':
	    Flags |= SHOW_INDEX;
	    break;

	case 'p':
	    Flags |= SHOW_PREFIX;
	    break;

	case 'c':
	    Flags |= SHOW_COMMENT;
	    break;

	case 'd':
	    Flags |= SHOW_DESC;
	    break;

	case 'D':
	    Flags |= SHOW_DISPLAY;
	    break;

	case 'f':
	    Flags |= SHOW_PLIST;
	    break;

	case 'i':
	    Flags |= SHOW_INSTALL;
	    break;

	case 'k':
	    Flags |= SHOW_DEINSTALL;
	    break;

	case 'r':
	    Flags |= SHOW_REQUIRE;
	    break;

	case 'R':
	    Flags |= SHOW_REQBY;
	    break;

	case 'L':
	    Flags |= SHOW_FILES;
	    break;

	case 'm':
	    Flags |= SHOW_MTREE;
	    break;

	case 'l':
	    InfoPrefix = optarg;
	    break;

	case 'q':
	    Quiet = TRUE;
	    break;

	case 't':
	    strcpy(PlayPen, optarg);
	    break;

	case 'e':
	    CheckPkg = optarg;
	    break;

	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}

    argc -= optind;
    argv += optind;

    if (argc == 0 && !Flags) {
	/* No argument or flags specified - assume -Ia */
	Flags = SHOW_INDEX;
	AllInstalled = TRUE;
    }

    /* Set some reasonable defaults */
    if (!Flags)
	Flags = SHOW_COMMENT | SHOW_DESC | SHOW_REQBY;

    /* Get all the remaining package names, if any */
    while (*argv)
	*pkgs++ = *argv++;

    /* If no packages, yelp */
    if (pkgs == start && !AllInstalled && !CheckPkg)
	warnx("missing package name(s)"), usage();
    *pkgs = NULL;
    return pkg_perform(start);
}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n%s\n",
	"usage: pkg_info [-cdDikrRpLqImv] [-e package] [-l prefix]",
	"                pkg-name [pkg-name ...]",
	"       pkg_info -a [flags]");
    exit(1);
}
