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

#include <krb5_locl.h>

RCSID("$Id: changepw.c,v 1.1.1.1.2.1 2001/04/05 23:25:10 he Exp $");

static krb5_error_code
get_kdc_address (krb5_context context,
		 krb5_realm realm,
		 struct addrinfo **ai)
{
    krb5_error_code ret;
    char **hostlist;
    int port = 0;
    int error;

    ret = krb5_get_krb_changepw_hst (context,
				     &realm,
				     &hostlist);
    if (ret)
	return ret;

    port = ntohs(krb5_getportbyname (context, "kpasswd", "udp", KPASSWD_PORT));
    error = roken_getaddrinfo_hostspec2(*hostlist, SOCK_DGRAM, port, ai);

    krb5_free_krbhst (context, hostlist);
    if(error)
	return krb5_eai_to_heim_errno(error);
    return 0;
}

static krb5_error_code
send_request (krb5_context context,
	      krb5_auth_context *auth_context,
	      krb5_creds *creds,
	      int sock,
	      struct sockaddr *sa,
	      int sa_size,
	      char *passwd)
{
    krb5_error_code ret;
    krb5_data ap_req_data;
    krb5_data krb_priv_data;
    krb5_data passwd_data;
    size_t len;
    u_char header[6];
    u_char *p;
    struct iovec iov[3];
    struct msghdr msghdr;

    krb5_data_zero (&ap_req_data);

    ret = krb5_mk_req_extended (context,
				auth_context,
				AP_OPTS_MUTUAL_REQUIRED,
				NULL, /* in_data */
				creds,
				&ap_req_data);
    if (ret)
	return ret;

    passwd_data.data   = passwd;
    passwd_data.length = strlen(passwd);

    krb5_data_zero (&krb_priv_data);

    ret = krb5_mk_priv (context,
			*auth_context,
			&passwd_data,
			&krb_priv_data,
			NULL);
    if (ret)
	goto out2;

    len = 6 + ap_req_data.length + krb_priv_data.length;
    p = header;
    *p++ = (len >> 8) & 0xFF;
    *p++ = (len >> 0) & 0xFF;
    *p++ = 0;
    *p++ = 1;
    *p++ = (ap_req_data.length >> 8) & 0xFF;
    *p++ = (ap_req_data.length >> 0) & 0xFF;

    memset(&msghdr, 0, sizeof(msghdr));
    msghdr.msg_name       = (void *)sa;
    msghdr.msg_namelen    = sa_size;
    msghdr.msg_iov        = iov;
    msghdr.msg_iovlen     = sizeof(iov)/sizeof(*iov);
#if 0
    msghdr.msg_control    = NULL;
    msghdr.msg_controllen = 0;
#endif

    iov[0].iov_base    = (void*)header;
    iov[0].iov_len     = 6;
    iov[1].iov_base    = ap_req_data.data;
    iov[1].iov_len     = ap_req_data.length;
    iov[2].iov_base    = krb_priv_data.data;
    iov[2].iov_len     = krb_priv_data.length;

    if (sendmsg (sock, &msghdr, 0) < 0)
	ret = errno;

    krb5_data_free (&krb_priv_data);
out2:
    krb5_data_free (&ap_req_data);
    return ret;
}

static void
str2data (krb5_data *d,
	  const char *fmt,
	  ...) __attribute__ ((__format__ (__printf__, 2, 3)));

static void
str2data (krb5_data *d,
	  const char *fmt,
	  ...)
{
    va_list args;

    va_start(args, fmt);
    d->length = vasprintf ((char **)&d->data, fmt, args);
    va_end(args);
}

static krb5_error_code
process_reply (krb5_context context,
	       krb5_auth_context auth_context,
	       int sock,
	       int *result_code,
	       krb5_data *result_code_string,
	       krb5_data *result_string)
{
    krb5_error_code ret;
    u_char reply[BUFSIZ];
    size_t len;
    u_int16_t pkt_len, pkt_ver;
    krb5_data ap_rep_data;

    ret = recvfrom (sock, reply, sizeof(reply), 0, NULL, NULL);
    if (ret < 0)
	return errno;

    len = ret;
    pkt_len = (reply[0] << 8) | (reply[1]);
    pkt_ver = (reply[2] << 8) | (reply[3]);

    if (pkt_len != len) {
	str2data (result_string, "client: wrong len in reply");
	*result_code = KRB5_KPASSWD_MALFORMED;
	return 0;
    }
    if (pkt_ver != 0x0001) {
	str2data (result_string,
		  "client: wrong version number (%d)", pkt_ver);
	*result_code = KRB5_KPASSWD_MALFORMED;
	return 0;
    }

    ap_rep_data.data = reply + 6;
    ap_rep_data.length  = (reply[4] << 8) | (reply[5]);
  
    if (ap_rep_data.length) {
	krb5_ap_rep_enc_part *ap_rep;
	krb5_data priv_data;
	u_char *p;

	ret = krb5_rd_rep (context,
			   auth_context,
			   &ap_rep_data,
			   &ap_rep);
	if (ret)
	    return ret;

	krb5_free_ap_rep_enc_part (context, ap_rep);

	priv_data.data   = (u_char*)ap_rep_data.data + ap_rep_data.length;
	priv_data.length = len - ap_rep_data.length - 6;

	ret = krb5_rd_priv (context,
			    auth_context,
			    &priv_data,
			    result_code_string,
			    NULL);
	if (ret) {
	    krb5_data_free (result_code_string);
	    return ret;
	}

	if (result_code_string->length < 2) {
	    *result_code = KRB5_KPASSWD_MALFORMED;
	    str2data (result_string,
		      "client: bad length in result");
	    return 0;
	}
	p = result_code_string->data;
      
	*result_code = (p[0] << 8) | p[1];
	krb5_data_copy (result_string,
			(unsigned char*)result_code_string->data + 2,
			result_code_string->length - 2);
	return 0;
    } else {
	KRB_ERROR error;
	size_t size;
	u_char *p;
      
	ret = decode_KRB_ERROR(reply + 6, len - 6, &error, &size);
	if (ret) {
	    return ret;
	}
	if (error.e_data->length < 2) {
	    krb5_warnx (context, "too short e_data to print anything usable");
	    return 1;
	}

	p = error.e_data->data;
	*result_code = (p[0] << 8) | p[1];
	krb5_data_copy (result_string,
			p + 2,
			error.e_data->length - 2);
	return 0;
    }
}

krb5_error_code
krb5_change_password (krb5_context	context,
		      krb5_creds	*creds,
		      char		*newpw,
		      int		*result_code,
		      krb5_data		*result_code_string,
		      krb5_data		*result_string)
{
    krb5_error_code ret;
    krb5_auth_context auth_context = NULL;
    int sock;
    int i;
    struct addrinfo *ai, *a;
    int done = 0;

    ret = krb5_auth_con_init (context, &auth_context);
    if (ret)
	return ret;

    ret = get_kdc_address (context, creds->client->realm, &ai);
    if (ret)
	goto out;

    for (a = ai; !done && a != NULL; a = a->ai_next) {
	int replied = 0;

	sock = socket (a->ai_family, a->ai_socktype, a->ai_protocol);
	if (sock < 0)
	    continue;

	for (i = 0; !done && i < 5; ++i) {
	    fd_set fdset;
	    struct timeval tv;

	    if (!replied) {
		replied = 0;
		ret = send_request (context,
				    &auth_context,
				    creds,
				    sock,
				    a->ai_addr,
				    a->ai_addrlen,
				    newpw);
		if (ret) {
		    close(sock);
		    goto out;
		}
	    }
	    
	    if (sock >= FD_SETSIZE) {
		ret = ERANGE;
		close (sock);
		goto out;
	    }

	    FD_ZERO(&fdset);
	    FD_SET(sock, &fdset);
	    tv.tv_usec = 0;
	    tv.tv_sec  = 1 + (1 << i);

	    ret = select (sock + 1, &fdset, NULL, NULL, &tv);
	    if (ret < 0 && errno != EINTR) {
		close(sock);
		goto out;
	    }
	    if (ret == 1) {
		ret = process_reply (context,
				     auth_context,
				     sock,
				     result_code,
				     result_code_string,
				     result_string);
		if (ret == 0)
		    done = 1;
		else if (i > 0 && ret == KRB5KRB_AP_ERR_MUT_FAIL)
		    replied = 1;
	    } else {
		ret = KRB5_KDC_UNREACH;
	    }
	}
	close (sock);
    }
    freeaddrinfo (ai);

out:
    krb5_auth_con_free (context, auth_context);
    if (done)
	return 0;
    else
	return ret;
}
