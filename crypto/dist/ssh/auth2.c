/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: auth2.c,v 1.42 2001/02/13 22:49:40 markus Exp $");

#include <openssl/evp.h>

#include "ssh2.h"
#include "xmalloc.h"
#include "rsa.h"
#include "pty.h"
#include "packet.h"
#include "buffer.h"
#include "log.h"
#include "servconf.h"
#include "compat.h"
#include "channels.h"
#include "bufaux.h"
#include "auth.h"
#include "session.h"
#include "dispatch.h"
#include "key.h"
#include "cipher.h"
#include "kex.h"
#include "pathnames.h"
#include "uidswap.h"
#include "auth-options.h"

/* import */
extern ServerOptions options;
extern u_char *session_id2;
extern int session_id2_len;

static Authctxt	*x_authctxt = NULL;
static int one = 1;

typedef struct Authmethod Authmethod;
struct Authmethod {
	char	*name;
	int	(*userauth)(Authctxt *authctxt);
	int	*enabled;
};

/* protocol */

void	input_service_request(int type, int plen, void *ctxt);
void	input_userauth_request(int type, int plen, void *ctxt);
void	protocol_error(int type, int plen, void *ctxt);

/* helper */
Authmethod	*authmethod_lookup(const char *name);
struct passwd	*pwcopy(struct passwd *pw);
int	user_key_allowed(struct passwd *pw, Key *key);
char	*authmethods_get(void);

/* auth */
void	userauth_banner(void);
int	userauth_none(Authctxt *authctxt);
int	userauth_passwd(Authctxt *authctxt);
int	userauth_pubkey(Authctxt *authctxt);
int	userauth_kbdint(Authctxt *authctxt);

Authmethod authmethods[] = {
	{"none",
		userauth_none,
		&one},
	{"publickey",
		userauth_pubkey,
		&options.pubkey_authentication},
	{"password",
		userauth_passwd,
		&options.password_authentication},
	{"keyboard-interactive",
		userauth_kbdint,
		&options.kbd_interactive_authentication},
	{NULL, NULL, NULL}
};

/*
 * loop until authctxt->success == TRUE
 */

void
do_authentication2()
{
	Authctxt *authctxt = authctxt_new();

	x_authctxt = authctxt;		/*XXX*/

	/* challenge-reponse is implemented via keyboard interactive */
	if (options.challenge_reponse_authentication)
		options.kbd_interactive_authentication = 1;

	dispatch_init(&protocol_error);
	dispatch_set(SSH2_MSG_SERVICE_REQUEST, &input_service_request);
	dispatch_run(DISPATCH_BLOCK, &authctxt->success, authctxt);
	do_authenticated2(authctxt);
}

void
protocol_error(int type, int plen, void *ctxt)
{
	log("auth: protocol error: type %d plen %d", type, plen);
	packet_start(SSH2_MSG_UNIMPLEMENTED);
	packet_put_int(0);
	packet_send();
	packet_write_wait();
}

void
input_service_request(int type, int plen, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	u_int len;
	int accept = 0;
	char *service = packet_get_string(&len);
	packet_done();

	if (authctxt == NULL)
		fatal("input_service_request: no authctxt");

	if (strcmp(service, "ssh-userauth") == 0) {
		if (!authctxt->success) {
			accept = 1;
			/* now we can handle user-auth requests */
			dispatch_set(SSH2_MSG_USERAUTH_REQUEST, &input_userauth_request);
		}
	}
	/* XXX all other service requests are denied */

	if (accept) {
		packet_start(SSH2_MSG_SERVICE_ACCEPT);
		packet_put_cstring(service);
		packet_send();
		packet_write_wait();
	} else {
		debug("bad service request %s", service);
		packet_disconnect("bad service request %s", service);
	}
	xfree(service);
}

void
input_userauth_request(int type, int plen, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	Authmethod *m = NULL;
	char *user, *service, *method, *style = NULL;
	int authenticated = 0;

	if (authctxt == NULL)
		fatal("input_userauth_request: no authctxt");

	user = packet_get_string(NULL);
	service = packet_get_string(NULL);
	method = packet_get_string(NULL);
	debug("userauth-request for user %s service %s method %s", user, service, method);
	debug("attempt %d failures %d", authctxt->attempt, authctxt->failures);

	if ((style = strchr(user, ':')) != NULL)
		*style++ = 0;

	if (authctxt->attempt++ == 0) {
		/* setup auth context */
		struct passwd *pw = NULL;
		pw = getpwnam(user);
		if (pw && allowed_user(pw) && strcmp(service, "ssh-connection")==0) {
			authctxt->pw = pwcopy(pw);
			authctxt->valid = 1;
			debug2("input_userauth_request: setting up authctxt for %s", user);
		} else {
			log("input_userauth_request: illegal user %s", user);
		}
		setproctitle("%s", pw ? user : "unknown");
		authctxt->user = xstrdup(user);
		authctxt->service = xstrdup(service);
		authctxt->style = style ? xstrdup(style) : NULL; /* currently unused */
	} else if (authctxt->valid) {
		if (strcmp(user, authctxt->user) != 0 ||
		    strcmp(service, authctxt->service) != 0) {
			log("input_userauth_request: missmatch: (%s,%s)!=(%s,%s)",
			    user, service, authctxt->user, authctxt->service);
			authctxt->valid = 0;
		}
	}
	/* reset state */
	dispatch_set(SSH2_MSG_USERAUTH_INFO_RESPONSE, &protocol_error);
	authctxt->postponed = 0;

	/* try to authenticate user */
	m = authmethod_lookup(method);
	if (m != NULL) {
		debug2("input_userauth_request: try method %s", method);
		authenticated =	m->userauth(authctxt);
	}
	if (!authctxt->valid && authenticated)
		fatal("INTERNAL ERROR: authenticated invalid user %s",
		    authctxt->user);

	/* Special handling for root */
	if (authenticated && authctxt->pw->pw_uid == 0 &&
	    !auth_root_allowed(method))
		authenticated = 0;

	/* Log before sending the reply */
	auth_log(authctxt, authenticated, method, " ssh2");

	if (!authctxt->postponed)
		userauth_reply(authctxt, authenticated);

	xfree(service);
	xfree(user);
	xfree(method);
}

void
userauth_banner(void)
{
	struct stat st;
	char *banner = NULL;
	off_t len, n;
	int fd;

	if (options.banner == NULL || (datafellows & SSH_BUG_BANNER))
		return;
	if ((fd = open(options.banner, O_RDONLY)) < 0) {
		error("userauth_banner: open %s failed: %s",
		    options.banner, strerror(errno));
		return;
	}
	if (fstat(fd, &st) < 0)
		goto done;
	len = st.st_size;
	banner = xmalloc(len + 1);
	if ((n = read(fd, banner, len)) < 0)
		goto done;
	banner[n] = '\0';
	packet_start(SSH2_MSG_USERAUTH_BANNER);
	packet_put_cstring(banner);
	packet_put_cstring("");		/* language, unused */
	packet_send();
	debug("userauth_banner: sent");
done:
	if (banner)
		xfree(banner);
	close(fd);
	return;
}

void
userauth_reply(Authctxt *authctxt, int authenticated)
{
	char *methods;

	/* XXX todo: check if multiple auth methods are needed */
	if (authenticated == 1) {
		/* turn off userauth */
		dispatch_set(SSH2_MSG_USERAUTH_REQUEST, &protocol_error);
		packet_start(SSH2_MSG_USERAUTH_SUCCESS);
		packet_send();
		packet_write_wait();
		/* now we can break out */
		authctxt->success = 1;
	} else {
		if (authctxt->failures++ > AUTH_FAIL_MAX)
			packet_disconnect(AUTH_FAIL_MSG, authctxt->user);
		methods = authmethods_get();
		packet_start(SSH2_MSG_USERAUTH_FAILURE);
		packet_put_cstring(methods);
		packet_put_char(0);	/* XXX partial success, unused */
		packet_send();
		packet_write_wait();
		xfree(methods);
	}
}

int
userauth_none(Authctxt *authctxt)
{
	/* disable method "none", only allowed one time */
	Authmethod *m = authmethod_lookup("none");
	if (m != NULL)
		m->enabled = NULL;
	packet_done();
	userauth_banner();
	return authctxt->valid ? auth_password(authctxt->pw, "") : 0;
}

int
userauth_passwd(Authctxt *authctxt)
{
	char *password;
	int authenticated = 0;
	int change;
	u_int len;
	change = packet_get_char();
	if (change)
		log("password change not supported");
	password = packet_get_string(&len);
	packet_done();
	if (authctxt->valid &&
	    auth_password(authctxt->pw, password) == 1)
		authenticated = 1;
	memset(password, 0, len);
	xfree(password);
	return authenticated;
}

int
userauth_kbdint(Authctxt *authctxt)
{
	int authenticated = 0;
	char *lang = NULL;
	char *devs = NULL;

	lang = packet_get_string(NULL);
	devs = packet_get_string(NULL);
	packet_done();

	debug("keyboard-interactive language %s devs %s", lang, devs);

	if (options.challenge_reponse_authentication)
		authenticated = auth2_challenge(authctxt, devs);

	xfree(lang);
	xfree(devs);
	return authenticated;
}

int
userauth_pubkey(Authctxt *authctxt)
{
	Buffer b;
	Key *key;
	char *pkalg, *pkblob, *sig;
	u_int alen, blen, slen;
	int have_sig, pktype;
	int authenticated = 0;

	if (!authctxt->valid) {
		debug2("userauth_pubkey: disabled because of invalid user");
		return 0;
	}
	have_sig = packet_get_char();
	if (datafellows & SSH_BUG_PKAUTH) {
		debug2("userauth_pubkey: SSH_BUG_PKAUTH");
		/* no explicit pkalg given */
		pkblob = packet_get_string(&blen);
		buffer_init(&b);
		buffer_append(&b, pkblob, blen);
		/* so we have to extract the pkalg from the pkblob */
		pkalg = buffer_get_string(&b, &alen);
		buffer_free(&b);
	} else {
		pkalg = packet_get_string(&alen);
		pkblob = packet_get_string(&blen);
	}
	pktype = key_type_from_name(pkalg);
	if (pktype == KEY_UNSPEC) {
		/* this is perfectly legal */
		log("userauth_pubkey: unsupported public key algorithm: %s", pkalg);
		xfree(pkalg);
		xfree(pkblob);
		return 0;
	}
	key = key_from_blob(pkblob, blen);
	if (key != NULL) {
		if (have_sig) {
			sig = packet_get_string(&slen);
			packet_done();
			buffer_init(&b);
			if (datafellows & SSH_OLD_SESSIONID) {
				buffer_append(&b, session_id2, session_id2_len);
			} else {
				buffer_put_string(&b, session_id2, session_id2_len);
			}
			/* reconstruct packet */
			buffer_put_char(&b, SSH2_MSG_USERAUTH_REQUEST);
			buffer_put_cstring(&b, authctxt->user);
			buffer_put_cstring(&b,
			    datafellows & SSH_BUG_PKSERVICE ?
			    "ssh-userauth" :
			    authctxt->service);
			if (datafellows & SSH_BUG_PKAUTH) {
				buffer_put_char(&b, have_sig);
			} else {
				buffer_put_cstring(&b, "publickey");
				buffer_put_char(&b, have_sig);
				buffer_put_cstring(&b, key_ssh_name(key));
			}
			buffer_put_string(&b, pkblob, blen);
#ifdef DEBUG_PK
			buffer_dump(&b);
#endif
			/* test for correct signature */
			if (user_key_allowed(authctxt->pw, key) &&
			    key_verify(key, sig, slen, buffer_ptr(&b), buffer_len(&b)) == 1)
				authenticated = 1;
			buffer_clear(&b);
			xfree(sig);
		} else {
			debug("test whether pkalg/pkblob are acceptable");
			packet_done();

			/* XXX fake reply and always send PK_OK ? */
			/*
			 * XXX this allows testing whether a user is allowed
			 * to login: if you happen to have a valid pubkey this
			 * message is sent. the message is NEVER sent at all
			 * if a user is not allowed to login. is this an
			 * issue? -markus
			 */
			if (user_key_allowed(authctxt->pw, key)) {
				packet_start(SSH2_MSG_USERAUTH_PK_OK);
				packet_put_string(pkalg, alen);
				packet_put_string(pkblob, blen);
				packet_send();
				packet_write_wait();
				authctxt->postponed = 1;
			}
		}
		if (authenticated != 1)
			auth_clear_options();
		key_free(key);
	}
	debug2("userauth_pubkey: authenticated %d pkalg %s", authenticated, pkalg);
	xfree(pkalg);
	xfree(pkblob);
	return authenticated;
}

/* get current user */

struct passwd*
auth_get_user(void)
{
	return (x_authctxt != NULL && x_authctxt->valid) ? x_authctxt->pw : NULL;
}

#define	DELIM	","

char *
authmethods_get(void)
{
	Authmethod *method = NULL;
	u_int size = 0;
	char *list;

	for (method = authmethods; method->name != NULL; method++) {
		if (strcmp(method->name, "none") == 0)
			continue;
		if (method->enabled != NULL && *(method->enabled) != 0) {
			if (size != 0)
				size += strlen(DELIM);
			size += strlen(method->name);
		}
	}
	size++;			/* trailing '\0' */
	list = xmalloc(size);
	list[0] = '\0';

	for (method = authmethods; method->name != NULL; method++) {
		if (strcmp(method->name, "none") == 0)
			continue;
		if (method->enabled != NULL && *(method->enabled) != 0) {
			if (list[0] != '\0')
				strlcat(list, DELIM, size);
			strlcat(list, method->name, size);
		}
	}
	return list;
}

Authmethod *
authmethod_lookup(const char *name)
{
	Authmethod *method = NULL;
	if (name != NULL)
		for (method = authmethods; method->name != NULL; method++)
			if (method->enabled != NULL &&
			    *(method->enabled) != 0 &&
			    strcmp(name, method->name) == 0)
				return method;
	debug2("Unrecognized authentication method name: %s", name ? name : "NULL");
	return NULL;
}

/* return 1 if user allows given key */
int
user_key_allowed(struct passwd *pw, Key *key)
{
	char line[8192], file[MAXPATHLEN];
	int found_key = 0;
	FILE *f;
	u_long linenum = 0;
	struct stat st;
	Key *found;

	if (pw == NULL)
		return 0;

	/* Temporarily use the user's uid. */
	temporarily_use_uid(pw->pw_uid);

	/* The authorized keys. */
	snprintf(file, sizeof file, "%.500s/%.100s", pw->pw_dir,
	    _PATH_SSH_USER_PERMITTED_KEYS2);

	/* Fail quietly if file does not exist */
	if (stat(file, &st) < 0) {
		/* Restore the privileged uid. */
		restore_uid();
		return 0;
	}
	/* Open the file containing the authorized keys. */
	f = fopen(file, "r");
	if (!f) {
		/* Restore the privileged uid. */
		restore_uid();
		return 0;
	}
	if (options.strict_modes) {
		int fail = 0;
		char buf[1024];
		/* Check open file in order to avoid open/stat races */
		if (fstat(fileno(f), &st) < 0 ||
		    (st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
		    (st.st_mode & 022) != 0) {
			snprintf(buf, sizeof buf,
			    "%s authentication refused for %.100s: "
			    "bad ownership or modes for '%s'.",
			    key_type(key), pw->pw_name, file);
			fail = 1;
		} else {
			/* Check path to _PATH_SSH_USER_PERMITTED_KEYS */
			int i;
			static const char *check[] = {
				"", _PATH_SSH_USER_DIR, NULL
			};
			for (i = 0; check[i]; i++) {
				snprintf(line, sizeof line, "%.500s/%.100s",
				    pw->pw_dir, check[i]);
				if (stat(line, &st) < 0 ||
				    (st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
				    (st.st_mode & 022) != 0) {
					snprintf(buf, sizeof buf,
					    "%s authentication refused for %.100s: "
					    "bad ownership or modes for '%s'.",
					    key_type(key), pw->pw_name, line);
					fail = 1;
					break;
				}
			}
		}
		if (fail) {
			fclose(f);
			log("%s",buf);
			restore_uid();
			return 0;
		}
	}
	found_key = 0;
	found = key_new(key->type);

	while (fgets(line, sizeof(line), f)) {
		char *cp, *options = NULL;
		linenum++;
		/* Skip leading whitespace, empty and comment lines. */
		for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '\n' || *cp == '#')
			continue;

		if (key_read(found, &cp) == -1) {
			/* no key?  check if there are options for this key */
			int quoted = 0;
			debug2("user_key_allowed: check options: '%s'", cp);
			options = cp;
			for (; *cp && (quoted || (*cp != ' ' && *cp != '\t')); cp++) {
				if (*cp == '\\' && cp[1] == '"')
					cp++;	/* Skip both */
				else if (*cp == '"')
					quoted = !quoted;
			}
			/* Skip remaining whitespace. */
			for (; *cp == ' ' || *cp == '\t'; cp++)
				;
			if (key_read(found, &cp) == -1) {
				debug2("user_key_allowed: advance: '%s'", cp);
				/* still no key?  advance to next line*/
				continue;
			}
		}
		if (key_equal(found, key) &&
		    auth_parse_options(pw, options, file, linenum) == 1) {
			found_key = 1;
			debug("matching key found: file %s, line %ld",
			    file, linenum);
			break;
		}
	}
	restore_uid();
	fclose(f);
	key_free(found);
	return found_key;
}
