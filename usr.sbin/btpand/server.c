/*	$NetBSD: server.c,v 1.1.2.2 2008/09/18 04:30:02 wrstuden Exp $	*/

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
__RCSID("$NetBSD: server.c,v 1.1.2.2 2008/09/18 04:30:02 wrstuden Exp $");

#include <sys/ioctl.h>

#include <bluetooth.h>
#include <errno.h>
#include <sdp.h>
#include <unistd.h>

#include "btpand.h"
#include "bnep.h"

static struct event	server_ev;
static int		server_fd;
static int		server_load;

static void *		server_ss;
static uint32_t		server_handle;

static void server_open(void);
static void server_close(void);
static void server_read(int, short, void *);
static void server_register(void);

void
server_init(void)
{

	server_fd = -1;
}

/*
 * The server_update() function is called whenever the channel count is
 * changed. We maintain the SDP record and open or close the server socket
 * as required.
 */
void
server_update(int count)
{

	if (server_limit == 0)
		return;

	log_debug("count %d", count);

	server_load = (count - 1) * 100 / server_limit;
	log_info("server_load: %d%%", server_load);

	if (server_load > 99 && server_fd != -1)
		server_close();

	if (server_load < 100 && server_fd == -1)
		server_open();

	if (service_name)
		server_register();
}

static void
server_open(void)
{
	struct sockaddr_bt sa;
	uint16_t mru;

	server_fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	if (server_fd == -1) {
		log_err("Could not open L2CAP socket: %m");
		exit(EXIT_FAILURE);
	}

	memset(&sa, 0, sizeof(sa));
	sa.bt_family = AF_BLUETOOTH;
	sa.bt_len = sizeof(sa);
	sa.bt_psm = l2cap_psm;
	bdaddr_copy(&sa.bt_bdaddr, &local_bdaddr);
	if (bind(server_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
		log_err("Could not bind server socket: %m");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(server_fd, BTPROTO_L2CAP,
	    SO_L2CAP_LM, &l2cap_mode, sizeof(l2cap_mode)) == -1) {
		log_err("Could not set link mode (0x%4.4x): %m", l2cap_mode);
		exit(EXIT_FAILURE);
	}

	mru = BNEP_MTU_MIN;
	if (setsockopt(server_fd, BTPROTO_L2CAP,
	    SO_L2CAP_IMTU, &mru, sizeof(mru)) == -1) {
		log_err("Could not set L2CAP IMTU (%d): %m", mru);
		exit(EXIT_FAILURE);
	}

	if (listen(server_fd, 0) == -1) {
		log_err("Could not listen on server socket: %m");
		exit(EXIT_FAILURE);
	}

	event_set(&server_ev, server_fd, EV_READ | EV_PERSIST, server_read, NULL);
	if (event_add(&server_ev, NULL) == -1) {
		log_err("Could not add server event: %m");
		exit(EXIT_FAILURE);
	}

	log_info("server socket open");
}

static void
server_close(void)
{

	event_del(&server_ev);
	close(server_fd);
	server_fd = -1;

	log_info("server socket closed");
}

/*
 * handle connection request
 */
static void
server_read(int s, short ev, void *arg)
{
	struct sockaddr_bt ra, la;
	channel_t *chan;
	socklen_t len;
	int fd, n;
	uint16_t mru, mtu;

	len = sizeof(ra);
	fd = accept(s, (struct sockaddr *)&ra, &len);
	if (fd == -1)
		return;

	n = 1;
	if (ioctl(fd, FIONBIO, &n) == -1) {
		log_err("Could not set NonBlocking IO: %m");
		close(fd);
		return;
	}

	len = sizeof(mru);
	if (getsockopt(fd, BTPROTO_L2CAP, SO_L2CAP_IMTU, &mru, &len) == -1) {
		log_err("Could not get L2CAP IMTU: %m");
		close(fd);
		return;
	}
	if(mru < BNEP_MTU_MIN) {
		log_err("L2CAP IMTU too small (%d)", mru);
		close(fd);
		return;
	}

	len = sizeof(mtu);
	if (getsockopt(fd, BTPROTO_L2CAP, SO_L2CAP_OMTU, &mtu, &len) == -1) {
		log_err("Could not get L2CAP OMTU: %m");
		close(fd);
		return;
	}
	if (mtu < BNEP_MTU_MIN) {
		log_err("L2CAP OMTU too small (%d)", mtu);
		close(fd);
		return;
	}

	len = sizeof(n);
	if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &n, &len) == -1) {
		log_err("Could not get socket send buffer size: %m");
		close(fd);
		return;
	}

	if (n < (mtu * 2)) {
		n = mtu * 2;
		if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n)) == -1) {
			log_err("Could not set socket send buffer size (%d): %m", n);
			close(fd);
			return;
		}
	}

	n = mtu;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDLOWAT, &n, sizeof(n)) == -1) {
		log_err("Could not set socket low water mark (%d): %m", n);
		close(fd);
		return;
	}

	len = sizeof(la);
	if (getsockname(fd, (struct sockaddr *)&la, &len) == -1) {
		log_err("Could not get socket address: %m");
		close(fd);
		return;
	}

	log_info("Accepted connection from %s", bt_ntoa(&ra.bt_bdaddr, NULL));

	chan = channel_alloc();
	if (chan == NULL) {
		close(fd);
		return;
	}

	chan->send = bnep_send;
	chan->recv = bnep_recv;
	chan->mru = mru;
	chan->mtu = mtu;
	b2eaddr(chan->raddr, &ra.bt_bdaddr);
	b2eaddr(chan->laddr, &la.bt_bdaddr);
	chan->state = CHANNEL_WAIT_CONNECT_REQ;
	channel_timeout(chan, 10);
	if (!channel_open(chan, fd)) {
		chan->state = CHANNEL_CLOSED;
		channel_free(chan);
		close(fd);
		return;
	}
}

static void
server_register(void)
{
	sdp_nap_profile_t p;
	int rv;

	if (server_ss == NULL) {
		server_ss = sdp_open_local(control_path);
		if (server_ss == NULL || sdp_error(server_ss) != 0) {
			log_err("failed to contact SDP server");
			return;
		}
	}

					memset(&p, 0, sizeof(p));

					p.psm = l2cap_psm;

	if (server_load < 1)		p.load_factor = 0;
	else if (server_load <= 17)	p.load_factor = 1;
	else if (server_load <= 33)	p.load_factor = 2;
	else if (server_load <= 50)	p.load_factor = 3;
	else if (server_load <= 67)	p.load_factor = 4;
	else if (server_load <= 83)	p.load_factor = 5;
	else if (server_load <= 99)	p.load_factor = 6;
	else				p.load_factor = 7;

	if (l2cap_mode != 0)		p.security_description = 0x0001;

	if (server_handle)
		rv = sdp_change_service(server_ss, server_handle,
		    (uint8_t *)&p, sizeof(p));
	else
		rv = sdp_register_service(server_ss, service_class,
		    &local_bdaddr, (uint8_t *)&p, sizeof(p), &server_handle);

	if (rv != 0) {
		errno = sdp_error(server_ss);
		log_err("%s: %m", service_name);
		exit(EXIT_FAILURE);
	}
}
