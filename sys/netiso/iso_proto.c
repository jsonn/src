/*	$NetBSD: iso_proto.c,v 1.13.16.2 2004/09/18 14:55:52 skrll Exp $	*/

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
 *	@(#)iso_proto.c	8.2 (Berkeley) 2/9/95
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
 * iso_proto.c : protocol switch tables in the ISO domain
 *
 * ISO protocol family includes TP, CLTP, CLNP, 8208
 * TP and CLNP are implemented here.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iso_proto.c,v 1.13.16.2 2004/09/18 14:55:52 skrll Exp $");

#include "opt_iso.h"

#ifdef	ISO
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>

#include <net/radix.h>

#include <netiso/iso.h>

#include <netiso/clnp.h>
#include <netiso/tp_param.h>
#include <netiso/tp_var.h>
#include <netiso/esis.h>
#include <netiso/idrp_var.h>
#include <netiso/iso_pcb.h>
#include <netiso/cltp_var.h>

const int isoctlerrmap[PRC_NCMDS] = {
	0,		0,		0,		0,
	0,		EMSGSIZE,	EHOSTDOWN,	EHOSTUNREACH,
	EHOSTUNREACH,	EHOSTUNREACH,	ECONNREFUSED,	ECONNREFUSED,
	EMSGSIZE,	EHOSTUNREACH,	0,		0,
	0,		0,		0,		0,
	ENOPROTOOPT
};

const struct protosw  isosw[] = {
	/*
	 *  We need a datagram entry through which net mgmt programs can get
	 *	to the iso_control procedure (iso ioctls). Thus, a minimal
	 *	SOCK_DGRAM interface is provided here.
	 *  THIS ONE MUST BE FIRST: Kludge city : socket() says if(!proto) call
	 *  pffindtype, which gets the first entry that matches the type.
	 *  sigh.
	 */
	{SOCK_DGRAM, &isodomain, ISOPROTO_CLTP, PR_ATOMIC | PR_ADDR,
		0, cltp_output, 0, 0,
		cltp_usrreq,
		cltp_init, 0, 0, 0
	},

	/*
	 *	A datagram interface for clnp cannot co-exist with TP/CLNP
	 *  because CLNP has no way to discriminate incoming TP packets from
	 *  packets coming in for any other higher layer protocol.
	 *  Old way: set it up so that pffindproto(... dgm, clnp) fails.
	 *  New way: let pffindproto work (for x.25, thank you) but create
	 *  	a clnp_usrreq() that returns error on PRU_ATTACH.
	 */
	{SOCK_DGRAM, &isodomain, ISOPROTO_CLNP, 0,
		0, clnp_output, 0, 0,
		clnp_usrreq,
		clnp_init, 0, clnp_slowtimo, clnp_drain,
	},

	/* raw clnp */
	{SOCK_RAW, &isodomain, ISOPROTO_RAW, PR_ATOMIC | PR_ADDR,
		rclnp_input, rclnp_output, 0, rclnp_ctloutput,
		clnp_usrreq,
		0, 0, 0, 0
	},

	/* ES-IS protocol */
	{SOCK_DGRAM, &isodomain, ISOPROTO_ESIS, PR_ATOMIC | PR_ADDR,
		esis_input, 0, esis_ctlinput, 0,
		esis_usrreq,
		esis_init, 0, 0, 0
	},

	/* ISOPROTO_INTRAISIS */
	{SOCK_DGRAM, &isodomain, ISOPROTO_INTRAISIS, PR_ATOMIC | PR_ADDR,
		isis_input, 0, 0, 0,
		esis_usrreq,
		0, 0, 0, 0
	},

	/* ISOPROTO_IDRP */
	{SOCK_DGRAM, &isodomain, ISOPROTO_IDRP, PR_ATOMIC | PR_ADDR,
		idrp_input, 0, 0, 0,
		idrp_usrreq,
		idrp_init, 0, 0, 0
	},

	/* ISOPROTO_TP */
	{SOCK_SEQPACKET, &isodomain, ISOPROTO_TP, PR_CONNREQUIRED | PR_WANTRCVD | PR_LISTEN | PR_ABRTACPTDIS,
		tpclnp_input, 0, tpclnp_ctlinput, tp_ctloutput,
		tp_usrreq,
		tp_init, tp_fasttimo, tp_slowtimo, tp_drain,
	},

#ifdef TPCONS
	/* ISOPROTO_TP */
	{SOCK_SEQPACKET, &isodomain, ISOPROTO_TP0, PR_CONNREQUIRED | PR_WANTRCVD | PR_LISTEN | PR_ABRTACPTDIS,
		tpcons_input, 0, 0, tp_ctloutput,
		tp_usrreq,
		cons_init, 0, 0, 0,
	},
#endif
};


struct domain   isodomain = {
	PF_ISO,			/* family */
	"iso-domain",		/* name */
	0,			/* initialize routine */
	0,			/* externalize access rights */
	0,			/* dispose of internalized rights */
	isosw,			/* protosw */
	&isosw[sizeof(isosw) / sizeof(isosw[0])],	/* NPROTOSW */
	0,			/* next */
	rn_inithead,		/* rtattach */
	48,			/* rtoffset */
	sizeof(struct sockaddr_iso)	/* maxkeylen */
};
#endif				/* ISO */
