/*	$NetBSD: init.c,v 1.17.16.1 2008/05/18 12:36:10 yamt Exp $	*/

/*-
 * Copyright (c) 2000-2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ben Harris and Jaromir Dolecek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
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

#include "sort.h"

#ifndef lint
__RCSID("$NetBSD: init.c,v 1.17.16.1 2008/05/18 12:36:10 yamt Exp $");
__SCCSID("@(#)init.c	8.1 (Berkeley) 6/6/93");
#endif /* not lint */

#include <ctype.h>
#include <string.h>

static void insertcol __P((struct field *));
static const char *setcolumn __P((const char *, struct field *, int));

u_char gweights[NBINS];

/*
 * masks of ignored characters.  Alltable is 256 ones.
 */
static u_char alltable[NBINS], dtable[NBINS], itable[NBINS];

/*
 * parsed key options
 */
struct coldesc *clist = NULL;
int ncols = 0;

/*
 * clist (list of columns which correspond to one or more icol or tcol)
 * is in increasing order of columns.
 * Fields are kept in increasing order of fields.
 */

/* 
 * keep clist in order--inserts a column in a sorted array
 */
static void
insertcol(field)
	struct field *field;
{
	int i;
	struct coldesc *p;

	/* Make space for new item */
	p = realloc(clist, (ncols + 2) * sizeof(*clist));
	if (!p)
		err(1, "realloc");
	clist = p;
	memset(&clist[ncols], 0, sizeof(clist[ncols]));

	for (i = 0; i < ncols; i++)
		if (field->icol.num <= clist[i].num)
			break;
	if (field->icol.num != clist[i].num) {
		memmove(clist+i+1, clist+i, sizeof(COLDESC)*(ncols-i));
		clist[i].num = field->icol.num;
		ncols++;
	}
	if (field->tcol.num && field->tcol.num != field->icol.num) {
		for (i = 0; i < ncols; i++)
			if (field->tcol.num <= clist[i].num)
				break;
		if (field->tcol.num != clist[i].num) {
			memmove(clist+i+1, clist+i,sizeof(COLDESC)*(ncols-i));
			clist[i].num = field->tcol.num;
			ncols++;
		}
	}
}

/*
 * matches fields with the appropriate columns--n^2 but who cares?
 */
void
fldreset(fldtab)
	struct field *fldtab;
{
	int i;

	fldtab[0].tcol.p = clist + ncols - 1;
	for (++fldtab; fldtab->icol.num; ++fldtab) {
		for (i = 0; fldtab->icol.num != clist[i].num; i++)
			;
		fldtab->icol.p = clist + i;
		if (!fldtab->tcol.num)
			continue;
		for (i = 0; fldtab->tcol.num != clist[i].num; i++)
			;
		fldtab->tcol.p = clist + i;
	}
}

/*
 * interprets a column in a -k field
 */
static const char *
setcolumn(pos, cur_fld, gflag)
	const char *pos;
	struct field *cur_fld;
	int gflag;
{
	struct column *col;
	char *npos;
	int tmp;
	col = cur_fld->icol.num ? (&cur_fld->tcol) : (&cur_fld->icol);
	col->num = (int) strtol(pos, &npos, 10);
	pos = npos;
	if (col->num <= 0 && !(col->num == 0 && col == &(cur_fld->tcol)))
		errx(2, "field numbers must be positive");
	if (*pos == '.') {
		if (!col->num)
			errx(2, "cannot indent end of line");
		++pos;
		col->indent = (int) strtol(pos, &npos, 10);
		pos = npos;
		if (&cur_fld->icol == col)
			col->indent--;
		if (col->indent < 0)
			errx(2, "illegal offset");
	}
	for(; (tmp = optval(*pos, cur_fld->tcol.num)); pos++)
		cur_fld->flags |= tmp;
	if (cur_fld->icol.num == 0)
		cur_fld->icol.num = 1;
	return (pos);
}

int
setfield(pos, cur_fld, gflag)
	const char *pos;
	struct field *cur_fld;
	int gflag;
{
	int tmp;

	cur_fld->weights = ascii;
	cur_fld->mask = alltable;

	pos = setcolumn(pos, cur_fld, gflag);
	if (*pos == '\0')			/* key extends to EOL. */
		cur_fld->tcol.num = 0;
	else {
		if (*pos != ',')
			errx(2, "illegal field descriptor");
		setcolumn((++pos), cur_fld, gflag);
	}
	if (!cur_fld->flags)
		cur_fld->flags = gflag;
	tmp = cur_fld->flags;

	/*
	 * Assign appropriate mask table and weight table.
	 * If the global weights are reversed, the local field
	 * must be "re-reversed".
	 */
	if (((tmp & R) ^ (gflag & R)) && (tmp & F))
		cur_fld->weights = RFtable;
	else if (tmp & F)
		cur_fld->weights = Ftable;
	else if ((tmp & R) ^ (gflag & R))
		cur_fld->weights = Rascii;

	if (tmp & I)
		cur_fld->mask = itable;
	else if (tmp & D)
		cur_fld->mask = dtable;

	cur_fld->flags |= (gflag & (BI | BT));
	if (!cur_fld->tcol.indent)	/* BT has no meaning at end of field */
		cur_fld->flags &= ~BT;

	if (cur_fld->tcol.num && !(!(cur_fld->flags & BI)
	    && cur_fld->flags & BT) && (cur_fld->tcol.num <= cur_fld->icol.num
	    && cur_fld->tcol.indent != 0 /* == 0 -> end of field, i.e. okay */
	    && cur_fld->tcol.indent < cur_fld->icol.indent))
		errx(2, "fields out of order");
	insertcol(cur_fld);
	return (cur_fld->tcol.num);
}

int
optval(desc, tcolflag)
	int desc, tcolflag;
{
	switch(desc) {
		case 'b':
			if (!tcolflag)
				return (BI);
			else
				return (BT);
		case 'd': return (D);
		case 'f': return (F);
		case 'i': return (I);
		case 'n': return (N);
		case 'r': return (R);
		default:  return (0);
	}
}

/*
 * Replace historic +SPEC arguments with appropriate -kSPEC.
 */ 
void
fixit(argc, argv)
	int *argc;
	char **argv;
{
	int i, j, fplus=0;
	char *vpos, *tpos, spec[20];
	int col, indent;
	size_t sz;

	for (i = 1; i < *argc; i++) {
		if (argv[i][0] != '+' && !fplus)
			continue;

		if (fplus && (argv[i][0] != '-' || !isdigit((unsigned char)argv[i][1]))) {
			fplus = 0;
			if (argv[i][0] != '+') {
				/* not a -POS argument, skip */
				continue;
			}
		}

		/* parse spec */
		tpos = argv[i]+1;
		col = (int)strtol(tpos, &tpos, 10);
		if (*tpos == '.') {
			++tpos;
			indent = (int) strtol(tpos, &tpos, 10);
		} else
			indent = 0;
		/* tpos points to optional flags now */

		/*
		 * For x.y, the obsolescent variant assumed 0 == beginning
		 * of line, while the new form uses 0 == end of line.
		 * Convert accordingly.
		 */
		if (!fplus) {
			/* +POS */
			col += 1;
			indent += 1;
		} else {
			/* -POS */
			if (indent > 0)
				col += 1;
		}

		/* new style spec */
		sz = snprintf(spec, sizeof(spec), "%d.%d%s", col, indent,
		    tpos);

		if (!fplus) {
			/* Replace the +POS argument with new-style -kSPEC */
			asprintf(&vpos, "-k%s", spec);
			argv[i] = vpos;
			fplus = 1;
		} else {
			/*
			 * Append the spec to one previously generated from
			 * +POS argument, and remove the argv element.
			 */
			asprintf(&vpos, "%s,%s", argv[i-1], spec);
			free(argv[i-1]);
			argv[i-1] = vpos;
			for (j=i; j < *argc; j++)
				argv[j] = argv[j+1];
			*argc -= 1;
			i--;
		}
	}
}

/*
 * ascii, Rascii, Ftable, and RFtable map
 * REC_D -> REC_D;  {not REC_D} -> {not REC_D}.
 * gweights maps REC_D -> (0 or 255); {not REC_D} -> {not gweights[REC_D]}.
 * Note: when sorting in forward order, to encode character zero in a key,
 * use \001\001; character 1 becomes \001\002.  In this case, character 0
 * is reserved for the field delimiter.  Analagously for -r (fld_d = 255).
 * Note: this is only good for ASCII sorting.  For different LC 's,
 * all bets are off.  See also num_init in number.c
 */
void
settables(gflags)
	int gflags;
{
	u_char *wts;
	int i, incr;
	for (i=0; i < 256; i++) {
		ascii[i] = i;
		if (i > REC_D && i < 255 - REC_D+1)
			Rascii[i] = 255 - i + 1;
		else
			Rascii[i] = 255 - i;
		if (islower(i)) {
			Ftable[i] = Ftable[toupper(i)];
			RFtable[i] = RFtable[toupper(i)];
		} else if (REC_D>= 'A' && REC_D < 'Z' && i < 'a' && i > REC_D) {
			Ftable[i] = i + 1;
			RFtable[i] = Rascii[i] - 1;
		} else {
			Ftable[i] = i;
			RFtable[i] = Rascii[i];
		}
		alltable[i] = 1;

		if (i == '\n' || isprint(i))
			itable[i] = 1;
		else
			itable[i] = 0;

		if (i == '\n' || i == '\t' || i == ' ' || isalnum(i))
			dtable[i] = 1;
		else
			dtable[i] = 0;
	}

	Rascii[REC_D] = RFtable[REC_D] = REC_D;
	if (isupper(REC_D))
		Ftable[tolower(REC_D)]++;

	if ((gflags & R) && !((gflags & F) && SINGL_FLD))
		wts = Rascii;
	else if (!((gflags & F) && SINGL_FLD))
		wts = ascii;
	else if (gflags & R)
		wts = RFtable;
	else
		wts = Ftable;

	memmove(gweights, wts, sizeof(gweights));
	incr = (gflags & R) ? -1 : 1;
	for (i = 0; i < REC_D; i++)
		gweights[i] += incr;
	gweights[REC_D] = ((gflags & R) ? 255 : 0);
	if (SINGL_FLD && (gflags & F)) {
		for (i = 0; i < REC_D; i++) {
			ascii[i] += incr;
			Rascii[i] += incr;
		}
		ascii[REC_D] = Rascii[REC_D] = gweights[REC_D];
	}
}
