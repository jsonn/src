/*	$NetBSD: clnp_stat.h,v 1.8.16.1 2006/06/21 15:11:37 yamt Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)clnp_stat.h	8.1 (Berkeley) 6/10/93
 */

/***********************************************************
		Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */

#ifndef _NETISO_CLNP_STAT_H_
#define _NETISO_CLNP_STAT_H_

struct clnp_stat {
	int             cns_total;	/* total pkts received */
	int             cns_toosmall;	/* fixed part of header too small */
	int             cns_badhlen;	/* header length is not reasonable */
	int             cns_badcsum;	/* checksum on packet failed */
	int             cns_badaddr;	/* address fields were not reasonable */
	int             cns_badvers;	/* incorrect version */
	int             cns_noseg;	/* segment information forgotten */
	int             cns_noproto;	/* incorrect protocol id */
	int             cns_delivered;	/* packets consumed by protocol */
	int             cns_ttlexpired;	/* ttl has expired */
	int             cns_forward;	/* forwarded packets */
	int             cns_sent;	/* total packets sent */
	int             cns_odropped;	/* o.k. packets discarded, e.g.
					 * ENOBUFS */
	int             cns_cantforward;	/* non-forwarded packets */
	int             cns_fragmented;	/* packets fragmented */
	int             cns_fragments;	/* fragments received */
	int             cns_fragdropped;	/* fragments discarded */
	int             cns_fragtimeout;	/* fragments timed out */
	int             cns_ofragments;	/* fragments generated */
	int             cns_cantfrag;	/* fragmentation prohibited */
	int             cns_reassembled;	/* packets reconstructed */
	int             cns_cachemiss;	/* cache misses */
	int             cns_congest_set;	/* congestion experienced bit
						 * set */
	int             cns_congest_rcvd;	/* congestion experienced bit
						 * received */
	int             cns_er_inhist[CLNP_ERRORS + 1];
	int             cns_er_outhist[CLNP_ERRORS + 1];
};

#ifdef _KERNEL
extern struct clnp_stat clnp_stat;

#ifdef INCSTAT
#undef INCSTAT
#endif				/* INCSTAT */
#define INCSTAT(x) clnp_stat.x++
#endif /* _KERNEL */

#endif /* !_NETISO_CLNP_STAT_H_ */
