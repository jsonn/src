/*	$NetBSD: path.c,v 1.1.2.3 2002/11/24 22:21:32 tron Exp $	*/

/*-
 * Copyright (c)2002 YAMAMOTO Takashi,
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
__RCSID("$NetBSD: path.c,v 1.1.2.3 2002/11/24 22:21:32 tron Exp $");
#endif

#include <err.h>

#include "lib.h"

struct pathhead PkgPath = TAILQ_HEAD_INITIALIZER(PkgPath);

/*
 * path_create: make PkgPath from a given string.
 *
 * => relative pathes are resolved to absolute ones.
 * => if NULL is passed, use "." instead. XXX
 */
void
path_create(const char *path)
{
	const char *cp;
	size_t len;
	char cwd[MAXPATHLEN];
	size_t cwdlen;

	path_free();

	if (path == NULL) {
		path = "."; /* XXX */
	}

	if (Verbose)
		printf("parsing: %s\n", path);

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		err(1, "getcwd");
	cwdlen = strlen(cwd);

	cp = path;
	while (*cp) {
		len = strcspn(cp, ";");
		if (len > 0) {
			/* add a new path */
			struct path *new;

			new = malloc(sizeof(*new));
			if (new == NULL)
				err(1, "path_create");

			if (!IS_FULLPATH(cp) && !IS_URL(cp)) {
				/* this is a relative path */
				size_t total;

				total = cwdlen + 1 + len + 1;
				new->pl_path = malloc(total);
				if (new->pl_path == NULL)
					err(1, "path_create");
				snprintf(new->pl_path, total, "%s/%*.*s", cwd, (int)len, (int)len, cp);
			}
			else {
				new->pl_path = malloc(len + 1);
				if (new->pl_path == NULL)
					err(1, "path_create");
				memcpy(new->pl_path, cp, len);
				new->pl_path[len] = '\0';
			}

			if (Verbose)
				printf("path: %s\n", new->pl_path);
			TAILQ_INSERT_TAIL(&PkgPath, new, pl_entry);
		}

		cp += len;
		if (*cp == '\0')
			break;
		cp++;
	}
}

/*
 * path_free: free PkgPath.
 */
void
path_free()
{
	struct path *p;

	while ((p = TAILQ_FIRST(&PkgPath)) != NULL) {
		TAILQ_REMOVE(&PkgPath, p, pl_entry);
		free(p->pl_path);
		free(p);
	}
}

/*
 * path_setenv: construct string from PkgPath and set it to a environment.
 *
 * => the environment name is given by envname.
 */
void
path_setenv(const char *envname)
{
	struct path *p;
	ssize_t len = 0;
	char *env, *env0, *envend;
	char *sep;

	TAILQ_FOREACH(p, &PkgPath, pl_entry)
		len += strlen(p->pl_path) + 1;

	env = malloc(len);
	if (env == NULL)
		err(1, "path_setenv");

	env0 = env;
	envend = env + len;
	sep = "";
	TAILQ_FOREACH(p, &PkgPath, pl_entry) {
		int r;

		r = snprintf(env, envend - env, "%s%s", sep, p->pl_path);
		if (r < 0 || r >= envend - env)
			err(1, "snprintf");
		env += r;
		sep = ";";
	}

	if (Verbose)
		printf("%s = %s\n", envname, env0);
	if (setenv(envname, env0, 1) != 0)
		err(1, "setenv");
	free(env0);
}
