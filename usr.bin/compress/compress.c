/*	$NetBSD: compress.c,v 1.9.6.1 1997/01/26 03:20:10 rat Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)compress.c	8.2 (Berkeley) 1/7/94";
#else
static char rcsid[] = "$NetBSD: compress.c,v 1.9.6.1 1997/01/26 03:20:10 rat Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void	compress __P((char *, char *, int));
void	cwarn __P((const char *, ...));
void	cwarnx __P((const char *, ...));
void	decompress __P((char *, char *, int));
int	permission __P((char *));
void	setfile __P((char *, struct stat *));
void	usage __P((int));

extern FILE *zopen __P((const char *fname, const char *mode, int bits));

int eval, force, verbose;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	enum {COMPRESS, DECOMPRESS} style;
	size_t len;
	int bits, cat, ch;
	char *p, newname[MAXPATHLEN];

	if ((p = rindex(argv[0], '/')) == NULL)
		p = argv[0];
	else
		++p;
	if (!strcmp(p, "uncompress"))
		style = DECOMPRESS;
	else if (!strcmp(p, "compress")) 
		style = COMPRESS;
	else
		errx(1, "unknown program name");

	bits = cat = 0;
	while ((ch = getopt(argc, argv, "b:cdfv")) != EOF)
		switch(ch) {
		case 'b':
			bits = strtol(optarg, &p, 10);
			if (*p)
				errx(1, "illegal bit count -- %s", optarg);
			break;
		case 'c':
			cat = 1;
			break;
		case 'd':		/* Backward compatible. */
			style = DECOMPRESS;
			break;
		case 'f':
			force = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage(style == COMPRESS);
		}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		switch(style) {
		case COMPRESS:
			(void)compress("/dev/stdin", "/dev/stdout", bits);
			break;
		case DECOMPRESS:
			(void)decompress("/dev/stdin", "/dev/stdout", bits);
			break;
		}
		exit (eval);
	}

	if (cat == 1 && argc > 1)
		errx(1, "the -c option permits only a single file argument");

	for (; *argv; ++argv) {
		switch(style) {
		case COMPRESS:
			if (cat) {
				compress(*argv, "/dev/stdout", bits);
				break;
			}
			if ((p = rindex(*argv, '.')) != NULL &&
			    !strcmp(p, ".Z")) {
				cwarnx("%s: name already has trailing .Z",
				    *argv);
				break;
			}
			len = strlen(*argv);
			if (len > sizeof(newname) - 3) {
				cwarnx("%s: name too long", *argv);
				break;
			}
			memmove(newname, *argv, len);
			newname[len] = '.';
			newname[len + 1] = 'Z';
			newname[len + 2] = '\0';
			compress(*argv, newname, bits);
			break;
		case DECOMPRESS:
			len = strlen(*argv);
			if ((p = rindex(*argv, '.')) == NULL ||
			    strcmp(p, ".Z")) {
				if (len > sizeof(newname) - 3) {
					cwarnx("%s: name too long", *argv);
					break;
				}
				memmove(newname, *argv, len);
				newname[len] = '.';
				newname[len + 1] = 'Z';
				newname[len + 2] = '\0';
				decompress(newname,
				    cat ? "/dev/stdout" : *argv, bits);
			} else {
				if (len - 2 > sizeof(newname) - 1) {
					cwarnx("%s: name too long", *argv);
					break;
				}
				memmove(newname, *argv, len - 2);
				newname[len - 2] = '\0';
				decompress(*argv,
				    cat ? "/dev/stdout" : newname, bits);
			}
			break;
		}
	}
	exit (eval);
}

void
compress(in, out, bits)
	char *in, *out;
	int bits;
{
	register int nr;
	struct stat isb, sb;
	FILE *ifp, *ofp;
	int exists, isreg, oreg;
	u_char buf[1024];

	exists = !stat(out, &sb);
	if (!force && exists && S_ISREG(sb.st_mode) && !permission(out))
		return;
	isreg = oreg = !exists || S_ISREG(sb.st_mode);

	ifp = ofp = NULL;
	if ((ifp = fopen(in, "r")) == NULL) {
		cwarn("%s", in);
		return;
	}
	if (stat(in, &isb)) {		/* DON'T FSTAT! */
		cwarn("%s", in);
		goto err;
	}
	if (!S_ISREG(isb.st_mode))
		isreg = 0;

	if ((ofp = zopen(out, "w", bits)) == NULL) {
		cwarn("%s", out);
		goto err;
	}
	while ((nr = fread(buf, 1, sizeof(buf), ifp)) != 0)
		if (fwrite(buf, 1, nr, ofp) != nr) {
			cwarn("%s", out);
			goto err;
		}

	if (ferror(ifp) || fclose(ifp)) {
		cwarn("%s", in);
		goto err;
	}
	ifp = NULL;

	if (fclose(ofp)) {
		cwarn("%s", out);
		goto err;
	}
	ofp = NULL;

	if (isreg) {
		if (stat(out, &sb)) {
			cwarn("%s", out);
			goto err;
		}

		if (!force && sb.st_size >= isb.st_size) {
			if (verbose)
		(void)printf("%s: file would grow; left unmodified\n", in);
			if (unlink(out))
				cwarn("%s", out);
			goto err;
		}

		setfile(out, &isb);

		if (unlink(in))
			cwarn("%s", in);

		if (verbose) {
			(void)printf("%s: ", out);
			if (isb.st_size > sb.st_size)
				(void)printf("%.0f%% compression\n",
				    ((float)sb.st_size / isb.st_size) * 100.0);
			else
				(void)printf("%.0f%% expansion\n",
				    ((float)isb.st_size / sb.st_size) * 100.0);
		}
	}
	return;

err:	if (ofp) {
		if (oreg)
			(void)unlink(out);
		(void)fclose(ofp);
	}
	if (ifp)
		(void)fclose(ifp);
}

void
decompress(in, out, bits)
	char *in, *out;
	int bits;
{
	register int nr;
	struct stat sb;
	FILE *ifp, *ofp;
	int exists, isreg, oreg;
	u_char buf[1024];

	exists = !stat(out, &sb);
	if (!force && exists && S_ISREG(sb.st_mode) && !permission(out))
		return;
	isreg = oreg = !exists || S_ISREG(sb.st_mode);

	ifp = ofp = NULL;
	if ((ofp = fopen(out, "w")) == NULL) {
		cwarn("%s", out);
		return;
	}

	if ((ifp = zopen(in, "r", bits)) == NULL) {
		cwarn("%s", in);
		goto err;
	}
	if (stat(in, &sb)) {
		cwarn("%s", in);
		goto err;
	}
	if (!S_ISREG(sb.st_mode))
		isreg = 0;

	while ((nr = fread(buf, 1, sizeof(buf), ifp)) != 0)
		if (fwrite(buf, 1, nr, ofp) != nr) {
			cwarn("%s", out);
			goto err;
		}

	if (ferror(ifp) || fclose(ifp)) {
		cwarn("%s", in);
		goto err;
	}
	ifp = NULL;

	if (fclose(ofp)) {
		cwarn("%s", out);
		goto err;
	}

	if (isreg) {
		setfile(out, &sb);

		if (unlink(in))
			cwarn("%s", in);
	}
	return;

err:	if (ofp) {
		if (oreg)
			(void)unlink(out);
		(void)fclose(ofp);
	}
	if (ifp)
		(void)fclose(ifp);
}

void
setfile(name, fs)
	char *name;
	register struct stat *fs;
{
	static struct timeval tv[2];

	fs->st_mode &= S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;

	TIMESPEC_TO_TIMEVAL(&tv[0], &fs->st_atimespec);
	TIMESPEC_TO_TIMEVAL(&tv[1], &fs->st_mtimespec);
	if (utimes(name, tv))
		cwarn("utimes: %s", name);

	/*
	 * Changing the ownership probably won't succeed, unless we're root
	 * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
	 * the mode; current BSD behavior is to remove all setuid bits on
	 * chown.  If chown fails, lose setuid/setgid bits.
	 */
	if (chown(name, fs->st_uid, fs->st_gid)) {
		if (errno != EPERM)
			cwarn("chown: %s", name);
		fs->st_mode &= ~(S_ISUID|S_ISGID);
	}
	if (chmod(name, fs->st_mode))
		cwarn("chown: %s", name);

	if (chflags(name, fs->st_flags))
		cwarn("chflags: %s", name);
}

int
permission(fname)
	char *fname;
{
	int ch, first;

	if (!isatty(fileno(stderr)))
		return (0);
	(void)fprintf(stderr, "overwrite %s? ", fname);
	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	return (first == 'y');
}

void
usage(iscompress)
	int iscompress;
{
	if (iscompress)
		(void)fprintf(stderr,
		    "usage: compress [-cfv] [-b bits] [file ...]\n");
	else
		(void)fprintf(stderr,
		    "usage: uncompress [-c] [-b bits] [file ...]\n");
	exit(1);
}

void
#if __STDC__
cwarnx(const char *fmt, ...)
#else
cwarnx(fmt, va_alist)
	int eval;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vwarnx(fmt, ap);
	va_end(ap);
	eval = 1;
}

void
#if __STDC__
cwarn(const char *fmt, ...)
#else
cwarn(fmt, va_alist)
	int eval;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vwarn(fmt, ap);
	va_end(ap);
	eval = 1;
}
