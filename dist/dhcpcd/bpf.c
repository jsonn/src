/*
 * dhcpcd - DHCP client daemon
 * Copyright 2006-2008 Roy Marples <roy@marples.name>
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
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <net/bpf.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "dhcp.h"
#include "logger.h"
#include "net.h"
#include "bpf-filter.h"

int
open_socket(struct interface *iface, int protocol)
{
	int fd = -1;
	struct ifreq ifr;
	int buf_len = 0;
	struct bpf_version pv;
	struct bpf_program pf;
#ifdef BIOCIMMEDIATE
	int flags;
#endif
#ifdef _PATH_BPF
	fd = open(_PATH_BPF, O_RDWR | O_NONBLOCK);
#else
	char *device;
	int n = 0;

	device = xmalloc(sizeof(char) * PATH_MAX);
	do {
		snprintf(device, PATH_MAX, "/dev/bpf%d", n++);
		fd = open(device, O_RDWR | O_NONBLOCK);
	} while (fd == -1 && errno == EBUSY);
	free(device);
#endif

	if (fd == -1)
		return -1;

	if (ioctl(fd, BIOCVERSION, &pv) == -1)
		goto eexit;
	if (pv.bv_major != BPF_MAJOR_VERSION ||
	    pv.bv_minor < BPF_MINOR_VERSION) {
		logger(LOG_ERR, "BPF version mismatch - recompile " PACKAGE);
		goto eexit;
	}

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, iface->name, sizeof(ifr.ifr_name));
	if (ioctl(fd, BIOCSETIF, &ifr) == -1)
		goto eexit;

	/* Get the required BPF buffer length from the kernel. */
	if (ioctl(fd, BIOCGBLEN, &buf_len) == -1)
		goto eexit;
	if (iface->buffer_size != (size_t)buf_len) {
		free(iface->buffer);
		iface->buffer_size = buf_len;
		iface->buffer = xmalloc(buf_len);
		iface->buffer_len = iface->buffer_pos = 0;
	}

#ifdef BIOCIMMEDIATE
	flags = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &flags) == -1)
		goto eexit;
#endif

	/* Install the DHCP filter */
	if (protocol == ETHERTYPE_ARP) {
		pf.bf_insns = UNCONST(arp_bpf_filter);
		pf.bf_len = arp_bpf_filter_len;
	} else {
		pf.bf_insns = UNCONST(dhcp_bpf_filter);
		pf.bf_len = dhcp_bpf_filter_len;
	}
	if (ioctl(fd, BIOCSETF, &pf) == -1)
		goto eexit;

	if (iface->fd != -1)
		close(iface->fd);

	close_on_exec(fd);
	iface->fd = fd;

	return fd;

eexit:
	free(iface->buffer);
	iface->buffer = NULL;
	close(fd);
	return -1;
}

ssize_t
send_raw_packet(const struct interface *iface, int type,
		const void *data, ssize_t len)
{
	struct iovec iov[2];
	struct ether_header hw;

	memset(&hw, 0, sizeof(hw));
	memset(&hw.ether_dhost, 0xff, ETHER_ADDR_LEN);
	hw.ether_type = htons(type);
	iov[0].iov_base = &hw;
	iov[0].iov_len = sizeof(hw);
	iov[1].iov_base = UNCONST(data);
	iov[1].iov_len = len;

	return writev(iface->fd, iov, 2);
}

/* BPF requires that we read the entire buffer.
 * So we pass the buffer in the API so we can loop on >1 dhcp packet. */
ssize_t
get_packet(struct interface *iface, void *data, ssize_t len)
{
	struct bpf_hdr packet;
	struct ether_header hw;
	ssize_t bytes;
	const unsigned char *payload, *d;

	for (;;) {
		if (iface->buffer_len == 0) {
			bytes = read(iface->fd, iface->buffer,
				     iface->buffer_size);
			if (bytes == -1)
				return errno == EAGAIN ? 0 : -1;
			else if ((size_t)bytes < sizeof(packet))
				return -1;
			iface->buffer_len = bytes;
			iface->buffer_pos = 0;
		}
		bytes = -1;
		memcpy(&packet, iface->buffer + iface->buffer_pos,
		       sizeof(packet));
		if (packet.bh_caplen != packet.bh_datalen)
			goto next; /* Incomplete packet, drop. */
		if (iface->buffer_pos + packet.bh_caplen + packet.bh_hdrlen >
		    iface->buffer_len)
			goto next; /* Packet beyond buffer, drop. */
		memcpy(&hw, iface->buffer + packet.bh_hdrlen, sizeof(hw));
		payload = iface->buffer + packet.bh_hdrlen + sizeof(hw);
		if (hw.ether_type == htons(ETHERTYPE_ARP)) {
			bytes = packet.bh_caplen - sizeof(hw);
			if (bytes > len)
				bytes = len;
			memcpy(data, payload, bytes);
		} else if (valid_udp_packet(payload) >= 0) {
			bytes = get_udp_data(&d, payload);
			if (bytes > len)
				bytes = len;
			memcpy(data, d, bytes);
		} else
			bytes = -1;
next:
		iface->buffer_pos += BPF_WORDALIGN(packet.bh_hdrlen +
						   packet.bh_caplen);
		if (iface->buffer_pos >= iface->buffer_len)
			iface->buffer_len = iface->buffer_pos = 0;
		if (bytes != -1)
			return bytes;
	}
}
