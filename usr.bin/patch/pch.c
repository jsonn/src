/*	$NetBSD: pch.c,v 1.20.2.1 2006/06/19 04:17:07 chap Exp $	*/

/*
 * Copyright (c) 1988, Larry Wall
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following condition
 * is met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this condition and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
__RCSID("$NetBSD: pch.c,v 1.20.2.1 2006/06/19 04:17:07 chap Exp $");
#endif /* not lint */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "INTERN.h"
#include "pch.h"

#include <stdlib.h>
#include <unistd.h>

/* Patch (diff listing) abstract type. */

static long p_filesize;			/* size of the patch file */
static LINENUM p_first;			/* 1st line number */
static LINENUM p_newfirst;		/* 1st line number of replacement */
static LINENUM p_ptrn_lines;		/* # lines in pattern */
static LINENUM p_repl_lines;		/* # lines in replacement text */
static LINENUM p_end = -1;		/* last line in hunk */
static LINENUM p_max;			/* max allowed value of p_end */
static LINENUM p_context = 3;		/* # of context lines */
static LINENUM p_input_line = 0;	/* current line # from patch file */
static char **p_line = NULL;		/* the text of the hunk */
static size_t *p_len = NULL;		/* length of each line */
static char *p_char = NULL;		/* +, -, and ! */
static int hunkmax = INITHUNKMAX;	/* size of above arrays */
static int p_indent;			/* indent to patch */
static long p_base;			/* where to intuit this time */
static LINENUM p_bline;			/* line # of p_base */
static long p_start;			/* where intuit found a patch */
static LINENUM p_sline;			/* and the line number for it */
static LINENUM p_hunk_beg;		/* line number of current hunk */
static LINENUM p_efake = -1;		/* end of faked up lines--don't free */
static LINENUM p_bfake = -1;		/* beg of faked up lines */
static FILE *pfp = NULL;		/* patch file pointer */

/* Prepare to look for the next patch in the patch file. */
static void malformed(void);

void
re_patch(void)
{
	p_first = Nulline;
	p_newfirst = Nulline;
	p_ptrn_lines = Nulline;
	p_repl_lines = Nulline;
	p_end = -1;
	p_max = Nulline;
	p_indent = 0;
}

/* 
 * Open the patch file at the beginning of time.
 */
void
open_patch_file(char *filename)
{
	if (filename == NULL || !*filename || strEQ(filename, "-")) {
		pfp = fopen(TMPPATNAME, "w");
		if (pfp == NULL)
			pfatal("can't create %s", TMPPATNAME);
		while (fgets(buf, sizeof buf, stdin) != NULL)
			fputs(buf, pfp);
		Fclose(pfp);
		filename = TMPPATNAME;
	}
	pfp = fopen(filename, "r");
	if (pfp == NULL)
		pfatal("patch file %s not found", filename);
	Fstat(fileno(pfp), &filestat);
	p_filesize = filestat.st_size;
	next_intuit_at(0L, 1);			/* start at the beginning */
	set_hunkmax();
}

/*
 * Make sure our dynamically realloced tables are malloced to begin with.
 */
void
set_hunkmax(void)
{
	if (p_line == NULL)
		p_line = xmalloc(hunkmax * sizeof(char *));
	if (p_len == NULL)
		p_len  = xmalloc(hunkmax * sizeof(size_t));
	if (p_char == NULL)
		p_char = xmalloc(hunkmax * sizeof(char));
}

/*
 * Enlarge the arrays containing the current hunk of patch.
 */
void
grow_hunkmax(void)
{
	hunkmax *= 2;

	p_line = xrealloc(p_line, hunkmax * sizeof(char *));
	p_len  = xrealloc(p_len,  hunkmax * sizeof(size_t));
	p_char = xrealloc(p_char, hunkmax * sizeof(char));
}

/*
 * True if the remainder of the patch file contains a diff of some sort.
 */
bool
there_is_another_patch(void)
{
	if (p_base != 0L && p_base >= p_filesize) {
		if (verbose)
			say("done\n");
		return FALSE;
	}
	if (verbose)
		say("Hmm...");
	diff_type = intuit_diff_type();
	if (!diff_type) {
		if (p_base != 0L) {
			if (verbose)
				say("  Ignoring the trailing garbage.\n"
				    "done\n");
		} else
			say("  I can't seem to find a patch in there"
			    " anywhere.\n");
		return FALSE;
	}
	if (verbose)
		say("  %sooks like %s to me...\n",
		    (p_base == 0L ? "L" : "The next patch l"),
		    diff_type == UNI_DIFF ? "a unified diff" :
		    diff_type == CONTEXT_DIFF ? "a context diff" :
		    diff_type == NEW_CONTEXT_DIFF ?
		    "a new-style context diff" :
		    diff_type == NORMAL_DIFF ? "a normal diff" :
		    "an ed script" );
	if (p_indent && verbose)
		say("(Patch is indented %d space%s.)\n",
		    p_indent, p_indent == 1 ? "" : "s");
	skip_to(p_start, p_sline);
	while (filearg[0] == NULL) {
		if (force || batch) {
			say("No file to patch.  Skipping...\n");
			filearg[0] = xstrdup(bestguess);
			skip_rest_of_patch = TRUE;
			return TRUE;
		}
		ask("File to patch: ");
		if (*buf != '\n') {
			if (bestguess)
				free(bestguess);
			bestguess = xstrdup(buf);
			filearg[0] = fetchname(buf, 0, FALSE);
		}
		if (filearg[0] == NULL) {
			ask("No file found--skip this patch? [n] ");
			if (*buf != 'y')
				continue;
			if (verbose)
				say("Skipping patch...\n");
			filearg[0] = fetchname(bestguess, 0, TRUE);
			skip_rest_of_patch = TRUE;
			return TRUE;
		}
	}
	return TRUE;
}

/*
 * Determine what kind of diff is in the remaining part of the patch file.
 */
int
intuit_diff_type(void)
{
	long this_line = 0;
	long previous_line;
	long first_command_line = -1;
	LINENUM fcl_line = -1;
	bool last_line_was_command = FALSE;
	bool this_is_a_command = FALSE;
	bool stars_last_line = FALSE;
	bool stars_this_line = FALSE;
	int indent;
	char *s;
	char *t;
	char *indtmp = NULL;
	char *oldtmp = NULL;
	char *newtmp = NULL;
	char *indname = NULL;
	char *oldname = NULL;
	char *newname = NULL;
	int retval;
	bool no_filearg = (filearg[0] == NULL);

	ok_to_create_file = FALSE;
	old_file_is_dev_null = FALSE;
	Fseek(pfp, p_base, 0);
	p_input_line = p_bline - 1;
	for (;;) {
		previous_line = this_line;
		last_line_was_command = this_is_a_command;
		stars_last_line = stars_this_line;
		this_line = ftell(pfp);
		indent = 0;
		p_input_line++;
		if (fgets(buf, sizeof buf, pfp) == NULL) {
			if (first_command_line >= 0L) {
				/* nothing but deletes!? */
				p_start = first_command_line;
				p_sline = fcl_line;
				retval = ED_DIFF;
				goto scan_exit;
			} else {
				p_start = this_line;
				p_sline = p_input_line;
				retval = 0;
				goto scan_exit;
			}
		}
		for (s = buf; *s == ' ' || *s == '\t' || *s == 'X'; s++) {
			if (*s == '\t')
				indent += 8 - (indent % 8);
			else
				indent++;
		}
		for (t = s; isdigit((unsigned char)*t) || *t == ','; t++)
			;
		this_is_a_command =
		    isdigit((unsigned char)*s) &&
		    (*t == 'd' || *t == 'c' || *t == 'a');
		if (first_command_line < 0L && this_is_a_command) { 
			first_command_line = this_line;
			fcl_line = p_input_line;
			p_indent = indent;	/* assume this for now */
		}
		if (!stars_last_line && strnEQ(s, "*** ", 4)) {
			if (oldtmp)
				free(oldtmp);
			oldtmp = xstrdup(s + 4);
		} else if (strnEQ(s, "--- ", 4)) {
			if (newtmp)
				free(newtmp);
			newtmp = xstrdup(s + 4);
		} else if (strnEQ(s, "+++ ", 4)) {
			if (oldtmp)
				free(oldtmp);
			oldtmp = xstrdup(s + 4);	/* pretend it is the old name */
		} else if (strnEQ(s, "Index:", 6)) {
			if (indtmp)
				free(indtmp);
			indtmp = xstrdup(s + 6);
		} else if (strnEQ(s, "Prereq:", 7)) {
			for (t = s + 7; isspace((unsigned char)*t); t++)
				;
			if (revision)
				free(revision);
			revision = xstrdup(t);
			for (t = revision;
			     *t && !isspace((unsigned char)*t);
			     t++)
				;
			*t = '\0';
			if (*revision == '\0') {
				free(revision);
				revision = NULL;
			}
		}
		if ((!diff_type || diff_type == ED_DIFF) &&
		    first_command_line >= 0L &&
		    strEQ(s, ".\n") ) {
			p_indent = indent;
			p_start = first_command_line;
			p_sline = fcl_line;
			retval = ED_DIFF;
			goto scan_exit;
		}
		if ((!diff_type || diff_type == UNI_DIFF) &&
		    strnEQ(s, "@@ -", 4)) {
			if (!atol(s + 3))
				ok_to_create_file = TRUE;
			p_indent = indent;
			p_start = this_line;
			p_sline = p_input_line;
			retval = UNI_DIFF;
			goto scan_exit;
		}
		stars_this_line = strnEQ(s, "********", 8);
		if ((!diff_type || diff_type == CONTEXT_DIFF) &&
		    stars_last_line &&
		    strnEQ(s, "*** ", 4)) {
			if (!atol(s + 4))
				ok_to_create_file = TRUE;
			/*
			 * If this is a new context diff the character just
			 * before the newline is a '*'.
			 */
			while (*s != '\n')
				s++;
			p_indent = indent;
			p_start = previous_line;
			p_sline = p_input_line - 1;
			retval = (*(s - 1) == '*' ?
				  NEW_CONTEXT_DIFF : CONTEXT_DIFF);
			goto scan_exit;
		}
		if ((!diff_type || diff_type == NORMAL_DIFF) && 
		    last_line_was_command &&
		    (strnEQ(s, "< ", 2) || strnEQ(s, "> ", 2)) ) {
			p_start = previous_line;
			p_sline = p_input_line - 1;
			p_indent = indent;
			retval = NORMAL_DIFF;
			goto scan_exit;
		}
	}
 scan_exit:
	if (no_filearg) {
		if (indtmp != NULL)
			indname = fetchname(indtmp,
					    strippath,
					    ok_to_create_file);
		if (oldtmp != NULL) {
			oldname = fetchname(oldtmp,
					    strippath,
					    ok_to_create_file);
			old_file_is_dev_null = filename_is_dev_null;
		}
		if (newtmp != NULL)
			newname = fetchname(newtmp,
					    strippath,
					    ok_to_create_file);
		if (oldname && newname) {
			if (strlen(oldname) < strlen(newname))
				filearg[0] = xstrdup(oldname);
			else
				filearg[0] = xstrdup(newname);
		}
		else if (oldname)
			filearg[0] = xstrdup(oldname);
		else if (newname)
			filearg[0] = xstrdup(newname);
		else if (indname)
			filearg[0] = xstrdup(indname);
	}
	if (bestguess) {
		free(bestguess);
		bestguess = NULL;
	}
	if (filearg[0] != NULL)
		bestguess = xstrdup(filearg[0]);
	else if (indtmp != NULL)
		bestguess = fetchname(indtmp, strippath, TRUE);
	else {
		if (oldtmp != NULL) {
			oldname = fetchname(oldtmp, strippath, TRUE);
			old_file_is_dev_null = filename_is_dev_null;
		}
		if (newtmp != NULL) {
			if (newname)
				free(newname);
			newname = fetchname(newtmp, strippath, TRUE);
		}
		if (oldname && newname) {
			if (strlen(oldname) < strlen(newname))
				bestguess = xstrdup(oldname);
			else
				bestguess = xstrdup(newname);
		}
		else if (oldname)
			bestguess = xstrdup(oldname);
		else if (newname)
			bestguess = xstrdup(newname);
	}
	if (indtmp != NULL)
		free(indtmp);
	if (oldtmp != NULL)
		free(oldtmp);
	if (newtmp != NULL)
		free(newtmp);
	if (indname != NULL)
		free(indname);
	if (oldname != NULL)
		free(oldname);
	if (newname != NULL)
		free(newname);
	return retval;
}

/*
 * Remember where this patch ends so we know where to start up again.
 */
void
next_intuit_at(long file_pos, LINENUM file_line)
{
	p_base = file_pos;
	p_bline = file_line;
}

/*
 * Basically a verbose fseek() to the actual diff listing.
 */
void
skip_to(long file_pos, LINENUM file_line)
{
	char *ret;

	if (p_base > file_pos)
		fatal("seeked too far %ld > %ld\n", p_base, file_pos);
	if (verbose && p_base < file_pos) {
		Fseek(pfp, p_base, 0);
		say("The text leading up to this was:\n"
		    "--------------------------\n");
		while (ftell(pfp) < file_pos) {
			ret = fgets(buf, sizeof buf, pfp);
			if (ret == NULL)
				fatal("Unexpected end of file\n");
			say("|%s", buf);
		}
		say("--------------------------\n");
	}
	else
		Fseek(pfp, file_pos, 0);
	p_input_line = file_line - 1;
}

/*
 * Make this a function for better debugging.
 */
static void
malformed(void)
{
	fatal("malformed patch at line %d: %s", p_input_line, buf);
		/* about as informative as "Syntax error" in C */
}

/*
 * True if the line has been discarded (i.e. it is a line saying
 *  "\ No newline at end of file".)
 */
static bool
remove_special_line(void)
{
	int c;

	c = fgetc(pfp);
	if (c == '\\') {
		do {
			c = fgetc(pfp);
		} while (c != EOF && c != '\n');

		return TRUE;
	}

	if (c != EOF)
		fseek(pfp, -1L, SEEK_CUR);

	return FALSE;
}

/*
 * True if there is more of the current diff listing to process.
 */
bool
another_hunk(void)
{
    char *s;
    char *ret;
    int context = 0;

    while (p_end >= 0) {
	if (p_end == p_efake)
	    p_end = p_bfake;		/* don't free twice */
	else
	    free(p_line[p_end]);
	p_end--;
    }
    if (p_end != -1)
	fatal("Internal error\n");
    p_efake = -1;

    p_max = hunkmax;			/* gets reduced when --- found */
    if (diff_type == CONTEXT_DIFF || diff_type == NEW_CONTEXT_DIFF) {
	long line_beginning = ftell(pfp);
					/* file pos of the current line */
	LINENUM repl_beginning = 0;	/* index of --- line */
	LINENUM fillcnt = 0;		/* #lines of missing ptrn or repl */
	LINENUM fillsrc = 0;		/* index of first line to copy */
	LINENUM filldst = 0;		/* index of first missing line */
	bool ptrn_spaces_eaten = FALSE;	/* ptrn was slightly misformed */
	bool repl_could_be_missing = TRUE;
					/* no + or ! lines in this hunk */
	bool repl_missing = FALSE;	/* we are now backtracking */
	long repl_backtrack_position = 0;
					/* file pos of first repl line */
	LINENUM repl_patch_line = 0;	/* input line number for same */
	LINENUM ptrn_copiable = 0;	/* # of copiable lines in ptrn */

	ret = pgets(buf, sizeof buf, pfp);
	p_input_line++;
	if (ret == NULL || strnNE(buf, "********", 8)) {
	    next_intuit_at(line_beginning,p_input_line);
	    return FALSE;
	}
	p_context = 100;
	p_hunk_beg = p_input_line + 1;
	while (p_end < p_max) {
	    line_beginning = ftell(pfp);
	    ret = pgets(buf, sizeof buf, pfp);
	    p_input_line++;
	    if (ret == NULL) {
		if (p_max - p_end < 4) {
		    /* assume blank lines got chopped */
		    strlcpy(buf, "  \n", sizeof(buf));
		} else {
		    if (repl_beginning && repl_could_be_missing) {
			repl_missing = TRUE;
			goto hunk_done;
		    }
		    fatal("unexpected end of file in patch\n");
		}
	    }
	    p_end++;
	    if (p_end >= hunkmax)
		fatal("hunk larger than current buffer size\n");
	    p_char[p_end] = *buf;
	    p_line[p_end] = NULL;
	    switch (*buf) {
	    case '*':
		if (strnEQ(buf, "********", 8)) {
		    if (repl_beginning && repl_could_be_missing) {
			repl_missing = TRUE;
			goto hunk_done;
		    }
		    else
			fatal("unexpected end of hunk at line %d\n",
			    p_input_line);
		}
		if (p_end != 0) {
		    if (repl_beginning && repl_could_be_missing) {
			repl_missing = TRUE;
			goto hunk_done;
		    }
		    fatal("unexpected *** at line %d: %s", p_input_line, buf);
		}
		context = 0;
		p_line[p_end] = xstrdup(buf);
		for (s = buf; *s && !isdigit((unsigned char)*s); s++)
			;
		if (!*s)
		    malformed();
		if (strnEQ(s, "0,0", 3))
		    strlcpy(s, s + 2, sizeof(buf) - (s - buf));
		p_first = atoi(s);
		while (isdigit((unsigned char)*s))
			s++;
		if (*s == ',') {
		    for (; *s && !isdigit((unsigned char)*s); s++)
			    ;
		    if (!*s)
			malformed();
		    p_ptrn_lines = atoi(s) - p_first + 1;
		} else if (p_first)
		    p_ptrn_lines = 1;
		else {
		    p_ptrn_lines = 0;
		    p_first = 1;
		}
		p_max = p_ptrn_lines + 6;  /* we need this much at least */
		while (p_max >= hunkmax)
		    grow_hunkmax();
		p_max = hunkmax;
		break;
	    case '-':
		if (buf[1] == '-') {
		    if (repl_beginning ||
			(p_end !=
			     p_ptrn_lines + 1 + (p_char[p_end - 1] == '\n'))) {
			if (p_end == 1) {
			    /* `old' lines were omitted - set up to fill */
			    /* them in from 'new' context lines. */
			    p_end = p_ptrn_lines + 1;
			    fillsrc = p_end + 1;
			    filldst = 1;
			    fillcnt = p_ptrn_lines;
			} else {
			    if (repl_beginning) {
				if (repl_could_be_missing) {
				    repl_missing = TRUE;
				    goto hunk_done;
				}
				fatal("duplicate \"---\" at line %d"
				      "--check line numbers at line %d\n",
				      p_input_line,
				      p_hunk_beg + repl_beginning);
			    } else {
				fatal("%s \"---\" at line %d"
				      "--check line numbers at line %d\n",
				      (p_end <= p_ptrn_lines
				       ? "Premature"
				       : "Overdue" ),
				      p_input_line, p_hunk_beg);
			    }
			}
		    }
		    repl_beginning = p_end;
		    repl_backtrack_position = ftell(pfp);
		    repl_patch_line = p_input_line;
		    p_line[p_end] = xstrdup(buf);
		    p_char[p_end] = '=';
		    for (s = buf; *s && !isdigit((unsigned char)*s); s++)
			    ;
		    if (!*s)
			malformed();
		    p_newfirst = atoi(s);
		    while (isdigit((unsigned char)*s))
			    s++;
		    if (*s == ',') {
			for (; *s && !isdigit((unsigned char)*s); s++)
				;
			if (!*s)
			    malformed();
			p_repl_lines = atoi(s) - p_newfirst + 1;
		    } else if (p_newfirst)
			p_repl_lines = 1;
		    else {
			p_repl_lines = 0;
			p_newfirst = 1;
		    }
		    p_max = p_repl_lines + p_end;
		    if (p_max > MAXHUNKSIZE)
			fatal("hunk too large (%d lines) at line %d: %s",
			      p_max, p_input_line, buf);
		    while (p_max >= hunkmax)
			grow_hunkmax();
		    if (p_repl_lines != ptrn_copiable
			&& (p_context != 0 || p_repl_lines != 1))
			repl_could_be_missing = FALSE;
		    break;
		}
		goto change_line;
	    case '+':  case '!':
		repl_could_be_missing = FALSE;
	    change_line:
		if (buf[1] == '\n' && canonicalize)
		    strlcpy(buf + 1," \n", sizeof(buf) - 1);
		if (!isspace((unsigned char)buf[1]) &&
		    buf[1] != '>' && buf[1] != '<' &&
		    repl_beginning && repl_could_be_missing) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		if (context >= 0) {
		    if (context < p_context)
			p_context = context;
		    context = -1000;
		}
		p_line[p_end] = xstrdup(buf + 2);
		if (p_end == p_ptrn_lines)
		{
			if (remove_special_line()) {
				int len;

				len = strlen(p_line[p_end]) - 1;
				(p_line[p_end])[len] = 0;
			}
		}
		break;
	    case '\t': case '\n':	/* assume the 2 spaces got eaten */
		if (repl_beginning && repl_could_be_missing &&
		    (!ptrn_spaces_eaten || diff_type == NEW_CONTEXT_DIFF)) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		p_line[p_end] = xstrdup(buf);
		if (p_end != p_ptrn_lines + 1) {
		    ptrn_spaces_eaten |= (repl_beginning != 0);
		    context++;
		    if (!repl_beginning)
			ptrn_copiable++;
		    p_char[p_end] = ' ';
		}
		break;
	    case ' ':
		if (!isspace((unsigned char)buf[1]) &&
		    repl_beginning && repl_could_be_missing) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		context++;
		if (!repl_beginning)
		    ptrn_copiable++;
		p_line[p_end] = xstrdup(buf + 2);
		break;
	    default:
		if (repl_beginning && repl_could_be_missing) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		malformed();
	    }
	    /* set up p_len for strncmp() so we don't have to */
	    /* assume null termination */
	    if (p_line[p_end])
		p_len[p_end] = strlen(p_line[p_end]);
	    else
		p_len[p_end] = 0;
	}
	
    hunk_done:
	if (p_end >= 0 && !repl_beginning)
	    fatal("no --- found in patch at line %d\n", pch_hunk_beg());

	if (repl_missing) {	    
	    /* reset state back to just after --- */
	    p_input_line = repl_patch_line;
	    for (p_end--; p_end > repl_beginning; p_end--)
		free(p_line[p_end]);
	    Fseek(pfp, repl_backtrack_position, 0);

	    /* redundant 'new' context lines were omitted - set */
	    /* up to fill them in from the old file context */
	    if (!p_context && p_repl_lines == 1) {
		p_repl_lines = 0;
		p_max--;
	    }
	    fillsrc = 1;
	    filldst = repl_beginning + 1;
	    fillcnt = p_repl_lines;
	    p_end = p_max;
	} else if (!p_context && fillcnt == 1) {
	    /* the first hunk was a null hunk with no context */
	    /* and we were expecting one line -- fix it up. */
	    while (filldst < p_end) {
		p_line[filldst] = p_line[filldst + 1];
		p_char[filldst] = p_char[filldst + 1];
		p_len[filldst] = p_len[filldst + 1];
		filldst++;
	    }
#if 0
	    repl_beginning--;		/* this doesn't need to be fixed */
#endif
	    p_end--;
	    p_first++;			/* do append rather than insert */
	    fillcnt = 0;
	    p_ptrn_lines = 0;
	}

	if (diff_type == CONTEXT_DIFF &&
	    (fillcnt || (p_first > 1 && ptrn_copiable > 2 * p_context))) {
	    if (verbose)
		say("%s\n",
		    "(Fascinating--this is really a new-style context diff"
		    "but without\nthe telltale extra asterisks on the *** "
		    "line that usually indicate\nthe new style...)");
	    diff_type = NEW_CONTEXT_DIFF;
	}
	
	/* if there were omitted context lines, fill them in now */
	if (fillcnt) {
	    p_bfake = filldst;		/* remember where not to free() */
	    p_efake = filldst + fillcnt - 1;
	    while (fillcnt-- > 0) {
		while (fillsrc <= p_end && p_char[fillsrc] != ' ')
		    fillsrc++;
		if (fillsrc > p_end)
		    fatal("replacement text or line numbers mangled in"
			  " hunk at line %d\n",
			  p_hunk_beg);
		p_line[filldst] = p_line[fillsrc];
		p_char[filldst] = p_char[fillsrc];
		p_len[filldst] = p_len[fillsrc];
		fillsrc++; filldst++;
	    }
	    while (fillsrc <= p_end && fillsrc != repl_beginning &&
		   p_char[fillsrc] != ' ')
		fillsrc++;
#ifdef DEBUGGING
	    if (debug & 64)
		printf("fillsrc %d, filldst %d, rb %d, e %d\n",
		    fillsrc, filldst, repl_beginning, p_end);
#endif
	    if (fillsrc != p_end + 1 && fillsrc != repl_beginning)
		malformed();
	    if (filldst != p_end + 1 && filldst != repl_beginning)
		malformed();
	}

	if (p_line[p_end] != NULL)
	{
		if (remove_special_line()) {
			p_len[p_end] -= 1;
			(p_line[p_end])[p_len[p_end]] = 0;
		}
	}
    } else if (diff_type == UNI_DIFF) {
	long line_beginning = ftell(pfp);
					/* file pos of the current line */
	LINENUM fillsrc;		/* index of old lines */
	LINENUM filldst;		/* index of new lines */
	char ch;

	ret = pgets(buf, sizeof buf, pfp);
	p_input_line++;
	if (ret == NULL || strnNE(buf, "@@ -", 4)) {
	    next_intuit_at(line_beginning,p_input_line);
	    return FALSE;
	}
	s = buf + 4;
	if (!*s)
	    malformed();
	p_first = atoi(s);
	while (isdigit((unsigned char)*s))
		s++;
	if (*s == ',') {
	    p_ptrn_lines = atoi(++s);
	    while (isdigit((unsigned char)*s))
		    s++;
	} else
	    p_ptrn_lines = 1;
	if (*s == ' ')
		s++;
	if (*s != '+' || !*++s)
	    malformed();
	p_newfirst = atoi(s);
	while (isdigit((unsigned char)*s))
		s++;
	if (*s == ',') {
	    p_repl_lines = atoi(++s);
	    while (isdigit((unsigned char)*s))
		    s++;
	} else
	    p_repl_lines = 1;
	if (*s == ' ')
		s++;
	if (*s != '@')
	    malformed();
	if (!p_ptrn_lines)
	    p_first++;			/* do append rather than insert */
	p_max = p_ptrn_lines + p_repl_lines + 1;
	while (p_max >= hunkmax)
	    grow_hunkmax();
	fillsrc = 1;
	filldst = fillsrc + p_ptrn_lines;
	p_end = filldst + p_repl_lines;
	snprintf(buf, sizeof(buf), "*** %d,%d ****\n", p_first,
	    p_first + p_ptrn_lines - 1);
	p_line[0] = xstrdup(buf);
	p_char[0] = '*';
        snprintf(buf, sizeof(buf), "--- %d,%d ----\n", p_newfirst,
		p_newfirst + p_repl_lines - 1);
	p_line[filldst] = xstrdup(buf);
	p_char[filldst++] = '=';
	p_context = 100;
	context = 0;
	p_hunk_beg = p_input_line + 1;
	while (fillsrc <= p_ptrn_lines || filldst <= p_end) {
	    line_beginning = ftell(pfp);
	    ret = pgets(buf, sizeof buf, pfp);
	    p_input_line++;
	    if (ret == NULL) {
		if (p_max - filldst < 3) {
		    /* assume blank lines got chopped */
		    strlcpy(buf, " \n", sizeof(buf));
		} else {
		    fatal("unexpected end of file in patch\n");
		}
	    }
	    if (*buf == '\t' || *buf == '\n') {
		ch = ' ';		/* assume the space got eaten */
		s = xstrdup(buf);
	    } else {
		ch = *buf;
		s = xstrdup(buf + 1);
	    }
	    switch (ch) {
	    case '-':
		if (fillsrc > p_ptrn_lines) {
		    free(s);
		    p_end = filldst - 1;
		    malformed();
		}
		p_char[fillsrc] = ch;
		p_line[fillsrc] = s;
		p_len[fillsrc++] = strlen(s);
		if (fillsrc > p_ptrn_lines) {
			if (remove_special_line()) {
				p_len[fillsrc - 1] -= 1;
				s[p_len[fillsrc - 1]] = 0;
			}
		}
		break;
	    case '=':
		ch = ' ';
		/* FALLTHROUGH */
	    case ' ':
		if (fillsrc > p_ptrn_lines) {
		    free(s);
		    while (--filldst > p_ptrn_lines)
			free(p_line[filldst]);
		    p_end = fillsrc - 1;
		    malformed();
		}
		context++;
		p_char[fillsrc] = ch;
		p_line[fillsrc] = s;
		p_len[fillsrc++] = strlen(s);
		s = xstrdup(s);
		/* FALLTHROUGH */
	    case '+':
		if (filldst > p_end) {
		    free(s);
		    while (--filldst > p_ptrn_lines)
			free(p_line[filldst]);
		    p_end = fillsrc - 1;
		    malformed();
		}
		p_char[filldst] = ch;
		p_line[filldst] = s;
		p_len[filldst++] = strlen(s);
		if (fillsrc > p_ptrn_lines) {
			if (remove_special_line()) {
				p_len[filldst - 1] -= 1;
				s[p_len[filldst - 1]] = 0;
			}
		}
		break;
	    default:
		p_end = filldst;
		malformed();
	    }
	    if (ch != ' ' && context > 0) {
		if (context < p_context)
		    p_context = context;
		context = -1000;
	    }
	}/* while */
    } else {				/* normal diff--fake it up */
	char hunk_type;
	int i;
	LINENUM min, max;
	long line_beginning = ftell(pfp);

	p_context = 0;
	ret = pgets(buf, sizeof buf, pfp);
	p_input_line++;
	if (ret == NULL || !isdigit((unsigned char)*buf)) {
	    next_intuit_at(line_beginning, p_input_line);
	    return FALSE;
	}
	p_first = atoi(buf);
	for (s = buf; isdigit((unsigned char)*s); s++)
		;
	if (*s == ',') {
	    p_ptrn_lines = atoi(++s) - p_first + 1;
	    while (isdigit((unsigned char)*s))
		    s++;
	} else
	    p_ptrn_lines = (*s != 'a');
	hunk_type = *s;
	if (hunk_type == 'a')
	    p_first++;			/* do append rather than insert */
	min = atoi(++s);
	for (; isdigit((unsigned char)*s); s++)
		;
	if (*s == ',')
	    max = atoi(++s);
	else
	    max = min;
	if (hunk_type == 'd')
	    min++;
	p_end = p_ptrn_lines + 1 + max - min + 1;
	if (p_end > MAXHUNKSIZE)
	    fatal("hunk too large (%d lines) at line %d: %s",
		  p_end, p_input_line, buf);
	while (p_end >= hunkmax)
	    grow_hunkmax();
	p_newfirst = min;
	p_repl_lines = max - min + 1;
	snprintf(buf, sizeof(buf), "*** %d,%d\n", p_first,
	    p_first + p_ptrn_lines - 1);
	p_line[0] = xstrdup(buf);
	p_char[0] = '*';
	for (i = 1; i <= p_ptrn_lines; i++) {
	    ret = pgets(buf, sizeof buf, pfp);
	    p_input_line++;
	    if (ret == NULL)
		fatal("unexpected end of file in patch at line %d\n",
		      p_input_line);
	    if (*buf != '<')
		fatal("< expected at line %d of patch\n", p_input_line);
	    p_line[i] = xstrdup(buf + 2);
	    p_len[i] = strlen(p_line[i]);
	    p_char[i] = '-';
	}

	if (remove_special_line()) {
		p_len[i - 1] -= 1;
		(p_line[i - 1])[p_len[i - 1]] = 0;
	}

	if (hunk_type == 'c') {
	    ret = pgets(buf, sizeof buf, pfp);
	    p_input_line++;
	    if (ret == NULL)
		fatal("unexpected end of file in patch at line %d\n",
		    p_input_line);
	    if (*buf != '-')
		fatal("--- expected at line %d of patch\n", p_input_line);
	}
	snprintf(buf, sizeof(buf), "--- %d,%d\n", min, max);
	p_line[i] = xstrdup(buf);
	p_char[i] = '=';
	for (i++; i <= p_end; i++) {
	    ret = pgets(buf, sizeof buf, pfp);
	    p_input_line++;
	    if (ret == NULL)
		fatal("unexpected end of file in patch at line %d\n",
		    p_input_line);
	    if (*buf != '>')
		fatal("> expected at line %d of patch\n", p_input_line);
	    p_line[i] = xstrdup(buf + 2);
	    p_len[i] = strlen(p_line[i]);
	    p_char[i] = '+';
	}

	if (remove_special_line()) {
		p_len[i - 1] -= 1;
		(p_line[i - 1])[p_len[i - 1]] = 0;
	}
    }
    if (reverse)			/* backwards patch? */
	if (!pch_swap())
	    say("Not enough memory to swap next hunk!\n");
#ifdef DEBUGGING
    if (debug & 2) {
	int i;
	char special;

	for (i = 0; i <= p_end; i++) {
	    if (i == p_ptrn_lines)
		special = '^';
	    else
		special = ' ';
	    fprintf(stderr, "%3d %c %c %s", i, p_char[i], special, p_line[i]);
	    Fflush(stderr);
	}
    }
#endif
    if (p_end + 1 < hunkmax)	/* paranoia reigns supreme... */
	p_char[p_end + 1] = '^';  /* add a stopper for apply_hunk */
    return TRUE;
}

/*
 * Input a line from the patch file, worrying about indentation.
 */
char *
pgets(char *bf, int sz, FILE *fp)
{
	char *ret = fgets(bf, sz, fp);
	char *s;
	int indent = 0;

	if (p_indent && ret != NULL) {
		for (s = buf;
		     indent < p_indent &&
			     (*s == ' ' || *s == '\t' || *s == 'X');
		     s++) {
			if (*s == '\t')
				indent += 8 - (indent % 7);
			else
				indent++;
		}
		if (buf != s)
			strlcpy(buf, s, sizeof(buf));
	}
	return ret;
}

/*
 * Reverse the old and new portions of the current hunk.
 */
bool
pch_swap(void)
{
	char **tp_line;		/* the text of the hunk */
	size_t *tp_len;		/* length of each line */
	char *tp_char;		/* +, -, and ! */
	LINENUM i;
	LINENUM n;
	bool blankline = FALSE;
	char *s;

	i = p_first;
	p_first = p_newfirst;
	p_newfirst = i;
    
	/* make a scratch copy */

	tp_line = p_line;
	tp_len = p_len;
	tp_char = p_char;
	p_line = NULL;		/* force set_hunkmax to allocate again */
	p_len = NULL;
	p_char = NULL;
	set_hunkmax();
	if (p_line == NULL || p_len == NULL || p_char == NULL) {
		if (p_line == NULL)
			free(p_line);
		p_line = tp_line;
		if (p_len == NULL)
			free(p_len);
		p_len = tp_len;
		if (p_char == NULL)
			free(p_char);
		p_char = tp_char;
		return FALSE;		/* not enough memory to swap hunk! */
	}

	/* now turn the new into the old */

	i = p_ptrn_lines + 1;
	if (tp_char[i] == '\n') {	/* account for possible blank line */
		blankline = TRUE;
		i++;
	}
	if (p_efake >= 0) {		/* fix non-freeable ptr range */
		if (p_efake <= i)
			n = p_end - i + 1;
		else
			n = -i;
		p_efake += n;
		p_bfake += n;
	}
	for (n = 0; i <= p_end; i++, n++) {
		p_line[n] = tp_line[i];
		p_char[n] = tp_char[i];
		if (p_char[n] == '+')
			p_char[n] = '-';
		p_len[n] = tp_len[i];
	}
	if (blankline) {
		i = p_ptrn_lines + 1;
		p_line[n] = tp_line[i];
		p_char[n] = tp_char[i];
		p_len[n] = tp_len[i];
		n++;
	}
	if (p_char[0] != '=')
		fatal("malformed patch at line %d: expected `=' found `%c'\n",
		    p_input_line, p_char[0]);
	p_char[0] = '*';
	for (s = p_line[0]; *s; s++)
		if (*s == '-')
			*s = '*';

	/* now turn the old into the new */

	if (tp_char[0] != '*')
		fatal("malformed patch at line %d: expected `*' found `%c'\n",
		    p_input_line, tp_char[0]);
	tp_char[0] = '=';
	for (s = tp_line[0]; *s; s++)
		if (*s == '*')
			*s = '-';
	for (i = 0; n <= p_end; i++, n++) {
		p_line[n] = tp_line[i];
		p_char[n] = tp_char[i];
		if (p_char[n] == '-')
			p_char[n] = '+';
		p_len[n] = tp_len[i];
	}
	if (i != p_ptrn_lines + 1)
		fatal("malformed patch at line %d: need %d lines, got %d\n", 
		    p_input_line, p_ptrn_lines + 1, i);
	i = p_ptrn_lines;
	p_ptrn_lines = p_repl_lines;
	p_repl_lines = i;
	if (tp_line == NULL)
		free(tp_line);
	if (tp_len == NULL)
		free(tp_len);
	if (tp_char == NULL)
		free(tp_char);
	return TRUE;
}

/*
 * Return the specified line position in the old file of the old context.
 */
LINENUM
pch_first(void)
{
	return p_first;
}

/*
 * Return the number of lines of old context.
 */
LINENUM
pch_ptrn_lines(void)
{
	return p_ptrn_lines;
}

/*
 * Return the probable line position in the new file of the first line.
 */
LINENUM
pch_newfirst(void)
{
	return p_newfirst;
}

/*
 * Return the number of lines in the replacement text including context.
 */
LINENUM
pch_repl_lines(void)
{
	return p_repl_lines;
}

/*
 * Return the number of lines in the whole hunk.
 */
LINENUM
pch_end(void)
{
	return p_end;
}

/*
 * Return the number of context lines before the first changed line.
 */
LINENUM
pch_context(void)
{
	return p_context;
}

/*
 * Return the length of a particular patch line.
 */
size_t
pch_line_len(LINENUM line)
{
	return p_len[line];
}

/*
 * Return the control character (+, -, *, !, etc) for a patch line.
 */
char
pch_char(LINENUM line)
{
	return p_char[line];
}

/*
 * Return a pointer to a particular patch line.
 */
char *
pfetch(LINENUM line)
{
	return p_line[line];
}

/*
 * Return where in the patch file this hunk began, for error messages.
 */
LINENUM
pch_hunk_beg(void)
{
	return p_hunk_beg;
}

/*
 * Apply an ed script by feeding ed itself.
 */
void
do_ed_script(void)
{
	char *t;
	long beginning_of_this_line;
	bool this_line_is_command = FALSE;
	FILE *pipefp = NULL;

	if (!skip_rest_of_patch) {
		Unlink(TMPOUTNAME);
		copy_file(filearg[0], TMPOUTNAME);
		if (verbose)
			snprintf(buf, sizeof(buf), "/bin/ed %s", TMPOUTNAME);
		else
			snprintf(buf, sizeof(buf), "/bin/ed - %s", TMPOUTNAME);
		pipefp = popen(buf, "w");
	}
	for (;;) {
		beginning_of_this_line = ftell(pfp);
		if (pgets(buf, sizeof buf, pfp) == NULL) {
			next_intuit_at(beginning_of_this_line, p_input_line);
			break;
		}
		p_input_line++;
		for (t = buf; isdigit((unsigned char)*t) || *t == ','; t++)
			;
		this_line_is_command = (isdigit((unsigned char)*buf) &&
					(*t == 'd' || *t == 'c' || *t == 'a'));
		if (this_line_is_command) {
			if (!skip_rest_of_patch)
				fputs(buf, pipefp);
			if (*t != 'd') {
				while (pgets(buf, sizeof buf, pfp) != NULL) {
					p_input_line++;
					if (!skip_rest_of_patch)
						fputs(buf, pipefp);
					if (strEQ(buf, ".\n"))
						break;
				}
			}
		} else {
			next_intuit_at(beginning_of_this_line,p_input_line);
			break;
		}
	}
	if (skip_rest_of_patch)
		return;
	fprintf(pipefp, "w\n");
	fprintf(pipefp, "q\n");
	Fflush(pipefp);
	Pclose(pipefp);
	ignore_signals();
	if (move_file(TMPOUTNAME, outname) < 0) {
		toutkeep = TRUE;
		chmod(TMPOUTNAME, filemode);
	} else
		chmod(outname, filemode);
	set_signals(1);
}
