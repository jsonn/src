/*	$NetBSD: exec.c,v 1.7.4.1 2002/02/23 18:12:15 he Exp $	*/

#include <sys/cdefs.h>
#ifndef lint
#if 0
static const char *rcsid = "from FreeBSD Id: exec.c,v 1.6 1997/10/08 07:47:50 charnier Exp";
#else
__RCSID("$NetBSD: exec.c,v 1.7.4.1 2002/02/23 18:12:15 he Exp $");
#endif
#endif

/*
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
 * Miscellaneous system routines.
 *
 */

#include <err.h>
#include "lib.h"

/*
 * Unusual system() substitute.  Accepts format string and args,
 * builds and executes command.  Returns exit code.
 */
int
vsystem(const char *fmt,...)
{
	va_list args;
	char   *cmd;
	size_t  maxargs;
	int     ret;

	maxargs = (size_t) sysconf(_SC_ARG_MAX);
	maxargs -= 32;		/* some slop for the sh -c */
	if ((cmd = (char *) malloc(maxargs)) == (char *) NULL) {
		warnx("vsystem can't alloc arg space");
		return 1;
	}

	va_start(args, fmt);
	if (vsnprintf(cmd, maxargs, fmt, args) > maxargs) {
		warnx("vsystem args are too long");
		va_end(args);
		return 1;
	}
#ifdef VSYSTEM_DEBUG
	printf("vsystem(\"%s\")\n", cmd);
#endif
	ret = system(cmd);
	va_end(args);
	free(cmd);
	return ret;
}
