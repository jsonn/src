/*	$NetBSD: tp_user.h,v 1.6.64.2 2004/09/18 14:55:52 skrll Exp $	*/

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
 *	@(#)tp_user.h	8.1 (Berkeley) 6/10/93
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
/*
 * These are the values a real-live user ;-) needs.
 */

#include <sys/types.h>

#ifndef _NETISO_TP_USER_H_
#define _NETISO_TP_USER_H_

struct tp_conn_param {
	/* PER CONNECTION parameters */
	short           p_Nretrans;
	short           p_dr_ticks;

	short           p_cc_ticks;
	short           p_dt_ticks;

	short           p_x_ticks;
	short           p_cr_ticks;

	short           p_keepalive_ticks;
	short           p_sendack_ticks;

	short           p_ref_ticks;
	short           p_inact_ticks;

	short           p_ptpdusize;	/* preferred tpdusize/128 */
	short           p_winsize;

	u_char          p_tpdusize;	/* log 2 of size */

	u_char          p_ack_strat;	/* see comments in tp_pcb.h */
	u_char          p_rx_strat;	/* see comments in tp_pcb.h */
	u_char          p_class;/* class bitmask */
	u_char          p_xtd_format;
	u_char          p_xpd_service;
	u_char          p_use_checksum;
	u_char          p_use_nxpd;	/* netwk expedited data: not
					 * implemented */
	u_char          p_use_rcc;	/* receipt confirmation: not
					 * implemented */
	u_char          p_use_efc;	/* explicit flow control: not
					 * implemented */
	u_char          p_no_disc_indications;	/* don't deliver indic on
						 * disc */
	u_char          p_dont_change_params;	/* use these params as they
						 * are */
	u_char          p_netservice;
	u_char          p_version;	/* only here for checking */
};

/*
 * These sockopt level definitions should be considered for socket.h
 */
#define	SOL_TRANSPORT	0xfffe
#define	SOL_NETWORK	0xfffd

/* get/set socket opt commands */
#define		TPACK_WINDOW	0x0	/* ack only on full window */
#define		TPACK_EACH		0x1	/* ack every packet */

#define		TPRX_USE_CW		0x8	/* use congestion window
						 * transmit */
#define		TPRX_EACH		0x4	/* retrans each packet of a
						 * set */
#define		TPRX_FASTSTART	0x1	/* don't use slow start */

#define TPOPT_INTERCEPT		0x200
#define TPOPT_FLAGS			0x300
#define TPOPT_CONN_DATA		0x400
#define TPOPT_DISC_DATA		0x500
#define TPOPT_CFRM_DATA		0x600
#define TPOPT_CDDATA_CLEAR	0x700
#define TPOPT_MY_TSEL		0x800
#define TPOPT_PEER_TSEL		0x900
#define TPOPT_PERF_MEAS		0xa00
#define TPOPT_PSTATISTICS	0xb00
#define TPOPT_PARAMS		0xc00	/* to replace a bunch of the others */
#define TPOPT_DISC_REASON	0xe00

struct tp_disc_reason {
	struct cmsghdr  dr_hdr;
	u_int           dr_reason;
};

/*
 * **********************flags**********************************
 */

/* read only flags */
#define TPFLAG_NLQOS_PDN		(u_char)0x01
#define TPFLAG_PEER_ON_SAMENET	(u_char)0x02
#define TPFLAG_GENERAL_ADDR		(u_char)0x04	/* bound to wildcard
							 * addr */


/*
 * **********************end flags******************************
 */


#endif				/* _NETISO_TP_USER_H_ */
