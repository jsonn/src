/* dhcp.c

   DHCP Protocol engine. */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.   All rights reserved.
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
"$Id: dhcp.c,v 1.8.2.1 1999/04/09 20:09:59 mellon Exp $ Copyright (c) 1995, 1996, 1997, 1998, 1999 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

int outstanding_pings;

static char dhcp_message [256];

void dhcp (packet)
	struct packet *packet;
{
	if (!locate_network (packet) && packet -> packet_type != DHCPREQUEST)
		return;

	switch (packet -> packet_type) {
	      case DHCPDISCOVER:
		dhcpdiscover (packet);
		break;

	      case DHCPREQUEST:
		dhcprequest (packet);
		break;

	      case DHCPRELEASE:
		dhcprelease (packet);
		break;

	      case DHCPDECLINE:
		dhcpdecline (packet);
		break;

	      case DHCPINFORM:
		dhcpinform (packet);
		break;

	      default:
		break;
	}
}

void dhcpdiscover (packet)
	struct packet *packet;
{
	struct lease *lease = find_lease (packet, packet -> shared_network, 0);
	struct host_decl *hp;

	note ("DHCPDISCOVER from %s via %s",
	      print_hw_addr (packet -> raw -> htype,
			     packet -> raw -> hlen,
			     packet -> raw -> chaddr),
	      packet -> raw -> giaddr.s_addr
	      ? inet_ntoa (packet -> raw -> giaddr)
	      : packet -> interface -> name);

	/* Sourceless packets don't make sense here. */
	if (!packet -> shared_network) {
		note ("Packet from unknown subnet: %s",
		      inet_ntoa (packet -> raw -> giaddr));
		return;
	}

	/* If we didn't find a lease, try to allocate one... */
	if (!lease) {
		lease = packet -> shared_network -> last_lease;

		/* If there are no leases in that subnet that have
		   expired, we have nothing to offer this client. */
		if (!lease || lease -> ends > cur_time) {
			note ("no free leases on subnet %s",
			      packet -> shared_network -> name);
			return;
		}

		/* If we find an abandoned lease, take it, but print a
		   warning message, so that if it continues to lose,
		   the administrator will eventually investigate. */
		if (lease -> flags & ABANDONED_LEASE) {
			struct lease *lp;

			/* See if we can find an unabandoned lease first. */
			for (lp = lease; lp; lp = lp -> prev) {
				if (lp -> ends > cur_time)
					break;
				if (!lp -> flags & ABANDONED_LEASE) {
					lease = lp;
					break;
				}
			}

			/* If we can't find an unabandoned lease, reclaim the
			   abandoned lease. */
			if (lease -> flags & ABANDONED_LEASE) {
				warn ("Reclaiming abandoned IP address %s.",
				      piaddr (lease -> ip_addr));
				lease -> flags &= ~ABANDONED_LEASE;
			}
		}

		/* Try to find a host_decl that matches the client
		   identifier or hardware address on the packet, and
		   has no fixed IP address.   If there is one, hang
		   it off the lease so that its option definitions
		   can be used. */
		if (((packet -> options [DHO_DHCP_CLIENT_IDENTIFIER].len
		      != 0) &&
		     ((hp = find_hosts_by_uid
		       (packet -> options [DHO_DHCP_CLIENT_IDENTIFIER].data,
			packet -> options [DHO_DHCP_CLIENT_IDENTIFIER].len))
		      != (struct host_decl *)0)) ||
		    ((hp = find_hosts_by_haddr (packet -> raw -> htype,
						packet -> raw -> chaddr,
						packet -> raw -> hlen))
		     != (struct host_decl *)0)) {
			for (; hp; hp = hp -> n_ipaddr) {
				if (!hp -> fixed_addr) {
					lease -> host = hp;
					break;
				}
			}
		} else {
			lease -> host = (struct host_decl *)0;
		}
	}

	/* If this subnet won't boot unknown clients, ignore the
	   request. */
	if (!lease -> host &&
	    !lease -> subnet -> group -> boot_unknown_clients) {
		note ("Ignoring unknown client %s",
		      print_hw_addr (packet -> raw -> htype,
				     packet -> raw -> hlen,
				     packet -> raw -> chaddr));
	} else if (lease -> host &&
		    !lease -> host -> group -> allow_booting) {
		note ("Declining to boot client %s",
		      lease -> host -> name
		      ? lease -> host -> name
		      : print_hw_addr (packet -> raw -> htype,
				       packet -> raw -> hlen,
				       packet -> raw -> chaddr));
	} else
		ack_lease (packet, lease, DHCPOFFER, cur_time + 120);
}

void dhcprequest (packet)
	struct packet *packet;
{
	struct lease *lease;
	struct iaddr cip;
	struct subnet *subnet;
	int ours = 0;

	if (packet -> options [DHO_DHCP_REQUESTED_ADDRESS].len) {
		cip.len = 4;
		memcpy (cip.iabuf,
			packet -> options [DHO_DHCP_REQUESTED_ADDRESS].data,
			4);
	} else {
		cip.len = 4;
		memcpy (cip.iabuf, &packet -> raw -> ciaddr.s_addr, 4);
	}
	subnet = find_subnet (cip);

	/* Find the lease that matches the address requested by the
	   client. */

	if (subnet)
		lease = find_lease (packet, subnet -> shared_network, &ours);
	else
		lease = (struct lease *)0;

	note ("DHCPREQUEST for %s from %s via %s",
	      piaddr (cip),
	      print_hw_addr (packet -> raw -> htype,
			     packet -> raw -> hlen,
			     packet -> raw -> chaddr),
	      packet -> raw -> giaddr.s_addr
	      ? inet_ntoa (packet -> raw -> giaddr)
	      : packet -> interface -> name);

	/* If a client on a given network REQUESTs a lease on an
	   address on a different network, NAK it.  If the Requested
	   Address option was used, the protocol says that it must
	   have been broadcast, so we can trust the source network
	   information.

	   If ciaddr was specified and Requested Address was not, then
	   we really only know for sure what network a packet came from
	   if it came through a BOOTP gateway - if it came through an
	   IP router, we'll just have to assume that it's cool.

	   If we don't think we know where the packet came from, it
	   came through a gateway from an unknown network, so it's not
	   from a RENEWING client.  If we recognize the network it
	   *thinks* it's on, we can NAK it even though we don't
	   recognize the network it's *actually* on; otherwise we just
	   have to ignore it.

	   We don't currently try to take advantage of access to the
	   raw packet, because it's not available on all platforms.
	   So a packet that was unicast to us through a router from a
	   RENEWING client is going to look exactly like a packet that
	   was broadcast to us from an INIT-REBOOT client.

	   Since we can't tell the difference between these two kinds
	   of packets, if the packet appears to have come in off the
	   local wire, we have to treat it as if it's a RENEWING
	   client.  This means that we can't NAK a RENEWING client on
	   the local wire that has a bogus address.  The good news is
	   that we won't ACK it either, so it should revert to INIT
	   state and send us a DHCPDISCOVER, which we *can* work with.

	   Because we can't detect that a RENEWING client is on the
	   wrong wire, it's going to sit there trying to renew until
	   it gets to the REBIND state, when we *can* NAK it because
	   the packet will get to us through a BOOTP gateway.  We
	   shouldn't actually see DHCPREQUEST packets from RENEWING
	   clients on the wrong wire anyway, since their idea of their
	   local router will be wrong.  In any case, the protocol
	   doesn't really allow us to NAK a DHCPREQUEST from a
	   RENEWING client, so we can punt on this issue. */

	if (!packet -> shared_network ||
	    (packet -> raw -> ciaddr.s_addr &&
	     packet -> raw -> giaddr.s_addr) ||
	    packet -> options [DHO_DHCP_REQUESTED_ADDRESS].len) {
		
		/* If we don't know where it came from but we do know
		   where it claims to have come from, it didn't come
		   from there.   Fry it. */
		if (!packet -> shared_network) {
			if (subnet &&
			    subnet -> shared_network -> group -> authoritative)
			{
				nak_lease (packet, &cip);
				return;
			}
			/* Otherwise, ignore it. */
			return;
		}

		/* If we do know where it came from and it asked for an
		   address that is not on that shared network, nak it. */
		subnet = find_grouped_subnet (packet -> shared_network, cip);
		if (!subnet) {
			if (packet -> shared_network -> group -> authoritative)
				nak_lease (packet, &cip);
			return;
		}
	}

	/* If we found a lease for the client but it's not the one the
	   client asked for, don't send it - some other server probably
	   made the cut. */
	if (lease && !addr_eq (lease -> ip_addr, cip)) {
		/* If we found the address the client asked for, but
                   it wasn't what got picked, the lease belongs to us,
                   so we should NAK it. */
		if (ours)
			nak_lease (packet, &cip);
		return;
	}

	/* If the address the client asked for is ours, but it wasn't
           available for the client, NAK it. */
	if (!lease && ours) {
		nak_lease (packet, &cip);
		return;
	}

	/* If we're not allowed to serve this client anymore, don't. */
	if (lease &&
	    !lease -> host &&
	    !lease -> subnet -> group -> boot_unknown_clients) {
		note ("Ignoring unknown client %s",
		      print_hw_addr (packet -> raw -> htype,
				     packet -> raw -> hlen,
				     packet -> raw -> chaddr));
		return;
	} else if (lease && lease -> host &&
		   !lease -> host -> group -> allow_booting) {
		note ("Declining to renew client %s",
		      lease -> host -> name
		      ? lease -> host -> name
		      : print_hw_addr (packet -> raw -> htype,
				       packet -> raw -> hlen,
				       packet -> raw -> chaddr));
		return;
	}

	/* If we own the lease that the client is asking for,
	   and it's already been assigned to the client, ack it. */
	if (lease &&
	    ((lease -> uid_len && lease -> uid_len == 
	      packet -> options [DHO_DHCP_CLIENT_IDENTIFIER].len &&
	      !memcmp (packet -> options
		       [DHO_DHCP_CLIENT_IDENTIFIER].data,
		       lease -> uid, lease -> uid_len)) ||
	     (lease -> hardware_addr.hlen == packet -> raw -> hlen &&
	      lease -> hardware_addr.htype == packet -> raw -> htype &&
	      !memcmp (lease -> hardware_addr.haddr,
		       packet -> raw -> chaddr,
		       packet -> raw -> hlen)))) {
		ack_lease (packet, lease, DHCPACK, 0);
		return;
	}
}

void dhcprelease (packet)
	struct packet *packet;
{
	struct lease *lease;
	struct iaddr cip;
	int i;

	/* DHCPRELEASE must not specify address in requested-address
           option, but old protocol specs weren't explicit about this,
           so let it go. */
	if (packet -> options [DHO_DHCP_REQUESTED_ADDRESS].len) {
		note ("DHCPRELEASE from %s specified requested-address.",
		      print_hw_addr (packet -> raw -> htype,
				     packet -> raw -> hlen,
				     packet -> raw -> chaddr));
	}

	i = DHO_DHCP_CLIENT_IDENTIFIER;
	if (packet -> options [i].len) {
		lease = find_lease_by_uid (packet -> options [i].data,
					   packet -> options [i].len);
	} else
		lease = (struct lease *)0;

	/* The client is supposed to pass a valid client-identifier,
	   but the spec on this has changed historically, so try the
	   IP address in ciaddr if the client-identifier fails. */
	if (!lease) {
		cip.len = 4;
		memcpy (cip.iabuf, &packet -> raw -> ciaddr, 4);
		lease = find_lease_by_ip_addr (cip);
	}


	note ("DHCPRELEASE of %s from %s via %s (%sfound)",
	      inet_ntoa (packet -> raw -> ciaddr),
	      print_hw_addr (packet -> raw -> htype,
			     packet -> raw -> hlen,
			     packet -> raw -> chaddr),
	      packet -> raw -> giaddr.s_addr
	      ? inet_ntoa (packet -> raw -> giaddr)
	      : packet -> interface -> name,
	      lease ? "" : "not ");

	/* If we found a lease, release it. */
	if (lease) {
		release_lease (lease);
	}
}

void dhcpdecline (packet)
	struct packet *packet;
{
	struct lease *lease;
	struct iaddr cip;

	/* DHCPDECLINE must specify address. */
	if (!packet -> options [DHO_DHCP_REQUESTED_ADDRESS].len) {
		return;
	}

	cip.len = 4;
	memcpy (cip.iabuf,
		packet -> options [DHO_DHCP_REQUESTED_ADDRESS].data, 4);
	lease = find_lease_by_ip_addr (cip);

	note ("DHCPDECLINE on %s from %s via %s",
	      piaddr (cip),
	      print_hw_addr (packet -> raw -> htype,
			     packet -> raw -> hlen,
			     packet -> raw -> chaddr),
	      packet -> raw -> giaddr.s_addr
	      ? inet_ntoa (packet -> raw -> giaddr)
	      : packet -> interface -> name);

	/* If we found a lease, mark it as unusable and complain. */
	if (lease) {
		abandon_lease (lease, "declined.");
	}
}

void dhcpinform (packet)
	struct packet *packet;
{
	note ("DHCPINFORM from %s",
	      inet_ntoa (packet -> raw -> ciaddr));
}

void nak_lease (packet, cip)
	struct packet *packet;
	struct iaddr *cip;
{
	struct sockaddr_in to;
	struct in_addr from;
	int result;
	struct dhcp_packet raw;
	unsigned char nak = DHCPNAK;
	struct packet outgoing;
	struct hardware hto;
	int i;

	struct tree_cache *options [256];
	struct tree_cache dhcpnak_tree;
	struct tree_cache dhcpmsg_tree;

	memset (options, 0, sizeof options);
	memset (&outgoing, 0, sizeof outgoing);
	memset (&raw, 0, sizeof raw);
	outgoing.raw = &raw;

	/* Set DHCP_MESSAGE_TYPE to DHCPNAK */
	options [DHO_DHCP_MESSAGE_TYPE] = &dhcpnak_tree;
	options [DHO_DHCP_MESSAGE_TYPE] -> value = &nak;
	options [DHO_DHCP_MESSAGE_TYPE] -> len = sizeof nak;
	options [DHO_DHCP_MESSAGE_TYPE] -> buf_size = sizeof nak;
	options [DHO_DHCP_MESSAGE_TYPE] -> timeout = 0xFFFFFFFF;
	options [DHO_DHCP_MESSAGE_TYPE] -> tree = (struct tree *)0;

	/* Set DHCP_MESSAGE to whatever the message is */
	options [DHO_DHCP_MESSAGE] = &dhcpmsg_tree;
	options [DHO_DHCP_MESSAGE] -> value = (unsigned char *)dhcp_message;
	options [DHO_DHCP_MESSAGE] -> len =
		options [DHO_DHCP_MESSAGE] -> buf_size = strlen (dhcp_message);
	options [DHO_DHCP_MESSAGE] -> timeout = 0xFFFFFFFF;
	options [DHO_DHCP_MESSAGE] -> tree = (struct tree *)0;

	/* Do not use the client's requested parameter list. */
	i = DHO_DHCP_PARAMETER_REQUEST_LIST;
	if (packet -> options [i].data) {
		packet -> options [i].len = 0;
		dfree (packet -> options [i].data, "nak_lease");
		packet -> options [i].data = (unsigned char *)0;
	}

	/* Set up the option buffer... */
	outgoing.packet_length =
		cons_options (packet, outgoing.raw, 0, options, 0, 0, 0,
			      (u_int8_t *)0, 0);

/*	memset (&raw.ciaddr, 0, sizeof raw.ciaddr);*/
	raw.siaddr = packet -> interface -> primary_address;
	raw.giaddr = packet -> raw -> giaddr;
	memcpy (raw.chaddr, packet -> raw -> chaddr, sizeof raw.chaddr);
	raw.hlen = packet -> raw -> hlen;
	raw.htype = packet -> raw -> htype;

	raw.xid = packet -> raw -> xid;
	raw.secs = packet -> raw -> secs;
	raw.flags = packet -> raw -> flags | htons (BOOTP_BROADCAST);
	raw.hops = packet -> raw -> hops;
	raw.op = BOOTREPLY;

	/* Report what we're sending... */
	note ("DHCPNAK on %s to %s via %s",
	      piaddr (*cip),
	      print_hw_addr (packet -> raw -> htype,
			     packet -> raw -> hlen,
			     packet -> raw -> chaddr),
	      packet -> raw -> giaddr.s_addr
	      ? inet_ntoa (packet -> raw -> giaddr)
	      : packet -> interface -> name);



#ifdef DEBUG_PACKET
	dump_packet (packet);
	dump_raw ((unsigned char *)packet -> raw, packet -> packet_length);
	dump_packet (&outgoing);
	dump_raw ((unsigned char *)&raw, outgoing.packet_length);
#endif

	hto.htype = packet -> raw -> htype;
	hto.hlen = packet -> raw -> hlen;
	memcpy (hto.haddr, packet -> raw -> chaddr, hto.hlen);

	/* Set up the common stuff... */
	to.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	to.sin_len = sizeof to;
#endif
	memset (to.sin_zero, 0, sizeof to.sin_zero);

	from = packet -> interface -> primary_address;

	/* Make sure that the packet is at least as big as a BOOTP packet. */
	if (outgoing.packet_length < BOOTP_MIN_LEN)
		outgoing.packet_length = BOOTP_MIN_LEN;

	/* If this was gatewayed, send it back to the gateway.
	   Otherwise, broadcast it on the local network. */
	if (raw.giaddr.s_addr) {
		to.sin_addr = raw.giaddr;
		to.sin_port = local_port;

		if (fallback_interface) {
			result = send_packet (fallback_interface,
					      packet, &raw,
					      outgoing.packet_length,
					      from, &to, &hto);
			if (result < 0)
				warn ("send_fallback: %m");
			return;
		}
	} else {
		to.sin_addr.s_addr = htonl (INADDR_BROADCAST);
		to.sin_port = remote_port;
	}

	errno = 0;
	result = send_packet (packet -> interface,
			      packet, &raw, outgoing.packet_length,
			      from, &to, (struct hardware *)0);
}

void ack_lease (packet, lease, offer, when)
	struct packet *packet;
	struct lease *lease;
	unsigned int offer;
	TIME when;
{
	struct lease lt;
	struct lease_state *state;
	TIME lease_time;
	TIME offered_lease_time;
	TIME max_lease_time;
	TIME default_lease_time;
	int ulafdr;

	struct class *vendor_class, *user_class;
	int i;

	/* If we're already acking this lease, don't do it again. */
	if (lease -> state) {
		note ("already acking lease %s", piaddr (lease -> ip_addr));
		return;
	}

	if (packet -> options [DHO_DHCP_CLASS_IDENTIFIER].len) {
		vendor_class =
			find_class (0,
				    packet ->
				    options [DHO_DHCP_CLASS_IDENTIFIER].data,
				    packet ->
				    options [DHO_DHCP_CLASS_IDENTIFIER].len);
	} else {
		vendor_class = (struct class *)0;
	}

	if (packet -> options [DHO_DHCP_USER_CLASS_ID].len) {
		user_class =
			find_class (1,
				    packet ->
				    options [DHO_DHCP_USER_CLASS_ID].data,
				    packet ->
				    options [DHO_DHCP_USER_CLASS_ID].len);
	} else {
		user_class = (struct class *)0;
	}

	/*
	 * If there is not a specific host entry, and either the
	 * vendor class or user class (if they exist) deny booting,
	 * then bug out.
	 */
	if (!lease -> host) {
		if (vendor_class && !vendor_class -> group -> allow_booting) {
			debug ("Booting denied by vendor class");
			return;
		}

		if (user_class && !user_class -> group -> allow_booting) {
			debug ("Booting denied by user class");
			return;
		}
	}

	/* Allocate a lease state structure... */
	state = new_lease_state ("ack_lease");
	if (!state)
		error ("unable to allocate lease state!");
	memset (state, 0, sizeof *state);

	/* Replace the old lease hostname with the new one, if it's changed. */
	if (packet -> options [DHO_HOST_NAME].len &&
	    lease -> client_hostname &&
	    (strlen (lease -> client_hostname) ==
	     packet -> options [DHO_HOST_NAME].len) &&
	    !memcmp (lease -> client_hostname,
		     packet -> options [DHO_HOST_NAME].data,
		     packet -> options [DHO_HOST_NAME].len)) {
	} else if (packet -> options [DHO_HOST_NAME].len) {
		if (lease -> client_hostname)
			free (lease -> client_hostname);
		lease -> client_hostname =
			malloc (packet -> options [DHO_HOST_NAME].len + 1);
		if (!lease -> client_hostname)
			error ("no memory for client hostname.\n");
		memcpy (lease -> client_hostname,
			packet -> options [DHO_HOST_NAME].data,
			packet -> options [DHO_HOST_NAME].len);
		lease -> client_hostname
			[packet -> options [DHO_HOST_NAME].len] = 0;
	} else if (lease -> client_hostname) {
		free (lease -> client_hostname);
		lease -> client_hostname = 0;
	}

	/* Choose a filename; first from the host_decl, if any, then from
	   the user class, then from the vendor class. */
	if (lease -> host && lease -> host -> group -> filename)
		strncpy (state -> filename, lease -> host -> group -> filename,
			 sizeof state -> filename);
	else if (user_class && user_class -> group -> filename)
		strncpy (state -> filename, user_class -> group -> filename,
			 sizeof state -> filename);
	else if (vendor_class  && vendor_class -> group -> filename)
		strncpy (state -> filename, vendor_class -> group -> filename,
			 sizeof state -> filename);
	else if (packet -> raw -> file [0])
		strncpy (state -> filename, packet -> raw -> file,
			 sizeof state -> filename);
	else if (lease -> subnet -> group -> filename)
		strncpy (state -> filename,
			 lease -> subnet -> group -> filename,
			 sizeof state -> filename);
	else
		strcpy (state -> filename, "");

	/* Choose a server name as above. */
	if (lease -> host && lease -> host -> group -> server_name)
		state -> server_name = lease -> host -> group -> server_name;
	else if (user_class && user_class -> group -> server_name)
		state -> server_name = user_class -> group -> server_name;
	else if (vendor_class  && vendor_class -> group -> server_name)
		state -> server_name = vendor_class -> group -> server_name;
	else if (lease -> subnet -> group -> server_name)
		state -> server_name =
			lease -> subnet -> group -> server_name;
	else state -> server_name = (char *)0;

	/* At this point, we have a lease that we can offer the client.
	   Now we construct a lease structure that contains what we want,
	   and call supersede_lease to do the right thing with it. */

	memset (&lt, 0, sizeof lt);

	/* Use the ip address of the lease that we finally found in
	   the database. */
	lt.ip_addr = lease -> ip_addr;

	/* Start now. */
	lt.starts = cur_time;

	/* Figure out maximum lease time. */
	if (lease -> host &&
	    lease -> host -> group -> max_lease_time)
		max_lease_time = lease -> host -> group -> max_lease_time;
	else
		max_lease_time = lease -> subnet -> group -> max_lease_time;

	/* Figure out default lease time. */
	if (lease -> host
	    && lease -> host -> group -> default_lease_time)
		default_lease_time =
			lease -> host -> group -> default_lease_time;
	else
		default_lease_time =
			lease -> subnet -> group -> default_lease_time;

	/* Figure out how long a lease to assign.    If this is a
	   dynamic BOOTP lease, its duration must be infinite. */
	if (offer) {
		if (packet -> options [DHO_DHCP_LEASE_TIME].len == 4) {
			lease_time = getULong
				(packet -> options [DHO_DHCP_LEASE_TIME].data);
			
			/* Don't let the client ask for a longer lease than
			   is supported for this subnet or host. */
			if (lease_time > max_lease_time)
				lease_time = max_lease_time;
		} else
			lease_time = default_lease_time;
		
		state -> offered_expiry = cur_time + lease_time;
		if (when)
			lt.ends = when;
		else
			lt.ends = state -> offered_expiry;
	} else {
		if (lease -> host &&
		    lease -> host -> group -> bootp_lease_length)
			lt.ends = (cur_time +
				   lease -> host ->
				   group -> bootp_lease_length);
		else if (lease -> subnet -> group -> bootp_lease_length)
			lt.ends = (cur_time +
				   lease -> subnet ->
				   group -> bootp_lease_length);
		else if (lease -> host &&
			 lease -> host -> group -> bootp_lease_cutoff)
			lt.ends = lease -> host -> group -> bootp_lease_cutoff;
		else
			lt.ends = (lease -> subnet ->
				   group -> bootp_lease_cutoff);
		state -> offered_expiry = lt.ends;
		lt.flags = BOOTP_LEASE;
	}

	/* Record the uid, if given... */
	i = DHO_DHCP_CLIENT_IDENTIFIER;
	if (packet -> options [i].len) {
		if (packet -> options [i].len <= sizeof lt.uid_buf) {
			memcpy (lt.uid_buf, packet -> options [i].data,
				packet -> options [i].len);
			lt.uid = lt.uid_buf;
			lt.uid_max = sizeof lt.uid_buf;
			lt.uid_len = packet -> options [i].len;
		} else {
			lt.uid_max = lt.uid_len = packet -> options [i].len;
			lt.uid = (unsigned char *)malloc (lt.uid_max);
			if (!lt.uid)
				error ("can't allocate memory for large uid.");
			memcpy (lt.uid,
				packet -> options [i].data, lt.uid_len);
		}
	}

	lt.host = lease -> host;
	lt.subnet = lease -> subnet;
	lt.shared_network = lease -> shared_network;

	/* Don't call supersede_lease on a mocked-up lease. */
	if (lease -> flags & STATIC_LEASE) {
		/* Copy the hardware address into the static lease
		   structure. */
		lease -> hardware_addr.hlen = packet -> raw -> hlen;
		lease -> hardware_addr.htype = packet -> raw -> htype;
		memcpy (lease -> hardware_addr.haddr, packet -> raw -> chaddr,
			sizeof packet -> raw -> chaddr); /* XXX */
	} else {
		/* Record the hardware address, if given... */
		lt.hardware_addr.hlen = packet -> raw -> hlen;
		lt.hardware_addr.htype = packet -> raw -> htype;
		memcpy (lt.hardware_addr.haddr, packet -> raw -> chaddr,
			sizeof packet -> raw -> chaddr);

		/* Install the new information about this lease in the
		   database.  If this is a DHCPACK or a dynamic BOOTREPLY
		   and we can't write the lease, don't ACK it (or BOOTREPLY
		   it) either. */

		if (!(supersede_lease (lease, &lt, !offer || offer == DHCPACK)
		      || (offer && offer != DHCPACK)))
			return;
	}

	/* Remember the interface on which the packet arrived. */
	state -> ip = packet -> interface;

	/* Set a flag if this client is a lame Microsoft client that NUL
	   terminates string options and expects us to do likewise. */
	if (packet -> options [DHO_HOST_NAME].data &&
	    packet -> options [DHO_HOST_NAME].data
	    [packet -> options [DHO_HOST_NAME].len - 1] == '\0')
		lease -> flags |= MS_NULL_TERMINATION;
	else
		lease -> flags &= ~MS_NULL_TERMINATION;

	/* Remember the giaddr, xid, secs, flags and hops. */
	state -> giaddr = packet -> raw -> giaddr;
	state -> ciaddr = packet -> raw -> ciaddr;
	state -> xid = packet -> raw -> xid;
	state -> secs = packet -> raw -> secs;
	state -> bootp_flags = packet -> raw -> flags;
	state -> hops = packet -> raw -> hops;
	state -> offer = offer;

	/* Get the Maximum Message Size option from the packet, if one
	   was sent. */
	if (packet -> options [DHO_DHCP_MAX_MESSAGE_SIZE].data &&
	    (packet -> options [DHO_DHCP_MAX_MESSAGE_SIZE].len >=
	     sizeof (u_int16_t)))
		state -> max_message_size =
			getUShort (packet -> options
				   [DHO_DHCP_MAX_MESSAGE_SIZE].data);

	/* Save the parameter request list if there is one. */
	i = DHO_DHCP_PARAMETER_REQUEST_LIST;
	if (packet -> options [i].data) {
		state -> prl = dmalloc (packet -> options [i].len,
					"ack_lease: prl");
		if (!state -> prl)
			warn ("no memory for parameter request list");
		else {
			memcpy (state -> prl,
				packet -> options [i].data,
				packet -> options [i].len);
			state -> prl_len = packet -> options [i].len;
		}
	}

	/* Figure out what options to send to the client: */

	/* Start out with the subnet options... */
	memcpy (state -> options,
		lease -> subnet -> group -> options,
		sizeof state -> options);

	/* Vendor and user classes are only supported for DHCP clients. */
	if (state -> offer) {
		/* If we have a vendor class, install those options,
		   superseding any subnet options. */
		if (vendor_class) {
			for (i = 0; i < 256; i++)
				if (vendor_class -> group -> options [i])
					state -> options [i] =
						(vendor_class -> group ->
						 options [i]);
		}

		/* If we have a user class, install those options,
		   superseding any subnet and vendor class options. */
		if (user_class) {
			for (i = 0; i < 256; i++)
				if (user_class -> group -> options [i])
					state -> options [i] =
						(user_class -> group ->
						 options [i]);
		}

	}

	/* If we have a host_decl structure, install the associated
	   options, superseding anything that's in the way. */
	if (lease -> host) {
		for (i = 0; i < 256; i++)
			if (lease -> host -> group -> options [i])
				state -> options [i] = (lease -> host ->
							group -> options [i]);
	}

	/* If we didn't get a hostname from an option somewhere, see if
	   we can get one from the lease. */
	i = DHO_HOST_NAME;
	if (!state -> options [i] && lease -> hostname) {
		state -> options [i] = new_tree_cache ("hostname");
		state -> options [i] -> flags = TC_TEMPORARY;
		state -> options [i] -> value =
			(unsigned char *)lease -> hostname;
		state -> options [i] -> len = strlen (lease -> hostname);
		state -> options [i] -> buf_size = state -> options [i] -> len;
		state -> options [i] -> timeout = 0xFFFFFFFF;
		state -> options [i] -> tree = (struct tree *)0;
	}

	/* Now, if appropriate, put in DHCP-specific options that
           override those. */
	if (state -> offer) {
		i = DHO_DHCP_MESSAGE_TYPE;
		state -> options [i] = new_tree_cache ("message-type");
		state -> options [i] -> flags = TC_TEMPORARY;
		state -> options [i] -> value = &state -> offer;
		state -> options [i] -> len = sizeof state -> offer;
		state -> options [i] -> buf_size = sizeof state -> offer;
		state -> options [i] -> timeout = 0xFFFFFFFF;
		state -> options [i] -> tree = (struct tree *)0;

		i = DHO_DHCP_SERVER_IDENTIFIER;
		if (!state -> options [i]) {
		 use_primary:
			state -> options [i] = new_tree_cache ("server-id");
			state -> options [i] -> value =
				(unsigned char *)&state ->
					ip -> primary_address;
			state -> options [i] -> len =
				sizeof state -> ip -> primary_address;
			state -> options [i] -> buf_size
				= state -> options [i] -> len;
			state -> options [i] -> timeout = 0xFFFFFFFF;
			state -> options [i] -> tree = (struct tree *)0;
			state -> from.len =
				sizeof state -> ip -> primary_address;
			memcpy (state -> from.iabuf,
				&state -> ip -> primary_address,
				state -> from.len);
		} else {
			/* Find the value of the server identifier... */
			if (!tree_evaluate (state -> options [i]))
				goto use_primary;
			if (!state -> options [i] -> value ||
			    (state -> options [i] -> len >
			     sizeof state -> from.iabuf))
				goto use_primary;
			
			state -> from.len = state -> options [i] -> len;
			memcpy (state -> from.iabuf,
				state -> options [i] -> value,
				state -> from.len);
		}

		/* Sanity check the lease time. */
		if ((state -> offered_expiry - cur_time) < 15)
			offered_lease_time = default_lease_time;
		else if (state -> offered_expiry - cur_time > max_lease_time)
			offered_lease_time = max_lease_time;
		else 
			offered_lease_time =
				state -> offered_expiry - cur_time;

		putULong ((unsigned char *)&state -> expiry,
			  offered_lease_time);
		i = DHO_DHCP_LEASE_TIME;
		state -> options [i] = new_tree_cache ("lease-expiry");
		state -> options [i] -> flags = TC_TEMPORARY;
		state -> options [i] -> value =
			(unsigned char *)&state -> expiry;
		state -> options [i] -> len = sizeof state -> expiry;
		state -> options [i] -> buf_size = sizeof state -> expiry;
		state -> options [i] -> timeout = 0xFFFFFFFF;
		state -> options [i] -> tree = (struct tree *)0;

		/* Renewal time is lease time * 0.5. */
		offered_lease_time /= 2;
		putULong ((unsigned char *)&state -> renewal,
			  offered_lease_time);
		i = DHO_DHCP_RENEWAL_TIME;
		state -> options [i] = new_tree_cache ("renewal-time");
		state -> options [i] -> flags = TC_TEMPORARY;
		state -> options [i] -> value =
			(unsigned char *)&state -> renewal;
		state -> options [i] -> len = sizeof state -> renewal;
		state -> options [i] -> buf_size = sizeof state -> renewal;
		state -> options [i] -> timeout = 0xFFFFFFFF;
		state -> options [i] -> tree = (struct tree *)0;


		/* Rebinding time is lease time * 0.875. */
		offered_lease_time += (offered_lease_time / 2
				       + offered_lease_time / 4);
		putULong ((unsigned char *)&state -> rebind,
			  offered_lease_time);
		i = DHO_DHCP_REBINDING_TIME;
		state -> options [i] = new_tree_cache ("rebind-time");
		state -> options [i] -> flags = TC_TEMPORARY;
		state -> options [i] -> value =
			(unsigned char *)&state -> rebind;
		state -> options [i] -> len = sizeof state -> rebind;
		state -> options [i] -> buf_size = sizeof state -> rebind;
		state -> options [i] -> timeout = 0xFFFFFFFF;
		state -> options [i] -> tree = (struct tree *)0;

		/* If we used the vendor class the client specified, we
		   have to return it. */
		if (vendor_class) {
			i = DHO_DHCP_CLASS_IDENTIFIER;
			state -> options [i] =
				new_tree_cache ("class-identifier");
			state -> options [i] -> flags = TC_TEMPORARY;
			state -> options [i] -> value =
				(unsigned char *)vendor_class -> name;
			state -> options [i] -> len =
				strlen (vendor_class -> name);
			state -> options [i] -> buf_size =
				state -> options [i] -> len;
			state -> options [i] -> timeout = 0xFFFFFFFF;
			state -> options [i] -> tree = (struct tree *)0;
		}

		/* If we used the user class the client specified, we
		   have to return it. */
		if (user_class) {
			i = DHO_DHCP_USER_CLASS_ID;
			state -> options [i] = new_tree_cache ("user-class");
			state -> options [i] -> flags = TC_TEMPORARY;
			state -> options [i] -> value =
				(unsigned char *)user_class -> name;
			state -> options [i] -> len =
				strlen (user_class -> name);
			state -> options [i] -> buf_size =
				state -> options [i] -> len;
			state -> options [i] -> timeout = 0xFFFFFFFF;
			state -> options [i] -> tree = (struct tree *)0;
		}
	}

	/* Use the subnet mask from the subnet declaration if no other
	   mask has been provided. */
	i = DHO_SUBNET_MASK;
	if (!state -> options [i]) {
		state -> options [i] = new_tree_cache ("subnet-mask");
		state -> options [i] -> flags = TC_TEMPORARY;
		state -> options [i] -> value =
			lease -> subnet -> netmask.iabuf;
		state -> options [i] -> len = lease -> subnet -> netmask.len;
		state -> options [i] -> buf_size =
			lease -> subnet -> netmask.len;
		state -> options [i] -> timeout = 0xFFFFFFFF;
		state -> options [i] -> tree = (struct tree *)0;
	}

	/* If so directed, use the leased IP address as the router address.
	   This supposedly makes Win95 machines ARP for all IP addresses,
	   so if the local router does proxy arp, you win. */

	ulafdr = 0;
	if (lease -> host) {
		if (lease -> host -> group -> use_lease_addr_for_default_route)
			ulafdr = 1;
	} else if (user_class) {
		if (user_class -> group -> use_lease_addr_for_default_route)
			ulafdr = 1;
	} else if (vendor_class) {
		if (vendor_class -> group -> use_lease_addr_for_default_route)
			ulafdr = 1;
	} else if (lease -> subnet -> group ->
		   use_lease_addr_for_default_route)
		ulafdr = 1;
	else
		ulafdr = 0;

	i = DHO_ROUTERS;
	if (ulafdr && !state -> options [i]) {
		state -> options [i] = new_tree_cache ("routers");
		state -> options [i] -> flags = TC_TEMPORARY;
		state -> options [i] -> value =
			lease -> ip_addr.iabuf;
		state -> options [i] -> len =
			lease -> ip_addr.len;
		state -> options [i] -> buf_size =
			lease -> ip_addr.len;
		state -> options [i] -> timeout = 0xFFFFFFFF;
		state -> options [i] -> tree = (struct tree *)0;
	}

#ifdef DEBUG_PACKET
	dump_packet (packet);
	dump_raw ((unsigned char *)packet -> raw, packet -> packet_length);
#endif

	lease -> state = state;

	/* If this is a DHCPOFFER, ping the lease address before actually
	   sending the offer. */
	if (offer == DHCPOFFER && !(lease -> flags & STATIC_LEASE) &&
	    cur_time - lease -> timestamp > 60) {
		lease -> timestamp = cur_time;
		icmp_echorequest (&lease -> ip_addr);
		add_timeout (cur_time + 1, lease_ping_timeout, lease);
		++outstanding_pings;
	} else {
		lease -> timestamp = cur_time;
		dhcp_reply (lease);
	}
}

void dhcp_reply (lease)
	struct lease *lease;
{
	int bufs = 0;
	int packet_length;
	struct dhcp_packet raw;
	struct sockaddr_in to;
	struct in_addr from;
	struct hardware hto;
	int result;
	int i;
	struct lease_state *state = lease -> state;
	int nulltp, bootpp;

	if (!state)
		error ("dhcp_reply was supplied lease with no state!");

	/* Compose a response for the client... */
	memset (&raw, 0, sizeof raw);

	/* Copy in the filename if given; otherwise, flag the filename
	   buffer as available for options. */
	if (state -> filename [0])
		strncpy (raw.file, state -> filename, sizeof raw.file);
	else
		bufs |= 1;

	/* Copy in the server name if given; otherwise, flag the
	   server_name buffer as available for options. */
	if (state -> server_name)
		strncpy (raw.sname, state -> server_name, sizeof raw.sname);
	else
		bufs |= 2; /* XXX */

	memcpy (raw.chaddr, lease -> hardware_addr.haddr, sizeof raw.chaddr);
	raw.hlen = lease -> hardware_addr.hlen;
	raw.htype = lease -> hardware_addr.htype;

	/* See if this is a Microsoft client that NUL-terminates its
	   strings and expects us to do likewise... */
	if (lease -> flags & MS_NULL_TERMINATION)
		nulltp = 1;
	else
		nulltp = 0;

	/* See if this is a bootp client... */
	if (state -> offer)
		bootpp = 0;
	else
		bootpp = 1;

	/* Insert such options as will fit into the buffer. */
	packet_length = cons_options ((struct packet *)0, &raw,
				      state -> max_message_size,
				      state -> options, bufs, nulltp, bootpp,
				      state -> prl, state -> prl_len);

	/* Having done the cons_options(), we can release the tree_cache
	   entries. */
	for (i = 0; i < 256; i++) {
		if (state -> options [i] &&
		    state -> options [i] -> flags & TC_TEMPORARY)
			free_tree_cache (state -> options [i], "dhcp_reply");
	}

	memcpy (&raw.ciaddr, &state -> ciaddr, sizeof raw.ciaddr);
	memcpy (&raw.yiaddr, lease -> ip_addr.iabuf, 4);

	/* Figure out the address of the next server. */
	if (lease -> host && lease -> host -> group -> next_server.len)
		memcpy (&raw.siaddr,
			lease -> host -> group -> next_server.iabuf, 4);
	else if (lease -> subnet -> group -> next_server.len)
		memcpy (&raw.siaddr,
			lease -> subnet -> group -> next_server.iabuf, 4);
	else if (lease -> subnet -> interface_address.len)
		memcpy (&raw.siaddr,
			lease -> subnet -> interface_address.iabuf, 4);
	else
		raw.siaddr = state -> ip -> primary_address;

	raw.giaddr = state -> giaddr;

	raw.xid = state -> xid;
	raw.secs = state -> secs;
	raw.flags = state -> bootp_flags;
	raw.hops = state -> hops;
	raw.op = BOOTREPLY;

	/* Say what we're doing... */
	note ("%s on %s to %s via %s",
	      (state -> offer
	       ? (state -> offer == DHCPACK ? "DHCPACK" : "DHCPOFFER")
	       : "BOOTREPLY"),
	      piaddr (lease -> ip_addr),
	      print_hw_addr (lease -> hardware_addr.htype,
			     lease -> hardware_addr.hlen,
			     lease -> hardware_addr.haddr),
	      state -> giaddr.s_addr
	      ? inet_ntoa (state -> giaddr)
	      : state -> ip -> name);

	/* Set up the hardware address... */
	hto.htype = lease -> hardware_addr.htype;
	hto.hlen = lease -> hardware_addr.hlen;
	memcpy (hto.haddr, lease -> hardware_addr.haddr, hto.hlen);

	to.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	to.sin_len = sizeof to;
#endif
	memset (to.sin_zero, 0, sizeof to.sin_zero);

#ifdef DEBUG_PACKET
	dump_raw ((unsigned char *)&raw, packet_length);
#endif

	/* Make sure outgoing packets are at least as big
	   as a BOOTP packet. */
	if (packet_length < BOOTP_MIN_LEN)
		packet_length = BOOTP_MIN_LEN;

	/* If this was gatewayed, send it back to the gateway... */
	if (raw.giaddr.s_addr) {
		to.sin_addr = raw.giaddr;
		to.sin_port = local_port;

		if (fallback_interface) {
			result = send_packet (fallback_interface,
					      (struct packet *)0,
					      &raw, packet_length,
					      raw.siaddr,
					      &to, (struct hardware *)0);

			free_lease_state (state, "dhcp_reply fallback 1");
			lease -> state = (struct lease_state *)0;
			return;
		}

	/* If the client is RENEWING, unicast to the client using the
	   regular IP stack. */
	} else if (raw.ciaddr.s_addr && state -> offer == DHCPACK) {
		to.sin_addr = raw.ciaddr;
		to.sin_port = remote_port;

		if (fallback_interface) {
			result = send_packet (fallback_interface,
					      (struct packet *)0,
					      &raw, packet_length,
					      raw.siaddr, &to,
					      (struct hardware *)0);
			free_lease_state (state,
					  "dhcp_reply fallback 2");
			lease -> state = (struct lease_state *)0;
			return;
		}

	/* If it comes from a client that already knows its address
	   and is not requesting a broadcast response, and we can
	   unicast to a client without using the ARP protocol, sent it
	   directly to that client. */
	} else if (!(raw.flags & htons (BOOTP_BROADCAST)) &&
		   can_unicast_without_arp ()) {
		to.sin_addr = raw.yiaddr;
		to.sin_port = remote_port;

	/* Otherwise, broadcast it on the local network. */
	} else {
		to.sin_addr.s_addr = htonl (INADDR_BROADCAST);
		to.sin_port = remote_port;
	}

	memcpy (&from, state -> from.iabuf, sizeof from);

	result = send_packet (state -> ip,
			      (struct packet *)0, &raw, packet_length,
			      from, &to, &hto);

	free_lease_state (state, "dhcp_reply");
	lease -> state = (struct lease_state *)0;
}

struct lease *find_lease (packet, share, ours)
	struct packet *packet;
	struct shared_network *share;
	int *ours;
{
	struct lease *uid_lease, *ip_lease, *hw_lease;
	struct lease *lease = (struct lease *)0;
	struct iaddr cip;
	struct host_decl *hp, *host = (struct host_decl *)0;
	struct lease *fixed_lease;

	/* Figure out what IP address the client is requesting, if any. */
	if (packet -> options [DHO_DHCP_REQUESTED_ADDRESS].len &&
	    packet -> options [DHO_DHCP_REQUESTED_ADDRESS].len == 4) {
		cip.len = 4;
		memcpy (cip.iabuf,
			packet -> options [DHO_DHCP_REQUESTED_ADDRESS].data,
			cip.len);
	} else if (packet -> raw -> ciaddr.s_addr) {
		cip.len = 4;
		memcpy (cip.iabuf, &packet -> raw -> ciaddr, 4);
	} else
		cip.len = 0;

	/* Try to find a host or lease that's been assigned to the
	   specified unique client identifier. */
	if (packet -> options [DHO_DHCP_CLIENT_IDENTIFIER].len) {
		/* First, try to find a fixed host entry for the specified
		   client identifier... */
		hp = find_hosts_by_uid (packet -> options
					[DHO_DHCP_CLIENT_IDENTIFIER].data,
					packet -> options
					[DHO_DHCP_CLIENT_IDENTIFIER].len);
		if (hp) {
			host = hp;
			fixed_lease = mockup_lease (packet, share, hp);
			uid_lease = (struct lease *)0;
		} else {
			uid_lease = find_lease_by_uid
				(packet -> options
				 [DHO_DHCP_CLIENT_IDENTIFIER].data,
				 packet -> options
				 [DHO_DHCP_CLIENT_IDENTIFIER].len);
			/* Find the lease matching this uid that's on the
			   network the packet came from (if any). */
			for (; uid_lease; uid_lease = uid_lease -> n_uid)
				if (uid_lease -> shared_network == share)
					break;
			fixed_lease = (struct lease *)0;
			if (uid_lease &&
			    (uid_lease -> flags & ABANDONED_LEASE))
				uid_lease = (struct lease *)0;
		}
	} else {
		uid_lease = (struct lease *)0;
		fixed_lease = (struct lease *)0;
	}

	/* If we didn't find a fixed lease using the uid, try doing
	   it with the hardware address... */
	if (!fixed_lease) {
		hp = find_hosts_by_haddr (packet -> raw -> htype,
					  packet -> raw -> chaddr,
					  packet -> raw -> hlen);
		if (hp) {
			host = hp; /* Save it for later. */
			fixed_lease = mockup_lease (packet, share, hp);
		}
	}

	/* If fixed_lease is present but does not match the requested
	   IP address, and this is a DHCPREQUEST, then we can't return
	   any other lease, so we might as well return now. */
	if (packet -> packet_type == DHCPREQUEST && fixed_lease &&
	    (fixed_lease -> ip_addr.len != cip.len ||
	     memcmp (fixed_lease -> ip_addr.iabuf,
		     cip.iabuf, cip.len))) {
		if (ours)
			*ours = 1;
		strcpy (dhcp_message, "requested address is incorrect");
		return (struct lease *)0;
	}

	/* Try to find a lease that's been attached to the client's
	   hardware address... */
	hw_lease = find_lease_by_hw_addr (packet -> raw -> chaddr,
					  packet -> raw -> hlen);
	/* Find the lease that's on the network the packet came from
	   (if any). */
	for (; hw_lease; hw_lease = hw_lease -> n_hw) {
		if (hw_lease -> shared_network == share) {
			if (hw_lease -> flags & ABANDONED_LEASE)
				continue;
			if (packet -> packet_type)
				break;
			if (hw_lease -> flags &
			    (BOOTP_LEASE | DYNAMIC_BOOTP_OK))
				break;
		}
	}

	/* Try to find a lease that's been allocated to the client's
	   IP address. */
	if (cip.len)
		ip_lease = find_lease_by_ip_addr (cip);
	else
		ip_lease = (struct lease *)0;

	/* If ip_lease is valid at this point, set ours to one, so that
	   even if we choose a different lease, we know that the address
	   the client was requesting was ours, and thus we can NAK it. */
	if (ip_lease && ours)
		*ours = 1;

	/* If the requested IP address isn't on the network the packet
	   came from, don't use it.  Allow abandoned leases to be matched
	   here - if the client is requesting it, there's a decent chance
	   that it's because the lease database got trashed and a client
	   that thought it had this lease answered an ARP or PING, causing the
	   lease to be abandoned.   If so, this request probably came from
	   that client. */
	if (ip_lease && (ip_lease -> shared_network != share)) {
		if (ours)
			*ours = 1;
		ip_lease = (struct lease *)0;
		strcpy (dhcp_message, "requested address on bad subnet");
	}

	/* Toss ip_lease if it hasn't yet expired and isn't owned by the
	   client. */
	if (ip_lease &&
	    ip_lease -> ends >= cur_time &&
	    ip_lease != uid_lease) {
		int i = DHO_DHCP_CLIENT_IDENTIFIER;
		/* Make sure that ip_lease actually belongs to the client,
		   and toss it if not. */
		if ((ip_lease -> uid_len &&
		     packet -> options [i].data &&
		     ip_lease -> uid_len ==  packet -> options [i].len &&
		     !memcmp (packet -> options [i].data,
			      ip_lease -> uid, ip_lease -> uid_len)) ||
		    (!ip_lease -> uid_len &&
		     (ip_lease -> hardware_addr.htype ==
		      packet -> raw -> htype) &&
		     ip_lease -> hardware_addr.hlen == packet -> raw -> hlen &&
		     !memcmp (ip_lease -> hardware_addr.haddr,
			      packet -> raw -> chaddr,
			      ip_lease -> hardware_addr.hlen))) {
			if (uid_lease) {
			    if (uid_lease -> ends > cur_time) {
				warn ("client %s has duplicate leases on %s",
				      print_hw_addr (packet -> raw -> htype,
						     packet -> raw -> hlen,
						     packet -> raw -> chaddr),
				      ip_lease -> shared_network -> name);

				if (uid_lease &&
				    !packet -> raw -> ciaddr.s_addr)
					release_lease (uid_lease);
			    }
			    uid_lease = ip_lease;
			}
		} else {
			strcpy (dhcp_message,
				"requested address is not available");
			ip_lease = (struct lease *)0;
		}

		/* If we get to here and fixed_lease is not null, that means
		   that there are both a dynamic lease and a fixed-address
		   declaration for the same IP address. */
		if (packet -> packet_type == DHCPREQUEST && fixed_lease) {
			fixed_lease = (struct lease *)0;
		      db_conflict:
			warn ("Both dynamic and static leases present for %s.",
			      piaddr (cip));
			warn ("Either remove host declaration %s or remove %s",
			      (fixed_lease && fixed_lease -> host
			       ? (fixed_lease -> host -> name
				  ? fixed_lease -> host -> name : piaddr (cip))
			       : piaddr (cip)),
			      piaddr (cip));
			warn ("from the dynamic address pool for %s",
			      ip_lease -> subnet -> shared_network -> name);
			if (fixed_lease)
				ip_lease = (struct lease *)0;
			strcpy (dhcp_message,
				"database conflict - call for help!");
		}
	}

	/* If we get to here with both fixed_lease and ip_lease not
	   null, then we have a configuration file bug. */
	if (packet -> packet_type == DHCPREQUEST && fixed_lease && ip_lease)
		goto db_conflict;

	/* Toss hw_lease if it hasn't yet expired and the uid doesn't
	   match, except that if the hardware address matches and the
	   client is now doing dynamic BOOTP (and thus hasn't provided
	   a uid) we let the client get away with it. */
	if (hw_lease &&
	    hw_lease -> ends >= cur_time &&
	    hw_lease -> uid &&
	    packet -> options [DHO_DHCP_CLIENT_IDENTIFIER].len &&
	    hw_lease != uid_lease)
		hw_lease = (struct lease *)0;

	/* Toss extra pointers to the same lease... */
	if (hw_lease == uid_lease)
		hw_lease = (struct lease *)0;
	if (ip_lease == hw_lease)
		hw_lease = (struct lease *)0;
	if (ip_lease == uid_lease)
		uid_lease = (struct lease *)0;

	/* If we've already eliminated the lease, it wasn't there to
	   begin with.   If we have come up with a matching lease,
	   set the message to bad network in case we have to throw it out. */
	if (!ip_lease) {
		strcpy (dhcp_message, "requested address not available");
	}

	/* Now eliminate leases that are on the wrong network... */
	if (ip_lease &&
	    (share != ip_lease -> shared_network)) {
		if (packet -> packet_type == DHCPREQUEST)
			release_lease (ip_lease);
		ip_lease = (struct lease *)0;
	}
	if (uid_lease &&
	    (share != uid_lease -> shared_network)) {
		if (packet -> packet_type == DHCPREQUEST)
			release_lease (uid_lease);
		uid_lease = (struct lease *)0;
	}
	if (hw_lease &&
	    (share != hw_lease -> shared_network)) {
		if (packet -> packet_type == DHCPREQUEST)
			release_lease (hw_lease);
		hw_lease = (struct lease *)0;
	}

	/* If this is a DHCPREQUEST, make sure the lease we're going to return
	   matches the requested IP address.   If it doesn't, don't return a
	   lease at all. */
	if (packet -> packet_type == DHCPREQUEST && !ip_lease && !fixed_lease)
		return (struct lease *)0;

	/* At this point, if fixed_lease is nonzero, we can assign it to
	   this client. */
	if (fixed_lease) {
		lease = fixed_lease;
	}

	/* If we got a lease that matched the ip address and don't have
	   a better offer, use that; otherwise, release it. */
	if (ip_lease) {
		if (lease) {
			if (packet -> packet_type == DHCPREQUEST)
				release_lease (ip_lease);
		} else {
			lease = ip_lease;
			lease -> host = (struct host_decl *)0;
		}
	}

	/* If we got a lease that matched the client identifier, we may want
	   to use it, but if we already have a lease we like, we must free
	   the lease that matched the client identifier. */
	if (uid_lease) {
		if (lease) {
			if (packet -> packet_type == DHCPREQUEST)	
				release_lease (uid_lease);
		} else {
			lease = uid_lease;
			lease -> host = (struct host_decl *)0;
		}
	}

	/* The lease that matched the hardware address is treated likewise. */
	if (hw_lease) {
		if (lease) {
			if (packet -> packet_type == DHCPREQUEST)	
				release_lease (hw_lease);
		} else {
			lease = hw_lease;
			lease -> host = (struct host_decl *)0;
		}
	}

	/* If we found a host_decl but no matching address, try to
	   find a host_decl that has no address, and if there is one,
	   hang it off the lease so that we can use the supplied
	   options. */
	if (lease && host && !lease -> host) {
		for (; host; host = host -> n_ipaddr) {
			if (!host -> fixed_addr) {
				lease -> host = host;
				break;
			}
		}
	}

	/* If we find an abandoned lease, take it, but print a
	   warning message, so that if it continues to lose,
	   the administrator will eventually investigate. */
	if (lease && (lease -> flags & ABANDONED_LEASE)) {
		if (packet -> packet_type == DHCPREQUEST) {
			warn ("Reclaiming REQUESTed abandoned IP address %s.",
			      piaddr (lease -> ip_addr));
			lease -> flags &= ~ABANDONED_LEASE;
		} else
			lease = (struct lease *)0;
	}

	return lease;
}

/* Search the provided host_decl structure list for an address that's on
   the specified shared network.  If one is found, mock up and return a
   lease structure for it; otherwise return the null pointer. */

struct lease *mockup_lease (packet, share, hp)
	struct packet *packet;
	struct shared_network *share;
	struct host_decl *hp;
{
	static struct lease mock;
	
	mock.subnet = find_host_for_network (&hp, &mock.ip_addr, share);
	if (!mock.subnet)
		return (struct lease *)0;
	mock.next = mock.prev = (struct lease *)0;
	mock.shared_network = mock.subnet -> shared_network;
	mock.host = hp;

	if (hp -> group -> options [DHO_DHCP_CLIENT_IDENTIFIER]) {
		mock.uid = hp -> group ->
			options [DHO_DHCP_CLIENT_IDENTIFIER] -> value;
		mock.uid_len = hp -> group ->
			options [DHO_DHCP_CLIENT_IDENTIFIER] -> len;
	} else {
		mock.uid = (unsigned char *)0;
		mock.uid_len = 0;
	}

	mock.hardware_addr = hp -> interface;
	mock.starts = mock.timestamp = mock.ends = MIN_TIME;
	mock.flags = STATIC_LEASE;
	return &mock;
}
