/* $NetBSD: if_pppoe.h,v 1.1.2.2 2001/06/21 20:08:11 nathanw Exp $ */

/*
 * Copyright (c) 2001 Martin Husemann. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

struct pppoediscparms {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	char	eth_ifname[IFNAMSIZ];	/* external ethernet interface name */
	char	*ac_name;		/* access concentrator name (or NULL) */
	size_t	ac_name_len;		/* on write: length of buffer for ac_name */
	char	*service_name;		/* service name (or NULL) */
	size_t	service_name_len;	/* on write: length of buffer for service name */
};

#define	PPPOESETPARMS	_IOW('i', 110, struct pppoediscparms)
#define	PPPOEGETPARMS	_IOR('i', 111, struct pppoediscparms)

#ifdef _KERNEL

extern struct ifqueue ppoediscinq;
extern struct ifqueue ppoeinq;

#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
extern void * pppoe_softintr;			/* softinterrupt cookie */
#else
extern struct callout pppoe_softintr;		/* callout (poor mans softint) */
extern void pppoe_softintr_handler(void*);	/* handler function */
#endif

#endif	/* _KERNEL */

