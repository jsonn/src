/*-
 * Copyright (c) 1992, 1993, 1994
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
static char sccsid[] = "@(#)v_text.c	8.43 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * !!!
 * Repeated input in the historic vi is mostly wrong and this isn't very
 * backward compatible.  For example, if the user entered "3Aab\ncd" in
 * the historic vi, the "ab" was repeated 3 times, and the "\ncd" was then
 * appended to the result.  There was also a hack which I don't remember
 * right now, where "3o" would open 3 lines and then let the user fill them
 * in, to make screen movements on 300 baud modems more tolerable.  I don't
 * think it's going to be missed.
 *
 * !!!
 * There's a problem with the way that we do logging for change commands with
 * implied motions (e.g. A, I, O, cc, etc.).  Since the main vi loop logs the
 * starting cursor position before the change command "moves" the cursor, the
 * cursor position to which we return on undo will be where the user entered
 * the change command, not the start of the change.  Several of the following
 * routines re-log the cursor to make this work correctly.  Historic vi tried
 * to do the same thing, and mostly got it right.  (The only spectacular way
 * it fails is if the user entered 'o' from anywhere but the last character of
 * the line, the undo returned the cursor to the start of the line.  If the
 * user was on the last character of the line, the cursor returned to that
 * position.)  We also check for mapped keys waiting, i.e. if we're in the
 * middle of a map, don't bother logging the cursor.
 */
#define	LOG_CORRECT {							\
	if (!MAPPED_KEYS_WAITING(sp))					\
		(void)log_cursor(sp, ep);				\
}
#define	LOG_CORRECT_FIRST {						\
	if (first == 1) {						\
		LOG_CORRECT;						\
		first = 0;						\
	}								\
}

static u_int	set_txt_std __P((SCR *, VICMDARG *, u_int));
static int	v_CS __P((SCR *, EXF *, VICMDARG *, u_int));

/*
 * v_iA -- [count]A
 *	Append text to the end of the line.
 */
int
v_iA(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;
	u_long cnt;
	size_t len;
	u_int flags;
	int first;
	char *p;

	sp->showmode = "Append";
	flags = set_txt_std(sp, vp, TXT_APPENDEOL);
	for (first = 1, lno = vp->m_start.lno,
	    cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		/* Move the cursor to the end of the line + 1. */
		if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0) {
				GETLINE_ERR(sp, lno);
				return (1);
			}
			lno = 1;
			len = 0;
		} else {
			/* Correct logging for implied cursor motion. */
			if (first == 1) {
				sp->cno = len == 0 ? 0 : len - 1;
				LOG_CORRECT;
				first = 0;
			}

			/* Start the change after the line. */
			sp->cno = len;
		}

		if (v_ntext(sp, ep,
		    sp->tiqp, NULL, p, len, &vp->m_final, 0, OOBLNO, flags))
			return (1);

		flags = set_txt_std(sp, vp, TXT_APPENDEOL | TXT_REPLAY);
		sp->lno = lno = vp->m_final.lno;
		sp->cno = vp->m_final.cno;
	}
	return (0);
}

/*
 * v_ia -- [count]a
 *	Append text to the cursor position.
 */
int
v_ia(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;
	u_long cnt;
	u_int flags;
	size_t len;
	char *p;

	sp->showmode = "Append";
	flags = set_txt_std(sp, vp, 0);
	for (lno = vp->m_start.lno,
	    cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		/*
		 * Move the cursor one column to the right and
		 * repaint the screen.
		 */
		if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0) {
				GETLINE_ERR(sp, lno);
				return (1);
			}
			lno = 1;
			len = 0;
			LF_SET(TXT_APPENDEOL);
		} else if (len) {
			if (len == sp->cno + 1) {
				sp->cno = len;
				LF_SET(TXT_APPENDEOL);
			} else
				++sp->cno;
		} else
			LF_SET(TXT_APPENDEOL);

		if (v_ntext(sp, ep,
		    sp->tiqp, NULL, p, len, &vp->m_final, 0, OOBLNO, flags))
			return (1);

		flags = set_txt_std(sp, vp, TXT_REPLAY);
		sp->lno = lno = vp->m_final.lno;
		sp->cno = vp->m_final.cno;
	}
	return (0);
}

/*
 * v_iI -- [count]I
 *	Insert text at the first non-blank character in the line.
 */
int
v_iI(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;
	u_long cnt;
	size_t len;
	u_int flags;
	int first;
	char *p;

	sp->showmode = "Insert";
	flags = set_txt_std(sp, vp, 0);
	for (first = 1, lno = vp->m_start.lno,
	    cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		/*
		 * Move the cursor to the start of the line and repaint
		 * the screen.
		 */
		if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0) {
				GETLINE_ERR(sp, lno);
				return (1);
			}
			lno = 1;
			len = 0;
		} else {
			sp->cno = 0;
			if (nonblank(sp, ep, lno, &sp->cno))
				return (1);

			/* Correct logging for implied cursor motion. */
			LOG_CORRECT_FIRST;
		}
		if (len == 0)
			LF_SET(TXT_APPENDEOL);

		if (v_ntext(sp, ep,
		    sp->tiqp, NULL, p, len, &vp->m_final, 0, OOBLNO, flags))
			return (1);

		flags = set_txt_std(sp, vp, TXT_REPLAY);
		sp->lno = lno = vp->m_final.lno;
		sp->cno = vp->m_final.cno;
	}
	return (0);
}

/*
 * v_ii -- [count]i
 *	Insert text at the cursor position.
 */
int
v_ii(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;
	u_long cnt;
	size_t len;
	u_int flags;
	char *p;

	sp->showmode = "Insert";
	flags = set_txt_std(sp, vp, 0);
	for (lno = vp->m_start.lno,
	    cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0) {
				GETLINE_ERR(sp, vp->m_start.lno);
				return (1);
			}
			lno = 1;
			len = 0;
		}
		/* If len == sp->cno, it's a replay caused by a count. */
		if (len == 0 || len == sp->cno)
			LF_SET(TXT_APPENDEOL);

		if (v_ntext(sp, ep,
		    sp->tiqp, NULL, p, len, &vp->m_final, 0, OOBLNO, flags))
			return (1);

		/*
		 * On replay, if the line isn't empty, advance the insert
		 * by one (make it an append).
		 */
		flags = set_txt_std(sp, vp, TXT_REPLAY);
		sp->lno = lno = vp->m_final.lno;
		if ((sp->cno = vp->m_final.cno) != 0)
			++sp->cno;
	}
	return (0);
}

/*
 * v_iO -- [count]O
 *	Insert text above this line.
 */
int
v_iO(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t ai_line, lno;
	size_t len;
	u_long cnt;
	u_int flags;
	int first;
	char *p;

	sp->showmode = "Insert";
	flags = set_txt_std(sp, vp, TXT_APPENDEOL);
	for (first = 1, cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		if (sp->lno == 1) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0)
				goto insert;
			p = NULL;
			len = 0;
			ai_line = OOBLNO;
		} else {
insert:			p = "";
			sp->cno = 0;

			/* Correct logging for implied cursor motion. */
			LOG_CORRECT_FIRST;

			if (file_iline(sp, ep, sp->lno, p, 0))
				return (1);
			if ((p = file_gline(sp, ep, sp->lno, &len)) == NULL) {
				GETLINE_ERR(sp, sp->lno);
				return (1);
			}
			ai_line = sp->lno + 1;
		}

		if (v_ntext(sp, ep,
		    sp->tiqp, NULL, p, len, &vp->m_final, 0, ai_line, flags))
			return (1);

		flags = set_txt_std(sp, vp, TXT_APPENDEOL | TXT_REPLAY);
		sp->lno = lno = vp->m_final.lno;
		sp->cno = vp->m_final.cno;
	}
	return (0);
}

/*
 * v_io -- [count]o
 *	Insert text after this line.
 */
int
v_io(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t ai_line, lno;
	size_t len;
	u_long cnt;
	u_int flags;
	int first;
	char *p;

	sp->showmode = "Insert";
	flags = set_txt_std(sp, vp, TXT_APPENDEOL);
	for (first = 1,
	    cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		if (sp->lno == 1) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0)
				goto insert;
			p = NULL;
			len = 0;
			ai_line = OOBLNO;
		} else {
insert:			p = "";
			sp->cno = 0;

			/* Correct logging for implied cursor motion. */
			LOG_CORRECT_FIRST;

			len = 0;
			if (file_aline(sp, ep, 1, sp->lno, p, len))
				return (1);
			if ((p = file_gline(sp, ep, ++sp->lno, &len)) == NULL) {
				GETLINE_ERR(sp, sp->lno);
				return (1);
			}
			ai_line = sp->lno - 1;
		}

		if (v_ntext(sp, ep,
		    sp->tiqp, NULL, p, len, &vp->m_final, 0, ai_line, flags))
			return (1);

		flags = set_txt_std(sp, vp, TXT_APPENDEOL | TXT_REPLAY);
		sp->lno = lno = vp->m_final.lno;
		sp->cno = vp->m_final.cno;
	}
	return (0);
}

/*
 * v_Change -- [buffer][count]C
 *	Change line command.
 */
int
v_Change(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (v_CS(sp, ep, vp, 0));
}

/*
 * v_Subst -- [buffer][count]S
 *	Line substitute command.
 */
int
v_Subst(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	u_int flags;

	/*
	 * The S command is the same as a 'C' command from the beginning
	 * of the line.  This is hard to do in the parser, so do it here.
	 *
	 * If autoindent is on, the change is from the first *non-blank*
	 * character of the line, not the first character.  And, to make
	 * it just a bit more exciting, the initial space is handled as
	 * auto-indent characters.
	 */
	LF_INIT(0);
	if (O_ISSET(sp, O_AUTOINDENT)) {
		vp->m_start.cno = 0;
		if (nonblank(sp, ep, vp->m_start.lno, &vp->m_start.cno))
			return (1);
		LF_SET(TXT_AICHARS);
	} else
		vp->m_start.cno = 0;
	sp->cno = vp->m_start.cno;
	return (v_CS(sp, ep, vp, flags));
}

/*
 * v_CS --
 *	C and S commands.
 */
static int
v_CS(sp, ep, vp, iflags)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	u_int iflags;
{
	MARK *tm;
	recno_t lno;
	size_t len;
	char *p;
	u_int flags;

	sp->showmode = "Change";
	flags = set_txt_std(sp, vp, iflags);

	/*
	 * There are two cases -- if a count is supplied, we do a line
	 * mode change where we delete the lines and then insert text
	 * into a new line.  Otherwise, we replace the current line.
	 */
	vp->m_stop.lno =
	    vp->m_start.lno + (F_ISSET(vp, VC_C1SET) ? vp->count - 1 : 0);
	if (vp->m_start.lno != vp->m_stop.lno) {
		/* Make sure that the to line is real. */
		if (file_gline(sp, ep,
		    vp->m_stop.lno, &vp->m_stop.cno) == NULL) {
			v_eof(sp, ep, &vp->m_start);
			return (1);
		}
		if (vp->m_stop.cno != 0)
			--vp->m_stop.cno;

		/*
		 * Cut the lines.
		 *
		 * !!!
		 * Historic practice, C and S did not cut into the numeric
		 * buffers, only the unnamed one.
		 */
		if (cut(sp, ep,
		    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
		    &vp->m_start, &vp->m_stop, CUT_LINEMODE))
			return (1);

		/* Insert a line while we still can... */
		if (file_iline(sp, ep, vp->m_start.lno, "", 0))
			return (1);
		++vp->m_start.lno;
		++vp->m_stop.lno;

		/* Delete the lines. */
		if (delete(sp, ep, &vp->m_start, &vp->m_stop, 1))
			return (1);

		/* Get the inserted line. */
		if ((p = file_gline(sp, ep, --vp->m_start.lno, &len)) == NULL) {
			GETLINE_ERR(sp, vp->m_start.lno);
			return (1);
		}
		tm = NULL;
		sp->lno = vp->m_start.lno;
		sp->cno = 0;
		LF_SET(TXT_APPENDEOL);
	} else {
		/* The line may be empty, but that's okay. */
		if ((p = file_gline(sp, ep, vp->m_start.lno, &len)) == NULL) {
			if (file_lline(sp, ep, &lno))
				return (1);
			if (lno != 0) {
				GETLINE_ERR(sp, vp->m_start.lno);
				return (1);
			}
			vp->m_stop.cno = len = 0;
			LF_SET(TXT_APPENDEOL);
		} else {
			if (len == 0) {
				vp->m_stop.cno = 0;
				LF_SET(TXT_APPENDEOL);
			} else
				vp->m_stop.cno = len - 1;
			/*
			 * !!!
			 * Historic practice, C and S did not cut into the
			 * numeric buffers, only the unnamed one.
			 */
			if (cut(sp, ep,
			    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
			    &vp->m_start, &vp->m_stop, CUT_LINEMODE))
				return (1);
			LF_SET(TXT_EMARK | TXT_OVERWRITE);
		}
		tm = &vp->m_stop;
	}

	/* Correct logging for implied cursor motion. */
	LOG_CORRECT;

	return (v_ntext(sp, ep,
	    sp->tiqp, tm, p, len, &vp->m_final, 0, OOBLNO, flags));
}

/*
 * v_change -- [buffer][count]c[count]motion
 *	Change command.
 */
int
v_change(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;
	size_t blen, len;
	u_int flags;
	int lmode, rval;
	char *bp, *p;

	sp->showmode = "Change";
	flags = set_txt_std(sp, vp, 0);

	/*
	 * Move the cursor to the start of the change.  Note, if autoindent
	 * is turned on, the cc command in line mode changes from the first
	 * *non-blank* character of the line, not the first character.  And,
	 * to make it just a bit more exciting, the initial space is handled
	 * as auto-indent characters.
	 */
	lmode = F_ISSET(vp, VM_LMODE) ? CUT_LINEMODE : 0;
	if (lmode) {
		vp->m_start.cno = 0;
		if (O_ISSET(sp, O_AUTOINDENT)) {
			if (nonblank(sp, ep, vp->m_start.lno, &vp->m_start.cno))
				return (1);
			LF_SET(TXT_AICHARS);
		}
	}
	sp->lno = vp->m_start.lno;
	sp->cno = vp->m_start.cno;

	/* Correct logging for implied cursor motion. */
	LOG_CORRECT;

	/*
	 * 'c' can be combined with motion commands that set the resulting
	 * cursor position, i.e. "cG".  Clear the VM_RCM flags and make the
	 * resulting cursor position stick, inserting text has its own rules
	 * for cursor positioning.
	 */
	F_CLR(vp, VM_RCM_MASK);
	F_SET(vp, VM_RCM_SET);

	/*
	 * If not in line mode and changing within a single line, the line
	 * either currently has text or it doesn't.  If it doesn't, insert
	 * some.  Otherwise, copy it and overwrite it.
	 */
	if (!lmode && vp->m_start.lno == vp->m_stop.lno) {
		if ((p = file_gline(sp, ep, vp->m_start.lno, &len)) == NULL) {
			if (p == NULL) {
				if (file_lline(sp, ep, &lno))
					return (1);
				if (lno != 0) {
					GETLINE_ERR(sp, vp->m_start.lno);
					return (1);
				}
			}
			vp->m_stop.cno = len = 0;
			LF_SET(TXT_APPENDEOL);
		} else {
			/*
			 * !!!
			 * Historic practice, c cut into the numeric buffers,
			 * as well as the unnamed one.
			 */
			if (cut(sp, ep,
			    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
			    &vp->m_start, &vp->m_stop, lmode | CUT_NUMOPT))
				return (1);
			if (len == 0)
				LF_SET(TXT_APPENDEOL);
			LF_SET(TXT_EMARK | TXT_OVERWRITE);
		}
		return (v_ntext(sp, ep, sp->tiqp,
		    &vp->m_stop, p, len, &vp->m_final, 0, OOBLNO, flags));
	}

	/*
	 * It's trickier if changing over multiple lines.  If we're in
	 * line mode we delete all of the lines and insert a replacement
	 * line which the user edits.  If there was leading whitespace
	 * in the first line being changed, we copy it and use it as the
	 * replacement.  If we're not in line mode, we just delete the
	 * text and start inserting.
	 *
	 * !!!
	 * Historic practice, c cut into the numeric buffers, as well as the
	 * unnamed one.
	 *
	 * Copy the text.
	 */
	if (cut(sp, ep,
	    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop, lmode | CUT_NUMOPT))
		return (1);

	/* If replacing entire lines and there's leading text. */
	if (lmode && vp->m_start.cno) {
		/* Get a copy of the first line changed. */
		if ((p = file_gline(sp, ep, vp->m_start.lno, &len)) == NULL) {
			GETLINE_ERR(sp, vp->m_start.lno);
			return (1);
		}
		/* Copy the leading text elsewhere. */
		GET_SPACE_RET(sp, bp, blen, vp->m_start.cno);
		memmove(bp, p, vp->m_start.cno);
	} else
		bp = NULL;

	/* Delete the text. */
	if (delete(sp, ep, &vp->m_start, &vp->m_stop, lmode))
		return (1);

	/* If replacing entire lines, insert a replacement line. */
	if (lmode) {
		if (file_iline(sp, ep, vp->m_start.lno, bp, vp->m_start.cno))
			return (1);
		sp->lno = vp->m_start.lno;
		len = sp->cno = vp->m_start.cno;
	}

	/* Get the line we're editing. */
	if ((p = file_gline(sp, ep, vp->m_start.lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno != 0) {
			GETLINE_ERR(sp, vp->m_start.lno);
			return (1);
		}
		len = 0;
	}

	/* Check to see if we're appending to the line. */
	if (vp->m_start.cno >= len)
		LF_SET(TXT_APPENDEOL);

	rval = v_ntext(sp, ep,
	    sp->tiqp, NULL, p, len, &vp->m_final, 0, OOBLNO, flags);

	if (bp != NULL)
		FREE_SPACE(sp, bp, blen);
	return (rval);
}

/*
 * v_Replace -- [count]R
 *	Overwrite multiple characters.
 */
int
v_Replace(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;
	u_long cnt;
	size_t len;
	u_int flags;
	char *p;

	sp->showmode = "Replace";
	flags = set_txt_std(sp, vp, 0);

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	if ((p = file_gline(sp, ep, vp->m_start.lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno != 0) {
			GETLINE_ERR(sp, vp->m_start.lno);
			return (1);
		}
		len = 0;
		LF_SET(TXT_APPENDEOL);
	} else {
		if (len == 0)
			LF_SET(TXT_APPENDEOL);
		LF_SET(TXT_OVERWRITE | TXT_REPLACE);
	}
	vp->m_stop.lno = vp->m_start.lno;
	vp->m_stop.cno = len ? len - 1 : 0;
	if (v_ntext(sp, ep, sp->tiqp,
	    &vp->m_stop, p, len, &vp->m_final, 0, OOBLNO, flags))
		return (1);

	/*
	 * Special case.  The historic vi handled [count]R badly, in that R
	 * would replace some number of characters, and then the count would
	 * append count-1 copies of the replacing chars to the replaced space.
	 * This seems wrong, so this version counts R commands.  There is some
	 * trickiness in moving back to where the user stopped replacing after
	 * each R command.  Basically, if the user ended with a newline, we
	 * want to use vp->m_final.cno (which will be 0).  Otherwise, use the
	 * column after the returned cursor, unless it would be past the end of
	 * the line, in which case we append to the line.
	 */
	while (--cnt) {
		if ((p = file_gline(sp, ep, vp->m_final.lno, &len)) == NULL)
			GETLINE_ERR(sp, vp->m_final.lno);
		flags = set_txt_std(sp, vp, TXT_REPLAY);

		sp->lno = vp->m_final.lno;

		if (len == 0 || vp->m_final.cno == len - 1) {
			sp->cno = len;
			LF_SET(TXT_APPENDEOL);
		} else {
			sp->cno = vp->m_final.cno;
			if (vp->m_final.cno != 0)
				++sp->cno;
			LF_SET(TXT_OVERWRITE | TXT_REPLACE);
		}

		vp->m_stop.lno = sp->lno;
		vp->m_stop.cno = sp->cno;
		if (v_ntext(sp, ep, sp->tiqp,
		    &vp->m_stop, p, len, &vp->m_final, 0, OOBLNO, flags))
			return (1);
	}
	return (0);
}

/*
 * v_subst -- [buffer][count]s
 *	Substitute characters.
 */
int
v_subst(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;
	size_t len;
	u_int flags;
	char *p;

	sp->showmode = "Change";
	flags = set_txt_std(sp, vp, 0);
	if ((p = file_gline(sp, ep, vp->m_start.lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno != 0) {
			GETLINE_ERR(sp, vp->m_start.lno);
			return (1);
		}
		len = 0;
		LF_SET(TXT_APPENDEOL);
	} else {
		if (len == 0)
			LF_SET(TXT_APPENDEOL);
		LF_SET(TXT_EMARK | TXT_OVERWRITE);
	}

	vp->m_stop.lno = vp->m_start.lno;
	vp->m_stop.cno =
	    vp->m_start.cno + (F_ISSET(vp, VC_C1SET) ? vp->count - 1 : 0);
	if (vp->m_stop.cno > len - 1)
		vp->m_stop.cno = len - 1;

	if (p != NULL && cut(sp, ep,
	    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop, 0))
		return (1);

	return (v_ntext(sp, ep, sp->tiqp,
	    &vp->m_stop, p, len, &vp->m_final, 0, OOBLNO, flags));
}

/*
 * set_txt_std --
 *	Initialize text processing flags.
 */
static u_int
set_txt_std(sp, vp, init)
	SCR *sp;
	VICMDARG *vp;
	u_int init;
{
	u_int flags;

	/* Text operations are all interruptible. */
	F_SET(sp, S_INTERRUPTIBLE);

	LF_INIT(init);
	LF_SET(TXT_CNTRLT |
	    TXT_ESCAPE | TXT_MAPINPUT | TXT_RECORD | TXT_RESOLVE);
	if (O_ISSET(sp, O_ALTWERASE))
		LF_SET(TXT_ALTWERASE);
	if (O_ISSET(sp, O_AUTOINDENT))
		LF_SET(TXT_AUTOINDENT);
	if (O_ISSET(sp, O_BEAUTIFY))
		LF_SET(TXT_BEAUTIFY);
	if (O_ISSET(sp, O_SHOWMATCH))
		LF_SET(TXT_SHOWMATCH);
	if (O_ISSET(sp, O_WRAPMARGIN))
		LF_SET(TXT_WRAPMARGIN);
	if (F_ISSET(sp, S_SCRIPT))
		LF_SET(TXT_CR);
	if (O_ISSET(sp, O_TTYWERASE))
		LF_SET(TXT_TTYWERASE);
	if (F_ISSET(vp,  VC_ISDOT))
		LF_SET(TXT_REPLAY);
	return (flags);
}
