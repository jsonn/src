/*	$NetBSD: manconf.h,v 1.1.2.2 2002/11/03 13:47:55 he Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.
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
 *
 *	@(#)config.h	8.4 (Berkeley) 12/18/93
 */

typedef struct _tag {
	TAILQ_ENTRY(_tag) q;		/* Queue of tags. */

	TAILQ_HEAD(tqh, _entry) list;	/* Queue of entries. */
	char *s;			/* Associated string. */
	size_t len;			/* Length of 's'. */
} TAG;
typedef struct _entry {
	TAILQ_ENTRY(_entry) q;		/* Queue of entries. */

	char *s;			/* Associated string. */
	size_t len;			/* Length of 's'. */
} ENTRY;

TAILQ_HEAD(_head, _tag);
extern struct _head head;

void	 addentry __P((TAG *, const char *, int));
void	 config __P((const char *));
#ifdef MANDEBUG
void	 debug __P((const char *));
#endif
TAG	*getlist __P((const char *, int));
void	removelist __P((const char *));
TAG	*renamelist __P((const char *, const char *));
