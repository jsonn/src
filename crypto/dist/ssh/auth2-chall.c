/*	$NetBSD: auth2-chall.c,v 1.1.1.1.2.4 2002/06/26 19:30:59 he Exp $	*/
/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Per Allansson.  All rights reserved.
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
RCSID("$OpenBSD: auth2-chall.c,v 1.8 2001/09/27 15:31:17 markus Exp $");

#include "ssh2.h"
#include "auth.h"
#include "packet.h"
#include "xmalloc.h"
#include "dispatch.h"
#include "auth.h"
#include "log.h"

static int auth2_challenge_start(Authctxt *);
static int send_userauth_info_request(Authctxt *);
static void input_userauth_info_response(int, int, void *);

#ifdef BSD_AUTH
extern KbdintDevice bsdauth_device;
#else
#ifdef SKEY
extern KbdintDevice skey_device;
#endif
#endif

KbdintDevice *devices[] = {
#ifdef BSD_AUTH
	&bsdauth_device,
#else
#ifdef SKEY
	&skey_device,
#endif
#endif
	NULL
};

typedef struct KbdintAuthctxt KbdintAuthctxt;
struct KbdintAuthctxt
{
	char *devices;
	void *ctxt;
	KbdintDevice *device;
};

static KbdintAuthctxt *
kbdint_alloc(const char *devs)
{
	KbdintAuthctxt *kbdintctxt;
	int i;
	char buf[1024];

	kbdintctxt = xmalloc(sizeof(KbdintAuthctxt));
	if (strcmp(devs, "") == 0) {
		buf[0] = '\0';
		for (i = 0; devices[i]; i++) {
			if (i != 0)
				strlcat(buf, ",", sizeof(buf));
			strlcat(buf, devices[i]->name, sizeof(buf));
		}
		debug("kbdint_alloc: devices '%s'", buf);
		kbdintctxt->devices = xstrdup(buf);
	} else {
		kbdintctxt->devices = xstrdup(devs);
	}
	kbdintctxt->ctxt = NULL;
	kbdintctxt->device = NULL;

	return kbdintctxt;
}
static void
kbdint_reset_device(KbdintAuthctxt *kbdintctxt)
{
	if (kbdintctxt->ctxt) {
		kbdintctxt->device->free_ctx(kbdintctxt->ctxt);
		kbdintctxt->ctxt = NULL;
	}
	kbdintctxt->device = NULL;
}
static void
kbdint_free(KbdintAuthctxt *kbdintctxt)
{
	if (kbdintctxt->device)
		kbdint_reset_device(kbdintctxt);
	if (kbdintctxt->devices) {
		xfree(kbdintctxt->devices);
		kbdintctxt->devices = NULL;
	}
	xfree(kbdintctxt);
}
/* get next device */
static int
kbdint_next_device(KbdintAuthctxt *kbdintctxt)
{
	size_t len;
	char *t;
	int i;

	if (kbdintctxt->device)
		kbdint_reset_device(kbdintctxt);
	do {
		len = kbdintctxt->devices ?
		    strcspn(kbdintctxt->devices, ",") : 0;

		if (len == 0)
			break;
		for (i = 0; devices[i]; i++)
			if (strncmp(kbdintctxt->devices, devices[i]->name, len) == 0)
				kbdintctxt->device = devices[i];
		t = kbdintctxt->devices;
		kbdintctxt->devices = t[len] ? xstrdup(t+len+1) : NULL;
		xfree(t);
		debug2("kbdint_next_device: devices %s", kbdintctxt->devices ?
		   kbdintctxt->devices : "<empty>");
	} while (kbdintctxt->devices && !kbdintctxt->device);

	return kbdintctxt->device ? 1 : 0;
}

/*
 * try challenge-response, set authctxt->postponed if we have to
 * wait for the response.
 */
int
auth2_challenge(Authctxt *authctxt, char *devs)
{
	debug("auth2_challenge: user=%s devs=%s",
	    authctxt->user ? authctxt->user : "<nouser>",
	    devs ? devs : "<no devs>");

	if (authctxt->user == NULL || !devs)
		return 0;
	if (authctxt->kbdintctxt == NULL) 
		authctxt->kbdintctxt = kbdint_alloc(devs);
	return auth2_challenge_start(authctxt);
}

/* side effect: sets authctxt->postponed if a reply was sent*/
static int
auth2_challenge_start(Authctxt *authctxt)
{
	KbdintAuthctxt *kbdintctxt = authctxt->kbdintctxt;

	debug2("auth2_challenge_start: devices %s",
	    kbdintctxt->devices ?  kbdintctxt->devices : "<empty>");

	if (kbdint_next_device(kbdintctxt) == 0) {
		kbdint_free(kbdintctxt);
		authctxt->kbdintctxt = NULL;
		return 0;
	}
	debug("auth2_challenge_start: trying authentication method '%s'",
	    kbdintctxt->device->name);

	if ((kbdintctxt->ctxt = kbdintctxt->device->init_ctx(authctxt)) == NULL) {
		kbdint_free(kbdintctxt);
		authctxt->kbdintctxt = NULL;
		return 0;
	}
	if (send_userauth_info_request(authctxt) == 0) {
		kbdint_free(kbdintctxt);
		authctxt->kbdintctxt = NULL;
		return 0;
	}
	dispatch_set(SSH2_MSG_USERAUTH_INFO_RESPONSE,
	    &input_userauth_info_response);

	authctxt->postponed = 1;
	return 0;
}

static int
send_userauth_info_request(Authctxt *authctxt)
{
	KbdintAuthctxt *kbdintctxt;
	char *name, *instr, **prompts;
	int i;
	u_int numprompts, *echo_on;

	kbdintctxt = authctxt->kbdintctxt;
	if (kbdintctxt->device->query(kbdintctxt->ctxt,
	    &name, &instr, &numprompts, &prompts, &echo_on))
		return 0;

	packet_start(SSH2_MSG_USERAUTH_INFO_REQUEST);
	packet_put_cstring(name);
	packet_put_cstring(instr);
	packet_put_cstring(""); 	/* language not used */
	packet_put_int(numprompts);
	for (i = 0; i < numprompts; i++) {
		packet_put_cstring(prompts[i]);
		packet_put_char(echo_on[i]);
	}
	packet_send();
	packet_write_wait();

	for (i = 0; i < numprompts; i++)
		xfree(prompts[i]);
	xfree(prompts);
	xfree(echo_on);
	xfree(name);
	xfree(instr);
	return 1;
}

static void
input_userauth_info_response(int type, int plen, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	KbdintAuthctxt *kbdintctxt;
	int i, authenticated = 0, res, len;
	u_int nresp;
	char **response = NULL, *method;

	if (authctxt == NULL)
		fatal("input_userauth_info_response: no authctxt");
	kbdintctxt = authctxt->kbdintctxt;
	if (kbdintctxt == NULL || kbdintctxt->ctxt == NULL)
		fatal("input_userauth_info_response: no kbdintctxt");
	if (kbdintctxt->device == NULL)
		fatal("input_userauth_info_response: no device");

	authctxt->postponed = 0;	/* reset */
	nresp = packet_get_int();
	if (nresp > 100)
		fatal("input_userauth_info_response: nresp too big %u", nresp);
	if (nresp > 0) {
		response = xmalloc(nresp * sizeof(char*));
		for (i = 0; i < nresp; i++)
			response[i] = packet_get_string(NULL);
	}
	packet_done();

	if (authctxt->valid) {
		res = kbdintctxt->device->respond(kbdintctxt->ctxt,
		    nresp, response);
	} else {
		res = -1;
	}

	for (i = 0; i < nresp; i++) {
		memset(response[i], 'r', strlen(response[i]));
		xfree(response[i]);
	}
	if (response)
		xfree(response);

	switch (res) {
	case 0:
		/* Success! */
		authenticated = 1;
		break;
	case 1:
		/* Authentication needs further interaction */
		authctxt->postponed = 1;
		if (send_userauth_info_request(authctxt) == 0) {
			authctxt->postponed = 0;
		}
		break;
	default:
		/* Failure! */
		break;
	}

	len = strlen("keyboard-interactive") + 2 +
		strlen(kbdintctxt->device->name);
	method = xmalloc(len);
	method[0] = '\0';
	strlcat(method, "keyboard-interactive", len);
	strlcat(method, "/", len);
	strlcat(method, kbdintctxt->device->name, len);

	if (!authctxt->postponed) {
		/* unregister callback */
		dispatch_set(SSH2_MSG_USERAUTH_INFO_RESPONSE, NULL);

		if (authenticated) {
			kbdint_free(kbdintctxt);
			authctxt->kbdintctxt = NULL;
		} else {
			/* start next device */
			/* may set authctxt->postponed */
			auth2_challenge_start(authctxt);
		}
	}
	userauth_finish(authctxt, authenticated, method);
	xfree(method);
}
