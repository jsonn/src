/*	$NetBSD: if_fddi.h,v 1.2.8.1 1997/02/20 16:41:06 is Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)if_fddi.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_IF_FDDI_H_
#define _NETINET_IF_FDDI_H_

/*
 * Structure of an 100Mb/s FDDI header.
 */
struct	fddi_header {
	u_char	fddi_fc;
	u_char	fddi_dhost[6];
	u_char	fddi_shost[6];
};

#define	FDDIMTU			4470
#define	FDDIMIN			3

#define	FDDIFC_C		0x80	/* 0b10000000 */
#define	FDDIFC_L		0x40	/* 0b01000000 */
#define	FDDIFC_F		0x30	/* 0b00110000 */
#define	FDDIFC_Z		0x0F	/* 0b00001111 */

#define	FDDIFC_LLC_ASYNC	0x50
#define	FDDIFC_LLC_PRIO0	0
#define	FDDIFC_LLC_PRIO1	1
#define	FDDIFC_LLC_PRIO2	2
#define	FDDIFC_LLC_PRIO3	3
#define	FDDIFC_LLC_PRIO4	4
#define	FDDIFC_LLC_PRIO5	5
#define	FDDIFC_LLC_PRIO6	6
#define	FDDIFC_LLC_PRIO7	7
#define FDDIFC_LLC_SYNC         0xd0
#define	FDDIFC_SMT		0x40

#if defined(_KERNEL)
#define	fddibroadcastaddr	etherbroadcastaddr
#define	fddi_ipmulticast_min	ether_ipmulticast_min
#define	fddi_ipmulticast_max	ether_ipmulticast_max
#define	fddi_addmulti		ether_addmulti
#define	fddi_delmulti		ether_delmulti
#define	fddi_sprintf		ether_sprintf

void    fddi_ifattach __P((struct ifnet *, caddr_t));
void    fddi_input __P((struct ifnet *, struct fddi_header *, struct mbuf *));
int     fddi_output __P((struct ifnet *,
           struct mbuf *, struct sockaddr *, struct rtentry *)); 

#endif

#endif
