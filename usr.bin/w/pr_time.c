/*	$NetBSD: pr_time.c,v 1.2.2.1 1996/06/07 01:45:11 thorpej Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
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
#if 0
static char sccsid[] = "@(#)pr_time.c	8.2 (Berkeley) 4/4/94";
#else
static char rcsid[] = "$NetBSD: pr_time.c,v 1.2.2.1 1996/06/07 01:45:11 thorpej Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>
#include <tzfile.h>

#include "extern.h"

/*
 * pr_attime --
 *	Print the time since the user logged in. 
 *
 *	Note: SCCS forces the bizarre string manipulation, things like
 *	8.2 get replaced in the source code.
 */
void
pr_attime(started, now)
	time_t *started, *now;
{
	static char buf[256];
	struct tm *tp;
	time_t diff;
	char fmt[20];

	tp = localtime(started);
	diff = *now - *started;

	/* If more than a week, use day-month-year. */
	if (diff > SECSPERDAY * DAYSPERWEEK)
		(void)strcpy(fmt, "%d%b%y");

	/* If not today, use day-hour-am/pm. */
	else if (*now / SECSPERDAY != *started / SECSPERDAY) {
		(void)strcpy(fmt, __CONCAT("%a%", "I%p"));
	}

	/* Default is hh:mm{am,pm}. */
	else {
		(void)strcpy(fmt, __CONCAT("%l:%", "M%p"));
	}

	(void)strftime(buf, sizeof(buf), fmt, tp);
	(void)printf("%s", buf);
}

/*
 * pr_idle --
 *	Display the idle time.
 */
void
pr_idle(idle)
	time_t idle;
{
	int days = idle / SECSPERDAY;

	/* If idle more than 36 hours, print as a number of days. */
	if (idle >= 36 * SECSPERHOUR)
		printf(days == 1 ? "  %dday " : " %ddays ", days);

	/* If idle more than an hour, print as HH:MM. */
	else if (idle >= SECSPERHOUR)
		(void)printf(" %2d:%02d ",
		    idle / SECSPERHOUR, (idle % SECSPERHOUR) / SECSPERMIN);

	/* Else print the minutes idle. */
	else
		(void)printf("    %2d ", idle / SECSPERMIN);
}
