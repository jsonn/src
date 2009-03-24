/*	$NetBSD: tap.c,v 1.2.2.2 2009/03/24 21:46:36 bouyer Exp $	*/

/*-
 * Copyright (c) 2008 Iain Hibbert
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: tap.c,v 1.2.2.2 2009/03/24 21:46:36 bouyer Exp $");

#include <sys/ioctl.h>
#include <sys/uio.h>

#include <net/if_dl.h>
#include <net/if_tap.h>

#include <fcntl.h>
#include <unistd.h>
#include <util.h>

#include "btpand.h"

static bool tap_send(channel_t *, packet_t *);
static bool tap_recv(packet_t *);

void
tap_init(void)
{
	channel_t *chan;
	struct sockaddr *sa;
	struct ifaliasreq ifra;
	struct ifreq ifr;
	int fd, s;

	fd = open(interface_name, O_RDWR);
	if (fd == -1) {
		log_err("Could not open \"%s\": %m", interface_name);
		exit(EXIT_FAILURE);
	}

	memset(&ifr, 0, sizeof(ifr));
	if (ioctl(fd, TAPGIFNAME, &ifr) == -1) {
		log_err("Could not get interface name: %m");
		exit(EXIT_FAILURE);
	}

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		log_err("Could not open PF_INET socket: %m");
		exit(EXIT_FAILURE);
	}

	memset(&ifra, 0, sizeof(ifra));
	memcpy(ifra.ifra_name, ifr.ifr_name, IFNAMSIZ);

	sa = &ifra.ifra_addr;
	sa->sa_family = AF_LINK;
	sa->sa_len = sizeof(struct sockaddr);
	b2eaddr(sa->sa_data, &local_bdaddr);

	if (ioctl(s, SIOCSIFPHYADDR, &ifra) == -1) {
		log_err("Could not set %s physical address: %m", ifra.ifra_name);
		exit(EXIT_FAILURE);
	}

	if (ioctl(s, SIOCGIFFLAGS, &ifr) == -1) {
		log_err("Could not get interface flags: %m");
		exit(EXIT_FAILURE);
	}

	if ((ifr.ifr_flags & IFF_UP) == 0) {
		ifr.ifr_flags |= IFF_UP;

		if (ioctl(s, SIOCSIFFLAGS, &ifr) == -1) {
			log_err("Could not set IFF_UP: %m");
			exit(EXIT_FAILURE);
		}
	}

	close(s);

	log_info("Using interface %s with addr %s",
	    ifr.ifr_name, ether_ntoa((struct ether_addr *)sa->sa_data));

	chan = channel_alloc();
	if (chan == NULL)
		exit(EXIT_FAILURE);

	chan->send = tap_send;
	chan->recv = tap_recv;
	chan->mru = ETHER_HDR_LEN + ETHER_MAX_LEN;
	memcpy(chan->raddr, sa->sa_data, ETHER_ADDR_LEN);
	memcpy(chan->laddr, sa->sa_data, ETHER_ADDR_LEN);
	chan->state = CHANNEL_OPEN;
	if (!channel_open(chan, fd))
		exit(EXIT_FAILURE);

	if (pidfile(ifr.ifr_name) == -1)
		log_err("pidfile not made");
}

static bool
tap_send(channel_t *chan, packet_t *pkt)
{
	struct iovec iov[4];
	ssize_t nw;

	iov[0].iov_base = pkt->dst;
	iov[0].iov_len = ETHER_ADDR_LEN;
	iov[1].iov_base = pkt->src;
	iov[1].iov_len = ETHER_ADDR_LEN;
	iov[2].iov_base = pkt->type;
	iov[2].iov_len = ETHER_TYPE_LEN;
	iov[3].iov_base = pkt->ptr;
	iov[3].iov_len = pkt->len;

	/* tap device write never fails */
	nw = writev(chan->fd, iov, __arraycount(iov));
	_DIAGASSERT(nw > 0);

	return true;
}

static bool
tap_recv(packet_t *pkt)
{

	if (pkt->len < ETHER_HDR_LEN)
		return false;

	pkt->dst = pkt->ptr;
	packet_adj(pkt, ETHER_ADDR_LEN);
	pkt->src = pkt->ptr;
	packet_adj(pkt, ETHER_ADDR_LEN);
	pkt->type = pkt->ptr;
	packet_adj(pkt, ETHER_TYPE_LEN);

	return true;
}
