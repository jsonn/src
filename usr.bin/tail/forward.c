/*	$NetBSD: forward.c,v 1.15.2.1 1999/08/20 05:36:20 cgd Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Sze-Tyan Wang.
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
static char sccsid[] = "@(#)forward.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: forward.c,v 1.15.2.1 1999/08/20 05:36:20 cgd Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>

#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extern.h"

static int rlines __P((FILE *, long, struct stat *));

/*
 * forward -- display the file, from an offset, forward.
 *
 * There are eight separate cases for this -- regular and non-regular
 * files, by bytes or lines and from the beginning or end of the file.
 *
 * FBYTES	byte offset from the beginning of the file
 *	REG	seek
 *	NOREG	read, counting bytes
 *
 * FLINES	line offset from the beginning of the file
 *	REG	read, counting lines
 *	NOREG	read, counting lines
 *
 * RBYTES	byte offset from the end of the file
 *	REG	seek
 *	NOREG	cyclically read characters into a wrap-around buffer
 *
 * RLINES
 *	REG	mmap the file and step back until reach the correct offset.
 *	NOREG	cyclically read lines into a wrap-around array of buffers
 */
void
forward(fp, style, off, sbp)
	FILE *fp;
	enum STYLE style;
	long off;
	struct stat *sbp;
{
	int ch;
	struct timeval second;
	int dostat = 0;
	struct stat statbuf;
	off_t lastsize = 0;
	dev_t lastdev;
	ino_t lastino;

	/* Keep track of file's previous incarnation. */
	lastdev = sbp->st_dev;
	lastino = sbp->st_ino;

	switch(style) {
	case FBYTES:
		if (off == 0)
			break;
		if (S_ISREG(sbp->st_mode)) {
			if (sbp->st_size < off)
				off = sbp->st_size;
			if (fseek(fp, off, SEEK_SET) == -1) {
				ierr();
				return;
			}
		} else while (off--)
			if ((ch = getc(fp)) == EOF) {
				if (ferror(fp)) {
					ierr();
					return;
				}
				break;
			}
		break;
	case FLINES:
		if (off == 0)
			break;
		for (;;) {
			if ((ch = getc(fp)) == EOF) {
				if (ferror(fp)) {
					ierr();
					return;
				}
				break;
			}
			if (ch == '\n' && !--off)
				break;
		}
		break;
	case RBYTES:
		if (S_ISREG(sbp->st_mode)) {
			if (sbp->st_size >= off &&
			    fseek(fp, -off, SEEK_END) == -1) {
				ierr();
				return;
			}
		} else if (off == 0) {
			while (getc(fp) != EOF);
			if (ferror(fp)) {
				ierr();
				return;
			}
		} else {
			if (bytes(fp, off))
				return;
		}
		break;
	case RLINES:
		if (S_ISREG(sbp->st_mode)) {
			if (!off) {
				if (fseek(fp, 0L, SEEK_END) == -1) {
					ierr();
					return;
				}
			} else {
				if (rlines(fp, off, sbp))
					return;
			}
		} else if (off == 0) {
			while (getc(fp) != EOF);
			if (ferror(fp)) {
				ierr();
				return;
			}
		} else {
			if (lines(fp, off))
				return;
		}
		break;
	default:
		break;
	}

	for (;;) {
		while ((ch = getc(fp)) != EOF)  {
			if (putchar(ch) == EOF)
				oerr();
		}
		if (ferror(fp)) {
			ierr();
			return;
		}
		(void)fflush(stdout);
		if (!fflag)
			break;
		/*
		 * We pause for one second after displaying any data that has
		 * accumulated since we read the file.  Since sleep(3) takes
		 * eight system calls, use select() instead.
		 */
		second.tv_sec = 1;
		second.tv_usec = 0;
		if (select(0, NULL, NULL, NULL, &second) == -1)
			err(1, "select: %s", strerror(errno));
		clearerr(fp);

		if (fflag == 1)
			continue;
		/*
		 * We restat the original filename every five seconds. If
		 * the size is ever smaller than the last time we read it,
		 * the file has probably been truncated; if the inode or
		 * or device number are different, it has been rotated.
		 * This causes us to close it, reopen it, and continue
		 * the tail -f. If stat returns an error (say, because
		 * the file has been removed), just continue with what
		 * we've got open now.
		 */
		if (dostat > 0)  {
			dostat -= 1;
		} else {
			dostat = 5;
			if (stat(fname, &statbuf) == 0)  {
				if (statbuf.st_dev != lastdev ||
				    statbuf.st_ino != lastino ||
				    statbuf.st_size < lastsize)  {
					lastdev = statbuf.st_dev;
					lastino = statbuf.st_ino;
					lastsize = 0;
					fclose(fp);
					if ((fp = fopen(fname, "r")) == NULL)
						err(1, "can't reopen %s: %s",
						    fname, strerror(errno));
				} else {
					lastsize = statbuf.st_size;
				}
			}
		}
	}
}

/*
 * rlines -- display the last offset lines of the file.
 *
 * Non-zero return means than a (non-fatal) error occurred.
 */
static int
rlines(fp, off, sbp)
	FILE *fp;
	long off;
	struct stat *sbp;
{
	off_t size;
	char *p;
	char *start;

	if (!(size = sbp->st_size))
		return (0);

	if (size > SIZE_T_MAX) {
		err(0, "%s: %s", fname, strerror(EFBIG));
		return (1);
	}

	if ((start = mmap(NULL, (size_t)size, PROT_READ,
	    MAP_FILE|MAP_SHARED, fileno(fp), (off_t)0)) == (caddr_t)-1) {
		err(0, "%s: %s", fname, strerror(EFBIG));
		return (1);
	}

	/* Last char is special, ignore whether newline or not. */
	for (p = start + size - 1; --size;)
		if (*--p == '\n' && !--off) {
			++p;
			break;
		}

	/* Set the file pointer to reflect the length displayed. */
	size = sbp->st_size - size;
	WR(p, size);
	if (fseek(fp, (long)sbp->st_size, SEEK_SET) == -1) {
		ierr();
		return (1);
	}
	if (munmap(start, (size_t)sbp->st_size)) {
		err(0, "%s: %s", fname, strerror(errno));
		return (1);
	}
	return (0);
}
