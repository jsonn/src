/*	$NetBSD: keyv2.h,v 1.2.2.3 1999/08/02 22:37:57 thorpej Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* KAME Id: keyv2.h,v 1.1.6.1.6.4 1999/06/08 05:33:39 itojun Exp */

/*
 * This file has been derived rfc 2367,
 * And added some flags of SADB_KEY_FLAGS_ as SADB_X_EXT_.
 *	sakane@ydc.co.jp
 */

#ifndef _NETKEY_KEYV2_H_
#define _NETKEY_KEYV2_H_

#ifdef __NetBSD__
# ifdef _KERNEL
#  define KERNEL
# endif
#endif

/*
This file defines structures and symbols for the PF_KEY Version 2
key management interface. It was written at the U.S. Naval Research
Laboratory. This file is in the public domain. The authors ask that
you leave this credit intact on any copies of this file.
*/
#ifndef __PFKEY_V2_H
#define __PFKEY_V2_H 1

#define PF_KEY_V2 2
#define PFKEYV2_REVISION        199806L

#define SADB_RESERVED    0
#define SADB_GETSPI      1
#define SADB_UPDATE      2
#define SADB_ADD         3
#define SADB_DELETE      4
#define SADB_GET         5
#define SADB_ACQUIRE     6
#define SADB_REGISTER    7
#define SADB_EXPIRE      8
#define SADB_FLUSH       9
#define SADB_DUMP        10
#define SADB_X_PROMISC   11
#define SADB_X_PCHANGE   12
#define SADB_X_SPDADD    13
#define SADB_X_SPDDELETE 14
#define SADB_X_SPDDUMP   15
#define SADB_X_SPDFLUSH  16
#define SADB_MAX         16

struct sadb_msg {
  u_int8_t sadb_msg_version;
  u_int8_t sadb_msg_type;
  u_int8_t sadb_msg_errno;
  u_int8_t sadb_msg_satype;
  u_int16_t sadb_msg_len;
  u_int16_t sadb_msg_reserved;
  u_int32_t sadb_msg_seq;
  u_int32_t sadb_msg_pid;
};

struct sadb_ext {
  u_int16_t sadb_ext_len;
  u_int16_t sadb_ext_type;
};

struct sadb_sa {
  u_int16_t sadb_sa_len;
  u_int16_t sadb_sa_exttype;
  u_int32_t sadb_sa_spi;
  u_int8_t sadb_sa_replay;
  u_int8_t sadb_sa_state;
  u_int8_t sadb_sa_auth;
  u_int8_t sadb_sa_encrypt;
  u_int32_t sadb_sa_flags;
};

struct sadb_lifetime {
  u_int16_t sadb_lifetime_len;
  u_int16_t sadb_lifetime_exttype;
  u_int32_t sadb_lifetime_allocations;
  u_int64_t sadb_lifetime_bytes;
  u_int64_t sadb_lifetime_addtime;
  u_int64_t sadb_lifetime_usetime;
};

struct sadb_address {
  u_int16_t sadb_address_len;
  u_int16_t sadb_address_exttype;
  u_int8_t sadb_address_proto;
  u_int8_t sadb_address_prefixlen;
  u_int16_t sadb_address_reserved;
};

struct sadb_key {
  u_int16_t sadb_key_len;
  u_int16_t sadb_key_exttype;
  u_int16_t sadb_key_bits;
  u_int16_t sadb_key_reserved;
};

struct sadb_ident {
  u_int16_t sadb_ident_len;
  u_int16_t sadb_ident_exttype;
  u_int16_t sadb_ident_type;
  u_int16_t sadb_ident_reserved;
  u_int64_t sadb_ident_id;
};

struct sadb_sens {
  u_int16_t sadb_sens_len;
  u_int16_t sadb_sens_exttype;
  u_int32_t sadb_sens_dpd;
  u_int8_t sadb_sens_sens_level;
  u_int8_t sadb_sens_sens_len;
  u_int8_t sadb_sens_integ_level;
  u_int8_t sadb_sens_integ_len;
  u_int32_t sadb_sens_reserved;
};

struct sadb_prop {
  u_int16_t sadb_prop_len;
  u_int16_t sadb_prop_exttype;
  u_int8_t sadb_prop_replay;
  u_int8_t sadb_prop_reserved[3];
};

struct sadb_comb {
  u_int8_t sadb_comb_auth;
  u_int8_t sadb_comb_encrypt;
  u_int16_t sadb_comb_flags;
  u_int16_t sadb_comb_auth_minbits;
  u_int16_t sadb_comb_auth_maxbits;
  u_int16_t sadb_comb_encrypt_minbits;
  u_int16_t sadb_comb_encrypt_maxbits;
  u_int32_t sadb_comb_reserved;
  u_int32_t sadb_comb_soft_allocations;
  u_int32_t sadb_comb_hard_allocations;
  u_int64_t sadb_comb_soft_bytes;
  u_int64_t sadb_comb_hard_bytes;
  u_int64_t sadb_comb_soft_addtime;
  u_int64_t sadb_comb_hard_addtime;
  u_int64_t sadb_comb_soft_usetime;
  u_int64_t sadb_comb_hard_usetime;
};

struct sadb_supported {
  u_int16_t sadb_supported_len;
  u_int16_t sadb_supported_exttype;
  u_int32_t sadb_supported_reserved;
};

struct sadb_alg {
  u_int8_t sadb_alg_id;
  u_int8_t sadb_alg_ivlen;
  u_int16_t sadb_alg_minbits;
  u_int16_t sadb_alg_maxbits;
  u_int16_t sadb_alg_reserved;
};

struct sadb_spirange {
  u_int16_t sadb_spirange_len;
  u_int16_t sadb_spirange_exttype;
  u_int32_t sadb_spirange_min;
  u_int32_t sadb_spirange_max;
  u_int32_t sadb_spirange_reserved;
};

struct sadb_x_kmprivate {
  u_int16_t sadb_x_kmprivate_len;
  u_int16_t sadb_x_kmprivate_exttype;
  u_int32_t sadb_x_kmprivate_reserved;
};

/* XXX Policy Extension */
/* sizeof(struct sadb_x_policy) == 8 */
struct sadb_x_policy {
  u_int16_t sadb_x_policy_len;
  u_int16_t sadb_x_policy_exttype;
  u_int16_t sadb_x_policy_type;		/* See ipsec.h */
  u_int16_t sadb_x_policy_reserved;
};
/*
 * followed by some of the ipsec policy request, if policy_type == IPSEC.
 * [total length of ipsec policy requests]
 *	= (sadb_x_policy_len * sizeof(uint64_t) - sizeof(struct sadb_x_policy))
 */

/* XXX IPsec Policy Request Extension */
/*
 * This structure is aligned 8 bytes. Also it's aligned with proxy address
 * if present.
 */
struct sadb_x_ipsecrequest {
  u_int16_t sadb_x_ipsecrequest_len;	/* structure length aligned 8 bytes.
					 * This value is true length of bytes.
					 * Not in units of 64 bits. */
  u_int16_t sadb_x_ipsecrequest_proto;	/* See ipsec.h */
  u_int16_t sadb_x_ipsecrequest_mode;	/* See ipsec.h */
  u_int16_t sadb_x_ipsecrequest_level;	/* See ipsec.h */
  /* If mode != 0, the proxy address encoded struct sockaddr is following. */
};

#define SADB_EXT_RESERVED             0
#define SADB_EXT_SA                   1
#define SADB_EXT_LIFETIME_CURRENT     2
#define SADB_EXT_LIFETIME_HARD        3
#define SADB_EXT_LIFETIME_SOFT        4
#define SADB_EXT_ADDRESS_SRC          5
#define SADB_EXT_ADDRESS_DST          6
#define SADB_EXT_ADDRESS_PROXY        7
#define SADB_EXT_KEY_AUTH             8
#define SADB_EXT_KEY_ENCRYPT          9
#define SADB_EXT_IDENTITY_SRC         10
#define SADB_EXT_IDENTITY_DST         11
#define SADB_EXT_SENSITIVITY          12
#define SADB_EXT_PROPOSAL             13
#define SADB_EXT_SUPPORTED_AUTH       14
#define SADB_EXT_SUPPORTED_ENCRYPT    15
#define SADB_EXT_SPIRANGE             16
#define SADB_X_EXT_KMPRIVATE          17
#define SADB_X_EXT_POLICY             18
#define SADB_EXT_MAX                  18

#define SADB_SATYPE_UNSPEC	0
#define SADB_SATYPE_AH		2
#define SADB_SATYPE_ESP		3
#define SADB_SATYPE_RSVP	5
#define SADB_SATYPE_OSPFV2	6
#define SADB_SATYPE_RIPV2	7
#define SADB_SATYPE_MIP		8
#define SADB_X_SATYPE_IPCOMP	9
#define SADB_SATYPE_MAX		9

#define SADB_SASTATE_LARVAL   0
#define SADB_SASTATE_MATURE   1
#define SADB_SASTATE_DYING    2
#define SADB_SASTATE_DEAD     3
#define SADB_SASTATE_MAX      3

#define SADB_SAFLAGS_PFS      1

#define SADB_AALG_NONE          0
#define SADB_AALG_MD5HMAC       1	/* 2 */
#define SADB_AALG_SHA1HMAC      2	/* 3 */
#define SADB_AALG_MD5           3       /* Keyed MD5 */
#define SADB_AALG_SHA           4       /* Keyed SHA */
#define SADB_AALG_NULL          5       /* null authentication */
#define SADB_AALG_MAX           6

#define SADB_EALG_NONE          0
#define SADB_EALG_DESCBC        1	/* 2 */
#define SADB_EALG_3DESCBC       2	/* 3 */
#define SADB_EALG_NULL          3	/* 11 */
#define SADB_EALG_BLOWFISHCBC   4
#define SADB_EALG_CAST128CBC    5
#if 0
#define SADB_EALG_RC5CBC        6
#define SADB_EALG_MAX           7
#else
#define SADB_EALG_MAX           6
#endif

#if 1	/*nonstandard */
#define SADB_X_CALG_NONE	0
#define SADB_X_CALG_OUI		1
#define SADB_X_CALG_DEFLATE	2
#define SADB_X_CALG_LZS		3
#endif

#define SADB_IDENTTYPE_RESERVED   0
#define SADB_IDENTTYPE_PREFIX     1
#define SADB_IDENTTYPE_FQDN       2
#define SADB_IDENTTYPE_USERFQDN   3
#define SADB_IDENTTYPE_MAX        3

/* `flags' in SA structure holds followings */
#define SADB_X_EXT_NONE		0x0000	/* i.e. new format. */
#define SADB_X_EXT_OLD		0x0001	/* old format. */
#define SADB_X_EXT_IV4B		0x0010	/* IV length of 4 bytes in use */
#define SADB_X_EXT_DERIV	0x0020	/* DES derived */
#define SADB_X_EXT_CYCSEQ	0x0040	/* allowing to cyclic sequence. */
	/* the followings are exclusive flags */
#define SADB_X_EXT_PSEQ		0x0000	/* sequencial padding for ESP */
#define SADB_X_EXT_PRAND	0x0100	/* random padding for ESP */
#define SADB_X_EXT_PZERO	0x0300	/* zero padding for ESP */
#define SADB_X_EXT_PMASK	0x0300	/* mask for padding flag */
#if 1
#define SADB_X_EXT_RAWCPI	0x0080	/* use well known CPI (IPComp) */
#endif
#define SADB_KEY_FLAGS_MAX	0x0fff

/* SPI size for PF_KEYv2 */
#define PFKEY_SPI_SIZE	sizeof(u_int32_t)

/* Identifier for menber of lifetime structure */
#define SADB_X_LIFETIME_ALLOCATIONS	0
#define SADB_X_LIFETIME_BYTES		1
#define SADB_X_LIFETIME_ADDTIME		2
#define SADB_X_LIFETIME_USETIME		3

/* The rate for SOFT lifetime against HARD one. */
#define PFKEY_SOFT_LIFETIME_RATE	80

/* Utilities */
#define PFKEY_ALIGN8(a) (1 + (((a) - 1) | (8 - 1)))
#define	PFKEY_EXTLEN(msg) \
	PFKEY_UNUNIT64(((struct sadb_ext *)(msg))->sadb_ext_len)
#define PFKEY_ADDR_PREFIX(ext) \
	(((struct sadb_address *)(ext))->sadb_address_prefixlen)
#define PFKEY_ADDR_PROTO(ext) \
	(((struct sadb_address *)(ext))->sadb_address_proto)
#define PFKEY_ADDR_SADDR(ext) \
	((struct sockaddr *)((caddr_t)(ext) + sizeof(struct sadb_address)))

#if 1
/* in 64bits */
#define	PFKEY_UNUNIT64(a)	((a) << 3)
#define	PFKEY_UNIT64(a)		((a) >> 3)
#else
#define	PFKEY_UNUNIT64(a)	(a)
#define	PFKEY_UNIT64(a)		(a)
#endif

#ifndef KERNEL
extern void pfkey_sadump(struct sadb_msg *m);
extern void pfkey_spdump(struct sadb_msg *m);

struct sockaddr;
extern int ipsec_check_keylen(u_int supported, u_int alg_id, u_int keylen);
extern int pfkey_check(struct sadb_msg *msg, caddr_t *mhp);
extern u_int pfkey_set_softrate(u_int type, u_int rate);
extern u_int pfkey_get_softrate(u_int type);
extern int pfkey_send_getspi(int so, u_int satype,
	struct sockaddr *src, u_int prefs,
	struct sockaddr *dst, u_int prefd, u_int proto,
	u_int32_t min, u_int32_t max, u_int32_t seq);
extern int pfkey_send_update( int so, u_int satype,
	struct sockaddr *src, u_int prefs,
	struct sockaddr *dst, u_int prefd, u_int proto,
	struct sockaddr *proxy,
	u_int32_t spi, caddr_t keymat,
	u_int e_type, u_int e_keylen, u_int a_type, u_int a_keylen,
	u_int flags,
	u_int32_t l_alloc, u_int32_t l_bytes,
	u_int32_t l_addtime, u_int32_t l_usetime, u_int32_t seq);
extern int pfkey_send_add( int so, u_int satype,
	struct sockaddr *src, u_int prefs,
	struct sockaddr *dst, u_int prefd, u_int proto,
	struct sockaddr *proxy,
	u_int32_t spi, caddr_t keymat,
	u_int e_type, u_int e_keylen, u_int a_type, u_int a_keylen,
	u_int flags,
	u_int32_t l_alloc, u_int32_t l_bytes,
	u_int32_t l_addtime, u_int32_t l_usetime, u_int32_t seq);
extern int pfkey_send_delete( int so, u_int satype,
	struct sockaddr *src, u_int prefs,
	struct sockaddr *dst, u_int prefd, u_int proto,
	u_int32_t spi);
extern int pfkey_send_get( int so, u_int satype,
	struct sockaddr *src, u_int prefs,
	struct sockaddr *dst, u_int prefd, u_int proto,
	u_int32_t spi);
extern int pfkey_send_register(int so, u_int satype);
extern int pfkey_recv_register(int so);
extern int pfkey_send_flush(int so, u_int satype);
extern int pfkey_send_dump(int so, u_int satype);
extern int pfkey_send_promisc_toggle(int so, int flag);

extern int pfkey_open(void);
extern void pfkey_close(int so);
extern struct sadb_msg *pfkey_recv(int so);
extern int pfkey_send(int so, struct sadb_msg *msg, int len);

#endif /*!KERNEL*/

#endif /* __PFKEY_V2_H */

#endif /* _NETKEY_KEYV2_H_ */
