/*	$NetBSD: lisp.c,v 1.8.2.1 2004/06/22 07:20:43 tron Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(lint)
#if 0
static char sccsid[] = "@(#)lisp.c	8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: lisp.c,v 1.8.2.1 2004/06/22 07:20:43 tron Exp $");
#endif
#endif /* not lint */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "ctags.h"

/*
 * lisp tag functions
 * just look for (def or (DEF
 */
void
l_entries()
{
	int	special;
	char	*cp;
	char	savedc;
	char	tok[MAXTOKEN];

	for (;;) {
		lineftell = ftell(inf);
		if (!fgets(lbuf, sizeof(lbuf), inf))
			return;
		++lineno;
		lbp = lbuf;
		if (!cicmp("(def"))
			continue;
		special = NO;
		switch(*lbp | ' ') {
		case 'm':
			if (cicmp("method"))
				special = YES;
			break;
		case 'w':
			if (cicmp("wrapper") || cicmp("whopper"))
				special = YES;
		}
		for (; !isspace((unsigned char)*lbp); ++lbp)
			continue;
		for (; isspace((unsigned char)*lbp); ++lbp)
			continue;
		for (cp = lbp; *cp && *cp != '\n'; ++cp)
			continue;
		*cp = EOS;
		if (special) {
			if (!(cp = strchr(lbp, ')')))
				continue;
			for (; cp >= lbp && *cp != ':'; --cp)
				continue;
			if (cp < lbp)
				continue;
			lbp = cp;
			for (; *cp && *cp != ')' && *cp != ' '; ++cp)
				continue;
		}
		else
			for (cp = lbp + 1;
			    *cp && *cp != '(' && *cp != ' '; ++cp)
				continue;
		savedc = *cp;
		*cp = EOS;
		(void)strlcpy(tok, lbp, sizeof(tok));
		*cp = savedc;
		getline();
		pfnote(tok, lineno);
	}
	/*NOTREACHED*/
}
