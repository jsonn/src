/*	$NetBSD: esis.h,v 1.22.22.2 2007/07/19 20:49:00 dyoung Exp $	*/

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
 *	@(#)esis.h	8.1 (Berkeley) 6/10/93
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

#ifndef _NETISO_ESIS_H_
#define _NETISO_ESIS_H_

#include <machine/endian.h>

#define	SNPAC_AGE		60	/* seconds */
#define	ESIS_CONFIG		60	/* seconds */
#define	ESIS_HT			(ESIS_CONFIG * 2)

/*
 *	Fixed part of an ESIS header
 */
struct esis_fixed {
	u_char          esis_proto_id;	/* network layer protocol identifier */
	u_char          esis_hdr_len;	/* length indicator (octets) */
	u_char          esis_vers;	/* version/protocol identifier
					 * extension */
	u_char          esis_res1;	/* reserved */
	u_char          esis_type;	/* type code */
	/* technically, type should be &='d 0x1f */
#define ESIS_ESH	0x02	/* End System Hello */
#define ESIS_ISH	0x04	/* Intermediate System Hello */
#define ESIS_RD		0x06	/* Redirect */
	u_char          esis_ht_msb;	/* holding time (seconds) high byte */
	u_char          esis_ht_lsb;	/* holding time (seconds) low byte */
	u_char          esis_cksum_msb;	/* checksum high byte */
	u_char          esis_cksum_lsb;	/* checksum low byte */
} __attribute__((packed));
/*
 * Values for ESIS datagram options
 */
#define ESISOVAL_NETMASK	0xe1	/* address mask option, RD PDU only */
#define ESISOVAL_SNPAMASK	0xe2	/* snpa mask option, RD PDU only */
#define ESISOVAL_ESCT		0xc6	/* end system conf. timer, ISH PDU
					 * only */


#define	ESIS_CKSUM_OFF		0x07
#define ESIS_CKSUM_REQUIRED(pdu)\
	((pdu->esis_cksum_msb != 0) || (pdu->esis_cksum_lsb != 0))

#define	ESIS_VERSION	1

struct esis_stat {
	u_short         es_nomem;	/* insufficient memory to send hello */
	u_short         es_badcsum;	/* incorrect checksum */
	u_short         es_badvers;	/* incorrect version number */
	u_short         es_badtype;	/* unknown pdu type field */
	u_short         es_toosmall;	/* packet too small */
	u_short         es_eshsent;	/* ESH sent */
	u_short         es_eshrcvd;	/* ESH rcvd */
	u_short         es_ishsent;	/* ISH sent */
	u_short         es_ishrcvd;	/* ISH rcvd */
	u_short         es_rdsent;	/* RD sent */
	u_short         es_rdrcvd;	/* RD rcvd */
};

#ifdef	_KERNEL
extern struct esis_stat esis_stat;
extern short esis_holding_time;
extern short esis_config_time;
extern short esis_esconfig_time;
struct socket;
struct mbuf;
struct snpa_hdr;
struct clnp_optidx;
struct iso_addr;
struct rtentry;
struct sockaddr_dl;

void esis_init (void);
int esis_usrreq (struct socket *, int, struct mbuf *, struct mbuf *,
		     struct mbuf *, struct lwp *);
void esis_input (struct mbuf *, ...);
void esis_rdoutput (struct snpa_hdr *, struct mbuf *, struct clnp_optidx *,
			struct iso_addr *, struct rtentry *);
int esis_insert_addr (void **, int *, const struct iso_addr *, struct mbuf *,
			  int);
void esis_eshinput (struct mbuf *, struct snpa_hdr *);
void esis_ishinput (struct mbuf *, struct snpa_hdr *);
void esis_rdinput (struct mbuf *, struct snpa_hdr *);
void esis_config (void *);
void esis_shoutput (struct ifnet *, int, int, const void *, int,
	               struct iso_addr *);
void isis_input (struct mbuf *, ...);
int isis_output (struct mbuf *, ...);
void *esis_ctlinput(int, const struct sockaddr *, void *);
#endif /* _KERNEL */

#endif /* !_NETISO_ESIS_H_ */
