/*	$NetBSD: if_gif.h,v 1.3.2.1 2000/11/20 18:10:02 bouyer Exp $	*/
/*	$KAME: if_gif.h,v 1.12 2000/04/19 06:20:11 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * if_gif.h
 */

#ifndef _NET_IF_GIF_H_
#define _NET_IF_GIF_H_

#include <sys/queue.h>

#if (defined(__FreeBSD__) && __FreeBSD__ >= 3) || defined(__NetBSD__)
#if defined(_KERNEL) && !defined(_LKM)
#include "opt_inet.h"
#endif
#endif

#include <netinet/in.h>
/* xxx sigh, why route have struct route instead of pointer? */

struct encaptab;

struct gif_softc {
	struct ifnet	gif_if;	   /* common area - must be at the top */
	struct sockaddr	*gif_psrc; /* Physical src addr */
	struct sockaddr	*gif_pdst; /* Physical dst addr */
	union {
		struct route  gifscr_ro;    /* xxx */
#ifdef INET6
		struct route_in6 gifscr_ro6; /* xxx */
#endif
	} gifsc_gifscr;
	int		gif_flags;
	const struct encaptab *encap_cookie4;
	const struct encaptab *encap_cookie6;
	LIST_ENTRY(gif_softc) gif_list;	/* list of all gifs */
};

#define gif_ro gifsc_gifscr.gifscr_ro
#ifdef INET6
#define gif_ro6 gifsc_gifscr.gifscr_ro6
#endif

#define GIF_MTU		(1280)	/* Default MTU */
#define	GIF_MTU_MIN	(1280)	/* Minimum MTU */
#define	GIF_MTU_MAX	(8192)	/* Maximum MTU */

/* Prototypes */
void gif_input __P((struct mbuf *, int, struct ifnet *));
int gif_output __P((struct ifnet *, struct mbuf *,
		    struct sockaddr *, struct rtentry *));
#if defined(__FreeBSD__) && __FreeBSD__ < 3
int gif_ioctl __P((struct ifnet *, int, caddr_t));
#else
int gif_ioctl __P((struct ifnet *, u_long, caddr_t));
#endif

#endif /* _NET_IF_GIF_H_ */
