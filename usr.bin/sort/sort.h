/*	$NetBSD: sort.h,v 1.19.8.1 2009/05/13 19:20:05 jym Exp $	*/

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
 *
 *	@(#)sort.h	8.1 (Berkeley) 6/6/93
 */

#include <sys/param.h>

#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NBINS		256

/* values for masks, weights, and other flags. */
#define I 1		/* mask out non-printable characters */
#define D 2		/* sort alphanumeric characters only */
#define N 4		/* Field is a number */
#define F 8		/* weight lower and upper case the same */
#define R 16		/* Field is reversed with respect to the global weight */
#define BI 32		/* ignore blanks in icol */
#define BT 64		/* ignore blanks in tcol */

/* masks for delimiters: blanks, fields, and termination. */
#define BLANK 1		/* ' ', '\t'; '\n' if -T is invoked */
#define FLD_D 2		/* ' ', '\t' default; from -t otherwise */
#define REC_D_F 4	/* '\n' default; from -T otherwise */

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define	FCLOSE(file) {							\
	if (EOF == fclose(file))					\
		err(2, "%p", file);					\
}

#define	EWRITE(ptr, size, n, f) {					\
	if (!fwrite(ptr, size, n, f))					\
		 err(2, NULL);						\
}

/* length of record is currently limited to maximum string length (size_t) */
typedef size_t length_t;

/* a record is a key/line pair starting at rec.data. It has a total length
 * and an offset to the start of the line half of the pair.
 */
typedef struct recheader {
	length_t length;
	length_t offset;
	u_char data[1];
} RECHEADER;

typedef struct trecheader {
	length_t length;
	length_t offset;
} TRECHEADER;

/* This is the column as seen by struct field.  It is used by enterfield.
 * They are matched with corresponding coldescs during initialization.
 */
struct column {
	struct coldesc *p;
	int num;
	int indent;
};

/* a coldesc has a number and pointers to the beginning and end of the
 * corresponding column in the current line.  This is determined in enterkey.
 */
typedef struct coldesc {
	u_char *start;
	u_char *end;
	int num;
} COLDESC;

/* A field has an initial and final column; an omitted final column
 * implies the end of the line.  Flags regulate omission of blanks and
 * numerical sorts; mask determines which characters are ignored (from -i, -d);
 * weights determines the sort weights of a character (from -f, -r).
 */
struct field {
	struct column icol;
	struct column tcol;
	u_int flags;
	u_char *mask;
	u_char *weights;
};

struct filelist {
	const char * const * names;
};

typedef int (*get_func_t)(int, int, struct filelist *, int,
		RECHEADER *, u_char *, struct field *);
typedef void (*put_func_t)(const struct recheader *, FILE *);

extern int PANIC;	/* maximum depth of fsort before fmerge is called */
extern u_char ascii[NBINS], Rascii[NBINS], Ftable[NBINS], RFtable[NBINS];
extern u_char d_mask[NBINS];
extern int SINGL_FLD, SEP_FLAG, UNIQUE;
extern int REC_D;
extern const char *tmpdir;
extern int stable_sort;
extern u_char gweights[NBINS];
extern struct coldesc *clist;
extern int ncols;

void	 append(const u_char **, int, int, FILE *,
	    void (*)(const RECHEADER *, FILE *), struct field *);
void	 concat(FILE *, FILE *);
length_t enterkey(RECHEADER *, DBT *, int, struct field *);
void	 fixit(int *, char **);
void	 fldreset(struct field *);
FILE	*ftmp(void);
void	 fmerge(int, int, struct filelist *, int,
		get_func_t, FILE *, put_func_t, struct field *);
void	 fsort(int, int, int, struct filelist *, int, FILE *,
		struct field *);
int	 geteasy(int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *);
int	 getnext(int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *);
int	 makekey(int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *);
int	 makeline(int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *);
void	 num_init(void);
void	 onepass(const u_char **, int, long, long *, u_char *, FILE *);
int	 optval(int, int);
void	 order(struct filelist *, get_func_t, struct field *);
void	 putline(const RECHEADER *, FILE *);
void	 putrec(const RECHEADER *, FILE *);
void	 rd_append(int, int, int, FILE *, u_char *, u_char *);
int	 setfield(const char *, struct field *, int);
void	 settables(int);
