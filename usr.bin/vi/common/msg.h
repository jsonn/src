/*-
 * Copyright (c) 1993, 1994
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
 *
 *	@(#)msg.h	8.13 (Berkeley) 8/8/94
 */

/*
 * Message types.
 *
 * !!!
 * In historical vi, O_VERBOSE didn't exist, and O_TERSE made the error
 * messages shorter.  In this implementation, O_TERSE has no effect and
 * O_VERBOSE results in informational displays about common errors for
 * naive users.
 *
 * M_BERR	Error: M_ERR if O_VERBOSE, else bell.
 * M_ERR	Error: Display in inverse video.
 * M_INFO	 Info: Display in normal video.
 * M_SYSERR	Error: M_ERR, using strerror(3) message.
 * M_VINFO	 Info: M_INFO if O_VERBOSE, else ignore.
 */
enum msgtype { M_BERR, M_ERR, M_INFO, M_SYSERR, M_VINFO };

typedef struct _msgh MSGH;	/* MESG list head structure. */
LIST_HEAD(_msgh, _msg);

struct _msg {
	LIST_ENTRY(_msg) q;	/* Linked list of messages. */
	char	*mbuf;		/* Message buffer. */
	size_t	 blen;		/* Message buffer length. */
	size_t	 len;		/* Message length. */

#define	M_EMPTY		0x01	/* No message. */
#define	M_INV_VIDEO	0x02	/* Inverse video. */
	u_int8_t flags;
};

/*
 * Define MSG_CATALOG for the Makefile compile command
 * line to enable message catalogs.
 */
#ifdef MSG_CATALOG
#define	M(number, fmt)	number
char	*get_msg __P((SCR *, char *));
#else
#define	M(number, fmt)	fmt
#endif

/* Messages. */
void	msg_app __P((GS *, SCR *, int, char *, size_t));
int	msg_rpt __P((SCR *, int));
int	msg_status __P((SCR *, EXF *, recno_t, int));
void	msgq __P((SCR *, enum msgtype, const char *, ...));
