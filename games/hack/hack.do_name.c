/*	$NetBSD: hack.do_name.c,v 1.6.26.1 2009/06/29 23:43:48 snj Exp $	*/

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
__RCSID("$NetBSD: hack.do_name.c,v 1.6.26.1 2009/06/29 23:43:48 snj Exp $");
#endif				/* not lint */

#include <stdlib.h>
#include "hack.h"
#include "extern.h"

coord
getpos(force, goal)
	int             force;
	const char           *goal;
{
	int             cx, cy, i, c;
	coord           cc;
	pline("(For instructions type a ?)");
	cx = u.ux;
	cy = u.uy;
	curs(cx, cy + 2);
	while ((c = readchar()) != '.') {
		for (i = 0; i < 8; i++)
			if (sdir[i] == c) {
				if (1 <= cx + xdir[i] && cx + xdir[i] <= COLNO)
					cx += xdir[i];
				if (0 <= cy + ydir[i] && cy + ydir[i] <= ROWNO - 1)
					cy += ydir[i];
				goto nxtc;
			}
		if (c == '?') {
			pline("Use [hjkl] to move the cursor to %s.", goal);
			pline("Type a . when you are at the right place.");
		} else {
			pline("Unknown direction: '%s' (%s).",
			      visctrl(c),
			      force ? "use hjkl or ." : "aborted");
			if (force)
				goto nxtc;
			cc.x = -1;
			cc.y = 0;
			return (cc);
		}
nxtc:		;
		curs(cx, cy + 2);
	}
	cc.x = cx;
	cc.y = cy;
	return (cc);
}

int
do_mname()
{
	char            buf[BUFSZ];
	coord           cc;
	int             cx, cy, lth, i;
	struct monst   *mtmp, *mtmp2;
	cc = getpos(0, "the monster you want to name");
	cx = cc.x;
	cy = cc.y;
	if (cx < 0)
		return (0);
	mtmp = m_at(cx, cy);
	if (!mtmp) {
		if (cx == u.ux && cy == u.uy)
			pline("This ugly monster is called %s and cannot be renamed.",
			      plname);
		else
			pline("There is no monster there.");
		return (1);
	}
	if (mtmp->mimic) {
		pline("I see no monster there.");
		return (1);
	}
	if (!cansee(cx, cy)) {
		pline("I cannot see a monster there.");
		return (1);
	}
	pline("What do you want to call %s? ", lmonnam(mtmp));
	getlin(buf);
	clrlin();
	if (!*buf || *buf == '\033')
		return (1);
	lth = strlen(buf) + 1;
	if (lth > 63) {
		buf[62] = 0;
		lth = 63;
	}
	mtmp2 = newmonst(mtmp->mxlth + lth);
	*mtmp2 = *mtmp;
	for (i = 0; i < mtmp->mxlth; i++)
		((char *) mtmp2->mextra)[i] = ((char *) mtmp->mextra)[i];
	mtmp2->mnamelth = lth;
	(void) strcpy(NAME(mtmp2), buf);
	replmon(mtmp, mtmp2);
	return (1);
}

/*
 * This routine changes the address of  obj . Be careful not to call it
 * when there might be pointers around in unknown places. For now: only
 * when  obj  is in the inventory.
 */
void
do_oname(obj)
	struct obj     *obj;
{
	struct obj     *otmp, *otmp2;
	int lth;
	char            buf[BUFSZ];
	pline("What do you want to name %s? ", doname(obj));
	getlin(buf);
	clrlin();
	if (!*buf || *buf == '\033')
		return;
	lth = strlen(buf) + 1;
	if (lth > 63) {
		buf[62] = 0;
		lth = 63;
	}
	otmp2 = newobj(lth);
	*otmp2 = *obj;
	otmp2->onamelth = lth;
	(void) strcpy(ONAME(otmp2), buf);

	setworn((struct obj *) 0, obj->owornmask);
	setworn(otmp2, otmp2->owornmask);

	/*
	 * do freeinv(obj); etc. by hand in order to preserve the position of
	 * this object in the inventory
	 */
	if (obj == invent)
		invent = otmp2;
	else
		for (otmp = invent;; otmp = otmp->nobj) {
			if (!otmp)
				panic("Do_oname: cannot find obj.");
			if (otmp->nobj == obj) {
				otmp->nobj = otmp2;
				break;
			}
		}
#if 0
	obfree(obj, otmp2);	/* now unnecessary: no pointers on bill */
#endif
	free((char *) obj);	/* let us hope nobody else saved a pointer */
}

int
ddocall()
{
	struct obj     *obj;

	pline("Do you want to name an individual object? [ny] ");
	switch (readchar()) {
	case '\033':
		break;
	case 'y':
		obj = getobj("#", "name");
		if (obj)
			do_oname(obj);
		break;
	default:
		obj = getobj("?!=/", "call");
		if (obj)
			docall(obj);
	}
	return (0);
}

void
docall(obj)
	struct obj     *obj;
{
	char            buf[BUFSZ];
	struct obj      otemp;
	char          **str1;
	char           *str;

	otemp = *obj;
	otemp.quan = 1;
	otemp.onamelth = 0;
	str = xname(&otemp);
	pline("Call %s %s: ", strchr(vowels, *str) ? "an" : "a", str);
	getlin(buf);
	clrlin();
	if (!*buf || *buf == '\033')
		return;
	str = newstring(strlen(buf) + 1);
	(void) strcpy(str, buf);
	str1 = &(objects[obj->otyp].oc_uname);
	if (*str1)
		free(*str1);
	*str1 = str;
}

const char *const ghostnames[] = {/* these names should have length < PL_NSIZ */
	"adri", "andries", "andreas", "bert", "david", "dirk", "emile",
	"frans", "fred", "greg", "hether", "jay", "john", "jon", "kay",
	"kenny", "maud", "michiel", "mike", "peter", "robert", "ron",
	"tom", "wilmar"
};

char           *
xmonnam(mtmp, vb)
	struct monst   *mtmp;
	int             vb;
{
	static char     buf[BUFSZ];	/* %% */
	if (mtmp->mnamelth && !vb) {
		(void) strlcpy(buf, NAME(mtmp), sizeof(buf));
		return (buf);
	}
	switch (mtmp->data->mlet) {
	case ' ':
		{
			const char           *gn = (char *) mtmp->mextra;
			if (!*gn) {	/* might also look in scorefile */
				gn = ghostnames[rn2(SIZE(ghostnames))];
				if (!rn2(2))
					(void)
						strcpy((char *) mtmp->mextra, !rn2(5) ? plname : gn);
			}
			(void) snprintf(buf, sizeof(buf), "%s's ghost", gn);
		}
		break;
	case '@':
		if (mtmp->isshk) {
			(void) strlcpy(buf, shkname(mtmp), sizeof(buf));
			break;
		}
		/* fall into next case */
	default:
		(void) snprintf(buf, sizeof(buf), "the %s%s",
			       mtmp->minvis ? "invisible " : "",
			       mtmp->data->mname);
	}
	if (vb && mtmp->mnamelth) {
		(void) strlcat(buf, " called ", sizeof(buf));
		(void) strlcat(buf, NAME(mtmp), sizeof(buf));
	}
	return (buf);
}

char           *
lmonnam(mtmp)
	struct monst   *mtmp;
{
	return (xmonnam(mtmp, 1));
}

char           *
monnam(mtmp)
	struct monst   *mtmp;
{
	return (xmonnam(mtmp, 0));
}

char           *
Monnam(mtmp)
	struct monst   *mtmp;
{
	char           *bp = monnam(mtmp);
	if ('a' <= *bp && *bp <= 'z')
		*bp += ('A' - 'a');
	return (bp);
}

char           *
amonnam(mtmp, adj)
	struct monst   *mtmp;
	const char           *adj;
{
	char           *bp = monnam(mtmp);
	static char     buf[BUFSZ];	/* %% */

	if (!strncmp(bp, "the ", 4))
		bp += 4;
	(void) snprintf(buf, sizeof(buf), "the %s %s", adj, bp);
	return (buf);
}

char           *
Amonnam(mtmp, adj)
	struct monst   *mtmp;
	const char           *adj;
{
	char           *bp = amonnam(mtmp, adj);

	*bp = 'T';
	return (bp);
}

char           *
Xmonnam(mtmp)
	struct monst   *mtmp;
{
	char           *bp = Monnam(mtmp);
	if (!strncmp(bp, "The ", 4)) {
		bp += 2;
		*bp = 'A';
	}
	return (bp);
}

char           *
visctrl(c)
	char            c;
{
	static char     ccc[3];
	if (c < 040) {
		ccc[0] = '^';
		ccc[1] = c + 0100;
		ccc[2] = 0;
	} else {
		ccc[0] = c;
		ccc[1] = 0;
	}
	return (ccc);
}
