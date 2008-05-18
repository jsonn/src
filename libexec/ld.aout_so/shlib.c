/*	$NetBSD: shlib.c,v 1.21.16.1 2008/05/18 12:30:44 yamt Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef sun
char	*strsep();
int	isdigit();
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <a.out.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <link_aout.h>

#include "shlib.h"

/*
 * Standard directories to search for files specified by -l.
 */
#ifndef STANDARD_SEARCH_DIRS
#define	STANDARD_SEARCH_DIRS	"/usr/lib"
#endif

/*
 * Actual vector of library search directories,
 * including `-L'ed and LD_LIBRARY_PATH spec'd ones.
 */
char	 **search_dirs;
int	n_search_dirs;

const char	*standard_search_dirs[] = {
	STANDARD_SEARCH_DIRS
};

void
add_search_dir(name)
	const char	*name;
{
	n_search_dirs += 2;
	search_dirs = (char **)
		xrealloc(search_dirs, n_search_dirs * sizeof search_dirs[0]);
	search_dirs[n_search_dirs - 2] = strdup(name);
	search_dirs[n_search_dirs - 1] =
	    xmalloc(sizeof(_PATH_EMUL_AOUT) + strlen(name));
	strcpy(search_dirs[n_search_dirs - 1], _PATH_EMUL_AOUT);
	strcat(search_dirs[n_search_dirs - 1], name);
}

void
remove_search_dir(name)
	char	*name;
{
	int	n;

	for (n = 0; n < n_search_dirs; n++) {
		if (strcmp(search_dirs[n], name))
			continue;
		free(search_dirs[n]);
		free(search_dirs[n+1]);
		if (n < (n_search_dirs - 2))
			bcopy(&search_dirs[n+2], &search_dirs[n],
			      (n_search_dirs - n - 2) * sizeof search_dirs[0]);
		n_search_dirs -= 2;
	}
}

void
add_search_path(path)
char	*path;
{
	register char	*cp, *dup;

	if (path == NULL)
		return;

	/* Add search directories from `path' */
	path = dup = strdup(path);
	while ((cp = strsep(&path, ":")) != NULL)
		add_search_dir(cp);
	free(dup);
}

void
remove_search_path(path)
char	*path;
{
	register char	*cp, *dup;

	if (path == NULL)
		return;

	/* Remove search directories from `path' */
	path = dup = strdup(path);
	while ((cp = strsep(&path, ":")) != NULL)
		remove_search_dir(cp);
	free(dup);
}

void
std_search_path()
{
	int	i, n;

	/* Append standard search directories */
	n = sizeof standard_search_dirs / sizeof standard_search_dirs[0];
	for (i = 0; i < n; i++)
		add_search_dir(standard_search_dirs[i]);
}

/*
 * Return true if CP points to a valid dewey number.
 * Decode and leave the result in the array DEWEY.
 * Return the number of decoded entries in DEWEY.
 */

int
getdewey(dewey, cp)
int	dewey[];
char	*cp;
{
	int	i, n;

	for (n = 0, i = 0; i < MAXDEWEY; i++) {
		if (*cp == '\0')
			break;

		if (*cp == '.') cp++;
#ifdef SUNOS_LIB_COMPAT
		if (!(isdigit)(*cp))
#else
		if (!isdigit((unsigned char)*cp))
#endif
			return 0;

		dewey[n++] = strtol(cp, &cp, 10);
	}

	return n;
}

/*
 * Compare two dewey arrays.
 * Return -1 if `d1' represents a smaller value than `d2'.
 * Return  1 if `d1' represents a greater value than `d2'.
 * Return  0 if equal.
 */
int
cmpndewey(d1, n1, d2, n2)
int	d1[], d2[];
int	n1, n2;
{
	register int	i;

	for (i = 0; i < n1 && i < n2; i++) {
		if (d1[i] < d2[i])
			return -1;
		if (d1[i] > d2[i])
			return 1;
	}

	if (n1 == n2)
		return 0;

	if (i == n1)
		return -1;

	if (i == n2)
		return 1;

	errx(1, "cmpndewey: cant happen");
	return 0;
}

/*
 * Search directories for a shared library matching the given
 * major and minor version numbers.
 *
 * MAJOR == -1 && MINOR == -1	--> find highest version
 * MAJOR != -1 && MINOR == -1   --> find highest minor version
 * MAJOR == -1 && MINOR != -1   --> invalid
 * MAJOR != -1 && MINOR != -1   --> find highest micro version
 */

/* Not interested in devices right now... */
#undef major
#undef minor

char *
findshlib(name, majorp, minorp, do_dot_a)
char	*name;
int	*majorp, *minorp;
int	do_dot_a;
{
	int		dewey[MAXDEWEY];
	int		ndewey;
	int		tmp[MAXDEWEY];
	int		i;
	int		len;
	char		*lname, *path = NULL;
	int		major = *majorp, minor = *minorp;

	len = strlen(name) + sizeof("lib");
#if defined(__SSP__) || defined(__SSP_ALL__)
	lname = xmalloc(len);
#else
	lname = alloca(len);
#endif
	len--;
	sprintf(lname, "lib%s", name);

	ndewey = 0;

	for (i = 0; i < n_search_dirs; i++) {
		DIR		*dd = opendir(search_dirs[i]);
		struct dirent	*dp;
		int		found_dot_a = 0;
		int		found_dot_so = 0;

		if (dd == NULL)
			continue;

		while ((dp = readdir(dd)) != NULL) {
			int	n;
			struct exec ex;
			char *xpath;
			FILE *fp;

			if (do_dot_a && path == NULL &&
					dp->d_namlen == len + 2 &&
					strncmp(dp->d_name, lname, len) == 0 &&
					(dp->d_name+len)[0] == '.' &&
					(dp->d_name+len)[1] == 'a') {

				path = concat(search_dirs[i], "/", dp->d_name);
				found_dot_a = 1;
			}

			if (dp->d_namlen < len + 4)
				continue;
			if (strncmp(dp->d_name, lname, len) != 0)
				continue;
			if (strncmp(dp->d_name+len, ".so.", 4) != 0)
				continue;

			if ((n = getdewey(tmp, dp->d_name+len+4)) == 0)
				continue;

			if (major != -1 && found_dot_a) { /* XXX */
				free(path);
				path = NULL;
				found_dot_a = 0;
			}

			/* verify the library is a.out */
			xpath = concat(search_dirs[i], "/", dp->d_name);
			fp = fopen(xpath, "r");
			free(xpath);
			if (fp == NULL) {
				continue;
			}
			if (sizeof(ex) != fread(&ex, 1, sizeof(ex), fp)) {
				fclose(fp);
				continue;
			}
			fclose(fp);
			if (N_GETMAGIC(ex) != ZMAGIC
			    || (N_GETFLAG(ex) & EX_DYNAMIC) == 0) {
				continue;
			}

			if (major == -1 && minor == -1) {
				goto compare_version;
			} else if (major != -1 && minor == -1) {
				if (tmp[0] == major)
					goto compare_version;
			} else if (major != -1 && minor != -1) {
				if (tmp[0] == major) {
					if (n == 1 || tmp[1] >= minor)
						goto compare_version;
				}
			}

			/* else, this file does not qualify */
			continue;

		compare_version:
			if (cmpndewey(tmp, n, dewey, ndewey) <= 0)
				continue;

			/* We have a better version */
			found_dot_so = 1;
			if (path)
				free(path);
			path = concat(search_dirs[i], "/", dp->d_name);
			found_dot_a = 0;
			bcopy(tmp, dewey, sizeof(dewey));
			ndewey = n;
			*majorp = dewey[0];
			*minorp = dewey[1];
		}
		closedir(dd);

		if (found_dot_a || found_dot_so)
			/*
			 * There's a lib in this dir; take it.
			 */
			return path;
	}
#if defined(__SSP__) || defined(__SSP_ALL__)
	free(lname);
#endif
	return path;
}


/*
 * Utility functions shared with others.
 */


/*
 * Like malloc but get fatal error if memory is exhausted.
 */
void *
xmalloc(size)
	size_t size;
{
	void	*result = (void *)malloc(size);

	if (!result)
		errx(1, "virtual memory exhausted");

	return (result);
}

/*
 * Like realloc but get fatal error if memory is exhausted.
 */
void *
xrealloc(ptr, size)
	void *ptr;
	size_t size;
{
	void	*result;

	result = (ptr == NULL) ? malloc(size) : realloc(ptr, size);
	if (result == NULL)
		errx(1, "virtual memory exhausted");

	return (result);
}

/*
 * Return a newly-allocated string whose contents concatenate
 * the strings S1, S2, S3.
 */
char *
concat(s1, s2, s3)
	const char *s1, *s2, *s3;
{
	int	len1 = strlen(s1),
		len2 = strlen(s2),
		len3 = strlen(s3);

	char *result = (char *)xmalloc(len1 + len2 + len3 + 1);

	strcpy(result, s1);
	strcpy(result + len1, s2);
	strcpy(result + len1 + len2, s3);
	result[len1 + len2 + len3] = 0;

	return (result);
}

