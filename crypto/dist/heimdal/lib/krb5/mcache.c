/*
 * Copyright (c) 1997-1999 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb5_locl.h"

RCSID("$Id: mcache.c,v 1.1.1.1.2.1 2000/08/11 20:36:15 thorpej Exp $");

typedef struct krb5_mcache {
    struct krb5_mcache *next;
    char *filename;
    unsigned int refcnt;
    int dead;
    krb5_principal primary_principal;
    struct link {
	krb5_creds cred;
	struct link *next;
    } *creds;
} krb5_mcache;

static struct krb5_mcache *mcc_head;

#define	MCACHE(X)	((krb5_mcache *)(X)->data.data)

#define	FILENAME(X)	(MCACHE(X)->filename)

#define MCC_CURSOR(C) ((struct link*)(C))

static char*
mcc_get_name(krb5_context context,
	     krb5_ccache id)
{
    return FILENAME(id);
}

static krb5_error_code
mcc_resolve(krb5_context context, krb5_ccache *id, const char *res)
{
    krb5_mcache *m;

    for (m = mcc_head; m != NULL; m = m->next)
	if (m->dead == 0 && strcmp(m->filename, res) == 0)
	    break;

    if (m != NULL) {
	m->refcnt++;
	(*id)->data.data = m;
	(*id)->data.length = sizeof(*m);
	return 0;
    }

    m = malloc(sizeof(*m));
    if (m == NULL)
	return KRB5_CC_NOMEM;

    m->filename = strdup(res);
    if (m->filename == NULL) {
	free(m);
	return KRB5_CC_NOMEM;
    }

    m->refcnt = 1;
    m->dead = 0;
    m->primary_principal = NULL;
    m->creds = NULL;
    (*id)->data.data = m;
    (*id)->data.length = sizeof(*m);

    m->next = mcc_head;
    mcc_head = m;

    return 0;
}

static krb5_error_code
mcc_gen_new(krb5_context context, krb5_ccache *id)
{
    krb5_mcache *m;
    char *file;

    m = malloc (sizeof(*m));
    if (m == NULL)
	return KRB5_CC_NOMEM;

    asprintf(&file, "%lX", (unsigned long)m);
    if (file == NULL) {
	free(m);
	return KRB5_CC_NOMEM;
    }

    m->filename = file;
    m->refcnt = 1;
    m->dead = 0;
    m->primary_principal = NULL;
    m->creds = NULL;
    (*id)->data.data = m;
    (*id)->data.length = sizeof(*m);

    m->next = mcc_head;
    mcc_head = m;

    return 0;
}

static krb5_error_code
mcc_initialize(krb5_context context,
	       krb5_ccache id,
	       krb5_principal primary_principal)
{
    krb5_mcache *m = MCACHE(id);
    krb5_error_code ret;

    ret = krb5_copy_principal (context,
			       primary_principal,
			       &m->primary_principal);
    if (ret)
	return ret;
    return 0;
}

static krb5_error_code
mcc_close(krb5_context context,
	  krb5_ccache id)
{
    krb5_mcache *m = MCACHE(id);

    if (--m->refcnt != 0)
	return 0;

    if (m->dead) {
	free(FILENAME(id));
	krb5_data_free(&id->data);
    }

    return 0;
}

static krb5_error_code
mcc_destroy(krb5_context context,
	    krb5_ccache id)
{
    krb5_mcache *n, *m = MCACHE(id);
    struct link *l;

    if (m->refcnt == 0)
	krb5_abortx(context, "mcc_destroy: refcnt already 0");

    if (m->dead == 0) {
	if (m == mcc_head)
	    mcc_head = m->next;
	else {
	    for (n = mcc_head; n != NULL; n = n->next) {
		if (n->next == m) {
		    n->next = m->next;
		    break;
		}
	    }
	}
    }

    if (m->primary_principal != NULL) {
	krb5_free_principal (context, m->primary_principal);
	m->primary_principal = NULL;
    }
    l = m->creds;
    while (l != NULL) {
	struct link *old;

	krb5_free_creds_contents (context, &l->cred);
	old = l;
	l = l->next;
	free (old);
    }
    m->creds = NULL;
    m->dead = 1;

    if (--m->refcnt != 0)
	return 0;

    free (FILENAME(id));
    krb5_data_free(&id->data);

    return 0;
}

static krb5_error_code
mcc_store_cred(krb5_context context,
	       krb5_ccache id,
	       krb5_creds *creds)
{
    krb5_mcache *m = MCACHE(id);
    krb5_error_code ret;
    struct link *l;

    if (m->dead)
	return ENOENT;

    l = malloc (sizeof(*l));
    if (l == NULL)
	return KRB5_CC_NOMEM;
    l->next = m->creds;
    m->creds = l;
    memset (&l->cred, 0, sizeof(l->cred));
    ret = krb5_copy_creds_contents (context, creds, &l->cred);
    if (ret) {
	m->creds = l->next;
	free (l);
	return ret;
    }
    return 0;
}

static krb5_error_code
mcc_get_principal(krb5_context context,
		  krb5_ccache id,
		  krb5_principal *principal)
{
    krb5_mcache *m = MCACHE(id);

    if (m->dead)
	return ENOENT;

    return krb5_copy_principal (context,
				m->primary_principal,
				principal);
}

static krb5_error_code
mcc_get_first (krb5_context context,
	       krb5_ccache id,
	       krb5_cc_cursor *cursor)
{
    krb5_mcache *m = MCACHE(id);

    if (m->dead)
	return ENOENT;

    *cursor = m->creds;
    return 0;
}

static krb5_error_code
mcc_get_next (krb5_context context,
	      krb5_ccache id,
	      krb5_cc_cursor *cursor,
	      krb5_creds *creds)
{
    krb5_mcache *m = MCACHE(id);
    struct link *l;

    if (m->dead)
	return ENOENT;

    l = *cursor;
    if (l != NULL) {
	*cursor = l->next;
	return krb5_copy_creds_contents (context,
					 &l->cred,
					 creds);
    } else
	return KRB5_CC_END;
}

static krb5_error_code
mcc_end_get (krb5_context context,
	     krb5_ccache id,
	     krb5_cc_cursor *cursor)
{
    return 0;
}

static krb5_error_code
mcc_remove_cred(krb5_context context,
		 krb5_ccache id,
		 krb5_flags which,
		 krb5_creds *cred)
{
    return 0; /* XXX */
}

static krb5_error_code
mcc_set_flags(krb5_context context,
	      krb5_ccache id,
	      krb5_flags flags)
{
    return 0; /* XXX */
}
		    
const krb5_cc_ops krb5_mcc_ops = {
    "MEMORY",
    mcc_get_name,
    mcc_resolve,
    mcc_gen_new,
    mcc_initialize,
    mcc_destroy,
    mcc_close,
    mcc_store_cred,
    NULL, /* mcc_retrieve */
    mcc_get_principal,
    mcc_get_first,
    mcc_get_next,
    mcc_end_get,
    mcc_remove_cred,
    mcc_set_flags
};
