/*	$NetBSD: extern.h,v 1.4.20.1 2009/05/13 19:18:43 jym Exp $	*/

/*
 * Copyright (c) 1997 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* announce.c */
int announce(CTL_MSG *, const char *);
int print_mesg(const char *, CTL_MSG *, const char *);

/* print.c */
void print_request(const char *, CTL_MSG *);
void print_response(const char *, CTL_RESPONSE *);

/* process.c */
void process_request(CTL_MSG *, CTL_RESPONSE *);
void do_announce(CTL_MSG *, CTL_RESPONSE *);
int find_user(const char *, char *, size_t);

/* table.c */
CTL_MSG *find_match(CTL_MSG *);
CTL_MSG *find_request(CTL_MSG *);
void insert_table(CTL_MSG *, CTL_RESPONSE *);
uint32_t new_id(void);
u_char delete_invite(uint32_t);

/* talkd.c */
extern int debug;
extern int logging;
void tsa2sa(struct sockaddr *, const struct talkd_sockaddr *);
