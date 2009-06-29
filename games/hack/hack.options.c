/*	$NetBSD: hack.options.c,v 1.7.26.1 2009/06/29 23:43:48 snj Exp $	*/

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
__RCSID("$NetBSD: hack.options.c,v 1.7.26.1 2009/06/29 23:43:48 snj Exp $");
#endif				/* not lint */

#include <stdlib.h>
#include <unistd.h>
#include "hack.h"
#include "extern.h"

void
initoptions()
{
	char           *opts;

	flags.time = flags.nonews = flags.notombstone = flags.end_own =
		flags.standout = flags.nonull = FALSE;
	flags.no_rest_on_space = TRUE;
	flags.invlet_constant = TRUE;
	flags.end_top = 5;
	flags.end_around = 4;
	flags.female = FALSE;	/* players are usually male */

	if ((opts = getenv("HACKOPTIONS")) != NULL)
		parseoptions(opts, TRUE);
}

void
parseoptions(opts, from_env)
	char           *opts;
	boolean         from_env;
{
	char           *op, *op2;
	unsigned        num;
	boolean         negated;

	if ((op = strchr(opts, ',')) != NULL) {
		*op++ = 0;
		parseoptions(op, from_env);
	}
	if ((op = strchr(opts, ' ')) != NULL) {
		op2 = op;
		while (*op++)
			if (*op != ' ')
				*op2++ = *op;
	}
	if (!*opts)
		return;
	negated = FALSE;
	while ((*opts == '!') || !strncmp(opts, "no", 2)) {
		if (*opts == '!')
			opts++;
		else
			opts += 2;
		negated = !negated;
	}

	if (!strncmp(opts, "standout", 8)) {
		flags.standout = !negated;
		return;
	}
	if (!strncmp(opts, "null", 3)) {
		flags.nonull = negated;
		return;
	}
	if (!strncmp(opts, "tombstone", 4)) {
		flags.notombstone = negated;
		return;
	}
	if (!strncmp(opts, "news", 4)) {
		flags.nonews = negated;
		return;
	}
	if (!strncmp(opts, "time", 4)) {
		flags.time = !negated;
		flags.botl = 1;
		return;
	}
	if (!strncmp(opts, "restonspace", 4)) {
		flags.no_rest_on_space = negated;
		return;
	}
	if (!strncmp(opts, "fixinv", 4)) {
		if (from_env)
			flags.invlet_constant = !negated;
		else
			pline("The fixinvlet option must be in HACKOPTIONS.");
		return;
	}
	if (!strncmp(opts, "male", 4)) {
		flags.female = negated;
		return;
	}
	if (!strncmp(opts, "female", 6)) {
		flags.female = !negated;
		return;
	}
	/* name:string */
	if (!strncmp(opts, "name", 4)) {
		if (!from_env) {
			pline("The playername can be set only from HACKOPTIONS.");
			return;
		}
		op = strchr(opts, ':');
		if (!op)
			goto bad;
		(void) strncpy(plname, op + 1, sizeof(plname) - 1);
		return;
	}
	/* endgame:5t[op] 5a[round] o[wn] */
	if (!strncmp(opts, "endgame", 3)) {
		op = strchr(opts, ':');
		if (!op)
			goto bad;
		op++;
		while (*op) {
			num = 1;
			if (digit(*op)) {
				num = atoi(op);
				while (digit(*op))
					op++;
			} else if (*op == '!') {
				negated = !negated;
				op++;
			}
			switch (*op) {
			case 't':
				flags.end_top = num;
				break;
			case 'a':
				flags.end_around = num;
				break;
			case 'o':
				flags.end_own = !negated;
				break;
			default:
				goto bad;
			}
			while (letter(*++op));
			if (*op == '/')
				op++;
		}
		return;
	}
bad:
	if (!from_env) {
		if (!strncmp(opts, "help", 4)) {
			pline("%s%s%s",
			      "To set options use `HACKOPTIONS=\"<options>\"' in your environment, or ",
			      "give the command 'O' followed by the line `<options>' while playing. ",
			      "Here <options> is a list of <option>s separated by commas.");
			pline("%s%s%s",
			      "Simple (boolean) options are rest_on_space, news, time, ",
			      "null, tombstone, (fe)male. ",
			      "These can be negated by prefixing them with '!' or \"no\".");
			pline("%s",
			      "A string option is name, as in HACKOPTIONS=\"name:Merlin-W\".");
			pline("%s%s%s",
			      "A compound option is endgame; it is followed by a description of what ",
			      "parts of the scorelist you want to see. You might for example say: ",
			      "`endgame:own scores/5 top scores/4 around my score'.");
			return;
		}
		pline("Bad option: %s.", opts);
		pline("Type `O help<cr>' for help.");
		return;
	}
	puts("Bad syntax in HACKOPTIONS.");
	puts("Use for example:");
	puts(
	     "HACKOPTIONS=\"!restonspace,notombstone,endgame:own/5 topscorers/4 around me\""
		);
	getret();
}

int
doset()
{
	char buf[BUFSZ];
	size_t pos;

	pline("What options do you want to set? ");
	getlin(buf);
	if (!buf[0] || buf[0] == '\033') {
		(void) strcpy(buf, "HACKOPTIONS=");
		(void) strcat(buf, flags.female ? "female," : "male,");
		if (flags.standout)
			(void) strlcat(buf, "standout,", sizeof(buf));
		if (flags.nonull)
			(void) strlcat(buf, "nonull,", sizeof(buf));
		if (flags.nonews)
			(void) strlcat(buf, "nonews,", sizeof(buf));
		if (flags.time)
			(void) strlcat(buf, "time,", sizeof(buf));
		if (flags.notombstone)
			(void) strlcat(buf, "notombstone,", sizeof(buf));
		if (flags.no_rest_on_space)
			(void) strlcat(buf, "!rest_on_space,", sizeof(buf));
		if (flags.end_top != 5 || flags.end_around != 4 || flags.end_own) {
			pos = strlen(buf);
			(void) snprintf(buf+pos, sizeof(buf)-pos,
				       "endgame: %u topscores/%u around me",
				       flags.end_top, flags.end_around);
			if (flags.end_own)
				(void) strlcat(buf, "/own scores", sizeof(buf));
		} else {
			char           *eop = eos(buf);
			if (*--eop == ',')
				*eop = 0;
		}
		pline(buf);
	} else
		parseoptions(buf, FALSE);

	return (0);
}
