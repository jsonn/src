/* print.c

   Turn data structures into printable text. */

/*
 * Copyright (c) 1995-2001 Internet Software Consortium.
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
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: print.c,v 1.1.1.6.4.2 2001/04/04 20:56:16 he Exp $ Copyright (c) 1995-2001 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

char *quotify_string (const char *s, const char *file, int line)
{
	unsigned len = 0;
	const char *sp;
	char *buf, *nsp;

	for (sp = s; sp && *sp; sp++) {
		if (*sp == ' ')
			len++;
		else if (!isascii (*sp) || !isprint (*sp))
			len += 4;
		else if (*sp == '"' || *sp == '\\')
			len += 2;
		else
			len++;
	}

	buf = dmalloc (len + 1, file, line);
	if (buf) {
		nsp = buf;
		for (sp = s; sp && *sp; sp++) {
			if (*sp == ' ')
				*nsp++ = ' ';
			else if (!isascii (*sp) || !isprint (*sp)) {
				sprintf (nsp, "\\%03o",
					 *(const unsigned char *)sp);
				nsp += 4;
			} else if (*sp == '"' || *sp == '\\') {
				*nsp++ = '\\';
				*nsp++ = *sp;
			} else
				*nsp++ = *sp;
		}
		*nsp++ = 0;
	}
	return buf;
}

char *quotify_buf (const unsigned char *s, unsigned len,
		   const char *file, int line)
{
	unsigned nulen = 0;
	char *buf, *nsp;
	int i;

	for (i = 0; i < len; i++) {
		if (s [i] == ' ')
			nulen++;
		else if (!isascii (s [i]) || !isprint (s [i]))
			nulen += 4;
		else if (s [i] == '"' || s [i] == '\\')
			nulen += 2;
		else
			nulen++;
	}

	buf = dmalloc (nulen + 1, MDL);
	if (buf) {
		nsp = buf;
		for (i = 0; i < len; i++) {
			if (s [i] == ' ')
				*nsp++ = ' ';
			else if (!isascii (s [i]) || !isprint (s [i])) {
				sprintf (nsp, "\\%3.3o", s [i]);
				nsp += 4;
			} else if (s [i] == '"' || s [i] == '\\') {
				*nsp++ = '\\';
				*nsp++ = s [i];
			} else
				*nsp++ = s [i];
		}
		*nsp++ = 0;
	}
	return buf;
}

char *print_hw_addr (htype, hlen, data)
	int htype;
	int hlen;
	unsigned char *data;
{
	static char habuf [49];
	char *s;
	int i;

	if (hlen == 0)
		habuf [0] = 0;
	else {
		s = habuf;
		for (i = 0; i < hlen; i++) {
			sprintf (s, "%02x", data [i]);
			s += strlen (s);
			*s++ = ':';
		}
		*--s = 0;
	}
	return habuf;
}

void print_lease (lease)
	struct lease *lease;
{
	struct tm *t;
	char tbuf [32];

	log_debug ("  Lease %s",
	       piaddr (lease -> ip_addr));
	
	t = gmtime (&lease -> starts);
	strftime (tbuf, sizeof tbuf, "%Y/%m/%d %H:%M:%S", t);
	log_debug ("  start %s", tbuf);
	
	t = gmtime (&lease -> ends);
	strftime (tbuf, sizeof tbuf, "%Y/%m/%d %H:%M:%S", t);
	log_debug ("  end %s", tbuf);
	
	if (lease -> hardware_addr.hlen)
		log_debug ("    hardware addr = %s",
			   print_hw_addr (lease -> hardware_addr.hbuf [0],
					  lease -> hardware_addr.hlen - 1,
					  &lease -> hardware_addr.hbuf [1]));
	log_debug ("  host %s  ",
	       lease -> host ? lease -> host -> name : "<none>");
}	

#if defined (DEBUG)
void dump_packet_option (struct option_cache *oc,
			 struct packet *packet,
			 struct lease *lease,
			 struct option_state *in_options,
			 struct option_state *cfg_options,
			 struct binding_scope **scope,
			 struct universe *u, void *foo)
{
	const char *name, *dot;
	struct data_string ds;
	memset (&ds, 0, sizeof ds);

	if (u != &dhcp_universe) {
		name = u -> name;
		dot = ".";
	} else {
		name = "";
		dot = "";
	}
	if (evaluate_option_cache (&ds, packet, lease,
				   in_options, cfg_options, scope, oc, MDL)) {
		log_debug ("  option %s%s%s %s;\n",
			   name, dot, oc -> option -> name,
			   pretty_print_option (oc -> option -> code,
						ds.data, ds.len, 1, 1));
		data_string_forget (&ds, MDL);
	}
}

void dump_packet (tp)
	struct packet *tp;
{
	struct dhcp_packet *tdp = tp -> raw;

	log_debug ("packet length %d", tp -> packet_length);
	log_debug ("op = %d  htype = %d  hlen = %d  hops = %d",
	       tdp -> op, tdp -> htype, tdp -> hlen, tdp -> hops);
	log_debug ("xid = %x  secs = %ld  flags = %x",
	       tdp -> xid, (unsigned long)tdp -> secs, tdp -> flags);
	log_debug ("ciaddr = %s", inet_ntoa (tdp -> ciaddr));
	log_debug ("yiaddr = %s", inet_ntoa (tdp -> yiaddr));
	log_debug ("siaddr = %s", inet_ntoa (tdp -> siaddr));
	log_debug ("giaddr = %s", inet_ntoa (tdp -> giaddr));
	log_debug ("chaddr = %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
	       ((unsigned char *)(tdp -> chaddr)) [0],
	       ((unsigned char *)(tdp -> chaddr)) [1],
	       ((unsigned char *)(tdp -> chaddr)) [2],
	       ((unsigned char *)(tdp -> chaddr)) [3],
	       ((unsigned char *)(tdp -> chaddr)) [4],
	       ((unsigned char *)(tdp -> chaddr)) [5]);
	log_debug ("filename = %s", tdp -> file);
	log_debug ("server_name = %s", tdp -> sname);
	if (tp -> options_valid) {
		int i;

		for (i = 0; i < tp -> options -> universe_count; i++) {
			if (tp -> options -> universes [i]) {
				option_space_foreach (tp, (struct lease *)0,
						      (struct option_state *)0,
						      tp -> options,
						      &global_scope,
						      universes [i], 0,
						      dump_packet_option);
			}
		}
	}
	log_debug ("%s", "");
}
#endif

void dump_raw (buf, len)
	const unsigned char *buf;
	unsigned len;
{
	int i;
	char lbuf [80];
	int lbix = 0;

	lbuf [0] = 0;

	for (i = 0; i < len; i++) {
		if ((i & 15) == 0) {
			if (lbix)
				log_info ("%s", lbuf);
			sprintf (lbuf, "%03x:", i);
			lbix = 4;
		} else if ((i & 7) == 0)
			lbuf [lbix++] = ' ';
		sprintf (&lbuf [lbix], " %02x", buf [i]);
		lbix += 3;
	}
	log_info ("%s", lbuf);
}

void hash_dump (table)
	struct hash_table *table;
{
	int i;
	struct hash_bucket *bp;

	if (!table)
		return;

	for (i = 0; i < table -> hash_count; i++) {
		if (!table -> buckets [i])
			continue;
		log_info ("hash bucket %d:", i);
		for (bp = table -> buckets [i]; bp; bp = bp -> next) {
			if (bp -> len)
				dump_raw (bp -> name, bp -> len);
			else
				log_info ("%s", (const char *)bp -> name);
		}
	}
}

#define HBLEN 60

#define DECLARE_HEX_PRINTER(x)						      \
char *print_hex##x (len, data, limit)					      \
	unsigned len;							      \
	const u_int8_t *data;						      \
	unsigned limit;							      \
{									      \
									      \
	static char hex_buf##x [HBLEN + 1];				      \
	unsigned i;							      \
									      \
	if (limit > HBLEN)						      \
		limit = HBLEN;						      \
									      \
	for (i = 0; i < (limit - 2) && i < len; i++) {			      \
		if (!isascii (data [i]) || !isprint (data [i])) {	      \
			for (i = 0; i < limit / 3 && i < len; i++) {	      \
				sprintf (&hex_buf##x [i * 3],		      \
					 "%02x:", data [i]);		      \
			}						      \
			hex_buf##x [i * 3 - 1] = 0;			      \
			return hex_buf##x;				      \
		}							      \
	}								      \
	hex_buf##x [0] = '"';						      \
	i = len;							      \
	if (i > limit - 2)						      \
		i = limit - 2;						      \
	memcpy (&hex_buf##x [1], data, i);				      \
	hex_buf##x [i + 1] = '"';					      \
	hex_buf##x [i + 2] = 0;						      \
	return hex_buf##x;						      \
}

DECLARE_HEX_PRINTER (_1)
DECLARE_HEX_PRINTER (_2)
DECLARE_HEX_PRINTER (_3)

#define DQLEN	80

char *print_dotted_quads (len, data)
	unsigned len;
	const u_int8_t *data;
{
	static char dq_buf [DQLEN + 1];
	int i;
	char *s, *last;

	s = &dq_buf [0];
	last = s;
	
	i = 0;

	do {
		sprintf (s, "%d.%d.%d.%d, ",
			 data [i], data [i + 1], data [i + 2], data [i + 3]);
		s += strlen (s);
		i += 4;
	} while ((s - &dq_buf [0] > DQLEN - 21) &&
		 i + 3 < len);
	if (i == len)
		s [-2] = 0;
	else
		strcpy (s, "...");
	return dq_buf;
}

char *print_dec_1 (val)
	unsigned long val;
{
	static char vbuf [32];
	sprintf (vbuf, "%lu", val);
	return vbuf;
}

char *print_dec_2 (val)
	unsigned long val;
{
	static char vbuf [32];
	sprintf (vbuf, "%lu", val);
	return vbuf;
}

static unsigned print_subexpression PROTO ((struct expression *,
					    char *, unsigned));

static unsigned print_subexpression (expr, buf, len)
	struct expression *expr;
	char *buf;
	unsigned len;
{
	unsigned rv, left;
	const char *s;
	
	switch (expr -> op) {
	      case expr_none:
		if (len > 3) {
			strcpy (buf, "nil");
			return 3;
		}
		break;
		  
	      case expr_match:
		if (len > 7) {
			strcpy (buf, "(match)");
			return 7;
		}
		break;

	      case expr_check:
		rv = 10 + strlen (expr -> data.check -> name);
		if (len > rv) {
			sprintf (buf, "(check %s)",
				 expr -> data.check -> name);
			return rv;
		}
		break;

	      case expr_equal:
		if (len > 6) {
			rv = 4;
			strcpy (buf, "(eq ");
			rv += print_subexpression (expr -> data.equal [0],
						   buf + rv, len - rv - 2);
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.equal [1],
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_not_equal:
		if (len > 7) {
			rv = 5;
			strcpy (buf, "(neq ");
			rv += print_subexpression (expr -> data.equal [0],
						   buf + rv, len - rv - 2);
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.equal [1],
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_substring:
		if (len > 11) {
			rv = 8;
			strcpy (buf, "(substr ");
			rv += print_subexpression (expr -> data.substring.expr,
						   buf + rv, len - rv - 3);
			buf [rv++] = ' ';
			rv += print_subexpression
				(expr -> data.substring.offset,
				 buf + rv, len - rv - 2);
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.substring.len,
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_suffix:
		if (len > 10) {
			rv = 8;
			strcpy (buf, "(suffix ");
			rv += print_subexpression (expr -> data.suffix.expr,
						   buf + rv, len - rv - 2);
			if (len > rv)
				buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.suffix.len,
						   buf + rv, len - rv - 1);
			if (len > rv)
				buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_concat:
		if (len > 10) {
			rv = 8;
			strcpy (buf, "(concat ");
			rv += print_subexpression (expr -> data.concat [0],
						   buf + rv, len - rv - 2);
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.concat [1],
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_pick_first_value:
		if (len > 8) {
			rv = 6;
			strcpy (buf, "(pick1st ");
			rv += print_subexpression
				(expr -> data.pick_first_value.car,
				 buf + rv, len - rv - 2);
			buf [rv++] = ' ';
			rv += print_subexpression
				(expr -> data.pick_first_value.cdr,
				 buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_host_lookup:
		rv = 15 + strlen (expr -> data.host_lookup -> hostname);
		if (len > rv) {
			sprintf (buf, "(dns-lookup %s)",
				 expr -> data.host_lookup -> hostname);
			return rv;
		}
		break;

	      case expr_and:
		s = "and";
	      binop:
		rv = strlen (s);
		if (len > rv + 4) {
			buf [0] = '(';
			strcpy (&buf [1], s);
			rv += 1;
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.and [0],
						buf + rv, len - rv - 2);
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.and [1],
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_or:
		s = "or";
		goto binop;

	      case expr_add:
		s = "+";
		goto binop;

	      case expr_subtract:
		s = "-";
		goto binop;

	      case expr_multiply:
		s = "*";
		goto binop;

	      case expr_divide:
		s = "/";
		goto binop;

	      case expr_remainder:
		s = "%";
		goto binop;

	      case expr_binary_and:
		s = "&";
		goto binop;

	      case expr_binary_or:
		s = "|";
		goto binop;

	      case expr_binary_xor:
		s = "^";
		goto binop;
		
	      case expr_not:
		if (len > 6) {
			rv = 5;
			strcpy (buf, "(not ");
			rv += print_subexpression (expr -> data.not,
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_config_option:
		s = "cfg-option";
		goto dooption;

	      case expr_option:
		s = "option";
	      dooption:
		rv = strlen (s) + 2 + (strlen (expr -> data.option -> name) +
			   strlen (expr -> data.option -> universe -> name));
		if (len > rv) {
			sprintf (buf, "(option %s.%s)",
				 expr -> data.option -> universe -> name,
				 expr -> data.option -> name);
			return rv;
		}
		break;

	      case expr_hardware:
		if (len > 10) {
			strcpy (buf, "(hardware)");
			return 10;
		}
		break;

	      case expr_packet:
		if (len > 10) {
			rv = 8;
			strcpy (buf, "(substr ");
			rv += print_subexpression (expr -> data.packet.offset,
						   buf + rv, len - rv - 2);
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.packet.len,
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_const_data:
		s = print_hex_1 (expr -> data.const_data.len,
				 expr -> data.const_data.data, len);
		rv = strlen (s);
		if (rv >= len)
			rv = len - 1;
		strncpy (buf, s, rv);
		buf [rv] = 0;
		return rv;

	      case expr_encapsulate:
		rv = 13;
		strcpy (buf, "(encapsulate ");
		rv += expr -> data.encapsulate.len;
		if (rv + 2 > len)
			rv = len - 2;
		strncpy (buf,
			 (const char *)expr -> data.encapsulate.data, rv - 13);
		buf [rv++] = ')';
		buf [rv++] = 0;
		break;

	      case expr_extract_int8:
		if (len > 7) {
			rv = 6;
			strcpy (buf, "(int8 ");
			rv += print_subexpression (expr -> data.extract_int,
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_extract_int16:
		if (len > 8) {
			rv = 7;
			strcpy (buf, "(int16 ");
			rv += print_subexpression (expr -> data.extract_int,
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_extract_int32:
		if (len > 8) {
			rv = 7;
			strcpy (buf, "(int32 ");
			rv += print_subexpression (expr -> data.extract_int,
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_encode_int8:
		if (len > 7) {
			rv = 6;
			strcpy (buf, "(to-int8 ");
			rv += print_subexpression (expr -> data.encode_int,
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_encode_int16:
		if (len > 8) {
			rv = 7;
			strcpy (buf, "(to-int16 ");
			rv += print_subexpression (expr -> data.encode_int,
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_encode_int32:
		if (len > 8) {
			rv = 7;
			strcpy (buf, "(to-int32 ");
			rv += print_subexpression (expr -> data.encode_int,
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_const_int:
		s = print_dec_1 (expr -> data.const_int);
		rv = strlen (s);
		if (len > rv) {
			strcpy (buf, s);
			return rv;
		}
		break;

	      case expr_exists:
		rv = 10 + (strlen (expr -> data.option -> name) +
			   strlen (expr -> data.option -> universe -> name));
		if (len > rv) {
			sprintf (buf, "(exists %s.%s)",
				 expr -> data.option -> universe -> name,
				 expr -> data.option -> name);
			return rv;
		}
		break;

	      case expr_variable_exists:
		rv = 10 + strlen (expr -> data.variable);
		if (len > rv) {
			sprintf (buf, "(defined %s)", expr -> data.variable);
			return rv;
		}
		break;

	      case expr_variable_reference:
		rv = strlen (expr -> data.variable);
		if (len > rv) {
			sprintf (buf, "%s", expr -> data.variable);
			return rv;
		}
		break;

	      case expr_known:
		s = "known";
	      astring:
		rv = strlen (s);
		if (len > rv) {
			strcpy (buf, s);
			return rv;
		}
		break;

	      case expr_leased_address:
		s = "leased-address";
		goto astring;

	      case expr_client_state:
		s = "client-state";
		goto astring;

	      case expr_host_decl_name:
		s = "host-decl-name";
		goto astring;

	      case expr_lease_time:
		s = "lease-time";
		goto astring;

	      case expr_static:
		s = "static";
		goto astring;

	      case expr_filename:
		s = "filename";
		goto astring;

	      case expr_sname:
		s = "server-name";
		goto astring;

	      case expr_reverse:
		if (len > 11) {
			rv = 13;
			strcpy (buf, "(reverse ");
			rv += print_subexpression (expr -> data.reverse.width,
						   buf + rv, len - rv - 2);
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.reverse.buffer,
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_binary_to_ascii:
		if (len > 5) {
			rv = 9;
			strcpy (buf, "(b2a ");
			rv += print_subexpression (expr -> data.b2a.base,
						   buf + rv, len - rv - 4);
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.b2a.width,
						   buf + rv, len - rv - 3);
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.b2a.seperator,
						   buf + rv, len - rv - 2);
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.b2a.buffer,
						   buf + rv, len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_dns_transaction:
		rv = 10;
		if (len < rv + 2) {
			buf [0] = '(';
			strcpy (&buf [1], "ns-update ");
			while (len < rv + 2) {
				rv += print_subexpression
					(expr -> data.dns_transaction.car,
					 buf + rv, len - rv - 2);
				buf [rv++] = ' ';
				expr = expr -> data.dns_transaction.cdr;
			}
			buf [rv - 1] = ')';
			buf [rv] = 0;
			return rv;
		}
		return 0;

	      case expr_ns_delete:
		s = "delete";
		left = 4;
		goto dodnsupd;
	      case expr_ns_exists:
		s = "exists";
		left = 4;
		goto dodnsupd;
	      case expr_ns_not_exists:
		s = "not_exists";
		left = 4;
		goto dodnsupd;
	      case expr_ns_add:
		s = "update";
		left = 5;
	      dodnsupd:
		rv = strlen (s);
		if (len > strlen (s) + 1) {
			buf [0] = '(';
			strcpy (buf + 1, s);
			rv++;
			buf [rv++] = ' ';
			s = print_dec_1 (expr -> data.ns_add.rrclass);
			if (len > rv + strlen (s) + left) {
				strcpy (&buf [rv], s);
				rv += strlen (&buf [rv]);
			}
			buf [rv++] = ' ';
			left--;
			s = print_dec_1 (expr -> data.ns_add.rrtype);
			if (len > rv + strlen (s) + left) {
				strcpy (&buf [rv], s);
				rv += strlen (&buf [rv]);
			}
			buf [rv++] = ' ';
			left--;
			rv += print_subexpression
				(expr -> data.ns_add.rrname,
				 buf + rv, len - rv - left);
			buf [rv++] = ' ';
			left--;
			rv += print_subexpression
				(expr -> data.ns_add.rrdata,
				 buf + rv, len - rv - left);
			buf [rv++] = ' ';
			left--;
			rv += print_subexpression
				(expr -> data.ns_add.ttl,
				 buf + rv, len - rv - left);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_null:
		if (len > 6) {
			strcpy (buf, "(null)");
			return 6;
		}
		break;
	      case expr_funcall:
		rv = 12 + strlen (expr -> data.funcall.name);
		if (len > rv + 1) {
			strcpy (buf, "(funcall  ");
			strcpy (buf + 9, expr -> data.funcall.name);
			buf [rv++] = ' ';
			rv += print_subexpression
				(expr -> data.funcall.arglist, buf + rv,
				 len - rv - 1);
			buf [rv++] = ')';
			buf [rv] = 0;
			return rv;
		}
		break;

	      case expr_arg:
		rv = print_subexpression (expr -> data.arg.val, buf, len);
		if (expr -> data.arg.next && rv + 2 < len) {
			buf [rv++] = ' ';
			rv += print_subexpression (expr -> data.arg.next,
						   buf, len);
			if (rv + 1 < len)
				buf [rv++] = 0;
			return rv;
		}
		break;
	      case expr_function:
		rv = 9;
		if (len > rv + 1) {
			struct string_list *foo;
			strcpy (buf, "(function");
			for (foo = expr -> data.func -> args;
			     foo; foo = foo -> next) {
				if (len > rv + 2 + strlen (foo -> string)) {
					buf [rv - 1] = ' ';
					strcpy (&buf [rv], foo -> string);
					rv += strlen (foo -> string);
				}
			}
			buf [rv] = ')';
			buf [rv++] = 0;
			return rv;
		}
	}
	return 0;
}

void print_expression (name, expr)
	char *name;
	struct expression *expr;
{
	char buf [1024];

	print_subexpression (expr, buf, sizeof buf);
	log_info ("%s: %s", name, buf);
}

int token_print_indent_concat (FILE *file, int col,  int indent,
			       const char *prefix, 
			       const char *suffix, ...)
{
	va_list list;
	char *buf;
	unsigned len;
	char *s, *t, *u;

	va_start (list, suffix);
	s = va_arg (list, char *);
	len = 0;
	while (s) {
		len += strlen (s);
		s = va_arg (list, char *);
	}
	va_end (list);

	t = dmalloc (len + 1, MDL);
	if (!t)
		log_fatal ("token_print_indent: no memory for copy buffer");

	va_start (list, suffix);
	s = va_arg (list, char *);
	u = t;
	while (s) {
		len = strlen (s);
		strcpy (u, s);
		u += len;
	}
	va_end (list);
	
	len = token_print_indent (file, col, indent,
				  prefix, suffix, t);
	dfree (t, MDL);
	return col;
}

int token_indent_data_string (FILE *file, int col, int indent,
			      const char *prefix, const char *suffix,
			      struct data_string *data)
{
	int i;
	char *buf;
	char obuf [3];

	/* See if this is just ASCII. */
	for (i = 0; i < data -> len; i++)
		if (!isascii (data -> data [i]) ||
		    !isprint (data -> data [i]))
			break;

	/* If we have a purely ASCII string, output it as text. */
	if (i == data -> len) {
		char *buf = dmalloc (data -> len + 3, MDL);
		if (buf) {
			buf [0] = '"';
			memcpy (buf + 1, data -> data, data -> len);
			buf [data -> len + 1] = '"';
			buf [data -> len + 2] = 0;
			i = token_print_indent (file, col, indent,
						prefix, suffix, buf);
			dfree (buf, MDL);
			return i;
		}
	}

	for (i = 0; i < data -> len; i++) {
		sprintf (obuf, "%2.2x", data -> data [i]);
		col = token_print_indent (file, col, indent,
					  i == 0 ? prefix : "",
					  (i + 1 == data -> len
					   ? suffix
					   : ""), obuf);
		if (i + 1 != data -> len)
			col = token_print_indent (file, col, indent,
						  prefix, suffix, ":");
	}
	return col;
}

int token_print_indent (FILE *file, int col, int indent,
			const char *prefix,
			const char *suffix, const char *buf)
{
	int len = strlen (buf) + strlen (prefix);
	if (col + len > 79) {
		if (indent + len < 79) {
			indent_spaces (file, indent);
			col = indent;
		} else {
			indent_spaces (file, col);
			col = len > 79 ? 0 : 79 - len - 1;
		}
	} else if (prefix && *prefix) {
		fputs (prefix, file);
		col += strlen (prefix);
	}
	fputs (buf, file);
	col += len;
	if (suffix && *suffix) {
		if (col + strlen (suffix) > 79) {
			indent_spaces (file, indent);
			col = indent;
		} else {
			fputs (suffix, file);
			col += strlen (suffix);
		}
	}
	return col;
}

void indent_spaces (FILE *file, int indent)
{
	int i;
	fputc ('\n', file);
	for (i = 0; i < indent; i++)
		fputc (' ', file);
}

#if defined (NSUPDATE)
void print_dns_status (int status, ns_updque *uq)
{
	char obuf [1024];
	char *s = &obuf [0], *end = &obuf [1022];
	ns_updrec *u;
	int position;
	int ttlp;
	const char *predicate = "if", *en, *op;
	int errorp;

	for (u = ISC_LIST_HEAD (*uq); u; u = ISC_LIST_NEXT (u, r_link)) {
		ttlp = 0;

		switch (u -> r_opcode)
		{
		      case NXRRSET:
			op = "rrset doesn't exist";
			position = 1;
			break;
		      case YXRRSET:
			op = "rrset exists";
			position = 1;
			break;
		      case NXDOMAIN:
			op = "domain doesn't exist";
			position = 1;
			break;
		      case YXDOMAIN:
			op = "domain exists";
			position = 1;
			break;
		      case ADD:
			op = "add";
			position = 0;
			ttlp = 1;
			break;
		      case DELETE:
			op = "delete";
			position = 0;
			break;
		      default:
			op = "unknown";
			position = 0;
			break;
		}
		if (!position) {
			if (s != &obuf [0] && s + 1 < end)
				*s++ = ' ';
			if (s + strlen (op) < end) {
				strcpy (s, op);
				s += strlen (s);
			}
		} else {
			if (s != &obuf [0] && s + 1 < end)
				*s++ = ' ';
			if (s + strlen (predicate) < end) {
				strcpy (s, predicate);
				s += strlen (s);
			}
			predicate = "and";
		}
		if (ttlp) {
			if (s + 1 < end)
				*s++ = ' ';
			/* 27 is as big as a ttl can get. */
			if (s + 27 < end) {
				sprintf (s, "%lu",
					 (unsigned long)(u -> r_ttl));
				s += strlen (s);
			}
		}
		switch (u -> r_class) {
		      case C_IN:
			en = "IN";
			break;
		      case C_CHAOS:
			en = "CHAOS";
			break;
		      case C_HS:
			en = "HS";
			break;
		      default:
			en = "UNKNOWN";
			break;
		}
		if (s + strlen (en) < end) {
			if (s + 1 < end)
				*s++ = ' ';
			strcpy (s, en);
			s += strlen (en);
		}
		switch (u -> r_type) {
		      case T_A:
			en = "A";
			break;
		      case T_PTR:
			en = "PTR";
			break;
		      case T_MX:
			en = "MX";
			break;
		      case T_TXT:
			en = "TXT";
			break;
		      case T_CNAME:
			en = "CNAME";
			break;
		      default:
			en = "UNKNOWN";
			break;
		}
		if (s + strlen (en) < end) {
			if (s + 1 < end)
				*s++ = ' ';
			strcpy (s, en);
			s += strlen (en);
		}
		if (u -> r_dname) {
			if (s + 1 < end)
				*s++ = ' ';
			if (s + strlen (u -> r_dname) < end) {
				strcpy (s, u -> r_dname);
				s += strlen (s);
			}
		}
		if (u -> r_data) {
			if (s + 1 < end)
				*s++ = ' ';
			if (u -> r_type == T_TXT) {
				if (s + 1 < end)
					*s++ = '"';
			}
			if (s + u -> r_size < end) {
				memcpy (s, u -> r_data, u -> r_size);
				s += u -> r_size;
				if (u -> r_type == T_TXT) {
					if (s + 1 < end)
						*s++ = '"';
				}
			}
		}
		if (position) {
			if (s + 1 < end)
				*s++ = ' ';
			if (s + strlen (op) < end) {
				strcpy (s, op);
				s += strlen (s);
			}
		}
		if (u == ISC_LIST_TAIL (*uq))
			break;
	}
	if (s == &obuf [0]) {
		strcpy (s, "empty update");
		s += strlen (s);
	}
	errorp = 1;
	en = isc_result_totext (status);
#if 0
	switch (status) {
	      case -1:
		en = "resolver failed";
		break;

	      case FORMERR:
		en = "format error";
		break;

	      case NOERROR:
		en = "succeeded";
		errorp = 0;
		break;

	      case NOTAUTH:
		en = "not authorized";
		break;

	      case NOTIMP:
		en = "not implemented";
		break;

	      case NOTZONE:
		en = "not a single valid zone";
		break;

	      case NXDOMAIN:
		en = "no such domain";
		break;

	      case NXRRSET:
		en = "no such record";
		break;

	      case REFUSED:
		en = "refused";
		break;

	      case SERVFAIL:
		en = "server failed";
		break;

	      case YXDOMAIN:
		en = "domain exists";
		break;

	      case YXRRSET:
		en = "record exists";
		break;

	      default:
		en = "unknown error";
		break;
	}
#endif

	if (s + 2 < end) {
		*s++ = ':';
		*s++ = ' ';
	}
	if (s + strlen (en) < end) {
		strcpy (s, en);
		s += strlen (en);
	}
	if (s + 1 < end)
		*s++ = '.';
	*s++ = 0;
	if (errorp)
		log_error ("%s", obuf);
	else
		log_info ("%s", obuf);
}
#endif /* NSUPDATE */
