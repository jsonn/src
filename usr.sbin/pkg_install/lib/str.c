/*	$NetBSD: str.c,v 1.50.2.5 2005/11/06 13:41:32 tron Exp $	*/

#include <sys/cdefs.h>
#ifndef lint
#if 0
static const char *rcsid = "Id: str.c,v 1.5 1997/10/08 07:48:21 charnier Exp";
#else
__RCSID("$NetBSD: str.c,v 1.50.2.5 2005/11/06 13:41:32 tron Exp $");
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
 * Miscellaneous string utilities.
 *
 */

#include <assert.h>
#include <err.h>
#include <fnmatch.h>
#include "lib.h"
#include "dewey.h"

/* pull in definitions and macros for resizing arrays as we go */
#include "defs.h"

/*
 * Return the suffix portion of a path
 */
const char *
suffix_of(const char *str)
{
	const char *dot;

	return ((dot = strrchr(basename_of(str), '.')) == NULL) ? "" : dot + 1;
}

/*
 * Return the filename portion of a path
 */
const char *
basename_of(const char *str)
{
	const char *slash;

	return ((slash = strrchr(str, '/')) == NULL) ? str : slash + 1;
}

/*
 * Return the dirname portion of a path
 */
const char *
dirname_of(const char *path)
{
	size_t  cc;
	char   *s;
	static char buf[PATH_MAX];

	if ((s = strrchr(path, '/')) == NULL) {
		return ".";
	}
	if (s == path) {
		/* "/foo" -> return "/" */
		return "/";
	}
	cc = (size_t) (s - path);
	if (cc >= sizeof(buf))
		errx(EXIT_FAILURE, "dirname_of: too long dirname: '%s'", path);
	(void) memcpy(buf, path, cc);
	buf[cc] = 0;
	return buf;
}

/*
 * Get a string parameter as a file spec or as a "contents follow -" spec
 */
char   *
get_dash_string(char **s)
{
	return *s = (**s == '-') ? strdup(*s + 1) : fileGetContents(*s);
}

/*
 * Lowercase a whole string
 */
void
str_lowercase(unsigned char *s)
{
	for (; *s; s++) {
		*s = tolower(*s);
	}
}

/*
 * Perform alternate match on "pkg" against "pattern",
 * calling pmatch (recursively) to resolve any other patterns.
 * Return 1 on match, 0 otherwise
 */
static int
alternate_match(const char *pattern, const char *pkg)
{
	char   *sep;
	char    buf[MaxPathSize];
	char   *last;
	char   *alt;
	char   *cp;
	int     cnt;
	int     found;

	if ((sep = strchr(pattern, '{')) == (char *) NULL) {
		errx(EXIT_FAILURE, "alternate_match(): '{' expected in `%s'", pattern);
	}
	(void) strncpy(buf, pattern, (size_t) (sep - pattern));
	alt = &buf[sep - pattern];
	last = (char *) NULL;
	for (cnt = 0, cp = sep; *cp && last == (char *) NULL; cp++) {
		if (*cp == '{') {
			cnt++;
		} else if (*cp == '}' && --cnt == 0 && last == (char *) NULL) {
			last = cp + 1;
		}
	}
	if (cnt != 0) {
		errx(EXIT_FAILURE, "Malformed alternate `%s'", pattern);
	}
	for (found = 0, cp = sep + 1; *sep != '}'; cp = sep + 1) {
		for (cnt = 0, sep = cp; cnt > 0 || (cnt == 0 && *sep != '}' && *sep != ','); sep++) {
			if (*sep == '{') {
				cnt++;
			} else if (*sep == '}') {
				cnt--;
			}
		}
		(void) snprintf(alt, sizeof(buf) - (alt - buf), "%.*s%s", (int) (sep - cp), cp, last);
		if (pmatch(buf, pkg) == 1) {
			found = 1;
		}
	}
	return found;
}

/*
 * Perform glob match on "pkg" against "pattern".
 * Return 1 on match, 0 otherwise
 */
static int
glob_match(const char *pattern, const char *pkg)
{
	return fnmatch(pattern, pkg, FNM_PERIOD) == 0;
}

/*
 * Perform simple match on "pkg" against "pattern". 
 * Return 1 on match, 0 otherwise
 */
static int
simple_match(const char *pattern, const char *pkg)
{
	return strcmp(pattern, pkg) == 0;
}

/*
 * Match pkg against pattern, return 1 if matching, 0 else
 */
int
pmatch(const char *pattern, const char *pkg)
{
	if (strchr(pattern, '{') != (char *) NULL) {
		/* emulate csh-type alternates */
		return alternate_match(pattern, pkg);
	}
	if (strpbrk(pattern, "<>") != (char *) NULL) {
		/* perform relational dewey match on version number */
		return dewey_match(pattern, pkg);
	}
	if (strpbrk(pattern, "*?[]") != (char *) NULL) {
		/* glob match */
		return glob_match(pattern, pkg);
	}
	
	/* no alternate, dewey or glob match -> simple compare */
	return simple_match(pattern, pkg);
}

/*
 * Search dir for pattern, calling match(pkg_found, data) for every match.
 * Returns -1 on error, 1 if found, 0 otherwise.
 */
int
findmatchingname(const char *dir, const char *pattern, matchfn match, void *data)
{
	struct dirent *dp;
	char tmp_pattern[PKG_PATTERN_MAX];
	DIR    *dirp;
	int     found;
	char pat_sfx[PKG_SUFFIX_MAX], file_sfx[PKG_SUFFIX_MAX];	/* suffixes */

	if (strlen(pattern) >= PKG_PATTERN_MAX)
		errx(EXIT_FAILURE, "too long pattern '%s'", pattern);

	found = 0;
	if ((dirp = opendir(dir)) == (DIR *) NULL) {
		/* warnx("can't opendir dir '%s'", dir); */
		return -1;
	}

	/* chop any possible suffix off of 'pattern' and
	 * store it in pat_sfx
	 */
	strip_txz(tmp_pattern, pat_sfx, pattern);
	
	while ((dp = readdir(dirp)) != (struct dirent *) NULL) {
		char    tmp_file[MaxPathSize];
		
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;

		/* chop any possible suffix off of 'tmp_file' and
		 * store it in file_sfx
		 */
		strip_txz(tmp_file, file_sfx, dp->d_name);
		
		/* we need to match pattern and suffix separately, in case
		 * each is a different pattern class (e.g. dewey and
		 * character class (.t[bg]z)) */
		if (pmatch(tmp_pattern, tmp_file)
		    && (pat_sfx[0] == '\0' || pmatch(pat_sfx, file_sfx))) {
			if (match) {
				match(dp->d_name, data);
				/* return value ignored for now */
			}
			found = 1;
		}
	}
	(void) closedir(dirp);
	return found;
}

/*
 * Does the pkgname contain any of the special chars ("{[]?*<>")? 
 * If so, return 1, else 0
 */
int
ispkgpattern(const char *pkg)
{
	return strpbrk(pkg, "<>[]?*{") != NULL;
}

/*
 * Auxiliary function called by findbestmatchingname() if pkg > data
 * Also called for FTP matching
 */
int
findbestmatchingname_fn(const char *found, void *vp)
{
	char *best = vp;
	char *found_version, *best_version;
	char found_no_sfx[PKG_PATTERN_MAX];
	char best_no_sfx[PKG_PATTERN_MAX];

	/* The same suffix-hack-off again, but we can't do it
	 * otherwise without changing the function call interface
	 */
	found_version = strrchr(found, '-');
	if (found_version) {
		/* skip '-', if any version found */
		found_version++;
	}
	strip_txz(found_no_sfx, NULL, found_version);
	found_version = found_no_sfx;
	
	best_version=NULL;
	if (best && best[0] != '\0') {
		best_version = strrchr(best, '-');
		if (best_version) {
			/* skip '-' if any version found */
			best_version++;
		}
		strip_txz(best_no_sfx, NULL, best_version);
		best_version = best_no_sfx;
	}

	if (found_version == NULL) {
		fprintf(stderr, "'%s' is not a usable package(version)\n",
			found);
	} else {
		/* if best_version==NULL only if best==NULL
		 * (or best[0]='\0') */
		if (best == NULL || best[0] == '\0' ||
		    dewey_cmp(found_version, DEWEY_GT, best_version)) {
			/* found pkg(version) is bigger than current "best"
			 * version - remember! */
			strcpy(best, found);
		}
	}

	return 0;
}

/*
 * Find best matching filename, i.e. the pkg with the highest
 * matching(!) version. 
 * Returns pointer to pkg name (which can be free(3)ed),
 * or NULL if no match is available.
 */
char *
findbestmatchingname(const char *dir, const char *pattern)
{
	char    buf[MaxPathSize];

	buf[0] = '\0';
	if (findmatchingname(dir, pattern, findbestmatchingname_fn, buf) > 0
	    && buf[0] != '\0') {
		return strdup(buf);
	}
	return NULL;
}

/*
 * Strip off any .tgz, .tbz or .t[bg]z suffix from fname,
 * and copy into buffer "buf", the suffix is stored in "sfx"
 * if "sfx" is not NULL. If no suffix is found, "sfx" is set
 * to an empty string. 
 */
void
strip_txz(char *buf, char *sfx, const char *fname)
{
	static const char *const suffixes[] = {
		".tgz", ".tbz", ".t[bg]z", 0};
	const char *const *suffixp;
	size_t len;

	len = strlen(fname);
	assert(len < PKG_PATTERN_MAX);

	if (sfx)
		sfx[0] = '\0';

	for (suffixp = suffixes; *suffixp; suffixp++) {
		size_t suffixlen = strlen(*suffixp);

		if (memcmp(&fname[len - suffixlen], *suffixp, suffixlen))
			continue;

		/* matched! */
		memcpy(buf, fname, len - suffixlen);
		buf[len - suffixlen] = 0;
		if (sfx) {
			if (suffixlen >= PKG_SUFFIX_MAX)
				errx(EXIT_FAILURE, "too long suffix '%s'", fname);
			memcpy(sfx, *suffixp, suffixlen+1);
			return;
		}
	}

	/* not found */
	memcpy(buf, fname, len+1);
}

/*
 * Called to see if pkg is already installed as some other version, 
 * note found version in "note".
 */
int
note_whats_installed(const char *found, void *vp)
{
	char *note = vp;

	(void) strlcpy(note, found, MaxPathSize);
	return 0;
}

/*
 * alloc lpkg for pkg and add it to list.
 */
int
add_to_list_fn(const char *pkg, void *vp)
{
	lpkg_head_t *pkgs = vp;
	lpkg_t *lpp;
	char fn[MaxPathSize];

	snprintf(fn, sizeof(fn), "%s/%s", _pkgdb_getPKGDB_DIR(), pkg);
	if (isdir(fn) || islinktodir(fn)) {
		lpp = alloc_lpkg(pkg);
		TAILQ_INSERT_TAIL(pkgs, lpp, lp_link);
	}

	return 0;
}
