/*	$NetBSD: setlocale.c,v 1.17.6.1 2000/08/09 17:42:24 tshiozak Exp $	*/

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)setlocale.c	8.1 (Berkeley) 7/4/93";
#else
__RCSID("$NetBSD: setlocale.c,v 1.17.6.1 2000/08/09 17:42:24 tshiozak Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#define _CTYPE_PRIVATE

#include "namespace.h"
#include <sys/localedef.h>
#include <ctype.h>
#include <limits.h>
#define __SETLOCALE_SOURCE__
#include <locale.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ctypeio.h"

/*
 * Category names for getenv()
 */
static const char *const categories[_LC_LAST] = {
    "LC_ALL",
    "LC_COLLATE",
    "LC_CTYPE",
    "LC_MONETARY",
    "LC_NUMERIC",
    "LC_TIME",
    "LC_MESSAGES"
};

/*
 * Current locales for each category
 */
static char current_categories[_LC_LAST][32] = {
    "C",
    "C",
    "C",
    "C",
    "C",
    "C",
    "C"
};
int __mb_cur_max = 1;

/*
 * The locales we are going to try and load
 */
static char new_categories[_LC_LAST][32];

static char current_locale_string[_LC_LAST * 33];
static char *PathLocale;

static char	*currentlocale __P((void));
static char	*loadlocale __P((int));

char *
__setlocale_mb_len_max_32(category, locale)
	int category;
	const char *locale;
{
	int i;
	size_t len;
	char *env, *r;

	/*
	 * XXX potential security problem here with set-id programs
	 * being able to read files the user can not normally read.
	 */
	if (!PathLocale && !(PathLocale = getenv("PATH_LOCALE")))
		PathLocale = _PATH_LOCALE;

	if (category < 0 || category >= _LC_LAST)
		return (NULL);

	if (!locale)
		return (category ?
		    current_categories[category] : currentlocale());

	/*
	 * Default to the current locale for everything.
	 */
	for (i = 1; i < _LC_LAST; ++i)
		(void)strncpy(new_categories[i], current_categories[i],
		    sizeof(new_categories[i]) - 1);

	/*
	 * Now go fill up new_categories from the locale argument
	 */
	if (!*locale) {
		env = getenv(categories[category]);

		if (!env || !*env)
			env = getenv(categories[0]);

		if (!env || !*env)
			env = getenv("LANG");

		if (!env || !*env)
			env = "C";

		(void)strncpy(new_categories[category], env, 31);
		new_categories[category][31] = 0;
		if (!category) {
			for (i = 1; i < _LC_LAST; ++i) {
				if (!(env = getenv(categories[i])) || !*env)
					env = new_categories[0];
				(void)strncpy(new_categories[i], env, 31);
				new_categories[i][31] = 0;
			}
		}
	} else if (category) {
		(void)strncpy(new_categories[category], locale, 31);
		new_categories[category][31] = 0;
	} else {
		if ((r = strchr(locale, '/')) == 0) {
			for (i = 1; i < _LC_LAST; ++i) {
				(void)strncpy(new_categories[i], locale, 31);
				new_categories[i][31] = 0;
			}
		} else {
			for (i = 1; r[1] == '/'; ++r);
			if (!r[1])
				return (NULL);	/* Hmm, just slashes... */
			do {
				len = r - locale > 31 ? 31 : r - locale;
				(void)strncpy(new_categories[i++], locale, len);
				new_categories[i++][len] = 0;
				locale = r;
				while (*locale == '/')
				    ++locale;
				while (*++r && *r != '/');
			} while (*locale);
			while (i < _LC_LAST)
				(void)strncpy(new_categories[i],
				    new_categories[i-1],
				    sizeof(new_categories[i]) - 1);
		}
	}

	if (category)
		return (loadlocale(category));

	for (i = 1; i < _LC_LAST; ++i)
		(void) loadlocale(i);

	return (currentlocale());
}

static char *
currentlocale()
{
	int i;

	(void)strncpy(current_locale_string, current_categories[1],
	    sizeof(current_locale_string) - 1);

	for (i = 2; i < _LC_LAST; ++i)
		if (strcmp(current_categories[1], current_categories[i])) {
			(void)snprintf(current_locale_string,
			    sizeof(current_locale_string), "%s/%s/%s/%s/%s",
			    current_categories[1], current_categories[2],
			    current_categories[3], current_categories[4],
			    current_categories[5]);
			break;
		}
	return (current_locale_string);
}

static char *
loadlocale(category)
	int category;
{
	char name[PATH_MAX];

	if (strcmp(new_categories[category], current_categories[category]) == 0)
		return (current_categories[category]);

	if (!strcmp(new_categories[category], "C") ||
	    !strcmp(new_categories[category], "POSIX")) {

		switch (category) {
		case LC_CTYPE:
			if (_ctype_ != _C_ctype_) {
				/* LINTED const castaway */
				free((void *)_ctype_);
				_ctype_ = _C_ctype_;
			}
			if (_toupper_tab_ != _C_toupper_) {
				/* LINTED const castaway */
				free((void *)_toupper_tab_);
				_toupper_tab_ = _C_toupper_;
			}
			if (_tolower_tab_ != _C_tolower_) {
				/* LINTED const castaway */
				free((void *)_tolower_tab_);
				_tolower_tab_ = _C_tolower_;
			}
		}

		(void)strncpy(current_categories[category],
		    new_categories[category],
		    sizeof(current_categories[category]) - 1);
		return current_categories[category];
	}

	/*
	 * Some day we will actually look at this file.
	 */
	(void)snprintf(name, sizeof(name), "%s/%s/%s",
	    PathLocale, new_categories[category], categories[category]);

	switch (category) {
	case LC_CTYPE:
		if (__loadctype(name)) {
			(void)strncpy(current_categories[category],
			    new_categories[category],
			    sizeof(current_categories[category]) - 1);
			return current_categories[category];
		}
		return NULL;

	case LC_COLLATE:
	case LC_MESSAGES:
	case LC_MONETARY:
	case LC_NUMERIC:
	case LC_TIME:
		return NULL;
	}

	return NULL;
}
