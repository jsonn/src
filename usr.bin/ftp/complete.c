/*	$NetBSD: complete.c,v 1.21.2.1 1999/06/25 01:14:57 cgd Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: complete.c,v 1.21.2.1 1999/06/25 01:14:57 cgd Exp $");
#endif /* not lint */

/*
 * FTP user program - command and file completion routines
 */

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftp_var.h"

#ifndef NO_EDITCOMPLETE

static int	     comparstr		__P((const void *, const void *));
static unsigned char complete_ambiguous	__P((char *, int, StringList *));
static unsigned char complete_command	__P((char *, int));
static unsigned char complete_local	__P((char *, int));
static unsigned char complete_remote	__P((char *, int));

static int
comparstr(a, b)
	const void *a, *b;
{
	return (strcmp(*(char **)a, *(char **)b));
}

/*
 * Determine if complete is ambiguous. If unique, insert.
 * If no choices, error. If unambiguous prefix, insert that.
 * Otherwise, list choices. words is assumed to be filtered
 * to only contain possible choices.
 * Args:
 *	word	word which started the match
 *	list	list by default
 *	words	stringlist containing possible matches
 * Returns a result as per el_set(EL_ADDFN, ...)
 */
static unsigned char
complete_ambiguous(word, list, words)
	char *word;
	int list;
	StringList *words;
{
	char insertstr[MAXPATHLEN];
	char *lastmatch;
	int i, j;
	size_t matchlen, wordlen;

	wordlen = strlen(word);
	if (words->sl_cur == 0)
		return (CC_ERROR);	/* no choices available */

	if (words->sl_cur == 1) {	/* only once choice available */
		char *p = words->sl_str[0] + wordlen;
		ftpvis(insertstr, sizeof(insertstr), p, strlen(p));
		if (el_insertstr(el, insertstr) == -1)
			return (CC_ERROR);
		else
			return (CC_REFRESH);
	}

	if (!list) {
		matchlen = 0;
		lastmatch = words->sl_str[0];
		matchlen = strlen(lastmatch);
		for (i = 1 ; i < words->sl_cur ; i++) {
			for (j = wordlen ; j < strlen(words->sl_str[i]); j++)
				if (lastmatch[j] != words->sl_str[i][j])
					break;
			if (j < matchlen)
				matchlen = j;
		}
		if (matchlen > wordlen) {
			ftpvis(insertstr, sizeof(insertstr),
			    lastmatch + wordlen, matchlen - wordlen);
			if (el_insertstr(el, insertstr) == -1)
				return (CC_ERROR);
			else
				return (CC_REFRESH_BEEP);
		}
	}

	putc('\n', ttyout);
	qsort(words->sl_str, words->sl_cur, sizeof(char *), comparstr);
	list_vertical(words);
	return (CC_REDISPLAY);
}

/*
 * Complete a command
 */
static unsigned char
complete_command(word, list)
	char *word;
	int list;
{
	struct cmd *c;
	StringList *words;
	size_t wordlen;
	unsigned char rv;

	words = sl_init();
	wordlen = strlen(word);

	for (c = cmdtab; c->c_name != NULL; c++) {
		if (wordlen > strlen(c->c_name))
			continue;
		if (strncmp(word, c->c_name, wordlen) == 0)
			sl_add(words, c->c_name);
	}

	rv = complete_ambiguous(word, list, words);
	if (rv == CC_REFRESH) {
		if (el_insertstr(el, " ") == -1)
			rv = CC_ERROR;
	}
	sl_free(words, 0);
	return (rv);
}

/*
 * Complete a local file
 */
static unsigned char
complete_local(word, list)
	char *word;
	int list;
{
	StringList *words;
	char dir[MAXPATHLEN];
	char *file;
	DIR *dd;
	struct dirent *dp;
	unsigned char rv;
	size_t len;

	if ((file = strrchr(word, '/')) == NULL) {
		dir[0] = '.';
		dir[1] = '\0';
		file = word;
	} else {
		if (file == word) {
			dir[0] = '/';
			dir[1] = '\0';
		} else {
			(void)strncpy(dir, word, file - word);
			dir[file - word] = '\0';
		}
		file++;
	}
	if (dir[0] == '~') {
		char *p;

		p = dir;
		if (!globulize(&p))
			return (CC_ERROR);
		if (p != dir) {
			strncpy(dir, p, sizeof(dir));
			dir[sizeof(dir)-1] = '\0';
			free(p);
		}
	}

	if ((dd = opendir(dir)) == NULL)
		return (CC_ERROR);

	words = sl_init();

	len = strlen(file);

	for (dp = readdir(dd); dp != NULL; dp = readdir(dd)) {
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;

#if defined(__SVR4) || defined(__linux__)
		if (len > strlen(dp->d_name))
			continue;
#else
		if (len > dp->d_namlen)
			continue;
#endif
		if (strncmp(file, dp->d_name, len) == 0) {
			char *tcp;

			tcp = xstrdup(dp->d_name);
			sl_add(words, tcp);
		}
	}
	closedir(dd);

	rv = complete_ambiguous(file, list, words);
	if (rv == CC_REFRESH) {
		struct stat sb;
		char path[MAXPATHLEN];

		snprintf(path, sizeof(path), "%s/%s", dir, words->sl_str[0]);
		if (stat(path, &sb) >= 0) {
			char suffix[2] = " ";

			if (S_ISDIR(sb.st_mode))
				suffix[0] = '/';
			if (el_insertstr(el, suffix) == -1)
				rv = CC_ERROR;
		}
	}
	sl_free(words, 1);
	return (rv);
}

/*
 * Complete a remote file
 */
static unsigned char
complete_remote(word, list)
	char *word;
	int list;
{
	static StringList *dirlist;
	static char	 lastdir[MAXPATHLEN];
	StringList	*words;
	char		 dir[MAXPATHLEN];
	char		*file, *cp;
	int		 i;
	unsigned char	 rv;

	char *dummyargv[] = { "complete", NULL, NULL };
	dummyargv[1] = dir;

	if ((file = strrchr(word, '/')) == NULL) {
		dir[0] = '.';
		dir[1] = '\0';
		file = word;
	} else {
		cp = file;
		while (*cp == '/' && cp > word)
			cp--;
		(void)strncpy(dir, word, cp - word + 1);
		dir[cp - word + 1] = '\0';
		file++;
	}

	if (dirchange || strcmp(dir, lastdir) != 0) {	/* dir not cached */
		char *emesg;

		if (dirlist != NULL)
			sl_free(dirlist, 1);
		dirlist = sl_init();

		mflag = 1;
		emesg = NULL;
		while ((cp = remglob(dummyargv, 0, &emesg)) != NULL) {
			char *tcp;

			if (!mflag)
				continue;
			if (*cp == '\0') {
				mflag = 0;
				continue;
			}
			tcp = strrchr(cp, '/');
			if (tcp)
				tcp++;
			else
				tcp = cp;
			tcp = xstrdup(tcp);
			sl_add(dirlist, tcp);
		}
		if (emesg != NULL) {
			fprintf(ttyout, "\n%s\n", emesg);
			return (CC_REDISPLAY);
		}
		(void)strcpy(lastdir, dir);
		dirchange = 0;
	}

	words = sl_init();
	for (i = 0; i < dirlist->sl_cur; i++) {
		cp = dirlist->sl_str[i];
		if (strlen(file) > strlen(cp))
			continue;
		if (strncmp(file, cp, strlen(file)) == 0)
			sl_add(words, cp);
	}
	rv = complete_ambiguous(file, list, words);
	sl_free(words, 0);
	return (rv);
}

/*
 * Generic complete routine
 */
unsigned char
complete(el, ch)
	EditLine *el;
	int ch;
{
	static char word[FTPBUFLEN];
	static int lastc_argc, lastc_argo;

	struct cmd *c;
	const LineInfo *lf;
	int celems, dolist;
	size_t len;

	lf = el_line(el);
	len = lf->lastchar - lf->buffer;
	if (len >= sizeof(line))
		return (CC_ERROR);
	(void)strncpy(line, lf->buffer, len);
	line[len] = '\0';
	cursor_pos = line + (lf->cursor - lf->buffer);
	lastc_argc = cursor_argc;	/* remember last cursor pos */
	lastc_argo = cursor_argo;
	makeargv();			/* build argc/argv of current line */

	if (cursor_argo >= sizeof(word))
		return (CC_ERROR);

	dolist = 0;
			/* if cursor and word is same, list alternatives */
	if (lastc_argc == cursor_argc && lastc_argo == cursor_argo
	    && strncmp(word, margv[cursor_argc], cursor_argo) == 0)
		dolist = 1;
	else
		(void)strncpy(word, margv[cursor_argc], cursor_argo);
	word[cursor_argo] = '\0';

	if (cursor_argc == 0)
		return (complete_command(word, dolist));

	c = getcmd(margv[0]);
	if (c == (struct cmd *)-1 || c == 0)
		return (CC_ERROR);
	celems = strlen(c->c_complete);

		/* check for 'continuation' completes (which are uppercase) */
	if ((cursor_argc > celems) && (celems > 0)
	    && isupper((unsigned char) c->c_complete[celems-1]))
		cursor_argc = celems;

	if (cursor_argc > celems)
		return (CC_ERROR);

	switch (c->c_complete[cursor_argc - 1]) {
		case 'l':			/* local complete */
		case 'L':
			return (complete_local(word, dolist));
		case 'r':			/* remote complete */
		case 'R':
			if (connected != -1) {
				fputs("\nMust be logged in to complete.\n",
				    ttyout);
				return (CC_REDISPLAY);
			}
			return (complete_remote(word, dolist));
		case 'c':			/* command complete */
		case 'C':
			return (complete_command(word, dolist));
		case 'n':			/* no complete */
		default:
			return (CC_ERROR);
	}

	return (CC_ERROR);
}

#endif /* !NO_EDITCOMPLETE */
