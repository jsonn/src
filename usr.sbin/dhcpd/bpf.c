/* bpf.c

   BPF socket interface code, originally contributed by Archie Cobbs. */

/*
 * Copyright (c) 1995, 1996 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: bpf.c,v 1.2.2.1 1997/03/10 20:34:49 is Exp $ Copyright (c) 1995, 1996 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#if defined (USE_BPF_SEND) || defined (USE_BPF_RECEIVE)
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <net/bpf.h>
#ifdef NEED_OSF_PFILT_HACKS
#include <net/pfilt.h>
#endif
#include <netinet/in_systm.h>
#ifdef __NetBSD__
#include <net/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#else
#include "includes/netinet/ip.h"
#include "includes/netinet/udp.h"
#include "includes/netinet/if_ether.h"
#endif

/* Called by get_interface_list for each interface that's discovered.
   Opens a packet filter for each interface and adds it to the select
   mask. */

int if_register_bpf (info, ifp)
	struct interface_info *info;
	struct ifreq *ifp;
{
	int sock;
	char filename[50];
	int b;

	/* Open a BPF device */
	for (b = 0; 1; b++) {
#ifndef NO_SNPRINTF
		snprintf(filename, sizeof(filename), BPF_FORMAT, b);
#else
		sprintf(filename, BPF_FORMAT, b);
#endif
		sock = open (filename, O_RDWR, 0);
		if (sock < 0) {
			if (errno == EBUSY) {
				continue;
			} else {
				error ("Can't find free bpf: %m");
			}
		} else {
			break;
		}
	}

	/* Set the BPF device to point at this interface. */
	if (ioctl (sock, BIOCSETIF, ifp) < 0)
		error ("Can't attach interface %s to bpf device %s: %m",
		       info -> name, filename);

	return sock;
}
#endif /* USE_BPF_SEND || USE_BPF_RECEIVE */

#ifdef USE_BPF_SEND
void if_register_send (info, interface)
	struct interface_info *info;
	struct ifreq *interface;
{
	/* If we're using the bpf API for sending and receiving,
	   we don't need to register this interface twice. */
#ifndef USE_BPF_RECEIVE
	info -> wfdesc = if_register_bpf (info, interface);
#else
	info -> wfdesc = info -> rfdesc;
#endif
	note ("Sending on   BPF/%s/%s/%s",
	      info -> name,
	      print_hw_addr (info -> hw_address.htype,
			     info -> hw_address.hlen,
			     info -> hw_address.haddr),
	      (info -> shared_network ?
	       info -> shared_network -> name : "unattached"));
}
#endif /* USE_BPF_SEND */

#ifdef USE_BPF_RECEIVE
/* Packet filter program...
   XXX Changes to the filter program may require changes to the constant
   offsets used in if_register_send to patch the BPF program! XXX */

struct bpf_insn filter [] = {
	/* Make sure this is an IP packet... */
	BPF_STMT (BPF_LD + BPF_H + BPF_ABS, 12),
	BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, ETHERTYPE_IP, 0, 8),

	/* Make sure it's a UDP packet... */
	BPF_STMT (BPF_LD + BPF_B + BPF_ABS, 23),
	BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, IPPROTO_UDP, 0, 6),

	/* Make sure this isn't a fragment... */
	BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 20),
	BPF_JUMP(BPF_JMP + BPF_JSET + BPF_K, 0x1fff, 4, 0),

	/* Get the IP header length... */
	BPF_STMT (BPF_LDX + BPF_B + BPF_MSH, 14),

	/* Make sure it's to the right port... */
	BPF_STMT (BPF_LD + BPF_H + BPF_IND, 16),
	BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, 67, 0, 1),             /* patch */

	/* If we passed all the tests, ask for the whole packet. */
	BPF_STMT(BPF_RET+BPF_K, (u_int)-1),

	/* Otherwise, drop it. */
	BPF_STMT(BPF_RET+BPF_K, 0),
};

void if_register_receive (info, interface)
	struct interface_info *info;
	struct ifreq *interface;
{
	int flag = 1;
	struct bpf_version v;
	u_int32_t addr;
	struct bpf_program p;
	u_int32_t bits;

	/* Open a BPF device and hang it on this interface... */
	info -> rfdesc = if_register_bpf (info, interface);

	/* Make sure the BPF version is in range... */
	if (ioctl (info -> rfdesc, BIOCVERSION, &v) < 0)
		error ("Can't get BPF version: %m");

	if (v.bv_major != BPF_MAJOR_VERSION ||
	    v.bv_minor < BPF_MINOR_VERSION)
		error ("Kernel BPF version out of range - recompile dhcpd!");

	/* Set immediate mode so that reads return as soon as a packet
	   comes in, rather than waiting for the input buffer to fill with
	   packets. */
	if (ioctl (info -> rfdesc, BIOCIMMEDIATE, &flag) < 0)
		error ("Can't set immediate mode on bpf device: %m");

#ifdef NEED_OSF_PFILT_HACKS
	/* Allow the copyall flag to be set... */
	if (ioctl(info -> rfdesc, EIOCALLOWCOPYALL, &flag) < 0)
		error ("Can't set ALLOWCOPYALL: %m");

	/* Clear all the packet filter mode bits first... */
	bits = 0;
	if (ioctl (info -> rfdesc, EIOCMBIS, &bits) < 0)
		error ("Can't clear pfilt bits: %m");

	/* Set the ENBATCH, ENCOPYALL, ENBPFHDR bits... */
	bits = ENBATCH | ENCOPYALL | ENBPFHDR;
	if (ioctl (info -> rfdesc, EIOCMBIS, &bits) < 0)
		error ("Can't set ENBATCH|ENCOPYALL|ENBPFHDR: %m");
#endif
	/* Get the required BPF buffer length from the kernel. */
	if (ioctl (info -> rfdesc, BIOCGBLEN, &info -> rbuf_max) < 0)
		error ("Can't get bpf buffer length: %m");
	info -> rbuf = malloc (info -> rbuf_max);
	if (!info -> rbuf)
		error ("Can't allocate %d bytes for bpf input buffer.");
	info -> rbuf_offset = 0;
	info -> rbuf_len = 0;

	/* Set up the bpf filter program structure. */
	p.bf_len = sizeof filter / sizeof (struct bpf_insn);
	p.bf_insns = filter;

        /* Patch the server port into the BPF  program...
	   XXX changes to filter program may require changes
	   to the insn number(s) used below! XXX */
	filter [8].k = ntohs (server_port);

	if (ioctl (info -> rfdesc, BIOCSETF, &p) < 0)
		error ("Can't install packet filter program: %m");
	note ("Listening on BPF/%s/%s/%s",
	      info -> name,
	      print_hw_addr (info -> hw_address.htype,
			     info -> hw_address.hlen,
			     info -> hw_address.haddr),
	      (info -> shared_network ?
	       info -> shared_network -> name : "unattached"));
}
#endif /* USE_BPF_RECEIVE */

#ifdef USE_BPF_SEND
size_t send_packet (interface, packet, raw, len, from, to, hto)
	struct interface_info *interface;
	struct packet *packet;
	struct dhcp_packet *raw;
	size_t len;
	struct in_addr from;
	struct sockaddr_in *to;
	struct hardware *hto;
{
	int bufp = 0;
	unsigned char buf [256];
	struct iovec iov [2];

	/* Assemble the headers... */
	assemble_hw_header (interface, buf, &bufp, hto);
	assemble_udp_ip_header (interface, buf, &bufp, from.s_addr,
				to -> sin_addr.s_addr, to -> sin_port,
				(unsigned char *)raw, len);

	/* Fire it off */
	iov [0].iov_base = (char *)buf;
	iov [0].iov_len = bufp;
	iov [1].iov_base = (char *)raw;
	iov [1].iov_len = len;

	return writev(interface -> wfdesc, iov, 2);
}
#endif /* USE_BPF_SEND */

#ifdef USE_BPF_RECEIVE
size_t receive_packet (interface, buf, len, from, hfrom)
	struct interface_info *interface;
	unsigned char *buf;
	size_t len;
	struct sockaddr_in *from;
	struct hardware *hfrom;
{
	int length = 0;
	int offset = 0;
	struct bpf_hdr hdr;

	/* All this complexity is because BPF doesn't guarantee
	   that only one packet will be returned at a time.   We're
	   getting what we deserve, though - this is a terrible abuse
	   of the BPF interface.   Sigh. */

	/* Process packets until we get one we can return or until we've
	   done a read and gotten nothing we can return... */

	do {
		/* If the buffer is empty, fill it. */
		if (interface -> rbuf_offset == interface -> rbuf_len) {
			length = read (interface -> rfdesc,
				       interface -> rbuf,
				       interface -> rbuf_max);
			if (length <= 0)
				return length;
			interface -> rbuf_offset = 0;
			interface -> rbuf_len = length;
		}

		/* If there isn't room for a whole bpf header, something went
		   wrong, but we'll ignore it and hope it goes away... XXX */
		if (interface -> rbuf_len -
		    interface -> rbuf_offset < sizeof hdr) {
			interface -> rbuf_offset = interface -> rbuf_len;
			continue;
		}

		/* Copy out a bpf header... */
		memcpy (&hdr, &interface -> rbuf [interface -> rbuf_offset],
			sizeof hdr);

		/* If the bpf header plus data doesn't fit in what's left
		   of the buffer, stick head in sand yet again... */
		if (interface -> rbuf_offset +
		    hdr.bh_hdrlen + hdr.bh_caplen > interface -> rbuf_len) {
			interface -> rbuf_offset = interface -> rbuf_len;
			continue;
		}

		/* If the captured data wasn't the whole packet, or if
		   the packet won't fit in the input buffer, all we
		   can do is drop it. */
		if (hdr.bh_caplen != hdr.bh_datalen) {
			interface -> rbuf_offset +=
				hdr.bh_hdrlen = hdr.bh_caplen;
			continue;
		}

		/* Skip over the BPF header... */
		interface -> rbuf_offset += hdr.bh_hdrlen;

		/* Decode the physical header... */
		offset = decode_hw_header (interface,
					   interface -> rbuf,
					   interface -> rbuf_offset,
					   hfrom);

		/* If a physical layer checksum failed (dunno of any
		   physical layer that supports this, but WTH), skip this
		   packet. */
		if (offset < 0) {
			interface -> rbuf_offset += hdr.bh_caplen;
			continue;
		}
		interface -> rbuf_offset += offset;
		hdr.bh_caplen -= offset;

		/* Decode the IP and UDP headers... */
		offset = decode_udp_ip_header (interface,
					       interface -> rbuf,
					       interface -> rbuf_offset,
					       from,
					       (unsigned char *)0,
					       hdr.bh_caplen);

		/* If the IP or UDP checksum was bad, skip the packet... */
		if (offset < 0) {
			interface -> rbuf_offset += hdr.bh_caplen;
			continue;
		}
		interface -> rbuf_offset += offset;
		hdr.bh_caplen -= offset;

		/* If there's not enough room to stash the packet data,
		   we have to skip it (this shouldn't happen in real
		   life, though). */
		if (hdr.bh_caplen > len) {
			interface -> rbuf_offset += hdr.bh_caplen;
			continue;
		}

		/* Copy out the data in the packet... */
		memcpy (buf, interface -> rbuf + interface -> rbuf_offset,
			hdr.bh_caplen);
		interface -> rbuf_offset += hdr.bh_caplen;
		return hdr.bh_caplen;
	} while (!length);
	return 0;
}
#endif
