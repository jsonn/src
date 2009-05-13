/*	$NetBSD: misc.c,v 1.16.12.1 2009/05/13 19:18:05 jym Exp $	*/

/*
 * Copyright (c) 1983, 1993
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

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)misc.c	8.2 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: misc.c,v 1.16.12.1 2009/05/13 19:18:05 jym Exp $");
#endif
#endif /* not lint */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "extern.h"
#include "pathnames.h"

#define distance(x,y) \
	(abs(x) >= abs(y) ? abs(x) + abs(y)/2 : abs(y) + abs(x)/2)

static int angle(int, int);

/* XXX */
int
range(struct ship *from, struct ship *to)
{
	int bow1r, bow1c, bow2r, bow2c;
	int stern1r, stern1c, stern2c, stern2r;
	int bb, bs, sb, ss, result;

	if (!to->file->dir)
		return -1;
	stern1r = bow1r = from->file->row;
	stern1c = bow1c = from->file->col;
	stern2r = bow2r = to->file->row;
	stern2c = bow2c = to->file->col;
	result = bb = distance(bow2r - bow1r, bow2c - bow1c);
	if (bb < 5) {
		stern2r += dr[to->file->dir];
		stern2c += dc[to->file->dir];
		stern1r += dr[from->file->dir];
		stern1c += dc[from->file->dir];
		bs = distance((bow2r - stern1r), (bow2c - stern1c));
		sb = distance((bow1r - stern2r), (bow1c - stern2c));
		ss = distance((stern2r - stern1r) ,(stern2c - stern1c));
		result = min(bb, min(bs, min(sb, ss)));
	}
	return result;
}

struct ship *
closestenemy(struct ship *from, int side, int anyship)
{
	struct ship *sp;
	char a;
	int olddist = 30000, dist;
	struct ship *closest = 0;

	a = capship(from)->nationality;
	foreachship(sp) {
		if (sp == from)
			continue;
		if (sp->file->dir == 0)
			continue;
		if (a == capship(sp)->nationality && !anyship)
			continue;
		if (side && gunsbear(from, sp) != side)
			continue;
		dist = range(from, sp);
		if (dist < olddist) {
			closest = sp;
			olddist = dist;
		}
	}
	return closest;
}

static int
angle(int Dr, int Dc)
{
	int i;

	if (Dc >= 0 && Dr > 0)
		i = 0;
	else if (Dr <= 0 && Dc > 0)
		i = 2;
	else if (Dc <= 0 && Dr < 0)
		i = 4;
	else
		i = 6;
	Dr = abs(Dr);
	Dc = abs(Dc);
	if ((i == 0 || i == 4) && Dc * 2.4 > Dr) {
		i++;
		if (Dc > Dr * 2.4)
			i++;
	} else if ((i == 2 || i == 6) && Dr * 2.4 > Dc) {
		i++;
		if (Dr > Dc * 2.4)
			i++;
	}
	return i % 8 + 1;
}

/* checks for target bow or stern */
int
gunsbear(struct ship *from, struct ship *to)
{
	int Dr, Dc, i;
	int ang;

	Dr = from->file->row - to->file->row;
	Dc = to->file->col - from->file->col;
	for (i = 2; i; i--) {
		if ((ang = angle(Dr, Dc) - from->file->dir + 1) < 1)
			ang += 8;
		if (ang >= 2 && ang <= 4)
			return 'r';
		if (ang >= 6 && ang <= 7)
			return 'l';
		Dr += dr[to->file->dir];
		Dc += dc[to->file->dir];
	}
	return 0;
}

/* returns true if fromship is shooting at onship's starboard side */
int
portside(struct ship *from, struct ship *on, int quick)
{
	int ang;
	int Dr, Dc;

	Dr = from->file->row - on->file->row;
	Dc = on->file->col - from->file->col;
	if (quick == -1) {
		Dr += dr[on->file->dir];
		Dc += dc[on->file->dir];
	}
	ang = angle(Dr, Dc);
	if (quick != 0)
		return ang;
	ang = (ang + 4 - on->file->dir - 1) % 8 + 1;
	return ang < 5;
}

int
colours(struct ship *sp)
{
	char flag = '\0';

	if (sp->file->struck)
		flag = '!';
	if (sp->file->explode)
		flag = '#';
	if (sp->file->sink)
		flag = '~';
	if (sp->file->struck)
		return flag;
	flag = *countryname[capship(sp)->nationality];
	return sp->file->FS ? flag : tolower((unsigned char)flag);
}

void
logger(struct ship *s)
{
	FILE *fp;
	int persons;
	int n;
	struct logs log[NLOG];
	float net;
	struct logs *lp;

	setegid(egid);
	if ((fp = fopen(_PATH_LOGFILE, "r+")) == NULL) {
		setegid(gid);
		return;
	}
	setegid(gid);
#ifdef LOCK_EX
	if (flock(fileno(fp), LOCK_EX) < 0)
		return;
#endif
	net = (float)s->file->points / s->specs->pts;
	persons = getw(fp);
	n = fread(log, sizeof(struct logs), NLOG, fp);
	for (lp = &log[n]; lp < &log[NLOG]; lp++)
		lp->l_name[0] = lp->l_uid = lp->l_shipnum
			= lp->l_gamenum = lp->l_netpoints = 0;
	rewind(fp);
	if (persons < 0)
		putw(1, fp);
	else
		putw(persons + 1, fp);
	for (lp = log; lp < &log[NLOG]; lp++)
		if (net > (float)lp->l_netpoints
		    / scene[lp->l_gamenum].ship[lp->l_shipnum].specs->pts) {
			fwrite(log, sizeof (struct logs), lp - log, fp);
			strcpy(log[NLOG-1].l_name, s->file->captain);
			log[NLOG-1].l_uid = getuid();
			log[NLOG-1].l_shipnum = s->file->index;
			log[NLOG-1].l_gamenum = game;
			log[NLOG-1].l_netpoints = s->file->points;
			fwrite(&log[NLOG-1], sizeof (struct logs), 1, fp);
			fwrite(lp, sizeof (struct logs), &log[NLOG-1] - lp, fp);
			break;
		}
#ifdef LOCK_EX
	flock(fileno(fp), LOCK_UN);
#endif
	fclose(fp);
}
