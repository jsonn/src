/*	$NetBSD: comm.c,v 1.16.6.1 2009/05/13 19:19:46 jym Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Case Larsen.
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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)comm.c	8.4 (Berkeley) 5/4/95";
#endif
__RCSID("$NetBSD: comm.c,v 1.16.6.1 2009/05/13 19:19:46 jym Exp $");
#endif /* not lint */

#include <err.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	MAXLINELEN	(LINE_MAX + 1)

const char *tabs[] = { "", "\t", "\t\t" };

FILE   *file(const char *);
void	show(FILE *, const char *, char *);
void	usage(void);

int
main(int argc, char **argv)
{
	int comp, file1done, file2done, read1, read2;
	int ch, flag1, flag2, flag3;
	FILE *fp1, *fp2;
	const char *col1, *col2, *col3, **p;
	char line1[MAXLINELEN], line2[MAXLINELEN];
	int (*compare)(const char*,const char*);

	(void)setlocale(LC_ALL, "");

	file1done = file2done = 0;
	flag1 = flag2 = flag3 = 1;
	compare = strcoll;
	while ((ch = getopt(argc, argv, "123f")) != -1)
		switch(ch) {
		case '1':
			flag1 = 0;
			break;
		case '2':
			flag2 = 0;
			break;
		case '3':
			flag3 = 0;
			break;
		case 'f':
			compare = strcasecmp;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	fp1 = file(argv[0]);
	fp2 = file(argv[1]);

	/* for each column printed, add another tab offset */
	p = tabs;
	col1 = col2 = col3 = NULL;
	if (flag1)
		col1 = *p++;
	if (flag2)
		col2 = *p++;
	if (flag3)
		col3 = *p;

	for (read1 = read2 = 1;;) {
		/* read next line, check for EOF */
		if (read1)
			file1done = !fgets(line1, MAXLINELEN, fp1);
		if (read2)
			file2done = !fgets(line2, MAXLINELEN, fp2);

		/* if one file done, display the rest of the other file */
		if (file1done) {
			if (!file2done && col2)
				show(fp2, col2, line2);
			break;
		}
		if (file2done) {
			if (!file1done && col1)
				show(fp1, col1, line1);
			break;
		}

		/* lines are the same */
		if (!(comp = compare(line1, line2))) {
			read1 = read2 = 1;
			if (col3)
				if (printf("%s%s", col3, line1) < 0)
					break;
			continue;
		}

		/* lines are different */
		if (comp < 0) {
			read1 = 1;
			read2 = 0;
			if (col1)
				if (printf("%s%s", col1, line1) < 0)
					break;
		} else {
			read1 = 0;
			read2 = 1;
			if (col2)
				if (printf("%s%s", col2, line2) < 0)
					break;
		}
	}

	if (ferror (stdout) || fclose (stdout) == EOF)
		err(1, "stdout");

	exit(0);
}

void
show(FILE *fp, const char *offset, char *buf)
{
	while (printf("%s%s", offset, buf) >= 0 && fgets(buf, MAXLINELEN, fp))
		;
}

FILE *
file(const char *name)
{
	FILE *fp;

	if (!strcmp(name, "-"))
		return (stdin);
	if ((fp = fopen(name, "r")) == NULL)
		err(1, "%s", name);
	return (fp);
}

void
usage(void)
{

	(void)fprintf(stderr, "usage: comm [-123f] file1 file2\n");
	exit(1);
}
