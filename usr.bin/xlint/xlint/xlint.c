/* $NetBSD: xlint.c,v 1.17.2.1 2000/06/23 16:40:22 minoura Exp $ */

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All Rights Reserved.
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: xlint.c,v 1.17.2.1 2000/06/23 16:40:22 minoura Exp $");
#endif

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <paths.h>

#include "lint.h"
#include "pathnames.h"

int main __P((int, char *[]));

/* directory for temporary files */
static	const	char *tmpdir;

/* path name for cpp output */
static	char	*cppout;

/* file descriptor for cpp output */
static	int	cppoutfd = -1;

/* files created by 1st pass */
static	char	**p1out;

/* input files for 2nd pass (without libraries) */
static	char	**p2in;

/* library which will be created by 2nd pass */
static	char	*p2out;

/* flags always passed to cc(1) */
static	char	**cflags;

/* flags for cc(1), controled by sflag/tflag */
static	char	**lcflags;

/* flags for lint1 */
static	char	**l1flags;

/* flags for lint2 */
static	char	**l2flags;

/* libraries for lint2 */
static	char	**l2libs;

/* default libraries */
static	char	**deflibs;

/* additional libraries */
static	char	**libs;

/* search path for libraries */
static	char	**libsrchpath;

static  char	*libexec_path;

/* flags */
static	int	iflag, oflag, Cflag, sflag, tflag, Fflag, dflag, Bflag;

/* print the commands executed to run the stages of compilation */
static	int	Vflag;

/* filename for oflag */
static	char	*outputfn;

/* reset after first .c source has been processed */
static	int	first = 1;

/*
 * name of a file which is currently written by a child and should
 * be removed after abnormal termination of the child
 */
static	const	char *currfn;


static	void	appstrg __P((char ***, char *));
static	void	appcstrg __P((char ***, const char *));
static	void	applst __P((char ***, char *const *));
static	void	freelst __P((char ***));
static	char	*concat2 __P((const char *, const char *));
static	char	*concat3 __P((const char *, const char *, const char *));
static	void	terminate __P((int)) __attribute__((__noreturn__));
static	const	char *basename __P((const char *, int));
static	void	appdef __P((char ***, const char *));
static	void	usage __P((void));
static	void	fname __P((const char *, int));
static	void	runchild __P((const char *, char *const *, const char *, int));
static	void	findlibs __P((char *const *));
static	int	rdok __P((const char *));
static	void	lint2 __P((void));
static	void	cat __P((char *const *, const char *));

/*
 * Some functions to deal with lists of strings.
 * Take care that we get no surprises in case of asyncron signals.
 */
static void
appstrg(lstp, s)
	char	***lstp, *s;
{
	char	**lst, **olst;
	int	i;

	olst = *lstp;
	for (i = 0; olst[i] != NULL; i++) ;
	lst = xrealloc(olst, (i + 2) * sizeof (char *));
	lst[i] = s;
	lst[i + 1] = NULL;
	*lstp = lst;
}	

static void
appcstrg(lstp, s)
	char	***lstp;
	const	char *s;
{
	appstrg(lstp, xstrdup(s));
}

static void
applst(destp, src)
	char	***destp;
	char	*const *src;
{
	int	i, k;
	char	**dest, **odest;

	odest = *destp;
	for (i = 0; odest[i] != NULL; i++) ;
	for (k = 0; src[k] != NULL; k++) ;
	dest = xrealloc(odest, (i + k + 1) * sizeof (char *));
	for (k = 0; src[k] != NULL; k++)
		dest[i + k] = xstrdup(src[k]);
	dest[i + k] = NULL;
	*destp = dest;
}

static void
freelst(lstp)
	char	***lstp;
{
	char	*s;
	int	i;

	for (i = 0; (*lstp)[i] != NULL; i++) ;
	while (i-- > 0) {
		s = (*lstp)[i];
		(*lstp)[i] = NULL;
		free(s);
	}
}

static char *
concat2(s1, s2)
	const	char *s1, *s2;
{
	char	*s;

	s = xmalloc(strlen(s1) + strlen(s2) + 1);
	(void)strcpy(s, s1);
	(void)strcat(s, s2);

	return (s);
}

static char *
concat3(s1, s2, s3)
	const	char *s1, *s2, *s3;
{
	char	*s;

	s = xmalloc(strlen(s1) + strlen(s2) + strlen(s3) + 1);
	(void)strcpy(s, s1);
	(void)strcat(s, s2);
	(void)strcat(s, s3);

	return (s);
}

/*
 * Clean up after a signal.
 */
static void
terminate(signo)
	int	signo;
{
	int	i;

	if (cppoutfd != -1)
		(void)close(cppoutfd);
	if (cppout != NULL)
		(void)remove(cppout);

	if (p1out != NULL) {
		for (i = 0; p1out[i] != NULL; i++)
			(void)remove(p1out[i]);
	}

	if (p2out != NULL)
		(void)remove(p2out);

	if (currfn != NULL)
		(void)remove(currfn);

	exit(signo != 0 ? 1 : 0);
}

/*
 * Returns a pointer to the last component of strg after delim.
 * Returns strg if the string does not contain delim.
 */
static const char *
basename(strg, delim)
	const	char *strg;
	int	delim;
{
	const	char *cp, *cp1, *cp2;

	cp = cp1 = cp2 = strg;
	while (*cp != '\0') {
		if (*cp++ == delim) {
			cp2 = cp1;
			cp1 = cp;
		}
	}
	return (*cp1 == '\0' ? cp2 : cp1);
}

static void
appdef(lstp, def)
	char	***lstp;
	const	char *def;
{
	appstrg(lstp, concat2("-D__", def));
	appstrg(lstp, concat3("-D__", def, "__"));
}

static void
usage()
{
	extern char *__progname;
	(void)fprintf(stderr,
    "Usage: %s [-abceghprvxzHF] [-s|-t] [-i|-nu] [-Dname[=def]] [-Uname]\n",
	    __progname);
	(void)fprintf(stderr, 
    "\t[-Idirectory] [-Ldirectory] [-llibrary] [-ooutputfile] file ...\n");
	(void)fprintf(stderr,
    "       %s [-abceghprvzHF] [-s|-t] -Clibrary [-Dname[=def]]\n", __progname);
	(void)fprintf(stderr, "\t[-Idirectory] [-Uname] [-Bpath] file ...\n");
	terminate(-1);
}


int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	int	c;
	char	flgbuf[3], *tmp, *s;
	size_t	len;

	if ((tmp = getenv("TMPDIR")) == NULL || (len = strlen(tmp)) == 0) {
		tmpdir = xstrdup(_PATH_TMP);
	} else {
		s = xmalloc(len + 2);
		(void)sprintf(s, "%s%s", tmp, tmp[len - 1] == '/' ? "" : "/");
		tmpdir = s;
	}

	cppout = xmalloc(strlen(tmpdir) + sizeof ("lint0.XXXXXX"));
	(void)sprintf(cppout, "%slint0.XXXXXX", tmpdir);
	cppoutfd = mkstemp(cppout);
	if (cppoutfd == -1) {
		warn("can't make temp");
		terminate(-1);
	}

	p1out = xcalloc(1, sizeof (char *));
	p2in = xcalloc(1, sizeof (char *));
	cflags = xcalloc(1, sizeof (char *));
	lcflags = xcalloc(1, sizeof (char *));
	l1flags = xcalloc(1, sizeof (char *));
	l2flags = xcalloc(1, sizeof (char *));
	l2libs = xcalloc(1, sizeof (char *));
	deflibs = xcalloc(1, sizeof (char *));
	libs = xcalloc(1, sizeof (char *));
	libsrchpath = xcalloc(1, sizeof (char *));

	appcstrg(&cflags, "-E");
	appcstrg(&cflags, "-x");
	appcstrg(&cflags, "c");
#if 0
	appcstrg(&cflags, "-D__attribute__(x)=");
	appcstrg(&cflags, "-D__extension__(x)=/*NOSTRICT*/0");
#else
	appcstrg(&cflags, "-U__GNUC__");
#endif
	appcstrg(&cflags, "-Wp,-$");
	appcstrg(&cflags, "-Wp,-CC");
	appcstrg(&cflags, "-Wcomment");
	appcstrg(&cflags, "-D__LINT__");
	appcstrg(&cflags, "-Dlint");		/* XXX don't def. with -s */

	appdef(&cflags, "lint");

	appcstrg(&lcflags, "-Wtraditional");

	appcstrg(&deflibs, "c");

	if (signal(SIGHUP, terminate) == SIG_IGN)
		(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, terminate);
	(void)signal(SIGQUIT, terminate);
	(void)signal(SIGTERM, terminate);

	while (argc > optind) {

		argc -= optind;
		argv += optind;
		optind = 0;

		c = getopt(argc, argv, "abcd:eghil:no:prstuvxzB:C:D:FHI:L:U:V");

		switch (c) {

		case 'a':
		case 'b':
		case 'c':
		case 'e':
		case 'g':
		case 'r':
		case 'v':
		case 'z':
			(void)sprintf(flgbuf, "-%c", c);
			appcstrg(&l1flags, flgbuf);
			break;

		case 'F':
			Fflag = 1;
			/* FALLTHROUGH */
		case 'u':
		case 'h':
			(void)sprintf(flgbuf, "-%c", c);
			appcstrg(&l1flags, flgbuf);
			appcstrg(&l2flags, flgbuf);
			break;

		case 'i':
			if (Cflag)
				usage();
			iflag = 1;
			break;

		case 'n':
			freelst(&deflibs);
			break;

		case 'p':
			appcstrg(&l1flags, "-p");
			appcstrg(&l2flags, "-p");
			if (*deflibs != NULL) {
				freelst(&deflibs);
				appcstrg(&deflibs, "c");
			}
			break;

		case 's':
			if (tflag)
				usage();
			freelst(&lcflags);
			appcstrg(&lcflags, "-trigraphs");
			appcstrg(&lcflags, "-Wtrigraphs");
			appcstrg(&lcflags, "-pedantic");
			appcstrg(&lcflags, "-D__STRICT_ANSI__");
			appcstrg(&l1flags, "-s");
			appcstrg(&l2flags, "-s");
			sflag = 1;
			break;

		case 't':
			if (sflag)
				usage();
			freelst(&lcflags);
			appcstrg(&lcflags, "-traditional");
			appstrg(&lcflags, concat2("-D", MACHINE));
			appstrg(&lcflags, concat2("-D", MACHINE_ARCH));
			appcstrg(&l1flags, "-t");
			appcstrg(&l2flags, "-t");
			tflag = 1;
			break;

		case 'x':
			appcstrg(&l2flags, "-x");
			break;

		case 'C':
			if (Cflag || oflag || iflag)
				usage();
			Cflag = 1;
			appstrg(&l2flags, concat2("-C", optarg));
			p2out = xmalloc(sizeof ("llib-l.ln") + strlen(optarg));
			(void)sprintf(p2out, "llib-l%s.ln", optarg);
			freelst(&deflibs);
			break;

		case 'd':
			if (dflag)
				usage();
			dflag = 1;
			appcstrg(&cflags, "-nostdinc");
			appcstrg(&cflags, "-idirafter");
			appcstrg(&cflags, optarg);
			break;
			
		case 'D':
		case 'I':
		case 'U':
			(void)sprintf(flgbuf, "-%c", c);
			appstrg(&cflags, concat2(flgbuf, optarg));
			break;

		case 'l':
			appcstrg(&libs, optarg);
			break;

		case 'o':
			if (Cflag || oflag)
				usage();
			oflag = 1;
			outputfn = xstrdup(optarg);
			break;

		case 'L':
			appcstrg(&libsrchpath, optarg);
			break;

		case 'H':
			appcstrg(&l2flags, "-H");
			break;

		case 'B':
			Bflag = 1;
			libexec_path = xstrdup(optarg);
			break;

		case 'V':
			Vflag = 1;
			break;

		case '?':
			usage();
			/* NOTREACHED */

		case -1:
			/* filename */
			fname(argv[0], argc == 1);
			first = 0;
			optind = 1;
		}

	}

	if (first)
		usage();

	if (iflag)
		terminate(0);

	if (!oflag) {
		if ((s = getenv("LIBDIR")) == NULL || strlen(s) == 0)
			s = PATH_LINTLIB;
		appcstrg(&libsrchpath, s);
		findlibs(libs);
		findlibs(deflibs);
	}

	(void)printf("Lint pass2:\n");
	lint2();

	if (oflag)
		cat(p2in, outputfn);

	if (Cflag)
		p2out = NULL;

	terminate(0);
	/* NOTREACHED */
}

/*
 * Read a file name from the command line
 * and pass it through lint1 if it is a C source.
 */
static void
fname(name, last)
	const	char *name;
	int	last;
{
	const	char *bn, *suff;
	char	**args, *ofn, *path;
	size_t	len;
	int is_stdin;
	int	fd;

	is_stdin = (strcmp(name, "-") == 0);
	bn = basename(name, '/');
	suff = basename(bn, '.');

	if (strcmp(suff, "ln") == 0) {
		/* only for lint2 */
		if (!iflag)
			appcstrg(&p2in, name);
		return;
	}

	if (!is_stdin && strcmp(suff, "c") != 0 &&
	    (strncmp(bn, "llib-l", 6) != 0 || bn != suff)) {
		warnx("unknown file type: %s\n", name);
		return;
	}

	if (!iflag || !first || !last)
		(void)printf("%s:\n",
		    is_stdin ? "{standard input}" : Fflag ? name : bn);

	/* build the name of the output file of lint1 */
	if (oflag) {
		ofn = outputfn;
		outputfn = NULL;
		oflag = 0;
	} else if (iflag) {
		if (is_stdin) {
			warnx("-i not supported without -o for standard input");
			return;
		}
		ofn = xmalloc(strlen(bn) + (bn == suff ? 4 : 2));
		len = bn == suff ? strlen(bn) : (suff - 1) - bn;
		(void)sprintf(ofn, "%.*s", (int)len, bn);
		(void)strcat(ofn, ".ln");
	} else {
		ofn = xmalloc(strlen(tmpdir) + sizeof ("lint1.XXXXXX"));
		(void)sprintf(ofn, "%slint1.XXXXXX", tmpdir);
		fd = mkstemp(ofn);
		if (fd == -1) {
			warn("can't make temp");
			terminate(-1);
		}
		close(fd);
	}
	if (!iflag)
		appcstrg(&p1out, ofn);

	args = xcalloc(1, sizeof (char *));

	/* run cc */

	if (getenv("CC") == NULL) {
		path = xmalloc(strlen(PATH_USRBIN) + sizeof ("/cc"));
		(void)sprintf(path, "%s/cc", PATH_USRBIN);
	} else {
		path = strdup(getenv("CC"));
	}

	appcstrg(&args, path);
	applst(&args, cflags);
	applst(&args, lcflags);
	appcstrg(&args, name);

	/* we reuse the same tmp file for cpp output, so rewind and truncate */
	if (lseek(cppoutfd, SEEK_SET, (off_t)0) != 0) {
		warn("lseek");
		terminate(-1);
	}
	if (ftruncate(cppoutfd, (off_t)0) != 0) {
		warn("ftruncate");
		terminate(-1);
	}
	 
	runchild(path, args, cppout, cppoutfd);
	free(path);
	freelst(&args);

	/* run lint1 */

	if (!Bflag) {
		path = xmalloc(strlen(PATH_LIBEXEC) + sizeof ("/lint1"));
		(void)sprintf(path, "%s/lint1", PATH_LIBEXEC);
	} else {
		path = xmalloc(strlen(libexec_path) + sizeof ("/lint1"));
		(void)sprintf(path, "%s/lint1", libexec_path);
	}

	appcstrg(&args, path);
	applst(&args, l1flags);
	appcstrg(&args, cppout);
	appcstrg(&args, ofn);

	runchild(path, args, ofn, -1);
	free(path);
	freelst(&args);

	appcstrg(&p2in, ofn);
	free(ofn);

	free(args);
}

static void
runchild(path, args, crfn, fdout)
	const	char *path, *crfn;
	char	*const *args;
	int	fdout;
{
	int	status, rv, signo, i;

	if (Vflag) {
		for (i = 0; args[i] != NULL; i++)
			(void)printf("%s ", args[i]);
		(void)printf("\n");
	}

	currfn = crfn;

	(void)fflush(stdout);

	switch (vfork()) {
	case -1:
		warn("cannot fork");
		terminate(-1);
		/* NOTREACHED */
	default:
		/* parent */
		break;
	case 0:
		/* child */

		/* setup the standard output if necessary */
		if (fdout != -1) {
			dup2(fdout, STDOUT_FILENO);
			close(fdout);
		}
		(void)execv(path, args);
		warn("cannot exec %s", path);
		_exit(1);
		/* NOTREACHED */
	}

	while ((rv = wait(&status)) == -1 && errno == EINTR) ;
	if (rv == -1) {
		warn("wait");
		terminate(-1);
	}
	if (WIFSIGNALED(status)) {
		signo = WTERMSIG(status);
		warnx("%s got SIG%s", path, sys_signame[signo]);
		terminate(-1);
	}
	if (WEXITSTATUS(status) != 0)
		terminate(-1);
	currfn = NULL;
}

static void
findlibs(liblst)
	char	*const *liblst;
{
	int	i, k;
	const	char *lib, *path;
	char	*lfn;
	size_t	len;

	lfn = NULL;

	for (i = 0; (lib = liblst[i]) != NULL; i++) {
		for (k = 0; (path = libsrchpath[k]) != NULL; k++) {
			len = strlen(path) + strlen(lib);
			lfn = xrealloc(lfn, len + sizeof ("/llib-l.ln"));
			(void)sprintf(lfn, "%s/llib-l%s.ln", path, lib);
			if (rdok(lfn))
				break;
			lfn = xrealloc(lfn, len + sizeof ("/lint/llib-l.ln"));
			(void)sprintf(lfn, "%s/lint/llib-l%s.ln", path, lib);
			if (rdok(lfn))
				break;
		}
		if (path != NULL) {
			appstrg(&l2libs, concat2("-l", lfn));
		} else {
			warnx("cannot find llib-l%s.ln", lib);
		}
	}

	free(lfn);
}

static int
rdok(path)
	const	char *path;
{
	struct	stat sbuf;

	if (stat(path, &sbuf) == -1)
		return (0);
	if (!S_ISREG(sbuf.st_mode))
		return (0);
	if (access(path, R_OK) == -1)
		return (0);
	return (1);
}

static void
lint2()
{
	char	*path, **args;

	args = xcalloc(1, sizeof (char *));

	if (!Bflag) {
		path = xmalloc(strlen(PATH_LIBEXEC) + sizeof ("/lint2"));
		(void)sprintf(path, "%s/lint2", PATH_LIBEXEC);
	} else {
		path = xmalloc(strlen(libexec_path) + sizeof ("/lint2"));
		(void)sprintf(path, "%s/lint2", libexec_path);
	}		
	
	appcstrg(&args, path);
	applst(&args, l2flags);
	applst(&args, l2libs);
	applst(&args, p2in);

	runchild(path, args, p2out, -1);
	free(path);
	freelst(&args);
	free(args);
}

static void
cat(srcs, dest)
	char	*const *srcs;
	const	char *dest;
{
	int	ifd, ofd, i;
	char	*src, *buf;
	ssize_t	rlen;

	if ((ofd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
		warn("cannot open %s", dest);
		terminate(-1);
	}

	buf = xmalloc(MBLKSIZ);

	for (i = 0; (src = srcs[i]) != NULL; i++) {
		if ((ifd = open(src, O_RDONLY)) == -1) {
			free(buf);
			warn("cannot open %s", src);
			terminate(-1);
		}
		do {
			if ((rlen = read(ifd, buf, MBLKSIZ)) == -1) {
				free(buf);
				warn("read error on %s", src);
				terminate(-1);
			}
			if (write(ofd, buf, (size_t)rlen) == -1) {
				free(buf);
				warn("write error on %s", dest);
				terminate(-1);
			}
		} while (rlen == MBLKSIZ);
		(void)close(ifd);
	}
	(void)close(ofd);
	free(buf);
}

