/*
 * Copyright (c) 1997-2000 Kungliga Tekniska H�gskolan
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

RCSID("$Id: recvauth.c,v 1.1.1.1.2.1 2001/04/05 23:23:14 he Exp $");

/*
 * See `sendauth.c' for the format.
 */

static krb5_boolean
match_exact(void *data, const char *appl_version)
{
    return strcmp(data, appl_version) == 0;
}

krb5_error_code
krb5_recvauth(krb5_context context,
	      krb5_auth_context *auth_context,
	      krb5_pointer p_fd,
	      char *appl_version,
	      krb5_principal server,
	      int32_t flags,
	      krb5_keytab keytab,
	      krb5_ticket **ticket)
{
    return krb5_recvauth_match_version(context, auth_context, p_fd,
				       match_exact, appl_version,
				       server, flags,
				       keytab, ticket);
}

krb5_error_code
krb5_recvauth_match_version(krb5_context context,
			    krb5_auth_context *auth_context,
			    krb5_pointer p_fd,
			    krb5_boolean (*match_appl_version)(void *, 
							       const char*),
			    void *match_data,
			    krb5_principal server,
			    int32_t flags,
			    krb5_keytab keytab,
			    krb5_ticket **ticket)
{
  krb5_error_code ret;
  const char *version = KRB5_SENDAUTH_VERSION;
  char her_version[sizeof(KRB5_SENDAUTH_VERSION)];
  char *her_appl_version;
  u_int32_t len;
  u_char repl;
  krb5_data data;
  krb5_flags ap_options;
  ssize_t n;

  /*
   * If there are no addresses in auth_context, get them from `fd'.
   */

  if (*auth_context == NULL) {
      ret = krb5_auth_con_init (context, auth_context);
      if (ret)
	  return ret;
  }

  ret = krb5_auth_con_setaddrs_from_fd (context,
					*auth_context,
					p_fd);
  if (ret)
      return ret;

  if(!(flags & KRB5_RECVAUTH_IGNORE_VERSION)) {
    n = krb5_net_read (context, p_fd, &len, 4);
    if (n < 0)
	return errno;
    if (n == 0)
	return KRB5_SENDAUTH_BADAUTHVERS;
    len = ntohl(len);
    if (len != sizeof(her_version)
	|| krb5_net_read (context, p_fd, her_version, len) != len
	|| strncmp (version, her_version, len)) {
      repl = 1;
      krb5_net_write (context, p_fd, &repl, 1);
      return KRB5_SENDAUTH_BADAUTHVERS;
    }
  }

  n = krb5_net_read (context, p_fd, &len, 4);
  if (n < 0)
      return errno;
  if (n == 0)
      return KRB5_SENDAUTH_BADAPPLVERS;
  len = ntohl(len);
  her_appl_version = malloc (len);
  if (her_appl_version == NULL) {
      repl = 2;
      krb5_net_write (context, p_fd, &repl, 1);
      return ENOMEM;
  }
  if (krb5_net_read (context, p_fd, her_appl_version, len) != len
      || !(*match_appl_version)(match_data, her_appl_version)) {
    repl = 2;
    krb5_net_write (context, p_fd, &repl, 1);
    free (her_appl_version);
    return KRB5_SENDAUTH_BADAPPLVERS;
  }
  free (her_appl_version);

  repl = 0;
  if (krb5_net_write (context, p_fd, &repl, 1) != 1)
    return errno;

  krb5_data_zero (&data);
  ret = krb5_read_message (context, p_fd, &data);
  if (ret)
      return ret;

  ret = krb5_rd_req (context,
		     auth_context,
		     &data,
		     server,
		     keytab,
		     &ap_options,
		     ticket);
  krb5_data_free (&data);
  if (ret) {
      krb5_data error_data;
      krb5_error_code ret2;

      ret2 = krb5_mk_error (context,
			    ret,
			    NULL,
			    NULL,
			    NULL,
			    server,
			    0,
			    &error_data);
      if (ret2 == 0) {
	  krb5_write_message (context, p_fd, &error_data);
	  krb5_data_free (&error_data);
      }
      return ret;
  }      

  len = 0;
  if (krb5_net_write (context, p_fd, &len, 4) != 4)
    return errno;

  if (ap_options & AP_OPTS_MUTUAL_REQUIRED) {
    ret = krb5_mk_rep (context, *auth_context, &data);
    if (ret)
      return ret;

    ret = krb5_write_message (context, p_fd, &data);
    if (ret)
	return ret;
    krb5_data_free (&data);
  }
  return 0;
}
