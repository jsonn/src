/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska H�gskolan
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

RCSID("$Id: init_creds.c,v 1.1.1.1.4.2 2000/06/16 18:32:58 thorpej Exp $");

void
krb5_get_init_creds_opt_init(krb5_get_init_creds_opt *opt)
{
    memset (opt, 0, sizeof(*opt));
    opt->flags = 0;
}

void
krb5_get_init_creds_opt_set_tkt_life(krb5_get_init_creds_opt *opt,
				     krb5_deltat tkt_life)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_TKT_LIFE;
    opt->tkt_life = tkt_life;
}

void
krb5_get_init_creds_opt_set_renew_life(krb5_get_init_creds_opt *opt,
				       krb5_deltat renew_life)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE;
    opt->renew_life = renew_life;
}

void
krb5_get_init_creds_opt_set_forwardable(krb5_get_init_creds_opt *opt,
					int forwardable)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_FORWARDABLE;
    opt->forwardable = forwardable;
}

void
krb5_get_init_creds_opt_set_proxiable(krb5_get_init_creds_opt *opt,
				      int proxiable)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_PROXIABLE;
    opt->proxiable = proxiable;
}

void
krb5_get_init_creds_opt_set_etype_list(krb5_get_init_creds_opt *opt,
				       krb5_enctype *etype_list,
				       int etype_list_length)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST;
    opt->etype_list = etype_list;
    opt->etype_list_length = etype_list_length;
}

void
krb5_get_init_creds_opt_set_address_list(krb5_get_init_creds_opt *opt,
					 krb5_addresses *addresses)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST;
    opt->address_list = addresses;
}

void
krb5_get_init_creds_opt_set_preauth_list(krb5_get_init_creds_opt *opt,
					 krb5_preauthtype *preauth_list,
					 int preauth_list_length)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST;
    opt->preauth_list_length = preauth_list_length;
    opt->preauth_list = preauth_list;
}

void
krb5_get_init_creds_opt_set_salt(krb5_get_init_creds_opt *opt,
				 krb5_data *salt)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_SALT;
    opt->salt = salt;
}

void
krb5_get_init_creds_opt_set_anonymous(krb5_get_init_creds_opt *opt,
				      int anonymous)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_ANONYMOUS;
    opt->anonymous = anonymous;
}
