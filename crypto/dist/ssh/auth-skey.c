/*	$NetBSD: auth-skey.c,v 1.1.1.1.2.3 2001/12/10 23:55:09 he Exp $	*/
/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: auth-skey.c,v 1.12 2001/05/18 14:13:28 markus Exp $");

#ifdef SKEY

#include <skey.h>

#include "xmalloc.h"
#include "auth.h"

static void *
skey_init_ctx(Authctxt *authctxt)
{
	return authctxt;
}

#define PROMPT "\nS/Key Password: "

static int
skey_query(void *ctx, char **name, char **infotxt, 
    u_int* numprompts, char ***prompts, u_int **echo_on)
{
	Authctxt *authctxt = ctx;
	char challenge[1024], *p;
	int len;
	struct skey skey;

	if (skeychallenge(&skey, authctxt->user, challenge, sizeof(challenge)) == -1)
		return -1;

	*name       = xstrdup("");
	*infotxt    = xstrdup("");
	*numprompts = 1;
	*prompts = xmalloc(*numprompts * sizeof(char*));
	*echo_on = xmalloc(*numprompts * sizeof(u_int));
	(*echo_on)[0] = 0;

	len = strlen(challenge) + strlen(PROMPT) + 1;
	p = xmalloc(len);
	p[0] = '\0';
	strlcat(p, challenge, len);
	strlcat(p, PROMPT, len);
	(*prompts)[0] = p;

	return 0;
}

static int
skey_respond(void *ctx, u_int numresponses, char **responses)
{
	Authctxt *authctxt = ctx;
 
	if (authctxt->valid &&
	    numresponses == 1 && 
	    skey_haskey(authctxt->pw->pw_name) == 0 &&
	    skey_passcheck(authctxt->pw->pw_name, responses[0]) != -1)
	    return 0;
	return -1;
}

static void
skey_free_ctx(void *ctx)
{
	/* we don't have a special context */
}

KbdintDevice skey_device = {
	"skey",
	skey_init_ctx,
	skey_query,
	skey_respond,
	skey_free_ctx
};
#endif /* SKEY */
