/* packet.c

   Packet assembly code, originally contributed by Archie Cobbs. */

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
"$Id: packet.c,v 1.2.2.1 1997/03/10 20:34:51 is Exp $ Copyright (c) 1996 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#if defined (PACKET_ASSEMBLY) || defined (PACKET_DECODING)
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

static u_int32_t checksum PROTO ((unsigned char *, int, u_int32_t));
static u_int32_t wrapsum PROTO ((u_int32_t));

/* Compute the easy part of the checksum on a range of bytes. */

static u_int32_t checksum (buf, nbytes, sum)
	unsigned char *buf;
	int nbytes;
	u_int32_t sum;
{
	int i;

#ifdef DEBUG_CHECKSUM
	debug ("checksum (%x %d %x)", buf, nbytes, sum);
#endif

	/* Checksum all the pairs of bytes first... */
	for (i = 0; i < (nbytes & ~1); i += 2) {
#ifdef DEBUG_CHECKSUM_VERBOSE
		debug ("sum = %x", sum);
#endif
		sum += (u_int16_t) ntohs(*((u_int16_t *)buf)++);
	}	

	/* If there's a single byte left over, checksum it, too.   Network
	   byte order is big-endian, so the remaining byte is the high byte. */
	if (i < nbytes) {
#ifdef DEBUG_CHECKSUM_VERBOSE
		debug ("sum = %x", sum);
#endif
		sum += (*buf) << 8;
	}
	
	return sum;
}

/* Fold the upper sixteen bits of the checksum down into the lower bits,
   complement the sum, and then put it into network byte order. */

static u_int32_t wrapsum (sum)
	u_int32_t sum;
{
#ifdef DEBUG_CHECKSUM
	debug ("wrapsum (%x)", sum);
#endif

	while (sum > 0x10000) {
		sum = (sum >> 16) + (sum & 0xFFFF);
#ifdef DEBUG_CHECKSUM_VERBOSE
		debug ("sum = %x", sum);
#endif
		sum += (sum >> 16);
#ifdef DEBUG_CHECKSUM_VERBOSE
		debug ("sum = %x", sum);
#endif
	}
	sum = sum ^ 0xFFFF;
#ifdef DEBUG_CHECKSUM_VERBOSE
	debug ("sum = %x", sum);
#endif
	
#ifdef DEBUG_CHECKSUM
	debug ("wrapsum returns %x", htons (sum));
#endif
	return htons(sum);
}
#endif /* PACKET_ASSEMBLY || PACKET_DECODING */

#ifdef PACKET_ASSEMBLY
/* Assemble an hardware header... */
/* XXX currently only supports ethernet; doesn't check for other types. */

void assemble_hw_header (interface, buf, bufix, to)
	struct interface_info *interface;
	unsigned char *buf;
	int *bufix;
	struct hardware *to;
{
	struct ether_header eh;

	if (to && to -> hlen == 6) /* XXX */
		memcpy (eh.ether_dhost, to -> haddr, sizeof eh.ether_dhost);
	else
		memset (eh.ether_dhost, 0xff, sizeof (eh.ether_dhost));
	if (interface -> hw_address.hlen == sizeof (eh.ether_shost))
		memcpy (eh.ether_shost, interface -> hw_address.haddr,
			sizeof (eh.ether_shost));
	else
		memset (eh.ether_shost, 0x00, sizeof (eh.ether_shost));

#ifdef BROKEN_FREEBSD_BPF /* Fixed in FreeBSD 2.2 */
	eh.ether_type = ETHERTYPE_IP;
#else
	eh.ether_type = htons (ETHERTYPE_IP);
#endif

	memcpy (&buf [*bufix], &eh, sizeof eh);
	*bufix += sizeof eh;
}

/* UDP header and IP header assembled together for convenience. */

void assemble_udp_ip_header (interface, buf, bufix,
			     from, to, port, data, len)
	struct interface_info *interface;
	unsigned char *buf;
	int *bufix;
	u_int32_t from;
	u_int32_t to;
	u_int16_t port;
	unsigned char *data;
	int len;
{
	struct ip ip;
	struct udphdr udp;

	/* Fill out the IP header */
	ip.ip_v = 4;
	ip.ip_hl = 5;
	ip.ip_tos = IPTOS_LOWDELAY;
	ip.ip_len = htons(sizeof(ip) + sizeof(udp) + len);
	ip.ip_id = 0;
	ip.ip_off = 0;
	ip.ip_ttl = 16;
	ip.ip_p = IPPROTO_UDP;
	ip.ip_sum = 0;
	ip.ip_src.s_addr = from;
	ip.ip_dst.s_addr = to;
	
	/* Checksum the IP header... */
	ip.ip_sum = wrapsum (checksum ((unsigned char *)&ip, sizeof ip, 0));
	
	/* Copy the ip header into the buffer... */
	memcpy (&buf [*bufix], &ip, sizeof ip);
	*bufix += sizeof ip;

	/* Fill out the UDP header */
	udp.uh_sport = server_port;		/* XXX */
	udp.uh_dport = port;			/* XXX */
	udp.uh_ulen = htons(sizeof(udp) + len);
	memset (&udp.uh_sum, 0, sizeof udp.uh_sum);

	/* Compute UDP checksums, including the ``pseudo-header'', the UDP
	   header and the data. */

#if 0
	udp.uh_sum =
		wrapsum (checksum ((unsigned char *)&udp, sizeof udp,
				   checksum (data, len, 
					     checksum ((unsigned char *)
						       &ip.ip_src,
						       sizeof ip.ip_src,
						       IPPROTO_UDP +
						       (u_int32_t)
						       ntohs (udp.uh_ulen)))));
#endif

	/* Copy the udp header into the buffer... */
	memcpy (&buf [*bufix], &udp, sizeof udp);
	*bufix += sizeof udp;
}
#endif /* PACKET_ASSEMBLY */

#ifdef PACKET_DECODING
/* Decode a hardware header... */
/* XXX currently only supports ethernet; doesn't check for other types. */

size_t decode_hw_header (interface, buf, bufix, from)
     struct interface_info *interface;
     unsigned char *buf;
     int bufix;
     struct hardware *from;
{
  struct ether_header eh;

  memcpy (&eh, buf + bufix, sizeof eh);

#ifdef USERLAND_FILTER
  if (ntohs (eh.ether_type) != ETHERTYPE_IP)
	  return -1;
#endif
  memcpy (from -> haddr, eh.ether_shost, sizeof (eh.ether_shost));
  from -> htype = ARPHRD_ETHER;
  from -> hlen = sizeof eh.ether_shost;

  return sizeof eh;
}

/* UDP header and IP header decoded together for convenience. */

size_t decode_udp_ip_header (interface, buf, bufix, from, data, len)
	struct interface_info *interface;
	unsigned char *buf;
	int bufix;
	struct sockaddr_in *from;
	unsigned char *data;
	int len;
{
  struct ip *ip;
  struct udphdr *udp;
  u_int32_t ip_len = (buf [bufix] & 0xf) << 2;
  u_int32_t sum, usum;

  ip = (struct ip *)(buf + bufix);
  udp = (struct udphdr *)(buf + bufix + ip_len);

#ifdef USERLAND_FILTER
  /* Is it a UDP packet? */
  if (ip -> ip_p != IPPROTO_UDP)
	  return -1;

  /* Is it to the port we're serving? */
  if (udp -> uh_dport != server_port)
	  return -1;
#endif /* USERLAND_FILTER */

  /* Check the IP header checksum - it should be zero. */
  if (wrapsum (checksum (buf + bufix, ip_len, 0))) {
	  note ("Bad IP checksum: %x",
		wrapsum (checksum (buf + bufix, sizeof *ip, 0)));
	  return -1;
  }

  /* Copy out the IP source address... */
  memcpy (&from -> sin_addr, &ip -> ip_src, 4);

  /* Compute UDP checksums, including the ``pseudo-header'', the UDP
     header and the data.   If the UDP checksum field is zero, we're
     not supposed to do a checksum. */

  if (!data) {
	  data = buf + bufix + ip_len + sizeof *udp;
	  len -= ip_len + sizeof *udp;
  }

#if 0
  usum = udp -> uh_sum;
  udp -> uh_sum = 0;

  sum = wrapsum (checksum ((unsigned char *)udp, sizeof *udp,
			   checksum (data, len,
				     checksum ((unsigned char *)
					       &ip -> ip_src,
					       sizeof ip -> ip_src,
					       IPPROTO_UDP +
					       (u_int32_t)
					       ntohs (udp -> uh_ulen)))));

  if (usum && usum != sum) {
	  note ("Bad udp checksum: %x %x", usum, sum);
	  return -1;
  }
#endif

  /* Copy out the port... */
  memcpy (&from -> sin_port, &udp -> uh_sport, sizeof udp -> uh_sport);

  return ip_len + sizeof *udp;
}
#endif /* PACKET_DECODING */
