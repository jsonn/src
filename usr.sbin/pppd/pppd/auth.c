/*	$NetBSD: auth.c,v 1.25.8.2 2000/09/30 06:21:41 simonb Exp $	*/

/*
 * auth.c - PPP authentication and phase control.
 *
 * Copyright (c) 1993 The Australian National University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Australian National University.  The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
#define RCSID	"Id: auth.c,v 1.60 1999/12/23 01:25:13 paulus Exp "
#else
__RCSID("$NetBSD: auth.c,v 1.25.8.2 2000/09/30 06:21:41 simonb Exp $");
#endif
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <utmp.h>
#include <fcntl.h>
#if defined(_PATH_LASTLOG) && defined(_linux_)
#include <lastlog.h>
#endif

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef USE_PAM
#include <security/pam_appl.h>
#endif

#ifdef HAS_SHADOW
#include <shadow.h>
#ifndef PW_PPP
#define PW_PPP PW_LOGIN
#endif
#endif

#include "pppd.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "upap.h"
#include "chap.h"
#ifdef CBCP_SUPPORT
#include "cbcp.h"
#endif
#include "pathnames.h"

#ifdef RCSID
static const char rcsid[] = RCSID;
#endif

/* Bits in scan_authfile return value */
#define NONWILD_SERVER	1
#define NONWILD_CLIENT	2

#define ISWILD(word)	(word[0] == '*' && word[1] == 0)

/* The name by which the peer authenticated itself to us. */
char peer_authname[MAXNAMELEN];

/* Records which authentication operations haven't completed yet. */
static int auth_pending[NUM_PPP];

/* Set if we have successfully called plogin() */
static int logged_in;

/* List of addresses which the peer may use. */
static struct permitted_ip *addresses[NUM_PPP];

/* Wordlist giving addresses which the peer may use
   without authenticating itself. */
static struct wordlist *noauth_addrs;

/* Extra options to apply, from the secrets file entry for the peer. */
static struct wordlist *extra_options;

/* Number of network protocols which we have opened. */
static int num_np_open;

/* Number of network protocols which have come up. */
static int num_np_up;

/* Set if we got the contents of passwd[] from the pap-secrets file. */
static int passwd_from_file;

/* Set if we require authentication only because we have a default route. */
static bool default_auth;

/* Hook to enable a plugin to control the idle time limit */
int (*idle_time_hook) __P((struct ppp_idle *)) = NULL;

/* Hook for a plugin to say whether we can possibly authenticate any peer */
int (*pap_check_hook) __P((void)) = NULL;

/* Hook for a plugin to check the PAP user and password */
int (*pap_auth_hook) __P((char *user, char *passwd, char **msgp,
			  struct wordlist **paddrs,
			  struct wordlist **popts)) = NULL;

/* Hook for a plugin to know about the PAP user logout */
void (*pap_logout_hook) __P((void)) = NULL;

/* Hook for a plugin to get the PAP password for authenticating us */
int (*pap_passwd_hook) __P((char *user, char *passwd)) = NULL;

/*
 * This is used to ensure that we don't start an auth-up/down
 * script while one is already running.
 */
enum script_state {
    s_down,
    s_up
};

static enum script_state auth_state = s_down;
static enum script_state auth_script_state = s_down;
static pid_t auth_script_pid = 0;

static int used_login;		/* peer authenticated against login database */

/*
 * Option variables.
 */
bool uselogin = 0;		/* Use /etc/passwd for checking PAP */
bool cryptpap = 0;		/* Passwords in pap-secrets are encrypted */
bool refuse_pap = 0;		/* Don't wanna auth. ourselves with PAP */
bool refuse_chap = 0;		/* Don't wanna auth. ourselves with CHAP */
bool usehostname = 0;		/* Use hostname for our_name */
bool auth_required = 0;		/* Always require authentication from peer */
bool allow_any_ip = 0;		/* Allow peer to use any IP address */
bool explicit_remote = 0;	/* User specified explicit remote name */
char remote_name[MAXNAMELEN];	/* Peer's name for authentication */

/* Bits in auth_pending[] */
#define PAP_WITHPEER	1
#define PAP_PEER	2
#define CHAP_WITHPEER	4
#define CHAP_PEER	8

extern char *crypt __P((const char *, const char *));

/* Prototypes for procedures local to this file. */

static void network_phase __P((int));
static void check_idle __P((void *));
static void connect_time_expired __P((void *));
static int  plogin __P((char *, char *, char **));
static void plogout __P((void));
static int  null_login __P((int));
static int  get_pap_passwd __P((char *));
static int  have_pap_secret __P((int *));
static int  have_chap_secret __P((char *, char *, int, int *));
static int  ip_addr_check __P((u_int32_t, struct permitted_ip *));
static int  scan_authfile __P((FILE *, char *, char *, char *,
			       struct wordlist **, struct wordlist **,
			       char *));
static void free_wordlist __P((struct wordlist *));
static void auth_script __P((char *));
static void auth_script_done __P((void *));
static void set_allowed_addrs __P((int, struct wordlist *, struct wordlist *));
static int  some_ip_ok __P((struct wordlist *));
static int  setupapfile __P((char **));
static int  privgroup __P((char **));
static int  set_noauth_addr __P((char **));
static void check_access __P((FILE *, char *));
static int  wordlist_count __P((struct wordlist *));

/*
 * Authentication-related options.
 */
option_t auth_options[] = {
    { "require-pap", o_bool, &lcp_wantoptions[0].neg_upap,
      "Require PAP authentication from peer", 1, &auth_required },
    { "+pap", o_bool, &lcp_wantoptions[0].neg_upap,
      "Require PAP authentication from peer", 1, &auth_required },
    { "refuse-pap", o_bool, &refuse_pap,
      "Don't agree to auth to peer with PAP", 1 },
    { "-pap", o_bool, &refuse_pap,
      "Don't allow PAP authentication with peer", 1 },
    { "require-chap", o_bool, &lcp_wantoptions[0].neg_chap,
      "Require CHAP authentication from peer", 1, &auth_required },
    { "+chap", o_bool, &lcp_wantoptions[0].neg_chap,
      "Require CHAP authentication from peer", 1, &auth_required },
    { "refuse-chap", o_bool, &refuse_chap,
      "Don't agree to auth to peer with CHAP", 1 },
    { "-chap", o_bool, &refuse_chap,
      "Don't allow CHAP authentication with peer", 1 },
    { "name", o_string, our_name,
      "Set local name for authentication",
      OPT_PRIV|OPT_STATIC, NULL, MAXNAMELEN },
    { "user", o_string, user,
      "Set name for auth with peer", OPT_STATIC, NULL, MAXNAMELEN },
    { "usehostname", o_bool, &usehostname,
      "Must use hostname for authentication", 1 },
    { "remotename", o_string, remote_name,
      "Set remote name for authentication", OPT_STATIC,
      &explicit_remote, MAXNAMELEN },
    { "auth", o_bool, &auth_required,
      "Require authentication from peer", 1 },
    { "noauth", o_bool, &auth_required,
      "Don't require peer to authenticate", OPT_PRIV, &allow_any_ip },
    {  "login", o_bool, &uselogin,
      "Use system password database for PAP", 1 },
    { "papcrypt", o_bool, &cryptpap,
      "PAP passwords are encrypted", 1 },
    { "+ua", o_special, (void *)setupapfile,
      "Get PAP user and password from file" },
    { "password", o_string, passwd,
      "Password for authenticating us to the peer", OPT_STATIC,
      NULL, MAXSECRETLEN },
    { "privgroup", o_special, (void *)privgroup,
      "Allow group members to use privileged options", OPT_PRIV },
    { "allow-ip", o_special, (void *)set_noauth_addr,
      "Set IP address(es) which can be used without authentication",
      OPT_PRIV },
    { NULL }
};

/*
 * setupapfile - specifies UPAP info for authenticating with peer.
 */
static int
setupapfile(argv)
    char **argv;
{
    FILE * ufile;
    int l;

    lcp_allowoptions[0].neg_upap = 1;

    /* open user info file */
    seteuid(getuid());
    ufile = fopen(*argv, "r");
    seteuid(0);
    if (ufile == NULL) {
	option_error("unable to open user login data file %s", *argv);
	return 0;
    }
    check_access(ufile, *argv);

    /* get username */
    if (fgets(user, MAXNAMELEN - 1, ufile) == NULL
	|| fgets(passwd, MAXSECRETLEN - 1, ufile) == NULL){
	option_error("unable to read user login data file %s", *argv);
	return 0;
    }
    fclose(ufile);

    /* get rid of newlines */
    l = strlen(user);
    if (l > 0 && user[l-1] == '\n')
	user[l-1] = 0;
    l = strlen(passwd);
    if (l > 0 && passwd[l-1] == '\n')
	passwd[l-1] = 0;

    return (1);
}


/*
 * privgroup - allow members of the group to have privileged access.
 */
static int
privgroup(argv)
    char **argv;
{
    struct group *g;
    int i;

    g = getgrnam(*argv);
    if (g == 0) {
	option_error("group %s is unknown", *argv);
	return 0;
    }
    for (i = 0; i < ngroups; ++i) {
	if (groups[i] == g->gr_gid) {
	    privileged = 1;
	    break;
	}
    }
    return 1;
}


/*
 * set_noauth_addr - set address(es) that can be used without authentication.
 * Equivalent to specifying an entry like `"" * "" addr' in pap-secrets.
 */
static int
set_noauth_addr(argv)
    char **argv;
{
    char *addr = *argv;
    int l = strlen(addr) + 1;
    struct wordlist *wp;

    wp = (struct wordlist *) malloc(sizeof(struct wordlist) + l);
    if (wp == NULL)
	novm("allow-ip argument");
    wp->word = (char *) (wp + 1);
    wp->next = noauth_addrs;
    BCOPY(addr, wp->word, l);
    noauth_addrs = wp;
    return 1;
}


/*
 * An Open on LCP has requested a change from Dead to Establish phase.
 * Do what's necessary to bring the physical layer up.
 */
void
link_required(unit)
    int unit;
{
}

/*
 * LCP has terminated the link; go to the Dead phase and take the
 * physical layer down.
 */
void
link_terminated(unit)
    int unit;
{
    if (phase == PHASE_DEAD)
	return;
    if (pap_logout_hook) {
	pap_logout_hook();
    } else {
	if (logged_in)
	    plogout();
    }
    new_phase(PHASE_DEAD);
    notice("Connection terminated.");
}

/*
 * LCP has gone down; it will either die or try to re-establish.
 */
void
link_down(unit)
    int unit;
{
    int i;
    struct protent *protp;

    auth_state = s_down;
    if (auth_script_state == s_up && auth_script_pid == 0) {
	update_link_stats(unit);
	auth_script_state = s_down;
	auth_script(_PATH_AUTHDOWN);
    }
    for (i = 0; (protp = protocols[i]) != NULL; ++i) {
	if (!protp->enabled_flag)
	    continue;
        if (protp->protocol != PPP_LCP && protp->lowerdown != NULL)
	    (*protp->lowerdown)(unit);
        if (protp->protocol < 0xC000 && protp->close != NULL)
	    (*protp->close)(unit, "LCP down");
    }
    num_np_open = 0;
    num_np_up = 0;
    if (phase != PHASE_DEAD)
	new_phase(PHASE_TERMINATE);
}

/*
 * The link is established.
 * Proceed to the Dead, Authenticate or Network phase as appropriate.
 */
void
link_established(unit)
    int unit;
{
    int auth;
    lcp_options *wo = &lcp_wantoptions[unit];
    lcp_options *go = &lcp_gotoptions[unit];
    lcp_options *ho = &lcp_hisoptions[unit];
    int i;
    struct protent *protp;

    /*
     * Tell higher-level protocols that LCP is up.
     */
    for (i = 0; (protp = protocols[i]) != NULL; ++i)
        if (protp->protocol != PPP_LCP && protp->enabled_flag
	    && protp->lowerup != NULL)
	    (*protp->lowerup)(unit);

    if (auth_required && !(go->neg_chap || go->neg_upap)) {
	/*
	 * We wanted the peer to authenticate itself, and it refused:
	 * if we have some address(es) it can use without auth, fine,
	 * otherwise treat it as though it authenticated with PAP using
	 * a username * of "" and a password of "".  If that's not OK,
	 * boot it out.
	 */
	if (noauth_addrs != NULL) {
	    set_allowed_addrs(unit, NULL, NULL);
	} else if (!wo->neg_upap || uselogin || !null_login(unit)) {
	    warn("peer refused to authenticate: terminating link");
	    lcp_close(unit, "peer refused to authenticate");
	    status = EXIT_PEER_AUTH_FAILED;
	    return;
	}
    }

    new_phase(PHASE_AUTHENTICATE);
    used_login = 0;
    auth = 0;
    if (go->neg_chap) {
	ChapAuthPeer(unit, our_name, go->chap_mdtype);
	auth |= CHAP_PEER;
    } else if (go->neg_upap) {
	upap_authpeer(unit);
	auth |= PAP_PEER;
    }
    if (ho->neg_chap) {
	ChapAuthWithPeer(unit, user, ho->chap_mdtype);
	auth |= CHAP_WITHPEER;
    } else if (ho->neg_upap) {
	if (passwd[0] == 0) {
	    passwd_from_file = 1;
	    if (!get_pap_passwd(passwd))
		error("No secret found for PAP login");
	}
	upap_authwithpeer(unit, user, passwd);
	auth |= PAP_WITHPEER;
    }
    auth_pending[unit] = auth;

    if (!auth)
	network_phase(unit);
}

/*
 * Proceed to the network phase.
 */
static void
network_phase(unit)
    int unit;
{
    lcp_options *go = &lcp_gotoptions[unit];

    /*
     * If the peer had to authenticate, run the auth-up script now.
     */
    if (go->neg_chap || go->neg_upap) {
	auth_state = s_up;
	if (auth_script_state == s_down && auth_script_pid == 0) {
	    auth_script_state = s_up;
	    auth_script(_PATH_AUTHUP);
	}
    }

#ifdef CBCP_SUPPORT
    /*
     * If we negotiated callback, do it now.
     */
    if (go->neg_cbcp) {
	new_phase(PHASE_CALLBACK);
	(*cbcp_protent.open)(unit);
	return;
    }
#endif

    /*
     * Process extra options from the secrets file
     */
    if (extra_options) {
	options_from_list(extra_options, 1);
	free_wordlist(extra_options);
	extra_options = 0;
    }
    start_networks();
}

void
start_networks()
{
    int i;
    struct protent *protp;

    new_phase(PHASE_NETWORK);

#ifdef HAVE_MULTILINK
    if (multilink) {
	if (mp_join_bundle()) {
	    if (updetach && !nodetach)
		detach();
	    return;
	}
    }
#endif /* HAVE_MULTILINK */

#ifdef PPP_FILTER
    if (!demand)
	set_filters(&pass_filter_in, &pass_filter_out,
		    &active_filter_in, &active_filter_out);
#endif
    for (i = 0; (protp = protocols[i]) != NULL; ++i)
        if (protp->protocol < 0xC000 && protp->enabled_flag
	    && protp->open != NULL) {
	    (*protp->open)(0);
	    if (protp->protocol != PPP_CCP)
		++num_np_open;
	}

    if (num_np_open == 0)
	/* nothing to do */
	lcp_close(0, "No network protocols running");
}

/*
 * The peer has failed to authenticate himself using `protocol'.
 */
void
auth_peer_fail(unit, protocol)
    int unit, protocol;
{
    /*
     * Authentication failure: take the link down
     */
    lcp_close(unit, "Authentication failed");
    status = EXIT_PEER_AUTH_FAILED;
}

/*
 * The peer has been successfully authenticated using `protocol'.
 */
void
auth_peer_success(unit, protocol, name, namelen)
    int unit, protocol;
    char *name;
    int namelen;
{
    int bit;

    switch (protocol) {
    case PPP_CHAP:
	bit = CHAP_PEER;
	break;
    case PPP_PAP:
	bit = PAP_PEER;
	break;
    default:
	warn("auth_peer_success: unknown protocol %x", protocol);
	return;
    }

    /*
     * Save the authenticated name of the peer for later.
     */
    if (namelen > sizeof(peer_authname) - 1)
	namelen = sizeof(peer_authname) - 1;
    BCOPY(name, peer_authname, namelen);
    peer_authname[namelen] = 0;
    script_setenv("PEERNAME", peer_authname, 0);

    /*
     * If there is no more authentication still to be done,
     * proceed to the network (or callback) phase.
     */
    if ((auth_pending[unit] &= ~bit) == 0)
        network_phase(unit);
}

/*
 * We have failed to authenticate ourselves to the peer using `protocol'.
 */
void
auth_withpeer_fail(unit, protocol)
    int unit, protocol;
{
    if (passwd_from_file)
	BZERO(passwd, MAXSECRETLEN);
    /*
     * We've failed to authenticate ourselves to our peer.
     * Some servers keep sending CHAP challenges, but there
     * is no point in persisting without any way to get updated
     * authentication secrets.
     */
    lcp_close(unit, "Failed to authenticate ourselves to peer");
    status = EXIT_AUTH_TOPEER_FAILED;
}

/*
 * We have successfully authenticated ourselves with the peer using `protocol'.
 */
void
auth_withpeer_success(unit, protocol)
    int unit, protocol;
{
    int bit;

    switch (protocol) {
    case PPP_CHAP:
	bit = CHAP_WITHPEER;
	break;
    case PPP_PAP:
	if (passwd_from_file)
	    BZERO(passwd, MAXSECRETLEN);
	bit = PAP_WITHPEER;
	break;
    default:
	warn("auth_withpeer_success: unknown protocol %x", protocol);
	bit = 0;
    }

    /*
     * If there is no more authentication still being done,
     * proceed to the network (or callback) phase.
     */
    if ((auth_pending[unit] &= ~bit) == 0)
	network_phase(unit);
}


/*
 * np_up - a network protocol has come up.
 */
void
np_up(unit, proto)
    int unit, proto;
{
    int tlim;

    if (num_np_up == 0) {
	/*
	 * At this point we consider that the link has come up successfully.
	 */
	status = EXIT_OK;
	unsuccess = 0;
	new_phase(PHASE_RUNNING);

	if (idle_time_hook != 0)
	    tlim = (*idle_time_hook)(NULL);
	else
	    tlim = idle_time_limit;
	if (tlim > 0)
	    TIMEOUT(check_idle, NULL, tlim);

	/*
	 * Set a timeout to close the connection once the maximum
	 * connect time has expired.
	 */
	if (maxconnect > 0)
	    TIMEOUT(connect_time_expired, 0, maxconnect);

	/*
	 * Detach now, if the updetach option was given.
	 */
	if (updetach && !nodetach)
	    detach();
    }
    ++num_np_up;
}

/*
 * np_down - a network protocol has gone down.
 */
void
np_down(unit, proto)
    int unit, proto;
{
    if (--num_np_up == 0) {
	UNTIMEOUT(check_idle, NULL);
	new_phase(PHASE_NETWORK);
    }
}

/*
 * np_finished - a network protocol has finished using the link.
 */
void
np_finished(unit, proto)
    int unit, proto;
{
    if (--num_np_open <= 0) {
	/* no further use for the link: shut up shop. */
	lcp_close(0, "No network protocols running");
    }
}

/*
 * check_idle - check whether the link has been idle for long
 * enough that we can shut it down.
 */
static void
check_idle(arg)
    void *arg;
{
    struct ppp_idle idle;
    time_t itime;
    int tlim;

    if (!get_idle_time(0, &idle))
	return;
    if (idle_time_hook != 0) {
	tlim = idle_time_hook(&idle);
    } else {
	itime = MIN(idle.xmit_idle, idle.recv_idle);
	tlim = idle_time_limit - itime;
    }
    if (tlim <= 0) {
	/* link is idle: shut it down. */
	notice("Terminating connection due to lack of activity.");
	lcp_close(0, "Link inactive");
	need_holdoff = 0;
	status = EXIT_IDLE_TIMEOUT;
    } else {
	TIMEOUT(check_idle, NULL, tlim);
    }
}

/*
 * connect_time_expired - log a message and close the connection.
 */
static void
connect_time_expired(arg)
    void *arg;
{
    info("Connect time expired");
    lcp_close(0, "Connect time expired");	/* Close connection */
    status = EXIT_CONNECT_TIME;
}

/*
 * auth_check_options - called to check authentication options.
 */
void
auth_check_options()
{
    lcp_options *wo = &lcp_wantoptions[0];
    int can_auth;
    int lacks_ip;

    /* Default our_name to hostname, and user to our_name */
    if (our_name[0] == 0 || usehostname)
	strlcpy(our_name, hostname, sizeof(our_name));
    if (user[0] == 0)
	strlcpy(user, our_name, sizeof(user));

    /*
     * If we have a default route, require the peer to authenticate
     * unless the noauth option was given or the real user is root.
     */
    if (!auth_required && !allow_any_ip && have_route_to(0) && !privileged) {
	auth_required = 1;
	default_auth = 1;
    }

    /* If authentication is required, ask peer for CHAP or PAP. */
    if (auth_required) {
	if (!wo->neg_chap && !wo->neg_upap) {
	    wo->neg_chap = 1;
	    wo->neg_upap = 1;
	}
    } else {
	wo->neg_chap = 0;
	wo->neg_upap = 0;
    }

    /*
     * Check whether we have appropriate secrets to use
     * to authenticate the peer.
     */
    lacks_ip = 0;
    can_auth = wo->neg_upap && (uselogin || have_pap_secret(&lacks_ip));
    if (!can_auth && wo->neg_chap) {
	can_auth = have_chap_secret((explicit_remote? remote_name: NULL),
				    our_name, 1, &lacks_ip);
    }

    if (auth_required && !can_auth && noauth_addrs == NULL) {
	if (default_auth) {
	    option_error(
"By default the remote system is required to authenticate itself");
	    option_error(
"(because this system has a default route to the internet)");
	} else if (explicit_remote)
	    option_error(
"The remote system (%s) is required to authenticate itself",
			 remote_name);
	else
	    option_error(
"The remote system is required to authenticate itself");
	option_error(
"but I couldn't find any suitable secret (password) for it to use to do so.");
	if (lacks_ip)
	    option_error(
"(None of the available passwords would let it use an IP address.)");

	exit(1);
    }
}

/*
 * auth_reset - called when LCP is starting negotiations to recheck
 * authentication options, i.e. whether we have appropriate secrets
 * to use for authenticating ourselves and/or the peer.
 */
void
auth_reset(unit)
    int unit;
{
    lcp_options *go = &lcp_gotoptions[unit];
    lcp_options *ao = &lcp_allowoptions[0];

    ao->neg_upap = !refuse_pap && (passwd[0] != 0 || get_pap_passwd(NULL));
    ao->neg_chap = !refuse_chap
	&& (passwd[0] != 0
	    || have_chap_secret(user, (explicit_remote? remote_name: NULL),
				0, NULL));

    if (go->neg_upap && !uselogin && !have_pap_secret(NULL))
	go->neg_upap = 0;
    if (go->neg_chap) {
	if (!have_chap_secret((explicit_remote? remote_name: NULL),
			      our_name, 1, NULL))
	    go->neg_chap = 0;
    }
}


/*
 * check_passwd - Check the user name and passwd against the PAP secrets
 * file.  If requested, also check against the system password database,
 * and login the user if OK.
 *
 * returns:
 *	UPAP_AUTHNAK: Authentication failed.
 *	UPAP_AUTHACK: Authentication succeeded.
 * In either case, msg points to an appropriate message.
 */
int
check_passwd(unit, auser, userlen, apasswd, passwdlen, msg)
    int unit;
    char *auser;
    int userlen;
    char *apasswd;
    int passwdlen;
    char **msg;
{
    int ret;
    char *filename;
    FILE *f;
    struct wordlist *addrs = NULL, *opts = NULL;
    char passwd[256], user[256];
    char secret[MAXWORDLEN];
    static int attempts = 0;

    /*
     * Make copies of apasswd and auser, then null-terminate them.
     * If there are unprintable characters in the password, make
     * them visible.
     */
    slprintf(passwd, sizeof(passwd), "%.*v", passwdlen, apasswd);
    slprintf(user, sizeof(user), "%.*v", userlen, auser);
    *msg = "";

    /*
     * Check if a plugin wants to handle this.
     */
    if (pap_auth_hook) {
	ret = (*pap_auth_hook)(user, passwd, msg, &addrs, &opts);
	if (ret >= 0) {
	    if (ret)
		set_allowed_addrs(unit, addrs, opts);
	    BZERO(passwd, sizeof(passwd));
	    if (addrs != 0)
		free_wordlist(addrs);
	    return ret? UPAP_AUTHACK: UPAP_AUTHNAK;
	}
    }

    /*
     * Open the file of pap secrets and scan for a suitable secret
     * for authenticating this user.
     */
    filename = _PATH_UPAPFILE;
    addrs = opts = NULL;
    ret = UPAP_AUTHNAK;
    f = fopen(filename, "r");
    if (f == NULL) {
	error("Can't open PAP password file %s: %m", filename);

    } else {
	check_access(f, filename);
	if (scan_authfile(f, user, our_name, secret, &addrs, &opts, filename) < 0) {
	    warn("no PAP secret found for %s", user);
	} else {
	    /*
	     * If the secret is "@login", it means to check
	     * the password against the login database.
	     */
	    int login_secret = strcmp(secret, "@login") == 0;
	    ret = UPAP_AUTHACK;
	    if (uselogin || login_secret) {
		/* login option or secret is @login */
		ret = plogin(user, passwd, msg);
		if (ret == UPAP_AUTHNAK)
		    warn("PAP login failure for %s", user);
		else
		    used_login = 1;
	    }
	    if (secret[0] != 0 && !login_secret) {
		/* password given in pap-secrets - must match */
		if ((cryptpap || strcmp(passwd, secret) != 0)
		    && strcmp(crypt(passwd, secret), secret) != 0) {
		    ret = UPAP_AUTHNAK;
		    warn("PAP authentication failure for %s", user);
		}
	    }
	}
	fclose(f);
    }

    if (ret == UPAP_AUTHNAK) {
        if (**msg == 0)
	    *msg = "Login incorrect";
	/*
	 * XXX can we ever get here more than once??
	 * Frustrate passwd stealer programs.
	 * Allow 10 tries, but start backing off after 3 (stolen from login).
	 * On 10'th, drop the connection.
	 */
	if (attempts++ >= 10) {
	    warn("%d LOGIN FAILURES ON %s, %s", attempts, devnam, user);
	    lcp_close(unit, "login failed");
	}
	if (attempts > 3)
	    sleep((u_int) (attempts - 3) * 5);
	if (opts != NULL)
	    free_wordlist(opts);

    } else {
	attempts = 0;			/* Reset count */
	if (**msg == 0)
	    *msg = "Login ok";
	set_allowed_addrs(unit, addrs, opts);
    }

    if (addrs != NULL)
	free_wordlist(addrs);
    BZERO(passwd, sizeof(passwd));
    BZERO(secret, sizeof(secret));

    return ret;
}

/*
 * This function is needed for PAM.
 */

#ifdef USE_PAM
/* Static variables used to communicate between the conversation function
 * and the server_login function 
 */
static char *PAM_username;
static char *PAM_password;
static int PAM_error = 0;
static pam_handle_t *pamh = NULL;

/* PAM conversation function
 * Here we assume (for now, at least) that echo on means login name, and
 * echo off means password.
 */

static int PAM_conv (int num_msg, const struct pam_message **msg,
                    struct pam_response **resp, void *appdata_ptr)
{
    int replies = 0;
    struct pam_response *reply = NULL;

#define COPY_STRING(s) (s) ? strdup(s) : NULL

    reply = malloc(sizeof(struct pam_response) * num_msg);
    if (!reply) return PAM_CONV_ERR;

    for (replies = 0; replies < num_msg; replies++) {
        switch (msg[replies]->msg_style) {
            case PAM_PROMPT_ECHO_ON:
                reply[replies].resp_retcode = PAM_SUCCESS;
                reply[replies].resp = COPY_STRING(PAM_username);
                /* PAM frees resp */
                break;
            case PAM_PROMPT_ECHO_OFF:
                reply[replies].resp_retcode = PAM_SUCCESS;
                reply[replies].resp = COPY_STRING(PAM_password);
                /* PAM frees resp */
                break;
            case PAM_TEXT_INFO:
                /* fall through */
            case PAM_ERROR_MSG:
                /* ignore it, but pam still wants a NULL response... */
                reply[replies].resp_retcode = PAM_SUCCESS;
                reply[replies].resp = NULL;
                break;
            default:       
                /* Must be an error of some sort... */
                free (reply);
                PAM_error = 1;
                return PAM_CONV_ERR;
        }
    }
    *resp = reply;     
    return PAM_SUCCESS;
}

static struct pam_conv PAM_conversation = {
    &PAM_conv,
    NULL
};
#endif  /* USE_PAM */

/*
 * plogin - Check the user name and password against the system
 * password database, and login the user if OK.
 *
 * returns:
 *	UPAP_AUTHNAK: Login failed.
 *	UPAP_AUTHACK: Login succeeded.
 * In either case, msg points to an appropriate message.
 */

static int
plogin(user, passwd, msg)
    char *user;
    char *passwd;
    char **msg;
{
    char *tty;

#ifdef USE_PAM
    int pam_error;

    pam_error = pam_start ("ppp", user, &PAM_conversation, &pamh);
    if (pam_error != PAM_SUCCESS) {
        *msg = (char *) pam_strerror (pamh, pam_error);
	reopen_log();
	return UPAP_AUTHNAK;
    }
    /*
     * Define the fields for the credential validation
     */
     
    PAM_username = user;
    PAM_password = passwd;
    PAM_error = 0;
    pam_set_item (pamh, PAM_TTY, devnam); /* this might be useful to some modules */

    /*
     * Validate the user
     */
    pam_error = pam_authenticate (pamh, PAM_SILENT);
    if (pam_error == PAM_SUCCESS && !PAM_error) {    
        pam_error = pam_acct_mgmt (pamh, PAM_SILENT);
        if (pam_error == PAM_SUCCESS)
	    pam_open_session (pamh, PAM_SILENT);
    }

    *msg = (char *) pam_strerror (pamh, pam_error);

    /*
     * Clean up the mess
     */
    reopen_log();	/* apparently the PAM stuff does closelog() */
    PAM_username = NULL;
    PAM_password = NULL;
    if (pam_error != PAM_SUCCESS)
        return UPAP_AUTHNAK;
#else /* #ifdef USE_PAM */

/*
 * Use the non-PAM methods directly
 */

#ifdef HAS_SHADOW
    struct spwd *spwd;
    struct spwd *getspnam();
#endif
    struct passwd *pw = getpwnam(user);

    endpwent();
    if (pw == NULL)
	return (UPAP_AUTHNAK);

#ifdef HAS_SHADOW
    spwd = getspnam(user);
    endspent();
    if (spwd) {
	/* check the age of the password entry */
	long now = time(NULL) / 86400L;

	if ((spwd->sp_expire > 0 && now >= spwd->sp_expire)
	    || ((spwd->sp_max >= 0 && spwd->sp_max < 10000)
		&& spwd->sp_lstchg >= 0
		&& now >= spwd->sp_lstchg + spwd->sp_max)) {
	    warn("Password for %s has expired", user);
	    return (UPAP_AUTHNAK);
	}
	pw->pw_passwd = spwd->sp_pwdp;
    }
#endif

    /*
     * If no passwd, don't let them login.
     */
    if (pw->pw_passwd == NULL || strlen(pw->pw_passwd) < 2
	|| strcmp(crypt(passwd, pw->pw_passwd), pw->pw_passwd) != 0)
	return (UPAP_AUTHNAK);

#endif /* #ifdef USE_PAM */

    /*
     * Write a wtmp entry for this user.
     */

    tty = devnam;
    if (strncmp(tty, "/dev/", 5) == 0)
	tty += 5;
    logwtmp(tty, user, remote_name);		/* Add wtmp login entry */

#if defined(_PATH_LASTLOG) && !defined(USE_PAM)
    if (pw != (struct passwd *)NULL) {
	    struct lastlog ll;
	    int fd;

	    if ((fd = open(_PATH_LASTLOG, O_RDWR, 0)) >= 0) {
		(void)lseek(fd, (off_t)(pw->pw_uid * sizeof(ll)), SEEK_SET);
		memset((void *)&ll, 0, sizeof(ll));
		(void)time(&ll.ll_time);
		(void)strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
		(void)write(fd, (char *)&ll, sizeof(ll));
		(void)close(fd);
	    }
    }
#endif /* _PATH_LASTLOG and not USE_PAM */

    info("user %s logged in", user);
    logged_in = 1;

    return (UPAP_AUTHACK);
}

/*
 * plogout - Logout the user.
 */
static void
plogout()
{
#ifdef USE_PAM
    int pam_error;

    if (pamh != NULL) {
	pam_error = pam_close_session (pamh, PAM_SILENT);
	pam_end (pamh, pam_error);
	pamh = NULL;
    }
    /* Apparently the pam stuff does closelog(). */
    reopen_log();
#else /* ! USE_PAM */   
    char *tty;

    tty = devnam;
    if (strncmp(tty, "/dev/", 5) == 0)
	tty += 5;
    logwtmp(tty, "", "");		/* Wipe out utmp logout entry */
#endif /* ! USE_PAM */
    logged_in = 0;
}


/*
 * null_login - Check if a username of "" and a password of "" are
 * acceptable, and iff so, set the list of acceptable IP addresses
 * and return 1.
 */
static int
null_login(unit)
    int unit;
{
    char *filename;
    FILE *f;
    int i, ret;
    struct wordlist *addrs, *opts;
    char secret[MAXWORDLEN];

    /*
     * Open the file of pap secrets and scan for a suitable secret.
     */
    filename = _PATH_UPAPFILE;
    addrs = NULL;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;
    check_access(f, filename);

    i = scan_authfile(f, "", our_name, secret, &addrs, &opts, filename);
    ret = i >= 0 && secret[0] == 0;
    BZERO(secret, sizeof(secret));

    if (ret)
	set_allowed_addrs(unit, addrs, opts);
    else if (opts != 0)
	free_wordlist(opts);
    if (addrs != 0)
	free_wordlist(addrs);

    fclose(f);
    return ret;
}


/*
 * get_pap_passwd - get a password for authenticating ourselves with
 * our peer using PAP.  Returns 1 on success, 0 if no suitable password
 * could be found.
 * Assumes passwd points to MAXSECRETLEN bytes of space (if non-null).
 */
static int
get_pap_passwd(passwd)
    char *passwd;
{
    char *filename;
    FILE *f;
    int ret;
    char secret[MAXWORDLEN];

    /*
     * Check whether a plugin wants to supply this.
     */
    if (pap_passwd_hook) {
	ret = (*pap_passwd_hook)(user, passwd);
	if (ret >= 0)
	    return ret;
    }

    filename = _PATH_UPAPFILE;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;
    check_access(f, filename);
    ret = scan_authfile(f, user,
			(remote_name[0]? remote_name: NULL),
			secret, NULL, NULL, filename);
    fclose(f);
    if (ret < 0)
	return 0;
    if (passwd != NULL)
	strlcpy(passwd, secret, MAXSECRETLEN);
    BZERO(secret, sizeof(secret));
    return 1;
}


/*
 * have_pap_secret - check whether we have a PAP file with any
 * secrets that we could possibly use for authenticating the peer.
 */
static int
have_pap_secret(lacks_ipp)
    int *lacks_ipp;
{
    FILE *f;
    int ret;
    char *filename;
    struct wordlist *addrs;

    /* let the plugin decide, if there is one */
    if (pap_check_hook) {
	ret = (*pap_check_hook)();
	if (ret >= 0)
	    return ret;
    }

    filename = _PATH_UPAPFILE;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;

    ret = scan_authfile(f, (explicit_remote? remote_name: NULL), our_name,
			NULL, &addrs, NULL, filename);
    fclose(f);
    if (ret >= 0 && !some_ip_ok(addrs)) {
	if (lacks_ipp != 0)
	    *lacks_ipp = 1;
	ret = -1;
    }
    if (addrs != 0)
	free_wordlist(addrs);

    return ret >= 0;
}


/*
 * have_chap_secret - check whether we have a CHAP file with a
 * secret that we could possibly use for authenticating `client'
 * on `server'.  Either can be the null string, meaning we don't
 * know the identity yet.
 */
static int
have_chap_secret(client, server, need_ip, lacks_ipp)
    char *client;
    char *server;
    int need_ip;
    int *lacks_ipp;
{
    FILE *f;
    int ret;
    char *filename;
    struct wordlist *addrs;

    filename = _PATH_CHAPFILE;
    f = fopen(filename, "r");
    if (f == NULL)
	return 0;

    if (client != NULL && client[0] == 0)
	client = NULL;
    else if (server != NULL && server[0] == 0)
	server = NULL;

    ret = scan_authfile(f, client, server, NULL, &addrs, NULL, filename);
    fclose(f);
    if (ret >= 0 && need_ip && !some_ip_ok(addrs)) {
	if (lacks_ipp != 0)
	    *lacks_ipp = 1;
	ret = -1;
    }
    if (addrs != 0)
	free_wordlist(addrs);

    return ret >= 0;
}


/*
 * get_secret - open the CHAP secret file and return the secret
 * for authenticating the given client on the given server.
 * (We could be either client or server).
 */
int
get_secret(unit, client, server, secret, secret_len, am_server)
    int unit;
    char *client;
    char *server;
    char *secret;
    int *secret_len;
    int am_server;
{
    FILE *f;
    int ret, len;
    char *filename;
    struct wordlist *addrs, *opts;
    char secbuf[MAXWORDLEN];

    if (!am_server && passwd[0] != 0) {
	strlcpy(secbuf, passwd, sizeof(secbuf));
    } else {
	filename = _PATH_CHAPFILE;
	addrs = NULL;
	secbuf[0] = 0;

	f = fopen(filename, "r");
	if (f == NULL) {
	    error("Can't open chap secret file %s: %m", filename);
	    return 0;
	}
	check_access(f, filename);

	ret = scan_authfile(f, client, server, secbuf, &addrs, &opts, filename);
	fclose(f);
	if (ret < 0)
	    return 0;

	if (am_server)
	    set_allowed_addrs(unit, addrs, opts);
	else if (opts != 0)
	    free_wordlist(opts);
	if (addrs != 0)
	    free_wordlist(addrs);
    }

    len = strlen(secbuf);
    if (len > MAXSECRETLEN) {
	error("Secret for %s on %s is too long", client, server);
	len = MAXSECRETLEN;
    }
    BCOPY(secbuf, secret, len);
    BZERO(secbuf, sizeof(secbuf));
    *secret_len = len;

    return 1;
}

/*
 * set_allowed_addrs() - set the list of allowed addresses.
 * Also looks for `--' indicating options to apply for this peer
 * and leaves the following words in extra_options.
 */
static void
set_allowed_addrs(unit, addrs, opts)
    int unit;
    struct wordlist *addrs;
    struct wordlist *opts;
{
    int n;
    struct wordlist *ap, **plink;
    struct permitted_ip *ip;
    char *ptr_word, *ptr_mask;
    struct hostent *hp;
    struct netent *np;
    u_int32_t a, mask, ah, offset;
    struct ipcp_options *wo = &ipcp_wantoptions[unit];
    u_int32_t suggested_ip = 0;

    if (addresses[unit] != NULL)
	free(addresses[unit]);
    addresses[unit] = NULL;
    if (extra_options != NULL)
	free_wordlist(extra_options);
    extra_options = opts;

    /*
     * Count the number of IP addresses given.
     */
    n = wordlist_count(addrs) + wordlist_count(noauth_addrs);
    if (n == 0)
	return;
    ip = (struct permitted_ip *) malloc((n + 1) * sizeof(struct permitted_ip));
    if (ip == 0)
	return;

    /* temporarily append the noauth_addrs list to addrs */
    for (plink = &addrs; *plink != NULL; plink = &(*plink)->next)
	;
    *plink = noauth_addrs;

    n = 0;
    for (ap = addrs; ap != NULL; ap = ap->next) {
	/* "-" means no addresses authorized, "*" means any address allowed */
	ptr_word = ap->word;
	if (strcmp(ptr_word, "-") == 0)
	    break;
	if (strcmp(ptr_word, "*") == 0) {
	    ip[n].permit = 1;
	    ip[n].base = ip[n].mask = 0;
	    ++n;
	    break;
	}

	ip[n].permit = 1;
	if (*ptr_word == '!') {
	    ip[n].permit = 0;
	    ++ptr_word;
	}

	mask = ~ (u_int32_t) 0;
	offset = 0;
	ptr_mask = strchr (ptr_word, '/');
	if (ptr_mask != NULL) {
	    int bit_count;
	    char *endp;

	    bit_count = (int) strtol (ptr_mask+1, &endp, 10);
	    if (bit_count <= 0 || bit_count > 32) {
		warn("invalid address length %v in auth. address list",
		     ptr_mask+1);
		continue;
	    }
	    bit_count = 32 - bit_count;	/* # bits in host part */
	    if (*endp == '+') {
		offset = ifunit + 1;
		++endp;
	    }
	    if (*endp != 0) {
		warn("invalid address length syntax: %v", ptr_mask+1);
		continue;
	    }
	    *ptr_mask = '\0';
	    mask <<= bit_count;
	}

	hp = gethostbyname(ptr_word);
	if (hp != NULL && hp->h_addrtype == AF_INET) {
	    a = *(u_int32_t *)hp->h_addr;
	} else {
	    np = getnetbyname (ptr_word);
	    if (np != NULL && np->n_addrtype == AF_INET) {
		a = htonl (*(u_int32_t *)np->n_net);
		if (ptr_mask == NULL) {
		    /* calculate appropriate mask for net */
		    ah = ntohl(a);
		    if (IN_CLASSA(ah))
			mask = IN_CLASSA_NET;
		    else if (IN_CLASSB(ah))
			mask = IN_CLASSB_NET;
		    else if (IN_CLASSC(ah))
			mask = IN_CLASSC_NET;
		}
	    } else {
		a = inet_addr (ptr_word);
	    }
	}

	if (ptr_mask != NULL)
	    *ptr_mask = '/';

	if (a == (u_int32_t)-1L) {
	    warn("unknown host %s in auth. address list", ap->word);
	    continue;
	}
	if (offset != 0) {
	    if (offset >= ~mask) {
		warn("interface unit %d too large for subnet %v",
		     ifunit, ptr_word);
		continue;
	    }
	    a = htonl((ntohl(a) & mask) + offset);
	    mask = ~(u_int32_t)0;
	}
	ip[n].mask = htonl(mask);
	ip[n].base = a & ip[n].mask;
	++n;
	if (~mask == 0 && suggested_ip == 0)
	    suggested_ip = a;
    }
    *plink = NULL;

    ip[n].permit = 0;		/* make the last entry forbid all addresses */
    ip[n].base = 0;		/* to terminate the list */
    ip[n].mask = 0;

    addresses[unit] = ip;

    /*
     * If the address given for the peer isn't authorized, or if
     * the user hasn't given one, AND there is an authorized address
     * which is a single host, then use that if we find one.
     */
    if (suggested_ip != 0
	&& (wo->hisaddr == 0 || !auth_ip_addr(unit, wo->hisaddr)))
	wo->hisaddr = suggested_ip;
}

/*
 * auth_ip_addr - check whether the peer is authorized to use
 * a given IP address.  Returns 1 if authorized, 0 otherwise.
 */
int
auth_ip_addr(unit, addr)
    int unit;
    u_int32_t addr;
{
    int ok;

    /* don't allow loopback or multicast address */
    if (bad_ip_adrs(addr))
	return 0;

    if (addresses[unit] != NULL) {
	ok = ip_addr_check(addr, addresses[unit]);
	if (ok >= 0)
	    return ok;
    }
    if (auth_required)
	return 0;		/* no addresses authorized */
    return allow_any_ip || privileged || !have_route_to(addr);
}

static int
ip_addr_check(addr, addrs)
    u_int32_t addr;
    struct permitted_ip *addrs;
{
    for (; ; ++addrs)
	if ((addr & addrs->mask) == addrs->base)
	    return addrs->permit;
}

/*
 * bad_ip_adrs - return 1 if the IP address is one we don't want
 * to use, such as an address in the loopback net or a multicast address.
 * addr is in network byte order.
 */
int
bad_ip_adrs(addr)
    u_int32_t addr;
{
    addr = ntohl(addr);
    return (addr >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET
	|| IN_MULTICAST(addr) || IN_BADCLASS(addr);
}

/*
 * some_ip_ok - check a wordlist to see if it authorizes any
 * IP address(es).
 */
static int
some_ip_ok(addrs)
    struct wordlist *addrs;
{
    for (; addrs != 0; addrs = addrs->next) {
	if (addrs->word[0] == '-')
	    break;
	if (addrs->word[0] != '!')
	    return 1;		/* some IP address is allowed */
    }
    return 0;
}

/*
 * check_access - complain if a secret file has too-liberal permissions.
 */
static void
check_access(f, filename)
    FILE *f;
    char *filename;
{
    struct stat sbuf;

    if (fstat(fileno(f), &sbuf) < 0) {
	warn("cannot stat secret file %s: %m", filename);
    } else if ((sbuf.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
	warn("Warning - secret file %s has world and/or group access",
	     filename);
    }
}


/*
 * scan_authfile - Scan an authorization file for a secret suitable
 * for authenticating `client' on `server'.  The return value is -1
 * if no secret is found, otherwise >= 0.  The return value has
 * NONWILD_CLIENT set if the secret didn't have "*" for the client, and
 * NONWILD_SERVER set if the secret didn't have "*" for the server.
 * Any following words on the line up to a "--" (i.e. address authorization
 * info) are placed in a wordlist and returned in *addrs.  Any
 * following words (extra options) are placed in a wordlist and
 * returned in *opts.
 * We assume secret is NULL or points to MAXWORDLEN bytes of space.
 */
static int
scan_authfile(f, client, server, secret, addrs, opts, filename)
    FILE *f;
    char *client;
    char *server;
    char *secret;
    struct wordlist **addrs;
    struct wordlist **opts;
    char *filename;
{
    int newline, xxx;
    int got_flag, best_flag;
    FILE *sf;
    struct wordlist *ap, *addr_list, *alist, **app;
    char word[MAXWORDLEN];
    char atfile[MAXWORDLEN];
    char lsecret[MAXWORDLEN];

    if (addrs != NULL)
	*addrs = NULL;
    if (opts != NULL)
	*opts = NULL;
    addr_list = NULL;
    if (!getword(f, word, &newline, filename))
	return -1;		/* file is empty??? */
    newline = 1;
    best_flag = -1;
    for (;;) {
	/*
	 * Skip until we find a word at the start of a line.
	 */
	while (!newline && getword(f, word, &newline, filename))
	    ;
	if (!newline)
	    break;		/* got to end of file */

	/*
	 * Got a client - check if it's a match or a wildcard.
	 */
	got_flag = 0;
	if (client != NULL && strcmp(word, client) != 0 && !ISWILD(word)) {
	    newline = 0;
	    continue;
	}
	if (!ISWILD(word))
	    got_flag = NONWILD_CLIENT;

	/*
	 * Now get a server and check if it matches.
	 */
	if (!getword(f, word, &newline, filename))
	    break;
	if (newline)
	    continue;
	if (!ISWILD(word)) {
	    if (server != NULL && strcmp(word, server) != 0)
		continue;
	    got_flag |= NONWILD_SERVER;
	}

	/*
	 * Got some sort of a match - see if it's better than what
	 * we have already.
	 */
	if (got_flag <= best_flag)
	    continue;

	/*
	 * Get the secret.
	 */
	if (!getword(f, word, &newline, filename))
	    break;
	if (newline)
	    continue;

	if (secret != NULL) {
	    /*
	     * Special syntax: @/pathname means read secret from file.
	     */
	    if (word[0] == '@' && word[1] == '/') {
		strlcpy(atfile, word+1, sizeof(atfile));
		if ((sf = fopen(atfile, "r")) == NULL) {
		    warn("can't open indirect secret file %s", atfile);
		    continue;
		}
		check_access(sf, atfile);
		if (!getword(sf, word, &xxx, atfile)) {
		    warn("no secret in indirect secret file %s", atfile);
		    fclose(sf);
		    continue;
		}
		fclose(sf);
	    }
	    strlcpy(lsecret, word, sizeof(lsecret));
	}

	/*
	 * Now read address authorization info and make a wordlist.
	 */
	app = &alist;
	for (;;) {
	    if (!getword(f, word, &newline, filename) || newline)
		break;
	    ap = (struct wordlist *) malloc(sizeof(struct wordlist));
	    if (ap == NULL)
		novm("authorized addresses");
	    ap->word = strdup(word);
	    if (ap->word == NULL)
		novm("authorized addresses");
	    *app = ap;
	    app = &ap->next;
	}
	*app = NULL;

	/*
	 * This is the best so far; remember it.
	 */
	best_flag = got_flag;
	if (addr_list)
	    free_wordlist(addr_list);
	addr_list = alist;
	if (secret != NULL)
	    strlcpy(secret, lsecret, MAXWORDLEN);

	if (!newline)
	    break;
    }

    /* scan for a -- word indicating the start of options */
    for (app = &addr_list; (ap = *app) != NULL; app = &ap->next)
	if (strcmp(ap->word, "--") == 0)
	    break;
    /* ap = start of options */
    if (ap != NULL) {
	ap = ap->next;		/* first option */
	free(*app);			/* free the "--" word */
	*app = NULL;		/* terminate addr list */
    }
    if (opts != NULL)
	*opts = ap;
    else if (ap != NULL)
	free_wordlist(ap);
    if (addrs != NULL)
	*addrs = addr_list;
    else if (addr_list != NULL)
	free_wordlist(addr_list);

    return best_flag;
}

/*
 * wordlist_count - return the number of items in a wordlist
 */
static int
wordlist_count(wp)
    struct wordlist *wp;
{
    int n;

    for (n = 0; wp != NULL; wp = wp->next)
	++n;
    return n;
}

/*
 * free_wordlist - release memory allocated for a wordlist.
 */
static void
free_wordlist(wp)
    struct wordlist *wp;
{
    struct wordlist *next;

    while (wp != NULL) {
	next = wp->next;
	free(wp);
	wp = next;
    }
}

/*
 * auth_script_done - called when the auth-up or auth-down script
 * has finished.
 */
static void
auth_script_done(arg)
    void *arg;
{
    auth_script_pid = 0;
    switch (auth_script_state) {
    case s_up:
	if (auth_state == s_down) {
	    auth_script_state = s_down;
	    auth_script(_PATH_AUTHDOWN);
	}
	break;
    case s_down:
	if (auth_state == s_up) {
	    auth_script_state = s_up;
	    auth_script(_PATH_AUTHUP);
	}
	break;
    }
}

/*
 * auth_script - execute a script with arguments
 * interface-name peer-name real-user tty speed
 */
static void
auth_script(script)
    char *script;
{
    char strspeed[32];
    struct passwd *pw;
    char struid[32];
    char *user_name;
    char *argv[8];

    if ((pw = getpwuid(getuid())) != NULL && pw->pw_name != NULL)
	user_name = pw->pw_name;
    else {
	slprintf(struid, sizeof(struid), "%d", getuid());
	user_name = struid;
    }
    slprintf(strspeed, sizeof(strspeed), "%d", baud_rate);

    argv[0] = script;
    argv[1] = ifname;
    argv[2] = peer_authname;
    argv[3] = user_name;
    argv[4] = devnam;
    argv[5] = strspeed;
    argv[6] = NULL;

    auth_script_pid = run_program(script, argv, 0, auth_script_done, NULL);
}
