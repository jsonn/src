/*	$NetBSD: regular.c,v 1.8.4.1 2000/07/20 19:06:09 jdolecek Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
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

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)regular.c	8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: regular.c,v 1.8.4.1 2000/07/20 19:06:09 jdolecek Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "extern.h"

void
c_regular(fd1, file1, skip1, len1, fd2, file2, skip2, len2)
	int fd1, fd2;
	char *file1, *file2;
	off_t skip1, len1, skip2, len2;
{
	u_char ch, *p1, *p2;
	off_t byte, length, line;
	int dfound;

	if (sflag && len1 != len2)
		exit(1);

	if (skip1 > len1)
		eofmsg(file1);
	len1 -= skip1;
	if (skip2 > len2)
		eofmsg(file2);
	len2 -= skip2;

	length = MIN(len1, len2);
	if (length > SIZE_T_MAX) {
mmap_failed:
		c_special(fd1, file1, skip1, fd2, file2, skip2);
		return;
	}

	if ((p1 = (u_char *)mmap(NULL, (size_t)length,
	    PROT_READ, MAP_FILE, fd1, skip1)) == MAP_FAILED)
		goto mmap_failed;
	(void)madvise(p1, (size_t)length, MADV_SEQUENTIAL);

	if ((p2 = (u_char *)mmap(NULL, (size_t)length,
	    PROT_READ, MAP_FILE, fd2, skip2)) == MAP_FAILED) {
		munmap(p1, (size_t) length);
		goto mmap_failed;
	}
	(void)madvise(p2, (size_t)length, MADV_SEQUENTIAL);

	dfound = 0;
	for (byte = line = 1; length--; ++p1, ++p2, ++byte) {
		if ((ch = *p1) != *p2) {
			if (lflag) {
				dfound = 1;
				(void)printf("%6qd %3o %3o\n", (long long)byte,
				    ch, *p2);
			} else
				diffmsg(file1, file2, byte, line);
				/* NOTREACHED */
		}
		if (ch == '\n')
			++line;
	}

	if (len1 != len2)
		eofmsg (len1 > len2 ? file2 : file1);
	if (dfound)
		exit(DIFF_EXIT);
}
