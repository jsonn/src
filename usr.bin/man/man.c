/*	$NetBSD: man.c,v 1.22.2.1 2000/06/23 16:39:48 minoura Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994, 1995
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

#include <sys/cdefs.h>

#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1987, 1993, 1994, 1995\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)man.c	8.17 (Berkeley) 1/31/95";
#else
__RCSID("$NetBSD: man.c,v 1.22.2.1 2000/06/23 16:39:48 minoura Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/utsname.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "pathnames.h"

int f_all, f_where;

int		 main __P((int, char **));
static void	 build_page __P((char *, char **));
static void	 cat __P((char *));
static const char	*check_pager __P((const char *));
static int	 cleanup __P((void));
static void	 how __P((char *));
static void	 jump __P((char **, char *, char *));
static int	 manual __P((char *, TAG *, glob_t *, const char *));
static void	 onsig __P((int));
static void	 usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	TAG *defp, *section, *newpathp, *subp;
	ENTRY *e_defp, *e_subp;
	glob_t pg;
	size_t len;
	int ch, f_cat, f_how, found, abs_section;
	char **ap, *cmd, *p, *p_add, *p_path;
	const char *machine, *pager, *conffile, *pathsearch, *sectionname;
	char buf[MAXPATHLEN * 2];

#ifdef __GNUC__
	pager = NULL;		/* XXX gcc -Wuninitialized */
#endif

	f_cat = f_how = 0;
	sectionname = pathsearch = conffile = p_add = p_path = NULL;
	while ((ch = getopt(argc, argv, "-aC:cfhkM:m:P:s:S:w")) != -1)
		switch (ch) {
		case 'a':
			f_all = 1;
			break;
		case 'C':
			conffile = optarg;
			break;
		case 'c':
		case '-':		/* Deprecated. */
			f_cat = 1;
			break;
		case 'h':
			f_how = 1;
			break;
		case 'm':
			p_add = optarg;
			break;
		case 'M':
		case 'P':		/* Backward compatibility. */
			p_path = strdup(optarg);
			break;
		/*
		 * The -f and -k options are backward compatible,
		 * undocumented ways of calling whatis(1) and apropos(1).
		 */
		case 'f':
			jump(argv, "-f", "whatis");
			/* NOTREACHED */
		case 'k':
			jump(argv, "-k", "apropos");
			/* NOTREACHED */
		case 's':
			if (sectionname != NULL)
				usage();
			sectionname = optarg;
			break;
		case 'S':
			pathsearch = optarg;
			break;
		case 'w':
			f_all = f_where = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!argc)
		usage();

	if (!f_cat && !f_how && !f_where) {
		if (!isatty(STDOUT_FILENO)) {
			f_cat = 1;
		} else {
			if ((pager = getenv("PAGER")) != NULL &&
			    pager[0] != '\0')
				pager = check_pager(pager);
			else
				pager = _PATH_PAGER;
		}
	}

	/* Read the configuration file. */
	config(conffile);

	/* Get the machine type. */
	if ((machine = getenv("MACHINE")) == NULL) {
		struct utsname utsname;

		if (uname(&utsname) == -1) {
			perror("uname");
			exit(1);
		}
		machine = utsname.machine;
	}

	/* create an empty _default list if the config file didn't have one */
	if ((defp = getlist("_default")) == NULL)
		defp = addlist("_default");

	/* if -M wasn't specified, check for MANPATH */
	if (p_path == NULL)
		p_path = getenv("MANPATH");

	/*
	 * get section.  abs_section will be non-zero iff the user
	 * specified a section and it had absolute (rather than
	 * relative) paths in the man.conf file.
	 */
	if ((argc > 1 || sectionname != NULL) &&
	    (section = getlist(sectionname ? sectionname : *argv)) != NULL) {
		if (sectionname == NULL) {
			argv++;
			argc--;
		}
		abs_section = (TAILQ_FIRST(&section->list) != NULL &&
		    *(TAILQ_FIRST(&section->list)->s) == '/');
	} else {
		section = NULL;
		abs_section = 0;
	}

	/* get subdir list */
	subp = getlist("_subdir");
	if (!subp)
		subp = addlist("_subdir");

	/*
	 * now that we have all the inputs we must generate a search path.
	 */

	/*
	 * 1: If user specified a section and it has absolute paths
	 * in the config file, then that overrides _default, MANPATH and
	 * path passed via -M.
	 */
	if (abs_section) {
		p_path = NULL;		/* zap -M/MANPATH */
		defp = section;		/* zap _default */
		section = NULL;		/* promoted to defp */
	}


	/*
	 * 2: Section can be non-null only if a section was specified
	 * and the config file has relative paths - the section list
	 * overrides _subdir in this case.
	 */
	if (section)
		subp = section;


	/*
	 * 3: now we either have text string path (p_path) or a tag
	 * based path (defp).   we need to append subp and machine
	 * to each element in the path.
	 *
	 * for backward compat, we do not append subp if abs_section
	 * and the path does not end in "/".
	 */
	newpathp = addlist("_new_path");
	if (p_path) {
		/* use p_path */
		for (; (p = strtok(p_path, ":")) != NULL; p_path = NULL) {
			for ( e_subp = TAILQ_FIRST(&subp->list) ;
			      e_subp != NULL ;
			      e_subp = TAILQ_NEXT(e_subp, q)) {
				snprintf(buf, sizeof(buf), "%s/%s{/%s,}",
					 p, e_subp->s, machine);
				addentry(newpathp, buf, 0);
			}
		}
	} else {
		/* use defp rather than p_path */
		for (e_defp = TAILQ_FIRST(&defp->list) ;
		     e_defp != NULL ;
		     e_defp = TAILQ_NEXT(e_defp, q)) {

			/* handle trailing "/" magic here ... */
		  	if (abs_section &&
			    e_defp->s[strlen(e_defp->s) - 1] != '/') {

				(void)snprintf(buf, sizeof(buf),
				    "%s{/%s,}", e_defp->s, machine);
				addentry(newpathp, buf, 0);
				continue;
			}

			for ( e_subp = TAILQ_FIRST(&subp->list) ;
			      e_subp != NULL ;
			      e_subp = TAILQ_NEXT(e_subp, q)) {
				snprintf(buf, sizeof(buf), "%s%s%s{/%s,}",
					 e_defp->s, (abs_section) ? "" : "/",
					 e_subp->s, machine);
				addentry(newpathp, buf, 0);
			}
		}
	}				/* using defp ... */

	/* now replace the current path with the new one */
	defp = newpathp;

	/*
	 * 4: prepend the "-m" path, if specified.   we always add
	 * subp and machine to this part of the path.
	 */

	if (p_add) {
		for (p = strtok(p_add, ":") ; p ; p = strtok(NULL, ":")) {
			for ( e_subp = TAILQ_FIRST(&subp->list) ;
			      e_subp != NULL ;
			      e_subp = TAILQ_NEXT(e_subp, q)) {
				snprintf(buf, sizeof(buf), "%s/%s{/%s,}",
					 p, e_subp->s, machine);
				addentry(newpathp, buf, 1);
			}
		}
	}


	/*
	 * 5: Search for the files.  Set up an interrupt handler, so the
	 *    temporary files go away.
	 */
	(void)signal(SIGINT, onsig);
	(void)signal(SIGHUP, onsig);
	(void)signal(SIGPIPE, onsig);

	memset(&pg, 0, sizeof(pg));
	for (found = 0; *argv; ++argv)
		if (manual(*argv, defp, &pg, pathsearch))
			found = 1;

	/* 6: If nothing found, we're done. */
	if (!found) {
		(void)cleanup();
		exit (1);
	}

	/* 7: If it's simple, display it fast. */
	if (f_cat) {
		for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
			if (**ap == '\0')
				continue;
			cat(*ap);
		}
		exit (cleanup());
	}
	if (f_how) {
		for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
			if (**ap == '\0')
				continue;
			how(*ap);
		}
		exit(cleanup());
	}
	if (f_where) {
		for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
			if (**ap == '\0')
				continue;
			(void)printf("%s\n", *ap);
		}
		exit(cleanup());
	}
		
	/*
	 * 8: We display things in a single command; build a list of things
	 *    to display.
	 */
	for (ap = pg.gl_pathv, len = strlen(pager) + 1; *ap != NULL; ++ap) {
		if (**ap == '\0')
			continue;
		len += strlen(*ap) + 1;
	}
	if ((cmd = malloc(len)) == NULL) {
		warn("malloc");
		(void)cleanup();
		exit(1);
	}
	p = cmd;
	len = strlen(pager);
	memmove(p, pager, len);
	p += len;
	*p++ = ' ';
	for (ap = pg.gl_pathv; *ap != NULL; ++ap) {
		if (**ap == '\0')
			continue;
		len = strlen(*ap);
		memmove(p, *ap, len);
		p += len;
		*p++ = ' ';
	}
	*--p = '\0';

	/* Use system(3) in case someone's pager is "pager arg1 arg2". */
	(void)system(cmd);

	exit(cleanup());
}

/*
 * manual --
 *	Search the manuals for the pages.
 */
static int
manual(page, tag, pg, pathsearch)
	char *page;
	TAG *tag;
	glob_t *pg;
	const char *pathsearch;
{
	ENTRY *ep, *e_sufp, *e_tag;
	TAG *missp, *sufp;
	int anyfound, cnt, error, found;
	char *p, buf[MAXPATHLEN];

	anyfound = 0;
	buf[0] = '*';

	/* For each element in the list... */
	e_tag = tag == NULL ? NULL : tag->list.tqh_first;
	for (; e_tag != NULL; e_tag = e_tag->q.tqe_next) {
		(void)snprintf(buf, sizeof(buf), "%s/%s.*", e_tag->s, page);
		if ((error = glob(buf,
		    GLOB_APPEND | GLOB_BRACE | GLOB_NOSORT, NULL, pg)) != 0) {
			if (error == GLOB_NOMATCH)
				continue;
			else {
				warn("globbing");
				(void)cleanup();
				exit(1);
			}
		}
		if (pg->gl_matchc == 0)
			continue;

		/* Find out if it's really a man page. */
		for (cnt = pg->gl_pathc - pg->gl_matchc;
		    cnt < pg->gl_pathc; ++cnt) {

			if (pathsearch) {
				p = strstr(pg->gl_pathv[cnt], pathsearch);
				if (!p || strchr(p, '/') == NULL) {
					pg->gl_pathv[cnt] = "";
					continue;
				}
			}

			/*
			 * Try the _suffix key words first.
			 *
			 * XXX
			 * Older versions of man.conf didn't have the suffix
			 * key words, it was assumed that everything was a .0.
			 * We just test for .0 first, it's fast and probably
			 * going to hit.
			 */
			(void)snprintf(buf, sizeof(buf), "*/%s.0", page);
			if (!fnmatch(buf, pg->gl_pathv[cnt], 0))
				goto next;

			e_sufp = (sufp = getlist("_suffix")) == NULL ?
			    NULL : sufp->list.tqh_first;
			for (found = 0;
			    e_sufp != NULL; e_sufp = e_sufp->q.tqe_next) {
				(void)snprintf(buf,
				     sizeof(buf), "*/%s%s", page, e_sufp->s);
				if (!fnmatch(buf, pg->gl_pathv[cnt], 0)) {
					found = 1;
					break;
				}
			}
			if (found)
				goto next;

			/* Try the _build key words next. */
			e_sufp = (sufp = getlist("_build")) == NULL ?
			    NULL : sufp->list.tqh_first;
			for (found = 0;
			    e_sufp != NULL; e_sufp = e_sufp->q.tqe_next) {
				for (p = e_sufp->s;
				    *p != '\0' && !isspace((unsigned char)*p); ++p);
				if (*p == '\0')
					continue;
				*p = '\0';
				(void)snprintf(buf,
				     sizeof(buf), "*/%s%s", page, e_sufp->s);
				if (!fnmatch(buf, pg->gl_pathv[cnt], 0)) {
					if (!f_where)
						build_page(p + 1,
						    &pg->gl_pathv[cnt]);
					*p = ' ';
					found = 1;
					break;
				}
				*p = ' ';
			}
			if (found) {
next:				anyfound = 1;
				if (!f_all) {
					/* Delete any other matches. */
					while (++cnt< pg->gl_pathc)
						pg->gl_pathv[cnt] = "";
					break;
				}
				continue;
			}

			/* It's not a man page, forget about it. */
			pg->gl_pathv[cnt] = "";
		}

		if (anyfound && !f_all)
			break;
	}

	/* If not found, enter onto the missing list. */
	if (!anyfound) {
		if ((missp = getlist("_missing")) == NULL)
			missp = addlist("_missing");
		if ((ep = malloc(sizeof(ENTRY))) == NULL ||
		    (ep->s = strdup(page)) == NULL) {
			warn("malloc");
			(void)cleanup();
			exit(1);
		}
		TAILQ_INSERT_TAIL(&missp->list, ep, q);
	}
	return (anyfound);
}

/* 
 * build_page --
 *	Build a man page for display.
 */
static void
build_page(fmt, pathp)
	char *fmt, **pathp;
{
	static int warned;
	ENTRY *ep;
	TAG *intmpp;
	int fd, n;
	char *p, *b;
	char buf[MAXPATHLEN], cmd[MAXPATHLEN], tpath[MAXPATHLEN];
	const char *tmpdir;

	/* Let the user know this may take awhile. */
	if (!warned) {
		warned = 1;
		warnx("Formatting manual page...");
	}

       /*
        * Historically man chdir'd to the root of the man tree. 
        * This was used in man pages that contained relative ".so"
        * directives (including other man pages for command aliases etc.)
        * It even went one step farther, by examining the first line
        * of the man page and parsing the .so filename so it would
        * make hard(?) links to the cat'ted man pages for space savings.
        * (We don't do that here, but we could).
        */
 
       /* copy and find the end */
       for (b = buf, p = *pathp; (*b++ = *p++) != '\0';)
               continue;
 
	/* skip the last two path components, page name and man[n] */
	for (--b, --p, n = 2; b != buf; b--, p--)
		if (*b == '/')
			if (--n == 0) {
				*b = '\0';
				(void) chdir(buf);
				p++;
				break;
			}


	/* Add a remove-when-done list. */
	if ((intmpp = getlist("_intmp")) == NULL)
		intmpp = addlist("_intmp");

	/* Move to the printf(3) format string. */
	for (; *fmt && isspace((unsigned char)*fmt); ++fmt)
		continue;

	/*
	 * Get a temporary file and build a version of the file
	 * to display.  Replace the old file name with the new one.
	 */
	if ((tmpdir = getenv("TMPDIR")) == NULL)
		tmpdir = _PATH_TMP;
	(void)snprintf(tpath, sizeof (tpath), "%s/%s", tmpdir, TMPFILE);
	if ((fd = mkstemp(tpath)) == -1) {
		warn("%s", tpath);
		(void)cleanup();
		exit(1);
	}
	(void)snprintf(buf, sizeof(buf), "%s > %s", fmt, tpath);
	(void)snprintf(cmd, sizeof(cmd), buf, p);
	(void)system(cmd);
	(void)close(fd);
	if ((*pathp = strdup(tpath)) == NULL) {
		warn("malloc");
		(void)cleanup();
		exit(1);
	}

	/* Link the built file into the remove-when-done list. */
	if ((ep = malloc(sizeof(ENTRY))) == NULL) {
		warn("malloc");
		(void)cleanup();
		exit(1);
	}
	ep->s = *pathp;
	TAILQ_INSERT_TAIL(&intmpp->list, ep, q);
}

/*
 * how --
 *	display how information
 */
static void
how(fname)
	char *fname;
{
	FILE *fp;

	int lcnt, print;
	char *p, buf[256];

	if (!(fp = fopen(fname, "r"))) {
		warn("%s", fname);
		(void)cleanup();
		exit (1);
	}
#define	S1	"SYNOPSIS"
#define	S2	"S\bSY\bYN\bNO\bOP\bPS\bSI\bIS\bS"
#define	D1	"DESCRIPTION"
#define	D2	"D\bDE\bES\bSC\bCR\bRI\bIP\bPT\bTI\bIO\bON\bN"
	for (lcnt = print = 0; fgets(buf, sizeof(buf), fp);) {
		if (!strncmp(buf, S1, sizeof(S1) - 1) ||
		    !strncmp(buf, S2, sizeof(S2) - 1)) {
			print = 1;
			continue;
		} else if (!strncmp(buf, D1, sizeof(D1) - 1) ||
		    !strncmp(buf, D2, sizeof(D2) - 1))
			return;
		if (!print)
			continue;
		if (*buf == '\n')
			++lcnt;
		else {
			for(; lcnt; --lcnt)
				(void)putchar('\n');
			for (p = buf; isspace((unsigned char)*p); ++p)
				continue;
			(void)fputs(p, stdout);
		}
	}
	(void)fclose(fp);
}

/*
 * cat --
 *	cat out the file
 */
static void
cat(fname)
	char *fname;
{
	int fd, n;
	char buf[2048];

	if ((fd = open(fname, O_RDONLY, 0)) < 0) {
		warn("%s", fname);
		(void)cleanup();
		exit(1);
	}
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		if (write(STDOUT_FILENO, buf, n) != n) {
			warn("write");
			(void)cleanup();
			exit (1);
		}
	if (n == -1) {
		warn("read");
		(void)cleanup();
		exit(1);
	}
	(void)close(fd);
}

/*
 * check_pager --
 *	check the user supplied page information
 */
static const char *
check_pager(name)
	const char *name;
{
	const char *p;

	/*
	 * if the user uses "more", we make it "more -s"; watch out for
	 * PAGER = "mypager /usr/ucb/more"
	 */
	for (p = name; *p && !isspace((unsigned char)*p); ++p)
		continue;
	for (; p > name && *p != '/'; --p);
	if (p != name)
		++p;

	/* make sure it's "more", not "morex" */
	if (!strncmp(p, "more", 4) && (!p[4] || isspace((unsigned char)p[4]))){
		char *newname;
		(void)asprintf(&newname, "%s %s", p, "-s");
		name = newname;
	}

	return (name);
}

/*
 * jump --
 *	strip out flag argument and jump
 */
static void
jump(argv, flag, name)
	char **argv, *flag, *name;
{
	char **arg;

	argv[0] = name;
	for (arg = argv + 1; *arg; ++arg)
		if (!strcmp(*arg, flag))
			break;
	for (; *arg; ++arg)
		arg[0] = arg[1];
	execvp(name, argv);
	(void)fprintf(stderr, "%s: Command not found.\n", name);
	exit(1);
}

/* 
 * onsig --
 *	If signaled, delete the temporary files.
 */
static void
onsig(signo)
	int signo;
{
	sigset_t set;

	(void)cleanup();

	(void)signal(signo, SIG_DFL);

	/* unblock the signal */
	sigemptyset(&set);
	sigaddset(&set, signo);
	sigprocmask(SIG_UNBLOCK, &set, (sigset_t *) NULL);

	(void)kill(getpid(), signo);

	/* NOTREACHED */
	exit (1);
}

/*
 * cleanup --
 *	Clean up temporary files, show any error messages.
 */
static int
cleanup()
{
	TAG *intmpp, *missp;
	ENTRY *ep;
	int rval;

	rval = 0;
	ep = (missp = getlist("_missing")) == NULL ?
	    NULL : missp->list.tqh_first;
	if (ep != NULL)
		for (; ep != NULL; ep = ep->q.tqe_next) {
			warnx("no entry for %s in the manual.", ep->s);
			rval = 1;
		}

	ep = (intmpp = getlist("_intmp")) == NULL ?
	    NULL : intmpp->list.tqh_first;
	for (; ep != NULL; ep = ep->q.tqe_next)
		(void)unlink(ep->s);
	return (rval);
}

/*
 * usage --
 *	print usage message and die
 */
static void
usage()
{
	extern char *__progname;
	(void)fprintf(stderr, "Usage: %s [-achw] [-C file] [-M path] [-m path]"
	    "[-S srch] [[-s] section] title ...\n", __progname);
	exit(1);
}
