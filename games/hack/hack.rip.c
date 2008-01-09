/*	$NetBSD: hack.rip.c,v 1.7.22.1 2008/01/09 01:30:50 matt Exp $	*/

/*
 * Copyright (c) 1985, Stichting Centrum voor Wiskunde en Informatica,
 * Amsterdam
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the Stichting Centrum voor Wiskunde en
 * Informatica, nor the names of its contributors may be used to endorse or
 * promote products derived from this software without specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982 Jay Fenlason <hack@gnu.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: hack.rip.c,v 1.7.22.1 2008/01/09 01:30:50 matt Exp $");
#endif				/* not lint */

#include "hack.h"
#include "extern.h"

static const char    *riptop = "\
                       ----------\n\
                      /          \\\n\
                     /    REST    \\\n\
                    /      IN      \\\n\
                   /     PEACE      \\\n\
                  /                  \\";

static const char    *ripmid = "                  | %*s%*s |\n";

static const char    *ripbot = "\
                 *|     *  *  *      | *\n\
        _________)/\\\\_//(\\/(/\\)/\\//\\/|_)_______";

void
outrip()
{
	char            buf[BUFSZ];

	cls();
	curs(1, 8);
	puts(riptop);
	(void) strcpy(buf, plname);
	buf[16] = 0;
	center(6, buf);
	(void) sprintf(buf, "%ld AU", u.ugold);
	center(7, buf);
	(void) sprintf(buf, "killed by%s",
		       !strncmp(killer, "the ", 4) ? "" :
		       !strcmp(killer, "starvation") ? "" :
		       strchr(vowels, *killer) ? " an" : " a");
	center(8, buf);
	(void) strcpy(buf, killer);
	{
		int             i1;
		if ((i1 = strlen(buf)) > 16) {
			int             i, i0;
			i0 = i1 = 0;
			for (i = 0; i <= 16; i++)
				if (buf[i] == ' ')
					i0 = i, i1 = i + 1;
			if (!i0)
				i0 = i1 = 16;
			buf[i1 + 16] = 0;
			buf[i0] = 0;
		}
		center(9, buf);
		center(10, buf + i1);
	}
	(void) sprintf(buf, "%4d", getyear());
	center(11, buf);
	puts(ripbot);
	getret();
}

void
center(line, text)
	int             line __unused;
	char           *text;
{
	int             n = strlen(text) / 2;
	printf(ripmid, 8 + n, text, 8 - n, "");
}
