/*	$NetBSD: futil.c,v 1.6.2.1 1999/09/13 21:22:26 he Exp $	*/

#include <sys/cdefs.h>
#ifndef lint
#if 0
static const char *rcsid = "from FreeBSD Id: futil.c,v 1.7 1997/10/08 07:45:39 charnier Exp";
#else
__RCSID("$NetBSD: futil.c,v 1.6.2.1 1999/09/13 21:22:26 he Exp $");
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
 * Miscellaneous file access utilities.
 *
 */

#include <err.h>
#include "lib.h"
#include "add.h"

/*
 * Assuming dir is a desired directory name, make it and all intervening
 * directories necessary.
 */
int
make_hierarchy(char *dir)
{
	char   *cp1, *cp2;

	if (dir[0] == '/')
		cp1 = cp2 = dir + 1;
	else
		cp1 = cp2 = dir;
	while (cp2) {
		if ((cp2 = strchr(cp1, '/')) != NULL)
			*cp2 = '\0';
		if (fexists(dir)) {
			if (!(isdir(dir) || islinktodir(dir)))
				return FAIL;
		} else {
			if (vsystem("mkdir %s", dir))
				return FAIL;
			apply_perms(NULL, dir);
		}
		/* Put it back */
		if (cp2) {
			*cp2 = '/';
			cp1 = cp2 + 1;
		}
	}
	return SUCCESS;
}

/*
 * Using permission defaults, apply them as necessary
 */
void
apply_perms(char *dir, char *arg)
{
	char   *cd_to;

	if (!dir || *arg == '/')/* absolute path? */
		cd_to = "/";
	else
		cd_to = dir;

	if (Mode)
		if (vsystem("cd %s && %s -R %s %s", cd_to, CHMOD, Mode, arg))
			warnx("couldn't change modes of '%s' to '%s'", arg, Mode);
	if (Owner && Group) {
		if (vsystem("cd %s && %s -R %s.%s %s", cd_to, CHOWN, Owner, Group, arg))
			warnx("couldn't change owner/group of '%s' to '%s.%s'",
			    arg, Owner, Group);
		return;
	}
	if (Owner) {
		if (vsystem("cd %s && %s -R %s %s", cd_to, CHOWN, Owner, arg))
			warnx("couldn't change owner of '%s' to '%s'", arg, Owner);
		return;
	} else if (Group)
		if (vsystem("cd %s && %s -R %s %s", cd_to, CHGRP, Group, arg))
			warnx("couldn't change group of '%s' to '%s'", arg, Group);
}
