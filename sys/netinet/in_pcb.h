/*	$NetBSD: in_pcb.h,v 1.26.8.1 1999/07/01 23:47:00 thorpej Exp $	*/

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
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *	@(#)in_pcb.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_IN_PCB_H_
#define _NETINET_IN_PCB_H_

#include <sys/queue.h>
#if 1	/*IPSEC*/
#include <netinet6/ipsec.h>
#endif

/*
 * Common structure pcb for internet protocol implementation.
 * Here are stored pointers to local and foreign host table
 * entries, local and foreign socket numbers, and pointers
 * up (to a socket structure) and down (to a protocol-specific)
 * control block.
 */
struct inpcb {
	LIST_ENTRY(inpcb) inp_hash;
	CIRCLEQ_ENTRY(inpcb) inp_queue;
	caddr_t	  inp_ppcb;		/* pointer to per-protocol pcb */
	int	  inp_state;		/* bind/connect state */
	u_int16_t inp_fport;		/* foreign port */
	u_int16_t inp_lport;		/* local port */
	struct	  socket *inp_socket;	/* back pointer to socket */
	struct	  route inp_route;	/* placeholder for routing entry */
	int	  inp_flags;		/* generic IP/datagram flags */
	struct	  ip inp_ip;		/* header prototype; should have more */
	struct	  mbuf *inp_options;	/* IP options */
	struct	  ip_moptions *inp_moptions; /* IP multicast options */
	int	  inp_errormtu;		/* MTU of last xmit status = EMSGSIZE */
	struct	  inpcbtable *inp_table;
#if 1 /*IPSEC*/
	struct secpolicy *inp_sp;	/* security policy. It may not be
					 * used according to policy selection.
					 */
#endif
};
#define	inp_faddr	inp_ip.ip_dst
#define	inp_laddr	inp_ip.ip_src

LIST_HEAD(inpcbhead, inpcb);

struct inpcbtable {
	CIRCLEQ_HEAD(, inpcb) inpt_queue;
	struct	  inpcbhead *inpt_bindhashtbl;
	struct	  inpcbhead *inpt_connecthashtbl;
	u_long	  inpt_bindhash;
	u_long	  inpt_connecthash;
	u_int16_t inpt_lastport;
	u_int16_t inpt_lastlow;
};
#define inpt_lasthi inpt_lastport

/* states in inp_state: */
#define	INP_ATTACHED		0
#define	INP_BOUND		1
#define	INP_CONNECTED		2

/* flags in inp_flags: */
#define	INP_RECVOPTS		0x01	/* receive incoming IP options */
#define	INP_RECVRETOPTS		0x02	/* receive IP options for reply */
#define	INP_RECVDSTADDR		0x04	/* receive IP dst address */
#define	INP_HDRINCL		0x08	/* user supplies entire IP header */
#define	INP_HIGHPORT		0x10	/* (unused; FreeBSD compat) */
#define	INP_LOWPORT		0x20	/* user wants "low" port binding */
#define	INP_ANONPORT		0x40	/* port chosen for user */
#define	INP_RECVIF		0x80	/* receive incoming interface */
#define	INP_CONTROLOPTS		(INP_RECVOPTS|INP_RECVRETOPTS|INP_RECVDSTADDR|\
				INP_RECVIF)

#define	sotoinpcb(so)		((struct inpcb *)(so)->so_pcb)

#ifdef _KERNEL
void	in_losing __P((struct inpcb *));
int	in_pcballoc __P((struct socket *, void *));
int	in_pcbbind __P((void *, struct mbuf *, struct proc *));
int	in_pcbconnect __P((void *, struct mbuf *));
void	in_pcbdetach __P((void *));
void	in_pcbdisconnect __P((void *));
void	in_pcbinit __P((struct inpcbtable *, int, int));
struct inpcb *
	in_pcblookup_bind __P((struct inpcbtable *,
	    struct in_addr, u_int));
struct inpcb *
	in_pcblookup_connect __P((struct inpcbtable *,
	    struct in_addr, u_int, struct in_addr, u_int));
int	in_pcbnotify __P((struct inpcbtable *, struct in_addr, u_int,
	    struct in_addr, u_int, int, void (*)(struct inpcb *, int)));
void	in_pcbnotifyall __P((struct inpcbtable *, struct in_addr, int,
	    void (*)(struct inpcb *, int)));
void	in_pcbstate __P((struct inpcb *, int));
void	in_rtchange __P((struct inpcb *, int));
void	in_setpeeraddr __P((struct inpcb *, struct mbuf *));
void	in_setsockaddr __P((struct inpcb *, struct mbuf *));
struct rtentry *
	in_pcbrtentry __P((struct inpcb *));
extern struct sockaddr_in *in_selectsrc __P((struct sockaddr_in *,
	struct route *, int, struct ip_moptions *, int *));
#endif

#endif /* _NETINET_IN_PCB_H_ */
