/*
 * Copyright (c) 1997, 1998, 1999 Kungliga Tekniska H�gskolan
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

#include "gssapi_locl.h"

RCSID("$Id: init_sec_context.c,v 1.1.1.1.2.2 2001/01/25 13:55:58 jhawk Exp $");

static OM_uint32
init_auth
           (OM_uint32 * minor_status,
            const gss_cred_id_t initiator_cred_handle,
            gss_ctx_id_t * context_handle,
            const gss_name_t target_name,
            const gss_OID mech_type,
            OM_uint32 req_flags,
            OM_uint32 time_req,
            const gss_channel_bindings_t input_chan_bindings,
            const gss_buffer_t input_token,
            gss_OID * actual_mech_type,
            gss_buffer_t output_token,
            OM_uint32 * ret_flags,
            OM_uint32 * time_rec
           )
{
    OM_uint32 ret = GSS_S_FAILURE;
    krb5_error_code kret;
    krb5_flags ap_options;
    krb5_creds this_cred, *cred;
    krb5_data outbuf;
    krb5_ccache ccache;
    u_int32_t flags;
    Authenticator *auth;
    krb5_data authenticator;
    Checksum cksum;
    krb5_enctype enctype;

    output_token->length = 0;
    output_token->value  = NULL;

    outbuf.length = 0;
    outbuf.data   = NULL;

    *minor_status = 0;

    *context_handle = malloc(sizeof(**context_handle));
    if (*context_handle == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    (*context_handle)->auth_context =  NULL;
    (*context_handle)->source = NULL;
    (*context_handle)->target = NULL;
    (*context_handle)->flags = 0;
    (*context_handle)->more_flags = 0;
    (*context_handle)->ticket = NULL;

    kret = krb5_auth_con_init (gssapi_krb5_context,
			       &(*context_handle)->auth_context);
    if (kret) {
	*minor_status = kret;
	ret = GSS_S_FAILURE;
	goto failure;
    }

    {
	int32_t tmp;

	krb5_auth_con_getflags(gssapi_krb5_context,
			       (*context_handle)->auth_context,
			       &tmp);
	tmp |= KRB5_AUTH_CONTEXT_DO_SEQUENCE;
	krb5_auth_con_setflags(gssapi_krb5_context,
			       (*context_handle)->auth_context,
			       tmp);
    }

    if (actual_mech_type)
	*actual_mech_type = GSS_KRB5_MECHANISM;

    flags = 0;
    ap_options = 0;
    if (req_flags & GSS_C_DELEG_FLAG)
	;				/* XXX */
    if (req_flags & GSS_C_MUTUAL_FLAG) {
	flags |= GSS_C_MUTUAL_FLAG;
	ap_options |= AP_OPTS_MUTUAL_REQUIRED;
    }
    if (req_flags & GSS_C_REPLAY_FLAG)
	;				/* XXX */
    if (req_flags & GSS_C_SEQUENCE_FLAG)
	;				/* XXX */
    if (req_flags & GSS_C_ANON_FLAG)
	;				/* XXX */
    flags |= GSS_C_CONF_FLAG;
    flags |= GSS_C_INTEG_FLAG;
    flags |= GSS_C_SEQUENCE_FLAG;
    flags |= GSS_C_TRANS_FLAG;

    if (ret_flags)
	*ret_flags = flags;
    (*context_handle)->flags = flags;
    (*context_handle)->more_flags = LOCAL;

    kret = krb5_cc_default (gssapi_krb5_context, &ccache);
    if (kret) {
	*minor_status = kret;
	ret = GSS_S_FAILURE;
	goto failure;
    }

    kret = krb5_cc_get_principal (gssapi_krb5_context,
				  ccache,
				  &(*context_handle)->source);
    if (kret) {
	*minor_status = kret;
	ret = GSS_S_FAILURE;
	goto failure;
    }

    kret = krb5_copy_principal (gssapi_krb5_context,
				target_name,
				&(*context_handle)->target);
    if (kret) {
	*minor_status = kret;
	ret = GSS_S_FAILURE;
	goto failure;
    }

    memset(&this_cred, 0, sizeof(this_cred));
    this_cred.client          = (*context_handle)->source;
    this_cred.server          = (*context_handle)->target;
    this_cred.times.endtime   = 0;
    this_cred.session.keytype = ETYPE_DES_CBC_CRC;
  
    kret = krb5_get_credentials (gssapi_krb5_context,
				 KRB5_TC_MATCH_KEYTYPE,
				 ccache,
				 &this_cred,
				 &cred);

    if (kret) {
	*minor_status = kret;
	ret = GSS_S_FAILURE;
	goto failure;
    }

    krb5_auth_con_setkey(gssapi_krb5_context, 
			 (*context_handle)->auth_context, 
			 &cred->session);
  
    kret = gssapi_krb5_create_8003_checksum (input_chan_bindings,
					     flags,
					     &cksum);
    if (kret) {
	*minor_status = kret;
	ret = GSS_S_FAILURE;
	goto failure;
    }

#if 1
    enctype = (*context_handle)->auth_context->keyblock->keytype;
#else
    if ((*context_handle)->auth_context->enctype)
	enctype = (*context_handle)->auth_context->enctype;
    else {
	kret = krb5_keytype_to_enctype(gssapi_krb5_context,
				       (*context_handle)->auth_context->keyblock->keytype,
				       &enctype);
	if (kret)
	    return kret;
    }
#endif



    kret = krb5_build_authenticator (gssapi_krb5_context,
				     (*context_handle)->auth_context,
				     enctype,
				     cred,
				     &cksum,
				     &auth,
				     &authenticator);

    if (kret) {
	*minor_status = kret;
	ret = GSS_S_FAILURE;
	goto failure;
    }

    kret = krb5_build_ap_req (gssapi_krb5_context,
			      enctype,
			      cred,
			      ap_options,
			      authenticator,
			      &outbuf);

    if (kret) {
	*minor_status = kret;
	ret = GSS_S_FAILURE;
	goto failure;
    }

    ret = gssapi_krb5_encapsulate (&outbuf,
				   output_token,
				   "\x01\x00");
    if (ret) {
	*minor_status = kret;
	goto failure;
    }

    if (flags & GSS_C_MUTUAL_FLAG) {
	return GSS_S_CONTINUE_NEEDED;
    } else {
	(*context_handle)->more_flags |= OPEN;
	return GSS_S_COMPLETE;
    }

failure:
    krb5_auth_con_free (gssapi_krb5_context,
			(*context_handle)->auth_context);
    if((*context_handle)->source)
	krb5_free_principal (gssapi_krb5_context,
			     (*context_handle)->source);
    if((*context_handle)->target)
	krb5_free_principal (gssapi_krb5_context,
			     (*context_handle)->target);
    free (*context_handle);
    krb5_data_free (&outbuf);
    *context_handle = GSS_C_NO_CONTEXT;
    return ret;
}

static OM_uint32
repl_mutual
           (OM_uint32 * minor_status,
            const gss_cred_id_t initiator_cred_handle,
            gss_ctx_id_t * context_handle,
            const gss_name_t target_name,
            const gss_OID mech_type,
            OM_uint32 req_flags,
            OM_uint32 time_req,
            const gss_channel_bindings_t input_chan_bindings,
            const gss_buffer_t input_token,
            gss_OID * actual_mech_type,
            gss_buffer_t output_token,
            OM_uint32 * ret_flags,
            OM_uint32 * time_rec
           )
{
  OM_uint32 ret;
  krb5_error_code kret;
  krb5_data indata;
  krb5_ap_rep_enc_part *repl;

  ret = gssapi_krb5_decapsulate (input_token,
				 &indata,
				 "\x02\x00");
  if (ret) {
				/* XXX - Handle AP_ERROR */
    return GSS_S_FAILURE;
  }

  kret = krb5_rd_rep (gssapi_krb5_context,
		      (*context_handle)->auth_context,
		      &indata,
		      &repl);
  if (kret)
    return GSS_S_FAILURE;
  krb5_free_ap_rep_enc_part (gssapi_krb5_context,
			     repl);

  output_token->length = 0;

  (*context_handle)->more_flags |= OPEN;

  return GSS_S_COMPLETE;
}

/*
 * gss_init_sec_context
 */

OM_uint32 gss_init_sec_context
           (OM_uint32 * minor_status,
            const gss_cred_id_t initiator_cred_handle,
            gss_ctx_id_t * context_handle,
            const gss_name_t target_name,
            const gss_OID mech_type,
            OM_uint32 req_flags,
            OM_uint32 time_req,
            const gss_channel_bindings_t input_chan_bindings,
            const gss_buffer_t input_token,
            gss_OID * actual_mech_type,
            gss_buffer_t output_token,
            OM_uint32 * ret_flags,
            OM_uint32 * time_rec
           )
{
  gssapi_krb5_init ();

  if (input_token == GSS_C_NO_BUFFER || input_token->length == 0)
    return init_auth (minor_status,
		      initiator_cred_handle,
		      context_handle,
		      target_name,
		      mech_type,
		      req_flags,
		      time_req,
		      input_chan_bindings,
		      input_token,
		      actual_mech_type,
		      output_token,
		      ret_flags,
		      time_rec);
  else
    return repl_mutual(minor_status,
		       initiator_cred_handle,
		       context_handle,
		       target_name,
		       mech_type,
		       req_flags,
		       time_req,
		       input_chan_bindings,
		       input_token,
		       actual_mech_type,
		       output_token,
		       ret_flags,
		       time_rec);
}
