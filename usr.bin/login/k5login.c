/*	$NetBSD: k5login.c,v 1.12.2.4 2000/11/14 16:05:34 tv Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1980, 1987, 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)klogin.c	5.11 (Berkeley) 7/12/92";
#endif
__RCSID("$NetBSD: k5login.c,v 1.12.2.4 2000/11/14 16:05:34 tv Exp $");
#endif /* not lint */

#ifdef KERBEROS5
#include <sys/param.h>
#include <sys/syslog.h>
#include <krb5/krb5.h>
#include <com_err.h>
#include <pwd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define KRB5_DEFAULT_OPTIONS 0
#define KRB5_DEFAULT_LIFE 60*60*10 /* 10 hours */

krb5_context kcontext;

int notickets;
int krb5_configured;
char *krb5tkfile_env;
extern char *tty;
extern int login_krb5_forwardable_tgt;
extern int has_ccache;

static char tkt_location[MAXPATHLEN];
static krb5_creds forw_creds;
int have_forward;
static krb5_principal me, server;

int k5_read_creds __P((char *));
int k5_write_creds __P((void));
int k5_verify_creds __P((krb5_context, krb5_ccache));
int k5login __P((struct passwd *, char *, char *, char *));
void k5destroy __P((void));

#ifndef krb5_realm_length
#define krb5_realm_length(r)	((r).length)
#endif
#ifndef krb5_realm_data
#define krb5_realm_data(r)	((r).data)
#endif

/*
 * Verify the Kerberos ticket-granting ticket just retrieved for the
 * user.  If the Kerberos server doesn't respond, assume the user is
 * trying to fake us out (since we DID just get a TGT from what is
 * supposedly our KDC).  If the host/<host> service is unknown (i.e.,
 * the local keytab doesn't have it), let her in.
 *
 * Returns 1 for confirmation, -1 for failure, 0 for uncertainty.
 */
int
k5_verify_creds(c, ccache)
	krb5_context c;
	krb5_ccache ccache;
{
	char phost[MAXHOSTNAMELEN];
	int retval, have_keys;
	krb5_principal princ;
	krb5_keyblock *kb = 0;
	krb5_error_code kerror;
	krb5_data packet;
	krb5_auth_context auth_context = NULL;
	krb5_ticket *ticket = NULL;

	kerror = krb5_sname_to_principal(c, 0, 0, KRB5_NT_SRV_HST, &princ);
	if (kerror) {
		com_err("login", kerror, "constructing local service name");
		return (-1);
	}

	/* Do we have host/<host> keys? */
	/* (use default keytab, kvno IGNORE_VNO to get the first match,
	 * and default enctype.) */
	kerror = krb5_kt_read_service_key(c, NULL, princ, 0, 0, &kb);
	if (kb)
		krb5_free_keyblock(c, kb);
	/* any failure means we don't have keys at all. */
	have_keys = kerror ? 0 : 1;

	/* XXX there should be a krb5 function like mk_req, but taking a full
	 * principal, instead of a service/hostname.  (Did I miss one?) */
	gethostname(phost, sizeof(phost));
	phost[sizeof(phost) - 1] = '\0';

	/* talk to the kdc and construct the ticket */
	kerror = krb5_mk_req(c, &auth_context, 0, "host", phost,
	                     0, ccache, &packet);
	/* wipe the auth context for rd_req */
	if (auth_context) {
		krb5_auth_con_free(c, auth_context);
		auth_context = NULL;
	}
	if (kerror == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN) {
		/* we have a service key, so something should be
		 * in the database, therefore this error packet could
		 * have come from an attacker. */
		if (have_keys) {
			retval = -1;
			goto EGRESS;
		}
		/* but if it is unknown and we've got no key, we don't
		 * have any security anyhow, so it is ok. */
		else {
			retval = 0;
			goto EGRESS;
		}
	}
	else if (kerror) {
		com_err("login", kerror,
		        "Unable to verify Kerberos V5 TGT: %s", phost);
		syslog(LOG_NOTICE|LOG_AUTH, "Kerberos V5 TGT bad: %s", 
		       krb5_get_err_text(kcontext, kerror));
		retval = -1;
		goto EGRESS;
	}
	/* got ticket, try to use it */
	kerror = krb5_rd_req(c, &auth_context, &packet,
	                     princ, NULL, NULL, &ticket);
	if (kerror) {
		if (!have_keys) {
			/* The krb5 errors aren't specified well, but I think
			 * these values cover the cases we expect. */
			switch (kerror) {
			case ENOENT:	/* no keytab */
			case KRB5_KT_NOTFOUND:
				retval = 0;
				break;
			default:
				/* unexpected error: fail */
				retval = -1;
				break;
			}
		}
		else {
			/* we have keys, so if we got any error, we could be
			 * under attack. */
			retval = -1;
		}
		com_err("login", kerror, "Unable to verify host ticket");
		syslog(LOG_NOTICE|LOG_AUTH, "can't verify v5 ticket: %s; %s\n",
		       krb5_get_err_text(kcontext, kerror),
		       retval
		         ? "keytab found, assuming failure"
		         : "no keytab found, assuming success");
		goto EGRESS;
	}
	/*
	 * The host/<host> ticket has been received _and_ verified.
	 */
	retval = 1;

	/* do cleanup and return */
EGRESS:
	if (auth_context)
		krb5_auth_con_free(c, auth_context);
	krb5_free_principal(c, princ);
	/* possibly ticket and packet need freeing here as well */
	return (retval);
}

/*
 * Attempt to read forwarded kerberos creds
 *
 * return 0 on success (forwarded creds in memory)
 *        1 if no forwarded creds.
 */
int
k5_read_creds(username)
	char *username;
{
	krb5_error_code code;
	krb5_creds mcreds;
	krb5_ccache ccache;

	have_forward = 0;
	memset((char*) &mcreds, 0, sizeof(forw_creds));
	memset((char*) &forw_creds, 0, sizeof(forw_creds));

	code = krb5_cc_default(kcontext, &ccache);
	if (code) {
		com_err("login", code, "while getting default ccache");
		return(1);
	}

	code = krb5_parse_name(kcontext, username, &me);
	if (code) {
		com_err("login", code, "when parsing name %s", username);
		return(1);
	}

	mcreds.client = me;
	code = krb5_build_principal_ext(kcontext, &mcreds.server,
					krb5_realm_length(*krb5_princ_realm(kcontext, me)),
					krb5_realm_data(*krb5_princ_realm(kcontext, me)),
					KRB5_TGS_NAME_SIZE,
					KRB5_TGS_NAME,
					krb5_realm_length(*krb5_princ_realm(kcontext, me)),
					krb5_realm_data(*krb5_princ_realm(kcontext, me)),
					0);
	if (code) {
		com_err("login", code, "while building server name");
		goto nuke_ccache;
	}

	code = krb5_cc_retrieve_cred(kcontext, ccache, 0,
				       &mcreds, &forw_creds);
	if (code) {
		com_err("login", code, "while retrieving V5 initial ticket for copy");
		goto nuke_ccache;
	}
	have_forward = 1;

	strcpy(tkt_location, getenv("KRB5CCNAME"));
	krb5tkfile_env = tkt_location;
	has_ccache = 1;
	notickets = 0;

nuke_ccache:
	krb5_cc_destroy(kcontext, ccache);
	return(!have_forward);
}

int
k5_write_creds()
{
	krb5_error_code code;
	krb5_ccache ccache;

	if (!have_forward)
		return(1);
	code = krb5_cc_default(kcontext, &ccache);
	if (code) {
		com_err("login", code, "while getting default ccache");
		return(1);
	}

	code = krb5_cc_initialize(kcontext, ccache, me);
	if (code) {
		com_err("login", code, "while re-initializing V5 ccache as user");
		goto nuke_ccache_contents;
	}

	code = krb5_cc_store_cred(kcontext, ccache, &forw_creds);
	if (code) {
		com_err("login", code, "while re-storing V5 ccache as user");
		goto nuke_ccache_contents;
	}

nuke_ccache_contents:
	krb5_free_cred_contents(kcontext, &forw_creds);
	return(code != 0);
}

/*
 * Attempt to log the user in using Kerberos authentication
 *
 * return 0 on success (will be logged in)
 *	  1 if Kerberos failed (try local password in login)
 */
int
k5login(pw, instance, localhost, password)
	struct passwd *pw;
	char *instance, *localhost, *password;
{
        krb5_error_code kerror;
	krb5_creds my_creds;
	krb5_timestamp now;
	krb5_ccache ccache = NULL;
	long lifetime = KRB5_DEFAULT_LIFE;
	int options = KRB5_DEFAULT_OPTIONS;
	char *realm, *client_name;
	char *principal;

	krb5_configured = 1;

	if (login_krb5_forwardable_tgt)
		options |= KDC_OPT_FORWARDABLE;

	/*
	 * Root logins don't use Kerberos.
	 * If we have a realm, try getting a ticket-granting ticket
	 * and using it to authenticate.  Otherwise, return
	 * failure so that we can try the normal passwd file
	 * for a password.  If that's ok, log the user in
	 * without issuing any tickets.
	 */
	if (strcmp(pw->pw_name, "root") == 0 ||
	    krb5_get_default_realm(kcontext, &realm)) {
		krb5_configured = 0;
		return (1);
	}

	/*
	 * get TGT for local realm
	 * tickets are stored in a file named TKT_ROOT plus uid
	 * except for user.root tickets.
	 */

	if (strcmp(instance, "root") != 0)
	    (void)snprintf(tkt_location, sizeof tkt_location,
		"FILE:/tmp/krb5cc_%d.%s", pw->pw_uid, tty);
	else
	    (void)snprintf(tkt_location, sizeof tkt_location,
		"FILE:/tmp/krb5cc_root_%d.%s", pw->pw_uid, tty);
	krb5tkfile_env = tkt_location;
	has_ccache = 1;

	principal = (char *)malloc(strlen(pw->pw_name)+strlen(instance)+2);
	strcpy(principal, pw->pw_name);	/* XXX strcpy is safe */
	if (strlen(instance)) {
	    strcat(principal, "/");		/* XXX strcat is safe */
	    strcat(principal, instance);	/* XXX strcat is safe */
	}
	
	if ((kerror = krb5_cc_resolve(kcontext, tkt_location, &ccache)) != 0) {
	    syslog(LOG_NOTICE, "warning: %s while getting default ccache",
		krb5_get_err_text(kcontext, kerror));
	    return(1);
	}

	if ((kerror = krb5_parse_name(kcontext, principal, &me)) != 0) {
	    syslog(LOG_NOTICE, "warning: %s when parsing name %s",
		krb5_get_err_text(kcontext, kerror), principal);
	    return(1);
	}
    
	if ((kerror = krb5_unparse_name(kcontext, me, &client_name)) != 0) {
	    syslog(LOG_NOTICE, "warning: %s when unparsing name %s",
		krb5_get_err_text(kcontext, kerror), principal);
	    return(1);
	}

	kerror = krb5_cc_initialize(kcontext, ccache, me);
	if (kerror != 0) {
	    syslog(LOG_NOTICE, "%s when initializing cache %s",
		krb5_get_err_text(kcontext, kerror), tkt_location);
	    return(1);
	}

	memset((char *)&my_creds, 0, sizeof(my_creds));
    
	my_creds.client = me;

	if ((kerror = krb5_build_principal_ext(kcontext,
					&server,
					krb5_realm_length(*krb5_princ_realm(kcontext, me)),
					krb5_realm_data(*krb5_princ_realm(kcontext, me)),
					KRB5_TGS_NAME_SIZE,
					KRB5_TGS_NAME,
					krb5_realm_length(*krb5_princ_realm(kcontext, me)),
					krb5_realm_data(*krb5_princ_realm(kcontext, me)),
					0)) != 0) {
	    syslog(LOG_NOTICE, "%s while building server name",
		krb5_get_err_text(kcontext, kerror));
	    return(1);
	}

	my_creds.server = server;

	if ((kerror = krb5_timeofday(kcontext, &now)) != 0) {
	    syslog(LOG_NOTICE, "%s while getting time of day",
		krb5_get_err_text(kcontext, kerror));
	    return(1);
	}
	my_creds.times.starttime = 0;	/* start timer when request
					   gets to KDC */
	my_creds.times.endtime = now + lifetime;
	my_creds.times.renew_till = 0;

	kerror = krb5_get_in_tkt_with_password(kcontext, options,
					       NULL,
					       NULL,
					       NULL,
					       password,
					       ccache,
					       &my_creds, 0);

	if (my_creds.server != NULL)
		krb5_free_principal(kcontext, my_creds.server);

	if (chown(&tkt_location[5], pw->pw_uid, pw->pw_gid) < 0)
		syslog(LOG_ERR, "chown tkfile (%s): %m", &tkt_location[5]);

	if (kerror) {
	    if (kerror == KRB5KRB_AP_ERR_BAD_INTEGRITY)
		printf("%s: Kerberos Password incorrect\n", principal);
	    else
		printf("%s while getting initial credentials\n", krb5_get_err_text(kcontext, kerror));

	    return(1);
	}
	if (k5_verify_creds(kcontext, ccache) < 0) {
	    return(1);
	}


	/* Success */
	notickets = 0;
	return(0);
}

/*
 * Remove any credentials
 */
void
k5destroy()
{
        krb5_error_code code;
	krb5_ccache ccache = NULL;

	if (krb5tkfile_env == NULL)
	    return;

	code = krb5_cc_resolve(kcontext, krb5tkfile_env, &ccache);
	if (!code)
	    code = krb5_cc_destroy(kcontext, ccache);
}
#endif
