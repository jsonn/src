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
static char sccsid[] = "@(#)v_sentence.c	8.17 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * !!!
 * In historic vi, a sentence was delimited by a '.', '?' or '!' character
 * followed by TWO spaces or a newline.  One or more empty lines was also
 * treated as a separate sentence.  The Berkeley documentation for historical
 * vi states that any number of ')', ']', '"' and '\'' characters can be
 * between the delimiter character and the spaces or end of line, however,
 * the historical implementation did not handle additional '"' characters.
 * We follow the documentation here, not the implementation.
 *
 * Once again, historical vi didn't do sentence movements associated with
 * counts consistently, mostly in the presence of lines containing only
 * white-space characters.
 *
 * This implementation also permits a single tab to delimit sentences, and
 * treats lines containing only white-space characters as empty lines.
 * Finally, tabs are eaten (along with spaces) when skipping to the start
 * of the text following a "sentence".
 */

/*
 * v_sentencef -- [count])
 *	Move forward count sentences.
 */
int
v_sentencef(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	enum { BLANK, NONE, PERIOD } state;
	VCS cs;
	size_t len;
	u_long cnt;

	cs.cs_lno = vp->m_start.lno;
	cs.cs_cno = vp->m_start.cno;
	if (cs_init(sp, ep, &cs))
		return (1);

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;

	/*
	 * !!!
	 * If in white-space, the next start of sentence counts as one.
	 * This may not handle "  .  " correctly, but it's real unclear
	 * what correctly means in that case.
	 */
	if (cs.cs_flags == CS_EMP || cs.cs_flags == 0 && isblank(cs.cs_ch)) {
		if (cs_fblank(sp, ep, &cs))
			return (1);
		if (--cnt == 0) {
			if (vp->m_start.lno != cs.cs_lno ||
			    vp->m_start.cno != cs.cs_cno)
				goto okret;
			return (1);
		}
	}

	for (state = NONE;;) {
		if (cs_next(sp, ep, &cs))
			return (1);
		if (cs.cs_flags == CS_EOF)
			break;
		if (cs.cs_flags == CS_EOL) {
			if ((state == PERIOD || state == BLANK) && --cnt == 0) {
				if (cs_next(sp, ep, &cs))
					return (1);
				if (cs.cs_flags == 0 &&
				    isblank(cs.cs_ch) && cs_fblank(sp, ep, &cs))
					return (1);
				goto okret;
			}
			state = NONE;
			continue;
		}
		if (cs.cs_flags == CS_EMP) {	/* An EMP is two sentences. */
			if (--cnt == 0)
				goto okret;
			if (cs_fblank(sp, ep, &cs))
				return (1);
			if (--cnt == 0)
				goto okret;
			state = NONE;
			continue;
		}
		switch (cs.cs_ch) {
		case '.':
		case '?':
		case '!':
			state = PERIOD;
			break;
		case ')':
		case ']':
		case '"':
		case '\'':
			if (state != PERIOD)
				state = NONE;
			break;
		case '\t':
			if (state == PERIOD)
				state = BLANK;
			/* FALLTHROUGH */
		case ' ':
			if (state == PERIOD) {
				state = BLANK;
				break;
			}
			if (state == BLANK && --cnt == 0) {
				if (cs_fblank(sp, ep, &cs))
					return (1);
				goto okret;
			}
			/* FALLTHROUGH */
		default:
			state = NONE;
			break;
		}
	}

	/* EOF is a movement sink, but it's an error not to have moved. */
	if (vp->m_start.lno == cs.cs_lno && vp->m_start.cno == cs.cs_cno) {
		v_eof(sp, ep, NULL);
		return (1);
	}

okret:	vp->m_stop.lno = cs.cs_lno;
	vp->m_stop.cno = cs.cs_cno;

	/*
	 * !!!
	 * Historic, uh, features, yeah, that's right, call 'em features.
	 * If the ending cursor position is at the first column in the
	 * line, i.e. the movement is cutting an entire line, the buffer
	 * is in line mode, and the ending position is the last character
	 * of the previous line.
	 *
	 * Non-motion commands move to the end of the range.  VC_D and
	 * VC_Y stay at the start.  Ignore VC_C and VC_DEF.  Adjust the
	 * end of the range for motion commands.
	 */
	if (ISMOTION(vp)) {
		if (vp->m_start.cno == 0 &&
		    (cs.cs_flags != 0 || vp->m_stop.cno == 0)) {
			if (file_gline(sp, ep,
			    --vp->m_stop.lno, &len) == NULL) {
				GETLINE_ERR(sp, vp->m_stop.lno);
				return (1);
			}
			vp->m_stop.cno = len ? len - 1 : 0;
			F_SET(vp, VM_LMODE);
		} else
			--vp->m_stop.cno;
		vp->m_final = vp->m_start;
	} else
		vp->m_final = vp->m_stop;
	return (0);
}

/*
 * v_sentenceb -- [count](
 *	Move backward count sentences.
 */
int
v_sentenceb(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	VCS cs;
	recno_t slno;
	size_t len, scno;
	u_long cnt;
	int last;

	/*
	 * !!!
	 * Historic vi permitted the user to hit SOF repeatedly.
	 */
	if (vp->m_start.lno == 1 && vp->m_start.cno == 0)
		return (0);

	cs.cs_lno = vp->m_start.lno;
	cs.cs_cno = vp->m_start.cno;
	if (cs_init(sp, ep, &cs))
		return (1);

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;

	/*
	 * !!!
	 * In empty lines, skip to the previous non-white-space character.
	 * If in text, skip to the prevous white-space character.  Believe
	 * it or not, in the paragraph:
	 *	ab cd.
	 *	AB CD.
	 * if the cursor is on the 'A' or 'B', ( moves to the 'a'.  If it
	 * is on the ' ', 'C' or 'D', it moves to the 'A'.  Yes, Virginia,
	 * Berkeley was once a major center of drug activity.
	 */
	if (cs.cs_flags == CS_EMP) {
		if (cs_bblank(sp, ep, &cs))
			return (1);
		for (;;) {
			if (cs_prev(sp, ep, &cs))
				return (1);
			if (cs.cs_flags != CS_EOL)
				break;
		}
	} else if (cs.cs_flags == 0 && !isblank(cs.cs_ch))
		for (;;) {
			if (cs_prev(sp, ep, &cs))
				return (1);
			if (cs.cs_flags != 0 || isblank(cs.cs_ch))
				break;
		}

	for (last = 0;;) {
		if (cs_prev(sp, ep, &cs))
			return (1);
		if (cs.cs_flags == CS_SOF)	/* SOF is a movement sink. */
			break;
		if (cs.cs_flags == CS_EOL) {
			last = 1;
			continue;
		}
		if (cs.cs_flags == CS_EMP) {
			if (--cnt == 0)
				goto ret;
			if (cs_bblank(sp, ep, &cs))
				return (1);
			last = 0;
			continue;
		}
		switch (cs.cs_ch) {
		case '.':
		case '?':
		case '!':
			if (!last || --cnt != 0) {
				last = 0;
				continue;
			}

ret:			slno = cs.cs_lno;
			scno = cs.cs_cno;

			/*
			 * Move to the start of the sentence, skipping blanks
			 * and special characters.
			 */
			do {
				if (cs_next(sp, ep, &cs))
					return (1);
			} while (!cs.cs_flags &&
			    (cs.cs_ch == ')' || cs.cs_ch == ']' ||
			    cs.cs_ch == '"' || cs.cs_ch == '\''));
			if ((cs.cs_flags || isblank(cs.cs_ch)) &&
			    cs_fblank(sp, ep, &cs))
				return (1);

			/*
			 * If it was ".  xyz", with the cursor on the 'x', or
			 * "end.  ", with the cursor in the spaces, or the
			 * beginning of a sentence preceded by an empty line,
			 * we can end up where we started.  Fix it.
			 */
			if (vp->m_start.lno != cs.cs_lno ||
			    vp->m_start.cno != cs.cs_cno)
				goto okret;

			/*
			 * Well, if an empty line preceded possible blanks
			 * and the sentence, it could be a real sentence.
			 */
			for (;;) {
				if (cs_prev(sp, ep, &cs))
					return (1);
				if (cs.cs_flags == CS_EOL)
					continue;
				if (cs.cs_flags == 0 && isblank(cs.cs_ch))
					continue;
				break;
			}
			if (cs.cs_flags == CS_EMP)
				goto okret;

			/* But it wasn't; try again. */
			++cnt;
			cs.cs_lno = slno;
			cs.cs_cno = scno;
			last = 0;
			break;
		case '\t':
			last = 1;
			break;
		default:
			last =
			    cs.cs_flags == CS_EOL || isblank(cs.cs_ch) ||
			    cs.cs_ch == ')' || cs.cs_ch == ']' ||
			    cs.cs_ch == '"' || cs.cs_ch == '\'' ? 1 : 0;
		}
	}

okret:	vp->m_stop.lno = cs.cs_lno;
	vp->m_stop.cno = cs.cs_cno;

	/*
	 * !!!
	 * If the starting and stopping cursor positions are at the first
	 * columns in the line, i.e. the movement is cutting an entire line,
	 * the buffer is in line mode, and the starting position is the last
	 * character of the previous line.
	 *
	 * All commands move to the end of the range.  Adjust the start of
	 * the range for motion commands.
	 */
	if (ISMOTION(vp))
		if (vp->m_start.cno == 0 &&
		    (cs.cs_flags != 0 || vp->m_stop.cno == 0)) {
			if (file_gline(sp, ep,
			    --vp->m_start.lno, &len) == NULL) {
				GETLINE_ERR(sp, vp->m_start.lno);
				return (1);
			}
			vp->m_start.cno = len ? len - 1 : 0;
			F_SET(vp, VM_LMODE);
		} else
			--vp->m_start.cno;
	vp->m_final = vp->m_stop;
	return (0);
}
