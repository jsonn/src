/*	$NetBSD: tuba_table.h,v 1.4.4.1 1996/12/11 04:08:48 mycroft Exp $	*/

/*-
 * Copyright (c) 1992, 1993
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
 *	@(#)tuba_table.h	8.1 (Berkeley) 6/10/93
 */

struct tuba_cache {
	struct radix_node tc_nodes[2];	/* convenient lookup */
	int             tc_refcnt;
	int             tc_time;/* last looked up */
	int             tc_flags;
#define TCF_PERM	1
	int             tc_index;
	u_short         tc_sum;	/* cksum of nsap inc. length */
	u_short         tc_ssum;/* swab(tc_sum) */
	struct sockaddr_iso tc_siso;	/* for responding */
};

#define ADDCARRY(x)  (x >= 65535 ? x -= 65535 : x)
#define REDUCE(a, b) { union { u_short s[2]; long l;} l_util; long x; \
	l_util.l = (b); x = l_util.s[0] + l_util.s[1]; ADDCARRY(x); \
	if (x == 0) x = 0xffff; a = x;}
#define SWAB(a, b) { union { u_char c[2]; u_short s;} s; u_char t; \
	s.s = (b); t = s.c[0]; s.c[0] = s.c[1]; s.c[1] = t; a = s.s;}

#ifdef _KERNEL
extern int      tuba_table_size;
extern struct tuba_cache **tuba_table;
extern struct radix_node_head *tuba_tree;

struct mbuf;
struct tcpcb;
struct isopcb;
struct inpcb;
struct sockaddr_iso;
struct socket;

/* tuba_subr.c */
void tuba_init __P((void));
int tuba_output __P((struct mbuf *, struct tcpcb *));
void tuba_refcnt __P((struct isopcb *, int ));
void tuba_pcbdetach __P((void *));
int tuba_pcbconnect __P((void *, struct mbuf *));
void tuba_tcpinput __P((struct mbuf *, ...));
int tuba_pcbconnect __P((void *, struct mbuf *));
void tuba_slowtimo __P((void));
void tuba_fasttimo __P((void));

/* tuba_table.c */
void tuba_timer __P((void *));
void tuba_table_init __P((void));
int tuba_lookup __P((struct sockaddr_iso *, int ));

/* tuba_usrreq.c */
int tuba_usrreq __P((struct socket *, int, struct mbuf *, struct mbuf *,
		     struct mbuf *, struct proc *));
int tuba_ctloutput __P((int, struct socket *, int, int , struct mbuf **));
#endif
