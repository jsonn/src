/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska H�gskolan
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

#include "krb_locl.h"

RCSID("$Id: create_ciph.c,v 1.1.1.1.4.2 2000/06/16 18:45:53 thorpej Exp $");

/*
 * This routine is used by the authentication server to create
 * a packet for its client, containing a ticket for the requested
 * service (given in "tkt"), and some information about the ticket,
 *
 * Returns KSUCCESS no matter what.
 *
 * The length of the cipher is stored in c->length; the format of
 * c->dat is as follows:
 *
 * 			variable
 * type			or constant	   data
 * ----			-----------	   ----
 * 
 * 
 * 8 bytes		session		session key for client, service
 * 
 * string		service		service name
 * 
 * string		instance	service instance
 * 
 * string		realm		KDC realm
 * 
 * unsigned char	life		ticket lifetime
 * 
 * unsigned char	kvno		service key version number
 * 
 * unsigned char	tkt->length	length of following ticket
 * 
 * data			tkt->dat	ticket for service
 * 
 * 4 bytes		kdc_time	KDC's timestamp
 *
 * <=7 bytes		null		   null pad to 8 byte multiple
 *
 */

int
create_ciph(KTEXT c,		/* Text block to hold ciphertext */
	    unsigned char *session, /* Session key to send to user */
	    char *service,	/* Service name on ticket */
	    char *instance,	/* Instance name on ticket */
	    char *realm,	/* Realm of this KDC */
	    u_int32_t life,	/* Lifetime of the ticket */
	    int kvno,		/* Key version number for service */
	    KTEXT tkt,		/* The ticket for the service */
	    u_int32_t kdc_time,	/* KDC time */
	    des_cblock *key)	/* Key to encrypt ciphertext with */

{
    unsigned char *p = c->dat;
    size_t rem = sizeof(c->dat);
    int tmp;

    memset(c, 0, sizeof(KTEXT_ST));

    if (rem < 8)
	return KFAILURE;
    memcpy(p, session, 8);
    p += 8;
    rem -= 8;
    
    tmp = krb_put_nir(service, instance, realm, p, rem);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    
    tmp = krb_put_int(life, p, rem, 1);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_int(kvno, p, rem, 1);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_int(tkt->length, p, rem, 1);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    if (rem < tkt->length)
	return KFAILURE;
    memcpy(p, tkt->dat, tkt->length);
    p += tkt->length;
    rem -= tkt->length;

    tmp = krb_put_int(kdc_time, p, rem, 4);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    /* multiple of eight bytes */
    c->length = (p - c->dat + 7) & ~7;

    encrypt_ktext(c, key, DES_ENCRYPT);
    return KSUCCESS;
}
