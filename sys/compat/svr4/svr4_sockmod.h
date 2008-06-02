/*	$NetBSD: svr4_sockmod.h,v 1.6.144.1 2008/06/02 13:23:08 mjf Exp $	 */

/*-
 * Copyright (c) 1994 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#ifndef	_SVR4_SOCKMOD_H_
#define	_SVR4_SOCKMOD_H_

#define	SVR4_SIMOD 		('I' << 8)

#define	SVR4_SI_OGETUDATA	(SVR4_SIMOD|101)
#define	SVR4_SI_SHUTDOWN	(SVR4_SIMOD|102)
#define	SVR4_SI_LISTEN		(SVR4_SIMOD|103)
#define	SVR4_SI_SETMYNAME	(SVR4_SIMOD|104)
#define	SVR4_SI_SETPEERNAME	(SVR4_SIMOD|105)
#define	SVR4_SI_GETINTRANSIT	(SVR4_SIMOD|106)
#define	SVR4_SI_TCL_LINK	(SVR4_SIMOD|107)
#define	SVR4_SI_TCL_UNLINK	(SVR4_SIMOD|108)
#define SVR4_SI_SOCKPARAMS	(SVR4_SIMOD|109)
#define SVR4_SI_GETUDATA	(SVR4_SIMOD|110)


#define SVR4_SOCK_DGRAM		1
#define SVR4_SOCK_STREAM	2
#define SVR4_SOCK_STREAM_ORD	3
#define SVR4_SOCK_RAW		4
#define SVR4_SOCK_RDM		5
#define SVR4_SOCK_SEQPACKET	6

struct svr4_si_sockparms {
	int	family;
	int	type;
	int	protocol;
};

struct svr4_si_oudata {
	int	tidusize;
	int	addrsize;
	int	optsize;
	int	etsdusize;
	int	servtype;
	int	so_state;
	int	so_options;
	int	tsdusize;
};

struct svr4_si_udata {
	int	tidusize;
	int	addrsize;
	int	optsize;
	int	etsdusize;
	int	servtype;
	int	so_state;
	int	so_options;
	int	tsdusize;
	struct svr4_si_sockparms sockparms;
};
#endif /* !_SVR4_SOCKMOD_H_ */
