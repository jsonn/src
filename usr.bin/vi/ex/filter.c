/*-
 * Copyright (c) 1991, 1993, 1994
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
static char sccsid[] = "@(#)filter.c	8.45 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>
#include <pathnames.h>

#include "vi.h"
#include "excmd.h"

static int	filter_ldisplay __P((SCR *, FILE *));

/*
 * filtercmd --
 *	Run a range of lines through a filter utility and optionally
 *	replace the original text with the stdout/stderr output of
 *	the utility.
 */
int
filtercmd(sp, ep, fm, tm, rp, cmd, ftype)
	SCR *sp;
	EXF *ep;
	MARK *fm, *tm, *rp;
	char *cmd;
	enum filtertype ftype;
{
	FILE *ifp, *ofp;
	pid_t parent_writer_pid, utility_pid;
	recno_t nread;
	int input[2], output[2], rval, teardown;
	char *name;

	/* Set return cursor position; guard against a line number of zero. */
	*rp = *fm;
	if (fm->lno == 0)
		rp->lno = 1;

	/*
	 * There are three different processes running through this code.
	 * They are the utility, the parent-writer and the parent-reader.
	 * The parent-writer is the process that writes from the file to
	 * the utility, the parent reader is the process that reads from
	 * the utility.
	 *
	 * Input and output are named from the utility's point of view.
	 * The utility reads from input[0] and the parent(s) write to
	 * input[1].  The parent(s) read from output[0] and the utility
	 * writes to output[1].
	 *
	 * In the FILTER_READ case, the utility isn't expected to want
	 * input.  Redirect its input from /dev/null.  Otherwise open
	 * up utility input pipe.
	 */
	teardown = 0;
	ofp = NULL;
	input[0] = input[1] = output[0] = output[1] = -1;
	if (ftype == FILTER_READ) {
		if ((input[0] = open(_PATH_DEVNULL, O_RDONLY, 0)) < 0) {
			msgq(sp, M_ERR,
			    "filter: %s: %s", _PATH_DEVNULL, strerror(errno));
			return (1);
		}
	} else
		if (pipe(input) < 0) {
			msgq(sp, M_SYSERR, "pipe");
			goto err;
		}

	/* Open up utility output pipe. */
	if (pipe(output) < 0) {
		msgq(sp, M_SYSERR, "pipe");
		goto err;
	}
	if ((ofp = fdopen(output[0], "r")) == NULL) {
		msgq(sp, M_SYSERR, "fdopen");
		goto err;
	}

	/*
	 * Save ex/vi terminal settings, and restore the original ones.
	 * Restoration so that users can do things like ":r! cat /dev/tty".
	 */
	teardown = ftype != FILTER_WRITE && !ex_sleave(sp);

	/* Fork off the utility process. */
	SIGBLOCK(sp->gp);
	switch (utility_pid = vfork()) {
	case -1:			/* Error. */
		SIGUNBLOCK(sp->gp);

		msgq(sp, M_SYSERR, "vfork");
err:		if (input[0] != -1)
			(void)close(input[0]);
		if (input[1] != -1)
			(void)close(input[1]);
		if (ofp != NULL)
			(void)fclose(ofp);
		else if (output[0] != -1)
			(void)close(output[0]);
		if (output[1] != -1)
			(void)close(output[1]);
		rval = 1;
		goto ret;
	case 0:				/* Utility. */
		/* The utility has default signal behavior. */
		sig_end();

		/*
		 * Redirect stdin from the read end of the input pipe, and
		 * redirect stdout/stderr to the write end of the output pipe.
		 *
		 * !!!
		 * Historically, ex only directed stdout into the input pipe,
		 * letting stderr come out on the terminal as usual.  Vi did
		 * not, directing both stdout and stderr into the input pipe.
		 * We match that practice for both ex and vi for consistency.
		 */
		(void)dup2(input[0], STDIN_FILENO);
		(void)dup2(output[1], STDOUT_FILENO);
		(void)dup2(output[1], STDERR_FILENO);

		/* Close the utility's file descriptors. */
		(void)close(input[0]);
		(void)close(input[1]);
		(void)close(output[0]);
		(void)close(output[1]);

		if ((name = strrchr(O_STR(sp, O_SHELL), '/')) == NULL)
			name = O_STR(sp, O_SHELL);
		else
			++name;

		execl(O_STR(sp, O_SHELL), name, "-c", cmd, NULL);
		msgq(sp, M_ERR, "Error: execl: %s: %s",
		    O_STR(sp, O_SHELL), strerror(errno));
		_exit (127);
		/* NOTREACHED */
	default:			/* Parent-reader, parent-writer. */
		SIGUNBLOCK(sp->gp);

		/* Close the pipe ends neither parent will use. */
		(void)close(input[0]);
		(void)close(output[1]);
		break;
	}

	/*
	 * FILTER_READ:
	 *
	 * Reading is the simple case -- we don't need a parent writer,
	 * so the parent reads the output from the read end of the output
	 * pipe until it finishes, then waits for the child.  Ex_readfp
	 * appends to the MARK, and closes ofp.
	 *
	 * !!!
	 * Set the return cursor to the last line read in.  Historically,
	 * this behaves differently from ":r file" command, which leaves
	 * the cursor at the first line read in.  Check to make sure that
	 * it's not past EOF because we were reading into an empty file.
	 */
	if (ftype == FILTER_READ) {
		rval = ex_readfp(sp, ep, "filter", ofp, fm, &nread, 0);
		sp->rptlines[L_ADDED] += nread;
		if (fm->lno == 0)
			rp->lno = nread;
		else
			rp->lno += nread;
		goto uwait;
	}

	/*
	 * FILTER, FILTER_WRITE
	 *
	 * Here we need both a reader and a writer.  Temporary files are
	 * expensive and we'd like to avoid disk I/O.  Using pipes has the
	 * obvious starvation conditions.  It's done as follows:
	 *
	 *	fork
	 *	child
	 *		write lines out
	 *		exit
	 *	parent
	 *		FILTER:
	 *			read lines into the file
	 *			delete old lines
	 *		FILTER_WRITE
	 *			read and display lines
	 *		wait for child
	 *
	 * XXX
	 * We get away without locking the underlying database because we know
	 * that none of the records that we're reading will be modified until
	 * after we've read them.  This depends on the fact that the current
	 * B+tree implementation doesn't balance pages or similar things when
	 * it inserts new records.  When the DB code has locking, we should
	 * treat vi as if it were multiple applications sharing a database, and
	 * do the required locking.  If necessary a work-around would be to do
	 * explicit locking in the line.c:file_gline() code, based on the flag
	 * set here.
	 */
	rval = 0;
	F_SET(ep, F_MULTILOCK);

	SIGBLOCK(sp->gp);
	switch (parent_writer_pid = fork()) {
	case -1:			/* Error. */
		SIGUNBLOCK(sp->gp);

		msgq(sp, M_SYSERR, "fork");
		(void)close(input[1]);
		(void)close(output[0]);
		rval = 1;
		break;
	case 0:				/* Parent-writer. */
		/*
		 * Write the selected lines to the write end of the input
		 * pipe.  This instance of ifp is closed by ex_writefp.
		 */
		(void)close(output[0]);
		if ((ifp = fdopen(input[1], "w")) == NULL)
			_exit (1);
		_exit(ex_writefp(sp, ep, "filter", ifp, fm, tm, NULL, NULL));

		/* NOTREACHED */
	default:			/* Parent-reader. */
		SIGUNBLOCK(sp->gp);

		(void)close(input[1]);
		if (ftype == FILTER_WRITE)
			/*
			 * Read the output from the read end of the output
			 * pipe and display it.  Filter_ldisplay closes ofp.
			 */
			rval = filter_ldisplay(sp, ofp);
		else {
			/*
			 * Read the output from the read end of the output
			 * pipe.  Ex_readfp appends to the MARK and closes
			 * ofp.
			 */
			rval = ex_readfp(sp, ep, "filter", ofp, tm, &nread, 0);
			sp->rptlines[L_ADDED] += nread;
		}

		/* Wait for the parent-writer. */
		rval |= proc_wait(sp,
		    (long)parent_writer_pid, "parent-writer", 1);

		/* Delete any lines written to the utility. */
		if (rval == 0 && ftype == FILTER &&
		    (cut(sp, ep, NULL, fm, tm, CUT_LINEMODE) ||
		    delete(sp, ep, fm, tm, 1))) {
			rval = 1;
			break;
		}

		/*
		 * If the filter had no output, we may have just deleted
		 * the cursor.  Don't do any real error correction, we'll
		 * try and recover later.
		 */
		 if (rp->lno > 1 && file_gline(sp, ep, rp->lno, NULL) == NULL)
			--rp->lno;
		break;
	}
	F_CLR(ep, F_MULTILOCK);

uwait:	rval |= proc_wait(sp, (long)utility_pid, cmd, 0);

	/* Restore ex/vi terminal settings. */
ret:	if (teardown)
		ex_rleave(sp);
	return (rval);
}

/*
 * proc_wait --
 *	Wait for one of the processes.
 *
 * !!!
 * The pid_t type varies in size from a short to a long depending on the
 * system.  It has to be cast into something or the standard promotion
 * rules get you.  I'm using a long based on the belief that nobody is
 * going to make it unsigned and it's unlikely to be a quad.
 */
int
proc_wait(sp, pid, cmd, okpipe)
	SCR *sp;
	long pid;
	const char *cmd;
	int okpipe;
{
	extern const char *const sys_siglist[];
	size_t len;
	int pstat;

	/*
	 * Wait for the utility to finish.  We can get interrupted
	 * by SIGALRM, just ignore it.
	 */
	for (;;) {
		errno = 0;
		if (waitpid((pid_t)pid, &pstat, 0) != -1)
			break;
		if (errno != EINTR) {
			msgq(sp, M_SYSERR, "wait error");
			return (1);
		}
	}

	/*
	 * Display the utility's exit status.  Ignore SIGPIPE from the
	 * parent-writer, as that only means that the utility chose to
	 * exit before reading all of its input.
	 */
	if (WIFSIGNALED(pstat) && (!okpipe || WTERMSIG(pstat) != SIGPIPE)) {
		for (; isblank(*cmd); ++cmd);
		len = strlen(cmd);
		msgq(sp, M_ERR, "%.*s%s: received signal: %s%s",
		    MIN(len, 20), cmd, len > 20 ? "..." : "",
		    sys_siglist[WTERMSIG(pstat)],
		    WCOREDUMP(pstat) ? "; core dumped" : "");
		return (1);
	}
	if (WIFEXITED(pstat) && WEXITSTATUS(pstat)) {
		for (; isblank(*cmd); ++cmd);
		len = strlen(cmd);
		msgq(sp, M_ERR, "%.*s%s: exited with status %d",
		    MIN(len, 20), cmd, len > 20 ? "..." : "",
		    WEXITSTATUS(pstat));
		return (1);
	}
	return (0);
}

/*
 * filter_ldisplay --
 *	Display output from a utility.
 *
 * !!!
 * Historically, the characters were passed unmodified to the terminal.
 * We use the ex print routines to make sure they're printable.
 */
static int
filter_ldisplay(sp, fp)
	SCR *sp;
	FILE *fp;
{
	size_t len;

	EX_PRIVATE *exp;

	F_SET(sp, S_INTERRUPTIBLE);
	for (exp = EXP(sp); !ex_getline(sp, fp, &len);) {
		if (ex_ldisplay(sp, exp->ibp, len, 0, 0))
			break;
		if (INTERRUPTED(sp))
			break;
	}
	if (ferror(fp))
		msgq(sp, M_SYSERR, "filter input");
	(void)fclose(fp);
	return (0);
}
