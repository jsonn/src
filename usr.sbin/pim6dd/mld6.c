/*	$NetBSD: mld6.c,v 1.2.4.1 1999/12/27 18:37:57 wrstuden Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
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
 *  Copyright (c) 1998 by the University of Southern California.
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation in source and binary forms for lawful
 *  purposes and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and that both
 *  the copyright notice and this permission notice appear in supporting
 *  documentation, and that any documentation, advertising materials,
 *  and other materials related to such distribution and use acknowledge
 *  that the software was developed by the University of Southern
 *  California and/or Information Sciences Institute.
 *  The name of the University of Southern California may not
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 *  THE UNIVERSITY OF SOUTHERN CALIFORNIA DOES NOT MAKE ANY REPRESENTATIONS
 *  ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND 
 *  NON-INFRINGEMENT.
 *
 *  IN NO EVENT SHALL USC, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
 *  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
 *  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
 *  THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 */
/*
 *  Questions concerning this software should be directed to 
 *  Pavlin Ivanov Radoslavov (pavlin@catarina.usc.edu)
 *
 *  KAME Id: mld6.c,v 1.3 1999/10/26 08:39:19 itojun Exp
 */
/*
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 */

#include "defs.h"
#include <sys/uio.h>

/*
 * Exported variables.
 */

char *mld6_recv_buf;		/* input packet buffer */
char *mld6_send_buf;		/* output packet buffer */
int mld6_socket;		/* socket for all network I/O */
struct sockaddr_in6 allrouters_group = {sizeof(struct sockaddr_in6), AF_INET6};
struct sockaddr_in6 allnodes_group = {sizeof(struct sockaddr_in6), AF_INET6};

/* Extenals */

extern struct in6_addr in6addr_linklocal_allnodes;

/* local variables. */
static struct sockaddr_in6 	dst = {sizeof(dst), AF_INET6};
static struct msghdr 		sndmh,
                		rcvmh;
static struct iovec 		sndiov[2];
static struct iovec 		rcviov[2];
static struct sockaddr_in6 	from;
static u_char   		rcvcmsgbuf[CMSG_SPACE(sizeof(struct in6_pktinfo)) +
			   	CMSG_SPACE(sizeof(int))];
u_int8_t raopt[IP6OPT_RTALERT_LEN];
static char *sndcmsgbuf;
static int ctlbuflen = 0;

/* local functions */

static void mld6_read __P((int i, fd_set * fds));
static void accept_mld6 __P((int len));
static void make_mld6_msg __P((int, int, struct sockaddr_in6 *,
	struct sockaddr_in6 *, struct in6_addr *, int, int, int, int));

/*
 * Open and initialize the MLD socket.
 */
void
init_mld6()
{
    struct icmp6_filter filt;
    int             on;
    u_short         rtalert_code = htons(IP6OPT_RTALERT_MLD);

    if (!mld6_recv_buf && (mld6_recv_buf = malloc(RECV_BUF_SIZE)) == NULL)
	    log(LOG_ERR, 0, "malloca failed");
    if (!mld6_send_buf && (mld6_send_buf = malloc(RECV_BUF_SIZE)) == NULL)
	    log(LOG_ERR, 0, "malloca failed");

    IF_DEBUG(DEBUG_KERN)
        log(LOG_DEBUG,0,"%d octets allocated for the emit/recept buffer mld6",RECV_BUF_SIZE);

    if ((mld6_socket = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0)
		log(LOG_ERR, errno, "MLD6 socket");

    k_set_rcvbuf(mld6_socket, SO_RECV_BUF_SIZE_MAX,
		 SO_RECV_BUF_SIZE_MIN);	/* lots of input buffering */
    k_set_hlim(mld6_socket, MINHLIM);	/* restrict multicasts to one hop */
    k_set_loop(mld6_socket, FALSE);	/* disable multicast loopback     */

    /* address initialization */
    allnodes_group.sin6_addr = in6addr_linklocal_allnodes;
    if (inet_pton(AF_INET6, "ff02::2",
		  (void *) &allrouters_group.sin6_addr) != 1)
	log(LOG_ERR, 0, "inet_pton failed for ff02::2");

    /* filter all non-MLD ICMP messages */
    ICMP6_FILTER_SETBLOCKALL(&filt);
    ICMP6_FILTER_SETPASS(ICMP6_MEMBERSHIP_QUERY, &filt);
    ICMP6_FILTER_SETPASS(ICMP6_MEMBERSHIP_REPORT, &filt);
    ICMP6_FILTER_SETPASS(ICMP6_MEMBERSHIP_REDUCTION, &filt);
    ICMP6_FILTER_SETPASS(MLD6_MTRACE_RESP, &filt);
    ICMP6_FILTER_SETPASS(MLD6_MTRACE, &filt);
    if (setsockopt(mld6_socket, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
		   sizeof(filt)) < 0)
	log(LOG_ERR, errno, "setsockopt(ICMP6_FILTER)");

    /* specify to tell receiving interface */
    on = 1;
    if (setsockopt(mld6_socket, IPPROTO_IPV6, IPV6_PKTINFO, &on,
		   sizeof(on)) < 0)
	log(LOG_ERR, errno, "setsockopt(IPV6_PKTINFO)");

    on = 1;
    /* specify to tell value of hoplimit field of received IP6 hdr */
    if (setsockopt(mld6_socket, IPPROTO_IPV6, IPV6_HOPLIMIT, &on,
		   sizeof(on)) < 0)
	log(LOG_ERR, errno, "setsockopt(IPV6_HOPLIMIT)");

    /* initialize msghdr for receiving packets */
    rcviov[0].iov_base = (caddr_t) mld6_recv_buf;
    rcviov[0].iov_len = RECV_BUF_SIZE;
    rcvmh.msg_name = (caddr_t) & from;
    rcvmh.msg_namelen = sizeof(from);
    rcvmh.msg_iov = rcviov;
    rcvmh.msg_iovlen = 1;
    rcvmh.msg_control = (caddr_t) rcvcmsgbuf;
    rcvmh.msg_controllen = sizeof(rcvcmsgbuf);

    /* initialize msghdr for sending packets */
    sndiov[0].iov_base = (caddr_t)mld6_send_buf;
    sndmh.msg_namelen = sizeof(struct sockaddr_in6);
    sndmh.msg_iov = sndiov;
    sndmh.msg_iovlen = 1;
    /* specifiy to insert router alert option in a hop-by-hop opt hdr. */
    raopt[0] = IP6OPT_RTALERT;
    raopt[1] = IP6OPT_RTALERT_LEN - 2;
    memcpy(&raopt[2], (caddr_t) & rtalert_code, sizeof(u_short));

    /* register MLD message handler */
    if (register_input_handler(mld6_socket, mld6_read) < 0)
	log(LOG_ERR, 0,
	    "Couldn't register mld6_read as an input handler");
}

/* Read an MLD message */
static void
mld6_read(i, rfd)
    int             i;
    fd_set         *rfd;
{
    register int    mld6_recvlen;

    mld6_recvlen = recvmsg(mld6_socket, &rcvmh, 0);

    if (mld6_recvlen < 0)
    {
	if (errno != EINTR)
	    log(LOG_ERR, errno, "MLD6 recvmsg");
	return;
    }

    /* TODO: make it as a thread in the future releases */
    accept_mld6(mld6_recvlen);
}

/*
 * Process a newly received MLD6 packet that is sitting in the input packet
 * buffer.
 */
static void
accept_mld6(recvlen)
int recvlen;
{
	struct in6_addr *group, *dst = NULL;
	struct mld6_hdr *mldh;
	struct cmsghdr *cm;
	struct in6_pktinfo *pi = NULL;
	int *hlimp = NULL;
	int ifindex = 0;
	struct sockaddr_in6 *src = (struct sockaddr_in6 *) rcvmh.msg_name;

	/*
	 * If control length is zero, it must be an upcall from the kernel
	 * multicast forwarding engine.
	 * XXX: can we trust it?
	 */
	if (rcvmh.msg_controllen == 0) {
		/* XXX: msg_controllen must be reset in this case. */
		rcvmh.msg_controllen = sizeof(rcvcmsgbuf);

		process_kernel_call();
		return;
	}

	if (recvlen < sizeof(struct mld6_hdr))
	{
		log(LOG_WARNING, 0,
		    "received packet too short (%u bytes) for MLD header",
		    recvlen);
		return;
	}
	mldh = (struct mld6_hdr *) rcvmh.msg_iov[0].iov_base;
	group = &mldh->mld6_addr;

	/* extract optional information via Advanced API */
	for (cm = (struct cmsghdr *) CMSG_FIRSTHDR(&rcvmh);
	     cm;
	     cm = (struct cmsghdr *) CMSG_NXTHDR(&rcvmh, cm))
	{
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo)))
		{
			pi = (struct in6_pktinfo *) (CMSG_DATA(cm));
			ifindex = pi->ipi6_ifindex;
			dst = &pi->ipi6_addr;
		}
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			hlimp = (int *) CMSG_DATA(cm);
	}
	if (hlimp == NULL)
	{
		log(LOG_WARNING, 0,
		    "failed to get receiving hop limit");
		return;
	}

	/* TODO: too noisy. Remove it? */
//#define NOSUCHDEF
#ifdef NOSUCHDEF
	IF_DEBUG(DEBUG_PKT | debug_kind(IPPROTO_ICMPV6, mldh->mld6_type,
					mldh->mld6_code))
		log(LOG_DEBUG, 0, "RECV %s from %s to %s",
		    packet_kind(IPPROTO_ICMPV6,
				mldh->mld6_type, mldh->mld6_code),
		    inet6_fmt(&src->sin6_addr), inet6_fmt(dst));
#endif				/* NOSUCHDEF */

	/* for an mtrace message, we don't need strict checks */
	if (mldh->mld6_type == MLD6_MTRACE) {
		accept_mtrace(src, dst, group, ifindex, (char *)(mldh + 1),
			      mldh->mld6_code, recvlen - sizeof(struct mld6_hdr));
		return;
	}

	/* hop limit check */
	if (*hlimp != 1)
	{
		log(LOG_WARNING, 0,
		    "received an MLD6 message with illegal hop limit(%d) from %s",
		    *hlimp, inet6_fmt(&src->sin6_addr));
		/* but accept the packet */
	}
	if (ifindex == 0)
	{
		log(LOG_WARNING, 0, "failed to get receiving interface");
		return;
	}

	/* scope check */
	if (IN6_IS_ADDR_MC_NODELOCAL(&mldh->mld6_addr))
	{
		log(LOG_INFO, 0,
		    "RECV %s with an invalid scope: %s from %s",
		    inet6_fmt(&mldh->mld6_addr),
		    inet6_fmt(&src->sin6_addr));
		return;			/* discard */
	}

	/* source address check */
	if (!IN6_IS_ADDR_LINKLOCAL(&src->sin6_addr))
	{
		log(LOG_INFO, 0,
		    "RECV %s from a non link local address: %s",
		    packet_kind(IPPROTO_ICMPV6, mldh->mld6_type,
				mldh->mld6_code),
		    inet6_fmt(&src->sin6_addr));
		return;
	}

	switch (mldh->mld6_type)
	{
	case MLD6_LISTENER_QUERY:
		accept_listener_query(src, dst, group,
				      ntohs(mldh->mld6_maxdelay));
		return;

	case MLD6_LISTENER_REPORT:
		accept_listener_report(src, dst, group);
		return;

	case MLD6_LISTENER_DONE:
		accept_listener_done(src, dst, group);
		return;

	default:
		/* This must be impossible since we set a type filter */
		log(LOG_INFO, 0,
		    "ignoring unknown ICMPV6 message type %x from %s to %s",
		    mldh->mld6_type, inet6_fmt(&src->sin6_addr),
		    inet6_fmt(dst));
		return;
	}
}

static void
make_mld6_msg(type, code, src, dst, group, ifindex, delay, datalen, alert)
    int type, code, ifindex, delay, datalen, alert;
    struct sockaddr_in6 *src, *dst;
    struct in6_addr *group;
{
    static struct sockaddr_in6 dst_sa = {sizeof(dst_sa), AF_INET6};
    struct mld6_hdr *mhp = (struct mld6_hdr *)mld6_send_buf;
    int ctllen;

    switch(type) {
    case MLD6_MTRACE:
    case MLD6_MTRACE_RESP:
	sndmh.msg_name = (caddr_t)dst;
	break;
    default:
	if (IN6_IS_ADDR_UNSPECIFIED(group))
	    dst_sa.sin6_addr = allnodes_group.sin6_addr;
	else
	    dst_sa.sin6_addr = *group;
	sndmh.msg_name = (caddr_t)&dst_sa;
	datalen = sizeof(struct mld6_hdr);
	break;
    }
   
    bzero(mhp, sizeof(*mhp));
    mhp->mld6_type = type;
    mhp->mld6_code = code;
    mhp->mld6_maxdelay = htons(delay);
    mhp->mld6_addr = *group;

    sndiov[0].iov_len = datalen;

    /* estimate total ancillary data length */
    ctllen = 0;
    if (ifindex != -1 || src)
	    ctllen += CMSG_SPACE(sizeof(struct in6_pktinfo));
    if (alert)
	    ctllen += inet6_option_space(sizeof(raopt));
    /* extend ancillary data space (if necessary) */
    if (ctlbuflen < ctllen) {
	    if (sndcmsgbuf)
		    free(sndcmsgbuf);
	    if ((sndcmsgbuf = malloc(ctllen)) == NULL)
		    log(LOG_ERR, 0, "make_mld6_msg: malloc failed"); /* assert */
	    ctlbuflen = ctllen;
    }
    /* store ancillary data */
    if ((sndmh.msg_controllen = ctllen) > 0) {
	    struct cmsghdr *cmsgp;

	    sndmh.msg_control = sndcmsgbuf;
	    cmsgp = CMSG_FIRSTHDR(&sndmh);

	    if (ifindex != -1 || src) {
		    struct in6_pktinfo *pktinfo;

		    cmsgp->cmsg_len = CMSG_SPACE(sizeof(struct in6_pktinfo));
		    cmsgp->cmsg_level = IPPROTO_IPV6;
		    cmsgp->cmsg_type = IPV6_PKTINFO;
		    pktinfo = (struct in6_pktinfo *)CMSG_DATA(cmsgp);
		    memset((caddr_t)pktinfo, 0, sizeof(*pktinfo));
		    if (ifindex != -1)
			    pktinfo->ipi6_ifindex = ifindex;
		    if (src)
			    pktinfo->ipi6_addr = src->sin6_addr;
		    cmsgp = CMSG_NXTHDR(&sndmh, cmsgp);
	    }
	    if (alert) {
		    if (inet6_option_init((void *)cmsgp, &cmsgp, IPV6_HOPOPTS))
			    log(LOG_ERR, 0, /* assert */
				"make_mld6_msg: inet6_option_init failed");
		    if (inet6_option_append(cmsgp, raopt, 4, 0))
			    log(LOG_ERR, 0, /* assert */
				"make_mld6_msg: inet6_option_append failed");
		    cmsgp = CMSG_NXTHDR(&sndmh, cmsgp);
	    }
    }
    else
	    sndmh.msg_control = NULL; /* clear for safety */
}

void
send_mld6(type, code, src, dst, group, index, delay, datalen, alert)
    int type;
    int code;		/* for trace packets only */
    struct sockaddr_in6 *src;
    struct sockaddr_in6 *dst; /* may be NULL */
    struct in6_addr *group;
    int index, delay, alert;
    int datalen;		/* for trace packets only */
{
    int setloop = 0;
    struct sockaddr_in6 *dstp;
	
    make_mld6_msg(type, code, src, dst, group, index, delay, datalen, alert);
    dstp = (struct sockaddr_in6 *)sndmh.msg_name;
    if (IN6_ARE_ADDR_EQUAL(&dstp->sin6_addr, &allnodes_group.sin6_addr)) {
	setloop = 1;
	k_set_loop(mld6_socket, TRUE);
    }
    if (sendmsg(mld6_socket, &sndmh, 0) < 0) {
	if (errno == ENETDOWN)
	    check_vif_state();
	else
	    log(log_level(IPPROTO_ICMPV6, type, 0), errno,
		"sendmsg to %s with src %s on %s",
		inet6_fmt(&dstp->sin6_addr),
		src ? inet6_fmt(&src->sin6_addr) : "(unspec)",
		ifindex2str(index));

	if (setloop)
	    k_set_loop(mld6_socket, FALSE);
	return;
    }
    
    IF_DEBUG(DEBUG_PKT|debug_kind(IPPROTO_IGMP, type, 0))
	log(LOG_DEBUG, 0, "SENT %s from %-15s to %s",
	    packet_kind(IPPROTO_ICMPV6, type, 0),
	    src ? inet6_fmt(&src->sin6_addr) : "unspec",
	    inet6_fmt(&dstp->sin6_addr));
}
