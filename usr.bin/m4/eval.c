/*	$NetBSD: eval.c,v 1.10.4.1 2000/10/19 16:31:45 he Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit at York University.
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
#ifndef lint
#if 0
static char sccsid[] = "@(#)eval.c	8.2 (Berkeley) 4/27/95";
#else
__RCSID("$NetBSD: eval.c,v 1.10.4.1 2000/10/19 16:31:45 he Exp $");
#endif
#endif /* not lint */

/*
 * eval.c
 * Facility: m4 macro processor
 * by: oz
 */

#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mdef.h"
#include "stdd.h"
#include "extern.h"
#include "pathnames.h"

/*
 * eval - evaluate built-in macros.
 *	  argc - number of elements in argv.
 *	  argv - element vector :
 *			argv[0] = definition of a user
 *				  macro or nil if built-in.
 *			argv[1] = name of the macro or
 *				  built-in.
 *			argv[2] = parameters to user-defined
 *			   .	  macro or built-in.
 *			   .
 *
 * Note that the minimum value for argc is 3. A call in the form
 * of macro-or-builtin() will result in:
 *			argv[0] = nullstr
 *			argv[1] = macro-or-builtin
 *			argv[2] = nullstr
 */

void
eval(argv, argc, td)
	char *argv[];
	int argc;
	int td;
{
	int c, n;
	static int sysval = 0;

#ifdef DEBUG
	printf("argc = %d\n", argc);
	for (n = 0; n < argc; n++)
		printf("argv[%d] = %s\n", n, argv[n]);
#endif
 /*
  * if argc == 3 and argv[2] is null, then we
  * have macro-or-builtin() type call. We adjust
  * argc to avoid further checking..
  */
	if (argc == 3 && !*(argv[2]))
		argc--;

	switch (td & ~STATIC) {

	case DEFITYPE:
		if (argc > 2)
			dodefine(argv[2], (argc > 3) ? argv[3] : null);
		break;

	case PUSDTYPE:
		if (argc > 2)
			dopushdef(argv[2], (argc > 3) ? argv[3] : null);
		break;

	case DUMPTYPE:
		dodump(argv, argc);
		break;

	case EXPRTYPE:
	/*
	 * doexpr - evaluate arithmetic
	 * expression
	 */
		if (argc > 2)
			pbnum(expr(argv[2]));
		break;

	case IFELTYPE:
		if (argc > 4)
			doifelse(argv, argc);
		break;

	case IFDFTYPE:
	/*
	 * doifdef - select one of two
	 * alternatives based on the existence of
	 * another definition
	 */
		if (argc > 3) {
			if (lookup(argv[2]) != nil)
				pbstr(argv[3]);
			else if (argc > 4)
				pbstr(argv[4]);
		}
		break;

	case LENGTYPE:
	/*
	 * dolen - find the length of the
	 * argument
	 */
		if (argc > 2)
			pbnum((argc > 2) ? strlen(argv[2]) : 0);
		break;

	case INCRTYPE:
	/*
	 * doincr - increment the value of the
	 * argument
	 */
		if (argc > 2)
			pbnum(atoi(argv[2]) + 1);
		break;

	case DECRTYPE:
	/*
	 * dodecr - decrement the value of the
	 * argument
	 */
		if (argc > 2)
			pbnum(atoi(argv[2]) - 1);
		break;

	case SYSCTYPE:
	/*
	 * dosys - execute system command
	 */
		if (argc > 2)
			sysval = system(argv[2]);
		break;

	case SYSVTYPE:
	/*
	 * dosysval - return value of the last
	 * system call.
	 * 
	 */
		pbnum(sysval);
		break;

	case INCLTYPE:
		if (argc > 2)
			if (!doincl(argv[2]))
				err(1, "%s", argv[2]);
		break;

	case SINCTYPE:
		if (argc > 2)
			(void) doincl(argv[2]);
		break;
#ifdef EXTENDED
	case PASTTYPE:
		if (argc > 2)
			if (!dopaste(argv[2]))
				err(1, "%s", argv[2]);
		break;

	case SPASTYPE:
		if (argc > 2)
			(void) dopaste(argv[2]);
		break;
#endif
	case CHNQTYPE:
		dochq(argv, argc);
		break;

	case CHNCTYPE:
		dochc(argv, argc);
		break;

	case SUBSTYPE:
	/*
	 * dosub - select substring
	 * 
	 */
		if (argc > 3)
			dosub(argv, argc);
		break;

	case SHIFTYPE:
	/*
	 * doshift - push back all arguments
	 * except the first one (i.e. skip
	 * argv[2])
	 */
		if (argc > 3) {
			for (n = argc - 1; n > 3; n--) {
				pbstr(rquote);
				pbstr(argv[n]);
				pbstr(lquote);
				putback(COMMA);
			}
			pbstr(rquote);
			pbstr(argv[3]);
			pbstr(lquote);
		}
		break;

	case DIVRTYPE:
		if (argc > 2 && (n = atoi(argv[2])) != 0)
			dodiv(n);
		else {
			active = stdout;
			oindex = 0;
		}
		break;

	case UNDVTYPE:
		doundiv(argv, argc);
		break;

	case DIVNTYPE:
	/*
	 * dodivnum - return the number of
	 * current output diversion
	 */
		pbnum(oindex);
		break;

	case UNDFTYPE:
	/*
	 * doundefine - undefine a previously
	 * defined macro(s) or m4 keyword(s).
	 */
		if (argc > 2)
			for (n = 2; n < argc; n++)
				remhash(argv[n], ALL);
		break;

	case POPDTYPE:
	/*
	 * dopopdef - remove the topmost
	 * definitions of macro(s) or m4
	 * keyword(s).
	 */
		if (argc > 2)
			for (n = 2; n < argc; n++)
				remhash(argv[n], TOP);
		break;

	case MKTMTYPE:
	/*
	 * dotemp - create a temporary file
	 */
		if (argc > 2)
			pbstr(mktemp(argv[2]));
		break;

	case TRNLTYPE:
	/*
	 * dotranslit - replace all characters in
	 * the source string that appears in the
	 * "from" string with the corresponding
	 * characters in the "to" string.
	 */
		if (argc > 3) {
			char temp[MAXTOK];
			if (argc > 4)
				map(temp, argv[2], argv[3], argv[4]);
			else
				map(temp, argv[2], argv[3], null);
			pbstr(temp);
		}
		else if (argc > 2)
			pbstr(argv[2]);
		break;

	case INDXTYPE:
	/*
	 * doindex - find the index of the second
	 * argument string in the first argument
	 * string. -1 if not present.
	 */
		pbnum((argc > 3) ? indx(argv[2], argv[3]) : -1);
		break;

	case ERRPTYPE:
	/*
	 * doerrp - print the arguments to stderr
	 * file
	 */
		if (argc > 2) {
			for (n = 2; n < argc; n++)
				fprintf(stderr, "%s ", argv[n]);
			fprintf(stderr, "\n");
		}
		break;

	case DNLNTYPE:
	/*
	 * dodnl - eat-up-to and including
	 * newline
	 */
		while ((c = gpbc()) != '\n' && c != EOF)
			;
		break;

	case M4WRTYPE:
	/*
	 * dom4wrap - set up for
	 * wrap-up/wind-down activity
	 */
		m4wraps = (argc > 2) ? xstrdup(argv[2]) : null;
		break;

	case EXITTYPE:
	/*
	 * doexit - immediate exit from m4.
	 */
		killdiv();
		exit((argc > 2) ? atoi(argv[2]) : 0);
		break;

	case DEFNTYPE:
		if (argc > 2)
			for (n = 2; n < argc; n++)
				dodefn(argv[n]);
		break;

	default:
		errx(1, "eval: major botch");
		break;
	}
}

const char dumpfmt[] = "`%s'\t`%s'\n";	       /* format string for dumpdef   */

/*
 * expand - user-defined macro expansion
 */
void
expand(argv, argc)
	char *argv[];
	int argc;
{
	char *t;
	char *p;
	int n;
	int argno;

	t = argv[0];		       /* defn string as a whole */
	p = t;
	while (*p)
		p++;
	p--;			       /* last character of defn */
	while (p > t) {
		if (*(p - 1) != ARGFLAG)
			putback(*p);
		else {
			switch (*p) {

			case '#':
				pbnum(argc - 2);
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if ((argno = *p - '0') < argc - 1)
					pbstr(argv[argno + 1]);
				break;
			case '*':
				for (n = argc - 1; n > 2; n--) {
					pbstr(argv[n]);
					putback(COMMA);
				}
				pbstr(argv[2]);
				break;
                        case '@':
                                for (n = argc - 1; n > 2; n--) {
                                        pbstr(rquote);
                                        pbstr(argv[n]);
                                        pbstr(lquote);
					putback(COMMA);
                                }
				pbstr(rquote);
                                pbstr(argv[2]);
				pbstr(lquote);
                                break;
			default:
				putback(*p);
				putback('$');
				break;
			}
			p--;
		}
		p--;
	}
	if (p == t)		       /* do last character */
		putback(*p);
}

/*
 * dodefine - install definition in the table
 */
void
dodefine(name, defn)
	char *name;
	char *defn;
{
	ndptr p;

	if (!*name)
		errx(1, "null definition");
	if (STREQ(name, defn))
		errx(1, "%s: recursive definition", name);
	if ((p = lookup(name)) == nil)
		p = addent(name);
	else if (p->defn != null)
		free((char *) p->defn);
	if (!*defn)
		p->defn = null;
	else
		p->defn = xstrdup(defn);
	p->type = MACRTYPE;
}

/*
 * dodefn - push back a quoted definition of
 *      the given name.
 */
void
dodefn(name)
	char *name;
{
	ndptr p;

	if ((p = lookup(name)) != nil && p->defn != null) {
		pbstr(rquote);
		pbstr(p->defn);
		pbstr(lquote);
	}
}

/*
 * dopushdef - install a definition in the hash table
 *      without removing a previous definition. Since
 *      each new entry is entered in *front* of the
 *      hash bucket, it hides a previous definition from
 *      lookup.
 */
void
dopushdef(name, defn)
	char *name;
	char *defn;
{
	ndptr p;

	if (!*name)
		errx(1, "null definition");
	if (STREQ(name, defn))
		errx(1, "%s: recursive definition", name);
	p = addent(name);
	if (!*defn)
		p->defn = null;
	else
		p->defn = xstrdup(defn);
	p->type = MACRTYPE;
}

/*
 * dodumpdef - dump the specified definitions in the hash
 *      table to stderr. If nothing is specified, the entire
 *      hash table is dumped.
 */
void
dodump(argv, argc)
	char *argv[];
	int argc;
{
	int n;
	ndptr p;

	if (argc > 2) {
		for (n = 2; n < argc; n++)
			if ((p = lookup(argv[n])) != nil)
				fprintf(stderr, dumpfmt, p->name,
					p->defn);
	}
	else {
		for (n = 0; n < HASHSIZE; n++)
			for (p = hashtab[n]; p != nil; p = p->nxtptr)
				fprintf(stderr, dumpfmt, p->name,
					p->defn);
	}
}

/*
 * doifelse - select one of two alternatives - loop.
 */
void
doifelse(argv, argc)
	char *argv[];
	int argc;
{
	cycle {
		if (STREQ(argv[2], argv[3]))
			pbstr(argv[4]);
		else if (argc == 6)
			pbstr(argv[5]);
		else if (argc > 6) {
			argv += 3;
			argc -= 3;
			continue;
		}
		break;
	}
}

/*
 * doinclude - include a given file.
 */
int
doincl(ifile)
	char *ifile;
{
	if (ilevel + 1 == MAXINP)
		errx(1, "too many include files");
	if ((infile[ilevel + 1] = fopen(ifile, "r")) != NULL) {
		ilevel++;
		bbase[ilevel] = bufbase = bp;
		return (1);
	}
	else
		return (0);
}

#ifdef EXTENDED
/*
 * dopaste - include a given file without any
 *           macro processing.
 */
int
dopaste(pfile)
	char *pfile;
{
	FILE *pf;
	int c;

	if ((pf = fopen(pfile, "r")) != NULL) {
		while ((c = getc(pf)) != EOF)
			putc(c, active);
		(void) fclose(pf);
		return (1);
	}
	else
		return (0);
}
#endif

/*
 * dochq - change quote characters
 */
void
dochq(argv, argc)
	char *argv[];
	int argc;
{
	if (argc > 2) {
		if (*argv[2])
			strncpy(lquote, argv[2], MAXCCHARS);
		if (argc > 3) {
			if (*argv[3])
				strncpy(rquote, argv[3], MAXCCHARS);
		}
		else
			strcpy(rquote, lquote);
	}
	else {
		lquote[0] = LQUOTE, lquote[1] = '\0';
		rquote[0] = RQUOTE, rquote[1] = '\0';
	}
}

/*
 * dochc - change comment characters
 */
void
dochc(argv, argc)
	char *argv[];
	int argc;
{
	if (argc > 2) {
		if (*argv[2])
			strncpy(scommt, argv[2], MAXCCHARS);
		if (argc > 3) {
			if (*argv[3])
				strncpy(ecommt, argv[3], MAXCCHARS);
		}
		else
			ecommt[0] = ECOMMT, ecommt[1] = '\0';
	}
	else {
		scommt[0] = SCOMMT, scommt[1] = '\0';
		ecommt[0] = ECOMMT, ecommt[1] = '\0';
	}
}

/*
 * dodivert - divert the output to a temporary file
 */
void
dodiv(n)
	int n;
{
	int tempfilenum;

	/*
	 * direct output to the appropriate temporary file (the bit
	 * bucket, if out of range).
	 */
	tempfilenum = (n < 0 || n >= MAXOUT) ? 0 : n;

	if (outfile[tempfilenum] == NULL) {
		m4temp[UNIQUE] = tempfilenum + '0';
		if ((outfile[tempfilenum] = fopen(m4temp, "w")) == NULL)
			err(1, "%s: cannot divert", m4temp);
	}
	oindex = n;
	active = outfile[tempfilenum];
}

/*
 * doundivert - undivert a specified output, or all
 *              other outputs, in numerical order.
 */
void
doundiv(argv, argc)
	char *argv[];
	int argc;
{
	int ind;
	int n;

	if (argc > 2) {
		for (ind = 2; ind < argc; ind++) {
			n = atoi(argv[ind]);
			if (n > 0 && n < MAXOUT && outfile[n] != NULL)
				getdiv(n);

		}
	}
	else
		for (n = 1; n < MAXOUT; n++)
			if (outfile[n] != NULL)
				getdiv(n);
}

/*
 * dosub - select substring
 */
void
dosub(argv, argc)
	char *argv[];
	int argc;
{
	char *ap, *fc, *k;
	int nc;

	if (argc < 5)
		nc = MAXTOK;
	else
#ifdef EXPR
		nc = expr(argv[4]);
#else
		nc = atoi(argv[4]);
#endif
	ap = argv[2];		       /* target string */
#ifdef EXPR
	fc = ap + expr(argv[3]);       /* first char */
#else
	fc = ap + atoi(argv[3]);       /* first char */
#endif
	if (fc >= ap && fc < ap + strlen(ap))
		for (k = fc + min(nc, strlen(fc)) - 1; k >= fc; k--)
			putback(*k);
}

/*
 * map:
 * map every character of s1 that is specified in from
 * into s3 and replace in s. (source s1 remains untouched)
 *
 * This is a standard implementation of map(s,from,to) function of ICON
 * language. Within mapvec, we replace every character of "from" with
 * the corresponding character in "to". If "to" is shorter than "from",
 * than the corresponding entries are null, which means that those
 * characters dissapear altogether. Furthermore, imagine
 * map(dest, "sourcestring", "srtin", "rn..*") type call. In this case,
 * `s' maps to `r', `r' maps to `n' and `n' maps to `*'. Thus, `s'
 * ultimately maps to `*'. In order to achieve this effect in an efficient
 * manner (i.e. without multiple passes over the destination string), we
 * loop over mapvec, starting with the initial source character. if the
 * character value (dch) in this location is different than the source
 * character (sch), sch becomes dch, once again to index into mapvec, until
 * the character value stabilizes (i.e. sch = dch, in other words
 * mapvec[n] == n). Even if the entry in the mapvec is null for an ordinary
 * character, it will stabilize, since mapvec[0] == 0 at all times. At the
 * end, we restore mapvec* back to normal where mapvec[n] == n for
 * 0 <= n <= 127. This strategy, along with the restoration of mapvec, is
 * about 5 times faster than any algorithm that makes multiple passes over
 * destination string.
 */
void
map(dest, src, from, to)
	char *dest;
	char *src;
	char *from;
	char *to;
{
	char *tmp;
	char sch, dch;
	static char mapvec[128] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
		12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
		36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
		48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
		60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
		72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83,
		84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
		96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107,
		108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
		120, 121, 122, 123, 124, 125, 126, 127
	};

	if (*src) {
		tmp = from;
	/*
	 * create a mapping between "from" and
	 * "to"
	 */
		while (*from)
			mapvec[(int)*from++] = (*to) ? *to++ : (char) 0;

		while (*src) {
			sch = *src++;
			dch = mapvec[(int)sch];
			while (dch != sch) {
				sch = dch;
				dch = mapvec[(int)sch];
			}
			if ((*dest = dch) != 0)
				dest++;
		}
	/*
	 * restore all the changed characters
	 */
		while (*tmp) {
			mapvec[(int)*tmp] = *tmp;
			tmp++;
		}
	}
	*dest = (char) 0;
}
