/*
 * Copyright (c) 2000 Markus Friedl. All rights reserved.
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

#include "includes.h"
RCSID("$OpenBSD: auth.c,v 1.17 2001/02/12 16:16:23 markus Exp $");

#include "xmalloc.h"
#include "match.h"
#include "groupaccess.h"
#include "log.h"
#include "servconf.h"
#include "auth.h"
#include "auth-options.h"
#include "canohost.h"

#ifdef HAVE_LOGIN_CAP
#include <login_cap.h>
#endif

/* import */
extern ServerOptions options;

/*
 * Check if the user is allowed to log in via ssh. If user is listed
 * in DenyUsers or one of user's groups is listed in DenyGroups, false
 * will be returned. If AllowUsers isn't empty and user isn't listed
 * there, or if AllowGroups isn't empty and one of user's groups isn't
 * listed there, false will be returned.
 * If the user's shell is not executable, false will be returned.
 * Otherwise true is returned.
 */
int
allowed_user(struct passwd * pw)
{
	struct stat st;
	char *shell;
	int i;
#ifdef HAVE_LOGIN_CAP
	int match_name, match_ip;
	login_cap_t *lc;
	const char *hostname, *ipaddr;
	char *cap_hlist, *hp;
#endif

	/* Shouldn't be called if pw is NULL, but better safe than sorry... */
	if (!pw || !pw->pw_name)
		return 0;

#ifdef HAVE_LOGIN_CAP
	hostname = get_canonical_hostname(1);
	ipaddr = get_remote_ipaddr();

	lc = login_getclass(pw->pw_class);

	/*
	 * Check the deny list.
	 */
	cap_hlist = login_getcapstr(lc, "host.deny", NULL, NULL);
	if (cap_hlist != NULL) {
		hp = strtok(cap_hlist, ",");
		while (hp != NULL) {
			match_name = match_hostname(hostname,
			    hp, strlen(hp));
			match_ip = match_hostname(ipaddr,
			    hp, strlen(hp));
			/*
			 * Only a positive match here causes a "deny".
			 */
			if (match_name > 0 || match_ip > 0) {
				free(cap_hlist);
				login_close(lc);
				return 0;
			}
			hp = strtok(NULL, ",");
		}
		free(cap_hlist);
	}

	/*
	 * Check the allow list.  If the allow list exists, and the
	 * remote host is not in it, the user is implicitly denied.
	 */
	cap_hlist = login_getcapstr(lc, "host.allow", NULL, NULL);
	if (cap_hlist != NULL) {
		hp = strtok(cap_hlist, ",");
		if (hp == NULL) {
			/* Just in case there's an empty string... */
			free(cap_hlist);
			login_close(lc);
			return 0;
		}
		while (hp != NULL) {
			match_name = match_hostname(hostname,
			    hp, strlen(hp));
			match_ip = match_hostname(ipaddr,
			    hp, strlen(hp));
			/*
			 * Negative match causes an immediate "deny".
			 * Positive match causes us to break out
			 * of the loop (allowing a fallthrough).
			 */
			if (match_name < 0 || match_ip < 0) {
				free(cap_hlist);
				login_close(lc);
				return 0;
			}
			if (match_name > 0 || match_ip > 0)
				break;
		}
		free(cap_hlist);
		if (hp == NULL) {
			login_close(lc);
			return 0;
		}
	}

	login_close(lc);
#endif

	/*
	 * Get the shell from the password data.  An empty shell field is
	 * legal, and means /bin/sh.
	 */
	shell = (pw->pw_shell[0] == '\0') ? _PATH_BSHELL : pw->pw_shell;

	/*
	 * deny if shell does not exists or is not executable.
	 * XXX Should check to see if it is executable by the
	 * XXX requesting user.  --thorpej
	 */
	if (stat(shell, &st) != 0)
		return 0;
	if (S_ISREG(st.st_mode) == 0 ||
	    (st.st_mode & (S_IXOTH|S_IXUSR|S_IXGRP)) == 0)
		return 0;
	/*
	 * XXX Consider nuking {Allow,Deny}{Users,Groups}.  We have the
	 * XXX login_cap(3) mechanism which covers all other types of
	 * XXX logins, too.
	 */

	/* Return false if user is listed in DenyUsers */
	if (options.num_deny_users > 0) {
		for (i = 0; i < options.num_deny_users; i++)
			if (match_pattern(pw->pw_name, options.deny_users[i]))
				return 0;
	}
	/* Return false if AllowUsers isn't empty and user isn't listed there */
	if (options.num_allow_users > 0) {
		for (i = 0; i < options.num_allow_users; i++)
			if (match_pattern(pw->pw_name, options.allow_users[i]))
				break;
		/* i < options.num_allow_users iff we break for loop */
		if (i >= options.num_allow_users)
			return 0;
	}
	if (options.num_deny_groups > 0 || options.num_allow_groups > 0) {
		/* Get the user's group access list (primary and supplementary) */
		if (ga_init(pw->pw_name, pw->pw_gid) == 0)
			return 0;

		/* Return false if one of user's groups is listed in DenyGroups */
		if (options.num_deny_groups > 0)
			if (ga_match(options.deny_groups,
			    options.num_deny_groups)) {
				ga_free();
				return 0;
			}
		/*
		 * Return false if AllowGroups isn't empty and one of user's groups
		 * isn't listed there
		 */
		if (options.num_allow_groups > 0)
			if (!ga_match(options.allow_groups,
			    options.num_allow_groups)) {
				ga_free();
				return 0;
			}
		ga_free();
	}
	/* We found no reason not to let this user try to log on... */
	return 1;
}

Authctxt *
authctxt_new(void)
{
	Authctxt *authctxt = xmalloc(sizeof(*authctxt));
	memset(authctxt, 0, sizeof(*authctxt));
	return authctxt;
}

struct passwd *
pwcopy(struct passwd *pw)
{
	struct passwd *copy = xmalloc(sizeof(*copy));
	memset(copy, 0, sizeof(*copy));
	copy->pw_name = xstrdup(pw->pw_name);
	copy->pw_passwd = xstrdup(pw->pw_passwd);
	copy->pw_uid = pw->pw_uid;
	copy->pw_gid = pw->pw_gid;
	copy->pw_class = xstrdup(pw->pw_class);
	copy->pw_dir = xstrdup(pw->pw_dir);
	copy->pw_shell = xstrdup(pw->pw_shell);
	return copy;
}

void
auth_log(Authctxt *authctxt, int authenticated, char *method, char *info)
{
	void (*authlog) (const char *fmt,...) = verbose;
	char *authmsg;

	/* Raise logging level */
	if (authenticated == 1 ||
	    !authctxt->valid ||
	    authctxt->failures >= AUTH_FAIL_LOG ||
	    strcmp(method, "password") == 0)
		authlog = log;

	if (authctxt->postponed)
		authmsg = "Postponed";
	else
		authmsg = authenticated ? "Accepted" : "Failed";

	authlog("%s %s for %s%.100s from %.200s port %d%s",
	    authmsg,
	    method,
	    authctxt->valid ? "" : "illegal user ",
	    authctxt->valid && authctxt->pw->pw_uid == 0 ? "ROOT" : authctxt->user,
	    get_remote_ipaddr(),
	    get_remote_port(),
	    info);
}

/*
 * Check whether root logins are disallowed.
 */
int
auth_root_allowed(char *method)
{
	switch (options.permit_root_login) {
	case PERMIT_YES:
		return 1;
		break;
	case PERMIT_NO_PASSWD:
		if (strcmp(method, "password") != 0)
			return 1;
		break;
	case PERMIT_FORCED_ONLY:
		if (forced_command) {
			log("Root login accepted for forced command.");
			return 1;
		}
		break;
	}
	log("ROOT LOGIN REFUSED FROM %.200s", get_remote_ipaddr());
	return 0;
}
