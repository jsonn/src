/*	$NetBSD: smtp_sasl_glue.c,v 1.1.1.5.2.1 2006/07/12 15:06:42 tron Exp $	*/

/*++
/* NAME
/*	smtp_sasl 3
/* SUMMARY
/*	Postfix SASL interface for SMTP client
/* SYNOPSIS
/*	#include smtp_sasl.h
/*
/*	void	smtp_sasl_initialize()
/*
/*	void	smtp_sasl_connect(session)
/*	SMTP_SESSION *session;
/*
/*	void	smtp_sasl_start(session, sasl_opts_name, sasl_opts_val)
/*	SMTP_SESSION *session;
/*
/*	int     smtp_sasl_passwd_lookup(session)
/*	SMTP_SESSION *session;
/*
/*	int	smtp_sasl_authenticate(session, why)
/*	SMTP_SESSION *session;
/*	VSTRING *why;
/*
/*	void	smtp_sasl_cleanup(session)
/*	SMTP_SESSION *session;
/*
/*	void	smtp_sasl_passivate(session, buf)
/*	SMTP_SESSION *session;
/*	VSTRING	*buf;
/*
/*	int	smtp_sasl_activate(session, buf)
/*	SMTP_SESSION *session;
/*	char	*buf;
/* DESCRIPTION
/*	smtp_sasl_initialize() initializes the SASL library. This
/*	routine must be called once at process startup, before any
/*	chroot operations.
/*
/*	smtp_sasl_connect() performs per-session initialization. This
/*	routine must be called once at the start of each connection.
/*
/*	smtp_sasl_start() performs per-session initialization. This
/*	routine must be called once per session before doing any SASL
/*	authentication. The sasl_opts_name and sasl_opts_val parameters are
/*	the postfix configuration parameters setting the security
/*	policy of the SASL authentication.
/*
/*	smtp_sasl_passwd_lookup() looks up the username/password
/*	for the current SMTP server. The result is zero in case
/*	of failure.
/*
/*	smtp_sasl_authenticate() implements the SASL authentication
/*	dialog. The result is < 0 in case of protocol failure, zero in
/*	case of unsuccessful authentication, > 0 in case of success.
/*	The why argument is updated with a reason for failure.
/*	This routine must be called only when smtp_sasl_passwd_lookup()
/*	suceeds.
/*
/*	smtp_sasl_cleanup() cleans up. It must be called at the
/*	end of every SMTP session that uses SASL authentication.
/*	This routine is a noop for non-SASL sessions.
/*
/*	smtp_sasl_passivate() appends flattened SASL attributes to the
/*	specified buffer. The SASL attributes are not destroyed.
/*
/*	smtp_sasl_activate() restores SASL attributes from the
/*	specified buffer. The buffer is modified. A result < 0
/*	means there was an error.
/*
/*	Arguments:
/* .IP session
/*	Session context.
/* .IP mech_list
/*	String of SASL mechanisms (separated by blanks)
/* DIAGNOSTICS
/*	All errors are fatal.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Original author:
/*	Till Franke
/*	SuSE Rhein/Main AG
/*	65760 Eschborn, Germany
/*
/*	Adopted by:
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

 /*
  * System library.
  */
#include <sys_defs.h>
#include <stdlib.h>
#include <string.h>
#ifdef STRCASECMP_IN_STRINGS_H
#include <strings.h>
#endif

 /*
  * Utility library
  */
#include <msg.h>
#include <mymalloc.h>
#include <stringops.h>
#include <split_at.h>
#include <name_mask.h>

 /*
  * Global library
  */
#include <mail_params.h>
#include <string_list.h>
#include <maps.h>

 /*
  * Application-specific
  */
#include "smtp.h"
#include "smtp_sasl.h"

#ifdef USE_SASL_AUTH

 /*
  * Authentication security options.
  */
static NAME_MASK smtp_sasl_sec_mask[] = {
    "noplaintext", SASL_SEC_NOPLAINTEXT,
    "noactive", SASL_SEC_NOACTIVE,
    "nodictionary", SASL_SEC_NODICTIONARY,
    "noanonymous", SASL_SEC_NOANONYMOUS,
#if SASL_VERSION_MAJOR >= 2
    "mutual_auth", SASL_SEC_MUTUAL_AUTH,
#endif
    0,
};

 /*
  * Silly little macros.
  */
#define STR(x)	vstring_str(x)

 /*
  * Macros to handle API differences between SASLv1 and SASLv2. Specifics:
  * 
  * The SASL_LOG_* constants were renamed in SASLv2.
  * 
  * SASLv2's sasl_client_new takes two new parameters to specify local and
  * remote IP addresses for auth mechs that use them.
  * 
  * SASLv2's sasl_client_start function no longer takes the secret parameter.
  * 
  * SASLv2's sasl_decode64 function takes an extra parameter for the length of
  * the output buffer.
  * 
  * The other major change is that SASLv2 now takes more responsibility for
  * deallocating memory that it allocates internally.  Thus, some of the
  * function parameters are now 'const', to make sure we don't try to free
  * them too.  This is dealt with in the code later on.
  */

#if SASL_VERSION_MAJOR < 2
/* SASL version 1.x */
#define SASL_LOG_WARN SASL_LOG_WARNING
#define SASL_LOG_NOTE SASL_LOG_INFO
#define SASL_CLIENT_NEW(srv, fqdn, lport, rport, prompt, secflags, pconn) \
	sasl_client_new(srv, fqdn, prompt, secflags, pconn)
#define SASL_CLIENT_START(conn, mechlst, secret, prompt, clout, cllen, mech) \
	sasl_client_start(conn, mechlst, secret, prompt, clout, cllen, mech)
#define SASL_DECODE64(in, inlen, out, outmaxlen, outlen) \
	sasl_decode64(in, inlen, out, outlen)
#endif

#if SASL_VERSION_MAJOR >= 2
/* SASL version > 2.x */
#define SASL_CLIENT_NEW(srv, fqdn, lport, rport, prompt, secflags, pconn) \
	sasl_client_new(srv, fqdn, lport, rport, prompt, secflags, pconn)
#define SASL_CLIENT_START(conn, mechlst, secret, prompt, clout, cllen, mech) \
	sasl_client_start(conn, mechlst, prompt, clout, cllen, mech)
#define SASL_DECODE64(in, inlen, out, outmaxlen, outlen) \
	sasl_decode64(in, inlen, out, outmaxlen, outlen)
#endif

 /*
  * Per-host login/password information.
  */
static MAPS *smtp_sasl_passwd_map;

 /* 
  * Supported SASL mechanisms.
  */
STRING_LIST *smtp_sasl_mechs;

/* smtp_sasl_log - logging call-back routine */

static int smtp_sasl_log(void *unused_context, int priority,
			         const char *message)
{
    switch (priority) {
	case SASL_LOG_ERR:		/* unusual errors */
	case SASL_LOG_WARN:		/* non-fatal warnings */
	msg_warn("SASL authentication problem: %s", message);
	break;
    case SASL_LOG_NOTE:			/* other info */
	if (msg_verbose)
	    msg_info("SASL authentication info: %s", message);
	break;
#if SASL_VERSION_MAJOR >= 2
    case SASL_LOG_FAIL:			/* authentication failures */
	msg_warn("SASL authentication failure: %s", message);
#endif
    }
    return (SASL_OK);
}

/* smtp_sasl_get_user - username lookup call-back routine */

static int smtp_sasl_get_user(void *context, int unused_id, const char **result,
			              unsigned *len)
{
    char   *myname = "smtp_sasl_get_user";
    SMTP_SESSION *session = (SMTP_SESSION *) context;

    if (msg_verbose)
	msg_info("%s: %s", myname, session->sasl_username);

    /*
     * Sanity check.
     */
    if (session->sasl_passwd == 0)
	msg_panic("%s: no username looked up", myname);

    *result = session->sasl_username;
    if (len)
	*len = strlen(session->sasl_username);
    return (SASL_OK);
}

/* smtp_sasl_get_passwd - password lookup call-back routine */

static int smtp_sasl_get_passwd(sasl_conn_t *conn, void *context,
				        int id, sasl_secret_t **psecret)
{
    char   *myname = "smtp_sasl_get_passwd";
    SMTP_SESSION *session = (SMTP_SESSION *) context;
    int     len;

    if (msg_verbose)
	msg_info("%s: %s", myname, session->sasl_passwd);

    /*
     * Sanity check.
     */
    if (!conn || !psecret || id != SASL_CB_PASS)
	return (SASL_BADPARAM);
    if (session->sasl_passwd == 0)
	msg_panic("%s: no password looked up", myname);

    /*
     * Convert the password into a counted string.
     */
    len = strlen(session->sasl_passwd);
    if ((*psecret = (sasl_secret_t *) malloc(sizeof(sasl_secret_t) + len)) == 0)
	return (SASL_NOMEM);
    (*psecret)->len = len;
    memcpy((*psecret)->data, session->sasl_passwd, len + 1);

    return (SASL_OK);
}

/* smtp_sasl_passwd_lookup - password lookup routine */

int     smtp_sasl_passwd_lookup(SMTP_SESSION *session)
{
    char   *myname = "smtp_sasl_passwd_lookup";
    const char *value;
    char   *passwd;

    /*
     * Sanity check.
     */
    if (smtp_sasl_passwd_map == 0)
	msg_panic("%s: passwd map not initialized", myname);

    /*
     * Look up the per-server password information. Try the hostname first,
     * then try the destination.
     * 
     * XXX Instead of using nexthop (the intended destination) we use dest
     * (either the intended destination, or a fall-back destination).
     * 
     * XXX SASL authentication currently depends on the host/domain but not on
     * the TCP port. If the port is not :25, we should append it to the table
     * lookup key. Code for this was briefly introduced into 2.2 snapshots,
     * but didn't canonicalize the TCP port, and did not append the port to
     * the MX hostname.
     */
    if ((value = maps_find(smtp_sasl_passwd_map, session->host, 0)) != 0
    || (value = maps_find(smtp_sasl_passwd_map, session->dest, 0)) != 0) {
	session->sasl_username = mystrdup(value);
	passwd = split_at(session->sasl_username, ':');
	session->sasl_passwd = mystrdup(passwd ? passwd : "");
	if (msg_verbose)
	    msg_info("%s: host `%s' user `%s' pass `%s'",
		     myname, session->host,
		     session->sasl_username, session->sasl_passwd);
	return (1);
    } else {
	if (msg_verbose)
	    msg_info("%s: host `%s' no auth info found",
		     myname, session->host);
	return (0);
    }
}

/* smtp_sasl_initialize - per-process initialization (pre jail) */

void    smtp_sasl_initialize(void)
{

    /*
     * Global callbacks. These have no per-session context.
     */
    static sasl_callback_t callbacks[] = {
	{SASL_CB_LOG, &smtp_sasl_log, 0},
	{SASL_CB_LIST_END, 0, 0}
    };

    /*
     * Sanity check.
     */
    if (smtp_sasl_passwd_map)
	msg_panic("smtp_sasl_initialize: repeated call");
    if (*var_smtp_sasl_passwd == 0)
	msg_fatal("specify a password table via the `%s' configuration parameter",
		  VAR_SMTP_SASL_PASSWD);

    /*
     * Open the per-host password table and initialize the SASL library. Use
     * shared locks for reading, just in case someone updates the table.
     */
    smtp_sasl_passwd_map = maps_create("smtp_sasl_passwd",
				       var_smtp_sasl_passwd, DICT_FLAG_LOCK);
    if (sasl_client_init(callbacks) != SASL_OK)
	msg_fatal("SASL library initialization");

    /*
     * Initialize optional supported mechanism matchlist
     */
    if (*var_smtp_sasl_mechs)
    	smtp_sasl_mechs = string_list_init(MATCH_FLAG_NONE,
					   var_smtp_sasl_mechs);
}

/* smtp_sasl_connect - per-session client initialization */

void    smtp_sasl_connect(SMTP_SESSION *session)
{
    session->sasl_mechanism_list = 0;
    session->sasl_username = 0;
    session->sasl_passwd = 0;
    session->sasl_conn = 0;
    session->sasl_encoded = 0;
    session->sasl_decoded = 0;
    session->sasl_callbacks = 0;
}

/* smtp_sasl_start - per-session SASL initialization */

void    smtp_sasl_start(SMTP_SESSION *session, const char *sasl_opts_name,
			        const char *sasl_opts_val)
{
    static sasl_callback_t callbacks[] = {
	{SASL_CB_USER, &smtp_sasl_get_user, 0},
	{SASL_CB_AUTHNAME, &smtp_sasl_get_user, 0},
	{SASL_CB_PASS, &smtp_sasl_get_passwd, 0},
	{SASL_CB_LIST_END, 0, 0}
    };
    sasl_callback_t *cp;
    sasl_security_properties_t sec_props;

    if (msg_verbose)
	msg_info("starting new SASL client");

    /*
     * Per-session initialization. Provide each session with its own callback
     * context.
     */
#define NULL_SECFLAGS		0

    session->sasl_callbacks = (sasl_callback_t *) mymalloc(sizeof(callbacks));
    memcpy((char *) session->sasl_callbacks, callbacks, sizeof(callbacks));
    for (cp = session->sasl_callbacks; cp->id != SASL_CB_LIST_END; cp++)
	cp->context = (void *) session;

#define NULL_SERVER_ADDR	((char *) 0)
#define NULL_CLIENT_ADDR	((char *) 0)

    if (SASL_CLIENT_NEW("smtp", session->host,
			NULL_CLIENT_ADDR, NULL_SERVER_ADDR,
			session->sasl_callbacks, NULL_SECFLAGS,
			(sasl_conn_t **) &session->sasl_conn) != SASL_OK)
	msg_fatal("per-session SASL client initialization");

    /*
     * Per-session security properties. XXX This routine is not sufficiently
     * documented. What is the purpose of all this?
     */
    memset(&sec_props, 0L, sizeof(sec_props));
    sec_props.min_ssf = 0;
    sec_props.max_ssf = 0;			/* don't allow real SASL
						 * security layer */
    sec_props.security_flags = name_mask(sasl_opts_name, smtp_sasl_sec_mask,
					 sasl_opts_val);
    sec_props.maxbufsize = 0;
    sec_props.property_names = 0;
    sec_props.property_values = 0;
    if (sasl_setprop(session->sasl_conn, SASL_SEC_PROPS,
		     &sec_props) != SASL_OK)
	msg_fatal("set per-session SASL security properties");

    /*
     * We use long-lived conversion buffers rather than local variables in
     * order to avoid memory leaks in case of read/write timeout or I/O
     * error.
     */
    session->sasl_encoded = vstring_alloc(10);
    session->sasl_decoded = vstring_alloc(10);
}

/* smtp_sasl_authenticate - run authentication protocol */

int     smtp_sasl_authenticate(SMTP_SESSION *session, VSTRING *why)
{
    char   *myname = "smtp_sasl_authenticate";
    unsigned enc_length;
    unsigned enc_length_out;

#if SASL_VERSION_MAJOR >= 2
    const char *clientout;

#else
    char   *clientout;

#endif
    unsigned clientoutlen;
    unsigned serverinlen;
    SMTP_RESP *resp;
    const char *mechanism;
    int     result;
    char   *line;

#define NO_SASL_SECRET		0
#define NO_SASL_INTERACTION	0
#define NO_SASL_LANGLIST	((const char *) 0)
#define NO_SASL_OUTLANG		((const char **) 0)

    if (msg_verbose)
	msg_info("%s: %s: SASL mechanisms %s",
		 myname, session->namaddr, session->sasl_mechanism_list);

    /*
     * Start the client side authentication protocol.
     */
    result = SASL_CLIENT_START((sasl_conn_t *) session->sasl_conn,
			       session->sasl_mechanism_list,
			       NO_SASL_SECRET, NO_SASL_INTERACTION,
			       &clientout, &clientoutlen, &mechanism);
    if (result != SASL_OK && result != SASL_CONTINUE) {
	vstring_sprintf(why, "cannot SASL authenticate to server %s: %s",
			session->namaddr,
			sasl_errstring(result, NO_SASL_LANGLIST,
				       NO_SASL_OUTLANG));
	return (-1);
    }

    /*
     * Send the AUTH command and the optional initial client response.
     * sasl_encode64() produces four bytes for each complete or incomplete
     * triple of input bytes. Allocate an extra byte for string termination.
     */
#define ENCODE64_LENGTH(n)	((((n) + 2) / 3) * 4)

    if (clientoutlen > 0) {
	if (msg_verbose)
	    msg_info("%s: %s: uncoded initial reply: %.*s",
		   myname, session->namaddr, (int) clientoutlen, clientout);
	enc_length = ENCODE64_LENGTH(clientoutlen) + 1;
	VSTRING_SPACE(session->sasl_encoded, enc_length);
	if (sasl_encode64(clientout, clientoutlen,
			  STR(session->sasl_encoded), enc_length,
			  &enc_length_out) != SASL_OK)
	    msg_panic("%s: sasl_encode64 botch", myname);
#if SASL_VERSION_MAJOR < 2
	/* SASL version 1 doesn't free memory that it allocates. */
	free(clientout);
#endif
	smtp_chat_cmd(session, "AUTH %s %s", mechanism,
		      STR(session->sasl_encoded));
    } else {
	smtp_chat_cmd(session, "AUTH %s", mechanism);
    }

    /*
     * Step through the authentication protocol until the server tells us
     * that we are done.
     */
    while ((resp = smtp_chat_resp(session))->code / 100 == 3) {

	/*
	 * Process a server challenge.
	 */
	line = resp->str;
	(void) mystrtok(&line, "- \t\n");	/* skip over result code */
	serverinlen = strlen(line);
	VSTRING_SPACE(session->sasl_decoded, serverinlen);
	if (SASL_DECODE64(line, serverinlen, STR(session->sasl_decoded),
			  serverinlen, &enc_length) != SASL_OK) {
	    vstring_sprintf(why, "malformed SASL challenge from server %s",
			    session->namaddr);
	    return (-1);
	}
	if (msg_verbose)
	    msg_info("%s: %s: decoded challenge: %.*s",
		     myname, session->namaddr, (int) enc_length,
		     STR(session->sasl_decoded));
	result = sasl_client_step((sasl_conn_t *) session->sasl_conn,
				  STR(session->sasl_decoded), enc_length,
			    NO_SASL_INTERACTION, &clientout, &clientoutlen);
	if (result != SASL_OK && result != SASL_CONTINUE)
	    msg_warn("SASL authentication failed to server %s: %s",
		  session->namaddr, sasl_errstring(result, NO_SASL_LANGLIST,
						   NO_SASL_OUTLANG));

	/*
	 * Send a client response.
	 */
	if (clientoutlen > 0) {
	    if (msg_verbose)
		msg_info("%s: %s: uncoded client response %.*s",
			 myname, session->namaddr,
			 (int) clientoutlen, clientout);
	    enc_length = ENCODE64_LENGTH(clientoutlen) + 1;
	    VSTRING_SPACE(session->sasl_encoded, enc_length);
	    if (sasl_encode64(clientout, clientoutlen,
			      STR(session->sasl_encoded), enc_length,
			      &enc_length_out) != SASL_OK)
		msg_panic("%s: sasl_encode64 botch", myname);
#if SASL_VERSION_MAJOR < 2
	    /* SASL version 1 doesn't free memory that it allocates. */
	    free(clientout);
#endif
	} else {
	    vstring_strcat(session->sasl_encoded, "");
	}
	smtp_chat_cmd(session, "%s", STR(session->sasl_encoded));
    }

    /*
     * We completed the authentication protocol.
     */
    if (resp->code / 100 != 2) {
	vstring_sprintf(why, "SASL authentication failed; server %s said: %s",
			session->namaddr, resp->str);
	return (0);
    }
    return (1);
}

/* smtp_sasl_cleanup - per-session cleanup */

void    smtp_sasl_cleanup(SMTP_SESSION *session)
{
    if (session->sasl_username) {
	myfree(session->sasl_username);
	session->sasl_username = 0;
    }
    if (session->sasl_passwd) {
	myfree(session->sasl_passwd);
	session->sasl_passwd = 0;
    }
    if (session->sasl_mechanism_list) {
	/* allocated in smtp_sasl_helo_auth */
	myfree(session->sasl_mechanism_list);
	session->sasl_mechanism_list = 0;
    }
    if (session->sasl_conn) {
	if (msg_verbose)
	    msg_info("disposing SASL state information");
	sasl_dispose(&session->sasl_conn);
    }
    if (session->sasl_callbacks) {
	myfree((char *) session->sasl_callbacks);
	session->sasl_callbacks = 0;
    }
    if (session->sasl_encoded) {
	vstring_free(session->sasl_encoded);
	session->sasl_encoded = 0;
    }
    if (session->sasl_decoded) {
	vstring_free(session->sasl_decoded);
	session->sasl_decoded = 0;
    }
}

/* smtp_sasl_passivate - append serialized SASL attributes */

void    smtp_sasl_passivate(SMTP_SESSION *session, VSTRING *buf)
{
}

/* smtp_sasl_activate - de-serialize SASL attributes */

int     smtp_sasl_activate(SMTP_SESSION *session, char *buf)
{
    return (0);
}

#endif
