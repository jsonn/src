/*	$NetBSD: pfil.h,v 1.13.4.1 2001/04/23 21:49:55 he Exp $	*/

/*
 * Copyright (c) 1996 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NET_PFIL_H_
#define _NET_PFIL_H_

#include <sys/queue.h>
#include <sys/null.h>

struct mbuf;
struct ifnet;

/*
 * The packet filter hooks are designed for anything to call them to
 * possibly intercept the packet.
 */
struct packet_filter_hook {
        TAILQ_ENTRY(packet_filter_hook) pfil_link;
        int	(*pfil_func) __P((void *, int, struct ifnet *, int,
				  struct mbuf **));
	int	pfil_flags;
};

#define PFIL_IN		0x00000001
#define PFIL_OUT	0x00000002
#define PFIL_WAITOK	0x00000004
#define PFIL_ALL	(PFIL_IN|PFIL_OUT)

typedef	TAILQ_HEAD(pfil_list, packet_filter_hook) pfil_list_t;

struct pfil_head {
	pfil_list_t	ph_in;
	pfil_list_t	ph_out;
	int		ph_init;
};
typedef struct pfil_head pfil_head_t;

struct packet_filter_hook *pfil_hook_get __P((int, struct pfil_head *));
int	pfil_add_hook __P((int (*func) __P((void *, int,
	    struct ifnet *, int, struct mbuf **)), int, struct pfil_head *));
int	pfil_remove_hook __P((int (*func) __P((void *, int,
	    struct ifnet *, int, struct mbuf **)), int, struct pfil_head *));

/* XXX */
#if defined(_KERNEL) && !defined(_LKM)
#include "ipfilter.h"
#endif

#if NIPFILTER > 0
#ifdef PFIL_HOOKS
#undef PFIL_HOOKS
#endif
#define PFIL_HOOKS
#endif /* NIPFILTER */

#endif /* _NET_PFIL_H_ */
