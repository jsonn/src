/* -*- C -*- */

/*
 * Copyright (c) 1995 - 1999 Kungliga Tekniska H�gskolan
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

/* @(#)$Id: rxkad.h,v 1.1.1.1.4.2 2000/06/16 18:45:51 thorpej Exp $ */

#ifndef __RXKAD_H
#define __RXKAD_H

#ifdef __STDC__
#ifndef __P
#define __P(x) x
#endif
#else
#ifndef __P
#define __P(x) ()
#endif
#endif

/* Krb4 tickets can't have a key version number of 512. This is used
 * as a magic kvno to indicate that this is really a krb5 ticket. The
 * real kvno can be retrieved from the cleartext portion of the
 * ticket. For more info see the Transarc header file rxkad.h.
 */
#define RXKAD_TKT_TYPE_KERBEROS_V5 512

/* Is this really large enough for a krb5 ticket? */
#define MAXKRB5TICKETLEN	1024

typedef char rxkad_level;
#define rxkad_clear 0		/* checksum some selected header fields */
#define rxkad_auth 1		/* rxkad_clear + protected packet length */
#define rxkad_crypt 2		/* rxkad_crypt + encrypt packet payload */
extern int rxkad_min_level;	/* enforce min level at client end */

extern int rxkad_EpochWasSet;

int32 rxkad_GetServerInfo __P((struct rx_connection *con,
			       rxkad_level *level,
			       u_int32 *expiration,
			       char *name,
			       char *instance,
			       char *cell,
			       int32 *kvno));

struct rx_securityClass *
rxkad_NewServerSecurityObject __P((/*rxkad_level*/ int min_level,
				   void *appl_data,
				   int (*get_key)(void *appl_data,
						  int kvno,
						  des_cblock *key),
				   int (*user_ok)(char *name,
						  char *inst,
						  char *realm,
						  int kvno)));

struct rx_securityClass *
rxkad_NewClientSecurityObject __P((/*rxkad_level*/ int level,
				   void *sessionkey,
				   int32 kvno,
				   int ticketLen,
				   char *ticket));

#define RXKADINCONSISTENCY	(19270400L)
#define RXKADPACKETSHORT	(19270401L)
#define RXKADLEVELFAIL		(19270402L)
#define RXKADTICKETLEN		(19270403L)
#define RXKADOUTOFSEQUENCE	(19270404L)
#define RXKADNOAUTH		(19270405L)
#define RXKADBADKEY		(19270406L)
#define RXKADBADTICKET		(19270407L)
#define RXKADUNKNOWNKEY		(19270408L)
#define RXKADEXPIRED		(19270409L)
#define RXKADSEALEDINCON	(19270410L)
#define RXKADDATALEN		(19270411L)
#define RXKADILLEGALLEVEL	(19270412L)

/* The rest is backwards compatibility stuff that we don't use! */
#define MAXKTCTICKETLIFETIME (30*24*60*60)
#define MINKTCTICKETLEN (32)
#define MAXKTCTICKETLEN (344)
#define MAXKTCNAMELEN (64)
#define MAXKTCREALMLEN (64)

#define KTC_TIME_UNCERTAINTY (CLOCK_SKEW)

struct ktc_encryptionKey {
  char data[8];
};

struct ktc_principal {
  char name[MAXKTCNAMELEN];
  char instance[MAXKTCNAMELEN];
  char cell[MAXKTCREALMLEN];
};

u_int32 life_to_time __P((u_int32 start, int life_));

int time_to_life __P((u_int32 start, u_int32 end));

int tkt_CheckTimes __P((int32 begin, int32 end, int32 now));

int
tkt_MakeTicket __P((char *ticket,
		    int *ticketLen,
		    struct ktc_encryptionKey *key,
		    char *name, char *inst, char *cell,
		    u_int32 start, u_int32 end,
		    struct ktc_encryptionKey *sessionKey,
		    u_int32 host,
		    char *sname, char *sinst));

int
tkt_DecodeTicket __P((char *asecret,
		      int32 ticketLen,
		      struct ktc_encryptionKey *key,
		      char *name,
		      char *inst,
		      char *cell,
		      char *sessionKey,
		      int32 *host,
		      int32 *start,
		      int32 *end));

#endif /* __RXKAD_H */
