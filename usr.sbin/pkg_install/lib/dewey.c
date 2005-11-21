/*	$NetBSD: dewey.c,v 1.1.2.5 2005/11/21 18:39:16 tron Exp $	*/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <nbcompat.h>

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

#if HAVE_CTYPE_H
#include <ctype.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "defs.h"
#include "dewey.h"

#define PKG_PATTERN_MAX 1024
typedef int (*matchfn) (const char *, void *);

/* do not modify these values, or things will NOT work */
enum {
        Alpha = -3,
        Beta = -2,
        RC = -1,
        Dot = 0,
        Patch = 1
};

/* this struct defines a version number */
typedef struct arr_t {
	unsigned	c;              /* # of version numbers */
	unsigned	size;           /* size of array */
	int	       *v;              /* array of decimal numbers */
	int		netbsd;         /* any "nb" suffix */
} arr_t;

/* this struct describes a test */
typedef struct test_t {
	const char     *s;              /* string representation */
	unsigned	len;            /* length of string */
	int		t;              /* enumerated type of test */
} test_t;


/* the tests that are recognised. */
 const test_t   tests[] = {
        {	"<=",	2,	DEWEY_LE	},
        {	"<",	1,	DEWEY_LT	},
        {	">=",	2,	DEWEY_GE	},
        {	">",	1,	DEWEY_GT	},
        {	"==",	2,	DEWEY_EQ	},
        {	"!=",	2,	DEWEY_NE	},
        {	NULL,	0,	0	}
};

 const test_t	modifiers[] = {
	{	"alpha",	5,	Alpha	},
	{	"beta",		4,	Beta	},
	{	"pre",		3,	RC	},
	{	"rc",		2,	RC	},
	{	"pl",		2,	Dot	},
	{	"_",		1,	Dot	},
	{	".",		1,	Dot	},
	{	NULL,		0,	0	}
};



/* locate the test in the tests array */
int
dewey_mktest(int *op, const char *test)
{
	const test_t *tp;

	for (tp = tests ; tp->s ; tp++) {
		if (strncasecmp(test, tp->s, tp->len) == 0) {
			*op = tp->t;
			return tp->len;
		}
	}
	return -1;
}

/*
 * make a component of a version number.
 * '.' encodes as Dot which is '0'
 * '_' encodes as 'patch level', or 'Dot', which is 0.
 * 'pl' encodes as 'patch level', or 'Dot', which is 0.
 * 'alpha' encodes as 'alpha version', or Alpha, which is -3.
 * 'beta' encodes as 'beta version', or Beta, which is -2.
 * 'rc' encodes as 'release candidate', or RC, which is -1.
 * 'nb' encodes as 'netbsd version', which is used after all other tests
 */
static int
mkcomponent(arr_t *ap, const char *num)
{
	static const char       alphas[] = "abcdefghijklmnopqrstuvwxyz";
	const test_t	       *modp;
	int                 n;
	const char             *cp;

	if (*num == 0) {
		return 0;
	}
	ALLOC(int, ap->v, ap->size, ap->c, 62, "mkver", exit(EXIT_FAILURE));
	if (isdigit((unsigned char)*num)) {
		for (cp = num, n = 0 ; isdigit((unsigned char)*num) ; num++) {
			n = (n * 10) + (*num - '0');
		}
		ap->v[ap->c++] = n;
		return (int)(num - cp);
	}
	for (modp = modifiers ; modp->s ; modp++) {
		if (strncasecmp(num, modp->s, modp->len) == 0) {
			ap->v[ap->c++] = modp->t;
			return modp->len;
		}
	}
	if (strncasecmp(num, "nb", 2) == 0) {
		for (cp = num, num += 2, n = 0 ; isdigit((unsigned char)*num) ; num++) {
			n = (n * 10) + (*num - '0');
		}
		ap->netbsd = n;
		return (int)(num - cp);
	}
	if (isalpha((unsigned char)*num)) {
		ap->v[ap->c++] = Dot;
		cp = strchr(alphas, tolower((unsigned char)*num));
		ALLOC(int, ap->v, ap->size, ap->c, 62, "mkver", exit(EXIT_FAILURE));
		ap->v[ap->c++] = (int)(cp - alphas) + 1;
		return 1;
	}
	return 1;
}

/* make a version number string into an array of comparable 64bit ints */
static int
mkversion(arr_t *ap, const char *num)
{
	(void) memset(ap, 0, sizeof(arr_t));
	while (*num) {
		num += mkcomponent(ap, num);
	}
	return 1;
}

#define DIGIT(v, c, n) (((n) < (c)) ? v[n] : 0)

/* compare the result against the test we were expecting */
static int
result(int cmp, int tst)
{
	switch(tst) {
	case DEWEY_LT:
		return cmp < 0;
	case DEWEY_LE:
		return cmp <= 0;
	case DEWEY_GT:
		return cmp > 0;
	case DEWEY_GE:
		return cmp >= 0;
	case DEWEY_EQ:
		return cmp == 0;
	case DEWEY_NE:
		return cmp != 0;
	default:
		return 0;
	}
}

/* do the test on the 2 vectors */
static int
vtest(arr_t *lhs, int tst, arr_t *rhs)
{
	int cmp;
	int     c;
	int     i;

	for (i = 0, c = MAX(lhs->c, rhs->c) ; i < c ; i++) {
		if ((cmp = DIGIT(lhs->v, lhs->c, i) - DIGIT(rhs->v, rhs->c, i)) != 0) {
			return result(cmp, tst);
		}
	}
	return result(lhs->netbsd - rhs->netbsd, tst);
}

/*
 * Compare two dewey decimal numbers
 */
int
dewey_cmp(const char *lhs, int op, const char *rhs)
{
	arr_t	right;
	arr_t	left;

	(void) memset(&left, 0, sizeof(left));
	if (!mkversion(&left, lhs)) {
		return 0;
	}
	(void) memset(&right, 0, sizeof(right));
	if (!mkversion(&right, rhs)) {
		return 0;
	}
        return vtest(&left, op, &right);
}

/*
 * Perform dewey match on "pkg" against "pattern".
 * Return 1 on match, 0 on non-match, -1 on error.
 */
int
dewey_match(const char *pattern, const char *pkg)
{
	const char *version;
	const char *sep, *sep2;
	int op, op2;
	int n;

	/* compare names */
	if ((version=strrchr(pkg, '-')) == NULL) {
		return 0;
	}
	if ((sep = strpbrk(pattern, "<>")) == NULL)
		return -1;
	/* compare name lengths */
	if ((sep-pattern != version-pkg) ||
	    strncmp(pkg, pattern, (size_t)(version-pkg)) != 0)
		return 0;
	version++;
	
	/* extract comparison operator */
        if ((n = dewey_mktest(&op, sep)) < 0) {
		return 0;
        }
	/* skip operator */
	sep += n;

	/* if greater than, look for less than */
	sep2 = NULL;
	if (op == DEWEY_GT || op == DEWEY_GE) {
		if ((sep2 = strchr(sep, '<')) != NULL) {
			if ((n = dewey_mktest(&op2, sep2)) < 0) {
				return 0;
			}
			/* compare upper limit */
			if (!dewey_cmp(version, op2, sep2+n))
				return 0;
		}
	}

	/* compare only pattern / lower limit */
	if (sep2) {
		char ver[PKG_PATTERN_MAX];

		strlcpy(ver, sep, MIN(sizeof(ver), sep2-sep+1));
		if (dewey_cmp(version, op, ver))
			return 1;
	}
	else {
		if (dewey_cmp(version, op, sep))
			return 1;
	}

	return 0;
}

