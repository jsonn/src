/*	$NetBSD: addrtoname.h,v 1.2.4.1 1997/03/11 16:29:20 is Exp $	*/

/*
 * Copyright (c) 1990, 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) Header: addrtoname.h,v 1.11 94/06/14 20:11:41 leres Exp (LBL)
 */

/* Name to address translation routines. */

extern char *linkaddr_string(const u_char *, const int);
extern char *etheraddr_string(const u_char *);
extern char *etherproto_string(u_short);
extern char *tcpport_string(u_short);
extern char *udpport_string(u_short);
extern char *getname(const u_char *);
extern char *intoa(u_int32);

extern void init_addrtoname(int, u_int32, u_int32);

#define ipaddr_string(p) getname((const u_char *)(p))
