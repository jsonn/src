%{
/*	$NetBSD: ns_parser.y,v 1.1.1.1 1998/10/05 18:02:00 tron Exp $	*/

#if !defined(lint) && !defined(SABER)
static char rcsid[] = "Id: ns_parser.y,v 8.11 1997/12/04 07:03:05 halley Exp";
#endif /* not lint */

/*
 * Copyright (c) 1996, 1997 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* Global C stuff goes here. */

#include "port_before.h"

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>

#include "port_after.h"

#include "named.h"
#include "ns_parseutil.h"
#include "ns_lexer.h"

#define SYM_ZONE	0x010000
#define SYM_SERVER	0x020000
#define SYM_KEY		0x030000
#define SYM_ACL		0x040000
#define SYM_CHANNEL	0x050000
#define SYM_PORT	0x060000

#define SYMBOL_TABLE_SIZE 29989		/* should always be prime */
static symbol_table symtab;

#define AUTH_TABLE_SIZE 397		/* should always be prime */
static symbol_table authtab = NULL;

static zone_config current_zone;
static int seen_zone;

static options current_options;
static int seen_options;

static topology_config current_topology;
static int seen_topology;

static server_config current_server;
static int seen_server;

static char *current_algorithm;
static char *current_secret;

static log_config current_logging;
static int current_category;
static int chan_type;
static int chan_level;
static u_int chan_flags;
static int chan_facility;
static char *chan_name;
static int chan_versions;
static u_long chan_max_size;

static log_channel lookup_channel(char *);
static void define_channel(char *, log_channel);
static char *canonical_name(char *);

int yyparse();

%}

%union {
	char *			cp;
	int			s_int;
	long			num;
	u_long			ul_int;
	u_int16_t		us_int;
	struct in_addr		ip_addr;
	ip_match_element	ime;
	ip_match_list		iml;
	key_info		keyi;
	enum axfr_format	axfr_fmt;
}

/* Lexical analyzer return values. */
%token			L_EOS
%token	<ip_addr>	L_IPADDR
%token	<num>		L_NUMBER
%token	<cp>		L_STRING
%token	<cp>		L_QSTRING
%token			L_END_INCLUDE

/* Include support */
%token			T_INCLUDE

/* Items related to the "options" statement: */
%token			T_OPTIONS
%token			T_DIRECTORY T_PIDFILE T_NAMED_XFER
%token			T_DUMP_FILE T_STATS_FILE T_MEMSTATS_FILE
%token			T_FAKE_IQUERY T_RECURSION T_FETCH_GLUE
%token			T_QUERY_SOURCE T_LISTEN_ON T_PORT T_ADDRESS
%type	<us_int>	in_port
%type	<us_int>	maybe_port
%type	<us_int>	maybe_wild_port
%type	<ip_addr>	maybe_wild_addr
%token			T_DATASIZE T_STACKSIZE T_CORESIZE
%token			T_DEFAULT T_UNLIMITED
%token			T_FILES
%token			T_HOSTSTATS T_DEALLOC_ON_EXIT
%token			T_TRANSFERS_IN T_TRANSFERS_OUT T_TRANSFERS_PER_NS
%token			T_TRANSFER_FORMAT T_MAX_TRANSFER_TIME_IN
%token			T_ONE_ANSWER T_MANY_ANSWERS
%type	<axfr_fmt>	transfer_format
%token			T_NOTIFY T_AUTH_NXDOMAIN T_MULTIPLE_CNAMES
%token			T_CLEAN_INTERVAL T_INTERFACE_INTERVAL T_STATS_INTERVAL

/* Items used for the "logging" statement: */
%token			T_LOGGING T_CATEGORY T_CHANNEL T_SEVERITY T_DYNAMIC
%token			T_FILE T_VERSIONS T_SIZE
%token			T_SYSLOG T_DEBUG T_NULL_OUTPUT
%token			T_PRINT_TIME T_PRINT_CATEGORY T_PRINT_SEVERITY
%type	<s_int>		category
%type	<cp>		category_name channel_name facility_name
%type	<s_int>		maybe_syslog_facility

/* Items used for the "topology" statement: */
%token			T_TOPOLOGY

/* ip_match_list */
%type	<ime>		address_match_simple address_match_element address_name
%type	<iml>		address_match_list

/* Items used for "server" statements: */
%token			T_SERVER 
%token			T_LONG_AXFR 
%token			T_BOGUS 
%token			T_TRANSFERS 
%token			T_KEYS

/* Items used for "zone" statements: */
%token			T_ZONE
%type	<num>		optional_class
%type	<s_int>		zone_type
%token			T_IN T_CHAOS T_HESIOD
%token			T_TYPE
%token			T_MASTER T_SLAVE T_STUB T_RESPONSE
%token			T_HINT
%token			T_MASTERS T_TRANSFER_SOURCE
%token			T_ALSO_NOTIFY

/* Items used for access control lists and "allow" clauses: */
%token			T_ACL 
%token	 		T_ALLOW_UPDATE T_ALLOW_QUERY T_ALLOW_TRANSFER

/* Items related to the "key" statement: */
%token			T_SEC_KEY T_ALGID T_SECRET
%type	<keyi>		key_ref
%type  	<cp>		algorithm_id secret

/* Items used for "size_spec" clauses: */
%type	<ul_int>	size_spec

/* Items used for a "check-names" clause: */
%token			T_CHECK_NAMES
%type	<s_int>		check_names_type
%type	<s_int>		check_names_opt
%token			T_WARN T_FAIL T_IGNORE

/* Items used for "forward" clauses: */
%token			T_FORWARD T_FORWARDERS
%token			T_ONLY T_FIRST T_IF_NO_ANSWER T_IF_NO_DOMAIN

/* Items used for yes/no responses: */
%type	<num>		yea_or_nay
%token			T_YES T_TRUE T_NO T_FALSE

/* Miscellaneous items (used in several places): */
%type	<cp>		any_string

%%
config_file: statement_list
	{
		/* nothing */
	}
	;

statement_list: statement
	| statement_list statement
	;

statement: include_stmt
	| options_stmt L_EOS
	| logging_stmt L_EOS
	| server_stmt L_EOS
	| zone_stmt L_EOS
	| acl_stmt L_EOS
	| key_stmt L_EOS
	| L_END_INCLUDE
	| error L_EOS
	| error L_END_INCLUDE
	;

include_stmt: T_INCLUDE L_QSTRING L_EOS	{ lexer_begin_file($2, NULL); }
	;

/*
 * Options
 */

options_stmt: T_OPTIONS 
	{
		if (seen_options)
			parser_error(0, "cannot redefine options");
		current_options = new_options();
	}
	'{' options '}'
	{
		if (!seen_options)
			set_options(current_options, 0);
		else
			free_options(current_options);
		current_options = NULL;
		seen_options = 1;
	}
	;

options: option L_EOS
	| options option L_EOS
	;

option: /* Empty */
	| T_DIRECTORY L_QSTRING
	{
		if (current_options->directory != NULL)
			freestr(current_options->directory);
		current_options->directory = $2;
	}
	| T_NAMED_XFER L_QSTRING
	{
		if (current_options->named_xfer != NULL)
			freestr(current_options->named_xfer);
		current_options->named_xfer = $2;
	}
	| T_PIDFILE L_QSTRING
	{
		if (current_options->pid_filename != NULL)
			freestr(current_options->pid_filename);
		current_options->pid_filename = $2;
	}
	| T_STATS_FILE L_QSTRING
	{
		if (current_options->stats_filename != NULL)
			freestr(current_options->stats_filename);
		current_options->stats_filename = $2;
	}
	| T_MEMSTATS_FILE L_QSTRING
	{
		if (current_options->memstats_filename != NULL)
			freestr(current_options->memstats_filename);
		current_options->memstats_filename = $2;
	}
	| T_DUMP_FILE L_QSTRING
	{
		if (current_options->dump_filename != NULL)
			freestr(current_options->dump_filename);
		current_options->dump_filename = $2;
	}
	| T_FAKE_IQUERY yea_or_nay
	{
		set_boolean_option(current_options, OPTION_FAKE_IQUERY, $2);
	}
	| T_RECURSION yea_or_nay
	{
		set_boolean_option(current_options, OPTION_NORECURSE, !$2);
	}
	| T_FETCH_GLUE yea_or_nay
	{
		set_boolean_option(current_options, OPTION_NOFETCHGLUE, !$2);
	}
	| T_NOTIFY yea_or_nay
	{
		set_boolean_option(current_options, OPTION_NONOTIFY, !$2);
	}
	| T_HOSTSTATS yea_or_nay
	{
		set_boolean_option(current_options, OPTION_HOSTSTATS, $2);
	}
	| T_DEALLOC_ON_EXIT yea_or_nay
	{
		set_boolean_option(current_options, OPTION_DEALLOC_ON_EXIT,
				   $2);
	}
	| T_AUTH_NXDOMAIN yea_or_nay
	{
		set_boolean_option(current_options, OPTION_NONAUTH_NXDOMAIN,
				   !$2);
	}
	| T_MULTIPLE_CNAMES yea_or_nay
	{
		set_boolean_option(current_options, OPTION_MULTIPLE_CNAMES,
				   $2);
	}
	| T_CHECK_NAMES check_names_type check_names_opt
	{
		current_options->check_names[$2] = $3;
	}
	| T_LISTEN_ON maybe_port '{' address_match_list '}'
	{
		char port_string[10];
		symbol_value value;

		(void)sprintf(port_string, "%u", $2);
		if (lookup_symbol(symtab, port_string, SYM_PORT, NULL))
			parser_error(0,
				     "cannot redefine listen-on for port %u",
				     ntohs($2));
		else {
			add_listen_on(current_options, $2, $4);
			value.pointer = NULL;
			define_symbol(symtab, savestr(port_string, 1),
				      SYM_PORT, value, SYMBOL_FREE_KEY);
		}

	}
	| T_FORWARD forward_opt
	| T_FORWARDERS 
	{
		if (current_options->fwdtab) {
			free_forwarders(current_options->fwdtab);
			current_options->fwdtab = NULL;
		}
	}
	'{' opt_forwarders_list '}'
	| T_QUERY_SOURCE query_source
	| T_ALLOW_QUERY '{' address_match_list '}'
	{
		if (current_options->query_acl)
			free_ip_match_list(current_options->query_acl);
		current_options->query_acl = $3;
	}
	| T_ALLOW_TRANSFER '{' address_match_list '}'
	{
		if (current_options->transfer_acl)
			free_ip_match_list(current_options->transfer_acl);
		current_options->transfer_acl = $3;
	}
	| T_TOPOLOGY '{' address_match_list '}'
	{
		if (current_options->topology)
			free_ip_match_list(current_options->topology);
		current_options->topology = $3;
	}
	| size_clause
	{
		/* To get around the $$ = $1 default rule. */
	}
	| transfer_clause
	| T_TRANSFER_FORMAT transfer_format
	{
		current_options->transfer_format = $2;
	}
	| T_MAX_TRANSFER_TIME_IN L_NUMBER
	{
		current_options->max_transfer_time_in = $2 * 60;
	}
	| T_CLEAN_INTERVAL L_NUMBER
	{
		current_options->clean_interval = $2 * 60;
	}
	| T_INTERFACE_INTERVAL L_NUMBER
	{
		current_options->interface_interval = $2 * 60;
	}
	| T_STATS_INTERVAL L_NUMBER
	{
		current_options->stats_interval = $2 * 60;
	}
	| error
	;

transfer_format: T_ONE_ANSWER
	{
		$$ = axfr_one_answer;
	}
	| T_MANY_ANSWERS
	{
		$$ = axfr_many_answers;
	}
	;
	
maybe_wild_addr: L_IPADDR { $$ = $1; }
	| '*' { $$.s_addr = htonl(INADDR_ANY); }
	;

maybe_wild_port: in_port { $$ = $1; }
	| '*' { $$ = htons(0); }
	;

query_source_address: T_ADDRESS maybe_wild_addr
	{
		current_options->query_source.sin_addr = $2;
	}
	;

query_source_port: T_PORT maybe_wild_port
	{
		current_options->query_source.sin_port = $2;
	}
	;

query_source: query_source_address
	| query_source_port
	| query_source_address query_source_port
	| query_source_port query_source_address
	;

maybe_port: /* nothing */ { $$ = htons(NS_DEFAULTPORT); }
	| T_PORT in_port { $$ = $2; }
	;

yea_or_nay: T_YES
	{ 
		$$ = 1;	
	}
	| T_TRUE
	{ 
		$$ = 1;	
	}
	| T_NO
	{ 
		$$ = 0;	
	}
	| T_FALSE 
	{ 
		$$ = 0;	
	}
	| L_NUMBER
	{ 
		if ($1 == 1 || $1 == 0) {
			$$ = $1;
		} else {
			parser_warning(0,
				       "number should be 0 or 1; assuming 1");
			$$ = 1;
		}
	}
	;

check_names_type: T_MASTER
	{
		$$ = primary_trans;
	}
	| T_SLAVE
	{
		$$ = secondary_trans;
	}
	| T_RESPONSE
	{
		$$ = response_trans;
	}
	;

check_names_opt: T_WARN
	{
		$$ = warn;
	}
	| T_FAIL
	{
		$$ = fail;
	}
	| T_IGNORE
	{
		$$ = ignore;
	}
	;

forward_opt: T_ONLY
	{
		set_boolean_option(current_options, OPTION_FORWARD_ONLY, 1);
	}
	| T_FIRST
	{
		set_boolean_option(current_options, OPTION_FORWARD_ONLY, 0);
	}
	| T_IF_NO_ANSWER
	{
		parser_warning(0, "forward if-no-answer is unimplemented");
	}
	| T_IF_NO_DOMAIN
	{
		parser_warning(0, "forward if-no-domain is unimplemented");
	}
	;

size_clause: T_DATASIZE size_spec
	{
		current_options->data_size = $2;
	}
	| T_STACKSIZE size_spec
	{
		current_options->stack_size = $2;
	}
	| T_CORESIZE size_spec
	{
		current_options->core_size = $2;
	}
	| T_FILES size_spec
	{
		current_options->files = $2;
	}
	;

size_spec: any_string
	{
		u_long result;

		if (unit_to_ulong($1, &result))
			$$ = result;
		else {
			parser_error(0, "invalid unit string '%s'", $1);
			/* 0 means "use default" */
			$$ = 0;
		}
		freestr($1);
	}
	| L_NUMBER
	{	
		$$ = (u_long)$1;
	}
	| T_DEFAULT
	{
		$$ = 0;
	}
	| T_UNLIMITED
	{
		$$ = ULONG_MAX;
	}
	;

transfer_clause: T_TRANSFERS_IN L_NUMBER
	{
		current_options->transfers_in = (u_long) $2;
	}
	| T_TRANSFERS_OUT L_NUMBER
	{
		current_options->transfers_out = (u_long) $2;
	}
	| T_TRANSFERS_PER_NS L_NUMBER
	{
		current_options->transfers_per_ns = (u_long) $2;
	}
	;

opt_forwarders_list: /* nothing */
	| forwarders_in_addr_list
	;

forwarders_in_addr_list: forwarders_in_addr L_EOS
	{
		/* nothing */
	}
	| forwarders_in_addr_list forwarders_in_addr L_EOS
	{
		/* nothing */
	}
	;

forwarders_in_addr: L_IPADDR
	{
	  	add_forwarder(current_options, $1);
	}
	;

/*
 * Logging
 */

logging_stmt: T_LOGGING
	{
		current_logging = begin_logging();
	}
	'{' logging_opts_list '}'
	{
		end_logging(current_logging, 1);
		current_logging = NULL;
	}
	;

logging_opts_list: logging_opt L_EOS
	| logging_opts_list logging_opt L_EOS
	| error
	;

logging_opt: T_CATEGORY category 
	{
		current_category = $2;
	}
	'{' channel_list '}'
	| T_CHANNEL channel_name
	{
		chan_type = log_null;
		chan_flags = 0;
		chan_level = log_info;
	}
	'{' channel_opt_list '}'
	{
		log_channel current_channel = NULL;

		if (lookup_channel($2) != NULL) {
			parser_error(0, "can't redefine channel '%s'", $2);
			freestr($2);
		} else {
			switch (chan_type) {
			case log_file:
				current_channel =
					log_new_file_channel(chan_flags,
							     chan_level,
							     chan_name, NULL,
							     chan_versions,
							     chan_max_size);
				freestr(chan_name);
				chan_name = NULL;
				break;
			case log_syslog:
				current_channel =
					log_new_syslog_channel(chan_flags,
							       chan_level,
							       chan_facility);
				break;
			case log_null:
				current_channel = log_new_null_channel();
				break;
			default:
				ns_panic(ns_log_parser, 1,
					 "unknown channel type: %d",
					 chan_type);
			}
			if (current_channel == NULL)
				ns_panic(ns_log_parser, 0,
					 "couldn't create channel");
			define_channel($2, current_channel);
		}
	}
	;

channel_severity: any_string
	{
		symbol_value value;

		if (lookup_symbol(constants, $1, SYM_LOGGING, &value)) {
			chan_level = value.integer;
		} else {
			parser_error(0, "unknown severity '%s'", $1);
			chan_level = log_debug(99);
		}
		freestr($1);
	}
	| T_DEBUG
	{
		chan_level = log_debug(1);
	}
	| T_DEBUG L_NUMBER
	{
		chan_level = $2;
	}
	| T_DYNAMIC
	{
		chan_level = 0;
		chan_flags |= LOG_USE_CONTEXT_LEVEL|LOG_REQUIRE_DEBUG;
	}
	;

version_modifier: T_VERSIONS L_NUMBER
	{
		chan_versions = $2;
		chan_flags |= LOG_TRUNCATE;
	}
	| T_VERSIONS T_UNLIMITED
	{
		chan_versions = LOG_MAX_VERSIONS;
		chan_flags |= LOG_TRUNCATE;
	}
	;

size_modifier: T_SIZE size_spec
	{
		chan_max_size = $2;
	}
	;

maybe_file_modifiers: /* nothing */
	{
		chan_versions = 0;
		chan_max_size = ULONG_MAX;
	}
	| version_modifier
	{
		chan_max_size = ULONG_MAX;
	}
	| size_modifier
	{
		chan_versions = 0;
	}
	| version_modifier size_modifier
	| size_modifier version_modifier
	;

channel_file: T_FILE L_QSTRING maybe_file_modifiers
	{
		chan_flags |= LOG_CLOSE_STREAM;
		chan_type = log_file;
		chan_name = $2;
	}
	;


facility_name: any_string { $$ = $1; }
	| T_SYSLOG { $$ = savestr("syslog", 1); }
	;

maybe_syslog_facility: /* nothing */ { $$ = LOG_DAEMON; }
	| facility_name
	{
		symbol_value value;

		if (lookup_symbol(constants, $1, SYM_SYSLOG, &value)) {
			$$ = value.integer;
		} else {
			parser_error(0, "unknown facility '%s'", $1);
			$$ = LOG_DAEMON;
		}
		freestr($1);
	}
	;

channel_syslog: T_SYSLOG maybe_syslog_facility
	{
		chan_type = log_syslog;
		chan_facility = $2;
	}
	;

channel_opt: channel_file { /* nothing to do */ }
	| channel_syslog { /* nothing to do */ }
	| T_NULL_OUTPUT
	{
		chan_type = log_null;
	}
	| T_SEVERITY channel_severity { /* nothing to do */ }
	| T_PRINT_TIME yea_or_nay
	{
		if ($2)
			chan_flags |= LOG_TIMESTAMP;
		else
			chan_flags &= ~LOG_TIMESTAMP;
	}
	| T_PRINT_CATEGORY yea_or_nay
	{
		if ($2)
			chan_flags |= LOG_PRINT_CATEGORY;
		else
			chan_flags &= ~LOG_PRINT_CATEGORY;
	}
	| T_PRINT_SEVERITY yea_or_nay
	{
		if ($2)
			chan_flags |= LOG_PRINT_LEVEL;
		else
			chan_flags &= ~LOG_PRINT_LEVEL;
	}
	;

channel_opt_list: channel_opt L_EOS
	| channel_opt_list channel_opt L_EOS
	| error
	;

channel_name: any_string
	| T_NULL_OUTPUT { $$ = savestr("null", 1); }
	;

channel: channel_name
	{
		log_channel channel;
		symbol_value value;

		if (current_category >= 0) {
			channel = lookup_channel($1);
			if (channel != NULL) {
				add_log_channel(current_logging,
						current_category, channel);
			} else
				parser_error(0, "unknown channel '%s'", $1);
		}
		freestr($1);
	}
	;

channel_list: channel L_EOS
	| channel_list channel L_EOS
	| error
	;

category_name: any_string
	| T_DEFAULT { $$ = savestr("default", 1); }
	| T_NOTIFY { $$ = savestr("notify", 1); }
	;

category: category_name
	{
		symbol_value value;

		if (lookup_symbol(constants, $1, SYM_CATEGORY, &value))
			$$ = value.integer;
		else {
			parser_error(0, "invalid logging category '%s'",
				     $1);
			$$ = -1;
		}
		freestr($1);
	}
	;

/*
 * Server Information
 */

server_stmt: T_SERVER L_IPADDR
	{
		char *ip_printable;
		symbol_value value;
		
		ip_printable = inet_ntoa($2);
		value.pointer = NULL;
		if (lookup_symbol(symtab, ip_printable, SYM_SERVER, NULL))
			seen_server = 1;
		else
			seen_server = 0;
		if (seen_server)
			parser_error(0, "cannot redefine server '%s'", 
				     ip_printable);
		else
			define_symbol(symtab, savestr(ip_printable, 1),
				      SYM_SERVER, value,
				      SYMBOL_FREE_KEY);
		current_server = begin_server($2);
	}
	'{' server_info_list '}'
	{
		end_server(current_server, !seen_server);
	}
	;

server_info_list: server_info L_EOS
	| server_info_list server_info L_EOS
	;

server_info: T_BOGUS yea_or_nay
	{
		set_server_option(current_server, SERVER_INFO_BOGUS, $2);
	}
	| T_TRANSFERS L_NUMBER
	{
		set_server_transfers(current_server, (int)$2);
	}
	| T_TRANSFER_FORMAT transfer_format
	{
		set_server_transfer_format(current_server, $2);
	}
	| T_KEYS '{' key_list '}'
	| error
	;

/*
 * Address Matching
 */

address_match_list: address_match_element L_EOS
	{
		ip_match_list iml;
		
		iml = new_ip_match_list();
		if ($1 != NULL)
			add_to_ip_match_list(iml, $1);
		$$ = iml;
	}
	| address_match_list address_match_element L_EOS
	{
		if ($2 != NULL)
			add_to_ip_match_list($1, $2);
		$$ = $1;
	}
	;

address_match_element: address_match_simple
	| '!' address_match_simple
	{
		if ($2 != NULL)
			ip_match_negate($2);
		$$ = $2;
	}
	;

address_match_simple: L_IPADDR
	{
		$$ = new_ip_match_pattern($1, 32);
	}
	| L_IPADDR '/' L_NUMBER
	{
		if ($3 < 0 || $3 > 32) {
			parser_error(0, "mask bits out of range; skipping");
			$$ = NULL;
		} else {
			$$ = new_ip_match_pattern($1, $3);
			if ($$ == NULL)
				parser_error(0, 
					   "address/mask mismatch; skipping");
		}
	}
	| L_NUMBER '/' L_NUMBER
	{
		struct in_addr ia;

		if ($1 > 255) {
			parser_error(0, "address out of range; skipping");
			$$ = NULL;
		} else {
			if ($3 < 0 || $3 > 32) {
				parser_error(0,
					"mask bits out of range; skipping");
					$$ = NULL;
			} else {
				ia.s_addr = htonl(($1 & 0xff) << 24);
				$$ = new_ip_match_pattern(ia, $3);
				if ($$ == NULL)
					parser_error(0, 
					   "address/mask mismatch; skipping");
			}
		}
	}
	| address_name
	| '{' address_match_list '}'
	{
		char name[256];

		/*
		 * We want to be able to clean up this iml later so
		 * we give it a name and treat it like any other acl.
		 */
		sprintf(name, "__internal_%p", $2);
		define_acl(savestr(name, 1), $2);
  		$$ = new_ip_match_indirect($2);
	}
	;

address_name: any_string
	{
		ip_match_list iml;

		iml = lookup_acl($1);
		if (iml == NULL) {
			parser_error(0, "unknown ACL '%s'", $1);
			$$ = NULL;
		} else
			$$ = new_ip_match_indirect(iml);
		freestr($1);
	}
	;

/*
 * Keys
 */

key_ref: any_string
	{
		key_info ki;

		ki = lookup_key($1);
		if (ki == NULL) {
			parser_error(0, "unknown key '%s'", $1);
			$$ = NULL;
		} else
			$$ = ki;
		freestr($1);
	}
	;

key_list_element: key_ref
	{
		if ($1 == NULL)
			parser_error(0, "empty key not added to server list ");
		else
			add_server_key_info(current_server, $1);
	}
	;

key_list: key_list_element L_EOS
	| key_list key_list_element L_EOS
	| error
	;

key_stmt: T_SEC_KEY
	{
		current_algorithm = NULL;
		current_secret = NULL;
	}
	any_string '{' key_definition '}'
	{
		key_info ki;

		if (lookup_key($3) != NULL) {
			parser_error(0, "can't redefine key '%s'", $3);
			freestr($3);
		} else {
			if (current_algorithm == NULL ||
			    current_secret == NULL)
				parser_error(0, "skipping bad key '%s'", $3);
			else {
				ki = new_key_info($3, current_algorithm,
						  current_secret);
				define_key($3, ki);
			}
		}
	}
	;
	
key_definition: algorithm_id secret
	{
		current_algorithm = $1;
		current_secret = $2;
	}
	| secret algorithm_id
	{
		current_algorithm = $2;
		current_secret = $1;
	}
	| error
	{
		current_algorithm = NULL;
		current_secret = NULL;
	}
	;

algorithm_id: T_ALGID any_string L_EOS { $$ = $2; }
	;

secret: T_SECRET any_string L_EOS { $$ = $2; }
	;

/*
 * ACLs
 */

acl_stmt: T_ACL any_string '{' address_match_list '}'
	{
		if (lookup_acl($2) != NULL) {
			parser_error(0, "can't redefine ACL '%s'", $2);
			freestr($2);
		} else
			define_acl($2, $4);
	}
	;

/*
 * Zones
 */
	
zone_stmt: T_ZONE L_QSTRING optional_class
	{
		int sym_type;
		symbol_value value;
		char *zone_name;

		if (!seen_options)
			parser_error(0,
             "no options statement before first zone; using previous/default");
		sym_type = SYM_ZONE | ($3 & 0xffff);
		value.pointer = NULL;
		zone_name = canonical_name($2);
		if (zone_name == NULL) {
			parser_error(0, "can't make zone name '%s' canonical",
				     $2);
			seen_zone = 1;
			zone_name = savestr("__bad_zone__", 1);
		} else {
			seen_zone = lookup_symbol(symtab, zone_name, sym_type,
						  NULL);
			if (seen_zone) {
				parser_error(0,
					"cannot redefine zone '%s' class %d",
					     zone_name, $3);
			} else
				define_symbol(symtab, zone_name, sym_type,
					      value, 0);
		}
		freestr($2);
		current_zone = begin_zone(zone_name, $3); 
	}
	optional_zone_options_list
	{ end_zone(current_zone, !seen_zone); }
	;

optional_zone_options_list: /* Empty */
	| '{' zone_option_list '}'
	;

optional_class: /* Empty */
	{
		$$ = C_IN;
	}
	| any_string
	{
		symbol_value value;

		if (lookup_symbol(constants, $1, SYM_CLASS, &value))
			$$ = value.integer;
		else {
			/* the zone validator will give the error */
			$$ = C_NONE;
		}
		freestr($1);
	}
	;

zone_type: T_MASTER
	{
		$$ = Z_MASTER;
	}
	| T_SLAVE
	{
		$$ = Z_SLAVE;
	}
	| T_HINT
	{
		$$ = Z_HINT;
	}
	| T_STUB
	{
		$$ = Z_STUB;
	}
	;

zone_option_list: zone_option L_EOS
	| zone_option_list zone_option L_EOS
	;

zone_option: T_TYPE zone_type
	{
		if (!set_zone_type(current_zone, $2))
			parser_warning(0, "zone type already set; skipping");
	}
	| T_FILE L_QSTRING
	{
		if (!set_zone_filename(current_zone, $2))
			parser_warning(0,
				       "zone filename already set; skipping");
	}
	| T_MASTERS '{' master_in_addr_list '}'
	| T_TRANSFER_SOURCE maybe_wild_addr
	{
		set_zone_transfer_source(current_zone, $2);
	}
	| T_CHECK_NAMES check_names_opt
	{
		if (!set_zone_checknames(current_zone, $2))
			parser_warning(0,
	                              "zone checknames already set; skipping");
	}
	| T_ALLOW_UPDATE '{' address_match_list '}'
	{
		if (!set_zone_update_acl(current_zone, $3))
			parser_warning(0,
				      "zone update acl already set; skipping");
	}
	| T_ALLOW_QUERY '{' address_match_list '}'
	{
		if (!set_zone_query_acl(current_zone, $3))
			parser_warning(0,
				      "zone query acl already set; skipping");
	}
	| T_ALLOW_TRANSFER '{' address_match_list '}'
	{
		if (!set_zone_transfer_acl(current_zone, $3))
			parser_warning(0,
				    "zone transfer acl already set; skipping");
	}
	| T_MAX_TRANSFER_TIME_IN L_NUMBER
	{
		if (!set_zone_transfer_time_in(current_zone, $2*60))
			parser_warning(0,
		       "zone max transfer time (in) already set; skipping");
	}
	| T_NOTIFY yea_or_nay
	{
		set_zone_notify(current_zone, $2);
	}
	| T_ALSO_NOTIFY '{' opt_notify_in_addr_list '}'
	| error
	;

master_in_addr_list: master_in_addr L_EOS
	{
		/* nothing */
	}
	| master_in_addr_list master_in_addr L_EOS
	{
		/* nothing */
	}
	;

master_in_addr: L_IPADDR
	{
	  	add_zone_master(current_zone, $1);
	}
	;

opt_notify_in_addr_list: /* nothing */
	| notify_in_addr_list
	;

notify_in_addr_list: notify_in_addr L_EOS
	{
		/* nothing */
	}
	| notify_in_addr_list notify_in_addr L_EOS
	{
		/* nothing */
	}
	;

notify_in_addr: L_IPADDR
	{
	  	add_zone_notify(current_zone, $1);
	}
	;

/*
 * Misc.
 */

in_port: L_NUMBER
	{
		if ($1 < 0 || $1 > 65535) {
		  	parser_warning(0, 
			  "invalid IP port number '%d'; setting port to 0",
			               $1);
			$1 = 0;
		} else
			$$ = htons($1);
	}
	;

any_string: L_STRING
	| L_QSTRING
	;
	
%%

static char *
canonical_name(char *name) {
	char canonical[MAXDNAME];
	
	if (strlen(name) >= MAXDNAME)
		return (NULL);
	strcpy(canonical, name);
	if (makename(canonical, ".", sizeof canonical) < 0)
		return (NULL);
	return (savestr(canonical, 0));
}

static void
init_acls() {
	ip_match_element ime;
	ip_match_list iml;
	struct in_addr address;

	/* Create the predefined ACLs */

	address.s_addr = 0U;

	/* ACL "any" */
	ime = new_ip_match_pattern(address, 0);
	iml = new_ip_match_list();
	add_to_ip_match_list(iml, ime);
	define_acl(savestr("any", 1), iml);

	/* ACL "none" */
	ime = new_ip_match_pattern(address, 0);
	ip_match_negate(ime);
	iml = new_ip_match_list();
	add_to_ip_match_list(iml, ime);
	define_acl(savestr("none", 1), iml);

	/* ACL "localhost" */
	ime = new_ip_match_localhost();
	iml = new_ip_match_list();
	add_to_ip_match_list(iml, ime);
	define_acl(savestr("localhost", 1), iml);

	/* ACL "localnets" */
	ime = new_ip_match_localnets();
	iml = new_ip_match_list();
	add_to_ip_match_list(iml, ime);
	define_acl(savestr("localnets", 1), iml);
}

static void
free_sym_value(int type, void *value) {
	ns_debug(ns_log_parser, 99, "free_sym_value: type %06x value %p",
		 type, value);
	type &= ~0xffff;
	switch (type) {
	case SYM_ACL:
		free_ip_match_list(value);
		break;
	case SYM_KEY:
		free_key_info(value);
		break;
	default:
		ns_panic(ns_log_parser, 1,
			 "unhandled case in free_sym_value()");
		/* NOTREACHED */
		break;
	}
}

static log_channel
lookup_channel(char *name) {
	symbol_value value;

	if (lookup_symbol(symtab, name, SYM_CHANNEL, &value))
		return ((log_channel)(value.pointer));
	return (NULL);
}

static void
define_channel(char *name, log_channel channel) {
	symbol_value value;

	value.pointer = channel;  
	define_symbol(symtab, name, SYM_CHANNEL, value, SYMBOL_FREE_KEY);
}

static void
define_builtin_channels() {
	define_channel(savestr("default_syslog", 1), syslog_channel);
	define_channel(savestr("default_debug", 1), debug_channel);
	define_channel(savestr("default_stderr", 1), stderr_channel);
	define_channel(savestr("null", 1), null_channel);
}

static void
parser_setup() {
	seen_options = 0;
	seen_topology = 0;
	symtab = new_symbol_table(SYMBOL_TABLE_SIZE, NULL);
	if (authtab != NULL)
		free_symbol_table(authtab);
	authtab = new_symbol_table(AUTH_TABLE_SIZE, free_sym_value);
	init_acls();
	define_builtin_channels();
}

static void
parser_cleanup() {
	if (symtab != NULL)
		free_symbol_table(symtab);
	symtab = NULL;
	/*
	 * We don't clean up authtab here because the ip_match_lists are in
	 * use.
	 */
}

/*
 * Public Interface
 */

ip_match_list
lookup_acl(char *name) {
	symbol_value value;

	if (lookup_symbol(authtab, name, SYM_ACL, &value))
		return ((ip_match_list)(value.pointer));
	return (NULL);
}

void
define_acl(char *name, ip_match_list iml) {
	symbol_value value;

	INSIST(name != NULL);
	INSIST(iml != NULL);

	value.pointer = iml;
	define_symbol(authtab, name, SYM_ACL, value,
		      SYMBOL_FREE_KEY|SYMBOL_FREE_VALUE);
	ns_debug(ns_log_parser, 7, "acl %s", name);
	dprint_ip_match_list(ns_log_parser, iml, 2, "allow ", "deny ");
}

key_info
lookup_key(char *name) {
	symbol_value value;

	if (lookup_symbol(authtab, name, SYM_KEY, &value))
		return ((key_info)(value.pointer));
	return (NULL);
}

void
define_key(char *name, key_info ki) {
	symbol_value value;

	INSIST(name != NULL);
	INSIST(ki != NULL);

	value.pointer = ki;
	define_symbol(authtab, name, SYM_KEY, value, SYMBOL_FREE_VALUE);
	dprint_key_info(ki);
}

void
parse_configuration(const char *filename) {
	FILE *config_stream;

	config_stream = fopen(filename, "r");
	if (config_stream == NULL)
		ns_panic(ns_log_parser, 0, "can't open '%s'", filename);

	lexer_setup();
	parser_setup();
	lexer_begin_file(filename, config_stream);
	(void)yyparse();
	lexer_end_file();
	parser_cleanup();
}

void
parser_initialize(void) {
	lexer_initialize();
}

void
parser_shutdown(void) {
	if (authtab != NULL)
		free_symbol_table(authtab);
	lexer_shutdown();
}
