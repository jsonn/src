/*	$NetBSD: find.c,v 1.18.2.1.2.2 2005/10/11 23:47:05 reed Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Cimarron D. Taylor of the University of California, Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
static char sccsid[] = "from: @(#)find.c	8.5 (Berkeley) 8/5/94";
#else
__RCSID("$NetBSD: find.c,v 1.18.2.1.2.2 2005/10/11 23:47:05 reed Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "find.h"

static int ftscompare __P((const FTSENT **, const FTSENT **));

static void sig_lock __P((sigset_t *));
static void sig_unlock __P((const sigset_t *));

/*
 * find_formplan --
 *	process the command line and create a "plan" corresponding to the
 *	command arguments.
 */
PLAN *
find_formplan(argv)
	char **argv;
{
	PLAN *plan, *tail, *new;

	/*
	 * for each argument in the command line, determine what kind of node
	 * it is, create the appropriate node type and add the new plan node
	 * to the end of the existing plan.  The resulting plan is a linked
	 * list of plan nodes.  For example, the string:
	 *
	 *	% find . -name foo -newer bar -print
	 *
	 * results in the plan:
	 *
	 *	[-name foo]--> [-newer bar]--> [-print]
	 *
	 * in this diagram, `[-name foo]' represents the plan node generated
	 * by c_name() with an argument of foo and `-->' represents the
	 * plan->next pointer.
	 */
	for (plan = tail = NULL; *argv;) {
		if (!(new = find_create(&argv)))
			continue;
		if (plan == NULL)
			tail = plan = new;
		else {
			tail->next = new;
			tail = new;
		}
	}

	/*
	 * if the user didn't specify one of -print, -ok or -exec, then -print
	 * is assumed so we bracket the current expression with parens, if
	 * necessary, and add a -print node on the end.
	 */
	if (!isoutput) {
		if (plan == NULL) {
			new = c_print(NULL, 0);
			tail = plan = new;
		} else {
			new = c_openparen(NULL, 0);
			new->next = plan;
			plan = new;
			new = c_closeparen(NULL, 0);
			tail->next = new;
			tail = new;
			new = c_print(NULL, 0);
			tail->next = new;
			tail = new;
		}
	}

	/*
	 * the command line has been completely processed into a search plan
	 * except for the (, ), !, and -o operators.  Rearrange the plan so
	 * that the portions of the plan which are affected by the operators
	 * are moved into operator nodes themselves.  For example:
	 *
	 *	[!]--> [-name foo]--> [-print]
	 *
	 * becomes
	 *
	 *	[! [-name foo] ]--> [-print]
	 *
	 * and
	 *
	 *	[(]--> [-depth]--> [-name foo]--> [)]--> [-print]
	 *
	 * becomes
	 *
	 *	[expr [-depth]-->[-name foo] ]--> [-print]
	 *
	 * operators are handled in order of precedence.
	 */

	plan = paren_squish(plan);		/* ()'s */
	plan = not_squish(plan);		/* !'s */
	plan = or_squish(plan);			/* -o's */
	return (plan);
}

static int
ftscompare(e1, e2)
	const FTSENT **e1, **e2;
{

	return (strcoll((*e1)->fts_name, (*e2)->fts_name));
}

static void
sig_lock(s)
	sigset_t *s;
{
	sigset_t new;

	sigemptyset(&new);
	sigaddset(&new, SIGINFO); /* block SIGINFO */
	sigprocmask(SIG_BLOCK, &new, s);
}

static void
sig_unlock(s)
	const sigset_t *s;
{

	sigprocmask(SIG_SETMASK, s, NULL);
}

FTS *tree;			/* pointer to top of FTS hierarchy */
FTSENT *g_entry;		/* shared with SIGINFO handler */

/*
 * find_execute --
 *	take a search plan and an array of search paths and executes the plan
 *	over all FTSENT's returned for the given search paths.
 */
int
find_execute(plan, paths)
	PLAN *plan;		/* search plan */
	char **paths;		/* array of pathnames to traverse */
{
	PLAN *p;
	int rval;
	sigset_t s;

	if (!(tree = fts_open(paths, ftsoptions, issort ? ftscompare : NULL)))
		err(1, "ftsopen");

	sig_lock(&s);
	for (rval = 0; (g_entry = fts_read(tree)) != NULL; sig_lock(&s)) {
		sig_unlock(&s);
		switch (g_entry->fts_info) {
		case FTS_D:
			if (isdepth)
				continue;
			break;
		case FTS_DP:
			if (!isdepth)
				continue;
			break;
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			(void)fflush(stdout);
			warnx("%s: %s",
			    g_entry->fts_path, strerror(g_entry->fts_errno));
			rval = 1;
			continue;
		}
#define	BADCH	" \t\n\\'\""
		if (isxargs && strpbrk(g_entry->fts_path, BADCH)) {
			(void)fflush(stdout);
			warnx("%s: illegal path", g_entry->fts_path);
			rval = 1;
			continue;
		}

		/*
		 * Call all the functions in the execution plan until one is
		 * false or all have been executed.  This is where we do all
		 * the work specified by the user on the command line.
		 */
		for (p = plan; p && (p->eval)(p, g_entry); p = p->next)
			;
	}
	sig_unlock(&s);
	if (errno)
		err(1, "fts_read");
	(void)fts_close(tree);
	return (rval);
}
