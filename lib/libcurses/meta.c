/*	$NetBSD: meta.c,v 1.5.20.1 2007/01/21 17:43:35 jdc Exp $	*/

/*-
 * Copyright (c) 1998-2000 Brett Lymn
 *                         (blymn@baea.com.au, brett_lymn@yahoo.com.au)
 * All rights reserved.
 *
 * This code has been donated to The NetBSD Foundation by the Author.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 *
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: meta.c,v 1.5.20.1 2007/01/21 17:43:35 jdc Exp $");
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

/*
 * meta --
 *    Turn on or off the terminal meta mode.
 */
int
meta(/*ARGSUSED*/ WINDOW *win, bool bf)
{
	if (bf == TRUE) {
		if (__tc_mm != NULL) {
#ifdef DEBUG
			__CTRACE(__CTRACE_MISC, "meta: TRUE\n");
#endif
			tputs(__tc_mm, 0, __cputchar);
			_cursesi_screen->meta_state = TRUE;
		}
	} else {
		if (__tc_mo != NULL) {
#ifdef DEBUG
			__CTRACE(__CTRACE_MISC, "meta: FALSE\n");
#endif
			tputs(__tc_mo, 0, __cputchar);
			_cursesi_screen->meta_state = FALSE;
		}
	}

	return OK;
}

/*
 * __restore_meta_state --
 *    Restore old meta state.
 */
void
__restore_meta_state(void)
{
	meta(NULL, _cursesi_screen->meta_state);
}

