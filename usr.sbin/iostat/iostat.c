/*	$NetBSD: iostat.c,v 1.25.2.1 2002/06/30 05:48:05 lukem Exp $	*/

/*
 * Copyright (c) 1996 John M. Vinopal
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by John M. Vinopal.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1986, 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1986, 1991, 1993\n\
        The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)iostat.c	8.3 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: iostat.c,v 1.25.2.1 2002/06/30 05:48:05 lukem Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/sched.h>
#include <sys/dkstat.h>
#include <sys/time.h>

#include <err.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dkstats.h"

/* Namelist and memory files. */
char	*nlistf, *memf;

int		hz, reps, interval;
static int	todo = 0;

#define ISSET(x, a)	((x) & (a))
#define SHOW_CPU	1<<0
#define SHOW_TTY	1<<1
#define SHOW_STATS_1	1<<2
#define SHOW_STATS_2	1<<3
#define SHOW_STATS_X	1<<4
#define SHOW_TOTALS	1<<7
#define SHOW_STATS_ALL	(SHOW_STATS_1 | SHOW_STATS_2 | SHOW_STATS_X)

static void cpustats(void);
static void disk_stats(double);
static void disk_stats2(double);
static void disk_statsx(double);
static void header(int);
static void usage(void);
static void display(void);
static void selectdrives(int, char **);

int main(int, char **);

int
main(int argc, char *argv[])
{
	int ch, hdrcnt;
	struct timeval	tv;

	while ((ch = getopt(argc, argv, "Cc:dDIM:N:Tw:x")) != -1)
		switch(ch) {
		case 'c':
			if ((reps = atoi(optarg)) <= 0)
				errx(1, "repetition count <= 0.");
			break;
		case 'C':
			todo |= SHOW_CPU;
			break;
		case 'd':
			todo &= ~SHOW_STATS_ALL;
			todo |= SHOW_STATS_1;
			break;
		case 'D':
			todo &= ~SHOW_STATS_ALL;
			todo |= SHOW_STATS_2;
			break;
		case 'I':
			todo |= SHOW_TOTALS;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'T':
			todo |= SHOW_TTY;
			break;
		case 'w':
			if ((interval = atoi(optarg)) <= 0)
				errx(1, "interval <= 0.");
			break;
		case 'x':
			todo &= ~SHOW_STATS_ALL;
			todo |= SHOW_STATS_X;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!ISSET(todo, SHOW_CPU | SHOW_TTY | SHOW_STATS_ALL))
		todo |= SHOW_CPU | SHOW_TTY | SHOW_STATS_1;
	if (ISSET(todo, SHOW_STATS_X)) {
		todo &= ~(SHOW_CPU | SHOW_TTY | SHOW_STATS_ALL);
		todo |= SHOW_STATS_X;
	}

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (nlistf != NULL || memf != NULL)
		setgid(getgid());

	dkinit(0);
	dkreadstats();
	selectdrives(argc, argv);

	tv.tv_sec = interval;
	tv.tv_usec = 0;

	/* print a new header on sigcont */
	(void)signal(SIGCONT, header);

	for (hdrcnt = 1;;) {
		if (!--hdrcnt) {
			header(0);
			hdrcnt = 20;
		}

		if (!ISSET(todo, SHOW_TOTALS))
			dkswap();
		display();

		if (reps >= 0 && --reps <= 0)
			break;
		select(0, NULL, NULL, NULL, &tv);
		dkreadstats();
	}
	exit(0);
}

static void
header(int signo)
{
	int i;

	if (ISSET(todo, SHOW_STATS_X))
		return;

					/* Main Headers. */
	if (ISSET(todo, SHOW_TTY))
		(void)printf("      tty");

	if (ISSET(todo, SHOW_STATS_1))
		for (i = 0; i < dk_ndrive; i++)
			if (cur.dk_select[i])
				(void)printf(
				    "        %7.7s ", cur.dk_name[i]);

	if (ISSET(todo, SHOW_STATS_2))
		for (i = 0; i < dk_ndrive; i++)
			if (cur.dk_select[i])
				(void)printf(
				    "       %7.7s ", cur.dk_name[i]);

	if (ISSET(todo, SHOW_CPU))
		(void)printf("            cpu");

	printf("\n");

					/* Sub-Headers. */
	if (ISSET(todo, SHOW_TTY))
		printf(" tin tout");

	if (ISSET(todo, SHOW_STATS_1)) {
		for (i = 0; i < dk_ndrive; i++)
			if (cur.dk_select[i]) {
				if (ISSET(todo, SHOW_TOTALS))
					(void)printf("  KB/t xfr MB   ");
				else
					(void)printf("  KB/t t/s MB/s ");
			}
	}

	if (ISSET(todo, SHOW_STATS_2))
		for (i = 0; i < dk_ndrive; i++)
			if (cur.dk_select[i])
				(void)printf("   KB xfr time ");

	if (ISSET(todo, SHOW_CPU))
		(void)printf(" us ni sy in id");
	printf("\n");
}

static void
disk_stats(double etime)
{
	int dn;
	double atime, mbps;

	for (dn = 0; dn < dk_ndrive; ++dn) {
		if (!cur.dk_select[dn])
			continue;

					/* average Kbytes per transfer. */
		if (cur.dk_xfer[dn])
			mbps = (cur.dk_bytes[dn] / (1024.0)) / cur.dk_xfer[dn];
		else
			mbps = 0.0;
		(void)printf(" %5.2f", mbps);

					/* average transfers per second. */
		(void)printf(" %3.0f", cur.dk_xfer[dn] / etime);

					/* time busy in disk activity */
		atime = (double)cur.dk_time[dn].tv_sec +
			((double)cur.dk_time[dn].tv_usec / (double)1000000);

					/* Megabytes per second. */
		if (atime != 0.0)
			mbps = cur.dk_bytes[dn] / (double)(1024 * 1024);
		else
			mbps = 0;
		(void)printf(" %4.2f ", mbps / etime);
	}
}

static void
disk_stats2(double etime)
{
	int dn;
	double atime;

	for (dn = 0; dn < dk_ndrive; ++dn) {
		if (!cur.dk_select[dn])
			continue;

					/* average kbytes per second. */
		(void)printf(" %4.0f", cur.dk_bytes[dn] / (1024.0) / etime);

					/* average transfers per second. */
		(void)printf(" %3.0f", cur.dk_xfer[dn] / etime);

					/* average time busy in disk activity */
		atime = (double)cur.dk_time[dn].tv_sec +
			((double)cur.dk_time[dn].tv_usec / (double)1000000);
		(void)printf(" %4.2f ", atime / etime);
	}
}

static void
disk_statsx(double etime)
{
	int dn;
	double atime, kbps;

	if (ISSET(todo, SHOW_TOTALS))
		(void)printf("device       KB/t      xfr     time       MB");
	else
		(void)printf("device       KB/t      t/s     time     MB/s");

	for (dn = 0; dn < dk_ndrive; ++dn) {
		(void)printf("\n");
		(void)printf("%-8.8s", cur.dk_name[dn]);

					/* average Kbytes per transfer */
		if (cur.dk_xfer[dn])
			kbps = (cur.dk_bytes[dn] / (1024.0)) / cur.dk_xfer[dn];
		else
			kbps = 0.0;
		(void)printf(" %8.2f", kbps);

					/* average transfers (per second) */
		(void)printf(" %8.0f", cur.dk_xfer[dn] / etime);

					/* time busy in disk activity */
		atime = (double)cur.dk_time[dn].tv_sec +
			((double)cur.dk_time[dn].tv_usec / (double)1000000);
		(void)printf(" %8.2f", atime / etime);

					/* average megabytes (per second) */
		(void)printf(" %8.2f",
		    cur.dk_bytes[dn] / (1024.0 * 1024) / etime);

	}
}

static void
cpustats(void)
{
	int state;
	double time;

	time = 0;
	for (state = 0; state < CPUSTATES; ++state)
		time += cur.cp_time[state];
	if (!time)
		time = 1.0;
			/* States are generally never 100% and can use %3.0f. */
	for (state = 0; state < CPUSTATES; ++state)
		printf(" %2.0f", 100. * cur.cp_time[state] / time);
}

static void
usage(void)
{

	(void)fprintf(stderr, "usage: iostat [-CdDITx] [-c count] [-M core] \
[-N system] [-w wait] [drives]\n");
	exit(1);
}

static void
display(void)
{
	double	etime;

	/* Sum up the elapsed ticks. */
	etime = cur.cp_etime;

	/* If we're showing totals only, then don't divide by the
	 * system time.
	 */
	if (ISSET(todo, SHOW_TOTALS))
		etime = 1.0;

	if (ISSET(todo, SHOW_TTY))
		printf("%4.0f %4.0f", cur.tk_nin / etime, cur.tk_nout / etime);

	if (ISSET(todo, SHOW_STATS_1))
		disk_stats(etime);

	if (ISSET(todo, SHOW_STATS_2))
		disk_stats2(etime);

	if (ISSET(todo, SHOW_STATS_X))
		disk_statsx(etime);

	if (ISSET(todo, SHOW_CPU))
		cpustats();

	(void)printf("\n");
	(void)fflush(stdout);
}

static void
selectdrives(int argc, char *argv[])
{
	int	i, ndrives;

	/*
	 * Choose drives to be displayed.  Priority goes to (in order) drives
	 * supplied as arguments and default drives.  If everything isn't
	 * filled in and there are drives not taken care of, display the first
	 * few that fit.
	 *
	 * The backward compatibility #ifdefs permit the syntax:
	 *	iostat [ drives ] [ interval [ count ] ]
	 */
	if (ISSET(todo, SHOW_STATS_X)) {
		for (i = 0; i < dk_ndrive; i++)
			cur.dk_select[i] = 1;
	}

#define	BACKWARD_COMPATIBILITY
	for (ndrives = 0; *argv; ++argv) {
#ifdef	BACKWARD_COMPATIBILITY
		if (isdigit(**argv))
			break;
#endif
		if (!ISSET(todo, SHOW_STATS_X))
			for (i = 0; i < dk_ndrive; i++) {
				if (strcmp(cur.dk_name[i], *argv))
					continue;
				cur.dk_select[i] = 1;
				++ndrives;
			}
	}
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		interval = atoi(*argv);
		if (*++argv)
			reps = atoi(*argv);
	}
#endif

	if (interval) {
		if (!reps)
			reps = -1;
	} else
		if (reps)
			interval = 1;

	/* Pick up to 4 drives if none specified. */
	if (ndrives == 0)
		for (i = 0; i < dk_ndrive && ndrives < 4; i++) {
			if (cur.dk_select[i])
				continue;
			cur.dk_select[i] = 1;
			++ndrives;
		}
}
