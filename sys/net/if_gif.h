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

#if defined(_KERNEL) && !defined(_LKM)
#include "opt_inet.h"
#endif

#include <netinet/in.h>
/* xxx sigh, why route have struct route instead of pointer? */

struct gif_softc {
	struct ifnet	gif_if;	   /* common area */
	struct sockaddr	*gif_psrc; /* Physical src addr */
	struct sockaddr	*gif_pdst; /* Physical dst addr */
	union {
		struct route  gifscr_ro;    /* xxx */
#ifdef INET6
		struct route_in6 gifscr_ro6; /* xxx */
#endif /*INET6*/
	} gifsc_gifscr;
	int		gif_flags;
};

#define gif_ro gifsc_gifscr.gifscr_ro
#ifdef INET6
#define gif_ro6 gifsc_gifscr.gifscr_ro6
#endif /*INET6*/

#define	GIFF_INUSE	0x1	/* gif is in use */

#define GIF_MTU		(1280)	/* Default MTU */
#define	GIF_MTU_MIN	(1280)	/* Minimum MTU */
#define	GIF_MTU_MAX	(8192)	/* Maximum MTU */

extern int ngif;
extern struct gif_softc *gif;

/* Prototypes */
void gif_input __P((struct mbuf *, int, struct ifnet *));
int gif_output __P((struct ifnet *, struct mbuf *,
		    struct sockaddr *, struct rtentry *));
int gif_ioctl __P((struct ifnet *, u_long, caddr_t));

#endif /* _NET_IF_GIF_H_ */
